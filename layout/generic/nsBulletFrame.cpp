/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for list-item bullets */

#include "nsBulletFrame.h"

#include <algorithm>
#include <utility>

#include "CounterStyleManager.h"
#include "ImageLayers.h"
#include "SVGImageContext.h"
#include "TextDrawTarget.h"
#include "UnitTransforms.h"
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxUtils.h"
#include "imgIContainer.h"
#include "imgRequestProxy.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/layers/LayersMessages.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/WebRenderMessages.h"
#include "nsAttrValueInlines.h"
#include "nsBidiUtils.h"
#include "nsCOMPtr.h"
#include "nsCSSFrameConstructor.h"
#include "nsCounterManager.h"
#include "nsDisplayList.h"
#include "nsFontMetrics.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsIURI.h"
#include "nsPresContext.h"

#ifdef ACCESSIBILITY
#  include "nsAccessibilityService.h"
#endif

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;
using namespace mozilla::layout;
using mozilla::dom::Document;

nsIFrame* NS_NewBulletFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsBulletFrame(aStyle, aPresShell->GetPresContext());
}

NS_DECLARE_FRAME_PROPERTY_SMALL_VALUE(FontSizeInflationProperty, float)

NS_IMPL_FRAMEARENA_HELPERS(nsBulletFrame)

#ifdef DEBUG
NS_QUERYFRAME_HEAD(nsBulletFrame)
  NS_QUERYFRAME_ENTRY(nsBulletFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsFrame)
#endif

nsBulletFrame::~nsBulletFrame() {}

CounterStyle* nsBulletFrame::ResolveCounterStyle() {
  return PresContext()->CounterStyleManager()->ResolveCounterStyle(
      StyleList()->mCounterStyle);
}

void nsBulletFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                PostDestroyData& aPostDestroyData) {
  // Stop image loading first.
  DeregisterAndCancelImageRequest();

  if (mListener) {
    mListener->SetFrame(nullptr);
  }

  // Let base class do the rest
  nsFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsBulletFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(NS_LITERAL_STRING("Bullet"), aResult);
}
#endif

bool nsBulletFrame::IsEmpty() { return IsSelfEmpty(); }

bool nsBulletFrame::IsSelfEmpty() {
  return StyleList()->mCounterStyle.IsNone();
}

/* virtual */
void nsBulletFrame::DidSetComputedStyle(ComputedStyle* aOldComputedStyle) {
  nsFrame::DidSetComputedStyle(aOldComputedStyle);

  imgRequestProxy* newRequest = StyleList()->GetListStyleImage();

  if (newRequest) {
    if (!mListener) {
      mListener = new nsBulletListener();
      mListener->SetFrame(this);
    }

    bool needNewRequest = true;

    if (mImageRequest) {
      // Reload the image, maybe...
      nsCOMPtr<nsIURI> oldURI;
      mImageRequest->GetURI(getter_AddRefs(oldURI));
      nsCOMPtr<nsIURI> newURI;
      newRequest->GetURI(getter_AddRefs(newURI));
      if (oldURI && newURI) {
        bool same;
        newURI->Equals(oldURI, &same);
        if (same) {
          needNewRequest = false;
        }
      }
    }

    if (needNewRequest) {
      RefPtr<imgRequestProxy> newRequestClone;
      newRequest->SyncClone(mListener, PresContext()->Document(),
                            getter_AddRefs(newRequestClone));

      // Deregister the old request. We wait until after Clone is done in case
      // the old request and the new request are the same underlying image
      // accessed via different URLs.
      DeregisterAndCancelImageRequest();

      // Register the new request.
      mImageRequest = std::move(newRequestClone);
      RegisterImageRequest(/* aKnownToBeAnimated = */ false);

      // Image bullets can affect the layout of the page, so boost the image
      // load priority.
      mImageRequest->BoostPriority(imgIRequest::CATEGORY_SIZE_QUERY);
    }
  } else {
    // No image request on the new ComputedStyle.
    DeregisterAndCancelImageRequest();
  }

#ifdef ACCESSIBILITY
  // Update the list bullet accessible. If old style list isn't available then
  // no need to update the accessible tree because it's not created yet.
  if (aOldComputedStyle) {
    if (nsAccessibilityService* accService =
            PresShell::GetAccessibilityService()) {
      const nsStyleList* oldStyleList = aOldComputedStyle->StyleList();
      bool hadBullet = oldStyleList->GetListStyleImage() ||
                       !oldStyleList->mCounterStyle.IsNone();

      const nsStyleList* newStyleList = StyleList();
      bool hasBullet = newStyleList->GetListStyleImage() ||
                       !newStyleList->mCounterStyle.IsNone();

      if (hadBullet != hasBullet) {
        nsIContent* listItem = mContent->GetParent();
        accService->UpdateListBullet(PresContext()->GetPresShell(), listItem,
                                     hasBullet);
      }
    }
  }
#endif  // #ifdef ACCESSIBILITY
}

class nsDisplayBulletGeometry
    : public nsDisplayItemGenericGeometry,
      public nsImageGeometryMixin<nsDisplayBulletGeometry> {
 public:
  nsDisplayBulletGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
      : nsDisplayItemGenericGeometry(aItem, aBuilder),
        nsImageGeometryMixin(aItem, aBuilder) {
    nsBulletFrame* f = static_cast<nsBulletFrame*>(aItem->Frame());
    mOrdinal = f->Ordinal();
  }

  virtual bool InvalidateForSyncDecodeImages() const override {
    return ShouldInvalidateToSyncDecodeImages();
  }

  int32_t mOrdinal;
};

class BulletRenderer final {
 public:
  BulletRenderer(imgIContainer* image, const nsRect& dest)
      : mImage(image),
        mDest(dest),
        mColor(NS_RGBA(0, 0, 0, 0)),
        mListStyleType(NS_STYLE_LIST_STYLE_NONE) {
    MOZ_ASSERT(IsImageType());
  }

  BulletRenderer(Path* path, nscolor color, int32_t listStyleType)
      : mColor(color), mPath(path), mListStyleType(listStyleType) {
    MOZ_ASSERT(IsPathType());
  }

