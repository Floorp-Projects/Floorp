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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

/* base class #1 for rendering objects that have child lists */

#include "nsContainerFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsIRenderingContext.h"
#include "nsStyleContext.h"
#include "nsRect.h"
#include "nsPoint.h"
#include "nsGUIEvent.h"
#include "nsStyleConsts.h"
#include "nsIView.h"
#include "nsHTMLContainerFrame.h"
#include "nsFrameManager.h"
#include "nsIPresShell.h"
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsGfxCIID.h"
#include "nsIServiceManager.h"
#include "nsCSSRendering.h"
#include "nsTransform2D.h"
#include "nsRegion.h"
#include "nsLayoutErrors.h"
#include "nsDisplayList.h"
#include "nsContentErrors.h"
#include "nsIEventStateManager.h"

#ifdef NS_DEBUG
#undef NOISY
#else
#undef NOISY
#endif

nsContainerFrame::~nsContainerFrame()
{
}

NS_IMETHODIMP
nsContainerFrame::Init(nsIContent* aContent,
                       nsIFrame*   aParent,
                       nsIFrame*   aPrevInFlow)
{
  nsresult rv = nsSplittableFrame::Init(aContent, aParent, aPrevInFlow);
  if (aPrevInFlow) {
    // Make sure we copy bits from our prev-in-flow that will affect
    // us. A continuation for a container frame needs to know if it
    // has a child with a view so that we'll properly reposition it.
    if (aPrevInFlow->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW)
      AddStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW);
  }
  return rv;
}

NS_IMETHODIMP
nsContainerFrame::SetInitialChildList(nsIAtom*  aListName,
                                      nsIFrame* aChildList)
{
  nsresult  result;
  if (!mFrames.IsEmpty()) {
    // We already have child frames which means we've already been
    // initialized
    NS_NOTREACHED("unexpected second call to SetInitialChildList");
    result = NS_ERROR_UNEXPECTED;
  } else if (aListName) {
    // All we know about is the unnamed principal child list
    NS_NOTREACHED("unknown frame list");
    result = NS_ERROR_INVALID_ARG;
  } else {
#ifdef NS_DEBUG
    nsFrame::VerifyDirtyBitSet(aChildList);
#endif
    mFrames.SetFrames(aChildList);
    result = NS_OK;
  }
  return result;
}

