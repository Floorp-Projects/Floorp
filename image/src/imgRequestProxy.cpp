/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgRequestProxy.h"
#include "imgIOnloadBlocker.h"

#include "nsIInputStream.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIMultiPartChannel.h"

#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"

#include "Image.h"
#include "nsError.h"
#include "ImageLogging.h"

#include "nspr.h"

using namespace mozilla::image;

// The split of imgRequestProxy and imgRequestProxyStatic means that
// certain overridden functions need to be usable in the destructor.
// Since virtual functions can't be used in that way, this class
// provides a behavioural trait for each class to use instead.
class ProxyBehaviour
{
 public:
  virtual ~ProxyBehaviour() {}

  virtual mozilla::image::Image* GetImage() const = 0;
  virtual imgStatusTracker& GetStatusTracker() const = 0;
  virtual imgRequest* GetOwner() const = 0;
  virtual void SetOwner(imgRequest* aOwner) = 0;
};

class RequestBehaviour : public ProxyBehaviour
{
 public:
  RequestBehaviour() : mOwner(nullptr), mOwnerHasImage(false) {}

  virtual mozilla::image::Image* GetImage() const MOZ_OVERRIDE;
  virtual imgStatusTracker& GetStatusTracker() const MOZ_OVERRIDE;

  virtual imgRequest* GetOwner() const MOZ_OVERRIDE {
    return mOwner;
  }

  virtual void SetOwner(imgRequest* aOwner) MOZ_OVERRIDE {
    mOwner = aOwner;
    mOwnerHasImage = !!aOwner->GetStatusTracker().GetImage();
  }

 private:
  // We maintain the following invariant:
  // The proxy is registered at most with a single imgRequest as an observer,
  // and whenever it is, mOwner points to that object. This helps ensure that
  // imgRequestProxy::~imgRequestProxy unregisters the proxy as an observer
  // from whatever request it was registered with (if any). This, in turn,
  // means that imgRequest::mObservers will not have any stale pointers in it.
  nsRefPtr<imgRequest> mOwner;

  bool mOwnerHasImage;
};

mozilla::image::Image*
RequestBehaviour::GetImage() const
{
  if (!mOwnerHasImage)
    return nullptr;
  return GetStatusTracker().GetImage();
}

imgStatusTracker&
RequestBehaviour::GetStatusTracker() const
{
  // NOTE: It's possible that our mOwner has an Image that it didn't notify
  // us about, if we were Canceled before its Image was constructed.
  // (Canceling removes us as an observer, so mOwner has no way to notify us).
  // That's why this method uses mOwner->GetStatusTracker() instead of just
  // mOwner->mStatusTracker -- we might have a null mImage and yet have an
  // mOwner with a non-null mImage (and a null mStatusTracker pointer).
  return mOwner->GetStatusTracker();
}

NS_IMPL_ADDREF(imgRequestProxy)
NS_IMPL_RELEASE(imgRequestProxy)

NS_INTERFACE_MAP_BEGIN(imgRequestProxy)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, imgIRequest)
  NS_INTERFACE_MAP_ENTRY(imgIRequest)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_INTERFACE_MAP_ENTRY(nsISupportsPriority)
  NS_INTERFACE_MAP_ENTRY(nsISecurityInfoProvider)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsITimedChannel, TimedChannel() != nullptr)
NS_INTERFACE_MAP_END

imgRequestProxy::imgRequestProxy() :
  mBehaviour(new RequestBehaviour),
  mURI(nullptr),
  mListener(nullptr),
  mLoadFlags(nsIRequest::LOAD_NORMAL),
  mLockCount(0),
  mAnimationConsumers(0),
  mCanceled(false),
  mIsInLoadGroup(false),
  mListenerIsStrongRef(false),
  mDecodeRequested(false),
  mDeferNotifications(false),
  mSentStartContainer(false)
{
  /* member initializers and constructor code */

}

