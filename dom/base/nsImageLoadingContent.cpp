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
#include "nsIScriptGlobalObject.h"
#include "nsServiceManagerUtils.h"
#include "nsContentList.h"
#include "nsContentPolicyUtils.h"
#include "nsIURI.h"
#include "imgIContainer.h"
#include "imgLoader.h"
#include "imgRequestProxy.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsImageFrame.h"

#include "nsIChannel.h"
#include "nsIStreamListener.h"

#include "nsIFrame.h"

#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsIContentPolicy.h"

#include "mozAutoDocUpdate.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_image.h"
#include "mozilla/SVGImageFrame.h"
#include "mozilla/SVGObserverUtils.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/PContent.h"  // For TextRecognitionResult
#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/ImageTextBinding.h"
#include "mozilla/dom/ImageTracker.h"
#include "mozilla/dom/ReferrerInfo.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/intl/Locale.h"
#include "mozilla/dom/LargestContentfulPaint.h"
#include "mozilla/net/UrlClassifierFeatureFactory.h"
#include "mozilla/widget/TextRecognition.h"

#include "Orientation.h"

#ifdef LoadImage
// Undefine LoadImage to prevent naming conflict with Windows.
#  undef LoadImage
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

const nsAttrValue::EnumTable nsImageLoadingContent::kDecodingTable[] = {
    {"auto", nsImageLoadingContent::ImageDecodingType::Auto},
    {"async", nsImageLoadingContent::ImageDecodingType::Async},
    {"sync", nsImageLoadingContent::ImageDecodingType::Sync},
    {nullptr, 0}};

const nsAttrValue::EnumTable* nsImageLoadingContent::kDecodingTableDefault =
    &nsImageLoadingContent::kDecodingTable[0];

nsImageLoadingContent::nsImageLoadingContent()
    : mObserverList(nullptr),
      mOutstandingDecodePromises(0),
      mRequestGeneration(0),
      mLoadingEnabled(true),
      mLoading(false),
      mNewRequestsWillNeedAnimationReset(false),
      mUseUrgentStartForChannel(false),
      mLazyLoading(false),
      mStateChangerDepth(0),
      mCurrentRequestRegistered(false),
      mPendingRequestRegistered(false),
      mIsStartingImageLoad(false),
      mSyncDecodingHint(false) {
  if (!nsContentUtils::GetImgLoaderForChannel(nullptr, nullptr)) {
    mLoadingEnabled = false;
  }

  mMostRecentRequestChange = TimeStamp::ProcessCreation();
}

void nsImageLoadingContent::Destroy() {
  // Cancel our requests so they won't hold stale refs to us
  // NB: Don't ask to discard the images here.
  RejectDecodePromises(NS_ERROR_DOM_IMAGE_INVALID_REQUEST);
  ClearCurrentRequest(NS_BINDING_ABORTED);
  ClearPendingRequest(NS_BINDING_ABORTED);
}

nsImageLoadingContent::~nsImageLoadingContent() {
  MOZ_ASSERT(!mCurrentRequest && !mPendingRequest, "Destroy not called");
  MOZ_ASSERT(!mObserverList.mObserver && !mObserverList.mNext,
             "Observers still registered?");
  MOZ_ASSERT(mScriptedObservers.IsEmpty(),
             "Scripted observers still registered?");
  MOZ_ASSERT(mOutstandingDecodePromises == 0,
             "Decode promises still unfulfilled?");
  MOZ_ASSERT(mDecodePromises.IsEmpty(), "Decode promises still unfulfilled?");
}

/*
 * imgINotificationObserver impl
 */
void nsImageLoadingContent::Notify(imgIRequest* aRequest, int32_t aType,
                                   const nsIntRect* aData) {
  MOZ_ASSERT(aRequest, "no request?");
  MOZ_ASSERT(aRequest == mCurrentRequest || aRequest == mPendingRequest,
             "Forgot to cancel a previous request?");

  if (aType == imgINotificationObserver::IS_ANIMATED) {
    return OnImageIsAnimated(aRequest);
  }

  if (aType == imgINotificationObserver::UNLOCKED_DRAW) {
    return OnUnlockedDraw();
  }

  {
    // Calling Notify on observers can modify the list of observers so make
    // a local copy.
    AutoTArray<nsCOMPtr<imgINotificationObserver>, 2> observers;
    for (ImageObserver *observer = &mObserverList, *next; observer;
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

      /* Handle image not loading error because source was a tracking URL (or
       * fingerprinting, cryptomining, etc).
       * We make a note of this image node by including it in a dedicated
       * array of blocked tracking nodes under its parent document.
       */
      if (net::UrlClassifierFeatureFactory::IsClassifierBlockingErrorCode(
              errorCode)) {
        Document* doc = GetOurOwnerDoc();
        doc->AddBlockedNodeByClassifier(AsContent());
      }
    }
    nsresult status =
        reqStatus & imgIRequest::STATUS_ERROR ? NS_ERROR_FAILURE : NS_OK;
    return OnLoadComplete(aRequest, status);
  }

  if ((aType == imgINotificationObserver::FRAME_COMPLETE ||
       aType == imgINotificationObserver::FRAME_UPDATE) &&
      mCurrentRequest == aRequest) {
    MaybeResolveDecodePromises();
  }

  if (aType == imgINotificationObserver::DECODE_COMPLETE) {
    nsCOMPtr<imgIContainer> container;
    aRequest->GetImage(getter_AddRefs(container));
    if (container) {
      container->PropagateUseCounters(GetOurOwnerDoc());
    }

    UpdateImageState(true);
  }
}

void nsImageLoadingContent::OnLoadComplete(imgIRequest* aRequest,
                                           nsresult aStatus) {
  uint32_t oldStatus;
  aRequest->GetImageStatus(&oldStatus);

  // XXXjdm This occurs when we have a pending request created, then another
  //       pending request replaces it before the first one is finished.
  //       This begs the question of what the correct behaviour is; we used
  //       to not have to care because we ran this code in OnStopDecode which
  //       wasn't called when the first request was cancelled. For now, I choose
  //       to punt when the given request doesn't appear to have terminated in
  //       an expected state.
  if (!(oldStatus &
        (imgIRequest::STATUS_ERROR | imgIRequest::STATUS_LOAD_COMPLETE))) {
    return;
  }

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
    FireEvent(u"load"_ns);
  } else {
    FireEvent(u"error"_ns);
  }

  Element* element = AsContent()->AsElement();
  SVGObserverUtils::InvalidateDirectRenderingObservers(element);
  MaybeResolveDecodePromises();
  LargestContentfulPaint::MaybeProcessImageForElementTiming(mCurrentRequest,
                                                            element);
}

void nsImageLoadingContent::OnUnlockedDraw() {
  // This notification is only sent for animated images. It's OK for
  // non-animated images to wait until the next frame visibility update to
  // become locked. (And that's preferable, since in the case of scrolling it
  // keeps memory usage minimal.)
  //
  // For animated images, though, we want to mark them visible right away so we
  // can call IncrementAnimationConsumers() on them and they'll start animating.

  nsIFrame* frame = GetOurPrimaryImageFrame();
  if (!frame) {
    return;
  }

  if (frame->GetVisibility() == Visibility::ApproximatelyVisible) {
    // This frame is already marked visible; there's nothing to do.
    return;
  }

  nsPresContext* presContext = frame->PresContext();
  if (!presContext) {
    return;
  }

  PresShell* presShell = presContext->GetPresShell();
  if (!presShell) {
    return;
  }

  presShell->EnsureFrameInApproximatelyVisibleList(frame);
}

