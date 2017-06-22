/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTableFrame_h__
#define nsTableFrame_h__

#include "mozilla/Attributes.h"
#include "celldata.h"
#include "imgIContainer.h"
#include "nscore.h"
#include "nsContainerFrame.h"
#include "nsStyleCoord.h"
#include "nsStyleConsts.h"
#include "nsCellMap.h"
#include "nsGkAtoms.h"
#include "nsDisplayList.h"
#include "TableArea.h"

struct BCPaintBorderAction;
class nsTableCellFrame;
class nsTableCellMap;
class nsTableColFrame;
class nsTableRowGroupFrame;
class nsTableRowFrame;
class nsTableColGroupFrame;
class nsITableLayoutStrategy;
class nsStyleContext;
namespace mozilla {
class WritingMode;
class LogicalMargin;
struct TableReflowInput;
namespace layers {
class StackingContextHelper;
}
} // namespace mozilla

struct BCPropertyData;

static inline bool
IS_TABLE_CELL(mozilla::LayoutFrameType frameType)
{
  return frameType == mozilla::LayoutFrameType::TableCell ||
         frameType == mozilla::LayoutFrameType::BCTableCell;
}

class nsDisplayTableItem : public nsDisplayItem
{
public:
  nsDisplayTableItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                     bool aDrawsBackground = true) :
      nsDisplayItem(aBuilder, aFrame),
      mPartHasFixedBackground(false),
      mDrawsBackground(aDrawsBackground) {}

  // With collapsed borders, parts of the collapsed border can extend outside
  // the table part frames, so allow this display element to blow out to our
  // overflow rect. This is also useful for row frames that have spanning
  // cells extending outside them.
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override;

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override;
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion *aInvalidRegion) override;

  void UpdateForFrameBackground(nsIFrame* aFrame);

private:
  bool mPartHasFixedBackground;
  bool mDrawsBackground;
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

enum nsTableColGroupType {
  eColGroupContent            = 0, // there is real col group content associated
  eColGroupAnonymousCol       = 1, // the result of a col
  eColGroupAnonymousCell      = 2  // the result of a cell alone
};

enum nsTableColType {
  eColContent            = 0, // there is real col content associated
  eColAnonymousCol       = 1, // the result of a span on a col
  eColAnonymousColGroup  = 2, // the result of a span on a col group
  eColAnonymousCell      = 3  // the result of a cell alone
};

/**
  * nsTableFrame maps the inner portion of a table (everything except captions.)
  * Used as a pseudo-frame within nsTableWrapperFrame, it may also be used
  * stand-alone as the top-level frame.
  *
  * The principal child list contains row group frames. There is also an
  * additional child list, kColGroupList, which contains the col group frames.
  */
class nsTableFrame : public nsContainerFrame
{
  typedef mozilla::image::DrawResult DrawResult;
  typedef mozilla::WritingMode WritingMode;
  typedef mozilla::LogicalMargin LogicalMargin;
  typedef mozilla::TableReflowInput TableReflowInput;

public:
  NS_DECL_FRAMEARENA_HELPERS(nsTableFrame)

  typedef nsTArray<nsIFrame*> FrameTArray;
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(PositionedTablePartArray, FrameTArray)

  /** nsTableWrapperFrame has intimate knowledge of the inner table frame */
  friend class nsTableWrapperFrame;

  /** instantiate a new instance of nsTableRowFrame.
    * @param aPresShell the pres shell for this frame
    *
    * @return           the frame that was created
    */
  friend nsTableFrame* NS_NewTableFrame(nsIPresShell* aPresShell,
                                        nsStyleContext* aContext);

  /** sets defaults for table-specific style.
    * @see nsIFrame::Init
    */
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

  static float GetTwipsToPixels(nsPresContext* aPresContext);

  // Return true if aParentReflowInput.frame or any of its ancestors within
  // the containing table have non-auto bsize. (e.g. pct or fixed bsize)
  static bool AncestorsHaveStyleBSize(const ReflowInput& aParentReflowInput);

  // See if a special bsize reflow will occur due to having a pct bsize when
  // the pct bsize basis may not yet be valid.
  static void CheckRequestSpecialBSizeReflow(const ReflowInput& aReflowInput);

  // Notify the frame and its ancestors (up to the containing table) that a special
  // height reflow will occur.
  static void RequestSpecialBSizeReflow(const ReflowInput& aReflowInput);

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

