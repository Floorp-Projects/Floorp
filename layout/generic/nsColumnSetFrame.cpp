/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for css3 multi-column layout */

#include "nsColumnSetFrame.h"

#include "mozilla/ColumnUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/ToString.h"
#include "nsCSSRendering.h"

using namespace mozilla;
using namespace mozilla::layout;

// To see this log, use $ MOZ_LOG=ColumnSet:4 ./mach run
static LazyLogModule sColumnSetLog("ColumnSet");
#define COLUMN_SET_LOG(msg, ...) \
  MOZ_LOG(sColumnSetLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

class nsDisplayColumnRule : public nsPaintedDisplayItem {
 public:
  nsDisplayColumnRule(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayColumnRule);
  }
  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayColumnRule)

  /**
   * Returns the frame's visual overflow rect instead of the frame's bounds.
   */
  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override {
    *aSnap = false;
    return static_cast<nsColumnSetFrame*>(mFrame)->CalculateColumnRuleBounds(
        ToReferenceFrame());
  }

  bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  NS_DISPLAY_DECL_NAME("ColumnRule", TYPE_COLUMN_RULE);

 private:
  nsTArray<nsCSSBorderRenderer> mBorderRenderers;
};

void nsDisplayColumnRule::Paint(nsDisplayListBuilder* aBuilder,
                                gfxContext* aCtx) {
  static_cast<nsColumnSetFrame*>(mFrame)->CreateBorderRenderers(
      mBorderRenderers, aCtx, GetPaintRect(), ToReferenceFrame());

  for (auto iter = mBorderRenderers.begin(); iter != mBorderRenderers.end();
       iter++) {
    iter->DrawBorders();
  }
}

bool nsDisplayColumnRule::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  RefPtr<gfxContext> screenRefCtx = gfxContext::CreateOrNull(
      gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget().get());

  static_cast<nsColumnSetFrame*>(mFrame)->CreateBorderRenderers(
      mBorderRenderers, screenRefCtx, GetPaintRect(), ToReferenceFrame());

  if (mBorderRenderers.IsEmpty()) {
    return true;
  }

  for (auto& renderer : mBorderRenderers) {
    renderer.CreateWebRenderCommands(this, aBuilder, aResources, aSc);
  }

  return true;
}

/**
 * Tracking issues:
 *
 * XXX cursor movement around the top and bottom of colums seems to make the
 * editor lose the caret.
 *
 * XXX should we support CSS columns applied to table elements?
 */
nsContainerFrame* NS_NewColumnSetFrame(PresShell* aPresShell,
                                       ComputedStyle* aStyle,
                                       nsFrameState aStateFlags) {
  nsColumnSetFrame* it =
      new (aPresShell) nsColumnSetFrame(aStyle, aPresShell->GetPresContext());
  it->AddStateBits(aStateFlags);
  return it;
}

NS_IMPL_FRAMEARENA_HELPERS(nsColumnSetFrame)

nsColumnSetFrame::nsColumnSetFrame(ComputedStyle* aStyle,
                                   nsPresContext* aPresContext)
    : nsContainerFrame(aStyle, aPresContext, kClassID),
      mLastBalanceBSize(NS_UNCONSTRAINEDSIZE) {}

