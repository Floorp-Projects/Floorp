/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageLogging.h"
#include "imgRequestProxy.h"
#include "imgIOnloadBlocker.h"
#include "imgLoader.h"
#include "Image.h"
#include "ImageOps.h"
#include "nsError.h"
#include "nsCRTGlue.h"
#include "imgINotificationObserver.h"

using namespace mozilla::image;

// The split of imgRequestProxy and imgRequestProxyStatic means that
// certain overridden functions need to be usable in the destructor.
// Since virtual functions can't be used in that way, this class
// provides a behavioural trait for each class to use instead.
class ProxyBehaviour
{
 public:
  virtual ~ProxyBehaviour() = default;

  virtual already_AddRefed<mozilla::image::Image> GetImage() const = 0;
  virtual bool HasImage() const = 0;
  virtual already_AddRefed<ProgressTracker> GetProgressTracker() const = 0;
  virtual imgRequest* GetOwner() const = 0;
  virtual void SetOwner(imgRequest* aOwner) = 0;
};

class RequestBehaviour : public ProxyBehaviour
{
 public:
  RequestBehaviour() : mOwner(nullptr), mOwnerHasImage(false) {}

  virtual already_AddRefed<mozilla::image::Image>GetImage() const override;
  virtual bool HasImage() const override;
  virtual already_AddRefed<ProgressTracker> GetProgressTracker() const override;

  virtual imgRequest* GetOwner() const override {
    return mOwner;
  }

  virtual void SetOwner(imgRequest* aOwner) override {
    mOwner = aOwner;

    if (mOwner) {
      RefPtr<ProgressTracker> ownerProgressTracker = GetProgressTracker();
      mOwnerHasImage = ownerProgressTracker && ownerProgressTracker->HasImage();
    } else {
      mOwnerHasImage = false;
    }
  }

 private:
  // We maintain the following invariant:
  // The proxy is registered at most with a single imgRequest as an observer,
  // and whenever it is, mOwner points to that object. This helps ensure that
  // imgRequestProxy::~imgRequestProxy unregisters the proxy as an observer
  // from whatever request it was registered with (if any). This, in turn,
  // means that imgRequest::mObservers will not have any stale pointers in it.
  RefPtr<imgRequest> mOwner;

  bool mOwnerHasImage;
};

already_AddRefed<mozilla::image::Image>
RequestBehaviour::GetImage() const
{
  if (!mOwnerHasImage) {
    return nullptr;
  }
  RefPtr<ProgressTracker> progressTracker = GetProgressTracker();
  return progressTracker->GetImage();
}

already_AddRefed<ProgressTracker>
RequestBehaviour::GetProgressTracker() const
{
  // NOTE: It's possible that our mOwner has an Image that it didn't notify
  // us about, if we were Canceled before its Image was constructed.
  // (Canceling removes us as an observer, so mOwner has no way to notify us).
  // That's why this method uses mOwner->GetProgressTracker() instead of just
  // mOwner->mProgressTracker -- we might have a null mImage and yet have an
  // mOwner with a non-null mImage (and a null mProgressTracker pointer).
  return mOwner->GetProgressTracker();
}

NS_IMPL_ADDREF(imgRequestProxy)
NS_IMPL_RELEASE(imgRequestProxy)

NS_INTERFACE_MAP_BEGIN(imgRequestProxy)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, imgIRequest)
  NS_INTERFACE_MAP_ENTRY(imgIRequest)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_INTERFACE_MAP_ENTRY(nsISupportsPriority)
  NS_INTERFACE_MAP_ENTRY(nsISecurityInfoProvider)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsITimedChannel,
                                     TimedChannel() != nullptr)
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
  mDeferNotifications(false)
{
  /* member initializers and constructor code */

}

imgRequestProxy::~imgRequestProxy()
{
  /* destructor code */
  NS_PRECONDITION(!mListener,
                  "Someone forgot to properly cancel this request!");

  // Unlock the image the proper number of times if we're holding locks on
  // it. Note that UnlockImage() decrements mLockCount each time it's called.
  while (mLockCount) {
    UnlockImage();
  }

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

nsresult
imgRequestProxy::Init(imgRequest* aOwner,
                      nsILoadGroup* aLoadGroup,
                      ImageURL* aURI,
                      imgINotificationObserver* aObserver)
{
  NS_PRECONDITION(!GetOwner() && !mListener,
                  "imgRequestProxy is already initialized");

  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequestProxy::Init", "request",
                       aOwner);

  MOZ_ASSERT(mAnimationConsumers == 0, "Cannot have animation before Init");

  mBehaviour->SetOwner(aOwner);
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
  if (GetOwner()) {
    GetOwner()->AddProxy(this);
  }

  return NS_OK;
}

