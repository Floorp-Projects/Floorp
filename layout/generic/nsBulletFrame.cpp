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
#include "TextDrawTarget.h"
#include "UnitTransforms.h"
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxUtils.h"
#include "imgIContainer.h"
#include "imgRequestProxy.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/PresShell.h"
#include "mozilla/SVGImageContext.h"
#include "mozilla/css/ImageLoader.h"
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
#include "nsLayoutUtils.h"
#include "nsPresContext.h"

#ifdef ACCESSIBILITY
#  include "nsAccessibilityService.h"
#endif

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;
using namespace mozilla::layout;

nsIFrame* NS_NewBulletFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsBulletFrame(aStyle, aPresShell->GetPresContext());
}

NS_DECLARE_FRAME_PROPERTY_SMALL_VALUE(FontSizeInflationProperty, float)

NS_IMPL_FRAMEARENA_HELPERS(nsBulletFrame)

#ifdef DEBUG
NS_QUERYFRAME_HEAD(nsBulletFrame)
  NS_QUERYFRAME_ENTRY(nsBulletFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsIFrame)
#endif

nsBulletFrame::~nsBulletFrame() = default;

CounterStyle* nsBulletFrame::ResolveCounterStyle() {
  return PresContext()->CounterStyleManager()->ResolveCounterStyle(
      StyleList()->mCounterStyle);
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsBulletFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"Bullet"_ns, aResult);
}
#endif

bool nsBulletFrame::IsEmpty() { return IsSelfEmpty(); }

bool nsBulletFrame::IsSelfEmpty() {
  return StyleList()->mCounterStyle.IsNone();
}

