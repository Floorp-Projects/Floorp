/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTableFrame_h__
#define nsTableFrame_h__

#include "mozilla/Attributes.h"
#include "celldata.h"
#include "nscore.h"
#include "nsContainerFrame.h"
#include "nsStyleCoord.h"
#include "nsStyleConsts.h"
#include "nsTableColFrame.h"
#include "nsTableColGroupFrame.h"
#include "nsCellMap.h"
#include "nsGkAtoms.h"
#include "nsDisplayList.h"

class nsTableCellFrame;
class nsTableCellMap;
class nsTableColFrame;
class nsColGroupFrame;
class nsTableRowGroupFrame;
class nsTableRowFrame;
class nsTableColGroupFrame;
class nsITableLayoutStrategy;
class nsStyleContext;

struct nsTableReflowState;
struct nsStylePosition;
struct BCPropertyData;

static inline bool IS_TABLE_CELL(nsIAtom* frameType) {
  return nsGkAtoms::tableCellFrame == frameType ||
    nsGkAtoms::bcTableCellFrame == frameType;
}

static inline bool FrameHasBorderOrBackground(nsIFrame* f) {
  return (f->StyleVisibility()->IsVisible() &&
          (!f->StyleBackground()->IsTransparent() ||
           f->StyleDisplay()->mAppearance ||
           f->StyleBorder()->HasBorder()));
}

class nsDisplayTableItem : public nsDisplayItem
{
public:
  nsDisplayTableItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame) : 
      nsDisplayItem(aBuilder, aFrame),
      mPartHasFixedBackground(false) {}

  virtual bool IsVaryingRelativeToMovingFrame(nsDisplayListBuilder* aBuilder,
                                                nsIFrame* aFrame) MOZ_OVERRIDE;
  // With collapsed borders, parts of the collapsed border can extend outside
  // the table part frames, so allow this display element to blow out to our
  // overflow rect. This is also useful for row frames that have spanning
  // cells extending outside them.
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) MOZ_OVERRIDE;

  void UpdateForFrameBackground(nsIFrame* aFrame);

private:
  bool mPartHasFixedBackground;
};

class nsAutoPushCurrentTableItem
{
public:
  nsAutoPushCurrentTableItem() : mBuilder(nullptr) {}
  
  void Push(nsDisplayListBuilder* aBuilder, nsDisplayTableItem* aPushItem)
  {
    mBuilder = aBuilder;
    mOldCurrentItem = aBuilder->GetCurrentTableItem();
    aBuilder->SetCurrentTableItem(aPushItem);
#ifdef DEBUG
    mPushedItem = aPushItem;
#endif
  }
  ~nsAutoPushCurrentTableItem() {
    if (!mBuilder)
      return;
#ifdef DEBUG
    NS_ASSERTION(mBuilder->GetCurrentTableItem() == mPushedItem,
                 "Someone messed with the current table item behind our back!");
#endif
    mBuilder->SetCurrentTableItem(mOldCurrentItem);
  }

private:
  nsDisplayListBuilder* mBuilder;
  nsDisplayTableItem*   mOldCurrentItem;
#ifdef DEBUG
  nsDisplayTableItem*   mPushedItem;
#endif
};

/* ============================================================================ */

/**
  * nsTableFrame maps the inner portion of a table (everything except captions.)
  * Used as a pseudo-frame within nsTableOuterFrame, it may also be used
  * stand-alone as the top-level frame.
  *
  * The principal child list contains row group frames. There is also an
  * additional child list, kColGroupList, which contains the col group frames.
  */
