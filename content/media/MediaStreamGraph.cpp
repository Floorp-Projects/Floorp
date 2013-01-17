/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamGraph.h"

#include "mozilla/Monitor.h"
#include "mozilla/TimeStamp.h"
#include "AudioSegment.h"
#include "VideoSegment.h"
#include "nsContentUtils.h"
#include "nsIAppShell.h"
#include "nsIObserver.h"
#include "nsServiceManagerUtils.h"
#include "nsWidgetsCID.h"
#include "nsXPCOMCIDInternal.h"
#include "prlog.h"
#include "VideoUtils.h"
#include "mozilla/Attributes.h"
#include "TrackUnionStream.h"
#include "ImageContainer.h"
#include "AudioChannelCommon.h"
#include <algorithm>

using namespace mozilla::layers;
using namespace mozilla::dom;

namespace mozilla {

#ifdef PR_LOGGING
PRLogModuleInfo* gMediaStreamGraphLog;
#define LOG(type, msg) PR_LOG(gMediaStreamGraphLog, type, msg)
#else
#define LOG(type, msg)
#endif

namespace {

/**
 * Assume we can run an iteration of the MediaStreamGraph loop in this much time
 * or less.
 * We try to run the control loop at this rate.
 */
const int MEDIA_GRAPH_TARGET_PERIOD_MS = 10;

/**
 * Assume that we might miss our scheduled wakeup of the MediaStreamGraph by
 * this much.
 */
const int SCHEDULE_SAFETY_MARGIN_MS = 10;

/**
 * Try have this much audio buffered in streams and queued to the hardware.
 * The maximum delay to the end of the next control loop
 * is 2*MEDIA_GRAPH_TARGET_PERIOD_MS + SCHEDULE_SAFETY_MARGIN_MS.
 * There is no point in buffering more audio than this in a stream at any
 * given time (until we add processing).
 * This is not optimal yet.
 */
const int AUDIO_TARGET_MS = 2*MEDIA_GRAPH_TARGET_PERIOD_MS +
    SCHEDULE_SAFETY_MARGIN_MS;

/**
 * Try have this much video buffered. Video frames are set
 * near the end of the iteration of the control loop. The maximum delay
 * to the setting of the next video frame is 2*MEDIA_GRAPH_TARGET_PERIOD_MS +
 * SCHEDULE_SAFETY_MARGIN_MS. This is not optimal yet.
 */
const int VIDEO_TARGET_MS = 2*MEDIA_GRAPH_TARGET_PERIOD_MS +
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
 * A ControlMessage always references a particular affected stream.
 */
class ControlMessage {
public:
  ControlMessage(MediaStream* aStream) : mStream(aStream)
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

}

/**
 * The implementation of a media stream graph. This class is private to this
 * file. It's not in the anonymous namespace because MediaStream needs to
 * be able to friend it.
 *
 * Currently we only have one per process.
 */
class MediaStreamGraphImpl : public MediaStreamGraph {
public:
  MediaStreamGraphImpl();
  ~MediaStreamGraphImpl()
  {
    NS_ASSERTION(IsEmpty(),
                 "All streams should have been destroyed by messages from the main thread");
    LOG(PR_LOG_DEBUG, ("MediaStreamGraph %p destroyed", this));
  }

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
  void PrepareUpdatesToMainThreadState();
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
  void UpdateStreamOrderForStream(nsTArray<MediaStream*>* aStack,
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
   * to the audio output stream.
   */
  void PlayAudio(MediaStream* aStream, GraphTime aFrom, GraphTime aTo);
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

  // For use by control messages
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
   * 2) Forced shutdown at application shutdown. A flag is set, RunThread()
   * detects the flag and exits, the next RunInStableState() detects the flag,
   * and dispatches the async event to Shutdown() the graph's threads. However
   * the graph object is not deleted. New messages for the graph are processed
   * synchronously on the main thread if necessary. When the last stream is
   * destroyed, the graph object is deleted.
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
    // to be destroyed. Only happens during application shutdown since normally
    // we'd only shut down a graph when it has no streams.
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
};

/**
 * The singleton graph instance.
 */
static MediaStreamGraphImpl* gGraph;

StreamTime
MediaStreamGraphImpl::GetDesiredBufferEnd(MediaStream* aStream)
{
  StreamTime current = mCurrentTime - aStream->mBufferStartTime;
  return current +
      MillisecondsToMediaTime(std::max(AUDIO_TARGET_MS, VIDEO_TARGET_MS));
}

void
MediaStreamGraphImpl::FinishStream(MediaStream* aStream)
{
  if (aStream->mFinished)
    return;
  LOG(PR_LOG_DEBUG, ("MediaStream %p will finish", aStream));
  aStream->mFinished = true;
  // Force at least one more iteration of the control loop, since we rely
  // on UpdateCurrentTime to notify our listeners once the stream end
  // has been reached.
  EnsureNextIteration();
}

void
MediaStreamGraphImpl::AddStream(MediaStream* aStream)
{
  aStream->mBufferStartTime = mCurrentTime;
  *mStreams.AppendElement() = already_AddRefed<MediaStream>(aStream);
  LOG(PR_LOG_DEBUG, ("Adding media stream %p to the graph", aStream));
}

void
MediaStreamGraphImpl::RemoveStream(MediaStream* aStream)
{
  // Remove references in mStreamUpdates before we allow aStream to die.
  // Pending updates are not needed (since the main thread has already given
  // up the stream) so we will just drop them.
  {
    MonitorAutoLock lock(mMonitor);
    for (uint32_t i = 0; i < mStreamUpdates.Length(); ++i) {
      if (mStreamUpdates[i].mStream == aStream) {
        mStreamUpdates[i].mStream = nullptr;
      }
    }
  }

  // This unrefs the stream, probably destroying it
  mStreams.RemoveElement(aStream);

  LOG(PR_LOG_DEBUG, ("Removing media stream %p from the graph", aStream));
}

void
MediaStreamGraphImpl::UpdateConsumptionState(SourceMediaStream* aStream)
{
  MediaStreamListener::Consumption state =
      aStream->mIsConsumed ? MediaStreamListener::CONSUMED
      : MediaStreamListener::NOT_CONSUMED;
  if (state != aStream->mLastConsumptionState) {
    aStream->mLastConsumptionState = state;
    for (uint32_t j = 0; j < aStream->mListeners.Length(); ++j) {
      MediaStreamListener* l = aStream->mListeners[j];
      l->NotifyConsumptionChanged(this, state);
    }
  }
}

void
MediaStreamGraphImpl::ExtractPendingInput(SourceMediaStream* aStream,
                                          GraphTime aDesiredUpToTime,
                                          bool* aEnsureNextIteration)
{
  bool finished;
  {
    MutexAutoLock lock(aStream->mMutex);
    if (aStream->mPullEnabled && !aStream->mFinished &&
        !aStream->mListeners.IsEmpty()) {
      // Compute how much stream time we'll need assuming we don't block
      // the stream at all between mBlockingDecisionsMadeUntilTime and
      // aDesiredUpToTime.
      StreamTime t =
        GraphTimeToStreamTime(aStream, mStateComputedTime) +
        (aDesiredUpToTime - mStateComputedTime);
      if (t > aStream->mBuffer.GetEnd()) {
        *aEnsureNextIteration = true;
        for (uint32_t j = 0; j < aStream->mListeners.Length(); ++j) {
          MediaStreamListener* l = aStream->mListeners[j];
          {
            MutexAutoUnlock unlock(aStream->mMutex);
            l->NotifyPull(this, t);
          }
        }
      }
    }
    finished = aStream->mUpdateFinished;
    for (int32_t i = aStream->mUpdateTracks.Length() - 1; i >= 0; --i) {
      SourceMediaStream::TrackData* data = &aStream->mUpdateTracks[i];
      for (uint32_t j = 0; j < aStream->mListeners.Length(); ++j) {
        MediaStreamListener* l = aStream->mListeners[j];
        TrackTicks offset = (data->mCommands & SourceMediaStream::TRACK_CREATE)
            ? data->mStart : aStream->mBuffer.FindTrack(data->mID)->GetSegment()->GetDuration();
        l->NotifyQueuedTrackChanges(this, data->mID, data->mRate,
                                    offset, data->mCommands, *data->mData);
      }
      if (data->mCommands & SourceMediaStream::TRACK_CREATE) {
        MediaSegment* segment = data->mData.forget();
        LOG(PR_LOG_DEBUG, ("SourceMediaStream %p creating track %d, rate %d, start %lld, initial end %lld",
                           aStream, data->mID, data->mRate, int64_t(data->mStart),
                           int64_t(segment->GetDuration())));
        aStream->mBuffer.AddTrack(data->mID, data->mRate, data->mStart, segment);
        // The track has taken ownership of data->mData, so let's replace
        // data->mData with an empty clone.
        data->mData = segment->CreateEmptyClone();
        data->mCommands &= ~SourceMediaStream::TRACK_CREATE;
      } else if (data->mData->GetDuration() > 0) {
        MediaSegment* dest = aStream->mBuffer.FindTrack(data->mID)->GetSegment();
        LOG(PR_LOG_DEBUG, ("SourceMediaStream %p track %d, advancing end from %lld to %lld",
                           aStream, data->mID,
                           int64_t(dest->GetDuration()),
                           int64_t(dest->GetDuration() + data->mData->GetDuration())));
        dest->AppendFrom(data->mData);
      }
      if (data->mCommands & SourceMediaStream::TRACK_END) {
        aStream->mBuffer.FindTrack(data->mID)->SetEnded();
        aStream->mUpdateTracks.RemoveElementAt(i);
      }
    }
    aStream->mBuffer.AdvanceKnownTracksTime(aStream->mUpdateKnownTracksTime);
  }
  if (finished) {
    FinishStream(aStream);
  }
}

void
MediaStreamGraphImpl::UpdateBufferSufficiencyState(SourceMediaStream* aStream)
{
  StreamTime desiredEnd = GetDesiredBufferEnd(aStream);
  nsTArray<SourceMediaStream::ThreadAndRunnable> runnables;

  {
    MutexAutoLock lock(aStream->mMutex);
    for (uint32_t i = 0; i < aStream->mUpdateTracks.Length(); ++i) {
      SourceMediaStream::TrackData* data = &aStream->mUpdateTracks[i];
      if (data->mCommands & SourceMediaStream::TRACK_CREATE) {
        // This track hasn't been created yet, so we have no sufficiency
        // data. The track will be created in the next iteration of the
        // control loop and then we'll fire insufficiency notifications
        // if necessary.
        continue;
      }
      if (data->mCommands & SourceMediaStream::TRACK_END) {
        // This track will end, so no point in firing not-enough-data
        // callbacks.
        continue;
      }
      StreamBuffer::Track* track = aStream->mBuffer.FindTrack(data->mID);
      // Note that track->IsEnded() must be false, otherwise we would have
      // removed the track from mUpdateTracks already.
      NS_ASSERTION(!track->IsEnded(), "What is this track doing here?");
      data->mHaveEnough = track->GetEndTimeRoundDown() >= desiredEnd;
      if (!data->mHaveEnough) {
        runnables.MoveElementsFrom(data->mDispatchWhenNotEnough);
      }
    }
  }

  for (uint32_t i = 0; i < runnables.Length(); ++i) {
    runnables[i].mThread->Dispatch(runnables[i].mRunnable, 0);
  }
}

StreamTime
MediaStreamGraphImpl::GraphTimeToStreamTime(MediaStream* aStream,
                                            GraphTime aTime)
{
  NS_ASSERTION(aTime <= mStateComputedTime,
               "Don't ask about times where we haven't made blocking decisions yet");
  if (aTime <= mCurrentTime) {
    return std::max<StreamTime>(0, aTime - aStream->mBufferStartTime);
  }
  GraphTime t = mCurrentTime;
  StreamTime s = t - aStream->mBufferStartTime;
  while (t < aTime) {
    GraphTime end;
    if (!aStream->mBlocked.GetAt(t, &end)) {
      s += std::min(aTime, end) - t;
    }
    t = end;
  }
  return std::max<StreamTime>(0, s);
}  

GraphTime
MediaStreamGraphImpl::StreamTimeToGraphTime(MediaStream* aStream,
                                            StreamTime aTime, uint32_t aFlags)
{
  if (aTime >= STREAM_TIME_MAX) {
    return GRAPH_TIME_MAX;
  }
  MediaTime bufferElapsedToCurrentTime = mCurrentTime - aStream->mBufferStartTime;
  if (aTime < bufferElapsedToCurrentTime ||
      (aTime == bufferElapsedToCurrentTime && !(aFlags & INCLUDE_TRAILING_BLOCKED_INTERVAL))) {
    return aTime + aStream->mBufferStartTime;
  }

  MediaTime streamAmount = aTime - bufferElapsedToCurrentTime;
  NS_ASSERTION(streamAmount >= 0, "Can't answer queries before current time");

  GraphTime t = mCurrentTime;
  while (t < GRAPH_TIME_MAX) {
    bool blocked;
    GraphTime end;
    if (t < mStateComputedTime) {
      blocked = aStream->mBlocked.GetAt(t, &end);
      end = std::min(end, mStateComputedTime);
    } else {
      blocked = false;
      end = GRAPH_TIME_MAX;
    }
    if (blocked) {
      t = end;
    } else {
      if (streamAmount == 0) {
        // No more stream time to consume at time t, so we're done.
        break;
      }
      MediaTime consume = std::min(end - t, streamAmount);
      streamAmount -= consume;
      t += consume;
    }
  }
  return t;
}

GraphTime
MediaStreamGraphImpl::GetAudioPosition(MediaStream* aStream)
{
  if (aStream->mAudioOutputStreams.IsEmpty()) {
    return mCurrentTime;
  }
  int64_t positionInFrames = aStream->mAudioOutputStreams[0].mStream->GetPositionInFrames();
  if (positionInFrames < 0) {
    return mCurrentTime;
  }
  return aStream->mAudioOutputStreams[0].mAudioPlaybackStartTime +
      TicksToTimeRoundDown(aStream->mAudioOutputStreams[0].mStream->GetRate(),
                           positionInFrames);
}

void
MediaStreamGraphImpl::UpdateCurrentTime()
{
  GraphTime prevCurrentTime = mCurrentTime;
  TimeStamp now = TimeStamp::Now();
  GraphTime nextCurrentTime =
    SecondsToMediaTime((now - mCurrentTimeStamp).ToSeconds()) + mCurrentTime;
  if (mStateComputedTime < nextCurrentTime) {
    LOG(PR_LOG_WARNING, ("Media graph global underrun detected"));
    LOG(PR_LOG_DEBUG, ("Advancing mStateComputedTime from %f to %f",
                       MediaTimeToSeconds(mStateComputedTime),
                       MediaTimeToSeconds(nextCurrentTime)));
    // Advance mStateComputedTime to nextCurrentTime by
    // adding blocked time to all streams starting at mStateComputedTime
    for (uint32_t i = 0; i < mStreams.Length(); ++i) {
      mStreams[i]->mBlocked.SetAtAndAfter(mStateComputedTime, true);
    }
    mStateComputedTime = nextCurrentTime;
  }
  mCurrentTimeStamp = now;

  LOG(PR_LOG_DEBUG, ("Updating current time to %f (real %f, mStateComputedTime %f)",
                     MediaTimeToSeconds(nextCurrentTime),
                     (now - mInitialTimeStamp).ToSeconds(),
                     MediaTimeToSeconds(mStateComputedTime)));

  if (prevCurrentTime >= nextCurrentTime) {
    NS_ASSERTION(prevCurrentTime == nextCurrentTime, "Time can't go backwards!");
    // This could happen due to low clock resolution, maybe?
    LOG(PR_LOG_DEBUG, ("Time did not advance"));
    // There's not much left to do here, but the code below that notifies
    // listeners that streams have ended still needs to run.
  }

  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaStream* stream = mStreams[i];

    // Calculate blocked time and fire Blocked/Unblocked events
    GraphTime blockedTime = 0;
    GraphTime t = prevCurrentTime;
    while (t < nextCurrentTime) {
      GraphTime end;
      bool blocked = stream->mBlocked.GetAt(t, &end);
      if (blocked) {
        blockedTime += std::min(end, nextCurrentTime) - t;
      }
      if (blocked != stream->mNotifiedBlocked) {
        for (uint32_t j = 0; j < stream->mListeners.Length(); ++j) {
          MediaStreamListener* l = stream->mListeners[j];
          l->NotifyBlockingChanged(this,
              blocked ? MediaStreamListener::BLOCKED : MediaStreamListener::UNBLOCKED);
        }
        stream->mNotifiedBlocked = blocked;
      }
      t = end;
    }

    stream->AdvanceTimeVaryingValuesToCurrentTime(nextCurrentTime, blockedTime);
    // Advance mBlocked last so that implementations of
    // AdvanceTimeVaryingValuesToCurrentTime can rely on the value of mBlocked.
    stream->mBlocked.AdvanceCurrentTime(nextCurrentTime);

    if (blockedTime < nextCurrentTime - prevCurrentTime) {
      for (uint32_t i = 0; i < stream->mListeners.Length(); ++i) {
        MediaStreamListener* l = stream->mListeners[i];
        l->NotifyOutput(this);
      }
    }

    if (stream->mFinished && !stream->mNotifiedFinished &&
        stream->mBufferStartTime + stream->GetBufferEnd() <= nextCurrentTime) {
      stream->mNotifiedFinished = true;
      stream->mLastPlayedVideoFrame.SetNull();
      for (uint32_t j = 0; j < stream->mListeners.Length(); ++j) {
        MediaStreamListener* l = stream->mListeners[j];
        l->NotifyFinished(this);
      }
    }

    LOG(PR_LOG_DEBUG, ("MediaStream %p bufferStartTime=%f blockedTime=%f",
                       stream, MediaTimeToSeconds(stream->mBufferStartTime),
                       MediaTimeToSeconds(blockedTime)));
  }

