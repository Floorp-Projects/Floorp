/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A class that handles style system image loads (other image loads are handled
 * by the nodes in the content tree).
 */

#include "mozilla/css/ImageLoader.h"

#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/ImageTracker.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsError.h"
#include "nsCanvasFrame.h"
#include "nsDisplayList.h"
#include "nsIFrameInlines.h"
#include "FrameLayerBuilder.h"
#include "SVGObserverUtils.h"
#include "imgIContainer.h"
#include "Image.h"
#include "GeckoProfiler.h"
#include "mozilla/PresShell.h"
#include "mozilla/layers/WebRenderUserData.h"

using namespace mozilla::dom;

namespace mozilla {
namespace css {

// This is a singleton observer which looks in the `GlobalRequestTable` to look
// at which loaders to notify.
struct GlobalImageObserver final : public imgINotificationObserver {
  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER

  GlobalImageObserver() = default;

 private:
  virtual ~GlobalImageObserver() = default;
};

NS_IMPL_ADDREF(GlobalImageObserver)
NS_IMPL_RELEASE(GlobalImageObserver)

NS_INTERFACE_MAP_BEGIN(GlobalImageObserver)
  NS_INTERFACE_MAP_ENTRY(imgINotificationObserver)
NS_INTERFACE_MAP_END

// Data associated with every started load.
struct ImageTableEntry {
  // Set of all ImageLoaders that have registered this URL and care for updates
  // for it.
  nsTHashtable<nsPtrHashKey<ImageLoader>> mImageLoaders;

