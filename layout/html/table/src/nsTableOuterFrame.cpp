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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsTableOuterFrame.h"
#include "nsTableFrame.h"
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "nsVoidArray.h"
#include "nsIPtr.h"
#include "prinrval.h"
#include "nsHTMLIIDs.h"
#include "nsLayoutAtoms.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsIPrintContext.h"

/* ----------- nsTableCaptionFrame ---------- */

#define NS_TABLE_FRAME_CAPTION_LIST_INDEX 0
#define NO_SIDE 100

// caption frame
nsTableCaptionFrame::nsTableCaptionFrame()
{
  // shrink wrap 
  SetFlags(NS_BLOCK_SPACE_MGR|NS_BLOCK_WRAP_SIZE);
}

nsTableCaptionFrame::~nsTableCaptionFrame()
{
}

NS_IMETHODIMP
nsTableOuterFrame::Destroy(nsIPresContext* aPresContext)
{
  if (mCaptionFrame) {
    mCaptionFrame->Destroy(aPresContext);
  }

  return nsHTMLContainerFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsTableCaptionFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::tableCaptionFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

nsresult 
NS_NewTableCaptionFrame(nsIPresShell* aPresShell, 
                        nsIFrame**    aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTableCaptionFrame* it = new (aPresShell) nsTableCaptionFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

/* ----------- nsTableOuterFrame ---------- */

NS_IMPL_ADDREF_INHERITED(nsTableOuterFrame, nsHTMLContainerFrame)
NS_IMPL_RELEASE_INHERITED(nsTableOuterFrame, nsHTMLContainerFrame)

nsTableOuterFrame::nsTableOuterFrame()
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
    nsIAccessible* acc = nsnull;
    return accService->CreateHTMLTableAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

NS_IMETHODIMP
nsTableOuterFrame::IsPercentageBase(PRBool& aBase) const
{
  aBase = PR_FALSE;
  return NS_OK;
}

// tables change 0 width into auto, trees override this and do nothing
NS_IMETHODIMP
nsTableOuterFrame::AdjustZeroWidth()
{
  /* It does not appear that this function is needed any longer, since
   the content node handles the deprecated width="0" attribute case. -- dwh
  */
  return NS_OK;
}

NS_IMETHODIMP
nsTableOuterFrame::Init(nsIPresContext*  aPresContext,
                   nsIContent*           aContent,
                   nsIFrame*             aParent,
                   nsIStyleContext*      aContext,
                   nsIFrame*             aPrevInFlow)
{
  nsresult rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, 
                                           aContext, aPrevInFlow);
  if (NS_FAILED(rv) || !mStyleContext) return rv;
  AdjustZeroWidth();

  // record that children that are ignorable whitespace should be excluded 
  mState |= NS_FRAME_EXCLUDE_IGNORABLE_WHITESPACE;

  return rv;
}

NS_IMETHODIMP
nsTableOuterFrame::FirstChild(nsIPresContext* aPresContext,
                              nsIAtom*        aListName,
                              nsIFrame**      aFirstChild) const
{
  NS_PRECONDITION(nsnull != aFirstChild, "null OUT parameter pointer");
  *aFirstChild = (nsLayoutAtoms::captionList == aListName)
                 ? mCaptionFrame : mFrames.FirstChild(); 
  return NS_OK;
}

NS_IMETHODIMP
nsTableOuterFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                              nsIAtom** aListName) const
{
  NS_PRECONDITION(nsnull != aListName, "null OUT parameter pointer");
  if (aIndex < 0) {
    return NS_ERROR_INVALID_ARG;
  }
  *aListName = nsnull;
  switch (aIndex) {
  case NS_TABLE_FRAME_CAPTION_LIST_INDEX:
    *aListName = nsLayoutAtoms::captionList;
    NS_ADDREF(*aListName);
    break;
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsTableOuterFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                       nsIAtom*        aListName,
                                       nsIFrame*       aChildList)
{
  if (nsLayoutAtoms::captionList == aListName) {
    // the frame constructor already checked for table-caption display type
    mCaptionFrame = aChildList;
  }
  else {
    mFrames.SetFrames(aChildList);
    mInnerTableFrame = aChildList;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTableOuterFrame::AppendFrames(nsIPresContext* aPresContext,
                                nsIPresShell&   aPresShell,
                                nsIAtom*        aListName,
                                nsIFrame*       aFrameList)
{
  nsresult rv;

  // We only have two child frames: the inner table and one caption frame.
  // The inner frame is provided when we're initialized, and it cannot change
  if (nsLayoutAtoms::captionList == aListName) {
    NS_PRECONDITION(!mCaptionFrame, "already have a caption frame");
    // We only support having a single caption frame
    if (mCaptionFrame || (LengthOf(aFrameList) > 1)) {
      rv = NS_ERROR_UNEXPECTED;
    } else {
      // Insert the caption frame into the child list
      mCaptionFrame = aFrameList;

      // Reflow the new caption frame. It's already marked dirty, so generate a reflow
      // command that tells us to reflow our dirty child frames
      nsIReflowCommand* reflowCmd;
  
      rv = NS_NewHTMLReflowCommand(&reflowCmd, this, nsIReflowCommand::ReflowDirty);
      if (NS_SUCCEEDED(rv)) {
        aPresShell.AppendReflowCommand(reflowCmd);
        NS_RELEASE(reflowCmd);
      }
    }
  }
  else {
    NS_PRECONDITION(PR_FALSE, "unexpected child frame type");
    rv = NS_ERROR_UNEXPECTED;
  }

  return rv;
}

NS_IMETHODIMP
nsTableOuterFrame::InsertFrames(nsIPresContext* aPresContext,
                                nsIPresShell&   aPresShell,
                                nsIAtom*        aListName,
                                nsIFrame*       aPrevFrame,
                                nsIFrame*       aFrameList)
{
  NS_PRECONDITION(!aPrevFrame, "invalid previous frame");
  return AppendFrames(aPresContext, aPresShell, aListName, aFrameList);
}

NS_IMETHODIMP
nsTableOuterFrame::RemoveFrame(nsIPresContext* aPresContext,
                               nsIPresShell&   aPresShell,
                               nsIAtom*        aListName,
                               nsIFrame*       aOldFrame)
{
  nsresult rv;

  // We only have two child frames: the inner table and one caption frame.
  // The inner frame can't be removed so this should be the caption
  NS_PRECONDITION(nsLayoutAtoms::captionList == aListName, "can't remove inner frame");
  NS_PRECONDITION(aOldFrame == mCaptionFrame, "invalid caption frame");

  // See if the caption's minimum width impacted the inner table
  if (mMinCaptionWidth > mRect.width) {
    // The old caption width had an effect on the inner table width so
    // we're going to need to reflow it. Mark it dirty
    nsFrameState  frameState;
    mInnerTableFrame->GetFrameState(&frameState);
    frameState |= NS_FRAME_IS_DIRTY;
    mInnerTableFrame->SetFrameState(frameState);
  }

  // Remove the caption frame and destroy it
  if (mCaptionFrame && (mCaptionFrame == aOldFrame)) {
    mCaptionFrame->Destroy(aPresContext);
    mCaptionFrame = nsnull;
    mMinCaptionWidth = 0;
  }

  // Generate a reflow command so we get reflowed
  nsIReflowCommand* reflowCmd;

  rv = NS_NewHTMLReflowCommand(&reflowCmd, this, nsIReflowCommand::ReflowDirty);
  if (NS_SUCCEEDED(rv)) {
    aPresShell.AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
  }

  return NS_OK;
}

NS_METHOD 
nsTableOuterFrame::Paint(nsIPresContext*      aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect&        aDirtyRect,
                         nsFramePaintLayer    aWhichLayer,
                         PRUint32             aFlags)
{
#ifdef DEBUG
  // for debug...
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(255,0,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
#endif
  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_FALSE, &isVisible)) && !isVisible) {
    return NS_OK;
  }

  // the remaining code was copied from nsContainerFrame::PaintChildren since
  // it only paints the primary child list

  const nsStyleDisplay* disp = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);

  // Child elements have the opportunity to override the visibility property
  // of their parent and display even if the parent is hidden
  PRBool clipState;

  // If overflow is hidden then set the clip rect so that children
  // don't leak out of us
  if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(nsRect(0, 0, mRect.width, mRect.height),
                                  nsClipCombine_kIntersect, clipState);
  }

  if (mCaptionFrame) {
    PaintChild(aPresContext, aRenderingContext, aDirtyRect, mCaptionFrame, aWhichLayer);
  }
  nsIFrame* kid = mFrames.FirstChild();
  while (nsnull != kid) {
    PaintChild(aPresContext, aRenderingContext, aDirtyRect, kid, aWhichLayer);
    kid->GetNextSibling(&kid);
  }

  if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
    aRenderingContext.PopState(clipState);
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsTableOuterFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                   const nsPoint& aPoint, 
                                   nsFramePaintLayer aWhichLayer,
                                   nsIFrame**     aFrame)
{
  nsresult rv;
  
  // caption frames live in a different list which we need to check separately
  if (mCaptionFrame) {
    rv = GetFrameForPointUsing(aPresContext, aPoint, nsLayoutAtoms::captionList, NS_FRAME_PAINT_LAYER_FOREGROUND, PR_FALSE, aFrame);
    if (NS_OK == rv) {
      return NS_OK;
    }
  }
  // this should act like a block, so we need to override
  return GetFrameForPointUsing(aPresContext, aPoint, nsnull, aWhichLayer, (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND), aFrame);
}

