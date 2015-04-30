/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTableCellFrame_h__
#define nsTableCellFrame_h__

#include "mozilla/Attributes.h"
#include "celldata.h"
#include "imgIContainer.h"
#include "nsITableCellLayout.h"
#include "nscore.h"
#include "nsContainerFrame.h"
#include "nsStyleContext.h"
#include "nsIPercentHeightObserver.h"
#include "nsGkAtoms.h"
#include "nsLayoutUtils.h"
#include "nsTArray.h"
#include "nsTableRowFrame.h"

/**
 * nsTableCellFrame
 * data structure to maintain information about a single table cell's frame
 *
 * NOTE:  frames are not ref counted.  We expose addref and release here
 * so we can change that decsion in the future.  Users of nsITableCellLayout
 * should refcount correctly as if this object is being ref counted, though
 * no actual support is under the hood.
 *
 * @author  sclark
 */
class nsTableCellFrame : public nsContainerFrame,
                         public nsITableCellLayout,
                         public nsIPercentHeightObserver
{
  typedef mozilla::image::DrawResult DrawResult;

public:
  NS_DECL_QUERYFRAME_TARGET(nsTableCellFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // default constructor supplied by the compiler

  nsTableCellFrame(nsStyleContext* aContext, nsTableFrame* aTableFrame);
  ~nsTableCellFrame();

  nsTableRowFrame* GetTableRowFrame() const
  {
    nsIFrame* parent = GetParent();
    MOZ_ASSERT(parent && parent->GetType() == nsGkAtoms::tableRowFrame);
    return static_cast<nsTableRowFrame*>(parent);
  }

  nsTableFrame* GetTableFrame() const
  {
    return GetTableRowFrame()->GetTableFrame();
  }

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

  virtual nsresult  AttributeChanged(int32_t         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     int32_t         aModType) override;

  /** @see nsIFrame::DidSetStyleContext */
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) override;

#ifdef DEBUG
  // Our anonymous block frame is the content insertion frame so these
  // methods should never be called:
  virtual void AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList) override;
  virtual void InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList) override;
  virtual void RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame) override;
#endif

  virtual nsContainerFrame* GetContentInsertionFrame() override {
    return GetFirstPrincipalChild()->GetContentInsertionFrame();
  }

  virtual nsMargin GetUsedMargin() const override;

  virtual void NotifyPercentHeight(const nsHTMLReflowState& aReflowState) override;

  virtual bool NeedsToObserve(const nsHTMLReflowState& aReflowState) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  DrawResult PaintCellBackground(nsRenderingContext& aRenderingContext,
                                 const nsRect& aDirtyRect, nsPoint aPt,
                                 uint32_t aFlags);

 
  virtual nsresult ProcessBorders(nsTableFrame* aFrame,
                                  nsDisplayListBuilder* aBuilder,
                                  const nsDisplayListSet& aLists);

  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) override;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) override;
  virtual IntrinsicISizeOffsetData
    IntrinsicISizeOffsets(nsRenderingContext* aRenderingContext) override;

  virtual void Reflow(nsPresContext*      aPresContext,
                      nsHTMLReflowMetrics& aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&      aStatus) override;

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::tableCellFrame
   */
  virtual nsIAtom* GetType() const override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  // Although the spec doesn't say that writing-mode is not applied to
  // table-cells, we still override this method here because we want to
  // make effective writing mode of table structure frames consistent
  // within a table. The content inside table cells is reflowed by an
  // anonymous block, hence their writing mode is not affected.
  virtual mozilla::WritingMode GetWritingMode() const override
    { return GetTableFrame()->GetWritingMode(); }

  void VerticallyAlignChild(nscoord aMaxAscent);

  /*
   * Get the value of vertical-align adjusted for CSS 2's rules for a
   * table cell, which means the result is always
   * NS_STYLE_VERTICAL_ALIGN_{TOP,MIDDLE,BOTTOM,BASELINE}.
   */
  virtual uint8_t GetVerticalAlign() const;

  bool HasVerticalAlignBaseline() const {
    return GetVerticalAlign() == NS_STYLE_VERTICAL_ALIGN_BASELINE;
  }

  bool CellHasVisibleContent(nscoord       height,
                               nsTableFrame* tableFrame,
                               nsIFrame*     kidFrame);

  /**
   * Get the first-line baseline of the cell relative to its top border
   * edge, as if the cell were vertically aligned to the top of the row.
   */
  nscoord GetCellBaseline() const;

  /**
   * return the cell's specified row span. this is what was specified in the
   * content model or in the style info, and is always >= 1.
   * to get the effective row span (the actual value that applies), use GetEffectiveRowSpan()
   * @see nsTableFrame::GetEffectiveRowSpan()
   */
  virtual int32_t GetRowSpan();

  // there is no set row index because row index depends on the cell's parent row only

  /*---------------- nsITableCellLayout methods ------------------------*/

  /**
   * return the cell's starting row index (starting at 0 for the first row).
   * for continued cell frames the row index is that of the cell's first-in-flow
   * and the column index (starting at 0 for the first column
   */
  NS_IMETHOD GetCellIndexes(int32_t &aRowIndex, int32_t &aColIndex) override;

  /** return the mapped cell's row index (starting at 0 for the first row) */
  virtual nsresult GetRowIndex(int32_t &aRowIndex) const override;

  /**
   * return the cell's specified col span. this is what was specified in the
   * content model or in the style info, and is always >= 1.
   * to get the effective col span (the actual value that applies), use GetEffectiveColSpan()
   * @see nsTableFrame::GetEffectiveColSpan()
   */
  virtual int32_t GetColSpan();

  /** return the cell's column index (starting at 0 for the first column) */
  virtual nsresult GetColIndex(int32_t &aColIndex) const override;
  void SetColIndex(int32_t aColIndex);

  /** return the available width given to this frame during its last reflow */
  inline nscoord GetPriorAvailWidth();

  /** set the available width given to this frame during its last reflow */
  inline void SetPriorAvailWidth(nscoord aPriorAvailWidth);

  /** return the desired size returned by this frame during its last reflow */
  inline mozilla::LogicalSize GetDesiredSize();

  /** set the desired size returned by this frame during its last reflow */
  inline void SetDesiredSize(const nsHTMLReflowMetrics & aDesiredSize);

  bool GetContentEmpty();
  void SetContentEmpty(bool aContentEmpty);

  bool HasPctOverHeight();
  void SetHasPctOverHeight(bool aValue);

  nsTableCellFrame* GetNextCell() const;

  virtual nsMargin* GetBorderWidth(nsMargin& aBorder) const;

  virtual DrawResult PaintBackground(nsRenderingContext& aRenderingContext,
                                     const nsRect&        aDirtyRect,
                                     nsPoint              aPt,
                                     uint32_t             aFlags);

  void DecorateForSelection(nsRenderingContext& aRenderingContext,
                            nsPoint              aPt);

  virtual bool UpdateOverflow() override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsContainerFrame::IsFrameOfType(aFlags & ~(nsIFrame::eTablePart));
  }
  
  virtual void InvalidateFrame(uint32_t aDisplayItemKey = 0) override;
  virtual void InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey = 0) override;
  virtual void InvalidateFrameForRemoval() override { InvalidateFrameSubtree(); }

