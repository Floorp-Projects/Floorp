/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageLogging.h"
#include "imgStatusTracker.h"

#include "imgIContainer.h"
#include "imgRequestProxy.h"
#include "Image.h"
#include "nsNetUtil.h"
#include "nsIObserverService.h"

#include "mozilla/Assertions.h"
#include "mozilla/Services.h"

using namespace mozilla::image;
using mozilla::WeakPtr;

imgStatusTrackerInit::imgStatusTrackerInit(mozilla::image::Image* aImage,
                                           imgStatusTracker* aTracker)
{
  MOZ_ASSERT(aImage);

  if (aTracker) {
    mTracker = aTracker;
    mTracker->SetImage(aImage);
  } else {
    mTracker = new imgStatusTracker(aImage);
  }
  aImage->SetStatusTracker(mTracker);
  MOZ_ASSERT(mTracker);
}

imgStatusTrackerInit::~imgStatusTrackerInit()
{
  mTracker->ResetImage();
}

void
imgStatusTracker::SetImage(Image* aImage)
{
  NS_ABORT_IF_FALSE(aImage, "Setting null image");
  NS_ABORT_IF_FALSE(!mImage, "Setting image when we already have one");
  mImage = aImage;
}

void
imgStatusTracker::ResetImage()
{
  NS_ABORT_IF_FALSE(mImage, "Resetting image when it's already null!");
  mImage = nullptr;
}

void imgStatusTracker::SetIsMultipart()
{
  mState |= FLAG_IS_MULTIPART;

  // If we haven't already blocked onload, make sure we never do.
  if (!(mState & FLAG_ONLOAD_BLOCKED)) {
    mState |= FLAG_ONLOAD_BLOCKED | FLAG_ONLOAD_UNBLOCKED;
  }
}

bool
imgStatusTracker::IsLoading() const
{
  // Checking for whether OnStopRequest has fired allows us to say we're
  // loading before OnStartRequest gets called, letting the request properly
  // get removed from the cache in certain cases.
  return !(mState & FLAG_REQUEST_STOPPED);
}

uint32_t
imgStatusTracker::GetImageStatus() const
{
  uint32_t status = imgIRequest::STATUS_NONE;

  // Translate our current state to a set of imgIRequest::STATE_* flags.
  if (mState & FLAG_HAS_SIZE) {
    status |= imgIRequest::STATUS_SIZE_AVAILABLE;
  }
  if (mState & FLAG_DECODE_STARTED) {
    status |= imgIRequest::STATUS_DECODE_STARTED;
  }
  if (mState & FLAG_DECODE_STOPPED) {
    status |= imgIRequest::STATUS_DECODE_COMPLETE;
  }
  if (mState & FLAG_FRAME_STOPPED) {
    status |= imgIRequest::STATUS_FRAME_COMPLETE;
  }
  if (mState & FLAG_REQUEST_STOPPED) {
    status |= imgIRequest::STATUS_LOAD_COMPLETE;
  }
  if (mState & FLAG_HAS_ERROR) {
    status |= imgIRequest::STATUS_ERROR;
  }

  return status;
}

// A helper class to allow us to call SyncNotify asynchronously.
class imgRequestNotifyRunnable : public nsRunnable
{
  public:
    imgRequestNotifyRunnable(imgStatusTracker* aTracker,
                             imgRequestProxy* aRequestProxy)
      : mTracker(aTracker)
    {
      MOZ_ASSERT(NS_IsMainThread(), "Should be created on the main thread");
      MOZ_ASSERT(aRequestProxy, "aRequestProxy should not be null");
      MOZ_ASSERT(aTracker, "aTracker should not be null");
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

      mTracker->mRequestRunnable = nullptr;
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
    friend class imgStatusTracker;

    nsRefPtr<imgStatusTracker> mTracker;
    nsTArray< nsRefPtr<imgRequestProxy> > mProxies;
};

void
imgStatusTracker::Notify(imgRequestProxy* proxy)
{
  MOZ_ASSERT(NS_IsMainThread(), "imgRequestProxy is not threadsafe");
#ifdef PR_LOGGING
  if (mImage && mImage->GetURI()) {
    nsRefPtr<ImageURL> uri(mImage->GetURI());
    nsAutoCString spec;
    uri->GetSpec(spec);
    LOG_FUNC_WITH_PARAM(GetImgLog(), "imgStatusTracker::Notify async", "uri", spec.get());
  } else {
    LOG_FUNC_WITH_PARAM(GetImgLog(), "imgStatusTracker::Notify async", "uri", "<unknown>");
  }
#endif

  proxy->SetNotificationsDeferred(true);

  // If we have an existing runnable that we can use, we just append this proxy
  // to its list of proxies to be notified. This ensures we don't unnecessarily
  // delay onload.
  imgRequestNotifyRunnable* runnable = static_cast<imgRequestNotifyRunnable*>(mRequestRunnable.get());
  if (runnable) {
    runnable->AddProxy(proxy);
  } else {
    mRequestRunnable = new imgRequestNotifyRunnable(this, proxy);
    NS_DispatchToCurrentThread(mRequestRunnable);
  }
}

// A helper class to allow us to call SyncNotify asynchronously for a given,
// fixed, state.
class imgStatusNotifyRunnable : public nsRunnable
{
  public:
    imgStatusNotifyRunnable(imgStatusTracker* statusTracker,
                            imgRequestProxy* requestproxy)
      : mStatusTracker(statusTracker), mProxy(requestproxy)
    {
      MOZ_ASSERT(NS_IsMainThread(), "Should be created on the main thread");
      MOZ_ASSERT(requestproxy, "requestproxy cannot be null");
      MOZ_ASSERT(statusTracker, "status should not be null");
      mImage = statusTracker->GetImage();
    }

