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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsTableRowGroupFrame_h__
#define nsTableRowGroupFrame_h__

#include "nscore.h"
#include "nsHTMLContainerFrame.h"
#include "nsIAtom.h"
#include "nsILineIterator.h"

class nsTableFrame;
class nsTableRowFrame;

/* ----------- RowGroupReflowState ---------- */

struct RowGroupReflowState {
  nsIPresContext* mPresContext;  // Our pres context
  const nsHTMLReflowState& reflowState;  // Our reflow state

  // The available size (computed from the parent)
  nsSize availSize;

  // Flags for whether the max size is unconstrained
  PRBool  unconstrainedWidth;
  PRBool  unconstrainedHeight;

  // Running y-offset
  nscoord y;

  // Flag used to set maxElementSize to my first row
  PRBool  firstRow;

  nsTableFrame *tableFrame;

  RowGroupReflowState(nsIPresContext*          aPresContext,
                      const nsHTMLReflowState& aReflowState,
                      nsTableFrame *           aTableFrame)
    : mPresContext(aPresContext),
      reflowState(aReflowState)
  {
    availSize.width = reflowState.availableWidth;
    availSize.height = reflowState.availableHeight;
    y=0;  // border/padding???
    unconstrainedWidth = PRBool(reflowState.availableWidth == NS_UNCONSTRAINEDSIZE);
    unconstrainedHeight = PRBool(reflowState.availableHeight == NS_UNCONSTRAINEDSIZE);
    firstRow = PR_TRUE;
    tableFrame = aTableFrame;
  }

  ~RowGroupReflowState() {
  }
};

#define NS_ITABLEROWGROUPFRAME_IID    \
{ 0xe940e7bc, 0xb534, 0x11d2,  \
  { 0x95, 0xa2, 0x0, 0x60, 0xb0, 0xc3, 0x44, 0x14 } }

// use a bit from nsFrame's frame state bits to determine whether a 
// thead or tfoot should be repeated on every printed page
#define NS_ROWGROUP_REPEATABLE 0x80000000
/**
 * nsTableRowGroupFrame is the frame that maps row groups 
 * (HTML tags THEAD, TFOOT, and TBODY). This class cannot be reused
 * outside of an nsTableFrame.  It assumes that its parent is an nsTableFrame, and 
 * its children are nsTableRowFrames.
 * 
 * @see nsTableFrame
 * @see nsTableRowFrame
 */
class nsTableRowGroupFrame : public nsHTMLContainerFrame, public nsILineIteratorNavigator
{
public:
  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // default constructor supplied by the compiler

  /** instantiate a new instance of nsTableRowGroupFrame.
    * @param aResult    the new object is returned in this out-param
    * @param aContent   the table object to map
    * @param aParent    the parent of the new frame
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  friend nsresult 
  NS_NewTableRowGroupFrame(nsIPresShell* aPresShell, nsIFrame** aResult);

  NS_IMETHOD AppendFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  
  NS_IMETHOD InsertFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);

  NS_IMETHOD RemoveFrame(nsIPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  /** @see nsIFrame::Paint */
  NS_IMETHOD Paint(nsIPresContext* aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  /** ask all children to paint themselves, without clipping (for cells with rowspan>1)
    * @see nsIFrame::Paint 
    */
  virtual void PaintChildren(nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer);

  /**
   * Find the correct descendant frame.
   * Return PR_TRUE if a frame containing the point is found.
   * @see nsContainerFrame::GetFrameForPoint
   */
  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext, const nsPoint& aPoint, nsFramePaintLayer aWhichLayer, nsIFrame** aFrame);

