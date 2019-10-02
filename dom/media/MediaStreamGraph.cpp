/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamGraphImpl.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Unused.h"

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
#include "AudioNodeStream.h"
#include "AudioNodeExternalInputStream.h"
#include "MediaStreamListener.h"
#include "mozilla/dom/BaseAudioContextBinding.h"
#include "mozilla/media/MediaUtils.h"
#include <algorithm>
#include "GeckoProfiler.h"
#include "VideoFrameContainer.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Unused.h"
#include "mtransport/runnable_utils.h"
#include "VideoUtils.h"
#include "GraphRunner.h"
#include "Tracing.h"

#include "webaudio/blink/DenormalDisabler.h"
#include "webaudio/blink/HRTFDatabaseLoader.h"

using namespace mozilla::layers;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::media;

mozilla::AsyncLogger gMSGTraceLogger("MSGTracing");

namespace mozilla {

LazyLogModule gMediaStreamGraphLog("MediaStreamGraph");
#ifdef LOG
#  undef LOG
#endif  // LOG
#define LOG(type, msg) MOZ_LOG(gMediaStreamGraphLog, type, msg)

/**
 * A hash table containing the graph instances, one per document.
 *
 * The key is a hash of nsPIDOMWindowInner, see `WindowToHash`.
 */
static nsDataHashtable<nsUint32HashKey, MediaStreamGraphImpl*> gGraphs;

MediaStreamGraphImpl::~MediaStreamGraphImpl() {
  MOZ_ASSERT(mStreams.IsEmpty() && mSuspendedStreams.IsEmpty(),
             "All streams should have been destroyed by messages from the main "
             "thread");
  LOG(LogLevel::Debug, ("MediaStreamGraph %p destroyed", this));
  LOG(LogLevel::Debug, ("MediaStreamGraphImpl::~MediaStreamGraphImpl"));

#ifdef TRACING
  gMSGTraceLogger.Stop();
#endif
}

void MediaStreamGraphImpl::AddStreamGraphThread(MediaStream* aStream) {
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  aStream->mStartTime = mProcessedTime;

  if (aStream->IsSuspended()) {
    mSuspendedStreams.AppendElement(aStream);
    LOG(LogLevel::Debug,
        ("%p: Adding media stream %p, in the suspended stream array", this,
         aStream));
  } else {
    mStreams.AppendElement(aStream);
    LOG(LogLevel::Debug, ("%p:  Adding media stream %p, count %zu", this,
                          aStream, mStreams.Length()));
  }

  SetStreamOrderDirty();
}

void MediaStreamGraphImpl::RemoveStreamGraphThread(MediaStream* aStream) {
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
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

  LOG(LogLevel::Debug, ("%p: Removed media stream %p, count %zu", this, aStream,
                        mStreams.Length()));

  NS_RELEASE(aStream);  // probably destroying it
}

StreamTime MediaStreamGraphImpl::GraphTimeToStreamTimeWithBlocking(
    const MediaStream* aStream, GraphTime aTime) const {
  MOZ_ASSERT(
      aTime <= mStateComputedTime,
      "Don't ask about times where we haven't made blocking decisions yet");
  return std::max<StreamTime>(
      0, std::min(aTime, aStream->mStartBlocking) - aStream->mStartTime);
}

GraphTime MediaStreamGraphImpl::IterationEnd() const {
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  return CurrentDriver()->IterationEnd();
}

void MediaStreamGraphImpl::UpdateCurrentTimeForStreams(
    GraphTime aPrevCurrentTime) {
  MOZ_ASSERT(OnGraphThread());
  for (MediaStream* stream : AllStreams()) {
    // Shouldn't have already notified of ended *and* have output!
    MOZ_ASSERT_IF(stream->mStartBlocking > aPrevCurrentTime,
                  !stream->mNotifiedEnded);

    // Calculate blocked time and fire Blocked/Unblocked events
    GraphTime blockedTime = mStateComputedTime - stream->mStartBlocking;
    NS_ASSERTION(blockedTime >= 0, "Error in blocking time");
    stream->AdvanceTimeVaryingValuesToCurrentTime(mStateComputedTime,
                                                  blockedTime);
    LOG(LogLevel::Verbose,
        ("%p: MediaStream %p bufferStartTime=%f blockedTime=%f", this, stream,
         MediaTimeToSeconds(stream->mStartTime),
         MediaTimeToSeconds(blockedTime)));
    stream->mStartBlocking = mStateComputedTime;

    StreamTime streamCurrentTime =
        stream->GraphTimeToStreamTime(mStateComputedTime);
    if (stream->mEnded) {
      MOZ_ASSERT(stream->GetEnd() <= streamCurrentTime);
      if (!stream->mNotifiedEnded) {
        // Playout of this track ended and listeners have not been notified.
        stream->mNotifiedEnded = true;
        SetStreamOrderDirty();
        for (const auto& listener : stream->mTrackListeners) {
          listener->NotifyOutput(this, stream->GetEnd());
          listener->NotifyEnded(this);
        }
      }
    } else {
      for (const auto& listener : stream->mTrackListeners) {
        listener->NotifyOutput(this, streamCurrentTime);
      }
    }
  }
}

template <typename C, typename Chunk>
void MediaStreamGraphImpl::ProcessChunkMetadataForInterval(MediaStream* aStream,
                                                           C& aSegment,
                                                           StreamTime aStart,
                                                           StreamTime aEnd) {
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  MOZ_ASSERT(aStream);

  StreamTime offset = 0;
  for (typename C::ConstChunkIterator chunk(aSegment); !chunk.IsEnded();
       chunk.Next()) {
    if (offset >= aEnd) {
      break;
    }
    offset += chunk->GetDuration();
    if (chunk->IsNull() || offset < aStart) {
      continue;
    }
    const PrincipalHandle& principalHandle = chunk->GetPrincipalHandle();
    if (principalHandle != aSegment.GetLastPrincipalHandle()) {
      aSegment.SetLastPrincipalHandle(principalHandle);
      LOG(LogLevel::Debug,
          ("%p: MediaStream %p, principalHandle "
           "changed in %sChunk with duration %lld",
           this, aStream,
           aSegment.GetType() == MediaSegment::AUDIO ? "Audio" : "Video",
           (long long)chunk->GetDuration()));
      for (const auto& listener : aStream->mTrackListeners) {
        listener->NotifyPrincipalHandleChanged(this, principalHandle);
      }
    }
  }
}

void MediaStreamGraphImpl::ProcessChunkMetadata(GraphTime aPrevCurrentTime) {
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  for (MediaStream* stream : AllStreams()) {
    StreamTime iterationStart = stream->GraphTimeToStreamTime(aPrevCurrentTime);
    StreamTime iterationEnd = stream->GraphTimeToStreamTime(mProcessedTime);
    if (!stream->mSegment) {
      continue;
    }
    if (stream->mType == MediaSegment::AUDIO) {
      ProcessChunkMetadataForInterval<AudioSegment, AudioChunk>(
          stream, *stream->GetData<AudioSegment>(), iterationStart,
          iterationEnd);
    } else if (stream->mType == MediaSegment::VIDEO) {
      ProcessChunkMetadataForInterval<VideoSegment, VideoChunk>(
          stream, *stream->GetData<VideoSegment>(), iterationStart,
          iterationEnd);
    } else {
      MOZ_CRASH("Unknown track type");
    }
  }
}

GraphTime MediaStreamGraphImpl::WillUnderrun(MediaStream* aStream,
                                             GraphTime aEndBlockingDecisions) {
  // Ended tracks can't underrun. ProcessedMediaStreams also can't cause
  // underrun currently, since we'll always be able to produce data for them
  // unless they block on some other stream.
  if (aStream->mEnded || aStream->AsProcessedStream()) {
    return aEndBlockingDecisions;
  }
  // This stream isn't ended or suspended. We don't need to call
  // StreamTimeToGraphTime since an underrun is the only thing that can block
  // it.
  GraphTime bufferEnd = aStream->GetEnd() + aStream->mStartTime;
#ifdef DEBUG
  if (bufferEnd < mProcessedTime) {
    LOG(LogLevel::Error, ("%p: MediaStream %p underrun, "
                          "bufferEnd %f < mProcessedTime %f (%" PRId64
                          " < %" PRId64 "), Streamtime %" PRId64,
                          this, aStream, MediaTimeToSeconds(bufferEnd),
                          MediaTimeToSeconds(mProcessedTime), bufferEnd,
                          mProcessedTime, aStream->GetEnd()));
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
}  // namespace

bool MediaStreamGraphImpl::AudioTrackPresent() {
  MOZ_ASSERT(OnGraphThreadOrNotRunning());

  bool audioTrackPresent = false;
  for (MediaStream* stream : mStreams) {
    if (stream->AsAudioNodeStream()) {
      audioTrackPresent = true;
      break;
    }

    if (stream->mType == MediaSegment::AUDIO && !stream->mNotifiedEnded) {
      audioTrackPresent = true;
      break;
    }
  }

  // XXX For some reason, there are race conditions when starting an audio input
  // where we find no active audio tracks.  In any case, if we have an active
  // audio input we should not allow a switch back to a SystemClockDriver
  if (!audioTrackPresent && mInputDeviceUsers.Count() != 0) {
    NS_WARNING("No audio tracks, but full-duplex audio is enabled!!!!!");
    audioTrackPresent = true;
  }

  return audioTrackPresent;
}

void MediaStreamGraphImpl::UpdateStreamOrder() {
  MOZ_ASSERT(OnGraphThread());
  bool audioTrackPresent = AudioTrackPresent();

  // Note that this looks for any audio streams, input or output, and switches
  // to a SystemClockDriver if there are none.  However, if another is already
  // pending, let that switch happen.

  if (!audioTrackPresent && mRealtime &&
      CurrentDriver()->AsAudioCallbackDriver()) {
    MonitorAutoLock mon(mMonitor);
    if (CurrentDriver()->AsAudioCallbackDriver()->IsStarted() &&
        !(CurrentDriver()->Switching())) {
      if (LifecycleStateRef() == LIFECYCLE_RUNNING) {
        SystemClockDriver* driver = new SystemClockDriver(this);
        CurrentDriver()->SwitchAtNextIteration(driver);
      }
    }
  }

  bool switching = false;
  {
    MonitorAutoLock mon(mMonitor);
    switching = CurrentDriver()->Switching();
  }

  if (audioTrackPresent && mRealtime &&
      !CurrentDriver()->AsAudioCallbackDriver() && !switching) {
    MonitorAutoLock mon(mMonitor);
    if (LifecycleStateRef() == LIFECYCLE_RUNNING) {
      AudioCallbackDriver* driver = new AudioCallbackDriver(
          this, AudioInputChannelCount(), AudioInputDevicePreference());
      CurrentDriver()->SwitchAtNextIteration(driver);
    }
  }

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
  // [2]
  // https://github.com/scipy/scipy/blob/e2c502fca/scipy/sparse/csgraph/_traversal.pyx#L582
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
      for (uint32_t i = inputs.Length(); i--;) {
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
    for (uint32_t i = inputs.Length(); i--;) {
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
    while (next && static_cast<ProcessedMediaStream*>(next)->mCycleMarker <=
                       cycleStackMarker) {
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

void MediaStreamGraphImpl::CreateOrDestroyAudioStreams(MediaStream* aStream) {
  MOZ_ASSERT(OnGraphThread());
  MOZ_ASSERT(mRealtime,
             "Should only attempt to create audio streams in real-time mode");

  if (aStream->mAudioOutputs.IsEmpty()) {
    aStream->mAudioOutputStream = nullptr;
    return;
  }

  if (aStream->mAudioOutputStream) {
    return;
  }

  LOG(LogLevel::Debug,
      ("%p: Updating AudioOutputStream for MediaStream %p", this, aStream));

  aStream->mAudioOutputStream = MakeUnique<MediaStream::AudioOutputStream>();
  aStream->mAudioOutputStream->mAudioPlaybackStartTime = mProcessedTime;
  aStream->mAudioOutputStream->mBlockedAudioTime = 0;
  aStream->mAudioOutputStream->mLastTickWritten = 0;

  bool switching = false;
  {
    MonitorAutoLock lock(mMonitor);
    switching = CurrentDriver()->Switching();
  }

  if (!CurrentDriver()->AsAudioCallbackDriver() && !switching) {
    MonitorAutoLock mon(mMonitor);
    if (LifecycleStateRef() == LIFECYCLE_RUNNING) {
      AudioCallbackDriver* driver = new AudioCallbackDriver(
          this, AudioInputChannelCount(), AudioInputDevicePreference());
      CurrentDriver()->SwitchAtNextIteration(driver);
    }
  }
}

StreamTime MediaStreamGraphImpl::PlayAudio(MediaStream* aStream) {
  MOZ_ASSERT(OnGraphThread());
  MOZ_ASSERT(mRealtime, "Should only attempt to play audio in realtime mode");

  float volume = 0.0f;
  for (uint32_t i = 0; i < aStream->mAudioOutputs.Length(); ++i) {
    volume += aStream->mAudioOutputs[i].mVolume * mGlobalVolume;
  }

  StreamTime ticksWritten = 0;

  if (aStream->mAudioOutputStream) {
    ticksWritten = 0;

    MediaStream::AudioOutputStream& audioOutput = *aStream->mAudioOutputStream;
    AudioSegment* audio = aStream->GetData<AudioSegment>();
    AudioSegment output;

    StreamTime offset = aStream->GraphTimeToStreamTime(mProcessedTime);

    // We don't update aStream->mTracksStartTime here to account for time spent
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
        LOG(LogLevel::Verbose,
            ("%p: MediaStream %p writing %" PRId64
             " blocking-silence samples for "
             "%f to %f (%" PRId64 " to %" PRId64 ")",
             this, aStream, toWrite, MediaTimeToSeconds(t),
             MediaTimeToSeconds(end), offset, offset + toWrite));
      } else {
        StreamTime endTicksNeeded = offset + toWrite;
        StreamTime endTicksAvailable = audio->GetDuration();

        if (endTicksNeeded <= endTicksAvailable) {
          LOG(LogLevel::Verbose,
              ("%p: MediaStream %p writing %" PRId64 " samples for %f to %f "
               "(samples %" PRId64 " to %" PRId64 ")",
               this, aStream, toWrite, MediaTimeToSeconds(t),
               MediaTimeToSeconds(end), offset, endTicksNeeded));
          output.AppendSlice(*audio, offset, endTicksNeeded);
          ticksWritten += toWrite;
          offset = endTicksNeeded;
        } else {
          // MOZ_ASSERT(track->IsEnded(), "Not enough data, and track not
          // ended."); If we are at the end of the track, maybe write the
          // remaining samples, and pad with/output silence.
          if (endTicksNeeded > endTicksAvailable &&
              offset < endTicksAvailable) {
            output.AppendSlice(*audio, offset, endTicksAvailable);
            LOG(LogLevel::Verbose,
                ("%p: MediaStream %p writing %" PRId64 " samples for %f to %f "
                 "(samples %" PRId64 " to %" PRId64 ")",
                 this, aStream, toWrite, MediaTimeToSeconds(t),
                 MediaTimeToSeconds(end), offset, endTicksNeeded));
            uint32_t available = endTicksAvailable - offset;
            ticksWritten += available;
            toWrite -= available;
            offset = endTicksAvailable;
          }
          output.AppendNullData(toWrite);
          LOG(LogLevel::Verbose,
              ("%p MediaStream %p writing %" PRId64
               " padding slsamples for %f to "
               "%f (samples %" PRId64 " to %" PRId64 ")",
               this, aStream, toWrite, MediaTimeToSeconds(t),
               MediaTimeToSeconds(end), offset, endTicksNeeded));
          ticksWritten += toWrite;
        }
        output.ApplyVolume(volume);
      }
      t = end;
    }
    audioOutput.mLastTickWritten = offset;

    output.WriteTo(mMixer, AudioOutputChannelCount(), mSampleRate);
  }
  return ticksWritten;
}

void MediaStreamGraphImpl::OpenAudioInputImpl(CubebUtils::AudioDeviceID aID,
                                              AudioDataListener* aListener) {
  MOZ_ASSERT(OnGraphThread());
  // Only allow one device per MSG (hence, per document), but allow opening a
  // device multiple times
  nsTArray<RefPtr<AudioDataListener>>& listeners =
      mInputDeviceUsers.GetOrInsert(aID);
  if (listeners.IsEmpty() && mInputDeviceUsers.Count() > 1) {
    // We don't support opening multiple input device in a graph for now.
    listeners.RemoveElement(aID);
    return;
  }

  MOZ_ASSERT(!listeners.Contains(aListener), "Don't add a listener twice.");

  listeners.AppendElement(aListener);

  if (listeners.Length() == 1) {  // first open for this device
    mInputDeviceID = aID;
    // Switch Drivers since we're adding input (to input-only or full-duplex)
    MonitorAutoLock mon(mMonitor);
    if (LifecycleStateRef() == LIFECYCLE_RUNNING) {
      AudioCallbackDriver* driver = new AudioCallbackDriver(
          this, AudioInputChannelCount(), AudioInputDevicePreference());
      LOG(LogLevel::Debug,
          ("%p OpenAudioInput: starting new AudioCallbackDriver(input) %p",
           this, driver));
      CurrentDriver()->SwitchAtNextIteration(driver);
    } else {
      LOG(LogLevel::Error, ("OpenAudioInput in shutdown!"));
      MOZ_ASSERT_UNREACHABLE("Can't open cubeb inputs in shutdown");
    }
  }
}

nsresult MediaStreamGraphImpl::OpenAudioInput(CubebUtils::AudioDeviceID aID,
                                              AudioDataListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  class Message : public ControlMessage {
   public:
    Message(MediaStreamGraphImpl* aGraph, CubebUtils::AudioDeviceID aID,
            AudioDataListener* aListener)
        : ControlMessage(nullptr),
          mGraph(aGraph),
          mID(aID),
          mListener(aListener) {}
    void Run() override { mGraph->OpenAudioInputImpl(mID, mListener); }
    MediaStreamGraphImpl* mGraph;
    CubebUtils::AudioDeviceID mID;
    RefPtr<AudioDataListener> mListener;
  };
  // XXX Check not destroyed!
  this->AppendMessage(MakeUnique<Message>(this, aID, aListener));
  return NS_OK;
}

void MediaStreamGraphImpl::CloseAudioInputImpl(
    Maybe<CubebUtils::AudioDeviceID>& aID, AudioDataListener* aListener) {
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  // It is possible to not know the ID here, find it first.
  if (aID.isNothing()) {
    for (auto iter = mInputDeviceUsers.Iter(); !iter.Done(); iter.Next()) {
      if (iter.Data().Contains(aListener)) {
        aID = Some(iter.Key());
      }
    }
    MOZ_ASSERT(aID.isSome(), "Closing an audio input that was not opened.");
  }

  nsTArray<RefPtr<AudioDataListener>>* listeners =
      mInputDeviceUsers.GetValue(aID.value());

  MOZ_ASSERT(listeners);
  DebugOnly<bool> wasPresent = listeners->RemoveElement(aListener);
  MOZ_ASSERT(wasPresent);

  // Breaks the cycle between the MSG and the listener.
  aListener->Disconnect(this);

  if (!listeners->IsEmpty()) {
    // There is still a consumer for this audio input device
    return;
  }

  mInputDeviceID = nullptr;  // reset to default
  mInputDeviceUsers.Remove(aID.value());

  // Switch Drivers since we're adding or removing an input (to nothing/system
  // or output only)
  bool audioTrackPresent = AudioTrackPresent();

  MonitorAutoLock mon(mMonitor);
  if (LifecycleStateRef() == LIFECYCLE_RUNNING) {
    GraphDriver* driver;
    if (audioTrackPresent) {
      // We still have audio output
      LOG(LogLevel::Debug,
          ("%p: CloseInput: output present (AudioCallback)", this));

      driver = new AudioCallbackDriver(this, AudioInputChannelCount(),
                                       AudioInputDevicePreference());
      CurrentDriver()->SwitchAtNextIteration(driver);
    } else if (CurrentDriver()->AsAudioCallbackDriver()) {
      LOG(LogLevel::Debug,
          ("%p: CloseInput: no output present (SystemClockCallback)", this));

      driver = new SystemClockDriver(this);
      CurrentDriver()->SwitchAtNextIteration(driver);
    }  // else SystemClockDriver->SystemClockDriver, no switch
  }
}

void MediaStreamGraphImpl::CloseAudioInput(
    Maybe<CubebUtils::AudioDeviceID>& aID, AudioDataListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  class Message : public ControlMessage {
   public:
    Message(MediaStreamGraphImpl* aGraph, Maybe<CubebUtils::AudioDeviceID>& aID,
            AudioDataListener* aListener)
        : ControlMessage(nullptr),
          mGraph(aGraph),
          mID(aID),
          mListener(aListener) {}
    void Run() override { mGraph->CloseAudioInputImpl(mID, mListener); }
    MediaStreamGraphImpl* mGraph;
    Maybe<CubebUtils::AudioDeviceID> mID;
    RefPtr<AudioDataListener> mListener;
  };
  this->AppendMessage(MakeUnique<Message>(this, aID, aListener));
}

// All AudioInput listeners get the same speaker data (at least for now).
void MediaStreamGraphImpl::NotifyOutputData(AudioDataValue* aBuffer,
                                            size_t aFrames, TrackRate aRate,
                                            uint32_t aChannels) {
#ifdef ANDROID
  // On Android, mInputDeviceID is always null and represents the default
  // device.
  // The absence of an input consumer is enough to know we need to bail out
  // here.
  if (!mInputDeviceUsers.GetValue(mInputDeviceID)) {
    return;
  }
#else
  if (!mInputDeviceID) {
    return;
  }
#endif
  // When/if we decide to support multiple input devices per graph, this needs
  // to loop over them.
  nsTArray<RefPtr<AudioDataListener>>* listeners =
      mInputDeviceUsers.GetValue(mInputDeviceID);
  MOZ_ASSERT(listeners);
  for (auto& listener : *listeners) {
    listener->NotifyOutputData(this, aBuffer, aFrames, aRate, aChannels);
  }
}

void MediaStreamGraphImpl::NotifyInputData(const AudioDataValue* aBuffer,
                                           size_t aFrames, TrackRate aRate,
                                           uint32_t aChannels) {
#ifdef ANDROID
  if (!mInputDeviceUsers.GetValue(mInputDeviceID)) {
    return;
  }
#else
#  ifdef DEBUG
  {
    MonitorAutoLock lock(mMonitor);
    // Either we have an audio input device, or we just removed the audio input
    // this iteration, and we're switching back to an output-only driver next
    // iteration.
    MOZ_ASSERT(mInputDeviceID || CurrentDriver()->Switching());
  }
#  endif
  if (!mInputDeviceID) {
    return;
  }
#endif
  nsTArray<RefPtr<AudioDataListener>>* listeners =
      mInputDeviceUsers.GetValue(mInputDeviceID);
  MOZ_ASSERT(listeners);
  for (auto& listener : *listeners) {
    listener->NotifyInputData(this, aBuffer, aFrames, aRate, aChannels);
  }
}

void MediaStreamGraphImpl::DeviceChangedImpl() {
  MOZ_ASSERT(OnGraphThread());

#ifdef ANDROID
  if (!mInputDeviceUsers.GetValue(mInputDeviceID)) {
    return;
  }
#else
  if (!mInputDeviceID) {
    return;
  }
#endif

  nsTArray<RefPtr<AudioDataListener>>* listeners =
      mInputDeviceUsers.GetValue(mInputDeviceID);
  for (auto& listener : *listeners) {
    listener->DeviceChanged(this);
  }
}

void MediaStreamGraphImpl::DeviceChanged() {
  // This is safe to be called from any thread: this message comes from an
  // underlying platform API, and we don't have much guarantees. If it is not
  // called from the main thread (and it probably will rarely be), it will post
  // itself to the main thread, and the actual device change message will be ran
  // and acted upon on the graph thread.
  if (!NS_IsMainThread()) {
    RefPtr<nsIRunnable> runnable =
        WrapRunnable(RefPtr<MediaStreamGraphImpl>(this),
                     &MediaStreamGraphImpl::DeviceChanged);
    mAbstractMainThread->Dispatch(runnable.forget());
    return;
  }

  class Message : public ControlMessage {
   public:
    explicit Message(MediaStreamGraph* aGraph)
        : ControlMessage(nullptr),
          mGraphImpl(static_cast<MediaStreamGraphImpl*>(aGraph)) {}
    void Run() override { mGraphImpl->DeviceChangedImpl(); }
    // We know that this is valid, because the graph can't shutdown if it has
    // messages.
    MediaStreamGraphImpl* mGraphImpl;
  };

  // Reset the latency, it will get fetched again next time it's queried.
  MOZ_ASSERT(NS_IsMainThread());
  mAudioOutputLatency = 0.0;

  AppendMessage(MakeUnique<Message>(this));
}

void MediaStreamGraphImpl::ReevaluateInputDevice() {
  MOZ_ASSERT(OnGraphThread());
  bool needToSwitch = false;

  if (CurrentDriver()->AsAudioCallbackDriver()) {
    AudioCallbackDriver* audioCallbackDriver =
        CurrentDriver()->AsAudioCallbackDriver();
    if (audioCallbackDriver->InputChannelCount() != AudioInputChannelCount()) {
      needToSwitch = true;
    }
    if (audioCallbackDriver->InputDevicePreference() !=
        AudioInputDevicePreference()) {
      needToSwitch = true;
    }
  } else {
    // We're already in the process of switching to a audio callback driver,
    // which will happen at the next iteration.
    // However, maybe it's not the correct number of channels. Re-query the
    // correct channel amount at this time.
#ifdef DEBUG
    MonitorAutoLock lock(mMonitor);
    MOZ_ASSERT(CurrentDriver()->Switching());
#endif
    needToSwitch = true;
  }
  if (needToSwitch) {
    AudioCallbackDriver* newDriver = new AudioCallbackDriver(
        this, AudioInputChannelCount(), AudioInputDevicePreference());
    {
      MonitorAutoLock lock(mMonitor);
      CurrentDriver()->SwitchAtNextIteration(newDriver);
    }
  }
}

bool MediaStreamGraphImpl::OnGraphThreadOrNotRunning() const {
  // either we're on the right thread (and calling CurrentDriver() is safe),
  // or we're going to fail the assert anyway, so don't cross-check
  // via CurrentDriver().
  return mDetectedNotRunning ? NS_IsMainThread() : OnGraphThread();
}

bool MediaStreamGraphImpl::OnGraphThread() const {
  // we're on the right thread (and calling mDriver is safe),
  MOZ_ASSERT(mDriver);
  if (mGraphRunner && mGraphRunner->OnThread()) {
    return true;
  }
  return mDriver->OnThread();
}

bool MediaStreamGraphImpl::Destroyed() const {
  MOZ_ASSERT(NS_IsMainThread());
  return !mSelfRef;
}

bool MediaStreamGraphImpl::ShouldUpdateMainThread() {
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  if (mRealtime) {
    return true;
  }

  TimeStamp now = TimeStamp::Now();
  // For offline graphs, update now if there is no pending iteration or if it
  // has been long enough since the last update.
  if (!mNeedAnotherIteration ||
      ((now - mLastMainThreadUpdate).ToMilliseconds() >
       CurrentDriver()->IterationDuration())) {
    mLastMainThreadUpdate = now;
    return true;
  }
  return false;
}

void MediaStreamGraphImpl::PrepareUpdatesToMainThreadState(bool aFinalUpdate) {
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  mMonitor.AssertCurrentThreadOwns();

  // We don't want to frequently update the main thread about timing update
  // when we are not running in realtime.
  if (aFinalUpdate || ShouldUpdateMainThread()) {
    // Strip updates that will be obsoleted below, so as to keep the length of
    // mStreamUpdates sane.
    size_t keptUpdateCount = 0;
    for (size_t i = 0; i < mStreamUpdates.Length(); ++i) {
      MediaStream* stream = mStreamUpdates[i].mStream;
      // RemoveStreamGraphThread() clears mStream in updates for
      // streams that are removed from the graph.
      MOZ_ASSERT(!stream || stream->GraphImpl() == this);
      if (!stream || stream->MainThreadNeedsUpdates()) {
        // Discard this update as it has either been cleared when the stream
        // was destroyed or there will be a newer update below.
        continue;
      }
      if (keptUpdateCount != i) {
        mStreamUpdates[keptUpdateCount] = std::move(mStreamUpdates[i]);
        MOZ_ASSERT(!mStreamUpdates[i].mStream);
      }
      ++keptUpdateCount;
    }
    mStreamUpdates.TruncateLength(keptUpdateCount);

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
      update->mNextMainThreadEnded = stream->mNotifiedEnded;
    }
    mNextMainThreadGraphTime = mProcessedTime;
    if (!mPendingUpdateRunnables.IsEmpty()) {
      mUpdateRunnables.AppendElements(std::move(mPendingUpdateRunnables));
    }
  }

  // If this is the final update, then a stable state event will soon be
  // posted just before this thread finishes, and so there is no need to also
  // post here.
  if (!aFinalUpdate &&
      // Don't send the message to the main thread if it's not going to have
      // any work to do.
      !(mUpdateRunnables.IsEmpty() && mStreamUpdates.IsEmpty())) {
    EnsureStableStateEventPosted();
  }
}

GraphTime MediaStreamGraphImpl::RoundUpToEndOfAudioBlock(GraphTime aTime) {
  if (aTime % WEBAUDIO_BLOCK_SIZE == 0) {
    return aTime;
  }
  return RoundUpToNextAudioBlock(aTime);
}

GraphTime MediaStreamGraphImpl::RoundUpToNextAudioBlock(GraphTime aTime) {
  uint64_t block = aTime >> WEBAUDIO_BLOCK_SIZE_BITS;
  uint64_t nextBlock = block + 1;
  GraphTime nextTime = nextBlock << WEBAUDIO_BLOCK_SIZE_BITS;
  return nextTime;
}

void MediaStreamGraphImpl::ProduceDataForStreamsBlockByBlock(
    uint32_t aStreamIndex, TrackRate aSampleRate) {
  MOZ_ASSERT(OnGraphThread());
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
        ps->ProcessInput(
            t, next,
            (next == mStateComputedTime) ? ProcessedMediaStream::ALLOW_END : 0);
      }
    }
    t = next;
  }
  NS_ASSERTION(t == mStateComputedTime,
               "Something went wrong with rounding to block boundaries");
}

