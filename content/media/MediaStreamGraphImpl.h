/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASTREAMGRAPHIMPL_H_
#define MOZILLA_MEDIASTREAMGRAPHIMPL_H_

#include "MediaStreamGraph.h"

#include "mozilla/Monitor.h"
#include "mozilla/TimeStamp.h"
#include "nsIMemoryReporter.h"
#include "nsIThread.h"
#include "nsIRunnable.h"
#include "Latency.h"
#include "mozilla/WeakPtr.h"
#include "GraphDriver.h"
#include "AudioMixer.h"

namespace mozilla {

template <typename T>
class LinkedList;
#ifdef MOZ_WEBRTC
class AudioOutputObserver;
#endif

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
  // This must not add new control messages to the graph.
  virtual void RunDuringShutdown() {}
  MediaStream* GetStream() { return mStream; }

protected:
  // We do not hold a reference to mStream. The graph will be holding
  // a reference to the stream until the Destroy message is processed. The
  // last message referencing a stream is the Destroy message for that stream.
  MediaStream* mStream;
};

class MessageBlock {
public:
  int64_t mGraphUpdateIndex;
  nsTArray<nsAutoPtr<ControlMessage> > mMessages;
};

/**
 * The implementation of a media stream graph. This class is private to this
 * file. It's not in the anonymous namespace because MediaStream needs to
 * be able to friend it.
 *
 * Currently we have one global instance per process, and one per each
 * OfflineAudioContext object.
 */
