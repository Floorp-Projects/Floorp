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
#include "prlog.h"
#include "mozilla/Attributes.h"
#include "TrackUnionStream.h"
#include "ImageContainer.h"
#include "AudioChannelService.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioNodeExternalInputStream.h"
#include <algorithm>
#include "DOMMediaStream.h"
#include "GeckoProfiler.h"
#include "mozilla/unused.h"
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

#ifdef PR_LOGGING
PRLogModuleInfo* gTrackUnionStreamLog;
#define STREAM_LOG(type, msg) PR_LOG(gTrackUnionStreamLog, type, msg)
#else
#define STREAM_LOG(type, msg)
#endif

TrackUnionStream::TrackUnionStream(DOMMediaStream* aWrapper) :
  ProcessedMediaStream(aWrapper),
  mFilterCallback(nullptr)
{
#ifdef PR_LOGGING
  if (!gTrackUnionStreamLog) {
    gTrackUnionStreamLog = PR_NewLogModule("TrackUnionStream");
  }
#endif
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
    bool allFinished = true;
    bool allHaveCurrentData = true;
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
        if (!found && (!mFilterCallback || mFilterCallback(tracks.get()))) {
          bool trackFinished = false;
          uint32_t mapIndex = AddTrack(mInputs[i], tracks.get(), aFrom);
          CopyTrackData(tracks.get(), mapIndex, aFrom, aTo, &trackFinished);
          mappedTracksFinished.AppendElement(trackFinished);
          mappedTracksWithMatchingInputTracks.AppendElement(true);
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

  // Consumers may specify a filtering callback to apply to every input track.
  // Returns true to allow the track to act as an input; false to reject it entirely.

  void TrackUnionStream::SetTrackIDFilter(TrackIDFilterCallback aCallback)
  {
    mFilterCallback = aCallback;
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

    TrackRate rate = aTrack->GetRate();
    // Round up the track start time so the track, if anything, starts a
    // little later than the true time. This means we'll have enough
    // samples in our input stream to go just beyond the destination time.
    TrackTicks outputStart = TimeToTicksRoundUp(rate, GraphTimeToStreamTime(aFrom));

    nsAutoPtr<MediaSegment> segment;
    segment = aTrack->GetSegment()->CreateEmptyClone();
    for (uint32_t j = 0; j < mListeners.Length(); ++j) {
      MediaStreamListener* l = mListeners[j];
      l->NotifyQueuedTrackChanges(Graph(), id, rate, outputStart,
                                  MediaStreamListener::TRACK_EVENT_CREATED,
                                  *segment);
    }
    segment->AppendNullData(outputStart);
    StreamBuffer::Track* track =
      &mBuffer.AddTrack(id, rate, outputStart, segment.forget());
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
      TrackTicks offset = outputTrack->GetSegment()->GetDuration();
      nsAutoPtr<MediaSegment> segment;
      segment = outputTrack->GetSegment()->CreateEmptyClone();
      l->NotifyQueuedTrackChanges(Graph(), outputTrack->GetID(),
                                  outputTrack->GetRate(), offset,
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

    TrackRate rate = outputTrack->GetRate();
    MediaSegment* segment = map->mSegment;
    MediaStream* source = map->mInputPort->GetSource();

    GraphTime next;
    *aOutputTrackFinished = false;
    for (GraphTime t = aFrom; t < aTo; t = next) {
      MediaInputPort::InputInterval interval = map->mInputPort->GetNextInputInterval(t);
      interval.mEnd = std::min(interval.mEnd, aTo);
      StreamTime inputEnd = source->GraphTimeToStreamTime(interval.mEnd);
      TrackTicks inputTrackEndPoint = aInputTrack->GetEnd();

      if (aInputTrack->IsEnded() &&
          aInputTrack->GetEndTimeRoundDown() <= inputEnd) {
        *aOutputTrackFinished = true;
      }

      if (interval.mStart >= interval.mEnd) {
        break;
      }
      next = interval.mEnd;

      // Ticks >= startTicks and < endTicks are in the interval
      StreamTime outputEnd = GraphTimeToStreamTime(interval.mEnd);
      TrackTicks startTicks = outputTrack->GetEnd();
      StreamTime outputStart = GraphTimeToStreamTime(interval.mStart);
      MOZ_ASSERT(startTicks == TimeToTicksRoundUp(rate, outputStart), "Samples missing");
      TrackTicks endTicks = TimeToTicksRoundUp(rate, outputEnd);
      TrackTicks ticks = endTicks - startTicks;
      StreamTime inputStart = source->GraphTimeToStreamTime(interval.mStart);

      if (interval.mInputIsBlocked) {
        // Maybe the input track ended?
        segment->AppendNullData(ticks);
        STREAM_LOG(PR_LOG_DEBUG+1, ("TrackUnionStream %p appending %lld ticks of null data to track %d",
                   this, (long long)ticks, outputTrack->GetID()));
      } else {
        // Figuring out which samples to use from the input stream is tricky
        // because its start time and our start time may differ by a fraction
        // of a tick. Assuming the input track hasn't ended, we have to ensure
        // that 'ticks' samples are gathered, even though a tick boundary may
        // occur between outputStart and outputEnd but not between inputStart
        // and inputEnd.
        // These are the properties we need to ensure:
        // 1) Exactly 'ticks' ticks of output are produced, i.e.
        // inputEndTicks - inputStartTicks = ticks.
        // 2) inputEndTicks <= aInputTrack->GetSegment()->GetDuration().
        // 3) In any sequence of intervals where neither stream is blocked,
        // the content of the input track we use is a contiguous sequence of
        // ticks with no gaps or overlaps.
        if (map->mEndOfLastInputIntervalInInputStream != inputStart ||
            map->mEndOfLastInputIntervalInOutputStream != outputStart) {
          // Start of a new series of intervals where neither stream is blocked.
          map->mEndOfConsumedInputTicks = TimeToTicksRoundDown(rate, inputStart) - 1;
        }
        TrackTicks inputStartTicks = map->mEndOfConsumedInputTicks;
        TrackTicks inputEndTicks = inputStartTicks + ticks;
        map->mEndOfConsumedInputTicks = inputEndTicks;
        map->mEndOfLastInputIntervalInInputStream = inputEnd;
        map->mEndOfLastInputIntervalInOutputStream = outputEnd;

        if (GraphImpl()->mFlushSourcesNow) {
          TrackTicks flushto = inputEndTicks;
          STREAM_LOG(PR_LOG_DEBUG, ("TrackUnionStream %p flushing after %lld of %lld ticks of input data from track %d for track %d",
              this, flushto, aInputTrack->GetSegment()->GetDuration(), aInputTrack->GetID(), outputTrack->GetID()));
          aInputTrack->FlushAfter(flushto);
          MOZ_ASSERT(inputTrackEndPoint >= aInputTrack->GetEnd());
        }
        // Now we prove that the above properties hold:
        // Property #1: trivial by construction.
        // Property #3: trivial by construction. Between every two
        // intervals where both streams are not blocked, the above if condition
        // is false and mEndOfConsumedInputTicks advances exactly to match
        // the ticks that were consumed.
        // Property #2:
        // Let originalOutputStart be the value of outputStart and originalInputStart
        // be the value of inputStart when the body of the "if" block was last
        // executed.
        // Let allTicks be the sum of the values of 'ticks' computed since then.
        // The interval [originalInputStart/rate, inputEnd/rate) is the
        // same length as the interval [originalOutputStart/rate, outputEnd/rate),
        // so the latter interval can have at most one more integer in it. Thus
        // TimeToTicksRoundUp(rate, outputEnd) - TimeToTicksRoundUp(rate, originalOutputStart)
        //   <= TimeToTicksRoundDown(rate, inputEnd) - TimeToTicksRoundDown(rate, originalInputStart) + 1
        // Then
        // inputEndTicks = TimeToTicksRoundDown(rate, originalInputStart) - 1 + allTicks
        //   = TimeToTicksRoundDown(rate, originalInputStart) - 1 + TimeToTicksRoundUp(rate, outputEnd) - TimeToTicksRoundUp(rate, originalOutputStart)
        //   <= TimeToTicksRoundDown(rate, originalInputStart) - 1 + TimeToTicksRoundDown(rate, inputEnd) - TimeToTicksRoundDown(rate, originalInputStart) + 1
        //   = TimeToTicksRoundDown(rate, inputEnd)
        //   <= inputEnd/rate
        // (now using the fact that inputEnd <= track->GetEndTimeRoundDown() for a non-ended track)
        //   <= TicksToTimeRoundDown(rate, aInputTrack->GetSegment()->GetDuration())/rate
        //   <= rate*aInputTrack->GetSegment()->GetDuration()/rate
        //   = aInputTrack->GetSegment()->GetDuration()
        // as required.
        // Note that while the above proof appears to be generally right, if we are suffering
        // from a lot of underrun, then in rare cases inputStartTicks >> inputTrackEndPoint.
        // As such, we still need to verify the sanity of property #2 and use null data as
        // appropriate.

        if (inputStartTicks < 0) {
          // Data before the start of the track is just null.
          // We have to add a small amount of delay to ensure that there is
          // always a sample available if we see an interval that contains a
          // tick boundary on the output stream's timeline but does not contain
          // a tick boundary on the input stream's timeline. 1 tick delay is
          // necessary and sufficient.
          segment->AppendNullData(-inputStartTicks);
          inputStartTicks = 0;
        }
        if (inputEndTicks > inputStartTicks) {
          if (inputEndTicks <= inputTrackEndPoint) {
            segment->AppendSlice(*aInputTrack->GetSegment(), inputStartTicks, inputEndTicks);
            STREAM_LOG(PR_LOG_DEBUG+1, ("TrackUnionStream %p appending %lld ticks of input data to track %d",
                       this, ticks, outputTrack->GetID()));
          } else {
            if (inputStartTicks < inputTrackEndPoint) {
              segment->AppendSlice(*aInputTrack->GetSegment(), inputStartTicks, inputTrackEndPoint);
              ticks -= inputTrackEndPoint - inputStartTicks;
            }
            segment->AppendNullData(ticks);
            STREAM_LOG(PR_LOG_DEBUG+1, ("TrackUnionStream %p appending %lld ticks of input data and %lld of null data to track %d",
                       this, inputTrackEndPoint - inputStartTicks, ticks, outputTrack->GetID()));
          }
        }
      }
      ApplyTrackDisabling(outputTrack->GetID(), segment);
      for (uint32_t j = 0; j < mListeners.Length(); ++j) {
        MediaStreamListener* l = mListeners[j];
        l->NotifyQueuedTrackChanges(Graph(), outputTrack->GetID(),
                                    outputTrack->GetRate(), startTicks, 0,
                                    *segment);
      }
      outputTrack->GetSegment()->AppendFrom(segment);
    }
  }
}
