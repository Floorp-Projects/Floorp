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


//
// nsDragAutoScrollTimer
//
// A timer class for handling auto-scrolling during drags
//
class nsDragAutoScrollTimer : public nsITimerCallback
{
public:

  NS_DECL_ISUPPORTS

  nsDragAutoScrollTimer ( nsXULTreeOuterGroupFrame* inTree )
      : mPoint(0,0), mDelay(30), mTree(inTree)
  {
    NS_INIT_ISUPPORTS();
  }

  virtual ~nsDragAutoScrollTimer()
  {
    if (mTimer)
      mTimer->Cancel();
  }

  nsresult Start(const nsPoint& aPoint)
  {
    mPoint = aPoint;

    if ( !mTimer ) {
      nsresult result;
      mTimer = do_CreateInstance("component://netscape/timer", &result);
      if (NS_FAILED(result))
        return result;
    }

    return mTimer->Init(this, mDelay);
  }

  nsresult Stop()
  {
    nsresult result = NS_OK;

    if (mTimer) {
      mTimer->Cancel();
      mTimer = nsnull;
    }

    return result;
  }

  nsresult SetDelay(PRUint32 aDelay)
  {
    mDelay = aDelay;
    return NS_OK;
  }

  NS_IMETHOD_(void) Notify(nsITimer *timer) ;
  
private:
  nsXULTreeOuterGroupFrame *mTree;
  nsCOMPtr<nsITimer> mTimer;
  nsPoint         mPoint;
  PRUint32        mDelay;
  
}; // nsDragAutoScrollTimer



/*-----------------------------------------------------------------*/


class nsXULTreeOuterGroupFrame : public nsXULTreeGroupFrame, public nsIScrollbarMediator,
                                 public nsIReflowCallback
{
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

  // nsIReflowCallback
  NS_IMETHOD ReflowFinished(nsIPresShell* aPresShell, PRBool* aFlushFlag);

protected:
  nsXULTreeOuterGroupFrame(nsIPresShell* aPresShell, PRBool aIsRoot = nsnull, nsIBoxLayout* aLayoutManager = nsnull, PRBool aDefaultHorizontal = PR_TRUE);
  virtual ~nsXULTreeOuterGroupFrame();

public:
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

  nscoord GetYPosition();
  nscoord GetAvailableHeight();
  NS_IMETHOD GetNumberOfVisibleRows(PRInt32 *aResult) {
    *aResult=GetAvailableHeight() / mRowHeight; return NS_OK;
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

  nsTreeLayoutState GetTreeLayoutState() { return mTreeLayoutState; }
  void SetTreeLayoutState(nsTreeLayoutState aState) { mTreeLayoutState = aState; }

  void PostReflowCallback();

protected: // Data Members

  void ComputeTotalRowCount(PRInt32& aRowCount, nsIContent* aParent);

    // Drag Auto-scrolling. Call HandleAutoScrollTracking() to setup everything and
    // do the scrolling magic. It can be called multiple times, though the 
    // setup will only be done once.
  nsresult HandleAutoScrollTracking ( const nsPoint & aPoint );
  nsresult StartAutoScrollTimer(const nsPoint& aPoint, PRUint32 aDelay);
  nsresult StopAutoScrollTimer();
  nsresult DoAutoScroll(const nsPoint& aPoint);

  nsXULTreeRowGroupInfo* mRowGroupInfo;
  PRInt32 mRowHeight;
  nscoord mOnePixel;
  PRInt32 mCurrentIndex; // Row-based
  PRPackedBool mTreeIsSorted;

    // our auto-scroll event listener registered with the content model. See the discussion
    // in Init() for why this is a weak ref.
  nsDragOverListener* mDragOverListener;
  nsDragAutoScrollTimer* mAutoScrollTimer;      // actually a strong ref
  nsTreeLayoutState mTreeLayoutState;
  PRBool mReflowCallbackPosted;
}; // class nsXULTreeOuterGroupFrame


#endif

