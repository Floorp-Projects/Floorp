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
#ifndef nsTableRowFrame_h__
#define nsTableRowFrame_h__

#include "nscore.h"
#include "nsHTMLContainerFrame.h"

class  nsTableFrame;
class  nsTableCellFrame;

/* ----------- RowReflowState ---------- */

struct RowReflowState {
  // Our reflow state
  const nsHTMLReflowState& reflowState;

  // The body's available size (computed from the body's parent)
  nsSize availSize;

  // the running x-offset
  nscoord x;

  nsTableFrame *tableFrame;

  RowReflowState(const nsHTMLReflowState& aReflowState,
                 nsTableFrame*            aTableFrame)
    : reflowState(aReflowState)
  {
    availSize.width = reflowState.availableWidth;
    availSize.height = reflowState.availableHeight;
    tableFrame = aTableFrame;
    x=0;
  }
};

/**
 * Additional frame-state bits
 */
#define NS_TABLE_MAX_ROW_INDEX  (1<<19)

/**
 * nsTableRowFrame is the frame that maps table rows 
 * (HTML tag TR). This class cannot be reused
 * outside of an nsTableRowGroupFrame.  It assumes that its parent is an nsTableRowGroupFrame,  
 * and its children are nsTableCellFrames.
 * 
 * @see nsTableFrame
 * @see nsTableRowGroupFrame
 * @see nsTableCellFrame
 */
class nsTableRowFrame : public nsHTMLContainerFrame
{
public:
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

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

