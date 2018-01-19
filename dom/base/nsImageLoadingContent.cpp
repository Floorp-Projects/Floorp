/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A base class which implements nsIImageLoadingContent and can be
 * subclassed by various content nodes that want to provide image
 * loading functionality (eg <img>, <object>, etc).
 */

#include "nsImageLoadingContent.h"
#include "nsError.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsContentPolicyUtils.h"
#include "nsIURI.h"
#include "nsILoadGroup.h"
#include "imgIContainer.h"
#include "imgLoader.h"
#include "imgRequestProxy.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsImageFrame.h"
#include "nsSVGImageFrame.h"

#include "nsIPresShell.h"

#include "nsIChannel.h"
#include "nsIStreamListener.h"

#include "nsIFrame.h"
#include "nsIDOMNode.h"

#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsIContentPolicy.h"
#include "SVGObserverUtils.h"

#include "gfxPrefs.h"

#include "mozAutoDocUpdate.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ImageTracker.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/Preferences.h"

#ifdef LoadImage
// Undefine LoadImage to prevent naming conflict with Windows.
#undef LoadImage
#endif

using namespace mozilla;
using namespace mozilla::dom;

#ifdef DEBUG_chb
static void PrintReqURL(imgIRequest* req) {
  if (!req) {
    printf("(null req)\n");
    return;
  }

  nsCOMPtr<nsIURI> uri;
  req->GetURI(getter_AddRefs(uri));
  if (!uri) {
    printf("(null uri)\n");
    return;
  }

  nsAutoCString spec;
  uri->GetSpec(spec);
  printf("spec='%s'\n", spec.get());
}
#endif /* DEBUG_chb */


nsImageLoadingContent::nsImageLoadingContent()
  : mCurrentRequestFlags(0),
    mPendingRequestFlags(0),
    mObserverList(nullptr),
    mImageBlockingStatus(nsIContentPolicy::ACCEPT),
    mLoadingEnabled(true),
    mIsImageStateForced(false),
    mLoading(false),
    // mBroken starts out true, since an image without a URI is broken....
    mBroken(true),
    mUserDisabled(false),
    mSuppressed(false),
    mNewRequestsWillNeedAnimationReset(false),
    mUseUrgentStartForChannel(false),
    mStateChangerDepth(0),
    mCurrentRequestRegistered(false),
    mPendingRequestRegistered(false),
    mIsStartingImageLoad(false)
{
  if (!nsContentUtils::GetImgLoaderForChannel(nullptr, nullptr)) {
    mLoadingEnabled = false;
  }

  mMostRecentRequestChange = TimeStamp::ProcessCreation();
}

void
nsImageLoadingContent::DestroyImageLoadingContent()
{
  // Cancel our requests so they won't hold stale refs to us
  // NB: Don't ask to discard the images here.
  ClearCurrentRequest(NS_BINDING_ABORTED);
  ClearPendingRequest(NS_BINDING_ABORTED);
}

nsImageLoadingContent::~nsImageLoadingContent()
{
  NS_ASSERTION(!mCurrentRequest && !mPendingRequest,
               "DestroyImageLoadingContent not called");
  NS_ASSERTION(!mObserverList.mObserver && !mObserverList.mNext,
               "Observers still registered?");
  NS_ASSERTION(mScriptedObservers.IsEmpty(),
               "Scripted observers still registered?");
}

/*
 * imgINotificationObserver impl
 */
NS_IMETHODIMP
nsImageLoadingContent::Notify(imgIRequest* aRequest,
                              int32_t aType,
                              const nsIntRect* aData)
{
  if (aType == imgINotificationObserver::IS_ANIMATED) {
    return OnImageIsAnimated(aRequest);
  }

  if (aType == imgINotificationObserver::UNLOCKED_DRAW) {
    OnUnlockedDraw();
    return NS_OK;
  }

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    // We should definitely have a request here
    MOZ_ASSERT(aRequest, "no request?");

    NS_PRECONDITION(aRequest == mCurrentRequest || aRequest == mPendingRequest,
                    "Unknown request");
  }

  {
    // Calling Notify on observers can modify the list of observers so make
    // a local copy.
    AutoTArray<nsCOMPtr<imgINotificationObserver>, 2> observers;
    for (ImageObserver* observer = &mObserverList, *next; observer;
         observer = next) {
      next = observer->mNext;
      if (observer->mObserver) {
        observers.AppendElement(observer->mObserver);
      }
    }

    nsAutoScriptBlocker scriptBlocker;

    for (auto& observer : observers) {
        observer->Notify(aRequest, aType, aData);
    }
  }

  if (aType == imgINotificationObserver::SIZE_AVAILABLE) {
    // Have to check for state changes here, since we might have been in
    // the LOADING state before.
    UpdateImageState(true);
  }

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    uint32_t reqStatus;
    aRequest->GetImageStatus(&reqStatus);
    /* triage STATUS_ERROR */
    if (reqStatus & imgIRequest::STATUS_ERROR) {
      nsresult errorCode = NS_OK;
      aRequest->GetImageErrorCode(&errorCode);

      /* Handle image not loading error because source was a tracking URL.
       * We make a note of this image node by including it in a dedicated
       * array of blocked tracking nodes under its parent document.
       */
      if (errorCode == NS_ERROR_TRACKING_URI) {
        nsCOMPtr<nsIContent> thisNode
          = do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));

        nsIDocument *doc = GetOurOwnerDoc();
        doc->AddBlockedTrackingNode(thisNode);
      }
    }
    nsresult status =
        reqStatus & imgIRequest::STATUS_ERROR ? NS_ERROR_FAILURE : NS_OK;
    return OnLoadComplete(aRequest, status);
  }

  if (aType == imgINotificationObserver::DECODE_COMPLETE) {
    nsCOMPtr<imgIContainer> container;
    aRequest->GetImage(getter_AddRefs(container));
    if (container) {
      container->PropagateUseCounters(GetOurOwnerDoc());
    }

    UpdateImageState(true);
  }

  return NS_OK;
}

nsresult
nsImageLoadingContent::OnLoadComplete(imgIRequest* aRequest, nsresult aStatus)
{
  uint32_t oldStatus;
  aRequest->GetImageStatus(&oldStatus);

  //XXXjdm This occurs when we have a pending request created, then another
  //       pending request replaces it before the first one is finished.
  //       This begs the question of what the correct behaviour is; we used
  //       to not have to care because we ran this code in OnStopDecode which
  //       wasn't called when the first request was cancelled. For now, I choose
  //       to punt when the given request doesn't appear to have terminated in
  //       an expected state.
  if (!(oldStatus & (imgIRequest::STATUS_ERROR | imgIRequest::STATUS_LOAD_COMPLETE)))
    return NS_OK;

  // Our state may change. Watch it.
  AutoStateChanger changer(this, true);

  // If the pending request is loaded, switch to it.
  if (aRequest == mPendingRequest) {
    MakePendingRequestCurrent();
  }
  MOZ_ASSERT(aRequest == mCurrentRequest,
             "One way or another, we should be current by now");

  // Fire the appropriate DOM event.
  if (NS_SUCCEEDED(aStatus)) {
    FireEvent(NS_LITERAL_STRING("load"));

    // Do not fire loadend event for multipart/x-mixed-replace image streams.
    bool isMultipart;
    if (NS_FAILED(aRequest->GetMultipart(&isMultipart)) || !isMultipart) {
      FireEvent(NS_LITERAL_STRING("loadend"));
    }
  } else {
    FireEvent(NS_LITERAL_STRING("error"));
    FireEvent(NS_LITERAL_STRING("loadend"));
  }

  nsCOMPtr<nsINode> thisNode = do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  SVGObserverUtils::InvalidateDirectRenderingObservers(thisNode->AsElement());

  return NS_OK;
}

