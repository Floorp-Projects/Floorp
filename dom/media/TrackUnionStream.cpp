/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamGraphImpl.h"
#include "MediaStreamListener.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Unused.h"

#include "AudioSegment.h"
#include "VideoSegment.h"
#include "nsContentUtils.h"
#include "nsIAppShell.h"
#include "nsIObserver.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "nsWidgetsCID.h"
#include "prerror.h"
#include "mozilla/Logging.h"
#include "mozilla/Attributes.h"
#include "TrackUnionStream.h"
#include "ImageContainer.h"
#include "AudioChannelService.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioNodeExternalInputStream.h"
#include "webaudio/MediaStreamAudioDestinationNode.h"
#include <algorithm>
#include "DOMMediaStream.h"
#include "GeckoProfiler.h"
#ifdef MOZ_WEBRTC
#include "AudioOutputObserver.h"
#endif

using namespace mozilla::layers;
using namespace mozilla::dom;
using namespace mozilla::gfx;

namespace mozilla {

#ifdef STREAM_LOG
#undef STREAM_LOG
#endif

LazyLogModule gTrackUnionStreamLog("TrackUnionStream");
#define STREAM_LOG(type, msg) MOZ_LOG(gTrackUnionStreamLog, type, msg)

TrackUnionStream::TrackUnionStream() :
  ProcessedMediaStream(), mNextAvailableTrackID(1)
{
}

