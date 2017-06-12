/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for list-item bullets */

#include "nsBulletFrame.h"

#include "gfx2DGlue.h"
#include "gfxPrefs.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/layers/LayersMessages.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/WebRenderDisplayItemLayer.h"
#include "mozilla/layers/WebRenderMessages.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Move.h"
#include "nsCOMPtr.h"
#include "nsFontMetrics.h"
#include "nsGkAtoms.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValueInlines.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsRenderingContext.h"
#include "nsDisplayList.h"
#include "nsCounterManager.h"
#include "nsBidiUtils.h"
#include "CounterStyleManager.h"
#include "UnitTransforms.h"

#include "imgIContainer.h"
#include "ImageLayers.h"
#include "imgRequestProxy.h"
#include "nsIURI.h"
#include "SVGImageContext.h"
#include "mozilla/layers/WebRenderBridgeChild.h"

#include <algorithm>

#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

NS_DECLARE_FRAME_PROPERTY_SMALL_VALUE(FontSizeInflationProperty, float)

NS_IMPL_FRAMEARENA_HELPERS(nsBulletFrame)

#ifdef DEBUG
NS_QUERYFRAME_HEAD(nsBulletFrame)
  NS_QUERYFRAME_ENTRY(nsBulletFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsFrame)
#endif

nsBulletFrame::~nsBulletFrame()
{
  NS_ASSERTION(!mBlockingOnload, "Still blocking onload in destructor?");
}

void
nsBulletFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // Stop image loading first.
  DeregisterAndCancelImageRequest();

  if (mListener) {
    mListener->SetFrame(nullptr);
  }

  // Let base class do the rest
  nsFrame::DestroyFrom(aDestructRoot);
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsBulletFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Bullet"), aResult);
}
#endif

bool
nsBulletFrame::IsEmpty()
{
  return IsSelfEmpty();
}

bool
nsBulletFrame::IsSelfEmpty()
{
  return StyleList()->mCounterStyle->IsNone();
}

/* virtual */ void
nsBulletFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsFrame::DidSetStyleContext(aOldStyleContext);

  imgRequestProxy *newRequest = StyleList()->GetListStyleImage();

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
      newRequest->Clone(mListener, getter_AddRefs(newRequestClone));

      // Deregister the old request. We wait until after Clone is done in case
      // the old request and the new request are the same underlying image
      // accessed via different URLs.
      DeregisterAndCancelImageRequest();

      // Register the new request.
      mImageRequest = Move(newRequestClone);
      RegisterImageRequest(/* aKnownToBeAnimated = */ false);
    }
  } else {
    // No image request on the new style context.
    DeregisterAndCancelImageRequest();
  }

#ifdef ACCESSIBILITY
  // Update the list bullet accessible. If old style list isn't available then
  // no need to update the accessible tree because it's not created yet.
  if (aOldStyleContext) {
    nsAccessibilityService* accService = nsIPresShell::AccService();
    if (accService) {
      const nsStyleList* oldStyleList = aOldStyleContext->PeekStyleList();
      if (oldStyleList) {
        bool hadBullet = oldStyleList->GetListStyleImage() ||
          !oldStyleList->mCounterStyle->IsNone();

        const nsStyleList* newStyleList = StyleList();
        bool hasBullet = newStyleList->GetListStyleImage() ||
          !newStyleList->mCounterStyle->IsNone();

        if (hadBullet != hasBullet) {
          accService->UpdateListBullet(PresContext()->GetPresShell(), mContent,
                                       hasBullet);
        }
      }
    }
  }
#endif
}

class nsDisplayBulletGeometry
  : public nsDisplayItemGenericGeometry
  , public nsImageGeometryMixin<nsDisplayBulletGeometry>
{
public:
  nsDisplayBulletGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
    : nsDisplayItemGenericGeometry(aItem, aBuilder)
    , nsImageGeometryMixin(aItem, aBuilder)
  {
    nsBulletFrame* f = static_cast<nsBulletFrame*>(aItem->Frame());
    mOrdinal = f->GetOrdinal();
  }

  int32_t mOrdinal;
};

class BulletRenderer final
{
public:
  BulletRenderer(imgIContainer* image, const nsRect& dest)
    : mImage(image)
    , mDest(dest)
    , mListStyleType(NS_STYLE_LIST_STYLE_NONE)
  {
    MOZ_ASSERT(IsImageType());
  }

  BulletRenderer(Path* path, nscolor color, int32_t listStyleType)
    : mColor(color)
    , mPath(path)
    , mListStyleType(listStyleType)
  {
    MOZ_ASSERT(IsPathType());
  }

  BulletRenderer(const nsString& text,
                 nsFontMetrics* fm,
                 nscolor color,
                 const nsPoint& point,
                 int32_t listStyleType)
    : mColor(color)
    , mText(text)
    , mFontMetrics(fm)
    , mPoint(point)
    , mListStyleType(listStyleType)
  {
    MOZ_ASSERT(IsTextType());
  }

  void
  CreateWebRenderCommands(nsDisplayItem* aItem,
                          wr::DisplayListBuilder& aBuilder,
                          const layers::StackingContextHelper& aSc,
                          nsTArray<layers::WebRenderParentCommand>& aParentCommands,
                          layers::WebRenderDisplayItemLayer* aLayer);

