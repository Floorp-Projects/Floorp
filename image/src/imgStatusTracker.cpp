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

#include "mozilla/Util.h"
#include "mozilla/Assertions.h"
#include "mozilla/Services.h"

using namespace mozilla::image;

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

  virtual void OnStartDecode()
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnStartDecode");
    NS_ABORT_IF_FALSE(mTracker->GetImage(),
                      "OnStartDecode callback before we've created our image");

    mTracker->RecordStartDecode();

    nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
    while (iter.HasMore()) {
      mTracker->SendStartDecode(iter.GetNext());
    }

    if (!mTracker->IsMultipart()) {
      MOZ_ASSERT(!mTracker->mBlockingOnload);
      mTracker->mBlockingOnload = true;

      mTracker->RecordBlockOnload();

      nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
      while (iter.HasMore()) {
        mTracker->SendBlockOnload(iter.GetNext());
      }
    }
  }

  virtual void OnStartRequest()
  {
    NS_NOTREACHED("imgRequest(imgDecoderObserver)::OnStartRequest");
  }

  virtual void OnStartContainer()
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnStartContainer");

    NS_ABORT_IF_FALSE(mTracker->GetImage(),
                      "OnStartContainer callback before we've created our image");
    mTracker->RecordStartContainer(mTracker->GetImage());

    nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
    while (iter.HasMore()) {
      mTracker->SendStartContainer(iter.GetNext());
    }
  }

  virtual void OnDataAvailable(const nsIntRect* rect)
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnDataAvailable");
    NS_ABORT_IF_FALSE(mTracker->GetImage(),
                      "OnDataAvailable callback before we've created our image");

    mTracker->RecordDataAvailable();

    nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
    while (iter.HasMore()) {
      mTracker->SendDataAvailable(iter.GetNext(), rect);
    }
  }

  virtual void FrameChanged(const nsIntRect* dirtyRect)
  {
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::FrameChanged");
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
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnStopFrame");
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
    LOG_SCOPE(GetImgLog(), "imgStatusTrackerObserver::OnStopDecode");
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

  virtual void OnStopRequest(bool aLastPart)
  {
    NS_NOTREACHED("imgRequest(imgDecoderObserver)::OnStopRequest");
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

private:
  imgStatusTracker* mTracker;
};


// imgStatusTracker methods

imgStatusTracker::imgStatusTracker(Image* aImage)
  : mImage(aImage),
    mTrackerObserver(new imgStatusTrackerObserver(this)),
    mState(0),
    mImageStatus(imgIRequest::STATUS_NONE),
    mIsMultipart(false),
    mHadLastPart(false),
    mBlockingOnload(false)
{}

imgStatusTracker::imgStatusTracker(const imgStatusTracker& aOther)
  : mImage(aOther.mImage),
    mState(aOther.mState),
    mImageStatus(aOther.mImageStatus),
    mIsMultipart(aOther.mIsMultipart),
    mHadLastPart(aOther.mHadLastPart),
    mBlockingOnload(aOther.mBlockingOnload)
    // Note: we explicitly don't copy mRequestRunnable, because it won't be
    // nulled out when the mRequestRunnable's Run function eventually gets
    // called.
{}

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

  nsCOMPtr<imgIRequest> kungFuDeathGrip(proxy);

  // OnStartRequest
  if (mState & stateRequestStarted)
    proxy->OnStartRequest();

  // OnStartContainer
  if (mState & stateHasSize)
    proxy->OnStartContainer();

  // OnStartDecode
  if (mState & stateDecodeStarted)
    proxy->OnStartDecode();

  // BlockOnload
  if (mState & stateBlockingOnload)
    proxy->BlockOnload();

  if (mImage) {
    // OnDataAvailable
    // XXX - Should only send partial rects here, but that needs to
    // wait until we fix up the observer interface
    nsIntRect r;
    mImage->GetCurrentFrameRect(r);

    // If there's any content in this frame at all (always true for
    // vector images, true for raster images that have decoded at
    // least one frame) then send OnFrameUpdate.
    if (!r.IsEmpty())
      proxy->OnFrameUpdate(&r);

    if (mState & stateFrameStopped)
      proxy->OnStopFrame();

    // OnImageIsAnimated
    bool isAnimated = false;

    nsresult rv = mImage->GetAnimated(&isAnimated);
    if (NS_SUCCEEDED(rv) && isAnimated) {
      proxy->OnImageIsAnimated();
    }
  }

  if (mState & stateDecodeStopped) {
    NS_ABORT_IF_FALSE(mImage, "stopped decoding without ever having an image?");
    proxy->OnStopDecode();
  }

  if (mState & stateRequestStopped) {
    proxy->OnStopRequest(mHadLastPart);
  }
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
  if (removed)
    EmulateRequestFinished(aConsumer, aStatus);
  return removed;
}

