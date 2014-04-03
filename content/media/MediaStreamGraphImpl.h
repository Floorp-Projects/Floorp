/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASTREAMGRAPHIMPL_H_
#define MOZILLA_MEDIASTREAMGRAPHIMPL_H_

#include "MediaStreamGraph.h"

#include "mozilla/Monitor.h"
#include "mozilla/TimeStamp.h"
#include "nsIThread.h"
#include "nsIRunnable.h"
#include "Latency.h"
#include "mozilla/WeakPtr.h"

namespace mozilla {

template <typename T>
class LinkedList;

class AudioMixer;

/**
 * Assume we can run an iteration of the MediaStreamGraph loop in this much time
 * or less.
 * We try to run the control loop at this rate.
 */
static const int MEDIA_GRAPH_TARGET_PERIOD_MS = 10;

/**
 * Assume that we might miss our scheduled wakeup of the MediaStreamGraph by
 * this much.
 */
static const int SCHEDULE_SAFETY_MARGIN_MS = 10;

/**
 * Try have this much audio buffered in streams and queued to the hardware.
 * The maximum delay to the end of the next control loop
 * is 2*MEDIA_GRAPH_TARGET_PERIOD_MS + SCHEDULE_SAFETY_MARGIN_MS.
 * There is no point in buffering more audio than this in a stream at any
 * given time (until we add processing).
 * This is not optimal yet.
 */
static const int AUDIO_TARGET_MS = 2*MEDIA_GRAPH_TARGET_PERIOD_MS +
    SCHEDULE_SAFETY_MARGIN_MS;

/**
 * Try have this much video buffered. Video frames are set
 * near the end of the iteration of the control loop. The maximum delay
 * to the setting of the next video frame is 2*MEDIA_GRAPH_TARGET_PERIOD_MS +
 * SCHEDULE_SAFETY_MARGIN_MS. This is not optimal yet.
 */
static const int VIDEO_TARGET_MS = 2*MEDIA_GRAPH_TARGET_PERIOD_MS +
    SCHEDULE_SAFETY_MARGIN_MS;

/**
 * A per-stream update message passed from the media graph thread to the
 * main thread.
 */
struct StreamUpdate {
  int64_t mGraphUpdateIndex;
  nsRefPtr<MediaStream> mStream;
  StreamTime mNextMainThreadCurrentTime;
  bool mNextMainThreadFinished;
};

/**
 * This represents a message passed from the main thread to the graph thread.
 * A ControlMessage always has a weak reference a particular affected stream.
 */
class ControlMessage {
public:
  explicit ControlMessage(MediaStream* aStream) : mStream(aStream)
  {
    MOZ_COUNT_CTOR(ControlMessage);
  }
  // All these run on the graph thread
  virtual ~ControlMessage()
  {
    MOZ_COUNT_DTOR(ControlMessage);
  }
  // Do the action of this message on the MediaStreamGraph thread. Any actions
  // affecting graph processing should take effect at mStateComputedTime.
  // All stream data for times < mStateComputedTime has already been
  // computed.
  virtual void Run() = 0;
  // When we're shutting down the application, most messages are ignored but
  // some cleanup messages should still be processed (on the main thread).
  virtual void RunDuringShutdown() {}
  MediaStream* GetStream() { return mStream; }

protected:
  // We do not hold a reference to mStream. The graph will be holding
  // a reference to the stream until the Destroy message is processed. The
  // last message referencing a stream is the Destroy message for that stream.
  MediaStream* mStream;
};

/**
 * The implementation of a media stream graph. This class is private to this
 * file. It's not in the anonymous namespace because MediaStream needs to
 * be able to friend it.
 *
 * Currently we have one global instance per process, and one per each
 * OfflineAudioContext object.
 */
class MediaStreamGraphImpl : public MediaStreamGraph {
public:
  /**
   * Set aRealtime to true in order to create a MediaStreamGraph which provides
   * support for real-time audio and video.  Set it to false in order to create
   * a non-realtime instance which just churns through its inputs and produces
   * output.  Those objects currently only support audio, and are used to
   * implement OfflineAudioContext.  They do not support MediaStream inputs.
   */
  explicit MediaStreamGraphImpl(bool aRealtime);
  virtual ~MediaStreamGraphImpl();

