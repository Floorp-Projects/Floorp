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
#include "nsTableOuterFrame.h"
#include "nsTableFrame.h"
#include "nsHTMLReflowCommand.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIRenderingContext.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "prinrval.h"
#include "nsLayoutAtoms.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsDisplayList.h"

/* ----------- nsTableCaptionFrame ---------- */

#define NS_TABLE_FRAME_CAPTION_LIST_INDEX 0
#define NO_SIDE 100

// caption frame
nsTableCaptionFrame::nsTableCaptionFrame(nsStyleContext* aContext):
  nsBlockFrame(aContext)
{
  // shrink wrap 
  SetFlags(NS_BLOCK_SPACE_MGR);
}

nsTableCaptionFrame::~nsTableCaptionFrame()
{
}

void
nsTableOuterFrame::Destroy()
{
  mCaptionFrames.DestroyFrames();
  nsHTMLContainerFrame::Destroy();
}

nsIAtom*
nsTableCaptionFrame::GetType() const
{
  return nsLayoutAtoms::tableCaptionFrame;
}

nsIFrame* 
NS_NewTableCaptionFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTableCaptionFrame(aContext);
}

/* ----------- nsTableOuterFrame ---------- */

NS_IMPL_ADDREF_INHERITED(nsTableOuterFrame, nsHTMLContainerFrame)
NS_IMPL_RELEASE_INHERITED(nsTableOuterFrame, nsHTMLContainerFrame)

nsTableOuterFrame::nsTableOuterFrame(nsStyleContext* aContext):
  nsHTMLContainerFrame(aContext)
{
  mPriorAvailWidth = 0;
#ifdef DEBUG_TABLE_REFLOW_TIMING
  mTimer = new nsReflowTimer(this);
#endif
}

nsTableOuterFrame::~nsTableOuterFrame()
{
#ifdef DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflowDone(this);
#endif
}

nsresult nsTableOuterFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(NS_GET_IID(nsITableLayout))) 
  { // note there is no addref here, frames are not addref'd
    *aInstancePtr = (void*)(nsITableLayout*)this;
    return NS_OK;
  }

  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsTableOuterFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    return accService->CreateHTMLTableAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

/* virtual */ PRBool
nsTableOuterFrame::IsContainingBlock() const
{
  return PR_FALSE;
}

NS_IMETHODIMP
nsTableOuterFrame::Init(
                   nsIContent*           aContent,
                   nsIFrame*             aParent,
                   nsIFrame*             aPrevInFlow)
{
  nsresult rv = nsHTMLContainerFrame::Init(aContent, aParent, aPrevInFlow);
  
  // record that children that are ignorable whitespace should be excluded 
  mState |= NS_FRAME_EXCLUDE_IGNORABLE_WHITESPACE;

  return rv;
}

nsIFrame*
nsTableOuterFrame::GetFirstChild(nsIAtom* aListName) const
{
  if (nsLayoutAtoms::captionList == aListName) {
    return mCaptionFrames.FirstChild();
  }
  if (!aListName) {
    return mFrames.FirstChild();
  }
  return nsnull;
}

nsIAtom*
nsTableOuterFrame::GetAdditionalChildListName(PRInt32 aIndex) const
{
  if (aIndex == NS_TABLE_FRAME_CAPTION_LIST_INDEX) {
    return nsLayoutAtoms::captionList;
  }
  return nsnull;
}

