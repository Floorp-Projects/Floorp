/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFieldSetFrame.h"

#include <algorithm>
#include "gfxContext.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Likely.h"
#include "mozilla/PresShell.h"
#include "mozilla/Maybe.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSRendering.h"
#include "nsDisplayList.h"
#include "nsGkAtoms.h"
#include "nsIFrameInlines.h"
#include "nsLayoutUtils.h"
#include "nsLegendFrame.h"
#include "nsStyleConsts.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layout;

nsContainerFrame* NS_NewFieldSetFrame(PresShell* aPresShell,
                                      ComputedStyle* aStyle) {
  return new (aPresShell) nsFieldSetFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsFieldSetFrame)

nsFieldSetFrame::nsFieldSetFrame(ComputedStyle* aStyle,
                                 nsPresContext* aPresContext)
    : nsContainerFrame(aStyle, aPresContext, kClassID),
      mLegendRect(GetWritingMode()) {
  mLegendSpace = 0;
}

nsRect nsFieldSetFrame::VisualBorderRectRelativeToSelf() const {
  WritingMode wm = GetWritingMode();
  LogicalRect r(wm, LogicalPoint(wm, 0, 0), GetLogicalSize(wm));
  nsSize containerSize = r.Size(wm).GetPhysicalSize(wm);
  if (nsIFrame* legend = GetLegend()) {
    nscoord legendSize = legend->GetLogicalSize(wm).BSize(wm);
    auto legendMargin = legend->GetLogicalUsedMargin(wm);
    nscoord legendStartMargin = legendMargin.BStart(wm);
    nscoord legendEndMargin = legendMargin.BEnd(wm);
    nscoord border = GetUsedBorder().Side(wm.PhysicalSide(eLogicalSideBStart));
    // Calculate the offset from the border area block-axis start edge needed to
    // center-align our border with the legend's border-box (in the block-axis).
    nscoord off = (legendStartMargin + legendSize / 2) - border / 2;
    // We don't want to display our border above our border area.
    if (off > nscoord(0)) {
      nscoord marginBoxSize = legendStartMargin + legendSize + legendEndMargin;
      if (marginBoxSize > border) {
        // We don't want to display our border below the legend's margin-box,
        // so we align it to the block-axis end if that happens.
        nscoord overflow = off + border - marginBoxSize;
        if (overflow > nscoord(0)) {
          off -= overflow;
        }
        r.BStart(wm) += off;
        r.BSize(wm) -= off;
      }
    }
  }
  return r.GetPhysicalRect(wm, containerSize);
}

nsIFrame* nsFieldSetFrame::GetInner() const {
  nsIFrame* last = mFrames.LastChild();
  if (last &&
      last->Style()->GetPseudoType() == PseudoStyleType::fieldsetContent) {
    return last;
  }
  MOZ_ASSERT(mFrames.LastChild() == mFrames.FirstChild());
  return nullptr;
}

nsIFrame* nsFieldSetFrame::GetLegend() const {
  if (mFrames.FirstChild() == GetInner()) {
    MOZ_ASSERT(mFrames.LastChild() == mFrames.FirstChild());
    return nullptr;
  }
  MOZ_ASSERT(mFrames.FirstChild() &&
             mFrames.FirstChild()->GetContentInsertionFrame()->IsLegendFrame());
  return mFrames.FirstChild();
}

class nsDisplayFieldSetBorder final : public nsPaintedDisplayItem {
 public:
  nsDisplayFieldSetBorder(nsDisplayListBuilder* aBuilder,
                          nsFieldSetFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayFieldSetBorder);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayFieldSetBorder() {
    MOZ_COUNT_DTOR(nsDisplayFieldSetBorder);
  }
#endif
  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  virtual nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override;
  virtual void ComputeInvalidationRegion(
      nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
      nsRegion* aInvalidRegion) const override;
  bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  NS_DISPLAY_DECL_NAME("FieldSetBorder", TYPE_FIELDSET_BORDER_BACKGROUND)
};

void nsDisplayFieldSetBorder::Paint(nsDisplayListBuilder* aBuilder,
                                    gfxContext* aCtx) {
  image::ImgDrawResult result =
      static_cast<nsFieldSetFrame*>(mFrame)->PaintBorder(
          aBuilder, *aCtx, ToReferenceFrame(), GetPaintRect());

  nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, result);
}