  // Main thread only.
  /**
   * This runs every time we need to sync state from the media graph thread
   * to the main thread while the main thread is not in the middle
   * of a script. It runs during a "stable state" (per HTML5) or during
   * an event posted to the main thread.
   */
  void RunInStableState();
  /**
   * Ensure a runnable to run RunInStableState is posted to the appshell to
   * run at the next stable state (per HTML5).
   * See EnsureStableStateEventPosted.
   */
  void EnsureRunInStableState();
  /**
   * Called to apply a StreamUpdate to its stream.
   */
  void ApplyStreamUpdate(StreamUpdate* aUpdate);
  /**
   * Append a ControlMessage to the message queue. This queue is drained
   * during RunInStableState; the messages will run on the graph thread.
   */
  void AppendMessage(ControlMessage* aMessage);
  /**
   * Make this MediaStreamGraph enter forced-shutdown state. This state
   * will be noticed by the media graph thread, which will shut down all streams
   * and other state controlled by the media graph thread.
   * This is called during application shutdown.
   */
  void ForceShutDown();
  /**
   * Shutdown() this MediaStreamGraph's threads and return when they've shut down.
   */
  void ShutdownThreads();

  /**
   * Called before the thread runs.
   */
  void Init();
  // The following methods run on the graph thread (or possibly the main thread if
  // mLifecycleState > LIFECYCLE_RUNNING)
  /**
   * Runs main control loop on the graph thread. Normally a single invocation
   * of this runs for the entire lifetime of the graph thread.
   */
  void RunThread();
  /**
   * Call this to indicate that another iteration of the control loop is
   * required on its regular schedule. The monitor must not be held.
   */
  void EnsureNextIteration();
  /**
   * As above, but with the monitor already held.
   */
  void EnsureNextIterationLocked(MonitorAutoLock& aLock);
  /**
   * Call this to indicate that another iteration of the control loop is
   * required immediately. The monitor must already be held.
   */
  void EnsureImmediateWakeUpLocked(MonitorAutoLock& aLock);
  /**
   * Ensure there is an event posted to the main thread to run RunInStableState.
   * mMonitor must be held.
   * See EnsureRunInStableState
   */
  void EnsureStableStateEventPosted();
  /**
   * Generate messages to the main thread to update it for all state changes.
   * mMonitor must be held.
   */
  void PrepareUpdatesToMainThreadState(bool aFinalUpdate);
  /**
   * Returns false if there is any stream that has finished but not yet finished
   * playing out.
   */
  bool AllFinishedStreamsNotified();
  /**
   * If we are rendering in non-realtime mode, we don't want to send messages to
   * the main thread at each iteration for performance reasons. We instead
   * notify the main thread at the same rate
   */
  bool ShouldUpdateMainThread();
  // The following methods are the various stages of RunThread processing.
  /**
   * Compute a new current time for the graph and advance all on-graph-thread
   * state to the new current time.
   */
  void UpdateCurrentTime();
  /**
   * Update the consumption state of aStream to reflect whether its data
   * is needed or not.
   */
  void UpdateConsumptionState(SourceMediaStream* aStream);
  /**
   * Extract any state updates pending in aStream, and apply them.
   */
  void ExtractPendingInput(SourceMediaStream* aStream,
                           GraphTime aDesiredUpToTime,
                           bool* aEnsureNextIteration);
  /**
   * Update "have enough data" flags in aStream.
   */
  void UpdateBufferSufficiencyState(SourceMediaStream* aStream);
  /*
   * If aStream hasn't already been ordered, push it onto aStack and order
   * its children.
   */
  void UpdateStreamOrderForStream(mozilla::LinkedList<MediaStream>* aStack,
                                  already_AddRefed<MediaStream> aStream);
  /**
   * Mark aStream and all its inputs (recursively) as consumed.
   */
  static void MarkConsumed(MediaStream* aStream);
  /**
   * Sort mStreams so that every stream not in a cycle is after any streams
   * it depends on, and every stream in a cycle is marked as being in a cycle.
   * Also sets mIsConsumed on every stream.
   */
  void UpdateStreamOrder();
  /**
   * Compute the blocking states of streams from mStateComputedTime
   * until the desired future time aEndBlockingDecisions.
   * Updates mStateComputedTime and sets MediaStream::mBlocked
   * for all streams.
   */
  void RecomputeBlocking(GraphTime aEndBlockingDecisions);
  // The following methods are used to help RecomputeBlocking.
  /**
   * If aStream isn't already in aStreams, add it and recursively call
   * AddBlockingRelatedStreamsToSet on all the streams whose blocking
   * status could depend on or affect the state of aStream.
   */
  void AddBlockingRelatedStreamsToSet(nsTArray<MediaStream*>* aStreams,
                                      MediaStream* aStream);
  /**
   * Mark a stream blocked at time aTime. If this results in decisions that need
   * to be revisited at some point in the future, *aEnd will be reduced to the
   * first time in the future to recompute those decisions.
   */
  void MarkStreamBlocking(MediaStream* aStream);
  /**
   * Recompute blocking for the streams in aStreams for the interval starting at aTime.
   * If this results in decisions that need to be revisited at some point
   * in the future, *aEnd will be reduced to the first time in the future to
   * recompute those decisions.
   */
  void RecomputeBlockingAt(const nsTArray<MediaStream*>& aStreams,
                           GraphTime aTime, GraphTime aEndBlockingDecisions,
                           GraphTime* aEnd);
  /**
   * Produce data for all streams >= aStreamIndex for the given time interval.
   * Advances block by block, each iteration producing data for all streams
   * for a single block.
   * This is called whenever we have an AudioNodeStream in the graph.
   */
  void ProduceDataForStreamsBlockByBlock(uint32_t aStreamIndex,
                                         TrackRate aSampleRate,
                                         GraphTime aFrom,
                                         GraphTime aTo);
  /**
   * Returns true if aStream will underrun at aTime for its own playback.
   * aEndBlockingDecisions is when we plan to stop making blocking decisions.
   * *aEnd will be reduced to the first time in the future to recompute these
   * decisions.
   */
  bool WillUnderrun(MediaStream* aStream, GraphTime aTime,
                    GraphTime aEndBlockingDecisions, GraphTime* aEnd);
  /**
   * Given a graph time aTime, convert it to a stream time taking into
   * account the time during which aStream is scheduled to be blocked.
   */
  StreamTime GraphTimeToStreamTime(MediaStream* aStream, GraphTime aTime);
  /**
   * Given a graph time aTime, convert it to a stream time taking into
   * account the time during which aStream is scheduled to be blocked, and
   * when we don't know whether it's blocked or not, we assume it's not blocked.
   */
  StreamTime GraphTimeToStreamTimeOptimistic(MediaStream* aStream, GraphTime aTime);
  enum {
    INCLUDE_TRAILING_BLOCKED_INTERVAL = 0x01
  };
  /**
   * Given a stream time aTime, convert it to a graph time taking into
   * account the time during which aStream is scheduled to be blocked.
   * aTime must be <= mStateComputedTime since blocking decisions
   * are only known up to that point.
   * If aTime is exactly at the start of a blocked interval, then the blocked
   * interval is included in the time returned if and only if
   * aFlags includes INCLUDE_TRAILING_BLOCKED_INTERVAL.
   */
  GraphTime StreamTimeToGraphTime(MediaStream* aStream, StreamTime aTime,
                                  uint32_t aFlags = 0);
  /**
   * Get the current audio position of the stream's audio output.
   */
  GraphTime GetAudioPosition(MediaStream* aStream);
  /**
   * Call NotifyHaveCurrentData on aStream's listeners.
   */
  void NotifyHasCurrentData(MediaStream* aStream);
  /**
   * If aStream needs an audio stream but doesn't have one, create it.
   * If aStream doesn't need an audio stream but has one, destroy it.
   */
  void CreateOrDestroyAudioStreams(GraphTime aAudioOutputStartTime,
                                   MediaStream* aStream);
  /**
   * Queue audio (mix of stream audio and silence for blocked intervals)
   * to the audio output stream. Returns the number of frames played.
   */
  TrackTicks PlayAudio(MediaStream* aStream, GraphTime aFrom, GraphTime aTo);
  /**
   * Set the correct current video frame for stream aStream.
   */
  void PlayVideo(MediaStream* aStream);
  /**
   * No more data will be forthcoming for aStream. The stream will end
   * at the current buffer end point. The StreamBuffer's tracks must be
   * explicitly set to finished by the caller.
   */
  void FinishStream(MediaStream* aStream);
  /**
   * Compute how much stream data we would like to buffer for aStream.
   */
  StreamTime GetDesiredBufferEnd(MediaStream* aStream);
  /**
   * Returns true when there are no active streams.
   */
  bool IsEmpty() { return mStreams.IsEmpty() && mPortCount == 0; }