void nsColumnSetFrame::ForEachColumnRule(
    const std::function<void(const nsRect& lineRect)>& aSetLineRect,
    const nsPoint& aPt) const {
  nsIFrame* child = mFrames.FirstChild();
  if (!child) return;  // no columns

  nsIFrame* nextSibling = child->GetNextSibling();
  if (!nextSibling) return;  // 1 column only - this means no gap to draw on

  const nsStyleColumn* colStyle = StyleColumn();
  nscoord ruleWidth = colStyle->GetComputedColumnRuleWidth();
  if (!ruleWidth) return;

  WritingMode wm = GetWritingMode();
  bool isVertical = wm.IsVertical();
  bool isRTL = wm.IsBidiRTL();

  nsRect contentRect = GetContentRectRelativeToSelf() + aPt;
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

nsRect nsColumnSetFrame::CalculateColumnRuleBounds(
    const nsPoint& aOffset) const {
  nsRect combined;
  ForEachColumnRule(
      [&combined](const nsRect& aLineRect) {
        combined = combined.Union(aLineRect);
      },
      aOffset);
  return combined;
}

void nsColumnSetFrame::CreateBorderRenderers(
    nsTArray<nsCSSBorderRenderer>& aBorderRenderers, gfxContext* aCtx,
    const nsRect& aDirtyRect, const nsPoint& aPt) {
  WritingMode wm = GetWritingMode();
  bool isVertical = wm.IsVertical();
  const nsStyleColumn* colStyle = StyleColumn();
  StyleBorderStyle ruleStyle;

  // Per spec, inset => ridge and outset => groove
  if (colStyle->mColumnRuleStyle == StyleBorderStyle::Inset)
    ruleStyle = StyleBorderStyle::Ridge;
  else if (colStyle->mColumnRuleStyle == StyleBorderStyle::Outset)
    ruleStyle = StyleBorderStyle::Groove;
  else
    ruleStyle = colStyle->mColumnRuleStyle;

  nscoord ruleWidth = colStyle->GetComputedColumnRuleWidth();
  if (!ruleWidth) return;

  aBorderRenderers.Clear();
  nscolor ruleColor =
      GetVisitedDependentColor(&nsStyleColumn::mColumnRuleColor);

  nsPresContext* presContext = PresContext();
  // In order to re-use a large amount of code, we treat the column rule as a
  // border. We create a new border style object and fill in all the details of
  // the column rule as the left border. PaintBorder() does all the rendering
  // for us, so we not only save an enormous amount of code but we'll support
  // all the line styles that we support on borders!
  nsStyleBorder border(*presContext->Document());
  Sides skipSides;
  if (isVertical) {
    border.SetBorderWidth(eSideTop, ruleWidth);
    border.SetBorderStyle(eSideTop, ruleStyle);
    border.mBorderTopColor = StyleColor::FromColor(ruleColor);
    skipSides |= mozilla::SideBits::eLeftRight;
    skipSides |= mozilla::SideBits::eBottom;
  } else {
    border.SetBorderWidth(eSideLeft, ruleWidth);
    border.SetBorderStyle(eSideLeft, ruleStyle);
    border.mBorderLeftColor = StyleColor::FromColor(ruleColor);
    skipSides |= mozilla::SideBits::eTopBottom;
    skipSides |= mozilla::SideBits::eRight;
  }
  // If we use box-decoration-break: slice (the default), the border
  // renderers will require clipping if we have continuations (see the
  // aNeedsClip parameter to ConstructBorderRenderer in nsCSSRendering).
  //
  // Since it doesn't matter which box-decoration-break we use since
  // we're only drawing borders (and not border-images), use 'clone'.
  border.mBoxDecorationBreak = StyleBoxDecorationBreak::Clone;

  ForEachColumnRule(
      [&](const nsRect& aLineRect) {
        // Assert that we're not drawing a border-image here; if we were, we
        // couldn't ignore the ImgDrawResult that PaintBorderWithStyleBorder
        // returns.
        MOZ_ASSERT(border.mBorderImageSource.IsNone());

        gfx::DrawTarget* dt = aCtx ? aCtx->GetDrawTarget() : nullptr;
        bool borderIsEmpty = false;
        Maybe<nsCSSBorderRenderer> br =
            nsCSSRendering::CreateBorderRendererWithStyleBorder(
                presContext, dt, this, aDirtyRect, aLineRect, border, Style(),
                &borderIsEmpty, skipSides);
        if (br.isSome()) {
          MOZ_ASSERT(!borderIsEmpty);
          aBorderRenderers.AppendElement(br.value());
        }
      },
      aPt);
}

static nscoord GetAvailableContentISize(const ReflowInput& aReflowInput) {
  if (aReflowInput.AvailableISize() == NS_UNCONSTRAINEDSIZE) {
    return NS_UNCONSTRAINEDSIZE;
  }

  WritingMode wm = aReflowInput.GetWritingMode();
  nscoord borderPaddingISize =
      aReflowInput.ComputedLogicalBorderPadding().IStartEnd(wm);
  return std::max(0, aReflowInput.AvailableISize() - borderPaddingISize);
}

static uint32_t ColumnBalancingDepth(const ReflowInput& aReflowInput,
                                     uint32_t aMaxDepth) {
  uint32_t depth = 0;
  for (const ReflowInput* ri = aReflowInput.mParentReflowInput;
       ri && depth < aMaxDepth; ri = ri->mParentReflowInput) {
    if (ri->mFlags.mIsColumnBalancing) {
      ++depth;
    }
  }
  return depth;
}

nsColumnSetFrame::ReflowConfig nsColumnSetFrame::ChooseColumnStrategy(
    const ReflowInput& aReflowInput, bool aForceAuto = false) const {
  const nsStyleColumn* colStyle = StyleColumn();
  nscoord availContentISize = GetAvailableContentISize(aReflowInput);
  if (aReflowInput.ComputedISize() != NS_UNCONSTRAINEDSIZE) {
    availContentISize = aReflowInput.ComputedISize();
  }

  nscoord colBSize = aReflowInput.AvailableBSize();
  nscoord colGap =
      ColumnUtils::GetColumnGap(this, aReflowInput.ComputedISize());
  int32_t numColumns = colStyle->mColumnCount;

  // If column-fill is set to 'balance', then we want to balance the columns.
  bool isBalancing =
      colStyle->mColumnFill == StyleColumnFill::Balance && !aForceAuto;
  if (isBalancing) {
    const uint32_t kMaxNestedColumnBalancingDepth = 2;
    const uint32_t balancingDepth =
        ColumnBalancingDepth(aReflowInput, kMaxNestedColumnBalancingDepth);
    if (balancingDepth == kMaxNestedColumnBalancingDepth) {
      isBalancing = false;
      numColumns = 1;
    }
  }

  nscoord colISize;
  // In vertical writing-mode, "column-width" (inline size) will actually be
  // physical height, but its CSS name is still column-width.
  if (colStyle->mColumnWidth.IsLength()) {
    colISize =
        ColumnUtils::ClampUsedColumnWidth(colStyle->mColumnWidth.AsLength());
    NS_ASSERTION(colISize >= 0, "negative column width");
    // Reduce column count if necessary to make columns fit in the
    // available width. Compute max number of columns that fit in
    // availContentISize, satisfying colGap*(maxColumns - 1) +
    // colISize*maxColumns <= availContentISize
    if (availContentISize != NS_UNCONSTRAINEDSIZE && colGap + colISize > 0 &&
        numColumns > 0) {
      // This expression uses truncated rounding, which is what we
      // want
      int32_t maxColumns =
          std::min(nscoord(nsStyleColumn::kMaxColumnCount),
                   (availContentISize + colGap) / (colGap + colISize));
      numColumns = std::max(1, std::min(numColumns, maxColumns));
    }
  } else if (numColumns > 0 && availContentISize != NS_UNCONSTRAINEDSIZE) {
    nscoord iSizeMinusGaps = availContentISize - colGap * (numColumns - 1);
    colISize = iSizeMinusGaps / numColumns;
  } else {
    colISize = NS_UNCONSTRAINEDSIZE;
  }
  // Take care of the situation where there's only one column but it's
  // still too wide
  colISize = std::max(1, std::min(colISize, availContentISize));

  nscoord expectedISizeLeftOver = 0;

  if (colISize != NS_UNCONSTRAINEDSIZE &&
      availContentISize != NS_UNCONSTRAINEDSIZE) {
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
        numColumns =
            std::min(nscoord(nsStyleColumn::kMaxColumnCount), numColumns);
      }
      if (numColumns <= 0) {
        numColumns = 1;
      }
    }

    // Compute extra space and divide it among the columns
    nscoord extraSpace =
        std::max(0, availContentISize -
                        (colISize * numColumns + colGap * (numColumns - 1)));
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
    // CSS Fragmentation spec says, "To guarantee progress, fragmentainers are
    // assumed to have a minimum block size of 1px regardless of their used
    // size." https://drafts.csswg.org/css-break/#breaking-rules
    //
    // Note: we don't enforce the minimum block-size during balancing because
    // this affects the result. If a balancing column container or its
    // next-in-flows has zero block-size, it eventually gives up balancing, and
    // ends up here.
    colBSize = std::max(colBSize, nsPresContext::CSSPixelsToAppUnits(1));
  }

  ReflowConfig config;
  config.mUsedColCount = numColumns;
  config.mColISize = colISize;
  config.mExpectedISizeLeftOver = expectedISizeLeftOver;
  config.mColGap = colGap;
  config.mColMaxBSize = colBSize;
  config.mIsBalancing = isBalancing;
  config.mForceAuto = aForceAuto;
  config.mKnownFeasibleBSize = NS_UNCONSTRAINEDSIZE;
  config.mKnownInfeasibleBSize = 0;

  COLUMN_SET_LOG(
      "%s: this=%p, mUsedColCount=%d, mColISize=%d, "
      "mExpectedISizeLeftOver=%d, mColGap=%d, mColMaxBSize=%d, mIsBalancing=%d",
      __func__, this, config.mUsedColCount, config.mColISize,
      config.mExpectedISizeLeftOver, config.mColGap, config.mColMaxBSize,
      config.mIsBalancing);

  return config;
}

