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
#include "nsTableColGroupFrame.h"
#include "nsTableColFrame.h"
#include "nsTableFrame.h"
#include "nsIHTMLTableColElement.h"
#include "nsIDOMHTMLTableColElement.h"
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLParts.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsCOMPtr.h"
#include "nsCSSRendering.h"
#include "nsIPresShell.h"
#include "nsLayoutAtoms.h"


#define COLGROUP_TYPE_CONTENT              0x0
#define COLGROUP_TYPE_ANONYMOUS_COL        0x1
#define COLGROUP_TYPE_ANONYMOUS_CELL       0x2

nsTableColGroupType nsTableColGroupFrame::GetType() const {
  switch(mBits.mType) {
  case COLGROUP_TYPE_ANONYMOUS_COL:
    return eColGroupAnonymousCol;
  case COLGROUP_TYPE_ANONYMOUS_CELL:
    return eColGroupAnonymousCell;
  default:
    return eColGroupContent;
  }
}

void nsTableColGroupFrame::SetType(nsTableColGroupType aType) {
  mBits.mType = aType - eColGroupContent;
}

void nsTableColGroupFrame::ResetColIndices(nsIPresContext* aPresContext,
                                           nsIFrame*       aFirstColGroup,
                                           PRInt32         aFirstColIndex,
                                           nsIFrame*       aStartColFrame)
{
  nsTableColGroupFrame* colGroupFrame = (nsTableColGroupFrame*)aFirstColGroup;
  PRInt32 colIndex = aFirstColIndex;
  while (colGroupFrame) {
    nsCOMPtr<nsIAtom> cgType;
    colGroupFrame->GetFrameType(getter_AddRefs(cgType));
    if (nsLayoutAtoms::tableColGroupFrame == cgType.get()) {
      // reset the starting col index for the first cg only if
      // aFirstColIndex is smaller than the existing starting col index
      if ((colIndex != aFirstColIndex) ||
          (colIndex < colGroupFrame->GetStartColumnIndex())) {
        colGroupFrame->SetStartColumnIndex(colIndex);
      }
      nsIFrame* colFrame = aStartColFrame; 
      if (!colFrame || (colIndex != aFirstColIndex)) {
        colGroupFrame->FirstChild(aPresContext, nsnull, &colFrame);
      }
      while (colFrame) {
        nsCOMPtr<nsIAtom> colType;
        colFrame->GetFrameType(getter_AddRefs(colType));
        if (nsLayoutAtoms::tableColFrame == colType.get()) {
          ((nsTableColFrame*)colFrame)->SetColIndex(colIndex);
          colIndex++;
        }
        colFrame->GetNextSibling(&colFrame);
      }
    }
    colGroupFrame->GetNextSibling((nsIFrame**)&colGroupFrame);
  }
}


NS_IMETHODIMP
nsTableColGroupFrame::AddColsToTable(nsIPresContext&  aPresContext,
                                     PRInt32          aFirstColIndex,
                                     PRBool           aResetSubsequentColIndices,
                                     nsIFrame*        aFirstFrame,
                                     nsIFrame*        aLastFrame)
{
  nsresult rv = NS_OK;
  nsTableFrame* tableFrame = nsnull;
  rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if (!tableFrame || !aFirstFrame) return NS_ERROR_NULL_POINTER;

  // set the col indices of the col frames and and add col info to the table
  PRInt32 colIndex = aFirstColIndex;
  nsIFrame* kidFrame = aFirstFrame;
  PRBool foundLastFrame = PR_FALSE;
  while (kidFrame) {
    nsIAtom* kidType;
    kidFrame->GetFrameType(&kidType);
    if (nsLayoutAtoms::tableColFrame == kidType) {
      ((nsTableColFrame*)kidFrame)->SetColIndex(colIndex);
      if (!foundLastFrame) {
        mColCount++;
        tableFrame->InsertCol(aPresContext, (nsTableColFrame &)*kidFrame, colIndex);
      }
      colIndex++;
    }
    NS_IF_RELEASE(kidType);
    if (kidFrame == aLastFrame) {
      foundLastFrame = PR_TRUE;
    }
    kidFrame->GetNextSibling(&kidFrame); 
  }

  if (aResetSubsequentColIndices) {
    nsIFrame* nextSibling;
    GetNextSibling(&nextSibling);
    if (nextSibling) {
      ResetColIndices(&aPresContext, nextSibling, colIndex);
    }
  }

  return rv;
}

