/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLParts.h"
#include "nsHTMLAtoms.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsStyleUtil.h"
#include "nsFont.h"
#include "nsCOMPtr.h"

static NS_DEFINE_IID(kLegendFrameCID, NS_LEGEND_FRAME_CID);
 
class nsLegendFrame;

class nsFieldSetFrame : public nsHTMLContainerFrame {
public:

  nsFieldSetFrame();

  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD Reflow(nsIPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
                               
  NS_METHOD Paint(nsIPresContext*      aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect&        aDirtyRect,
                  nsFramePaintLayer    aWhichLayer,
                  PRUint32             aFlags);

  NS_IMETHOD  AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  InsertFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  RemoveFrame(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);
  NS_IMETHOD  ReplaceFrame(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aOldFrame,
                           nsIFrame*       aNewFrame);
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("FieldSet", aResult);
  }
#endif

protected:

  virtual PRIntn GetSkipSides() const;

  nsIFrame* mLegendFrame;
  nsIFrame* mContentFrame;
  nsRect    mLegendRect;
  nscoord   mLegendSpace;
};

nsresult
NS_NewFieldSetFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aStateFlags )
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
  nsFrameState state;
  it->GetFrameState( &state );
  state |= aStateFlags;
  it->SetFrameState( state );
  
  *aNewFrame = it;
  return NS_OK;
}

nsFieldSetFrame::nsFieldSetFrame()
  : nsHTMLContainerFrame()
{
  mContentFrame    = nsnull;
  mLegendFrame     = nsnull;
  mLegendSpace   = 0;
}


NS_IMETHODIMP
nsFieldSetFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aChildList)
{
 
  // get the content and legend frames.
  mContentFrame = aChildList;
  mContentFrame->GetNextSibling(&mLegendFrame);

  // Queue up the frames for the content frame
  return nsHTMLContainerFrame::SetInitialChildList(aPresContext, nsnull, aChildList);
}

// this is identical to nsHTMLContainerFrame::Paint except for the background and border. 
NS_IMETHODIMP
nsFieldSetFrame::Paint(nsIPresContext*      aPresContext,
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
      const nsStyleBackground* color =
        (const nsStyleBackground*)mStyleContext->GetStyleData(eStyleStruct_Background);
      const nsStyleBorder* borderStyle = 
        (const nsStyleBorder*)mStyleContext->GetStyleData(eStyleStruct_Border);
       
        nsMargin border;
        if (!borderStyle->GetBorder(border)) {
          NS_NOTYETIMPLEMENTED("percentage border");
        }

        nscoord yoff = 0;
        
        // if the border is smaller than the legend. Move the border down
        // to be centered on the legend. 
        if (border.top < mLegendRect.height)
            yoff = (mLegendRect.height - border.top)/2;
       
        nsRect rect(0, yoff, mRect.width, mRect.height - yoff);

        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *color, *borderStyle, 0, 0);


        if (mLegendFrame) {

          // we should probably use PaintBorderEdges to do this but for now just use clipping
          // to achieve the same effect.
          PRBool clipState;

          // draw left side
          nsRect clipRect(rect);
          clipRect.width = mLegendRect.x - rect.x;
          clipRect.height = border.top;

          aRenderingContext.PushState();
          aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect, clipState);
          nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *borderStyle, mStyleContext, skipSides);
  
          aRenderingContext.PopState(clipState);


          // draw right side
          clipRect = rect;
          clipRect.x = mLegendRect.x + mLegendRect.width;
          clipRect.width -= (mLegendRect.x + mLegendRect.width);
          clipRect.height = border.top;

          aRenderingContext.PushState();
          aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect, clipState);
          nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *borderStyle, mStyleContext, skipSides);
  
          aRenderingContext.PopState(clipState);

        
          // draw bottom

          clipRect = rect;
          clipRect.y += border.top;
          clipRect.height = mRect.height - (yoff + border.top);
        
          aRenderingContext.PushState();
          aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect, clipState);
          nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *borderStyle, mStyleContext, skipSides);
  
          aRenderingContext.PopState(clipState);
        } else {

          
          nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                      aDirtyRect, nsRect(0,0,mRect.width, mRect.height), *borderStyle, mStyleContext, skipSides);
        }
    }
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

