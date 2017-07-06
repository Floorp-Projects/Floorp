/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for css3 multi-column layout */

#include "mozilla/Unused.h"
#include "nsColumnSetFrame.h"
#include "nsCSSRendering.h"

using namespace mozilla;
using namespace mozilla::layout;


class nsDisplayColumnRule : public nsDisplayItem {
public:
  nsDisplayColumnRule(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame)
  {
    MOZ_COUNT_CTOR(nsDisplayColumnRule);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayColumnRule() {
    MOZ_COUNT_DTOR(nsDisplayColumnRule);
    mBorderRenderers.Clear();
  }
#endif

  /**
   * Returns the frame's visual overflow rect instead of the frame's bounds.
   */
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) override
  {
    *aSnap = false;
    return static_cast<nsColumnSetFrame*>(mFrame)->CalculateBounds(ToReferenceFrame());
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       const StackingContextHelper& aSc,
                                       nsTArray<WebRenderParentCommand>& aParentCommands,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     gfxContext* aCtx) override;

  NS_DISPLAY_DECL_NAME("ColumnRule", nsDisplayItem::TYPE_COLUMN_RULE);

private:
  nsTArray<nsCSSBorderRenderer> mBorderRenderers;
};

void
nsDisplayColumnRule::Paint(nsDisplayListBuilder* aBuilder,
                           gfxContext* aCtx)
{
  static_cast<nsColumnSetFrame*>(mFrame)->
    CreateBorderRenderers(mBorderRenderers, aCtx, mVisibleRect, ToReferenceFrame());

  for (auto iter = mBorderRenderers.begin(); iter != mBorderRenderers.end(); iter++) {
    iter->DrawBorders();
  }
}
LayerState
nsDisplayColumnRule::GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters)
{
  if (!gfxPrefs::LayersAllowColumnRuleLayers()) {
    return LAYER_NONE;
  }

  RefPtr<gfxContext> screenRefCtx =
    gfxContext::CreateOrNull(gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget());

  static_cast<nsColumnSetFrame*>(mFrame)->
    CreateBorderRenderers(mBorderRenderers, screenRefCtx, mVisibleRect, ToReferenceFrame());

  if (mBorderRenderers.IsEmpty()) {
    return LAYER_NONE;
  }

  for (auto iter = mBorderRenderers.begin(); iter != mBorderRenderers.end(); iter++) {
    if (!iter->CanCreateWebRenderCommands()) {
      return LAYER_NONE;
    }
  }

  return LAYER_ACTIVE;
}

already_AddRefed<Layer>
nsDisplayColumnRule::BuildLayer(nsDisplayListBuilder* aBuilder,
                                LayerManager* aManager,
                                const ContainerLayerParameters& aContainerParameters)
{
  return BuildDisplayItemLayer(aBuilder, aManager, aContainerParameters);
}

bool
nsDisplayColumnRule::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                             const StackingContextHelper& aSc,
                                             nsTArray<WebRenderParentCommand>& aParentCommands,
                                             mozilla::layers::WebRenderLayerManager* aManager,
                                             nsDisplayListBuilder* aDisplayListBuilder)
{
  if (aManager->IsLayersFreeTransaction()) {
    RefPtr<gfxContext> screenRefCtx =
      gfxContext::CreateOrNull(gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget());

    static_cast<nsColumnSetFrame*>(mFrame)->
      CreateBorderRenderers(mBorderRenderers, screenRefCtx, mVisibleRect, ToReferenceFrame());

    if (mBorderRenderers.IsEmpty()) {
      return false;
    }

    for (auto iter = mBorderRenderers.begin(); iter != mBorderRenderers.end(); iter++) {
      if (!iter->CanCreateWebRenderCommands()) {
        return false;
      }
    }
  }

  for (auto iter = mBorderRenderers.begin(); iter != mBorderRenderers.end(); iter++) {
    iter->CreateWebRenderCommands(aBuilder, aSc);
  }

  return true;
}

/**
 * Tracking issues:
 *
 * XXX cursor movement around the top and bottom of colums seems to make the editor
 * lose the caret.
 *
 * XXX should we support CSS columns applied to table elements?
 */
nsContainerFrame*
NS_NewColumnSetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, nsFrameState aStateFlags)
{
  nsColumnSetFrame* it = new (aPresShell) nsColumnSetFrame(aContext);
  it->AddStateBits(aStateFlags | NS_BLOCK_MARGIN_ROOT);
  return it;
}

NS_IMPL_FRAMEARENA_HELPERS(nsColumnSetFrame)

nsColumnSetFrame::nsColumnSetFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext, kClassID)
  , mLastBalanceBSize(NS_INTRINSICSIZE)
{
}

void
nsColumnSetFrame::ForEachColumn(const std::function<void(const nsRect& lineRect)>& aSetLineRect,
                                const nsPoint& aPt)
{
  nsIFrame* child = mFrames.FirstChild();
  if (!child)
    return;  // no columns

  nsIFrame* nextSibling = child->GetNextSibling();
  if (!nextSibling)
    return;  // 1 column only - this means no gap to draw on

  const nsStyleColumn* colStyle = StyleColumn();
  nscoord ruleWidth = colStyle->GetComputedColumnRuleWidth();
  if (!ruleWidth)
    return;

  WritingMode wm = GetWritingMode();
  bool isVertical = wm.IsVertical();
  bool isRTL = !wm.IsBidiLTR();

  // Get our content rect as an absolute coordinate, not relative to
  // our parent (which is what the X and Y normally is)
  nsRect contentRect = GetContentRect() - GetRect().TopLeft() + aPt;
  nsSize ruleSize = isVertical ? nsSize(contentRect.width, ruleWidth)
                               : nsSize(ruleWidth, contentRect.height);

  while (nextSibling) {
    // The frame tree goes RTL in RTL.
    // The |prevFrame| and |nextFrame| frames here are the visually preceding
    // (left/above) and following (right/below) frames, not in logical writing-
    // mode direction.
    nsIFrame* prevFrame = isRTL ? nextSibling : child;
    nsIFrame* nextFrame = isRTL ? child : nextSibling;

    // Each child frame's position coordinates is actually relative to this
    // nsColumnSetFrame.
    // linePt will be at the top-left edge to paint the line.
    nsPoint linePt;
    if (isVertical) {
      nscoord edgeOfPrev = prevFrame->GetRect().YMost() + aPt.y;
      nscoord edgeOfNext = nextFrame->GetRect().Y() + aPt.y;
      linePt = nsPoint(contentRect.x,
                       (edgeOfPrev + edgeOfNext - ruleSize.height) / 2);
    } else {
      nscoord edgeOfPrev = prevFrame->GetRect().XMost() + aPt.x;
      nscoord edgeOfNext = nextFrame->GetRect().X() + aPt.x;
      linePt = nsPoint((edgeOfPrev + edgeOfNext - ruleSize.width) / 2,
                       contentRect.y);
    }

    aSetLineRect(nsRect(linePt, ruleSize));

    child = nextSibling;
    nextSibling = nextSibling->GetNextSibling();
  }
}

