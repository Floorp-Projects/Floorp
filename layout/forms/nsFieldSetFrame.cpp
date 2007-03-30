/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// YY need to pass isMultiple before create called

//#include "nsFormControlFrame.h"
#include "nsHTMLContainerFrame.h"
#include "nsLegendFrame.h"
#include "nsIDOMNode.h"
#include "nsIDOMHTMLFieldSetElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsCSSRendering.h"
//#include "nsIDOMHTMLCollection.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsPresContext.h"
#include "nsFrameManager.h"
#include "nsHTMLParts.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsFont.h"
#include "nsCOMPtr.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIServiceManager.h"
#include "nsDisplayList.h"

class nsLegendFrame;

class nsFieldSetFrame : public nsHTMLContainerFrame {
public:

  nsFieldSetFrame(nsStyleContext* aContext);

  NS_IMETHOD SetInitialChildList(nsIAtom*       aListName,
                                 nsIFrame*      aChildList);

  NS_HIDDEN_(nscoord)
    GetLegendPrefWidth(nsIRenderingContext* aRenderingContext);
  NS_HIDDEN_(nscoord)
    GetContentMinWidth(nsIRenderingContext* aRenderingContext);
  virtual nscoord GetMinWidth(nsIRenderingContext* aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext* aRenderingContext);
  virtual nsSize ComputeSize(nsIRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             PRBool aShrinkWrap);

  NS_IMETHOD Reflow(nsPresContext*           aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
                               
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  void PaintBorderBackground(nsIRenderingContext& aRenderingContext,
    nsPoint aPt, const nsRect& aDirtyRect);

  NS_IMETHOD AppendFrames(nsIAtom*       aListName,
                          nsIFrame*      aFrameList);
  NS_IMETHOD InsertFrames(nsIAtom*       aListName,
                          nsIFrame*      aPrevFrame,
                          nsIFrame*      aFrameList);
  NS_IMETHOD RemoveFrame(nsIAtom*       aListName,
                         nsIFrame*      aOldFrame);

  virtual nsIAtom* GetType() const;
  virtual PRBool IsContainingBlock() const;

#ifdef ACCESSIBILITY  
  NS_IMETHOD  GetAccessible(nsIAccessible** aAccessible);
#endif

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const {
    return MakeFrameName(NS_LITERAL_STRING("FieldSet"), aResult);
  }
#endif

protected:

  virtual PRIntn GetSkipSides() const;
  nsIFrame* MaybeSetLegend(nsIFrame* aFrameList, nsIAtom* aListName);
  void ReParentFrameList(nsIFrame* aFrameList);

  nsIFrame* mLegendFrame;
  nsIFrame* mContentFrame;
  nsRect    mLegendRect;
  nscoord   mLegendSpace;
};

nsIFrame*
NS_NewFieldSetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsFieldSetFrame(aContext);
}

nsFieldSetFrame::nsFieldSetFrame(nsStyleContext* aContext)
  : nsHTMLContainerFrame(aContext)
{
  mContentFrame = nsnull;
  mLegendFrame  = nsnull;
  mLegendSpace  = 0;
}

nsIAtom*
nsFieldSetFrame::GetType() const
{
  return nsGkAtoms::fieldSetFrame;
}

PRBool
nsFieldSetFrame::IsContainingBlock() const
{
  return PR_TRUE;
}

NS_IMETHODIMP
nsFieldSetFrame::SetInitialChildList(nsIAtom*       aListName,
                                     nsIFrame*      aChildList)
{
  // Get the content and legend frames.
  if (aChildList->GetNextSibling()) {
    mContentFrame = aChildList->GetNextSibling();
    mLegendFrame  = aChildList;
  } else {
    mContentFrame = aChildList;
    mLegendFrame  = nsnull;
  }

  // Queue up the frames for the content frame
  return nsHTMLContainerFrame::SetInitialChildList(nsnull, aChildList);
}