nsresult
imgRequestProxy::ChangeOwner(imgRequest* aNewOwner)
{
  NS_PRECONDITION(GetOwner(),
                  "Cannot ChangeOwner on a proxy without an owner!");

  if (mCanceled) {
    // Ensure that this proxy has received all notifications to date
    // before we clean it up when removing it from the old owner below.
    SyncNotifyListener();
  }

  // If we're holding locks, unlock the old image.
  // Note that UnlockImage decrements mLockCount each time it's called.
  uint32_t oldLockCount = mLockCount;
  while (mLockCount) {
    UnlockImage();
  }

  // If we're holding animation requests, undo them.
  uint32_t oldAnimationConsumers = mAnimationConsumers;
  ClearAnimationConsumers();

  GetOwner()->RemoveProxy(this, NS_IMAGELIB_CHANGING_OWNER);

  mBehaviour->SetOwner(aNewOwner);

  // If we were locked, apply the locks here
  for (uint32_t i = 0; i < oldLockCount; i++) {
    LockImage();
  }

  // If we had animation requests, restore them here. Note that we
  // do this *after* RemoveProxy, which clears out animation consumers
  // (see bug 601723).
  for (uint32_t i = 0; i < oldAnimationConsumers; i++) {
    IncrementAnimationConsumers();
  }

  GetOwner()->AddProxy(this);

  // If we'd previously requested a synchronous decode, request a decode on the
  // new image.
  if (mDecodeRequested) {
    StartDecoding();
  }

  return NS_OK;
}

void
imgRequestProxy::AddToLoadGroup()
{
  NS_ASSERTION(!mIsInLoadGroup, "Whaa, we're already in the loadgroup!");

  if (!mIsInLoadGroup && mLoadGroup) {
    mLoadGroup->AddRequest(this, nullptr);
    mIsInLoadGroup = true;
  }
}

