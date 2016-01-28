/**
 * @file
 *
 * @author Pierre Jacob <jacob@ceremade.dauphine.fr>
 * @author Lawrence Murray <lawrence.murray@csiro.au>
 * $Rev$
 * $Date$
 */
#ifndef BI_SAMPLER_MARGINALSIR_HPP
#define BI_SAMPLER_MARGINALSIR_HPP

#include "../state/Schedule.hpp"
#include "../misc/exception.hpp"
#include "../primitive/vector_primitive.hpp"

namespace bi {
/**
 * Marginal sequential importance resampling.
 *
 * @ingroup method_sampler
 *
 * @tparam B Model type
 * @tparam F Filter type.
 * @tparam A Adapter type.
 * @tparam R Resampler type.
 *
 * Implements sequential importance resampling over parameters, which, when
 * combined with a particle filter, gives the SMC^2 method described in
 * @ref Chopin2013 "Chopin, Jacob \& Papaspiliopoulos (2013)".
 *
 * @todo Add support for adapter classes.
 * @todo Add support for stopper classes for theta particles.
 */
template<class B, class F, class A, class R>
class MarginalSIR {
public:
  /**
   * Constructor.
   *
   * @param m Model.
   * @param filter Filter.
   * @param adapter Adapter.
   * @param resam Resampler for theta-particles.
   * @param nmoves Number of steps per \f$\theta\f$-particle.
   * @param adapter Proposal adaptation strategy.
   * @param adapterScale Scaling factor for local proposals.
   * @param out Output.
   */
  MarginalSIR(B& m, F& filter, A& adapter, R& resam, const int nmoves = 1);

  /**
   * @name High-level interface
   */
  //@{
  /**
   * @copydoc MarginalMH::sample()
   */
  template<class S1, class IO1, class IO2>
  void sample(Random& rng, const ScheduleIterator first,
      const ScheduleIterator last, S1& s, const int C, IO1& out, IO2& inInit);
  //@}

  /**
   * @name Low-level interface
   */
  //@{
  /**
   * Initialise.
   *
   * @tparam S1 State type.
   * @tparam IO1 Output type.
   * @tparam IO2 Input type.
   *
   * @param[in,out] rng Random number generator.
   * @param first Start of time schedule.
   * @param s State.
   * @param out Output buffer.
   * @param inInit Init buffer.
   */
  template<class S1, class IO1, class IO2>
  void init(Random& rng, const ScheduleIterator first, S1& s, IO1& out,
      IO2& inInit);

  /**
   * Step \f$x\f$-particles forward.
   *
   * @tparam S1 State type.
   * @tparam IO1 Output type.
   *
   * @param[in,out] rng Random number generator.
   * @param first Start of time schedule.
   * @param[in,out] iter Current position in time schedule. Advanced on
   * return.
   * @param last End of time schedule.
   * @param[out] s State.
   * @param out Output buffer.
   */
  template<class S1, class IO1>
  void step(Random& rng, const ScheduleIterator first, ScheduleIterator& iter,
      const ScheduleIterator last, S1& s, IO1& out);

  /**
   * Adapt proposal.
   *
   * @tparam S1 State type.
   *
   * @param[in,out] s State.
   */
  template<class S1>
  void adapt(const S1& s);

  /**
   * Resample \f$\theta\f$-particles.
   *
   * @tparam S1 State type.
   *
   * @param[in,out] rng Random number generator.
   * @param now Current step in time schedule.
   * @param[in,out] s State.
   */
  template<class S1>
  void resample(Random& rng, const ScheduleElement now, S1& s);

  /**
   * Rejuvenate \f$\theta\f$-particles.
   *
   * @tparam S1 State type.
   *
   * @param[in,out] rng Random number generator.
   * @param first Start of time schedule.
   * @param now Current position in time schedule.
   * @param[in,out] s State.
   */
  template<class S1>
  void rejuvenate(Random& rng, const ScheduleIterator first,
      const ScheduleIterator now, S1& s);

  /**
   * @copydoc Simulator::outputT()
   */
  template<class S1, class IO1>
  void outputT(const S1& s, IO1& out);

  /**
   * Report progress on stderr.
   *
   * @param now Current step in time schedule.
   * @param s State.
   */
  template<class S1>
  void report(const ScheduleElement now, S1& s);

  /**
   * Report last step on stderr.
   *
   * @param now Current step in time schedule.
   */
  void reportT(const ScheduleElement now);

