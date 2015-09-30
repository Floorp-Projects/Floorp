/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_TRACKUNIONSTREAM_H_
#define MOZILLA_TRACKUNIONSTREAM_H_

#include "MediaStreamGraph.h"
#include <algorithm>

namespace mozilla {

/**
 * See MediaStreamGraph::CreateTrackUnionStream.
 */
class TrackUnionStream : public ProcessedMediaStream {
public:
  explicit TrackUnionStream(DOMMediaStream* aWrapper);

  virtual void RemoveInput(MediaInputPort* aPort) override;
  virtual void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;

protected:
  // Only non-ended tracks are allowed to persist in this map.
  struct TrackMapEntry {
    // mEndOfConsumedInputTicks is the end of the input ticks that we've consumed.
    // 0 if we haven't consumed any yet.
    StreamTime mEndOfConsumedInputTicks;
    // mEndOfLastInputIntervalInInputStream is the timestamp for the end of the
    // previous interval which was unblocked for both the input and output
    // stream, in the input stream's timeline, or -1 if there wasn't one.
    StreamTime mEndOfLastInputIntervalInInputStream;
    // mEndOfLastInputIntervalInOutputStream is the timestamp for the end of the
    // previous interval which was unblocked for both the input and output
    // stream, in the output stream's timeline, or -1 if there wasn't one.
    StreamTime mEndOfLastInputIntervalInOutputStream;
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

  // Add the track to this stream, retaining its TrackID if it has never
  // been previously used in this stream, allocating a new TrackID otherwise.
  uint32_t AddTrack(MediaInputPort* aPort, StreamBuffer::Track* aTrack,
                    GraphTime aFrom);
  void EndTrack(uint32_t aIndex);
  void CopyTrackData(StreamBuffer::Track* aInputTrack,
                     uint32_t aMapIndex, GraphTime aFrom, GraphTime aTo,
                     bool* aOutputTrackFinished);

  nsTArray<TrackMapEntry> mTrackMap;

  // The next available TrackID, starting at 1 and progressing upwards.
  // All TrackIDs in [1, mNextAvailableTrackID) have implicitly been used.
  TrackID mNextAvailableTrackID;

  // Sorted array of used TrackIDs that require manual tracking.
  nsTArray<TrackID> mUsedTracks;
};

} // namespace mozilla

#endif /* MOZILLA_MEDIASTREAMGRAPH_H_ */