  mCurrentTime = nextCurrentTime;
}

bool
MediaStreamGraphImpl::WillUnderrun(MediaStream* aStream, GraphTime aTime,
                                   GraphTime aEndBlockingDecisions, GraphTime* aEnd)
{
  // Finished streams can't underrun. ProcessedMediaStreams also can't cause
  // underrun currently, since we'll always be able to produce data for them
  // unless they block on some other stream.
  if (aStream->mFinished || aStream->AsProcessedStream()) {
    return false;
  }
  GraphTime bufferEnd =
    StreamTimeToGraphTime(aStream, aStream->GetBufferEnd(),
                          INCLUDE_TRAILING_BLOCKED_INTERVAL);
  NS_ASSERTION(bufferEnd >= mCurrentTime, "Buffer underran");
  // We should block after bufferEnd.
  if (bufferEnd <= aTime) {
    LOG(PR_LOG_DEBUG, ("MediaStream %p will block due to data underrun, "
                       "bufferEnd %f",
                       aStream, MediaTimeToSeconds(bufferEnd)));
    return true;
  }
  // We should keep blocking if we're currently blocked and we don't have
  // data all the way through to aEndBlockingDecisions. If we don't have
  // data all the way through to aEndBlockingDecisions, we'll block soon,
  // but we might as well remain unblocked and play the data we've got while
  // we can.
  if (bufferEnd <= aEndBlockingDecisions && aStream->mBlocked.GetBefore(aTime)) {
    LOG(PR_LOG_DEBUG, ("MediaStream %p will block due to speculative data underrun, "
                       "bufferEnd %f",
                       aStream, MediaTimeToSeconds(bufferEnd)));
    return true;
  }
  // Reconsider decisions at bufferEnd
  *aEnd = std::min(*aEnd, bufferEnd);
  return false;
}

void
MediaStreamGraphImpl::MarkConsumed(MediaStream* aStream)
{
  if (aStream->mIsConsumed) {
    return;
  }
  aStream->mIsConsumed = true;

  ProcessedMediaStream* ps = aStream->AsProcessedStream();
  if (!ps) {
    return;
  }
  // Mark all the inputs to this stream as consumed
  for (uint32_t i = 0; i < ps->mInputs.Length(); ++i) {
    MarkConsumed(ps->mInputs[i]->mSource);
  }
}

void
MediaStreamGraphImpl::UpdateStreamOrderForStream(nsTArray<MediaStream*>* aStack,
                                                 already_AddRefed<MediaStream> aStream)
{
  nsRefPtr<MediaStream> stream = aStream;
  NS_ASSERTION(!stream->mHasBeenOrdered, "stream should not have already been ordered");
  if (stream->mIsOnOrderingStack) {
    for (int32_t i = aStack->Length() - 1; ; --i) {
      aStack->ElementAt(i)->AsProcessedStream()->mInCycle = true;
      if (aStack->ElementAt(i) == stream)
        break;
    }
    return;
  }
  ProcessedMediaStream* ps = stream->AsProcessedStream();
  if (ps) {
    aStack->AppendElement(stream);
    stream->mIsOnOrderingStack = true;
    for (uint32_t i = 0; i < ps->mInputs.Length(); ++i) {
      MediaStream* source = ps->mInputs[i]->mSource;
      if (!source->mHasBeenOrdered) {
        nsRefPtr<MediaStream> s = source;
        UpdateStreamOrderForStream(aStack, s.forget());
      }
    }
    aStack->RemoveElementAt(aStack->Length() - 1);
    stream->mIsOnOrderingStack = false;
  }

  stream->mHasBeenOrdered = true;
  *mStreams.AppendElement() = stream.forget();
}

void
MediaStreamGraphImpl::UpdateStreamOrder()
{
  nsTArray<nsRefPtr<MediaStream> > oldStreams;
  oldStreams.SwapElements(mStreams);
  for (uint32_t i = 0; i < oldStreams.Length(); ++i) {
    MediaStream* stream = oldStreams[i];
    stream->mHasBeenOrdered = false;
    stream->mIsConsumed = false;
    stream->mIsOnOrderingStack = false;
    stream->mInBlockingSet = false;
    ProcessedMediaStream* ps = stream->AsProcessedStream();
    if (ps) {
      ps->mInCycle = false;
    }
  }

  nsAutoTArray<MediaStream*,10> stack;
  for (uint32_t i = 0; i < oldStreams.Length(); ++i) {
    nsRefPtr<MediaStream>& s = oldStreams[i];
    if (!s->mAudioOutputs.IsEmpty() || !s->mVideoOutputs.IsEmpty()) {
      MarkConsumed(s);
    }
    if (!s->mHasBeenOrdered) {
      UpdateStreamOrderForStream(&stack, s.forget());
    }
  }
}

void
MediaStreamGraphImpl::RecomputeBlocking(GraphTime aEndBlockingDecisions)
{
  bool blockingDecisionsWillChange = false;

  LOG(PR_LOG_DEBUG, ("Media graph %p computing blocking for time %f",
                     this, MediaTimeToSeconds(mStateComputedTime)));
  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaStream* stream = mStreams[i];
    if (!stream->mInBlockingSet) {
      // Compute a partition of the streams containing 'stream' such that we can
      // compute the blocking status of each subset independently.
      nsAutoTArray<MediaStream*,10> streamSet;
      AddBlockingRelatedStreamsToSet(&streamSet, stream);

      GraphTime end;
      for (GraphTime t = mStateComputedTime;
           t < aEndBlockingDecisions; t = end) {
        end = GRAPH_TIME_MAX;
        RecomputeBlockingAt(streamSet, t, aEndBlockingDecisions, &end);
        if (end < GRAPH_TIME_MAX) {
          blockingDecisionsWillChange = true;
        }
      }
    }

    GraphTime end;
    stream->mBlocked.GetAt(mCurrentTime, &end);
    if (end < GRAPH_TIME_MAX) {
      blockingDecisionsWillChange = true;
    }
  }
  LOG(PR_LOG_DEBUG, ("Media graph %p computed blocking for interval %f to %f",
                     this, MediaTimeToSeconds(mStateComputedTime),
                     MediaTimeToSeconds(aEndBlockingDecisions)));
  mStateComputedTime = aEndBlockingDecisions;
 