nsRect
nsColumnSetFrame::CalculateBounds(const nsPoint& aOffset)
{
  nsRect combined;
  ForEachColumn([&combined](const nsRect& aLineRect)
                {
                  combined = combined.Union(aLineRect);
                }, aOffset);
  return combined;
}

void
nsColumnSetFrame::CreateBorderRenderers(nsTArray<nsCSSBorderRenderer>& aBorderRenderers,
                                        gfxContext* aCtx,
                                        const nsRect& aDirtyRect,
                                        const nsPoint& aPt)
{
  WritingMode wm = GetWritingMode();
  bool isVertical = wm.IsVertical();
  const nsStyleColumn* colStyle = StyleColumn();
  uint8_t ruleStyle;

  // Per spec, inset => ridge and outset => groove
  if (colStyle->mColumnRuleStyle == NS_STYLE_BORDER_STYLE_INSET)
    ruleStyle = NS_STYLE_BORDER_STYLE_RIDGE;
  else if (colStyle->mColumnRuleStyle == NS_STYLE_BORDER_STYLE_OUTSET)
    ruleStyle = NS_STYLE_BORDER_STYLE_GROOVE;
  else
    ruleStyle = colStyle->mColumnRuleStyle;

  nsPresContext* presContext = PresContext();
  nscoord ruleWidth = colStyle->GetComputedColumnRuleWidth();
  if (!ruleWidth)
    return;

  aBorderRenderers.Clear();
  nscolor ruleColor =
    GetVisitedDependentColor(&nsStyleColumn::mColumnRuleColor);

  // In order to re-use a large amount of code, we treat the column rule as a border.
  // We create a new border style object and fill in all the details of the column rule as
  // the left border. PaintBorder() does all the rendering for us, so we not
  // only save an enormous amount of code but we'll support all the line styles that
  // we support on borders!
  nsStyleBorder border(presContext);
  Sides skipSides;
  if (isVertical) {
    border.SetBorderWidth(eSideTop, ruleWidth);
    border.SetBorderStyle(eSideTop, ruleStyle);
    border.mBorderTopColor = StyleComplexColor::FromColor(ruleColor);
    skipSides |= mozilla::eSideBitsLeftRight;
    skipSides |= mozilla::eSideBitsBottom;
  } else {
    border.SetBorderWidth(eSideLeft, ruleWidth);
    border.SetBorderStyle(eSideLeft, ruleStyle);
    border.mBorderLeftColor = StyleComplexColor::FromColor(ruleColor);
    skipSides |= mozilla::eSideBitsTopBottom;
    skipSides |= mozilla::eSideBitsRight;
  }

  ForEachColumn([&]
                (const nsRect& aLineRect)
                {
                  // Assert that we're not drawing a border-image here; if we were, we
                  // couldn't ignore the DrawResult that PaintBorderWithStyleBorder returns.
                  MOZ_ASSERT(border.mBorderImageSource.GetType() == eStyleImageType_Null);

                  gfx::DrawTarget* dt = aCtx ? aCtx->GetDrawTarget() : nullptr;
                  Maybe<nsCSSBorderRenderer> br =
                    nsCSSRendering::CreateBorderRendererWithStyleBorder(presContext, dt,
                                                                        this, aDirtyRect,
                                                                        aLineRect, border,
                                                                        StyleContext(), skipSides);
                  if (br.isSome()) {
                    aBorderRenderers.AppendElement(br.value());
                  }
                }, aPt);
}

static nscoord
GetAvailableContentISize(const ReflowInput& aReflowInput)
{
  if (aReflowInput.AvailableISize() == NS_INTRINSICSIZE) {
    return NS_INTRINSICSIZE;
  }

  WritingMode wm = aReflowInput.GetWritingMode();
  nscoord borderPaddingISize =
    aReflowInput.ComputedLogicalBorderPadding().IStartEnd(wm);
  return std::max(0, aReflowInput.AvailableISize() - borderPaddingISize);
}

nscoord
nsColumnSetFrame::GetAvailableContentBSize(const ReflowInput& aReflowInput)
{
  if (aReflowInput.AvailableBSize() == NS_INTRINSICSIZE) {
    return NS_INTRINSICSIZE;
  }

  WritingMode wm = aReflowInput.GetWritingMode();
  LogicalMargin bp = aReflowInput.ComputedLogicalBorderPadding();
  bp.ApplySkipSides(GetLogicalSkipSides(&aReflowInput));
  bp.BEnd(wm) = aReflowInput.ComputedLogicalBorderPadding().BEnd(wm);
  return std::max(0, aReflowInput.AvailableBSize() - bp.BStartEnd(wm));
}

static nscoord
GetColumnGap(nsColumnSetFrame*    aFrame,
             const nsStyleColumn* aColStyle)
{
  if (eStyleUnit_Normal == aColStyle->mColumnGap.GetUnit())
    return aFrame->StyleFont()->mFont.size;
  if (eStyleUnit_Coord == aColStyle->mColumnGap.GetUnit()) {
    nscoord colGap = aColStyle->mColumnGap.GetCoordValue();
    NS_ASSERTION(colGap >= 0, "negative column gap");
    return colGap;
  }

  NS_NOTREACHED("Unknown gap type");
  return 0;
}