// this is called when a col frame doesn't have an explicit col group parent.
nsTableColGroupFrame* 
nsTableColGroupFrame::FindParentForAppendedCol(nsTableFrame*  aTableFrame, 
                                               nsTableColType aColType)
{
  nsVoidArray& cols = aTableFrame->GetColCache();
  PRInt32 numCols = cols.Count();
  nsIFrame* lastColGroup;
  nsIFrame* lastCol = (nsIFrame*)cols.ElementAt(numCols - 1);
  if (!lastCol) return nsnull; // no columns so no colgroups
  lastCol->GetParent(&lastColGroup);
  if (!lastColGroup) return nsnull; // shouldn't happen
 
  nsTableColGroupFrame* relevantColGroup = (nsTableColGroupFrame *)lastColGroup;
  nsTableColGroupType relevantColGroupType = relevantColGroup->GetType();
  if (eColGroupAnonymousCell == relevantColGroupType) {
    if (eColAnonymousCell == aColType) {
      return relevantColGroup;
    }
    else {
      // find the next to last col group
      for (PRInt32 colX = numCols - 2; colX >= 0; colX--) {
        nsTableColFrame* colFrame = (nsTableColFrame*)cols.ElementAt(colX);
        nsTableColGroupFrame* colGroupFrame;
        colFrame->GetParent((nsIFrame**)&colGroupFrame);
        nsTableColGroupType cgType = colGroupFrame->GetType();
        if (cgType != relevantColGroupType) {
          relevantColGroup = colGroupFrame;
          relevantColGroupType = cgType;
          break;
        }
        else if (0 == colX) {
          return nsnull;
        }
      }
    }
  }

  if (eColGroupAnonymousCol == relevantColGroupType) {
    if ((eColContent == aColType) || (eColAnonymousCol == aColType)) {
      return relevantColGroup;
    }
  }

  return nsnull;
}

PRBool
nsTableColGroupFrame::GetLastRealColGroup(nsTableFrame* aTableFrame, 
                                          nsIFrame**    aLastColGroup)
{
  *aLastColGroup = nsnull;
  nsFrameList colGroups = aTableFrame->GetColGroups();

  nsIFrame* nextToLastColGroup = nsnull;
  nsIFrame* lastColGroup       = colGroups.FirstChild();
  while(lastColGroup) {
    nsIFrame* next;
    lastColGroup->GetNextSibling(&next);
    if (next) {
      nextToLastColGroup = lastColGroup;
      lastColGroup = next;
    }
    else {
      break;
    }
  }

  if (!lastColGroup) return PR_TRUE; // there are no col group frames
 
  nsTableColGroupType lastColGroupType = ((nsTableColGroupFrame *)lastColGroup)->GetType();
  if (eColGroupAnonymousCell == lastColGroupType) {
    *aLastColGroup = nextToLastColGroup;
    return PR_FALSE;
  }
  else {
    *aLastColGroup = lastColGroup;
    return PR_TRUE;
  }
}

// don't set mColCount here, it is done in AddColsToTable
NS_IMETHODIMP
nsTableColGroupFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                          nsIAtom*        aListName,
                                          nsIFrame*       aChildList)
{
  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);
  if (!tableFrame) return NS_ERROR_NULL_POINTER;

  if (!aChildList) {
    nsIFrame* firstChild;
    tableFrame->CreateAnonymousColFrames(*aPresContext, *this, GetSpan(), eColAnonymousColGroup, 
                                         PR_FALSE, nsnull, &firstChild);
    if (firstChild) {
      SetInitialChildList(aPresContext, aListName, firstChild);
    }
    return NS_OK; 
  }

  nsIFrame* kidFrame = aChildList;
  while (kidFrame) {
    nsIAtom* kidType;
    kidFrame->GetFrameType(&kidType);
    if (nsLayoutAtoms::tableColFrame == kidType) {
      // Set the preliminary values for the column frame
      PRInt32 span = ((nsTableColFrame*)kidFrame)->GetSpan();
      if (span > 1) {
        nsTableColFrame* firstSpannedCol;
        tableFrame->CreateAnonymousColFrames(*aPresContext, *this, span - 1, eColAnonymousCol,
                                             PR_FALSE, (nsTableColFrame*)kidFrame, (nsIFrame **)&firstSpannedCol);
        nsIFrame* spanner = kidFrame;
        kidFrame->GetNextSibling(&kidFrame); // need to do this before we insert the new frames
        nsFrameList newChildren(aChildList); // used as a convience to hook up siblings
        newChildren.InsertFrames(this, (nsTableColFrame*)spanner, (nsIFrame *)firstSpannedCol);
        NS_RELEASE(kidType);
        continue;
      }
    }
    NS_IF_RELEASE(kidType);
    kidFrame->GetNextSibling(&kidFrame); 
  }

  mFrames.AppendFrames(this, aChildList);
  return NS_OK;
}