class nsTableFrame : public nsContainerFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  static void DestroyPositionedTablePartArray(void* aPropertyValue);
  NS_DECLARE_FRAME_PROPERTY(PositionedTablePartArray, DestroyPositionedTablePartArray)

  /** nsTableOuterFrame has intimate knowledge of the inner table frame */
  friend class nsTableOuterFrame;

  /** instantiate a new instance of nsTableRowFrame.
    * @param aPresShell the pres shell for this frame
    *
    * @return           the frame that was created
    */
  friend nsIFrame* NS_NewTableFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  /** sets defaults for table-specific style.
    * @see nsIFrame::Init 
    */
  virtual void Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        aPrevInFlow) MOZ_OVERRIDE;

  static float GetTwipsToPixels(nsPresContext* aPresContext);

  // Return true if aParentReflowState.frame or any of its ancestors within
  // the containing table have non-auto height. (e.g. pct or fixed height)
  static bool AncestorsHaveStyleHeight(const nsHTMLReflowState& aParentReflowState);

  // See if a special height reflow will occur due to having a pct height when
  // the pct height basis may not yet be valid.
  static void CheckRequestSpecialHeightReflow(const nsHTMLReflowState& aReflowState);

  // Notify the frame and its ancestors (up to the containing table) that a special
  // height reflow will occur. 
  static void RequestSpecialHeightReflow(const nsHTMLReflowState& aReflowState);

  static void RePositionViews(nsIFrame* aFrame);

  static bool PageBreakAfter(nsIFrame* aSourceFrame,
                               nsIFrame* aNextFrame);
  
  // Register a positioned table part with its nsTableFrame. These objects will
  // be visited by FixupPositionedTableParts after reflow is complete. (See that
  // function for more explanation.) Should be called during frame construction.
  static void RegisterPositionedTablePart(nsIFrame* aFrame);

  // Unregister a positioned table part with its nsTableFrame.
  static void UnregisterPositionedTablePart(nsIFrame* aFrame,
                                            nsIFrame* aDestructRoot);

  nsPoint GetFirstSectionOrigin(const nsHTMLReflowState& aReflowState) const;
  /*
   * Notification that aAttribute has changed for content inside a table (cell, row, etc)
   */
  void AttributeChangedFor(nsIFrame*       aFrame,
                           nsIContent*     aContent, 
                           nsIAtom*        aAttribute); 

  /** @see nsIFrame::DestroyFrom */
  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;
  
  /** @see nsIFrame::DidSetStyleContext */
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) MOZ_OVERRIDE;

  virtual nsresult AppendFrames(ChildListID     aListID,
                                nsFrameList&    aFrameList) MOZ_OVERRIDE;
  virtual nsresult InsertFrames(ChildListID     aListID,
                                nsIFrame*       aPrevFrame,
                                nsFrameList&    aFrameList) MOZ_OVERRIDE;
  virtual nsresult RemoveFrame(ChildListID     aListID,
                               nsIFrame*       aOldFrame) MOZ_OVERRIDE;

  virtual nsMargin GetUsedBorder() const MOZ_OVERRIDE;
  virtual nsMargin GetUsedPadding() const MOZ_OVERRIDE;
  virtual nsMargin GetUsedMargin() const MOZ_OVERRIDE;

  // Get the offset from the border box to the area where the row groups fit
  nsMargin GetChildAreaOffset(const nsHTMLReflowState* aReflowState) const;

  /** helper method to find the table parent of any table frame object */
  static nsTableFrame* GetTableFrame(nsIFrame* aSourceFrame);
                                 
  /* Like GetTableFrame, but will return nullptr if we don't pass through
   * aMustPassThrough on the way to the table.
   */
  static nsTableFrame* GetTableFramePassingThrough(nsIFrame* aMustPassThrough,
                                                   nsIFrame* aSourceFrame);

  typedef void (* DisplayGenericTablePartTraversal)
      (nsDisplayListBuilder* aBuilder, nsFrame* aFrame,
       const nsRect& aDirtyRect, const nsDisplayListSet& aLists);
  static void GenericTraversal(nsDisplayListBuilder* aBuilder, nsFrame* aFrame,
                               const nsRect& aDirtyRect, const nsDisplayListSet& aLists);

  /**
   * Helper method to handle display common to table frames, rowgroup frames
   * and row frames. It creates a background display item for handling events
   * if necessary, an outline display item if necessary, and displays
   * all the the frame's children.
   * @param aDisplayItem the display item created for this part, or null
   * if this part's border/background painting is delegated to an ancestor
   * @param aTraversal a function that gets called to traverse the table
   * part's child frames and add their display list items to a
   * display list set.
   */
  static void DisplayGenericTablePart(nsDisplayListBuilder* aBuilder,
                                      nsFrame* aFrame,
                                      const nsRect& aDirtyRect,
                                      const nsDisplayListSet& aLists,
                                      nsDisplayTableItem* aDisplayItem,
                                      DisplayGenericTablePartTraversal aTraversal = GenericTraversal);

  // Return the closest sibling of aPriorChildFrame (including aPriroChildFrame)
  // of type aChildType.
  static nsIFrame* GetFrameAtOrBefore(nsIFrame*       aParentFrame,
                                      nsIFrame*       aPriorChildFrame,
                                      nsIAtom*        aChildType);
  bool IsAutoHeight();
  
  /** @return true if aDisplayType represents a rowgroup of any sort
    * (header, footer, or body)
    */
  bool IsRowGroup(int32_t aDisplayType) const;

  /** Initialize the table frame with a set of children.
    * @see nsIFrame::SetInitialChildList 
    */
  virtual nsresult SetInitialChildList(ChildListID     aListID,
                                       nsFrameList&    aChildList) MOZ_OVERRIDE;

  virtual const nsFrameList& GetChildList(ChildListID aListID) const MOZ_OVERRIDE;
  virtual void GetChildLists(nsTArray<ChildList>* aLists) const MOZ_OVERRIDE;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  /**
   * Paint the background of the table and its parts (column groups,
   * columns, row groups, rows, and cells), and the table border, and all
   * internal borders if border-collapse is on.
   */
  void PaintTableBorderBackground(nsRenderingContext& aRenderingContext,
                                  const nsRect& aDirtyRect,
                                  nsPoint aPt, uint32_t aBGPaintFlags);

  /**
   * Determines if any table part has a background image that is currently not
   * decoded. Does not look into cell contents (ie only table parts).
   */
  static bool AnyTablePartHasUndecodedBackgroundImage(nsIFrame* aStart,
                                                      nsIFrame* aEnd);

  /** Get the outer half (i.e., the part outside the height and width of
   *  the table) of the largest segment (?) of border-collapsed border on
   *  the table on each side, or 0 for non border-collapsed tables.
   */
  nsMargin GetOuterBCBorder() const;

  /** Same as above, but only if it's included from the border-box width
   *  of the table.
   */
  nsMargin GetIncludedOuterBCBorder() const;

  /** Same as above, but only if it's excluded from the border-box width
   *  of the table.  This is the area that leaks out into the margin
   *  (or potentially past it, if there is no margin).
   */
  nsMargin GetExcludedOuterBCBorder() const;

  /**
   * In quirks mode, the size of the table background is reduced
   * by the outer BC border. Compute the reduction needed.
   */
  nsMargin GetDeflationForBackground(nsPresContext* aPresContext) const;

  /** Get width of table + colgroup + col collapse: elements that
   *  continue along the length of the whole left side.
   *  see nsTablePainter about continuous borders
   */
  nscoord GetContinuousLeftBCBorderWidth() const;
  void SetContinuousLeftBCBorderWidth(nscoord aValue);

  friend class nsDelayedCalcBCBorders;
  
  void AddBCDamageArea(const nsIntRect& aValue);
  bool BCRecalcNeeded(nsStyleContext* aOldStyleContext,
                        nsStyleContext* aNewStyleContext);
  void PaintBCBorders(nsRenderingContext& aRenderingContext,
                      const nsRect&        aDirtyRect);

  virtual void MarkIntrinsicWidthsDirty() MOZ_OVERRIDE;
  // For border-collapse tables, the caller must not add padding and
  // border to the results of these functions.
  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual IntrinsicWidthOffsetData
    IntrinsicWidthOffsets(nsRenderingContext* aRenderingContext) MOZ_OVERRIDE;

  virtual nsSize ComputeSize(nsRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             uint32_t aFlags) MOZ_OVERRIDE;
  virtual nsSize ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, bool aShrinkWrap) MOZ_OVERRIDE;
  /**
   * A copy of nsFrame::ShrinkWidthToFit that calls a different
   * GetPrefWidth, since tables have two different ones.
   */
  nscoord TableShrinkWidthToFit(nsRenderingContext *aRenderingContext,
                                nscoord aWidthInCB);

  // XXXldb REWRITE THIS COMMENT!
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
    * @see nsIFrame::Reflow
    */
  virtual nsresult Reflow(nsPresContext*          aPresContext,
                          nsHTMLReflowMetrics&     aDesiredSize,
                          const nsHTMLReflowState& aReflowState,
                          nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  nsresult ReflowTable(nsHTMLReflowMetrics&     aDesiredSize,
                       const nsHTMLReflowState& aReflowState,
                       nscoord                  aAvailHeight,
                       nsIFrame*&               aLastChildReflowed,
                       nsReflowStatus&          aStatus);

  nsFrameList& GetColGroups();

  virtual nsIFrame* GetParentStyleContextFrame() const MOZ_OVERRIDE;

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::tableFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    if (aFlags & eSupportsCSSTransforms) {
      return false;
    }
    return nsContainerFrame::IsFrameOfType(aFlags);
  }