class nsDisplayFieldSetBorderBackground : public nsDisplayItem {
public:
  nsDisplayFieldSetBorderBackground(nsFieldSetFrame* aFrame)
    : nsDisplayItem(aFrame) {
    MOZ_COUNT_CTOR(nsDisplayFieldSetBorderBackground);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayFieldSetBorderBackground() {
    MOZ_COUNT_DTOR(nsDisplayFieldSetBorderBackground);
  }
#endif

  virtual nsIFrame* HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt);
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("FieldSetBorderBackground")
};

nsIFrame* nsDisplayFieldSetBorderBackground::HitTest(nsDisplayListBuilder* aBuilder,
    nsPoint aPt)
{
  // aPt is guaranteed to be in this item's bounds. We do the hit test based on the
  // frame bounds even though our background doesn't cover the whole frame.
  // It's not clear whether this is correct.
  return mFrame;
}

void
nsDisplayFieldSetBorderBackground::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect)
{
  NS_STATIC_CAST(nsFieldSetFrame*, mFrame)->
    PaintBorderBackground(*aCtx, aBuilder->ToReferenceFrame(mFrame), aDirtyRect);
}

NS_IMETHODIMP
nsFieldSetFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                  const nsRect&           aDirtyRect,
                                  const nsDisplayListSet& aLists) {
  // Paint our background and border in a special way.
  // REVIEW: We don't really need to check frame emptiness here; if it's empty,
  // the background/border display item won't do anything, and if it isn't empty,
  // we need to paint the outline
  if (IsVisibleForPainting(aBuilder)) {
    // don't bother checking to see if we really have a border or background.
    // we usually will have a border.
    nsresult rv = aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
        nsDisplayFieldSetBorderBackground(this));
    NS_ENSURE_SUCCESS(rv, rv);
  
    rv = DisplayOutlineUnconditional(aBuilder, aLists);
    NS_ENSURE_SUCCESS(rv, rv);

    DO_GLOBAL_REFLOW_COUNT_DSP("nsFieldSetFrame");
  }

  nsDisplayListCollection contentDisplayItems;
  if (mContentFrame) {
    // Collect mContentFrame's display items into their own collection. We need
    // to be calling BuildDisplayList on mContentFrame before mLegendFrame in
    // case it contains out-of-flow frames whose placeholders are under
    // mLegendFrame. However, we want mContentFrame's display items to be
    // after mLegendFrame's display items in z-order, so we need to save them
    // and append them later.
    nsresult rv = BuildDisplayListForChild(aBuilder, mContentFrame, aDirtyRect,
                                           contentDisplayItems);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (mLegendFrame) {
    nsDisplayListSet set(aLists, aLists.Content());
    nsresult rv = BuildDisplayListForChild(aBuilder, mLegendFrame, aDirtyRect, set);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // Put mContentFrame's display items on the master list. Note that
  // this moves mContentFrame's border/background display items to our
  // BorderBackground() list, which isn't really correct, but it's OK because
  // mContentFrame is anonymous and can't have its own border and background.
  contentDisplayItems.MoveTo(aLists);
  return NS_OK;
}

void
nsFieldSetFrame::PaintBorderBackground(nsIRenderingContext& aRenderingContext,
    nsPoint aPt, const nsRect& aDirtyRect)
{
  PRIntn skipSides = GetSkipSides();
  const nsStyleBorder* borderStyle = GetStyleBorder();
  const nsStylePadding* paddingStyle = GetStylePadding();
       
  nscoord topBorder = borderStyle->GetBorderWidth(NS_SIDE_TOP);
  nscoord yoff = 0;
  nsPresContext* presContext = PresContext();
     
  // if the border is smaller than the legend. Move the border down
  // to be centered on the legend. 
  if (topBorder < mLegendRect.height)
    yoff = (mLegendRect.height - topBorder)/2;
      
  nsRect rect(aPt.x, aPt.y + yoff, mRect.width, mRect.height - yoff);

  nsCSSRendering::PaintBackground(presContext, aRenderingContext, this,
                                  aDirtyRect, rect, *borderStyle,
                                  *paddingStyle, PR_TRUE);

   if (mLegendFrame) {

    // Use the rect of the legend frame, not mLegendRect, so we draw our
    // border under the legend's left and right margins.
    nsRect legendRect = mLegendFrame->GetRect() + aPt;
    
    // we should probably use PaintBorderEdges to do this but for now just use clipping
    // to achieve the same effect.

    // draw left side
    nsRect clipRect(rect);
    clipRect.width = legendRect.x - rect.x;
    clipRect.height = topBorder;

    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect);
    nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                aDirtyRect, rect, *borderStyle, mStyleContext, skipSides);

    aRenderingContext.PopState();


    // draw right side
    clipRect = rect;
    clipRect.x = legendRect.XMost();
    clipRect.width = rect.XMost() - legendRect.XMost();
    clipRect.height = topBorder;

    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect);
    nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                aDirtyRect, rect, *borderStyle, mStyleContext, skipSides);

    aRenderingContext.PopState();

    
    // draw bottom
    clipRect = rect;
    clipRect.y += topBorder;
    clipRect.height = mRect.height - (yoff + topBorder);
    
    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect);
    nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                aDirtyRect, rect, *borderStyle, mStyleContext, skipSides);

    aRenderingContext.PopState();
  } else {

    nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                aDirtyRect,
                                nsRect(aPt, mRect.Size()),
                                *borderStyle, mStyleContext, skipSides);
  }
}