void
imgStatusTracker::RecordCancel()
{
  if (!(mImageStatus & imgIRequest::STATUS_LOAD_PARTIAL))
    mImageStatus |= imgIRequest::STATUS_ERROR;
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
}

void
imgStatusTracker::RecordStartDecode()
{
  NS_ABORT_IF_FALSE(mImage, "RecordStartDecode without an Image");
  mState |= stateDecodeStarted;
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
imgStatusTracker::RecordDataAvailable()
{
  NS_ABORT_IF_FALSE(mImage,
                    "RecordDataAvailable called before we have an Image");
  // no bookkeeping necessary here - this is implied by imgIContainer's
  // number of frames and frame rect
}

void
imgStatusTracker::SendDataAvailable(imgRequestProxy* aProxy,
                                    const nsIntRect* aRect)
{
  if (!aProxy->NotificationsDeferred())
    aProxy->OnFrameUpdate(aRect);
}


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

  if (NS_SUCCEEDED(aStatus) && mImageStatus != imgIRequest::STATUS_ERROR)
    mImageStatus |= imgIRequest::STATUS_DECODE_COMPLETE;
  // If we weren't successful, clear all success status bits and set error.
  else
    mImageStatus = imgIRequest::STATUS_ERROR;
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
  uint32_t statusBitsToClear = imgIRequest::STATUS_FRAME_COMPLETE
                               | imgIRequest::STATUS_DECODE_COMPLETE;
  mImageStatus &= ~statusBitsToClear;
}

void
imgStatusTracker::SendImageIsAnimated(imgRequestProxy* aProxy)
{
  if (!aProxy->NotificationsDeferred())
    aProxy->OnImageIsAnimated();
}

void
imgStatusTracker::RecordImageIsAnimated()
{
  NS_ABORT_IF_FALSE(mImage,
                    "RecordImageIsAnimated called before we have an Image");
  // No bookkeeping necessary here - once decoding is complete, GetAnimated()
  // will accurately return that this is an animated image. Until that time,
  // the OnImageIsAnimated notification is the only indication an observer
  // will have that an image has more than 1 frame.
}

void
imgStatusTracker::SendDiscard(imgRequestProxy* aProxy)
{
  if (!aProxy->NotificationsDeferred())
    aProxy->OnDiscard();
}

void
imgStatusTracker::RecordFrameChanged(const nsIntRect* aDirtyRect)
{
  NS_ABORT_IF_FALSE(mImage,
                    "RecordFrameChanged called before we have an Image");
  // no bookkeeping necessary here - this is only for in-frame updates, which we
  // don't fire while we're recording
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
  mState &= ~stateRequestStarted;
  mState &= ~stateDecodeStarted;
  mState &= ~stateDecodeStopped;
  mState &= ~stateRequestStopped;
  mState &= ~stateBlockingOnload;

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
  MOZ_ASSERT(mState & stateBlockingOnload);
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
  if (!mBlockingOnload) {
    return;
  }

  mBlockingOnload = false;

  RecordUnblockOnload();

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mConsumers);
  while (iter.HasMore()) {
    SendUnblockOnload(iter.GetNext());
  }
}

void
imgStatusTracker::FireFailureNotification()
{
  // Some kind of problem has happened with image decoding.
  // Report the URI to net:failed-to-process-uri-conent observers.
  nsCOMPtr<nsIURI> uri = GetImage()->GetURI();
  if (uri) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->NotifyObservers(uri, "net:failed-to-process-uri-content", nullptr);
    }
  }
}