NS_IMETHODIMP
nsTableColGroupFrame::AppendFrames(nsIPresContext* aPresContext,
                                   nsIPresShell&   aPresShell,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aFrameList)
{
  mFrames.AppendFrames(this, aFrameList);
  InsertColsReflow(*aPresContext, aPresShell, mColCount, aFrameList);
  return NS_OK;
}

NS_IMETHODIMP
nsTableColGroupFrame::InsertFrames(nsIPresContext* aPresContext,
                                   nsIPresShell&   aPresShell,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aPrevFrameIn,
                                   nsIFrame*       aFrameList)
{
  nsFrameList frames(aFrameList); // convience for getting last frame
  nsIFrame* lastFrame = frames.LastChild();

  mFrames.InsertFrames(this, aPrevFrameIn, aFrameList);
  nsIFrame* prevFrame = nsTableFrame::GetFrameAtOrBefore(aPresContext, this, aPrevFrameIn, 
                                                         nsLayoutAtoms::tableColFrame);

  PRInt32 colIndex = (prevFrame) ? ((nsTableColFrame*)prevFrame)->GetColIndex() + 1 : 0;
  InsertColsReflow(*aPresContext, aPresShell, colIndex, aFrameList, lastFrame);

  return NS_OK;
}

void
nsTableColGroupFrame::InsertColsReflow(nsIPresContext& aPresContext,
                                       nsIPresShell&   aPresShell,
                                       PRInt32         aColIndex,
                                       nsIFrame*       aFirstFrame,
                                       nsIFrame*       aLastFrame)
{
  AddColsToTable(aPresContext, aColIndex, PR_TRUE, aFirstFrame, aLastFrame);

  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);
  if (!tableFrame) return;

  // XXX this could be optimized with much effort
  tableFrame->SetNeedStrategyInit(PR_TRUE);

  // Generate a reflow command so we reflow the table
  nsTableFrame::AppendDirtyReflowCommand(&aPresShell, tableFrame);
}

void
nsTableColGroupFrame::RemoveChild(nsIPresContext&  aPresContext,
                                  nsTableColFrame& aChild,
                                  PRBool           aResetColIndices)
{
  PRInt32 colIndex = 0;
  nsIFrame* nextChild = nsnull;
  if (aResetColIndices) {
    colIndex = aChild.GetColIndex();
    aChild.GetNextSibling(&nextChild);
  }
  if (mFrames.DestroyFrame(&aPresContext, (nsIFrame*)&aChild)) {
    mColCount--;
    if (aResetColIndices) {
      ResetColIndices(&aPresContext, this, colIndex, nextChild);
    }
  }
  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);
  if (!tableFrame) return;

  // XXX this could be optimized with much effort
  tableFrame->SetNeedStrategyInit(PR_TRUE);
  // Generate a reflow command so we reflow the table
  nsTableFrame::AppendDirtyReflowCommand(nsTableFrame::GetPresShellNoAddref(&aPresContext), tableFrame);
}

// this removes children form the last col group (eColGroupAnonymousCell) in the 
// table only,so there is no need to reset col indices for subsequent col groups.
void
nsTableColGroupFrame::RemoveChildrenAtEnd(nsIPresContext& aPresContext,
                                          PRInt32         aNumChildrenToRemove)
{
  PRInt32 numToRemove = aNumChildrenToRemove;
  if (numToRemove > mColCount) {
    NS_ASSERTION(PR_FALSE, "invalid arg to RemoveChildrenAtEnd");
    numToRemove = mColCount;
  }
  PRInt32 offsetOfFirstRemoval = mColCount - numToRemove;
  PRInt32 offsetX = 0;
  nsIFrame* kidFrame = mFrames.FirstChild();
  while(kidFrame) {
    nsIAtom* kidType;
    kidFrame->GetFrameType(&kidType);
    if (nsLayoutAtoms::tableColFrame == kidType) {
      offsetX++;
      if (offsetX > offsetOfFirstRemoval) {
        nsIFrame* byebye = kidFrame;
        kidFrame->GetNextSibling(&kidFrame);
        mFrames.DestroyFrame(&aPresContext, byebye);
        NS_RELEASE(kidType);
        continue;
      }
    }
    NS_IF_RELEASE(kidType);
    kidFrame->GetNextSibling(&kidFrame);
  }
}