protected:
  virtual LogicalSides GetLogicalSkipSides(const nsHTMLReflowState* aReflowState= nullptr) const override;

  /**
   * GetBorderOverflow says how far the cell's own borders extend
   * outside its own bounds.  In the separated borders model this should
   * just be zero (as it is for most frames), but in the collapsed
   * borders model (for which nsBCTableCellFrame overrides this virtual
   * method), it considers the extents of the collapsed border.
   */
  virtual nsMargin GetBorderOverflow();

  friend class nsTableRowFrame;

  uint32_t     mColIndex;             // the starting column for this cell

  nscoord      mPriorAvailWidth;      // the avail width during the last reflow
  mozilla::LogicalSize mDesiredSize;  // the last desired inline and block size
};

inline nscoord nsTableCellFrame::GetPriorAvailWidth()
{ return mPriorAvailWidth;}

inline void nsTableCellFrame::SetPriorAvailWidth(nscoord aPriorAvailWidth)
{ mPriorAvailWidth = aPriorAvailWidth;}

inline mozilla::LogicalSize nsTableCellFrame::GetDesiredSize()
{ return mDesiredSize; }

inline void nsTableCellFrame::SetDesiredSize(const nsHTMLReflowMetrics & aDesiredSize)
{
  mozilla::WritingMode wm = aDesiredSize.GetWritingMode();
  mDesiredSize = aDesiredSize.Size(wm).ConvertTo(GetWritingMode(), wm);
}

inline bool nsTableCellFrame::GetContentEmpty()
{
  return (mState & NS_TABLE_CELL_CONTENT_EMPTY) ==
         NS_TABLE_CELL_CONTENT_EMPTY;
}

inline void nsTableCellFrame::SetContentEmpty(bool aContentEmpty)
{
  if (aContentEmpty) {
    mState |= NS_TABLE_CELL_CONTENT_EMPTY;
  } else {
    mState &= ~NS_TABLE_CELL_CONTENT_EMPTY;
  }
}

inline bool nsTableCellFrame::HasPctOverHeight()
{
  return (mState & NS_TABLE_CELL_HAS_PCT_OVER_HEIGHT) ==
         NS_TABLE_CELL_HAS_PCT_OVER_HEIGHT;
}

inline void nsTableCellFrame::SetHasPctOverHeight(bool aValue)
{
  if (aValue) {
    mState |= NS_TABLE_CELL_HAS_PCT_OVER_HEIGHT;
  } else {
    mState &= ~NS_TABLE_CELL_HAS_PCT_OVER_HEIGHT;
  }
}

// nsBCTableCellFrame
class nsBCTableCellFrame final : public nsTableCellFrame
{
  typedef mozilla::image::DrawResult DrawResult;
public:
  NS_DECL_FRAMEARENA_HELPERS

  nsBCTableCellFrame(nsStyleContext* aContext, nsTableFrame* aTableFrame);

  ~nsBCTableCellFrame();

  virtual nsIAtom* GetType() const override;

  virtual nsMargin GetUsedBorder() const override;
  virtual bool GetBorderRadii(const nsSize& aFrameSize,
                              const nsSize& aBorderArea,
                              Sides aSkipSides,
                              nscoord aRadii[8]) const override;

  // Get the *inner half of the border only*, in twips.
  virtual nsMargin* GetBorderWidth(nsMargin& aBorder) const override;

  // Get the *inner half of the border only*, in pixels.
  BCPixelSize GetBorderWidth(mozilla::css::Side aSide) const;

  // Set the full (both halves) width of the border
  void SetBorderWidth(mozilla::css::Side aSide, BCPixelSize aPixelValue);

  virtual nsMargin GetBorderOverflow() override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  virtual DrawResult PaintBackground(nsRenderingContext& aRenderingContext,
                                     const nsRect&        aDirtyRect,
                                     nsPoint              aPt,
                                     uint32_t             aFlags) override;

private:

  // These are the entire width of the border (the cell edge contains only
  // the inner half, per the macros in nsTablePainter.h).
  BCPixelSize mTopBorder;
  BCPixelSize mRightBorder;
  BCPixelSize mBottomBorder;
  BCPixelSize mLeftBorder;
};

#endif
