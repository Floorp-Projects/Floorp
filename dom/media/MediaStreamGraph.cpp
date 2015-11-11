/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamGraphImpl.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/unused.h"

#include "AudioSegment.h"
#include "VideoSegment.h"
#include "nsContentUtils.h"
#include "nsIObserver.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "prerror.h"
#include "mozilla/Logging.h"
#include "mozilla/Attributes.h"
#include "TrackUnionStream.h"
#include "ImageContainer.h"
#include "AudioCaptureStream.h"
#include "AudioChannelService.h"
#include "AudioNodeStream.h"
#include "AudioNodeExternalInputStream.h"
#include "mozilla/dom/AudioContextBinding.h"
#include <algorithm>
#include "DOMMediaStream.h"
#include "GeckoProfiler.h"
#include "mozilla/unused.h"
#ifdef MOZ_WEBRTC
#include "AudioOutputObserver.h"
#endif

#include "webaudio/blink/HRTFDatabaseLoader.h"

using namespace mozilla::layers;
using namespace mozilla::dom;
using namespace mozilla::gfx;

namespace mozilla {

LazyLogModule gMediaStreamGraphLog("MediaStreamGraph");
#define STREAM_LOG(type, msg) MOZ_LOG(gMediaStreamGraphLog, type, msg)

// #define ENABLE_LIFECYCLE_LOG

// We don't use NSPR log here because we want this interleaved with adb logcat
// on Android/B2G
#ifdef ENABLE_LIFECYCLE_LOG
#  ifdef ANDROID
#    include "android/log.h"
#    define LIFECYCLE_LOG(...)  __android_log_print(ANDROID_LOG_INFO, "Gecko - MSG", ## __VA_ARGS__); printf(__VA_ARGS__);printf("\n");
#  else
#    define LIFECYCLE_LOG(...) printf(__VA_ARGS__);printf("\n");
#  endif
#else
#  define LIFECYCLE_LOG(...)
#endif

/**
 * A hash table containing the graph instances, one per AudioChannel.
 */
static nsDataHashtable<nsUint32HashKey, MediaStreamGraphImpl*> gGraphs;

MediaStreamGraphImpl::~MediaStreamGraphImpl()
{
  NS_ASSERTION(IsEmpty(),
               "All streams should have been destroyed by messages from the main thread");
  STREAM_LOG(LogLevel::Debug, ("MediaStreamGraph %p destroyed", this));
  LIFECYCLE_LOG("MediaStreamGraphImpl::~MediaStreamGraphImpl\n");
}

void
MediaStreamGraphImpl::FinishStream(MediaStream* aStream)
{
  if (aStream->mFinished)
    return;
  STREAM_LOG(LogLevel::Debug, ("MediaStream %p will finish", aStream));
  aStream->mFinished = true;
  aStream->mBuffer.AdvanceKnownTracksTime(STREAM_TIME_MAX);

  SetStreamOrderDirty();
}

void
MediaStreamGraphImpl::AddStreamGraphThread(MediaStream* aStream)
{
  aStream->mBufferStartTime = mProcessedTime;
  if (aStream->IsSuspended()) {
    mSuspendedStreams.AppendElement(aStream);
    STREAM_LOG(LogLevel::Debug, ("Adding media stream %p to the graph, in the suspended stream array", aStream));
  } else {
    mStreams.AppendElement(aStream);
    STREAM_LOG(LogLevel::Debug, ("Adding media stream %p to the graph", aStream));
  }

  SetStreamOrderDirty();
}

void
MediaStreamGraphImpl::RemoveStreamGraphThread(MediaStream* aStream)
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

  // Ensure that mFirstCycleBreaker and mMixer are updated when necessary.
  SetStreamOrderDirty();

  if (aStream->IsSuspended()) {
    mSuspendedStreams.RemoveElement(aStream);
  } else {
    mStreams.RemoveElement(aStream);
  }

  NS_RELEASE(aStream); // probably destroying it

  STREAM_LOG(LogLevel::Debug, ("Removing media stream %p from the graph", aStream));
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
      // the stream at all.
      StreamTime t = aStream->GraphTimeToStreamTime(aDesiredUpToTime);
      STREAM_LOG(LogLevel::Verbose, ("Calling NotifyPull aStream=%p t=%f current end=%f", aStream,
                                  MediaTimeToSeconds(t),
                                  MediaTimeToSeconds(aStream->mBuffer.GetEnd())));
      if (t > aStream->mBuffer.GetEnd()) {
        *aEnsureNextIteration = true;
#ifdef DEBUG
        if (aStream->mListeners.Length() == 0) {
          STREAM_LOG(LogLevel::Error, ("No listeners in NotifyPull aStream=%p desired=%f current end=%f",
                                    aStream, MediaTimeToSeconds(t),
                                    MediaTimeToSeconds(aStream->mBuffer.GetEnd())));
          aStream->DumpTrackInfo();
        }
#endif
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
    bool notifiedTrackCreated = false;
    for (int32_t i = aStream->mUpdateTracks.Length() - 1; i >= 0; --i) {
      SourceMediaStream::TrackData* data = &aStream->mUpdateTracks[i];
      aStream->ApplyTrackDisabling(data->mID, data->mData);
      for (MediaStreamListener* l : aStream->mListeners) {
        StreamTime offset = (data->mCommands & SourceMediaStream::TRACK_CREATE)
            ? data->mStart : aStream->mBuffer.FindTrack(data->mID)->GetSegment()->GetDuration();
        l->NotifyQueuedTrackChanges(this, data->mID,
                                    offset, data->mCommands, *data->mData);
      }
      if (data->mCommands & SourceMediaStream::TRACK_CREATE) {
        MediaSegment* segment = data->mData.forget();
        STREAM_LOG(LogLevel::Debug, ("SourceMediaStream %p creating track %d, start %lld, initial end %lld",
                                  aStream, data->mID, int64_t(data->mStart),
                                  int64_t(segment->GetDuration())));

        data->mEndOfFlushedData += segment->GetDuration();
        aStream->mBuffer.AddTrack(data->mID, data->mStart, segment);
        // The track has taken ownership of data->mData, so let's replace
        // data->mData with an empty clone.
        data->mData = segment->CreateEmptyClone();
        data->mCommands &= ~SourceMediaStream::TRACK_CREATE;
        notifiedTrackCreated = true;
      } else if (data->mData->GetDuration() > 0) {
        MediaSegment* dest = aStream->mBuffer.FindTrack(data->mID)->GetSegment();
        STREAM_LOG(LogLevel::Verbose, ("SourceMediaStream %p track %d, advancing end from %lld to %lld",
                                    aStream, data->mID,
                                    int64_t(dest->GetDuration()),
                                    int64_t(dest->GetDuration() + data->mData->GetDuration())));
        data->mEndOfFlushedData += data->mData->GetDuration();
        dest->AppendFrom(data->mData);
      }
      if (data->mCommands & SourceMediaStream::TRACK_END) {
        aStream->mBuffer.FindTrack(data->mID)->SetEnded();
        aStream->mUpdateTracks.RemoveElementAt(i);
      }
    }
    if (notifiedTrackCreated) {
      for (MediaStreamListener* l : aStream->mListeners) {
        l->NotifyFinishedTrackCreation(this);
      }
    }
    if (!aStream->mFinished) {
      aStream->mBuffer.AdvanceKnownTracksTime(aStream->mUpdateKnownTracksTime);
    }
  }
  if (aStream->mBuffer.GetEnd() > 0) {
    aStream->mHasCurrentData = true;
  }
  if (finished) {
    FinishStream(aStream);
  }
}

StreamTime
MediaStreamGraphImpl::GraphTimeToStreamTimeWithBlocking(MediaStream* aStream,
                                                        GraphTime aTime)
{
  MOZ_ASSERT(aTime <= mStateComputedTime,
             "Don't ask about times where we haven't made blocking decisions yet");
  return std::max<StreamTime>(0,
      std::min(aTime, aStream->mStartBlocking) - aStream->mBufferStartTime);
}

GraphTime
MediaStreamGraphImpl::IterationEnd() const
{
  return CurrentDriver()->IterationEnd();
}

void
MediaStreamGraphImpl::UpdateCurrentTimeForStreams(GraphTime aPrevCurrentTime)
{
  for (MediaStream* stream : AllStreams()) {
    bool isAnyBlocked = stream->mStartBlocking < mStateComputedTime;
    bool isAnyUnblocked = stream->mStartBlocking > aPrevCurrentTime;

    // Calculate blocked time and fire Blocked/Unblocked events
    GraphTime blockedTime = mStateComputedTime - stream->mStartBlocking;
    NS_ASSERTION(blockedTime >= 0, "Error in blocking time");
    stream->AdvanceTimeVaryingValuesToCurrentTime(mStateComputedTime,
                                                  blockedTime);
    STREAM_LOG(LogLevel::Verbose,
               ("MediaStream %p bufferStartTime=%f blockedTime=%f", stream,
                MediaTimeToSeconds(stream->mBufferStartTime),
                MediaTimeToSeconds(blockedTime)));
    stream->mStartBlocking = mStateComputedTime;

    if (isAnyUnblocked && stream->mNotifiedBlocked) {
      for (uint32_t j = 0; j < stream->mListeners.Length(); ++j) {
        MediaStreamListener* l = stream->mListeners[j];
        l->NotifyBlockingChanged(this, MediaStreamListener::UNBLOCKED);
      }
      stream->mNotifiedBlocked = false;
    }
    if (isAnyBlocked && !stream->mNotifiedBlocked) {
      for (uint32_t j = 0; j < stream->mListeners.Length(); ++j) {
        MediaStreamListener* l = stream->mListeners[j];
        l->NotifyBlockingChanged(this, MediaStreamListener::BLOCKED);
      }
      stream->mNotifiedBlocked = true;
    }

    if (isAnyUnblocked) {
      NS_ASSERTION(!stream->mNotifiedFinished,
        "Shouldn't have already notified of finish *and* have output!");
      for (uint32_t j = 0; j < stream->mListeners.Length(); ++j) {
        MediaStreamListener* l = stream->mListeners[j];
        l->NotifyOutput(this, mProcessedTime);
      }
    }

    // The stream is fully finished when all of its track data has been played
    // out.
    if (stream->mFinished && !stream->mNotifiedFinished &&
        mProcessedTime >=
          stream->StreamTimeToGraphTime(stream->GetStreamBuffer().GetAllTracksEnd())) {
      stream->mNotifiedFinished = true;
      SetStreamOrderDirty();
      for (uint32_t j = 0; j < stream->mListeners.Length(); ++j) {
        MediaStreamListener* l = stream->mListeners[j];
        l->NotifyEvent(this, MediaStreamListener::EVENT_FINISHED);
      }
    }
  }
}

GraphTime
MediaStreamGraphImpl::WillUnderrun(MediaStream* aStream,
                                   GraphTime aEndBlockingDecisions)
{
  // Finished streams can't underrun. ProcessedMediaStreams also can't cause
  // underrun currently, since we'll always be able to produce data for them
  // unless they block on some other stream.
  if (aStream->mFinished || aStream->AsProcessedStream()) {
    return aEndBlockingDecisions;
  }
  // This stream isn't finished or suspended. We don't need to call
  // StreamTimeToGraphTime since an underrun is the only thing that can block
  // it.
  GraphTime bufferEnd = aStream->GetBufferEnd() + aStream->mBufferStartTime;
#ifdef DEBUG
  if (bufferEnd < mProcessedTime) {
    STREAM_LOG(LogLevel::Error, ("MediaStream %p underrun, "
                              "bufferEnd %f < mProcessedTime %f (%lld < %lld), Streamtime %lld",
                              aStream, MediaTimeToSeconds(bufferEnd), MediaTimeToSeconds(mProcessedTime),
                              bufferEnd, mProcessedTime, aStream->GetBufferEnd()));
    aStream->DumpTrackInfo();
    NS_ASSERTION(bufferEnd >= mProcessedTime, "Buffer underran");
  }
#endif
  return std::min(bufferEnd, aEndBlockingDecisions);
}

namespace {
  // Value of mCycleMarker for unvisited streams in cycle detection.
  const uint32_t NOT_VISITED = UINT32_MAX;
  // Value of mCycleMarker for ordered streams in muted cycles.
  const uint32_t IN_MUTED_CYCLE = 1;
} // namespace

void
MediaStreamGraphImpl::UpdateStreamOrder()
{
#ifdef MOZ_WEBRTC
  bool shouldAEC = false;
#endif
  bool audioTrackPresent = false;
  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaStream* stream = mStreams[i];
#ifdef MOZ_WEBRTC
    if (stream->AsSourceStream() &&
        stream->AsSourceStream()->NeedsMixing()) {
      shouldAEC = true;
    }
#endif
    // If this is a AudioNodeStream, force a AudioCallbackDriver.
    if (stream->AsAudioNodeStream()) {
      audioTrackPresent = true;
    } else {
      for (StreamBuffer::TrackIter tracks(stream->GetStreamBuffer(), MediaSegment::AUDIO);
           !tracks.IsEnded(); tracks.Next()) {
        audioTrackPresent = true;
      }
    }
  }

  if (!audioTrackPresent && mRealtime &&
      CurrentDriver()->AsAudioCallbackDriver()) {
    MonitorAutoLock mon(mMonitor);
    if (CurrentDriver()->AsAudioCallbackDriver()->IsStarted()) {
      if (mLifecycleState == LIFECYCLE_RUNNING) {
        SystemClockDriver* driver = new SystemClockDriver(this);
        mMixer.RemoveCallback(CurrentDriver()->AsAudioCallbackDriver());
        CurrentDriver()->SwitchAtNextIteration(driver);
      }
    }
  }

  if (audioTrackPresent && mRealtime &&
      !CurrentDriver()->AsAudioCallbackDriver() &&
      !CurrentDriver()->Switching()) {
    MonitorAutoLock mon(mMonitor);
    if (mLifecycleState == LIFECYCLE_RUNNING) {
      AudioCallbackDriver* driver = new AudioCallbackDriver(this);
      mMixer.AddCallback(driver);
      CurrentDriver()->SwitchAtNextIteration(driver);
    }
  }