  BulletRenderer(const LayoutDeviceRect& aPathRect, nscolor color,
                 int32_t listStyleType)
      : mPathRect(aPathRect), mColor(color), mListStyleType(listStyleType) {
    MOZ_ASSERT(IsPathType());
  }

  BulletRenderer(const nsString& text, nsFontMetrics* fm, nscolor color,
                 const nsPoint& point, int32_t listStyleType)
      : mColor(color),
        mText(text),
        mFontMetrics(fm),
        mPoint(point),
        mListStyleType(listStyleType) {
    MOZ_ASSERT(IsTextType());
  }

  ImgDrawResult CreateWebRenderCommands(
      nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
      wr::IpcResourceUpdateQueue& aResources,
      const layers::StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder);

  ImgDrawResult Paint(gfxContext& aRenderingContext, nsPoint aPt,
                      const nsRect& aDirtyRect, uint32_t aFlags,
                      bool aDisableSubpixelAA, nsIFrame* aFrame);

  bool IsImageType() const {
    return mListStyleType == NS_STYLE_LIST_STYLE_NONE && mImage;
  }

  bool IsPathType() const {
    return mListStyleType == NS_STYLE_LIST_STYLE_DISC ||
           mListStyleType == NS_STYLE_LIST_STYLE_CIRCLE ||
           mListStyleType == NS_STYLE_LIST_STYLE_SQUARE ||
           mListStyleType == NS_STYLE_LIST_STYLE_DISCLOSURE_OPEN ||
           mListStyleType == NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED;
  }

  bool IsTextType() const {
    return mListStyleType != NS_STYLE_LIST_STYLE_NONE &&
           mListStyleType != NS_STYLE_LIST_STYLE_DISC &&
           mListStyleType != NS_STYLE_LIST_STYLE_CIRCLE &&
           mListStyleType != NS_STYLE_LIST_STYLE_SQUARE &&
           mListStyleType != NS_STYLE_LIST_STYLE_DISCLOSURE_OPEN &&
           mListStyleType != NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED &&
           !mText.IsEmpty();
  }

  void PaintTextToContext(nsIFrame* aFrame, gfxContext* aCtx,
                          bool aDisableSubpixelAA);

  bool IsImageContainerAvailable(layers::LayerManager* aManager,
                                 uint32_t aFlags);

 private:
  ImgDrawResult CreateWebRenderCommandsForImage(
      nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
      wr::IpcResourceUpdateQueue& aResources,
      const layers::StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder);

  bool CreateWebRenderCommandsForPath(
      nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
      wr::IpcResourceUpdateQueue& aResources,
      const layers::StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder);

  bool CreateWebRenderCommandsForText(
      nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
      wr::IpcResourceUpdateQueue& aResources,
      const layers::StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder);

 private:
  // mImage and mDest are the properties for list-style-image.
  // mImage is the image content and mDest is the image position.
  RefPtr<imgIContainer> mImage;
  nsRect mDest;

  // Some bullet types are stored as a rect (in device pixels) instead of a Path
  // to allow generating proper WebRender commands. When webrender is disabled
  // the Path is lazily created for these items before painting.
  // TODO: The size of this structure doesn't seem to be an issue since it has
  // so many fields that are specific to a bullet style or another, but if it
  // becomes one we can easily store mDest and mPathRect into the same memory
  // location since they are never used by the same bullet types.
  LayoutDeviceRect mPathRect;

  // mColor indicate the color of list-style. Both text and path type would use
  // this member.
  nscolor mColor;

  // mPath record the path of the list-style for later drawing.
  // Included following types: square, circle, disc, disclosure open and
  // disclosure closed.
  RefPtr<Path> mPath;

  // mText, mFontMertrics, mPoint, mFont and mGlyphs are for other
  // list-style-type which can be drawed by text.
  nsString mText;
  RefPtr<nsFontMetrics> mFontMetrics;
  nsPoint mPoint;
  RefPtr<ScaledFont> mFont;
  nsTArray<layers::GlyphArray> mGlyphs;

  // Store the type of list-style-type.
  int32_t mListStyleType;
};

ImgDrawResult BulletRenderer::CreateWebRenderCommands(
    nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResources,
    const layers::StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  if (IsImageType()) {
    return CreateWebRenderCommandsForImage(aItem, aBuilder, aResources, aSc,
                                           aManager, aDisplayListBuilder);
  }

  bool success;
  if (IsPathType()) {
    success = CreateWebRenderCommandsForPath(aItem, aBuilder, aResources, aSc,
                                             aManager, aDisplayListBuilder);
  } else {
    MOZ_ASSERT(IsTextType());
    success = CreateWebRenderCommandsForText(aItem, aBuilder, aResources, aSc,
                                             aManager, aDisplayListBuilder);
  }

  return success ? ImgDrawResult::SUCCESS : ImgDrawResult::NOT_SUPPORTED;
}

ImgDrawResult BulletRenderer::Paint(gfxContext& aRenderingContext, nsPoint aPt,
                                    const nsRect& aDirtyRect, uint32_t aFlags,
                                    bool aDisableSubpixelAA, nsIFrame* aFrame) {
  if (IsImageType()) {
    SamplingFilter filter = nsLayoutUtils::GetSamplingFilterForFrame(aFrame);
    return nsLayoutUtils::DrawSingleImage(
        aRenderingContext, aFrame->PresContext(), mImage, filter, mDest,
        aDirtyRect,
        /* no SVGImageContext */ Nothing(), aFlags);
  }

  if (IsPathType()) {
    DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();

    if (!mPath) {
      RefPtr<PathBuilder> builder = drawTarget->CreatePathBuilder();
      switch (mListStyleType) {
        case NS_STYLE_LIST_STYLE_CIRCLE:
        case NS_STYLE_LIST_STYLE_DISC:
          AppendEllipseToPath(builder, mPathRect.Center().ToUnknownPoint(),
                              mPathRect.Size().ToUnknownSize());
          break;
        case NS_STYLE_LIST_STYLE_SQUARE:
          AppendRectToPath(builder, mPathRect.ToUnknownRect());
          break;
        default:
          MOZ_ASSERT(false, "Should have a parth.");
      }
      mPath = builder->Finish();
    }

    switch (mListStyleType) {
      case NS_STYLE_LIST_STYLE_CIRCLE:
        drawTarget->Stroke(mPath, ColorPattern(ToDeviceColor(mColor)));
        break;
      case NS_STYLE_LIST_STYLE_DISC:
      case NS_STYLE_LIST_STYLE_SQUARE:
      case NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED:
      case NS_STYLE_LIST_STYLE_DISCLOSURE_OPEN:
        drawTarget->Fill(mPath, ColorPattern(ToDeviceColor(mColor)));
        break;
      default:
        MOZ_CRASH("unreachable");
    }
  }

  if (IsTextType()) {
    PaintTextToContext(aFrame, &aRenderingContext, aDisableSubpixelAA);
  }

  return ImgDrawResult::SUCCESS;
}

