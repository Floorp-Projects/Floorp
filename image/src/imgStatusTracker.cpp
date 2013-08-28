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
#include "RasterImage.h"
#include "nsIObserverService.h"
#include "RasterImage.h"

#include "mozilla/Util.h"
#include "mozilla/Assertions.h"
#include "mozilla/Services.h"

using namespace mozilla::image;

class imgStatusTrackerNotifyingObserver : public imgDecoderObserver
{
public:
  imgStatusTrackerNotifyingObserver(imgStatusTracker* aTracker)
  : mTracker(aTracker) {}

  virtual ~imgStatusTrackerNotifyingObserver() {}

  void SetTracker(imgStatusTracker* aTracker) {
    mTracker = aTracker;
  }

  /** imgDecoderObserver methods **/

  virtual void OnStartDecode()
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerNotifyingObserver::OnStartDecode");
    NS_ABORT_IF_FALSE(mTracker->GetImage(),
                      "OnStartDecode callback before we've created our image");

    mTracker->RecordStartDecode();

    nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
    while (iter.HasMore()) {
      mTracker->SendStartDecode(iter.GetNext());
    }

    if (!mTracker->IsMultipart()) {
      mTracker->RecordBlockOnload();

      nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
      while (iter.HasMore()) {
        mTracker->SendBlockOnload(iter.GetNext());
      }
    }
  }

  virtual void OnStartRequest()
  {
    NS_NOTREACHED("imgStatusTrackerNotifyingObserver(imgDecoderObserver)::OnStartRequest");
  }

  virtual void OnStartContainer()
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerNotifyingObserver::OnStartContainer");

    NS_ABORT_IF_FALSE(mTracker->GetImage(),
                      "OnStartContainer callback before we've created our image");
    mTracker->RecordStartContainer(mTracker->GetImage());

    nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
    while (iter.HasMore()) {
      mTracker->SendStartContainer(iter.GetNext());
    }
  }

  virtual void OnStartFrame()
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerNotifyingObserver::OnStartFrame");
    NS_ABORT_IF_FALSE(mTracker->GetImage(),
                      "OnStartFrame callback before we've created our image");

    mTracker->RecordStartFrame();

    // This is not observed below the imgStatusTracker level, so we don't need
    // to SendStartFrame.
  }

  virtual void FrameChanged(const nsIntRect* dirtyRect)
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerNotifyingObserver::FrameChanged");
    NS_ABORT_IF_FALSE(mTracker->GetImage(),
                      "FrameChanged callback before we've created our image");

    mTracker->RecordFrameChanged(dirtyRect);

    nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
    while (iter.HasMore()) {
      mTracker->SendFrameChanged(iter.GetNext(), dirtyRect);
    }
  }

  virtual void OnStopFrame()
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerNotifyingObserver::OnStopFrame");
    NS_ABORT_IF_FALSE(mTracker->GetImage(),
                      "OnStopFrame callback before we've created our image");

    mTracker->RecordStopFrame();

    nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
    while (iter.HasMore()) {
      mTracker->SendStopFrame(iter.GetNext());
    }

    mTracker->MaybeUnblockOnload();
  }

  virtual void OnStopDecode(nsresult aStatus)
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerNotifyingObserver::OnStopDecode");
    NS_ABORT_IF_FALSE(mTracker->GetImage(),
                      "OnStopDecode callback before we've created our image");

    bool preexistingError = mTracker->GetImageStatus() == imgIRequest::STATUS_ERROR;

    mTracker->RecordStopDecode(aStatus);

    nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
    while (iter.HasMore()) {
      mTracker->SendStopDecode(iter.GetNext(), aStatus);
    }

    // This is really hacky. We need to handle the case where we start decoding,
    // block onload, but then hit an error before we get to our first frame.
    mTracker->MaybeUnblockOnload();

    if (NS_FAILED(aStatus) && !preexistingError) {
      mTracker->FireFailureNotification();
    }
  }

  virtual void OnStopRequest(bool aLastPart, nsresult aStatus)
  {
    NS_NOTREACHED("imgStatusTrackerNotifyingObserver(imgDecoderObserver)::OnStopRequest");
  }

  virtual void OnDiscard()
  {
    NS_ABORT_IF_FALSE(mTracker->GetImage(),
                      "OnDiscard callback before we've created our image");

    mTracker->RecordDiscard();

    nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
    while (iter.HasMore()) {
      mTracker->SendDiscard(iter.GetNext());
    }
  }

  virtual void OnUnlockedDraw()
  {
    NS_ABORT_IF_FALSE(mTracker->GetImage(),
                      "OnUnlockedDraw callback before we've created our image");
    mTracker->RecordUnlockedDraw();

    nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
    while (iter.HasMore()) {
      mTracker->SendUnlockedDraw(iter.GetNext());
    }
  }

  virtual void OnImageIsAnimated()
  {
    NS_ABORT_IF_FALSE(mTracker->GetImage(),
                      "OnImageIsAnimated callback before we've created our image");
    mTracker->RecordImageIsAnimated();

    nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
    while (iter.HasMore()) {
      mTracker->SendImageIsAnimated(iter.GetNext());
    }
  }

  virtual void OnError()
  {
    mTracker->RecordError();
  }

