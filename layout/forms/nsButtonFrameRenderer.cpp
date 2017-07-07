/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsButtonFrameRenderer.h"
#include "nsCSSRendering.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsCSSPseudoElements.h"
#include "nsNameSpaceManager.h"
#include "mozilla/StyleSetHandle.h"
#include "mozilla/StyleSetHandleInlines.h"
#include "nsDisplayList.h"
#include "nsITheme.h"
#include "nsFrame.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/Element.h"
#include "Layers.h"
#include "gfxPrefs.h"
#include "gfxUtils.h"
#include "mozilla/layers/WebRenderDisplayItemLayer.h"

#define ACTIVE   "active"
#define HOVER    "hover"
#define FOCUS    "focus"

using namespace mozilla;
using namespace mozilla::image;
using namespace mozilla::layers;

nsButtonFrameRenderer::nsButtonFrameRenderer()
{
  MOZ_COUNT_CTOR(nsButtonFrameRenderer);
}

nsButtonFrameRenderer::~nsButtonFrameRenderer()
{
  MOZ_COUNT_DTOR(nsButtonFrameRenderer);

#ifdef DEBUG
  if (mInnerFocusStyle) {
    mInnerFocusStyle->FrameRelease();
  }
#endif
}

void
nsButtonFrameRenderer::SetFrame(nsFrame* aFrame, nsPresContext* aPresContext)
{
  mFrame = aFrame;
  ReResolveStyles(aPresContext);
}

nsIFrame*
nsButtonFrameRenderer::GetFrame()
{
  return mFrame;
}

void
nsButtonFrameRenderer::SetDisabled(bool aDisabled, bool notify)
{
  if (aDisabled)
    mFrame->GetContent()->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled, EmptyString(),
                                  notify);
  else
    mFrame->GetContent()->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, notify);
}

bool
nsButtonFrameRenderer::isDisabled()
{
  return mFrame->GetContent()->AsElement()->
    State().HasState(NS_EVENT_STATE_DISABLED);
}

class nsDisplayButtonBoxShadowOuter : public nsDisplayItem {
public:
  nsDisplayButtonBoxShadowOuter(nsDisplayListBuilder* aBuilder,
                                nsButtonFrameRenderer* aRenderer)
    : nsDisplayItem(aBuilder, aRenderer->GetFrame()) {
    MOZ_COUNT_CTOR(nsDisplayButtonBoxShadowOuter);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayButtonBoxShadowOuter() {
    MOZ_COUNT_DTOR(nsDisplayButtonBoxShadowOuter);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     gfxContext* aCtx) override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) override;
  NS_DISPLAY_DECL_NAME("ButtonBoxShadowOuter", TYPE_BUTTON_BOX_SHADOW_OUTER)
};

nsRect
nsDisplayButtonBoxShadowOuter::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  *aSnap = false;
  return mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
}

void
nsDisplayButtonBoxShadowOuter::Paint(nsDisplayListBuilder* aBuilder,
                                     gfxContext* aCtx) {
  nsRect frameRect = nsRect(ToReferenceFrame(), mFrame->GetSize());

  nsCSSRendering::PaintBoxShadowOuter(mFrame->PresContext(), *aCtx, mFrame,
                                      frameRect, mVisibleRect);
}

class nsDisplayButtonBorder : public nsDisplayItem {
public:
  nsDisplayButtonBorder(nsDisplayListBuilder* aBuilder,
                                  nsButtonFrameRenderer* aRenderer)
    : nsDisplayItem(aBuilder, aRenderer->GetFrame()), mBFR(aRenderer) {
    MOZ_COUNT_CTOR(nsDisplayButtonBorder);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayButtonBorder() {
    MOZ_COUNT_DTOR(nsDisplayButtonBorder);
  }
#endif
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*> *aOutFrames) override {
    aOutFrames->AppendElement(mFrame);
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     gfxContext* aCtx) override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) override;
  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override;
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion *aInvalidRegion) override;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       const StackingContextHelper& aSc,
                                       nsTArray<WebRenderParentCommand>& aParentCommands,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;
  NS_DISPLAY_DECL_NAME("ButtonBorderBackground", TYPE_BUTTON_BORDER_BACKGROUND)
private:
  nsButtonFrameRenderer* mBFR;
  Maybe<nsCSSBorderRenderer> mBorderRenderer;
};

nsDisplayItemGeometry*
nsDisplayButtonBorder::AllocateGeometry(nsDisplayListBuilder* aBuilder)
{
  return new nsDisplayItemGenericImageGeometry(this, aBuilder);
}