static void MarkPrincipalChildrenDirty(nsIFrame* aFrame) {
  for (nsIFrame* childFrame : aFrame->PrincipalChildList()) {
    childFrame->MarkSubtreeDirty();
  }
}

nsColumnSetFrame::ColumnBalanceData nsColumnSetFrame::ReflowColumns(
    ReflowOutput& aDesiredSize, const ReflowInput& aReflowInput,
    nsReflowStatus& aReflowStatus, ReflowConfig& aConfig,
    bool aUnboundedLastColumn) {
  const ColumnBalanceData colData = ReflowChildren(
      aDesiredSize, aReflowInput, aReflowStatus, aConfig, aUnboundedLastColumn);

  if (!colData.mHasExcessBSize) {
    return colData;
  }

  aConfig = ChooseColumnStrategy(aReflowInput, true);

  // We need to reflow our children again one last time, otherwise we might
  // end up with a stale column block-size for some of our columns, since we
  // bailed out of balancing.
  return ReflowChildren(aDesiredSize, aReflowInput, aReflowStatus, aConfig,
                        aUnboundedLastColumn);
}

static void MoveChildTo(nsIFrame* aChild, LogicalPoint aOrigin, WritingMode aWM,
                        const nsSize& aContainerSize) {
  if (aChild->GetLogicalPosition(aWM, aContainerSize) == aOrigin) {
    return;
  }

  aChild->SetPosition(aWM, aOrigin, aContainerSize);
  nsContainerFrame::PlaceFrameView(aChild);
}

nscoord nsColumnSetFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord iSize = 0;
  DISPLAY_MIN_INLINE_SIZE(this, iSize);

  if (mFrames.FirstChild()) {
    // We want to ignore this in the case that we're size contained
    // because our children should not contribute to our
    // intrinsic size.
    iSize = mFrames.FirstChild()->GetMinISize(aRenderingContext);
  }
  const nsStyleColumn* colStyle = StyleColumn();
  if (colStyle->mColumnWidth.IsLength()) {
    nscoord colISize =
        ColumnUtils::ClampUsedColumnWidth(colStyle->mColumnWidth.AsLength());
    // As available width reduces to zero, we reduce our number of columns
    // to one, and don't enforce the column width, so just return the min
    // of the child's min-width with any specified column width.
    iSize = std::min(iSize, colISize);
  } else {
    NS_ASSERTION(colStyle->mColumnCount > 0,
                 "column-count and column-width can't both be auto");
    // As available width reduces to zero, we still have mColumnCount columns,
    // so compute our minimum size based on the number of columns and their gaps
    // and minimum per-column size.
    nscoord colGap = ColumnUtils::GetColumnGap(this, NS_UNCONSTRAINEDSIZE);
    iSize = ColumnUtils::IntrinsicISize(colStyle->mColumnCount, colGap, iSize);
  }
  // XXX count forced column breaks here? Maybe we should return the child's
  // min-width times the minimum number of columns.
  return iSize;
}

nscoord nsColumnSetFrame::GetPrefISize(gfxContext* aRenderingContext) {
  // Our preferred width is our desired column width, if specified, otherwise
  // the child's preferred width, times the number of columns, plus the width
  // of any required column gaps
  // XXX what about forced column breaks here?
  nscoord result = 0;
  DISPLAY_PREF_INLINE_SIZE(this, result);
  const nsStyleColumn* colStyle = StyleColumn();

  nscoord colISize;
  if (colStyle->mColumnWidth.IsLength()) {
    colISize =
        ColumnUtils::ClampUsedColumnWidth(colStyle->mColumnWidth.AsLength());
  } else if (mFrames.FirstChild()) {
    // We want to ignore this in the case that we're size contained
    // because our children should not contribute to our
    // intrinsic size.
    colISize = mFrames.FirstChild()->GetPrefISize(aRenderingContext);
  } else {
    colISize = 0;
  }

  // If column-count is auto, assume one column.
  uint32_t numColumns =
      colStyle->mColumnCount == nsStyleColumn::kColumnCountAuto
          ? 1
          : colStyle->mColumnCount;
  nscoord colGap = ColumnUtils::GetColumnGap(this, NS_UNCONSTRAINEDSIZE);
  result = ColumnUtils::IntrinsicISize(numColumns, colGap, colISize);
  return result;
}

