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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsXULTreeOuterGroupFrame.h"
#include "nsXULAtoms.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollbarFrame.h"
#include "nsISupportsArray.h"
#include "nsCSSFrameConstructor.h"
#include "nsIDocument.h"

#define TICK_FACTOR 50

//
// NS_NewXULTreeOuterGroupFrame
//
// Creates a new TreeOuterGroup frame
//
nsresult
NS_NewXULTreeOuterGroupFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot, 
                        nsIBoxLayout* aLayoutManager, PRBool aIsHorizontal)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsXULTreeOuterGroupFrame* it = new (aPresShell) nsXULTreeOuterGroupFrame(aPresShell, aIsRoot, aLayoutManager, aIsHorizontal);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewXULTreeOuterGroupFrame


// Constructor
nsXULTreeOuterGroupFrame::nsXULTreeOuterGroupFrame(nsIPresShell* aPresShell, PRBool aIsRoot, nsIBoxLayout* aLayoutManager, PRBool aIsHorizontal)
:nsXULTreeGroupFrame(aPresShell, aIsRoot, aLayoutManager, aIsHorizontal),
 mRowGroupInfo(nsnull), mRowHeight(TREE_LINE_HEIGHT), mCurrentIndex(0), mTwipIndex(0)
{}

// Destructor
nsXULTreeOuterGroupFrame::~nsXULTreeOuterGroupFrame()
{
  delete mRowGroupInfo;
}

NS_IMETHODIMP_(nsrefcnt) 
nsXULTreeOuterGroupFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsXULTreeOuterGroupFrame::Release(void)
{
  return NS_OK;
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsXULTreeOuterGroupFrame)
  NS_INTERFACE_MAP_ENTRY(nsIScrollbarMediator)
NS_INTERFACE_MAP_END_INHERITING(nsXULTreeGroupFrame)

NS_IMETHODIMP
nsXULTreeOuterGroupFrame::Init(nsIPresContext* aPresContext, nsIContent* aContent,
                               nsIFrame* aParent, nsIStyleContext* aContext, nsIFrame* aPrevInFlow)
{
  nsresult rv = nsXULTreeGroupFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  mOnePixel = NSIntPixelsToTwips(1, p2t);
  
  nsIFrame* box;
  aParent->GetParent(&box);
  if (!box)
    return rv;

  nsCOMPtr<nsIScrollableFrame> scrollFrame(do_QueryInterface(box));
  if (!scrollFrame)
    return rv;

  nsIBox* verticalScrollbar;
  scrollFrame->GetScrollbarBox(PR_TRUE, &verticalScrollbar);
  if (!verticalScrollbar) {
    NS_ERROR("Unable to install the scrollbar mediator on the tree widget. You must be using GFX scrollbars.");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIScrollbarFrame> scrollbarFrame(do_QueryInterface(verticalScrollbar));
  scrollbarFrame->SetScrollbarMediator(this);

  return rv;

} // Init

nscoord
nsXULTreeOuterGroupFrame::GetYPosition()
{
  nsIBox* box;
  GetParentBox(&box);
  if (!box)
    return 0;

  box->GetParentBox(&box);
  if (!box)
    return 0;

  nsCOMPtr<nsIScrollableFrame> scrollFrame(do_QueryInterface(box));
  if (!scrollFrame)
    return 0;

  nscoord x, y;
  scrollFrame->GetScrollPosition(mPresContext, x, y);
  return y;
}

void
nsXULTreeOuterGroupFrame::VerticalScroll(PRInt32 aDelta)
{
  nsIBox* box;
  GetParentBox(&box);
  if (!box)
    return;

  box->GetParentBox(&box);
  if (!box)
    return;

  nsCOMPtr<nsIScrollableFrame> scrollFrame(do_QueryInterface(box));
  if (!scrollFrame)
    return;

  nscoord x, y;
  scrollFrame->GetScrollPosition(mPresContext, x, y);
 
  scrollFrame->ScrollTo(mPresContext, x, aDelta);
}

nscoord
nsXULTreeOuterGroupFrame::GetAvailableHeight()
{
  nsIBox* box;
  GetParentBox(&box);
  if (!box)
    return 0;

  box->GetParentBox(&box);
  if (!box)
    return 0;

  nsCOMPtr<nsIScrollableFrame> scrollFrame(do_QueryInterface(box));
  if (!scrollFrame)
    return 0;

  nscoord x, y;
  scrollFrame->GetClipSize(mPresContext, &x, &y);
  return y;
}

void
nsXULTreeOuterGroupFrame::ComputeTotalRowCount(PRInt32& aCount, nsIContent* aParent)
{
  if (!mRowGroupInfo) {
    mRowGroupInfo = new nsXULTreeRowGroupInfo();
  }

  PRInt32 childCount;
  aParent->ChildCount(childCount);

  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> childContent;
    aParent->ChildAt(i, *getter_AddRefs(childContent));
    nsCOMPtr<nsIAtom> tag;
    childContent->GetTag(*getter_AddRefs(tag));
    if (tag.get() == nsXULAtoms::treerow) {
      if ((aCount%TICK_FACTOR) == 0)
        mRowGroupInfo->Add(childContent);

      mRowGroupInfo->mLastChild = childContent;

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
      if (isOpen.EqualsWithConversion("true"))
        ComputeTotalRowCount(aCount, childContent);
    }
  }
}