  nsPoint GetFirstSectionOrigin(const ReflowInput& aReflowInput) const;
  /*
   * Notification that aAttribute has changed for content inside a table (cell, row, etc)
   */
  void AttributeChangedFor(nsIFrame*       aFrame,
                           nsIContent*     aContent,
                           nsIAtom*        aAttribute);

  /** @see nsIFrame::DestroyFrom */
  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  /** @see nsIFrame::DidSetStyleContext */
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) override;

  virtual void SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList) override;
  virtual void AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList) override;
  virtual void InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList) override;
  virtual void RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame) override;

  virtual nsMargin GetUsedBorder() const override;
  virtual nsMargin GetUsedPadding() const override;
  virtual nsMargin GetUsedMargin() const override;

  // Get the offset from the border box to the area where the row groups fit
  LogicalMargin GetChildAreaOffset(const WritingMode aWM,
                                   const ReflowInput* aReflowInput) const;

  /** helper method to find the table parent of any table frame object */
  static nsTableFrame* GetTableFrame(nsIFrame* aSourceFrame);

  /* Like GetTableFrame, but will set *aDidPassThrough to false if we don't
   * pass through aMustPassThrough on the way to the table.
   */
  static nsTableFrame* GetTableFramePassingThrough(nsIFrame* aMustPassThrough,
                                                   nsIFrame* aSourceFrame,
                                                   bool* aDidPassThrough);

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
                                      DisplayGenericTablePartTraversal aTraversal = GenericTraversal);

  // Return the closest sibling of aPriorChildFrame (including aPriroChildFrame)
  // of type aChildType.
  static nsIFrame* GetFrameAtOrBefore(nsIFrame* aParentFrame,
                                      nsIFrame* aPriorChildFrame,
                                      mozilla::LayoutFrameType aChildType);
  bool IsAutoBSize(mozilla::WritingMode aWM);

  /** @return true if aDisplayType represents a rowgroup of any sort
    * (header, footer, or body)
    */
  bool IsRowGroup(mozilla::StyleDisplay aDisplayType) const;

  virtual const nsFrameList& GetChildList(ChildListID aListID) const override;
  virtual void GetChildLists(nsTArray<ChildList>* aLists) const override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  /** Get the outer half (i.e., the part outside the height and width of
   *  the table) of the largest segment (?) of border-collapsed border on
   *  the table on each side, or 0 for non border-collapsed tables.
   */
  LogicalMargin GetOuterBCBorder(const WritingMode aWM) const;

  /** Same as above, but only if it's included from the border-box width
   *  of the table.
   */
  LogicalMargin GetIncludedOuterBCBorder(const WritingMode aWM) const;

  /** Same as above, but only if it's excluded from the border-box width
   *  of the table.  This is the area that leaks out into the margin
   *  (or potentially past it, if there is no margin).
   */
  LogicalMargin GetExcludedOuterBCBorder(const WritingMode aWM) const;

  /**
   * In quirks mode, the size of the table background is reduced
   * by the outer BC border. Compute the reduction needed.
   */
  nsMargin GetDeflationForBackground(nsPresContext* aPresContext) const;

  /** Get width of table + colgroup + col collapse: elements that
   *  continue along the length of the whole iStart side.
   *  see nsTablePainter about continuous borders
   */
  nscoord GetContinuousIStartBCBorderWidth() const;
  void SetContinuousIStartBCBorderWidth(nscoord aValue);

  friend class nsDelayedCalcBCBorders;

  void AddBCDamageArea(const mozilla::TableArea& aValue);
  bool BCRecalcNeeded(nsStyleContext* aOldStyleContext,
                        nsStyleContext* aNewStyleContext);
  void PaintBCBorders(DrawTarget& aDrawTarget, const nsRect& aDirtyRect);
  void CreateWebRenderCommandsForBCBorders(mozilla::wr::DisplayListBuilder& aBuilder,
                                           const mozilla::layers::StackingContextHelper& aSc,
                                           nsTArray<mozilla::layers::WebRenderParentCommand>& aParentCommands,
                                           mozilla::layers::WebRenderDisplayItemLayer* aLayer,
                                           const nsPoint& aPt);

  virtual void MarkIntrinsicISizesDirty() override;
  // For border-collapse tables, the caller must not add padding and
  // border to the results of these functions.
  virtual nscoord GetMinISize(gfxContext *aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext *aRenderingContext) override;
  virtual IntrinsicISizeOffsetData IntrinsicISizeOffsets() override;

  virtual mozilla::LogicalSize
  ComputeSize(gfxContext*                 aRenderingContext,
              mozilla::WritingMode        aWM,
              const mozilla::LogicalSize& aCBSize,
              nscoord                     aAvailableISize,
              const mozilla::LogicalSize& aMargin,
              const mozilla::LogicalSize& aBorder,
              const mozilla::LogicalSize& aPadding,
              ComputeSizeFlags            aFlags) override;

  virtual mozilla::LogicalSize
  ComputeAutoSize(gfxContext*                 aRenderingContext,
                  mozilla::WritingMode        aWM,
                  const mozilla::LogicalSize& aCBSize,
                  nscoord                     aAvailableISize,
                  const mozilla::LogicalSize& aMargin,
                  const mozilla::LogicalSize& aBorder,
                  const mozilla::LogicalSize& aPadding,
                  ComputeSizeFlags            aFlags) override;

  /**
   * A copy of nsFrame::ShrinkWidthToFit that calls a different
   * GetPrefISize, since tables have two different ones.
   */
  nscoord TableShrinkISizeToFit(gfxContext *aRenderingContext,
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
  virtual void Reflow(nsPresContext*           aPresContext,
                      ReflowOutput&     aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&          aStatus) override;

  void ReflowTable(ReflowOutput&     aDesiredSize,
                   const ReflowInput& aReflowInput,
                   nscoord                  aAvailBSize,
                   nsIFrame*&               aLastChildReflowed,
                   nsReflowStatus&          aStatus);

  nsFrameList& GetColGroups();

  virtual nsStyleContext*
  GetParentStyleContext(nsIFrame** aProviderFrame) const override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    if (aFlags & eSupportsCSSTransforms) {
      return false;
    }
    return nsContainerFrame::IsFrameOfType(aFlags);
  }