void BulletRenderer::PaintTextToContext(nsIFrame* aFrame, gfxContext* aCtx,
                                        bool aDisableSubpixelAA) {
  MOZ_ASSERT(IsTextType());

  DrawTarget* drawTarget = aCtx->GetDrawTarget();
  DrawTargetAutoDisableSubpixelAntialiasing disable(drawTarget,
                                                    aDisableSubpixelAA);

  aCtx->SetColor(Color::FromABGR(mColor));

  nsPresContext* presContext = aFrame->PresContext();
  if (!presContext->BidiEnabled() && HasRTLChars(mText)) {
    presContext->SetBidiEnabled();
  }
  nsLayoutUtils::DrawString(aFrame, *mFontMetrics, aCtx, mText.get(),
                            mText.Length(), mPoint);
}

bool BulletRenderer::IsImageContainerAvailable(layers::LayerManager* aManager,
                                               uint32_t aFlags) {
  MOZ_ASSERT(IsImageType());

  return mImage->IsImageContainerAvailable(aManager, aFlags);
}

ImgDrawResult BulletRenderer::CreateWebRenderCommandsForImage(
    nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResources,
    const layers::StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  MOZ_RELEASE_ASSERT(IsImageType());
  MOZ_RELEASE_ASSERT(mImage);

  uint32_t flags = imgIContainer::FLAG_ASYNC_NOTIFY;
  if (aDisplayListBuilder->IsPaintingToWindow()) {
    flags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;
  }
  if (aDisplayListBuilder->ShouldSyncDecodeImages()) {
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  }

  const int32_t appUnitsPerDevPixel =
      aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
  LayoutDeviceRect destRect =
      LayoutDeviceRect::FromAppUnits(mDest, appUnitsPerDevPixel);

  Maybe<SVGImageContext> svgContext;
  gfx::IntSize decodeSize =
      nsLayoutUtils::ComputeImageContainerDrawingParameters(
          mImage, aItem->Frame(), destRect, aSc, flags, svgContext);

  RefPtr<layers::ImageContainer> container;
  ImgDrawResult drawResult = mImage->GetImageContainerAtSize(
      aManager->LayerManager(), decodeSize, svgContext, flags,
      getter_AddRefs(container));
  if (!container) {
    return drawResult;
  }

  mozilla::wr::ImageRendering rendering = wr::ToImageRendering(
      nsLayoutUtils::GetSamplingFilterForFrame(aItem->Frame()));
  gfx::IntSize size;
  Maybe<wr::ImageKey> key = aManager->CommandBuilder().CreateImageKey(
      aItem, container, aBuilder, aResources, rendering, aSc, size, Nothing());
  if (key.isNothing()) {
    return drawResult;
  }

  wr::LayoutRect dest = wr::ToLayoutRect(destRect);

  aBuilder.PushImage(dest, dest, !aItem->BackfaceIsHidden(), rendering,
                     key.value());

  return drawResult;
}

bool BulletRenderer::CreateWebRenderCommandsForPath(
    nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResources,
    const layers::StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  MOZ_ASSERT(IsPathType());
  wr::LayoutRect dest = wr::ToLayoutRect(mPathRect);
  auto color = wr::ToColorF(ToDeviceColor(mColor));
  bool isBackfaceVisible = !aItem->BackfaceIsHidden();
  switch (mListStyleType) {
    case NS_STYLE_LIST_STYLE_CIRCLE: {
      LayoutDeviceSize radii = mPathRect.Size() / 2.0;
      auto borderWidths = wr::ToBorderWidths(1.0, 1.0, 1.0, 1.0);
      wr::BorderSide side = {color, wr::BorderStyle::Solid};
      wr::BorderSide sides[4] = {side, side, side, side};
      Range<const wr::BorderSide> sidesRange(sides, 4);
      aBuilder.PushBorder(dest, dest, isBackfaceVisible, borderWidths,
                          sidesRange,
                          wr::ToBorderRadius(radii, radii, radii, radii));
      return true;
    }
    case NS_STYLE_LIST_STYLE_DISC: {
      aBuilder.PushRoundedRect(dest, dest, isBackfaceVisible, color);
      return true;
    }
    case NS_STYLE_LIST_STYLE_SQUARE: {
      aBuilder.PushRect(dest, dest, isBackfaceVisible, color);
      return true;
    }
    default:
      if (!aManager->CommandBuilder().PushItemAsImage(
              aItem, aBuilder, aResources, aSc, aDisplayListBuilder)) {
        NS_WARNING("Fail to create WebRender commands for Bullet path.");
        return false;
      }
  }

  return true;
}

bool BulletRenderer::CreateWebRenderCommandsForText(
    nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResources,
    const layers::StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  MOZ_ASSERT(IsTextType());

  bool dummy;
  nsRect bounds = aItem->GetBounds(aDisplayListBuilder, &dummy);

  if (bounds.IsEmpty()) {
    return true;
  }

  RefPtr<TextDrawTarget> textDrawer =
      new TextDrawTarget(aBuilder, aResources, aSc, aManager, aItem, bounds);
  RefPtr<gfxContext> captureCtx = gfxContext::CreateOrNull(textDrawer);
  PaintTextToContext(aItem->Frame(), captureCtx, aItem->IsSubpixelAADisabled());
  textDrawer->TerminateShadows();

  return textDrawer->Finish();
}

