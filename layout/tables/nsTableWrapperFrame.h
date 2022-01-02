/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTableWrapperFrame_h__
#define nsTableWrapperFrame_h__

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "nscore.h"
#include "nsContainerFrame.h"
#include "nsCellMap.h"
#include "nsTableFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

/**
 * Primary frame for a table element,
 * the nsTableWrapperFrame contains 0 or one caption frame, and a nsTableFrame
 * pseudo-frame (referred to as the "inner frame').
 */
class nsTableWrapperFrame : public nsContainerFrame {
 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsTableWrapperFrame)

  /** instantiate a new instance of nsTableRowFrame.
   * @param aPresShell the pres shell for this frame
   *
   * @return           the frame that was created
   */
  friend nsTableWrapperFrame* NS_NewTableWrapperFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

  // nsIFrame overrides - see there for a description

  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;

  virtual const nsFrameList& GetChildList(ChildListID aListID) const override;
  virtual void GetChildLists(nsTArray<ChildList>* aLists) const override;

  virtual void SetInitialChildList(ChildListID aListID,
                                   nsFrameList& aChildList) override;
  virtual void AppendFrames(ChildListID aListID,
                            nsFrameList& aFrameList) override;
  virtual void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                            const nsLineList::iterator* aPrevFrameLine,
                            nsFrameList& aFrameList) override;
  virtual void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) override;

  virtual nsContainerFrame* GetContentInsertionFrame() override {
    return PrincipalChildList().FirstChild()->GetContentInsertionFrame();
  }

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  void BuildDisplayListForInnerTable(nsDisplayListBuilder* aBuilder,
                                     const nsDisplayListSet& aLists);

  virtual nscoord GetLogicalBaseline(
      mozilla::WritingMode aWritingMode) const override;

  bool GetNaturalBaselineBOffset(mozilla::WritingMode aWM,
                                 BaselineSharingGroup aBaselineGroup,
                                 nscoord* aBaseline) const override {
    if (StyleDisplay()->IsContainLayout()) {
      return false;
    }
    auto innerTable = InnerTableFrame();
    nscoord offset;
    if (innerTable->GetNaturalBaselineBOffset(aWM, aBaselineGroup, &offset)) {
      auto bStart = innerTable->BStart(aWM, mRect.Size());
      if (aBaselineGroup == BaselineSharingGroup::First) {
        *aBaseline = offset + bStart;
      } else {
        auto bEnd = bStart + innerTable->BSize(aWM);
        *aBaseline = BSize(aWM) - (bEnd - offset);
      }
      return true;
    }
    return false;
  }

  virtual nscoord GetMinISize(gfxContext* aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  SizeComputationResult ComputeSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWM,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin,
      const mozilla::LogicalSize& aBorderPadding,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      mozilla::ComputeSizeFlags aFlags) override;

  mozilla::LogicalSize ComputeAutoSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWM,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin,
      const mozilla::LogicalSize& aBorderPadding,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      mozilla::ComputeSizeFlags aFlags) override;

  /** process a reflow command for the table.
   * This involves reflowing the caption and the inner table.
   * @see nsIFrame::Reflow */
  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  virtual ComputedStyle* GetParentComputedStyle(
      nsIFrame** aProviderFrame) const override;

  /**
   * Return the content for the cell at the given row and column.
   */
  nsIContent* GetCellAt(uint32_t aRowIdx, uint32_t aColIdx) const;

  /**
   * Return the number of rows in the table.
   */
  int32_t GetRowCount() const { return InnerTableFrame()->GetRowCount(); }

  /**
   * Return the number of columns in the table.
   */
  int32_t GetColCount() const { return InnerTableFrame()->GetColCount(); }

  /**
   * Return the index of the cell at the given row and column.
   */
  int32_t GetIndexByRowAndColumn(int32_t aRowIdx, int32_t aColIdx) const {
    nsTableCellMap* cellMap = InnerTableFrame()->GetCellMap();
    if (!cellMap) return -1;

    return cellMap->GetIndexByRowAndColumn(aRowIdx, aColIdx);
  }

  /**
   * Get the row and column indices for the cell at the given index.
   */
  void GetRowAndColumnByIndex(int32_t aCellIdx, int32_t* aRowIdx,
                              int32_t* aColIdx) const {
    *aRowIdx = *aColIdx = 0;
    nsTableCellMap* cellMap = InnerTableFrame()->GetCellMap();
    if (cellMap) {
      cellMap->GetRowAndColumnByIndex(aCellIdx, aRowIdx, aColIdx);
    }
  }

  /**
   * return the frame for the cell at the given row and column.
   */
  nsTableCellFrame* GetCellFrameAt(uint32_t aRowIdx, uint32_t aColIdx) const {
    nsTableCellMap* map = InnerTableFrame()->GetCellMap();
    if (!map) {
      return nullptr;
    }

    return map->GetCellInfoAt(aRowIdx, aColIdx);
  }

  /**
   * Return the col span of the cell at the given row and column indices.
   */
  uint32_t GetEffectiveColSpanAt(uint32_t aRowIdx, uint32_t aColIdx) const {
    nsTableCellMap* map = InnerTableFrame()->GetCellMap();
    return map->GetEffectiveColSpan(aRowIdx, aColIdx);
  }

  /**
   * Return the effective row span of the cell at the given row and column.
   */
  uint32_t GetEffectiveRowSpanAt(uint32_t aRowIdx, uint32_t aColIdx) const {
    nsTableCellMap* map = InnerTableFrame()->GetCellMap();
    return map->GetEffectiveRowSpan(aRowIdx, aColIdx);
  }

 protected:
  explicit nsTableWrapperFrame(ComputedStyle* aStyle,
                               nsPresContext* aPresContext,
                               ClassID aID = kClassID);
  virtual ~nsTableWrapperFrame();

  using MaybeCaptionSide = Maybe<mozilla::StyleCaptionSide>;

  // Get a StyleCaptionSide value, or Nothing if no caption is present.
  //
  // (Remember that caption-side values are interpreted logically, despite
  // having "physical" names.)
  MaybeCaptionSide GetCaptionSide() const;

  bool HasSideCaption() const {
    auto captionSide = GetCaptionSide();
    return captionSide && IsSideCaption(*captionSide);
  }

  static bool IsSideCaption(const mozilla::StyleCaptionSide aCaptionSide) {
    return aCaptionSide == mozilla::StyleCaptionSide::Left ||
           aCaptionSide == mozilla::StyleCaptionSide::Right;
  }

  mozilla::StyleVerticalAlignKeyword GetCaptionVerticalAlign() const;

  nscoord ComputeFinalBSize(const MaybeCaptionSide&,
                            const mozilla::LogicalSize& aInnerSize,
                            const mozilla::LogicalSize& aCaptionSize,
                            const mozilla::LogicalMargin& aCaptionMargin,
                            const mozilla::WritingMode aWM) const;

  nsresult GetCaptionOrigin(mozilla::StyleCaptionSide,
                            const mozilla::LogicalSize& aContainBlockSize,
                            const mozilla::LogicalSize& aInnerSize,
                            const mozilla::LogicalSize& aCaptionSize,
                            mozilla::LogicalMargin& aCaptionMargin,
                            mozilla::LogicalPoint& aOrigin,
                            mozilla::WritingMode aWM);

  nsresult GetInnerOrigin(const MaybeCaptionSide&,
                          const mozilla::LogicalSize& aContainBlockSize,
                          const mozilla::LogicalSize& aCaptionSize,
                          const mozilla::LogicalMargin& aCaptionMargin,
                          const mozilla::LogicalSize& aInnerSize,
                          mozilla::LogicalPoint& aOrigin,
                          mozilla::WritingMode aWM);

  // Returns the area occupied by the caption within our content box depending
  // on the caption side.
  //
  // @param aCaptionMarginBoxSize the caption's margin-box size in our
  //        writing-mode.
  mozilla::LogicalSize GetAreaOccupiedByCaption(
      mozilla::StyleCaptionSide,
      const mozilla::LogicalSize& aCaptionMarginBoxSize) const;

  // Create and init the child reflow input, using passed-in aChildRI, so that
  // caller can use it after we return.
  //
  // @param aAreaOccupiedByCaption the value computed by
  //        GetAreaOccupiedByCaption() if we have a caption.
  void CreateReflowInputForInnerTable(
      nsPresContext* aPresContext, nsTableFrame* aTableFrame,
      const ReflowInput& aOuterRI, Maybe<ReflowInput>& aChildRI,
      const nscoord aAvailISize,
      const mozilla::Maybe<mozilla::LogicalSize>& aAreaOccupiedByCaption =
          mozilla::Nothing()) const;
  void CreateReflowInputForCaption(nsPresContext* aPresContext,
                                   nsIFrame* aCaptionFrame,
                                   const ReflowInput& aOuterRI,
                                   Maybe<ReflowInput>& aChildRI,
                                   const nscoord aAvailISize) const;

  // Reflow the child (caption or inner table frame).
  void ReflowChild(nsPresContext* aPresContext, nsIFrame* aChildFrame,
                   const ReflowInput& aChildRI, ReflowOutput& aMetrics,
                   nsReflowStatus& aStatus);

  // Set the overflow areas in our reflow metrics
  void UpdateOverflowAreas(ReflowOutput& aMet);

  virtual bool IsFrameOfType(uint32_t aFlags) const override {
    return nsContainerFrame::IsFrameOfType(aFlags &
                                           (~eCanContainOverflowContainers));
  }

  nsTableFrame* InnerTableFrame() const {
    return static_cast<nsTableFrame*>(mFrames.FirstChild());
  }

  /**
   * Helper for ComputeAutoSize.
   * Compute the margin-box inline size of the frame given the inputs.
   *
   * Note: CaptionShrinkWrapISize doesn't need StyleSizeOverrides parameter.
   */
  mozilla::LogicalSize InnerTableShrinkWrapSize(
      gfxContext* aRenderingContext, nsTableFrame* aTableFrame,
      mozilla::WritingMode aWM, const mozilla::LogicalSize& aCBSize,
      nscoord aAvailableISize,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      mozilla::ComputeSizeFlags aFlag) const;
  mozilla::LogicalSize CaptionShrinkWrapSize(
      gfxContext* aRenderingContext, nsIFrame* aCaptionFrame,
      mozilla::WritingMode aWM, const mozilla::LogicalSize& aCBSize,
      nscoord aAvailableISize, mozilla::ComputeSizeFlags aFlag) const;

  /**
   * Create a new StyleSize by reducing the size by aAmountToReduce.
   *
   * @param aStyleSize must be a Length.
   */
  mozilla::StyleSize ReduceStyleSizeBy(const mozilla::StyleSize& aStyleSize,
                                       const nscoord aAmountToReduce) const;

  /**
   * Compute StyleSizeOverrides for inner table frame given the overrides of the
   * table wrapper frame.
   */
  mozilla::StyleSizeOverrides ComputeSizeOverridesForInnerTable(
      const nsTableFrame* aTableFrame,
      const mozilla::StyleSizeOverrides& aWrapperSizeOverrides,
      const mozilla::LogicalSize& aBorderPadding,
      const mozilla::LogicalSize& aAreaOccupiedByCaption) const;

 private:
  nsFrameList mCaptionFrames;
};

#endif
