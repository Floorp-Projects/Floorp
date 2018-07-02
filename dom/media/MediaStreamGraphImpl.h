/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASTREAMGRAPHIMPL_H_
#define MOZILLA_MEDIASTREAMGRAPHIMPL_H_

#include "MediaStreamGraph.h"

#include "AudioMixer.h"
#include "GraphDriver.h"
#include "Latency.h"
#include "mozilla/Atomics.h"
#include "mozilla/Monitor.h"
#include "mozilla/Services.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "nsDataHashtable.h"
#include "nsIMemoryReporter.h"
#include "nsINamed.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsITimer.h"
#include "AsyncLogger.h"

namespace mozilla {

namespace media {
class ShutdownTicket;
}

template <typename T>
class LinkedList;

/**
 * A per-stream update message passed from the media graph thread to the
 * main thread.
 */
struct StreamUpdate
{
  RefPtr<MediaStream> mStream;
  StreamTime mNextMainThreadCurrentTime;
  bool mNextMainThreadFinished;
};

/**
 * This represents a message run on the graph thread to modify stream or graph
 * state.  These are passed from main thread to graph thread through
 * AppendMessage(), or scheduled on the graph thread with
 * RunMessageAfterProcessing().  A ControlMessage
 * always has a weak reference to a particular affected stream.
 */
class ControlMessage
{
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
  // affecting graph processing should take effect at mProcessedTime.
  // All stream data for times < mProcessedTime has already been
  // computed.
  virtual void Run() = 0;
  // RunDuringShutdown() is only relevant to messages generated on the main
  // thread (for AppendMessage()).
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

class MessageBlock
{
public:
  nsTArray<UniquePtr<ControlMessage>> mMessages;
};

/**
 * The implementation of a media stream graph. This class is private to this
 * file. It's not in the anonymous namespace because MediaStream needs to
 * be able to friend it.
 *
 * There can be multiple MediaStreamGraph per process: one per document.
 * Additionaly, each OfflineAudioContext object creates its own MediaStreamGraph
 * object too.
 */
class MediaStreamGraphImpl : public MediaStreamGraph,
                             public nsIMemoryReporter,
                             public nsITimerCallback,
                             public nsINamed
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  /**
   * Use aGraphDriverRequested with SYSTEM_THREAD_DRIVER or AUDIO_THREAD_DRIVER
   * to create a MediaStreamGraph which provides support for real-time audio
   * and/or video.  Set it to OFFLINE_THREAD_DRIVER in order to create a
   * non-realtime instance which just churns through its inputs and produces
   * output.  Those objects currently only support audio, and are used to
   * implement OfflineAudioContext.  They do not support MediaStream inputs.
   */
  explicit MediaStreamGraphImpl(GraphDriverType aGraphDriverRequested,
                                TrackRate aSampleRate,
                                AbstractThread* aWindow);

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
  void AppendMessage(UniquePtr<ControlMessage> aMessage);

  /**
   * Dispatches a runnable from any thread to the correct main thread for this
   * MediaStreamGraph.
   */
  void Dispatch(already_AddRefed<nsIRunnable>&& aRunnable);

  /**
   * Make this MediaStreamGraph enter forced-shutdown state. This state
   * will be noticed by the media graph thread, which will shut down all streams
   * and other state controlled by the media graph thread.
   * This is called during application shutdown.
   */
  void ForceShutDown(media::ShutdownTicket* aShutdownTicket);

  /**
   * Called before the thread runs.
   */
  void Init();

  /**
   * Respond to CollectReports with sizes collected on the graph thread.
   */
  static void
  FinishCollectReports(nsIHandleReportCallback* aHandleReport,
                       nsISupports* aData,
                       const nsTArray<AudioNodeSizes>& aAudioStreamSizes);

