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
#include "nsStyleCoord.h"

class nsCellMap;
class nsCellLayoutData;
class nsVoidArray;
class nsTableCellFrame;
class nsTableColFrame;
class nsTableRowGroupFrame;
class nsTableRowFrame;
class nsITableLayoutStrategy;
class nsHTMLValue;
class ColumnInfoCache;
struct InnerTableReflowState;
struct nsStylePosition;
struct nsStyleSpacing;

/* ff1d2780-06d6-11d2-8f37-006008159b0c */
#define NS_TABLEFRAME_CID \
 {0xff1d2780, 0x06d6, 0x11d2, {0x8f, 0x37, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x0c}}

extern const nsIID kTableFrameCID;

/** Data stored by nsCellMap to rationalize rowspan and colspan cells.
  * if mCell is null then mRealCell will be the rowspan/colspan source
  * in addition, if fOverlap is non-null then it will point to the
  * other cell that overlaps this position
  * @see nsCellMap
  * @see nsTableFrame::BuildCellMap
  * @see nsTableFrame::GrowCellMap
  * @see nsTableFrame::BuildCellIntoMap
  * 
  */
class CellData
{
public:
  nsTableCellFrame *mCell;
  CellData *mRealCell;
  CellData *mOverlap;

  CellData();

  ~CellData();
};


