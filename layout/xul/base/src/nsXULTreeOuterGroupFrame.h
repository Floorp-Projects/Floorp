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
#include "nsBoxLayoutState.h"
#include "nsISupportsArray.h"
#include "nsXULTreeGroupFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsIPresContext.h"

class nsCSSFrameConstructor;

#define TREE_LINE_HEIGHT 225

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

class nsXULTreeOuterGroupFrame : public nsXULTreeGroupFrame, public nsIScrollbarMediator
{
public:
  NS_DECL_ISUPPORTS

  friend nsresult NS_NewXULTreeOuterGroupFrame(nsIPresShell* aPresShell, 
                                          nsIFrame** aNewFrame, 
                                          PRBool aIsRoot = PR_FALSE,
                                          nsIBoxLayout* aLayoutManager = nsnull,
                                          PRBool aDefaultHorizontal = PR_TRUE);

  NS_IMETHOD Init(nsIPresContext* aPresContext, nsIContent* aContent,
                  nsIFrame* aParent, nsIStyleContext* aContext, nsIFrame* aPrevInFlow);

protected:
  nsXULTreeOuterGroupFrame(nsIPresShell* aPresShell, PRBool aIsRoot = nsnull, nsIBoxLayout* aLayoutManager = nsnull, PRBool aDefaultHorizontal = PR_TRUE);
  virtual ~nsXULTreeOuterGroupFrame();

  void ComputeTotalRowCount(PRInt32& aRowCount, nsIContent* aParent);

public:
  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
  {
    NeedsRecalc();
    return nsXULTreeGroupFrame::GetPrefSize(aBoxLayoutState, aSize);
  };

  NS_IMETHOD IsOutermostFrame(PRBool *aResult) { *aResult = PR_TRUE; return NS_OK; };

  PRInt32 GetRowCount() { if (mRowGroupInfo && (mRowGroupInfo->mRowCount != -1)) return mRowGroupInfo->mRowCount; PRInt32 count = 0;
                          ComputeTotalRowCount(count, mContent); mRowGroupInfo->mRowCount = count; return count; };

  PRInt32 GetRowHeightTwips() { 
    return mRowHeight;
  }

  void ClearRowGroupInfo() { if (mRowGroupInfo) mRowGroupInfo->Clear(); NeedsRecalc(); };
  
  void SetRowHeight(PRInt32 aRowHeight) 
  { 
    if (mRowHeight != aRowHeight) { 
      mRowHeight = aRowHeight;
      nsBoxLayoutState state(mPresContext);
      MarkDirtyChildren(state); 
    } 
  };

  nscoord GetYPosition();
  nscoord GetAvailableHeight();

  NS_IMETHOD PositionChanged(PRInt32 aOldIndex, PRInt32 aNewIndex);
  NS_IMETHOD ScrollbarButtonPressed(PRInt32 aOldIndex, PRInt32 aNewIndex);

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
  
  NS_IMETHOD InternalPositionChanged(PRBool aUp, PRInt32 aDelta);

protected: // Data Members
  nsXULTreeRowGroupInfo* mRowGroupInfo;
  PRInt32 mRowHeight;
  nscoord mOnePixel;
  PRInt32 mCurrentIndex; // Row-based
  PRInt32 mTwipIndex; // Not really accurate. Used to handle thumb dragging
}; // class nsXULTreeOuterGroupFrame
