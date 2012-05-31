/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// YY need to pass isMultiple before create called

#include "nsBoxFrame.h"
#include "nsCSSRendering.h"
#include "nsRenderingContext.h"
#include "nsStyleContext.h"
#include "nsDisplayList.h"

class nsGroupBoxFrame : public nsBoxFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  nsGroupBoxFrame(nsIPresShell* aShell, nsStyleContext* aContext):
    nsBoxFrame(aShell, aContext) {}

  NS_IMETHOD GetBorderAndPadding(nsMargin& aBorderAndPadding);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const {
    return MakeFrameName(NS_LITERAL_STRING("GroupBoxFrame"), aResult);
  }
#endif

  virtual bool HonorPrintBackgroundSettings() { return false; }

  void PaintBorderBackground(nsRenderingContext& aRenderingContext,
      nsPoint aPt, const nsRect& aDirtyRect);

  // make sure we our kids get our orient and align instead of us.
  // our child box has no content node so it will search for a parent with one.
  // that will be us.
  virtual void GetInitialOrientation(bool& aHorizontal) { aHorizontal = false; }
  virtual bool GetInitialHAlignment(Halignment& aHalign)  { aHalign = hAlign_Left; return true; } 
  virtual bool GetInitialVAlignment(Valignment& aValign)  { aValign = vAlign_Top; return true; } 
  virtual bool GetInitialAutoStretch(bool& aStretch)    { aStretch = true; return true; } 

  nsIBox* GetCaptionBox(nsPresContext* aPresContext, nsRect& aCaptionRect);
};

/*
class nsGroupBoxInnerFrame : public nsBoxFrame {
public:

    nsGroupBoxInnerFrame(nsIPresShell* aShell, nsStyleContext* aContext):
      nsBoxFrame(aShell, aContext) {}


#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("GroupBoxFrameInner", aResult);
  }
#endif
  
  // we are always flexible
  virtual bool GetDefaultFlex(PRInt32& aFlex) { aFlex = 1; return true; }

};
*/

nsIFrame*
NS_NewGroupBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsGroupBoxFrame(aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsGroupBoxFrame)

class nsDisplayXULGroupBackground : public nsDisplayItem {
public:
  nsDisplayXULGroupBackground(nsDisplayListBuilder* aBuilder,
                              nsGroupBoxFrame* aFrame) :
    nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayXULGroupBackground);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayXULGroupBackground() {
    MOZ_COUNT_DTOR(nsDisplayXULGroupBackground);
  }
#endif

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) {
    aOutFrames->AppendElement(mFrame);
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx);
  NS_DISPLAY_DECL_NAME("XULGroupBackground", TYPE_XUL_GROUP_BACKGROUND)
};

void
nsDisplayXULGroupBackground::Paint(nsDisplayListBuilder* aBuilder,
                                   nsRenderingContext* aCtx)
{
  static_cast<nsGroupBoxFrame*>(mFrame)->
    PaintBorderBackground(*aCtx, ToReferenceFrame(), mVisibleRect);
}

NS_IMETHODIMP
nsGroupBoxFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                  const nsRect&           aDirtyRect,
                                  const nsDisplayListSet& aLists)
{
  // Paint our background and border
  if (IsVisibleForPainting(aBuilder)) {
    nsresult rv = aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
        nsDisplayXULGroupBackground(aBuilder, this));
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = DisplayOutline(aBuilder, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return BuildDisplayListForChildren(aBuilder, aDirtyRect, aLists);
  // REVIEW: Debug borders now painted by nsFrame::BuildDisplayListForChild
}

void
nsGroupBoxFrame::PaintBorderBackground(nsRenderingContext& aRenderingContext,
    nsPoint aPt, const nsRect& aDirtyRect) {
  PRIntn skipSides = 0;
  const nsStyleBorder* borderStyleData = GetStyleBorder();
  const nsMargin& border = borderStyleData->GetComputedBorder();
  nscoord yoff = 0;
  nsPresContext* presContext = PresContext();

  nsRect groupRect;
  nsIBox* groupBox = GetCaptionBox(presContext, groupRect);

  if (groupBox) {        
    // if the border is smaller than the legend. Move the border down
    // to be centered on the legend. 
    nsMargin groupMargin;
    groupBox->GetStyleMargin()->GetMargin(groupMargin);
    groupRect.Inflate(groupMargin);
 
    if (border.top < groupRect.height)
        yoff = (groupRect.height - border.top)/2 + groupRect.y;
  }

  nsRect rect(aPt.x, aPt.y + yoff, mRect.width, mRect.height - yoff);

  groupRect += aPt;

  nsCSSRendering::PaintBackground(presContext, aRenderingContext, this,
                                  aDirtyRect, rect,
                                  nsCSSRendering::PAINTBG_SYNC_DECODE_IMAGES);

  if (groupBox) {

    // we should probably use PaintBorderEdges to do this but for now just use clipping
    // to achieve the same effect.

    // draw left side
    nsRect clipRect(rect);
    clipRect.width = groupRect.x - rect.x;
    clipRect.height = border.top;

    aRenderingContext.PushState();
    aRenderingContext.IntersectClip(clipRect);
    nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                aDirtyRect, rect, mStyleContext, skipSides);

    aRenderingContext.PopState();


    // draw right side
    clipRect = rect;
    clipRect.x = groupRect.XMost();
    clipRect.width = rect.XMost() - groupRect.XMost();
    clipRect.height = border.top;

    aRenderingContext.PushState();
    aRenderingContext.IntersectClip(clipRect);
    nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                aDirtyRect, rect, mStyleContext, skipSides);

    aRenderingContext.PopState();

    
  
    // draw bottom

    clipRect = rect;
    clipRect.y += border.top;
    clipRect.height = mRect.height - (yoff + border.top);
  
    aRenderingContext.PushState();
    aRenderingContext.IntersectClip(clipRect);
    nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                aDirtyRect, rect, mStyleContext, skipSides);

    aRenderingContext.PopState();
    
  } else {
    nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                aDirtyRect, nsRect(aPt, GetSize()),
                                mStyleContext, skipSides);
  }
}

nsIBox*
nsGroupBoxFrame::GetCaptionBox(nsPresContext* aPresContext, nsRect& aCaptionRect)
{
    // first child is our grouped area
    nsIBox* box = GetChildBox();

    // no area fail.
    if (!box)
      return nsnull;

    // get the first child in the grouped area, that is the caption
    box = box->GetChildBox();

    // nothing in the area? fail
    if (!box)
      return nsnull;

    // now get the caption itself. It is in the caption frame.
    nsIBox* child = box->GetChildBox();

    if (child) {
       // convert to our coordinates.
       nsRect parentRect(box->GetRect());
       aCaptionRect = child->GetRect();
       aCaptionRect.x += parentRect.x;
       aCaptionRect.y += parentRect.y;
    }

    return child;
}

NS_IMETHODIMP
nsGroupBoxFrame::GetBorderAndPadding(nsMargin& aBorderAndPadding)
{
  aBorderAndPadding.SizeTo(0,0,0,0);
  return NS_OK;
}