  // The following methods run on the graph thread (or possibly the main thread
  // if mLifecycleState > LIFECYCLE_RUNNING)
  void CollectSizesForMemoryReport(
         already_AddRefed<nsIHandleReportCallback> aHandleReport,
         already_AddRefed<nsISupports> aHandlerData);

  /**
   * Returns true if this MediaStreamGraph should keep running
   */
  bool UpdateMainThreadState();

  /**
   * Returns true if this MediaStreamGraph should keep running
   */
  bool OneIteration(GraphTime aStateEnd);

  /**
   * Called from the driver, when the graph thread is about to stop, to tell
   * the main thread to attempt to begin cleanup.  The main thread may either
   * shutdown or revive the graph depending on whether it receives new
   * messages.
   */
  void SignalMainThreadCleanup();

  bool Running() const
  {
    return LifecycleStateRef() == LIFECYCLE_RUNNING;
  }

  /* This is the end of the current iteration, that is, the current time of the
   * graph. */
  GraphTime IterationEnd() const;

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
   * Advance all stream state to mStateComputedTime.
   */
  void UpdateCurrentTimeForStreams(GraphTime aPrevCurrentTime);
  /**
   * Process chunks for all streams and raise events for properties that have
   * changed, such as principalId.
   */
  void ProcessChunkMetadata(GraphTime aPrevCurrentTime);
  /**
   * Process chunks for the given stream and interval, and raise events for
   * properties that have changed, such as principalId.
   */
  template<typename C, typename Chunk>
  void ProcessChunkMetadataForInterval(MediaStream* aStream,
                                       TrackID aTrackID,
                                       C& aSegment,
                                       StreamTime aStart,
                                       StreamTime aEnd);
  /**
   * Process graph messages in mFrontMessageQueue.
   */
  void RunMessagesInQueue();
  /**
   * Update stream processing order and recompute stream blocking until
   * aEndBlockingDecisions.
   */
  void UpdateGraph(GraphTime aEndBlockingDecisions);

  void SwapMessageQueues()
  {
    MOZ_ASSERT(CurrentDriver()->OnThread());
    MOZ_ASSERT(mFrontMessageQueue.IsEmpty());
    mMonitor.AssertCurrentThreadOwns();
    mFrontMessageQueue.SwapElements(mBackMessageQueue);
  }
  /**
   * Do all the processing and play the audio and video, from
   * mProcessedTime to mStateComputedTime.
   */
  void Process();

  /**
   * For use during ProcessedMediaStream::ProcessInput() or
   * MediaStreamListener callbacks, when graph state cannot be changed.
   * Schedules |aMessage| to run after processing, at a time when graph state
   * can be changed.  Graph thread.
   */
  void RunMessageAfterProcessing(UniquePtr<ControlMessage> aMessage);

  /**
   * Called when a suspend/resume/close operation has been completed, on the
   * graph thread.
   */
  void AudioContextOperationCompleted(MediaStream* aStream,
                                      void* aPromise,
                                      dom::AudioContextOperation aOperation);

  /**
   * Apply and AudioContext operation (suspend/resume/closed), on the graph
   * thread.
   */
  void ApplyAudioContextOperationImpl(MediaStream* aDestinationStream,
                                      const nsTArray<MediaStream*>& aStreams,
                                      dom::AudioContextOperation aOperation,
                                      void* aPromise);

  /**
   * Increment suspend count on aStream and move it to mSuspendedStreams if
   * necessary.
   */
  void IncrementSuspendCount(MediaStream* aStream);
  /**
   * Increment suspend count on aStream and move it to mStreams if
   * necessary.
   */
  void DecrementSuspendCount(MediaStream* aStream);

  /*
   * Move streams from the mStreams to mSuspendedStream if suspending/closing an
   * AudioContext, or the inverse when resuming an AudioContext.
   */
  void SuspendOrResumeStreams(dom::AudioContextOperation aAudioContextOperation,
                              const nsTArray<MediaStream*>& aStreamSet);

