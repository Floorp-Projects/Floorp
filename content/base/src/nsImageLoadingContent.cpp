/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp tw=78 sw=2 et ts=2
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
#include "nsEventStates.h"

#include "nsIChannel.h"
#include "nsIStreamListener.h"

#include "nsIFrame.h"
#include "nsIDOMNode.h"

#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsIContentPolicy.h"
#include "nsEventDispatcher.h"
#include "nsSVGEffects.h"

#include "mozAutoDocUpdate.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ScriptSettings.h"

#if defined(XP_WIN)
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
    mFireEventsOnDecode(false),
    mNewRequestsWillNeedAnimationReset(false),
    mStateChangerDepth(0),
    mCurrentRequestRegistered(false),
    mPendingRequestRegistered(false),
    mFrameCreateCalled(false),
    mVisibleCount(0)
{
  if (!nsContentUtils::GetImgLoaderForChannel(nullptr)) {
    mLoadingEnabled = false;
  }
}

void
nsImageLoadingContent::DestroyImageLoadingContent()
{
  // Cancel our requests so they won't hold stale refs to us
  // NB: Don't ask to discard the images here.
  ClearCurrentRequest(NS_BINDING_ABORTED, 0);
  ClearPendingRequest(NS_BINDING_ABORTED, 0);
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
    NS_ABORT_IF_FALSE(aRequest, "no request?");

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
    nsresult status =
        reqStatus & imgIRequest::STATUS_ERROR ? NS_ERROR_FAILURE : NS_OK;
    return OnStopRequest(aRequest, status);
  }

  if (aType == imgINotificationObserver::DECODE_COMPLETE && mFireEventsOnDecode) {
    mFireEventsOnDecode = false;

    uint32_t reqStatus;
    aRequest->GetImageStatus(&reqStatus);
    if (reqStatus & imgIRequest::STATUS_ERROR) {
      FireEvent(NS_LITERAL_STRING("error"));
    } else {
      FireEvent(NS_LITERAL_STRING("load"));
    }

    UpdateImageState(true);
  }

  return NS_OK;
}

nsresult
nsImageLoadingContent::OnStopRequest(imgIRequest* aRequest,
                                     nsresult aStatus)
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
  NS_ABORT_IF_FALSE(aRequest == mCurrentRequest,
                    "One way or another, we should be current by now");

  // We just loaded all the data we're going to get. If we're visible and
  // haven't done an initial paint (*), we want to make sure the image starts
  // decoding immediately, for two reasons:
  //
  // 1) This image is sitting idle but might need to be decoded as soon as we
  // start painting, in which case we've wasted time.
  //
  // 2) We want to block onload until all visible images are decoded. We do this
  // by blocking onload until all in-progress decodes get at least one frame
  // decoded. However, if all the data comes in while painting is suppressed
  // (ie, before the initial paint delay is finished), we fire onload without
  // doing a paint first. This means that decode-on-draw images don't start
  // decoding, so we can't wait for them to finish. See bug 512435.
  //
  // (*) IsPaintingSuppressed returns false if we haven't gotten the initial
  // reflow yet, so we have to test !DidInitialize || IsPaintingSuppressed.
  // It's possible for painting to be suppressed for reasons other than the
  // initial paint delay (for example, being in the bfcache), but we probably
  // aren't loading images in those situations.

  // XXXkhuey should this be GetOurCurrentDoc?  Decoding if we're not in
  // the document seems silly.
  bool startedDecoding = false;
  nsIDocument* doc = GetOurOwnerDoc();
  nsIPresShell* shell = doc ? doc->GetShell() : nullptr;
  if (shell && shell->IsVisible() &&
      (!shell->DidInitialize() || shell->IsPaintingSuppressed())) {

    // If we've gotten a frame and that frame has called FrameCreate and that
    // frame has been reflowed then we know that it checked it's own visibility
    // so we can trust our visible count and we don't start decode if we are not
    // visible.
    nsIFrame* f = GetOurPrimaryFrame();
    if (!mFrameCreateCalled || !f || (f->GetStateBits() & NS_FRAME_FIRST_REFLOW) ||
        mVisibleCount > 0 || shell->AssumeAllImagesVisible()) {
      if (NS_SUCCEEDED(mCurrentRequest->StartDecoding())) {
        startedDecoding = true;
      }
    }
  }

  // We want to give the decoder a chance to find errors. If we haven't found
  // an error yet and we've started decoding, either from the above
  // StartDecoding or from some other place, we must only fire these events
  // after we finish decoding.
  uint32_t reqStatus;
  aRequest->GetImageStatus(&reqStatus);
  if (NS_SUCCEEDED(aStatus) && !(reqStatus & imgIRequest::STATUS_ERROR) &&
      (reqStatus & imgIRequest::STATUS_DECODE_STARTED ||
       (startedDecoding && !(reqStatus & imgIRequest::STATUS_DECODE_COMPLETE)))) {
    mFireEventsOnDecode = true;
  } else {
    // Fire the appropriate DOM event.
    if (NS_SUCCEEDED(aStatus)) {
      FireEvent(NS_LITERAL_STRING("load"));
    } else {
      FireEvent(NS_LITERAL_STRING("error"));
    }
  }

  nsCOMPtr<nsINode> thisNode = do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));
  nsSVGEffects::InvalidateDirectRenderingObservers(thisNode->AsElement());

  return NS_OK;
}

