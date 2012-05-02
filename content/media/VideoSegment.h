/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_VIDEOSEGMENT_H_
#define MOZILLA_VIDEOSEGMENT_H_

#include "MediaSegment.h"
#include "ImageLayers.h"

namespace mozilla {

class VideoFrame {
public:
  typedef mozilla::layers::Image Image;

  VideoFrame(already_AddRefed<Image> aImage, const gfxIntSize& aIntrinsicSize)
    : mImage(aImage), mIntrinsicSize(aIntrinsicSize) {}
  VideoFrame() : mIntrinsicSize(0, 0) {}

  bool operator==(const VideoFrame& aFrame) const
  {
    return mImage == aFrame.mImage && mIntrinsicSize == aFrame.mIntrinsicSize;
  }
  bool operator!=(const VideoFrame& aFrame) const
  {
    return !operator==(aFrame);
  }

  Image* GetImage() const { return mImage; }
  const gfxIntSize& GetIntrinsicSize() const { return mIntrinsicSize; }
  void SetNull() { mImage = nsnull; mIntrinsicSize = gfxIntSize(0, 0); }
  void TakeFrom(VideoFrame* aFrame)
  {
    mImage = aFrame->mImage.forget();
    mIntrinsicSize = aFrame->mIntrinsicSize;
  }

protected:
  // mImage can be null to indicate "no video" (aka "empty frame"). It can
  // still have an intrinsic size in this case.
  nsRefPtr<Image> mImage;
  // The desired size to render the video frame at.
  gfxIntSize mIntrinsicSize;
};


struct VideoChunk {
  void SliceTo(TrackTicks aStart, TrackTicks aEnd)
  {
    NS_ASSERTION(aStart >= 0 && aStart < aEnd && aEnd <= mDuration,
                 "Slice out of bounds");
    mDuration = aEnd - aStart;
  }
  TrackTicks GetDuration() const { return mDuration; }
  bool CanCombineWithFollowing(const VideoChunk& aOther) const
  {
    return aOther.mFrame == mFrame;
  }
  bool IsNull() const { return !mFrame.GetImage(); }
  void SetNull(TrackTicks aDuration)
  {
    mDuration = aDuration;
    mFrame.SetNull();
  }

  TrackTicks mDuration;
  VideoFrame mFrame;
};

class VideoSegment : public MediaSegmentBase<VideoSegment, VideoChunk> {
public:
  typedef mozilla::layers::Image Image;

  VideoSegment() : MediaSegmentBase<VideoSegment, VideoChunk>(VIDEO) {}

  void AppendFrame(already_AddRefed<Image> aImage, TrackTicks aDuration,
                   const gfxIntSize& aIntrinsicSize)
  {
    VideoChunk* chunk = AppendChunk(aDuration);
    VideoFrame frame(aImage, aIntrinsicSize);
    chunk->mFrame.TakeFrom(&frame);
  }
  const VideoFrame* GetFrameAt(TrackTicks aOffset, TrackTicks* aStart = nsnull)
  {
    VideoChunk* c = FindChunkContaining(aOffset, aStart);
    if (!c) {
      return nsnull;
    }
    return &c->mFrame;
  }
  const VideoFrame* GetLastFrame(TrackTicks* aStart = nsnull)
  {
    VideoChunk* c = GetLastChunk();
    if (!c) {
      return nsnull;
    }
    if (aStart) {
      *aStart = mDuration - c->mDuration;
    }
    return &c->mFrame;
  }

  // Segment-generic methods not in MediaSegmentBase
  void InitFrom(const VideoSegment& aOther)
  {
  }
  void SliceFrom(const VideoSegment& aOther, TrackTicks aStart, TrackTicks aEnd) {
    BaseSliceFrom(aOther, aStart, aEnd);
  }
  static Type StaticType() { return VIDEO; }
};

}

#endif /* MOZILLA_VIDEOSEGMENT_H_ */