static bool
ImageIsAnimated(imgIRequest* aRequest)
{
  if (!aRequest) {
    return false;
  }

  nsCOMPtr<imgIContainer> image;
  if (NS_SUCCEEDED(aRequest->GetImage(getter_AddRefs(image)))) {
    bool isAnimated = false;
    nsresult rv = image->GetAnimated(&isAnimated);
    if (NS_SUCCEEDED(rv) && isAnimated) {
      return true;
    }
  }

  return false;
}

void
nsImageLoadingContent::OnUnlockedDraw()
{
  // It's OK for non-animated images to wait until the next frame visibility
  // update to become locked. (And that's preferable, since in the case of
  // scrolling it keeps memory usage minimal.) For animated images, though, we
  // want to mark them visible right away so we can call
  // IncrementAnimationConsumers() on them and they'll start animating.
  if (!ImageIsAnimated(mCurrentRequest) && !ImageIsAnimated(mPendingRequest)) {
    return;
  }

  nsIFrame* frame = GetOurPrimaryFrame();
  if (!frame) {
    return;
  }

  if (frame->GetVisibility() == Visibility::APPROXIMATELY_VISIBLE) {
    // This frame is already marked visible; there's nothing to do.
    return;
  }

  nsPresContext* presContext = frame->PresContext();
  if (!presContext) {
    return;
  }

  nsIPresShell* presShell = presContext->PresShell();
  if (!presShell) {
    return;
  }

  presShell->EnsureFrameInApproximatelyVisibleList(frame);
}

nsresult
nsImageLoadingContent::OnImageIsAnimated(imgIRequest *aRequest)
{
  bool* requestFlag = GetRegisteredFlagForRequest(aRequest);
  if (requestFlag) {
    nsLayoutUtils::RegisterImageRequest(GetFramePresContext(),
                                        aRequest, requestFlag);
  }

  return NS_OK;
}

/*
 * nsIImageLoadingContent impl
 */

void
nsImageLoadingContent::SetLoadingEnabled(bool aLoadingEnabled)
{
  if (nsContentUtils::GetImgLoaderForChannel(nullptr, nullptr)) {
    mLoadingEnabled = aLoadingEnabled;
  }
}

NS_IMETHODIMP
nsImageLoadingContent::GetImageBlockingStatus(int16_t* aStatus)
{
  NS_PRECONDITION(aStatus, "Null out param");
  *aStatus = ImageBlockingStatus();
  return NS_OK;
}

static void
ReplayImageStatus(imgIRequest* aRequest, imgINotificationObserver* aObserver)
{
  if (!aRequest) {
    return;
  }

  uint32_t status = 0;
  nsresult rv = aRequest->GetImageStatus(&status);
  if (NS_FAILED(rv)) {
    return;
  }

  if (status & imgIRequest::STATUS_SIZE_AVAILABLE) {
    aObserver->Notify(aRequest, imgINotificationObserver::SIZE_AVAILABLE, nullptr);
  }
  if (status & imgIRequest::STATUS_FRAME_COMPLETE) {
    aObserver->Notify(aRequest, imgINotificationObserver::FRAME_COMPLETE, nullptr);
  }
  if (status & imgIRequest::STATUS_HAS_TRANSPARENCY) {
    aObserver->Notify(aRequest, imgINotificationObserver::HAS_TRANSPARENCY, nullptr);
  }
  if (status & imgIRequest::STATUS_IS_ANIMATED) {
    aObserver->Notify(aRequest, imgINotificationObserver::IS_ANIMATED, nullptr);
  }
  if (status & imgIRequest::STATUS_DECODE_COMPLETE) {
    aObserver->Notify(aRequest, imgINotificationObserver::DECODE_COMPLETE, nullptr);
  }
  if (status & imgIRequest::STATUS_LOAD_COMPLETE) {
    aObserver->Notify(aRequest, imgINotificationObserver::LOAD_COMPLETE, nullptr);
  }
}

