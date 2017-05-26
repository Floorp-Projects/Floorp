/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTableRowFrame_h__
#define nsTableRowFrame_h__

#include "mozilla/Attributes.h"
#include "nscore.h"
#include "nsContainerFrame.h"
#include "nsTableRowGroupFrame.h"
#include "mozilla/WritingModes.h"

class  nsTableCellFrame;
namespace mozilla {
struct TableCellReflowInput;
} // namespace mozilla

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
class nsTableRowFrame : public nsContainerFrame
{
  using TableCellReflowInput = mozilla::TableCellReflowInput;

public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsTableRowFrame)

  virtual ~nsTableRowFrame();

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

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

  /** instantiate a new instance of nsTableRowFrame.
    * @param aPresShell the pres shell for this frame
    *
    * @return           the frame that was created
    */
  friend nsTableRowFrame* NS_NewTableRowFrame(nsIPresShell* aPresShell,
                                              nsStyleContext* aContext);

  nsTableRowGroupFrame* GetTableRowGroupFrame() const
  {
    nsIFrame* parent = GetParent();
    MOZ_ASSERT(parent && parent->IsTableRowGroupFrame());
    return static_cast<nsTableRowGroupFrame*>(parent);
  }

  nsTableFrame* GetTableFrame() const
  {
    return GetTableRowGroupFrame()->GetTableFrame();
  }

  virtual nsMargin GetUsedMargin() const override;
  virtual nsMargin GetUsedBorder() const override;
  virtual nsMargin GetUsedPadding() const override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  nsTableCellFrame* GetFirstCell() ;

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
  virtual void Reflow(nsPresContext*           aPresContext,
                      ReflowOutput&     aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&          aStatus) override;

  void DidResize();

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  void UpdateBSize(nscoord           aBSize,
                   nscoord           aAscent,
                   nscoord           aDescent,
                   nsTableFrame*     aTableFrame = nullptr,
                   nsTableCellFrame* aCellFrame  = nullptr);

  void ResetBSize(nscoord aRowStyleBSize);

  // calculate the bsize, considering content bsize of the 
  // cells and the style bsize of the row and cells, excluding pct bsizes
  nscoord CalcBSize(const ReflowInput& aReflowInput);

  // Support for cells with 'vertical-align: baseline'.

  /**
   * returns the max-ascent amongst all the cells that have 
   * 'vertical-align: baseline', *including* cells with rowspans.
   * returns 0 if we don't have any cell with 'vertical-align: baseline'
   */
  nscoord GetMaxCellAscent() const;

  /* return the row ascent
   */
  nscoord GetRowBaseline(mozilla::WritingMode aWritingMode);

  /** returns the ordinal position of this row in its table */
  virtual int32_t GetRowIndex() const;

  /** set this row's starting row index */
  void SetRowIndex (int aRowIndex);

  // See nsTableFrame.h
  int32_t GetAdjustmentForStoredIndex(int32_t aStoredIndex) const;

  // See nsTableFrame.h
  void AddDeletedRowIndex();

  /** used by row group frame code */
  nscoord ReflowCellFrame(nsPresContext*           aPresContext,
                          const ReflowInput& aReflowInput,
                          bool                     aIsTopOfPage,
                          nsTableCellFrame*        aCellFrame,
                          nscoord                  aAvailableBSize,
                          nsReflowStatus&          aStatus);
  /**
    * Collapse the row if required, apply col and colgroup visibility: collapse
    * info to the cells in the row.
    * @return the amount to shift bstart-wards all following rows
    * @param aRowOffset     - shift the row bstart-wards by this amount
    * @param aISize         - new isize of the row
    * @param aCollapseGroup - parent rowgroup is collapsed so this row needs
    *                         to be collapsed
    * @param aDidCollapse   - the row has been collapsed
    */
  nscoord CollapseRowIfNecessary(nscoord aRowOffset,
                                 nscoord aISize,
                                 bool    aCollapseGroup,
                                 bool&   aDidCollapse);

  /**
   * Insert a cell frame after the last cell frame that has a col index
   * that is less than aColIndex.  If no such cell frame is found the
   * frame to insert is prepended to the child list.
   * @param aFrame the cell frame to insert
   * @param aColIndex the col index
   */
  void InsertCellFrame(nsTableCellFrame* aFrame,
                       int32_t           aColIndex);

  nsresult CalculateCellActualBSize(nsTableCellFrame*    aCellFrame,
                                    nscoord&             aDesiredBSize,
                                    mozilla::WritingMode aWM);

  bool IsFirstInserted() const;
  void   SetFirstInserted(bool aValue);

  nscoord GetContentBSize() const;
  void    SetContentBSize(nscoord aTwipValue);

  bool HasStyleBSize() const;

  bool HasFixedBSize() const;
  void   SetHasFixedBSize(bool aValue);

  bool HasPctBSize() const;
  void   SetHasPctBSize(bool aValue);

  nscoord GetFixedBSize() const;
  void    SetFixedBSize(nscoord aValue);

  float   GetPctBSize() const;
  void    SetPctBSize(float  aPctValue,
                       bool aForce = false);

  nscoord GetInitialBSize(nscoord aBasis = 0) const;

  nsTableRowFrame* GetNextRow() const;

  bool    HasUnpaginatedBSize();
  void    SetHasUnpaginatedBSize(bool aValue);
  nscoord GetUnpaginatedBSize();
  void    SetUnpaginatedBSize(nsPresContext* aPresContext, nscoord aValue);

  nscoord GetBStartBCBorderWidth() const { return mBStartBorderWidth; }
  nscoord GetBEndBCBorderWidth() const { return mBEndBorderWidth; }
  void SetBStartBCBorderWidth(BCPixelSize aWidth) { mBStartBorderWidth = aWidth; }
  void SetBEndBCBorderWidth(BCPixelSize aWidth) { mBEndBorderWidth = aWidth; }
  mozilla::LogicalMargin GetBCBorderWidth(mozilla::WritingMode aWM);
                             
  /**
   * Gets inner border widths before collapsing with cell borders
   * Caller must get block-end border from next row or from table
   * GetContinuousBCBorderWidth will not overwrite that border
   * see nsTablePainter about continuous borders
   */
  void GetContinuousBCBorderWidth(mozilla::WritingMode aWM,
                                  mozilla::LogicalMargin& aBorder);

  /**
   * @returns outer block-start bc border == prev row's block-end inner
   */
  nscoord GetOuterBStartContBCBorderWidth();
  /**
   * Sets full border widths before collapsing with cell borders
   * @param aForSide - side to set; only accepts iend, istart, and bstart
   */
  void SetContinuousBCBorderWidth(mozilla::LogicalSide aForSide,
                                  BCPixelSize aPixelValue);

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsContainerFrame::IsFrameOfType(aFlags & ~(nsIFrame::eTablePart));
  }

  virtual void InvalidateFrame(uint32_t aDisplayItemKey = 0) override;
  virtual void InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey = 0) override;
  virtual void InvalidateFrameForRemoval() override { InvalidateFrameSubtree(); }

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

