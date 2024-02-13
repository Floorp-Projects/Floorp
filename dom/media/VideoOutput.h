/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VideoOutput_h
#define VideoOutput_h

#include "MediaTrackListener.h"
#include "VideoFrameContainer.h"

namespace mozilla {

static bool SetImageToBlackPixel(layers::PlanarYCbCrImage* aImage) {
  uint8_t blackPixel[] = {0x10, 0x80, 0x80};

  layers::PlanarYCbCrData data;
  data.mYChannel = blackPixel;
  data.mCbChannel = blackPixel + 1;
  data.mCrChannel = blackPixel + 2;
  data.mYStride = data.mCbCrStride = 1;
  data.mPictureRect = gfx::IntRect(0, 0, 1, 1);
  data.mYUVColorSpace = gfx::YUVColorSpace::BT601;
  // This could be made FULL once bug 1568745 is complete. A black pixel being
  // 0x00, 0x80, 0x80
  data.mColorRange = gfx::ColorRange::LIMITED;

  return NS_SUCCEEDED(aImage->CopyData(data));
}

class VideoOutput : public DirectMediaTrackListener {
 protected:
  typedef layers::Image Image;
  typedef layers::ImageContainer ImageContainer;
  typedef layers::ImageContainer::FrameID FrameID;
  typedef layers::ImageContainer::ProducerID ProducerID;

  virtual ~VideoOutput() = default;

  void DropPastFrames() {
    TimeStamp now = TimeStamp::Now();
    size_t nrChunksInPast = 0;
    for (const auto& idChunkPair : mFrames) {
      const VideoChunk& chunk = idChunkPair.second;
      if (chunk.mTimeStamp > now) {
        break;
      }
      ++nrChunksInPast;
    }
    if (nrChunksInPast > 1) {
      // We need to keep one frame that starts in the past, because it only ends
      // when the next frame starts (which also needs to be in the past for it
      // to drop).
      mFrames.RemoveElementsAt(0, nrChunksInPast - 1);
    }
  }

  void SendFramesEnsureLocked() {
    mMutex.AssertCurrentThreadOwns();
    SendFrames();
  }

  void SendFrames() {
    DropPastFrames();

    if (mFrames.IsEmpty()) {
      return;
    }

    if (!mEnabled && mDisabledBlackImageSent) {
      return;
    }

    // Collect any new frames produced in this iteration.
    AutoTArray<ImageContainer::NonOwningImage, 16> images;
    PrincipalHandle lastPrincipalHandle = PRINCIPAL_HANDLE_NONE;

    for (const auto& idChunkPair : mFrames) {
      ImageContainer::FrameID frameId = idChunkPair.first;
      const VideoChunk& chunk = idChunkPair.second;
      const VideoFrame& frame = chunk.mFrame;
      Image* image = frame.GetImage();
      if (frame.GetForceBlack() || !mEnabled) {
        if (!mBlackImage) {
          RefPtr<Image> blackImage = mVideoFrameContainer->GetImageContainer()
                                         ->CreatePlanarYCbCrImage();
          if (blackImage) {
            // Sets the image to a single black pixel, which will be scaled to
            // fill the rendered size.
            if (SetImageToBlackPixel(blackImage->AsPlanarYCbCrImage())) {
              mBlackImage = blackImage;
            }
          }
        }
        if (mBlackImage) {
          image = mBlackImage;
        }
      }
      if (!image) {
        // We ignore null images.
        continue;
      }
      images.AppendElement(ImageContainer::NonOwningImage(
          image, chunk.mTimeStamp, frameId, mProducerID));

      lastPrincipalHandle = chunk.GetPrincipalHandle();

      if (!mEnabled && mBlackImage) {
        MOZ_ASSERT(images.Length() == 1);
        mDisabledBlackImageSent = true;
        break;
      }
    }

    if (images.IsEmpty()) {
      // This could happen if the only images in mFrames are null. We leave the
      // container at the current frame in this case.
      mVideoFrameContainer->ClearFutureFrames();
      return;
    }

    bool principalHandleChanged =
        lastPrincipalHandle != PRINCIPAL_HANDLE_NONE &&
        lastPrincipalHandle != mVideoFrameContainer->GetLastPrincipalHandle();

    if (principalHandleChanged) {
      mVideoFrameContainer->UpdatePrincipalHandleForFrameID(
          lastPrincipalHandle, images.LastElement().mFrameID);
    }

    mVideoFrameContainer->SetCurrentFrames(
        mFrames[0].second.mFrame.GetIntrinsicSize(), images);
    mMainThread->Dispatch(NewRunnableMethod("VideoFrameContainer::Invalidate",
                                            mVideoFrameContainer,
                                            &VideoFrameContainer::Invalidate));
  }

