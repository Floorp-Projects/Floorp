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
#ifndef nsTableFrame_h__
#define nsTableFrame_h__

#include "nscore.h"
#include "nsContainerFrame.h"

class nsCellLayoutData;
class nsTableCell;
class nsVoidArray;
class nsTableCellFrame;
class CellData;
class nsITableLayoutStrategy;
struct InnerTableReflowState;
struct nsStylePosition;

/** nsTableFrame maps the inner portion of a table (everything except captions.)
  * Used as a pseudo-frame within nsTableOuterFrame, 
  * it may also be used stand-alone as the top-level frame.
  * The meaningful child frames of nsTableFrame map rowgroups.
  *
  * @author  sclark
  *
  * TODO: make methods virtual so nsTableFrame can be used as a base class in the future.
  */
class nsTableFrame : public nsContainerFrame
{
public:

  /** nsTableOuterFrame has intimate knowledge of the inner table frame */
  friend class nsTableOuterFrame;

  /** instantiate a new instance of nsTableFrame.
    * @param aInstancePtrResult  the new object is returned in this out-param
    * @param aContent            the table object to map
    * @param aParent             the parent of the new frame
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  static nsresult NewFrame(nsIFrame** aInstancePtrResult,
                           nsIContent* aContent,
                           nsIFrame*   aParent);

  /** @see nsIFrame::Paint */
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  /** inner tables are reflowed in two steps.
    * <pre>
    * if mFirstPassValid is false, this is our first time through since content was last changed
    *   set pass to 1
    *   do pass 1
    *     get min/max info for all cells in an infinite space
    *   do column balancing
    *   set mFirstPassValid to true
    *   do pass 2
    *     use column widths to ResizeReflow cells
    *     shrinkWrap Cells in each row to tallest, realigning contents within the cell
    * </pre>
    *
    * @see ResizeReflowPass1
    * @see ResizeReflowPass2
    * @see BalanceColumnWidths
    * @see nsIFrame::ResizeReflow 
    */
  NS_IMETHOD ResizeReflow(nsIPresContext* aPresContext,
                          nsReflowMetrics& aDesiredSize,
                          const nsSize& aMaxSize,
                          nsSize* aMaxElementSize,
                          nsReflowStatus& aStatus);

  /** @see nsIFrame::IncrementalReflow */
  NS_IMETHOD IncrementalReflow(nsIPresContext* aPresContext,
                               nsReflowMetrics& aDesiredSize,
                               const nsSize&    aMaxSize,
                               nsReflowCommand& aReflowCommand,
                               nsReflowStatus& aStatus);

  /** @see nsContainerFrame::CreateContinuingFrame */
  NS_IMETHOD CreateContinuingFrame(nsIPresContext*  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);

  /** resize myself and my children according to the arcane rules of cell height magic. 
    * By default, the height of a cell is the max (height of cells in its row)
    * In the case of a cell with rowspan>1 (lets call this C),
    *   if the sum of the height of the rows spanned (excluding C in the calculation)
    *   is greater than or equal to the height of C, 
    *     the cells in the rows are sized normally and 
    *     the height of C is set to the sum of the heights (taking into account borders, padding, and margins)
    *   else
    *     the height of each row is expanded by a percentage of the difference between
    *     the row's desired height and the height of C
    */
  virtual void ShrinkWrapChildren(nsIPresContext* aPresContext, 
                                  nsReflowMetrics& aDesiredSize,
                                  nsSize* aMaxElementSize);
  
  /** return the column layout data for this inner table frame.
    * if this is a continuing frame, return the first-in-flow's column layout data.
    */
  virtual nsVoidArray *GetColumnLayoutData();

  /** Associate aData with the cell at (aRow,aCol)
    * @return PR_TRUE if the data was successfully associated with a Cell
    *         PR_FALSE if there was an error, such as aRow or aCol being invalid
    */
  virtual PRBool SetCellLayoutData(nsCellLayoutData * aData, nsTableCell *aCell);

  /** Get the layout data associated with the cell at (aRow,aCol)
    * @return PR_NULL if there was an error, such as aRow or aCol being invalid
    *         otherwise, the data is returned.
    */
  virtual nsCellLayoutData * GetCellLayoutData(nsTableCell *aCell);

  /** returns PR_TRUE if this table has proportional width
    */
  PRBool IsProportionalWidth(nsStylePosition* aStylePosition);


          
  /**
    * DEBUG METHOD
    *
    */
  void    ListColumnLayoutData(FILE* out = stdout, PRInt32 aIndent = 0) const;

  
  /** return the width of the column at aColIndex    */
          PRInt32 GetColumnWidth(PRInt32 aColIndex);

  /** set the width of the column at aColIndex to aWidth    */
          void SetColumnWidth(PRInt32 aColIndex, PRInt32 aWidth);

          
  /**
    * Calculate Layout Information
    *
    */
  nsCellLayoutData* FindCellLayoutData(nsTableCell* aCell);
  void    AppendLayoutData(nsVoidArray* aList, nsTableCell* aTableCell);
  void    RecalcLayoutData();
  void    ResetCellLayoutData( nsTableCell* aCell, 
                               nsTableCell* aAbove,
                               nsTableCell* aBelow,
                               nsTableCell* aLeft,
                               nsTableCell* aRight);



protected:

  /** protected constructor.
    * @see NewFrame
    */
  nsTableFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  /** destructor, responsible for mColumnLayoutData and mColumnWidths */
  virtual ~nsTableFrame();

  /** helper method to delete contents of mColumnLayoutData
    * should be called with care (ie, only by destructor)
    */
  virtual void DeleteColumnLayoutData();