NS_IMETHODIMP
nsTableColGroupFrame::RemoveFrame(nsIPresContext* aPresContext,
                                  nsIPresShell&   aPresShell,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aOldFrame)
{
  if (!aOldFrame) return NS_OK;

  nsIAtom* frameType = nsnull;
  aOldFrame->GetFrameType(&frameType);
  if (nsLayoutAtoms::tableColFrame == frameType) {
    nsTableColFrame* colFrame = (nsTableColFrame*)aOldFrame;
    PRInt32 colIndex = colFrame->GetColIndex();
    RemoveChild(*aPresContext, *colFrame, PR_TRUE);
    
    nsTableFrame* tableFrame;
    nsTableFrame::GetTableFrame(this, tableFrame);
    if (!tableFrame) return NS_ERROR_NULL_POINTER;

    tableFrame->RemoveCol(*aPresContext, this, colIndex, PR_TRUE, PR_TRUE);

    // XXX This could probably be optimized with much effort
    tableFrame->SetNeedStrategyInit(PR_TRUE);
    // Generate a reflow command so we reflow the table
    nsTableFrame::AppendDirtyReflowCommand(&aPresShell, tableFrame);
  }
  else {
    mFrames.DestroyFrame(aPresContext, aOldFrame);
  }
  NS_IF_RELEASE(frameType);

  return NS_OK;
}

NS_METHOD 
nsTableColGroupFrame::Paint(nsIPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect&        aDirtyRect,
                            nsFramePaintLayer    aWhichLayer,
                            PRUint32             aFlags)
{
  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_FALSE, &isVisible)) && !isVisible) {
    return NS_OK;
  }
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    nsCompatibility mode;
    aPresContext->GetCompatibilityMode(&mode);
    if (eCompatibility_Standard == mode) {
      const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
      if (vis->IsVisibleOrCollapsed()) {
        const nsStyleBorder* border =
          (const nsStyleBorder*)mStyleContext->GetStyleData(eStyleStruct_Border);
        const nsStyleBackground* color =
          (const nsStyleBackground*)mStyleContext->GetStyleData(eStyleStruct_Background);
        nsRect rect(0, 0, mRect.width, mRect.height);
        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *color, *border, 0, 0);
      }
    }
  }
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  return NS_OK;
}

PRIntn
nsTableColGroupFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  if (nsnull != mPrevInFlow) {
    skip |= 1 << NS_SIDE_TOP;
  }
  if (nsnull != mNextInFlow) {
    skip |= 1 << NS_SIDE_BOTTOM;
  }
  return skip;
}

NS_IMETHODIMP
nsTableColGroupFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                   const nsPoint& aPoint, 
                                   nsFramePaintLayer aWhichLayer,
                                   nsIFrame**     aFrame)
{
  // this should act like a block, so we need to override
  return GetFrameForPointUsing(aPresContext, aPoint, nsnull, aWhichLayer, (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND), aFrame);
}

NS_METHOD nsTableColGroupFrame::Reflow(nsIPresContext*          aPresContext,
                                       nsHTMLReflowMetrics&     aDesiredSize,
                                       const nsHTMLReflowState& aReflowState,
                                       nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableColGroupFrame", aReflowState.reason);
  DISPLAY_REFLOW(this, aReflowState, aDesiredSize, aStatus);
  NS_ASSERTION(nsnull!=mContent, "bad state -- null content for frame");
  nsresult rv=NS_OK;
  // for every content child that (is a column thingy and does not already have a frame)
  // create a frame and adjust it's style
  nsIFrame* kidFrame = nsnull;
 
  if (eReflowReason_Incremental == aReflowState.reason) {
    rv = IncrementalReflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  }

  for (kidFrame = mFrames.FirstChild(); nsnull != kidFrame;
       kidFrame->GetNextSibling(&kidFrame)) {
    // Give the child frame a chance to reflow, even though we know it'll have 0 size
    nsHTMLReflowMetrics kidSize(nsnull);
    // XXX Use a valid reason...
    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame,
                                     nsSize(0,0), eReflowReason_Initial);

    nsReflowStatus status;
    ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, 0, 0, 0, status);
    FinishReflowChild(kidFrame, aPresContext, kidSize, 0, 0, 0);
  }

  aDesiredSize.width=0;
  aDesiredSize.height=0;
  aDesiredSize.ascent=aDesiredSize.height;
  aDesiredSize.descent=0;
  if (nsnull!=aDesiredSize.maxElementSize)
  {
    aDesiredSize.maxElementSize->width=0;
    aDesiredSize.maxElementSize->height=0;
  }
  aStatus = NS_FRAME_COMPLETE;
  return rv;
}

