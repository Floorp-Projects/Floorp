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
#include "nsVoidArray.h"
#include "nsHTMLContainerFrame.h"
#include "nsStyleCoord.h"
#include "nsStyleConsts.h"
#include "nsIStyleContext.h"
#include "nsIFrameReflow.h"   // for nsReflowReason enum


class nsCellMap;
class nsTableCellFrame;
class nsTableColFrame;
class nsTableRowGroupFrame;
class nsTableRowFrame;
class nsTableColGroupFrame;
class nsITableLayoutStrategy;
class nsHTMLValue;
class ColumnInfoCache;
struct InnerTableReflowState;
struct nsStylePosition;
struct nsStyleSpacing;

/**
 * Child list name indices
 * @see #GetAdditionalChildListName()
 */
#define NS_TABLE_FRAME_COLGROUP_LIST_INDEX 0
#define NS_TABLE_FRAME_LAST_LIST_INDEX    NS_TABLE_FRAME_COLGROUP_LIST_INDEX

/* ============================================================================ */

/** nsTableFrame maps the inner portion of a table (everything except captions.)
  * Used as a pseudo-frame within nsTableOuterFrame, it may also be used
  * stand-alone as the top-level frame.
  *
  * The flowed child list contains row group framess. There is also an additional
  * named child list:
  * - "ColGroup-list" which contains the col group frames
  *
  * @see nsLayoutAtoms::colGroupList
  *
  * TODO: make methods virtual so nsTableFrame can be used as a base class in the future.
  */
class nsTableFrame : public nsHTMLContainerFrame
{
public:

  /** nsTableOuterFrame has intimate knowledge of the inner table frame */
  friend class nsTableOuterFrame;

  /** instantiate a new instance of nsTableFrame.
    * @param aResult    the new object is returned in this out-param
    * @param aContent   the table object to map
    * @param aParent    the parent of the new frame
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  friend nsresult 
  NS_NewTableFrame(nsIFrame*& aResult);

  NS_IMETHOD Init(nsIPresContext&  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext);


  // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD DeleteFrame(nsIPresContext& aPresContext);

  /** helper method for determining if this is a nested table or not */
  PRBool IsNested(const nsHTMLReflowState& aReflowState, const nsStylePosition *& aPosition) const;

  /** helper method to find the table parent of any table frame object */
  static NS_METHOD GetTableFrame(nsIFrame *aSourceFrame, nsTableFrame *& aTableFrame);

  /** helper method for getting the width of the table's containing block */
  static nscoord GetTableContainerWidth(const nsHTMLReflowState& aReflowState);


  static PRBool TableIsAutoWidth(nsTableFrame *           aTableFrame,
                                 nsIStyleContext *        aTableStyle,
                                 const nsHTMLReflowState& aReflowState,
                                 nscoord&                 aSpecifiedTableWidth);
  
  /** return PR_TRUE if aDisplayType represents a rowgroup of any sort
    * (header, footer, or body)
    */
  PRBool IsRowGroup(PRInt32 aDisplayType) const;

  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD FirstChild(nsIAtom* aListName, nsIFrame*& aFirstChild) const;

  NS_IMETHOD  GetAdditionalChildListName(PRInt32   aIndex,
                                         nsIAtom*& aListName) const;

  /** complete the append of aRowGroupFrame to the table
    * this builds the cell map
    */
  NS_IMETHOD DidAppendRowGroup(nsTableRowGroupFrame *aRowGroupFrame);

  /** @see nsIFrame::Paint */
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

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
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  /** @see nsContainerFrame::CreateContinuingFrame */
  NS_IMETHOD CreateContinuingFrame(nsIPresContext&  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);

  /** allow the cell and row attributes to effect the column frame
    * currently, the only reason this exists is to support the HTML "rule"
    * that a width attribute on a cell in the first column sets the column width.
    */
  virtual NS_METHOD SetColumnStyleFromCell(nsIPresContext  & aPresContext,
                                           nsTableCellFrame* aCellFrame,
                                           nsTableRowFrame * aRowFrame);

  /** the COLS attribute can be modified by any cell's width attribute.
    * deal with it here.  Must be called before any call to 
    * ColumnInfoCache::AddColumnInfo
    */
  virtual void AdjustColumnsForCOLSAttribute();