  DrawResult
  Paint(nsRenderingContext& aRenderingContext, nsPoint aPt,
        const nsRect& aDirtyRect, uint32_t aFlags,
        bool aDisableSubpixelAA, nsIFrame* aFrame);

  bool
  IsImageType() const
  {
    return mListStyleType == NS_STYLE_LIST_STYLE_NONE && mImage;
  }

  bool
  IsPathType() const
  {
    return (mListStyleType == NS_STYLE_LIST_STYLE_DISC ||
            mListStyleType == NS_STYLE_LIST_STYLE_CIRCLE ||
            mListStyleType == NS_STYLE_LIST_STYLE_SQUARE ||
            mListStyleType == NS_STYLE_LIST_STYLE_DISCLOSURE_OPEN ||
            mListStyleType == NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED) &&
           mPath;
  }

  bool
  IsTextType() const
  {
    return mListStyleType != NS_STYLE_LIST_STYLE_NONE &&
           mListStyleType != NS_STYLE_LIST_STYLE_DISC &&
           mListStyleType != NS_STYLE_LIST_STYLE_CIRCLE &&
           mListStyleType != NS_STYLE_LIST_STYLE_SQUARE &&
           mListStyleType != NS_STYLE_LIST_STYLE_DISCLOSURE_OPEN &&
           mListStyleType != NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED &&
           !mText.IsEmpty();
  }

  bool
  BuildGlyphForText(nsDisplayItem* aItem, bool disableSubpixelAA);

  bool
  IsImageContainerAvailable(layers::LayerManager* aManager, uint32_t aFlags);

private:
  void
  CreateWebRenderCommandsForImage(nsDisplayItem* aItem,
                                  wr::DisplayListBuilder& aBuilder,
                                  const layers::StackingContextHelper& aSc,
                                  nsTArray<layers::WebRenderParentCommand>& aParentCommands,
                                  layers::WebRenderDisplayItemLayer* aLayer);

  void
  CreateWebRenderCommandsForPath(nsDisplayItem* aItem,
                                 wr::DisplayListBuilder& aBuilder,
                                 const layers::StackingContextHelper& aSc,
                                 nsTArray<layers::WebRenderParentCommand>& aParentCommands,
                                 layers::WebRenderDisplayItemLayer* aLayer);

  void
  CreateWebRenderCommandsForText(nsDisplayItem* aItem,
                                 wr::DisplayListBuilder& aBuilder,
                                 const layers::StackingContextHelper& aSc,
                                 layers::WebRenderDisplayItemLayer* aLayer);

private:
  // mImage and mDest are the properties for list-style-image.
  // mImage is the image content and mDest is the image position.
  RefPtr<imgIContainer> mImage;
  nsRect mDest;

  // mColor indicate the color of list-style. Both text and path type would use this memeber.
  nscolor mColor;

