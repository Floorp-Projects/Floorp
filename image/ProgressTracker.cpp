/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageLogging.h"
#include "ProgressTracker.h"

#include "imgIContainer.h"
#include "imgINotificationObserver.h"
#include "imgIRequest.h"
#include "Image.h"
#include "nsNetUtil.h"
#include "nsIObserverService.h"

#include "mozilla/Assertions.h"
#include "mozilla/Services.h"

using mozilla::WeakPtr;

namespace mozilla {
namespace image {

static void
CheckProgressConsistency(Progress aProgress)
{
  // Check preconditions for every progress bit.

  if (aProgress & FLAG_SIZE_AVAILABLE) {
    // No preconditions.
  }
  if (aProgress & FLAG_DECODE_COMPLETE) {
    // No preconditions.
  }
  if (aProgress & FLAG_FRAME_COMPLETE) {
    // No preconditions.
  }
  if (aProgress & FLAG_LOAD_COMPLETE) {
    // No preconditions.
  }
  if (aProgress & FLAG_ONLOAD_BLOCKED) {
    // No preconditions.
  }
  if (aProgress & FLAG_ONLOAD_UNBLOCKED) {
    MOZ_ASSERT(aProgress & FLAG_ONLOAD_BLOCKED);
    MOZ_ASSERT(aProgress & (FLAG_SIZE_AVAILABLE | FLAG_HAS_ERROR));
  }
  if (aProgress & FLAG_IS_ANIMATED) {
    MOZ_ASSERT(aProgress & FLAG_SIZE_AVAILABLE);
  }
  if (aProgress & FLAG_HAS_TRANSPARENCY) {
    MOZ_ASSERT(aProgress & FLAG_SIZE_AVAILABLE);
  }
  if (aProgress & FLAG_LAST_PART_COMPLETE) {
    MOZ_ASSERT(aProgress & FLAG_LOAD_COMPLETE);
  }
  if (aProgress & FLAG_HAS_ERROR) {
    // No preconditions.
  }
}

void
ProgressTracker::SetImage(Image* aImage)
{
  MutexAutoLock lock(mImageMutex);
  MOZ_ASSERT(aImage, "Setting null image");
  MOZ_ASSERT(!mImage, "Setting image when we already have one");
  mImage = aImage;
}

void
ProgressTracker::ResetImage()
{
  MutexAutoLock lock(mImageMutex);
  MOZ_ASSERT(mImage, "Resetting image when it's already null!");
  mImage = nullptr;
}

uint32_t
ProgressTracker::GetImageStatus() const
{
  uint32_t status = imgIRequest::STATUS_NONE;

  // Translate our current state to a set of imgIRequest::STATE_* flags.
  if (mProgress & FLAG_SIZE_AVAILABLE) {
    status |= imgIRequest::STATUS_SIZE_AVAILABLE;
  }
  if (mProgress & FLAG_DECODE_COMPLETE) {
    status |= imgIRequest::STATUS_DECODE_COMPLETE;
  }
  if (mProgress & FLAG_FRAME_COMPLETE) {
    status |= imgIRequest::STATUS_FRAME_COMPLETE;
  }
  if (mProgress & FLAG_LOAD_COMPLETE) {
    status |= imgIRequest::STATUS_LOAD_COMPLETE;
  }
  if (mProgress & FLAG_IS_ANIMATED) {
    status |= imgIRequest::STATUS_IS_ANIMATED;
  }
  if (mProgress & FLAG_HAS_TRANSPARENCY) {
    status |= imgIRequest::STATUS_HAS_TRANSPARENCY;
  }
  if (mProgress & FLAG_HAS_ERROR) {
    status |= imgIRequest::STATUS_ERROR;
  }

  return status;
}

// A helper class to allow us to call SyncNotify asynchronously.
class AsyncNotifyRunnable : public nsRunnable
{
  public:
    AsyncNotifyRunnable(ProgressTracker* aTracker,
                        IProgressObserver* aObserver)
      : mTracker(aTracker)
    {
      MOZ_ASSERT(NS_IsMainThread(), "Should be created on the main thread");
      MOZ_ASSERT(aTracker, "aTracker should not be null");
      MOZ_ASSERT(aObserver, "aObserver should not be null");
      mObservers.AppendElement(aObserver);
    }

