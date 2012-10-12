/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgStatusTracker.h"

#include "imgRequest.h"
#include "imgIContainer.h"
#include "imgRequestProxy.h"
#include "Image.h"
#include "ImageLogging.h"
#include "RasterImage.h"
#include "nsIObserverService.h"

#include "mozilla/Util.h"
#include "mozilla/Assertions.h"
#include "mozilla/Services.h"

using namespace mozilla::image;

NS_IMPL_ISUPPORTS3(imgStatusTrackerObserver,
                   imgIDecoderObserver,
                   imgIContainerObserver,
                   nsISupportsWeakReference)

/** imgIContainerObserver methods **/

/* [noscript] void frameChanged (in imgIRequest request,
   in imgIContainer container,
   in nsIntRect dirtyRect); */
NS_IMETHODIMP imgStatusTrackerObserver::FrameChanged(imgIRequest *request,
                                                     imgIContainer *container,
                                                     const nsIntRect *dirtyRect)
{
  LOG_SCOPE(gImgLog, "imgStatusTrackerObserver::FrameChanged");
  NS_ABORT_IF_FALSE(mTracker->GetImage(),
                    "FrameChanged callback before we've created our image");

  mTracker->RecordFrameChanged(container, dirtyRect);

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
  while (iter.HasMore()) {
    mTracker->SendFrameChanged(iter.GetNext(), container, dirtyRect);
  }

  return NS_OK;
}

/** imgIDecoderObserver methods **/

/* void onStartDecode (in imgIRequest request); */
NS_IMETHODIMP imgStatusTrackerObserver::OnStartDecode(imgIRequest *request)
{
  LOG_SCOPE(gImgLog, "imgStatusTrackerObserver::OnStartDecode");
  NS_ABORT_IF_FALSE(mTracker->GetImage(),
                    "OnStartDecode callback before we've created our image");


  mTracker->RecordStartDecode();

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
  while (iter.HasMore()) {
    mTracker->SendStartDecode(iter.GetNext());
  }

  if (!mTracker->GetRequest()->GetMultipart()) {
    MOZ_ASSERT(!mTracker->mBlockingOnload);
    mTracker->mBlockingOnload = true;

    mTracker->RecordBlockOnload();

    nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
    while (iter.HasMore()) {
      mTracker->SendBlockOnload(iter.GetNext());
    }
  }

  /* In the case of streaming jpegs, it is possible to get multiple OnStartDecodes which
     indicates the beginning of a new decode.
     The cache entry's size therefore needs to be reset to 0 here.  If we do not do this,
     the code in imgStatusTrackerObserver::OnStopFrame will continue to increase the data size cumulatively.
  */
  mTracker->GetRequest()->ResetCacheEntry();

  return NS_OK;
}

NS_IMETHODIMP imgStatusTrackerObserver::OnStartRequest(imgIRequest *aRequest)
{
  NS_NOTREACHED("imgRequest(imgIDecoderObserver)::OnStartRequest");
  return NS_OK;
}

/* void onStartContainer (in imgIRequest request, in imgIContainer image); */
NS_IMETHODIMP imgStatusTrackerObserver::OnStartContainer(imgIRequest *request, imgIContainer *image)
{
  LOG_SCOPE(gImgLog, "imgStatusTrackerObserver::OnStartContainer");

  NS_ASSERTION(image, "imgStatusTrackerObserver::OnStartContainer called with a null image!");
  if (!image) return NS_ERROR_UNEXPECTED;

  NS_ABORT_IF_FALSE(mTracker->GetImage(),
                    "OnStartContainer callback before we've created our image");
  NS_ABORT_IF_FALSE(image == mTracker->GetImage(),
                    "OnStartContainer callback from an image we don't own");
  mTracker->RecordStartContainer(image);

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
  while (iter.HasMore()) {
    mTracker->SendStartContainer(iter.GetNext(), image);
  }

  return NS_OK;
}

/* void onStartFrame (in imgIRequest request, in unsigned long frame); */
NS_IMETHODIMP imgStatusTrackerObserver::OnStartFrame(imgIRequest *request,
                                       uint32_t frame)
{
  LOG_SCOPE(gImgLog, "imgStatusTrackerObserver::OnStartFrame");
  NS_ABORT_IF_FALSE(mTracker->GetImage(),
                    "OnStartFrame callback before we've created our image");

  mTracker->RecordStartFrame(frame);

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
  while (iter.HasMore()) {
    mTracker->SendStartFrame(iter.GetNext(), frame);
  }

  return NS_OK;
}

