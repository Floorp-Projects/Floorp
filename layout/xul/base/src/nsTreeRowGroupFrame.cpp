/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsCOMPtr.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsTreeFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsTreeRowGroupFrame.h"
#include "nsIStyleContext.h"
#include "nsCSSFrameConstructor.h"
#include "nsIContent.h"
#include "nsIXULContent.h"
#include "nsCSSRendering.h"
#include "nsTreeCellFrame.h"
#include "nsCellMap.h"
#include "nsIReflowCommand.h"
#include "nsHTMLParts.h"
#include "nsScrollbarButtonFrame.h"
#include "nsSliderFrame.h"
#include "nsIDOMElement.h"
#include "nsISupportsArray.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMDragListener.h"
#include "nsTreeItemDragCapturer.h"
#include "nsLayoutAtoms.h"
#include "nsIViewManager.h"
#include "nsIView.h"

// XXX This should probably be based off the height of a row in pixels
#define SCROLL_FACTOR 16 

// I added the following function to improve keeping the frame 
// chains in synch with the table. repackage as appropriate - karnaze
void GetRowStartAndCount(nsIFrame* aFrame,
                         PRInt32&  aStartRowIndex,
                         PRInt32&  aNumRows)
{
  aStartRowIndex = -1; 
  aNumRows = 0;
  nsIAtom* fType;
  aFrame->GetFrameType(&fType);
  if (nsLayoutAtoms::tableRowFrame == fType) {
    aStartRowIndex = ((nsTableRowFrame*)aFrame)->GetRowIndex();
    aNumRows = 1;
  }
  else if (nsLayoutAtoms::tableRowGroupFrame == fType) { 
    aStartRowIndex = ((nsTableRowGroupFrame*)aFrame)->GetStartRowIndex();
    ((nsTableRowGroupFrame*)aFrame)->GetRowCount(aNumRows);
  }
  NS_IF_RELEASE(fType);
}

// define this to get some help
#undef DEBUG_tree

//
// NS_NewTreeFrame
//
// Creates a new tree frame
//
nsresult
NS_NewTreeRowGroupFrame (nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTreeRowGroupFrame* it = new (aPresShell) nsTreeRowGroupFrame;
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewTreeFrame


// Constructor
nsTreeRowGroupFrame::nsTreeRowGroupFrame()
: nsTableRowGroupFrame(), mTopFrame(nsnull), mBottomFrame(nsnull),
  mLinkupFrame(nsnull), mIsFull(PR_FALSE),
  mScrollbar(nsnull), mOuterFrame(nsnull),
  mContentChain(nsnull), mFrameConstructor(nsnull),
  mRowGroupHeight(0), mCurrentIndex(0), mRowCount(0),
  mYDropLoc(-1), mDropOnContainer(PR_FALSE)
{ }

// Destructor
nsTreeRowGroupFrame::~nsTreeRowGroupFrame()
{
  nsCOMPtr<nsIContent> content;
  GetContent(getter_AddRefs(content));
  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(content));

  // NOTE: the last Remove will delete the drag capturer
  if ( receiver ) {
    receiver->RemoveEventListener("dragover", mDragCapturer, PR_TRUE);
    receiver->RemoveEventListener("dragexit", mDragCapturer, PR_TRUE);
  }
  
  NS_IF_RELEASE(mContentChain);
}

NS_IMETHODIMP
nsTreeRowGroupFrame::Destroy(nsIPresContext* aPresContext)
{
  if (mScrollbar) {
    mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, mScrollbar, nsnull);
    mScrollbar->Destroy(aPresContext);
  }
  return nsTableRowGroupFrame::Destroy(aPresContext);
}

nsrefcnt nsTreeRowGroupFrame::AddRef(void)
{
  return 1;
}

nsrefcnt nsTreeRowGroupFrame::Release(void)
{
  return 1;
}
  
NS_IMETHODIMP
nsTreeRowGroupFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr) 
{
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(NS_GET_IID(nsIScrollbarListener))) {                                         
    *aInstancePtr = (void*)(nsIScrollbarListener*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }   

  return nsTableRowGroupFrame::QueryInterface(aIID, aInstancePtr);   
}


//
// Init
//
// Setup event capturers for drag and drop. Our frame's lifetime is bounded by the
// lifetime of the content model, so we're guaranteed that the content node won't go away on us. As
// a result, our drag capturer can't go away before the frame is deleted. Since the content
// node holds owning references to our drag capturer, which we tear down in the dtor, there is no 
// need to hold an owning ref to it ourselves.
//
NS_IMETHODIMP
nsTreeRowGroupFrame::Init ( nsIPresContext*  aPresContext, nsIContent* aContent,
                             nsIFrame* aParent, nsIStyleContext* aContext, nsIFrame* aPrevInFlow)
{
  nsresult rv = nsTableRowGroupFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // Set our outer frame variable.
  // See what kind of frame we have
  const nsStyleDisplay *display;
  aParent->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));

  if ((NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == display->mDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == display->mDisplay) ||
      (NS_STYLE_DISPLAY_TABLE_ROW_GROUP    == display->mDisplay)) {
    nsTreeRowGroupFrame* rg = (nsTreeRowGroupFrame*)aParent;
    mOuterFrame = rg->mOuterFrame;  
  }
  else mOuterFrame = this;

  if ( NS_SUCCEEDED(rv) ) {
    nsCOMPtr<nsIContent> content;
    GetContent(getter_AddRefs(content));
    nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(content));

    // register our drag over and exit capturers. These annotate the content object
    // with enough info to determine where the drop would happen so that JS down the 
    // line can do the right thing.
    mDragCapturer = new nsTreeItemDragCapturer(this, aPresContext);
    receiver->AddEventListener("dragover", mDragCapturer, PR_TRUE);
    receiver->AddEventListener("dragexit", mDragCapturer, PR_TRUE);
  }
  
  return rv;

} // Init


void nsTreeRowGroupFrame::DestroyRows(nsTableFrame* aTableFrame, nsIPresContext* aPresContext, PRInt32& rowsToLose) 
{
  // We need to destroy frames until our row count has been properly
  // reduced.  A reflow will then pick up and create the new frames.
  nsIFrame* childFrame = GetFirstFrame();
  while (childFrame && rowsToLose > 0) {
    PRBool rowIndex = -1; 
    if (IsTableRowGroupFrame(childFrame))
    {
      PRInt32 rowGroupCount;
      ((nsTreeRowGroupFrame*)childFrame)->GetRowCount(rowGroupCount);
      if ((rowGroupCount - rowsToLose) > 0) {
        // The row group will destroy as many rows as it can, and it will
        // modify rowsToLose.
        ((nsTreeRowGroupFrame*)childFrame)->DestroyRows(aTableFrame, aPresContext, rowsToLose);
        return;
      }
      else 
        ((nsTreeRowGroupFrame*)childFrame)->DestroyRows(aTableFrame, aPresContext, rowsToLose);
    }
    else if (IsTableRowFrame(childFrame))
    {
      // Lost a row.
      rowsToLose--;
      rowIndex = ((nsTableRowFrame*)childFrame)->GetRowIndex();
    }
    
    nsIFrame* nextFrame;
    GetNextFrame(childFrame, &nextFrame);
    mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, childFrame, nsnull);
    mFrames.DestroyFrame(aPresContext, childFrame);
    // remove the row from the table after it is removed from the sibling chain
    if (rowIndex >= 0) {
      aTableFrame->RemoveRows(*aPresContext, rowIndex, 1, PR_FALSE);
      aTableFrame->InvalidateColumnWidths();
    }
    mTopFrame = childFrame = nextFrame;
  }
}

void nsTreeRowGroupFrame::ReverseDestroyRows(nsTableFrame* aTableFrame, nsIPresContext* aPresContext, PRInt32& rowsToLose) 
{
  // We need to destroy frames until our row count has been properly
  // reduced.  A reflow will then pick up and create the new frames.
  nsIFrame* childFrame = GetLastFrame();
  while (childFrame && rowsToLose > 0) {
    PRInt32 rowIndex = -1;
    if (IsTableRowGroupFrame(childFrame))
    {
      PRInt32 rowGroupCount;
      ((nsTreeRowGroupFrame*)childFrame)->GetRowCount(rowGroupCount);
      if ((rowGroupCount - rowsToLose) > 0) {
        // The row group will destroy as many rows as it can, and it will
        // modify rowsToLose.
        ((nsTreeRowGroupFrame*)childFrame)->ReverseDestroyRows(aTableFrame, aPresContext, rowsToLose);
        return;
      }
      else
        ((nsTreeRowGroupFrame*)childFrame)->ReverseDestroyRows(aTableFrame, aPresContext, rowsToLose);
    }
    else if (IsTableRowFrame(childFrame))
    {
      // Lost a row.
      rowsToLose--;
      rowIndex = ((nsTableRowFrame*)childFrame)->GetRowIndex();
    }
    
    nsIFrame* prevFrame;
    prevFrame = mFrames.GetPrevSiblingFor(childFrame);
    mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, childFrame, nsnull);
    mFrames.DestroyFrame(aPresContext, childFrame);
    // remove the row from the table after it is removed from the sibling chain
    if (rowIndex >= 0) {
      aTableFrame->RemoveRows(*aPresContext, rowIndex, 1, PR_FALSE);
      aTableFrame->InvalidateColumnWidths();
    }
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
    nsCOMPtr<nsIContent> otherContent = currContent;
    otherContent->GetParent(*getter_AddRefs(currContent));
  }

  NS_ASSERTION(currContent.get() == mContent, "Disaster! Content not contained in our tree!\n");
}

