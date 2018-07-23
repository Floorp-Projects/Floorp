/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A class that handles style system image loads (other image loads are handled
 * by the nodes in the content tree).
 */

#include "mozilla/css/ImageLoader.h"
#include "nsAutoPtr.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsError.h"
#include "nsDisplayList.h"
#include "nsIFrameInlines.h"
#include "FrameLayerBuilder.h"
#include "SVGObserverUtils.h"
#include "imgIContainer.h"
#include "Image.h"
#include "GeckoProfiler.h"
#include "mozilla/layers/WebRenderUserData.h"

namespace mozilla {
namespace css {

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

// Normally, arrays of requests and frames are sorted by their pointer address,
// for faster lookup. When recording or replaying, we don't do this, so that
// the arrays retain their insertion order and are consistent between recording
// and replaying.
template <typename Elem, typename Item, typename Comparator = nsDefaultComparator<Elem, Item>>
static size_t
GetMaybeSortedIndex(const nsTArray<Elem>& aArray, const Item& aItem, bool* aFound,
                    Comparator aComparator = Comparator())
{
  if (recordreplay::IsRecordingOrReplaying()) {
    size_t index = aArray.IndexOf(aItem, 0, aComparator);
    *aFound = index != nsTArray<Elem>::NoIndex;
    return *aFound ? index + 1 : aArray.Length();
  }
  size_t index = aArray.IndexOfFirstElementGt(aItem, aComparator);
  *aFound = index > 0 && aComparator.Equals(aItem, aArray.ElementAt(index - 1));
  return index;
}

void
ImageLoader::AssociateRequestToFrame(imgIRequest* aRequest,
                                     nsIFrame* aFrame,
                                     FrameFlags aFlags)
{
  nsCOMPtr<imgINotificationObserver> observer;
  aRequest->GetNotificationObserver(getter_AddRefs(observer));
  if (!observer) {
    // The request has already been canceled, so ignore it.  This is ok because
    // we're not going to get any more notifications from a canceled request.
    return;
  }

  MOZ_ASSERT(observer == this);

  FrameSet* frameSet =
    mRequestToFrameMap.LookupForAdd(aRequest).OrInsert([=]() {
      nsPresContext* presContext = GetPresContext();
      if (presContext) {
        nsLayoutUtils::RegisterImageRequestIfAnimated(presContext,
                                                      aRequest,
                                                      nullptr);
      }
      return new FrameSet();
    });

  RequestSet* requestSet =
    mFrameToRequestMap.LookupForAdd(aFrame).OrInsert([=]() {
      aFrame->SetHasImageRequest(true);
      return new RequestSet();
    });

  // Add frame to the frameSet, and handle any special processing the
  // frame might require.
  FrameWithFlags fwf(aFrame);
  FrameWithFlags* fwfToModify(&fwf);

  // See if the frameSet already has this frame.
  bool found;
  uint32_t i = GetMaybeSortedIndex(*frameSet, fwf, &found, FrameOnlyComparator());
  if (found) {
    // We're already tracking this frame, so prepare to modify the
    // existing FrameWithFlags object.
    fwfToModify = &frameSet->ElementAt(i-1);
  }

  // Check if the frame requires special processing.
  if (aFlags & REQUEST_REQUIRES_REFLOW) {
    fwfToModify->mFlags |= REQUEST_REQUIRES_REFLOW;

    // If we weren't already blocking onload, do that now.
    if ((fwfToModify->mFlags & REQUEST_HAS_BLOCKED_ONLOAD) == 0) {
      // Get request status to see if we should block onload, and if we can
      // request reflow immediately.
      uint32_t status = 0;
      if (NS_SUCCEEDED(aRequest->GetImageStatus(&status)) &&
          !(status & imgIRequest::STATUS_ERROR)) {
        // No error, so we can block onload.
        fwfToModify->mFlags |= REQUEST_HAS_BLOCKED_ONLOAD;

        // Block document onload until we either remove the frame in
        // RemoveRequestToFrameMapping or onLoadComplete, or complete a reflow.
        mDocument->BlockOnload();

        // We need to stay blocked until we get a reflow. If the first frame
        // is not yet decoded, we'll trigger that reflow from onFrameComplete.
        // But if the first frame is already decoded, we need to trigger that
        // reflow now, because we'll never get a call to onFrameComplete.
        if(status & imgIRequest::STATUS_FRAME_COMPLETE) {
          RequestReflowOnFrame(fwfToModify, aRequest);
        } else {
          // If we don't already have a complete frame, kickoff decode. This
          // will ensure that either onFrameComplete or onLoadComplete will
          // unblock document onload.

          // We want to request decode in such a way that avoids triggering
          // sync decode. First, we attempt to convert the aRequest into
          // a imgIContainer. If that succeeds, then aRequest has an image
          // and we can request decoding for size at zero size, and that will
          // trigger async decode. If the conversion to imgIContainer is
          // unsuccessful, then that means aRequest doesn't have an image yet,
          // which means we can safely call StartDecoding() on it without
          // triggering any synchronous work.
          nsCOMPtr<imgIContainer> imgContainer;
          aRequest->GetImage(getter_AddRefs(imgContainer));
          if (imgContainer) {
            imgContainer->RequestDecodeForSize(gfx::IntSize(0, 0),
              imgIContainer::DECODE_FLAGS_DEFAULT);
          } else {
            // It's safe to call StartDecoding directly, since it can't
            // trigger synchronous decode without an image. Flags are ignored.
            aRequest->StartDecoding(imgIContainer::FLAG_NONE);
          }
        }
      }
    }
  }

  // Do some sanity checking to ensure that we only add to one mapping
  // iff we also add to the other mapping.
  DebugOnly<bool> didAddToFrameSet(false);
  DebugOnly<bool> didAddToRequestSet(false);

  // If we weren't already tracking this frame, add it to the frameSet.
  if (!found) {
    frameSet->InsertElementAt(i, fwf);
    didAddToFrameSet = true;
  }

  // Add request to the request set if it wasn't already there.
  i = GetMaybeSortedIndex(*requestSet, aRequest, &found);
  if (!found) {
    requestSet->InsertElementAt(i, aRequest);
    didAddToRequestSet = true;
  }

  MOZ_ASSERT(didAddToFrameSet == didAddToRequestSet,
             "We should only add to one map iff we also add to the other map.");
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

  RefPtr<imgRequestProxy> request;

  // Ignore errors here.  If cloning fails for some reason we'll put a null
  // entry in the hash and we won't keep trying to clone.
  mInClone = true;
  canonicalRequest->SyncClone(this, mDocument, getter_AddRefs(request));
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
ImageLoader::RemoveRequestToFrameMapping(imgIRequest* aRequest,
                                         nsIFrame*    aFrame)
{
#ifdef DEBUG
  {
    nsCOMPtr<imgINotificationObserver> observer;
    aRequest->GetNotificationObserver(getter_AddRefs(observer));
    MOZ_ASSERT(!observer || observer == this);
  }
#endif

  if (auto entry = mRequestToFrameMap.Lookup(aRequest)) {
    FrameSet* frameSet = entry.Data();
    MOZ_ASSERT(frameSet, "This should never be null");

    // Before we remove aFrame from the frameSet, unblock onload if needed.
    bool found;
    uint32_t i = GetMaybeSortedIndex(*frameSet, FrameWithFlags(aFrame), &found,
                                     FrameOnlyComparator());
    if (found) {
      FrameWithFlags& fwf = frameSet->ElementAt(i-1);
      if (fwf.mFlags & REQUEST_HAS_BLOCKED_ONLOAD) {
        mDocument->UnblockOnload(false);
        // We're about to remove fwf from the frameSet, so we don't bother
        // updating the flag.
      }
      frameSet->RemoveElementAt(i-1);
    }

    if (frameSet->IsEmpty()) {
      nsPresContext* presContext = GetPresContext();
      if (presContext) {
        nsLayoutUtils::DeregisterImageRequest(presContext, aRequest, nullptr);
      }
      entry.Remove();
    }
  }
}

void
ImageLoader::RemoveFrameToRequestMapping(imgIRequest* aRequest,
                                         nsIFrame*    aFrame)
{
  if (auto entry = mFrameToRequestMap.Lookup(aFrame)) {
    RequestSet* requestSet = entry.Data();
    MOZ_ASSERT(requestSet, "This should never be null");
    if (recordreplay::IsRecordingOrReplaying()) {
      requestSet->RemoveElement(aRequest);
    } else {
      requestSet->RemoveElementSorted(aRequest);
    }
    if (requestSet->IsEmpty()) {
      aFrame->SetHasImageRequest(false);
      entry.Remove();
    }
  }
}

void
ImageLoader::DisassociateRequestFromFrame(imgIRequest* aRequest,
                                          nsIFrame*    aFrame)
{
  MOZ_ASSERT(aFrame->HasImageRequest(), "why call me?");
  RemoveRequestToFrameMapping(aRequest, aFrame);
  RemoveFrameToRequestMapping(aRequest, aFrame);
}

void
ImageLoader::DropRequestsForFrame(nsIFrame* aFrame)
{
  MOZ_ASSERT(aFrame->HasImageRequest(), "why call me?");
  nsAutoPtr<RequestSet> requestSet;
  mFrameToRequestMap.Remove(aFrame, &requestSet);
  aFrame->SetHasImageRequest(false);
  if (MOZ_UNLIKELY(!requestSet)) {
    MOZ_ASSERT_UNREACHABLE("HasImageRequest was lying");
    return;
  }
  for (imgIRequest* request : *requestSet) {
    RemoveRequestToFrameMapping(request, aFrame);
  }
}

void
ImageLoader::SetAnimationMode(uint16_t aMode)
{
  NS_ASSERTION(aMode == imgIContainer::kNormalAnimMode ||
               aMode == imgIContainer::kDontAnimMode ||
               aMode == imgIContainer::kLoopOnceAnimMode,
               "Wrong Animation Mode is being set!");

  for (auto iter = mRequestToFrameMap.ConstIter(); !iter.Done(); iter.Next()) {
    auto request = static_cast<imgIRequest*>(iter.Key());

#ifdef DEBUG
    {
      nsCOMPtr<imgIRequest> debugRequest = do_QueryInterface(request);
      NS_ASSERTION(debugRequest == request, "This is bad");
    }
#endif

    nsCOMPtr<imgIContainer> container;
    request->GetImage(getter_AddRefs(container));
    if (!container) {
      continue;
    }

    // This can fail if the image is in error, and we don't care.
    container->SetAnimationMode(aMode);
  }
}

void
ImageLoader::ClearFrames(nsPresContext* aPresContext)
{
  for (auto iter = mRequestToFrameMap.ConstIter(); !iter.Done(); iter.Next()) {
    auto request = static_cast<imgIRequest*>(iter.Key());

#ifdef DEBUG
    {
      nsCOMPtr<imgIRequest> debugRequest = do_QueryInterface(request);
      NS_ASSERTION(debugRequest == request, "This is bad");
    }
#endif

    if (aPresContext) {
      nsLayoutUtils::DeregisterImageRequest(aPresContext,
                                            request,
                                            nullptr);
    }
  }

  mRequestToFrameMap.Clear();
  mFrameToRequestMap.Clear();
}

void
ImageLoader::LoadImage(nsIURI* aURI, nsIPrincipal* aOriginPrincipal,
                       nsIURI* aReferrer, ImageLoader::Image* aImage,
                       CORSMode aCorsMode)
{
  NS_ASSERTION(aImage->mRequests.Count() == 0, "Huh?");

  aImage->mRequests.Put(nullptr, nullptr);

  if (!aURI) {
    return;
  }

  int32_t loadFlags = nsIRequest::LOAD_NORMAL |
                      nsContentUtils::CORSModeToLoadImageFlags(aCorsMode);

  RefPtr<imgRequestProxy> request;
  nsresult rv = nsContentUtils::LoadImage(aURI, mDocument, mDocument,
                                          aOriginPrincipal, 0, aReferrer,
                                          mDocument->GetReferrerPolicy(),
                                          nullptr, loadFlags,
                                          NS_LITERAL_STRING("css"),
                                          getter_AddRefs(request));

  if (NS_FAILED(rv) || !request) {
    return;
  }

  RefPtr<imgRequestProxy> clonedRequest;
  mInClone = true;
  rv = request->SyncClone(this, mDocument, getter_AddRefs(clonedRequest));
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

  return mDocument->GetPresContext();
}

static bool
IsRenderNoImages(uint32_t aDisplayItemKey)
{
  DisplayItemType type = GetDisplayItemTypeFromKey(aDisplayItemKey);
  uint8_t flags = GetDisplayItemFlagsForType(type);
  return flags & TYPE_RENDERS_NO_IMAGES;
}

static void
InvalidateImages(nsIFrame* aFrame)
{
  bool invalidateFrame = false;
  const SmallPointerArray<DisplayItemData>& array = aFrame->DisplayItemData();
  for (uint32_t i = 0; i < array.Length(); i++) {
    DisplayItemData* data = DisplayItemData::AssertDisplayItemData(array.ElementAt(i));
    uint32_t displayItemKey = data->GetDisplayItemKey();
    if (displayItemKey != 0 && !IsRenderNoImages(displayItemKey)) {
      if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
        DisplayItemType type = GetDisplayItemTypeFromKey(displayItemKey);
        printf_stderr("Invalidating display item(type=%d) based on frame %p \
                       because it might contain an invalidated image\n",
                       static_cast<uint32_t>(type), aFrame);
      }

      data->Invalidate();
      invalidateFrame = true;
    }
  }
  if (auto userDataTable =
       aFrame->GetProperty(layers::WebRenderUserDataProperty::Key())) {
    for (auto iter = userDataTable->Iter(); !iter.Done(); iter.Next()) {
      RefPtr<layers::WebRenderUserData> data = iter.UserData();
      if (data->GetType() == layers::WebRenderAnimationData::UserDataType::eFallback &&
          !IsRenderNoImages(data->GetDisplayItemKey())) {
        static_cast<layers::WebRenderFallbackData*>(data.get())->SetInvalid(true);
      }
      //XXX: handle Blob data
      invalidateFrame = true;
    }
  }

  if (invalidateFrame) {
    aFrame->SchedulePaint();
  }
}

void
ImageLoader::DoRedraw(FrameSet* aFrameSet, bool aForcePaint)
{
  NS_ASSERTION(aFrameSet, "Must have a frame set");
  NS_ASSERTION(mDocument, "Should have returned earlier!");

  for (FrameWithFlags& fwf : *aFrameSet) {
    nsIFrame* frame = fwf.mFrame;
    if (frame->StyleVisibility()->IsVisible()) {
      if (frame->IsFrameOfType(nsIFrame::eTablePart)) {
        // Tables don't necessarily build border/background display items
        // for the individual table part frames, so IterateRetainedDataFor
        // might not find the right display item.
        frame->InvalidateFrame();
      } else {
        InvalidateImages(frame);

        // Update ancestor rendering observers (-moz-element etc)
        nsIFrame *f = frame;
        while (f && !f->HasAnyStateBits(NS_FRAME_DESCENDANT_NEEDS_PAINT)) {
          SVGObserverUtils::InvalidateDirectRenderingObservers(f);
          f = nsLayoutUtils::GetCrossDocParentFrame(f);
        }

        if (aForcePaint) {
          frame->SchedulePaint();
        }
      }
    }
  }
}

void
ImageLoader::UnblockOnloadIfNeeded(nsIFrame* aFrame, imgIRequest* aRequest)
{
  MOZ_ASSERT(aFrame);
  MOZ_ASSERT(aRequest);

  FrameSet* frameSet = mRequestToFrameMap.Get(aRequest);
  if (!frameSet) {
    return;
  }

  size_t i = frameSet->BinaryIndexOf(FrameWithFlags(aFrame),
                                     FrameOnlyComparator());
  if (i != FrameSet::NoIndex) {
    FrameWithFlags& fwf = frameSet->ElementAt(i);
    if (fwf.mFlags & REQUEST_HAS_BLOCKED_ONLOAD) {
      mDocument->UnblockOnload(false);
      fwf.mFlags &= ~REQUEST_HAS_BLOCKED_ONLOAD;
    }
  }
}

void
ImageLoader::RequestReflowIfNeeded(FrameSet* aFrameSet, imgIRequest* aRequest)
{
  MOZ_ASSERT(aFrameSet);

  for (FrameWithFlags& fwf : *aFrameSet) {
    if (fwf.mFlags & REQUEST_REQUIRES_REFLOW) {
      // Tell the container of the frame to reflow because the
      // image request has finished decoding its first frame.
      RequestReflowOnFrame(&fwf, aRequest);
    }
  }
}

void
ImageLoader::RequestReflowOnFrame(FrameWithFlags* aFwf, imgIRequest* aRequest)
{
  nsIFrame* frame = aFwf->mFrame;

  // Actually request the reflow.
  nsIFrame* parent = frame->GetInFlowParent();
  parent->PresShell()->FrameNeedsReflow(parent, nsIPresShell::eStyleChange,
                                        NS_FRAME_IS_DIRTY);

  // We'll respond to the reflow events by unblocking onload, regardless
  // of whether the reflow was completed or cancelled. The callback will
  // also delete itself when it is called.
  ImageReflowCallback* unblocker = new ImageReflowCallback(this, frame,
                                                           aRequest);
  parent->PresShell()->PostReflowCallback(unblocker);
}

NS_IMPL_ADDREF(ImageLoader)
NS_IMPL_RELEASE(ImageLoader)

NS_INTERFACE_MAP_BEGIN(ImageLoader)
  NS_INTERFACE_MAP_ENTRY(imgINotificationObserver)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
ImageLoader::Notify(imgIRequest* aRequest, int32_t aType, const nsIntRect* aData)
{
#ifdef MOZ_GECKO_PROFILER
  nsCString uriString;
  if (profiler_is_active()) {
    nsCOMPtr<nsIURI> uri;
    aRequest->GetFinalURI(getter_AddRefs(uri));
    if (uri) {
      uri->GetSpec(uriString);
    }
  }

  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING("ImageLoader::Notify", OTHER, uriString);
#endif

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

  if (aType == imgINotificationObserver::DECODE_COMPLETE) {
    nsCOMPtr<imgIContainer> image;
    aRequest->GetImage(getter_AddRefs(image));
    if (image && mDocument) {
      image->PropagateUseCounters(mDocument);
    }
  }

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    return OnLoadComplete(aRequest);
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

  FrameSet* frameSet = mRequestToFrameMap.Get(aRequest);
  if (!frameSet) {
    return NS_OK;
  }

  for (FrameWithFlags& fwf : *frameSet) {
    nsIFrame* frame = fwf.mFrame;
    if (frame->StyleVisibility()->IsVisible()) {
      frame->MarkNeedsDisplayItemRebuild();
    }
  }

  return NS_OK;
}

nsresult
ImageLoader::OnImageIsAnimated(imgIRequest* aRequest)
{
  if (!mDocument) {
    return NS_OK;
  }

  FrameSet* frameSet = mRequestToFrameMap.Get(aRequest);
  if (!frameSet) {
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

  FrameSet* frameSet = mRequestToFrameMap.Get(aRequest);
  if (!frameSet) {
    return NS_OK;
  }

  // We may need reflow (for example if the image is from shape-outside).
  RequestReflowIfNeeded(frameSet, aRequest);

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

  FrameSet* frameSet = mRequestToFrameMap.Get(aRequest);
  if (!frameSet) {
    return NS_OK;
  }

  DoRedraw(frameSet, /* aForcePaint = */ false);

  return NS_OK;
}

nsresult
ImageLoader::OnLoadComplete(imgIRequest* aRequest)
{
  if (!mDocument || mInClone) {
    return NS_OK;
  }

  FrameSet* frameSet = mRequestToFrameMap.Get(aRequest);
  if (!frameSet) {
    return NS_OK;
  }

  // Check if aRequest has an error state. If it does, we need to unblock
  // Document onload for all the frames associated with this request that
  // have blocked onload. This is what happens in a CORS mode violation, and
  // may happen during other network events.
  uint32_t status = 0;
  if(NS_SUCCEEDED(aRequest->GetImageStatus(&status)) &&
     status & imgIRequest::STATUS_ERROR) {
    for (FrameWithFlags& fwf : *frameSet) {
      if (fwf.mFlags & REQUEST_HAS_BLOCKED_ONLOAD) {
        // We've blocked onload. Unblock onload and clear the flag.
        mDocument->UnblockOnload(false);
        fwf.mFlags &= ~REQUEST_HAS_BLOCKED_ONLOAD;
      }
    }
  }

  return NS_OK;
}

void
ImageLoader::FlushUseCounters()
{
  for (auto iter = mImages.Iter(); !iter.Done(); iter.Next()) {
    nsPtrHashKey<Image>* key = iter.Get();
    ImageLoader::Image* image = key->GetKey();

    imgIRequest* request = image->mRequests.GetWeak(mDocument);

    nsCOMPtr<imgIContainer> container;
    request->GetImage(getter_AddRefs(container));
    if (container) {
      static_cast<image::Image*>(container.get())->ReportUseCounters();
    }
  }
}

bool
ImageLoader::ImageReflowCallback::ReflowFinished()
{
  // Check that the frame is still valid. If it isn't, then onload was
  // unblocked when the frame was removed from the FrameSet in
  // RemoveRequestToFrameMapping.
  if (mFrame.IsAlive()) {
    mLoader->UnblockOnloadIfNeeded(mFrame, mRequest);
  }

  // Get rid of this callback object.
  delete this;

  // We don't need to trigger layout.
  return false;
}

void
ImageLoader::ImageReflowCallback::ReflowCallbackCanceled()
{
  // Check that the frame is still valid. If it isn't, then onload was
  // unblocked when the frame was removed from the FrameSet in
  // RemoveRequestToFrameMapping.
  if (mFrame.IsAlive()) {
    mLoader->UnblockOnloadIfNeeded(mFrame, mRequest);
  }

  // Get rid of this callback object.
  delete this;
}

} // namespace css
} // namespace mozilla