#ifdef DEBUG_FRAME_DUMP
  /** @see nsIFrame::GetFrameName */
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

  /** return the width of the column at aColIndex    */
  int32_t GetColumnWidth(int32_t aColIndex);

  /** helper to get the cell spacing X style value */
  nscoord GetCellSpacingX();

  /** helper to get the cell spacing Y style value */
  nscoord GetCellSpacingY();
 
  virtual nscoord GetBaseline() const MOZ_OVERRIDE;
  /** return the row span of a cell, taking into account row span magic at the bottom
    * of a table. The row span equals the number of rows spanned by aCell starting at
    * aStartRowIndex, and can be smaller if aStartRowIndex is greater than the row
    * index in which aCell originates.
    *
    * @param aStartRowIndex the cell
    * @param aCell          the cell
    *
    * @return  the row span, correcting for row spans that extend beyond the bottom
    *          of the table.
    */
  int32_t  GetEffectiveRowSpan(int32_t                 aStartRowIndex,
                               const nsTableCellFrame& aCell) const;
  int32_t  GetEffectiveRowSpan(const nsTableCellFrame& aCell,
                               nsCellMap*              aCellMap = nullptr);

  /** return the col span of a cell, taking into account col span magic at the edge
    * of a table.
    *
    * @param aCell      the cell
    *
    * @return  the col span, correcting for col spans that extend beyond the edge
    *          of the table.
    */
  int32_t  GetEffectiveColSpan(const nsTableCellFrame& aCell,
                               nsCellMap*              aCellMap = nullptr) const;

  /** indicate whether the row has more than one cell that either originates
    * or is spanned from the rows above
    */
  bool HasMoreThanOneCell(int32_t aRowIndex) const;

  /** return the column frame associated with aColIndex
    * returns nullptr if the col frame has not yet been allocated, or if
    * aColIndex is out of range
    */
  nsTableColFrame* GetColFrame(int32_t aColIndex) const;

  /** Insert a col frame reference into the colframe cache and adapt the cellmap
    * @param aColFrame    - the column frame
    * @param aColIndex    - index where the column should be inserted into the
    *                       colframe cache
    */
  void InsertCol(nsTableColFrame& aColFrame,
                 int32_t          aColIndex);

  nsTableColGroupFrame* CreateAnonymousColGroupFrame(nsTableColGroupType aType);

  int32_t DestroyAnonymousColFrames(int32_t aNumFrames);

  // Append aNumColsToAdd anonymous col frames of type eColAnonymousCell to our
  // last eColGroupAnonymousCell colgroup.  If we have no such colgroup, then
  // create one.
  void AppendAnonymousColFrames(int32_t aNumColsToAdd);

  // Append aNumColsToAdd anonymous col frames of type aColType to
  // aColGroupFrame.  If aAddToTable is true, also call AddColsToTable on the
  // new cols.
  void AppendAnonymousColFrames(nsTableColGroupFrame* aColGroupFrame,
                                int32_t               aNumColsToAdd,
                                nsTableColType        aColType,
                                bool                  aAddToTable);

  void MatchCellMapToColCache(nsTableCellMap* aCellMap);
  /** empty the column frame cache */
  void ClearColCache();

  void DidResizeColumns();

  void AppendCell(nsTableCellFrame& aCellFrame,
                  int32_t           aRowIndex);

  void InsertCells(nsTArray<nsTableCellFrame*>& aCellFrames,
                   int32_t                      aRowIndex,
                   int32_t                      aColIndexBefore);

  void RemoveCell(nsTableCellFrame* aCellFrame,
                  int32_t           aRowIndex);

  void AppendRows(nsTableRowGroupFrame*       aRowGroupFrame,
                  int32_t                     aRowIndex,
                  nsTArray<nsTableRowFrame*>& aRowFrames);

  int32_t InsertRows(nsTableRowGroupFrame*       aRowGroupFrame,
                     nsTArray<nsTableRowFrame*>& aFrames,
                     int32_t                     aRowIndex,
                     bool                        aConsiderSpans);

  void RemoveRows(nsTableRowFrame& aFirstRowFrame,
                  int32_t          aNumRowsToRemove,
                  bool             aConsiderSpans);

  /** Insert multiple rowgroups into the table cellmap handling
    * @param aRowGroups - iterator that iterates over the rowgroups to insert
    */
  void InsertRowGroups(const nsFrameList::Slice& aRowGroups);

  void InsertColGroups(int32_t                   aStartColIndex,
                       const nsFrameList::Slice& aColgroups);

  void RemoveCol(nsTableColGroupFrame* aColGroupFrame,
                 int32_t               aColIndex,
                 bool                  aRemoveFromCache,
                 bool                  aRemoveFromCellMap);

  bool ColumnHasCellSpacingBefore(int32_t aColIndex) const;

  bool HasPctCol() const;
  void SetHasPctCol(bool aValue);

  bool HasCellSpanningPctCol() const;
  void SetHasCellSpanningPctCol(bool aValue);

  /**
   * To be called on a frame by its parent after setting its size/position and
   * calling DidReflow (possibly via FinishReflowChild()).  This can also be
   * used for child frames which are not being reflowed but did have their size
   * or position changed.
   *
   * @param aFrame The frame to invalidate
   * @param aOrigRect The original rect of aFrame (before the change).
   * @param aOrigVisualOverflow The original overflow rect of aFrame.
   * @param aIsFirstReflow True if the size/position change is due to the
   *                       first reflow of aFrame.
   */
  static void InvalidateTableFrame(nsIFrame* aFrame,
                                   const nsRect& aOrigRect,
                                   const nsRect& aOrigVisualOverflow,
                                   bool aIsFirstReflow);

  virtual bool UpdateOverflow() MOZ_OVERRIDE;