    NS_IMETHOD Run()
    {
      MOZ_ASSERT(NS_IsMainThread(), "Should be running on the main thread");
      MOZ_ASSERT(mTracker, "mTracker should not be null");
      for (uint32_t i = 0; i < mObservers.Length(); ++i) {
        mObservers[i]->SetNotificationsDeferred(false);
        mTracker->SyncNotify(mObservers[i]);
      }

      mTracker->mRunnable = nullptr;
      return NS_OK;
    }

    void AddObserver(IProgressObserver* aObserver)
    {
      mObservers.AppendElement(aObserver);
    }

    void RemoveObserver(IProgressObserver* aObserver)
    {
      mObservers.RemoveElement(aObserver);
    }

  private:
    friend class ProgressTracker;

    nsRefPtr<ProgressTracker> mTracker;
    nsTArray<nsRefPtr<IProgressObserver>> mObservers;
};

void
ProgressTracker::Notify(IProgressObserver* aObserver)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (MOZ_LOG_TEST(GetImgLog(), LogLevel::Debug)) {
    nsRefPtr<Image> image = GetImage();
    if (image && image->GetURI()) {
      nsRefPtr<ImageURL> uri(image->GetURI());
      nsAutoCString spec;
      uri->GetSpec(spec);
      LOG_FUNC_WITH_PARAM(GetImgLog(),
                          "ProgressTracker::Notify async", "uri", spec.get());
    } else {
      LOG_FUNC_WITH_PARAM(GetImgLog(),
                          "ProgressTracker::Notify async", "uri", "<unknown>");
    }
  }

  aObserver->SetNotificationsDeferred(true);

  // If we have an existing runnable that we can use, we just append this
  // observer to its list of observers to be notified. This ensures we don't
  // unnecessarily delay onload.
  AsyncNotifyRunnable* runnable =
    static_cast<AsyncNotifyRunnable*>(mRunnable.get());

  if (runnable) {
    runnable->AddObserver(aObserver);
  } else {
    mRunnable = new AsyncNotifyRunnable(this, aObserver);
    NS_DispatchToCurrentThread(mRunnable);
  }
}

// A helper class to allow us to call SyncNotify asynchronously for a given,
// fixed, state.
class AsyncNotifyCurrentStateRunnable : public nsRunnable
{
  public:
    AsyncNotifyCurrentStateRunnable(ProgressTracker* aProgressTracker,
                                    IProgressObserver* aObserver)
      : mProgressTracker(aProgressTracker)
      , mObserver(aObserver)
    {
      MOZ_ASSERT(NS_IsMainThread(), "Should be created on the main thread");
      MOZ_ASSERT(mProgressTracker, "mProgressTracker should not be null");
      MOZ_ASSERT(mObserver, "mObserver should not be null");
      mImage = mProgressTracker->GetImage();
    }

    NS_IMETHOD Run()
    {
      MOZ_ASSERT(NS_IsMainThread(), "Should be running on the main thread");
      mObserver->SetNotificationsDeferred(false);

      mProgressTracker->SyncNotify(mObserver);
      return NS_OK;
    }

  private:
    nsRefPtr<ProgressTracker> mProgressTracker;
    nsRefPtr<IProgressObserver> mObserver;

    // We have to hold on to a reference to the tracker's image, just in case
    // it goes away while we're in the event queue.
    nsRefPtr<Image> mImage;
};

void
ProgressTracker::NotifyCurrentState(IProgressObserver* aObserver)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (MOZ_LOG_TEST(GetImgLog(), LogLevel::Debug)) {
    nsRefPtr<Image> image = GetImage();
    nsAutoCString spec;
    if (image && image->GetURI()) {
      image->GetURI()->GetSpec(spec);
    }
    LOG_FUNC_WITH_PARAM(GetImgLog(),
                        "ProgressTracker::NotifyCurrentState", "uri", spec.get());
  }

  aObserver->SetNotificationsDeferred(true);

  nsCOMPtr<nsIRunnable> ev = new AsyncNotifyCurrentStateRunnable(this,
                                                                 aObserver);
  NS_DispatchToCurrentThread(ev);
}

#define NOTIFY_IMAGE_OBSERVERS(OBSERVERS, FUNC) \
  do { \
    ObserverArray::ForwardIterator iter(OBSERVERS); \
    while (iter.HasMore()) { \
      nsRefPtr<IProgressObserver> observer = iter.GetNext().get(); \
      if (observer && !observer->NotificationsDeferred()) { \
        observer->FUNC; \
      } \
    } \
  } while (false);