class nsDisplayBullet final : public nsPaintedDisplayItem {
 public:
  nsDisplayBullet(nsDisplayListBuilder* aBuilder, nsBulletFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayBullet);
  }
  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayBullet)

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override {
    *aSnap = false;
    return mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
  }

  virtual bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue&, const StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*>* aOutFrames) override {
    aOutFrames->AppendElement(mFrame);
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  NS_DISPLAY_DECL_NAME("Bullet", TYPE_BULLET)

  virtual nsRect GetComponentAlphaBounds(
      nsDisplayListBuilder* aBuilder) const override {
    bool snap;
    return GetBounds(aBuilder, &snap);
  }

  virtual nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override {
    return new nsDisplayBulletGeometry(this, aBuilder);
  }

  virtual void ComputeInvalidationRegion(
      nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
      nsRegion* aInvalidRegion) const override {
    const nsDisplayBulletGeometry* geometry =
        static_cast<const nsDisplayBulletGeometry*>(aGeometry);
    nsBulletFrame* f = static_cast<nsBulletFrame*>(mFrame);

    if (f->Ordinal() != geometry->mOrdinal) {
      bool snap;
      aInvalidRegion->Or(geometry->mBounds, GetBounds(aBuilder, &snap));
      return;
    }

    nsCOMPtr<imgIContainer> image = f->GetImage();
    if (aBuilder->ShouldSyncDecodeImages() && image &&
        geometry->ShouldInvalidateToSyncDecodeImages()) {
      bool snap;
      aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
    }

    return nsPaintedDisplayItem::ComputeInvalidationRegion(aBuilder, aGeometry,
                                                           aInvalidRegion);
  }

 protected:
  Maybe<BulletRenderer> mBulletRenderer;
};

bool nsDisplayBullet::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  // FIXME: avoid needing to make this target if we're drawing text
  // (non-trivial refactor of all this code)
  RefPtr<gfxContext> screenRefCtx = gfxContext::CreateOrNull(
      gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget().get());
  Maybe<BulletRenderer> br =
      static_cast<nsBulletFrame*>(mFrame)->CreateBulletRenderer(
          *screenRefCtx, ToReferenceFrame());

  if (!br) {
    return false;
  }

  ImgDrawResult drawResult = br->CreateWebRenderCommands(
      this, aBuilder, aResources, aSc, aManager, aDisplayListBuilder);
  if (drawResult == ImgDrawResult::NOT_SUPPORTED) {
    return false;
  }

  nsDisplayBulletGeometry::UpdateDrawResult(this, drawResult);
  return true;
}

void nsDisplayBullet::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  uint32_t flags = imgIContainer::FLAG_NONE;
  if (aBuilder->ShouldSyncDecodeImages()) {
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  }

  ImgDrawResult result = static_cast<nsBulletFrame*>(mFrame)->PaintBullet(
      *aCtx, ToReferenceFrame(), GetPaintRect(), flags, IsSubpixelAADisabled());

  nsDisplayBulletGeometry::UpdateDrawResult(this, result);
}

void nsBulletFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                     const nsDisplayListSet& aLists) {
  if (!IsVisibleForPainting()) return;

  DO_GLOBAL_REFLOW_COUNT_DSP("nsBulletFrame");

  aLists.Content()->AppendNewToTop<nsDisplayBullet>(aBuilder, this);
}

