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

/* rendering object that goes directly inside the document's scrollbars */

#include "nsCanvasFrame.h"
#include "nsIServiceManager.h"
#include "nsHTMLParts.h"
#include "nsHTMLContainerFrame.h"
#include "nsCSSRendering.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsRenderingContext.h"
#include "nsGUIEvent.h"
#include "nsStyleConsts.h"
#include "nsGkAtoms.h"
#include "nsEventStateManager.h"
#include "nsIPresShell.h"
#include "nsIScrollPositionListener.h"
#include "nsDisplayList.h"
#include "nsAbsoluteContainingBlock.h"
#include "nsCSSFrameConstructor.h"
#include "nsFrameManager.h"

// for focus
#include "nsIScrollableFrame.h"
#include "nsIDocShell.h"

#ifdef DEBUG_rods
//#define DEBUG_CANVAS_FOCUS
#endif

#define CANVAS_ABS_POS_CHILD_LIST NS_CONTAINER_LIST_COUNT_INCL_OC


nsIFrame*
NS_NewCanvasFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsCanvasFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsCanvasFrame)

NS_QUERYFRAME_HEAD(nsCanvasFrame)
  NS_QUERYFRAME_ENTRY(nsCanvasFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsHTMLContainerFrame)

void
nsCanvasFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  mAbsoluteContainer.DestroyFrames(this, aDestructRoot);

  nsIScrollableFrame* sf =
    PresContext()->GetPresShell()->GetRootScrollFrameAsScrollable();
  if (sf) {
    sf->RemoveScrollPositionListener(this);
  }

  nsHTMLContainerFrame::DestroyFrom(aDestructRoot);
}

void
nsCanvasFrame::ScrollPositionWillChange(nscoord aX, nscoord aY)
{
  if (mDoPaintFocus) {
    mDoPaintFocus = PR_FALSE;
    PresContext()->FrameManager()->GetRootFrame()->InvalidateFrameSubtree();
  }
}

NS_IMETHODIMP
nsCanvasFrame::SetHasFocus(PRBool aHasFocus)
{
  if (mDoPaintFocus != aHasFocus) {
    mDoPaintFocus = aHasFocus;
    PresContext()->FrameManager()->GetRootFrame()->InvalidateFrameSubtree();

    if (!mAddedScrollPositionListener) {
      nsIScrollableFrame* sf =
        PresContext()->GetPresShell()->GetRootScrollFrameAsScrollable();
      if (sf) {
        sf->AddScrollPositionListener(this);
        mAddedScrollPositionListener = PR_TRUE;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCanvasFrame::SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList)
{
  if (kAbsoluteList == aListID)
    return mAbsoluteContainer.SetInitialChildList(this, aListID, aChildList);

  NS_ASSERTION(aListID != kPrincipalList ||
               aChildList.IsEmpty() || aChildList.OnlyChild(),
               "Primary child list can have at most one frame in it");
  return nsHTMLContainerFrame::SetInitialChildList(aListID, aChildList);
}

NS_IMETHODIMP
nsCanvasFrame::AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList)
{
  if (kAbsoluteList == aListID)
    return mAbsoluteContainer.AppendFrames(this, aListID, aFrameList);

  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list ID");
  NS_PRECONDITION(mFrames.IsEmpty(), "already have a child frame");
  if (aListID != kPrincipalList) {
    // We only support the Principal and Absolute child lists.
    return NS_ERROR_INVALID_ARG;
  }

  if (!mFrames.IsEmpty()) {
    // We only allow a single principal child frame.
    return NS_ERROR_INVALID_ARG;
  }

  // Insert the new frames
  NS_ASSERTION(aFrameList.FirstChild() == aFrameList.LastChild(),
               "Only one principal child frame allowed");
#ifdef NS_DEBUG
  nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
  mFrames.AppendFrames(nsnull, aFrameList);

  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                     NS_FRAME_HAS_DIRTY_CHILDREN);

  return NS_OK;
}

NS_IMETHODIMP
nsCanvasFrame::InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList)
{
  if (kAbsoluteList == aListID)
    return mAbsoluteContainer.InsertFrames(this, aListID, aPrevFrame, aFrameList);

  // Because we only support a single child frame inserting is the same
  // as appending
  NS_PRECONDITION(!aPrevFrame, "unexpected previous sibling frame");
  if (aPrevFrame)
    return NS_ERROR_UNEXPECTED;

  return AppendFrames(aListID, aFrameList);
}