LayerState
nsDisplayButtonBorder::GetLayerState(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     const ContainerLayerParameters& aParameters)
{
  if (ShouldUseAdvancedLayer(aManager, gfxPrefs::LayersAllowDisplayButtonBorder)) {
    // TODO: Figure out what to do with sync decode images
    if (aBuilder->ShouldSyncDecodeImages()) {
      return LAYER_NONE;
    }

    nsPoint offset = ToReferenceFrame();
    if (!nsDisplayBoxShadowInner::CanCreateWebRenderCommands(aBuilder,
                                                             mFrame,
                                                             offset)) {
      return LAYER_NONE;
    }

    Maybe<nsCSSBorderRenderer> br =
    nsCSSRendering::CreateBorderRenderer(mFrame->PresContext(),
                                         nullptr,
                                         mFrame,
                                         nsRect(),
                                         nsRect(offset, mFrame->GetSize()),
                                         mFrame->StyleContext(),
                                         mFrame->GetSkipSides());
    if (!br) {
      return LAYER_NONE;
    }

    if (!br->CanCreateWebRenderCommands()) {
      return LAYER_NONE;
    }

    mBorderRenderer = br;
    return LAYER_ACTIVE;
  }

  return LAYER_NONE;
}

already_AddRefed<Layer>
nsDisplayButtonBorder::BuildLayer(nsDisplayListBuilder* aBuilder,
                                  LayerManager* aManager,
                                  const ContainerLayerParameters& aContainerParameters)
{
  return BuildDisplayItemLayer(aBuilder, aManager, aContainerParameters);
}

bool
nsDisplayButtonBorder::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                               const StackingContextHelper& aSc,
                                               nsTArray<WebRenderParentCommand>& aParentCommands,
                                               mozilla::layers::WebRenderLayerManager* aManager,
                                               nsDisplayListBuilder* aDisplayListBuilder)
{
  if (aManager->IsLayersFreeTransaction()) {
    ContainerLayerParameters parameter;
    if (GetLayerState(aDisplayListBuilder, aManager, parameter) != LAYER_ACTIVE) {
      return false;
    }
  }

  MOZ_ASSERT(mBorderRenderer);

  // This is really a combination of paint box shadow inner +
  // paint border.
  nsRect buttonRect = nsRect(ToReferenceFrame(), mFrame->GetSize());
  bool snap;
  nsRegion visible = GetBounds(aDisplayListBuilder, &snap);
  nsDisplayBoxShadowInner::CreateInsetBoxShadowWebRenderCommands(aBuilder,
                                                                 aSc,
                                                                 visible,
                                                                 mFrame,
                                                                 buttonRect);
  mBorderRenderer->CreateWebRenderCommands(aBuilder, aSc);

  return true;
}

void
nsDisplayButtonBorder::ComputeInvalidationRegion(
  nsDisplayListBuilder* aBuilder,
  const nsDisplayItemGeometry* aGeometry,
  nsRegion *aInvalidRegion)
{
  auto geometry =
    static_cast<const nsDisplayItemGenericImageGeometry*>(aGeometry);

  if (aBuilder->ShouldSyncDecodeImages() &&
      geometry->ShouldInvalidateToSyncDecodeImages()) {
    bool snap;
    aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
  }

  nsDisplayItem::ComputeInvalidationRegion(aBuilder, aGeometry, aInvalidRegion);
}

void
nsDisplayButtonBorder::Paint(nsDisplayListBuilder* aBuilder,
                             gfxContext* aCtx)
{
  NS_ASSERTION(mFrame, "No frame?");
  nsPresContext* pc = mFrame->PresContext();
  nsRect r = nsRect(ToReferenceFrame(), mFrame->GetSize());

  // draw the border and background inside the focus and outline borders
  DrawResult result =
    mBFR->PaintBorder(aBuilder, pc, *aCtx, mVisibleRect, r);

  nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
}

nsRect
nsDisplayButtonBorder::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  *aSnap = false;
  return aBuilder->IsForEventDelivery() ? nsRect(ToReferenceFrame(), mFrame->GetSize())
          : mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
}

class nsDisplayButtonForeground : public nsDisplayItem {
public:
  nsDisplayButtonForeground(nsDisplayListBuilder* aBuilder,
                            nsButtonFrameRenderer* aRenderer)
    : nsDisplayItem(aBuilder, aRenderer->GetFrame()), mBFR(aRenderer) {
    MOZ_COUNT_CTOR(nsDisplayButtonForeground);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayButtonForeground() {
    MOZ_COUNT_DTOR(nsDisplayButtonForeground);
  }
#endif

  nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override;
  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion *aInvalidRegion) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     gfxContext* aCtx) override;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
   virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                        const StackingContextHelper& aSc,
                                        nsTArray<WebRenderParentCommand>& aParentCommands,
                                        mozilla::layers::WebRenderLayerManager* aManager,
                                        nsDisplayListBuilder* aDisplayListBuilder) override;
  NS_DISPLAY_DECL_NAME("ButtonForeground", TYPE_BUTTON_FOREGROUND)