  FrameID NewFrameID() {
    mMutex.AssertCurrentThreadOwns();
    return ++mFrameID;
  }

 public:
  VideoOutput(VideoFrameContainer* aContainer, AbstractThread* aMainThread)
      : mMutex("VideoOutput::mMutex"),
        mVideoFrameContainer(aContainer),
        mMainThread(aMainThread) {}
  void NotifyRealtimeTrackData(MediaTrackGraph* aGraph, TrackTime aTrackOffset,
                               const MediaSegment& aMedia) override {
    MOZ_ASSERT(aMedia.GetType() == MediaSegment::VIDEO);
    const VideoSegment& video = static_cast<const VideoSegment&>(aMedia);
    MutexAutoLock lock(mMutex);
    for (VideoSegment::ConstChunkIterator i(video); !i.IsEnded(); i.Next()) {
      if (!mLastFrameTime.IsNull() && i->mTimeStamp < mLastFrameTime) {
        // Time can go backwards if the source is a captured MediaDecoder and
        // it seeks, as the previously buffered frames would stretch into the
        // future. If this happens, we clear the buffered frames and start over.
        mFrames.ClearAndRetainStorage();
      }
      mFrames.AppendElement(std::make_pair(NewFrameID(), *i));
      mLastFrameTime = i->mTimeStamp;
    }

    SendFramesEnsureLocked();
  }
  void NotifyRemoved(MediaTrackGraph* aGraph) override {
    // Doesn't need locking by mMutex, since the direct listener is removed from
    // the track before we get notified.
    if (mFrames.Length() <= 1) {
      // The compositor has already received the last frame.
      mFrames.ClearAndRetainStorage();
      mVideoFrameContainer->ClearFutureFrames();
      return;
    }

    // The compositor has multiple frames. ClearFutureFrames() would only retain
    // the first as that's normally the current one. We however stop doing
    // SetCurrentFrames() once we've received the last frame in a track, so
    // there might be old frames lingering. We'll find the current one and
    // re-send that.
    DropPastFrames();
    mFrames.RemoveLastElements(mFrames.Length() - 1);
    SendFrames();
    mFrames.ClearAndRetainStorage();
  }
  void NotifyEnded(MediaTrackGraph* aGraph) override {
    // Doesn't need locking by mMutex, since for the track to end, it must have
    // been ended by the source, meaning that the source won't append more data.
    if (mFrames.IsEmpty()) {
      return;
    }

    // Re-send only the last one to the compositor.
    mFrames.RemoveElementsAt(0, mFrames.Length() - 1);
    SendFrames();
    mFrames.ClearAndRetainStorage();
  }
  void NotifyEnabledStateChanged(MediaTrackGraph* aGraph,
                                 bool aEnabled) override {
    MutexAutoLock lock(mMutex);
    mEnabled = aEnabled;
    DropPastFrames();
    if (mEnabled) {
      mDisabledBlackImageSent = false;
    }
    if (!mEnabled || mFrames.Length() > 1) {
      // Re-send frames when disabling, as new frames may not arrive. When
      // enabling we keep them black until new frames arrive, or re-send if we
      // already have frames in the future. If we're disabling and there are no
      // frames available yet, we invent one. Unfortunately with a hardcoded
      // size.
      //
      // Since mEnabled will affect whether
      // frames are real, or black, we assign new FrameIDs whenever we re-send
      // frames after an mEnabled change.
      for (auto& idChunkPair : mFrames) {
        idChunkPair.first = NewFrameID();
      }
      if (mFrames.IsEmpty()) {
        VideoSegment v;
        v.AppendFrame(nullptr, gfx::IntSize(640, 480), PRINCIPAL_HANDLE_NONE,
                      true, TimeStamp::Now());
        mFrames.AppendElement(std::make_pair(NewFrameID(), *v.GetLastChunk()));
      }
      SendFramesEnsureLocked();
    }
  }

