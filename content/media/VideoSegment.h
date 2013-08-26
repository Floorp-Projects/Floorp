/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_VIDEOSEGMENT_H_
#define MOZILLA_VIDEOSEGMENT_H_

#include "MediaSegment.h"
#include "nsCOMPtr.h"
#include "gfxPoint.h"
#include "nsAutoPtr.h"
#include "ImageContainer.h"

namespace mozilla {

namespace layers {
class Image;
}

class VideoFrame {
public:
  typedef mozilla::layers::Image Image;

  VideoFrame(already_AddRefed<Image> aImage, const gfxIntSize& aIntrinsicSize);
  VideoFrame();
  ~VideoFrame();

  bool operator==(const VideoFrame& aFrame) const
  {
    return mIntrinsicSize == aFrame.mIntrinsicSize &&
           mForceBlack == aFrame.mForceBlack &&
           ((mForceBlack && aFrame.mForceBlack) || mImage == aFrame.mImage);
  }
  bool operator!=(const VideoFrame& aFrame) const
  {
    return !operator==(aFrame);
  }

  Image* GetImage() const { return mImage; }
  void SetForceBlack(bool aForceBlack) { mForceBlack = true; }
  bool GetForceBlack() const { return mForceBlack; }
  const gfxIntSize& GetIntrinsicSize() const { return mIntrinsicSize; }
  void SetNull();
  void TakeFrom(VideoFrame* aFrame);

protected:
  // mImage can be null to indicate "no video" (aka "empty frame"). It can
  // still have an intrinsic size in this case.
  nsRefPtr<Image> mImage;
  // The desired size to render the video frame at.
  gfxIntSize mIntrinsicSize;
  bool mForceBlack;
};

struct VideoChunk {
  VideoChunk();
  ~VideoChunk();
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
  void SetForceBlack(bool aForceBlack) { mFrame.SetForceBlack(aForceBlack); }

  TrackTicks mDuration;
  VideoFrame mFrame;
};

class VideoSegment : public MediaSegmentBase<VideoSegment, VideoChunk> {
public:
  typedef mozilla::layers::Image Image;

  VideoSegment();
  ~VideoSegment();

  void AppendFrame(already_AddRefed<Image> aImage, TrackTicks aDuration,
                   const gfxIntSize& aIntrinsicSize);
  const VideoFrame* GetFrameAt(TrackTicks aOffset, TrackTicks* aStart = nullptr)
  {
    VideoChunk* c = FindChunkContaining(aOffset, aStart);
    if (!c) {
      return nullptr;
    }
    return &c->mFrame;
  }
  const VideoFrame* GetLastFrame(TrackTicks* aStart = nullptr)
  {
    VideoChunk* c = GetLastChunk();
    if (!c) {
      return nullptr;
    }
    if (aStart) {
      *aStart = mDuration - c->mDuration;
    }
    return &c->mFrame;
  }

  // Segment-generic methods not in MediaSegmentBase
  static Type StaticType() { return VIDEO; }
};

}

#endif /* MOZILLA_VIDEOSEGMENT_H_ */