#ifdef DEBUG_FRAME_DUMP
  /** @see nsIFrame::GetFrameName */
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  /** Return the isize of the column at aColIndex.
   *  This may only be called on the table's first-in-flow.
   */
  nscoord GetColumnISizeFromFirstInFlow(int32_t aColIndex);

  /** Helper to get the column spacing style value.
   *  The argument refers to the space between column aColIndex and column
   *  aColIndex + 1.  An index of -1 indicates the padding between the table
   *  and the left border, an index equal to the number of columns indicates
   *  the padding between the table and the right border.
   *
   *  Although in this class cell spacing does not depend on the index, it
   *  may be important for overriding classes.
   */
  virtual nscoord GetColSpacing(int32_t aColIndex);

  /** Helper to find the sum of the cell spacing between arbitrary columns.
   *  The argument refers to the space between column aColIndex and column
   *  aColIndex + 1.  An index of -1 indicates the padding between the table
   *  and the left border, an index equal to the number of columns indicates
   *  the padding between the table and the right border.
   *
   *  This method is equivalent to
   *  nscoord result = 0;
   *  for (i = aStartColIndex; i < aEndColIndex; i++) {
   *    result += GetColSpacing(i);
   *  }
   *  return result;
   */
  virtual nscoord GetColSpacing(int32_t aStartColIndex,
                                int32_t aEndColIndex);

  /** Helper to get the row spacing style value.
   *  The argument refers to the space between row aRowIndex and row
   *  aRowIndex + 1.  An index of -1 indicates the padding between the table
   *  and the top border, an index equal to the number of rows indicates
   *  the padding between the table and the bottom border.
   *
   *  Although in this class cell spacing does not depend on the index, it
   *  may be important for overriding classes.
   */
  virtual nscoord GetRowSpacing(int32_t aRowIndex);

  /** Helper to find the sum of the cell spacing between arbitrary rows.
   *  The argument refers to the space between row aRowIndex and row
   *  aRowIndex + 1.  An index of -1 indicates the padding between the table
   *  and the top border, an index equal to the number of rows indicates
   *  the padding between the table and the bottom border.
   *
   *  This method is equivalent to
   *  nscoord result = 0;
   *  for (i = aStartRowIndex; i < aEndRowIndex; i++) {
   *    result += GetRowSpacing(i);
   *  }
   *  return result;
   */
  virtual nscoord GetRowSpacing(int32_t aStartRowIndex,
                                  int32_t aEndRowIndex);

private:
  /* For the base implementation of nsTableFrame, cell spacing does not depend
   * on row/column indexing.
   */
  nscoord GetColSpacing();
  nscoord GetRowSpacing();

public:
  virtual nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const override;
  bool GetNaturalBaselineBOffset(mozilla::WritingMode aWM,
                                 BaselineSharingGroup aBaselineGroup,
                                 nscoord*             aBaseline) const override;

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

  virtual bool ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas) override;

  // Return our wrapper frame.
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) override;