void 
nsTreeRowGroupFrame::ConstructOldContentChain(nsIPresContext* aPresContext,
                                              nsIContent* aOldRowContent)
{
	nsCOMPtr<nsIContent> childOfCommonAncestor;

	//Find the first child of the common ancestor between the new top row's content chain
	//and the old top row.  Everything between this child and the old top row potentially need
	//to have their content chains reset.
	FindChildOfCommonContentChainAncestor(aOldRowContent, getter_AddRefs(childOfCommonAncestor));

	if(childOfCommonAncestor)
	{
      //Set up the old top rows content chian.
	  CreateOldContentChain(aPresContext, aOldRowContent, childOfCommonAncestor);
	}
}

void
nsTreeRowGroupFrame::FindChildOfCommonContentChainAncestor(nsIContent *startContent, nsIContent **child)
{

	PRUint32 count;

	if(mContentChain)
	{
	  nsresult rv = mContentChain->Count(&count);

	  if(NS_SUCCEEDED(rv) && (count >0))
	  {
	    for(PRInt32 curItem = count - 1; curItem >= 0; curItem--)
		{
		  nsCOMPtr<nsISupports> supports;
          mContentChain->GetElementAt(curItem, getter_AddRefs(supports));
          nsCOMPtr<nsIContent> curContent = do_QueryInterface(supports);

		  //See if curContent is an ancestor of startContent.
		  if(IsAncestor(curContent, startContent, child))
		  {
			return;
		  }
		}
	  }
	}

	//mContent isn't actually put in the content chain, so we need to
	//check it separately.
	if(IsAncestor(mContent, startContent, child))
		return;

	*child = nsnull;
}


// if oldRowContent is an ancestor of rowContent, return true,
// and return the previous ancestor if requested
PRBool
nsTreeRowGroupFrame::IsAncestor(nsIContent *aRowContent, nsIContent *aOldRowContent, nsIContent** firstDescendant)
{
  nsCOMPtr<nsIContent> prevContent;	
  nsCOMPtr<nsIContent> currContent = dont_QueryInterface(aOldRowContent);
  
  while (currContent) {

	if(aRowContent == currContent.get())
	{
		if(firstDescendant)
		{
		  *firstDescendant = prevContent;
		  NS_IF_ADDREF(*firstDescendant);
		}
		return PR_TRUE;
	}
    
	prevContent = currContent;
	prevContent->GetParent(*getter_AddRefs(currContent));
  }

  return PR_FALSE;
}

PRBool
nsTreeRowGroupFrame::IsTableRowGroupFrame(nsIFrame *frame)
{
    if (!frame) return PR_FALSE;
    
    const nsStyleDisplay *display;
    frame->GetStyleData(eStyleStruct_Display,
                        (const nsStyleStruct*&)display);

    return (display->mDisplay == NS_STYLE_DISPLAY_TABLE_ROW_GROUP);
}

PRBool
nsTreeRowGroupFrame::IsTableRowFrame(nsIFrame *frame)
{
    if (!frame) return PR_FALSE;
    
    const nsStyleDisplay *display;
    frame->GetStyleData(eStyleStruct_Display,
                        (const nsStyleStruct*&)display);

    return (display->mDisplay == NS_STYLE_DISPLAY_TABLE_ROW);
}

void
nsTreeRowGroupFrame::CreateOldContentChain(nsIPresContext* aPresContext, nsIContent* aOldRowContent, nsIContent* topOfChain)
{
  nsCOMPtr<nsIContent> currContent = dont_QueryInterface(aOldRowContent);
  nsCOMPtr<nsIContent> prevContent;

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));

  //For each item between the (oldtoprow) and
  // (the new first child of common ancestry between new top row and old top row)
  // we need to see if the content chain has to be reset.
  while(currContent.get() != topOfChain)
  {
    nsIFrame* primaryFrame = nsnull;
    shell->GetPrimaryFrameFor(currContent, &primaryFrame);
      
    if(primaryFrame)
	{
	  if (IsTableRowGroupFrame(primaryFrame))
	  {
		//Get the current content's parent's first child
		nsCOMPtr<nsIContent> parent;
		currContent->GetParent(*getter_AddRefs(parent));
		
		nsCOMPtr<nsIContent> firstChild;
		parent->ChildAt(0, *getter_AddRefs(firstChild));

        nsTreeRowGroupFrame *curRowGroup = (nsTreeRowGroupFrame*)primaryFrame;
		nsIFrame *parentFrame;
		curRowGroup->GetParent(&parentFrame);

	    if (IsTableRowGroupFrame(parentFrame))
		{
           //Get the current content's parent's first frame.
           nsTreeRowGroupFrame *parentRowGroupFrame =
               (nsTreeRowGroupFrame*)parentFrame;
		   nsIFrame *currentTopFrame = parentRowGroupFrame->GetFirstFrame();

			nsCOMPtr<nsIContent> topContent;
			currentTopFrame->GetContent(getter_AddRefs(topContent));

			// If the current content's parent's first child is different
            // than the current frame's parent's first child then we know
            // they are out of synch and we need to set the content
            // chain correctly.
			if(topContent.get() != firstChild.get())
			{

				nsCOMPtr<nsISupportsArray> contentChain;
				NS_NewISupportsArray(getter_AddRefs(contentChain));

				contentChain->InsertElementAt(firstChild, 0);

				parentRowGroupFrame->SetContentChain(contentChain);
			}
		}
	  }
	}
	prevContent = currContent;
	prevContent->GetParent(*getter_AddRefs(currContent));
  }
}

void 
nsTreeRowGroupFrame::GetFirstRowContent(nsIContent** aResult)
{
  *aResult = nsnull;
  nsIFrame* kid = GetFirstFrame();
  while (kid) {
    if (IsTableRowGroupFrame(kid))
    {
      ((nsTreeRowGroupFrame*)kid)->GetFirstRowContent(aResult);
      if (*aResult)
        return;
    }
    else if (IsTableRowFrame(kid))
    {
      kid->GetContent(aResult); // The ADDREF happens here.
      return;
    }
    GetNextFrame(kid, &kid);
  }
}

void
nsTreeRowGroupFrame::FindRowContentAtIndex(PRInt32& aIndex,
                                           nsIContent* aParent,
                                           nsIContent** aResult)
{
  // Init to nsnull.
  *aResult = nsnull;

  // It disappoints me that this function is completely tied to the content nodes,
  // but I can't see any other way to handle this.  I don't have the frames, so I have nothing
  // else to fall back on but the content nodes.

  PRInt32 childCount;
  aParent->ChildCount(childCount);

  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> childContent;
    aParent->ChildAt(i, *getter_AddRefs(childContent));
    nsCOMPtr<nsIAtom> tag;
    childContent->GetTag(*getter_AddRefs(tag));

    // treerow - Count this row, if we're at 0, then we've found the row.
    if (tag.get() == nsXULAtoms::treerow) {
      aIndex--;
      if (aIndex < 0) {
        *aResult = childContent;
        NS_IF_ADDREF(*aResult);
        return;
      }
    }
    else if (tag.get() == nsXULAtoms::treeitem) {
      // Descend into this row group and try to find the next row.
      FindRowContentAtIndex(aIndex, childContent, aResult);
      if (aIndex < 0)
        return;

      // If it's open, descend into its treechildren.
      nsCOMPtr<nsIAtom> openAtom = dont_AddRef(NS_NewAtom("open"));
      nsAutoString isOpen;
      childContent->GetAttribute(kNameSpaceID_None, openAtom, isOpen);
      if (isOpen.Equals("true")) {
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
          FindRowContentAtIndex(aIndex, grandChild, aResult);
      
        if (aIndex < 0)
          return;
      }
    }
  }
}

