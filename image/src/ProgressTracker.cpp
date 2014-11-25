/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageLogging.h"
#include "ProgressTracker.h"

#include "imgIContainer.h"
#include "imgRequestProxy.h"
#include "Image.h"
#include "nsNetUtil.h"
#include "nsIObserverService.h"

#include "mozilla/Assertions.h"
#include "mozilla/Services.h"

using mozilla::WeakPtr;

namespace mozilla {
namespace image {

ProgressTrackerInit::ProgressTrackerInit(Image* aImage,
                                         ProgressTracker* aTracker)
{
  MOZ_ASSERT(aImage);

  if (aTracker) {
    mTracker = aTracker;
    mTracker->SetImage(aImage);
  } else {
    mTracker = new ProgressTracker(aImage);
  }
  aImage->SetProgressTracker(mTracker);
  MOZ_ASSERT(mTracker);
}

ProgressTrackerInit::~ProgressTrackerInit()
{
  mTracker->ResetImage();
}

static void
CheckProgressConsistency(Progress aProgress)
{
  // Check preconditions for every progress bit.

  if (aProgress & FLAG_SIZE_AVAILABLE) {
    // No preconditions.
  }
  if (aProgress & FLAG_DECODE_STARTED) {
    // No preconditions.
  }
  if (aProgress & FLAG_DECODE_COMPLETE) {
    MOZ_ASSERT(aProgress & FLAG_DECODE_STARTED);
  }
  if (aProgress & FLAG_FRAME_COMPLETE) {
    MOZ_ASSERT(aProgress & FLAG_DECODE_STARTED);
  }
  if (aProgress & FLAG_LOAD_COMPLETE) {
    // No preconditions.
  }
  if (aProgress & FLAG_ONLOAD_BLOCKED) {
    if (aProgress & FLAG_IS_MULTIPART) {
      MOZ_ASSERT(aProgress & FLAG_ONLOAD_UNBLOCKED);
    } else {
      MOZ_ASSERT(aProgress & FLAG_DECODE_STARTED);
    }
  }
  if (aProgress & FLAG_ONLOAD_UNBLOCKED) {
    MOZ_ASSERT(aProgress & FLAG_ONLOAD_BLOCKED);
    MOZ_ASSERT(aProgress & (FLAG_FRAME_COMPLETE |
                            FLAG_IS_MULTIPART |
                            FLAG_HAS_ERROR));
  }
  if (aProgress & FLAG_IS_ANIMATED) {
    MOZ_ASSERT(aProgress & FLAG_DECODE_STARTED);
    MOZ_ASSERT(aProgress & FLAG_SIZE_AVAILABLE);
  }
  if (aProgress & FLAG_HAS_TRANSPARENCY) {
    MOZ_ASSERT(aProgress & FLAG_SIZE_AVAILABLE);
  }
  if (aProgress & FLAG_IS_MULTIPART) {
    // No preconditions.
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
  NS_ABORT_IF_FALSE(aImage, "Setting null image");
  NS_ABORT_IF_FALSE(!mImage, "Setting image when we already have one");
  mImage = aImage;
}

void
ProgressTracker::ResetImage()
{
  NS_ABORT_IF_FALSE(mImage, "Resetting image when it's already null!");
  mImage = nullptr;
}

void ProgressTracker::SetIsMultipart()
{
  if (mProgress & FLAG_IS_MULTIPART) {
    return;
  }

  MOZ_ASSERT(!(mProgress & FLAG_ONLOAD_BLOCKED),
             "Blocked onload before we knew we were multipart?");

  // Set the MULTIPART flag and ensure that we never block onload.
  mProgress |= FLAG_IS_MULTIPART | FLAG_ONLOAD_BLOCKED | FLAG_ONLOAD_UNBLOCKED;

  CheckProgressConsistency(mProgress);
}

bool
ProgressTracker::IsLoading() const
{
  // Checking for whether OnStopRequest has fired allows us to say we're
  // loading before OnStartRequest gets called, letting the request properly
  // get removed from the cache in certain cases.
  return !(mProgress & FLAG_LOAD_COMPLETE);
}

uint32_t
ProgressTracker::GetImageStatus() const
{
  uint32_t status = imgIRequest::STATUS_NONE;

  // Translate our current state to a set of imgIRequest::STATE_* flags.
  if (mProgress & FLAG_SIZE_AVAILABLE) {
    status |= imgIRequest::STATUS_SIZE_AVAILABLE;
  }
  if (mProgress & FLAG_DECODE_STARTED) {
    status |= imgIRequest::STATUS_DECODE_STARTED;
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
                        imgRequestProxy* aRequestProxy)
      : mTracker(aTracker)
    {
      MOZ_ASSERT(NS_IsMainThread(), "Should be created on the main thread");
      MOZ_ASSERT(aTracker, "aTracker should not be null");
      MOZ_ASSERT(aRequestProxy, "aRequestProxy should not be null");
      mProxies.AppendElement(aRequestProxy);
    }

    NS_IMETHOD Run()
    {
      MOZ_ASSERT(NS_IsMainThread(), "Should be running on the main thread");
      MOZ_ASSERT(mTracker, "mTracker should not be null");
      for (uint32_t i = 0; i < mProxies.Length(); ++i) {
        mProxies[i]->SetNotificationsDeferred(false);
        mTracker->SyncNotify(mProxies[i]);
      }

      mTracker->mRunnable = nullptr;
      return NS_OK;
    }

    void AddProxy(imgRequestProxy* aRequestProxy)
    {
      mProxies.AppendElement(aRequestProxy);
    }

    void RemoveProxy(imgRequestProxy* aRequestProxy)
    {
      mProxies.RemoveElement(aRequestProxy);
    }

  private:
    friend class ProgressTracker;

    nsRefPtr<ProgressTracker> mTracker;
    nsTArray<nsRefPtr<imgRequestProxy>> mProxies;
};

void
ProgressTracker::Notify(imgRequestProxy* proxy)
{
  MOZ_ASSERT(NS_IsMainThread(), "imgRequestProxy is not threadsafe");
#ifdef PR_LOGGING
  if (mImage && mImage->GetURI()) {
    nsRefPtr<ImageURL> uri(mImage->GetURI());
    nsAutoCString spec;
    uri->GetSpec(spec);
    LOG_FUNC_WITH_PARAM(GetImgLog(), "ProgressTracker::Notify async", "uri", spec.get());
  } else {
    LOG_FUNC_WITH_PARAM(GetImgLog(), "ProgressTracker::Notify async", "uri", "<unknown>");
  }
#endif

  proxy->SetNotificationsDeferred(true);

  // If we have an existing runnable that we can use, we just append this proxy
  // to its list of proxies to be notified. This ensures we don't unnecessarily
  // delay onload.
  AsyncNotifyRunnable* runnable =
    static_cast<AsyncNotifyRunnable*>(mRunnable.get());

  if (runnable) {
    runnable->AddProxy(proxy);
  } else {
    mRunnable = new AsyncNotifyRunnable(this, proxy);
    NS_DispatchToCurrentThread(mRunnable);
  }
}

// A helper class to allow us to call SyncNotify asynchronously for a given,
// fixed, state.
class AsyncNotifyCurrentStateRunnable : public nsRunnable
{
  public:
    AsyncNotifyCurrentStateRunnable(ProgressTracker* aProgressTracker,
                                    imgRequestProxy* aProxy)
      : mProgressTracker(aProgressTracker)
      , mProxy(aProxy)
    {
      MOZ_ASSERT(NS_IsMainThread(), "Should be created on the main thread");
      MOZ_ASSERT(mProgressTracker, "mProgressTracker should not be null");
      MOZ_ASSERT(mProxy, "mProxy should not be null");
      mImage = mProgressTracker->GetImage();
    }

    NS_IMETHOD Run()
    {
      MOZ_ASSERT(NS_IsMainThread(), "Should be running on the main thread");
      mProxy->SetNotificationsDeferred(false);

      mProgressTracker->SyncNotify(mProxy);
      return NS_OK;
    }

  private:
    nsRefPtr<ProgressTracker> mProgressTracker;
    nsRefPtr<imgRequestProxy> mProxy;

    // We have to hold on to a reference to the tracker's image, just in case
    // it goes away while we're in the event queue.
    nsRefPtr<Image> mImage;
};

void
ProgressTracker::NotifyCurrentState(imgRequestProxy* proxy)
{
  MOZ_ASSERT(NS_IsMainThread(), "imgRequestProxy is not threadsafe");
#ifdef PR_LOGGING
  nsRefPtr<ImageURL> uri;
  proxy->GetURI(getter_AddRefs(uri));
  nsAutoCString spec;
  uri->GetSpec(spec);
  LOG_FUNC_WITH_PARAM(GetImgLog(), "ProgressTracker::NotifyCurrentState", "uri", spec.get());
#endif

  proxy->SetNotificationsDeferred(true);

  nsCOMPtr<nsIRunnable> ev = new AsyncNotifyCurrentStateRunnable(this, proxy);
  NS_DispatchToCurrentThread(ev);
}

#define NOTIFY_IMAGE_OBSERVERS(PROXIES, FUNC) \
  do { \
    ProxyArray::ForwardIterator iter(PROXIES); \
    while (iter.HasMore()) { \
      nsRefPtr<imgRequestProxy> proxy = iter.GetNext().get(); \
      if (proxy && !proxy->NotificationsDeferred()) { \
        proxy->FUNC; \
      } \
    } \
  } while (false);

/* static */ void
ProgressTracker::SyncNotifyInternal(ProxyArray& aProxies,
                                    bool aHasImage,
                                    Progress aProgress,
                                    const nsIntRect& aDirtyRect)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aProgress & FLAG_SIZE_AVAILABLE)
    NOTIFY_IMAGE_OBSERVERS(aProxies, OnSizeAvailable());