imgRequestProxy::~imgRequestProxy()
{
  /* destructor code */
  NS_PRECONDITION(!mListener, "Someone forgot to properly cancel this request!");

  // Unlock the image the proper number of times if we're holding locks on it.
  // Note that UnlockImage() decrements mLockCount each time it's called.
  while (mLockCount)
    UnlockImage();

  ClearAnimationConsumers();

  // Explicitly set mListener to null to ensure that the RemoveProxy
  // call below can't send |this| to an arbitrary listener while |this|
  // is being destroyed.  This is all belt-and-suspenders in view of the
  // above assert.
  NullOutListener();

  if (GetOwner()) {
    /* Call RemoveProxy with a successful status.  This will keep the
       channel, if still downloading data, from being canceled if 'this' is
       the last observer.  This allows the image to continue to download and
       be cached even if no one is using it currently.
    */
    mCanceled = true;
    GetOwner()->RemoveProxy(this, NS_OK);
  }
}

nsresult imgRequestProxy::Init(imgStatusTracker* aStatusTracker,
                               nsILoadGroup* aLoadGroup,
                               nsIURI* aURI, imgINotificationObserver* aObserver)
{
  NS_PRECONDITION(!GetOwner() && !mListener, "imgRequestProxy is already initialized");

  LOG_SCOPE_WITH_PARAM(GetImgLog(), "imgRequestProxy::Init", "request", aStatusTracker->GetRequest());

  NS_ABORT_IF_FALSE(mAnimationConsumers == 0, "Cannot have animation before Init");

  mBehaviour->SetOwner(aStatusTracker->GetRequest());
  mListener = aObserver;
  // Make sure to addref mListener before the AddProxy call below, since
  // that call might well want to release it if the imgRequest has
  // already seen OnStopRequest.
  if (mListener) {
    mListenerIsStrongRef = true;
    NS_ADDREF(mListener);
  }
  mLoadGroup = aLoadGroup;
  mURI = aURI;

  // Note: AddProxy won't send all the On* notifications immediately
  if (GetOwner())
    GetOwner()->AddProxy(this);

  return NS_OK;
}

nsresult imgRequestProxy::ChangeOwner(imgRequest *aNewOwner)
{
  NS_PRECONDITION(GetOwner(), "Cannot ChangeOwner on a proxy without an owner!");

  if (mCanceled) {
    // Ensure that this proxy has received all notifications to date before
    // we clean it up when removing it from the old owner below.
    SyncNotifyListener();
  }

  // If we're holding locks, unlock the old image.
  // Note that UnlockImage decrements mLockCount each time it's called.
  uint32_t oldLockCount = mLockCount;
  while (mLockCount)
    UnlockImage();

  // If we're holding animation requests, undo them.
  uint32_t oldAnimationConsumers = mAnimationConsumers;
  ClearAnimationConsumers();

  // Were we decoded before?
  bool wasDecoded = false;
  if (GetImage() &&
      (GetStatusTracker().GetImageStatus() & imgIRequest::STATUS_FRAME_COMPLETE)) {
    wasDecoded = true;
  }

  GetOwner()->RemoveProxy(this, NS_IMAGELIB_CHANGING_OWNER);

  mBehaviour->SetOwner(aNewOwner);

  // If we were locked, apply the locks here
  for (uint32_t i = 0; i < oldLockCount; i++)
    LockImage();

  // If we had animation requests, restore them here. Note that we
  // do this *after* RemoveProxy, which clears out animation consumers
  // (see bug 601723).
  for (uint32_t i = 0; i < oldAnimationConsumers; i++)
    IncrementAnimationConsumers();

  GetOwner()->AddProxy(this);

  // If we were decoded, or if we'd previously requested a decode, request a
  // decode on the new image
  if (wasDecoded || mDecodeRequested)
    GetOwner()->StartDecoding();

  return NS_OK;
}

void imgRequestProxy::AddToLoadGroup()
{
  NS_ASSERTION(!mIsInLoadGroup, "Whaa, we're already in the loadgroup!");

  if (!mIsInLoadGroup && mLoadGroup) {
    mLoadGroup->AddRequest(this, nullptr);
    mIsInLoadGroup = true;
  }
}