#ifdef MOZ_WEBRTC
  if (shouldAEC && !mFarendObserverRef && gFarendObserver) {
    mFarendObserverRef = gFarendObserver;
    mMixer.AddCallback(mFarendObserverRef);
  } else if (!shouldAEC && mFarendObserverRef){
    if (mMixer.FindCallback(mFarendObserverRef)) {
      mMixer.RemoveCallback(mFarendObserverRef);
      mFarendObserverRef = nullptr;
    }
  }
#endif

  if (!mStreamOrderDirty) {
    return;
  }

  mStreamOrderDirty = false;

  // The algorithm for finding cycles is based on Tim Leslie's iterative
  // implementation [1][2] of Pearce's variant [3] of Tarjan's strongly
  // connected components (SCC) algorithm.  There are variations (a) to
  // distinguish whether streams in SCCs of size 1 are in a cycle and (b) to
  // re-run the algorithm over SCCs with breaks at DelayNodes.
  //
  // [1] http://www.timl.id.au/?p=327
  // [2] https://github.com/scipy/scipy/blob/e2c502fca/scipy/sparse/csgraph/_traversal.pyx#L582
  // [3] http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.102.1707
  //
  // There are two stacks.  One for the depth-first search (DFS),
  mozilla::LinkedList<MediaStream> dfsStack;
  // and another for streams popped from the DFS stack, but still being
  // considered as part of SCCs involving streams on the stack.
  mozilla::LinkedList<MediaStream> sccStack;

  // An index into mStreams for the next stream found with no unsatisfied
  // upstream dependencies.
  uint32_t orderedStreamCount = 0;

  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaStream* s = mStreams[i];
    ProcessedMediaStream* ps = s->AsProcessedStream();
    if (ps) {
      // The dfsStack initially contains a list of all processed streams in
      // unchanged order.
      dfsStack.insertBack(s);
      ps->mCycleMarker = NOT_VISITED;
    } else {
      // SourceMediaStreams have no inputs and so can be ordered now.
      mStreams[orderedStreamCount] = s;
      ++orderedStreamCount;
    }
  }

  // mNextStackMarker corresponds to "index" in Tarjan's algorithm.  It is a
  // counter to label mCycleMarker on the next visited stream in the DFS
  // uniquely in the set of visited streams that are still being considered.
  //
  // In this implementation, the counter descends so that the values are
  // strictly greater than the values that mCycleMarker takes when the stream
  // has been ordered (0 or IN_MUTED_CYCLE).
  //
  // Each new stream labelled, as the DFS searches upstream, receives a value
  // less than those used for all other streams being considered.
  uint32_t nextStackMarker = NOT_VISITED - 1;
  // Reset list of DelayNodes in cycles stored at the tail of mStreams.
  mFirstCycleBreaker = mStreams.Length();

  // Rearrange dfsStack order as required to DFS upstream and pop streams
  // in processing order to place in mStreams.
  while (auto ps = static_cast<ProcessedMediaStream*>(dfsStack.getFirst())) {
    const auto& inputs = ps->mInputs;
    MOZ_ASSERT(ps->AsProcessedStream());
    if (ps->mCycleMarker == NOT_VISITED) {
      // Record the position on the visited stack, so that any searches
      // finding this stream again know how much of the stack is in the cycle.
      ps->mCycleMarker = nextStackMarker;
      --nextStackMarker;
      // Not-visited input streams should be processed first.
      // SourceMediaStreams have already been ordered.
      for (uint32_t i = inputs.Length(); i--; ) {
        if (inputs[i]->mSource->IsSuspended()) {
          continue;
        }
        auto input = inputs[i]->mSource->AsProcessedStream();
        if (input && input->mCycleMarker == NOT_VISITED) {
          // It can be that this stream has an input which is from a suspended
          // AudioContext.
          if (input->isInList()) {
            input->remove();
            dfsStack.insertFront(input);
          }
        }
      }
      continue;
    }

    // Returning from DFS.  Pop from dfsStack.
    ps->remove();

    // cycleStackMarker keeps track of the highest marker value on any
    // upstream stream, if any, found receiving input, directly or indirectly,
    // from the visited stack (and so from |ps|, making a cycle).  In a
    // variation from Tarjan's SCC algorithm, this does not include |ps|
    // unless it is part of the cycle.
    uint32_t cycleStackMarker = 0;
    for (uint32_t i = inputs.Length(); i--; ) {
      if (inputs[i]->mSource->IsSuspended()) {
        continue;
      }
      auto input = inputs[i]->mSource->AsProcessedStream();
      if (input) {
        cycleStackMarker = std::max(cycleStackMarker, input->mCycleMarker);
      }
    }

    if (cycleStackMarker <= IN_MUTED_CYCLE) {
      // All inputs have been ordered and their stack markers have been removed.
      // This stream is not part of a cycle.  It can be processed next.
      ps->mCycleMarker = 0;
      mStreams[orderedStreamCount] = ps;
      ++orderedStreamCount;
      continue;
    }

    // A cycle has been found.  Record this stream for ordering when all
    // streams in this SCC have been popped from the DFS stack.
    sccStack.insertFront(ps);

    if (cycleStackMarker > ps->mCycleMarker) {
      // Cycles have been found that involve streams that remain on the stack.
      // Leave mCycleMarker indicating the most downstream (last) stream on
      // the stack known to be part of this SCC.  In this way, any searches on
      // other paths that find |ps| will know (without having to traverse from
      // this stream again) that they are part of this SCC (i.e. part of an
      // intersecting cycle).
      ps->mCycleMarker = cycleStackMarker;
      continue;
    }

    // |ps| is the root of an SCC involving no other streams on dfsStack, the
    // complete SCC has been recorded, and streams in this SCC are part of at
    // least one cycle.
    MOZ_ASSERT(cycleStackMarker == ps->mCycleMarker);
    // If there are DelayNodes in this SCC, then they may break the cycles.
    bool haveDelayNode = false;
    auto next = sccStack.getFirst();
    // Streams in this SCC are identified by mCycleMarker <= cycleStackMarker.
    // (There may be other streams later in sccStack from other incompletely
    // searched SCCs, involving streams still on dfsStack.)
    //
    // DelayNodes in cycles must behave differently from those not in cycles,
    // so all DelayNodes in the SCC must be identified.
    while (next && static_cast<ProcessedMediaStream*>(next)->
           mCycleMarker <= cycleStackMarker) {
      auto ns = next->AsAudioNodeStream();
      // Get next before perhaps removing from list below.
      next = next->getNext();
      if (ns && ns->Engine()->AsDelayNodeEngine()) {
        haveDelayNode = true;
        // DelayNodes break cycles by producing their output in a
        // preprocessing phase; they do not need to be ordered before their
        // consumers.  Order them at the tail of mStreams so that they can be
        // handled specially.  Do so now, so that DFS ignores them.
        ns->remove();
        ns->mCycleMarker = 0;
        --mFirstCycleBreaker;
        mStreams[mFirstCycleBreaker] = ns;
      }
    }
    auto after_scc = next;
    while ((next = sccStack.getFirst()) != after_scc) {
      next->remove();
      auto removed = static_cast<ProcessedMediaStream*>(next);
      if (haveDelayNode) {
        // Return streams to the DFS stack again (to order and detect cycles
        // without delayNodes).  Any of these streams that are still inputs
        // for streams on the visited stack must be returned to the front of
        // the stack to be ordered before their dependents.  We know that none
        // of these streams need input from streams on the visited stack, so
        // they can all be searched and ordered before the current stack head
        // is popped.
        removed->mCycleMarker = NOT_VISITED;
        dfsStack.insertFront(removed);
      } else {
        // Streams in cycles without any DelayNodes must be muted, and so do
        // not need input and can be ordered now.  They must be ordered before
        // their consumers so that their muted output is available.
        removed->mCycleMarker = IN_MUTED_CYCLE;
        mStreams[orderedStreamCount] = removed;
        ++orderedStreamCount;
      }
    }
  }

  MOZ_ASSERT(orderedStreamCount == mFirstCycleBreaker);
}

void
MediaStreamGraphImpl::NotifyHasCurrentData(MediaStream* aStream)
{
  if (!aStream->mNotifiedHasCurrentData && aStream->mHasCurrentData) {
    for (uint32_t j = 0; j < aStream->mListeners.Length(); ++j) {
      MediaStreamListener* l = aStream->mListeners[j];
      l->NotifyHasCurrentData(this);
    }
    aStream->mNotifiedHasCurrentData = true;
  }
}

void
MediaStreamGraphImpl::CreateOrDestroyAudioStreams(MediaStream* aStream)
{
  MOZ_ASSERT(mRealtime, "Should only attempt to create audio streams in real-time mode");

  if (aStream->mAudioOutputs.IsEmpty()) {
    aStream->mAudioOutputStreams.Clear();
    return;
  }

  if (!aStream->GetStreamBuffer().GetAndResetTracksDirty() &&
      !aStream->mAudioOutputStreams.IsEmpty()) {
    return;
  }

  STREAM_LOG(LogLevel::Debug, ("Updating AudioOutputStreams for MediaStream %p", aStream));

  nsAutoTArray<bool,2> audioOutputStreamsFound;
  for (uint32_t i = 0; i < aStream->mAudioOutputStreams.Length(); ++i) {
    audioOutputStreamsFound.AppendElement(false);
  }

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
      MediaStream::AudioOutputStream* audioOutputStream =
        aStream->mAudioOutputStreams.AppendElement();
      audioOutputStream->mAudioPlaybackStartTime = mProcessedTime;
      audioOutputStream->mBlockedAudioTime = 0;
      audioOutputStream->mLastTickWritten = 0;
      audioOutputStream->mTrackID = tracks->GetID();

      if (!CurrentDriver()->AsAudioCallbackDriver() &&
          !CurrentDriver()->Switching()) {
        MonitorAutoLock mon(mMonitor);
        if (mLifecycleState == LIFECYCLE_RUNNING) {
          AudioCallbackDriver* driver = new AudioCallbackDriver(this);
          mMixer.AddCallback(driver);
          CurrentDriver()->SwitchAtNextIteration(driver);
        }
      }
    }
  }

  for (int32_t i = audioOutputStreamsFound.Length() - 1; i >= 0; --i) {
    if (!audioOutputStreamsFound[i]) {
      aStream->mAudioOutputStreams.RemoveElementAt(i);
    }
  }
}

StreamTime
MediaStreamGraphImpl::PlayAudio(MediaStream* aStream)
{
  MOZ_ASSERT(mRealtime, "Should only attempt to play audio in realtime mode");

  float volume = 0.0f;
  for (uint32_t i = 0; i < aStream->mAudioOutputs.Length(); ++i) {
    volume += aStream->mAudioOutputs[i].mVolume;
  }

  StreamTime ticksWritten = 0;

  for (uint32_t i = 0; i < aStream->mAudioOutputStreams.Length(); ++i) {
    ticksWritten = 0;

    MediaStream::AudioOutputStream& audioOutput = aStream->mAudioOutputStreams[i];
    StreamBuffer::Track* track = aStream->mBuffer.FindTrack(audioOutput.mTrackID);
    AudioSegment* audio = track->Get<AudioSegment>();
    AudioSegment output;

    StreamTime offset = aStream->GraphTimeToStreamTime(mProcessedTime);

    // We don't update aStream->mBufferStartTime here to account for time spent
    // blocked. Instead, we'll update it in UpdateCurrentTimeForStreams after
    // the blocked period has completed. But we do need to make sure we play
    // from the right offsets in the stream buffer, even if we've already
    // written silence for some amount of blocked time after the current time.
    GraphTime t = mProcessedTime;
    while (t < mStateComputedTime) {
      bool blocked = t >= aStream->mStartBlocking;
      GraphTime end = blocked ? mStateComputedTime : aStream->mStartBlocking;
      NS_ASSERTION(end <= mStateComputedTime, "mStartBlocking is wrong!");

      // Check how many ticks of sound we can provide if we are blocked some
      // time in the middle of this cycle.
      StreamTime toWrite = end - t;

      if (blocked) {
        output.InsertNullDataAtStart(toWrite);
        ticksWritten += toWrite;
        STREAM_LOG(LogLevel::Verbose, ("MediaStream %p writing %ld blocking-silence samples for %f to %f (%ld to %ld)\n",
                                    aStream, toWrite, MediaTimeToSeconds(t), MediaTimeToSeconds(end),
                                    offset, offset + toWrite));
      } else {
        StreamTime endTicksNeeded = offset + toWrite;
        StreamTime endTicksAvailable = audio->GetDuration();

        if (endTicksNeeded <= endTicksAvailable) {
          STREAM_LOG(LogLevel::Verbose,
                     ("MediaStream %p writing %ld samples for %f to %f "
                      "(samples %ld to %ld)\n",
                      aStream, toWrite, MediaTimeToSeconds(t),
                      MediaTimeToSeconds(end), offset, endTicksNeeded));
          output.AppendSlice(*audio, offset, endTicksNeeded);
          ticksWritten += toWrite;
          offset = endTicksNeeded;
        } else {
          // MOZ_ASSERT(track->IsEnded(), "Not enough data, and track not ended.");
          // If we are at the end of the track, maybe write the remaining
          // samples, and pad with/output silence.
          if (endTicksNeeded > endTicksAvailable &&
              offset < endTicksAvailable) {
            output.AppendSlice(*audio, offset, endTicksAvailable);
            STREAM_LOG(LogLevel::Verbose,
                       ("MediaStream %p writing %ld samples for %f to %f "
                        "(samples %ld to %ld)\n",
                        aStream, toWrite, MediaTimeToSeconds(t),
                        MediaTimeToSeconds(end), offset, endTicksNeeded));
            uint32_t available = endTicksAvailable - offset;
            ticksWritten += available;
            toWrite -= available;
            offset = endTicksAvailable;
          }
          output.AppendNullData(toWrite);
          STREAM_LOG(LogLevel::Verbose,
                     ("MediaStream %p writing %ld padding slsamples for %f to "
                      "%f (samples %ld to %ld)\n",
                      aStream, toWrite, MediaTimeToSeconds(t),
                      MediaTimeToSeconds(end), offset, endTicksNeeded));
          ticksWritten += toWrite;
        }
        output.ApplyVolume(volume);
      }
      t = end;
    }
    audioOutput.mLastTickWritten = offset;

    // Need unique id for stream & track - and we want it to match the inserter
    output.WriteTo(LATENCY_STREAM_ID(aStream, track->GetID()),
                                     mMixer, AudioChannelCount(),
                                     mSampleRate);
  }
  return ticksWritten;
}