nsColumnSetFrame::ReflowConfig
nsColumnSetFrame::ChooseColumnStrategy(const ReflowInput& aReflowInput,
                                       bool aForceAuto = false,
                                       nscoord aFeasibleBSize = NS_INTRINSICSIZE,
                                       nscoord aInfeasibleBSize = 0)
{
  nscoord knownFeasibleBSize = aFeasibleBSize;
  nscoord knownInfeasibleBSize = aInfeasibleBSize;
  WritingMode wm = aReflowInput.GetWritingMode();

  const nsStyleColumn* colStyle = StyleColumn();
  nscoord availContentISize = GetAvailableContentISize(aReflowInput);
  if (aReflowInput.ComputedISize() != NS_INTRINSICSIZE) {
    availContentISize = aReflowInput.ComputedISize();
  }

  nscoord consumedBSize = ConsumedBSize(wm);

  // The effective computed height is the height of the current continuation
  // of the column set frame. This should be the same as the computed height
  // if we have an unconstrained available height.
  nscoord computedBSize = GetEffectiveComputedBSize(aReflowInput,
                                                    consumedBSize);
  nscoord colBSize = GetAvailableContentBSize(aReflowInput);

  if (aReflowInput.ComputedBSize() != NS_INTRINSICSIZE) {
    colBSize = aReflowInput.ComputedBSize();
  } else if (aReflowInput.ComputedMaxBSize() != NS_INTRINSICSIZE) {
    colBSize = std::min(colBSize, aReflowInput.ComputedMaxBSize());
  }

  nscoord colGap = GetColumnGap(this, colStyle);
  int32_t numColumns = colStyle->mColumnCount;

  // If column-fill is set to 'balance', then we want to balance the columns.
  const bool isBalancing = colStyle->mColumnFill == NS_STYLE_COLUMN_FILL_BALANCE
                           && !aForceAuto;
  if (isBalancing) {
    const uint32_t MAX_NESTED_COLUMN_BALANCING = 2;
    uint32_t cnt = 0;
    for (const ReflowInput* rs = aReflowInput.mParentReflowInput;
         rs && cnt < MAX_NESTED_COLUMN_BALANCING; rs = rs->mParentReflowInput) {
      if (rs->mFlags.mIsColumnBalancing) {
        ++cnt;
      }
    }
    if (cnt == MAX_NESTED_COLUMN_BALANCING) {
      numColumns = 1;
    }
  }

  nscoord colISize;
  // In vertical writing-mode, "column-width" (inline size) will actually be
  // physical height, but its CSS name is still column-width.
  if (colStyle->mColumnWidth.GetUnit() == eStyleUnit_Coord) {
    colISize = colStyle->mColumnWidth.GetCoordValue();
    NS_ASSERTION(colISize >= 0, "negative column width");
    // Reduce column count if necessary to make columns fit in the
    // available width. Compute max number of columns that fit in
    // availContentISize, satisfying colGap*(maxColumns - 1) +
    // colISize*maxColumns <= availContentISize
    if (availContentISize != NS_INTRINSICSIZE && colGap + colISize > 0
        && numColumns > 0) {
      // This expression uses truncated rounding, which is what we
      // want
      int32_t maxColumns =
        std::min(nscoord(nsStyleColumn::kMaxColumnCount),
                 (availContentISize + colGap) / (colGap + colISize));
      numColumns = std::max(1, std::min(numColumns, maxColumns));
    }
  } else if (numColumns > 0 && availContentISize != NS_INTRINSICSIZE) {
    nscoord iSizeMinusGaps = availContentISize - colGap * (numColumns - 1);
    colISize = iSizeMinusGaps / numColumns;
  } else {
    colISize = NS_INTRINSICSIZE;
  }
  // Take care of the situation where there's only one column but it's
  // still too wide
  colISize = std::max(1, std::min(colISize, availContentISize));

  nscoord expectedISizeLeftOver = 0;

  if (colISize != NS_INTRINSICSIZE && availContentISize != NS_INTRINSICSIZE) {
    // distribute leftover space

    // First, determine how many columns will be showing if the column
    // count is auto
    if (numColumns <= 0) {
      // choose so that colGap*(nominalColumnCount - 1) +
      // colISize*nominalColumnCount is nearly availContentISize
      // make sure to round down
      if (colGap + colISize > 0) {
        numColumns = (availContentISize + colGap) / (colGap + colISize);
        // The number of columns should never exceed kMaxColumnCount.
        numColumns = std::min(nscoord(nsStyleColumn::kMaxColumnCount),
                              numColumns);
      }
      if (numColumns <= 0) {
        numColumns = 1;
      }
    }

    // Compute extra space and divide it among the columns
    nscoord extraSpace =
      std::max(0, availContentISize - (colISize * numColumns +
                                       colGap * (numColumns - 1)));
    nscoord extraToColumns = extraSpace / numColumns;
    colISize += extraToColumns;
    expectedISizeLeftOver = extraSpace - (extraToColumns * numColumns);
  }

  if (isBalancing) {
    if (numColumns <= 0) {
      // Hmm, auto column count, column width or available width is unknown,
      // and balancing is required. Let's just use one column then.
      numColumns = 1;
    }
    colBSize = std::min(mLastBalanceBSize, colBSize);
  } else {
    // This is the case when the column-fill property is set to 'auto'.
    // No balancing, so don't limit the column count
    numColumns = INT32_MAX;

    // XXX_jwir3: If a page's height is set to 0, we could continually
    //            create continuations, resulting in an infinite loop, since
    //            no progress is ever made. This is an issue with the spec
    //            (css3-multicol, css3-page, and css3-break) that is
    //            unresolved as of 27 Feb 2013. For the time being, we set this
    //            to have a minimum of 1 css px. Once a resolution is made
    //            on what minimum to have for a page height, we may need to
    //            change this value to match the appropriate spec(s).
    colBSize = std::max(colBSize, nsPresContext::CSSPixelsToAppUnits(1));
  }

#ifdef DEBUG_roc
  printf("*** nsColumnSetFrame::ChooseColumnStrategy: numColumns=%d, colISize=%d,"
         " expectedISizeLeftOver=%d, colBSize=%d, colGap=%d\n",
         numColumns, colISize, expectedISizeLeftOver, colBSize, colGap);
#endif
  ReflowConfig config = { numColumns, colISize, expectedISizeLeftOver, colGap,
                          colBSize, isBalancing, knownFeasibleBSize,
                          knownInfeasibleBSize, computedBSize, consumedBSize };
  return config;
}

bool
nsColumnSetFrame::ReflowColumns(ReflowOutput& aDesiredSize,
                                const ReflowInput& aReflowInput,
                                nsReflowStatus& aReflowStatus,
                                ReflowConfig& aConfig,
                                bool aLastColumnUnbounded,
                                nsCollapsingMargin* aCarriedOutBEndMargin,
                                ColumnBalanceData& aColData)
{
  bool feasible = ReflowChildren(aDesiredSize, aReflowInput,
                                 aReflowStatus, aConfig, aLastColumnUnbounded,
                                 aCarriedOutBEndMargin, aColData);

  if (aColData.mHasExcessBSize) {
    aConfig = ChooseColumnStrategy(aReflowInput, true);

    // We need to reflow our children again one last time, otherwise we might
    // end up with a stale column height for some of our columns, since we
    // bailed out of balancing.
    feasible = ReflowChildren(aDesiredSize, aReflowInput, aReflowStatus,
                              aConfig, aLastColumnUnbounded,
                              aCarriedOutBEndMargin, aColData);
  }

  return feasible;
}

static void MoveChildTo(nsIFrame* aChild, LogicalPoint aOrigin,
                        WritingMode aWM, const nsSize& aContainerSize)
{
  if (aChild->GetLogicalPosition(aWM, aContainerSize) == aOrigin) {
    return;
  }

  aChild->SetPosition(aWM, aOrigin, aContainerSize);
  nsContainerFrame::PlaceFrameView(aChild);
}