NS_IMETHODIMP
nsImageLoadingContent::AddNativeObserver(imgINotificationObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  if (!mObserverList.mObserver) {
    // Don't touch the linking of the list!
    mObserverList.mObserver = aObserver;

    ReplayImageStatus(mCurrentRequest, aObserver);
    ReplayImageStatus(mPendingRequest, aObserver);

    return NS_OK;
  }

  // otherwise we have to create a new entry

  ImageObserver* observer = &mObserverList;
  while (observer->mNext) {
    observer = observer->mNext;
  }

  observer->mNext = new ImageObserver(aObserver);
  ReplayImageStatus(mCurrentRequest, aObserver);
  ReplayImageStatus(mPendingRequest, aObserver);

  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::RemoveNativeObserver(imgINotificationObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  if (mObserverList.mObserver == aObserver) {
    mObserverList.mObserver = nullptr;
    // Don't touch the linking of the list!
    return NS_OK;
  }

  // otherwise have to find it and splice it out
  ImageObserver* observer = &mObserverList;
  while (observer->mNext && observer->mNext->mObserver != aObserver) {
    observer = observer->mNext;
  }

  // At this point, we are pointing to the list element whose mNext is
  // the right observer (assuming of course that mNext is not null)
  if (observer->mNext) {
    // splice it out
    ImageObserver* oldObserver = observer->mNext;
    observer->mNext = oldObserver->mNext;
    oldObserver->mNext = nullptr;  // so we don't destroy them all
    delete oldObserver;
  }
#ifdef DEBUG
  else {
    NS_WARNING("Asked to remove nonexistent observer");
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::AddObserver(imgINotificationObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  nsresult rv = NS_OK;
  RefPtr<imgRequestProxy> currentReq;
  if (mCurrentRequest) {
    // Scripted observers may not belong to the same document as us, so when we
    // create the imgRequestProxy, we shouldn't use any. This allows the request
    // to dispatch notifications from the correct scheduler group.
    rv = mCurrentRequest->Clone(aObserver, nullptr, getter_AddRefs(currentReq));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  RefPtr<imgRequestProxy> pendingReq;
  if (mPendingRequest) {
    // See above for why we don't use the loading document.
    rv = mPendingRequest->Clone(aObserver, nullptr, getter_AddRefs(pendingReq));
    if (NS_FAILED(rv)) {
      mCurrentRequest->CancelAndForgetObserver(NS_BINDING_ABORTED);
      return rv;
    }
  }

  mScriptedObservers.AppendElement(
    new ScriptedImageObserver(aObserver, Move(currentReq), Move(pendingReq)));
  return rv;
}

NS_IMETHODIMP
nsImageLoadingContent::RemoveObserver(imgINotificationObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  if (NS_WARN_IF(mScriptedObservers.IsEmpty())) {
    return NS_OK;
  }

  RefPtr<ScriptedImageObserver> observer;
  auto i = mScriptedObservers.Length();
  do {
    --i;
    if (mScriptedObservers[i]->mObserver == aObserver) {
      observer = Move(mScriptedObservers[i]);
      mScriptedObservers.RemoveElementAt(i);
      break;
    }
  } while(i > 0);

  if (NS_WARN_IF(!observer)) {
    return NS_OK;
  }

  // If the cancel causes a mutation, it will be harmless, because we have
  // already removed the observer from the list.
  observer->CancelRequests();
  return NS_OK;
}

void
nsImageLoadingContent::ClearScriptedRequests(int32_t aRequestType, nsresult aReason)
{
  if (MOZ_LIKELY(mScriptedObservers.IsEmpty())) {
    return;
  }

  nsTArray<RefPtr<ScriptedImageObserver>> observers(mScriptedObservers);
  auto i = observers.Length();
  do {
    --i;

    RefPtr<imgRequestProxy> req;
    switch (aRequestType) {
    case CURRENT_REQUEST:
      req = Move(observers[i]->mCurrentRequest);
      break;
    case PENDING_REQUEST:
      req = Move(observers[i]->mPendingRequest);
      break;
    default:
      NS_ERROR("Unknown request type");
      return;
    }

    if (req) {
      req->CancelAndForgetObserver(aReason);
    }
  } while (i > 0);
}

void
nsImageLoadingContent::CloneScriptedRequests(imgRequestProxy* aRequest)
{
  MOZ_ASSERT(aRequest);

  if (MOZ_LIKELY(mScriptedObservers.IsEmpty())) {
    return;
  }

  bool current;
  if (aRequest == mCurrentRequest) {
    current = true;
  } else if (aRequest == mPendingRequest) {
    current = false;
  } else {
    MOZ_ASSERT_UNREACHABLE("Unknown request type");
    return;
  }

  nsTArray<RefPtr<ScriptedImageObserver>> observers(mScriptedObservers);
  auto i = observers.Length();
  do {
    --i;

    ScriptedImageObserver* observer = observers[i];
    RefPtr<imgRequestProxy>& req = current ? observer->mCurrentRequest
                                           : observer->mPendingRequest;
    if (NS_WARN_IF(req)) {
      MOZ_ASSERT_UNREACHABLE("Should have cancelled original request");
      req->CancelAndForgetObserver(NS_BINDING_ABORTED);
      req = nullptr;
    }

    nsresult rv = aRequest->Clone(observer->mObserver, nullptr, getter_AddRefs(req));
    Unused << NS_WARN_IF(NS_FAILED(rv));
  } while (i > 0);
}

void
nsImageLoadingContent::MakePendingScriptedRequestsCurrent()
{
  if (MOZ_LIKELY(mScriptedObservers.IsEmpty())) {
    return;
  }

  nsTArray<RefPtr<ScriptedImageObserver>> observers(mScriptedObservers);
  auto i = observers.Length();
  do {
    --i;

    ScriptedImageObserver* observer = observers[i];
    if (observer->mCurrentRequest) {
      observer->mCurrentRequest->CancelAndForgetObserver(NS_BINDING_ABORTED);
    }
    observer->mCurrentRequest = Move(observer->mPendingRequest);
  } while (i > 0);
}

already_AddRefed<imgIRequest>
nsImageLoadingContent::GetRequest(int32_t aRequestType,
                                  ErrorResult& aError)
{
  nsCOMPtr<imgIRequest> request;
  switch(aRequestType) {
  case CURRENT_REQUEST:
    request = mCurrentRequest;
    break;
  case PENDING_REQUEST:
    request = mPendingRequest;
    break;
  default:
    NS_ERROR("Unknown request type");
    aError.Throw(NS_ERROR_UNEXPECTED);
  }

  return request.forget();
}

NS_IMETHODIMP
nsImageLoadingContent::GetRequest(int32_t aRequestType,
                                  imgIRequest** aRequest)
{
  NS_ENSURE_ARG_POINTER(aRequest);

  ErrorResult result;
  *aRequest = GetRequest(aRequestType, result).take();

  return result.StealNSResult();
}

NS_IMETHODIMP_(bool)
nsImageLoadingContent::CurrentRequestHasSize()
{
  return HaveSize(mCurrentRequest);
}

NS_IMETHODIMP_(void)
nsImageLoadingContent::FrameCreated(nsIFrame* aFrame)
{
  NS_ASSERTION(aFrame, "aFrame is null");

  TrackImage(mCurrentRequest, aFrame);
  TrackImage(mPendingRequest, aFrame);

  // We need to make sure that our image request is registered, if it should
  // be registered.
  nsPresContext* presContext = aFrame->PresContext();
  if (mCurrentRequest) {
    nsLayoutUtils::RegisterImageRequestIfAnimated(presContext, mCurrentRequest,
                                                  &mCurrentRequestRegistered);
  }

  if (mPendingRequest) {
    nsLayoutUtils::RegisterImageRequestIfAnimated(presContext, mPendingRequest,
                                                  &mPendingRequestRegistered);
  }
}

NS_IMETHODIMP_(void)
nsImageLoadingContent::FrameDestroyed(nsIFrame* aFrame)
{
  NS_ASSERTION(aFrame, "aFrame is null");

  // We need to make sure that our image request is deregistered.
  nsPresContext* presContext = GetFramePresContext();
  if (mCurrentRequest) {
    nsLayoutUtils::DeregisterImageRequest(presContext,
                                          mCurrentRequest,
                                          &mCurrentRequestRegistered);
  }

  if (mPendingRequest) {
    nsLayoutUtils::DeregisterImageRequest(presContext,
                                          mPendingRequest,
                                          &mPendingRequestRegistered);
  }

  UntrackImage(mCurrentRequest);
  UntrackImage(mPendingRequest);

  nsIPresShell* presShell = presContext ? presContext->GetPresShell() : nullptr;
  if (presShell) {
    presShell->RemoveFrameFromApproximatelyVisibleList(aFrame);
  }
}

/* static */
nsContentPolicyType
nsImageLoadingContent::PolicyTypeForLoad(ImageLoadType aImageLoadType)
{
  if (aImageLoadType == eImageLoadType_Imageset) {
    return nsIContentPolicy::TYPE_IMAGESET;
  }

  MOZ_ASSERT(aImageLoadType == eImageLoadType_Normal,
             "Unknown ImageLoadType type in PolicyTypeForLoad");
  return nsIContentPolicy::TYPE_INTERNAL_IMAGE;
}

int32_t
nsImageLoadingContent::GetRequestType(imgIRequest* aRequest,
                                      ErrorResult& aError)
{
  if (aRequest == mCurrentRequest) {
    return CURRENT_REQUEST;
  }

  if (aRequest == mPendingRequest) {
    return PENDING_REQUEST;
  }

  NS_ERROR("Unknown request");
  aError.Throw(NS_ERROR_UNEXPECTED);
  return UNKNOWN_REQUEST;
}

NS_IMETHODIMP
nsImageLoadingContent::GetRequestType(imgIRequest* aRequest,
                                      int32_t* aRequestType)
{
  NS_PRECONDITION(aRequestType, "Null out param");

  ErrorResult result;
  *aRequestType = GetRequestType(aRequest, result);
  return result.StealNSResult();
}

already_AddRefed<nsIURI>
nsImageLoadingContent::GetCurrentURI(ErrorResult& aError)
{
  nsCOMPtr<nsIURI> uri;
  if (mCurrentRequest) {
    mCurrentRequest->GetURI(getter_AddRefs(uri));
  } else if (mCurrentURI) {
    nsresult rv = NS_EnsureSafeToReturn(mCurrentURI, getter_AddRefs(uri));
    if (NS_FAILED(rv)) {
      aError.Throw(rv);
    }
  }

  return uri.forget();
}

NS_IMETHODIMP
nsImageLoadingContent::GetCurrentURI(nsIURI** aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);

  ErrorResult result;
  *aURI = GetCurrentURI(result).take();
  return result.StealNSResult();
}

already_AddRefed<nsIURI>
nsImageLoadingContent::GetCurrentRequestFinalURI()
{
  nsCOMPtr<nsIURI> uri;
  if (mCurrentRequest) {
    mCurrentRequest->GetFinalURI(getter_AddRefs(uri));
  }

  return uri.forget();
}

NS_IMETHODIMP
nsImageLoadingContent::LoadImageWithChannel(nsIChannel* aChannel,
                                            nsIStreamListener** aListener)
{
  imgLoader* loader =
    nsContentUtils::GetImgLoaderForChannel(aChannel, GetOurOwnerDoc());
  if (!loader) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIDocument> doc = GetOurOwnerDoc();
  if (!doc) {
    // Don't bother
    *aListener = nullptr;
    return NS_OK;
  }

  // XXX what should we do with content policies here, if anything?
  // Shouldn't that be done before the start of the load?
  // XXX what about shouldProcess?

  // Our state might change. Watch it.
  AutoStateChanger changer(this, true);

  // Do the load.
  RefPtr<imgRequestProxy>& req = PrepareNextRequest(eImageLoadType_Normal);
  nsresult rv = loader->
    LoadImageWithChannel(aChannel, this, doc, aListener, getter_AddRefs(req));
  if (NS_SUCCEEDED(rv)) {
    CloneScriptedRequests(req);
    TrackImage(req);
    ResetAnimationIfNeeded();
    return NS_OK;
  }

  MOZ_ASSERT(!req, "Shouldn't have non-null request here");
  // If we don't have a current URI, we might as well store this URI so people
  // know what we tried (and failed) to load.
  if (!mCurrentRequest)
    aChannel->GetURI(getter_AddRefs(mCurrentURI));

  FireEvent(NS_LITERAL_STRING("error"));
  FireEvent(NS_LITERAL_STRING("loadend"));
  return rv;
}

void
nsImageLoadingContent::ForceReload(const mozilla::dom::Optional<bool>& aNotify,
                                   mozilla::ErrorResult& aError)
{
  nsCOMPtr<nsIURI> currentURI;
  GetCurrentURI(getter_AddRefs(currentURI));
  if (!currentURI) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  // defaults to true
  bool notify = !aNotify.WasPassed() || aNotify.Value();

  // We keep this flag around along with the old URI even for failed requests
  // without a live request object
  ImageLoadType loadType = \
    (mCurrentRequestFlags & REQUEST_IS_IMAGESET) ? eImageLoadType_Imageset
                                                 : eImageLoadType_Normal;
  nsresult rv = LoadImage(currentURI, true, notify, loadType, true, nullptr,
                          nsIRequest::VALIDATE_ALWAYS);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
  }
}

NS_IMETHODIMP
nsImageLoadingContent::ForceReload(bool aNotify /* = true */,
                                   uint8_t aArgc)
{
  mozilla::dom::Optional<bool> notify;
  if (aArgc >= 1) {
    notify.Construct() = aNotify;
  }

  ErrorResult result;
  ForceReload(notify, result);
  return result.StealNSResult();
}

/*
 * Non-interface methods
 */

nsresult
nsImageLoadingContent::LoadImage(const nsAString& aNewURI,
                                 bool aForce,
                                 bool aNotify,
                                 ImageLoadType aImageLoadType,
                                 nsIPrincipal* aTriggeringPrincipal)
{
  // First, get a document (needed for security checks and the like)
  nsIDocument* doc = GetOurOwnerDoc();
  if (!doc) {
    // No reason to bother, I think...
    return NS_OK;
  }

  // Pending load/error events need to be canceled in some situations. This
  // is not documented in the spec, but can cause site compat problems if not
  // done. See bug 1309461 and https://github.com/whatwg/html/issues/1872.
  CancelPendingEvent();

  if (aNewURI.IsEmpty()) {
    // Cancel image requests and then fire only error event per spec.
    CancelImageRequests(aNotify);
    // Mark error event as cancelable only for src="" case, since only this
    // error causes site compat problem (bug 1308069) for now.
    FireEvent(NS_LITERAL_STRING("error"), true);
    return NS_OK;
  }

  // Fire loadstart event
  FireEvent(NS_LITERAL_STRING("loadstart"));

  // Parse the URI string to get image URI
  nsCOMPtr<nsIURI> imageURI;
  nsresult rv = StringToURI(aNewURI, doc, getter_AddRefs(imageURI));
  NS_ENSURE_SUCCESS(rv, rv);
  // XXXbiesi fire onerror if that failed?

  NS_TryToSetImmutable(imageURI);

  return LoadImage(imageURI, aForce, aNotify, aImageLoadType, false, doc,
                   nsIRequest::LOAD_NORMAL, aTriggeringPrincipal);
}

nsresult
nsImageLoadingContent::LoadImage(nsIURI* aNewURI,
                                 bool aForce,
                                 bool aNotify,
                                 ImageLoadType aImageLoadType,
                                 bool aLoadStart,
                                 nsIDocument* aDocument,
                                 nsLoadFlags aLoadFlags,
                                 nsIPrincipal* aTriggeringPrincipal)
{
  MOZ_ASSERT(!mIsStartingImageLoad, "some evil code is reentering LoadImage.");
  if (mIsStartingImageLoad) {
    return NS_OK;
  }

  // Pending load/error events need to be canceled in some situations. This
  // is not documented in the spec, but can cause site compat problems if not
  // done. See bug 1309461 and https://github.com/whatwg/html/issues/1872.
  CancelPendingEvent();

  // Fire loadstart event if required
  if (aLoadStart) {
    FireEvent(NS_LITERAL_STRING("loadstart"));
  }

  if (!mLoadingEnabled) {
    // XXX Why fire an error here? seems like the callers to SetLoadingEnabled
    // don't want/need it.
    FireEvent(NS_LITERAL_STRING("error"));
    FireEvent(NS_LITERAL_STRING("loadend"));
    return NS_OK;
  }

  NS_ASSERTION(!aDocument || aDocument == GetOurOwnerDoc(),
               "Bogus document passed in");
  // First, get a document (needed for security checks and the like)
  if (!aDocument) {
    aDocument = GetOurOwnerDoc();
    if (!aDocument) {
      // No reason to bother, I think...
      return NS_OK;
    }
  }

  AutoRestore<bool> guard(mIsStartingImageLoad);
  mIsStartingImageLoad = true;

  // Data documents, or documents from DOMParser shouldn't perform image loading.
  if (aDocument->IsLoadedAsData()) {
    SetBlockedRequest(nsIContentPolicy::REJECT_REQUEST);

    FireEvent(NS_LITERAL_STRING("error"));
    FireEvent(NS_LITERAL_STRING("loadend"));
    return NS_OK;
  }

  // URI equality check.
  //
  // We skip the equality check if our current image was blocked, since in that
  // case we really do want to try loading again.
  if (!aForce && NS_CP_ACCEPTED(mImageBlockingStatus)) {
    nsCOMPtr<nsIURI> currentURI;
    GetCurrentURI(getter_AddRefs(currentURI));
    bool equal;
    if (currentURI &&
        NS_SUCCEEDED(currentURI->Equals(aNewURI, &equal)) &&
        equal) {
      // Nothing to do here.
      return NS_OK;
    }
  }

  // From this point on, our image state could change. Watch it.
  AutoStateChanger changer(this, aNotify);

  // Sanity check.
  //
  // We use the principal of aDocument to avoid having to QI |this| an extra
  // time. It should always be the same as the principal of this node.
#ifdef DEBUG
  nsIContent* thisContent = AsContent();
  MOZ_ASSERT(thisContent->NodePrincipal() == aDocument->NodePrincipal(),
             "Principal mismatch?");
#endif

  nsLoadFlags loadFlags = aLoadFlags;
  int32_t corsmode = GetCORSMode();
  if (corsmode == CORS_ANONYMOUS) {
    loadFlags |= imgILoader::LOAD_CORS_ANONYMOUS;
  } else if (corsmode == CORS_USE_CREDENTIALS) {
    loadFlags |= imgILoader::LOAD_CORS_USE_CREDENTIALS;
  }

  // get document wide referrer policy
  // if referrer attributes are enabled in preferences, load img referrer attribute
  // if the image does not provide a referrer attribute, ignore this
  net::ReferrerPolicy referrerPolicy = aDocument->GetReferrerPolicy();
  net::ReferrerPolicy imgReferrerPolicy = GetImageReferrerPolicy();
  if (imgReferrerPolicy != net::RP_Unset) {
    referrerPolicy = imgReferrerPolicy;
  }

  RefPtr<imgRequestProxy>& req = PrepareNextRequest(aImageLoadType);
  nsCOMPtr<nsIContent> content =
      do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));

  nsCOMPtr<nsIPrincipal> triggeringPrincipal;
  bool result =
    nsContentUtils::QueryTriggeringPrincipal(content, aTriggeringPrincipal,
                                             getter_AddRefs(triggeringPrincipal));

  // If result is true, which means this node has specified 'triggeringprincipal'
  // attribute on it, so we use favicon as the policy type.
  nsContentPolicyType policyType = result ?
                                     nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON:
                                     PolicyTypeForLoad(aImageLoadType);

  nsCOMPtr<nsINode> thisNode =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  nsresult rv = nsContentUtils::LoadImage(aNewURI,
                                          thisNode,
                                          aDocument,
                                          triggeringPrincipal,
                                          0,
                                          aDocument->GetDocumentURI(),
                                          referrerPolicy,
                                          this, loadFlags,
                                          content->LocalName(),
                                          getter_AddRefs(req),
                                          policyType,
                                          mUseUrgentStartForChannel);

  // Reset the flag to avoid loading from XPCOM or somewhere again else without
  // initiated by user interaction.
  mUseUrgentStartForChannel = false;

  // Tell the document to forget about the image preload, if any, for
  // this URI, now that we might have another imgRequestProxy for it.
  // That way if we get canceled later the image load won't continue.
  aDocument->ForgetImagePreload(aNewURI);

  if (NS_SUCCEEDED(rv)) {
    CloneScriptedRequests(req);
    TrackImage(req);
    ResetAnimationIfNeeded();

    // Handle cases when we just ended up with a pending request but it's
    // already done.  In that situation we have to synchronously switch that
    // request to being the current request, because websites depend on that
    // behavior.
    if (req == mPendingRequest) {
      uint32_t pendingLoadStatus;
      rv = req->GetImageStatus(&pendingLoadStatus);
      if (NS_SUCCEEDED(rv) &&
          (pendingLoadStatus & imgIRequest::STATUS_LOAD_COMPLETE)) {
        MakePendingRequestCurrent();
        MOZ_ASSERT(mCurrentRequest,
                   "How could we not have a current request here?");

        nsImageFrame *f = do_QueryFrame(GetOurPrimaryFrame());
        if (f) {
          f->NotifyNewCurrentRequest(mCurrentRequest, NS_OK);
        }
      }
    }
  } else {
    MOZ_ASSERT(!req, "Shouldn't have non-null request here");
    // If we don't have a current URI, we might as well store this URI so people
    // know what we tried (and failed) to load.
    if (!mCurrentRequest)
      mCurrentURI = aNewURI;

    FireEvent(NS_LITERAL_STRING("error"));
    FireEvent(NS_LITERAL_STRING("loadend"));
  }

  return NS_OK;
}