    NS_IMETHOD Run()
    {
      MOZ_ASSERT(NS_IsMainThread(), "Should be running on the main thread");
      mProxy->SetNotificationsDeferred(false);

      mStatusTracker->SyncNotify(mProxy);
      return NS_OK;
    }

  private:
    nsRefPtr<imgStatusTracker> mStatusTracker;
    // We have to hold on to a reference to the tracker's image, just in case
    // it goes away while we're in the event queue.
    nsRefPtr<Image> mImage;
    nsRefPtr<imgRequestProxy> mProxy;
};

void
imgStatusTracker::NotifyCurrentState(imgRequestProxy* proxy)
{
  MOZ_ASSERT(NS_IsMainThread(), "imgRequestProxy is not threadsafe");
#ifdef PR_LOGGING
  nsRefPtr<ImageURL> uri;
  proxy->GetURI(getter_AddRefs(uri));
  nsAutoCString spec;
  uri->GetSpec(spec);
  LOG_FUNC_WITH_PARAM(GetImgLog(), "imgStatusTracker::NotifyCurrentState", "uri", spec.get());
#endif

  proxy->SetNotificationsDeferred(true);

  // We don't keep track of
  nsCOMPtr<nsIRunnable> ev = new imgStatusNotifyRunnable(this, proxy);
  NS_DispatchToCurrentThread(ev);
}

#define NOTIFY_IMAGE_OBSERVERS(func) \
  do { \
    ProxyArray::ForwardIterator iter(aProxies); \
    while (iter.HasMore()) { \
      nsRefPtr<imgRequestProxy> proxy = iter.GetNext().get(); \
      if (proxy && !proxy->NotificationsDeferred()) { \
        proxy->func; \
      } \
    } \
  } while (false);

/* static */ void
imgStatusTracker::SyncNotifyState(ProxyArray& aProxies,
                                  bool aHasImage, uint32_t aState,
                                  const nsIntRect& aDirtyRect)
{
  MOZ_ASSERT(NS_IsMainThread());
  // OnStartRequest
  if (aState & FLAG_REQUEST_STARTED)
    NOTIFY_IMAGE_OBSERVERS(OnStartRequest());

  // OnStartContainer
  if (aState & FLAG_HAS_SIZE)
    NOTIFY_IMAGE_OBSERVERS(OnStartContainer());

  // OnStartDecode
  if (aState & FLAG_DECODE_STARTED)
    NOTIFY_IMAGE_OBSERVERS(OnStartDecode());

  // BlockOnload
  if (aState & FLAG_ONLOAD_BLOCKED)
    NOTIFY_IMAGE_OBSERVERS(BlockOnload());

  if (aHasImage) {
    // OnFrameUpdate
    // If there's any content in this frame at all (always true for
    // vector images, true for raster images that have decoded at
    // least one frame) then send OnFrameUpdate.
    if (!aDirtyRect.IsEmpty())
      NOTIFY_IMAGE_OBSERVERS(OnFrameUpdate(&aDirtyRect));

    if (aState & FLAG_FRAME_STOPPED)
      NOTIFY_IMAGE_OBSERVERS(OnStopFrame());

    // OnImageIsAnimated
    if (aState & FLAG_IS_ANIMATED)
      NOTIFY_IMAGE_OBSERVERS(OnImageIsAnimated());
  }

  // Send UnblockOnload before OnStopDecode and OnStopRequest. This allows
  // observers that can fire events when they receive those notifications to do
  // so then, instead of being forced to wait for UnblockOnload.
  if (aState & FLAG_ONLOAD_UNBLOCKED) {
    NOTIFY_IMAGE_OBSERVERS(UnblockOnload());
  }

  if (aState & FLAG_DECODE_STOPPED) {
    MOZ_ASSERT(aHasImage, "Stopped decoding without ever having an image?");
    NOTIFY_IMAGE_OBSERVERS(OnStopDecode());
  }

  if (aState & FLAG_REQUEST_STOPPED) {
    NOTIFY_IMAGE_OBSERVERS(OnStopRequest(aState & FLAG_MULTIPART_STOPPED));
  }
}

ImageStatusDiff
imgStatusTracker::Difference(const ImageStatusDiff& aOther) const
{
  ImageStatusDiff diff;
  diff.diffState = ~mState & aOther.diffState;
  return diff;
}

void
imgStatusTracker::SyncNotifyDifference(const ImageStatusDiff& aDiff,
                                       const nsIntRect& aInvalidRect /* = nsIntRect() */)
{
  MOZ_ASSERT(NS_IsMainThread(), "Use mConsumers on main thread only");
  LOG_SCOPE(GetImgLog(), "imgStatusTracker::SyncNotifyDifference");

  // Don't unblock onload if we're not blocked.
  ImageStatusDiff diff = Difference(aDiff);
  if (!((mState | diff.diffState) & FLAG_ONLOAD_BLOCKED)) {
    diff.diffState &= ~FLAG_ONLOAD_UNBLOCKED;
  }

  // Apply the changes.
  mState |= diff.diffState;

  // Send notifications.
  SyncNotifyState(mConsumers, !!mImage, diff.diffState, aInvalidRect);

  if (diff.diffState & FLAG_HAS_ERROR) {
    FireFailureNotification();
  }
}

void
imgStatusTracker::SyncNotify(imgRequestProxy* proxy)
{
  MOZ_ASSERT(NS_IsMainThread(), "imgRequestProxy is not threadsafe");
#ifdef PR_LOGGING
  nsRefPtr<ImageURL> uri;
  proxy->GetURI(getter_AddRefs(uri));
  nsAutoCString spec;
  uri->GetSpec(spec);
  LOG_SCOPE_WITH_PARAM(GetImgLog(), "imgStatusTracker::SyncNotify", "uri", spec.get());
#endif

  nsIntRect r;
  if (mImage) {
    // XXX - Should only send partial rects here, but that needs to
    // wait until we fix up the observer interface
    r = mImage->FrameRect(imgIContainer::FRAME_CURRENT);
  }

  ProxyArray array;
  array.AppendElement(proxy);
  SyncNotifyState(array, !!mImage, mState, r);
}

void
imgStatusTracker::EmulateRequestFinished(imgRequestProxy* aProxy,
                                         nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread(),
             "SyncNotifyState and mConsumers are not threadsafe");
  nsCOMPtr<imgIRequest> kungFuDeathGrip(aProxy);