NS_IMETHODIMP
nsCanvasFrame::RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame)
{
  if (kAbsoluteList == aListID) {
    mAbsoluteContainer.RemoveFrame(this, aListID, aOldFrame);
    return NS_OK;
  }

  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list ID");
  if (aListID != kPrincipalList) {
    // We only support the Principal and Absolute child lists.
    return NS_ERROR_INVALID_ARG;
  }

  if (aOldFrame != mFrames.FirstChild())
    return NS_ERROR_FAILURE;

  // It's our one and only child frame
  // Damage the area occupied by the deleted frame
  // The child of the canvas probably can't have an outline, but why bother
  // thinking about that?
  Invalidate(aOldFrame->GetVisualOverflowRect() + aOldFrame->GetPosition());

  // Remove the frame and destroy it
  mFrames.DestroyFrame(aOldFrame);

  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                     NS_FRAME_HAS_DIRTY_CHILDREN);
  return NS_OK;
}

nsFrameList
nsCanvasFrame::GetChildList(ChildListID aListID) const
{
  if (kAbsoluteList == aListID)
    return mAbsoluteContainer.GetChildList();

  return nsHTMLContainerFrame::GetChildList(aListID);
}

void
nsCanvasFrame::GetChildLists(nsTArray<ChildList>* aLists) const
{
  nsHTMLContainerFrame::GetChildLists(aLists);
  mAbsoluteContainer.AppendChildList(aLists, kAbsoluteList);
}

nsRect nsCanvasFrame::CanvasArea() const
{
  // Not clear which overflow rect we want here, but it probably doesn't
  // matter.
  nsRect result(GetVisualOverflowRect());

  nsIScrollableFrame *scrollableFrame = do_QueryFrame(GetParent());
  if (scrollableFrame) {
    nsRect portRect = scrollableFrame->GetScrollPortRect();
    result.UnionRect(result, nsRect(nsPoint(0, 0), portRect.Size()));
  }
  return result;
}

void
nsDisplayCanvasBackground::Paint(nsDisplayListBuilder* aBuilder,
                                 nsRenderingContext* aCtx)
{
  nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
  nsPoint offset = ToReferenceFrame();
  nsRect bgClipRect = frame->CanvasArea() + offset;

  if (NS_GET_A(mExtraBackgroundColor) > 0) {
    aCtx->SetColor(mExtraBackgroundColor);
    aCtx->FillRect(bgClipRect);
  }

  nsCSSRendering::PaintBackground(mFrame->PresContext(), *aCtx, mFrame,
                                  mVisibleRect,
                                  nsRect(offset, mFrame->GetSize()),
                                  aBuilder->GetBackgroundPaintFlags(),
                                  &bgClipRect);
}

/**
 * A display item to paint the focus ring for the document.
 *
 * The only reason this can't use nsDisplayGeneric is overriding GetBounds.
 */
class nsDisplayCanvasFocus : public nsDisplayItem {
public:
  nsDisplayCanvasFocus(nsDisplayListBuilder* aBuilder, nsCanvasFrame *aFrame)
    : nsDisplayItem(aBuilder, aFrame)
  {
    MOZ_COUNT_CTOR(nsDisplayCanvasFocus);
  }
  virtual ~nsDisplayCanvasFocus() {
    MOZ_COUNT_DTOR(nsDisplayCanvasFocus);
  }

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder)
  {
    // This is an overestimate, but that's not a problem.
    nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
    return frame->CanvasArea() + ToReferenceFrame();
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx)
  {
    nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
    frame->PaintFocus(*aCtx, ToReferenceFrame());
  }

  NS_DISPLAY_DECL_NAME("CanvasFocus", TYPE_CANVAS_FOCUS)
};