  void TrackUnionStream::RemoveInput(MediaInputPort* aPort)
  {
    STREAM_LOG(LogLevel::Debug, ("TrackUnionStream %p removing input %p", this, aPort));
    for (int32_t i = mTrackMap.Length() - 1; i >= 0; --i) {
      if (mTrackMap[i].mInputPort == aPort) {
        STREAM_LOG(LogLevel::Debug, ("TrackUnionStream %p removing trackmap entry %d", this, i));
        EndTrack(i);
        for (auto listener : mTrackMap[i].mOwnedDirectListeners) {
          // Remove listeners while the entry still exists.
          RemoveDirectTrackListenerImpl(listener, mTrackMap[i].mOutputTrackID);
        }
        mTrackMap.RemoveElementAt(i);
      }
    }
    ProcessedMediaStream::RemoveInput(aPort);
  }
  void TrackUnionStream::ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags)
  {
    if (IsFinishedOnGraphThread()) {
      return;
    }
    AutoTArray<bool,8> mappedTracksFinished;
    AutoTArray<bool,8> mappedTracksWithMatchingInputTracks;
    for (uint32_t i = 0; i < mTrackMap.Length(); ++i) {
      mappedTracksFinished.AppendElement(true);
      mappedTracksWithMatchingInputTracks.AppendElement(false);
    }

    AutoTArray<MediaInputPort*, 32> inputs(mInputs);
    inputs.AppendElements(mSuspendedInputs);

    bool allFinished = !inputs.IsEmpty();
    bool allHaveCurrentData = !inputs.IsEmpty();
    for (uint32_t i = 0; i < inputs.Length(); ++i) {
      MediaStream* stream = inputs[i]->GetSource();
      if (!stream->IsFinishedOnGraphThread()) {
        // XXX we really should check whether 'stream' has finished within time aTo,
        // not just that it's finishing when all its queued data eventually runs
        // out.
        allFinished = false;
      }
      if (!stream->HasCurrentData()) {
        allHaveCurrentData = false;
      }
      bool trackAdded = false;
      for (StreamTracks::TrackIter tracks(stream->GetStreamTracks());
           !tracks.IsEnded(); tracks.Next()) {
        bool found = false;
        for (uint32_t j = 0; j < mTrackMap.Length(); ++j) {
          TrackMapEntry* map = &mTrackMap[j];
          if (map->mInputPort == inputs[i] && map->mInputTrackID == tracks->GetID()) {
            bool trackFinished = false;
            StreamTracks::Track* outputTrack = mTracks.FindTrack(map->mOutputTrackID);
            found = true;
            if (!outputTrack || outputTrack->IsEnded() ||
                !inputs[i]->PassTrackThrough(tracks->GetID())) {
              trackFinished = true;
            } else {
              CopyTrackData(tracks.get(), j, aFrom, aTo, &trackFinished);
            }
            mappedTracksFinished[j] = trackFinished;
            mappedTracksWithMatchingInputTracks[j] = true;
            break;
          }
        }
        if (!found && inputs[i]->AllowCreationOf(tracks->GetID())) {
          bool trackFinished = false;
          trackAdded = true;
          uint32_t mapIndex = AddTrack(inputs[i], tracks.get(), aFrom);
          CopyTrackData(tracks.get(), mapIndex, aFrom, aTo, &trackFinished);
          mappedTracksFinished.AppendElement(trackFinished);
          mappedTracksWithMatchingInputTracks.AppendElement(true);
        }
      }
      if (trackAdded) {
        for (MediaStreamListener* l : mListeners) {
          l->NotifyFinishedTrackCreation(Graph());
        }
      }
    }
    for (int32_t i = mTrackMap.Length() - 1; i >= 0; --i) {
      if (mappedTracksFinished[i]) {
        EndTrack(i);
      } else {
        allFinished = false;
      }
      if (!mappedTracksWithMatchingInputTracks[i]) {
        for (auto listener : mTrackMap[i].mOwnedDirectListeners) {
          // Remove listeners while the entry still exists.
          RemoveDirectTrackListenerImpl(listener, mTrackMap[i].mOutputTrackID);
        }
        mTrackMap.RemoveElementAt(i);
      }
    }
    if (allFinished && mAutofinish && (aFlags & ALLOW_FINISH)) {
      // All streams have finished and won't add any more tracks, and
      // all our tracks have actually finished and been removed from our map,
      // so we're finished now.
      FinishOnGraphThread();
    } else {
      mTracks.AdvanceKnownTracksTime(GraphTimeToStreamTimeWithBlocking(aTo));
    }
    if (allHaveCurrentData) {
      // We can make progress if we're not blocked
      mHasCurrentData = true;
    }
  }

  uint32_t TrackUnionStream::AddTrack(MediaInputPort* aPort, StreamTracks::Track* aTrack,
                    GraphTime aFrom)
  {
    STREAM_LOG(LogLevel::Verbose, ("TrackUnionStream %p adding track %d for "
                                   "input stream %p track %d, desired id %d",
                                   this, aTrack->GetID(), aPort->GetSource(),
                                   aTrack->GetID(),
                                   aPort->GetDestinationTrackId()));

    TrackID id;
    if (IsTrackIDExplicit(id = aPort->GetDestinationTrackId())) {
      MOZ_ASSERT(id >= mNextAvailableTrackID &&
                 mUsedTracks.BinaryIndexOf(id) == mUsedTracks.NoIndex,
                 "Desired destination id taken. Only provide a destination ID "
                 "if you can assure its availability, or we may not be able "
                 "to bind to the correct DOM-side track.");
#ifdef DEBUG
      AutoTArray<MediaInputPort*, 32> inputs(mInputs);
      inputs.AppendElements(mSuspendedInputs);
      for (size_t i = 0; inputs[i] != aPort; ++i) {
        MOZ_ASSERT(inputs[i]->GetSourceTrackId() != TRACK_ANY,
                   "You are adding a MediaInputPort with a track mapping "
                   "while there already exist generic MediaInputPorts for this "
                   "destination stream. This can lead to TrackID collisions!");
      }
#endif
      mUsedTracks.InsertElementSorted(id);
    } else if ((id = aTrack->GetID()) &&
               id > mNextAvailableTrackID &&
               mUsedTracks.BinaryIndexOf(id) == mUsedTracks.NoIndex) {
      // Input id available. Mark it used in mUsedTracks.
      mUsedTracks.InsertElementSorted(id);
    } else {
      // No desired destination id and Input id taken, allocate a new one.
      id = mNextAvailableTrackID;

      // Update mNextAvailableTrackID and prune any mUsedTracks members it now
      // covers.
      while (1) {
        if (!mUsedTracks.RemoveElementSorted(++mNextAvailableTrackID)) {
          // Not in use. We're done.
          break;
        }
      }
    }

    // Round up the track start time so the track, if anything, starts a
    // little later than the true time. This means we'll have enough
    // samples in our input stream to go just beyond the destination time.
    StreamTime outputStart = GraphTimeToStreamTimeWithBlocking(aFrom);

    nsAutoPtr<MediaSegment> segment;
    segment = aTrack->GetSegment()->CreateEmptyClone();
    for (uint32_t j = 0; j < mListeners.Length(); ++j) {
      MediaStreamListener* l = mListeners[j];
      l->NotifyQueuedTrackChanges(Graph(), id, outputStart,
                                  TrackEventCommand::TRACK_EVENT_CREATED,
                                  *segment,
                                  aPort->GetSource(), aTrack->GetID());
    }
    segment->AppendNullData(outputStart);
    StreamTracks::Track* track =
      &mTracks.AddTrack(id, outputStart, segment.forget());
    STREAM_LOG(LogLevel::Debug, ("TrackUnionStream %p added track %d for input stream %p track %d, start ticks %lld",
                                 this, track->GetID(), aPort->GetSource(), aTrack->GetID(),
                                 (long long)outputStart));

    TrackMapEntry* map = mTrackMap.AppendElement();
    map->mEndOfConsumedInputTicks = 0;
    map->mEndOfLastInputIntervalInInputStream = -1;
    map->mEndOfLastInputIntervalInOutputStream = -1;
    map->mInputPort = aPort;
    map->mInputTrackID = aTrack->GetID();
    map->mOutputTrackID = track->GetID();
    map->mSegment = aTrack->GetSegment()->CreateEmptyClone();

    for (int32_t i = mPendingDirectTrackListeners.Length() - 1; i >= 0; --i) {
      TrackBound<DirectMediaStreamTrackListener>& bound =
        mPendingDirectTrackListeners[i];
      if (bound.mTrackID != map->mOutputTrackID) {
        continue;
      }
      MediaStream* source = map->mInputPort->GetSource();
      map->mOwnedDirectListeners.AppendElement(bound.mListener);
      DisabledTrackMode currentMode = GetDisabledTrackMode(bound.mTrackID);
      if (currentMode != DisabledTrackMode::ENABLED) {
        bound.mListener->IncreaseDisabled(currentMode);
      }
      STREAM_LOG(LogLevel::Debug, ("TrackUnionStream %p adding direct listener "
                                   "%p for track %d. Forwarding to input "
                                   "stream %p track %d.",
                                   this, bound.mListener.get(), bound.mTrackID,
                                   source, map->mInputTrackID));
      source->AddDirectTrackListenerImpl(bound.mListener.forget(),
                                         map->mInputTrackID);
      mPendingDirectTrackListeners.RemoveElementAt(i);
    }

    return mTrackMap.Length() - 1;
  }

  void TrackUnionStream::EndTrack(uint32_t aIndex)
  {
    StreamTracks::Track* outputTrack = mTracks.FindTrack(mTrackMap[aIndex].mOutputTrackID);
    if (!outputTrack || outputTrack->IsEnded())
      return;
    STREAM_LOG(LogLevel::Debug, ("TrackUnionStream %p ending track %d", this, outputTrack->GetID()));
    for (uint32_t j = 0; j < mListeners.Length(); ++j) {
      MediaStreamListener* l = mListeners[j];
      StreamTime offset = outputTrack->GetSegment()->GetDuration();
      nsAutoPtr<MediaSegment> segment;
      segment = outputTrack->GetSegment()->CreateEmptyClone();
      l->NotifyQueuedTrackChanges(Graph(), outputTrack->GetID(), offset,
                                  TrackEventCommand::TRACK_EVENT_ENDED,
                                  *segment,
                                  mTrackMap[aIndex].mInputPort->GetSource(),
                                  mTrackMap[aIndex].mInputTrackID);
    }
    for (TrackBound<MediaStreamTrackListener>& b : mTrackListeners) {
      if (b.mTrackID == outputTrack->GetID()) {
        b.mListener->NotifyEnded();
      }
    }
    outputTrack->SetEnded();
  }

  void TrackUnionStream::CopyTrackData(StreamTracks::Track* aInputTrack,
                     uint32_t aMapIndex, GraphTime aFrom, GraphTime aTo,
                     bool* aOutputTrackFinished)
  {
    TrackMapEntry* map = &mTrackMap[aMapIndex];
    StreamTracks::Track* outputTrack = mTracks.FindTrack(map->mOutputTrackID);
    MOZ_ASSERT(outputTrack && !outputTrack->IsEnded(), "Can't copy to ended track");

    MediaSegment* segment = map->mSegment;
    MediaStream* source = map->mInputPort->GetSource();

    GraphTime next;
    *aOutputTrackFinished = false;
    for (GraphTime t = aFrom; t < aTo; t = next) {
      MediaInputPort::InputInterval interval = map->mInputPort->GetNextInputInterval(t);
      interval.mEnd = std::min(interval.mEnd, aTo);
      StreamTime inputEnd = source->GraphTimeToStreamTimeWithBlocking(interval.mEnd);
      StreamTime inputTrackEndPoint = STREAM_TIME_MAX;

      if (aInputTrack->IsEnded() &&
          aInputTrack->GetEnd() <= inputEnd) {
        inputTrackEndPoint = aInputTrack->GetEnd();
        *aOutputTrackFinished = true;
      }

      if (interval.mStart >= interval.mEnd) {
        break;
      }
      StreamTime ticks = interval.mEnd - interval.mStart;
      next = interval.mEnd;

      StreamTime outputStart = outputTrack->GetEnd();

      if (interval.mInputIsBlocked) {
        // Maybe the input track ended?
        segment->AppendNullData(ticks);
        STREAM_LOG(LogLevel::Verbose, ("TrackUnionStream %p appending %lld ticks of null data to track %d",
                   this, (long long)ticks, outputTrack->GetID()));
      } else if (InMutedCycle()) {
        segment->AppendNullData(ticks);
      } else {
        if (source->IsSuspended()) {
          segment->AppendNullData(aTo - aFrom);
        } else {
          MOZ_ASSERT(outputTrack->GetEnd() == GraphTimeToStreamTimeWithBlocking(interval.mStart),
                     "Samples missing");
          StreamTime inputStart = source->GraphTimeToStreamTimeWithBlocking(interval.mStart);
          segment->AppendSlice(*aInputTrack->GetSegment(),
                               std::min(inputTrackEndPoint, inputStart),
                               std::min(inputTrackEndPoint, inputEnd));
        }
      }
      ApplyTrackDisabling(outputTrack->GetID(), segment);
      for (uint32_t j = 0; j < mListeners.Length(); ++j) {
        MediaStreamListener* l = mListeners[j];
        // Separate Audio and Video.
        if (segment->GetType() == MediaSegment::AUDIO) {
          l->NotifyQueuedAudioData(Graph(), outputTrack->GetID(),
                                   outputStart,
                                   *static_cast<AudioSegment*>(segment),
                                   map->mInputPort->GetSource(),
                                   map->mInputTrackID);
        }
      }
      for (TrackBound<MediaStreamTrackListener>& b : mTrackListeners) {
        if (b.mTrackID != outputTrack->GetID()) {
          continue;
        }
        b.mListener->NotifyQueuedChanges(Graph(), outputStart, *segment);
      }
      outputTrack->GetSegment()->AppendFrom(segment);
    }
  }