protected:

  /** protected constructor. 
    * @see NewFrame
    */
  nsTableFrame(nsStyleContext* aContext);

  /** destructor, responsible for mColumnLayoutData */
  virtual ~nsTableFrame();

  void InitChildReflowState(nsHTMLReflowState& aReflowState);

  virtual int GetSkipSides(const nsHTMLReflowState* aReflowState = nullptr) const MOZ_OVERRIDE;

public:
  bool IsRowInserted() const;
  void   SetRowInserted(bool aValue);

protected:
    
  // A helper function to reflow a header or footer with unconstrained height
  // to see if it should be made repeatable and also to determine its desired
  // height.
  nsresult SetupHeaderFooterChild(const nsTableReflowState& aReflowState,
                                  nsTableRowGroupFrame* aFrame,
                                  nscoord* aDesiredHeight);

  nsresult ReflowChildren(nsTableReflowState&  aReflowState,
                          nsReflowStatus&      aStatus,
                          nsIFrame*&           aLastChildReflowed,
                          nsOverflowAreas&     aOverflowAreas);

  // This calls the col group and column reflow methods, which do two things:
  //  (1) set all the dimensions to 0
  //  (2) notify the table about colgroups or columns with hidden visibility
  void ReflowColGroups(nsRenderingContext* aRenderingContext);

  /** return the width of the table taking into account visibility collapse
    * on columns and colgroups
    * @param aBorderPadding  the border and padding of the table
    */
  nscoord GetCollapsedWidth(nsMargin aBorderPadding);

  
  /** Adjust the table for visibility.collapse set on rowgroups, rows,
    * colgroups and cols
    * @param aDesiredSize    the metrics of the table
    * @param aBorderPadding  the border and padding of the table
    */
  void AdjustForCollapsingRowsCols(nsHTMLReflowMetrics& aDesiredSize,
                                   nsMargin             aBorderPadding);

  /** FixupPositionedTableParts is called at the end of table reflow to reflow
   *  the absolutely positioned descendants of positioned table parts. This is
   *  necessary because the dimensions of table parts may change after they've
   *  been reflowed (e.g. in AdjustForCollapsingRowsCols).
   */

  void FixupPositionedTableParts(nsPresContext* aPresContext,
                                 const nsHTMLReflowState& aReflowState);

  // Clears the list of positioned table parts.
  void ClearAllPositionedTableParts();

  nsITableLayoutStrategy* LayoutStrategy() const {
    return static_cast<nsTableFrame*>(FirstInFlow())->
      mTableLayoutStrategy;
  }

  // Helper for InsertFrames.
  void HomogenousInsertFrames(ChildListID     aListID,
                              nsIFrame*       aPrevFrame,
                              nsFrameList&    aFrameList);
