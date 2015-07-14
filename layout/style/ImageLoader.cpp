/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A class that handles style system image loads (other image loads are handled
 * by the nodes in the content tree).
 */

#include "mozilla/css/ImageLoader.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsError.h"
#include "nsDisplayList.h"
#include "FrameLayerBuilder.h"
#include "nsSVGEffects.h"
#include "imgIContainer.h"

namespace mozilla {
namespace css {

/* static */ PLDHashOperator
ImageLoader::SetAnimationModeEnumerator(nsISupports* aKey, FrameSet* aValue,
                                        void* aClosure)
{
  imgIRequest* request = static_cast<imgIRequest*>(aKey);

  uint16_t* mode = static_cast<uint16_t*>(aClosure);

#ifdef DEBUG
  {
    nsCOMPtr<imgIRequest> debugRequest = do_QueryInterface(aKey);
    NS_ASSERTION(debugRequest == request, "This is bad");
  }
#endif

  nsCOMPtr<imgIContainer> container;
  request->GetImage(getter_AddRefs(container));
  if (!container) {
    return PL_DHASH_NEXT;
  }

  // This can fail if the image is in error, and we don't care.
  container->SetAnimationMode(*mode);

  return PL_DHASH_NEXT;
}

void
ImageLoader::DropDocumentReference()
{
  // It's okay if GetPresContext returns null here (due to the presshell pointer
  // on the document being null) as that means the presshell has already
  // been destroyed, and it also calls ClearFrames when it is destroyed.
  ClearFrames(GetPresContext());

  for (auto it = mImages.Iter(); !it.Done(); it.Next()) {
    ImageLoader::Image* image = it.Get()->GetKey();
    imgIRequest* request = image->mRequests.GetWeak(mDocument);
    if (request) {
      request->CancelAndForgetObserver(NS_BINDING_ABORTED);
    }
    image->mRequests.Remove(mDocument);
  }
  mImages.Clear();

  mDocument = nullptr;
}

void
ImageLoader::AssociateRequestToFrame(imgIRequest* aRequest,
                                     nsIFrame* aFrame)
{
  nsCOMPtr<imgINotificationObserver> observer;
  aRequest->GetNotificationObserver(getter_AddRefs(observer));
  if (!observer) {
    // The request has already been canceled, so ignore it.  This is ok because
    // we're not going to get any more notifications from a canceled request.
    return;
  }

  MOZ_ASSERT(observer == this);

  FrameSet* frameSet = nullptr;
  if (mRequestToFrameMap.Get(aRequest, &frameSet)) {
    NS_ASSERTION(frameSet, "This should never be null!");
  }

  if (!frameSet) {
    nsAutoPtr<FrameSet> newFrameSet(new FrameSet());

    mRequestToFrameMap.Put(aRequest, newFrameSet);
    frameSet = newFrameSet.forget();

    nsPresContext* presContext = GetPresContext();
    if (presContext) {
      nsLayoutUtils::RegisterImageRequestIfAnimated(presContext,
                                                    aRequest,
                                                    nullptr);
    }
  }

  RequestSet* requestSet = nullptr;
  if (mFrameToRequestMap.Get(aFrame, &requestSet)) {
    NS_ASSERTION(requestSet, "This should never be null");
  }

  if (!requestSet) {
    nsAutoPtr<RequestSet> newRequestSet(new RequestSet());

    mFrameToRequestMap.Put(aFrame, newRequestSet);
    requestSet = newRequestSet.forget();
  }

  // Add these to the sets, but only if they're not already there.
  uint32_t i = frameSet->IndexOfFirstElementGt(aFrame);
  if (i == 0 || aFrame != frameSet->ElementAt(i-1)) {
    frameSet->InsertElementAt(i, aFrame);
  }
  i = requestSet->IndexOfFirstElementGt(aRequest);
  if (i == 0 || aRequest != requestSet->ElementAt(i-1)) {
    requestSet->InsertElementAt(i, aRequest);
  }
}

void
ImageLoader::MaybeRegisterCSSImage(ImageLoader::Image* aImage)
{
  NS_ASSERTION(aImage, "This should never be null!");

  bool found = false;
  aImage->mRequests.GetWeak(mDocument, &found);
  if (found) {
    // This document already has a request.
    return;
  }

  imgRequestProxy* canonicalRequest = aImage->mRequests.GetWeak(nullptr);
  if (!canonicalRequest) {
    // The image was blocked or something.
    return;
  }

  nsRefPtr<imgRequestProxy> request;

  // Ignore errors here.  If cloning fails for some reason we'll put a null
  // entry in the hash and we won't keep trying to clone.
  mInClone = true;
  canonicalRequest->Clone(this, getter_AddRefs(request));
  mInClone = false;

  aImage->mRequests.Put(mDocument, request);

  AddImage(aImage);
}

void
ImageLoader::DeregisterCSSImage(ImageLoader::Image* aImage)
{
  RemoveImage(aImage);
}

void
ImageLoader::DisassociateRequestFromFrame(imgIRequest* aRequest,
                                          nsIFrame* aFrame)
{
  FrameSet* frameSet = nullptr;
  RequestSet* requestSet = nullptr;

#ifdef DEBUG
  {
    nsCOMPtr<imgINotificationObserver> observer;
    aRequest->GetNotificationObserver(getter_AddRefs(observer));
    MOZ_ASSERT(!observer || observer == this);
  }
#endif

  mRequestToFrameMap.Get(aRequest, &frameSet);
  mFrameToRequestMap.Get(aFrame, &requestSet);

  if (frameSet) {
    frameSet->RemoveElementSorted(aFrame);
  }
  if (requestSet) {
    requestSet->RemoveElementSorted(aRequest);
  }

  if (frameSet && !frameSet->Length()) {
    mRequestToFrameMap.Remove(aRequest);

    nsPresContext* presContext = GetPresContext();
    if (presContext) {
      nsLayoutUtils::DeregisterImageRequest(presContext,
                                            aRequest,
                                            nullptr);
    }
  }

  if (requestSet && !requestSet->Length()) {
    mFrameToRequestMap.Remove(aFrame);
  }
}

void
ImageLoader::DropRequestsForFrame(nsIFrame* aFrame)
{
  RequestSet* requestSet = nullptr;
  if (!mFrameToRequestMap.Get(aFrame, &requestSet)) {
    return;
  }

  NS_ASSERTION(requestSet, "This should never be null");

  RequestSet frozenRequestSet(*requestSet);
  for (RequestSet::size_type i = frozenRequestSet.Length(); i != 0; --i) {
    imgIRequest* request = frozenRequestSet.ElementAt(i - 1);

    DisassociateRequestFromFrame(request, aFrame);
  }
}

void
ImageLoader::SetAnimationMode(uint16_t aMode)
{
  NS_ASSERTION(aMode == imgIContainer::kNormalAnimMode ||
               aMode == imgIContainer::kDontAnimMode ||
               aMode == imgIContainer::kLoopOnceAnimMode,
               "Wrong Animation Mode is being set!");

  mRequestToFrameMap.EnumerateRead(SetAnimationModeEnumerator, &aMode);
}

/* static */ PLDHashOperator
ImageLoader::DeregisterRequestEnumerator(nsISupports* aKey, FrameSet* aValue,
                                         void* aClosure)
{
  imgIRequest* request = static_cast<imgIRequest*>(aKey);

#ifdef DEBUG
  {
    nsCOMPtr<imgIRequest> debugRequest = do_QueryInterface(aKey);
    NS_ASSERTION(debugRequest == request, "This is bad");
  }
#endif

  nsPresContext* presContext = static_cast<nsPresContext*>(aClosure);
  if (presContext) {
    nsLayoutUtils::DeregisterImageRequest(presContext,
                                          request,
                                          nullptr);
  }

  return PL_DHASH_NEXT;
}

void
ImageLoader::ClearFrames(nsPresContext* aPresContext)
{
  mRequestToFrameMap.EnumerateRead(DeregisterRequestEnumerator, aPresContext);
  mRequestToFrameMap.Clear();
  mFrameToRequestMap.Clear();
}

void
ImageLoader::LoadImage(nsIURI* aURI, nsIPrincipal* aOriginPrincipal,
                       nsIURI* aReferrer, ImageLoader::Image* aImage)
{
  NS_ASSERTION(aImage->mRequests.Count() == 0, "Huh?");

  aImage->mRequests.Put(nullptr, nullptr);

  if (!aURI) {
    return;
  }

  if (!nsContentUtils::CanLoadImage(aURI, mDocument, mDocument,
                                    aOriginPrincipal)) {
    return;
  }

  nsRefPtr<imgRequestProxy> request;
  nsContentUtils::LoadImage(aURI, mDocument, aOriginPrincipal, aReferrer,
                            mDocument->GetReferrerPolicy(),
                            nullptr, nsIRequest::LOAD_NORMAL,
                            NS_LITERAL_STRING("css"),
                            getter_AddRefs(request));

  if (!request) {
    return;
  }

  nsRefPtr<imgRequestProxy> clonedRequest;
  mInClone = true;
  nsresult rv = request->Clone(this, getter_AddRefs(clonedRequest));
  mInClone = false;

  if (NS_FAILED(rv)) {
    return;
  }

  aImage->mRequests.Put(nullptr, request);
  aImage->mRequests.Put(mDocument, clonedRequest);

  AddImage(aImage);
}

void
ImageLoader::AddImage(ImageLoader::Image* aImage)
{
  NS_ASSERTION(!mImages.Contains(aImage), "Huh?");
  mImages.PutEntry(aImage);
}

void
ImageLoader::RemoveImage(ImageLoader::Image* aImage)
{
  NS_ASSERTION(mImages.Contains(aImage), "Huh?");
  mImages.RemoveEntry(aImage);
}

nsPresContext*
ImageLoader::GetPresContext()
{
  if (!mDocument) {
    return nullptr;
  }

  nsIPresShell* shell = mDocument->GetShell();
  if (!shell) {
    return nullptr;
  }

  return shell->GetPresContext();
}

void InvalidateImagesCallback(nsIFrame* aFrame, 
                              FrameLayerBuilder::DisplayItemData* aItem)
{
  nsDisplayItem::Type type = nsDisplayItem::GetDisplayItemTypeFromKey(aItem->GetDisplayItemKey());
  uint8_t flags = nsDisplayItem::GetDisplayItemFlagsForType(type);

  if (flags & nsDisplayItem::TYPE_RENDERS_NO_IMAGES) {
    return;
  }

  if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
    printf_stderr("Invalidating display item(type=%d) based on frame %p \
      because it might contain an invalidated image\n", type, aFrame);
  }
  aItem->Invalidate();
  aFrame->SchedulePaint();
}

void
ImageLoader::DoRedraw(FrameSet* aFrameSet, bool aForcePaint)
{
  NS_ASSERTION(aFrameSet, "Must have a frame set");
  NS_ASSERTION(mDocument, "Should have returned earlier!");

  FrameSet::size_type length = aFrameSet->Length();
  for (FrameSet::size_type i = 0; i < length; i++) {
    nsIFrame* frame = aFrameSet->ElementAt(i);

    if (frame->StyleVisibility()->IsVisible()) {
      if (frame->IsFrameOfType(nsIFrame::eTablePart)) {
        // Tables don't necessarily build border/background display items
        // for the individual table part frames, so IterateRetainedDataFor
        // might not find the right display item.
        frame->InvalidateFrame();
      } else {
        FrameLayerBuilder::IterateRetainedDataFor(frame, InvalidateImagesCallback);

        // Update ancestor rendering observers (-moz-element etc)
        nsIFrame *f = frame;
        while (f && !f->HasAnyStateBits(NS_FRAME_DESCENDANT_NEEDS_PAINT)) {
          nsSVGEffects::InvalidateDirectRenderingObservers(f);
          f = nsLayoutUtils::GetCrossDocParentFrame(f);
        }

        if (aForcePaint) {
          frame->SchedulePaint();
        }
      }
    }
  }
}

NS_IMPL_ADDREF(ImageLoader)
NS_IMPL_RELEASE(ImageLoader)

NS_INTERFACE_MAP_BEGIN(ImageLoader)
  NS_INTERFACE_MAP_ENTRY(imgINotificationObserver)
  NS_INTERFACE_MAP_ENTRY(imgIOnloadBlocker)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
ImageLoader::Notify(imgIRequest* aRequest, int32_t aType, const nsIntRect* aData)
{
  if (aType == imgINotificationObserver::SIZE_AVAILABLE) {
    nsCOMPtr<imgIContainer> image;
    aRequest->GetImage(getter_AddRefs(image));
    return OnSizeAvailable(aRequest, image);
  }

  if (aType == imgINotificationObserver::IS_ANIMATED) {
    return OnImageIsAnimated(aRequest);
  }

  if (aType == imgINotificationObserver::FRAME_COMPLETE) {
    return OnFrameComplete(aRequest);
  }

  if (aType == imgINotificationObserver::FRAME_UPDATE) {
    return OnFrameUpdate(aRequest);
  }

  return NS_OK;
}

nsresult
ImageLoader::OnSizeAvailable(imgIRequest* aRequest, imgIContainer* aImage)
{ 
  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return NS_OK;
  }

