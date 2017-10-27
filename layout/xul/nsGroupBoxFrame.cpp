/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// YY need to pass isMultiple before create called

#include "nsBoxFrame.h"

#include "gfxContext.h"
#include "mozilla/gfx/2D.h"
#include "nsCSSRendering.h"
#include "nsLayoutUtils.h"
#include "nsStyleContext.h"
#include "nsDisplayList.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

class nsGroupBoxFrame final : public nsBoxFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS(nsGroupBoxFrame)

  explicit nsGroupBoxFrame(nsStyleContext* aContext):
    nsBoxFrame(aContext, kClassID) {}

  virtual nsresult GetXULBorderAndPadding(nsMargin& aBorderAndPadding) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsDisplayListSet& aLists) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("GroupBoxFrame"), aResult);
  }
#endif

  virtual bool HonorPrintBackgroundSettings() override { return false; }

  DrawResult PaintBorder(gfxContext& aRenderingContext,
                                   nsPoint aPt,
                                   const nsRect& aDirtyRect);
  nsRect GetBackgroundRectRelativeToSelf(nscoord* aOutYOffset = nullptr, nsRect* aOutGroupRect = nullptr);

  // make sure we our kids get our orient and align instead of us.
  // our child box has no content node so it will search for a parent with one.
  // that will be us.
  virtual void GetInitialOrientation(bool& aHorizontal) override { aHorizontal = false; }
  virtual bool GetInitialHAlignment(Halignment& aHalign) override { aHalign = hAlign_Left; return true; }
  virtual bool GetInitialVAlignment(Valignment& aValign) override { aValign = vAlign_Top; return true; }
  virtual bool GetInitialAutoStretch(bool& aStretch) override { aStretch = true; return true; }

  nsIFrame* GetCaptionBox(nsRect& aCaptionRect);
};

/*
class nsGroupBoxInnerFrame : public nsBoxFrame {
public:

    nsGroupBoxInnerFrame(nsIPresShell* aShell, nsStyleContext* aContext):
      nsBoxFrame(aShell, aContext) {}


#ifdef DEBUG_FRAME_DUMP
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("GroupBoxFrameInner", aResult);
  }
#endif

  // we are always flexible
  virtual bool GetDefaultFlex(int32_t& aFlex) { aFlex = 1; return true; }

};
*/

nsIFrame*
NS_NewGroupBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsGroupBoxFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsGroupBoxFrame)

class nsDisplayXULGroupBorder final : public nsDisplayItem
{
public:
  nsDisplayXULGroupBorder(nsDisplayListBuilder* aBuilder,
                              nsGroupBoxFrame* aFrame) :
    nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayXULGroupBorder);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayXULGroupBorder() {
    MOZ_COUNT_DTOR(nsDisplayXULGroupBorder);
  }
#endif

  nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override;
  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion *aInvalidRegion) const override;
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) override {
    aOutFrames->AppendElement(mFrame);
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     gfxContext* aCtx) override;
  NS_DISPLAY_DECL_NAME("XULGroupBackground", TYPE_XUL_GROUP_BACKGROUND)
};

nsDisplayItemGeometry*
nsDisplayXULGroupBorder::AllocateGeometry(nsDisplayListBuilder* aBuilder)
{
  return new nsDisplayItemGenericImageGeometry(this, aBuilder);
}

void
nsDisplayXULGroupBorder::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                                   const nsDisplayItemGeometry* aGeometry,
                                                   nsRegion* aInvalidRegion) const
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
nsDisplayXULGroupBorder::Paint(nsDisplayListBuilder* aBuilder,
                                   gfxContext* aCtx)
{
  DrawResult result = static_cast<nsGroupBoxFrame*>(mFrame)
    ->PaintBorder(*aCtx, ToReferenceFrame(), mVisibleRect);

  nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
}

void
nsGroupBoxFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                  const nsDisplayListSet& aLists)
{
  // Paint our background and border
  if (IsVisibleForPainting(aBuilder)) {
    nsDisplayBackgroundImage::AppendBackgroundItemsToTop(
      aBuilder, this, GetBackgroundRectRelativeToSelf(),
      aLists.BorderBackground());
    aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
      nsDisplayXULGroupBorder(aBuilder, this));

    DisplayOutline(aBuilder, aLists);
  }

  BuildDisplayListForChildren(aBuilder, aLists);
}