nsDisplayItemGeometry* nsDisplayFieldSetBorder::AllocateGeometry(
    nsDisplayListBuilder* aBuilder) {
  return new nsDisplayItemGenericImageGeometry(this, aBuilder);
}

void nsDisplayFieldSetBorder::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  auto geometry =
      static_cast<const nsDisplayItemGenericImageGeometry*>(aGeometry);

  if (aBuilder->ShouldSyncDecodeImages() &&
      geometry->ShouldInvalidateToSyncDecodeImages()) {
    bool snap;
    aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
  }

  nsDisplayItem::ComputeInvalidationRegion(aBuilder, aGeometry, aInvalidRegion);
}

nsRect nsDisplayFieldSetBorder::GetBounds(nsDisplayListBuilder* aBuilder,
                                          bool* aSnap) const {
  // Just go ahead and claim our frame's overflow rect as the bounds, because we
  // may have border-image-outset or other features that cause borders to extend
  // outside the border rect.  We could try to duplicate all the complexity
  // nsDisplayBorder has here, but keeping things in sync would be a pain, and
  // this code is not typically performance-sensitive.
  *aSnap = false;
  return Frame()->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
}

bool nsDisplayFieldSetBorder::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  auto frame = static_cast<nsFieldSetFrame*>(mFrame);
  auto offset = ToReferenceFrame();
  nsRect rect;
  Maybe<wr::SpaceAndClipChainHelper> clipOut;

  if (nsIFrame* legend = frame->GetLegend()) {
    rect = frame->VisualBorderRectRelativeToSelf() + offset;

    nsRect legendRect = legend->GetNormalRect() + offset;

    // Make sure we clip all of the border in case the legend is smaller.
    nscoord borderTopWidth = frame->GetUsedBorder().top;
    if (legendRect.height < borderTopWidth) {
      legendRect.height = borderTopWidth;
      legendRect.y = offset.y;
    }

    if (!legendRect.IsEmpty()) {
      // We need to clip out the part of the border where the legend would go
      auto appUnitsPerDevPixel = frame->PresContext()->AppUnitsPerDevPixel();
      auto layoutRect = wr::ToLayoutRect(LayoutDeviceRect::FromAppUnits(
          frame->GetVisualOverflowRectRelativeToSelf() + offset,
          appUnitsPerDevPixel));

      wr::ComplexClipRegion region;
      region.rect = wr::ToLayoutRect(
          LayoutDeviceRect::FromAppUnits(legendRect, appUnitsPerDevPixel));
      region.mode = wr::ClipMode::ClipOut;
      region.radii = wr::EmptyBorderRadius();
      nsTArray<mozilla::wr::ComplexClipRegion> array{region};

      auto clip = aBuilder.DefineClip(Nothing(), layoutRect, &array, nullptr);
      auto clipChain = aBuilder.DefineClipChain({clip}, true);
      clipOut.emplace(aBuilder, clipChain);
    }
  } else {
    rect = nsRect(offset, frame->GetRect().Size());
  }

  ImgDrawResult drawResult = nsCSSRendering::CreateWebRenderCommandsForBorder(
      this, mFrame, rect, aBuilder, aResources, aSc, aManager,
      aDisplayListBuilder);
  if (drawResult == ImgDrawResult::NOT_SUPPORTED) {
    return false;
  }

  nsDisplayItemGenericImageGeometry::UpdateDrawResult(this, drawResult);
  return true;
};

void nsFieldSetFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                       const nsDisplayListSet& aLists) {
  // Paint our background and border in a special way.
  // REVIEW: We don't really need to check frame emptiness here; if it's empty,
  // the background/border display item won't do anything, and if it isn't
  // empty, we need to paint the outline
  if (!(GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) &&
      IsVisibleForPainting()) {
    DisplayOutsetBoxShadowUnconditional(aBuilder, aLists.BorderBackground());

    const nsRect rect =
        VisualBorderRectRelativeToSelf() + aBuilder->ToReferenceFrame(this);

    nsDisplayBackgroundImage::AppendBackgroundItemsToTop(
        aBuilder, this, rect, aLists.BorderBackground(),
        /* aAllowWillPaintBorderOptimization = */ false);

    aLists.BorderBackground()->AppendNewToTop<nsDisplayFieldSetBorder>(aBuilder,
                                                                       this);

    DisplayOutlineUnconditional(aBuilder, aLists);

    DO_GLOBAL_REFLOW_COUNT_DSP("nsFieldSetFrame");
  }

  if (GetPrevInFlow()) {
    DisplayOverflowContainers(aBuilder, aLists);
  }

  nsDisplayListCollection contentDisplayItems(aBuilder);
  if (nsIFrame* inner = GetInner()) {
    // Collect the inner frame's display items into their own collection.
    // We need to be calling BuildDisplayList on it before the legend in
    // case it contains out-of-flow frames whose placeholders are in the
    // legend. However, we want the inner frame's display items to be
    // after the legend's display items in z-order, so we need to save them
    // and append them later.
    BuildDisplayListForChild(aBuilder, inner, contentDisplayItems);
  }
  if (nsIFrame* legend = GetLegend()) {
    // The legend's background goes on our BlockBorderBackgrounds list because
    // it's a block child.
    nsDisplayListSet set(aLists, aLists.BlockBorderBackgrounds());
    BuildDisplayListForChild(aBuilder, legend, set);
  }
  // Put the inner frame's display items on the master list. Note that this
  // moves its border/background display items to our BorderBackground() list,
  // which isn't really correct, but it's OK because the inner frame is
  // anonymous and can't have its own border and background.
  contentDisplayItems.MoveTo(aLists);
}

image::ImgDrawResult nsFieldSetFrame::PaintBorder(
    nsDisplayListBuilder* aBuilder, gfxContext& aRenderingContext, nsPoint aPt,
    const nsRect& aDirtyRect) {
  // If the border is smaller than the legend, move the border down
  // to be centered on the legend.  We call VisualBorderRectRelativeToSelf() to
  // compute the border positioning.
  // FIXME: This means border-radius clamping is incorrect; we should
  // override nsIFrame::GetBorderRadii.
  nsRect rect = VisualBorderRectRelativeToSelf() + aPt;
  nsPresContext* presContext = PresContext();

  PaintBorderFlags borderFlags = aBuilder->ShouldSyncDecodeImages()
                                     ? PaintBorderFlags::SyncDecodeImages
                                     : PaintBorderFlags();

  ImgDrawResult result = ImgDrawResult::SUCCESS;

  nsCSSRendering::PaintBoxShadowInner(presContext, aRenderingContext, this,
                                      rect);

  if (nsIFrame* legend = GetLegend()) {
    // We want to avoid drawing our border under the legend, so clip out the
    // legend while drawing our border.  We don't want to use mLegendRect here,
    // because we do want to draw our border under the legend's inline-start and
    // -end margins.  And we use GetNormalRect(), not GetRect(), because we do
    // not want relative positioning applied to the legend to change how our
    // border looks.
    nsRect legendRect = legend->GetNormalRect() + aPt;

    // Make sure we clip all of the border in case the legend is smaller.
    nscoord borderTopWidth = GetUsedBorder().top;
    if (legendRect.height < borderTopWidth) {
      legendRect.height = borderTopWidth;
      legendRect.y = aPt.y;
    }

    DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();
    // We set up a clip path which has our rect clockwise and the legend rect
    // counterclockwise, with FILL_WINDING as the fill rule.  That will allow us
    // to paint within our rect but outside the legend rect.  For "our rect" we
    // use our visual overflow rect (relative to ourselves, so it's not affected
    // by transforms), because we can have borders sticking outside our border
    // box (e.g. due to border-image-outset).
    RefPtr<PathBuilder> pathBuilder =
        drawTarget->CreatePathBuilder(FillRule::FILL_WINDING);
    int32_t appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
    AppendRectToPath(
        pathBuilder,
        NSRectToSnappedRect(GetVisualOverflowRectRelativeToSelf() + aPt,
                            appUnitsPerDevPixel, *drawTarget),
        true);
    AppendRectToPath(
        pathBuilder,
        NSRectToSnappedRect(legendRect, appUnitsPerDevPixel, *drawTarget),
        false);
    RefPtr<Path> clipPath = pathBuilder->Finish();

    aRenderingContext.Save();
    aRenderingContext.Clip(clipPath);
    result &= nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                          aDirtyRect, rect, mComputedStyle,
                                          borderFlags);
    aRenderingContext.Restore();
  } else {
    result &= nsCSSRendering::PaintBorder(presContext, aRenderingContext, this,
                                          aDirtyRect, nsRect(aPt, mRect.Size()),
                                          mComputedStyle, borderFlags);
  }

  return result;
}