/* ============================================================================ */

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

  // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  /** helper method for getting the width of the table's containing block */
  static nscoord GetTableContainerWidth(const nsReflowState& aState);


  static PRBool TableIsAutoWidth(nsTableFrame *aTableFrame,
                                 nsIStyleContext *aTableStyle,
                                 const nsReflowState& aReflowState,
                                 nscoord& aSpecifiedTableWidth);

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
    *     use column widths to Reflow cells
    * </pre>
    *
    * @see ResizeReflowPass1
    * @see ResizeReflowPass2
    * @see BalanceColumnWidths
    * @see nsIFrame::Reflow 
    */
  NS_IMETHOD Reflow(nsIPresContext* aPresContext,
                    nsReflowMetrics& aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus& aStatus);

  /** @see nsContainerFrame::CreateContinuingFrame */
  NS_IMETHOD CreateContinuingFrame(nsIPresContext*  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);

  /** allow the cell and row attributes to effect the column frame
    * currently, the only reason this exists is to support the HTML "rule"
    * that a width attribute on a cell in the first column sets the column width.
    */
  virtual NS_METHOD SetColumnStyleFromCell(nsIPresContext  * aPresContext,
                                           nsTableCellFrame* aCellFrame,
                                           nsTableRowFrame * aRowFrame);

  /** return the column frame corresponding to the given column index
    * there are two ways to do this, depending on whether we have cached
    * column information yet.
    */
  NS_METHOD GetColumnFrame(PRInt32 aColIndex, nsTableColFrame *&aColFrame);

  /** return the column layout data for this inner table frame.
    * if this is a continuing frame, return the first-in-flow's column layout data.
    */
  virtual nsVoidArray *GetColumnLayoutData();

  /** Associate aData with the cell at (aRow,aCol)
    *
    * @param aPresContext -- used to resolve style when initializing caches
    * @param aData  -- the info to cache
    * @param aCell -- the content object for which layout info is being cached
    *
    * @return PR_TRUE if the data was successfully associated with a Cell
    *         PR_FALSE if there was an error, such as aRow or aCol being invalid
    */
  virtual PRBool SetCellLayoutData(nsIPresContext   * aPresContext,
                                   nsCellLayoutData * aData, 
                                   nsTableCellFrame * aCell);

  /** Get the layout data associated with the cell at (aRow,aCol)
    * @return PR_NULL if there was an error, such as aRow or aCol being invalid
    *         otherwise, the data is returned.
    */
  virtual nsCellLayoutData * GetCellLayoutData(nsTableCellFrame *aCell);
          
  /**
    * DEBUG METHOD
    *
    */
  virtual void ListColumnLayoutData(FILE* out = stdout, PRInt32 aIndent = 0) const;

  
  /** return the width of the column at aColIndex    */
  virtual PRInt32 GetColumnWidth(PRInt32 aColIndex);

  /** set the width of the column at aColIndex to aWidth    */
  virtual void SetColumnWidth(PRInt32 aColIndex, nscoord aWidth);
          
  /**
    * Calculate Layout Information
    *
    */
  void    AppendLayoutData(nsVoidArray* aList, nsTableCellFrame* aTableCell);
  void    RecalcLayoutData();

  // Get cell margin information
  NS_IMETHOD GetCellMarginData(nsTableCellFrame* aKidFrame, nsMargin& aMargin);

  /** get cached column information for a subset of the columns
    *
    * @param aType -- information is returned for the subset of columns with aType style
    * @param aOutNumColumns -- out param, the number of columns matching aType
    * @param aOutColumnIndexes -- out param, the indexes of the columns matching aType
    *                             
    * TODO : make aOutColumnIndexes safe
    */
  void GetColumnsByType(const nsStyleUnit aType, 
                        PRInt32& aOutNumColumns,
                        PRInt32 *& aOutColumnIndexes);

  /** return the row span of a cell, taking into account row span magic at the bottom
    * of a table.
    * @param aRowIndex  the first row that contains the cell
    * @param aCell      the content object representing the cell
    * @return  the row span, correcting for row spans that extend beyond the bottom
    *          of the table.
    */
  virtual PRInt32  GetEffectiveRowSpan(PRInt32 aRowIndex, nsTableCellFrame *aCell);

  // For DEBUGGING Purposes Only
  NS_IMETHOD  MoveTo(nscoord aX, nscoord aY);
  NS_IMETHOD  SizeTo(nscoord aWidth, nscoord aHeight);

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
    * @see Reflow
    */
  virtual nsReflowStatus ResizeReflowPass1(nsIPresContext*      aPresContext,
                                           nsReflowMetrics&     aDesiredSize,
                                           const nsReflowState& aReflowState,
                                           nsReflowStatus&      aStatus);

  /** second pass of ResizeReflow.
    * lays out all table content with aMaxSize(computed_table_width, given_table_height) 
    * Pass 2 is executed every time the table needs to resize.  An optimization is included
    * so that if the table doesn't need to actually be resized, no work is done (see NeedsReflow).
    * 
    * @param aMinCaptionWidth - the max of all the minimum caption widths.  0 if no captions.
    * @param aMaxCaptionWidth - the max of all the desired caption widths.  0 if no captions.
    *
    * @see Reflow
    * @see NeedsReflow
    */
  virtual nsReflowStatus ResizeReflowPass2(nsIPresContext*  aPresContext,
                                           nsReflowMetrics& aDesiredSize,
                                           const nsReflowState& aReflowState,
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
  virtual void BalanceColumnWidths(nsIPresContext*      aPresContext, 
                                   const nsReflowState& aReflowState,
                                   const nsSize&        aMaxSize, 
                                   nsSize*              aMaxElementSize);

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
  NS_IMETHOD DidSetStyleContext(nsIPresContext* aPresContext);

  /** Support methods for DidSetStyleContext */
  void      MapBorderMarginPadding(nsIPresContext* aPresContext);
  void      MapHTMLBorderStyle(nsStyleSpacing& aSpacingStyle, nscoord aBorderWidth);
  PRBool    ConvertToPixelValue(nsHTMLValue& aValue, PRInt32 aDefault, PRInt32& aResult);

  /** build as much of the CellMap as possible from the info we have so far 
    */
  virtual void BuildCellMap ();

  /** called whenever the number of columns changes, to increase the storage in mCellMap 
    */
  virtual void GrowCellMap(PRInt32 aColCount);

  /** ResetCellMap is called when the cell structure of the table is changed.
    * Call with caution, only when changing the structure of the table such as 
    * inserting or removing rows, changing the rowspan or colspan attribute of a cell, etc.
    */
  virtual void ResetCellMap ();

  /** ResetColumns is called when the column structure of the table is changed.
    * Call with caution, only when adding or removing columns, changing 
    * column attributes, changing the rowspan or colspan attribute of a cell, etc.
    */
  virtual void ResetColumns ();

  /** sum the columns represented by all nsTableColGroup objects. 
    * if the cell map says there are more columns than this, 
    * add extra implicit columns to the content tree.
    */ 
  virtual void EnsureColumns (nsIPresContext*      aPresContext,
                              nsReflowMetrics&     aDesiredSize,
                              const nsReflowState& aReflowState,
                              nsReflowStatus&      aStatus);

  /** Ensure that the cell map has been built for the table
    */
  virtual void EnsureCellMap();

  virtual void BuildColumnCache(nsIPresContext*      aPresContext,
                                nsReflowMetrics&     aDesiredSize,
                                const nsReflowState& aReflowState,
                                nsReflowStatus&      aStatus);

  /** called every time we discover we have a new cell to add to the table.
    * This could be because we got actual cell content, because of rowspan/colspan attributes, etc.
    * This method changes mCellMap as necessary to account for the new cell.
    *
    * @param aCell     the content object created for the cell
    * @param aRowIndex the row into which the cell is to be inserted
    * @param aColIndex the col into which the cell is to be inserted
    */
  virtual void BuildCellIntoMap (nsTableCellFrame *aCell, PRInt32 aRowIndex, PRInt32 aColIndex);

  /** returns the index of the first child after aStartIndex that is a row group 
    */
  virtual nsTableRowGroupFrame* NextRowGroupFrame (nsTableRowGroupFrame*);

  /** returns the number of rows in this table.
    * if mCellMap has been created, it is asked for the number of rows.<br>
    * otherwise, the content is enumerated and the rows are counted.
    */
  virtual PRInt32 GetRowCount();


  /** return the number of columns as specified by the input. 
    * has 2 side effects:<br>
    * calls SetStartColumnIndex on each nsTableColumn<br>
    * sets mSpecifiedColCount.<br>
    */
  virtual PRInt32 GetSpecifiedColumnCount ();

public:
  virtual void        DumpCellMap() const; 
  virtual nsCellMap*  GetCellMap() const;


private:
  void DebugPrintCount() const; // Debugging routine


  /**  table reflow is a multi-pass operation.  Use these constants to keep track of
    *  which pass is currently being executed.
    */
  enum {kPASS_UNDEFINED=0, kPASS_FIRST=1, kPASS_SECOND=2, kPASS_THIRD=3, kPASS_INCREMENTAL=4};

  nsVoidArray *mColumnLayoutData;   // array of array of cellLayoutData's
  PRInt32     *mColumnWidths;       // widths of each column
  //TODO: move all column info into this object
  ColumnInfoCache *mColCache;       // cached information about the table columns
  PRBool       mFirstPassValid;     // PR_TRUE if first pass data is still legit
  PRInt32      mPass;               // which Reflow pass are we currently in?
  PRBool       mIsInvariantWidth;   // PR_TRUE if table width cannot change
  nsITableLayoutStrategy * mTableLayoutStrategy; // the layout strategy for this frame
  PRInt32      mColCount;           // the number of columns in this table
  nsCellMap*   mCellMap;            // maintains the relationships between rows, cols, and cells
};

#endif