nsRect
nsGroupBoxFrame::GetBackgroundRectRelativeToSelf(nscoord* aOutYOffset, nsRect* aOutGroupRect)
{
  const nsMargin& border = StyleBorder()->GetComputedBorder();

  nsRect groupRect;
  nsIFrame* groupBox = GetCaptionBox(groupRect);

  nscoord yoff = 0;
  if (groupBox) {
    // If the border is smaller than the legend, move the border down
    // to be centered on the legend.
    nsMargin groupMargin;
    groupBox->StyleMargin()->GetMargin(groupMargin);
    groupRect.Inflate(groupMargin);

    if (border.top < groupRect.height) {
      yoff = (groupRect.height - border.top) / 2 + groupRect.y;
    }
  }

  if (aOutYOffset) {
    *aOutYOffset = yoff;
  }
  if (aOutGroupRect) {
    *aOutGroupRect = groupRect;
  }

  return nsRect(0, yoff, mRect.width, mRect.height - yoff);
}

DrawResult
nsGroupBoxFrame::PaintBorder(gfxContext& aRenderingContext,
    nsPoint aPt, const nsRect& aDirtyRect) {

  DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();

  Sides skipSides;
  const nsStyleBorder* borderStyleData = StyleBorder();
  const nsMargin& border = borderStyleData->GetComputedBorder();
  nsPresContext* presContext = PresContext();

  nsRect groupRect;
  nsIFrame* groupBox = GetCaptionBox(groupRect);

  nscoord yoff = 0;
  nsRect rect = GetBackgroundRectRelativeToSelf(&yoff, &groupRect) + aPt;
  groupRect += aPt;

  DrawResult result = DrawResult::SUCCESS;
  if (groupBox) {
    int32_t appUnitsPerDevPixel = PresContext()->AppUnitsPerDevPixel();

    // we should probably use PaintBorderEdges to do this but for now just use clipping
    // to achieve the same effect.

    // draw left side
    nsRect clipRect(rect);
    clipRect.width = groupRect.x - rect.x;
    clipRect.height = border.top;

    aRenderingContext.Save();
    aRenderingContext.Clip(
      NSRectToSnappedRect(clipRect, appUnitsPerDevPixel, *drawTarget));
    result &=
      nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                  aDirtyRect, rect, mStyleContext,
                                  PaintBorderFlags::SYNC_DECODE_IMAGES, skipSides);
    aRenderingContext.Restore();

    // draw right side
    clipRect = rect;
    clipRect.x = groupRect.XMost();
    clipRect.width = rect.XMost() - groupRect.XMost();
    clipRect.height = border.top;

    aRenderingContext.Save();
    aRenderingContext.Clip(
      NSRectToSnappedRect(clipRect, appUnitsPerDevPixel, *drawTarget));
    result &=
      nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                  aDirtyRect, rect, mStyleContext,
                                  PaintBorderFlags::SYNC_DECODE_IMAGES, skipSides);

    aRenderingContext.Restore();
    // draw bottom

    clipRect = rect;
    clipRect.y += border.top;
    clipRect.height = mRect.height - (yoff + border.top);

    aRenderingContext.Save();
    aRenderingContext.Clip(
      NSRectToSnappedRect(clipRect, appUnitsPerDevPixel, *drawTarget));
    result &=
      nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                  aDirtyRect, rect, mStyleContext,
                                  PaintBorderFlags::SYNC_DECODE_IMAGES, skipSides);

    aRenderingContext.Restore();
  } else {
    result &=
      nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                  aDirtyRect, nsRect(aPt, GetSize()),
                                  mStyleContext,
                                  PaintBorderFlags::SYNC_DECODE_IMAGES, skipSides);
  }

  return result;
}

nsIFrame*
nsGroupBoxFrame::GetCaptionBox(nsRect& aCaptionRect)
{
    // first child is our grouped area
    nsIFrame* box = nsBox::GetChildXULBox(this);

    // no area fail.
    if (!box)
      return nullptr;

    // get the first child in the grouped area, that is the caption
    box = nsBox::GetChildXULBox(box);

    // nothing in the area? fail
    if (!box)
      return nullptr;

    // now get the caption itself. It is in the caption frame.
    nsIFrame* child = nsBox::GetChildXULBox(box);

    if (child) {
       // convert to our coordinates.
       nsRect parentRect(box->GetRect());
       aCaptionRect = child->GetRect();
       aCaptionRect.x += parentRect.x;
       aCaptionRect.y += parentRect.y;
    }

    return child;
}

nsresult
nsGroupBoxFrame::GetXULBorderAndPadding(nsMargin& aBorderAndPadding)
{
  aBorderAndPadding.SizeTo(0,0,0,0);
  return NS_OK;
}

