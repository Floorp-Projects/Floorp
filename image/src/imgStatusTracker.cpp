/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgStatusTracker.h"

#include "imgIContainer.h"
#include "imgRequestProxy.h"
#include "imgDecoderObserver.h"
#include "Image.h"
#include "ImageLogging.h"
#include "nsNetUtil.h"
#include "nsIObserverService.h"

#include "mozilla/Assertions.h"
#include "mozilla/Services.h"

using namespace mozilla::image;

class imgStatusTrackerObserver : public imgDecoderObserver
{
public:
  imgStatusTrackerObserver(imgStatusTracker* aTracker)
  : mTracker(aTracker->asWeakPtr())
  {
    MOZ_ASSERT(aTracker);
  }

  virtual ~imgStatusTrackerObserver() {}

  void SetTracker(imgStatusTracker* aTracker)
  {
    MOZ_ASSERT(aTracker);
    mTracker = aTracker->asWeakPtr();
  }

  /** imgDecoderObserver methods **/

  virtual void OnStartDecode() MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnStartDecode");
    nsRefPtr<imgStatusTracker> tracker = mTracker.get();
    if (!tracker) { return; }
    tracker->RecordStartDecode();
    if (!tracker->IsMultipart()) {
      tracker->RecordBlockOnload();
    }
  }

  virtual void OnStartRequest() MOZ_OVERRIDE
  {
    NS_NOTREACHED("imgStatusTrackerObserver(imgDecoderObserver)::OnStartRequest");
  }

  virtual void OnStartContainer() MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnStartContainer");
    nsRefPtr<imgStatusTracker> tracker = mTracker.get();
    if (!tracker) { return; }
    nsRefPtr<Image> image = tracker->GetImage();;
    tracker->RecordStartContainer(image);
  }

  virtual void OnStartFrame() MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnStartFrame");
    nsRefPtr<imgStatusTracker> tracker = mTracker.get();
    if (!tracker) { return; }
    tracker->RecordStartFrame();
  }

  virtual void FrameChanged(const nsIntRect* dirtyRect) MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::FrameChanged");
    nsRefPtr<imgStatusTracker> tracker = mTracker.get();
    if (!tracker) { return; }
    tracker->RecordFrameChanged(dirtyRect);
  }

  virtual void OnStopFrame() MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnStopFrame");
    nsRefPtr<imgStatusTracker> tracker = mTracker.get();
    if (!tracker) { return; }
    tracker->RecordStopFrame();
    tracker->RecordUnblockOnload();
  }

  virtual void OnStopDecode(nsresult aStatus) MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnStopDecode");
    nsRefPtr<imgStatusTracker> tracker = mTracker.get();
    if (!tracker) { return; }
    tracker->RecordStopDecode(aStatus);

    // This is really hacky. We need to handle the case where we start decoding,
    // block onload, but then hit an error before we get to our first frame.
    tracker->RecordUnblockOnload();
  }

  virtual void OnStopRequest(bool aLastPart, nsresult aStatus) MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnStopRequest");
    nsRefPtr<imgStatusTracker> tracker = mTracker.get();
    if (!tracker) { return; }
    tracker->RecordStopRequest(aLastPart, aStatus);
  }

  virtual void OnDiscard() MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnDiscard");
    nsRefPtr<imgStatusTracker> tracker = mTracker.get();
    if (!tracker) { return; }
    tracker->RecordDiscard();
  }

  virtual void OnUnlockedDraw() MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnUnlockedDraw");
    nsRefPtr<imgStatusTracker> tracker = mTracker.get();
    if (!tracker) { return; }
    NS_ABORT_IF_FALSE(tracker->HasImage(),
                      "OnUnlockedDraw callback before we've created our image");
    tracker->RecordUnlockedDraw();
  }

  virtual void OnImageIsAnimated() MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnImageIsAnimated");
    nsRefPtr<imgStatusTracker> tracker = mTracker.get();
    if (!tracker) { return; }
    tracker->RecordImageIsAnimated();
  }

  virtual void OnError() MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnError");
    nsRefPtr<imgStatusTracker> tracker = mTracker.get();
    if (!tracker) { return; }
    tracker->RecordError();
  }