NS_IMETHODIMP
nsCanvasFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists)
{
  nsresult rv;

  if (GetPrevInFlow()) {
    DisplayOverflowContainers(aBuilder, aDirtyRect, aLists);
  }

  aBuilder->MarkFramesForDisplayList(this, mAbsoluteContainer.GetChildList(),
                                     aDirtyRect);
  
  // Force a background to be shown. We may have a background propagated to us,
  // in which case GetStyleBackground wouldn't have the right background
  // and the code in nsFrame::DisplayBorderBackgroundOutline might not give us
  // a background.
  // We don't have any border or outline, and our background draws over
  // the overflow area, so just add nsDisplayCanvasBackground instead of
  // calling DisplayBorderBackgroundOutline.
  if (IsVisibleForPainting(aBuilder)) { 
    rv = aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
           nsDisplayCanvasBackground(aBuilder, this));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsIFrame* kid;
  for (kid = GetFirstPrincipalChild(); kid; kid = kid->GetNextSibling()) {
    // Put our child into its own pseudo-stack.
    rv = BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
  }

#ifdef DEBUG_CANVAS_FOCUS
  nsCOMPtr<nsIContent> focusContent;
  aPresContext->EventStateManager()->
    GetFocusedContent(getter_AddRefs(focusContent));

  PRBool hasFocus = PR_FALSE;
  nsCOMPtr<nsISupports> container;
  aPresContext->GetContainer(getter_AddRefs(container));
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
  if (docShell) {
    docShell->GetHasFocus(&hasFocus);
    printf("%p - nsCanvasFrame::Paint R:%d,%d,%d,%d  DR: %d,%d,%d,%d\n", this, 
            mRect.x, mRect.y, mRect.width, mRect.height,
            aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  }
  printf("%p - Focus: %s   c: %p  DoPaint:%s\n", docShell.get(), hasFocus?"Y":"N", 
         focusContent.get(), mDoPaintFocus?"Y":"N");
#endif

  if (!mDoPaintFocus)
    return NS_OK;
  // Only paint the focus if we're visible
  if (!GetStyleVisibility()->IsVisible())
    return NS_OK;
  
  return aLists.Outlines()->AppendNewToTop(new (aBuilder)
      nsDisplayCanvasFocus(aBuilder, this));
}

void
nsCanvasFrame::PaintFocus(nsRenderingContext& aRenderingContext, nsPoint aPt)
{
  nsRect focusRect(aPt, GetSize());

  nsIScrollableFrame *scrollableFrame = do_QueryFrame(GetParent());
  if (scrollableFrame) {
    nsRect portRect = scrollableFrame->GetScrollPortRect();
    focusRect.width = portRect.width;
    focusRect.height = portRect.height;
    focusRect.MoveBy(scrollableFrame->GetScrollPosition());
  }

 // XXX use the root frame foreground color, but should we find BODY frame
 // for HTML documents?
  nsIFrame* root = mFrames.FirstChild();
  const nsStyleColor* color =
    root ? root->GetStyleContext()->GetStyleColor() :
           mStyleContext->GetStyleColor();
  if (!color) {
    NS_ERROR("current color cannot be found");
    return;
  }

  nsCSSRendering::PaintFocus(PresContext(), aRenderingContext,
                             focusRect, color->mColor);
}

/* virtual */ nscoord
nsCanvasFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetMinWidth(aRenderingContext);
  return result;
}

/* virtual */ nscoord
nsCanvasFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetPrefWidth(aRenderingContext);
  return result;
}

