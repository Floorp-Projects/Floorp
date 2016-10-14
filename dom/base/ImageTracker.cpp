/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* table of images used in a document, for batch locking/unlocking and
 * animating */

#include "ImageTracker.h"

namespace mozilla {
namespace dom {

ImageTracker::ImageTracker()
  : mLocking(false)
  , mAnimating(true)
{
}

ImageTracker::~ImageTracker()
{
  SetLockingState(false);
}

nsresult
ImageTracker::Add(imgIRequest* aImage)
{
  MOZ_ASSERT(aImage);

  // See if the image is already in the hashtable. If it is, get the old count.
  uint32_t oldCount = 0;
  mImages.Get(aImage, &oldCount);

  // Put the image in the hashtable, with the proper count.
  mImages.Put(aImage, oldCount + 1);

  nsresult rv = NS_OK;

  // If this is the first insertion and we're locking images, lock this image
  // too.
  if (oldCount == 0 && mLocking) {
    rv = aImage->LockImage();
  }

  // If this is the first insertion and we're animating images, request
  // that this image be animated too.
  if (oldCount == 0 && mAnimating) {
    nsresult rv2 = aImage->IncrementAnimationConsumers();
    rv = NS_SUCCEEDED(rv) ? rv2 : rv;
  }

  return rv;
}

nsresult
ImageTracker::Remove(imgIRequest* aImage, uint32_t aFlags)
{
  NS_ENSURE_ARG_POINTER(aImage);

  // Get the old count. It should exist and be > 0.
  uint32_t count = 0;
  DebugOnly<bool> found = mImages.Get(aImage, &count);
  MOZ_ASSERT(found, "Removing image that wasn't in the tracker!");
  MOZ_ASSERT(count > 0, "Entry in the cache tracker with count 0!");

  // We're removing, so decrement the count.
  count--;

  // If the count is now zero, remove from the tracker.
  // Otherwise, set the new value.
  if (count != 0) {
    mImages.Put(aImage, count);
    return NS_OK;
  }

  mImages.Remove(aImage);

  nsresult rv = NS_OK;

  // Now that we're no longer tracking this image, unlock it if we'd
  // previously locked it.
  if (mLocking) {
    rv = aImage->UnlockImage();
  }

  // If we're animating images, remove our request to animate this one.
  if (mAnimating) {
    nsresult rv2 = aImage->DecrementAnimationConsumers();
    rv = NS_SUCCEEDED(rv) ? rv2 : rv;
  }

  if (aFlags & REQUEST_DISCARD) {
    // Request that the image be discarded if nobody else holds a lock on it.
    // Do this even if !mLocking, because even if we didn't just unlock
    // this image, it might still be a candidate for discarding.
    aImage->RequestDiscard();
  }

  return rv;
}

nsresult
ImageTracker::SetLockingState(bool aLocked)
{
  if (XRE_IsContentProcess() &&
      !Preferences::GetBool("image.mem.allow_locking_in_content_processes", true)) {
    return NS_OK;
  }

  // If there's no change, there's nothing to do.
  if (mLocking == aLocked)
    return NS_OK;

  // Otherwise, iterate over our images and perform the appropriate action.
  for (auto iter = mImages.Iter(); !iter.Done(); iter.Next()) {
    imgIRequest* image = iter.Key();
    if (aLocked) {
      image->LockImage();
    } else {
      image->UnlockImage();
    }
  }

  // Update state.
  mLocking = aLocked;

  return NS_OK;
}

void
ImageTracker::SetAnimatingState(bool aAnimating)
{
  // If there's no change, there's nothing to do.
  if (mAnimating == aAnimating)
    return;

  // Otherwise, iterate over our images and perform the appropriate action.
  for (auto iter = mImages.Iter(); !iter.Done(); iter.Next()) {
    imgIRequest* image = iter.Key();
    if (aAnimating) {
      image->IncrementAnimationConsumers();
    } else {
      image->DecrementAnimationConsumers();
    }
  }

  // Update state.
  mAnimating = aAnimating;
}

void
ImageTracker::RequestDiscardAll()
{
  for (auto iter = mImages.Iter(); !iter.Done(); iter.Next()) {
    iter.Key()->RequestDiscard();
  }
}

} // namespace dom
} // namespace mozilla
