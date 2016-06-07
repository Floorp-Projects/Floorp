/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VIDEOFRAMECONTAINER_H_
#define VIDEOFRAMECONTAINER_H_

#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "gfxPoint.h"
#include "nsCOMPtr.h"
#include "ImageContainer.h"
#include "MediaSegment.h"

namespace mozilla {

namespace dom {
class HTMLMediaElement;
} // namespace dom

/**
 * This object is used in the decoder backend threads and the main thread
 * to manage the "current video frame" state. This state includes timing data
 * and an intrinsic size (see below).
 * This has to be a thread-safe object since it's accessed by resource decoders
 * and other off-main-thread components. So we can't put this state in the media
 * element itself ... well, maybe we could, but it could be risky and/or
 * confusing.
 */
class VideoFrameContainer {
  ~VideoFrameContainer();

public:
  typedef layers::ImageContainer ImageContainer;
  typedef layers::Image Image;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoFrameContainer)

  VideoFrameContainer(dom::HTMLMediaElement* aElement,
                      already_AddRefed<ImageContainer> aContainer);

  // Call on any thread
  // Returns the last principalHandle we notified mElement about.
  PrincipalHandle GetLastPrincipalHandle();
  // We will notify mElement that aPrincipalHandle has been applied when all
  // FrameIDs prior to aFrameID have been flushed out.
  // aFrameID is ignored if aPrincipalHandle already is our pending principalHandle.
  void UpdatePrincipalHandleForFrameID(const PrincipalHandle& aPrincipalHandle,
                                       const ImageContainer::FrameID& aFrameID);
  void SetCurrentFrame(const gfx::IntSize& aIntrinsicSize, Image* aImage,
                       const TimeStamp& aTargetTime);
  void SetCurrentFrames(const gfx::IntSize& aIntrinsicSize,
                        const nsTArray<ImageContainer::NonOwningImage>& aImages);
  void ClearCurrentFrame(const gfx::IntSize& aIntrinsicSize)
  {
    SetCurrentFrames(aIntrinsicSize, nsTArray<ImageContainer::NonOwningImage>());
  }

  void ClearCurrentFrame();
  // Make the current frame the only frame in the container, i.e. discard
  // all future frames.
  void ClearFutureFrames();
  // Time in seconds by which the last painted video frame was late by.
  // E.g. if the last painted frame should have been painted at time t,
  // but was actually painted at t+n, this returns n in seconds. Threadsafe.
  double GetFrameDelay();

  // Returns a new frame ID for SetCurrentFrames().  The client must either
  // call this on only one thread or provide barriers.  Do not use together
  // with SetCurrentFrame().
  ImageContainer::FrameID NewFrameID()
  {
    return ++mFrameID;
  }

  // Call on main thread
  enum {
    INVALIDATE_DEFAULT,
    INVALIDATE_FORCE
  };
  void Invalidate() { InvalidateWithFlags(INVALIDATE_DEFAULT); }
  void InvalidateWithFlags(uint32_t aFlags);
  ImageContainer* GetImageContainer();
  void ForgetElement() { mElement = nullptr; }

  uint32_t GetDroppedImageCount() { return mImageContainer->GetDroppedImageCount(); }

protected:
  void SetCurrentFramesLocked(const gfx::IntSize& aIntrinsicSize,
                              const nsTArray<ImageContainer::NonOwningImage>& aImages);

  // Non-addreffed pointer to the element. The element calls ForgetElement
  // to clear this reference when the element is destroyed.
  dom::HTMLMediaElement* mElement;
  RefPtr<ImageContainer> mImageContainer;

  // mMutex protects all the fields below.
  Mutex mMutex;
  // The intrinsic size is the ideal size which we should render the
  // ImageContainer's current Image at.
  // This can differ from the Image's actual size when the media resource
  // specifies that the Image should be stretched to have the correct aspect
  // ratio.
  gfx::IntSize mIntrinsicSize;
  // We maintain our own mFrameID which is auto-incremented at every
  // SetCurrentFrame() or NewFrameID() call.
  ImageContainer::FrameID mFrameID;
  // True when the intrinsic size has been changed by SetCurrentFrame() since
  // the last call to Invalidate().
  // The next call to Invalidate() will recalculate
  // and update the intrinsic size on the element, request a frame reflow and
  // then reset this flag.
  bool mIntrinsicSizeChanged;
  // True when the Image size has changed since the last time Invalidate() was
  // called. When set, the next call to Invalidate() will ensure that the
  // frame is fully invalidated instead of just invalidating for the image change
  // in the ImageLayer.
  bool mImageSizeChanged;
  // The last PrincipalHandle we notified mElement about.
  PrincipalHandle mLastPrincipalHandle;
  // The PrincipalHandle the client has notified us is changing with FrameID
  // mFrameIDForPendingPrincipalHandle.
  PrincipalHandle mPendingPrincipalHandle;
  // The FrameID for which mPendingPrincipal is first valid.
  ImageContainer::FrameID mFrameIDForPendingPrincipalHandle;
};

} // namespace mozilla

#endif /* VIDEOFRAMECONTAINER_H_ */