/* static */ void
ProgressTracker::SyncNotifyInternal(ObserverArray& aObservers,
                                    bool aHasImage,
                                    Progress aProgress,
                                    const nsIntRect& aDirtyRect)
{
  MOZ_ASSERT(NS_IsMainThread());

  typedef imgINotificationObserver I;

  if (aProgress & FLAG_SIZE_AVAILABLE) {
    NOTIFY_IMAGE_OBSERVERS(aObservers, Notify(I::SIZE_AVAILABLE));
  }

  if (aProgress & FLAG_ONLOAD_BLOCKED) {
    NOTIFY_IMAGE_OBSERVERS(aObservers, BlockOnload());
  }

  if (aHasImage) {
    // OnFrameUpdate
    // If there's any content in this frame at all (always true for
    // vector images, true for raster images that have decoded at
    // least one frame) then send OnFrameUpdate.
    if (!aDirtyRect.IsEmpty()) {
      NOTIFY_IMAGE_OBSERVERS(aObservers, Notify(I::FRAME_UPDATE, &aDirtyRect));
    }

    if (aProgress & FLAG_FRAME_COMPLETE) {
      NOTIFY_IMAGE_OBSERVERS(aObservers, Notify(I::FRAME_COMPLETE));
    }

    if (aProgress & FLAG_HAS_TRANSPARENCY) {
      NOTIFY_IMAGE_OBSERVERS(aObservers, Notify(I::HAS_TRANSPARENCY));
    }

    if (aProgress & FLAG_IS_ANIMATED) {
      NOTIFY_IMAGE_OBSERVERS(aObservers, Notify(I::IS_ANIMATED));
    }
  }

  // Send UnblockOnload before OnStopDecode and OnStopRequest. This allows
  // observers that can fire events when they receive those notifications to do
  // so then, instead of being forced to wait for UnblockOnload.
  if (aProgress & FLAG_ONLOAD_UNBLOCKED) {
    NOTIFY_IMAGE_OBSERVERS(aObservers, UnblockOnload());
  }

  if (aProgress & FLAG_DECODE_COMPLETE) {
    MOZ_ASSERT(aHasImage, "Stopped decoding without ever having an image?");
    NOTIFY_IMAGE_OBSERVERS(aObservers, Notify(I::DECODE_COMPLETE));
  }

  if (aProgress & FLAG_LOAD_COMPLETE) {
    NOTIFY_IMAGE_OBSERVERS(aObservers,
                           OnLoadComplete(aProgress & FLAG_LAST_PART_COMPLETE));
  }
}

void
ProgressTracker::SyncNotifyProgress(Progress aProgress,
                                    const nsIntRect& aInvalidRect
                                                  /* = nsIntRect() */)
{
  MOZ_ASSERT(NS_IsMainThread(), "Use mObservers on main thread only");

  // Don't unblock onload if we're not blocked.
  Progress progress = Difference(aProgress);
  if (!((mProgress | progress) & FLAG_ONLOAD_BLOCKED)) {
    progress &= ~FLAG_ONLOAD_UNBLOCKED;
  }

  // XXX(seth): Hack to work around the fact that some observers have bugs and
  // need to get onload blocking notifications multiple times. We should fix
  // those observers and remove this.
  if ((aProgress & FLAG_DECODE_COMPLETE) &&
      (mProgress & FLAG_ONLOAD_BLOCKED) &&
      (mProgress & FLAG_ONLOAD_UNBLOCKED)) {
    progress |= FLAG_ONLOAD_BLOCKED | FLAG_ONLOAD_UNBLOCKED;
  }

  // Apply the changes.
  mProgress |= progress;

  CheckProgressConsistency(mProgress);

  // Send notifications.
  SyncNotifyInternal(mObservers, HasImage(), progress, aInvalidRect);

  if (progress & FLAG_HAS_ERROR) {
    FireFailureNotification();
  }
}

