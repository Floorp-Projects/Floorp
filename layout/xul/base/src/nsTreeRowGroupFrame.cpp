/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsCOMPtr.h"
#include "nsXULAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIFrameReflow.h"
#include "nsTreeFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsTreeRowGroupFrame.h"
#include "nsIStyleContext.h"
#include "nsCSSFrameConstructor.h"
#include "nsIContent.h"
#include "nsCSSRendering.h"
#include "nsTreeCellFrame.h"
#include "nsCellMap.h"
#include "nsIReflowCommand.h"
#include "nsHTMLParts.h"
#include "nsScrollbarButtonFrame.h"
#include "nsSliderFrame.h"
#include "nsIDOMElement.h"
#include "nsISupportsArray.h"

//
// NS_NewTreeFrame
//
// Creates a new tree frame
//
nsresult
NS_NewTreeRowGroupFrame (nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTreeRowGroupFrame* it = new nsTreeRowGroupFrame;
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewTreeFrame


// Constructor
nsTreeRowGroupFrame::nsTreeRowGroupFrame()
:nsTableRowGroupFrame(), mScrollbar(nsnull), mFrameConstructor(nsnull),
 mTopFrame(nsnull), mBottomFrame(nsnull), mIsLazy(PR_FALSE), mIsFull(PR_FALSE), mContentChain(nsnull)
{ }

// Destructor
nsTreeRowGroupFrame::~nsTreeRowGroupFrame()
{
  NS_IF_RELEASE(mContentChain);
}

NS_IMPL_ADDREF(nsTreeRowGroupFrame)
NS_IMPL_RELEASE(nsTreeRowGroupFrame)
  
NS_IMETHODIMP
nsTreeRowGroupFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr) 
{
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(nsIScrollbarListener::GetIID())) {                                         
    *aInstancePtr = (void*)(nsIScrollbarListener*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }   

  return nsTableRowGroupFrame::QueryInterface(aIID, aInstancePtr);   
}

void nsTreeRowGroupFrame::DestroyRows(nsIPresContext& aPresContext, PRInt32& rowsToLose) 
{
  // We need to destroy frames until our row count has been properly
  // reduced.  A reflow will then pick up and create the new frames.
  nsIFrame* childFrame = GetFirstFrame();
  while (childFrame && rowsToLose > 0) {
    const nsStyleDisplay *childDisplay;
    childFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay)
    {
      PRInt32 rowGroupCount;
      ((nsTreeRowGroupFrame*)childFrame)->GetRowCount(rowGroupCount);
      if ((rowGroupCount - rowsToLose) > 0) {
        // The row group will destroy as many rows as it can, and it will
        // modify rowsToLose.
        ((nsTreeRowGroupFrame*)childFrame)->DestroyRows(aPresContext, rowsToLose);
        return;
      }
      else rowsToLose = 0;
    }
    else if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
    {
      // Lost a row.
      rowsToLose--;
    }
    
    nsIFrame* nextFrame;
    GetNextFrame(childFrame, &nextFrame);
    mFrames.DeleteFrame(aPresContext, childFrame);
    mTopFrame = childFrame = nextFrame;
  }
}

void nsTreeRowGroupFrame::ReverseDestroyRows(nsIPresContext& aPresContext, PRInt32& rowsToLose) 
{
  // We need to destroy frames until our row count has been properly
  // reduced.  A reflow will then pick up and create the new frames.
  nsIFrame* childFrame = GetLastFrame();
  while (childFrame && rowsToLose > 0) {
    const nsStyleDisplay *childDisplay;
    childFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay)
    {
      PRInt32 rowGroupCount;
      ((nsTreeRowGroupFrame*)childFrame)->GetRowCount(rowGroupCount);
      if ((rowGroupCount - rowsToLose) > 0) {
        // The row group will destroy as many rows as it can, and it will
        // modify rowsToLose.
        ((nsTreeRowGroupFrame*)childFrame)->ReverseDestroyRows(aPresContext, rowsToLose);
        return;
      }
      else rowsToLose = 0;
    }
    else if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
    {
      // Lost a row.
      rowsToLose--;
    }
    
    nsIFrame* prevFrame;
    prevFrame = mFrames.GetPrevSiblingFor(childFrame);
    mFrames.DeleteFrame(aPresContext, childFrame);
    mBottomFrame = childFrame = prevFrame;
  }
}

