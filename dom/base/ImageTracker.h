/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* table of images used in a document, for batch locking/unlocking and
 * animating */

#ifndef mozilla_dom_ImageTracker
#define mozilla_dom_ImageTracker

#include "nsDataHashtable.h"
#include "nsHashKeys.h"

class imgIRequest;

namespace mozilla {
namespace dom {

/*
 * Image Tracking
 *
 * Style and content images register their imgIRequests with their document's
 * image tracker, so that we can efficiently tell all descendant images when
 * they are and are not visible. When an image is on-screen, we want to call
 * LockImage() on it so that it doesn't do things like discarding frame data
 * to save memory. The PresShell informs its document's image tracker whether
 * its images should be locked or not via SetImageLockingState().
 *
 * See bug 512260.
 */
class ImageTracker
{
public:
  ImageTracker();
  ImageTracker(const ImageTracker&) = delete;
  ImageTracker& operator=(const ImageTracker&) = delete;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ImageTracker)

  nsresult AddImage(imgIRequest* aImage);

  enum { REQUEST_DISCARD = 0x1 };
  nsresult RemoveImage(imgIRequest* aImage, uint32_t aFlags = 0);

  // Makes the images on this document locked/unlocked. By default, the locking
  // state is unlocked/false.
  nsresult SetImageLockingState(bool aLocked);

  // Makes the images on this document capable of having their animation
  // active or suspended. An Image will animate as long as at least one of its
  // owning Documents needs it to animate; otherwise it can suspend.
  void SetImagesNeedAnimating(bool aAnimating);

  void RequestDiscardAll();

private:
  ~ImageTracker();

  nsDataHashtable<nsPtrHashKey<imgIRequest>, uint32_t> mImageTracker;
  bool mLockingImages;
  bool mAnimatingImages;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ImageTracker
