/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIASTREAMVIDEOSINK_H_
#define MEDIASTREAMVIDEOSINK_H_

#include "mozilla/Pair.h"

#include "gfxPoint.h"
#include "MediaStreamListener.h"

namespace mozilla {

class VideoFrameContainer;

/**
 * Base class of MediaStreamVideoSink family. This is the output of MediaStream.
 */
class MediaStreamVideoSink : public DirectMediaStreamTrackListener {
public:
  // Method of DirectMediaStreamTrackListener.
  void NotifyRealtimeTrackData(MediaStreamGraph* aGraph,
                               StreamTime aTrackOffset,
                               const MediaSegment& aMedia) override;

  // Call on any thread
  virtual void SetCurrentFrames(const VideoSegment& aSegment) = 0;
  virtual void ClearFrames() = 0;

  virtual VideoFrameContainer* AsVideoFrameContainer() { return nullptr; }
  virtual void Invalidate() {}

protected:
  virtual ~MediaStreamVideoSink() {};
};

} // namespace mozilla

#endif /* MEDIASTREAMVIDEOSINK_H_ */