  /**
   * Finalise.
   */
  template<class S1>
  void term(Random& rng, S1& s);
  //@}

private:
  /**
   * Model.
   */
  B& m;

  /**
   * Filter.
   */
  F& filter;

  /**
   * Adapter.
   */
  A& adapter;

  /**
   * Resampler for the theta-particles
   */
  R& resam;

  /**
   * Number of PMMH steps when rejuvenating.
   */
  int nmoves;

  /**
   * Was a resample performed on the last step?
   */
  bool lastResample;

  /**
   * Last acceptance rate when rejuvenating.
   */
  double lastAcceptRate;
};
}

#include <sstream>

template<class B, class F, class A, class R>
bi::MarginalSIR<B,F,A,R>::MarginalSIR(B& m, F& filter, A& adapter, R& resam,
    const int nmoves) :
    m(m), filter(filter), adapter(adapter), resam(resam), nmoves(nmoves), lastResample(
        false), lastAcceptRate(0.0) {
  //
}

template<class B, class F, class A, class R>
template<class S1, class IO1, class IO2>
void bi::MarginalSIR<B,F,A,R>::sample(Random& rng,
    const ScheduleIterator first, const ScheduleIterator last, S1& s,
    const int C, IO1& out, IO2& inInit) {
  // should look very similar to Filter::filter()
  ScheduleIterator iter = first;
  init(rng, iter, s, out, inInit);
#if ENABLE_DIAGNOSTICS == 3
  std::stringstream buf;
  buf << "sir" << iter->indexOutput() << ".nc";
  SMCBuffer<SMCCache<ON_HOST,SMCNetCDFBuffer> > outtmp(m, s.size(), last->indexOutput(), buf.str(), REPLACE);
  outtmp.write(s);
  outtmp.flush();
#endif
  while (iter + 1 != last) {
    step(rng, first, iter, last, s, out);
#if ENABLE_DIAGNOSTICS == 3
    std::stringstream buf;
    buf << "sir" << iter->indexOutput() << ".nc";
    SMCBuffer<SMCCache<ON_HOST,SMCNetCDFBuffer> > outtmp(m, s.size(), last->indexOutput(), buf.str(), REPLACE);
    outtmp.write(s);
    outtmp.flush();
#endif
  }
  term(rng, s);
  reportT(*iter);
  outputT(s, out);
}

template<class B, class F, class A, class R>
template<class S1, class IO1, class IO2>
void bi::MarginalSIR<B,F,A,R>::init(Random& rng, const ScheduleIterator first,
    S1& s, IO1& out, IO2& inInit) {
  for (int p = 0; p < s.size(); ++p) {
    BOOST_AUTO(&s1, *s.s1s[p]);
    BOOST_AUTO(&out1, *s.out1s[p]);

    filter.init(rng, *first, s1, out1, inInit);
    filter.output0(s1, out1);
    filter.correct(rng, *first, s1);
    filter.output(*first, s1, out1);

    s.logWeights()(p) = s1.logLikelihood;
    s.ancestors()(p) = p;
  }
  out.clear();

  lastResample = false;
  lastAcceptRate = 0.0;
}

template<class B, class F, class A, class R>
template<class S1, class IO1>
void bi::MarginalSIR<B,F,A,R>::step(Random& rng, const ScheduleIterator first,
    ScheduleIterator& iter, const ScheduleIterator last, S1& s, IO1& out) {
  ScheduleIterator iter1;
  do {
    adapt(s);
    resample(rng, *iter, s);
    rejuvenate(rng, first, iter + 1, s);
    report(*iter, s);

    for (int p = 0; p < s.size(); ++p) {
      BOOST_AUTO(&s1, *s.s1s[p]);
      BOOST_AUTO(&out1, *s.out1s[p]);

      iter1 = iter;
      filter.step(rng, iter1, last, s1, out1);
#if ENABLE_DIAGNOSTICS == 3
      filter.samplePath(rng, s1, out1);
#endif
      s.logWeights()(p) += s1.logIncrements(iter1->indexObs());
    }
    iter = iter1;
  } while (iter + 1 != last && !iter->isObserved());

  double lW;
  s.ess = ess_reduce(s.logWeights(), &lW);
  s.logIncrements(iter->indexObs()) = lW - s.logLikelihood;
  s.logLikelihood = lW;
}

template<class B, class F, class A, class R>
template<class S1>
void bi::MarginalSIR<B,F,A,R>::adapt(const S1& s) {
  adapter.clear();
  adapter.add(s);
  if (adapter.ready()) {
    adapter.adapt();
  }
}