  // For use by control messages, on graph thread only.
  /**
   * Identify which graph update index we are currently processing.
   */
  int64_t GetProcessingGraphUpdateIndex() { return mProcessingGraphUpdateIndex; }
  /**
   * Add aStream to the graph and initializes its graph-specific state.
   */
  void AddStream(MediaStream* aStream);
  /**
   * Remove aStream from the graph. Ensures that pending messages about the
   * stream back to the main thread are flushed.
   */
  void RemoveStream(MediaStream* aStream);
  /**
   * Remove aPort from the graph and release it.
   */
  void DestroyPort(MediaInputPort* aPort);
  /**
   * Mark the media stream order as dirty.
   */
  void SetStreamOrderDirty()
  {
    mStreamOrderDirty = true;
  }
  /**
   * Pause all AudioStreams being written to by MediaStreams
   */
  void PauseAllAudioOutputs();
  /**
   * Resume all AudioStreams being written to by MediaStreams
   */
  void ResumeAllAudioOutputs();

  // Data members

  /**
   * Media graph thread.
   * Readonly after initialization on the main thread.
   */
  nsCOMPtr<nsIThread> mThread;

  // The following state is managed on the graph thread only, unless
  // mLifecycleState > LIFECYCLE_RUNNING in which case the graph thread
  // is not running and this state can be used from the main thread.