  if (aProgress & FLAG_DECODE_STARTED)
    NOTIFY_IMAGE_OBSERVERS(aProxies, OnStartDecode());

  if (aProgress & FLAG_ONLOAD_BLOCKED)
    NOTIFY_IMAGE_OBSERVERS(aProxies, BlockOnload());

  if (aHasImage) {
    // OnFrameUpdate
    // If there's any content in this frame at all (always true for
    // vector images, true for raster images that have decoded at
    // least one frame) then send OnFrameUpdate.
    if (!aDirtyRect.IsEmpty())
      NOTIFY_IMAGE_OBSERVERS(aProxies, OnFrameUpdate(&aDirtyRect));

    if (aProgress & FLAG_FRAME_COMPLETE)
      NOTIFY_IMAGE_OBSERVERS(aProxies, OnFrameComplete());

    if (aProgress & FLAG_HAS_TRANSPARENCY)
      NOTIFY_IMAGE_OBSERVERS(aProxies, OnImageHasTransparency());

    if (aProgress & FLAG_IS_ANIMATED)
      NOTIFY_IMAGE_OBSERVERS(aProxies, OnImageIsAnimated());
  }

  // Send UnblockOnload before OnStopDecode and OnStopRequest. This allows
  // observers that can fire events when they receive those notifications to do
  // so then, instead of being forced to wait for UnblockOnload.
  if (aProgress & FLAG_ONLOAD_UNBLOCKED) {
    NOTIFY_IMAGE_OBSERVERS(aProxies, UnblockOnload());
  }