void 
nsTreeRowGroupFrame::ConstructContentChain(nsIContent* aRowContent)
{
  // Create the content chain array.
  NS_IF_RELEASE(mContentChain);
  NS_NewISupportsArray(&mContentChain);

  // Move up the chain until we hit our content node.
  nsCOMPtr<nsIContent> currContent = dont_QueryInterface(aRowContent);
  while (currContent && (currContent.get() != mContent)) {
    mContentChain->InsertElementAt(currContent, 0);
    currContent->GetParent(*getter_AddRefs(currContent));
  }

  NS_ASSERTION(currContent.get() == mContent, "Disaster! Content not contained in our tree!\n");
}

void 
nsTreeRowGroupFrame::GetFirstRowContent(nsIContent** aResult)
{
  *aResult = nsnull;
  nsIFrame* kid = GetFirstFrame();
  while (kid) {
    const nsStyleDisplay *childDisplay;
    kid->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay)
    {
      ((nsTreeRowGroupFrame*)kid)->GetFirstRowContent(aResult);
      if (*aResult)
        return;
    }
    else if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
    {
      kid->GetContent(aResult); // The ADDREF happens here.
      return;
    }
    GetNextFrame(kid, &kid);
  }
}

void 
nsTreeRowGroupFrame::FindPreviousRowContent(PRInt32& aDelta, nsIContent* aUpwardHint, 
                                            nsIContent* aDownwardHint, nsIContent** aResult)
{
  // Init to nsnull.
  *aResult = nsnull;

  // It disappoints me that this function is completely tied to the content nodes,
  // but I can't see any other way to handle this.  I don't have the frames, so I have nothing
  // else to fall back on but the content nodes.
  PRInt32 index = 0;
  nsCOMPtr<nsIContent> parentContent;
  if (aUpwardHint) {
    aUpwardHint->GetParent(*getter_AddRefs(parentContent));
    parentContent->IndexOf(aUpwardHint, index);
  }
  else if (aDownwardHint) {
    parentContent = dont_QueryInterface(aDownwardHint);
    parentContent->ChildCount(index);
  }

  nsIAtom* aAtom;
  parentContent->GetTag(aAtom);
  nsString result;
  aAtom->ToString(result);
  
  for (PRInt32 i = index-1; i >= 0; i--) {
    nsCOMPtr<nsIContent> childContent;
    parentContent->ChildAt(i, *getter_AddRefs(childContent));
    nsCOMPtr<nsIAtom> tag;
    childContent->GetTag(*getter_AddRefs(tag));
    if (tag.get() == nsXULAtoms::treerow) {
      aDelta--;
      if (aDelta == 0) {
        *aResult = childContent;
        NS_IF_ADDREF(*aResult);
        return;
      }
    }
    else if (tag.get() == nsXULAtoms::treeitem) {
      // If it's open, descend into its treechildren node first.
      nsCOMPtr<nsIAtom> openAtom = dont_AddRef(NS_NewAtom("open"));
      nsString isOpen;
      childContent->GetAttribute(kNameSpaceID_None, openAtom, isOpen);
      if (isOpen == "true") {
        // Find the <treechildren> node.
        PRInt32 childContentCount;
        nsCOMPtr<nsIContent> grandChild;
        childContent->ChildCount(childContentCount);

        PRInt32 j;
        for (j = childContentCount-1; j >= 0; j--) {
          
          childContent->ChildAt(j, *getter_AddRefs(grandChild));
          nsCOMPtr<nsIAtom> grandChildTag;
          grandChild->GetTag(*getter_AddRefs(grandChildTag));
          if (grandChildTag.get() == nsXULAtoms::treechildren)
            break;
        }
        if (j >= 0 && grandChild)
          FindPreviousRowContent(aDelta, nsnull, grandChild, aResult);
      
        if (aDelta == 0)
          return;
      }

      // Descend into this row group and try to find a previous row.
      FindPreviousRowContent(aDelta, nsnull, childContent, aResult);
      if (aDelta == 0)
        return;
    }
  }

  // We didn't find it here. We need to go up to our parent, using ourselves as a hint.
  FindPreviousRowContent(aDelta, parentContent, nsnull, aResult);

  // Bail. There's nothing else we can do.
}

