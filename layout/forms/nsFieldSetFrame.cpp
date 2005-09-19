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
#include "nsHTMLAtoms.h"
#include "nsLayoutAtoms.h"
#include "nsStyleConsts.h"
#include "nsFont.h"
#include "nsCOMPtr.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIServiceManager.h"
#include "nsSpaceManager.h"

class nsLegendFrame;

class nsFieldSetFrame : public nsHTMLContainerFrame {
public:

  nsFieldSetFrame();

  NS_IMETHOD SetInitialChildList(nsPresContext* aPresContext,
                                 nsIAtom*       aListName,
                                 nsIFrame*      aChildList);

  NS_IMETHOD Reflow(nsPresContext*           aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
                               
  NS_IMETHOD Paint(nsPresContext*       aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags);

  virtual PRBool CanPaintBackground();

  NS_IMETHOD AppendFrames(nsIAtom*       aListName,
                          nsIFrame*      aFrameList);
  NS_IMETHOD InsertFrames(nsIAtom*       aListName,
                          nsIFrame*      aPrevFrame,
                          nsIFrame*      aFrameList);
  NS_IMETHOD RemoveFrame(nsIAtom*       aListName,
                         nsIFrame*      aOldFrame);
  NS_IMETHOD ReplaceFrame(nsIAtom*       aListName,
                          nsIFrame*      aOldFrame,
                          nsIFrame*      aNewFrame);

  virtual nsIFrame* GetFrameForPoint(const nsPoint&    aPoint,
                                     nsFramePaintLayer aWhichLayer);

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

nsresult
NS_NewFieldSetFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aStateFlags)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsFieldSetFrame* it = new (aPresShell) nsFieldSetFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // set the state flags (if any are provided)
  it->AddStateBits(aStateFlags);
  
  *aNewFrame = it;
  return NS_OK;
}

nsFieldSetFrame::nsFieldSetFrame()
  : nsHTMLContainerFrame()
{
  mContentFrame = nsnull;
  mLegendFrame  = nsnull;
  mLegendSpace  = 0;
}

nsIAtom*
nsFieldSetFrame::GetType() const
{
  return nsLayoutAtoms::fieldSetFrame;
}

PRBool
nsFieldSetFrame::IsContainingBlock() const
{
  return PR_TRUE;
}

NS_IMETHODIMP
nsFieldSetFrame::SetInitialChildList(nsPresContext* aPresContext,
                                     nsIAtom*       aListName,
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
  return nsHTMLContainerFrame::SetInitialChildList(aPresContext, nsnull, aChildList);
}

// this is identical to nsHTMLContainerFrame::Paint except for the background and border. 
NS_IMETHODIMP
nsFieldSetFrame::Paint(nsPresContext*       aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect&        aDirtyRect,
                       nsFramePaintLayer    aWhichLayer,
                       PRUint32             aFlags)
{
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    // Paint our background and border
    PRBool isVisible;
    if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_TRUE, &isVisible)) && 
                     isVisible && mRect.width && mRect.height) {
      PRIntn skipSides = GetSkipSides();
      const nsStyleBorder* borderStyle = GetStyleBorder();
      const nsStylePadding* paddingStyle = GetStylePadding();
       
      nscoord topBorder = borderStyle->GetBorderWidth(NS_SIDE_TOP);

      nscoord yoff = 0;
      
      // if the border is smaller than the legend. Move the border down
      // to be centered on the legend. 
      if (topBorder < mLegendRect.height)
        yoff = (mLegendRect.height - topBorder)/2;
      
      nsRect rect(0, yoff, mRect.width, mRect.height - yoff);

      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *borderStyle,
                                      *paddingStyle, PR_TRUE);

      if (mLegendFrame) {

        // Use the rect of the legend frame, not mLegendRect, so we draw our
        // border under the legend's left and right margins.
        const nsRect & legendRect = mLegendFrame->GetRect();
      
        // we should probably use PaintBorderEdges to do this but for now just use clipping
        // to achieve the same effect.

        // draw left side
        nsRect clipRect(rect);
        clipRect.width = legendRect.x - rect.x;
        clipRect.height = topBorder;

        aRenderingContext.PushState();
        aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect);
        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *borderStyle, mStyleContext, skipSides);
  
        aRenderingContext.PopState();


        // draw right side
        clipRect = rect;
        clipRect.x = legendRect.x + legendRect.width;
        clipRect.width -= (legendRect.x + legendRect.width);
        clipRect.height = topBorder;

        aRenderingContext.PushState();
        aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect);
        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *borderStyle, mStyleContext, skipSides);
  
        aRenderingContext.PopState();

      
        // draw bottom
        clipRect = rect;
        clipRect.y += topBorder;
        clipRect.height = mRect.height - (yoff + topBorder);
      
        aRenderingContext.PushState();
        aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect);
        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *borderStyle, mStyleContext, skipSides);
  
        aRenderingContext.PopState();
      } else {

        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                    aDirtyRect,
                                    nsRect(0,0,mRect.width, mRect.height),
                                    *borderStyle, mStyleContext, skipSides);
      }
    }
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