#ifdef DEBUG
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    nsIView* view;
    GetView(aPresContext, &view);
    if (nsnull != view) {
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


NS_IMETHODIMP 
nsFieldSetFrame::Reflow(nsIPresContext*          aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsFieldSetFrame", aReflowState.reason);
  DISPLAY_REFLOW(this, aReflowState, aDesiredSize, aStatus);
 
  // Initialize OUT parameter
   aStatus = NS_FRAME_COMPLETE;

   //------------ Handle Incremental Reflow -----------------
   nsIFrame* incrementalChild = nsnull;
   PRBool reflowContent = PR_TRUE;
   PRBool reflowLegend = PR_TRUE;
   nsReflowReason reason = aReflowState.reason;

    if ( aReflowState.reason == eReflowReason_Incremental ) {
        nsIReflowCommand::ReflowType  reflowType;
        aReflowState.reflowCommand->GetType(reflowType);

        // See if it's targeted at us
        nsIFrame* targetFrame;    
        aReflowState.reflowCommand->GetTarget(targetFrame);

        if (this == targetFrame) {
            switch (reflowType) {

              case nsIReflowCommand::StyleChanged: 
                  {
                    nsHTMLReflowState newState(aReflowState);
                    newState.reason = eReflowReason_StyleChange;
                    return Reflow(aPresContext, aDesiredSize, newState, aStatus);
                  }
                  break;

                  // if its a dirty type then reflow us with a dirty reflow
                  case nsIReflowCommand::ReflowDirty: 
                  {
                    nsHTMLReflowState newState(aReflowState);
                    newState.reason = eReflowReason_Dirty;
                    return Reflow(aPresContext, aDesiredSize, newState, aStatus);
                  }
                  break;

                  default:
                    NS_ERROR("Unexpected Reflow Type");
            }
        } else {
             aReflowState.reflowCommand->GetNext(incrementalChild);

             reflowContent = PR_FALSE;
             reflowLegend = PR_FALSE;

             if (incrementalChild == mLegendFrame)
                 reflowLegend = PR_TRUE;
             else if (incrementalChild == mContentFrame)
                 reflowContent = PR_TRUE;       
        }
    }

    // if dirty then check dirty flags
    if (reason == eReflowReason_Dirty) 
    {
        if (reflowContent) {
              nsFrameState state;
              mContentFrame->GetFrameState(&state);
              reflowContent = (state & NS_FRAME_IS_DIRTY) || (state & NS_FRAME_HAS_DIRTY_CHILDREN);
        }

        if (reflowLegend) {
              nsFrameState state;
              mLegendFrame->GetFrameState(&state);
              reflowLegend = (state & NS_FRAME_IS_DIRTY) || (state & NS_FRAME_HAS_DIRTY_CHILDREN);
        }
    }

    // availSize could have unconstrained values, don't perform any addition on them
    nsSize availSize(aReflowState.mComputedWidth, aReflowState.availableHeight);
  
    // get our border and padding
    const nsMargin &borderPadding = aReflowState.mComputedBorderPadding;
    const nsMargin &padding       = aReflowState.mComputedPadding;
    nsMargin border = borderPadding - padding;

    // Figure out how big the legend is if there is one. 
    // get the legend's margin
    nsMargin legendMargin(0,0,0,0);
    // reflow the legend only if needed
    if (mLegendFrame) {
        const nsStyleMargin* marginStyle;
        mLegendFrame->GetStyleData(eStyleStruct_Margin,
                              (const nsStyleStruct*&) marginStyle);
        marginStyle->GetMargin(legendMargin);

        if (reflowLegend) {
          nsHTMLReflowState legendReflowState(aPresContext, aReflowState,
                                              mLegendFrame, nsSize(NS_INTRINSICSIZE,NS_INTRINSICSIZE));

          // always give the legend as much size as it needs
          legendReflowState.mComputedWidth = NS_INTRINSICSIZE;
          legendReflowState.mComputedHeight = NS_INTRINSICSIZE;
          legendReflowState.reason = reason;

          nsHTMLReflowMetrics legendDesiredSize(0,0);

          ReflowChild(mLegendFrame, aPresContext, legendDesiredSize, legendReflowState,
                      0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
#ifdef NOISY_REFLOW
          printf("  returned (%d, %d)\n", legendDesiredSize.width, legendDesiredSize.height);
          if (legendDesiredSize.maxElementSize)
            printf("  and maxES (%d, %d)\n", 
                   legendDesiredSize.maxElementSize->width, legendDesiredSize.maxElementSize->height);
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
              mLegendSpace = mLegendRect.height - borderPadding.top;
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

          FinishReflowChild(mLegendFrame, aPresContext, legendDesiredSize, 0, 0, NS_FRAME_NO_MOVE_FRAME);    

        }
    }

    nsRect contentRect;

    // reflow the content frame only if needed
    if (mContentFrame) {
        if (reflowContent) {
            availSize.width = aReflowState.mComputedWidth;

#if 0
            if (aReflowState.mComputedHeight != NS_INTRINSICSIZE) {
              availSize.height = aReflowState.mComputedHeight - mLegendSpace;
            } else {
              availSize.height = NS_INTRINSICSIZE;
            }
#endif
            nsHTMLReflowState kidReflowState(aPresContext, aReflowState, mContentFrame,
                                             availSize);

            kidReflowState.reason = reason;

            nsHTMLReflowMetrics kidDesiredSize(0,0);

            // Reflow the frame
            ReflowChild(mContentFrame, aPresContext, kidDesiredSize, kidReflowState,
                        kidReflowState.mComputedMargin.left, kidReflowState.mComputedMargin.top,
                        0, aStatus);
#ifdef NOISY_REFLOW
            printf("  returned (%d, %d)\n", kidDesiredSize.width, kidDesiredSize.height);
            if (kidDesiredSize.maxElementSize)
              printf("  and maxES (%d, %d)\n", 
                     kidDesiredSize.maxElementSize->width, kidDesiredSize.maxElementSize->height);
#endif

            /*
            printf("*** %p computedHgt: %d ", this, aReflowState.mComputedHeight);
              printf("Reason: ");
              switch (aReflowState.reason) {
                case eReflowReason_Initial:printf("Initil");break;
                case eReflowReason_Incremental:printf("Increm");break;
                case eReflowReason_Resize: printf("Resize");      break;
                case eReflowReason_StyleChange:printf("eReflowReason_StyleChange");break;
              }
              */

            // set the rect. make sure we add the margin back in.
            contentRect.SetRect(borderPadding.left,borderPadding.top + mLegendSpace,kidDesiredSize.width ,kidDesiredSize.height);
            if (aReflowState.mComputedHeight != NS_INTRINSICSIZE &&
                borderPadding.top + mLegendSpace+kidDesiredSize.height > aReflowState.mComputedHeight) {
              kidDesiredSize.height = aReflowState.mComputedHeight-(borderPadding.top + mLegendSpace);
            }

            FinishReflowChild(mContentFrame, aPresContext, kidDesiredSize, contentRect.x, contentRect.y, 0);

            nsFrameState  kidState;
            mContentFrame->GetFrameState(&kidState);

           // printf("width: %d, height: %d\n", desiredSize.mCombinedArea.width, desiredSize.mCombinedArea.height);

            /*
            if (kidState & NS_FRAME_OUTSIDE_CHILDREN) {
                 mState |= NS_FRAME_OUTSIDE_CHILDREN;
                 aDesiredSize.mOverflowArea.width += borderPadding.left + borderPadding.right;
                 aDesiredSize.mOverflowArea.height += borderPadding.top + borderPadding.bottom + mLegendSpace;
            }
            */

            NS_FRAME_TRACE_REFLOW_OUT("FieldSet::Reflow", aStatus);

        } else {
            // if we don't need to reflow just get the old size
            mContentFrame->GetRect(contentRect);
            const nsStyleMargin* marginStyle;
            mContentFrame->GetStyleData(eStyleStruct_Margin,
                                  (const nsStyleStruct*&) marginStyle);

            nsMargin m(0,0,0,0);
            marginStyle->GetMargin(m);
            contentRect.Inflate(m);
        }
    }

    if (mLegendFrame) {
      if (contentRect.width > mLegendRect.width) {
        PRInt32 align = ((nsLegendFrame*)mLegendFrame)->GetAlign();
  
        switch(align) {
          case NS_STYLE_TEXT_ALIGN_RIGHT:
            mLegendRect.x = contentRect.width - mLegendRect.width + borderPadding.left;
            break;
          case NS_STYLE_TEXT_ALIGN_CENTER:
            mLegendRect.x = contentRect.width/2 - mLegendRect.width/2 + borderPadding.left;
        }
  
      } else {
        contentRect.width = mLegendRect.width + borderPadding.left + borderPadding.right;
        aDesiredSize.width = contentRect.width;
      }
      // place the legend
      nsRect actualLegendRect(mLegendRect);
      actualLegendRect.Deflate(legendMargin);

      nsPoint curOrigin;
      mLegendFrame->GetOrigin(curOrigin);

      // only if the origin changed
      if ((curOrigin.x != mLegendRect.x) || (curOrigin.y != mLegendRect.y)) {
          mLegendFrame->MoveTo(aPresContext,  actualLegendRect.x , actualLegendRect.y);
          nsContainerFrame::PositionFrameView(aPresContext, mLegendFrame);

          // We need to recursively process the legend frame's
          // children since we're moving the frame after Reflow.
          nsContainerFrame::PositionChildViews(aPresContext, mLegendFrame);
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

    if (aReflowState.mComputedWidth == NS_INTRINSICSIZE) {
        aDesiredSize.width = borderPadding.left + 
                             contentRect.width +
                             borderPadding.right;
    } else {
       nscoord min = borderPadding.left + borderPadding.right + mLegendRect.width;
       aDesiredSize.width = aReflowState.mComputedWidth + borderPadding.left + borderPadding.right;
       if (aDesiredSize.width < min)
           aDesiredSize.width = min;
    }

    aDesiredSize.ascent  = aDesiredSize.height;
    aDesiredSize.descent = 0;
    aDesiredSize.mMaximumWidth = aDesiredSize.width;
    if (nsnull != aDesiredSize.maxElementSize) {
        // if the legend is wider use it
        if (aDesiredSize.maxElementSize->width < mLegendRect.width)
            aDesiredSize.maxElementSize->width = mLegendRect.width;

        // add in padding.
        aDesiredSize.maxElementSize->width += borderPadding.left + borderPadding.right;

        // height is border + legend
        aDesiredSize.maxElementSize->height += borderPadding.top + borderPadding.bottom + mLegendRect.height;
    }
#ifdef NOISY_REFLOW
    printf("FIELDSET:  w=%d, maxWidth=%d, MES=%d\n",
           aDesiredSize.width, aDesiredSize.mMaximumWidth, 
           aDesiredSize.maxElementSize ? aDesiredSize.maxElementSize->width : -1);
    if (aDesiredSize.mFlags & NS_REFLOW_CALC_MAX_WIDTH)
      printf("  and preferred size = %d\n", aDesiredSize.mMaximumWidth);

#endif
  return NS_OK;
}

PRIntn
nsFieldSetFrame::GetSkipSides() const
{
  return 0;
}

NS_IMETHODIMP
nsFieldSetFrame::AppendFrames(nsIPresContext* aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aFrameList)
{
  return mContentFrame->AppendFrames(aPresContext,
                                     aPresShell,
                                     aListName,
                                     aFrameList);
}

NS_IMETHODIMP
nsFieldSetFrame::InsertFrames(nsIPresContext* aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aPrevFrame,
                      nsIFrame*       aFrameList)
{
  return mContentFrame->InsertFrames(aPresContext,
                                     aPresShell,
                                     aListName,
                                     aPrevFrame,
                                     aFrameList);
}

NS_IMETHODIMP
nsFieldSetFrame::RemoveFrame(nsIPresContext* aPresContext,
                     nsIPresShell&   aPresShell,
                     nsIAtom*        aListName,
                     nsIFrame*       aOldFrame)
{
  // XXX XXX
  // XXX temporary fix for bug 70648
  if (aOldFrame == mLegendFrame) {   
    nsIFrame* sibling;
    mContentFrame->GetNextSibling(&sibling);
    NS_ASSERTION(sibling == mLegendFrame, "legendFrame is not next sibling");
#ifdef DEBUG
    nsIFrame* legendParent;
    mLegendFrame->GetParent(&legendParent);
    NS_ASSERTION(legendParent == this, "Legend Parent has wrong parent");
#endif
    nsIFrame* legendSibling;
    sibling->GetNextSibling(&legendSibling);
    // replace the legend, which is the next sibling, with any siblings of the legend (XXX always null?)
    mContentFrame->SetNextSibling(legendSibling);
    // OK, the legend is now removed from the sibling list, but who has ownership of it?
    mLegendFrame->Destroy(aPresContext);
    mLegendFrame = nsnull;
    return NS_OK;
  } else {
    return mContentFrame->RemoveFrame (aPresContext,
                                       aPresShell,
                                       aListName,
                                       aOldFrame);
  }
}


NS_IMETHODIMP
nsFieldSetFrame::ReplaceFrame(nsIPresContext* aPresContext,
                     nsIPresShell&   aPresShell,
                     nsIAtom*        aListName,
                     nsIFrame*       aOldFrame,
                     nsIFrame*       aNewFrame)
{
  return mContentFrame->ReplaceFrame (aPresContext,
                                     aPresShell,
                                     aListName,
                                     aOldFrame,
                                     aNewFrame);
}