void
nsImageLoadingContent::OnUnlockedDraw()
{
  if (mVisibleCount > 0) {
    // We should already be marked as visible, there is nothing more we can do.
    return;
  }

  nsPresContext* presContext = GetFramePresContext();
  if (!presContext)
    return;

  nsIPresShell* presShell = presContext->PresShell();
  if (!presShell)
    return;

  presShell->EnsureImageInVisibleList(this);
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
  if (nsContentUtils::GetImgLoaderForChannel(nullptr)) {
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

NS_IMETHODIMP
nsImageLoadingContent::AddObserver(imgINotificationObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  if (!mObserverList.mObserver) {
    mObserverList.mObserver = aObserver;
    // Don't touch the linking of the list!
    return NS_OK;
  }

  // otherwise we have to create a new entry

  ImageObserver* observer = &mObserverList;
  while (observer->mNext) {
    observer = observer->mNext;
  }

  observer->mNext = new ImageObserver(aObserver);
  if (! observer->mNext) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

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

  return result.ErrorCode();
}

NS_IMETHODIMP_(void)
nsImageLoadingContent::FrameCreated(nsIFrame* aFrame)
{
  NS_ASSERTION(aFrame, "aFrame is null");

  mFrameCreateCalled = true;

  if (aFrame->HasAnyStateBits(NS_FRAME_IN_POPUP)) {
    // Assume all images in popups are visible.
    IncrementVisibleCount();
  }

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
  if (mCurrentRequest) {
    nsLayoutUtils::DeregisterImageRequest(GetFramePresContext(),
                                          mCurrentRequest,
                                          &mCurrentRequestRegistered);
  }

  if (mPendingRequest) {
    nsLayoutUtils::DeregisterImageRequest(GetFramePresContext(),
                                          mPendingRequest,
                                          &mPendingRequestRegistered);
  }

  UntrackImage(mCurrentRequest);
  UntrackImage(mPendingRequest);

  if (aFrame->HasAnyStateBits(NS_FRAME_IN_POPUP)) {
    // We assume all images in popups are visible, so this decrement balances
    // out the increment in FrameCreated above.
    DecrementVisibleCount();
  }
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
  return result.ErrorCode();
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
  return result.ErrorCode();
}

already_AddRefed<nsIStreamListener>
nsImageLoadingContent::LoadImageWithChannel(nsIChannel* aChannel,
                                            ErrorResult& aError)
{
  if (!nsContentUtils::GetImgLoaderForChannel(aChannel)) {
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
  nsRefPtr<imgRequestProxy>& req = PrepareNextRequest();
  nsresult rv = nsContentUtils::GetImgLoaderForChannel(aChannel)->
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
  return result.ErrorCode();
}

void
nsImageLoadingContent::ForceReload(ErrorResult& aError)
{
  nsCOMPtr<nsIURI> currentURI;
  GetCurrentURI(getter_AddRefs(currentURI));
  if (!currentURI) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  nsresult rv = LoadImage(currentURI, true, true, nullptr, nsIRequest::VALIDATE_ALWAYS);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
  }
}

NS_IMETHODIMP nsImageLoadingContent::ForceReload()
{
  ErrorResult result;
  ForceReload(result);
  return result.ErrorCode();
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

void
nsImageLoadingContent::IncrementVisibleCount()
{
  mVisibleCount++;
  if (mVisibleCount == 1) {
    TrackImage(mCurrentRequest);
    TrackImage(mPendingRequest);
  }
}

void
nsImageLoadingContent::DecrementVisibleCount()
{
  NS_ASSERTION(mVisibleCount > 0, "visible count should be positive here");
  mVisibleCount--;

  if (mVisibleCount == 0) {
    UntrackImage(mCurrentRequest);
    UntrackImage(mPendingRequest);
  }
}

uint32_t
nsImageLoadingContent::GetVisibleCount()
{
  return mVisibleCount;
}

/*
 * Non-interface methods
 */

nsresult
nsImageLoadingContent::LoadImage(const nsAString& aNewURI,
                                 bool aForce,
                                 bool aNotify)
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

  return LoadImage(imageURI, aForce, aNotify, doc);
}

nsresult
nsImageLoadingContent::LoadImage(nsIURI* aNewURI,
                                 bool aForce,
                                 bool aNotify,
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
  NS_ABORT_IF_FALSE(thisContent &&
                    thisContent->NodePrincipal() == aDocument->NodePrincipal(),
                    "Principal mismatch?");
#endif

  // Are we blocked?
  int16_t cpDecision = nsIContentPolicy::REJECT_REQUEST;
  nsContentUtils::CanLoadImage(aNewURI,
                               static_cast<nsIImageLoadingContent*>(this),
                               aDocument,
                               aDocument->NodePrincipal(),
                               &cpDecision);
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

  // Not blocked. Do the load.
  nsRefPtr<imgRequestProxy>& req = PrepareNextRequest();
  nsresult rv;
  rv = nsContentUtils::LoadImage(aNewURI, aDocument,
                                 aDocument->NodePrincipal(),
                                 aDocument->GetDocumentURI(),
                                 this, loadFlags,
                                 getter_AddRefs(req));
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
nsImageLoadingContent::ForceImageState(bool aForce, nsEventStates::InternalType aState)
{
  mIsImageStateForced = aForce;
  mForcedImageState = nsEventStates(aState);
  return NS_OK;
}

nsEventStates
nsImageLoadingContent::ImageState() const
{
  if (mIsImageStateForced) {
    return mForcedImageState;
  }

  nsEventStates states;

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
    // Ignore this call; we'll update our state when the outermost state
    // changer is destroyed. Need this to work around the fact that some libpr0n
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
  ClearPendingRequest(NS_BINDING_ABORTED, REQUEST_DISCARD);
  ClearCurrentRequest(NS_BINDING_ABORTED, REQUEST_DISCARD);
}

nsresult
nsImageLoadingContent::UseAsPrimaryRequest(imgRequestProxy* aRequest,
                                           bool aNotify)
{
  // Our state will change. Watch it.
  AutoStateChanger changer(this, aNotify);

  // Get rid if our existing images
  ClearPendingRequest(NS_BINDING_ABORTED, REQUEST_DISCARD);
  ClearCurrentRequest(NS_BINDING_ABORTED, REQUEST_DISCARD);

  // Clone the request we were given.
  nsRefPtr<imgRequestProxy>& req = PrepareNextRequest();
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

  return thisContent->GetCurrentDoc();
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
  // We have to fire the event asynchronously so that we won't go into infinite
  // loops in cases when onLoad handlers reset the src and the new src is in
  // cache.

  nsCOMPtr<nsINode> thisNode = do_QueryInterface(static_cast<nsIImageLoadingContent*>(this));

  nsRefPtr<AsyncEventDispatcher> loadBlockingAsyncDispatcher =
    new LoadBlockingAsyncEventDispatcher(thisNode, aEventType, false, false);
  loadBlockingAsyncDispatcher->PostDOMEvent();

  return NS_OK;
}

nsRefPtr<imgRequestProxy>&
nsImageLoadingContent::PrepareNextRequest()
{
  // If we don't have a usable current request, get rid of any half-baked
  // request that might be sitting there and make this one current.
  if (!HaveSize(mCurrentRequest))
    return PrepareCurrentRequest();

  // Otherwise, make it pending.
  return PreparePendingRequest();
}

void
nsImageLoadingContent::SetBlockedRequest(nsIURI* aURI, int16_t aContentDecision)
{
  // Sanity
  NS_ABORT_IF_FALSE(!NS_CP_ACCEPTED(aContentDecision), "Blocked but not?");

  // We do some slightly illogical stuff here to maintain consistency with
  // old behavior that people probably depend on. Even in the case where the
  // new image is blocked, the old one should really be canceled with the
  // reason "image source changed". However, apparently there's some abuse
  // over in nsImageFrame where the displaying of the "broken" icon for the
  // next image depends on the cancel reason of the previous image. ugh.
  ClearPendingRequest(NS_ERROR_IMAGE_BLOCKED, REQUEST_DISCARD);

  // For the blocked case, we only want to cancel the existing current request
  // if size is not available. bz says the web depends on this behavior.
  if (!HaveSize(mCurrentRequest)) {

    mImageBlockingStatus = aContentDecision;
    ClearCurrentRequest(NS_ERROR_IMAGE_BLOCKED, REQUEST_DISCARD);

    // We still want to remember what URI we were despite not having an actual
    // request.
    mCurrentURI = aURI;
  }
}

nsRefPtr<imgRequestProxy>&
nsImageLoadingContent::PrepareCurrentRequest()
{
  // Blocked images go through SetBlockedRequest, which is a separate path. For
  // everything else, we're unblocked.
  mImageBlockingStatus = nsIContentPolicy::ACCEPT;

  // Get rid of anything that was there previously.
  ClearCurrentRequest(NS_ERROR_IMAGE_SRC_CHANGED, REQUEST_DISCARD);

  if (mNewRequestsWillNeedAnimationReset) {
    mCurrentRequestFlags |= REQUEST_NEEDS_ANIMATION_RESET;
  }

  // Return a reference.
  return mCurrentRequest;
}

nsRefPtr<imgRequestProxy>&
nsImageLoadingContent::PreparePendingRequest()
{
  // Get rid of anything that was there previously.
  ClearPendingRequest(NS_ERROR_IMAGE_SRC_CHANGED, REQUEST_DISCARD);

  if (mNewRequestsWillNeedAnimationReset) {
    mPendingRequestFlags |= REQUEST_NEEDS_ANIMATION_RESET;
  }

  // Return a reference.
  return mPendingRequest;
}

namespace {

class ImageRequestAutoLock
{
public:
  ImageRequestAutoLock(imgIRequest* aRequest)
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

} // anonymous namespace

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

  PrepareCurrentRequest() = mPendingRequest;
  mPendingRequest = nullptr;
  mCurrentRequestFlags = mPendingRequestFlags;
  mPendingRequestFlags = 0;
  ResetAnimationIfNeeded();
}

void
nsImageLoadingContent::ClearCurrentRequest(nsresult aReason,
                                           uint32_t aFlags)
{
  if (!mCurrentRequest) {
    // Even if we didn't have a current request, we might have been keeping
    // a URI as a placeholder for a failed load. Clear that now.
    mCurrentURI = nullptr;
    return;
  }
  NS_ABORT_IF_FALSE(!mCurrentURI,
                    "Shouldn't have both mCurrentRequest and mCurrentURI!");

  // Deregister this image from the refresh driver so it no longer receives
  // notifications.
  nsLayoutUtils::DeregisterImageRequest(GetFramePresContext(), mCurrentRequest,
                                        &mCurrentRequestRegistered);

  // Clean up the request.
  UntrackImage(mCurrentRequest, aFlags);
  mCurrentRequest->CancelAndForgetObserver(aReason);
  mCurrentRequest = nullptr;
  mCurrentRequestFlags = 0;
}

void
nsImageLoadingContent::ClearPendingRequest(nsresult aReason,
                                           uint32_t aFlags)
{
  if (!mPendingRequest)
    return;

  // Deregister this image from the refresh driver so it no longer receives
  // notifications.
  nsLayoutUtils::DeregisterImageRequest(GetFramePresContext(), mPendingRequest,
                                        &mPendingRequestRegistered);

  UntrackImage(mPendingRequest, aFlags);
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
nsImageLoadingContent::TrackImage(imgIRequest* aImage)
{
  if (!aImage)
    return;

  MOZ_ASSERT(aImage == mCurrentRequest || aImage == mPendingRequest,
             "Why haven't we heard of this request?");

  nsIDocument* doc = GetOurCurrentDoc();
  if (doc && (mFrameCreateCalled || GetOurPrimaryFrame()) &&
      (mVisibleCount > 0)) {
    if (aImage == mCurrentRequest && !(mCurrentRequestFlags & REQUEST_IS_TRACKED)) {
      mCurrentRequestFlags |= REQUEST_IS_TRACKED;
      doc->AddImage(mCurrentRequest);
    }
    if (aImage == mPendingRequest && !(mPendingRequestFlags & REQUEST_IS_TRACKED)) {
      mPendingRequestFlags |= REQUEST_IS_TRACKED;
      doc->AddImage(mPendingRequest);
    }
  }
}

void
nsImageLoadingContent::UntrackImage(imgIRequest* aImage, uint32_t aFlags /* = 0 */)
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
                       (aFlags & REQUEST_DISCARD) ? nsIDocument::REQUEST_DISCARD : 0);
    }
    else if (aFlags & REQUEST_DISCARD) {
      // If we're not in the document we may still need to be discarded.
      aImage->RequestDiscard();
    }
  }
  if (aImage == mPendingRequest) {
    if (doc && (mPendingRequestFlags & REQUEST_IS_TRACKED)) {
      mPendingRequestFlags &= ~REQUEST_IS_TRACKED;
      doc->RemoveImage(mPendingRequest,
                       (aFlags & REQUEST_DISCARD) ? nsIDocument::REQUEST_DISCARD : 0);
    }
    else if (aFlags & REQUEST_DISCARD) {
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