private:
  mozilla::WeakPtr<imgStatusTracker> mTracker;
};

// imgStatusTracker methods

imgStatusTracker::imgStatusTracker(Image* aImage)
  : mImage(aImage),
    mState(0),
    mImageStatus(imgIRequest::STATUS_NONE),
    mIsMultipart(false),
    mHadLastPart(false),
    mHasBeenDecoded(false)
{
  mTrackerObserver = new imgStatusTrackerObserver(this);
}

// Private, used only by CloneForRecording.
imgStatusTracker::imgStatusTracker(const imgStatusTracker& aOther)
  : mImage(aOther.mImage),
    mState(aOther.mState),
    mImageStatus(aOther.mImageStatus),
    mIsMultipart(aOther.mIsMultipart),
    mHadLastPart(aOther.mHadLastPart),
    mHasBeenDecoded(aOther.mHasBeenDecoded)
    // Note: we explicitly don't copy several fields:
    //  - mRequestRunnable, because it won't be nulled out when the
    //    mRequestRunnable's Run function eventually gets called.
    //  - mProperties, because we don't need it and it'd just point at the same
    //    object
    //  - mConsumers, because we don't need to talk to consumers
    //  - mInvalidRect, because the point of it is to be fired off and reset
{
  mTrackerObserver = new imgStatusTrackerObserver(this);
}

imgStatusTracker::~imgStatusTracker()
{}

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

bool
imgStatusTracker::IsLoading() const
{
  // Checking for whether OnStopRequest has fired allows us to say we're
  // loading before OnStartRequest gets called, letting the request properly
  // get removed from the cache in certain cases.
  return !(mState & stateRequestStopped);
}

uint32_t
imgStatusTracker::GetImageStatus() const
{
  return mImageStatus;
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
    nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(proxies); \
    while (iter.HasMore()) { \
      nsRefPtr<imgRequestProxy> proxy = iter.GetNext(); \
      if (!proxy->NotificationsDeferred()) { \
        proxy->func; \
      } \
    } \
  } while (false);

/* static */ void
imgStatusTracker::SyncNotifyState(nsTObserverArray<imgRequestProxy*>& proxies,
                                  bool hasImage, uint32_t state,
                                  nsIntRect& dirtyRect, bool hadLastPart)
{
  MOZ_ASSERT(NS_IsMainThread());
  // OnStartRequest
  if (state & stateRequestStarted)
    NOTIFY_IMAGE_OBSERVERS(OnStartRequest());

  // OnStartContainer
  if (state & stateHasSize)
    NOTIFY_IMAGE_OBSERVERS(OnStartContainer());

  // OnStartDecode
  if (state & stateDecodeStarted)
    NOTIFY_IMAGE_OBSERVERS(OnStartDecode());

  // BlockOnload
  if (state & stateBlockingOnload)
    NOTIFY_IMAGE_OBSERVERS(BlockOnload());

  if (hasImage) {
    // OnFrameUpdate
    // If there's any content in this frame at all (always true for
    // vector images, true for raster images that have decoded at
    // least one frame) then send OnFrameUpdate.
    if (!dirtyRect.IsEmpty())
      NOTIFY_IMAGE_OBSERVERS(OnFrameUpdate(&dirtyRect));

    if (state & stateFrameStopped)
      NOTIFY_IMAGE_OBSERVERS(OnStopFrame());

    // OnImageIsAnimated
    if (state & stateImageIsAnimated)
      NOTIFY_IMAGE_OBSERVERS(OnImageIsAnimated());
  }

  if (state & stateDecodeStopped) {
    NS_ABORT_IF_FALSE(hasImage, "stopped decoding without ever having an image?");
    NOTIFY_IMAGE_OBSERVERS(OnStopDecode());
  }

  if (state & stateRequestStopped) {
    NOTIFY_IMAGE_OBSERVERS(OnStopRequest(hadLastPart));
  }
}

