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

using namespace mozilla::layers;

namespace mozilla {

namespace {

#ifdef PR_LOGGING
PRLogModuleInfo* gMediaStreamGraphLog;
#define LOG(type, msg) PR_LOG(gMediaStreamGraphLog, type, msg)
#else
#define LOG(type, msg)
#endif

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
  PRInt64 mGraphUpdateIndex;
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
  // Executed before we know what the action time for this message will be.
  // Call NoteStreamAffected on the stream whose output will be
  // modified by this message. Default implementation calls
  // NoteStreamAffected(mStream).
  virtual void UpdateAffectedStream();
  // Executed after we know what the action time for this message will be.
  virtual void Process() {}
  // When we're shutting down the application, most messages are ignored but
  // some cleanup messages should still be processed (on the main thread).
  virtual void ProcessDuringShutdown() {}

protected:
  // We do not hold a reference to mStream. The main thread will be holding
  // a reference to the stream while this message is in flight. The last message
  // referencing a stream is the Destroy message for that stream.
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
   * Update mLastActionTime to the time at which the current set of messages
   * will take effect.
   */
  void ChooseActionTime();
  /**
   * Update the consumption state of aStream to reflect whether its data
   * is needed or not.
   */
  void UpdateConsumptionState(SourceMediaStream* aStream);
  /**
   * Extract any state updates pending in aStream, and apply them.
   */
  void ExtractPendingInput(SourceMediaStream* aStream);
  /**
   * Update "have enough data" flags in aStream.
   */
  void UpdateBufferSufficiencyState(SourceMediaStream* aStream);
  /**
   * Compute the blocking states of streams from mBlockingDecisionsMadeUntilTime
   * until the desired future time (determined by heuristic).
   * Updates mBlockingDecisionsMadeUntilTime and sets MediaStream::mBlocked
   * for all streams.
   */
  void RecomputeBlocking();
  // The following methods are used to help RecomputeBlocking.
  /**
   * Mark a stream blocked at time aTime. If this results in decisions that need
   * to be revisited at some point in the future, *aEnd will be reduced to the
   * first time in the future to recompute those decisions.
   */
  void MarkStreamBlocked(MediaStream* aStream, GraphTime aTime, GraphTime* aEnd);
  /**
   * Recompute blocking for all streams for the interval starting at aTime.
   * If this results in decisions that need to be revisited at some point
   * in the future, *aEnd will be reduced to the first time in the future to
   * recompute those decisions.
   */
  void RecomputeBlockingAt(GraphTime aTime, GraphTime aEndBlockingDecisions,
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
   * Return true if there is an explicit blocker set from the current time
   * indefinitely far into the future.
   */
  bool IsAlwaysExplicitlyBlocked(MediaStream* aStream);
  /**
   * Given a graph time aTime, convert it to a stream time taking into
   * account the time during which aStream is scheduled to be blocked.
   */
  StreamTime GraphTimeToStreamTime(MediaStream* aStream, StreamTime aTime);
  enum {
    INCLUDE_TRAILING_BLOCKED_INTERVAL = 0x01
  };
  /**
   * Given a stream time aTime, convert it to a graph time taking into
   * account the time during which aStream is scheduled to be blocked.
   * aTime must be <= mBlockingDecisionsMadeUntilTime since blocking decisions
   * are only known up to that point.
   * If aTime is exactly at the start of a blocked interval, then the blocked
   * interval is included in the time returned if and only if
   * aFlags includes INCLUDE_TRAILING_BLOCKED_INTERVAL.
   */
  GraphTime StreamTimeToGraphTime(MediaStream* aStream, StreamTime aTime,
                                  PRUint32 aFlags = 0);
  /**
   * Get the current audio position of the stream's audio output.
   */
  GraphTime GetAudioPosition(MediaStream* aStream);
  /**
   * If aStream needs an audio stream but doesn't have one, create it.
   * If aStream doesn't need an audio stream but has one, destroy it.
   */
  void CreateOrDestroyAudioStream(GraphTime aAudioOutputStartTime,
                                  MediaStream* aStream);
  /**
   * Update aStream->mFirstActiveTracks.
   */
  void UpdateFirstActiveTracks(MediaStream* aStream);
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
  bool IsEmpty() { return mStreams.IsEmpty(); }

  // For use by control messages
  /**
   * Identify which graph update index we are currently processing.
   */
  PRInt64 GetProcessingGraphUpdateIndex() { return mProcessingGraphUpdateIndex; }
  /**
   * Marks aStream as affected by a change in its output at desired time aTime
   * (in the timeline of aStream). The change may not actually happen at this time,
   * it may be delayed until later if there is buffered data we can't change.
   */
  void NoteStreamAffected(MediaStream* aStream, double aTime);
  /**
   * Marks aStream as affected by a change in its output at the earliest
   * possible time.
   */
  void NoteStreamAffected(MediaStream* aStream);
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
   * Compute the earliest time at which an action be allowed to occur on any
   * stream. Actions cannot be earlier than the previous action time, and
   * cannot affect already-committed blocking decisions (and associated
   * buffered audio).
   */
  GraphTime GetEarliestActionTime()
  {
    return NS_MAX(mCurrentTime, NS_MAX(mLastActionTime, mBlockingDecisionsMadeUntilTime));
  }

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
   * The time the last action was deemed to have occurred. This could be
   * later than mCurrentTime if actions have to be delayed during data
   * buffering, or before mCurrentTime if mCurrentTime has advanced since
   * the last action happened. In ControlMessage::Process calls,
   * mLastActionTime has always been updated to be >= mCurrentTime.
   */
  GraphTime mLastActionTime;
  /**
   * The current graph time for the current iteration of the RunThread control
   * loop.
   */
  GraphTime mCurrentTime;
  /**
   * Blocking decisions have been made up to this time. We also buffer audio
   * up to this time.
   */
  GraphTime mBlockingDecisionsMadeUntilTime;
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
  PRInt64 mProcessingGraphUpdateIndex;

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
    PRInt64 mGraphUpdateIndex;
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
  StreamTime desiredEnd = current;
  if (!aStream->mAudioOutputs.IsEmpty()) {
    desiredEnd = NS_MAX(desiredEnd, current + MillisecondsToMediaTime(AUDIO_TARGET_MS));
  }
  if (!aStream->mVideoOutputs.IsEmpty()) {
    desiredEnd = NS_MAX(desiredEnd, current + MillisecondsToMediaTime(VIDEO_TARGET_MS));
  }
  return desiredEnd;
}

bool
MediaStreamGraphImpl::IsAlwaysExplicitlyBlocked(MediaStream* aStream)
{
  GraphTime t = mCurrentTime;
  while (true) {
    GraphTime end;
    if (aStream->mExplicitBlockerCount.GetAt(t, &end) == 0)
      return false;
    if (end >= GRAPH_TIME_MAX)
      return true;
    t = end;
  }
}

void
MediaStreamGraphImpl::FinishStream(MediaStream* aStream)
{
  if (aStream->mFinished)
    return;
  printf("MediaStreamGraphImpl::FinishStream\n");
  LOG(PR_LOG_DEBUG, ("MediaStream %p will finish", aStream));
  aStream->mFinished = true;
  // Force at least one more iteration of the control loop, since we rely
  // on UpdateCurrentTime to notify our listeners once the stream end
  // has been reached.
  EnsureNextIteration();
}

void
MediaStreamGraphImpl::NoteStreamAffected(MediaStream* aStream, double aTime)
{
  NS_ASSERTION(aTime >= 0, "Bad time");
  GraphTime t =
      NS_MAX(GetEarliestActionTime(),
          StreamTimeToGraphTime(aStream, SecondsToMediaTime(aTime),
                                INCLUDE_TRAILING_BLOCKED_INTERVAL));
  aStream->mMessageAffectedTime = NS_MIN(aStream->mMessageAffectedTime, t);
}

void
MediaStreamGraphImpl::NoteStreamAffected(MediaStream* aStream)
{
  GraphTime t = GetEarliestActionTime();
  aStream->mMessageAffectedTime = NS_MIN(aStream->mMessageAffectedTime, t);
}

void
ControlMessage::UpdateAffectedStream()
{
  NS_ASSERTION(mStream, "Must have stream for default UpdateAffectedStream");
  mStream->GraphImpl()->NoteStreamAffected(mStream);
}

void
MediaStreamGraphImpl::AddStream(MediaStream* aStream)
{
  aStream->mBufferStartTime = mCurrentTime;
  aStream->mMessageAffectedTime = GetEarliestActionTime();
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
    for (PRUint32 i = 0; i < mStreamUpdates.Length(); ++i) {
      if (mStreamUpdates[i].mStream == aStream) {
        mStreamUpdates[i].mStream = nsnull;
      }
    }
  }

