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
#include "nsAutoPtr.h"
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

#include "nsIPresShell.h"

#include "nsIChannel.h"
#include "nsIStreamListener.h"

#include "nsIFrame.h"
#include "nsIDOMNode.h"

#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsIContentPolicy.h"
#include "nsSVGEffects.h"

#include "gfxPrefs.h"

#include "mozAutoDocUpdate.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/Preferences.h"

#ifdef LoadImage
// Undefine LoadImage to prevent naming conflict with Windows.
#undef LoadImage
#endif

using namespace mozilla;

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
    mStateChangerDepth(0),
    mCurrentRequestRegistered(false),
    mPendingRequestRegistered(false),
    mFrameCreateCalled(false)
{
  if (!nsContentUtils::GetImgLoaderForChannel(nullptr, nullptr)) {
    mLoadingEnabled = false;
  }

  bool isInconsistent;
  mMostRecentRequestChange = TimeStamp::ProcessCreation(isInconsistent);
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
    nsAutoScriptBlocker scriptBlocker;

    for (ImageObserver* observer = &mObserverList, *next; observer;
         observer = next) {
      next = observer->mNext;
      if (observer->mObserver) {
        observer->mObserver->Notify(aRequest, aType, aData);
      }
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
  } else {
    FireEvent(NS_LITERAL_STRING("error"));
  }

  nsCOMPtr<nsINode> thisNode = do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  nsSVGEffects::InvalidateDirectRenderingObservers(thisNode->AsElement());

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

  if (frame->IsVisibleOrMayBecomeVisibleSoon()) {
    return;  // Nothing to do.
  }

  nsPresContext* presContext = frame->PresContext();
  if (!presContext) {
    return;
  }

  nsIPresShell* presShell = presContext->PresShell();
  if (!presShell) {
    return;
  }

  presShell->MarkFrameVisible(frame, VisibilityCounter::IN_DISPLAYPORT);
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

NS_IMETHODIMP
nsImageLoadingContent::GetLoadingEnabled(bool *aLoadingEnabled)
{
  *aLoadingEnabled = mLoadingEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::SetLoadingEnabled(bool aLoadingEnabled)
{
  if (nsContentUtils::GetImgLoaderForChannel(nullptr, nullptr)) {
    mLoadingEnabled = aLoadingEnabled;
  }
  return NS_OK;
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
nsImageLoadingContent::AddObserver(imgINotificationObserver* aObserver)
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
nsImageLoadingContent::RemoveObserver(imgINotificationObserver* aObserver)
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

  mFrameCreateCalled = true;

  TrackImage(mCurrentRequest);
  TrackImage(mPendingRequest);

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

  mFrameCreateCalled = false;

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
    presShell->MarkFrameNonvisible(aFrame);
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

already_AddRefed<nsIStreamListener>
nsImageLoadingContent::LoadImageWithChannel(nsIChannel* aChannel,
                                            ErrorResult& aError)
{
  imgLoader* loader =
    nsContentUtils::GetImgLoaderForChannel(aChannel, GetOurOwnerDoc());
  if (!loader) {
    aError.Throw(NS_ERROR_NULL_POINTER);
    return nullptr;
  }

  nsCOMPtr<nsIDocument> doc = GetOurOwnerDoc();
  if (!doc) {
    // Don't bother
    return nullptr;
  }

  // XXX what should we do with content policies here, if anything?
  // Shouldn't that be done before the start of the load?
  // XXX what about shouldProcess?

  // Our state might change. Watch it.
  AutoStateChanger changer(this, true);

  // Do the load.
  nsCOMPtr<nsIStreamListener> listener;
  RefPtr<imgRequestProxy>& req = PrepareNextRequest(eImageLoadType_Normal);
  nsresult rv = loader->
    LoadImageWithChannel(aChannel, this, doc,
                         getter_AddRefs(listener),
                         getter_AddRefs(req));
  if (NS_SUCCEEDED(rv)) {
    TrackImage(req);
    ResetAnimationIfNeeded();
  } else {
    MOZ_ASSERT(!req, "Shouldn't have non-null request here");
    // If we don't have a current URI, we might as well store this URI so people
    // know what we tried (and failed) to load.
    if (!mCurrentRequest)
      aChannel->GetURI(getter_AddRefs(mCurrentURI));
    FireEvent(NS_LITERAL_STRING("error"));
    aError.Throw(rv);
  }
  return listener.forget();
}

NS_IMETHODIMP
nsImageLoadingContent::LoadImageWithChannel(nsIChannel* aChannel,
                                            nsIStreamListener** aListener)
{
  NS_ENSURE_ARG_POINTER(aListener);

  ErrorResult result;
  *aListener = LoadImageWithChannel(aChannel, result).take();
  return result.StealNSResult();
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
  nsresult rv = LoadImage(currentURI, true, notify, loadType, nullptr,
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

NS_IMETHODIMP
nsImageLoadingContent::BlockOnload(imgIRequest* aRequest)
{
  if (aRequest == mCurrentRequest) {
    NS_ASSERTION(!(mCurrentRequestFlags & REQUEST_BLOCKS_ONLOAD),
                 "Double BlockOnload!?");
    mCurrentRequestFlags |= REQUEST_BLOCKS_ONLOAD;
  } else if (aRequest == mPendingRequest) {
    NS_ASSERTION(!(mPendingRequestFlags & REQUEST_BLOCKS_ONLOAD),
                 "Double BlockOnload!?");
    mPendingRequestFlags |= REQUEST_BLOCKS_ONLOAD;
  } else {
    return NS_OK;
  }

  nsIDocument* doc = GetOurCurrentDoc();
  if (doc) {
    doc->BlockOnload();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::UnblockOnload(imgIRequest* aRequest)
{
  if (aRequest == mCurrentRequest) {
    NS_ASSERTION(mCurrentRequestFlags & REQUEST_BLOCKS_ONLOAD,
                 "Double UnblockOnload!?");
    mCurrentRequestFlags &= ~REQUEST_BLOCKS_ONLOAD;
  } else if (aRequest == mPendingRequest) {
    NS_ASSERTION(mPendingRequestFlags & REQUEST_BLOCKS_ONLOAD,
                 "Double UnblockOnload!?");
    mPendingRequestFlags &= ~REQUEST_BLOCKS_ONLOAD;
  } else {
    return NS_OK;
  }

  nsIDocument* doc = GetOurCurrentDoc();
  if (doc) {
    doc->UnblockOnload(false);
  }

  return NS_OK;
}

/*
 * Non-interface methods
 */

nsresult
nsImageLoadingContent::LoadImage(const nsAString& aNewURI,
                                 bool aForce,
                                 bool aNotify,
                                 ImageLoadType aImageLoadType)
{
  // First, get a document (needed for security checks and the like)
  nsIDocument* doc = GetOurOwnerDoc();
  if (!doc) {
    // No reason to bother, I think...
    return NS_OK;
  }

  nsCOMPtr<nsIURI> imageURI;
  nsresult rv = StringToURI(aNewURI, doc, getter_AddRefs(imageURI));
  NS_ENSURE_SUCCESS(rv, rv);
  // XXXbiesi fire onerror if that failed?

  bool equal;

  if (aNewURI.IsEmpty() &&
      doc->GetDocumentURI() &&
      NS_SUCCEEDED(doc->GetDocumentURI()->EqualsExceptRef(imageURI, &equal)) &&
      equal)  {

    // Loading an embedded img from the same URI as the document URI will not work
    // as a resource cannot recursively embed itself. Attempting to do so generally
    // results in having to pre-emptively close down an in-flight HTTP transaction
    // and then incurring the significant cost of establishing a new TCP channel.
    // This is generally triggered from <img src="">
    // In light of that, just skip loading it..
    // Do make sure to drop our existing image, if any
    CancelImageRequests(aNotify);
    return NS_OK;
  }

  NS_TryToSetImmutable(imageURI);

  return LoadImage(imageURI, aForce, aNotify, aImageLoadType, doc);
}

nsresult
nsImageLoadingContent::LoadImage(nsIURI* aNewURI,
                                 bool aForce,
                                 bool aNotify,
                                 ImageLoadType aImageLoadType,
                                 nsIDocument* aDocument,
                                 nsLoadFlags aLoadFlags)
{
  if (!mLoadingEnabled) {
    // XXX Why fire an error here? seems like the callers to SetLoadingEnabled
    // don't want/need it.
    FireEvent(NS_LITERAL_STRING("error"));
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
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  MOZ_ASSERT(thisContent &&
             thisContent->NodePrincipal() == aDocument->NodePrincipal(),
             "Principal mismatch?");
#endif

  // Are we blocked?
  int16_t cpDecision = nsIContentPolicy::REJECT_REQUEST;
  nsContentPolicyType policyType = PolicyTypeForLoad(aImageLoadType);

  nsContentUtils::CanLoadImage(aNewURI,
                               static_cast<nsIImageLoadingContent*>(this),
                               aDocument,
                               aDocument->NodePrincipal(),
                               &cpDecision,
                               policyType);
  if (!NS_CP_ACCEPTED(cpDecision)) {
    FireEvent(NS_LITERAL_STRING("error"));
    SetBlockedRequest(aNewURI, cpDecision);
    return NS_OK;
  }

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

  // Not blocked. Do the load.
  RefPtr<imgRequestProxy>& req = PrepareNextRequest(aImageLoadType);
  nsCOMPtr<nsIContent> content =
      do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  nsCOMPtr<nsINode> thisNode =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  nsresult rv = nsContentUtils::LoadImage(aNewURI,
                                          thisNode,
                                          aDocument,
                                          aDocument->NodePrincipal(),
                                          aDocument->GetDocumentURI(),
                                          referrerPolicy,
                                          this, loadFlags,
                                          content->LocalName(),
                                          getter_AddRefs(req),
                                          policyType);

  // Tell the document to forget about the image preload, if any, for
  // this URI, now that we might have another imgRequestProxy for it.
  // That way if we get canceled later the image load won't continue.
  aDocument->ForgetImagePreload(aNewURI);

  if (NS_SUCCEEDED(rv)) {
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
    return NS_OK;
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

  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  if (!thisContent) {
    return;
  }

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
  nsresult rv = aRequest->Clone(this, getter_AddRefs(req));
  if (NS_SUCCEEDED(rv)) {
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
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ENSURE_TRUE(thisContent, nullptr);

  return thisContent->OwnerDoc();
}

nsIDocument*
nsImageLoadingContent::GetOurCurrentDoc()
{
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ENSURE_TRUE(thisContent, nullptr);

  return thisContent->GetComposedDoc();
}

nsIFrame*
nsImageLoadingContent::GetOurPrimaryFrame()
{
  nsCOMPtr<nsIContent> thisContent =
    do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  return thisContent->GetPrimaryFrame();
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
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  NS_ASSERTION(thisContent, "An image loading content must be an nsIContent");
  nsCOMPtr<nsIURI> baseURL = thisContent->GetBaseURI();

  // (2) Get the charset
  const nsAFlatCString &charset = aDocument->GetDocumentCharacterSet();

  // (3) Construct the silly thing
  return NS_NewURI(aURI,
                   aSpec,
                   charset.IsEmpty() ? nullptr : charset.get(),
                   baseURL,
                   nsContentUtils::GetIOService());
}

nsresult
nsImageLoadingContent::FireEvent(const nsAString& aEventType)
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

  return NS_OK;
}

RefPtr<imgRequestProxy>&
nsImageLoadingContent::PrepareNextRequest(ImageLoadType aImageLoadType)
{
  nsImageFrame* frame = do_QueryFrame(GetOurPrimaryFrame());
  if (frame) {
    // Detect JavaScript-based animations created by changing the |src|
    // attribute on a timer.
    TimeStamp now = TimeStamp::Now();
    TimeDuration threshold =
      TimeDuration::FromMilliseconds(
        gfxPrefs::ImageInferSrcAnimationThresholdMS());

    // If the length of time between request changes is less than the threshold,
    // then force sync decoding to eliminate flicker from the animation.
    frame->SetForceSyncDecoding(now - mMostRecentRequestChange < threshold);

    mMostRecentRequestChange = now;
  }

  // If we don't have a usable current request, get rid of any half-baked
  // request that might be sitting there and make this one current.
  if (!HaveSize(mCurrentRequest))
    return PrepareCurrentRequest(aImageLoadType);

  // Otherwise, make it pending.
  return PreparePendingRequest(aImageLoadType);
}

void
nsImageLoadingContent::SetBlockedRequest(nsIURI* aURI, int16_t aContentDecision)
{
  // Sanity
  MOZ_ASSERT(!NS_CP_ACCEPTED(aContentDecision), "Blocked but not?");

  // We do some slightly illogical stuff here to maintain consistency with
  // old behavior that people probably depend on. Even in the case where the
  // new image is blocked, the old one should really be canceled with the
  // reason "image source changed". However, apparently there's some abuse
  // over in nsImageFrame where the displaying of the "broken" icon for the
  // next image depends on the cancel reason of the previous image. ugh.
  // XXX(seth): So shouldn't we fix nsImageFrame?!
  ClearPendingRequest(NS_ERROR_IMAGE_BLOCKED,
                      Some(OnNonvisible::DISCARD_IMAGES));

  // For the blocked case, we only want to cancel the existing current request
  // if size is not available. bz says the web depends on this behavior.
  if (!HaveSize(mCurrentRequest)) {

    mImageBlockingStatus = aContentDecision;
    uint32_t keepFlags = mCurrentRequestFlags & REQUEST_IS_IMAGESET;
    ClearCurrentRequest(NS_ERROR_IMAGE_BLOCKED,
                        Some(OnNonvisible::DISCARD_IMAGES));

    // We still want to remember what URI we were and if it was an imageset,
    // despite not having an actual request. These are both cleared as part of
    // ClearCurrentRequest() before a new request is started.
    mCurrentURI = aURI;
    mCurrentRequestFlags = keepFlags;
  }
}

RefPtr<imgRequestProxy>&
nsImageLoadingContent::PrepareCurrentRequest(ImageLoadType aImageLoadType)
{
  // Blocked images go through SetBlockedRequest, which is a separate path. For
  // everything else, we're unblocked.
  mImageBlockingStatus = nsIContentPolicy::ACCEPT;

  // Get rid of anything that was there previously.
  ClearCurrentRequest(NS_ERROR_IMAGE_SRC_CHANGED,
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
  ClearPendingRequest(NS_ERROR_IMAGE_SRC_CHANGED,
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
  mPendingRequest = nullptr;
  mCurrentRequestFlags = mPendingRequestFlags;
  mPendingRequestFlags = 0;
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
  mPendingRequest->CancelAndForgetObserver(aReason);
  mPendingRequest = nullptr;
  mPendingRequestFlags = 0;
}

bool*
nsImageLoadingContent::GetRegisteredFlagForRequest(imgIRequest* aRequest)
{
  if (aRequest == mCurrentRequest) {
    return &mCurrentRequestRegistered;
  } else if (aRequest == mPendingRequest) {
    return &mPendingRequestRegistered;
  } else {
    return nullptr;
  }
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

  if (mCurrentRequestFlags & REQUEST_BLOCKS_ONLOAD)
    aDocument->BlockOnload();
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

  if (mCurrentRequestFlags & REQUEST_BLOCKS_ONLOAD)
    doc->UnblockOnload(false);
}

void
nsImageLoadingContent::OnVisibilityChange(Visibility aOldVisibility,
                                          Visibility aNewVisibility,
                                          const Maybe<OnNonvisible>& aNonvisibleAction)
{
  switch (aNewVisibility) {
    case Visibility::MAY_BECOME_VISIBLE:
    case Visibility::IN_DISPLAYPORT:
      if (aOldVisibility == Visibility::NONVISIBLE) {
        TrackImage(mCurrentRequest);
        TrackImage(mPendingRequest);
      }
      break;

    case Visibility::NONVISIBLE:
      UntrackImage(mCurrentRequest, aNonvisibleAction);
      UntrackImage(mPendingRequest, aNonvisibleAction);
      break;

    case Visibility::UNTRACKED:
      MOZ_ASSERT_UNREACHABLE("Shouldn't notify for untracked visibility");
      break;
  }
}

void
nsImageLoadingContent::TrackImage(imgIRequest* aImage)
{
  if (!aImage)
    return;

  MOZ_ASSERT(aImage == mCurrentRequest || aImage == mPendingRequest,
             "Why haven't we heard of this request?");

  nsIDocument* doc = GetOurCurrentDoc();
  if (!doc) {
    return;
  }

  // We only want to track this request if we're visible. Ordinarily we check
  // whether our frame considers itself visible, but in cases where
  // GetOurPrimaryFrame() cannot obtain a frame (e.g. <feImage>), we assume
  // we're visible if FrameCreated() was called.
  nsIFrame* frame = GetOurPrimaryFrame();
  if ((frame && !frame->IsVisibleOrMayBecomeVisibleSoon()) ||
      (!frame && !mFrameCreateCalled)) {
    return;
  }

  if (aImage == mCurrentRequest && !(mCurrentRequestFlags & REQUEST_IS_TRACKED)) {
    mCurrentRequestFlags |= REQUEST_IS_TRACKED;
    doc->AddImage(mCurrentRequest);
  }
  if (aImage == mPendingRequest && !(mPendingRequestFlags & REQUEST_IS_TRACKED)) {
    mPendingRequestFlags |= REQUEST_IS_TRACKED;
    doc->AddImage(mPendingRequest);
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
      doc->RemoveImage(mCurrentRequest,
                       aNonvisibleAction == Some(OnNonvisible::DISCARD_IMAGES)
                         ? nsIDocument::REQUEST_DISCARD
                         : 0);
    } else if (aNonvisibleAction == Some(OnNonvisible::DISCARD_IMAGES)) {
      // If we're not in the document we may still need to be discarded.
      aImage->RequestDiscard();
    }
  }
  if (aImage == mPendingRequest) {
    if (doc && (mPendingRequestFlags & REQUEST_IS_TRACKED)) {
      mPendingRequestFlags &= ~REQUEST_IS_TRACKED;
      doc->RemoveImage(mPendingRequest,
                       aNonvisibleAction == Some(OnNonvisible::DISCARD_IMAGES)
                         ? nsIDocument::REQUEST_DISCARD
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
  aDest->mCurrentRequest = nsContentUtils::GetStaticRequest(mCurrentRequest);
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

// Only HTMLInputElement.h overrides this for <img> tags
// all other subclasses use this one, i.e. ignore referrer attributes
mozilla::net::ReferrerPolicy
nsImageLoadingContent::GetImageReferrerPolicy()
{
  return mozilla::net::RP_Unset;
};