void MediaStreamGraphImpl::RunMessageAfterProcessing(
    UniquePtr<ControlMessage> aMessage) {
  MOZ_ASSERT(OnGraphThread());

  if (mFrontMessageQueue.IsEmpty()) {
    mFrontMessageQueue.AppendElement();
  }

  // Only one block is used for messages from the graph thread.
  MOZ_ASSERT(mFrontMessageQueue.Length() == 1);
  mFrontMessageQueue[0].mMessages.AppendElement(std::move(aMessage));
}

void MediaStreamGraphImpl::RunMessagesInQueue() {
  TRACE_AUDIO_CALLBACK();
  MOZ_ASSERT(OnGraphThread());
  // Calculate independent action times for each batch of messages (each
  // batch corresponding to an event loop task). This isolates the performance
  // of different scripts to some extent.
  for (uint32_t i = 0; i < mFrontMessageQueue.Length(); ++i) {
    nsTArray<UniquePtr<ControlMessage>>& messages =
        mFrontMessageQueue[i].mMessages;

    for (uint32_t j = 0; j < messages.Length(); ++j) {
      messages[j]->Run();
    }
  }
  mFrontMessageQueue.Clear();
}

void MediaStreamGraphImpl::UpdateGraph(GraphTime aEndBlockingDecisions) {
  TRACE_AUDIO_CALLBACK();
  MOZ_ASSERT(OnGraphThread());
  MOZ_ASSERT(aEndBlockingDecisions >= mProcessedTime);
  // The next state computed time can be the same as the previous: it
  // means the driver would have been blocking indefinitly, but the graph has
  // been woken up right after having been to sleep.
  MOZ_ASSERT(aEndBlockingDecisions >= mStateComputedTime);

  UpdateStreamOrder();

  bool ensureNextIteration = false;

  for (MediaStream* stream : mStreams) {
    if (SourceMediaStream* is = stream->AsSourceStream()) {
      ensureNextIteration |= is->PullNewData(aEndBlockingDecisions);
      is->ExtractPendingInput(mStateComputedTime, aEndBlockingDecisions);
    }
    if (stream->mEnded) {
      // The stream's not suspended, and since it's ended, underruns won't
      // stop it playing out. So there's no blocking other than what we impose
      // here.
      GraphTime endTime = stream->GetEnd() + stream->mStartTime;
      if (endTime <= mStateComputedTime) {
        LOG(LogLevel::Verbose,
            ("%p: MediaStream %p is blocked due to being ended", this, stream));
        stream->mStartBlocking = mStateComputedTime;
      } else {
        LOG(LogLevel::Verbose,
            ("%p: MediaStream %p has ended, but is not blocked yet (current "
             "time %f, end at %f)",
             this, stream, MediaTimeToSeconds(mStateComputedTime),
             MediaTimeToSeconds(endTime)));
        // Data can't be added to a ended stream, so underruns are irrelevant.
        MOZ_ASSERT(endTime <= aEndBlockingDecisions);
        stream->mStartBlocking = endTime;
      }
    } else {
      stream->mStartBlocking = WillUnderrun(stream, aEndBlockingDecisions);

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
      if (SourceMediaStream* s = stream->AsSourceStream()) {
        if (s->Ended()) {
          continue;
        }
        {
          MutexAutoLock lock(s->mMutex);
          if (!s->mUpdateTrack->mPullingEnabled) {
            // The invariant that data must be provided is only enforced when
            // pulling.
            continue;
          }
        }
        if (stream->GetEnd() <
            stream->GraphTimeToStreamTime(aEndBlockingDecisions)) {
          LOG(LogLevel::Error,
              ("%p: SourceMediaStream %p (%s) is live and pulled, "
               "but wasn't fed "
               "enough data. TrackListeners=%zu. Track-end=%f, "
               "Iteration-end=%f",
               this, stream,
               (stream->mType == MediaSegment::AUDIO ? "audio" : "video"),
               stream->mTrackListeners.Length(),
               MediaTimeToSeconds(stream->GetEnd()),
               MediaTimeToSeconds(
                   stream->GraphTimeToStreamTime(aEndBlockingDecisions))));
          MOZ_DIAGNOSTIC_ASSERT(false,
                                "A non-ended SourceMediaStream wasn't fed "
                                "enough data by NotifyPull");
        }
      }
#endif /* MOZ_DIAGNOSTIC_ASSERT_ENABLED */
    }
  }

  for (MediaStream* stream : mSuspendedStreams) {
    stream->mStartBlocking = mStateComputedTime;
  }

  // If the loop is woken up so soon that IterationEnd() barely advances or
  // if an offline graph is not currently rendering, we end up having
  // aEndBlockingDecisions == mStateComputedTime.
  // Since the process interval [mStateComputedTime, aEndBlockingDecision) is
  // empty, Process() will not find any unblocked stream and so will not
  // ensure another iteration.  If the graph should be rendering, then ensure
  // another iteration to render.
  if (ensureNextIteration || (aEndBlockingDecisions == mStateComputedTime &&
                              mStateComputedTime < mEndTime)) {
    EnsureNextIteration();
  }
}