static void
SetImageToBlackPixel(PlanarYCbCrImage* aImage)
{
  uint8_t blackPixel[] = { 0x10, 0x80, 0x80 };

  PlanarYCbCrData data;
  data.mYChannel = blackPixel;
  data.mCbChannel = blackPixel + 1;
  data.mCrChannel = blackPixel + 2;
  data.mYStride = data.mCbCrStride = 1;
  data.mPicSize = data.mYSize = data.mCbCrSize = IntSize(1, 1);
  aImage->SetData(data);
}

class VideoFrameContainerInvalidateRunnable : public nsRunnable {
public:
  explicit VideoFrameContainerInvalidateRunnable(VideoFrameContainer* aVideoFrameContainer)
    : mVideoFrameContainer(aVideoFrameContainer)
  {}
  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    mVideoFrameContainer->Invalidate();

    return NS_OK;
  }
private:
  RefPtr<VideoFrameContainer> mVideoFrameContainer;
};

void
MediaStreamGraphImpl::PlayVideo(MediaStream* aStream)
{
  MOZ_ASSERT(mRealtime, "Should only attempt to play video in realtime mode");

  if (aStream->mVideoOutputs.IsEmpty())
    return;

  TimeStamp currentTimeStamp = CurrentDriver()->GetCurrentTimeStamp();

  // Collect any new frames produced in this iteration.
  nsAutoTArray<ImageContainer::NonOwningImage,4> newImages;
  RefPtr<Image> blackImage;

  MOZ_ASSERT(mProcessedTime >= aStream->mBufferStartTime, "frame position before buffer?");
  // We only look at the non-blocking interval
  StreamTime frameBufferTime = aStream->GraphTimeToStreamTime(mProcessedTime);
  StreamTime bufferEndTime = aStream->GraphTimeToStreamTime(aStream->mStartBlocking);
  StreamTime start;
  const VideoChunk* chunk;
  for ( ;
       frameBufferTime < bufferEndTime;
       frameBufferTime = start + chunk->GetDuration()) {
    // Pick the last track that has a video chunk for the time, and
    // schedule its frame.
    chunk = nullptr;
    for (StreamBuffer::TrackIter tracks(aStream->GetStreamBuffer(),
                                        MediaSegment::VIDEO);
         !tracks.IsEnded();
         tracks.Next()) {
      VideoSegment* segment = tracks->Get<VideoSegment>();
      StreamTime thisStart;
      const VideoChunk* thisChunk =
        segment->FindChunkContaining(frameBufferTime, &thisStart);
      if (thisChunk && thisChunk->mFrame.GetImage()) {
        start = thisStart;
        chunk = thisChunk;
      }
    }
    if (!chunk)
      break;

    const VideoFrame* frame = &chunk->mFrame;
    if (*frame == aStream->mLastPlayedVideoFrame) {
      continue;
    }

    Image* image = frame->GetImage();
    STREAM_LOG(LogLevel::Verbose,
               ("MediaStream %p writing video frame %p (%dx%d)",
                aStream, image, frame->GetIntrinsicSize().width,
                frame->GetIntrinsicSize().height));
    // Schedule this frame after the previous frame finishes, instead of at
    // its start time.  These times only differ in the case of multiple
    // tracks.
    // frameBufferTime is in the non-blocking interval.
    GraphTime frameTime = aStream->StreamTimeToGraphTime(frameBufferTime);
    TimeStamp targetTime = currentTimeStamp +
      TimeDuration::FromSeconds(MediaTimeToSeconds(frameTime - IterationEnd()));

    if (frame->GetForceBlack()) {
      if (!blackImage) {
        blackImage = aStream->mVideoOutputs[0]->
          GetImageContainer()->CreateImage(ImageFormat::PLANAR_YCBCR);
        if (blackImage) {
          // Sets the image to a single black pixel, which will be scaled to
          // fill the rendered size.
          SetImageToBlackPixel(static_cast<PlanarYCbCrImage*>
                               (blackImage.get()));
        }
      }
      if (blackImage) {
        image = blackImage;
      }
    }
    newImages.AppendElement(ImageContainer::NonOwningImage(image, targetTime));

    aStream->mLastPlayedVideoFrame = *frame;
  }

  if (!aStream->mLastPlayedVideoFrame.GetImage())
    return;

  nsAutoTArray<ImageContainer::NonOwningImage,4> images;
  bool haveMultipleImages = false;

  for (uint32_t i = 0; i < aStream->mVideoOutputs.Length(); ++i) {
    VideoFrameContainer* output = aStream->mVideoOutputs[i];

    // Find previous frames that may still be valid.
    nsAutoTArray<ImageContainer::OwningImage,4> previousImages;
    output->GetImageContainer()->GetCurrentImages(&previousImages);
    uint32_t j = previousImages.Length();
    if (j) {
      // Re-use the most recent frame before currentTimeStamp and subsequent,
      // always keeping at least one frame.
      do {
        --j;
      } while (j > 0 && previousImages[j].mTimeStamp > currentTimeStamp);
    }
    if (previousImages.Length() - j + newImages.Length() > 1) {
      haveMultipleImages = true;
    }

    // Don't update if there are no changes.
    if (j == 0 && newImages.IsEmpty())
      continue;

    for ( ; j < previousImages.Length(); ++j) {
      const auto& image = previousImages[j];
      // Cope with potential clock skew with AudioCallbackDriver.
      if (newImages.Length() && image.mTimeStamp > newImages[0].mTimeStamp) {
        STREAM_LOG(LogLevel::Warning,
                   ("Dropping %u video frames due to clock skew",
                    unsigned(previousImages.Length() - j)));
        break;
      }

      images.AppendElement(ImageContainer::
                           NonOwningImage(image.mImage,
                                          image.mTimeStamp, image.mFrameID));
    }

    // Add the frames from this iteration.
    for (auto& image : newImages) {
      image.mFrameID = output->NewFrameID();
      images.AppendElement(image);
    }
    output->SetCurrentFrames(aStream->mLastPlayedVideoFrame.GetIntrinsicSize(),
                             images);

    nsCOMPtr<nsIRunnable> event =
      new VideoFrameContainerInvalidateRunnable(output);
    DispatchToMainThreadAfterStreamStateUpdate(event.forget());

    images.ClearAndRetainStorage();
  }

  // If the stream has finished and the timestamps of all frames have expired
  // then no more updates are required.
  if (aStream->mFinished && !haveMultipleImages) {
    aStream->mLastPlayedVideoFrame.SetNull();
  }
}

bool
MediaStreamGraphImpl::ShouldUpdateMainThread()
{
  if (mRealtime) {
    return true;
  }

  TimeStamp now = TimeStamp::Now();
  if ((now - mLastMainThreadUpdate).ToMilliseconds() > CurrentDriver()->IterationDuration()) {
    mLastMainThreadUpdate = now;
    return true;
  }
  return false;
}

void
MediaStreamGraphImpl::PrepareUpdatesToMainThreadState(bool aFinalUpdate)
{
  mMonitor.AssertCurrentThreadOwns();

  // We don't want to frequently update the main thread about timing update
  // when we are not running in realtime.
  if (aFinalUpdate || ShouldUpdateMainThread()) {
    mStreamUpdates.SetCapacity(mStreamUpdates.Length() + mStreams.Length() +
        mSuspendedStreams.Length());
    for (MediaStream* stream : AllStreams()) {
      if (!stream->MainThreadNeedsUpdates()) {
        continue;
      }
      StreamUpdate* update = mStreamUpdates.AppendElement();
      update->mStream = stream;
      // No blocking to worry about here, since we've passed
      // UpdateCurrentTimeForStreams.
      update->mNextMainThreadCurrentTime =
        stream->GraphTimeToStreamTime(mProcessedTime);
      update->mNextMainThreadFinished = stream->mNotifiedFinished;
    }
    if (!mPendingUpdateRunnables.IsEmpty()) {
      mUpdateRunnables.AppendElements(Move(mPendingUpdateRunnables));
    }
  }

  // Don't send the message to the main thread if it's not going to have
  // any work to do.
  if (aFinalUpdate ||
      !mUpdateRunnables.IsEmpty() ||
      !mStreamUpdates.IsEmpty()) {
    EnsureStableStateEventPosted();
  }
}

GraphTime
MediaStreamGraphImpl::RoundUpToNextAudioBlock(GraphTime aTime)
{
  StreamTime ticks = aTime;
  uint64_t block = ticks >> WEBAUDIO_BLOCK_SIZE_BITS;
  uint64_t nextBlock = block + 1;
  StreamTime nextTicks = nextBlock << WEBAUDIO_BLOCK_SIZE_BITS;
  return nextTicks;
}

void
MediaStreamGraphImpl::ProduceDataForStreamsBlockByBlock(uint32_t aStreamIndex,
                                                        TrackRate aSampleRate)
{
  MOZ_ASSERT(aStreamIndex <= mFirstCycleBreaker,
             "Cycle breaker is not AudioNodeStream?");
  GraphTime t = mProcessedTime;
  while (t < mStateComputedTime) {
    GraphTime next = RoundUpToNextAudioBlock(t);
    for (uint32_t i = mFirstCycleBreaker; i < mStreams.Length(); ++i) {
      auto ns = static_cast<AudioNodeStream*>(mStreams[i]);
      MOZ_ASSERT(ns->AsAudioNodeStream());
      ns->ProduceOutputBeforeInput(t);
    }
    for (uint32_t i = aStreamIndex; i < mStreams.Length(); ++i) {
      ProcessedMediaStream* ps = mStreams[i]->AsProcessedStream();
      if (ps) {
        ps->ProcessInput(t, next,
            (next == mStateComputedTime) ? ProcessedMediaStream::ALLOW_FINISH : 0);
      }
    }
    t = next;
  }
  NS_ASSERTION(t == mStateComputedTime,
               "Something went wrong with rounding to block boundaries");
}

bool
MediaStreamGraphImpl::AllFinishedStreamsNotified()
{
  for (MediaStream* stream : AllStreams()) {
    if (stream->mFinished && !stream->mNotifiedFinished) {
      return false;
    }
  }
  return true;
}

void
MediaStreamGraphImpl::RunMessageAfterProcessing(nsAutoPtr<ControlMessage> aMessage)
{
  MOZ_ASSERT(CurrentDriver()->OnThread());

  if (mFrontMessageQueue.IsEmpty()) {
    mFrontMessageQueue.AppendElement();
  }

  // Only one block is used for messages from the graph thread.
  MOZ_ASSERT(mFrontMessageQueue.Length() == 1);
  mFrontMessageQueue[0].mMessages.AppendElement(Move(aMessage));
}

void
MediaStreamGraphImpl::RunMessagesInQueue()
{
  // Calculate independent action times for each batch of messages (each
  // batch corresponding to an event loop task). This isolates the performance
  // of different scripts to some extent.
  for (uint32_t i = 0; i < mFrontMessageQueue.Length(); ++i) {
    nsTArray<nsAutoPtr<ControlMessage> >& messages = mFrontMessageQueue[i].mMessages;

    for (uint32_t j = 0; j < messages.Length(); ++j) {
      messages[j]->Run();
    }
  }
  mFrontMessageQueue.Clear();
}

void
MediaStreamGraphImpl::UpdateGraph(GraphTime aEndBlockingDecisions)
{
  MOZ_ASSERT(aEndBlockingDecisions >= mProcessedTime);
  // The next state computed time can be the same as the previous: it
  // means the driver would be have been blocking indefinitly, but the graph has
  // been woken up right after having been to sleep.
  MOZ_ASSERT(aEndBlockingDecisions >= mStateComputedTime);

  UpdateStreamOrder();

  bool ensureNextIteration = false;

  // Grab pending stream input and compute blocking time
  for (MediaStream* stream : mStreams) {
    if (SourceMediaStream* is = stream->AsSourceStream()) {
      ExtractPendingInput(is, aEndBlockingDecisions, &ensureNextIteration);
    }

    if (stream->mFinished) {
      // The stream's not suspended, and since it's finished, underruns won't
      // stop it playing out. So there's no blocking other than what we impose
      // here.
      GraphTime endTime = stream->GetStreamBuffer().GetAllTracksEnd() +
          stream->mBufferStartTime;
      if (endTime <= mStateComputedTime) {
        STREAM_LOG(LogLevel::Verbose, ("MediaStream %p is blocked due to being finished", stream));
        stream->mStartBlocking = mStateComputedTime;
      } else {
        STREAM_LOG(LogLevel::Verbose, ("MediaStream %p is finished, but not blocked yet (end at %f, with blocking at %f)",
            stream, MediaTimeToSeconds(stream->GetBufferEnd()),
            MediaTimeToSeconds(endTime)));
        // Data can't be added to a finished stream, so underruns are irrelevant.
        stream->mStartBlocking = std::min(endTime, aEndBlockingDecisions);
      }
    } else {
      stream->mStartBlocking = WillUnderrun(stream, aEndBlockingDecisions);
    }
  }

  for (MediaStream* stream : mSuspendedStreams) {
    stream->mStartBlocking = mStateComputedTime;
  }

  // The loop is woken up so soon that IterationEnd() barely advances and we
  // end up having aEndBlockingDecision == mStateComputedTime.
  // Since stream blocking is computed in the interval of
  // [mStateComputedTime, aEndBlockingDecision), it won't be computed at all.
  // We should ensure next iteration so that pending blocking changes will be
  // computed in next loop.
  if (ensureNextIteration ||
      aEndBlockingDecisions == mStateComputedTime) {
    EnsureNextIteration();
  }
}

