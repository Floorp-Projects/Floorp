/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsTableRowGroupFrame_h__
#define nsTableRowGroupFrame_h__

#include "nscore.h"
#include "nsHTMLContainerFrame.h"
#include "nsIAtom.h"

class nsTableFrame;
class nsTableRowFrame;

/* ----------- RowGroupReflowState ---------- */

struct RowGroupReflowState {
  nsIPresContext& mPresContext;  // Our pres context
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

  RowGroupReflowState(nsIPresContext&          aPresContext,
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

/**
 * nsTableRowGroupFrame is the frame that maps row groups 
 * (HTML tags THEAD, TFOOT, and TBODY). This class cannot be reused
 * outside of an nsTableFrame.  It assumes that its parent is an nsTableFrame, and 
 * its children are nsTableRowFrames.
 * 
 * @see nsTableFrame
 * @see nsTableRowFrame
 */
class nsTableRowGroupFrame : public nsHTMLContainerFrame
{
public:

  // default constructor supplied by the compiler

  /** instantiate a new instance of nsTableRowGroupFrame.
    * @param aResult    the new object is returned in this out-param
    * @param aContent   the table object to map
    * @param aParent    the parent of the new frame
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  friend nsresult 
  NS_NewTableRowGroupFrame(nsIFrame** aResult);

  NS_METHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD AppendFrames(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  
  NS_IMETHOD InsertFrames(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);

  NS_IMETHOD RemoveFrame(nsIPresContext& aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  /** @see nsIFrame::Paint */
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  /** ask all children to paint themselves, without clipping (for cells with rowspan>1)
    * @see nsIFrame::Paint 
    */
  virtual void PaintChildren(nsIPresContext&      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer);

  /**
   * Find the correct descendant frame.
   * Return PR_TRUE if a frame containing the point is found.
   * @see nsContainerFrame::GetFrameForPoint
   */
  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame);

   /** calls Reflow for all of its child rows.
    * Rows are all set to the same width and stacked vertically.
    * <P> rows are not split unless absolutely necessary.
    *
    * @param aDesiredSize width set to width of rows, height set to 
    *                     sum of height of rows that fit in aMaxSize.height.
    *
    * @see nsIFrame::Reflow
    */
  NS_IMETHOD Reflow(nsIPresContext& aPresContext,
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
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

  NS_IMETHOD GetFrameName(nsString& aResult) const;

  /** set aCount to the number of child rows (not necessarily == number of child frames) */
  NS_METHOD GetRowCount(PRInt32 &aCount, PRBool aDeepCount = PR_TRUE);

  /** return the table-relative row index of the first row in this rowgroup.
    * if there are no rows, -1 is returned.
    */
  PRInt32 GetStartRowIndex();

  /** get the maximum number of columns taken up by any row in this rowgroup */
  NS_METHOD GetMaxColumns(PRInt32 &aMaxColumns);

  /**
   * Used for header and footer row group frames that are repeated when
   * splitting a table frame.
   *
   * Performs any table specific initialization
   *
   * @param aHeaderFooterFrame the original header or footer row group frame
   * that was repeated
   */
  nsresult  InitRepeatedFrame(nsTableRowGroupFrame* aHeaderFooterFrame);

  
  /**
   * Get the total height of all the row rects
   */
  NS_METHOD GetHeightOfRows(nscoord& aResult);
  
  virtual PRBool RowGroupReceivesExcessSpace() { return PR_TRUE; }

  virtual PRBool ContinueReflow(nsIPresContext& aPresContext, nscoord y, nscoord height) { return PR_TRUE; }

  void GetMaxElementSize(nsSize& aMaxElementSize) const;

protected:

  /** implement abstract method on nsHTMLContainerFrame */
  virtual PRIntn GetSkipSides() const;

  void PlaceChild(nsIPresContext&      aPresContext,
                  RowGroupReflowState& aReflowState,
                  nsIFrame*            aKidFrame,
                  const nsRect&        aKidRect,
                  nsSize*              aMaxElementSize,
                  nsSize&              aKidMaxElementSize);

  void CalculateRowHeights(nsIPresContext& aPresContext, 
                           nsHTMLReflowMetrics& aDesiredSize,
                           const nsHTMLReflowState& aReflowState);


  /** Incremental Reflow attempts to do column balancing with the minimum number of reflow
    * commands to child elements.  This is done by processing the reflow command,
    * rebalancing column widths (if necessary), then comparing the resulting column widths
    * to the prior column widths and reflowing only those cells that require a reflow.
    *
    * @see Reflow
    */
  NS_IMETHOD IncrementalReflow(nsIPresContext&      aPresContext,
                               nsHTMLReflowMetrics& aDesiredSize,
                               RowGroupReflowState& aReflowState,
                               nsReflowStatus&      aStatus);

  NS_IMETHOD IR_TargetIsChild(nsIPresContext&      aPresContext,
                              nsHTMLReflowMetrics& aDesiredSize,
                              RowGroupReflowState& aReflowState,
                              nsReflowStatus&      aStatus,
                              nsIFrame *           aNextFrame);

  NS_IMETHOD IR_TargetIsMe(nsIPresContext&      aPresContext,
                           nsHTMLReflowMetrics& aDesiredSize,
                           RowGroupReflowState& aReflowState,
                           nsReflowStatus&      aStatus);

  NS_IMETHOD IR_StyleChanged(nsIPresContext&      aPresContext,
                             nsHTMLReflowMetrics& aDesiredSize,
                             RowGroupReflowState& aReflowState,
                             nsReflowStatus&      aStatus);

  NS_IMETHOD DidAppendRow(nsTableRowFrame *aRowFrame);

  PRBool NoRowsFollow();

  nsresult AdjustSiblingsAfterReflow(nsIPresContext&      aPresContext,
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
  NS_METHOD     ReflowMappedChildren(nsIPresContext&      aPresContext,
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
  NS_METHOD PullUpAllRowFrames(nsIPresContext& aPresContext);

  nsresult SplitRowGroup(nsIPresContext&          aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsTableFrame*            aTableFrame,
                         nsReflowStatus&          aStatus);

  NS_IMETHOD     ReflowBeforeRowLayout(nsIPresContext&      aPresContext,
                                      nsHTMLReflowMetrics& aDesiredSize,
                                      RowGroupReflowState& aReflowState,
                                      nsReflowStatus&      aStatus,
                                      nsReflowReason       aReason) { return NS_OK; };

  NS_IMETHOD     ReflowAfterRowLayout(nsIPresContext&      aPresContext,
                                      nsHTMLReflowMetrics& aDesiredSize,
                                      RowGroupReflowState& aReflowState,
                                      nsReflowStatus&      aStatus,
                                      nsReflowReason       aReason) { return NS_OK; };

  nsresult AddTableDirtyReflowCommand(nsIPresContext& aPresContext,
                                      nsIPresShell&   aPresShell,
                                      nsIFrame*       aTableFrame);

  PRBool IsSimpleRowFrame(nsTableFrame* aTableFrame, nsIFrame* aFrame);

  virtual nsIFrame* GetFirstFrameForReflow(nsIPresContext& aPresContext) { return mFrames.FirstChild(); };
  virtual void GetNextFrameForReflow(nsIPresContext& aPresContext, nsIFrame* aFrame, nsIFrame** aResult) { aFrame->GetNextSibling(aResult); };
  void GetNextRowSibling(nsIFrame** aRowFrame);

public:
  virtual nsIFrame* GetFirstFrame() { return mFrames.FirstChild(); };
  virtual nsIFrame* GetLastFrame() { return mFrames.LastChild(); };
  virtual void GetNextFrame(nsIFrame* aFrame, nsIFrame** aResult) { aFrame->GetNextSibling(aResult); };
  virtual PRBool RowsDesireExcessSpace() { return PR_TRUE; };
  virtual PRBool RowGroupDesiresExcessSpace() { return PR_TRUE; };

private:
  nsIAtom *mType;
  nsSize   mMaxElementSize;

};

inline void nsTableRowGroupFrame::GetMaxElementSize(nsSize& aMaxElementSize) const
{
  aMaxElementSize = mMaxElementSize;
}


#endif