void 
nsTreeRowGroupFrame::FindPreviousRowContent(PRInt32& aDelta, nsIContent* aUpwardHint, 
                                            nsIContent* aDownwardHint,
                                            nsIContent** aResult)
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
    if (!parentContent) {
      NS_ERROR("Parent content should not be NULL!");
      return;
    }
    parentContent->IndexOf(aUpwardHint, index);
  }
  else if (aDownwardHint) {
    parentContent = dont_QueryInterface(aDownwardHint);
    parentContent->ChildCount(index);
  }

  /* Let me see inside the damn nsCOMptrs
  nsIAtom* aAtom;
  parentContent->GetTag(aAtom);
  nsAutoString result;
  aAtom->ToString(result);
  */

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
      nsAutoString isOpen;
      childContent->GetAttribute(kNameSpaceID_None, openAtom, isOpen);
      if (isOpen.Equals("true")) {
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

  nsCOMPtr<nsIAtom> tag;
  parentContent->GetTag(*getter_AddRefs(tag));
  if (tag && tag.get() == nsXULAtoms::tree) {
    // Hopeless. It ain't in there.
    return;
  }
  else if (!aDownwardHint) // We didn't find it here. We need to go up to our parent, using ourselves as a hint.
    FindPreviousRowContent(aDelta, parentContent, nsnull, aResult);

  // Bail. There's nothing else we can do.
}

void
nsTreeRowGroupFrame::ComputeTotalRowCount(PRInt32& aCount, nsIContent* aParent)
{
  // XXX Check for visibility collapse and for display none on all objects.
  // ARGH!
  PRInt32 childCount;
  aParent->ChildCount(childCount);

  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> childContent;
    aParent->ChildAt(i, *getter_AddRefs(childContent));
    nsCOMPtr<nsIAtom> tag;
    childContent->GetTag(*getter_AddRefs(tag));
    if (tag.get() == nsXULAtoms::treerow) {
      aCount++;
    }
    else if (tag.get() == nsXULAtoms::treeitem) {
      // Descend into this row group and try to find the next row.
      ComputeTotalRowCount(aCount, childContent);
    }
    else if (tag.get() == nsXULAtoms::treechildren) {
      // If it's open, descend into its treechildren.
      nsCOMPtr<nsIAtom> openAtom = dont_AddRef(NS_NewAtom("open"));
      nsAutoString isOpen;
      nsCOMPtr<nsIContent> parent;
      childContent->GetParent(*getter_AddRefs(parent));
      parent->GetAttribute(kNameSpaceID_None, openAtom, isOpen);
      if (isOpen.Equals("true"))
      ComputeTotalRowCount(aCount, childContent);
    }
  }
}

NS_IMETHODIMP
nsTreeRowGroupFrame::PositionChanged(nsIPresContext* aPresContext, PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  PRInt32 oldIndex, newIndex;
  oldIndex = aOldIndex / SCROLL_FACTOR;
  newIndex = aNewIndex / SCROLL_FACTOR;

#ifdef DEBUG_tree
    printf("PositionChanged from %d to %d (mCurrentIndex is %d)\n",
           oldIndex, newIndex, mCurrentIndex);
#endif
  if (newIndex < 0)
    return NS_OK;

  if (oldIndex == newIndex)
    return NS_OK;

  mCurrentIndex = newIndex;

  // Get our row count.
  PRInt32 rowCount;
  GetRowCount(rowCount);

  // Get our owning tree.
  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);

  // Figure out how many rows we need to lose (if we moved down) or gain (if we moved up).
  PRInt32 delta = newIndex > oldIndex ? newIndex - oldIndex : oldIndex - newIndex;
  
#ifdef DEBUG_tree
  printf("Scrolling, the delta is: %d\n", delta);
#endif

  // Get our presentation context.
  if (delta < rowCount) {
    if (mContentChain) {
      // XXX This could cause problems because of async reflow.
      // Eventually we need to make the code smart enough to look at a content chain
      // when building ANOTHER content chain.
  
      // Ensure all reflows happen first.
      nsCOMPtr<nsIPresShell> shell;
      aPresContext->GetShell(getter_AddRefs(shell));
      shell->ProcessReflowCommands(PR_FALSE);
    }

    PRInt32 loseRows = delta;

    // scrolling down
    if (newIndex > oldIndex) {
      // Figure out how many rows we have to lose off the top.
      DestroyRows(tableFrame, aPresContext, loseRows);
    }
    // scrolling up
    else {
      // Get our first row content.
      nsCOMPtr<nsIContent> rowContent;
      GetFirstRowContent(getter_AddRefs(rowContent));

      // Figure out how many rows we have to lose off the bottom.
      ReverseDestroyRows(tableFrame, aPresContext, loseRows);
    
      // Now that we've lost some rows, we need to create a
      // content chain that provides a hint for moving forward.
      nsCOMPtr<nsIContent> topRowContent;

      FindPreviousRowContent(delta, rowContent, nsnull, getter_AddRefs(topRowContent));

      ConstructContentChain(topRowContent);
	    //Now construct the chain for the old top row so its content chain gets
	    //set up correctly.
	    ConstructOldContentChain(aPresContext, rowContent);
    }
  }
  else {
    // Just blow away all our frames, but keep a content chain
    // as a hint to figure out how to build the frames.
    // Remove the scrollbar first.
    // get the starting row index and row count
    PRInt32 startRowIndex, numRows;
    GetRowStartAndCount(this, startRowIndex, numRows);

    mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, this, nsnull);
    mFrames.DestroyFrames(aPresContext);

    // remove the rows from the table after removing from the sibling chain
    if ((startRowIndex >= 0) && (numRows > 0)) {
      tableFrame->RemoveRows(*aPresContext, startRowIndex, rowCount, PR_FALSE);
      tableFrame->InvalidateColumnWidths();
    }

    nsCOMPtr<nsIContent> topRowContent;
    FindRowContentAtIndex(newIndex, mContent, getter_AddRefs(topRowContent));

    if (topRowContent)
      ConstructContentChain(topRowContent);
  }

  mTopFrame = mBottomFrame = nsnull; // Make sure everything is cleared out.

  // Force a reflow.
  nsTreeFrame* treeFrame = (nsTreeFrame*) tableFrame;  
  MarkTreeAsDirty(aPresContext, treeFrame);

  return NS_OK;
}

NS_IMETHODIMP
nsTreeRowGroupFrame::PagedUpDown()
{
  // Set the scrollbar's pageincrement
  if (mScrollbar) {
    PRInt32 rowGroupCount;
    GetRowCount(rowGroupCount);
  
    nsCOMPtr<nsIContent> scrollbarContent;
    mScrollbar->GetContent(getter_AddRefs(scrollbarContent));

    rowGroupCount--;
    rowGroupCount *= SCROLL_FACTOR;
    char ch[100];
    sprintf(ch,"%d", rowGroupCount);
    
#ifdef DEBUG_tree
    printf("PagedUpDown, setting increment to %d\n", rowGroupCount);
#endif
    scrollbarContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::pageincrement, nsAutoString(ch), PR_FALSE);
  }

  return NS_OK;
}

void
nsTreeRowGroupFrame::SetScrollbarFrame(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  mScrollbar = aFrame;
  nsFrameList frameList(mScrollbar);
  
  // Place it in its own list.
  mScrollbarList.AppendFrames(this,frameList);

  nsCOMPtr<nsIContent> scrollbarContent;
  aFrame->GetContent(getter_AddRefs(scrollbarContent));
  
  nsAutoString scrollFactor;
  scrollFactor.Append(SCROLL_FACTOR);

  scrollbarContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::curpos, "0", PR_FALSE);
  scrollbarContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::increment, scrollFactor, PR_FALSE);
  scrollbarContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::pageincrement, "1", PR_FALSE);
  scrollbarContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::maxpos, "5000", PR_FALSE);

  nsIFrame* result;
  nsScrollbarButtonFrame::GetChildWithTag(aPresContext, nsXULAtoms::slider, aFrame, result);
  ((nsSliderFrame*)result)->SetScrollbarListener(this);
}

PRBool nsTreeRowGroupFrame::RowGroupDesiresExcessSpace() 
{
  nsIFrame* parentFrame;
  GetParent(&parentFrame);
  if (IsTableRowGroupFrame(parentFrame))
    return PR_FALSE; // Nested groups don't grow.
  else return PR_TRUE; // The outermost group can grow.
}

PRBool nsTreeRowGroupFrame::RowGroupReceivesExcessSpace()
{
  if (IsTableRowGroupFrame(this))
    return PR_TRUE;
  else return PR_FALSE; // Headers and footers don't get excess space.
}

NS_IMETHODIMP
nsTreeRowGroupFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                      const nsPoint& aPoint,
                                      nsFramePaintLayer aWhichLayer,
                                      nsIFrame** aFrame)
{
  // XXX Should this be needed?  Shouldn't scrollbars be checked in
  // general?
  nsPoint tmp;
  nsresult rv;
  if (mScrollbar) {
    tmp.MoveTo(aPoint.x - mRect.x, aPoint.y - mRect.y);
    rv = mScrollbar->GetFrameForPoint(aPresContext, tmp, aWhichLayer, aFrame);
    if (rv == NS_OK)
      return NS_OK;
  }

  return nsTableRowGroupFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
}