  if (blockingDecisionsWillChange) {
    // Make sure we wake up to notify listeners about these changes.
    EnsureNextIteration();
  }
}

void
MediaStreamGraphImpl::AddBlockingRelatedStreamsToSet(nsTArray<MediaStream*>* aStreams,
                                                     MediaStream* aStream)
{
  if (aStream->mInBlockingSet)
    return;
  aStream->mInBlockingSet = true;
  aStreams->AppendElement(aStream);
  for (uint32_t i = 0; i < aStream->mConsumers.Length(); ++i) {
    MediaInputPort* port = aStream->mConsumers[i];
    if (port->mFlags & (MediaInputPort::FLAG_BLOCK_INPUT | MediaInputPort::FLAG_BLOCK_OUTPUT)) {
      AddBlockingRelatedStreamsToSet(aStreams, port->mDest);
    }
  }
  ProcessedMediaStream* ps = aStream->AsProcessedStream();
  if (ps) {
    for (uint32_t i = 0; i < ps->mInputs.Length(); ++i) {
      MediaInputPort* port = ps->mInputs[i];
      if (port->mFlags & (MediaInputPort::FLAG_BLOCK_INPUT | MediaInputPort::FLAG_BLOCK_OUTPUT)) {
        AddBlockingRelatedStreamsToSet(aStreams, port->mSource);
      }
    }
  }
}

void
MediaStreamGraphImpl::MarkStreamBlocking(MediaStream* aStream)
{
  if (aStream->mBlockInThisPhase)
    return;
  aStream->mBlockInThisPhase = true;
  for (uint32_t i = 0; i < aStream->mConsumers.Length(); ++i) {
    MediaInputPort* port = aStream->mConsumers[i];
    if (port->mFlags & MediaInputPort::FLAG_BLOCK_OUTPUT) {
      MarkStreamBlocking(port->mDest);
    }
  }
  ProcessedMediaStream* ps = aStream->AsProcessedStream();
  if (ps) {
    for (uint32_t i = 0; i < ps->mInputs.Length(); ++i) {
      MediaInputPort* port = ps->mInputs[i];
      if (port->mFlags & MediaInputPort::FLAG_BLOCK_INPUT) {
        MarkStreamBlocking(port->mSource);
      }
    }
  }
}