  // mPath record the path of the list-style for later drawing.
  // Included following types: square, circle, disc, disclosure open and disclosure closed.
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

void
BulletRenderer::CreateWebRenderCommands(nsDisplayItem* aItem,
                                        wr::DisplayListBuilder& aBuilder,
                                        const layers::StackingContextHelper& aSc,
                                        nsTArray<layers::WebRenderParentCommand>& aParentCommands,
                                        layers::WebRenderDisplayItemLayer* aLayer)
{
  if (IsImageType()) {
    CreateWebRenderCommandsForImage(aItem, aBuilder, aSc, aParentCommands, aLayer);
  } else if (IsPathType()) {
    CreateWebRenderCommandsForPath(aItem, aBuilder, aSc, aParentCommands, aLayer);
  } else {
    MOZ_ASSERT(IsTextType());
    CreateWebRenderCommandsForText(aItem, aBuilder, aSc, aLayer);
  }
}

DrawResult
BulletRenderer::Paint(nsRenderingContext& aRenderingContext, nsPoint aPt,
                      const nsRect& aDirtyRect, uint32_t aFlags,
                      bool aDisableSubpixelAA, nsIFrame* aFrame)
{
  if (IsImageType()) {
    SamplingFilter filter = nsLayoutUtils::GetSamplingFilterForFrame(aFrame);
    return nsLayoutUtils::DrawSingleImage(*aRenderingContext.ThebesContext(),
                                          aFrame->PresContext(), mImage, filter,
                                          mDest, aDirtyRect,
                                          /* no SVGImageContext */ Nothing(),
                                          aFlags);
  }

  if (IsPathType()) {
    DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();
    switch (mListStyleType) {
    case NS_STYLE_LIST_STYLE_CIRCLE:
      MOZ_ASSERT(mPath);
      drawTarget->Stroke(mPath, ColorPattern(ToDeviceColor(mColor)));
      break;
    case NS_STYLE_LIST_STYLE_DISC:
    case NS_STYLE_LIST_STYLE_SQUARE:
    case NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED:
    case NS_STYLE_LIST_STYLE_DISCLOSURE_OPEN:
      MOZ_ASSERT(mPath);
      drawTarget->Fill(mPath, ColorPattern(ToDeviceColor(mColor)));
      break;
    default:
      MOZ_CRASH("unreachable");
    }
  }

  if (IsTextType()) {
    DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();
    DrawTargetAutoDisableSubpixelAntialiasing
      disable(drawTarget, aDisableSubpixelAA);

    aRenderingContext.ThebesContext()->SetColor(
      Color::FromABGR(mColor));

    nsPresContext* presContext = aFrame->PresContext();
    if (!presContext->BidiEnabled() && HasRTLChars(mText)) {
      presContext->SetBidiEnabled();
    }
    nsLayoutUtils::DrawString(aFrame, *mFontMetrics, &aRenderingContext,
                              mText.get(), mText.Length(), mPoint);
  }

  return DrawResult::SUCCESS;
}

bool
BulletRenderer::BuildGlyphForText(nsDisplayItem* aItem, bool disableSubpixelAA)
{
  MOZ_ASSERT(IsTextType());

  RefPtr<DrawTargetCapture> capture =
    gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget()->CreateCaptureDT(IntSize());
  RefPtr<gfxContext> captureCtx = gfxContext::CreateOrNull(capture);
  nsRenderingContext ctx(captureCtx);

  {
    DrawTargetAutoDisableSubpixelAntialiasing
      disable(capture, disableSubpixelAA);

    ctx.ThebesContext()->SetColor(
      Color::FromABGR(mColor));

    nsPresContext* presContext = aItem->Frame()->PresContext();
    if (!presContext->BidiEnabled() && HasRTLChars(mText)) {
      presContext->SetBidiEnabled();
    }

    nsLayoutUtils::DrawString(aItem->Frame(), *mFontMetrics, &ctx,
                              mText.get(), mText.Length(), mPoint);
  }

  layers::GlyphArray* g = mGlyphs.AppendElement();
  std::vector<Glyph> glyphs;
  Color color;
  if (!capture->ContainsOnlyColoredGlyphs(mFont, color, glyphs)) {
    mFont = nullptr;
    mGlyphs.Clear();
    return false;
  }

  g->glyphs().SetLength(glyphs.size());
  PodCopy(g->glyphs().Elements(), glyphs.data(), glyphs.size());
  g->color() = color;

  return true;
}

bool
BulletRenderer::IsImageContainerAvailable(layers::LayerManager* aManager, uint32_t aFlags)
{
  MOZ_ASSERT(IsImageType());

  return mImage->IsImageContainerAvailable(aManager, aFlags);
}

void
BulletRenderer::CreateWebRenderCommandsForImage(nsDisplayItem* aItem,
                                                wr::DisplayListBuilder& aBuilder,
                                                const layers::StackingContextHelper& aSc,
                                                nsTArray<layers::WebRenderParentCommand>& aParentCommands,
                                                layers::WebRenderDisplayItemLayer* aLayer)
{
  MOZ_ASSERT(IsImageType());

  if (!mImage) {
     return;
  }

  layers::WebRenderDisplayItemLayer* layer = static_cast<layers::WebRenderDisplayItemLayer*>(aLayer);
  nsDisplayListBuilder* builder = layer->GetDisplayListBuilder();
  uint32_t flags = builder->ShouldSyncDecodeImages() ?
                   imgIContainer::FLAG_SYNC_DECODE :
                   imgIContainer::FLAG_NONE;

  RefPtr<layers::ImageContainer> container =
    mImage->GetImageContainer(aLayer->WrManager(), flags);
  if (!container) {
    return;
  }

  Maybe<wr::ImageKey> key = layer->SendImageContainer(container, aParentCommands);
  if (key.isNothing()) {
    return;
  }

  const int32_t appUnitsPerDevPixel = aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
  LayoutDeviceRect destRect = LayoutDeviceRect::FromAppUnits(mDest, appUnitsPerDevPixel);
  WrRect dest = aSc.ToRelativeWrRectRounded(destRect);

  WrClipRegionToken clipRegion = aBuilder.PushClipRegion(dest);

  aBuilder.PushImage(dest,
                     clipRegion,
                     WrImageRendering::Auto,
                     key.value());
}

void
BulletRenderer::CreateWebRenderCommandsForPath(nsDisplayItem* aItem,
                                               wr::DisplayListBuilder& aBuilder,
                                               const layers::StackingContextHelper& aSc,
                                               nsTArray<layers::WebRenderParentCommand>& aParentCommands,
                                               layers::WebRenderDisplayItemLayer* aLayer)
{
  MOZ_ASSERT(IsPathType());
  MOZ_ASSERT(aLayer->GetDisplayItem() == aItem);

  if (!aLayer->PushItemAsBlobImage(aBuilder, aSc)) {
    NS_WARNING("Fail to create WebRender commands for Bullet path.");
  }
}

void
BulletRenderer::CreateWebRenderCommandsForText(nsDisplayItem* aItem,
                                               wr::DisplayListBuilder& aBuilder,
                                               const layers::StackingContextHelper& aSc,
                                               layers::WebRenderDisplayItemLayer* aLayer)
{
  MOZ_ASSERT(IsTextType());
  MOZ_ASSERT(mFont);
  MOZ_ASSERT(!mGlyphs.IsEmpty());

  layers::WebRenderDisplayItemLayer* layer = static_cast<layers::WebRenderDisplayItemLayer*>(aLayer);
  nsDisplayListBuilder* builder = layer->GetDisplayListBuilder();
  const int32_t appUnitsPerDevPixel = aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
  bool dummy;
  LayerRect destRect = ViewAs<LayerPixel>(
      LayoutDeviceRect::FromAppUnits(
          aItem->GetBounds(builder, &dummy), appUnitsPerDevPixel),
      PixelCastJustification::WebRenderHasUnitResolution);

  layer->WrBridge()->PushGlyphs(aBuilder, mGlyphs, mFont, aSc, destRect, destRect);
}

class nsDisplayBullet final : public nsDisplayItem {
public:
  nsDisplayBullet(nsDisplayListBuilder* aBuilder, nsBulletFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame)
    , mDisableSubpixelAA(false)
  {
    MOZ_COUNT_CTOR(nsDisplayBullet);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayBullet() {
    MOZ_COUNT_DTOR(nsDisplayBullet);
  }
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) override
  {
    *aSnap = false;
    return mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aParameters) override;