  // This unrefs the stream, probably destroying it
  mStreams.RemoveElement(aStream);

  LOG(PR_LOG_DEBUG, ("Removing media stream %p from the graph", aStream));
}

void
MediaStreamGraphImpl::ChooseActionTime()
{
  mLastActionTime = GetEarliestActionTime();
}

void
MediaStreamGraphImpl::UpdateConsumptionState(SourceMediaStream* aStream)
{
  bool isConsumed = !aStream->mAudioOutputs.IsEmpty() ||
    !aStream->mVideoOutputs.IsEmpty();
  MediaStreamListener::Consumption state = isConsumed ? MediaStreamListener::CONSUMED
    : MediaStreamListener::NOT_CONSUMED;
  if (state != aStream->mLastConsumptionState) {
    aStream->mLastConsumptionState = state;
    for (PRUint32 j = 0; j < aStream->mListeners.Length(); ++j) {
      MediaStreamListener* l = aStream->mListeners[j];
      l->NotifyConsumptionChanged(this, state);
    }
  }
}

void
MediaStreamGraphImpl::ExtractPendingInput(SourceMediaStream* aStream)
{
  bool finished;
  {
    MutexAutoLock lock(aStream->mMutex);
    finished = aStream->mUpdateFinished;
    for (PRInt32 i = aStream->mUpdateTracks.Length() - 1; i >= 0; --i) {
      SourceMediaStream::TrackData* data = &aStream->mUpdateTracks[i];
      for (PRUint32 j = 0; j < aStream->mListeners.Length(); ++j) {
        MediaStreamListener* l = aStream->mListeners[j];
        TrackTicks offset = (data->mCommands & SourceMediaStream::TRACK_CREATE)
            ? data->mStart : aStream->mBuffer.FindTrack(data->mID)->GetSegment()->GetDuration();
        l->NotifyQueuedTrackChanges(this, data->mID, data->mRate,
                                    offset, data->mCommands, *data->mData);
      }
      if (data->mCommands & SourceMediaStream::TRACK_CREATE) {
        MediaSegment* segment = data->mData.forget();
        LOG(PR_LOG_DEBUG, ("SourceMediaStream %p creating track %d, rate %d, start %lld, initial end %lld",
                           aStream, data->mID, data->mRate, PRInt64(data->mStart),
                           PRInt64(segment->GetDuration())));
        aStream->mBuffer.AddTrack(data->mID, data->mRate, data->mStart, segment);
        // The track has taken ownership of data->mData, so let's replace
        // data->mData with an empty clone.
        data->mData = segment->CreateEmptyClone();
        data->mCommands &= ~SourceMediaStream::TRACK_CREATE;
      } else if (data->mData->GetDuration() > 0) {
        MediaSegment* dest = aStream->mBuffer.FindTrack(data->mID)->GetSegment();
        LOG(PR_LOG_DEBUG, ("SourceMediaStream %p track %d, advancing end from %lld to %lld",
                           aStream, data->mID,
                           PRInt64(dest->GetDuration()),
                           PRInt64(dest->GetDuration() + data->mData->GetDuration())));
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
    for (PRUint32 i = 0; i < aStream->mUpdateTracks.Length(); ++i) {
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

  for (PRUint32 i = 0; i < runnables.Length(); ++i) {
    runnables[i].mThread->Dispatch(runnables[i].mRunnable, 0);
  }
}


StreamTime
MediaStreamGraphImpl::GraphTimeToStreamTime(MediaStream* aStream,
                                            GraphTime aTime)
{
  NS_ASSERTION(aTime <= mBlockingDecisionsMadeUntilTime,
               "Don't ask about times where we haven't made blocking decisions yet");
  if (aTime <= mCurrentTime) {
    return NS_MAX<StreamTime>(0, aTime - aStream->mBufferStartTime);
  }
  GraphTime t = mCurrentTime;
  StreamTime s = t - aStream->mBufferStartTime;
  while (t < aTime) {
    GraphTime end;
    if (!aStream->mBlocked.GetAt(t, &end)) {
      s += NS_MIN(aTime, end) - t;
    }
    t = end;
  }
  return NS_MAX<StreamTime>(0, s);
}  

GraphTime
MediaStreamGraphImpl::StreamTimeToGraphTime(MediaStream* aStream,
                                            StreamTime aTime, PRUint32 aFlags)
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
    if (t < mBlockingDecisionsMadeUntilTime) {
      blocked = aStream->mBlocked.GetAt(t, &end);
      end = NS_MIN(end, mBlockingDecisionsMadeUntilTime);
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
      MediaTime consume = NS_MIN(end - t, streamAmount);
      streamAmount -= consume;
      t += consume;
    }
  }
  return t;
}

GraphTime
MediaStreamGraphImpl::GetAudioPosition(MediaStream* aStream)
{
  if (!aStream->mAudioOutput) {
    return mCurrentTime;
  }
  PRInt64 positionInFrames = aStream->mAudioOutput->GetPositionInFrames();
  if (positionInFrames < 0) {
    return mCurrentTime;
  }
  return aStream->mAudioPlaybackStartTime +
      TicksToTimeRoundDown(aStream->mAudioOutput->GetRate(),
                           positionInFrames);
}

void
MediaStreamGraphImpl::UpdateCurrentTime()
{
  GraphTime prevCurrentTime = mCurrentTime;
  TimeStamp now = TimeStamp::Now();
  GraphTime nextCurrentTime =
    SecondsToMediaTime((now - mCurrentTimeStamp).ToSeconds()) + mCurrentTime;
  if (mBlockingDecisionsMadeUntilTime < nextCurrentTime) {
    LOG(PR_LOG_WARNING, ("Media graph global underrun detected"));
    LOG(PR_LOG_DEBUG, ("Advancing mBlockingDecisionsMadeUntilTime from %f to %f",
                       MediaTimeToSeconds(mBlockingDecisionsMadeUntilTime),
                       MediaTimeToSeconds(nextCurrentTime)));
    // Advance mBlockingDecisionsMadeUntilTime to nextCurrentTime by
    // adding blocked time to all streams starting at mBlockingDecisionsMadeUntilTime
    for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
      mStreams[i]->mBlocked.SetAtAndAfter(mBlockingDecisionsMadeUntilTime, true);
    }
    mBlockingDecisionsMadeUntilTime = nextCurrentTime;
  }
  mCurrentTimeStamp = now;

  LOG(PR_LOG_DEBUG, ("Updating current time to %f (real %f, mBlockingDecisionsMadeUntilTime %f)",
                     MediaTimeToSeconds(nextCurrentTime),
                     (now - mInitialTimeStamp).ToSeconds(),
                     MediaTimeToSeconds(mBlockingDecisionsMadeUntilTime)));

  if (prevCurrentTime >= nextCurrentTime) {
    NS_ASSERTION(prevCurrentTime == nextCurrentTime, "Time can't go backwards!");
    // This could happen due to low clock resolution, maybe?
    LOG(PR_LOG_DEBUG, ("Time did not advance"));
    // There's not much left to do here, but the code below that notifies
    // listeners that streams have ended still needs to run.
  }

  for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
    MediaStream* stream = mStreams[i];

    // Calculate blocked time and fire Blocked/Unblocked events
    GraphTime blockedTime = 0;
    GraphTime t = prevCurrentTime;
    // Save current blocked status
    bool wasBlocked = stream->mBlocked.GetAt(prevCurrentTime);
    while (t < nextCurrentTime) {
      GraphTime end;
      bool blocked = stream->mBlocked.GetAt(t, &end);
      if (blocked) {
        blockedTime += NS_MIN(end, nextCurrentTime) - t;
      }
      if (blocked != wasBlocked) {
        for (PRUint32 j = 0; j < stream->mListeners.Length(); ++j) {
          MediaStreamListener* l = stream->mListeners[j];
          l->NotifyBlockingChanged(this,
              blocked ? MediaStreamListener::BLOCKED : MediaStreamListener::UNBLOCKED);
        }
        wasBlocked = blocked;
      }
      t = end;
    }

    stream->AdvanceTimeVaryingValuesToCurrentTime(nextCurrentTime, blockedTime);
    // Advance mBlocked last so that implementations of
    // AdvanceTimeVaryingValuesToCurrentTime can rely on the value of mBlocked.
    stream->mBlocked.AdvanceCurrentTime(nextCurrentTime);

    if (blockedTime < nextCurrentTime - prevCurrentTime) {
      for (PRUint32 i = 0; i < stream->mListeners.Length(); ++i) {
        MediaStreamListener* l = stream->mListeners[i];
        l->NotifyOutput(this);
      }
    }

    if (stream->mFinished && !stream->mNotifiedFinished &&
        stream->mBufferStartTime + stream->GetBufferEnd() <= nextCurrentTime) {
      stream->mNotifiedFinished = true;
      for (PRUint32 j = 0; j < stream->mListeners.Length(); ++j) {
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

void
MediaStreamGraphImpl::MarkStreamBlocked(MediaStream* aStream,
                                        GraphTime aTime, GraphTime* aEnd)
{
  NS_ASSERTION(!aStream->mBlocked.GetAt(aTime), "MediaStream already blocked");

  aStream->mBlocked.SetAtAndAfter(aTime, true);
}

bool
MediaStreamGraphImpl::WillUnderrun(MediaStream* aStream, GraphTime aTime,
                                   GraphTime aEndBlockingDecisions, GraphTime* aEnd)
{
  // Finished streams, or streams that aren't being played back, can't underrun.
  if (aStream->mFinished ||
      (aStream->mAudioOutputs.IsEmpty() && aStream->mVideoOutputs.IsEmpty())) {
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
  *aEnd = NS_MIN(*aEnd, bufferEnd);
  return false;
}

void
MediaStreamGraphImpl::RecomputeBlocking()
{
  PRInt32 writeAudioUpTo = AUDIO_TARGET_MS;
  GraphTime endBlockingDecisions =
    mCurrentTime + MillisecondsToMediaTime(writeAudioUpTo);

  bool blockingDecisionsWillChange = false;
  // mBlockingDecisionsMadeUntilTime has been set in UpdateCurrentTime
  while (mBlockingDecisionsMadeUntilTime < endBlockingDecisions) {
    LOG(PR_LOG_DEBUG, ("Media graph %p computing blocking for time %f",
                       this, MediaTimeToSeconds(mBlockingDecisionsMadeUntilTime)));
    GraphTime end = GRAPH_TIME_MAX;
    RecomputeBlockingAt(mBlockingDecisionsMadeUntilTime, endBlockingDecisions, &end);
    LOG(PR_LOG_DEBUG, ("Media graph %p computed blocking for interval %f to %f",
                       this, MediaTimeToSeconds(mBlockingDecisionsMadeUntilTime),
                       MediaTimeToSeconds(end)));
    mBlockingDecisionsMadeUntilTime = end;
    if (end < GRAPH_TIME_MAX) {
      blockingDecisionsWillChange = true;
    }
  }
  mBlockingDecisionsMadeUntilTime = endBlockingDecisions;

  for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
    MediaStream* stream = mStreams[i];
    GraphTime end;
    stream->mBlocked.GetAt(mCurrentTime, &end);
    if (end < GRAPH_TIME_MAX) {
      blockingDecisionsWillChange = true;
    }
  }
  if (blockingDecisionsWillChange) {
    // Make sure we wake up to notify listeners about these changes.
    EnsureNextIteration();
  }
}

void
MediaStreamGraphImpl::RecomputeBlockingAt(GraphTime aTime,
                                          GraphTime aEndBlockingDecisions,
                                          GraphTime* aEnd)
{
  for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
    MediaStream* stream = mStreams[i];
    stream->mBlocked.SetAtAndAfter(aTime, false);
  }

  for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
    MediaStream* stream = mStreams[i];
    // Stream might be blocked by some other stream (due to processing
    // constraints)
    if (stream->mBlocked.GetAt(aTime)) {
      continue;
    }

    if (stream->mFinished) {
      GraphTime endTime = StreamTimeToGraphTime(stream, stream->GetBufferEnd());
      if (endTime <= aTime) {
        LOG(PR_LOG_DEBUG, ("MediaStream %p is blocked due to being finished", stream));
        MarkStreamBlocked(stream, aTime, aEnd);
        continue;
      } else {
        LOG(PR_LOG_DEBUG, ("MediaStream %p is finished, but not blocked yet (end at %f, with blocking at %f)",
                           stream, MediaTimeToSeconds(stream->GetBufferEnd()),
                           MediaTimeToSeconds(endTime)));
        *aEnd = NS_MIN(*aEnd, endTime);
      }
    }

    // We don't need to explicitly check for cycles; streams in a cycle will
    // just never be able to produce data, and WillUnderrun will trigger.
    GraphTime end;
    bool explicitBlock = stream->mExplicitBlockerCount.GetAt(aTime, &end) > 0;
    *aEnd = NS_MIN(*aEnd, end);
    if (explicitBlock) {
      LOG(PR_LOG_DEBUG, ("MediaStream %p is blocked due to explicit blocker", stream));
      MarkStreamBlocked(stream, aTime, aEnd);
      continue;
    }

    bool underrun = WillUnderrun(stream, aTime, aEndBlockingDecisions, aEnd);
    if (underrun) {
      MarkStreamBlocked(stream, aTime, aEnd);
      continue;
    }

    if (stream->mAudioOutputs.IsEmpty() && stream->mVideoOutputs.IsEmpty()) {
      // See if the stream is being consumed anywhere. If not, it should block.
      LOG(PR_LOG_DEBUG, ("MediaStream %p is blocked due to having no consumers", stream));
      MarkStreamBlocked(stream, aTime, aEnd);
      continue;
    }
  }

  NS_ASSERTION(*aEnd > aTime, "Failed to advance!");
}

void
MediaStreamGraphImpl::UpdateFirstActiveTracks(MediaStream* aStream)
{
  StreamBuffer::Track* newTracksByType[MediaSegment::TYPE_COUNT];
  for (PRUint32 i = 0; i < ArrayLength(newTracksByType); ++i) {
    newTracksByType[i] = nsnull;
  }

  for (StreamBuffer::TrackIter iter(aStream->mBuffer);
       !iter.IsEnded(); iter.Next()) {
    MediaSegment::Type type = iter->GetType();
    if ((newTracksByType[type] &&
         iter->GetStartTimeRoundDown() < newTracksByType[type]->GetStartTimeRoundDown()) ||
         aStream->mFirstActiveTracks[type] == TRACK_NONE) {
      newTracksByType[type] = &(*iter);
      aStream->mFirstActiveTracks[type] = iter->GetID();
    }
  }
}

void
MediaStreamGraphImpl::CreateOrDestroyAudioStream(GraphTime aAudioOutputStartTime,
                                                 MediaStream* aStream)
{
  StreamBuffer::Track* track;

  if (aStream->mAudioOutputs.IsEmpty() ||
      !(track = aStream->mBuffer.FindTrack(aStream->mFirstActiveTracks[MediaSegment::AUDIO]))) {
    if (aStream->mAudioOutput) {
      aStream->mAudioOutput->Shutdown();
      aStream->mAudioOutput = nsnull;
    }
    return;
  }

  if (aStream->mAudioOutput)
    return;

  // No output stream created yet. Check if it's time to create one.
  GraphTime startTime =
    StreamTimeToGraphTime(aStream, track->GetStartTimeRoundDown(),
                          INCLUDE_TRAILING_BLOCKED_INTERVAL);
  if (startTime >= mBlockingDecisionsMadeUntilTime) {
    // The stream wants to play audio, but nothing will play for the forseeable
    // future, so don't create the stream.
    return;
  }

  // Don't bother destroying the nsAudioStream for ended tracks yet.

  // XXX allocating a nsAudioStream could be slow so we're going to have to do
  // something here ... preallocation, async allocation, multiplexing onto a single
  // stream ...

  AudioSegment* audio = track->Get<AudioSegment>();
  aStream->mAudioPlaybackStartTime = aAudioOutputStartTime;
  aStream->mAudioOutput = nsAudioStream::AllocateStream();
  aStream->mAudioOutput->Init(audio->GetChannels(),
                              track->GetRate(),
                              audio->GetFirstFrameFormat());
}

void
MediaStreamGraphImpl::PlayAudio(MediaStream* aStream,
                                GraphTime aFrom, GraphTime aTo)
{
  if (!aStream->mAudioOutput)
    return;

  StreamBuffer::Track* track =
    aStream->mBuffer.FindTrack(aStream->mFirstActiveTracks[MediaSegment::AUDIO]);
  AudioSegment* audio = track->Get<AudioSegment>();

  // When we're playing multiple copies of this stream at the same time, they're
  // perfectly correlated so adding volumes is the right thing to do.
  float volume = 0.0f;
  for (PRUint32 i = 0; i < aStream->mAudioOutputs.Length(); ++i) {
    volume += aStream->mAudioOutputs[i].mVolume;
  }

  // We don't update aStream->mBufferStartTime here to account for
  // time spent blocked. Instead, we'll update it in UpdateCurrentTime after the
  // blocked period has completed. But we do need to make sure we play from the
  // right offsets in the stream buffer, even if we've already written silence for
  // some amount of blocked time after the current time.
  GraphTime t = aFrom;
  while (t < aTo) {
    GraphTime end;
    bool blocked = aStream->mBlocked.GetAt(t, &end);
    end = NS_MIN(end, aTo);

    AudioSegment output;
    if (blocked) {
      // Track total blocked time in aStream->mBlockedAudioTime so that
      // the amount of silent samples we've inserted for blocking never gets
      // more than one sample away from the ideal amount.
      TrackTicks startTicks =
          TimeToTicksRoundDown(track->GetRate(), aStream->mBlockedAudioTime);
      aStream->mBlockedAudioTime += end - t;
      TrackTicks endTicks =
          TimeToTicksRoundDown(track->GetRate(), aStream->mBlockedAudioTime);

      output.InitFrom(*audio);
      output.InsertNullDataAtStart(endTicks - startTicks);
      LOG(PR_LOG_DEBUG, ("MediaStream %p writing blocking-silence samples for %f to %f",
                         aStream, MediaTimeToSeconds(t), MediaTimeToSeconds(end)));
    } else {
      TrackTicks startTicks =
          track->TimeToTicksRoundDown(GraphTimeToStreamTime(aStream, t));
      TrackTicks endTicks =
          track->TimeToTicksRoundDown(GraphTimeToStreamTime(aStream, end));

      output.SliceFrom(*audio, startTicks, endTicks);
      output.ApplyVolume(volume);
      LOG(PR_LOG_DEBUG, ("MediaStream %p writing samples for %f to %f (samples %lld to %lld)",
                         aStream, MediaTimeToSeconds(t), MediaTimeToSeconds(end),
                         startTicks, endTicks));
    }
    output.WriteTo(aStream->mAudioOutput);
    t = end;
  }
}

void
MediaStreamGraphImpl::PlayVideo(MediaStream* aStream)
{
  if (aStream->mVideoOutputs.IsEmpty())
    return;

  StreamBuffer::Track* track =
    aStream->mBuffer.FindTrack(aStream->mFirstActiveTracks[MediaSegment::VIDEO]);
  if (!track)
    return;
  VideoSegment* video = track->Get<VideoSegment>();

  // Display the next frame a bit early. This is better than letting the current
  // frame be displayed for too long.
  GraphTime framePosition = mCurrentTime + MEDIA_GRAPH_TARGET_PERIOD_MS;
  NS_ASSERTION(framePosition >= aStream->mBufferStartTime, "frame position before buffer?");
  StreamTime frameBufferTime = GraphTimeToStreamTime(aStream, framePosition);
  TrackTicks start;
  const VideoFrame* frame =
    video->GetFrameAt(track->TimeToTicksRoundDown(frameBufferTime), &start);
  if (!frame) {
    frame = video->GetLastFrame(&start);
    if (!frame)
      return;
  }

  if (*frame != aStream->mLastPlayedVideoFrame) {
    LOG(PR_LOG_DEBUG, ("MediaStream %p writing video frame %p (%dx%d)",
                       aStream, frame->GetImage(), frame->GetIntrinsicSize().width,
                       frame->GetIntrinsicSize().height));
    GraphTime startTime = StreamTimeToGraphTime(aStream,
        track->TicksToTimeRoundDown(start), INCLUDE_TRAILING_BLOCKED_INTERVAL);
    TimeStamp targetTime = mCurrentTimeStamp +
        TimeDuration::FromMilliseconds(double(startTime - mCurrentTime));
    for (PRUint32 i = 0; i < aStream->mVideoOutputs.Length(); ++i) {
      VideoFrameContainer* output = aStream->mVideoOutputs[i];
      output->SetCurrentFrame(frame->GetIntrinsicSize(), frame->GetImage(),
                              targetTime);
      nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethod(output, &VideoFrameContainer::Invalidate);
      NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
    }
    aStream->mLastPlayedVideoFrame = *frame;
  }
}

void
MediaStreamGraphImpl::PrepareUpdatesToMainThreadState()
{
  mMonitor.AssertCurrentThreadOwns();

  for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
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
    for (PRUint32 i = 0; i < messageQueue.Length(); ++i) {
      mProcessingGraphUpdateIndex = messageQueue[i].mGraphUpdateIndex;
      nsTArray<nsAutoPtr<ControlMessage> >& messages = messageQueue[i].mMessages;

      for (PRUint32 j = 0; j < mStreams.Length(); ++j) {
        mStreams[j]->mMessageAffectedTime = GRAPH_TIME_MAX;
      }
      for (PRUint32 j = 0; j < messages.Length(); ++j) {
        messages[j]->UpdateAffectedStream();
      }

      ChooseActionTime();

      for (PRUint32 j = 0; j < messages.Length(); ++j) {
        messages[j]->Process();
      }
    }
    messageQueue.Clear();

    // Grab pending ProcessingEngine results.
    for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
      SourceMediaStream* is = mStreams[i]->AsSourceStream();
      if (is) {
        UpdateConsumptionState(is);
        ExtractPendingInput(is);
      }
    }

    GraphTime prevBlockingDecisionsMadeUntilTime = mBlockingDecisionsMadeUntilTime;
    RecomputeBlocking();

    PRUint32 audioStreamsActive = 0;
    bool allBlockedForever = true;
    // Figure out what each stream wants to do
    for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
      MediaStream* stream = mStreams[i];
      UpdateFirstActiveTracks(stream);
      CreateOrDestroyAudioStream(prevBlockingDecisionsMadeUntilTime, stream);
      PlayAudio(stream, prevBlockingDecisionsMadeUntilTime,
                mBlockingDecisionsMadeUntilTime);
      if (stream->mAudioOutput) {
        ++audioStreamsActive;
      }
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
    if (!allBlockedForever || audioStreamsActive > 0) {
      EnsureNextIteration();
    }

    {
      MonitorAutoLock lock(mMonitor);
      PrepareUpdatesToMainThreadState();
      if (mForceShutDown || (IsEmpty() && mMessageQueue.IsEmpty())) {
        // Enter shutdown mode. The stable-state handler will detect this
        // and complete shutdown. Destroy any streams immediately.
        LOG(PR_LOG_DEBUG, ("MediaStreamGraph %p waiting for main thread cleanup", this));
        mLifecycleState = LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP;
        {
          MonitorAutoUnlock unlock(mMonitor);
          // Unlock mMonitor while destroying our streams, since
          // SourceMediaStream::DestroyImpl needs to take its lock while
          // we're not holding mMonitor.
          for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
            mStreams[i]->DestroyImpl();
          }
        }
        return;
      }

      PRIntervalTime timeout = PR_INTERVAL_NO_TIMEOUT;
      TimeStamp now = TimeStamp::Now();
      if (mNeedAnotherIteration) {
        PRInt64 timeoutMS = MEDIA_GRAPH_TARGET_PERIOD_MS -
          PRInt64((now - mCurrentTimeStamp).ToMilliseconds());
        // Make sure timeoutMS doesn't overflow 32 bits by waking up at
        // least once a minute, if we need to wake up at all
        timeoutMS = NS_MAX<PRInt64>(0, NS_MIN<PRInt64>(timeoutMS, 60*1000));
        timeout = PR_MillisecondsToInterval(PRUint32(timeoutMS));
        LOG(PR_LOG_DEBUG, ("Waiting for next iteration; at %f, timeout=%f",
                           (now - mInitialTimeStamp).ToSeconds(), timeoutMS/1000.0));
        mWaitState = WAITSTATE_WAITING_FOR_NEXT_ITERATION;
      } else {
        mWaitState = WAITSTATE_WAITING_INDEFINITELY;
      }
      if (timeout > 0) {
        lock.Wait(timeout);
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
}

void
MediaStreamGraphImpl::ShutdownThreads()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called on main thread");
  // mGraph's thread is not running so it's OK to do whatever here
  LOG(PR_LOG_DEBUG, ("Stopping threads for MediaStreamGraph %p", this));

  if (mThread) {
    mThread->Shutdown();
    mThread = nsnull;
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
  virtual void UpdateAffectedStream()
  {
    mStream->GraphImpl()->AddStream(mStream);
  }
  virtual void Process()
  {
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

  {
    MonitorAutoLock lock(mMonitor);
    mPostedRunInStableStateEvent = false;

    runnables.SwapElements(mUpdateRunnables);
    for (PRUint32 i = 0; i < mStreamUpdates.Length(); ++i) {
      StreamUpdate* update = &mStreamUpdates[i];
      if (update->mStream) {
        ApplyStreamUpdate(update);
      }
    }
    mStreamUpdates.Clear();

    if (mLifecycleState == LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP && mForceShutDown) {
      for (PRUint32 i = 0; i < mMessageQueue.Length(); ++i) {
        MessageBlock& mb = mMessageQueue[i];
        for (PRUint32 j = 0; j < mb.mMessages.Length(); ++j) {
          mb.mMessages[j]->ProcessDuringShutdown();
        }
      }
      mMessageQueue.Clear();
      for (PRUint32 i = 0; i < mCurrentTaskMessageQueue.Length(); ++i) {
        mCurrentTaskMessageQueue[i]->ProcessDuringShutdown();
      }
      mCurrentTaskMessageQueue.Clear();
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
        gGraph = nsnull;
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

  for (PRUint32 i = 0; i < runnables.Length(); ++i) {
    runnables[i]->Run();
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

  if (mDetectedNotRunning &&
      mLifecycleState > LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP) {
    // The graph control loop is not running and main thread cleanup has
    // happened. From now on we can't append messages to mCurrentTaskMessageQueue,
    // because that will never be processed again, so just ProcessDuringShutdown
    // this message.
    // This should only happen during forced shutdown.
    aMessage->ProcessDuringShutdown();
    delete aMessage;
    if (IsEmpty()) {
      NS_ASSERTION(gGraph == this, "Switched managers during forced shutdown?");
      gGraph = nsnull;
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
  mExplicitBlockerCount.SetAtAndAfter(graph->mLastActionTime, false);
}

MediaStreamGraphImpl*
MediaStream::GraphImpl()
{
  return gGraph;
}

void
MediaStream::DestroyImpl()
{
  if (mAudioOutput) {
    mAudioOutput->Shutdown();
    mAudioOutput = nsnull;
  }
}

void
MediaStream::Destroy()
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream) : ControlMessage(aStream) {}
    virtual void UpdateAffectedStream()
    {
      mStream->DestroyImpl();
      mStream->GraphImpl()->RemoveStream(mStream);
    }
    virtual void ProcessDuringShutdown()
    { UpdateAffectedStream(); }
  };
  mWrapper = nsnull;
  GraphImpl()->AppendMessage(new Message(this));
}

void
MediaStream::AddAudioOutput(void* aKey)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, void* aKey) : ControlMessage(aStream), mKey(aKey) {}
    virtual void UpdateAffectedStream()
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
  for (PRUint32 i = 0; i < mAudioOutputs.Length(); ++i) {
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
    virtual void UpdateAffectedStream()
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
  for (PRUint32 i = 0; i < mAudioOutputs.Length(); ++i) {
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
    virtual void UpdateAffectedStream()
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
    virtual void UpdateAffectedStream()
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
    virtual void UpdateAffectedStream()
    {
      mStream->RemoveVideoOutputImpl(mContainer);
    }
    nsRefPtr<VideoFrameContainer> mContainer;
  };
  GraphImpl()->AppendMessage(new Message(this, aContainer));
}

void
MediaStream::ChangeExplicitBlockerCount(PRInt32 aDelta)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, PRInt32 aDelta) :
      ControlMessage(aStream), mDelta(aDelta) {}
    virtual void UpdateAffectedStream()
    {
      mStream->ChangeExplicitBlockerCountImpl(
          mStream->GraphImpl()->mLastActionTime, mDelta);
    }
    PRInt32 mDelta;
  };
  GraphImpl()->AppendMessage(new Message(this, aDelta));
}

void
MediaStream::AddListenerImpl(already_AddRefed<MediaStreamListener> aListener)
{
  MediaStreamListener* listener = *mListeners.AppendElement() = aListener;
  listener->NotifyBlockingChanged(GraphImpl(),
    mBlocked.GetAt(GraphImpl()->mCurrentTime) ? MediaStreamListener::BLOCKED : MediaStreamListener::UNBLOCKED);
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
    virtual void UpdateAffectedStream()
    {
      mStream->AddListenerImpl(mListener.forget());
    }
    nsRefPtr<MediaStreamListener> mListener;
  };
  GraphImpl()->AppendMessage(new Message(this, aListener));
}

void
MediaStream::RemoveListener(MediaStreamListener* aListener)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, MediaStreamListener* aListener) :
      ControlMessage(aStream), mListener(aListener) {}
    virtual void UpdateAffectedStream()
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
  TrackData *track = FindDataForTrack(aID);
  if (track) {
    track->mData->AppendFrom(aSegment);
  } else {
    NS_ERROR("Append to non-existent track!");
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
  TrackData *track = FindDataForTrack(aID);
  if (track) {
    track->mCommands |= TRACK_END;
  } else {
    NS_ERROR("End of non-existant track");
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
SourceMediaStream::Finish()
{
  MutexAutoLock lock(mMutex);
  mUpdateFinished = true;
  if (!mDestroyed) {
    GraphImpl()->EnsureNextIteration();
  }
}

static const PRUint32 kThreadLimit = 4;
static const PRUint32 kIdleThreadLimit = 4;
static const PRUint32 kIdleThreadTimeoutMs = 2000;

/**
 * We make the initial mCurrentTime nonzero so that zero times can have
 * special meaning if necessary.
 */
static const PRInt32 INITIAL_CURRENT_TIME = 1;

MediaStreamGraphImpl::MediaStreamGraphImpl()
  : mLastActionTime(INITIAL_CURRENT_TIME)
  , mCurrentTime(INITIAL_CURRENT_TIME)
  , mBlockingDecisionsMadeUntilTime(1)
  , mProcessingGraphUpdateIndex(0)
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
MediaStreamGraph::CreateInputStream(nsDOMMediaStream* aWrapper)
{
  SourceMediaStream* stream = new SourceMediaStream(aWrapper);
  NS_ADDREF(stream);
  static_cast<MediaStreamGraphImpl*>(this)->AppendMessage(new CreateMessage(stream));
  return stream;
}

}