NS_IMETHODIMP
nsTreeRowGroupFrame::PositionChanged(nsIPresContext& aPresContext, PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  if (aOldIndex == aNewIndex)
    return NS_OK;

  printf("The position changed!\n");
  
  // Get our row count.
  PRInt32 rowCount;
  GetRowCount(rowCount);

  // Get our owning tree.
  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);

  // Figure out how many rows we need to lose (if we moved down) or gain (if we moved up).
  PRInt32 delta = aNewIndex > aOldIndex ? aNewIndex - aOldIndex : aOldIndex - aNewIndex;
  
  // Get our presentation context.
  if (delta < rowCount) {
    PRInt32 loseRows = delta;
    if (aNewIndex > aOldIndex) {
      // Figure out how many rows we have to lose off the top.
      DestroyRows(aPresContext, loseRows);
    }
    else {
      // Get our first row content.
      nsCOMPtr<nsIContent> rowContent;
      GetFirstRowContent(getter_AddRefs(rowContent));

      // Figure out how many rows we have to lose off the bottom.
      ReverseDestroyRows(aPresContext, loseRows);
    
      // Now that we've lost some rows, we need to create a
      // content chain that provides a hint for moving forward.
      nsCOMPtr<nsIContent> topRowContent;
      FindPreviousRowContent(delta, rowContent, nsnull, getter_AddRefs(topRowContent));
      ConstructContentChain(topRowContent);
    }
  }
  else {
    // Just blow away all our frames, but keep a content chain
    // as a hint to figure out how to build the frames.
    NS_ERROR("Not yet implemented!\n");
    //mFrames.DeleteFrames(aPresContext); // Destroys everything.
  }

  // Invalidate the cell map and column cache.
  tableFrame->InvalidateCellMap();
  tableFrame->InvalidateColumnCache();
    
  return NS_OK;
}

NS_IMETHODIMP
nsTreeRowGroupFrame::PagedUpDown()
{
  printf("Hey! You paged up and down!\n");
  return NS_OK;
}

void
nsTreeRowGroupFrame::SetScrollbarFrame(nsIFrame* aFrame)
{
  mIsLazy = PR_TRUE;
  mScrollbar = aFrame;
  nsCOMPtr<nsIAtom> sliderAtom = dont_AddRef(NS_NewAtom("slider"));
  nsCOMPtr<nsIAtom> incrementAtom = dont_AddRef(NS_NewAtom("increment"));
  nsCOMPtr<nsIAtom> pageIncrementAtom = dont_AddRef(NS_NewAtom("pageincrement"));
  
  nsCOMPtr<nsIContent> scrollbarContent;
  aFrame->GetContent(getter_AddRefs(scrollbarContent));
  
  scrollbarContent->SetAttribute(kNameSpaceID_None, incrementAtom, "1", PR_FALSE);
  scrollbarContent->SetAttribute(kNameSpaceID_None, pageIncrementAtom, "1", PR_FALSE);

  nsIFrame* result;
  nsScrollbarButtonFrame::GetChildWithTag(sliderAtom, aFrame, result);
  ((nsSliderFrame*)result)->SetScrollbarListener(this);
}

PRBool nsTreeRowGroupFrame::RowGroupDesiresExcessSpace() 
{
  nsIFrame* parentFrame;
  GetParent(&parentFrame);
  const nsStyleDisplay *display;
  parentFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));
  if (display->mDisplay == NS_STYLE_DISPLAY_TABLE_ROW_GROUP)
    return PR_FALSE; // Nested groups don't grow.
  else return PR_TRUE; // The outermost group can grow.
}

PRBool nsTreeRowGroupFrame::RowGroupReceivesExcessSpace()
{
  const nsStyleDisplay *display;
  GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));
  if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == display->mDisplay)
    return PR_TRUE;
  else return PR_FALSE; // Headers and footers don't get excess space.
}

NS_IMETHODIMP
nsTreeRowGroupFrame::GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame)
{
  nsPoint tmp;
  nsRect kidRect;
  if (mScrollbar) {
    mScrollbar->GetRect(kidRect);
    if (kidRect.Contains(aPoint)) {
      tmp.MoveTo(aPoint.x - kidRect.x, aPoint.y - kidRect.y);
      return mScrollbar->GetFrameForPoint(tmp, aFrame);
    }
  }

  return nsTableRowGroupFrame::GetFrameForPoint(aPoint, aFrame);
}