nscoord nsFieldSetFrame::GetIntrinsicISize(
    gfxContext* aRenderingContext, nsLayoutUtils::IntrinsicISizeType aType) {
  nscoord legendWidth = 0;
  nscoord contentWidth = 0;
  if (!StyleDisplay()->IsContainSize()) {
    // Both inner and legend are children, and if the fieldset is
    // size-contained they should not contribute to the intrinsic size.
    if (nsIFrame* legend = GetLegend()) {
      legendWidth = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                                         legend, aType);
    }

    if (nsIFrame* inner = GetInner()) {
      // Ignore padding on the inner, since the padding will be applied to the
      // outer instead, and the padding computed for the inner is wrong
      // for percentage padding.
      contentWidth = nsLayoutUtils::IntrinsicForContainer(
          aRenderingContext, inner, aType, nsLayoutUtils::IGNORE_PADDING);
    }
  }

  return std::max(legendWidth, contentWidth);
}

nscoord nsFieldSetFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord result = 0;
  DISPLAY_MIN_INLINE_SIZE(this, result);

  result = GetIntrinsicISize(aRenderingContext, nsLayoutUtils::MIN_ISIZE);
  return result;
}

nscoord nsFieldSetFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord result = 0;
  DISPLAY_PREF_INLINE_SIZE(this, result);

  result = GetIntrinsicISize(aRenderingContext, nsLayoutUtils::PREF_ISIZE);
  return result;
}