private:
  nsButtonFrameRenderer* mBFR;
  Maybe<nsCSSBorderRenderer> mBorderRenderer;
};

nsDisplayItemGeometry*
nsDisplayButtonForeground::AllocateGeometry(nsDisplayListBuilder* aBuilder)
{
  return new nsDisplayItemGenericImageGeometry(this, aBuilder);
}

void
nsDisplayButtonForeground::ComputeInvalidationRegion(
  nsDisplayListBuilder* aBuilder,
  const nsDisplayItemGeometry* aGeometry,
  nsRegion* aInvalidRegion)
{
  auto geometry =
    static_cast<const nsDisplayItemGenericImageGeometry*>(aGeometry);

  if (aBuilder->ShouldSyncDecodeImages() &&
      geometry->ShouldInvalidateToSyncDecodeImages()) {
    bool snap;
    aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
  }

  nsDisplayItem::ComputeInvalidationRegion(aBuilder, aGeometry, aInvalidRegion);
}

void nsDisplayButtonForeground::Paint(nsDisplayListBuilder* aBuilder,
                                      gfxContext* aCtx)
{
  nsPresContext *presContext = mFrame->PresContext();
  const nsStyleDisplay *disp = mFrame->StyleDisplay();
  if (!mFrame->IsThemed(disp) ||
      !presContext->GetTheme()->ThemeDrawsFocusForWidget(disp->mAppearance)) {
    nsRect r = nsRect(ToReferenceFrame(), mFrame->GetSize());

    // Draw the -moz-focus-inner border
    DrawResult result =
      mBFR->PaintInnerFocusBorder(aBuilder, presContext, *aCtx, mVisibleRect, r);

    nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
  }
}

LayerState
nsDisplayButtonForeground::GetLayerState(nsDisplayListBuilder* aBuilder,
                                         LayerManager* aManager,
                                         const ContainerLayerParameters& aParameters)
{
  Maybe<nsCSSBorderRenderer> br;

  if (ShouldUseAdvancedLayer(aManager, gfxPrefs::LayersAllowButtonForegroundLayers)) {
    nsPresContext *presContext = mFrame->PresContext();
    const nsStyleDisplay *disp = mFrame->StyleDisplay();
    if (!mFrame->IsThemed(disp) ||
        !presContext->GetTheme()->ThemeDrawsFocusForWidget(disp->mAppearance)) {
      nsRect r = nsRect(ToReferenceFrame(), mFrame->GetSize());
      br = mBFR->CreateInnerFocusBorderRenderer(aBuilder, presContext, nullptr, mVisibleRect, r);
    }
  }

  if (!br || !br->CanCreateWebRenderCommands()) {
    return LAYER_NONE;
  }

  mBorderRenderer = br;

  return LAYER_ACTIVE;
}

already_AddRefed<mozilla::layers::Layer>
nsDisplayButtonForeground::BuildLayer(nsDisplayListBuilder* aBuilder,
                                      LayerManager* aManager,
                                      const ContainerLayerParameters& aContainerParameters)
{
  return BuildDisplayItemLayer(aBuilder, aManager, aContainerParameters);
}

bool
nsDisplayButtonForeground::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                   const StackingContextHelper& aSc,
                                                   nsTArray<WebRenderParentCommand>& aParentCommands,
                                                   mozilla::layers::WebRenderLayerManager* aManager,
                                                   nsDisplayListBuilder* aDisplayListBuilder)
{
  if (aManager->IsLayersFreeTransaction()) {
    ContainerLayerParameters parameter;
    if (GetLayerState(aDisplayListBuilder, aManager, parameter) != LAYER_ACTIVE) {
      return false;
    }
  }

  mBorderRenderer->CreateWebRenderCommands(aBuilder, aSc);
  return true;
}

nsresult
nsButtonFrameRenderer::DisplayButton(nsDisplayListBuilder* aBuilder,
                                     nsDisplayList* aBackground,
                                     nsDisplayList* aForeground)
{
  if (mFrame->StyleEffects()->mBoxShadow) {
    aBackground->AppendNewToTop(new (aBuilder)
      nsDisplayButtonBoxShadowOuter(aBuilder, this));
  }

  nsRect buttonRect = mFrame->GetRectRelativeToSelf();

  nsDisplayBackgroundImage::AppendBackgroundItemsToTop(
    aBuilder, mFrame, buttonRect, aBackground);

  aBackground->AppendNewToTop(new (aBuilder)
    nsDisplayButtonBorder(aBuilder, this));

  // Only display focus rings if we actually have them. Since at most one
  // button would normally display a focus ring, most buttons won't have them.
  if (mInnerFocusStyle && mInnerFocusStyle->StyleBorder()->HasBorder()) {
    aForeground->AppendNewToTop(new (aBuilder)
      nsDisplayButtonForeground(aBuilder, this));
  }
  return NS_OK;
}

