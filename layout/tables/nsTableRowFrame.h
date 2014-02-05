/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTableRowFrame_h__
#define nsTableRowFrame_h__

#include "mozilla/Attributes.h"
#include "nscore.h"
#include "nsContainerFrame.h"
#include "nsTablePainter.h"

class  nsTableFrame;
class  nsTableCellFrame;
struct nsTableCellReflowState;

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
public:
  NS_DECL_QUERYFRAME_TARGET(nsTableRowFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  virtual ~nsTableRowFrame();

  virtual void Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        aPrevInFlow) MOZ_OVERRIDE;
  /** @see nsIFrame::DidSetStyleContext */
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) MOZ_OVERRIDE;
  
  NS_IMETHOD AppendFrames(ChildListID     aListID,
                          nsFrameList&    aFrameList) MOZ_OVERRIDE;
  NS_IMETHOD InsertFrames(ChildListID     aListID,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList) MOZ_OVERRIDE;
  NS_IMETHOD RemoveFrame(ChildListID     aListID,
                         nsIFrame*       aOldFrame) MOZ_OVERRIDE;

  /** instantiate a new instance of nsTableRowFrame.
    * @param aPresShell the pres shell for this frame
    *
    * @return           the frame that was created
    */
  friend nsIFrame* NS_NewTableRowFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  virtual nsMargin GetUsedMargin() const MOZ_OVERRIDE;
  virtual nsMargin GetUsedBorder() const MOZ_OVERRIDE;
  virtual nsMargin GetUsedPadding() const MOZ_OVERRIDE;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

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
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  void DidResize();

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::tableRowFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

#ifdef DEBUG_FRAME_DUMP
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif
 
  void UpdateHeight(nscoord           aHeight,
                    nscoord           aAscent,
                    nscoord           aDescent,
                    nsTableFrame*     aTableFrame = nullptr,
                    nsTableCellFrame* aCellFrame  = nullptr);

  void ResetHeight(nscoord aRowStyleHeight);

  // calculate the height, considering content height of the 
  // cells and the style height of the row and cells, excluding pct heights
  nscoord CalcHeight(const nsHTMLReflowState& aReflowState);

  // Support for cells with 'vertical-align: baseline'.

  /** 
   * returns the max-ascent amongst all the cells that have 
   * 'vertical-align: baseline', *including* cells with rowspans.
   * returns 0 if we don't have any cell with 'vertical-align: baseline'
   */
  nscoord GetMaxCellAscent() const;

  /* return the row ascent
   */
  nscoord GetRowBaseline();
 
  /** returns the ordinal position of this row in its table */
  virtual int32_t GetRowIndex() const;

  /** set this row's starting row index */
  void SetRowIndex (int aRowIndex);

  /** used by row group frame code */
  nscoord ReflowCellFrame(nsPresContext*          aPresContext,
                          const nsHTMLReflowState& aReflowState,
                          bool                     aIsTopOfPage,
                          nsTableCellFrame*        aCellFrame,
                          nscoord                  aAvailableHeight,
                          nsReflowStatus&          aStatus);
  /**
    * Collapse the row if required, apply col and colgroup visibility: collapse
    * info to the cells in the row.
    * @return he amount to shift up all following rows
    * @param aRowOffset     - shift the row up by this amount
    * @param aWidth         - new width of the row
    * @param aCollapseGroup - parent rowgroup is collapsed so this row needs
    *                         to be collapsed
    * @param aDidCollapse   - the row has been collapsed
    */
  nscoord CollapseRowIfNecessary(nscoord aRowOffset,
                                 nscoord aWidth,
                                 bool    aCollapseGroup,
                                 bool& aDidCollapse);

  /**
   * Insert a cell frame after the last cell frame that has a col index
   * that is less than aColIndex.  If no such cell frame is found the
   * frame to insert is prepended to the child list.
   * @param aFrame the cell frame to insert
   * @param aColIndex the col index
   */
  void InsertCellFrame(nsTableCellFrame* aFrame,
                       int32_t           aColIndex);

  nsresult CalculateCellActualHeight(nsTableCellFrame* aCellFrame,
                                     nscoord&          aDesiredHeight);

  bool IsFirstInserted() const;
  void   SetFirstInserted(bool aValue);

  nscoord GetContentHeight() const;
  void    SetContentHeight(nscoord aTwipValue);

  bool HasStyleHeight() const;

  bool HasFixedHeight() const;
  void   SetHasFixedHeight(bool aValue);

  bool HasPctHeight() const;
  void   SetHasPctHeight(bool aValue);

  nscoord GetFixedHeight() const;
  void    SetFixedHeight(nscoord aValue);

  float   GetPctHeight() const;
  void    SetPctHeight(float  aPctValue,
                       bool aForce = false);

  nscoord GetHeight(nscoord aBasis = 0) const;

  nsTableRowFrame* GetNextRow() const;

  bool    HasUnpaginatedHeight();
  void    SetHasUnpaginatedHeight(bool aValue);
  nscoord GetUnpaginatedHeight(nsPresContext* aPresContext);
  void    SetUnpaginatedHeight(nsPresContext* aPresContext, nscoord aValue);

  nscoord GetTopBCBorderWidth();
  void    SetTopBCBorderWidth(BCPixelSize aWidth);
  nscoord GetBottomBCBorderWidth();
  void    SetBottomBCBorderWidth(BCPixelSize aWidth);
  nsMargin* GetBCBorderWidth(nsMargin& aBorder);
                             
  /**
   * Gets inner border widths before collapsing with cell borders
   * Caller must get bottom border from next row or from table
   * GetContinuousBCBorderWidth will not overwrite aBorder.bottom
   * see nsTablePainter about continuous borders
   */
  void GetContinuousBCBorderWidth(nsMargin& aBorder);
  /**
   * @returns outer top bc border == prev row's bottom inner
   */
  nscoord GetOuterTopContBCBorderWidth();
  /**
   * Sets full border widths before collapsing with cell borders
   * @param aForSide - side to set; only accepts right, left, and top
   */
  void SetContinuousBCBorderWidth(uint8_t     aForSide,
                                  BCPixelSize aPixelValue);

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return nsContainerFrame::IsFrameOfType(aFlags & ~(nsIFrame::eTablePart));
  }

  virtual void InvalidateFrame(uint32_t aDisplayItemKey = 0) MOZ_OVERRIDE;
  virtual void InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey = 0) MOZ_OVERRIDE;
  virtual void InvalidateFrameForRemoval() MOZ_OVERRIDE { InvalidateFrameSubtree(); }

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
#endif