ImageStatusDiff
imgStatusTracker::Difference(imgStatusTracker* aOther) const
{
  MOZ_ASSERT(aOther, "aOther cannot be null");
  ImageStatusDiff diff;
  diff.diffState = ~mState & aOther->mState & ~stateRequestStarted;
  diff.diffImageStatus = ~mImageStatus & aOther->mImageStatus;
  diff.unblockedOnload = mState & stateBlockingOnload && !(aOther->mState & stateBlockingOnload);
  diff.unsetDecodeStarted = mImageStatus & imgIRequest::STATUS_DECODE_STARTED
                         && !(aOther->mImageStatus & imgIRequest::STATUS_DECODE_STARTED);
  diff.foundError = (mImageStatus != imgIRequest::STATUS_ERROR)
                 && (aOther->mImageStatus == imgIRequest::STATUS_ERROR);

  MOZ_ASSERT(!mIsMultipart || aOther->mIsMultipart, "mIsMultipart should be monotonic");
  diff.foundIsMultipart = !mIsMultipart && aOther->mIsMultipart;
  diff.foundLastPart = !mHadLastPart && aOther->mHadLastPart;

  diff.gotDecoded = !mHasBeenDecoded && aOther->mHasBeenDecoded;

  // Only record partial invalidations if we haven't been decoded before.
  // When images are re-decoded after discarding, we don't want to display
  // partially decoded versions to the user.
  const uint32_t combinedStatus = mImageStatus | aOther->mImageStatus;
  const bool doInvalidations  = !(mHasBeenDecoded || aOther->mHasBeenDecoded)
                             || combinedStatus & imgIRequest::STATUS_ERROR
                             || combinedStatus & imgIRequest::STATUS_DECODE_COMPLETE;

  // Record and reset the invalid rectangle.
  // XXX(seth): We shouldn't be resetting anything here; see bug 910441.
  if (doInvalidations) {
    diff.invalidRect = aOther->mInvalidRect;
    aOther->mInvalidRect.SetEmpty();
  }

  return diff;
}

ImageStatusDiff
imgStatusTracker::DecodeStateAsDifference() const
{
  ImageStatusDiff diff;
  diff.diffState = mState & ~stateRequestStarted;

  // All other ImageStatusDiff fields are intentionally left at their default
  // values; we only want to notify decode state changes.

  return diff;
}

void
imgStatusTracker::ApplyDifference(const ImageStatusDiff& aDiff)
{
  LOG_SCOPE(GetImgLog(), "imgStatusTracker::ApplyDifference");

  // We must not modify or notify for the start-load state, which happens from Necko callbacks.
  uint32_t loadState = mState & stateRequestStarted;

  // Synchronize our state.
  mState |= aDiff.diffState | loadState;
  if (aDiff.unblockedOnload)
    mState &= ~stateBlockingOnload;

  mIsMultipart = mIsMultipart || aDiff.foundIsMultipart;
  mHadLastPart = mHadLastPart || aDiff.foundLastPart;
  mHasBeenDecoded = mHasBeenDecoded || aDiff.gotDecoded;

  // Update the image status. There are some subtle points which are handled below.
  mImageStatus |= aDiff.diffImageStatus;

  // Unset bits which can get unset as part of the decoding process.
  if (aDiff.unsetDecodeStarted)
    mImageStatus &= ~imgIRequest::STATUS_DECODE_STARTED;

  // The error state is sticky and overrides all other bits.
  if (mImageStatus & imgIRequest::STATUS_ERROR)
    mImageStatus = imgIRequest::STATUS_ERROR;
}

