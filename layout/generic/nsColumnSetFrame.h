/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for css3 multi-column layout */

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"

class nsColumnSetFrame : public nsContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  nsColumnSetFrame(nsStyleContext* aContext);

  virtual void SetInitialChildList(ChildListID     aListID,
                                       nsFrameList&    aChildList) MOZ_OVERRIDE;

  virtual void Reflow(nsPresContext* aPresContext,
                      nsHTMLReflowMetrics& aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus) MOZ_OVERRIDE;

  virtual void  AppendFrames(ChildListID     aListID,
                                 nsFrameList&    aFrameList) MOZ_OVERRIDE;
  virtual void  InsertFrames(ChildListID     aListID,
                                 nsIFrame*       aPrevFrame,
                                 nsFrameList&    aFrameList) MOZ_OVERRIDE;
  virtual void  RemoveFrame(ChildListID     aListID,
                                nsIFrame*       aOldFrame) MOZ_OVERRIDE;

  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;

  /**
   * Retrieve the available height for content of this frame. The available content
   * height is the available height for the frame, minus borders and padding.
   */
  virtual nscoord GetAvailableContentHeight(const nsHTMLReflowState& aReflowState);

  virtual nsContainerFrame* GetContentInsertionFrame() MOZ_OVERRIDE {
    nsIFrame* frame = GetFirstPrincipalChild();

    // if no children return nullptr
    if (!frame)
      return nullptr;

    return frame->GetContentInsertionFrame();
  }

  virtual nsresult StealFrame(nsIFrame* aChild, bool aForceNormal) MOZ_OVERRIDE
  { // nsColumnSetFrame keeps overflow containers in main child list
    return nsContainerFrame::StealFrame(aChild, true);
  }

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
   {
     return nsContainerFrame::IsFrameOfType(aFlags &
              ~(nsIFrame::eCanContainOverflowContainers));
   }

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  virtual void PaintColumnRule(nsRenderingContext* aCtx,
                               const nsRect&        aDirtyRect,
                               const nsPoint&       aPt);

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE {
    return MakeFrameName(NS_LITERAL_STRING("ColumnSet"), aResult);
  }
#endif

protected:
  nscoord        mLastBalanceHeight;
  nsReflowStatus mLastFrameStatus;

  /**
   * These are the parameters that control the layout of columns.
   */
  struct ReflowConfig {
    // The number of columns that we want to balance across. If we're not
    // balancing, this will be set to INT32_MAX.
    int32_t mBalanceColCount;

    // The width of each individual column.
    nscoord mColWidth;

    // The amount of width that is expected to be left over after all the
    // columns and column gaps are laid out.
    nscoord mExpectedWidthLeftOver;

    // The width of each column gap.
    nscoord mColGap;

    // The maximum height of any individual column during a reflow iteration.
    // This parameter is set during each iteration of the binary search for
    // the best column height.
    nscoord mColMaxHeight;

    // A boolean controlling whether or not we are balancing. This should be
    // equivalent to mBalanceColCount == INT32_MAX.
    bool mIsBalancing;

    // The last known column height that was 'feasible'. A column height is
    // feasible if all child content fits within the specified height.
    nscoord mKnownFeasibleHeight;

    // The last known height that was 'infeasible'. A column height is
    // infeasible if not all child content fits within the specified height.
    nscoord mKnownInfeasibleHeight;

    // Height of the column set frame
    nscoord mComputedHeight;

    // The height "consumed" by previous-in-flows.
    // The computed height should be equal to the height of the element (i.e.
    // the computed height itself) plus the consumed height.
    nscoord mConsumedHeight;
  };

  /**
   * Some data that is better calculated during reflow
   */
  struct ColumnBalanceData {
    // The maximum "content height" of any column
    nscoord mMaxHeight;
    // The sum of the "content heights" for all columns
    nscoord mSumHeight;
    // The "content height" of the last column
    nscoord mLastHeight;
    // The maximum "content height" of all columns that overflowed
    // their available height
    nscoord mMaxOverflowingHeight;
    // This flag determines whether the last reflow of children exceeded the
    // computed height of the column set frame. If so, we set the height to
    // this maximum allowable height, and continue reflow without balancing.
    bool mHasExcessHeight;

    void Reset() {
      mMaxHeight = mSumHeight = mLastHeight = mMaxOverflowingHeight = 0;
      mHasExcessHeight = false;
    }
  };

  /**
   * Similar to nsBlockFrame::DrainOverflowLines. Locate any columns not
   * handled by our prev-in-flow, and any columns sitting on our own
   * overflow list, and put them in our primary child list for reflowing.
   */
  void DrainOverflowColumns();

  bool ReflowColumns(nsHTMLReflowMetrics& aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus& aReflowStatus,
                     ReflowConfig& aConfig,
                     bool aLastColumnUnbounded,
                     nsCollapsingMargin* aCarriedOutBottomMargin,
                     ColumnBalanceData& aColData);

  /**
   * The basic reflow strategy is to call this function repeatedly to
   * obtain specific parameters that determine the layout of the
   * columns. This function will compute those parameters from the CSS
   * style. This function will also be responsible for implementing
   * the state machine that controls column balancing.
   */
  ReflowConfig ChooseColumnStrategy(const nsHTMLReflowState& aReflowState,
                                    bool aForceAuto, nscoord aFeasibleHeight,
                                    nscoord aInfeasibleHeight);

  /**
   * Perform the binary search for the best balance height for this column set.
   *
   * @param aReflowState The input parameters for the current reflow iteration.
   * @param aPresContext The presentation context in which the current reflow
   *        iteration is occurring.
   * @param aConfig The ReflowConfig object associated with this column set
   *        frame, generated by ChooseColumnStrategy().
   * @param aColData A data structure used to keep track of data needed between
   *        successive iterations of the balancing process.
   * @param aDesiredSize The final output size of the column set frame (output
   *        of reflow procedure).
   * @param aOutMargin The bottom margin of the column set frame that may be
   *        carried out from reflow (and thus collapsed).
   * @param aUnboundedLastColumn A boolean value indicating that the last column
   *        can be of any height. Used during the first iteration of the
   *        balancing procedure to measure the height of all content in
   *        descendant frames of the column set.
   * @param aRunWasFeasible An input/output parameter indicating whether or not
   *        the last iteration of the balancing loop was a feasible height to
   *        fit all content from descendant frames.
   * @param aStatus A final reflow status of the column set frame, passed in as
   *        an output parameter.
   */
  void FindBestBalanceHeight(const nsHTMLReflowState& aReflowState,
                             nsPresContext* aPresContext,
                             ReflowConfig& aConfig,
                             ColumnBalanceData& aColData,
                             nsHTMLReflowMetrics& aDesiredSize,
                             nsCollapsingMargin& aOutMargin,
                             bool& aUnboundedLastColumn,
                             bool& aRunWasFeasible,
                             nsReflowStatus& aStatus);
  /**
   * Reflow column children. Returns true iff the content that was reflowed
   * fit into the mColMaxHeight.
   */
  bool ReflowChildren(nsHTMLReflowMetrics& aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus& aStatus,
                        const ReflowConfig& aConfig,
                        bool aLastColumnUnbounded,
                        nsCollapsingMargin* aCarriedOutBottomMargin,
                        ColumnBalanceData& aColData);
};
