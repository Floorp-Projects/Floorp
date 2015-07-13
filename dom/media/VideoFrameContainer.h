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
#include "nsAutoPtr.h"
#include "ImageContainer.h"

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
  B2G_ACL_EXPORT ~VideoFrameContainer();

public:
  typedef layers::ImageContainer ImageContainer;
  typedef layers::Image Image;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoFrameContainer)

  VideoFrameContainer(dom::HTMLMediaElement* aElement,
                      already_AddRefed<ImageContainer> aContainer);

  // Call on any thread
  B2G_ACL_EXPORT void SetCurrentFrame(const gfxIntSize& aIntrinsicSize, Image* aImage,
                       const TimeStamp& aTargetTime);
  void SetCurrentFrames(const gfxIntSize& aIntrinsicSize,
                        const nsTArray<ImageContainer::NonOwningImage>& aImages);
  void ClearCurrentFrame(const gfxIntSize& aIntrinsicSize)
  {
    SetCurrentFrames(aIntrinsicSize, nsTArray<ImageContainer::NonOwningImage>());
  }

  void ClearCurrentFrame();
  // Time in seconds by which the last painted video frame was late by.
  // E.g. if the last painted frame should have been painted at time t,
  // but was actually painted at t+n, this returns n in seconds. Threadsafe.
  double GetFrameDelay();
  // Call on main thread
  enum {
    INVALIDATE_DEFAULT,
    INVALIDATE_FORCE
  };
  void Invalidate() { InvalidateWithFlags(INVALIDATE_DEFAULT); }
  B2G_ACL_EXPORT void InvalidateWithFlags(uint32_t aFlags);
  B2G_ACL_EXPORT ImageContainer* GetImageContainer();
  void ForgetElement() { mElement = nullptr; }

protected:
  void SetCurrentFramesLocked(const gfxIntSize& aIntrinsicSize,
                              const nsTArray<ImageContainer::NonOwningImage>& aImages);

  // Non-addreffed pointer to the element. The element calls ForgetElement
  // to clear this reference when the element is destroyed.
  dom::HTMLMediaElement* mElement;
  nsRefPtr<ImageContainer> mImageContainer;

  // mMutex protects all the fields below.
  Mutex mMutex;
  // The intrinsic size is the ideal size which we should render the
  // ImageContainer's current Image at.
  // This can differ from the Image's actual size when the media resource
  // specifies that the Image should be stretched to have the correct aspect
  // ratio.
  gfxIntSize mIntrinsicSize;
  // For SetCurrentFrame callers we maintain our own mFrameID which is auto-
  // incremented at every SetCurrentFrame.
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
};

} // namespace mozilla

#endif /* VIDEOFRAMECONTAINER_H_ */
