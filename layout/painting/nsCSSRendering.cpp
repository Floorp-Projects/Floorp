/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* utility functions for drawing borders and backgrounds */

#include <ctime>

#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MathAlgorithms.h"

#include "BorderConsts.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIFrame.h"
#include "nsIFrameInlines.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsIPresShell.h"
#include "nsFrameManager.h"
#include "nsStyleContext.h"
#include "nsGkAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsIContent.h"
#include "nsIDocumentInlines.h"
#include "nsIScrollableFrame.h"
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "ImageOps.h"
#include "nsCSSRendering.h"
#include "nsCSSColorUtils.h"
#include "nsITheme.h"
#include "nsThemeConstants.h"
#include "nsLayoutUtils.h"
#include "nsBlockFrame.h"
#include "nsStyleStructInlines.h"
#include "nsCSSFrameConstructor.h"
#include "nsCSSProps.h"
#include "nsContentUtils.h"
#include "nsSVGEffects.h"
#include "nsSVGIntegrationUtils.h"
#include "gfxDrawable.h"
#include "GeckoProfiler.h"
#include "nsCSSRenderingBorders.h"
#include "mozilla/css/ImageLoader.h"
#include "ImageContainer.h"
#include "mozilla/Telemetry.h"
#include "gfxUtils.h"
#include "gfxGradientCache.h"
#include "nsInlineFrame.h"
#include "nsRubyTextContainerFrame.h"
#include <algorithm>
#include "SVGImageContext.h"
#include "mozilla/layers/WebRenderDisplayItemLayer.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::gfx;
using namespace mozilla::image;
using mozilla::CSSSizeOrRatio;

static int gFrameTreeLockCount = 0;

// To avoid storing this data on nsInlineFrame (bloat) and to avoid
// recalculating this for each frame in a continuation (perf), hold
// a cache of various coordinate information that we need in order
// to paint inline backgrounds.
struct InlineBackgroundData
{
  InlineBackgroundData()
      : mFrame(nullptr), mLineContainer(nullptr)
  {
  }

  ~InlineBackgroundData()
  {
  }

  void Reset()
  {
    mBoundingBox.SetRect(0,0,0,0);
    mContinuationPoint = mLineContinuationPoint = mUnbrokenMeasure = 0;
    mFrame = mLineContainer = nullptr;
    mPIStartBorderData.Reset();
  }

  /**
   * Return a continuous rect for (an inline) aFrame relative to the
   * continuation that draws the left-most part of the background.
   * This is used when painting backgrounds.
   */
  nsRect GetContinuousRect(nsIFrame* aFrame)
  {
    MOZ_ASSERT(static_cast<nsInlineFrame*>(do_QueryFrame(aFrame)));

    SetFrame(aFrame);

    nscoord pos; // an x coordinate if writing-mode is horizontal;
                 // y coordinate if vertical
    if (mBidiEnabled) {
      pos = mLineContinuationPoint;

      // Scan continuations on the same line as aFrame and accumulate the widths
      // of frames that are to the left (if this is an LTR block) or right
      // (if it's RTL) of the current one.
      bool isRtlBlock = (mLineContainer->StyleVisibility()->mDirection ==
                           NS_STYLE_DIRECTION_RTL);
      nscoord curOffset = mVertical ? aFrame->GetOffsetTo(mLineContainer).y
                                    : aFrame->GetOffsetTo(mLineContainer).x;

      // If the continuation is fluid we know inlineFrame is not on the same line.
      // If it's not fluid, we need to test further to be sure.
      nsIFrame* inlineFrame = aFrame->GetPrevContinuation();
      while (inlineFrame && !inlineFrame->GetNextInFlow() &&
             AreOnSameLine(aFrame, inlineFrame)) {
        nscoord frameOffset = mVertical
          ? inlineFrame->GetOffsetTo(mLineContainer).y
          : inlineFrame->GetOffsetTo(mLineContainer).x;
        if (isRtlBlock == (frameOffset >= curOffset)) {
          pos += mVertical
               ? inlineFrame->GetSize().height
               : inlineFrame->GetSize().width;
        }
        inlineFrame = inlineFrame->GetPrevContinuation();
      }

      inlineFrame = aFrame->GetNextContinuation();
      while (inlineFrame && !inlineFrame->GetPrevInFlow() &&
             AreOnSameLine(aFrame, inlineFrame)) {
        nscoord frameOffset = mVertical
          ? inlineFrame->GetOffsetTo(mLineContainer).y
          : inlineFrame->GetOffsetTo(mLineContainer).x;
        if (isRtlBlock == (frameOffset >= curOffset)) {
          pos += mVertical
                 ? inlineFrame->GetSize().height
                 : inlineFrame->GetSize().width;
        }
        inlineFrame = inlineFrame->GetNextContinuation();
      }
      if (isRtlBlock) {
        // aFrame itself is also to the right of its left edge, so add its width.
        pos += mVertical ? aFrame->GetSize().height : aFrame->GetSize().width;
        // pos is now the distance from the left [top] edge of aFrame to the right [bottom] edge
        // of the unbroken content. Change it to indicate the distance from the
        // left [top] edge of the unbroken content to the left [top] edge of aFrame.
        pos = mUnbrokenMeasure - pos;
      }
    } else {
      pos = mContinuationPoint;
    }

    // Assume background-origin: border and return a rect with offsets
    // relative to (0,0).  If we have a different background-origin,
    // then our rect should be deflated appropriately by our caller.
    return mVertical
      ? nsRect(0, -pos, mFrame->GetSize().width, mUnbrokenMeasure)
      : nsRect(-pos, 0, mUnbrokenMeasure, mFrame->GetSize().height);
  }

  /**
   * Return a continuous rect for (an inline) aFrame relative to the
   * continuation that should draw the left[top]-border.  This is used when painting
   * borders and clipping backgrounds.  This may NOT be the same continuous rect
   * as for drawing backgrounds; the continuation with the left[top]-border might be
   * somewhere in the middle of that rect (e.g. BIDI), in those cases we need
   * the reverse background order starting at the left[top]-border continuation.
   */
  nsRect GetBorderContinuousRect(nsIFrame* aFrame, nsRect aBorderArea)
  {
    // Calling GetContinuousRect(aFrame) here may lead to Reset/Init which
    // resets our mPIStartBorderData so we save it ...
    PhysicalInlineStartBorderData saved(mPIStartBorderData);
    nsRect joinedBorderArea = GetContinuousRect(aFrame);
    if (!saved.mIsValid || saved.mFrame != mPIStartBorderData.mFrame) {
      if (aFrame == mPIStartBorderData.mFrame) {
        if (mVertical) {
          mPIStartBorderData.SetCoord(joinedBorderArea.y);
        } else {
          mPIStartBorderData.SetCoord(joinedBorderArea.x);
        }
      } else if (mPIStartBorderData.mFrame) {
        if (mVertical) {
          mPIStartBorderData.SetCoord(GetContinuousRect(mPIStartBorderData.mFrame).y);
        } else {
          mPIStartBorderData.SetCoord(GetContinuousRect(mPIStartBorderData.mFrame).x);
        }
      }
    } else {
      // ... and restore it when possible.
      mPIStartBorderData.mCoord = saved.mCoord;
    }
    if (mVertical) {
      if (joinedBorderArea.y > mPIStartBorderData.mCoord) {
        joinedBorderArea.y =
          -(mUnbrokenMeasure + joinedBorderArea.y - aBorderArea.height);
      } else {
        joinedBorderArea.y -= mPIStartBorderData.mCoord;
      }
    } else {
      if (joinedBorderArea.x > mPIStartBorderData.mCoord) {
        joinedBorderArea.x =
          -(mUnbrokenMeasure + joinedBorderArea.x - aBorderArea.width);
      } else {
        joinedBorderArea.x -= mPIStartBorderData.mCoord;
      }
    }
    return joinedBorderArea;
  }

  nsRect GetBoundingRect(nsIFrame* aFrame)
  {
    SetFrame(aFrame);

    // Move the offsets relative to (0,0) which puts the bounding box into
    // our coordinate system rather than our parent's.  We do this by
    // moving it the back distance from us to the bounding box.
    // This also assumes background-origin: border, so our caller will
    // need to deflate us if needed.
    nsRect boundingBox(mBoundingBox);
    nsPoint point = mFrame->GetPosition();
    boundingBox.MoveBy(-point.x, -point.y);

    return boundingBox;
  }

protected:
  // This is a coordinate on the inline axis, but is not a true logical inline-
  // coord because it is always measured from left to right (if horizontal) or
  // from top to bottom (if vertical), ignoring any bidi RTL directionality.
  // We'll call this "physical inline start", or PIStart for short.
  struct PhysicalInlineStartBorderData {
    nsIFrame* mFrame;   // the continuation that may have a left-border
    nscoord   mCoord;   // cached GetContinuousRect(mFrame).x or .y
    bool      mIsValid; // true if mCoord is valid
    void Reset() { mFrame = nullptr; mIsValid = false; }
    void SetCoord(nscoord aCoord) { mCoord = aCoord; mIsValid = true; }
  };

  nsIFrame*      mFrame;
  nsIFrame*      mLineContainer;
  nsRect         mBoundingBox;
  nscoord        mContinuationPoint;
  nscoord        mUnbrokenMeasure;
  nscoord        mLineContinuationPoint;
  PhysicalInlineStartBorderData mPIStartBorderData;
  bool           mBidiEnabled;
  bool           mVertical;

  void SetFrame(nsIFrame* aFrame)
  {
    NS_PRECONDITION(aFrame, "Need a frame");
    NS_ASSERTION(gFrameTreeLockCount > 0,
                 "Can't call this when frame tree is not locked");

    if (aFrame == mFrame) {
      return;
    }

    nsIFrame *prevContinuation = GetPrevContinuation(aFrame);

    if (!prevContinuation || mFrame != prevContinuation) {
      // Ok, we've got the wrong frame.  We have to start from scratch.
      Reset();
      Init(aFrame);
      return;
    }

    // Get our last frame's size and add its width to our continuation
    // point before we cache the new frame.
    mContinuationPoint += mVertical ? mFrame->GetSize().height
                                    : mFrame->GetSize().width;

    // If this a new line, update mLineContinuationPoint.
    if (mBidiEnabled &&
        (aFrame->GetPrevInFlow() || !AreOnSameLine(mFrame, aFrame))) {
       mLineContinuationPoint = mContinuationPoint;
    }

    mFrame = aFrame;
  }

  nsIFrame* GetPrevContinuation(nsIFrame* aFrame)
  {
    nsIFrame* prevCont = aFrame->GetPrevContinuation();
    if (!prevCont &&
        (aFrame->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT)) {
      nsIFrame* block =
        aFrame->GetProperty(nsIFrame::IBSplitPrevSibling());
      if (block) {
        // The {ib} properties are only stored on first continuations
        NS_ASSERTION(!block->GetPrevContinuation(),
                     "Incorrect value for IBSplitPrevSibling");
        prevCont =
          block->GetProperty(nsIFrame::IBSplitPrevSibling());
        NS_ASSERTION(prevCont, "How did that happen?");
      }
    }
    return prevCont;
  }

  nsIFrame* GetNextContinuation(nsIFrame* aFrame)
  {
    nsIFrame* nextCont = aFrame->GetNextContinuation();
    if (!nextCont &&
        (aFrame->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT)) {
      // The {ib} properties are only stored on first continuations
      aFrame = aFrame->FirstContinuation();
      nsIFrame* block = aFrame->GetProperty(nsIFrame::IBSplitSibling());
      if (block) {
        nextCont = block->GetProperty(nsIFrame::IBSplitSibling());
        NS_ASSERTION(nextCont, "How did that happen?");
      }
    }
    return nextCont;
  }

  void Init(nsIFrame* aFrame)
  {
    mPIStartBorderData.Reset();
    mBidiEnabled = aFrame->PresContext()->BidiEnabled();
    if (mBidiEnabled) {
      // Find the line container frame
      mLineContainer = aFrame;
      while (mLineContainer &&
             mLineContainer->IsFrameOfType(nsIFrame::eLineParticipant)) {
        mLineContainer = mLineContainer->GetParent();
      }

      MOZ_ASSERT(mLineContainer, "Cannot find line containing frame.");
      MOZ_ASSERT(mLineContainer != aFrame, "line container frame "
                 "should be an ancestor of the target frame.");
    }

    mVertical = aFrame->GetWritingMode().IsVertical();

    // Start with the previous flow frame as our continuation point
    // is the total of the widths of the previous frames.
    nsIFrame* inlineFrame = GetPrevContinuation(aFrame);
    while (inlineFrame) {
      if (!mPIStartBorderData.mFrame &&
          !(mVertical ? inlineFrame->GetSkipSides().Top()
                      : inlineFrame->GetSkipSides().Left())) {
        mPIStartBorderData.mFrame = inlineFrame;
      }
      nsRect rect = inlineFrame->GetRect();
      mContinuationPoint += mVertical ? rect.height : rect.width;
      if (mBidiEnabled && !AreOnSameLine(aFrame, inlineFrame)) {
        mLineContinuationPoint += mVertical ? rect.height : rect.width;
      }
      mUnbrokenMeasure += mVertical ? rect.height : rect.width;
      mBoundingBox.UnionRect(mBoundingBox, rect);
      inlineFrame = GetPrevContinuation(inlineFrame);
    }

    // Next add this frame and subsequent frames to the bounding box and
    // unbroken width.
    inlineFrame = aFrame;
    while (inlineFrame) {
      if (!mPIStartBorderData.mFrame &&
          !(mVertical ? inlineFrame->GetSkipSides().Top()
                      : inlineFrame->GetSkipSides().Left())) {
        mPIStartBorderData.mFrame = inlineFrame;
      }
      nsRect rect = inlineFrame->GetRect();
      mUnbrokenMeasure += mVertical ? rect.height : rect.width;
      mBoundingBox.UnionRect(mBoundingBox, rect);
      inlineFrame = GetNextContinuation(inlineFrame);
    }

    mFrame = aFrame;
  }

  bool AreOnSameLine(nsIFrame* aFrame1, nsIFrame* aFrame2) {
    if (nsBlockFrame* blockFrame = do_QueryFrame(mLineContainer)) {
      bool isValid1, isValid2;
      nsBlockInFlowLineIterator it1(blockFrame, aFrame1, &isValid1);
      nsBlockInFlowLineIterator it2(blockFrame, aFrame2, &isValid2);
      return isValid1 && isValid2 &&
        // Make sure aFrame1 and aFrame2 are in the same continuation of
        // blockFrame.
        it1.GetContainer() == it2.GetContainer() &&
        // And on the same line in it
        it1.GetLine() == it2.GetLine();
    }
    if (nsRubyTextContainerFrame* rtcFrame = do_QueryFrame(mLineContainer)) {
      nsBlockFrame* block = nsLayoutUtils::FindNearestBlockAncestor(rtcFrame);
      // Ruby text container can only hold one line of text, so if they
      // are in the same continuation, they are in the same line. Since
      // ruby text containers are bidi isolate, they are never split for
      // bidi reordering, which means being in different continuation
      // indicates being in different lines.
      for (nsIFrame* frame = rtcFrame->FirstContinuation();
           frame; frame = frame->GetNextContinuation()) {
        bool isDescendant1 =
          nsLayoutUtils::IsProperAncestorFrame(frame, aFrame1, block);
        bool isDescendant2 =
          nsLayoutUtils::IsProperAncestorFrame(frame, aFrame2, block);
        if (isDescendant1 && isDescendant2) {
          return true;
        }
        if (isDescendant1 || isDescendant2) {
          return false;
        }
      }
      MOZ_ASSERT_UNREACHABLE("None of the frames is a descendant of this rtc?");
    }
    MOZ_ASSERT_UNREACHABLE("Do we have any other type of line container?");
    return false;
  }
};

/* Local functions */
static nscolor MakeBevelColor(mozilla::Side whichSide, uint8_t style,
                              nscolor aBackgroundColor,
                              nscolor aBorderColor);

static InlineBackgroundData* gInlineBGData = nullptr;

// Initialize any static variables used by nsCSSRendering.
void nsCSSRendering::Init()
{
  NS_ASSERTION(!gInlineBGData, "Init called twice");
  gInlineBGData = new InlineBackgroundData();
}

// Clean up any global variables used by nsCSSRendering.
void nsCSSRendering::Shutdown()
{
  delete gInlineBGData;
  gInlineBGData = nullptr;
}

/**
 * Make a bevel color
 */
static nscolor
MakeBevelColor(mozilla::Side whichSide, uint8_t style,
               nscolor aBackgroundColor, nscolor aBorderColor)
{

  nscolor colors[2];
  nscolor theColor;

  // Given a background color and a border color
  // calculate the color used for the shading
  NS_GetSpecial3DColors(colors, aBackgroundColor, aBorderColor);

  if ((style == NS_STYLE_BORDER_STYLE_OUTSET) ||
      (style == NS_STYLE_BORDER_STYLE_RIDGE)) {
    // Flip colors for these two border styles
    switch (whichSide) {
    case eSideBottom: whichSide = eSideTop;    break;
    case eSideRight:  whichSide = eSideLeft;   break;
    case eSideTop:    whichSide = eSideBottom; break;
    case eSideLeft:   whichSide = eSideRight;  break;
    }
  }

  switch (whichSide) {
  case eSideBottom:
    theColor = colors[1];
    break;
  case eSideRight:
    theColor = colors[1];
    break;
  case eSideTop:
    theColor = colors[0];
    break;
  case eSideLeft:
  default:
    theColor = colors[0];
    break;
  }
  return theColor;
}

static bool
GetRadii(nsIFrame* aForFrame, const nsStyleBorder& aBorder,
         const nsRect& aOrigBorderArea, const nsRect& aBorderArea,
         nscoord aRadii[8])
{
  bool haveRoundedCorners;
  nsSize sz = aBorderArea.Size();
  nsSize frameSize = aForFrame->GetSize();
  if (&aBorder == aForFrame->StyleBorder() &&
      frameSize == aOrigBorderArea.Size()) {
    haveRoundedCorners = aForFrame->GetBorderRadii(sz, sz, Sides(), aRadii);
   } else {
    haveRoundedCorners =
      nsIFrame::ComputeBorderRadii(aBorder.mBorderRadius, frameSize, sz, Sides(), aRadii);
  }

  return haveRoundedCorners;
}

static bool
GetRadii(nsIFrame* aForFrame, const nsStyleBorder& aBorder,
         const nsRect& aOrigBorderArea, const nsRect& aBorderArea,
         RectCornerRadii* aBgRadii)
{
  nscoord radii[8];
  bool haveRoundedCorners = GetRadii(aForFrame, aBorder, aOrigBorderArea, aBorderArea, radii);

  if (haveRoundedCorners) {
    auto d2a = aForFrame->PresContext()->AppUnitsPerDevPixel();
    nsCSSRendering::ComputePixelRadii(radii, d2a, aBgRadii);
  }
  return haveRoundedCorners;
}

static nsRect
JoinBoxesForBlockAxisSlice(nsIFrame* aFrame, const nsRect& aBorderArea)
{
  // Inflate the block-axis size as if our continuations were laid out
  // adjacent in that axis.  Note that we don't touch the inline size.
  nsRect borderArea = aBorderArea;
  nscoord bSize = 0;
  auto wm = aFrame->GetWritingMode();
  nsIFrame* f = aFrame->GetNextContinuation();
  for (; f; f = f->GetNextContinuation()) {
    MOZ_ASSERT(!(f->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT),
               "anonymous ib-split block shouldn't have border/background");
    bSize += f->BSize(wm);
  }
  (wm.IsVertical() ? borderArea.width : borderArea.height) += bSize;
  bSize = 0;
  f = aFrame->GetPrevContinuation();
  for (; f; f = f->GetPrevContinuation()) {
    MOZ_ASSERT(!(f->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT),
               "anonymous ib-split block shouldn't have border/background");
    bSize += f->BSize(wm);
  }
  (wm.IsVertical() ? borderArea.x : borderArea.y) -= bSize;
  (wm.IsVertical() ? borderArea.width : borderArea.height) += bSize;
  return borderArea;
}

/**
 * Inflate aBorderArea which is relative to aFrame's origin to calculate
 * a hypothetical non-split frame area for all the continuations.
 * See "Joining Boxes for 'slice'" in
 * http://dev.w3.org/csswg/css-break/#break-decoration
 */
enum InlineBoxOrder { eForBorder, eForBackground };
static nsRect
JoinBoxesForSlice(nsIFrame* aFrame, const nsRect& aBorderArea,
                  InlineBoxOrder aOrder)
{
  if (static_cast<nsInlineFrame*>(do_QueryFrame(aFrame))) {
    return (aOrder == eForBorder
            ? gInlineBGData->GetBorderContinuousRect(aFrame, aBorderArea)
            : gInlineBGData->GetContinuousRect(aFrame)) +
      aBorderArea.TopLeft();
  }
  return JoinBoxesForBlockAxisSlice(aFrame, aBorderArea);
}

static bool
IsBoxDecorationSlice(const nsStyleBorder& aStyleBorder)
{
  return aStyleBorder.mBoxDecorationBreak == StyleBoxDecorationBreak::Slice;
}

static nsRect
BoxDecorationRectForBorder(nsIFrame* aFrame, const nsRect& aBorderArea,
                           Sides aSkipSides,
                           const nsStyleBorder* aStyleBorder = nullptr)
{
  if (!aStyleBorder) {
    aStyleBorder = aFrame->StyleBorder();
  }
  // If aSkipSides.IsEmpty() then there are no continuations, or it's
  // a ::first-letter that wants all border sides on the first continuation.
  return ::IsBoxDecorationSlice(*aStyleBorder) && !aSkipSides.IsEmpty()
           ? ::JoinBoxesForSlice(aFrame, aBorderArea, eForBorder)
           : aBorderArea;
}

static nsRect
BoxDecorationRectForBackground(nsIFrame* aFrame, const nsRect& aBorderArea,
                               Sides aSkipSides,
                               const nsStyleBorder* aStyleBorder = nullptr)
{
  if (!aStyleBorder) {
    aStyleBorder = aFrame->StyleBorder();
  }
  // If aSkipSides.IsEmpty() then there are no continuations, or it's
  // a ::first-letter that wants all border sides on the first continuation.
  return ::IsBoxDecorationSlice(*aStyleBorder) && !aSkipSides.IsEmpty()
           ? ::JoinBoxesForSlice(aFrame, aBorderArea, eForBackground)
           : aBorderArea;
}

//----------------------------------------------------------------------
// Thebes Border Rendering Code Start

/*
 * Compute the float-pixel radii that should be used for drawing
 * this border/outline, given the various input bits.
 */