  /** instantiate a new instance of nsTableRowFrame.
    * @param aResult    the new object is returned in this out-param
    * @param aContent   the table object to map
    * @param aParent    the parent of the new frame
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  friend nsresult 
  NS_NewTableRowFrame(nsIPresShell* aPresShell, nsIFrame** aResult);

  /** @see nsIFrame::Paint */
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer);


  /** ask all children to paint themselves, without clipping (for cells with rowspan>1)
    * @see nsIFrame::Paint 
    */
  virtual void PaintChildren(nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer);

  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame**     aFrame);

  /** calls Reflow for all of its child cells.
    * Cells with rowspan=1 are all set to the same height and stacked horizontally.
    * <P> Cells are not split unless absolutely necessary.
    * <P> Cells are resized in nsTableFrame::BalanceColumnWidths 
    * and nsTableFrame::ShrinkWrapChildren
    *
    * @param aDesiredSize width set to width of the sum of the cells, height set to 
    *                     height of cells with rowspan=1.
    *
    * @see nsIFrame::Reflow
    * @see nsTableFrame::BalanceColumnWidths
    * @see nsTableFrame::ShrinkWrapChildren
    */
  NS_IMETHOD Reflow(nsIPresContext*      aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  virtual void DidResize(nsIPresContext* aPresContext,
                         const nsHTMLReflowState& aReflowState);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::tableRowFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif
 
  void SetTallestCell(nscoord           aHeight,
                      nscoord           aAscent,
                      nscoord           aDescent,
                      nsTableFrame*     aTableFrame = nsnull,
                      nsTableCellFrame* aCellFrame  = nsnull);

  void ResetTallestCell(nscoord aRowStyleHeight);

  // calculate the tallest child when the previous tallest child gets shorter
  void CalcTallestCell();

  /** returns the tallest child in this row (ignoring any cell with rowspans) */
  nscoord GetTallestCell() const;

  // Support for cells with 'vertical-align: baseline'.

  /** 
   * returns the max-ascent amongst all the cells that have 
   * 'vertical-align: baseline', *including* cells with rowspans.
   * returns 0 if we don't have any cell with 'vertical-align: baseline'
   */
  nscoord GetMaxCellAscent() const;
 
#if 0 // nobody uses this
  /** 
   * returns the max-descent amongst all the cells that have
   * 'vertical-align: baseline', *ignoring* any cell with rowspans.
   * returns 0 if we don't have any cell with 'vertical-align: baseline'
   */
  nscoord GetMaxCellDescent() const;
#endif

  /** returns the ordinal position of this row in its table */
  virtual PRInt32 GetRowIndex() const;

  /** set this row's starting row index */
  void SetRowIndex (int aRowIndex);

  virtual PRBool Contains(nsIPresContext* aPresContext, const nsPoint& aPoint);

  void GetMaxElementSize(nsSize& aMaxElementSize) const;

  /** used by yje row group frame code */
  void ReflowCellFrame(nsIPresContext*          aPresContext,
                       const nsHTMLReflowState& aReflowState,
                       nsTableCellFrame*        aCellFrame,
                       nscoord                  aAvailableHeight,
                       nsReflowStatus&          aStatus);
  void InsertCellFrame(nsTableCellFrame* aFrame, nsTableCellFrame* aPrevSibling);

  nsresult CalculateCellActualSize(nsIFrame* aRowFrame,
                                   nscoord&  aDesiredWidth,
                                   nscoord&  aDesiredHeight,
                                   nscoord   aAvailWidth);

protected:

  /** protected constructor.
    * @see NewFrame
    */
  nsTableRowFrame();

  /** implement abstract method on nsHTMLContainerFrame */
  virtual PRIntn GetSkipSides() const;

  /** Incremental Reflow attempts to do column balancing with the minimum number of reflow
    * commands to child elements.  This is done by processing the reflow command,
    * rebalancing column widths (if necessary), then comparing the resulting column widths
    * to the prior column widths and reflowing only those cells that require a reflow.
    *
    * @see Reflow
    */
  NS_IMETHOD IncrementalReflow(nsIPresContext*       aPresContext,
                               nsHTMLReflowMetrics&  aDesiredSize,
                               RowReflowState&       aReflowState,
                               nsReflowStatus&       aStatus);

  NS_IMETHOD IR_TargetIsChild(nsIPresContext*      aPresContext,
                              nsHTMLReflowMetrics& aDesiredSize,
                              RowReflowState&      aReflowState,
                              nsReflowStatus&      aStatus,
                              nsIFrame *           aNextFrame);

  NS_IMETHOD IR_TargetIsMe(nsIPresContext*      aPresContext,
                           nsHTMLReflowMetrics& aDesiredSize,
                           RowReflowState&      aReflowState,
                           nsReflowStatus&      aStatus);

  NS_IMETHOD IR_StyleChanged(nsIPresContext*      aPresContext,
                             nsHTMLReflowMetrics& aDesiredSize,
                             RowReflowState&      aReflowState,
                             nsReflowStatus&      aStatus);

  nsresult AddTableDirtyReflowCommand(nsIPresContext* aPresContext,
                                      nsIPresShell&   aPresShell,
                                      nsIFrame*       aTableFrame);

  // row-specific methods

  void GetMinRowSpan(nsTableFrame *aTableFrame);

  void FixMinCellHeight(nsTableFrame *aTableFrame);

  NS_IMETHOD RecoverState(nsIPresContext* aPresContext,
                          RowReflowState& aState,
                          nsIFrame*       aKidFrame,
                          nsSize*         aMaxElementSize);

  void PlaceChild(nsIPresContext*      aPresContext,
                  RowReflowState&      aState,
                  nsIFrame*            aKidFrame,
                  nsHTMLReflowMetrics& aDesiredSize,
                  nscoord              aX,
                  nscoord              aY,
                  nsSize*              aMaxElementSize,
                  nsSize*              aKidMaxElementSize);

  nscoord ComputeCellXOffset(const RowReflowState& aState,
                             nsIFrame*             aKidFrame,
                             const nsMargin&       aKidMargin) const;
  nscoord ComputeCellAvailWidth(const RowReflowState& aState,
                                nsIFrame*             aKidFrame) const;

  /**
   * Called for a resize reflow. Typically because the column widths have
   * changed. Reflows all the existing table cell frames
   */
  NS_IMETHOD ResizeReflow(nsIPresContext*      aPresContext,
                          nsHTMLReflowMetrics& aDesiredSize,
                          RowReflowState&      aReflowState,
                          nsReflowStatus&      aStatus,
                          PRBool               aDirtyOnly = PR_FALSE);

  /**
   * Called for the initial reflow. Creates each table cell frame, and
   * reflows the cell frame to gets its minimum and maximum sizes
   */
  NS_IMETHOD InitialReflow(nsIPresContext*      aPresContext,
                           nsHTMLReflowMetrics& aDesiredSize,
                           RowReflowState&      aReflowState,
                           nsReflowStatus&      aStatus,
                           nsTableCellFrame *   aStartFrame,
                           PRBool               aDoSiblings);

  nscoord CalculateCellAvailableWidth(nsTableFrame* aTableFrame,
                                      nsIFrame*     aCellFrame,
                                      PRInt32       aCellColIndex,
                                      PRInt32       aNumColSpans,
                                      nscoord       aCellSpacingX);

public:
  struct RowBits {
    int      mRowIndex:20;
    unsigned mMinRowSpan:12;        // the smallest row span among all my child cells
  };

private:
  union {
    PRUint32 mAllBits;
    RowBits  mBits;
  };
  nscoord  mTallestCell;          // not my height, but the height of my tallest child
  nsSize   mMaxElementSize;       // cached max element size

  // max-ascent and max-descent amongst all cells that have 'vertical-align: baseline'
  nscoord mMaxCellAscent;  // does include cells with rowspan > 1
  nscoord mMaxCellDescent; // does *not* include cells with rowspan > 1
};

inline PRInt32 nsTableRowFrame::GetRowIndex() const
{
  return PRInt32(mBits.mRowIndex);
}

inline void nsTableRowFrame::SetRowIndex (int aRowIndex)
{
  NS_PRECONDITION(aRowIndex < NS_TABLE_MAX_ROW_INDEX, "unexpected row index");
  mBits.mRowIndex = aRowIndex;
}

inline void nsTableRowFrame::GetMaxElementSize(nsSize& aMaxElementSize) const
{
  aMaxElementSize.width = mMaxElementSize.width;
  aMaxElementSize.height = mMaxElementSize.height;
}

#endif