  /** return the column frame corresponding to the given column index
    * there are two ways to do this, depending on whether we have cached
    * column information yet.
    */
  NS_METHOD GetColumnFrame(PRInt32 aColIndex, nsTableColFrame *&aColFrame);

  /** return PR_TRUE if the column width information has been set */
  PRBool IsColumnWidthsSet();

         
  /**
    * DEBUG METHOD
    *
    */
  //virtual void ListColumnLayoutData(FILE* out = stdout, PRInt32 aIndent = 0) const;
  NS_IMETHOD GetFrameName(nsString& aResult) const;

  
  /** return the width of the column at aColIndex    */
  virtual PRInt32 GetColumnWidth(PRInt32 aColIndex);

  /** set the width of the column at aColIndex to aWidth    */
  virtual void SetColumnWidth(PRInt32 aColIndex, nscoord aWidth);

  /** helper to get the border collapse style value */
  virtual PRUint8 GetBorderCollapseStyle();

  /** helper to get the cell spacing X style value */
  virtual nscoord GetCellSpacingX();

  /** helper to get the cell spacing Y style value */
  virtual nscoord GetCellSpacingY();

  /** helper to get the cell padding style value */
  virtual nscoord GetCellPadding();
          
  /** Calculate Layout Information */
  void    AppendLayoutData(nsVoidArray* aList, nsTableCellFrame* aTableCell);

  /** compute the left and right collapsed borders between aStartRowIndex and aEndRowIndex, inclusive */
  void    ComputeVerticalCollapsingBorders(nsIPresContext& aPresContext,
                                           PRInt32 aStartRowIndex, 
                                           PRInt32 aEndRowIndex);

  /** compute the top and bottom collapsed borders between aStartRowIndex and aEndRowIndex, inclusive */
  void    ComputeHorizontalCollapsingBorders(nsIPresContext& aPresContext,
                                             PRInt32 aStartRowIndex, 
                                             PRInt32 aEndRowIndex);

  void    ComputeLeftBorderForEdgeAt(nsIPresContext& aPresContext,
                                     PRInt32 aRowIndex, 
                                     PRInt32 aColIndex);
  void    ComputeRightBorderForEdgeAt(nsIPresContext& aPresContext,
                                      PRInt32 aRowIndex, 
                                      PRInt32 aColIndex);
  void    ComputeTopBorderForEdgeAt(nsIPresContext& aPresContext,
                                    PRInt32 aRowIndex, 
                                    PRInt32 aColIndex);
  void    ComputeBottomBorderForEdgeAt(nsIPresContext& aPresContext,
                                       PRInt32 aRowIndex, 
                                       PRInt32 aColIndex);
  
  /** at the time we initially compute collapsing borders, we don't yet have the 
    * column widths.  So we set them as a post-process of the column balancing algorithm.
    */
  void    SetCollapsingBorderHorizontalEdgeLengths();

  PRUint8 GetOpposingEdge(PRUint8 aEdge);

  nscoord GetWidthForSide(const nsMargin &aBorder, PRUint8 aSide);

  PRUint8 CompareBorderStyles(PRUint8 aStyle1, PRUint8 aStyle2);

  void    SetBorderEdgeLength(PRUint8 aSide, PRInt32 aIndex, nscoord aLength);

  void    ComputeCollapsedBorderSegment(PRUint8       aSide, 
                                        nsVoidArray * aStyles, 
                                        nsBorderEdge& aBorder,
                                        PRBool        aFlipLastSide);


  void    RecalcLayoutData(nsIPresContext& aPresContext);

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
    *
    * @param aRowIndex  the row from which to measure effective row span
    * @param aCell      the cell
    *
    * @return  the row span, correcting for row spans that extend beyond the bottom
    *          of the table.
    */
  virtual PRInt32  GetEffectiveRowSpan(PRInt32 aRowIndex, nsTableCellFrame *aCell);

  /** return the col span of a cell, taking into account col span magic at the edge
    * of a table.
    *
    * @param aColIndex  the column from which to measure effective col span
    * @param aCell      the cell
    *
    * @return  the col span, correcting for col spans that extend beyond the edge
    *          of the table.
    */
  virtual PRInt32  GetEffectiveColSpan(PRInt32 aColIndex, nsTableCellFrame *aCell);