NS_METHOD nsTableColGroupFrame::IncrementalReflow(nsIPresContext*          aPresContext,
                                                  nsHTMLReflowMetrics&     aDesiredSize,
                                                  const nsHTMLReflowState& aReflowState,
                                                  nsReflowStatus&          aStatus)
{
  nsresult  rv = NS_OK;

  // determine if this frame is the target or not
  nsIFrame *target=nsnull;
  rv = aReflowState.reflowCommand->GetTarget(target);
  if ((PR_TRUE==NS_SUCCEEDED(rv)) && (nsnull!=target))
  {
    if (this==target)
      rv = IR_TargetIsMe(aPresContext, aDesiredSize, aReflowState, aStatus);
    else
    {
      // Get the next frame in the reflow chain
      nsIFrame* nextFrame;
      aReflowState.reflowCommand->GetNext(nextFrame);
      rv = IR_TargetIsChild(aPresContext, aDesiredSize, aReflowState, aStatus, nextFrame);
    }
  }
  return rv;
}

NS_METHOD nsTableColGroupFrame::IR_TargetIsMe(nsIPresContext*          aPresContext,
                                              nsHTMLReflowMetrics&     aDesiredSize,
                                              const nsHTMLReflowState& aReflowState,
                                              nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;
  aStatus = NS_FRAME_COMPLETE;
  nsIReflowCommand::ReflowType type;
  aReflowState.reflowCommand->GetType(type);
  nsIFrame *objectFrame;
  aReflowState.reflowCommand->GetChildFrame(objectFrame); 
  const nsStyleDisplay *childDisplay=nsnull;
  if (nsnull!=objectFrame)
    objectFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
  switch (type)
  {
  case nsIReflowCommand::StyleChanged :
    rv = IR_StyleChanged(aPresContext, aDesiredSize, aReflowState, aStatus);
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

NS_METHOD nsTableColGroupFrame::IR_StyleChanged(nsIPresContext*          aPresContext,
                                                nsHTMLReflowMetrics&     aDesiredSize,
                                                const nsHTMLReflowState& aReflowState,
                                                nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;
  // we presume that all the easy optimizations were done in the nsHTMLStyleSheet before we were called here
  // XXX: we can optimize this when we know which style attribute changed
  nsTableFrame* tableFrame = nsnull;
  rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if (tableFrame)  {
    tableFrame->SetNeedStrategyInit(PR_TRUE);
  }
  return rv;
}

NS_METHOD nsTableColGroupFrame::IR_TargetIsChild(nsIPresContext*          aPresContext,
                                                 nsHTMLReflowMetrics&     aDesiredSize,
                                                 const nsHTMLReflowState& aReflowState,
                                                 nsReflowStatus&          aStatus,
                                                 nsIFrame *               aNextFrame)
{
  nsresult rv;
 
  // Pass along the reflow command
  nsHTMLReflowMetrics desiredSize(nsnull);
  nsHTMLReflowState kidReflowState(aPresContext, aReflowState, aNextFrame,
                                   nsSize(aReflowState.availableWidth,
                                          aReflowState.availableHeight));
  rv = ReflowChild(aNextFrame, aPresContext, desiredSize, kidReflowState, 0, 0, 0, aStatus);
  aNextFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
  if (NS_FAILED(rv))
    return rv;

  nsTableFrame *tableFrame=nsnull;
  rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if (tableFrame) {
    // compare the new col count to the old col count.  
    // If they are the same, we just need to rebalance column widths
    // If they differ, we need to fix up other column groups and the column cache
    // XXX for now assume the worse
    tableFrame->SetNeedStrategyInit(PR_TRUE);
  }
  return rv;
}

nsTableColFrame * nsTableColGroupFrame::GetFirstColumn()
{
  return GetNextColumn(nsnull);
}

nsTableColFrame * nsTableColGroupFrame::GetNextColumn(nsIFrame *aChildFrame)
{
  nsTableColFrame *result = nsnull;
  nsIFrame *childFrame = aChildFrame;
  if (nsnull==childFrame)
    childFrame = mFrames.FirstChild();
  while (nsnull!=childFrame)
  {
    const nsStyleDisplay *childDisplay;
    childFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_COLUMN == childDisplay->mDisplay)
    {
      result = (nsTableColFrame *)childFrame;
      break;
    }
    childFrame->GetNextSibling(&childFrame);
  }
  return result;
}


nsTableColFrame * nsTableColGroupFrame::GetColumnAt (PRInt32 aColIndex)
{
  nsTableColFrame *result = nsnull;
  PRInt32 count = 0;
  nsIFrame *childFrame = mFrames.FirstChild();

  while (nsnull!=childFrame) {
    const nsStyleDisplay *childDisplay;
    childFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_COLUMN == childDisplay->mDisplay) {
      nsTableColFrame *col = (nsTableColFrame *)childFrame;
      count++;
      if (aColIndex<=count) {
        result = col;
      }
    }
    childFrame->GetNextSibling(&childFrame);
  }

  return result;
}