  virtual void CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       const StackingContextHelper& aSc,
                                       nsTArray<layers::WebRenderParentCommand>& aParentCommands,
                                       layers::WebRenderDisplayItemLayer* aLayer) override;

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*> *aOutFrames) override {
    aOutFrames->AppendElement(mFrame);
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) override;
  NS_DISPLAY_DECL_NAME("Bullet", TYPE_BULLET)

  virtual nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) override
  {
    bool snap;
    return GetBounds(aBuilder, &snap);
  }

  virtual void DisableComponentAlpha() override {
    mDisableSubpixelAA = true;
  }

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplayBulletGeometry(this, aBuilder);
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion *aInvalidRegion) override
  {
    const nsDisplayBulletGeometry* geometry = static_cast<const nsDisplayBulletGeometry*>(aGeometry);
    nsBulletFrame* f = static_cast<nsBulletFrame*>(mFrame);

    if (f->GetOrdinal() != geometry->mOrdinal) {
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

    return nsDisplayItem::ComputeInvalidationRegion(aBuilder, aGeometry, aInvalidRegion);
  }

protected:
  bool mDisableSubpixelAA;
  Maybe<BulletRenderer> mBulletRenderer;
};

LayerState
nsDisplayBullet::GetLayerState(nsDisplayListBuilder* aBuilder,
                               LayerManager* aManager,
                               const ContainerLayerParameters& aParameters)
{
  if (!ShouldUseAdvancedLayer(aManager, gfxPrefs::LayersAllowBulletLayers)) {
    return LAYER_NONE;
  }

  RefPtr<gfxContext> screenRefCtx =
    gfxContext::CreateOrNull(gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget());
  nsRenderingContext ctx(screenRefCtx);

  Maybe<BulletRenderer> br = static_cast<nsBulletFrame*>(mFrame)->
    CreateBulletRenderer(ctx, ToReferenceFrame());

  if (!br) {
    return LAYER_NONE;
  }

  if (br->IsImageType()) {
    uint32_t flags = aBuilder->ShouldSyncDecodeImages()
                   ? imgIContainer::FLAG_SYNC_DECODE
                   : imgIContainer::FLAG_NONE;

    if (!br->IsImageContainerAvailable(aManager, flags)) {
      return LAYER_NONE;
    }
  }

  if (br->IsTextType()) {
    if (!br->BuildGlyphForText(this, mDisableSubpixelAA)) {
      return LAYER_NONE;
    }
  }

  mBulletRenderer = br;
  return LAYER_ACTIVE;
}

already_AddRefed<layers::Layer>
nsDisplayBullet::BuildLayer(nsDisplayListBuilder* aBuilder,
                            LayerManager* aManager,
                            const ContainerLayerParameters& aContainerParameters)
{
  if (!mBulletRenderer) {
    return nullptr;
  }

  return BuildDisplayItemLayer(aBuilder, aManager, aContainerParameters);
}

void
nsDisplayBullet::CreateWebRenderCommands(wr::DisplayListBuilder& aBuilder,
                                         const StackingContextHelper& aSc,
                                         nsTArray<layers::WebRenderParentCommand>& aParentCommands,
                                         layers::WebRenderDisplayItemLayer* aLayer)
{
  if (!mBulletRenderer)
    return;

  mBulletRenderer->CreateWebRenderCommands(this, aBuilder, aSc, aParentCommands, aLayer);
}

void nsDisplayBullet::Paint(nsDisplayListBuilder* aBuilder,
                            nsRenderingContext* aCtx)
{
  uint32_t flags = imgIContainer::FLAG_NONE;
  if (aBuilder->ShouldSyncDecodeImages()) {
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  }

  DrawResult result = static_cast<nsBulletFrame*>(mFrame)->
    PaintBullet(*aCtx, ToReferenceFrame(), mVisibleRect, flags,
                mDisableSubpixelAA);

  nsDisplayBulletGeometry::UpdateDrawResult(this, result);
}

void
nsBulletFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists)
{
  if (!IsVisibleForPainting(aBuilder))
    return;

  DO_GLOBAL_REFLOW_COUNT_DSP("nsBulletFrame");

  aLists.Content()->AppendNewToTop(
    new (aBuilder) nsDisplayBullet(aBuilder, this));
}

