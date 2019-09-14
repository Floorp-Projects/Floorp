/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsButtonFrameRenderer.h"
#include "nsCSSRendering.h"
#include "nsPresContext.h"
#include "nsPresContextInlines.h"
#include "nsGkAtoms.h"
#include "nsCSSPseudoElements.h"
#include "nsNameSpaceManager.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/Unused.h"
#include "nsDisplayList.h"
#include "nsITheme.h"
#include "nsFrame.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/Element.h"
#include "Layers.h"

#include "gfxUtils.h"
#include "mozilla/layers/RenderRootStateManager.h"

#define ACTIVE "active"
#define HOVER "hover"
#define FOCUS "focus"

using namespace mozilla;
using namespace mozilla::image;
using namespace mozilla::layers;

nsButtonFrameRenderer::nsButtonFrameRenderer() : mFrame(nullptr) {
  MOZ_COUNT_CTOR(nsButtonFrameRenderer);
}

nsButtonFrameRenderer::~nsButtonFrameRenderer() {
  MOZ_COUNT_DTOR(nsButtonFrameRenderer);
}

void nsButtonFrameRenderer::SetFrame(nsFrame* aFrame,
                                     nsPresContext* aPresContext) {
  mFrame = aFrame;
  ReResolveStyles(aPresContext);
}

nsIFrame* nsButtonFrameRenderer::GetFrame() { return mFrame; }

void nsButtonFrameRenderer::SetDisabled(bool aDisabled, bool aNotify) {
  Element* element = mFrame->GetContent()->AsElement();
  if (aDisabled)
    element->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled, EmptyString(),
                     aNotify);
  else
    element->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, aNotify);
}

bool nsButtonFrameRenderer::isDisabled() {
  return mFrame->GetContent()->AsElement()->State().HasState(
      NS_EVENT_STATE_DISABLED);
}

class nsDisplayButtonBoxShadowOuter : public nsPaintedDisplayItem {
 public:
  nsDisplayButtonBoxShadowOuter(nsDisplayListBuilder* aBuilder,
                                nsIFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayButtonBoxShadowOuter);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayButtonBoxShadowOuter() {
    MOZ_COUNT_DTOR(nsDisplayButtonBoxShadowOuter);
  }
#endif

  virtual bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;

  bool CanBuildWebRenderDisplayItems();

  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  NS_DISPLAY_DECL_NAME("ButtonBoxShadowOuter", TYPE_BUTTON_BOX_SHADOW_OUTER)
};

nsRect nsDisplayButtonBoxShadowOuter::GetBounds(nsDisplayListBuilder* aBuilder,
                                                bool* aSnap) const {
  *aSnap = false;
  return mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
}

void nsDisplayButtonBoxShadowOuter::Paint(nsDisplayListBuilder* aBuilder,
                                          gfxContext* aCtx) {
  nsRect frameRect = nsRect(ToReferenceFrame(), mFrame->GetSize());

  nsCSSRendering::PaintBoxShadowOuter(mFrame->PresContext(), *aCtx, mFrame,
                                      frameRect, GetPaintRect());
}

bool nsDisplayButtonBoxShadowOuter::CanBuildWebRenderDisplayItems() {
  // FIXME(emilio): Is this right? That doesn't make much sense.
  if (mFrame->StyleEffects()->mBoxShadow.IsEmpty()) {
    return false;
  }

  bool hasBorderRadius;
  bool nativeTheme =
      nsCSSRendering::HasBoxShadowNativeTheme(mFrame, hasBorderRadius);

  // We don't support native themed things yet like box shadows around
  // input buttons.
  if (nativeTheme) {
    return false;
  }

  return true;
}