  if (aProgress & FLAG_DECODE_COMPLETE) {
    MOZ_ASSERT(aHasImage, "Stopped decoding without ever having an image?");
    NOTIFY_IMAGE_OBSERVERS(aProxies, OnDecodeComplete());
  }

  if (aProgress & FLAG_LOAD_COMPLETE) {
    NOTIFY_IMAGE_OBSERVERS(aProxies,
                           OnLoadComplete(aProgress & FLAG_LAST_PART_COMPLETE));
  }
}

void
ProgressTracker::SyncNotifyProgress(Progress aProgress,
                                    const nsIntRect& aInvalidRect /* = nsIntRect() */)
{
  MOZ_ASSERT(NS_IsMainThread(), "Use mConsumers on main thread only");

  // Don't unblock onload if we're not blocked.
  Progress progress = Difference(aProgress);
  if (!((mProgress | progress) & FLAG_ONLOAD_BLOCKED)) {
    progress &= ~FLAG_ONLOAD_UNBLOCKED;
  }

  // Apply the changes.
  mProgress |= progress;

  CheckProgressConsistency(mProgress);

  // Send notifications.
  SyncNotifyInternal(mConsumers, !!mImage, progress, aInvalidRect);

  if (progress & FLAG_HAS_ERROR) {
    FireFailureNotification();
  }
}

void
ProgressTracker::SyncNotify(imgRequestProxy* proxy)
{
  MOZ_ASSERT(NS_IsMainThread(), "imgRequestProxy is not threadsafe");
#ifdef PR_LOGGING
  nsRefPtr<ImageURL> uri;
  proxy->GetURI(getter_AddRefs(uri));
  nsAutoCString spec;
  uri->GetSpec(spec);
  LOG_SCOPE_WITH_PARAM(GetImgLog(), "ProgressTracker::SyncNotify", "uri", spec.get());
#endif

  nsIntRect r;
  if (mImage) {
    // XXX - Should only send partial rects here, but that needs to
    // wait until we fix up the observer interface
    r = mImage->FrameRect(imgIContainer::FRAME_CURRENT);
  }

  ProxyArray array;
  array.AppendElement(proxy);
  SyncNotifyInternal(array, !!mImage, mProgress, r);
}

void
ProgressTracker::EmulateRequestFinished(imgRequestProxy* aProxy,
                                        nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread(),
             "SyncNotifyState and mConsumers are not threadsafe");
  nsCOMPtr<imgIRequest> kungFuDeathGrip(aProxy);

