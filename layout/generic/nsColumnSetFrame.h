/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for css3 multi-column layout */

#include "nsContainerFrame.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsPresContext.h"
#include "nsHTMLParts.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsCOMPtr.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "nsCSSRendering.h"
#include <algorithm>

class nsColumnSetFrame : public nsContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  nsColumnSetFrame(nsStyleContext* aContext);

  NS_IMETHOD SetInitialChildList(ChildListID     aListID,
                                 nsFrameList&    aChildList);

  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);

  NS_IMETHOD  AppendFrames(ChildListID     aListID,
                           nsFrameList&    aFrameList);
  NS_IMETHOD  InsertFrames(ChildListID     aListID,
                           nsIFrame*       aPrevFrame,
                           nsFrameList&    aFrameList);
  NS_IMETHOD  RemoveFrame(ChildListID     aListID,
                          nsIFrame*       aOldFrame);

  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext);

  virtual nsIFrame* GetContentInsertionFrame() {
    nsIFrame* frame = GetFirstPrincipalChild();

    // if no children return nullptr
    if (!frame)
      return nullptr;

    return frame->GetContentInsertionFrame();
  }

  virtual nsresult StealFrame(nsPresContext* aPresContext,
                              nsIFrame*      aChild,
                              bool           aForceNormal)
  { // nsColumnSetFrame keeps overflow containers in main child list
    return nsContainerFrame::StealFrame(aPresContext, aChild, true);
  }

  virtual bool IsFrameOfType(uint32_t aFlags) const
   {
     return nsContainerFrame::IsFrameOfType(aFlags &
              ~(nsIFrame::eCanContainOverflowContainers));
   }

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  virtual nsIAtom* GetType() const;

  virtual void PaintColumnRule(nsRenderingContext* aCtx,
                               const nsRect&        aDirtyRect,
                               const nsPoint&       aPt);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const {
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
    // Whether or not we should revert back to 'auto' setting for column-fill.
    // This happens if we overflow our columns such that we no longer have
    // enough room to keep balancing.
    bool mShouldRevertToAuto;
    void Reset() {
      mMaxHeight = mSumHeight = mLastHeight = mMaxOverflowingHeight = 0;
      mShouldRevertToAuto = false;
    }
  };

  /**
   * Similar to nsBlockFrame::DrainOverflowLines. Locate any columns not
   * handled by our prev-in-flow, and any columns sitting on our own
   * overflow list, and put them in our primary child list for reflowing.
   */
  void DrainOverflowColumns();

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