  /** return the value of the COLS attribute, adjusted for the 
    * actual number of columns in the table
    */
  PRInt32 GetEffectiveCOLSAttribute();

  /** return the index of the next row that is not yet assigned.
    * If no row is initialized, 0 is returned.
    */
  PRInt32 GetNextAvailRowIndex() const;

  /** return the index of the next column in aRowIndex after aColIndex 
    * that does not have a cell assigned to it.
    * If aColIndex is past the end of the row, it is returned.
    * If the row is not initialized, 0 is returned.
    */
  PRInt32 GetNextAvailColIndex(PRInt32 aRowIndex, PRInt32 aColIndex) const;

  /** build as much of the CellMap as possible from the info we have so far 
    */
  virtual void AddCellToTable (nsTableRowFrame  *aRowFrame, 
                               nsTableCellFrame *aCellFrame,
                               PRBool            aAddRow);

  virtual void AddColumnFrame (nsTableColFrame *aColFrame);

  NS_IMETHOD  List(FILE* out = stdout, PRInt32 aIndent = 0, nsIListFilter *aFilter = nsnull) const;

protected:

  /** protected constructor.
    * @see NewFrame
    */
  nsTableFrame();

  /** destructor, responsible for mColumnLayoutData and mColumnWidths */
  virtual ~nsTableFrame();

  /** implement abstract method on nsHTMLContainerFrame */
  virtual PRIntn GetSkipSides() const;

  /** first pass of ResizeReflow.  
    * lays out all table content with aMaxSize(NS_UNCONSTRAINEDSIZE,NS_UNCONSTRAINEDSIZE) and
    * a non-null aMaxElementSize so we get all the metrics we need to do column balancing.
    * Pass 1 only needs to be executed once no matter how many times the table is resized, 
    * as long as content and style don't change.  This is managed in the member variable mFirstPassIsValid.
    * The layout information for each cell is cached in mColumLayoutData.
    * Incremental layout can take advantage of aStartingFrame to pick up where a previous
    * ResizeReflowPass1 left off.
    *
    * @see Reflow
    */
  NS_IMETHOD ResizeReflowPass1(nsIPresContext&          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus,
                               nsTableRowGroupFrame *   aStartingFrame,
                               nsReflowReason           aReason,
                               PRBool                   aDoSiblings);

  /** second pass of ResizeReflow.
    * lays out all table content with aMaxSize(computed_table_width, given_table_height) 
    * Pass 2 is executed every time the table needs to resize.  An optimization is included
    * so that if the table doesn't need to actually be resized, no work is done (see NeedsReflow).
    * 
    *
    * @see Reflow
    * @see NeedsReflow
    */
  NS_IMETHOD ResizeReflowPass2(nsIPresContext&          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus);

  /** Incremental Reflow attempts to do column balancing with the minimum number of reflow
    * commands to child elements.  This is done by processing the reflow command,
    * rebalancing column widths (if necessary), then comparing the resulting column widths
    * to the prior column widths and reflowing only those cells that require a reflow.
    *
    * @see Reflow
    */
  NS_IMETHOD IncrementalReflow(nsIPresContext&          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus);

  NS_IMETHOD IR_TargetIsChild(nsIPresContext&        aPresContext,
                              nsHTMLReflowMetrics&   aDesiredSize,
                              InnerTableReflowState& aReflowState,
                              nsReflowStatus&        aStatus,
                              nsIFrame *             aNextFrame);

  NS_IMETHOD IR_TargetIsMe(nsIPresContext&        aPresContext,
                           nsHTMLReflowMetrics&   aDesiredSize,
                           InnerTableReflowState& aReflowState,
                           nsReflowStatus&        aStatus);

  NS_IMETHOD IR_ColGroupInserted(nsIPresContext&        aPresContext,
                                 nsHTMLReflowMetrics&   aDesiredSize,
                                 InnerTableReflowState& aReflowState,
                                 nsReflowStatus&        aStatus,
                                 nsTableColGroupFrame * aInsertedFrame,
                                 PRBool                 aReplace);

  NS_IMETHOD IR_ColGroupAppended(nsIPresContext&        aPresContext,
                                 nsHTMLReflowMetrics&   aDesiredSize,
                                 InnerTableReflowState& aReflowState,
                                 nsReflowStatus&        aStatus,
                                 nsTableColGroupFrame * aAppendedFrame); 