protected:

  /** protected constructor.
    * @see NewFrame
    */
  explicit nsTableRowFrame(nsStyleContext* aContext, ClassID aID = kClassID);

  void InitChildReflowInput(nsPresContext&              aPresContext,
                            const mozilla::LogicalSize& aAvailSize,
                            bool                        aBorderCollapse,
                            TableCellReflowInput&     aReflowInput);
  
  virtual LogicalSides GetLogicalSkipSides(const ReflowInput* aReflowInput = nullptr) const override;

  // row-specific methods

  nscoord ComputeCellXOffset(const ReflowInput& aState,
                             nsIFrame*                aKidFrame,
                             const nsMargin&          aKidMargin) const;
  /**
   * Called for incremental/dirty and resize reflows. If aDirtyOnly is true then
   * only reflow dirty cells.
   */
  void ReflowChildren(nsPresContext*           aPresContext,
                      ReflowOutput&     aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsTableFrame&            aTableFrame,
                      nsReflowStatus&          aStatus);

private:
  struct RowBits {
    unsigned mRowIndex:29;
    unsigned mHasFixedBSize:1; // set if the dominating style bsize on the row or any cell is pixel based
    unsigned mHasPctBSize:1;   // set if the dominating style bsize on the row or any cell is pct based
    unsigned mFirstInserted:1; // if true, then it was the bstart-most newly inserted row 
  } mBits;

  // the desired bsize based on the content of the tallest cell in the row
  nscoord mContentBSize;
  // the bsize based on a style percentage bsize on either the row or any cell
  // if mHasPctBSize is set
  nscoord mStylePctBSize;
  // the bsize based on a style pixel bsize on the row or any
  // cell if mHasFixedBSize is set
  nscoord mStyleFixedBSize;

  // max-ascent and max-descent amongst all cells that have 'vertical-align: baseline'
  nscoord mMaxCellAscent;  // does include cells with rowspan > 1
  nscoord mMaxCellDescent; // does *not* include cells with rowspan > 1

  // border widths in pixels in the collapsing border model of the *inner*
  // half of the border only
  BCPixelSize mBStartBorderWidth;
  BCPixelSize mBEndBorderWidth;
  BCPixelSize mIEndContBorderWidth;
  BCPixelSize mBStartContBorderWidth;
  BCPixelSize mIStartContBorderWidth;

  /**
   * Sets the NS_ROW_HAS_CELL_WITH_STYLE_BSIZE bit to indicate whether
   * this row has any cells that have non-auto-bsize.  (Row-spanning
   * cells are ignored.)
   */
  void InitHasCellWithStyleBSize(nsTableFrame* aTableFrame);

};