NS_IMETHODIMP nsTableOuterFrame::SetSelected(nsIPresContext* aPresContext,
                                             nsIDOMRange *aRange,
                                             PRBool aSelected,
                                             nsSpread aSpread)
{
  nsresult result = nsFrame::SetSelected(aPresContext, aRange,aSelected, aSpread);
  if (NS_SUCCEEDED(result) && mInnerTableFrame)
    return mInnerTableFrame->SetSelected(aPresContext, aRange,aSelected, aSpread);
  return result;
}

// GetParentStyleContextProvider:
//  The innerTableFame is he parent style context provider
//  Fortunately, we cache that as a data member, so just return the cached pointer value
//
NS_IMETHODIMP 
nsTableOuterFrame::GetParentStyleContextProvider(nsIPresContext* aPresContext,
                                                 nsIFrame** aProviderFrame, 
                                                 nsContextProviderRelationship& aRelationship)
{
  NS_ASSERTION(aProviderFrame && aPresContext, "null argument: aPresContext and-or aProviderFrame");
  if (aProviderFrame) {
    // parent context provider is the innerTableFrame
    NS_ASSERTION(mInnerTableFrame, "innerTableFrame should not be null");
    *aProviderFrame = mInnerTableFrame;
    aRelationship = eContextProvider_Descendant;
  }
  return ((aProviderFrame != nsnull) && (*aProviderFrame != nsnull)) ? NS_OK : NS_ERROR_FAILURE;
}

// INCREMENTAL REFLOW HELPER FUNCTIONS 

nsSize
GetFrameSize(nsIFrame& aFrame) 
{
  nsRect rect;
  aFrame.GetRect(rect);
  nsSize size(rect.width, rect.height);
  return size;
}

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

void 
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

// get the margin and padding data. nsHTMLReflowState doesn't handle the
// case of auto margins
void
GetMarginPadding(nsIPresContext*          aPresContext,                     
                 const nsHTMLReflowState& aOuterRS,
                 nsIFrame*                aChildFrame,
                 nsMargin&                aMargin,
                 nsMargin&                aMarginNoAuto,
                 nsMargin&                aPadding)
{
  // construct a reflow state to compute margin and padding. Auto margins
  // will not be computed at this time.
  nsHTMLReflowState childRS(aPresContext, aOuterRS, aChildFrame, 
                            nsSize(aOuterRS.availableWidth, aOuterRS.availableHeight), 
                            eReflowReason_Resize);
  nsRect childRect;
  aChildFrame->GetRect(childRect);
  FixAutoMargins(aOuterRS.availableWidth, childRect.width, childRS); 
  aMargin = childRS.mComputedMargin;
  aMarginNoAuto = aMargin;
  nsTableOuterFrame::ZeroAutoMargin(childRS, aMarginNoAuto);

  aPadding = childRS.mComputedPadding;
}

nscoord CalcAutoMargin(nscoord aAutoMargin,
                       nscoord aOppositeMargin,
                       nscoord aContainBlockSize,
                       nscoord aFrameSize)
{
  nscoord margin;
  if (NS_AUTOMARGIN == aOppositeMargin) {
    margin = (aContainBlockSize - aFrameSize) / 2;
  }
  else {
    margin = aContainBlockSize - aFrameSize - aOppositeMargin;
  }
  return PR_MAX(0, margin);
}

nscoord
nsTableOuterFrame::GetChildAvailWidth(nsIPresContext*          aPresContext,
                                      nsIFrame*                aChildFrame,
                                      const nsHTMLReflowState& aOuterRS,
                                      nscoord                  aOuterWidth,
                                      nsMargin&                aMarginNoAuto,
                                      nsMargin&                aPadding)
{
  if (NS_UNCONSTRAINEDSIZE == aOuterWidth) return aOuterWidth;

  nsMargin marginIgnore;
  GetMarginPadding(aPresContext, aOuterRS, aChildFrame, marginIgnore, aMarginNoAuto, aPadding);
  nscoord width = aOuterWidth;
  if (NS_UNCONSTRAINEDSIZE != width) {
    width = aOuterWidth - aMarginNoAuto.left - aMarginNoAuto.right;
    width = PR_MAX(width, mMinCaptionWidth);
  }
  return width;
}

void
MoveFrameTo(nsIPresContext* aPresContext, 
            nsIFrame*       aFrame, 
            nscoord         aX,
            nscoord         aY)
{
  nsRect oldRect;
  aFrame->GetRect(oldRect);
  if ((oldRect.x != aX) || (oldRect.y != aY)) {
    aFrame->MoveTo(aPresContext, aX, aY);
    nsTableFrame::RePositionViews(aPresContext, aFrame);
  }
}