// XXXbz This duplicates code in nsGkAtoms (near IntrinsicForContainer)

static nscoord GetCoord(const nsStyleCoord& aCoord, nscoord aIfNotCoord)
{
  return aCoord.GetUnit() == eStyleUnit_Coord
           ? aCoord.GetCoordValue()
           : aIfNotCoord;
}

nscoord
nsFieldSetFrame::GetLegendPrefWidth(nsIRenderingContext* aRenderingContext)
{
  NS_ASSERTION(mLegendFrame, "Don't call me if there is no legend frame!");

  // We don't want to use nsLayoutUtils::IntrinsicForContainer,
  // because legends ignore their CSS-specified width.
  nscoord result = mLegendFrame->GetPrefWidth(aRenderingContext);

  nsStyleCoord tmp;

  const nsStylePadding *stylePadding = mLegendFrame->GetStylePadding();
  result += GetCoord(stylePadding->mPadding.GetLeft(tmp), 0);
  result += GetCoord(stylePadding->mPadding.GetRight(tmp), 0);

  const nsStyleBorder *styleBorder = mLegendFrame->GetStyleBorder();
  result += styleBorder->GetBorderWidth(NS_SIDE_LEFT);
  result += styleBorder->GetBorderWidth(NS_SIDE_RIGHT);

  const nsStyleMargin *styleMargin = mLegendFrame->GetStyleMargin();
  result += GetCoord(styleMargin->mMargin.GetLeft(tmp), 0);
  result += GetCoord(styleMargin->mMargin.GetRight(tmp), 0);

  return result;
}

nscoord
nsFieldSetFrame::GetContentMinWidth(nsIRenderingContext* aRenderingContext)
{
  NS_ASSERTION(mContentFrame, "Don't call me if there is no legend frame!");

  return nsLayoutUtils::IntrinsicForContainer(aRenderingContext, mContentFrame,
                                              nsLayoutUtils::MIN_WIDTH);
}

nscoord
nsFieldSetFrame::GetMinWidth(nsIRenderingContext* aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_MIN_WIDTH(this, result);

  nscoord legendPrefWidth = 0;
  nscoord contentMinWidth = 0;
  if (mLegendFrame) {
    legendPrefWidth = GetLegendPrefWidth(aRenderingContext);
  }

  if (mContentFrame) {
    contentMinWidth = GetContentMinWidth(aRenderingContext);
  }
      
  result = PR_MAX(legendPrefWidth, contentMinWidth);
  return result;
}

nscoord
nsFieldSetFrame::GetPrefWidth(nsIRenderingContext* aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_PREF_WIDTH(this, result);

  nscoord legendPrefWidth = 0;
  nscoord contentPrefWidth = 0;
  if (mLegendFrame) {
    legendPrefWidth = GetLegendPrefWidth(aRenderingContext);
  }

  if (mContentFrame) {
    contentPrefWidth =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext, mContentFrame,
                                           nsLayoutUtils::PREF_WIDTH);
  }
      
  result = PR_MAX(legendPrefWidth, contentPrefWidth);
  return result;
}