/* [noscript] void onDataAvailable (in imgIRequest request, in boolean aCurrentFrame, [const] in nsIntRect rect); */
NS_IMETHODIMP imgStatusTrackerObserver::OnDataAvailable(imgIRequest *request,
                                          bool aCurrentFrame,
                                          const nsIntRect * rect)
{
  LOG_SCOPE(gImgLog, "imgStatusTrackerObserver::OnDataAvailable");
  NS_ABORT_IF_FALSE(mTracker->GetImage(),
                    "OnDataAvailable callback before we've created our image");

  mTracker->RecordDataAvailable(aCurrentFrame, rect);

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
  while (iter.HasMore()) {
    mTracker->SendDataAvailable(iter.GetNext(), aCurrentFrame, rect);
  }

  return NS_OK;
}

/* void onStopFrame (in imgIRequest request, in unsigned long frame); */
NS_IMETHODIMP imgStatusTrackerObserver::OnStopFrame(imgIRequest *request,
                                      uint32_t frame)
{
  LOG_SCOPE(gImgLog, "imgStatusTrackerObserver::OnStopFrame");
  NS_ABORT_IF_FALSE(mTracker->GetImage(),
                    "OnStopFrame callback before we've created our image");

  mTracker->RecordStopFrame(frame);

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
  while (iter.HasMore()) {
    mTracker->SendStopFrame(iter.GetNext(), frame);
  }

  mTracker->MaybeUnblockOnload();

  return NS_OK;
}

/* void onStopDecode (in imgIRequest request, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP imgStatusTrackerObserver::OnStopDecode(imgIRequest *aRequest,
                                                     nsresult aStatus)
{
  LOG_SCOPE(gImgLog, "imgStatusTrackerObserver::OnStopDecode");
  NS_ABORT_IF_FALSE(mTracker->GetImage(),
                    "OnStopDecode callback before we've created our image");

  // We finished the decode, and thus have the decoded frames. Update the cache
  // entry size to take this into account.
  mTracker->GetRequest()->UpdateCacheEntrySize();

  mTracker->RecordStopDecode(aStatus);

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
  while (iter.HasMore()) {
    mTracker->SendStopDecode(iter.GetNext(), aStatus);
  }

  // This is really hacky. We need to handle the case where we start decoding,
  // block onload, but then hit an error before we get to our first frame.
  mTracker->MaybeUnblockOnload();

  if (NS_FAILED(aStatus)) {
    // Some kind of problem has happened with image decoding.
    // Report the URI to net:failed-to-process-uri-conent observers.

    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      nsCOMPtr<nsIURI> uri;
      mTracker->GetRequest()->GetURI(getter_AddRefs(uri));
      os->NotifyObservers(uri, "net:failed-to-process-uri-content", nullptr);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP imgStatusTrackerObserver::OnStopRequest(imgIRequest *aRequest,
                                        bool aLastPart)
{
  NS_NOTREACHED("imgRequest(imgIDecoderObserver)::OnStopRequest");
  return NS_OK;
}

/* void onDiscard (in imgIRequest request); */
NS_IMETHODIMP imgStatusTrackerObserver::OnDiscard(imgIRequest *aRequest)
{
  NS_ABORT_IF_FALSE(mTracker->GetImage(),
                    "OnDiscard callback before we've created our image");

  mTracker->RecordDiscard();

  // Update the cache entry size, since we just got rid of frame data
  mTracker->GetRequest()->UpdateCacheEntrySize();

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
  while (iter.HasMore()) {
    mTracker->SendDiscard(iter.GetNext());
  }

  return NS_OK;
}

NS_IMETHODIMP imgStatusTrackerObserver::OnImageIsAnimated(imgIRequest *aRequest)
{
  NS_ABORT_IF_FALSE(mTracker->GetImage(),
                    "OnImageIsAnimated callback before we've created our image");
  mTracker->RecordImageIsAnimated();

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mTracker->mConsumers);
  while (iter.HasMore()) {
    mTracker->SendImageIsAnimated(iter.GetNext());
  }

  return NS_OK;
}

// imgStatusTracker methods

static nsresult
GetResultFromImageStatus(uint32_t aStatus)
{
  if (aStatus & imgIRequest::STATUS_ERROR)
    return NS_IMAGELIB_ERROR_FAILURE;
  if (aStatus & imgIRequest::STATUS_LOAD_COMPLETE)
    return NS_IMAGELIB_SUCCESS_LOAD_FINISHED;
  return NS_OK;
}

imgStatusTracker::imgStatusTracker(Image* aImage, imgRequest* aRequest)
  : mImage(aImage),
    mRequest(aRequest),
    mState(0),
    mImageStatus(imgIRequest::STATUS_NONE),
    mHadLastPart(false),
    mBlockingOnload(false),
    mTrackerObserver(new imgStatusTrackerObserver(this))
{}