  nsTArray<nsRefPtr<MediaStream> > mStreams;
  /**
   * mOldStreams is used as temporary storage for streams when computing the
   * order in which we compute them.
   */
  nsTArray<nsRefPtr<MediaStream> > mOldStreams;
  /**
   * The current graph time for the current iteration of the RunThread control
   * loop.
   */
  GraphTime mCurrentTime;
  /**
   * Blocking decisions and all stream contents have been computed up to this
   * time. The next batch of updates from the main thread will be processed
   * at this time. Always >= mCurrentTime.
   */
  GraphTime mStateComputedTime;
  /**
   * This is only used for logging.
   */
  TimeStamp mInitialTimeStamp;
  /**
   * The real timestamp of the latest run of UpdateCurrentTime.
   */
  TimeStamp mCurrentTimeStamp;
  /**
   * Date of the last time we updated the main thread with the graph state.
   */
  TimeStamp mLastMainThreadUpdate;
  /**
   * Which update batch we are currently processing.
   */
  int64_t mProcessingGraphUpdateIndex;
  /**
   * Number of active MediaInputPorts
   */
  int32_t mPortCount;

  // mMonitor guards the data below.
  // MediaStreamGraph normally does its work without holding mMonitor, so it is
  // not safe to just grab mMonitor from some thread and start monkeying with
  // the graph. Instead, communicate with the graph thread using provided
  // mechanisms such as the ControlMessage queue.
  Monitor mMonitor;

  // Data guarded by mMonitor (must always be accessed with mMonitor held,
  // regardless of the value of mLifecycleState.