/* static */ void
nsCSSRendering::ComputePixelRadii(const nscoord *aAppUnitsRadii,
                                  nscoord aAppUnitsPerPixel,
                                  RectCornerRadii *oBorderRadii)
{
  Float radii[8];
  NS_FOR_CSS_HALF_CORNERS(corner)
    radii[corner] = Float(aAppUnitsRadii[corner]) / aAppUnitsPerPixel;

  (*oBorderRadii)[C_TL] = Size(radii[eCornerTopLeftX],
                               radii[eCornerTopLeftY]);
  (*oBorderRadii)[C_TR] = Size(radii[eCornerTopRightX],
                               radii[eCornerTopRightY]);
  (*oBorderRadii)[C_BR] = Size(radii[eCornerBottomRightX],
                               radii[eCornerBottomRightY]);
  (*oBorderRadii)[C_BL] = Size(radii[eCornerBottomLeftX],
                               radii[eCornerBottomLeftY]);
}

DrawResult
nsCSSRendering::PaintBorder(nsPresContext* aPresContext,
                            gfxContext& aRenderingContext,
                            nsIFrame* aForFrame,
                            const nsRect& aDirtyRect,
                            const nsRect& aBorderArea,
                            nsStyleContext* aStyleContext,
                            PaintBorderFlags aFlags,
                            Sides aSkipSides)
{
  AUTO_PROFILER_LABEL("nsCSSRendering::PaintBorder", GRAPHICS);

  nsStyleContext *styleIfVisited = aStyleContext->GetStyleIfVisited();
  const nsStyleBorder *styleBorder = aStyleContext->StyleBorder();
  // Don't check RelevantLinkVisited here, since we want to take the
  // same amount of time whether or not it's true.
  if (!styleIfVisited) {
    return PaintBorderWithStyleBorder(aPresContext, aRenderingContext, aForFrame,
                                      aDirtyRect, aBorderArea, *styleBorder,
                                      aStyleContext, aFlags, aSkipSides);
  }

  nsStyleBorder newStyleBorder(*styleBorder);

  NS_FOR_CSS_SIDES(side) {
    nscolor color = aStyleContext->
      GetVisitedDependentColor(nsStyleBorder::BorderColorFieldFor(side));
    newStyleBorder.mBorderColor[side] = StyleComplexColor::FromColor(color);
  }
  return PaintBorderWithStyleBorder(aPresContext, aRenderingContext, aForFrame,
                                    aDirtyRect, aBorderArea, newStyleBorder,
                                    aStyleContext, aFlags, aSkipSides);
}

Maybe<nsCSSBorderRenderer>
nsCSSRendering::CreateBorderRenderer(nsPresContext* aPresContext,
                                     DrawTarget* aDrawTarget,
                                     nsIFrame* aForFrame,
                                     const nsRect& aDirtyRect,
                                     const nsRect& aBorderArea,
                                     nsStyleContext* aStyleContext,
                                     Sides aSkipSides)
{
  nsStyleContext *styleIfVisited = aStyleContext->GetStyleIfVisited();
  const nsStyleBorder *styleBorder = aStyleContext->StyleBorder();
  // Don't check RelevantLinkVisited here, since we want to take the
  // same amount of time whether or not it's true.
  if (!styleIfVisited) {
    return CreateBorderRendererWithStyleBorder(aPresContext, aDrawTarget,
                                               aForFrame, aDirtyRect,
                                               aBorderArea, *styleBorder,
                                               aStyleContext, aSkipSides);
  }

  nsStyleBorder newStyleBorder(*styleBorder);

  NS_FOR_CSS_SIDES(side) {
    nscolor color = aStyleContext->
      GetVisitedDependentColor(nsStyleBorder::BorderColorFieldFor(side));
    newStyleBorder.mBorderColor[side] = StyleComplexColor::FromColor(color);
  }
  return CreateBorderRendererWithStyleBorder(aPresContext, aDrawTarget,
                                             aForFrame, aDirtyRect, aBorderArea,
                                             newStyleBorder, aStyleContext,
                                             aSkipSides);
}

nsCSSBorderRenderer
ConstructBorderRenderer(nsPresContext* aPresContext,
                        nsStyleContext* aStyleContext,
                        DrawTarget* aDrawTarget,
                        nsIFrame* aForFrame,
                        const nsRect& aDirtyRect,
                        const nsRect& aBorderArea,
                        const nsStyleBorder& aStyleBorder,
                        Sides aSkipSides,
                        bool* aNeedsClip)
{
  nsMargin border = aStyleBorder.GetComputedBorder();

  // Get our style context's color struct.
  const nsStyleColor* ourColor = aStyleContext->StyleColor();

  // In NavQuirks mode we want to use the parent's context as a starting point
  // for determining the background color.
  bool quirks = aPresContext->CompatibilityMode() == eCompatibility_NavQuirks;
  nsIFrame* bgFrame = nsCSSRendering::FindNonTransparentBackgroundFrame(aForFrame, quirks);
  nsStyleContext* bgContext = bgFrame->StyleContext();
  nscolor bgColor = bgContext->
    GetVisitedDependentColor(&nsStyleBackground::mBackgroundColor);

  // Compute the outermost boundary of the area that might be painted.
  // Same coordinate space as aBorderArea & aBGClipRect.
  nsRect joinedBorderArea =
    ::BoxDecorationRectForBorder(aForFrame, aBorderArea, aSkipSides, &aStyleBorder);
  RectCornerRadii bgRadii;
  ::GetRadii(aForFrame, aStyleBorder, aBorderArea, joinedBorderArea, &bgRadii);

  PrintAsFormatString(" joinedBorderArea: %d %d %d %d\n", joinedBorderArea.x, joinedBorderArea.y,
     joinedBorderArea.width, joinedBorderArea.height);

  // start drawing
  if (::IsBoxDecorationSlice(aStyleBorder)) {
    if (joinedBorderArea.IsEqualEdges(aBorderArea)) {
      // No need for a clip, just skip the sides we don't want.
      border.ApplySkipSides(aSkipSides);
    } else {
      // We're drawing borders around the joined continuation boxes so we need
      // to clip that to the slice that we want for this frame.
      *aNeedsClip = true;
    }
  } else {
    MOZ_ASSERT(joinedBorderArea.IsEqualEdges(aBorderArea),
               "Should use aBorderArea for box-decoration-break:clone");
    MOZ_ASSERT(aForFrame->GetSkipSides().IsEmpty() ||
               IS_TRUE_OVERFLOW_CONTAINER(aForFrame),
               "Should not skip sides for box-decoration-break:clone except "
               "::first-letter/line continuations or other frame types that "
               "don't have borders but those shouldn't reach this point. "
               "Overflow containers do reach this point though.");
    border.ApplySkipSides(aSkipSides);
  }

  // Convert to dev pixels.
  nscoord twipsPerPixel = aPresContext->DevPixelsToAppUnits(1);
  Rect joinedBorderAreaPx = NSRectToRect(joinedBorderArea, twipsPerPixel);
  Float borderWidths[4] = { Float(border.top) / twipsPerPixel,
                                   Float(border.right) / twipsPerPixel,
                                   Float(border.bottom) / twipsPerPixel,
                                   Float(border.left) / twipsPerPixel };
  Rect dirtyRect = NSRectToRect(aDirtyRect, twipsPerPixel);

  uint8_t borderStyles[4];
  nscolor borderColors[4];
  nsBorderColors* compositeColors[4];

  // pull out styles, colors, composite colors
  NS_FOR_CSS_SIDES (i) {
    borderStyles[i] = aStyleBorder.GetBorderStyle(i);
    borderColors[i] = ourColor->CalcComplexColor(aStyleBorder.mBorderColor[i]);
    aStyleBorder.GetCompositeColors(i, &compositeColors[i]);
  }

  PrintAsFormatString(" borderStyles: %d %d %d %d\n", borderStyles[0], borderStyles[1], borderStyles[2], borderStyles[3]);

  nsIDocument* document = nullptr;
  nsIContent* content = aForFrame->GetContent();
  if (content) {
    document = content->OwnerDoc();
  }

  return nsCSSBorderRenderer(aPresContext,
                             document,
                             aDrawTarget,
                             dirtyRect,
                             joinedBorderAreaPx,
                             borderStyles,
                             borderWidths,
                             bgRadii,
                             borderColors,
                             compositeColors,
                             bgColor);
}


DrawResult
nsCSSRendering::PaintBorderWithStyleBorder(nsPresContext* aPresContext,
                                           gfxContext& aRenderingContext,
                                           nsIFrame* aForFrame,
                                           const nsRect& aDirtyRect,
                                           const nsRect& aBorderArea,
                                           const nsStyleBorder& aStyleBorder,
                                           nsStyleContext* aStyleContext,
                                           PaintBorderFlags aFlags,
                                           Sides aSkipSides)
{
  DrawTarget& aDrawTarget = *aRenderingContext.GetDrawTarget();

  PrintAsStringNewline("++ PaintBorder");

  // Check to see if we have an appearance defined.  If so, we let the theme
  // renderer draw the border.  DO not get the data from aForFrame, since the passed in style context
  // may be different!  Always use |aStyleContext|!
  const nsStyleDisplay* displayData = aStyleContext->StyleDisplay();
  if (displayData->mAppearance) {
    nsITheme *theme = aPresContext->GetTheme();
    if (theme &&
        theme->ThemeSupportsWidget(aPresContext, aForFrame,
                                   displayData->mAppearance)) {
      return DrawResult::SUCCESS; // Let the theme handle it.
    }
  }

  if (!aStyleBorder.mBorderImageSource.IsEmpty()) {
    DrawResult result = DrawResult::SUCCESS;

    uint32_t irFlags = 0;
    if (aFlags & PaintBorderFlags::SYNC_DECODE_IMAGES) {
      irFlags |= nsImageRenderer::FLAG_SYNC_DECODE_IMAGES;
    }

    // Creating the border image renderer will request a decode, and we rely on
    // that happening.
    Maybe<nsCSSBorderImageRenderer> renderer =
      nsCSSBorderImageRenderer::CreateBorderImageRenderer(aPresContext, aForFrame, aBorderArea,
                                                          aStyleBorder, aDirtyRect, aSkipSides,
                                                          irFlags, &result);
    // renderer was created successfully, which means border image is ready to
    // be used.
    if (renderer) {
      MOZ_ASSERT(result == DrawResult::SUCCESS);
      return renderer->DrawBorderImage(aPresContext, aRenderingContext,
                                       aForFrame, aDirtyRect);
    }
  }

  DrawResult result = DrawResult::SUCCESS;

  // If we had a border-image, but it wasn't loaded, then we should return
  // DrawResult::NOT_READY; we'll want to try again if we do a paint with sync
  // decoding enabled.
  if (aStyleBorder.mBorderImageSource.GetType() != eStyleImageType_Null) {
    result = DrawResult::NOT_READY;
  }

  nsMargin border = aStyleBorder.GetComputedBorder();
  if (0 == border.left && 0 == border.right &&
      0 == border.top  && 0 == border.bottom) {
    // Empty border area
    return result;
  }

  bool needsClip = false;
  nsCSSBorderRenderer br = ConstructBorderRenderer(aPresContext,
                                                   aStyleContext,
                                                   &aDrawTarget,
                                                   aForFrame,
                                                   aDirtyRect,
                                                   aBorderArea,
                                                   aStyleBorder,
                                                   aSkipSides,
                                                   &needsClip);
  if (needsClip) {
    aDrawTarget.PushClipRect(
        NSRectToSnappedRect(aBorderArea,
                            aForFrame->PresContext()->AppUnitsPerDevPixel(),
                            aDrawTarget));
  }

  br.DrawBorders();

  if (needsClip) {
    aDrawTarget.PopClip();
  }

  PrintAsStringNewline();

  return result;
}

Maybe<nsCSSBorderRenderer>
nsCSSRendering::CreateBorderRendererWithStyleBorder(nsPresContext* aPresContext,
                                                    DrawTarget* aDrawTarget,
                                                    nsIFrame* aForFrame,
                                                    const nsRect& aDirtyRect,
                                                    const nsRect& aBorderArea,
                                                    const nsStyleBorder& aStyleBorder,
                                                    nsStyleContext* aStyleContext,
                                                    Sides aSkipSides)
{
  const nsStyleDisplay* displayData = aStyleContext->StyleDisplay();
  if (displayData->mAppearance) {
    nsITheme *theme = aPresContext->GetTheme();
    if (theme &&
        theme->ThemeSupportsWidget(aPresContext, aForFrame,
                                   displayData->mAppearance)) {
      return Nothing();
    }
  }

  if (aStyleBorder.mBorderImageSource.GetType() != eStyleImageType_Null) {
    return Nothing();
  }

  nsMargin border = aStyleBorder.GetComputedBorder();
  if (0 == border.left && 0 == border.right &&
      0 == border.top  && 0 == border.bottom) {
    // Empty border area
    return Nothing();
  }

  bool needsClip = false;
  nsCSSBorderRenderer br = ConstructBorderRenderer(aPresContext,
                                                   aStyleContext,
                                                   aDrawTarget,
                                                   aForFrame,
                                                   aDirtyRect,
                                                   aBorderArea,
                                                   aStyleBorder,
                                                   aSkipSides,
                                                   &needsClip);
  if (needsClip) {
    return Nothing();
  }
  return Some(br);
}

static nsRect
GetOutlineInnerRect(nsIFrame* aFrame)
{
  nsRect* savedOutlineInnerRect =
    aFrame->GetProperty(nsIFrame::OutlineInnerRectProperty());
  if (savedOutlineInnerRect)
    return *savedOutlineInnerRect;
  NS_NOTREACHED("we should have saved a frame property");
  return nsRect(nsPoint(0, 0), aFrame->GetSize());
}

Maybe<nsCSSBorderRenderer>
nsCSSRendering::CreateBorderRendererForOutline(nsPresContext* aPresContext,
                                               gfxContext* aRenderingContext,
                                               nsIFrame* aForFrame,
                                               const nsRect& aDirtyRect,
                                               const nsRect& aBorderArea,
                                               nsStyleContext* aStyleContext)
{
  nscoord             twipsRadii[8];

  // Get our style context's color struct.
  const nsStyleOutline* ourOutline = aStyleContext->StyleOutline();

  if (!ourOutline->ShouldPaintOutline()) {
    // Empty outline
    return Nothing();
  }

  nsIFrame* bgFrame = nsCSSRendering::FindNonTransparentBackgroundFrame
    (aForFrame, false);
  nsStyleContext* bgContext = bgFrame->StyleContext();
  nscolor bgColor = bgContext->
    GetVisitedDependentColor(&nsStyleBackground::mBackgroundColor);

  nsRect innerRect;
  if (
#ifdef MOZ_XUL
      aStyleContext->GetPseudoType() == CSSPseudoElementType::XULTree
#else
      false
#endif
     ) {
    innerRect = aBorderArea;
  } else {
    innerRect = GetOutlineInnerRect(aForFrame) + aBorderArea.TopLeft();
  }
  nscoord offset = ourOutline->mOutlineOffset;
  innerRect.Inflate(offset, offset);
  // If the dirty rect is completely inside the border area (e.g., only the
  // content is being painted), then we can skip out now
  // XXX this isn't exactly true for rounded borders, where the inside curves may
  // encroach into the content area.  A safer calculation would be to
  // shorten insideRect by the radius one each side before performing this test.
  if (innerRect.Contains(aDirtyRect))
    return Nothing();

  nscoord width = ourOutline->GetOutlineWidth();

  nsRect outerRect = innerRect;
  outerRect.Inflate(width, width);

  // get the radius for our outline
  nsIFrame::ComputeBorderRadii(ourOutline->mOutlineRadius, aBorderArea.Size(),
                               outerRect.Size(), Sides(), twipsRadii);

  // Get our conversion values
  nscoord twipsPerPixel = aPresContext->DevPixelsToAppUnits(1);

  // get the outer rectangles
  Rect oRect(NSRectToRect(outerRect, twipsPerPixel));

  // convert the radii
  nsMargin outlineMargin(width, width, width, width);
  RectCornerRadii outlineRadii;
  ComputePixelRadii(twipsRadii, twipsPerPixel, &outlineRadii);

  uint8_t outlineStyle = ourOutline->mOutlineStyle;
  if (outlineStyle == NS_STYLE_BORDER_STYLE_AUTO) {
    if (nsLayoutUtils::IsOutlineStyleAutoEnabled()) {
      nsITheme* theme = aPresContext->GetTheme();
      if (theme && theme->ThemeSupportsWidget(aPresContext, aForFrame,
                                              NS_THEME_FOCUS_OUTLINE)) {
        theme->DrawWidgetBackground(aRenderingContext, aForFrame,
                                    NS_THEME_FOCUS_OUTLINE, innerRect,
                                    aDirtyRect);
        return Nothing();
      }
    }
    if (width == 0) {
      return Nothing(); // empty outline
    }
    // http://dev.w3.org/csswg/css-ui/#outline
    // "User agents may treat 'auto' as 'solid'."
    outlineStyle = NS_STYLE_BORDER_STYLE_SOLID;
  }

  uint8_t outlineStyles[4] = { outlineStyle, outlineStyle,
                               outlineStyle, outlineStyle };

  // This handles treating the initial color as 'currentColor'; if we
  // ever want 'invert' back we'll need to do a bit of work here too.
  nscolor outlineColor =
    aStyleContext->GetVisitedDependentColor(&nsStyleOutline::mOutlineColor);
  nscolor outlineColors[4] = { outlineColor,
                               outlineColor,
                               outlineColor,
                               outlineColor };

  // convert the border widths
  Float outlineWidths[4] = { Float(width) / twipsPerPixel,
                             Float(width) / twipsPerPixel,
                             Float(width) / twipsPerPixel,
                             Float(width) / twipsPerPixel };
  Rect dirtyRect = NSRectToRect(aDirtyRect, twipsPerPixel);

  nsIDocument* document = nullptr;
  nsIContent* content = aForFrame->GetContent();
  if (content) {
    document = content->OwnerDoc();
  }

  DrawTarget* dt = aRenderingContext ? aRenderingContext->GetDrawTarget() : nullptr;
  nsCSSBorderRenderer br(aPresContext,
                         document,
                         dt,
                         dirtyRect,
                         oRect,
                         outlineStyles,
                         outlineWidths,
                         outlineRadii,
                         outlineColors,
                         nullptr,
                         bgColor);

  return Some(br);
}

void
nsCSSRendering::PaintOutline(nsPresContext* aPresContext,
                             gfxContext& aRenderingContext,
                             nsIFrame* aForFrame,
                             const nsRect& aDirtyRect,
                             const nsRect& aBorderArea,
                             nsStyleContext* aStyleContext)
{
  Maybe<nsCSSBorderRenderer> br = CreateBorderRendererForOutline(aPresContext,
                                                                 &aRenderingContext,
                                                                 aForFrame,
                                                                 aDirtyRect,
                                                                 aBorderArea,
                                                                 aStyleContext);
  if (!br) {
    return;
  }

  // start drawing
  br->DrawBorders();

  PrintAsStringNewline();
}