nscoord
nsColumnSetFrame::GetMinISize(gfxContext *aRenderingContext)
{
  nscoord iSize = 0;
  DISPLAY_MIN_WIDTH(this, iSize);
  if (mFrames.FirstChild()) {
    iSize = mFrames.FirstChild()->GetMinISize(aRenderingContext);
  }
  const nsStyleColumn* colStyle = StyleColumn();
  nscoord colISize;
  if (colStyle->mColumnWidth.GetUnit() == eStyleUnit_Coord) {
    colISize = colStyle->mColumnWidth.GetCoordValue();
    // As available width reduces to zero, we reduce our number of columns
    // to one, and don't enforce the column width, so just return the min
    // of the child's min-width with any specified column width.
    iSize = std::min(iSize, colISize);
  } else {
    NS_ASSERTION(colStyle->mColumnCount > 0,
                 "column-count and column-width can't both be auto");
    // As available width reduces to zero, we still have mColumnCount columns,
    // so multiply the child's min-width by the number of columns (n) and
    // include n-1 column gaps.
    colISize = iSize;
    iSize *= colStyle->mColumnCount;
    nscoord colGap = GetColumnGap(this, colStyle);
    iSize += colGap * (colStyle->mColumnCount - 1);
    // The multiplication above can make 'width' negative (integer overflow),
    // so use std::max to protect against that.
    iSize = std::max(iSize, colISize);
  }
  // XXX count forced column breaks here? Maybe we should return the child's
  // min-width times the minimum number of columns.
  return iSize;
}

nscoord
nsColumnSetFrame::GetPrefISize(gfxContext *aRenderingContext)
{
  // Our preferred width is our desired column width, if specified, otherwise
  // the child's preferred width, times the number of columns, plus the width
  // of any required column gaps
  // XXX what about forced column breaks here?
  nscoord result = 0;
  DISPLAY_PREF_WIDTH(this, result);
  const nsStyleColumn* colStyle = StyleColumn();
  nscoord colGap = GetColumnGap(this, colStyle);

  nscoord colISize;
  if (colStyle->mColumnWidth.GetUnit() == eStyleUnit_Coord) {
    colISize = colStyle->mColumnWidth.GetCoordValue();
  } else if (mFrames.FirstChild()) {
    colISize = mFrames.FirstChild()->GetPrefISize(aRenderingContext);
  } else {
    colISize = 0;
  }

  int32_t numColumns = colStyle->mColumnCount;
  if (numColumns <= 0) {
    // if column-count is auto, assume one column
    numColumns = 1;
  }

  nscoord iSize = colISize * numColumns + colGap * (numColumns - 1);
  // The multiplication above can make 'iSize' negative (integer overflow),
  // so use std::max to protect against that.
  result = std::max(iSize, colISize);
  return result;
}