template<class B, class F, class A, class R>
template<class S1>
void bi::MarginalSIR<B,F,A,R>::resample(Random& rng,
    const ScheduleElement now, S1& s) {
  lastResample = resam.resample(rng, now, s);
}

template<class B, class F, class A, class R>
template<class S1>
void bi::MarginalSIR<B,F,A,R>::rejuvenate(Random& rng,
    const ScheduleIterator first, const ScheduleIterator last, S1& s) {
  if (lastResample) {
    int naccept = 0;
    bool accept = false;
    bool ready = adapter.ready();

    for (int p = 0; p < s.size(); ++p) {
      BOOST_AUTO(&s1, *s.s1s[p]);
      BOOST_AUTO(&out1, *s.out1s[p]);
      BOOST_AUTO(&s2, s.s2);
      BOOST_AUTO(&out2, s.out2);

      for (int move = 0; move < nmoves; ++move) {
        /* propose replacement */
        try {
          if (ready) {
            filter.propose(rng, *first, s1, s2, out2, adapter);
          } else {
            filter.propose(rng, *first, s1, s2, out2);
          }
          if (bi::is_finite(s2.logPrior)) {
            filter.filter(rng, first, last, s2, out2);
          }
        } catch (CholeskyException e) {
          s2.logLikelihood = -BI_INF;
        } catch (ParticleFilterDegeneratedException e) {
          s2.logLikelihood = -BI_INF;
        }

        /* accept or reject */
        if (!bi::is_finite(s2.logLikelihood)) {
          accept = false;
        } else if (!bi::is_finite(s1.logLikelihood)) {
          accept = true;
        } else {
          double loglr = s2.logLikelihood - s1.logLikelihood;
          double logpr = s2.logPrior - s1.logPrior;
          double logqr = s1.logProposal - s2.logProposal;

          if (!bi::is_finite(s1.logProposal)
              && !bi::is_finite(s2.logProposal)) {
            logqr = 0.0;
          }
          double logratio = loglr + logpr + logqr;
          double u = rng.uniform<double>();

          accept = bi::log(u) < logratio;
        }

        if (accept) {
#if ENABLE_DIAGNOSTICS == 3
          filter.samplePath(rng, s2, out2);
#endif
          s1.swap(s2);
          out1.swap(out2);
          ++naccept;
        }
      }
    }

    int ntotal = nmoves * s.size();
#ifdef ENABLE_MPI
    boost::mpi::communicator world;
    const int rank = world.rank();
    boost::mpi::all_reduce(world, &ntotal, 1, &ntotal, std::plus<int>());
    boost::mpi::all_reduce(world, &naccept, 1, &naccept, std::plus<int>());
#endif
    lastAcceptRate = double(naccept) / ntotal;
  }
}

template<class B, class F, class A, class R>
template<class S1, class IO1>
void bi::MarginalSIR<B,F,A,R>::outputT(const S1& s, IO1& out) {
  out.write(s);
}

template<class B, class F, class A, class R>
template<class S1>
void bi::MarginalSIR<B,F,A,R>::report(const ScheduleElement now, S1& s) {
#ifdef ENABLE_MPI
  boost::mpi::communicator world;
  const int rank = world.rank();
#else
  const int rank = 0;
#endif

  if (rank == 0) {
    std::cerr << now.indexOutput() << ":\ttime " << now.getTime() << "\tESS "
        << s.ess;
    if (lastResample) {
      std::cerr << "\tresample-move with acceptance rate " << lastAcceptRate;
    }
    std::cerr << std::endl;
  }
}

template<class B, class F, class A, class R>
void bi::MarginalSIR<B,F,A,R>::reportT(const ScheduleElement now) {
#ifdef ENABLE_MPI
  boost::mpi::communicator world;
  const int rank = world.rank();
#else
  const int rank = 0;
#endif

  if (rank == 0) {
    std::cerr << now.indexOutput() << ":\ttime " << now.getTime()
        << "\t...finished." << std::endl;
  }
}

template<class B, class F, class A, class R>
template<class S1>
void bi::MarginalSIR<B,F,A,R>::term(Random& rng, S1& s) {
  s.logLikelihood += logsumexp_reduce(s.logWeights())
      - bi::log(double(s.size()));
  for (int p = 0; p < s.size(); ++p) {
    BOOST_AUTO(&s1, *s.s1s[p]);
    BOOST_AUTO(&out1, *s.out1s[p]);
    filter.samplePath(rng, s1, out1);
  }
}

#endif