private:
  /* Handle a row that got inserted during reflow.  aNewHeight is the
     new height of the table after reflow. */
  void ProcessRowInserted(nscoord aNewHeight);

  // WIDTH AND HEIGHT CALCULATION

public:

  // calculate the computed height of aFrame including its border and padding given 
  // its reflow state.
  nscoord CalcBorderBoxHeight(const nsHTMLReflowState& aReflowState);

protected:

  // update the  desired height of this table taking into account the current
  // reflow state, the table attributes and the content driven rowgroup heights
  // this function can change the overflow area
  void CalcDesiredHeight(const nsHTMLReflowState& aReflowState, nsHTMLReflowMetrics& aDesiredSize);

  // The following is a helper for CalcDesiredHeight 
 
  void DistributeHeightToRows(const nsHTMLReflowState& aReflowState,
                              nscoord                  aAmount);

  void PlaceChild(nsTableReflowState&  aReflowState,
                  nsIFrame*            aKidFrame,
                  nsHTMLReflowMetrics& aKidDesiredSize,
                  const nsRect&        aOriginalKidRect,
                  const nsRect&        aOriginalKidVisualOverflow);
   void PlaceRepeatedFooter(nsTableReflowState& aReflowState,
                            nsTableRowGroupFrame *aTfoot,
                            nscoord aFooterHeight);

  nsIFrame* GetFirstBodyRowGroupFrame();