NS_IMETHODIMP
nsTreeRowGroupFrame::FirstChild(nsIPresContext* aPresContext,
                                nsIAtom*        aListName,
                                nsIFrame**      aFirstChild) const
{
  if (nsXULAtoms::scrollbarlist == aListName) {
    *aFirstChild = mScrollbarList.FirstChild();
    return NS_OK;
  }
  
  return nsTableRowGroupFrame::FirstChild(aPresContext, aListName, aFirstChild);
}

NS_IMETHODIMP
nsTreeRowGroupFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                                nsIAtom** aListName) const
{
  *aListName = nsnull;

  if (aIndex == 0) {
    *aListName = nsXULAtoms::scrollbarlist;
    NS_IF_ADDREF(*aListName);
  }

  return NS_OK;
}

void nsTreeRowGroupFrame::PaintChildren(nsIPresContext*      aPresContext,
                                         nsIRenderingContext& aRenderingContext,
                                         const nsRect&        aDirtyRect,
                                         nsFramePaintLayer    aWhichLayer)
{
  nsHTMLContainerFrame::PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

  if (mScrollbar) {
    nsIView *pView;
     
    mScrollbar->GetView(aPresContext, &pView);
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
#ifdef DEBUG
      if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) &&
          GetShowFrameBorders()) {
        aRenderingContext.SetColor(NS_RGB(255,0,0));
        aRenderingContext.DrawRect(0, 0, kidRect.width, kidRect.height);
      }
#endif
      aRenderingContext.PopState(clipState);
    }
  }
}

NS_IMETHODIMP
nsTreeRowGroupFrame::IR_TargetIsChild(nsIPresContext*      aPresContext,
                                      nsHTMLReflowMetrics& aDesiredSize,
                                      RowGroupReflowState& aReflowState,
                                      nsReflowStatus&      aStatus,
                                      nsIFrame *           aNextFrame)
{
  if (aNextFrame && (aNextFrame == mScrollbar)) {
    nsresult rv;
    // Recover the state as if aNextFrame is about to be reflowed
    RecoverState(aReflowState, aNextFrame, aDesiredSize.maxElementSize);

    // Remember the old rect
    nsRect  oldKidRect;
    aNextFrame->GetRect(oldKidRect);

    // Pass along the reflow command
    nsHTMLReflowState   kidReflowState(aPresContext, aReflowState.reflowState,
                                       aNextFrame, aReflowState.availSize);
    kidReflowState.mComputedHeight = mRowGroupHeight;
    nsHTMLReflowMetrics desiredSize(nsnull);

    rv = ReflowChild(aNextFrame, aPresContext, desiredSize, kidReflowState,
                     0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);

    nscoord xpos = 0;

    // Lose the width of the scrollbar as far as the rows are concerned.
    if (aReflowState.availSize.width != NS_UNCONSTRAINEDSIZE) {
      xpos = aReflowState.availSize.width - desiredSize.width;
      /*aReflowState.availSize.width -= desiredSize.width;
      if (aReflowState.availSize.width < 0)
        aReflowState.availSize.width = 0;*/ 
    }

    // Place the child
    FinishReflowChild(aNextFrame, aPresContext, desiredSize, xpos, 0, 0);

    // Return our desired width and height
    nsRect rect;
    GetRect(rect);
    aDesiredSize.width = aReflowState.reflowState.availableWidth;
    aDesiredSize.height = rect.height;

    if (mNextInFlow) {
      aStatus = NS_FRAME_NOT_COMPLETE;
    }

    return rv;
  }
 
  return nsTableRowGroupFrame::IR_TargetIsChild(aPresContext,
                                      aDesiredSize,
                                      aReflowState,
                                      aStatus,
                                      aNextFrame);
}

NS_IMETHODIMP 
nsTreeRowGroupFrame::ReflowBeforeRowLayout(nsIPresContext*      aPresContext,
                                           nsHTMLReflowMetrics& aDesiredSize,
                                           RowGroupReflowState& aReflowState,
                                           nsReflowStatus&      aStatus,
                                           nsReflowReason       aReason)
{
  NS_ASSERTION(!aReflowState.unconstrainedWidth, 
               "Reflowing tree row group with unconstrained width!!!!");
  
  NS_ASSERTION(!aReflowState.unconstrainedHeight, 
               "Reflowing tree row group with unconstrained height!!!!");

  //if (mScrollbar)
  //  printf("TRG Width: %d, TRG Height: %d\n", aReflowState.availSize.width, 
  //                                            aReflowState.availSize.height);

  nsresult rv = NS_OK;
  mRowGroupHeight = aReflowState.availSize.height;
  mRowGroupWidth = aReflowState.availSize.width;

  // Lose the width of the scrollbar if we've got one.
  if (mScrollbar) {
    nsRect rect;
    mScrollbar->GetRect(rect);
    aReflowState.availSize.width -= rect.width;
    ((nsHTMLReflowState&)aReflowState.reflowState).mComputedWidth -= rect.width;
    ((nsHTMLReflowState&)aReflowState.reflowState).availableWidth -= rect.width;
  }

  return rv;
}