void imgRequestProxy::RemoveFromLoadGroup(bool releaseLoadGroup)
{
  if (!mIsInLoadGroup)
    return;

  /* calling RemoveFromLoadGroup may cause the document to finish
     loading, which could result in our death.  We need to make sure
     that we stay alive long enough to fight another battle... at
     least until we exit this function.
  */
  nsCOMPtr<imgIRequest> kungFuDeathGrip(this);

  mLoadGroup->RemoveRequest(this, nullptr, NS_OK);
  mIsInLoadGroup = false;

  if (releaseLoadGroup) {
    // We're done with the loadgroup, release it.
    mLoadGroup = nullptr;
  }
}


/**  nsIRequest / imgIRequest methods **/

/* readonly attribute wstring name; */
NS_IMETHODIMP imgRequestProxy::GetName(nsACString &aName)
{
  aName.Truncate();

  if (mURI)
    mURI->GetSpec(aName);

  return NS_OK;
}

/* boolean isPending (); */
NS_IMETHODIMP imgRequestProxy::IsPending(bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsresult status; */
NS_IMETHODIMP imgRequestProxy::GetStatus(nsresult *aStatus)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void cancel (in nsresult status); */
NS_IMETHODIMP imgRequestProxy::Cancel(nsresult status)
{
  if (mCanceled)
    return NS_ERROR_FAILURE;

  LOG_SCOPE(GetImgLog(), "imgRequestProxy::Cancel");

  mCanceled = true;

  nsCOMPtr<nsIRunnable> ev = new imgCancelRunnable(this, status);
  return NS_DispatchToCurrentThread(ev);
}

void
imgRequestProxy::DoCancel(nsresult status)
{
  if (GetOwner()) {
    GetOwner()->RemoveProxy(this, status);
  }

  NullOutListener();
}

/* void cancelAndForgetObserver (in nsresult aStatus); */
NS_IMETHODIMP imgRequestProxy::CancelAndForgetObserver(nsresult aStatus)
{
  // If mCanceled is true but mListener is non-null, that means
  // someone called Cancel() on us but the imgCancelRunnable is still
  // pending.  We still need to null out mListener before returning
  // from this function in this case.  That means we want to do the
  // RemoveProxy call right now, because we need to deliver the
  // onStopRequest.
  if (mCanceled && !mListener)
    return NS_ERROR_FAILURE;

  LOG_SCOPE(GetImgLog(), "imgRequestProxy::CancelAndForgetObserver");

  mCanceled = true;

  // Now cheat and make sure our removal from loadgroup happens async
  bool oldIsInLoadGroup = mIsInLoadGroup;
  mIsInLoadGroup = false;

  if (GetOwner()) {
    GetOwner()->RemoveProxy(this, aStatus);
  }

  mIsInLoadGroup = oldIsInLoadGroup;

  if (mIsInLoadGroup) {
    nsCOMPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(this, &imgRequestProxy::DoRemoveFromLoadGroup);
    NS_DispatchToCurrentThread(ev);
  }

  NullOutListener();

  return NS_OK;
}

/* void startDecode (); */
NS_IMETHODIMP
imgRequestProxy::StartDecoding()
{
  if (!GetOwner())
    return NS_ERROR_FAILURE;

  // Flag this, so we know to transfer the request if our owner changes
  mDecodeRequested = true;

  // Forward the request
  return GetOwner()->StartDecoding();
}

/* void requestDecode (); */
NS_IMETHODIMP
imgRequestProxy::RequestDecode()
{
  if (!GetOwner())
    return NS_ERROR_FAILURE;

  // Flag this, so we know to transfer the request if our owner changes
  mDecodeRequested = true;

  // Forward the request
  return GetOwner()->RequestDecode();
}


/* void lockImage (); */
NS_IMETHODIMP
imgRequestProxy::LockImage()
{
  mLockCount++;
  if (GetImage())
    return GetImage()->LockImage();
  return NS_OK;
}

/* void unlockImage (); */
NS_IMETHODIMP
imgRequestProxy::UnlockImage()
{
  NS_ABORT_IF_FALSE(mLockCount > 0, "calling unlock but no locks!");

  mLockCount--;
  if (GetImage())
    return GetImage()->UnlockImage();
  return NS_OK;
}

/* void requestDiscard (); */
NS_IMETHODIMP
imgRequestProxy::RequestDiscard()
{
  if (GetImage())
    return GetImage()->RequestDiscard();
  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::IncrementAnimationConsumers()
{
  mAnimationConsumers++;
  if (GetImage())
    GetImage()->IncrementAnimationConsumers();
  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::DecrementAnimationConsumers()
{
  // We may get here if some responsible code called Increment,
  // then called us, but we have meanwhile called ClearAnimationConsumers
  // because we needed to get rid of them earlier (see
  // imgRequest::RemoveProxy), and hence have nothing left to
  // decrement. (In such a case we got rid of the animation consumers
  // early, but not the observer.)
  if (mAnimationConsumers > 0) {
    mAnimationConsumers--;
    if (GetImage())
      GetImage()->DecrementAnimationConsumers();
  }
  return NS_OK;
}

void
imgRequestProxy::ClearAnimationConsumers()
{
  while (mAnimationConsumers > 0)
    DecrementAnimationConsumers();
}

/* void suspend (); */
NS_IMETHODIMP imgRequestProxy::Suspend()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void resume (); */
NS_IMETHODIMP imgRequestProxy::Resume()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsILoadGroup loadGroup */
NS_IMETHODIMP imgRequestProxy::GetLoadGroup(nsILoadGroup **loadGroup)
{
  NS_IF_ADDREF(*loadGroup = mLoadGroup.get());
  return NS_OK;
}
NS_IMETHODIMP imgRequestProxy::SetLoadGroup(nsILoadGroup *loadGroup)
{
  mLoadGroup = loadGroup;
  return NS_OK;
}

/* attribute nsLoadFlags loadFlags */
NS_IMETHODIMP imgRequestProxy::GetLoadFlags(nsLoadFlags *flags)
{
  *flags = mLoadFlags;
  return NS_OK;
}
NS_IMETHODIMP imgRequestProxy::SetLoadFlags(nsLoadFlags flags)
{
  mLoadFlags = flags;
  return NS_OK;
}

/**  imgIRequest methods **/

/* attribute imgIContainer image; */
NS_IMETHODIMP imgRequestProxy::GetImage(imgIContainer * *aImage)
{
  // It's possible that our owner has an image but hasn't notified us of it -
  // that'll happen if we get Canceled before the owner instantiates its image
  // (because Canceling unregisters us as a listener on mOwner). If we're
  // in that situation, just grab the image off of mOwner.
  imgIContainer* imageToReturn = GetImage();
  if (!imageToReturn && GetOwner())
    imageToReturn = GetOwner()->mImage.get();

  if (!imageToReturn)
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aImage = imageToReturn);

  return NS_OK;
}

/* readonly attribute unsigned long imageStatus; */
NS_IMETHODIMP imgRequestProxy::GetImageStatus(uint32_t *aStatus)
{
  *aStatus = GetStatusTracker().GetImageStatus();

  return NS_OK;
}

/* readonly attribute nsIURI URI; */
NS_IMETHODIMP imgRequestProxy::GetURI(nsIURI **aURI)
{
  if (!mURI)
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aURI = mURI);

  return NS_OK;
}

/* readonly attribute imgINotificationObserver notificationObserver; */
NS_IMETHODIMP imgRequestProxy::GetNotificationObserver(imgINotificationObserver **aObserver)
{
  *aObserver = mListener;
  NS_IF_ADDREF(*aObserver);
  return NS_OK;
}

/* readonly attribute string mimeType; */
NS_IMETHODIMP imgRequestProxy::GetMimeType(char **aMimeType)
{
  if (!GetOwner())
    return NS_ERROR_FAILURE;

  const char *type = GetOwner()->GetMimeType();
  if (!type)
    return NS_ERROR_FAILURE;

  *aMimeType = NS_strdup(type);

  return NS_OK;
}

static imgRequestProxy* NewProxy(imgRequestProxy* /*aThis*/)
{
  return new imgRequestProxy();
}

imgRequestProxy* NewStaticProxy(imgRequestProxy* aThis)
{
  nsCOMPtr<nsIPrincipal> currentPrincipal;
  aThis->GetImagePrincipal(getter_AddRefs(currentPrincipal));
  return new imgRequestProxyStatic(aThis->GetImage(), currentPrincipal);
}

NS_IMETHODIMP imgRequestProxy::Clone(imgINotificationObserver* aObserver,
                                     imgIRequest** aClone)
{
  nsresult result;
  imgRequestProxy* proxy;
  result = Clone(aObserver, &proxy);
  *aClone = proxy;
  return result;
}

nsresult imgRequestProxy::Clone(imgINotificationObserver* aObserver,
                                imgRequestProxy** aClone)
{
  return PerformClone(aObserver, NewProxy, aClone);
}

nsresult imgRequestProxy::PerformClone(imgINotificationObserver* aObserver,
                                       imgRequestProxy* (aAllocFn)(imgRequestProxy*),
                                       imgRequestProxy** aClone)
{
  NS_PRECONDITION(aClone, "Null out param");

  LOG_SCOPE(GetImgLog(), "imgRequestProxy::Clone");

  *aClone = nullptr;
  nsRefPtr<imgRequestProxy> clone = aAllocFn(this);

  // It is important to call |SetLoadFlags()| before calling |Init()| because
  // |Init()| adds the request to the loadgroup.
  // When a request is added to a loadgroup, its load flags are merged
  // with the load flags of the loadgroup.
  // XXXldb That's not true anymore.  Stuff from imgLoader adds the
  // request to the loadgroup.
  clone->SetLoadFlags(mLoadFlags);
  nsresult rv = clone->Init(&GetStatusTracker(), mLoadGroup, mURI, aObserver);
  if (NS_FAILED(rv))
    return rv;

  // Assign to *aClone before calling Notify so that if the caller expects to
  // only be notified for requests it's already holding pointers to it won't be
  // surprised.
  NS_ADDREF(*aClone = clone);

  // This is wrong!!! We need to notify asynchronously, but there's code that
  // assumes that we don't. This will be fixed in bug 580466.
  clone->SyncNotifyListener();

  return NS_OK;
}

/* readonly attribute nsIPrincipal imagePrincipal; */
NS_IMETHODIMP imgRequestProxy::GetImagePrincipal(nsIPrincipal **aPrincipal)
{
  if (!GetOwner())
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aPrincipal = GetOwner()->GetPrincipal());
  return NS_OK;
}