public:
  typedef nsAutoTArray<nsTableRowGroupFrame*, 8> RowGroupArray;
  /**
   * Push all our child frames from the aRowGroups array, in order, starting
   * from the frame at aPushFrom to the end of the array. The frames are put on
   * our overflow list or moved directly to our next-in-flow if one exists.
   */
protected:
  void PushChildren(const RowGroupArray& aRowGroups, int32_t aPushFrom);

public:
  // put the children frames in the display order (e.g. thead before tbodies
  // before tfoot). This will handle calling GetRowGroupFrame() on the
  // children, and not append nulls, so the array is guaranteed to contain
  // nsTableRowGroupFrames.  If there are multiple theads or tfoots, all but
  // the first one are treated as tbodies instead.

  void OrderRowGroups(RowGroupArray& aChildren,
                      nsTableRowGroupFrame** aHead = nullptr,
                      nsTableRowGroupFrame** aFoot = nullptr) const;

  // Return the thead, if any
  nsTableRowGroupFrame* GetTHead() const;

  // Return the tfoot, if any
  nsTableRowGroupFrame* GetTFoot() const;

  // Returns true if there are any cells above the row at
  // aRowIndex and spanning into the row at aRowIndex, the number of
  // effective columns limits the search up to that column
  bool RowIsSpannedInto(int32_t aRowIndex, int32_t aNumEffCols);

  // Returns true if there is a cell originating in aRowIndex
  // which spans into the next row,  the number of effective
  // columns limits the search up to that column
  bool RowHasSpanningCells(int32_t aRowIndex, int32_t aNumEffCols);

protected:

  bool HaveReflowedColGroups() const;
  void   SetHaveReflowedColGroups(bool aValue);