void
MediaStreamGraphImpl::Process()
{
  // Play stream contents.
  bool allBlockedForever = true;
  // True when we've done ProcessInput for all processed streams.
  bool doneAllProducing = false;
  // This is the number of frame that are written to the AudioStreams, for
  // this cycle.
  StreamTime ticksPlayed = 0;

  mMixer.StartMixing();

  // Figure out what each stream wants to do
  for (uint32_t i = 0; i < mStreams.Length(); ++i) {
    MediaStream* stream = mStreams[i];
    if (!doneAllProducing) {
      ProcessedMediaStream* ps = stream->AsProcessedStream();
      if (ps) {
        AudioNodeStream* n = stream->AsAudioNodeStream();
        if (n) {
#ifdef DEBUG
          // Verify that the sampling rate for all of the following streams is the same
          for (uint32_t j = i + 1; j < mStreams.Length(); ++j) {
            AudioNodeStream* nextStream = mStreams[j]->AsAudioNodeStream();
            if (nextStream) {
              MOZ_ASSERT(n->SampleRate() == nextStream->SampleRate(),
                         "All AudioNodeStreams in the graph must have the same sampling rate");
            }
          }
#endif
          // Since an AudioNodeStream is present, go ahead and
          // produce audio block by block for all the rest of the streams.
          ProduceDataForStreamsBlockByBlock(i, n->SampleRate());
          doneAllProducing = true;
        } else {
          ps->ProcessInput(mProcessedTime, mStateComputedTime,
                           ProcessedMediaStream::ALLOW_FINISH);
          NS_WARN_IF_FALSE(stream->mBuffer.GetEnd() >=
                           GraphTimeToStreamTimeWithBlocking(stream, mStateComputedTime),
                           "Stream did not produce enough data");
        }
      }
    }
    NotifyHasCurrentData(stream);
    // Only playback audio and video in real-time mode
    if (mRealtime) {
      CreateOrDestroyAudioStreams(stream);
      if (CurrentDriver()->AsAudioCallbackDriver()) {
        StreamTime ticksPlayedForThisStream = PlayAudio(stream);
        if (!ticksPlayed) {
          ticksPlayed = ticksPlayedForThisStream;
        } else {
          MOZ_ASSERT(!ticksPlayedForThisStream || ticksPlayedForThisStream == ticksPlayed,
              "Each stream should have the same number of frame.");
        }
      }
      PlayVideo(stream);
    }
    if (stream->mStartBlocking > mProcessedTime) {
      allBlockedForever = false;
    }
  }

  if (CurrentDriver()->AsAudioCallbackDriver() && ticksPlayed) {
    mMixer.FinishMixing();
  }

  // If we are switching away from an AudioCallbackDriver, we don't need the
  // mixer anymore.
  if (CurrentDriver()->AsAudioCallbackDriver() &&
      CurrentDriver()->Switching()) {
    bool isStarted;
    {
      MonitorAutoLock mon(mMonitor);
      isStarted = CurrentDriver()->AsAudioCallbackDriver()->IsStarted();
    }
    if (isStarted) {
      mMixer.RemoveCallback(CurrentDriver()->AsAudioCallbackDriver());
    }
  }

  if (!allBlockedForever) {
    EnsureNextIteration();
  }
}

void
MediaStreamGraphImpl::MaybeProduceMemoryReport()
{
  MonitorAutoLock lock(mMemoryReportMonitor);
  if (mNeedsMemoryReport) {
    mNeedsMemoryReport = false;

    for (MediaStream* s : AllStreams()) {
      AudioNodeStream* stream = s->AsAudioNodeStream();
      if (stream) {
        AudioNodeSizes usage;
        stream->SizeOfAudioNodesIncludingThis(MallocSizeOf, usage);
        mAudioStreamSizes.AppendElement(usage);
      }
    }

    lock.Notify();
  }
}

bool
MediaStreamGraphImpl::UpdateMainThreadState()
{
  MonitorAutoLock lock(mMonitor);
  bool finalUpdate = mForceShutDown ||
    (mProcessedTime >= mEndTime && AllFinishedStreamsNotified()) ||
    (IsEmpty() && mBackMessageQueue.IsEmpty());
  PrepareUpdatesToMainThreadState(finalUpdate);
  if (finalUpdate) {
    // Enter shutdown mode. The stable-state handler will detect this
    // and complete shutdown. Destroy any streams immediately.
    STREAM_LOG(LogLevel::Debug, ("MediaStreamGraph %p waiting for main thread cleanup", this));
    // We'll shut down this graph object if it does not get restarted.
    mLifecycleState = LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP;
    // No need to Destroy streams here. The main-thread owner of each
    // stream is responsible for calling Destroy on them.
    return false;
  }

  CurrentDriver()->WaitForNextIteration();

  SwapMessageQueues();
  return true;
}

bool
MediaStreamGraphImpl::OneIteration(GraphTime aStateEnd)
{
  MaybeProduceMemoryReport();

  // Process graph message from the main thread for this iteration.
  RunMessagesInQueue();

  GraphTime stateEnd = std::min(aStateEnd, mEndTime);
  UpdateGraph(stateEnd);

  mStateComputedTime = stateEnd;

  Process();

  GraphTime oldProcessedTime = mProcessedTime;
  mProcessedTime = stateEnd;

  UpdateCurrentTimeForStreams(oldProcessedTime);

  // Process graph messages queued from RunMessageAfterProcessing() on this
  // thread during the iteration.
  RunMessagesInQueue();

  return UpdateMainThreadState();
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

  if (stream->ShouldNotifyStreamFinished()) {
    if (stream->mWrapper) {
      stream->mWrapper->NotifyStreamFinished();
    }

    stream->NotifyMainThreadListeners();
  }
}

void
MediaStreamGraphImpl::ForceShutDown()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called on main thread");
  STREAM_LOG(LogLevel::Debug, ("MediaStreamGraph %p ForceShutdown", this));
  {
    MonitorAutoLock lock(mMonitor);
    mForceShutDown = true;
    EnsureNextIterationLocked();
  }
}

namespace {

class MediaStreamGraphShutDownRunnable : public nsRunnable {
public:
  explicit MediaStreamGraphShutDownRunnable(MediaStreamGraphImpl* aGraph)
    : mGraph(aGraph)
  {}
  NS_IMETHOD Run()
  {
    NS_ASSERTION(mGraph->mDetectedNotRunning,
                 "We should know the graph thread control loop isn't running!");

    LIFECYCLE_LOG("Shutting down graph %p", mGraph.get());

    // We've asserted the graph isn't running.  Use mDriver instead of CurrentDriver
    // to avoid thread-safety checks
#if 0 // AudioCallbackDrivers are released asynchronously anyways
    // XXX a better test would be have setting mDetectedNotRunning make sure
    // any current callback has finished and block future ones -- or just
    // handle it all in Shutdown()!
    if (mGraph->mDriver->AsAudioCallbackDriver()) {
      MOZ_ASSERT(!mGraph->mDriver->AsAudioCallbackDriver()->InCallback());
    }
#endif

    mGraph->mDriver->Shutdown();

    // mGraph's thread is not running so it's OK to do whatever here
    if (mGraph->IsEmpty()) {
      // mGraph is no longer needed, so delete it.
      mGraph->Destroy();
    } else {
      // The graph is not empty.  We must be in a forced shutdown, or a
      // non-realtime graph that has finished processing.  Some later
      // AppendMessage will detect that the manager has been emptied, and
      // delete it.
      NS_ASSERTION(mGraph->mForceShutDown || !mGraph->mRealtime,
                   "Not in forced shutdown?");
      for (MediaStream* stream : mGraph->AllStreams()) {
        DOMMediaStream* s = stream->GetWrapper();
        if (s) {
          s->NotifyMediaStreamGraphShutdown();
        }
      }

      mGraph->mLifecycleState =
        MediaStreamGraphImpl::LIFECYCLE_WAITING_FOR_STREAM_DESTRUCTION;
    }
    return NS_OK;
  }
private:
  RefPtr<MediaStreamGraphImpl> mGraph;
};

class MediaStreamGraphStableStateRunnable : public nsRunnable {
public:
  explicit MediaStreamGraphStableStateRunnable(MediaStreamGraphImpl* aGraph,
                                               bool aSourceIsMSG)
    : mGraph(aGraph)
    , mSourceIsMSG(aSourceIsMSG)
  {
  }
  NS_IMETHOD Run()
  {
    if (mGraph) {
      mGraph->RunInStableState(mSourceIsMSG);
    }
    return NS_OK;
  }
private:
  RefPtr<MediaStreamGraphImpl> mGraph;
  bool mSourceIsMSG;
};

/*
 * Control messages forwarded from main thread to graph manager thread
 */
class CreateMessage : public ControlMessage {
public:
  explicit CreateMessage(MediaStream* aStream) : ControlMessage(aStream) {}
  virtual void Run() override
  {
    mStream->GraphImpl()->AddStreamGraphThread(mStream);
  }
  virtual void RunDuringShutdown() override
  {
    // Make sure to run this message during shutdown too, to make sure
    // that we balance the number of streams registered with the graph
    // as they're destroyed during shutdown.
    Run();
  }
};

class MediaStreamGraphShutdownObserver final : public nsIObserver
{
  ~MediaStreamGraphShutdownObserver() {}
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

} // namespace

void
MediaStreamGraphImpl::RunInStableState(bool aSourceIsMSG)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called on main thread");

  nsTArray<nsCOMPtr<nsIRunnable> > runnables;
  // When we're doing a forced shutdown, pending control messages may be
  // run on the main thread via RunDuringShutdown. Those messages must
  // run without the graph monitor being held. So, we collect them here.
  nsTArray<nsAutoPtr<ControlMessage> > controlMessagesToRunDuringShutdown;

  {
    MonitorAutoLock lock(mMonitor);
    if (aSourceIsMSG) {
      MOZ_ASSERT(mPostedRunInStableStateEvent);
      mPostedRunInStableStateEvent = false;
    }

#ifdef ENABLE_LIFECYCLE_LOG
  // This should be kept in sync with the LifecycleState enum in
  // MediaStreamGraphImpl.h
  const char * LifecycleState_str[] = {
    "LIFECYCLE_THREAD_NOT_STARTED",
    "LIFECYCLE_RUNNING",
    "LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP",
    "LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN",
    "LIFECYCLE_WAITING_FOR_STREAM_DESTRUCTION"
  };

  if (mLifecycleState != LIFECYCLE_RUNNING) {
    LIFECYCLE_LOG("Running %p in stable state. Current state: %s\n",
        this, LifecycleState_str[mLifecycleState]);
  }
#endif

    runnables.SwapElements(mUpdateRunnables);
    for (uint32_t i = 0; i < mStreamUpdates.Length(); ++i) {
      StreamUpdate* update = &mStreamUpdates[i];
      if (update->mStream) {
        ApplyStreamUpdate(update);
      }
    }
    mStreamUpdates.Clear();

    if (mCurrentTaskMessageQueue.IsEmpty()) {
      if (mLifecycleState == LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP && IsEmpty()) {
        // Complete shutdown. First, ensure that this graph is no longer used.
        // A new graph graph will be created if one is needed.
        // Asynchronously clean up old graph. We don't want to do this
        // synchronously because it spins the event loop waiting for threads
        // to shut down, and we don't want to do that in a stable state handler.
        mLifecycleState = LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN;
        LIFECYCLE_LOG("Sending MediaStreamGraphShutDownRunnable %p", this);
        nsCOMPtr<nsIRunnable> event = new MediaStreamGraphShutDownRunnable(this );
        NS_DispatchToMainThread(event.forget());

        LIFECYCLE_LOG("Disconnecting MediaStreamGraph %p", this);
        MediaStreamGraphImpl* graph;
        if (gGraphs.Get(uint32_t(mAudioChannel), &graph) && graph == this) {
          // null out gGraph if that's the graph being shut down
          gGraphs.Remove(uint32_t(mAudioChannel));
        }
      }
    } else {
      if (mLifecycleState <= LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP) {
        MessageBlock* block = mBackMessageQueue.AppendElement();
        block->mMessages.SwapElements(mCurrentTaskMessageQueue);
        EnsureNextIterationLocked();
      }

      // If the MediaStreamGraph has more messages going to it, try to revive
      // it to process those messages. Don't do this if we're in a forced
      // shutdown or it's a non-realtime graph that has already terminated
      // processing.
      if (mLifecycleState == LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP &&
          mRealtime && !mForceShutDown) {
        mLifecycleState = LIFECYCLE_RUNNING;
        // Revive the MediaStreamGraph since we have more messages going to it.
        // Note that we need to put messages into its queue before reviving it,
        // or it might exit immediately.
        {
          LIFECYCLE_LOG("Reviving a graph (%p) ! %s\n",
              this, CurrentDriver()->AsAudioCallbackDriver() ? "AudioDriver" :
                                                               "SystemDriver");
          RefPtr<GraphDriver> driver = CurrentDriver();
          MonitorAutoUnlock unlock(mMonitor);
          driver->Revive();
        }
      }
    }

    // Don't start the thread for a non-realtime graph until it has been
    // explicitly started by StartNonRealtimeProcessing.
    if (mLifecycleState == LIFECYCLE_THREAD_NOT_STARTED &&
        (mRealtime || mNonRealtimeProcessing)) {
      mLifecycleState = LIFECYCLE_RUNNING;
      // Start the thread now. We couldn't start it earlier because
      // the graph might exit immediately on finding it has no streams. The
      // first message for a new graph must create a stream.
      {
        // We should exit the monitor for now, because starting a stream might
        // take locks, and we don't want to deadlock.
        LIFECYCLE_LOG("Starting a graph (%p) ! %s\n",
                      this,
                      CurrentDriver()->AsAudioCallbackDriver() ? "AudioDriver" :
                                                                 "SystemDriver");
        RefPtr<GraphDriver> driver = CurrentDriver();
        MonitorAutoUnlock unlock(mMonitor);
        driver->Start();
      }
    }

    if ((mForceShutDown || !mRealtime) &&
        mLifecycleState == LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP) {
      // Defer calls to RunDuringShutdown() to happen while mMonitor is not held.
      for (uint32_t i = 0; i < mBackMessageQueue.Length(); ++i) {
        MessageBlock& mb = mBackMessageQueue[i];
        controlMessagesToRunDuringShutdown.AppendElements(Move(mb.mMessages));
      }
      mBackMessageQueue.Clear();
      MOZ_ASSERT(mCurrentTaskMessageQueue.IsEmpty());
      // Stop MediaStreamGraph threads. Do not clear gGraph since
      // we have outstanding DOM objects that may need it.
      mLifecycleState = LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN;
      nsCOMPtr<nsIRunnable> event = new MediaStreamGraphShutDownRunnable(this);
      NS_DispatchToMainThread(event.forget());
    }

    mDetectedNotRunning = mLifecycleState > LIFECYCLE_RUNNING;
  }

