/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A class that handles style system image loads (other image loads are handled
 * by the nodes in the content tree).
 */

#include "mozilla/css/ImageLoader.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsNetError.h"

namespace mozilla {
namespace css {

/* static */ PLDHashOperator
ImageLoader::SetAnimationModeEnumerator(nsISupports* aKey, FrameSet* aValue,
                                        void* aClosure)
{
  imgIRequest* request = static_cast<imgIRequest*>(aKey);

  PRUint16* mode = static_cast<PRUint16*>(aClosure);

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

nsresult
ImageLoader::Init()
{
  MOZ_ASSERT(mDocument);

  if (!mRequestToFrameMap.Init() ||
      !mFrameToRequestMap.Init() ||
      !mImages.Init()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void
ImageLoader::DropDocumentReference()
{
  ClearAll();
  mDocument = nsnull;
}

void
ImageLoader::AssociateRequestToFrame(imgIRequest* aRequest,
                                     nsIFrame* aFrame)
{
  MOZ_ASSERT(mRequestToFrameMap.IsInitialized() &&
             mFrameToRequestMap.IsInitialized() &&
             mImages.IsInitialized());

  nsCOMPtr<imgIDecoderObserver> observer;
  aRequest->GetDecoderObserver(getter_AddRefs(observer));
  if (!observer) {
    // The request has already been canceled, so ignore it.  This is ok because
    // we're not going to get any more notifications from a canceled request.
    return;
  }

  MOZ_ASSERT(observer == this);

  FrameSet* frameSet = nsnull;
  if (mRequestToFrameMap.Get(aRequest, &frameSet)) {
    NS_ASSERTION(frameSet, "This should never be null!");
  }

  if (!frameSet) {
    nsAutoPtr<FrameSet> newFrameSet(new FrameSet());

    bool result = mRequestToFrameMap.Put(aRequest, newFrameSet);
    if (!result) {
      return;
    }

    frameSet = newFrameSet.forget();
  }

  RequestSet* requestSet = nsnull;
  if (mFrameToRequestMap.Get(aFrame, &requestSet)) {
    NS_ASSERTION(requestSet, "This should never be null");
  }

  if (!requestSet) {
    nsAutoPtr<RequestSet> newRequestSet(new RequestSet());

    bool result = mFrameToRequestMap.Put(aFrame, newRequestSet);
    if (!result) {
      return;
    }

    requestSet = newRequestSet.forget();
  }

  // Add these to the sets, but only if they're not already there.
  PRUint32 i;
  if (!frameSet->GreatestIndexLtEq(aFrame, i)) {
    frameSet->InsertElementAt(i, aFrame);
  }
  if (!requestSet->GreatestIndexLtEq(aRequest, i)) {
    requestSet->InsertElementAt(i, aRequest);
  }
}

void
ImageLoader::MaybeRegisterCSSImage(nsCSSValue::Image* aImage)
{
  NS_ASSERTION(aImage, "This should never be null!");

  bool found = false;
  aImage->mRequests.GetWeak(mDocument, &found);
  if (found) {
    // This document already has a request.
    return;
  }

  imgIRequest* canonicalRequest = aImage->mRequests.GetWeak(nsnull);
  if (!canonicalRequest) {
    // The image was blocked or something.
    return;
  }

  nsCOMPtr<imgIRequest> request;

  // Ignore errors here.  If cloning fails for some reason we'll put a null
  // entry in the hash and we won't keep trying to clone.
  mInClone = true;
  canonicalRequest->Clone(this, getter_AddRefs(request));
  mInClone = false;

  aImage->mRequests.Put(mDocument, request);

  AddImage(aImage);
}

void
ImageLoader::DeregisterCSSImage(nsCSSValue::Image* aImage)
{
  RemoveImage(aImage);
}

void
ImageLoader::DisassociateRequestFromFrame(imgIRequest* aRequest,
                                          nsIFrame* aFrame)
{
  FrameSet* frameSet = nsnull;
  RequestSet* requestSet = nsnull;

  MOZ_ASSERT(mRequestToFrameMap.IsInitialized() &&
             mFrameToRequestMap.IsInitialized() &&
             mImages.IsInitialized());

#ifdef DEBUG
  {
    nsCOMPtr<imgIDecoderObserver> observer;
    aRequest->GetDecoderObserver(getter_AddRefs(observer));
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
                                            nsnull);
    }
  }

  if (requestSet && !requestSet->Length()) {
    mFrameToRequestMap.Remove(aFrame);
  }
}

void
ImageLoader::DropRequestsForFrame(nsIFrame* aFrame)
{
  RequestSet* requestSet = nsnull;
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
ImageLoader::SetAnimationMode(PRUint16 aMode)
{
  NS_ASSERTION(aMode == imgIContainer::kNormalAnimMode ||
               aMode == imgIContainer::kDontAnimMode ||
               aMode == imgIContainer::kLoopOnceAnimMode,
               "Wrong Animation Mode is being set!");

  mRequestToFrameMap.EnumerateRead(SetAnimationModeEnumerator, &aMode);
}

static PLDHashOperator
ClearImageHashSet(nsPtrHashKey<nsCSSValue::Image>* aKey, void* aClosure)
{
  nsIDocument* doc = static_cast<nsIDocument*>(aClosure);
  nsCSSValue::Image* image = aKey->GetKey();

  imgIRequest* request = image->mRequests.GetWeak(doc);
  if (request) {
    request->CancelAndForgetObserver(NS_BINDING_ABORTED);
  }

  image->mRequests.Remove(doc);

  return PL_DHASH_REMOVE;
}

void
ImageLoader::ClearAll()
{
  mRequestToFrameMap.Clear();
  mFrameToRequestMap.Clear();
  mImages.EnumerateEntries(&ClearImageHashSet, mDocument);
}

void
ImageLoader::LoadImage(nsIURI* aURI, nsIPrincipal* aOriginPrincipal,
                       nsIURI* aReferrer, nsCSSValue::Image* aImage)
{
  NS_ASSERTION(aImage->mRequests.Count() == 0, "Huh?");

  aImage->mRequests.Put(nsnull, nsnull);

  if (!aURI) {
    return;
  }

  if (!nsContentUtils::CanLoadImage(aURI, mDocument, mDocument,
                                    aOriginPrincipal)) {
    return;
  }

  nsCOMPtr<imgIRequest> request;
  nsContentUtils::LoadImage(aURI, mDocument, aOriginPrincipal, aReferrer,
                            nsnull, nsIRequest::LOAD_NORMAL,
                            getter_AddRefs(request));

  if (!request) {
    return;
  }

  nsCOMPtr<imgIRequest> clonedRequest;
  mInClone = true;
  nsresult rv = request->Clone(this, getter_AddRefs(clonedRequest));
  mInClone = false;

  if (NS_FAILED(rv)) {
    return;
  }

  aImage->mRequests.Put(nsnull, request);
  aImage->mRequests.Put(mDocument, clonedRequest);

  AddImage(aImage);
}

void
ImageLoader::AddImage(nsCSSValue::Image* aImage)
{
  NS_ASSERTION(!mImages.Contains(aImage), "Huh?");
  if (!mImages.PutEntry(aImage)) {
    NS_RUNTIMEABORT("OOM");
  }
}

void
ImageLoader::RemoveImage(nsCSSValue::Image* aImage)
{
  NS_ASSERTION(mImages.Contains(aImage), "Huh?");
  mImages.RemoveEntry(aImage);
}

nsPresContext*
ImageLoader::GetPresContext()
{
  if (!mDocument) {
    return nsnull;
  }

  nsIPresShell* shell = mDocument->GetShell();
  if (!shell) {
    return nsnull;
  }

  return shell->GetPresContext();
}

void
ImageLoader::DoRedraw(FrameSet* aFrameSet)
{
  NS_ASSERTION(aFrameSet, "Must have a frame set");
  NS_ASSERTION(mDocument, "Should have returned earlier!");
  NS_ASSERTION(mHavePainted, "Should have returned earlier!");

  FrameSet::size_type length = aFrameSet->Length();
  for (FrameSet::size_type i = 0; i < length; i++) {
    nsIFrame* frame = aFrameSet->ElementAt(i);

    // NOTE: It is not sufficient to invalidate only the size of the image:
    //       the image may be tiled! 
    //       The best option is to call into the frame, however lacking this
    //       we have to at least invalidate the frame's bounds, hence
    //       as long as we have a frame we'll use its size.
    //

    // Invalidate the entire frame
    // XXX We really only need to invalidate the client area of the frame...    

    nsRect bounds(nsPoint(0, 0), frame->GetSize());

    if (frame->GetType() == nsGkAtoms::canvasFrame) {
      // The canvas's background covers the whole viewport.
      bounds = frame->GetVisualOverflowRect();
    }

    if (frame->GetStyleVisibility()->IsVisible()) {
      frame->Invalidate(bounds);
    }
  }
}

NS_IMPL_ADDREF(ImageLoader)
NS_IMPL_RELEASE(ImageLoader)

NS_INTERFACE_MAP_BEGIN(ImageLoader)
  NS_INTERFACE_MAP_ENTRY(imgIDecoderObserver)
  NS_INTERFACE_MAP_ENTRY(imgIContainerObserver)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
ImageLoader::OnStartContainer(imgIRequest* aRequest, imgIContainer* aImage)
{ 
  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return NS_OK;
  }

  aImage->SetAnimationMode(presContext->ImageAnimationMode());

  return NS_OK;
}

NS_IMETHODIMP
ImageLoader::OnImageIsAnimated(imgIRequest* aRequest)
{
  // NB: Don't ignore this when cloning, it's our only chance to register
  // the request with the refresh driver.
  if (!mDocument) {
    return NS_OK;
  }

  // Register with the refresh driver now that we are aware that
  // we are animated.
  nsPresContext* presContext = GetPresContext();
  if (presContext) {
    nsLayoutUtils::RegisterImageRequest(presContext,
                                        aRequest,
                                        nsnull);
  }

  return NS_OK;
}

NS_IMETHODIMP
ImageLoader::OnStopFrame(imgIRequest *aRequest, PRUint32 aFrame)
{
  if (!mDocument || !mHavePainted || mInClone) {
    return NS_OK;
  }

  FrameSet* frameSet = nsnull;
  if (!mRequestToFrameMap.Get(aRequest, &frameSet)) {
    return NS_OK;
  }

  NS_ASSERTION(frameSet, "This should never be null!");

  DoRedraw(frameSet);

  return NS_OK;
}

NS_IMETHODIMP
ImageLoader::FrameChanged(imgIRequest *aRequest,
                          imgIContainer *aContainer,
                          const nsIntRect *aDirtyRect)
{
  if (!mDocument || !mHavePainted || mInClone) {
    return NS_OK;
  }

  FrameSet* frameSet = nsnull;
  if (!mRequestToFrameMap.Get(aRequest, &frameSet)) {
    return NS_OK;
  }

  NS_ASSERTION(frameSet, "This should never be null!");

  DoRedraw(frameSet);

  return NS_OK;
}

} // namespace css
} // namespace mozilla