void
MediaStreamGraphImpl::RecomputeBlockingAt(const nsTArray<MediaStream*>& aStreams,
                                          GraphTime aTime,
                                          GraphTime aEndBlockingDecisions,
                                          GraphTime* aEnd)
{
  for (uint32_t i = 0; i < aStreams.Length(); ++i) {
    MediaStream* stream = aStreams[i];
    stream->mBlockInThisPhase = false;
  }

  for (uint32_t i = 0; i < aStreams.Length(); ++i) {
    MediaStream* stream = aStreams[i];

    if (stream->mFinished) {
      GraphTime endTime = StreamTimeToGraphTime(stream, stream->GetBufferEnd());
      if (endTime <= aTime) {
        LOG(PR_LOG_DEBUG, ("MediaStream %p is blocked due to being finished", stream));
        // We'll block indefinitely
        MarkStreamBlocking(stream);
        *aEnd = aEndBlockingDecisions;
        continue;
      } else {
        LOG(PR_LOG_DEBUG, ("MediaStream %p is finished, but not blocked yet (end at %f, with blocking at %f)",
                           stream, MediaTimeToSeconds(stream->GetBufferEnd()),
                           MediaTimeToSeconds(endTime)));
        *aEnd = std::min(*aEnd, endTime);
      }
    }

    GraphTime end;
    bool explicitBlock = stream->mExplicitBlockerCount.GetAt(aTime, &end) > 0;
    *aEnd = std::min(*aEnd, end);
    if (explicitBlock) {
      LOG(PR_LOG_DEBUG, ("MediaStream %p is blocked due to explicit blocker", stream));
      MarkStreamBlocking(stream);
      continue;
    }

    bool underrun = WillUnderrun(stream, aTime, aEndBlockingDecisions, aEnd);
    if (underrun) {
      // We'll block indefinitely
      MarkStreamBlocking(stream);
      *aEnd = aEndBlockingDecisions;
      continue;
    }
  }
  NS_ASSERTION(*aEnd > aTime, "Failed to advance!");

  for (uint32_t i = 0; i < aStreams.Length(); ++i) {
    MediaStream* stream = aStreams[i];
    stream->mBlocked.SetAtAndAfter(aTime, stream->mBlockInThisPhase);
  }
}

void
MediaStreamGraphImpl::NotifyHasCurrentData(MediaStream* aStream)
{
  for (uint32_t j = 0; j < aStream->mListeners.Length(); ++j) {
    MediaStreamListener* l = aStream->mListeners[j];
    l->NotifyHasCurrentData(this,
      GraphTimeToStreamTime(aStream, mCurrentTime) < aStream->mBuffer.GetEnd());
  }
}

void
MediaStreamGraphImpl::CreateOrDestroyAudioStreams(GraphTime aAudioOutputStartTime,
                                                  MediaStream* aStream)
{
  nsAutoTArray<bool,2> audioOutputStreamsFound;
  for (uint32_t i = 0; i < aStream->mAudioOutputStreams.Length(); ++i) {
    audioOutputStreamsFound.AppendElement(false);
  }

  if (!aStream->mAudioOutputs.IsEmpty()) {
    for (StreamBuffer::TrackIter tracks(aStream->GetStreamBuffer(), MediaSegment::AUDIO);
         !tracks.IsEnded(); tracks.Next()) {
      uint32_t i;
      for (i = 0; i < audioOutputStreamsFound.Length(); ++i) {
        if (aStream->mAudioOutputStreams[i].mTrackID == tracks->GetID()) {
          break;
        }
      }
      if (i < audioOutputStreamsFound.Length()) {
        audioOutputStreamsFound[i] = true;
      } else {
        // No output stream created for this track yet. Check if it's time to
        // create one.
        GraphTime startTime =
          StreamTimeToGraphTime(aStream, tracks->GetStartTimeRoundDown(),
                                INCLUDE_TRAILING_BLOCKED_INTERVAL);
        if (startTime >= mStateComputedTime) {
          // The stream wants to play audio, but nothing will play for the forseeable
          // future, so don't create the stream.
          continue;
        }

        // XXX allocating a AudioStream could be slow so we're going to have to do
        // something here ... preallocation, async allocation, multiplexing onto a single
        // stream ...
        AudioSegment* audio = tracks->Get<AudioSegment>();
        MediaStream::AudioOutputStream* audioOutputStream =
          aStream->mAudioOutputStreams.AppendElement();
        audioOutputStream->mAudioPlaybackStartTime = aAudioOutputStartTime;
        audioOutputStream->mBlockedAudioTime = 0;
        audioOutputStream->mStream = AudioStream::AllocateStream();
        audioOutputStream->mStream->Init(audio->GetChannels(),
                                         tracks->GetRate(), AUDIO_CHANNEL_NORMAL);
        audioOutputStream->mTrackID = tracks->GetID();
      }
    }
  }

  for (int32_t i = audioOutputStreamsFound.Length() - 1; i >= 0; --i) {
    if (!audioOutputStreamsFound[i]) {
      aStream->mAudioOutputStreams[i].mStream->Shutdown();
      aStream->mAudioOutputStreams.RemoveElementAt(i);
    }
  }
}

void
MediaStreamGraphImpl::PlayAudio(MediaStream* aStream,
                                GraphTime aFrom, GraphTime aTo)
{
  if (aStream->mAudioOutputStreams.IsEmpty()) {
    return;
  }

  // When we're playing multiple copies of this stream at the same time, they're
  // perfectly correlated so adding volumes is the right thing to do.
  float volume = 0.0f;
  for (uint32_t i = 0; i < aStream->mAudioOutputs.Length(); ++i) {
    volume += aStream->mAudioOutputs[i].mVolume;
  }

  for (uint32_t i = 0; i < aStream->mAudioOutputStreams.Length(); ++i) {
    MediaStream::AudioOutputStream& audioOutput = aStream->mAudioOutputStreams[i];
    StreamBuffer::Track* track = aStream->mBuffer.FindTrack(audioOutput.mTrackID);
    AudioSegment* audio = track->Get<AudioSegment>();

    // We don't update aStream->mBufferStartTime here to account for
    // time spent blocked. Instead, we'll update it in UpdateCurrentTime after the
    // blocked period has completed. But we do need to make sure we play from the
    // right offsets in the stream buffer, even if we've already written silence for
    // some amount of blocked time after the current time.
    GraphTime t = aFrom;
    while (t < aTo) {
      GraphTime end;
      bool blocked = aStream->mBlocked.GetAt(t, &end);
      end = std::min(end, aTo);

      AudioSegment output;
      output.InitFrom(*audio);
      if (blocked) {
        // Track total blocked time in aStream->mBlockedAudioTime so that
        // the amount of silent samples we've inserted for blocking never gets
        // more than one sample away from the ideal amount.
        TrackTicks startTicks =
            TimeToTicksRoundDown(track->GetRate(), audioOutput.mBlockedAudioTime);
        audioOutput.mBlockedAudioTime += end - t;
        TrackTicks endTicks =
            TimeToTicksRoundDown(track->GetRate(), audioOutput.mBlockedAudioTime);

        output.InsertNullDataAtStart(endTicks - startTicks);
        LOG(PR_LOG_DEBUG, ("MediaStream %p writing blocking-silence samples for %f to %f",
                         aStream, MediaTimeToSeconds(t), MediaTimeToSeconds(end)));
      } else {
        TrackTicks startTicks =
            track->TimeToTicksRoundDown(GraphTimeToStreamTime(aStream, t));
        TrackTicks endTicks =
            track->TimeToTicksRoundDown(GraphTimeToStreamTime(aStream, end));

        // If startTicks is before the track start, then that part of 'audio'
        // will just be silence, which is fine here. But if endTicks is after
        // the track end, then 'audio' won't be long enough, so we'll need
        // to explicitly play silence.
        TrackTicks sliceEnd = std::min(endTicks, audio->GetDuration());
        if (sliceEnd > startTicks) {
          output.AppendSlice(*audio, startTicks, sliceEnd);
        }
        // Play silence where the track has ended
        output.AppendNullData(endTicks - sliceEnd);
        NS_ASSERTION(endTicks == sliceEnd || track->IsEnded(),
                     "Ran out of data but track not ended?");
        output.ApplyVolume(volume);
        LOG(PR_LOG_DEBUG, ("MediaStream %p writing samples for %f to %f (samples %lld to %lld)",
                           aStream, MediaTimeToSeconds(t), MediaTimeToSeconds(end),
                           startTicks, endTicks));
      }
      output.WriteTo(audioOutput.mStream);
      t = end;
    }
  }
}

