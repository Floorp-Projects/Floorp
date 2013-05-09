/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_TRACKUNIONSTREAM_H_
#define MOZILLA_TRACKUNIONSTREAM_H_

#include "MediaStreamGraph.h"
#include <algorithm>

namespace mozilla {

#ifdef PR_LOGGING
#define LOG(type, msg) PR_LOG(gMediaStreamGraphLog, type, msg)
#else
#define LOG(type, msg)
#endif

/**
 * See MediaStreamGraph::CreateTrackUnionStream.
 * This file is only included by MediaStreamGraph.cpp so it's OK to put the
 * entire implementation in this header file.
 */
class TrackUnionStream : public ProcessedMediaStream {
public:
  TrackUnionStream(DOMMediaStream* aWrapper) :
    ProcessedMediaStream(aWrapper),
    mMaxTrackID(0) {}

  virtual void RemoveInput(MediaInputPort* aPort)
  {
    for (int32_t i = mTrackMap.Length() - 1; i >= 0; --i) {
      if (mTrackMap[i].mInputPort == aPort) {
        EndTrack(i);
        mTrackMap.RemoveElementAt(i);
      }
    }
    ProcessedMediaStream::RemoveInput(aPort);
  }
  virtual void ProduceOutput(GraphTime aFrom, GraphTime aTo)
  {
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
        if (!found) {
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
    if (allFinished && mAutofinish) {
      // All streams have finished and won't add any more tracks, and
      // all our tracks have actually finished and been removed from our map,
      // so we're finished now.
      FinishOnGraphThread();
    }
    mBuffer.AdvanceKnownTracksTime(GraphTimeToStreamTime(aTo));
    if (allHaveCurrentData) {
      // We can make progress if we're not blocked
      mHasCurrentData = true;
    }
  }

protected:
  // Only non-ended tracks are allowed to persist in this map.
  struct TrackMapEntry {
    MediaInputPort* mInputPort;
    // We keep track IDs instead of track pointers because
    // tracks can be removed without us being notified (e.g.
    // when a finished track is forgotten.) When we need a Track*,
    // we call StreamBuffer::FindTrack, which will return null if
    // the track has been deleted.
    TrackID mInputTrackID;
    TrackID mOutputTrackID;
    nsAutoPtr<MediaSegment> mSegment;
  };

  uint32_t AddTrack(MediaInputPort* aPort, StreamBuffer::Track* aTrack,
                    GraphTime aFrom)
  {
    // Use the ID of the source track if we can, otherwise allocate a new
    // unique ID
    TrackID id = std::max(mMaxTrackID + 1, aTrack->GetID());
    mMaxTrackID = id;

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
    LOG(PR_LOG_DEBUG, ("TrackUnionStream %p adding track %d for input stream %p track %d, start ticks %lld",
                       this, id, aPort->GetSource(), aTrack->GetID(),
                       (long long)outputStart));

    TrackMapEntry* map = mTrackMap.AppendElement();
    map->mInputPort = aPort;
    map->mInputTrackID = aTrack->GetID();
    map->mOutputTrackID = track->GetID();
    map->mSegment = aTrack->GetSegment()->CreateEmptyClone();
    return mTrackMap.Length() - 1;
  }
  void EndTrack(uint32_t aIndex)
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
  void CopyTrackData(StreamBuffer::Track* aInputTrack,
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
      if (interval.mStart >= interval.mEnd)
        break;
      next = interval.mEnd;

      // Ticks >= startTicks and < endTicks are in the interval
      StreamTime outputEnd = GraphTimeToStreamTime(interval.mEnd);
      TrackTicks startTicks = outputTrack->GetEnd();
#ifdef DEBUG
      StreamTime outputStart = GraphTimeToStreamTime(interval.mStart);
#endif
      NS_ASSERTION(startTicks == TimeToTicksRoundUp(rate, outputStart),
                   "Samples missing");
      TrackTicks endTicks = TimeToTicksRoundUp(rate, outputEnd);
      TrackTicks ticks = endTicks - startTicks;
      // StreamTime inputStart = source->GraphTimeToStreamTime(interval.mStart);
      StreamTime inputEnd = source->GraphTimeToStreamTime(interval.mEnd);
      TrackTicks inputTrackEndPoint = TRACK_TICKS_MAX;

      if (aInputTrack->IsEnded()) {
        TrackTicks inputEndTicks = aInputTrack->TimeToTicksRoundDown(inputEnd);
        if (aInputTrack->GetEnd() <= inputEndTicks) {
          inputTrackEndPoint = aInputTrack->GetEnd();
          *aOutputTrackFinished = true;
        }
      }

      if (interval.mInputIsBlocked) {
        // Maybe the input track ended?
        segment->AppendNullData(ticks);
        LOG(PR_LOG_DEBUG+1, ("TrackUnionStream %p appending %lld ticks of null data to track %d",
            this, (long long)ticks, outputTrack->GetID()));
      } else {
        // Figuring out which samples to use from the input stream is tricky
        // because its start time and our start time may differ by a fraction
        // of a tick. Assuming the input track hasn't ended, we have to ensure
        // that 'ticks' samples are gathered, even though a tick boundary may
        // occur between outputStart and outputEnd but not between inputStart
        // and inputEnd.
        // We'll take the latest samples we can.
        TrackTicks inputEndTicks = TimeToTicksRoundUp(rate, inputEnd);
        TrackTicks inputStartTicks = inputEndTicks - ticks;
        segment->AppendSlice(*aInputTrack->GetSegment(),
                             std::min(inputTrackEndPoint, inputStartTicks),
                             std::min(inputTrackEndPoint, inputEndTicks));
        LOG(PR_LOG_DEBUG+1, ("TrackUnionStream %p appending %lld ticks of input data to track %d",
            this, (long long)(std::min(inputTrackEndPoint, inputEndTicks) - std::min(inputTrackEndPoint, inputStartTicks)),
            outputTrack->GetID()));
      }
      for (uint32_t j = 0; j < mListeners.Length(); ++j) {
        MediaStreamListener* l = mListeners[j];
        l->NotifyQueuedTrackChanges(Graph(), outputTrack->GetID(),
                                    outputTrack->GetRate(), startTicks, 0,
                                    *segment);
      }
      outputTrack->GetSegment()->AppendFrom(segment);
    }
  }

  nsTArray<TrackMapEntry> mTrackMap;
  TrackID mMaxTrackID;
};

}

#endif /* MOZILLA_MEDIASTREAMGRAPH_H_ */