bool
nsColumnSetFrame::ReflowChildren(ReflowOutput&     aDesiredSize,
                                 const ReflowInput& aReflowInput,
                                 nsReflowStatus&          aStatus,
                                 const ReflowConfig&      aConfig,
                                 bool                     aUnboundedLastColumn,
                                 nsCollapsingMargin*      aCarriedOutBEndMargin,
                                 ColumnBalanceData&       aColData)
{
  aColData.Reset();
  bool allFit = true;
  WritingMode wm = GetWritingMode();
  bool isRTL = !wm.IsBidiLTR();
  bool shrinkingBSizeOnly = !NS_SUBTREE_DIRTY(this) &&
    mLastBalanceBSize > aConfig.mColMaxBSize;

#ifdef DEBUG_roc
  printf("*** Doing column reflow pass: mLastBalanceBSize=%d, mColMaxBSize=%d, RTL=%d\n"
         "    mBalanceColCount=%d, mColISize=%d, mColGap=%d\n",
         mLastBalanceBSize, aConfig.mColMaxBSize, isRTL, aConfig.mBalanceColCount,
         aConfig.mColISize, aConfig.mColGap);
#endif

  DrainOverflowColumns();

  const bool colBSizeChanged = mLastBalanceBSize != aConfig.mColMaxBSize;

  if (colBSizeChanged) {
    mLastBalanceBSize = aConfig.mColMaxBSize;
    // XXX Seems like this could fire if incremental reflow pushed the column set
    // down so we reflow incrementally with a different available height.
    // We need a way to do an incremental reflow and be sure availableHeight
    // changes are taken account of! Right now I think block frames with absolute
    // children might exit early.
    //NS_ASSERTION(aKidReason != eReflowReason_Incremental,
    //             "incremental reflow should not have changed the balance height");
  }

  // get our border and padding
  LogicalMargin borderPadding = aReflowInput.ComputedLogicalBorderPadding();
  borderPadding.ApplySkipSides(GetLogicalSkipSides(&aReflowInput));

  nsRect contentRect(0, 0, 0, 0);
  nsOverflowAreas overflowRects;

  nsIFrame* child = mFrames.FirstChild();
  LogicalPoint childOrigin(wm, borderPadding.IStart(wm),
                           borderPadding.BStart(wm));
  // In vertical-rl mode, columns will not be correctly placed if the
  // reflowInput's ComputedWidth() is UNCONSTRAINED (in which case we'll get
  // a containerSize.width of zero here). In that case, the column positions
  // will be adjusted later, after our correct contentSize is known.
  nsSize containerSize = aReflowInput.ComputedSizeAsContainerIfConstrained();

  // For RTL, since the columns might not fill the frame exactly, we
  // need to account for the slop. Otherwise we'll waste time moving the
  // columns by some tiny amount

  // XXX when all of layout is converted to logical coordinates, we
  //     probably won't need to do this hack any more. For now, we
  //     confine it to the legacy horizontal-rl case
  if (!wm.IsVertical() && isRTL) {
    nscoord availISize = aReflowInput.AvailableISize();
    if (aReflowInput.ComputedISize() != NS_INTRINSICSIZE) {
      availISize = aReflowInput.ComputedISize();
    }
    if (availISize != NS_INTRINSICSIZE) {
      childOrigin.I(wm) = containerSize.width - borderPadding.Left(wm) -
                          availISize;
#ifdef DEBUG_roc
      printf("*** childOrigin.iCoord = %d\n", childOrigin.I(wm));
#endif
    }
  }

  int columnCount = 0;
  int contentBEnd = 0;
  bool reflowNext = false;

  while (child) {
    // Try to skip reflowing the child. We can't skip if the child is dirty. We also can't
    // skip if the next column is dirty, because the next column's first line(s)
    // might be pullable back to this column. We can't skip if it's the last child
    // because we need to obtain the bottom margin. We can't skip
    // if this is the last column and we're supposed to assign unbounded
    // height to it, because that could change the available height from
    // the last time we reflowed it and we should try to pull all the
    // content from its next sibling. (Note that it might be the last
    // column, but not be the last child because the desired number of columns
    // has changed.)
    bool skipIncremental = !aReflowInput.ShouldReflowAllKids()
      && !NS_SUBTREE_DIRTY(child)
      && child->GetNextSibling()
      && !(aUnboundedLastColumn && columnCount == aConfig.mBalanceColCount - 1)
      && !NS_SUBTREE_DIRTY(child->GetNextSibling());
    // If we need to pull up content from the prev-in-flow then this is not just
    // a height shrink. The prev in flow will have set the dirty bit.
    // Check the overflow rect YMost instead of just the child's content height. The child
    // may have overflowing content that cares about the available height boundary.
    // (It may also have overflowing content that doesn't care about the available height
    // boundary, but if so, too bad, this optimization is defeated.)
    // We want scrollable overflow here since this is a calculation that
    // affects layout.
    bool skipResizeBSizeShrink = false;
    if (shrinkingBSizeOnly) {
      switch (wm.GetBlockDir()) {
      case WritingMode::eBlockTB:
        if (child->GetScrollableOverflowRect().YMost() <= aConfig.mColMaxBSize) {
          skipResizeBSizeShrink = true;
        }
        break;
      case WritingMode::eBlockLR:
        if (child->GetScrollableOverflowRect().XMost() <= aConfig.mColMaxBSize) {
          skipResizeBSizeShrink = true;
        }
        break;
      case WritingMode::eBlockRL:
        // XXX not sure how to handle this, so for now just don't attempt
        // the optimization
        break;
      default:
        NS_NOTREACHED("unknown block direction");
        break;
      }
    }

    nscoord childContentBEnd = 0;
    if (!reflowNext && (skipIncremental || skipResizeBSizeShrink)) {
      // This child does not need to be reflowed, but we may need to move it
      MoveChildTo(child, childOrigin, wm, containerSize);

      // If this is the last frame then make sure we get the right status
      nsIFrame* kidNext = child->GetNextSibling();
      if (kidNext) {
        aStatus.Reset();
        if (kidNext->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) {
          aStatus.SetOverflowIncomplete();
        } else {
          aStatus.SetIncomplete();
        }
      } else {
        aStatus = mLastFrameStatus;
      }
      childContentBEnd = nsLayoutUtils::CalculateContentBEnd(wm, child);
#ifdef DEBUG_roc
      printf("*** Skipping child #%d %p (incremental %d, resize block-size shrink %d): status = %d\n",
             columnCount, (void*)child, skipIncremental, skipResizeBSizeShrink, aStatus);
#endif
    } else {
      LogicalSize availSize(wm, aConfig.mColISize, aConfig.mColMaxBSize);
      if (aUnboundedLastColumn && columnCount == aConfig.mBalanceColCount - 1) {
        availSize.BSize(wm) = GetAvailableContentBSize(aReflowInput);
      }

      LogicalSize computedSize = aReflowInput.ComputedSize(wm);

      if (reflowNext)
        child->AddStateBits(NS_FRAME_IS_DIRTY);

      LogicalSize kidCBSize(wm, availSize.ISize(wm), computedSize.BSize(wm));
      ReflowInput kidReflowInput(PresContext(), aReflowInput, child,
                                       availSize, &kidCBSize);
      kidReflowInput.mFlags.mIsTopOfPage = true;
      kidReflowInput.mFlags.mTableIsSplittable = false;
      kidReflowInput.mFlags.mIsColumnBalancing = aConfig.mBalanceColCount < INT32_MAX;

      // We need to reflow any float placeholders, even if our column height
      // hasn't changed.
      kidReflowInput.mFlags.mMustReflowPlaceholders = !colBSizeChanged;

#ifdef DEBUG_roc
      printf("*** Reflowing child #%d %p: availHeight=%d\n",
             columnCount, (void*)child,availSize.BSize(wm));
#endif

      // Note if the column's next in flow is not being changed by this incremental reflow.
      // This may allow the current column to avoid trying to pull lines from the next column.
      if (child->GetNextSibling() &&
          !(GetStateBits() & NS_FRAME_IS_DIRTY) &&
        !(child->GetNextSibling()->GetStateBits() & NS_FRAME_IS_DIRTY)) {
        kidReflowInput.mFlags.mNextInFlowUntouched = true;
      }

      ReflowOutput kidDesiredSize(wm, aDesiredSize.mFlags);

      // XXX it would be cool to consult the float manager for the
      // previous block to figure out the region of floats from the
      // previous column that extend into this column, and subtract
      // that region from the new float manager.  So you could stick a
      // really big float in the first column and text in following
      // columns would flow around it.

      // Reflow the frame
      LogicalPoint origin(wm,
                          childOrigin.I(wm) +
                          kidReflowInput.ComputedLogicalMargin().IStart(wm),
                          childOrigin.B(wm) +
                          kidReflowInput.ComputedLogicalMargin().BStart(wm));
      ReflowChild(child, PresContext(), kidDesiredSize, kidReflowInput,
                  wm, origin, containerSize, 0, aStatus);

      reflowNext = aStatus.NextInFlowNeedsReflow();

#ifdef DEBUG_roc
      printf("*** Reflowed child #%d %p: status = %d, desiredSize=%d,%d CarriedOutBEndMargin=%d\n",
             columnCount, (void*)child, aStatus, kidDesiredSize.Width(), kidDesiredSize.Height(),
             kidDesiredSize.mCarriedOutBEndMargin.get());
#endif

      NS_FRAME_TRACE_REFLOW_OUT("Column::Reflow", aStatus);

      *aCarriedOutBEndMargin = kidDesiredSize.mCarriedOutBEndMargin;

      FinishReflowChild(child, PresContext(), kidDesiredSize,
                        &kidReflowInput, wm, childOrigin, containerSize, 0);

      childContentBEnd = nsLayoutUtils::CalculateContentBEnd(wm, child);
      if (childContentBEnd > aConfig.mColMaxBSize) {
        allFit = false;
      }
      if (childContentBEnd > availSize.BSize(wm)) {
        aColData.mMaxOverflowingBSize = std::max(childContentBEnd,
            aColData.mMaxOverflowingBSize);
      }
    }

    contentRect.UnionRect(contentRect, child->GetRect());

    ConsiderChildOverflow(overflowRects, child);
    contentBEnd = std::max(contentBEnd, childContentBEnd);
    aColData.mLastBSize = childContentBEnd;
    aColData.mSumBSize += childContentBEnd;

    // Build a continuation column if necessary
    nsIFrame* kidNextInFlow = child->GetNextInFlow();

    if (aStatus.IsFullyComplete() && !aStatus.IsTruncated()) {
      NS_ASSERTION(!kidNextInFlow, "next in flow should have been deleted");
      child = nullptr;
      break;
    } else {
      ++columnCount;
      // Make sure that the column has a next-in-flow. If not, we must
      // create one to hold the overflowing stuff, even if we're just
      // going to put it on our overflow list and let *our*
      // next in flow handle it.
      if (!kidNextInFlow) {
        NS_ASSERTION(aStatus.NextInFlowNeedsReflow(),
                     "We have to create a continuation, but the block doesn't want us to reflow it?");

        // We need to create a continuing column
        kidNextInFlow = CreateNextInFlow(child);
      }

      // Make sure we reflow a next-in-flow when it switches between being
      // normal or overflow container
      if (aStatus.IsOverflowIncomplete()) {
        if (!(kidNextInFlow->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER)) {
          aStatus.SetNextInFlowNeedsReflow();
          reflowNext = true;
          kidNextInFlow->AddStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
        }
      }
      else if (kidNextInFlow->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) {
        aStatus.SetNextInFlowNeedsReflow();
        reflowNext = true;
        kidNextInFlow->RemoveStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
      }

      if ((contentBEnd > aReflowInput.ComputedMaxBSize() ||
           contentBEnd > aReflowInput.ComputedBSize()) &&
           aConfig.mBalanceColCount < INT32_MAX) {
        // We overflowed vertically, but have not exceeded the number of
        // columns. We're going to go into overflow columns now, so balancing
        // no longer applies.
        aColData.mHasExcessBSize = true;
      }

      if (columnCount >= aConfig.mBalanceColCount) {
        // No more columns allowed here. Stop.
        aStatus.SetNextInFlowNeedsReflow();
        kidNextInFlow->AddStateBits(NS_FRAME_IS_DIRTY);
        // Move any of our leftover columns to our overflow list. Our
        // next-in-flow will eventually pick them up.
        const nsFrameList& continuationColumns = mFrames.RemoveFramesAfter(child);
        if (continuationColumns.NotEmpty()) {
          SetOverflowFrames(continuationColumns);
        }
        child = nullptr;
        break;
      }
    }

    if (PresContext()->HasPendingInterrupt()) {
      // Stop the loop now while |child| still points to the frame that bailed
      // out.  We could keep going here and condition a bunch of the code in
      // this loop on whether there's an interrupt, or even just keep going and
      // trying to reflow the blocks (even though we know they'll interrupt
      // right after their first line), but stopping now is conceptually the
      // simplest (and probably fastest) thing.
      break;
    }

    // Advance to the next column
    child = child->GetNextSibling();

    if (child) {
      childOrigin.I(wm) += aConfig.mColISize + aConfig.mColGap;

#ifdef DEBUG_roc
      printf("*** NEXT CHILD ORIGIN.icoord = %d\n", childOrigin.I(wm));
#endif
    }
  }

  if (PresContext()->CheckForInterrupt(this) &&
      (GetStateBits() & NS_FRAME_IS_DIRTY)) {
    // Mark all our kids starting with |child| dirty

    // Note that this is a CheckForInterrupt call, not a HasPendingInterrupt,
    // because we might have interrupted while reflowing |child|, and since
    // we're about to add a dirty bit to |child| we need to make sure that
    // |this| is scheduled to have dirty bits marked on it and its ancestors.
    // Otherwise, when we go to mark dirty bits on |child|'s ancestors we'll
    // bail out immediately, since it'll already have a dirty bit.
    for (; child; child = child->GetNextSibling()) {
      child->AddStateBits(NS_FRAME_IS_DIRTY);
    }
  }

  aColData.mMaxBSize = contentBEnd;
  LogicalSize contentSize = LogicalSize(wm, contentRect.Size());
  contentSize.BSize(wm) = std::max(contentSize.BSize(wm), contentBEnd);
  mLastFrameStatus = aStatus;

  // Apply computed and min/max values
  if (aConfig.mComputedBSize != NS_INTRINSICSIZE) {
    if (aReflowInput.AvailableBSize() != NS_INTRINSICSIZE) {
      contentSize.BSize(wm) = std::min(contentSize.BSize(wm),
                                       aConfig.mComputedBSize);
    } else {
      contentSize.BSize(wm) = aConfig.mComputedBSize;
    }
  } else {
    // We add the "consumed" block-size back in so that we're applying
    // constraints to the correct bSize value, then subtract it again
    // after we've finished with the min/max calculation. This prevents us from
    // having a last continuation that is smaller than the min bSize. but which
    // has prev-in-flows, trigger a larger bSize than actually required.
    contentSize.BSize(wm) =
      aReflowInput.ApplyMinMaxBSize(contentSize.BSize(wm),
                                    aConfig.mConsumedBSize);
  }
  if (aReflowInput.ComputedISize() != NS_INTRINSICSIZE) {
    contentSize.ISize(wm) = aReflowInput.ComputedISize();
  } else {
    contentSize.ISize(wm) =
      aReflowInput.ApplyMinMaxISize(contentSize.ISize(wm));
  }

  contentSize.ISize(wm) += borderPadding.IStartEnd(wm);
  contentSize.BSize(wm) += borderPadding.BStartEnd(wm);
  aDesiredSize.SetSize(wm, contentSize);
  aDesiredSize.mOverflowAreas = overflowRects;
  aDesiredSize.UnionOverflowAreasWithDesiredBounds();

  // In vertical-rl mode, make a second pass if necessary to reposition the
  // columns with the correct container width. (In other writing modes,
  // correct containerSize was not required for column positioning so we don't
  // need this fixup.)
  if (wm.IsVerticalRL() && containerSize.width != contentSize.Width(wm)) {
    const nsSize finalContainerSize = aDesiredSize.PhysicalSize();
    for (nsIFrame* child : mFrames) {
      // Get the logical position as set previously using a provisional or
      // dummy containerSize, and reset with the correct container size.
      child->SetPosition(wm, child->GetLogicalPosition(wm, containerSize),
                         finalContainerSize);
    }
  }

#ifdef DEBUG_roc
  printf("*** DONE PASS feasible=%d\n", allFit && aStatus.IsFullyComplete()
         && !aStatus.IsTruncated());
#endif
  return allFit && aStatus.IsFullyComplete()
    && !aStatus.IsTruncated();
}