void
ProgressTracker::SyncNotify(IProgressObserver* aObserver)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<Image> image = GetImage();

  nsAutoCString spec;
  if (image && image->GetURI()) {
    image->GetURI()->GetSpec(spec);
  }
  LOG_SCOPE_WITH_PARAM(GetImgLog(),
                       "ProgressTracker::SyncNotify", "uri", spec.get());

  nsIntRect rect;
  if (image) {
    if (NS_FAILED(image->GetWidth(&rect.width)) ||
        NS_FAILED(image->GetHeight(&rect.height))) {
      // Either the image has no intrinsic size, or it has an error.
      rect = GetMaxSizedIntRect();
    }
  }

  ObserverArray array;
  array.AppendElement(aObserver);
  SyncNotifyInternal(array, !!image, mProgress, rect);
}

void
ProgressTracker::EmulateRequestFinished(IProgressObserver* aObserver)
{
  MOZ_ASSERT(NS_IsMainThread(),
             "SyncNotifyState and mObservers are not threadsafe");
  nsRefPtr<IProgressObserver> kungFuDeathGrip(aObserver);

  if (mProgress & FLAG_ONLOAD_BLOCKED && !(mProgress & FLAG_ONLOAD_UNBLOCKED)) {
    aObserver->UnblockOnload();
  }

  if (!(mProgress & FLAG_LOAD_COMPLETE)) {
    aObserver->OnLoadComplete(true);
  }
}

void
ProgressTracker::AddObserver(IProgressObserver* aObserver)
{
  MOZ_ASSERT(NS_IsMainThread());
  mObservers.AppendElementUnlessExists(aObserver);
}

bool
ProgressTracker::RemoveObserver(IProgressObserver* aObserver)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Remove the observer from the list.
  bool removed = mObservers.RemoveElement(aObserver);

  // Observers can get confused if they don't get all the proper teardown
  // notifications. Part ways on good terms.
  if (removed && !aObserver->NotificationsDeferred()) {
    EmulateRequestFinished(aObserver);
  }

  // Make sure we don't give callbacks to an observer that isn't interested in
  // them any more.
  AsyncNotifyRunnable* runnable =
    static_cast<AsyncNotifyRunnable*>(mRunnable.get());

  if (aObserver->NotificationsDeferred() && runnable) {
    runnable->RemoveObserver(aObserver);
    aObserver->SetNotificationsDeferred(false);
  }

  return removed;
}

bool
ProgressTracker::FirstObserverIs(IProgressObserver* aObserver)
{
  MOZ_ASSERT(NS_IsMainThread(), "Use mObservers on main thread only");
  ObserverArray::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    nsRefPtr<IProgressObserver> observer = iter.GetNext().get();
    if (observer) {
      return observer.get() == aObserver;
    }
  }
  return false;
}

void
ProgressTracker::OnUnlockedDraw()
{
  MOZ_ASSERT(NS_IsMainThread());
  NOTIFY_IMAGE_OBSERVERS(mObservers,
                         Notify(imgINotificationObserver::UNLOCKED_DRAW));
}

void
ProgressTracker::ResetForNewRequest()
{
  MOZ_ASSERT(NS_IsMainThread());
  mProgress = NoProgress;
  CheckProgressConsistency(mProgress);
}

void
ProgressTracker::OnDiscard()
{
  MOZ_ASSERT(NS_IsMainThread());
  NOTIFY_IMAGE_OBSERVERS(mObservers,
                         Notify(imgINotificationObserver::DISCARD));
}

void
ProgressTracker::OnImageAvailable()
{
  MOZ_ASSERT(NS_IsMainThread());
  // Notify any imgRequestProxys that are observing us that we have an Image.
  ObserverArray::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    nsRefPtr<IProgressObserver> observer = iter.GetNext().get();
    if (observer) {
      observer->SetHasImage();
    }
  }
}

void
ProgressTracker::FireFailureNotification()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Some kind of problem has happened with image decoding.
  // Report the URI to net:failed-to-process-uri-conent observers.
  nsRefPtr<Image> image = GetImage();
  if (image) {
    // Should be on main thread, so ok to create a new nsIURI.
    nsCOMPtr<nsIURI> uri;
    {
      nsRefPtr<ImageURL> threadsafeUriData = image->GetURI();
      uri = threadsafeUriData ? threadsafeUriData->ToIURI() : nullptr;
    }
    if (uri) {
      nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
      if (os) {
        os->NotifyObservers(uri, "net:failed-to-process-uri-content", nullptr);
      }
    }
  }
}

} // namespace image
} // namespace mozilla