  Mutex mMutex MOZ_UNANNOTATED;
  TimeStamp mLastFrameTime;
  // Once the frame is forced to black, we initialize mBlackImage for use in any
  // following forced-black frames.
  RefPtr<Image> mBlackImage;
  // True once mBlackImage has been sent due to mEnabled being false.
  bool mDisabledBlackImageSent = false;
  bool mEnabled = true;
  // This array is accessed from both the direct video thread, and the graph
  // thread. Protected by mMutex.
  nsTArray<std::pair<ImageContainer::FrameID, VideoChunk>> mFrames;
  // Accessed from both the direct video thread, and the graph thread. Protected
  // by mMutex.
  FrameID mFrameID = 0;
  const RefPtr<VideoFrameContainer> mVideoFrameContainer;
  const RefPtr<AbstractThread> mMainThread;
  const ProducerID mProducerID = ImageContainer::AllocateProducerID();
};

/**
 * This listener observes the first video frame to arrive with a non-empty size,
 * and renders it to its VideoFrameContainer.
 */
class FirstFrameVideoOutput : public VideoOutput {
 public:
  FirstFrameVideoOutput(VideoFrameContainer* aContainer,
                        AbstractThread* aMainThread)
      : VideoOutput(aContainer, aMainThread) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  // NB that this overrides VideoOutput::NotifyRealtimeTrackData, so we can
  // filter out all frames but the first one with a real size. This allows us to
  // later re-use the logic in VideoOutput for rendering that frame.
  void NotifyRealtimeTrackData(MediaTrackGraph* aGraph, TrackTime aTrackOffset,
                               const MediaSegment& aMedia) override {
    MOZ_ASSERT(aMedia.GetType() == MediaSegment::VIDEO);

    if (mInitialSizeFound) {
      return;
    }

    const VideoSegment& video = static_cast<const VideoSegment&>(aMedia);
    for (VideoSegment::ConstChunkIterator c(video); !c.IsEnded(); c.Next()) {
      if (c->mFrame.GetIntrinsicSize() != gfx::IntSize(0, 0)) {
        mInitialSizeFound = true;

        mMainThread->Dispatch(NS_NewRunnableFunction(
            "FirstFrameVideoOutput::FirstFrameRenderedSetter",
            [self = RefPtr<FirstFrameVideoOutput>(this)] {
              self->mFirstFrameRendered = true;
            }));

        // Pick the first frame and run it through the rendering code.
        VideoSegment segment;
        segment.AppendFrame(do_AddRef(c->mFrame.GetImage()),
                            c->mFrame.GetIntrinsicSize(),
                            c->mFrame.GetPrincipalHandle(),
                            c->mFrame.GetForceBlack(), c->mTimeStamp);
        VideoOutput::NotifyRealtimeTrackData(aGraph, aTrackOffset, segment);
        return;
      }
    }
  }

  // Main thread only.
  Watchable<bool> mFirstFrameRendered = {
      false, "FirstFrameVideoOutput::mFirstFrameRendered"};

 private:
  // Whether a frame with a concrete size has been received. May only be
  // accessed on the MTG's appending thread. (this is a direct listener so we
  // get called by whoever is producing this track's data)
  bool mInitialSizeFound = false;
};

}  // namespace mozilla

#endif  // VideoOutput_h
