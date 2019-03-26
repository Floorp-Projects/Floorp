/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_GraphRunner_h
#define mozilla_GraphRunner_h

#include "MediaSegment.h"
#include "mozilla/Monitor.h"

#include <thread>

struct PRThread;

namespace mozilla {

class GraphDriver;
class MediaStreamGraphImpl;

class GraphRunner {
 public:
  explicit GraphRunner(MediaStreamGraphImpl* aGraph);
  ~GraphRunner();

  /**
   * Marks us as shut down and signals mThread, so that it runs until the end.
   */
  void Shutdown();

  /**
   * Signals one iteration of mGraph. Hands aStateEnd over to mThread and runs
   * the iteration there.
   */
  bool OneIteration(GraphTime aStateEnd);

  /**
   * Runs mGraph until it shuts down.
   */
  void Run();

  /**
   * Returns true if called on mThread.
   */
  bool OnThread();

#ifdef DEBUG
  /**
   * Returns true if called on mThread, and aDriver was the driver that called
   * OneIteration() last.
   */
  bool RunByGraphDriver(GraphDriver* aDriver);
#endif

 private:
  // Monitor used for yielding mThread through Wait(), and scheduling mThread
  // through Signal() from a GraphDriver.
  Monitor mMonitor;
  // The MediaStreamGraph we're running. Weakptr beecause this graph owns us and
  // guarantees that our lifetime will not go beyond that of itself.
  MediaStreamGraphImpl* const mGraph;
  // GraphTime being handed over to the graph through OneIteration. Protected by
  // mMonitor.
  GraphTime mStateEnd;
  // Reply from mGraph's OneIteration. Protected by mMonitor.
  bool mStillProcessing;

  enum class ThreadState {
    Wait,      // Waiting for a message.  This is the initial state.
               // A transition from Run back to Wait occurs on the runner
               // thread after it processes as far as mStateEnd and sets
               // mStillProcessing.
    Run,       // Set on driver thread after each mStateEnd update.
    Shutdown,  // Set when Shutdown() is called on main thread.
  };
  // Protected by mMonitor until set to Shutdown, after which this is not
  // modified.
  ThreadState mThreadState;

  // The thread running mGraph.  Set on construction, after other members are
  // initialized.  Cleared at the end of Shutdown().
  PRThread* mThread;

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