/* virtual */ nsSize
nsFieldSetFrame::ComputeSize(nsIRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             PRBool aShrinkWrap)
{
  nsSize result =
    nsHTMLContainerFrame::ComputeSize(aRenderingContext, aCBSize,
                                      aAvailableWidth,
                                      aMargin, aBorder, aPadding, aShrinkWrap);

  // Fieldsets never shrink below their min width.
  nscoord minWidth = GetMinWidth(aRenderingContext);
  if (minWidth > result.width)
    result.width = minWidth;

  return result;
}

NS_IMETHODIMP 
nsFieldSetFrame::Reflow(nsPresContext*           aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsFieldSetFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  NS_PRECONDITION(aReflowState.ComputedWidth() != NS_INTRINSICSIZE,
                  "Should have a precomputed width!");      
  
  // Initialize OUT parameter
  aStatus = NS_FRAME_COMPLETE;

  //------------ Handle Incremental Reflow -----------------
  PRBool reflowContent;
  PRBool reflowLegend;

  if (aReflowState.ShouldReflowAllKids()) {
    reflowContent = mContentFrame != nsnull;
    reflowLegend = mLegendFrame != nsnull;
  } else {
    reflowContent = mContentFrame &&
      (mContentFrame->GetStateBits() &
       (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) != 0;

    reflowLegend = mLegendFrame &&
      (mLegendFrame->GetStateBits() &
       (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) != 0;
  }

  nsSize availSize(aReflowState.ComputedWidth(), aReflowState.availableHeight);
  NS_ASSERTION(!mContentFrame ||
               GetContentMinWidth(aReflowState.rendContext) <= availSize.width,
               "Bogus availSize.width; should be bigger");

  // get our border and padding
  const nsMargin &borderPadding = aReflowState.mComputedBorderPadding;
  const nsMargin &padding       = aReflowState.mComputedPadding;
  nsMargin border = borderPadding - padding;  

  // Figure out how big the legend is if there is one. 
  // get the legend's margin
  nsMargin legendMargin(0,0,0,0);
  // reflow the legend only if needed
  if (reflowLegend) {
    const nsStyleMargin* marginStyle = mLegendFrame->GetStyleMargin();
    marginStyle->GetMargin(legendMargin);

    // Give the legend all the space it wants.
    nsSize legendAvailSize(GetLegendPrefWidth(aReflowState.rendContext),
                           NS_INTRINSICSIZE);

    nsHTMLReflowState legendReflowState(aPresContext, aReflowState,
                                        mLegendFrame,
                                        legendAvailSize);

    // always give the legend as much size as it wants
    legendReflowState.
      SetComputedWidth(mLegendFrame->GetPrefWidth(aReflowState.rendContext));
    legendReflowState.mComputedHeight = NS_INTRINSICSIZE;

    nsHTMLReflowMetrics legendDesiredSize;

    ReflowChild(mLegendFrame, aPresContext, legendDesiredSize, legendReflowState,
                0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
#ifdef NOISY_REFLOW
    printf("  returned (%d, %d)\n", legendDesiredSize.width, legendDesiredSize.height);
#endif
    // figure out the legend's rectangle
    mLegendRect.width  = legendDesiredSize.width + legendMargin.left + legendMargin.right;
    mLegendRect.height = legendDesiredSize.height + legendMargin.top + legendMargin.bottom;
    mLegendRect.x = borderPadding.left;
    mLegendRect.y = 0;

    nscoord oldSpace = mLegendSpace;
    mLegendSpace = 0;
    if (mLegendRect.height > border.top) {
      // center the border on the legend
      mLegendSpace = mLegendRect.height - border.top;
    } else {
      mLegendRect.y = (border.top - mLegendRect.height)/2;
    }

    // if the legend space changes then we need to reflow the 
    // content area as well.
    if (mLegendSpace != oldSpace && mContentFrame) {
      reflowContent = PR_TRUE;
    }

    // if we are contrained then remove the legend from our available height.
    if (NS_INTRINSICSIZE != availSize.height) {
      availSize.height -= mLegendSpace;
      availSize.height = PR_MAX(availSize.height, 0);
    }
  
    NS_ASSERTION(availSize.width >= mLegendRect.width,
                 "Bogus availSize.width.  Should be bigger");

    FinishReflowChild(mLegendFrame, aPresContext, &legendReflowState, 
                      legendDesiredSize, 0, 0, NS_FRAME_NO_MOVE_FRAME);    
  } else if (!mLegendFrame) {
    mLegendRect.Empty();
    mLegendSpace = 0;
  } // else mLegendSpace and mLegendRect haven't changed... 

  nsRect contentRect;

  // reflow the content frame only if needed
  if (reflowContent) {
    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, mContentFrame,
                                     availSize);

    nsHTMLReflowMetrics kidDesiredSize(aDesiredSize.mFlags);
    // Reflow the frame
    ReflowChild(mContentFrame, aPresContext, kidDesiredSize, kidReflowState,
                borderPadding.left + kidReflowState.mComputedMargin.left,
                borderPadding.top + mLegendSpace + kidReflowState.mComputedMargin.top,
                0, aStatus);

    // set the rect. make sure we add the margin back in.
    contentRect.SetRect(borderPadding.left,borderPadding.top + mLegendSpace,kidDesiredSize.width ,kidDesiredSize.height);
    if (aReflowState.mComputedHeight != NS_INTRINSICSIZE &&
        borderPadding.top + mLegendSpace+kidDesiredSize.height > aReflowState.mComputedHeight) {
      kidDesiredSize.height = aReflowState.mComputedHeight-(borderPadding.top + mLegendSpace);
    }

    FinishReflowChild(mContentFrame, aPresContext, &kidReflowState, 
                      kidDesiredSize, contentRect.x, contentRect.y, 0);
    NS_FRAME_TRACE_REFLOW_OUT("FieldSet::Reflow", aStatus);

  } else if (mContentFrame) {
    // if we don't need to reflow just get the old size
    // XXXbz what about auto or percent margins?  Those wouldn't be in
    // the style!
    contentRect = mContentFrame->GetRect();
    const nsStyleMargin* marginStyle = mContentFrame->GetStyleMargin();

    nsMargin m(0,0,0,0);
    marginStyle->GetMargin(m);
    contentRect.Inflate(m);
  } else {
    contentRect.Empty();
  }

  // use the computed width if the inner content does not fill it
  if (aReflowState.ComputedWidth() > contentRect.width) {
    contentRect.width = aReflowState.ComputedWidth();
  }

  if (mLegendFrame) {
    // if the content rect is larger then the  legend we can align the legend
    if (contentRect.width > mLegendRect.width) {
      PRInt32 align = ((nsLegendFrame*)mLegendFrame)->GetAlign();

      switch(align) {
        case NS_STYLE_TEXT_ALIGN_RIGHT:
          mLegendRect.x = contentRect.width - mLegendRect.width + borderPadding.left;
          break;
        case NS_STYLE_TEXT_ALIGN_CENTER:
          // Note: rounding removed; there doesn't seem to be any need
          mLegendRect.x = contentRect.width / 2 - mLegendRect.width / 2 + borderPadding.left;
          break;
      }
  
    } else {
      //otherwise make place for the legend
      contentRect.width = mLegendRect.width;
    }
    // place the legend
    nsRect actualLegendRect(mLegendRect);
    actualLegendRect.Deflate(legendMargin);

    nsPoint curOrigin = mLegendFrame->GetPosition();

    // only if the origin changed
    if ((curOrigin.x != mLegendRect.x) || (curOrigin.y != mLegendRect.y)) {
      mLegendFrame->SetPosition(nsPoint(actualLegendRect.x , actualLegendRect.y));
      nsContainerFrame::PositionFrameView(mLegendFrame);

      // We need to recursively process the legend frame's
      // children since we're moving the frame after Reflow.
      nsContainerFrame::PositionChildViews(mLegendFrame);
    }
  }

  // Return our size and our result
  if (aReflowState.mComputedHeight == NS_INTRINSICSIZE) {
    aDesiredSize.height = mLegendSpace + 
                          borderPadding.top +
                          contentRect.height +
                          borderPadding.bottom;
  } else {
    nscoord min = borderPadding.top + borderPadding.bottom + mLegendRect.height;
    aDesiredSize.height = aReflowState.mComputedHeight + borderPadding.top + borderPadding.bottom;
    if (aDesiredSize.height < min)
      aDesiredSize.height = min;
  }
  aDesiredSize.width = contentRect.width + borderPadding.left + borderPadding.right;
  aDesiredSize.mOverflowArea = nsRect(0, 0, aDesiredSize.width, aDesiredSize.height);
  if (mLegendFrame)
    ConsiderChildOverflow(aDesiredSize.mOverflowArea, mLegendFrame);
  if (mContentFrame)
    ConsiderChildOverflow(aDesiredSize.mOverflowArea, mContentFrame);
  FinishAndStoreOverflow(&aDesiredSize);

  Invalidate(aDesiredSize.mOverflowArea);

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

PRIntn
nsFieldSetFrame::GetSkipSides() const
{
  return 0;
}

NS_IMETHODIMP
nsFieldSetFrame::AppendFrames(nsIAtom*       aListName,
                              nsIFrame*      aFrameList)
{
  aFrameList = MaybeSetLegend(aFrameList, aListName);
  if (aFrameList) {
    ReParentFrameList(aFrameList);
    return mContentFrame->AppendFrames(aListName, aFrameList);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFieldSetFrame::InsertFrames(nsIAtom*       aListName,
                              nsIFrame*      aPrevFrame,
                              nsIFrame*      aFrameList)
{
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this ||
               aPrevFrame->GetParent() == mContentFrame,
               "inserting after sibling frame with different parent");

  aFrameList = MaybeSetLegend(aFrameList, aListName);
  if (aFrameList) {
    ReParentFrameList(aFrameList);
    if (NS_UNLIKELY(aPrevFrame == mLegendFrame)) {
      aPrevFrame = nsnull;
    }
    return mContentFrame->InsertFrames(aListName, aPrevFrame, aFrameList);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFieldSetFrame::RemoveFrame(nsIAtom*       aListName,
                             nsIFrame*      aOldFrame)
{
  // For reference, see bug 70648, bug 276104 and bug 236071.
  if (aOldFrame == mLegendFrame) {
    NS_ASSERTION(!aListName, "Unexpected frame list when removing legend frame");
    NS_ASSERTION(mLegendFrame->GetParent() == this, "Legend Parent has wrong parent");
    NS_ASSERTION(mLegendFrame->GetNextSibling() == mContentFrame, "mContentFrame is not next sibling");

    mFrames.DestroyFrame(mLegendFrame);
    mLegendFrame = nsnull;
    AddStateBits(NS_FRAME_IS_DIRTY);
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange);
    return NS_OK;
  }
  return mContentFrame->RemoveFrame(aListName, aOldFrame);
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsFieldSetFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    return accService->CreateHTMLGroupboxAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

nsIFrame*
nsFieldSetFrame::MaybeSetLegend(nsIFrame* aFrameList, nsIAtom* aListName)
{
  if (!mLegendFrame && aFrameList->GetType() == nsGkAtoms::legendFrame) {
    NS_ASSERTION(!aListName, "Unexpected frame list when adding legend frame");
    mLegendFrame = aFrameList;
    aFrameList = mLegendFrame->GetNextSibling();
    mLegendFrame->SetNextSibling(mContentFrame);
    mFrames.SetFrames(mLegendFrame);
    AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange);
  }
  return aFrameList;
}

void
nsFieldSetFrame::ReParentFrameList(nsIFrame* aFrameList)
{
  nsFrameManager* frameManager = PresContext()->FrameManager();
  for (nsIFrame* frame = aFrameList; frame; frame = frame->GetNextSibling()) {
    frame->SetParent(mContentFrame);
    frameManager->ReParentStyleContext(frame);
  }
  mContentFrame->AddStateBits(GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW);
}
