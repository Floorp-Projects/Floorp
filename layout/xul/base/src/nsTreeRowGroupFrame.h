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
 * Contributor(s): 
 */

#include "nsTableRowGroupFrame.h"
#include "nsVoidArray.h"
#include "nsIScrollbarListener.h"
#include "nsIStyleContext.h"
#include "nsCOMPtr.h"

class nsTreeFrame;
class nsTreeCellFrame;
class nsCSSFrameConstructor;
class nsISupportsArray;
class nsTreeItemDragCapturer;


class nsTreeRowGroupFrame : public nsTableRowGroupFrame, public nsIScrollbarListener
{
public:
  friend nsresult NS_NewTreeRowGroupFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

    // hook in to setup d&d capturers
  NS_IMETHOD Init ( nsIPresContext*  aPresContext, nsIContent* aContent,
                      nsIFrame* aParent, nsIStyleContext* aContext, nsIFrame* aPrevInFlow) ;

  NS_IMETHOD  GetAdditionalChildListName(PRInt32   aIndex,
                                         nsIAtom** aListName) const;
  NS_IMETHOD FirstChild(nsIPresContext* aPresContext,
                        nsIAtom*        aListName,
                        nsIFrame**      aFirstChild) const;

  void SetScrollbarFrame(nsIPresContext* aPresContext, nsIFrame* aFrame);
  void SetFrameConstructor(nsCSSFrameConstructor* aFrameConstructor) { mFrameConstructor = aFrameConstructor; };
  
  void CreateScrollbar(nsIPresContext* aPresContext);
  void CollapseScrollbar(PRBool aHide, nsIPresContext* aPresContext, nsIFrame* aCurrentFrame);

  NS_IMETHOD  TreeAppendFrames(nsIFrame*       aFrameList);

  NS_IMETHOD  TreeInsertFrames(nsIFrame*       aPrevFrame,
                               nsIFrame*       aFrameList);

  NS_IMETHOD  GetFrameForPoint(nsIPresContext* aPresContext, const nsPoint& aPoint, nsFramePaintLayer aWhichLayer, nsIFrame** aFrame);
  void PaintChildren(nsIPresContext*      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer);

  NS_IMETHOD Destroy(nsIPresContext* aPresContext);

  PRBool ContinueReflow(nsIFrame* aFrame, nsIPresContext* aPresContext, nscoord y, nscoord height);

  PRBool IsFull() { return mIsFull; };

  // Responses to changes
  void OnContentAdded(nsIPresContext* aPresContext);
  void OnContentInserted(nsIPresContext* aPresContext, nsIFrame* aNextSibling, PRInt32 aIndex);
  void OnContentRemoved(nsIPresContext* aPresContext, nsIFrame* aChildFrame, PRInt32 aIndex);
  void ReflowScrollbar(nsIPresContext* aPresContext);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext, nsIContent* aChild,
                                 PRInt32 aNameSpaceID, nsIAtom* aAttribute, PRInt32 aHint) ;

    // nsIHTMLReflow overrides
  NS_IMETHOD Paint(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect, nsFramePaintLayer aWhichLayer);
                    
  virtual nsIFrame* GetFirstFrame();
  virtual nsIFrame* GetLastFrame();
  virtual void GetNextFrame(nsIFrame* aFrame, nsIFrame** aResult);
  virtual PRBool RowsDesireExcessSpace() { return PR_FALSE; };
  virtual PRBool RowGroupDesiresExcessSpace();

  NS_DECL_ISUPPORTS

  NS_IMETHOD PositionChanged(nsIPresContext* aPresContext, PRInt32 aOldIndex, PRInt32 aNewIndex);
  NS_IMETHOD PagedUpDown();

  void ScrollByLines(nsIPresContext* aPresContext, PRInt32 lines);