public:
  bool IsBorderCollapse() const;

  bool NeedToCalcBCBorders() const;
  void SetNeedToCalcBCBorders(bool aValue);

  bool NeedToCollapse() const;
  void SetNeedToCollapse(bool aValue);

  bool HasZeroColSpans() const;
  void SetHasZeroColSpans(bool aValue);

  bool NeedColSpanExpansion() const;
  void SetNeedColSpanExpansion(bool aValue);

  /** The GeometryDirty bit is similar to the NS_FRAME_IS_DIRTY frame
    * state bit, which implies that all descendants are dirty.  The
    * GeometryDirty still implies that all the parts of the table are
    * dirty, but resizing optimizations should still apply to the
    * contents of the individual cells.
    */
  void SetGeometryDirty() { mBits.mGeometryDirty = true; }
  void ClearGeometryDirty() { mBits.mGeometryDirty = false; }
  bool IsGeometryDirty() const { return mBits.mGeometryDirty; }

  /** Get the cell map for this table frame.  It is not always mCellMap.
    * Only the firstInFlow has a legit cell map
    */
  nsTableCellMap* GetCellMap() const;

  /** Iterate over the row groups and adjust the row indices of all rows 
    * whose index is >= aRowIndex.  
    * @param aRowIndex   - start adjusting with this index
    * @param aAdjustment - shift the row index by this amount
    */
  void AdjustRowIndices(int32_t aRowIndex,
                        int32_t aAdjustment);

  /** Reset the rowindices of all rows as they might have changed due to 
    * rowgroup reordering, exclude new row group frames that show in the
    * reordering but are not yet inserted into the cellmap
    * @param aRowGroupsToExclude - an iterator that will produce the row groups
    *                              to exclude.
    */
  void ResetRowIndices(const nsFrameList::Slice& aRowGroupsToExclude);

  nsTArray<nsTableColFrame*>& GetColCache();


protected:

  void SetBorderCollapse(bool aValue);

  BCPropertyData* GetBCProperty(bool aCreateIfNecessary = false) const;
  void SetFullBCDamageArea();
  void CalcBCBorders();

  void ExpandBCDamageArea(nsIntRect& aRect) const;

  void SetColumnDimensions(nscoord         aHeight,
                           const nsMargin& aReflowState);

  int32_t CollectRows(nsIFrame*                   aFrame,
                      nsTArray<nsTableRowFrame*>& aCollection);

public: /* ----- Cell Map public methods ----- */

  int32_t GetStartRowIndex(nsTableRowGroupFrame* aRowGroupFrame);

  /** returns the number of rows in this table.
    */
  int32_t GetRowCount () const
  {
    return GetCellMap()->GetRowCount();
  }

  /** returns the number of columns in this table after redundant columns have been removed 
    */
  int32_t GetEffectiveColCount() const;

  /* return the col count including dead cols */
  int32_t GetColCount () const
  {
    return GetCellMap()->GetColCount();
  }

  // return the last col index which isn't of type eColAnonymousCell
  int32_t GetIndexOfLastRealCol();

  /** returns true if table-layout:auto  */
  bool IsAutoLayout();

public:
 
#ifdef DEBUG
  void Dump(bool            aDumpRows,
            bool            aDumpCols, 
            bool            aDumpCellMap);
#endif

protected:
  /**
   * Helper method for RemoveFrame.
   */
  void DoRemoveFrame(ChildListID aListID, nsIFrame* aOldFrame);
#ifdef DEBUG
  void DumpRowGroup(nsIFrame* aChildFrame);
#endif
  // DATA MEMBERS
  nsAutoTArray<nsTableColFrame*, 8> mColFrames;

  struct TableBits {
    uint32_t mHaveReflowedColGroups:1; // have the col groups gotten their initial reflow
    uint32_t mHasPctCol:1;             // does any cell or col have a pct width
    uint32_t mCellSpansPctCol:1;       // does any cell span a col with a pct width (or containing a cell with a pct width)
    uint32_t mIsBorderCollapse:1;      // border collapsing model vs. separate model
    uint32_t mRowInserted:1;
    uint32_t mNeedToCalcBCBorders:1;
    uint32_t mGeometryDirty:1;
    uint32_t mLeftContBCBorder:8;
    uint32_t mNeedToCollapse:1;    // rows, cols that have visibility:collapse need to be collapsed
    uint32_t mHasZeroColSpans:1;
    uint32_t mNeedColSpanExpansion:1;
    uint32_t mResizedColumns:1;        // have we resized columns since last reflow?
  } mBits;

  nsTableCellMap*         mCellMap;            // maintains the relationships between rows, cols, and cells
  nsITableLayoutStrategy* mTableLayoutStrategy;// the layout strategy for this frame
  nsFrameList             mColGroups;          // the list of colgroup frames
};