NS_IMETHODIMP 
nsTreeRowGroupFrame::ReflowAfterRowLayout(nsIPresContext*       aPresContext,
                                           nsHTMLReflowMetrics& aDesiredSize,
                                           RowGroupReflowState& aReflowState,
                                           nsReflowStatus&      aStatus,
                                           nsReflowReason       aReason)
{
  nsresult rv = NS_OK;
  ReflowScrollbar(aPresContext);

  if ((mOuterFrame == this) && (mRowGroupHeight != NS_UNCONSTRAINEDSIZE) &&
      (mIsFull || mScrollbar)) {

    PRBool createdScrollbar = PR_FALSE;

    // Ensure the scrollbar has been created.
    if (!mScrollbar) {
      CreateScrollbar(aPresContext);
      createdScrollbar = PR_TRUE;
    }

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
    
    if (mRowGroupHeight == -1) // This happens when a fixed row tree wishes to alter the scrollbar's height
      mRowGroupHeight = aReflowState.y;
    kidReflowState.mComputedHeight = mRowGroupHeight;
    rv = ReflowChild(mScrollbar, aPresContext, desiredSize, kidReflowState,
                     0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
    if (NS_FAILED(rv))
      return rv;

    nscoord xpos = 0;

    if (aReflowState.availSize.width != NS_UNCONSTRAINEDSIZE) {
      xpos = aReflowState.availSize.width;

      if (mRowGroupWidth == aReflowState.availSize.width) {
        // Never had a scrollbar before.  Move it over.
        xpos -= desiredSize.width;
      }
    }

    // Place the child
    FinishReflowChild(mScrollbar, aPresContext, desiredSize, xpos, 0, 0);

    if (createdScrollbar) {
      // Let another reflow happen.
      // Dirty the tree for another reflow.
      nsTableFrame* tableFrame;
      nsTableFrame::GetTableFrame(this, tableFrame);
      
      MarkTreeAsDirty(aPresContext, (nsTreeFrame*)tableFrame);
    }
  } 
  return rv;
}


void nsTreeRowGroupFrame::LocateFrame(nsIFrame* aStartFrame, nsIFrame** aResult)
{
  if (aStartFrame == nsnull)
  {
    *aResult = mFrames.FirstChild();
  }
  else aStartFrame->GetNextSibling(aResult);
}
   
nsIFrame*
nsTreeRowGroupFrame::GetFirstFrame()
{
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
  LocateFrame(aPrevFrame, aResult);
}

nsIFrame* 
nsTreeRowGroupFrame::GetFirstFrameForReflow(nsIPresContext* aPresContext) 
{ 
  // Clear ourselves out.
  mLinkupFrame = nsnull;
  mBottomFrame = mTopFrame;
  mIsFull = PR_FALSE;

  // If we have a frame and no content chain (e.g., unresolved/uncreated content)
  if (mTopFrame && !mContentChain)
  {
	  return mTopFrame;
  }

  // See if we have any frame whatsoever.
  LocateFrame(nsnull, &mTopFrame);
  
  mBottomFrame = mTopFrame;

  nsCOMPtr<nsIContent> startContent;
  if (mContentChain) {
    nsCOMPtr<nsISupports> supports;
    mContentChain->GetElementAt(0, getter_AddRefs(supports));
    nsCOMPtr<nsIContent> chainContent = do_QueryInterface(supports);
    startContent = chainContent;

    if (mTopFrame) {
    
      // We have a content chain. If the top frame is the same as our content
      // chain, we can go ahead and destroy our content chain and return the
      // top frame.
      nsCOMPtr<nsIContent> topContent;
      mTopFrame->GetContent(getter_AddRefs(topContent));
      if (chainContent.get() == topContent.get()) {
        // The two content nodes are the same.  Our content chain has
        // been synched up, and we can now remove our element and
        // pass the content chain inwards.
        InitSubContentChain((nsTreeRowGroupFrame*)mTopFrame);

        return mTopFrame;
      }
      else mLinkupFrame = mTopFrame; // We have some frames that we'll eventually catch up with.
                                     // Cache the pointer to the first of these frames, so
                                     // we'll know it when we hit it.
    }
  }
  else if (mTopFrame) {
    return mTopFrame;
  }

  // We don't have a top frame instantiated. Let's
  // try to make one.

  // If startContent is initialized, we have a content chain, and 
  // we're using that content node to make our frame.
  // Otherwise we have nothing, and we should just try to grab the first child.
  if (!startContent) {
    PRInt32 childCount;
    mContent->ChildCount(childCount);
    nsCOMPtr<nsIContent> childContent;
    if (childCount > 0) {
      mContent->ChildAt(0, *getter_AddRefs(childContent));
      startContent = childContent;
    }
  }

  if (startContent) {
    nsTableFrame* tableFrame;
    nsTableFrame::GetTableFrame(this, tableFrame);
     
    PRBool isAppend = (mLinkupFrame == nsnull);

    mFrameConstructor->CreateTreeWidgetContent(aPresContext, this, nsnull, startContent,
                                               &mTopFrame, isAppend, PR_FALSE, 
                                               nsnull);
    
    // XXX Can be optimized if we detect that we're appending a row.
    // Also the act of appending or inserting a row group is harmless.

    //printf("Created a frame\n");
    mBottomFrame = mTopFrame;
    if (IsTableRowFrame(mTopFrame)) {
      nsCOMPtr<nsIContent> rowContent;
      mTopFrame->GetContent(getter_AddRefs(rowContent));
      
      nsCOMPtr<nsIContent> topRowContent;
      PRInt32 delta = 1;
      FindPreviousRowContent(delta, rowContent, nsnull, getter_AddRefs(topRowContent));

      PRInt32 numColsAdded = AddRowToMap(tableFrame, *aPresContext, topRowContent, mTopFrame);
      if (numColsAdded > 0) {
        MarkTreeAsDirty(aPresContext, (nsTreeFrame*) tableFrame);
      }
      //PostAppendRow(mTopFrame, aPresContext, aTableFrame->GetColCount());
    }
    else if (IsTableRowGroupFrame(mTopFrame) && mContentChain) {
      // We have just instantiated a row group, and we have a content chain. This
      // means we need to potentially pass a sub-content chain to the instantiated
      // frame, so that it can also sync up with its children.
      InitSubContentChain((nsTreeRowGroupFrame*)mTopFrame);
    }

    SetContentChain(nsnull);

    return mTopFrame;
  }
  return nsnull;
}
  
void 
nsTreeRowGroupFrame::GetNextFrameForReflow(nsIPresContext* aPresContext, nsIFrame* aFrame, nsIFrame** aResult) 
{ 
  // We're ultra-cool. We build our frames on the fly.
  LocateFrame(aFrame, aResult);
  if (*aResult && (*aResult == mLinkupFrame)) {
    // We haven't really found a result. We've only found a result if
    // the linkup frame is really the next frame following the
    // previous frame.
    nsCOMPtr<nsIContent> prevContent;
    aFrame->GetContent(getter_AddRefs(prevContent));
    nsCOMPtr<nsIContent> linkupContent;
    mLinkupFrame->GetContent(getter_AddRefs(linkupContent));
    PRInt32 i, j;
    mContent->IndexOf(prevContent, i);
    mContent->IndexOf(linkupContent, j);
    if (i+1==j) {
      // We have found a match and successfully linked back up with our
      // old frame. 
      mBottomFrame = mLinkupFrame;
      mLinkupFrame = nsnull;
		  return;
    }
    else *aResult = nsnull; // No true linkup. We need to make a frame.
  }

  if (!*aResult) {
    // No result found. See if there's a content node that wants a frame.
    PRInt32 i, childCount;
    nsCOMPtr<nsIContent> prevContent;
    aFrame->GetContent(getter_AddRefs(prevContent));
    nsCOMPtr<nsIContent> parentContent;
    mContent->IndexOf(prevContent, i);
    mContent->ChildCount(childCount);
    if (i+1 < childCount) {
      nsTableFrame* tableFrame;
      nsTableFrame::GetTableFrame(this, tableFrame);

      // There is a content node that wants a frame.
      nsCOMPtr<nsIContent> nextContent;
      mContent->ChildAt(i+1, *getter_AddRefs(nextContent));
      nsIFrame* prevFrame = nsnull; // Default is to append
      PRBool isAppend = PR_TRUE;
      if (mLinkupFrame) {
        // This will be an insertion, since we have frames on the end.
        prevFrame = aFrame;
        isAppend = PR_FALSE;
      }

      mFrameConstructor->CreateTreeWidgetContent(aPresContext, this, prevFrame, nextContent,
                                                 aResult, isAppend, PR_FALSE,
                                                 nsnull);

      // XXX Can be optimized if we detect that we're appending a row to the end of the tree.
      // Also the act of appending or inserting a row group is harmless.
      if (IsTableRowFrame(*aResult)) {
        
        nsCOMPtr<nsIContent> topRowContent;
        PRInt32 delta = 1;
        FindPreviousRowContent(delta, nextContent, nsnull, getter_AddRefs(topRowContent));
        
        PRInt32 numColsAdded = AddRowToMap(tableFrame, *aPresContext, topRowContent, (*aResult));
        if (numColsAdded > 0) {
          MarkTreeAsDirty(aPresContext, (nsTreeFrame*) tableFrame);
        }
        //PostAppendRow(*aResult, aPresContext, tableFrame->GetColCount());
      }
    }
  }

  mBottomFrame = *aResult;
  return;
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

PRBool nsTreeRowGroupFrame::ContinueReflow(nsIFrame* aFrame, nsIPresContext* aPresContext, nscoord y, nscoord height) 
{ 
  //printf("Y is: %d\n", y);
  //printf("Height is: %d\n", height); 
  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);
  nsTreeFrame* treeFrame = (nsTreeFrame*)tableFrame;

  PRInt32 rowSize = treeFrame->GetFixedRowSize();
  
  // Let's figure out if we want to halt the reflow.
  if (IsTableRowFrame(aFrame)) {
    // Cast it and get the row index.
    PRInt32 rowIndex = ((nsTableRowFrame*)aFrame)->GetRowIndex();
    if (rowSize != -1 && rowIndex == (rowSize-1)) {
      treeFrame->HaltReflow();

      // Get our running height total and clear the row group's mRowGroupHeight
      mOuterFrame->ClearRowGroupHeight();
    }
  }

  PRBool reflowStopped = treeFrame->IsReflowHalted();
  /*if (reflowStopped) {
    // This is only hit for fixed row trees (e.g., rows="3")
    nsCOMPtr<nsIContent> nextRow;
    nsCOMPtr<nsIContent> treeContent;
    treeFrame->GetContent(getter_AddRefs(treeContent));

    FindRowContentAtIndex(rowSize, treeContent,
                          getter_AddRefs(nextRow));

    if (nextRow)
      mIsFull = PR_TRUE;
  }*/

  if ((rowSize == -1 && height <= 0) || reflowStopped) {
    mIsFull = PR_TRUE;

    nsIFrame* lastChild = GetLastFrame();
    nsIFrame* startingPoint = mBottomFrame;
    if (startingPoint == nsnull) {
      // We just want to delete everything but the first item.
      startingPoint = GetFirstFrame();
    }

    if (lastChild != startingPoint) {
      // We have some hangers on (probably caused by shrinking the size of the window).
      // Nuke them.
      nsIFrame* currFrame;
      startingPoint->GetNextSibling(&currFrame);
      while (currFrame) {
        // get the starting row index and row count
        PRInt32 startRowIndex, numRows;
        GetRowStartAndCount(currFrame, startRowIndex, numRows);

        nsIFrame* nextFrame;
        currFrame->GetNextSibling(&nextFrame);
        mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, currFrame, nsnull);
        mFrames.DestroyFrame(aPresContext, currFrame);

        // remove the rows from the table after removing from the sibling chain
        if ((startRowIndex >= 0) && (numRows > 0)) {
          tableFrame->RemoveRows(*aPresContext, startRowIndex, numRows, PR_FALSE);
          tableFrame->InvalidateColumnWidths();
        }

        currFrame = nextFrame;
        //printf("Nuked one off the end.\n");
      }
    }
    return PR_FALSE;
  }
  else
    return PR_TRUE;
}
  
// Responses to changes
void nsTreeRowGroupFrame::OnContentAdded(nsIPresContext* aPresContext) 
{
  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);

  nsTreeFrame* treeFrame = (nsTreeFrame*) tableFrame;  
  MarkTreeAsDirty(aPresContext, treeFrame);
  
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  shell->ProcessReflowCommands(PR_FALSE);
}