protected:
  nsTreeRowGroupFrame();
  virtual ~nsTreeRowGroupFrame();

  virtual PRBool RowGroupReceivesExcessSpace();
  
  void DestroyRows(nsTableFrame* aTableFrame, nsIPresContext* aPresContext, PRInt32& rowsToLose);
  void ReverseDestroyRows(nsTableFrame* aTableFrame, nsIPresContext* aPresContext, PRInt32& rowsToLose);

  NS_IMETHOD     ReflowBeforeRowLayout(nsIPresContext*      aPresContext,
                                      nsHTMLReflowMetrics& aDesiredSize,
                                      RowGroupReflowState& aReflowState,
                                      nsReflowStatus&      aStatus,
                                      nsReflowReason       aReason);

  NS_IMETHOD     ReflowAfterRowLayout(nsIPresContext*      aPresContext,
                                      nsHTMLReflowMetrics& aDesiredSize,
                                      RowGroupReflowState& aReflowState,
                                      nsReflowStatus&      aStatus,
                                      nsReflowReason       aReason);

  NS_IMETHOD     IR_TargetIsChild(nsIPresContext*      aPresContext,
                                      nsHTMLReflowMetrics& aDesiredSize,
                                      RowGroupReflowState& aReflowState,
                                      nsReflowStatus&      aStatus,
                                      nsIFrame *           aNextFrame);

  virtual nsIFrame* GetFirstFrameForReflow(nsIPresContext* aPresContext);
  virtual void GetNextFrameForReflow(nsIPresContext* aPresContext, nsIFrame* aFrame, nsIFrame** aResult);

  void LocateFrame(nsIFrame* aStartFrame, nsIFrame** aResult);

  void SetContentChain(nsISupportsArray* aContentChain);
  void InitSubContentChain(nsTreeRowGroupFrame* aRowGroupFrame);

  void ConstructContentChain(nsIContent* aRowContent);
  void ConstructOldContentChain(nsIPresContext* aPresContext, nsIContent* aOldRowContent);
  void CreateOldContentChain(nsIPresContext* aPresContext, nsIContent* aOldRowContent, nsIContent* topOfChain);

  void FindChildOfCommonContentChainAncestor(nsIContent *startContent, nsIContent **child);

  PRBool IsAncestor(nsIContent *aRowContent, nsIContent *aOldRowContent, nsIContent **firstDescendant);

  void FindPreviousRowContent(PRInt32& aDelta, nsIContent* aUpwardHint, 
                              nsIContent* aDownwardHint, nsIContent** aResult);
  static void FindRowContentAtIndex(PRInt32& aIndex, nsIContent* aParent, 
                             nsIContent** aResult);
  void MarkTreeAsDirty(nsIPresContext* aPresContext, nsTreeFrame* aTreeFrame);

  void GetFirstRowContent(nsIContent** aRowContent);

  void ComputeTotalRowCount(PRInt32& rowCount, nsIContent* aParent);

  static void ClearFrameRefs(nsIPresContext* aPresContext, nsIPresShell* aPresShell, nsIFrame *aParent);

    // handle drawing the drop feedback
  void PaintDropFeedback ( nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext ) ;
  void PaintSortedDropFeedback ( nscolor inColor, nsIRenderingContext& inRenderingContext, float & inP2T ) ;
  void PaintOnContainerDropFeedback ( nscolor inColor, nsIRenderingContext& inRenderingContext, 
                                        nsIPresContext* inPresContext, float & inP2T ) ;
  void PaintInBetweenDropFeedback ( nscolor inColor, nsIRenderingContext& inRenderingContext, 
                                        nsIPresContext* inPresContext, float & inP2T ) ;

    // helpers for drop feedback
  PRInt32 FindIndentation ( nsIPresContext* inPresContext, nsIFrame* inStartFrame ) const ;
  void FindFirstChildTreeItemFrame ( nsIPresContext* inPresContext, nsIFrame** outChild ) const ;
  PRBool IsOpenContainer ( ) const ;
  nscolor GetColorFromStyleContext ( nsIPresContext* inPresContext, nsIAtom* inAtom, 
                                       nscolor inDefaultColor ) ;

public:
  // Helpers that allow access to info. The tree is the primary consumer of this
  // info.

  // Tells you the row and index of a cell (given only the content node).
  // This method is expensive.
  void IndexOfCell(nsIPresContext* aPresContext, nsIContent* aCellContent, 
                   PRInt32& aRowIndex, PRInt32& aColIndex);
  
  // Tells you the row index of a row (given only the content node).
  // This method is expensive.
  void IndexOfRow(nsIPresContext* aPresContext, nsIContent* aRowContent, PRInt32& aRowIndex);

  // get the first frame
  void GetFirstRowFrame(nsTableRowFrame **aRowFrame);

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

  PRInt32 GetInsertionIndex(nsIFrame *aFrame, PRInt32 aCurrentIndex, PRBool& aDone);
  PRInt32 GetInsertionIndexForContent(nsTableFrame* aTableFrame,
                                      nsIPresContext* aPresContext,
                                      nsIContent *aContent);

  // Also returns the number of new columns that were added
  PRInt32 AddRowToMap(nsTableFrame*   aTableFrame,
                      nsIPresContext& aPresContext,
                      nsIContent*     aContent, 
                      nsIFrame*       aCurrentFrame);
  
  void ClearRowGroupHeight() { mRowGroupHeight = -1; }

  static PRBool IsTableRowGroupFrame(nsIFrame*);
  static PRBool IsTableRowFrame(nsIFrame*);
  
  PRBool IsOutermostTreeItem ( ) const { return this == mOuterFrame; }
  
protected: // Data Members
  nsIFrame* mTopFrame; // The current topmost frame in the view.
  nsIFrame* mBottomFrame; // The current bottom frame in the view.
  nsIFrame* mLinkupFrame; // An old top frame that we're trying to link up with.

  PRBool mIsLazy; // Whether or not we're a lazily instantiated beast
  PRBool mIsFull; // Whether or not we have any more room.
  
  nsIFrame* mScrollbar; // Our scrollbar.
  nsFrameList mScrollbarList; // A frame list that holds our scrollbar.
  
  nsTreeRowGroupFrame* mOuterFrame; // A pointer to our outermost row group frame.

  nsISupportsArray* mContentChain; // Our content chain

  nsCSSFrameConstructor* mFrameConstructor; // We don't own this. (No addref/release allowed, punk.)

  nscoord mRowGroupHeight; // The height of the row group.
  nscoord mRowGroupWidth; // The width of the row group.

  PRInt32 mCurrentIndex; // Our current scrolled index.
  PRInt32 mRowCount; // The current number of visible rows.

    // our event capturer registered with the content model. See the discussion
    // in Init() for why this is a weak ref.
  nsTreeItemDragCapturer* mDragCapturer;

    // only used during drag and drop for drop feedback. These are not
    // guaranteed to be meaningful when no drop is underway.
  PRInt32 mYDropLoc;
  PRBool mDropOnContainer;
  PRBool mTreeIsSorted;         // only needs to be on the topLevel rowGroup, I _think_

}; // class nsTreeRowGroupFrame