  // In certain cases the request might not have started yet.
  // We still need to fulfill the contract.
  if (!(mState & FLAG_REQUEST_STARTED)) {
    aProxy->OnStartRequest();
  }

  if (mState & FLAG_ONLOAD_BLOCKED && !(mState & FLAG_ONLOAD_UNBLOCKED)) {
    aProxy->UnblockOnload();
  }

  if (!(mState & FLAG_REQUEST_STOPPED)) {
    aProxy->OnStopRequest(true);
  }
}

void
imgStatusTracker::AddConsumer(imgRequestProxy* aConsumer)
{
  MOZ_ASSERT(NS_IsMainThread());
  mConsumers.AppendElementUnlessExists(aConsumer);
}

// XXX - The last argument should go away.
bool
imgStatusTracker::RemoveConsumer(imgRequestProxy* aConsumer, nsresult aStatus)
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
  imgRequestNotifyRunnable* runnable = static_cast<imgRequestNotifyRunnable*>(mRequestRunnable.get());
  if (aConsumer->NotificationsDeferred() && runnable) {
    runnable->RemoveProxy(aConsumer);
    aConsumer->SetNotificationsDeferred(false);
  }

  return removed;
}

bool
imgStatusTracker::FirstConsumerIs(imgRequestProxy* aConsumer)
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
imgStatusTracker::RecordError()
{
  mState |= FLAG_HAS_ERROR;
}

void
imgStatusTracker::SendStartDecode(imgRequestProxy* aProxy)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aProxy->NotificationsDeferred())
    aProxy->OnStartDecode();
}

void
imgStatusTracker::SendStartContainer(imgRequestProxy* aProxy)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aProxy->NotificationsDeferred())
    aProxy->OnStartContainer();
}

void
imgStatusTracker::SendStopFrame(imgRequestProxy* aProxy)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aProxy->NotificationsDeferred())
    aProxy->OnStopFrame();
}

void
imgStatusTracker::SendStopDecode(imgRequestProxy* aProxy,
                                 nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aProxy->NotificationsDeferred())
    aProxy->OnStopDecode();
}