  // Make sure we get a new current time in the next event loop task
  if (!aSourceIsMSG) {
    MOZ_ASSERT(mPostedRunInStableState);
    mPostedRunInStableState = false;
  }

  for (uint32_t i = 0; i < controlMessagesToRunDuringShutdown.Length(); ++i) {
    controlMessagesToRunDuringShutdown[i]->RunDuringShutdown();
  }

#ifdef DEBUG
  mCanRunMessagesSynchronously = mDetectedNotRunning &&
    mLifecycleState >= LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN;
#endif

  for (uint32_t i = 0; i < runnables.Length(); ++i) {
    runnables[i]->Run();
    // "Direct" tail dispatcher are supposed to run immediately following the
    // execution of the current task. So the meta-tasking that we do here in
    // RunInStableState() breaks that abstraction a bit unless we handle it here.
    //
    // This is particularly important because we can end up with a "stream
    // ended" notification immediately following a "stream available" notification,
    // and we need to make sure that the watcher responding to "stream available"
    // has a chance to run before the second notification starts tearing things
    // down.
    AbstractThread::MainThread()->TailDispatcher().DrainDirectTasks();
  }
}


void
MediaStreamGraphImpl::EnsureRunInStableState()
{
  NS_ASSERTION(NS_IsMainThread(), "main thread only");

  if (mPostedRunInStableState)
    return;
  mPostedRunInStableState = true;
  nsCOMPtr<nsIRunnable> event = new MediaStreamGraphStableStateRunnable(this, false);
  nsContentUtils::RunInStableState(event.forget());
}

void
MediaStreamGraphImpl::EnsureStableStateEventPosted()
{
  mMonitor.AssertCurrentThreadOwns();

  if (mPostedRunInStableStateEvent)
    return;
  mPostedRunInStableStateEvent = true;
  nsCOMPtr<nsIRunnable> event = new MediaStreamGraphStableStateRunnable(this, true);
  NS_DispatchToMainThread(event.forget());
}

void
MediaStreamGraphImpl::AppendMessage(ControlMessage* aMessage)
{
  MOZ_ASSERT(NS_IsMainThread(), "main thread only");
  MOZ_ASSERT(!aMessage->GetStream() ||
             !aMessage->GetStream()->IsDestroyed(),
             "Stream already destroyed");

  if (mDetectedNotRunning &&
      mLifecycleState > LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP) {
    // The graph control loop is not running and main thread cleanup has
    // happened. From now on we can't append messages to mCurrentTaskMessageQueue,
    // because that will never be processed again, so just RunDuringShutdown
    // this message.
    // This should only happen during forced shutdown, or after a non-realtime
    // graph has finished processing.
#ifdef DEBUG
    MOZ_ASSERT(mCanRunMessagesSynchronously);
    mCanRunMessagesSynchronously = false;
#endif
    aMessage->RunDuringShutdown();
#ifdef DEBUG
    mCanRunMessagesSynchronously = true;
#endif
    delete aMessage;
    if (IsEmpty() &&
        mLifecycleState >= LIFECYCLE_WAITING_FOR_STREAM_DESTRUCTION) {

      MediaStreamGraphImpl* graph;
      if (gGraphs.Get(uint32_t(mAudioChannel), &graph) && graph == this) {
        gGraphs.Remove(uint32_t(mAudioChannel));
      }

      Destroy();
    }
    return;
  }

  mCurrentTaskMessageQueue.AppendElement(aMessage);
  EnsureRunInStableState();
}

MediaStream::MediaStream(DOMMediaStream* aWrapper)
  : mBufferStartTime(0)
  , mStartBlocking(GRAPH_TIME_MAX)
  , mSuspendedCount(0)
  , mFinished(false)
  , mNotifiedFinished(false)
  , mNotifiedBlocked(false)
  , mHasCurrentData(false)
  , mNotifiedHasCurrentData(false)
  , mWrapper(aWrapper)
  , mMainThreadCurrentTime(0)
  , mMainThreadFinished(false)
  , mFinishedNotificationSent(false)
  , mMainThreadDestroyed(false)
  , mGraph(nullptr)
  , mAudioChannelType(dom::AudioChannel::Normal)
{
  MOZ_COUNT_CTOR(MediaStream);
  // aWrapper should not already be connected to a MediaStream! It needs
  // to be hooked up to this stream, and since this stream is only just
  // being created now, aWrapper must not be connected to anything.
  NS_ASSERTION(!aWrapper || !aWrapper->GetPlaybackStream(),
               "Wrapper already has another media stream hooked up to it!");
}

size_t
MediaStream::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = 0;

  // Not owned:
  // - mGraph - Not reported here
  // - mConsumers - elements
  // Future:
  // - mWrapper
  // - mVideoOutputs - elements
  // - mLastPlayedVideoFrame
  // - mListeners - elements
  // - mAudioOutputStream - elements

  amount += mBuffer.SizeOfExcludingThis(aMallocSizeOf);
  amount += mAudioOutputs.ShallowSizeOfExcludingThis(aMallocSizeOf);
  amount += mVideoOutputs.ShallowSizeOfExcludingThis(aMallocSizeOf);
  amount += mListeners.ShallowSizeOfExcludingThis(aMallocSizeOf);
  amount += mMainThreadListeners.ShallowSizeOfExcludingThis(aMallocSizeOf);
  amount += mDisabledTrackIDs.ShallowSizeOfExcludingThis(aMallocSizeOf);
  amount += mConsumers.ShallowSizeOfExcludingThis(aMallocSizeOf);

  return amount;
}

size_t
MediaStream::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

MediaStreamGraphImpl*
MediaStream::GraphImpl()
{
  return mGraph;
}

MediaStreamGraph*
MediaStream::Graph()
{
  return mGraph;
}

void
MediaStream::SetGraphImpl(MediaStreamGraphImpl* aGraph)
{
  MOZ_ASSERT(!mGraph, "Should only be called once");
  mGraph = aGraph;
  mAudioChannelType = aGraph->AudioChannel();
  mBuffer.InitGraphRate(aGraph->GraphRate());
}

void
MediaStream::SetGraphImpl(MediaStreamGraph* aGraph)
{
  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(aGraph);
  SetGraphImpl(graph);
}

StreamTime
MediaStream::GraphTimeToStreamTime(GraphTime aTime)
{
  NS_ASSERTION(mStartBlocking == GraphImpl()->mStateComputedTime ||
               aTime <= mStartBlocking,
               "Incorrectly ignoring blocking!");
  return aTime - mBufferStartTime;
}

GraphTime
MediaStream::StreamTimeToGraphTime(StreamTime aTime)
{
  NS_ASSERTION(mStartBlocking == GraphImpl()->mStateComputedTime ||
               aTime + mBufferStartTime <= mStartBlocking,
               "Incorrectly ignoring blocking!");
  return aTime + mBufferStartTime;
}

StreamTime
MediaStream::GraphTimeToStreamTimeWithBlocking(GraphTime aTime)
{
  return GraphImpl()->GraphTimeToStreamTimeWithBlocking(this, aTime);
}

void
MediaStream::FinishOnGraphThread()
{
  GraphImpl()->FinishStream(this);
}

StreamBuffer::Track*
MediaStream::EnsureTrack(TrackID aTrackId)
{
  StreamBuffer::Track* track = mBuffer.FindTrack(aTrackId);
  if (!track) {
    nsAutoPtr<MediaSegment> segment(new AudioSegment());
    for (uint32_t j = 0; j < mListeners.Length(); ++j) {
      MediaStreamListener* l = mListeners[j];
      l->NotifyQueuedTrackChanges(Graph(), aTrackId, 0,
                                  MediaStreamListener::TRACK_EVENT_CREATED,
                                  *segment);
      // TODO If we ever need to ensure several tracks at once, we will have to
      // change this.
      l->NotifyFinishedTrackCreation(Graph());
    }
    track = &mBuffer.AddTrack(aTrackId, 0, segment.forget());
  }
  return track;
}

void
MediaStream::RemoveAllListenersImpl()
{
  for (int32_t i = mListeners.Length() - 1; i >= 0; --i) {
    RefPtr<MediaStreamListener> listener = mListeners[i].forget();
    listener->NotifyEvent(GraphImpl(), MediaStreamListener::EVENT_REMOVED);
  }
  mListeners.Clear();
}

void
MediaStream::DestroyImpl()
{
  for (int32_t i = mConsumers.Length() - 1; i >= 0; --i) {
    mConsumers[i]->Disconnect();
  }
  mGraph = nullptr;
}