nsresult
nsImageLoadingContent::ForceImageState(bool aForce,
                                       EventStates::InternalType aState)
{
  mIsImageStateForced = aForce;
  mForcedImageState = EventStates(aState);
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::GetNaturalWidth(uint32_t* aNaturalWidth)
{
  NS_ENSURE_ARG_POINTER(aNaturalWidth);

  nsCOMPtr<imgIContainer> image;
  if (mCurrentRequest) {
    mCurrentRequest->GetImage(getter_AddRefs(image));
  }

  int32_t width;
  if (image && NS_SUCCEEDED(image->GetWidth(&width))) {
    *aNaturalWidth = width;
  } else {
    *aNaturalWidth = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::GetNaturalHeight(uint32_t* aNaturalHeight)
{
  NS_ENSURE_ARG_POINTER(aNaturalHeight);

  nsCOMPtr<imgIContainer> image;
  if (mCurrentRequest) {
    mCurrentRequest->GetImage(getter_AddRefs(image));
  }

  int32_t height;
  if (image && NS_SUCCEEDED(image->GetHeight(&height))) {
    *aNaturalHeight = height;
  } else {
    *aNaturalHeight = 0;
  }

  return NS_OK;
}

EventStates
nsImageLoadingContent::ImageState() const
{
  if (mIsImageStateForced) {
    return mForcedImageState;
  }

  EventStates states;

  if (mBroken) {
    states |= NS_EVENT_STATE_BROKEN;
  }
  if (mUserDisabled) {
    states |= NS_EVENT_STATE_USERDISABLED;
  }
  if (mSuppressed) {
    states |= NS_EVENT_STATE_SUPPRESSED;
  }
  if (mLoading) {
    states |= NS_EVENT_STATE_LOADING;
  }

  return states;
}

void
nsImageLoadingContent::UpdateImageState(bool aNotify)
{
  if (mStateChangerDepth > 0) {
    // Ignore this call; we'll update our state when the outermost state changer
    // is destroyed. Need this to work around the fact that some ImageLib
    // stuff is actually sync and hence we can get OnStopDecode called while
    // we're still under LoadImage, and OnStopDecode doesn't know anything about
    // aNotify.
    // XXX - This machinery should be removed after bug 521604.
    return;
  }

  nsIContent* thisContent = AsContent();

  mLoading = mBroken = mUserDisabled = mSuppressed = false;

  // If we were blocked by server-based content policy, we claim to be
  // suppressed.  If we were blocked by type-based content policy, we claim to
  // be user-disabled.  Otherwise, claim to be broken.
  if (mImageBlockingStatus == nsIContentPolicy::REJECT_SERVER) {
    mSuppressed = true;
  } else if (mImageBlockingStatus == nsIContentPolicy::REJECT_TYPE) {
    mUserDisabled = true;
  } else if (!mCurrentRequest) {
    // No current request means error, since we weren't disabled or suppressed
    mBroken = true;
  } else {
    uint32_t currentLoadStatus;
    nsresult rv = mCurrentRequest->GetImageStatus(&currentLoadStatus);
    if (NS_FAILED(rv) || (currentLoadStatus & imgIRequest::STATUS_ERROR)) {
      mBroken = true;
    } else if (!(currentLoadStatus & imgIRequest::STATUS_SIZE_AVAILABLE)) {
      mLoading = true;
    }
  }

  NS_ASSERTION(thisContent->IsElement(), "Not an element?");
  thisContent->AsElement()->UpdateState(aNotify);
}

void
nsImageLoadingContent::CancelImageRequests(bool aNotify)
{
  AutoStateChanger changer(this, aNotify);
  ClearPendingRequest(NS_BINDING_ABORTED, Some(OnNonvisible::DISCARD_IMAGES));
  ClearCurrentRequest(NS_BINDING_ABORTED, Some(OnNonvisible::DISCARD_IMAGES));
}

nsresult
nsImageLoadingContent::UseAsPrimaryRequest(imgRequestProxy* aRequest,
                                           bool aNotify,
                                           ImageLoadType aImageLoadType)
{
  // Our state will change. Watch it.
  AutoStateChanger changer(this, aNotify);

  // Get rid if our existing images
  ClearPendingRequest(NS_BINDING_ABORTED, Some(OnNonvisible::DISCARD_IMAGES));
  ClearCurrentRequest(NS_BINDING_ABORTED, Some(OnNonvisible::DISCARD_IMAGES));

  // Clone the request we were given.
  RefPtr<imgRequestProxy>& req = PrepareNextRequest(aImageLoadType);
  nsresult rv = aRequest->SyncClone(this, GetOurOwnerDoc(), getter_AddRefs(req));
  if (NS_SUCCEEDED(rv)) {
    CloneScriptedRequests(req);
    TrackImage(req);
  } else {
    MOZ_ASSERT(!req, "Shouldn't have non-null request here");
    return rv;
  }

  return NS_OK;
}

nsIDocument*
nsImageLoadingContent::GetOurOwnerDoc()
{
  return AsContent()->OwnerDoc();
}

nsIDocument*
nsImageLoadingContent::GetOurCurrentDoc()
{
  return AsContent()->GetComposedDoc();
}

nsIFrame*
nsImageLoadingContent::GetOurPrimaryFrame()
{
  return AsContent()->GetPrimaryFrame();
}

nsPresContext* nsImageLoadingContent::GetFramePresContext()
{
  nsIFrame* frame = GetOurPrimaryFrame();
  if (!frame) {
    return nullptr;
  }

  return frame->PresContext();
}

nsresult
nsImageLoadingContent::StringToURI(const nsAString& aSpec,
                                   nsIDocument* aDocument,
                                   nsIURI** aURI)
{
  NS_PRECONDITION(aDocument, "Must have a document");
  NS_PRECONDITION(aURI, "Null out param");

  // (1) Get the base URI
  nsIContent* thisContent = AsContent();
  nsCOMPtr<nsIURI> baseURL = thisContent->GetBaseURI();

  // (2) Get the charset
  auto encoding = aDocument->GetDocumentCharacterSet();

  // (3) Construct the silly thing
  return NS_NewURI(aURI,
                   aSpec,
                   encoding,
                   baseURL,
                   nsContentUtils::GetIOService());
}

nsresult
nsImageLoadingContent::FireEvent(const nsAString& aEventType, bool aIsCancelable)
{
  if (nsContentUtils::DocumentInactiveForImageLoads(GetOurOwnerDoc())) {
    // Don't bother to fire any events, especially error events.
    return NS_OK;
  }

  // We have to fire the event asynchronously so that we won't go into infinite
  // loops in cases when onLoad handlers reset the src and the new src is in
  // cache.

  nsCOMPtr<nsINode> thisNode = do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));

  RefPtr<AsyncEventDispatcher> loadBlockingAsyncDispatcher =
    new LoadBlockingAsyncEventDispatcher(thisNode, aEventType, false, false);
  loadBlockingAsyncDispatcher->PostDOMEvent();

  if (aIsCancelable) {
    mPendingEvent = loadBlockingAsyncDispatcher;
  }

  return NS_OK;
}

void
nsImageLoadingContent::AsyncEventRunning(AsyncEventDispatcher* aEvent)
{
  if (mPendingEvent == aEvent) {
    mPendingEvent = nullptr;
  }
}

void
nsImageLoadingContent::CancelPendingEvent()
{
  if (mPendingEvent) {
    mPendingEvent->Cancel();
    mPendingEvent = nullptr;
  }
}

RefPtr<imgRequestProxy>&
nsImageLoadingContent::PrepareNextRequest(ImageLoadType aImageLoadType)
{
  nsImageFrame* imageFrame = do_QueryFrame(GetOurPrimaryFrame());
  nsSVGImageFrame* svgImageFrame = do_QueryFrame(GetOurPrimaryFrame());
  if (imageFrame || svgImageFrame) {
    // Detect JavaScript-based animations created by changing the |src|
    // attribute on a timer.
    TimeStamp now = TimeStamp::Now();
    TimeDuration threshold =
      TimeDuration::FromMilliseconds(
        gfxPrefs::ImageInferSrcAnimationThresholdMS());

    // If the length of time between request changes is less than the threshold,
    // then force sync decoding to eliminate flicker from the animation.
    bool forceSync = (now - mMostRecentRequestChange < threshold);
    if (imageFrame) {
      imageFrame->SetForceSyncDecoding(forceSync);
    } else {
      svgImageFrame->SetForceSyncDecoding(forceSync);
    }

    mMostRecentRequestChange = now;
  }

  // We only want to cancel the existing current request if size is not
  // available. bz says the web depends on this behavior.
  // Otherwise, we get rid of any half-baked request that might be sitting there
  // and make this one current.
  return HaveSize(mCurrentRequest) ?
           PreparePendingRequest(aImageLoadType) :
           PrepareCurrentRequest(aImageLoadType);
}

nsresult
nsImageLoadingContent::SetBlockedRequest(int16_t aContentDecision)
{
  // If this is not calling from LoadImage, for example, from ServiceWorker,
  // bail out.
  if (!mIsStartingImageLoad) {
    return NS_OK;
  }

  // Sanity
  MOZ_ASSERT(!NS_CP_ACCEPTED(aContentDecision), "Blocked but not?");

  // We should never have a pending request after we got blocked.
  MOZ_ASSERT(!mPendingRequest, "mPendingRequest should be null.");

  if (HaveSize(mCurrentRequest)) {
    // PreparePendingRequest set mPendingRequestFlags, now since we've decided
    // to block it, we reset it back to 0.
    mPendingRequestFlags = 0;
  } else {
    mImageBlockingStatus = aContentDecision;
  }

  return NS_OK;
}

RefPtr<imgRequestProxy>&
nsImageLoadingContent::PrepareCurrentRequest(ImageLoadType aImageLoadType)
{
  // Blocked images go through SetBlockedRequest, which is a separate path. For
  // everything else, we're unblocked.
  mImageBlockingStatus = nsIContentPolicy::ACCEPT;

  // Get rid of anything that was there previously.
  ClearCurrentRequest(NS_BINDING_ABORTED,
                      Some(OnNonvisible::DISCARD_IMAGES));

  if (mNewRequestsWillNeedAnimationReset) {
    mCurrentRequestFlags |= REQUEST_NEEDS_ANIMATION_RESET;
  }

  if (aImageLoadType == eImageLoadType_Imageset) {
    mCurrentRequestFlags |= REQUEST_IS_IMAGESET;
  }

  // Return a reference.
  return mCurrentRequest;
}

RefPtr<imgRequestProxy>&
nsImageLoadingContent::PreparePendingRequest(ImageLoadType aImageLoadType)
{
  // Get rid of anything that was there previously.
  ClearPendingRequest(NS_BINDING_ABORTED,
                      Some(OnNonvisible::DISCARD_IMAGES));

  if (mNewRequestsWillNeedAnimationReset) {
    mPendingRequestFlags |= REQUEST_NEEDS_ANIMATION_RESET;
  }

  if (aImageLoadType == eImageLoadType_Imageset) {
    mPendingRequestFlags |= REQUEST_IS_IMAGESET;
  }

  // Return a reference.
  return mPendingRequest;
}

namespace {

class ImageRequestAutoLock
{
public:
  explicit ImageRequestAutoLock(imgIRequest* aRequest)
    : mRequest(aRequest)
  {
    if (mRequest) {
      mRequest->LockImage();
    }
  }

  ~ImageRequestAutoLock()
  {
    if (mRequest) {
      mRequest->UnlockImage();
    }
  }

private:
  nsCOMPtr<imgIRequest> mRequest;
};

} // namespace

void
nsImageLoadingContent::MakePendingRequestCurrent()
{
  MOZ_ASSERT(mPendingRequest);

  // Lock mCurrentRequest for the duration of this method.  We do this because
  // PrepareCurrentRequest() might unlock mCurrentRequest.  If mCurrentRequest
  // and mPendingRequest are both requests for the same image, unlocking
  // mCurrentRequest before we lock mPendingRequest can cause the lock count
  // to go to 0 and the image to be discarded!
  ImageRequestAutoLock autoLock(mCurrentRequest);

  ImageLoadType loadType = \
    (mPendingRequestFlags & REQUEST_IS_IMAGESET) ? eImageLoadType_Imageset
                                                 : eImageLoadType_Normal;

  PrepareCurrentRequest(loadType) = mPendingRequest;
  MakePendingScriptedRequestsCurrent();
  mPendingRequest = nullptr;
  mCurrentRequestFlags = mPendingRequestFlags;
  mPendingRequestFlags = 0;
  mCurrentRequestRegistered = mPendingRequestRegistered;
  mPendingRequestRegistered = false;
  ResetAnimationIfNeeded();
}

void
nsImageLoadingContent::ClearCurrentRequest(nsresult aReason,
                                           const Maybe<OnNonvisible>& aNonvisibleAction)
{
  if (!mCurrentRequest) {
    // Even if we didn't have a current request, we might have been keeping
    // a URI and flags as a placeholder for a failed load. Clear that now.
    mCurrentURI = nullptr;
    mCurrentRequestFlags = 0;
    return;
  }
  MOZ_ASSERT(!mCurrentURI,
             "Shouldn't have both mCurrentRequest and mCurrentURI!");

  // Deregister this image from the refresh driver so it no longer receives
  // notifications.
  nsLayoutUtils::DeregisterImageRequest(GetFramePresContext(), mCurrentRequest,
                                        &mCurrentRequestRegistered);

  // Clean up the request.
  UntrackImage(mCurrentRequest, aNonvisibleAction);
  ClearScriptedRequests(CURRENT_REQUEST, aReason);
  mCurrentRequest->CancelAndForgetObserver(aReason);
  mCurrentRequest = nullptr;
  mCurrentRequestFlags = 0;
}

void
nsImageLoadingContent::ClearPendingRequest(nsresult aReason,
                                           const Maybe<OnNonvisible>& aNonvisibleAction)
{
  if (!mPendingRequest)
    return;

  // Deregister this image from the refresh driver so it no longer receives
  // notifications.
  nsLayoutUtils::DeregisterImageRequest(GetFramePresContext(), mPendingRequest,
                                        &mPendingRequestRegistered);

  UntrackImage(mPendingRequest, aNonvisibleAction);
  ClearScriptedRequests(PENDING_REQUEST, aReason);
  mPendingRequest->CancelAndForgetObserver(aReason);
  mPendingRequest = nullptr;
  mPendingRequestFlags = 0;
}

bool*
nsImageLoadingContent::GetRegisteredFlagForRequest(imgIRequest* aRequest)
{
  if (aRequest == mCurrentRequest) {
    return &mCurrentRequestRegistered;
  }
  if (aRequest == mPendingRequest) {
    return &mPendingRequestRegistered;
  }
  return nullptr;
}

void
nsImageLoadingContent::ResetAnimationIfNeeded()
{
  if (mCurrentRequest &&
      (mCurrentRequestFlags & REQUEST_NEEDS_ANIMATION_RESET)) {
    nsCOMPtr<imgIContainer> container;
    mCurrentRequest->GetImage(getter_AddRefs(container));
    if (container)
      container->ResetAnimation();
    mCurrentRequestFlags &= ~REQUEST_NEEDS_ANIMATION_RESET;
  }
}

bool
nsImageLoadingContent::HaveSize(imgIRequest *aImage)
{
  // Handle the null case
  if (!aImage)
    return false;

  // Query the image
  uint32_t status;
  nsresult rv = aImage->GetImageStatus(&status);
  return (NS_SUCCEEDED(rv) && (status & imgIRequest::STATUS_SIZE_AVAILABLE));
}

void
nsImageLoadingContent::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                  nsIContent* aBindingParent,
                                  bool aCompileEventHandlers)
{
  // We may be entering the document, so if our image should be tracked,
  // track it.
  if (!aDocument)
    return;

  TrackImage(mCurrentRequest);
  TrackImage(mPendingRequest);
}