private:
  imgStatusTracker* mTracker;
};

class imgStatusTrackerObserver : public imgDecoderObserver
{
public:
  imgStatusTrackerObserver(imgStatusTracker* aTracker)
  : mTracker(aTracker) {}

  virtual ~imgStatusTrackerObserver() {}

  void SetTracker(imgStatusTracker* aTracker) {
    mTracker = aTracker;
  }

  /** imgDecoderObserver methods **/

  virtual void OnStartDecode() MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnStartDecode");
    mTracker->RecordStartDecode();
    if (!mTracker->IsMultipart()) {
      mTracker->RecordBlockOnload();
    }
  }

  virtual void OnStartRequest() MOZ_OVERRIDE
  {
    NS_NOTREACHED("imgStatusTrackerObserver(imgDecoderObserver)::OnStartRequest");
  }

  virtual void OnStartContainer() MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnStartContainer");
    mTracker->RecordStartContainer(mTracker->GetImage());
  }

  virtual void OnStartFrame() MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnStartFrame");
    mTracker->RecordStartFrame();
  }

  virtual void FrameChanged(const nsIntRect* dirtyRect) MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::FrameChanged");
    mTracker->RecordFrameChanged(dirtyRect);
  }

  virtual void OnStopFrame() MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnStopFrame");
    mTracker->RecordStopFrame();
    mTracker->RecordUnblockOnload();
  }

  virtual void OnStopDecode(nsresult aStatus) MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnStopDecode");
    mTracker->RecordStopDecode(aStatus);

    // This is really hacky. We need to handle the case where we start decoding,
    // block onload, but then hit an error before we get to our first frame.
    mTracker->RecordUnblockOnload();
  }

  virtual void OnStopRequest(bool aLastPart, nsresult aStatus) MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnStopRequest");
    mTracker->RecordStopRequest(aLastPart, aStatus);
  }

  virtual void OnDiscard() MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnDiscard");
    mTracker->RecordDiscard();
  }

  virtual void OnUnlockedDraw() MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnUnlockedDraw");
    NS_ABORT_IF_FALSE(mTracker->GetImage(),
                      "OnUnlockedDraw callback before we've created our image");
    mTracker->RecordUnlockedDraw();
  }

  virtual void OnImageIsAnimated() MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnImageIsAnimated");
    mTracker->RecordImageIsAnimated();
  }

  virtual void OnError() MOZ_OVERRIDE
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnError");
    mTracker->RecordError();
  }

private:
  imgStatusTracker* mTracker;
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