void nsTreeRowGroupFrame::OnContentInserted(nsIPresContext* aPresContext, nsIFrame* aNextSibling, PRInt32 aIndex)
{
  nsIFrame* currFrame = aNextSibling;

  // this will probably never happen - 
  // if we haven't created the topframe, it doesn't matter if
  // content was inserted
  if (mTopFrame == nsnull) return;

  // if we're inserting content at the top of visible content,
  // then ignore it because it would go off-screen
  // except of course in the case of the first row, where we're
  // actually adding visible content
  if(aNextSibling == mTopFrame) {
    if (aIndex == 0)
      // it's the first row, blow away mTopFrame so it can be
      // crecreated later
      mTopFrame = nsnull;
    else
      // it's not visible, nothing to do
      return;
  }

  while (currFrame) {
    nsIFrame* nextFrame;
    currFrame->GetNextSibling(&nextFrame);
    mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, currFrame, nsnull);
    // get the starting row index and row count
    PRInt32 startRowIndex, numRows;
    GetRowStartAndCount(currFrame, startRowIndex, numRows);

    mFrames.DestroyFrame(aPresContext, currFrame);
    currFrame = nextFrame;
    //printf("Nuked one off the end.\n");

    // remove the rows from the table after removing from the sibling chain
    if ((startRowIndex >= 0) && (numRows > 0)) {
      nsTableFrame* tableFrame;
      nsTableFrame::GetTableFrame(this, tableFrame);
      tableFrame->RemoveRows(*aPresContext, startRowIndex, numRows, PR_FALSE);
      tableFrame->InvalidateColumnWidths();
    }
  }
  OnContentAdded(aPresContext);
	
}

void nsTreeRowGroupFrame::OnContentRemoved(nsIPresContext* aPresContext, 
                                           nsIFrame* aChildFrame,
                                           PRInt32 aIndex)
{
  // if we're removing the top row, the new top row is the next row
  if (mTopFrame && mTopFrame == aChildFrame)
    mTopFrame->GetNextSibling(&mTopFrame);

  // if we're removing the last frame in this rowgroup and if we have
  // a scrollbar, we have to yank in some rows from above
  if (!mTopFrame && mScrollbar && mCurrentIndex > 0) {  
    // sync up the scrollbar, now that we've scrolled one row
     mCurrentIndex--;
     nsAutoString indexStr;
     PRInt32 pixelIndex = mCurrentIndex * SCROLL_FACTOR;
     indexStr.Append(pixelIndex);
    
     nsCOMPtr<nsIContent> scrollbarContent;
     mScrollbar->GetContent(getter_AddRefs(scrollbarContent));
     scrollbarContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::curpos,
                                     indexStr, /* notify */ PR_TRUE);

     // Now force the reflow to happen immediately, because we need to
     // deal with cleaning out the content chain.
     nsCOMPtr<nsIPresShell> shell;
     aPresContext->GetShell(getter_AddRefs(shell));
     shell->ProcessReflowCommands(PR_FALSE);

     return; // All frames got deleted anyway by the pos change.
  }
  
  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);

  nsTreeFrame* treeFrame = (nsTreeFrame*)tableFrame;

  // Go ahead and delete the frame.
  if (aChildFrame) {
    // get the starting row index and row count
    PRInt32 startRowIndex, numRows;
    GetRowStartAndCount(aChildFrame, startRowIndex, numRows);

    mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, aChildFrame, nsnull);
    mFrames.DestroyFrame(aPresContext, aChildFrame);

    // remove the rows from the table after removing from the sibling chain
    if ((startRowIndex >= 0) && (numRows > 0)) {
      treeFrame->RemoveRows(*aPresContext, startRowIndex, numRows, PR_FALSE);
      treeFrame->InvalidateColumnWidths();
    }
  }

  MarkTreeAsDirty(aPresContext, treeFrame);
}

void
nsTreeRowGroupFrame::ReflowScrollbar(nsIPresContext* aPresContext)
{
  PRInt32 count = 0;
  ComputeTotalRowCount(count, mContent); // XXX This sucks! Needs to be cheap!
  mRowCount = count;

  if (!mScrollbar)
    return;
                                     
  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);
  nsTreeFrame* treeFrame = (nsTreeFrame*)tableFrame;

  // Our page size is the # of rows instantiated.
  PRInt32 pageRowCount;
  GetRowCount(pageRowCount);

  if (mScrollbar) {
    PRBool nukeScrollbar=PR_FALSE;
    nsAutoString value;
    
    nsCOMPtr<nsIContent> scrollbarContent;
    mScrollbar->GetContent(getter_AddRefs(scrollbarContent));
    
    if (count < pageRowCount) {
      // first set the position to 0 so that all visible content
      // scrolls into view
      value.Append(0);
      scrollbarContent->SetAttribute(kNameSpaceID_None,
                                     nsXULAtoms::curpos,
                                     value, PR_TRUE);
      // now force nuking the scrollbar
      // (otherwise it takes a bunch of reflows to actually make it go away)
      nukeScrollbar=PR_TRUE;
    }

    else {
      scrollbarContent->GetAttribute(kNameSpaceID_None,
                                     nsXULAtoms::curpos, value);
    }
   
    if (nukeScrollbar || (value.Equals("0") && !mIsFull)) {
      
      // clear the scrollbar out of the event state manager so that the
      // event manager doesn't send events to the destroyed scrollbar frames
      nsCOMPtr<nsIPresShell> shell;
      aPresContext->GetShell(getter_AddRefs(shell));
      ClearFrameRefs(aPresContext, shell, mScrollbar);
      
      // Nuke the scrollbar.
      mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, mScrollbar, nsnull);
      mScrollbarList.DestroyFrames(aPresContext);
      mScrollbar = nsnull;

      // Dirty the tree for another reflow.
      MarkTreeAsDirty(aPresContext, treeFrame);
    }
  }

  if (!mScrollbar)
    return;

  // Set the maxpos of the scrollbar.
  nsCOMPtr<nsIContent> scrollbarContent;
  mScrollbar->GetContent(getter_AddRefs(scrollbarContent));

  PRInt32 rowCount = count-1;
  if (rowCount < 0)
    rowCount = 0;

  // Subtract one from our maxpos if we're a fixed row height.
  PRInt32 rowSize = treeFrame->GetFixedRowSize();
  if (rowSize != -1) {
    rowCount--;
  }

  nsAutoString maxpos;
  if (!mIsFull) {
    // We are not full. This means that we are not allowed to scroll any further. We are
    // at the max position right now.
    scrollbarContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::curpos, maxpos);
  }
  else {

    if (pageRowCount < 2)
      pageRowCount = 2;

    rowCount -= (pageRowCount-2);

    rowCount *= SCROLL_FACTOR;
    char ch[100];
    sprintf(ch,"%d", rowCount);
    maxpos = ch;
  }

  // Make sure our position is accurate
  nsAutoString value;  
  scrollbarContent->GetAttribute(kNameSpaceID_None,
                                     nsXULAtoms::maxpos, value);
                                     
  if(value != maxpos){
    scrollbarContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::maxpos, maxpos, PR_TRUE);
  }
}

void nsTreeRowGroupFrame::SetContentChain(nsISupportsArray* aContentChain)
{
  NS_IF_RELEASE(mContentChain);
  mContentChain = aContentChain;
  NS_IF_ADDREF(mContentChain);
}

void nsTreeRowGroupFrame::InitSubContentChain(nsTreeRowGroupFrame* aRowGroupFrame)
{
  if (mContentChain) {
    mContentChain->RemoveElementAt(0);
    PRUint32 chainSize;
    mContentChain->Count(&chainSize);
    if (chainSize > 0 && aRowGroupFrame) {
      aRowGroupFrame->SetContentChain(mContentChain);
    }
    // The chain is dead. Long live the chain.
    SetContentChain(nsnull);
  }
}

void nsTreeRowGroupFrame::CreateScrollbar(nsIPresContext* aPresContext)
{
  if ((mOuterFrame == this) && !mScrollbar) {
    // Create an anonymous scrollbar node.
    nsCOMPtr<nsIDocument> idocument;
    mContent->GetDocument(*getter_AddRefs(idocument));

    nsCOMPtr<nsIDOMDocument> document(do_QueryInterface(idocument));

    nsCOMPtr<nsIDOMElement> node;
    document->CreateElement("scrollbar",getter_AddRefs(node));

    nsCOMPtr<nsIContent> content = do_QueryInterface(node);
    content->SetDocument(idocument, PR_FALSE);
    content->SetParent(mContent);

    nsCOMPtr<nsIXULContent> xulContent = do_QueryInterface(content);
    if (xulContent)
      xulContent->SetAnonymousState(PR_TRUE); // Mark as anonymous to keep events from getting out.
    
    nsCOMPtr<nsIAtom> align = dont_AddRef(NS_NewAtom("align"));
    content->SetAttribute(kNameSpaceID_None, align, "vertical", PR_FALSE);
    
    nsIFrame* aResult;
    mFrameConstructor->CreateTreeWidgetContent(aPresContext, this, nsnull, content,
                                               &aResult, PR_FALSE, PR_TRUE, nsnull);

  }
}

