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
#include "nsTableColGroupFrame.h"
#include "nsTableColFrame.h"
#include "nsTableFrame.h"
#include "nsIHTMLTableColElement.h"
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIPtr.h"
#include "nsHTMLAtoms.h"

NS_DEF_PTR(nsIContent);
NS_DEF_PTR(nsIStyleContext);

static NS_DEFINE_IID(kIHTMLTableColElementIID, NS_IHTMLTABLECOLELEMENT_IID);


static PRBool gsDebug = PR_FALSE;

nsTableColGroupFrame::nsTableColGroupFrame(nsIContent* aContent,
                     nsIFrame*   aParentFrame)
  : nsContainerFrame(aContent, aParentFrame)
{
  mColCount=0;
}

nsTableColGroupFrame::~nsTableColGroupFrame()
{
}

nsresult
nsTableColGroupFrame::InitNewFrames(nsIPresContext& aPresContext, nsIFrame* aChildList)
{
  // Process the newly added column frames
  for (nsIFrame* kidFrame = aChildList; nsnull != kidFrame; kidFrame->GetNextSibling(kidFrame)) {
    // Set the preliminary values for the column frame
    nsIContent* kid;
    kidFrame->GetContent(kid);
    PRInt32 repeat=1;
    nsIHTMLTableColElement* colContent = nsnull;
    nsresult  rv = kid->QueryInterface(kIHTMLTableColElementIID, 
                     (void**) &colContent); // colContent: ADDREF++
    NS_RELEASE(kid);
    if (rv==NS_OK)
    {
      colContent->GetSpanValue(&repeat);
      NS_RELEASE(colContent);
    }
    PRInt32 colIndex = mStartColIndex + mColCount;
    ((nsTableColFrame *)(kidFrame))->InitColFrame (colIndex, repeat);
    mColCount+= repeat;

    // Set nsColFrame-specific information
    ((nsTableColFrame *)kidFrame)->SetColumnIndex(colIndex);
    nsIFrame* tableFrame=nsnull;
    GetGeometricParent(tableFrame);
    ((nsTableFrame *)tableFrame)->AddColumnFrame((nsTableColFrame *)kidFrame);

    SetStyleContextForFirstPass(&aPresContext, colIndex);
  }

  return NS_OK;
}

nsresult
nsTableColGroupFrame::AppendNewFrames(nsIPresContext& aPresContext, nsIFrame* aChildList)
{
  nsIFrame* lastChild;
  LastChild(lastChild);

  // Append the new frames to the child list
  if (nsnull == lastChild) {
    mFirstChild = aChildList;
  } else {
    lastChild->SetNextSibling(aChildList);
  }
  mChildCount += LengthOf(aChildList);

  return NS_OK;
}

NS_IMETHODIMP
nsTableColGroupFrame::Init(nsIPresContext& aPresContext, nsIFrame* aChildList)
{
  return AppendNewFrames(aPresContext, aChildList);
}

NS_METHOD nsTableColGroupFrame::Paint(nsIPresContext& aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      const nsRect&        aDirtyRect)
{
  if (gsDebug==PR_TRUE) printf("nsTableColGroupFrame::Paint\n");
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);
  return NS_OK;
}