void MediaStreamGraphImpl::Process() {
  TRACE_AUDIO_CALLBACK();
  MOZ_ASSERT(OnGraphThread());
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
          // Verify that the sampling rate for all of the following streams is
          // the same
          for (uint32_t j = i + 1; j < mStreams.Length(); ++j) {
            AudioNodeStream* nextStream = mStreams[j]->AsAudioNodeStream();
            if (nextStream) {
              MOZ_ASSERT(n->mSampleRate == nextStream->mSampleRate,
                         "All AudioNodeStreams in the graph must have the same "
                         "sampling rate");
            }
          }
#endif
          // Since an AudioNodeStream is present, go ahead and
          // produce audio block by block for all the rest of the streams.
          ProduceDataForStreamsBlockByBlock(i, n->mSampleRate);
          doneAllProducing = true;
        } else {
          ps->ProcessInput(mProcessedTime, mStateComputedTime,
                           ProcessedMediaStream::ALLOW_END);
          // Assert that a live stream produced enough data
          MOZ_ASSERT_IF(!stream->mEnded,
                        stream->GetEnd() >= GraphTimeToStreamTimeWithBlocking(
                                                stream, mStateComputedTime));
        }
      }
    }
    // Only playback audio and video in real-time mode
    if (mRealtime) {
      CreateOrDestroyAudioStreams(stream);
      if (CurrentDriver()->AsAudioCallbackDriver()) {
        StreamTime ticksPlayedForThisStream = PlayAudio(stream);
        if (!ticksPlayed) {
          ticksPlayed = ticksPlayedForThisStream;
        } else {
          MOZ_ASSERT(!ticksPlayedForThisStream ||
                         ticksPlayedForThisStream == ticksPlayed,
                     "Each stream should have the same number of frame.");
        }
      }
    }
    if (stream->mStartBlocking > mProcessedTime) {
      allBlockedForever = false;
    }
  }

  if (CurrentDriver()->AsAudioCallbackDriver()) {
    if (!ticksPlayed) {
      // Nothing was played, so the mixer doesn't know how many frames were
      // processed. We still tell it so AudioCallbackDriver knows how much has
      // been processed. (bug 1406027)
      mMixer.Mix(nullptr,
                 CurrentDriver()->AsAudioCallbackDriver()->OutputChannelCount(),
                 mStateComputedTime - mProcessedTime, mSampleRate);
    }
    mMixer.FinishMixing();
  }

  if (!allBlockedForever) {
    EnsureNextIteration();
  }
}

bool MediaStreamGraphImpl::UpdateMainThreadState() {
  MOZ_ASSERT(OnGraphThread());
  if (mForceShutDown) {
    for (MediaStream* stream : AllStreams()) {
      stream->NotifyForcedShutdown();
    }
  }

  MonitorAutoLock lock(mMonitor);
  bool finalUpdate =
      mForceShutDown || (IsEmpty() && mBackMessageQueue.IsEmpty());
  PrepareUpdatesToMainThreadState(finalUpdate);
  if (finalUpdate) {
    // Enter shutdown mode when this iteration is completed.
    // No need to Destroy streams here. The main-thread owner of each
    // stream is responsible for calling Destroy on them.
    return false;
  }

  CurrentDriver()->WaitForNextIteration();

  SwapMessageQueues();
  return true;
}

bool MediaStreamGraphImpl::OneIteration(GraphTime aStateEnd) {
  if (mGraphRunner) {
    return mGraphRunner->OneIteration(aStateEnd);
  }

  return OneIterationImpl(aStateEnd);
}

bool MediaStreamGraphImpl::OneIterationImpl(GraphTime aStateEnd) {
  TRACE_AUDIO_CALLBACK();

  // Changes to LIFECYCLE_RUNNING occur before starting or reviving the graph
  // thread, and so the monitor need not be held to check mLifecycleState.
  // LIFECYCLE_THREAD_NOT_STARTED is possible when shutting down offline
  // graphs that have not started.
  MOZ_DIAGNOSTIC_ASSERT(mLifecycleState <= LIFECYCLE_RUNNING);
  MOZ_ASSERT(OnGraphThread());

  WebCore::DenormalDisabler disabler;

  // Process graph message from the main thread for this iteration.
  RunMessagesInQueue();

  GraphTime stateEnd = std::min(aStateEnd, GraphTime(mEndTime));
  UpdateGraph(stateEnd);

  mStateComputedTime = stateEnd;

  Process();

  GraphTime oldProcessedTime = mProcessedTime;
  mProcessedTime = stateEnd;

  UpdateCurrentTimeForStreams(oldProcessedTime);

  ProcessChunkMetadata(oldProcessedTime);

  // Process graph messages queued from RunMessageAfterProcessing() on this
  // thread during the iteration.
  RunMessagesInQueue();

  return UpdateMainThreadState();
}

void MediaStreamGraphImpl::ApplyStreamUpdate(StreamUpdate* aUpdate) {
  MOZ_ASSERT(NS_IsMainThread());
  mMonitor.AssertCurrentThreadOwns();

  MediaStream* stream = aUpdate->mStream;
  if (!stream) return;
  stream->mMainThreadCurrentTime = aUpdate->mNextMainThreadCurrentTime;
  stream->mMainThreadEnded = aUpdate->mNextMainThreadEnded;

  if (stream->ShouldNotifyTrackEnded()) {
    stream->NotifyMainThreadListeners();
  }
}

void MediaStreamGraphImpl::ForceShutDown() {
  MOZ_ASSERT(NS_IsMainThread(), "Must be called on main thread");
  LOG(LogLevel::Debug, ("%p: MediaStreamGraph::ForceShutdown", this));

  if (mShutdownBlocker) {
    // Avoid waiting forever for a graph to shut down
    // synchronously.  Reports are that some 3rd-party audio drivers
    // occasionally hang in shutdown (both for us and Chrome).
    NS_NewTimerWithCallback(
        getter_AddRefs(mShutdownTimer), this,
        MediaStreamGraph::AUDIO_CALLBACK_DRIVER_SHUTDOWN_TIMEOUT,
        nsITimer::TYPE_ONE_SHOT);
  }

  class Message final : public ControlMessage {
   public:
    explicit Message(MediaStreamGraphImpl* aGraph)
        : ControlMessage(nullptr), mGraph(aGraph) {}
    void Run() override { mGraph->mForceShutDown = true; }
    // The graph owns this message.
    MediaStreamGraphImpl* MOZ_NON_OWNING_REF mGraph;
  };

  if (mMainThreadStreamCount > 0 || mMainThreadPortCount > 0) {
    // If both the stream and port counts are zero, the regular shutdown
    // sequence will progress shortly to shutdown threads and destroy the graph.
    AppendMessage(MakeUnique<Message>(this));
  }
}

NS_IMETHODIMP
MediaStreamGraphImpl::Notify(nsITimer* aTimer) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ASSERTION(!mShutdownBlocker,
               "MediaStreamGraph took too long to shut down!");
  // Sigh, graph took too long to shut down.  Stop blocking system
  // shutdown and hope all is well.
  RemoveShutdownBlocker();
  return NS_OK;
}

void MediaStreamGraphImpl::AddShutdownBlocker() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mShutdownBlocker);

  class Blocker : public media::ShutdownBlocker {
    const RefPtr<MediaStreamGraphImpl> mGraph;

   public:
    Blocker(MediaStreamGraphImpl* aGraph, const nsString& aName)
        : media::ShutdownBlocker(aName), mGraph(aGraph) {}

    NS_IMETHOD
    BlockShutdown(nsIAsyncShutdownClient* aProfileBeforeChange) override {
      mGraph->ForceShutDown();
      return NS_OK;
    }
  };

  // Blocker names must be distinct.
  nsString blockerName;
  blockerName.AppendPrintf("MediaStreamGraph %p shutdown", this);
  mShutdownBlocker = MakeAndAddRef<Blocker>(this, blockerName);
  nsresult rv = media::GetShutdownBarrier()->AddBlocker(
      mShutdownBlocker, NS_LITERAL_STRING(__FILE__), __LINE__,
      NS_LITERAL_STRING("MediaStreamGraph shutdown"));
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
}

void MediaStreamGraphImpl::RemoveShutdownBlocker() {
  if (!mShutdownBlocker) {
    return;
  }
  media::GetShutdownBarrier()->RemoveBlocker(mShutdownBlocker);
  mShutdownBlocker = nullptr;
}

NS_IMETHODIMP
MediaStreamGraphImpl::GetName(nsACString& aName) {
  aName.AssignLiteral("MediaStreamGraphImpl");
  return NS_OK;
}

namespace {

class MediaStreamGraphShutDownRunnable : public Runnable {
 public:
  explicit MediaStreamGraphShutDownRunnable(MediaStreamGraphImpl* aGraph)
      : Runnable("MediaStreamGraphShutDownRunnable"), mGraph(aGraph) {}
  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mGraph->mDetectedNotRunning && mGraph->mDriver,
               "We should know the graph thread control loop isn't running!");

    LOG(LogLevel::Debug, ("%p: Shutting down graph", mGraph.get()));

    // We've asserted the graph isn't running.  Use mDriver instead of
    // CurrentDriver to avoid thread-safety checks
#if 0  // AudioCallbackDrivers are released asynchronously anyways
    // XXX a better test would be have setting mDetectedNotRunning make sure
    // any current callback has finished and block future ones -- or just
    // handle it all in Shutdown()!
    if (mGraph->mDriver->AsAudioCallbackDriver()) {
      MOZ_ASSERT(!mGraph->mDriver->AsAudioCallbackDriver()->InCallback());
    }
#endif

    if (mGraph->mGraphRunner) {
      mGraph->mGraphRunner->Shutdown();
    }

    mGraph->mDriver
        ->Shutdown();  // This will wait until it's shutdown since
                       // we'll start tearing down the graph after this

    // Release the driver now so that an AudioCallbackDriver will release its
    // SharedThreadPool reference.  Each SharedThreadPool reference must be
    // released before SharedThreadPool::SpinUntilEmpty() runs on
    // xpcom-shutdown-threads.  Don't wait for GC/CC to release references to
    // objects owning streams, or for expiration of mGraph->mShutdownTimer,
    // which won't otherwise release its reference on the graph until
    // nsTimerImpl::Shutdown(), which runs after xpcom-shutdown-threads.
    {
      MonitorAutoLock mon(mGraph->mMonitor);
      mGraph->SetCurrentDriver(nullptr);
    }

    // Safe to access these without the monitor since the graph isn't running.
    // We may be one of several graphs. Drop ticket to eventually unblock
    // shutdown.
    if (mGraph->mShutdownTimer && !mGraph->mShutdownBlocker) {
      MOZ_ASSERT(
          false,
          "AudioCallbackDriver took too long to shut down and we let shutdown"
          " continue - freezing and leaking");

      // The timer fired, so we may be deeper in shutdown now.  Block any
      // further teardown and just leak, for safety.
      return NS_OK;
    }

    // mGraph's thread is not running so it's OK to do whatever here
    for (MediaStream* stream : mGraph->AllStreams()) {
      // Clean up all MediaSegments since we cannot release Images too
      // late during shutdown. Also notify listeners that they were removed
      // so they can clean up any gfx resources.
      stream->RemoveAllResourcesAndListenersImpl();
    }

    MOZ_ASSERT(mGraph->mUpdateRunnables.IsEmpty());
    mGraph->mPendingUpdateRunnables.Clear();

    mGraph->RemoveShutdownBlocker();

    // We can't block past the final LIFECYCLE_WAITING_FOR_STREAM_DESTRUCTION
    // stage, since completion of that stage requires all streams to be freed,
    // which requires shutdown to proceed.