/* readonly attribute bool multipart; */
NS_IMETHODIMP imgRequestProxy::GetMultipart(bool *aMultipart)
{
  if (!GetOwner())
    return NS_ERROR_FAILURE;

  *aMultipart = GetOwner()->GetMultipart();

  return NS_OK;
}

/* readonly attribute int32_t CORSMode; */
NS_IMETHODIMP imgRequestProxy::GetCORSMode(int32_t* aCorsMode)
{
  if (!GetOwner())
    return NS_ERROR_FAILURE;

  *aCorsMode = GetOwner()->GetCORSMode();

  return NS_OK;
}

/** nsISupportsPriority methods **/

NS_IMETHODIMP imgRequestProxy::GetPriority(int32_t *priority)
{
  NS_ENSURE_STATE(GetOwner());
  *priority = GetOwner()->Priority();
  return NS_OK;
}

NS_IMETHODIMP imgRequestProxy::SetPriority(int32_t priority)
{
  NS_ENSURE_STATE(GetOwner() && !mCanceled);
  GetOwner()->AdjustPriority(this, priority - GetOwner()->Priority());
  return NS_OK;
}

NS_IMETHODIMP imgRequestProxy::AdjustPriority(int32_t priority)
{
  NS_ENSURE_STATE(GetOwner() && !mCanceled);
  GetOwner()->AdjustPriority(this, priority);
  return NS_OK;
}