void
imgRequestProxy::RemoveFromLoadGroup(bool releaseLoadGroup)
{
  if (!mIsInLoadGroup) {
    return;
  }

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

NS_IMETHODIMP
imgRequestProxy::GetName(nsACString& aName)
{
  aName.Truncate();

  if (mURI) {
    mURI->GetSpec(aName);
  }

  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::IsPending(bool* _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
imgRequestProxy::GetStatus(nsresult* aStatus)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
imgRequestProxy::Cancel(nsresult status)
{
  if (mCanceled) {
    return NS_ERROR_FAILURE;
  }

  LOG_SCOPE(gImgLog, "imgRequestProxy::Cancel");

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

NS_IMETHODIMP
imgRequestProxy::CancelAndForgetObserver(nsresult aStatus)
{
  // If mCanceled is true but mListener is non-null, that means
  // someone called Cancel() on us but the imgCancelRunnable is still
  // pending.  We still need to null out mListener before returning
  // from this function in this case.  That means we want to do the
  // RemoveProxy call right now, because we need to deliver the
  // onStopRequest.
  if (mCanceled && !mListener) {
    return NS_ERROR_FAILURE;
  }

  LOG_SCOPE(gImgLog, "imgRequestProxy::CancelAndForgetObserver");

  mCanceled = true;

  // Now cheat and make sure our removal from loadgroup happens async
  bool oldIsInLoadGroup = mIsInLoadGroup;
  mIsInLoadGroup = false;

  if (GetOwner()) {
    GetOwner()->RemoveProxy(this, aStatus);
  }

  mIsInLoadGroup = oldIsInLoadGroup;

  if (mIsInLoadGroup) {
    NS_DispatchToCurrentThread(NewRunnableMethod(this, &imgRequestProxy::DoRemoveFromLoadGroup));
  }

  NullOutListener();

  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::StartDecoding()
{
  // Flag this, so we know to transfer the request if our owner changes
  mDecodeRequested = true;

  RefPtr<Image> image = GetImage();
  if (image) {
    return image->StartDecoding();
  }

  if (GetOwner()) {
    GetOwner()->StartDecoding();
  }

  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::LockImage()
{
  mLockCount++;
  RefPtr<Image> image = GetImage();
  if (image) {
    return image->LockImage();
  }
  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::UnlockImage()
{
  MOZ_ASSERT(mLockCount > 0, "calling unlock but no locks!");

  mLockCount--;
  RefPtr<Image> image = GetImage();
  if (image) {
    return image->UnlockImage();
  }
  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::RequestDiscard()
{
  RefPtr<Image> image = GetImage();
  if (image) {
    return image->RequestDiscard();
  }
  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::IncrementAnimationConsumers()
{
  mAnimationConsumers++;
  RefPtr<Image> image = GetImage();
  if (image) {
    image->IncrementAnimationConsumers();
  }
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
    RefPtr<Image> image = GetImage();
    if (image) {
      image->DecrementAnimationConsumers();
    }
  }
  return NS_OK;
}

void
imgRequestProxy::ClearAnimationConsumers()
{
  while (mAnimationConsumers > 0) {
    DecrementAnimationConsumers();
  }
}

NS_IMETHODIMP
imgRequestProxy::Suspend()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
imgRequestProxy::Resume()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
imgRequestProxy::GetLoadGroup(nsILoadGroup** loadGroup)
{
  NS_IF_ADDREF(*loadGroup = mLoadGroup.get());
  return NS_OK;
}
NS_IMETHODIMP
imgRequestProxy::SetLoadGroup(nsILoadGroup* loadGroup)
{
  mLoadGroup = loadGroup;
  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::GetLoadFlags(nsLoadFlags* flags)
{
  *flags = mLoadFlags;
  return NS_OK;
}
NS_IMETHODIMP
imgRequestProxy::SetLoadFlags(nsLoadFlags flags)
{
  mLoadFlags = flags;
  return NS_OK;
}

/**  imgIRequest methods **/

NS_IMETHODIMP
imgRequestProxy::GetImage(imgIContainer** aImage)
{
  NS_ENSURE_TRUE(aImage, NS_ERROR_NULL_POINTER);
  // It's possible that our owner has an image but hasn't notified us of it -
  // that'll happen if we get Canceled before the owner instantiates its image
  // (because Canceling unregisters us as a listener on mOwner). If we're
  // in that situation, just grab the image off of mOwner.
  RefPtr<Image> image = GetImage();
  nsCOMPtr<imgIContainer> imageToReturn;
  if (image) {
    imageToReturn = do_QueryInterface(image);
  }
  if (!imageToReturn && GetOwner()) {
    imageToReturn = GetOwner()->GetImage();
  }
  if (!imageToReturn) {
    return NS_ERROR_FAILURE;
  }

  imageToReturn.swap(*aImage);

  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::GetImageStatus(uint32_t* aStatus)
{
  RefPtr<ProgressTracker> progressTracker = GetProgressTracker();
  *aStatus = progressTracker->GetImageStatus();

  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::GetImageErrorCode(nsresult* aStatus)
{
  if (!GetOwner()) {
    return NS_ERROR_FAILURE;
  }

  *aStatus = GetOwner()->GetImageErrorCode();

  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::GetURI(nsIURI** aURI)
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread to convert URI");
  nsCOMPtr<nsIURI> uri = mURI->ToIURI();
  uri.forget(aURI);
  return NS_OK;
}

nsresult
imgRequestProxy::GetCurrentURI(nsIURI** aURI)
{
  if (!GetOwner()) {
    return NS_ERROR_FAILURE;
  }

  return GetOwner()->GetCurrentURI(aURI);
}

nsresult
imgRequestProxy::GetURI(ImageURL** aURI)
{
  if (!mURI) {
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aURI = mURI);

  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::GetNotificationObserver(imgINotificationObserver** aObserver)
{
  *aObserver = mListener;
  NS_IF_ADDREF(*aObserver);
  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::GetMimeType(char** aMimeType)
{
  if (!GetOwner()) {
    return NS_ERROR_FAILURE;
  }

  const char* type = GetOwner()->GetMimeType();
  if (!type) {
    return NS_ERROR_FAILURE;
  }

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
  RefPtr<Image> image = aThis->GetImage();
  return new imgRequestProxyStatic(image, currentPrincipal);
}

NS_IMETHODIMP
imgRequestProxy::Clone(imgINotificationObserver* aObserver,
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

nsresult
imgRequestProxy::PerformClone(imgINotificationObserver* aObserver,
                              imgRequestProxy* (aAllocFn)(imgRequestProxy*),
                              imgRequestProxy** aClone)
{
  NS_PRECONDITION(aClone, "Null out param");

  LOG_SCOPE(gImgLog, "imgRequestProxy::Clone");

  *aClone = nullptr;
  RefPtr<imgRequestProxy> clone = aAllocFn(this);

  // It is important to call |SetLoadFlags()| before calling |Init()| because
  // |Init()| adds the request to the loadgroup.
  // When a request is added to a loadgroup, its load flags are merged
  // with the load flags of the loadgroup.
  // XXXldb That's not true anymore.  Stuff from imgLoader adds the
  // request to the loadgroup.
  clone->SetLoadFlags(mLoadFlags);
  nsresult rv = clone->Init(mBehaviour->GetOwner(), mLoadGroup,
                            mURI, aObserver);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (GetOwner() && GetOwner()->GetValidator()) {
    clone->SetNotificationsDeferred(true);
    GetOwner()->GetValidator()->AddProxy(clone);
  }

  // Assign to *aClone before calling Notify so that if the caller expects to
  // only be notified for requests it's already holding pointers to it won't be
  // surprised.
  NS_ADDREF(*aClone = clone);

  // This is wrong!!! We need to notify asynchronously, but there's code that
  // assumes that we don't. This will be fixed in bug 580466.
  clone->SyncNotifyListener();

  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::GetImagePrincipal(nsIPrincipal** aPrincipal)
{
  if (!GetOwner()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPrincipal> principal = GetOwner()->GetPrincipal();
  principal.forget(aPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::GetMultipart(bool* aMultipart)
{
  if (!GetOwner()) {
    return NS_ERROR_FAILURE;
  }

  *aMultipart = GetOwner()->GetMultipart();

  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::GetCORSMode(int32_t* aCorsMode)
{
  if (!GetOwner()) {
    return NS_ERROR_FAILURE;
  }

  *aCorsMode = GetOwner()->GetCORSMode();

  return NS_OK;
}

/** nsISupportsPriority methods **/

NS_IMETHODIMP
imgRequestProxy::GetPriority(int32_t* priority)
{
  NS_ENSURE_STATE(GetOwner());
  *priority = GetOwner()->Priority();
  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::SetPriority(int32_t priority)
{
  NS_ENSURE_STATE(GetOwner() && !mCanceled);
  GetOwner()->AdjustPriority(this, priority - GetOwner()->Priority());
  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::AdjustPriority(int32_t priority)
{
  // We don't require |!mCanceled| here. This may be called even if we're
  // cancelled, because it's invoked as part of the process of removing an image
  // from the load group.
  NS_ENSURE_STATE(GetOwner());
  GetOwner()->AdjustPriority(this, priority);
  return NS_OK;
}

/** nsISecurityInfoProvider methods **/

NS_IMETHODIMP
imgRequestProxy::GetSecurityInfo(nsISupports** _retval)
{
  if (GetOwner()) {
    return GetOwner()->GetSecurityInfo(_retval);
  }

  *_retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
imgRequestProxy::GetHasTransferredData(bool* hasData)
{
  if (GetOwner()) {
    *hasData = GetOwner()->HasTransferredData();
  } else {
    // The safe thing to do is to claim we have data
    *hasData = true;
  }
  return NS_OK;
}

static const char*
NotificationTypeToString(int32_t aType)
{
  switch(aType)
  {
    case imgINotificationObserver::SIZE_AVAILABLE: return "SIZE_AVAILABLE";
    case imgINotificationObserver::FRAME_UPDATE: return "FRAME_UPDATE";
    case imgINotificationObserver::FRAME_COMPLETE: return "FRAME_COMPLETE";
    case imgINotificationObserver::LOAD_COMPLETE: return "LOAD_COMPLETE";
    case imgINotificationObserver::DECODE_COMPLETE: return "DECODE_COMPLETE";
    case imgINotificationObserver::DISCARD: return "DISCARD";
    case imgINotificationObserver::UNLOCKED_DRAW: return "UNLOCKED_DRAW";
    case imgINotificationObserver::IS_ANIMATED: return "IS_ANIMATED";
    case imgINotificationObserver::HAS_TRANSPARENCY: return "HAS_TRANSPARENCY";
    default:
      NS_NOTREACHED("Notification list should be exhaustive");
      return "(unknown notification)";
  }
}

void
imgRequestProxy::Notify(int32_t aType, const mozilla::gfx::IntRect* aRect)
{
  MOZ_ASSERT(aType != imgINotificationObserver::LOAD_COMPLETE,
             "Should call OnLoadComplete");

  LOG_FUNC_WITH_PARAM(gImgLog, "imgRequestProxy::Notify", "type",
                      NotificationTypeToString(aType));

  if (!mListener || mCanceled) {
    return;
  }

  // Make sure the listener stays alive while we notify.
  nsCOMPtr<imgINotificationObserver> listener(mListener);

  listener->Notify(this, aType, aRect);
}

void
imgRequestProxy::OnLoadComplete(bool aLastPart)
{
  if (MOZ_LOG_TEST(gImgLog, LogLevel::Debug)) {
    nsAutoCString name;
    GetName(name);
    LOG_FUNC_WITH_PARAM(gImgLog, "imgRequestProxy::OnLoadComplete",
                        "name", name.get());
  }

  // There's all sorts of stuff here that could kill us (the OnStopRequest call
  // on the listener, the removal from the loadgroup, the release of the
  // listener, etc).  Don't let them do it.
  nsCOMPtr<imgIRequest> kungFuDeathGrip(this);

  if (mListener && !mCanceled) {
    // Hold a ref to the listener while we call it, just in case.
    nsCOMPtr<imgINotificationObserver> listener(mListener);
    listener->Notify(this, imgINotificationObserver::LOAD_COMPLETE, nullptr);
  }

  // If we're expecting more data from a multipart channel, re-add ourself
  // to the loadgroup so that the document doesn't lose track of the load.
  // If the request is already a background request and there's more data
  // coming, we can just leave the request in the loadgroup as-is.
  if (aLastPart || (mLoadFlags & nsIRequest::LOAD_BACKGROUND) == 0) {
    RemoveFromLoadGroup(aLastPart);
    // More data is coming, so change the request to be a background request
    // and put it back in the loadgroup.
    if (!aLastPart) {
      mLoadFlags |= nsIRequest::LOAD_BACKGROUND;
      AddToLoadGroup();
    }
  }

  if (mListenerIsStrongRef && aLastPart) {
    NS_PRECONDITION(mListener, "How did that happen?");
    // Drop our strong ref to the listener now that we're done with
    // everything.  Note that this can cancel us and other fun things
    // like that.  Don't add anything in this method after this point.
    imgINotificationObserver* obs = mListener;
    mListenerIsStrongRef = false;
    NS_RELEASE(obs);
  }
}

void
imgRequestProxy::BlockOnload()
{
  if (MOZ_LOG_TEST(gImgLog, LogLevel::Debug)) {
    nsAutoCString name;
    GetName(name);
    LOG_FUNC_WITH_PARAM(gImgLog, "imgRequestProxy::BlockOnload",
                        "name", name.get());
  }

  nsCOMPtr<imgIOnloadBlocker> blocker = do_QueryInterface(mListener);
  if (blocker) {
    blocker->BlockOnload(this);
  }
}

void
imgRequestProxy::UnblockOnload()
{
  if (MOZ_LOG_TEST(gImgLog, LogLevel::Debug)) {
    nsAutoCString name;
    GetName(name);
    LOG_FUNC_WITH_PARAM(gImgLog, "imgRequestProxy::UnblockOnload",
                        "name", name.get());
  }

  nsCOMPtr<imgIOnloadBlocker> blocker = do_QueryInterface(mListener);
  if (blocker) {
    blocker->UnblockOnload(this);
  }
}

void
imgRequestProxy::NullOutListener()
{
  // If we have animation consumers, then they don't matter anymore
  if (mListener) {
    ClearAnimationConsumers();
  }

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
  imgRequestProxy* proxy;
  nsresult result = GetStaticRequest(&proxy);
  *aReturn = proxy;
  return result;
}

nsresult
imgRequestProxy::GetStaticRequest(imgRequestProxy** aReturn)
{
  *aReturn = nullptr;
  RefPtr<Image> image = GetImage();

  bool animated;
  if (!image || (NS_SUCCEEDED(image->GetAnimated(&animated)) && !animated)) {
    // Early exit - we're not animated, so we don't have to do anything.
    NS_ADDREF(*aReturn = this);
    return NS_OK;
  }

  // Check for errors in the image. Callers code rely on GetStaticRequest
  // failing in this case, though with FrozenImage there's no technical reason
  // for it anymore.
  if (image->HasError()) {
    return NS_ERROR_FAILURE;
  }

  // We are animated. We need to create a frozen version of this image.
  RefPtr<Image> frozenImage = ImageOps::Freeze(image);

  // Create a static imgRequestProxy with our new extracted frame.
  nsCOMPtr<nsIPrincipal> currentPrincipal;
  GetImagePrincipal(getter_AddRefs(currentPrincipal));
  RefPtr<imgRequestProxy> req = new imgRequestProxyStatic(frozenImage,
                                                            currentPrincipal);
  req->Init(nullptr, nullptr, mURI, nullptr);

  NS_ADDREF(*aReturn = req);

  return NS_OK;
}

void
imgRequestProxy::NotifyListener()
{
  // It would be nice to notify the observer directly in the status tracker
  // instead of through the proxy, but there are several places we do extra
  // processing when we receive notifications (like OnStopRequest()), and we
  // need to check mCanceled everywhere too.

  RefPtr<ProgressTracker> progressTracker = GetProgressTracker();
  if (GetOwner()) {
    // Send the notifications to our listener asynchronously.
    progressTracker->Notify(this);
  } else {
    // We don't have an imgRequest, so we can only notify the clone of our
    // current state, but we still have to do that asynchronously.
    MOZ_ASSERT(HasImage(),
               "if we have no imgRequest, we should have an Image");
    progressTracker->NotifyCurrentState(this);
  }
}

void
imgRequestProxy::SyncNotifyListener()
{
  // It would be nice to notify the observer directly in the status tracker
  // instead of through the proxy, but there are several places we do extra
  // processing when we receive notifications (like OnStopRequest()), and we
  // need to check mCanceled everywhere too.

  RefPtr<ProgressTracker> progressTracker = GetProgressTracker();
  progressTracker->SyncNotify(this);
}

void
imgRequestProxy::SetHasImage()
{
  RefPtr<ProgressTracker> progressTracker = GetProgressTracker();
  MOZ_ASSERT(progressTracker);
  RefPtr<Image> image = progressTracker->GetImage();
  MOZ_ASSERT(image);

  // Force any private status related to the owner to reflect
  // the presence of an image;
  mBehaviour->SetOwner(mBehaviour->GetOwner());

  // Apply any locks we have
  for (uint32_t i = 0; i < mLockCount; ++i) {
    image->LockImage();
  }

  // Apply any animation consumers we have
  for (uint32_t i = 0; i < mAnimationConsumers; i++) {
    image->IncrementAnimationConsumers();
  }
}

already_AddRefed<ProgressTracker>
imgRequestProxy::GetProgressTracker() const
{
  return mBehaviour->GetProgressTracker();
}

already_AddRefed<mozilla::image::Image>
imgRequestProxy::GetImage() const
{
  return mBehaviour->GetImage();
}

bool
RequestBehaviour::HasImage() const
{
  if (!mOwnerHasImage) {
    return false;
  }
  RefPtr<ProgressTracker> progressTracker = GetProgressTracker();
  return progressTracker ? progressTracker->HasImage() : false;
}

bool
imgRequestProxy::HasImage() const
{
  return mBehaviour->HasImage();
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
  explicit StaticBehaviour(mozilla::image::Image* aImage) : mImage(aImage) {}

  virtual already_AddRefed<mozilla::image::Image>
  GetImage() const override {
    RefPtr<mozilla::image::Image> image = mImage;
    return image.forget();
  }

  virtual bool HasImage() const override {
    return mImage;
  }

  virtual already_AddRefed<ProgressTracker> GetProgressTracker()
    const override  {
    return mImage->GetProgressTracker();
  }

  virtual imgRequest* GetOwner() const override {
    return nullptr;
  }

  virtual void SetOwner(imgRequest* aOwner) override {
    MOZ_ASSERT(!aOwner,
               "We shouldn't be giving static requests a non-null owner.");
  }

private:
  // Our image. We have to hold a strong reference here, because that's normally
  // the job of the underlying request.
  RefPtr<mozilla::image::Image> mImage;
};

imgRequestProxyStatic::imgRequestProxyStatic(mozilla::image::Image* aImage,
                                             nsIPrincipal* aPrincipal)
: mPrincipal(aPrincipal)
{
  mBehaviour = mozilla::MakeUnique<StaticBehaviour>(aImage);
}

NS_IMETHODIMP
imgRequestProxyStatic::GetImagePrincipal(nsIPrincipal** aPrincipal)
{
  if (!mPrincipal) {
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aPrincipal = mPrincipal);

  return NS_OK;
}

nsresult
imgRequestProxyStatic::Clone(imgINotificationObserver* aObserver,
                             imgRequestProxy** aClone)
{
  return PerformClone(aObserver, NewStaticProxy, aClone);
}