void
MediaStreamGraphImpl::PlayVideo(MediaStream* aStream)
{
  if (aStream->mVideoOutputs.IsEmpty())
    return;

  // Display the next frame a bit early. This is better than letting the current
  // frame be displayed for too long.
  GraphTime framePosition = mCurrentTime + MEDIA_GRAPH_TARGET_PERIOD_MS;
  NS_ASSERTION(framePosition >= aStream->mBufferStartTime, "frame position before buffer?");
  StreamTime frameBufferTime = GraphTimeToStreamTime(aStream, framePosition);

  TrackTicks start;
  const VideoFrame* frame = nullptr;
  StreamBuffer::Track* track;
  for (StreamBuffer::TrackIter tracks(aStream->GetStreamBuffer(), MediaSegment::VIDEO);
       !tracks.IsEnded(); tracks.Next()) {
    VideoSegment* segment = tracks->Get<VideoSegment>();
    TrackTicks thisStart;
    const VideoFrame* thisFrame =
      segment->GetFrameAt(tracks->TimeToTicksRoundDown(frameBufferTime), &thisStart);
    if (thisFrame && thisFrame->GetImage()) {
      start = thisStart;
      frame = thisFrame;
      track = tracks.get();
    }
  }
  if (!frame || *frame == aStream->mLastPlayedVideoFrame)
    return;

  LOG(PR_LOG_DEBUG, ("MediaStream %p writing video frame %p (%dx%d)",
                     aStream, frame->GetImage(), frame->GetIntrinsicSize().width,
                     frame->GetIntrinsicSize().height));
  GraphTime startTime = StreamTimeToGraphTime(aStream,
      track->TicksToTimeRoundDown(start), INCLUDE_TRAILING_BLOCKED_INTERVAL);
  TimeStamp targetTime = mCurrentTimeStamp +
      TimeDuration::FromMilliseconds(double(startTime - mCurrentTime));
  for (uint32_t i = 0; i < aStream->mVideoOutputs.Length(); ++i) {
    VideoFrameContainer* output = aStream->mVideoOutputs[i];
    output->SetCurrentFrame(frame->GetIntrinsicSize(), frame->GetImage(),
                            targetTime);
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(output, &VideoFrameContainer::Invalidate);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }
  if (!aStream->mNotifiedFinished) {
    aStream->mLastPlayedVideoFrame = *frame;
  }
}

void
MediaStreamGraphImpl::PrepareUpdatesToMainThreadState()
{
  mMonitor.AssertCurrentThreadOwns();

  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaStream* stream = mStreams[i];
    StreamUpdate* update = mStreamUpdates.AppendElement();
    update->mGraphUpdateIndex = stream->mGraphUpdateIndices.GetAt(mCurrentTime);
    update->mStream = stream;
    update->mNextMainThreadCurrentTime =
      GraphTimeToStreamTime(stream, mCurrentTime);
    update->mNextMainThreadFinished =
      stream->mFinished &&
      StreamTimeToGraphTime(stream, stream->GetBufferEnd()) <= mCurrentTime;
  }
  mUpdateRunnables.MoveElementsFrom(mPendingUpdateRunnables);

  EnsureStableStateEventPosted();
}

void
MediaStreamGraphImpl::EnsureImmediateWakeUpLocked(MonitorAutoLock& aLock)
{
  if (mWaitState == WAITSTATE_WAITING_FOR_NEXT_ITERATION ||
      mWaitState == WAITSTATE_WAITING_INDEFINITELY) {
    mWaitState = WAITSTATE_WAKING_UP;
    aLock.Notify();
  }
}

void
MediaStreamGraphImpl::EnsureNextIteration()
{
  MonitorAutoLock lock(mMonitor);
  EnsureNextIterationLocked(lock);
}

void
MediaStreamGraphImpl::EnsureNextIterationLocked(MonitorAutoLock& aLock)
{
  if (mNeedAnotherIteration)
    return;
  mNeedAnotherIteration = true;
  if (mWaitState == WAITSTATE_WAITING_INDEFINITELY) {
    mWaitState = WAITSTATE_WAKING_UP;
    aLock.Notify();
  }
}

void
MediaStreamGraphImpl::RunThread()
{
  nsTArray<MessageBlock> messageQueue;
  {
    MonitorAutoLock lock(mMonitor);
    messageQueue.SwapElements(mMessageQueue);
  }
  NS_ASSERTION(!messageQueue.IsEmpty(),
               "Shouldn't have started a graph with empty message queue!");

  for (;;) {
    // Update mCurrentTime to the min of the playing audio times, or using the
    // wall-clock time change if no audio is playing.
    UpdateCurrentTime();

    // Calculate independent action times for each batch of messages (each
    // batch corresponding to an event loop task). This isolates the performance
    // of different scripts to some extent.
    for (uint32_t i = 0; i < messageQueue.Length(); ++i) {
      mProcessingGraphUpdateIndex = messageQueue[i].mGraphUpdateIndex;
      nsTArray<nsAutoPtr<ControlMessage> >& messages = messageQueue[i].mMessages;

      for (uint32_t j = 0; j < messages.Length(); ++j) {
        messages[j]->Run();
      }
    }
    messageQueue.Clear();

    UpdateStreamOrder();

    int32_t writeAudioUpTo = AUDIO_TARGET_MS;
    GraphTime endBlockingDecisions =
      mCurrentTime + MillisecondsToMediaTime(writeAudioUpTo);
    bool ensureNextIteration = false;

    // Grab pending stream input.
    for (uint32_t i = 0; i < mStreams.Length(); ++i) {
      SourceMediaStream* is = mStreams[i]->AsSourceStream();
      if (is) {
        UpdateConsumptionState(is);
        ExtractPendingInput(is, endBlockingDecisions, &ensureNextIteration);
      }
    }

    // Figure out which streams are blocked and when.
    GraphTime prevComputedTime = mStateComputedTime;
    RecomputeBlocking(endBlockingDecisions);

    // Play stream contents.
    uint32_t audioStreamsActive = 0;
    bool allBlockedForever = true;
    // Figure out what each stream wants to do
    for (uint32_t i = 0; i < mStreams.Length(); ++i) {
      MediaStream* stream = mStreams[i];
      ProcessedMediaStream* ps = stream->AsProcessedStream();
      if (ps && !ps->mFinished) {
        ps->ProduceOutput(prevComputedTime, mStateComputedTime);
        NS_ASSERTION(stream->mBuffer.GetEnd() >=
                     GraphTimeToStreamTime(stream, mStateComputedTime),
                     "Stream did not produce enough data");
      }
      NotifyHasCurrentData(stream);
      CreateOrDestroyAudioStreams(prevComputedTime, stream);
      PlayAudio(stream, prevComputedTime, mStateComputedTime);
      audioStreamsActive += stream->mAudioOutputStreams.Length();
      PlayVideo(stream);
      SourceMediaStream* is = stream->AsSourceStream();
      if (is) {
        UpdateBufferSufficiencyState(is);
      }
      GraphTime end;
      if (!stream->mBlocked.GetAt(mCurrentTime, &end) || end < GRAPH_TIME_MAX) {
        allBlockedForever = false;
      }
    }
    if (ensureNextIteration || !allBlockedForever || audioStreamsActive > 0) {
      EnsureNextIteration();
    }

    // Send updates to the main thread and wait for the next control loop
    // iteration.
    {
      // Not using MonitorAutoLock since we need to unlock in a way
      // that doesn't match lexical scopes.
      MonitorAutoLock lock(mMonitor);
      PrepareUpdatesToMainThreadState();
      if (mForceShutDown || (IsEmpty() && mMessageQueue.IsEmpty())) {
        // Enter shutdown mode. The stable-state handler will detect this
        // and complete shutdown. Destroy any streams immediately.
        LOG(PR_LOG_DEBUG, ("MediaStreamGraph %p waiting for main thread cleanup", this));
        // Commit to shutting down this graph object.
        mLifecycleState = LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP;
        // No need to Destroy streams here. The main-thread owner of each
        // stream is responsible for calling Destroy them.
        return;
      }

      PRIntervalTime timeout = PR_INTERVAL_NO_TIMEOUT;
      TimeStamp now = TimeStamp::Now();
      if (mNeedAnotherIteration) {
        int64_t timeoutMS = MEDIA_GRAPH_TARGET_PERIOD_MS -
          int64_t((now - mCurrentTimeStamp).ToMilliseconds());
        // Make sure timeoutMS doesn't overflow 32 bits by waking up at
        // least once a minute, if we need to wake up at all
        timeoutMS = std::max<int64_t>(0, std::min<int64_t>(timeoutMS, 60*1000));
        timeout = PR_MillisecondsToInterval(uint32_t(timeoutMS));
        LOG(PR_LOG_DEBUG, ("Waiting for next iteration; at %f, timeout=%f",
                           (now - mInitialTimeStamp).ToSeconds(), timeoutMS/1000.0));
        mWaitState = WAITSTATE_WAITING_FOR_NEXT_ITERATION;
      } else {
        mWaitState = WAITSTATE_WAITING_INDEFINITELY;
      }
      if (timeout > 0) {
        mMonitor.Wait(timeout);
        LOG(PR_LOG_DEBUG, ("Resuming after timeout; at %f, elapsed=%f",
                           (TimeStamp::Now() - mInitialTimeStamp).ToSeconds(),
                           (TimeStamp::Now() - now).ToSeconds()));
      }
      mWaitState = WAITSTATE_RUNNING;
      mNeedAnotherIteration = false;
      messageQueue.SwapElements(mMessageQueue);
    }
  }
}