void
imgStatusTracker::SyncNotifyDifference(const ImageStatusDiff& diff)
{
  MOZ_ASSERT(NS_IsMainThread(), "Use mConsumers on main thread only");
  LOG_SCOPE(GetImgLog(), "imgStatusTracker::SyncNotifyDifference");

  nsIntRect invalidRect = mInvalidRect.Union(diff.invalidRect);

  SyncNotifyState(mConsumers, !!mImage, diff.diffState, invalidRect, mHadLastPart);

  mInvalidRect.SetEmpty();

  if (diff.unblockedOnload) {
    nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mConsumers);
    while (iter.HasMore()) {
      // Hold on to a reference to this proxy, since notifying the state can
      // cause it to disappear.
      nsRefPtr<imgRequestProxy> proxy = iter.GetNext();

      if (!proxy->NotificationsDeferred()) {
        SendUnblockOnload(proxy);
      }
    }
  }

  if (diff.foundError) {
    FireFailureNotification();
  }
}

already_AddRefed<imgStatusTracker>
imgStatusTracker::CloneForRecording()
{
  // Grab a ref to this to ensure it isn't deleted.
  nsRefPtr<imgStatusTracker> thisStatusTracker = this;
  nsRefPtr<imgStatusTracker> clone = new imgStatusTracker(*thisStatusTracker);
  return clone.forget();
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

  nsTObserverArray<imgRequestProxy*> array;
  array.AppendElement(proxy);
  SyncNotifyState(array, !!mImage, mState, r, mHadLastPart);
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
  if (!(mState & stateRequestStarted)) {
    aProxy->OnStartRequest();
  }

  if (mState & stateBlockingOnload) {
    aProxy->UnblockOnload();
  }

  if (!(mState & stateRequestStopped)) {
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

void
imgStatusTracker::RecordCancel()
{
  if (!(mImageStatus & imgIRequest::STATUS_LOAD_PARTIAL))
    mImageStatus = imgIRequest::STATUS_ERROR;
}

void
imgStatusTracker::RecordLoaded()
{
  NS_ABORT_IF_FALSE(mImage, "RecordLoaded called before we have an Image");
  mState |= stateRequestStarted | stateHasSize | stateRequestStopped;
  mImageStatus |= imgIRequest::STATUS_SIZE_AVAILABLE | imgIRequest::STATUS_LOAD_COMPLETE;
  mHadLastPart = true;
}

void
imgStatusTracker::RecordDecoded()
{
  NS_ABORT_IF_FALSE(mImage, "RecordDecoded called before we have an Image");
  mState |= stateDecodeStarted | stateDecodeStopped | stateFrameStopped;
  mImageStatus |= imgIRequest::STATUS_FRAME_COMPLETE | imgIRequest::STATUS_DECODE_COMPLETE;
  mImageStatus &= ~imgIRequest::STATUS_DECODE_STARTED;
}

void
imgStatusTracker::RecordStartDecode()
{
  NS_ABORT_IF_FALSE(mImage, "RecordStartDecode without an Image");
  mState |= stateDecodeStarted;
  mImageStatus |= imgIRequest::STATUS_DECODE_STARTED;
}

void
imgStatusTracker::SendStartDecode(imgRequestProxy* aProxy)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aProxy->NotificationsDeferred())
    aProxy->OnStartDecode();
}

void
imgStatusTracker::RecordStartContainer(imgIContainer* aContainer)
{
  NS_ABORT_IF_FALSE(mImage,
                    "RecordStartContainer called before we have an Image");
  NS_ABORT_IF_FALSE(mImage == aContainer,
                    "RecordStartContainer called with wrong Image");
  mState |= stateHasSize;
  mImageStatus |= imgIRequest::STATUS_SIZE_AVAILABLE;
}

void
imgStatusTracker::SendStartContainer(imgRequestProxy* aProxy)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aProxy->NotificationsDeferred())
    aProxy->OnStartContainer();
}

void
imgStatusTracker::RecordStartFrame()
{
  mInvalidRect.SetEmpty();
}

// No SendStartFrame since it's not observed below us.