Maybe<BulletRenderer>
nsBulletFrame::CreateBulletRenderer(nsRenderingContext& aRenderingContext, nsPoint aPt)
{
  const nsStyleList* myList = StyleList();
  CounterStyle* listStyleType = myList->mCounterStyle;
  nsMargin padding = mPadding.GetPhysicalMargin(GetWritingMode());

  if (myList->GetListStyleImage() && mImageRequest) {
    uint32_t status;
    mImageRequest->GetImageStatus(&status);
    if (status & imgIRequest::STATUS_LOAD_COMPLETE &&
        !(status & imgIRequest::STATUS_ERROR)) {
      nsCOMPtr<imgIContainer> imageCon;
      mImageRequest->GetImage(getter_AddRefs(imageCon));
      if (imageCon) {
        nsRect dest(padding.left, padding.top,
                    mRect.width - (padding.left + padding.right),
                    mRect.height - (padding.top + padding.bottom));
        BulletRenderer br(imageCon, dest + aPt);
        return Some(br);
      }
    }
  }

  nscolor color = nsLayoutUtils::GetColor(this, &nsStyleColor::mColor);

  DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();
  int32_t appUnitsPerDevPixel = PresContext()->AppUnitsPerDevPixel();

  switch (listStyleType->GetStyle()) {
  case NS_STYLE_LIST_STYLE_NONE:
    return Nothing();

  case NS_STYLE_LIST_STYLE_DISC:
  case NS_STYLE_LIST_STYLE_CIRCLE:
    {
      nsRect rect(padding.left + aPt.x,
                  padding.top + aPt.y,
                  mRect.width - (padding.left + padding.right),
                  mRect.height - (padding.top + padding.bottom));
      Rect devPxRect = NSRectToRect(rect, appUnitsPerDevPixel);
      RefPtr<PathBuilder> builder = drawTarget->CreatePathBuilder();
      AppendEllipseToPath(builder, devPxRect.Center(), devPxRect.Size());

      RefPtr<Path> path = builder->Finish();
      BulletRenderer br(path, color, listStyleType->GetStyle());
      return Some(br);
    }

  case NS_STYLE_LIST_STYLE_SQUARE:
    {
      nsRect rect(aPt, mRect.Size());
      rect.Deflate(padding);

      // Snap the height and the width of the rectangle to device pixels,
      // and then center the result within the original rectangle, so that
      // all square bullets at the same font size have the same visual
      // size (bug 376690).
      // FIXME: We should really only do this if we're not transformed
      // (like gfxContext::UserToDevicePixelSnapped does).
      nsPresContext *pc = PresContext();
      nsRect snapRect(rect.x, rect.y,
                      pc->RoundAppUnitsToNearestDevPixels(rect.width),
                      pc->RoundAppUnitsToNearestDevPixels(rect.height));
      snapRect.MoveBy((rect.width - snapRect.width) / 2,
                      (rect.height - snapRect.height) / 2);
      Rect devPxRect =
        NSRectToSnappedRect(snapRect, appUnitsPerDevPixel, *drawTarget);
      RefPtr<PathBuilder> builder = drawTarget->CreatePathBuilder();
      AppendRectToPath(builder, devPxRect);

      RefPtr<Path> path = builder->Finish();
      BulletRenderer br(path, color, listStyleType->GetStyle());
      return Some(br);
    }

  case NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED:
  case NS_STYLE_LIST_STYLE_DISCLOSURE_OPEN:
    {
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
      nsPresContext *pc = PresContext();
      rect.x = pc->RoundAppUnitsToNearestDevPixels(rect.x);
      rect.y = pc->RoundAppUnitsToNearestDevPixels(rect.y);

      RefPtr<PathBuilder> builder = drawTarget->CreatePathBuilder();
      if (isDown) {
        // to bottom
        builder->MoveTo(NSPointToPoint(rect.TopLeft(), appUnitsPerDevPixel));
        builder->LineTo(NSPointToPoint(rect.TopRight(), appUnitsPerDevPixel));
        builder->LineTo(NSPointToPoint((rect.BottomLeft() + rect.BottomRight()) / 2,
                                       appUnitsPerDevPixel));
      } else {
        bool isLR = isVertical ? wm.IsVerticalLR() : wm.IsBidiLTR();
        if (isLR) {
          // to right
          builder->MoveTo(NSPointToPoint(rect.TopLeft(), appUnitsPerDevPixel));
          builder->LineTo(NSPointToPoint((rect.TopRight() + rect.BottomRight()) / 2,
                                         appUnitsPerDevPixel));
          builder->LineTo(NSPointToPoint(rect.BottomLeft(), appUnitsPerDevPixel));
        } else {
          // to left
          builder->MoveTo(NSPointToPoint(rect.TopRight(), appUnitsPerDevPixel));
          builder->LineTo(NSPointToPoint(rect.BottomRight(), appUnitsPerDevPixel));
          builder->LineTo(NSPointToPoint((rect.TopLeft() + rect.BottomLeft()) / 2,
                                         appUnitsPerDevPixel));
        }
      }

      RefPtr<Path> path = builder->Finish();
      BulletRenderer br(path, color, listStyleType->GetStyle());
      return Some(br);
    }

  default:
    {
      RefPtr<nsFontMetrics> fm =
        nsLayoutUtils::GetFontMetricsForFrame(this, GetFontSizeInflation());
      nsAutoString text;
      GetListItemText(text);
      WritingMode wm = GetWritingMode();
      nscoord ascent = wm.IsLineInverted()
                         ? fm->MaxDescent() : fm->MaxAscent();
      aPt.MoveBy(padding.left, padding.top);
      gfxContext *ctx = aRenderingContext.ThebesContext();
      if (wm.IsVertical()) {
        if (wm.IsVerticalLR()) {
          aPt.x = NSToCoordRound(nsLayoutUtils::GetSnappedBaselineX(
                                   this, ctx, aPt.x, ascent));
        } else {
          aPt.x = NSToCoordRound(nsLayoutUtils::GetSnappedBaselineX(
                                   this, ctx, aPt.x + mRect.width,
                                   -ascent));
        }
      } else {
        aPt.y = NSToCoordRound(nsLayoutUtils::GetSnappedBaselineY(
                                 this, ctx, aPt.y, ascent));
      }

      BulletRenderer br(text, fm, color, aPt, listStyleType->GetStyle());
      return Some(br);
    }
  }

  MOZ_CRASH("unreachable");
  return Nothing();
}