    if (mGraph->IsEmpty()) {
      // mGraph is no longer needed, so delete it.
      mGraph->Destroy();
    } else {
      // The graph is not empty.  We must be in a forced shutdown, either for
      // process shutdown or a non-realtime graph that has finished
      // processing. Some later AppendMessage will detect that the graph has
      // been emptied, and delete it.
      NS_ASSERTION(mGraph->mForceShutDown, "Not in forced shutdown?");
      mGraph->LifecycleStateRef() =
          MediaStreamGraphImpl::LIFECYCLE_WAITING_FOR_STREAM_DESTRUCTION;
    }
    return NS_OK;
  }

 private:
  RefPtr<MediaStreamGraphImpl> mGraph;
};

class MediaStreamGraphStableStateRunnable : public Runnable {
 public:
  explicit MediaStreamGraphStableStateRunnable(MediaStreamGraphImpl* aGraph,
                                               bool aSourceIsMSG)
      : Runnable("MediaStreamGraphStableStateRunnable"),
        mGraph(aGraph),
        mSourceIsMSG(aSourceIsMSG) {}
  NS_IMETHOD Run() override {
    TRACE();
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
  void Run() override { mStream->GraphImpl()->AddStreamGraphThread(mStream); }
  void RunDuringShutdown() override {
    // Make sure to run this message during shutdown too, to make sure
    // that we balance the number of streams registered with the graph
    // as they're destroyed during shutdown.
    Run();
  }
};

}  // namespace

void MediaStreamGraphImpl::RunInStableState(bool aSourceIsMSG) {
  MOZ_ASSERT(NS_IsMainThread(), "Must be called on main thread");

  nsTArray<nsCOMPtr<nsIRunnable>> runnables;
  // When we're doing a forced shutdown, pending control messages may be
  // run on the main thread via RunDuringShutdown. Those messages must
  // run without the graph monitor being held. So, we collect them here.
  nsTArray<UniquePtr<ControlMessage>> controlMessagesToRunDuringShutdown;

  {
    MonitorAutoLock lock(mMonitor);
    if (aSourceIsMSG) {
      MOZ_ASSERT(mPostedRunInStableStateEvent);
      mPostedRunInStableStateEvent = false;
    }

    // This should be kept in sync with the LifecycleState enum in
    // MediaStreamGraphImpl.h
    const char* LifecycleState_str[] = {
        "LIFECYCLE_THREAD_NOT_STARTED", "LIFECYCLE_RUNNING",
        "LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP",
        "LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN",
        "LIFECYCLE_WAITING_FOR_STREAM_DESTRUCTION"};

    if (LifecycleStateRef() != LIFECYCLE_RUNNING) {
      LOG(LogLevel::Debug,
          ("%p: Running stable state callback. Current state: %s", this,
           LifecycleState_str[LifecycleStateRef()]));
    }

    runnables.SwapElements(mUpdateRunnables);
    for (uint32_t i = 0; i < mStreamUpdates.Length(); ++i) {
      StreamUpdate* update = &mStreamUpdates[i];
      if (update->mStream) {
        ApplyStreamUpdate(update);
      }
    }
    mStreamUpdates.Clear();

    mMainThreadGraphTime = mNextMainThreadGraphTime;

    if (mCurrentTaskMessageQueue.IsEmpty()) {
      if (LifecycleStateRef() == LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP &&
          IsEmpty()) {
        // Complete shutdown. First, ensure that this graph is no longer used.
        // A new graph graph will be created if one is needed.
        // Asynchronously clean up old graph. We don't want to do this
        // synchronously because it spins the event loop waiting for threads
        // to shut down, and we don't want to do that in a stable state handler.
        LifecycleStateRef() = LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN;
        LOG(LogLevel::Debug,
            ("%p: Sending MediaStreamGraphShutDownRunnable", this));
        nsCOMPtr<nsIRunnable> event =
            new MediaStreamGraphShutDownRunnable(this);
        mAbstractMainThread->Dispatch(event.forget());
      }
    } else {
      if (LifecycleStateRef() <= LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP) {
        MessageBlock* block = mBackMessageQueue.AppendElement();
        block->mMessages.SwapElements(mCurrentTaskMessageQueue);
        EnsureNextIterationLocked();
      }

      // If this MediaStreamGraph has entered regular (non-forced) shutdown it
      // is not able to process any more messages. Those messages being added to
      // the graph in the first place is an error.
      MOZ_DIAGNOSTIC_ASSERT(mForceShutDown ||
                            LifecycleStateRef() <
                                LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP);
    }

    if (LifecycleStateRef() == LIFECYCLE_THREAD_NOT_STARTED) {
      LifecycleStateRef() = LIFECYCLE_RUNNING;
      // Start the thread now. We couldn't start it earlier because
      // the graph might exit immediately on finding it has no streams. The
      // first message for a new graph must create a stream.
      {
        // We should exit the monitor for now, because starting a stream might
        // take locks, and we don't want to deadlock.
        LOG(LogLevel::Debug,
            ("%p: Starting a graph with a %s", this,
             CurrentDriver()->AsAudioCallbackDriver() ? "AudioCallbackDriver"
                                                      : "SystemClockDriver"));
        RefPtr<GraphDriver> driver = CurrentDriver();
        MonitorAutoUnlock unlock(mMonitor);
        driver->Start();
        // It's not safe to Shutdown() a thread from StableState, and
        // releasing this may shutdown a SystemClockDriver thread.
        // Proxy the release to outside of StableState.
        NS_ReleaseOnMainThreadSystemGroup("MediaStreamGraphImpl::CurrentDriver",
                                          driver.forget(),
                                          true);  // always proxy
      }
    }

    if (LifecycleStateRef() == LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP &&
        mForceShutDown) {
      // Defer calls to RunDuringShutdown() to happen while mMonitor is not
      // held.
      for (uint32_t i = 0; i < mBackMessageQueue.Length(); ++i) {
        MessageBlock& mb = mBackMessageQueue[i];
        controlMessagesToRunDuringShutdown.AppendElements(
            std::move(mb.mMessages));
      }
      mBackMessageQueue.Clear();
      MOZ_ASSERT(mCurrentTaskMessageQueue.IsEmpty());
      // Stop MediaStreamGraph threads.
      LifecycleStateRef() = LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN;
      nsCOMPtr<nsIRunnable> event = new MediaStreamGraphShutDownRunnable(this);
      mAbstractMainThread->Dispatch(event.forget());
    }

    mDetectedNotRunning = LifecycleStateRef() > LIFECYCLE_RUNNING;
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
  mCanRunMessagesSynchronously =
      mDetectedNotRunning &&
      LifecycleStateRef() >= LIFECYCLE_WAITING_FOR_THREAD_SHUTDOWN;
#endif

  for (uint32_t i = 0; i < runnables.Length(); ++i) {
    runnables[i]->Run();
  }
}

void MediaStreamGraphImpl::EnsureRunInStableState() {
  MOZ_ASSERT(NS_IsMainThread(), "main thread only");

  if (mPostedRunInStableState) return;
  mPostedRunInStableState = true;
  nsCOMPtr<nsIRunnable> event =
      new MediaStreamGraphStableStateRunnable(this, false);
  nsContentUtils::RunInStableState(event.forget());
}

void MediaStreamGraphImpl::EnsureStableStateEventPosted() {
  MOZ_ASSERT(OnGraphThread());
  mMonitor.AssertCurrentThreadOwns();

  if (mPostedRunInStableStateEvent) return;
  mPostedRunInStableStateEvent = true;
  nsCOMPtr<nsIRunnable> event =
      new MediaStreamGraphStableStateRunnable(this, true);
  mAbstractMainThread->Dispatch(event.forget());
}

void MediaStreamGraphImpl::SignalMainThreadCleanup() {
  MOZ_ASSERT(mDriver->OnThread());

  MonitorAutoLock lock(mMonitor);
  // LIFECYCLE_THREAD_NOT_STARTED is possible when shutting down offline
  // graphs that have not started.
  MOZ_DIAGNOSTIC_ASSERT(mLifecycleState <= LIFECYCLE_RUNNING);
  LOG(LogLevel::Debug,
      ("%p: MediaStreamGraph waiting for main thread cleanup", this));
  LifecycleStateRef() =
      MediaStreamGraphImpl::LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP;
  EnsureStableStateEventPosted();
}

void MediaStreamGraphImpl::AppendMessage(UniquePtr<ControlMessage> aMessage) {
  MOZ_ASSERT(NS_IsMainThread(), "main thread only");
  MOZ_ASSERT_IF(aMessage->GetStream(), !aMessage->GetStream()->IsDestroyed());
  MOZ_DIAGNOSTIC_ASSERT(mMainThreadStreamCount > 0 || mMainThreadPortCount > 0);

  if (mDetectedNotRunning &&
      LifecycleStateRef() > LIFECYCLE_WAITING_FOR_MAIN_THREAD_CLEANUP) {
    // The graph control loop is not running and main thread cleanup has
    // happened. From now on we can't append messages to
    // mCurrentTaskMessageQueue, because that will never be processed again, so
    // just RunDuringShutdown this message. This should only happen during
    // forced shutdown, or after a non-realtime graph has finished processing.
#ifdef DEBUG
    MOZ_ASSERT(mCanRunMessagesSynchronously);
    mCanRunMessagesSynchronously = false;
#endif
    aMessage->RunDuringShutdown();
#ifdef DEBUG
    mCanRunMessagesSynchronously = true;
#endif
    if (IsEmpty() &&
        LifecycleStateRef() >= LIFECYCLE_WAITING_FOR_STREAM_DESTRUCTION) {
      Destroy();
    }
    return;
  }

  mCurrentTaskMessageQueue.AppendElement(std::move(aMessage));
  EnsureRunInStableState();
}

void MediaStreamGraphImpl::Dispatch(already_AddRefed<nsIRunnable>&& aRunnable) {
  mAbstractMainThread->Dispatch(std::move(aRunnable));
}

MediaStream::MediaStream(TrackRate aSampleRate, MediaSegment::Type aType,
                         MediaSegment* aSegment)
    : mSampleRate(aSampleRate),
      mType(aType),
      mSegment(aSegment),
      mStartTime(0),
      mForgottenTime(0),
      mEnded(false),
      mNotifiedEnded(false),
      mDisabledMode(DisabledTrackMode::ENABLED),
      mStartBlocking(GRAPH_TIME_MAX),
      mSuspendedCount(0),
      mMainThreadCurrentTime(0),
      mMainThreadEnded(false),
      mEndedNotificationSent(false),
      mMainThreadDestroyed(false),
      mGraph(nullptr) {
  MOZ_COUNT_CTOR(MediaStream);
  MOZ_ASSERT_IF(mSegment, mSegment->GetType() == aType);
}

MediaStream::~MediaStream() {
  MOZ_COUNT_DTOR(MediaStream);
  NS_ASSERTION(mMainThreadDestroyed, "Should have been destroyed already");
  NS_ASSERTION(mMainThreadListeners.IsEmpty(),
               "All main thread listeners should have been removed");
}

size_t MediaStream::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t amount = 0;

  // Not owned:
  // - mGraph - Not reported here
  // - mConsumers - elements
  // Future:
  // - mLastPlayedVideoFrame
  // - mTrackListeners - elements
  // - mAudioOutputStream - elements

  amount += mAudioOutputs.ShallowSizeOfExcludingThis(aMallocSizeOf);
  amount += mTrackListeners.ShallowSizeOfExcludingThis(aMallocSizeOf);
  amount += mMainThreadListeners.ShallowSizeOfExcludingThis(aMallocSizeOf);
  amount += mConsumers.ShallowSizeOfExcludingThis(aMallocSizeOf);
  amount += aMallocSizeOf(mAudioOutputStream.get());

  return amount;
}

size_t MediaStream::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

void MediaStream::IncrementSuspendCount() {
  ++mSuspendedCount;
  if (mSuspendedCount == 1) {
    for (uint32_t i = 0; i < mConsumers.Length(); ++i) {
      mConsumers[i]->Suspended();
    }
  }
}

void MediaStream::DecrementSuspendCount() {
  NS_ASSERTION(mSuspendedCount > 0, "Suspend count underrun");
  --mSuspendedCount;
  if (mSuspendedCount == 0) {
    for (uint32_t i = 0; i < mConsumers.Length(); ++i) {
      mConsumers[i]->Resumed();
    }
  }
}

MediaStreamGraphImpl* MediaStream::GraphImpl() { return mGraph; }

const MediaStreamGraphImpl* MediaStream::GraphImpl() const { return mGraph; }

MediaStreamGraph* MediaStream::Graph() { return mGraph; }

const MediaStreamGraph* MediaStream::Graph() const { return mGraph; }

void MediaStream::SetGraphImpl(MediaStreamGraphImpl* aGraph) {
  MOZ_ASSERT(!mGraph, "Should only be called once");
  MOZ_ASSERT(mSampleRate == aGraph->GraphRate());
  mGraph = aGraph;
}

void MediaStream::SetGraphImpl(MediaStreamGraph* aGraph) {
  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(aGraph);
  SetGraphImpl(graph);
}

StreamTime MediaStream::GraphTimeToStreamTime(GraphTime aTime) const {
  NS_ASSERTION(mStartBlocking == GraphImpl()->mStateComputedTime ||
                   aTime <= mStartBlocking,
               "Incorrectly ignoring blocking!");
  return aTime - mStartTime;
}

GraphTime MediaStream::StreamTimeToGraphTime(StreamTime aTime) const {
  NS_ASSERTION(mStartBlocking == GraphImpl()->mStateComputedTime ||
                   aTime + mStartTime <= mStartBlocking,
               "Incorrectly ignoring blocking!");
  return aTime + mStartTime;
}

StreamTime MediaStream::GraphTimeToStreamTimeWithBlocking(
    GraphTime aTime) const {
  return GraphImpl()->GraphTimeToStreamTimeWithBlocking(this, aTime);
}

void MediaStream::RemoveAllResourcesAndListenersImpl() {
  GraphImpl()->AssertOnGraphThreadOrNotRunning();

  auto trackListeners(mTrackListeners);
  for (auto& l : trackListeners) {
    l->NotifyRemoved(Graph());
  }
  mTrackListeners.Clear();

  RemoveAllDirectListenersImpl();

  if (mSegment) {
    mSegment->Clear();
  }
}

void MediaStream::DestroyImpl() {
  for (int32_t i = mConsumers.Length() - 1; i >= 0; --i) {
    mConsumers[i]->Disconnect();
  }
  if (mSegment) {
    mSegment->Clear();
  }
  mGraph = nullptr;
}

void MediaStream::Destroy() {
  // Keep this stream alive until we leave this method
  RefPtr<MediaStream> kungFuDeathGrip = this;

  class Message : public ControlMessage {
   public:
    explicit Message(MediaStream* aStream) : ControlMessage(aStream) {}
    void Run() override {
      mStream->RemoveAllResourcesAndListenersImpl();
      auto graph = mStream->GraphImpl();
      mStream->DestroyImpl();
      graph->RemoveStreamGraphThread(mStream);
    }
    void RunDuringShutdown() override { Run(); }
  };
  // Keep a reference to the graph, since Message might RunDuringShutdown()
  // synchronously and make GraphImpl() invalid.
  RefPtr<MediaStreamGraphImpl> graph = GraphImpl();
  graph->AppendMessage(MakeUnique<Message>(this));
  graph->RemoveStream(this);
  // Message::RunDuringShutdown may have removed this stream from the graph,
  // but our kungFuDeathGrip above will have kept this stream alive if
  // necessary.
  mMainThreadDestroyed = true;
}

StreamTime MediaStream::GetEnd() const {
  return mSegment ? mSegment->GetDuration() : 0;
}

