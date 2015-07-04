/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTableOuterFrame_h__
#define nsTableOuterFrame_h__

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "nscore.h"
#include "nsContainerFrame.h"
#include "nsCellMap.h"
#include "nsTableFrame.h"

/**
 * Primary frame for a table element,
 * the nsTableOuterFrame contains 0 or one caption frame, and a nsTableFrame
 * pseudo-frame (referred to as the "inner frame').
 */
class nsTableOuterFrame : public nsContainerFrame
{
public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  NS_DECL_QUERYFRAME_TARGET(nsTableOuterFrame)

  /** instantiate a new instance of nsTableRowFrame.
    * @param aPresShell the pres shell for this frame
    *
    * @return           the frame that was created
    */
  friend nsTableOuterFrame* NS_NewTableOuterFrame(nsIPresShell* aPresShell,
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
    return GetFirstPrincipalChild()->GetContentInsertionFrame();
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

  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) override;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) override;

  virtual mozilla::LogicalSize
  ComputeAutoSize(nsRenderingContext *aRenderingContext,
                  mozilla::WritingMode aWritingMode,
                  const mozilla::LogicalSize& aCBSize,
                  nscoord aAvailableISize,
                  const mozilla::LogicalSize& aMargin,
                  const mozilla::LogicalSize& aBorder,
                  const mozilla::LogicalSize& aPadding,
                  bool aShrinkWrap) override;

  /** process a reflow command for the table.
    * This involves reflowing the caption and the inner table.
    * @see nsIFrame::Reflow */
  virtual void Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus) override;

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::tableOuterFrame
   */
  virtual nsIAtom* GetType() const override;

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

protected:


  explicit nsTableOuterFrame(nsStyleContext* aContext);
  virtual ~nsTableOuterFrame();

  void InitChildReflowState(nsPresContext&    aPresContext,                     
                            nsHTMLReflowState& aReflowState);

  // Get a NS_STYLE_CAPTION_SIDE_* value, with physically-specified sides
  // resolved to logical sides, or NO_SIDE if no caption is present.
  uint8_t GetLogicalCaptionSide(mozilla::WritingMode aWM);

  bool HasSideCaption(mozilla::WritingMode aWM) {
    uint8_t captionSide = GetLogicalCaptionSide(aWM);
    return captionSide == NS_STYLE_CAPTION_SIDE_ISTART ||
           captionSide == NS_STYLE_CAPTION_SIDE_IEND;
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
                             const nsHTMLReflowState&           aOuterRS,
                             mozilla::Maybe<nsHTMLReflowState>& aChildRS,
                             nscoord                            aAvailISize);

  void OuterDoReflowChild(nsPresContext*           aPresContext,
                          nsIFrame*                aChildFrame,
                          const nsHTMLReflowState& aChildRS,
                          nsHTMLReflowMetrics&     aMetrics,
                          nsReflowStatus&          aStatus);

  // Set the overflow areas in our reflow metrics
  void UpdateOverflowAreas(nsHTMLReflowMetrics& aMet);

  // Get the margin.
  void GetChildMargin(nsPresContext*           aPresContext,
                      const nsHTMLReflowState& aOuterRS,
                      nsIFrame*                aChildFrame,
                      nscoord                  aAvailableWidth,
                      mozilla::LogicalMargin&  aMargin);

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
                                           (~eCanContainOverflowContainers));
  }

  nsTableFrame* InnerTableFrame() const {
    return static_cast<nsTableFrame*>(mFrames.FirstChild());
  }
  
private:
  nsFrameList   mCaptionFrames;
};

#endif