protected:
  static void UpdateStyleOfOwnedAnonBoxesForTableWrapper(
    nsIFrame* aOwningFrame,
    nsIFrame* aWrapperFrame,
    mozilla::ServoRestyleState& aRestyleState);

  /** protected constructor.
    * @see NewFrame
    */
  explicit nsTableFrame(nsStyleContext* aContext, ClassID aID = kClassID);

  /** destructor, responsible for mColumnLayoutData */
  virtual ~nsTableFrame();

  void InitChildReflowInput(ReflowInput& aReflowInput);

  virtual LogicalSides GetLogicalSkipSides(const ReflowInput* aReflowInput = nullptr) const override;

  void IterateBCBorders(BCPaintBorderAction& aAction, const nsRect& aDirtyRect);

public:
  bool IsRowInserted() const;
  void   SetRowInserted(bool aValue);

protected:

  // A helper function to reflow a header or footer with unconstrained height
  // to see if it should be made repeatable and also to determine its desired
  // height.
  nsresult SetupHeaderFooterChild(const TableReflowInput& aReflowInput,
                                  nsTableRowGroupFrame* aFrame,
                                  nscoord* aDesiredHeight);

  void ReflowChildren(TableReflowInput&  aReflowInput,
                      nsReflowStatus&      aStatus,
                      nsIFrame*&           aLastChildReflowed,
                      nsOverflowAreas&     aOverflowAreas);

  // This calls the col group and column reflow methods, which do two things:
  //  (1) set all the dimensions to 0
  //  (2) notify the table about colgroups or columns with hidden visibility
  void ReflowColGroups(gfxContext* aRenderingContext);

  /** return the isize of the table taking into account visibility collapse
    * on columns and colgroups
    * @param aBorderPadding  the border and padding of the table
    */
  nscoord GetCollapsedISize(const WritingMode aWM,
                            const LogicalMargin& aBorderPadding);


  /** Adjust the table for visibility.collapse set on rowgroups, rows,
    * colgroups and cols
    * @param aDesiredSize    the metrics of the table
    * @param aBorderPadding  the border and padding of the table
    */
  void AdjustForCollapsingRowsCols(ReflowOutput& aDesiredSize,
                                   const WritingMode aWM,
                                   const LogicalMargin& aBorderPadding);

  /** FixupPositionedTableParts is called at the end of table reflow to reflow
   *  the absolutely positioned descendants of positioned table parts. This is
   *  necessary because the dimensions of table parts may change after they've
   *  been reflowed (e.g. in AdjustForCollapsingRowsCols).
   */
  void FixupPositionedTableParts(nsPresContext*           aPresContext,
                                 ReflowOutput&     aDesiredSize,
                                 const ReflowInput& aReflowInput);

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

  // calculate the computed block-size of aFrame including its border and
  // padding given its reflow state.
  nscoord CalcBorderBoxBSize(const ReflowInput& aReflowInput);

protected:

  // update the  desired block-size of this table taking into account the current
  // reflow state, the table attributes and the content driven rowgroup bsizes
  // this function can change the overflow area
  void CalcDesiredBSize(const ReflowInput& aReflowInput,
                        ReflowOutput& aDesiredSize);

  // The following is a helper for CalcDesiredBSize

  void DistributeBSizeToRows(const ReflowInput& aReflowInput,
                             nscoord                  aAmount);

  void PlaceChild(TableReflowInput&  aReflowInput,
                  nsIFrame*            aKidFrame,
                  nsPoint              aKidPosition,
                  ReflowOutput& aKidDesiredSize,
                  const nsRect&        aOriginalKidRect,
                  const nsRect&        aOriginalKidVisualOverflow);
   void PlaceRepeatedFooter(TableReflowInput& aReflowInput,
                            nsTableRowGroupFrame *aTfoot,
                            nscoord aFooterHeight);

  nsIFrame* GetFirstBodyRowGroupFrame();