Maybe<BulletRenderer> nsBulletFrame::CreateBulletRenderer(
    gfxContext& aRenderingContext, nsPoint aPt) {
  const nsStyleList* myList = StyleList();
  CounterStyle* listStyleType = ResolveCounterStyle();
  nsMargin padding = mPadding.GetPhysicalMargin(GetWritingMode());

  if (myList->GetListStyleImage() && mImageRequest) {
    uint32_t status;
    mImageRequest->GetImageStatus(&status);
    if (!(status & imgIRequest::STATUS_ERROR)) {
      if (status & imgIRequest::STATUS_LOAD_COMPLETE) {
        nsCOMPtr<imgIContainer> imageCon;
        mImageRequest->GetImage(getter_AddRefs(imageCon));
        if (imageCon) {
          nsRect dest(padding.left, padding.top,
                      mRect.width - (padding.left + padding.right),
                      mRect.height - (padding.top + padding.bottom));
          BulletRenderer br(imageCon, dest + aPt);
          return Some(br);
        }
      } else {
        // Boost the load priority further now that we know we want to display
        // the bullet image.
        mImageRequest->BoostPriority(imgIRequest::CATEGORY_DISPLAY);
      }
    }
  }

  nscolor color = nsLayoutUtils::GetColor(this, &nsStyleText::mColor);

  DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();
  int32_t appUnitsPerDevPixel = PresContext()->AppUnitsPerDevPixel();

  switch (listStyleType->GetStyle()) {
    case NS_STYLE_LIST_STYLE_NONE:
      return Nothing();

    case NS_STYLE_LIST_STYLE_DISC:
    case NS_STYLE_LIST_STYLE_CIRCLE: {
      nsRect rect(padding.left + aPt.x, padding.top + aPt.y,
                  mRect.width - (padding.left + padding.right),
                  mRect.height - (padding.top + padding.bottom));
      auto devPxRect =
          LayoutDeviceRect::FromAppUnits(rect, appUnitsPerDevPixel);
      return Some(BulletRenderer(devPxRect, color, listStyleType->GetStyle()));
    }

    case NS_STYLE_LIST_STYLE_SQUARE: {
      nsRect rect(aPt, mRect.Size());
      rect.Deflate(padding);

      // Snap the height and the width of the rectangle to device pixels,
      // and then center the result within the original rectangle, so that
      // all square bullets at the same font size have the same visual
      // size (bug 376690).
      // FIXME: We should really only do this if we're not transformed
      // (like gfxContext::UserToDevicePixelSnapped does).
      nsPresContext* pc = PresContext();
      nsRect snapRect(rect.x, rect.y,
                      pc->RoundAppUnitsToNearestDevPixels(rect.width),
                      pc->RoundAppUnitsToNearestDevPixels(rect.height));
      snapRect.MoveBy((rect.width - snapRect.width) / 2,
                      (rect.height - snapRect.height) / 2);
      auto devPxRect =
          LayoutDeviceRect::FromAppUnits(snapRect, appUnitsPerDevPixel);
      return Some(BulletRenderer(devPxRect, color, listStyleType->GetStyle()));
    }

    case NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED:
    case NS_STYLE_LIST_STYLE_DISCLOSURE_OPEN: {
      nsRect rect(aPt, mRect.Size());
      rect.Deflate(padding);

      WritingMode wm = GetWritingMode();
      bool isVertical = wm.IsVertical();
      bool isClosed =
          listStyleType->GetStyle() == NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED;
      bool isDown = (!isVertical && !isClosed) || (isVertical && isClosed);
      nscoord diff = NSToCoordRound(0.1f * rect.height);
      if (isDown) {
        rect.y += diff * 2;
        rect.height -= diff * 2;
      } else {
        rect.Deflate(diff, 0);
      }
      nsPresContext* pc = PresContext();
      rect.x = pc->RoundAppUnitsToNearestDevPixels(rect.x);
      rect.y = pc->RoundAppUnitsToNearestDevPixels(rect.y);

      RefPtr<PathBuilder> builder = drawTarget->CreatePathBuilder();
      if (isDown) {
        // to bottom
        builder->MoveTo(NSPointToPoint(rect.TopLeft(), appUnitsPerDevPixel));
        builder->LineTo(NSPointToPoint(rect.TopRight(), appUnitsPerDevPixel));
        builder->LineTo(NSPointToPoint(
            (rect.BottomLeft() + rect.BottomRight()) / 2, appUnitsPerDevPixel));
      } else {
        if (wm.IsPhysicalLTR()) {
          // to right
          builder->MoveTo(NSPointToPoint(rect.TopLeft(), appUnitsPerDevPixel));
          builder->LineTo(NSPointToPoint(
              (rect.TopRight() + rect.BottomRight()) / 2, appUnitsPerDevPixel));
          builder->LineTo(
              NSPointToPoint(rect.BottomLeft(), appUnitsPerDevPixel));
        } else {
          // to left
          builder->MoveTo(NSPointToPoint(rect.TopRight(), appUnitsPerDevPixel));
          builder->LineTo(
              NSPointToPoint(rect.BottomRight(), appUnitsPerDevPixel));
          builder->LineTo(NSPointToPoint(
              (rect.TopLeft() + rect.BottomLeft()) / 2, appUnitsPerDevPixel));
        }
      }

      RefPtr<Path> path = builder->Finish();
      BulletRenderer br(path, color, listStyleType->GetStyle());
      return Some(br);
    }

    default: {
      RefPtr<nsFontMetrics> fm =
          nsLayoutUtils::GetFontMetricsForFrame(this, GetFontSizeInflation());
      nsAutoString text;
      WritingMode wm = GetWritingMode();
      GetListItemText(listStyleType, wm, Ordinal(), text);
      nscoord ascent = wm.IsLineInverted() ? fm->MaxDescent() : fm->MaxAscent();
      aPt.MoveBy(padding.left, padding.top);
      if (wm.IsVertical()) {
        if (wm.IsVerticalLR()) {
          aPt.x = NSToCoordRound(nsLayoutUtils::GetSnappedBaselineX(
              this, &aRenderingContext, aPt.x, ascent));
        } else {
          aPt.x = NSToCoordRound(nsLayoutUtils::GetSnappedBaselineX(
              this, &aRenderingContext, aPt.x + mRect.width, -ascent));
        }
      } else {
        aPt.y = NSToCoordRound(nsLayoutUtils::GetSnappedBaselineY(
            this, &aRenderingContext, aPt.y, ascent));
      }

      BulletRenderer br(text, fm, color, aPt, listStyleType->GetStyle());
      return Some(br);
    }
  }

  MOZ_CRASH("unreachable");
  return Nothing();
}

ImgDrawResult nsBulletFrame::PaintBullet(gfxContext& aRenderingContext,
                                         nsPoint aPt, const nsRect& aDirtyRect,
                                         uint32_t aFlags,
                                         bool aDisableSubpixelAA) {
  Maybe<BulletRenderer> br = CreateBulletRenderer(aRenderingContext, aPt);

  if (!br) {
    return ImgDrawResult::SUCCESS;
  }

  return br->Paint(aRenderingContext, aPt, aDirtyRect, aFlags,
                   aDisableSubpixelAA, this);
}

void nsBulletFrame::GetListItemText(CounterStyle* aStyle,
                                    mozilla::WritingMode aWritingMode,
                                    int32_t aOrdinal, nsAString& aResult) {
  NS_ASSERTION(
      aStyle->GetStyle() != NS_STYLE_LIST_STYLE_NONE &&
          aStyle->GetStyle() != NS_STYLE_LIST_STYLE_DISC &&
          aStyle->GetStyle() != NS_STYLE_LIST_STYLE_CIRCLE &&
          aStyle->GetStyle() != NS_STYLE_LIST_STYLE_SQUARE &&
          aStyle->GetStyle() != NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED &&
          aStyle->GetStyle() != NS_STYLE_LIST_STYLE_DISCLOSURE_OPEN,
      "we should be using specialized code for these types");

  bool isRTL;
  nsAutoString counter, prefix, suffix;
  aStyle->GetPrefix(prefix);
  aStyle->GetSuffix(suffix);
  aStyle->GetCounterText(aOrdinal, aWritingMode, counter, isRTL);

  aResult.Truncate();
  aResult.Append(prefix);
  if (aWritingMode.IsBidiRTL() == isRTL) {
    aResult.Append(counter);
  } else {
    // RLM = 0x200f, LRM = 0x200e
    char16_t mark = isRTL ? 0x200f : 0x200e;
    aResult.Append(mark);
    aResult.Append(counter);
    aResult.Append(mark);
  }
  aResult.Append(suffix);
}

#define MIN_BULLET_SIZE 1

void nsBulletFrame::AppendSpacingToPadding(nsFontMetrics* aFontMetrics,
                                           LogicalMargin* aPadding) {
  aPadding->IEnd(GetWritingMode()) += aFontMetrics->EmHeight() / 2;
}