   /** calls Reflow for all of its child rows.
    * Rows are all set to the same width and stacked vertically.
    * <P> rows are not split unless absolutely necessary.
    *
    * @param aDesiredSize width set to width of rows, height set to 
    *                     sum of height of rows that fit in aMaxSize.height.
    *
    * @see nsIFrame::Reflow
    */
  NS_IMETHOD Reflow(nsIPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::tableRowGroupFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

  /** set aCount to the number of child rows (not necessarily == number of child frames) */
  NS_METHOD GetRowCount(PRInt32 &aCount, PRBool aDeepCount = PR_TRUE);

  /** return the table-relative row index of the first row in this rowgroup.
    * if there are no rows, -1 is returned.
    */
  PRInt32 GetStartRowIndex();

  /**
   * Used for header and footer row group frames that are repeated when
   * splitting a table frame.
   *
   * Performs any table specific initialization
   *
   * @param aHeaderFooterFrame the original header or footer row group frame
   * that was repeated
   */
  nsresult  InitRepeatedFrame(nsIPresContext*       aPresContext,
                              nsTableRowGroupFrame* aHeaderFooterFrame);

  
  /**
   * Get the total height of all the row rects
   */
  NS_METHOD GetHeightOfRows(nsIPresContext* aPresContext, nscoord& aResult);
  
  virtual PRBool RowGroupReceivesExcessSpace() { return PR_TRUE; }

  virtual PRBool ContinueReflow(nsIFrame* aFrame, nsIPresContext* aPresContext, nscoord y, nscoord height) { return PR_TRUE; }

  void GetMaxElementSize(nsSize& aMaxElementSize) const;

// nsILineIterator methods
public:
  NS_IMETHOD GetNumLines(PRInt32* aResult);
  NS_IMETHOD GetDirection(PRBool* aIsRightToLeft);
  
  NS_IMETHOD GetLine(PRInt32 aLineNumber,
                     nsIFrame** aFirstFrameOnLine,
                     PRInt32* aNumFramesOnLine,
                     nsRect& aLineBounds,
                     PRUint32* aLineFlags);
  
  NS_IMETHOD FindLineContaining(nsIFrame* aFrame, PRInt32* aLineNumberResult);
  NS_IMETHOD FindLineAt(nscoord aY, PRInt32* aLineNumberResult);
  
  NS_IMETHOD FindFrameAt(PRInt32 aLineNumber,
                         nscoord aX,
                         nsIFrame** aFrameFound,
                         PRBool* aXIsBeforeFirstFrame,
                         PRBool* aXIsAfterLastFrame);

  NS_IMETHOD GetNextSiblingOnLine(nsIFrame*& aFrame, PRInt32 aLineNumber);


protected:

  /** implement abstract method on nsHTMLContainerFrame */
  virtual PRIntn GetSkipSides() const;

  void PlaceChild(nsIPresContext*      aPresContext,
                  RowGroupReflowState& aReflowState,
                  nsIFrame*            aKidFrame,
                  nsHTMLReflowMetrics& aDesiredSize,
                  nscoord              aX,
                  nscoord              aY,
                  nsSize*              aMaxElementSize,
                  nsSize&              aKidMaxElementSize);

  void CalculateRowHeights(nsIPresContext* aPresContext, 
                           nsHTMLReflowMetrics& aDesiredSize,
                           const nsHTMLReflowState& aReflowState);


  /** Incremental Reflow attempts to do column balancing with the minimum number of reflow
    * commands to child elements.  This is done by processing the reflow command,
    * rebalancing column widths (if necessary), then comparing the resulting column widths
    * to the prior column widths and reflowing only those cells that require a reflow.
    *
    * @see Reflow
    */
  NS_IMETHOD IncrementalReflow(nsIPresContext*      aPresContext,
                               nsHTMLReflowMetrics& aDesiredSize,
                               RowGroupReflowState& aReflowState,
                               nsReflowStatus&      aStatus);

  NS_IMETHOD IR_TargetIsChild(nsIPresContext*      aPresContext,
                              nsHTMLReflowMetrics& aDesiredSize,
                              RowGroupReflowState& aReflowState,
                              nsReflowStatus&      aStatus,
                              nsIFrame *           aNextFrame);

  NS_IMETHOD IR_TargetIsMe(nsIPresContext*      aPresContext,
                           nsHTMLReflowMetrics& aDesiredSize,
                           RowGroupReflowState& aReflowState,
                           nsReflowStatus&      aStatus);

  NS_IMETHOD IR_StyleChanged(nsIPresContext*      aPresContext,
                             nsHTMLReflowMetrics& aDesiredSize,
                             RowGroupReflowState& aReflowState,
                             nsReflowStatus&      aStatus);

  nsresult AdjustSiblingsAfterReflow(nsIPresContext*      aPresContext,
                                     RowGroupReflowState& aReflowState,
                                     nsIFrame*            aKidFrame,
                                     nsSize*              aMaxElementSize,
                                     nscoord              aDeltaY);
  
  nsresult RecoverState(RowGroupReflowState& aReflowState,
                        nsIFrame*            aKidFrame,
                        nsSize*              aMaxElementSize);

  /**
   * Reflow the frames we've already created
   *
   * @param   aPresContext presentation context to use
   * @param   aReflowState current inline state
   * @return  true if we successfully reflowed all the mapped children and false
   *            otherwise, e.g. we pushed children to the next in flow
   */
  NS_METHOD     ReflowMappedChildren(nsIPresContext*      aPresContext,
                                     nsHTMLReflowMetrics& aDesiredSize,
                                     RowGroupReflowState& aReflowState,
                                     nsReflowStatus&      aStatus,
                                     nsTableRowFrame *    aStartFrame,
                                     nsReflowReason       aReason,
                                     PRBool               aDoSiblings,
                                     PRBool               aDirtyOnly = PR_FALSE);

  /**
   * Pull-up all the row frames from our next-in-flow
   */
  NS_METHOD PullUpAllRowFrames(nsIPresContext* aPresContext);

  nsresult SplitRowGroup(nsIPresContext*          aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsTableFrame*            aTableFrame,
                         nsReflowStatus&          aStatus);

  NS_IMETHOD     ReflowBeforeRowLayout(nsIPresContext*      aPresContext,
                                      nsHTMLReflowMetrics& aDesiredSize,
                                      RowGroupReflowState& aReflowState,
                                      nsReflowStatus&      aStatus,
                                      nsReflowReason       aReason) { return NS_OK; };

  NS_IMETHOD     ReflowAfterRowLayout(nsIPresContext*      aPresContext,
                                      nsHTMLReflowMetrics& aDesiredSize,
                                      RowGroupReflowState& aReflowState,
                                      nsReflowStatus&      aStatus,
                                      nsReflowReason       aReason) { return NS_OK; };

  nsresult AddTableDirtyReflowCommand(nsIPresContext* aPresContext,
                                      nsIPresShell&   aPresShell,
                                      nsIFrame*       aTableFrame);

  PRBool IsSimpleRowFrame(nsTableFrame* aTableFrame, nsIFrame* aFrame);

  virtual nsIFrame* GetFirstFrameForReflow(nsIPresContext* aPresContext) { return mFrames.FirstChild(); };
  virtual void GetNextFrameForReflow(nsIPresContext* aPresContext, nsIFrame* aFrame, nsIFrame** aResult) { aFrame->GetNextSibling(aResult); };
  void GetNextRowSibling(nsIFrame** aRowFrame);

public:
  virtual nsIFrame* GetFirstFrame() { return mFrames.FirstChild(); };
  virtual nsIFrame* GetLastFrame() { return mFrames.LastChild(); };
  virtual void GetNextFrame(nsIFrame* aFrame, nsIFrame** aResult) { aFrame->GetNextSibling(aResult); };
  virtual PRBool RowsDesireExcessSpace() { return PR_TRUE; };
  virtual PRBool RowGroupDesiresExcessSpace() { return PR_TRUE; };
  PRBool  IsRepeatable();
  void    SetRepeatable(PRBool aRepeatable);

private:
  nsSize   mMaxElementSize;

};

inline void nsTableRowGroupFrame::GetMaxElementSize(nsSize& aMaxElementSize) const
{
  aMaxElementSize = mMaxElementSize;
}

inline PRBool nsTableRowGroupFrame::IsRepeatable()
{
  return (mState & NS_ROWGROUP_REPEATABLE) == NS_ROWGROUP_REPEATABLE;
}

inline void nsTableRowGroupFrame::SetRepeatable(PRBool aRepeatable)
{
  if (aRepeatable) {
    mState |= NS_ROWGROUP_REPEATABLE;
  } else {
    mState &= ~NS_ROWGROUP_REPEATABLE;
  }
}

#endif
