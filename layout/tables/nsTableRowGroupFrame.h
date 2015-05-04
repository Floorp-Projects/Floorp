/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTableRowGroupFrame_h__
#define nsTableRowGroupFrame_h__

#include "mozilla/Attributes.h"
#include "nscore.h"
#include "nsContainerFrame.h"
#include "nsIAtom.h"
#include "nsILineIterator.h"
#include "nsTablePainter.h"
#include "nsTArray.h"
#include "nsTableFrame.h"
#include "mozilla/WritingModes.h"

class nsTableRowFrame;

struct nsRowGroupReflowState {
  const nsHTMLReflowState& reflowState;  // Our reflow state

  nsTableFrame* tableFrame;

  // The available size (computed from the parent)
  nsSize availSize;

  // Running y-offset
  nscoord y;

  nsRowGroupReflowState(const nsHTMLReflowState& aReflowState,
                        nsTableFrame*            aTableFrame)
      :reflowState(aReflowState), tableFrame(aTableFrame)
  {
    availSize.width  = reflowState.AvailableWidth();
    availSize.height = reflowState.AvailableHeight();
    y = 0;
  }

  ~nsRowGroupReflowState() {}
};

#define MIN_ROWS_NEEDING_CURSOR 20

/**
 * nsTableRowGroupFrame is the frame that maps row groups
 * (HTML tags THEAD, TFOOT, and TBODY). This class cannot be reused
 * outside of an nsTableFrame.  It assumes that its parent is an nsTableFrame, and
 * its children are nsTableRowFrames.
 *
 * @see nsTableFrame
 * @see nsTableRowFrame
 */
class nsTableRowGroupFrame final
  : public nsContainerFrame
  , public nsILineIterator
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsTableRowGroupFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  /** instantiate a new instance of nsTableRowFrame.
    * @param aPresShell the pres shell for this frame
    *
    * @return           the frame that was created
    */
  friend nsTableRowGroupFrame* NS_NewTableRowGroupFrame(nsIPresShell* aPresShell,
                                                        nsStyleContext* aContext);
  virtual ~nsTableRowGroupFrame();

  nsTableFrame* GetTableFrame() const
  {
    nsIFrame* parent = GetParent();
    MOZ_ASSERT(parent && parent->GetType() == nsGkAtoms::tableFrame);
    return static_cast<nsTableFrame*>(parent);
  }

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  /** @see nsIFrame::DidSetStyleContext */
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) override;

  virtual void AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList) override;
  virtual void InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList) override;
  virtual void RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame) override;

  virtual nsMargin GetUsedMargin() const override;
  virtual nsMargin GetUsedBorder() const override;
  virtual nsMargin GetUsedPadding() const override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

   /** calls Reflow for all of its child rows.
    * Rows are all set to the same width and stacked vertically.
    * <P> rows are not split unless absolutely necessary.
    *
    * @param aDesiredSize width set to width of rows, height set to
    *                     sum of height of rows that fit in aMaxSize.height.
    *
    * @see nsIFrame::Reflow
    */
  virtual void Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus) override;

  virtual bool UpdateOverflow() override;

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::tableRowGroupFrame
   */
  virtual nsIAtom* GetType() const override;

  nsTableRowFrame* GetFirstRow();

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  virtual mozilla::WritingMode GetWritingMode() const override
    { return GetTableFrame()->GetWritingMode(); }

  /** return the number of child rows (not necessarily == number of child frames) */
  int32_t GetRowCount();

  /** return the table-relative row index of the first row in this rowgroup.
    * if there are no rows, -1 is returned.
    */
  int32_t GetStartRowIndex();

  /** Adjust the row indices of all rows  whose index is >= aRowIndex.
    * @param aRowIndex   - start adjusting with this index
    * @param aAdjustment - shift the row index by this amount
    */
  void AdjustRowIndices(int32_t   aRowIndex,
                        int32_t   anAdjustment);

  /**
   * Used for header and footer row group frames that are repeated when
   * splitting a table frame.
   *
   * Performs any table specific initialization
   *
   * @param aHeaderFooterFrame the original header or footer row group frame
   * that was repeated
   */
  nsresult  InitRepeatedFrame(nsPresContext*        aPresContext,
                              nsTableRowGroupFrame* aHeaderFooterFrame);


  /**
   * Get the total height of all the row rects
   */
  nscoord GetHeightBasis(const nsHTMLReflowState& aReflowState);

  mozilla::LogicalMargin GetBCBorderWidth(mozilla::WritingMode aWM);

  /**
   * Gets inner border widths before collapsing with cell borders
   * Caller must get top border from previous row group or from table
   * GetContinuousBCBorderWidth will not overwrite aBorder.top
   * see nsTablePainter about continuous borders
   */
  void GetContinuousBCBorderWidth(nsMargin& aBorder);
  /**
   * Sets full border widths before collapsing with cell borders
   * @param aForSide - side to set; only right, left, and bottom valid
   */
  void SetContinuousBCBorderWidth(uint8_t     aForSide,
                                  BCPixelSize aPixelValue);
  /**
    * Adjust to the effect of visibibility:collapse on the row group and
    * its children
    * @return              additional shift upward that should be applied to
    *                      subsequent rowgroups due to rows and this rowgroup
    *                      being collapsed
    * @param aYTotalOffset the total amount that the rowgroup is shifted up
    * @param aWidth        new width of the rowgroup
    */
  nscoord CollapseRowGroupIfNecessary(nscoord aYTotalOffset,
                                      nscoord aWidth);

