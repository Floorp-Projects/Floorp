/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsTiming = PR_FALSE;
static PRBool gsDebugIR = PR_FALSE;
//#define NOISY
//#define NOISY_FLOW
#define NOISY_MARGINS
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsTiming = PR_FALSE;
static const PRBool gsDebugIR = PR_FALSE;
#endif

// these macros are defined to improve the readability of the main code
// XXX they need to be put in an include file and shared 
#define TDBG_S(typ,str) \
  if (typ) { \
    printf((str)); \
  }

#define TDBG_SP(typ,str,ptr) \
  if (typ) { \
    printf((str),(ptr)); \
  }

#define TDBG_SPD(typ,str,ptr,dec) \
  if (typ) { \
    printf((str),(ptr),(dec)); \
  }

#define TDBG_SPDD(typ,str,ptr,dec1,dec2) \
  if (typ) { \
    printf((str),(ptr),(dec1),(dec2)); \
  }

#define TDBG_SPDDDD(typ,str,ptr,dec1,dec2,dec3,dec4) \
  if (typ) { \
    printf((str),(ptr),(dec1),(dec2),(dec3),(dec4)); \
  }

#define TDBG_SFDD(typ,str,flt,dec1,dec2) \
  if (typ) { \
    printf((str),(flt),(dec1),(dec2)); \
  }

#define TDBG_SDF(typ,str,dec,flt) \
  if (typ) { \
    printf((str),(dec),(flt)); \
  }

#define TDBG_SDFDDD(typ,str,dec1,flt,dec2,dec3,dec4) \
  if (typ) { \
    printf((str),(dec1),(flt),(dec2),(dec3),(dec4)); \
  }

#define TDBG_SD(typ,str,dec) \
  if (typ) { \
    printf((str),(dec)); \
  }

#define TDBG_SDD(typ,str,dec1,dec2) \
  if (typ) { \
    printf((str),(dec1),(dec2)); \
  }

#define TDBG_SDDD(typ,str,dec1,dec2,dec3) \
  if (typ) { \
    printf((str),(dec1),(dec2),(dec3)); \
  }

#define TDBG_SDFD(typ,str,dec1,flt,dec2) \
  if (typ) { \
    printf((str),(dec1),(flt),(dec2)); \
  }

#define TDBG_SDDDD(typ,str,dec1,dec2,dec3,dec4) \
  if (typ) { \
    printf((str),(dec1),(dec2),(dec3),(dec4)); \
  }

#define TDBG_SDDDF(typ,str,dec1,dec2,dec3,flt) \
  if (typ) { \
    printf((str),(dec1),(dec2),(dec3),(flt)); \
  }

#define TDBG_SDDDDD(typ,str,dec1,dec2,dec3,dec4,dec5) \
  if (typ) { \
    printf((str),(dec1),(dec2),(dec3),(dec4),(dec5)); \
  }

#define TDBG_SDDDDDD(typ,str,dec1,dec2,dec3,dec4,dec5,dec6) \
  if (typ) { \
    printf((str),(dec1),(dec2),(dec3),(dec4),(dec5),(dec6)); \
  }


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

  OuterTableReflowState(nsIPresContext&          aPresContext,
                        const nsHTMLReflowState& aReflowState)
    : reflowState(aReflowState)
  {
    pc = &aPresContext;
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



NS_IMETHODIMP nsTableOuterFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                                     nsIAtom*        aListName,
                                                     nsIFrame*       aChildList)
{
  mFrames.SetFrames(aChildList);

  // Set our internal member data
  mInnerTableFrame = aChildList;
  //XXX this should go through the child list looking for a displaytype==caption
  if (1 < mFrames.GetLength()) {
    nsIFrame *child;
    nsresult result = aChildList->GetNextSibling(&child);
    while ((NS_SUCCEEDED(result)) && (nsnull!=child)) {
      const nsStyleDisplay* childDisplay;
      child->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)childDisplay);
      if (NS_STYLE_DISPLAY_TABLE_CAPTION==childDisplay->mDisplay) {
        mCaptionFrame = child;
        break;
      }
      result = child->GetNextSibling(&child);
    }
  }

  return NS_OK;
}

NS_METHOD nsTableOuterFrame::Paint(nsIPresContext&      aPresContext,
                                   nsIRenderingContext& aRenderingContext,
                                   const nsRect&        aDirtyRect,
                                   nsFramePaintLayer    aWhichLayer)
{
  // for debug...
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(255,0,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  return NS_OK;
}

PRBool nsTableOuterFrame::NeedsReflow(const nsHTMLReflowState& aReflowState, const nsSize& aMaxSize)
{
  PRBool result=PR_TRUE;
  if (nsnull != mInnerTableFrame) {
    result = ((nsTableFrame *)mInnerTableFrame)->NeedsReflow(aReflowState, aMaxSize);
  }
  return result;
}

// Recover the reflow state to what it should be if aKidFrame is about
// to be reflowed
nsresult nsTableOuterFrame::RecoverState(OuterTableReflowState& aReflowState,
                                         nsIFrame*              aKidFrame)
{
#if 0
  // Get aKidFrame's previous sibling
  nsIFrame* prevKidFrame = nsnull;
  for (nsIFrame* frame = mFrames.FirstChild(); frame != aKidFrame;) {
    prevKidFrame = frame;
    frame->GetNextSibling(frame);
  }

  if (nsnull != prevKidFrame) {
    nsRect  rect;

    // Set our running y-offset
    prevKidFrame->GetRect(rect);
    aReflowState.y = rect.YMost();

    // Adjust the available space
    if (PR_FALSE == aReflowState.unconstrainedHeight) {
      aReflowState.availSize.height -= aReflowState.y;
    }

    // Get the previous frame's bottom margin
    const nsStyleSpacing* kidSpacing;
    prevKidFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)kidSpacing);
    nsMargin margin;
    kidSpacing->CalcMarginFor(prevKidFrame, margin);
    if (margin.bottom < 0) {
      aReflowState.prevMaxNegBottomMargin = -margin.bottom;
    } else {
      aReflowState.prevMaxPosBottomMargin = margin.bottom;
    }
  }
