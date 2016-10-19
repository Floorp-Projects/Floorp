/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* utility functions for drawing borders and backgrounds */

#include <ctime>

#include "gfx2DGlue.h"
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
#include "gfxContext.h"
#include "nsRenderingContext.h"
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
        aFrame->Properties().Get(nsIFrame::IBSplitPrevSibling());
      if (block) {
        // The {ib} properties are only stored on first continuations
        NS_ASSERTION(!block->GetPrevContinuation(),
                     "Incorrect value for IBSplitPrevSibling");
        prevCont =
          block->Properties().Get(nsIFrame::IBSplitPrevSibling());
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
      nsIFrame* block = aFrame->Properties().Get(nsIFrame::IBSplitSibling());
      if (block) {
        nextCont = block->Properties().Get(nsIFrame::IBSplitSibling());
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

// A resolved color stop, with a specific position along the gradient line and
// a color.
struct ColorStop {
  ColorStop(): mPosition(0), mIsMidpoint(false) {}
  ColorStop(double aPosition, bool aIsMidPoint, const Color& aColor) :
    mPosition(aPosition), mIsMidpoint(aIsMidPoint), mColor(aColor) {}
  double mPosition; // along the gradient line; 0=start, 1=end
  bool mIsMidpoint;
  Color mColor;
};

/* Local functions */
static DrawResult DrawBorderImage(nsPresContext* aPresContext,
                                  nsRenderingContext& aRenderingContext,
                                  nsIFrame* aForFrame,
                                  const nsRect& aBorderArea,
                                  const nsStyleBorder& aStyleBorder,
                                  const nsRect& aDirtyRect,
                                  Sides aSkipSides,
                                  PaintBorderFlags aFlags);

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
JoinBoxesForVerticalSlice(nsIFrame* aFrame, const nsRect& aBorderArea)
{
  // Inflate vertically as if our continuations were laid out vertically
  // adjacent. Note that we don't touch the width.
  nsRect borderArea = aBorderArea;
  nscoord h = 0;
  nsIFrame* f = aFrame->GetNextContinuation();
  for (; f; f = f->GetNextContinuation()) {
    MOZ_ASSERT(!(f->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT),
               "anonymous ib-split block shouldn't have border/background");
    h += f->GetRect().height;
  }
  borderArea.height += h;
  h = 0;
  f = aFrame->GetPrevContinuation();
  for (; f; f = f->GetPrevContinuation()) {
    MOZ_ASSERT(!(f->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT),
               "anonymous ib-split block shouldn't have border/background");
    h += f->GetRect().height;
  }
  borderArea.y -= h;
  borderArea.height += h;
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
  return JoinBoxesForVerticalSlice(aFrame, aBorderArea);
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

  (*oBorderRadii)[C_TL] = Size(radii[NS_CORNER_TOP_LEFT_X],
                               radii[NS_CORNER_TOP_LEFT_Y]);
  (*oBorderRadii)[C_TR] = Size(radii[NS_CORNER_TOP_RIGHT_X],
                               radii[NS_CORNER_TOP_RIGHT_Y]);
  (*oBorderRadii)[C_BR] = Size(radii[NS_CORNER_BOTTOM_RIGHT_X],
                               radii[NS_CORNER_BOTTOM_RIGHT_Y]);
  (*oBorderRadii)[C_BL] = Size(radii[NS_CORNER_BOTTOM_LEFT_X],
                               radii[NS_CORNER_BOTTOM_LEFT_Y]);
}

DrawResult
nsCSSRendering::PaintBorder(nsPresContext* aPresContext,
                            nsRenderingContext& aRenderingContext,
                            nsIFrame* aForFrame,
                            const nsRect& aDirtyRect,
                            const nsRect& aBorderArea,
                            nsStyleContext* aStyleContext,
                            PaintBorderFlags aFlags,
                            Sides aSkipSides)
{
  PROFILER_LABEL("nsCSSRendering", "PaintBorder",
    js::ProfileEntry::Category::GRAPHICS);

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
    nscolor color = aStyleContext->GetVisitedDependentColor(
      nsCSSProps::SubpropertyEntryFor(eCSSProperty_border_color)[side]);
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
    nscolor color = aStyleContext->GetVisitedDependentColor(
      nsCSSProps::SubpropertyEntryFor(eCSSProperty_border_color)[side]);
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
  nscolor bgColor =
    bgContext->GetVisitedDependentColor(eCSSProperty_background_color);

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
                                           nsRenderingContext& aRenderingContext,
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

  if (aStyleBorder.IsBorderImageLoaded()) {
    return DrawBorderImage(aPresContext, aRenderingContext, aForFrame,
                           aBorderArea, aStyleBorder, aDirtyRect,
                           aSkipSides, aFlags);
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
    aFrame->Properties().Get(nsIFrame::OutlineInnerRectProperty());
  if (savedOutlineInnerRect)
    return *savedOutlineInnerRect;
  NS_NOTREACHED("we should have saved a frame property");
  return nsRect(nsPoint(0, 0), aFrame->GetSize());
}

void
nsCSSRendering::PaintOutline(nsPresContext* aPresContext,
                             nsRenderingContext& aRenderingContext,
                             nsIFrame* aForFrame,
                             const nsRect& aDirtyRect,
                             const nsRect& aBorderArea,
                             nsStyleContext* aStyleContext)
{
  nscoord             twipsRadii[8];

  // Get our style context's color struct.
  const nsStyleOutline* ourOutline = aStyleContext->StyleOutline();
  MOZ_ASSERT(ourOutline != NS_STYLE_BORDER_STYLE_NONE,
             "shouldn't have created nsDisplayOutline item");

  uint8_t outlineStyle = ourOutline->mOutlineStyle;
  nscoord width = ourOutline->GetOutlineWidth();

  if (width == 0 && outlineStyle != NS_STYLE_BORDER_STYLE_AUTO) {
    // Empty outline
    return;
  }

  nsIFrame* bgFrame = nsCSSRendering::FindNonTransparentBackgroundFrame
    (aForFrame, false);
  nsStyleContext* bgContext = bgFrame->StyleContext();
  nscolor bgColor =
    bgContext->GetVisitedDependentColor(eCSSProperty_background_color);

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
    return;

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

  if (outlineStyle == NS_STYLE_BORDER_STYLE_AUTO) {
    if (nsLayoutUtils::IsOutlineStyleAutoEnabled()) {
      nsITheme* theme = aPresContext->GetTheme();
      if (theme && theme->ThemeSupportsWidget(aPresContext, aForFrame,
                                              NS_THEME_FOCUS_OUTLINE)) {
        theme->DrawWidgetBackground(&aRenderingContext, aForFrame,
                                    NS_THEME_FOCUS_OUTLINE, innerRect,
                                    aDirtyRect);
        return;
      }
    }
    if (width == 0) {
      return; // empty outline
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
    aStyleContext->GetVisitedDependentColor(eCSSProperty_outline_color);
  nscolor outlineColors[4] = { outlineColor,
                               outlineColor,
                               outlineColor,
                               outlineColor };

  // convert the border widths
  Float outlineWidths[4] = { Float(width / twipsPerPixel),
                             Float(width / twipsPerPixel),
                             Float(width / twipsPerPixel),
                             Float(width / twipsPerPixel) };
  Rect dirtyRect = NSRectToRect(aDirtyRect, twipsPerPixel);

  nsIDocument* document = nullptr;
  nsIContent* content = aForFrame->GetContent();
  if (content) {
    document = content->OwnerDoc();
  }

  // start drawing

  nsCSSBorderRenderer br(aPresContext,
                         document,
                         aRenderingContext.GetDrawTarget(),
                         dirtyRect,
                         oRect,
                         outlineStyles,
                         outlineWidths,
                         outlineRadii,
                         outlineColors,
                         nullptr,
                         bgColor);
  br.DrawBorders();

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
  Float focusWidths[4] = { Float(oneCSSPixel / oneDevPixel),
                           Float(oneCSSPixel / oneDevPixel),
                           Float(oneCSSPixel / oneDevPixel),
                           Float(oneCSSPixel / oneDevPixel) };

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
    if (NS_GET_A(frame->StyleBackground()->mBackgroundColor) > 0)
      break;

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
  nsIAtom* frameType = aFrame->GetType();
  return frameType == nsGkAtoms::canvasFrame ||
         frameType == nsGkAtoms::rootFrame ||
         frameType == nsGkAtoms::pageContentFrame ||
         frameType == nsGkAtoms::viewportFrame;
}

nsIFrame*
nsCSSRendering::FindBackgroundStyleFrame(nsIFrame* aForFrame)
{
  const nsStyleBackground* result = aForFrame->StyleBackground();

  // Check if we need to do propagation from BODY rather than HTML.
  if (!result->IsTransparent()) {
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
  return !htmlBG->IsTransparent();
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

void
nsCSSRendering::PaintBoxShadowOuter(nsPresContext* aPresContext,
                                    nsRenderingContext& aRenderingContext,
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
  bool nativeTheme; // mutually exclusive with hasBorderRadius
  const nsStyleDisplay* styleDisplay = aForFrame->StyleDisplay();
  nsITheme::Transparency transparency;
  if (aForFrame->IsThemed(styleDisplay, &transparency)) {
    // We don't respect border-radius for native-themed widgets
    hasBorderRadius = false;
    // For opaque (rectangular) theme widgets we can take the generic
    // border-box path with border-radius disabled.
    nativeTheme = transparency != nsITheme::eOpaque;
  } else {
    nativeTheme = false;
    hasBorderRadius = true; // we'll update this below
  }

  nsRect frameRect = nativeTheme ?
    aForFrame->GetVisualOverflowRectRelativeToSelf() + aFrameArea.TopLeft() :
    aFrameArea;
  Sides skipSides = aForFrame->GetSkipSides();
  frameRect = ::BoxDecorationRectForBorder(aForFrame, frameRect, skipSides);

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

  gfxContext* renderContext = aRenderingContext.ThebesContext();

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

    // Set the shadow color; if not specified, use the foreground color
    nscolor shadowColor;
    if (shadowItem->mHasColor)
      shadowColor = shadowItem->mColor;
    else
      shadowColor = aForFrame->StyleColor()->mColor;

    Color gfxShadowColor(Color::FromABGR(shadowColor));
    gfxShadowColor.a *= aOpacity;

    if (nativeTheme) {
      nsContextBoxBlur blurringArea;

      // When getting the widget shape from the native theme, we're going
      // to draw the widget into the shadow surface to create a mask.
      // We need to ensure that there actually *is* a shadow surface
      // and that we're not going to draw directly into renderContext.
      gfxContext* shadowContext =
        blurringArea.Init(shadowRect, shadowItem->mSpread,
                          blurRadius, twipsPerPixel, renderContext, aDirtyRect,
                          useSkipGfxRect ? &skipGfxRect : nullptr,
                          nsContextBoxBlur::FORCE_MASK);
      if (!shadowContext)
        continue;

      MOZ_ASSERT(shadowContext == blurringArea.GetContext());

      renderContext->Save();
      renderContext->SetColor(gfxShadowColor);

      // Draw the shape of the frame so it can be blurred. Recall how nsContextBoxBlur
      // doesn't make any temporary surfaces if blur is 0 and it just returns the original
      // surface? If we have no blur, we're painting this fill on the actual content surface
      // (renderContext == shadowContext) which is why we set up the color and clip
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
        shadowContext->CurrentMatrix().Translate(devPixelOffset));

      nsRect nativeRect = aDirtyRect;
      nativeRect.MoveBy(-nsPoint(shadowItem->mXOffset, shadowItem->mYOffset));
      nativeRect.IntersectRect(frameRect, nativeRect);
      nsRenderingContext wrapperCtx(shadowContext);
      aPresContext->GetTheme()->DrawWidgetBackground(&wrapperCtx, aForFrame,
          styleDisplay->mAppearance, aFrameArea, nativeRect);

      blurringArea.DoPaint();
      renderContext->Restore();
    } else {
      renderContext->Save();

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
        renderContext->Clip(path);
      }

      // Clip the shadow so that we only get the part that applies to aForFrame.
      nsRect fragmentClip = shadowRectPlusBlur;
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
      renderContext->
        Clip(NSRectToSnappedRect(fragmentClip,
                                 aForFrame->PresContext()->AppUnitsPerDevPixel(),
                                 aDrawTarget));

      RectCornerRadii clipRectRadii;
      if (hasBorderRadius) {
        Float spreadDistance = shadowItem->mSpread / twipsPerPixel;

        Float borderSizes[4];

        borderSizes[eSideLeft] = spreadDistance;
        borderSizes[eSideTop] = spreadDistance;
        borderSizes[eSideRight] = spreadDistance;
        borderSizes[eSideBottom] = spreadDistance;

        nsCSSBorderRenderer::ComputeOuterRadii(borderRadii, borderSizes,
            &clipRectRadii);

      }
      nsContextBoxBlur::BlurRectangle(renderContext,
                                      shadowRect,
                                      twipsPerPixel,
                                      hasBorderRadius ? &clipRectRadii : nullptr,
                                      blurRadius,
                                      gfxShadowColor,
                                      aDirtyRect,
                                      skipGfxRect);
      renderContext->Restore();
    }

  }
}

void
nsCSSRendering::PaintBoxShadowInner(nsPresContext* aPresContext,
                                    nsRenderingContext& aRenderingContext,
                                    nsIFrame* aForFrame,
                                    const nsRect& aFrameArea)
{
  nsCSSShadowArray* shadows = aForFrame->StyleEffects()->mBoxShadow;
  if (!shadows)
    return;
  if (aForFrame->IsThemed() && aForFrame->GetContent() &&
      !nsContentUtils::IsChromeDoc(aForFrame->GetContent()->GetUncomposedDoc())) {
    // There's no way of getting hold of a shape corresponding to a
    // "padding-box" for native-themed widgets, so just don't draw
    // inner box-shadows for them. But we allow chrome to paint inner
    // box shadows since chrome can be aware of the platform theme.
    return;
  }

  NS_ASSERTION(aForFrame->GetType() == nsGkAtoms::fieldSetFrame ||
               aFrameArea.Size() == aForFrame->GetSize(), "unexpected size");

  Sides skipSides = aForFrame->GetSkipSides();
  nsRect frameRect =
    ::BoxDecorationRectForBorder(aForFrame, aFrameArea, skipSides);
  nsRect paddingRect = frameRect;
  nsMargin border = aForFrame->GetUsedBorder();
  paddingRect.Deflate(border);

  // Get any border radius, since box-shadow must also have rounded corners
  // if the frame does.
  nscoord twipsRadii[8];
  nsSize sz = frameRect.Size();
  bool hasBorderRadius = aForFrame->GetBorderRadii(sz, sz, Sides(), twipsRadii);
  const nscoord twipsPerPixel = aPresContext->DevPixelsToAppUnits(1);

  RectCornerRadii innerRadii;
  if (hasBorderRadius) {
    RectCornerRadii borderRadii;

    ComputePixelRadii(twipsRadii, twipsPerPixel, &borderRadii);
    Float borderSizes[4] = {
      Float(border.top / twipsPerPixel),
      Float(border.right / twipsPerPixel),
      Float(border.bottom / twipsPerPixel),
      Float(border.left / twipsPerPixel)
    };
    nsCSSBorderRenderer::ComputeInnerRadii(borderRadii, borderSizes,
                                           &innerRadii);
  }

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

    Rect shadowPaintGfxRect = NSRectToRect(shadowPaintRect, twipsPerPixel);
    shadowPaintGfxRect.RoundOut();

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
    gfxContext* renderContext = aRenderingContext.ThebesContext();
    DrawTarget* drawTarget = renderContext->GetDrawTarget();

    // Clip the context to the area of the frame's padding rect, so no part of the
    // shadow is painted outside. Also cut out anything beyond where the inset shadow
    // will be.
    Rect shadowGfxRect = NSRectToRect(paddingRect, twipsPerPixel);
    shadowGfxRect.Round();

    // Set the shadow color; if not specified, use the foreground color
    Color shadowColor = Color::FromABGR(shadowItem->mHasColor ?
                                          shadowItem->mColor :
                                          aForFrame->StyleColor()->mColor);

    renderContext->Save();

    // This clips the outside border radius.
    // clipRectRadii is the border radius inside the inset shadow.
    if (hasBorderRadius) {
      RefPtr<Path> roundedRect =
        MakePathForRoundedRect(*drawTarget, shadowGfxRect, innerRadii);
      renderContext->Clip(roundedRect);
    } else {
      renderContext->Clip(shadowGfxRect);
    }

    nsContextBoxBlur insetBoxBlur;
    gfxRect destRect = nsLayoutUtils::RectToGfxRect(shadowPaintRect, twipsPerPixel);
    Point shadowOffset(shadowItem->mXOffset / twipsPerPixel,
                       shadowItem->mYOffset / twipsPerPixel);

    insetBoxBlur.InsetBoxBlur(renderContext, ToRect(destRect),
                              shadowClipGfxRect, shadowColor,
                              blurRadius, spreadDistanceAppUnits,
                              twipsPerPixel, hasBorderRadius,
                              clipRectRadii, ToRect(skipGfxRect),
                              shadowOffset);
    renderContext->Restore();
  }
}

/* static */
nsCSSRendering::PaintBGParams
nsCSSRendering::PaintBGParams::ForAllLayers(nsPresContext& aPresCtx,
                                            nsRenderingContext& aRenderingCtx,
                                            const nsRect& aDirtyRect,
                                            const nsRect& aBorderArea,
                                            nsIFrame *aFrame,
                                            uint32_t aPaintFlags)
{
  MOZ_ASSERT(aFrame);

  PaintBGParams result(aPresCtx, aRenderingCtx, aDirtyRect, aBorderArea, aFrame,
    aPaintFlags, -1, CompositionOp::OP_OVER);

  return result;
}

/* static */
nsCSSRendering::PaintBGParams
nsCSSRendering::PaintBGParams::ForSingleLayer(nsPresContext& aPresCtx,
                                              nsRenderingContext& aRenderingCtx,
                                              const nsRect& aDirtyRect,
                                              const nsRect& aBorderArea,
                                              nsIFrame *aFrame,
                                              uint32_t aPaintFlags,
                                              int32_t aLayer,
                                              CompositionOp aCompositionOp)
{
  MOZ_ASSERT(aFrame && (aLayer != -1));

  PaintBGParams result(aPresCtx, aRenderingCtx, aDirtyRect, aBorderArea, aFrame,
    aPaintFlags, aLayer, aCompositionOp);

  return result;
}

DrawResult
nsCSSRendering::PaintBackground(const PaintBGParams& aParams)
{
  PROFILER_LABEL("nsCSSRendering", "PaintBackground",
    js::ProfileEntry::Category::GRAPHICS);

  NS_PRECONDITION(aParams.frame,
                  "Frame is expected to be provided to PaintBackground");

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

  return PaintBackgroundWithSC(aParams, sc, *aParams.frame->StyleBorder());
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

/* static */ void
nsCSSRendering::GetImageLayerClip(const nsStyleImageLayers::Layer& aLayer,
                                  nsIFrame* aForFrame, const nsStyleBorder& aBorder,
                                  const nsRect& aBorderArea, const nsRect& aCallerDirtyRect,
                                  bool aWillPaintBorder, nscoord aAppUnitsPerPixel,
                                  /* out */ ImageLayerClipState* aClipState)
{
  // Compute the outermost boundary of the area that might be painted.
  // Same coordinate space as aBorderArea.
  Sides skipSides = aForFrame->GetSkipSides();
  nsRect clipBorderArea =
    ::BoxDecorationRectForBorder(aForFrame, aBorderArea, skipSides, &aBorder);

  bool haveRoundedCorners = GetRadii(aForFrame, aBorder, aBorderArea,
                                     clipBorderArea, aClipState->mRadii);
  uint8_t backgroundClip = aLayer.mClip;

  // XXX TODO: bug 1303623 only implements the parser of fill-box|stroke-box|view-box|no-clip.
  // So we need to fallback to default value when rendering. We should remove this
  // in bug 1311270.
  if (backgroundClip == NS_STYLE_IMAGELAYER_CLIP_FILL ||
      backgroundClip == NS_STYLE_IMAGELAYER_CLIP_STROKE ||
      backgroundClip == NS_STYLE_IMAGELAYER_CLIP_VIEW ||
      backgroundClip == NS_STYLE_IMAGELAYER_CLIP_NO_CLIP) {
    backgroundClip = NS_STYLE_IMAGELAYER_CLIP_BORDER;
  }

  bool isSolidBorder =
      aWillPaintBorder && IsOpaqueBorder(aBorder);
  if (isSolidBorder && backgroundClip == NS_STYLE_IMAGELAYER_CLIP_BORDER) {
    // If we have rounded corners, we need to inflate the background
    // drawing area a bit to avoid seams between the border and
    // background.
    backgroundClip = haveRoundedCorners ?
      NS_STYLE_IMAGELAYER_CLIP_MOZ_ALMOST_PADDING : NS_STYLE_IMAGELAYER_CLIP_PADDING;
  }

  aClipState->mBGClipArea = clipBorderArea;
  aClipState->mHasAdditionalBGClipArea = false;
  aClipState->mCustomClip = false;

  if (aForFrame->GetType() == nsGkAtoms::scrollFrame &&
      NS_STYLE_IMAGELAYER_ATTACHMENT_LOCAL == aLayer.mAttachment) {
    // As of this writing, this is still in discussion in the CSS Working Group
    // http://lists.w3.org/Archives/Public/www-style/2013Jul/0250.html

    // The rectangle for 'background-clip' scrolls with the content,
    // but the background is also clipped at a non-scrolling 'padding-box'
    // like the content. (See below.)
    // Therefore, only 'content-box' makes a difference here.
    if (backgroundClip == NS_STYLE_IMAGELAYER_CLIP_CONTENT) {
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
    backgroundClip = NS_STYLE_IMAGELAYER_CLIP_PADDING;
  }

  if (backgroundClip != NS_STYLE_IMAGELAYER_CLIP_BORDER &&
      backgroundClip != NS_STYLE_IMAGELAYER_CLIP_TEXT) {
    nsMargin border = aForFrame->GetUsedBorder();
    if (backgroundClip == NS_STYLE_IMAGELAYER_CLIP_MOZ_ALMOST_PADDING) {
      // Reduce |border| by 1px (device pixels) on all sides, if
      // possible, so that we don't get antialiasing seams between the
      // background and border.
      border.top = std::max(0, border.top - aAppUnitsPerPixel);
      border.right = std::max(0, border.right - aAppUnitsPerPixel);
      border.bottom = std::max(0, border.bottom - aAppUnitsPerPixel);
      border.left = std::max(0, border.left - aAppUnitsPerPixel);
    } else if (backgroundClip != NS_STYLE_IMAGELAYER_CLIP_PADDING) {
      NS_ASSERTION(backgroundClip == NS_STYLE_IMAGELAYER_CLIP_CONTENT,
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
    aClipState->mHasRoundedCorners = true;
  } else {
    aClipState->mHasRoundedCorners = false;
  }


  if (!haveRoundedCorners && aClipState->mHasAdditionalBGClipArea) {
    // Do the intersection here to account for the fast path(?) below.
    aClipState->mBGClipArea =
      aClipState->mBGClipArea.Intersect(aClipState->mAdditionalBGClipArea);
    aClipState->mHasAdditionalBGClipArea = false;
  }

  SetupDirtyRects(aClipState->mBGClipArea, aCallerDirtyRect, aAppUnitsPerPixel,
                  &aClipState->mDirtyRect, &aClipState->mDirtyRectGfx);
}

static void
SetupImageLayerClip(nsCSSRendering::ImageLayerClipState& aClipState,
                    gfxContext *aCtx, nscoord aAppUnitsPerPixel,
                    gfxContextAutoSaveRestore* aAutoSR)
{
  if (aClipState.mDirtyRectGfx.IsEmpty()) {
    // Our caller won't draw anything under this condition, so no need
    // to set more up.
    return;
  }

  if (aClipState.mCustomClip) {
    // We don't support custom clips and rounded corners, arguably a bug, but
    // table painting seems to depend on it.
    return;
  }

  DrawTarget* drawTarget = aCtx->GetDrawTarget();

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
    bgAreaGfx.Condition();

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
      // http://hg.mozilla.org/mozilla-central/rev/50e934e4979b landed.
      NS_WARNING("converted background area should not be empty");
      // Make our caller not do anything.
      aClipState.mDirtyRectGfx.SizeTo(gfxSize(0.0, 0.0));
      return;
    }

    aAutoSR->EnsureSaved(aCtx);

    RefPtr<Path> roundedRect =
      MakePathForRoundedRect(*drawTarget, bgAreaGfx, aClipState.mClippedRadii);
    aCtx->Clip(roundedRect);
  }
}

static void
DrawBackgroundColor(nsCSSRendering::ImageLayerClipState& aClipState,
                    gfxContext *aCtx, nscoord aAppUnitsPerPixel)
{
  if (aClipState.mDirtyRectGfx.IsEmpty()) {
    // Our caller won't draw anything under this condition, so no need
    // to set more up.
    return;
  }

  DrawTarget* drawTarget = aCtx->GetDrawTarget();

  // We don't support custom clips and rounded corners, arguably a bug, but
  // table painting seems to depend on it.
  if (!aClipState.mHasRoundedCorners || aClipState.mCustomClip) {
    aCtx->NewPath();
    aCtx->Rectangle(aClipState.mDirtyRectGfx, true);
    aCtx->Fill();
    return;
  }

  Rect bgAreaGfx = NSRectToRect(aClipState.mBGClipArea, aAppUnitsPerPixel);
  bgAreaGfx.Round();

  if (bgAreaGfx.IsEmpty()) {
    // I think it's become possible to hit this since
    // http://hg.mozilla.org/mozilla-central/rev/50e934e4979b landed.
    NS_WARNING("converted background area should not be empty");
    // Make our caller not do anything.
    aClipState.mDirtyRectGfx.SizeTo(gfxSize(0.0, 0.0));
    return;
  }

  aCtx->Save();
  gfxRect dirty = ThebesRect(bgAreaGfx).Intersect(aClipState.mDirtyRectGfx);

  aCtx->NewPath();
  aCtx->Rectangle(dirty, true);
  aCtx->Clip();

  if (aClipState.mHasAdditionalBGClipArea) {
    gfxRect bgAdditionalAreaGfx = nsLayoutUtils::RectToGfxRect(
      aClipState.mAdditionalBGClipArea, aAppUnitsPerPixel);
    bgAdditionalAreaGfx.Round();
    bgAdditionalAreaGfx.Condition();
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
    bgColor =
      aStyleContext->GetVisitedDependentColor(eCSSProperty_background_color);
    if (NS_GET_A(bgColor) == 0) {
      aDrawBackgroundColor = false;
    }
  } else {
    // If GetBackgroundColorDraw() is false, we are still expected to
    // draw color in the background of any frame that's not completely
    // transparent, but we are expected to use white instead of whatever
    // color was specified.
    bgColor = NS_RGB(255, 255, 255);
    if (aDrawBackgroundImage || !bg->IsTransparent()) {
      aDrawBackgroundColor = true;
    } else {
      bgColor = NS_RGBA(0,0,0,0);
    }
  }

  // We can skip painting the background color if a background image is opaque.
  nsStyleImageLayers::Repeat repeat = bg->BottomLayer().mRepeat;
  bool xFullRepeat = repeat.mXRepeat == NS_STYLE_IMAGELAYER_REPEAT_REPEAT ||
                     repeat.mXRepeat == NS_STYLE_IMAGELAYER_REPEAT_ROUND;
  bool yFullRepeat = repeat.mYRepeat == NS_STYLE_IMAGELAYER_REPEAT_REPEAT ||
                     repeat.mYRepeat == NS_STYLE_IMAGELAYER_REPEAT_ROUND;
  if (aDrawBackgroundColor &&
      xFullRepeat && yFullRepeat &&
      bg->BottomLayer().mImage.IsOpaque() &&
      bg->BottomLayer().mBlendMode == NS_STYLE_BLEND_NORMAL) {
    aDrawBackgroundColor = false;
  }

  return bgColor;
}

static gfxFloat
ConvertGradientValueToPixels(const nsStyleCoord& aCoord,
                             gfxFloat aFillLength,
                             int32_t aAppUnitsPerPixel)
{
  switch (aCoord.GetUnit()) {
    case eStyleUnit_Percent:
      return aCoord.GetPercentValue() * aFillLength;
    case eStyleUnit_Coord:
      return NSAppUnitsToFloatPixels(aCoord.GetCoordValue(), aAppUnitsPerPixel);
    case eStyleUnit_Calc: {
      const nsStyleCoord::Calc *calc = aCoord.GetCalcValue();
      return calc->mPercent * aFillLength +
             NSAppUnitsToFloatPixels(calc->mLength, aAppUnitsPerPixel);
    }
    default:
      NS_WARNING("Unexpected coord unit");
      return 0;
  }
}

// Given a box with size aBoxSize and origin (0,0), and an angle aAngle,
// and a starting point for the gradient line aStart, find the endpoint of
// the gradient line --- the intersection of the gradient line with a line
// perpendicular to aAngle that passes through the farthest corner in the
// direction aAngle.
static gfxPoint
ComputeGradientLineEndFromAngle(const gfxPoint& aStart,
                                double aAngle,
                                const gfxSize& aBoxSize)
{
  double dx = cos(-aAngle);
  double dy = sin(-aAngle);
  gfxPoint farthestCorner(dx > 0 ? aBoxSize.width : 0,
                          dy > 0 ? aBoxSize.height : 0);
  gfxPoint delta = farthestCorner - aStart;
  double u = delta.x*dy - delta.y*dx;
  return farthestCorner + gfxPoint(-u*dy, u*dx);
}

// Compute the start and end points of the gradient line for a linear gradient.
static void
ComputeLinearGradientLine(nsPresContext* aPresContext,
                          nsStyleGradient* aGradient,
                          const gfxSize& aBoxSize,
                          gfxPoint* aLineStart,
                          gfxPoint* aLineEnd)
{
  if (aGradient->mBgPosX.GetUnit() == eStyleUnit_None) {
    double angle;
    if (aGradient->mAngle.IsAngleValue()) {
      angle = aGradient->mAngle.GetAngleValueInRadians();
      if (!aGradient->mLegacySyntax) {
        angle = M_PI_2 - angle;
      }
    } else {
      angle = -M_PI_2; // defaults to vertical gradient starting from top
    }
    gfxPoint center(aBoxSize.width/2, aBoxSize.height/2);
    *aLineEnd = ComputeGradientLineEndFromAngle(center, angle, aBoxSize);
    *aLineStart = gfxPoint(aBoxSize.width, aBoxSize.height) - *aLineEnd;
  } else if (!aGradient->mLegacySyntax) {
    float xSign = aGradient->mBgPosX.GetPercentValue() * 2 - 1;
    float ySign = 1 - aGradient->mBgPosY.GetPercentValue() * 2;
    double angle = atan2(ySign * aBoxSize.width, xSign * aBoxSize.height);
    gfxPoint center(aBoxSize.width/2, aBoxSize.height/2);
    *aLineEnd = ComputeGradientLineEndFromAngle(center, angle, aBoxSize);
    *aLineStart = gfxPoint(aBoxSize.width, aBoxSize.height) - *aLineEnd;
  } else {
    int32_t appUnitsPerPixel = aPresContext->AppUnitsPerDevPixel();
    *aLineStart = gfxPoint(
      ConvertGradientValueToPixels(aGradient->mBgPosX, aBoxSize.width,
                                   appUnitsPerPixel),
      ConvertGradientValueToPixels(aGradient->mBgPosY, aBoxSize.height,
                                   appUnitsPerPixel));
    if (aGradient->mAngle.IsAngleValue()) {
      MOZ_ASSERT(aGradient->mLegacySyntax);
      double angle = aGradient->mAngle.GetAngleValueInRadians();
      *aLineEnd = ComputeGradientLineEndFromAngle(*aLineStart, angle, aBoxSize);
    } else {
      // No angle, the line end is just the reflection of the start point
      // through the center of the box
      *aLineEnd = gfxPoint(aBoxSize.width, aBoxSize.height) - *aLineStart;
    }
  }
}

// Compute the start and end points of the gradient line for a radial gradient.
// Also returns the horizontal and vertical radii defining the circle or
// ellipse to use.
static void
ComputeRadialGradientLine(nsPresContext* aPresContext,
                          nsStyleGradient* aGradient,
                          const gfxSize& aBoxSize,
                          gfxPoint* aLineStart,
                          gfxPoint* aLineEnd,
                          double* aRadiusX,
                          double* aRadiusY)
{
  if (aGradient->mBgPosX.GetUnit() == eStyleUnit_None) {
    // Default line start point is the center of the box
    *aLineStart = gfxPoint(aBoxSize.width/2, aBoxSize.height/2);
  } else {
    int32_t appUnitsPerPixel = aPresContext->AppUnitsPerDevPixel();
    *aLineStart = gfxPoint(
      ConvertGradientValueToPixels(aGradient->mBgPosX, aBoxSize.width,
                                   appUnitsPerPixel),
      ConvertGradientValueToPixels(aGradient->mBgPosY, aBoxSize.height,
                                   appUnitsPerPixel));
  }

  // Compute gradient shape: the x and y radii of an ellipse.
  double radiusX, radiusY;
  double leftDistance = Abs(aLineStart->x);
  double rightDistance = Abs(aBoxSize.width - aLineStart->x);
  double topDistance = Abs(aLineStart->y);
  double bottomDistance = Abs(aBoxSize.height - aLineStart->y);
  switch (aGradient->mSize) {
  case NS_STYLE_GRADIENT_SIZE_CLOSEST_SIDE:
    radiusX = std::min(leftDistance, rightDistance);
    radiusY = std::min(topDistance, bottomDistance);
    if (aGradient->mShape == NS_STYLE_GRADIENT_SHAPE_CIRCULAR) {
      radiusX = radiusY = std::min(radiusX, radiusY);
    }
    break;
  case NS_STYLE_GRADIENT_SIZE_CLOSEST_CORNER: {
    // Compute x and y distances to nearest corner
    double offsetX = std::min(leftDistance, rightDistance);
    double offsetY = std::min(topDistance, bottomDistance);
    if (aGradient->mShape == NS_STYLE_GRADIENT_SHAPE_CIRCULAR) {
      radiusX = radiusY = NS_hypot(offsetX, offsetY);
    } else {
      // maintain aspect ratio
      radiusX = offsetX*M_SQRT2;
      radiusY = offsetY*M_SQRT2;
    }
    break;
  }
  case NS_STYLE_GRADIENT_SIZE_FARTHEST_SIDE:
    radiusX = std::max(leftDistance, rightDistance);
    radiusY = std::max(topDistance, bottomDistance);
    if (aGradient->mShape == NS_STYLE_GRADIENT_SHAPE_CIRCULAR) {
      radiusX = radiusY = std::max(radiusX, radiusY);
    }
    break;
  case NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER: {
    // Compute x and y distances to nearest corner
    double offsetX = std::max(leftDistance, rightDistance);
    double offsetY = std::max(topDistance, bottomDistance);
    if (aGradient->mShape == NS_STYLE_GRADIENT_SHAPE_CIRCULAR) {
      radiusX = radiusY = NS_hypot(offsetX, offsetY);
    } else {
      // maintain aspect ratio
      radiusX = offsetX*M_SQRT2;
      radiusY = offsetY*M_SQRT2;
    }
    break;
  }
  case NS_STYLE_GRADIENT_SIZE_EXPLICIT_SIZE: {
    int32_t appUnitsPerPixel = aPresContext->AppUnitsPerDevPixel();
    radiusX = ConvertGradientValueToPixels(aGradient->mRadiusX,
                                           aBoxSize.width, appUnitsPerPixel);
    radiusY = ConvertGradientValueToPixels(aGradient->mRadiusY,
                                           aBoxSize.height, appUnitsPerPixel);
    break;
  }
  default:
    radiusX = radiusY = 0;
    MOZ_ASSERT(false, "unknown radial gradient sizing method");
  }
  *aRadiusX = radiusX;
  *aRadiusY = radiusY;

  double angle;
  if (aGradient->mAngle.IsAngleValue()) {
    angle = aGradient->mAngle.GetAngleValueInRadians();
  } else {
    // Default angle is 0deg
    angle = 0.0;
  }

  // The gradient line end point is where the gradient line intersects
  // the ellipse.
  *aLineEnd = *aLineStart + gfxPoint(radiusX*cos(-angle), radiusY*sin(-angle));
}


static float Interpolate(float aF1, float aF2, float aFrac)
{
  return aF1 + aFrac * (aF2 - aF1);
}

// Returns aFrac*aC2 + (1 - aFrac)*C1. The interpolation is done
// in unpremultiplied space, which is what SVG gradients and cairo
// gradients expect.
static Color
InterpolateColor(const Color& aC1, const Color& aC2, float aFrac)
{
  double other = 1 - aFrac;
  return Color(aC2.r*aFrac + aC1.r*other,
               aC2.g*aFrac + aC1.g*other,
               aC2.b*aFrac + aC1.b*other,
               aC2.a*aFrac + aC1.a*other);
}

static nscoord
FindTileStart(nscoord aDirtyCoord, nscoord aTilePos, nscoord aTileDim)
{
  NS_ASSERTION(aTileDim > 0, "Non-positive tile dimension");
  double multiples = floor(double(aDirtyCoord - aTilePos)/aTileDim);
  return NSToCoordRound(multiples*aTileDim + aTilePos);
}

static gfxFloat
LinearGradientStopPositionForPoint(const gfxPoint& aGradientStart,
                                   const gfxPoint& aGradientEnd,
                                   const gfxPoint& aPoint)
{
  gfxPoint d = aGradientEnd - aGradientStart;
  gfxPoint p = aPoint - aGradientStart;
  /**
   * Compute a parameter t such that a line perpendicular to the
   * d vector, passing through aGradientStart + d*t, also
   * passes through aPoint.
   *
   * t is given by
   *   (p.x - d.x*t)*d.x + (p.y - d.y*t)*d.y = 0
   *
   * Solving for t we get
   *   numerator = d.x*p.x + d.y*p.y
   *   denominator = d.x^2 + d.y^2
   *   t = numerator/denominator
   *
   * In nsCSSRendering::PaintGradient we know the length of d
   * is not zero.
   */
  double numerator = d.x * p.x + d.y * p.y;
  double denominator = d.x * d.x + d.y * d.y;
  return numerator / denominator;
}

static bool
RectIsBeyondLinearGradientEdge(const gfxRect& aRect,
                               const gfxMatrix& aPatternMatrix,
                               const nsTArray<ColorStop>& aStops,
                               const gfxPoint& aGradientStart,
                               const gfxPoint& aGradientEnd,
                               Color* aOutEdgeColor)
{
  gfxFloat topLeft = LinearGradientStopPositionForPoint(
    aGradientStart, aGradientEnd, aPatternMatrix.Transform(aRect.TopLeft()));
  gfxFloat topRight = LinearGradientStopPositionForPoint(
    aGradientStart, aGradientEnd, aPatternMatrix.Transform(aRect.TopRight()));
  gfxFloat bottomLeft = LinearGradientStopPositionForPoint(
    aGradientStart, aGradientEnd, aPatternMatrix.Transform(aRect.BottomLeft()));
  gfxFloat bottomRight = LinearGradientStopPositionForPoint(
    aGradientStart, aGradientEnd, aPatternMatrix.Transform(aRect.BottomRight()));

  const ColorStop& firstStop = aStops[0];
  if (topLeft < firstStop.mPosition && topRight < firstStop.mPosition &&
      bottomLeft < firstStop.mPosition && bottomRight < firstStop.mPosition) {
    *aOutEdgeColor = firstStop.mColor;
    return true;
  }

  const ColorStop& lastStop = aStops.LastElement();
  if (topLeft >= lastStop.mPosition && topRight >= lastStop.mPosition &&
      bottomLeft >= lastStop.mPosition && bottomRight >= lastStop.mPosition) {
    *aOutEdgeColor = lastStop.mColor;
    return true;
  }

  return false;
}

static void ResolveMidpoints(nsTArray<ColorStop>& stops)
{
  for (size_t x = 1; x < stops.Length() - 1;) {
    if (!stops[x].mIsMidpoint) {
      x++;
      continue;
    }

    Color color1 = stops[x-1].mColor;
    Color color2 = stops[x+1].mColor;
    float offset1 = stops[x-1].mPosition;
    float offset2 = stops[x+1].mPosition;
    float offset = stops[x].mPosition;
    // check if everything coincides. If so, ignore the midpoint.
    if (offset - offset1 == offset2 - offset) {
      stops.RemoveElementAt(x);
      continue;
    }

    // Check if we coincide with the left colorstop.
    if (offset1 == offset) {
      // Morph the midpoint to a regular stop with the color of the next
      // color stop.
      stops[x].mColor = color2;
      stops[x].mIsMidpoint = false;
      continue;
    }

    // Check if we coincide with the right colorstop.
    if (offset2 == offset) {
      // Morph the midpoint to a regular stop with the color of the previous
      // color stop.
      stops[x].mColor = color1;
      stops[x].mIsMidpoint = false;
      continue;
    }

    float midpoint = (offset - offset1) / (offset2 - offset1);
    ColorStop newStops[9];
    if (midpoint > .5f) {
      for (size_t y = 0; y < 7; y++) {
        newStops[y].mPosition = offset1 + (offset - offset1) * (7 + y) / 13;
      }

      newStops[7].mPosition = offset + (offset2 - offset) / 3;
      newStops[8].mPosition = offset + (offset2 - offset) * 2 / 3;
    } else {
      newStops[0].mPosition = offset1 + (offset - offset1) / 3;
      newStops[1].mPosition = offset1 + (offset - offset1) * 2 / 3;

      for (size_t y = 0; y < 7; y++) {
        newStops[y+2].mPosition = offset + (offset2 - offset) * y / 13;
      }
    }
    // calculate colors

    for (size_t y = 0; y < 9; y++) {
      // Calculate the intermediate color stops per the formula of the CSS images
      // spec. http://dev.w3.org/csswg/css-images/#color-stop-syntax
      // 9 points were chosen since it is the minimum number of stops that always
      // give the smoothest appearace regardless of midpoint position and difference
      // in luminance of the end points.
      float relativeOffset = (newStops[y].mPosition - offset1) / (offset2 - offset1);
      float multiplier = powf(relativeOffset, logf(.5f) / logf(midpoint));

      gfx::Float red = color1.r + multiplier * (color2.r - color1.r);
      gfx::Float green = color1.g + multiplier * (color2.g - color1.g);
      gfx::Float blue = color1.b + multiplier * (color2.b - color1.b);
      gfx::Float alpha = color1.a + multiplier * (color2.a - color1.a);

      newStops[y].mColor = Color(red, green, blue, alpha);
    }

    stops.ReplaceElementsAt(x, 1, newStops, 9);
    x += 9;
  }
}

static Color
Premultiply(const Color& aColor)
{
  gfx::Float a = aColor.a;
  return Color(aColor.r * a, aColor.g * a, aColor.b * a, a);
}

static Color
Unpremultiply(const Color& aColor)
{
  gfx::Float a = aColor.a;
  return (a > 0.f)
       ? Color(aColor.r / a, aColor.g / a, aColor.b / a, a)
       : aColor;
}

static Color
TransparentColor(Color aColor) {
  aColor.a = 0;
  return aColor;
}

// Adjusts and adds color stops in such a way that drawing the gradient with
// unpremultiplied interpolation looks nearly the same as if it were drawn with
// premultiplied interpolation.
static const float kAlphaIncrementPerGradientStep = 0.1f;
static void
ResolvePremultipliedAlpha(nsTArray<ColorStop>& aStops)
{
  for (size_t x = 1; x < aStops.Length(); x++) {
    const ColorStop leftStop = aStops[x - 1];
    const ColorStop rightStop = aStops[x];

    // if the left and right stop have the same alpha value, we don't need
    // to do anything
    if (leftStop.mColor.a == rightStop.mColor.a) {
      continue;
    }

    // Is the stop on the left 100% transparent? If so, have it adopt the color
    // of the right stop
    if (leftStop.mColor.a == 0) {
      aStops[x - 1].mColor = TransparentColor(rightStop.mColor);
      continue;
    }

    // Is the stop on the right completely transparent?
    // If so, duplicate it and assign it the color on the left.
    if (rightStop.mColor.a == 0) {
      ColorStop newStop = rightStop;
      newStop.mColor = TransparentColor(leftStop.mColor);
      aStops.InsertElementAt(x, newStop);
      x++;
      continue;
    }

    // Now handle cases where one or both of the stops are partially transparent.
    if (leftStop.mColor.a != 1.0f || rightStop.mColor.a != 1.0f) {
      Color premulLeftColor = Premultiply(leftStop.mColor);
      Color premulRightColor = Premultiply(rightStop.mColor);
      // Calculate how many extra steps. We do a step per 10% transparency.
      size_t stepCount = NSToIntFloor(fabsf(leftStop.mColor.a - rightStop.mColor.a) / kAlphaIncrementPerGradientStep);
      for (size_t y = 1; y < stepCount; y++) {
        float frac = static_cast<float>(y) / stepCount;
        ColorStop newStop(Interpolate(leftStop.mPosition, rightStop.mPosition, frac), false,
                          Unpremultiply(InterpolateColor(premulLeftColor, premulRightColor, frac)));
        aStops.InsertElementAt(x, newStop);
        x++;
      }
    }
  }
}

static ColorStop
InterpolateColorStop(const ColorStop& aFirst, const ColorStop& aSecond,
                     double aPosition, const Color& aDefault)
{
  MOZ_ASSERT(aFirst.mPosition <= aPosition);
  MOZ_ASSERT(aPosition <= aSecond.mPosition);

  double delta = aSecond.mPosition - aFirst.mPosition;

  if (delta < 1e-6) {
    return ColorStop(aPosition, false, aDefault);
  }

  return ColorStop(aPosition, false,
                   Unpremultiply(InterpolateColor(Premultiply(aFirst.mColor),
                                                  Premultiply(aSecond.mColor),
                                                  (aPosition - aFirst.mPosition) / delta)));
}

// Clamp and extend the given ColorStop array in-place to fit exactly into the
// range [0, 1].
static void
ClampColorStops(nsTArray<ColorStop>& aStops)
{
  MOZ_ASSERT(aStops.Length() > 0);

  // If all stops are outside the range, then get rid of everything and replace
  // with a single colour.
  if (aStops.Length() < 2 || aStops[0].mPosition > 1 ||
      aStops.LastElement().mPosition < 0) {
    Color c = aStops[0].mPosition > 1 ? aStops[0].mColor : aStops.LastElement().mColor;
    aStops.Clear();
    aStops.AppendElement(ColorStop(0, false, c));
    return;
  }

  // Create the 0 and 1 points if they fall in the range of |aStops|, and discard
  // all stops outside the range [0, 1].
  // XXX: If we have stops positioned at 0 or 1, we only keep the innermost of
  // those stops. This should be fine for the current user(s) of this function.
  for (size_t i = aStops.Length() - 1; i > 0; i--) {
    if (aStops[i - 1].mPosition < 1 && aStops[i].mPosition >= 1) {
      // Add a point to position 1.
      aStops[i] = InterpolateColorStop(aStops[i - 1], aStops[i],
                                       /* aPosition = */ 1,
                                       aStops[i - 1].mColor);
      // Remove all the elements whose position is greater than 1.
      aStops.RemoveElementsAt(i + 1, aStops.Length() - (i + 1));
    }
    if (aStops[i - 1].mPosition <= 0 && aStops[i].mPosition > 0) {
      // Add a point to position 0.
      aStops[i - 1] = InterpolateColorStop(aStops[i - 1], aStops[i],
                                           /* aPosition = */ 0,
                                           aStops[i].mColor);
      // Remove all of the preceding stops -- they are all negative.
      aStops.RemoveElementsAt(0, i - 1);
      break;
    }
  }

  MOZ_ASSERT(aStops[0].mPosition >= -1e6);
  MOZ_ASSERT(aStops.LastElement().mPosition - 1 <= 1e6);

  // The end points won't exist yet if they don't fall in the original range of
  // |aStops|. Create them if needed.
  if (aStops[0].mPosition > 0) {
    aStops.InsertElementAt(0, ColorStop(0, false, aStops[0].mColor));
  }
  if (aStops.LastElement().mPosition < 1) {
    aStops.AppendElement(ColorStop(1, false, aStops.LastElement().mColor));
  }
}

void
nsCSSRendering::PaintGradient(nsPresContext* aPresContext,
                              nsRenderingContext& aRenderingContext,
                              nsStyleGradient* aGradient,
                              const nsRect& aDirtyRect,
                              const nsRect& aDest,
                              const nsRect& aFillArea,
                              const nsSize& aRepeatSize,
                              const CSSIntRect& aSrc,
                              const nsSize& aIntrinsicSize)
{
  PROFILER_LABEL("nsCSSRendering", "PaintGradient",
    js::ProfileEntry::Category::GRAPHICS);

  Telemetry::AutoTimer<Telemetry::GRADIENT_DURATION, Telemetry::Microsecond> gradientTimer;
  if (aDest.IsEmpty() || aFillArea.IsEmpty()) {
    return;
  }

  gfxContext *ctx = aRenderingContext.ThebesContext();
  nscoord appUnitsPerDevPixel = aPresContext->AppUnitsPerDevPixel();
  gfxSize srcSize = gfxSize(gfxFloat(aIntrinsicSize.width)/appUnitsPerDevPixel,
                            gfxFloat(aIntrinsicSize.height)/appUnitsPerDevPixel);

  bool cellContainsFill = aDest.Contains(aFillArea);

  // Compute "gradient line" start and end relative to the intrinsic size of
  // the gradient.
  gfxPoint lineStart, lineEnd;
  double radiusX = 0, radiusY = 0; // for radial gradients only
  if (aGradient->mShape == NS_STYLE_GRADIENT_SHAPE_LINEAR) {
    ComputeLinearGradientLine(aPresContext, aGradient, srcSize,
                              &lineStart, &lineEnd);
  } else {
    ComputeRadialGradientLine(aPresContext, aGradient, srcSize,
                              &lineStart, &lineEnd, &radiusX, &radiusY);
  }
  // Avoid sending Infs or Nans to downwind draw targets.
  if (!lineStart.IsFinite() || !lineEnd.IsFinite()) {
    lineStart = lineEnd = gfxPoint(0, 0);
  }
  gfxFloat lineLength = NS_hypot(lineEnd.x - lineStart.x,
                                  lineEnd.y - lineStart.y);

  MOZ_ASSERT(aGradient->mStops.Length() >= 2,
             "The parser should reject gradients with less than two stops");

  // Build color stop array and compute stop positions
  nsTArray<ColorStop> stops;
  // If there is a run of stops before stop i that did not have specified
  // positions, then this is the index of the first stop in that run, otherwise
  // it's -1.
  int32_t firstUnsetPosition = -1;
  for (uint32_t i = 0; i < aGradient->mStops.Length(); ++i) {
    const nsStyleGradientStop& stop = aGradient->mStops[i];
    double position;
    switch (stop.mLocation.GetUnit()) {
    case eStyleUnit_None:
      if (i == 0) {
        // First stop defaults to position 0.0
        position = 0.0;
      } else if (i == aGradient->mStops.Length() - 1) {
        // Last stop defaults to position 1.0
        position = 1.0;
      } else {
        // Other stops with no specified position get their position assigned
        // later by interpolation, see below.
        // Remeber where the run of stops with no specified position starts,
        // if it starts here.
        if (firstUnsetPosition < 0) {
          firstUnsetPosition = i;
        }
        stops.AppendElement(ColorStop(0, stop.mIsInterpolationHint,
                                      Color::FromABGR(stop.mColor)));
        continue;
      }
      break;
    case eStyleUnit_Percent:
      position = stop.mLocation.GetPercentValue();
      break;
    case eStyleUnit_Coord:
      position = lineLength < 1e-6 ? 0.0 :
          stop.mLocation.GetCoordValue() / appUnitsPerDevPixel / lineLength;
      break;
    case eStyleUnit_Calc:
      nsStyleCoord::Calc *calc;
      calc = stop.mLocation.GetCalcValue();
      position = calc->mPercent +
          ((lineLength < 1e-6) ? 0.0 :
          (NSAppUnitsToFloatPixels(calc->mLength, appUnitsPerDevPixel) / lineLength));
      break;
    default:
      MOZ_ASSERT(false, "Unknown stop position type");
    }

    if (i > 0) {
      // Prevent decreasing stop positions by advancing this position
      // to the previous stop position, if necessary
      position = std::max(position, stops[i - 1].mPosition);
    }
    stops.AppendElement(ColorStop(position, stop.mIsInterpolationHint,
                                  Color::FromABGR(stop.mColor)));
    if (firstUnsetPosition > 0) {
      // Interpolate positions for all stops that didn't have a specified position
      double p = stops[firstUnsetPosition - 1].mPosition;
      double d = (stops[i].mPosition - p)/(i - firstUnsetPosition + 1);
      for (uint32_t j = firstUnsetPosition; j < i; ++j) {
        p += d;
        stops[j].mPosition = p;
      }
      firstUnsetPosition = -1;
    }
  }

  // If a non-repeating linear gradient is axis-aligned and there are no gaps
  // between tiles, we can optimise away most of the work by converting to a
  // repeating linear gradient and filling the whole destination rect at once.
  bool forceRepeatToCoverTiles =
    aGradient->mShape == NS_STYLE_GRADIENT_SHAPE_LINEAR &&
    (lineStart.x == lineEnd.x) != (lineStart.y == lineEnd.y) &&
    aRepeatSize.width == aDest.width && aRepeatSize.height == aDest.height &&
    !aGradient->mRepeating && !aSrc.IsEmpty() && !cellContainsFill;

  gfxMatrix matrix;
  if (forceRepeatToCoverTiles) {
    // Length of the source rectangle along the gradient axis.
    double rectLen;
    // The position of the start of the rectangle along the gradient.
    double offset;

    // The gradient line is "backwards". Flip the line upside down to make
    // things easier, and then rotate the matrix to turn everything back the
    // right way up.
    if (lineStart.x > lineEnd.x || lineStart.y > lineEnd.y) {
      std::swap(lineStart, lineEnd);
      matrix.Scale(-1, -1);
    }

    // Fit the gradient line exactly into the source rect.
    // aSrc is relative to aIntrinsincSize.
    // srcRectDev will be relative to srcSize, so in the same coordinate space
    // as lineStart / lineEnd.
    gfxRect srcRectDev = nsLayoutUtils::RectToGfxRect(
      CSSPixel::ToAppUnits(aSrc), appUnitsPerDevPixel);
    if (lineStart.x != lineEnd.x) {
      rectLen = srcRectDev.width;
      offset = (srcRectDev.x - lineStart.x) / lineLength;
      lineStart.x = srcRectDev.x;
      lineEnd.x = srcRectDev.XMost();
    } else {
      rectLen = srcRectDev.height;
      offset = (srcRectDev.y - lineStart.y) / lineLength;
      lineStart.y = srcRectDev.y;
      lineEnd.y = srcRectDev.YMost();
    }

    // Adjust gradient stop positions for the new gradient line.
    double scale = lineLength / rectLen;
    for (size_t i = 0; i < stops.Length(); i++) {
      stops[i].mPosition = (stops[i].mPosition - offset) * fabs(scale);
    }

    // Clamp or extrapolate gradient stops to exactly [0, 1].
    ClampColorStops(stops);

    lineLength = rectLen;
  }

  // Eliminate negative-position stops if the gradient is radial.
  double firstStop = stops[0].mPosition;
  if (aGradient->mShape != NS_STYLE_GRADIENT_SHAPE_LINEAR && firstStop < 0.0) {
    if (aGradient->mRepeating) {
      // Choose an instance of the repeated pattern that gives us all positive
      // stop-offsets.
      double lastStop = stops[stops.Length() - 1].mPosition;
      double stopDelta = lastStop - firstStop;
      // If all the stops are in approximately the same place then logic below
      // will kick in that makes us draw just the last stop color, so don't
      // try to do anything in that case. We certainly need to avoid
      // dividing by zero.
      if (stopDelta >= 1e-6) {
        double instanceCount = ceil(-firstStop/stopDelta);
        // Advance stops by instanceCount multiples of the period of the
        // repeating gradient.
        double offset = instanceCount*stopDelta;
        for (uint32_t i = 0; i < stops.Length(); i++) {
          stops[i].mPosition += offset;
        }
      }
    } else {
      // Move negative-position stops to position 0.0. We may also need
      // to set the color of the stop to the color the gradient should have
      // at the center of the ellipse.
      for (uint32_t i = 0; i < stops.Length(); i++) {
        double pos = stops[i].mPosition;
        if (pos < 0.0) {
          stops[i].mPosition = 0.0;
          // If this is the last stop, we don't need to adjust the color,
          // it will fill the entire area.
          if (i < stops.Length() - 1) {
            double nextPos = stops[i + 1].mPosition;
            // If nextPos is approximately equal to pos, then we don't
            // need to adjust the color of this stop because it's
            // not going to be displayed.
            // If nextPos is negative, we don't need to adjust the color of
            // this stop since it's not going to be displayed because
            // nextPos will also be moved to 0.0.
            if (nextPos >= 0.0 && nextPos - pos >= 1e-6) {
              // Compute how far the new position 0.0 is along the interval
              // between pos and nextPos.
              // XXX Color interpolation (in cairo, too) should use the
              // CSS 'color-interpolation' property!
              float frac = float((0.0 - pos)/(nextPos - pos));
              stops[i].mColor =
                InterpolateColor(stops[i].mColor, stops[i + 1].mColor, frac);
            }
          }
        }
      }
    }
    firstStop = stops[0].mPosition;
    MOZ_ASSERT(firstStop >= 0.0, "Failed to fix stop offsets");
  }

  if (aGradient->mShape != NS_STYLE_GRADIENT_SHAPE_LINEAR && !aGradient->mRepeating) {
    // Direct2D can only handle a particular class of radial gradients because
    // of the way the it specifies gradients. Setting firstStop to 0, when we
    // can, will help us stay on the fast path. Currently we don't do this
    // for repeating gradients but we could by adjusting the stop collection
    // to start at 0
    firstStop = 0;
  }

  double lastStop = stops[stops.Length() - 1].mPosition;
  // Cairo gradients must have stop positions in the range [0, 1]. So,
  // stop positions will be normalized below by subtracting firstStop and then
  // multiplying by stopScale.
  double stopScale;
  double stopOrigin = firstStop;
  double stopEnd = lastStop;
  double stopDelta = lastStop - firstStop;
  bool zeroRadius = aGradient->mShape != NS_STYLE_GRADIENT_SHAPE_LINEAR &&
                      (radiusX < 1e-6 || radiusY < 1e-6);
  if (stopDelta < 1e-6 || lineLength < 1e-6 || zeroRadius) {
    // Stops are all at the same place. Map all stops to 0.0.
    // For repeating radial gradients, or for any radial gradients with
    // a zero radius, we need to fill with the last stop color, so just set
    // both radii to 0.
    if (aGradient->mRepeating || zeroRadius) {
      radiusX = radiusY = 0.0;
    }
    stopDelta = 0.0;
    lastStop = firstStop;
  }

  // Don't normalize non-repeating or degenerate gradients below 0..1
  // This keeps the gradient line as large as the box and doesn't
  // lets us avoiding having to get padding correct for stops
  // at 0 and 1
  if (!aGradient->mRepeating || stopDelta == 0.0) {
    stopOrigin = std::min(stopOrigin, 0.0);
    stopEnd = std::max(stopEnd, 1.0);
  }
  stopScale = 1.0/(stopEnd - stopOrigin);

  // Create the gradient pattern.
  RefPtr<gfxPattern> gradientPattern;
  gfxPoint gradientStart;
  gfxPoint gradientEnd;
  if (aGradient->mShape == NS_STYLE_GRADIENT_SHAPE_LINEAR) {
    // Compute the actual gradient line ends we need to pass to cairo after
    // stops have been normalized.
    gradientStart = lineStart + (lineEnd - lineStart)*stopOrigin;
    gradientEnd = lineStart + (lineEnd - lineStart)*stopEnd;
    gfxPoint gradientStopStart = lineStart + (lineEnd - lineStart)*firstStop;
    gfxPoint gradientStopEnd = lineStart + (lineEnd - lineStart)*lastStop;

    if (stopDelta == 0.0) {
      // Stops are all at the same place. For repeating gradients, this will
      // just paint the last stop color. We don't need to do anything.
      // For non-repeating gradients, this should render as two colors, one
      // on each "side" of the gradient line segment, which is a point. All
      // our stops will be at 0.0; we just need to set the direction vector
      // correctly.
      gradientEnd = gradientStart + (lineEnd - lineStart);
      gradientStopEnd = gradientStopStart + (lineEnd - lineStart);
    }

    gradientPattern = new gfxPattern(gradientStart.x, gradientStart.y,
                                      gradientEnd.x, gradientEnd.y);
  } else {
    NS_ASSERTION(firstStop >= 0.0,
                  "Negative stops not allowed for radial gradients");

    // To form an ellipse, we'll stretch a circle vertically, if necessary.
    // So our radii are based on radiusX.
    double innerRadius = radiusX*stopOrigin;
    double outerRadius = radiusX*stopEnd;
    if (stopDelta == 0.0) {
      // Stops are all at the same place.  See above (except we now have
      // the inside vs. outside of an ellipse).
      outerRadius = innerRadius + 1;
    }
    gradientPattern = new gfxPattern(lineStart.x, lineStart.y, innerRadius,
                                     lineStart.x, lineStart.y, outerRadius);
    if (radiusX != radiusY) {
      // Stretch the circles into ellipses vertically by setting a transform
      // in the pattern.
      // Recall that this is the transform from user space to pattern space.
      // So to stretch the ellipse by factor of P vertically, we scale
      // user coordinates by 1/P.
      matrix.Translate(lineStart);
      matrix.Scale(1.0, radiusX/radiusY);
      matrix.Translate(-lineStart);
    }
  }
  // Use a pattern transform to take account of source and dest rects
  matrix.Translate(gfxPoint(aPresContext->CSSPixelsToDevPixels(aSrc.x),
                            aPresContext->CSSPixelsToDevPixels(aSrc.y)));
  matrix.Scale(gfxFloat(aPresContext->CSSPixelsToAppUnits(aSrc.width))/aDest.width,
               gfxFloat(aPresContext->CSSPixelsToAppUnits(aSrc.height))/aDest.height);
  gradientPattern->SetMatrix(matrix);

  if (gradientPattern->CairoStatus())
    return;

  if (stopDelta == 0.0) {
    // Non-repeating gradient with all stops in same place -> just add
    // first stop and last stop, both at position 0.
    // Repeating gradient with all stops in the same place, or radial
    // gradient with radius of 0 -> just paint the last stop color.
    // We use firstStop offset to keep |stops| with same units (will later normalize to 0).
    Color firstColor(stops[0].mColor);
    Color lastColor(stops.LastElement().mColor);
    stops.Clear();

    if (!aGradient->mRepeating && !zeroRadius) {
      stops.AppendElement(ColorStop(firstStop, false, firstColor));
    }
    stops.AppendElement(ColorStop(firstStop, false, lastColor));
  }

  ResolveMidpoints(stops);
  ResolvePremultipliedAlpha(stops);

  bool isRepeat = aGradient->mRepeating || forceRepeatToCoverTiles;

  // Now set normalized color stops in pattern.
  // Offscreen gradient surface cache (not a tile):
  // On some backends (e.g. D2D), the GradientStops object holds an offscreen surface
  // which is a lookup table used to evaluate the gradient. This surface can use
  // much memory (ram and/or GPU ram) and can be expensive to create. So we cache it.
  // The cache key correlates 1:1 with the arguments for CreateGradientStops (also the implied backend type)
  // Note that GradientStop is a simple struct with a stop value (while GradientStops has the surface).
  nsTArray<gfx::GradientStop> rawStops(stops.Length());
  rawStops.SetLength(stops.Length());
  for(uint32_t i = 0; i < stops.Length(); i++) {
    rawStops[i].color = stops[i].mColor;
    rawStops[i].offset = stopScale * (stops[i].mPosition - stopOrigin);
  }
  RefPtr<mozilla::gfx::GradientStops> gs =
    gfxGradientCache::GetOrCreateGradientStops(ctx->GetDrawTarget(),
                                               rawStops,
                                               isRepeat ? gfx::ExtendMode::REPEAT : gfx::ExtendMode::CLAMP);
  gradientPattern->SetColorStops(gs);

  // Paint gradient tiles. This isn't terribly efficient, but doing it this
  // way is simple and sure to get pixel-snapping right. We could speed things
  // up by drawing tiles into temporary surfaces and copying those to the
  // destination, but after pixel-snapping tiles may not all be the same size.
  nsRect dirty;
  if (!dirty.IntersectRect(aDirtyRect, aFillArea))
    return;

  gfxRect areaToFill =
    nsLayoutUtils::RectToGfxRect(aFillArea, appUnitsPerDevPixel);
  gfxRect dirtyAreaToFill = nsLayoutUtils::RectToGfxRect(dirty, appUnitsPerDevPixel);
  dirtyAreaToFill.RoundOut();

  gfxMatrix ctm = ctx->CurrentMatrix();
  bool isCTMPreservingAxisAlignedRectangles = ctm.PreservesAxisAlignedRectangles();

  // xStart/yStart are the top-left corner of the top-left tile.
  nscoord xStart = FindTileStart(dirty.x, aDest.x, aRepeatSize.width);
  nscoord yStart = FindTileStart(dirty.y, aDest.y, aRepeatSize.height);
  nscoord xEnd = forceRepeatToCoverTiles ? xStart + aDest.width : dirty.XMost();
  nscoord yEnd = forceRepeatToCoverTiles ? yStart + aDest.height : dirty.YMost();

  // x and y are the top-left corner of the tile to draw
  for (nscoord y = yStart; y < yEnd; y += aRepeatSize.height) {
    for (nscoord x = xStart; x < xEnd; x += aRepeatSize.width) {
      // The coordinates of the tile
      gfxRect tileRect = nsLayoutUtils::RectToGfxRect(
                      nsRect(x, y, aDest.width, aDest.height),
                      appUnitsPerDevPixel);
      // The actual area to fill with this tile is the intersection of this
      // tile with the overall area we're supposed to be filling
      gfxRect fillRect =
        forceRepeatToCoverTiles ? areaToFill : tileRect.Intersect(areaToFill);
      // Try snapping the fill rect. Snap its top-left and bottom-right
      // independently to preserve the orientation.
      gfxPoint snappedFillRectTopLeft = fillRect.TopLeft();
      gfxPoint snappedFillRectTopRight = fillRect.TopRight();
      gfxPoint snappedFillRectBottomRight = fillRect.BottomRight();
      // Snap three points instead of just two to ensure we choose the
      // correct orientation if there's a reflection.
      if (isCTMPreservingAxisAlignedRectangles &&
          ctx->UserToDevicePixelSnapped(snappedFillRectTopLeft, true) &&
          ctx->UserToDevicePixelSnapped(snappedFillRectBottomRight, true) &&
          ctx->UserToDevicePixelSnapped(snappedFillRectTopRight, true)) {
        if (snappedFillRectTopLeft.x == snappedFillRectBottomRight.x ||
            snappedFillRectTopLeft.y == snappedFillRectBottomRight.y) {
          // Nothing to draw; avoid scaling by zero and other weirdness that
          // could put the context in an error state.
          continue;
        }
        // Set the context's transform to the transform that maps fillRect to
        // snappedFillRect. The part of the gradient that was going to
        // exactly fill fillRect will fill snappedFillRect instead.
        gfxMatrix transform = gfxUtils::TransformRectToRect(fillRect,
            snappedFillRectTopLeft, snappedFillRectTopRight,
            snappedFillRectBottomRight);
        ctx->SetMatrix(transform);
      }
      ctx->NewPath();
      ctx->Rectangle(fillRect);

      gfxRect dirtyFillRect = fillRect.Intersect(dirtyAreaToFill);
      gfxRect fillRectRelativeToTile = dirtyFillRect - tileRect.TopLeft();
      Color edgeColor;
      if (aGradient->mShape == NS_STYLE_GRADIENT_SHAPE_LINEAR && !isRepeat &&
          RectIsBeyondLinearGradientEdge(fillRectRelativeToTile, matrix, stops,
                                         gradientStart, gradientEnd, &edgeColor)) {
        ctx->SetColor(edgeColor);
      } else {
        ctx->SetMatrix(
          ctx->CurrentMatrix().Copy().Translate(tileRect.TopLeft()));
        ctx->SetPattern(gradientPattern);
      }
      ctx->Fill();
      ctx->SetMatrix(ctm);
    }
  }
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
nsCSSRendering::PaintBackgroundWithSC(const PaintBGParams& aParams,
                                      nsStyleContext *aBackgroundSC,
                                      const nsStyleBorder& aBorder)
{
  NS_PRECONDITION(aParams.frame,
                  "Frame is expected to be provided to PaintBackground");

  // If we're drawing all layers, aCompositonOp is ignored, so make sure that
  // it was left at its default value.
  MOZ_ASSERT_IF(aParams.layer == -1,
                aParams.compositionOp == CompositionOp::OP_OVER);

  DrawResult result = DrawResult::SUCCESS;

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
      theme->DrawWidgetBackground(&aParams.renderingCtx, aParams.frame,
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

  // Compute the outermost boundary of the area that might be painted.
  // Same coordinate space as aParams.borderArea & aParams.bgClipRect.
  Sides skipSides = aParams.frame->GetSkipSides();
  nsRect paintBorderArea =
    ::BoxDecorationRectForBackground(aParams.frame, aParams.borderArea,
                                     skipSides, &aBorder);
  nsRect clipBorderArea =
    ::BoxDecorationRectForBorder(aParams.frame, aParams.borderArea,
                                 skipSides, &aBorder);

  // The 'bgClipArea' (used only by the image tiling logic, far below)
  // is the caller-provided aParams.bgClipRect if any, or else the area
  // determined by the value of 'background-clip' in
  // SetupCurrentBackgroundClip.  (Arguably it should be the
  // intersection, but that breaks the table painter -- in particular,
  // taking the intersection breaks reftests/bugs/403249-1[ab].)
  gfxContext* ctx = aParams.renderingCtx.ThebesContext();
  nscoord appUnitsPerPixel = aParams.presCtx.AppUnitsPerDevPixel();
  ImageLayerClipState clipState;
  if (aParams.bgClipRect) {
    clipState.mBGClipArea = *aParams.bgClipRect;
    clipState.mCustomClip = true;
    clipState.mHasRoundedCorners = false;
    SetupDirtyRects(clipState.mBGClipArea, aParams.dirtyRect, appUnitsPerPixel,
                    &clipState.mDirtyRect, &clipState.mDirtyRectGfx);
  } else {
    GetImageLayerClip(layers.BottomLayer(),
                      aParams.frame, aBorder, aParams.borderArea,
                      aParams.dirtyRect,
                      (aParams.paintFlags & PAINTBG_WILL_PAINT_BORDER),
                      appUnitsPerPixel,
                      &clipState);
  }

  // If we might be using a background color, go ahead and set it now.
  if (drawBackgroundColor && !isCanvasFrame)
    ctx->SetColor(Color::FromABGR(bgColor));

  // NOTE: no Save() yet, we do that later by calling autoSR.EnsureSaved(ctx)
  // in the cases we need it.
  gfxContextAutoSaveRestore autoSR;

  // If there is no background image, draw a color.  (If there is
  // neither a background image nor a color, we wouldn't have gotten
  // this far.)
  if (!drawBackgroundImage) {
    if (!isCanvasFrame) {
      DrawBackgroundColor(clipState, ctx, appUnitsPerPixel);
    }
    return DrawResult::SUCCESS;
  }

  if (layers.mImageCount < 1) {
    // Return if there are no background layers, all work from this point
    // onwards happens iteratively on these.
    return DrawResult::SUCCESS;
  }

  // Validate the layer range before we start iterating.
  int32_t startLayer = aParams.layer;
  int32_t nLayers = 1;
  if (startLayer < 0) {
    startLayer = (int32_t)layers.mImageCount - 1;
    nLayers = layers.mImageCount;
  }

  // Ensure we get invalidated for loads of the image.  We need to do
  // this here because this might be the only code that knows about the
  // association of the style data with the frame.
  if (aBackgroundSC != aParams.frame->StyleContext()) {
    NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT_WITH_RANGE(i, layers, startLayer, nLayers) {
      aParams.frame->AssociateImage(layers.mLayers[i].mImage,
                                    &aParams.presCtx);
    }
  }

  // The background color is rendered over the entire dirty area,
  // even if the image isn't.
  if (drawBackgroundColor && !isCanvasFrame) {
    DrawBackgroundColor(clipState, ctx, appUnitsPerPixel);
  }

  if (drawBackgroundImage) {
    bool clipSet = false;
    uint8_t currentBackgroundClip = NS_STYLE_IMAGELAYER_CLIP_BORDER;
    NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT_WITH_RANGE(i, layers, layers.mImageCount - 1,
                                                         nLayers + (layers.mImageCount -
                                                         startLayer - 1)) {
      const nsStyleImageLayers::Layer& layer = layers.mLayers[i];
      if (!aParams.bgClipRect) {
        if (currentBackgroundClip != layer.mClip || !clipSet) {
          currentBackgroundClip = layer.mClip;
          // If clipSet is false that means this is the bottom layer and we
          // already called GetImageLayerClip above and it stored its results
          // in clipState.
          if (clipSet) {
            autoSR.Restore(); // reset the previous one
            GetImageLayerClip(layer, aParams.frame,
                              aBorder, aParams.borderArea, aParams.dirtyRect,
                              (aParams.paintFlags & PAINTBG_WILL_PAINT_BORDER),
                              appUnitsPerPixel, &clipState);
          }
          SetupImageLayerClip(clipState, ctx, appUnitsPerPixel, &autoSR);
          clipSet = true;
          if (!clipBorderArea.IsEqualEdges(aParams.borderArea)) {
            // We're drawing the background for the joined continuation boxes
            // so we need to clip that to the slice that we want for this
            // frame.
            gfxRect clip =
              nsLayoutUtils::RectToGfxRect(aParams.borderArea, appUnitsPerPixel);
            autoSR.EnsureSaved(ctx);
            ctx->NewPath();
            ctx->SnappedRectangle(clip);
            ctx->Clip();
          }
        }
      }
      if ((aParams.layer < 0 || i == (uint32_t)startLayer) &&
          !clipState.mDirtyRectGfx.IsEmpty()) {
        CompositionOp co = DetermineCompositionOp(aParams, layers, i);
        nsBackgroundLayerState state =
          PrepareImageLayer(&aParams.presCtx, aParams.frame,
                            aParams.paintFlags, paintBorderArea, clipState.mBGClipArea,
                            layer, nullptr);
        result &= state.mImageRenderer.PrepareResult();
        if (!state.mFillArea.IsEmpty()) {
          if (co != CompositionOp::OP_OVER) {
            NS_ASSERTION(ctx->CurrentOp() == CompositionOp::OP_OVER,
                         "It is assumed the initial op is OP_OVER, when it is "
                         "restored later");
            ctx->SetOp(co);
          }

          result &=
            state.mImageRenderer.DrawBackground(&aParams.presCtx,
                                                aParams.renderingCtx,
                                                state.mDestArea, state.mFillArea,
                                                state.mAnchor + paintBorderArea.TopLeft(),
                                                clipState.mDirtyRect,
                                                state.mRepeatSize);

          if (co != CompositionOp::OP_OVER) {
            ctx->SetOp(CompositionOp::OP_OVER);
          }
        }
      }
    }
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
  // Compute background origin area relative to aBorderArea now as we may need
  // it to compute the effective image size for a CSS gradient.
  nsRect bgPositioningArea;

  uint8_t backgroundOrigin = aLayer.mOrigin;

  // XXX TODO: bug 1303623 only implements the parser of fill-box|stroke-box|view-box|no-clip.
  // So we need to fallback to default value when rendering. We should remove this
  // in bug 1311270.
  if (backgroundOrigin == NS_STYLE_IMAGELAYER_ORIGIN_FILL ||
      backgroundOrigin == NS_STYLE_IMAGELAYER_ORIGIN_STROKE ||
      backgroundOrigin == NS_STYLE_IMAGELAYER_ORIGIN_VIEW) {
    backgroundOrigin = NS_STYLE_IMAGELAYER_ORIGIN_BORDER;
  }

  nsIAtom* frameType = aForFrame->GetType();
  nsIFrame* geometryFrame = aForFrame;
  if (MOZ_UNLIKELY(frameType == nsGkAtoms::scrollFrame &&
                   NS_STYLE_IMAGELAYER_ATTACHMENT_LOCAL == aLayer.mAttachment)) {
    nsIScrollableFrame* scrollableFrame = do_QueryFrame(aForFrame);
    bgPositioningArea = nsRect(
      scrollableFrame->GetScrolledFrame()->GetPosition()
        // For the dir=rtl case:
        + scrollableFrame->GetScrollRange().TopLeft(),
      scrollableFrame->GetScrolledRect().Size());
    // The ScrolledRect’s size does not include the borders or scrollbars,
    // reverse the handling of background-origin
    // compared to the common case below.
    if (backgroundOrigin == NS_STYLE_IMAGELAYER_ORIGIN_BORDER) {
      nsMargin border = geometryFrame->GetUsedBorder();
      border.ApplySkipSides(geometryFrame->GetSkipSides());
      bgPositioningArea.Inflate(border);
      bgPositioningArea.Inflate(scrollableFrame->GetActualScrollbarSizes());
    } else if (backgroundOrigin != NS_STYLE_IMAGELAYER_ORIGIN_PADDING) {
      nsMargin padding = geometryFrame->GetUsedPadding();
      padding.ApplySkipSides(geometryFrame->GetSkipSides());
      bgPositioningArea.Deflate(padding);
      NS_ASSERTION(backgroundOrigin == NS_STYLE_IMAGELAYER_ORIGIN_CONTENT,
                   "unknown background-origin value");
    }
    *aAttachedToFrame = aForFrame;
    return bgPositioningArea;
  }

  if (MOZ_UNLIKELY(frameType == nsGkAtoms::canvasFrame)) {
    geometryFrame = aForFrame->PrincipalChildList().FirstChild();
    // geometryFrame might be null if this canvas is a page created
    // as an overflow container (e.g. the in-flow content has already
    // finished and this page only displays the continuations of
    // absolutely positioned content).
    if (geometryFrame) {
      bgPositioningArea = geometryFrame->GetRect();
    }
  } else {
    bgPositioningArea = nsRect(nsPoint(0,0), aBorderArea.Size());
  }

  // Background images are tiled over the 'background-clip' area
  // but the origin of the tiling is based on the 'background-origin' area
  // XXX: Bug 1303623 will bring in new origin value, we should iterate from
  // NS_STYLE_IMAGELAYER_ORIGIN_MARGIN instead of
  // NS_STYLE_IMAGELAYER_ORIGIN_BORDER.
  if (aLayer.mOrigin != NS_STYLE_IMAGELAYER_ORIGIN_BORDER && geometryFrame) {
    nsMargin border = geometryFrame->GetUsedBorder();
    if (backgroundOrigin != NS_STYLE_IMAGELAYER_ORIGIN_PADDING) {
      border += geometryFrame->GetUsedPadding();
      NS_ASSERTION(backgroundOrigin == NS_STYLE_IMAGELAYER_ORIGIN_CONTENT,
                   "unknown background-origin value");
    }
    bgPositioningArea.Deflate(border);
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
      pageContentFrame =
        nsLayoutUtils::GetClosestFrameOfType(aForFrame, nsGkAtoms::pageContentFrame);
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
      bgPositioningArea =
        nsRect(-aForFrame->GetOffsetTo(attachedToFrame), attachedToFrame->GetSize());

      if (!pageContentFrame) {
        // Subtract the size of scrollbars.
        nsIScrollableFrame* scrollableFrame =
          aPresContext->PresShell()->GetRootScrollFrameAsScrollable();
        if (scrollableFrame) {
          nsMargin scrollbars = scrollableFrame->GetActualScrollbarSizes();
          bgPositioningArea.Deflate(scrollbars);
        }
      }
    }
  }
  *aAttachedToFrame = attachedToFrame;

  return bgPositioningArea;
}

// Implementation of the formula for computation of background-repeat round
// See http://dev.w3.org/csswg/css3-background/#the-background-size
// This function returns the adjusted size of the background image.
static nscoord
ComputeRoundedSize(nscoord aCurrentSize, nscoord aPositioningSize)
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
                              uint8_t aXRepeat, uint8_t aYRepeat)
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
  bool isRepeatRoundInBothDimensions = aXRepeat == NS_STYLE_IMAGELAYER_REPEAT_ROUND &&
                                       aYRepeat == NS_STYLE_IMAGELAYER_REPEAT_ROUND;

  // Calculate the rounded size only if the background-size computation
  // returned a correct size for the image.
  if (imageSize.width && aXRepeat == NS_STYLE_IMAGELAYER_REPEAT_ROUND) {
    imageSize.width = ComputeRoundedSize(imageSize.width, aBgPositioningArea.width);
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
  if (imageSize.height && aYRepeat == NS_STYLE_IMAGELAYER_REPEAT_ROUND) {
    imageSize.height = ComputeRoundedSize(imageSize.height, aBgPositioningArea.height);
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

/* ComputeBorderSpacedRepeatSize
 * aImageDimension: the image width/height
 * aAvailableSpace: the background positioning area width/height
 * aSpace: the space between each image
 * Returns the image size plus gap size of app units for use as spacing
 */
static nscoord
ComputeBorderSpacedRepeatSize(nscoord aImageDimension,
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
  nsRect bgPositioningArea =
    ComputeImageLayerPositioningArea(aPresContext, aForFrame, aBorderArea,
                                     aLayer, &attachedToFrame, &transformedFixed);
  if (aOutIsTransformedFixed) {
    *aOutIsTransformedFixed = transformedFixed;
  }

  // For background-attachment:fixed backgrounds, we'll limit the area
  // where the background can be drawn to the viewport.
  nsRect bgClipRect = aBGClipRect;

  // Compute the anchor point.
  //
  // relative to aBorderArea.TopLeft() (which is where the top-left
  // of aForFrame's border-box will be rendered)
  nsPoint imageTopLeft;
  if (NS_STYLE_IMAGELAYER_ATTACHMENT_FIXED == aLayer.mAttachment && !transformedFixed) {
    if (aFlags & nsCSSRendering::PAINTBG_TO_WINDOW) {
      // Clip background-attachment:fixed backgrounds to the viewport, if we're
      // painting to the screen and not transformed. This avoids triggering
      // tiling in common cases, without affecting output since drawing is
      // always clipped to the viewport when we draw to the screen. (But it's
      // not a pure optimization since it can affect the values of pixels at the
      // edge of the viewport --- whether they're sampled from a putative "next
      // tile" or not.)
      bgClipRect.IntersectRect(bgClipRect, bgPositioningArea + aBorderArea.TopLeft());
    }
  }

  int repeatX = aLayer.mRepeat.mXRepeat;
  int repeatY = aLayer.mRepeat.mYRepeat;

  // Scale the image as specified for background-size and background-repeat.
  // Also as required for proper background positioning when background-position
  // is defined with percentages.
  CSSSizeOrRatio intrinsicSize = state.mImageRenderer.ComputeIntrinsicSize();
  nsSize bgPositionSize = bgPositioningArea.Size();
  nsSize imageSize = ComputeDrawnSizeForBackground(intrinsicSize,
                                                   bgPositionSize,
                                                   aLayer.mSize,
                                                   repeatX,
                                                   repeatY);

  if (imageSize.width <= 0 || imageSize.height <= 0)
    return state;

  state.mImageRenderer.SetPreferredSize(intrinsicSize,
                                        imageSize);

  // Compute the position of the background now that the background's size is
  // determined.
  nsImageRenderer::ComputeObjectAnchorPoint(aLayer.mPosition,
                                            bgPositionSize, imageSize,
                                            &imageTopLeft, &state.mAnchor);
  state.mRepeatSize = imageSize;
  if (repeatX == NS_STYLE_IMAGELAYER_REPEAT_SPACE) {
    bool isRepeat;
    state.mRepeatSize.width = ComputeSpacedRepeatSize(imageSize.width,
                                                      bgPositionSize.width,
                                                      isRepeat);
    if (isRepeat) {
      imageTopLeft.x = 0;
      state.mAnchor.x = 0;
    } else {
      repeatX = NS_STYLE_IMAGELAYER_REPEAT_NO_REPEAT;
    }
  }

  if (repeatY == NS_STYLE_IMAGELAYER_REPEAT_SPACE) {
    bool isRepeat;
    state.mRepeatSize.height = ComputeSpacedRepeatSize(imageSize.height,
                                                       bgPositionSize.height,
                                                       isRepeat);
    if (isRepeat) {
      imageTopLeft.y = 0;
      state.mAnchor.y = 0;
    } else {
      repeatY = NS_STYLE_IMAGELAYER_REPEAT_NO_REPEAT;
    }
  }

  imageTopLeft += bgPositioningArea.TopLeft();
  state.mAnchor += bgPositioningArea.TopLeft();
  state.mDestArea = nsRect(imageTopLeft + aBorderArea.TopLeft(), imageSize);
  state.mFillArea = state.mDestArea;

  ExtendMode repeatMode = ExtendMode::CLAMP;
  if (repeatX == NS_STYLE_IMAGELAYER_REPEAT_REPEAT ||
      repeatX == NS_STYLE_IMAGELAYER_REPEAT_ROUND ||
      repeatX == NS_STYLE_IMAGELAYER_REPEAT_SPACE) {
    state.mFillArea.x = bgClipRect.x;
    state.mFillArea.width = bgClipRect.width;
    repeatMode = ExtendMode::REPEAT_X;
  }
  if (repeatY == NS_STYLE_IMAGELAYER_REPEAT_REPEAT ||
      repeatY == NS_STYLE_IMAGELAYER_REPEAT_ROUND ||
      repeatY == NS_STYLE_IMAGELAYER_REPEAT_SPACE) {
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

static DrawResult
DrawBorderImage(nsPresContext*       aPresContext,
                nsRenderingContext&  aRenderingContext,
                nsIFrame*            aForFrame,
                const nsRect&        aBorderArea,
                const nsStyleBorder& aStyleBorder,
                const nsRect&        aDirtyRect,
                Sides                aSkipSides,
                PaintBorderFlags     aFlags)
{
  NS_PRECONDITION(aStyleBorder.IsBorderImageLoaded(),
                  "drawing border image that isn't successfully loaded");

  if (aDirtyRect.IsEmpty()) {
    return DrawResult::SUCCESS;
  }

  uint32_t irFlags = 0;
  if (aFlags & PaintBorderFlags::SYNC_DECODE_IMAGES) {
    irFlags |= nsImageRenderer::FLAG_SYNC_DECODE_IMAGES;
  }
  nsImageRenderer renderer(aForFrame, &aStyleBorder.mBorderImageSource, irFlags);

  // Ensure we get invalidated for loads and animations of the image.
  // We need to do this here because this might be the only code that
  // knows about the association of the style data with the frame.
  // XXX We shouldn't really... since if anybody is passing in a
  // different style, they'll potentially have the wrong size for the
  // border too.
  aForFrame->AssociateImage(aStyleBorder.mBorderImageSource, aPresContext);

  if (!renderer.PrepareImage()) {
    return renderer.PrepareResult();
  }

  // NOTE: no Save() yet, we do that later by calling autoSR.EnsureSaved()
  // in case we need it.
  gfxContextAutoSaveRestore autoSR;

  // Determine the border image area, which by default corresponds to the
  // border box but can be modified by 'border-image-outset'.
  // Note that 'border-radius' do not apply to 'border-image' borders per
  // <http://dev.w3.org/csswg/css-backgrounds/#corner-clipping>.
  nsRect borderImgArea;
  nsMargin borderWidths(aStyleBorder.GetComputedBorder());
  nsMargin imageOutset(aStyleBorder.GetImageOutset());
  if (::IsBoxDecorationSlice(aStyleBorder) && !aSkipSides.IsEmpty()) {
    borderImgArea = ::BoxDecorationRectForBorder(aForFrame, aBorderArea,
                                                 aSkipSides, &aStyleBorder);
    if (borderImgArea.IsEqualEdges(aBorderArea)) {
      // No need for a clip, just skip the sides we don't want.
      borderWidths.ApplySkipSides(aSkipSides);
      imageOutset.ApplySkipSides(aSkipSides);
      borderImgArea.Inflate(imageOutset);
    } else {
      // We're drawing borders around the joined continuation boxes so we need
      // to clip that to the slice that we want for this frame.
      borderImgArea.Inflate(imageOutset);
      imageOutset.ApplySkipSides(aSkipSides);
      nsRect clip = aBorderArea;
      clip.Inflate(imageOutset);
      autoSR.EnsureSaved(aRenderingContext.ThebesContext());
      aRenderingContext.ThebesContext()->
        Clip(NSRectToSnappedRect(clip,
                                 aForFrame->PresContext()->AppUnitsPerDevPixel(),
                                 *aRenderingContext.GetDrawTarget()));
    }
  } else {
    borderImgArea = aBorderArea;
    borderImgArea.Inflate(imageOutset);
  }

  // Calculate the image size used to compute slice points.
  CSSSizeOrRatio intrinsicSize = renderer.ComputeIntrinsicSize();
  nsSize imageSize = nsImageRenderer::ComputeConcreteSize(CSSSizeOrRatio(),
                                                          intrinsicSize,
                                                          borderImgArea.Size());
  renderer.SetPreferredSize(intrinsicSize, imageSize);

  // Compute the used values of 'border-image-slice' and 'border-image-width';
  // we do them together because the latter can depend on the former.
  nsMargin slice;
  nsMargin border;
  NS_FOR_CSS_SIDES(s) {
    nsStyleCoord coord = aStyleBorder.mBorderImageSlice.Get(s);
    int32_t imgDimension = NS_SIDE_IS_VERTICAL(s)
                           ? imageSize.width : imageSize.height;
    nscoord borderDimension = NS_SIDE_IS_VERTICAL(s)
                           ? borderImgArea.width : borderImgArea.height;
    double value;
    switch (coord.GetUnit()) {
      case eStyleUnit_Percent:
        value = coord.GetPercentValue() * imgDimension;
        break;
      case eStyleUnit_Factor:
        value = nsPresContext::CSSPixelsToAppUnits(
          NS_lround(coord.GetFactorValue()));
        break;
      default:
        NS_NOTREACHED("unexpected CSS unit for image slice");
        value = 0;
        break;
    }
    if (value < 0)
      value = 0;
    if (value > imgDimension)
      value = imgDimension;
    slice.Side(s) = value;

    coord = aStyleBorder.mBorderImageWidth.Get(s);
    switch (coord.GetUnit()) {
      case eStyleUnit_Coord: // absolute dimension
        value = coord.GetCoordValue();
        break;
      case eStyleUnit_Percent:
        value = coord.GetPercentValue() * borderDimension;
        break;
      case eStyleUnit_Factor:
        value = coord.GetFactorValue() * borderWidths.Side(s);
        break;
      case eStyleUnit_Auto:  // same as the slice value, in CSS pixels
        value = slice.Side(s);
        break;
      default:
        NS_NOTREACHED("unexpected CSS unit for border image area division");
        value = 0;
        break;
    }
    // NSToCoordRoundWithClamp rounds towards infinity, but that's OK
    // because we expect value to be non-negative.
    MOZ_ASSERT(value >= 0);
    border.Side(s) = NSToCoordRoundWithClamp(value);
    MOZ_ASSERT(border.Side(s) >= 0);
  }

  // "If two opposite border-image-width offsets are large enough that they
  // overlap, their used values are proportionately reduced until they no
  // longer overlap."
  uint32_t combinedBorderWidth = uint32_t(border.left) +
                                 uint32_t(border.right);
  double scaleX = combinedBorderWidth > uint32_t(borderImgArea.width)
                  ? borderImgArea.width / double(combinedBorderWidth)
                  : 1.0;
  uint32_t combinedBorderHeight = uint32_t(border.top) +
                                  uint32_t(border.bottom);
  double scaleY = combinedBorderHeight > uint32_t(borderImgArea.height)
                  ? borderImgArea.height / double(combinedBorderHeight)
                  : 1.0;
  double scale = std::min(scaleX, scaleY);
  if (scale < 1.0) {
    border.left *= scale;
    border.right *= scale;
    border.top *= scale;
    border.bottom *= scale;
    NS_ASSERTION(border.left + border.right <= borderImgArea.width &&
                 border.top + border.bottom <= borderImgArea.height,
                 "rounding error in width reduction???");
  }

  // These helper tables recharacterize the 'slice' and 'width' margins
  // in a more convenient form: they are the x/y/width/height coords
  // required for various bands of the border, and they have been transformed
  // to be relative to the innerRect (for 'slice') or the page (for 'border').
  enum {
    LEFT, MIDDLE, RIGHT,
    TOP = LEFT, BOTTOM = RIGHT
  };
  const nscoord borderX[3] = {
    borderImgArea.x + 0,
    borderImgArea.x + border.left,
    borderImgArea.x + borderImgArea.width - border.right,
  };
  const nscoord borderY[3] = {
    borderImgArea.y + 0,
    borderImgArea.y + border.top,
    borderImgArea.y + borderImgArea.height - border.bottom,
  };
  const nscoord borderWidth[3] = {
    border.left,
    borderImgArea.width - border.left - border.right,
    border.right,
  };
  const nscoord borderHeight[3] = {
    border.top,
    borderImgArea.height - border.top - border.bottom,
    border.bottom,
  };
  const int32_t sliceX[3] = {
    0,
    slice.left,
    imageSize.width - slice.right,
  };
  const int32_t sliceY[3] = {
    0,
    slice.top,
    imageSize.height - slice.bottom,
  };
  const int32_t sliceWidth[3] = {
    slice.left,
    std::max(imageSize.width - slice.left - slice.right, 0),
    slice.right,
  };
  const int32_t sliceHeight[3] = {
    slice.top,
    std::max(imageSize.height - slice.top - slice.bottom, 0),
    slice.bottom,
  };

  DrawResult result = DrawResult::SUCCESS;

  // intrinsicSize.CanComputeConcreteSize() return false means we can not
  // read intrinsic size from aStyleBorder.mBorderImageSource.
  // In this condition, we pass imageSize(a resolved size comes from
  // default sizing algorithm) to renderer as the viewport size.
  Maybe<nsSize> svgViewportSize = intrinsicSize.CanComputeConcreteSize() ?
    Nothing() : Some(imageSize);
  bool hasIntrinsicRatio = intrinsicSize.HasRatio();
  renderer.PurgeCacheForViewportChange(svgViewportSize, hasIntrinsicRatio);

  for (int i = LEFT; i <= RIGHT; i++) {
    for (int j = TOP; j <= BOTTOM; j++) {
      uint8_t fillStyleH, fillStyleV;
      nsSize unitSize;

      if (i == MIDDLE && j == MIDDLE) {
        // Discard the middle portion unless set to fill.
        if (NS_STYLE_BORDER_IMAGE_SLICE_NOFILL ==
            aStyleBorder.mBorderImageFill) {
          continue;
        }

        NS_ASSERTION(NS_STYLE_BORDER_IMAGE_SLICE_FILL ==
                     aStyleBorder.mBorderImageFill,
                     "Unexpected border image fill");

        // css-background:
        //     The middle image's width is scaled by the same factor as the
        //     top image unless that factor is zero or infinity, in which
        //     case the scaling factor of the bottom is substituted, and
        //     failing that, the width is not scaled. The height of the
        //     middle image is scaled by the same factor as the left image
        //     unless that factor is zero or infinity, in which case the
        //     scaling factor of the right image is substituted, and failing
        //     that, the height is not scaled.
        gfxFloat hFactor, vFactor;

        if (0 < border.left && 0 < slice.left)
          vFactor = gfxFloat(border.left)/slice.left;
        else if (0 < border.right && 0 < slice.right)
          vFactor = gfxFloat(border.right)/slice.right;
        else
          vFactor = 1;

        if (0 < border.top && 0 < slice.top)
          hFactor = gfxFloat(border.top)/slice.top;
        else if (0 < border.bottom && 0 < slice.bottom)
          hFactor = gfxFloat(border.bottom)/slice.bottom;
        else
          hFactor = 1;

        unitSize.width = sliceWidth[i]*hFactor;
        unitSize.height = sliceHeight[j]*vFactor;
        fillStyleH = aStyleBorder.mBorderImageRepeatH;
        fillStyleV = aStyleBorder.mBorderImageRepeatV;

      } else if (i == MIDDLE) { // top, bottom
        // Sides are always stretched to the thickness of their border,
        // and stretched proportionately on the other axis.
        gfxFloat factor;
        if (0 < borderHeight[j] && 0 < sliceHeight[j])
          factor = gfxFloat(borderHeight[j])/sliceHeight[j];
        else
          factor = 1;

        unitSize.width = sliceWidth[i]*factor;
        unitSize.height = borderHeight[j];
        fillStyleH = aStyleBorder.mBorderImageRepeatH;
        fillStyleV = NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH;

      } else if (j == MIDDLE) { // left, right
        gfxFloat factor;
        if (0 < borderWidth[i] && 0 < sliceWidth[i])
          factor = gfxFloat(borderWidth[i])/sliceWidth[i];
        else
          factor = 1;

        unitSize.width = borderWidth[i];
        unitSize.height = sliceHeight[j]*factor;
        fillStyleH = NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH;
        fillStyleV = aStyleBorder.mBorderImageRepeatV;

      } else {
        // Corners are always stretched to fit the corner.
        unitSize.width = borderWidth[i];
        unitSize.height = borderHeight[j];
        fillStyleH = NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH;
        fillStyleV = NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH;
      }

      nsRect destArea(borderX[i], borderY[j], borderWidth[i], borderHeight[j]);
      nsRect subArea(sliceX[i], sliceY[j], sliceWidth[i], sliceHeight[j]);
      if (subArea.IsEmpty())
        continue;

      nsIntRect intSubArea = subArea.ToOutsidePixels(nsPresContext::AppUnitsPerCSSPixel());
      result &=
        renderer.DrawBorderImageComponent(aPresContext,
                                          aRenderingContext, aDirtyRect,
                                          destArea, CSSIntRect(intSubArea.x,
                                                               intSubArea.y,
                                                               intSubArea.width,
                                                               intSubArea.height),
                                          fillStyleH, fillStyleV,
                                          unitSize, j * (RIGHT + 1) + i,
                                          svgViewportSize, hasIntrinsicRatio);
    }
  }

  return result;
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
nsCSSRendering::DrawTableBorderSegment(DrawTarget&              aDrawTarget,
                                       uint8_t                  aBorderStyle,
                                       nscolor                  aBorderColor,
                                       const nsStyleBackground* aBGColor,
                                       const nsRect&            aBorder,
                                       int32_t                  aAppUnitsPerDevPixel,
                                       int32_t                  aAppUnitsPerCSSPixel,
                                       uint8_t                  aStartBevelSide,
                                       nscoord                  aStartBevelOffset,
                                       uint8_t                  aEndBevelSide,
                                       nscoord                  aEndBevelOffset)
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
                                          aBGColor->mBackgroundColor,
                                          aBorderColor);
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
                                  aBGColor->mBackgroundColor, aBorderColor);
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

// ------------------
// ImageRenderer
// ------------------
nsImageRenderer::nsImageRenderer(nsIFrame* aForFrame,
                                 const nsStyleImage* aImage,
                                 uint32_t aFlags)
  : mForFrame(aForFrame)
  , mImage(aImage)
  , mType(aImage->GetType())
  , mImageContainer(nullptr)
  , mGradientData(nullptr)
  , mPaintServerFrame(nullptr)
  , mPrepareResult(DrawResult::NOT_READY)
  , mSize(0, 0)
  , mFlags(aFlags)
  , mExtendMode(ExtendMode::CLAMP)
  , mMaskOp(NS_STYLE_MASK_MODE_MATCH_SOURCE)
{
}

nsImageRenderer::~nsImageRenderer()
{
}

static bool
ShouldTreatAsCompleteDueToSyncDecode(const nsStyleImage* aImage,
                                     uint32_t aFlags)
{
  if (!(aFlags & nsImageRenderer::FLAG_SYNC_DECODE_IMAGES)) {
    return false;
  }

  if (aImage->GetType() != eStyleImageType_Image) {
    return false;
  }

  imgRequestProxy* req = aImage->GetImageData();
  if (!req) {
    return false;
  }

  uint32_t status = 0;
  if (NS_FAILED(req->GetImageStatus(&status))) {
    return false;
  }

  if (status & imgIRequest::STATUS_ERROR) {
    // The image is "complete" since it's a corrupt image. If we created an
    // imgIContainer at all, return true.
    nsCOMPtr<imgIContainer> image;
    req->GetImage(getter_AddRefs(image));
    return bool(image);
  }

  if (!(status & imgIRequest::STATUS_LOAD_COMPLETE)) {
    // We must have loaded all of the image's data and the size must be
    // available, or else sync decoding won't be able to decode the image.
    return false;
  }

  return true;
}

bool
nsImageRenderer::PrepareImage()
{
  if (mImage->IsEmpty()) {
    mPrepareResult = DrawResult::BAD_IMAGE;
    return false;
  }

  if (!mImage->IsComplete()) {
    // Make sure the image is actually decoding.
    mImage->StartDecoding();

    // Check again to see if we finished.
    // We cannot prepare the image for rendering if it is not fully loaded.
    // Special case: If we requested a sync decode and the image has loaded, push
    // on through because the Draw() will do a sync decode then.
    if (!mImage->IsComplete() &&
        !ShouldTreatAsCompleteDueToSyncDecode(mImage, mFlags)) {
      mPrepareResult = DrawResult::NOT_READY;
      return false;
    }
  }

  switch (mType) {
    case eStyleImageType_Image: {
      MOZ_ASSERT(mImage->GetImageData(),
                 "must have image data, since we checked IsEmpty above");
      nsCOMPtr<imgIContainer> srcImage;
      DebugOnly<nsresult> rv =
        mImage->GetImageData()->GetImage(getter_AddRefs(srcImage));
      MOZ_ASSERT(NS_SUCCEEDED(rv) && srcImage,
                 "If GetImage() is failing, mImage->IsComplete() "
                 "should have returned false");

      if (!mImage->GetCropRect()) {
        mImageContainer.swap(srcImage);
      } else {
        nsIntRect actualCropRect;
        bool isEntireImage;
        bool success =
          mImage->ComputeActualCropRect(actualCropRect, &isEntireImage);
        NS_ASSERTION(success, "ComputeActualCropRect() should not fail here");
        if (!success || actualCropRect.IsEmpty()) {
          // The cropped image has zero size
          mPrepareResult = DrawResult::BAD_IMAGE;
          return false;
        }
        if (isEntireImage) {
          // The cropped image is identical to the source image
          mImageContainer.swap(srcImage);
        } else {
          nsCOMPtr<imgIContainer> subImage = ImageOps::Clip(srcImage,
                                                            actualCropRect,
                                                            Nothing());
          mImageContainer.swap(subImage);
        }
      }
      mPrepareResult = DrawResult::SUCCESS;
      break;
    }
    case eStyleImageType_Gradient:
      mGradientData = mImage->GetGradientData();
      mPrepareResult = DrawResult::SUCCESS;
      break;
    case eStyleImageType_Element:
    {
      nsAutoString elementId =
        NS_LITERAL_STRING("#") + nsDependentString(mImage->GetElementId());
      nsCOMPtr<nsIURI> targetURI;
      nsCOMPtr<nsIURI> base = mForFrame->GetContent()->GetBaseURI();
      nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), elementId,
                                                mForFrame->GetContent()->GetUncomposedDoc(), base);
      nsSVGPaintingProperty* property = nsSVGEffects::GetPaintingPropertyForURI(
          targetURI, mForFrame->FirstContinuation(),
          nsSVGEffects::BackgroundImageProperty());
      if (!property) {
        mPrepareResult = DrawResult::BAD_IMAGE;
        return false;
      }

      // If the referenced element is an <img>, <canvas>, or <video> element,
      // prefer SurfaceFromElement as it's more reliable.
      mImageElementSurface =
        nsLayoutUtils::SurfaceFromElement(property->GetReferencedElement());
      if (!mImageElementSurface.GetSourceSurface()) {
        mPaintServerFrame = property->GetReferencedFrame();
        if (!mPaintServerFrame) {
          mPrepareResult = DrawResult::BAD_IMAGE;
          return false;
        }
      }

      mPrepareResult = DrawResult::SUCCESS;
      break;
    }
    case eStyleImageType_Null:
    default:
      break;
  }

  return IsReady();
}

nsSize
CSSSizeOrRatio::ComputeConcreteSize() const
{
  NS_ASSERTION(CanComputeConcreteSize(), "Cannot compute");
  if (mHasWidth && mHasHeight) {
    return nsSize(mWidth, mHeight);
  }
  if (mHasWidth) {
    nscoord height = NSCoordSaturatingNonnegativeMultiply(
      mWidth,
      double(mRatio.height) / mRatio.width);
    return nsSize(mWidth, height);
  }

  MOZ_ASSERT(mHasHeight);
  nscoord width = NSCoordSaturatingNonnegativeMultiply(
    mHeight,
    double(mRatio.width) / mRatio.height);
  return nsSize(width, mHeight);
}

CSSSizeOrRatio
nsImageRenderer::ComputeIntrinsicSize()
{
  NS_ASSERTION(IsReady(), "Ensure PrepareImage() has returned true "
                          "before calling me");

  CSSSizeOrRatio result;
  switch (mType) {
    case eStyleImageType_Image:
    {
      bool haveWidth, haveHeight;
      CSSIntSize imageIntSize;
      nsLayoutUtils::ComputeSizeForDrawing(mImageContainer, imageIntSize,
                                           result.mRatio, haveWidth, haveHeight);
      if (haveWidth) {
        result.SetWidth(nsPresContext::CSSPixelsToAppUnits(imageIntSize.width));
      }
      if (haveHeight) {
        result.SetHeight(nsPresContext::CSSPixelsToAppUnits(imageIntSize.height));
      }
      break;
    }
    case eStyleImageType_Element:
    {
      // XXX element() should have the width/height of the referenced element,
      //     and that element's ratio, if it matches.  If it doesn't match, it
      //     should have no width/height or ratio.  See element() in CSS images:
      //     <http://dev.w3.org/csswg/css-images-4/#element-notation>.
      //     Make sure to change nsStyleImageLayers::Size::DependsOnFrameSize
      //     when fixing this!
      if (mPaintServerFrame) {
        // SVG images have no intrinsic size
        if (!mPaintServerFrame->IsFrameOfType(nsIFrame::eSVG)) {
          // The intrinsic image size for a generic nsIFrame paint server is
          // the union of the border-box rects of all of its continuations,
          // rounded to device pixels.
          int32_t appUnitsPerDevPixel =
            mForFrame->PresContext()->AppUnitsPerDevPixel();
          result.SetSize(
            IntSizeToAppUnits(
              nsSVGIntegrationUtils::GetContinuationUnionSize(mPaintServerFrame).
                ToNearestPixels(appUnitsPerDevPixel),
              appUnitsPerDevPixel));
        }
      } else {
        NS_ASSERTION(mImageElementSurface.GetSourceSurface(),
                     "Surface should be ready.");
        IntSize surfaceSize = mImageElementSurface.mSize;
        result.SetSize(
          nsSize(nsPresContext::CSSPixelsToAppUnits(surfaceSize.width),
                 nsPresContext::CSSPixelsToAppUnits(surfaceSize.height)));
      }
      break;
    }
    case eStyleImageType_Gradient:
      // Per <http://dev.w3.org/csswg/css3-images/#gradients>, gradients have no
      // intrinsic dimensions.
    case eStyleImageType_Null:
    default:
      break;
  }

  return result;
}

/* static */ nsSize
nsImageRenderer::ComputeConcreteSize(const CSSSizeOrRatio& aSpecifiedSize,
                                     const CSSSizeOrRatio& aIntrinsicSize,
                                     const nsSize& aDefaultSize)
{
  // The specified size is fully specified, just use that
  if (aSpecifiedSize.IsConcrete()) {
    return aSpecifiedSize.ComputeConcreteSize();
  }

  MOZ_ASSERT(!aSpecifiedSize.mHasWidth || !aSpecifiedSize.mHasHeight);

  if (!aSpecifiedSize.mHasWidth && !aSpecifiedSize.mHasHeight) {
    // no specified size, try using the intrinsic size
    if (aIntrinsicSize.CanComputeConcreteSize()) {
      return aIntrinsicSize.ComputeConcreteSize();
    }

    if (aIntrinsicSize.mHasWidth) {
      return nsSize(aIntrinsicSize.mWidth, aDefaultSize.height);
    }
    if (aIntrinsicSize.mHasHeight) {
      return nsSize(aDefaultSize.width, aIntrinsicSize.mHeight);
    }

    // couldn't use the intrinsic size either, revert to using the default size
    return ComputeConstrainedSize(aDefaultSize,
                                  aIntrinsicSize.mRatio,
                                  CONTAIN);
  }

  MOZ_ASSERT(aSpecifiedSize.mHasWidth || aSpecifiedSize.mHasHeight);

  // The specified height is partial, try to compute the missing part.
  if (aSpecifiedSize.mHasWidth) {
    nscoord height;
    if (aIntrinsicSize.HasRatio()) {
      height = NSCoordSaturatingNonnegativeMultiply(
        aSpecifiedSize.mWidth,
        double(aIntrinsicSize.mRatio.height) / aIntrinsicSize.mRatio.width);
    } else if (aIntrinsicSize.mHasHeight) {
      height = aIntrinsicSize.mHeight;
    } else {
      height = aDefaultSize.height;
    }
    return nsSize(aSpecifiedSize.mWidth, height);
  }

  MOZ_ASSERT(aSpecifiedSize.mHasHeight);
  nscoord width;
  if (aIntrinsicSize.HasRatio()) {
    width = NSCoordSaturatingNonnegativeMultiply(
      aSpecifiedSize.mHeight,
      double(aIntrinsicSize.mRatio.width) / aIntrinsicSize.mRatio.height);
  } else if (aIntrinsicSize.mHasWidth) {
    width = aIntrinsicSize.mWidth;
  } else {
    width = aDefaultSize.width;
  }
  return nsSize(width, aSpecifiedSize.mHeight);
}

/* static */ nsSize
nsImageRenderer::ComputeConstrainedSize(const nsSize& aConstrainingSize,
                                        const nsSize& aIntrinsicRatio,
                                        FitType aFitType)
{
  if (aIntrinsicRatio.width <= 0 && aIntrinsicRatio.height <= 0) {
    return aConstrainingSize;
  }

  float scaleX = double(aConstrainingSize.width) / aIntrinsicRatio.width;
  float scaleY = double(aConstrainingSize.height) / aIntrinsicRatio.height;
  nsSize size;
  if ((aFitType == CONTAIN) == (scaleX < scaleY)) {
    size.width = aConstrainingSize.width;
    size.height = NSCoordSaturatingNonnegativeMultiply(
                    aIntrinsicRatio.height, scaleX);
    // If we're reducing the size by less than one css pixel, then just use the
    // constraining size.
    if (aFitType == CONTAIN && aConstrainingSize.height - size.height < nsPresContext::AppUnitsPerCSSPixel()) {
      size.height = aConstrainingSize.height;
    }
  } else {
    size.width = NSCoordSaturatingNonnegativeMultiply(
                   aIntrinsicRatio.width, scaleY);
    if (aFitType == CONTAIN && aConstrainingSize.width - size.width < nsPresContext::AppUnitsPerCSSPixel()) {
      size.width = aConstrainingSize.width;
    }
    size.height = aConstrainingSize.height;
  }
  return size;
}

/**
 * mSize is the image's "preferred" size for this particular rendering, while
 * the drawn (aka concrete) size is the actual rendered size after accounting
 * for background-size etc..  The preferred size is most often the image's
 * intrinsic dimensions.  But for images with incomplete intrinsic dimensions,
 * the preferred size varies, depending on the specified and default sizes, see
 * nsImageRenderer::Compute*Size.
 *
 * This distinction is necessary because the components of a vector image are
 * specified with respect to its preferred size for a rendering situation, not
 * to its actual rendered size.  For example, consider a 4px wide background
 * vector image with no height which contains a left-aligned
 * 2px wide black rectangle with height 100%.  If the background-size width is
 * auto (or 4px), the vector image will render 4px wide, and the black rectangle
 * will be 2px wide.  If the background-size width is 8px, the vector image will
 * render 8px wide, and the black rectangle will be 4px wide -- *not* 2px wide.
 * In both cases mSize.width will be 4px; but in the first case the returned
 * width will be 4px, while in the second case the returned width will be 8px.
 */
void
nsImageRenderer::SetPreferredSize(const CSSSizeOrRatio& aIntrinsicSize,
                                  const nsSize& aDefaultSize)
{
  mSize.width = aIntrinsicSize.mHasWidth
                  ? aIntrinsicSize.mWidth
                  : aDefaultSize.width;
  mSize.height = aIntrinsicSize.mHasHeight
                  ? aIntrinsicSize.mHeight
                  : aDefaultSize.height;
}

// Convert from nsImageRenderer flags to the flags we want to use for drawing in
// the imgIContainer namespace.
static uint32_t
ConvertImageRendererToDrawFlags(uint32_t aImageRendererFlags)
{
  uint32_t drawFlags = imgIContainer::FLAG_NONE;
  if (aImageRendererFlags & nsImageRenderer::FLAG_SYNC_DECODE_IMAGES) {
    drawFlags |= imgIContainer::FLAG_SYNC_DECODE;
  }
  if (aImageRendererFlags & nsImageRenderer::FLAG_PAINTING_TO_WINDOW) {
    drawFlags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;
  }
  return drawFlags;
}

/*
 *  SVG11: A luminanceToAlpha operation is equivalent to the following matrix operation:                                                   |
 *  | R' |     |      0        0        0  0  0 |   | R |
 *  | G' |     |      0        0        0  0  0 |   | G |
 *  | B' |  =  |      0        0        0  0  0 | * | B |
 *  | A' |     | 0.2125   0.7154   0.0721  0  0 |   | A |
 *  | 1  |     |      0        0        0  0  1 |   | 1 |
 */
static void
RGBALuminanceOperation(uint8_t *aData,
                       int32_t aStride,
                       const IntSize &aSize)
{
  int32_t redFactor = 55;    // 256 * 0.2125
  int32_t greenFactor = 183; // 256 * 0.7154
  int32_t blueFactor = 18;   // 256 * 0.0721

  for (int32_t y = 0; y < aSize.height; y++) {
    uint32_t *pixel = (uint32_t*)(aData + aStride * y);
    for (int32_t x = 0; x < aSize.width; x++) {
      *pixel = (((((*pixel & 0x00FF0000) >> 16) * redFactor) +
                 (((*pixel & 0x0000FF00) >>  8) * greenFactor) +
                  ((*pixel & 0x000000FF)        * blueFactor)) >> 8) << 24;
      pixel++;
    }
  }
}


DrawResult
nsImageRenderer::Draw(nsPresContext*       aPresContext,
                      nsRenderingContext&  aRenderingContext,
                      const nsRect&        aDirtyRect,
                      const nsRect&        aDest,
                      const nsRect&        aFill,
                      const nsPoint&       aAnchor,
                      const nsSize&        aRepeatSize,
                      const CSSIntRect&    aSrc)
{
  if (!IsReady()) {
    NS_NOTREACHED("Ensure PrepareImage() has returned true before calling me");
    return DrawResult::TEMPORARY_ERROR;
  }
  if (aDest.IsEmpty() || aFill.IsEmpty() ||
      mSize.width <= 0 || mSize.height <= 0) {
    return DrawResult::SUCCESS;
  }

  SamplingFilter samplingFilter = nsLayoutUtils::GetSamplingFilterForFrame(mForFrame);
  DrawResult result = DrawResult::SUCCESS;
  RefPtr<gfxContext> ctx = aRenderingContext.ThebesContext();
  IntRect tmpDTRect;

  if (ctx->CurrentOp() != CompositionOp::OP_OVER || mMaskOp == NS_STYLE_MASK_MODE_LUMINANCE) {
    gfxRect clipRect = ctx->GetClipExtents();
    tmpDTRect = RoundedOut(ToRect(clipRect));
    if (tmpDTRect.IsEmpty()) {
      return DrawResult::SUCCESS;
    }
    RefPtr<DrawTarget> tempDT =
      gfxPlatform::GetPlatform()->CreateSimilarSoftwareDrawTarget(ctx->GetDrawTarget(),
                                                                  tmpDTRect.Size(),
                                                                  SurfaceFormat::B8G8R8A8);
    if (!tempDT || !tempDT->IsValid()) {
      gfxDevCrash(LogReason::InvalidContext) << "ImageRenderer::Draw problem " << gfx::hexa(tempDT);
      return DrawResult::TEMPORARY_ERROR;
    }
    tempDT->SetTransform(Matrix::Translation(-tmpDTRect.TopLeft()));
    ctx = gfxContext::CreatePreservingTransformOrNull(tempDT);
    if (!ctx) {
      gfxDevCrash(LogReason::InvalidContext) << "ImageRenderer::Draw problem " << gfx::hexa(tempDT);
      return DrawResult::TEMPORARY_ERROR;
    }
  }

  switch (mType) {
    case eStyleImageType_Image:
    {
      CSSIntSize imageSize(nsPresContext::AppUnitsToIntCSSPixels(mSize.width),
                           nsPresContext::AppUnitsToIntCSSPixels(mSize.height));
      result =
        nsLayoutUtils::DrawBackgroundImage(*ctx,
                                           aPresContext,
                                           mImageContainer, imageSize,
                                           samplingFilter,
                                           aDest, aFill, aRepeatSize,
                                           aAnchor, aDirtyRect,
                                           ConvertImageRendererToDrawFlags(mFlags),
                                           mExtendMode);
      break;
    }
    case eStyleImageType_Gradient:
    {
      nsCSSRendering::PaintGradient(aPresContext, aRenderingContext,
                                    mGradientData, aDirtyRect,
                                    aDest, aFill, aRepeatSize, aSrc, mSize);
      break;
    }
    case eStyleImageType_Element:
    {
      RefPtr<gfxDrawable> drawable = DrawableForElement(aDest,
                                                          aRenderingContext);
      if (!drawable) {
        NS_WARNING("Could not create drawable for element");
        return DrawResult::TEMPORARY_ERROR;
      }

      nsCOMPtr<imgIContainer> image(ImageOps::CreateFromDrawable(drawable));
      result =
        nsLayoutUtils::DrawImage(*ctx,
                                 aPresContext, image,
                                 samplingFilter, aDest, aFill, aAnchor, aDirtyRect,
                                 ConvertImageRendererToDrawFlags(mFlags));
      break;
    }
    case eStyleImageType_Null:
    default:
      break;
  }

  if (!tmpDTRect.IsEmpty()) {
    RefPtr<SourceSurface> surf = ctx->GetDrawTarget()->Snapshot();
    if (mMaskOp == NS_STYLE_MASK_MODE_LUMINANCE) {
      RefPtr<DataSourceSurface> maskData = surf->GetDataSurface();
      DataSourceSurface::MappedSurface map;
      if (!maskData->Map(DataSourceSurface::MapType::WRITE, &map)) {
        return result;
      }

      RGBALuminanceOperation(map.mData, map.mStride, maskData->GetSize());
      maskData->Unmap();
      surf = maskData;
    }

    DrawTarget* dt = aRenderingContext.ThebesContext()->GetDrawTarget();
    dt->DrawSurface(surf, Rect(tmpDTRect.x, tmpDTRect.y, tmpDTRect.width, tmpDTRect.height),
                    Rect(0, 0, tmpDTRect.width, tmpDTRect.height),
                    DrawSurfaceOptions(SamplingFilter::POINT),
                    DrawOptions(1.0f, aRenderingContext.ThebesContext()->CurrentOp()));
  }

  return result;
}

already_AddRefed<gfxDrawable>
nsImageRenderer::DrawableForElement(const nsRect& aImageRect,
                                    nsRenderingContext&  aRenderingContext)
{
  NS_ASSERTION(mType == eStyleImageType_Element,
               "DrawableForElement only makes sense if backed by an element");
  if (mPaintServerFrame) {
    // XXX(seth): In order to not pass FLAG_SYNC_DECODE_IMAGES here,
    // DrawableFromPaintServer would have to return a DrawResult indicating
    // whether any images could not be painted because they weren't fully
    // decoded. Even always passing FLAG_SYNC_DECODE_IMAGES won't eliminate all
    // problems, as it won't help if there are image which haven't finished
    // loading, but it's better than nothing.
    int32_t appUnitsPerDevPixel = mForFrame->PresContext()->AppUnitsPerDevPixel();
    nsRect destRect = aImageRect - aImageRect.TopLeft();
    nsIntSize roundedOut = destRect.ToOutsidePixels(appUnitsPerDevPixel).Size();
    IntSize imageSize(roundedOut.width, roundedOut.height);
    RefPtr<gfxDrawable> drawable =
      nsSVGIntegrationUtils::DrawableFromPaintServer(
        mPaintServerFrame, mForFrame, mSize, imageSize,
        aRenderingContext.GetDrawTarget(),
        aRenderingContext.ThebesContext()->CurrentMatrix(),
        nsSVGIntegrationUtils::FLAG_SYNC_DECODE_IMAGES);

    return drawable.forget();
  }
  NS_ASSERTION(mImageElementSurface.GetSourceSurface(), "Surface should be ready.");
  RefPtr<gfxDrawable> drawable = new gfxSurfaceDrawable(
                                mImageElementSurface.GetSourceSurface().get(),
                                mImageElementSurface.mSize);
  return drawable.forget();
}

DrawResult
nsImageRenderer::DrawBackground(nsPresContext*       aPresContext,
                                nsRenderingContext&  aRenderingContext,
                                const nsRect&        aDest,
                                const nsRect&        aFill,
                                const nsPoint&       aAnchor,
                                const nsRect&        aDirty,
                                const nsSize&        aRepeatSize)
{
  if (!IsReady()) {
    NS_NOTREACHED("Ensure PrepareImage() has returned true before calling me");
    return DrawResult::TEMPORARY_ERROR;
  }
  if (aDest.IsEmpty() || aFill.IsEmpty() ||
      mSize.width <= 0 || mSize.height <= 0) {
    return DrawResult::SUCCESS;
  }

  return Draw(aPresContext, aRenderingContext,
              aDirty, aDest, aFill, aAnchor, aRepeatSize,
              CSSIntRect(0, 0,
                         nsPresContext::AppUnitsToIntCSSPixels(mSize.width),
                         nsPresContext::AppUnitsToIntCSSPixels(mSize.height)));
}

/**
 * Compute the size and position of the master copy of the image. I.e., a single
 * tile used to fill the dest rect.
 * aFill The destination rect to be filled
 * aHFill and aVFill are the repeat patterns for the component -
 * NS_STYLE_BORDER_IMAGE_REPEAT_* - i.e., how a tiling unit is used to fill aFill
 * aUnitSize The size of the source rect in dest coords.
 */
static nsRect
ComputeTile(nsRect&              aFill,
            uint8_t              aHFill,
            uint8_t              aVFill,
            const nsSize&        aUnitSize,
            nsSize&              aRepeatSize)
{
  nsRect tile;
  switch (aHFill) {
  case NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH:
    tile.x = aFill.x;
    tile.width = aFill.width;
    aRepeatSize.width = tile.width;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT_REPEAT:
    tile.x = aFill.x + aFill.width/2 - aUnitSize.width/2;
    tile.width = aUnitSize.width;
    aRepeatSize.width = tile.width;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT_ROUND:
    tile.x = aFill.x;
    tile.width = ComputeRoundedSize(aUnitSize.width, aFill.width);
    aRepeatSize.width = tile.width;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT_SPACE:
    {
      nscoord space;
      aRepeatSize.width =
        ComputeBorderSpacedRepeatSize(aUnitSize.width, aFill.width, space);
      tile.x = aFill.x + space;
      tile.width = aUnitSize.width;
      aFill.x = tile.x;
      aFill.width = aFill.width - space * 2;
    }
    break;
  default:
    NS_NOTREACHED("unrecognized border-image fill style");
  }

  switch (aVFill) {
  case NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH:
    tile.y = aFill.y;
    tile.height = aFill.height;
    aRepeatSize.height = tile.height;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT_REPEAT:
    tile.y = aFill.y + aFill.height/2 - aUnitSize.height/2;
    tile.height = aUnitSize.height;
    aRepeatSize.height = tile.height;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT_ROUND:
    tile.y = aFill.y;
    tile.height = ComputeRoundedSize(aUnitSize.height, aFill.height);
    aRepeatSize.height = tile.height;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT_SPACE:
    {
      nscoord space;
      aRepeatSize.height =
        ComputeBorderSpacedRepeatSize(aUnitSize.height, aFill.height, space);
      tile.y = aFill.y + space;
      tile.height = aUnitSize.height;
      aFill.y = tile.y;
      aFill.height = aFill.height - space * 2;
    }
    break;
  default:
    NS_NOTREACHED("unrecognized border-image fill style");
  }

  return tile;
}

/**
 * Returns true if the given set of arguments will require the tiles which fill
 * the dest rect to be scaled from the source tile. See comment on ComputeTile
 * for argument descriptions.
 */
static bool
RequiresScaling(const nsRect&        aFill,
                uint8_t              aHFill,
                uint8_t              aVFill,
                const nsSize&        aUnitSize)
{
  // If we have no tiling in either direction, we can skip the intermediate
  // scaling step.
  return (aHFill != NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH ||
          aVFill != NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH) &&
         (aUnitSize.width != aFill.width ||
          aUnitSize.height != aFill.height);
}

DrawResult
nsImageRenderer::DrawBorderImageComponent(nsPresContext*       aPresContext,
                                          nsRenderingContext&  aRenderingContext,
                                          const nsRect&        aDirtyRect,
                                          const nsRect&        aFill,
                                          const CSSIntRect&    aSrc,
                                          uint8_t              aHFill,
                                          uint8_t              aVFill,
                                          const nsSize&        aUnitSize,
                                          uint8_t              aIndex,
                                          const Maybe<nsSize>& aSVGViewportSize,
                                          const bool           aHasIntrinsicRatio)
{
  if (!IsReady()) {
    NS_NOTREACHED("Ensure PrepareImage() has returned true before calling me");
    return DrawResult::BAD_ARGS;
  }
  if (aFill.IsEmpty() || aSrc.IsEmpty()) {
    return DrawResult::SUCCESS;
  }

  if (mType == eStyleImageType_Image || mType == eStyleImageType_Element) {
    nsCOMPtr<imgIContainer> subImage;

    // To draw one portion of an image into a border component, we stretch that
    // portion to match the size of that border component and then draw onto.
    // However, preserveAspectRatio attribute of a SVG image may break this rule.
    // To get correct rendering result, we add
    // FLAG_FORCE_PRESERVEASPECTRATIO_NONE flag here, to tell mImage to ignore
    // preserveAspectRatio attribute, and always do non-uniform stretch.
    uint32_t drawFlags = ConvertImageRendererToDrawFlags(mFlags) |
                           imgIContainer::FLAG_FORCE_PRESERVEASPECTRATIO_NONE;
    // For those SVG image sources which don't have fixed aspect ratio (i.e.
    // without viewport size and viewBox), we should scale the source uniformly
    // after the viewport size is decided by "Default Sizing Algorithm".
    if (!aHasIntrinsicRatio) {
      drawFlags = drawFlags | imgIContainer::FLAG_FORCE_UNIFORM_SCALING;
    }
    // Retrieve or create the subimage we'll draw.
    nsIntRect srcRect(aSrc.x, aSrc.y, aSrc.width, aSrc.height);
    if (mType == eStyleImageType_Image) {
      if ((subImage = mImage->GetSubImage(aIndex)) == nullptr) {
        subImage = ImageOps::Clip(mImageContainer, srcRect, aSVGViewportSize);
        mImage->SetSubImage(aIndex, subImage);
      }
    } else {
      // This path, for eStyleImageType_Element, is currently slower than it
      // needs to be because we don't cache anything. (In particular, if we have
      // to draw to a temporary surface inside ClippedImage, we don't cache that
      // temporary surface since we immediately throw the ClippedImage we create
      // here away.) However, if we did cache, we'd need to know when to
      // invalidate that cache, and it's not clear that it's worth the trouble
      // since using border-image with -moz-element is rare.

      RefPtr<gfxDrawable> drawable = DrawableForElement(nsRect(nsPoint(), mSize),
                                                          aRenderingContext);
      if (!drawable) {
        NS_WARNING("Could not create drawable for element");
        return DrawResult::TEMPORARY_ERROR;
      }

      nsCOMPtr<imgIContainer> image(ImageOps::CreateFromDrawable(drawable));
      subImage = ImageOps::Clip(image, srcRect, aSVGViewportSize);
    }

    MOZ_ASSERT_IF(aSVGViewportSize,
                  subImage->GetType() == imgIContainer::TYPE_VECTOR);

    SamplingFilter samplingFilter = nsLayoutUtils::GetSamplingFilterForFrame(mForFrame);

    if (!RequiresScaling(aFill, aHFill, aVFill, aUnitSize)) {
      return nsLayoutUtils::DrawSingleImage(*aRenderingContext.ThebesContext(),
                                            aPresContext,
                                            subImage,
                                            samplingFilter,
                                            aFill, aDirtyRect,
                                            nullptr,
                                            drawFlags);
    }

    nsSize repeatSize;
    nsRect fillRect(aFill);
    nsRect tile = ComputeTile(fillRect, aHFill, aVFill, aUnitSize, repeatSize);
    CSSIntSize imageSize(srcRect.width, srcRect.height);
    return nsLayoutUtils::DrawBackgroundImage(*aRenderingContext.ThebesContext(),
                                              aPresContext,
                                              subImage, imageSize, samplingFilter,
                                              tile, fillRect, repeatSize,
                                              tile.TopLeft(), aDirtyRect,
                                              drawFlags,
                                              ExtendMode::CLAMP);
  }

  nsSize repeatSize(aFill.Size());
  nsRect fillRect(aFill);
  nsRect destTile = RequiresScaling(fillRect, aHFill, aVFill, aUnitSize)
                  ? ComputeTile(fillRect, aHFill, aVFill, aUnitSize, repeatSize)
                  : fillRect;
  return Draw(aPresContext, aRenderingContext, aDirtyRect, destTile,
              fillRect, destTile.TopLeft(), repeatSize, aSrc);
}

bool
nsImageRenderer::IsRasterImage()
{
  if (mType != eStyleImageType_Image || !mImageContainer)
    return false;
  return mImageContainer->GetType() == imgIContainer::TYPE_RASTER;
}

bool
nsImageRenderer::IsAnimatedImage()
{
  if (mType != eStyleImageType_Image || !mImageContainer)
    return false;
  bool animated = false;
  if (NS_SUCCEEDED(mImageContainer->GetAnimated(&animated)) && animated)
    return true;

  return false;
}

already_AddRefed<imgIContainer>
nsImageRenderer::GetImage()
{
  if (mType != eStyleImageType_Image || !mImageContainer) {
    return nullptr;
  }

  nsCOMPtr<imgIContainer> image = mImageContainer;
  return image.forget();
}

void
nsImageRenderer::PurgeCacheForViewportChange(
  const Maybe<nsSize>& aSVGViewportSize, const bool aHasIntrinsicRatio)
{
  // Check if we should flush the cached data - only vector images need to do
  // the check since they might not have fixed ratio.
  if (mImageContainer &&
      mImageContainer->GetType() == imgIContainer::TYPE_VECTOR) {
    mImage->PurgeCacheForViewportChange(aSVGViewportSize, aHasIntrinsicRatio);
  }
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
  if (aSkipRect) {
    gfxRect skipRect = transform.TransformBounds(*aSkipRect);
    mContext = mAlphaBoxBlur.Init(aDestinationCtx, rect, spreadRadius,
                                  blurRadius, &dirtyRect, &skipRect);
  } else {
    mContext = mAlphaBoxBlur.Init(aDestinationCtx, rect, spreadRadius,
                                  blurRadius, &dirtyRect, nullptr);
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