bool nsDisplayButtonBoxShadowOuter::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  if (!CanBuildWebRenderDisplayItems()) {
    return false;
  }
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  nsRect shadowRect = nsRect(ToReferenceFrame(), mFrame->GetSize());
  LayoutDeviceRect deviceBox =
      LayoutDeviceRect::FromAppUnits(shadowRect, appUnitsPerDevPixel);
  wr::LayoutRect deviceBoxRect = wr::ToLayoutRect(deviceBox);

  LayoutDeviceRect clipRect =
      LayoutDeviceRect::FromAppUnits(GetPaintRect(), appUnitsPerDevPixel);
  wr::LayoutRect deviceClipRect = wr::ToLayoutRect(clipRect);

  bool hasBorderRadius;
  Unused << nsCSSRendering::HasBoxShadowNativeTheme(mFrame, hasBorderRadius);

  LayoutDeviceSize zeroSize;
  wr::BorderRadius borderRadius =
      wr::ToBorderRadius(zeroSize, zeroSize, zeroSize, zeroSize);
  if (hasBorderRadius) {
    mozilla::gfx::RectCornerRadii borderRadii;
    hasBorderRadius = nsCSSRendering::GetBorderRadii(shadowRect, shadowRect,
                                                     mFrame, borderRadii);
    if (hasBorderRadius) {
      borderRadius = wr::ToBorderRadius(
          LayoutDeviceSize::FromUnknownSize(borderRadii.TopLeft()),
          LayoutDeviceSize::FromUnknownSize(borderRadii.TopRight()),
          LayoutDeviceSize::FromUnknownSize(borderRadii.BottomLeft()),
          LayoutDeviceSize::FromUnknownSize(borderRadii.BottomRight()));
    }
  }

  const Span<const StyleBoxShadow> shadows =
      mFrame->StyleEffects()->mBoxShadow.AsSpan();
  MOZ_ASSERT(!shadows.IsEmpty());

  for (const StyleBoxShadow& shadow : Reversed(shadows)) {
    if (shadow.inset) {
      continue;
    }
    float blurRadius =
        float(shadow.base.blur.ToAppUnits()) / float(appUnitsPerDevPixel);
    gfx::Color shadowColor =
        nsCSSRendering::GetShadowColor(shadow.base, mFrame, 1.0);

    LayoutDevicePoint shadowOffset = LayoutDevicePoint::FromAppUnits(
        nsPoint(shadow.base.horizontal.ToAppUnits(),
                shadow.base.vertical.ToAppUnits()),
        appUnitsPerDevPixel);

    float spreadRadius =
        float(shadow.spread.ToAppUnits()) / float(appUnitsPerDevPixel);

    aBuilder.PushBoxShadow(deviceBoxRect, deviceClipRect, !BackfaceIsHidden(),
                           deviceBoxRect, wr::ToLayoutVector2D(shadowOffset),
                           wr::ToColorF(shadowColor), blurRadius, spreadRadius,
                           borderRadius, wr::BoxShadowClipMode::Outset);
  }
  return true;
}

class nsDisplayButtonBorder final : public nsPaintedDisplayItem {
 public:
  nsDisplayButtonBorder(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                        nsButtonFrameRenderer* aRenderer)
      : nsPaintedDisplayItem(aBuilder, aFrame), mBFR(aRenderer) {
    MOZ_COUNT_CTOR(nsDisplayButtonBorder);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayButtonBorder() { MOZ_COUNT_DTOR(nsDisplayButtonBorder); }
#endif
  virtual bool MustPaintOnContentSide() const override { return true; }

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*>* aOutFrames) override {
    aOutFrames->AppendElement(mFrame);
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  virtual nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override;
  virtual void ComputeInvalidationRegion(
      nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
      nsRegion* aInvalidRegion) const override;
  virtual bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  NS_DISPLAY_DECL_NAME("ButtonBorderBackground", TYPE_BUTTON_BORDER_BACKGROUND)
 private:
  nsButtonFrameRenderer* mBFR;
};

nsDisplayItemGeometry* nsDisplayButtonBorder::AllocateGeometry(
    nsDisplayListBuilder* aBuilder) {
  return new nsDisplayItemGenericImageGeometry(this, aBuilder);
}

bool nsDisplayButtonBorder::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  // This is really a combination of paint box shadow inner +
  // paint border.
  const nsRect buttonRect = nsRect(ToReferenceFrame(), mFrame->GetSize());
  bool snap;
  nsRegion visible = GetBounds(aDisplayListBuilder, &snap);
  nsDisplayBoxShadowInner::CreateInsetBoxShadowWebRenderCommands(
      aBuilder, aSc, visible, mFrame, buttonRect);

  bool borderIsEmpty = false;
  Maybe<nsCSSBorderRenderer> br = nsCSSRendering::CreateBorderRenderer(
      mFrame->PresContext(), nullptr, mFrame, nsRect(),
      nsRect(ToReferenceFrame(), mFrame->GetSize()), mFrame->Style(),
      &borderIsEmpty, mFrame->GetSkipSides());
  if (!br) {
    return borderIsEmpty;
  }

  br->CreateWebRenderCommands(this, aBuilder, aResources, aSc);

  return true;
}

void nsDisplayButtonBorder::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  auto geometry =
      static_cast<const nsDisplayItemGenericImageGeometry*>(aGeometry);

  if (aBuilder->ShouldSyncDecodeImages() &&
      geometry->ShouldInvalidateToSyncDecodeImages()) {
    bool snap;
    aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
  }

  nsDisplayItem::ComputeInvalidationRegion(aBuilder, aGeometry, aInvalidRegion);
}