#endif

  aReflowState.y = 0;

  // Set the inner table max size
  nsSize  innerTableSize(0,0);

  mInnerTableFrame->GetSize(innerTableSize);
  aReflowState.innerTableMaxSize.width = innerTableSize.width;
  aReflowState.innerTableMaxSize.height = aReflowState.reflowState.availableHeight;

  return NS_OK;
}

nsresult nsTableOuterFrame::IncrementalReflow(nsIPresContext&        aPresContext,
                                              nsHTMLReflowMetrics&   aDesiredSize,
                                              OuterTableReflowState& aReflowState,
                                              nsReflowStatus&        aStatus)
{
  TDBG_S(gsDebugIR,"\nTOF IR: IncrementalReflow\n");
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

      // Recover our reflow state
      RecoverState(aReflowState, nextFrame);
      rv = IR_TargetIsChild(aPresContext, aDesiredSize, aReflowState, aStatus, nextFrame);
    }
  }
  return rv;
}

nsresult nsTableOuterFrame::IR_TargetIsChild(nsIPresContext&        aPresContext,
                                             nsHTMLReflowMetrics&   aDesiredSize,
                                             OuterTableReflowState& aReflowState,
                                             nsReflowStatus&        aStatus,
                                             nsIFrame*              aNextFrame)
{
  TDBG_S(gsDebugIR,"TOF IR: IR_TargetIsChild\n");
  nsresult rv;
  if (aNextFrame == mInnerTableFrame) {
    rv = IR_TargetIsInnerTableFrame(aPresContext, aDesiredSize, aReflowState, aStatus);
  }
  else if (aNextFrame==mCaptionFrame) {
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

nsresult nsTableOuterFrame::IR_TargetIsInnerTableFrame(nsIPresContext&        aPresContext,
                                                       nsHTMLReflowMetrics&   aDesiredSize,
                                                       OuterTableReflowState& aReflowState,
                                                       nsReflowStatus&        aStatus)
{
  TDBG_S(gsDebugIR,"TOF IR: IR_TargetIsInnerTableFrame\n");
  nsresult rv = IR_InnerTableReflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  return rv;
}

nsresult nsTableOuterFrame::IR_TargetIsCaptionFrame(nsIPresContext&        aPresContext,
                                                    nsHTMLReflowMetrics&   aDesiredSize,
                                                    OuterTableReflowState& aReflowState,
                                                    nsReflowStatus&        aStatus)
{
  nsresult rv;
  TDBG_S(gsDebugIR,"TOF IR: IR_TargetIsCaptionFrame\n");
  PRBool innerTableNeedsReflow = PR_FALSE;
  // remember the old width and height
  nsRect priorCaptionRect;
  mCaptionFrame->GetRect(priorCaptionRect);
  // if the reflow type is a style change, also remember the prior style
  nsIReflowCommand::ReflowType reflowCommandType;
  aReflowState.reflowState.reflowCommand->GetType(reflowCommandType);
  const nsStyleText* priorCaptionTextStyle = nsnull;
  if (nsIReflowCommand::StyleChanged == reflowCommandType) {
    mCaptionFrame->GetStyleData(eStyleStruct_Text, ((const nsStyleStruct *&)priorCaptionTextStyle));
  }
  TDBG_SDDD(gsDebugIR,"TOF IR: prior caption width=%d height=%d, minWidth=%d\n", 
            priorCaptionRect.width, priorCaptionRect.height, mMinCaptionWidth);

  // pass along the reflow command to the caption
  TDBG_S(gsDebugIR,"TOF IR: passing down incremental reflow command to caption.\n");
  nsSize captionMES(0,0);
  nsHTMLReflowMetrics captionSize(&captionMES);
  nsHTMLReflowState captionReflowState(aPresContext, aReflowState.reflowState, mCaptionFrame,
                                       nsSize(mRect.width, aReflowState.reflowState.availableHeight),
                                       aReflowState.reflowState.reason);
  captionReflowState.reflowCommand = aReflowState.reflowState.reflowCommand;
  rv = ReflowChild(mCaptionFrame, aPresContext, captionSize, captionReflowState, aStatus);
  TDBG_SDDDD(gsDebugIR,"TOF IR: caption reflow returned %d with width=%d height=%d, minCaptionWidth=%d\n", 
             rv, captionSize.width, captionSize.height, captionMES.width);
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
    const nsStyleText* captionTextStyle;
    mCaptionFrame->GetStyleData(eStyleStruct_Text, ((const nsStyleStruct *&)captionTextStyle));
    if (PR_TRUE == IR_CaptionChangedAxis(priorCaptionTextStyle, captionTextStyle)) {
      innerTableNeedsReflow=PR_TRUE;
    }
  }

  // if we've determined that the inner table needs to be reflowed, do it here
  nsSize innerTableSize;
  if (PR_TRUE == innerTableNeedsReflow) {
    TDBG_S(gsDebugIR,"inner table needs resize reflow.\n");
    // Compute the width to use for the table. In the case of an auto sizing
    // table this represents the maximum available width
    nscoord tableWidth = ComputeAvailableTableWidth(aReflowState.reflowState);

    // If the caption max element size is larger, then use it instead.
    // XXX: caption align = left|right ignored here!
    if (mMinCaptionWidth > tableWidth) {
      tableWidth = mMinCaptionWidth;
    }
    nsHTMLReflowMetrics innerSize(aDesiredSize.maxElementSize); 
    nsHTMLReflowState innerReflowState(aPresContext, aReflowState.reflowState, mInnerTableFrame, 
                                       nsSize(tableWidth, aReflowState.reflowState.availableHeight),
                                       eReflowReason_Resize);
    rv = ReflowChild(mInnerTableFrame, aPresContext, innerSize, innerReflowState, aStatus);
    if (NS_FAILED(rv)) {
      return rv;
    }
    innerTableSize.SizeTo(innerSize.width, innerSize.height);
    // set maxElementSize width if requested
    if (nsnull != aDesiredSize.maxElementSize) {
      ((nsTableFrame *)mInnerTableFrame)->SetMaxElementSize(aDesiredSize.maxElementSize);
      if (mMinCaptionWidth > aDesiredSize.maxElementSize->width) {
        aDesiredSize.maxElementSize->width = mMinCaptionWidth;
      }
    }
  }
  else {
    TDBG_S(gsDebugIR,"skipping inner table resize reflow.\n");
    nsRect innerTableRect;
    mInnerTableFrame->GetRect(innerTableRect);
    innerTableSize.SizeTo(innerTableRect.width, innerTableRect.height);
  }

  // regardless of what we've done up to this point, place the caption and inner table
  rv = SizeAndPlaceChildren(innerTableSize, 
                            nsSize (captionSize.width, captionSize.height), 
                            aReflowState);

  return rv;
}

// IR_TargetIsMe is free to foward the request to the inner table frame 
nsresult nsTableOuterFrame::IR_TargetIsMe(nsIPresContext&        aPresContext,
                                          nsHTMLReflowMetrics&   aDesiredSize,
                                          OuterTableReflowState& aReflowState,
                                          nsReflowStatus&        aStatus)
{
  nsresult rv = NS_OK;
  nsIReflowCommand::ReflowType type;
  aReflowState.reflowState.reflowCommand->GetType(type);
  nsIFrame* objectFrame;
  aReflowState.reflowState.reflowCommand->GetChildFrame(objectFrame); 
  TDBG_SD(gsDebugIR,"TOF IR: IncrementalReflow_TargetIsMe with type=%d\n", type);
  switch (type) {
  case nsIReflowCommand::FrameAppended :
  case nsIReflowCommand::FrameInserted :
  {
    const nsStyleDisplay* childDisplay;
    objectFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_CAPTION == childDisplay->mDisplay) {
      rv = IR_CaptionInserted(aPresContext, aDesiredSize, aReflowState, aStatus, 
                              objectFrame, PR_FALSE);
    }
    else {
      TDBG_S(gsDebugIR,"TOF IR: calling inner table reflow.\n");
      rv = IR_InnerTableReflow(aPresContext, aDesiredSize, aReflowState, aStatus);
    }
    break;
  }

  /*
  case nsIReflowCommand::FrameReplaced :
    // if the frame to be replaced is mCaptionFrame
    if (mCaptionFrame==objectFrame)
    {
      // verify that the new frame is a caption frame
      const nsStyleDisplay *newFrameDisplay;
      newFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)newFrameDisplay));
      NS_ASSERTION(NS_STYLE_DISPLAY_TABLE_CAPTION == childDisplay->mDisplay, "bad new frame");
      if (NS_STYLE_DISPLAY_TABLE_CAPTION == childDisplay->mDisplay)
      {
        rv = IR_CaptionInserted(aPresContext, aDesiredSize, aReflowState, aStatus, 
                             objectFrame, PR_TRUE);
      }
      else
        rv = NS_ERROR_ILLEGAL_VALUE;
    }
    else
    {
      if (PR_TRUE==gsDebugIR) printf("TOF IR: calling inner table reflow.\n");
      rv = IR_InnerTableReflow(aPresContext, aDesiredSize, aReflowState, aStatus);
    }
  */

  case nsIReflowCommand::FrameRemoved :
    if (mCaptionFrame==objectFrame) {
      rv = IR_CaptionRemoved(aPresContext, aDesiredSize, aReflowState, aStatus);
    }
    else {
      TDBG_S(gsDebugIR,"TOF IR: calling inner table reflow.\n");
      rv = IR_InnerTableReflow(aPresContext, aDesiredSize, aReflowState, aStatus);
    }
    break;

  case nsIReflowCommand::StyleChanged :
    TDBG_S(gsDebugIR,"TOF IR: calling inner table reflow.\n");
    rv = IR_InnerTableReflow(aPresContext, aDesiredSize, aReflowState, aStatus);
    break;

  case nsIReflowCommand::ContentChanged :
    NS_ASSERTION(PR_FALSE, "illegal reflow type: ContentChanged");
    rv = NS_ERROR_ILLEGAL_VALUE;
    break;
  
  case nsIReflowCommand::PullupReflow:
  case nsIReflowCommand::PushReflow:
  case nsIReflowCommand::CheckPullupReflow :
  case nsIReflowCommand::UserDefined :
  default:
    NS_NOTYETIMPLEMENTED("unimplemented reflow command type");
    rv = NS_ERROR_NOT_IMPLEMENTED;
    TDBG_S(gsDebugIR,"TOF IR: reflow command not implemented.\n");
    break;
  }

  return rv;
}