// TODO:  incremental reflow
// today, we just throw away the column frames and start over every time
// this is dumb, we should be able to maintain column frames and adjust incrementally
NS_METHOD nsTableColGroupFrame::Reflow(nsIPresContext&      aPresContext,
                                       nsReflowMetrics&     aDesiredSize,
                                       const nsReflowState& aReflowState,
                                       nsReflowStatus&      aStatus)
{
  NS_ASSERTION(nsnull!=mContent, "bad state -- null content for frame");

  // for every content child that (is a column thingy and does not already have a frame)
  // create a frame and adjust it's style
  nsIFrame* kidFrame = nsnull;
 
  if (eReflowReason_Initial == aReflowState.reason) {
    // XXX Don't do this in the Init() member function, because the cell
    // map hasn't been created yet..
    InitNewFrames(aPresContext, mFirstChild);

  } else if (eReflowReason_Incremental == aReflowState.reason) {
    NS_ASSERTION(nsnull != aReflowState.reflowCommand, "null reflow command");

    // Get the type of reflow command
    nsIReflowCommand::ReflowType reflowCmdType;
    aReflowState.reflowCommand->GetType(reflowCmdType);

    // Currently we only expect appended reflow commands
    NS_ASSERTION(nsIReflowCommand::FrameAppended == reflowCmdType,
                 "unexpected reflow command");

    // Get the new column frames
    nsIFrame* childList;
    aReflowState.reflowCommand->GetChildFrame(childList);

    // Append them to the child list
    AppendNewFrames(aPresContext, childList);
    InitNewFrames(aPresContext, childList);
  }

  for (kidFrame = mFirstChild; nsnull != kidFrame; kidFrame->GetNextSibling(kidFrame)) {
    // Give the child frame a chance to reflow, even though we know it'll have 0 size
    nsReflowMetrics kidSize(nsnull);
    // XXX Use a valid reason...
    nsReflowState   kidReflowState(kidFrame, aReflowState, nsSize(0,0), eReflowReason_Initial);
    kidFrame->WillReflow(aPresContext);
    nsReflowStatus status = ReflowChild(kidFrame,&aPresContext, kidSize,
                                        kidReflowState);
    // note that DidReflow is called as the result of some ancestor firing off a DidReflow above me
    kidFrame->SetRect(nsRect(0,0,0,0));
  }

  aDesiredSize.width=0;
  aDesiredSize.height=0;
  if (nsnull!=aDesiredSize.maxElementSize)
  {
    aDesiredSize.maxElementSize->width=0;
    aDesiredSize.maxElementSize->height=0;
  }
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

// Subclass hook for style post processing
NS_METHOD nsTableColGroupFrame::SetStyleContextForFirstPass(nsIPresContext* aPresContext,
                                                            PRInt32         aColIndex)
{
  // get the table frame
  nsIFrame* tableFrame=nsnull;
  GetGeometricParent(tableFrame);
  tableFrame->GetGeometricParent(tableFrame); // get the outer frame
  
  // get the style for the table frame
  nsIStyleContextPtr tableSC;
  tableFrame->GetStyleContext(aPresContext, tableSC.AssignRef());
  nsStyleTable *tableStyle = (nsStyleTable*)tableSC->GetStyleData(eStyleStruct_Table);

  // if COLS is set, then map it into the COL frames
  if (NS_STYLE_TABLE_COLS_NONE != tableStyle->mCols)
  {
    // set numCols to the number of columns effected by the COLS attribute
    PRInt32 numCols=0;
    if (NS_STYLE_TABLE_COLS_ALL == tableStyle->mCols)
      ChildCount(numCols);
    else 
      numCols = tableStyle->mCols;

    // for every column effected, set its width style
    PRInt32 colIndex=0;
    nsIFrame *colFrame=nsnull;
    nsIStyleContextPtr colStyleContext;
    ChildAt(aColIndex, colFrame);
    if (nsnull!=colFrame)
    {
      nsStylePosition * colPosition=nsnull;
      colFrame->GetStyleContext(aPresContext, colStyleContext.AssignRef());
      colPosition = (nsStylePosition*)colStyleContext->GetMutableStyleData(eStyleStruct_Position);
      nsStyleCoord width (1, eStyleUnit_Proportional);
      colPosition->mWidth = width;
      colStyleContext->RecalcAutomaticData(aPresContext);

      // if there are more columns, there width is set to "minimum"
      PRInt32 numChildFrames;
      ChildCount(numChildFrames);
      for (; aColIndex<numChildFrames-1; colIndex++)
      {
        ChildAt(colIndex, colFrame);
        if (nsnull==colFrame)
          break;
        nsStylePosition * colPosition=nsnull;
        colFrame->GetStyleContext(aPresContext, colStyleContext.AssignRef());
        colPosition = (nsStylePosition*)colStyleContext->GetMutableStyleData(eStyleStruct_Position);
        colPosition->mWidth.SetCoordValue(0);
        colStyleContext->RecalcAutomaticData(aPresContext);
      }
    }

    mStyleContext->RecalcAutomaticData(aPresContext);
  }
  return NS_OK;
}


/** returns the number of columns represented by this group.
  * if there are col children, count them (taking into account the span of each)
  * else, check my own span attribute.
  */
int nsTableColGroupFrame::GetColumnCount ()
{
  mColCount=0;
  int count;
  ChildCount (count);
  if (0 < count)
  {
    nsIFrame * child = nsnull;
    ChildAt(0, child);
    NS_ASSERTION(nsnull!=child, "bad child");
    while (nsnull!=child)
    {
      nsTableColFrame *col = (nsTableColFrame *)child;
      col->SetColumnIndex (mStartColIndex + mColCount);
      mColCount += col->GetRepeat ();
      child->GetNextSibling(child);
    }
  }
  else
  {
    const nsStyleTable *tableStyle;
    GetStyleData(eStyleStruct_Table, (nsStyleStruct *&)tableStyle);
    mColCount = tableStyle->mSpan;
  }
  return mColCount;
}

/* ----- global methods ----- */

nsresult 
NS_NewTableColGroupFrame(nsIContent* aContent,
                         nsIFrame*   aParentFrame,
                         nsIFrame*&  aResult)
{
  nsIFrame* it = new nsTableColGroupFrame(aContent, aParentFrame);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = it;
  return NS_OK;
}

/* ----- debugging methods ----- */
NS_METHOD nsTableColGroupFrame::List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const
{
  // if a filter is present, only output this frame if the filter says we should
  // since this could be any "tag" with the right display type, we'll
  // just pretend it's a colgroup
  if (nsnull==aFilter)
    return nsContainerFrame::List(out, aIndent, aFilter);

  nsAutoString tagString("colgroup");
  PRBool outputMe = aFilter->OutputTag(&tagString);
  if (PR_TRUE==outputMe)
  {
    // Indent
    for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);

    // Output the tag and rect
    nsIAtom* tag;
    mContent->GetTag(tag);
    if (tag != nsnull) {
      nsAutoString buf;
      tag->ToString(buf);
      fputs(buf, out);
      NS_RELEASE(tag);
    }
    PRInt32 contentIndex;

    GetContentIndex(contentIndex);
    fprintf(out, "(%d)", contentIndex);
    out << mRect;
    if (0 != mState) {
      fprintf(out, " [state=%08x]", mState);
    }
    fputs("\n", out);
  }
  // Output the children
  if (mChildCount > 0) {
    if (PR_TRUE==outputMe)
    {
      if (0 != mState) {
        fprintf(out, " [state=%08x]\n", mState);
      }
    }
    for (nsIFrame* child = mFirstChild; child; NextChild(child, child)) {
      child->List(out, aIndent + 1, aFilter);
    }
  } else {
    if (PR_TRUE==outputMe)
    {
      if (0 != mState) {
        fprintf(out, " [state=%08x]\n", mState);
      }
    }
  }
  return NS_OK;
}