void
nsCSSRendering::PaintFocus(nsPresContext* aPresContext,
                           DrawTarget* aDrawTarget,
                           const nsRect& aFocusRect,
                           nscolor aColor)
{
  nscoord oneCSSPixel = nsPresContext::CSSPixelsToAppUnits(1);
  nscoord oneDevPixel = aPresContext->DevPixelsToAppUnits(1);

  Rect focusRect(NSRectToRect(aFocusRect, oneDevPixel));

  RectCornerRadii focusRadii;
  {
    nscoord twipsRadii[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    ComputePixelRadii(twipsRadii, oneDevPixel, &focusRadii);
  }
  Float focusWidths[4] = { Float(oneCSSPixel) / oneDevPixel,
                           Float(oneCSSPixel) / oneDevPixel,
                           Float(oneCSSPixel) / oneDevPixel,
                           Float(oneCSSPixel) / oneDevPixel };

  uint8_t focusStyles[4] = { NS_STYLE_BORDER_STYLE_DOTTED,
                             NS_STYLE_BORDER_STYLE_DOTTED,
                             NS_STYLE_BORDER_STYLE_DOTTED,
                             NS_STYLE_BORDER_STYLE_DOTTED };
  nscolor focusColors[4] = { aColor, aColor, aColor, aColor };

  // Because this renders a dotted border, the background color
  // should not be used.  Therefore, we provide a value that will
  // be blatantly wrong if it ever does get used.  (If this becomes
  // something that CSS can style, this function will then have access
  // to a style context and can use the same logic that PaintBorder
  // and PaintOutline do.)
  nsCSSBorderRenderer br(aPresContext,
                         nullptr,
                         aDrawTarget,
                         focusRect,
                         focusRect,
                         focusStyles,
                         focusWidths,
                         focusRadii,
                         focusColors,
                         nullptr,
                         NS_RGB(255, 0, 0));
  br.DrawBorders();

  PrintAsStringNewline();
}

// Thebes Border Rendering Code End
//----------------------------------------------------------------------


//----------------------------------------------------------------------

/**
 * Helper for ComputeObjectAnchorPoint; parameters are the same as for
 * that function, except they're for a single coordinate / a single size
 * dimension. (so, x/width vs. y/height)
 */
static void
ComputeObjectAnchorCoord(const Position::Coord& aCoord,
                         const nscoord aOriginBounds,
                         const nscoord aImageSize,
                         nscoord* aTopLeftCoord,
                         nscoord* aAnchorPointCoord)
{
  *aAnchorPointCoord = aCoord.mLength;
  *aTopLeftCoord = aCoord.mLength;

  if (aCoord.mHasPercent) {
    // Adjust aTopLeftCoord by the specified % of the extra space.
    nscoord extraSpace = aOriginBounds - aImageSize;
    *aTopLeftCoord += NSToCoordRound(aCoord.mPercent * extraSpace);

    // The anchor-point doesn't care about our image's size; just the size
    // of the region we're rendering into.
    *aAnchorPointCoord += NSToCoordRound(aCoord.mPercent * aOriginBounds);
  }
}

void
nsImageRenderer::ComputeObjectAnchorPoint(
  const Position& aPos,
  const nsSize& aOriginBounds,
  const nsSize& aImageSize,
  nsPoint* aTopLeft,
  nsPoint* aAnchorPoint)
{
  ComputeObjectAnchorCoord(aPos.mXPosition,
                           aOriginBounds.width, aImageSize.width,
                           &aTopLeft->x, &aAnchorPoint->x);

  ComputeObjectAnchorCoord(aPos.mYPosition,
                           aOriginBounds.height, aImageSize.height,
                           &aTopLeft->y, &aAnchorPoint->y);
}

nsIFrame*
nsCSSRendering::FindNonTransparentBackgroundFrame(nsIFrame* aFrame,
                                                  bool aStartAtParent /*= false*/)
{
  NS_ASSERTION(aFrame, "Cannot find NonTransparentBackgroundFrame in a null frame");

  nsIFrame* frame = nullptr;
  if (aStartAtParent) {
    frame = nsLayoutUtils::GetParentOrPlaceholderFor(aFrame);
  }
  if (!frame) {
    frame = aFrame;
  }

  while (frame) {
    // No need to call GetVisitedDependentColor because it always uses
    // this alpha component anyway.
    if (NS_GET_A(frame->StyleBackground()->BackgroundColor(frame)) > 0) {
      break;
    }

    if (frame->IsThemed())
      break;

    nsIFrame* parent = nsLayoutUtils::GetParentOrPlaceholderFor(frame);
    if (!parent)
      break;

    frame = parent;
  }
  return frame;
}

// Returns true if aFrame is a canvas frame.
// We need to treat the viewport as canvas because, even though
// it does not actually paint a background, we need to get the right
// background style so we correctly detect transparent documents.
bool
nsCSSRendering::IsCanvasFrame(nsIFrame* aFrame)
{
  LayoutFrameType frameType = aFrame->Type();
  return frameType == LayoutFrameType::Canvas ||
         frameType == LayoutFrameType::Root ||
         frameType == LayoutFrameType::PageContent ||
         frameType == LayoutFrameType::Viewport;
}

nsIFrame*
nsCSSRendering::FindBackgroundStyleFrame(nsIFrame* aForFrame)
{
  const nsStyleBackground* result = aForFrame->StyleBackground();

  // Check if we need to do propagation from BODY rather than HTML.
  if (!result->IsTransparent(aForFrame)) {
    return aForFrame;
  }

  nsIContent* content = aForFrame->GetContent();
  // The root element content can't be null. We wouldn't know what
  // frame to create for aFrame.
  // Use |OwnerDoc| so it works during destruction.
  if (!content) {
    return aForFrame;
  }

  nsIDocument* document = content->OwnerDoc();

  dom::Element* bodyContent = document->GetBodyElement();
  // We need to null check the body node (bug 118829) since
  // there are cases, thanks to the fix for bug 5569, where we
  // will reflow a document with no body.  In particular, if a
  // SCRIPT element in the head blocks the parser and then has a
  // SCRIPT that does "document.location.href = 'foo'", then
  // nsParser::Terminate will call |DidBuildModel| methods
  // through to the content sink, which will call |StartLayout|
  // and thus |Initialize| on the pres shell.  See bug 119351
  // for the ugly details.
  if (!bodyContent) {
    return aForFrame;
  }

  nsIFrame *bodyFrame = bodyContent->GetPrimaryFrame();
  if (!bodyFrame) {
    return aForFrame;
  }

  return nsLayoutUtils::GetStyleFrame(bodyFrame);
}

/**
 * |FindBackground| finds the correct style data to use to paint the
 * background.  It is responsible for handling the following two
 * statements in section 14.2 of CSS2:
 *
 *   The background of the box generated by the root element covers the
 *   entire canvas.
 *
 *   For HTML documents, however, we recommend that authors specify the
 *   background for the BODY element rather than the HTML element. User
 *   agents should observe the following precedence rules to fill in the
 *   background: if the value of the 'background' property for the HTML
 *   element is different from 'transparent' then use it, else use the
 *   value of the 'background' property for the BODY element. If the
 *   resulting value is 'transparent', the rendering is undefined.
 *
 * Thus, in our implementation, it is responsible for ensuring that:
 *  + we paint the correct background on the |nsCanvasFrame|,
 *    |nsRootBoxFrame|, or |nsPageFrame|,
 *  + we don't paint the background on the root element, and
 *  + we don't paint the background on the BODY element in *some* cases,
 *    and for SGML-based HTML documents only.
 *
 * |FindBackground| returns true if a background should be painted, and
 * the resulting style context to use for the background information
 * will be filled in to |aBackground|.
 */
nsStyleContext*
nsCSSRendering::FindRootFrameBackground(nsIFrame* aForFrame)
{
  return FindBackgroundStyleFrame(aForFrame)->StyleContext();
}

inline bool
FindElementBackground(nsIFrame* aForFrame, nsIFrame* aRootElementFrame,
                      nsStyleContext** aBackgroundSC)
{
  if (aForFrame == aRootElementFrame) {
    // We must have propagated our background to the viewport or canvas. Abort.
    return false;
  }

  *aBackgroundSC = aForFrame->StyleContext();

  // Return true unless the frame is for a BODY element whose background
  // was propagated to the viewport.

  nsIContent* content = aForFrame->GetContent();
  if (!content || content->NodeInfo()->NameAtom() != nsGkAtoms::body)
    return true; // not frame for a "body" element
  // It could be a non-HTML "body" element but that's OK, we'd fail the
  // bodyContent check below

  if (aForFrame->StyleContext()->GetPseudo())
    return true; // A pseudo-element frame.

  // We should only look at the <html> background if we're in an HTML document
  nsIDocument* document = content->OwnerDoc();

  dom::Element* bodyContent = document->GetBodyElement();
  if (bodyContent != content)
    return true; // this wasn't the background that was propagated

  // This can be called even when there's no root element yet, during frame
  // construction, via nsLayoutUtils::FrameHasTransparency and
  // nsContainerFrame::SyncFrameViewProperties.
  if (!aRootElementFrame)
    return true;

  const nsStyleBackground* htmlBG = aRootElementFrame->StyleBackground();
  return !htmlBG->IsTransparent(aRootElementFrame);
}

bool
nsCSSRendering::FindBackground(nsIFrame* aForFrame,
                               nsStyleContext** aBackgroundSC)
{
  nsIFrame* rootElementFrame =
    aForFrame->PresContext()->PresShell()->FrameConstructor()->GetRootElementStyleFrame();
  if (IsCanvasFrame(aForFrame)) {
    *aBackgroundSC = FindCanvasBackground(aForFrame, rootElementFrame);
    return true;
  } else {
    return FindElementBackground(aForFrame, rootElementFrame, aBackgroundSC);
  }
}

void
nsCSSRendering::BeginFrameTreesLocked()
{
  ++gFrameTreeLockCount;
}

void
nsCSSRendering::EndFrameTreesLocked()
{
  NS_ASSERTION(gFrameTreeLockCount > 0, "Unbalanced EndFrameTreeLocked");
  --gFrameTreeLockCount;
  if (gFrameTreeLockCount == 0) {
    gInlineBGData->Reset();
  }
}

bool
nsCSSRendering::HasBoxShadowNativeTheme(nsIFrame* aFrame,
                                        bool& aMaybeHasBorderRadius)
{
  const nsStyleDisplay* styleDisplay = aFrame->StyleDisplay();
  nsITheme::Transparency transparency;
  if (aFrame->IsThemed(styleDisplay, &transparency)) {
    aMaybeHasBorderRadius = false;
    // For opaque (rectangular) theme widgets we can take the generic
    // border-box path with border-radius disabled.
    return transparency != nsITheme::eOpaque;
  }

  aMaybeHasBorderRadius = true;
  return false;
}

gfx::Color
nsCSSRendering::GetShadowColor(nsCSSShadowItem* aShadow,
                               nsIFrame* aFrame,
                               float aOpacity)
{
  // Get the shadow color; if not specified, use the foreground color
  nscolor shadowColor;
  if (aShadow->mHasColor)
    shadowColor = aShadow->mColor;
  else
    shadowColor = aFrame->StyleColor()->mColor;

  Color color = Color::FromABGR(shadowColor);
  color.a *= aOpacity;
  return color;
}

nsRect
nsCSSRendering::GetShadowRect(const nsRect aFrameArea,
                              bool aNativeTheme,
                              nsIFrame* aForFrame)
{
  nsRect frameRect = aNativeTheme ?
    aForFrame->GetVisualOverflowRectRelativeToSelf() + aFrameArea.TopLeft() :
    aFrameArea;
  Sides skipSides = aForFrame->GetSkipSides();
  frameRect = ::BoxDecorationRectForBorder(aForFrame, frameRect, skipSides);

  // Explicitly do not need to account for the spread radius here
  // Webrender does it for us or PaintBoxShadow will for non-WR
  return frameRect;
}

bool
nsCSSRendering::GetBorderRadii(const nsRect& aFrameRect,
                               const nsRect& aBorderRect,
                               nsIFrame* aFrame,
                               RectCornerRadii& aOutRadii)
{
  const nscoord twipsPerPixel = aFrame->PresContext()->DevPixelsToAppUnits(1);
  nscoord twipsRadii[8];
  NS_ASSERTION(aBorderRect.Size() == aFrame->VisualBorderRectRelativeToSelf().Size(),
              "unexpected size");
  nsSize sz = aFrameRect.Size();
  bool hasBorderRadius = aFrame->GetBorderRadii(sz, sz, Sides(), twipsRadii);
  if (hasBorderRadius) {
    ComputePixelRadii(twipsRadii, twipsPerPixel, &aOutRadii);
  }

  return hasBorderRadius;
}

void
nsCSSRendering::PaintBoxShadowOuter(nsPresContext* aPresContext,
                                    gfxContext& aRenderingContext,
                                    nsIFrame* aForFrame,
                                    const nsRect& aFrameArea,
                                    const nsRect& aDirtyRect,
                                    float aOpacity)
{
  DrawTarget& aDrawTarget = *aRenderingContext.GetDrawTarget();
  nsCSSShadowArray* shadows = aForFrame->StyleEffects()->mBoxShadow;
  if (!shadows)
    return;

  bool hasBorderRadius;
  // mutually exclusive with hasBorderRadius
  bool nativeTheme = HasBoxShadowNativeTheme(aForFrame, hasBorderRadius);
  const nsStyleDisplay* styleDisplay = aForFrame->StyleDisplay();

  nsRect frameRect = GetShadowRect(aFrameArea, nativeTheme, aForFrame);

  // Get any border radius, since box-shadow must also have rounded corners if
  // the frame does.
  RectCornerRadii borderRadii;
  const nscoord twipsPerPixel = aPresContext->DevPixelsToAppUnits(1);
  if (hasBorderRadius) {
    nscoord twipsRadii[8];
    NS_ASSERTION(aFrameArea.Size() == aForFrame->VisualBorderRectRelativeToSelf().Size(),
                 "unexpected size");
    nsSize sz = frameRect.Size();
    hasBorderRadius = aForFrame->GetBorderRadii(sz, sz, Sides(), twipsRadii);
    if (hasBorderRadius) {
      ComputePixelRadii(twipsRadii, twipsPerPixel, &borderRadii);
    }
  }


  // We don't show anything that intersects with the frame we're blurring on. So tell the
  // blurrer not to do unnecessary work there.
  gfxRect skipGfxRect = ThebesRect(NSRectToRect(frameRect, twipsPerPixel));
  skipGfxRect.Round();
  bool useSkipGfxRect = true;
  if (nativeTheme) {
    // Optimize non-leaf native-themed frames by skipping computing pixels
    // in the padding-box. We assume the padding-box is going to be painted
    // opaquely for non-leaf frames.
    // XXX this may not be a safe assumption; we should make this go away
    // by optimizing box-shadow drawing more for the cases where we don't have a skip-rect.
    useSkipGfxRect = !aForFrame->IsLeaf();
    nsRect paddingRect =
      aForFrame->GetPaddingRect() - aForFrame->GetPosition() + aFrameArea.TopLeft();
    skipGfxRect = nsLayoutUtils::RectToGfxRect(paddingRect, twipsPerPixel);
  } else if (hasBorderRadius) {
    skipGfxRect.Deflate(gfxMargin(
        std::max(borderRadii[C_TL].height, borderRadii[C_TR].height), 0,
        std::max(borderRadii[C_BL].height, borderRadii[C_BR].height), 0));
  }


  for (uint32_t i = shadows->Length(); i > 0; --i) {
    nsCSSShadowItem* shadowItem = shadows->ShadowAt(i - 1);
    if (shadowItem->mInset)
      continue;

    nsRect shadowRect = frameRect;
    shadowRect.MoveBy(shadowItem->mXOffset, shadowItem->mYOffset);
    if (!nativeTheme) {
      shadowRect.Inflate(shadowItem->mSpread, shadowItem->mSpread);
    }

    // shadowRect won't include the blur, so make an extra rect here that includes the blur
    // for use in the even-odd rule below.
    nsRect shadowRectPlusBlur = shadowRect;
    nscoord blurRadius = shadowItem->mRadius;
    shadowRectPlusBlur.Inflate(
      nsContextBoxBlur::GetBlurRadiusMargin(blurRadius, twipsPerPixel));

    Rect shadowGfxRectPlusBlur =
      NSRectToRect(shadowRectPlusBlur, twipsPerPixel);
    shadowGfxRectPlusBlur.RoundOut();
    MaybeSnapToDevicePixels(shadowGfxRectPlusBlur, aDrawTarget, true);

    Color gfxShadowColor = GetShadowColor(shadowItem, aForFrame, aOpacity);

    if (nativeTheme) {
      nsContextBoxBlur blurringArea;

      // When getting the widget shape from the native theme, we're going
      // to draw the widget into the shadow surface to create a mask.
      // We need to ensure that there actually *is* a shadow surface
      // and that we're not going to draw directly into aRenderingContext.
      gfxContext* shadowContext =
        blurringArea.Init(shadowRect, shadowItem->mSpread, blurRadius,
                          twipsPerPixel, &aRenderingContext, aDirtyRect,
                          useSkipGfxRect ? &skipGfxRect : nullptr,
                          nsContextBoxBlur::FORCE_MASK);
      if (!shadowContext)
        continue;

      MOZ_ASSERT(shadowContext == blurringArea.GetContext());

      aRenderingContext.Save();
      aRenderingContext.SetColor(gfxShadowColor);

      // Draw the shape of the frame so it can be blurred. Recall how nsContextBoxBlur
      // doesn't make any temporary surfaces if blur is 0 and it just returns the original
      // surface? If we have no blur, we're painting this fill on the actual content surface
      // (aRenderingContext == shadowContext) which is why we set up the color and clip
      // before doing this.

      // We don't clip the border-box from the shadow, nor any other box.
      // We assume that the native theme is going to paint over the shadow.

      // Draw the widget shape
      gfxContextMatrixAutoSaveRestore save(shadowContext);
      gfxPoint devPixelOffset =
        nsLayoutUtils::PointToGfxPoint(nsPoint(shadowItem->mXOffset,
                                               shadowItem->mYOffset),
                                       aPresContext->AppUnitsPerDevPixel());
      shadowContext->SetMatrix(
        shadowContext->CurrentMatrix().PreTranslate(devPixelOffset));

      nsRect nativeRect = aDirtyRect;
      nativeRect.MoveBy(-nsPoint(shadowItem->mXOffset, shadowItem->mYOffset));
      nativeRect.IntersectRect(frameRect, nativeRect);
      aPresContext->GetTheme()->DrawWidgetBackground(shadowContext, aForFrame,
          styleDisplay->mAppearance, aFrameArea, nativeRect);

      blurringArea.DoPaint();
      aRenderingContext.Restore();
    } else {
      aRenderingContext.Save();

      {
        Rect innerClipRect = NSRectToRect(frameRect, twipsPerPixel);
        if (!MaybeSnapToDevicePixels(innerClipRect, aDrawTarget, true)) {
          innerClipRect.Round();
        }

        // Clip out the interior of the frame's border edge so that the shadow
        // is only painted outside that area.
        RefPtr<PathBuilder> builder =
          aDrawTarget.CreatePathBuilder(FillRule::FILL_EVEN_ODD);
        AppendRectToPath(builder, shadowGfxRectPlusBlur);
        if (hasBorderRadius) {
          AppendRoundedRectToPath(builder, innerClipRect, borderRadii);
        } else {
          AppendRectToPath(builder, innerClipRect);
        }
        RefPtr<Path> path = builder->Finish();
        aRenderingContext.Clip(path);
      }

      // Clip the shadow so that we only get the part that applies to aForFrame.
      nsRect fragmentClip = shadowRectPlusBlur;
      Sides skipSides = aForFrame->GetSkipSides();
      if (!skipSides.IsEmpty()) {
        if (skipSides.Left()) {
          nscoord xmost = fragmentClip.XMost();
          fragmentClip.x = aFrameArea.x;
          fragmentClip.width = xmost - fragmentClip.x;
        }
        if (skipSides.Right()) {
          nscoord xmost = fragmentClip.XMost();
          nscoord overflow = xmost - aFrameArea.XMost();
          if (overflow > 0) {
            fragmentClip.width -= overflow;
          }
        }
        if (skipSides.Top()) {
          nscoord ymost = fragmentClip.YMost();
          fragmentClip.y = aFrameArea.y;
          fragmentClip.height = ymost - fragmentClip.y;
        }
        if (skipSides.Bottom()) {
          nscoord ymost = fragmentClip.YMost();
          nscoord overflow = ymost - aFrameArea.YMost();
          if (overflow > 0) {
            fragmentClip.height -= overflow;
          }
        }
      }
      fragmentClip = fragmentClip.Intersect(aDirtyRect);
      aRenderingContext.
        Clip(NSRectToSnappedRect(fragmentClip,
                                 aForFrame->PresContext()->AppUnitsPerDevPixel(),
                                 aDrawTarget));

      RectCornerRadii clipRectRadii;
      if (hasBorderRadius) {
        Float spreadDistance = Float(shadowItem->mSpread) / twipsPerPixel;

        Float borderSizes[4];

        borderSizes[eSideLeft] = spreadDistance;
        borderSizes[eSideTop] = spreadDistance;
        borderSizes[eSideRight] = spreadDistance;
        borderSizes[eSideBottom] = spreadDistance;

        nsCSSBorderRenderer::ComputeOuterRadii(borderRadii, borderSizes,
            &clipRectRadii);

      }
      nsContextBoxBlur::BlurRectangle(&aRenderingContext,
                                      shadowRect,
                                      twipsPerPixel,
                                      hasBorderRadius ? &clipRectRadii : nullptr,
                                      blurRadius,
                                      gfxShadowColor,
                                      aDirtyRect,
                                      skipGfxRect);
      aRenderingContext.Restore();
    }

  }
}

nsRect
nsCSSRendering::GetBoxShadowInnerPaddingRect(nsIFrame* aFrame,
                                             const nsRect& aFrameArea)
{
  Sides skipSides = aFrame->GetSkipSides();
  nsRect frameRect =
    ::BoxDecorationRectForBorder(aFrame, aFrameArea, skipSides);

  nsRect paddingRect = frameRect;
  nsMargin border = aFrame->GetUsedBorder();
  paddingRect.Deflate(border);
  return paddingRect;
}

bool
nsCSSRendering::ShouldPaintBoxShadowInner(nsIFrame* aFrame)
{
  nsCSSShadowArray* shadows = aFrame->StyleEffects()->mBoxShadow;
  if (!shadows)
    return false;

  if (aFrame->IsThemed() && aFrame->GetContent() &&
      !nsContentUtils::IsChromeDoc(aFrame->GetContent()->GetUncomposedDoc())) {
    // There's no way of getting hold of a shape corresponding to a
    // "padding-box" for native-themed widgets, so just don't draw
    // inner box-shadows for them. But we allow chrome to paint inner
    // box shadows since chrome can be aware of the platform theme.
    return false;
  }

  return true;
}

bool
nsCSSRendering::GetShadowInnerRadii(nsIFrame* aFrame,
                                    const nsRect& aFrameArea,
                                    RectCornerRadii& aOutInnerRadii)
{
  // Get any border radius, since box-shadow must also have rounded corners
  // if the frame does.
  nscoord twipsRadii[8];
  nsRect frameRect =
    ::BoxDecorationRectForBorder(aFrame, aFrameArea, aFrame->GetSkipSides());
  nsSize sz = frameRect.Size();
  nsMargin border = aFrame->GetUsedBorder();
  bool hasBorderRadius = aFrame->GetBorderRadii(sz, sz, Sides(), twipsRadii);
  const nscoord twipsPerPixel = aFrame->PresContext()->DevPixelsToAppUnits(1);

  RectCornerRadii borderRadii;

  hasBorderRadius = GetBorderRadii(frameRect, aFrameArea, aFrame, borderRadii);
  if (hasBorderRadius) {
    ComputePixelRadii(twipsRadii, twipsPerPixel, &borderRadii);

    Float borderSizes[4] = {
      Float(border.top) / twipsPerPixel,
      Float(border.right) / twipsPerPixel,
      Float(border.bottom) / twipsPerPixel,
      Float(border.left) / twipsPerPixel
    };
    nsCSSBorderRenderer::ComputeInnerRadii(borderRadii,
                                           borderSizes,
                                           &aOutInnerRadii);
  }

  return hasBorderRadius;
}

void
nsCSSRendering::PaintBoxShadowInner(nsPresContext* aPresContext,
                                    gfxContext& aRenderingContext,
                                    nsIFrame* aForFrame,
                                    const nsRect& aFrameArea)
{
  if (!ShouldPaintBoxShadowInner(aForFrame)) {
    return;
  }

  nsCSSShadowArray* shadows = aForFrame->StyleEffects()->mBoxShadow;
  NS_ASSERTION(aForFrame->IsFieldSetFrame() ||
               aFrameArea.Size() == aForFrame->GetSize(), "unexpected size");

  nsRect paddingRect = GetBoxShadowInnerPaddingRect(aForFrame, aFrameArea);

  RectCornerRadii innerRadii;
  bool hasBorderRadius = GetShadowInnerRadii(aForFrame,
                                             aFrameArea,
                                             innerRadii);

  const nscoord twipsPerPixel = aPresContext->DevPixelsToAppUnits(1);

  for (uint32_t i = shadows->Length(); i > 0; --i) {
    nsCSSShadowItem* shadowItem = shadows->ShadowAt(i - 1);
    if (!shadowItem->mInset)
      continue;

    // shadowPaintRect: the area to paint on the temp surface
    // shadowClipRect: the area on the temporary surface within shadowPaintRect
    //                 that we will NOT paint in
    nscoord blurRadius = shadowItem->mRadius;
    nsMargin blurMargin =
      nsContextBoxBlur::GetBlurRadiusMargin(blurRadius, twipsPerPixel);
    nsRect shadowPaintRect = paddingRect;
    shadowPaintRect.Inflate(blurMargin);

    // Round the spread radius to device pixels (by truncation).
    // This mostly matches what we do for borders, except that we don't round
    // up values between zero and one device pixels to one device pixel.
    // This way of rounding is symmetric around zero, which makes sense for
    // the spread radius.
    int32_t spreadDistance = shadowItem->mSpread / twipsPerPixel;
    nscoord spreadDistanceAppUnits = aPresContext->DevPixelsToAppUnits(spreadDistance);

    nsRect shadowClipRect = paddingRect;
    shadowClipRect.MoveBy(shadowItem->mXOffset, shadowItem->mYOffset);
    shadowClipRect.Deflate(spreadDistanceAppUnits, spreadDistanceAppUnits);

    Rect shadowClipGfxRect = NSRectToRect(shadowClipRect, twipsPerPixel);
    shadowClipGfxRect.Round();

    RectCornerRadii clipRectRadii;
    if (hasBorderRadius) {
      // Calculate the radii the inner clipping rect will have
      Float borderSizes[4] = {0, 0, 0, 0};

      // See PaintBoxShadowOuter and bug 514670
      if (innerRadii[C_TL].width > 0 || innerRadii[C_BL].width > 0) {
        borderSizes[eSideLeft] = spreadDistance;
      }

      if (innerRadii[C_TL].height > 0 || innerRadii[C_TR].height > 0) {
        borderSizes[eSideTop] = spreadDistance;
      }

      if (innerRadii[C_TR].width > 0 || innerRadii[C_BR].width > 0) {
        borderSizes[eSideRight] = spreadDistance;
      }

      if (innerRadii[C_BL].height > 0 || innerRadii[C_BR].height > 0) {
        borderSizes[eSideBottom] = spreadDistance;
      }

      nsCSSBorderRenderer::ComputeInnerRadii(innerRadii, borderSizes,
                                             &clipRectRadii);
    }

    // Set the "skip rect" to the area within the frame that we don't paint in,
    // including after blurring.
    nsRect skipRect = shadowClipRect;
    skipRect.Deflate(blurMargin);
    gfxRect skipGfxRect = nsLayoutUtils::RectToGfxRect(skipRect, twipsPerPixel);
    if (hasBorderRadius) {
      skipGfxRect.Deflate(gfxMargin(
          std::max(clipRectRadii[C_TL].height, clipRectRadii[C_TR].height), 0,
          std::max(clipRectRadii[C_BL].height, clipRectRadii[C_BR].height), 0));
    }

    // When there's a blur radius, gfxAlphaBoxBlur leaves the skiprect area
    // unchanged. And by construction the gfxSkipRect is not touched by the
    // rendered shadow (even after blurring), so those pixels must be completely
    // transparent in the shadow, so drawing them changes nothing.
    DrawTarget* drawTarget = aRenderingContext.GetDrawTarget();

    // Clip the context to the area of the frame's padding rect, so no part of the
    // shadow is painted outside. Also cut out anything beyond where the inset shadow
    // will be.
    Rect shadowGfxRect = NSRectToRect(paddingRect, twipsPerPixel);
    shadowGfxRect.Round();

    Color shadowColor = GetShadowColor(shadowItem, aForFrame, 1.0);
    aRenderingContext.Save();

    // This clips the outside border radius.
    // clipRectRadii is the border radius inside the inset shadow.
    if (hasBorderRadius) {
      RefPtr<Path> roundedRect =
        MakePathForRoundedRect(*drawTarget, shadowGfxRect, innerRadii);
      aRenderingContext.Clip(roundedRect);
    } else {
      aRenderingContext.Clip(shadowGfxRect);
    }

    nsContextBoxBlur insetBoxBlur;
    gfxRect destRect = nsLayoutUtils::RectToGfxRect(shadowPaintRect, twipsPerPixel);
    Point shadowOffset(shadowItem->mXOffset / twipsPerPixel,
                       shadowItem->mYOffset / twipsPerPixel);

    insetBoxBlur.InsetBoxBlur(&aRenderingContext, ToRect(destRect),
                              shadowClipGfxRect, shadowColor,
                              blurRadius, spreadDistanceAppUnits,
                              twipsPerPixel, hasBorderRadius,
                              clipRectRadii, ToRect(skipGfxRect),
                              shadowOffset);
    aRenderingContext.Restore();
  }
}

/* static */
nsCSSRendering::PaintBGParams
nsCSSRendering::PaintBGParams::ForAllLayers(nsPresContext& aPresCtx,
                                            const nsRect& aDirtyRect,
                                            const nsRect& aBorderArea,
                                            nsIFrame *aFrame,
                                            uint32_t aPaintFlags,
                                            float aOpacity)
{
  MOZ_ASSERT(aFrame);

  PaintBGParams result(aPresCtx, aDirtyRect, aBorderArea,
                       aFrame, aPaintFlags, -1, CompositionOp::OP_OVER,
                       aOpacity);

  return result;
}

/* static */
nsCSSRendering::PaintBGParams
nsCSSRendering::PaintBGParams::ForSingleLayer(nsPresContext& aPresCtx,
                                              const nsRect& aDirtyRect,
                                              const nsRect& aBorderArea,
                                              nsIFrame *aFrame,
                                              uint32_t aPaintFlags,
                                              int32_t aLayer,
                                              CompositionOp aCompositionOp,
                                              float aOpacity)
{
  MOZ_ASSERT(aFrame && (aLayer != -1));

  PaintBGParams result(aPresCtx, aDirtyRect, aBorderArea,
                       aFrame, aPaintFlags, aLayer, aCompositionOp,
                       aOpacity);

  return result;
}

DrawResult
nsCSSRendering::PaintStyleImageLayer(const PaintBGParams& aParams,
                                     gfxContext& aRenderingCtx)
{
  AUTO_PROFILER_LABEL("nsCSSRendering::PaintStyleImageLayer", GRAPHICS);

  NS_PRECONDITION(aParams.frame,
                  "Frame is expected to be provided to PaintStyleImageLayer");

  nsStyleContext *sc;
  if (!FindBackground(aParams.frame, &sc)) {
    // We don't want to bail out if moz-appearance is set on a root
    // node. If it has a parent content node, bail because it's not
    // a root, otherwise keep going in order to let the theme stuff
    // draw the background. The canvas really should be drawing the
    // bg, but there's no way to hook that up via css.
    if (!aParams.frame->StyleDisplay()->mAppearance) {
      return DrawResult::SUCCESS;
    }

    nsIContent* content = aParams.frame->GetContent();
    if (!content || content->GetParent()) {
      return DrawResult::SUCCESS;
    }

    sc = aParams.frame->StyleContext();
  }

  return PaintStyleImageLayerWithSC(aParams, aRenderingCtx, sc, *aParams.frame->StyleBorder());
}

bool
nsCSSRendering::CanBuildWebRenderDisplayItemsForStyleImageLayer(LayerManager* aManager,
                                                                nsPresContext& aPresCtx,
                                                                nsIFrame *aFrame,
                                                                const nsStyleBackground* aBackgroundStyle,
                                                                int32_t aLayer)
{
  if (!aBackgroundStyle) {
    return false;
  }

  MOZ_ASSERT(aFrame &&
             aLayer >= 0 &&
             (uint32_t)aLayer < aBackgroundStyle->mImage.mLayers.Length());

  // We cannot draw native themed backgrounds
  const nsStyleDisplay* displayData = aFrame->StyleDisplay();
  if (displayData->mAppearance) {
    nsITheme *theme = aPresCtx.GetTheme();
    if (theme && theme->ThemeSupportsWidget(&aPresCtx,
                                            aFrame,
                                            displayData->mAppearance)) {
      return false;
    }
  }

  // We only support painting gradients and image for a single style image layer
  const nsStyleImage* styleImage = &aBackgroundStyle->mImage.mLayers[aLayer].mImage;
  if (styleImage->GetType() == eStyleImageType_Image) {
    if (styleImage->GetCropRect()) {
      return false;
    }

    imgRequestProxy* requestProxy = styleImage->GetImageData();
    if (!requestProxy) {
      return false;
    }

    nsCOMPtr<imgIContainer> srcImage;
    requestProxy->GetImage(getter_AddRefs(srcImage));
    if (!srcImage || !srcImage->IsImageContainerAvailable(aManager, imgIContainer::FLAG_NONE)) {
      return false;
    }

    return true;
  }

  if (styleImage->GetType() == eStyleImageType_Gradient) {
    return true;
  }

  return false;
}

DrawResult
nsCSSRendering::BuildWebRenderDisplayItemsForStyleImageLayer(const PaintBGParams& aParams,
                                                             mozilla::wr::DisplayListBuilder& aBuilder,
                                                             const mozilla::layers::StackingContextHelper& aSc,
                                                             nsTArray<WebRenderParentCommand>& aParentCommands,
                                                             mozilla::layers::WebRenderDisplayItemLayer* aLayer,
                                                             mozilla::layers::WebRenderLayerManager* aManager,
                                                             nsDisplayItem* aItem)
{
  NS_PRECONDITION(aParams.frame,
                  "Frame is expected to be provided to BuildWebRenderDisplayItemsForStyleImageLayer");

  nsStyleContext *sc;
  if (!FindBackground(aParams.frame, &sc)) {
    // We don't want to bail out if moz-appearance is set on a root
    // node. If it has a parent content node, bail because it's not
    // a root, otherwise keep going in order to let the theme stuff
    // draw the background. The canvas really should be drawing the
    // bg, but there's no way to hook that up via css.
    if (!aParams.frame->StyleDisplay()->mAppearance) {
      return DrawResult::SUCCESS;
    }

    nsIContent* content = aParams.frame->GetContent();
    if (!content || content->GetParent()) {
      return DrawResult::SUCCESS;
    }

    sc = aParams.frame->StyleContext();
  }
  return BuildWebRenderDisplayItemsForStyleImageLayerWithSC(aParams, aBuilder, aSc, aParentCommands,
                                                            aLayer, aManager, aItem,
                                                            sc, *aParams.frame->StyleBorder());
}

static bool
IsOpaqueBorderEdge(const nsStyleBorder& aBorder, mozilla::Side aSide)
{
  if (aBorder.GetComputedBorder().Side(aSide) == 0)
    return true;
  switch (aBorder.GetBorderStyle(aSide)) {
  case NS_STYLE_BORDER_STYLE_SOLID:
  case NS_STYLE_BORDER_STYLE_GROOVE:
  case NS_STYLE_BORDER_STYLE_RIDGE:
  case NS_STYLE_BORDER_STYLE_INSET:
  case NS_STYLE_BORDER_STYLE_OUTSET:
    break;
  default:
    return false;
  }

  // If we're using a border image, assume it's not fully opaque,
  // because we may not even have the image loaded at this point, and
  // even if we did, checking whether the relevant tile is fully
  // opaque would be too much work.
  if (aBorder.mBorderImageSource.GetType() != eStyleImageType_Null)
    return false;

  StyleComplexColor color = aBorder.mBorderColor[aSide];
  // We don't know the foreground color here, so if it's being used
  // we must assume it might be transparent.
  if (!color.IsNumericColor()) {
    return false;
  }
  return NS_GET_A(color.mColor) == 255;
}

/**
 * Returns true if all border edges are either missing or opaque.
 */
static bool
IsOpaqueBorder(const nsStyleBorder& aBorder)
{
  if (aBorder.mBorderColors)
    return false;
  NS_FOR_CSS_SIDES(i) {
    if (!IsOpaqueBorderEdge(aBorder, i))
      return false;
  }
  return true;
}

static inline void
SetupDirtyRects(const nsRect& aBGClipArea, const nsRect& aCallerDirtyRect,
                nscoord aAppUnitsPerPixel,
                /* OUT: */
                nsRect* aDirtyRect, gfxRect* aDirtyRectGfx)
{
  aDirtyRect->IntersectRect(aBGClipArea, aCallerDirtyRect);

  // Compute the Thebes equivalent of the dirtyRect.
  *aDirtyRectGfx = nsLayoutUtils::RectToGfxRect(*aDirtyRect, aAppUnitsPerPixel);
  NS_WARNING_ASSERTION(aDirtyRect->IsEmpty() || !aDirtyRectGfx->IsEmpty(),
                       "converted dirty rect should not be empty");
  MOZ_ASSERT(!aDirtyRect->IsEmpty() || aDirtyRectGfx->IsEmpty(),
             "second should be empty if first is");
}

static bool
IsSVGStyleGeometryBox(StyleGeometryBox aBox)
{
  return (aBox == StyleGeometryBox::FillBox ||
          aBox == StyleGeometryBox::StrokeBox ||
          aBox == StyleGeometryBox::ViewBox);
}

static bool
IsHTMLStyleGeometryBox(StyleGeometryBox aBox)
{
  return (aBox == StyleGeometryBox::ContentBox ||
          aBox == StyleGeometryBox::PaddingBox ||
          aBox == StyleGeometryBox::BorderBox ||
          aBox == StyleGeometryBox::MarginBox);
}

static StyleGeometryBox
ComputeBoxValue(nsIFrame* aForFrame, StyleGeometryBox aBox)
{
  if (!(aForFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT)) {
    // For elements with associated CSS layout box, the values fill-box,
    // stroke-box and view-box compute to the initial value of mask-clip.
    if (IsSVGStyleGeometryBox(aBox)) {
      return StyleGeometryBox::BorderBox;
    }
  } else {
    // For SVG elements without associated CSS layout box, the values
    // content-box, padding-box, border-box and margin-box compute to fill-box.
    if (IsHTMLStyleGeometryBox(aBox)) {
      return StyleGeometryBox::FillBox;
    }
  }

  return aBox;
}

bool
nsCSSRendering::ImageLayerClipState::IsValid() const
{
  // mDirtyRectInDevPx comes from mDirtyRectInAppUnits. mDirtyRectInAppUnits
  // can not be empty if mDirtyRectInDevPx is not.
  if (!mDirtyRectInDevPx.IsEmpty() && mDirtyRectInAppUnits.IsEmpty()) {
    return false;
  }

  if (mHasRoundedCorners == mClippedRadii.IsEmpty()) {
    return false;
  }

  return true;
}

/* static */ void
nsCSSRendering::GetImageLayerClip(const nsStyleImageLayers::Layer& aLayer,
                                  nsIFrame* aForFrame, const nsStyleBorder& aBorder,
                                  const nsRect& aBorderArea, const nsRect& aCallerDirtyRect,
                                  bool aWillPaintBorder, nscoord aAppUnitsPerPixel,
                                  /* out */ ImageLayerClipState* aClipState)
{
  aClipState->mHasRoundedCorners = false;
  aClipState->mHasAdditionalBGClipArea = false;
  aClipState->mAdditionalBGClipArea.SetEmpty();
  aClipState->mCustomClip = false;

  StyleGeometryBox layerClip = ComputeBoxValue(aForFrame, aLayer.mClip);
  if (IsSVGStyleGeometryBox(layerClip)) {
    MOZ_ASSERT(aForFrame->IsFrameOfType(nsIFrame::eSVG) &&
               !aForFrame->IsSVGOuterSVGFrame());

    // The coordinate space of clipArea is svg user space.
    nsRect clipArea =
      nsLayoutUtils::ComputeGeometryBox(aForFrame, layerClip);

    nsRect strokeBox = (layerClip == StyleGeometryBox::StrokeBox)
      ? clipArea
      : nsLayoutUtils::ComputeGeometryBox(aForFrame, StyleGeometryBox::StrokeBox);
    nsRect clipAreaRelativeToStrokeBox = clipArea - strokeBox.TopLeft();

    // aBorderArea is the stroke-box area in a coordinate space defined by
    // the caller. This coordinate space can be svg user space of aForFrame,
    // the space of aForFrame's reference-frame, or anything else.
    //
    // Which coordinate space chosen for aBorderArea is not matter. What
    // matter is to ensure returning aClipState->mBGClipArea in the consistent
    // coordiante space with aBorderArea. So we evaluate the position of clip
    // area base on the position of aBorderArea here.
    aClipState->mBGClipArea =
      clipAreaRelativeToStrokeBox + aBorderArea.TopLeft();

    SetupDirtyRects(aClipState->mBGClipArea, aCallerDirtyRect,
                    aAppUnitsPerPixel, &aClipState->mDirtyRectInAppUnits,
                    &aClipState->mDirtyRectInDevPx);
    MOZ_ASSERT(aClipState->IsValid());
    return;
  }

  if (layerClip == StyleGeometryBox::NoClip) {
    aClipState->mBGClipArea = aCallerDirtyRect;

    SetupDirtyRects(aClipState->mBGClipArea, aCallerDirtyRect,
                    aAppUnitsPerPixel, &aClipState->mDirtyRectInAppUnits,
                    &aClipState->mDirtyRectInDevPx);
    MOZ_ASSERT(aClipState->IsValid());
    return;
  }

  MOZ_ASSERT(!aForFrame->IsFrameOfType(nsIFrame::eSVG) ||
             aForFrame->IsSVGOuterSVGFrame());

  // Compute the outermost boundary of the area that might be painted.
  // Same coordinate space as aBorderArea.
  Sides skipSides = aForFrame->GetSkipSides();
  nsRect clipBorderArea =
    ::BoxDecorationRectForBorder(aForFrame, aBorderArea, skipSides, &aBorder);

  bool haveRoundedCorners = false;
  LayoutFrameType fType = aForFrame->Type();
  if (fType != LayoutFrameType::TableColGroup &&
      fType != LayoutFrameType::TableCol &&
      fType != LayoutFrameType::TableRow &&
      fType != LayoutFrameType::TableRowGroup) {
    haveRoundedCorners = GetRadii(aForFrame, aBorder, aBorderArea,
                                  clipBorderArea, aClipState->mRadii);
  }
  bool isSolidBorder =
      aWillPaintBorder && IsOpaqueBorder(aBorder);
  if (isSolidBorder && layerClip == StyleGeometryBox::BorderBox) {
    // If we have rounded corners, we need to inflate the background
    // drawing area a bit to avoid seams between the border and
    // background.
    layerClip = haveRoundedCorners
                     ? StyleGeometryBox::MozAlmostPadding
                     : StyleGeometryBox::PaddingBox;
  }

  aClipState->mBGClipArea = clipBorderArea;

  if (aForFrame->IsScrollFrame() &&
      NS_STYLE_IMAGELAYER_ATTACHMENT_LOCAL == aLayer.mAttachment) {
    // As of this writing, this is still in discussion in the CSS Working Group
    // http://lists.w3.org/Archives/Public/www-style/2013Jul/0250.html

    // The rectangle for 'background-clip' scrolls with the content,
    // but the background is also clipped at a non-scrolling 'padding-box'
    // like the content. (See below.)
    // Therefore, only 'content-box' makes a difference here.
    if (layerClip == StyleGeometryBox::ContentBox) {
      nsIScrollableFrame* scrollableFrame = do_QueryFrame(aForFrame);
      // Clip at a rectangle attached to the scrolled content.
      aClipState->mHasAdditionalBGClipArea = true;
      aClipState->mAdditionalBGClipArea = nsRect(
        aClipState->mBGClipArea.TopLeft()
          + scrollableFrame->GetScrolledFrame()->GetPosition()
          // For the dir=rtl case:
          + scrollableFrame->GetScrollRange().TopLeft(),
        scrollableFrame->GetScrolledRect().Size());
      nsMargin padding = aForFrame->GetUsedPadding();
      // padding-bottom is ignored on scrollable frames:
      // https://bugzilla.mozilla.org/show_bug.cgi?id=748518
      padding.bottom = 0;
      padding.ApplySkipSides(skipSides);
      aClipState->mAdditionalBGClipArea.Deflate(padding);
    }

    // Also clip at a non-scrolling, rounded-corner 'padding-box',
    // same as the scrolled content because of the 'overflow' property.
    layerClip = StyleGeometryBox::PaddingBox;
  }

  // See the comment of StyleGeometryBox::Margin.
  // Hitting this assertion means we decide to turn on margin-box support for
  // positioned mask from CSS parser and style system. In this case, you
  // should *inflate* mBGClipArea by the margin returning from
  // aForFrame->GetUsedMargin() in the code chunk bellow.
  MOZ_ASSERT(layerClip != StyleGeometryBox::MarginBox,
             "StyleGeometryBox::MarginBox rendering is not supported yet.\n");

  if (layerClip != StyleGeometryBox::BorderBox &&
      layerClip != StyleGeometryBox::Text) {
    nsMargin border = aForFrame->GetUsedBorder();
    if (layerClip == StyleGeometryBox::MozAlmostPadding) {
      // Reduce |border| by 1px (device pixels) on all sides, if
      // possible, so that we don't get antialiasing seams between the
      // {background|mask} and border.
      border.top = std::max(0, border.top - aAppUnitsPerPixel);
      border.right = std::max(0, border.right - aAppUnitsPerPixel);
      border.bottom = std::max(0, border.bottom - aAppUnitsPerPixel);
      border.left = std::max(0, border.left - aAppUnitsPerPixel);
    } else if (layerClip != StyleGeometryBox::PaddingBox) {
      NS_ASSERTION(layerClip == StyleGeometryBox::ContentBox,
                   "unexpected background-clip");
      border += aForFrame->GetUsedPadding();
    }
    border.ApplySkipSides(skipSides);
    aClipState->mBGClipArea.Deflate(border);

    if (haveRoundedCorners) {
      nsIFrame::InsetBorderRadii(aClipState->mRadii, border);
    }
  }

  if (haveRoundedCorners) {
    auto d2a = aForFrame->PresContext()->AppUnitsPerDevPixel();
    nsCSSRendering::ComputePixelRadii(aClipState->mRadii, d2a, &aClipState->mClippedRadii);
    aClipState->mHasRoundedCorners = !aClipState->mClippedRadii.IsEmpty();
  }


  if (!haveRoundedCorners && aClipState->mHasAdditionalBGClipArea) {
    // Do the intersection here to account for the fast path(?) below.
    aClipState->mBGClipArea =
      aClipState->mBGClipArea.Intersect(aClipState->mAdditionalBGClipArea);
    aClipState->mHasAdditionalBGClipArea = false;
  }

  SetupDirtyRects(aClipState->mBGClipArea, aCallerDirtyRect, aAppUnitsPerPixel,
                  &aClipState->mDirtyRectInAppUnits,
                  &aClipState->mDirtyRectInDevPx);

  MOZ_ASSERT(aClipState->IsValid());
}

static void
SetupImageLayerClip(nsCSSRendering::ImageLayerClipState& aClipState,
                    gfxContext *aCtx, nscoord aAppUnitsPerPixel,
                    gfxContextAutoSaveRestore* aAutoSR)
{
  if (aClipState.mDirtyRectInDevPx.IsEmpty()) {
    // Our caller won't draw anything under this condition, so no need
    // to set more up.
    return;
  }

  if (aClipState.mCustomClip) {
    // We don't support custom clips and rounded corners, arguably a bug, but
    // table painting seems to depend on it.
    return;
  }

  // If we have rounded corners, clip all subsequent drawing to the
  // rounded rectangle defined by bgArea and bgRadii (we don't know
  // whether the rounded corners intrude on the dirtyRect or not).
  // Do not do this if we have a caller-provided clip rect --
  // as above with bgArea, arguably a bug, but table painting seems
  // to depend on it.

  if (aClipState.mHasAdditionalBGClipArea) {
    gfxRect bgAreaGfx = nsLayoutUtils::RectToGfxRect(
      aClipState.mAdditionalBGClipArea, aAppUnitsPerPixel);
    bgAreaGfx.Round();
    gfxUtils::ConditionRect(bgAreaGfx);

    aAutoSR->EnsureSaved(aCtx);
    aCtx->NewPath();
    aCtx->Rectangle(bgAreaGfx, true);
    aCtx->Clip();
  }

  if (aClipState.mHasRoundedCorners) {
    Rect bgAreaGfx = NSRectToRect(aClipState.mBGClipArea, aAppUnitsPerPixel);
    bgAreaGfx.Round();

    if (bgAreaGfx.IsEmpty()) {
      // I think it's become possible to hit this since
      // https://hg.mozilla.org/mozilla-central/rev/50e934e4979b landed.
      NS_WARNING("converted background area should not be empty");
      // Make our caller not do anything.
      aClipState.mDirtyRectInDevPx.SizeTo(gfxSize(0.0, 0.0));
      return;
    }

    aAutoSR->EnsureSaved(aCtx);

    RefPtr<Path> roundedRect =
      MakePathForRoundedRect(*aCtx->GetDrawTarget(), bgAreaGfx,
                             aClipState.mClippedRadii);
    aCtx->Clip(roundedRect);
  }
}

static void
DrawBackgroundColor(nsCSSRendering::ImageLayerClipState& aClipState,
                    gfxContext *aCtx, nscoord aAppUnitsPerPixel)
{
  if (aClipState.mDirtyRectInDevPx.IsEmpty()) {
    // Our caller won't draw anything under this condition, so no need
    // to set more up.
    return;
  }

  DrawTarget* drawTarget = aCtx->GetDrawTarget();

  // We don't support custom clips and rounded corners, arguably a bug, but
  // table painting seems to depend on it.
  if (!aClipState.mHasRoundedCorners || aClipState.mCustomClip) {
    aCtx->NewPath();
    aCtx->Rectangle(aClipState.mDirtyRectInDevPx, true);
    aCtx->Fill();
    return;
  }

  Rect bgAreaGfx = NSRectToRect(aClipState.mBGClipArea, aAppUnitsPerPixel);
  bgAreaGfx.Round();

  if (bgAreaGfx.IsEmpty()) {
    // I think it's become possible to hit this since
    // https://hg.mozilla.org/mozilla-central/rev/50e934e4979b landed.
    NS_WARNING("converted background area should not be empty");
    // Make our caller not do anything.
    aClipState.mDirtyRectInDevPx.SizeTo(gfxSize(0.0, 0.0));
    return;
  }

  aCtx->Save();
  gfxRect dirty = ThebesRect(bgAreaGfx).Intersect(aClipState.mDirtyRectInDevPx);

  aCtx->NewPath();
  aCtx->Rectangle(dirty, true);
  aCtx->Clip();

  if (aClipState.mHasAdditionalBGClipArea) {
    gfxRect bgAdditionalAreaGfx = nsLayoutUtils::RectToGfxRect(
      aClipState.mAdditionalBGClipArea, aAppUnitsPerPixel);
    bgAdditionalAreaGfx.Round();
    gfxUtils::ConditionRect(bgAdditionalAreaGfx);
    aCtx->NewPath();
    aCtx->Rectangle(bgAdditionalAreaGfx, true);
    aCtx->Clip();
  }

  RefPtr<Path> roundedRect =
    MakePathForRoundedRect(*drawTarget, bgAreaGfx, aClipState.mClippedRadii);
  aCtx->SetPath(roundedRect);
  aCtx->Fill();
  aCtx->Restore();
}

nscolor
nsCSSRendering::DetermineBackgroundColor(nsPresContext* aPresContext,
                                         nsStyleContext* aStyleContext,
                                         nsIFrame* aFrame,
                                         bool& aDrawBackgroundImage,
                                         bool& aDrawBackgroundColor)
{
  aDrawBackgroundImage = true;
  aDrawBackgroundColor = true;

  const nsStyleVisibility* visibility = aStyleContext->StyleVisibility();

  if (visibility->mColorAdjust != NS_STYLE_COLOR_ADJUST_EXACT &&
      aFrame->HonorPrintBackgroundSettings()) {
    aDrawBackgroundImage = aPresContext->GetBackgroundImageDraw();
    aDrawBackgroundColor = aPresContext->GetBackgroundColorDraw();
  }

  const nsStyleBackground *bg = aStyleContext->StyleBackground();
  nscolor bgColor;
  if (aDrawBackgroundColor) {
    bgColor = aStyleContext->
      GetVisitedDependentColor(&nsStyleBackground::mBackgroundColor);
    if (NS_GET_A(bgColor) == 0) {
      aDrawBackgroundColor = false;
    }
  } else {
    // If GetBackgroundColorDraw() is false, we are still expected to
    // draw color in the background of any frame that's not completely
    // transparent, but we are expected to use white instead of whatever
    // color was specified.
    bgColor = NS_RGB(255, 255, 255);
    if (aDrawBackgroundImage || !bg->IsTransparent(aStyleContext)) {
      aDrawBackgroundColor = true;
    } else {
      bgColor = NS_RGBA(0,0,0,0);
    }
  }

  // We can skip painting the background color if a background image is opaque.
  nsStyleImageLayers::Repeat repeat = bg->BottomLayer().mRepeat;
  bool xFullRepeat = repeat.mXRepeat == StyleImageLayerRepeat::Repeat ||
                     repeat.mXRepeat == StyleImageLayerRepeat::Round;
  bool yFullRepeat = repeat.mYRepeat == StyleImageLayerRepeat::Repeat ||
                     repeat.mYRepeat == StyleImageLayerRepeat::Round;
  if (aDrawBackgroundColor &&
      xFullRepeat && yFullRepeat &&
      bg->BottomLayer().mImage.IsOpaque() &&
      bg->BottomLayer().mBlendMode == NS_STYLE_BLEND_NORMAL) {
    aDrawBackgroundColor = false;
  }

  return bgColor;
}

static CompositionOp
DetermineCompositionOp(const nsCSSRendering::PaintBGParams& aParams,
                       const nsStyleImageLayers& aLayers,
                       uint32_t aLayerIndex)
{
  if (aParams.layer >= 0) {
    // When drawing a single layer, use the specified composition op.
    return aParams.compositionOp;
  }

  const nsStyleImageLayers::Layer& layer = aLayers.mLayers[aLayerIndex];
  // When drawing all layers, get the compositon op from each image layer.
  if (aParams.paintFlags & nsCSSRendering::PAINTBG_MASK_IMAGE) {
    // Always using OP_OVER mode while drawing the bottom mask layer.
    if (aLayerIndex == (aLayers.mImageCount - 1)) {
      return CompositionOp::OP_OVER;
    }

    return nsCSSRendering::GetGFXCompositeMode(layer.mComposite);
  }

  return nsCSSRendering::GetGFXBlendMode(layer.mBlendMode);
}

DrawResult
nsCSSRendering::PaintStyleImageLayerWithSC(const PaintBGParams& aParams,
                                           gfxContext& aRenderingCtx,
                                           nsStyleContext *aBackgroundSC,
                                           const nsStyleBorder& aBorder)
{
  NS_PRECONDITION(aParams.frame,
                  "Frame is expected to be provided to PaintStyleImageLayerWithSC");

  // If we're drawing all layers, aCompositonOp is ignored, so make sure that
  // it was left at its default value.
  MOZ_ASSERT(aParams.layer != -1 ||
             aParams.compositionOp == CompositionOp::OP_OVER);

  // Check to see if we have an appearance defined.  If so, we let the theme
  // renderer draw the background and bail out.
  // XXXzw this ignores aParams.bgClipRect.
  const nsStyleDisplay* displayData = aParams.frame->StyleDisplay();
  if (displayData->mAppearance) {
    nsITheme *theme = aParams.presCtx.GetTheme();
    if (theme && theme->ThemeSupportsWidget(&aParams.presCtx,
                                            aParams.frame,
                                            displayData->mAppearance)) {
      nsRect drawing(aParams.borderArea);
      theme->GetWidgetOverflow(aParams.presCtx.DeviceContext(),
                               aParams.frame, displayData->mAppearance,
                               &drawing);
      drawing.IntersectRect(drawing, aParams.dirtyRect);
      theme->DrawWidgetBackground(&aRenderingCtx, aParams.frame,
                                  displayData->mAppearance, aParams.borderArea,
                                  drawing);
      return DrawResult::SUCCESS;
    }
  }

  // For canvas frames (in the CSS sense) we draw the background color using
  // a solid color item that gets added in nsLayoutUtils::PaintFrame,
  // or nsSubDocumentFrame::BuildDisplayList (bug 488242). (The solid
  // color may be moved into nsDisplayCanvasBackground by
  // nsPresShell::AddCanvasBackgroundColorItem, and painted by
  // nsDisplayCanvasBackground directly.) Either way we don't need to
  // paint the background color here.
  bool isCanvasFrame = IsCanvasFrame(aParams.frame);

  // Determine whether we are drawing background images and/or
  // background colors.
  bool drawBackgroundImage;
  bool drawBackgroundColor;

  nscolor bgColor = DetermineBackgroundColor(&aParams.presCtx,
                                             aBackgroundSC,
                                             aParams.frame,
                                             drawBackgroundImage,
                                             drawBackgroundColor);

  bool paintMask = (aParams.paintFlags & PAINTBG_MASK_IMAGE);
  const nsStyleImageLayers& layers = paintMask ?
    aBackgroundSC->StyleSVGReset()->mMask :
    aBackgroundSC->StyleBackground()->mImage;
  // If we're drawing a specific layer, we don't want to draw the
  // background color.
  if ((drawBackgroundColor && aParams.layer >= 0) || paintMask) {
    drawBackgroundColor = false;
  }

  // At this point, drawBackgroundImage and drawBackgroundColor are
  // true if and only if we are actually supposed to paint an image or
  // color into aDirtyRect, respectively.
  if (!drawBackgroundImage && !drawBackgroundColor)
    return DrawResult::SUCCESS;

  // The 'bgClipArea' (used only by the image tiling logic, far below)
  // is the caller-provided aParams.bgClipRect if any, or else the area
  // determined by the value of 'background-clip' in
  // SetupCurrentBackgroundClip.  (Arguably it should be the
  // intersection, but that breaks the table painter -- in particular,
  // taking the intersection breaks reftests/bugs/403249-1[ab].)
  nscoord appUnitsPerPixel = aParams.presCtx.AppUnitsPerDevPixel();
  ImageLayerClipState clipState;
  if (aParams.bgClipRect) {
    clipState.mBGClipArea = *aParams.bgClipRect;
    clipState.mCustomClip = true;
    clipState.mHasRoundedCorners = false;
    SetupDirtyRects(clipState.mBGClipArea, aParams.dirtyRect, appUnitsPerPixel,
                    &clipState.mDirtyRectInAppUnits,
                    &clipState.mDirtyRectInDevPx);
  } else {
    GetImageLayerClip(layers.BottomLayer(),
                      aParams.frame, aBorder, aParams.borderArea,
                      aParams.dirtyRect,
                      (aParams.paintFlags & PAINTBG_WILL_PAINT_BORDER),
                      appUnitsPerPixel,
                      &clipState);
  }

  // If we might be using a background color, go ahead and set it now.
  if (drawBackgroundColor && !isCanvasFrame) {
    aRenderingCtx.SetColor(Color::FromABGR(bgColor));
  }

  // If there is no background image, draw a color.  (If there is
  // neither a background image nor a color, we wouldn't have gotten
  // this far.)
  if (!drawBackgroundImage) {
    if (!isCanvasFrame) {
      DrawBackgroundColor(clipState, &aRenderingCtx, appUnitsPerPixel);
    }
    return DrawResult::SUCCESS;
  }

  if (layers.mImageCount < 1) {
    // Return if there are no background layers, all work from this point
    // onwards happens iteratively on these.
    return DrawResult::SUCCESS;
  }

  MOZ_ASSERT((aParams.layer < 0) ||
             (layers.mImageCount > uint32_t(aParams.layer)));
  bool drawAllLayers = (aParams.layer < 0);

  // Ensure we get invalidated for loads of the image.  We need to do
  // this here because this might be the only code that knows about the
  // association of the style data with the frame.
  if (aBackgroundSC != aParams.frame->StyleContext()) {
    uint32_t startLayer = drawAllLayers ? layers.mImageCount - 1
                                        : aParams.layer;
    uint32_t count = drawAllLayers ? layers.mImageCount : 1;
    NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT_WITH_RANGE(i, layers, startLayer,
                                                         count) {
      aParams.frame->AssociateImage(layers.mLayers[i].mImage,
                                    &aParams.presCtx);
    }
  }

  // The background color is rendered over the entire dirty area,
  // even if the image isn't.
  if (drawBackgroundColor && !isCanvasFrame) {
    DrawBackgroundColor(clipState, &aRenderingCtx, appUnitsPerPixel);
  }

  // Compute the outermost boundary of the area that might be painted.
  // Same coordinate space as aParams.borderArea & aParams.bgClipRect.
  Sides skipSides = aParams.frame->GetSkipSides();
  nsRect paintBorderArea =
    ::BoxDecorationRectForBackground(aParams.frame, aParams.borderArea,
                                     skipSides, &aBorder);
  nsRect clipBorderArea =
    ::BoxDecorationRectForBorder(aParams.frame, aParams.borderArea,
                                 skipSides, &aBorder);

  DrawResult result = DrawResult::SUCCESS;
  StyleGeometryBox currentBackgroundClip = StyleGeometryBox::BorderBox;
  uint32_t count = drawAllLayers
    ? layers.mImageCount                  // iterate all image layers.
    : layers.mImageCount - aParams.layer; // iterate from the bottom layer to
                                          // the 'aParams.layer-th' layer.
  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT_WITH_RANGE(i, layers,
                                                       layers.mImageCount - 1,
                                                       count) {
    // NOTE: no Save() yet, we do that later by calling autoSR.EnsureSaved(ctx)
    // in the cases we need it.
    gfxContextAutoSaveRestore autoSR;
    const nsStyleImageLayers::Layer& layer = layers.mLayers[i];

    if (!aParams.bgClipRect) {
      bool isBottomLayer = (i == layers.mImageCount - 1);
      if (currentBackgroundClip != layer.mClip || isBottomLayer) {
        currentBackgroundClip = layer.mClip;
        // For  the bottom layer, we already called GetImageLayerClip above
        // and it stored its results in clipState.
        if (!isBottomLayer) {
          GetImageLayerClip(layer, aParams.frame,
                            aBorder, aParams.borderArea, aParams.dirtyRect,
                            (aParams.paintFlags & PAINTBG_WILL_PAINT_BORDER),
                            appUnitsPerPixel, &clipState);
        }
        SetupImageLayerClip(clipState, &aRenderingCtx,
                            appUnitsPerPixel, &autoSR);
        if (!clipBorderArea.IsEqualEdges(aParams.borderArea)) {
          // We're drawing the background for the joined continuation boxes
          // so we need to clip that to the slice that we want for this
          // frame.
          gfxRect clip =
            nsLayoutUtils::RectToGfxRect(aParams.borderArea, appUnitsPerPixel);
          autoSR.EnsureSaved(&aRenderingCtx);
          aRenderingCtx.NewPath();
          aRenderingCtx.SnappedRectangle(clip);
          aRenderingCtx.Clip();
        }
      }
    }

    // Skip the following layer preparing and painting code if the current
    // layer is not selected for drawing.
    if (aParams.layer >= 0 && i != (uint32_t)aParams.layer) {
      continue;
    }
    nsBackgroundLayerState state =
      PrepareImageLayer(&aParams.presCtx, aParams.frame,
                        aParams.paintFlags, paintBorderArea,
                        clipState.mBGClipArea, layer, nullptr);
    result &= state.mImageRenderer.PrepareResult();

    // Skip the layer painting code if we found the dirty region is empty.
    if (clipState.mDirtyRectInDevPx.IsEmpty()) {
      continue;
    }

    if (!state.mFillArea.IsEmpty()) {
      CompositionOp co = DetermineCompositionOp(aParams, layers, i);
      if (co != CompositionOp::OP_OVER) {
        NS_ASSERTION(aRenderingCtx.CurrentOp() == CompositionOp::OP_OVER,
                     "It is assumed the initial op is OP_OVER, when it is "
                     "restored later");
        aRenderingCtx.SetOp(co);
      }

      result &=
        state.mImageRenderer.DrawLayer(&aParams.presCtx,
                                       aRenderingCtx,
                                       state.mDestArea, state.mFillArea,
                                       state.mAnchor + paintBorderArea.TopLeft(),
                                       clipState.mDirtyRectInAppUnits,
                                       state.mRepeatSize, aParams.opacity);

      if (co != CompositionOp::OP_OVER) {
        aRenderingCtx.SetOp(CompositionOp::OP_OVER);
      }
    }
  }

  return result;
}

DrawResult
nsCSSRendering::BuildWebRenderDisplayItemsForStyleImageLayerWithSC(const PaintBGParams& aParams,
                                                                   mozilla::wr::DisplayListBuilder& aBuilder,
                                                                   const mozilla::layers::StackingContextHelper& aSc,
                                                                   nsTArray<WebRenderParentCommand>& aParentCommands,
                                                                   mozilla::layers::WebRenderDisplayItemLayer* aLayer,
                                                                   mozilla::layers::WebRenderLayerManager* aManager,
                                                                   nsDisplayItem* aItem,
                                                                   nsStyleContext *aBackgroundSC,
                                                                   const nsStyleBorder& aBorder)
{
  MOZ_ASSERT(!(aParams.paintFlags & PAINTBG_MASK_IMAGE));

  nscoord appUnitsPerPixel = aParams.presCtx.AppUnitsPerDevPixel();
  ImageLayerClipState clipState;

  clipState.mBGClipArea = *aParams.bgClipRect;
  clipState.mCustomClip = true;
  clipState.mHasRoundedCorners = false;
  SetupDirtyRects(clipState.mBGClipArea, aParams.dirtyRect, appUnitsPerPixel,
                  &clipState.mDirtyRectInAppUnits,
                  &clipState.mDirtyRectInDevPx);

  // Compute the outermost boundary of the area that might be painted.
  // Same coordinate space as aParams.borderArea & aParams.bgClipRect.
  Sides skipSides = aParams.frame->GetSkipSides();
  nsRect paintBorderArea =
    ::BoxDecorationRectForBackground(aParams.frame, aParams.borderArea,
                                     skipSides, &aBorder);

  const nsStyleImageLayers& layers = aBackgroundSC->StyleBackground()->mImage;
  const nsStyleImageLayers::Layer& layer = layers.mLayers[aParams.layer];

  // Skip the following layer painting code if we found the dirty region is
  // empty or the current layer is not selected for drawing.
  if (clipState.mDirtyRectInDevPx.IsEmpty()) {
    return DrawResult::SUCCESS;
  }

  DrawResult result = DrawResult::SUCCESS;
  nsBackgroundLayerState state =
    PrepareImageLayer(&aParams.presCtx, aParams.frame,
                      aParams.paintFlags, paintBorderArea,
                      clipState.mBGClipArea, layer, nullptr);
  result &= state.mImageRenderer.PrepareResult();
  if (!state.mFillArea.IsEmpty()) {
    return state.mImageRenderer.BuildWebRenderDisplayItemsForLayer(&aParams.presCtx,
                                     aBuilder, aSc, aParentCommands,
                                     aLayer, aManager, aItem,
                                     state.mDestArea, state.mFillArea,
                                     state.mAnchor + paintBorderArea.TopLeft(),
                                     clipState.mDirtyRectInAppUnits,
                                     state.mRepeatSize, aParams.opacity);
  }

  return result;
}

nsRect
nsCSSRendering::ComputeImageLayerPositioningArea(nsPresContext* aPresContext,
                                                 nsIFrame* aForFrame,
                                                 const nsRect& aBorderArea,
                                                 const nsStyleImageLayers::Layer& aLayer,
                                                 nsIFrame** aAttachedToFrame,
                                                 bool* aOutIsTransformedFixed)
{
  // Compute {background|mask} origin area relative to aBorderArea now as we
  // may need  it to compute the effective image size for a CSS gradient.
  nsRect positionArea;

  StyleGeometryBox layerOrigin =
    ComputeBoxValue(aForFrame, aLayer.mOrigin);

  if (IsSVGStyleGeometryBox(layerOrigin)) {
    MOZ_ASSERT(aForFrame->IsFrameOfType(nsIFrame::eSVG) &&
               !aForFrame->IsSVGOuterSVGFrame());
    *aAttachedToFrame = aForFrame;

    positionArea =
      nsLayoutUtils::ComputeGeometryBox(aForFrame, layerOrigin);

    nsPoint toStrokeBoxOffset = nsPoint(0, 0);
    if (layerOrigin != StyleGeometryBox::StrokeBox) {
      nsRect strokeBox =
        nsLayoutUtils::ComputeGeometryBox(aForFrame,
                                          StyleGeometryBox::StrokeBox);
      toStrokeBoxOffset = positionArea.TopLeft() - strokeBox.TopLeft();
    }

    // For SVG frames, the return value is relative to the stroke box
    return nsRect(toStrokeBoxOffset, positionArea.Size());
  }

  MOZ_ASSERT(!aForFrame->IsFrameOfType(nsIFrame::eSVG) ||
             aForFrame->IsSVGOuterSVGFrame());

  LayoutFrameType frameType = aForFrame->Type();
  nsIFrame* geometryFrame = aForFrame;
  if (MOZ_UNLIKELY(frameType == LayoutFrameType::Scroll &&
                   NS_STYLE_IMAGELAYER_ATTACHMENT_LOCAL == aLayer.mAttachment)) {
    nsIScrollableFrame* scrollableFrame = do_QueryFrame(aForFrame);
    positionArea = nsRect(
      scrollableFrame->GetScrolledFrame()->GetPosition()
        // For the dir=rtl case:
        + scrollableFrame->GetScrollRange().TopLeft(),
      scrollableFrame->GetScrolledRect().Size());
    // The ScrolledRect’s size does not include the borders or scrollbars,
    // reverse the handling of background-origin
    // compared to the common case below.
    if (layerOrigin == StyleGeometryBox::BorderBox) {
      nsMargin border = geometryFrame->GetUsedBorder();
      border.ApplySkipSides(geometryFrame->GetSkipSides());
      positionArea.Inflate(border);
      positionArea.Inflate(scrollableFrame->GetActualScrollbarSizes());
    } else if (layerOrigin != StyleGeometryBox::PaddingBox) {
      nsMargin padding = geometryFrame->GetUsedPadding();
      padding.ApplySkipSides(geometryFrame->GetSkipSides());
      positionArea.Deflate(padding);
      NS_ASSERTION(layerOrigin == StyleGeometryBox::ContentBox,
                   "unknown background-origin value");
    }
    *aAttachedToFrame = aForFrame;
    return positionArea;
  }

  if (MOZ_UNLIKELY(frameType == LayoutFrameType::Canvas)) {
    geometryFrame = aForFrame->PrincipalChildList().FirstChild();
    // geometryFrame might be null if this canvas is a page created
    // as an overflow container (e.g. the in-flow content has already
    // finished and this page only displays the continuations of
    // absolutely positioned content).
    if (geometryFrame) {
      positionArea = geometryFrame->GetRect();
    }
  } else {
    positionArea = nsRect(nsPoint(0,0), aBorderArea.Size());
  }

  // See the comment of StyleGeometryBox::MarginBox.
  // Hitting this assertion means we decide to turn on margin-box support for
  // positioned mask from CSS parser and style system. In this case, you
  // should *inflate* positionArea by the margin returning from
  // geometryFrame->GetUsedMargin() in the code chunk bellow.
  MOZ_ASSERT(aLayer.mOrigin != StyleGeometryBox::MarginBox,
             "StyleGeometryBox::MarginBox rendering is not supported yet.\n");

  // {background|mask} images are tiled over the '{background|mask}-clip' area
  // but the origin of the tiling is based on the '{background|mask}-origin'
  // area.
  if (layerOrigin != StyleGeometryBox::BorderBox && geometryFrame) {
    nsMargin border = geometryFrame->GetUsedBorder();
    if (layerOrigin != StyleGeometryBox::PaddingBox) {
      border += geometryFrame->GetUsedPadding();
      NS_ASSERTION(layerOrigin == StyleGeometryBox::ContentBox,
                   "unknown background-origin value");
    }
    positionArea.Deflate(border);
  }

  nsIFrame* attachedToFrame = aForFrame;
  if (NS_STYLE_IMAGELAYER_ATTACHMENT_FIXED == aLayer.mAttachment) {
    // If it's a fixed background attachment, then the image is placed
    // relative to the viewport, which is the area of the root frame
    // in a screen context or the page content frame in a print context.
    attachedToFrame = aPresContext->PresShell()->FrameManager()->GetRootFrame();
    NS_ASSERTION(attachedToFrame, "no root frame");
    nsIFrame* pageContentFrame = nullptr;
    if (aPresContext->IsPaginated()) {
      pageContentFrame = nsLayoutUtils::GetClosestFrameOfType(
        aForFrame, LayoutFrameType::PageContent);
      if (pageContentFrame) {
        attachedToFrame = pageContentFrame;
      }
      // else this is an embedded shell and its root frame is what we want
    }

    // If the background is affected by a transform, treat is as if it
    // wasn't fixed.
    if (nsLayoutUtils::IsTransformed(aForFrame, attachedToFrame)) {
      attachedToFrame = aForFrame;
      *aOutIsTransformedFixed = true;
    } else {
      // Set the background positioning area to the viewport's area
      // (relative to aForFrame)
      positionArea =
        nsRect(-aForFrame->GetOffsetTo(attachedToFrame), attachedToFrame->GetSize());

      if (!pageContentFrame) {
        // Subtract the size of scrollbars.
        nsIScrollableFrame* scrollableFrame =
          aPresContext->PresShell()->GetRootScrollFrameAsScrollable();
        if (scrollableFrame) {
          nsMargin scrollbars = scrollableFrame->GetActualScrollbarSizes();
          positionArea.Deflate(scrollbars);
        }
      }
    }
  }
  *aAttachedToFrame = attachedToFrame;

  return positionArea;
}

/* static */ nscoord
nsCSSRendering::ComputeRoundedSize(nscoord aCurrentSize, nscoord aPositioningSize)
{
  float repeatCount = NS_roundf(float(aPositioningSize) / float(aCurrentSize));
  if (repeatCount < 1.0f) {
    return aPositioningSize;
  }
  return nscoord(NS_lround(float(aPositioningSize) / repeatCount));
}

// Apply the CSS image sizing algorithm as it applies to background images.
// See http://www.w3.org/TR/css3-background/#the-background-size .
// aIntrinsicSize is the size that the background image 'would like to be'.
// It can be found by calling nsImageRenderer::ComputeIntrinsicSize.
static nsSize
ComputeDrawnSizeForBackground(const CSSSizeOrRatio& aIntrinsicSize,
                              const nsSize& aBgPositioningArea,
                              const nsStyleImageLayers::Size& aLayerSize,
                              StyleImageLayerRepeat aXRepeat,
                              StyleImageLayerRepeat aYRepeat)
{
  nsSize imageSize;

  // Size is dictated by cover or contain rules.
  if (aLayerSize.mWidthType == nsStyleImageLayers::Size::eContain ||
      aLayerSize.mWidthType == nsStyleImageLayers::Size::eCover) {
    nsImageRenderer::FitType fitType =
      aLayerSize.mWidthType == nsStyleImageLayers::Size::eCover
        ? nsImageRenderer::COVER
        : nsImageRenderer::CONTAIN;
    imageSize = nsImageRenderer::ComputeConstrainedSize(aBgPositioningArea,
                                                        aIntrinsicSize.mRatio,
                                                        fitType);
  } else {
    // No cover/contain constraint, use default algorithm.
    CSSSizeOrRatio specifiedSize;
    if (aLayerSize.mWidthType == nsStyleImageLayers::Size::eLengthPercentage) {
      specifiedSize.SetWidth(
        aLayerSize.ResolveWidthLengthPercentage(aBgPositioningArea));
    }
    if (aLayerSize.mHeightType == nsStyleImageLayers::Size::eLengthPercentage) {
      specifiedSize.SetHeight(
        aLayerSize.ResolveHeightLengthPercentage(aBgPositioningArea));
    }

    imageSize = nsImageRenderer::ComputeConcreteSize(specifiedSize,
                                                     aIntrinsicSize,
                                                     aBgPositioningArea);
  }

  // See https://www.w3.org/TR/css3-background/#background-size .
  // "If 'background-repeat' is 'round' for one (or both) dimensions, there is a second
  //  step. The UA must scale the image in that dimension (or both dimensions) so that
  //  it fits a whole number of times in the background positioning area."
  // "If 'background-repeat' is 'round' for one dimension only and if 'background-size'
  //  is 'auto' for the other dimension, then there is a third step: that other dimension
  //  is scaled so that the original aspect ratio is restored."
  bool isRepeatRoundInBothDimensions = aXRepeat == StyleImageLayerRepeat::Round  &&
                                       aYRepeat == StyleImageLayerRepeat::Round;

  // Calculate the rounded size only if the background-size computation
  // returned a correct size for the image.
  if (imageSize.width && aXRepeat == StyleImageLayerRepeat::Round) {
    imageSize.width =
      nsCSSRendering::ComputeRoundedSize(imageSize.width,
                                         aBgPositioningArea.width);
    if (!isRepeatRoundInBothDimensions &&
        aLayerSize.mHeightType == nsStyleImageLayers::Size::DimensionType::eAuto) {
      // Restore intrinsic rato
      if (aIntrinsicSize.mRatio.width) {
        float scale = float(aIntrinsicSize.mRatio.height) / aIntrinsicSize.mRatio.width;
        imageSize.height = NSCoordSaturatingNonnegativeMultiply(imageSize.width, scale);
      }
    }
  }

  // Calculate the rounded size only if the background-size computation
  // returned a correct size for the image.
  if (imageSize.height && aYRepeat == StyleImageLayerRepeat::Round) {
    imageSize.height =
      nsCSSRendering::ComputeRoundedSize(imageSize.height,
                                         aBgPositioningArea.height);
    if (!isRepeatRoundInBothDimensions &&
        aLayerSize.mWidthType == nsStyleImageLayers::Size::DimensionType::eAuto) {
      // Restore intrinsic rato
      if (aIntrinsicSize.mRatio.height) {
        float scale = float(aIntrinsicSize.mRatio.width) / aIntrinsicSize.mRatio.height;
        imageSize.width = NSCoordSaturatingNonnegativeMultiply(imageSize.height, scale);
      }
    }
  }

  return imageSize;
}

/* ComputeSpacedRepeatSize
 * aImageDimension: the image width/height
 * aAvailableSpace: the background positioning area width/height
 * aRepeat: determine whether the image is repeated
 * Returns the image size plus gap size of app units for use as spacing
 */
static nscoord
ComputeSpacedRepeatSize(nscoord aImageDimension,
                        nscoord aAvailableSpace,
                        bool& aRepeat) {
  float ratio = static_cast<float>(aAvailableSpace) / aImageDimension;

  if (ratio < 2.0f) { // If you can't repeat at least twice, then don't repeat.
    aRepeat = false;
    return aImageDimension;
  } else {
    aRepeat = true;
    return (aAvailableSpace - aImageDimension) / (NSToIntFloor(ratio) - 1);
  }
}

/* static */ nscoord
nsCSSRendering::ComputeBorderSpacedRepeatSize(nscoord aImageDimension,
                                              nscoord aAvailableSpace,
                                              nscoord& aSpace)
{
  int32_t count = aAvailableSpace / aImageDimension;
  aSpace = (aAvailableSpace - aImageDimension * count) / (count + 1);
  return aSpace + aImageDimension;
}

nsBackgroundLayerState
nsCSSRendering::PrepareImageLayer(nsPresContext* aPresContext,
                                  nsIFrame* aForFrame,
                                  uint32_t aFlags,
                                  const nsRect& aBorderArea,
                                  const nsRect& aBGClipRect,
                                  const nsStyleImageLayers::Layer& aLayer,
                                  bool* aOutIsTransformedFixed)
{
  /*
   * The properties we need to keep in mind when drawing style image
   * layers are:
   *
   *   background-image/ mask-image
   *   background-repeat/ mask-repeat
   *   background-attachment
   *   background-position/ mask-position
   *   background-clip/ mask-clip
   *   background-origin/ mask-origin
   *   background-size/ mask-size
   *   background-blend-mode
   *   box-decoration-break
   *   mask-mode
   *   mask-composite
   *
   * (background-color applies to the entire element and not to individual
   * layers, so it is irrelevant to this method.)
   *
   * These properties have the following dependencies upon each other when
   * determining rendering:
   *
   *   background-image/ mask-image
   *     no dependencies
   *   background-repeat/ mask-repeat
   *     no dependencies
   *   background-attachment
   *     no dependencies
   *   background-position/ mask-position
   *     depends upon background-size/mask-size (for the image's scaled size)
   *     and background-break (for the background positioning area)
   *   background-clip/ mask-clip
   *     no dependencies
   *   background-origin/ mask-origin
   *     depends upon background-attachment (only in the case where that value
   *     is 'fixed')
   *   background-size/ mask-size
   *     depends upon box-decoration-break (for the background positioning area
   *     for resolving percentages), background-image (for the image's intrinsic
   *     size), background-repeat (if that value is 'round'), and
   *     background-origin (for the background painting area, when
   *     background-repeat is 'round')
   *   background-blend-mode
   *     no dependencies
   *   mask-mode
   *     no dependencies
   *   mask-composite
   *     no dependencies
   *   box-decoration-break
   *     no dependencies
   *
   * As a result of only-if dependencies we don't strictly do a topological
   * sort of the above properties when processing, but it's pretty close to one:
   *
   *   background-clip/mask-clip (by caller)
   *   background-image/ mask-image
   *   box-decoration-break, background-origin/ mask origin
   *   background-attachment (postfix for background-origin if 'fixed')
   *   background-size/ mask-size
   *   background-position/ mask-position
   *   background-repeat/ mask-repeat
   */

  uint32_t irFlags = 0;
  if (aFlags & nsCSSRendering::PAINTBG_SYNC_DECODE_IMAGES) {
    irFlags |= nsImageRenderer::FLAG_SYNC_DECODE_IMAGES;
  }
  if (aFlags & nsCSSRendering::PAINTBG_TO_WINDOW) {
    irFlags |= nsImageRenderer::FLAG_PAINTING_TO_WINDOW;
  }

  nsBackgroundLayerState state(aForFrame, &aLayer.mImage, irFlags);
  if (!state.mImageRenderer.PrepareImage()) {
    // There's no image or it's not ready to be painted.
    if (aOutIsTransformedFixed) {
      *aOutIsTransformedFixed = false;
    }
    return state;
  }

  // The frame to which the background is attached
  nsIFrame* attachedToFrame = aForFrame;
  // Is the background marked 'fixed', but affected by a transform?
  bool transformedFixed = false;
  // Compute background origin area relative to aBorderArea now as we may need
  // it to compute the effective image size for a CSS gradient.
  nsRect positionArea =
    ComputeImageLayerPositioningArea(aPresContext, aForFrame, aBorderArea,
                                     aLayer, &attachedToFrame, &transformedFixed);
  if (aOutIsTransformedFixed) {
    *aOutIsTransformedFixed = transformedFixed;
  }

  // For background-attachment:fixed backgrounds, we'll override the area
  // where the background can be drawn to the viewport.
  nsRect bgClipRect = aBGClipRect;

  if (NS_STYLE_IMAGELAYER_ATTACHMENT_FIXED == aLayer.mAttachment &&
      !transformedFixed &&
      (aFlags & nsCSSRendering::PAINTBG_TO_WINDOW)) {
    bgClipRect = positionArea + aBorderArea.TopLeft();
  }

  StyleImageLayerRepeat repeatX = aLayer.mRepeat.mXRepeat;
  StyleImageLayerRepeat repeatY = aLayer.mRepeat.mYRepeat;

  // Scale the image as specified for background-size and background-repeat.
  // Also as required for proper background positioning when background-position
  // is defined with percentages.
  CSSSizeOrRatio intrinsicSize = state.mImageRenderer.ComputeIntrinsicSize();
  nsSize bgPositionSize = positionArea.Size();
  nsSize imageSize = ComputeDrawnSizeForBackground(intrinsicSize,
                                                   bgPositionSize,
                                                   aLayer.mSize,
                                                   repeatX,
                                                   repeatY);

  if (imageSize.width <= 0 || imageSize.height <= 0)
    return state;

  state.mImageRenderer.SetPreferredSize(intrinsicSize,
                                        imageSize);

  // Compute the anchor point.
  //
  // relative to aBorderArea.TopLeft() (which is where the top-left
  // of aForFrame's border-box will be rendered)
  nsPoint imageTopLeft;

  // Compute the position of the background now that the background's size is
  // determined.
  nsImageRenderer::ComputeObjectAnchorPoint(aLayer.mPosition,
                                            bgPositionSize, imageSize,
                                            &imageTopLeft, &state.mAnchor);
  state.mRepeatSize = imageSize;
  if (repeatX == StyleImageLayerRepeat::Space) {
    bool isRepeat;
    state.mRepeatSize.width = ComputeSpacedRepeatSize(imageSize.width,
                                                      bgPositionSize.width,
                                                      isRepeat);
    if (isRepeat) {
      imageTopLeft.x = 0;
      state.mAnchor.x = 0;
    } else {
      repeatX = StyleImageLayerRepeat::NoRepeat;
    }
  }

  if (repeatY == StyleImageLayerRepeat::Space) {
    bool isRepeat;
    state.mRepeatSize.height = ComputeSpacedRepeatSize(imageSize.height,
                                                       bgPositionSize.height,
                                                       isRepeat);
    if (isRepeat) {
      imageTopLeft.y = 0;
      state.mAnchor.y = 0;
    } else {
      repeatY = StyleImageLayerRepeat::NoRepeat;
    }
  }

  imageTopLeft += positionArea.TopLeft();
  state.mAnchor += positionArea.TopLeft();
  state.mDestArea = nsRect(imageTopLeft + aBorderArea.TopLeft(), imageSize);
  state.mFillArea = state.mDestArea;

  ExtendMode repeatMode = ExtendMode::CLAMP;
  if (repeatX == StyleImageLayerRepeat::Repeat ||
      repeatX == StyleImageLayerRepeat::Round ||
      repeatX == StyleImageLayerRepeat::Space) {
    state.mFillArea.x = bgClipRect.x;
    state.mFillArea.width = bgClipRect.width;
    repeatMode = ExtendMode::REPEAT_X;
  }
  if (repeatY == StyleImageLayerRepeat::Repeat ||
      repeatY == StyleImageLayerRepeat::Round ||
      repeatY == StyleImageLayerRepeat::Space) {
    state.mFillArea.y = bgClipRect.y;
    state.mFillArea.height = bgClipRect.height;

    /***
     * We're repeating on the X axis already,
     * so if we have to repeat in the Y axis,
     * we really need to repeat in both directions.
     */
    if (repeatMode == ExtendMode::REPEAT_X) {
      repeatMode = ExtendMode::REPEAT;
    } else {
      repeatMode = ExtendMode::REPEAT_Y;
    }
  }
  state.mImageRenderer.SetExtendMode(repeatMode);
  state.mImageRenderer.SetMaskOp(aLayer.mMaskMode);

  state.mFillArea.IntersectRect(state.mFillArea, bgClipRect);

  return state;
}

nsRect
nsCSSRendering::GetBackgroundLayerRect(nsPresContext* aPresContext,
                                       nsIFrame* aForFrame,
                                       const nsRect& aBorderArea,
                                       const nsRect& aClipRect,
                                       const nsStyleImageLayers::Layer& aLayer,
                                       uint32_t aFlags)
{
  Sides skipSides = aForFrame->GetSkipSides();
  nsRect borderArea =
    ::BoxDecorationRectForBackground(aForFrame, aBorderArea, skipSides);
  nsBackgroundLayerState state =
      PrepareImageLayer(aPresContext, aForFrame, aFlags, borderArea,
                             aClipRect, aLayer);
  return state.mFillArea;
}

// Begin table border-collapsing section
// These functions were written to not disrupt the normal ones and yet satisfy some additional requirements
// At some point, all functions should be unified to include the additional functionality that these provide

static nscoord
RoundIntToPixel(nscoord aValue,
                nscoord aTwipsPerPixel,
                bool    aRoundDown = false)
{
  if (aTwipsPerPixel <= 0)
    // We must be rendering to a device that has a resolution greater than Twips!
    // In that case, aValue is as accurate as it's going to get.
    return aValue;

  nscoord halfPixel = NSToCoordRound(aTwipsPerPixel / 2.0f);
  nscoord extra = aValue % aTwipsPerPixel;
  nscoord finalValue = (!aRoundDown && (extra >= halfPixel)) ? aValue + (aTwipsPerPixel - extra) : aValue - extra;
  return finalValue;
}

static nscoord
RoundFloatToPixel(float   aValue,
                  nscoord aTwipsPerPixel,
                  bool    aRoundDown = false)
{
  return RoundIntToPixel(NSToCoordRound(aValue), aTwipsPerPixel, aRoundDown);
}

static void SetPoly(const Rect& aRect, Point* poly)
{
  poly[0].x = aRect.x;
  poly[0].y = aRect.y;
  poly[1].x = aRect.x + aRect.width;
  poly[1].y = aRect.y;
  poly[2].x = aRect.x + aRect.width;
  poly[2].y = aRect.y + aRect.height;
  poly[3].x = aRect.x;
  poly[3].y = aRect.y + aRect.height;
}

static void
DrawDashedSegment(DrawTarget&          aDrawTarget,
                  nsRect               aRect,
                  nscoord              aDashLength,
                  nscolor              aColor,
                  int32_t              aAppUnitsPerDevPixel,
                  nscoord              aTwipsPerPixel,
                  bool                 aHorizontal)
{
  ColorPattern color(ToDeviceColor(aColor));
  DrawOptions drawOptions(1.f, CompositionOp::OP_OVER, AntialiasMode::NONE);
  StrokeOptions strokeOptions;

  Float dash[2];
  dash[0] = Float(aDashLength) / aAppUnitsPerDevPixel;
  dash[1] = dash[0];

  strokeOptions.mDashPattern = dash;
  strokeOptions.mDashLength = MOZ_ARRAY_LENGTH(dash);

  if (aHorizontal) {
    nsPoint left = (aRect.TopLeft() + aRect.BottomLeft()) / 2;
    nsPoint right = (aRect.TopRight() + aRect.BottomRight()) / 2;
    strokeOptions.mLineWidth = Float(aRect.height) / aAppUnitsPerDevPixel;
    StrokeLineWithSnapping(left, right,
                           aAppUnitsPerDevPixel, aDrawTarget,
                           color, strokeOptions, drawOptions);
  } else {
    nsPoint top = (aRect.TopLeft() + aRect.TopRight()) / 2;
    nsPoint bottom = (aRect.BottomLeft() + aRect.BottomRight()) / 2;
    strokeOptions.mLineWidth = Float(aRect.width) / aAppUnitsPerDevPixel;
    StrokeLineWithSnapping(top, bottom,
                           aAppUnitsPerDevPixel, aDrawTarget,
                           color, strokeOptions, drawOptions);
  }
}

static void
DrawSolidBorderSegment(DrawTarget&          aDrawTarget,
                       nsRect               aRect,
                       nscolor              aColor,
                       int32_t              aAppUnitsPerDevPixel,
                       nscoord              aTwipsPerPixel,
                       uint8_t              aStartBevelSide = 0,
                       nscoord              aStartBevelOffset = 0,
                       uint8_t              aEndBevelSide = 0,
                       nscoord              aEndBevelOffset = 0)
{
  ColorPattern color(ToDeviceColor(aColor));
  DrawOptions drawOptions(1.f, CompositionOp::OP_OVER, AntialiasMode::NONE);

  // We don't need to bevel single pixel borders
  if ((aRect.width == aTwipsPerPixel) || (aRect.height == aTwipsPerPixel) ||
      ((0 == aStartBevelOffset) && (0 == aEndBevelOffset))) {
    // simple rectangle
    aDrawTarget.FillRect(NSRectToSnappedRect(aRect, aAppUnitsPerDevPixel,
                                             aDrawTarget),
                         color, drawOptions);
  }
  else {
    // polygon with beveling
    Point poly[4];
    SetPoly(NSRectToSnappedRect(aRect, aAppUnitsPerDevPixel, aDrawTarget),
            poly);

    Float startBevelOffset =
      NSAppUnitsToFloatPixels(aStartBevelOffset, aAppUnitsPerDevPixel);
    switch(aStartBevelSide) {
    case eSideTop:
      poly[0].x += startBevelOffset;
      break;
    case eSideBottom:
      poly[3].x += startBevelOffset;
      break;
    case eSideRight:
      poly[1].y += startBevelOffset;
      break;
    case eSideLeft:
      poly[0].y += startBevelOffset;
    }

    Float endBevelOffset =
      NSAppUnitsToFloatPixels(aEndBevelOffset, aAppUnitsPerDevPixel);
    switch(aEndBevelSide) {
    case eSideTop:
      poly[1].x -= endBevelOffset;
      break;
    case eSideBottom:
      poly[2].x -= endBevelOffset;
      break;
    case eSideRight:
      poly[2].y -= endBevelOffset;
      break;
    case eSideLeft:
      poly[3].y -= endBevelOffset;
    }

    RefPtr<PathBuilder> builder = aDrawTarget.CreatePathBuilder();
    builder->MoveTo(poly[0]);
    builder->LineTo(poly[1]);
    builder->LineTo(poly[2]);
    builder->LineTo(poly[3]);
    builder->Close();
    RefPtr<Path> path = builder->Finish();
    aDrawTarget.Fill(path, color, drawOptions);
  }
}

static void
GetDashInfo(nscoord  aBorderLength,
            nscoord  aDashLength,
            nscoord  aTwipsPerPixel,
            int32_t& aNumDashSpaces,
            nscoord& aStartDashLength,
            nscoord& aEndDashLength)
{
  aNumDashSpaces = 0;
  if (aStartDashLength + aDashLength + aEndDashLength >= aBorderLength) {
    aStartDashLength = aBorderLength;
    aEndDashLength = 0;
  }
  else {
    aNumDashSpaces = (aBorderLength - aDashLength)/ (2 * aDashLength); // round down
    nscoord extra = aBorderLength - aStartDashLength - aEndDashLength - (((2 * aNumDashSpaces) - 1) * aDashLength);
    if (extra > 0) {
      nscoord half = RoundIntToPixel(extra / 2, aTwipsPerPixel);
      aStartDashLength += half;
      aEndDashLength += (extra - half);
    }
  }
}

void
nsCSSRendering::DrawTableBorderSegment(DrawTarget&   aDrawTarget,
                                       uint8_t       aBorderStyle,
                                       nscolor       aBorderColor,
                                       nscolor       aBGColor,
                                       const nsRect& aBorder,
                                       int32_t       aAppUnitsPerDevPixel,
                                       int32_t       aAppUnitsPerCSSPixel,
                                       uint8_t       aStartBevelSide,
                                       nscoord       aStartBevelOffset,
                                       uint8_t       aEndBevelSide,
                                       nscoord       aEndBevelOffset)
{
  bool horizontal = ((eSideTop == aStartBevelSide) || (eSideBottom == aStartBevelSide));
  nscoord twipsPerPixel = NSIntPixelsToAppUnits(1, aAppUnitsPerCSSPixel);
  uint8_t ridgeGroove = NS_STYLE_BORDER_STYLE_RIDGE;

  if ((twipsPerPixel >= aBorder.width) || (twipsPerPixel >= aBorder.height) ||
      (NS_STYLE_BORDER_STYLE_DASHED == aBorderStyle) || (NS_STYLE_BORDER_STYLE_DOTTED == aBorderStyle)) {
    // no beveling for 1 pixel border, dash or dot
    aStartBevelOffset = 0;
    aEndBevelOffset = 0;
  }

  switch (aBorderStyle) {
  case NS_STYLE_BORDER_STYLE_NONE:
  case NS_STYLE_BORDER_STYLE_HIDDEN:
    //NS_ASSERTION(false, "style of none or hidden");
    break;
  case NS_STYLE_BORDER_STYLE_DOTTED:
  case NS_STYLE_BORDER_STYLE_DASHED:
    {
      nscoord dashLength = (NS_STYLE_BORDER_STYLE_DASHED == aBorderStyle) ? DASH_LENGTH : DOT_LENGTH;
      // make the dash length proportional to the border thickness
      dashLength *= (horizontal) ? aBorder.height : aBorder.width;
      // make the min dash length for the ends 1/2 the dash length
      nscoord minDashLength = (NS_STYLE_BORDER_STYLE_DASHED == aBorderStyle)
                              ? RoundFloatToPixel(((float)dashLength) / 2.0f, twipsPerPixel) : dashLength;
      minDashLength = std::max(minDashLength, twipsPerPixel);
      nscoord numDashSpaces = 0;
      nscoord startDashLength = minDashLength;
      nscoord endDashLength   = minDashLength;
      if (horizontal) {
        GetDashInfo(aBorder.width, dashLength, twipsPerPixel, numDashSpaces,
                    startDashLength, endDashLength);
        nsRect rect(aBorder.x, aBorder.y, startDashLength, aBorder.height);
        DrawSolidBorderSegment(aDrawTarget, rect, aBorderColor,
                               aAppUnitsPerDevPixel, twipsPerPixel);

        rect.x += startDashLength + dashLength;
        rect.width = aBorder.width
                     - (startDashLength + endDashLength + dashLength);
        DrawDashedSegment(aDrawTarget, rect, dashLength, aBorderColor,
                          aAppUnitsPerDevPixel, twipsPerPixel, horizontal);

        rect.x += rect.width;
        rect.width = endDashLength;
        DrawSolidBorderSegment(aDrawTarget, rect, aBorderColor,
                               aAppUnitsPerDevPixel, twipsPerPixel);
      }
      else {
        GetDashInfo(aBorder.height, dashLength, twipsPerPixel, numDashSpaces,
                    startDashLength, endDashLength);
        nsRect rect(aBorder.x, aBorder.y, aBorder.width, startDashLength);
        DrawSolidBorderSegment(aDrawTarget, rect, aBorderColor,
                               aAppUnitsPerDevPixel, twipsPerPixel);

        rect.y += rect.height + dashLength;
        rect.height = aBorder.height
                      - (startDashLength + endDashLength + dashLength);
        DrawDashedSegment(aDrawTarget, rect, dashLength, aBorderColor,
                          aAppUnitsPerDevPixel, twipsPerPixel, horizontal);

        rect.y += rect.height;
        rect.height = endDashLength;
        DrawSolidBorderSegment(aDrawTarget, rect, aBorderColor,
                               aAppUnitsPerDevPixel, twipsPerPixel);
      }
    }
    break;
  case NS_STYLE_BORDER_STYLE_GROOVE:
    ridgeGroove = NS_STYLE_BORDER_STYLE_GROOVE; // and fall through to ridge
    MOZ_FALLTHROUGH;
  case NS_STYLE_BORDER_STYLE_RIDGE:
    if ((horizontal && (twipsPerPixel >= aBorder.height)) ||
        (!horizontal && (twipsPerPixel >= aBorder.width))) {
      // a one pixel border
      DrawSolidBorderSegment(aDrawTarget, aBorder, aBorderColor,
                             aAppUnitsPerDevPixel, twipsPerPixel,
                             aStartBevelSide, aStartBevelOffset,
                             aEndBevelSide, aEndBevelOffset);
    }
    else {
      nscoord startBevel = (aStartBevelOffset > 0)
                            ? RoundFloatToPixel(0.5f * (float)aStartBevelOffset, twipsPerPixel, true) : 0;
      nscoord endBevel =   (aEndBevelOffset > 0)
                            ? RoundFloatToPixel(0.5f * (float)aEndBevelOffset, twipsPerPixel, true) : 0;
      mozilla::Side ridgeGrooveSide = (horizontal) ? eSideTop : eSideLeft;
      // FIXME: In theory, this should use the visited-dependent
      // background color, but I don't care.
      nscolor bevelColor = MakeBevelColor(ridgeGrooveSide, ridgeGroove,
                                          aBGColor, aBorderColor);
      nsRect rect(aBorder);
      nscoord half;
      if (horizontal) { // top, bottom
        half = RoundFloatToPixel(0.5f * (float)aBorder.height, twipsPerPixel);
        rect.height = half;
        if (eSideTop == aStartBevelSide) {
          rect.x += startBevel;
          rect.width -= startBevel;
        }
        if (eSideTop == aEndBevelSide) {
          rect.width -= endBevel;
        }
        DrawSolidBorderSegment(aDrawTarget, rect, bevelColor,
                               aAppUnitsPerDevPixel, twipsPerPixel,
                               aStartBevelSide, startBevel, aEndBevelSide,
                               endBevel);
      }
      else { // left, right
        half = RoundFloatToPixel(0.5f * (float)aBorder.width, twipsPerPixel);
        rect.width = half;
        if (eSideLeft == aStartBevelSide) {
          rect.y += startBevel;
          rect.height -= startBevel;
        }
        if (eSideLeft == aEndBevelSide) {
          rect.height -= endBevel;
        }
        DrawSolidBorderSegment(aDrawTarget, rect, bevelColor,
                               aAppUnitsPerDevPixel, twipsPerPixel,
                               aStartBevelSide, startBevel, aEndBevelSide,
                               endBevel);
      }

      rect = aBorder;
      ridgeGrooveSide = (eSideTop == ridgeGrooveSide) ? eSideBottom : eSideRight;
      // FIXME: In theory, this should use the visited-dependent
      // background color, but I don't care.
      bevelColor = MakeBevelColor(ridgeGrooveSide, ridgeGroove,
                                  aBGColor, aBorderColor);
      if (horizontal) {
        rect.y = rect.y + half;
        rect.height = aBorder.height - half;
        if (eSideBottom == aStartBevelSide) {
          rect.x += startBevel;
          rect.width -= startBevel;
        }
        if (eSideBottom == aEndBevelSide) {
          rect.width -= endBevel;
        }
        DrawSolidBorderSegment(aDrawTarget, rect, bevelColor,
                               aAppUnitsPerDevPixel, twipsPerPixel,
                               aStartBevelSide, startBevel, aEndBevelSide,
                               endBevel);
      }
      else {
        rect.x = rect.x + half;
        rect.width = aBorder.width - half;
        if (eSideRight == aStartBevelSide) {
          rect.y += aStartBevelOffset - startBevel;
          rect.height -= startBevel;
        }
        if (eSideRight == aEndBevelSide) {
          rect.height -= endBevel;
        }
        DrawSolidBorderSegment(aDrawTarget, rect, bevelColor,
                               aAppUnitsPerDevPixel, twipsPerPixel,
                               aStartBevelSide, startBevel, aEndBevelSide,
                               endBevel);
      }
    }
    break;
  case NS_STYLE_BORDER_STYLE_DOUBLE:
    // We can only do "double" borders if the thickness of the border
    // is more than 2px.  Otherwise, we fall through to painting a
    // solid border.
    if ((aBorder.width > 2*twipsPerPixel || horizontal) &&
        (aBorder.height > 2*twipsPerPixel || !horizontal)) {
      nscoord startBevel = (aStartBevelOffset > 0)
                            ? RoundFloatToPixel(0.333333f * (float)aStartBevelOffset, twipsPerPixel) : 0;
      nscoord endBevel =   (aEndBevelOffset > 0)
                            ? RoundFloatToPixel(0.333333f * (float)aEndBevelOffset, twipsPerPixel) : 0;
      if (horizontal) { // top, bottom
        nscoord thirdHeight = RoundFloatToPixel(0.333333f * (float)aBorder.height, twipsPerPixel);

        // draw the top line or rect
        nsRect topRect(aBorder.x, aBorder.y, aBorder.width, thirdHeight);
        if (eSideTop == aStartBevelSide) {
          topRect.x += aStartBevelOffset - startBevel;
          topRect.width -= aStartBevelOffset - startBevel;
        }
        if (eSideTop == aEndBevelSide) {
          topRect.width -= aEndBevelOffset - endBevel;
        }
        DrawSolidBorderSegment(aDrawTarget, topRect, aBorderColor,
                               aAppUnitsPerDevPixel, twipsPerPixel,
                               aStartBevelSide, startBevel, aEndBevelSide,
                               endBevel);

        // draw the botom line or rect
        nscoord heightOffset = aBorder.height - thirdHeight;
        nsRect bottomRect(aBorder.x, aBorder.y + heightOffset, aBorder.width, aBorder.height - heightOffset);
        if (eSideBottom == aStartBevelSide) {
          bottomRect.x += aStartBevelOffset - startBevel;
          bottomRect.width -= aStartBevelOffset - startBevel;
        }
        if (eSideBottom == aEndBevelSide) {
          bottomRect.width -= aEndBevelOffset - endBevel;
        }
        DrawSolidBorderSegment(aDrawTarget, bottomRect, aBorderColor,
                               aAppUnitsPerDevPixel, twipsPerPixel,
                               aStartBevelSide, startBevel, aEndBevelSide,
                               endBevel);
      }
      else { // left, right
        nscoord thirdWidth = RoundFloatToPixel(0.333333f * (float)aBorder.width, twipsPerPixel);

        nsRect leftRect(aBorder.x, aBorder.y, thirdWidth, aBorder.height);
        if (eSideLeft == aStartBevelSide) {
          leftRect.y += aStartBevelOffset - startBevel;
          leftRect.height -= aStartBevelOffset - startBevel;
        }
        if (eSideLeft == aEndBevelSide) {
          leftRect.height -= aEndBevelOffset - endBevel;
        }
        DrawSolidBorderSegment(aDrawTarget, leftRect, aBorderColor,
                               aAppUnitsPerDevPixel, twipsPerPixel,
                               aStartBevelSide, startBevel, aEndBevelSide,
                               endBevel);

        nscoord widthOffset = aBorder.width - thirdWidth;
        nsRect rightRect(aBorder.x + widthOffset, aBorder.y, aBorder.width - widthOffset, aBorder.height);
        if (eSideRight == aStartBevelSide) {
          rightRect.y += aStartBevelOffset - startBevel;
          rightRect.height -= aStartBevelOffset - startBevel;
        }
        if (eSideRight == aEndBevelSide) {
          rightRect.height -= aEndBevelOffset - endBevel;
        }
        DrawSolidBorderSegment(aDrawTarget, rightRect, aBorderColor,
                               aAppUnitsPerDevPixel, twipsPerPixel,
                               aStartBevelSide, startBevel, aEndBevelSide,
                               endBevel);
      }
      break;
    }
    // else fall through to solid
    MOZ_FALLTHROUGH;
  case NS_STYLE_BORDER_STYLE_SOLID:
    DrawSolidBorderSegment(aDrawTarget, aBorder, aBorderColor,
                           aAppUnitsPerDevPixel, twipsPerPixel, aStartBevelSide,
                           aStartBevelOffset, aEndBevelSide, aEndBevelOffset);
    break;
  case NS_STYLE_BORDER_STYLE_OUTSET:
  case NS_STYLE_BORDER_STYLE_INSET:
    NS_ASSERTION(false, "inset, outset should have been converted to groove, ridge");
    break;
  case NS_STYLE_BORDER_STYLE_AUTO:
    NS_ASSERTION(false, "Unexpected 'auto' table border");
    break;
  }
}

// End table border-collapsing section

Rect
nsCSSRendering::ExpandPaintingRectForDecorationLine(
                  nsIFrame* aFrame,
                  const uint8_t aStyle,
                  const Rect& aClippedRect,
                  const Float aICoordInFrame,
                  const Float aCycleLength,
                  bool aVertical)
{
  switch (aStyle) {
    case NS_STYLE_TEXT_DECORATION_STYLE_DOTTED:
    case NS_STYLE_TEXT_DECORATION_STYLE_DASHED:
    case NS_STYLE_TEXT_DECORATION_STYLE_WAVY:
      break;
    default:
      NS_ERROR("Invalid style was specified");
      return aClippedRect;
  }

  nsBlockFrame* block = nullptr;
  // Note that when we paint the decoration lines in relative positioned
  // box, we should paint them like all of the boxes are positioned as static.
  nscoord framePosInBlockAppUnits = 0;
  for (nsIFrame* f = aFrame; f; f = f->GetParent()) {
    block = do_QueryFrame(f);
    if (block) {
      break;
    }
    framePosInBlockAppUnits += aVertical ?
      f->GetNormalPosition().y : f->GetNormalPosition().x;
  }

  NS_ENSURE_TRUE(block, aClippedRect);

  nsPresContext *pc = aFrame->PresContext();
  Float framePosInBlock = Float(pc->AppUnitsToGfxUnits(framePosInBlockAppUnits));
  int32_t rectPosInBlock =
    int32_t(NS_round(framePosInBlock + aICoordInFrame));
  int32_t extraStartEdge =
    rectPosInBlock - (rectPosInBlock / int32_t(aCycleLength) * aCycleLength);
  Rect rect(aClippedRect);
  if (aVertical) {
    rect.y -= extraStartEdge;
    rect.height += extraStartEdge;
  } else {
    rect.x -= extraStartEdge;
    rect.width += extraStartEdge;
  }
  return rect;
}

void
nsCSSRendering::PaintDecorationLine(nsIFrame* aFrame, DrawTarget& aDrawTarget,
                                    const PaintDecorationLineParams& aParams)
{
  NS_ASSERTION(aParams.style != NS_STYLE_TEXT_DECORATION_STYLE_NONE,
               "aStyle is none");

  Rect rect = ToRect(GetTextDecorationRectInternal(aParams.pt, aParams));
  if (rect.IsEmpty() || !rect.Intersects(aParams.dirtyRect)) {
    return;
  }

  if (aParams.decoration != NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE &&
      aParams.decoration != NS_STYLE_TEXT_DECORATION_LINE_OVERLINE &&
      aParams.decoration != NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH) {
    NS_ERROR("Invalid decoration value!");
    return;
  }

  Float lineThickness = std::max(NS_round(aParams.lineSize.height), 1.0);

  ColorPattern color(ToDeviceColor(aParams.color));
  StrokeOptions strokeOptions(lineThickness);
  DrawOptions drawOptions;

  Float dash[2];

  AutoPopClips autoPopClips(&aDrawTarget);

  switch (aParams.style) {
    case NS_STYLE_TEXT_DECORATION_STYLE_SOLID:
    case NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE:
      break;
    case NS_STYLE_TEXT_DECORATION_STYLE_DASHED: {
      autoPopClips.PushClipRect(rect);
      Float dashWidth = lineThickness * DOT_LENGTH * DASH_LENGTH;
      dash[0] = dashWidth;
      dash[1] = dashWidth;
      strokeOptions.mDashPattern = dash;
      strokeOptions.mDashLength = MOZ_ARRAY_LENGTH(dash);
      strokeOptions.mLineCap = CapStyle::BUTT;
      rect = ExpandPaintingRectForDecorationLine(aFrame, aParams.style,
                                                 rect, aParams.icoordInFrame,
                                                 dashWidth * 2,
                                                 aParams.vertical);
      // We should continue to draw the last dash even if it is not in the rect.
      rect.width += dashWidth;
      break;
    }
    case NS_STYLE_TEXT_DECORATION_STYLE_DOTTED: {
      autoPopClips.PushClipRect(rect);
      Float dashWidth = lineThickness * DOT_LENGTH;
      if (lineThickness > 2.0) {
        dash[0] = 0.f;
        dash[1] = dashWidth * 2.f;
        strokeOptions.mLineCap = CapStyle::ROUND;
      } else {
        dash[0] = dashWidth;
        dash[1] = dashWidth;
      }
      strokeOptions.mDashPattern = dash;
      strokeOptions.mDashLength = MOZ_ARRAY_LENGTH(dash);
      rect = ExpandPaintingRectForDecorationLine(aFrame, aParams.style,
                                                 rect, aParams.icoordInFrame,
                                                 dashWidth * 2,
                                                 aParams.vertical);
      // We should continue to draw the last dot even if it is not in the rect.
      rect.width += dashWidth;
      break;
    }
    case NS_STYLE_TEXT_DECORATION_STYLE_WAVY:
      autoPopClips.PushClipRect(rect);
      if (lineThickness > 2.0) {
        drawOptions.mAntialiasMode = AntialiasMode::SUBPIXEL;
      } else {
        // Don't use anti-aliasing here.  Because looks like lighter color wavy
        // line at this case.  And probably, users don't think the
        // non-anti-aliased wavy line is not pretty.
        drawOptions.mAntialiasMode = AntialiasMode::NONE;
      }
      break;
    default:
      NS_ERROR("Invalid style value!");
      return;
  }

  // The block-direction position should be set to the middle of the line.
  if (aParams.vertical) {
    rect.x += lineThickness / 2;
  } else {
    rect.y += lineThickness / 2;
  }

  switch (aParams.style) {
    case NS_STYLE_TEXT_DECORATION_STYLE_SOLID:
    case NS_STYLE_TEXT_DECORATION_STYLE_DOTTED:
    case NS_STYLE_TEXT_DECORATION_STYLE_DASHED: {
      Point p1 = rect.TopLeft();
      Point p2 = aParams.vertical ? rect.BottomLeft() : rect.TopRight();
      aDrawTarget.StrokeLine(p1, p2, color, strokeOptions, drawOptions);
      return;
    }
    case NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE: {
      /**
       *  We are drawing double line as:
       *
       * +-------------------------------------------+
       * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| ^
       * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| | lineThickness
       * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| v
       * |                                           |
       * |                                           |
       * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| ^
       * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| | lineThickness
       * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| v
       * +-------------------------------------------+
       */
      Point p1 = rect.TopLeft();
      Point p2 = aParams.vertical ? rect.BottomLeft() : rect.TopRight();
      aDrawTarget.StrokeLine(p1, p2, color, strokeOptions, drawOptions);

      if (aParams.vertical) {
        rect.width -= lineThickness;
      } else {
        rect.height -= lineThickness;
      }

      p1 = aParams.vertical ? rect.TopRight() : rect.BottomLeft();
      p2 = rect.BottomRight();
      aDrawTarget.StrokeLine(p1, p2, color, strokeOptions, drawOptions);
      return;
    }
    case NS_STYLE_TEXT_DECORATION_STYLE_WAVY: {
      /**
       *  We are drawing wavy line as:
       *
       *  P: Path, X: Painted pixel
       *
       *     +---------------------------------------+
       *   XX|X            XXXXXX            XXXXXX  |
       *   PP|PX          XPPPPPPX          XPPPPPPX |    ^
       *   XX|XPX        XPXXXXXXPX        XPXXXXXXPX|    |
       *     | XPX      XPX      XPX      XPX      XP|X   |adv
       *     |  XPXXXXXXPX        XPXXXXXXPX        X|PX  |
       *     |   XPPPPPPX          XPPPPPPX          |XPX v
       *     |    XXXXXX            XXXXXX           | XX
       *     +---------------------------------------+
       *      <---><--->                                ^
       *      adv  flatLengthAtVertex                   rightMost
       *
       *  1. Always starts from top-left of the drawing area, however, we need
       *     to draw  the line from outside of the rect.  Because the start
       *     point of the line is not good style if we draw from inside it.
       *  2. First, draw horizontal line from outside the rect to top-left of
       *     the rect;
       *  3. Goes down to bottom of the area at 45 degrees.
       *  4. Slides to right horizontaly, see |flatLengthAtVertex|.
       *  5. Goes up to top of the area at 45 degrees.
       *  6. Slides to right horizontaly.
       *  7. Repeat from 2 until reached to right-most edge of the area.
       *
       * In the vertical case, swap horizontal and vertical coordinates and
       * directions in the above description.
       */

      Float& rectICoord = aParams.vertical ? rect.y : rect.x;
      Float& rectISize = aParams.vertical ? rect.height : rect.width;
      const Float rectBSize = aParams.vertical ? rect.width : rect.height;

      const Float adv = rectBSize - lineThickness;
      const Float flatLengthAtVertex =
        std::max((lineThickness - 1.0) * 2.0, 1.0);

      // Align the start of wavy lines to the nearest ancestor block.
      const Float cycleLength = 2 * (adv + flatLengthAtVertex);
      rect = ExpandPaintingRectForDecorationLine(aFrame, aParams.style, rect,
                                                 aParams.icoordInFrame,
                                                 cycleLength, aParams.vertical);
      // figure out if we can trim whole cycles from the left and right edges
      // of the line, to try and avoid creating an unnecessarily long and
      // complex path
      const Float dirtyRectICoord = aParams.vertical ? aParams.dirtyRect.y
                                                     : aParams.dirtyRect.x;
      int32_t skipCycles = floor((dirtyRectICoord - rectICoord) / cycleLength);
      if (skipCycles > 0) {
        rectICoord += skipCycles * cycleLength;
        rectISize -= skipCycles * cycleLength;
      }

      rectICoord += lineThickness / 2.0;
      Point pt(rect.TopLeft());
      Float& ptICoord = aParams.vertical ? pt.y : pt.x;
      Float& ptBCoord = aParams.vertical ? pt.x : pt.y;
      if (aParams.vertical) {
        ptBCoord += adv + lineThickness / 2.0;
      }
      Float iCoordLimit = ptICoord + rectISize + lineThickness;

      const Float dirtyRectIMost = aParams.vertical ?
        aParams.dirtyRect.YMost() : aParams.dirtyRect.XMost();
      skipCycles = floor((iCoordLimit - dirtyRectIMost) / cycleLength);
      if (skipCycles > 0) {
        iCoordLimit -= skipCycles * cycleLength;
      }

      RefPtr<PathBuilder> builder = aDrawTarget.CreatePathBuilder();
      RefPtr<Path> path;

      ptICoord -= lineThickness;
      builder->MoveTo(pt); // 1

      ptICoord = rectICoord;
      builder->LineTo(pt); // 2

      // In vertical mode, to go "down" relative to the text we need to
      // decrease the block coordinate, whereas in horizontal we increase
      // it. So the sense of this flag is effectively inverted.
      bool goDown = aParams.vertical ? false : true;
      uint32_t iter = 0;
      while (ptICoord < iCoordLimit) {
        if (++iter > 1000) {
          // stroke the current path and start again, to avoid pathological
          // behavior in cairo with huge numbers of path segments
          path = builder->Finish();
          aDrawTarget.Stroke(path, color, strokeOptions, drawOptions);
          builder = aDrawTarget.CreatePathBuilder();
          builder->MoveTo(pt);
          iter = 0;
        }
        ptICoord += adv;
        ptBCoord += goDown ? adv : -adv;

        builder->LineTo(pt); // 3 and 5

        ptICoord += flatLengthAtVertex;
        builder->LineTo(pt); // 4 and 6

        goDown = !goDown;
      }
      path = builder->Finish();
      aDrawTarget.Stroke(path, color, strokeOptions, drawOptions);
      return;
    }
    default:
      NS_ERROR("Invalid style value!");
  }
}

Rect
nsCSSRendering::DecorationLineToPath(const PaintDecorationLineParams& aParams)
{
  NS_ASSERTION(aParams.style != NS_STYLE_TEXT_DECORATION_STYLE_NONE,
               "aStyle is none");

  Rect path; // To benefit from RVO, we return this from all return points

  Rect rect = ToRect(GetTextDecorationRectInternal(aParams.pt, aParams));
  if (rect.IsEmpty() || !rect.Intersects(aParams.dirtyRect)) {
    return path;
  }

  if (aParams.decoration != NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE &&
      aParams.decoration != NS_STYLE_TEXT_DECORATION_LINE_OVERLINE &&
      aParams.decoration != NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH) {
    NS_ERROR("Invalid decoration value!");
    return path;
  }

  if (aParams.style != NS_STYLE_TEXT_DECORATION_STYLE_SOLID) {
    // For the moment, we support only solid text decorations.
    return path;
  }

  Float lineThickness = std::max(NS_round(aParams.lineSize.height), 1.0);

  // The block-direction position should be set to the middle of the line.
  if (aParams.vertical) {
    rect.x += lineThickness / 2;
    path = Rect(rect.TopLeft() - Point(lineThickness / 2, 0.0),
                Size(lineThickness, rect.Height()));
  } else {
    rect.y += lineThickness / 2;
    path = Rect(rect.TopLeft() - Point(0.0, lineThickness / 2),
                Size(rect.Width(), lineThickness));
  }

  return path;
}

nsRect
nsCSSRendering::GetTextDecorationRect(nsPresContext* aPresContext,
                                      const DecorationRectParams& aParams)
{
  NS_ASSERTION(aPresContext, "aPresContext is null");
  NS_ASSERTION(aParams.style != NS_STYLE_TEXT_DECORATION_STYLE_NONE,
               "aStyle is none");

  gfxRect rect = GetTextDecorationRectInternal(Point(0, 0), aParams);
  // The rect values are already rounded to nearest device pixels.
  nsRect r;
  r.x = aPresContext->GfxUnitsToAppUnits(rect.X());
  r.y = aPresContext->GfxUnitsToAppUnits(rect.Y());
  r.width = aPresContext->GfxUnitsToAppUnits(rect.Width());
  r.height = aPresContext->GfxUnitsToAppUnits(rect.Height());
  return r;
}

gfxRect
nsCSSRendering::GetTextDecorationRectInternal(const Point& aPt,
                                              const DecorationRectParams& aParams)
{
  NS_ASSERTION(aParams.style <= NS_STYLE_TEXT_DECORATION_STYLE_WAVY,
               "Invalid aStyle value");

  if (aParams.style == NS_STYLE_TEXT_DECORATION_STYLE_NONE)
    return gfxRect(0, 0, 0, 0);

  bool canLiftUnderline = aParams.descentLimit >= 0.0;

  gfxFloat iCoord = aParams.vertical ? aPt.y : aPt.x;
  gfxFloat bCoord = aParams.vertical ? aPt.x : aPt.y;

  // 'left' and 'right' are relative to the line, so for vertical writing modes
  // they will actually become top and bottom of the rendered line.
  // Similarly, aLineSize.width and .height are actually length and thickness
  // of the line, which runs horizontally or vertically according to aVertical.
  const gfxFloat left  = floor(iCoord + 0.5),
                 right = floor(iCoord + aParams.lineSize.width + 0.5);

  // We compute |r| as if for a horizontal text run, and then swap vertical
  // and horizontal coordinates at the end if vertical was requested.
  gfxRect r(left, 0, right - left, 0);

  gfxFloat lineThickness = NS_round(aParams.lineSize.height);
  lineThickness = std::max(lineThickness, 1.0);

  gfxFloat ascent = NS_round(aParams.ascent);
  gfxFloat descentLimit = floor(aParams.descentLimit);

  gfxFloat suggestedMaxRectHeight = std::max(std::min(ascent, descentLimit), 1.0);
  r.height = lineThickness;
  if (aParams.style == NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE) {
    /**
     *  We will draw double line as:
     *
     * +-------------------------------------------+
     * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| ^
     * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| | lineThickness
     * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| v
     * |                                           | ^
     * |                                           | | gap
     * |                                           | v
     * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| ^
     * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| | lineThickness
     * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| v
     * +-------------------------------------------+
     */
    gfxFloat gap = NS_round(lineThickness / 2.0);
    gap = std::max(gap, 1.0);
    r.height = lineThickness * 2.0 + gap;
    if (canLiftUnderline) {
      if (r.Height() > suggestedMaxRectHeight) {
        // Don't shrink the line height, because the thickness has some meaning.
        // We can just shrink the gap at this time.
        r.height = std::max(suggestedMaxRectHeight, lineThickness * 2.0 + 1.0);
      }
    }
  } else if (aParams.style == NS_STYLE_TEXT_DECORATION_STYLE_WAVY) {
    /**
     *  We will draw wavy line as:
     *
     * +-------------------------------------------+
     * |XXXXX            XXXXXX            XXXXXX  | ^
     * |XXXXXX          XXXXXXXX          XXXXXXXX | | lineThickness
     * |XXXXXXX        XXXXXXXXXX        XXXXXXXXXX| v
     * |     XXX      XXX      XXX      XXX      XX|
     * |      XXXXXXXXXX        XXXXXXXXXX        X|
     * |       XXXXXXXX          XXXXXXXX          |
     * |        XXXXXX            XXXXXX           |
     * +-------------------------------------------+
     */
    r.height = lineThickness > 2.0 ? lineThickness * 4.0 : lineThickness * 3.0;
    if (canLiftUnderline) {
      if (r.Height() > suggestedMaxRectHeight) {
        // Don't shrink the line height even if there is not enough space,
        // because the thickness has some meaning.  E.g., the 1px wavy line and
        // 2px wavy line can be used for different meaning in IME selections
        // at same time.
        r.height = std::max(suggestedMaxRectHeight, lineThickness * 2.0);
      }
    }
  }

  gfxFloat baseline = floor(bCoord + aParams.ascent + 0.5);
  gfxFloat offset = 0.0;
  switch (aParams.decoration) {
    case NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE:
      offset = aParams.offset;
      if (canLiftUnderline) {
        if (descentLimit < -offset + r.Height()) {
          // If we can ignore the offset and the decoration line is overflowing,
          // we should align the bottom edge of the decoration line rect if it's
          // possible.  Otherwise, we should lift up the top edge of the rect as
          // far as possible.
          gfxFloat offsetBottomAligned = -descentLimit + r.Height();
          gfxFloat offsetTopAligned = 0.0;
          offset = std::min(offsetBottomAligned, offsetTopAligned);
        }
      }
      break;
    case NS_STYLE_TEXT_DECORATION_LINE_OVERLINE:
      offset = aParams.offset - lineThickness + r.Height();
      break;
    case NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH: {
      gfxFloat extra = floor(r.Height() / 2.0 + 0.5);
      extra = std::max(extra, lineThickness);
      offset = aParams.offset - lineThickness + extra;
      break;
    }
    default:
      NS_ERROR("Invalid decoration value!");
  }

  if (aParams.vertical) {
    r.y = baseline + floor(offset + 0.5);
    Swap(r.x, r.y);
    Swap(r.width, r.height);
  } else {
    r.y = baseline - floor(offset + 0.5);
  }

  return r;
}

#define MAX_BLUR_RADIUS 300
#define MAX_SPREAD_RADIUS 50

static inline gfxPoint ComputeBlurStdDev(nscoord aBlurRadius,
                                         int32_t aAppUnitsPerDevPixel,
                                         gfxFloat aScaleX,
                                         gfxFloat aScaleY)
{
  // http://dev.w3.org/csswg/css3-background/#box-shadow says that the
  // standard deviation of the blur should be half the given blur value.
  gfxFloat blurStdDev = gfxFloat(aBlurRadius) / gfxFloat(aAppUnitsPerDevPixel);

  return gfxPoint(std::min((blurStdDev * aScaleX),
                           gfxFloat(MAX_BLUR_RADIUS)) / 2.0,
                  std::min((blurStdDev * aScaleY),
                           gfxFloat(MAX_BLUR_RADIUS)) / 2.0);
}

static inline IntSize
ComputeBlurRadius(nscoord aBlurRadius,
                  int32_t aAppUnitsPerDevPixel,
                  gfxFloat aScaleX = 1.0,
                  gfxFloat aScaleY = 1.0)
{
  gfxPoint scaledBlurStdDev = ComputeBlurStdDev(aBlurRadius, aAppUnitsPerDevPixel,
                                                aScaleX, aScaleY);
  return
    gfxAlphaBoxBlur::CalculateBlurRadius(scaledBlurStdDev);
}

// -----
// nsContextBoxBlur
// -----
gfxContext*
nsContextBoxBlur::Init(const nsRect& aRect, nscoord aSpreadRadius,
                       nscoord aBlurRadius,
                       int32_t aAppUnitsPerDevPixel,
                       gfxContext* aDestinationCtx,
                       const nsRect& aDirtyRect,
                       const gfxRect* aSkipRect,
                       uint32_t aFlags)
{
  if (aRect.IsEmpty()) {
    mContext = nullptr;
    return nullptr;
  }

  IntSize blurRadius;
  IntSize spreadRadius;
  GetBlurAndSpreadRadius(aDestinationCtx->GetDrawTarget(), aAppUnitsPerDevPixel,
                         aBlurRadius, aSpreadRadius,
                         blurRadius, spreadRadius);

  mDestinationCtx = aDestinationCtx;

  // If not blurring, draw directly onto the destination device
  if (blurRadius.width <= 0 && blurRadius.height <= 0 &&
      spreadRadius.width <= 0 && spreadRadius.height <= 0 &&
      !(aFlags & FORCE_MASK)) {
    mContext = aDestinationCtx;
    return mContext;
  }

  // Convert from app units to device pixels
  gfxRect rect = nsLayoutUtils::RectToGfxRect(aRect, aAppUnitsPerDevPixel);

  gfxRect dirtyRect =
    nsLayoutUtils::RectToGfxRect(aDirtyRect, aAppUnitsPerDevPixel);
  dirtyRect.RoundOut();

  gfxMatrix transform = aDestinationCtx->CurrentMatrix();
  rect = transform.TransformBounds(rect);

  mPreTransformed = !transform.IsIdentity();

  // Create the temporary surface for blurring
  dirtyRect = transform.TransformBounds(dirtyRect);
  bool useHardwareAccel = !(aFlags & DISABLE_HARDWARE_ACCELERATION_BLUR);
  if (aSkipRect) {
    gfxRect skipRect = transform.TransformBounds(*aSkipRect);
    mContext = mAlphaBoxBlur.Init(aDestinationCtx, rect, spreadRadius,
                                  blurRadius, &dirtyRect, &skipRect,
                                  useHardwareAccel);
  } else {
    mContext = mAlphaBoxBlur.Init(aDestinationCtx, rect, spreadRadius,
                                  blurRadius, &dirtyRect, nullptr,
                                  useHardwareAccel);
  }

  if (mContext) {
    // we don't need to blur if skipRect is equal to rect
    // and mContext will be nullptr
    mContext->Multiply(transform);
  }
  return mContext;
}

void
nsContextBoxBlur::DoPaint()
{
  if (mContext == mDestinationCtx) {
    return;
  }

  gfxContextMatrixAutoSaveRestore saveMatrix(mDestinationCtx);

  if (mPreTransformed) {
    mDestinationCtx->SetMatrix(gfxMatrix());
  }

  mAlphaBoxBlur.Paint(mDestinationCtx);
}

gfxContext*
nsContextBoxBlur::GetContext()
{
  return mContext;
}

/* static */ nsMargin
nsContextBoxBlur::GetBlurRadiusMargin(nscoord aBlurRadius,
                                      int32_t aAppUnitsPerDevPixel)
{
  IntSize blurRadius = ComputeBlurRadius(aBlurRadius, aAppUnitsPerDevPixel);

  nsMargin result;
  result.top = result.bottom = blurRadius.height * aAppUnitsPerDevPixel;
  result.left = result.right = blurRadius.width  * aAppUnitsPerDevPixel;
  return result;
}

/* static */ void
nsContextBoxBlur::BlurRectangle(gfxContext* aDestinationCtx,
                                const nsRect& aRect,
                                int32_t aAppUnitsPerDevPixel,
                                RectCornerRadii* aCornerRadii,
                                nscoord aBlurRadius,
                                const Color& aShadowColor,
                                const nsRect& aDirtyRect,
                                const gfxRect& aSkipRect)
{
  DrawTarget& aDestDrawTarget = *aDestinationCtx->GetDrawTarget();

  if (aRect.IsEmpty()) {
    return;
  }

  Rect shadowGfxRect = NSRectToRect(aRect, aAppUnitsPerDevPixel);

  if (aBlurRadius <= 0) {
    ColorPattern color(ToDeviceColor(aShadowColor));
    if (aCornerRadii) {
      RefPtr<Path> roundedRect = MakePathForRoundedRect(aDestDrawTarget,
                                                        shadowGfxRect,
                                                        *aCornerRadii);
      aDestDrawTarget.Fill(roundedRect, color);
    } else {
      aDestDrawTarget.FillRect(shadowGfxRect, color);
    }
    return;
  }

  gfxFloat scaleX = 1;
  gfxFloat scaleY = 1;

  // Do blurs in device space when possible.
  // Chrome/Skia always does the blurs in device space
  // and will sometimes get incorrect results (e.g. rotated blurs)
  gfxMatrix transform = aDestinationCtx->CurrentMatrix();
  // XXX: we could probably handle negative scales but for now it's easier just to fallback
  if (!transform.HasNonAxisAlignedTransform() && transform._11 > 0.0 && transform._22 > 0.0) {
    scaleX = transform._11;
    scaleY = transform._22;
    aDestinationCtx->SetMatrix(gfxMatrix());
  } else {
    transform = gfxMatrix();
  }

  gfxPoint blurStdDev = ComputeBlurStdDev(aBlurRadius, aAppUnitsPerDevPixel, scaleX, scaleY);

  gfxRect dirtyRect =
    nsLayoutUtils::RectToGfxRect(aDirtyRect, aAppUnitsPerDevPixel);
  dirtyRect.RoundOut();

  gfxRect shadowThebesRect = transform.TransformBounds(ThebesRect(shadowGfxRect));
  dirtyRect = transform.TransformBounds(dirtyRect);
  gfxRect skipRect = transform.TransformBounds(aSkipRect);

  if (aCornerRadii) {
    aCornerRadii->Scale(scaleX, scaleY);
  }

  gfxAlphaBoxBlur::BlurRectangle(aDestinationCtx,
                                 shadowThebesRect,
                                 aCornerRadii,
                                 blurStdDev,
                                 aShadowColor,
                                 dirtyRect,
                                 skipRect);
}

/* static */ void
nsContextBoxBlur::GetBlurAndSpreadRadius(DrawTarget* aDestDrawTarget,
                                         int32_t aAppUnitsPerDevPixel,
                                         nscoord aBlurRadius,
                                         nscoord aSpreadRadius,
                                         IntSize& aOutBlurRadius,
                                         IntSize& aOutSpreadRadius,
                                         bool aConstrainSpreadRadius)
{
  // Do blurs in device space when possible.
  // Chrome/Skia always does the blurs in device space
  // and will sometimes get incorrect results (e.g. rotated blurs)
  Matrix transform = aDestDrawTarget->GetTransform();
  // XXX: we could probably handle negative scales but for now it's easier just to fallback
  gfxFloat scaleX, scaleY;
  if (transform.HasNonAxisAlignedTransform() || transform._11 <= 0.0 || transform._22 <= 0.0) {
    scaleX = 1;
    scaleY = 1;
  } else {
    scaleX = transform._11;
    scaleY = transform._22;
  }

  // compute a large or smaller blur radius
  aOutBlurRadius = ComputeBlurRadius(aBlurRadius, aAppUnitsPerDevPixel, scaleX, scaleY);
  aOutSpreadRadius =
      IntSize(int32_t(aSpreadRadius * scaleX / aAppUnitsPerDevPixel),
              int32_t(aSpreadRadius * scaleY / aAppUnitsPerDevPixel));


  if (aConstrainSpreadRadius) {
    aOutSpreadRadius.width = std::min(aOutSpreadRadius.width, int32_t(MAX_SPREAD_RADIUS));
    aOutSpreadRadius.height = std::min(aOutSpreadRadius.height, int32_t(MAX_SPREAD_RADIUS));
  }
}

/* static */ bool
nsContextBoxBlur::InsetBoxBlur(gfxContext* aDestinationCtx,
                               Rect aDestinationRect,
                               Rect aShadowClipRect,
                               Color& aShadowColor,
                               nscoord aBlurRadiusAppUnits,
                               nscoord aSpreadDistanceAppUnits,
                               int32_t aAppUnitsPerDevPixel,
                               bool aHasBorderRadius,
                               RectCornerRadii& aInnerClipRectRadii,
                               Rect aSkipRect, Point aShadowOffset)
{
  if (aDestinationRect.IsEmpty()) {
    mContext = nullptr;
    return false;
  }

  gfxContextAutoSaveRestore autoRestore(aDestinationCtx);

  IntSize blurRadius;
  IntSize spreadRadius;
  // Convert the blur and spread radius to device pixels
  bool constrainSpreadRadius = false;
  GetBlurAndSpreadRadius(aDestinationCtx->GetDrawTarget(), aAppUnitsPerDevPixel,
                         aBlurRadiusAppUnits, aSpreadDistanceAppUnits,
                         blurRadius, spreadRadius, constrainSpreadRadius);

  // The blur and spread radius are scaled already, so scale all
  // input data to the blur. This way, we don't have to scale the min
  // inset blur to the invert of the dest context, then rescale it back
  // when we draw to the destination surface.
  gfxSize scale = aDestinationCtx->CurrentMatrix().ScaleFactors(true);
  Matrix transform = ToMatrix(aDestinationCtx->CurrentMatrix());

  // XXX: we could probably handle negative scales but for now it's easier just to fallback
  if (!transform.HasNonAxisAlignedTransform() && transform._11 > 0.0 && transform._22 > 0.0) {
    // If we don't have a rotation, we're pre-transforming all the rects.
    aDestinationCtx->SetMatrix(gfxMatrix());
  } else {
    // Don't touch anything, we have a rotation.
    transform = Matrix();
  }

  Rect transformedDestRect = transform.TransformBounds(aDestinationRect);
  Rect transformedShadowClipRect = transform.TransformBounds(aShadowClipRect);
  Rect transformedSkipRect = transform.TransformBounds(aSkipRect);

  transformedDestRect.Round();
  transformedShadowClipRect.Round();
  transformedSkipRect.RoundIn();

  for (size_t i = 0; i < 4; i++) {
    aInnerClipRectRadii[i].width = std::floor(scale.width * aInnerClipRectRadii[i].width);
    aInnerClipRectRadii[i].height = std::floor(scale.height * aInnerClipRectRadii[i].height);
  }

  mAlphaBoxBlur.BlurInsetBox(aDestinationCtx, transformedDestRect,
                             transformedShadowClipRect,
                             blurRadius, aShadowColor,
                             aHasBorderRadius ? &aInnerClipRectRadii : nullptr,
                             transformedSkipRect, aShadowOffset);
  return true;
}