void nsBulletFrame::GetDesiredSize(nsPresContext* aCX,
                                   gfxContext* aRenderingContext,
                                   ReflowOutput& aMetrics,
                                   float aFontSizeInflation,
                                   LogicalMargin* aPadding) {
  // Reset our padding.  If we need it, we'll set it below.
  WritingMode wm = GetWritingMode();
  aPadding->SizeTo(wm, 0, 0, 0, 0);
  LogicalSize finalSize(wm);

  const nsStyleList* myList = StyleList();
  nscoord ascent;
  RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetFontMetricsForFrame(this, aFontSizeInflation);

  RemoveStateBits(BULLET_FRAME_IMAGE_LOADING);

  if (myList->GetListStyleImage() && mImageRequest) {
    uint32_t status;
    mImageRequest->GetImageStatus(&status);
    if (status & imgIRequest::STATUS_SIZE_AVAILABLE &&
        !(status & imgIRequest::STATUS_ERROR)) {
      // auto size the image
      finalSize.ISize(wm) = mIntrinsicSize.ISize(wm);
      aMetrics.SetBlockStartAscent(finalSize.BSize(wm) =
                                       mIntrinsicSize.BSize(wm));
      aMetrics.SetSize(wm, finalSize);

      AppendSpacingToPadding(fm, aPadding);

      AddStateBits(BULLET_FRAME_IMAGE_LOADING);

      return;
    }
  }

  // If we're getting our desired size and don't have an image, reset
  // mIntrinsicSize to (0,0).  Otherwise, if we used to have an image, it
  // changed, and the new one is coming in, but we're reflowing before it's
  // fully there, we'll end up with mIntrinsicSize not matching our size, but
  // won't trigger a reflow in OnStartContainer (because mIntrinsicSize will
  // match the image size).
  mIntrinsicSize.SizeTo(wm, 0, 0);

  nscoord bulletSize;

  nsAutoString text;
  CounterStyle* style = ResolveCounterStyle();
  switch (style->GetStyle()) {
    case NS_STYLE_LIST_STYLE_NONE:
      finalSize.ISize(wm) = finalSize.BSize(wm) = 0;
      aMetrics.SetBlockStartAscent(0);
      break;

    case NS_STYLE_LIST_STYLE_DISC:
    case NS_STYLE_LIST_STYLE_CIRCLE:
    case NS_STYLE_LIST_STYLE_SQUARE: {
      ascent = fm->MaxAscent();
      bulletSize = std::max(nsPresContext::CSSPixelsToAppUnits(MIN_BULLET_SIZE),
                            NSToCoordRound(0.8f * (float(ascent) / 2.0f)));
      aPadding->BEnd(wm) = NSToCoordRound(float(ascent) / 8.0f);
      finalSize.ISize(wm) = finalSize.BSize(wm) = bulletSize;
      aMetrics.SetBlockStartAscent(bulletSize + aPadding->BEnd(wm));
      AppendSpacingToPadding(fm, aPadding);
      break;
    }

    case NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED:
    case NS_STYLE_LIST_STYLE_DISCLOSURE_OPEN:
      ascent = fm->EmAscent();
      bulletSize = std::max(nsPresContext::CSSPixelsToAppUnits(MIN_BULLET_SIZE),
                            NSToCoordRound(0.75f * ascent));
      aPadding->BEnd(wm) = NSToCoordRound(0.125f * ascent);
      finalSize.ISize(wm) = finalSize.BSize(wm) = bulletSize;
      if (!wm.IsVertical()) {
        aMetrics.SetBlockStartAscent(bulletSize + aPadding->BEnd(wm));
      }
      AppendSpacingToPadding(fm, aPadding);
      break;

    default:
      GetListItemText(style, wm, Ordinal(), text);
      finalSize.BSize(wm) = fm->MaxHeight();
      finalSize.ISize(wm) = nsLayoutUtils::AppUnitWidthOfStringBidi(
          text, this, *fm, *aRenderingContext);
      aMetrics.SetBlockStartAscent(wm.IsLineInverted() ? fm->MaxDescent()
                                                       : fm->MaxAscent());
      break;
  }
  aMetrics.SetSize(wm, finalSize);
}

void nsBulletFrame::Reflow(nsPresContext* aPresContext, ReflowOutput& aMetrics,
                           const ReflowInput& aReflowInput,
                           nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsBulletFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aMetrics, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "The reflow status should be empty!");

  float inflation = nsLayoutUtils::FontSizeInflationFor(this);
  SetFontSizeInflation(inflation);

  // Get the base size
  GetDesiredSize(aPresContext, aReflowInput.mRenderingContext, aMetrics,
                 inflation, &mPadding);

  // Add in the border and padding; split the top/bottom between the
  // ascent and descent to make things look nice
  WritingMode wm = aReflowInput.GetWritingMode();
  const LogicalMargin& bp = aReflowInput.ComputedLogicalBorderPadding();
  mPadding.BStart(wm) += NSToCoordRound(bp.BStart(wm) * inflation);
  mPadding.IEnd(wm) += NSToCoordRound(bp.IEnd(wm) * inflation);
  mPadding.BEnd(wm) += NSToCoordRound(bp.BEnd(wm) * inflation);
  mPadding.IStart(wm) += NSToCoordRound(bp.IStart(wm) * inflation);

  WritingMode lineWM = aMetrics.GetWritingMode();
  LogicalMargin linePadding = mPadding.ConvertTo(lineWM, wm);
  aMetrics.ISize(lineWM) += linePadding.IStartEnd(lineWM);
  aMetrics.BSize(lineWM) += linePadding.BStartEnd(lineWM);
  aMetrics.SetBlockStartAscent(aMetrics.BlockStartAscent() +
                               linePadding.BStart(lineWM));

  // XXX this is a bit of a hack, we're assuming that no glyphs used for bullets
  // overflow their font-boxes. It'll do for now; to fix it for real, we really
  // should rewrite all the text-handling code here to use gfxTextRun (bug
  // 397294).
  aMetrics.SetOverflowAreasToDesiredBounds();

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aMetrics);
}

/* virtual */
nscoord nsBulletFrame::GetMinISize(gfxContext* aRenderingContext) {
  WritingMode wm = GetWritingMode();
  ReflowOutput reflowOutput(wm);
  DISPLAY_MIN_INLINE_SIZE(this, reflowOutput.ISize(wm));
  LogicalMargin padding(wm);
  GetDesiredSize(PresContext(), aRenderingContext, reflowOutput, 1.0f,
                 &padding);
  reflowOutput.ISize(wm) += padding.IStartEnd(wm);
  return reflowOutput.ISize(wm);
}

