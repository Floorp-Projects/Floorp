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
#ifndef nsTableRowFrame_h__
#define nsTableRowFrame_h__

#include "nscore.h"
#include "nsHTMLContainerFrame.h"

class  nsTableFrame;
class  nsTableCellFrame;
struct RowReflowState;

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
  NS_IMETHOD Init(nsIPresContext&  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

  /** Initialization of data */
  NS_IMETHOD InitChildren(PRInt32 aRowIndex=-1);

  void ResetInitChildren();

  /** instantiate a new instance of nsTableRowFrame.
    * @param aResult    the new object is returned in this out-param
    * @param aContent   the table object to map
    * @param aParent    the parent of the new frame
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  friend nsresult 
  NS_NewTableRowFrame(nsIFrame*& aResult);

  /** @see nsIFrame::Paint */
  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer);


  /** ask all children to paint themselves, without clipping (for cells with rowspan>1)
    * @see nsIFrame::Paint 
    */
  virtual void PaintChildren(nsIPresContext&      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer);

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
  NS_IMETHOD Reflow(nsIPresContext&      aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  virtual void DidResize(nsIPresContext& aPresContext,
                         const nsHTMLReflowState& aReflowState);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::tableRowFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

  NS_IMETHOD GetFrameName(nsString& aResult) const;
  
  /** set mTallestCell to 0 in anticipation of recalculating it */
  void ResetMaxChildHeight();

  /** set mTallestCell to max(mTallestCell, aChildHeight) */ 
  void SetMaxChildHeight(nscoord aChildHeight, nscoord aCellTopMargin, nscoord aCellBottomMargin);

  /** returns the tallest child in this row (ignoring any cell with rowspans) */
  nscoord GetTallestChild() const;
  nscoord GetChildMaxTopMargin() const;
  nscoord GetChildMaxBottomMargin() const;

  PRInt32 GetMaxColumns() const;

  /** returns the ordinal position of this row in its table */
  virtual PRInt32 GetRowIndex() const;

  /** set this row's starting row index */
  void SetRowIndex (int aRowIndex);

  virtual PRBool Contains(const nsPoint& aPoint);

  /** used by yje row group frame code */
  void ReflowCellFrame(nsIPresContext&          aPresContext,
                       const nsHTMLReflowState& aReflowState,
                       nsTableCellFrame*        aCellFrame,
                       nscoord                  aAvailableHeight,
                       nsReflowStatus&          aStatus);
  void InsertCellFrame(nsTableCellFrame* aFrame, nsTableCellFrame* aPrevSibling);

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
  NS_IMETHOD IncrementalReflow(nsIPresContext&       aPresContext,
                               nsHTMLReflowMetrics&  aDesiredSize,
                               RowReflowState&       aReflowState,
                               nsReflowStatus&       aStatus);

  NS_IMETHOD IR_TargetIsChild(nsIPresContext&      aPresContext,
                              nsHTMLReflowMetrics& aDesiredSize,
                              RowReflowState&      aReflowState,
                              nsReflowStatus&      aStatus,
                              nsIFrame *           aNextFrame);

  NS_IMETHOD IR_TargetIsMe(nsIPresContext&      aPresContext,
                           nsHTMLReflowMetrics& aDesiredSize,
                           RowReflowState&      aReflowState,
                           nsReflowStatus&      aStatus);

  NS_IMETHOD IR_CellInserted(nsIPresContext&     aPresContext,
                            nsHTMLReflowMetrics& aDesiredSize,
                            RowReflowState&      aReflowState,
                            nsReflowStatus&      aStatus,
                            nsTableCellFrame *   aInsertedFrame,
                            PRBool               aReplace);

  NS_IMETHOD IR_CellAppended(nsIPresContext&     aPresContext,
                            nsHTMLReflowMetrics& aDesiredSize,
                            RowReflowState&      aReflowState,
                            nsReflowStatus&      aStatus,
                            nsTableCellFrame *   aAppendedFrame);

  NS_IMETHOD IR_DidAppendCell(nsTableCellFrame *aRowFrame);

  NS_IMETHOD IR_CellRemoved(nsIPresContext&     aPresContext,
                           nsHTMLReflowMetrics& aDesiredSize,
                           RowReflowState&      aReflowState,
                           nsReflowStatus&      aStatus,
                           nsTableCellFrame *   aDeletedFrame);

  NS_IMETHOD IR_StyleChanged(nsIPresContext&      aPresContext,
                             nsHTMLReflowMetrics& aDesiredSize,
                             RowReflowState&      aReflowState,
                             nsReflowStatus&      aStatus);

  // row-specific methods

  void GetMinRowSpan(nsTableFrame *aTableFrame);

  void FixMinCellHeight(nsTableFrame *aTableFrame);

  NS_IMETHOD RecoverState(nsIPresContext& aPresContext,
                          RowReflowState& aState,
                          nsIFrame*       aKidFrame,
                          nscoord&        aMaxCellTopMargin,
                          nscoord&        aMaxCellBottomMargin);

  void PlaceChild(nsIPresContext& aPresContext,
                  RowReflowState& aState,
                  nsIFrame*       aKidFrame,
                  const nsRect&   aKidRect,
                  nsSize*         aMaxElementSize,
                  nsSize*         aKidMaxElementSize);

  nscoord ComputeCellXOffset(const RowReflowState& aState,
                             nsIFrame*             aKidFrame,
                             const nsMargin&       aKidMargin) const;
  nscoord ComputeCellAvailWidth(const RowReflowState& aState,
                                nsIFrame*             aKidFrame) const;

  /**
   * Called for a resize reflow. Typically because the column widths have
   * changed. Reflows all the existing table cell frames
   */
  NS_IMETHOD ResizeReflow(nsIPresContext&      aPresContext,
                          nsHTMLReflowMetrics& aDesiredSize,
                          RowReflowState&      aReflowState,
                          nsReflowStatus&      aStatus);

  /**
   * Called for the initial reflow. Creates each table cell frame, and
   * reflows the cell frame to gets its minimum and maximum sizes
   */
  NS_IMETHOD InitialReflow(nsIPresContext&      aPresContext,
                           nsHTMLReflowMetrics& aDesiredSize,
                           RowReflowState&      aReflowState,
                           nsReflowStatus&      aStatus,
                           nsTableCellFrame *   aStartFrame,
                           PRBool               aDoSiblings);

private:
  PRInt32  mRowIndex;
  nscoord  mTallestCell;          // not my height, but the height of my tallest child
  nscoord  mCellMaxTopMargin;
  nscoord  mCellMaxBottomMargin;
  PRInt32  mMinRowSpan;           // the smallest row span among all my child cells
  PRBool   mInitializedChildren;  // PR_TRUE if child cells have been initialized 
                                  // (for now, that means "added to the table", and
                                  // is NOT the same as having nsIFrame::Init() called.)
  
};

inline PRInt32 nsTableRowFrame::GetRowIndex() const
{
  NS_ASSERTION(0<=mRowIndex, "bad state: row index");
  return (mRowIndex);
}

inline void nsTableRowFrame::SetRowIndex (int aRowIndex)
{
  mRowIndex = aRowIndex;
}

inline void nsTableRowFrame::ResetInitChildren()
{ mInitializedChildren=PR_FALSE; } 

#endif