/** nsISecurityInfoProvider methods **/

NS_IMETHODIMP imgRequestProxy::GetSecurityInfo(nsISupports** _retval)
{
  if (GetOwner())
    return GetOwner()->GetSecurityInfo(_retval);

  *_retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP imgRequestProxy::GetHasTransferredData(bool* hasData)
{
  if (GetOwner()) {
    *hasData = GetOwner()->HasTransferredData();
  } else {
    // The safe thing to do is to claim we have data
    *hasData = true;
  }
  return NS_OK;
}

/** imgDecoderObserver methods **/

void imgRequestProxy::OnStartContainer()
{
  LOG_FUNC(GetImgLog(), "imgRequestProxy::OnStartContainer");

  if (mListener && !mCanceled && !mSentStartContainer) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgINotificationObserver> kungFuDeathGrip(mListener);
    mListener->Notify(this, imgINotificationObserver::SIZE_AVAILABLE, nullptr);
    mSentStartContainer = true;
  }
}

void imgRequestProxy::OnFrameUpdate(const nsIntRect * rect)
{
  LOG_FUNC(GetImgLog(), "imgRequestProxy::OnDataAvailable");

  if (mListener && !mCanceled) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgINotificationObserver> kungFuDeathGrip(mListener);
    mListener->Notify(this, imgINotificationObserver::FRAME_UPDATE, rect);
  }
}