protected:

  /** protected constructor.
    * @see NewFrame
    */
  nsTableRowFrame(nsStyleContext *aContext);

  void InitChildReflowState(nsPresContext&         aPresContext,
                            const nsSize&           aAvailSize,
                            bool                    aBorderCollapse,
                            nsTableCellReflowState& aReflowState);
  
  virtual int GetSkipSides(const nsHTMLReflowState* aReflowState = nullptr) const MOZ_OVERRIDE;

  // row-specific methods

  nscoord ComputeCellXOffset(const nsHTMLReflowState& aState,
                             nsIFrame*                aKidFrame,
                             const nsMargin&          aKidMargin) const;
  /**
   * Called for incremental/dirty and resize reflows. If aDirtyOnly is true then
   * only reflow dirty cells.
   */
  nsresult ReflowChildren(nsPresContext*           aPresContext,
                          nsHTMLReflowMetrics&     aDesiredSize,
                          const nsHTMLReflowState& aReflowState,
                          nsTableFrame&            aTableFrame,
                          nsReflowStatus&          aStatus);

private:
  struct RowBits {
    unsigned mRowIndex:29;
    unsigned mHasFixedHeight:1; // set if the dominating style height on the row or any cell is pixel based
    unsigned mHasPctHeight:1;   // set if the dominating style height on the row or any cell is pct based
    unsigned mFirstInserted:1;  // if true, then it was the top most newly inserted row 
  } mBits;

  // the desired height based on the content of the tallest cell in the row
  nscoord mContentHeight;
  // the height based on a style percentage height on either the row or any cell
  // if mHasPctHeight is set
  nscoord mStylePctHeight;
  // the height based on a style pixel height on the row or any
  // cell if mHasFixedHeight is set
  nscoord mStyleFixedHeight;

  // max-ascent and max-descent amongst all cells that have 'vertical-align: baseline'
  nscoord mMaxCellAscent;  // does include cells with rowspan > 1
  nscoord mMaxCellDescent; // does *not* include cells with rowspan > 1

  // border widths in pixels in the collapsing border model of the *inner*
  // half of the border only
  BCPixelSize mTopBorderWidth;
  BCPixelSize mBottomBorderWidth;
  BCPixelSize mRightContBorderWidth;
  BCPixelSize mTopContBorderWidth;
  BCPixelSize mLeftContBorderWidth;

  /**
   * Sets the NS_ROW_HAS_CELL_WITH_STYLE_HEIGHT bit to indicate whether
   * this row has any cells that have non-auto-height.  (Row-spanning
   * cells are ignored.)
   */
  void InitHasCellWithStyleHeight(nsTableFrame* aTableFrame);

};