void MediaStream::AddAudioOutput(void* aKey) {
  class Message : public ControlMessage {
   public:
    Message(MediaStream* aStream, void* aKey)
        : ControlMessage(aStream), mKey(aKey) {}
    void Run() override { mStream->AddAudioOutputImpl(mKey); }
    void* mKey;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aKey));
}

void MediaStream::SetAudioOutputVolumeImpl(void* aKey, float aVolume) {
  for (uint32_t i = 0; i < mAudioOutputs.Length(); ++i) {
    if (mAudioOutputs[i].mKey == aKey) {
      mAudioOutputs[i].mVolume = aVolume;
      return;
    }
  }
  NS_ERROR("Audio output key not found");
}

void MediaStream::SetAudioOutputVolume(void* aKey, float aVolume) {
  class Message : public ControlMessage {
   public:
    Message(MediaStream* aStream, void* aKey, float aVolume)
        : ControlMessage(aStream), mKey(aKey), mVolume(aVolume) {}
    void Run() override { mStream->SetAudioOutputVolumeImpl(mKey, mVolume); }
    void* mKey;
    float mVolume;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aKey, aVolume));
}

void MediaStream::AddAudioOutputImpl(void* aKey) {
  LOG(LogLevel::Info,
      ("MediaStream %p Adding AudioOutput for key %p", this, aKey));
  mAudioOutputs.AppendElement(AudioOutput(aKey));
}

void MediaStream::RemoveAudioOutputImpl(void* aKey) {
  LOG(LogLevel::Info,
      ("MediaStream %p Removing AudioOutput for key %p", this, aKey));
  for (uint32_t i = 0; i < mAudioOutputs.Length(); ++i) {
    if (mAudioOutputs[i].mKey == aKey) {
      mAudioOutputs.RemoveElementAt(i);
      return;
    }
  }
  NS_ERROR("Audio output key not found");
}

void MediaStream::RemoveAudioOutput(void* aKey) {
  class Message : public ControlMessage {
   public:
    Message(MediaStream* aStream, void* aKey)
        : ControlMessage(aStream), mKey(aKey) {}
    void Run() override { mStream->RemoveAudioOutputImpl(mKey); }
    void* mKey;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aKey));
}

void MediaStream::Suspend() {
  class Message : public ControlMessage {
   public:
    explicit Message(MediaStream* aStream) : ControlMessage(aStream) {}
    void Run() override {
      mStream->GraphImpl()->IncrementSuspendCount(mStream);
    }
  };

  // This can happen if this method has been called asynchronously, and the
  // stream has been destroyed since then.
  if (mMainThreadDestroyed) {
    return;
  }
  GraphImpl()->AppendMessage(MakeUnique<Message>(this));
}

void MediaStream::Resume() {
  class Message : public ControlMessage {
   public:
    explicit Message(MediaStream* aStream) : ControlMessage(aStream) {}
    void Run() override {
      mStream->GraphImpl()->DecrementSuspendCount(mStream);
    }
  };

  // This can happen if this method has been called asynchronously, and the
  // stream has been destroyed since then.
  if (mMainThreadDestroyed) {
    return;
  }
  GraphImpl()->AppendMessage(MakeUnique<Message>(this));
}

void MediaStream::AddListenerImpl(
    already_AddRefed<MediaStreamTrackListener> aListener) {
  RefPtr<MediaStreamTrackListener> l(aListener);
  mTrackListeners.AppendElement(std::move(l));

  PrincipalHandle lastPrincipalHandle = mSegment->GetLastPrincipalHandle();
  mTrackListeners.LastElement()->NotifyPrincipalHandleChanged(
      Graph(), lastPrincipalHandle);
  if (mNotifiedEnded) {
    mTrackListeners.LastElement()->NotifyEnded(Graph());
  }
  if (mDisabledMode == DisabledTrackMode::SILENCE_BLACK) {
    mTrackListeners.LastElement()->NotifyEnabledStateChanged(Graph(), false);
  }
}

void MediaStream::AddListener(MediaStreamTrackListener* aListener) {
  class Message : public ControlMessage {
   public:
    Message(MediaStream* aStream, MediaStreamTrackListener* aListener)
        : ControlMessage(aStream), mListener(aListener) {}
    void Run() override { mStream->AddListenerImpl(mListener.forget()); }
    RefPtr<MediaStreamTrackListener> mListener;
  };
  MOZ_ASSERT(mSegment, "Segment-less tracks do not support listeners");
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aListener));
}

void MediaStream::RemoveListenerImpl(MediaStreamTrackListener* aListener) {
  for (size_t i = 0; i < mTrackListeners.Length(); ++i) {
    if (mTrackListeners[i] == aListener) {
      mTrackListeners[i]->NotifyRemoved(Graph());
      mTrackListeners.RemoveElementAt(i);
      return;
    }
  }
}

void MediaStream::RemoveListener(MediaStreamTrackListener* aListener) {
  class Message : public ControlMessage {
   public:
    Message(MediaStream* aStream, MediaStreamTrackListener* aListener)
        : ControlMessage(aStream), mListener(aListener) {}
    void Run() override { mStream->RemoveListenerImpl(mListener); }
    void RunDuringShutdown() override {
      // During shutdown we still want the listener's NotifyRemoved to be
      // called, since not doing that might block shutdown of other modules.
      Run();
    }
    RefPtr<MediaStreamTrackListener> mListener;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aListener));
}

void MediaStream::AddDirectListenerImpl(
    already_AddRefed<DirectMediaStreamTrackListener> aListener) {
  // Base implementation, for streams that don't support direct track listeners.
  RefPtr<DirectMediaStreamTrackListener> listener = aListener;
  listener->NotifyDirectListenerInstalled(
      DirectMediaStreamTrackListener::InstallationResult::STREAM_NOT_SUPPORTED);
}

void MediaStream::AddDirectListener(DirectMediaStreamTrackListener* aListener) {
  class Message : public ControlMessage {
   public:
    Message(MediaStream* aStream, DirectMediaStreamTrackListener* aListener)
        : ControlMessage(aStream), mListener(aListener) {}
    void Run() override { mStream->AddDirectListenerImpl(mListener.forget()); }
    RefPtr<DirectMediaStreamTrackListener> mListener;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aListener));
}

void MediaStream::RemoveDirectListenerImpl(
    DirectMediaStreamTrackListener* aListener) {
  // Base implementation, the listener was never added so nothing to do.
}

void MediaStream::RemoveDirectListener(
    DirectMediaStreamTrackListener* aListener) {
  class Message : public ControlMessage {
   public:
    Message(MediaStream* aStream, DirectMediaStreamTrackListener* aListener)
        : ControlMessage(aStream), mListener(aListener) {}
    void Run() override { mStream->RemoveDirectListenerImpl(mListener); }
    void RunDuringShutdown() override {
      // During shutdown we still want the listener's
      // NotifyDirectListenerUninstalled to be called, since not doing that
      // might block shutdown of other modules.
      Run();
    }
    RefPtr<DirectMediaStreamTrackListener> mListener;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aListener));
}

void MediaStream::RunAfterPendingUpdates(
    already_AddRefed<nsIRunnable> aRunnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MediaStreamGraphImpl* graph = GraphImpl();
  nsCOMPtr<nsIRunnable> runnable(aRunnable);

  class Message : public ControlMessage {
   public:
    Message(MediaStream* aStream, already_AddRefed<nsIRunnable> aRunnable)
        : ControlMessage(aStream), mRunnable(aRunnable) {}
    void Run() override {
      mStream->Graph()->DispatchToMainThreadStableState(mRunnable.forget());
    }
    void RunDuringShutdown() override {
      // Don't run mRunnable now as it may call AppendMessage() which would
      // assume that there are no remaining controlMessagesToRunDuringShutdown.
      MOZ_ASSERT(NS_IsMainThread());
      mStream->GraphImpl()->Dispatch(mRunnable.forget());
    }

   private:
    nsCOMPtr<nsIRunnable> mRunnable;
  };

  graph->AppendMessage(MakeUnique<Message>(this, runnable.forget()));
}

void MediaStream::SetEnabledImpl(DisabledTrackMode aMode) {
  if (aMode == DisabledTrackMode::ENABLED) {
    mDisabledMode = DisabledTrackMode::ENABLED;
    for (const auto& l : mTrackListeners) {
      l->NotifyEnabledStateChanged(Graph(), true);
    }
  } else {
    MOZ_DIAGNOSTIC_ASSERT(
        mDisabledMode == DisabledTrackMode::ENABLED,
        "Changing disabled track mode for a track is not allowed");
    mDisabledMode = aMode;
    if (aMode == DisabledTrackMode::SILENCE_BLACK) {
      for (const auto& l : mTrackListeners) {
        l->NotifyEnabledStateChanged(Graph(), false);
      }
    }
  }
}

void MediaStream::SetEnabled(DisabledTrackMode aMode) {
  class Message : public ControlMessage {
   public:
    Message(MediaStream* aStream, DisabledTrackMode aMode)
        : ControlMessage(aStream), mMode(aMode) {}
    void Run() override { mStream->SetEnabledImpl(mMode); }
    DisabledTrackMode mMode;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aMode));
}

void MediaStream::ApplyTrackDisabling(MediaSegment* aSegment,
                                      MediaSegment* aRawSegment) {
  if (mDisabledMode == DisabledTrackMode::ENABLED) {
    return;
  }
  if (mDisabledMode == DisabledTrackMode::SILENCE_BLACK) {
    aSegment->ReplaceWithDisabled();
    if (aRawSegment) {
      aRawSegment->ReplaceWithDisabled();
    }
  } else if (mDisabledMode == DisabledTrackMode::SILENCE_FREEZE) {
    aSegment->ReplaceWithNull();
    if (aRawSegment) {
      aRawSegment->ReplaceWithNull();
    }
  } else {
    MOZ_CRASH("Unsupported mode");
  }
}

void MediaStream::AddMainThreadListener(
    MainThreadMediaStreamListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(!mMainThreadListeners.Contains(aListener));

  mMainThreadListeners.AppendElement(aListener);

  // If it is not yet time to send the notification, then exit here.
  if (!mEndedNotificationSent) {
    return;
  }

  class NotifyRunnable final : public Runnable {
   public:
    explicit NotifyRunnable(MediaStream* aStream)
        : Runnable("MediaStream::NotifyRunnable"), mStream(aStream) {}

    NS_IMETHOD Run() override {
      MOZ_ASSERT(NS_IsMainThread());
      mStream->NotifyMainThreadListeners();
      return NS_OK;
    }

   private:
    ~NotifyRunnable() {}

    RefPtr<MediaStream> mStream;
  };

  nsCOMPtr<nsIRunnable> runnable = new NotifyRunnable(this);
  GraphImpl()->Dispatch(runnable.forget());
}

void MediaStream::AdvanceTimeVaryingValuesToCurrentTime(
    GraphTime aCurrentTime, GraphTime aBlockedTime) {
  mStartTime += aBlockedTime;

  if (!mSegment) {
    // No data to be forgotten.
    return;
  }

  StreamTime time = aCurrentTime - mStartTime;
  // Only prune if there is a reasonable chunk (50ms @ 48kHz) to forget, so we
  // don't spend too much time pruning segments.
  const StreamTime minChunkSize = 2400;
  if (time < mForgottenTime + minChunkSize) {
    return;
  }

  mForgottenTime = std::min(GetEnd() - 1, time);
  mSegment->ForgetUpTo(mForgottenTime);
}

SourceMediaStream::SourceMediaStream(MediaSegment::Type aType,
                                     TrackRate aSampleRate)
    : MediaStream(aSampleRate, aType,
                  aType == MediaSegment::AUDIO
                      ? static_cast<MediaSegment*>(new AudioSegment())
                      : static_cast<MediaSegment*>(new VideoSegment())),
      mMutex("mozilla::media::SourceMediaStream") {
  mUpdateTrack = MakeUnique<TrackData>();
  mUpdateTrack->mInputRate = aSampleRate;
  mUpdateTrack->mResamplerChannelCount = 0;
  mUpdateTrack->mData = UniquePtr<MediaSegment>(mSegment->CreateEmptyClone());
  mUpdateTrack->mEnded = false;
  mUpdateTrack->mPullingEnabled = false;
}

nsresult SourceMediaStream::OpenAudioInput(CubebUtils::AudioDeviceID aID,
                                           AudioDataListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(GraphImpl());
  MOZ_ASSERT(!mInputListener);
  mInputListener = aListener;
  return GraphImpl()->OpenAudioInput(aID, aListener);
}

void SourceMediaStream::CloseAudioInput(Maybe<CubebUtils::AudioDeviceID>& aID) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(GraphImpl());
  if (!mInputListener) {
    return;
  }
  GraphImpl()->CloseAudioInput(aID, mInputListener);
  mInputListener = nullptr;
}

void SourceMediaStream::Destroy() {
  MOZ_ASSERT(NS_IsMainThread());
  Maybe<CubebUtils::AudioDeviceID> id = Nothing();
  CloseAudioInput(id);

  MediaStream::Destroy();
}

void SourceMediaStream::DestroyImpl() {
  GraphImpl()->AssertOnGraphThreadOrNotRunning();
  for (int32_t i = mConsumers.Length() - 1; i >= 0; --i) {
    // Disconnect before we come under mMutex's lock since it can call back
    // through RemoveDirectListenerImpl() and deadlock.
    mConsumers[i]->Disconnect();
  }

  // Hold mMutex while mGraph is reset so that other threads holding mMutex
  // can null-check know that the graph will not destroyed.
  MutexAutoLock lock(mMutex);
  mUpdateTrack = nullptr;
  MediaStream::DestroyImpl();
}

void SourceMediaStream::SetPullingEnabled(bool aEnabled) {
  class Message : public ControlMessage {
   public:
    Message(SourceMediaStream* aStream, bool aEnabled)
        : ControlMessage(nullptr), mStream(aStream), mEnabled(aEnabled) {}
    void Run() override {
      MutexAutoLock lock(mStream->mMutex);
      if (!mStream->mUpdateTrack) {
        // We can't enable pulling for a stream that has ended. We ignore
        // this if we're disabling pulling, since shutdown sequences are
        // complex. If there's truly an issue we'll have issues enabling anyway.
        MOZ_ASSERT_IF(mEnabled, mStream->mEnded);
        return;
      }
      MOZ_ASSERT(mStream->mType == MediaSegment::AUDIO,
                 "Pulling is not allowed for video");
      mStream->mUpdateTrack->mPullingEnabled = mEnabled;
    }
    SourceMediaStream* mStream;
    bool mEnabled;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aEnabled));
}

bool SourceMediaStream::PullNewData(GraphTime aDesiredUpToTime) {
  TRACE_AUDIO_CALLBACK_COMMENT("SourceMediaStream %p", this);
  StreamTime t;
  StreamTime current;
  {
    if (mEnded) {
      return false;
    }
    MutexAutoLock lock(mMutex);
    if (mUpdateTrack->mEnded) {
      return false;
    }
    if (!mUpdateTrack->mPullingEnabled) {
      return false;
    }
    // Compute how much stream time we'll need assuming we don't block
    // the stream at all.
    t = GraphTimeToStreamTime(aDesiredUpToTime);
    current = GetEnd() + mUpdateTrack->mData->GetDuration();
  }
  if (t <= current) {
    return false;
  }
  LOG(LogLevel::Verbose,
      ("%p: Calling NotifyPull stream=%p t=%f current end=%f", GraphImpl(),
       this, GraphImpl()->MediaTimeToSeconds(t),
       GraphImpl()->MediaTimeToSeconds(current)));
  for (auto& l : mTrackListeners) {
    l->NotifyPull(Graph(), current, t);
  }
  return true;
}