void imgRequestProxy::OnStopFrame()
{
  LOG_FUNC(GetImgLog(), "imgRequestProxy::OnStopFrame");

  if (mListener && !mCanceled) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgINotificationObserver> kungFuDeathGrip(mListener);
    mListener->Notify(this, imgINotificationObserver::FRAME_COMPLETE, nullptr);
  }
}

void imgRequestProxy::OnStopDecode()
{
  LOG_FUNC(GetImgLog(), "imgRequestProxy::OnStopDecode");

  if (mListener && !mCanceled) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgINotificationObserver> kungFuDeathGrip(mListener);
    mListener->Notify(this, imgINotificationObserver::DECODE_COMPLETE, nullptr);
  }

  // Multipart needs reset for next OnStartContainer
  if (GetOwner() && GetOwner()->GetMultipart())
    mSentStartContainer = false;
}

void imgRequestProxy::OnDiscard()
{
  LOG_FUNC(GetImgLog(), "imgRequestProxy::OnDiscard");

  if (mListener && !mCanceled) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgINotificationObserver> kungFuDeathGrip(mListener);
    mListener->Notify(this, imgINotificationObserver::DISCARD, nullptr);
  }
}

void imgRequestProxy::OnImageIsAnimated()
{
  LOG_FUNC(GetImgLog(), "imgRequestProxy::OnImageIsAnimated");
  if (mListener && !mCanceled) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgINotificationObserver> kungFuDeathGrip(mListener);
    mListener->Notify(this, imgINotificationObserver::IS_ANIMATED, nullptr);
  }
}

void imgRequestProxy::OnStartRequest()
{
#ifdef PR_LOGGING
  nsAutoCString name;
  GetName(name);
  LOG_FUNC_WITH_PARAM(GetImgLog(), "imgRequestProxy::OnStartRequest", "name", name.get());
#endif
}

