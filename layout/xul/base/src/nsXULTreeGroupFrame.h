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

#include "nsBoxFrame.h"
#include "nsIXULTreeSlice.h"

class nsCSSFrameConstructor;
class nsXULTreeOuterGroupFrame;

class nsXULTreeGroupFrame : public nsBoxFrame, public nsIXULTreeSlice
{
public:
  NS_DECL_ISUPPORTS

  friend nsresult NS_NewXULTreeGroupFrame(nsIPresShell* aPresShell, 
                                          nsIFrame** aNewFrame, 
                                          PRBool aIsRoot = PR_FALSE,
                                          nsIBoxLayout* aLayoutManager = nsnull,
                                          PRBool aDefaultHorizontal = PR_TRUE);

protected:
  nsXULTreeGroupFrame(nsIPresShell* aPresShell, PRBool aIsRoot = nsnull, nsIBoxLayout* aLayoutManager = nsnull, PRBool aDefaultHorizontal = PR_TRUE);
  virtual ~nsXULTreeGroupFrame();

  void LocateFrame(nsIFrame* aStartFrame, nsIFrame** aResult);

public:
  void InitGroup(nsCSSFrameConstructor* aFC, nsIPresContext* aContext, nsXULTreeOuterGroupFrame* aOuterFrame) 
  {
    mFrameConstructor = aFC;
    mPresContext = aContext;
    mOuterFrame = aOuterFrame;
  }

  nsXULTreeOuterGroupFrame* GetOuterFrame() { return mOuterFrame; };
  nsIBox* GetFirstTreeBox();
  nsIBox* GetNextTreeBox(nsIBox* aBox);

  nsIFrame* GetFirstFrame();
  nsIFrame* GetNextFrame(nsIFrame* aCurrFrame);
  nsIFrame* GetLastFrame();
  
  NS_IMETHOD IsDirty(PRBool& aDirtyFlag) { aDirtyFlag = PR_TRUE; return NS_OK; };

  NS_IMETHOD TreeAppendFrames(nsIFrame*       aFrameList);

  NS_IMETHOD TreeInsertFrames(nsIFrame*       aPrevFrame,
                              nsIFrame*       aFrameList);

  // Responses to changes
  void OnContentInserted(nsIPresContext* aPresContext, nsIFrame* aNextSibling, PRInt32 aIndex);
  void OnContentRemoved(nsIPresContext* aPresContext, nsIFrame* aChildFrame, PRInt32 aIndex);

  // nsIXULTreeSlice
  NS_IMETHOD IsOutermostFrame(PRBool* aResult) { *aResult = PR_FALSE; return NS_OK; };
  NS_IMETHOD IsGroupFrame(PRBool* aResult) { *aResult = PR_TRUE; return NS_OK; };
  NS_IMETHOD IsRowFrame(PRBool* aResult) { *aResult = PR_FALSE; return NS_OK; };
  
  virtual nscoord GetAvailableHeight() { return mAvailableHeight; };
  void SetAvailableHeight(nscoord aHeight) { mAvailableHeight = aHeight; };

  virtual nscoord GetYPosition() { return 0; };
  PRBool ContinueReflow(nscoord height);

  void DestroyRows(PRInt32& aRowsToLose);
  void ReverseDestroyRows(PRInt32& aRowsToLose);
  void GetFirstRowContent(nsIContent** aResult);

  void SetContentChain(nsISupportsArray* aContentChain);
  void InitSubContentChain(nsXULTreeGroupFrame* aRowGroupFrame);

protected: // Data Members
  nsCSSFrameConstructor* mFrameConstructor; // We don't own this. (No addref/release allowed, punk.)
  nsIPresContext* mPresContext;
  nsXULTreeOuterGroupFrame* mOuterFrame;
  nscoord mAvailableHeight;
  nsIFrame* mTopFrame;
  nsIFrame* mBottomFrame;
  nsIFrame* mLinkupFrame;
  nsISupportsArray* mContentChain; // Our content chain
}; // class nsXULTreeGroupFrame