DrawResult
nsBulletFrame::PaintBullet(nsRenderingContext& aRenderingContext, nsPoint aPt,
                           const nsRect& aDirtyRect, uint32_t aFlags,
                           bool aDisableSubpixelAA)
{
  Maybe<BulletRenderer> br = CreateBulletRenderer(aRenderingContext, aPt);

  if (!br) {
    return DrawResult::SUCCESS;
  }

  return br->Paint(aRenderingContext, aPt, aDirtyRect,
                   aFlags, aDisableSubpixelAA, this);
}

int32_t
nsBulletFrame::SetListItemOrdinal(int32_t aNextOrdinal,
                                  bool* aChanged,
                                  int32_t aIncrement)
{
  MOZ_ASSERT(aIncrement == 1 || aIncrement == -1,
             "We shouldn't have weird increments here");

  // Assume that the ordinal comes from the caller
  int32_t oldOrdinal = mOrdinal;
  mOrdinal = aNextOrdinal;

  // Try to get value directly from the list-item, if it specifies a
  // value attribute. Note: we do this with our parent's content
  // because our parent is the list-item.
  nsIContent* parentContent = GetParent()->GetContent();
  if (parentContent) {
    nsGenericHTMLElement *hc =
      nsGenericHTMLElement::FromContent(parentContent);
    if (hc) {
      const nsAttrValue* attr = hc->GetParsedAttr(nsGkAtoms::value);
      if (attr && attr->Type() == nsAttrValue::eInteger) {
        // Use ordinal specified by the value attribute
        mOrdinal = attr->GetIntegerValue();
      }
    }
  }

  *aChanged = oldOrdinal != mOrdinal;

  return nsCounterManager::IncrementCounter(mOrdinal, aIncrement);
}

void
nsBulletFrame::GetListItemText(nsAString& aResult)
{
  CounterStyle* style = StyleList()->mCounterStyle;
  NS_ASSERTION(style->GetStyle() != NS_STYLE_LIST_STYLE_NONE &&
               style->GetStyle() != NS_STYLE_LIST_STYLE_DISC &&
               style->GetStyle() != NS_STYLE_LIST_STYLE_CIRCLE &&
               style->GetStyle() != NS_STYLE_LIST_STYLE_SQUARE &&
               style->GetStyle() != NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED &&
               style->GetStyle() != NS_STYLE_LIST_STYLE_DISCLOSURE_OPEN,
               "we should be using specialized code for these types");

  bool isRTL;
  nsAutoString counter, prefix, suffix;
  style->GetPrefix(prefix);
  style->GetSuffix(suffix);
  style->GetCounterText(mOrdinal, GetWritingMode(), counter, isRTL);

  aResult.Truncate();
  aResult.Append(prefix);
  if (GetWritingMode().IsBidiLTR() != isRTL) {
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

void
nsBulletFrame::AppendSpacingToPadding(nsFontMetrics* aFontMetrics,
                                      LogicalMargin* aPadding)
{
  aPadding->IEnd(GetWritingMode()) += aFontMetrics->EmHeight() / 2;
}

void
nsBulletFrame::GetDesiredSize(nsPresContext*  aCX,
                              nsRenderingContext *aRenderingContext,
                              ReflowOutput& aMetrics,
                              float aFontSizeInflation,
                              LogicalMargin* aPadding)
{
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
  switch (myList->mCounterStyle->GetStyle()) {
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
      bulletSize = std::max(
          nsPresContext::CSSPixelsToAppUnits(MIN_BULLET_SIZE),
          NSToCoordRound(0.75f * ascent));
      aPadding->BEnd(wm) = NSToCoordRound(0.125f * ascent);
      finalSize.ISize(wm) = finalSize.BSize(wm) = bulletSize;
      if (!wm.IsVertical()) {
        aMetrics.SetBlockStartAscent(bulletSize + aPadding->BEnd(wm));
      }
      AppendSpacingToPadding(fm, aPadding);
      break;

    default:
      GetListItemText(text);
      finalSize.BSize(wm) = fm->MaxHeight();
      finalSize.ISize(wm) =
        nsLayoutUtils::AppUnitWidthOfStringBidi(text, this, *fm, *aRenderingContext);
      aMetrics.SetBlockStartAscent(wm.IsLineInverted()
                                     ? fm->MaxDescent() : fm->MaxAscent());
      break;
  }
  aMetrics.SetSize(wm, finalSize);
}

void
nsBulletFrame::Reflow(nsPresContext* aPresContext,
                      ReflowOutput& aMetrics,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsBulletFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aMetrics, aStatus);

  float inflation = nsLayoutUtils::FontSizeInflationFor(this);
  SetFontSizeInflation(inflation);

  // Get the base size
  GetDesiredSize(aPresContext, aReflowInput.mRenderingContext, aMetrics, inflation,
                 &mPadding);

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

  aStatus.Reset();
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aMetrics);
}