inline int32_t nsTableRowFrame::GetRowIndex() const
{
  return int32_t(mBits.mRowIndex);
}

inline void nsTableRowFrame::SetRowIndex (int aRowIndex)
{
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

inline bool nsTableRowFrame::HasStyleHeight() const
{
  return (bool)mBits.mHasFixedHeight || (bool)mBits.mHasPctHeight;
}

inline bool nsTableRowFrame::HasFixedHeight() const
{
  return (bool)mBits.mHasFixedHeight;
}

inline void nsTableRowFrame::SetHasFixedHeight(bool aValue)
{
  mBits.mHasFixedHeight = aValue;
}

inline bool nsTableRowFrame::HasPctHeight() const
{
  return (bool)mBits.mHasPctHeight;
}

inline void nsTableRowFrame::SetHasPctHeight(bool aValue)
{
  mBits.mHasPctHeight = aValue;
}

inline nscoord nsTableRowFrame::GetContentHeight() const
{
  return mContentHeight;
}

inline void nsTableRowFrame::SetContentHeight(nscoord aValue)
{
  mContentHeight = aValue;
}

inline nscoord nsTableRowFrame::GetFixedHeight() const
{
  if (mBits.mHasFixedHeight)
    return mStyleFixedHeight;
  else
    return 0;
}

inline float nsTableRowFrame::GetPctHeight() const
{
  if (mBits.mHasPctHeight) 
    return (float)mStylePctHeight / 100.0f;
  else
    return 0.0f;
}

inline bool nsTableRowFrame::HasUnpaginatedHeight()
{
  return (mState & NS_TABLE_ROW_HAS_UNPAGINATED_HEIGHT) ==
         NS_TABLE_ROW_HAS_UNPAGINATED_HEIGHT;
}

inline void nsTableRowFrame::SetHasUnpaginatedHeight(bool aValue)
{
  if (aValue) {
    mState |= NS_TABLE_ROW_HAS_UNPAGINATED_HEIGHT;
  } else {
    mState &= ~NS_TABLE_ROW_HAS_UNPAGINATED_HEIGHT;
  }
}

inline nscoord nsTableRowFrame::GetTopBCBorderWidth()
{
  return mTopBorderWidth;
}

inline void nsTableRowFrame::SetTopBCBorderWidth(BCPixelSize aWidth)
{
  mTopBorderWidth = aWidth;
}

inline nscoord nsTableRowFrame::GetBottomBCBorderWidth()
{
  return mBottomBorderWidth;
}

inline void nsTableRowFrame::SetBottomBCBorderWidth(BCPixelSize aWidth)
{
  mBottomBorderWidth = aWidth;
}

inline nsMargin* nsTableRowFrame::GetBCBorderWidth(nsMargin& aBorder)
{
  aBorder.left = aBorder.right = 0;

  aBorder.top    = nsPresContext::CSSPixelsToAppUnits(mTopBorderWidth);
  aBorder.bottom = nsPresContext::CSSPixelsToAppUnits(mBottomBorderWidth);

  return &aBorder;
}

inline void
nsTableRowFrame::GetContinuousBCBorderWidth(nsMargin& aBorder)
{
  int32_t aPixelsToTwips = nsPresContext::AppUnitsPerCSSPixel();
  aBorder.right = BC_BORDER_LEFT_HALF_COORD(aPixelsToTwips,
                                            mLeftContBorderWidth);
  aBorder.top = BC_BORDER_BOTTOM_HALF_COORD(aPixelsToTwips,
                                            mTopContBorderWidth);
  aBorder.left = BC_BORDER_RIGHT_HALF_COORD(aPixelsToTwips,
                                            mRightContBorderWidth);
}

inline nscoord nsTableRowFrame::GetOuterTopContBCBorderWidth()
{
  int32_t aPixelsToTwips = nsPresContext::AppUnitsPerCSSPixel();
  return BC_BORDER_TOP_HALF_COORD(aPixelsToTwips, mTopContBorderWidth);
}

#endif