nsColumnSetFrame::ColumnBalanceData nsColumnSetFrame::ReflowChildren(
    ReflowOutput& aDesiredSize, const ReflowInput& aReflowInput,
    nsReflowStatus& aStatus, const ReflowConfig& aConfig,
    bool aUnboundedLastColumn) {
  ColumnBalanceData colData;
  bool allFit = true;
  WritingMode wm = GetWritingMode();
  const bool isRTL = wm.IsBidiRTL();
  const bool shrinkingBSize = mLastBalanceBSize > aConfig.mColMaxBSize;
  const bool changingBSize = mLastBalanceBSize != aConfig.mColMaxBSize;

  COLUMN_SET_LOG(
      "%s: Doing column reflow pass: mLastBalanceBSize=%d,"
      " mColMaxBSize=%d, RTL=%d, mUsedColCount=%d,"
      " mColISize=%d, mColGap=%d",
      __func__, mLastBalanceBSize, aConfig.mColMaxBSize, isRTL,
      aConfig.mUsedColCount, aConfig.mColISize, aConfig.mColGap);

  DrainOverflowColumns();

  if (changingBSize) {
    mLastBalanceBSize = aConfig.mColMaxBSize;
    // XXX Seems like this could fire if incremental reflow pushed the column
    // set down so we reflow incrementally with a different available height.
    // We need a way to do an incremental reflow and be sure availableHeight
    // changes are taken account of! Right now I think block frames with
    // absolute children might exit early.
    /*
    NS_ASSERTION(
        aKidReason != eReflowReason_Incremental,
        "incremental reflow should not have changed the balance height");
    */
  }

  nsRect contentRect(0, 0, 0, 0);
  nsOverflowAreas overflowRects;

  nsIFrame* child = mFrames.FirstChild();
  LogicalPoint childOrigin(wm, 0, 0);

  // In vertical-rl mode, columns will not be correctly placed if the
  // reflowInput's ComputedWidth() is UNCONSTRAINED (in which case we'll get
  // a containerSize.width of zero here). In that case, the column positions
  // will be adjusted later, after our correct contentSize is known.
  //
  // When column-span is enabled, containerSize.width is always constrained.
  // However, for RTL, we need to adjust the column positions as well after our
  // correct containerSize is known.
  nsSize containerSize = aReflowInput.ComputedSizeAsContainerIfConstrained();

  const nscoord computedBSize =
      aReflowInput.mParentReflowInput->ComputedBSize();
  int columnCount = 0;
  nscoord contentBEnd = 0;
  bool reflowNext = false;

  while (child) {
    const bool isMeasuringFeasibleContentBSize =
        aUnboundedLastColumn && columnCount == aConfig.mUsedColCount - 1 &&
        aConfig.mIsBalancing;

    // Try to skip reflowing the child. We can't skip if the child is dirty. We
    // also can't skip if the next column is dirty, because the next column's
    // first line(s) might be pullable back to this column. We can't skip if
    // it's the last child because we need to obtain the bottom margin. We can't
    // skip if this is the last column and we're supposed to assign unbounded
    // block-size to it, because that could change the available block-size from
    // the last time we reflowed it and we should try to pull all the
    // content from its next sibling. (Note that it might be the last
    // column, but not be the last child because the desired number of columns
    // has changed.)
    bool skipIncremental =
        !aReflowInput.ShouldReflowAllKids() && !NS_SUBTREE_DIRTY(child) &&
        child->GetNextSibling() && !isMeasuringFeasibleContentBSize &&
        !NS_SUBTREE_DIRTY(child->GetNextSibling());

    // If column-fill is auto (not the default), then we might need to
    // move content between columns for any change in column block-size.
    //
    // The same is true if we have a non-'auto' computed block-size.
    //
    // FIXME: It's not clear to me why it's *ever* valid to have
    // skipIncremental be true when changingBSize is true, since it
    // seems like a child broken over multiple columns might need to
    // change the size of the fragment in each column.
    if (skipIncremental && changingBSize &&
        (StyleColumn()->mColumnFill == StyleColumnFill::Auto ||
         computedBSize != NS_UNCONSTRAINEDSIZE)) {
      skipIncremental = false;
    }
    // If we need to pull up content from the prev-in-flow then this is not just
    // a block-size shrink. The prev in flow will have set the dirty bit.
    // Check the overflow rect YMost instead of just the child's content
    // block-size. The child may have overflowing content that cares about the
    // available block-size boundary. (It may also have overflowing content that
    // doesn't care about the available block-size boundary, but if so, too bad,
    // this optimization is defeated.) We want scrollable overflow here since
    // this is a calculation that affects layout.
    if (skipIncremental && shrinkingBSize) {
      switch (wm.GetBlockDir()) {
        case WritingMode::eBlockTB:
          if (child->GetScrollableOverflowRect().YMost() >
              aConfig.mColMaxBSize) {
            skipIncremental = false;
          }
          break;
        case WritingMode::eBlockLR:
          if (child->GetScrollableOverflowRect().XMost() >
              aConfig.mColMaxBSize) {
            skipIncremental = false;
          }
          break;
        case WritingMode::eBlockRL:
          // XXX not sure how to handle this, so for now just don't attempt
          // the optimization
          skipIncremental = false;
          break;
        default:
          MOZ_ASSERT_UNREACHABLE("unknown block direction");
          break;
      }
    }

    nscoord childContentBEnd = 0;
    if (!reflowNext && skipIncremental) {
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

      COLUMN_SET_LOG("%s: Skipping child #%d %p (incremental %d): status=%s",
                     __func__, columnCount, child, skipIncremental,
                     ToString(aStatus).c_str());
    } else {
      LogicalSize availSize(wm, aConfig.mColISize, aConfig.mColMaxBSize);
      if (isMeasuringFeasibleContentBSize) {
        availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;

        COLUMN_SET_LOG(
            "%s: Measuring content block-size, change available block-size "
            "from %d to %d",
            __func__, aConfig.mColMaxBSize, availSize.BSize(wm));
      }

      if (reflowNext) {
        child->MarkSubtreeDirty();
      }

      LogicalSize kidCBSize(wm, availSize.ISize(wm), computedBSize);
      ReflowInput kidReflowInput(PresContext(), aReflowInput, child, availSize,
                                 Some(kidCBSize));
      kidReflowInput.mFlags.mIsTopOfPage = true;
      kidReflowInput.mFlags.mTableIsSplittable = false;
      kidReflowInput.mFlags.mIsColumnBalancing = aConfig.mIsBalancing;

      // We need to reflow any float placeholders, even if our column block-size
      // hasn't changed.
      kidReflowInput.mFlags.mMustReflowPlaceholders = !changingBSize;

      COLUMN_SET_LOG(
          "%s: Reflowing child #%d %p: availSize=(%d,%d), kidCBSize=(%d,%d)",
          __func__, columnCount, child, availSize.ISize(wm),
          availSize.BSize(wm), kidCBSize.ISize(wm), kidCBSize.BSize(wm));

      // Note if the column's next in flow is not being changed by this
      // incremental reflow. This may allow the current column to avoid trying
      // to pull lines from the next column.
      if (child->GetNextSibling() && !(GetStateBits() & NS_FRAME_IS_DIRTY) &&
          !(child->GetNextSibling()->GetStateBits() & NS_FRAME_IS_DIRTY)) {
        kidReflowInput.mFlags.mNextInFlowUntouched = true;
      }

      ReflowOutput kidDesiredSize(wm);

      // XXX it would be cool to consult the float manager for the
      // previous block to figure out the region of floats from the
      // previous column that extend into this column, and subtract
      // that region from the new float manager.  So you could stick a
      // really big float in the first column and text in following
      // columns would flow around it.

      // Reflow the frame
      LogicalPoint origin(
          wm,
          childOrigin.I(wm) + kidReflowInput.ComputedLogicalMargin().IStart(wm),
          childOrigin.B(wm) +
              kidReflowInput.ComputedLogicalMargin().BStart(wm));
      aStatus.Reset();
      ReflowChild(child, PresContext(), kidDesiredSize, kidReflowInput, wm,
                  origin, containerSize, ReflowChildFlags::Default, aStatus);

      reflowNext = aStatus.NextInFlowNeedsReflow();

      COLUMN_SET_LOG(
          "%s: Reflowed child #%d %p: status=%s,"
          " desiredSize=(%d,%d), CarriedOutBEndMargin=%d (ignored)",
          __func__, columnCount, child, ToString(aStatus).c_str(),
          kidDesiredSize.ISize(wm), kidDesiredSize.BSize(wm),
          kidDesiredSize.mCarriedOutBEndMargin.get());

      // The carried-out block-end margin of column content might be non-zero
      // when we try to find the best column balancing block size, but it should
      // never affect the size column set nor be further carried out. Set it to
      // zero.
      //
      // FIXME: For some types of fragmentation, we should carry the margin into
      // the next column. Also see
      // https://drafts.csswg.org/css-break-4/#break-margins
      //
      // FIXME: This should never happen for the last column, since it should be
      // a margin root; see nsBlockFrame::IsMarginRoot(). However, sometimes the
      // last column has an empty continuation while searching for the best
      // column balancing bsize, which prevents the last column from being a
      // margin root.
      kidDesiredSize.mCarriedOutBEndMargin.Zero();

      NS_FRAME_TRACE_REFLOW_OUT("Column::Reflow", aStatus);

      FinishReflowChild(child, PresContext(), kidDesiredSize, &kidReflowInput,
                        wm, childOrigin, containerSize,
                        ReflowChildFlags::Default);

      childContentBEnd = nsLayoutUtils::CalculateContentBEnd(wm, child);
      if (childContentBEnd > aConfig.mColMaxBSize) {
        allFit = false;
      }
      if (childContentBEnd > availSize.BSize(wm)) {
        colData.mMaxOverflowingBSize =
            std::max(childContentBEnd, colData.mMaxOverflowingBSize);
      }
    }

    contentRect.UnionRect(contentRect, child->GetRect());

    ConsiderChildOverflow(overflowRects, child);
    contentBEnd = std::max(contentBEnd, childContentBEnd);
    colData.mLastBSize = childContentBEnd;
    colData.mSumBSize += childContentBEnd;

    // Build a continuation column if necessary
    nsIFrame* kidNextInFlow = child->GetNextInFlow();

    if (aStatus.IsFullyComplete() && !aStatus.IsTruncated()) {
      NS_ASSERTION(!kidNextInFlow, "next in flow should have been deleted");
      child = nullptr;
      break;
    }

    // Make sure that the column has a next-in-flow. If not, we must
    // create one to hold the overflowing stuff, even if we're just
    // going to put it on our overflow list and let *our*
    // next in flow handle it.
    if (!kidNextInFlow) {
      NS_ASSERTION(aStatus.NextInFlowNeedsReflow(),
                   "We have to create a continuation, but the block doesn't "
                   "want us to reflow it?");

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
    } else if (kidNextInFlow->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) {
      aStatus.SetNextInFlowNeedsReflow();
      reflowNext = true;
      kidNextInFlow->RemoveStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
    }

    if ((contentBEnd > aReflowInput.ComputedMaxBSize() ||
         contentBEnd > aReflowInput.ComputedBSize() ||
         contentBEnd > aReflowInput.mCBReflowInput->ComputedMaxBSize()) &&
        aConfig.mIsBalancing) {
      // We overflowed vertically, but have not exceeded the number of
      // columns. We're going to go into overflow columns now, so balancing
      // no longer applies.
      colData.mHasExcessBSize = true;
    }

    // We have reached the maximum number of columns. If we are balancing, stop
    // this reflow and continue finding the optimal balancing block-size.
    //
    // Otherwise, i.e. we are not balancing, stop this reflow and let the parent
    // of our multicol container create a next-in-flow if all of the following
    // conditions are met.
    //
    // 1) We fill columns sequentially by the request of the style, not by our
    // internal needs, i.e. aConfig.mForceAuto is false.
    //
    // We don't want to stop this reflow when we force fill the columns
    // sequentially. We usually go into this mode when giving up balancing, and
    // this is the last resort to fit all our children by creating overflow
    // columns.
    //
    // 2) In a fragmented context, our multicol container still has block-size
    // left for its next-in-flow, i.e.
    // aReflowInput.mFlags.mColumnSetWrapperHasNoBSizeLeft is false.
    //
    // Note that in a continuous context, i.e. our multicol container's
    // available block-size is unconstrained, if it has a fixed block-size
    // mColumnSetWrapperHasNoBSizeLeft is always true because nothing stops it
    // from applying all its block-size in the first-in-flow. Otherwise, i.e.
    // our multicol container has an unconstrained block-size, we shouldn't be
    // here because all our children should fit in the very first column even if
    // mColumnSetWrapperHasNoBSizeLeft is false.
    //
    // According to the definition of mColumnSetWrapperHasNoBSizeLeft, if the
    // bit is *not* set, either our multicol container has unconstrained
    // block-size, or it has a constrained block-size and has block-size left
    // for its next-in-flow. In either cases, the parent of our multicol
    // container can create a next-in-flow for the container that guaranteed to
    // have non-zero block-size for the container's children.
    //
    // Put simply, if either one of the above conditions is not met, we are
    // going to create more overflow columns until all our children are fit.
    if (columnCount >= aConfig.mUsedColCount - 1 &&
        (aConfig.mIsBalancing ||
         (!aConfig.mForceAuto &&
          !aReflowInput.mFlags.mColumnSetWrapperHasNoBSizeLeft))) {
      NS_ASSERTION(aConfig.mIsBalancing ||
                       aReflowInput.AvailableBSize() != NS_UNCONSTRAINEDSIZE,
                   "Why are we here if we have unlimited block-size to fill "
                   "columns sequentially.");

      // No more columns allowed here. Stop.
      aStatus.SetNextInFlowNeedsReflow();
      kidNextInFlow->MarkSubtreeDirty();
      // Move any of our leftover columns to our overflow list. Our
      // next-in-flow will eventually pick them up.
      const nsFrameList& continuationColumns = mFrames.RemoveFramesAfter(child);
      if (continuationColumns.NotEmpty()) {
        SetOverflowFrames(continuationColumns);
      }
      child = nullptr;

      COLUMN_SET_LOG("%s: We are not going to create overflow columns.",
                     __func__);
      break;
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
    ++columnCount;

    if (child) {
      childOrigin.I(wm) += aConfig.mColISize + aConfig.mColGap;

      COLUMN_SET_LOG("%s: Next childOrigin.iCoord=%d", __func__,
                     childOrigin.I(wm));
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
      child->MarkSubtreeDirty();
    }
  }

  colData.mMaxBSize = contentBEnd;
  LogicalSize contentSize = LogicalSize(wm, contentRect.Size());
  contentSize.BSize(wm) = std::max(contentSize.BSize(wm), contentBEnd);
  mLastFrameStatus = aStatus;

  if (computedBSize != NS_UNCONSTRAINEDSIZE && !HasColumnSpanSiblings()) {
    NS_ASSERTION(aReflowInput.AvailableBSize() != NS_UNCONSTRAINEDSIZE,
                 "Available block-size should be constrained because it's "
                 "restricted by the computed block-size when our reflow "
                 "input is created in nsBlockFrame::ReflowBlockFrame()!");

    // If a) our parent ColumnSetWrapper has constrained block-size
    // (nsBlockFrame::ReflowBlockFrame() applies the block-size constraint
    // when creating BlockReflowInput for ColumnSetFrame); and b) we are the
    // sole ColumnSet or the last ColumnSet continuation split by column-spans
    // in a ColumnSetWrapper, extend our block-size to consume the available
    // block-size so that the column-rules are drawn to the content block-end
    // edge of the multicol container.
    contentSize.BSize(wm) =
        std::max(contentSize.BSize(wm), aReflowInput.AvailableBSize());
  }

  aDesiredSize.SetSize(wm, contentSize);
  aDesiredSize.mOverflowAreas = overflowRects;
  aDesiredSize.UnionOverflowAreasWithDesiredBounds();

  // In vertical-rl mode, make a second pass if necessary to reposition the
  // columns with the correct container width. (In other writing modes,
  // correct containerSize was not required for column positioning so we don't
  // need this fixup.)
  //
  // RTL column positions also depend on ColumnSet's actual contentSize. We need
  // this fixup, too.
  if ((wm.IsVerticalRL() || isRTL) &&
      containerSize.width != contentSize.Width(wm)) {
    const nsSize finalContainerSize = aDesiredSize.PhysicalSize();
    nsOverflowAreas overflowRects;
    for (nsIFrame* child : mFrames) {
      // Get the logical position as set previously using a provisional or
      // dummy containerSize, and reset with the correct container size.
      child->SetPosition(wm, child->GetLogicalPosition(wm, containerSize),
                         finalContainerSize);
      ConsiderChildOverflow(overflowRects, child);
    }
    aDesiredSize.mOverflowAreas = overflowRects;
    aDesiredSize.UnionOverflowAreasWithDesiredBounds();
  }

  colData.mFeasible =
      allFit && aStatus.IsFullyComplete() && !aStatus.IsTruncated();
  COLUMN_SET_LOG(
      "%s: Done column reflow pass: %s, mMaxBSize=%d, mSumBSize=%d, "
      "mMaxOverflowingBSize=%d",
      __func__, colData.mFeasible ? "Feasible :)" : "Infeasible :(",
      colData.mMaxBSize, colData.mSumBSize, colData.mMaxOverflowingBSize);

  return colData;
}

void nsColumnSetFrame::DrainOverflowColumns() {
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

void nsColumnSetFrame::FindBestBalanceBSize(const ReflowInput& aReflowInput,
                                            nsPresContext* aPresContext,
                                            ReflowConfig& aConfig,
                                            ColumnBalanceData aColData,
                                            ReflowOutput& aDesiredSize,
                                            bool aUnboundedLastColumn,
                                            nsReflowStatus& aStatus) {
  const nscoord availableContentBSize = aReflowInput.AvailableBSize();

  // Termination of the algorithm below is guaranteed because
  // aConfig.knownFeasibleBSize - aConfig.knownInfeasibleBSize decreases in
  // every iteration.

  // We set this flag when we detect that we may contain a frame
  // that can break anywhere (thus foiling the linear decrease-by-one
  // search)
  bool maybeContinuousBreakingDetected = false;

  while (!aPresContext->HasPendingInterrupt()) {
    nscoord lastKnownFeasibleBSize = aConfig.mKnownFeasibleBSize;

    // Record what we learned from the last reflow
    if (aColData.mFeasible) {
      // maxBSize is feasible. Also, mLastBalanceBSize is feasible.
      aConfig.mKnownFeasibleBSize =
          std::min(aConfig.mKnownFeasibleBSize, aColData.mMaxBSize);
      aConfig.mKnownFeasibleBSize =
          std::min(aConfig.mKnownFeasibleBSize, mLastBalanceBSize);

      // Furthermore, no block-size less than the block-size of the last
      // column can ever be feasible. (We might be able to reduce the
      // block-size of a non-last column by moving content to a later column,
      // but we can't do that with the last column.)
      if (mFrames.GetLength() == aConfig.mUsedColCount) {
        aConfig.mKnownInfeasibleBSize =
            std::max(aConfig.mKnownInfeasibleBSize, aColData.mLastBSize - 1);
      }
    } else {
      aConfig.mKnownInfeasibleBSize =
          std::max(aConfig.mKnownInfeasibleBSize, mLastBalanceBSize);

      // If a column didn't fit in its available block-size, then its current
      // block-size must be the minimum block-size for unbreakable content in
      // the column, and therefore no smaller block-size can be feasible.
      aConfig.mKnownInfeasibleBSize = std::max(
          aConfig.mKnownInfeasibleBSize, aColData.mMaxOverflowingBSize - 1);

      if (aUnboundedLastColumn) {
        // The last column is unbounded, so all content got reflowed, so the
        // mMaxBSize is feasible.
        aConfig.mKnownFeasibleBSize =
            std::min(aConfig.mKnownFeasibleBSize, aColData.mMaxBSize);

        NS_ASSERTION(mLastFrameStatus.IsComplete(),
                     "Last column should be complete if the available "
                     "block-size is unconstrained!");
      }
    }

    COLUMN_SET_LOG(
        "%s: this=%p, mKnownInfeasibleBSize=%d, mKnownFeasibleBSize=%d",
        __func__, this, aConfig.mKnownInfeasibleBSize,
        aConfig.mKnownFeasibleBSize);

    if (aConfig.mKnownInfeasibleBSize >= aConfig.mKnownFeasibleBSize - 1) {
      // aConfig.mKnownFeasibleBSize is where we want to be
      break;
    }

    if (aConfig.mKnownInfeasibleBSize >= availableContentBSize) {
      // There's no feasible block-size to fit our contents. We may need to
      // reflow one more time after this loop.
      break;
    }

    if (lastKnownFeasibleBSize - aConfig.mKnownFeasibleBSize == 1) {
      // We decreased the feasible block-size by one twip only. This could
      // indicate that there is a continuously breakable child frame
      // that we are crawling through.
      maybeContinuousBreakingDetected = true;
    }

    nscoord nextGuess =
        (aConfig.mKnownFeasibleBSize + aConfig.mKnownInfeasibleBSize) / 2;
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
      nextGuess = aColData.mSumBSize / aConfig.mUsedColCount + 600;
      // Sanitize it
      nextGuess = clamped(nextGuess, aConfig.mKnownInfeasibleBSize + 1,
                          aConfig.mKnownFeasibleBSize - 1);
    } else if (aConfig.mKnownFeasibleBSize == NS_UNCONSTRAINEDSIZE) {
      // This can happen when we had a next-in-flow so we didn't
      // want to do an unbounded block-size measuring step. Let's just increase
      // from the infeasible block-size by some reasonable amount.
      nextGuess = aConfig.mKnownInfeasibleBSize * 2 + 600;
    }
    // Don't bother guessing more than our block-size constraint.
    nextGuess = std::min(availableContentBSize, nextGuess);

    COLUMN_SET_LOG("%s: Choosing next guess=%d", __func__, nextGuess);

    aConfig.mColMaxBSize = nextGuess;

    aUnboundedLastColumn = false;
    MarkPrincipalChildrenDirty(this);
    aColData =
        ReflowColumns(aDesiredSize, aReflowInput, aStatus, aConfig, false);

    if (!aConfig.mIsBalancing) {
      // Looks like we had excess block-size when balancing, so we gave up on
      // trying to balance.
      break;
    }
  }

  if (aConfig.mIsBalancing && !aColData.mFeasible &&
      !aPresContext->HasPendingInterrupt()) {
    // We need to reflow one more time at the feasible block-size to
    // get a valid layout.
    if (aConfig.mKnownInfeasibleBSize >= availableContentBSize) {
      aConfig.mColMaxBSize = availableContentBSize;
      if (mLastBalanceBSize == availableContentBSize) {
        // If we end up here, we have a constrained available content
        // block-size, and our last column's block-size exceeds it. Also, if
        // this is the first balancing iteration, the last column is given
        // unconstrained available block-size, so it has a fully complete
        // reflow status. Therefore, we always want to reflow again at the
        // available content block-size to get a valid layout and a correct
        // reflow status (likely an *incomplete* status) so that our column
        // container can be fragmented if needed.

        if (aReflowInput.mFlags.mColumnSetWrapperHasNoBSizeLeft) {
          // If our column container has a constrained block-size (either in a
          // paginated context or in a nested column container), and is going
          // to consume all its computed block-size in this fragment, then our
          // column container has no block-size left to contain our
          // next-in-flows. We have to give up balancing, and create our
          // own overflow columns.
          //
          // We don't want to create overflow columns immediately when our
          // content doesn't fit since this changes our reflow status from
          // incomplete to complete. Valid reasons include 1) the outer column
          // container might do column balancing, and it can enlarge the
          // available content block-size so that the nested one could fit its
          // content in next balancing iteration; or 2) the outer column
          // container is filling columns sequentially, and may have more
          // inline-size to create more column boxes for the nested column
          // container's next-in-flows.
          aConfig = ChooseColumnStrategy(aReflowInput, true);
        }
      }
    } else {
      aConfig.mColMaxBSize = aConfig.mKnownFeasibleBSize;
    }

    // This is our last attempt to reflow. If our column container's available
    // block-size is unconstrained, make sure that the last column is
    // allowed to have arbitrary block-size here, even though we were
    // balancing. Otherwise we'd have to split, and it's not clear what we'd
    // do with that.
    const bool forceUnboundedLastColumn =
        aReflowInput.mParentReflowInput->AvailableBSize() ==
        NS_UNCONSTRAINEDSIZE;
    MarkPrincipalChildrenDirty(this);
    ReflowColumns(aDesiredSize, aReflowInput, aStatus, aConfig,
                  forceUnboundedLastColumn);
  }
}

void nsColumnSetFrame::Reflow(nsPresContext* aPresContext,
                              ReflowOutput& aDesiredSize,
                              const ReflowInput& aReflowInput,
                              nsReflowStatus& aStatus) {
  MarkInReflow();
  // Don't support interruption in columns
  nsPresContext::InterruptPreventer noInterrupts(aPresContext);

  DO_GLOBAL_REFLOW_COUNT("nsColumnSetFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  MOZ_ASSERT(aReflowInput.mCBReflowInput->mFrame->StyleColumn()
                 ->IsColumnContainerStyle(),
             "The column container should have relevant column styles!");
  MOZ_ASSERT(aReflowInput.mParentReflowInput->mFrame->IsColumnSetWrapperFrame(),
             "The column container should be ColumnSetWrapperFrame!");
  MOZ_ASSERT(aReflowInput.ComputedLogicalBorderPadding().IsAllZero(),
             "Only the column container can have border and padding!");

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
    ReflowOverflowContainerChildren(aPresContext, aReflowInput, ocBounds,
                                    ReflowChildFlags::Default, ocStatus);
  }

  //------------ Handle Incremental Reflow -----------------

  // If inline size is unconstrained, set aForceAuto to true to allow
  // the columns to expand in the inline direction. (This typically
  // happens in orthogonal flows where the inline direction is the
  // container's block direction).
  ReflowConfig config = ChooseColumnStrategy(
      aReflowInput, aReflowInput.ComputedISize() == NS_UNCONSTRAINEDSIZE);

  // If balancing, then we allow the last column to grow to unbounded
  // block-size during the first reflow. This gives us a way to estimate
  // what the average column block-size should be, because we can measure
  // the block-size of all the columns and sum them up. But don't do this
  // if we have a next in flow because we don't want to suck all its
  // content back here and then have to push it out again!
  nsIFrame* nextInFlow = GetNextInFlow();
  bool unboundedLastColumn = config.mIsBalancing && !nextInFlow;
  const ColumnBalanceData colData = ReflowColumns(
      aDesiredSize, aReflowInput, aStatus, config, unboundedLastColumn);

  // If we're not balancing, then we're already done, since we should have
  // reflown all of our children, and there is no need for a binary search to
  // determine proper column block-size.
  if (config.mIsBalancing && !aPresContext->HasPendingInterrupt()) {
    FindBestBalanceBSize(aReflowInput, aPresContext, config, colData,
                         aDesiredSize, unboundedLastColumn, aStatus);
  }

  if (aPresContext->HasPendingInterrupt() &&
      aReflowInput.AvailableBSize() == NS_UNCONSTRAINEDSIZE) {
    // In this situation, we might be lying about our reflow status, because
    // our last kid (the one that got interrupted) was incomplete.  Fix that.
    aStatus.Reset();
  }

  NS_ASSERTION(aStatus.IsFullyComplete() ||
                   aReflowInput.AvailableBSize() != NS_UNCONSTRAINEDSIZE,
               "Column set should be complete if the available block-size is "
               "unconstrained");

  // Merge overflow container bounds and status.
  aDesiredSize.mOverflowAreas.UnionWith(ocBounds);
  aStatus.MergeCompletionStatusFrom(ocStatus);

  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize, aReflowInput,
                                 aStatus, false);

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

void nsColumnSetFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                        const nsDisplayListSet& aLists) {
  DisplayBorderBackgroundOutline(aBuilder, aLists);

  if (IsVisibleForPainting()) {
    aLists.BorderBackground()->AppendNewToTop<nsDisplayColumnRule>(aBuilder,
                                                                   this);
  }

  // Our children won't have backgrounds so it doesn't matter where we put them.
  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    BuildDisplayListForChild(aBuilder, e.get(), aLists);
  }
}

void nsColumnSetFrame::AppendDirectlyOwnedAnonBoxes(
    nsTArray<OwnedAnonBox>& aResult) {
  // Everything in mFrames is continuations of the first thing in mFrames.
  nsIFrame* column = mFrames.FirstChild();

  // We might not have any columns, apparently?
  if (!column) {
    return;
  }

  MOZ_ASSERT(column->Style()->GetPseudoType() == PseudoStyleType::columnContent,
             "What sort of child is this?");
  aResult.AppendElement(OwnedAnonBox(column));
}

#ifdef DEBUG
void nsColumnSetFrame::SetInitialChildList(ChildListID aListID,
                                           nsFrameList& aChildList) {
  MOZ_ASSERT(aListID != kPrincipalList || aChildList.OnlyChild(),
             "initial principal child list must have exactly one child");
  nsContainerFrame::SetInitialChildList(aListID, aChildList);
}

void nsColumnSetFrame::AppendFrames(ChildListID aListID,
                                    nsFrameList& aFrameList) {
  MOZ_CRASH("unsupported operation");
}

void nsColumnSetFrame::InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                                    const nsLineList::iterator* aPrevFrameLine,
                                    nsFrameList& aFrameList) {
  MOZ_CRASH("unsupported operation");
}

void nsColumnSetFrame::RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) {
  MOZ_CRASH("unsupported operation");
}
#endif