/* virtual */
void nsBulletFrame::DidSetComputedStyle(ComputedStyle* aOldStyle) {
  nsIFrame::DidSetComputedStyle(aOldStyle);

  css::ImageLoader* loader = PresContext()->Document()->StyleImageLoader();
  imgIRequest* oldListImage =
      aOldStyle ? aOldStyle->StyleList()->mListStyleImage.GetImageRequest()
                : nullptr;
  imgIRequest* newListImage = StyleList()->mListStyleImage.GetImageRequest();
  if (oldListImage != newListImage) {
    if (oldListImage && HasImageRequest()) {
      loader->DisassociateRequestFromFrame(oldListImage, this);
    }
    if (newListImage) {
      loader->AssociateRequestToFrame(
          newListImage, this,
          css::ImageLoader::Flags::RequiresReflowOnSizeAvailable);

      // Image bullets can affect the layout of the page, so boost the image
      // load priority.
      newListImage->BoostPriority(imgIRequest::CATEGORY_SIZE_QUERY);
    }
  }

#ifdef ACCESSIBILITY
  // Update the list bullet accessible. If old style list isn't available then
  // no need to update the accessible tree because it's not created yet.
  if (aOldStyle) {
    if (nsAccessibilityService* accService =
            PresShell::GetAccessibilityService()) {
      const nsStyleList* oldStyleList = aOldStyle->StyleList();
      bool hadBullet = !oldStyleList->mListStyleImage.IsNone() ||
                       !oldStyleList->mCounterStyle.IsNone();

      const nsStyleList* newStyleList = StyleList();
      bool hasBullet = !newStyleList->mListStyleImage.IsNone() ||
                       !newStyleList->mCounterStyle.IsNone();

      if (hadBullet != hasBullet) {
        if (hadBullet) {
          accService->ContentRemoved(PresContext()->GetPresShell(), mContent);
        } else {
          accService->ContentRangeInserted(PresContext()->GetPresShell(),
                                           mContent, nullptr);
        }
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
  BulletRenderer(nsIFrame* aFrame, const StyleImage& aImage,
                 const nsRect& aDest, uint32_t aFlags)
      : mDest(aDest),
        mColor(NS_RGBA(0, 0, 0, 0)),
        mListStyleType(NS_STYLE_LIST_STYLE_NONE) {
    mImageRenderer.emplace(aFrame, &aImage, aFlags);
    mImageRenderer->SetPreferredSize({}, aDest.Size());
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
                      const nsRect& aDirtyRect, bool aDisableSubpixelAA,
                      nsIFrame* aFrame);

  bool IsImageType() const { return !!mImageRenderer; }

  bool PrepareImage() {
    MOZ_ASSERT(IsImageType());
    return mImageRenderer->PrepareImage();
  }

  ImgDrawResult PrepareResult() const {
    MOZ_ASSERT(IsImageType());
    return mImageRenderer->PrepareResult();
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
  Maybe<nsImageRenderer> mImageRenderer;

  // The position and size of the image bullet.
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

  // mText, mFontMetrics, mPoint are for other list-style-type which can be
  // drawed by text.
  nsString mText;
  RefPtr<nsFontMetrics> mFontMetrics;
  nsPoint mPoint;

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
                                    const nsRect& aDirtyRect,
                                    bool aDisableSubpixelAA, nsIFrame* aFrame) {
  if (IsImageType()) {
    return mImageRenderer->DrawLayer(aFrame->PresContext(), aRenderingContext,
                                     mDest, mDest, mDest.TopLeft(), aDirtyRect,
                                     mDest.Size(),
                                     /* aOpacity = */ 1.0f);
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

  aCtx->SetColor(sRGBColor::FromABGR(mColor));

  nsPresContext* presContext = aFrame->PresContext();
  if (!presContext->BidiEnabled() && HasRTLChars(mText)) {
    presContext->SetBidiEnabled();
  }
  nsLayoutUtils::DrawString(aFrame, *mFontMetrics, aCtx, mText.get(),
                            mText.Length(), mPoint);
}

ImgDrawResult BulletRenderer::CreateWebRenderCommandsForImage(
    nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResources,
    const layers::StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  MOZ_RELEASE_ASSERT(IsImageType());
  return mImageRenderer->BuildWebRenderDisplayItemsForLayer(
      aItem->Frame()->PresContext(), aBuilder, aResources, aSc, aManager, aItem,
      mDest, mDest, mDest.TopLeft(), mDest, mDest.Size(),
      /* aOpacity = */ 1.0f);
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
    return mFrame->InkOverflowRectRelativeToSelf() + ToReferenceFrame();
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

  ImgDrawResult drawResult;
  Maybe<BulletRenderer> br =
      static_cast<nsBulletFrame*>(mFrame)->CreateBulletRenderer(
          *screenRefCtx, aDisplayListBuilder, ToReferenceFrame(), &drawResult);

  if (br) {
    ImgDrawResult commandBuilderResult = br->CreateWebRenderCommands(
        this, aBuilder, aResources, aSc, aManager, aDisplayListBuilder);
    if (commandBuilderResult == ImgDrawResult::NOT_SUPPORTED) {
      return false;
    }

    drawResult &= commandBuilderResult;
  }

  nsDisplayBulletGeometry::UpdateDrawResult(this, drawResult);
  return true;
}

void nsDisplayBullet::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  ImgDrawResult result = static_cast<nsBulletFrame*>(mFrame)->PaintBullet(
      *aCtx, aBuilder, ToReferenceFrame(), GetPaintRect(),
      IsSubpixelAADisabled());
  nsDisplayBulletGeometry::UpdateDrawResult(this, result);
}

void nsBulletFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                     const nsDisplayListSet& aLists) {
  if (!IsVisibleForPainting()) return;

  DO_GLOBAL_REFLOW_COUNT_DSP("nsBulletFrame");

  aLists.Content()->AppendNewToTop<nsDisplayBullet>(aBuilder, this);
}

Maybe<BulletRenderer> nsBulletFrame::CreateBulletRenderer(
    gfxContext& aRenderingContext, nsDisplayListBuilder* aBuilder, nsPoint aPt,
    ImgDrawResult* aDrawResult) {
  *aDrawResult = ImgDrawResult::SUCCESS;

  CounterStyle* listStyleType = ResolveCounterStyle();
  nsMargin padding = mPadding.GetPhysicalMargin(GetWritingMode());
  const auto& image = StyleList()->mListStyleImage;
  if (!image.IsNone()) {
    nsRect dest(padding.left, padding.top,
                mRect.width - (padding.left + padding.right),
                mRect.height - (padding.top + padding.bottom));
    BulletRenderer br(this, image, dest + aPt,
                      aBuilder->GetImageRendererFlags());
    if (br.PrepareImage()) {
      return Some(br);
    }
    *aDrawResult = br.PrepareResult();
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
                                         nsDisplayListBuilder* aBuilder,
                                         nsPoint aPt, const nsRect& aDirtyRect,
                                         bool aDisableSubpixelAA) {
  ImgDrawResult drawResult;
  Maybe<BulletRenderer> br =
      CreateBulletRenderer(aRenderingContext, aBuilder, aPt, &drawResult);

  if (!br) {
    return drawResult;
  }

  return drawResult & br->Paint(aRenderingContext, aPt, aDirtyRect,
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

  nscoord ascent;
  RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetFontMetricsForFrame(this, aFontSizeInflation);

  RemoveStateBits(BULLET_FRAME_IMAGE_LOADING);

  if (Maybe<CSSIntSize> intrinsicSize =
          StyleList()->mListStyleImage.GetIntrinsicSize()) {
    LogicalSize size(GetWritingMode(), CSSPixel::ToAppUnits(*intrinsicSize));
    // auto size the image
    finalSize.ISize(wm) = size.ISize(wm);
    finalSize.BSize(wm) = size.BSize(wm);
    aMetrics.SetBlockStartAscent(size.BSize(wm));
    aMetrics.SetSize(wm, finalSize);

    AppendSpacingToPadding(fm, aPadding);

    AddStateBits(BULLET_FRAME_IMAGE_LOADING);
    return;
  }

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
  const auto bp = aReflowInput.ComputedLogicalBorderPadding(wm);
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
  return listStyle->mCounterStyle.IsNone() &&
         listStyle->mListStyleImage.IsNone();
}

/* virtual */
void nsBulletFrame::AddInlineMinISize(gfxContext* aRenderingContext,
                                      nsIFrame::InlineMinISizeData* aData) {
  nscoord isize = nsLayoutUtils::IntrinsicForContainer(
      aRenderingContext, this, IntrinsicISizeType::MinISize);
  if (MOZ_LIKELY(!::IsIgnoreable(this, isize))) {
    aData->DefaultAddInlineMinISize(this, isize);
  }
}

/* virtual */
void nsBulletFrame::AddInlinePrefISize(gfxContext* aRenderingContext,
                                       nsIFrame::InlinePrefISizeData* aData) {
  nscoord isize = nsLayoutUtils::IntrinsicForContainer(
      aRenderingContext, this, IntrinsicISizeType::PrefISize);
  if (MOZ_LIKELY(!::IsIgnoreable(this, isize))) {
    aData->DefaultAddInlinePrefISize(isize);
  }
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
  if (auto* request = StyleList()->mListStyleImage.GetImageRequest()) {
    nsCOMPtr<imgIContainer> imageCon;
    request->GetImage(getter_AddRefs(imageCon));
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
  if (HasAnyStateBits(BULLET_FRAME_IMAGE_LOADING)) {
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
  if (HasAnyStateBits(BULLET_FRAME_IMAGE_LOADING)) {
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
