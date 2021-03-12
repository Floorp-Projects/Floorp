/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* table of images used in a document, for batch locking/unlocking and
 * animating */

#include "ImageTracker.h"

#include "imgIContainer.h"
#include "imgIRequest.h"
#include "mozilla/Preferences.h"
#include "nsXULAppAPI.h"

namespace mozilla::dom {

ImageTracker::ImageTracker() : mLocking(false), mAnimating(true) {}

ImageTracker::~ImageTracker() { SetLockingState(false); }

nsresult ImageTracker::Add(imgIRequest* aImage) {
  MOZ_ASSERT(aImage);

  const nsresult rv = mImages.WithEntryHandle(aImage, [&](auto&& entry) {
    nsresult rv = NS_OK;
    if (entry) {
      // The image is already in the hashtable.  Increment its count.
      uint32_t oldCount = entry.Data();
      MOZ_ASSERT(oldCount > 0, "Entry in the image tracker with count 0!");
      entry.Data() = oldCount + 1;
    } else {
      // A new entry was inserted - set the count to 1.
      entry.Insert(1);

      // If we're locking images, lock this image too.
      if (mLocking) {
        rv = aImage->LockImage();
      }

      // If we're animating images, request that this image be animated too.
      if (mAnimating) {
        nsresult rv2 = aImage->IncrementAnimationConsumers();
        rv = NS_SUCCEEDED(rv) ? rv2 : rv;
      }
    }

    return rv;
  });

  return rv;
}

nsresult ImageTracker::Remove(imgIRequest* aImage, uint32_t aFlags) {
  NS_ENSURE_ARG_POINTER(aImage);

  // Get the old count. It should exist and be > 0.
  if (auto entry = mImages.Lookup(aImage)) {
    MOZ_ASSERT(entry.Data() > 0, "Entry in the image tracker with count 0!");
    // If the count becomes zero, remove it from the tracker.
    if (--entry.Data() == 0) {
      entry.Remove();
    } else {
      return NS_OK;
    }
  } else {
    MOZ_ASSERT_UNREACHABLE("Removing image that wasn't in the tracker!");
    return NS_OK;
  }

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

nsresult ImageTracker::SetLockingState(bool aLocked) {
  if (XRE_IsContentProcess() &&
      !Preferences::GetBool("image.mem.allow_locking_in_content_processes",
                            true)) {
    return NS_OK;
  }

  // If there's no change, there's nothing to do.
  if (mLocking == aLocked) return NS_OK;

  // Otherwise, iterate over our images and perform the appropriate action.
  for (const auto& entry : mImages) {
    imgIRequest* image = entry.GetKey();
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

void ImageTracker::SetAnimatingState(bool aAnimating) {
  // If there's no change, there's nothing to do.
  if (mAnimating == aAnimating) return;

  // Otherwise, iterate over our images and perform the appropriate action.
  for (const auto& entry : mImages) {
    imgIRequest* image = entry.GetKey();
    if (aAnimating) {
      image->IncrementAnimationConsumers();
    } else {
      image->DecrementAnimationConsumers();
    }
  }

  // Update state.
  mAnimating = aAnimating;
}

void ImageTracker::RequestDiscardAll() {
  for (const auto& entry : mImages) {
    entry.GetKey()->RequestDiscard();
  }
}

void ImageTracker::MediaFeatureValuesChangedAllDocuments(
    const MediaFeatureChange& aChange) {
  // Inform every content image used in the document that media feature values
  // have changed.  If the same image is used in multiple places, then we can
  // end up informing them multiple times.  Theme changes are rare though and we
  // don't bother trying to ensure we only do this once per image.
  //
  // Pull the images out into an array and iterate over them, in case the
  // image notifications do something that ends up modifying the table.
  nsTArray<nsCOMPtr<imgIContainer>> images;
  for (const auto& entry : mImages) {
    imgIRequest* req = entry.GetKey();
    nsCOMPtr<imgIContainer> image;
    req->GetImage(getter_AddRefs(image));
    if (!image) {
      continue;
    }
    images.AppendElement(image->Unwrap());
  }
  for (imgIContainer* image : images) {
    image->MediaFeatureValuesChangedAllDocuments(aChange);
  }
}

}  // namespace mozilla::dom