void imgRequestProxy::OnStopRequest(bool lastPart)
{
#ifdef PR_LOGGING
  nsAutoCString name;
  GetName(name);
  LOG_FUNC_WITH_PARAM(GetImgLog(), "imgRequestProxy::OnStopRequest", "name", name.get());
#endif
  // There's all sorts of stuff here that could kill us (the OnStopRequest call
  // on the listener, the removal from the loadgroup, the release of the
  // listener, etc).  Don't let them do it.
  nsCOMPtr<imgIRequest> kungFuDeathGrip(this);

  if (mListener) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgINotificationObserver> kungFuDeathGrip(mListener);
    mListener->Notify(this, imgINotificationObserver::LOAD_COMPLETE, nullptr);
  }

  // If we're expecting more data from a multipart channel, re-add ourself
  // to the loadgroup so that the document doesn't lose track of the load.
  // If the request is already a background request and there's more data
  // coming, we can just leave the request in the loadgroup as-is.
  if (lastPart || (mLoadFlags & nsIRequest::LOAD_BACKGROUND) == 0) {
    RemoveFromLoadGroup(lastPart);
    // More data is coming, so change the request to be a background request
    // and put it back in the loadgroup.
    if (!lastPart) {
      mLoadFlags |= nsIRequest::LOAD_BACKGROUND;
      AddToLoadGroup();
    }
  }

  if (mListenerIsStrongRef) {
    NS_PRECONDITION(mListener, "How did that happen?");
    // Drop our strong ref to the listener now that we're done with
    // everything.  Note that this can cancel us and other fun things
    // like that.  Don't add anything in this method after this point.
    imgINotificationObserver* obs = mListener;
    mListenerIsStrongRef = false;
    NS_RELEASE(obs);
  }
}

void imgRequestProxy::BlockOnload()
{
#ifdef PR_LOGGING
  nsAutoCString name;
  GetName(name);
  LOG_FUNC_WITH_PARAM(GetImgLog(), "imgRequestProxy::BlockOnload", "name", name.get());
#endif

  nsCOMPtr<imgIOnloadBlocker> blocker = do_QueryInterface(mListener);
  if (blocker) {
    blocker->BlockOnload(this);
  }
}

void imgRequestProxy::UnblockOnload()
{
#ifdef PR_LOGGING
  nsAutoCString name;
  GetName(name);
  LOG_FUNC_WITH_PARAM(GetImgLog(), "imgRequestProxy::UnblockOnload", "name", name.get());
#endif

  nsCOMPtr<imgIOnloadBlocker> blocker = do_QueryInterface(mListener);
  if (blocker) {
    blocker->UnblockOnload(this);
  }
}

void imgRequestProxy::NullOutListener()
{
  // If we have animation consumers, then they don't matter anymore
  if (mListener)
    ClearAnimationConsumers();

  if (mListenerIsStrongRef) {
    // Releasing could do weird reentery stuff, so just play it super-safe
    nsCOMPtr<imgINotificationObserver> obs;
    obs.swap(mListener);
    mListenerIsStrongRef = false;
  } else {
    mListener = nullptr;
  }
}

NS_IMETHODIMP
imgRequestProxy::GetStaticRequest(imgIRequest** aReturn)
{
  imgRequestProxy *proxy;
  nsresult result = GetStaticRequest(&proxy);
  *aReturn = proxy;
  return result;
}

nsresult
imgRequestProxy::GetStaticRequest(imgRequestProxy** aReturn)
{
  *aReturn = nullptr;
  mozilla::image::Image* image = GetImage();

  bool animated;
  if (!image || (NS_SUCCEEDED(image->GetAnimated(&animated)) && !animated)) {
    // Early exit - we're not animated, so we don't have to do anything.
    NS_ADDREF(*aReturn = this);
    return NS_OK;
  }

  // We are animated. We need to extract the current frame from this image.
  int32_t w = 0;
  int32_t h = 0;
  image->GetWidth(&w);
  image->GetHeight(&h);
  nsIntRect rect(0, 0, w, h);
  nsCOMPtr<imgIContainer> currentFrame;
  nsresult rv = image->ExtractFrame(imgIContainer::FRAME_CURRENT, rect,
                                    imgIContainer::FLAG_SYNC_DECODE,
                                    getter_AddRefs(currentFrame));
  if (NS_FAILED(rv))
    return rv;

  nsRefPtr<Image> frame = static_cast<Image*>(currentFrame.get());

  // Create a static imgRequestProxy with our new extracted frame.
  nsCOMPtr<nsIPrincipal> currentPrincipal;
  GetImagePrincipal(getter_AddRefs(currentPrincipal));
  nsRefPtr<imgRequestProxy> req = new imgRequestProxyStatic(frame, currentPrincipal);
  req->Init(&frame->GetStatusTracker(), nullptr, mURI, nullptr);

  NS_ADDREF(*aReturn = req);

  return NS_OK;
}