void
imgStatusTracker::RecordStopFrame()
{
  NS_ABORT_IF_FALSE(mImage, "RecordStopFrame called before we have an Image");
  mState |= stateFrameStopped;
  mImageStatus |= imgIRequest::STATUS_FRAME_COMPLETE;
}

void
imgStatusTracker::SendStopFrame(imgRequestProxy* aProxy)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aProxy->NotificationsDeferred())
    aProxy->OnStopFrame();
}

void
imgStatusTracker::RecordStopDecode(nsresult aStatus)
{
  NS_ABORT_IF_FALSE(mImage,
                    "RecordStopDecode called before we have an Image");
  mState |= stateDecodeStopped;

  if (NS_SUCCEEDED(aStatus) && mImageStatus != imgIRequest::STATUS_ERROR) {
    mImageStatus |= imgIRequest::STATUS_DECODE_COMPLETE;
    mImageStatus &= ~imgIRequest::STATUS_DECODE_STARTED;
    mHasBeenDecoded = true;
  // If we weren't successful, clear all success status bits and set error.
  } else {
    mImageStatus = imgIRequest::STATUS_ERROR;
  }
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
imgStatusTracker::RecordDiscard()
{
  NS_ABORT_IF_FALSE(mImage,
                    "RecordDiscard called before we have an Image");
  // Clear the state bits we no longer deserve.
  uint32_t stateBitsToClear = stateDecodeStopped;
  mState &= ~stateBitsToClear;

  // Clear the status bits we no longer deserve.
  uint32_t statusBitsToClear = imgIRequest::STATUS_DECODE_STARTED |
                               imgIRequest::STATUS_FRAME_COMPLETE |
                               imgIRequest::STATUS_DECODE_COMPLETE;
  mImageStatus &= ~statusBitsToClear;
}

void
imgStatusTracker::SendDiscard(imgRequestProxy* aProxy)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aProxy->NotificationsDeferred())
    aProxy->OnDiscard();
}


void
imgStatusTracker::RecordUnlockedDraw()
{
  NS_ABORT_IF_FALSE(mImage,
                    "RecordUnlockedDraw called before we have an Image");
}

void
imgStatusTracker::RecordImageIsAnimated()
{
  NS_ABORT_IF_FALSE(mImage,
                    "RecordImageIsAnimated called before we have an Image");
  mState |= stateImageIsAnimated;
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
  RecordUnlockedDraw();
  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mConsumers);
  while (iter.HasMore()) {
    SendUnlockedDraw(iter.GetNext());
  }
}

void
imgStatusTracker::RecordFrameChanged(const nsIntRect* aDirtyRect)
{
  NS_ABORT_IF_FALSE(mImage,
                    "RecordFrameChanged called before we have an Image");
  mInvalidRect = mInvalidRect.Union(*aDirtyRect);
}

void
imgStatusTracker::SendFrameChanged(imgRequestProxy* aProxy,
                                   const nsIntRect* aDirtyRect)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!aProxy->NotificationsDeferred())
    aProxy->OnFrameUpdate(aDirtyRect);
}

/* non-virtual sort-of-nsIRequestObserver methods */
void
imgStatusTracker::RecordStartRequest()
{
  // We're starting a new load, so clear any status and state bits indicating
  // load/decode
  mImageStatus &= ~imgIRequest::STATUS_LOAD_PARTIAL;
  mImageStatus &= ~imgIRequest::STATUS_LOAD_COMPLETE;
  mImageStatus &= ~imgIRequest::STATUS_FRAME_COMPLETE;
  mImageStatus &= ~imgIRequest::STATUS_DECODE_STARTED;
  mImageStatus &= ~imgIRequest::STATUS_DECODE_COMPLETE;
  mState &= ~stateRequestStarted;
  mState &= ~stateDecodeStarted;
  mState &= ~stateDecodeStopped;
  mState &= ~stateRequestStopped;
  mState &= ~stateBlockingOnload;
  mState &= ~stateImageIsAnimated;

  mState |= stateRequestStarted;
}

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
  RecordStartRequest();
  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mConsumers);
  while (iter.HasMore()) {
    SendStartRequest(iter.GetNext());
  }
}