NS_IMETHODIMP
nsXULTreeOuterGroupFrame::ScrollbarButtonPressed(PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  if (aOldIndex == aNewIndex)
    return NS_OK;
  if (aNewIndex < aOldIndex)
    mCurrentIndex--;
  else mCurrentIndex++;
  if (mCurrentIndex < 0) {
    mCurrentIndex = 0;
    return NS_OK;
  }
  InternalPositionChanged(aNewIndex < aOldIndex, 1);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeOuterGroupFrame::PositionChanged(PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  if (aOldIndex == aNewIndex)
    return NS_OK;
  
  printf("Old Index: %d, New Index: %d\n", aOldIndex, aNewIndex);

  PRInt32 oldTwipIndex, newTwipIndex;
  oldTwipIndex = (aOldIndex*mOnePixel);
  newTwipIndex = (aNewIndex*mOnePixel);

  PRInt32 twipDelta = newTwipIndex > oldTwipIndex ? newTwipIndex - oldTwipIndex : oldTwipIndex - newTwipIndex;
  PRInt32 delta = twipDelta / mRowHeight;
  PRInt32 remainder = twipDelta % mRowHeight;
  if (remainder > (mRowHeight/2))
    delta++;

  if (delta == 0)
    return NS_OK;

  mCurrentIndex = newTwipIndex > oldTwipIndex ? mCurrentIndex + delta : mCurrentIndex - delta;
  
  if (mCurrentIndex < 0) {
    mCurrentIndex = 0;
    return NS_OK;
  }

  return InternalPositionChanged(newTwipIndex < oldTwipIndex, delta);
}

NS_IMETHODIMP
nsXULTreeOuterGroupFrame::InternalPositionChanged(PRBool aUp, PRInt32 aDelta)
{
  PRInt32 visibleRows = GetAvailableHeight()/mRowHeight;
  
  // Get our presentation context.
  if (aDelta < visibleRows) {
    if (mContentChain) {
      // XXX This could cause problems because of async reflow.
      // Eventually we need to make the code smart enough to look at a content chain
      // when building ANOTHER content chain.
  
      // Ensure all reflows happen first.
      nsCOMPtr<nsIPresShell> shell;
      mPresContext->GetShell(getter_AddRefs(shell));
      shell->FlushPendingNotifications();
    }

    PRInt32 loseRows = aDelta;

    // scrolling down
    if (!aUp) {
      // Figure out how many rows we have to lose off the top.
      DestroyRows(loseRows);
    }
    // scrolling up
    else {
      // Get our first row content.
      nsCOMPtr<nsIContent> rowContent;
      GetFirstRowContent(getter_AddRefs(rowContent));

      // Figure out how many rows we have to lose off the bottom.
      ReverseDestroyRows(loseRows);
    
      // Now that we've lost some rows, we need to create a
      // content chain that provides a hint for moving forward.
      nsCOMPtr<nsIContent> topRowContent;
      PRInt32 findContent = aDelta;
      FindPreviousRowContent(findContent, rowContent, nsnull, getter_AddRefs(topRowContent));
      ConstructContentChain(topRowContent);
	    //Now construct the chain for the old top row so its content chain gets
	    //set up correctly.
	    ConstructOldContentChain(rowContent);
    }
  }
  else {
    // Just blow away all our frames, but keep a content chain
    // as a hint to figure out how to build the frames.
    // Remove the scrollbar first.
    // get the starting row index and row count
    
    mFrameConstructor->RemoveMappingsForFrameSubtree(mPresContext, this, nsnull);
    nsBoxLayoutState state(mPresContext);
    mFirstChild = mLastChild = nsnull;
    mFrames.DestroyFrames(mPresContext);

    nsCOMPtr<nsIContent> topRowContent;
    FindRowContentAtIndex(mCurrentIndex, mContent, getter_AddRefs(topRowContent));

    if (topRowContent)
      ConstructContentChain(topRowContent);
  }

  mTopFrame = mBottomFrame = nsnull; // Make sure everything is cleared out.

  nsBoxLayoutState state(mPresContext);
  MarkDirtyChildren(state);

  VerticalScroll(mCurrentIndex*mRowHeight);

  return NS_OK;
}


void 
nsXULTreeOuterGroupFrame::ConstructContentChain(nsIContent* aRowContent)
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
nsXULTreeOuterGroupFrame::ConstructOldContentChain(nsIContent* aOldRowContent)
{
	nsCOMPtr<nsIContent> childOfCommonAncestor;

	//Find the first child of the common ancestor between the new top row's content chain
	//and the old top row.  Everything between this child and the old top row potentially need
	//to have their content chains reset.
	FindChildOfCommonContentChainAncestor(aOldRowContent, getter_AddRefs(childOfCommonAncestor));

	if (childOfCommonAncestor) {
      //Set up the old top rows content chian.
	  CreateOldContentChain(aOldRowContent, childOfCommonAncestor);
	}
}

void
nsXULTreeOuterGroupFrame::FindChildOfCommonContentChainAncestor(nsIContent *startContent, nsIContent **child)
{
	PRUint32 count;
	if (mContentChain)
	{
	  nsresult rv = mContentChain->Count(&count);

	  if (NS_SUCCEEDED(rv) && (count >0)) {
	    for (PRInt32 curItem = count - 1; curItem >= 0; curItem--) {
		    nsCOMPtr<nsISupports> supports;
        mContentChain->GetElementAt(curItem, getter_AddRefs(supports));
        nsCOMPtr<nsIContent> curContent = do_QueryInterface(supports);

		    //See if curContent is an ancestor of startContent.
		    if (IsAncestor(curContent, startContent, child))
		      return;
		  }
	  }
	}

	//mContent isn't actually put in the content chain, so we need to
	//check it separately.
	if (IsAncestor(mContent, startContent, child))
		return;

	*child = nsnull;
}


// if oldRowContent is an ancestor of rowContent, return true,
// and return the previous ancestor if requested
PRBool
nsXULTreeOuterGroupFrame::IsAncestor(nsIContent *aRowContent, nsIContent *aOldRowContent, nsIContent** firstDescendant)
{
  nsCOMPtr<nsIContent> prevContent;	
  nsCOMPtr<nsIContent> currContent = dont_QueryInterface(aOldRowContent);
  
  while (currContent) {
	  if (aRowContent == currContent.get()) {
		  if (firstDescendant) {
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

void
nsXULTreeOuterGroupFrame::CreateOldContentChain(nsIContent* aOldRowContent, nsIContent* topOfChain)
{
  nsCOMPtr<nsIContent> currContent = dont_QueryInterface(aOldRowContent);
  nsCOMPtr<nsIContent> prevContent;

  nsCOMPtr<nsIPresShell> shell;
  mPresContext->GetShell(getter_AddRefs(shell));

  //For each item between the (oldtoprow) and
  // (the new first child of common ancestry between new top row and old top row)
  // we need to see if the content chain has to be reset.
  while (currContent.get() != topOfChain) {
    nsIFrame* primaryFrame = nsnull;
    shell->GetPrimaryFrameFor(currContent, &primaryFrame);
      
    if (primaryFrame) {
      nsCOMPtr<nsIXULTreeSlice> slice(do_QueryInterface(primaryFrame));
      PRBool isRowGroup = PR_FALSE;
      if (slice)
        slice->IsGroupFrame(&isRowGroup);
    
	    if (isRowGroup) {
		    //Get the current content's parent's first child
		    nsCOMPtr<nsIContent> parent;
		    currContent->GetParent(*getter_AddRefs(parent));
		    
		    nsCOMPtr<nsIContent> firstChild;
		    parent->ChildAt(0, *getter_AddRefs(firstChild));

        nsIFrame* parentFrame;
        primaryFrame->GetParent(&parentFrame);
        PRBool isParentRowGroup = PR_FALSE;
        nsCOMPtr<nsIXULTreeSlice> slice(do_QueryInterface(parentFrame));
        if (slice)
          slice->IsGroupFrame(&isParentRowGroup);
    
	      if (isParentRowGroup) {
           //Get the current content's parent's first frame.
           nsXULTreeGroupFrame *parentRowGroupFrame =
               (nsXULTreeGroupFrame*)parentFrame;
		       nsIFrame *currentTopFrame = parentRowGroupFrame->GetFirstFrame();

			     nsCOMPtr<nsIContent> topContent;
			     currentTopFrame->GetContent(getter_AddRefs(topContent));

			     // If the current content's parent's first child is different
           // than the current frame's parent's first child then we know
           // they are out of synch and we need to set the content
           // chain correctly.
			     if(topContent.get() != firstChild.get()) {
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
nsXULTreeOuterGroupFrame::FindRowContentAtIndex(PRInt32& aIndex,
                                                nsIContent* aParent,
                                                nsIContent** aResult)
{
  // Init to nsnull.
  *aResult = nsnull;

  // Walk over the tick array.
  if (mRowGroupInfo == nsnull)
    return;

  PRUint32 index = 0;
  PRUint32 arrayCount;
  mRowGroupInfo->mTickArray->Count(&arrayCount);
  nsCOMPtr<nsIContent> startContent;
  PRUint32 location = aIndex/TICK_FACTOR + 1;
  PRUint32 point = location*TICK_FACTOR;
  if (location >= arrayCount) {
    startContent = mRowGroupInfo->mLastChild;
    point = mRowGroupInfo->mRowCount-1;
  }
  else {
    nsCOMPtr<nsISupports> supp = getter_AddRefs(mRowGroupInfo->mTickArray->ElementAt(location));
    startContent = do_QueryInterface(supp);
  }

  if (!startContent) {
    NS_ERROR("The tree's tick array is confused!");
    return;
  }

  PRInt32 delta = (PRInt32)(point-aIndex);
  FindPreviousRowContent(delta, startContent, nsnull, aResult);
}

void 
nsXULTreeOuterGroupFrame::FindPreviousRowContent(PRInt32& aDelta, nsIContent* aUpwardHint, 
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
      if (isOpen.EqualsWithConversion("true")) {
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
nsXULTreeOuterGroupFrame::FindNextRowContent(PRInt32& aDelta, nsIContent* aUpwardHint, 
                                             nsIContent* aDownwardHint,
                                             nsIContent** aResult)
{
  // Init to nsnull.
  *aResult = nsnull;

  // It disappoints me that this function is completely tied to the content nodes,
  // but I can't see any other way to handle this.  I don't have the frames, so I have nothing
  // else to fall back on but the content nodes.
  PRInt32 index = -1;
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
  }

  PRInt32 childCount;
  parentContent->ChildCount(childCount);
  for (PRInt32 i = index+1; i < childCount; i++) {
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
      // Descend into this row group and try to find a next row.
      FindNextRowContent(aDelta, nsnull, childContent, aResult);
      if (aDelta == 0)
        return;
    }
    else if (tag.get() == nsXULAtoms::treechildren) {
      // If it's open, descend into its treechildren node first.
      nsCOMPtr<nsIAtom> openAtom = dont_AddRef(NS_NewAtom("open"));
      nsAutoString isOpen;
      parentContent->GetAttribute(kNameSpaceID_None, openAtom, isOpen);
      if (isOpen.EqualsWithConversion("true")) {
        FindNextRowContent(aDelta, nsnull, childContent, aResult);
        if (aDelta == 0)
          return;
      }
    }
  }

  nsCOMPtr<nsIAtom> tag;
  parentContent->GetTag(*getter_AddRefs(tag));
  if (tag && tag.get() == nsXULAtoms::tree) {
    // Hopeless. It ain't in there.
    return;
  }
  else if (!aDownwardHint) // We didn't find it here. We need to go up to our parent, using ourselves as a hint.
    FindNextRowContent(aDelta, parentContent, nsnull, aResult);

  // Bail. There's nothing else we can do.
}

void
nsXULTreeOuterGroupFrame::EnsureRowIsVisible(PRInt32 aRowIndex)
{
  PRInt32 rows = GetAvailableHeight()/mRowHeight;
  PRInt32 bottomIndex = mCurrentIndex + rows - 1;
  
  // if row is visible, ignore
  if (mCurrentIndex <= aRowIndex && aRowIndex <= bottomIndex)
    return;

  // Try to center the new index.
  PRInt32 newIndex = aRowIndex - rows/2;
  if (newIndex < 0)
    newIndex = 0;

  PRInt32 delta = mCurrentIndex > newIndex ? mCurrentIndex - newIndex : newIndex - mCurrentIndex;
  PRInt32 up = newIndex < mCurrentIndex;
  mCurrentIndex = newIndex;
  InternalPositionChanged(up, delta);

  // This change has to happen immediately.
  // Flush any pending reflow commands.
  nsCOMPtr<nsIDocument> doc;
  mContent->GetDocument(*getter_AddRefs(doc));
  doc->FlushPendingNotifications();
}

void
nsXULTreeOuterGroupFrame::ScrollToIndex(PRInt32 aRowIndex)
{
  PRInt32 newIndex = aRowIndex;
  PRInt32 delta = mCurrentIndex > newIndex ? mCurrentIndex - newIndex : newIndex - mCurrentIndex;
  PRInt32 up = newIndex < mCurrentIndex;
  mCurrentIndex = newIndex;
  InternalPositionChanged(up, delta);

  // This change has to happen immediately.
  // Flush any pending reflow commands.
  nsCOMPtr<nsIDocument> doc;
  mContent->GetDocument(*getter_AddRefs(doc));
  doc->FlushPendingNotifications();
}

// walks the DOM to get the zero-based row index of the current content
// note that aContent can be any element, this will get the index of the
// element's parent
NS_IMETHODIMP
nsXULTreeOuterGroupFrame::IndexOfItem(nsIContent* aRoot, nsIContent* aContent,
                                      PRBool aDescendIntoRows, // Invariant
                                      PRBool aParentIsOpen,
                                      PRInt32 *aResult)
{
  PRInt32 childCount=0;
  aRoot->ChildCount(childCount);

  nsresult rv;
  
  PRInt32 childIndex;
  for (childIndex=0; childIndex<childCount; childIndex++) {
    nsCOMPtr<nsIContent> child;
    aRoot->ChildAt(childIndex, *getter_AddRefs(child));
    
    nsCOMPtr<nsIAtom> childTag;
    child->GetTag(*getter_AddRefs(childTag));

    // is this it?
    if (child.get() == aContent)
      return NS_OK;

    // we hit a treerow, count it
    if (childTag.get() == nsXULAtoms::treeitem)
      (*aResult)++;
  
    PRBool descend = PR_TRUE;
    PRBool parentIsOpen = aParentIsOpen;

    // don't descend into closed children
    if (childTag.get() == nsXULAtoms::treechildren && !parentIsOpen)
      descend = PR_FALSE;

    // speed optimization - descend into rows only when told
    else if (childTag.get() == nsXULAtoms::treerow && !aDescendIntoRows)
      descend = PR_FALSE;

    // descend as normally, but remember that the parent is closed!
    else if (childTag.get() == nsXULAtoms::treeitem) {
      nsAutoString isOpen;
      rv = child->GetAttribute(kNameSpaceID_None, nsXULAtoms::open, isOpen);

      if (!isOpen.EqualsWithConversion("true"))
        parentIsOpen=PR_FALSE;
    }

    // now that we've analyzed the tags, recurse
    if (descend) {
      rv = IndexOfItem(child, aContent,
                       aDescendIntoRows, parentIsOpen, aResult);
      if (NS_SUCCEEDED(rv))
        return NS_OK;
    }
  }

  // not found
  return NS_ERROR_FAILURE;
}