  /**
   * Determine if we have any audio tracks, or are about to add any audiotracks.
   */
  bool AudioTrackPresent();

  /**
   * Sort mStreams so that every stream not in a cycle is after any streams
   * it depends on, and every stream in a cycle is marked as being in a cycle.
   * Also sets mIsConsumed on every stream.
   */
  void UpdateStreamOrder();

  /**
   * Returns smallest value of t such that t is a multiple of
   * WEBAUDIO_BLOCK_SIZE and t >= aTime.
   */
  GraphTime RoundUpToEndOfAudioBlock(GraphTime aTime);
  /**
   * Returns smallest value of t such that t is a multiple of
   * WEBAUDIO_BLOCK_SIZE and t > aTime.
   */
  GraphTime RoundUpToNextAudioBlock(GraphTime aTime);
  /**
   * Produce data for all streams >= aStreamIndex for the current time interval.
   * Advances block by block, each iteration producing data for all streams
   * for a single block.
   * This is called whenever we have an AudioNodeStream in the graph.
   */
  void ProduceDataForStreamsBlockByBlock(uint32_t aStreamIndex,
                                         TrackRate aSampleRate);
  /**
   * If aStream will underrun between aTime, and aEndBlockingDecisions, returns
   * the time at which the underrun will start. Otherwise return
   * aEndBlockingDecisions.
   */
  GraphTime WillUnderrun(MediaStream* aStream, GraphTime aEndBlockingDecisions);

  /**
   * Given a graph time aTime, convert it to a stream time taking into
   * account the time during which aStream is scheduled to be blocked.
   */
  StreamTime GraphTimeToStreamTimeWithBlocking(const MediaStream* aStream, GraphTime aTime) const;

  /**
   * Call NotifyHaveCurrentData on aStream's listeners.
   */
  void NotifyHasCurrentData(MediaStream* aStream);
  /**
   * If aStream needs an audio stream but doesn't have one, create it.
   * If aStream doesn't need an audio stream but has one, destroy it.
   */
  void CreateOrDestroyAudioStreams(MediaStream* aStream);
  /**
   * Queue audio (mix of stream audio and silence for blocked intervals)
   * to the audio output stream. Returns the number of frames played.
   */
  StreamTime PlayAudio(MediaStream* aStream);
  /**
   * No more data will be forthcoming for aStream. The stream will end
   * at the current buffer end point. The StreamTracks's tracks must be
   * explicitly set to finished by the caller.
   */
  void OpenAudioInputImpl(int aID,
                          AudioDataListener *aListener);
  virtual nsresult OpenAudioInput(int aID,
                                  AudioDataListener *aListener) override;
  void CloseAudioInputImpl(AudioDataListener *aListener);
  virtual void CloseAudioInput(AudioDataListener *aListener) override;

  /**
   * Compute how much stream data we would like to buffer for aStream.
   */
  StreamTime GetDesiredBufferEnd(MediaStream* aStream);
  /**
   * Returns true when there are no active streams.
   */
  bool IsEmpty() const
  {
    MOZ_ASSERT(OnGraphThreadOrNotRunning() ||
               (NS_IsMainThread() &&
                LifecycleStateRef() >= LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP));
    return mStreams.IsEmpty() && mSuspendedStreams.IsEmpty() && mPortCount == 0;
  }

  /**
   * Add aStream to the graph and initializes its graph-specific state.
   */
  void AddStreamGraphThread(MediaStream* aStream);
  /**
   * Remove aStream from the graph. Ensures that pending messages about the
   * stream back to the main thread are flushed.
   */
  void RemoveStreamGraphThread(MediaStream* aStream);
  /**
   * Remove aPort from the graph and release it.
   */
  void DestroyPort(MediaInputPort* aPort);
  /**
   * Mark the media stream order as dirty.
   */
  void SetStreamOrderDirty()
  {
    MOZ_ASSERT(OnGraphThreadOrNotRunning());
    mStreamOrderDirty = true;
  }