NS_IMETHODIMP
nsContainerFrame::AppendFrames(nsIAtom*  aListName,
                               nsIFrame* aFrameList)
{
  if (nsnull != aListName) {
#ifdef IBMBIDI
    if (aListName != nsGkAtoms::nextBidi)
#endif
    {
      NS_ERROR("unexpected child list");
      return NS_ERROR_INVALID_ARG;
    }
  }
  if (aFrameList) {
    mFrames.AppendFrames(this, aFrameList);

    // Ask the parent frame to reflow me.
#ifdef IBMBIDI
    if (nsnull == aListName)
#endif
    {
      AddStateBits(NS_FRAME_IS_DIRTY);
      PresContext()->PresShell()->
        FrameNeedsReflow(this, nsIPresShell::eTreeChange);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsContainerFrame::InsertFrames(nsIAtom*  aListName,
                               nsIFrame* aPrevFrame,
                               nsIFrame* aFrameList)
{
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");

  if (nsnull != aListName) {
#ifdef IBMBIDI
    if (aListName != nsGkAtoms::nextBidi)
#endif
    {
      NS_ERROR("unexpected child list");
      return NS_ERROR_INVALID_ARG;
    }
  }
  if (aFrameList) {
    // Insert frames after aPrevFrame
    mFrames.InsertFrames(this, aPrevFrame, aFrameList);

#ifdef IBMBIDI
    if (nsnull == aListName)
#endif
    {
      // Ask the parent frame to reflow me.
      AddStateBits(NS_FRAME_IS_DIRTY);
      PresContext()->PresShell()->
        FrameNeedsReflow(this, nsIPresShell::eTreeChange);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsContainerFrame::RemoveFrame(nsIAtom*  aListName,
                              nsIFrame* aOldFrame)
{
  if (nsnull != aListName) {
#ifdef IBMBIDI
    if (nsGkAtoms::nextBidi != aListName)
#endif
    {
      NS_ERROR("unexpected child list");
      return NS_ERROR_INVALID_ARG;
    }
  }

  if (aOldFrame) {
    // Loop and destroy the frame and all of its continuations.
    // If the frame we are removing is a brFrame, we need a reflow so
    // the line the brFrame was on can attempt to pull up any frames
    // that can fit from lines below it.
    PRBool generateReflowCommand =
      aOldFrame->GetType() == nsGkAtoms::brFrame;

    nsContainerFrame* parent = NS_STATIC_CAST(nsContainerFrame*, aOldFrame->GetParent());
    while (aOldFrame) {
#ifdef IBMBIDI
      if (nsGkAtoms::nextBidi != aListName) {
#endif
      // If the frame being removed has zero size then don't bother
      // generating a reflow command, otherwise make sure we do.
      nsRect bbox = aOldFrame->GetRect();
      if (bbox.width || bbox.height) {
        generateReflowCommand = PR_TRUE;
      }
#ifdef IBMBIDI
      }
#endif

      // When the parent is an inline frame we have a simple task - just
      // remove the frame from its parents list and generate a reflow
      // command.
      nsIFrame* oldFrameNextContinuation = aOldFrame->GetNextContinuation();
      parent->mFrames.DestroyFrame(aOldFrame);
      aOldFrame = oldFrameNextContinuation;
      if (aOldFrame) {
        parent = NS_STATIC_CAST(nsContainerFrame*, aOldFrame->GetParent());
      }
    }

    if (generateReflowCommand) {
      // Ask the parent frame to reflow me.
      AddStateBits(NS_FRAME_IS_DIRTY);
      PresContext()->PresShell()->
        FrameNeedsReflow(this, nsIPresShell::eTreeChange);
    }
  }

  return NS_OK;
}

void
nsContainerFrame::CleanupGeneratedContentIn(nsIContent* aRealContent,
                                            nsIFrame* aRoot) {
  nsIAtom* frameList = nsnull;
  PRInt32 listIndex = 0;
  do {
    nsIFrame* child = aRoot->GetFirstChild(frameList);
    while (child) {
      nsIContent* content = child->GetContent();
      if (content && content != aRealContent) {
        // Tell the ESM that this content is going away now, so it'll update
        // its hover content, etc.
        aRoot->PresContext()->EventStateManager()->ContentRemoved(content);
        content->UnbindFromTree();
      }
      CleanupGeneratedContentIn(aRealContent, child);
      child = child->GetNextSibling();
    }
    frameList = aRoot->GetAdditionalChildListName(listIndex++);
  } while (frameList);
}

void
nsContainerFrame::Destroy()
{
  // Prevent event dispatch during destruction
  if (HasView()) {
    GetView()->SetClientData(nsnull);
  }

  // Delete the primary child list
  mFrames.DestroyFrames();
  
  // Destroy overflow frames now
  nsFrameList overflowFrames(GetOverflowFrames(PresContext(), PR_TRUE));
  overflowFrames.DestroyFrames();

  // Destroy the frame and remove the flow pointers
  nsSplittableFrame::Destroy();
}

/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

nsIFrame*
nsContainerFrame::GetFirstChild(nsIAtom* aListName) const
{
  // We only know about the unnamed principal child list and the overflow
  // list
  if (nsnull == aListName) {
    return mFrames.FirstChild();
  } else if (nsGkAtoms::overflowList == aListName) {
    return GetOverflowFrames(PresContext(), PR_FALSE);
  } else {
    return nsnull;
  }
}

nsIAtom*
nsContainerFrame::GetAdditionalChildListName(PRInt32 aIndex) const
{
  if (aIndex == 0) {
    return nsGkAtoms::overflowList;
  } else {
    return nsnull;
  }
}

/////////////////////////////////////////////////////////////////////////////
// Painting/Events

NS_IMETHODIMP
nsContainerFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                   const nsRect&           aDirtyRect,
                                   const nsDisplayListSet& aLists)
{
  nsresult rv = DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  return BuildDisplayListForNonBlockChildren(aBuilder, aDirtyRect, aLists);
}

nsresult
nsContainerFrame::BuildDisplayListForNonBlockChildren(nsDisplayListBuilder*   aBuilder,
                                                      const nsRect&           aDirtyRect,
                                                      const nsDisplayListSet& aLists,
                                                      PRUint32                aFlags)
{
  nsIFrame* kid = mFrames.FirstChild();
  // Put each child's background directly onto the content list
  nsDisplayListSet set(aLists, aLists.Content());
  // The children should be in content order
  while (kid) {
    nsresult rv = BuildDisplayListForChild(aBuilder, kid, aDirtyRect, set, aFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    kid = kid->GetNextSibling();
  }
  return NS_OK;
}

/* virtual */ void
nsContainerFrame::ChildIsDirty(nsIFrame* aChild)
{
  AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
}

PRBool
nsContainerFrame::IsLeaf() const
{
  return PR_FALSE;
}

PRBool
nsContainerFrame::PeekOffsetNoAmount(PRBool aForward, PRInt32* aOffset)
{
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  // Don't allow the caret to stay in an empty (leaf) container frame.
  return PR_FALSE;
}

PRBool
nsContainerFrame::PeekOffsetCharacter(PRBool aForward, PRInt32* aOffset)
{
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  // Don't allow the caret to stay in an empty (leaf) container frame.
  return PR_FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// Helper member functions

/**
 * Position the view associated with |aKidFrame|, if there is one. A
 * container frame should call this method after positioning a frame,
 * but before |Reflow|.
 */
void
nsContainerFrame::PositionFrameView(nsIFrame* aKidFrame)
{
  nsIFrame* parentFrame = aKidFrame->GetParent();
  if (!aKidFrame->HasView() || !parentFrame)
    return;

  nsIView* view = aKidFrame->GetView();
  nsIViewManager* vm = view->GetViewManager();
  nsPoint pt;
  nsIView* ancestorView = parentFrame->GetClosestView(&pt);

  if (ancestorView != view->GetParent()) {
    NS_ASSERTION(ancestorView == view->GetParent()->GetParent(),
                 "Allowed only one anonymous view between frames");
    // parentFrame is responsible for positioning aKidFrame's view
    // explicitly
    return;
  }

  pt += aKidFrame->GetPosition();
  vm->MoveViewTo(view, pt.x, pt.y);
}

// This code is duplicated in nsDisplayList.cpp. We'll remove the duplication
// when we remove all this view sync code.
static PRBool
NonZeroStyleCoord(const nsStyleCoord& aCoord) {
  switch (aCoord.GetUnit()) {
  case eStyleUnit_Percent:
    return aCoord.GetPercentValue() > 0;
  case eStyleUnit_Coord:
    return aCoord.GetCoordValue() > 0;
  case eStyleUnit_Null:
    return PR_FALSE;
  default:
    return PR_TRUE;
  }
}

static PRBool
HasNonZeroBorderRadius(nsStyleContext* aStyleContext) {
  const nsStyleBorder* border = aStyleContext->GetStyleBorder();

  nsStyleCoord coord;
  border->mBorderRadius.GetTop(coord);
  if (NonZeroStyleCoord(coord)) return PR_TRUE;
  border->mBorderRadius.GetRight(coord);
  if (NonZeroStyleCoord(coord)) return PR_TRUE;
  border->mBorderRadius.GetBottom(coord);
  if (NonZeroStyleCoord(coord)) return PR_TRUE;
  border->mBorderRadius.GetLeft(coord);
  if (NonZeroStyleCoord(coord)) return PR_TRUE;

  return PR_FALSE;
}

static void
SyncFrameViewGeometryDependentProperties(nsPresContext*  aPresContext,
                                         nsIFrame*        aFrame,
                                         nsStyleContext*  aStyleContext,
                                         nsIView*         aView,
                                         PRUint32         aFlags)
{
  nsIViewManager* vm = aView->GetViewManager();

  PRBool isCanvas;
  const nsStyleBackground* bg;
  PRBool hasBG =
    nsCSSRendering::FindBackground(aPresContext, aFrame, &bg, &isCanvas);

  const nsStyleDisplay* display = aStyleContext->GetStyleDisplay();
  // If the frame has a solid background color, 'background-clip:border',
  // and it's a kind of frame that paints its background, and rounded borders aren't
  // clipping the background, then it's opaque.
  // If the frame has a native theme appearance then its background
  // color is actually not relevant.
  PRBool  viewHasTransparentContent =
    !(hasBG && !(bg->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT) &&
      !display->mAppearance && bg->mBackgroundClip == NS_STYLE_BG_CLIP_BORDER &&
      !HasNonZeroBorderRadius(aStyleContext));

  if (isCanvas) {
    nsIView* rootView;
    vm->GetRootView(rootView);
    nsIView* rootParent = rootView->GetParent();
    if (!rootParent) {
      // We're the root of a view manager hierarchy. We will have to
      // paint something. NOTE: this can be overridden below.
      viewHasTransparentContent = PR_FALSE;
    }

    nsIDocument *doc = aPresContext->PresShell()->GetDocument();
    if (doc) {
      nsIContent *rootElem = doc->GetRootContent();
      if (!doc->GetParentDocument() &&
          (nsCOMPtr<nsISupports>(doc->GetContainer())) &&
          rootElem && rootElem->IsNodeOfType(nsINode::eXUL)) {
        // we're XUL at the root of the document hierarchy. Try to make our
        // window translucent.
        // don't proceed unless this is the root view
        // (sometimes the non-root-view is a canvas)
        if (aView->HasWidget() && aView == rootView) {
          viewHasTransparentContent = hasBG && (bg->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT);
          aView->GetWidget()->SetWindowTranslucency(viewHasTransparentContent);
        }
      }
    }
  }
  // XXX we should also set widget transparency for XUL popups
}

void
nsContainerFrame::SyncFrameViewAfterReflow(nsPresContext* aPresContext,
                                           nsIFrame*       aFrame,
                                           nsIView*        aView,
                                           const nsRect*   aCombinedArea,
                                           PRUint32        aFlags)
{
  if (!aView) {
    return;
  }

  NS_ASSERTION(aCombinedArea, "Combined area must be passed in now");

  // Make sure the view is sized and positioned correctly
  if (0 == (aFlags & NS_FRAME_NO_MOVE_VIEW)) {
    PositionFrameView(aFrame);
  }

  if (0 == (aFlags & NS_FRAME_NO_SIZE_VIEW)) {
    nsIViewManager* vm = aView->GetViewManager();

    vm->ResizeView(aView, *aCombinedArea, PR_TRUE);

    // Even if the size hasn't changed, we need to sync up the
    // geometry dependent properties, because (kidState &
    // NS_FRAME_OUTSIDE_CHILDREN) might have changed, and we can't
    // detect whether it has or not. Likewise, whether the view size
    // has changed or not, we may need to change the transparency
    // state even if there is no clip.
    nsStyleContext* savedStyleContext = aFrame->GetStyleContext();
    SyncFrameViewGeometryDependentProperties(aPresContext, aFrame, savedStyleContext, aView, aFlags);
  }
}

void
nsContainerFrame::SyncFrameViewProperties(nsPresContext*  aPresContext,
                                          nsIFrame*        aFrame,
                                          nsStyleContext*  aStyleContext,
                                          nsIView*         aView,
                                          PRUint32         aFlags)
{
  NS_ASSERTION(!aStyleContext || aFrame->GetStyleContext() == aStyleContext,
               "Wrong style context for frame?");

  if (!aView) {
    return;
  }

  nsIViewManager* vm = aView->GetViewManager();

  if (nsnull == aStyleContext) {
    aStyleContext = aFrame->GetStyleContext();
  }

  // Make sure visibility is correct
  if (0 == (aFlags & NS_FRAME_NO_VISIBILITY)) {
    // See if the view should be hidden or visible
    PRBool  viewIsVisible = PR_TRUE;

    if (!aStyleContext->GetStyleVisibility()->IsVisible() &&
        !aFrame->SupportsVisibilityHidden()) {
      // If it's a scrollable frame that can't hide its scrollbars,
      // hide the view. This means that child elements can't override
      // their parent's visibility, but it's not practical to leave it
      // visible in all cases because the scrollbars will be showing
      // XXXldb Does the view system really enforce this correctly?
      viewIsVisible = PR_FALSE;
    } else {
      // if the view is for a popup, don't show the view if the popup is closed
      nsIWidget* widget = aView->GetWidget();
      if (widget) {
        nsWindowType windowType;
        widget->GetWindowType(windowType);
        if (windowType == eWindowType_popup) {
          widget->IsVisible(viewIsVisible);
        }
      }
    }

    vm->SetViewVisibility(aView, viewIsVisible ? nsViewVisibility_kShow :
                          nsViewVisibility_kHide);
  }

  // See if the frame is being relatively positioned or absolutely
  // positioned
  PRBool isPositioned = aStyleContext->GetStyleDisplay()->IsPositioned();

  PRInt32 zIndex = 0;
  PRBool  autoZIndex = PR_FALSE;

  if (!isPositioned) {
    autoZIndex = PR_TRUE;
  } else {
    // Make sure z-index is correct
    const nsStylePosition* position = aStyleContext->GetStylePosition();

    if (position->mZIndex.GetUnit() == eStyleUnit_Integer) {
      zIndex = position->mZIndex.GetIntValue();
    } else if (position->mZIndex.GetUnit() == eStyleUnit_Auto) {
      autoZIndex = PR_TRUE;
    }
  }

  vm->SetViewZIndex(aView, autoZIndex, zIndex, isPositioned);

  SyncFrameViewGeometryDependentProperties(aPresContext, aFrame, aStyleContext, aView, aFlags);
}

PRBool
nsContainerFrame::FrameNeedsView(nsIFrame* aFrame)
{
  // XXX Check needed because frame construction can't properly figure out when
  // a frame is the child of a scrollframe
  if (aFrame->GetStyleContext()->GetPseudoType() ==
      nsCSSAnonBoxes::scrolledContent) {
    return PR_TRUE;
  }
  return aFrame->NeedsView();
}

static nscoord GetCoord(const nsStyleCoord& aCoord, nscoord aIfNotCoord)
{
  return aCoord.GetUnit() == eStyleUnit_Coord
           ? aCoord.GetCoordValue()
           : aIfNotCoord;
}

void
nsContainerFrame::DoInlineIntrinsicWidth(nsIRenderingContext *aRenderingContext,
                                         InlineIntrinsicWidthData *aData,
                                         nsLayoutUtils::IntrinsicWidthType aType)
{
  if (GetPrevInFlow())
    return; // Already added.

  NS_PRECONDITION(aType == nsLayoutUtils::MIN_WIDTH ||
                  aType == nsLayoutUtils::PREF_WIDTH, "bad type");

  PRUint8 startSide, endSide;
  if (GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_LTR) {
    startSide = NS_SIDE_LEFT;
    endSide = NS_SIDE_RIGHT;
  } else {
    startSide = NS_SIDE_RIGHT;
    endSide = NS_SIDE_LEFT;
  }

  const nsStylePadding *stylePadding = GetStylePadding();
  const nsStyleBorder *styleBorder = GetStyleBorder();
  const nsStyleMargin *styleMargin = GetStyleMargin();
  nsStyleCoord tmp;

  // This goes at the beginning no matter how things are broken and how
  // messy the bidi situations are, since per CSS2.1 section 8.6
  // (implemented in bug 328168), the startSide border is always on the
  // first line.
  aData->currentLine +=
    GetCoord(stylePadding->mPadding.Get(startSide, tmp), 0) +
    styleBorder->GetBorderWidth(startSide) +
    GetCoord(styleMargin->mMargin.Get(startSide, tmp), 0);

  for (nsContainerFrame *nif = this; nif;
       nif = (nsContainerFrame*) nif->GetNextInFlow()) {
    for (nsIFrame *kid = nif->mFrames.FirstChild(); kid;
         kid = kid->GetNextSibling()) {
      if (aType == nsLayoutUtils::MIN_WIDTH)
        kid->AddInlineMinWidth(aRenderingContext,
                               NS_STATIC_CAST(InlineMinWidthData*, aData));
      else
        kid->AddInlinePrefWidth(aRenderingContext,
                                NS_STATIC_CAST(InlinePrefWidthData*, aData));
    }
  }

  // This goes at the end no matter how things are broken and how
  // messy the bidi situations are, since per CSS2.1 section 8.6
  // (implemented in bug 328168), the endSide border is always on the
  // last line.
  aData->currentLine +=
    GetCoord(stylePadding->mPadding.Get(endSide, tmp), 0) +
    styleBorder->GetBorderWidth(endSide) +
    GetCoord(styleMargin->mMargin.Get(endSide, tmp), 0);
}

/* virtual */ nsSize
nsContainerFrame::ComputeAutoSize(nsIRenderingContext *aRenderingContext,
                                  nsSize aCBSize, nscoord aAvailableWidth,
                                  nsSize aMargin, nsSize aBorder,
                                  nsSize aPadding, PRBool aShrinkWrap)
{
  nsSize result(0xdeadbeef, NS_UNCONSTRAINEDSIZE);
  nscoord availBased = aAvailableWidth - aMargin.width - aBorder.width -
                       aPadding.width;
  // replaced elements always shrink-wrap
  if (aShrinkWrap || IsFrameOfType(eReplaced)) {
    // don't bother setting it if the result won't be used
    if (GetStylePosition()->mWidth.GetUnit() == eStyleUnit_Auto) {
      result.width = ShrinkWidthToFit(aRenderingContext, availBased);
    }
  } else {
    result.width = availBased;
  }
  return result;
}

/**
 * Invokes the WillReflow() function, positions the frame and its view (if
 * requested), and then calls Reflow(). If the reflow succeeds and the child
 * frame is complete, deletes any next-in-flows using DeleteNextInFlowChild()
 */
nsresult
nsContainerFrame::ReflowChild(nsIFrame*                aKidFrame,
                              nsPresContext*           aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nscoord                  aX,
                              nscoord                  aY,
                              PRUint32                 aFlags,
                              nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(aReflowState.frame == aKidFrame, "bad reflow state");

  nsresult  result;

  // Send the WillReflow() notification, and position the child frame
  // and its view if requested
  aKidFrame->WillReflow(aPresContext);

  if (0 == (aFlags & NS_FRAME_NO_MOVE_FRAME)) {
    aKidFrame->SetPosition(nsPoint(aX, aY));
  }

  if (0 == (aFlags & NS_FRAME_NO_MOVE_VIEW)) {
    PositionFrameView(aKidFrame);
  }

  // Reflow the child frame
  result = aKidFrame->Reflow(aPresContext, aDesiredSize, aReflowState,
                             aStatus);

  // If the reflow was successful and the child frame is complete, delete any
  // next-in-flows
  if (NS_SUCCEEDED(result) && NS_FRAME_IS_COMPLETE(aStatus)) {
    nsIFrame* kidNextInFlow = aKidFrame->GetNextInFlow();
    if (nsnull != kidNextInFlow) {
      // Remove all of the childs next-in-flows. Make sure that we ask
      // the right parent to do the removal (it's possible that the
      // parent is not this because we are executing pullup code)
      NS_STATIC_CAST(nsContainerFrame*, kidNextInFlow->GetParent())
        ->DeleteNextInFlowChild(aPresContext, kidNextInFlow);
    }
  }
  return result;
}


/**
 * Position the views of |aFrame|'s descendants. A container frame
 * should call this method if it moves a frame after |Reflow|.
 */
void
nsContainerFrame::PositionChildViews(nsIFrame* aFrame)
{
  if (!(aFrame->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW)) {
    return;
  }

  nsIAtom*  childListName = nsnull;
  PRInt32   childListIndex = 0;

  do {
    // Recursively walk aFrame's child frames
    nsIFrame* childFrame = aFrame->GetFirstChild(childListName);
    while (childFrame) {
      // Position the frame's view (if it has one) otherwise recursively
      // process its children
      if (childFrame->HasView()) {
        PositionFrameView(childFrame);
      } else {
        PositionChildViews(childFrame);
      }

      // Get the next sibling child frame
      childFrame = childFrame->GetNextSibling();
    }

    childListName = aFrame->GetAdditionalChildListName(childListIndex++);
  } while (childListName);
}

/**
 * The second half of frame reflow. Does the following:
 * - sets the frame's bounds
 * - sizes and positions (if requested) the frame's view. If the frame's final
 *   position differs from the current position and the frame itself does not
 *   have a view, then any child frames with views are positioned so they stay
 *   in sync
 * - sets the view's visibility, opacity, content transparency, and clip
 * - invoked the DidReflow() function
 *
 * Flags:
 * NS_FRAME_NO_MOVE_FRAME - don't move the frame. aX and aY are ignored in this
 *    case. Also implies NS_FRAME_NO_MOVE_VIEW
 * NS_FRAME_NO_MOVE_VIEW - don't position the frame's view. Set this if you
 *    don't want to automatically sync the frame and view
 * NS_FRAME_NO_SIZE_VIEW - don't size the frame's view
 */
nsresult
nsContainerFrame::FinishReflowChild(nsIFrame*                 aKidFrame,
                                    nsPresContext*            aPresContext,
                                    const nsHTMLReflowState*  aReflowState,
                                    nsHTMLReflowMetrics&      aDesiredSize,
                                    nscoord                   aX,
                                    nscoord                   aY,
                                    PRUint32                  aFlags)
{
  nsPoint curOrigin = aKidFrame->GetPosition();
  nsRect  bounds(aX, aY, aDesiredSize.width, aDesiredSize.height);

  aKidFrame->SetRect(bounds);

  if (aKidFrame->HasView()) {
    nsIView* view = aKidFrame->GetView();
    // Make sure the frame's view is properly sized and positioned and has
    // things like opacity correct
    SyncFrameViewAfterReflow(aPresContext, aKidFrame, view,
                             &aDesiredSize.mOverflowArea,
                             aFlags);
  }

  if (!(aFlags & NS_FRAME_NO_MOVE_VIEW) &&
      (curOrigin.x != aX || curOrigin.y != aY)) {
    if (!aKidFrame->HasView()) {
      // If the frame has moved, then we need to make sure any child views are
      // correctly positioned
      PositionChildViews(aKidFrame);
    }

    // We also need to redraw everything associated with the frame
    // because if the frame's Reflow issued any invalidates, then they
    // will be at the wrong offset ... note that this includes
    // invalidates issued against the frame's children, so we need to
    // invalidate the overflow area too.
    aKidFrame->Invalidate(aDesiredSize.mOverflowArea);
  }
  
  return aKidFrame->DidReflow(aPresContext, aReflowState, NS_FRAME_REFLOW_FINISHED);
}

/**
 * Remove and delete aNextInFlow and its next-in-flows. Updates the sibling and flow
 * pointers
 */
void
nsContainerFrame::DeleteNextInFlowChild(nsPresContext* aPresContext,
                                        nsIFrame*       aNextInFlow)
{
  nsIFrame* prevInFlow = aNextInFlow->GetPrevInFlow();
  NS_PRECONDITION(prevInFlow, "bad prev-in-flow");

  // If the next-in-flow has a next-in-flow then delete it, too (and
  // delete it first).
  // Do this in a loop so we don't overflow the stack for frames
  // with very many next-in-flows
  nsIFrame* nextNextInFlow = aNextInFlow->GetNextInFlow();
  if (nextNextInFlow) {
    nsAutoVoidArray frames;
    for (nsIFrame* f = nextNextInFlow; f; f = f->GetNextInFlow()) {
      frames.AppendElement(f);
    }
    for (PRInt32 i = frames.Count() - 1; i >= 0; --i) {
      nsIFrame* delFrame = NS_STATIC_CAST(nsIFrame*, frames.ElementAt(i));
      NS_STATIC_CAST(nsContainerFrame*, delFrame->GetParent())
        ->DeleteNextInFlowChild(aPresContext, delFrame);
    }
  }

  // Disconnect the next-in-flow from the flow list
  nsSplittableFrame::BreakFromPrevFlow(aNextInFlow);

  // Take the next-in-flow out of the parent's child list
  PRBool result = mFrames.RemoveFrame(aNextInFlow);
  if (!result) {
    // We didn't find the child in the parent's principal child list.
    // Maybe it's on the overflow list?
    nsFrameList overflowFrames(GetOverflowFrames(aPresContext, PR_TRUE));

    if (overflowFrames.IsEmpty() || !overflowFrames.RemoveFrame(aNextInFlow)) {
      NS_ASSERTION(result, "failed to remove frame");
    }

    // Set the overflow property again
    if (overflowFrames.NotEmpty()) {
      SetOverflowFrames(aPresContext, overflowFrames.FirstChild());
    }
  }

  // Delete the next-in-flow frame and its descendants.
  aNextInFlow->Destroy();

  NS_POSTCONDITION(!prevInFlow->GetNextInFlow(), "non null next-in-flow");
}

nsIFrame*
nsContainerFrame::GetOverflowFrames(nsPresContext* aPresContext,
                                    PRBool          aRemoveProperty) const
{
  nsPropertyTable *propTable = aPresContext->PropertyTable();
  if (aRemoveProperty) {
    return (nsIFrame*) propTable->UnsetProperty(this,
                                              nsGkAtoms::overflowProperty);
  }

  return (nsIFrame*) propTable->GetProperty(this,
                                            nsGkAtoms::overflowProperty);
}

// Destructor function for the overflow frame property
static void
DestroyOverflowFrames(void*           aFrame,
                      nsIAtom*        aPropertyName,
                      void*           aPropertyValue,
                      void*           aDtorData)
{
  if (aPropertyValue) {
    nsFrameList frames((nsIFrame*)aPropertyValue);

    frames.DestroyFrames();
  }
}

nsresult
nsContainerFrame::SetOverflowFrames(nsPresContext* aPresContext,
                                    nsIFrame*       aOverflowFrames)
{
  nsresult rv =
    aPresContext->PropertyTable()->SetProperty(this,
                                               nsGkAtoms::overflowProperty,
                                               aOverflowFrames,
                                               DestroyOverflowFrames,
                                               nsnull);

  // Verify that we didn't overwrite an existing overflow list
  NS_ASSERTION(rv != NS_PROPTABLE_PROP_OVERWRITTEN, "existing overflow list");

  return rv;
}

/**
 * Push aFromChild and its next siblings to the next-in-flow. Change the
 * geometric parent of each frame that's pushed. If there is no next-in-flow
 * the frames are placed on the overflow list (and the geometric parent is
 * left unchanged).
 *
 * Updates the next-in-flow's child count. Does <b>not</b> update the
 * pusher's child count.
 *
 * @param   aFromChild the first child frame to push. It is disconnected from
 *            aPrevSibling
 * @param   aPrevSibling aFromChild's previous sibling. Must not be null. It's
 *            an error to push a parent's first child frame
 */
void
nsContainerFrame::PushChildren(nsPresContext* aPresContext,
                               nsIFrame*       aFromChild,
                               nsIFrame*       aPrevSibling)
{
  NS_PRECONDITION(nsnull != aFromChild, "null pointer");
  NS_PRECONDITION(nsnull != aPrevSibling, "pushing first child");
  NS_PRECONDITION(aPrevSibling->GetNextSibling() == aFromChild, "bad prev sibling");

  // Disconnect aFromChild from its previous sibling
  aPrevSibling->SetNextSibling(nsnull);

  if (nsnull != GetNextInFlow()) {
    // XXX This is not a very good thing to do. If it gets removed
    // then remove the copy of this routine that doesn't do this from
    // nsInlineFrame.
    nsContainerFrame* nextInFlow = (nsContainerFrame*)GetNextInFlow();
    // When pushing and pulling frames we need to check for whether any
    // views need to be reparented.
    for (nsIFrame* f = aFromChild; f; f = f->GetNextSibling()) {
      nsHTMLContainerFrame::ReparentFrameView(aPresContext, f, this, nextInFlow);
    }
    nextInFlow->mFrames.InsertFrames(nextInFlow, nsnull, aFromChild);
  }
  else {
    // Add the frames to our overflow list
    SetOverflowFrames(aPresContext, aFromChild);
  }
}

/**
 * Moves any frames on the overflow lists (the prev-in-flow's overflow list and
 * the receiver's overflow list) to the child list.
 *
 * Updates this frame's child count and content mapping.
 *
 * @return  PR_TRUE if any frames were moved and PR_FALSE otherwise
 */
PRBool
nsContainerFrame::MoveOverflowToChildList(nsPresContext* aPresContext)
{
  PRBool result = PR_FALSE;

  // Check for an overflow list with our prev-in-flow
  nsContainerFrame* prevInFlow = (nsContainerFrame*)GetPrevInFlow();
  if (nsnull != prevInFlow) {
    nsIFrame* prevOverflowFrames = prevInFlow->GetOverflowFrames(aPresContext,
                                                                 PR_TRUE);
    if (prevOverflowFrames) {
      NS_ASSERTION(mFrames.IsEmpty(), "bad overflow list");
      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented.
      for (nsIFrame* f = prevOverflowFrames; f; f = f->GetNextSibling()) {
        nsHTMLContainerFrame::ReparentFrameView(aPresContext, f, prevInFlow, this);
      }
      mFrames.InsertFrames(this, nsnull, prevOverflowFrames);
      result = PR_TRUE;
    }
  }

  // It's also possible that we have an overflow list for ourselves
  nsIFrame* overflowFrames = GetOverflowFrames(aPresContext, PR_TRUE);
  if (overflowFrames) {
    NS_ASSERTION(mFrames.NotEmpty(), "overflow list w/o frames");
    mFrames.AppendFrames(nsnull, overflowFrames);
    result = PR_TRUE;
  }
  return result;
}

/////////////////////////////////////////////////////////////////////////////
// Debugging

#ifdef NS_DEBUG
NS_IMETHODIMP
nsContainerFrame::List(FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
#ifdef DEBUG_waterson
  fprintf(out, " [parent=%p]", NS_STATIC_CAST(void*, mParent));
#endif
  if (HasView()) {
    fprintf(out, " [view=%p]", NS_STATIC_CAST(void*, GetView()));
  }
  if (nsnull != mNextSibling) {
    fprintf(out, " next=%p", NS_STATIC_CAST(void*, mNextSibling));
  }
  if (nsnull != GetPrevContinuation()) {
    fprintf(out, " prev-continuation=%p", NS_STATIC_CAST(void*, GetPrevContinuation()));
  }
  if (nsnull != GetNextContinuation()) {
    fprintf(out, " next-continuation=%p", NS_STATIC_CAST(void*, GetNextContinuation()));
  }
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
  fprintf(out, " [content=%p]", NS_STATIC_CAST(void*, mContent));
  nsContainerFrame* f = NS_CONST_CAST(nsContainerFrame*, this);
  nsRect* overflowArea = f->GetOverflowAreaProperty(PR_FALSE);
  if (overflowArea) {
    fprintf(out, " [overflow=%d,%d,%d,%d]", overflowArea->x, overflowArea->y,
            overflowArea->width, overflowArea->height);
  }
  fprintf(out, " [sc=%p]", NS_STATIC_CAST(void*, mStyleContext));
  nsIAtom* pseudoTag = mStyleContext->GetPseudoType();
  if (pseudoTag) {
    nsAutoString atomString;
    pseudoTag->ToString(atomString);
    fprintf(out, " pst=%s",
            NS_LossyConvertUTF16toASCII(atomString).get());
  }

  // Output the children
  nsIAtom* listName = nsnull;
  PRInt32 listIndex = 0;
  PRBool outputOneList = PR_FALSE;
  do {
    nsIFrame* kid = GetFirstChild(listName);
    if (nsnull != kid) {
      if (outputOneList) {
        IndentBy(out, aIndent);
      }
      outputOneList = PR_TRUE;
      nsAutoString tmp;
      if (nsnull != listName) {
        listName->ToString(tmp);
        fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);
      }
      fputs("<\n", out);
      while (nsnull != kid) {
        // Verify the child frame's parent frame pointer is correct
        NS_ASSERTION(kid->GetParent() == (nsIFrame*)this, "bad parent frame pointer");

        // Have the child frame list
        nsIFrameDebug*  frameDebug;
        if (NS_SUCCEEDED(kid->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
          frameDebug->List(out, aIndent + 1);
        }
        kid = kid->GetNextSibling();
      }
      IndentBy(out, aIndent);
      fputs(">\n", out);
    }
    listName = GetAdditionalChildListName(listIndex++);
  } while(nsnull != listName);

  if (!outputOneList) {
    fputs("<>\n", out);
  }

  return NS_OK;
}
#endif
