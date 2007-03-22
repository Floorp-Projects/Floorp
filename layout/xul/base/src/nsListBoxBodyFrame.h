/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David W. Hyatt (hyatt@netscape.com) (Original Author)
 *   Joe Hewitt (hewitt@netscape.com)
 *   Mike Pinkerton (pinkerton@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsListBoxBodyFrame_h
#define nsListBoxBodyFrame_h

#include "nsCOMPtr.h"
#include "nsBoxFrame.h"
#include "nsIListBoxObject.h"
#include "nsIScrollbarMediator.h"
#include "nsIReflowCallback.h"
#include "nsPresContext.h"
#include "nsBoxLayoutState.h"

class nsListScrollSmoother;
nsIFrame* NS_NewListBoxBodyFrame(nsIPresShell* aPresShell,
                                 nsStyleContext* aContext,
                                 PRBool aIsRoot = PR_FALSE,
                                 nsIBoxLayout* aLayoutManager = nsnull);

class nsListBoxBodyFrame : public nsBoxFrame,
                           public nsIListBoxObject,
                           public nsIScrollbarMediator,
                           public nsIReflowCallback
{
  nsListBoxBodyFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRBool aIsRoot = nsnull, nsIBoxLayout* aLayoutManager = nsnull);
  virtual ~nsListBoxBodyFrame();

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILISTBOXOBJECT

  friend nsIFrame* NS_NewListBoxBodyFrame(nsIPresShell* aPresShell,
                                          nsStyleContext* aContext,
                                          PRBool aIsRoot,
                                          nsIBoxLayout* aLayoutManager);
  
  // nsIFrame
  NS_IMETHOD Init(nsIContent*     aContent,
                  nsIFrame*       aParent, 
                  nsIFrame*       aPrevInFlow);
  virtual void Destroy();

  NS_IMETHOD AttributeChanged(PRInt32 aNameSpaceID, nsIAtom* aAttribute, PRInt32 aModType);

  // nsIScrollbarMediator
  NS_IMETHOD PositionChanged(nsISupports* aScrollbar, PRInt32 aOldIndex, PRInt32& aNewIndex);
  NS_IMETHOD ScrollbarButtonPressed(nsISupports* aScrollbar, PRInt32 aOldIndex, PRInt32 aNewIndex);
  NS_IMETHOD VisibilityChanged(nsISupports* aScrollbar, PRBool aVisible);

  // nsIReflowCallback
  virtual PRBool ReflowFinished();

  // nsIBox
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  virtual void MarkIntrinsicWidthsDirty();

  virtual nsSize GetMinSizeForScrollArea(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState);

  // size calculation 
  PRInt32 GetRowCount();
  PRInt32 GetRowHeightAppUnits() { return mRowHeight; }
  PRInt32 GetFixedRowSize();
  void SetRowHeight(PRInt32 aRowHeight);
  nscoord GetYPosition();
  nscoord GetAvailableHeight();
  nscoord ComputeIntrinsicWidth(nsBoxLayoutState& aBoxLayoutState);

  // scrolling
  NS_IMETHOD InternalPositionChangedCallback();
  NS_IMETHOD InternalPositionChanged(PRBool aUp, PRInt32 aDelta);
  nsListScrollSmoother* GetSmoother();
  void VerticalScroll(PRInt32 aDelta);

  // frames
  nsIFrame* GetFirstFrame();
  nsIFrame* GetLastFrame();

  // lazy row creation and destruction
  void CreateRows();
  void DestroyRows(PRInt32& aRowsToLose);
  void ReverseDestroyRows(PRInt32& aRowsToLose);
  nsIBox* GetFirstItemBox(PRInt32 aOffset, PRBool* aCreated);
  nsIBox* GetNextItemBox(nsIBox* aBox, PRInt32 aOffset, PRBool* aCreated);
  PRBool ContinueReflow(nscoord height);
  NS_IMETHOD ListBoxAppendFrames(nsIFrame* aFrameList);
  NS_IMETHOD ListBoxInsertFrames(nsIFrame* aPrevFrame, nsIFrame* aFrameList);
  void OnContentInserted(nsPresContext* aPresContext, nsIContent* aContent);
  void OnContentRemoved(nsPresContext* aPresContext,  nsIFrame* aChildFrame, PRInt32 aIndex);

  void GetListItemContentAt(PRInt32 aIndex, nsIContent** aContent);
  void GetListItemNextSibling(nsIContent* aListItem, nsIContent** aContent, PRInt32& aSiblingIndex);

  void PostReflowCallback();

protected:
  void ComputeTotalRowCount();
  void RemoveChildFrame(nsBoxLayoutState &aState, nsIFrame *aChild);

  // row height
  PRInt32 mRowCount;
  nscoord mRowHeight;
  PRPackedBool mRowHeightWasSet;
  nscoord mAvailableHeight;
  nscoord mStringWidth;

  // frame markers
  nsWeakFrame mTopFrame;
  nsIFrame* mBottomFrame;
  nsIFrame* mLinkupFrame;
  PRInt32 mRowsToPrepend;

  // scrolling
  PRInt32 mCurrentIndex; // Row-based
  PRInt32 mOldIndex; 
  PRPackedBool mScrolling;
  PRPackedBool mAdjustScroll;
  PRInt32 mYPosition;
  nsListScrollSmoother* mScrollSmoother;
  PRInt32 mTimePerRow;

  PRPackedBool mReflowCallbackPosted;
}; 

#endif // nsListBoxBodyFrame_h