  uint32_t AudioChannelCount() const
  {
    return mOutputChannels;
  }

  double MediaTimeToSeconds(GraphTime aTime) const
  {
    NS_ASSERTION(aTime > -STREAM_TIME_MAX && aTime <= STREAM_TIME_MAX,
                 "Bad time");
    return static_cast<double>(aTime)/GraphRate();
  }

  GraphTime SecondsToMediaTime(double aS) const
  {
    NS_ASSERTION(0 <= aS && aS <= TRACK_TICKS_MAX/TRACK_RATE_MAX,
                 "Bad seconds");
    return GraphRate() * aS;
  }

  GraphTime MillisecondsToMediaTime(int32_t aMS) const
  {
    return RateConvertTicksRoundDown(GraphRate(), 1000, aMS);
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
  GraphDriver* CurrentDriver() const
  {
#ifdef DEBUG
    if (!OnGraphThreadOrNotRunning()) {
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
   * We can also switch from Revive() (on MainThread). Monitor must be held.
   */
  void SetCurrentDriver(GraphDriver* aDriver)
  {
    MOZ_ASSERT(mDriver->OnThread() || !mDriver->ThreadRunning());
#ifdef DEBUG
    mMonitor.AssertCurrentThreadOwns();
#endif
    mDriver = aDriver;
  }

  Monitor& GetMonitor()
  {
    return mMonitor;
  }

  void EnsureNextIteration()
  {
    mNeedAnotherIteration = true; // atomic
    // Note: GraphDriver must ensure that there's no race on setting
    // mNeedAnotherIteration and mGraphDriverAsleep -- see WaitForNextIteration()
    if (mGraphDriverAsleep) { // atomic
      MonitorAutoLock mon(mMonitor);
      CurrentDriver()->WakeUp(); // Might not be the same driver; might have woken already
    }
  }

  void EnsureNextIterationLocked()
  {
    mNeedAnotherIteration = true; // atomic
    if (mGraphDriverAsleep) { // atomic
      CurrentDriver()->WakeUp(); // Might not be the same driver; might have woken already
    }
  }

  // Capture Stream API. This allows to get a mixed-down output for a window.
  void RegisterCaptureStreamForWindow(uint64_t aWindowId,
                                      ProcessedMediaStream* aCaptureStream);
  void UnregisterCaptureStreamForWindow(uint64_t aWindowId);
  already_AddRefed<MediaInputPort>
  ConnectToCaptureStream(uint64_t aWindowId, MediaStream* aMediaStream);

  class StreamSet {
  public:
    class iterator {
    public:
      explicit iterator(MediaStreamGraphImpl& aGraph)
        : mGraph(&aGraph), mArrayNum(-1), mArrayIndex(0)
      {
        ++(*this);
      }
      iterator() : mGraph(nullptr), mArrayNum(2), mArrayIndex(0) {}
      MediaStream* operator*()
      {
        return Array()->ElementAt(mArrayIndex);
      }
      iterator operator++()
      {
        ++mArrayIndex;
        while (mArrayNum < 2 &&
          (mArrayNum < 0 || mArrayIndex >= Array()->Length())) {
          ++mArrayNum;
          mArrayIndex = 0;
        }
        return *this;
      }
      bool operator==(const iterator& aOther) const
      {
        return mArrayNum == aOther.mArrayNum && mArrayIndex == aOther.mArrayIndex;
      }
      bool operator!=(const iterator& aOther) const
      {
        return !(*this == aOther);
      }
    private:
      nsTArray<MediaStream*>* Array()
      {
        return mArrayNum == 0 ? &mGraph->mStreams : &mGraph->mSuspendedStreams;
      }
      MediaStreamGraphImpl* mGraph;
      int mArrayNum;
      uint32_t mArrayIndex;
    };

    explicit StreamSet(MediaStreamGraphImpl& aGraph) : mGraph(aGraph) {}
    iterator begin() { return iterator(mGraph); }
    iterator end() { return iterator(); }
  private:
    MediaStreamGraphImpl& mGraph;
  };
  StreamSet AllStreams() { return StreamSet(*this); }

  // Data members
  //
  /**
   * Graphs own owning references to their driver, until shutdown. When a driver
   * switch occur, previous driver is either deleted, or it's ownership is
   * passed to a event that will take care of the asynchronous cleanup, as
   * audio stream can take some time to shut down.
   * Accessed on both the main thread and the graph thread; both read and write.
   * Must hold monitor to access it.
   */
  RefPtr<GraphDriver> mDriver;

  // The following state is managed on the graph thread only, unless
  // mLifecycleState > LIFECYCLE_RUNNING in which case the graph thread
  // is not running and this state can be used from the main thread.

  /**
   * The graph keeps a reference to each stream.
   * References are maintained manually to simplify reordering without
   * unnecessary thread-safe refcount changes.
   * Must satisfy OnGraphThreadOrNotRunning().
   */
  nsTArray<MediaStream*> mStreams;
  /**
   * This stores MediaStreams that are part of suspended AudioContexts.
   * mStreams and mSuspendStream are disjoint sets: a stream is either suspended
   * or not suspended. Suspended streams are not ordered in UpdateStreamOrder,
   * and are therefore not doing any processing.
   * Must satisfy OnGraphThreadOrNotRunning().
   */
  nsTArray<MediaStream*> mSuspendedStreams;
  /**
   * Streams from mFirstCycleBreaker to the end of mStreams produce output
   * before they receive input.  They correspond to DelayNodes that are in
   * cycles.
   */
  uint32_t mFirstCycleBreaker;
  /**
   * Blocking decisions have been computed up to this time.
   * Between each iteration, this is the same as mProcessedTime.
   */
  GraphTime mStateComputedTime = 0;
  /**
   * All stream contents have been computed up to this time.
   * The next batch of updates from the main thread will be processed
   * at this time.  This is behind mStateComputedTime during processing.
   */
  GraphTime mProcessedTime = 0;
  /**
   * Date of the last time we updated the main thread with the graph state.
   */
  TimeStamp mLastMainThreadUpdate;
  /**
   * Number of active MediaInputPorts
   */
  int32_t mPortCount;

  /**
   * Devices to use for cubeb input & output, or NULL for no input (void*),
   * and boolean to control if we want input/output
   */
  bool mInputWanted;
  int mInputDeviceID;
  bool mOutputWanted;
  int mOutputDeviceID;
  // Maps AudioDataListeners to a usecount of streams using the listener
  // so we can know when it's no longer in use.
  nsDataHashtable<nsPtrHashKey<AudioDataListener>, uint32_t> mInputDeviceUsers;

  // True if the graph needs another iteration after the current iteration.
  Atomic<bool> mNeedAnotherIteration;
  // GraphDriver may need a WakeUp() if something changes
  Atomic<bool> mGraphDriverAsleep;

  // mMonitor guards the data below.
  // MediaStreamGraph normally does its work without holding mMonitor, so it is
  // not safe to just grab mMonitor from some thread and start monkeying with
  // the graph. Instead, communicate with the graph thread using provided
  // mechanisms such as the ControlMessage queue.
  Monitor mMonitor;

  // Data guarded by mMonitor (must always be accessed with mMonitor held,
  // regardless of the value of mLifecycleState).

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
  /*
   * Message queue processed by the MSG thread during an iteration.
   * Accessed on graph thread only.
   */
  nsTArray<MessageBlock> mFrontMessageQueue;
  /*
   * Message queue in which the main thread appends messages.
   * Access guarded by mMonitor.
   */
  nsTArray<MessageBlock> mBackMessageQueue;

  /* True if there will messages to process if we swap the message queues. */
  bool MessagesQueued() const
  {
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
  enum LifecycleState
  {
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

  /**
   * Modified only in mMonitor.  Transitions to
   * LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP occur on the graph thread at
   * the end of an iteration.  All other transitions occur on the main thread.
   */
  LifecycleState mLifecycleState;
  LifecycleState& LifecycleStateRef()
  {
#if DEBUG
    if (!mDetectedNotRunning) {
      mMonitor.AssertCurrentThreadOwns();
    }
#endif
    return mLifecycleState;
  }
  const LifecycleState& LifecycleStateRef() const
  {
#if DEBUG
    if (!mDetectedNotRunning) {
      mMonitor.AssertCurrentThreadOwns();
    }
#endif
    return mLifecycleState;
  }
  /**
   * The graph should stop processing at or after this time.
   * Only set on main thread. Read on both main and MSG thread.
   */
  Atomic<GraphTime> mEndTime;

  /**
   * True when we need to do a forced shutdown during application shutdown.
   * Only set on main thread.
   * Can be read safely on the main thread, on all other threads mMonitor must
   * be held.
   */
  bool mForceShutDown;

  /**
   * Drop this reference during shutdown to unblock shutdown.
   * Only accessed on the main thread.
   **/
  RefPtr<media::ShutdownTicket> mForceShutdownTicket;

  /**
   * True when we have posted an event to the main thread to run
   * RunInStableState() and the event hasn't run yet.
   * Accessed on both main and MSG thread, mMonitor must be held.
   */
  bool mPostedRunInStableStateEvent;

  // Main thread only

  /**
   * Messages posted by the current event loop task. These are forwarded to
   * the media graph thread during RunInStableState. We can't forward them
   * immediately because we want all messages between stable states to be
   * processed as an atomic batch.
   */
  nsTArray<UniquePtr<ControlMessage>> mCurrentTaskMessageQueue;
  /**
   * True when RunInStableState has determined that mLifecycleState is >
   * LIFECYCLE_RUNNING. Since only the main thread can reset mLifecycleState to
   * LIFECYCLE_RUNNING, this can be relied on to not change unexpectedly.
   */
  Atomic<bool> mDetectedNotRunning;
  /**
   * True when a stable state runner has been posted to the appshell to run
   * RunInStableState at the next stable state.
   * Only accessed on the main thread.
   */
  bool mPostedRunInStableState;
  /**
   * True when processing real-time audio/video.  False when processing non-realtime
   * audio.
   */
  const bool mRealtime;
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
  RefPtr<AsyncLatencyLogger> mLatencyLog;
  AudioMixer mMixer;
  const RefPtr<AbstractThread> mAbstractMainThread;

  // used to limit graph shutdown time
  // Only accessed on the main thread.
  nsCOMPtr<nsITimer> mShutdownTimer;

private:
  virtual ~MediaStreamGraphImpl();

  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

  /**
   * This class uses manual memory management, and all pointers to it are raw
   * pointers. However, in order for it to implement nsIMemoryReporter, it needs
   * to implement nsISupports and so be ref-counted. So it maintains a single
   * nsRefPtr to itself, giving it a ref-count of 1 during its entire lifetime,
   * and Destroy() nulls this self-reference in order to trigger self-deletion.
   */
  RefPtr<MediaStreamGraphImpl> mSelfRef;

  struct WindowAndStream
  {
    uint64_t mWindowId;
    RefPtr<ProcessedMediaStream> mCaptureStreamSink;
  };
  /**
   * Stream for window audio capture.
   */
  nsTArray<WindowAndStream> mWindowCaptureStreams;

  /**
   * Number of channels on output.
   */
  const uint32_t mOutputChannels;

#ifdef DEBUG
  /**
   * Used to assert when AppendMessage() runs ControlMessages synchronously.
   */
  bool mCanRunMessagesSynchronously;
#endif
};

} // namespace mozilla

#endif /* MEDIASTREAMGRAPHIMPL_H_ */
