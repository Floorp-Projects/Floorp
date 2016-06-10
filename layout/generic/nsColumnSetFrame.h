/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsColumnSetFrame_h___
#define nsColumnSetFrame_h___

/* rendering object for css3 multi-column layout */

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsIFrameInlines.h" // for methods used by IS_TRUE_OVERFLOW_CONTAINER

/**
 * nsColumnSetFrame implements CSS multi-column layout.
 * @note nsColumnSetFrame keeps true overflow containers in the normal flow
 * child lists (i.e. the principal and overflow lists).
 */
class nsColumnSetFrame final : public nsContainerFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  explicit nsColumnSetFrame(nsStyleContext* aContext);

  virtual void Reflow(nsPresContext* aPresContext,
                      nsHTMLReflowMetrics& aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus) override;

#ifdef DEBUG
  virtual void SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList) override;
  virtual void AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList) override;
  virtual void InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList) override;
  virtual void RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame) override;
#endif

  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) override;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) override;

  /**
   * Retrieve the available height for content of this frame. The available content
   * height is the available height for the frame, minus borders and padding.
   */
  virtual nscoord GetAvailableContentBSize(const nsHTMLReflowState& aReflowState);

  virtual nsContainerFrame* GetContentInsertionFrame() override {
    nsIFrame* frame = PrincipalChildList().FirstChild();

    // if no children return nullptr
    if (!frame)
      return nullptr;

    return frame->GetContentInsertionFrame();
  }

  virtual bool IsFrameOfType(uint32_t aFlags) const override
   {
     return nsContainerFrame::IsFrameOfType(aFlags &
              ~(nsIFrame::eCanContainOverflowContainers));
   }

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  virtual nsIAtom* GetType() const override;

  virtual void PaintColumnRule(nsRenderingContext* aCtx,
                               const nsRect&        aDirtyRect,
                               const nsPoint&       aPt);

  /**
   * Similar to nsBlockFrame::DrainOverflowLines. Locate any columns not
   * handled by our prev-in-flow, and any columns sitting on our own
   * overflow list, and put them in our primary child list for reflowing.
   */
  void DrainOverflowColumns();

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("ColumnSet"), aResult);
  }
#endif

protected:
  nscoord        mLastBalanceBSize;
  nsReflowStatus mLastFrameStatus;

  /**
   * These are the parameters that control the layout of columns.
   */
  struct ReflowConfig {
    // The number of columns that we want to balance across. If we're not
    // balancing, this will be set to INT32_MAX.
    int32_t mBalanceColCount;

    // The inline-size of each individual column.
    nscoord mColISize;

    // The amount of inline-size that is expected to be left over after all the
    // columns and column gaps are laid out.
    nscoord mExpectedISizeLeftOver;

    // The width (inline-size) of each column gap.
    nscoord mColGap;

    // The maximum bSize of any individual column during a reflow iteration.
    // This parameter is set during each iteration of the binary search for
    // the best column block-size.
    nscoord mColMaxBSize;

    // A boolean controlling whether or not we are balancing. This should be
    // equivalent to mBalanceColCount == INT32_MAX.
    bool mIsBalancing;

    // The last known column block-size that was 'feasible'. A column bSize is
    // feasible if all child content fits within the specified bSize.
    nscoord mKnownFeasibleBSize;

    // The last known block-size that was 'infeasible'. A column bSize is
    // infeasible if not all child content fits within the specified bSize.
    nscoord mKnownInfeasibleBSize;

    // block-size of the column set frame
    nscoord mComputedBSize;

    // The block-size "consumed" by previous-in-flows.
    // The computed block-size should be equal to the block-size of the element
    // (i.e. the computed block-size itself) plus the consumed block-size.
    nscoord mConsumedBSize;
  };

  /**
   * Some data that is better calculated during reflow
   */
  struct ColumnBalanceData {
    // The maximum "content block-size" of any column
    nscoord mMaxBSize;
    // The sum of the "content block-size" for all columns
    nscoord mSumBSize;
    // The "content block-size" of the last column
    nscoord mLastBSize;
    // The maximum "content block-size" of all columns that overflowed
    // their available block-size
    nscoord mMaxOverflowingBSize;
    // This flag determines whether the last reflow of children exceeded the
    // computed block-size of the column set frame. If so, we set the bSize to
    // this maximum allowable bSize, and continue reflow without balancing.
    bool mHasExcessBSize;

    void Reset() {
      mMaxBSize = mSumBSize = mLastBSize = mMaxOverflowingBSize = 0;
      mHasExcessBSize = false;
    }
  };

  bool ReflowColumns(nsHTMLReflowMetrics& aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus& aReflowStatus,
                     ReflowConfig& aConfig,
                     bool aLastColumnUnbounded,
                     nsCollapsingMargin* aCarriedOutBEndMargin,
                     ColumnBalanceData& aColData);

  /**
   * The basic reflow strategy is to call this function repeatedly to
   * obtain specific parameters that determine the layout of the
   * columns. This function will compute those parameters from the CSS
   * style. This function will also be responsible for implementing
   * the state machine that controls column balancing.
   */
  ReflowConfig ChooseColumnStrategy(const nsHTMLReflowState& aReflowState,
                                    bool aForceAuto, nscoord aFeasibleBSize,
                                    nscoord aInfeasibleBSize);

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
  void FindBestBalanceBSize(const nsHTMLReflowState& aReflowState,
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
   * fit into the mColMaxBSize.
   */
  bool ReflowChildren(nsHTMLReflowMetrics& aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus& aStatus,
                        const ReflowConfig& aConfig,
                        bool aLastColumnUnbounded,
                        nsCollapsingMargin* aCarriedOutBEndMargin,
                        ColumnBalanceData& aColData);
};

#endif // nsColumnSetFrame_h___