void
TrackUnionStream::SetTrackEnabledImpl(TrackID aTrackID, DisabledTrackMode aMode) {
  bool enabled = aMode == DisabledTrackMode::ENABLED;
  for (TrackMapEntry& entry : mTrackMap) {
    if (entry.mOutputTrackID == aTrackID) {
      STREAM_LOG(LogLevel::Info, ("TrackUnionStream %p track %d was explicitly %s",
                                   this, aTrackID, enabled ? "enabled" : "disabled"));
      for (DirectMediaStreamTrackListener* listener : entry.mOwnedDirectListeners) {
        DisabledTrackMode oldMode = GetDisabledTrackMode(aTrackID);
        bool oldEnabled = oldMode == DisabledTrackMode::ENABLED;
        if (!oldEnabled && enabled) {
          STREAM_LOG(LogLevel::Debug, ("TrackUnionStream %p track %d setting "
                                       "direct listener enabled",
                                       this, aTrackID));
          listener->DecreaseDisabled(oldMode);
        } else if (oldEnabled && !enabled) {
          STREAM_LOG(LogLevel::Debug, ("TrackUnionStream %p track %d setting "
                                       "direct listener disabled",
                                       this, aTrackID));
          listener->IncreaseDisabled(aMode);
        }
      }
    }
  }
  MediaStream::SetTrackEnabledImpl(aTrackID, aMode);
}