// nsILineIterator methods
public:
  virtual void DisposeLineIterator() override { }

  // The table row is the equivalent to a line in block layout.
  // The nsILineIterator assumes that a line resides in a block, this role is
  // fullfilled by the row group. Rows in table are counted relative to the
  // table. The row index of row corresponds to the cellmap coordinates. The
  // line index with respect to a row group can be computed by substracting the
  // row index of the first row in the row group.

  /** Get the number of rows in a row group
    * @return the number of lines in a row group
    */
  virtual int32_t GetNumLines() override;

  /** @see nsILineIterator.h GetDirection
    * @return true if the table is rtl
    */
  virtual bool GetDirection() override;

  /** Return structural information about a line.
    * @param aLineNumber       - the index of the row relative to the row group
    *                            If the line-number is invalid then
    *                            aFirstFrameOnLine will be nullptr and
    *                            aNumFramesOnLine will be zero.
    * @param aFirstFrameOnLine - the first cell frame that originates in row
    *                            with a rowindex that matches a line number
    * @param aNumFramesOnLine  - return the numbers of cells originating in
    *                            this row
    * @param aLineBounds       - rect of the row
    */
  NS_IMETHOD GetLine(int32_t aLineNumber,
                     nsIFrame** aFirstFrameOnLine,
                     int32_t* aNumFramesOnLine,
                     nsRect& aLineBounds) override;

  /** Given a frame that's a child of the rowgroup, find which line its on.
    * @param aFrame       - frame, should be a row
    * @param aStartLine   - minimal index to return
    * @return               row index relative to the row group if this a row
    *                       frame and the index is at least aStartLine.
    *                       -1 if the frame cannot be found.
    */
  virtual int32_t FindLineContaining(nsIFrame* aFrame, int32_t aStartLine = 0) override;

  /** Find the orginating cell frame on a row that is the nearest to the
    * inline-dir coordinate of aPos.
    * @param aLineNumber          - the index of the row relative to the row group
    * @param aPos                 - coordinate in twips relative to the
    *                               origin of the row group
    * @param aFrameFound          - pointer to the cellframe
    * @param aPosIsBeforeFirstFrame - the point is before the first originating
    *                               cellframe
    * @param aPosIsAfterLastFrame   - the point is after the last originating
    *                               cellframe
    */
  NS_IMETHOD FindFrameAt(int32_t aLineNumber,
                         nsPoint aPos,
                         nsIFrame** aFrameFound,
                         bool* aPosIsBeforeFirstFrame,
                         bool* aPosIsAfterLastFrame) override;

   /** Check whether visual and logical order of cell frames within a line are
     * identical. As the layout will reorder them this is always the case
     * @param aLine        - the index of the row relative to the table
     * @param aIsReordered - returns false
     * @param aFirstVisual - if the table is rtl first originating cell frame
     * @param aLastVisual  - if the table is rtl last originating cell frame
     */

  NS_IMETHOD CheckLineOrder(int32_t                  aLine,
                            bool                     *aIsReordered,
                            nsIFrame                 **aFirstVisual,
                            nsIFrame                 **aLastVisual) override;

  /** Find the next originating cell frame that originates in the row.
    * @param aFrame      - cell frame to start with, will return the next cell
    *                      originating in a row
    * @param aLineNumber - the index of the row relative to the table
    */
  NS_IMETHOD GetNextSiblingOnLine(nsIFrame*& aFrame, int32_t aLineNumber) override;

  // row cursor methods to speed up searching for the row(s)
  // containing a point. The basic idea is that we set the cursor
  // property if the rows' y and yMosts are non-decreasing (considering only
  // rows with nonempty overflowAreas --- empty overflowAreas never participate
  // in event handling or painting), and the rowgroup has sufficient number of
  // rows. The cursor property points to a "recently used" row. If we get a
  // series of requests that work on rows "near" the cursor, then we can find
  // those nearby rows quickly by starting our search at the cursor.
  // This code is based on the line cursor code in nsBlockFrame. It's more general
  // though, and could be extracted and used elsewhere.
  struct FrameCursorData {
    nsTArray<nsIFrame*> mFrames;
    uint32_t            mCursorIndex;
    nscoord             mOverflowAbove;
    nscoord             mOverflowBelow;

    FrameCursorData()
      : mFrames(MIN_ROWS_NEEDING_CURSOR), mCursorIndex(0), mOverflowAbove(0),
        mOverflowBelow(0) {}

    bool AppendFrame(nsIFrame* aFrame);

    void FinishBuildingCursor() {
      mFrames.Compact();
    }
  };

  // Clear out row cursor because we're disturbing the rows (e.g., Reflow)
  void ClearRowCursor();

  /**
   * Get the first row that might contain y-coord 'aY', or nullptr if you must search
   * all rows.
   * The actual row returned might not contain 'aY', but if not, it is guaranteed
   * to be before any row which does contain 'aY'.
   * aOverflowAbove is the maximum over all rows of -row.GetOverflowRect().y.
   * To find all rows that intersect the vertical interval aY/aYMost, call
   * GetFirstRowContaining(aY, &overflowAbove), and then iterate through all
   * rows until reaching a row where row->GetRect().y - overflowAbove >= aYMost.
   * That row and all subsequent rows cannot intersect the interval.
   */
  nsIFrame* GetFirstRowContaining(nscoord aY, nscoord* aOverflowAbove);

  /**
   * Set up the row cursor. After this, call AppendFrame for every
   * child frame in sibling order. Ensure that the child frame y and YMost values
   * form non-decreasing sequences (should always be true for table rows);
   * if this is violated, call ClearRowCursor(). If we return nullptr, then we
   * decided not to use a cursor or we already have one set up.
   */
  FrameCursorData* SetupRowCursor();

  virtual nsILineIterator* GetLineIterator() override { return this; }

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsContainerFrame::IsFrameOfType(aFlags & ~(nsIFrame::eTablePart));
  }

  virtual void InvalidateFrame(uint32_t aDisplayItemKey = 0) override;
  virtual void InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey = 0) override;
  virtual void InvalidateFrameForRemoval() override { InvalidateFrameSubtree(); }