  aImage->SetAnimationMode(presContext->ImageAnimationMode());

  return NS_OK;
}

nsresult
ImageLoader::OnImageIsAnimated(imgIRequest* aRequest)
{
  if (!mDocument) {
    return NS_OK;
  }

  FrameSet* frameSet = nullptr;
  if (!mRequestToFrameMap.Get(aRequest, &frameSet)) {
    return NS_OK;
  }

  // Register with the refresh driver now that we are aware that
  // we are animated.
  nsPresContext* presContext = GetPresContext();
  if (presContext) {
    nsLayoutUtils::RegisterImageRequest(presContext,
                                        aRequest,
                                        nullptr);
  }

  return NS_OK;
}

nsresult
ImageLoader::OnFrameComplete(imgIRequest* aRequest)
{
  if (!mDocument || mInClone) {
    return NS_OK;
  }

  FrameSet* frameSet = nullptr;
  if (!mRequestToFrameMap.Get(aRequest, &frameSet)) {
    return NS_OK;
  }

  NS_ASSERTION(frameSet, "This should never be null!");

  // Since we just finished decoding a frame, we always want to paint, in case
  // we're now able to paint an image that we couldn't paint before (and hence
  // that we don't have retained data for).
  DoRedraw(frameSet, /* aForcePaint = */ true);

  return NS_OK;
}

nsresult
ImageLoader::OnFrameUpdate(imgIRequest* aRequest)
{
  if (!mDocument || mInClone) {
    return NS_OK;
  }

  FrameSet* frameSet = nullptr;
  if (!mRequestToFrameMap.Get(aRequest, &frameSet)) {
    return NS_OK;
  }

  NS_ASSERTION(frameSet, "This should never be null!");

  DoRedraw(frameSet, /* aForcePaint = */ false);

  return NS_OK;
}

NS_IMETHODIMP
ImageLoader::BlockOnload(imgIRequest* aRequest)
{
  if (!mDocument) {
    return NS_OK;
  }

  mDocument->BlockOnload();

  return NS_OK;
}

NS_IMETHODIMP
ImageLoader::UnblockOnload(imgIRequest* aRequest)
{
  if (!mDocument) {
    return NS_OK;
  }

  mDocument->UnblockOnload(false);

  return NS_OK;
}

} // namespace css
} // namespace mozilla
