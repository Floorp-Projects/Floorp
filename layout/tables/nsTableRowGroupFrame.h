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
struct RowGroupReflowState;

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
  NS_NewTableRowGroupFrame(nsIFrame*& aResult);

  NS_METHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

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

  NS_IMETHOD GetFrameName(nsString& aResult) const;

  /** set aCount to the number of child rows (not necessarily == number of child frames) */
  NS_METHOD GetRowCount(PRInt32 &aCount);

  /** return the table-relative row index of the first row in this rowgroup.
    * if there are no rows, -1 is returned.
    */
  PRInt32 GetStartRowIndex();

  /** get the maximum number of columns taken up by any row in this rowgroup */
  NS_METHOD GetMaxColumns(PRInt32 &aMaxColumns) const;

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

  NS_IMETHOD IR_RowInserted(nsIPresContext&      aPresContext,
                            nsHTMLReflowMetrics& aDesiredSize,
                            RowGroupReflowState& aReflowState,
                            nsReflowStatus&      aStatus,
                            nsTableRowFrame *    aInsertedFrame,
                            PRBool               aReplace);

  NS_IMETHOD IR_RowAppended(nsIPresContext&      aPresContext,
                            nsHTMLReflowMetrics& aDesiredSize,
                            RowGroupReflowState& aReflowState,
                            nsReflowStatus&      aStatus,
                            nsTableRowFrame *    aAppendedFrame);

  NS_IMETHOD IR_RowRemoved(nsIPresContext&      aPresContext,
                           nsHTMLReflowMetrics& aDesiredSize,
                           RowGroupReflowState& aReflowState,
                           nsReflowStatus&      aStatus,
                           nsTableRowFrame *    aDeletedFrame);

  NS_IMETHOD IR_StyleChanged(nsIPresContext&      aPresContext,
                             nsHTMLReflowMetrics& aDesiredSize,
                             RowGroupReflowState& aReflowState,
                             nsReflowStatus&      aStatus);

  NS_IMETHOD DidAppendRow(nsTableRowFrame *aRowFrame);

  PRBool NoRowsFollow();

  nsresult AdjustSiblingsAfterReflow(nsIPresContext&      aPresContext,
                                     RowGroupReflowState& aReflowState,
                                     nsIFrame*            aKidFrame,
                                     nscoord              aDeltaY);

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
                                     PRBool               aDoSiblings);

  /**
   * Pull-up all the row frames from our next-in-flow
   */
  NS_METHOD PullUpAllRowFrames(nsIPresContext& aPresContext);

  nsresult SplitRowGroup(nsIPresContext&          aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsTableFrame*            aTableFrame,
                         nsReflowStatus&          aStatus);

private:
  nsIAtom *mType;

};


#endif
