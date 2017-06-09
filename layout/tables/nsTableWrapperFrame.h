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

/**
 * Primary frame for a table element,
 * the nsTableWrapperFrame contains 0 or one caption frame, and a nsTableFrame
 * pseudo-frame (referred to as the "inner frame').
 */
class nsTableWrapperFrame : public nsContainerFrame
{
public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsTableWrapperFrame)

  /** instantiate a new instance of nsTableRowFrame.
    * @param aPresShell the pres shell for this frame
    *
    * @return           the frame that was created
    */
  friend nsTableWrapperFrame* NS_NewTableWrapperFrame(nsIPresShell* aPresShell,
                                                      nsStyleContext* aContext);

  // nsIFrame overrides - see there for a description

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual const nsFrameList& GetChildList(ChildListID aListID) const override;
  virtual void GetChildLists(nsTArray<ChildList>* aLists) const override;

  virtual void SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList) override;
  virtual void AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList) override;
  virtual void InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList) override;
  virtual void RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame) override;

  virtual nsContainerFrame* GetContentInsertionFrame() override {
    return PrincipalChildList().FirstChild()->GetContentInsertionFrame();
  }

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  void BuildDisplayListForInnerTable(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists);

  virtual nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const override;

  bool GetNaturalBaselineBOffset(mozilla::WritingMode aWM,
                                 BaselineSharingGroup aBaselineGroup,
                                 nscoord*             aBaseline) const override
  {
    auto innerTable = InnerTableFrame();
    nscoord offset;
    if (innerTable->GetNaturalBaselineBOffset(aWM, aBaselineGroup, &offset)) {
      auto bStart = innerTable->BStart(aWM, mRect.Size());
      if (aBaselineGroup == BaselineSharingGroup::eFirst) {
        *aBaseline = offset + bStart;
      } else {
        auto bEnd = bStart + innerTable->BSize(aWM);
        *aBaseline = BSize(aWM) - (bEnd - offset);
      }
      return true;
    }
    return false;
  }

  virtual nscoord GetMinISize(gfxContext *aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext *aRenderingContext) override;

  virtual mozilla::LogicalSize
  ComputeAutoSize(gfxContext*                 aRenderingContext,
                  mozilla::WritingMode        aWM,
                  const mozilla::LogicalSize& aCBSize,
                  nscoord                     aAvailableISize,
                  const mozilla::LogicalSize& aMargin,
                  const mozilla::LogicalSize& aBorder,
                  const mozilla::LogicalSize& aPadding,
                  ComputeSizeFlags            aFlags) override;

  /** process a reflow command for the table.
    * This involves reflowing the caption and the inner table.
    * @see nsIFrame::Reflow */
  virtual void Reflow(nsPresContext*           aPresContext,
                      ReflowOutput&     aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&          aStatus) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  virtual nsStyleContext* GetParentStyleContext(nsIFrame** aProviderFrame) const override;

  /**
   * Return the content for the cell at the given row and column.
   */
  nsIContent* GetCellAt(uint32_t aRowIdx, uint32_t aColIdx) const;

  /**
   * Return the number of rows in the table.
   */
  int32_t GetRowCount() const
  {
    return InnerTableFrame()->GetRowCount();
  }

  /**
   * Return the number of columns in the table.
   */
  int32_t GetColCount() const
  {
    return InnerTableFrame()->GetColCount();
  }

  /**
   * Return the index of the cell at the given row and column.
   */
  int32_t GetIndexByRowAndColumn(int32_t aRowIdx, int32_t aColIdx) const
  {
    nsTableCellMap* cellMap = InnerTableFrame()->GetCellMap();
    if (!cellMap)
      return -1;

    return cellMap->GetIndexByRowAndColumn(aRowIdx, aColIdx);
  }

  /**
   * Get the row and column indices for the cell at the given index.
   */
  void GetRowAndColumnByIndex(int32_t aCellIdx, int32_t* aRowIdx,
                              int32_t* aColIdx) const
  {
    *aRowIdx = *aColIdx = 0;
    nsTableCellMap* cellMap = InnerTableFrame()->GetCellMap();
    if (cellMap) {
      cellMap->GetRowAndColumnByIndex(aCellIdx, aRowIdx, aColIdx);
    }
  }

  /**
   * return the frame for the cell at the given row and column.
   */
  nsTableCellFrame* GetCellFrameAt(uint32_t aRowIdx, uint32_t aColIdx) const
  {
    nsTableCellMap* map = InnerTableFrame()->GetCellMap();
    if (!map) {
      return nullptr;
    }

    return map->GetCellInfoAt(aRowIdx, aColIdx);
  }

  /**
   * Return the col span of the cell at the given row and column indices.
   */
  uint32_t GetEffectiveColSpanAt(uint32_t aRowIdx, uint32_t aColIdx) const
  {
    nsTableCellMap* map = InnerTableFrame()->GetCellMap();
    return map->GetEffectiveColSpan(aRowIdx, aColIdx);
  }

  /**
   * Return the effective row span of the cell at the given row and column.
   */
  uint32_t GetEffectiveRowSpanAt(uint32_t aRowIdx, uint32_t aColIdx) const
  {
    nsTableCellMap* map = InnerTableFrame()->GetCellMap();
    return map->GetEffectiveRowSpan(aRowIdx, aColIdx);
  }

  /**
   * The CB size to use for the inner table frame if we're a grid item.
   */
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(GridItemCBSizeProperty, mozilla::LogicalSize);