class MediaStreamGraphImpl : public MediaStreamGraph,
                             public nsIMemoryReporter {
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  /**
   * Set aRealtime to true in order to create a MediaStreamGraph which provides
   * support for real-time audio and video.  Set it to false in order to create
   * a non-realtime instance which just churns through its inputs and produces
   * output.  Those objects currently only support audio, and are used to
   * implement OfflineAudioContext.  They do not support MediaStream inputs.
   */
  explicit MediaStreamGraphImpl(bool aRealtime,
                                TrackRate aSampleRate,
                                DOMMediaStream::TrackTypeHints aHint,
                                dom::AudioChannel aChannel = dom::AudioChannel::Normal);

  /**
   * Unregisters memory reporting and deletes this instance. This should be
   * called instead of calling the destructor directly.
   */
  void Destroy();

  // Main thread only.
  /**
   * This runs every time we need to sync state from the media graph thread
   * to the main thread while the main thread is not in the middle
   * of a script. It runs during a "stable state" (per HTML5) or during
   * an event posted to the main thread.
   * The boolean affects which boolean controlling runnable dispatch is cleared
   */
  void RunInStableState(bool aSourceIsMSG);
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
  void AssertOnGraphThreadOrNotRunning() {
    // either we're on the right thread (and calling CurrentDriver() is safe),
    // or we're going to assert anyways, so don't cross-check CurrentDriver
    MOZ_ASSERT(mDriver->OnThread() ||
               (mLifecycleState > LIFECYCLE_RUNNING && NS_IsMainThread()));
  }
  /*
   * This does the actual iteration: Message processing, MediaStream ordering,
   * blocking computation and processing.
   */
  void DoIteration();

  bool OneIteration(GraphTime aFrom, GraphTime aTo,
                    GraphTime aStateFrom, GraphTime aStateEnd);

  bool Running() {
    mMonitor.AssertCurrentThreadOwns();
    return mLifecycleState == LIFECYCLE_RUNNING;
  }

  // Get the message queue, from the current GraphDriver thread.
  nsTArray<MessageBlock>& MessageQueue() {
    mMonitor.AssertCurrentThreadOwns();
    return mFrontMessageQueue;
  }

  /* This is the end of the current iteration, that is, the current time of the
   * graph. */
  GraphTime IterationEnd();
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
   * Advance all stream state to the new current time.
   */
  void UpdateCurrentTimeForStreams(GraphTime aPrevCurrentTime,
                                   GraphTime aNextCurrentTime);
  /**
   * Process graph message for this iteration, update stream processing order,
   * and recompute stream blocking until aEndBlockingDecisions.
   */
  void UpdateGraph(GraphTime aEndBlockingDecisions);

  void SwapMessageQueues() {
    mMonitor.AssertCurrentThreadOwns();
    mFrontMessageQueue.SwapElements(mBackMessageQueue);
  }
  /**
   * Do all the processing and play the audio and video, ffrom aFrom to aTo.
   */
  void Process(GraphTime aFrom, GraphTime aTo);
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
   * Returns smallest value of t such that
   * TimeToTicksRoundUp(aSampleRate, t) is a multiple of WEBAUDIO_BLOCK_SIZE
   * and floor(TimeToTicksRoundUp(aSampleRate, t)/WEBAUDIO_BLOCK_SIZE) >
   * floor(TimeToTicksRoundUp(aSampleRate, aTime)/WEBAUDIO_BLOCK_SIZE).
   */
  GraphTime RoundUpToNextAudioBlock(GraphTime aTime);
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
  void CreateOrDestroyAudioStreams(GraphTime aAudioOutputStartTime, MediaStream* aStream);
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

  TrackRate AudioSampleRate() const { return mSampleRate; }
  TrackRate GraphRate() const { return mSampleRate; }
  // Always stereo for now.
  uint32_t AudioChannelCount() { return 2; }

  double MediaTimeToSeconds(GraphTime aTime)
  {
    return TrackTicksToSeconds(GraphRate(), aTime);
  }
  GraphTime SecondsToMediaTime(double aS)
  {
    return SecondsToTicksRoundDown(GraphRate(), aS);
  }
  GraphTime MillisecondsToMediaTime(int32_t aMS)
  {
    return RateConvertTicksRoundDown(GraphRate(), 1000, aMS);
  }

  TrackTicks TimeToTicksRoundDown(TrackRate aRate, StreamTime aTime)
  {
    return RateConvertTicksRoundDown(aRate, GraphRate(), aTime);
  }

  /**
   * Signal to the graph that the thread has paused indefinitly,
   * or resumed.
   */
  void PausedIndefinitly();
  void ResumedFromPaused();

  /**
   * Not safe to call off the MediaStreamGraph thread unless monitor is held!
   */
  GraphDriver* CurrentDriver() {
#ifdef DEBUG
    // #ifdef since we're not wrapping it all in MOZ_ASSERT()
    if (!mDriver->OnThread()) {
      mMonitor.AssertCurrentThreadOwns();
    }
#endif
    return mDriver;
  }

  /**
   * Effectively set the new driver, while we are switching.
   * It is only safe to call this at the very end of an iteration, when there
   * has been a SwitchAtNextIteration call during the iteration. The driver
   * should return and pass the control to the new driver shortly after.
   * We can also switch from Revive() (on MainThread), in which case the
   * monitor is held
   */
  void SetCurrentDriver(GraphDriver* aDriver) {
#ifdef DEBUG
    // #ifdef since we're not wrapping it all in MOZ_ASSERT()
    if (!mDriver->OnThread()) {
      mMonitor.AssertCurrentThreadOwns();
    }
#endif
    mDriver = aDriver;
  }

  Monitor& GetMonitor() {
    return mMonitor;
  }

  /**
   * Must implement here to avoid dangerous data races around CurrentDriver() -
   * we don't want stuff off MSG thread using "graph->CurrentDriver()->EnsureNextIteration()"
   * because CurrentDriver may change (and it's a TSAN data race)
   */
  void EnsureNextIteration() {
    MonitorAutoLock mon(mMonitor);
    CurrentDriver()->EnsureNextIterationLocked();
  }

  // Data members
  //
  /**
   * Graphs own owning references to their driver, until shutdown. When a driver
   * switch occur, previous driver is either deleted, or it's ownership is
   * passed to a event that will take care of the asynchronous cleanup, as
   * audio stream can take some time to shut down.
   */
  nsRefPtr<GraphDriver> mDriver;

  // The following state is managed on the graph thread only, unless
  // mLifecycleState > LIFECYCLE_RUNNING in which case the graph thread
  // is not running and this state can be used from the main thread.

  /**
   * The graph keeps a reference to each stream.
   * References are maintained manually to simplify reordering without
   * unnecessary thread-safe refcount changes.
   */
  nsTArray<MediaStream*> mStreams;
  /**
   * Streams from mFirstCycleBreaker to the end of mStreams produce output
   * before they receive input.  They correspond to DelayNodes that are in
   * cycles.
   */
  uint32_t mFirstCycleBreaker;
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
  /**
   * A list of batches of messages to process. Each batch is processed
   * as an atomic unit.
   */
  /* Message queue processed by the MSG thread during an iteration. */
  nsTArray<MessageBlock> mFrontMessageQueue;
  /* Message queue in which the main thread appends messages. */
  nsTArray<MessageBlock> mBackMessageQueue;

  /* True if there will messages to process if we swap the message queues. */
  bool MessagesQueued() {
    mMonitor.AssertCurrentThreadOwns();
    return !mBackMessageQueue.IsEmpty();
  }
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
   *
   * This should be kept in sync with the LifecycleState_str array in
   * MediaStreamGraph.cpp
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
   * The graph should stop processing at or after this time.
   */
  GraphTime mEndTime;

  /**
   * Sample rate at which this graph runs. For real time graphs, this is
   * the rate of the audio mixer. For offline graphs, this is the rate specified
   * at construction.
   */
  TrackRate mSampleRate;
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
  AudioMixer mMixer;
#ifdef MOZ_WEBRTC
  nsRefPtr<AudioOutputObserver> mFarendObserverRef;
#endif

private:
  virtual ~MediaStreamGraphImpl();

  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

  /**
   * Used to signal that a memory report has been requested.
   */
  Monitor mMemoryReportMonitor;
  /**
   * This class uses manual memory management, and all pointers to it are raw
   * pointers. However, in order for it to implement nsIMemoryReporter, it needs
   * to implement nsISupports and so be ref-counted. So it maintains a single
   * nsRefPtr to itself, giving it a ref-count of 1 during its entire lifetime,
   * and Destroy() nulls this self-reference in order to trigger self-deletion.
   */
  nsRefPtr<MediaStreamGraphImpl> mSelfRef;
  /**
   * Used to pass memory report information across threads.
   */
  nsTArray<AudioNodeSizes> mAudioStreamSizes;
  /**
   * Indicates that the MSG thread should gather data for a memory report.
   */
  bool mNeedsMemoryReport;

#ifdef DEBUG
  /**
   * Used to assert when AppendMessage() runs ControlMessages synchronously.
   */
  bool mCanRunMessagesSynchronously;
#endif

};

}

#endif /* MEDIASTREAMGRAPHIMPL_H_ */