void
nsTreeRowGroupFrame::CollapseScrollbar(PRBool hide, nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  nsIFrame* frame = aFrame;
  if (frame == nsnull)
    frame = mScrollbar;

  if (frame == nsnull)
    return;

  // shrink the view
  nsIView* view = nsnull;
  frame->GetView(aPresContext, &view);

  // if we find a view stop right here. All views under it
  // will be clipped.
  if (view) {
     nsViewVisibility v;
     view->GetVisibility(v);
     nsCOMPtr<nsIWidget> widget;
     view->GetWidget(*getter_AddRefs(widget));
     if (hide) {
         view->SetVisibility(nsViewVisibility_kHide);
     } else {
         view->SetVisibility(nsViewVisibility_kShow);
     }
     if (widget) {

       return;
     }
  }

  // collapse the child
  nsIFrame* child = nsnull;
  frame->FirstChild(aPresContext, nsnull, &child);

  while (nsnull != child) 
  {
     CollapseScrollbar(hide, aPresContext, child);
     nsresult rv = child->GetNextSibling(&child);
     NS_ASSERTION(rv == NS_OK,"failed to get next child");
  }
}

void
nsTreeRowGroupFrame::IndexOfCell(nsIPresContext* aPresContext,
                                 nsIContent* aCellContent, PRInt32& aRowIndex, PRInt32& aColIndex)
{
  // Get the index of our parent row.
  nsCOMPtr<nsIContent> row;
  aCellContent->GetParent(*getter_AddRefs(row));
  IndexOfRow(aPresContext, row, aRowIndex);
  if (aRowIndex == -1)
    return;

  // To determine the column index, just ask what our indexOf is.
  row->IndexOf(aCellContent, aColIndex);
}

// returns the 0-based index of the content node, within the content tree
void
nsTreeRowGroupFrame::IndexOfRow(nsIPresContext* aPresContext,
                                nsIContent* aRowContent, PRInt32& aRowIndex)
{
  // Use GetPrimaryFrameFor to retrieve the frame.
  // This crawls only the frame tree, and will be much faster for the case
  // where the frame is onscreen.
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));

  nsIFrame* result = nsnull;
  shell->GetPrimaryFrameFor(aRowContent, &result);

  if (result) {
    // We found a frame. This is good news. It means we can look at our row
    // index and just adjust based on our current offset index.
    nsTableRowFrame* row = (nsTableRowFrame*)result;
    PRInt32 screenRowIndex = row->GetRowIndex();
    aRowIndex = screenRowIndex + mCurrentIndex;
  }
  else {
    // We didn't find a frame.  This mean we have no choice but to crawl
    // the row group.
#ifdef DEBUG_tree
    printf("Searching for non-visible content node\n");
#endif
  }
}

                                   
PRBool
nsTreeRowGroupFrame::IsValidRow(PRInt32 aRowIndex)
{
  if (aRowIndex >= 0 && aRowIndex < mRowCount)
    return PR_TRUE;
  return PR_FALSE;
}

void
nsTreeRowGroupFrame::EnsureRowIsVisible(PRInt32 aRowIndex)
{
  // if no scrollbar, then it must be visible
  if (!mScrollbar) return;

  PRInt32 rows;
  GetRowCount(rows);
  PRInt32 bottomIndex = mCurrentIndex + rows - 1;
  
  // if row is visible, ignore
  if (mCurrentIndex <= aRowIndex && aRowIndex <= bottomIndex)
    return;

#ifdef DEBUG_tree
  printf("top = %d, bottom = %d, going to %d\n",
         mCurrentIndex, bottomIndex, aRowIndex);
#endif
  
  nsCOMPtr<nsIContent> scrollbarContent;
  mScrollbar->GetContent(getter_AddRefs(scrollbarContent));
  
  PRInt32 scrollTo = mCurrentIndex;
  if (aRowIndex < mCurrentIndex) {
    // row is above us, scroll up from mCurrentIndex
    // scroll such that the top row is aRowIndex
#ifdef DEBUG_tree
    printf("row is above, scroll to %d\n", aRowIndex);
#endif
    scrollTo=aRowIndex;
  } else {
    // aRowIndex > bottomIndex here
    // row is below us, so scroll down from bottomIndex
    // scroll such that the top row is "rows" above aRowIndex
    NS_ASSERTION(aRowIndex - rows >=0, "scrolling to negative row?!");
    scrollTo=aRowIndex - rows + 1;
#ifdef DEBUG_tree
    printf("row is below, scroll to %d\n", scrollTo);
#endif
    
  }

  nsAutoString value;
  
#ifdef DEBUG_tree
  // dump state
  scrollbarContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::maxpos, value);
  printf("maxpos = %s\n", value.ToNewCString());
  scrollbarContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::curpos, value);
  printf("curpos = %s\n", value.ToNewCString());

  value="";
#endif
  
  scrollTo *= SCROLL_FACTOR;
  value.Append(scrollTo);
  scrollbarContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::curpos,
                                 value, PR_TRUE);

  // This change has to happen immediately.
  // Flush any pending reflow commands.
  nsCOMPtr<nsIDocument> doc;
  mContent->GetDocument(*getter_AddRefs(doc));
  doc->FlushPendingNotifications();
}

void
nsTreeRowGroupFrame::GetCellFrameAtIndex(PRInt32 aRowIndex, PRInt32 aColIndex, 
                                         nsTreeCellFrame** aResult)
{
#ifdef DEBUG_tree
  printf("Looking for cell (%d, %d)..", aRowIndex, aColIndex);
#endif

  // Init the result to null.
  *aResult = nsnull;

  // The screen index = (aRowIndex - mCurrentIndex)
  PRInt32 screenIndex = aRowIndex - mCurrentIndex;

  // Get the table frame.
  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);

  nsTableCellFrame* cellFrame;

#ifdef DEBUG_tree
  printf("(screen index (%d,%d))\n", screenIndex, aColIndex);
#endif
  
  nsTableCellMap * cellMap = tableFrame->GetCellMap();
  CellData* cellData = cellMap->GetCellAt(screenIndex, aColIndex);
  if (cellData && cellData->IsOrig()) { // the cell originates at (rowX, colX)
    cellFrame = cellData->GetCellFrame();
    if (cellFrame) { 
      *aResult = (nsTreeCellFrame*)cellFrame; // XXX I am evil.
    }
  }

#ifdef DEBUG_tree
  printf("got cell frame %p\n", *aResult);
#endif
}

void nsTreeRowGroupFrame::ScrollByLines(nsIPresContext* aPresContext,
                                                 PRInt32 lines)
{
  if (nsnull == mScrollbar)  // nothing to scroll
    return;

  PRInt32 scrollTo = mCurrentIndex + lines;
  PRInt32 visRows, totalRows;
  totalRows = GetVisibleRowCount();
  GetRowCount(visRows);

  if (scrollTo < 0)
    scrollTo = 0;
  if (!IsValidRow(scrollTo + visRows - 1))  // don't go down too far
    scrollTo = totalRows - visRows + 1;

  scrollTo *= SCROLL_FACTOR;
  nsAutoString value;
  value.Append(scrollTo);

  nsCOMPtr<nsIContent> scrollbarContent;
  mScrollbar->GetContent(getter_AddRefs(scrollbarContent));
  if (scrollbarContent)
    scrollbarContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::curpos,
                                   value, PR_TRUE);
}


void
nsTreeRowGroupFrame::MarkTreeAsDirty(nsIPresContext* aPresContext, nsTreeFrame* aTreeFrame)
{
  if (!aTreeFrame->IsSlatedForReflow()) {
    aTreeFrame->SlateForReflow();

    // Mark the table frame as dirty
    nsFrameState      frameState;
    
    aTreeFrame->GetFrameState(&frameState);
    frameState |= NS_FRAME_IS_DIRTY;
    aTreeFrame->SetFrameState(frameState);
     
    // Schedule a reflow for us.
    nsCOMPtr<nsIReflowCommand> reflowCmd;
    nsIFrame*                  outerTableFrame;
    nsresult                   rv;

    aTreeFrame->GetParent(&outerTableFrame);
    rv = NS_NewHTMLReflowCommand(getter_AddRefs(reflowCmd), aTreeFrame,
                                 nsIReflowCommand::ReflowDirty);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIPresShell> presShell;
      aPresContext->GetShell(getter_AddRefs(presShell));
      presShell->AppendReflowCommand(reflowCmd);
    }
  }
}