void imgRequestProxy::NotifyListener()
{
  // It would be nice to notify the observer directly in the status tracker
  // instead of through the proxy, but there are several places we do extra
  // processing when we receive notifications (like OnStopRequest()), and we
  // need to check mCanceled everywhere too.

  if (GetOwner()) {
    // Send the notifications to our listener asynchronously.
    GetStatusTracker().Notify(GetOwner(), this);
  } else {
    // We don't have an imgRequest, so we can only notify the clone of our
    // current state, but we still have to do that asynchronously.
    NS_ABORT_IF_FALSE(GetImage(),
                      "if we have no imgRequest, we should have an Image");
    GetStatusTracker().NotifyCurrentState(this);
  }
}

void imgRequestProxy::SyncNotifyListener()
{
  // It would be nice to notify the observer directly in the status tracker
  // instead of through the proxy, but there are several places we do extra
  // processing when we receive notifications (like OnStopRequest()), and we
  // need to check mCanceled everywhere too.

  GetStatusTracker().SyncNotify(this);
}

void
imgRequestProxy::SetHasImage()
{
  Image* image = GetStatusTracker().GetImage();

  // Force any private status related to the owner to reflect
  // the presence of an image;
  mBehaviour->SetOwner(mBehaviour->GetOwner());

  // Apply any locks we have
  for (uint32_t i = 0; i < mLockCount; ++i)
    image->LockImage();

  // Apply any animation consumers we have
  for (uint32_t i = 0; i < mAnimationConsumers; i++)
    image->IncrementAnimationConsumers();
}

imgStatusTracker&
imgRequestProxy::GetStatusTracker() const
{
  return mBehaviour->GetStatusTracker();
}

mozilla::image::Image*
imgRequestProxy::GetImage() const
{
  return mBehaviour->GetImage();
}

imgRequest*
imgRequestProxy::GetOwner() const
{
  return mBehaviour->GetOwner();
}

////////////////// imgRequestProxyStatic methods

class StaticBehaviour : public ProxyBehaviour
{
public:
  StaticBehaviour(mozilla::image::Image* aImage) : mImage(aImage) {}

  virtual mozilla::image::Image* GetImage() const MOZ_OVERRIDE {
    return mImage;
  }

  virtual imgStatusTracker& GetStatusTracker() const MOZ_OVERRIDE {
    return mImage->GetStatusTracker();
  }

  virtual imgRequest* GetOwner() const MOZ_OVERRIDE {
    return nullptr;
  }

  virtual void SetOwner(imgRequest* aOwner) MOZ_OVERRIDE {
    MOZ_ASSERT_IF(aOwner, "We shouldn't be giving static requests a non-null owner.");
  }

private:
  // Our image. We have to hold a strong reference here, because that's normally
  // the job of the underlying request.
  nsRefPtr<mozilla::image::Image> mImage;
};

imgRequestProxyStatic::imgRequestProxyStatic(mozilla::image::Image* aImage,
                                             nsIPrincipal* aPrincipal)
: mPrincipal(aPrincipal)
{
  mBehaviour = new StaticBehaviour(aImage);
}

NS_IMETHODIMP imgRequestProxyStatic::GetImagePrincipal(nsIPrincipal **aPrincipal)
{
  if (!mPrincipal)
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aPrincipal = mPrincipal);

  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxyStatic::Clone(imgINotificationObserver* aObserver,
                             imgIRequest** aClone)
{
  nsresult result;
  imgRequestProxy* proxy;
  result = PerformClone(aObserver, NewStaticProxy, &proxy);
  *aClone = proxy;
  return result;
}