void
nsButtonFrameRenderer::GetButtonInnerFocusRect(const nsRect& aRect, nsRect& aResult)
{
  aResult = aRect;
  aResult.Deflate(mFrame->GetUsedBorderAndPadding());

  nsMargin innerFocusPadding(0,0,0,0);
  if (mInnerFocusStyle) {
    mInnerFocusStyle->StylePadding()->GetPadding(innerFocusPadding);
  }
  aResult.Inflate(innerFocusPadding);
}

DrawResult
nsButtonFrameRenderer::PaintInnerFocusBorder(
  nsDisplayListBuilder* aBuilder,
  nsPresContext* aPresContext,
  gfxContext& aRenderingContext,
  const nsRect& aDirtyRect,
  const nsRect& aRect)
{
  // we draw the -moz-focus-inner border just inside the button's
  // normal border and padding, to match Windows themes.

  nsRect rect;

  PaintBorderFlags flags = aBuilder->ShouldSyncDecodeImages()
                         ? PaintBorderFlags::SYNC_DECODE_IMAGES
                         : PaintBorderFlags();

  DrawResult result = DrawResult::SUCCESS;

  if (mInnerFocusStyle) {
    GetButtonInnerFocusRect(aRect, rect);

    result &=
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, mFrame,
                                  aDirtyRect, rect, mInnerFocusStyle, flags);
  }

  return result;
}

Maybe<nsCSSBorderRenderer>
nsButtonFrameRenderer::CreateInnerFocusBorderRenderer(
  nsDisplayListBuilder* aBuilder,
  nsPresContext* aPresContext,
  gfxContext* aRenderingContext,
  const nsRect& aDirtyRect,
  const nsRect& aRect)
{
  if (mInnerFocusStyle) {
    nsRect rect;
    GetButtonInnerFocusRect(aRect, rect);

    gfx::DrawTarget* dt = aRenderingContext ? aRenderingContext->GetDrawTarget() : nullptr;
    return nsCSSRendering::CreateBorderRenderer(aPresContext,
                                                dt,
                                                mFrame,
                                                aDirtyRect,
                                                rect,
                                                mInnerFocusStyle);
  }

  return Nothing();
}

DrawResult
nsButtonFrameRenderer::PaintBorder(
  nsDisplayListBuilder* aBuilder,
  nsPresContext* aPresContext,
  gfxContext& aRenderingContext,
  const nsRect& aDirtyRect,
  const nsRect& aRect)
{
  // get the button rect this is inside the focus and outline rects
  nsRect buttonRect = aRect;
  nsStyleContext* context = mFrame->StyleContext();

  PaintBorderFlags borderFlags = aBuilder->ShouldSyncDecodeImages()
                               ? PaintBorderFlags::SYNC_DECODE_IMAGES
                               : PaintBorderFlags();

  nsCSSRendering::PaintBoxShadowInner(aPresContext, aRenderingContext,
                                      mFrame, buttonRect);

  DrawResult result =
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, mFrame,
                                aDirtyRect, buttonRect, context, borderFlags);

  return result;
}

/**
 * Call this when styles change
 */
void
nsButtonFrameRenderer::ReResolveStyles(nsPresContext* aPresContext)
{
  // get all the styles
  nsStyleContext* context = mFrame->StyleContext();
  StyleSetHandle styleSet = aPresContext->StyleSet();

#ifdef DEBUG
  if (mInnerFocusStyle) {
    mInnerFocusStyle->FrameRelease();
  }
#endif

  // get styles assigned to -moz-inner-focus (ie dotted border on Windows)
  mInnerFocusStyle =
    styleSet->ProbePseudoElementStyle(mFrame->GetContent()->AsElement(),
                                      CSSPseudoElementType::mozFocusInner,
                                      context);

#ifdef DEBUG
  if (mInnerFocusStyle) {
    mInnerFocusStyle->FrameAddRef();
  }
#endif
}

nsStyleContext*
nsButtonFrameRenderer::GetStyleContext(int32_t aIndex) const
{
  switch (aIndex) {
  case NS_BUTTON_RENDERER_FOCUS_INNER_CONTEXT_INDEX:
    return mInnerFocusStyle;
  default:
    return nullptr;
  }
}

void
nsButtonFrameRenderer::SetStyleContext(int32_t aIndex, nsStyleContext* aStyleContext)
{
  switch (aIndex) {
  case NS_BUTTON_RENDERER_FOCUS_INNER_CONTEXT_INDEX:
#ifdef DEBUG
    if (mInnerFocusStyle) {
      mInnerFocusStyle->FrameRelease();
    }
#endif
    mInnerFocusStyle = aStyleContext;
    break;
  }
#ifdef DEBUG
  aStyleContext->FrameAddRef();
#endif
}
