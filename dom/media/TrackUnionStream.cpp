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

PRLogModuleInfo* gTrackUnionStreamLog;
#define STREAM_LOG(type, msg) PR_LOG(gTrackUnionStreamLog, type, msg)

TrackUnionStream::TrackUnionStream(DOMMediaStream* aWrapper) :
  ProcessedMediaStream(aWrapper)
{
  if (!gTrackUnionStreamLog) {
    gTrackUnionStreamLog = PR_NewLogModule("TrackUnionStream");
  }
}

  void TrackUnionStream::RemoveInput(MediaInputPort* aPort)
  {
    for (int32_t i = mTrackMap.Length() - 1; i >= 0; --i) {
      if (mTrackMap[i].mInputPort == aPort) {
        EndTrack(i);
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
    nsAutoTArray<bool,8> mappedTracksFinished;
    nsAutoTArray<bool,8> mappedTracksWithMatchingInputTracks;
    for (uint32_t i = 0; i < mTrackMap.Length(); ++i) {
      mappedTracksFinished.AppendElement(true);
      mappedTracksWithMatchingInputTracks.AppendElement(false);
    }
    bool allFinished = !mInputs.IsEmpty();
    bool allHaveCurrentData = !mInputs.IsEmpty();
    for (uint32_t i = 0; i < mInputs.Length(); ++i) {
      MediaStream* stream = mInputs[i]->GetSource();
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
      for (StreamBuffer::TrackIter tracks(stream->GetStreamBuffer());
           !tracks.IsEnded(); tracks.Next()) {
        bool found = false;
        for (uint32_t j = 0; j < mTrackMap.Length(); ++j) {
          TrackMapEntry* map = &mTrackMap[j];
          if (map->mInputPort == mInputs[i] && map->mInputTrackID == tracks->GetID()) {
            bool trackFinished;
            StreamBuffer::Track* outputTrack = mBuffer.FindTrack(map->mOutputTrackID);
            if (!outputTrack || outputTrack->IsEnded()) {
              trackFinished = true;
            } else {
              CopyTrackData(tracks.get(), j, aFrom, aTo, &trackFinished);
            }
            mappedTracksFinished[j] = trackFinished;
            mappedTracksWithMatchingInputTracks[j] = true;
            found = true;
            break;
          }
        }
        if (!found) {
          bool trackFinished = false;
          trackAdded = true;
          uint32_t mapIndex = AddTrack(mInputs[i], tracks.get(), aFrom);
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
        mTrackMap.RemoveElementAt(i);
      }
    }
    if (allFinished && mAutofinish && (aFlags & ALLOW_FINISH)) {
      // All streams have finished and won't add any more tracks, and
      // all our tracks have actually finished and been removed from our map,
      // so we're finished now.
      FinishOnGraphThread();
    } else {
      mBuffer.AdvanceKnownTracksTime(GraphTimeToStreamTime(aTo));
    }
    if (allHaveCurrentData) {
      // We can make progress if we're not blocked
      mHasCurrentData = true;
    }
  }

  // Forward SetTrackEnabled(output_track_id, enabled) to the Source MediaStream,
  // translating the output track ID into the correct ID in the source.
  void TrackUnionStream::ForwardTrackEnabled(TrackID aOutputID, bool aEnabled)
  {
    for (int32_t i = mTrackMap.Length() - 1; i >= 0; --i) {
      if (mTrackMap[i].mOutputTrackID == aOutputID) {
        mTrackMap[i].mInputPort->GetSource()->
          SetTrackEnabled(mTrackMap[i].mInputTrackID, aEnabled);
      }
    }
  }

  uint32_t TrackUnionStream::AddTrack(MediaInputPort* aPort, StreamBuffer::Track* aTrack,
                    GraphTime aFrom)
  {
    // Use the ID of the source track if it's not already assigned to a track,
    // otherwise allocate a new unique ID.
    TrackID id = aTrack->GetID();
    TrackID maxTrackID = 0;
    for (uint32_t i = 0; i < mTrackMap.Length(); ++i) {
      TrackID outID = mTrackMap[i].mOutputTrackID;
      maxTrackID = std::max(maxTrackID, outID);
    }
    // Note: we might have removed it here, but it might still be in the
    // StreamBuffer if the TrackUnionStream sees its input stream flip from
    // A to B, where both A and B have a track with the same ID
    while (1) {
      // search until we find one not in use here, and not in mBuffer
      if (!mBuffer.FindTrack(id)) {
        break;
      }
      id = ++maxTrackID;
    }

    // Round up the track start time so the track, if anything, starts a
    // little later than the true time. This means we'll have enough
    // samples in our input stream to go just beyond the destination time.
    StreamTime outputStart = GraphTimeToStreamTime(aFrom);

    nsAutoPtr<MediaSegment> segment;
    segment = aTrack->GetSegment()->CreateEmptyClone();
    for (uint32_t j = 0; j < mListeners.Length(); ++j) {
      MediaStreamListener* l = mListeners[j];
      l->NotifyQueuedTrackChanges(Graph(), id, outputStart,
                                  MediaStreamListener::TRACK_EVENT_CREATED,
                                  *segment);
    }
    segment->AppendNullData(outputStart);
    StreamBuffer::Track* track =
      &mBuffer.AddTrack(id, outputStart, segment.forget());
    STREAM_LOG(PR_LOG_DEBUG, ("TrackUnionStream %p adding track %d for input stream %p track %d, start ticks %lld",
                              this, id, aPort->GetSource(), aTrack->GetID(),
                              (long long)outputStart));

    TrackMapEntry* map = mTrackMap.AppendElement();
    map->mEndOfConsumedInputTicks = 0;
    map->mEndOfLastInputIntervalInInputStream = -1;
    map->mEndOfLastInputIntervalInOutputStream = -1;
    map->mInputPort = aPort;
    map->mInputTrackID = aTrack->GetID();
    map->mOutputTrackID = track->GetID();
    map->mSegment = aTrack->GetSegment()->CreateEmptyClone();
    return mTrackMap.Length() - 1;
  }

  void TrackUnionStream::EndTrack(uint32_t aIndex)
  {
    StreamBuffer::Track* outputTrack = mBuffer.FindTrack(mTrackMap[aIndex].mOutputTrackID);
    if (!outputTrack || outputTrack->IsEnded())
      return;
    for (uint32_t j = 0; j < mListeners.Length(); ++j) {
      MediaStreamListener* l = mListeners[j];
      StreamTime offset = outputTrack->GetSegment()->GetDuration();
      nsAutoPtr<MediaSegment> segment;
      segment = outputTrack->GetSegment()->CreateEmptyClone();
      l->NotifyQueuedTrackChanges(Graph(), outputTrack->GetID(), offset,
                                  MediaStreamListener::TRACK_EVENT_ENDED,
                                  *segment);
    }
    outputTrack->SetEnded();
  }

  void TrackUnionStream::CopyTrackData(StreamBuffer::Track* aInputTrack,
                     uint32_t aMapIndex, GraphTime aFrom, GraphTime aTo,
                     bool* aOutputTrackFinished)
  {
    TrackMapEntry* map = &mTrackMap[aMapIndex];
    StreamBuffer::Track* outputTrack = mBuffer.FindTrack(map->mOutputTrackID);
    MOZ_ASSERT(outputTrack && !outputTrack->IsEnded(), "Can't copy to ended track");

    MediaSegment* segment = map->mSegment;
    MediaStream* source = map->mInputPort->GetSource();

    GraphTime next;
    *aOutputTrackFinished = false;
    for (GraphTime t = aFrom; t < aTo; t = next) {
      MediaInputPort::InputInterval interval = map->mInputPort->GetNextInputInterval(t);
      interval.mEnd = std::min(interval.mEnd, aTo);
      StreamTime inputEnd = source->GraphTimeToStreamTime(interval.mEnd);
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
        STREAM_LOG(PR_LOG_DEBUG+1, ("TrackUnionStream %p appending %lld ticks of null data to track %d",
                   this, (long long)ticks, outputTrack->GetID()));
      } else if (InMutedCycle()) {
        segment->AppendNullData(ticks);
      } else {
        if (GraphImpl()->StreamSuspended(source)) {
          segment->AppendNullData(aTo - aFrom);
        } else {
          MOZ_ASSERT(outputTrack->GetEnd() == GraphTimeToStreamTime(interval.mStart),
                     "Samples missing");
          StreamTime inputStart = source->GraphTimeToStreamTime(interval.mStart);
          segment->AppendSlice(*aInputTrack->GetSegment(),
                               std::min(inputTrackEndPoint, inputStart),
                               std::min(inputTrackEndPoint, inputEnd));
        }
      }
      ApplyTrackDisabling(outputTrack->GetID(), segment);
      for (uint32_t j = 0; j < mListeners.Length(); ++j) {
        MediaStreamListener* l = mListeners[j];
        l->NotifyQueuedTrackChanges(Graph(), outputTrack->GetID(),
                                    outputStart, 0, *segment);
      }
      outputTrack->GetSegment()->AppendFrom(segment);
    }
  }
}