void
nsImageLoadingContent::UnbindFromTree(bool aDeep, bool aNullParent)
{
  // We may be leaving the document, so if our image is tracked, untrack it.
  nsCOMPtr<nsIDocument> doc = GetOurCurrentDoc();
  if (!doc)
    return;

  UntrackImage(mCurrentRequest);
  UntrackImage(mPendingRequest);
}

void
nsImageLoadingContent::OnVisibilityChange(Visibility aNewVisibility,
                                          const Maybe<OnNonvisible>& aNonvisibleAction)
{
  switch (aNewVisibility) {
    case Visibility::APPROXIMATELY_VISIBLE:
      TrackImage(mCurrentRequest);
      TrackImage(mPendingRequest);
      break;

    case Visibility::APPROXIMATELY_NONVISIBLE:
      UntrackImage(mCurrentRequest, aNonvisibleAction);
      UntrackImage(mPendingRequest, aNonvisibleAction);
      break;

    case Visibility::UNTRACKED:
      MOZ_ASSERT_UNREACHABLE("Shouldn't notify for untracked visibility");
      break;
  }
}

void
nsImageLoadingContent::TrackImage(imgIRequest* aImage,
                                  nsIFrame* aFrame /*= nullptr */)
{
  if (!aImage)
    return;

  MOZ_ASSERT(aImage == mCurrentRequest || aImage == mPendingRequest,
             "Why haven't we heard of this request?");

  nsIDocument* doc = GetOurCurrentDoc();
  if (!doc) {
    return;
  }

  if (!aFrame) {
    aFrame = GetOurPrimaryFrame();
  }

  /* This line is deceptively simple. It hides a lot of subtlety. Before we
   * create an nsImageFrame we call nsImageFrame::ShouldCreateImageFrameFor
   * to determine if we should create an nsImageFrame or create a frame based
   * on the display of the element (ie inline, block, etc). Inline, block, etc
   * frames don't register for visibility tracking so they will return UNTRACKED
   * from GetVisibility(). So this line is choosing to mark such images as
   * visible. Once the image loads we will get an nsImageFrame and the proper
   * visibility. This is a pitfall of tracking the visibility on the frames
   * instead of the content node.
   */
  if (!aFrame || aFrame->GetVisibility() == Visibility::APPROXIMATELY_NONVISIBLE) {
    return;
  }

  if (aImage == mCurrentRequest && !(mCurrentRequestFlags & REQUEST_IS_TRACKED)) {
    mCurrentRequestFlags |= REQUEST_IS_TRACKED;
    doc->ImageTracker()->Add(mCurrentRequest);
  }
  if (aImage == mPendingRequest && !(mPendingRequestFlags & REQUEST_IS_TRACKED)) {
    mPendingRequestFlags |= REQUEST_IS_TRACKED;
    doc->ImageTracker()->Add(mPendingRequest);
  }
}