MediaStream*
TrackUnionStream::GetInputStreamFor(TrackID aTrackID)
{
  for (TrackMapEntry& entry : mTrackMap) {
    if (entry.mOutputTrackID == aTrackID && entry.mInputPort) {
      return entry.mInputPort->GetSource();
    }
  }

  return nullptr;
}

TrackID
TrackUnionStream::GetInputTrackIDFor(TrackID aTrackID)
{
  for (TrackMapEntry& entry : mTrackMap) {
    if (entry.mOutputTrackID == aTrackID) {
      return entry.mInputTrackID;
    }
  }

  return TRACK_NONE;
}

void
TrackUnionStream::AddDirectTrackListenerImpl(already_AddRefed<DirectMediaStreamTrackListener> aListener,
                                             TrackID aTrackID)
{
  RefPtr<DirectMediaStreamTrackListener> listener = aListener;

  for (TrackMapEntry& entry : mTrackMap) {
    if (entry.mOutputTrackID == aTrackID) {
      MediaStream* source = entry.mInputPort->GetSource();
      STREAM_LOG(LogLevel::Debug, ("TrackUnionStream %p adding direct listener "
                                   "%p for track %d. Forwarding to input "
                                   "stream %p track %d.",
                                   this, listener.get(), aTrackID, source,
                                   entry.mInputTrackID));
      entry.mOwnedDirectListeners.AppendElement(listener);
      DisabledTrackMode currentMode = GetDisabledTrackMode(aTrackID);
      if (currentMode != DisabledTrackMode::ENABLED) {
        listener->IncreaseDisabled(currentMode);
      }
      source->AddDirectTrackListenerImpl(listener.forget(),
                                         entry.mInputTrackID);
      return;
    }
  }

  TrackBound<DirectMediaStreamTrackListener>* bound =
    mPendingDirectTrackListeners.AppendElement();
  bound->mListener = listener.forget();
  bound->mTrackID = aTrackID;
}