void
imgStatusTracker::SendDiscard(imgRequestProxy* aProxy)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aProxy->NotificationsDeferred())
    aProxy->OnDiscard();
}


void
imgStatusTracker::SendImageIsAnimated(imgRequestProxy* aProxy)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aProxy->NotificationsDeferred())
    aProxy->OnImageIsAnimated();
}

void
imgStatusTracker::SendUnlockedDraw(imgRequestProxy* aProxy)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aProxy->NotificationsDeferred())
    aProxy->OnUnlockedDraw();
}

void
imgStatusTracker::OnUnlockedDraw()
{
  MOZ_ASSERT(NS_IsMainThread());
  ProxyArray::ForwardIterator iter(mConsumers);
  while (iter.HasMore()) {
    nsRefPtr<imgRequestProxy> proxy = iter.GetNext().get();
    if (proxy) {
      SendUnlockedDraw(proxy);
    }
  }
}

/* non-virtual sort-of-nsIRequestObserver methods */
void
imgStatusTracker::SendStartRequest(imgRequestProxy* aProxy)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aProxy->NotificationsDeferred())
    aProxy->OnStartRequest();
}

void
imgStatusTracker::OnStartRequest()
{
  MOZ_ASSERT(NS_IsMainThread());

  // We're starting a new load, so clear any status and state bits indicating
  // load/decode.
  // XXX(seth): Are these really the only flags we want to clear?
  mState &= ~FLAG_REQUEST_STARTED;
  mState &= ~FLAG_DECODE_STARTED;
  mState &= ~FLAG_DECODE_STOPPED;
  mState &= ~FLAG_REQUEST_STOPPED;
  mState &= ~FLAG_ONLOAD_BLOCKED;
  mState &= ~FLAG_ONLOAD_UNBLOCKED;
  mState &= ~FLAG_IS_ANIMATED;

  mState |= FLAG_REQUEST_STARTED;

  ProxyArray::ForwardIterator iter(mConsumers);
  while (iter.HasMore()) {
    nsRefPtr<imgRequestProxy> proxy = iter.GetNext().get();
    if (proxy) {
      SendStartRequest(proxy);
    }
  }
}

void
imgStatusTracker::SendStopRequest(imgRequestProxy* aProxy,
                                  bool aLastPart,
                                  nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aProxy->NotificationsDeferred()) {
    aProxy->OnStopRequest(aLastPart);
  }
}

void
imgStatusTracker::OnDiscard()
{
  MOZ_ASSERT(NS_IsMainThread());

  /* notify the kids */
  ProxyArray::ForwardIterator iter(mConsumers);
  while (iter.HasMore()) {
    nsRefPtr<imgRequestProxy> proxy = iter.GetNext().get();
    if (proxy) {
      SendDiscard(proxy);
    }
  }
}

void
imgStatusTracker::OnDataAvailable()
{
  if (!NS_IsMainThread()) {
    // Note: SetHasImage calls Image::Lock and Image::IncrementAnimationCounter
    // so subsequent calls or dispatches which Unlock or Decrement~ should
    // be issued after this to avoid race conditions.
    NS_DispatchToMainThread(
      NS_NewRunnableMethod(this, &imgStatusTracker::OnDataAvailable));
    return;
  }
  // Notify any imgRequestProxys that are observing us that we have an Image.
  ProxyArray::ForwardIterator iter(mConsumers);
  while (iter.HasMore()) {
    nsRefPtr<imgRequestProxy> proxy = iter.GetNext().get();
    if (proxy) {
      proxy->SetHasImage();
    }
  }
}

void
imgStatusTracker::SendBlockOnload(imgRequestProxy* aProxy)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aProxy->NotificationsDeferred()) {
    aProxy->BlockOnload();
  }
}

void
imgStatusTracker::SendUnblockOnload(imgRequestProxy* aProxy)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aProxy->NotificationsDeferred()) {
    aProxy->UnblockOnload();
  }
}

void
imgStatusTracker::MaybeUnblockOnload()
{
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(
      NS_NewRunnableMethod(this, &imgStatusTracker::MaybeUnblockOnload));
    return;
  }
  if (!(mState & FLAG_ONLOAD_BLOCKED) || (mState & FLAG_ONLOAD_UNBLOCKED)) {
    return;
  }

  mState |= FLAG_ONLOAD_UNBLOCKED;

  ProxyArray::ForwardIterator iter(mConsumers);
  while (iter.HasMore()) {
    nsRefPtr<imgRequestProxy> proxy = iter.GetNext().get();
    if (proxy) {
      SendUnblockOnload(proxy);
    }
  }
}

void
imgStatusTracker::FireFailureNotification()
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