/**
 * This moves chunks from aIn to aOut. For audio this is simple. For video
 * we carry durations over if present, or extend up to aDesiredUpToTime if not.
 *
 * We also handle "resetters" from captured media elements. This type of source
 * pushes future frames into the track, and should it need to remove some, e.g.,
 * because of a seek or pause, it tells us by letting time go backwards. Without
 * this, tracks would be live for too long after a seek or pause.
 */
static void MoveToSegment(SourceMediaStream* aStream, MediaSegment* aIn,
                          MediaSegment* aOut, StreamTime aCurrentTime,
                          StreamTime aDesiredUpToTime) {
  MOZ_ASSERT(aIn->GetType() == aOut->GetType());
  MOZ_ASSERT(aOut->GetDuration() >= aCurrentTime);
  if (aIn->GetType() == MediaSegment::AUDIO) {
    aOut->AppendFrom(aIn);
  } else {
    VideoSegment* in = static_cast<VideoSegment*>(aIn);
    VideoSegment* out = static_cast<VideoSegment*>(aOut);
    for (VideoSegment::ConstChunkIterator c(*in); !c.IsEnded(); c.Next()) {
      MOZ_ASSERT(!c->mTimeStamp.IsNull());
      VideoChunk* last = out->GetLastChunk();
      if (!last || last->mTimeStamp.IsNull()) {
        // This is the first frame, or the last frame pushed to `out` has been
        // all consumed. Just append and we deal with its duration later.
        out->AppendFrame(do_AddRef(c->mFrame.GetImage()),
                         c->mFrame.GetIntrinsicSize(),
                         c->mFrame.GetPrincipalHandle(),
                         c->mFrame.GetForceBlack(), c->mTimeStamp);
        if (c->GetDuration() > 0) {
          out->ExtendLastFrameBy(c->GetDuration());
        }
        continue;
      }

      // We now know when this frame starts, aka when the last frame ends.

      if (c->mTimeStamp < last->mTimeStamp) {
        // Time is going backwards. This is a resetting frame from
        // DecodedStream. Clear everything up to currentTime.
        out->Clear();
        out->AppendNullData(aCurrentTime);
      }

      // Append the current frame (will have duration 0).
      out->AppendFrame(do_AddRef(c->mFrame.GetImage()),
                       c->mFrame.GetIntrinsicSize(),
                       c->mFrame.GetPrincipalHandle(),
                       c->mFrame.GetForceBlack(), c->mTimeStamp);
      if (c->GetDuration() > 0) {
        out->ExtendLastFrameBy(c->GetDuration());
      }
    }
    if (out->GetDuration() < aDesiredUpToTime) {
      out->ExtendLastFrameBy(aDesiredUpToTime - out->GetDuration());
    }
    in->Clear();
  }
  MOZ_ASSERT(aIn->GetDuration() == 0, "aIn must be consumed");
}

void SourceMediaStream::ExtractPendingInput(GraphTime aCurrentTime,
                                            GraphTime aDesiredUpToTime) {
  MutexAutoLock lock(mMutex);

  if (!mUpdateTrack) {
    MOZ_ASSERT(mEnded);
    return;
  }

  StreamTime streamCurrentTime = GraphTimeToStreamTime(aCurrentTime);

  ApplyTrackDisabling(mUpdateTrack->mData.get());

  if (!mUpdateTrack->mData->IsEmpty()) {
    for (const auto& l : mTrackListeners) {
      l->NotifyQueuedChanges(GraphImpl(), GetEnd(), *mUpdateTrack->mData);
    }
  }
  StreamTime streamDesiredUpToTime = GraphTimeToStreamTime(aDesiredUpToTime);
  StreamTime endTime = streamDesiredUpToTime;
  if (mUpdateTrack->mEnded) {
    endTime = std::min(streamDesiredUpToTime,
                       GetEnd() + mUpdateTrack->mData->GetDuration());
  }
  LOG(LogLevel::Verbose,
      ("%p: SourceMediaStream %p advancing end from %" PRId64 " to %" PRId64,
       GraphImpl(), this, int64_t(streamCurrentTime), int64_t(endTime)));
  MoveToSegment(this, mUpdateTrack->mData.get(), mSegment.get(),
                streamCurrentTime, endTime);
  if (mUpdateTrack->mEnded && GetEnd() < streamDesiredUpToTime) {
    mEnded = true;
    mUpdateTrack = nullptr;
  }
}

void SourceMediaStream::ResampleAudioToGraphSampleRate(MediaSegment* aSegment) {
  mMutex.AssertCurrentThreadOwns();
  if (aSegment->GetType() != MediaSegment::AUDIO ||
      mUpdateTrack->mInputRate == GraphImpl()->GraphRate()) {
    return;
  }
  AudioSegment* segment = static_cast<AudioSegment*>(aSegment);
  int channels = segment->ChannelCount();

  // If this segment is just silence, we delay instanciating the resampler. We
  // also need to recreate the resampler if the channel count or input rate
  // changes.
  if (channels && mUpdateTrack->mResamplerChannelCount != channels) {
    SpeexResamplerState* state = speex_resampler_init(
        channels, mUpdateTrack->mInputRate, GraphImpl()->GraphRate(),
        SPEEX_RESAMPLER_QUALITY_MIN, nullptr);
    if (!state) {
      return;
    }
    mUpdateTrack->mResampler.own(state);
    mUpdateTrack->mResamplerChannelCount = channels;
  }
  segment->ResampleChunks(mUpdateTrack->mResampler, mUpdateTrack->mInputRate,
                          GraphImpl()->GraphRate());
}

void SourceMediaStream::AdvanceTimeVaryingValuesToCurrentTime(
    GraphTime aCurrentTime, GraphTime aBlockedTime) {
  MutexAutoLock lock(mMutex);
  MediaStream::AdvanceTimeVaryingValuesToCurrentTime(aCurrentTime,
                                                     aBlockedTime);
}

void SourceMediaStream::SetAppendDataSourceRate(TrackRate aRate) {
  MutexAutoLock lock(mMutex);
  if (!mUpdateTrack) {
    return;
  }
  MOZ_DIAGNOSTIC_ASSERT(mSegment->GetType() == MediaSegment::AUDIO);
  // Set the new input rate and reset the resampler.
  mUpdateTrack->mInputRate = aRate;
  mUpdateTrack->mResampler.own(nullptr);
  mUpdateTrack->mResamplerChannelCount = 0;
}

StreamTime SourceMediaStream::AppendData(MediaSegment* aSegment,
                                         MediaSegment* aRawSegment) {
  MutexAutoLock lock(mMutex);
  MOZ_DIAGNOSTIC_ASSERT(aSegment->GetType() == mType);
  StreamTime appended = 0;
  auto graph = GraphImpl();
  if (!mUpdateTrack || mUpdateTrack->mEnded || !graph) {
    aSegment->Clear();
    return appended;
  }

  // Data goes into mData, and on the next iteration of the MSG moves
  // into the track's segment after NotifyQueuedTrackChanges().  This adds
  // 0-10ms of delay before data gets to direct listeners.
  // Indirect listeners (via subsequent TrackUnion nodes) are synced to
  // playout time, and so can be delayed by buffering.

  // Apply track disabling before notifying any consumers directly
  // or inserting into the graph
  ApplyTrackDisabling(aSegment, aRawSegment);

  ResampleAudioToGraphSampleRate(aSegment);

  // Must notify first, since AppendFrom() will empty out aSegment
  NotifyDirectConsumers(aRawSegment ? aRawSegment : aSegment);
  appended = aSegment->GetDuration();
  mUpdateTrack->mData->AppendFrom(aSegment);  // note: aSegment is now dead
  graph->EnsureNextIteration();

  return appended;
}

void SourceMediaStream::NotifyDirectConsumers(MediaSegment* aSegment) {
  mMutex.AssertCurrentThreadOwns();

  for (const auto& l : mDirectTrackListeners) {
    StreamTime offset = 0;  // FIX! need a separate StreamTime.... or the end of
                            // the internal buffer
    l->NotifyRealtimeTrackDataAndApplyTrackDisabling(Graph(), offset,
                                                     *aSegment);
  }
}

void SourceMediaStream::AddDirectListenerImpl(
    already_AddRefed<DirectMediaStreamTrackListener> aListener) {
  MutexAutoLock lock(mMutex);

  RefPtr<DirectMediaStreamTrackListener> listener = aListener;
  LOG(LogLevel::Debug,
      ("%p: Adding direct track listener %p to source stream %p", GraphImpl(),
       listener.get(), this));

  MOZ_ASSERT(mType == MediaSegment::VIDEO);
  for (const auto& l : mDirectTrackListeners) {
    if (l == listener) {
      listener->NotifyDirectListenerInstalled(
          DirectMediaStreamTrackListener::InstallationResult::ALREADY_EXISTS);
      return;
    }
  }

  mDirectTrackListeners.AppendElement(listener);

  LOG(LogLevel::Debug,
      ("%p: Added direct track listener %p", GraphImpl(), listener.get()));
  listener->NotifyDirectListenerInstalled(
      DirectMediaStreamTrackListener::InstallationResult::SUCCESS);

  if (mEnded) {
    return;
  }

  // Pass buffered data to the listener
  VideoSegment bufferedData;
  size_t videoFrames = 0;
  VideoSegment& segment = *GetData<VideoSegment>();
  for (VideoSegment::ConstChunkIterator iter(segment); !iter.IsEnded();
       iter.Next()) {
    if (iter->mTimeStamp.IsNull()) {
      // No timestamp means this is only for the graph's internal book-keeping,
      // denoting a late start of the stream.
      continue;
    }
    ++videoFrames;
    bufferedData.AppendFrame(do_AddRef(iter->mFrame.GetImage()),
                             iter->mFrame.GetIntrinsicSize(),
                             iter->mFrame.GetPrincipalHandle(),
                             iter->mFrame.GetForceBlack(), iter->mTimeStamp);
  }

  VideoSegment& video = static_cast<VideoSegment&>(*mUpdateTrack->mData);
  for (VideoSegment::ConstChunkIterator iter(video); !iter.IsEnded();
       iter.Next()) {
    ++videoFrames;
    MOZ_ASSERT(!iter->mTimeStamp.IsNull());
    bufferedData.AppendFrame(do_AddRef(iter->mFrame.GetImage()),
                             iter->mFrame.GetIntrinsicSize(),
                             iter->mFrame.GetPrincipalHandle(),
                             iter->mFrame.GetForceBlack(), iter->mTimeStamp);
  }

  LOG(LogLevel::Info,
      ("%p: Notifying direct listener %p of %zu video frames and duration "
       "%" PRId64,
       GraphImpl(), listener.get(), videoFrames, bufferedData.GetDuration()));
  listener->NotifyRealtimeTrackData(Graph(), 0, bufferedData);
}

void SourceMediaStream::RemoveDirectListenerImpl(
    DirectMediaStreamTrackListener* aListener) {
  MutexAutoLock lock(mMutex);
  for (int32_t i = mDirectTrackListeners.Length() - 1; i >= 0; --i) {
    const RefPtr<DirectMediaStreamTrackListener>& l = mDirectTrackListeners[i];
    if (l == aListener) {
      aListener->NotifyDirectListenerUninstalled();
      mDirectTrackListeners.RemoveElementAt(i);
    }
  }
}

void SourceMediaStream::End() {
  MutexAutoLock lock(mMutex);
  if (!mUpdateTrack) {
    // Already ended
    return;
  }
  mUpdateTrack->mEnded = true;
  if (auto graph = GraphImpl()) {
    graph->EnsureNextIteration();
  }
}

void SourceMediaStream::SetEnabledImpl(DisabledTrackMode aMode) {
  {
    MutexAutoLock lock(mMutex);
    for (const auto& l : mDirectTrackListeners) {
      DisabledTrackMode oldMode = mDisabledMode;
      bool oldEnabled = oldMode == DisabledTrackMode::ENABLED;
      if (!oldEnabled && aMode == DisabledTrackMode::ENABLED) {
        LOG(LogLevel::Debug, ("%p: SourceMediaStream %p setting "
                              "direct listener enabled",
                              GraphImpl(), this));
        l->DecreaseDisabled(oldMode);
      } else if (oldEnabled && aMode != DisabledTrackMode::ENABLED) {
        LOG(LogLevel::Debug, ("%p: SourceMediaStream %p setting "
                              "direct listener disabled",
                              GraphImpl(), this));
        l->IncreaseDisabled(aMode);
      }
    }
  }
  MediaStream::SetEnabledImpl(aMode);
}

void SourceMediaStream::RemoveAllDirectListenersImpl() {
  GraphImpl()->AssertOnGraphThreadOrNotRunning();

  auto directListeners(mDirectTrackListeners);
  for (auto& l : directListeners) {
    l->NotifyDirectListenerUninstalled();
  }
  mDirectTrackListeners.Clear();
}

SourceMediaStream::~SourceMediaStream() {}

void MediaInputPort::Init() {
  LOG(LogLevel::Debug, ("%p: Adding MediaInputPort %p (from %p to %p)",
                        mSource->GraphImpl(), this, mSource, mDest));
  // Only connect the port if it wasn't disconnected on allocation.
  if (mSource) {
    mSource->AddConsumer(this);
    mDest->AddInput(this);
  }
  // mPortCount decremented via MediaInputPort::Destroy's message
  ++mGraph->mPortCount;
}

void MediaInputPort::Disconnect() {
  GraphImpl()->AssertOnGraphThreadOrNotRunning();
  NS_ASSERTION(!mSource == !mDest,
               "mSource must either both be null or both non-null");
  if (!mSource) return;

  mSource->RemoveConsumer(this);
  mDest->RemoveInput(this);
  mSource = nullptr;
  mDest = nullptr;

  GraphImpl()->SetStreamOrderDirty();
}

MediaInputPort::InputInterval MediaInputPort::GetNextInputInterval(
    MediaInputPort const* aPort, GraphTime aTime) {
  InputInterval result = {GRAPH_TIME_MAX, GRAPH_TIME_MAX, false};
  if (!aPort) {
    result.mStart = aTime;
    result.mInputIsBlocked = true;
    return result;
  }
  if (aTime >= aPort->mDest->mStartBlocking) {
    return result;
  }
  result.mStart = aTime;
  result.mEnd = aPort->mDest->mStartBlocking;
  result.mInputIsBlocked = aTime >= aPort->mSource->mStartBlocking;
  if (!result.mInputIsBlocked) {
    result.mEnd = std::min(result.mEnd, aPort->mSource->mStartBlocking);
  }
  return result;
}

void MediaInputPort::Suspended() { mDest->InputSuspended(this); }

void MediaInputPort::Resumed() { mDest->InputResumed(this); }

void MediaInputPort::Destroy() {
  class Message : public ControlMessage {
   public:
    explicit Message(MediaInputPort* aPort)
        : ControlMessage(nullptr), mPort(aPort) {}
    void Run() override {
      mPort->Disconnect();
      --mPort->GraphImpl()->mPortCount;
      mPort->SetGraphImpl(nullptr);
      NS_RELEASE(mPort);
    }
    void RunDuringShutdown() override { Run(); }
    MediaInputPort* mPort;
  };
  // Keep a reference to the graph, since Message might RunDuringShutdown()
  // synchronously and make GraphImpl() invalid.
  RefPtr<MediaStreamGraphImpl> graph = GraphImpl();
  graph->AppendMessage(MakeUnique<Message>(this));
  --graph->mMainThreadPortCount;
}

MediaStreamGraphImpl* MediaInputPort::GraphImpl() { return mGraph; }

MediaStreamGraph* MediaInputPort::Graph() { return mGraph; }

void MediaInputPort::SetGraphImpl(MediaStreamGraphImpl* aGraph) {
  MOZ_ASSERT(!mGraph || !aGraph, "Should only be set once");
  mGraph = aGraph;
}