/* virtual */
nscoord nsBulletFrame::GetPrefISize(gfxContext* aRenderingContext) {
  WritingMode wm = GetWritingMode();
  ReflowOutput metrics(wm);
  DISPLAY_PREF_INLINE_SIZE(this, metrics.ISize(wm));
  LogicalMargin padding(wm);
  GetDesiredSize(PresContext(), aRenderingContext, metrics, 1.0f, &padding);
  metrics.ISize(wm) += padding.IStartEnd(wm);
  return metrics.ISize(wm);
}

// If a bullet has zero size and is "ignorable" from its styling, we behave
// as if it doesn't exist, from a line-breaking/isize-computation perspective.
// Otherwise, we use the default implementation, same as nsFrame.
static inline bool IsIgnoreable(const nsIFrame* aFrame, nscoord aISize) {
  if (aISize != nscoord(0)) {
    return false;
  }
  auto listStyle = aFrame->StyleList();
  return listStyle->mCounterStyle.IsNone() && !listStyle->GetListStyleImage();
}

/* virtual */
void nsBulletFrame::AddInlineMinISize(gfxContext* aRenderingContext,
                                      nsIFrame::InlineMinISizeData* aData) {
  nscoord isize = nsLayoutUtils::IntrinsicForContainer(
      aRenderingContext, this, nsLayoutUtils::MIN_ISIZE);
  if (MOZ_LIKELY(!::IsIgnoreable(this, isize))) {
    aData->DefaultAddInlineMinISize(this, isize);
  }
}

/* virtual */
void nsBulletFrame::AddInlinePrefISize(gfxContext* aRenderingContext,
                                       nsIFrame::InlinePrefISizeData* aData) {
  nscoord isize = nsLayoutUtils::IntrinsicForContainer(
      aRenderingContext, this, nsLayoutUtils::PREF_ISIZE);
  if (MOZ_LIKELY(!::IsIgnoreable(this, isize))) {
    aData->DefaultAddInlinePrefISize(isize);
  }
}

NS_IMETHODIMP
nsBulletFrame::Notify(imgIRequest* aRequest, int32_t aType,
                      const nsIntRect* aData) {
  if (aType == imgINotificationObserver::SIZE_AVAILABLE) {
    nsCOMPtr<imgIContainer> image;
    aRequest->GetImage(getter_AddRefs(image));
    return OnSizeAvailable(aRequest, image);
  }

  if (aType == imgINotificationObserver::FRAME_UPDATE) {
    // The image has changed.
    // Invalidate the entire content area. Maybe it's not optimal but it's
    // simple and always correct, and I'll be a stunned mullet if it ever
    // matters for performance
    InvalidateFrame();
  }

  if (aType == imgINotificationObserver::IS_ANIMATED) {
    // Register the image request with the refresh driver now that we know it's
    // animated.
    if (aRequest == mImageRequest) {
      RegisterImageRequest(/* aKnownToBeAnimated = */ true);
    }
  }

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    // Unconditionally start decoding for now.
    // XXX(seth): We eventually want to decide whether to do this based on
    // visibility. We should get that for free from bug 1091236.
    nsCOMPtr<imgIContainer> container;
    aRequest->GetImage(getter_AddRefs(container));
    if (container) {
      // Retrieve the intrinsic size of the image.
      int32_t width = 0;
      int32_t height = 0;
      container->GetWidth(&width);
      container->GetHeight(&height);

      // Request a decode at that size.
      container->RequestDecodeForSize(
          IntSize(width, height), imgIContainer::DECODE_FLAGS_DEFAULT |
                                      imgIContainer::FLAG_HIGH_QUALITY_SCALING);
    }

    InvalidateFrame();
  }

  if (aType == imgINotificationObserver::DECODE_COMPLETE) {
    if (Document* parent = GetOurCurrentDoc()) {
      nsCOMPtr<imgIContainer> container;
      aRequest->GetImage(getter_AddRefs(container));
      if (container) {
        container->PropagateUseCounters(parent);
      }
    }
  }

  return NS_OK;
}

Document* nsBulletFrame::GetOurCurrentDoc() const {
  nsIContent* parentContent = GetParent()->GetContent();
  return parentContent ? parentContent->GetComposedDoc() : nullptr;
}

nsresult nsBulletFrame::OnSizeAvailable(imgIRequest* aRequest,
                                        imgIContainer* aImage) {
  if (!aImage) return NS_ERROR_INVALID_ARG;
  if (!aRequest) return NS_ERROR_INVALID_ARG;

  uint32_t status;
  aRequest->GetImageStatus(&status);
  if (status & imgIRequest::STATUS_ERROR) {
    return NS_OK;
  }

  nscoord w, h;
  aImage->GetWidth(&w);
  aImage->GetHeight(&h);

  nsPresContext* presContext = PresContext();

  LogicalSize newsize(GetWritingMode(),
                      nsSize(nsPresContext::CSSPixelsToAppUnits(w),
                             nsPresContext::CSSPixelsToAppUnits(h)));

  if (mIntrinsicSize != newsize) {
    mIntrinsicSize = newsize;

    // Now that the size is available (or an error occurred), trigger
    // a reflow of the bullet frame.
    mozilla::PresShell* presShell = presContext->GetPresShell();
    if (presShell) {
      presShell->FrameNeedsReflow(this, IntrinsicDirty::StyleChange,
                                  NS_FRAME_IS_DIRTY);
    }
  }

  // Handle animations
  aImage->SetAnimationMode(presContext->ImageAnimationMode());
  // Ensure the animation (if any) is started. Note: There is no
  // corresponding call to Decrement for this. This Increment will be
  // 'cleaned up' by the Request when it is destroyed, but only then.
  aRequest->IncrementAnimationConsumers();

  return NS_OK;
}

void nsBulletFrame::GetLoadGroup(nsPresContext* aPresContext,
                                 nsILoadGroup** aLoadGroup) {
  if (!aPresContext) return;

  MOZ_ASSERT(nullptr != aLoadGroup, "null OUT parameter pointer");

  mozilla::PresShell* presShell = aPresContext->GetPresShell();
  if (!presShell) {
    return;
  }

  Document* doc = presShell->GetDocument();
  if (!doc) return;

  *aLoadGroup = doc->GetDocumentLoadGroup().take();
}