  /**
   * State to copy to main thread
   */
  nsTArray<StreamUpdate> mStreamUpdates;
  /**
   * Runnables to run after the next update to main thread state.
   */
  nsTArray<nsCOMPtr<nsIRunnable> > mUpdateRunnables;
  struct MessageBlock {
    int64_t mGraphUpdateIndex;
    nsTArray<nsAutoPtr<ControlMessage> > mMessages;
  };
  /**
   * A list of batches of messages to process. Each batch is processed
   * as an atomic unit.
   */
  nsTArray<MessageBlock> mMessageQueue;
  /**
   * This enum specifies where this graph is in its lifecycle. This is used
   * to control shutdown.
   * Shutdown is tricky because it can happen in two different ways:
   * 1) Shutdown due to inactivity. RunThread() detects that it has no
   * pending messages and no streams, and exits. The next RunInStableState()
   * checks if there are new pending messages from the main thread (true only
   * if new stream creation raced with shutdown); if there are, it revives
   * RunThread(), otherwise it commits to shutting down the graph. New stream
   * creation after this point will create a new graph. An async event is
   * dispatched to Shutdown() the graph's threads and then delete the graph
   * object.
   * 2) Forced shutdown at application shutdown, or completion of a
   * non-realtime graph. A flag is set, RunThread() detects the flag and
   * exits, the next RunInStableState() detects the flag, and dispatches the
   * async event to Shutdown() the graph's threads. However the graph object
   * is not deleted. New messages for the graph are processed synchronously on
   * the main thread if necessary. When the last stream is destroyed, the
   * graph object is deleted.
   */
  enum LifecycleState {
    // The graph thread hasn't started yet.
    LIFECYCLE_THREAD_NOT_STARTED,
    // RunThread() is running normally.
    LIFECYCLE_RUNNING,
    // In the following states, the graph thread is not running so
    // all "graph thread only" state in this class can be used safely
    // on the main thread.
    // RunThread() has exited and we're waiting for the next
    // RunInStableState(), at which point we can clean up the main-thread
    // side of the graph.
    LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP,
    // RunInStableState() posted a ShutdownRunnable, and we're waiting for it
    // to shut down the graph thread(s).
    LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN,
    // Graph threads have shut down but we're waiting for remaining streams
    // to be destroyed. Only happens during application shutdown and on
    // completed non-realtime graphs, since normally we'd only shut down a
    // realtime graph when it has no streams.
    LIFECYCLE_WAITING_FOR_STREAM_DESTRUCTION
  };
  LifecycleState mLifecycleState;
  /**
   * This enum specifies the wait state of the graph thread.
   */
  enum WaitState {
    // RunThread() is running normally
    WAITSTATE_RUNNING,
    // RunThread() is paused waiting for its next iteration, which will
    // happen soon
    WAITSTATE_WAITING_FOR_NEXT_ITERATION,
    // RunThread() is paused indefinitely waiting for something to change
    WAITSTATE_WAITING_INDEFINITELY,
    // Something has signaled RunThread() to wake up immediately,
    // but it hasn't done so yet
    WAITSTATE_WAKING_UP
  };
  WaitState mWaitState;
  /**
   * The graph should stop processing at or after this time.
   */
  GraphTime mEndTime;
  /**
   * True when another iteration of the control loop is required.
   */
  bool mNeedAnotherIteration;
  /**
   * True when we need to do a forced shutdown during application shutdown.
   */
  bool mForceShutDown;
  /**
   * True when we have posted an event to the main thread to run
   * RunInStableState() and the event hasn't run yet.
   */
  bool mPostedRunInStableStateEvent;

  // Main thread only

  /**
   * Messages posted by the current event loop task. These are forwarded to
   * the media graph thread during RunInStableState. We can't forward them
   * immediately because we want all messages between stable states to be
   * processed as an atomic batch.
   */
  nsTArray<nsAutoPtr<ControlMessage> > mCurrentTaskMessageQueue;
  /**
   * True when RunInStableState has determined that mLifecycleState is >
   * LIFECYCLE_RUNNING. Since only the main thread can reset mLifecycleState to
   * LIFECYCLE_RUNNING, this can be relied on to not change unexpectedly.
   */
  bool mDetectedNotRunning;
  /**
   * True when a stable state runner has been posted to the appshell to run
   * RunInStableState at the next stable state.
   */
  bool mPostedRunInStableState;
  /**
   * True when processing real-time audio/video.  False when processing non-realtime
   * audio.
   */
  bool mRealtime;
  /**
   * True when a non-realtime MediaStreamGraph has started to process input.  This
   * value is only accessed on the main thread.
   */
  bool mNonRealtimeProcessing;
  /**
   * True when a change has happened which requires us to recompute the stream
   * blocking order.
   */
  bool mStreamOrderDirty;
  /**
   * Hold a ref to the Latency logger
   */
  nsRefPtr<AsyncLatencyLogger> mLatencyLog;
  /**
   * If this is not null, all the audio output for the MSG will be mixed down.
   */
  nsAutoPtr<AudioMixer> mMixer;
};

}

#endif /* MEDIASTREAMGRAPHIMPL_H_ */