inline int32_t
nsTableRowFrame::GetAdjustmentForStoredIndex(int32_t aStoredIndex) const
{
  nsTableRowGroupFrame* parentFrame = GetTableRowGroupFrame();
  return parentFrame->GetAdjustmentForStoredIndex(aStoredIndex);
}

inline void nsTableRowFrame::AddDeletedRowIndex()
{
  nsTableRowGroupFrame* parentFrame = GetTableRowGroupFrame();
  parentFrame->AddDeletedRowIndex(int32_t(mBits.mRowIndex));
}

inline int32_t nsTableRowFrame::GetRowIndex() const
{
  int32_t storedRowIndex = int32_t(mBits.mRowIndex);
  int32_t rowIndexAdjustment = GetAdjustmentForStoredIndex(storedRowIndex);
  return (storedRowIndex - rowIndexAdjustment);
}

inline void nsTableRowFrame::SetRowIndex (int aRowIndex)
{
  // Note: Setting the index of a row (as in the case of adding new rows) should
  // be preceded by a call to nsTableFrame::RecalculateRowIndices()
  // so as to correctly clear mDeletedRowIndexRanges.
  MOZ_ASSERT(GetTableRowGroupFrame()->
               GetTableFrame()->IsDeletedRowIndexRangesEmpty(),
             "mDeletedRowIndexRanges should be empty here!");
  mBits.mRowIndex = aRowIndex;
}

inline bool nsTableRowFrame::IsFirstInserted() const
{
  return bool(mBits.mFirstInserted);
}

inline void nsTableRowFrame::SetFirstInserted(bool aValue)
{
  mBits.mFirstInserted = aValue;
}

inline bool nsTableRowFrame::HasStyleBSize() const
{
  return (bool)mBits.mHasFixedBSize || (bool)mBits.mHasPctBSize;
}

inline bool nsTableRowFrame::HasFixedBSize() const
{
  return (bool)mBits.mHasFixedBSize;
}

inline void nsTableRowFrame::SetHasFixedBSize(bool aValue)
{
  mBits.mHasFixedBSize = aValue;
}

inline bool nsTableRowFrame::HasPctBSize() const
{
  return (bool)mBits.mHasPctBSize;
}

inline void nsTableRowFrame::SetHasPctBSize(bool aValue)
{
  mBits.mHasPctBSize = aValue;
}

inline nscoord nsTableRowFrame::GetContentBSize() const
{
  return mContentBSize;
}

inline void nsTableRowFrame::SetContentBSize(nscoord aValue)
{
  mContentBSize = aValue;
}

inline nscoord nsTableRowFrame::GetFixedBSize() const
{
  if (mBits.mHasFixedBSize) {
    return mStyleFixedBSize;
  }
  return 0;
}

inline float nsTableRowFrame::GetPctBSize() const
{
  if (mBits.mHasPctBSize) {
    return (float)mStylePctBSize / 100.0f;
  }
  return 0.0f;
}

inline bool nsTableRowFrame::HasUnpaginatedBSize()
{
  return HasAnyStateBits(NS_TABLE_ROW_HAS_UNPAGINATED_BSIZE);
}

inline void nsTableRowFrame::SetHasUnpaginatedBSize(bool aValue)
{
  if (aValue) {
    AddStateBits(NS_TABLE_ROW_HAS_UNPAGINATED_BSIZE);
  } else {
    RemoveStateBits(NS_TABLE_ROW_HAS_UNPAGINATED_BSIZE);
  }
}

inline mozilla::LogicalMargin
nsTableRowFrame::GetBCBorderWidth(mozilla::WritingMode aWM)
{
  return mozilla::LogicalMargin(
    aWM, nsPresContext::CSSPixelsToAppUnits(mBStartBorderWidth), 0,
    nsPresContext::CSSPixelsToAppUnits(mBEndBorderWidth), 0);
}

inline void
nsTableRowFrame::GetContinuousBCBorderWidth(mozilla::WritingMode aWM,
                                            mozilla::LogicalMargin& aBorder)
{
  int32_t aPixelsToTwips = nsPresContext::AppUnitsPerCSSPixel();
  aBorder.IEnd(aWM) = BC_BORDER_START_HALF_COORD(aPixelsToTwips,
                                                 mIStartContBorderWidth);
  aBorder.BStart(aWM) = BC_BORDER_END_HALF_COORD(aPixelsToTwips,
                                                 mBStartContBorderWidth);
  aBorder.IStart(aWM) = BC_BORDER_END_HALF_COORD(aPixelsToTwips,
                                                 mIEndContBorderWidth);
}

inline nscoord nsTableRowFrame::GetOuterBStartContBCBorderWidth()
{
  int32_t aPixelsToTwips = nsPresContext::AppUnitsPerCSSPixel();
  return BC_BORDER_START_HALF_COORD(aPixelsToTwips, mBStartContBorderWidth);
}

#endif