// given a content node, insert a row into the cellmap
// for wherever it belows in the map. Return the number of new colums that were added
PRInt32
nsTreeRowGroupFrame::AddRowToMap(nsTableFrame*   aTableFrame,
                                 nsIPresContext& aPresContext,
                                 nsIContent*     aContent,
                                 nsIFrame*       aCurrentFrame)
{
  PRInt32 insertionIndex =
    GetInsertionIndexForContent(aTableFrame, &aPresContext, aContent);

  PRInt32 numNewCols = 0;
  if (aCurrentFrame) {
    numNewCols = aTableFrame->InsertRow(aPresContext, *mOuterFrame, *aCurrentFrame, 
                                        insertionIndex, PR_FALSE);
  }

  
  return numNewCols;
}


// determine the appropriate index of this row in the row map
PRInt32
nsTreeRowGroupFrame::GetInsertionIndexForContent(nsTableFrame *aTableFrame,
                                                 nsIPresContext* aPresContext,
                                                 nsIContent *aContent)
{

  PRInt32 insertionIndex = -1;
  
  // see if there is a frame for aContent
  if (aContent) {
    // Retrieve the primary frame.
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    
    nsIFrame* result = nsnull;
    shell->GetPrimaryFrameFor(aContent, &result);
    if (result) {
      // We have a primary frame. Get its row index. We're equal to that + 1.
      nsTableRowFrame* rowFrame = (nsTableRowFrame*)result;
      insertionIndex = rowFrame->GetRowIndex() + 1;
    }
  }
  
  // nope, no frame, so go find where it belongs by walking the frames
  if (insertionIndex==-1)
    insertionIndex = ((nsTreeFrame*)aTableFrame)->GetInsertionIndex(this);
  
  return insertionIndex;
}

// get the row index of aFrame
// recurse into rowgroups, count rows
PRInt32
nsTreeRowGroupFrame::GetInsertionIndex(nsIFrame *aFrame, PRInt32 aCurrentIndex, PRBool& aDone)
{
  nsIFrame *child = mFrames.FirstChild();

  PRInt32 index=aCurrentIndex;
  while (child) {

    // stop when we hit aFrame
    if (child==aFrame) {
      aDone=PR_TRUE;
      return index;
    }

    // recurse into rowgroups
    if (IsTableRowGroupFrame(child)) {
        index = ((nsTreeRowGroupFrame*)child)->GetInsertionIndex(aFrame, index, aDone);
        // short-circut the return
        if (aDone) return index;
    } 

    // count rows
    else if (IsTableRowFrame(child))
        index++;

    child->GetNextSibling(&child);
  }

  return index;
}

void
nsTreeRowGroupFrame::GetFirstRowFrame(nsTableRowFrame **aRowFrame)
{
  nsIFrame* child = mFrames.FirstChild();

  while (child) {
    if (IsTableRowFrame(child)) {
      *aRowFrame = (nsTableRowFrame*)child;
      return;
    }
    
    if (IsTableRowGroupFrame(child)) {
      ((nsTreeRowGroupFrame*)child)->GetFirstRowFrame(aRowFrame);
      if (*aRowFrame) return;
    }      
      
    child->GetNextSibling(&child);
  }
  
  return;
}

//
// pinkerton
// code copied from the toolbar to bootstrap tree d&d. I hope to god
// it goes away and i don't forget about it.
//
// We really _want_ to use CSS to do the drawing/etc, but changing the
// borders of the cells would cause reflows and that could really suck.
// As a first stab, let's try just to paint ourselves.
//

#include "nsIView.h"
#include "nsIViewManager.h"


//XXX NOTE: try to consolodate with the version in the toolbar code.
static void ForceDrawFrame(nsIPresContext* aPresContext, nsIFrame * aFrame);
static void ForceDrawFrame(nsIPresContext* aPresContext, nsIFrame * aFrame)
{
  if (aFrame == nsnull) {
    return;
  }
  nsRect    rect;
  nsIView * view;
  nsPoint   pnt;
  aFrame->GetOffsetFromView(aPresContext, pnt, &view);
  aFrame->GetRect(rect);
  rect.x = pnt.x;
  rect.y = pnt.y;
  if (view) {
    nsCOMPtr<nsIViewManager> viewMgr;
    view->GetViewManager(*getter_AddRefs(viewMgr));
    if (viewMgr)
      viewMgr->UpdateView(view, rect, NS_VMREFRESH_AUTO_DOUBLE_BUFFER | NS_VMREFRESH_IMMEDIATE);
  }

}


//
// AttributeChanged
//
// Track several attributes set by the d&d drop feedback tracking mechanism. The first
// is the "dd-triggerrepaint" attribute so JS can trigger a repaint when it
// needs up update the drop feedback. The second is the x (or y, if bar is vertical) 
// coordinate of where the drop feedback bar should be drawn.
//
NS_IMETHODIMP
nsTreeRowGroupFrame :: AttributeChanged ( nsIPresContext* aPresContext, nsIContent* aChild,
                                           PRInt32 aNameSpaceID, nsIAtom* aAttribute, PRInt32 aHint)
{
  nsresult rv = NS_OK;
  
  if ( aAttribute == nsXULAtoms::ddTriggerRepaint )
    ForceDrawFrame ( aPresContext, this );
  else if ( aAttribute == nsXULAtoms::ddDropLocationCoord ) {
    nsAutoString attribute;
    aChild->GetAttribute ( kNameSpaceID_None, aAttribute, attribute );
    char* iHateNSString = attribute.ToNewCString();
    mYDropLoc = atoi( iHateNSString );
    nsAllocator::Free ( iHateNSString );
  }
  else if ( aAttribute == nsXULAtoms::ddDropOn ) {
    nsAutoString attribute;
    aChild->GetAttribute ( kNameSpaceID_None, aAttribute, attribute );
    attribute.ToLowerCase();
    mDropOnContainer = attribute.Equals("true");
  }
  else
    rv = nsTableRowGroupFrame::AttributeChanged ( aPresContext, aChild, aNameSpaceID, aAttribute, aHint );

  return rv;
  
} // AttributeChanged


void
nsTreeRowGroupFrame::ClearFrameRefs(nsIPresContext* aPresContext,
                                    nsIPresShell *aShell,
                                    nsIFrame *aParent)
{
  nsIFrame* child;
  aParent->FirstChild(aPresContext, nsnull,&child);

  while (child) {

    // since we're destroying anonymous frames, we should also
    // set the parent to null. Otherwise, the event manager holds a
    // reference to the anonymous node, but the parent <scrollbar> node
    // goes away
    // we don't do this to aParent because that's the scrollbar node itself
    nsCOMPtr<nsIContent> content;
    child->GetContent(getter_AddRefs(content));
    content->SetParent(nsnull);
    
    ClearFrameRefs(aPresContext, aShell, child);
    child->GetNextSibling(&child);
  }
  aShell->ClearFrameRefs(aParent);
}



//
// Paint
//
// Used to draw the drop feedback based on attributes set by the drag event capturer
//
NS_IMETHODIMP
nsTreeRowGroupFrame :: Paint ( nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect, nsFramePaintLayer aWhichLayer)
{
  nsresult res = nsTableRowGroupFrame::Paint ( aPresContext, aRenderingContext, aDirtyRect, aWhichLayer );
  
  if ( mYDropLoc != -1 || mDropOnContainer ) {
    // go looking for the psuedo-style that describes the drop feedback marker. If we don't
    // have it yet, go looking for it.
    if (!mMarkerStyle) {
      nsCOMPtr<nsIAtom> atom ( getter_AddRefs(NS_NewAtom(":-moz-drop-marker")) );
      aPresContext->ProbePseudoStyleContextFor(mContent, atom, mStyleContext,
                                                PR_FALSE, getter_AddRefs(mMarkerStyle));
    }

    nscolor color;
    if ( mMarkerStyle ) {
      const nsStyleColor* styleColor = 
                 NS_STATIC_CAST(const nsStyleColor*, mMarkerStyle->GetStyleData(eStyleStruct_Color));
      color = styleColor->mColor;
    }
    else
      color = NS_RGB(0,0,0);

    // draw different drop feedback depending on if we are dropping on the 
    // container or above/below it
    if ( !mDropOnContainer ) {
      
      //XXX compute horiz indentation, fix up constants.
      
      aRenderingContext.SetColor(color);
      nsRect dividingLine ( 0, mYDropLoc, 20*50, 30 );
      aRenderingContext.FillRect(dividingLine);
    }
    else {
      aRenderingContext.SetColor(NS_RGB(0x7F,0x7F,0x7F));
      nsRect treeItemBounds ( 0, 0, mRect.width, mRect.height );
      aRenderingContext.DrawRect ( treeItemBounds );
    }
  }

  return res;
  
} // Paint