float nsBulletFrame::GetFontSizeInflation() const {
  if (!HasFontSizeInflation()) {
    return 1.0f;
  }
  return GetProperty(FontSizeInflationProperty());
}

void nsBulletFrame::SetFontSizeInflation(float aInflation) {
  if (aInflation == 1.0f) {
    if (HasFontSizeInflation()) {
      RemoveStateBits(BULLET_FRAME_HAS_FONT_INFLATION);
      RemoveProperty(FontSizeInflationProperty());
    }
    return;
  }

  AddStateBits(BULLET_FRAME_HAS_FONT_INFLATION);
  SetProperty(FontSizeInflationProperty(), aInflation);
}

already_AddRefed<imgIContainer> nsBulletFrame::GetImage() const {
  if (mImageRequest && StyleList()->GetListStyleImage()) {
    nsCOMPtr<imgIContainer> imageCon;
    mImageRequest->GetImage(getter_AddRefs(imageCon));
    return imageCon.forget();
  }

  return nullptr;
}

nscoord nsBulletFrame::GetListStyleAscent() const {
  RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetFontMetricsForFrame(this, GetFontSizeInflation());
  auto* list = StyleList();
  if (list->mCounterStyle.IsNone()) {
    return 0;
  }
  if (list->mCounterStyle.IsAnonymous()) {
    return fm->MaxAscent();
  }
  // NOTE(emilio): @counter-style can override most of the styles from this
  // list, and we still return the changed ascent. Do we care about that?
  //
  // https://github.com/w3c/csswg-drafts/issues/3584
  nsAtom* style = list->mCounterStyle.AsAtom();
  if (style == nsGkAtoms::disc || style == nsGkAtoms::circle ||
      style == nsGkAtoms::square) {
    nscoord ascent = fm->MaxAscent();
    nscoord baselinePadding = NSToCoordRound(float(ascent) / 8.0f);
    ascent = std::max(nsPresContext::CSSPixelsToAppUnits(MIN_BULLET_SIZE),
                      NSToCoordRound(0.8f * (float(ascent) / 2.0f)));
    return ascent + baselinePadding;
  }
  if (style == nsGkAtoms::disclosure_open ||
      style == nsGkAtoms::disclosure_closed) {
    nscoord ascent = fm->EmAscent();
    nscoord baselinePadding = NSToCoordRound(0.125f * ascent);
    ascent = std::max(nsPresContext::CSSPixelsToAppUnits(MIN_BULLET_SIZE),
                      NSToCoordRound(0.75f * ascent));
    return ascent + baselinePadding;
  }
  return fm->MaxAscent();
}

nscoord nsBulletFrame::GetLogicalBaseline(WritingMode aWritingMode) const {
  nscoord ascent = 0;
  if (GetStateBits() & BULLET_FRAME_IMAGE_LOADING) {
    ascent = BSize(aWritingMode);
  } else {
    ascent = GetListStyleAscent();
  }
  return ascent + GetLogicalUsedMargin(aWritingMode).BStart(aWritingMode);
}

bool nsBulletFrame::GetNaturalBaselineBOffset(WritingMode aWM,
                                              BaselineSharingGroup,
                                              nscoord* aBaseline) const {
  nscoord ascent = 0;
  if (GetStateBits() & BULLET_FRAME_IMAGE_LOADING) {
    ascent = BSize(aWM);
  } else {
    ascent = GetListStyleAscent();
  }
  *aBaseline = ascent;
  return true;
}

#ifdef ACCESSIBILITY
void nsBulletFrame::GetSpokenText(nsAString& aText) {
  CounterStyle* style =
      PresContext()->CounterStyleManager()->ResolveCounterStyle(
          StyleList()->mCounterStyle);
  bool isBullet;
  style->GetSpokenCounterText(Ordinal(), GetWritingMode(), aText, isBullet);
  if (isBullet) {
    if (!style->IsNone()) {
      aText.Append(' ');
    }
  } else {
    nsAutoString prefix, suffix;
    style->GetPrefix(prefix);
    style->GetSuffix(suffix);
    aText = prefix + aText + suffix;
  }
}
#endif

void nsBulletFrame::RegisterImageRequest(bool aKnownToBeAnimated) {
  if (mImageRequest) {
    // mRequestRegistered is a bitfield; unpack it temporarily so we can take
    // the address.
    bool isRequestRegistered = mRequestRegistered;

    if (aKnownToBeAnimated) {
      nsLayoutUtils::RegisterImageRequest(PresContext(), mImageRequest,
                                          &isRequestRegistered);
    } else {
      nsLayoutUtils::RegisterImageRequestIfAnimated(
          PresContext(), mImageRequest, &isRequestRegistered);
    }

    mRequestRegistered = isRequestRegistered;
  }
}

void nsBulletFrame::DeregisterAndCancelImageRequest() {
  if (mImageRequest) {
    // mRequestRegistered is a bitfield; unpack it temporarily so we can take
    // the address.
    bool isRequestRegistered = mRequestRegistered;

    // Deregister our image request from the refresh driver.
    nsLayoutUtils::DeregisterImageRequest(PresContext(), mImageRequest,
                                          &isRequestRegistered);

    mRequestRegistered = isRequestRegistered;

    // Cancel the image request and forget about it.
    mImageRequest->CancelAndForgetObserver(NS_ERROR_FAILURE);
    mImageRequest = nullptr;
  }
}

void nsBulletFrame::SetOrdinal(int32_t aOrdinal, bool aNotify) {
  if (mOrdinal == aOrdinal) {
    return;
  }
  mOrdinal = aOrdinal;
  if (aNotify) {
    PresShell()->FrameNeedsReflow(this, IntrinsicDirty::StyleChange,
                                  NS_FRAME_IS_DIRTY);
  }
}

NS_IMPL_ISUPPORTS(nsBulletListener, imgINotificationObserver)

nsBulletListener::nsBulletListener() : mFrame(nullptr) {}

nsBulletListener::~nsBulletListener() {}

NS_IMETHODIMP
nsBulletListener::Notify(imgIRequest* aRequest, int32_t aType,
                         const nsIntRect* aData) {
  if (!mFrame) {
    return NS_ERROR_FAILURE;
  }
  return mFrame->Notify(aRequest, aType, aData);
}