  NS_IMETHOD IR_ColGroupRemoved(nsIPresContext&        aPresContext,
                                nsHTMLReflowMetrics&   aDesiredSize,
                                InnerTableReflowState& aReflowState,
                                nsReflowStatus&        aStatus,
                                nsTableColGroupFrame * aDeletedFrame);

  NS_IMETHOD IR_RowGroupInserted(nsIPresContext&        aPresContext,
                                 nsHTMLReflowMetrics&   aDesiredSize,
                                 InnerTableReflowState& aReflowState,
                                 nsReflowStatus&        aStatus,
                                 nsTableRowGroupFrame * aInsertedFrame,
                                 PRBool                 aReplace);

  NS_IMETHOD IR_RowGroupAppended(nsIPresContext&        aPresContext,
                                 nsHTMLReflowMetrics&   aDesiredSize,
                                 InnerTableReflowState& aReflowState,
                                 nsReflowStatus&        aStatus,
                                 nsTableRowGroupFrame * aAppendedFrame);

  NS_IMETHOD IR_RowGroupRemoved(nsIPresContext&        aPresContext,
                                nsHTMLReflowMetrics&   aDesiredSize,
                                InnerTableReflowState& aReflowState,
                                nsReflowStatus&        aStatus,
                                nsTableRowGroupFrame * aDeletedFrame);

  NS_IMETHOD IR_StyleChanged(nsIPresContext&        aPresContext,
                             nsHTMLReflowMetrics&   aDesiredSize,
                             InnerTableReflowState& aReflowState,
                             nsReflowStatus&        aStatus);
  
  NS_IMETHOD AdjustSiblingsAfterReflow(nsIPresContext&        aPresContext,
                                       InnerTableReflowState& aReflowState,
                                       nsIFrame*              aKidFrame,
                                       nscoord                aDeltaY);

  /** return the desired width of this table accounting for the current
    * reflow state, and for the table attributes and parent
    */
  nscoord ComputeDesiredWidth(const nsHTMLReflowState& aReflowState) const;

  nscoord ComputeDesiredHeight(nsIPresContext&          aPresContext,
                               const nsHTMLReflowState& aReflowState,
                               nscoord                  aDefaultHeight);

  nscoord GetEffectiveContainerHeight(const nsHTMLReflowState& aReflowState);

  nscoord GetTopMarginFor(nsIPresContext& aCX,
                          InnerTableReflowState& aReflowState,
                          const nsMargin& aKidMargin);

  void PlaceChild(nsIPresContext&        aPresContext,
                  InnerTableReflowState& aReflowState,
                  nsIFrame*              aKidFrame,
                  const nsRect&          aKidRect,
                  nsSize*                aMaxElementSize,
                  nsSize&                aKidMaxElementSize);

  /**
   * Reflow the frames we've already created
   *
   * @param   aPresContext presentation context to use
   * @param   aReflowState current inline state
   * @return  true if we successfully reflowed all the mapped children and false
   *            otherwise, e.g. we pushed children to the next in flow
   */
  NS_IMETHOD ReflowMappedChildren(nsIPresContext&        aPresContext,
                                  nsHTMLReflowMetrics&   aDesiredSize,
                                  InnerTableReflowState& aReflowState,
                                  nsReflowStatus&        aStatus);
  /**
   * Try and pull-up frames from our next-in-flow
   *
   * @param   aPresContext presentation context to use
   * @param   aReflowState current reflow state
   * @return  true if we successfully pulled-up all the children and false
   *            otherwise, e.g. child didn't fit
   */
  NS_IMETHOD PullUpChildren(nsIPresContext&        aPresContext,
                            nsHTMLReflowMetrics&   aDesiredSize,
                            InnerTableReflowState& aReflowState,
                            nsReflowStatus&        aStatus);

  /** assign widths for each column, taking into account the table content, the effective style, 
    * the layout constraints, and the compatibility mode.  Sets mColumnWidths as a side effect.
    * @param aPresContext     the presentation context
    * @param aTableStyle      the resolved style for the table
    * @param aMaxSize         the height and width constraints
    * @param aMaxElementSize  the min size of the largest indivisible object
    */
  virtual void BalanceColumnWidths(nsIPresContext&          aPresContext, 
                                   const nsHTMLReflowState& aReflowState,
                                   const nsSize&            aMaxSize, 
                                   nsSize*                  aMaxElementSize);