NS_IMETHODIMP
nsCanvasFrame::Reflow(nsPresContext*           aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsCanvasFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  NS_FRAME_TRACE_REFLOW_IN("nsCanvasFrame::Reflow");

  // Initialize OUT parameter
  aStatus = NS_FRAME_COMPLETE;

  nsCanvasFrame* prevCanvasFrame = static_cast<nsCanvasFrame*>
                                               (GetPrevInFlow());
  if (prevCanvasFrame) {
    nsAutoPtr<nsFrameList> overflow(prevCanvasFrame->StealOverflowFrames());
    if (overflow) {
      NS_ASSERTION(overflow->OnlyChild(),
                   "must have doc root as canvas frame's only child");
      nsContainerFrame::ReparentFrameViewList(aPresContext, *overflow,
                                              prevCanvasFrame, this);
      // Prepend overflow to the our child list. There may already be
      // children placeholders for fixed-pos elements, which don't get
      // reflowed but must not be lost until the canvas frame is destroyed.
      mFrames.InsertFrames(this, nsnull, *overflow);
    }
  }

  // Set our size up front, since some parts of reflow depend on it
  // being already set.  Note that the computed height may be
  // unconstrained; that's ok.  Consumers should watch out for that.
  SetSize(nsSize(aReflowState.ComputedWidth(), aReflowState.ComputedHeight())); 

  // Reflow our one and only normal child frame. It's either the root
  // element's frame or a placeholder for that frame, if the root element
  // is abs-pos or fixed-pos. We may have additional children which
  // are placeholders for continuations of fixed-pos content, but those
  // don't need to be reflowed. The normal child is always comes before
  // the fixed-pos placeholders, because we insert it at the start
  // of the child list, above.
  nsHTMLReflowMetrics kidDesiredSize;
  if (mFrames.IsEmpty()) {
    // We have no child frame, so return an empty size
    aDesiredSize.width = aDesiredSize.height = 0;
  } else {
    nsIFrame* kidFrame = mFrames.FirstChild();
    nsRect oldKidRect = kidFrame->GetRect();
    PRBool kidDirty = (kidFrame->GetStateBits() & NS_FRAME_IS_DIRTY) != 0;

    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame,
                                     nsSize(aReflowState.availableWidth,
                                            aReflowState.availableHeight));

    if (aReflowState.mFlags.mVResize &&
        (kidFrame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_HEIGHT)) {
      // Tell our kid it's being vertically resized too.  Bit of a
      // hack for framesets.
      kidReflowState.mFlags.mVResize = PR_TRUE;
    }

    nsPoint kidPt(kidReflowState.mComputedMargin.left,
                  kidReflowState.mComputedMargin.top);
    // Apply CSS relative positioning
    const nsStyleDisplay* styleDisp = kidFrame->GetStyleDisplay();
    if (NS_STYLE_POSITION_RELATIVE == styleDisp->mPosition) {
      kidPt += nsPoint(kidReflowState.mComputedOffsets.left,
                       kidReflowState.mComputedOffsets.top);
    }

    // Reflow the frame
    ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                kidPt.x, kidPt.y, 0, aStatus);

    // Complete the reflow and position and size the child frame
    FinishReflowChild(kidFrame, aPresContext, &kidReflowState, kidDesiredSize,
                      kidPt.x, kidPt.y, 0);

    if (!NS_FRAME_IS_FULLY_COMPLETE(aStatus)) {
      nsIFrame* nextFrame = kidFrame->GetNextInFlow();
      NS_ASSERTION(nextFrame || aStatus & NS_FRAME_REFLOW_NEXTINFLOW,
        "If it's incomplete and has no nif yet, it must flag a nif reflow.");
      if (!nextFrame) {
        nsresult rv = aPresContext->PresShell()->FrameConstructor()->
          CreateContinuingFrame(aPresContext, kidFrame, this, &nextFrame);
        NS_ENSURE_SUCCESS(rv, rv);
        SetOverflowFrames(aPresContext, nsFrameList(nextFrame, nextFrame));
        // Root overflow containers will be normal children of
        // the canvas frame, but that's ok because there
        // aren't any other frames we need to isolate them from
        // during reflow.
      }
      if (NS_FRAME_OVERFLOW_IS_INCOMPLETE(aStatus)) {
        nextFrame->AddStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
      }
    }

    // If the child frame was just inserted, then we're responsible for making sure
    // it repaints
    if (kidDirty) {
      // But we have a new child, which will affect our background, so
      // invalidate our whole rect.
      // Note: Even though we request to be sized to our child's size, our
      // scroll frame ensures that we are always the size of the viewport.
      // Also note: GetPosition() on a CanvasFrame is always going to return
      // (0, 0). We only want to invalidate GetRect() since Get*OverflowRect()
      // could also include overflow to our top and left (out of the viewport)
      // which doesn't need to be painted.
      nsIFrame* viewport = PresContext()->GetPresShell()->GetRootFrame();
      viewport->Invalidate(nsRect(nsPoint(0, 0), viewport->GetSize()));
    } else {
      nsRect newKidRect = kidFrame->GetRect();
      if (newKidRect.TopLeft() == oldKidRect.TopLeft()) {
        InvalidateRectDifference(oldKidRect, kidFrame->GetRect());
      } else {
        Invalidate(oldKidRect);
        Invalidate(newKidRect);
      }
    }
    
    // Return our desired size. Normally it's what we're told, but
    // sometimes we can be given an unconstrained height (when a window
    // is sizing-to-content), and we should compute our desired height.
    aDesiredSize.width = aReflowState.ComputedWidth();
    if (aReflowState.ComputedHeight() == NS_UNCONSTRAINEDSIZE) {
      aDesiredSize.height = kidFrame->GetRect().height +
        kidReflowState.mComputedMargin.TopBottom();
    } else {
      aDesiredSize.height = aReflowState.ComputedHeight();
    }

    aDesiredSize.SetOverflowAreasToDesiredBounds();
    aDesiredSize.mOverflowAreas.UnionWith(
      kidDesiredSize.mOverflowAreas + kidPt);

    if (mAbsoluteContainer.HasAbsoluteFrames()) {
      PRBool widthChanged = aDesiredSize.width != mRect.width;
      PRBool heightChanged = aDesiredSize.height != mRect.height;
      mAbsoluteContainer.Reflow(this, aPresContext, aReflowState, aStatus,
                                aDesiredSize.width, aDesiredSize.height,
                                PR_TRUE, widthChanged, heightChanged,
                                &aDesiredSize.mOverflowAreas);
    }

    // Handle invalidating fixed-attachment backgrounds propagated to the
    // canvas when the canvas size (and therefore the background positioning
    // area's size) changes.  Such backgrounds are not invalidated in the
    // normal manner because the size of the original frame for that background
    // may not have changed.
    //
    // This isn't the right fix for this issue, taken more generally.  In
    // particular, this doesn't handle fixed-attachment backgrounds that are *not*
    // propagated.  If a layer with the characteristics tested for below exists
    // in a non-propagated background, we should invalidate the "corresponding"
    // frame (which subsumes this special case if defined broadly).  For now,
    // however, this addresses the most common case.  Given that this behavior has
    // long been broken (non-zero percent background-size may be a new instance,
    // but non-zero percent background-position is longstanding), we defer a
    // fully correct fix until later.
    if (nsSize(aDesiredSize.width, aDesiredSize.height) != GetSize()) {
      nsIFrame* rootElementFrame =
        aPresContext->PresShell()->FrameConstructor()->GetRootElementStyleFrame();
      nsStyleContext* bgSC =
        nsCSSRendering::FindCanvasBackground(this, rootElementFrame);
      const nsStyleBackground* bg = bgSC->GetStyleBackground();
      if (!bg->IsTransparent()) {
        NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(i, bg) {
          const nsStyleBackground::Layer& layer = bg->mLayers[i];
          if (layer.mAttachment == NS_STYLE_BG_ATTACHMENT_FIXED &&
              layer.RenderingMightDependOnFrameSize()) {
            Invalidate(nsRect(nsPoint(0, 0), GetSize()));
            break;
          }
        }
      }
    }
  }

  if (prevCanvasFrame) {
    ReflowOverflowContainerChildren(aPresContext, aReflowState,
                                    aDesiredSize.mOverflowAreas, 0,
                                    aStatus);
  }

  FinishAndStoreOverflow(&aDesiredSize);

  NS_FRAME_TRACE_REFLOW_OUT("nsCanvasFrame::Reflow", aStatus);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

PRIntn
nsCanvasFrame::GetSkipSides() const
{
  return 0;
}

nsIAtom*
nsCanvasFrame::GetType() const
{
  return nsGkAtoms::canvasFrame;
}

NS_IMETHODIMP 
nsCanvasFrame::GetContentForEvent(nsPresContext* aPresContext,
                                nsEvent* aEvent,
                                nsIContent** aContent)
{
  NS_ENSURE_ARG_POINTER(aContent);
  nsresult rv = nsFrame::GetContentForEvent(aPresContext,
                                            aEvent,
                                            aContent);
  if (NS_FAILED(rv) || !*aContent) {
    nsIFrame* kid = mFrames.FirstChild();
    if (kid) {
      rv = kid->GetContentForEvent(aPresContext,
                                   aEvent,
                                   aContent);
    }
  }

  return rv;
}

#ifdef DEBUG
NS_IMETHODIMP
nsCanvasFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Canvas"), aResult);
}
#endif