void
imgStatusTracker::SetImage(Image* aImage)
{
  NS_ABORT_IF_FALSE(aImage, "Setting null image");
  NS_ABORT_IF_FALSE(!mImage, "Setting image when we already have one");
  mImage = aImage;
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
    imgRequestNotifyRunnable(imgStatusTracker* aTracker, imgRequestProxy* aRequestProxy)
      : mTracker(aTracker)
    {
      mProxies.AppendElement(aRequestProxy);
    }

    NS_IMETHOD Run()
    {
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
#ifdef PR_LOGGING
  if (GetImage() && GetImage()->GetURI()) {
    nsCOMPtr<nsIURI> uri(GetImage()->GetURI());
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
    imgStatusNotifyRunnable(imgStatusTracker& status,
                            imgRequestProxy* requestproxy)
      : mStatus(status), mImage(status.mImage), mProxy(requestproxy)
    {}

    NS_IMETHOD Run()
    {
      mProxy->SetNotificationsDeferred(false);

      mStatus.SyncNotify(mProxy);
      return NS_OK;
    }

  private:
    imgStatusTracker mStatus;
    // We have to hold on to a reference to the tracker's image, just in case
    // it goes away while we're in the event queue.
    nsRefPtr<Image> mImage;
    nsRefPtr<imgRequestProxy> mProxy;
};

void
imgStatusTracker::NotifyCurrentState(imgRequestProxy* proxy)
{
#ifdef PR_LOGGING
  nsCOMPtr<nsIURI> uri;
  proxy->GetURI(getter_AddRefs(uri));
  nsAutoCString spec;
  uri->GetSpec(spec);
  LOG_FUNC_WITH_PARAM(GetImgLog(), "imgStatusTracker::NotifyCurrentState", "uri", spec.get());
#endif

  proxy->SetNotificationsDeferred(true);

  // We don't keep track of
  nsCOMPtr<nsIRunnable> ev = new imgStatusNotifyRunnable(*this, proxy);
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
  MOZ_ASSERT(!mHadLastPart || aOther->mHadLastPart, "mHadLastPart should be monotonic");
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
  LOG_SCOPE(GetImgLog(), "imgStatusTracker::SyncNotifyDifference");

  nsIntRect invalidRect = mInvalidRect.Union(diff.invalidRect);
  mInvalidRect.SetEmpty();
  SyncNotifyState(mConsumers, !!mImage, diff.diffState, invalidRect, mHadLastPart);

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

imgStatusTracker*
imgStatusTracker::CloneForRecording()
{
  imgStatusTracker* clone = new imgStatusTracker(*this);
  return clone;
}

void
imgStatusTracker::SyncNotify(imgRequestProxy* proxy)
{
#ifdef PR_LOGGING
  nsCOMPtr<nsIURI> uri;
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
imgStatusTracker::SyncNotifyDecodeState()
{
  LOG_SCOPE(GetImgLog(), "imgStatusTracker::SyncNotifyDecodeState");

  SyncNotifyState(mConsumers, !!mImage, mState & ~stateRequestStarted, mInvalidRect, mHadLastPart);

  mInvalidRect.SetEmpty();
}

void
imgStatusTracker::EmulateRequestFinished(imgRequestProxy* aProxy,
                                         nsresult aStatus)
{
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
  mConsumers.AppendElementUnlessExists(aConsumer);
}

// XXX - The last argument should go away.
bool
imgStatusTracker::RemoveConsumer(imgRequestProxy* aConsumer, nsresult aStatus)
{
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
  if (!aProxy->NotificationsDeferred())
    aProxy->OnImageIsAnimated();
}

void
imgStatusTracker::SendUnlockedDraw(imgRequestProxy* aProxy)
{
  if (!aProxy->NotificationsDeferred())
    aProxy->OnUnlockedDraw();
}

void
imgStatusTracker::OnUnlockedDraw()
{
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
  if (!aProxy->NotificationsDeferred())
    aProxy->OnStartRequest();
}

void
imgStatusTracker::OnStartRequest()
{
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
  if (!aProxy->NotificationsDeferred()) {
    aProxy->OnStopRequest(aLastPart);
  }
}

void
imgStatusTracker::OnStopRequest(bool aLastPart,
                                nsresult aStatus)
{
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
  if (!aProxy->NotificationsDeferred()) {
    aProxy->UnblockOnload();
  }
}

void
imgStatusTracker::MaybeUnblockOnload()
{
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
  if (GetImage()) {
    nsCOMPtr<nsIURI> uri = GetImage()->GetURI();
    if (uri) {
      nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
      if (os) {
        os->NotifyObservers(uri, "net:failed-to-process-uri-content", nullptr);
      }
    }
  }
}