  if (mProgress & FLAG_ONLOAD_BLOCKED && !(mProgress & FLAG_ONLOAD_UNBLOCKED)) {
    aProxy->UnblockOnload();
  }

  if (!(mProgress & FLAG_LOAD_COMPLETE)) {
    aProxy->OnLoadComplete(true);
  }
}

void
ProgressTracker::AddConsumer(imgRequestProxy* aConsumer)
{
  MOZ_ASSERT(NS_IsMainThread());
  mConsumers.AppendElementUnlessExists(aConsumer);
}

// XXX - The last argument should go away.
bool
ProgressTracker::RemoveConsumer(imgRequestProxy* aConsumer, nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());
  // Remove the proxy from the list.
  bool removed = mConsumers.RemoveElement(aConsumer);

  // Consumers can get confused if they don't get all the proper teardown
  // notifications. Part ways on good terms.
  if (removed && !aConsumer->NotificationsDeferred()) {
    EmulateRequestFinished(aConsumer, aStatus);
  }

  // Make sure we don't give callbacks to a consumer that isn't interested in
  // them any more.
  AsyncNotifyRunnable* runnable =
    static_cast<AsyncNotifyRunnable*>(mRunnable.get());

  if (aConsumer->NotificationsDeferred() && runnable) {
    runnable->RemoveProxy(aConsumer);
    aConsumer->SetNotificationsDeferred(false);
  }

  return removed;
}

bool
ProgressTracker::FirstConsumerIs(imgRequestProxy* aConsumer)
{
  MOZ_ASSERT(NS_IsMainThread(), "Use mConsumers on main thread only");
  ProxyArray::ForwardIterator iter(mConsumers);
  while (iter.HasMore()) {
    nsRefPtr<imgRequestProxy> proxy = iter.GetNext().get();
    if (proxy) {
      return proxy.get() == aConsumer;
    }
  }
  return false;
}

void
ProgressTracker::OnUnlockedDraw()
{
  MOZ_ASSERT(NS_IsMainThread());
  NOTIFY_IMAGE_OBSERVERS(mConsumers, OnUnlockedDraw());
}

void
ProgressTracker::ResetForNewRequest()
{
  MOZ_ASSERT(NS_IsMainThread());

  // We're starting a new load (and if this is called more than once, this is a
  // multipart request) so keep only the bits that carry over between loads.
  mProgress &= FLAG_IS_MULTIPART | FLAG_HAS_ERROR |
               FLAG_ONLOAD_BLOCKED | FLAG_ONLOAD_UNBLOCKED;

  CheckProgressConsistency(mProgress);
}

void
ProgressTracker::OnDiscard()
{
  MOZ_ASSERT(NS_IsMainThread());
  NOTIFY_IMAGE_OBSERVERS(mConsumers, OnDiscard());
}

void
ProgressTracker::OnImageAvailable()
{
  if (!NS_IsMainThread()) {
    // Note: SetHasImage calls Image::Lock and Image::IncrementAnimationCounter
    // so subsequent calls or dispatches which Unlock or Decrement~ should
    // be issued after this to avoid race conditions.
    NS_DispatchToMainThread(
      NS_NewRunnableMethod(this, &ProgressTracker::OnImageAvailable));
    return;
  }

  NOTIFY_IMAGE_OBSERVERS(mConsumers, SetHasImage());
}

void
ProgressTracker::FireFailureNotification()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Some kind of problem has happened with image decoding.
  // Report the URI to net:failed-to-process-uri-conent observers.
  if (mImage) {
    // Should be on main thread, so ok to create a new nsIURI.
    nsCOMPtr<nsIURI> uri;
    {
      nsRefPtr<ImageURL> threadsafeUriData = mImage->GetURI();
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