/* virtual */
void nsFieldSetFrame::Reflow(nsPresContext* aPresContext,
                             ReflowOutput& aDesiredSize,
                             const ReflowInput& aReflowInput,
                             nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsFieldSetFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  NS_WARNING_ASSERTION(aReflowInput.ComputedISize() != NS_UNCONSTRAINEDSIZE,
                       "Should have a precomputed inline-size!");

  nsOverflowAreas ocBounds;
  nsReflowStatus ocStatus;
  if (GetPrevInFlow()) {
    ReflowOverflowContainerChildren(aPresContext, aReflowInput, ocBounds,
                                    ReflowChildFlags::Default, ocStatus);
  }

  //------------ Handle Incremental Reflow -----------------
  bool reflowInner;
  bool reflowLegend;
  nsIFrame* legend = GetLegend();
  nsIFrame* inner = GetInner();
  if (aReflowInput.ShouldReflowAllKids()) {
    reflowInner = inner != nullptr;
    reflowLegend = legend != nullptr;
  } else {
    reflowInner = inner && NS_SUBTREE_DIRTY(inner);
    reflowLegend = legend && NS_SUBTREE_DIRTY(legend);
  }

  // We don't allow fieldsets to break vertically. If we did, we'd
  // need logic here to push and pull overflow frames.
  // Since we're not applying our padding in this frame, we need to add it here
  // to compute the available width for our children.
  WritingMode wm = GetWritingMode();
  WritingMode innerWM = inner ? inner->GetWritingMode() : wm;
  WritingMode legendWM = legend ? legend->GetWritingMode() : wm;
  LogicalSize innerAvailSize = aReflowInput.ComputedSizeWithPadding(innerWM);
  LogicalSize legendAvailSize = aReflowInput.ComputedSize(legendWM);
  innerAvailSize.BSize(innerWM) = legendAvailSize.BSize(legendWM) =
      NS_UNCONSTRAINEDSIZE;

  // get our border and padding
  LogicalMargin border = aReflowInput.ComputedLogicalBorderPadding() -
                         aReflowInput.ComputedLogicalPadding();

  // Figure out how big the legend is if there is one.
  // get the legend's margin
  LogicalMargin legendMargin(wm);
  // reflow the legend only if needed
  Maybe<ReflowInput> legendReflowInput;
  if (legend) {
    legendReflowInput.emplace(aPresContext, aReflowInput, legend,
                              legendAvailSize);
  }
  if (reflowLegend) {
    ReflowOutput legendDesiredSize(aReflowInput);

    // We'll move the legend to its proper place later, so the position
    // and containerSize passed here are unimportant.
    const nsSize dummyContainerSize;
    ReflowChild(legend, aPresContext, legendDesiredSize, *legendReflowInput, wm,
                LogicalPoint(wm), dummyContainerSize,
                ReflowChildFlags::NoMoveFrame, aStatus);
#ifdef NOISY_REFLOW
    printf("  returned (%d, %d)\n", legendDesiredSize.Width(),
           legendDesiredSize.Height());
#endif
    // Calculate the legend's margin-box rectangle.
    legendMargin = legend->GetLogicalUsedMargin(wm);
    mLegendRect = LogicalRect(
        wm, 0, 0, legendDesiredSize.ISize(wm) + legendMargin.IStartEnd(wm),
        legendDesiredSize.BSize(wm) + legendMargin.BStartEnd(wm));
    nscoord oldSpace = mLegendSpace;
    mLegendSpace = 0;
    nscoord borderBStart = border.BStart(wm);
    if (mLegendRect.BSize(wm) > borderBStart) {
      // mLegendSpace is the space to subtract from our content-box size below.
      mLegendSpace = mLegendRect.BSize(wm) - borderBStart;
    } else {
      // Calculate the border-box position that would center the legend's
      // border-box within the fieldset border:
      nscoord off = (borderBStart - legendDesiredSize.BSize(wm)) / 2;
      off -= legendMargin.BStart(wm);  // convert to a margin-box position
      if (off > nscoord(0)) {
        // Align the legend to the end if center-aligning it would overflow.
        nscoord overflow = off + mLegendRect.BSize(wm) - borderBStart;
        if (overflow > nscoord(0)) {
          off -= overflow;
        }
        mLegendRect.BStart(wm) += off;
      }
    }

    // if the legend space changes then we need to reflow the
    // content area as well.
    if (mLegendSpace != oldSpace && inner) {
      reflowInner = true;
    }

    FinishReflowChild(legend, aPresContext, legendDesiredSize,
                      legendReflowInput.ptr(), wm, LogicalPoint(wm),
                      dummyContainerSize, ReflowChildFlags::NoMoveFrame);
  } else if (!legend) {
    mLegendRect.SetEmpty();
    mLegendSpace = 0;
  } else {
    // mLegendSpace and mLegendRect haven't changed, but we need
    // the used margin when placing the legend.
    legendMargin = legend->GetLogicalUsedMargin(wm);
  }

  // This containerSize is incomplete as yet: it does not include the size
  // of the |inner| frame itself.
  nsSize containerSize =
      (LogicalSize(wm, 0, mLegendSpace) + border.Size(wm)).GetPhysicalSize(wm);
  // reflow the content frame only if needed
  if (reflowInner) {
    ReflowInput kidReflowInput(aPresContext, aReflowInput, inner,
                               innerAvailSize, Nothing(),
                               ReflowInput::CALLER_WILL_INIT);
    // Override computed padding, in case it's percentage padding
    kidReflowInput.Init(aPresContext, Nothing(), nullptr,
                        &aReflowInput.ComputedPhysicalPadding());
    // Our child is "height:100%" but we actually want its height to be reduced
    // by the amount of content-height the legend is eating up, unless our
    // height is unconstrained (in which case the child's will be too).
    if (aReflowInput.ComputedBSize() != NS_UNCONSTRAINEDSIZE) {
      kidReflowInput.SetComputedBSize(
          std::max(0, aReflowInput.ComputedBSize() - mLegendSpace));
    }

    if (aReflowInput.ComputedMinBSize() > 0) {
      kidReflowInput.ComputedMinBSize() =
          std::max(0, aReflowInput.ComputedMinBSize() - mLegendSpace);
    }

    if (aReflowInput.ComputedMaxBSize() != NS_UNCONSTRAINEDSIZE) {
      kidReflowInput.ComputedMaxBSize() =
          std::max(0, aReflowInput.ComputedMaxBSize() - mLegendSpace);
    }

    ReflowOutput kidDesiredSize(kidReflowInput);
    // Reflow the frame
    NS_ASSERTION(
        kidReflowInput.ComputedPhysicalMargin() == nsMargin(0, 0, 0, 0),
        "Margins on anonymous fieldset child not supported!");
    LogicalPoint pt(wm, border.IStart(wm), border.BStart(wm) + mLegendSpace);

    // We don't know the correct containerSize until we have reflowed |inner|,
    // so we use a dummy value for now; FinishReflowChild will fix the position
    // if necessary.
    const nsSize dummyContainerSize;
    ReflowChild(inner, aPresContext, kidDesiredSize, kidReflowInput, wm, pt,
                dummyContainerSize, ReflowChildFlags::Default, aStatus);

    // Update containerSize to account for size of the inner frame, so that
    // FinishReflowChild can position it correctly.
    containerSize += kidDesiredSize.PhysicalSize();
    FinishReflowChild(inner, aPresContext, kidDesiredSize, &kidReflowInput, wm,
                      pt, containerSize, ReflowChildFlags::Default);
    NS_FRAME_TRACE_REFLOW_OUT("FieldSet::Reflow", aStatus);
  } else if (inner) {
    // |inner| didn't need to be reflowed but we do need to include its size
    // in containerSize.
    containerSize += inner->GetSize();
  }

  LogicalRect contentRect(wm);
  if (inner) {
    // We don't support margins on inner, so our content rect is just the
    // inner's border-box. (We don't really care about container size at this
    // point, as we'll figure out the actual positioning later.)
    contentRect = inner->GetLogicalRect(wm, containerSize);
  }

  // Our content rect must fill up the available width
  LogicalSize availSize = aReflowInput.ComputedSizeWithPadding(wm);
  if (availSize.ISize(wm) > contentRect.ISize(wm)) {
    contentRect.ISize(wm) = innerAvailSize.ISize(wm);
  }

  if (legend) {
    // The legend is positioned inline-wards within the inner's content rect
    // (so that padding on the fieldset affects the legend position).
    LogicalRect innerContentRect = contentRect;
    innerContentRect.Deflate(wm, aReflowInput.ComputedLogicalPadding());
    // If the inner content rect is larger than the legend, we can align the
    // legend.
    if (innerContentRect.ISize(wm) > mLegendRect.ISize(wm)) {
      // NOTE legend @align values are: left/right/center
      // GetLogicalAlign converts left/right to start/end for the given WM.
      // @see HTMLLegendElement::ParseAttribute, nsLegendFrame::GetLogicalAlign
      int32_t align =
          static_cast<nsLegendFrame*>(legend->GetContentInsertionFrame())
              ->GetLogicalAlign(wm);
      switch (align) {
        case NS_STYLE_TEXT_ALIGN_END:
          mLegendRect.IStart(wm) =
              innerContentRect.IEnd(wm) - mLegendRect.ISize(wm);
          break;
        case NS_STYLE_TEXT_ALIGN_CENTER:
          // Note: rounding removed; there doesn't seem to be any need
          mLegendRect.IStart(wm) =
              innerContentRect.IStart(wm) +
              (innerContentRect.ISize(wm) - mLegendRect.ISize(wm)) / 2;
          break;
        case NS_STYLE_TEXT_ALIGN_START:
          mLegendRect.IStart(wm) = innerContentRect.IStart(wm);
          break;
        default:
          MOZ_ASSERT_UNREACHABLE("unexpected GetLogicalAlign value");
      }
    } else {
      // otherwise just start-align it.
      mLegendRect.IStart(wm) = innerContentRect.IStart(wm);
    }

    // place the legend
    LogicalRect actualLegendRect = mLegendRect;
    actualLegendRect.Deflate(wm, legendMargin);
    LogicalPoint actualLegendPos(actualLegendRect.Origin(wm));

    // Note that legend's writing mode may be different from the fieldset's,
    // so we need to convert offsets before applying them to it (bug 1134534).
    LogicalMargin offsets =
        legendReflowInput->ComputedLogicalOffsets().ConvertTo(
            wm, legendReflowInput->GetWritingMode());
    ReflowInput::ApplyRelativePositioning(legend, wm, offsets, &actualLegendPos,
                                          containerSize);

    legend->SetPosition(wm, actualLegendPos, containerSize);
    nsContainerFrame::PositionFrameView(legend);
    nsContainerFrame::PositionChildViews(legend);
  }

  // Return our size and our result.
  LogicalSize finalSize(
      wm, contentRect.ISize(wm) + border.IStartEnd(wm),
      mLegendSpace + border.BStartEnd(wm) + (inner ? inner->BSize(wm) : 0));
  if (aReflowInput.mStyleDisplay->IsContainSize()) {
    // If we're size-contained, then we must set finalSize to be what
    // it'd be if we had no children (i.e. if we had no legend and if
    // 'inner' were empty).  Note: normally the fieldset's own padding
    // (which we still must honor) would be accounted for as part of
    // inner's size (see kidReflowInput.Init() call above).  So: since
    // we're disregarding sizing information from 'inner', we need to
    // account for that padding ourselves here.
    nscoord contentBoxBSize =
        aReflowInput.ComputedBSize() == NS_UNCONSTRAINEDSIZE
            ? aReflowInput.ApplyMinMaxBSize(0)
            : aReflowInput.ComputedBSize();
    finalSize.BSize(wm) =
        contentBoxBSize +
        aReflowInput.ComputedLogicalBorderPadding().BStartEnd(wm);
  }
  aDesiredSize.SetSize(wm, finalSize);
  aDesiredSize.SetOverflowAreasToDesiredBounds();

  if (legend) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, legend);
  }
  if (inner) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, inner);
  }

  // Merge overflow container bounds and status.
  aDesiredSize.mOverflowAreas.UnionWith(ocBounds);
  aStatus.MergeCompletionStatusFrom(ocStatus);

  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize, aReflowInput,
                                 aStatus);

  InvalidateFrame();

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