void
imgStatusTracker::RecordStopRequest(bool aLastPart,
                                    nsresult aStatus)
{
  mHadLastPart = aLastPart;
  mState |= stateRequestStopped;

  // If we were successful in loading, note that the image is complete.
  if (NS_SUCCEEDED(aStatus) && mImageStatus != imgIRequest::STATUS_ERROR)
    mImageStatus |= imgIRequest::STATUS_LOAD_COMPLETE;
  else
    mImageStatus = imgIRequest::STATUS_ERROR;
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

class OnStopRequestEvent : public nsRunnable
{
public:
  OnStopRequestEvent(imgStatusTracker* aTracker,
                     bool aLastPart,
                     nsresult aStatus)
    : mTracker(aTracker)
    , mLastPart(aLastPart)
    , mStatus(aStatus)
  {
    MOZ_ASSERT(!NS_IsMainThread(), "Should be created off the main thread");
    MOZ_ASSERT(aTracker, "aTracker should not be null");
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread(), "Should be running on the main thread");
    MOZ_ASSERT(mTracker, "mTracker should not be null");
    mTracker->OnStopRequest(mLastPart, mStatus);
    return NS_OK;
  }
private:
  nsRefPtr<imgStatusTracker> mTracker;
  bool mLastPart;
  nsresult mStatus;
};

void
imgStatusTracker::OnStopRequest(bool aLastPart,
                                nsresult aStatus)
{
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(
      new OnStopRequestEvent(this, aLastPart, aStatus));
    return;
  }
  bool preexistingError = mImageStatus == imgIRequest::STATUS_ERROR;

  RecordStopRequest(aLastPart, aStatus);
  /* notify the kids */
  nsTObserverArray<imgRequestProxy*>::ForwardIterator srIter(mConsumers);
  while (srIter.HasMore()) {
    SendStopRequest(srIter.GetNext(), aLastPart, aStatus);
  }

  if (NS_FAILED(aStatus) && !preexistingError) {
    FireFailureNotification();
  }
}

void
imgStatusTracker::OnDiscard()
{
  MOZ_ASSERT(NS_IsMainThread());
  RecordDiscard();

  /* notify the kids */
  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mConsumers);
  while (iter.HasMore()) {
    SendDiscard(iter.GetNext());
  }
}

void
imgStatusTracker::FrameChanged(const nsIntRect* aDirtyRect)
{
  MOZ_ASSERT(NS_IsMainThread());
  RecordFrameChanged(aDirtyRect);

  /* notify the kids */
  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mConsumers);
  while (iter.HasMore()) {
    SendFrameChanged(iter.GetNext(), aDirtyRect);
  }
}

void
imgStatusTracker::OnStopFrame()
{
  MOZ_ASSERT(NS_IsMainThread());
  RecordStopFrame();

  /* notify the kids */
  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mConsumers);
  while (iter.HasMore()) {
    SendStopFrame(iter.GetNext());
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
  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mConsumers);
  while (iter.HasMore()) {
    iter.GetNext()->SetHasImage();
  }
}

void
imgStatusTracker::RecordBlockOnload()
{
  MOZ_ASSERT(!(mState & stateBlockingOnload));
  mState |= stateBlockingOnload;
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
imgStatusTracker::RecordUnblockOnload()
{
  mState &= ~stateBlockingOnload;
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
  if (!(mState & stateBlockingOnload)) {
    return;
  }

  RecordUnblockOnload();

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mConsumers);
  while (iter.HasMore()) {
    SendUnblockOnload(iter.GetNext());
  }
}

void
imgStatusTracker::RecordError()
{
  mImageStatus = imgIRequest::STATUS_ERROR;
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
