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
 *  Mike Pinkerton (pinkerton@netscape.com)
 */

#ifndef NSXULTREEOUTERGROUPFRAME
#define NSXULTREEOUTERGROUPFRAME


#include "nsCOMPtr.h"
#include "nsBoxLayoutState.h"
#include "nsISupportsArray.h"
#include "nsXULTreeGroupFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsIPresContext.h"
#include "nsITimerCallback.h"
#include "nsITimer.h"
#include "nsIReflowCallback.h"

class nsCSSFrameConstructor;
class nsDragOverListener;
class nsDragAutoScrollTimer;


// I want to eventually use a delay so that the user has to hover over
// the top or bottom of a tree during a drag to get it to auto-scroll, but
// right now Mac can't handle timers during a drag so I'm punting and the
// tree will just always scroll when you drag the mouse near the top or
// bottom. Leaving the code around so that if we ever get this, we'll be
// good (pinkerton)
#define USE_TIMER_TO_DELAY_SCROLLING 0


enum nsTreeLayoutState {
  eTreeLayoutNormal,
  eTreeLayoutAbort,
  eTreeLayoutDirtyAll
};

class nsXULTreeRowGroupInfo {
public:
  PRInt32 mRowCount;
  nsCOMPtr<nsISupportsArray> mTickArray;
  nsIContent* mLastChild;

  nsXULTreeRowGroupInfo() :mRowCount(-1),mLastChild(nsnull)
  {
    NS_NewISupportsArray(getter_AddRefs(mTickArray));
  };

  ~nsXULTreeRowGroupInfo() { Clear(); };

  void Add(nsIContent* aContent) {
    mTickArray->AppendElement(aContent);
  }

  void Clear() {
    mLastChild = nsnull;
    mRowCount = -1;
    mTickArray->Clear();
  }
};



/*-----------------------------------------------------------------*/


class nsXULTreeOuterGroupFrame : public nsXULTreeGroupFrame, public nsIScrollbarMediator,
                                 public nsIReflowCallback /*, public nsITimerCallback */
{
  nsXULTreeOuterGroupFrame(nsIPresShell* aPresShell, PRBool aIsRoot = nsnull, nsIBoxLayout* aLayoutManager = nsnull, PRBool aDefaultHorizontal = PR_TRUE);
  virtual ~nsXULTreeOuterGroupFrame();

public:
  NS_DECL_ISUPPORTS
  
  friend class nsDragAutoScrollTimer;
  friend class nsDragOverListener;
  friend nsresult NS_NewXULTreeOuterGroupFrame(nsIPresShell* aPresShell, 
                                          nsIFrame** aNewFrame, 
                                          PRBool aIsRoot = PR_FALSE,
                                          nsIBoxLayout* aLayoutManager = nsnull,
                                          PRBool aDefaultHorizontal = PR_TRUE);
  
  NS_IMETHOD Init(nsIPresContext* aPresContext, nsIContent* aContent,
                  nsIFrame* aParent, nsIStyleContext* aContext, nsIFrame* aPrevInFlow);

  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);

    // nsIReflowCallback
  NS_IMETHOD ReflowFinished(nsIPresShell* aPresShell, PRBool* aFlushFlag);

#if USE_TIMER_TO_DELAY_SCROLLING
    // nsITimerCallback
  NS_IMETHOD_(void) Notify(nsITimer *timer) ;