#ifdef DEBUG
void nsFieldSetFrame::SetInitialChildList(ChildListID aListID,
                                          nsFrameList& aChildList) {
  nsContainerFrame::SetInitialChildList(aListID, aChildList);
  MOZ_ASSERT(aListID != kPrincipalList || GetInner(),
             "Setting principal child list should populate our inner frame");
}
void nsFieldSetFrame::AppendFrames(ChildListID aListID,
                                   nsFrameList& aFrameList) {
  MOZ_CRASH("nsFieldSetFrame::AppendFrames not supported");
}

void nsFieldSetFrame::InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                                   const nsLineList::iterator* aPrevFrameLine,
                                   nsFrameList& aFrameList) {
  MOZ_CRASH("nsFieldSetFrame::InsertFrames not supported");
}

void nsFieldSetFrame::RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) {
  MOZ_CRASH("nsFieldSetFrame::RemoveFrame not supported");
}
#endif

#ifdef ACCESSIBILITY
a11y::AccType nsFieldSetFrame::AccessibleType() {
  return a11y::eHTMLGroupboxType;
}
#endif

nscoord nsFieldSetFrame::GetLogicalBaseline(WritingMode aWM) const {
  switch (StyleDisplay()->mDisplay) {
    case mozilla::StyleDisplay::Grid:
    case mozilla::StyleDisplay::InlineGrid:
    case mozilla::StyleDisplay::Flex:
    case mozilla::StyleDisplay::InlineFlex:
      return BaselineBOffset(aWM, BaselineSharingGroup::First,
                             AlignmentContext::Inline);
    default:
      return BSize(aWM) - BaselineBOffset(aWM, BaselineSharingGroup::Last,
                                          AlignmentContext::Inline);
  }
}

