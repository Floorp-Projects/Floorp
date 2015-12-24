/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_VIDEOSEGMENT_H_
#define MOZILLA_VIDEOSEGMENT_H_

#include "MediaSegment.h"
#include "nsCOMPtr.h"
#include "gfxPoint.h"
#include "ImageContainer.h"

namespace mozilla {

namespace layers {
class Image;
} // namespace layers

class VideoFrame {
public:
  typedef mozilla::layers::Image Image;

  VideoFrame(already_AddRefed<Image>& aImage, const gfx::IntSize& aIntrinsicSize);
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
  void SetForceBlack(bool aForceBlack) { mForceBlack = aForceBlack; }
  bool GetForceBlack() const { return mForceBlack; }
  void SetPrincipalHandle(const PrincipalHandle& aPrincipalHandle) { mPrincipalHandle = aPrincipalHandle; }
  PrincipalHandle GetPrincipalHandle() const { return mPrincipalHandle; }
  const gfx::IntSize& GetIntrinsicSize() const { return mIntrinsicSize; }
  void SetNull();
  void TakeFrom(VideoFrame* aFrame);

  // Create a planar YCbCr black image.
  static already_AddRefed<Image> CreateBlackImage(const gfx::IntSize& aSize);

protected:
  // mImage can be null to indicate "no video" (aka "empty frame"). It can
  // still have an intrinsic size in this case.
  RefPtr<Image> mImage;
  // The desired size to render the video frame at.
  gfx::IntSize mIntrinsicSize;
  bool mForceBlack;
  // principalHandle for the image in this frame.
  // This can be compared to an nsIPrincipal when back on main thread.
  PrincipalHandle mPrincipalHandle;
};

struct VideoChunk {
  VideoChunk();
  ~VideoChunk();
  void SliceTo(StreamTime aStart, StreamTime aEnd)
  {
    NS_ASSERTION(aStart >= 0 && aStart < aEnd && aEnd <= mDuration,
                 "Slice out of bounds");
    mDuration = aEnd - aStart;
  }
  StreamTime GetDuration() const { return mDuration; }
  bool CanCombineWithFollowing(const VideoChunk& aOther) const
  {
    return aOther.mFrame == mFrame;
  }
  bool IsNull() const { return !mFrame.GetImage(); }
  void SetNull(StreamTime aDuration)
  {
    mDuration = aDuration;
    mFrame.SetNull();
    mTimeStamp = TimeStamp();
  }
  void SetForceBlack(bool aForceBlack) { mFrame.SetForceBlack(aForceBlack); }

  size_t SizeOfExcludingThisIfUnshared(MallocSizeOf aMallocSizeOf) const
  {
    // Future:
    // - mFrame
    return 0;
  }

  PrincipalHandle GetPrincipalHandle() const { return mFrame.GetPrincipalHandle(); }

  StreamTime mDuration;
  VideoFrame mFrame;
  mozilla::TimeStamp mTimeStamp;
};

class VideoSegment : public MediaSegmentBase<VideoSegment, VideoChunk> {
public:
  typedef mozilla::layers::Image Image;
  typedef mozilla::gfx::IntSize IntSize;

  VideoSegment();
  ~VideoSegment();

  void AppendFrame(already_AddRefed<Image>&& aImage,
                   StreamTime aDuration,
                   const IntSize& aIntrinsicSize,
                   const PrincipalHandle& aPrincipalHandle,
                   bool aForceBlack = false);
  const VideoFrame* GetLastFrame(StreamTime* aStart = nullptr)
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
  // Override default impl
  void ReplaceWithDisabled() override {
    for (ChunkIterator i(*this);
         !i.IsEnded(); i.Next()) {
      VideoChunk& chunk = *i;
      chunk.SetForceBlack(true);
    }
  }

  // Segment-generic methods not in MediaSegmentBase
  static Type StaticType() { return VIDEO; }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  bool IsEmpty() const
  {
    return mChunks.IsEmpty();
  }

};

} // namespace mozilla

#endif /* MOZILLA_VIDEOSEGMENT_H_ */