void nsTreeRowGroupFrame::PaintChildren(nsIPresContext&      aPresContext,
                                         nsIRenderingContext& aRenderingContext,
                                         const nsRect&        aDirtyRect,
                                         nsFramePaintLayer    aWhichLayer)
{
  nsTableRowGroupFrame::PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

  if (mScrollbar) {
    nsIView *pView;
     
    mScrollbar->GetView(&pView);
    if (nsnull == pView) {
      PRBool clipState;
      nsRect kidRect;
      mScrollbar->GetRect(kidRect);
      nsRect damageArea(aDirtyRect);
      // Translate damage area into kid's coordinate system
      nsRect kidDamageArea(damageArea.x - kidRect.x, damageArea.y - kidRect.y,
                           damageArea.width, damageArea.height);
      aRenderingContext.PushState();
      aRenderingContext.Translate(kidRect.x, kidRect.y);
      mScrollbar->Paint(aPresContext, aRenderingContext, kidDamageArea, aWhichLayer);
      if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) &&
          GetShowFrameBorders()) {
        aRenderingContext.SetColor(NS_RGB(255,0,0));
        aRenderingContext.DrawRect(0, 0, kidRect.width, kidRect.height);
      }
      aRenderingContext.PopState(clipState);
    }
  }
}

NS_IMETHODIMP 
nsTreeRowGroupFrame::ReflowBeforeRowLayout(nsIPresContext&      aPresContext,
                                           nsHTMLReflowMetrics& aDesiredSize,
                                           RowGroupReflowState& aReflowState,
                                           nsReflowStatus&      aStatus,
                                           nsReflowReason       aReason)
{
  nsresult rv = NS_OK;
  // Reflow a scrollbar if we have one.
  if (mScrollbar && (aReflowState.availSize.height != NS_UNCONSTRAINEDSIZE)) {
    // We must be constrained, or a scrollbar makes no sense.
    nsSize    kidMaxElementSize;
    nsSize*   pKidMaxElementSize = (nsnull != aDesiredSize.maxElementSize) ? &kidMaxElementSize : nsnull;
  
    nsSize kidAvailSize(aReflowState.availSize);
    nsHTMLReflowMetrics desiredSize(pKidMaxElementSize);
    desiredSize.width=desiredSize.height=desiredSize.ascent=desiredSize.descent=0;

    // Reflow the child into the available space, giving it as much width as it
    // wants, but constraining its height.
    kidAvailSize.width = NS_UNCONSTRAINEDSIZE;
    nsHTMLReflowState kidReflowState(aPresContext, aReflowState.reflowState, mScrollbar,
                                     kidAvailSize, aReason);
    
    kidReflowState.computedHeight = kidAvailSize.height;
    rv = ReflowChild(mScrollbar, aPresContext, desiredSize, kidReflowState, aStatus);
    if (NS_FAILED(rv))
      return rv;

    nscoord xpos = 0;

    // Lose the width of the scrollbar as far as the rows are concerned.
    if (aReflowState.availSize.width != NS_UNCONSTRAINEDSIZE) {
      xpos = aReflowState.availSize.width - desiredSize.width;
      /*aReflowState.availSize.width -= desiredSize.width;
      if (aReflowState.availSize.width < 0)
        aReflowState.availSize.width = 0;*/ 
    }

    // Place the child
    nsRect kidRect (xpos, 0, desiredSize.width, aReflowState.availSize.height);
    mScrollbar->SetRect(kidRect);
  }

  return rv;
}


PRBool nsTreeRowGroupFrame::ExcludeFrameFromReflow(nsIFrame* aFrame)
{
  if (aFrame == mScrollbar)
    return PR_TRUE;
  else return PR_FALSE;
}

void nsTreeRowGroupFrame::LocateFrame(nsIFrame* aStartFrame, nsIFrame** aResult)
{
  if (aStartFrame == nsnull)
  {
    aStartFrame = mFrames.FirstChild();
  }
  else aStartFrame->GetNextSibling(&aStartFrame);

  if (!aStartFrame) {
    *aResult = nsnull;
  } else if (aStartFrame != mScrollbar) {
    *aResult = aStartFrame;
  } else {
    aStartFrame->GetNextSibling(&aStartFrame);
    *aResult = aStartFrame;
  }
}
   
nsIFrame*
nsTreeRowGroupFrame::GetFirstFrame()
{
  // We may just be a normal row group.
  if (!mIsLazy)
    return mFrames.FirstChild();

  LocateFrame(nsnull, &mTopFrame);
  return mTopFrame;
}

nsIFrame*
nsTreeRowGroupFrame::GetLastFrame()
{
  // For now just return the one on the end.
  return mFrames.LastChild();
}

void
nsTreeRowGroupFrame::GetNextFrame(nsIFrame* aPrevFrame, nsIFrame** aResult)
{
  if (!mIsLazy)
    aPrevFrame->GetNextSibling(aResult);
  else LocateFrame(aPrevFrame, aResult);
}