#ifdef DEBUG
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    if (HasView()) {
      aRenderingContext.SetColor(NS_RGB(0,0,255));
    }
    else {
      aRenderingContext.SetColor(NS_RGB(255,0,0));
    }
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
#endif
  DO_GLOBAL_REFLOW_COUNT_DSP("nsFieldSetFrame", &aRenderingContext);
  return NS_OK;
}

PRBool
nsFieldSetFrame::CanPaintBackground()
{
  // If we have a legend, we won't be painting our background across our whole
  // rect.  So return false here, since we'll usually have a legend.
  return PR_FALSE;
}

NS_IMETHODIMP 
nsFieldSetFrame::Reflow(nsPresContext*           aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsFieldSetFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  // Initialize OUT parameter
  aStatus = NS_FRAME_COMPLETE;

  // Should we create a space manager?
  nsAutoSpaceManager autoSpaceManager(NS_CONST_CAST(nsHTMLReflowState &, aReflowState));

  // XXXldb If we start storing the space manager in the frame rather
  // than keeping it around only during reflow then we should create it
  // only when there are actually floats to manage.  Otherwise things
  // like tables will gain significant bloat.
  if (NS_BLOCK_SPACE_MGR & mState)
    autoSpaceManager.CreateSpaceManagerFor(aPresContext, this);

  
  //------------ Handle Incremental Reflow -----------------
  PRBool reflowContent = PR_TRUE;
  PRBool reflowLegend = PR_TRUE;
  nsReflowReason reason = aReflowState.reason;

  if (reason == eReflowReason_Incremental) {
      nsHTMLReflowCommand *command = aReflowState.path->mReflowCommand;

      // See if it's targeted at us
      if (command) {
        nsReflowType  reflowType;
        command->GetType(reflowType);

        switch (reflowType) {
          case eReflowType_StyleChanged:
            reason = eReflowReason_StyleChange;
            break;

          case eReflowType_ReflowDirty: 
            reason = eReflowReason_Dirty;
            break;

          default:
            NS_ERROR("Unexpected Reflow Type");
        }
      } else {
           reflowContent = PR_FALSE;
           reflowLegend = PR_FALSE;

           nsReflowPath::iterator iter = aReflowState.path->FirstChild();
           nsReflowPath::iterator end = aReflowState.path->EndChildren();

           for ( ; iter != end; ++iter) {
               if (*iter == mLegendFrame)
                   reflowLegend = PR_TRUE;
               else if (*iter == mContentFrame)
                   reflowContent = PR_TRUE;
           }
      }
  }

  if (aDesiredSize.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
    reflowLegend = PR_TRUE;
    reflowContent = PR_TRUE;
  } else if (reason == eReflowReason_Dirty) {
    // if dirty then check dirty flags
    if (GetStateBits() & NS_FRAME_IS_DIRTY) {
      reflowLegend = PR_TRUE;
      reflowContent = PR_TRUE;
    } else {
      if (reflowContent) {
        reflowContent = mContentFrame ?
          (mContentFrame->GetStateBits()
           & (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) != 0 : PR_FALSE;
      }

      if (reflowLegend) {
        reflowLegend = mLegendFrame ?
          (mLegendFrame->GetStateBits()
           & (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) != 0 : PR_FALSE;
      }
    }
  }

  // availSize could have unconstrained values, don't perform any addition on them
  nsSize availSize(aReflowState.mComputedWidth, aReflowState.availableHeight);

  // get our border and padding
  const nsMargin &borderPadding = aReflowState.mComputedBorderPadding;
  const nsMargin &padding       = aReflowState.mComputedPadding;
  nsMargin border = borderPadding - padding;
  if (aDesiredSize.mComputeMEW) {
    aDesiredSize.mMaxElementWidth = borderPadding.left + borderPadding.right;
  }
  

  // Figure out how big the legend is if there is one. 
  // get the legend's margin
  nsMargin legendMargin(0,0,0,0);
  // reflow the legend only if needed
  if (mLegendFrame) {
    const nsStyleMargin* marginStyle = mLegendFrame->GetStyleMargin();
    marginStyle->GetMargin(legendMargin);

    if (reflowLegend) {
      nsHTMLReflowState legendReflowState(aPresContext, aReflowState,
                                          mLegendFrame, nsSize(NS_INTRINSICSIZE,NS_INTRINSICSIZE),
                                          reason);

      // always give the legend as much size as it needs
      legendReflowState.mComputedWidth = NS_INTRINSICSIZE;
      legendReflowState.mComputedHeight = NS_INTRINSICSIZE;

      nsHTMLReflowMetrics legendDesiredSize(0,0);

      ReflowChild(mLegendFrame, aPresContext, legendDesiredSize, legendReflowState,
                  0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
#ifdef NOISY_REFLOW
      printf("  returned (%d, %d)\n", legendDesiredSize.width, legendDesiredSize.height);
      if (legendDesiredSize.mComputeMEW)
        printf("  and maxEW %d\n", 
               legendDesiredSize.mMaxElementWidth);
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
      if (mLegendSpace != oldSpace) {
        if (reflowContent == PR_FALSE || reason == eReflowReason_Dirty) {
          reflowContent = PR_TRUE;
          reason = eReflowReason_Resize;
        }
      }

      // if we are contrained then remove the legend from our available height.
      if (NS_INTRINSICSIZE != availSize.height) {
        if (availSize.height >= mLegendSpace)
          availSize.height -= mLegendSpace;
      }
  
      // don't get any smaller than the legend
      if (NS_INTRINSICSIZE != availSize.width) {
        if (availSize.width < mLegendRect.width)
          availSize.width = mLegendRect.width;
      }

      FinishReflowChild(mLegendFrame, aPresContext, &legendReflowState, 
                        legendDesiredSize, 0, 0, NS_FRAME_NO_MOVE_FRAME);    

    }
  } else {
    mLegendRect.Empty();
    mLegendSpace = 0;
  }

  nsRect contentRect;

  // reflow the content frame only if needed
  if (mContentFrame) {
    if (reflowContent) {
      availSize.width = aReflowState.mComputedWidth;

      nsHTMLReflowState kidReflowState(aPresContext, aReflowState, mContentFrame,
                                       availSize, reason);

      nsHTMLReflowMetrics kidDesiredSize(aDesiredSize.mComputeMEW, aDesiredSize.mFlags);
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
      if (aDesiredSize.mComputeMEW) {
        aDesiredSize.mMaxElementWidth = kidDesiredSize.mMaxElementWidth;
        if (eStyleUnit_Coord == aReflowState.mStylePosition->mWidth.GetUnit() &&
            NS_INTRINSICSIZE != aReflowState.mComputedWidth)
          aDesiredSize.mMaxElementWidth = aReflowState.mComputedWidth;
        if (eStyleUnit_Percent == aReflowState.mStylePosition->mWidth.GetUnit())
          aDesiredSize.mMaxElementWidth = 0;
        aDesiredSize.mMaxElementWidth += borderPadding.left + borderPadding.right;
      }
      if (aDesiredSize.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
        aDesiredSize.mMaximumWidth = kidDesiredSize.mMaximumWidth +
                                     borderPadding.left + borderPadding.right;
      }
      NS_FRAME_TRACE_REFLOW_OUT("FieldSet::Reflow", aStatus);

    } else {
      // if we don't need to reflow just get the old size
      contentRect = mContentFrame->GetRect();
      const nsStyleMargin* marginStyle = mContentFrame->GetStyleMargin();

      nsMargin m(0,0,0,0);
      marginStyle->GetMargin(m);
      contentRect.Inflate(m);
    }
  }

  // use the computed width if the inner content does not fill it
  if (aReflowState.mComputedWidth != NS_INTRINSICSIZE &&
      aReflowState.mComputedWidth > contentRect.width) {
    contentRect.width = aReflowState.mComputedWidth;
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
          float p2t;
          p2t = aPresContext->PixelsToTwips();
          mLegendRect.x = NSIntPixelsToTwips((nscoord) NSToIntRound((float)(contentRect.width/2 - mLegendRect.width/2 + borderPadding.left) / p2t),p2t);
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
  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;
  if (aDesiredSize.mComputeMEW) {
    // if the legend is wider use it
    if (aDesiredSize.mMaxElementWidth < mLegendRect.width + borderPadding.left + borderPadding.right)
      aDesiredSize.mMaxElementWidth = mLegendRect.width + borderPadding.left + borderPadding.right;
  }
  aDesiredSize.mOverflowArea = nsRect(0, 0, aDesiredSize.width, aDesiredSize.height);
  // make the mMaximumWidth large enough if the legendframe determines the size
  if ((aDesiredSize.mFlags & NS_REFLOW_CALC_MAX_WIDTH) && mLegendFrame) {
    aDesiredSize.mMaximumWidth = PR_MAX(aDesiredSize.mMaximumWidth, mLegendRect.width +
                                        borderPadding.left + borderPadding.right);
  }
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
  aFrameList = MaybeSetLegend(aFrameList, aListName);
  if (aFrameList) {
    ReParentFrameList(aFrameList);
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
    nsPresContext* presContext = GetPresContext();
    mFrames.DestroyFrame(presContext, mLegendFrame);
    mLegendFrame = nsnull;
    AddStateBits(NS_FRAME_IS_DIRTY);
    if (GetParent()) {
      GetParent()->ReflowDirtyChild(presContext->GetPresShell(), this);
    }
    return NS_OK;
  }
  return mContentFrame->RemoveFrame(aListName, aOldFrame);
}

NS_IMETHODIMP
nsFieldSetFrame::ReplaceFrame(nsIAtom*       aListName,
                              nsIFrame*      aOldFrame,
                              nsIFrame*      aNewFrame)
{
  if (aOldFrame == mLegendFrame) {
    mLegendFrame = aNewFrame;
    return nsContainerFrame::ReplaceFrame(aListName, aOldFrame, aNewFrame);
  }
  return mContentFrame->ReplaceFrame(aListName, aOldFrame, aNewFrame);
}

nsIFrame*
nsFieldSetFrame::GetFrameForPoint(const nsPoint&    aPoint,
                                  nsFramePaintLayer aWhichLayer)
{
  // this should act like a block, so we need to override
  return GetFrameForPointUsing(aPoint, nsnull, aWhichLayer, 
                               aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND);
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
  if (!mLegendFrame && aFrameList->GetType() == nsLayoutAtoms::legendFrame) {
    NS_ASSERTION(!aListName, "Unexpected frame list when adding legend frame");
    mLegendFrame = aFrameList;
    aFrameList = mLegendFrame->GetNextSibling();
    mLegendFrame->SetNextSibling(mContentFrame);
    mFrames.SetFrames(mLegendFrame);
    AddStateBits(NS_FRAME_IS_DIRTY);
    if (GetParent()) {
      GetParent()->ReflowDirtyChild(mLegendFrame->GetPresContext()->GetPresShell(), this);
    }
  }
  return aFrameList;
}

void
nsFieldSetFrame::ReParentFrameList(nsIFrame* aFrameList)
{
  nsFrameManager* frameManager = mContentFrame->GetPresContext()->FrameManager();
  nsStyleContext* newParentContext = mContentFrame->GetStyleContext();
  for (nsIFrame* frame = aFrameList; frame; frame = frame->GetNextSibling()) {
    frame->SetParent(mContentFrame);
    frameManager->ReParentStyleContext(frame, newParentContext);
  }
  mContentFrame->AddStateBits(GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW);
}