protected:

  explicit nsTableWrapperFrame(nsStyleContext* aContext, ClassID aID = kClassID);
  virtual ~nsTableWrapperFrame();

  void InitChildReflowInput(nsPresContext&     aPresContext,
                            ReflowInput& aReflowInput);

  // Get a NS_STYLE_CAPTION_SIDE_* value, or NO_SIDE if no caption is present.
  // (Remember that caption-side values are interpreted logically, despite
  // having "physical" names.)
  uint8_t GetCaptionSide();

  bool HasSideCaption() {
    uint8_t captionSide = GetCaptionSide();
    return captionSide == NS_STYLE_CAPTION_SIDE_LEFT ||
           captionSide == NS_STYLE_CAPTION_SIDE_RIGHT;
  }
  
  uint8_t GetCaptionVerticalAlign();

  void SetDesiredSize(uint8_t                       aCaptionSide,
                      const mozilla::LogicalSize&   aInnerSize,
                      const mozilla::LogicalSize&   aCaptionSize,
                      const mozilla::LogicalMargin& aInnerMargin,
                      const mozilla::LogicalMargin& aCaptionMargin,
                      nscoord&                      aISize,
                      nscoord&                      aBSize,
                      mozilla::WritingMode          aWM);

  nsresult   GetCaptionOrigin(uint32_t         aCaptionSide,
                              const mozilla::LogicalSize&    aContainBlockSize,
                              const mozilla::LogicalSize&    aInnerSize, 
                              const mozilla::LogicalMargin&  aInnerMargin,
                              const mozilla::LogicalSize&    aCaptionSize,
                              mozilla::LogicalMargin&        aCaptionMargin,
                              mozilla::LogicalPoint&         aOrigin,
                              mozilla::WritingMode           aWM);

  nsresult   GetInnerOrigin(uint32_t         aCaptionSide,
                            const mozilla::LogicalSize&    aContainBlockSize,
                            const mozilla::LogicalSize&    aCaptionSize, 
                            const mozilla::LogicalMargin&  aCaptionMargin,
                            const mozilla::LogicalSize&    aInnerSize,
                            mozilla::LogicalMargin&        aInnerMargin,
                            mozilla::LogicalPoint&         aOrigin,
                            mozilla::WritingMode           aWM);
  
  // reflow the child (caption or innertable frame)
  void OuterBeginReflowChild(nsPresContext*                     aPresContext,
                             nsIFrame*                          aChildFrame,
                             const ReflowInput&           aOuterRI,
                             mozilla::Maybe<ReflowInput>& aChildRI,
                             nscoord                            aAvailISize);

  void OuterDoReflowChild(nsPresContext*           aPresContext,
                          nsIFrame*                aChildFrame,
                          const ReflowInput& aChildRI,
                          ReflowOutput&     aMetrics,
                          nsReflowStatus&          aStatus);

  // Set the overflow areas in our reflow metrics
  void UpdateOverflowAreas(ReflowOutput& aMet);

  // Get the margin.
  void GetChildMargin(nsPresContext*           aPresContext,
                      const ReflowInput& aOuterRI,
                      nsIFrame*                aChildFrame,
                      nscoord                  aAvailableWidth,
                      mozilla::LogicalMargin&  aMargin);

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
                                           (~eCanContainOverflowContainers));
  }

  nsTableFrame* InnerTableFrame() const
  {
    return static_cast<nsTableFrame*>(mFrames.FirstChild());
  }

  /**
   * Helper for ComputeAutoSize.
   * Compute the margin-box inline size of aChildFrame given the inputs.
   * If aMarginResult is non-null, fill it with the part of the
   * margin-isize that was contributed by the margin.
   */
  nscoord ChildShrinkWrapISize(gfxContext*          aRenderingContext,
                               nsIFrame*            aChildFrame,
                               mozilla::WritingMode aWM,
                               mozilla::LogicalSize aCBSize,
                               nscoord              aAvailableISize,
                               nscoord*             aMarginResult = nullptr) const;

private:
  nsFrameList   mCaptionFrames;
};

#endif