void
MediaStreamGraphImpl::ApplyStreamUpdate(StreamUpdate* aUpdate)
{
  mMonitor.AssertCurrentThreadOwns();

  MediaStream* stream = aUpdate->mStream;
  if (!stream)
    return;
  stream->mMainThreadCurrentTime = aUpdate->mNextMainThreadCurrentTime;
  stream->mMainThreadFinished = aUpdate->mNextMainThreadFinished;

  for (int32_t i = stream->mMainThreadListeners.Length() - 1; i >= 0; --i) {
    stream->mMainThreadListeners[i]->NotifyMainThreadStateChanged();
  }
}

void
MediaStreamGraphImpl::ShutdownThreads()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called on main thread");
  // mGraph's thread is not running so it's OK to do whatever here
  LOG(PR_LOG_DEBUG, ("Stopping threads for MediaStreamGraph %p", this));

  if (mThread) {
    mThread->Shutdown();
    mThread = nullptr;
  }
}

void
MediaStreamGraphImpl::ForceShutDown()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called on main thread");
  LOG(PR_LOG_DEBUG, ("MediaStreamGraph %p ForceShutdown", this));
  {
    MonitorAutoLock lock(mMonitor);
    mForceShutDown = true;
    EnsureImmediateWakeUpLocked(lock);
  }
}

namespace {

class MediaStreamGraphThreadRunnable : public nsRunnable {
public:
  NS_IMETHOD Run()
  {
    gGraph->RunThread();
    return NS_OK;
  }
};

class MediaStreamGraphShutDownRunnable : public nsRunnable {
public:
  MediaStreamGraphShutDownRunnable(MediaStreamGraphImpl* aGraph) : mGraph(aGraph) {}
  NS_IMETHOD Run()
  {
    NS_ASSERTION(mGraph->mDetectedNotRunning,
                 "We should know the graph thread control loop isn't running!");
    // mGraph's thread is not running so it's OK to do whatever here
    if (mGraph->IsEmpty()) {
      // mGraph is no longer needed, so delete it. If the graph is not empty
      // then we must be in a forced shutdown and some later AppendMessage will
      // detect that the manager has been emptied, and delete it.
      delete mGraph;
    } else {
      NS_ASSERTION(mGraph->mForceShutDown, "Not in forced shutdown?");
      mGraph->mLifecycleState =
        MediaStreamGraphImpl::LIFECYCLE_WAITING_FOR_STREAM_DESTRUCTION;
    }
    return NS_OK;
  }
private:
  MediaStreamGraphImpl* mGraph;
};

class MediaStreamGraphStableStateRunnable : public nsRunnable {
public:
  NS_IMETHOD Run()
  {
    if (gGraph) {
      gGraph->RunInStableState();
    }
    return NS_OK;
  }
};

/*
 * Control messages forwarded from main thread to graph manager thread
 */
class CreateMessage : public ControlMessage {
public:
  CreateMessage(MediaStream* aStream) : ControlMessage(aStream) {}
  virtual void Run()
  {
    mStream->GraphImpl()->AddStream(mStream);
    mStream->Init();
  }
};

class MediaStreamGraphShutdownObserver MOZ_FINAL : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

}

void
MediaStreamGraphImpl::RunInStableState()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called on main thread");

  nsTArray<nsCOMPtr<nsIRunnable> > runnables;
  // When we're doing a forced shutdown, pending control messages may be
  // run on the main thread via RunDuringShutdown. Those messages must
  // run without the graph monitor being held. So, we collect them here.
  nsTArray<nsAutoPtr<ControlMessage> > controlMessagesToRunDuringShutdown;

  {
    MonitorAutoLock lock(mMonitor);
    mPostedRunInStableStateEvent = false;

    runnables.SwapElements(mUpdateRunnables);
    for (uint32_t i = 0; i < mStreamUpdates.Length(); ++i) {
      StreamUpdate* update = &mStreamUpdates[i];
      if (update->mStream) {
        ApplyStreamUpdate(update);
      }
    }
    mStreamUpdates.Clear();

    if (mLifecycleState == LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP && mForceShutDown) {
      // Defer calls to RunDuringShutdown() to happen while mMonitor is not held.
      for (uint32_t i = 0; i < mMessageQueue.Length(); ++i) {
        MessageBlock& mb = mMessageQueue[i];
        controlMessagesToRunDuringShutdown.MoveElementsFrom(mb.mMessages);
      }
      mMessageQueue.Clear();
      controlMessagesToRunDuringShutdown.MoveElementsFrom(mCurrentTaskMessageQueue);
      // Stop MediaStreamGraph threads. Do not clear gGraph since
      // we have outstanding DOM objects that may need it.
      mLifecycleState = LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN;
      nsCOMPtr<nsIRunnable> event = new MediaStreamGraphShutDownRunnable(this);
      NS_DispatchToMainThread(event);
    }

    if (mLifecycleState == LIFECYCLE_THREAD_NOT_STARTED) {
      mLifecycleState = LIFECYCLE_RUNNING;
      // Start the thread now. We couldn't start it earlier because
      // the graph might exit immediately on finding it has no streams. The
      // first message for a new graph must create a stream.
      nsCOMPtr<nsIRunnable> event = new MediaStreamGraphThreadRunnable();
      NS_NewThread(getter_AddRefs(mThread), event);
    }

    if (mCurrentTaskMessageQueue.IsEmpty()) {
      if (mLifecycleState == LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP && IsEmpty()) {
        NS_ASSERTION(gGraph == this, "Not current graph??");
        // Complete shutdown. First, ensure that this graph is no longer used.
        // A new graph graph will be created if one is needed.
        LOG(PR_LOG_DEBUG, ("Disconnecting MediaStreamGraph %p", gGraph));
        gGraph = nullptr;
        // Asynchronously clean up old graph. We don't want to do this
        // synchronously because it spins the event loop waiting for threads
        // to shut down, and we don't want to do that in a stable state handler.
        mLifecycleState = LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN;
        nsCOMPtr<nsIRunnable> event = new MediaStreamGraphShutDownRunnable(this);
        NS_DispatchToMainThread(event);
      }
    } else {
      if (mLifecycleState <= LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP) {
        MessageBlock* block = mMessageQueue.AppendElement();
        block->mMessages.SwapElements(mCurrentTaskMessageQueue);
        block->mGraphUpdateIndex = mGraphUpdatesSent;
        ++mGraphUpdatesSent;
        EnsureNextIterationLocked(lock);
      }

      if (mLifecycleState == LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP) {
        mLifecycleState = LIFECYCLE_RUNNING;
        // Revive the MediaStreamGraph since we have more messages going to it.
        // Note that we need to put messages into its queue before reviving it,
        // or it might exit immediately.
        nsCOMPtr<nsIRunnable> event = new MediaStreamGraphThreadRunnable();
        mThread->Dispatch(event, 0);
      }
    }

    mDetectedNotRunning = mLifecycleState > LIFECYCLE_RUNNING;
  }

  // Make sure we get a new current time in the next event loop task
  mPostedRunInStableState = false;

  for (uint32_t i = 0; i < runnables.Length(); ++i) {
    runnables[i]->Run();
  }
  for (uint32_t i = 0; i < controlMessagesToRunDuringShutdown.Length(); ++i) {
    controlMessagesToRunDuringShutdown[i]->RunDuringShutdown();
  }
}

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

void
MediaStreamGraphImpl::EnsureRunInStableState()
{
  NS_ASSERTION(NS_IsMainThread(), "main thread only");

  if (mPostedRunInStableState)
    return;
  mPostedRunInStableState = true;
  nsCOMPtr<nsIRunnable> event = new MediaStreamGraphStableStateRunnable();
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  if (appShell) {
    appShell->RunInStableState(event);
  } else {
    NS_ERROR("Appshell already destroyed?");
  }
}

void
MediaStreamGraphImpl::EnsureStableStateEventPosted()
{
  mMonitor.AssertCurrentThreadOwns();

  if (mPostedRunInStableStateEvent)
    return;
  mPostedRunInStableStateEvent = true;
  nsCOMPtr<nsIRunnable> event = new MediaStreamGraphStableStateRunnable();
  NS_DispatchToMainThread(event);
}

void
MediaStreamGraphImpl::AppendMessage(ControlMessage* aMessage)
{
  NS_ASSERTION(NS_IsMainThread(), "main thread only");
  NS_ASSERTION(!aMessage->GetStream() ||
               !aMessage->GetStream()->IsDestroyed(),
               "Stream already destroyed");

  if (mDetectedNotRunning &&
      mLifecycleState > LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP) {
    // The graph control loop is not running and main thread cleanup has
    // happened. From now on we can't append messages to mCurrentTaskMessageQueue,
    // because that will never be processed again, so just RunDuringShutdown
    // this message.
    // This should only happen during forced shutdown.
    aMessage->RunDuringShutdown();
    delete aMessage;
    if (IsEmpty()) {
      NS_ASSERTION(gGraph == this, "Switched managers during forced shutdown?");
      gGraph = nullptr;
      delete this;
    }
    return;
  }

  mCurrentTaskMessageQueue.AppendElement(aMessage);
  EnsureRunInStableState();
}

