/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceSurfaceBlobImage.h"
#include "AutoRestoreSVGState.h"
#include "ImageRegion.h"
#include "SVGDocumentWrapper.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/WebRenderDrawEventRecorder.h"

using namespace mozilla::gfx;
using namespace mozilla::layers;

namespace mozilla::image {

SourceSurfaceBlobImage::SourceSurfaceBlobImage(
    image::SVGDocumentWrapper* aSVGDocumentWrapper,
    const Maybe<SVGImageContext>& aSVGContext,
    const Maybe<ImageIntRegion>& aRegion, const IntSize& aSize,
    uint32_t aWhichFrame, uint32_t aImageFlags)
    : mSVGDocumentWrapper(aSVGDocumentWrapper),
      mSVGContext(aSVGContext),
      mRegion(aRegion),
      mSize(aSize),
      mWhichFrame(aWhichFrame),
      mImageFlags(aImageFlags) {
  MOZ_ASSERT(mSVGDocumentWrapper);
  MOZ_ASSERT(aWhichFrame <= imgIContainer::FRAME_MAX_VALUE);
  MOZ_ASSERT(aImageFlags & imgIContainer::FLAG_RECORD_BLOB);
}

SourceSurfaceBlobImage::~SourceSurfaceBlobImage() {
  if (NS_IsMainThread()) {
    DestroyKeys(mKeys);
    return;
  }

  NS_ReleaseOnMainThread("SourceSurfaceBlobImage::mSVGDocumentWrapper",
                         mSVGDocumentWrapper.forget());
  NS_DispatchToMainThread(
      NS_NewRunnableFunction("SourceSurfaceBlobImage::DestroyKeys",
                             [keys = std::move(mKeys)] { DestroyKeys(keys); }));
}

/* static */ void SourceSurfaceBlobImage::DestroyKeys(
    const AutoTArray<BlobImageKeyData, 1>& aKeys) {
  for (const auto& entry : aKeys) {
    if (!entry.mManager->IsDestroyed()) {
      entry.mManager->GetRenderRootStateManager()->AddBlobImageKeyForDiscard(
          entry.mBlobKey);
    }
  }
}

Maybe<wr::BlobImageKey> SourceSurfaceBlobImage::UpdateKey(
    WebRenderLayerManager* aManager, wr::IpcResourceUpdateQueue& aResources) {
  MOZ_ASSERT(NS_IsMainThread());

  Maybe<wr::BlobImageKey> key;
  auto i = mKeys.Length();
  while (i > 0) {
    --i;
    BlobImageKeyData& entry = mKeys[i];
    if (entry.mManager->IsDestroyed()) {
      mKeys.RemoveElementAt(i);
    } else if (entry.mManager == aManager) {
      WebRenderBridgeChild* wrBridge = aManager->WrBridge();
      MOZ_ASSERT(wrBridge);

      bool ownsKey = wrBridge->MatchesNamespace(entry.mBlobKey);
      if (ownsKey && !entry.mDirty) {
        key.emplace(entry.mBlobKey);
        continue;
      }

      // Even if the manager is the same, its underlying WebRenderBridgeChild
      // can change state. Either our namespace differs, and our old key has
      // already been discarded, or the blob has changed. Either way, we need
      // to rerecord it.
      auto newEntry = RecordDrawing(aManager, aResources,
                                    ownsKey ? Some(entry.mBlobKey) : Nothing());
      if (!newEntry) {
        if (ownsKey) {
          aManager->GetRenderRootStateManager()->AddBlobImageKeyForDiscard(
              entry.mBlobKey);
        }
        mKeys.RemoveElementAt(i);
        continue;
      }

      key.emplace(newEntry.ref().mBlobKey);
      entry = std::move(newEntry.ref());
      MOZ_ASSERT(!entry.mDirty);
    }
  }

  // We didn't find an entry. Attempt to record the blob with a new key.
  if (!key) {
    auto newEntry = RecordDrawing(aManager, aResources, Nothing());
    if (newEntry) {
      key.emplace(newEntry.ref().mBlobKey);
      mKeys.AppendElement(std::move(newEntry.ref()));
    }
  }

  return key;
}

void SourceSurfaceBlobImage::MarkDirty() {
  MOZ_ASSERT(NS_IsMainThread());

  auto i = mKeys.Length();
  while (i > 0) {
    --i;
    BlobImageKeyData& entry = mKeys[i];
    if (entry.mManager->IsDestroyed()) {
      mKeys.RemoveElementAt(i);
    } else {
      entry.mDirty = true;
    }
  }
}

Maybe<BlobImageKeyData> SourceSurfaceBlobImage::RecordDrawing(
    WebRenderLayerManager* aManager, wr::IpcResourceUpdateQueue& aResources,
    Maybe<wr::BlobImageKey> aBlobKey) {
  MOZ_ASSERT(!aManager->IsDestroyed());

  if (mSVGDocumentWrapper->IsDrawing()) {
    return Nothing();
  }

  // This is either our first pass, or we have a stale key requiring us to
  // re-record the SVG image draw commands.
  auto* rootManager = aManager->GetRenderRootStateManager();
  auto* wrBridge = aManager->WrBridge();

  IntRect imageRect =
      mRegion ? mRegion->Rect() : IntRect(IntPoint(0, 0), mSize);
  IntRect imageRectOrigin = imageRect - imageRect.TopLeft();

  std::vector<RefPtr<ScaledFont>> fonts;
  bool validFonts = true;
  RefPtr<WebRenderDrawEventRecorder> recorder =
      MakeAndAddRef<WebRenderDrawEventRecorder>(
          [&](MemStream& aStream,
              std::vector<RefPtr<ScaledFont>>& aScaledFonts) {
            auto count = aScaledFonts.size();
            aStream.write((const char*)&count, sizeof(count));

            for (auto& scaled : aScaledFonts) {
              Maybe<wr::FontInstanceKey> key =
                  wrBridge->GetFontKeyForScaledFont(scaled, &aResources);
              if (key.isNothing()) {
                validFonts = false;
                break;
              }
              BlobFont font = {key.value(), scaled};
              aStream.write((const char*)&font, sizeof(font));
            }

            fonts = std::move(aScaledFonts);
          });

  RefPtr<DrawTarget> dummyDt = Factory::CreateDrawTarget(
      BackendType::SKIA, IntSize(1, 1), SurfaceFormat::OS_RGBA);
  RefPtr<DrawTarget> dt =
      Factory::CreateRecordingDrawTarget(recorder, dummyDt, imageRectOrigin);

  if (!dt || !dt->IsValid()) {
    return Nothing();
  }

  bool contextPaint = mSVGContext && mSVGContext->GetContextPaint();

  float animTime = (mWhichFrame == imgIContainer::FRAME_FIRST)
                       ? 0.0f
                       : mSVGDocumentWrapper->GetCurrentTimeAsFloat();

  IntSize viewportSize = mSize;
  if (mSVGContext) {
    auto cssViewportSize = mSVGContext->GetViewportSize();
    if (cssViewportSize) {
      // XXX losing unit
      viewportSize.SizeTo(cssViewportSize->width, cssViewportSize->height);
    }
  }

  {
    // Get (& sanity-check) the helper-doc's presShell
    RefPtr<PresShell> presShell = mSVGDocumentWrapper->GetPresShell();
    MOZ_ASSERT(presShell, "GetPresShell returned null for an SVG image?");

    nsPresContext* presContext = presShell->GetPresContext();
    MOZ_ASSERT(presContext, "pres shell w/out pres context");

    auto* doc = presShell->GetDocument();
    [[maybe_unused]] nsIURI* uri = doc ? doc->GetDocumentURI() : nullptr;
    AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING(
        "SVG Image recording", GRAPHICS,
        nsPrintfCString("(%d,%d) %dx%d from %dx%d %s", imageRect.x, imageRect.y,
                        imageRect.width, imageRect.height, mSize.width,
                        mSize.height,
                        uri ? uri->GetSpecOrDefault().get() : "N/A"));

    AutoRestoreSVGState autoRestore(mSVGContext, animTime, mSVGDocumentWrapper,
                                    contextPaint);

    mSVGDocumentWrapper->UpdateViewportBounds(viewportSize);
    mSVGDocumentWrapper->FlushImageTransformInvalidation();

    RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(dt);
    MOZ_ASSERT(ctx);  // Already checked the draw target above.

    nsRect svgRect;
    auto auPerDevPixel = presContext->AppUnitsPerDevPixel();
    if (mSize != viewportSize) {
      auto scaleX = double(mSize.width) / viewportSize.width;
      auto scaleY = double(mSize.height) / viewportSize.height;
      ctx->SetMatrix(Matrix::Scaling(float(scaleX), float(scaleY)));

      auto scaledVisibleRect = IntRectToRect(imageRect);
      scaledVisibleRect.Scale(float(auPerDevPixel / scaleX),
                              float(auPerDevPixel / scaleY));
      scaledVisibleRect.Round();
      svgRect.SetRect(
          int32_t(scaledVisibleRect.x), int32_t(scaledVisibleRect.y),
          int32_t(scaledVisibleRect.width), int32_t(scaledVisibleRect.height));
    } else {
      auto scaledVisibleRect(imageRect);
      scaledVisibleRect.Scale(auPerDevPixel);
      svgRect.SetRect(scaledVisibleRect.x, scaledVisibleRect.y,
                      scaledVisibleRect.width, scaledVisibleRect.height);
    }

    RenderDocumentFlags renderDocFlags =
        RenderDocumentFlags::IgnoreViewportScrolling;
    if (!(mImageFlags & imgIContainer::FLAG_SYNC_DECODE)) {
      renderDocFlags |= RenderDocumentFlags::AsyncDecodeImages;
    }
    if (mImageFlags & imgIContainer::FLAG_HIGH_QUALITY_SCALING) {
      renderDocFlags |= RenderDocumentFlags::UseHighQualityScaling;
    }

    presShell->RenderDocument(svgRect, renderDocFlags,
                              NS_RGBA(0, 0, 0, 0),  // transparent
                              ctx);
  }

  recorder->FlushItem(imageRectOrigin);
  recorder->Finish();

  if (!validFonts) {
    gfxCriticalNote << "Failed serializing fonts for blob vector image";
    return Nothing();
  }

  Range<uint8_t> bytes((uint8_t*)recorder->mOutputStream.mData,
                       recorder->mOutputStream.mLength);
  wr::BlobImageKey key = aBlobKey
                             ? aBlobKey.value()
                             : wr::BlobImageKey{wrBridge->GetNextImageKey()};
  wr::ImageDescriptor descriptor(imageRect.Size(), 0, SurfaceFormat::OS_RGBA,
                                 wr::OpacityType::HasAlphaChannel);

  auto visibleRect = ImageIntRect::FromUnknownRect(imageRectOrigin);
  if (aBlobKey) {
    if (!aResources.UpdateBlobImage(key, descriptor, bytes, visibleRect,
                                    visibleRect)) {
      return Nothing();
    }
  } else if (!aResources.AddBlobImage(key, descriptor, bytes, visibleRect)) {
    return Nothing();
  }

  std::vector<RefPtr<SourceSurface>> externalSurfaces;
  recorder->TakeExternalSurfaces(externalSurfaces);

  for (auto& surface : externalSurfaces) {
    // While we don't use the image key with the surface, because the blob image
    // renderer doesn't have easy access to the resource set, we still want to
    // ensure one is generated. That will ensure the surface remains alive until
    // at least the last epoch which the blob image could be used in.
    wr::ImageKey key = {};
    DebugOnly<nsresult> rv =
        SharedSurfacesChild::Share(surface, rootManager, aResources, key);
    MOZ_ASSERT(rv.value != NS_ERROR_NOT_IMPLEMENTED);
  }

  return Some(BlobImageKeyData(aManager, key, std::move(fonts),
                               std::move(externalSurfaces)));
}

}  // namespace mozilla::image