inline bool nsTableFrame::IsRowGroup(int32_t aDisplayType) const
{
  return bool((NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == aDisplayType) ||
                (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == aDisplayType) ||
                (NS_STYLE_DISPLAY_TABLE_ROW_GROUP    == aDisplayType));
}

inline void nsTableFrame::SetHaveReflowedColGroups(bool aValue)
{
  mBits.mHaveReflowedColGroups = aValue;
}

inline bool nsTableFrame::HaveReflowedColGroups() const
{
  return (bool)mBits.mHaveReflowedColGroups;
}

inline bool nsTableFrame::HasPctCol() const
{
  return (bool)mBits.mHasPctCol;
}

inline void nsTableFrame::SetHasPctCol(bool aValue)
{
  mBits.mHasPctCol = (unsigned)aValue;
}

inline bool nsTableFrame::HasCellSpanningPctCol() const
{
  return (bool)mBits.mCellSpansPctCol;
}

inline void nsTableFrame::SetHasCellSpanningPctCol(bool aValue)
{
  mBits.mCellSpansPctCol = (unsigned)aValue;
}

inline bool nsTableFrame::IsRowInserted() const
{
  return (bool)mBits.mRowInserted;
}

inline void nsTableFrame::SetRowInserted(bool aValue)
{
  mBits.mRowInserted = (unsigned)aValue;
}

inline void nsTableFrame::SetNeedToCollapse(bool aValue)
{
  static_cast<nsTableFrame*>(FirstInFlow())->mBits.mNeedToCollapse = (unsigned)aValue;
}

inline bool nsTableFrame::NeedToCollapse() const
{
  return (bool) static_cast<nsTableFrame*>(FirstInFlow())->mBits.mNeedToCollapse;
}

inline void nsTableFrame::SetHasZeroColSpans(bool aValue)
{
  mBits.mHasZeroColSpans = (unsigned)aValue;
}

inline bool nsTableFrame::HasZeroColSpans() const
{
  return (bool)mBits.mHasZeroColSpans;
}

inline void nsTableFrame::SetNeedColSpanExpansion(bool aValue)
{
  mBits.mNeedColSpanExpansion = (unsigned)aValue;
}

inline bool nsTableFrame::NeedColSpanExpansion() const
{
  return (bool)mBits.mNeedColSpanExpansion;
}


inline nsFrameList& nsTableFrame::GetColGroups()
{
  return static_cast<nsTableFrame*>(FirstInFlow())->mColGroups;
}

inline nsTArray<nsTableColFrame*>& nsTableFrame::GetColCache()
{
  return mColFrames;
}

inline bool nsTableFrame::IsBorderCollapse() const
{
  return (bool)mBits.mIsBorderCollapse;
}

inline void nsTableFrame::SetBorderCollapse(bool aValue) 
{
  mBits.mIsBorderCollapse = aValue;
}

inline bool nsTableFrame::NeedToCalcBCBorders() const
{
  return (bool)mBits.mNeedToCalcBCBorders;
}

inline void nsTableFrame::SetNeedToCalcBCBorders(bool aValue)
{
  mBits.mNeedToCalcBCBorders = (unsigned)aValue;
}

inline nscoord
nsTableFrame::GetContinuousLeftBCBorderWidth() const
{
  int32_t aPixelsToTwips = nsPresContext::AppUnitsPerCSSPixel();
  return BC_BORDER_RIGHT_HALF_COORD(aPixelsToTwips, mBits.mLeftContBCBorder);
}

inline void nsTableFrame::SetContinuousLeftBCBorderWidth(nscoord aValue)
{
  mBits.mLeftContBCBorder = (unsigned) aValue;
}

class nsTableIterator
{
public:
  nsTableIterator(nsIFrame& aSource);
  nsTableIterator(nsFrameList& aSource);
  nsIFrame* First();
  nsIFrame* Next();
  bool      IsLeftToRight();
  int32_t   Count();

protected:
  void Init(nsIFrame* aFirstChild);
  bool      mLeftToRight;
  nsIFrame* mFirstListChild;
  nsIFrame* mFirstChild;
  nsIFrame* mCurrentChild;
  int32_t   mCount;
};

#define ABORT0() \
{NS_ASSERTION(false, "CellIterator program error"); \
return;}

#define ABORT1(aReturn) \
{NS_ASSERTION(false, "CellIterator program error"); \
return aReturn;} 

#endif