void
nsColumnSetFrame::DrainOverflowColumns()
{
  // First grab the prev-in-flows overflows and reparent them to this
  // frame.
  nsPresContext* presContext = PresContext();
  nsColumnSetFrame* prev = static_cast<nsColumnSetFrame*>(GetPrevInFlow());
  if (prev) {
    AutoFrameListPtr overflows(presContext, prev->StealOverflowFrames());
    if (overflows) {
      nsContainerFrame::ReparentFrameViewList(*overflows, prev, this);

      mFrames.InsertFrames(this, nullptr, *overflows);
    }
  }

  // Now pull back our own overflows and append them to our children.
  // We don't need to reparent them since we're already their parent.
  AutoFrameListPtr overflows(presContext, StealOverflowFrames());
  if (overflows) {
    // We're already the parent for these frames, so no need to set
    // their parent again.
    mFrames.AppendFrames(nullptr, *overflows);
  }
}

void
nsColumnSetFrame::FindBestBalanceBSize(const ReflowInput& aReflowInput,
                                       nsPresContext* aPresContext,
                                       ReflowConfig& aConfig,
                                       ColumnBalanceData& aColData,
                                       ReflowOutput& aDesiredSize,
                                       nsCollapsingMargin& aOutMargin,
                                       bool& aUnboundedLastColumn,
                                       bool& aRunWasFeasible,
                                       nsReflowStatus& aStatus)
{
  bool feasible = aRunWasFeasible;

  nsMargin bp = aReflowInput.ComputedPhysicalBorderPadding();
  bp.ApplySkipSides(GetSkipSides());
  bp.bottom = aReflowInput.ComputedPhysicalBorderPadding().bottom;

  nscoord availableContentBSize =
    GetAvailableContentBSize(aReflowInput);

  // Termination of the algorithm below is guaranteed because
  // aConfig.knownFeasibleBSize - aConfig.knownInfeasibleBSize decreases in every
  // iteration.

  // We set this flag when we detect that we may contain a frame
  // that can break anywhere (thus foiling the linear decrease-by-one
  // search)
  bool maybeContinuousBreakingDetected = false;

  while (!aPresContext->HasPendingInterrupt()) {
    nscoord lastKnownFeasibleBSize = aConfig.mKnownFeasibleBSize;

    // Record what we learned from the last reflow
    if (feasible) {
      // maxBSize is feasible. Also, mLastBalanceBSize is feasible.
      aConfig.mKnownFeasibleBSize = std::min(aConfig.mKnownFeasibleBSize,
                                              aColData.mMaxBSize);
      aConfig.mKnownFeasibleBSize = std::min(aConfig.mKnownFeasibleBSize,
                                              mLastBalanceBSize);

      // Furthermore, no height less than the height of the last
      // column can ever be feasible. (We might be able to reduce the
      // height of a non-last column by moving content to a later column,
      // but we can't do that with the last column.)
      if (mFrames.GetLength() == aConfig.mBalanceColCount) {
        aConfig.mKnownInfeasibleBSize = std::max(aConfig.mKnownInfeasibleBSize,
                                       aColData.mLastBSize - 1);
      }
    } else {
      aConfig.mKnownInfeasibleBSize = std::max(aConfig.mKnownInfeasibleBSize,
                                                mLastBalanceBSize);
      // If a column didn't fit in its available height, then its current
      // height must be the minimum height for unbreakable content in
      // the column, and therefore no smaller height can be feasible.
      aConfig.mKnownInfeasibleBSize = std::max(aConfig.mKnownInfeasibleBSize,
                                         aColData.mMaxOverflowingBSize - 1);

      if (aUnboundedLastColumn) {
        // The last column is unbounded, so all content got reflowed, so the
        // mColMaxBSize is feasible.
        aConfig.mKnownFeasibleBSize = std::min(aConfig.mKnownFeasibleBSize,
                                                aColData.mMaxBSize);
      }
    }

#ifdef DEBUG_roc
    printf("*** nsColumnSetFrame::Reflow balancing knownInfeasible=%d knownFeasible=%d\n",
           aConfig.mKnownInfeasibleBSize, aConfig.mKnownFeasibleBSize);
#endif


    if (aConfig.mKnownInfeasibleBSize >= aConfig.mKnownFeasibleBSize - 1) {
      // aConfig.mKnownFeasibleBSize is where we want to be
      break;
    }

    if (aConfig.mKnownInfeasibleBSize >= availableContentBSize) {
      break;
    }

    if (lastKnownFeasibleBSize - aConfig.mKnownFeasibleBSize == 1) {
      // We decreased the feasible height by one twip only. This could
      // indicate that there is a continuously breakable child frame
      // that we are crawling through.
      maybeContinuousBreakingDetected = true;
    }

    nscoord nextGuess = (aConfig.mKnownFeasibleBSize + aConfig.mKnownInfeasibleBSize)/2;
    // The constant of 600 twips is arbitrary. It's about two line-heights.
    if (aConfig.mKnownFeasibleBSize - nextGuess < 600 &&
        !maybeContinuousBreakingDetected) {
      // We're close to our target, so just try shrinking just the
      // minimum amount that will cause one of our columns to break
      // differently.
      nextGuess = aConfig.mKnownFeasibleBSize - 1;
    } else if (aUnboundedLastColumn) {
      // Make a guess by dividing that into N columns. Add some slop
      // to try to make it on the feasible side.  The constant of
      // 600 twips is arbitrary. It's about two line-heights.
      nextGuess = aColData.mSumBSize/aConfig.mBalanceColCount + 600;
      // Sanitize it
      nextGuess = clamped(nextGuess, aConfig.mKnownInfeasibleBSize + 1,
                                     aConfig.mKnownFeasibleBSize - 1);
    } else if (aConfig.mKnownFeasibleBSize == NS_INTRINSICSIZE) {
      // This can happen when we had a next-in-flow so we didn't
      // want to do an unbounded height measuring step. Let's just increase
      // from the infeasible height by some reasonable amount.
      nextGuess = aConfig.mKnownInfeasibleBSize*2 + 600;
    }
    // Don't bother guessing more than our height constraint.
    nextGuess = std::min(availableContentBSize, nextGuess);

#ifdef DEBUG_roc
    printf("*** nsColumnSetFrame::Reflow balancing choosing next guess=%d\n", nextGuess);
#endif

    aConfig.mColMaxBSize = nextGuess;

    aUnboundedLastColumn = false;
    AddStateBits(NS_FRAME_IS_DIRTY);
    feasible = ReflowColumns(aDesiredSize, aReflowInput, aStatus, aConfig, false,
                             &aOutMargin, aColData);

    if (!aConfig.mIsBalancing) {
      // Looks like we had excess height when balancing, so we gave up on
      // trying to balance.
      break;
    }
  }

  if (aConfig.mIsBalancing && !feasible &&
      !aPresContext->HasPendingInterrupt()) {
    // We may need to reflow one more time at the feasible height to
    // get a valid layout.
    bool skip = false;
    if (aConfig.mKnownInfeasibleBSize >= availableContentBSize) {
      aConfig.mColMaxBSize = availableContentBSize;
      if (mLastBalanceBSize == availableContentBSize) {
        skip = true;
      }
    } else {
      aConfig.mColMaxBSize = aConfig.mKnownFeasibleBSize;
    }
    if (!skip) {
      // If our height is unconstrained, make sure that the last column is
      // allowed to have arbitrary height here, even though we were balancing.
      // Otherwise we'd have to split, and it's not clear what we'd do with
      // that.
      AddStateBits(NS_FRAME_IS_DIRTY);
      feasible = ReflowColumns(aDesiredSize, aReflowInput, aStatus, aConfig,
                               availableContentBSize == NS_UNCONSTRAINEDSIZE,
                               &aOutMargin, aColData);
    }
  }

  aRunWasFeasible = feasible;
}