void
MediaStream::Init()
{
  MediaStreamGraphImpl* graph = GraphImpl();
  mBlocked.SetAtAndAfter(graph->mCurrentTime, true);
  mExplicitBlockerCount.SetAtAndAfter(graph->mCurrentTime, true);
  mExplicitBlockerCount.SetAtAndAfter(graph->mStateComputedTime, false);
}

MediaStreamGraphImpl*
MediaStream::GraphImpl()
{
  return gGraph;
}

MediaStreamGraph*
MediaStream::Graph()
{
  return gGraph;
}

StreamTime
MediaStream::GraphTimeToStreamTime(GraphTime aTime)
{
  return GraphImpl()->GraphTimeToStreamTime(this, aTime);
}

void
MediaStream::FinishOnGraphThread()
{
  GraphImpl()->FinishStream(this);
}

void
MediaStream::RemoveAllListenersImpl()
{
  for (int32_t i = mListeners.Length() - 1; i >= 0; --i) {
    nsRefPtr<MediaStreamListener> listener = mListeners[i].forget();
    listener->NotifyRemoved(GraphImpl());
  }
  mListeners.Clear();
}

void
MediaStream::DestroyImpl()
{
  RemoveAllListenersImpl();

  for (int32_t i = mConsumers.Length() - 1; i >= 0; --i) {
    mConsumers[i]->Disconnect();
  }
  for (uint32_t i = 0; i < mAudioOutputStreams.Length(); ++i) {
    mAudioOutputStreams[i].mStream->Shutdown();
  }
  mAudioOutputStreams.Clear();
}

void
MediaStream::Destroy()
{
  // Keep this stream alive until we leave this method
  nsRefPtr<MediaStream> kungFuDeathGrip = this;

  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream) : ControlMessage(aStream) {}
    virtual void Run()
    {
      mStream->DestroyImpl();
      mStream->GraphImpl()->RemoveStream(mStream);
    }
    virtual void RunDuringShutdown()
    { Run(); }
  };
  mWrapper = nullptr;
  GraphImpl()->AppendMessage(new Message(this));
  // Message::RunDuringShutdown may have removed this stream from the graph,
  // but our kungFuDeathGrip above will have kept this stream alive if
  // necessary.
  mMainThreadDestroyed = true;
}

void
MediaStream::AddAudioOutput(void* aKey)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, void* aKey) : ControlMessage(aStream), mKey(aKey) {}
    virtual void Run()
    {
      mStream->AddAudioOutputImpl(mKey);
    }
    void* mKey;
  };
  GraphImpl()->AppendMessage(new Message(this, aKey));
}

void
MediaStream::SetAudioOutputVolumeImpl(void* aKey, float aVolume)
{
  for (uint32_t i = 0; i < mAudioOutputs.Length(); ++i) {
    if (mAudioOutputs[i].mKey == aKey) {
      mAudioOutputs[i].mVolume = aVolume;
      return;
    }
  }
  NS_ERROR("Audio output key not found");
}

void
MediaStream::SetAudioOutputVolume(void* aKey, float aVolume)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, void* aKey, float aVolume) :
      ControlMessage(aStream), mKey(aKey), mVolume(aVolume) {}
    virtual void Run()
    {
      mStream->SetAudioOutputVolumeImpl(mKey, mVolume);
    }
    void* mKey;
    float mVolume;
  };
  GraphImpl()->AppendMessage(new Message(this, aKey, aVolume));
}

void
MediaStream::RemoveAudioOutputImpl(void* aKey)
{
  for (uint32_t i = 0; i < mAudioOutputs.Length(); ++i) {
    if (mAudioOutputs[i].mKey == aKey) {
      mAudioOutputs.RemoveElementAt(i);
      return;
    }
  }
  NS_ERROR("Audio output key not found");
}

void
MediaStream::RemoveAudioOutput(void* aKey)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, void* aKey) :
      ControlMessage(aStream), mKey(aKey) {}
    virtual void Run()
    {
      mStream->RemoveAudioOutputImpl(mKey);
    }
    void* mKey;
  };
  GraphImpl()->AppendMessage(new Message(this, aKey));
}

void
MediaStream::AddVideoOutput(VideoFrameContainer* aContainer)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, VideoFrameContainer* aContainer) :
      ControlMessage(aStream), mContainer(aContainer) {}
    virtual void Run()
    {
      mStream->AddVideoOutputImpl(mContainer.forget());
    }
    nsRefPtr<VideoFrameContainer> mContainer;
  };
  GraphImpl()->AppendMessage(new Message(this, aContainer));
}

void
MediaStream::RemoveVideoOutput(VideoFrameContainer* aContainer)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, VideoFrameContainer* aContainer) :
      ControlMessage(aStream), mContainer(aContainer) {}
    virtual void Run()
    {
      mStream->RemoveVideoOutputImpl(mContainer);
    }
    nsRefPtr<VideoFrameContainer> mContainer;
  };
  GraphImpl()->AppendMessage(new Message(this, aContainer));
}

void
MediaStream::ChangeExplicitBlockerCount(int32_t aDelta)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, int32_t aDelta) :
      ControlMessage(aStream), mDelta(aDelta) {}
    virtual void Run()
    {
      mStream->ChangeExplicitBlockerCountImpl(
          mStream->GraphImpl()->mStateComputedTime, mDelta);
    }
    int32_t mDelta;
  };
  GraphImpl()->AppendMessage(new Message(this, aDelta));
}

void
MediaStream::AddListenerImpl(already_AddRefed<MediaStreamListener> aListener)
{
  MediaStreamListener* listener = *mListeners.AppendElement() = aListener;
  listener->NotifyBlockingChanged(GraphImpl(),
    mNotifiedBlocked ? MediaStreamListener::BLOCKED : MediaStreamListener::UNBLOCKED);
  if (mNotifiedFinished) {
    listener->NotifyFinished(GraphImpl());
  }
}

void
MediaStream::AddListener(MediaStreamListener* aListener)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, MediaStreamListener* aListener) :
      ControlMessage(aStream), mListener(aListener) {}
    virtual void Run()
    {
      mStream->AddListenerImpl(mListener.forget());
    }
    nsRefPtr<MediaStreamListener> mListener;
  };
  GraphImpl()->AppendMessage(new Message(this, aListener));
}

void
MediaStream::RemoveListenerImpl(MediaStreamListener* aListener)
{ 
  // wouldn't need this if we could do it in the opposite order
  nsRefPtr<MediaStreamListener> listener(aListener);
  mListeners.RemoveElement(aListener);
  listener->NotifyRemoved(GraphImpl());
}

void
MediaStream::RemoveListener(MediaStreamListener* aListener)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, MediaStreamListener* aListener) :
      ControlMessage(aStream), mListener(aListener) {}
    virtual void Run()
    {
      mStream->RemoveListenerImpl(mListener);
    }
    nsRefPtr<MediaStreamListener> mListener;
  };
  GraphImpl()->AppendMessage(new Message(this, aListener));
}

void
SourceMediaStream::DestroyImpl()
{
  {
    MutexAutoLock lock(mMutex);
    mDestroyed = true;
  }
  MediaStream::DestroyImpl();
}

void
SourceMediaStream::SetPullEnabled(bool aEnabled)
{
  MutexAutoLock lock(mMutex);
  mPullEnabled = aEnabled;
  if (mPullEnabled && !mDestroyed) {
    GraphImpl()->EnsureNextIteration();
  }
}

void
SourceMediaStream::AddTrack(TrackID aID, TrackRate aRate, TrackTicks aStart,
                            MediaSegment* aSegment)
{
  MutexAutoLock lock(mMutex);
  TrackData* data = mUpdateTracks.AppendElement();
  data->mID = aID;
  data->mRate = aRate;
  data->mStart = aStart;
  data->mCommands = TRACK_CREATE;
  data->mData = aSegment;
  data->mHaveEnough = false;
  if (!mDestroyed) {
    GraphImpl()->EnsureNextIteration();
  }
}

void
SourceMediaStream::AppendToTrack(TrackID aID, MediaSegment* aSegment)
{
  MutexAutoLock lock(mMutex);
  // ::EndAllTrackAndFinished() can end these before the sources notice
  if (!mFinished) {
    TrackData *track = FindDataForTrack(aID);
    if (track) {
      track->mData->AppendFrom(aSegment);
    } else {
      NS_ERROR("Append to non-existent track!");
    }
  }
  if (!mDestroyed) {
    GraphImpl()->EnsureNextIteration();
  }
}

bool
SourceMediaStream::HaveEnoughBuffered(TrackID aID)
{
  MutexAutoLock lock(mMutex);
  TrackData *track = FindDataForTrack(aID);
  if (track) {
    return track->mHaveEnough;
  }
  NS_ERROR("No track in HaveEnoughBuffered!");
  return true;
}