NS_IMETHODIMP 
nsTableOuterFrame::SetInitialChildList(nsIAtom*        aListName,
                                       nsIFrame*       aChildList)
{
  if (nsLayoutAtoms::captionList == aListName) {
    // the frame constructor already checked for table-caption display type
    mCaptionFrames.SetFrames(aChildList);
    mCaptionFrame  = mCaptionFrames.FirstChild();
  }
  else {
    NS_ASSERTION(!aListName, "wrong childlist");
    NS_ASSERTION(mFrames.IsEmpty(), "Frame leak!");
    mFrames.SetFrames(aChildList);
    mInnerTableFrame = nsnull;
    if (aChildList) {
      if (nsLayoutAtoms::tableFrame == aChildList->GetType()) {
        mInnerTableFrame = (nsTableFrame*)aChildList;
      }
      else {
        NS_ERROR("expected a table frame");
        return NS_ERROR_INVALID_ARG;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTableOuterFrame::AppendFrames(nsIAtom*        aListName,
                                nsIFrame*       aFrameList)
{
  nsresult rv;

  // We only have two child frames: the inner table and a caption frame.
  // The inner frame is provided when we're initialized, and it cannot change
  if (nsLayoutAtoms::captionList == aListName) {
    NS_ASSERTION(!aFrameList ||
                 aFrameList->GetType() == nsLayoutAtoms::tableCaptionFrame,
                 "appending non-caption frame to captionList");
    mCaptionFrames.AppendFrames(this, aFrameList);
    mCaptionFrame = mCaptionFrames.FirstChild();

    // Reflow the new caption frame. It's already marked dirty, so generate a reflow
    // command that tells us to reflow our dirty child frames
    rv = GetPresContext()->
        PresShell()->AppendReflowCommand(this, eReflowType_ReflowDirty,
                                           nsnull);
    
  }
  else {
    NS_PRECONDITION(PR_FALSE, "unexpected child list");
    rv = NS_ERROR_UNEXPECTED;
  }

  return rv;
}

NS_IMETHODIMP
nsTableOuterFrame::InsertFrames(nsIAtom*        aListName,
                                nsIFrame*       aPrevFrame,
                                nsIFrame*       aFrameList)
{
  if (nsLayoutAtoms::captionList == aListName) {
    NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
                 "inserting after sibling frame with different parent");
    NS_ASSERTION(!aFrameList ||
                 aFrameList->GetType() == nsLayoutAtoms::tableCaptionFrame,
                 "inserting non-caption frame into captionList");
    mCaptionFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);
    mCaptionFrame = mCaptionFrames.FirstChild();
    return NS_OK;
  }
  else {
    NS_PRECONDITION(!aPrevFrame, "invalid previous frame");
    return AppendFrames(aListName, aFrameList);
  }
}

NS_IMETHODIMP
nsTableOuterFrame::RemoveFrame(nsIAtom*        aListName,
                               nsIFrame*       aOldFrame)
{
  // We only have two child frames: the inner table and one caption frame.
  // The inner frame can't be removed so this should be the caption
  NS_PRECONDITION(nsLayoutAtoms::captionList == aListName, "can't remove inner frame");

  PRUint8 captionSide = GetCaptionSide();

  // See if the (top/bottom) caption's minimum width impacted the inner table or there
  // is a left/right caption (that likely impacts the inner table)
  if ((mMinCaptionWidth == mRect.width) || 
      (NS_SIDE_LEFT == captionSide) || (NS_SIDE_RIGHT == captionSide)) {
    // The old caption width had an effect on the inner table width so
    // we're going to need to reflow it. Mark it dirty
    mInnerTableFrame->AddStateBits(NS_FRAME_IS_DIRTY);
  }

  // Remove the frame and destroy it
  mCaptionFrames.DestroyFrame(aOldFrame);
  mCaptionFrame = mCaptionFrames.FirstChild();
  
  mMinCaptionWidth = 0;

  // Generate a reflow command so we get reflowed
  GetPresContext()->PresShell()->AppendReflowCommand(this,
                                                     eReflowType_ReflowDirty,
                                                     nsnull);

  return NS_OK;
}

NS_METHOD 
nsTableOuterFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                    const nsRect&           aDirtyRect,
                                    const nsDisplayListSet& aLists)
{
  // No border, background or outline are painted because they all belong
  // to the inner table.
  if (!IsVisibleInSelection(aBuilder))
    return NS_OK;

  // If there's no caption, take a short cut to avoid having to create
  // the special display list set and then sort it.
  if (!mCaptionFrame)
    return BuildDisplayListForInnerTable(aBuilder, aDirtyRect, aLists);
    
  nsDisplayListCollection set;
  nsresult rv = BuildDisplayListForInnerTable(aBuilder, aDirtyRect, set);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsDisplayListSet captionSet(set, set.BlockBorderBackgrounds());
  rv = BuildDisplayListForChild(aBuilder, mCaptionFrame, aDirtyRect, captionSet);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Now we have to sort everything by content order, since the caption
  // may be somewhere inside the table
  set.SortAllByContentOrder(aBuilder, GetContent());
  set.MoveTo(aLists);
  return NS_OK;
}

nsresult
nsTableOuterFrame::BuildDisplayListForInnerTable(nsDisplayListBuilder*   aBuilder,
                                                 const nsRect&           aDirtyRect,
                                                 const nsDisplayListSet& aLists)
{
  // Just paint the regular children, but the children's background is our
  // true background (there should only be one, the real table)
  nsIFrame* kid = mFrames.FirstChild();
  // The children should be in content order
  while (kid) {
    nsresult rv = BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
    kid = kid->GetNextSibling();
  }
  return NS_OK;
}

NS_IMETHODIMP nsTableOuterFrame::SetSelected(nsPresContext* aPresContext,
                                             nsIDOMRange *aRange,
                                             PRBool aSelected,
                                             nsSpread aSpread)
{
  nsresult result = nsFrame::SetSelected(aPresContext, aRange,aSelected, aSpread);
  if (NS_SUCCEEDED(result) && mInnerTableFrame)
    return mInnerTableFrame->SetSelected(aPresContext, aRange,aSelected, aSpread);
  return result;
}

NS_IMETHODIMP 
nsTableOuterFrame::GetParentStyleContextFrame(nsPresContext* aPresContext,
                                              nsIFrame**      aProviderFrame,
                                              PRBool*         aIsChild)
{
  // The table outer frame and the (inner) table frame split the style
  // data by giving the table frame the style context associated with
  // the table content node and creating a style context for the outer
  // frame that is a *child* of the table frame's style context,
  // matching the ::-moz-table-outer pseudo-element.  html.css has a
  // rule that causes that pseudo-element (and thus the outer table)
  // to inherit *some* style properties from the table frame.  The
  // children of the table inherit directly from the inner table, and
  // the outer table's style context is a leaf.

  if (!mInnerTableFrame) {
    *aProviderFrame = this;
    *aIsChild = PR_FALSE;
    return NS_ERROR_FAILURE;
  }
  *aProviderFrame = mInnerTableFrame;
  *aIsChild = PR_TRUE;
  return NS_OK;
}

// INCREMENTAL REFLOW HELPER FUNCTIONS 

void
nsTableOuterFrame::ZeroAutoMargin(nsHTMLReflowState& aReflowState,
                                  nsMargin&          aMargin)
{
  if (eStyleUnit_Auto == aReflowState.mStyleMargin->mMargin.GetLeftUnit()) {
    aMargin.left = 0;
  }
  if (eStyleUnit_Auto == aReflowState.mStyleMargin->mMargin.GetRightUnit()) {
    aMargin.right = 0;
  }
}

static void 
FixAutoMargins(nscoord            aAvailWidth,
               nscoord            aChildWidth,
               nsHTMLReflowState& aReflowState)
{
  // see if there are auto margins. they may have been set to 0 in mComputedMargin
  PRBool hasAutoMargin = eStyleUnit_Auto == aReflowState.mStyleMargin->mMargin.GetLeftUnit() ||
                         eStyleUnit_Auto == aReflowState.mStyleMargin->mMargin.GetRightUnit();
  if (hasAutoMargin) {
    nscoord width = aChildWidth;
    if (NS_UNCONSTRAINEDSIZE == width) {
      width = 0;
    }
    aReflowState.CalculateBlockSideMargins(aAvailWidth, width);
  }
}

void
nsTableOuterFrame::InitChildReflowState(nsPresContext&    aPresContext,                     
                                        nsHTMLReflowState& aReflowState)
                                    
{
  nsMargin collapseBorder;
  nsMargin collapsePadding(0,0,0,0);
  nsMargin* pCollapseBorder  = nsnull;
  nsMargin* pCollapsePadding = nsnull;
  if ((aReflowState.frame == mInnerTableFrame) && (mInnerTableFrame->IsBorderCollapse())) {
    if (mInnerTableFrame->NeedToCalcBCBorders()) {
      mInnerTableFrame->CalcBCBorders();
    }
    collapseBorder  = mInnerTableFrame->GetBCBorder();
    pCollapseBorder = &collapseBorder;
    pCollapsePadding = &collapsePadding;
  }
  aReflowState.Init(&aPresContext, -1, -1, pCollapseBorder, pCollapsePadding);
}

// get the margin and padding data. nsHTMLReflowState doesn't handle the
// case of auto margins
void
nsTableOuterFrame::GetMarginPadding(nsPresContext*          aPresContext,                     
                                    const nsHTMLReflowState& aOuterRS,
                                    nsIFrame*                aChildFrame,
                                    nscoord                  aAvailWidth, 
                                    nsMargin&                aMargin,
                                    nsMargin&                aMarginNoAuto,
                                    nsMargin&                aPadding)
{
  // construct a reflow state to compute margin and padding. Auto margins
  // will not be computed at this time.

  // create and init the child reflow state
  nsHTMLReflowState childRS(aPresContext, aOuterRS, aChildFrame, 
                            nsSize(aAvailWidth, aOuterRS.availableHeight), 
                            eReflowReason_Resize, PR_FALSE);
  InitChildReflowState(*aPresContext, childRS);

  FixAutoMargins(aAvailWidth, aChildFrame->GetSize().width, childRS); 
  aMargin = childRS.mComputedMargin;
  aMarginNoAuto = aMargin;
  nsTableOuterFrame::ZeroAutoMargin(childRS, aMarginNoAuto);

  aPadding = childRS.mComputedPadding;
}

static
nscoord CalcAutoMargin(nscoord aAutoMargin,
                       nscoord aOppositeMargin,
                       nscoord aContainBlockSize,
                       nscoord aFrameSize,
                       float   aPixelToTwips)
{
  nscoord margin;
  if (NS_AUTOMARGIN == aOppositeMargin) 
    margin = nsTableFrame::RoundToPixel((aContainBlockSize - aFrameSize) / 2, aPixelToTwips);
  else {
    margin = aContainBlockSize - aFrameSize - aOppositeMargin;
  }
  return PR_MAX(0, margin);
}

static void
MoveFrameTo(nsIFrame* aFrame, 
            nscoord   aX,
            nscoord   aY)
{
  nsPoint pt = aFrame->GetPosition();
  if ((pt.x != aX) || (pt.y != aY)) {
    aFrame->SetPosition(nsPoint(aX, aY));
    nsTableFrame::RePositionViews(aFrame);
  }
}

static nsSize
GetContainingBlockSize(const nsHTMLReflowState& aOuterRS)
{
  nsSize size(0,0);
  const nsHTMLReflowState* containRS =
    aOuterRS.mCBReflowState;

  if (containRS) {
    size.width = containRS->mComputedWidth;
    if (NS_UNCONSTRAINEDSIZE == size.width) {
      size.width = 0;
    }
    size.height = containRS->mComputedHeight;
    if (NS_UNCONSTRAINEDSIZE == size.height) {
      size.height = 0;
    }
  }
  return size;
}

void
nsTableOuterFrame::InvalidateDamage(PRUint8         aCaptionSide,
                                    const nsSize&   aOuterSize,
                                    PRBool          aInnerChanged,
                                    PRBool          aCaptionChanged,
                                    nsRect*         aOldOverflowArea)
{
  if (!aInnerChanged && !aCaptionChanged) return;

  nsRect damage;
  if (aInnerChanged && aCaptionChanged) {
    damage = nsRect(0, 0, aOuterSize.width, aOuterSize.height);
    if (aOldOverflowArea) {
      damage.UnionRect(damage, *aOldOverflowArea);
    }
    nsRect* overflowArea = GetOverflowAreaProperty();
    if (overflowArea) {
      damage.UnionRect(damage, *overflowArea);
    }
  }
  else {
    nsRect captionRect(0,0,0,0);
    nsRect innerRect = mInnerTableFrame->GetRect();
    if (mCaptionFrame) {
      captionRect = mCaptionFrame->GetRect();
    }
    
    damage.x = 0;
    damage.width  = aOuterSize.width;
    switch(aCaptionSide) {
    case NS_SIDE_BOTTOM:
      if (aCaptionChanged) {
        damage.y = innerRect.y;
        damage.height = aOuterSize.height - damage.y;
      }
      else { // aInnerChanged 
        damage.y = 0;
        damage.height = captionRect.y;
      }
      break;
    case NS_SIDE_LEFT:
      if (aCaptionChanged) {
        damage.width = innerRect.x;
        damage.y = 0;
        damage.height = captionRect.YMost();
      }
      else { // aInnerChanged
        damage.x = captionRect.XMost();
        damage.width = innerRect.XMost() - damage.x;
        damage.y = 0;
        damage.height = innerRect.YMost();
      }
      break;
    case NS_SIDE_RIGHT:
     if (aCaptionChanged) {
        damage.x = innerRect.XMost();
        damage.width -= damage.x;
        damage.y = 0;
        damage.height = captionRect.YMost();
      }
     else { // aInnerChanged
        damage.width -= captionRect.width;
        damage.y = 0;
        damage.height = innerRect.YMost();
      }
      break;
    default: // NS_SIDE_TOP
      if (aCaptionChanged) {
        damage.y = 0;
        damage.height = innerRect.y;
      }
      else { // aInnerChanged
        damage.y = captionRect.y;
        damage.height = aOuterSize.height - damage.y;
      }
      break;
    }
     
    nsIFrame* kidFrame = aCaptionChanged ? mCaptionFrame : mInnerTableFrame;
    ConsiderChildOverflow(damage, kidFrame);
    if (aOldOverflowArea) {
      damage.UnionRect(damage, *aOldOverflowArea);
    }
  }
  Invalidate(damage);
}

nscoord
nsTableOuterFrame::GetCaptionAvailWidth(nsPresContext*          aPresContext,
                                        nsIFrame*                aCaptionFrame,
                                        const nsHTMLReflowState& aOuterRS,
                                        nsMargin&                aCaptionMargin,
                                        nsMargin&                aCaptionPad,
                                        nscoord*                 aInnerWidth,
                                        const nsMargin*          aInnerMarginNoAuto,
                                        const nsMargin*          aInnerMargin)
{
  nscoord availWidth;
  if (aInnerWidth) {
    nscoord innerWidth = *aInnerWidth;
    if (NS_UNCONSTRAINEDSIZE == innerWidth) {
      availWidth = innerWidth;
    }
    else {
      nsMargin innerMarginNoAuto(0,0,0,0);
      if (aInnerMarginNoAuto) {
        innerMarginNoAuto = *aInnerMarginNoAuto;
      }
      nsMargin innerMargin(0,0,0,0);
      if (aInnerMargin) {
        innerMargin = *aInnerMargin;
      }
      PRUint8 captionSide = GetCaptionSide();
      switch (captionSide) {
        case NS_SIDE_LEFT: 
        case NS_SIDE_RIGHT:
          availWidth = (NS_SIDE_LEFT == captionSide) ? innerMargin.left : innerMargin.right;
          break;
        default: // NS_SIDE_TOP, NS_SIDE_BOTTOM
          availWidth = innerWidth + innerMarginNoAuto.left + innerMarginNoAuto.right;
          break;
      }
    }
  }
  else {
    availWidth = GetSize().width;
  }

  if (NS_UNCONSTRAINEDSIZE == availWidth) {
    return availWidth;
  }
  else {
    nsMargin marginIgnore;
    GetMarginPadding(aPresContext, aOuterRS, aCaptionFrame, availWidth, 
                     marginIgnore, aCaptionMargin, aCaptionPad);
    availWidth -= aCaptionMargin.left + aCaptionMargin.right;
    return (PR_MAX(availWidth, mMinCaptionWidth));
  }
}

nscoord
nsTableOuterFrame::GetInnerTableAvailWidth(nsPresContext*          aPresContext,
                                           nsIFrame*                aInnerTable,
                                           const nsHTMLReflowState& aOuterRS,
                                           nscoord*                 aCaptionWidth,
                                           nsMargin&                aInnerMargin,
                                           nsMargin&                aInnerPadding)
{
  nscoord availWidth;
  nscoord captionWidth = 0;
  if (aCaptionWidth) {
    captionWidth = *aCaptionWidth;
    if (NS_UNCONSTRAINEDSIZE == captionWidth) {
      availWidth = captionWidth;
    }
    else {
      availWidth = aOuterRS.availableWidth;
    }
  }
  else {
    availWidth = GetSize().width;
  }

  if (NS_UNCONSTRAINEDSIZE == availWidth) {
    return availWidth;
  }
  else {
    nsMargin marginIgnore;
    GetMarginPadding(aPresContext, aOuterRS, aInnerTable, availWidth, marginIgnore, aInnerMargin, aInnerPadding);
    availWidth -= aInnerMargin.left + aInnerMargin.right;
    PRUint8 captionSide = GetCaptionSide();
    switch (captionSide) {
      case NS_SIDE_LEFT:
        if (captionWidth > marginIgnore.left) {
          availWidth -= captionWidth - aInnerMargin.left;
        }
        break;
      case NS_SIDE_RIGHT:
        if (captionWidth > marginIgnore.right) {
          availWidth -= captionWidth - aInnerMargin.right;
        }
        break;
      default:
        availWidth = PR_MAX(availWidth, mMinCaptionWidth);
        break;
    }
    return(availWidth);
  }
}

nscoord
nsTableOuterFrame::GetMaxElementWidth(PRUint8         aCaptionSide,
                                      const nsMargin& aInnerMargin,
                                      const nsMargin& aInnerPadding,
                                      const nsMargin& aCaptionMargin)
{
  nscoord width = aInnerMargin.left + 
                  ((nsTableFrame *)mInnerTableFrame)->GetMinWidth() + 
                  aInnerMargin.right;
  if (mCaptionFrame) { 
    nscoord capWidth = mMinCaptionWidth + aCaptionMargin.left + aCaptionMargin.right;
    switch(aCaptionSide) {
    case NS_SIDE_LEFT:
      if (capWidth > aInnerMargin.left) {
        width += capWidth - aInnerMargin.left;
      }
      break;
    case NS_SIDE_RIGHT:
      if (capWidth > aInnerMargin.right) {
        width += capWidth - aInnerMargin.right;
      }
      break;
    default:
      if (capWidth > width) {
        width = capWidth;
      }
    }
  }
  return width;
}

nscoord
nsTableOuterFrame::GetMaxWidth(PRUint8         aCaptionSide,
                               const nsMargin& aInnerMargin,
                               const nsMargin& aCaptionMargin)
{
  nscoord maxWidth;

  maxWidth = ((nsTableFrame *)mInnerTableFrame)->GetPreferredWidth() + 
               aInnerMargin.left + aInnerMargin.right;
  if (mCaptionFrame) {
    switch(aCaptionSide) {
    case NS_SIDE_LEFT:
    case NS_SIDE_RIGHT:
      maxWidth += mCaptionFrame->GetSize().width + aCaptionMargin.left + aCaptionMargin.right;
      // the caption plus it margins should cover the corresponding inner table side
      // margin - don't count it twice.
      maxWidth -= (NS_SIDE_LEFT == aCaptionSide) ? aInnerMargin.left : aInnerMargin.right;
      break;
    case NS_SIDE_TOP:
    case NS_SIDE_BOTTOM:
    default:  // no caption 
      maxWidth = PR_MAX(maxWidth, mMinCaptionWidth + aCaptionMargin.left + aCaptionMargin.right);
    }
  }
  return maxWidth;
}


PRUint8
nsTableOuterFrame::GetCaptionSide()
{
  if (mCaptionFrame) {
    return mCaptionFrame->GetStyleTableBorder()->mCaptionSide;
  }
  else {
    return NO_SIDE; // no caption
  }
}

PRUint8
nsTableOuterFrame::GetCaptionVerticalAlign()
{
  const nsStyleCoord& va = mCaptionFrame->GetStyleTextReset()->mVerticalAlign;
  return (va.GetUnit() == eStyleUnit_Enumerated)
           ? va.GetIntValue()
           : NS_STYLE_VERTICAL_ALIGN_TOP;
}

void
nsTableOuterFrame::SetDesiredSize(PRUint8         aCaptionSide,
                                  const nsMargin& aInnerMargin,
                                  const nsMargin& aCaptionMargin,
                                  nscoord         aAvailableWidth,
                                  nscoord&        aWidth,
                                  nscoord&        aHeight)
{
  aWidth = aHeight = 0;

  nsRect innerRect = mInnerTableFrame->GetRect();
  nscoord innerWidth = innerRect.width;

  nsRect captionRect(0,0,0,0);
  nscoord captionWidth = 0;
  if (mCaptionFrame) {
    captionRect = mCaptionFrame->GetRect();
    captionWidth = captionRect.width;
    if ((NS_UNCONSTRAINEDSIZE == aAvailableWidth) &&
        ((NS_SIDE_LEFT == aCaptionSide) || (NS_SIDE_RIGHT == aCaptionSide))) {
      BalanceLeftRightCaption(aCaptionSide, aInnerMargin, aCaptionMargin, 
                              innerWidth, captionWidth);
    }
  }
  switch(aCaptionSide) {
    case NS_SIDE_LEFT:
      aWidth = PR_MAX(aInnerMargin.left, aCaptionMargin.left + captionWidth + aCaptionMargin.right) +
               innerWidth + aInnerMargin.right;
      break;
    case NS_SIDE_RIGHT:
      aWidth = PR_MAX(aInnerMargin.right, aCaptionMargin.left + captionWidth + aCaptionMargin.right) +
               innerWidth + aInnerMargin.left;
      break;
    default:
      aWidth = aInnerMargin.left + innerWidth + aInnerMargin.right;
      aWidth = PR_MAX(aWidth, captionRect.XMost() + aCaptionMargin.right);
  }
  aHeight = innerRect.YMost() + aInnerMargin.bottom;
  aHeight = PR_MAX(aHeight, captionRect.YMost() + aCaptionMargin.bottom);

}

void 
nsTableOuterFrame::PctAdjustMinCaptionWidth(nsPresContext*           aPresContext,
                                            const nsHTMLReflowState&  aOuterRS,
                                            PRUint8                   aCaptionSide,
                                            nscoord&                  capMin)
{
  if (NS_SIDE_LEFT == aCaptionSide || NS_SIDE_RIGHT == aCaptionSide) {
    if (mCaptionFrame) {
      nsMargin capMarginIgnore;
      nsMargin capMarginNoAuto;
      nsMargin captionPaddingIgnore;
      GetMarginPadding(aPresContext, aOuterRS, mCaptionFrame, aOuterRS.availableWidth, capMarginIgnore, 
                       capMarginNoAuto, captionPaddingIgnore);
      PRBool isPctWidth;
      IsAutoWidth(*mCaptionFrame,&isPctWidth);
      if (isPctWidth) {
        capMin = mCaptionFrame->GetSize().width;
      }
      capMin += capMarginNoAuto.left + capMarginNoAuto.right;
    }
  }
}
void
nsTableOuterFrame::BalanceLeftRightCaption(PRUint8         aCaptionSide,
                                           const nsMargin& aInnerMargin,
                                           const nsMargin& aCaptionMargin,
                                           nscoord&        aInnerWidth, 
                                           nscoord&        aCaptionWidth)
{
  
  /* balance the caption and inner table widths to ensure space for percent widths
  *  Percent widths for captions or the inner table frame can determine how much of the
  *  available width is used and how the available width is distributed between those frames
  *  The inner table frame has already a quite sophisticated treatment of percentage widths 
  *  (see BasicTableLayoutStrategy.cpp). So it acts as master in the below computations.
  *  There are four possible scenarios 
  *  a) None of the frames have a percentage width - then the aInnerWidth and aCaptionwidth will not change
  *  b) Only the inner frame has a percentage width - this is handled in BasicTableLayoutStrategy.cpp, 
  *     both widths will not change
  *  c) Only the caption has a percentage width - then the overall width (ow) will be different depending on
  *     the caption side. For the left side
  *     ow = aCaptionMargin.left + aCaptionWidth + aCaptionMargin.right + aInnerwidth + aInnerMargin.right
  *     aCaptionWidth = capPercent * ow
  *     solving this equation for aCaptionWidth gives:
  *     aCaptionWidth = capPercent/(1-capPercent) * 
  *                      (aCaptionMargin.left + aCaptionMargin.right + aInnerwidth + aInnerMargin.right)
  *     this result will cause problems for capPercent >= 1, in these cases the algorithm will now bail out
  *     a similar expression can be found for the right case
  *  d) both frames have percent widths in this case the caption width will be the inner width multiplied 
  *     by the weight capPercent/innerPercent
  */
    

  float capPercent   = -1.0;
  float innerPercent = -1.0;
  const nsStylePosition* position = mCaptionFrame->GetStylePosition();
  if (eStyleUnit_Percent == position->mWidth.GetUnit()) {
    capPercent = position->mWidth.GetPercentValue();
    if (capPercent >= 1.0)
      return;
  }

  position = mInnerTableFrame->GetStylePosition();
  if (eStyleUnit_Percent == position->mWidth.GetUnit()) {
    innerPercent = position->mWidth.GetPercentValue();
    if (innerPercent >= 1.0)
      return;
  }

  if ((capPercent <= 0.0) && (innerPercent <= 0.0))
    return;

  
  if (innerPercent <= 0.0) {
    if (NS_SIDE_LEFT == aCaptionSide) 
      aCaptionWidth= (nscoord) ((capPercent / (1.0 - capPercent)) * (aCaptionMargin.left + aCaptionMargin.right + 
                                                          aInnerWidth + aInnerMargin.right));
    else
      aCaptionWidth= (nscoord) ((capPercent / (1.0 - capPercent)) * (aCaptionMargin.left + aCaptionMargin.right + 
                                                          aInnerWidth + aInnerMargin.left)); 
  } 
  else {
    aCaptionWidth = (nscoord) ((capPercent / innerPercent) * aInnerWidth);
  }
  aCaptionWidth = nsTableFrame::RoundToPixel(aCaptionWidth,
                                             GetPresContext()->ScaledPixelsToTwips(),
                                             eAlwaysRoundDown);
}

nsresult 
nsTableOuterFrame::GetCaptionOrigin(PRUint32         aCaptionSide,
                                    const nsSize&    aContainBlockSize,
                                    const nsSize&    aInnerSize, 
                                    const nsMargin&  aInnerMargin,
                                    const nsSize&    aCaptionSize,
                                    nsMargin&        aCaptionMargin,
                                    nsPoint&         aOrigin)
{
  aOrigin.x = aOrigin.y = 0;
  if ((NS_UNCONSTRAINEDSIZE == aInnerSize.width) || (NS_UNCONSTRAINEDSIZE == aInnerSize.height) ||  
      (NS_UNCONSTRAINEDSIZE == aCaptionSize.width) || (NS_UNCONSTRAINEDSIZE == aCaptionSize.height)) {
    return NS_OK;
  }
  if (!mCaptionFrame) return NS_OK;

  GET_PIXELS_TO_TWIPS(GetPresContext(), p2t);

  switch(aCaptionSide) {
  case NS_SIDE_BOTTOM: {
    if (NS_AUTOMARGIN == aCaptionMargin.left) {
      aCaptionMargin.left = CalcAutoMargin(aCaptionMargin.left, aCaptionMargin.right,
                                           aContainBlockSize.width, aCaptionSize.width, p2t);
    }
    aOrigin.x = aCaptionMargin.left;
    if (NS_AUTOMARGIN == aCaptionMargin.top) {
      aCaptionMargin.top = 0;
    }
    nsCollapsingMargin marg;
    marg.Include(aCaptionMargin.top);
    marg.Include(aInnerMargin.bottom);
    nscoord collapseMargin = marg.get();
    if (NS_AUTOMARGIN == aCaptionMargin.bottom) {
      nscoord height = aInnerSize.height + collapseMargin + aCaptionSize.height;
      aCaptionMargin.bottom = CalcAutoMargin(aCaptionMargin.bottom, aInnerMargin.top,
                                             aContainBlockSize.height, height, p2t);
    }
    aOrigin.y = aInnerMargin.top + aInnerSize.height + collapseMargin;
  } break;
  case NS_SIDE_LEFT: {
    if (NS_AUTOMARGIN == aCaptionMargin.left) {
      if (NS_AUTOMARGIN != aInnerMargin.left) {
        aCaptionMargin.left = CalcAutoMargin(aCaptionMargin.left, aCaptionMargin.right,
                                             aInnerMargin.left, aCaptionSize.width, p2t);
      } 
      else {
        // zero for now
        aCaptionMargin.left = 0;
      } 
    }
    aOrigin.x = aCaptionMargin.left;
    aOrigin.y = aInnerMargin.top;
    switch(GetCaptionVerticalAlign()) {
      case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
        aOrigin.y = PR_MAX(0, aInnerMargin.top + ((aInnerSize.height - aCaptionSize.height) / 2));
        break;
      case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
        aOrigin.y = PR_MAX(0, aInnerMargin.top + aInnerSize.height - aCaptionSize.height);
        break;
      default:
        break;
    }
  } break;
  case NS_SIDE_RIGHT: {
    if (NS_AUTOMARGIN == aCaptionMargin.left) {
      if (NS_AUTOMARGIN != aInnerMargin.right) {
        aCaptionMargin.left = CalcAutoMargin(aCaptionMargin.left, aCaptionMargin.right,
                                             aInnerMargin.right, aCaptionSize.width, p2t);
      }
      else {
       // zero for now
       aCaptionMargin.left = 0;
      } 
    }
    aOrigin.x = aInnerMargin.left + aInnerSize.width + aCaptionMargin.left;
    aOrigin.y = aInnerMargin.top;
    switch(GetCaptionVerticalAlign()) {
      case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
        aOrigin.y += PR_MAX(0, (aInnerSize.height - aCaptionSize.height) / 2);
        break;
      case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
        aOrigin.y += PR_MAX(0, aInnerSize.height - aCaptionSize.height);
        break;
      default:
        break;
    }
  } break;
  default: { // top
    if (NS_AUTOMARGIN == aCaptionMargin.left) {
      aCaptionMargin.left = CalcAutoMargin(aCaptionMargin.left, aCaptionMargin.right,
                                           aContainBlockSize.width, aCaptionSize.width, p2t);
    }
    aOrigin.x = aCaptionMargin.left;
    if (NS_AUTOMARGIN == aCaptionMargin.bottom) {
      aCaptionMargin.bottom = 0;
    }
    if (NS_AUTOMARGIN == aCaptionMargin.top) {
      nsCollapsingMargin marg;
      marg.Include(aCaptionMargin.bottom);
      marg.Include(aInnerMargin.top);
      nscoord collapseMargin = marg.get();
      nscoord height = aCaptionSize.height + collapseMargin + aInnerSize.height;
      aCaptionMargin.top = CalcAutoMargin(aCaptionMargin.top, aInnerMargin.bottom,
                                          aContainBlockSize.height, height, p2t);
    }
    aOrigin.y = aCaptionMargin.top;
  } break;
  }
  return NS_OK;
}

nsresult 
nsTableOuterFrame::GetInnerOrigin(PRUint32         aCaptionSide,
                                  const nsSize&    aContainBlockSize,
                                  const nsSize&    aCaptionSize, 
                                  const nsMargin&  aCaptionMargin,
                                  const nsSize&    aInnerSize,
                                  nsMargin&        aInnerMargin,
                                  nsPoint&         aOrigin)
{
  aOrigin.x = aOrigin.y = 0;
  if ((NS_UNCONSTRAINEDSIZE == aInnerSize.width) || (NS_UNCONSTRAINEDSIZE == aInnerSize.height) ||  
      (NS_UNCONSTRAINEDSIZE == aCaptionSize.width) || (NS_UNCONSTRAINEDSIZE == aCaptionSize.height)) {
    return NS_OK;
  }

  GET_PIXELS_TO_TWIPS(GetPresContext(), p2t);

  nscoord minCapWidth = aCaptionSize.width;
  if (NS_AUTOMARGIN != aCaptionMargin.left)
    minCapWidth += aCaptionMargin.left;
  if (NS_AUTOMARGIN != aCaptionMargin.right)
    minCapWidth += aCaptionMargin.right;

  switch(aCaptionSide) {
  case NS_SIDE_BOTTOM: {
    if (NS_AUTOMARGIN == aInnerMargin.left) {
      aInnerMargin.left = CalcAutoMargin(aInnerMargin.left, aInnerMargin.right,
                                         aContainBlockSize.width, aInnerSize.width, p2t);
    }
    aOrigin.x = aInnerMargin.left;
    if (NS_AUTOMARGIN == aInnerMargin.bottom) {
      aInnerMargin.bottom = 0;
    }
    if (NS_AUTOMARGIN == aInnerMargin.top) {
      nsCollapsingMargin marg;
      marg.Include(aInnerMargin.bottom);
      marg.Include(aCaptionMargin.top);
      nscoord collapseMargin = marg.get();
      nscoord height = aInnerSize.height + collapseMargin + aCaptionSize.height;
      aInnerMargin.top = CalcAutoMargin(aInnerMargin.top, aCaptionMargin.bottom,
                                        aContainBlockSize.height, height, p2t);
    }
    aOrigin.y = aInnerMargin.top;
  } break;
  case NS_SIDE_LEFT: {
    
    if (NS_AUTOMARGIN == aInnerMargin.left) {
      aInnerMargin.left = CalcAutoMargin(aInnerMargin.left, aInnerMargin.right,
                                         aContainBlockSize.width, aInnerSize.width, p2t);
      
    }
    if (aInnerMargin.left < minCapWidth) {
      // shift the inner table to get some place for the caption
      aInnerMargin.right += aInnerMargin.left - minCapWidth;
      aInnerMargin.right  = PR_MAX(0, aInnerMargin.right);
      aInnerMargin.left   = minCapWidth;
    }
    aOrigin.x = aInnerMargin.left;
    if (NS_AUTOMARGIN == aInnerMargin.top) {
      aInnerMargin.top = 0;
    }
    aOrigin.y = aInnerMargin.top;
    switch(GetCaptionVerticalAlign()) {
      case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
        aOrigin.y = PR_MAX(aInnerMargin.top, (aCaptionSize.height - aInnerSize.height) / 2);
        break;
      case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
        aOrigin.y = PR_MAX(aInnerMargin.top, aCaptionSize.height - aInnerSize.height);
        break;
      default:
        break;
    }
  } break;
  case NS_SIDE_RIGHT: {
    if (NS_AUTOMARGIN == aInnerMargin.right) {
      aInnerMargin.right = CalcAutoMargin(aInnerMargin.left, aInnerMargin.right,
                                          aContainBlockSize.width, aInnerSize.width, p2t);
      if (aInnerMargin.right < minCapWidth) {
        // shift the inner table to get some place for the caption
        aInnerMargin.left -= aInnerMargin.right - minCapWidth;
        aInnerMargin.left  = PR_MAX(0, aInnerMargin.left);
        aInnerMargin.right = minCapWidth;
      }
    }
    aOrigin.x = aInnerMargin.left;
    if (NS_AUTOMARGIN == aInnerMargin.top) {
      aInnerMargin.top = 0;
    }
    aOrigin.y = aInnerMargin.top;
    switch(GetCaptionVerticalAlign()) {
      case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
        aOrigin.y = PR_MAX(aInnerMargin.top, (aCaptionSize.height - aInnerSize.height) / 2);
        break;
      case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
        aOrigin.y = PR_MAX(aInnerMargin.top, aCaptionSize.height - aInnerSize.height);
        break;
      default:
        break;
    }
  } break;
  default: { // top
    if (NS_AUTOMARGIN == aInnerMargin.left) {
      aInnerMargin.left = CalcAutoMargin(aInnerMargin.left, aInnerMargin.right,
                                         aContainBlockSize.width, aInnerSize.width, p2t);
    }
    aOrigin.x = aInnerMargin.left;
    if (NS_AUTOMARGIN == aInnerMargin.top) {
      aInnerMargin.top = 0;
    }
    nsCollapsingMargin marg;
    marg.Include(aCaptionMargin.bottom);
    marg.Include(aInnerMargin.top);
    nscoord collapseMargin = marg.get();
    if (NS_AUTOMARGIN == aInnerMargin.bottom) {
      nscoord height = aCaptionSize.height + collapseMargin + aInnerSize.height;
      aInnerMargin.bottom = CalcAutoMargin(aCaptionMargin.bottom, aInnerMargin.top,
                                           aContainBlockSize.height, height, p2t);
    }
    aOrigin.y = aCaptionMargin.top + aCaptionSize.height + collapseMargin;
  } break;
  }
  return NS_OK;
}

// helper method for determining if this is a nested table or not
PRBool 
nsTableOuterFrame::IsNested(const nsHTMLReflowState& aReflowState) const
{
  // Walk up the reflow state chain until we find a cell or the root
  const nsHTMLReflowState* rs = aReflowState.parentReflowState;
  while (rs) {
    if (nsLayoutAtoms::tableFrame == rs->frame->GetType()) {
      return PR_TRUE;
    }
    rs = rs->parentReflowState;
  }
  return PR_FALSE;
}

PRBool 
nsTableOuterFrame::IsAutoWidth(nsIFrame& aTableOrCaption,
                               PRBool*   aIsPctWidth)
{
  PRBool isAuto = PR_TRUE;  // the default
  if (aIsPctWidth) {
    *aIsPctWidth = PR_FALSE;
  }

  const nsStylePosition* position = aTableOrCaption.GetStylePosition();

#ifdef __SUNPRO_CC
  // Bug 239962, this is a workaround to avoid incorrect code generation by Sun Forte compiler.
  float percent = position->mWidth.GetPercentValue();
#endif

  switch (position->mWidth.GetUnit()) {
    case eStyleUnit_Auto:         // specified auto width
    case eStyleUnit_Proportional: // illegal for table, so ignored
      break;
    case eStyleUnit_Coord:
      isAuto = PR_FALSE;
      break;
    case eStyleUnit_Percent:
#ifdef __SUNPRO_CC
      if (percent > 0.0f) {
#else
      if (position->mWidth.GetPercentValue() > 0.0f) {
#endif
        isAuto = PR_FALSE;
        if (aIsPctWidth) {
          *aIsPctWidth = PR_TRUE;
        }
      }
      break;
    default:
      break;
  }

  return isAuto; 
}
// eReflowReason_Resize was being used for incremental cases

nsresult
nsTableOuterFrame::OuterReflowChild(nsPresContext*            aPresContext,
                                    nsIFrame*                  aChildFrame,
                                    const nsHTMLReflowState&   aOuterRS,
                                    nsHTMLReflowMetrics&       aMetrics,
                                    nscoord                    aAvailWidth, 
                                    nsSize&                    aDesiredSize,
                                    nsMargin&                  aMargin,
                                    nsMargin&                  aMarginNoAuto,
                                    nsMargin&                  aPadding,
                                    nsReflowReason             aReflowReason,
                                    nsReflowStatus&            aStatus,
                                    PRBool*                    aNeedToReflowCaption)
{ 
  aMargin = aPadding = nsMargin(0,0,0,0);

  // work around pixel rounding errors, round down to ensure we don't exceed the avail height in
  nscoord availHeight = aOuterRS.availableHeight;
  if (NS_UNCONSTRAINEDSIZE != availHeight) {
    nsMargin margin, marginNoAuto, padding;
    GetMarginPadding(aPresContext, aOuterRS, aChildFrame, aOuterRS.availableWidth, margin, 
                     marginNoAuto, padding);
    
    NS_ASSERTION(NS_UNCONSTRAINEDSIZE != margin.top, "No unconstrainedsize arithmetic, please");
    availHeight -= margin.top;
    
    NS_ASSERTION(NS_UNCONSTRAINEDSIZE != margin.bottom, "No unconstrainedsize arithmetic, please");
    availHeight -= margin.bottom;
    
    availHeight = nsTableFrame::RoundToPixel(availHeight,
                                           aPresContext->ScaledPixelsToTwips(),
                                             eAlwaysRoundDown);
  }
  nsSize availSize(aAvailWidth, availHeight);
  if (mCaptionFrame == aChildFrame) {
    PRUint8 captionSide = GetCaptionSide();
    if ((NS_SIDE_LEFT  == captionSide) || (NS_SIDE_RIGHT != captionSide)) {
      PRBool isPctWidth;
      IsAutoWidth(*aChildFrame, &isPctWidth);
      if (isPctWidth) {
        availSize.width = aOuterRS.availableWidth;
      }
    }
  }
  // create and init the child reflow state
  nsHTMLReflowState childRS(aPresContext, aOuterRS, aChildFrame, availSize, 
                            aReflowReason);
  InitChildReflowState(*aPresContext, childRS);
  childRS.mPercentHeightObserver = nsnull; // the observer is for non table related frames inside cells


  // If mComputedWidth > availWidth and availWidth >= minWidth for a nested percent table 
  // then adjust mComputedWidth based on availableWidth if this isn't the initial reflow   
  if ((NS_UNCONSTRAINEDSIZE != childRS.mComputedWidth)  &&
      (eReflowReason_Initial != aReflowReason)          &&
      (childRS.mComputedWidth + childRS.mComputedBorderPadding.left +
       childRS.mComputedBorderPadding.right > childRS.availableWidth) &&
      IsNested(aOuterRS)) {
    PRBool isPctWidth;
    IsAutoWidth(*aChildFrame, &isPctWidth);
    if (isPctWidth) {
      if ( ((aChildFrame == mInnerTableFrame) && 
            ((nsTableFrame*)aChildFrame)->GetMinWidth() <= childRS.availableWidth) ||
           (aChildFrame != mInnerTableFrame)) {
        childRS.mComputedWidth = childRS.availableWidth - childRS.mComputedBorderPadding.left -
                                                          childRS.mComputedBorderPadding.right;
      }
    }
  }

  // see if we need to reset top of page due to a caption
  if (mCaptionFrame) {
    PRUint8 captionSide = GetCaptionSide();
    if (((NS_SIDE_BOTTOM == captionSide) && (mCaptionFrame == aChildFrame)) || 
        ((NS_SIDE_TOP == captionSide) && (mInnerTableFrame == aChildFrame))) {
      childRS.mFlags.mIsTopOfPage = PR_FALSE;
    }
    if ((mCaptionFrame == aChildFrame) && (NS_SIDE_LEFT  != captionSide) 
                                       && (NS_SIDE_RIGHT != captionSide)) {
      aAvailWidth = aOuterRS.availableWidth;
    }
  }
  // see if we need to reflow the caption in addition
  if (aNeedToReflowCaption && !*aNeedToReflowCaption &&
      mInnerTableFrame == aChildFrame && childRS.reason == eReflowReason_Incremental) {
    nsHTMLReflowCommand* command = childRS.path->mReflowCommand;
    if (command) {
      *aNeedToReflowCaption = eReflowType_StyleChanged == command->Type();
    }
  }

  // use the current position as a best guess for placement
  nsPoint childPt = aChildFrame->GetPosition();
  nsresult rv = ReflowChild(aChildFrame, aPresContext, aMetrics, childRS,
                            childPt.x, childPt.y, NS_FRAME_NO_MOVE_FRAME, aStatus);
  if (NS_FAILED(rv)) return rv;
  
  FixAutoMargins(aAvailWidth, aMetrics.width, childRS);
  aMargin = childRS.mComputedMargin;
  aMarginNoAuto = childRS.mComputedMargin;
  ZeroAutoMargin(childRS, aMarginNoAuto);

  aDesiredSize.width  = aMetrics.width;
  aDesiredSize.height = aMetrics.height;

  return rv;
}

void 
nsTableOuterFrame::UpdateReflowMetrics(PRUint8              aCaptionSide,
                                       nsHTMLReflowMetrics& aMet,
                                       const nsMargin&      aInnerMargin,
                                       const nsMargin&      aInnerMarginNoAuto,
                                       const nsMargin&      aInnerPadding,
                                       const nsMargin&      aCaptionMargin,
                                       const nsMargin&      aCaptionMarginNoAuto,
                                       const nscoord        aAvailableWidth)
{
  SetDesiredSize(aCaptionSide, aInnerMargin, aCaptionMargin, aAvailableWidth, aMet.width, aMet.height);

  // set maxElementSize width if requested
  if (aMet.mComputeMEW) {
    aMet.mMaxElementWidth = GetMaxElementWidth(aCaptionSide, aInnerMarginNoAuto, aInnerPadding, aCaptionMarginNoAuto);
  }
  // set maximum width if requested
  if (aMet.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
    aMet.mMaximumWidth = GetMaxWidth(aCaptionSide, aInnerMarginNoAuto, aCaptionMarginNoAuto);
  }
 
  aMet.mOverflowArea = nsRect(0, 0, aMet.width, aMet.height);
  ConsiderChildOverflow(aMet.mOverflowArea, mInnerTableFrame);
  if (mCaptionFrame) {
    ConsiderChildOverflow(aMet.mOverflowArea, mCaptionFrame);
  }
  FinishAndStoreOverflow(&aMet);
}

nsresult 
nsTableOuterFrame::IncrementalReflow(nsPresContext*          aPresContext,
                                     nsHTMLReflowMetrics&     aDesiredSize,
                                     const nsHTMLReflowState& aReflowState,
                                     nsReflowStatus&          aStatus)
{
  // the outer table is a target if its path has a reflow command
  nsHTMLReflowCommand* command = aReflowState.path->mReflowCommand;
  if (command)
    IR_TargetIsMe(aPresContext, aDesiredSize, aReflowState, aStatus);

  // see if the chidren are targets as well
  nsReflowPath::iterator iter = aReflowState.path->FirstChild();
  nsReflowPath::iterator end  = aReflowState.path->EndChildren();
  for (; iter != end; ++iter)
    IR_TargetIsChild(aPresContext, aDesiredSize, aReflowState, aStatus, *iter);

  return NS_OK;
}

nsresult 
nsTableOuterFrame::IR_TargetIsChild(nsPresContext*          aPresContext,
                                    nsHTMLReflowMetrics&     aDesiredSize,
                                    const nsHTMLReflowState& aReflowState,
                                    nsReflowStatus&          aStatus,
                                    nsIFrame*                aNextFrame)
{
  nsresult rv;
  if (!aNextFrame) {
    // this will force Reflow to return the height of the last reflow rather than 0
    aDesiredSize.height = mRect.height; 
    return NS_OK;
  }

  if (aNextFrame == mInnerTableFrame) {
    rv = IR_TargetIsInnerTableFrame(aPresContext, aDesiredSize, aReflowState, aStatus);
  }
  else if (aNextFrame == mCaptionFrame) {
    rv = IR_TargetIsCaptionFrame(aPresContext, aDesiredSize, aReflowState, aStatus);
  }
  else {
    const nsStyleDisplay* nextDisplay = aNextFrame->GetStyleDisplay();
    if (NS_STYLE_DISPLAY_TABLE_HEADER_GROUP==nextDisplay->mDisplay ||
        NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP==nextDisplay->mDisplay ||
        NS_STYLE_DISPLAY_TABLE_ROW_GROUP   ==nextDisplay->mDisplay ||
        NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP==nextDisplay->mDisplay) {
      rv = IR_TargetIsInnerTableFrame(aPresContext, aDesiredSize, aReflowState, aStatus);
    }
    else {
      NS_ASSERTION(PR_FALSE, "illegal next frame in incremental reflow.");
      rv = NS_ERROR_ILLEGAL_VALUE;
    }
  }
  return rv;
}

nsresult 
nsTableOuterFrame::IR_TargetIsInnerTableFrame(nsPresContext*           aPresContext,
                                              nsHTMLReflowMetrics&      aDesiredSize,
                                              const nsHTMLReflowState&  aReflowState,
                                              nsReflowStatus&           aStatus)
{
  nsresult rv = IR_InnerTableReflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  return rv;
}

nsresult 
nsTableOuterFrame::IR_TargetIsCaptionFrame(nsPresContext*           aPresContext,
                                           nsHTMLReflowMetrics&      aDesiredSize,
                                           const nsHTMLReflowState&  aOuterRS,
                                           nsReflowStatus&           aStatus)
{
  nsresult rv = NS_OK;
  aStatus = NS_FRAME_COMPLETE;
  PRUint8 captionSide = GetCaptionSide();

  nsSize captionSize;
  nsMargin captionMargin, captionMarginNoAuto, captionPadding;
  // reflow the caption frame, getting it's MES
  nsRect prevInnerRect = mInnerTableFrame->GetRect();
  nsMargin prevInnerMargin(prevInnerRect.x, 0, mRect.width - prevInnerRect.x - prevInnerRect.width, 0);
  nscoord availCaptionWidth = GetCaptionAvailWidth(aPresContext, mCaptionFrame, aOuterRS, captionMargin, 
                                                   captionPadding, &prevInnerRect.width, nsnull, &prevInnerMargin);
  nsHTMLReflowMetrics captionMet(PR_TRUE);
  nsReflowStatus capStatus; // don't let the caption cause incomplete
  // for now just reflow the table if a style changed. This should be improved
  PRBool needInnerReflow = PR_FALSE;
  nsReflowReason reflowReason = eReflowReason_Incremental;
  nsHTMLReflowCommand* command = aOuterRS.path->mReflowCommand;
  if (command) {
    switch(command->Type()) {
      case eReflowType_StyleChanged:
        needInnerReflow = PR_TRUE;
        break;
      default:
        break;
    }
  }
  
  OuterReflowChild(aPresContext, mCaptionFrame, aOuterRS, captionMet, availCaptionWidth, captionSize, 
                   captionMargin, captionMarginNoAuto, captionPadding, reflowReason, capStatus);

  nsMargin innerMargin, innerMarginNoAuto, innerPadding;
  nsPoint  innerOrigin;
  nsSize   containSize = GetContainingBlockSize(aOuterRS);
  nscoord  capMin = mMinCaptionWidth + captionMarginNoAuto.left + captionMarginNoAuto.right;
  

  if (mMinCaptionWidth != captionMet.mMaxElementWidth) {  
    // set the new caption min width, and set state to reflow the inner table if necessary
    mMinCaptionWidth = captionMet.mMaxElementWidth;
    // see if the captions min width could cause the table to be wider
    // XXX this really only affects an auto width table
    if ((capMin) > mRect.width) {
      needInnerReflow = PR_TRUE;
    }
  }
  if (NS_SIDE_LEFT == captionSide || NS_SIDE_RIGHT == captionSide) {
    if (mCaptionFrame) {
      PRBool isPctWidth;
      IsAutoWidth( *mCaptionFrame,&isPctWidth);
      if (isPctWidth) {
        capMin = captionSize.width + captionMarginNoAuto.left + captionMarginNoAuto.right;
      }
    }
  }

  nsPoint captionOrigin;
  if (needInnerReflow) {
    nsSize innerSize;
    nsHTMLReflowMetrics innerMet(PR_FALSE);
    nscoord availTableWidth = GetInnerTableAvailWidth(aPresContext, mInnerTableFrame, aOuterRS, 
                                                      &capMin, innerMargin, innerPadding);
    OuterReflowChild(aPresContext, mInnerTableFrame, aOuterRS, innerMet, availTableWidth, innerSize, 
                     innerMargin, innerMarginNoAuto, innerPadding, eReflowReason_Resize, aStatus);

    GetInnerOrigin(captionSide, containSize, captionSize,
                   captionMargin, innerSize, innerMargin, innerOrigin);
    rv = FinishReflowChild(mInnerTableFrame, aPresContext, nsnull, innerMet,
                           innerOrigin.x, innerOrigin.y, 0);
    if (NS_FAILED(rv)) return rv;
    
    GetCaptionOrigin(captionSide, containSize, innerSize, 
                     innerMargin, captionSize, captionMargin, captionOrigin);
  }
  else {
    // reposition the inner frame if necessary and set the caption's origin
    nsSize innerSize = mInnerTableFrame->GetSize();
    GetMarginPadding(aPresContext, aOuterRS, mInnerTableFrame, aOuterRS.availableWidth, innerMargin, 
                     innerMarginNoAuto, innerPadding);
    GetInnerOrigin(captionSide, containSize, captionSize, 
                   captionMargin, innerSize, innerMargin, innerOrigin);
    GetCaptionOrigin(captionSide, containSize, innerSize, 
                     innerMargin, captionSize, captionMargin, captionOrigin);
    MoveFrameTo(mInnerTableFrame, innerOrigin.x, innerOrigin.y); 
  }

  rv = FinishReflowChild(mCaptionFrame, aPresContext, nsnull, captionMet,
                         captionOrigin.x, captionOrigin.y, 0);
  nsRect* oldOverflowArea = GetOverflowAreaProperty();
  nsRect* overflowStorage = nsnull;
  nsRect  overflow;
  if (oldOverflowArea) {
    overflow = *oldOverflowArea;
    overflowStorage = &overflow;
  }

  UpdateReflowMetrics(captionSide, aDesiredSize, innerMargin, innerMarginNoAuto, 
                      innerPadding, captionMargin, captionMarginNoAuto, aOuterRS.availableWidth);
  nsSize desSize(aDesiredSize.width, aDesiredSize.height);
  PRBool innerMoved = (innerOrigin.x != prevInnerRect.x) || (innerOrigin.y != prevInnerRect.y);
  InvalidateDamage(captionSide, desSize, innerMoved, PR_TRUE, overflowStorage);
  return rv;
}

nsresult
nsTableOuterFrame::IR_ReflowDirty(nsPresContext*           aPresContext,
                                  nsHTMLReflowMetrics&      aDesiredSize,
                                  const nsHTMLReflowState&  aReflowState,
                                  nsReflowStatus&           aStatus)
{
  nsresult      rv = NS_OK;
  PRBool sizeSet = PR_FALSE;
  // See if the caption frame is dirty. This would be because of a newly
  // inserted caption
  if (mCaptionFrame) {
    if (mCaptionFrame->GetStateBits() & NS_FRAME_IS_DIRTY) {
      rv = IR_CaptionInserted(aPresContext, aDesiredSize, aReflowState, aStatus);
      sizeSet = PR_TRUE;
    }
  }

  // See if the inner table frame is dirty
  if (mInnerTableFrame->GetStateBits() & NS_FRAME_IS_DIRTY) {
    rv = IR_InnerTableReflow(aPresContext, aDesiredSize, aReflowState, aStatus);
    sizeSet = PR_TRUE;
  } 
  else if (!mCaptionFrame) {
    // The inner table isn't dirty so we don't need to reflow it, but make
    // sure it's placed correctly. It could be that we're dirty because the
    // caption was removed
    nsRect   innerRect = mInnerTableFrame->GetRect();
    nsSize   innerSize(innerRect.width, innerRect.height);
    nsPoint  innerOrigin;
    nsMargin innerMargin, innerMarginNoAuto, innerPadding;
    GetMarginPadding(aPresContext, aReflowState, mInnerTableFrame, aReflowState.availableWidth, innerMargin, 
                     innerMarginNoAuto, innerPadding);
    nsSize containSize = GetContainingBlockSize(aReflowState);
    GetInnerOrigin(NO_SIDE, containSize, nsSize(0,0),
                   nsMargin(0,0,0,0), innerSize, innerMargin, innerOrigin);
    MoveFrameTo(mInnerTableFrame, innerOrigin.x, innerOrigin.y); 

    aDesiredSize.width  = innerRect.XMost() + innerMargin.right;
    aDesiredSize.height = innerRect.YMost() + innerMargin.bottom; 
    sizeSet = PR_TRUE;
    // Repaint the inner's entire bounds if it moved
    nsRect* oldOverflowArea = GetOverflowAreaProperty();
    PRBool innerMoved = (innerRect.x != innerOrigin.x) ||
                         (innerRect.y != innerOrigin.y);
    nsSize desSize(aDesiredSize.width, aDesiredSize.height);
    InvalidateDamage((PRUint8) NO_SIDE, desSize, innerMoved, PR_FALSE, oldOverflowArea);
  }
  if (!sizeSet) {
    // set our desired size to what it was before
    nsSize size = GetSize();
    aDesiredSize.width  = size.width;
    aDesiredSize.height = size.height;
  }

  return rv;
}

// IR_TargetIsMe is free to foward the request to the inner table frame 
nsresult nsTableOuterFrame::IR_TargetIsMe(nsPresContext*           aPresContext,
                                          nsHTMLReflowMetrics&      aDesiredSize,
                                          const nsHTMLReflowState&  aReflowState,
                                          nsReflowStatus&           aStatus)
{
  nsresult rv = NS_OK;
  switch (aReflowState.path->mReflowCommand->Type()) {
  case eReflowType_ReflowDirty:
     rv = IR_ReflowDirty(aPresContext, aDesiredSize, aReflowState, aStatus);
    break;

  case eReflowType_StyleChanged :    
    rv = IR_InnerTableReflow(aPresContext, aDesiredSize, aReflowState, aStatus);
    break;

  case eReflowType_ContentChanged :
    NS_ASSERTION(PR_FALSE, "illegal reflow type: ContentChanged");
    rv = NS_ERROR_ILLEGAL_VALUE;
    break;
  
  default:
    NS_NOTYETIMPLEMENTED("unexpected reflow command type");
    rv = NS_ERROR_NOT_IMPLEMENTED;
    break;
  }

  return rv;
}

nsresult 
nsTableOuterFrame::IR_InnerTableReflow(nsPresContext*           aPresContext,
                                       nsHTMLReflowMetrics&      aOuterMet,
                                       const nsHTMLReflowState&  aOuterRS,
                                       nsReflowStatus&           aStatus)
{
  aStatus = NS_FRAME_COMPLETE;
  PRUint8 captionSide = GetCaptionSide();

  nsSize priorInnerSize = mInnerTableFrame->GetSize();

  nsSize   innerSize;
  nsMargin innerMargin, innerMarginNoAuto, innerPadding;

  // pass along the reflow command to the inner table, requesting the same info in our flags
  nsHTMLReflowMetrics innerMet(aOuterMet.mComputeMEW, aOuterMet.mFlags);

  // If the incremental reflow command is a StyleChanged reflow and
  // it's target is the current frame, then make sure we send
  // StyleChange reflow reasons down to the children so that they
  // don't over-optimize their reflow.  Also make sure we reflow the caption.
  PRBool reflowCaption = PR_FALSE;
  nsReflowReason reflowReason = eReflowReason_Incremental;
  nsHTMLReflowCommand* command = aOuterRS.path->mReflowCommand;
  if (command) {
    if (eReflowType_StyleChanged == command->Type()) {
      reflowReason = eReflowReason_StyleChange;
      reflowCaption = PR_TRUE;
    }
  }
  nscoord capMin = mMinCaptionWidth;
  PctAdjustMinCaptionWidth(aPresContext, aOuterRS, captionSide, capMin);
  nscoord availTableWidth = GetInnerTableAvailWidth(aPresContext, mInnerTableFrame, aOuterRS, 
                                                    &capMin, innerMargin, innerPadding);
  nsresult rv = OuterReflowChild(aPresContext, mInnerTableFrame, aOuterRS, innerMet,
                                 availTableWidth, innerSize, innerMargin, innerMarginNoAuto,
                                 innerPadding, reflowReason, aStatus, &reflowCaption);
  if (NS_FAILED(rv)) return rv;
  
  if (eReflowReason_StyleChange != reflowReason && reflowCaption) {
    // inner table frame was target for a style change reflow issue a style 
    // change reflow for the caption too.
    reflowReason = eReflowReason_StyleChange;
  }
  nsPoint  innerOrigin(0,0);
  nsMargin captionMargin(0,0,0,0);
  nsMargin captionMarginNoAuto(0,0,0,0);
  nsSize   captionSize(0,0);
  nsSize   containSize = GetContainingBlockSize(aOuterRS);
  PRBool   captionMoved = PR_FALSE;
  // if there is a caption and the width or height of the inner table changed 
  // from a reflow, then reflow or move the caption as needed
  if (mCaptionFrame) {
    nsPoint captionOrigin;
    nsRect prevCaptionRect = mCaptionFrame->GetRect();

    reflowCaption = reflowCaption ||
                    priorInnerSize.width != innerMet.width;

    if (reflowCaption) {
      nsMargin ignorePadding;
      nsHTMLReflowMetrics captionMet(eReflowReason_StyleChange == reflowReason);
      nscoord availCaptionWidth = GetCaptionAvailWidth(aPresContext, mCaptionFrame, aOuterRS, captionMargin,
                                                       ignorePadding, &innerSize.width, &innerMarginNoAuto);
      nsReflowStatus capStatus; // don't let the caption cause incomplete
      if (reflowReason == eReflowReason_Incremental) {
         reflowReason = eReflowReason_Resize;
      }
      rv = OuterReflowChild(aPresContext, mCaptionFrame, aOuterRS, captionMet, availCaptionWidth,
                            captionSize, captionMargin, captionMarginNoAuto, 
                            ignorePadding, reflowReason, capStatus);
      if (NS_FAILED(rv)) return rv;

      GetCaptionOrigin(captionSide, containSize, innerSize, 
                       innerMargin, captionSize, captionMargin, captionOrigin);
      FinishReflowChild(mCaptionFrame, aPresContext, nsnull, captionMet,
                        captionOrigin.x, captionOrigin.y, 0);

      GetInnerOrigin(captionSide, containSize, captionSize, 
                     captionMargin, innerSize, innerMargin, innerOrigin);
    }
    else {
      // reposition the caption frame if necessary and set the inner's origin
      captionSize = mCaptionFrame->GetSize();
      nsMargin captionPadding;
      GetMarginPadding(aPresContext, aOuterRS, mCaptionFrame, aOuterRS.availableWidth, captionMargin, 
                       captionMarginNoAuto, captionPadding);
      GetCaptionOrigin(captionSide, containSize, innerSize, 
                       innerMargin, captionSize, captionMargin, captionOrigin);
      GetInnerOrigin(captionSide, containSize, captionSize, 
                     captionMargin, innerSize, innerMargin, innerOrigin);
      MoveFrameTo(mCaptionFrame, captionOrigin.x, captionOrigin.y); 
    }
    if ((captionOrigin.x != prevCaptionRect.x) || (captionOrigin.y != prevCaptionRect.y)) {
      captionMoved = PR_TRUE;
    }
    if ((captionSize.width != prevCaptionRect.width) || (captionSize.height != prevCaptionRect.height)) {
      captionMoved = PR_TRUE;
    }
  }
  else {
    GetInnerOrigin(captionSide, containSize, captionSize, 
                   captionMargin, innerSize, innerMargin, innerOrigin);
  }

  FinishReflowChild(mInnerTableFrame, aPresContext, nsnull, innerMet,
                    innerOrigin.x, innerOrigin.y, 0);
  if (aOuterMet.mComputeMEW) {
    aOuterMet.mMaxElementWidth = innerMet.mMaxElementWidth;
  }
  nsRect* oldOverflowArea = GetOverflowAreaProperty();
  nsRect* overflowStorage = nsnull;
  nsRect  overflow;
  if (oldOverflowArea) {
    overflow = *oldOverflowArea;
    overflowStorage = &overflow;
  }
  
  UpdateReflowMetrics(captionSide, aOuterMet, innerMargin, innerMarginNoAuto, 
                      innerPadding, captionMargin, captionMarginNoAuto, aOuterRS.availableWidth);
  nsSize desSize(aOuterMet.width, aOuterMet.height);
  InvalidateDamage(captionSide, desSize, (innerSize.width != priorInnerSize.width),
                   captionMoved, overflowStorage);

  return rv;
}

/* the only difference between an insert and a replace is a replace 
   checks the old maxElementWidth and reflows the table only if it
   has changed
*/
nsresult 
nsTableOuterFrame::IR_CaptionInserted(nsPresContext*           aPresContext,
                                      nsHTMLReflowMetrics&      aDesiredSize,
                                      const nsHTMLReflowState&  aOuterRS,
                                      nsReflowStatus&           aStatus)
{
  PRUint8 captionSide = GetCaptionSide();
  aStatus = NS_FRAME_COMPLETE;

  // reflow the caption frame, getting it's MES
  nsSize   captionSize;
  nsMargin captionMargin, captionMarginNoAuto, captionPadding;
  nsHTMLReflowMetrics captionMet(PR_TRUE);
  // reflow the caption
  nscoord availCaptionWidth = GetCaptionAvailWidth(aPresContext, mCaptionFrame, aOuterRS, captionMargin, captionPadding);
  nsReflowStatus capStatus; // don't let the caption cause incomplete
  nsresult rv = OuterReflowChild(aPresContext, mCaptionFrame, aOuterRS, captionMet,
                                 availCaptionWidth, captionSize, captionMargin, captionMarginNoAuto,
                                 captionPadding, eReflowReason_Initial, capStatus);

  if (NS_FAILED(rv)) return rv;

  mMinCaptionWidth = captionMet.mMaxElementWidth;

  nsPoint  captionOrigin(0,0); 

  nsMargin innerMargin, innerMarginNoAuto, innerPadding;
  nsPoint innerOrigin;
  nsSize containSize = GetContainingBlockSize(aOuterRS);
  nsPoint prevInnerOrigin = mInnerTableFrame->GetPosition();
  nscoord capMin = mMinCaptionWidth + captionMarginNoAuto.left + captionMarginNoAuto.right;

  // if the caption's MES + margins > outer width, reflow the inner table
  if (capMin > mRect.width) {
    nsHTMLReflowMetrics innerMet(aDesiredSize.mComputeMEW); 
    nsSize innerSize;
    nscoord availTableWidth = GetInnerTableAvailWidth(aPresContext, mInnerTableFrame, aOuterRS, 
                                                      &capMin, innerMargin, innerPadding);
    rv = OuterReflowChild(aPresContext, mInnerTableFrame, aOuterRS, innerMet,
                          availTableWidth, innerSize, innerMargin, innerMarginNoAuto,
                          innerPadding, eReflowReason_Resize, aStatus);
    if (NS_FAILED(rv)) return rv;

    GetInnerOrigin(captionSide, containSize, captionSize, 
                   captionMargin, innerSize, innerMargin, innerOrigin);
    rv = FinishReflowChild(mInnerTableFrame, aPresContext, nsnull, innerMet,
                           innerOrigin.x, innerOrigin.y, 0);
    if (aDesiredSize.mComputeMEW) {
      aDesiredSize.mMaxElementWidth = innerMet.mMaxElementWidth;
    }
    if (NS_FAILED(rv)) return rv;
    GetCaptionOrigin(captionSide, containSize, innerSize, 
                     innerMargin, captionSize, captionMargin, captionOrigin);
  }
  else {
    // reposition the inner frame if necessary and set the caption's origin
    nsSize innerSize = mInnerTableFrame->GetSize();
    GetMarginPadding(aPresContext, aOuterRS, mInnerTableFrame, aOuterRS.availableWidth, innerMargin, 
                     innerMarginNoAuto, innerPadding);
    GetInnerOrigin(captionSide, containSize, captionSize, 
                   captionMargin, innerSize, innerMargin, innerOrigin);
    GetCaptionOrigin(captionSide, containSize, innerSize, 
                     innerMargin, captionSize, captionMargin, captionOrigin);
    MoveFrameTo(mInnerTableFrame, innerOrigin.x, innerOrigin.y); 
  }

  rv = FinishReflowChild(mCaptionFrame, aPresContext, nsnull, captionMet,
                         captionOrigin.x, captionOrigin.y, 0);

  nsRect* oldOverflowArea = GetOverflowAreaProperty();
  nsRect* overflowStorage = nsnull;
  nsRect  overflow;
  if (oldOverflowArea) {
    overflow = *oldOverflowArea;
    overflowStorage = &overflow;
  }
  UpdateReflowMetrics(captionSide, aDesiredSize, innerMargin, innerMarginNoAuto, 
                      innerPadding, captionMargin, captionMarginNoAuto, aOuterRS.availableWidth);
  nsSize desSize(aDesiredSize.width, aDesiredSize.height);
  PRBool innerMoved = innerOrigin != prevInnerOrigin;
  InvalidateDamage(captionSide, desSize, innerMoved, PR_TRUE, overflowStorage);

  return rv;
}

PRBool nsTableOuterFrame::IR_CaptionChangedAxis(const nsStyleTable* aOldStyle, 
                                                const nsStyleTable* aNewStyle) const
{
  PRBool result = PR_FALSE;
  //XXX: write me to support left|right captions!
  return result;
}


static PRBool
IsPctHeight(nsIFrame* aFrame)
{
  if (aFrame) {
    const nsStylePosition* position = aFrame->GetStylePosition();
    if (eStyleUnit_Percent == position->mHeight.GetUnit()) {
      float percent = position->mHeight.GetPercentValue();
      if (percent > 0.0f) {
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

/**
  * Reflow is a multi-step process.
  * 1. First we reflow the caption frame and get its maximum element size. We
  *    do this once during our initial reflow and whenever the caption changes
  *    incrementally
  * 2. Next we reflow the inner table. This gives us the actual table width.
  *    The table must be at least as wide as the caption maximum element size
  * 3. Now that we have the table width we reflow the caption and gets its
  *    desired height
  * 4. Then we place the caption and the inner table
  *
  * If the table height is constrained, e.g. page mode, then it's possible the
  * inner table no longer fits and has to be reflowed again this time with s
  * smaller available height
  */
NS_METHOD nsTableOuterFrame::Reflow(nsPresContext*          aPresContext,
                                    nsHTMLReflowMetrics&     aDesiredSize,
                                    const nsHTMLReflowState& aOuterRS,
                                    nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableOuterFrame", aOuterRS.reason);
  DISPLAY_REFLOW(aPresContext, this, aOuterRS, aDesiredSize, aStatus);
#if defined DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflow(this, (nsHTMLReflowState&)aOuterRS);
#endif

  // We desperately need an inner table frame, 
  // if this fails fix the frame constructor
  if (mFrames.IsEmpty() || !mInnerTableFrame) {
    NS_ERROR("incomplete children");
    return NS_ERROR_FAILURE;
  }
  nsresult rv = NS_OK;
  PRUint8 captionSide = GetCaptionSide();

  // Initialize out parameters
  aDesiredSize.width = aDesiredSize.height = 0;
  if (aDesiredSize.mComputeMEW) {
    aDesiredSize.mMaxElementWidth =  0;
  }
  aStatus = NS_FRAME_COMPLETE;

  PRBool needUpdateMetrics = PR_TRUE;
  PRBool isPctWidth;
  IsAutoWidth(*mInnerTableFrame, &isPctWidth);
  if ((eReflowReason_Resize    == aOuterRS.reason)  &&
      (aOuterRS.availableWidth == mPriorAvailWidth) &&
      !aPresContext->IsPaginated()                  &&
      !::IsPctHeight(mInnerTableFrame)              &&
      !isPctWidth) {
    // don't do much if we are resize reflowed exactly like last time
    aDesiredSize.width  = mRect.width;
    aDesiredSize.height = mRect.height;

    // for floats, our view has not been positioned yet as we have not been placed
    //  - the block code will position our views after the float is placed
    if (aOuterRS.mStyleDisplay &&
        !aOuterRS.mStyleDisplay->IsFloating()) {
      // We know our view (if we have one) has been positioned
      // correctly, but it's up to us to make sure that our child views
      // are correctly positioned, too.
      nsContainerFrame::PositionChildViews(this);
    }
  }
  else if (eReflowReason_Incremental == aOuterRS.reason) {
    rv = IncrementalReflow(aPresContext, aDesiredSize, aOuterRS, aStatus);
  } 
  else {
    if (eReflowReason_Initial == aOuterRS.reason) {
      // Set up our kids.  They're already present, on an overflow list, 
      // or there are none so we'll create them now
      MoveOverflowToChildList(aPresContext);

      // Lay out the caption and get its maximum element size
      if (nsnull != mCaptionFrame) {
        nsHTMLReflowMetrics captionMet(PR_TRUE);
        nsHTMLReflowState captionReflowState(aPresContext, aOuterRS, mCaptionFrame,
                                             nsSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE),
                                             eReflowReason_Initial);

        mCaptionFrame->WillReflow(aPresContext);
        rv = mCaptionFrame->Reflow(aPresContext, captionMet, captionReflowState, aStatus);
        mCaptionFrame->DidReflow(aPresContext, nsnull, NS_FRAME_REFLOW_FINISHED);
        mMinCaptionWidth = captionMet.mMaxElementWidth;
      }
    }
    
    nsSize   innerSize;
    nsMargin innerMargin, innerMarginNoAuto, innerPadding;

    // First reflow the inner table
    nsHTMLReflowMetrics innerMet(aDesiredSize.mComputeMEW);
    
    nscoord capMin = mMinCaptionWidth;
    PctAdjustMinCaptionWidth(aPresContext, aOuterRS, captionSide, capMin);

    nscoord availTableWidth = GetInnerTableAvailWidth(aPresContext, mInnerTableFrame, aOuterRS, 
                                                 &capMin, innerMargin, innerPadding);
    rv = OuterReflowChild(aPresContext, mInnerTableFrame, aOuterRS, innerMet,
                          availTableWidth, innerSize, innerMargin, innerMarginNoAuto,
                          innerPadding, aOuterRS.reason, aStatus);
    if (NS_FAILED(rv)) return rv;

    nsPoint  innerOrigin(0,0);
    nsMargin captionMargin(0,0,0,0), captionMarginNoAuto(0,0,0,0), ignorePadding;
    nsSize   captionSize(0,0);
    nsSize   containSize = GetContainingBlockSize(aOuterRS);

    // Now that we know the table width we can reflow the caption, and
    // place the caption and the inner table
    if (mCaptionFrame) {
      // reflow the caption
      nscoord availCaptionWidth = GetCaptionAvailWidth(aPresContext, mCaptionFrame, aOuterRS, captionMargin,
                                                       ignorePadding, &innerSize.width, &innerMarginNoAuto, &innerMargin);
      nsHTMLReflowMetrics captionMet(PR_FALSE);
      nsReflowReason reason = aOuterRS.reason;
      if (eReflowReason_Initial == aOuterRS.reason) {
        reason = eReflowReason_Resize; // we have already done the initial reflow
      }
      nsReflowStatus capStatus; // don't let the caption cause incomplete
      rv = OuterReflowChild(aPresContext, mCaptionFrame, aOuterRS, captionMet, 
                            availCaptionWidth, captionSize, captionMargin, captionMarginNoAuto,
                            ignorePadding, reason, capStatus);
      if (NS_FAILED(rv)) return rv;

      nsPoint captionOrigin;

      GetCaptionOrigin(captionSide, containSize, innerSize, 
                       innerMargin, captionSize, captionMargin, captionOrigin);
      FinishReflowChild(mCaptionFrame, aPresContext, nsnull, captionMet,
                        captionOrigin.x, captionOrigin.y, 0);

      GetInnerOrigin(captionSide, containSize, captionSize, 
                     captionMargin, innerSize, innerMargin, innerOrigin);

      // XXX If the height is constrained then we need to check whether the inner table still fits...
    } 
    else {
      GetInnerOrigin(captionSide, containSize, captionSize, 
                     captionMargin, innerSize, innerMargin, innerOrigin);
    }

    FinishReflowChild(mInnerTableFrame, aPresContext, nsnull, innerMet,
                      innerOrigin.x, innerOrigin.y, 0);
    if (aDesiredSize.mComputeMEW) {
      aDesiredSize.mMaxElementWidth = innerMet.mMaxElementWidth;
    }

    UpdateReflowMetrics(captionSide, aDesiredSize, innerMargin, innerMarginNoAuto, 
                        innerPadding, captionMargin, captionMarginNoAuto, aOuterRS.availableWidth);
    needUpdateMetrics = PR_FALSE;
  }
  
  // Return our desired rect
  if (mInnerTableFrame) {
    aDesiredSize.ascent  = mInnerTableFrame->GetAscent();
    aDesiredSize.descent = aDesiredSize.height - aDesiredSize.ascent;
  }
  else {
    aDesiredSize.ascent  = aDesiredSize.height;
    aDesiredSize.descent = 0;
  }

  // compute max element size and maximum width if it hasn't already been 
  if (needUpdateMetrics) {
    nsMargin innerMargin, innerMarginNoAuto, capMargin(0,0,0,0), 
             capMarginNoAuto(0,0,0,0), innerPadding, capPadding(0,0,0,0);
    GetMarginPadding(aPresContext, aOuterRS, mInnerTableFrame, aOuterRS.availableWidth, 
                     innerMargin, innerMarginNoAuto, innerPadding);
    if (mCaptionFrame) {
      nscoord outerWidth;
      switch (captionSide) {
        case NS_SIDE_LEFT: 
          outerWidth = innerMargin.left;
          break;
        case NS_SIDE_RIGHT:
          outerWidth = innerMargin.right;
          break;
        default:
          outerWidth = aOuterRS.availableWidth;
          break;
      }
      GetMarginPadding(aPresContext, aOuterRS, mCaptionFrame, outerWidth,
                       capMargin, capMarginNoAuto, capPadding);
    }
    UpdateReflowMetrics(captionSide, aDesiredSize, innerMargin, innerMarginNoAuto, 
                        innerPadding, capMargin, capMarginNoAuto, aOuterRS.availableWidth);
  }
#ifdef CHECK_THIS_AND_REMOVE
  // See if we are supposed to compute our maximum width
  if (aDesiredSize.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
    // XXX this needs to consider the possibility of a caption being wider 
    // than the inner table, but this is the safest way to fix bug 55545
    if (mInnerTableFrame) {
      aDesiredSize.mMaximumWidth = ((nsTableFrame*)mInnerTableFrame)->GetPreferredWidth();
    }
  }
#endif

  mPriorAvailWidth = aOuterRS.availableWidth;

#if defined DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflow(this, (nsHTMLReflowState&)aOuterRS, &aDesiredSize, aStatus);
#endif
  NS_FRAME_SET_TRUNCATION(aStatus, aOuterRS, aDesiredSize);
  return rv;
}

NS_METHOD nsTableOuterFrame::VerifyTree() const
{
  return NS_OK;
}

/**
 * Remove and delete aChild's next-in-flow(s). Updates the sibling and flow
 * pointers.
 *
 * Updates the child count and content offsets of all containers that are
 * affected
 *
 * Overloaded here because nsContainerFrame makes assumptions about pseudo-frames
 * that are not true for tables.
 *
 * @param   aChild child this child's next-in-flow
 * @return  PR_TRUE if successful and PR_FALSE otherwise
 */
void nsTableOuterFrame::DeleteChildsNextInFlow(nsPresContext* aPresContext, 
                                               nsIFrame*       aChild)
{
  if (!aChild) return;
  NS_PRECONDITION(mFrames.ContainsFrame(aChild), "bad geometric parent");

  nsIFrame* nextInFlow = aChild->GetNextInFlow();
  if (!nextInFlow) {
    NS_ASSERTION(PR_FALSE, "null next-in-flow");
    return;
  }

  nsTableOuterFrame* parent = NS_STATIC_CAST(nsTableOuterFrame*,
                                             nextInFlow->GetParent());
  if (!parent) {
    NS_ASSERTION(PR_FALSE, "null parent");
    return;
  }
  // If the next-in-flow has a next-in-flow then delete it too (and
  // delete it first).
  nsIFrame* nextNextInFlow = nextInFlow->GetNextInFlow();
  if (nextNextInFlow) {
    parent->DeleteChildsNextInFlow(aPresContext, nextInFlow);
  }

  // Disconnect the next-in-flow from the flow list
  nsSplittableFrame::BreakFromPrevFlow(nextInFlow);

  // Take the next-in-flow out of the parent's child list
  if (parent->mFrames.FirstChild() == nextInFlow) {
    parent->mFrames.SetFrames(nextInFlow->GetNextSibling());
  } else {
    // Because the next-in-flow is not the first child of the parent
    // we know that it shares a parent with aChild. Therefore, we need
    // to capture the next-in-flow's next sibling (in case the
    // next-in-flow is the last next-in-flow for aChild AND the
    // next-in-flow is not the last child in parent)
    NS_ASSERTION(aChild->GetNextSibling() == nextInFlow, "unexpected sibling");

    aChild->SetNextSibling(nextInFlow->GetNextSibling());
  }

  // Delete the next-in-flow frame and adjust its parent's child count
  nextInFlow->Destroy();

  NS_POSTCONDITION(!aChild->GetNextInFlow(), "non null next-in-flow");
}

nsIAtom*
nsTableOuterFrame::GetType() const
{
  return nsLayoutAtoms::tableOuterFrame;
}

/* ----- global methods ----- */

/*------------------ nsITableLayout methods ------------------------------*/
NS_IMETHODIMP 
nsTableOuterFrame::GetCellDataAt(PRInt32 aRowIndex, PRInt32 aColIndex, 
                                 nsIDOMElement* &aCell,   //out params
                                 PRInt32& aStartRowIndex, PRInt32& aStartColIndex, 
                                 PRInt32& aRowSpan, PRInt32& aColSpan,
                                 PRInt32& aActualRowSpan, PRInt32& aActualColSpan,
                                 PRBool& aIsSelected)
{
  if (!mInnerTableFrame) { return NS_ERROR_NOT_INITIALIZED; }
  nsITableLayout *inner;
  if (NS_SUCCEEDED(CallQueryInterface(mInnerTableFrame, &inner))) {
    return (inner->GetCellDataAt(aRowIndex, aColIndex, aCell,
                                 aStartRowIndex, aStartColIndex, 
                                 aRowSpan, aColSpan, aActualRowSpan, aActualColSpan, 
                                 aIsSelected));
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsTableOuterFrame::GetTableSize(PRInt32& aRowCount, PRInt32& aColCount)
{
  if (!mInnerTableFrame) { return NS_ERROR_NOT_INITIALIZED; }
  nsITableLayout *inner;
  if (NS_SUCCEEDED(CallQueryInterface(mInnerTableFrame, &inner))) {
    return (inner->GetTableSize(aRowCount, aColCount));
  }
  return NS_ERROR_NULL_POINTER;
}

/*---------------- end of nsITableLayout implementation ------------------*/


nsIFrame*
NS_NewTableOuterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTableOuterFrame(aContext);
}

#ifdef DEBUG
NS_IMETHODIMP
nsTableOuterFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("TableOuter"), aResult);
}
#endif