  /** sets the width of the table according to the computed widths of each column. */
  virtual void SetTableWidth(nsIPresContext&  aPresContext);

  /** given the new parent size, do I really need to do a reflow? */
  virtual PRBool NeedsReflow(const nsHTMLReflowState& aReflowState, 
                             const nsSize&            aMaxSize);

  /** returns PR_TRUE if the cached pass 1 data is still valid */
  virtual PRBool IsFirstPassValid() const;

  /** returns PR_TRUE if the cached column info is still valid */
  virtual PRBool IsColumnCacheValid() const;

  /** returns PR_TRUE if the cached column info is still valid */
  virtual PRBool IsColumnWidthsValid() const;

public:
  virtual void InvalidateFirstPassCache();

  virtual void InvalidateColumnCache();

  virtual void InvalidateColumnWidths();

protected:

  /** Support methods for DidSetStyleContext */
  void      MapBorderMarginPadding(nsIPresContext& aPresContext);
  void      MapHTMLBorderStyle(nsStyleSpacing& aSpacingStyle, nscoord aBorderWidth);
  PRBool    ConvertToPixelValue(nsHTMLValue& aValue, PRInt32 aDefault, PRInt32& aResult);

  /** called whenever the number of columns changes, to increase the storage in mCellMap 
    */
  virtual void GrowCellMap(PRInt32 aColCount);

  /** returns PR_TRUE if the cached pass 1 data is still valid */
  virtual PRBool IsCellMapValid() const;

public:
  /** ResetCellMap is called when the cell structure of the table is changed.
    * Call with caution, only when changing the structure of the table such as 
    * inserting or removing rows, changing the rowspan or colspan attribute of a cell, etc.
    */
  virtual void InvalidateCellMap();
    
protected:
  /** iterates all child frames and creates a new cell map */
  NS_IMETHOD ReBuildCellMap();

  /** Get the cell map for this table frame.  It is not always mCellMap.
    * Only the firstInFlow has a legit cell map
    */
  virtual nsCellMap *GetCellMap() const;

#ifdef NS_DEBUG
  /** for debugging only
    * prints out information about the cell map
    */
  void DumpCellMap();

  /** for debugging only
    * prints out info about the table layout state, printing columns and their cells
    */
  void ListColumnLayoutData(FILE* out, PRInt32 aIndent);
#endif

  /** sum the columns represented by all nsTableColGroup objects. 
    * if the cell map says there are more columns than this, 
    * add extra implicit columns to the content tree.
    */ 
  virtual void EnsureColumns (nsIPresContext& aPresContext);

  /** Set the min col span for every column in the table.  Scans the whole table. */
  virtual void SetMinColSpanForTable();

  virtual void BuildColumnCache(nsIPresContext&          aPresContext,
                                nsHTMLReflowMetrics&     aDesiredSize,
                                const nsHTMLReflowState& aReflowState,
                                nsReflowStatus&          aStatus);

  /** called every time we discover we have a new cell to add to the table.
    * This could be because we got actual cell content, because of rowspan/colspan attributes, etc.
    * This method changes mCellMap as necessary to account for the new cell.
    *
    * @param aCell     the content object created for the cell
    * @param aRowIndex the row into which the cell is to be inserted
    * @param aColIndex the col into which the cell is to be inserted
    */
  virtual void BuildCellIntoMap (nsTableCellFrame *aCell, PRInt32 aRowIndex, PRInt32 aColIndex);

  /** return the number of columns as specified by the input. 
    * has 2 side effects:<br>
    * calls SetStartColumnIndex on each nsTableColumn<br>
    * sets mSpecifiedColCount.<br>
    */
  virtual PRInt32 GetSpecifiedColumnCount ();

public: /* ----- Cell Map public methods ----- */

  /** returns the number of rows in this table.
    * if mCellMap has been created, it is asked for the number of rows.<br>
    * otherwise, the content is enumerated and the rows are counted.
    */
  virtual PRInt32 GetRowCount() const;

  /** returns the number of columns in this table. */
  virtual PRInt32 GetColCount();

  /** adjust the col count for screwy table attributes.
    * currently just handles excess colspan at end of table
    */
  virtual void SetEffectiveColCount();