void
TrackUnionStream::RemoveDirectTrackListenerImpl(DirectMediaStreamTrackListener* aListener,
                                                TrackID aTrackID)
{
  for (TrackMapEntry& entry : mTrackMap) {
    // OutputTrackID is unique to this stream so we only need to do this once.
    if (entry.mOutputTrackID != aTrackID) {
      continue;
    }
    for (size_t i = 0; i < entry.mOwnedDirectListeners.Length(); ++i) {
      if (entry.mOwnedDirectListeners[i] == aListener) {
        STREAM_LOG(LogLevel::Debug, ("TrackUnionStream %p removing direct "
                                     "listener %p for track %d, forwarding "
                                     "to input stream %p track %d",
                                     this, aListener, aTrackID,
                                     entry.mInputPort->GetSource(),
                                     entry.mInputTrackID));
        DisabledTrackMode currentMode = GetDisabledTrackMode(aTrackID);
        if (currentMode != DisabledTrackMode::ENABLED) {
          // Reset the listener's state.
          aListener->DecreaseDisabled(currentMode);
        }
        entry.mOwnedDirectListeners.RemoveElementAt(i);
        break;
      }
    }
    // Forward to the input
    MediaStream* source = entry.mInputPort->GetSource();
    source->RemoveDirectTrackListenerImpl(aListener, entry.mInputTrackID);
    return;
  }

  for (size_t i = 0; i < mPendingDirectTrackListeners.Length(); ++i) {
    TrackBound<DirectMediaStreamTrackListener>& bound =
      mPendingDirectTrackListeners[i];
    if (bound.mListener == aListener && bound.mTrackID == aTrackID) {
      mPendingDirectTrackListeners.RemoveElementAt(i);
      return;
    }
  }
}
} // namespace mozilla