nsSize
GetContainingBlockSize(const nsHTMLReflowState& aOuterRS)
{
  nsSize size(0,0);
  const nsHTMLReflowState* containRS = 
    nsHTMLReflowState::GetContainingBlockReflowState(aOuterRS.parentReflowState);

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
nsTableOuterFrame::InvalidateDamage(nsIPresContext* aPresContext,
                                    PRUint8         aCaptionSide,
                                    nsSize&         aOuterSize,
                                    PRBool          aInnerChanged,
                                    PRBool          aCaptionChanged)
{
  if (!aInnerChanged && !aCaptionChanged) return;

  nsRect damage;
  if (aInnerChanged && aCaptionChanged) {
    damage = nsRect(0, 0, aOuterSize.width, aOuterSize.height);
  }
  else {
    nsRect innerRect, captionRect(0,0,0,0);
    mInnerTableFrame->GetRect(innerRect);
    if (mCaptionFrame) {
      mCaptionFrame->GetRect(captionRect);
    }
    // only works for vertical captions {
    damage.x = 0;
    damage.width  = aOuterSize.width;
    switch(aCaptionSide) {
    case NS_SIDE_BOTTOM:
      if (aCaptionChanged) {
        damage.y = innerRect.y;
        damage.height = aOuterSize.height - damage.y;
      }
      else {
        damage.y = 0;
        damage.height = captionRect.y;
      }
      break;
    default:
      if (aCaptionChanged) {
        damage.y = 0;
        damage.height = innerRect.y;
      }
      else {
        damage.y = captionRect.y;
        damage.height = aOuterSize.height - damage.y;
      }
      break;
    }
  }
  Invalidate(aPresContext, damage);
}

nscoord
nsTableOuterFrame::GetCaptionAvailWidth(nsIPresContext*          aPresContext,
                                        nsIFrame*                aCaptionFrame,
                                        const nsHTMLReflowState& aOuterRS,
                                        nscoord*                 aInnerWidth,
                                        const nsMargin*          aInnerMarginNoAuto)
{
  nscoord outerWidth;
  if (aInnerWidth) {
    nscoord innerWidth = *aInnerWidth;
    if (NS_UNCONSTRAINEDSIZE == innerWidth) {
      outerWidth = innerWidth;
    }
    else {
      nsMargin innerMargin(0,0,0,0);
      if (aInnerMarginNoAuto) {
        innerMargin = *aInnerMarginNoAuto;
      }
      outerWidth = innerWidth + innerMargin.left + innerMargin.right;
    }
  }
  else {
    nsSize outerSize = GetFrameSize(*this);
    outerWidth = outerSize.width;
  }

  if (NS_UNCONSTRAINEDSIZE == outerWidth) {
    return outerWidth;
  }
  else {
    nsMargin capMargin, capPad;
    return GetChildAvailWidth(aPresContext, aCaptionFrame, aOuterRS,
                              outerWidth, capMargin, capPad);
  }
}

nsSize
nsTableOuterFrame::GetMaxElementSize(const nsMargin& aInnerMargin,
                                     const nsMargin& aInnerPadding,
                                     const nsMargin& aCaptionMargin)
{
  nsSize size(0,0);
  size.width += aInnerMargin.left + 
                ((nsTableFrame *)mInnerTableFrame)->GetMinWidth() + 
                aInnerMargin.right;
  if (mCaptionFrame) {
    nscoord capWidth = mMinCaptionWidth + aCaptionMargin.left + aCaptionMargin.right;
    if (capWidth > size.width) {
      size.width = capWidth;
    }
  }
  // leave max element height set to 0. It is not used for anything
  return size;
}

nscoord
nsTableOuterFrame::GetMaxWidth(PRUint8         aCaptionSide,
                               const nsMargin& aInnerMargin,
                               const nsMargin& aCaptionMargin)
{
  nscoord maxWidth;
  switch(aCaptionSide) {
  case NS_SIDE_TOP:
  case NS_SIDE_BOTTOM:
  case NS_SIDE_LEFT:
  case NS_SIDE_RIGHT:
  default:  // no caption
    maxWidth = ((nsTableFrame *)mInnerTableFrame)->GetPreferredWidth() + 
               aInnerMargin.left + aInnerMargin.right;
    if (mCaptionFrame) {
      maxWidth = PR_MAX(maxWidth, mMinCaptionWidth + aCaptionMargin.left + aCaptionMargin.right);
    }
  }
  return maxWidth;
}


PRUint8
nsTableOuterFrame::GetCaptionSide()
{
  const nsStyleTableBorder* tableStyle;
  if (mCaptionFrame) {
    mCaptionFrame->GetStyleData(eStyleStruct_TableBorder, ((const nsStyleStruct *&)tableStyle));
    return tableStyle->mCaptionSide;
  }
  else {
    return NO_SIDE; // no caption
  }
}



void
nsTableOuterFrame::SetDesiredSize(PRUint8         aCaptionSide,
                                  const nsMargin& aInnerMargin,
                                  const nsMargin& aCaptionMargin,
                                  nscoord&        aWidth,
                                  nscoord&        aHeight)
{
  aWidth = aHeight = 0;

  nsRect innerRect;
  mInnerTableFrame->GetRect(innerRect);

  nsRect captionRect(0,0,0,0);
  if (mCaptionFrame) {
    mCaptionFrame->GetRect(captionRect);
  }

  switch(aCaptionSide) {
  case NS_SIDE_TOP:
  case NS_SIDE_BOTTOM:
  case NS_SIDE_LEFT:
  case NS_SIDE_RIGHT:
  default:  // no caption
    aWidth = innerRect.XMost() + aInnerMargin.right;
    aWidth = PR_MAX(aWidth, captionRect.XMost() + aCaptionMargin.right);
    break;
  }

  switch(aCaptionSide) {
  case NS_SIDE_BOTTOM:
    aHeight = captionRect.YMost() + aCaptionMargin.bottom;
    break;
  default:  // top caption or no caption
    aHeight = innerRect.YMost() + aInnerMargin.bottom;
  }
}


nsresult 
nsTableOuterFrame::GetCaptionOrigin(nsIPresContext*  aPresContext,
                                    PRUint32         aCaptionSide,
                                    const nsSize&    aContainBlockSize,
                                    const nsSize&    aInnerSize, 
                                    const nsMargin&  aInnerMargin,
                                    const nsSize&    aCaptionSize,
                                    nsMargin&        aCaptionMargin,
                                    nsPoint&         aOrigin)
{
  aOrigin.x = aOrigin.y = 0;
  if ((NS_UNCONSTRAINEDSIZE == aInnerSize.width) || (NS_UNCONSTRAINEDSIZE == aInnerSize.width) ||  
      (NS_UNCONSTRAINEDSIZE == aCaptionSize.width) || (NS_UNCONSTRAINEDSIZE == aCaptionSize.width)) {
    return NS_OK;
  }
  if (!mCaptionFrame) return NS_OK;

  nscoord collapseMargin;
  switch(aCaptionSide) {
  case NS_SIDE_BOTTOM:
    if (NS_AUTOMARGIN == aCaptionMargin.left) {
      aCaptionMargin.left = CalcAutoMargin(aCaptionMargin.left, aCaptionMargin.right,
                                           aContainBlockSize.width, aCaptionSize.width);
    }
    aOrigin.x = aCaptionMargin.left;
    if (NS_AUTOMARGIN == aCaptionMargin.top) {
      aCaptionMargin.top = 0;
    }
    collapseMargin = PR_MAX(aCaptionMargin.top, aInnerMargin.bottom);
    if (NS_AUTOMARGIN == aCaptionMargin.bottom) {
      nscoord height = aInnerSize.height + collapseMargin + aCaptionSize.height;
      aCaptionMargin.bottom = CalcAutoMargin(aCaptionMargin.bottom, aInnerMargin.top,
                                             aContainBlockSize.height, height);
    }
    aOrigin.y = aInnerMargin.top + aInnerSize.height + collapseMargin;
    break;
  default: // all others are treated as top for now
    if (NS_AUTOMARGIN == aCaptionMargin.left) {
      aCaptionMargin.left = CalcAutoMargin(aCaptionMargin.left, aCaptionMargin.right,
                                           aContainBlockSize.width, aCaptionSize.width);
    }
    aOrigin.x = aCaptionMargin.left;
    if (NS_AUTOMARGIN == aCaptionMargin.bottom) {
      aCaptionMargin.bottom = 0;
    }
    if (NS_AUTOMARGIN == aCaptionMargin.top) {
      collapseMargin = PR_MAX(aCaptionMargin.bottom, aInnerMargin.top);
      nscoord height = aCaptionSize.height + collapseMargin + aInnerSize.height;
      aCaptionMargin.top = CalcAutoMargin(aCaptionMargin.top, aInnerMargin.bottom,
                                          aContainBlockSize.height, height);
    }
    aOrigin.y = aCaptionMargin.top;
    break;

  }
  return NS_OK;
}

nsresult 
nsTableOuterFrame::GetInnerOrigin(nsIPresContext*  aPresContext,
                                  PRUint32         aCaptionSide,
                                  const nsSize&    aContainBlockSize,
                                  const nsSize&    aCaptionSize, 
                                  const nsMargin&  aCaptionMargin,
                                  const nsSize&    aInnerSize,
                                  nsMargin&        aInnerMargin,
                                  nsPoint&         aOrigin)
{
  aOrigin.x = aOrigin.y = 0;
  if ((NS_UNCONSTRAINEDSIZE == aInnerSize.width) || (NS_UNCONSTRAINEDSIZE == aInnerSize.width) ||  
      (NS_UNCONSTRAINEDSIZE == aCaptionSize.width) || (NS_UNCONSTRAINEDSIZE == aCaptionSize.width)) {
    return NS_OK;
  }

  nscoord collapseMargin;
  switch(aCaptionSide) {
  case NS_SIDE_BOTTOM:
    if (NS_AUTOMARGIN == aInnerMargin.left) {
      aInnerMargin.left = CalcAutoMargin(aInnerMargin.left, aInnerMargin.right,
                                         aContainBlockSize.width, aInnerSize.width);
    }
    aOrigin.x = aInnerMargin.left;
    if (NS_AUTOMARGIN == aInnerMargin.bottom) {
      aInnerMargin.bottom = 0;
    }
    if (NS_AUTOMARGIN == aInnerMargin.top) {
      collapseMargin = PR_MAX(aInnerMargin.bottom, aCaptionMargin.top);
      nscoord height = aInnerSize.height + collapseMargin + aCaptionSize.height;
      aInnerMargin.top = CalcAutoMargin(aInnerMargin.top, aCaptionMargin.bottom,
                                        aContainBlockSize.height, height);
    }
    aOrigin.y = aInnerMargin.top;
    break;

  default: // all others are treated as top for now
    if (NS_AUTOMARGIN == aInnerMargin.left) {
      aInnerMargin.left = CalcAutoMargin(aInnerMargin.left, aInnerMargin.right,
                                         aContainBlockSize.width, aInnerSize.width);
    }
    aOrigin.x = aInnerMargin.left;
    if (NS_AUTOMARGIN == aInnerMargin.top) {
      aInnerMargin.top = 0;
    }
    collapseMargin = PR_MAX(aCaptionMargin.bottom, aInnerMargin.top);
    if (NS_AUTOMARGIN == aInnerMargin.bottom) {
      nscoord height = aCaptionSize.height + collapseMargin + aInnerSize.height;
      aInnerMargin.bottom = CalcAutoMargin(aCaptionMargin.bottom, aInnerMargin.top,
                                           aContainBlockSize.height, height);
    }
    aOrigin.y = aCaptionMargin.top + aCaptionSize.height + collapseMargin;
    break;
  }
  return NS_OK;
}

// helper method for determining if this is a nested table or not
PRBool 
nsTableOuterFrame::IsNested(const nsHTMLReflowState& aReflowState) const
{
  PRBool result = PR_FALSE;
  // Walk up the reflow state chain until we find a cell or the root
  const nsHTMLReflowState* rs = aReflowState.parentReflowState;
  while (rs) {
    nsCOMPtr<nsIAtom> frameType;
    rs->frame->GetFrameType(getter_AddRefs(frameType));
    if (nsLayoutAtoms::tableFrame == frameType.get()) {
      result = PR_TRUE;
      break;
    }
    rs = rs->parentReflowState;
  }
  return result;
}

PRBool 
nsTableOuterFrame::IsAutoWidth(nsIFrame& aTableOrCaption,
                               PRBool*   aIsPctWidth)
{
  PRBool isAuto = PR_TRUE;  // the default
  if (aIsPctWidth) {
    *aIsPctWidth = PR_FALSE;
  }

  nsCOMPtr<nsIStyleContext> styleContext;
  aTableOrCaption.GetStyleContext(getter_AddRefs(styleContext)); 

  nsStylePosition* position = (nsStylePosition*)styleContext->GetStyleData(eStyleStruct_Position);

  switch (position->mWidth.GetUnit()) {
    case eStyleUnit_Auto:         // specified auto width
    case eStyleUnit_Proportional: // illegal for table, so ignored
      break;
    case eStyleUnit_Inherit:
      // get width of parent and see if it is a specified value or not
      // XXX for now, just return true
      break;
    case eStyleUnit_Coord:
      isAuto = PR_FALSE;
      break;
    case eStyleUnit_Percent:
      if (position->mWidth.GetPercentValue() > 0.0f) {
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
nsTableOuterFrame::OuterReflowChild(nsIPresContext*            aPresContext,
                                    nsIFrame*                  aChildFrame,
                                    const nsHTMLReflowState&   aOuterRS,
                                    nsHTMLReflowMetrics&       aMetrics,
                                    nscoord*                   aAvailWidth,
                                    nsSize&                    aDesiredSize,
                                    nsMargin&                  aMargin,
                                    nsMargin&                  aMarginNoAuto,
                                    nsMargin&                  aPadding,
                                    nsReflowReason             aReflowReason,
                                    nsReflowStatus&            aStatus)
{ 
  aMargin = aPadding = nsMargin(0,0,0,0);

  nscoord availWidth = GetChildAvailWidth(aPresContext, aChildFrame, aOuterRS,
                                          aOuterRS.availableWidth, aMargin, aPadding);
  if (aAvailWidth) {
    availWidth = *aAvailWidth;
  }
  nsHTMLReflowState childRS(aPresContext, aOuterRS, aChildFrame,
                            nsSize(availWidth, aOuterRS.availableHeight));
  childRS.reason = aReflowReason;

  // If mComputedWidth > availableWidth for a nested percent table then adjust it
  // based on availableWidth if this isn't the intial reflow. 
  if ((childRS.mComputedWidth > childRS.availableWidth) && 
      (NS_UNCONSTRAINEDSIZE != childRS.mComputedWidth)  &&
      (eReflowReason_Initial != aReflowReason)          &&
      IsNested(aOuterRS)) {
    PRBool isPctWidth;
    IsAutoWidth(*aChildFrame, &isPctWidth);
    if (isPctWidth) {
      childRS.mComputedWidth = childRS.availableWidth - childRS.mComputedBorderPadding.left -
                                                        childRS.mComputedBorderPadding.right;
    }
  }

  // Normally, the outer table's mComputed values are NS_INTRINSICSIZE (although to
  // to work around boxes they can also be set to 0) since they depend on the caption 
  // and inner table. Boxes can force a constrained size.
  // XXX remove this code when trees are converted to use grids instead of tables
  if ((aOuterRS.mComputedWidth != NS_INTRINSICSIZE) && 
      (aOuterRS.mComputedWidth != 0)) {
    if (mInnerTableFrame == aChildFrame) {
      childRS.mComputedWidth = aOuterRS.mComputedWidth - aMargin.left - childRS.mComputedBorderPadding.left -
                               childRS.mComputedBorderPadding.right - aMargin.right;
      childRS.mComputedWidth = PR_MAX(0, childRS.mComputedWidth);
    }
    else {
      NS_ASSERTION(PR_FALSE, "box set mComputedWidth on an outer table with a caption");
    }
  }
  if ((aOuterRS.mComputedHeight != NS_INTRINSICSIZE) &&
      (aOuterRS.mComputedHeight != 0)) {
    if (mInnerTableFrame == aChildFrame) {
      childRS.mComputedHeight = aOuterRS.mComputedHeight - aMargin.top - childRS.mComputedBorderPadding.top -
                                childRS.mComputedBorderPadding.bottom - aMargin.bottom;
      childRS.mComputedHeight = PR_MAX(0, childRS.mComputedHeight);
    }
    else {
      NS_ASSERTION(PR_FALSE, "box set mComputedHeight on an outer table with a caption");
    }
  }

  // use the current position as a best guess for placement
  nsRect childRect;
  aChildFrame->GetRect(childRect);
  nsresult rv = ReflowChild(aChildFrame, aPresContext, aMetrics, childRS,
                            childRect.x, childRect.y, NS_FRAME_NO_MOVE_FRAME, aStatus);
  if (NS_FAILED(rv)) return rv;

  FixAutoMargins(aOuterRS.availableWidth, aMetrics.width, childRS);
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
                                       const nsMargin&      aCaptionMarginNoAuto)
{
  SetDesiredSize(aCaptionSide, aInnerMargin, aCaptionMargin, aMet.width, aMet.height);

  // set maxElementSize width if requested
  if (aMet.maxElementSize) {
    *aMet.maxElementSize = GetMaxElementSize(aInnerMarginNoAuto, aInnerPadding, aCaptionMarginNoAuto);
  }
  // set maximum width if requested
  if (aMet.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
    aMet.mMaximumWidth = GetMaxWidth(aCaptionSide, aInnerMarginNoAuto, aCaptionMarginNoAuto);
  }

}

nsresult 
nsTableOuterFrame::IncrementalReflow(nsIPresContext*          aPresContext,
                                     nsHTMLReflowMetrics&     aDesiredSize,
                                     const nsHTMLReflowState& aReflowState,
                                     nsReflowStatus&          aStatus)
{
  nsresult  rv = NS_OK;

  // determine if this frame is the target or not
  nsIFrame* target=nsnull;
  rv = aReflowState.reflowCommand->GetTarget(target);
  if (NS_SUCCEEDED(rv) && target) {
    if (this == target) {
      rv = IR_TargetIsMe(aPresContext, aDesiredSize, aReflowState, aStatus);
    }
    else {
      // Get the next frame in the reflow chain
      nsIFrame* nextFrame;
      aReflowState.reflowCommand->GetNext(nextFrame);
      NS_ASSERTION(nextFrame, "next frame in reflow command is null"); 
      rv = IR_TargetIsChild(aPresContext, aDesiredSize, aReflowState, aStatus, nextFrame);
    }
  }
  return rv;
}

nsresult 
nsTableOuterFrame::IR_TargetIsChild(nsIPresContext*          aPresContext,
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
    const nsStyleDisplay* nextDisplay;
    aNextFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)nextDisplay);
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
nsTableOuterFrame::IR_TargetIsInnerTableFrame(nsIPresContext*           aPresContext,
                                              nsHTMLReflowMetrics&      aDesiredSize,
                                              const nsHTMLReflowState&  aReflowState,
                                              nsReflowStatus&           aStatus)
{
  nsresult rv = IR_InnerTableReflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  return rv;
}

nsresult 
nsTableOuterFrame::IR_TargetIsCaptionFrame(nsIPresContext*           aPresContext,
                                           nsHTMLReflowMetrics&      aDesiredSize,
                                           const nsHTMLReflowState&  aOuterRS,
                                           nsReflowStatus&           aStatus)
{
  nsresult rv = NS_OK;
  aStatus = NS_FRAME_COMPLETE;
  PRUint8 captionSide = GetCaptionSide();

  nsSize captionSize, captionMES;
  nsMargin captionMargin, captionMarginNoAuto, captionPadding;
  // reflow the caption frame, getting it's MES
  nscoord availWidth = GetCaptionAvailWidth(aPresContext, mCaptionFrame, aOuterRS);
  nsHTMLReflowMetrics captionMet(&captionMES);
  nsReflowStatus capStatus; // don't let the caption cause incomplete
  OuterReflowChild(aPresContext, mCaptionFrame, aOuterRS, captionMet, &availWidth, captionSize, 
                   captionMargin, captionMarginNoAuto, captionPadding, eReflowReason_Incremental, capStatus);

  nsMargin innerMargin, innerMarginNoAuto, innerPadding;
  nsPoint  innerOrigin;
  nsSize   containSize = GetContainingBlockSize(aOuterRS);

  // for now just reflow the table if a style changed. This should be improved
  nsIReflowCommand::ReflowType reflowCommandType;
  aOuterRS.reflowCommand->GetType(reflowCommandType);
  PRBool needInnerReflow = (nsIReflowCommand::StyleChanged == reflowCommandType)
                            ? PR_TRUE : PR_FALSE;

  if (mMinCaptionWidth != captionMES.width) {  
    // set the new caption min width, and set state to reflow the inner table if necessary
    mMinCaptionWidth = captionMES.width;
    // see if the captions min width could cause the table to be wider
    // XXX this really only affects an auto width table
    if ((mMinCaptionWidth + captionMarginNoAuto.left + captionMarginNoAuto.right) > mRect.width) {
      needInnerReflow = PR_TRUE;
    }
  }

  nsRect prevInnerRect;
  mInnerTableFrame->GetRect(prevInnerRect);
  nsPoint captionOrigin;
  if (needInnerReflow) {
    nsSize innerSize;
    nsHTMLReflowMetrics innerMet(nsnull);
    OuterReflowChild(aPresContext, mInnerTableFrame, aOuterRS, innerMet, nsnull, innerSize, 
                     innerMargin, innerMarginNoAuto, innerPadding, eReflowReason_Resize, aStatus);

    GetInnerOrigin(aPresContext, captionSide, containSize, captionSize,
                   captionMargin, innerSize, innerMargin, innerOrigin);
    rv = FinishReflowChild(mInnerTableFrame, aPresContext, innerMet,
                           innerOrigin.x, innerOrigin.y, 0);
    if (NS_FAILED(rv)) return rv;
    
    GetCaptionOrigin(aPresContext, captionSide, containSize, innerSize, 
                     innerMargin, captionSize, captionMargin, captionOrigin);
  }
  else {
    // reposition the inner frame if necessary and set the caption's origin
    nsSize innerSize = GetFrameSize(*mInnerTableFrame);
    GetMarginPadding(aPresContext, aOuterRS, mInnerTableFrame, innerMargin, 
                     innerMarginNoAuto, innerPadding);
    GetInnerOrigin(aPresContext, captionSide, containSize, captionSize, 
                   captionMargin, innerSize, innerMargin, innerOrigin);
    GetCaptionOrigin(aPresContext, captionSide, containSize, innerSize, 
                     innerMargin, captionSize, captionMargin, captionOrigin);
    MoveFrameTo(aPresContext, mInnerTableFrame, innerOrigin.x, innerOrigin.y); 
  }

  rv = FinishReflowChild(mCaptionFrame, aPresContext, captionMet,
                         captionOrigin.x, captionOrigin.y, 0);

  UpdateReflowMetrics(captionSide, aDesiredSize, innerMargin, innerMarginNoAuto, 
                      innerPadding, captionMargin, captionMarginNoAuto);
  nsSize desSize(aDesiredSize.width, aDesiredSize.height);
  PRBool innerMoved = (innerOrigin.x != prevInnerRect.x) || (innerOrigin.y != prevInnerRect.y);
  InvalidateDamage(aPresContext, captionSide, desSize, innerMoved, PR_TRUE);
  return rv;
}

nsresult
nsTableOuterFrame::IR_ReflowDirty(nsIPresContext*           aPresContext,
                                  nsHTMLReflowMetrics&      aDesiredSize,
                                  const nsHTMLReflowState&  aReflowState,
                                  nsReflowStatus&           aStatus)
{
  nsFrameState  frameState;
  nsresult      rv = NS_OK;
  PRBool sizeSet = PR_FALSE;
  // See if the caption frame is dirty. This would be because of a newly
  // inserted caption
  if (mCaptionFrame) {
    mCaptionFrame->GetFrameState(&frameState);
    if (frameState & NS_FRAME_IS_DIRTY) {
      rv = IR_CaptionInserted(aPresContext, aDesiredSize, aReflowState, aStatus);
      sizeSet = PR_TRUE;
    }
  }

  // See if the inner table frame is dirty
  mInnerTableFrame->GetFrameState(&frameState);
  if (frameState & NS_FRAME_IS_DIRTY) {
    rv = IR_InnerTableReflow(aPresContext, aDesiredSize, aReflowState, aStatus);
    sizeSet = PR_TRUE;
  } 
  else if (!mCaptionFrame) {
    // The inner table isn't dirty so we don't need to reflow it, but make
    // sure it's placed correctly. It could be that we're dirty because the
    // caption was removed
    nsRect   innerRect;
    mInnerTableFrame->GetRect(innerRect);
    nsSize   innerSize(innerRect.width, innerRect.height);
    nsPoint  innerOrigin;
    nsMargin innerMargin, innerMarginNoAuto, innerPadding;
    GetMarginPadding(aPresContext, aReflowState, mInnerTableFrame, innerMargin, 
                     innerMarginNoAuto, innerPadding);
    nsSize containSize = GetContainingBlockSize(aReflowState);
    GetInnerOrigin(aPresContext, NO_SIDE, containSize, nsSize(0,0),
                   nsMargin(0,0,0,0), innerSize, innerMargin, innerOrigin);
    MoveFrameTo(aPresContext, mInnerTableFrame, innerOrigin.x, innerOrigin.y); 

    aDesiredSize.width  = innerRect.XMost() + innerMargin.right;
    aDesiredSize.height = innerRect.YMost() + innerMargin.bottom; 
    sizeSet = PR_TRUE;
    // Repaint the inner's entire bounds if it moved
    if ((innerRect.x != innerOrigin.x) || (innerRect.y != innerOrigin.y)) {
      Invalidate(aPresContext, nsRect(0, 0, aDesiredSize.width, aDesiredSize.height));
    }
  }
  if (!sizeSet) {
    // set our desired size to what it was before
    nsSize size = GetFrameSize(*this);
    aDesiredSize.width  = size.width;
    aDesiredSize.height = size.height;
  }

  return rv;
}

// IR_TargetIsMe is free to foward the request to the inner table frame 
nsresult nsTableOuterFrame::IR_TargetIsMe(nsIPresContext*           aPresContext,
                                          nsHTMLReflowMetrics&      aDesiredSize,
                                          const nsHTMLReflowState&  aReflowState,
                                          nsReflowStatus&           aStatus)
{
  nsresult rv = NS_OK;
  nsIReflowCommand::ReflowType type;
  aReflowState.reflowCommand->GetType(type);
  nsIFrame* objectFrame;
  aReflowState.reflowCommand->GetChildFrame(objectFrame); 
  switch (type) {
  case nsIReflowCommand::ReflowDirty:
     rv = IR_ReflowDirty(aPresContext, aDesiredSize, aReflowState, aStatus);
    break;

  case nsIReflowCommand::StyleChanged :    
  case nsIReflowCommand::Timeout :    
    rv = IR_InnerTableReflow(aPresContext, aDesiredSize, aReflowState, aStatus);
    break;

  case nsIReflowCommand::ContentChanged :
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
nsTableOuterFrame::IR_InnerTableReflow(nsIPresContext*           aPresContext,
                                       nsHTMLReflowMetrics&      aOuterMet,
                                       const nsHTMLReflowState&  aOuterRS,
                                       nsReflowStatus&           aStatus)
{
  aStatus = NS_FRAME_COMPLETE;
  PRUint8 captionSide = GetCaptionSide();

  nsRect priorInnerRect;
  mInnerTableFrame->GetRect(priorInnerRect);

  nsSize   innerSize;
  nsMargin innerMargin, innerMarginNoAuto, innerPadding;

  // pass along the reflow command to the inner table, requesting the same info in our flags
  nsHTMLReflowMetrics innerMet(aOuterMet.maxElementSize, aOuterMet.mFlags);

  nsresult rv = OuterReflowChild(aPresContext, mInnerTableFrame, aOuterRS, innerMet,
                                 nsnull, innerSize, innerMargin, innerMarginNoAuto, innerPadding,  
                                 eReflowReason_Incremental, aStatus);
  if (NS_FAILED(rv)) return rv;

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
    nsRect prevCaptionRect;
    mCaptionFrame->GetRect(prevCaptionRect);

    if (priorInnerRect.width != innerMet.width) {
      nsMargin ignorePadding;
      // XXX only need to reflow if the caption is auto width
      nsHTMLReflowMetrics captionMet(nsnull);  // don't ask for MES, it hasn't changed
      nscoord availWidth = GetCaptionAvailWidth(aPresContext, mCaptionFrame, aOuterRS,
                                                &innerSize.width, &innerMarginNoAuto);
      nsReflowStatus capStatus; // don't let the caption cause incomplete
      rv = OuterReflowChild(aPresContext, mCaptionFrame, aOuterRS, captionMet, &availWidth,
                            captionSize, captionMargin, captionMarginNoAuto, 
                            ignorePadding, eReflowReason_Resize, capStatus);
      if (NS_FAILED(rv)) return rv;

      GetCaptionOrigin(aPresContext, captionSide, containSize, innerSize, 
                       innerMargin, captionSize, captionMargin, captionOrigin);
      FinishReflowChild(mCaptionFrame, aPresContext, captionMet,
                        captionOrigin.x, captionOrigin.y, 0);

      GetInnerOrigin(aPresContext, captionSide, containSize, captionSize, 
                     captionMargin, innerSize, innerMargin, innerOrigin);
    }
    else {
      // reposition the caption frame if necessary and set the inner's origin
      captionSize = GetFrameSize(*mCaptionFrame);
      nsMargin captionPadding;
      GetMarginPadding(aPresContext, aOuterRS, mCaptionFrame, captionMargin, 
                       captionMarginNoAuto, captionPadding);
      GetCaptionOrigin(aPresContext, captionSide, containSize, innerSize, 
                       innerMargin, captionSize, captionMargin, captionOrigin);
      GetInnerOrigin(aPresContext, captionSide, containSize, captionSize, 
                     captionMargin, innerSize, innerMargin, innerOrigin);
      MoveFrameTo(aPresContext, mCaptionFrame, captionOrigin.x, captionOrigin.y); 
    }
    if ((captionOrigin.x != prevCaptionRect.x) || (captionOrigin.x != prevCaptionRect.x)) {
      captionMoved = PR_TRUE;
    }
    if ((captionOrigin.x != prevCaptionRect.x) || (captionOrigin.x != prevCaptionRect.x)) {
      captionMoved = PR_TRUE;
    }
  }
  else {
    GetInnerOrigin(aPresContext, captionSide, containSize, captionSize, 
                   captionMargin, innerSize, innerMargin, innerOrigin);
  }

  FinishReflowChild(mInnerTableFrame, aPresContext, innerMet,
                    innerOrigin.x, innerOrigin.y, 0);

  UpdateReflowMetrics(captionSide, aOuterMet, innerMargin, innerMarginNoAuto, 
                      innerPadding, captionMargin, captionMarginNoAuto);
  nsSize desSize(aOuterMet.width, aOuterMet.height);
  InvalidateDamage(aPresContext, captionSide, desSize, (innerSize.width != priorInnerRect.width), captionMoved);

  return rv;
}

/* the only difference between an insert and a replace is a replace 
   checks the old maxElementSize and reflows the table only if it
   has changed
*/
nsresult 
nsTableOuterFrame::IR_CaptionInserted(nsIPresContext*           aPresContext,
                                      nsHTMLReflowMetrics&      aDesiredSize,
                                      const nsHTMLReflowState&  aOuterRS,
                                      nsReflowStatus&           aStatus)
{
  PRUint8 captionSide = GetCaptionSide();
  aStatus = NS_FRAME_COMPLETE;

  // reflow the caption frame, getting it's MES
  nsSize   captionSize;
  nsMargin captionMargin, captionMarginNoAuto, ignorePadding;
  nsSize maxElementSize(0,0);
  nsHTMLReflowMetrics captionMet(&maxElementSize);
  // reflow the caption
  nscoord availWidth = GetCaptionAvailWidth(aPresContext, mCaptionFrame, aOuterRS);
  nsReflowStatus capStatus; // don't let the caption cause incomplete
  nsresult rv = OuterReflowChild(aPresContext, mCaptionFrame, aOuterRS, captionMet,
                                 &availWidth, captionSize, captionMargin, captionMarginNoAuto,
                                 ignorePadding, eReflowReason_Initial, capStatus);

  if (NS_FAILED(rv)) return rv;

  mMinCaptionWidth = maxElementSize.width;

  nsPoint  captionOrigin(0,0); 

  nsMargin innerMargin, innerMarginNoAuto, innerPadding;
  nsPoint innerOrigin;
  nsSize containSize = GetContainingBlockSize(aOuterRS);
  nsRect prevInnerRect;
  mInnerTableFrame->GetRect(prevInnerRect);
  // XXX: caption align = left|right ignored here!
  // if the caption's MES + margins > outer width, reflow the inner table
  if (mMinCaptionWidth + captionMarginNoAuto.left + captionMarginNoAuto.right > mRect.width) {
    nsHTMLReflowMetrics innerMet(aDesiredSize.maxElementSize); 
    nsSize innerSize;

    rv = OuterReflowChild(aPresContext, mInnerTableFrame, aOuterRS, innerMet,
                          nsnull, innerSize, innerMargin, innerMarginNoAuto,
                          innerPadding, eReflowReason_Resize, aStatus);
    if (NS_FAILED(rv)) return rv;

    GetInnerOrigin(aPresContext, captionSide, containSize, captionSize, 
                   captionMargin, innerSize, innerMargin, innerOrigin);
    rv = FinishReflowChild(mInnerTableFrame, aPresContext, innerMet,
                           innerOrigin.x, innerOrigin.y, 0);
    if (NS_FAILED(rv)) return rv;
    GetCaptionOrigin(aPresContext, captionSide, containSize, innerSize, 
                     innerMargin, captionSize, captionMargin, captionOrigin);
  }
  else {
    // reposition the inner frame if necessary and set the caption's origin
    nsSize innerSize = GetFrameSize(*mInnerTableFrame);
    GetMarginPadding(aPresContext, aOuterRS, mInnerTableFrame, innerMargin, 
                     innerMarginNoAuto, innerPadding);
    GetInnerOrigin(aPresContext, captionSide, containSize, captionSize, 
                   captionMargin, innerSize, innerMargin, innerOrigin);
    GetCaptionOrigin(aPresContext, captionSide, containSize, innerSize, 
                     innerMargin, captionSize, captionMargin, captionOrigin);
    MoveFrameTo(aPresContext, mInnerTableFrame, innerOrigin.x, innerOrigin.y); 
  }

  rv = FinishReflowChild(mCaptionFrame, aPresContext, captionMet,
                         captionOrigin.x, captionOrigin.y, 0);

  UpdateReflowMetrics(captionSide, aDesiredSize, innerMargin, innerMarginNoAuto, 
                      innerPadding, captionMargin, captionMarginNoAuto);
  nsSize desSize(aDesiredSize.width, aDesiredSize.height);
  PRBool innerMoved = (innerOrigin.x != prevInnerRect.x) || (innerOrigin.y != prevInnerRect.y);
  InvalidateDamage(aPresContext, captionSide, desSize, innerMoved, PR_TRUE);

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
    nsCOMPtr<nsIStyleContext> styleContext;
    aFrame->GetStyleContext(getter_AddRefs(styleContext)); 
    nsStylePosition* position = (nsStylePosition*)styleContext->GetStyleData(eStyleStruct_Position);
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
NS_METHOD nsTableOuterFrame::Reflow(nsIPresContext*          aPresContext,
                                    nsHTMLReflowMetrics&     aDesiredSize,
                                    const nsHTMLReflowState& aOuterRS,
                                    nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableOuterFrame", aOuterRS.reason);
#if defined DEBUG_TABLE_REFLOW | DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflow(this, (nsHTMLReflowState&)aOuterRS);
#endif

  nsresult rv = NS_OK;
  PRUint8 captionSide = GetCaptionSide();

  // Initialize out parameters
  aDesiredSize.width = aDesiredSize.height = 0;
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width =  0;
    aDesiredSize.maxElementSize->height = 0;
  }
  aStatus = NS_FRAME_COMPLETE;

  PRBool isPaginated;
  aPresContext->IsPaginated(&isPaginated);
  PRBool needUpdateMetrics = PR_TRUE;

  if ((eReflowReason_Resize    == aOuterRS.reason)  &&
      (aOuterRS.availableWidth == mPriorAvailWidth) &&
      !isPaginated                                  &&
      !::IsPctHeight(mInnerTableFrame)) {
    // don't do much if we are resize reflowed exactly like last time
    aDesiredSize.width  = mRect.width;
    aDesiredSize.height = mRect.height;

    // We know our view (if we have one) has been positioned
    // correctly, but it's up to us to make sure that our child views
    // are correctly positioned, too.
    nsContainerFrame::PositionChildViews(aPresContext, this);
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
        nsSize maxElementSize;
        nsHTMLReflowMetrics captionMet(&maxElementSize);
        captionMet.maxElementSize = &maxElementSize;
        nsHTMLReflowState captionReflowState(aPresContext, aOuterRS, mCaptionFrame,
                                             nsSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE),
                                             eReflowReason_Initial);

        mCaptionFrame->WillReflow(aPresContext);
        rv = mCaptionFrame->Reflow(aPresContext, captionMet, captionReflowState, aStatus);
        mCaptionFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
        mMinCaptionWidth = maxElementSize.width;
      }
    }
    // At this point, we must have an inner table frame, and we might have a caption
    NS_ASSERTION(mFrames.NotEmpty() && mInnerTableFrame, "incomplete children");
    nsSize   innerSize;
    nsMargin innerMargin, innerMarginNoAuto, innerPadding;

    // First reflow the inner table
    nsHTMLReflowMetrics innerMet(aDesiredSize.maxElementSize);
    rv = OuterReflowChild(aPresContext, mInnerTableFrame, aOuterRS, innerMet,
                          nsnull, innerSize, innerMargin, innerMarginNoAuto,
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
      nscoord availWidth = GetCaptionAvailWidth(aPresContext, mCaptionFrame, aOuterRS,
                                                &innerSize.width, &innerMarginNoAuto);
      nsHTMLReflowMetrics captionMet(nsnull);
      nsReflowStatus capStatus; // don't let the caption cause incomplete
      rv = OuterReflowChild(aPresContext, mCaptionFrame, aOuterRS, captionMet, 
                            &availWidth, captionSize, captionMargin, captionMarginNoAuto,
                            ignorePadding, aOuterRS.reason, capStatus);
      if (NS_FAILED(rv)) return rv;

      nsPoint captionOrigin;

      GetCaptionOrigin(aPresContext, captionSide, containSize, innerSize, 
                       innerMargin, captionSize, captionMargin, captionOrigin);
      FinishReflowChild(mCaptionFrame, aPresContext, captionMet,
                        captionOrigin.x, captionOrigin.y, 0);

      GetInnerOrigin(aPresContext, captionSide, containSize, captionSize, 
                     captionMargin, innerSize, innerMargin, innerOrigin);

      // XXX If the height is constrained then we need to check whether the inner table still fits...
    } 
    else {
      GetInnerOrigin(aPresContext, captionSide, containSize, captionSize, 
                     captionMargin, innerSize, innerMargin, innerOrigin);
    }

    FinishReflowChild(mInnerTableFrame, aPresContext, innerMet,
                      innerOrigin.x, innerOrigin.y, 0);

    UpdateReflowMetrics(captionSide, aDesiredSize, innerMargin, innerMarginNoAuto, 
                        innerPadding, captionMargin, captionMarginNoAuto);
    needUpdateMetrics = PR_FALSE;
  }
  
  // Return our desired rect
  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;

  // compute max element size and maximum width if it hasn't already been 
  if (needUpdateMetrics) {
    nsMargin innerMargin, innerMarginNoAuto, capMargin(0,0,0,0), 
             capMarginNoAuto(0,0,0,0), innerPadding, capPadding(0,0,0,0);
    GetMarginPadding(aPresContext, aOuterRS, mInnerTableFrame, 
                     innerMargin, innerMarginNoAuto, innerPadding);
    if (mCaptionFrame) {
      GetMarginPadding(aPresContext, aOuterRS, mCaptionFrame, 
                       capMargin, capMarginNoAuto, capPadding);
    }
    UpdateReflowMetrics(captionSide, aDesiredSize, innerMargin, innerMarginNoAuto, 
                        innerPadding, capMargin, capMarginNoAuto);
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

#if defined DEBUG_TABLE_REFLOW | DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflow(this, (nsHTMLReflowState&)aOuterRS, &aDesiredSize, aStatus);
#endif
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
void nsTableOuterFrame::DeleteChildsNextInFlow(nsIPresContext* aPresContext, 
                                               nsIFrame*       aChild)
{
  NS_PRECONDITION(mFrames.ContainsFrame(aChild), "bad geometric parent");

  nsIFrame* nextInFlow;
   
  aChild->GetNextInFlow(&nextInFlow);

  NS_PRECONDITION(nsnull != nextInFlow, "null next-in-flow");
  nsTableOuterFrame* parent;
   
  nextInFlow->GetParent((nsIFrame**)&parent);

  // If the next-in-flow has a next-in-flow then delete it too (and
  // delete it first).
  nsIFrame* nextNextInFlow;

  nextInFlow->GetNextInFlow(&nextNextInFlow);
  if (nsnull != nextNextInFlow) {
    parent->DeleteChildsNextInFlow(aPresContext, nextInFlow);
  }

  // Disconnect the next-in-flow from the flow list
  nsSplittableFrame::BreakFromPrevFlow(nextInFlow);

  // Take the next-in-flow out of the parent's child list
  if (parent->mFrames.FirstChild() == nextInFlow) {
    nsIFrame* nextSibling;
    nextInFlow->GetNextSibling(&nextSibling);
    parent->mFrames.SetFrames(nextSibling);

  } else {
    nsIFrame* nextSibling;

    // Because the next-in-flow is not the first child of the parent
    // we know that it shares a parent with aChild. Therefore, we need
    // to capture the next-in-flow's next sibling (in case the
    // next-in-flow is the last next-in-flow for aChild AND the
    // next-in-flow is not the last child in parent)
    aChild->GetNextSibling(&nextSibling);
    NS_ASSERTION(nextSibling == nextInFlow, "unexpected sibling");

    nextInFlow->GetNextSibling(&nextSibling);
    aChild->SetNextSibling(nextSibling);
  }

  // Delete the next-in-flow frame and adjust it's parent's child count
  nextInFlow->Destroy(aPresContext);

#ifdef NS_DEBUG
  aChild->GetNextInFlow(&nextInFlow);
  NS_POSTCONDITION(nsnull == nextInFlow, "non null next-in-flow");
#endif
}

NS_IMETHODIMP
nsTableOuterFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::tableOuterFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
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
  nsresult result = mInnerTableFrame->QueryInterface(NS_GET_IID(nsITableLayout), (void **)&inner);
  if (NS_SUCCEEDED(result) && inner)
  {
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
  nsresult result = mInnerTableFrame->QueryInterface(NS_GET_IID(nsITableLayout), (void **)&inner);
  if (NS_SUCCEEDED(result) && inner)
  {
    return (inner->GetTableSize(aRowCount, aColCount));
  }
  return NS_ERROR_NULL_POINTER;
}

/*---------------- end of nsITableLayout implementation ------------------*/


nsresult 
NS_NewTableOuterFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTableOuterFrame* it = new (aPresShell) nsTableOuterFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsTableOuterFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("TableOuter", aResult);
}

NS_IMETHODIMP
nsTableOuterFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  PRUint32 sum = sizeof(*this);
  *aResult = sum;
  return NS_OK;
}
#endif