void
MediaStream::Destroy()
{
  // Keep this stream alive until we leave this method
  RefPtr<MediaStream> kungFuDeathGrip = this;

  class Message : public ControlMessage {
  public:
    explicit Message(MediaStream* aStream) : ControlMessage(aStream) {}
    virtual void Run()
    {
      mStream->RemoveAllListenersImpl();
      auto graph = mStream->GraphImpl();
      mStream->DestroyImpl();
      graph->RemoveStreamGraphThread(mStream);
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
MediaStream::AddAudioOutputImpl(void* aKey)
{
  STREAM_LOG(LogLevel::Info, ("MediaStream %p Adding AudioOutput for key %p",
                              this, aKey));
  mAudioOutputs.AppendElement(AudioOutput(aKey));
}

void
MediaStream::RemoveAudioOutputImpl(void* aKey)
{
  STREAM_LOG(LogLevel::Info, ("MediaStream %p Removing AudioOutput for key %p",
                              this, aKey));
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
MediaStream::AddVideoOutputImpl(already_AddRefed<VideoFrameContainer> aContainer)
{
  RefPtr<VideoFrameContainer> container = aContainer;
  STREAM_LOG(LogLevel::Info, ("MediaStream %p Adding VideoFrameContainer %p as output",
                              this, container.get()));
  *mVideoOutputs.AppendElement() = container.forget();
}

void
MediaStream::RemoveVideoOutputImpl(VideoFrameContainer* aContainer)
{
  STREAM_LOG(LogLevel::Info, ("MediaStream %p Removing VideoFrameContainer %p as output",
                              this, aContainer));
  // Ensure that any frames currently queued for playback by the compositor
  // are removed.
  aContainer->ClearFutureFrames();
  mVideoOutputs.RemoveElement(aContainer);
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
    RefPtr<VideoFrameContainer> mContainer;
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
    RefPtr<VideoFrameContainer> mContainer;
  };
  GraphImpl()->AppendMessage(new Message(this, aContainer));
}

void
MediaStream::Suspend()
{
  class Message : public ControlMessage {
  public:
    explicit Message(MediaStream* aStream) :
      ControlMessage(aStream) {}
    virtual void Run()
    {
      mStream->GraphImpl()->IncrementSuspendCount(mStream);
    }
  };

  // This can happen if this method has been called asynchronously, and the
  // stream has been destroyed since then.
  if (mMainThreadDestroyed) {
    return;
  }
  GraphImpl()->AppendMessage(new Message(this));
}

void
MediaStream::Resume()
{
  class Message : public ControlMessage {
  public:
    explicit Message(MediaStream* aStream) :
      ControlMessage(aStream) {}
    virtual void Run()
    {
      mStream->GraphImpl()->DecrementSuspendCount(mStream);
    }
  };

  // This can happen if this method has been called asynchronously, and the
  // stream has been destroyed since then.
  if (mMainThreadDestroyed) {
    return;
  }
  GraphImpl()->AppendMessage(new Message(this));
}

void
MediaStream::AddListenerImpl(already_AddRefed<MediaStreamListener> aListener)
{
  MediaStreamListener* listener = *mListeners.AppendElement() = aListener;
  listener->NotifyBlockingChanged(GraphImpl(),
    mNotifiedBlocked ? MediaStreamListener::BLOCKED : MediaStreamListener::UNBLOCKED);
  if (mNotifiedFinished) {
    listener->NotifyEvent(GraphImpl(), MediaStreamListener::EVENT_FINISHED);
  }
  if (mNotifiedHasCurrentData) {
    listener->NotifyHasCurrentData(GraphImpl());
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
    RefPtr<MediaStreamListener> mListener;
  };
  GraphImpl()->AppendMessage(new Message(this, aListener));
}

void
MediaStream::RemoveListenerImpl(MediaStreamListener* aListener)
{
  // wouldn't need this if we could do it in the opposite order
  RefPtr<MediaStreamListener> listener(aListener);
  mListeners.RemoveElement(aListener);
  listener->NotifyEvent(GraphImpl(), MediaStreamListener::EVENT_REMOVED);
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
    RefPtr<MediaStreamListener> mListener;
  };
  // If the stream is destroyed the Listeners have or will be
  // removed.
  if (!IsDestroyed()) {
    GraphImpl()->AppendMessage(new Message(this, aListener));
  }
}

void
MediaStream::RunAfterPendingUpdates(already_AddRefed<nsIRunnable> aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MediaStreamGraphImpl* graph = GraphImpl();
  nsCOMPtr<nsIRunnable> runnable(aRunnable);

  // Special case when a non-realtime graph has not started, to ensure the
  // runnable will run in finite time.
  if (!(graph->mRealtime || graph->mNonRealtimeProcessing)) {
    runnable->Run();
    return;
  }

  class Message : public ControlMessage {
  public:
    explicit Message(MediaStream* aStream,
                     already_AddRefed<nsIRunnable> aRunnable)
      : ControlMessage(aStream)
      , mRunnable(aRunnable) {}
    virtual void Run() override
    {
      mStream->Graph()->
        DispatchToMainThreadAfterStreamStateUpdate(mRunnable.forget());
    }
    virtual void RunDuringShutdown() override
    {
      // Don't run mRunnable now as it may call AppendMessage() which would
      // assume that there are no remaining controlMessagesToRunDuringShutdown.
      MOZ_ASSERT(NS_IsMainThread());
      NS_DispatchToCurrentThread(mRunnable);
    }
  private:
    nsCOMPtr<nsIRunnable> mRunnable;
  };

  graph->AppendMessage(new Message(this, runnable.forget()));
}

void
MediaStream::SetTrackEnabledImpl(TrackID aTrackID, bool aEnabled)
{
  if (aEnabled) {
    mDisabledTrackIDs.RemoveElement(aTrackID);
  } else {
    if (!mDisabledTrackIDs.Contains(aTrackID)) {
      mDisabledTrackIDs.AppendElement(aTrackID);
    }
  }
}

void
MediaStream::SetTrackEnabled(TrackID aTrackID, bool aEnabled)
{
  class Message : public ControlMessage {
  public:
    Message(MediaStream* aStream, TrackID aTrackID, bool aEnabled) :
      ControlMessage(aStream), mTrackID(aTrackID), mEnabled(aEnabled) {}
    virtual void Run()
    {
      mStream->SetTrackEnabledImpl(mTrackID, mEnabled);
    }
    TrackID mTrackID;
    bool mEnabled;
  };
  GraphImpl()->AppendMessage(new Message(this, aTrackID, aEnabled));
}

void
MediaStream::ApplyTrackDisabling(TrackID aTrackID, MediaSegment* aSegment, MediaSegment* aRawSegment)
{
  if (!mDisabledTrackIDs.Contains(aTrackID)) {
    return;
  }
  aSegment->ReplaceWithDisabled();
  if (aRawSegment) {
    aRawSegment->ReplaceWithDisabled();
  }
}

void
MediaStream::AddMainThreadListener(MainThreadMediaStreamListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(!mMainThreadListeners.Contains(aListener));

  mMainThreadListeners.AppendElement(aListener);

  // If it is not yet time to send the notification, then finish here.
  if (!mFinishedNotificationSent) {
    return;
  }

  class NotifyRunnable final : public nsRunnable
  {
  public:
    explicit NotifyRunnable(MediaStream* aStream)
      : mStream(aStream)
    {}

    NS_IMETHOD Run() override
    {
      MOZ_ASSERT(NS_IsMainThread());
      mStream->NotifyMainThreadListeners();
      return NS_OK;
    }

  private:
    ~NotifyRunnable() {}

    RefPtr<MediaStream> mStream;
  };

  RefPtr<nsRunnable> runnable = new NotifyRunnable(this);
  NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(runnable.forget())));
}

void
SourceMediaStream::DestroyImpl()
{
  // Hold mMutex while mGraph is reset so that other threads holding mMutex
  // can null-check know that the graph will not destroyed.
  MutexAutoLock lock(mMutex);
  MediaStream::DestroyImpl();
}

void
SourceMediaStream::SetPullEnabled(bool aEnabled)
{
  MutexAutoLock lock(mMutex);
  mPullEnabled = aEnabled;
  if (mPullEnabled && GraphImpl()) {
    GraphImpl()->EnsureNextIteration();
  }
}

void
SourceMediaStream::AddTrackInternal(TrackID aID, TrackRate aRate, StreamTime aStart,
                                    MediaSegment* aSegment, uint32_t aFlags)
{
  MutexAutoLock lock(mMutex);
  nsTArray<TrackData> *track_data = (aFlags & ADDTRACK_QUEUED) ?
                                    &mPendingTracks : &mUpdateTracks;
  TrackData* data = track_data->AppendElement();
  data->mID = aID;
  data->mInputRate = aRate;
  data->mResamplerChannelCount = 0;
  data->mStart = aStart;
  data->mEndOfFlushedData = aStart;
  data->mCommands = TRACK_CREATE;
  data->mData = aSegment;
  if (!(aFlags & ADDTRACK_QUEUED) && GraphImpl()) {
    GraphImpl()->EnsureNextIteration();
  }
}

void
SourceMediaStream::FinishAddTracks()
{
  MutexAutoLock lock(mMutex);
  mUpdateTracks.AppendElements(Move(mPendingTracks));
  if (GraphImpl()) {
    GraphImpl()->EnsureNextIteration();
  }
}

StreamBuffer::Track*
SourceMediaStream::FindTrack(TrackID aID)
{
  return mBuffer.FindTrack(aID);
}

void
SourceMediaStream::ResampleAudioToGraphSampleRate(TrackData* aTrackData, MediaSegment* aSegment)
{
  if (aSegment->GetType() != MediaSegment::AUDIO ||
      aTrackData->mInputRate == GraphImpl()->GraphRate()) {
    return;
  }
  AudioSegment* segment = static_cast<AudioSegment*>(aSegment);
  int channels = segment->ChannelCount();

  // If this segment is just silence, we delay instanciating the resampler. We
  // also need to recreate the resampler if the channel count changes.
  if (channels && aTrackData->mResamplerChannelCount != channels) {
    SpeexResamplerState* state = speex_resampler_init(channels,
        aTrackData->mInputRate,
        GraphImpl()->GraphRate(),
        SPEEX_RESAMPLER_QUALITY_MIN,
        nullptr);
    if (!state) {
      return;
    }
    aTrackData->mResampler.own(state);
    aTrackData->mResamplerChannelCount = channels;
  }
  segment->ResampleChunks(aTrackData->mResampler, aTrackData->mInputRate, GraphImpl()->GraphRate());
}

bool
SourceMediaStream::AppendToTrack(TrackID aID, MediaSegment* aSegment, MediaSegment *aRawSegment)
{
  MutexAutoLock lock(mMutex);
  // ::EndAllTrackAndFinished() can end these before the sources notice
  bool appended = false;
  auto graph = GraphImpl();
  if (!mFinished && graph) {
    TrackData *track = FindDataForTrack(aID);
    if (track) {
      // Data goes into mData, and on the next iteration of the MSG moves
      // into the track's segment after NotifyQueuedTrackChanges().  This adds
      // 0-10ms of delay before data gets to direct listeners.
      // Indirect listeners (via subsequent TrackUnion nodes) are synced to
      // playout time, and so can be delayed by buffering.

      // Apply track disabling before notifying any consumers directly
      // or inserting into the graph
      ApplyTrackDisabling(aID, aSegment, aRawSegment);

      ResampleAudioToGraphSampleRate(track, aSegment);

      // Must notify first, since AppendFrom() will empty out aSegment
      NotifyDirectConsumers(track, aRawSegment ? aRawSegment : aSegment);
      track->mData->AppendFrom(aSegment); // note: aSegment is now dead
      appended = true;
      GraphImpl()->EnsureNextIteration();
    } else {
      aSegment->Clear();
    }
  }
  return appended;
}

void
SourceMediaStream::NotifyDirectConsumers(TrackData *aTrack,
                                         MediaSegment *aSegment)
{
  // Call with mMutex locked
  MOZ_ASSERT(aTrack);

  for (uint32_t j = 0; j < mDirectListeners.Length(); ++j) {
    MediaStreamDirectListener* l = mDirectListeners[j];
    StreamTime offset = 0; // FIX! need a separate StreamTime.... or the end of the internal buffer
    l->NotifyRealtimeData(static_cast<MediaStreamGraph*>(GraphImpl()), aTrack->mID,
                          offset, aTrack->mCommands, *aSegment);
  }
}

// These handle notifying all the listeners of an event
void
SourceMediaStream::NotifyListenersEventImpl(MediaStreamListener::MediaStreamGraphEvent aEvent)
{
  for (uint32_t j = 0; j < mListeners.Length(); ++j) {
    MediaStreamListener* l = mListeners[j];
    l->NotifyEvent(GraphImpl(), aEvent);
  }
}

void
SourceMediaStream::NotifyListenersEvent(MediaStreamListener::MediaStreamGraphEvent aNewEvent)
{
  class Message : public ControlMessage {
  public:
    Message(SourceMediaStream* aStream, MediaStreamListener::MediaStreamGraphEvent aEvent) :
      ControlMessage(aStream), mEvent(aEvent) {}
    virtual void Run()
      {
        mStream->AsSourceStream()->NotifyListenersEventImpl(mEvent);
      }
    MediaStreamListener::MediaStreamGraphEvent mEvent;
  };
  GraphImpl()->AppendMessage(new Message(this, aNewEvent));
}

void
SourceMediaStream::AddDirectListener(MediaStreamDirectListener* aListener)
{
  bool wasEmpty;
  {
    MutexAutoLock lock(mMutex);
    wasEmpty = mDirectListeners.IsEmpty();
    mDirectListeners.AppendElement(aListener);
  }

  if (wasEmpty) {
    // Async
    NotifyListenersEvent(MediaStreamListener::EVENT_HAS_DIRECT_LISTENERS);
  }
}

void
SourceMediaStream::RemoveDirectListener(MediaStreamDirectListener* aListener)
{
  bool isEmpty;
  {
    MutexAutoLock lock(mMutex);
    mDirectListeners.RemoveElement(aListener);
    isEmpty = mDirectListeners.IsEmpty();
  }

  if (isEmpty) {
    // Async
    NotifyListenersEvent(MediaStreamListener::EVENT_HAS_NO_DIRECT_LISTENERS);
  }
}

StreamTime
SourceMediaStream::GetEndOfAppendedData(TrackID aID)
{
  MutexAutoLock lock(mMutex);
  TrackData *track = FindDataForTrack(aID);
  if (track) {
    return track->mEndOfFlushedData + track->mData->GetDuration();
  }
  NS_ERROR("Track not found");
  return 0;
}

void
SourceMediaStream::EndTrack(TrackID aID)
{
  MutexAutoLock lock(mMutex);
  TrackData *track = FindDataForTrack(aID);
  if (track) {
    track->mCommands |= TRACK_END;
  }
  if (auto graph = GraphImpl()) {
    graph->EnsureNextIteration();
  }
}

void
SourceMediaStream::AdvanceKnownTracksTime(StreamTime aKnownTime)
{
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(aKnownTime >= mUpdateKnownTracksTime);
  mUpdateKnownTracksTime = aKnownTime;
  if (auto graph = GraphImpl()) {
    graph->EnsureNextIteration();
  }
}

void
SourceMediaStream::FinishWithLockHeld()
{
  mMutex.AssertCurrentThreadOwns();
  mUpdateFinished = true;
  if (auto graph = GraphImpl()) {
    graph->EnsureNextIteration();
  }
}

void
SourceMediaStream::EndAllTrackAndFinish()
{
  MutexAutoLock lock(mMutex);
  for (uint32_t i = 0; i < mUpdateTracks.Length(); ++i) {
    SourceMediaStream::TrackData* data = &mUpdateTracks[i];
    data->mCommands |= TRACK_END;
  }
  mPendingTracks.Clear();
  FinishWithLockHeld();
  // we will call NotifyEvent() to let GetUserMedia know
}

void
SourceMediaStream::RegisterForAudioMixing()
{
  MutexAutoLock lock(mMutex);
  mNeedsMixing = true;
}

bool
SourceMediaStream::NeedsMixing()
{
  MutexAutoLock lock(mMutex);
  return mNeedsMixing;
}

void
MediaInputPort::Init()
{
  STREAM_LOG(LogLevel::Debug, ("Adding MediaInputPort %p (from %p to %p) to the graph",
             this, mSource, mDest));
  mSource->AddConsumer(this);
  mDest->AddInput(this);
  // mPortCount decremented via MediaInputPort::Destroy's message
  ++mDest->GraphImpl()->mPortCount;
}