protected:
  explicit nsTableRowGroupFrame(nsStyleContext* aContext);

  void InitChildReflowState(nsPresContext&     aPresContext,
                            bool               aBorderCollapse,
                            nsHTMLReflowState& aReflowState);

  virtual LogicalSides GetLogicalSkipSides(const nsHTMLReflowState* aReflowState = nullptr) const override;

  void PlaceChild(nsPresContext*         aPresContext,
                  nsRowGroupReflowState& aReflowState,
                  nsIFrame*              aKidFrame,
                  nsPoint                aKidPosition,
                  nsHTMLReflowMetrics&   aDesiredSize,
                  const nsRect&          aOriginalKidRect,
                  const nsRect&          aOriginalKidVisualOverflow);

  void CalculateRowHeights(nsPresContext*           aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState);

  void DidResizeRows(nsHTMLReflowMetrics& aDesiredSize);

  void SlideChild(nsRowGroupReflowState& aReflowState,
                  nsIFrame*              aKidFrame);

  /**
   * Reflow the frames we've already created
   *
   * @param   aPresContext presentation context to use
   * @param   aReflowState current inline state
   */
  void ReflowChildren(nsPresContext*         aPresContext,
                      nsHTMLReflowMetrics&   aDesiredSize,
                      nsRowGroupReflowState& aReflowState,
                      nsReflowStatus&        aStatus,
                      bool*                aPageBreakBeforeEnd = nullptr);

  nsresult SplitRowGroup(nsPresContext*           aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsTableFrame*            aTableFrame,
                         nsReflowStatus&          aStatus,
                         bool                     aRowForcedPageBreak);

  void SplitSpanningCells(nsPresContext&           aPresContext,
                          const nsHTMLReflowState& aReflowState,
                          nsTableFrame&            aTableFrame,
                          nsTableRowFrame&         aFirstRow,
                          nsTableRowFrame&         aLastRow,
                          bool                     aFirstRowIsTopOfPage,
                          nscoord                  aSpanningRowBottom,
                          nsTableRowFrame*&        aContRowFrame,
                          nsTableRowFrame*&        aFirstTruncatedRow,
                          nscoord&                 aDesiredHeight);

  void CreateContinuingRowFrame(nsPresContext& aPresContext,
                                nsIFrame&      aRowFrame,
                                nsIFrame**     aContRowFrame);

  bool IsSimpleRowFrame(nsTableFrame* aTableFrame,
                          nsIFrame*     aFrame);

  void GetNextRowSibling(nsIFrame** aRowFrame);

  void UndoContinuedRow(nsPresContext*   aPresContext,
                        nsTableRowFrame* aRow);