  // The amount of style values that are sharing this image.
  uint32_t mSharedCount = 1;
};

using GlobalRequestTable =
    nsClassHashtable<nsRefPtrHashKey<imgIRequest>, ImageTableEntry>;

// A table of all loads, keyed by their id mapping them to the set of
// ImageLoaders they have been registered in, and recording their "canonical"
// image request.
//
// We use the load id as the key since we can only access sImages on the
// main thread, but LoadData objects might be destroyed from other threads,
// and we don't want to leave dangling pointers around.
static GlobalRequestTable* sImages = nullptr;
static StaticRefPtr<GlobalImageObserver> sImageObserver;

/* static */
void ImageLoader::Init() {
  sImages = new GlobalRequestTable();
  sImageObserver = new GlobalImageObserver();
}

/* static */
void ImageLoader::Shutdown() {
  delete sImages;
  sImages = nullptr;
  sImageObserver = nullptr;
}

void ImageLoader::DropDocumentReference() {
  MOZ_ASSERT(NS_IsMainThread());

  // It's okay if GetPresContext returns null here (due to the presshell pointer
  // on the document being null) as that means the presshell has already
  // been destroyed, and it also calls ClearFrames when it is destroyed.
  ClearFrames(GetPresContext());

  mDocument = nullptr;
}

// Arrays of requests and frames are sorted by their pointer address,
// for faster lookup.
template <typename Elem, typename Item,
          typename Comparator = nsDefaultComparator<Elem, Item>>
static size_t GetMaybeSortedIndex(const nsTArray<Elem>& aArray,
                                  const Item& aItem, bool* aFound,
                                  Comparator aComparator = Comparator()) {
  size_t index = aArray.IndexOfFirstElementGt(aItem, aComparator);
  *aFound = index > 0 && aComparator.Equals(aItem, aArray.ElementAt(index - 1));
  return index;
}

void ImageLoader::AssociateRequestToFrame(imgIRequest* aRequest,
                                          nsIFrame* aFrame, FrameFlags aFlags) {
  MOZ_ASSERT(NS_IsMainThread());

  {
    nsCOMPtr<imgINotificationObserver> observer;
    aRequest->GetNotificationObserver(getter_AddRefs(observer));
    if (!observer) {
      // The request has already been canceled, so ignore it. This is ok because
      // we're not going to get any more notifications from a canceled request.
      return;
    }
    MOZ_ASSERT(observer == sImageObserver);
  }

  const auto& frameSet =
      mRequestToFrameMap.LookupForAdd(aRequest).OrInsert([=]() {
        mDocument->ImageTracker()->Add(aRequest);

        if (auto entry = sImages->Lookup(aRequest)) {
          DebugOnly<bool> inserted =
              entry.Data()->mImageLoaders.EnsureInserted(this);
          MOZ_ASSERT(inserted);
        } else {
          MOZ_ASSERT_UNREACHABLE(
              "Shouldn't be associating images not in sImages");
        }

        if (nsPresContext* presContext = GetPresContext()) {
          nsLayoutUtils::RegisterImageRequestIfAnimated(presContext, aRequest,
                                                        nullptr);
        }
        return new FrameSet();
      });

  const auto& requestSet =
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
  uint32_t i =
      GetMaybeSortedIndex(*frameSet, fwf, &found, FrameOnlyComparator());
  if (found) {
    // We're already tracking this frame, so prepare to modify the
    // existing FrameWithFlags object.
    fwfToModify = &frameSet->ElementAt(i - 1);
  }

  // Check if the frame requires special processing.
  if (aFlags & REQUEST_REQUIRES_REFLOW) {
    fwfToModify->mFlags |= REQUEST_REQUIRES_REFLOW;

    // If we weren't already blocking onload, do that now.
    if ((fwfToModify->mFlags & REQUEST_HAS_BLOCKED_ONLOAD) == 0) {
      // Get request status to see if we should block onload, and if we can
      // request reflow immediately.
      uint32_t status = 0;
      // Don't block onload if we've already got a frame complete status
      // (since in that case the image is already loaded), or if we get an error
      // status (since then we know the image won't ever load).
      if (NS_SUCCEEDED(aRequest->GetImageStatus(&status)) &&
          !(status & imgIRequest::STATUS_FRAME_COMPLETE) &&
          !(status & imgIRequest::STATUS_ERROR)) {
        // If there's no error, and the image has not loaded yet, so we can
        // block onload.
        fwfToModify->mFlags |= REQUEST_HAS_BLOCKED_ONLOAD;

        // Block document onload until we either remove the frame in
        // RemoveRequestToFrameMapping or onLoadComplete, or complete a reflow.
        mDocument->BlockOnload();

        // If we don't already have a complete frame, kickoff decode. This
        // will ensure that either onFrameComplete or onLoadComplete will
        // unblock document onload.

        // We want to request decode in such a way that avoids triggering
        // sync decode. First, we attempt to convert the aRequest into
        // a imgIContainer. If that succeeds, then aRequest has an image
        // and we can request decoding for size at zero size, the size will
        // be ignored because we don't pass the FLAG_HIGH_QUALITY_SCALING
        // flag and an async decode (because we didn't pass any sync decoding
        // flags) at the intrinsic size will be requested. If the conversion
        // to imgIContainer is unsuccessful, then that means aRequest doesn't
        // have an image yet, which means we can safely call StartDecoding()
        // on it without triggering any synchronous work.
        nsCOMPtr<imgIContainer> imgContainer;
        aRequest->GetImage(getter_AddRefs(imgContainer));
        if (imgContainer) {
          imgContainer->RequestDecodeForSize(
              gfx::IntSize(0, 0), imgIContainer::DECODE_FLAGS_DEFAULT);
        } else {
          // It's safe to call StartDecoding directly, since it can't
          // trigger synchronous decode without an image. Flags are ignored.
          aRequest->StartDecoding(imgIContainer::FLAG_NONE);
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

void ImageLoader::RemoveRequestToFrameMapping(imgIRequest* aRequest,
                                              nsIFrame* aFrame) {
#ifdef DEBUG
  {
    nsCOMPtr<imgINotificationObserver> observer;
    aRequest->GetNotificationObserver(getter_AddRefs(observer));
    MOZ_ASSERT(!observer || observer == sImageObserver);
  }
#endif

  if (auto entry = mRequestToFrameMap.Lookup(aRequest)) {
    const auto& frameSet = entry.Data();
    MOZ_ASSERT(frameSet, "This should never be null");

    // Before we remove aFrame from the frameSet, unblock onload if needed.
    bool found;
    uint32_t i = GetMaybeSortedIndex(*frameSet, FrameWithFlags(aFrame), &found,
                                     FrameOnlyComparator());
    if (found) {
      FrameWithFlags& fwf = frameSet->ElementAt(i - 1);
      if (fwf.mFlags & REQUEST_HAS_BLOCKED_ONLOAD) {
        mDocument->UnblockOnload(false);
        // We're about to remove fwf from the frameSet, so we don't bother
        // updating the flag.
      }
      frameSet->RemoveElementAt(i - 1);
    }

    if (frameSet->IsEmpty()) {
      DeregisterImageRequest(aRequest, GetPresContext());
      entry.Remove();
    }
  }
}

void ImageLoader::DeregisterImageRequest(imgIRequest* aRequest,
                                         nsPresContext* aPresContext) {
  mDocument->ImageTracker()->Remove(aRequest);

  if (auto entry = sImages->Lookup(aRequest)) {
    entry.Data()->mImageLoaders.EnsureRemoved(this);
  }

  if (aPresContext) {
    nsLayoutUtils::DeregisterImageRequest(aPresContext, aRequest, nullptr);
  }
}

void ImageLoader::RemoveFrameToRequestMapping(imgIRequest* aRequest,
                                              nsIFrame* aFrame) {
  if (auto entry = mFrameToRequestMap.Lookup(aFrame)) {
    const auto& requestSet = entry.Data();
    MOZ_ASSERT(requestSet, "This should never be null");
    requestSet->RemoveElementSorted(aRequest);
    if (requestSet->IsEmpty()) {
      aFrame->SetHasImageRequest(false);
      entry.Remove();
    }
  }
}

void ImageLoader::DisassociateRequestFromFrame(imgIRequest* aRequest,
                                               nsIFrame* aFrame) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aFrame->HasImageRequest(), "why call me?");

  RemoveRequestToFrameMapping(aRequest, aFrame);
  RemoveFrameToRequestMapping(aRequest, aFrame);
}

void ImageLoader::DropRequestsForFrame(nsIFrame* aFrame) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aFrame->HasImageRequest(), "why call me?");

  UniquePtr<RequestSet> requestSet;
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

void ImageLoader::SetAnimationMode(uint16_t aMode) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ASSERTION(aMode == imgIContainer::kNormalAnimMode ||
                   aMode == imgIContainer::kDontAnimMode ||
                   aMode == imgIContainer::kLoopOnceAnimMode,
               "Wrong Animation Mode is being set!");

  for (auto iter = mRequestToFrameMap.ConstIter(); !iter.Done(); iter.Next()) {
    auto request = static_cast<imgIRequest*>(iter.Key());

#ifdef DEBUG
    {
      nsCOMPtr<imgIRequest> debugRequest = request;
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

void ImageLoader::ClearFrames(nsPresContext* aPresContext) {
  MOZ_ASSERT(NS_IsMainThread());

  for (auto iter = mRequestToFrameMap.ConstIter(); !iter.Done(); iter.Next()) {
    auto request = static_cast<imgIRequest*>(iter.Key());

#ifdef DEBUG
    {
      nsCOMPtr<imgIRequest> debugRequest = request;
      NS_ASSERTION(debugRequest == request, "This is bad");
    }
#endif

    DeregisterImageRequest(request, aPresContext);
  }

  mRequestToFrameMap.Clear();
  mFrameToRequestMap.Clear();
}

static CORSMode EffectiveCorsMode(nsIURI* aURI,
                                  const StyleComputedImageUrl& aImage) {
  MOZ_ASSERT(aURI);
  StyleCorsMode mode = aImage.CorsMode();
  if (mode == StyleCorsMode::None) {
    return CORSMode::CORS_NONE;
  }
  MOZ_ASSERT(mode == StyleCorsMode::Anonymous);
  if (aURI->SchemeIs("resource")) {
    return CORSMode::CORS_NONE;
  }
  return CORSMode::CORS_ANONYMOUS;
}

/* static */
already_AddRefed<imgRequestProxy> ImageLoader::LoadImage(
    const StyleComputedImageUrl& aImage, Document& aDocument) {
  MOZ_ASSERT(NS_IsMainThread());
  nsIURI* uri = aImage.GetURI();
  if (!uri) {
    return nullptr;
  }

  int32_t loadFlags =
      nsIRequest::LOAD_NORMAL |
      nsContentUtils::CORSModeToLoadImageFlags(EffectiveCorsMode(uri, aImage));

  const URLExtraData& data = aImage.ExtraData();

  // NB: If aDocument is not the original document, we may not be able to load
  // images from aDocument.  Instead we do the image load from the original
  // doc and clone it to aDocument.
  Document* loadingDoc = aDocument.GetOriginalDocument();
  const bool isPrint = !!loadingDoc;
  if (!loadingDoc) {
    loadingDoc = &aDocument;
  }

  RefPtr<imgRequestProxy> request;
  nsresult rv = nsContentUtils::LoadImage(
      uri, loadingDoc, loadingDoc, data.Principal(), 0, data.ReferrerInfo(),
      sImageObserver, loadFlags, NS_LITERAL_STRING("css"),
      getter_AddRefs(request));

  if (NS_FAILED(rv) || !request) {
    return nullptr;
  }

  if (isPrint) {
    RefPtr<imgRequestProxy> ret;
    request->GetStaticRequest(&aDocument, getter_AddRefs(ret));
    // Now we have a static image. If it is different from the one from the
    // loading doc (that is, `request` is an animated image, and `ret` is a
    // frozen version of it), we can forget about notifications from the
    // animated image (assuming nothing else cares about it already).
    //
    // This is not technically needed for correctness, but helps keep the
    // invariant that we only receive notifications for images that are in
    // `sImages`.
    if (ret != request) {
      if (!sImages->Contains(request)) {
        request->CancelAndForgetObserver(NS_BINDING_ABORTED);
      }
      if (!ret) {
        return nullptr;
      }
      request = std::move(ret);
    }
  }

  sImages->LookupForAdd(request).OrInsert([] { return new ImageTableEntry(); });
  return request.forget();
}

void ImageLoader::UnloadImage(imgRequestProxy* aImage) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aImage);

  auto lookup = sImages->Lookup(aImage);
  MOZ_DIAGNOSTIC_ASSERT(lookup, "Unregistered image?");
  if (MOZ_UNLIKELY(!lookup)) {
    return;
  }

  if (MOZ_UNLIKELY(--lookup.Data()->mSharedCount)) {
    // Someone else still cares about this image.
    return;
  }

  aImage->CancelAndForgetObserver(NS_BINDING_ABORTED);
  MOZ_DIAGNOSTIC_ASSERT(lookup.Data()->mImageLoaders.IsEmpty(),
                        "Shouldn't be keeping references to any loader "
                        "by now");
  lookup.Remove();
}

void ImageLoader::NoteSharedLoad(imgRequestProxy* aImage) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aImage);

  auto lookup = sImages->Lookup(aImage);
  MOZ_DIAGNOSTIC_ASSERT(lookup, "Unregistered image?");
  if (MOZ_UNLIKELY(!lookup)) {
    return;
  }

  lookup.Data()->mSharedCount++;
}

nsPresContext* ImageLoader::GetPresContext() {
  if (!mDocument) {
    return nullptr;
  }

  return mDocument->GetPresContext();
}

static bool IsRenderNoImages(uint32_t aDisplayItemKey) {
  DisplayItemType type = GetDisplayItemTypeFromKey(aDisplayItemKey);
  uint8_t flags = GetDisplayItemFlagsForType(type);
  return flags & TYPE_RENDERS_NO_IMAGES;
}

static void InvalidateImages(nsIFrame* aFrame, imgIRequest* aRequest,
                             bool aForcePaint) {
  if (!aFrame->StyleVisibility()->IsVisible()) {
    return;
  }

  if (aFrame->IsFrameOfType(nsIFrame::eTablePart)) {
    // Tables don't necessarily build border/background display items
    // for the individual table part frames, so IterateRetainedDataFor
    // might not find the right display item.
    return aFrame->InvalidateFrame();
  }

  if (aFrame->IsPrimaryFrameOfRootOrBodyElement()) {
    // Try to invalidate the canvas too, in the probable case the background
    // was propagated to it.
    InvalidateImages(aFrame->PresShell()->GetCanvasFrame(), aRequest,
                     aForcePaint);
  }

  bool invalidateFrame = aForcePaint;
  const SmallPointerArray<DisplayItemData>& array = aFrame->DisplayItemData();
  for (uint32_t i = 0; i < array.Length(); i++) {
    DisplayItemData* data =
        DisplayItemData::AssertDisplayItemData(array.ElementAt(i));
    uint32_t displayItemKey = data->GetDisplayItemKey();
    if (displayItemKey != 0 && !IsRenderNoImages(displayItemKey)) {
      if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
        DisplayItemType type = GetDisplayItemTypeFromKey(displayItemKey);
        printf_stderr(
            "Invalidating display item(type=%d) based on frame %p \
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
      switch (data->GetType()) {
        case layers::WebRenderUserData::UserDataType::eFallback:
          if (!IsRenderNoImages(data->GetDisplayItemKey())) {
            static_cast<layers::WebRenderFallbackData*>(data.get())
                ->SetInvalid(true);
          }
          // XXX: handle Blob data
          invalidateFrame = true;
          break;
        case layers::WebRenderUserData::UserDataType::eImage:
          if (static_cast<layers::WebRenderImageData*>(data.get())
                  ->UsingSharedSurface(aRequest->GetProducerId())) {
            break;
          }
          [[fallthrough]];
        default:
          invalidateFrame = true;
          break;
      }
    }
  }

  // Update ancestor rendering observers (-moz-element etc)
  //
  // NOTE: We need to do this even if invalidateFrame is false, see bug 1114526.
  {
    nsIFrame* f = aFrame;
    while (f && !f->HasAnyStateBits(NS_FRAME_DESCENDANT_NEEDS_PAINT)) {
      SVGObserverUtils::InvalidateDirectRenderingObservers(f);
      f = nsLayoutUtils::GetCrossDocParentFrame(f);
    }
  }

  if (invalidateFrame) {
    aFrame->SchedulePaint();
  }
}

void ImageLoader::RequestPaintIfNeeded(FrameSet* aFrameSet,
                                       imgIRequest* aRequest,
                                       bool aForcePaint) {
  NS_ASSERTION(aFrameSet, "Must have a frame set");
  NS_ASSERTION(mDocument, "Should have returned earlier!");

  for (FrameWithFlags& fwf : *aFrameSet) {
    InvalidateImages(fwf.mFrame, aRequest, aForcePaint);
  }
}

void ImageLoader::UnblockOnloadIfNeeded(nsIFrame* aFrame,
                                        imgIRequest* aRequest) {
  MOZ_ASSERT(aFrame);
  MOZ_ASSERT(aRequest);

  FrameSet* frameSet = mRequestToFrameMap.Get(aRequest);
  if (!frameSet) {
    return;
  }

  size_t i =
      frameSet->BinaryIndexOf(FrameWithFlags(aFrame), FrameOnlyComparator());
  if (i != FrameSet::NoIndex) {
    FrameWithFlags& fwf = frameSet->ElementAt(i);
    if (fwf.mFlags & REQUEST_HAS_BLOCKED_ONLOAD) {
      mDocument->UnblockOnload(false);
      fwf.mFlags &= ~REQUEST_HAS_BLOCKED_ONLOAD;
    }
  }
}

void ImageLoader::RequestReflowIfNeeded(FrameSet* aFrameSet,
                                        imgIRequest* aRequest) {
  MOZ_ASSERT(aFrameSet);

  for (FrameWithFlags& fwf : *aFrameSet) {
    if (fwf.mFlags & REQUEST_REQUIRES_REFLOW) {
      // Tell the container of the frame to reflow because the
      // image request has finished decoding its first frame.
      RequestReflowOnFrame(&fwf, aRequest);
    }
  }
}

void ImageLoader::RequestReflowOnFrame(FrameWithFlags* aFwf,
                                       imgIRequest* aRequest) {
  nsIFrame* frame = aFwf->mFrame;

  // Actually request the reflow.
  //
  // FIXME(emilio): Why requesting reflow on the _parent_?
  nsIFrame* parent = frame->GetInFlowParent();
  parent->PresShell()->FrameNeedsReflow(parent, IntrinsicDirty::StyleChange,
                                        NS_FRAME_IS_DIRTY);

  // We'll respond to the reflow events by unblocking onload, regardless
  // of whether the reflow was completed or cancelled. The callback will
  // also delete itself when it is called.
  auto* unblocker = new ImageReflowCallback(this, frame, aRequest);
  parent->PresShell()->PostReflowCallback(unblocker);
}

void GlobalImageObserver::Notify(imgIRequest* aRequest, int32_t aType,
                                 const nsIntRect* aData) {
  auto entry = sImages->Lookup(aRequest);
  MOZ_DIAGNOSTIC_ASSERT(entry);
  if (MOZ_UNLIKELY(!entry)) {
    return;
  }

  auto& loaders = entry.Data()->mImageLoaders;
  nsTArray<RefPtr<ImageLoader>> loadersToNotify(loaders.Count());
  for (auto iter = loaders.Iter(); !iter.Done(); iter.Next()) {
    loadersToNotify.AppendElement(iter.Get()->GetKey());
  }
  for (auto& loader : loadersToNotify) {
    loader->Notify(aRequest, aType, aData);
  }
}

void ImageLoader::Notify(imgIRequest* aRequest, int32_t aType,
                         const nsIntRect* aData) {
#ifdef MOZ_GECKO_PROFILER
  nsCString uriString;
  if (profiler_is_active()) {
    nsCOMPtr<nsIURI> uri;
    aRequest->GetFinalURI(getter_AddRefs(uri));
    if (uri) {
      uri->GetSpec(uriString);
    }
  }

  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING("ImageLoader::Notify", OTHER,
                                        uriString);
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
}

void ImageLoader::OnSizeAvailable(imgIRequest* aRequest,
                                  imgIContainer* aImage) {
  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return;
  }

  aImage->SetAnimationMode(presContext->ImageAnimationMode());

  FrameSet* frameSet = mRequestToFrameMap.Get(aRequest);
  if (!frameSet) {
    return;
  }

  for (const FrameWithFlags& fwf : *frameSet) {
    if (fwf.mFrame->StyleVisibility()->IsVisible()) {
      fwf.mFrame->SchedulePaint();
    }
  }
}

void ImageLoader::OnImageIsAnimated(imgIRequest* aRequest) {
  if (!mDocument) {
    return;
  }

  FrameSet* frameSet = mRequestToFrameMap.Get(aRequest);
  if (!frameSet) {
    return;
  }

  // Register with the refresh driver now that we are aware that
  // we are animated.
  nsPresContext* presContext = GetPresContext();
  if (presContext) {
    nsLayoutUtils::RegisterImageRequest(presContext, aRequest, nullptr);
  }
}

void ImageLoader::OnFrameComplete(imgIRequest* aRequest) {
  if (!mDocument) {
    return;
  }

  FrameSet* frameSet = mRequestToFrameMap.Get(aRequest);
  if (!frameSet) {
    return;
  }

  // We may need reflow (for example if the image is from shape-outside).
  RequestReflowIfNeeded(frameSet, aRequest);

  // Since we just finished decoding a frame, we always want to paint, in case
  // we're now able to paint an image that we couldn't paint before (and hence
  // that we don't have retained data for).
  RequestPaintIfNeeded(frameSet, aRequest, /* aForcePaint = */ true);
}

void ImageLoader::OnFrameUpdate(imgIRequest* aRequest) {
  if (!mDocument) {
    return;
  }

  FrameSet* frameSet = mRequestToFrameMap.Get(aRequest);
  if (!frameSet) {
    return;
  }

  RequestPaintIfNeeded(frameSet, aRequest, /* aForcePaint = */ false);
}

void ImageLoader::OnLoadComplete(imgIRequest* aRequest) {
  if (!mDocument) {
    return;
  }

  FrameSet* frameSet = mRequestToFrameMap.Get(aRequest);
  if (!frameSet) {
    return;
  }

  // Check if aRequest has an error state. If it does, we need to unblock
  // Document onload for all the frames associated with this request that
  // have blocked onload. This is what happens in a CORS mode violation, and
  // may happen during other network events.
  uint32_t status = 0;
  if (NS_SUCCEEDED(aRequest->GetImageStatus(&status)) &&
      status & imgIRequest::STATUS_ERROR) {
    for (FrameWithFlags& fwf : *frameSet) {
      if (fwf.mFlags & REQUEST_HAS_BLOCKED_ONLOAD) {
        // We've blocked onload. Unblock onload and clear the flag.
        mDocument->UnblockOnload(false);
        fwf.mFlags &= ~REQUEST_HAS_BLOCKED_ONLOAD;
      }
    }
  }
}

bool ImageLoader::ImageReflowCallback::ReflowFinished() {
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

void ImageLoader::ImageReflowCallback::ReflowCallbackCanceled() {
  // Check that the frame is still valid. If it isn't, then onload was
  // unblocked when the frame was removed from the FrameSet in
  // RemoveRequestToFrameMapping.
  if (mFrame.IsAlive()) {
    mLoader->UnblockOnloadIfNeeded(mFrame, mRequest);
  }

  // Get rid of this callback object.
  delete this;
}

}  // namespace css
}  // namespace mozilla