imgStatusTracker::imgStatusTracker(const imgStatusTracker& aOther)
  : mImage(aOther.mImage),
    mRequest(aOther.mRequest),
    mState(aOther.mState),
    mImageStatus(aOther.mImageStatus),
    mHadLastPart(aOther.mHadLastPart),
    mBlockingOnload(aOther.mBlockingOnload)
    // Note: we explicitly don't copy mRequestRunnable, because it won't be
    // nulled out when the mRequestRunnable's Run function eventually gets
    // called.
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
    imgRequestNotifyRunnable(imgRequest* request, imgRequestProxy* requestproxy)
      : mRequest(request)
    {
      mProxies.AppendElement(requestproxy);
    }

    NS_IMETHOD Run()
    {
      imgStatusTracker& statusTracker = mRequest->GetStatusTracker();

      for (uint32_t i = 0; i < mProxies.Length(); ++i) {
        mProxies[i]->SetNotificationsDeferred(false);
        statusTracker.SyncNotify(mProxies[i]);
      }

      statusTracker.mRequestRunnable = nullptr;
      return NS_OK;
    }

    void AddProxy(imgRequestProxy* aRequestProxy)
    {
      mProxies.AppendElement(aRequestProxy);
    }

  private:
    friend class imgStatusTracker;

    nsRefPtr<imgRequest> mRequest;
    nsTArray<nsRefPtr<imgRequestProxy> > mProxies;
};