nsIFrame* 
nsTreeRowGroupFrame::GetFirstFrameForReflow(nsIPresContext& aPresContext) 
{ 
  // Clear ourselves out.
  mBottomFrame = mTopFrame;
  mIsFull = PR_FALSE;

  // We may just be a normal row group.
  if (!mIsLazy)
    return mFrames.FirstChild();

  if (mTopFrame)
    return mTopFrame;

  // See if we have any frame whatsoever.
  LocateFrame(nsnull, &mTopFrame);
  
  mBottomFrame = mTopFrame;

  if (mTopFrame)
    return mTopFrame;

  // We don't have a top frame instantiated. Let's
  // try to make one.

  // If we have a content chain, use that content node to make our frame,
  // and prepare a sub-content chain for the new child frame that we make.
  
  PRInt32 childCount;
  mContent->ChildCount(childCount);
  nsCOMPtr<nsIContent> childContent;
  if (childCount > 0) {
    mContent->ChildAt(0, *getter_AddRefs(childContent));
    mFrameConstructor->CreateTreeWidgetContent(&aPresContext, this, childContent,
                                               &mTopFrame);
    printf("Created a frame\n");
    mBottomFrame = mTopFrame;
    const nsStyleDisplay *rowDisplay;
    mTopFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)rowDisplay);
    if (NS_STYLE_DISPLAY_TABLE_ROW==rowDisplay->mDisplay) {
      ((nsTableRowFrame *)mTopFrame)->InitChildren();
    }
    return mTopFrame;
  }
  
  return nsnull;
}
  
void 
nsTreeRowGroupFrame::GetNextFrameForReflow(nsIPresContext& aPresContext, nsIFrame* aFrame, nsIFrame** aResult) 
{ 
  if (mIsLazy) {
    // We're ultra-cool. We build our frames on the fly.
    LocateFrame(aFrame, aResult);
    if (!*aResult) {
      // No result found. See if there's a content node that wants a frame.
      PRInt32 i, childCount;
      nsCOMPtr<nsIContent> prevContent;
      aFrame->GetContent(getter_AddRefs(prevContent));
      nsCOMPtr<nsIContent> parentContent;
      mContent->IndexOf(prevContent, i);
      mContent->ChildCount(childCount);
      if (i+1 < childCount) {
        // There is a content node that wants a frame.
        nsCOMPtr<nsIContent> nextContent;
        mContent->ChildAt(i+1, *getter_AddRefs(nextContent));
        mFrameConstructor->CreateTreeWidgetContent(&aPresContext, this, nextContent,
                                                   aResult);
        mBottomFrame = *aResult;
        printf("Created a frame\n");
        const nsStyleDisplay *rowDisplay;
        mBottomFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)rowDisplay);
        if (NS_STYLE_DISPLAY_TABLE_ROW==rowDisplay->mDisplay) {
          ((nsTableRowFrame *)mBottomFrame)->InitChildren();
        }
      }
    }
    return;
  }
  
  // Ho-hum. Move along, nothing to see here.
  aFrame->GetNextSibling(aResult);
}

NS_IMETHODIMP
nsTreeRowGroupFrame::TreeInsertFrames(nsIFrame* aPrevFrame, nsIFrame* aFrameList)
{
  mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeRowGroupFrame::TreeAppendFrames(nsIFrame* aFrameList)
{
  mFrames.AppendFrames(nsnull, aFrameList);
  return NS_OK;
}

PRBool nsTreeRowGroupFrame::ContinueReflow(nscoord y, nscoord height) 
{ 
  //printf("Y is: %d\n", y);
  //printf("Height is: %d\n", height); 
  if (height <= 0 && IsLazy()) {
    mIsFull = PR_TRUE;
    return PR_FALSE;
  }
  else
    return PR_TRUE;
}
  
// Responses to changes
void nsTreeRowGroupFrame::OnContentAdded(nsIPresContext& aPresContext) 
{
  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);

  nsTreeFrame* treeFrame = (nsTreeFrame*)tableFrame;

  if (IsLazy() && !treeFrame->IsSlatedForReflow()) {
    treeFrame->SlateForReflow();

    // Schedule a reflow for us.
    nsCOMPtr<nsIReflowCommand> reflowCmd;
    
    nsresult rv = NS_NewHTMLReflowCommand(getter_AddRefs(reflowCmd), treeFrame,
                                          nsIReflowCommand::FrameAppended, nsnull);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIPresShell> presShell;
      aPresContext.GetShell(getter_AddRefs(presShell));
      presShell->AppendReflowCommand(reflowCmd);
    }
  }
}