void
SourceMediaStream::DispatchWhenNotEnoughBuffered(TrackID aID,
    nsIThread* aSignalThread, nsIRunnable* aSignalRunnable)
{
  MutexAutoLock lock(mMutex);
  TrackData* data = FindDataForTrack(aID);
  if (!data) {
    NS_ERROR("No track in DispatchWhenNotEnoughBuffered");
    return;
  }

  if (data->mHaveEnough) {
    data->mDispatchWhenNotEnough.AppendElement()->Init(aSignalThread, aSignalRunnable);
  } else {
    aSignalThread->Dispatch(aSignalRunnable, 0);
  }
}

void
SourceMediaStream::EndTrack(TrackID aID)
{
  MutexAutoLock lock(mMutex);
  // ::EndAllTrackAndFinished() can end these before the sources call this
  if (!mFinished) {
    TrackData *track = FindDataForTrack(aID);
    if (track) {
      track->mCommands |= TRACK_END;
    } else {
      NS_ERROR("End of non-existant track");
    }
  }
  if (!mDestroyed) {
    GraphImpl()->EnsureNextIteration();
  }
}

void
SourceMediaStream::AdvanceKnownTracksTime(StreamTime aKnownTime)
{
  MutexAutoLock lock(mMutex);
  mUpdateKnownTracksTime = aKnownTime;
  if (!mDestroyed) {
    GraphImpl()->EnsureNextIteration();
  }
}

void
SourceMediaStream::FinishWithLockHeld()
{
  mUpdateFinished = true;
  if (!mDestroyed) {
    GraphImpl()->EnsureNextIteration();
  }
}

void
SourceMediaStream::EndAllTrackAndFinish()
{
  {
    MutexAutoLock lock(mMutex);
    for (uint32_t i = 0; i < mUpdateTracks.Length(); ++i) {
      SourceMediaStream::TrackData* data = &mUpdateTracks[i];
      data->mCommands |= TRACK_END;
    }
  }
  FinishWithLockHeld();
  // we will call NotifyFinished() to let GetUserMedia know
}

void
MediaInputPort::Init()
{
  LOG(PR_LOG_DEBUG, ("Adding MediaInputPort %p (from %p to %p) to the graph",
      this, mSource, mDest));
  mSource->AddConsumer(this);
  mDest->AddInput(this);
  // mPortCount decremented via MediaInputPort::Destroy's message
  ++mDest->GraphImpl()->mPortCount;
}

void
MediaInputPort::Disconnect()
{
  NS_ASSERTION(!mSource == !mDest,
               "mSource must either both be null or both non-null");
  if (!mSource)
    return;

  mSource->RemoveConsumer(this);
  mSource = nullptr;
  mDest->RemoveInput(this);
  mDest = nullptr;
}

MediaInputPort::InputInterval
MediaInputPort::GetNextInputInterval(GraphTime aTime)
{
  InputInterval result = { GRAPH_TIME_MAX, GRAPH_TIME_MAX, false };
  GraphTime t = aTime;
  GraphTime end;
  for (;;) {
    if (!mDest->mBlocked.GetAt(t, &end))
      break;
    if (end == GRAPH_TIME_MAX)
      return result;
    t = end;
  }
  result.mStart = t;
  GraphTime sourceEnd;
  result.mInputIsBlocked = mSource->mBlocked.GetAt(t, &sourceEnd);
  result.mEnd = std::min(end, sourceEnd);
  return result;
}

void
MediaInputPort::Destroy()
{
  class Message : public ControlMessage {
  public:
    Message(MediaInputPort* aPort)
      : ControlMessage(nullptr), mPort(aPort) {}
    virtual void Run()
    {
      mPort->Disconnect();
      --mPort->GraphImpl()->mPortCount;
      NS_RELEASE(mPort);
    }
    virtual void RunDuringShutdown()
    {
      Run();
    }
    // This does not need to be strongly referenced; the graph is holding
    // a strong reference to the port, which we will remove. This will be the
    // last message for the port.
    MediaInputPort* mPort;
  };
  GraphImpl()->AppendMessage(new Message(this));
}

MediaStreamGraphImpl*
MediaInputPort::GraphImpl()
{
  return gGraph;
}

MediaStreamGraph*
MediaInputPort::Graph()
{
  return gGraph;
}

MediaInputPort*
ProcessedMediaStream::AllocateInputPort(MediaStream* aStream, uint32_t aFlags)
{
  class Message : public ControlMessage {
  public:
    Message(MediaInputPort* aPort)
      : ControlMessage(aPort->GetDestination()),
        mPort(aPort) {}
    virtual void Run()
    {
      mPort->Init();
    }
    MediaInputPort* mPort;
  };
  MediaInputPort* port = new MediaInputPort(aStream, this, aFlags);
  NS_ADDREF(port);
  GraphImpl()->AppendMessage(new Message(port));
  return port;
}

void
ProcessedMediaStream::Finish()
{
  class Message : public ControlMessage {
  public:
    Message(ProcessedMediaStream* aStream)
      : ControlMessage(aStream) {}
    virtual void Run()
    {
      mStream->GraphImpl()->FinishStream(mStream);
    }
  };
  GraphImpl()->AppendMessage(new Message(this));
}

void
ProcessedMediaStream::SetAutofinish(bool aAutofinish)
{
  class Message : public ControlMessage {
  public:
    Message(ProcessedMediaStream* aStream, bool aAutofinish)
      : ControlMessage(aStream), mAutofinish(aAutofinish) {}
    virtual void Run()
    {
      mStream->AsProcessedStream()->SetAutofinishImpl(mAutofinish);
    }
    bool mAutofinish;
  };
  GraphImpl()->AppendMessage(new Message(this, aAutofinish));
}

void
ProcessedMediaStream::DestroyImpl()
{
  for (int32_t i = mInputs.Length() - 1; i >= 0; --i) {
    mInputs[i]->Disconnect();
  }
  MediaStream::DestroyImpl();
}

/**
 * We make the initial mCurrentTime nonzero so that zero times can have
 * special meaning if necessary.
 */
static const int32_t INITIAL_CURRENT_TIME = 1;

MediaStreamGraphImpl::MediaStreamGraphImpl()
  : mCurrentTime(INITIAL_CURRENT_TIME)
  , mStateComputedTime(INITIAL_CURRENT_TIME)
  , mProcessingGraphUpdateIndex(0)
  , mPortCount(0)
  , mMonitor("MediaStreamGraphImpl")
  , mLifecycleState(LIFECYCLE_THREAD_NOT_STARTED)
  , mWaitState(WAITSTATE_RUNNING)
  , mNeedAnotherIteration(false)
  , mForceShutDown(false)
  , mPostedRunInStableStateEvent(false)
  , mDetectedNotRunning(false)
  , mPostedRunInStableState(false)
{
#ifdef PR_LOGGING
  if (!gMediaStreamGraphLog) {
    gMediaStreamGraphLog = PR_NewLogModule("MediaStreamGraph");
  }
#endif

  mCurrentTimeStamp = mInitialTimeStamp = TimeStamp::Now();
}

NS_IMPL_ISUPPORTS1(MediaStreamGraphShutdownObserver, nsIObserver)

static bool gShutdownObserverRegistered = false;

NS_IMETHODIMP
MediaStreamGraphShutdownObserver::Observe(nsISupports *aSubject,
                                          const char *aTopic,
                                          const PRUnichar *aData)
{
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    if (gGraph) {
      gGraph->ForceShutDown();
    }
    nsContentUtils::UnregisterShutdownObserver(this);
    gShutdownObserverRegistered = false;
  }
  return NS_OK;
}

MediaStreamGraph*
MediaStreamGraph::GetInstance()
{
  NS_ASSERTION(NS_IsMainThread(), "Main thread only");

  if (!gGraph) {
    if (!gShutdownObserverRegistered) {
      gShutdownObserverRegistered = true;
      nsContentUtils::RegisterShutdownObserver(new MediaStreamGraphShutdownObserver());
    }

    gGraph = new MediaStreamGraphImpl();
    LOG(PR_LOG_DEBUG, ("Starting up MediaStreamGraph %p", gGraph));
  }

  return gGraph;
}

SourceMediaStream*
MediaStreamGraph::CreateSourceStream(nsDOMMediaStream* aWrapper)
{
  SourceMediaStream* stream = new SourceMediaStream(aWrapper);
  NS_ADDREF(stream);
  static_cast<MediaStreamGraphImpl*>(this)->AppendMessage(new CreateMessage(stream));
  return stream;
}

ProcessedMediaStream*
MediaStreamGraph::CreateTrackUnionStream(nsDOMMediaStream* aWrapper)
{
  TrackUnionStream* stream = new TrackUnionStream(aWrapper);
  NS_ADDREF(stream);
  static_cast<MediaStreamGraphImpl*>(this)->AppendMessage(new CreateMessage(stream));
  return stream;
}

}