#endif
  
  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
  {
    NeedsRecalc();
    return nsXULTreeGroupFrame::GetPrefSize(aBoxLayoutState, aSize);
  };

  NS_IMETHOD Paint(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect, nsFramePaintLayer aWhichLayer);
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext, nsIContent* aChild,
                                 PRInt32 aNameSpaceID, nsIAtom* aAttribute, PRInt32 aHint) ;

  NS_IMETHOD IsOutermostFrame(PRBool *aResult) { *aResult = PR_TRUE; return NS_OK; };

  PRInt32 GetRowCount() { if (mRowGroupInfo && (mRowGroupInfo->mRowCount != -1)) return mRowGroupInfo->mRowCount; PRInt32 count = 0;
                          ComputeTotalRowCount(count, mContent); mRowGroupInfo->mRowCount = count; return count; };

  PRInt32 GetRowHeightTwips() { 
    return mRowHeight;
  }

  void ClearRowGroupInfo() { if (mRowGroupInfo) mRowGroupInfo->Clear(); NeedsRecalc(); };
  
  void SetRowHeight(PRInt32 aRowHeight);
  PRBool IsFixedRowSize();

  void SetLayingOut(PRBool aLayingOut) { mLayingOut = aLayingOut; };

  nscoord GetYPosition();
  nscoord GetAvailableHeight();
  NS_IMETHOD GetNumberOfVisibleRows(PRInt32 *aResult) {
    *aResult=(mRowHeight ? GetAvailableHeight() / mRowHeight : 0);
    return NS_OK;
  }
  NS_IMETHOD GetIndexOfFirstVisibleRow(PRInt32 *aResult) {
    *aResult=mCurrentIndex; return NS_OK;
  }

  NS_IMETHOD GetRowCount(PRInt32* aResult) { *aResult = GetRowCount(); return NS_OK; }
  
  NS_IMETHOD PositionChanged(PRInt32 aOldIndex, PRInt32 aNewIndex);
  NS_IMETHOD ScrollbarButtonPressed(PRInt32 aOldIndex, PRInt32 aNewIndex);
  NS_IMETHOD VisibilityChanged(PRBool aVisible);

  void VerticalScroll(PRInt32 aDelta);

  void ConstructContentChain(nsIContent* aRowContent);
  void ConstructOldContentChain(nsIContent* aOldRowContent);
  void CreateOldContentChain(nsIContent* aOldRowContent, nsIContent* topOfChain);

  void FindChildOfCommonContentChainAncestor(nsIContent *startContent, nsIContent **child);

  PRBool IsAncestor(nsIContent *aRowContent, nsIContent *aOldRowContent, nsIContent **firstDescendant);

  void FindPreviousRowContent(PRInt32& aDelta, nsIContent* aUpwardHint, 
                              nsIContent* aDownwardHint, nsIContent** aResult);
  void FindNextRowContent(PRInt32& aDelta, nsIContent* aUpwardHint, 
                          nsIContent* aDownwardHint, nsIContent** aResult);
  void FindRowContentAtIndex(PRInt32& aIndex, nsIContent* aParent, 
                             nsIContent** aResult);
  
  // This method ensures that a row is onscreen.  It will scroll the tree widget such
  // that the row is at the top of the screen (if the row was offscreen to start with).
  void EnsureRowIsVisible(PRInt32 aRowIndex);

  void ScrollToIndex(PRInt32 aRowIndex);
  
  NS_IMETHOD IndexOfItem(nsIContent* aRoot, nsIContent* aContent,
                         PRBool aDescendIntoRows, // Invariant
                         PRBool aParentIsOpen,
                         PRInt32 *aResult);

  NS_IMETHOD InternalPositionChanged(PRBool aUp, PRInt32 aDelta);

  PRBool IsTreeSorted ( ) const { return mTreeIsSorted; }
  PRBool CanDropBetweenRows ( ) const { return mCanDropBetweenRows; }

  nsTreeLayoutState GetTreeLayoutState() { return mTreeLayoutState; }
  void SetTreeLayoutState(nsTreeLayoutState aState) { mTreeLayoutState = aState; }

  void PostReflowCallback();

protected:

  void ComputeTotalRowCount(PRInt32& aRowCount, nsIContent* aParent);

    // Drag Auto-scrolling
  nsresult HandleAutoScrollTracking ( const nsPoint & aPoint );
  PRBool IsInDragScrollRegion ( const nsPoint& inPoint, PRBool* outScrollUp ) ;
#if USE_TIMER_TO_DELAY_SCROLLING
  nsresult StopScrollTracking();
#endif

  nsXULTreeRowGroupInfo* mRowGroupInfo;
  PRInt32 mRowHeight;
  nscoord mOnePixel;
  PRInt32 mCurrentIndex; // Row-based
  PRPackedBool mTreeIsSorted;
  PRPackedBool mCanDropBetweenRows;      // is the user allowed to drop between rows
  
#if USE_TIMER_TO_DELAY_SCROLLING
  PRPackedBool mAutoScrollTimerHasFired;
  PRPackedBool mAutoScrollTimerStarted;
  nsCOMPtr<nsITimer> mAutoScrollTimer;
#endif
  
    // our auto-scroll event listener registered with the content model. See the discussion
    // in Init() for why this is a weak ref.
  nsDragOverListener* mDragOverListener;

  nsTreeLayoutState mTreeLayoutState;
  PRBool mReflowCallbackPosted;
  PRBool mLayingOut;
}; // class nsXULTreeOuterGroupFrame


#endif