  /** return the column frame at colIndex.
    * returns nsnull if the col frame has not yet been allocated, or if aColIndex is out of range
    */
  nsTableColFrame * GetColFrame(PRInt32 aColIndex);

  /** return the cell frame at aRowIndex, aColIndex.
    * returns nsnull if the cell frame has not yet been allocated, 
    * or if aRowIndex or aColIndex is out of range
    */
  nsTableCellFrame * GetCellFrameAt(PRInt32 aRowIndex, PRInt32 aColIndex);

  /** returns PR_TRUE if the row at aRowIndex has any cells that are the result
    * of a row-spanning cell above it.
	* @see nsCellMap::RowIsSpannedInto
    */
  PRBool RowIsSpannedInto(PRInt32 aRowIndex);

  /** returns PR_TRUE if the row at aRowIndex has any cells that have a rowspan>1
    * that originate in aRowIndex.
    * @see nsCellMap::RowHasSpanningCells
    */
  PRBool RowHasSpanningCells(PRInt32 aRowIndex);

	/** returns PR_TRUE if the col at aColIndex has any cells that are the result
	  * of a col-spanning cell.
	  * @see nsCellMap::ColIsSpannedInto
	  */
	PRBool ColIsSpannedInto(PRInt32 aColIndex);

	/** returns PR_TRUE if the row at aColIndex has any cells that have a colspan>1
	  * @see nsCellMap::ColHasSpanningCells
	  */
	PRBool ColHasSpanningCells(PRInt32 aColIndex);

  /** return the minimum width of the table caption.  Return 0 if there is no caption. */
  nscoord GetMinCaptionWidth();

  /** return the minimum width of the table.  Return 0 if the min width is unknown. */
  nscoord GetMinTableWidth();

  /** return the maximum width of the table caption.  Return 0 if the max width is unknown. */
  nscoord GetMaxTableWidth();

  /** compute the max-element-size for the table
    * @param aMaxElementSize  [OUT] width field set to the min legal width of the table
    */
  void SetMaxElementSize(nsSize* aMaxElementSize);

  /** returns PR_TRUE if table layout requires a preliminary pass over the content */
  PRBool RequiresPass1Layout();

public:
  static nsIAtom* gColGroupAtom;

private:
  void DebugPrintCount() const; // Debugging routine

// data members
  PRInt32     *mColumnWidths;       // widths of each column
  PRInt32      mColumnWidthsLength; // the number of column lengths this frame has allocated
  PRBool       mColumnWidthsSet;    // PR_TRUE if column widths have been set at least once
  PRBool       mColumnWidthsValid;  // PR_TRUE if column width data is still legit, PR_FALSE if it needs to be recalculated
  PRBool       mFirstPassValid;     // PR_TRUE if first pass data is still legit, PR_FALSE if it needs to be recalculated
  PRBool       mColumnCacheValid;   // PR_TRUE if column cache info is still legit, PR_FALSE if it needs to be recalculated
  PRBool       mCellMapValid;       // PR_TRUE if cell map data is still legit, PR_FALSE if it needs to be recalculated
  PRBool       mIsInvariantWidth;   // PR_TRUE if table width cannot change
  PRInt32      mColCount;           // the number of columns in this table
  PRInt32      mEffectiveColCount;  // the number of columns in this table adjusted for weird table attributes
  nsCellMap*   mCellMap;            // maintains the relationships between rows, cols, and cells
  ColumnInfoCache *mColCache;       // cached information about the table columns
  nsITableLayoutStrategy * mTableLayoutStrategy; // the layout strategy for this frame
  nsIFrame*    mColGroups;          // the list of colgroup frames
  nscoord      mDefaultCellSpacingX;// the default cell spacing X for this table
  nscoord      mDefaultCellSpacingY;// the default cell spacing X for this table
  nscoord      mDefaultCellPadding; // the default cell padding for this table

  nsBorderEdges mBorderEdges;       // one list of border segments for each side of the table frame
                                    // used only for the collapsing border model
};


inline PRBool nsTableFrame::IsRowGroup(PRInt32 aDisplayType) const
{
  return PRBool((NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == aDisplayType) ||
                (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == aDisplayType) ||
                (NS_STYLE_DISPLAY_TABLE_ROW_GROUP    == aDisplayType));
}

#endif