void
nsImageLoadingContent::UntrackImage(imgIRequest* aImage,
                                    const Maybe<OnNonvisible>& aNonvisibleAction
                                      /* = Nothing() */)
{
  if (!aImage)
    return;

  MOZ_ASSERT(aImage == mCurrentRequest || aImage == mPendingRequest,
             "Why haven't we heard of this request?");

  // We may not be in the document.  If we outlived our document that's fine,
  // because the document empties out the tracker and unlocks all locked images
  // on destruction.  But if we were never in the document we may need to force
  // discarding the image here, since this is the only chance we have.
  nsIDocument* doc = GetOurCurrentDoc();
  if (aImage == mCurrentRequest) {
    if (doc && (mCurrentRequestFlags & REQUEST_IS_TRACKED)) {
      mCurrentRequestFlags &= ~REQUEST_IS_TRACKED;
      doc->ImageTracker()->Remove(
        mCurrentRequest,
        aNonvisibleAction == Some(OnNonvisible::DISCARD_IMAGES)
          ? ImageTracker::REQUEST_DISCARD
          : 0);
    } else if (aNonvisibleAction == Some(OnNonvisible::DISCARD_IMAGES)) {
      // If we're not in the document we may still need to be discarded.
      aImage->RequestDiscard();
    }
  }
  if (aImage == mPendingRequest) {
    if (doc && (mPendingRequestFlags & REQUEST_IS_TRACKED)) {
      mPendingRequestFlags &= ~REQUEST_IS_TRACKED;
      doc->ImageTracker()->Remove(
        mPendingRequest,
        aNonvisibleAction == Some(OnNonvisible::DISCARD_IMAGES)
          ? ImageTracker::REQUEST_DISCARD
          : 0);
    } else if (aNonvisibleAction == Some(OnNonvisible::DISCARD_IMAGES)) {
      // If we're not in the document we may still need to be discarded.
      aImage->RequestDiscard();
    }
  }
}