  /** first pass of ResizeReflow.  
    * lays out all table content with aMaxSize(NS_UNCONSTRAINEDSIZE,NS_UNCONSTRAINEDSIZE) and
    * a non-null aMaxElementSize so we get all the metrics we need to do column balancing.
    * Pass 1 only needs to be executed once no matter how many times the table is resized, 
    * as long as content and style don't change.  This is managed in the member variable mFirstPassIsValid.
    * The layout information for each cell is cached in mColumLayoutData.
    *
    * @see ResizeReflow
    */
  virtual nsReflowStatus ResizeReflowPass1(nsIPresContext*  aPresContext,
                                           nsReflowMetrics& aDesiredSize,
                                           const nsSize&    aMaxSize,
                                           nsSize*          aMaxElementSize);

  /** second pass of ResizeReflow.
    * lays out all table content with aMaxSize(computed_table_width, given_table_height) 
    * Pass 2 is executed every time the table needs to resize.  An optimization is included
    * so that if the table doesn't need to actually be resized, no work is done (see NeedsReflow).
    * 
    * @param aMinCaptionWidth - the max of all the minimum caption widths.  0 if no captions.
    * @param aMaxCaptionWidth - the max of all the desired caption widths.  0 if no captions.
    *
    * @see ResizeReflow
    * @see NeedsReflow
    */
  virtual nsReflowStatus ResizeReflowPass2(nsIPresContext*  aPresContext,
                                           nsReflowMetrics& aDesiredSize,
                                           const nsSize&    aMaxSize,
                                           nsSize*          aMaxElementSize,
                                           PRInt32 aMinCaptionWidth,
                                           PRInt32 mMaxCaptionWidth);

  nscoord GetTopMarginFor(nsIPresContext* aCX,
                          InnerTableReflowState& aState,
                          const nsMargin& aKidMargin);

  void PlaceChild(nsIPresContext*    aPresContext,
                  InnerTableReflowState& aState,
                  nsIFrame*          aKidFrame,
                  const nsRect&      aKidRect,
                  nsSize*            aMaxElementSize,
                  nsSize&            aKidMaxElementSize);

  /**
   * Reflow the frames we've already created
   *
   * @param   aPresContext presentation context to use
   * @param   aState current inline state
   * @return  true if we successfully reflowed all the mapped children and false
   *            otherwise, e.g. we pushed children to the next in flow
   */
  PRBool        ReflowMappedChildren(nsIPresContext*        aPresContext,
                                     InnerTableReflowState& aState,
                                     nsSize*                aMaxElementSize);
  /**
   * Try and pull-up frames from our next-in-flow
   *
   * @param   aPresContext presentation context to use
   * @param   aState current inline state
   * @return  true if we successfully pulled-up all the children and false
   *            otherwise, e.g. child didn't fit
   */
  PRBool        PullUpChildren(nsIPresContext*        aPresContext,
                               InnerTableReflowState& aState,
                               nsSize*                aMaxElementSize);

  /**
   * Create new frames for content we haven't yet mapped
   *
   * @param   aPresContext presentation context to use
   * @param   aState current inline state
   * @return  frComplete if all content has been mapped and frNotComplete
   *            if we should be continued
   */
  nsReflowStatus  ReflowUnmappedChildren(nsIPresContext*        aPresContext,
                                         InnerTableReflowState& aState,
                                         nsSize*                aMaxElementSize);


  /** assign widths for each column, taking into account the table content, the effective style, 
    * the layout constraints, and the compatibility mode.  Sets mColumnWidths as a side effect.
    * @param aPresContext     the presentation context
    * @param aTableStyle      the resolved style for the table
    * @param aMaxSize         the height and width constraints
    * @param aMaxElementSize  the min size of the largest indivisible object
    */
  virtual void BalanceColumnWidths(nsIPresContext*  aPresContext, 
                                   const nsSize&    aMaxSize, 
                                   nsSize*          aMaxElementSize);

  /** sets the width of the table according to the computed widths of each column. */
  virtual void SetTableWidth(nsIPresContext*  aPresContext);

  /**
    */
  virtual void VerticallyAlignChildren(nsIPresContext* aPresContext,
                                        nscoord* aAscents,
                                        nscoord aMaxAscent,
                                        nscoord aMaxHeight);

  /** given the new parent size, do I really need to do a reflow? */
  virtual PRBool NeedsReflow(const nsSize& aMaxSize);

  /** what stage of reflow is currently in process? */
  virtual PRInt32 GetReflowPass() const;

  /** sets the reflow pass flag.  use with caution! */
  virtual void SetReflowPass(PRInt32 aReflowPass);

  /** returns PR_TRUE if the cached pass 1 data is still valid */
  virtual PRBool IsFirstPassValid() const;

  /** do post processing to setting up style information for the frame */
  virtual NS_METHOD DidSetStyleContext(nsIPresContext* aPresContext);

private:
  void DebugPrintCount() const; // Debugging routine


  /**  table reflow is a multi-pass operation.  Use these constants to keep track of
    *  which pass is currently being executed.
    */
  enum {kPASS_UNDEFINED=0, kPASS_FIRST=1, kPASS_SECOND=2, kPASS_THIRD=3};

  nsVoidArray *mColumnLayoutData;   // array of array of cellLayoutData's
  PRInt32     *mColumnWidths;       // widths of each column
  PRBool       mFirstPassValid;     // PR_TRUE if first pass data is still legit
  PRInt32      mPass;               // which Reflow pass are we currently in?
  PRBool       mIsInvariantWidth;   // PR_TRUE if table width cannot change
  nsITableLayoutStrategy * mTableLayoutStrategy; // the layout strategy for this frame
};

#endif