nsresult nsTableOuterFrame::IR_InnerTableReflow(nsIPresContext&        aPresContext,
                                                nsHTMLReflowMetrics&   aDesiredSize,
                                                OuterTableReflowState& aReflowState,
                                                nsReflowStatus&        aStatus)
{
  TDBG_S(gsDebugIR,"TOF IR: IR_InnerTableReflow\n");
  nsresult rv = NS_OK;
  const nsStyleText* captionTextStyle=nsnull;
  nsMargin captionMargin;
  // remember the old width and height
  nsRect priorInnerTableRect;
  mInnerTableFrame->GetRect(priorInnerTableRect);
  TDBG_SDD(gsDebugIR,"TOF IR: prior width=%d height=%d\n", priorInnerTableRect.width, priorInnerTableRect.height);
  // pass along the reflow command to the inner table
  TDBG_S(gsDebugIR,"TOF IR: passing down incremental reflow command to inner table.\n");
  nsHTMLReflowMetrics innerSize(aDesiredSize.maxElementSize);
  nscoord tableMaxWidth = PR_MAX(aReflowState.reflowState.availableWidth, mMinCaptionWidth);
  TDBG_SDD(gsDebugIR,"TOF IR: mincaptionWidth=%d, tableMaxWidth=%d.\n", mMinCaptionWidth, tableMaxWidth);
  nsHTMLReflowState innerReflowState(aPresContext, aReflowState.reflowState, mInnerTableFrame,
                                     nsSize(tableMaxWidth, aReflowState.reflowState.availableHeight));
  rv = ReflowChild(mInnerTableFrame, aPresContext, innerSize, innerReflowState, aStatus);
  TDBG_SDDD(gsDebugIR,"TOF IR: inner table reflow returned %d with width=%d height=%d\n", rv, innerSize.width, innerSize.height);
  // if there is a caption and the width or height of the inner table changed from a successful reflow, 
  // then reflow or move the caption as needed
  if ((nsnull != mCaptionFrame) && (PR_TRUE==NS_SUCCEEDED(rv))) {
    // remember the old caption height
    nsRect oldCaptionRect;
    mCaptionFrame->GetRect(oldCaptionRect);
    TDBG_SD(gsDebugIR,"TOF IR: prior caption height = %d\n", oldCaptionRect.height);
    nsHTMLReflowMetrics captionSize(nsnull);  // don't ask for MES, it hasn't changed
    PRBool captionDimChanged  = PR_FALSE;
    PRBool captionWasReflowed = PR_FALSE;
    if (priorInnerTableRect.width != innerSize.width) { 
      // the table width changed, so reflow the caption
      nsHTMLReflowState captionReflowState(aPresContext, aReflowState.reflowState, mCaptionFrame,
                                           nsSize(innerSize.width, aReflowState.reflowState.availableHeight),
                                           eReflowReason_Resize);
      nsIHTMLReflow* htmlReflow;
      if (NS_OK == mCaptionFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) { 
        // reflow the caption
        TDBG_S(gsDebugIR,"TOF IR: table width changed, resize-reflowing caption.\n");
        htmlReflow->WillReflow(aPresContext);
        rv = htmlReflow->Reflow(aPresContext, captionSize, captionReflowState, aStatus);
        captionWasReflowed = PR_TRUE;
        TDBG_SDDD(gsDebugIR,"TOF IR: caption reflow returned %d with width=%d height=%d\n", rv, captionSize.width, captionSize.height);
        if ((oldCaptionRect.height!=captionSize.height) || 
            (oldCaptionRect.width!=captionSize.width)) {
          captionDimChanged=PR_TRUE;
        }
      }
    }
    else {
      TDBG_S(gsDebugIR,"TOF IR: skipping caption reflow\n");
    }
    TDBG_SD(gsDebugIR,"TOF IR: captionDimChanged = %d\n", captionDimChanged);
    // XXX: should just call SizeAndPlaceChildren regardless
    // find where to place the caption
    const nsStyleSpacing* spacing;
    mCaptionFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
    spacing->CalcMarginFor(mCaptionFrame, captionMargin);
    mCaptionFrame->GetStyleData(eStyleStruct_Text, ((const nsStyleStruct *&)captionTextStyle));
    if ((priorInnerTableRect.height!=innerSize.height) || 
        (PR_TRUE==captionDimChanged)) {
      // Compute the caption's y-origin
      nscoord captionY = captionMargin.top;
      if ((captionTextStyle->mVerticalAlign.GetUnit()==eStyleUnit_Enumerated) && 
          (captionTextStyle->mVerticalAlign.GetIntValue()==NS_STYLE_VERTICAL_ALIGN_BOTTOM)) {
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
      mCaptionFrame->SetRect(captionRect);
      TDBG_SDDDD(gsDebugIR,"    TOF IR: captionRect=%d %d %d %d\n", captionRect.x, captionRect.y, captionRect.width, captionRect.height);
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
    // we're guaranteed that captionMargin and captionTextStyle are set at this point
    if ((captionTextStyle->mVerticalAlign.GetUnit()     == eStyleUnit_Enumerated) &&
        (captionTextStyle->mVerticalAlign.GetIntValue() == NS_STYLE_VERTICAL_ALIGN_BOTTOM)) { 
      // bottom caption
      innerY = 0;   // the inner table goes at the top of the outer table
      // the total v-space consumed is the inner table height + the caption height + the margin between them
      aReflowState.y = innerSize.height + updatedCaptionRect.YMost() + captionMargin.top; 
    } 
    else { // top caption
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
  mInnerTableFrame->SetRect(innerRect);
  TDBG_SDDDD(gsDebugIR, "    TOF IR: innerRect=%d %d %d %d\n", innerRect.x, innerRect.y, innerRect.width, innerRect.height);

  aReflowState.innerTableMaxSize.width = innerSize.width;
  return rv;
}



/* the only difference between an insert and a replace is a replace 
   checks the old maxElementSize and reflows the table only if it
   has changed
*/
nsresult nsTableOuterFrame::IR_CaptionInserted(nsIPresContext&        aPresContext,
                                               nsHTMLReflowMetrics&   aDesiredSize,
                                               OuterTableReflowState& aReflowState,
                                               nsReflowStatus&        aStatus,
                                               nsIFrame*              aCaptionFrame,
                                               PRBool                 aReplace)
{
  TDBG_S(gsDebugIR,"TOF IR: CaptionInserted\n");
  nsresult rv = NS_OK;
  nscoord oldCaptionMES = 0;
  NS_PRECONDITION(nsnull != aCaptionFrame, "null arg: aCaptionFrame");
  if (PR_TRUE == aReplace && nsnull != mCaptionFrame) {
    oldCaptionMES = mMinCaptionWidth;
  }
  // make aCaptionFrame this table's caption
  mCaptionFrame = aCaptionFrame;
  mInnerTableFrame->SetNextSibling(mCaptionFrame);

  // reflow the caption frame, getting it's MES
  TDBG_S(gsDebugIR,"TOF IR: initial-reflowing caption\n");

  nsSize              maxElementSize;
  nsHTMLReflowMetrics captionSize(&maxElementSize);
  nsHTMLReflowState   captionReflowState(aPresContext, aReflowState.reflowState, mCaptionFrame,
                                         nsSize(mRect.width, aReflowState.reflowState.availableHeight),
                                         eReflowReason_Initial);
  nsIHTMLReflow*      htmlReflow;

  if (NS_OK == mCaptionFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) { 
    // initial reflow of the caption
    htmlReflow->WillReflow(aPresContext);
    rv = htmlReflow->Reflow(aPresContext, captionSize, captionReflowState, aStatus);
    if (NS_FAILED(rv)) {
      return rv;
    }
    TDBG_SDDDD(gsDebugIR, "TOF IR: caption reflow returned %d with width =%d, height = %d, minWidth=%d\n",
               rv, captionSize.width, captionSize.height, maxElementSize.width);
    mMinCaptionWidth = maxElementSize.width;
    // get the caption's alignment
    const nsStyleText* captionTextStyle;
    mCaptionFrame->GetStyleData(eStyleStruct_Text, ((const nsStyleStruct *&)captionTextStyle));
    nsStyleUnit vAlignUnit = captionTextStyle->mVerticalAlign.GetUnit();
    if (PR_TRUE ||  // XXX: this is a hack because we don't support left|right captions yet
        (vAlignUnit != eStyleUnit_Enumerated) ||    // default is "top"
        ((vAlignUnit == eStyleUnit_Enumerated) &&
         ((vAlignUnit == NS_STYLE_VERTICAL_ALIGN_TOP) ||
          (vAlignUnit == NS_STYLE_VERTICAL_ALIGN_BOTTOM)))) {
      // XXX: caption align = left|right ignored here!
      // if the caption's MES > table width, reflow the inner table
      nsHTMLReflowMetrics innerSize(aDesiredSize.maxElementSize); 
      if ((oldCaptionMES != mMinCaptionWidth) && (mMinCaptionWidth > mRect.width)) {
        TDBG_S(gsDebugIR,"TOF IR: resize-reflowing inner table\n");
        nsHTMLReflowState innerReflowState(aPresContext, aReflowState.reflowState, mInnerTableFrame,
                                             nsSize(mMinCaptionWidth, aReflowState.reflowState.availableHeight),
                                             eReflowReason_Resize);
        rv = ReflowChild(mInnerTableFrame, aPresContext, innerSize, innerReflowState, aStatus);
        TDBG_SDDD(gsDebugIR,"TOF IR: inner table reflow returned %d with width =%d, height = %d\n",
                  rv, innerSize.width, innerSize.height);
      }
      else { // set innerSize as if the inner table were reflowed
        TDBG_S(gsDebugIR,"TOF IR: skipping reflow of inner table\n");
        innerSize.height = mRect.height;
        innerSize.width  = mRect.width;
      }
      // set maxElementSize width if requested
      if (nsnull != aDesiredSize.maxElementSize) {
        ((nsTableFrame *)mInnerTableFrame)->SetMaxElementSize(aDesiredSize.maxElementSize);
        if (mMinCaptionWidth > aDesiredSize.maxElementSize->width) {
          aDesiredSize.maxElementSize->width = mMinCaptionWidth;
        }
      }

      rv = SizeAndPlaceChildren(nsSize (innerSize.width, innerSize.height), 
                                nsSize (captionSize.width, captionSize.height), 
                                aReflowState);
    }
  }
  return rv;
}

nsresult nsTableOuterFrame::IR_CaptionRemoved(nsIPresContext&        aPresContext,
                                              nsHTMLReflowMetrics&   aDesiredSize,
                                              OuterTableReflowState& aReflowState,
                                              nsReflowStatus&        aStatus)
{
  TDBG_S(gsDebugIR,"TOF IR: CaptionRemoved\n");
  nsresult rv = NS_OK;
  if (nsnull != mCaptionFrame) {
    // get the caption's alignment
    const nsStyleText* captionTextStyle;
    mCaptionFrame->GetStyleData(eStyleStruct_Text, ((const nsStyleStruct *&)captionTextStyle));
    // if the caption's MES > table width, reflow the inner table minus the MES
    nsHTMLReflowMetrics innerSize(aDesiredSize.maxElementSize); 
    nscoord oldMinCaptionWidth = mMinCaptionWidth;
    // set the cached min caption width to 0 here, so it can't effect inner table reflow
    mMinCaptionWidth = 0;
    mCaptionFrame = nsnull;
    mInnerTableFrame->SetNextSibling(nsnull);
    if (oldMinCaptionWidth > mRect.width) { 
      // the old caption width had an effect on the inner table width, so reflow the inner table
      TDBG_S(gsDebugIR,"TOF IR: reflowing inner table\n");
      nsHTMLReflowState innerReflowState(aPresContext, aReflowState.reflowState, mInnerTableFrame,
                                         nsSize(aReflowState.reflowState.availableWidth, 
                                                aReflowState.reflowState.availableHeight));
      // ReflowChild sets MES
      rv = ReflowChild(mInnerTableFrame, aPresContext, innerSize, innerReflowState, aStatus);
      if (NS_FAILED(rv)) {
        return rv;
      }
      // set the output state
      aReflowState.innerTableMaxSize.width = innerSize.width;
      aReflowState.y = innerSize.height;
    }
    else {
      TDBG_S(gsDebugIR, "TOF IR: skipping reflow of inner table\n");
      nsRect innerRect;
      mInnerTableFrame->GetRect(innerRect);
      aReflowState.innerTableMaxSize.width = innerRect.width;
      aReflowState.y = innerRect.height;
    }
    // if the caption was a top caption, move the inner table to the correct offset
    if ((captionTextStyle->mVerticalAlign.GetUnit()==eStyleUnit_Enumerated) && 
        (captionTextStyle->mVerticalAlign.GetIntValue()==NS_STYLE_VERTICAL_ALIGN_BOTTOM)) {
      mInnerTableFrame->MoveTo(0,0);
    }
  }
  else {
    rv = NS_ERROR_ILLEGAL_VALUE;
  }

  return rv;
}

PRBool nsTableOuterFrame::IR_CaptionChangedAxis(const nsStyleText* aOldStyle, 
                                                const nsStyleText* aNewStyle) const
{
  PRBool result = PR_FALSE;
  //XXX: write me to support left|right captions!
  return result;
}

nsresult nsTableOuterFrame::SizeAndPlaceChildren(const nsSize&          aInnerSize, 
                                                 const nsSize&          aCaptionSize,
                                                 OuterTableReflowState& aReflowState)
{
  TDBG_S(gsDebugIR, "  TOF IR: SizeAndPlaceChildren\n");
  nsresult rv = NS_OK;
  // find where to place the caption
  nsMargin captionMargin;
  const nsStyleSpacing* spacing;
  mCaptionFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
  spacing->CalcMarginFor(mCaptionFrame, captionMargin);
  // Compute the caption's y-origin
  nscoord captionY = captionMargin.top;
  const nsStyleText* captionTextStyle;
  mCaptionFrame->GetStyleData(eStyleStruct_Text, ((const nsStyleStruct *&)captionTextStyle));
  if ((captionTextStyle->mVerticalAlign.GetUnit()==eStyleUnit_Enumerated) && 
      (captionTextStyle->mVerticalAlign.GetIntValue()==NS_STYLE_VERTICAL_ALIGN_BOTTOM)) {
    captionY += aInnerSize.height;
  }
  // Place the caption
  nsRect captionRect(captionMargin.left, captionY, 0, 0);
  captionRect.SizeTo(aCaptionSize.width, aCaptionSize.height);
  mCaptionFrame->SetRect(captionRect);
  TDBG_SDDDD(gsDebugIR, "    TOF IR: captionRect=%d %d %d %d\n", captionRect.x, captionRect.y,
             captionRect.width, captionRect.height);

  // Place the inner table
  nscoord innerY;
  if ((captionTextStyle->mVerticalAlign.GetUnit()==eStyleUnit_Enumerated) &&
      (captionTextStyle->mVerticalAlign.GetIntValue()==NS_STYLE_VERTICAL_ALIGN_BOTTOM)) { 
    // bottom caption
    innerY = 0;
    aReflowState.y = captionRect.YMost() + captionMargin.bottom;
  } 
  else { // top caption
    innerY = captionRect.YMost() + captionMargin.bottom;
    aReflowState.y = innerY + aInnerSize.height;
  }
  nsRect innerRect(0, innerY, aInnerSize.width, aInnerSize.height);
  mInnerTableFrame->SetRect(innerRect);
  TDBG_SDDDD(gsDebugIR, "    TOF IR: innerRect=%d %d %d %d\n", innerRect.x, innerRect.y, innerRect.width, innerRect.height);
  aReflowState.innerTableMaxSize.width = aInnerSize.width;
  return rv;
}

// computes the table width 
nscoord nsTableOuterFrame::ComputeAvailableTableWidth(const nsHTMLReflowState& aReflowState)
{
  nscoord maxWidth;

  // Figure out the overall table width constraint. Default case, get 100% of
  // available space
  if (NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth) {
    maxWidth = aReflowState.availableWidth;

  } else {
    const nsStylePosition* position =
      (const nsStylePosition*)mStyleContext->GetStyleData(eStyleStruct_Position);
  
    switch (position->mWidth.GetUnit()) {
    case eStyleUnit_Coord:
      maxWidth = position->mWidth.GetCoordValue();
      // NAV4 compatibility:  0-coord-width == auto-width
      // XXX This doesn't belong here. If the HTML attribute "width=0" should work
      // like 'auto', then have the style mapping code in the content object
      // handle it...
      if (0 == maxWidth) {
        maxWidth = aReflowState.availableWidth;
      }
      break;
  
    case eStyleUnit_Auto:
      maxWidth = aReflowState.availableWidth;
      break;
  
    case eStyleUnit_Percent:
      maxWidth = (nscoord)((float)aReflowState.availableWidth * position->mWidth.GetPercentValue());
      // NAV4 compatibility:  0-percent-width == auto-width
      // XXX Ditto...
      if (0 == maxWidth) {
        maxWidth = aReflowState.availableWidth;
      }
      break;

    case eStyleUnit_Proportional:
    case eStyleUnit_Inherit:
      // XXX for now these fall through
  
    default:
      maxWidth = aReflowState.availableWidth;
      break;
    }
  
    // Subtract out border and padding
    const nsStyleSpacing* spacing =
      (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
    nsMargin borderPadding;
    spacing->CalcBorderPaddingFor(this, borderPadding);
  
    maxWidth -= borderPadding.left + borderPadding.right;
    if (maxWidth <= 0) {
      // Nonsense style specification
      maxWidth = 0;
    }
  }

  return maxWidth;
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
NS_METHOD nsTableOuterFrame::Reflow(nsIPresContext&          aPresContext,
                                    nsHTMLReflowMetrics&     aDesiredSize,
                                    const nsHTMLReflowState& aReflowState,
                                    nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;
  TDBG_SPDD(gsDebug,"%p: nsTableOuterFrame::Reflow : maxSize=%d,%d\n", 
            this, aReflowState.availableWidth, aReflowState.availableHeight);

  PRIntervalTime startTime;
  if (gsTiming) {
    startTime = PR_IntervalNow();
  }

  // Initialize out parameters
  aDesiredSize.width  = 0;
  aDesiredSize.height = 0;
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }
  aStatus = NS_FRAME_COMPLETE;

  // Initialize our local reflow state
  OuterTableReflowState state(aPresContext, aReflowState);
  if (eReflowReason_Incremental == aReflowState.reason) {
    rv = IncrementalReflow(aPresContext, aDesiredSize, state, aStatus);
  } else {
    if (eReflowReason_Initial == aReflowState.reason) {
      //KIPP/TROY:  uncomment the following line for your own testing, do not check it in
      // NS_ASSERTION(nsnull == mFirstChild, "unexpected reflow reason");

      // Set up our kids.  They're already present, on an overflow list, 
      // or there are none so we'll create them now
      MoveOverflowToChildList();

      // Lay out the caption and get its maximum element size
      if (nsnull != mCaptionFrame) {
        nsSize              maxElementSize;
        nsHTMLReflowMetrics captionSize(&maxElementSize);
        nsHTMLReflowState   captionReflowState(aPresContext, aReflowState, mCaptionFrame,
                                               nsSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE),
                                               eReflowReason_Initial);
        nsIHTMLReflow*      htmlReflow;

        if (NS_OK == mCaptionFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
          htmlReflow->WillReflow(aPresContext);
          rv = htmlReflow->Reflow(aPresContext, captionSize, captionReflowState, aStatus);
          mMinCaptionWidth = maxElementSize.width;
        }
      }
    }

    // At this point, we must have an inner table frame, and we might have a caption
    NS_ASSERTION(mFrames.NotEmpty(), "no children");
    NS_ASSERTION(nsnull != mInnerTableFrame, "no mInnerTableFrame");

    // Compute the width to use for the table. In the case of an auto sizing
    // table this represents the maximum available width
    nscoord tableWidth = ComputeAvailableTableWidth(aReflowState);

    // If the caption max element size is larger, then use it instead.
    // XXX: caption align = left|right ignored here!
    if (mMinCaptionWidth > tableWidth) {
      tableWidth = mMinCaptionWidth;
    }

    // First reflow the inner table
    nsHTMLReflowState   innerReflowState(aPresContext, aReflowState, mInnerTableFrame,
                                         nsSize(tableWidth, aReflowState.availableHeight));
    nsHTMLReflowMetrics innerSize(aDesiredSize.maxElementSize); 

    rv = ReflowChild(mInnerTableFrame, aPresContext, innerSize, innerReflowState, aStatus);

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
    if (nsnull != mCaptionFrame) {
      // Get the caption's margin
      nsMargin              captionMargin;
      const nsStyleSpacing* spacing;

      mCaptionFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct *&)spacing);
      spacing->CalcMarginFor(mCaptionFrame, captionMargin);

      // Compute the caption's y-origin
      nscoord captionY = captionMargin.top;
      const nsStyleText* captionTextStyle;
      mCaptionFrame->GetStyleData(eStyleStruct_Text, ((const nsStyleStruct *&)captionTextStyle));
      if ((captionTextStyle->mVerticalAlign.GetUnit()==eStyleUnit_Enumerated) &&
          (captionTextStyle->mVerticalAlign.GetIntValue()==NS_STYLE_VERTICAL_ALIGN_BOTTOM)) {
        captionY += innerSize.height;
      }

      // Reflow the caption. Let it be as high as it wants
      nsHTMLReflowState   captionReflowState(aPresContext, state.reflowState, mCaptionFrame,
                                             nsSize(innerSize.width, NS_UNCONSTRAINEDSIZE),
                                             eReflowReason_Resize);
      nsHTMLReflowMetrics captionSize(nsnull);
      nsIHTMLReflow*      htmlReflow;
      nsRect captionRect(captionMargin.left, captionY, 0, 0);

      if (NS_OK == mCaptionFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
        nsReflowStatus  captionStatus;
        htmlReflow->WillReflow(aPresContext);
        htmlReflow->Reflow(aPresContext, captionSize, captionReflowState,
                           captionStatus);
        NS_ASSERTION(NS_FRAME_IS_COMPLETE(captionStatus), "unexpected reflow status");

        // XXX If the height is constrained then we need to check whether the inner
        // table still fits...

        // Place the caption
        captionRect.SizeTo(captionSize.width, captionSize.height);
        mCaptionFrame->SetRect(captionRect);
      }

      // Place the inner table
      nscoord innerY;
      if ((captionTextStyle->mVerticalAlign.GetUnit()!=eStyleUnit_Enumerated) ||
          (captionTextStyle->mVerticalAlign.GetIntValue()!=NS_STYLE_VERTICAL_ALIGN_BOTTOM)) {
        // top caption
        innerY = captionRect.YMost() + captionMargin.bottom;
        state.y = innerY + innerSize.height;
      } 
      else {
        // bottom caption
        innerY  = 0;
        state.y = captionRect.YMost() + captionMargin.bottom;
      }
      nsRect innerRect(0, innerY, innerSize.width, innerSize.height);
      mInnerTableFrame->SetRect(innerRect);

    } 
    else {
      // Place the inner table
      nsRect innerRect(0, 0, innerSize.width, innerSize.height);
      mInnerTableFrame->SetRect(innerRect);
      state.y = innerSize.height;
    }
  }
  
  // Return our desired rect
  aDesiredSize.width   = state.innerTableMaxSize.width;
  aDesiredSize.height  = state.y;
  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;

  if ((PR_TRUE == gsDebug) || (PR_TRUE == gsDebugIR)) {
    if (nsnull!=aDesiredSize.maxElementSize) {
      printf("%p: Outer frame Reflow complete, returning %s with aDesiredSize = %d,%d and aMaxElementSize=%d,%d\n",
              this, NS_FRAME_IS_COMPLETE(aStatus)? "Complete" : "Not Complete",
              aDesiredSize.width, aDesiredSize.height, 
              aDesiredSize.maxElementSize->width, aDesiredSize.maxElementSize->height);
    }
    else {
      printf("%p: Outer frame Reflow complete, returning %s with aDesiredSize = %d,%d and NSNULL aMaxElementSize\n",
              this, NS_FRAME_IS_COMPLETE(aStatus)? "Complete" : "Not Complete",
              aDesiredSize.width, aDesiredSize.height);
    }
  }

  if (gsTiming) {
    PRIntervalTime endTime = PR_IntervalNow();
    printf("Table reflow took %d ticks for frame %p\n",
           endTime-startTime, this);/* XXX need to use LL_* macros! */
  }

  return rv;
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame, and includes
// any left/top margin.
void nsTableOuterFrame::PlaceChild(OuterTableReflowState& aReflowState,
                                   nsIFrame*              aKidFrame,
                                   const nsRect&          aKidRect,
                                   nsSize*                aMaxElementSize,
                                   nsSize&                aKidMaxElementSize)
{
  TDBG_SPDDDD(gsDebug,"outer table place child: %p with aKidRect %d %d %d %d\n", 
              aKidFrame, aKidRect.x, aKidRect.y, aKidRect.width, aKidRect.height);
  // Place and size the child
  aKidFrame->SetRect(aKidRect);

  // Adjust the running y-offset
  aReflowState.y += aKidRect.height;

  // If our height is constrained then update the available height
  if (PR_FALSE == aReflowState.unconstrainedHeight) {
    aReflowState.availSize.height -= aKidRect.height;
  }

  /* Update the maximum element size, which is the max of:
   *   the maxElementSize of our first row
   *   or the maxElementSize of the caption if we include it
   */
  if (aKidFrame == mCaptionFrame) {
    if (nsnull != aMaxElementSize) {
      aMaxElementSize->width = aKidMaxElementSize.width;
      aMaxElementSize->height = aKidMaxElementSize.height;
    }
  }
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
void nsTableOuterFrame::DeleteChildsNextInFlow(nsIPresContext& aPresContext, 
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
  nextInFlow->BreakFromPrevFlow();

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
  nextInFlow->DeleteFrame(aPresContext);

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

nsresult 
NS_NewTableOuterFrame(nsIFrame*& aResult)
{
  nsIFrame* it = new nsTableOuterFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = it;
  return NS_OK;
}

NS_IMETHODIMP
nsTableOuterFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("TableOuter", aResult);
}