void
nsImageLoadingContent::CreateStaticImageClone(nsImageLoadingContent* aDest) const
{
  aDest->ClearScriptedRequests(CURRENT_REQUEST, NS_BINDING_ABORTED);
  aDest->mCurrentRequest =
    nsContentUtils::GetStaticRequest(aDest->GetOurOwnerDoc(), mCurrentRequest);
  if (aDest->mCurrentRequest) {
    aDest->CloneScriptedRequests(aDest->mCurrentRequest);
  }
  aDest->TrackImage(aDest->mCurrentRequest);
  aDest->mForcedImageState = mForcedImageState;
  aDest->mImageBlockingStatus = mImageBlockingStatus;
  aDest->mLoadingEnabled = mLoadingEnabled;
  aDest->mStateChangerDepth = mStateChangerDepth;
  aDest->mIsImageStateForced = mIsImageStateForced;
  aDest->mLoading = mLoading;
  aDest->mBroken = mBroken;
  aDest->mUserDisabled = mUserDisabled;
  aDest->mSuppressed = mSuppressed;
}

CORSMode
nsImageLoadingContent::GetCORSMode()
{
  return CORS_NONE;
}

nsImageLoadingContent::ImageObserver::ImageObserver(imgINotificationObserver* aObserver)
  : mObserver(aObserver)
  , mNext(nullptr)
{
  MOZ_COUNT_CTOR(ImageObserver);
}

