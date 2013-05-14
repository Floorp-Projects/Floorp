/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTableOuterFrame_h__
#define nsTableOuterFrame_h__

#include "mozilla/Attributes.h"
#include "nscore.h"
#include "nsContainerFrame.h"
#include "nsCellMap.h"
#include "nsBlockFrame.h"
#include "nsTableFrame.h"

class nsTableCaptionFrame : public nsBlockFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsISupports
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
  friend nsIFrame* NS_NewTableCaptionFrame(nsIPresShell* aPresShell, nsStyleContext*  aContext);

  virtual nsSize ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, bool aShrinkWrap);

  virtual nsIFrame* GetParentStyleContextFrame() const;

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
#endif

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

protected:
  nsTableCaptionFrame(nsStyleContext*  aContext);
  virtual ~nsTableCaptionFrame();
};


/* TODO
1. decide if we'll allow subclassing.  If so, decide which methods really need to be virtual.
*/

/**
 * main frame for an nsTable content object, 
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
  friend nsIFrame* NS_NewTableOuterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
  
  // nsIFrame overrides - see there for a description

  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

  NS_IMETHOD SetInitialChildList(ChildListID     aListID,
                                 nsFrameList&    aChildList) MOZ_OVERRIDE;
 
  virtual const nsFrameList& GetChildList(ChildListID aListID) const MOZ_OVERRIDE;
  virtual void GetChildLists(nsTArray<ChildList>* aLists) const MOZ_OVERRIDE;

  NS_IMETHOD AppendFrames(ChildListID     aListID,
                          nsFrameList&    aFrameList) MOZ_OVERRIDE;

  NS_IMETHOD InsertFrames(ChildListID     aListID,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList) MOZ_OVERRIDE;

  NS_IMETHOD RemoveFrame(ChildListID     aListID,
                         nsIFrame*       aOldFrame) MOZ_OVERRIDE;

  virtual nsIFrame* GetContentInsertionFrame() MOZ_OVERRIDE {
    return GetFirstPrincipalChild()->GetContentInsertionFrame();
  }

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  void BuildDisplayListForInnerTable(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists);

  virtual nscoord GetBaseline() const MOZ_OVERRIDE;

  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nsSize ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, bool aShrinkWrap) MOZ_OVERRIDE;

  /** process a reflow command for the table.
    * This involves reflowing the caption and the inner table.
    * @see nsIFrame::Reflow */
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::tableOuterFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

  virtual nsIFrame* GetParentStyleContextFrame() const MOZ_OVERRIDE;

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


  nsTableOuterFrame(nsStyleContext* aContext);
  virtual ~nsTableOuterFrame();

  void InitChildReflowState(nsPresContext&    aPresContext,                     
                            nsHTMLReflowState& aReflowState);

  uint8_t GetCaptionSide(); // NS_STYLE_CAPTION_SIDE_* or NO_SIDE

  bool HasSideCaption() {
    uint8_t captionSide = GetCaptionSide();
    return captionSide == NS_STYLE_CAPTION_SIDE_LEFT ||
           captionSide == NS_STYLE_CAPTION_SIDE_RIGHT;
  }
  
  uint8_t GetCaptionVerticalAlign();

  void SetDesiredSize(uint8_t         aCaptionSide,
                      const nsMargin& aInnerMargin,
                      const nsMargin& aCaptionMargin,
                      nscoord&        aWidth,
                      nscoord&        aHeight);

  nsresult   GetCaptionOrigin(uint32_t         aCaptionSide,
                              const nsSize&    aContainBlockSize,
                              const nsSize&    aInnerSize, 
                              const nsMargin&  aInnerMargin,
                              const nsSize&    aCaptionSize,
                              nsMargin&        aCaptionMargin,
                              nsPoint&         aOrigin);

  nsresult   GetInnerOrigin(uint32_t         aCaptionSide,
                            const nsSize&    aContainBlockSize,
                            const nsSize&    aCaptionSize, 
                            const nsMargin&  aCaptionMargin,
                            const nsSize&    aInnerSize,
                            nsMargin&        aInnerMargin,
                            nsPoint&         aOrigin);
  
  // reflow the child (caption or innertable frame)
  void OuterBeginReflowChild(nsPresContext*           aPresContext,
                             nsIFrame*                aChildFrame,
                             const nsHTMLReflowState& aOuterRS,
                             void*                    aChildRSSpace,
                             nscoord                  aAvailWidth);

  nsresult OuterDoReflowChild(nsPresContext*           aPresContext,
                              nsIFrame*                aChildFrame,
                              const nsHTMLReflowState& aChildRS,
                              nsHTMLReflowMetrics&     aMetrics,
                              nsReflowStatus&          aStatus);

  // Set the reflow metrics
  void UpdateReflowMetrics(uint8_t              aCaptionSide,
                           nsHTMLReflowMetrics& aMet,
                           const nsMargin&      aInnerMargin,
                           const nsMargin&      aCaptionMargin);

  // Get the margin.  aMarginNoAuto is aMargin, but with auto 
  // margins set to 0
  void GetChildMargin(nsPresContext*           aPresContext,
                      const nsHTMLReflowState& aOuterRS,
                      nsIFrame*                aChildFrame,
                      nscoord                  aAvailableWidth,
                      nsMargin&                aMargin);

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
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