void nsImageLoadingContent::OnImageIsAnimated(imgIRequest* aRequest) {
  bool* requestFlag = nullptr;
  if (aRequest == mCurrentRequest) {
    requestFlag = &mCurrentRequestRegistered;
  } else if (aRequest == mPendingRequest) {
    requestFlag = &mPendingRequestRegistered;
  } else {
    MOZ_ASSERT_UNREACHABLE("Which image is this?");
    return;
  }
  nsLayoutUtils::RegisterImageRequest(GetFramePresContext(), aRequest,
                                      requestFlag);
}

static bool IsOurImageFrame(nsIFrame* aFrame) {
  if (nsImageFrame* f = do_QueryFrame(aFrame)) {
    return f->IsForImageLoadingContent();
  }
  return aFrame->IsSVGImageFrame() || aFrame->IsSVGFEImageFrame();
}

nsIFrame* nsImageLoadingContent::GetOurPrimaryImageFrame() {
  nsIFrame* frame = AsContent()->GetPrimaryFrame();
  if (!frame || !IsOurImageFrame(frame)) {
    return nullptr;
  }
  return frame;
}

/*
 * nsIImageLoadingContent impl
 */

void nsImageLoadingContent::SetLoadingEnabled(bool aLoadingEnabled) {
  if (nsContentUtils::GetImgLoaderForChannel(nullptr, nullptr)) {
    mLoadingEnabled = aLoadingEnabled;
  }
}

nsresult nsImageLoadingContent::GetSyncDecodingHint(bool* aHint) {
  *aHint = mSyncDecodingHint;
  return NS_OK;
}