nsImageLoadingContent::ImageObserver::~ImageObserver()
{
  MOZ_COUNT_DTOR(ImageObserver);
  NS_CONTENT_DELETE_LIST_MEMBER(ImageObserver, this, mNext);
}

nsImageLoadingContent::ScriptedImageObserver::ScriptedImageObserver(imgINotificationObserver* aObserver,
                                                                    RefPtr<imgRequestProxy>&& aCurrentRequest,
                                                                    RefPtr<imgRequestProxy>&& aPendingRequest)
  : mObserver(aObserver)
  , mCurrentRequest(aCurrentRequest)
  , mPendingRequest(aPendingRequest)
{ }

nsImageLoadingContent::ScriptedImageObserver::~ScriptedImageObserver()
{
  // We should have cancelled any requests before getting released.
  DebugOnly<bool> cancel = CancelRequests();
  MOZ_ASSERT(!cancel, "Still have requests in ~ScriptedImageObserver!");
}

bool
nsImageLoadingContent::ScriptedImageObserver::CancelRequests()
{
  bool cancelled = false;
  if (mCurrentRequest) {
    mCurrentRequest->CancelAndForgetObserver(NS_BINDING_ABORTED);
    mCurrentRequest = nullptr;
    cancelled = true;
  }
  if (mPendingRequest) {
    mPendingRequest->CancelAndForgetObserver(NS_BINDING_ABORTED);
    mPendingRequest = nullptr;
    cancelled = true;
  }
  return cancelled;
}

// Only HTMLInputElement.h overrides this for <img> tags
// all other subclasses use this one, i.e. ignore referrer attributes
mozilla::net::ReferrerPolicy
nsImageLoadingContent::GetImageReferrerPolicy()
{
  return mozilla::net::RP_Unset;
};
