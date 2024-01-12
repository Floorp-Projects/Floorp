/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_GraphRunner_h
#define mozilla_GraphRunner_h

#include "GraphDriver.h"
#include "MediaSegment.h"
#include "mozilla/Monitor.h"

#include <thread>

struct PRThread;

namespace mozilla {

class AudioMixer;
class MediaTrackGraphImpl;

class GraphRunner final : public Runnable {
  using IterationResult = GraphInterface::IterationResult;

 public:
  static already_AddRefed<GraphRunner> Create(MediaTrackGraphImpl* aGraph);

  /**
   * Marks us as shut down and signals mThread, so that it runs until the end.
   */
  MOZ_CAN_RUN_SCRIPT void Shutdown();

  /**
   * Signals one iteration of mGraph. Hands state over to mThread and runs
   * the iteration there.
   */
  IterationResult OneIteration(GraphTime aStateTime, GraphTime aIterationEnd,
                               MixerCallbackReceiver* aMixerReceiver);

  /**
   * Runs mGraph until it shuts down.
   */
  NS_IMETHOD Run() override;

  /**
   * Returns true if called on mThread.
   */
  bool OnThread() const;

#ifdef DEBUG
  /**
   * Returns true if called on mThread, and aDriver was the driver that called
   * OneIteration() last.
   */
  bool InDriverIteration(const GraphDriver* aDriver) const;
#endif

 private:
  explicit GraphRunner(MediaTrackGraphImpl* aGraph,
                       already_AddRefed<nsIThread> aThread);
  ~GraphRunner();

  class IterationState {
    GraphTime mStateTime;
    GraphTime mIterationEnd;
    MixerCallbackReceiver* MOZ_NON_OWNING_REF mMixerReceiver;

   public:
    IterationState(GraphTime aStateTime, GraphTime aIterationEnd,
                   MixerCallbackReceiver* aMixerReceiver)
        : mStateTime(aStateTime),
          mIterationEnd(aIterationEnd),
          mMixerReceiver(aMixerReceiver) {}
    IterationState& operator=(const IterationState& aOther) = default;
    GraphTime StateTime() const { return mStateTime; }
    GraphTime IterationEnd() const { return mIterationEnd; }
    MixerCallbackReceiver* MixerReceiver() const { return mMixerReceiver; }
  };

  // Monitor used for yielding mThread through Wait(), and scheduling mThread
  // through Signal() from a GraphDriver.
  Monitor mMonitor;
  // The MediaTrackGraph we're running. Weakptr beecause this graph owns us and
  // guarantees that our lifetime will not go beyond that of itself.
  MediaTrackGraphImpl* const mGraph;
  // State being handed over to the graph through OneIteration. Protected by
  // mMonitor.
  Maybe<IterationState> mIterationState MOZ_GUARDED_BY(mMonitor);
  // Result from mGraph's OneIteration. Protected by mMonitor.
  IterationResult mIterationResult MOZ_GUARDED_BY(mMonitor);

  enum class ThreadState {
    Wait,      // Waiting for a message.  This is the initial state.
               // A transition from Run back to Wait occurs on the runner thread
               // after it processes as far as mIterationState->mStateTime
               // and sets mIterationResult.
    Run,       // Set on driver thread after each mIterationState update.
    Shutdown,  // Set when Shutdown() is called on main thread.
  };
  // Protected by mMonitor until set to Shutdown, after which this is not
  // modified.
  ThreadState mThreadState MOZ_GUARDED_BY(mMonitor);

  // The thread running mGraph.  Set on construction, after other members are
  // initialized.  Cleared at the end of Shutdown().
  const nsCOMPtr<nsIThread> mThread;

#ifdef DEBUG
  // Set to mGraph's audio callback driver's thread id, if run by an
  // AudioCallbackDriver, while OneIteration() is running.
  std::thread::id mAudioDriverThreadId = std::thread::id();
  // Set to mGraph's system clock driver's thread, if run by a
  // SystemClockDriver, while OneIteration() is running.
  nsIThread* mClockDriverThread = nullptr;
#endif
};

}  // namespace mozilla

#endif