already_AddRefed<MediaInputPort> ProcessedMediaStream::AllocateInputPort(
    MediaStream* aStream, uint16_t aInputNumber, uint16_t aOutputNumber) {
  // This method creates two references to the MediaInputPort: one for
  // the main thread, and one for the MediaStreamGraph.
  class Message : public ControlMessage {
   public:
    explicit Message(MediaInputPort* aPort)
        : ControlMessage(aPort->GetDestination()), mPort(aPort) {}
    void Run() override {
      mPort->Init();
      // The graph holds its reference implicitly
      mPort->GraphImpl()->SetStreamOrderDirty();
      Unused << mPort.forget();
    }
    void RunDuringShutdown() override { Run(); }
    RefPtr<MediaInputPort> mPort;
  };

  MOZ_DIAGNOSTIC_ASSERT(aStream->mType == mType);
  RefPtr<MediaInputPort> port;
  if (aStream->IsDestroyed()) {
    // Create a port that's disconnected, which is what it'd be after its source
    // stream is Destroy()ed normally. Disconnect() is idempotent so destroying
    // this later is fine.
    port = new MediaInputPort(nullptr, nullptr, aInputNumber, aOutputNumber);
  } else {
    MOZ_ASSERT(aStream->GraphImpl() == GraphImpl());
    port = new MediaInputPort(aStream, this, aInputNumber, aOutputNumber);
  }
  port->SetGraphImpl(GraphImpl());
  ++GraphImpl()->mMainThreadPortCount;
  GraphImpl()->AppendMessage(MakeUnique<Message>(port));
  return port.forget();
}

void ProcessedMediaStream::QueueSetAutoend(bool aAutoend) {
  class Message : public ControlMessage {
   public:
    Message(ProcessedMediaStream* aStream, bool aAutoend)
        : ControlMessage(aStream), mAutoend(aAutoend) {}
    void Run() override {
      static_cast<ProcessedMediaStream*>(mStream)->SetAutoendImpl(mAutoend);
    }
    bool mAutoend;
  };
  GraphImpl()->AppendMessage(MakeUnique<Message>(this, aAutoend));
}

void ProcessedMediaStream::DestroyImpl() {
  for (int32_t i = mInputs.Length() - 1; i >= 0; --i) {
    mInputs[i]->Disconnect();
  }

  for (int32_t i = mSuspendedInputs.Length() - 1; i >= 0; --i) {
    mSuspendedInputs[i]->Disconnect();
  }

  MediaStream::DestroyImpl();
  // The stream order is only important if there are connections, in which
  // case MediaInputPort::Disconnect() called SetStreamOrderDirty().
  // MediaStreamGraphImpl::RemoveStreamGraphThread() will also call
  // SetStreamOrderDirty(), for other reasons.
}

MediaStreamGraphImpl::MediaStreamGraphImpl(GraphDriverType aDriverRequested,
                                           GraphRunType aRunTypeRequested,
                                           TrackRate aSampleRate,
                                           uint32_t aChannelCount,
                                           AbstractThread* aMainThread)
    : MediaStreamGraph(aSampleRate),
      mGraphRunner(aRunTypeRequested == SINGLE_THREAD ? new GraphRunner(this)
                                                      : nullptr),
      mFirstCycleBreaker(0)
      // An offline graph is not initially processing.
      ,
      mEndTime(aDriverRequested == OFFLINE_THREAD_DRIVER ? 0 : GRAPH_TIME_MAX),
      mPortCount(0),
      mInputDeviceID(nullptr),
      mOutputDeviceID(nullptr),
      mNeedAnotherIteration(false),
      mGraphDriverAsleep(false),
      mMonitor("MediaStreamGraphImpl"),
      mLifecycleState(LIFECYCLE_THREAD_NOT_STARTED),
      mForceShutDown(false),
      mPostedRunInStableStateEvent(false),
      mDetectedNotRunning(false),
      mPostedRunInStableState(false),
      mRealtime(aDriverRequested != OFFLINE_THREAD_DRIVER),
      mStreamOrderDirty(false),
      mAbstractMainThread(aMainThread),
      mSelfRef(this),
      mOutputChannels(aChannelCount),
      mGlobalVolume(CubebUtils::GetVolumeScale())
#ifdef DEBUG
      ,
      mCanRunMessagesSynchronously(false)
#endif
      ,
      mMainThreadGraphTime(0, "MediaStreamGraphImpl::mMainThreadGraphTime"),
      mAudioOutputLatency(0.0) {
  if (mRealtime) {
    if (aDriverRequested == AUDIO_THREAD_DRIVER) {
      // Always start with zero input channels, and no particular preferences
      // for the input channel.
      mDriver = new AudioCallbackDriver(this, 0, AudioInputType::Unknown);
    } else {
      mDriver = new SystemClockDriver(this);
    }

#ifdef TRACING
    // This is a noop if the logger has not been enabled.
    gMSGTraceLogger.Start();
    gMSGTraceLogger.Log("[");
#endif
  } else {
    mDriver = new OfflineClockDriver(this, MEDIA_GRAPH_TARGET_PERIOD_MS);
  }

  mLastMainThreadUpdate = TimeStamp::Now();

  RegisterWeakAsyncMemoryReporter(this);
}

AbstractThread* MediaStreamGraph::AbstractMainThread() {
  MOZ_ASSERT(static_cast<MediaStreamGraphImpl*>(this)->mAbstractMainThread);
  return static_cast<MediaStreamGraphImpl*>(this)->mAbstractMainThread;
}

#ifdef DEBUG
bool MediaStreamGraphImpl::RunByGraphDriver(GraphDriver* aDriver) {
  return aDriver->OnThread() ||
         (mGraphRunner && mGraphRunner->RunByGraphDriver(aDriver));
}
#endif

void MediaStreamGraphImpl::Destroy() {
  // First unregister from memory reporting.
  UnregisterWeakMemoryReporter(this);

  // Clear the self reference which will destroy this instance if all
  // associated GraphDrivers are destroyed.
  mSelfRef = nullptr;
}

static uint32_t WindowToHash(nsPIDOMWindowInner* aWindow,
                             TrackRate aSampleRate) {
  uint32_t hashkey = 0;

  hashkey = AddToHash(hashkey, aWindow);
  hashkey = AddToHash(hashkey, aSampleRate);

  return hashkey;
}

MediaStreamGraph* MediaStreamGraph::GetInstanceIfExists(
    nsPIDOMWindowInner* aWindow, TrackRate aSampleRate) {
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");

  TrackRate sampleRate =
      aSampleRate ? aSampleRate : CubebUtils::PreferredSampleRate();
  uint32_t hashkey = WindowToHash(aWindow, sampleRate);

  MediaStreamGraphImpl* graph = nullptr;
  gGraphs.Get(hashkey, &graph);
  return graph;
}

MediaStreamGraph* MediaStreamGraph::GetInstance(
    MediaStreamGraph::GraphDriverType aGraphDriverRequested,
    nsPIDOMWindowInner* aWindow, TrackRate aSampleRate) {
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");

  TrackRate sampleRate =
      aSampleRate ? aSampleRate : CubebUtils::PreferredSampleRate();
  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(
      GetInstanceIfExists(aWindow, sampleRate));

  if (!graph) {
    AbstractThread* mainThread;
    if (aWindow) {
      mainThread =
          aWindow->AsGlobal()->AbstractMainThreadFor(TaskCategory::Other);
    } else {
      // Uncommon case, only for some old configuration of webspeech.
      mainThread = AbstractThread::MainThread();
    }

    GraphRunType runType = DIRECT_DRIVER;
    if (aGraphDriverRequested != OFFLINE_THREAD_DRIVER &&
        (StaticPrefs::dom_audioworklet_enabled() ||
         Preferences::GetBool("media.audiograph.single_thread.enabled",
                              false))) {
      runType = SINGLE_THREAD;
    }

    // In a real time graph, the number of output channels is determined by
    // the underlying number of channel of the default audio output device, and
    // capped to 8.
    uint32_t channelCount =
        std::min<uint32_t>(8, CubebUtils::MaxNumberOfChannels());
    graph = new MediaStreamGraphImpl(aGraphDriverRequested, runType, sampleRate,
                                     channelCount, mainThread);

    if (!graph->IsNonRealtime()) {
      graph->AddShutdownBlocker();
    }

    uint32_t hashkey = WindowToHash(aWindow, sampleRate);
    gGraphs.Put(hashkey, graph);

    LOG(LogLevel::Debug,
        ("Starting up MediaStreamGraph %p for window %p", graph, aWindow));
  }

  return graph;
}

MediaStreamGraph* MediaStreamGraph::CreateNonRealtimeInstance(
    TrackRate aSampleRate, nsPIDOMWindowInner* aWindow) {
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");

  AbstractThread* mainThread = AbstractThread::MainThread();
  // aWindow can be null when the document is being unlinked, so this works when
  // with a generic main thread if that's the case.
  if (aWindow) {
    mainThread =
        aWindow->AsGlobal()->AbstractMainThreadFor(TaskCategory::Other);
  }

  // Offline graphs have 0 output channel count: they write the output to a
  // buffer, not an audio output stream.
  MediaStreamGraphImpl* graph = new MediaStreamGraphImpl(
      OFFLINE_THREAD_DRIVER, DIRECT_DRIVER, aSampleRate, 0, mainThread);

  LOG(LogLevel::Debug, ("Starting up Offline MediaStreamGraph %p", graph));

  return graph;
}

void MediaStreamGraph::DestroyNonRealtimeInstance(MediaStreamGraph* aGraph) {
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");
  MOZ_ASSERT(aGraph->IsNonRealtime(),
             "Should not destroy the global graph here");

  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(aGraph);

  graph->ForceShutDown();
}

NS_IMPL_ISUPPORTS(MediaStreamGraphImpl, nsIMemoryReporter, nsITimerCallback,
                  nsINamed)

NS_IMETHODIMP
MediaStreamGraphImpl::CollectReports(nsIHandleReportCallback* aHandleReport,
                                     nsISupports* aData, bool aAnonymize) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mMainThreadStreamCount == 0) {
    // No streams to report.
    FinishCollectReports(aHandleReport, aData, nsTArray<AudioNodeSizes>());
    return NS_OK;
  }

  class Message final : public ControlMessage {
   public:
    Message(MediaStreamGraphImpl* aGraph,
            nsIHandleReportCallback* aHandleReport, nsISupports* aHandlerData)
        : ControlMessage(nullptr),
          mGraph(aGraph),
          mHandleReport(aHandleReport),
          mHandlerData(aHandlerData) {}
    void Run() override {
      mGraph->CollectSizesForMemoryReport(mHandleReport.forget(),
                                          mHandlerData.forget());
    }
    void RunDuringShutdown() override {
      // Run this message during shutdown too, so that endReports is called.
      Run();
    }
    MediaStreamGraphImpl* mGraph;
    // nsMemoryReporterManager keeps the callback and data alive only if it
    // does not time out.
    nsCOMPtr<nsIHandleReportCallback> mHandleReport;
    nsCOMPtr<nsISupports> mHandlerData;
  };

  AppendMessage(MakeUnique<Message>(this, aHandleReport, aData));

  return NS_OK;
}

void MediaStreamGraphImpl::CollectSizesForMemoryReport(
    already_AddRefed<nsIHandleReportCallback> aHandleReport,
    already_AddRefed<nsISupports> aHandlerData) {
  class FinishCollectRunnable final : public Runnable {
   public:
    explicit FinishCollectRunnable(
        already_AddRefed<nsIHandleReportCallback> aHandleReport,
        already_AddRefed<nsISupports> aHandlerData)
        : mozilla::Runnable("FinishCollectRunnable"),
          mHandleReport(aHandleReport),
          mHandlerData(aHandlerData) {}

    NS_IMETHOD Run() override {
      MediaStreamGraphImpl::FinishCollectReports(mHandleReport, mHandlerData,
                                                 std::move(mAudioStreamSizes));
      return NS_OK;
    }

    nsTArray<AudioNodeSizes> mAudioStreamSizes;

   private:
    ~FinishCollectRunnable() {}

    // Avoiding nsCOMPtr because NSCAP_ASSERT_NO_QUERY_NEEDED in its
    // constructor modifies the ref-count, which cannot be done off main
    // thread.
    RefPtr<nsIHandleReportCallback> mHandleReport;
    RefPtr<nsISupports> mHandlerData;
  };

  RefPtr<FinishCollectRunnable> runnable = new FinishCollectRunnable(
      std::move(aHandleReport), std::move(aHandlerData));

  auto audioStreamSizes = &runnable->mAudioStreamSizes;

  for (MediaStream* s : AllStreams()) {
    AudioNodeStream* stream = s->AsAudioNodeStream();
    if (stream) {
      AudioNodeSizes* usage = audioStreamSizes->AppendElement();
      stream->SizeOfAudioNodesIncludingThis(MallocSizeOf, *usage);
    }
  }

  mAbstractMainThread->Dispatch(runnable.forget());
}

void MediaStreamGraphImpl::FinishCollectReports(
    nsIHandleReportCallback* aHandleReport, nsISupports* aData,
    const nsTArray<AudioNodeSizes>& aAudioStreamSizes) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIMemoryReporterManager> manager =
      do_GetService("@mozilla.org/memory-reporter-manager;1");

  if (!manager) return;

#define REPORT(_path, _amount, _desc)                                    \
  aHandleReport->Callback(EmptyCString(), _path, KIND_HEAP, UNITS_BYTES, \
                          _amount, NS_LITERAL_CSTRING(_desc), aData);

  for (size_t i = 0; i < aAudioStreamSizes.Length(); i++) {
    const AudioNodeSizes& usage = aAudioStreamSizes[i];
    const char* const nodeType =
        usage.mNodeType ? usage.mNodeType : "<unknown>";

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
           hrtfLoaders, "Memory used by PannerNode databases (Web Audio).");
  }

#undef REPORT

  manager->EndReport();
}

SourceMediaStream* MediaStreamGraph::CreateSourceStream(
    MediaSegment::Type aType) {
  SourceMediaStream* stream = new SourceMediaStream(aType, GraphRate());
  AddStream(stream);
  return stream;
}

ProcessedMediaStream* MediaStreamGraph::CreateTrackUnionStream(
    MediaSegment::Type aType) {
  TrackUnionStream* stream = new TrackUnionStream(GraphRate(), aType);
  AddStream(stream);
  return stream;
}

AudioCaptureStream* MediaStreamGraph::CreateAudioCaptureStream() {
  AudioCaptureStream* stream = new AudioCaptureStream(GraphRate());
  AddStream(stream);
  return stream;
}

void MediaStreamGraph::AddStream(MediaStream* aStream) {
  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(this);
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  if (graph->mRealtime) {
    bool found = false;
    for (auto iter = gGraphs.ConstIter(); !iter.Done(); iter.Next()) {
      if (iter.UserData() == graph) {
        found = true;
        break;
      }
    }
    MOZ_DIAGNOSTIC_ASSERT(found, "Graph must not be shutting down");
  }
#endif
  NS_ADDREF(aStream);
  aStream->SetGraphImpl(graph);
  ++graph->mMainThreadStreamCount;
  graph->AppendMessage(MakeUnique<CreateMessage>(aStream));
}

void MediaStreamGraphImpl::RemoveStream(MediaStream* aStream) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(mMainThreadStreamCount > 0);
  if (--mMainThreadStreamCount == 0) {
    LOG(LogLevel::Info, ("MediaStreamGraph %p, last stream %p removed from "
                         "main thread. Graph will shut down.",
                         this, aStream));
    // Find the graph in the hash table and remove it.
    for (auto iter = gGraphs.Iter(); !iter.Done(); iter.Next()) {
      if (iter.UserData() == this) {
        iter.Remove();
        break;
      }
    }
  }
}

class GraphStartedRunnable final : public Runnable {
 public:
  GraphStartedRunnable(AudioNodeStream* aStream, MediaStreamGraph* aGraph)
      : Runnable("GraphStartedRunnable"), mStream(aStream), mGraph(aGraph) {}