public:
  typedef AutoTArray<nsTableRowGroupFrame*, 8> RowGroupArray;
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

  bool NeedToCalcHasBCBorders() const;
  void SetNeedToCalcHasBCBorders(bool aValue);

  void CalcHasBCBorders();
  bool HasBCBorders();
  void SetHasBCBorders(bool aValue);

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

  BCPropertyData* GetBCProperty() const;
  BCPropertyData* GetOrCreateBCProperty();
  void SetFullBCDamageArea();
  void CalcBCBorders();

  void ExpandBCDamageArea(mozilla::TableArea& aRect) const;

  void SetColumnDimensions(nscoord aHeight, WritingMode aWM,
                           const LogicalMargin& aBorderPadding,
                           const nsSize& aContainerSize);

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

  /* ---------- Row index management methods ------------ */

  /** Add the given index to the existing ranges of
   *  deleted row indices and merge ranges if, with the addition of the new
   *  index, they become consecutive.
   *  @param aDeletedRowStoredIndex - index of the row that was deleted
   *  Note - 'stored' index here refers to the index that was assigned to
   *  the row before any remove row operations were performed i.e. the
   *  value of mRowIndex and not the value returned by GetRowIndex()
   */
  void AddDeletedRowIndex(int32_t aDeletedRowStoredIndex);

  /** Calculate the change that aStoredIndex must be increased/decreased by
   *  to get new index.
   *  Note that aStoredIndex is always the index of an undeleted row (since
   *  rows that have already been deleted can never call this method).
   *  @param aStoredIndex - The stored index value that must be adjusted
   *  Note - 'stored' index here refers to the index that was assigned to
   *  the row before any remove row operations were performed i.e. the
   *  value of mRowIndex and not the value returned by GetRowIndex()
   */
  int32_t GetAdjustmentForStoredIndex(int32_t aStoredIndex);

  /** Returns whether mDeletedRowIndexRanges is empty
   */
  bool IsDeletedRowIndexRangesEmpty() const {
    return mDeletedRowIndexRanges.empty();
  }

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
  AutoTArray<nsTableColFrame*, 8> mColFrames;

  struct TableBits {
    uint32_t mHaveReflowedColGroups:1; // have the col groups gotten their initial reflow
    uint32_t mHasPctCol:1;             // does any cell or col have a pct width
    uint32_t mCellSpansPctCol:1;       // does any cell span a col with a pct width (or containing a cell with a pct width)
    uint32_t mIsBorderCollapse:1;      // border collapsing model vs. separate model
    uint32_t mRowInserted:1;
    uint32_t mNeedToCalcBCBorders:1;
    uint32_t mGeometryDirty:1;
    uint32_t mIStartContBCBorder:8;
    uint32_t mNeedToCollapse:1;        // rows, cols that have visibility:collapse need to be collapsed
    uint32_t mResizedColumns:1;        // have we resized columns since last reflow?
    uint32_t mNeedToCalcHasBCBorders:1;
    uint32_t mHasBCBorders:1;
  } mBits;

  std::map<int32_t, int32_t> mDeletedRowIndexRanges; // maintains ranges of row
                                                     // indices of deleted rows
  nsTableCellMap*         mCellMap;            // maintains the relationships between rows, cols, and cells
  nsITableLayoutStrategy* mTableLayoutStrategy;// the layout strategy for this frame
  nsFrameList             mColGroups;          // the list of colgroup frames
};


inline bool nsTableFrame::IsRowGroup(mozilla::StyleDisplay aDisplayType) const
{
  return mozilla::StyleDisplay::TableHeaderGroup == aDisplayType ||
         mozilla::StyleDisplay::TableFooterGroup == aDisplayType ||
         mozilla::StyleDisplay::TableRowGroup    == aDisplayType;
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

inline bool nsTableFrame::NeedToCalcHasBCBorders() const
{
  return (bool)mBits.mNeedToCalcHasBCBorders;
}

inline void nsTableFrame::SetNeedToCalcHasBCBorders(bool aValue)
{
  mBits.mNeedToCalcHasBCBorders = (unsigned)aValue;
}

inline bool nsTableFrame::HasBCBorders()
{
  if (NeedToCalcHasBCBorders()) {
    CalcHasBCBorders();
    SetNeedToCalcHasBCBorders(false);
  }
  return (bool)mBits.mHasBCBorders;
}

inline void nsTableFrame::SetHasBCBorders(bool aValue)
{
  mBits.mHasBCBorders = (unsigned)aValue;
}

inline nscoord
nsTableFrame::GetContinuousIStartBCBorderWidth() const
{
  int32_t aPixelsToTwips = nsPresContext::AppUnitsPerCSSPixel();
  return BC_BORDER_END_HALF_COORD(aPixelsToTwips, mBits.mIStartContBCBorder);
}

inline void nsTableFrame::SetContinuousIStartBCBorderWidth(nscoord aValue)
{
  mBits.mIStartContBCBorder = (unsigned) aValue;
}

#define ABORT0() \
{NS_ASSERTION(false, "CellIterator program error"); \
return;}

#define ABORT1(aReturn) \
{NS_ASSERTION(false, "CellIterator program error"); \
return aReturn;}

#endif