void
MediaInputPort::Disconnect()
{
  GraphImpl()->AssertOnGraphThreadOrNotRunning();
  NS_ASSERTION(!mSource == !mDest,
               "mSource must either both be null or both non-null");
  if (!mSource)
    return;

  mSource->RemoveConsumer(this);
  mDest->RemoveInput(this);
  mSource = nullptr;
  mDest = nullptr;

  GraphImpl()->SetStreamOrderDirty();
}

MediaInputPort::InputInterval
MediaInputPort::GetNextInputInterval(GraphTime aTime)
{
  InputInterval result = { GRAPH_TIME_MAX, GRAPH_TIME_MAX, false };
  if (aTime >= mDest->mStartBlocking) {
    return result;
  }
  result.mStart = aTime;
  result.mEnd = mDest->mStartBlocking;
  result.mInputIsBlocked = aTime >= mSource->mStartBlocking;
  if (!result.mInputIsBlocked) {
    result.mEnd = std::min(result.mEnd, mSource->mStartBlocking);
  }
  return result;
}

void
MediaInputPort::Destroy()
{
  class Message : public ControlMessage {
  public:
    explicit Message(MediaInputPort* aPort)
      : ControlMessage(nullptr), mPort(aPort) {}
    virtual void Run()
    {
      mPort->Disconnect();
      --mPort->GraphImpl()->mPortCount;
      mPort->SetGraphImpl(nullptr);
      NS_RELEASE(mPort);
    }
    virtual void RunDuringShutdown()
    {
      Run();
    }
    MediaInputPort* mPort;
  };
  GraphImpl()->AppendMessage(new Message(this));
}

MediaStreamGraphImpl*
MediaInputPort::GraphImpl()
{
  return mGraph;
}

MediaStreamGraph*
MediaInputPort::Graph()
{
  return mGraph;
}

void
MediaInputPort::SetGraphImpl(MediaStreamGraphImpl* aGraph)
{
  MOZ_ASSERT(!mGraph || !aGraph, "Should only be set once");
  mGraph = aGraph;
}

void
MediaInputPort::BlockTrackIdImpl(TrackID aTrackId)
{
  mBlockedTracks.AppendElement(aTrackId);
}

void
MediaInputPort::BlockTrackId(TrackID aTrackId)
{
  class Message : public ControlMessage {
  public:
    explicit Message(MediaInputPort* aPort, TrackID aTrackId)
      : ControlMessage(aPort->GetDestination()),
        mPort(aPort), mTrackId(aTrackId) {}
    virtual void Run()
    {
      mPort->BlockTrackIdImpl(mTrackId);
    }
    virtual void RunDuringShutdown()
    {
      Run();
    }
    RefPtr<MediaInputPort> mPort;
    TrackID mTrackId;
  };

  MOZ_ASSERT(aTrackId != TRACK_NONE && aTrackId != TRACK_INVALID && aTrackId != TRACK_ANY,
             "Only explicit TrackID is allowed");
  GraphImpl()->AppendMessage(new Message(this, aTrackId));
}

already_AddRefed<MediaInputPort>
ProcessedMediaStream::AllocateInputPort(MediaStream* aStream, TrackID aTrackID,
                                        uint16_t aInputNumber, uint16_t aOutputNumber)
{
  // This method creates two references to the MediaInputPort: one for
  // the main thread, and one for the MediaStreamGraph.
  class Message : public ControlMessage {
  public:
    explicit Message(MediaInputPort* aPort)
      : ControlMessage(aPort->GetDestination()),
        mPort(aPort) {}
    virtual void Run()
    {
      mPort->Init();
      // The graph holds its reference implicitly
      mPort->GraphImpl()->SetStreamOrderDirty();
      Unused << mPort.forget();
    }
    virtual void RunDuringShutdown()
    {
      Run();
    }
    RefPtr<MediaInputPort> mPort;
  };

  MOZ_ASSERT(aTrackID != TRACK_NONE && aTrackID != TRACK_INVALID,
             "Only TRACK_ANY and explicit ID are allowed");
  RefPtr<MediaInputPort> port = new MediaInputPort(aStream, aTrackID, this,
                                                     aInputNumber, aOutputNumber);
  port->SetGraphImpl(GraphImpl());
  GraphImpl()->AppendMessage(new Message(port));
  return port.forget();
}

void
ProcessedMediaStream::Finish()
{
  class Message : public ControlMessage {
  public:
    explicit Message(ProcessedMediaStream* aStream)
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
      static_cast<ProcessedMediaStream*>(mStream)->SetAutofinishImpl(mAutofinish);
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
  // The stream order is only important if there are connections, in which
  // case MediaInputPort::Disconnect() called SetStreamOrderDirty().
  // MediaStreamGraphImpl::RemoveStreamGraphThread() will also call
  // SetStreamOrderDirty(), for other reasons.
}

MediaStreamGraphImpl::MediaStreamGraphImpl(GraphDriverType aDriverRequested,
                                           TrackRate aSampleRate,
                                           dom::AudioChannel aChannel)
  : MediaStreamGraph(aSampleRate)
  , mPortCount(0)
  , mNeedAnotherIteration(false)
  , mGraphDriverAsleep(false)
  , mMonitor("MediaStreamGraphImpl")
  , mLifecycleState(LIFECYCLE_THREAD_NOT_STARTED)
  , mEndTime(GRAPH_TIME_MAX)
  , mForceShutDown(false)
  , mPostedRunInStableStateEvent(false)
  , mDetectedNotRunning(false)
  , mPostedRunInStableState(false)
  , mRealtime(aDriverRequested != OFFLINE_THREAD_DRIVER)
  , mNonRealtimeProcessing(false)
  , mStreamOrderDirty(false)
  , mLatencyLog(AsyncLatencyLogger::Get())
#ifdef MOZ_WEBRTC
  , mFarendObserverRef(nullptr)
#endif
  , mMemoryReportMonitor("MSGIMemory")
  , mSelfRef(this)
  , mAudioStreamSizes()
  , mNeedsMemoryReport(false)
#ifdef DEBUG
  , mCanRunMessagesSynchronously(false)
#endif
  , mAudioChannel(aChannel)
{
  if (mRealtime) {
    if (aDriverRequested == AUDIO_THREAD_DRIVER) {
      AudioCallbackDriver* driver = new AudioCallbackDriver(this);
      mDriver = driver;
      mMixer.AddCallback(driver);
    } else {
      mDriver = new SystemClockDriver(this);
    }
  } else {
    mDriver = new OfflineClockDriver(this, MEDIA_GRAPH_TARGET_PERIOD_MS);
  }

  mLastMainThreadUpdate = TimeStamp::Now();

  RegisterWeakMemoryReporter(this);
}

void
MediaStreamGraphImpl::Destroy()
{
  // First unregister from memory reporting.
  UnregisterWeakMemoryReporter(this);

  // Clear the self reference which will destroy this instance.
  mSelfRef = nullptr;
}

NS_IMPL_ISUPPORTS(MediaStreamGraphShutdownObserver, nsIObserver)

static bool gShutdownObserverRegistered = false;

NS_IMETHODIMP
MediaStreamGraphShutdownObserver::Observe(nsISupports *aSubject,
                                          const char *aTopic,
                                          const char16_t *aData)
{
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    for (auto iter = gGraphs.Iter(); !iter.Done(); iter.Next()) {
      MediaStreamGraphImpl* graph = iter.UserData();
      graph->ForceShutDown();
    }
    nsContentUtils::UnregisterShutdownObserver(this);
    gShutdownObserverRegistered = false;
  }
  return NS_OK;
}

MediaStreamGraph*
MediaStreamGraph::GetInstance(MediaStreamGraph::GraphDriverType aGraphDriverRequested,
                              dom::AudioChannel aChannel)
{
  NS_ASSERTION(NS_IsMainThread(), "Main thread only");

  uint32_t channel = static_cast<uint32_t>(aChannel);
  MediaStreamGraphImpl* graph = nullptr;

  if (!gGraphs.Get(channel, &graph)) {
    if (!gShutdownObserverRegistered) {
      gShutdownObserverRegistered = true;
      nsContentUtils::RegisterShutdownObserver(new MediaStreamGraphShutdownObserver());
    }

    CubebUtils::InitPreferredSampleRate();

    graph = new MediaStreamGraphImpl(aGraphDriverRequested,
                                     CubebUtils::PreferredSampleRate(),
                                     aChannel);

    gGraphs.Put(channel, graph);

    STREAM_LOG(LogLevel::Debug,
        ("Starting up MediaStreamGraph %p for channel %s",
         graph, AudioChannelValues::strings[channel]));
  }

  return graph;
}

MediaStreamGraph*
MediaStreamGraph::CreateNonRealtimeInstance(TrackRate aSampleRate)
{
  NS_ASSERTION(NS_IsMainThread(), "Main thread only");

  MediaStreamGraphImpl* graph =
    new MediaStreamGraphImpl(OFFLINE_THREAD_DRIVER,
                             aSampleRate,
                             AudioChannel::Normal);

  STREAM_LOG(LogLevel::Debug, ("Starting up Offline MediaStreamGraph %p", graph));

  return graph;
}

void
MediaStreamGraph::DestroyNonRealtimeInstance(MediaStreamGraph* aGraph)
{
  NS_ASSERTION(NS_IsMainThread(), "Main thread only");
  MOZ_ASSERT(aGraph->IsNonRealtime(), "Should not destroy the global graph here");

  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(aGraph);
  if (graph->mForceShutDown)
    return; // already done

  if (!graph->mNonRealtimeProcessing) {
    // Start the graph, but don't produce anything
    graph->StartNonRealtimeProcessing(0);
  }
  graph->ForceShutDown();
}

NS_IMPL_ISUPPORTS(MediaStreamGraphImpl, nsIMemoryReporter)

struct ArrayClearer
{
  explicit ArrayClearer(nsTArray<AudioNodeSizes>& aArray) : mArray(aArray) {}
  ~ArrayClearer() { mArray.Clear(); }
  nsTArray<AudioNodeSizes>& mArray;
};

NS_IMETHODIMP
MediaStreamGraphImpl::CollectReports(nsIHandleReportCallback* aHandleReport,
                                     nsISupports* aData, bool aAnonymize)
{
  // Clears out the report array after we're done with it.
  ArrayClearer reportCleanup(mAudioStreamSizes);

  {
    MonitorAutoLock memoryReportLock(mMemoryReportMonitor);
    mNeedsMemoryReport = true;

    {
      // Wake up the MSG thread if it's real time (Offline graphs can't be
      // sleeping).
      MonitorAutoLock monitorLock(mMonitor);
      if (!CurrentDriver()->AsOfflineClockDriver()) {
        CurrentDriver()->WakeUp();
      }
    }

    if (mLifecycleState >= LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN) {
      // Shutting down, nothing to report.
      return NS_OK;
    }

    // Wait for up to one second for the report to complete.
    nsresult rv;
    const PRIntervalTime kMaxWait = PR_SecondsToInterval(1);
    while ((rv = memoryReportLock.Wait(kMaxWait)) != NS_OK) {
      if (PR_GetError() != PR_PENDING_INTERRUPT_ERROR) {
        return rv;
      }
    }
  }

#define REPORT(_path, _amount, _desc)                                       \
  do {                                                                      \
    nsresult rv;                                                            \
    rv = aHandleReport->Callback(EmptyCString(), _path,                     \
                                 KIND_HEAP, UNITS_BYTES, _amount,           \
                                 NS_LITERAL_CSTRING(_desc), aData);         \
    NS_ENSURE_SUCCESS(rv, rv);                                              \
  } while (0)

  for (size_t i = 0; i < mAudioStreamSizes.Length(); i++) {
    const AudioNodeSizes& usage = mAudioStreamSizes[i];
    const char* const nodeType =  usage.mNodeType.IsEmpty() ?
                                  "<unknown>" : usage.mNodeType.get();

    nsPrintfCString domNodePath("explicit/webaudio/audio-node/%s/dom-nodes",
                                nodeType);
    REPORT(domNodePath, usage.mDomNode,
           "Memory used by AudioNode DOM objects (Web Audio).");

    nsPrintfCString enginePath("explicit/webaudio/audio-node/%s/engine-objects",
                               nodeType);
    REPORT(enginePath, usage.mEngine,
           "Memory used by AudioNode engine objects (Web Audio).");

    nsPrintfCString streamPath("explicit/webaudio/audio-node/%s/stream-objects",
                               nodeType);
    REPORT(streamPath, usage.mStream,
           "Memory used by AudioNode stream objects (Web Audio).");

  }

  size_t hrtfLoaders = WebCore::HRTFDatabaseLoader::sizeOfLoaders(MallocSizeOf);
  if (hrtfLoaders) {

    REPORT(NS_LITERAL_CSTRING(
              "explicit/webaudio/audio-node/PannerNode/hrtf-databases"),
           hrtfLoaders,
           "Memory used by PannerNode databases (Web Audio).");
  }

#undef REPORT

  return NS_OK;
}

SourceMediaStream*
MediaStreamGraph::CreateSourceStream(DOMMediaStream* aWrapper)
{
  SourceMediaStream* stream = new SourceMediaStream(aWrapper);
  AddStream(stream);
  return stream;
}

ProcessedMediaStream*
MediaStreamGraph::CreateTrackUnionStream(DOMMediaStream* aWrapper)
{
  TrackUnionStream* stream = new TrackUnionStream(aWrapper);
  AddStream(stream);
  return stream;
}

ProcessedMediaStream*
MediaStreamGraph::CreateAudioCaptureStream(DOMMediaStream* aWrapper,
                                           TrackID aTrackId)
{
  AudioCaptureStream* stream = new AudioCaptureStream(aWrapper, aTrackId);
  AddStream(stream);
  return stream;
}

void
MediaStreamGraph::AddStream(MediaStream* aStream)
{
  NS_ADDREF(aStream);
  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(this);
  aStream->SetGraphImpl(graph);
  graph->AppendMessage(new CreateMessage(aStream));
}

