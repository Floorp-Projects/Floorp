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

#include "nsTableRowGroupFrame.h"
#include "nsVoidArray.h"
#include "nsIScrollbarListener.h"

class nsTreeFrame;
class nsCSSFrameConstructor;
class nsISupportsArray;

class nsTreeRowGroupFrame : public nsTableRowGroupFrame, public nsIScrollbarListener
{
public:
  friend nsresult NS_NewTreeRowGroupFrame(nsIFrame** aNewFrame);

  NS_IMETHOD  GetAdditionalChildListName(PRInt32   aIndex,
                                         nsIAtom** aListName) const;
  NS_IMETHOD FirstChild(nsIAtom* aListName, nsIFrame** aFirstChild) const;

  void SetScrollbarFrame(nsIFrame* aFrame);
  void SetFrameConstructor(nsCSSFrameConstructor* aFrameConstructor) { mFrameConstructor = aFrameConstructor; };
  void SetShouldHaveScrollbar();

  void CreateScrollbar(nsIPresContext& aPresContext);

  void MakeLazy() { mIsLazy = PR_TRUE; };
  PRBool IsLazy() { return mIsLazy; };

  NS_IMETHOD  TreeAppendFrames(nsIFrame*       aFrameList);

  NS_IMETHOD  TreeInsertFrames(nsIFrame*       aPrevFrame,
                               nsIFrame*       aFrameList);

  NS_IMETHOD  GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame);
  void PaintChildren(nsIPresContext&      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer);

  NS_IMETHOD Destroy(nsIPresContext& aPresContext);

  PRBool ContinueReflow(nsIPresContext& aPresContext, nscoord y, nscoord height);

  PRBool IsFull() { return mIsFull; };

  // Responses to changes
  void OnContentAdded(nsIPresContext& aPresContext);
  void OnContentRemoved(nsIPresContext& aPresContext, nsIFrame* aChildFrame);

  virtual nsIFrame* GetFirstFrame();
  virtual nsIFrame* GetLastFrame();
  virtual void GetNextFrame(nsIFrame* aFrame, nsIFrame** aResult);
  virtual PRBool RowsDesireExcessSpace() { return PR_FALSE; };
  virtual PRBool RowGroupDesiresExcessSpace();

  NS_DECL_ISUPPORTS

  NS_IMETHOD PositionChanged(nsIPresContext& aPresContext, PRInt32 aOldIndex, PRInt32 aNewIndex);
  NS_IMETHOD PagedUpDown();

protected:
  nsTreeRowGroupFrame();
  virtual ~nsTreeRowGroupFrame();

  virtual PRBool RowGroupReceivesExcessSpace();
  
  void DestroyRows(nsIPresContext& aPresContext, PRInt32& rowsToLose);
  void ReverseDestroyRows(nsIPresContext& aPresContext, PRInt32& rowsToLose);

  NS_IMETHOD     ReflowBeforeRowLayout(nsIPresContext&      aPresContext,
                                      nsHTMLReflowMetrics& aDesiredSize,
                                      RowGroupReflowState& aReflowState,
                                      nsReflowStatus&      aStatus,
                                      nsReflowReason       aReason);

  NS_IMETHOD     ReflowAfterRowLayout(nsIPresContext&      aPresContext,
                                      nsHTMLReflowMetrics& aDesiredSize,
                                      RowGroupReflowState& aReflowState,
                                      nsReflowStatus&      aStatus,
                                      nsReflowReason       aReason);

  virtual nsIFrame* GetFirstFrameForReflow(nsIPresContext& aPresContext);
  virtual void GetNextFrameForReflow(nsIPresContext& aPresContext, nsIFrame* aFrame, nsIFrame** aResult);

  void LocateFrame(nsIFrame* aStartFrame, nsIFrame** aResult);

  void SetContentChain(nsISupportsArray* aContentChain);
  void InitSubContentChain(nsTreeRowGroupFrame* aRowGroupFrame);

  void ConstructContentChain(nsIContent* aRowContent);
  void FindPreviousRowContent(PRInt32& aDelta, nsIContent* aUpwardHint, 
                              nsIContent* aDownwardHint, nsIContent** aResult);
  void FindRowContentAtIndex(PRInt32& aIndex, nsIContent* aParent, 
                             nsIContent** aResult);
  void GetFirstRowContent(nsIContent** aRowContent);

  void ComputeVisibleRowCount(PRInt32& rowCount, nsIContent* aParent);

public:
  // Helpers that allow access to info. The tree is the primary consumer of this
  // info.

  // Tells you the row and index of a cell (given only the content node).
  // This method is expensive.
  void IndexOfCell(nsIPresContext& aPresContext, nsIContent* aCellContent, 
                   PRInt32& aRowIndex, PRInt32& aColIndex);
  
  // Tells you the row index of a row (given only the content node).
  // This method is expensive.
  void IndexOfRow(nsIPresContext& aPresContext, nsIContent* aRowContent, PRInt32& aRowIndex);

  // Whether or not the row is valid. This is a cheap method, since the total row count
  // is cached.
  PRBool IsValidRow(PRInt32 aRowIndex);

  // This method ensures that a row is onscreen.  It will scroll the tree widget such
  // that the row is at the top of the screen (if the row was offscreen to start with).
  void EnsureRowIsVisible(PRInt32 aRowIndex);

  // This method retrieves a cell at a given index. The intent of this method is that it be
  // cheap.  It should not cause frames to be built, so this should only be called when the
  // cell is onscreen (use EnsureRowIsVisible to guarantee this).
  void GetCellFrameAtIndex(PRInt32 aRowIndex, PRInt32 aColIndex, nsTreeCellFrame** aResult);

  PRInt32 GetVisibleRowCount() { return mRowCount; };

protected: // Data Members
  nsIFrame* mTopFrame; // The current topmost frame in the view.
  nsIFrame* mBottomFrame; // The current bottom frame in the view.
  nsIFrame* mLinkupFrame; // An old top frame that we're trying to link up with.

  PRBool mIsLazy; // Whether or not we're a lazily instantiated beast
  PRBool mIsFull; // Whether or not we have any more room.
  
  nsIFrame* mScrollbar; // Our scrollbar.
  nsFrameList mScrollbarList; // A frame list that holds our scrollbar.
  PRBool mShouldHaveScrollbar; // Whether or not we could potentially have a scrollbar.

  nsISupportsArray* mContentChain; // Our content chain

  nsCSSFrameConstructor* mFrameConstructor; // We don't own this. (No addref/release allowed, punk.)

  nscoord mRowGroupHeight; // The height of the row group.

  PRInt32 mCurrentIndex; // Our current scrolled index.
  PRInt32 mRowCount; // The current number of visible rows.

}; // class nsTreeRowGroupFrame
