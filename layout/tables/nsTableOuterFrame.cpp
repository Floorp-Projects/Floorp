/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
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

/* ----------- nsTableCaptionFrame ---------- */

#define NS_TABLE_FRAME_CAPTION_LIST_INDEX 0

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

// OuterTableReflowState

struct OuterTableReflowState {

  // The presentation context
  nsIPresContext *pc;

  // Our reflow state
  const nsHTMLReflowState& reflowState;

  // The total available size (computed from the parent)
  nsSize availSize;
  // The available size for the inner table frame
  nsSize innerTableMaxSize;

  // Margin tracking information
  nscoord prevMaxPosBottomMargin;
  nscoord prevMaxNegBottomMargin;

  // Flags for whether the max size is unconstrained
  PRBool  unconstrainedWidth;
  PRBool  unconstrainedHeight;

  // Running y-offset
  nscoord y;

  OuterTableReflowState(nsIPresContext*          aPresContext,
                        const nsHTMLReflowState& aReflowState)
    : reflowState(aReflowState)
  {
    pc = aPresContext;
    availSize.width = reflowState.availableWidth;
    availSize.height = reflowState.availableHeight;
    prevMaxPosBottomMargin = 0;
    prevMaxNegBottomMargin = 0;
    y=0;  // border/padding/margin???
    unconstrainedWidth = PRBool(aReflowState.availableWidth == NS_UNCONSTRAINEDSIZE);
    unconstrainedHeight = PRBool(aReflowState.availableHeight == NS_UNCONSTRAINEDSIZE);
    innerTableMaxSize.width=0;
    innerTableMaxSize.height=0;
  }

  ~OuterTableReflowState() {
  }
};

/* ----------- nsTableOuterFrame ---------- */

NS_IMPL_ADDREF_INHERITED(nsTableOuterFrame, nsHTMLContainerFrame)
NS_IMPL_RELEASE_INHERITED(nsTableOuterFrame, nsHTMLContainerFrame)

nsTableOuterFrame::nsTableOuterFrame()
{
}

nsTableOuterFrame::~nsTableOuterFrame()
{
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
  } else {
    return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
  }
}