void
nsColumnSetFrame::Reflow(nsPresContext*           aPresContext,
                         ReflowOutput&     aDesiredSize,
                         const ReflowInput& aReflowInput,
                         nsReflowStatus&          aStatus)
{
  MarkInReflow();
  // Don't support interruption in columns
  nsPresContext::InterruptPreventer noInterrupts(aPresContext);

  DO_GLOBAL_REFLOW_COUNT("nsColumnSetFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);

  // Initialize OUT parameter
  aStatus.Reset();

  // Our children depend on our block-size if we have a fixed block-size.
  if (aReflowInput.ComputedBSize() != NS_AUTOHEIGHT) {
    AddStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);
  } else {
    RemoveStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);
  }

#ifdef DEBUG
  nsFrameList::Enumerator oc(GetChildList(kOverflowContainersList));
  for (; !oc.AtEnd(); oc.Next()) {
    MOZ_ASSERT(!IS_TRUE_OVERFLOW_CONTAINER(oc.get()));
  }
  nsFrameList::Enumerator eoc(GetChildList(kExcessOverflowContainersList));
  for (; !eoc.AtEnd(); eoc.Next()) {
    MOZ_ASSERT(!IS_TRUE_OVERFLOW_CONTAINER(eoc.get()));
  }
#endif

  nsOverflowAreas ocBounds;
  nsReflowStatus ocStatus;
  if (GetPrevInFlow()) {
    ReflowOverflowContainerChildren(aPresContext, aReflowInput, ocBounds, 0,
                                    ocStatus);
  }

  //------------ Handle Incremental Reflow -----------------

  // If inline size is unconstrained, set aForceAuto to true to allow
  // the columns to expand in the inline direction. (This typically
  // happens in orthogonal flows where the inline direction is the
  // container's block direction).
  ReflowConfig config =
    ChooseColumnStrategy(aReflowInput,
                         aReflowInput.ComputedISize() == NS_UNCONSTRAINEDSIZE);

  // If balancing, then we allow the last column to grow to unbounded
  // height during the first reflow. This gives us a way to estimate
  // what the average column height should be, because we can measure
  // the heights of all the columns and sum them up. But don't do this
  // if we have a next in flow because we don't want to suck all its
  // content back here and then have to push it out again!
  nsIFrame* nextInFlow = GetNextInFlow();
  bool unboundedLastColumn = config.mIsBalancing && !nextInFlow;
  nsCollapsingMargin carriedOutBottomMargin;
  ColumnBalanceData colData;
  colData.mHasExcessBSize = false;

  bool feasible = ReflowColumns(aDesiredSize, aReflowInput, aStatus, config,
                                unboundedLastColumn, &carriedOutBottomMargin,
                                colData);

  // If we're not balancing, then we're already done, since we should have
  // reflown all of our children, and there is no need for a binary search to
  // determine proper column height.
  if (config.mIsBalancing && !aPresContext->HasPendingInterrupt()) {
    FindBestBalanceBSize(aReflowInput, aPresContext, config, colData,
                          aDesiredSize, carriedOutBottomMargin,
                          unboundedLastColumn, feasible, aStatus);
  }

  if (aPresContext->HasPendingInterrupt() &&
      aReflowInput.AvailableBSize() == NS_UNCONSTRAINEDSIZE) {
    // In this situation, we might be lying about our reflow status, because
    // our last kid (the one that got interrupted) was incomplete.  Fix that.
    aStatus.Reset();
  }

  NS_ASSERTION(aStatus.IsFullyComplete() ||
               aReflowInput.AvailableBSize() != NS_UNCONSTRAINEDSIZE,
               "Column set should be complete if the available block-size is unconstrained");

  // Merge overflow container bounds and status.
  aDesiredSize.mOverflowAreas.UnionWith(ocBounds);
  aStatus.MergeCompletionStatusFrom(ocStatus);

  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize, aReflowInput, aStatus, false);

  aDesiredSize.mCarriedOutBEndMargin = carriedOutBottomMargin;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