void nsDisplayButtonBorder::Paint(nsDisplayListBuilder* aBuilder,
                                  gfxContext* aCtx) {
  NS_ASSERTION(mFrame, "No frame?");
  nsPresContext* pc = mFrame->PresContext();
  nsRect r = nsRect(ToReferenceFrame(), mFrame->GetSize());

  // draw the border and background inside the focus and outline borders
  ImgDrawResult result =
      mBFR->PaintBorder(aBuilder, pc, *aCtx, GetPaintRect(), r);

  nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
}

nsRect nsDisplayButtonBorder::GetBounds(nsDisplayListBuilder* aBuilder,
                                        bool* aSnap) const {
  *aSnap = false;
  return aBuilder->IsForEventDelivery()
             ? nsRect(ToReferenceFrame(), mFrame->GetSize())
             : mFrame->GetVisualOverflowRectRelativeToSelf() +
                   ToReferenceFrame();
}

class nsDisplayButtonForeground final : public nsPaintedDisplayItem {
 public:
  nsDisplayButtonForeground(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                            nsButtonFrameRenderer* aRenderer)
      : nsPaintedDisplayItem(aBuilder, aFrame), mBFR(aRenderer) {
    MOZ_COUNT_CTOR(nsDisplayButtonForeground);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayButtonForeground() {
    MOZ_COUNT_DTOR(nsDisplayButtonForeground);
  }
#endif

  nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override;
  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  virtual bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  NS_DISPLAY_DECL_NAME("ButtonForeground", TYPE_BUTTON_FOREGROUND)
 private:
  nsButtonFrameRenderer* mBFR;
};

nsDisplayItemGeometry* nsDisplayButtonForeground::AllocateGeometry(
    nsDisplayListBuilder* aBuilder) {
  return new nsDisplayItemGenericImageGeometry(this, aBuilder);
}

void nsDisplayButtonForeground::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
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
                                      gfxContext* aCtx) {
  nsPresContext* presContext = mFrame->PresContext();
  const nsStyleDisplay* disp = mFrame->StyleDisplay();
  if (!mFrame->IsThemed(disp) ||
      !presContext->GetTheme()->ThemeDrawsFocusForWidget(disp->mAppearance)) {
    nsRect r = nsRect(ToReferenceFrame(), mFrame->GetSize());

    // Draw the -moz-focus-inner border
    ImgDrawResult result = mBFR->PaintInnerFocusBorder(
        aBuilder, presContext, *aCtx, GetPaintRect(), r);

    nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
  }
}

bool nsDisplayButtonForeground::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  Maybe<nsCSSBorderRenderer> br;
  bool borderIsEmpty = false;
  nsPresContext* presContext = mFrame->PresContext();
  const nsStyleDisplay* disp = mFrame->StyleDisplay();
  if (!mFrame->IsThemed(disp) ||
      !presContext->GetTheme()->ThemeDrawsFocusForWidget(disp->mAppearance)) {
    nsRect r = nsRect(ToReferenceFrame(), mFrame->GetSize());
    br = mBFR->CreateInnerFocusBorderRenderer(aDisplayListBuilder, presContext,
                                              nullptr, GetPaintRect(), r,
                                              &borderIsEmpty);
  }

  if (!br) {
    return borderIsEmpty;
  }

  br->CreateWebRenderCommands(this, aBuilder, aResources, aSc);
  return true;
}

nsresult nsButtonFrameRenderer::DisplayButton(nsDisplayListBuilder* aBuilder,
                                              nsDisplayList* aBackground,
                                              nsDisplayList* aForeground) {
  if (!mFrame->StyleEffects()->mBoxShadow.IsEmpty()) {
    aBackground->AppendNewToTop<nsDisplayButtonBoxShadowOuter>(aBuilder,
                                                               GetFrame());
  }

  nsRect buttonRect =
      mFrame->GetRectRelativeToSelf() + aBuilder->ToReferenceFrame(mFrame);

  nsDisplayBackgroundImage::AppendBackgroundItemsToTop(aBuilder, mFrame,
                                                       buttonRect, aBackground);

  aBackground->AppendNewToTop<nsDisplayButtonBorder>(aBuilder, GetFrame(),
                                                     this);

  // Only display focus rings if we actually have them. Since at most one
  // button would normally display a focus ring, most buttons won't have them.
  if (mInnerFocusStyle && mInnerFocusStyle->StyleBorder()->HasBorder()) {
    aForeground->AppendNewToTop<nsDisplayButtonForeground>(aBuilder, GetFrame(),
                                                           this);
  }
  return NS_OK;
}

