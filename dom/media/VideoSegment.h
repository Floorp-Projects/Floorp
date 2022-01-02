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
}  // namespace layers

class VideoFrame {
 public:
  typedef mozilla::layers::Image Image;

  VideoFrame(already_AddRefed<Image> aImage,
             const gfx::IntSize& aIntrinsicSize);
  VideoFrame();
  ~VideoFrame();

  bool operator==(const VideoFrame& aFrame) const {
    return mIntrinsicSize == aFrame.mIntrinsicSize &&
           mForceBlack == aFrame.mForceBlack &&
           ((mForceBlack && aFrame.mForceBlack) || mImage == aFrame.mImage) &&
           mPrincipalHandle == aFrame.mPrincipalHandle;
  }
  bool operator!=(const VideoFrame& aFrame) const {
    return !operator==(aFrame);
  }

  Image* GetImage() const { return mImage; }
  void SetForceBlack(bool aForceBlack) { mForceBlack = aForceBlack; }
  bool GetForceBlack() const { return mForceBlack; }
  void SetPrincipalHandle(PrincipalHandle aPrincipalHandle) {
    mPrincipalHandle = std::forward<PrincipalHandle>(aPrincipalHandle);
  }
  const PrincipalHandle& GetPrincipalHandle() const { return mPrincipalHandle; }
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
  void SliceTo(TrackTime aStart, TrackTime aEnd) {
    NS_ASSERTION(aStart >= 0 && aStart < aEnd && aEnd <= mDuration,
                 "Slice out of bounds");
    mDuration = aEnd - aStart;
  }
  TrackTime GetDuration() const { return mDuration; }
  bool CanCombineWithFollowing(const VideoChunk& aOther) const {
    return aOther.mFrame == mFrame;
  }
  bool IsNull() const { return !mFrame.GetImage(); }
  void SetNull(TrackTime aDuration) {
    mDuration = aDuration;
    mFrame.SetNull();
    mTimeStamp = TimeStamp();
  }
  void SetForceBlack(bool aForceBlack) { mFrame.SetForceBlack(aForceBlack); }

  size_t SizeOfExcludingThisIfUnshared(MallocSizeOf aMallocSizeOf) const {
    // Future:
    // - mFrame
    return 0;
  }

  const PrincipalHandle& GetPrincipalHandle() const {
    return mFrame.GetPrincipalHandle();
  }

  TrackTime mDuration;
  VideoFrame mFrame;
  TimeStamp mTimeStamp;
};

class VideoSegment : public MediaSegmentBase<VideoSegment, VideoChunk> {
 public:
  typedef mozilla::layers::Image Image;
  typedef mozilla::gfx::IntSize IntSize;

  VideoSegment();
  VideoSegment(VideoSegment&& aSegment);

  VideoSegment(const VideoSegment&) = delete;
  VideoSegment& operator=(const VideoSegment&) = delete;

  ~VideoSegment();

  void AppendFrame(already_AddRefed<Image>&& aImage,
                   const IntSize& aIntrinsicSize,
                   const PrincipalHandle& aPrincipalHandle,
                   bool aForceBlack = false,
                   TimeStamp aTimeStamp = TimeStamp::Now());
  void ExtendLastFrameBy(TrackTime aDuration) {
    if (aDuration <= 0) {
      return;
    }
    if (mChunks.IsEmpty()) {
      mChunks.AppendElement()->SetNull(aDuration);
    } else {
      mChunks[mChunks.Length() - 1].mDuration += aDuration;
    }
    mDuration += aDuration;
  }
  const VideoFrame* GetLastFrame(TrackTime* aStart = nullptr) {
    VideoChunk* c = GetLastChunk();
    if (!c) {
      return nullptr;
    }
    if (aStart) {
      *aStart = mDuration - c->mDuration;
    }
    return &c->mFrame;
  }
  VideoChunk* FindChunkContaining(const TimeStamp& aTime) {
    VideoChunk* previousChunk = nullptr;
    for (VideoChunk& c : mChunks) {
      if (c.mTimeStamp.IsNull()) {
        continue;
      }
      if (c.mTimeStamp > aTime) {
        return previousChunk;
      }
      previousChunk = &c;
    }
    return previousChunk;
  }
  void ForgetUpToTime(const TimeStamp& aTime) {
    VideoChunk* chunk = FindChunkContaining(aTime);
    if (!chunk) {
      return;
    }
    TrackTime duration = 0;
    size_t chunksToRemove = 0;
    for (const VideoChunk& c : mChunks) {
      if (c.mTimeStamp >= chunk->mTimeStamp) {
        break;
      }
      duration += c.GetDuration();
      ++chunksToRemove;
    }
    mChunks.RemoveElementsAt(0, chunksToRemove);
    mDuration -= duration;
    MOZ_ASSERT(mChunks.Capacity() >= DEFAULT_SEGMENT_CAPACITY,
               "Capacity must be retained after removing chunks");
  }
  // Override default impl
  void ReplaceWithDisabled() override {
    for (ChunkIterator i(*this); !i.IsEnded(); i.Next()) {
      VideoChunk& chunk = *i;
      chunk.SetForceBlack(true);
    }
  }

  // Segment-generic methods not in MediaSegmentBase
  static Type StaticType() { return VIDEO; }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }
};

}  // namespace mozilla

#endif /* MOZILLA_VIDEOSEGMENT_H_ */