// tables change 0 width into auto, trees override this and do nothing
NS_IMETHODIMP
nsTableOuterFrame::AdjustZeroWidth()
{
  // a 0 width table becomes auto
  PRBool makeAuto = PR_FALSE;
  nsStylePosition* position = (nsStylePosition*)mStyleContext->GetMutableStyleData(eStyleStruct_Position);
  if (position->mWidth.GetUnit() == eStyleUnit_Coord) {
    if (0 >= position->mWidth.GetCoordValue()) {
      makeAuto= PR_TRUE;
    }
  }
  else if (position->mWidth.GetUnit() == eStyleUnit_Percent) {
    if (0.0f >= position->mWidth.GetPercentValue()) {
      makeAuto= PR_TRUE;
    }
  }

  if (makeAuto) {
    position->mWidth = nsStyleCoord(eStyleUnit_Auto);
  }
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

NS_METHOD nsTableOuterFrame::Paint(nsIPresContext*      aPresContext,
                                   nsIRenderingContext& aRenderingContext,
                                   const nsRect&        aDirtyRect,
                                   nsFramePaintLayer    aWhichLayer)
{
#ifdef DEBUG
  // for debug...
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(255,0,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
#endif

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

PRBool nsTableOuterFrame::NeedsReflow(const nsHTMLReflowState& aReflowState)
{
  PRBool result=PR_TRUE;
  if (nsnull != mInnerTableFrame) {
    result = ((nsTableFrame *)mInnerTableFrame)->NeedsReflow(aReflowState);
  }
  return result;
}

// Recover the reflow state to what it should be if aKidFrame is about
// to be reflowed
nsresult nsTableOuterFrame::RecoverState(OuterTableReflowState& aReflowState,
                                         nsIFrame*              aKidFrame)
{
  aReflowState.y = 0;

  // Set the inner table max size
  nsSize  innerTableSize(0,0);

  mInnerTableFrame->GetSize(innerTableSize);
  aReflowState.innerTableMaxSize.width = innerTableSize.width;
  aReflowState.innerTableMaxSize.height = aReflowState.reflowState.availableHeight;

  return NS_OK;
}

nsresult nsTableOuterFrame::IncrementalReflow(nsIPresContext*        aPresContext,
                                              nsHTMLReflowMetrics&   aDesiredSize,
                                              OuterTableReflowState& aReflowState,
                                              nsReflowStatus&        aStatus)
{
  nsresult  rv = NS_OK;

  // determine if this frame is the target or not
  nsIFrame* target=nsnull;
  rv = aReflowState.reflowState.reflowCommand->GetTarget(target);
  if ((PR_TRUE == NS_SUCCEEDED(rv)) && (nsnull != target)) {
    if (this == target) {
      rv = IR_TargetIsMe(aPresContext, aDesiredSize, aReflowState, aStatus);
    }
    else {
      // Get the next frame in the reflow chain
      nsIFrame* nextFrame;
      aReflowState.reflowState.reflowCommand->GetNext(nextFrame);
      NS_ASSERTION(nextFrame, "next frame in reflow command is null"); 

      // Recover our reflow state
      RecoverState(aReflowState, nextFrame);
      rv = IR_TargetIsChild(aPresContext, aDesiredSize, aReflowState, aStatus, nextFrame);
    }
  }
  return rv;
}

nsresult nsTableOuterFrame::IR_TargetIsChild(nsIPresContext*        aPresContext,
                                             nsHTMLReflowMetrics&   aDesiredSize,
                                             OuterTableReflowState& aReflowState,
                                             nsReflowStatus&        aStatus,
                                             nsIFrame*              aNextFrame)
{
  nsresult rv;
  if (!aNextFrame) {
    // this will force Reflow to return the height of the last reflow rather than 0
    aReflowState.y = mRect.height; 
    return NS_OK;
  }

  if (aNextFrame == mInnerTableFrame) {
    rv = IR_TargetIsInnerTableFrame(aPresContext, aDesiredSize, aReflowState, aStatus);
  }
  else if (aNextFrame==mCaptionFrame) {
    rv = IR_TargetIsCaptionFrame(aPresContext, aDesiredSize, aReflowState, aStatus);

    // If we're supposed to update our maximum width, then just use the inner
    // table's maximum width
    if (aDesiredSize.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
      aDesiredSize.mMaximumWidth = mInnerTableMaximumWidth;
    }
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

nsresult nsTableOuterFrame::IR_TargetIsInnerTableFrame(nsIPresContext*        aPresContext,
                                                       nsHTMLReflowMetrics&   aDesiredSize,
                                                       OuterTableReflowState& aReflowState,
                                                       nsReflowStatus&        aStatus)
{
  nsresult rv = IR_InnerTableReflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  return rv;
}

nsresult nsTableOuterFrame::IR_TargetIsCaptionFrame(nsIPresContext*        aPresContext,
                                                    nsHTMLReflowMetrics&   aDesiredSize,
                                                    OuterTableReflowState& aReflowState,
                                                    nsReflowStatus&        aStatus)
{
  nsresult rv;
  PRBool innerTableNeedsReflow = PR_FALSE;
  // remember the old width and height
  nsRect priorCaptionRect;
  mCaptionFrame->GetRect(priorCaptionRect);
  // if the reflow type is a style change, also remember the prior style
  nsIReflowCommand::ReflowType reflowCommandType;
  aReflowState.reflowState.reflowCommand->GetType(reflowCommandType);
  const nsStyleTable* priorCaptionTableStyle = nsnull;
  if (nsIReflowCommand::StyleChanged == reflowCommandType) {
    mCaptionFrame->GetStyleData(eStyleStruct_Table, ((const nsStyleStruct *&)priorCaptionTableStyle));
  }

  // pass along the reflow command to the caption
  nsSize captionMES(0,0);
  nsHTMLReflowMetrics captionSize(&captionMES);
  nsHTMLReflowState captionReflowState(aPresContext, aReflowState.reflowState, mCaptionFrame,
                                       nsSize(mRect.width, aReflowState.reflowState.availableHeight),
                                       aReflowState.reflowState.reason);
  captionReflowState.reflowCommand = aReflowState.reflowState.reflowCommand;

  rv = ReflowChild(mCaptionFrame, aPresContext, captionSize, captionReflowState,
                   0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
  mCaptionFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (mMinCaptionWidth != captionMES.width) {  
    // set the new caption min width, and set state to reflow the inner table if necessary
    mMinCaptionWidth = captionMES.width;
    if (mMinCaptionWidth > mRect.width) {
      innerTableNeedsReflow=PR_TRUE;
    }
  }

  // check if the caption alignment changed axis
  if (nsIReflowCommand::StyleChanged == reflowCommandType) {
    const nsStyleTable* captionTableStyle;
    mCaptionFrame->GetStyleData(eStyleStruct_Table, ((const nsStyleStruct *&)captionTableStyle));
    if (PR_TRUE == IR_CaptionChangedAxis(priorCaptionTableStyle, captionTableStyle)) {
      innerTableNeedsReflow=PR_TRUE;
    }
  }

  // if we've determined that the inner table needs to be reflowed, do it here
  nsSize innerTableSize;
  if (PR_TRUE == innerTableNeedsReflow) {
    // Compute the width to use for the table. In the case of an auto sizing
    // table this represents the maximum available width
    nscoord tableWidth = ((nsTableFrame*)mInnerTableFrame)->CalcBorderBoxWidth(aReflowState.reflowState);

    // If the caption max element size is larger, then use it instead.
    // XXX: caption align = left|right ignored here!
    if (mMinCaptionWidth > tableWidth) {
      tableWidth = mMinCaptionWidth;
    }
    nsHTMLReflowMetrics innerSize(aDesiredSize.maxElementSize); 
    nsHTMLReflowState innerReflowState(aPresContext, aReflowState.reflowState, mInnerTableFrame, 
                                       nsSize(tableWidth, aReflowState.reflowState.availableHeight),
                                       eReflowReason_Resize);
    rv = ReflowChild(mInnerTableFrame, aPresContext, innerSize, innerReflowState,
                    0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
    mInnerTableFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
    if (NS_FAILED(rv)) {
      return rv;
    }
    innerTableSize.SizeTo(innerSize.width, innerSize.height);
    // set maxElementSize width if requested
    if (nsnull != aDesiredSize.maxElementSize) {
      ((nsTableFrame *)mInnerTableFrame)->SetMaxElementSize(aDesiredSize.maxElementSize,
                                                            aReflowState.reflowState.mComputedPadding);
      if (mMinCaptionWidth > aDesiredSize.maxElementSize->width) {
        aDesiredSize.maxElementSize->width = mMinCaptionWidth;
      }
    }
  }
  else {
    nsRect innerTableRect;
    mInnerTableFrame->GetRect(innerTableRect);
    innerTableSize.SizeTo(innerTableRect.width, innerTableRect.height);
  }

  // regardless of what we've done up to this point, place the caption and inner table
  rv = SizeAndPlaceChildren(aPresContext, innerTableSize, 
                            nsSize (captionSize.width, captionSize.height), 
                            aReflowState, captionReflowState.mComputedMargin);

  return rv;
}

nsresult
nsTableOuterFrame::IR_ReflowDirty(nsIPresContext*        aPresContext,
                                  nsHTMLReflowMetrics&   aDesiredSize,
                                  OuterTableReflowState& aReflowState,
                                  nsReflowStatus&        aStatus)
{
  nsFrameState  frameState;
  nsresult      rv;

  // See if the caption frame is dirty. This would be because of a newly
  // inserted caption
  if (mCaptionFrame) {
    mCaptionFrame->GetFrameState(&frameState);
    if (frameState & NS_FRAME_IS_DIRTY) {
      rv = IR_CaptionInserted(aPresContext, aDesiredSize, aReflowState, aStatus);

      // Repaint our entire bounds
      // XXX Improve this...
      Invalidate(aPresContext, nsRect(0, 0, mRect.width, mRect.height));
    }
  }

  // See if the inner table frame is dirty
  mInnerTableFrame->GetFrameState(&frameState);
  if (frameState & NS_FRAME_IS_DIRTY) {
    // Inner table is dirty so reflow it. Change the reflow state and set the
    // reason to resize reflow.
    ((nsHTMLReflowState&)aReflowState.reflowState).reason = eReflowReason_Resize;
    ((nsHTMLReflowState&)aReflowState.reflowState).reflowCommand = nsnull;

    // Get the inner table frame's current bounds. We'll use that when
    // repainting it
    // XXX It should really do the repainting, but because it think it's
    // getting a resize reflow it won't know to...
    nsRect  dirtyRect;
    mInnerTableFrame->GetRect(dirtyRect);
    rv = IR_InnerTableReflow(aPresContext, aDesiredSize, aReflowState, aStatus);

    // Repaint the inner table frame's entire visible area
    dirtyRect.x = dirtyRect.y = 0;
    Invalidate(aPresContext, dirtyRect);

  } else if (!mCaptionFrame) {
    // The inner table isn't dirty so we don't need to reflow it, but make
    // sure it's placed correctly. It could be that we're dirty because the
    // caption was removed
    mInnerTableFrame->MoveTo(aPresContext, 0,0);

    // Update our state so we calculate our desired size correctly
    nsRect  innerRect;
    mInnerTableFrame->GetRect(innerRect);
    aReflowState.innerTableMaxSize.width = innerRect.width;
    aReflowState.y = innerRect.height;
    
    // Repaint our entire bounds
    // XXX Improve this...
    Invalidate(aPresContext, nsRect(0, 0, mRect.width, mRect.height));
  }

  return rv;
}

// IR_TargetIsMe is free to foward the request to the inner table frame 
nsresult nsTableOuterFrame::IR_TargetIsMe(nsIPresContext*        aPresContext,
                                          nsHTMLReflowMetrics&   aDesiredSize,
                                          OuterTableReflowState& aReflowState,
                                          nsReflowStatus&        aStatus)
{
  nsresult rv = NS_OK;
  nsIReflowCommand::ReflowType type;
  aReflowState.reflowState.reflowCommand->GetType(type);
  nsIFrame* objectFrame;
  aReflowState.reflowState.reflowCommand->GetChildFrame(objectFrame); 
  switch (type) {
  case nsIReflowCommand::ReflowDirty:
     rv = IR_ReflowDirty(aPresContext, aDesiredSize, aReflowState, aStatus);
    break;

  case nsIReflowCommand::StyleChanged :    
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

nsresult nsTableOuterFrame::IR_InnerTableReflow(nsIPresContext*        aPresContext,
                                                nsHTMLReflowMetrics&   aDesiredSize,
                                                OuterTableReflowState& aReflowState,
                                                nsReflowStatus&        aStatus)
{
  nsresult rv = NS_OK;
  const nsStyleTable* captionTableStyle=nsnull;
  // remember the old width and height
  nsRect priorInnerTableRect;
  mInnerTableFrame->GetRect(priorInnerTableRect);
  // pass along the reflow command to the inner table
  nsHTMLReflowMetrics innerSize(aDesiredSize.maxElementSize);
  nscoord tableMaxWidth = PR_MAX(aReflowState.reflowState.availableWidth, mMinCaptionWidth);
  nsHTMLReflowState innerReflowState(aPresContext, aReflowState.reflowState, mInnerTableFrame,
                                     nsSize(tableMaxWidth, aReflowState.reflowState.availableHeight));
  
  // When the above reflow state is constructed, mComputedWidth and mComputedHeight get set
  // to the table's styled width and height.  Flex from XUL boxes can make the styled #s
  // inaccurate, which means that mComputedWidth and mComputedHeight from the parent
  // reflow state should be used instead.
  
  // The following function will patch the reflow state so that trees behave properly inside boxes. 
  // This might work for tables as well, but until regression tests can be run to make sure,
  // I'm holding off on patching tables.
  FixBadReflowState(aReflowState.reflowState, innerReflowState);

  // Always request the maximum width if we are an auto layout table
  if (((nsTableFrame*)mInnerTableFrame)->IsAutoLayout()) {
    innerSize.mFlags |= NS_REFLOW_CALC_MAX_WIDTH;
  }
  rv = ReflowChild(mInnerTableFrame, aPresContext, innerSize, innerReflowState,
                   0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
  mInnerTableFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
  if (aDesiredSize.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
    aDesiredSize.mMaximumWidth = innerSize.mMaximumWidth;

    // Update the cached value
    mInnerTableMaximumWidth = innerSize.mMaximumWidth;
  }

  nsMargin captionMargin(0,0,0,0);
  // if there is a caption and the width or height of the inner table changed from a successful reflow, 
  // then reflow or move the caption as needed
  if ((nsnull != mCaptionFrame) && NS_SUCCEEDED(rv)) {
    // remember the old caption height
    nsRect oldCaptionRect;
    mCaptionFrame->GetRect(oldCaptionRect);
    nsHTMLReflowMetrics captionSize(nsnull);  // don't ask for MES, it hasn't changed
    PRBool captionDimChanged  = PR_FALSE;
    PRBool captionWasReflowed = PR_FALSE;
    if (priorInnerTableRect.width != innerSize.width) { 
      // the table width changed, so reflow the caption
      nsHTMLReflowState captionReflowState(aPresContext, aReflowState.reflowState, mCaptionFrame,
                                           nsSize(innerSize.width, aReflowState.reflowState.availableHeight),
                                           eReflowReason_Resize);
      // reflow the caption
      mCaptionFrame->WillReflow(aPresContext);
      rv = mCaptionFrame->Reflow(aPresContext, captionSize, captionReflowState, aStatus);
      mCaptionFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
      captionWasReflowed = PR_TRUE;
      if ((oldCaptionRect.height != captionSize.height) || 
          (oldCaptionRect.width != captionSize.width)) {
        captionDimChanged = PR_TRUE;
      }
      captionMargin = captionReflowState.mComputedMargin;
    }
    // XXX: should just call SizeAndPlaceChildren regardless
    // find where to place the caption
    mCaptionFrame->GetStyleData(eStyleStruct_Text, ((const nsStyleStruct *&)captionTableStyle));
    if ((priorInnerTableRect.height != innerSize.height) || captionDimChanged) {
      if (!captionWasReflowed) { // get the computed margin by constructing a reflow state
        nsHTMLReflowState rState(aPresContext, aReflowState.reflowState, mCaptionFrame,
                                 nsSize(innerSize.width, aReflowState.reflowState.availableHeight),
                                 eReflowReason_Resize);
        captionMargin = rState.mComputedMargin;
      }
      // Compute the caption's y-origin
      nscoord captionY = captionMargin.top;
      if (NS_SIDE_BOTTOM == captionTableStyle->mCaptionSide) {
        captionY += innerSize.height;
      }
      // Place the caption
      nsRect captionRect(captionMargin.left, captionY, 0, 0);
      if (PR_TRUE==captionWasReflowed) {
        captionRect.SizeTo(captionSize.width, captionSize.height);
      }
      else {
        captionRect.SizeTo(oldCaptionRect.width, oldCaptionRect.height);
      }
      mCaptionFrame->SetRect(aPresContext, captionRect);
    }
  }

  // if anything above failed, we just want to return an error at this point
  if (NS_FAILED(rv)) {
    return rv;
  }
  // Place the inner table
  nsRect updatedCaptionRect(0,0,0,0);
  if (nsnull != mCaptionFrame) {
    mCaptionFrame->GetRect(updatedCaptionRect);
  }
  nscoord innerY;   // innerY is the y-offset of the inner table
  if (nsnull != mCaptionFrame) { 
    // factor in caption and it's margin
    // we're guaranteed that captionMargin and captionTableStyle are set at this point
    if (NS_SIDE_BOTTOM != captionTableStyle->mCaptionSide) { 
      // top caption
      innerY = 0;   // the inner table goes at the top of the outer table
      // the total v-space consumed is the inner table height + the caption height + the margin between them
      aReflowState.y = innerSize.height + updatedCaptionRect.YMost() + captionMargin.top; 
    } 
    else { // bottom caption
      innerY = updatedCaptionRect.YMost() + captionMargin.bottom;
      // the total v-space consumed is the inner table height + the caption height + the margin between them
      aReflowState.y = innerY + innerSize.height;
    }
  }
  else { // no caption
    innerY=0;
    aReflowState.y = innerSize.height;
  }
  nsRect innerRect(0, innerY, innerSize.width, innerSize.height);
  mInnerTableFrame->SetRect(aPresContext, innerRect);

  aReflowState.innerTableMaxSize.width = innerSize.width;
  return rv;
}



/* the only difference between an insert and a replace is a replace 
   checks the old maxElementSize and reflows the table only if it
   has changed
*/
nsresult nsTableOuterFrame::IR_CaptionInserted(nsIPresContext*        aPresContext,
                                               nsHTMLReflowMetrics&   aDesiredSize,
                                               OuterTableReflowState& aReflowState,
                                               nsReflowStatus&        aStatus)
{  
  nsresult rv = NS_OK;

  // reflow the caption frame, getting it's MES
  nsSize              maxElementSize;
  nsHTMLReflowMetrics captionSize(&maxElementSize);
  nsHTMLReflowState   captionReflowState(aPresContext, aReflowState.reflowState, mCaptionFrame,
                                         nsSize(mRect.width, aReflowState.reflowState.availableHeight),
                                         eReflowReason_Initial);
  // initial reflow of the caption
  mCaptionFrame->WillReflow(aPresContext);
  rv = mCaptionFrame->Reflow(aPresContext, captionSize, captionReflowState, aStatus);
  mMinCaptionWidth = maxElementSize.width;
  // XXX: caption align = left|right ignored here!
  // if the caption's MES > table width, reflow the inner table
  nsHTMLReflowMetrics innerSize(aDesiredSize.maxElementSize); 
  if (mMinCaptionWidth > mRect.width) {
    nsHTMLReflowState innerReflowState(aPresContext, aReflowState.reflowState, mInnerTableFrame,
                                       nsSize(mMinCaptionWidth, aReflowState.reflowState.availableHeight),
                                       eReflowReason_Resize);
    rv = ReflowChild(mInnerTableFrame, aPresContext, innerSize, innerReflowState,
                     0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
    mInnerTableFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
  }
  else { // set innerSize as if the inner table were reflowed
    innerSize.height = mRect.height;
    innerSize.width  = mRect.width;
  }
  // set maxElementSize width if requested
  if (nsnull != aDesiredSize.maxElementSize) {
    ((nsTableFrame *)mInnerTableFrame)->SetMaxElementSize(aDesiredSize.maxElementSize,
                                                          aReflowState.reflowState.mComputedPadding);
    if (mMinCaptionWidth > aDesiredSize.maxElementSize->width) {
      aDesiredSize.maxElementSize->width = mMinCaptionWidth;
    }
  }

  rv = SizeAndPlaceChildren(aPresContext,
                            nsSize (innerSize.width, innerSize.height), 
                            nsSize (captionSize.width, captionSize.height), 
                            aReflowState, captionReflowState.mComputedMargin);
  return rv;
}

PRBool nsTableOuterFrame::IR_CaptionChangedAxis(const nsStyleTable* aOldStyle, 
                                                const nsStyleTable* aNewStyle) const
{
  PRBool result = PR_FALSE;
  //XXX: write me to support left|right captions!
  return result;
}

nsresult nsTableOuterFrame::SizeAndPlaceChildren(nsIPresContext*        aPresContext,
                                                 const nsSize&          aInnerSize, 
                                                 const nsSize&          aCaptionSize,
                                                 OuterTableReflowState& aReflowState,
                                                 const nsMargin&        aCaptionMargin)
{
  nsresult rv = NS_OK;
  // find where to place the caption
  // Compute the caption's y-origin
  nscoord captionY = aCaptionMargin.top;
  const nsStyleTable* captionTableStyle;
  mCaptionFrame->GetStyleData(eStyleStruct_Table, ((const nsStyleStruct *&)captionTableStyle));
  if (NS_SIDE_BOTTOM == captionTableStyle->mCaptionSide) {
    captionY += aInnerSize.height;
  }
  // Place the caption
  nsRect captionRect(aCaptionMargin.left, captionY, 0, 0);
  captionRect.SizeTo(aCaptionSize.width, aCaptionSize.height);
  mCaptionFrame->SetRect(aPresContext, captionRect);

  // Place the inner table
  nscoord innerY;
  if (NS_SIDE_BOTTOM == captionTableStyle->mCaptionSide) { 
    // bottom caption
    innerY = 0;
    aReflowState.y = captionRect.YMost() + aCaptionMargin.bottom;
  } 
  else { // top caption
    innerY = captionRect.YMost() + aCaptionMargin.bottom;
    aReflowState.y = innerY + aInnerSize.height;
  }
  nsRect innerRect(0, innerY, aInnerSize.width, aInnerSize.height);
  mInnerTableFrame->SetRect(aPresContext, innerRect);
  aReflowState.innerTableMaxSize.width = aInnerSize.width;
  return rv;
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
                                    const nsHTMLReflowState& aReflowState,
                                    nsReflowStatus&          aStatus)
{
  if (nsDebugTable::gRflTableOuter) nsTableFrame::DebugReflow("TO::Rfl en", this, &aReflowState, nsnull);
  nsresult rv = NS_OK;
  // Initialize out parameters
  aDesiredSize.width  = 0;
  aDesiredSize.height = 0;
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }
  aStatus = NS_FRAME_COMPLETE;

  nsHTMLReflowMetrics captionSize(nsnull);

  // Initialize our local reflow state
  OuterTableReflowState state(aPresContext, aReflowState);
  if (eReflowReason_Incremental == aReflowState.reason) {
    rv = IncrementalReflow(aPresContext, aDesiredSize, state, aStatus);
  } else {
    if (eReflowReason_Initial == aReflowState.reason) {
      // Set up our kids.  They're already present, on an overflow list, 
      // or there are none so we'll create them now
      MoveOverflowToChildList(aPresContext);

      // Lay out the caption and get its maximum element size
      if (nsnull != mCaptionFrame) {
        nsSize maxElementSize;
        captionSize.maxElementSize = &maxElementSize;
        nsHTMLReflowState captionReflowState(aPresContext, aReflowState, mCaptionFrame,
                                             nsSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE),
                                             eReflowReason_Initial);

        mCaptionFrame->WillReflow(aPresContext);
        rv = mCaptionFrame->Reflow(aPresContext, captionSize, captionReflowState, aStatus);
        mCaptionFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
        mMinCaptionWidth = maxElementSize.width;
        captionSize.maxElementSize = nsnull;
      }
    }

    // At this point, we must have an inner table frame, and we might have a caption
    NS_ASSERTION(mFrames.NotEmpty(), "no children");
    NS_ASSERTION(nsnull != mInnerTableFrame, "no mInnerTableFrame");

    nscoord availWidth = aReflowState.availableWidth;

    // If the caption max element size is larger, then use it instead.
    // XXX: caption align = left|right ignored here!
    if (mMinCaptionWidth > availWidth) {
      availWidth = mMinCaptionWidth;
    }

    // First reflow the inner table
    nsHTMLReflowState innerReflowState(aPresContext, aReflowState, mInnerTableFrame, 
                                       nsSize(availWidth, aReflowState.availableHeight));
    innerReflowState.mComputedWidth  = PR_MAX(aReflowState.mComputedWidth, mMinCaptionWidth);
    innerReflowState.mComputedHeight = aReflowState.mComputedHeight;

    nsHTMLReflowMetrics innerSize(aDesiredSize.maxElementSize); 
    // XXX To do this efficiently we really need to know where the inner
    // table will be placed. In the case of a top caption that means
    // reflowing the caption first and getting its desired height...
    rv = ReflowChild(mInnerTableFrame, aPresContext, innerSize, innerReflowState,
                     0, 0, 0, aStatus);

    if (NS_UNCONSTRAINEDSIZE == innerReflowState.availableWidth) {
      // Remember the inner table's maximum width
      mInnerTableMaximumWidth = innerSize.width;
    }

    // Table's max element size is the MAX of the caption's max element size
    // and the inner table's max element size...
    if (nsnull != aDesiredSize.maxElementSize) {
      if (mMinCaptionWidth > aDesiredSize.maxElementSize->width) {
        aDesiredSize.maxElementSize->width = mMinCaptionWidth;
      }
    }
    state.innerTableMaxSize.width = innerSize.width;

    // Now that we know the table width we can reflow the caption, and
    // place the caption and the inner table
    nscoord innerY = 0;
    if (nsnull != mCaptionFrame) {
      // construct the caption reflow state
      nscoord captionAvailWidth = PR_MAX(innerSize.width, mMinCaptionWidth);
      nsHTMLReflowState captionReflowState(aPresContext, state.reflowState, mCaptionFrame,
                                           nsSize(captionAvailWidth, NS_UNCONSTRAINEDSIZE),
                                           eReflowReason_Resize);
      // Compute the caption's y-origin
      nscoord captionY = (NS_AUTOMARGIN == captionReflowState.mComputedMargin.top)
                         ? 0 : captionReflowState.mComputedMargin.top;
      const nsStyleTable* captionTableStyle;
      mCaptionFrame->GetStyleData(eStyleStruct_Table, ((const nsStyleStruct *&)captionTableStyle));
      if (NS_SIDE_BOTTOM == captionTableStyle->mCaptionSide) {
        captionY += innerSize.height;
      }

      // Reflow the caption. Let it be as high as it wants
      nscoord leftCapMargin = (NS_AUTOMARGIN == captionReflowState.mComputedMargin.left)
                              ? 0 : captionReflowState.mComputedMargin.left;
      nsRect captionRect(leftCapMargin, captionY, 0, 0);
      nsReflowStatus captionStatus;
      captionSize.maxElementSize = nsnull;
      ReflowChild(mCaptionFrame, aPresContext, captionSize, captionReflowState,
                  captionRect.x, captionRect.y, 0, captionStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(captionStatus), "unexpected reflow status");

      // XXX If the height is constrained then we need to check whether the inner
      // table still fits...

      // Place the caption
      captionRect.SizeTo(captionSize.width, captionSize.height);
      FinishReflowChild(mCaptionFrame, aPresContext, captionSize,
                        captionRect.x, captionRect.y, 0);

      // Place the inner table
      nscoord bottomCapMargin = (NS_AUTOMARGIN == captionReflowState.mComputedMargin.bottom)
                                ? 0 : captionReflowState.mComputedMargin.bottom;
      if (NS_SIDE_BOTTOM != captionTableStyle->mCaptionSide) {
        // top caption
        innerY = captionRect.YMost() + bottomCapMargin;
        state.y = innerY + innerSize.height;
      } 
      else {
        // bottom caption
        innerY  = 0;
        state.y = captionRect.YMost() + bottomCapMargin;
      }

    } 
    else {
      // Place the inner table at 0
      innerY = 0;
      state.y = innerSize.height;
    }

    // Finish the inner table reflow
    FinishReflowChild(mInnerTableFrame, aPresContext, innerSize,
                      0, innerY, 0);
  }
  
  // Return our desired rect
  aDesiredSize.width   = PR_MAX(state.innerTableMaxSize.width, captionSize.width);
  aDesiredSize.height  = state.y;
  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;

  if (nsDebugTable::gRflTableOuter) nsTableFrame::DebugReflow("TO::Rfl ex", this, nsnull, &aDesiredSize, aStatus);
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