class GraphStartedRunnable final : public nsRunnable
{
public:
  GraphStartedRunnable(AudioNodeStream* aStream, MediaStreamGraph* aGraph)
  : mStream(aStream)
  , mGraph(aGraph)
  { }

  NS_IMETHOD Run() {
    mGraph->NotifyWhenGraphStarted(mStream);
    return NS_OK;
  }

private:
  RefPtr<AudioNodeStream> mStream;
  MediaStreamGraph* mGraph;
};

void
MediaStreamGraph::NotifyWhenGraphStarted(AudioNodeStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());

  class GraphStartedNotificationControlMessage : public ControlMessage
  {
  public:
    explicit GraphStartedNotificationControlMessage(AudioNodeStream* aStream)
      : ControlMessage(aStream)
    {
    }
    virtual void Run()
    {
      // This runs on the graph thread, so when this runs, and the current
      // driver is an AudioCallbackDriver, we know the audio hardware is
      // started. If not, we are going to switch soon, keep reposting this
      // ControlMessage.
      MediaStreamGraphImpl* graphImpl = mStream->GraphImpl();
      if (graphImpl->CurrentDriver()->AsAudioCallbackDriver()) {
        nsCOMPtr<nsIRunnable> event = new dom::StateChangeTask(
            mStream->AsAudioNodeStream(), nullptr, AudioContextState::Running);
        NS_DispatchToMainThread(event.forget());
      } else {
        nsCOMPtr<nsIRunnable> event = new GraphStartedRunnable(
            mStream->AsAudioNodeStream(), mStream->Graph());
        NS_DispatchToMainThread(event.forget());
      }
    }
    virtual void RunDuringShutdown()
    {
    }
  };

  if (!aStream->IsDestroyed()) {
    MediaStreamGraphImpl* graphImpl = static_cast<MediaStreamGraphImpl*>(this);
    graphImpl->AppendMessage(new GraphStartedNotificationControlMessage(aStream));
  }
}

void
MediaStreamGraphImpl::IncrementSuspendCount(MediaStream* aStream)
{
  if (!aStream->IsSuspended()) {
    MOZ_ASSERT(mStreams.Contains(aStream));
    mStreams.RemoveElement(aStream);
    mSuspendedStreams.AppendElement(aStream);
    SetStreamOrderDirty();
  }
  aStream->IncrementSuspendCount();
}

void
MediaStreamGraphImpl::DecrementSuspendCount(MediaStream* aStream)
{
  bool wasSuspended = aStream->IsSuspended();
  aStream->DecrementSuspendCount();
  if (wasSuspended && !aStream->IsSuspended()) {
    MOZ_ASSERT(mSuspendedStreams.Contains(aStream));
    mSuspendedStreams.RemoveElement(aStream);
    mStreams.AppendElement(aStream);
    ProcessedMediaStream* ps = aStream->AsProcessedStream();
    if (ps) {
      ps->mCycleMarker = NOT_VISITED;
    }
    SetStreamOrderDirty();
  }
}

void
MediaStreamGraphImpl::SuspendOrResumeStreams(AudioContextOperation aAudioContextOperation,
                                             const nsTArray<MediaStream*>& aStreamSet)
{
  // For our purpose, Suspend and Close are equivalent: we want to remove the
  // streams from the set of streams that are going to be processed.
  for (MediaStream* stream : aStreamSet) {
    if (aAudioContextOperation == AudioContextOperation::Resume) {
      DecrementSuspendCount(stream);
    } else {
      IncrementSuspendCount(stream);
    }
  }
  STREAM_LOG(LogLevel::Debug, ("Moving streams between suspended and running"
      "state: mStreams: %d, mSuspendedStreams: %d\n", mStreams.Length(),
      mSuspendedStreams.Length()));
#ifdef DEBUG
  // The intersection of the two arrays should be null.
  for (uint32_t i = 0; i < mStreams.Length(); i++) {
    for (uint32_t j = 0; j < mSuspendedStreams.Length(); j++) {
      MOZ_ASSERT(
        mStreams[i] != mSuspendedStreams[j],
        "The suspended stream set and running stream set are not disjoint.");
    }
  }
#endif
}

void
MediaStreamGraphImpl::AudioContextOperationCompleted(MediaStream* aStream,
                                                     void* aPromise,
                                                     AudioContextOperation aOperation)
{
  // This can be called from the thread created to do cubeb operation, or the
  // MSG thread. The pointers passed back here are refcounted, so are still
  // alive.
  MonitorAutoLock lock(mMonitor);

  AudioContextState state;
  switch (aOperation) {
    case AudioContextOperation::Suspend:
      state = AudioContextState::Suspended;
      break;
    case AudioContextOperation::Resume:
      state = AudioContextState::Running;
      break;
    case AudioContextOperation::Close:
      state = AudioContextState::Closed;
      break;
    default: MOZ_CRASH("Not handled.");
  }

  nsCOMPtr<nsIRunnable> event = new dom::StateChangeTask(
      aStream->AsAudioNodeStream(), aPromise, state);
  NS_DispatchToMainThread(event.forget());
}

void
MediaStreamGraphImpl::ApplyAudioContextOperationImpl(
    MediaStream* aDestinationStream, const nsTArray<MediaStream*>& aStreams,
    AudioContextOperation aOperation, void* aPromise)
{
  MOZ_ASSERT(CurrentDriver()->OnThread());

  SuspendOrResumeStreams(aOperation, aStreams);

  // If we have suspended the last AudioContext, and we don't have other
  // streams that have audio, this graph will automatically switch to a
  // SystemCallbackDriver, because it can't find a MediaStream that has an audio
  // track. When resuming, force switching to an AudioCallbackDriver (if we're
  // not already switching). It would have happened at the next iteration
  // anyways, but doing this now save some time.
  if (aOperation == AudioContextOperation::Resume) {
    if (!CurrentDriver()->AsAudioCallbackDriver()) {
      AudioCallbackDriver* driver;
      if (CurrentDriver()->Switching()) {
        MOZ_ASSERT(CurrentDriver()->NextDriver()->AsAudioCallbackDriver());
        driver = CurrentDriver()->NextDriver()->AsAudioCallbackDriver();
      } else {
        driver = new AudioCallbackDriver(this);
        mMixer.AddCallback(driver);
        CurrentDriver()->SwitchAtNextIteration(driver);
      }
      driver->EnqueueStreamAndPromiseForOperation(aDestinationStream,
          aPromise, aOperation);
    } else {
      // We are resuming a context, but we are already using an
      // AudioCallbackDriver, we can resolve the promise now.
      AudioContextOperationCompleted(aDestinationStream, aPromise, aOperation);
    }
  }
  // Close, suspend: check if we are going to switch to a
  // SystemAudioCallbackDriver, and pass the promise to the AudioCallbackDriver
  // if that's the case, so it can notify the content.
  // This is the same logic as in UpdateStreamOrder, but it's simpler to have it
  // here as well so we don't have to store the Promise(s) on the Graph.
  if (aOperation != AudioContextOperation::Resume) {
    bool audioTrackPresent = false;
    for (uint32_t i = 0; i < mStreams.Length(); ++i) {
      MediaStream* stream = mStreams[i];
      if (stream->AsAudioNodeStream()) {
        audioTrackPresent = true;
      }
      for (StreamBuffer::TrackIter tracks(stream->GetStreamBuffer(), MediaSegment::AUDIO);
          !tracks.IsEnded(); tracks.Next()) {
        audioTrackPresent = true;
      }
    }
    if (!audioTrackPresent && CurrentDriver()->AsAudioCallbackDriver()) {
      CurrentDriver()->AsAudioCallbackDriver()->
        EnqueueStreamAndPromiseForOperation(aDestinationStream, aPromise,
                                            aOperation);

      SystemClockDriver* driver;
      if (CurrentDriver()->NextDriver()) {
        MOZ_ASSERT(!CurrentDriver()->NextDriver()->AsAudioCallbackDriver());
      } else {
        driver = new SystemClockDriver(this);
        mMixer.RemoveCallback(CurrentDriver()->AsAudioCallbackDriver());
        CurrentDriver()->SwitchAtNextIteration(driver);
      }
      // We are closing or suspending an AudioContext, but we just got resumed.
      // Queue the operation on the next driver so that the ordering is
      // preserved.
    } else if (!audioTrackPresent && CurrentDriver()->Switching()) {
      MOZ_ASSERT(CurrentDriver()->NextDriver()->AsAudioCallbackDriver());
      CurrentDriver()->NextDriver()->AsAudioCallbackDriver()->
        EnqueueStreamAndPromiseForOperation(aDestinationStream, aPromise,
                                            aOperation);
    } else {
      // We are closing or suspending an AudioContext, but something else is
      // using the audio stream, we can resolve the promise now.
      AudioContextOperationCompleted(aDestinationStream, aPromise, aOperation);
    }
  }
}

void
MediaStreamGraph::ApplyAudioContextOperation(MediaStream* aDestinationStream,
                                             const nsTArray<MediaStream*>& aStreams,
                                             AudioContextOperation aOperation,
                                             void* aPromise)
{
  class AudioContextOperationControlMessage : public ControlMessage
  {
  public:
    AudioContextOperationControlMessage(MediaStream* aDestinationStream,
                                        const nsTArray<MediaStream*>& aStreams,
                                        AudioContextOperation aOperation,
                                        void* aPromise)
      : ControlMessage(aDestinationStream)
      , mStreams(aStreams)
      , mAudioContextOperation(aOperation)
      , mPromise(aPromise)
    {
    }
    virtual void Run()
    {
      mStream->GraphImpl()->ApplyAudioContextOperationImpl(mStream,
        mStreams, mAudioContextOperation, mPromise);
    }
    virtual void RunDuringShutdown()
    {
      MOZ_ASSERT(false, "We should be reviving the graph?");
    }

  private:
    // We don't need strong references here for the same reason ControlMessage
    // doesn't.
    nsTArray<MediaStream*> mStreams;
    AudioContextOperation mAudioContextOperation;
    void* mPromise;
  };

  MediaStreamGraphImpl* graphImpl = static_cast<MediaStreamGraphImpl*>(this);
  graphImpl->AppendMessage(
    new AudioContextOperationControlMessage(aDestinationStream, aStreams,
                                            aOperation, aPromise));
}

bool
MediaStreamGraph::IsNonRealtime() const
{
  const MediaStreamGraphImpl* impl = static_cast<const MediaStreamGraphImpl*>(this);
  MediaStreamGraphImpl* graph;

  return !gGraphs.Get(uint32_t(impl->AudioChannel()), &graph) || graph != impl;
}

void
MediaStreamGraph::StartNonRealtimeProcessing(uint32_t aTicksToProcess)
{
  NS_ASSERTION(NS_IsMainThread(), "main thread only");

  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(this);
  NS_ASSERTION(!graph->mRealtime, "non-realtime only");

  if (graph->mNonRealtimeProcessing)
    return;

  graph->mEndTime =
    graph->RoundUpToNextAudioBlock(graph->mStateComputedTime +
                                   aTicksToProcess - 1);
  graph->mNonRealtimeProcessing = true;
  graph->EnsureRunInStableState();
}

void
ProcessedMediaStream::AddInput(MediaInputPort* aPort)
{
  mInputs.AppendElement(aPort);
  GraphImpl()->SetStreamOrderDirty();
}

void
MediaStreamGraph::RegisterCaptureStreamForWindow(
    uint64_t aWindowId, ProcessedMediaStream* aCaptureStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  MediaStreamGraphImpl* graphImpl = static_cast<MediaStreamGraphImpl*>(this);
  graphImpl->RegisterCaptureStreamForWindow(aWindowId, aCaptureStream);
}

void
MediaStreamGraphImpl::RegisterCaptureStreamForWindow(
  uint64_t aWindowId, ProcessedMediaStream* aCaptureStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  WindowAndStream winAndStream;
  winAndStream.mWindowId = aWindowId;
  winAndStream.mCaptureStreamSink = aCaptureStream;
  mWindowCaptureStreams.AppendElement(winAndStream);
}

void
MediaStreamGraph::UnregisterCaptureStreamForWindow(uint64_t aWindowId)
{
  MOZ_ASSERT(NS_IsMainThread());
  MediaStreamGraphImpl* graphImpl = static_cast<MediaStreamGraphImpl*>(this);
  graphImpl->UnregisterCaptureStreamForWindow(aWindowId);
}

void
MediaStreamGraphImpl::UnregisterCaptureStreamForWindow(uint64_t aWindowId)
{
  MOZ_ASSERT(NS_IsMainThread());
  for (uint32_t i = 0; i < mWindowCaptureStreams.Length(); i++) {
    if (mWindowCaptureStreams[i].mWindowId == aWindowId) {
      mWindowCaptureStreams.RemoveElementAt(i);
    }
  }
}

already_AddRefed<MediaInputPort>
MediaStreamGraph::ConnectToCaptureStream(uint64_t aWindowId,
                                         MediaStream* aMediaStream)
{
  return aMediaStream->GraphImpl()->ConnectToCaptureStream(aWindowId,
                                                           aMediaStream);
}

already_AddRefed<MediaInputPort>
MediaStreamGraphImpl::ConnectToCaptureStream(uint64_t aWindowId,
                                             MediaStream* aMediaStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  for (uint32_t i = 0; i < mWindowCaptureStreams.Length(); i++) {
    if (mWindowCaptureStreams[i].mWindowId == aWindowId) {
      ProcessedMediaStream* sink = mWindowCaptureStreams[i].mCaptureStreamSink;
      return sink->AllocateInputPort(aMediaStream);
    }
  }
  return nullptr;
}

} // namespace mozilla