  NS_IMETHOD Run() override {
    mGraph->NotifyWhenGraphStarted(mStream);
    return NS_OK;
  }

 private:
  RefPtr<AudioNodeStream> mStream;
  MediaStreamGraph* mGraph;
};

void MediaStreamGraph::NotifyWhenGraphStarted(AudioNodeStream* aStream) {
  MOZ_ASSERT(NS_IsMainThread());

  class GraphStartedNotificationControlMessage : public ControlMessage {
   public:
    explicit GraphStartedNotificationControlMessage(AudioNodeStream* aStream)
        : ControlMessage(aStream) {}
    void Run() override {
      // This runs on the graph thread, so when this runs, and the current
      // driver is an AudioCallbackDriver, we know the audio hardware is
      // started. If not, we are going to switch soon, keep reposting this
      // ControlMessage.
      MediaStreamGraphImpl* graphImpl = mStream->GraphImpl();
      if (graphImpl->CurrentDriver()->AsAudioCallbackDriver()) {
        nsCOMPtr<nsIRunnable> event = new dom::StateChangeTask(
            mStream->AsAudioNodeStream(), nullptr, AudioContextState::Running);
        graphImpl->Dispatch(event.forget());
      } else {
        nsCOMPtr<nsIRunnable> event = new GraphStartedRunnable(
            mStream->AsAudioNodeStream(), mStream->Graph());
        graphImpl->Dispatch(event.forget());
      }
    }
    void RunDuringShutdown() override {}
  };

  if (!aStream->IsDestroyed()) {
    MediaStreamGraphImpl* graphImpl = static_cast<MediaStreamGraphImpl*>(this);
    graphImpl->AppendMessage(
        MakeUnique<GraphStartedNotificationControlMessage>(aStream));
  }
}

void MediaStreamGraphImpl::IncrementSuspendCount(MediaStream* aStream) {
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  bool wasSuspended = aStream->IsSuspended();
  aStream->IncrementSuspendCount();
  if (!wasSuspended && aStream->IsSuspended()) {
    MOZ_ASSERT(mStreams.Contains(aStream));
    mStreams.RemoveElement(aStream);
    mSuspendedStreams.AppendElement(aStream);
    SetStreamOrderDirty();
  }
}

void MediaStreamGraphImpl::DecrementSuspendCount(MediaStream* aStream) {
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
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

void MediaStreamGraphImpl::SuspendOrResumeStreams(
    AudioContextOperation aAudioContextOperation,
    const nsTArray<MediaStream*>& aStreamSet) {
  MOZ_ASSERT(OnGraphThreadOrNotRunning());
  // For our purpose, Suspend and Close are equivalent: we want to remove the
  // streams from the set of streams that are going to be processed.
  for (MediaStream* stream : aStreamSet) {
    if (aAudioContextOperation == AudioContextOperation::Resume) {
      DecrementSuspendCount(stream);
    } else {
      IncrementSuspendCount(stream);
    }
  }
  LOG(LogLevel::Debug, ("Moving streams between suspended and running"
                        "state: mStreams: %zu, mSuspendedStreams: %zu",
                        mStreams.Length(), mSuspendedStreams.Length()));
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

void MediaStreamGraphImpl::AudioContextOperationCompleted(
    MediaStream* aStream, void* aPromise, AudioContextOperation aOperation,
    AudioContextOperationFlags aFlags) {
  if (aFlags != AudioContextOperationFlags::SendStateChange) {
    MOZ_ASSERT(!aPromise);
    return;
  }
  // This can be called from the thread created to do cubeb operation, or the
  // MSG thread. The pointers passed back here are refcounted, so are still
  // alive.
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
    default:
      MOZ_CRASH("Not handled.");
  }

  nsCOMPtr<nsIRunnable> event =
      new dom::StateChangeTask(aStream->AsAudioNodeStream(), aPromise, state);
  mAbstractMainThread->Dispatch(event.forget());
}

void MediaStreamGraphImpl::ApplyAudioContextOperationImpl(
    MediaStream* aDestinationStream, const nsTArray<MediaStream*>& aStreams,
    AudioContextOperation aOperation, void* aPromise,
    AudioContextOperationFlags aFlags) {
  MOZ_ASSERT(OnGraphThread());

  SuspendOrResumeStreams(aOperation, aStreams);

  bool switching = false;
  GraphDriver* nextDriver = nullptr;
  {
    MonitorAutoLock lock(mMonitor);
    switching = CurrentDriver()->Switching();
    if (switching) {
      nextDriver = CurrentDriver()->NextDriver();
    }
  }

  // If we have suspended the last AudioContext, and we don't have other
  // streams that have audio, this graph will automatically switch to a
  // SystemCallbackDriver, because it can't find a MediaStream that has an audio
  // track. When resuming, force switching to an AudioCallbackDriver (if we're
  // not already switching). It would have happened at the next iteration
  // anyways, but doing this now save some time.
  if (aOperation == AudioContextOperation::Resume) {
    if (!CurrentDriver()->AsAudioCallbackDriver()) {
      AudioCallbackDriver* driver;
      if (switching) {
        MOZ_ASSERT(nextDriver->AsAudioCallbackDriver());
        driver = nextDriver->AsAudioCallbackDriver();
      } else {
        driver = new AudioCallbackDriver(this, AudioInputChannelCount(),
                                         AudioInputDevicePreference());
        MonitorAutoLock lock(mMonitor);
        CurrentDriver()->SwitchAtNextIteration(driver);
      }
      driver->EnqueueStreamAndPromiseForOperation(aDestinationStream, aPromise,
                                                  aOperation, aFlags);
    } else {
      // We are resuming a context, but we are already using an
      // AudioCallbackDriver, we can resolve the promise now.
      AudioContextOperationCompleted(aDestinationStream, aPromise, aOperation,
                                     aFlags);
    }
  }
  // Close, suspend: check if we are going to switch to a
  // SystemAudioCallbackDriver, and pass the promise to the AudioCallbackDriver
  // if that's the case, so it can notify the content.
  // This is the same logic as in UpdateStreamOrder, but it's simpler to have it
  // here as well so we don't have to store the Promise(s) on the Graph.
  if (aOperation != AudioContextOperation::Resume) {
    bool audioTrackPresent = AudioTrackPresent();

    if (!audioTrackPresent && CurrentDriver()->AsAudioCallbackDriver()) {
      CurrentDriver()
          ->AsAudioCallbackDriver()
          ->EnqueueStreamAndPromiseForOperation(aDestinationStream, aPromise,
                                                aOperation, aFlags);

      SystemClockDriver* driver;
      if (!nextDriver) {
        driver = new SystemClockDriver(this);
        MonitorAutoLock lock(mMonitor);
        CurrentDriver()->SwitchAtNextIteration(driver);
      }
      // We are closing or suspending an AudioContext, but we just got resumed.
      // Queue the operation on the next driver so that the ordering is
      // preserved.
    } else if (!audioTrackPresent && switching) {
      MOZ_ASSERT(nextDriver->AsAudioCallbackDriver() ||
                 nextDriver->AsSystemClockDriver()->IsFallback());
      if (nextDriver->AsAudioCallbackDriver()) {
        nextDriver->AsAudioCallbackDriver()
            ->EnqueueStreamAndPromiseForOperation(aDestinationStream, aPromise,
                                                  aOperation, aFlags);
      } else {
        // If this is not an AudioCallbackDriver, this means we failed opening
        // an AudioCallbackDriver in the past, and we're constantly trying to
        // re-open an new audio stream, but are running this graph that has an
        // audio track off a SystemClockDriver for now to keep things moving.
        // This is the case where we're trying to switch an an system driver
        // (because suspend or close have been called on an AudioContext, or
        // we've closed the page), but we're already running one. We can just
        // resolve the promise now: we're already running off a system thread.
        AudioContextOperationCompleted(aDestinationStream, aPromise, aOperation,
                                       aFlags);
      }
    } else {
      // We are closing or suspending an AudioContext, but something else is
      // using the audio stream, we can resolve the promise now.
      AudioContextOperationCompleted(aDestinationStream, aPromise, aOperation,
                                     aFlags);
    }
  }
}

void MediaStreamGraph::ApplyAudioContextOperation(
    MediaStream* aDestinationStream, const nsTArray<MediaStream*>& aStreams,
    AudioContextOperation aOperation, void* aPromise,
    AudioContextOperationFlags aFlags) {
  class AudioContextOperationControlMessage : public ControlMessage {
   public:
    AudioContextOperationControlMessage(MediaStream* aDestinationStream,
                                        const nsTArray<MediaStream*>& aStreams,
                                        AudioContextOperation aOperation,
                                        void* aPromise,
                                        AudioContextOperationFlags aFlags)
        : ControlMessage(aDestinationStream),
          mStreams(aStreams),
          mAudioContextOperation(aOperation),
          mPromise(aPromise),
          mFlags(aFlags) {}
    void Run() override {
      mStream->GraphImpl()->ApplyAudioContextOperationImpl(
          mStream, mStreams, mAudioContextOperation, mPromise, mFlags);
    }
    void RunDuringShutdown() override {
      MOZ_ASSERT(mAudioContextOperation == AudioContextOperation::Close,
                 "We should be reviving the graph?");
    }

   private:
    // We don't need strong references here for the same reason ControlMessage
    // doesn't.
    nsTArray<MediaStream*> mStreams;
    AudioContextOperation mAudioContextOperation;
    void* mPromise;
    AudioContextOperationFlags mFlags;
  };

  MediaStreamGraphImpl* graphImpl = static_cast<MediaStreamGraphImpl*>(this);
  graphImpl->AppendMessage(MakeUnique<AudioContextOperationControlMessage>(
      aDestinationStream, aStreams, aOperation, aPromise, aFlags));
}

double MediaStreamGraph::AudioOutputLatency() {
  return static_cast<MediaStreamGraphImpl*>(this)->AudioOutputLatency();
}

double MediaStreamGraphImpl::AudioOutputLatency() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mAudioOutputLatency != 0.0) {
    return mAudioOutputLatency;
  }
  MonitorAutoLock lock(mMonitor);
  if (CurrentDriver()->AsAudioCallbackDriver()) {
    mAudioOutputLatency = CurrentDriver()
                              ->AsAudioCallbackDriver()
                              ->AudioOutputLatency()
                              .ToSeconds();
  } else {
    // Failure mode: return 0.0 if running on a normal thread.
    mAudioOutputLatency = 0.0;
  }

  return mAudioOutputLatency;
}

bool MediaStreamGraph::IsNonRealtime() const {
  return !static_cast<const MediaStreamGraphImpl*>(this)->mRealtime;
}

void MediaStreamGraph::StartNonRealtimeProcessing(uint32_t aTicksToProcess) {
  MOZ_ASSERT(NS_IsMainThread(), "main thread only");

  MediaStreamGraphImpl* graph = static_cast<MediaStreamGraphImpl*>(this);
  NS_ASSERTION(!graph->mRealtime, "non-realtime only");

  class Message : public ControlMessage {
   public:
    explicit Message(MediaStreamGraphImpl* aGraph, uint32_t aTicksToProcess)
        : ControlMessage(nullptr),
          mGraph(aGraph),
          mTicksToProcess(aTicksToProcess) {}
    void Run() override {
      MOZ_ASSERT(mGraph->mEndTime == 0,
                 "StartNonRealtimeProcessing should be called only once");
      mGraph->mEndTime = mGraph->RoundUpToEndOfAudioBlock(
          mGraph->mStateComputedTime + mTicksToProcess);
    }
    // The graph owns this message.
    MediaStreamGraphImpl* MOZ_NON_OWNING_REF mGraph;
    uint32_t mTicksToProcess;
  };

  graph->AppendMessage(MakeUnique<Message>(graph, aTicksToProcess));
}

void ProcessedMediaStream::AddInput(MediaInputPort* aPort) {
  MediaStream* s = aPort->GetSource();
  if (!s->IsSuspended()) {
    mInputs.AppendElement(aPort);
  } else {
    mSuspendedInputs.AppendElement(aPort);
  }
  GraphImpl()->SetStreamOrderDirty();
}

void ProcessedMediaStream::InputSuspended(MediaInputPort* aPort) {
  GraphImpl()->AssertOnGraphThreadOrNotRunning();
  mInputs.RemoveElement(aPort);
  mSuspendedInputs.AppendElement(aPort);
  GraphImpl()->SetStreamOrderDirty();
}

void ProcessedMediaStream::InputResumed(MediaInputPort* aPort) {
  GraphImpl()->AssertOnGraphThreadOrNotRunning();
  mSuspendedInputs.RemoveElement(aPort);
  mInputs.AppendElement(aPort);
  GraphImpl()->SetStreamOrderDirty();
}

void MediaStreamGraph::RegisterCaptureStreamForWindow(
    uint64_t aWindowId, ProcessedMediaStream* aCaptureStream) {
  MOZ_ASSERT(NS_IsMainThread());
  MediaStreamGraphImpl* graphImpl = static_cast<MediaStreamGraphImpl*>(this);
  graphImpl->RegisterCaptureStreamForWindow(aWindowId, aCaptureStream);
}

void MediaStreamGraphImpl::RegisterCaptureStreamForWindow(
    uint64_t aWindowId, ProcessedMediaStream* aCaptureStream) {
  MOZ_ASSERT(NS_IsMainThread());
  WindowAndStream winAndStream;
  winAndStream.mWindowId = aWindowId;
  winAndStream.mCaptureStreamSink = aCaptureStream;
  mWindowCaptureStreams.AppendElement(winAndStream);
}

void MediaStreamGraph::UnregisterCaptureStreamForWindow(uint64_t aWindowId) {
  MOZ_ASSERT(NS_IsMainThread());
  MediaStreamGraphImpl* graphImpl = static_cast<MediaStreamGraphImpl*>(this);
  graphImpl->UnregisterCaptureStreamForWindow(aWindowId);
}

void MediaStreamGraphImpl::UnregisterCaptureStreamForWindow(
    uint64_t aWindowId) {
  MOZ_ASSERT(NS_IsMainThread());
  for (int32_t i = mWindowCaptureStreams.Length() - 1; i >= 0; i--) {
    if (mWindowCaptureStreams[i].mWindowId == aWindowId) {
      mWindowCaptureStreams.RemoveElementAt(i);
    }
  }
}

already_AddRefed<MediaInputPort> MediaStreamGraph::ConnectToCaptureStream(
    uint64_t aWindowId, MediaStream* aMediaStream) {
  return aMediaStream->GraphImpl()->ConnectToCaptureStream(aWindowId,
                                                           aMediaStream);
}

already_AddRefed<MediaInputPort> MediaStreamGraphImpl::ConnectToCaptureStream(
    uint64_t aWindowId, MediaStream* aMediaStream) {
  MOZ_ASSERT(NS_IsMainThread());
  for (uint32_t i = 0; i < mWindowCaptureStreams.Length(); i++) {
    if (mWindowCaptureStreams[i].mWindowId == aWindowId) {
      ProcessedMediaStream* sink = mWindowCaptureStreams[i].mCaptureStreamSink;
      return sink->AllocateInputPort(aMediaStream);
    }
  }
  return nullptr;
}

void MediaStreamGraph::DispatchToMainThreadStableState(
    already_AddRefed<nsIRunnable> aRunnable) {
  AssertOnGraphThreadOrNotRunning();
  static_cast<MediaStreamGraphImpl*>(this)
      ->mPendingUpdateRunnables.AppendElement(std::move(aRunnable));
}

Watchable<mozilla::GraphTime>& MediaStreamGraphImpl::CurrentTime() {
  MOZ_ASSERT(NS_IsMainThread());
  return mMainThreadGraphTime;
}

}  // namespace mozilla