private:
  // border widths in pixels in the collapsing border model
  BCPixelSize mRightContBorderWidth;
  BCPixelSize mBottomContBorderWidth;
  BCPixelSize mLeftContBorderWidth;

public:
  bool IsRepeatable() const;
  void   SetRepeatable(bool aRepeatable);
  bool HasStyleHeight() const;
  void   SetHasStyleHeight(bool aValue);
  bool HasInternalBreakBefore() const;
  bool HasInternalBreakAfter() const;
};


inline bool nsTableRowGroupFrame::IsRepeatable() const
{
  return (mState & NS_ROWGROUP_REPEATABLE) == NS_ROWGROUP_REPEATABLE;
}

inline void nsTableRowGroupFrame::SetRepeatable(bool aRepeatable)
{
  if (aRepeatable) {
    mState |= NS_ROWGROUP_REPEATABLE;
  } else {
    mState &= ~NS_ROWGROUP_REPEATABLE;
  }
}

inline bool nsTableRowGroupFrame::HasStyleHeight() const
{
  return (mState & NS_ROWGROUP_HAS_STYLE_HEIGHT) == NS_ROWGROUP_HAS_STYLE_HEIGHT;
}

inline void nsTableRowGroupFrame::SetHasStyleHeight(bool aValue)
{
  if (aValue) {
    mState |= NS_ROWGROUP_HAS_STYLE_HEIGHT;
  } else {
    mState &= ~NS_ROWGROUP_HAS_STYLE_HEIGHT;
  }
}

inline void
nsTableRowGroupFrame::GetContinuousBCBorderWidth(nsMargin& aBorder)
{
  int32_t aPixelsToTwips = nsPresContext::AppUnitsPerCSSPixel();
  aBorder.right = BC_BORDER_START_HALF_COORD(aPixelsToTwips,
                                             mRightContBorderWidth);
  aBorder.bottom = BC_BORDER_START_HALF_COORD(aPixelsToTwips,
                                              mBottomContBorderWidth);
  aBorder.left = BC_BORDER_END_HALF_COORD(aPixelsToTwips,
                                          mLeftContBorderWidth);
  return;
}
#endif