already_AddRefed<Promise> nsImageLoadingContent::QueueDecodeAsync(
    ErrorResult& aRv) {
  Document* doc = GetOurOwnerDoc();
  RefPtr<Promise> promise = Promise::Create(doc->GetScopeObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  class QueueDecodeTask final : public MicroTaskRunnable {
   public:
    QueueDecodeTask(nsImageLoadingContent* aOwner, Promise* aPromise,
                    uint32_t aRequestGeneration)
        : mOwner(aOwner),
          mPromise(aPromise),
          mRequestGeneration(aRequestGeneration) {}

    virtual void Run(AutoSlowOperation& aAso) override {
      mOwner->DecodeAsync(std::move(mPromise), mRequestGeneration);
    }

    virtual bool Suppressed() override {
      nsIGlobalObject* global = mOwner->GetOurOwnerDoc()->GetScopeObject();
      return global && global->IsInSyncOperation();
    }

   private:
    RefPtr<nsImageLoadingContent> mOwner;
    RefPtr<Promise> mPromise;
    uint32_t mRequestGeneration;
  };

  if (++mOutstandingDecodePromises == 1) {
    MOZ_ASSERT(mDecodePromises.IsEmpty());
    doc->RegisterActivityObserver(AsContent()->AsElement());
  }

  auto task = MakeRefPtr<QueueDecodeTask>(this, promise, mRequestGeneration);
  CycleCollectedJSContext::Get()->DispatchToMicroTask(task.forget());
  return promise.forget();
}

void nsImageLoadingContent::DecodeAsync(RefPtr<Promise>&& aPromise,
                                        uint32_t aRequestGeneration) {
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(mOutstandingDecodePromises > mDecodePromises.Length());

  // The request may have gotten updated since the decode call was issued.
  if (aRequestGeneration != mRequestGeneration) {
    aPromise->MaybeReject(NS_ERROR_DOM_IMAGE_INVALID_REQUEST);
    // We never got placed in mDecodePromises, so we must ensure we decrement
    // the counter explicitly.
    --mOutstandingDecodePromises;
    MaybeDeregisterActivityObserver();
    return;
  }

  bool wasEmpty = mDecodePromises.IsEmpty();
  mDecodePromises.AppendElement(std::move(aPromise));
  if (wasEmpty) {
    MaybeResolveDecodePromises();
  }
}

void nsImageLoadingContent::MaybeResolveDecodePromises() {
  if (mDecodePromises.IsEmpty()) {
    return;
  }

  if (!mCurrentRequest) {
    RejectDecodePromises(NS_ERROR_DOM_IMAGE_INVALID_REQUEST);
    return;
  }

  // Only can resolve if our document is the active document. If not we are
  // supposed to reject the promise, even if it was fulfilled successfully.
  if (!GetOurOwnerDoc()->IsCurrentActiveDocument()) {
    RejectDecodePromises(NS_ERROR_DOM_IMAGE_INACTIVE_DOCUMENT);
    return;
  }

  // If any error occurred while decoding, we need to reject first.
  uint32_t status = imgIRequest::STATUS_NONE;
  mCurrentRequest->GetImageStatus(&status);
  if (status & imgIRequest::STATUS_ERROR) {
    RejectDecodePromises(NS_ERROR_DOM_IMAGE_BROKEN);
    return;
  }

  // We need the size to bother with requesting a decode, as we are either
  // blocked on validation or metadata decoding.
  if (!(status & imgIRequest::STATUS_SIZE_AVAILABLE)) {
    return;
  }

  // Check the surface cache status and/or request decoding begin. We do this
  // before LOAD_COMPLETE because we want to start as soon as possible.
  uint32_t flags = imgIContainer::FLAG_HIGH_QUALITY_SCALING |
                   imgIContainer::FLAG_AVOID_REDECODE_FOR_SIZE;
  imgIContainer::DecodeResult decodeResult =
      mCurrentRequest->RequestDecodeWithResult(flags);
  if (decodeResult == imgIContainer::DECODE_REQUESTED) {
    return;
  }
  if (decodeResult == imgIContainer::DECODE_REQUEST_FAILED) {
    RejectDecodePromises(NS_ERROR_DOM_IMAGE_BROKEN);
    return;
  }
  MOZ_ASSERT(decodeResult == imgIContainer::DECODE_SURFACE_AVAILABLE);

  // We can only fulfill the promises once we have all the data.
  if (!(status & imgIRequest::STATUS_LOAD_COMPLETE)) {
    return;
  }

  for (auto& promise : mDecodePromises) {
    promise->MaybeResolveWithUndefined();
  }

  MOZ_ASSERT(mOutstandingDecodePromises >= mDecodePromises.Length());
  mOutstandingDecodePromises -= mDecodePromises.Length();
  mDecodePromises.Clear();
  MaybeDeregisterActivityObserver();
}

void nsImageLoadingContent::RejectDecodePromises(nsresult aStatus) {
  if (mDecodePromises.IsEmpty()) {
    return;
  }

  for (auto& promise : mDecodePromises) {
    promise->MaybeReject(aStatus);
  }

  MOZ_ASSERT(mOutstandingDecodePromises >= mDecodePromises.Length());
  mOutstandingDecodePromises -= mDecodePromises.Length();
  mDecodePromises.Clear();
  MaybeDeregisterActivityObserver();
}

void nsImageLoadingContent::MaybeAgeRequestGeneration(nsIURI* aNewURI) {
  MOZ_ASSERT(mCurrentRequest);

  // If the current request is about to change, we need to verify if the new
  // URI matches the existing current request's URI. If it doesn't, we need to
  // reject any outstanding promises due to the current request mutating as per
  // step 2.2 of the decode API requirements.
  //
  // https://html.spec.whatwg.org/multipage/embedded-content.html#dom-img-decode
  if (aNewURI) {
    nsCOMPtr<nsIURI> currentURI;
    mCurrentRequest->GetURI(getter_AddRefs(currentURI));

    bool equal = false;
    if (NS_SUCCEEDED(aNewURI->Equals(currentURI, &equal)) && equal) {
      return;
    }
  }

  ++mRequestGeneration;
  RejectDecodePromises(NS_ERROR_DOM_IMAGE_INVALID_REQUEST);
}

void nsImageLoadingContent::MaybeDeregisterActivityObserver() {
  if (mOutstandingDecodePromises == 0) {
    MOZ_ASSERT(mDecodePromises.IsEmpty());
    GetOurOwnerDoc()->UnregisterActivityObserver(AsContent()->AsElement());
  }
}

void nsImageLoadingContent::SetSyncDecodingHint(bool aHint) {
  if (mSyncDecodingHint == aHint) {
    return;
  }

  mSyncDecodingHint = aHint;
  MaybeForceSyncDecoding(/* aPrepareNextRequest */ false);
}

void nsImageLoadingContent::MaybeForceSyncDecoding(
    bool aPrepareNextRequest, nsIFrame* aFrame /* = nullptr */) {
  // GetOurPrimaryImageFrame() might not return the frame during frame init.
  nsIFrame* frame = aFrame ? aFrame : GetOurPrimaryImageFrame();
  if (!frame) {
    return;
  }

  bool forceSync = mSyncDecodingHint;
  if (!forceSync && aPrepareNextRequest) {
    // Detect JavaScript-based animations created by changing the |src|
    // attribute on a timer.
    TimeStamp now = TimeStamp::Now();
    TimeDuration threshold = TimeDuration::FromMilliseconds(
        StaticPrefs::image_infer_src_animation_threshold_ms());

    // If the length of time between request changes is less than the threshold,
    // then force sync decoding to eliminate flicker from the animation.
    forceSync = (now - mMostRecentRequestChange < threshold);
    mMostRecentRequestChange = now;
  }

  if (nsImageFrame* imageFrame = do_QueryFrame(frame)) {
    imageFrame->SetForceSyncDecoding(forceSync);
  } else if (SVGImageFrame* svgImageFrame = do_QueryFrame(frame)) {
    svgImageFrame->SetForceSyncDecoding(forceSync);
  }
}

static void ReplayImageStatus(imgIRequest* aRequest,
                              imgINotificationObserver* aObserver) {
  if (!aRequest) {
    return;
  }

  uint32_t status = 0;
  nsresult rv = aRequest->GetImageStatus(&status);
  if (NS_FAILED(rv)) {
    return;
  }

  if (status & imgIRequest::STATUS_SIZE_AVAILABLE) {
    aObserver->Notify(aRequest, imgINotificationObserver::SIZE_AVAILABLE,
                      nullptr);
  }
  if (status & imgIRequest::STATUS_FRAME_COMPLETE) {
    aObserver->Notify(aRequest, imgINotificationObserver::FRAME_COMPLETE,
                      nullptr);
  }
  if (status & imgIRequest::STATUS_HAS_TRANSPARENCY) {
    aObserver->Notify(aRequest, imgINotificationObserver::HAS_TRANSPARENCY,
                      nullptr);
  }
  if (status & imgIRequest::STATUS_IS_ANIMATED) {
    aObserver->Notify(aRequest, imgINotificationObserver::IS_ANIMATED, nullptr);
  }
  if (status & imgIRequest::STATUS_DECODE_COMPLETE) {
    aObserver->Notify(aRequest, imgINotificationObserver::DECODE_COMPLETE,
                      nullptr);
  }
  if (status & imgIRequest::STATUS_LOAD_COMPLETE) {
    aObserver->Notify(aRequest, imgINotificationObserver::LOAD_COMPLETE,
                      nullptr);
  }
}

void nsImageLoadingContent::AddNativeObserver(
    imgINotificationObserver* aObserver) {
  if (NS_WARN_IF(!aObserver)) {
    return;
  }

  if (!mObserverList.mObserver) {
    // Don't touch the linking of the list!
    mObserverList.mObserver = aObserver;

    ReplayImageStatus(mCurrentRequest, aObserver);
    ReplayImageStatus(mPendingRequest, aObserver);

    return;
  }

  // otherwise we have to create a new entry

  ImageObserver* observer = &mObserverList;
  while (observer->mNext) {
    observer = observer->mNext;
  }

  observer->mNext = new ImageObserver(aObserver);
  ReplayImageStatus(mCurrentRequest, aObserver);
  ReplayImageStatus(mPendingRequest, aObserver);
}

void nsImageLoadingContent::RemoveNativeObserver(
    imgINotificationObserver* aObserver) {
  if (NS_WARN_IF(!aObserver)) {
    return;
  }

  if (mObserverList.mObserver == aObserver) {
    mObserverList.mObserver = nullptr;
    // Don't touch the linking of the list!
    return;
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
}

void nsImageLoadingContent::AddObserver(imgINotificationObserver* aObserver) {
  if (NS_WARN_IF(!aObserver)) {
    return;
  }

  RefPtr<imgRequestProxy> currentReq;
  if (mCurrentRequest) {
    // Scripted observers may not belong to the same document as us, so when we
    // create the imgRequestProxy, we shouldn't use any. This allows the request
    // to dispatch notifications from the correct scheduler group.
    nsresult rv =
        mCurrentRequest->Clone(aObserver, nullptr, getter_AddRefs(currentReq));
    if (NS_FAILED(rv)) {
      return;
    }
  }

  RefPtr<imgRequestProxy> pendingReq;
  if (mPendingRequest) {
    // See above for why we don't use the loading document.
    nsresult rv =
        mPendingRequest->Clone(aObserver, nullptr, getter_AddRefs(pendingReq));
    if (NS_FAILED(rv)) {
      mCurrentRequest->CancelAndForgetObserver(NS_BINDING_ABORTED);
      return;
    }
  }

  mScriptedObservers.AppendElement(new ScriptedImageObserver(
      aObserver, std::move(currentReq), std::move(pendingReq)));
}

void nsImageLoadingContent::RemoveObserver(
    imgINotificationObserver* aObserver) {
  if (NS_WARN_IF(!aObserver)) {
    return;
  }

  if (NS_WARN_IF(mScriptedObservers.IsEmpty())) {
    return;
  }

  RefPtr<ScriptedImageObserver> observer;
  auto i = mScriptedObservers.Length();
  do {
    --i;
    if (mScriptedObservers[i]->mObserver == aObserver) {
      observer = std::move(mScriptedObservers[i]);
      mScriptedObservers.RemoveElementAt(i);
      break;
    }
  } while (i > 0);

  if (NS_WARN_IF(!observer)) {
    return;
  }

  // If the cancel causes a mutation, it will be harmless, because we have
  // already removed the observer from the list.
  observer->CancelRequests();
}

void nsImageLoadingContent::ClearScriptedRequests(int32_t aRequestType,
                                                  nsresult aReason) {
  if (MOZ_LIKELY(mScriptedObservers.IsEmpty())) {
    return;
  }

  nsTArray<RefPtr<ScriptedImageObserver>> observers(mScriptedObservers.Clone());
  auto i = observers.Length();
  do {
    --i;

    RefPtr<imgRequestProxy> req;
    switch (aRequestType) {
      case CURRENT_REQUEST:
        req = std::move(observers[i]->mCurrentRequest);
        break;
      case PENDING_REQUEST:
        req = std::move(observers[i]->mPendingRequest);
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

void nsImageLoadingContent::CloneScriptedRequests(imgRequestProxy* aRequest) {
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

  nsTArray<RefPtr<ScriptedImageObserver>> observers(mScriptedObservers.Clone());
  auto i = observers.Length();
  do {
    --i;

    ScriptedImageObserver* observer = observers[i];
    RefPtr<imgRequestProxy>& req =
        current ? observer->mCurrentRequest : observer->mPendingRequest;
    if (NS_WARN_IF(req)) {
      MOZ_ASSERT_UNREACHABLE("Should have cancelled original request");
      req->CancelAndForgetObserver(NS_BINDING_ABORTED);
      req = nullptr;
    }

    nsresult rv =
        aRequest->Clone(observer->mObserver, nullptr, getter_AddRefs(req));
    Unused << NS_WARN_IF(NS_FAILED(rv));
  } while (i > 0);
}

void nsImageLoadingContent::MakePendingScriptedRequestsCurrent() {
  if (MOZ_LIKELY(mScriptedObservers.IsEmpty())) {
    return;
  }

  nsTArray<RefPtr<ScriptedImageObserver>> observers(mScriptedObservers.Clone());
  auto i = observers.Length();
  do {
    --i;

    ScriptedImageObserver* observer = observers[i];
    if (observer->mCurrentRequest) {
      observer->mCurrentRequest->CancelAndForgetObserver(NS_BINDING_ABORTED);
    }
    observer->mCurrentRequest = std::move(observer->mPendingRequest);
  } while (i > 0);
}

already_AddRefed<imgIRequest> nsImageLoadingContent::GetRequest(
    int32_t aRequestType, ErrorResult& aError) {
  nsCOMPtr<imgIRequest> request;
  switch (aRequestType) {
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
                                  imgIRequest** aRequest) {
  NS_ENSURE_ARG_POINTER(aRequest);

  ErrorResult result;
  *aRequest = GetRequest(aRequestType, result).take();

  return result.StealNSResult();
}

NS_IMETHODIMP_(void)
nsImageLoadingContent::FrameCreated(nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame, "aFrame is null");
  MOZ_ASSERT(IsOurImageFrame(aFrame));

  MaybeForceSyncDecoding(/* aPrepareNextRequest */ false, aFrame);
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
nsImageLoadingContent::FrameDestroyed(nsIFrame* aFrame) {
  NS_ASSERTION(aFrame, "aFrame is null");

  // We need to make sure that our image request is deregistered.
  nsPresContext* presContext = GetFramePresContext();
  if (mCurrentRequest) {
    nsLayoutUtils::DeregisterImageRequest(presContext, mCurrentRequest,
                                          &mCurrentRequestRegistered);
  }

  if (mPendingRequest) {
    nsLayoutUtils::DeregisterImageRequest(presContext, mPendingRequest,
                                          &mPendingRequestRegistered);
  }

  UntrackImage(mCurrentRequest);
  UntrackImage(mPendingRequest);

  PresShell* presShell = presContext ? presContext->GetPresShell() : nullptr;
  if (presShell) {
    presShell->RemoveFrameFromApproximatelyVisibleList(aFrame);
  }
}

/* static */
nsContentPolicyType nsImageLoadingContent::PolicyTypeForLoad(
    ImageLoadType aImageLoadType) {
  if (aImageLoadType == eImageLoadType_Imageset) {
    return nsIContentPolicy::TYPE_IMAGESET;
  }

  MOZ_ASSERT(aImageLoadType == eImageLoadType_Normal,
             "Unknown ImageLoadType type in PolicyTypeForLoad");
  return nsIContentPolicy::TYPE_INTERNAL_IMAGE;
}

int32_t nsImageLoadingContent::GetRequestType(imgIRequest* aRequest,
                                              ErrorResult& aError) {
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
                                      int32_t* aRequestType) {
  MOZ_ASSERT(aRequestType, "Null out param");

  ErrorResult result;
  *aRequestType = GetRequestType(aRequest, result);
  return result.StealNSResult();
}

already_AddRefed<nsIURI> nsImageLoadingContent::GetCurrentURI() {
  nsCOMPtr<nsIURI> uri;
  if (mCurrentRequest) {
    mCurrentRequest->GetURI(getter_AddRefs(uri));
  } else {
    uri = mCurrentURI;
  }

  return uri.forget();
}

NS_IMETHODIMP
nsImageLoadingContent::GetCurrentURI(nsIURI** aURI) {
  NS_ENSURE_ARG_POINTER(aURI);
  *aURI = GetCurrentURI().take();
  return NS_OK;
}

already_AddRefed<nsIURI> nsImageLoadingContent::GetCurrentRequestFinalURI() {
  nsCOMPtr<nsIURI> uri;
  if (mCurrentRequest) {
    mCurrentRequest->GetFinalURI(getter_AddRefs(uri));
  }
  return uri.forget();
}

NS_IMETHODIMP
nsImageLoadingContent::LoadImageWithChannel(nsIChannel* aChannel,
                                            nsIStreamListener** aListener) {
  imgLoader* loader =
      nsContentUtils::GetImgLoaderForChannel(aChannel, GetOurOwnerDoc());
  if (!loader) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<Document> doc = GetOurOwnerDoc();
  if (!doc) {
    // Don't bother
    *aListener = nullptr;
    return NS_OK;
  }

  // XXX what should we do with content policies here, if anything?
  // Shouldn't that be done before the start of the load?
  // XXX what about shouldProcess?

  // If we have a current request without a size, we know we will replace it
  // with the PrepareNextRequest below. If the new current request is for a
  // different URI, then we need to reject any outstanding promises.
  if (mCurrentRequest && !HaveSize(mCurrentRequest)) {
    nsCOMPtr<nsIURI> uri;
    aChannel->GetOriginalURI(getter_AddRefs(uri));
    MaybeAgeRequestGeneration(uri);
  }

  // Our state might change. Watch it.
  AutoStateChanger changer(this, true);

  // Do the load.
  RefPtr<imgRequestProxy>& req = PrepareNextRequest(eImageLoadType_Normal);
  nsresult rv = loader->LoadImageWithChannel(aChannel, this, doc, aListener,
                                             getter_AddRefs(req));
  if (NS_SUCCEEDED(rv)) {
    CloneScriptedRequests(req);
    TrackImage(req);
    ResetAnimationIfNeeded();
    return NS_OK;
  }

  MOZ_ASSERT(!req, "Shouldn't have non-null request here");
  // If we don't have a current URI, we might as well store this URI so people
  // know what we tried (and failed) to load.
  if (!mCurrentRequest) aChannel->GetURI(getter_AddRefs(mCurrentURI));

  FireEvent(u"error"_ns);
  return rv;
}

void nsImageLoadingContent::ForceReload(bool aNotify, ErrorResult& aError) {
  nsCOMPtr<nsIURI> currentURI;
  GetCurrentURI(getter_AddRefs(currentURI));
  if (!currentURI) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  // We keep this flag around along with the old URI even for failed requests
  // without a live request object
  ImageLoadType loadType = (mCurrentRequestFlags & REQUEST_IS_IMAGESET)
                               ? eImageLoadType_Imageset
                               : eImageLoadType_Normal;
  nsresult rv = LoadImage(currentURI, true, aNotify, loadType,
                          nsIRequest::VALIDATE_ALWAYS | LoadFlags());
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
  }
}

/*
 * Non-interface methods
 */

nsresult nsImageLoadingContent::LoadImage(const nsAString& aNewURI, bool aForce,
                                          bool aNotify,
                                          ImageLoadType aImageLoadType,
                                          nsIPrincipal* aTriggeringPrincipal) {
  // First, get a document (needed for security checks and the like)
  Document* doc = GetOurOwnerDoc();
  if (!doc) {
    // No reason to bother, I think...
    return NS_OK;
  }

  // Parse the URI string to get image URI
  nsCOMPtr<nsIURI> imageURI;
  if (!aNewURI.IsEmpty()) {
    Unused << StringToURI(aNewURI, doc, getter_AddRefs(imageURI));
  }

  return LoadImage(imageURI, aForce, aNotify, aImageLoadType, LoadFlags(), doc,
                   aTriggeringPrincipal);
}

nsresult nsImageLoadingContent::LoadImage(nsIURI* aNewURI, bool aForce,
                                          bool aNotify,
                                          ImageLoadType aImageLoadType,
                                          nsLoadFlags aLoadFlags,
                                          Document* aDocument,
                                          nsIPrincipal* aTriggeringPrincipal) {
  MOZ_ASSERT(!mIsStartingImageLoad, "some evil code is reentering LoadImage.");
  if (mIsStartingImageLoad) {
    return NS_OK;
  }

  // Pending load/error events need to be canceled in some situations. This
  // is not documented in the spec, but can cause site compat problems if not
  // done. See bug 1309461 and https://github.com/whatwg/html/issues/1872.
  CancelPendingEvent();

  if (!aNewURI) {
    // Cancel image requests and then fire only error event per spec.
    CancelImageRequests(aNotify);
    if (aImageLoadType == eImageLoadType_Normal) {
      // Mark error event as cancelable only for src="" case, since only this
      // error causes site compat problem (bug 1308069) for now.
      FireEvent(u"error"_ns, true);
    }
    return NS_OK;
  }

  if (!mLoadingEnabled) {
    // XXX Why fire an error here? seems like the callers to SetLoadingEnabled
    // don't want/need it.
    FireEvent(u"error"_ns);
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

  // Data documents, or documents from DOMParser shouldn't perform image
  // loading.
  //
  // FIXME(emilio): Shouldn't this check be part of
  // Document::ShouldLoadImages()? Or alternatively check ShouldLoadImages here
  // instead? (It seems we only check ShouldLoadImages in HTMLImageElement,
  // which seems wrong...)
  if (aDocument->IsLoadedAsData() && !aDocument->IsStaticDocument()) {
    // Clear our pending request if we do have one.
    ClearPendingRequest(NS_BINDING_ABORTED, Some(OnNonvisible::DiscardImages));

    FireEvent(u"error"_ns);
    return NS_OK;
  }

  // URI equality check.
  //
  // We skip the equality check if we don't have a current image, since in that
  // case we really do want to try loading again.
  if (!aForce && mCurrentRequest) {
    nsCOMPtr<nsIURI> currentURI;
    GetCurrentURI(getter_AddRefs(currentURI));
    bool equal;
    if (currentURI && NS_SUCCEEDED(currentURI->Equals(aNewURI, &equal)) &&
        equal) {
      // Nothing to do here.
      return NS_OK;
    }
  }

  // If we have a current request without a size, we know we will replace it
  // with the PrepareNextRequest below. If the new current request is for a
  // different URI, then we need to reject any outstanding promises.
  if (mCurrentRequest && !HaveSize(mCurrentRequest)) {
    MaybeAgeRequestGeneration(aNewURI);
  }

  // From this point on, our image state could change. Watch it.
  AutoStateChanger changer(this, aNotify);

  // Sanity check.
  //
  // We use the principal of aDocument to avoid having to QI |this| an extra
  // time. It should always be the same as the principal of this node.
  Element* element = AsContent()->AsElement();
  MOZ_ASSERT(element->NodePrincipal() == aDocument->NodePrincipal(),
             "Principal mismatch?");

  nsLoadFlags loadFlags =
      aLoadFlags | nsContentUtils::CORSModeToLoadImageFlags(GetCORSMode());

  RefPtr<imgRequestProxy>& req = PrepareNextRequest(aImageLoadType);
  nsCOMPtr<nsIPrincipal> triggeringPrincipal;
  bool result = nsContentUtils::QueryTriggeringPrincipal(
      element, aTriggeringPrincipal, getter_AddRefs(triggeringPrincipal));

  // If result is true, which means this node has specified
  // 'triggeringprincipal' attribute on it, so we use favicon as the policy
  // type.
  nsContentPolicyType policyType =
      result ? nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON
             : PolicyTypeForLoad(aImageLoadType);

  auto referrerInfo = MakeRefPtr<ReferrerInfo>(*element);
  nsresult rv = nsContentUtils::LoadImage(
      aNewURI, element, aDocument, triggeringPrincipal, 0, referrerInfo, this,
      loadFlags, element->LocalName(), getter_AddRefs(req), policyType,
      mUseUrgentStartForChannel);

  // Reset the flag to avoid loading from XPCOM or somewhere again else without
  // initiated by user interaction.
  mUseUrgentStartForChannel = false;

  // Tell the document to forget about the image preload, if any, for
  // this URI, now that we might have another imgRequestProxy for it.
  // That way if we get canceled later the image load won't continue.
  aDocument->ForgetImagePreload(aNewURI);

  if (NS_SUCCEEDED(rv)) {
    // Based on performance testing unsuppressing painting soon after the page
    // has gotten an image may improve visual metrics.
    if (Document* doc = element->GetComposedDoc()) {
      if (PresShell* shell = doc->GetPresShell()) {
        shell->TryUnsuppressPaintingSoon();
      }
    }

    CloneScriptedRequests(req);
    TrackImage(req);
    ResetAnimationIfNeeded();

    // Handle cases when we just ended up with a request but it's already done.
    // In that situation we have to synchronously switch that request to being
    // the current request, because websites depend on that behavior.
    {
      uint32_t loadStatus;
      if (NS_SUCCEEDED(req->GetImageStatus(&loadStatus)) &&
          (loadStatus & imgIRequest::STATUS_LOAD_COMPLETE)) {
        if (req == mPendingRequest) {
          MakePendingRequestCurrent();
        }
        MOZ_ASSERT(mCurrentRequest,
                   "How could we not have a current request here?");

        if (nsImageFrame* f = do_QueryFrame(GetOurPrimaryImageFrame())) {
          f->NotifyNewCurrentRequest(mCurrentRequest, NS_OK);
        }
      }
    }
  } else {
    MOZ_ASSERT(!req, "Shouldn't have non-null request here");
    // If we don't have a current URI, we might as well store this URI so people
    // know what we tried (and failed) to load.
    if (!mCurrentRequest) {
      mCurrentURI = aNewURI;
    }

    FireEvent(u"error"_ns);
  }

  return NS_OK;
}

already_AddRefed<Promise> nsImageLoadingContent::RecognizeCurrentImageText(
    ErrorResult& aRv) {
  using widget::TextRecognition;

  if (!mCurrentRequest) {
    aRv.ThrowInvalidStateError("No current request");
    return nullptr;
  }
  nsCOMPtr<imgIContainer> image;
  mCurrentRequest->GetImage(getter_AddRefs(image));
  if (!image) {
    aRv.ThrowInvalidStateError("No image");
    return nullptr;
  }

  RefPtr<Promise> domPromise =
      Promise::Create(GetOurOwnerDoc()->GetScopeObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // The list of ISO 639-1 language tags to pass to the text recognition API.
  AutoTArray<nsCString, 4> languages;
  {
    // The document's locale should be the top language to use. Parse the BCP 47
    // locale and extract the ISO 639-1 language tag. e.g. "en-US" -> "en".
    nsAutoCString elementLanguage;
    nsAtom* imgLanguage = AsContent()->GetLang();
    intl::Locale locale;
    if (imgLanguage) {
      imgLanguage->ToUTF8String(elementLanguage);
      auto result = intl::LocaleParser::TryParse(elementLanguage, locale);
      if (result.isOk()) {
        languages.AppendElement(locale.Language().Span());
      }
    }
  }

  {
    // The app locales should also be included after the document's locales.
    // Extract the language tag like above.
    nsTArray<nsCString> appLocales;
    intl::LocaleService::GetInstance()->GetAppLocalesAsBCP47(appLocales);

    for (const auto& localeString : appLocales) {
      intl::Locale locale;
      auto result = intl::LocaleParser::TryParse(localeString, locale);
      if (result.isErr()) {
        NS_WARNING("Could not parse an app locale string, ignoring it.");
        continue;
      }
      languages.AppendElement(locale.Language().Span());
    }
  }

  TextRecognition::FindText(*image, languages)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [weak = RefPtr{do_GetWeakReference(this)},
           request = RefPtr{mCurrentRequest}, domPromise](
              TextRecognition::NativePromise::ResolveOrRejectValue&& aValue) {
            if (aValue.IsReject()) {
              domPromise->MaybeRejectWithNotSupportedError(
                  aValue.RejectValue());
              return;
            }
            RefPtr<nsIImageLoadingContent> iilc = do_QueryReferent(weak.get());
            if (!iilc) {
              domPromise->MaybeRejectWithInvalidStateError(
                  "Element was dead when we got the results");
              return;
            }
            auto* ilc = static_cast<nsImageLoadingContent*>(iilc.get());
            if (ilc->mCurrentRequest != request) {
              domPromise->MaybeRejectWithInvalidStateError(
                  "Request not current");
              return;
            }
            auto& textRecognitionResult = aValue.ResolveValue();
            Element* el = ilc->AsContent()->AsElement();

            // When enabled, this feature will place the recognized text as
            // spans inside of the shadow dom of the img element. These are then
            // positioned so that the user can select the text.
            if (Preferences::GetBool("dom.text-recognition.shadow-dom-enabled",
                                     false)) {
              el->AttachAndSetUAShadowRoot(Element::NotifyUAWidgetSetup::Yes);
              TextRecognition::FillShadow(*el->GetShadowRoot(),
                                          textRecognitionResult);
              el->NotifyUAWidgetSetupOrChange();
            }

            nsTArray<ImageText> imageTexts(
                textRecognitionResult.quads().Length());
            nsIGlobalObject* global = el->OwnerDoc()->GetOwnerGlobal();

            for (const auto& quad : textRecognitionResult.quads()) {
              NotNull<ImageText*> imageText = imageTexts.AppendElement();

              // Note: These points are not actually CSSPixels, but a DOMQuad is
              // a conveniently similar structure that can store these values.
              CSSPoint points[4];
              points[0] = CSSPoint(quad.points()[0].x, quad.points()[0].y);
              points[1] = CSSPoint(quad.points()[1].x, quad.points()[1].y);
              points[2] = CSSPoint(quad.points()[2].x, quad.points()[2].y);
              points[3] = CSSPoint(quad.points()[3].x, quad.points()[3].y);

              imageText->mQuad = new DOMQuad(global, points);
              imageText->mConfidence = quad.confidence();
              imageText->mString = quad.string();
            }
            domPromise->MaybeResolve(std::move(imageTexts));
          });
  return domPromise.forget();
}

CSSIntSize nsImageLoadingContent::GetWidthHeightForImage() {
  Element* element = AsContent()->AsElement();
  if (nsIFrame* frame = element->GetPrimaryFrame(FlushType::Layout)) {
    return CSSIntSize::FromAppUnitsRounded(frame->GetContentRect().Size());
  }
  const nsAttrValue* value;
  nsCOMPtr<imgIContainer> image;
  if (mCurrentRequest) {
    mCurrentRequest->GetImage(getter_AddRefs(image));
  }

  CSSIntSize size;
  if ((value = element->GetParsedAttr(nsGkAtoms::width)) &&
      value->Type() == nsAttrValue::eInteger) {
    size.width = value->GetIntegerValue();
  } else if (image) {
    image->GetWidth(&size.width);
  }

  if ((value = element->GetParsedAttr(nsGkAtoms::height)) &&
      value->Type() == nsAttrValue::eInteger) {
    size.height = value->GetIntegerValue();
  } else if (image) {
    image->GetHeight(&size.height);
  }

  NS_ASSERTION(size.width >= 0, "negative width");
  NS_ASSERTION(size.height >= 0, "negative height");
  return size;
}

void nsImageLoadingContent::UpdateImageState(bool aNotify) {
  if (mStateChangerDepth > 0) {
    // Ignore this call; we'll update our state when the outermost state changer
    // is destroyed. Need this to work around the fact that some ImageLib
    // stuff is actually sync and hence we can get OnStopDecode called while
    // we're still under LoadImage, and OnStopDecode doesn't know anything about
    // aNotify.
    // XXX - This machinery should be removed after bug 521604.
    return;
  }

  Element* thisElement = AsContent()->AsElement();

  mLoading = false;

  Element::AutoStateChangeNotifier notifier(*thisElement, aNotify);
  thisElement->RemoveStatesSilently(ElementState::BROKEN);

  // If we were blocked, we're broken, so are we if we don't have an image
  // request at all or the image has errored.
  if (!mCurrentRequest) {
    if (!mLazyLoading) {
      // In case of non-lazy loading, no current request means error, since we
      // weren't disabled or suppressed
      thisElement->AddStatesSilently(ElementState::BROKEN);
      RejectDecodePromises(NS_ERROR_DOM_IMAGE_BROKEN);
    }
  } else {
    uint32_t currentLoadStatus;
    nsresult rv = mCurrentRequest->GetImageStatus(&currentLoadStatus);
    if (NS_FAILED(rv) || (currentLoadStatus & imgIRequest::STATUS_ERROR)) {
      thisElement->AddStatesSilently(ElementState::BROKEN);
      RejectDecodePromises(NS_ERROR_DOM_IMAGE_BROKEN);
    } else if (!(currentLoadStatus & imgIRequest::STATUS_SIZE_AVAILABLE)) {
      mLoading = true;
    }
  }
}

void nsImageLoadingContent::CancelImageRequests(bool aNotify) {
  RejectDecodePromises(NS_ERROR_DOM_IMAGE_INVALID_REQUEST);
  AutoStateChanger changer(this, aNotify);
  ClearPendingRequest(NS_BINDING_ABORTED, Some(OnNonvisible::DiscardImages));
  ClearCurrentRequest(NS_BINDING_ABORTED, Some(OnNonvisible::DiscardImages));
}

Document* nsImageLoadingContent::GetOurOwnerDoc() {
  return AsContent()->OwnerDoc();
}

Document* nsImageLoadingContent::GetOurCurrentDoc() {
  return AsContent()->GetComposedDoc();
}

nsPresContext* nsImageLoadingContent::GetFramePresContext() {
  nsIFrame* frame = GetOurPrimaryImageFrame();
  if (!frame) {
    return nullptr;
  }
  return frame->PresContext();
}

nsresult nsImageLoadingContent::StringToURI(const nsAString& aSpec,
                                            Document* aDocument,
                                            nsIURI** aURI) {
  MOZ_ASSERT(aDocument, "Must have a document");
  MOZ_ASSERT(aURI, "Null out param");

  // (1) Get the base URI
  nsIContent* thisContent = AsContent();
  nsIURI* baseURL = thisContent->GetBaseURI();

  // (2) Get the charset
  auto encoding = aDocument->GetDocumentCharacterSet();

  // (3) Construct the silly thing
  return NS_NewURI(aURI, aSpec, encoding, baseURL);
}

nsresult nsImageLoadingContent::FireEvent(const nsAString& aEventType,
                                          bool aIsCancelable) {
  if (nsContentUtils::DocumentInactiveForImageLoads(GetOurOwnerDoc())) {
    // Don't bother to fire any events, especially error events.
    RejectDecodePromises(NS_ERROR_DOM_IMAGE_INACTIVE_DOCUMENT);
    return NS_OK;
  }

  // We have to fire the event asynchronously so that we won't go into infinite
  // loops in cases when onLoad handlers reset the src and the new src is in
  // cache.

  nsCOMPtr<nsINode> thisNode = AsContent();

  RefPtr<AsyncEventDispatcher> loadBlockingAsyncDispatcher =
      new LoadBlockingAsyncEventDispatcher(thisNode, aEventType, CanBubble::eNo,
                                           ChromeOnlyDispatch::eNo);
  loadBlockingAsyncDispatcher->PostDOMEvent();

  if (aIsCancelable) {
    mPendingEvent = loadBlockingAsyncDispatcher;
  }

  return NS_OK;
}

void nsImageLoadingContent::AsyncEventRunning(AsyncEventDispatcher* aEvent) {
  if (mPendingEvent == aEvent) {
    mPendingEvent = nullptr;
  }
}

void nsImageLoadingContent::CancelPendingEvent() {
  if (mPendingEvent) {
    mPendingEvent->Cancel();
    mPendingEvent = nullptr;
  }
}

RefPtr<imgRequestProxy>& nsImageLoadingContent::PrepareNextRequest(
    ImageLoadType aImageLoadType) {
  MaybeForceSyncDecoding(/* aPrepareNextRequest */ true);

  // We only want to cancel the existing current request if size is not
  // available. bz says the web depends on this behavior.
  // Otherwise, we get rid of any half-baked request that might be sitting there
  // and make this one current.
  return HaveSize(mCurrentRequest) ? PreparePendingRequest(aImageLoadType)
                                   : PrepareCurrentRequest(aImageLoadType);
}

RefPtr<imgRequestProxy>& nsImageLoadingContent::PrepareCurrentRequest(
    ImageLoadType aImageLoadType) {
  // Get rid of anything that was there previously.
  ClearCurrentRequest(NS_BINDING_ABORTED, Some(OnNonvisible::DiscardImages));

  if (mNewRequestsWillNeedAnimationReset) {
    mCurrentRequestFlags |= REQUEST_NEEDS_ANIMATION_RESET;
  }

  if (aImageLoadType == eImageLoadType_Imageset) {
    mCurrentRequestFlags |= REQUEST_IS_IMAGESET;
  }

  // Return a reference.
  return mCurrentRequest;
}

RefPtr<imgRequestProxy>& nsImageLoadingContent::PreparePendingRequest(
    ImageLoadType aImageLoadType) {
  // Get rid of anything that was there previously.
  ClearPendingRequest(NS_BINDING_ABORTED, Some(OnNonvisible::DiscardImages));

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

class ImageRequestAutoLock {
 public:
  explicit ImageRequestAutoLock(imgIRequest* aRequest) : mRequest(aRequest) {
    if (mRequest) {
      mRequest->LockImage();
    }
  }

  ~ImageRequestAutoLock() {
    if (mRequest) {
      mRequest->UnlockImage();
    }
  }

 private:
  nsCOMPtr<imgIRequest> mRequest;
};

}  // namespace

void nsImageLoadingContent::MakePendingRequestCurrent() {
  MOZ_ASSERT(mPendingRequest);

  // If we have a pending request, we know that there is an existing current
  // request with size information. If the pending request is for a different
  // URI, then we need to reject any outstanding promises.
  nsCOMPtr<nsIURI> uri;
  mPendingRequest->GetURI(getter_AddRefs(uri));
  MaybeAgeRequestGeneration(uri);

  // Lock mCurrentRequest for the duration of this method.  We do this because
  // PrepareCurrentRequest() might unlock mCurrentRequest.  If mCurrentRequest
  // and mPendingRequest are both requests for the same image, unlocking
  // mCurrentRequest before we lock mPendingRequest can cause the lock count
  // to go to 0 and the image to be discarded!
  ImageRequestAutoLock autoLock(mCurrentRequest);

  ImageLoadType loadType = (mPendingRequestFlags & REQUEST_IS_IMAGESET)
                               ? eImageLoadType_Imageset
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

void nsImageLoadingContent::ClearCurrentRequest(
    nsresult aReason, const Maybe<OnNonvisible>& aNonvisibleAction) {
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

void nsImageLoadingContent::ClearPendingRequest(
    nsresult aReason, const Maybe<OnNonvisible>& aNonvisibleAction) {
  if (!mPendingRequest) return;

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

void nsImageLoadingContent::ResetAnimationIfNeeded() {
  if (mCurrentRequest &&
      (mCurrentRequestFlags & REQUEST_NEEDS_ANIMATION_RESET)) {
    nsCOMPtr<imgIContainer> container;
    mCurrentRequest->GetImage(getter_AddRefs(container));
    if (container) container->ResetAnimation();
    mCurrentRequestFlags &= ~REQUEST_NEEDS_ANIMATION_RESET;
  }
}

bool nsImageLoadingContent::HaveSize(imgIRequest* aImage) {
  // Handle the null case
  if (!aImage) return false;

  // Query the image
  uint32_t status;
  nsresult rv = aImage->GetImageStatus(&status);
  return (NS_SUCCEEDED(rv) && (status & imgIRequest::STATUS_SIZE_AVAILABLE));
}

void nsImageLoadingContent::NotifyOwnerDocumentActivityChanged() {
  if (!GetOurOwnerDoc()->IsCurrentActiveDocument()) {
    RejectDecodePromises(NS_ERROR_DOM_IMAGE_INACTIVE_DOCUMENT);
  }
}

void nsImageLoadingContent::BindToTree(BindContext& aContext,
                                       nsINode& aParent) {
  // We may be getting connected, if so our image should be tracked,
  if (aContext.InComposedDoc()) {
    TrackImage(mCurrentRequest);
    TrackImage(mPendingRequest);
  }
}

void nsImageLoadingContent::UnbindFromTree(bool aNullParent) {
  // We may be leaving the document, so if our image is tracked, untrack it.
  nsCOMPtr<Document> doc = GetOurCurrentDoc();
  if (!doc) return;

  UntrackImage(mCurrentRequest);
  UntrackImage(mPendingRequest);
}

void nsImageLoadingContent::OnVisibilityChange(
    Visibility aNewVisibility, const Maybe<OnNonvisible>& aNonvisibleAction) {
  switch (aNewVisibility) {
    case Visibility::ApproximatelyVisible:
      TrackImage(mCurrentRequest);
      TrackImage(mPendingRequest);
      break;

    case Visibility::ApproximatelyNonVisible:
      UntrackImage(mCurrentRequest, aNonvisibleAction);
      UntrackImage(mPendingRequest, aNonvisibleAction);
      break;

    case Visibility::Untracked:
      MOZ_ASSERT_UNREACHABLE("Shouldn't notify for untracked visibility");
      break;
  }
}

void nsImageLoadingContent::TrackImage(imgIRequest* aImage,
                                       nsIFrame* aFrame /*= nullptr */) {
  if (!aImage) return;

  MOZ_ASSERT(aImage == mCurrentRequest || aImage == mPendingRequest,
             "Why haven't we heard of this request?");

  Document* doc = GetOurCurrentDoc();
  if (!doc) {
    return;
  }

  if (!aFrame) {
    aFrame = GetOurPrimaryImageFrame();
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
  if (!aFrame ||
      aFrame->GetVisibility() == Visibility::ApproximatelyNonVisible) {
    return;
  }

  if (aImage == mCurrentRequest &&
      !(mCurrentRequestFlags & REQUEST_IS_TRACKED)) {
    mCurrentRequestFlags |= REQUEST_IS_TRACKED;
    doc->ImageTracker()->Add(mCurrentRequest);
  }
  if (aImage == mPendingRequest &&
      !(mPendingRequestFlags & REQUEST_IS_TRACKED)) {
    mPendingRequestFlags |= REQUEST_IS_TRACKED;
    doc->ImageTracker()->Add(mPendingRequest);
  }
}

void nsImageLoadingContent::UntrackImage(
    imgIRequest* aImage, const Maybe<OnNonvisible>& aNonvisibleAction
    /* = Nothing() */) {
  if (!aImage) return;

  MOZ_ASSERT(aImage == mCurrentRequest || aImage == mPendingRequest,
             "Why haven't we heard of this request?");

  // We may not be in the document.  If we outlived our document that's fine,
  // because the document empties out the tracker and unlocks all locked images
  // on destruction.  But if we were never in the document we may need to force
  // discarding the image here, since this is the only chance we have.
  Document* doc = GetOurCurrentDoc();
  if (aImage == mCurrentRequest) {
    if (doc && (mCurrentRequestFlags & REQUEST_IS_TRACKED)) {
      mCurrentRequestFlags &= ~REQUEST_IS_TRACKED;
      doc->ImageTracker()->Remove(
          mCurrentRequest,
          aNonvisibleAction == Some(OnNonvisible::DiscardImages)
              ? ImageTracker::REQUEST_DISCARD
              : 0);
    } else if (aNonvisibleAction == Some(OnNonvisible::DiscardImages)) {
      // If we're not in the document we may still need to be discarded.
      aImage->RequestDiscard();
    }
  }
  if (aImage == mPendingRequest) {
    if (doc && (mPendingRequestFlags & REQUEST_IS_TRACKED)) {
      mPendingRequestFlags &= ~REQUEST_IS_TRACKED;
      doc->ImageTracker()->Remove(
          mPendingRequest,
          aNonvisibleAction == Some(OnNonvisible::DiscardImages)
              ? ImageTracker::REQUEST_DISCARD
              : 0);
    } else if (aNonvisibleAction == Some(OnNonvisible::DiscardImages)) {
      // If we're not in the document we may still need to be discarded.
      aImage->RequestDiscard();
    }
  }
}

CORSMode nsImageLoadingContent::GetCORSMode() { return CORS_NONE; }

nsImageLoadingContent::ImageObserver::ImageObserver(
    imgINotificationObserver* aObserver)
    : mObserver(aObserver), mNext(nullptr) {
  MOZ_COUNT_CTOR(ImageObserver);
}

nsImageLoadingContent::ImageObserver::~ImageObserver() {
  MOZ_COUNT_DTOR(ImageObserver);
  NS_CONTENT_DELETE_LIST_MEMBER(ImageObserver, this, mNext);
}

nsImageLoadingContent::ScriptedImageObserver::ScriptedImageObserver(
    imgINotificationObserver* aObserver,
    RefPtr<imgRequestProxy>&& aCurrentRequest,
    RefPtr<imgRequestProxy>&& aPendingRequest)
    : mObserver(aObserver),
      mCurrentRequest(aCurrentRequest),
      mPendingRequest(aPendingRequest) {}

nsImageLoadingContent::ScriptedImageObserver::~ScriptedImageObserver() {
  // We should have cancelled any requests before getting released.
  DebugOnly<bool> cancel = CancelRequests();
  MOZ_ASSERT(!cancel, "Still have requests in ~ScriptedImageObserver!");
}

bool nsImageLoadingContent::ScriptedImageObserver::CancelRequests() {
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

Element* nsImageLoadingContent::FindImageMap() {
  return FindImageMap(AsContent()->AsElement());
}

/* static */ Element* nsImageLoadingContent::FindImageMap(Element* aElement) {
  nsAutoString useMap;
  aElement->GetAttr(nsGkAtoms::usemap, useMap);
  if (useMap.IsEmpty()) {
    return nullptr;
  }

  nsAString::const_iterator start, end;
  useMap.BeginReading(start);
  useMap.EndReading(end);

  int32_t hash = useMap.FindChar('#');
  if (hash < 0) {
    return nullptr;
  }
  // useMap contains a '#', set start to point right after the '#'
  start.advance(hash + 1);

  if (start == end) {
    return nullptr;  // useMap == "#"
  }

  RefPtr<nsContentList> imageMapList;
  if (aElement->IsInUncomposedDoc()) {
    // Optimize the common case and use document level image map.
    imageMapList = aElement->OwnerDoc()->ImageMapList();
  } else {
    // Per HTML spec image map should be searched in the element's scope,
    // so using SubtreeRoot() here.
    // Because this is a temporary list, we don't need to make it live.
    imageMapList =
        new nsContentList(aElement->SubtreeRoot(), kNameSpaceID_XHTML,
                          nsGkAtoms::map, nsGkAtoms::map, true, /* deep */
                          false /* live */);
  }

  nsAutoString mapName(Substring(start, end));

  uint32_t i, n = imageMapList->Length(true);
  for (i = 0; i < n; ++i) {
    nsIContent* map = imageMapList->Item(i);
    if (map->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::id, mapName,
                                      eCaseMatters) ||
        map->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                      mapName, eCaseMatters)) {
      return map->AsElement();
    }
  }

  return nullptr;
}

nsLoadFlags nsImageLoadingContent::LoadFlags() {
  auto* image = HTMLImageElement::FromNode(AsContent());
  if (image && image->OwnerDoc()->IsScriptEnabled() &&
      !image->OwnerDoc()->IsStaticDocument() &&
      image->LoadingState() == Element::Loading::Lazy) {
    // Note that LOAD_BACKGROUND is not about priority of the load, but about
    // whether it blocks the load event (by bypassing the loadgroup).
    return nsIRequest::LOAD_BACKGROUND;
  }
  return nsIRequest::LOAD_NORMAL;
}