void nsButtonFrameRenderer::GetButtonInnerFocusRect(const nsRect& aRect,
                                                    nsRect& aResult) {
  aResult = aRect;
  aResult.Deflate(mFrame->GetUsedBorderAndPadding());

  if (mInnerFocusStyle) {
    nsMargin innerFocusPadding(0, 0, 0, 0);
    mInnerFocusStyle->StylePadding()->GetPadding(innerFocusPadding);

    nsMargin framePadding = mFrame->GetUsedPadding();

    innerFocusPadding.top = std::min(innerFocusPadding.top, framePadding.top);
    innerFocusPadding.right =
        std::min(innerFocusPadding.right, framePadding.right);
    innerFocusPadding.bottom =
        std::min(innerFocusPadding.bottom, framePadding.bottom);
    innerFocusPadding.left =
        std::min(innerFocusPadding.left, framePadding.left);

    aResult.Inflate(innerFocusPadding);
  }
}

ImgDrawResult nsButtonFrameRenderer::PaintInnerFocusBorder(
    nsDisplayListBuilder* aBuilder, nsPresContext* aPresContext,
    gfxContext& aRenderingContext, const nsRect& aDirtyRect,
    const nsRect& aRect) {
  // we draw the -moz-focus-inner border just inside the button's
  // normal border and padding, to match Windows themes.

  nsRect rect;

  PaintBorderFlags flags = aBuilder->ShouldSyncDecodeImages()
                               ? PaintBorderFlags::SyncDecodeImages
                               : PaintBorderFlags();

  ImgDrawResult result = ImgDrawResult::SUCCESS;

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
    nsDisplayListBuilder* aBuilder, nsPresContext* aPresContext,
    gfxContext* aRenderingContext, const nsRect& aDirtyRect,
    const nsRect& aRect, bool* aBorderIsEmpty) {
  if (mInnerFocusStyle) {
    nsRect rect;
    GetButtonInnerFocusRect(aRect, rect);

    gfx::DrawTarget* dt =
        aRenderingContext ? aRenderingContext->GetDrawTarget() : nullptr;
    return nsCSSRendering::CreateBorderRenderer(
        aPresContext, dt, mFrame, aDirtyRect, rect, mInnerFocusStyle,
        aBorderIsEmpty);
  }

  return Nothing();
}

ImgDrawResult nsButtonFrameRenderer::PaintBorder(nsDisplayListBuilder* aBuilder,
                                                 nsPresContext* aPresContext,
                                                 gfxContext& aRenderingContext,
                                                 const nsRect& aDirtyRect,
                                                 const nsRect& aRect) {
  // get the button rect this is inside the focus and outline rects
  nsRect buttonRect = aRect;
  ComputedStyle* context = mFrame->Style();

  PaintBorderFlags borderFlags = aBuilder->ShouldSyncDecodeImages()
                                     ? PaintBorderFlags::SyncDecodeImages
                                     : PaintBorderFlags();

  nsCSSRendering::PaintBoxShadowInner(aPresContext, aRenderingContext, mFrame,
                                      buttonRect);

  ImgDrawResult result =
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, mFrame,
                                  aDirtyRect, buttonRect, context, borderFlags);

  return result;
}

/**
 * Call this when styles change
 */
void nsButtonFrameRenderer::ReResolveStyles(nsPresContext* aPresContext) {
  // get all the styles
  ServoStyleSet* styleSet = aPresContext->StyleSet();

  // get styles assigned to -moz-focus-inner (ie dotted border on Windows)
  mInnerFocusStyle = styleSet->ProbePseudoElementStyle(
      *mFrame->GetContent()->AsElement(), PseudoStyleType::mozFocusInner,
      mFrame->Style());
}

ComputedStyle* nsButtonFrameRenderer::GetComputedStyle(int32_t aIndex) const {
  switch (aIndex) {
    case NS_BUTTON_RENDERER_FOCUS_INNER_CONTEXT_INDEX:
      return mInnerFocusStyle;
    default:
      return nullptr;
  }
}

void nsButtonFrameRenderer::SetComputedStyle(int32_t aIndex,
                                             ComputedStyle* aComputedStyle) {
  switch (aIndex) {
    case NS_BUTTON_RENDERER_FOCUS_INNER_CONTEXT_INDEX:
      mInnerFocusStyle = aComputedStyle;
      break;
  }
}