bool nsFieldSetFrame::GetVerticalAlignBaseline(WritingMode aWM,
                                               nscoord* aBaseline) const {
  if (StyleDisplay()->IsContainLayout()) {
    // If we are layout-contained, our child 'inner' should not
    // affect how we calculate our baseline.
    return false;
  }
  nsIFrame* inner = GetInner();
  MOZ_ASSERT(!inner->GetWritingMode().IsOrthogonalTo(aWM));
  if (!inner->GetVerticalAlignBaseline(aWM, aBaseline)) {
    return false;
  }
  nscoord innerBStart = inner->BStart(aWM, GetSize());
  *aBaseline += innerBStart;
  return true;
}

bool nsFieldSetFrame::GetNaturalBaselineBOffset(
    WritingMode aWM, BaselineSharingGroup aBaselineGroup,
    nscoord* aBaseline) const {
  if (StyleDisplay()->IsContainLayout()) {
    // If we are layout-contained, our child 'inner' should not
    // affect how we calculate our baseline.
    return false;
  }
  nsIFrame* inner = GetInner();
  MOZ_ASSERT(!inner->GetWritingMode().IsOrthogonalTo(aWM));
  if (!inner->GetNaturalBaselineBOffset(aWM, aBaselineGroup, aBaseline)) {
    return false;
  }
  nscoord innerBStart = inner->BStart(aWM, GetSize());
  if (aBaselineGroup == BaselineSharingGroup::First) {
    *aBaseline += innerBStart;
  } else {
    *aBaseline += BSize(aWM) - (innerBStart + inner->BSize(aWM));
  }
  return true;
}

void nsFieldSetFrame::AppendDirectlyOwnedAnonBoxes(
    nsTArray<OwnedAnonBox>& aResult) {
  if (nsIFrame* kid = GetInner()) {
    aResult.AppendElement(OwnedAnonBox(kid));
  }
}