/* virtual */ nscoord
nsBulletFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  WritingMode wm = GetWritingMode();
  ReflowOutput reflowOutput(wm);
  DISPLAY_MIN_WIDTH(this, reflowOutput.ISize(wm));
  LogicalMargin padding(wm);
  GetDesiredSize(PresContext(), aRenderingContext, reflowOutput, 1.0f, &padding);
  reflowOutput.ISize(wm) += padding.IStartEnd(wm);
  return reflowOutput.ISize(wm);
}

/* virtual */ nscoord
nsBulletFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  WritingMode wm = GetWritingMode();
  ReflowOutput metrics(wm);
  DISPLAY_PREF_WIDTH(this, metrics.ISize(wm));
  LogicalMargin padding(wm);
  GetDesiredSize(PresContext(), aRenderingContext, metrics, 1.0f, &padding);
  metrics.ISize(wm) += padding.IStartEnd(wm);
  return metrics.ISize(wm);
}

// If a bullet has zero size and is "ignorable" from its styling, we behave
// as if it doesn't exist, from a line-breaking/isize-computation perspective.
// Otherwise, we use the default implementation, same as nsFrame.
static inline bool
IsIgnoreable(const nsIFrame* aFrame, nscoord aISize)
{
  if (aISize != nscoord(0)) {
    return false;
  }
  auto listStyle = aFrame->StyleList();
  return listStyle->mCounterStyle->IsNone() &&
         !listStyle->GetListStyleImage();
}

/* virtual */ void
nsBulletFrame::AddInlineMinISize(nsRenderingContext* aRenderingContext,
                                 nsIFrame::InlineMinISizeData* aData)
{
  nscoord isize = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                    this, nsLayoutUtils::MIN_ISIZE);
  if (MOZ_LIKELY(!::IsIgnoreable(this, isize))) {
    aData->DefaultAddInlineMinISize(this, isize);
  }
}

/* virtual */ void
nsBulletFrame::AddInlinePrefISize(nsRenderingContext* aRenderingContext,
                                  nsIFrame::InlinePrefISizeData* aData)
{
  nscoord isize = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                    this, nsLayoutUtils::PREF_ISIZE);
  if (MOZ_LIKELY(!::IsIgnoreable(this, isize))) {
    aData->DefaultAddInlinePrefISize(isize);
  }
}