void
imgStatusTracker::Notify(imgRequest* request, imgRequestProxy* proxy)
{
#ifdef PR_LOGGING
  nsCOMPtr<nsIURI> uri;
  request->GetURI(getter_AddRefs(uri));
  nsAutoCString spec;
  uri->GetSpec(spec);
  LOG_FUNC_WITH_PARAM(gImgLog, "imgStatusTracker::Notify async", "uri", spec.get());
#endif

  proxy->SetNotificationsDeferred(true);

  // If we have an existing runnable that we can use, we just append this proxy
  // to its list of proxies to be notified. This ensures we don't unnecessarily
  // delay onload.
  imgRequestNotifyRunnable* runnable = static_cast<imgRequestNotifyRunnable*>(mRequestRunnable.get());
  if (runnable && runnable->mRequest == request) {
    runnable->AddProxy(proxy);
  } else {
    // It's okay to overwrite an existing mRequestRunnable, because adding a
    // new proxy is strictly a performance optimization. The notification will
    // always happen, regardless of whether we hold a reference to a runnable.
    mRequestRunnable = new imgRequestNotifyRunnable(request, proxy);
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
  LOG_FUNC_WITH_PARAM(gImgLog, "imgStatusTracker::NotifyCurrentState", "uri", spec.get());
#endif

  proxy->SetNotificationsDeferred(true);

  // We don't keep track of 
  nsCOMPtr<nsIRunnable> ev = new imgStatusNotifyRunnable(*this, proxy);
  NS_DispatchToCurrentThread(ev);
}

void
imgStatusTracker::SyncNotify(imgRequestProxy* proxy)
{
  NS_ABORT_IF_FALSE(!proxy->NotificationsDeferred(),
    "Calling imgStatusTracker::Notify() on a proxy that doesn't want notifications!");

#ifdef PR_LOGGING
  nsCOMPtr<nsIURI> uri;
  proxy->GetURI(getter_AddRefs(uri));
  nsAutoCString spec;
  uri->GetSpec(spec);
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgStatusTracker::SyncNotify", "uri", spec.get());
#endif

  nsCOMPtr<imgIRequest> kungFuDeathGrip(proxy);

  // OnStartRequest
  if (mState & stateRequestStarted)
    proxy->OnStartRequest();

  // OnStartContainer
  if (mState & stateHasSize)
    proxy->OnStartContainer(mImage);

  // OnStartDecode
  if (mState & stateDecodeStarted)
    proxy->OnStartDecode();

  // BlockOnload
  if (mState & stateBlockingOnload)
    proxy->BlockOnload();

  if (mImage) {
    int16_t imageType = mImage->GetType();
    // Send frame messages (OnStartFrame, OnDataAvailable, OnStopFrame)
    if (imageType == imgIContainer::TYPE_VECTOR ||
        static_cast<RasterImage*>(mImage)->GetNumFrames() > 0) {

      uint32_t frame = (imageType == imgIContainer::TYPE_VECTOR) ?
        0 : static_cast<RasterImage*>(mImage)->GetCurrentFrameIndex();

      proxy->OnStartFrame(frame);

      // OnDataAvailable
      // XXX - Should only send partial rects here, but that needs to
      // wait until we fix up the observer interface
      nsIntRect r;
      mImage->GetCurrentFrameRect(r);
      proxy->OnDataAvailable(frame, &r);

      if (mState & stateFrameStopped)
        proxy->OnStopFrame(frame);
    }

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

// XXX - The last two arguments should go away.
bool
imgStatusTracker::RemoveConsumer(imgRequestProxy* aConsumer, nsresult aStatus,
                                 bool aOnlySendStopRequest)
{
  // Remove the proxy from the list.
  bool removed = mConsumers.RemoveElement(aConsumer);
  //MOZ_NONFATAL_ASSERT(removed, "Trying to remove a consumer we don't have");

  // Consumers can get confused if they don't get all the proper teardown
  // notifications. Part ways on good terms.
  if (removed)
    EmulateRequestFinished(aConsumer, aStatus, aOnlySendStopRequest);
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

/* non-virtual imgIDecoderObserver methods */
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
imgStatusTracker::SendStartContainer(imgRequestProxy* aProxy, imgIContainer* aContainer)
{
  if (!aProxy->NotificationsDeferred())
    aProxy->OnStartContainer(aContainer);
}

void
imgStatusTracker::RecordStartFrame(uint32_t aFrame)
{
  NS_ABORT_IF_FALSE(mImage, "RecordStartFrame called before we have an Image");
  // no bookkeeping necessary here - this is implied by imgIContainer's number
  // of frames
}

void
imgStatusTracker::SendStartFrame(imgRequestProxy* aProxy, uint32_t aFrame)
{
  if (!aProxy->NotificationsDeferred())
    aProxy->OnStartFrame(aFrame);
}

void
imgStatusTracker::RecordDataAvailable(bool aCurrentFrame, const nsIntRect* aRect)
{
  NS_ABORT_IF_FALSE(mImage,
                    "RecordDataAvailable called before we have an Image");
  // no bookkeeping necessary here - this is implied by imgIContainer's
  // number of frames and frame rect
}

void
imgStatusTracker::SendDataAvailable(imgRequestProxy* aProxy, bool aCurrentFrame,
                                         const nsIntRect* aRect)
{
  if (!aProxy->NotificationsDeferred())
    aProxy->OnDataAvailable(aCurrentFrame, aRect);
}


void
imgStatusTracker::RecordStopFrame(uint32_t aFrame)
{
  NS_ABORT_IF_FALSE(mImage, "RecordStopFrame called before we have an Image");
  mState |= stateFrameStopped;
  mImageStatus |= imgIRequest::STATUS_FRAME_COMPLETE;
}

void
imgStatusTracker::SendStopFrame(imgRequestProxy* aProxy, uint32_t aFrame)
{
  if (!aProxy->NotificationsDeferred())
    aProxy->OnStopFrame(aFrame);
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
imgStatusTracker::SendStopDecode(imgRequestProxy* aProxy, nsresult aStatus)
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
  uint32_t stateBitsToClear = stateDecodeStarted | stateDecodeStopped;
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

/* non-virtual imgIContainerObserver methods */
void
imgStatusTracker::RecordFrameChanged(imgIContainer* aContainer,
                                     const nsIntRect* aDirtyRect)
{
  NS_ABORT_IF_FALSE(mImage,
                    "RecordFrameChanged called before we have an Image");
  // no bookkeeping necessary here - this is only for in-frame updates, which we
  // don't fire while we're recording
}

void
imgStatusTracker::SendFrameChanged(imgRequestProxy* aProxy, imgIContainer* aContainer,
                                   const nsIntRect* aDirtyRect)
{
  if (!aProxy->NotificationsDeferred())
    aProxy->FrameChanged(aContainer, aDirtyRect);
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
imgStatusTracker::RecordStopRequest(bool aLastPart, nsresult aStatus)
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
imgStatusTracker::SendStopRequest(imgRequestProxy* aProxy, bool aLastPart, nsresult aStatus)
{
  if (!aProxy->NotificationsDeferred()) {
    aProxy->OnStopRequest(aLastPart);
  }
}

void
imgStatusTracker::OnStopRequest(bool aLastPart, nsresult aStatus)
{
  RecordStopRequest(aLastPart, aStatus);
  /* notify the kids */
  nsTObserverArray<imgRequestProxy*>::ForwardIterator srIter(mConsumers);
  while (srIter.HasMore()) {
    SendStopRequest(srIter.GetNext(), aLastPart, aStatus);
  }

  if (NS_FAILED(aStatus)) {
    // Some kind of problem has happened with image decoding.
    // Report the URI to net:failed-to-process-uri-conent observers.

    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      nsCOMPtr<nsIURI> uri;
      mTracker->GetRequest()->GetURI(getter_AddRefs(uri));
      os->NotifyObservers(uri, "net:failed-to-process-uri-content", nullptr);
    }
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
