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
 * Author:
 * Eric D Vaughan
 *
 * Contributor(s): 
 */

#ifndef nsBoxToBlockAdaptor_h___
#define nsBoxToBlockAdaptor_h___

#include "nsIBoxToBlockAdaptor.h"
#include "nsBox.h"
class nsSpaceManager;

class nsBoxToBlockAdaptor : public nsBox, public nsIBoxToBlockAdaptor {

public:

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMaxSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetFlex(nsBoxLayoutState& aBoxLayoutState, nscoord& aFlex);
  NS_IMETHOD GetAscent(nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent);
  NS_IMETHOD IsCollapsed(nsBoxLayoutState& aBoxLayoutState, PRBool& aCollapsed);
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  NS_IMETHOD GetFrame(nsIFrame** aFrame);
  NS_IMETHOD SetIncludeOverflow(PRBool aInclude);
  NS_IMETHOD GetOverflow(nsSize& aOverflow);
  NS_IMETHOD NeedsRecalc();
  NS_IMETHOD SetParentBox(nsIBox* aParent);

  NS_IMETHOD Recycle(nsIPresShell* aPresShell);

  void* operator new(size_t sz, nsIPresShell* aPresShell);
  void operator delete(void* aPtr, size_t sz);

  
  nsBoxToBlockAdaptor(nsIPresShell* aShell, nsIFrame* aFrame);
  virtual ~nsBoxToBlockAdaptor();

protected:
  virtual void GetBoxName(nsAutoString& aName);
  virtual PRBool HasStyleChange();
  virtual void SetStyleChangeFlag(PRBool aDirty);

  virtual PRBool GetWasCollapsed(nsBoxLayoutState& aState);
  virtual void SetWasCollapsed(nsBoxLayoutState& aState, PRBool aWas);

  virtual nsresult Reflow(nsBoxLayoutState& aState,
                   nsIPresContext*   aPresContext,
                   nsHTMLReflowMetrics&     aDesiredSize,
                   const nsHTMLReflowState& aReflowState,
                   nsReflowStatus&          aStatus,
                   nscoord aX,
                   nscoord aY,
                   nscoord aWidth,
                   nscoord aHeight,
                   PRBool aMoveFrame = PR_TRUE);

  nsIFrame* mFrame;
  nsSize mPrefSize;
  nsSize mMinSize;
  nsSize mMaxSize;
  nscoord mFlex;
  nscoord mAscent;
  PRBool mPrefNeedsRecalc;
  nsSpaceManager* mSpaceManager;
  nsSize mLastSize;
  nscoord mMinWidth;
  PRBool mWasCollapsed;
  nscoord mCachedMaxElementHeight;
  PRBool mStyleChange;
  PRBool mSizeSet;
  nsSize mOverflow;
  PRBool mIncludeOverflow;
  nsIPresShell* mPresShell;
};

#endif