PRInt32 nsTableColGroupFrame::GetSpan()
{
  PRInt32 span = 1;
  nsCOMPtr<nsIContent> iContent;
  nsresult rv = GetContent(getter_AddRefs(iContent));
  if (NS_FAILED(rv) || !iContent) return rv;

  // col group element derives from col element
  nsIDOMHTMLTableColElement* cgContent = nsnull;
  rv = iContent->QueryInterface(NS_GET_IID(nsIDOMHTMLTableColElement), 
                               (void **)&cgContent);  
  if (cgContent && NS_SUCCEEDED(rv)) { 
    cgContent->GetSpan(&span);
    // XXX why does this work!!
    if (span == -1) {
      span = 1;
    }
    NS_RELEASE(cgContent);
  }
  return span;
}

/** returns colcount because it is frequently used in the context of 
  * shuffling relative colgroup order, and it's convenient to not have to
  * call GetColumnCount redundantly.
  */
PRInt32 nsTableColGroupFrame::SetStartColumnIndex (int aIndex)
{
  PRInt32 result = mColCount;
  if (aIndex != mStartColIndex) {  
    mStartColIndex = aIndex;
    result = GetColCount(); 
  }
  return result;
}


// this could be optimized by using col group frame starting indicies, 
// but typically there aren't enough very large col groups for the added complexity.
nsTableColGroupFrame* 
nsTableColGroupFrame::GetColGroupFrameContaining(nsIPresContext*  aPresContext,
                                                 nsFrameList&     aColGroupList,
                                                 nsTableColFrame& aColFrame)
{
  nsIFrame* childFrame = aColGroupList.FirstChild();
  while (childFrame) { 
    nsIAtom* frameType = nsnull;
    childFrame->GetFrameType(&frameType);
    if (nsLayoutAtoms::tableColGroupFrame == frameType) {
      nsTableColFrame* colFrame = nsnull;
      childFrame->FirstChild(aPresContext, nsnull, (nsIFrame **)&colFrame);
      while (colFrame) {
        if (colFrame == &aColFrame) {
          NS_RELEASE(frameType);
          return (nsTableColGroupFrame *)childFrame;
        }
        colFrame->GetNextSibling((nsIFrame **)&colFrame);
      }
    }
    NS_IF_RELEASE(frameType);
    childFrame->GetNextSibling(&childFrame);
  }
  return nsnull;
}

void nsTableColGroupFrame::DeleteColFrame(nsIPresContext* aPresContext, nsTableColFrame* aColFrame)
{
  mFrames.DestroyFrame(aPresContext, aColFrame);
}

/* ----- global methods ----- */

nsresult 
NS_NewTableColGroupFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTableColGroupFrame* it = new (aPresShell) nsTableColGroupFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

NS_IMETHODIMP
nsTableColGroupFrame::Init(nsIPresContext*  aPresContext,
                           nsIContent*      aContent,
                           nsIFrame*        aParent,
                           nsIStyleContext* aContext,
                           nsIFrame*        aPrevInFlow)
{
  nsresult  rv;

  // Let the base class do its processing
  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext,
                                  aPrevInFlow);

  // record that children that are ignorable whitespace should be excluded 
  mState |= NS_FRAME_EXCLUDE_IGNORABLE_WHITESPACE;

  return rv;
}

NS_IMETHODIMP
nsTableColGroupFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::tableColGroupFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsTableColGroupFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("TableColGroup"), aResult);
}

NS_IMETHODIMP
nsTableColGroupFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  PRUint32 sum = sizeof(*this);
  *aResult = sum;
  return NS_OK;
}
#endif
