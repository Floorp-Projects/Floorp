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

namespace mozilla {

namespace dom {
class HTMLMediaElement;
}

namespace layers {
class Image;
class ImageContainer;
}

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
public:
  typedef layers::ImageContainer ImageContainer;
  typedef layers::Image Image;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoFrameContainer)

  VideoFrameContainer(dom::HTMLMediaElement* aElement,
                      already_AddRefed<ImageContainer> aContainer);
  ~VideoFrameContainer();

  // Call on any thread
  void SetCurrentFrame(const gfxIntSize& aIntrinsicSize, Image* aImage,
                       TimeStamp aTargetTime);
  void ClearCurrentFrame(bool aResetSize = false);
  // Reset the VideoFrameContainer
  void Reset();
  // Time in seconds by which the last painted video frame was late by.
  // E.g. if the last painted frame should have been painted at time t,
  // but was actually painted at t+n, this returns n in seconds. Threadsafe.
  double GetFrameDelay();
  // Call on main thread
  void Invalidate();
  ImageContainer* GetImageContainer();
  void ForgetElement() { mElement = nullptr; }

protected:
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
  // The time at which the current video frame should have been painted.
  // Access protected by mVideoUpdateLock.
  TimeStamp mPaintTarget;
  // The delay between the last video frame being presented and it being
  // painted. This is time elapsed after mPaintTarget until the most recently
  // painted frame appeared on screen.
  TimeDuration mPaintDelay;
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

  bool mNeedInvalidation;
};

}

#endif /* VIDEOFRAMECONTAINER_H_ */