void
nsColumnSetFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                   const nsRect&           aDirtyRect,
                                   const nsDisplayListSet& aLists)
{
  DisplayBorderBackgroundOutline(aBuilder, aLists);

  if (IsVisibleForPainting(aBuilder)) {
    aLists.BorderBackground()->
      AppendNewToTop(new (aBuilder)nsDisplayColumnRule(aBuilder, this));
  }

  // Our children won't have backgrounds so it doesn't matter where we put them.
  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    BuildDisplayListForChild(aBuilder, e.get(), aDirtyRect, aLists);
  }
}

void
nsColumnSetFrame::AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult)
{
  // Everything in mFrames is continuations of the first thing in mFrames.
  nsIFrame* column = mFrames.FirstChild();

  // We might not have any columns, apparently?
  if (!column) {
    return;
  }

  MOZ_ASSERT(column->StyleContext()->GetPseudo() ==
               nsCSSAnonBoxes::columnContent,
             "What sort of child is this?");
  aResult.AppendElement(OwnedAnonBox(column));
}

#ifdef DEBUG
void
nsColumnSetFrame::SetInitialChildList(ChildListID     aListID,
                                      nsFrameList&    aChildList)
{
  MOZ_ASSERT(aListID != kPrincipalList || aChildList.OnlyChild(),
             "initial principal child list must have exactly one child");
  nsContainerFrame::SetInitialChildList(kPrincipalList, aChildList);
}

void
nsColumnSetFrame::AppendFrames(ChildListID     aListID,
                               nsFrameList&    aFrameList)
{
  MOZ_CRASH("unsupported operation");
}

void
nsColumnSetFrame::InsertFrames(ChildListID     aListID,
                               nsIFrame*       aPrevFrame,
                               nsFrameList&    aFrameList)
{
  MOZ_CRASH("unsupported operation");
}

void
nsColumnSetFrame::RemoveFrame(ChildListID     aListID,
                              nsIFrame*       aOldFrame)
{
  MOZ_CRASH("unsupported operation");
}
#endif