NS_IMETHODIMP
nsBulletFrame::Notify(imgIRequest *aRequest, int32_t aType, const nsIntRect* aData)
{
  if (aType == imgINotificationObserver::SIZE_AVAILABLE) {
    nsCOMPtr<imgIContainer> image;
    aRequest->GetImage(getter_AddRefs(image));
    return OnSizeAvailable(aRequest, image);
  }

  if (aType == imgINotificationObserver::FRAME_UPDATE) {
    // The image has changed.
    // Invalidate the entire content area. Maybe it's not optimal but it's simple and
    // always correct, and I'll be a stunned mullet if it ever matters for performance
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
      container->RequestDecodeForSize(IntSize(width, height),
                                      imgIContainer::DECODE_FLAGS_DEFAULT);
    }

    InvalidateFrame();
  }

  if (aType == imgINotificationObserver::DECODE_COMPLETE) {
    if (nsIDocument* parent = GetOurCurrentDoc()) {
      nsCOMPtr<imgIContainer> container;
      aRequest->GetImage(getter_AddRefs(container));
      if (container) {
        container->PropagateUseCounters(parent);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBulletFrame::BlockOnload(imgIRequest* aRequest)
{
  if (aRequest != mImageRequest) {
    return NS_OK;
  }

  NS_ASSERTION(!mBlockingOnload, "Double BlockOnload for an nsBulletFrame?");

  nsIDocument* doc = GetOurCurrentDoc();
  if (doc) {
    mBlockingOnload = true;
    doc->BlockOnload();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBulletFrame::UnblockOnload(imgIRequest* aRequest)
{
  if (aRequest != mImageRequest) {
    return NS_OK;
  }

  NS_ASSERTION(!mBlockingOnload, "Double UnblockOnload for an nsBulletFrame?");

  nsIDocument* doc = GetOurCurrentDoc();
  if (doc) {
    doc->UnblockOnload(false);
  }
  mBlockingOnload = false;

  return NS_OK;
}

nsIDocument*
nsBulletFrame::GetOurCurrentDoc() const
{
  nsIContent* parentContent = GetParent()->GetContent();
  return parentContent ? parentContent->GetComposedDoc()
                       : nullptr;
}

nsresult
nsBulletFrame::OnSizeAvailable(imgIRequest* aRequest, imgIContainer* aImage)
{
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
    nsIPresShell *shell = presContext->GetPresShell();
    if (shell) {
      shell->FrameNeedsReflow(this, nsIPresShell::eStyleChange,
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

void
nsBulletFrame::GetLoadGroup(nsPresContext *aPresContext, nsILoadGroup **aLoadGroup)
{
  if (!aPresContext)
    return;

  NS_PRECONDITION(nullptr != aLoadGroup, "null OUT parameter pointer");

  nsIPresShell *shell = aPresContext->GetPresShell();

  if (!shell)
    return;

  nsIDocument *doc = shell->GetDocument();
  if (!doc)
    return;

  *aLoadGroup = doc->GetDocumentLoadGroup().take();
}

float
nsBulletFrame::GetFontSizeInflation() const
{
  if (!HasFontSizeInflation()) {
    return 1.0f;
  }
  return GetProperty(FontSizeInflationProperty());
}

void
nsBulletFrame::SetFontSizeInflation(float aInflation)
{
  if (aInflation == 1.0f) {
    if (HasFontSizeInflation()) {
      RemoveStateBits(BULLET_FRAME_HAS_FONT_INFLATION);
      DeleteProperty(FontSizeInflationProperty());
    }
    return;
  }

  AddStateBits(BULLET_FRAME_HAS_FONT_INFLATION);
  SetProperty(FontSizeInflationProperty(), aInflation);
}

already_AddRefed<imgIContainer>
nsBulletFrame::GetImage() const
{
  if (mImageRequest && StyleList()->GetListStyleImage()) {
    nsCOMPtr<imgIContainer> imageCon;
    mImageRequest->GetImage(getter_AddRefs(imageCon));
    return imageCon.forget();
  }

  return nullptr;
}

nscoord
nsBulletFrame::GetLogicalBaseline(WritingMode aWritingMode) const
{
  nscoord ascent = 0, baselinePadding;
  if (GetStateBits() & BULLET_FRAME_IMAGE_LOADING) {
    ascent = BSize(aWritingMode);
  } else {
    RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetFontMetricsForFrame(this, GetFontSizeInflation());
    CounterStyle* listStyleType = StyleList()->mCounterStyle;
    switch (listStyleType->GetStyle()) {
      case NS_STYLE_LIST_STYLE_NONE:
        break;

      case NS_STYLE_LIST_STYLE_DISC:
      case NS_STYLE_LIST_STYLE_CIRCLE:
      case NS_STYLE_LIST_STYLE_SQUARE:
        ascent = fm->MaxAscent();
        baselinePadding = NSToCoordRound(float(ascent) / 8.0f);
        ascent = std::max(nsPresContext::CSSPixelsToAppUnits(MIN_BULLET_SIZE),
                        NSToCoordRound(0.8f * (float(ascent) / 2.0f)));
        ascent += baselinePadding;
        break;

      case NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED:
      case NS_STYLE_LIST_STYLE_DISCLOSURE_OPEN:
        ascent = fm->EmAscent();
        baselinePadding = NSToCoordRound(0.125f * ascent);
        ascent = std::max(
            nsPresContext::CSSPixelsToAppUnits(MIN_BULLET_SIZE),
            NSToCoordRound(0.75f * ascent));
        ascent += baselinePadding;
        break;

      default:
        ascent = fm->MaxAscent();
        break;
    }
  }
  return ascent +
    GetLogicalUsedMargin(aWritingMode).BStart(aWritingMode);
}

void
nsBulletFrame::GetSpokenText(nsAString& aText)
{
  CounterStyle* style = StyleList()->mCounterStyle;
  bool isBullet;
  style->GetSpokenCounterText(mOrdinal, GetWritingMode(), aText, isBullet);
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

void
nsBulletFrame::RegisterImageRequest(bool aKnownToBeAnimated)
{
  if (mImageRequest) {
    // mRequestRegistered is a bitfield; unpack it temporarily so we can take
    // the address.
    bool isRequestRegistered = mRequestRegistered;

    if (aKnownToBeAnimated) {
      nsLayoutUtils::RegisterImageRequest(PresContext(), mImageRequest,
                                          &isRequestRegistered);
    } else {
      nsLayoutUtils::RegisterImageRequestIfAnimated(PresContext(),
                                                    mImageRequest,
                                                    &isRequestRegistered);
    }

    isRequestRegistered = mRequestRegistered;
  }
}


void
nsBulletFrame::DeregisterAndCancelImageRequest()
{
  if (mImageRequest) {
    // mRequestRegistered is a bitfield; unpack it temporarily so we can take
    // the address.
    bool isRequestRegistered = mRequestRegistered;

    // Deregister our image request from the refresh driver.
    nsLayoutUtils::DeregisterImageRequest(PresContext(),
                                          mImageRequest,
                                          &isRequestRegistered);

    isRequestRegistered = mRequestRegistered;

    // Unblock onload if we blocked it.
    if (mBlockingOnload) {
      nsIDocument* doc = GetOurCurrentDoc();
      if (doc) {
        doc->UnblockOnload(false);
      }
      mBlockingOnload = false;
    }

    // Cancel the image request and forget about it.
    mImageRequest->CancelAndForgetObserver(NS_ERROR_FAILURE);
    mImageRequest = nullptr;
  }
}






NS_IMPL_ISUPPORTS(nsBulletListener, imgINotificationObserver)

nsBulletListener::nsBulletListener() :
  mFrame(nullptr)
{
}

nsBulletListener::~nsBulletListener()
{
}

NS_IMETHODIMP
nsBulletListener::Notify(imgIRequest *aRequest, int32_t aType, const nsIntRect* aData)
{
  if (!mFrame) {
    return NS_ERROR_FAILURE;
  }
  return mFrame->Notify(aRequest, aType, aData);
}

NS_IMETHODIMP
nsBulletListener::BlockOnload(imgIRequest* aRequest)
{
  if (!mFrame) {
    return NS_ERROR_FAILURE;
  }
  return mFrame->BlockOnload(aRequest);
}

NS_IMETHODIMP
nsBulletListener::UnblockOnload(imgIRequest* aRequest)
{
  if (!mFrame) {
    return NS_ERROR_FAILURE;
  }
  return mFrame->UnblockOnload(aRequest);
}
