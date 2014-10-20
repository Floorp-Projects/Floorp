/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* utility functions for drawing borders and backgrounds */

#include <ctime>

#include "mozilla/DebugOnly.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MathAlgorithms.h"

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
#include "gfxColor.h"
#include "gfxGradientCache.h"
#include "GraphicsFilter.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::gfx;
using mozilla::image::ImageOps;
using mozilla::CSSSizeOrRatio;

static int gFrameTreeLockCount = 0;

// To avoid storing this data on nsInlineFrame (bloat) and to avoid
// recalculating this for each frame in a continuation (perf), hold
// a cache of various coordinate information that we need in order
// to paint inline backgrounds.
struct InlineBackgroundData
{
  InlineBackgroundData()
      : mFrame(nullptr), mBlockFrame(nullptr)
  {
  }

  ~InlineBackgroundData()
  {
  }

  void Reset()
  {
    mBoundingBox.SetRect(0,0,0,0);
    mContinuationPoint = mLineContinuationPoint = mUnbrokenWidth = 0;
    mFrame = mBlockFrame = nullptr;
    mLeftBorderData.Reset();
  }

  /**
   * Return a continuous rect for (an inline) aFrame relative to the
   * continuation that draws the left-most part of the background.
   * This is used when painting backgrounds.
   */
  nsRect GetContinuousRect(nsIFrame* aFrame)
  {
    MOZ_ASSERT(aFrame->GetType() == nsGkAtoms::inlineFrame);

    SetFrame(aFrame);

    nscoord x;
    if (mBidiEnabled) {
      x = mLineContinuationPoint;

      // Scan continuations on the same line as aFrame and accumulate the widths
      // of frames that are to the left (if this is an LTR block) or right
      // (if it's RTL) of the current one.
      bool isRtlBlock = (mBlockFrame->StyleVisibility()->mDirection ==
                           NS_STYLE_DIRECTION_RTL);
      nscoord curOffset = aFrame->GetOffsetTo(mBlockFrame).x;

      // If the continuation is fluid we know inlineFrame is not on the same line.
      // If it's not fluid, we need to test further to be sure.
      nsIFrame* inlineFrame = aFrame->GetPrevContinuation();
      while (inlineFrame && !inlineFrame->GetNextInFlow() &&
             AreOnSameLine(aFrame, inlineFrame)) {
        nscoord frameXOffset = inlineFrame->GetOffsetTo(mBlockFrame).x;
        if(isRtlBlock == (frameXOffset >= curOffset)) {
          x += inlineFrame->GetSize().width;
        }
        inlineFrame = inlineFrame->GetPrevContinuation();
      }

      inlineFrame = aFrame->GetNextContinuation();
      while (inlineFrame && !inlineFrame->GetPrevInFlow() &&
             AreOnSameLine(aFrame, inlineFrame)) {
        nscoord frameXOffset = inlineFrame->GetOffsetTo(mBlockFrame).x;
        if(isRtlBlock == (frameXOffset >= curOffset)) {
          x += inlineFrame->GetSize().width;
        }
        inlineFrame = inlineFrame->GetNextContinuation();
      }
      if (isRtlBlock) {
        // aFrame itself is also to the right of its left edge, so add its width.
        x += aFrame->GetSize().width;
        // x is now the distance from the left edge of aFrame to the right edge
        // of the unbroken content. Change it to indicate the distance from the
        // left edge of the unbroken content to the left edge of aFrame.
        x = mUnbrokenWidth - x;
      }
    } else {
      x = mContinuationPoint;
    }

    // Assume background-origin: border and return a rect with offsets
    // relative to (0,0).  If we have a different background-origin,
    // then our rect should be deflated appropriately by our caller.
    return nsRect(-x, 0, mUnbrokenWidth, mFrame->GetSize().height);
  }

  /**
   * Return a continuous rect for (an inline) aFrame relative to the
   * continuation that should draw the left-border.  This is used when painting
   * borders and clipping backgrounds.  This may NOT be the same continuous rect
   * as for drawing backgrounds; the continuation with the left-border might be
   * somewhere in the middle of that rect (e.g. BIDI), in those cases we need
   * the reverse background order starting at the left-border continuation.
   */
  nsRect GetBorderContinuousRect(nsIFrame* aFrame, nsRect aBorderArea)
  {
    // Calling GetContinuousRect(aFrame) here may lead to Reset/Init which
    // resets our mLeftBorderData so we save it ...
    LeftBorderData saved(mLeftBorderData);
    nsRect joinedBorderArea = GetContinuousRect(aFrame);
    if (!saved.mIsValid || saved.mFrame != mLeftBorderData.mFrame) {
      if (aFrame == mLeftBorderData.mFrame) {
        mLeftBorderData.SetX(joinedBorderArea.x);
      } else if (mLeftBorderData.mFrame) {
        mLeftBorderData.SetX(GetContinuousRect(mLeftBorderData.mFrame).x);
      }
    } else {
      // ... and restore it when possible.
      mLeftBorderData.mX = saved.mX;
    }
    if (joinedBorderArea.x > mLeftBorderData.mX) {
      joinedBorderArea.x =
        -(mUnbrokenWidth + joinedBorderArea.x - aBorderArea.width);
    } else {
      joinedBorderArea.x -= mLeftBorderData.mX;
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
  struct LeftBorderData {
    nsIFrame* mFrame;   // the continuation that may have a left-border
    nscoord   mX;       // cached GetContinuousRect(mFrame).x
    bool      mIsValid; // true if mX is valid
    void Reset() { mFrame = nullptr; mIsValid = false; }
    void SetX(nscoord aX) { mX = aX; mIsValid = true; }
  };

  nsIFrame*      mFrame;
  nsBlockFrame*  mBlockFrame;
  nsRect         mBoundingBox;
  nscoord        mContinuationPoint;
  nscoord        mUnbrokenWidth;
  nscoord        mLineContinuationPoint;
  LeftBorderData mLeftBorderData;
  bool           mBidiEnabled;

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
    mContinuationPoint += mFrame->GetSize().width;

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
      nsIFrame* block = static_cast<nsIFrame*>
        (aFrame->Properties().Get(nsIFrame::IBSplitPrevSibling()));
      if (block) {
        // The {ib} properties are only stored on first continuations
        NS_ASSERTION(!block->GetPrevContinuation(),
                     "Incorrect value for IBSplitPrevSibling");
        prevCont = static_cast<nsIFrame*>
          (block->Properties().Get(nsIFrame::IBSplitPrevSibling()));
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
      nsIFrame* block = static_cast<nsIFrame*>
        (aFrame->Properties().Get(nsIFrame::IBSplitSibling()));
      if (block) {
        nextCont = static_cast<nsIFrame*>
          (block->Properties().Get(nsIFrame::IBSplitSibling()));
        NS_ASSERTION(nextCont, "How did that happen?");
      }
    }
    return nextCont;
  }

  void Init(nsIFrame* aFrame)
  {
    mLeftBorderData.Reset();
    mBidiEnabled = aFrame->PresContext()->BidiEnabled();
    if (mBidiEnabled) {
      // Find the containing block frame
      nsIFrame* frame = aFrame;
      do {
        frame = frame->GetParent();
        mBlockFrame = do_QueryFrame(frame);
      }
      while (frame && frame->IsFrameOfType(nsIFrame::eLineParticipant));

      NS_ASSERTION(mBlockFrame, "Cannot find containing block.");
    }

    // Start with the previous flow frame as our continuation point
    // is the total of the widths of the previous frames.
    nsIFrame* inlineFrame = GetPrevContinuation(aFrame);
    while (inlineFrame) {
      if (!mLeftBorderData.mFrame &&
          !inlineFrame->GetSkipSides().Left()) {
        mLeftBorderData.mFrame = inlineFrame;
      }
      nsRect rect = inlineFrame->GetRect();
      mContinuationPoint += rect.width;
      if (mBidiEnabled && !AreOnSameLine(aFrame, inlineFrame)) {
        mLineContinuationPoint += rect.width;
      }
      mUnbrokenWidth += rect.width;
      mBoundingBox.UnionRect(mBoundingBox, rect);
      inlineFrame = GetPrevContinuation(inlineFrame);
    }

    // Next add this frame and subsequent frames to the bounding box and
    // unbroken width.
    inlineFrame = aFrame;
    while (inlineFrame) {
      if (!mLeftBorderData.mFrame &&
          !inlineFrame->GetSkipSides().Left()) {
        mLeftBorderData.mFrame = inlineFrame;
      }
      nsRect rect = inlineFrame->GetRect();
      mUnbrokenWidth += rect.width;
      mBoundingBox.UnionRect(mBoundingBox, rect);
      inlineFrame = GetNextContinuation(inlineFrame);
    }

    mFrame = aFrame;
  }

  bool AreOnSameLine(nsIFrame* aFrame1, nsIFrame* aFrame2) {
    bool isValid1, isValid2;
    nsBlockInFlowLineIterator it1(mBlockFrame, aFrame1, &isValid1);
    nsBlockInFlowLineIterator it2(mBlockFrame, aFrame2, &isValid2);
    return isValid1 && isValid2 &&
      // Make sure aFrame1 and aFrame2 are in the same continuation of
      // mBlockFrame.
      it1.GetContainer() == it2.GetContainer() &&
      // And on the same line in it
      it1.GetLine() == it2.GetLine();
  }
};

// A resolved color stop --- with a specific position along the gradient line,
// and a Thebes color
struct ColorStop {
  ColorStop(double aPosition, gfxRGBA aColor) :
    mPosition(aPosition), mColor(aColor) {}
  double mPosition; // along the gradient line; 0=start, 1=end
  gfxRGBA mColor;
};

/* Local functions */
static void DrawBorderImage(nsPresContext* aPresContext,
                            nsRenderingContext& aRenderingContext,
                            nsIFrame* aForFrame,
                            const nsRect& aBorderArea,
                            const nsStyleBorder& aStyleBorder,
                            const nsRect& aDirtyRect,
                            Sides aSkipSides);

static nscolor MakeBevelColor(mozilla::css::Side whichSide, uint8_t style,
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
MakeBevelColor(mozilla::css::Side whichSide, uint8_t style,
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
    case NS_SIDE_BOTTOM: whichSide = NS_SIDE_TOP;    break;
    case NS_SIDE_RIGHT:  whichSide = NS_SIDE_LEFT;   break;
    case NS_SIDE_TOP:    whichSide = NS_SIDE_BOTTOM; break;
    case NS_SIDE_LEFT:   whichSide = NS_SIDE_RIGHT;  break;
    }
  }

  switch (whichSide) {
  case NS_SIDE_BOTTOM:
    theColor = colors[1];
    break;
  case NS_SIDE_RIGHT:
    theColor = colors[1];
    break;
  case NS_SIDE_TOP:
    theColor = colors[0];
    break;
  case NS_SIDE_LEFT:
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
         gfxCornerSizes* aBgRadii)
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
  if (aFrame->GetType() == nsGkAtoms::inlineFrame) {
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
  return aStyleBorder.mBoxDecorationBreak ==
           NS_STYLE_BOX_DECORATION_BREAK_SLICE;
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
                                  gfxCornerSizes *oBorderRadii)
{
  gfxFloat radii[8];
  NS_FOR_CSS_HALF_CORNERS(corner)
    radii[corner] = gfxFloat(aAppUnitsRadii[corner]) / aAppUnitsPerPixel;

  (*oBorderRadii)[C_TL] = gfxSize(radii[NS_CORNER_TOP_LEFT_X],
                                  radii[NS_CORNER_TOP_LEFT_Y]);
  (*oBorderRadii)[C_TR] = gfxSize(radii[NS_CORNER_TOP_RIGHT_X],
                                  radii[NS_CORNER_TOP_RIGHT_Y]);
  (*oBorderRadii)[C_BR] = gfxSize(radii[NS_CORNER_BOTTOM_RIGHT_X],
                                  radii[NS_CORNER_BOTTOM_RIGHT_Y]);
  (*oBorderRadii)[C_BL] = gfxSize(radii[NS_CORNER_BOTTOM_LEFT_X],
                                  radii[NS_CORNER_BOTTOM_LEFT_Y]);
}

void
nsCSSRendering::PaintBorder(nsPresContext* aPresContext,
                            nsRenderingContext& aRenderingContext,
                            nsIFrame* aForFrame,
                            const nsRect& aDirtyRect,
                            const nsRect& aBorderArea,
                            nsStyleContext* aStyleContext,
                            Sides aSkipSides)
{
  PROFILER_LABEL("nsCSSRendering", "PaintBorder",
    js::ProfileEntry::Category::GRAPHICS);

  nsStyleContext *styleIfVisited = aStyleContext->GetStyleIfVisited();
  const nsStyleBorder *styleBorder = aStyleContext->StyleBorder();
  // Don't check RelevantLinkVisited here, since we want to take the
  // same amount of time whether or not it's true.
  if (!styleIfVisited) {
    PaintBorderWithStyleBorder(aPresContext, aRenderingContext, aForFrame,
                               aDirtyRect, aBorderArea, *styleBorder,
                               aStyleContext, aSkipSides);
    return;
  }

  nsStyleBorder newStyleBorder(*styleBorder);
  // We could do something fancy to avoid the TrackImage/UntrackImage
  // work, but it doesn't seem worth it.  (We need to call TrackImage
  // since we're not going through nsRuleNode::ComputeBorderData.)
  newStyleBorder.TrackImage(aPresContext);

  NS_FOR_CSS_SIDES(side) {
    newStyleBorder.SetBorderColor(side,
      aStyleContext->GetVisitedDependentColor(
        nsCSSProps::SubpropertyEntryFor(eCSSProperty_border_color)[side]));
  }
  PaintBorderWithStyleBorder(aPresContext, aRenderingContext, aForFrame,
                             aDirtyRect, aBorderArea, newStyleBorder,
                             aStyleContext, aSkipSides);

  // We could do something fancy to avoid the TrackImage/UntrackImage
  // work, but it doesn't seem worth it.  (We need to call UntrackImage
  // since we're not going through nsStyleBorder::Destroy.)
  newStyleBorder.UntrackImage(aPresContext);
}

void
nsCSSRendering::PaintBorderWithStyleBorder(nsPresContext* aPresContext,
                                           nsRenderingContext& aRenderingContext,
                                           nsIFrame* aForFrame,
                                           const nsRect& aDirtyRect,
                                           const nsRect& aBorderArea,
                                           const nsStyleBorder& aStyleBorder,
                                           nsStyleContext* aStyleContext,
                                           Sides aSkipSides)
{
  PrintAsStringNewline("++ PaintBorder");

  // Check to see if we have an appearance defined.  If so, we let the theme
  // renderer draw the border.  DO not get the data from aForFrame, since the passed in style context
  // may be different!  Always use |aStyleContext|!
  const nsStyleDisplay* displayData = aStyleContext->StyleDisplay();
  if (displayData->mAppearance) {
    nsITheme *theme = aPresContext->GetTheme();
    if (theme && theme->ThemeSupportsWidget(aPresContext, aForFrame, displayData->mAppearance))
      return; // Let the theme handle it.
  }

  if (aStyleBorder.IsBorderImageLoaded()) {
    DrawBorderImage(aPresContext, aRenderingContext, aForFrame,
                    aBorderArea, aStyleBorder, aDirtyRect, aSkipSides);
    return;
  }

  // Get our style context's color struct.
  const nsStyleColor* ourColor = aStyleContext->StyleColor();

  // In NavQuirks mode we want to use the parent's context as a starting point
  // for determining the background color.
  bool quirks = aPresContext->CompatibilityMode() == eCompatibility_NavQuirks;
  nsIFrame* bgFrame = FindNonTransparentBackgroundFrame(aForFrame, quirks);
  nsStyleContext* bgContext = bgFrame->StyleContext();
  nscolor bgColor =
    bgContext->GetVisitedDependentColor(eCSSProperty_background_color);

  nsMargin border = aStyleBorder.GetComputedBorder();
  if (0 == border.left && 0 == border.right &&
      0 == border.top  && 0 == border.bottom) {
    // Empty border area
    return;
  }

  // Compute the outermost boundary of the area that might be painted.
  // Same coordinate space as aBorderArea & aBGClipRect.
  nsRect joinedBorderArea =
    ::BoxDecorationRectForBorder(aForFrame, aBorderArea, aSkipSides, &aStyleBorder);
  gfxCornerSizes bgRadii;
  ::GetRadii(aForFrame, aStyleBorder, aBorderArea, joinedBorderArea, &bgRadii);

  PrintAsFormatString(" joinedBorderArea: %d %d %d %d\n", joinedBorderArea.x, joinedBorderArea.y,
     joinedBorderArea.width, joinedBorderArea.height);

  // start drawing
  gfxContext* ctx = aRenderingContext.ThebesContext();
  ctx->Save();

  if (::IsBoxDecorationSlice(aStyleBorder)) {
    if (joinedBorderArea.IsEqualEdges(aBorderArea)) {
      // No need for a clip, just skip the sides we don't want.
      border.ApplySkipSides(aSkipSides);
    } else {
      // We're drawing borders around the joined continuation boxes so we need
      // to clip that to the slice that we want for this frame.
      aRenderingContext.ThebesContext()->
        Clip(NSRectToRect(aBorderArea,
                          aForFrame->PresContext()->AppUnitsPerDevPixel(),
                          *aRenderingContext.GetDrawTarget()));
    }
  } else {
    MOZ_ASSERT(joinedBorderArea.IsEqualEdges(aBorderArea),
               "Should use aBorderArea for box-decoration-break:clone");
    MOZ_ASSERT(aForFrame->GetSkipSides().IsEmpty(),
               "Should not skip sides for box-decoration-break:clone except "
               "::first-letter/line continuations or other frame types that "
               "don't have borders but those shouldn't reach this point.");
  }

  // Convert to dev pixels.
  nscoord twipsPerPixel = aPresContext->DevPixelsToAppUnits(1);
  gfxRect joinedBorderAreaPx =
    nsLayoutUtils::RectToGfxRect(joinedBorderArea, twipsPerPixel);
  gfxFloat borderWidths[4] = { gfxFloat(border.top / twipsPerPixel),
                               gfxFloat(border.right / twipsPerPixel),
                               gfxFloat(border.bottom / twipsPerPixel),
                               gfxFloat(border.left / twipsPerPixel) };

  uint8_t borderStyles[4];
  nscolor borderColors[4];
  nsBorderColors *compositeColors[4];

  // pull out styles, colors, composite colors
  NS_FOR_CSS_SIDES (i) {
    bool foreground;
    borderStyles[i] = aStyleBorder.GetBorderStyle(i);
    aStyleBorder.GetBorderColor(i, borderColors[i], foreground);
    aStyleBorder.GetCompositeColors(i, &compositeColors[i]);

    if (foreground)
      borderColors[i] = ourColor->mColor;
  }

  PrintAsFormatString(" borderStyles: %d %d %d %d\n", borderStyles[0], borderStyles[1], borderStyles[2], borderStyles[3]);
  //PrintAsFormatString ("bgRadii: %f %f %f %f\n", bgRadii[0], bgRadii[1], bgRadii[2], bgRadii[3]);

#if 0
  // this will draw a transparent red backround underneath the border area
  ctx->Save();
  ctx->Rectangle(joinedBorderAreaPx);
  ctx->SetColor(gfxRGBA(1.0, 0.0, 0.0, 0.5));
  ctx->Fill();
  ctx->Restore();
#endif

  nsCSSBorderRenderer br(twipsPerPixel,
                         ctx,
                         joinedBorderAreaPx,
                         borderStyles,
                         borderWidths,
                         bgRadii,
                         borderColors,
                         compositeColors,
                         bgColor);
  br.DrawBorders();

  ctx->Restore();

  PrintAsStringNewline();
}

static nsRect
GetOutlineInnerRect(nsIFrame* aFrame)
{
  nsRect* savedOutlineInnerRect = static_cast<nsRect*>
    (aFrame->Properties().Get(nsIFrame::OutlineInnerRectProperty()));
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

  uint8_t outlineStyle = ourOutline->GetOutlineStyle();
  nscoord width;
  ourOutline->GetOutlineWidth(width);

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
      aStyleContext->GetPseudoType() == nsCSSPseudoElements::ePseudo_XULTree
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
  gfxRect oRect(nsLayoutUtils::RectToGfxRect(outerRect, twipsPerPixel));

  // convert the radii
  nsMargin outlineMargin(width, width, width, width);
  gfxCornerSizes outlineRadii;
  ComputePixelRadii(twipsRadii, twipsPerPixel, &outlineRadii);

  if (nsLayoutUtils::IsOutlineStyleAutoEnabled()) {
    if (outlineStyle == NS_STYLE_BORDER_STYLE_AUTO) {
      nsITheme* theme = aPresContext->GetTheme();
      if (theme && theme->ThemeSupportsWidget(aPresContext, aForFrame,
                                              NS_THEME_FOCUS_OUTLINE)) {
        theme->DrawWidgetBackground(&aRenderingContext, aForFrame,
                                    NS_THEME_FOCUS_OUTLINE, innerRect,
                                    aDirtyRect);
        return;
      } else if (width == 0) {
        return; // empty outline
      }
      // http://dev.w3.org/csswg/css-ui/#outline
      // "User agents may treat 'auto' as 'solid'."
      outlineStyle = NS_STYLE_BORDER_STYLE_SOLID;
    }
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
  gfxFloat outlineWidths[4] = { gfxFloat(width / twipsPerPixel),
                                gfxFloat(width / twipsPerPixel),
                                gfxFloat(width / twipsPerPixel),
                                gfxFloat(width / twipsPerPixel) };

  // start drawing
  gfxContext *ctx = aRenderingContext.ThebesContext();

  ctx->Save();

  nsCSSBorderRenderer br(twipsPerPixel,
                         ctx,
                         oRect,
                         outlineStyles,
                         outlineWidths,
                         outlineRadii,
                         outlineColors,
                         nullptr,
                         bgColor);
  br.DrawBorders();

  ctx->Restore();

  PrintAsStringNewline();
}

void
nsCSSRendering::PaintFocus(nsPresContext* aPresContext,
                           nsRenderingContext& aRenderingContext,
                           const nsRect& aFocusRect,
                           nscolor aColor)
{
  nscoord oneCSSPixel = nsPresContext::CSSPixelsToAppUnits(1);
  nscoord oneDevPixel = aPresContext->DevPixelsToAppUnits(1);

  gfxRect focusRect(nsLayoutUtils::RectToGfxRect(aFocusRect, oneDevPixel));

  gfxCornerSizes focusRadii;
  {
    nscoord twipsRadii[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    ComputePixelRadii(twipsRadii, oneDevPixel, &focusRadii);
  }
  gfxFloat focusWidths[4] = { gfxFloat(oneCSSPixel / oneDevPixel),
                              gfxFloat(oneCSSPixel / oneDevPixel),
                              gfxFloat(oneCSSPixel / oneDevPixel),
                              gfxFloat(oneCSSPixel / oneDevPixel) };

  uint8_t focusStyles[4] = { NS_STYLE_BORDER_STYLE_DOTTED,
                             NS_STYLE_BORDER_STYLE_DOTTED,
                             NS_STYLE_BORDER_STYLE_DOTTED,
                             NS_STYLE_BORDER_STYLE_DOTTED };
  nscolor focusColors[4] = { aColor, aColor, aColor, aColor };

  gfxContext *ctx = aRenderingContext.ThebesContext();

  ctx->Save();

  // Because this renders a dotted border, the background color
  // should not be used.  Therefore, we provide a value that will
  // be blatantly wrong if it ever does get used.  (If this becomes
  // something that CSS can style, this function will then have access
  // to a style context and can use the same logic that PaintBorder
  // and PaintOutline do.)
  nsCSSBorderRenderer br(oneDevPixel,
                         ctx,
                         focusRect,
                         focusStyles,
                         focusWidths,
                         focusRadii,
                         focusColors,
                         nullptr,
                         NS_RGB(255, 0, 0));
  br.DrawBorders();

  ctx->Restore();

  PrintAsStringNewline();
}

// Thebes Border Rendering Code End
//----------------------------------------------------------------------


//----------------------------------------------------------------------

/**
 * Computes the placement of a background image.
 *
 * @param aOriginBounds is the box to which the tiling position should be
 * relative
 * This should correspond to 'background-origin' for the frame,
 * except when painting on the canvas, in which case the origin bounds
 * should be the bounds of the root element's frame.
 * @param aTopLeft the top-left corner where an image tile should be drawn
 * @param aAnchorPoint a point which should be pixel-aligned by
 * nsLayoutUtils::DrawImage. This is the same as aTopLeft, unless CSS
 * specifies a percentage (including 'right' or 'bottom'), in which case
 * it's that percentage within of aOriginBounds. So 'right' would set
 * aAnchorPoint.x to aOriginBounds.XMost().
 *
 * Points are returned relative to aOriginBounds.
 */
static void
ComputeBackgroundAnchorPoint(const nsStyleBackground::Layer& aLayer,
                             const nsSize& aOriginBounds,
                             const nsSize& aImageSize,
                             nsPoint* aTopLeft,
                             nsPoint* aAnchorPoint)
{
  double percentX = aLayer.mPosition.mXPosition.mPercent;
  nscoord lengthX = aLayer.mPosition.mXPosition.mLength;
  aAnchorPoint->x = lengthX + NSToCoordRound(percentX*aOriginBounds.width);
  aTopLeft->x = lengthX +
    NSToCoordRound(percentX*(aOriginBounds.width - aImageSize.width));

  double percentY = aLayer.mPosition.mYPosition.mPercent;
  nscoord lengthY = aLayer.mPosition.mYPosition.mLength;
  aAnchorPoint->y = lengthY + NSToCoordRound(percentY*aOriginBounds.height);
  aTopLeft->y = lengthY +
    NSToCoordRound(percentY*(aOriginBounds.height - aImageSize.height));
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
  if (!content || content->Tag() != nsGkAtoms::body)
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
  const nsStyleBorder* styleBorder = aForFrame->StyleBorder();
  nsCSSShadowArray* shadows = styleBorder->mBoxShadow;
  if (!shadows)
    return;

  gfxContextAutoSaveRestore gfxStateRestorer;
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
  gfxCornerSizes borderRadii;
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

  gfxRect frameGfxRect(nsLayoutUtils::RectToGfxRect(frameRect, twipsPerPixel));
  frameGfxRect.Round();

  // We don't show anything that intersects with the frame we're blurring on. So tell the
  // blurrer not to do unnecessary work there.
  gfxRect skipGfxRect = frameGfxRect;
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

    gfxRect shadowGfxRectPlusBlur =
      nsLayoutUtils::RectToGfxRect(shadowRectPlusBlur, twipsPerPixel);
    shadowGfxRectPlusBlur.RoundOut();

    // Set the shadow color; if not specified, use the foreground color
    nscolor shadowColor;
    if (shadowItem->mHasColor)
      shadowColor = shadowItem->mColor;
    else
      shadowColor = aForFrame->StyleColor()->mColor;

    gfxRGBA gfxShadowColor(shadowColor);
    gfxShadowColor.a *= aOpacity;

    gfxContext* renderContext = aRenderingContext.ThebesContext();
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

      // shadowContext is owned by either blurringArea or aRenderingContext.
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
      nsRefPtr<nsRenderingContext> wrapperCtx = new nsRenderingContext();
      wrapperCtx->Init(shadowContext);
      gfxPoint devPixelOffset =
        nsLayoutUtils::PointToGfxPoint(nsPoint(shadowItem->mXOffset,
                                               shadowItem->mYOffset),
                                       aPresContext->AppUnitsPerDevPixel());
      wrapperCtx->ThebesContext()->SetMatrix(
        wrapperCtx->ThebesContext()->CurrentMatrix().Translate(devPixelOffset));

      nsRect nativeRect;
      nativeRect.IntersectRect(frameRect, aDirtyRect);
      aPresContext->GetTheme()->DrawWidgetBackground(wrapperCtx, aForFrame,
          styleDisplay->mAppearance, aFrameArea, nativeRect);

      blurringArea.DoPaint();
      renderContext->Restore();
    } else {
      renderContext->Save();
      // Clip out the area of the actual frame so the shadow is not shown within
      // the frame.
      renderContext->NewPath();
      renderContext->Rectangle(shadowGfxRectPlusBlur);
      if (hasBorderRadius) {
        renderContext->RoundedRectangle(frameGfxRect, borderRadii);
      } else {
        renderContext->Rectangle(frameGfxRect);
      }

      renderContext->SetFillRule(FillRule::FILL_EVEN_ODD);
      renderContext->Clip();

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
      aRenderingContext.ThebesContext()->
        Clip(NSRectToRect(fragmentClip,
                          aForFrame->PresContext()->AppUnitsPerDevPixel(),
                          *aRenderingContext.GetDrawTarget()));

      gfxCornerSizes clipRectRadii;
      if (hasBorderRadius) {
        gfxFloat spreadDistance = shadowItem->mSpread / twipsPerPixel;

        gfxFloat borderSizes[4];

        borderSizes[NS_SIDE_LEFT] = spreadDistance;
        borderSizes[NS_SIDE_TOP] = spreadDistance;
        borderSizes[NS_SIDE_RIGHT] = spreadDistance;
        borderSizes[NS_SIDE_BOTTOM] = spreadDistance;

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
                                    const nsRect& aFrameArea,
                                    const nsRect& aDirtyRect)
{
  const nsStyleBorder* styleBorder = aForFrame->StyleBorder();
  nsCSSShadowArray* shadows = styleBorder->mBoxShadow;
  if (!shadows)
    return;
  if (aForFrame->IsThemed() && aForFrame->GetContent() &&
      !nsContentUtils::IsChromeDoc(aForFrame->GetContent()->GetCurrentDoc())) {
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

  gfxCornerSizes innerRadii;
  if (hasBorderRadius) {
    gfxCornerSizes borderRadii;

    ComputePixelRadii(twipsRadii, twipsPerPixel, &borderRadii);
    gfxFloat borderSizes[4] = {
      gfxFloat(border.top / twipsPerPixel),
      gfxFloat(border.right / twipsPerPixel),
      gfxFloat(border.bottom / twipsPerPixel),
      gfxFloat(border.left / twipsPerPixel)
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

    nsRect shadowClipRect = paddingRect;
    shadowClipRect.MoveBy(shadowItem->mXOffset, shadowItem->mYOffset);
    shadowClipRect.Deflate(shadowItem->mSpread, shadowItem->mSpread);

    gfxCornerSizes clipRectRadii;
    if (hasBorderRadius) {
      // Calculate the radii the inner clipping rect will have
      gfxFloat spreadDistance = shadowItem->mSpread / twipsPerPixel;
      gfxFloat borderSizes[4] = {0, 0, 0, 0};

      // See PaintBoxShadowOuter and bug 514670
      if (innerRadii[C_TL].width > 0 || innerRadii[C_BL].width > 0) {
        borderSizes[NS_SIDE_LEFT] = spreadDistance;
      }

      if (innerRadii[C_TL].height > 0 || innerRadii[C_TR].height > 0) {
        borderSizes[NS_SIDE_TOP] = spreadDistance;
      }

      if (innerRadii[C_TR].width > 0 || innerRadii[C_BR].width > 0) {
        borderSizes[NS_SIDE_RIGHT] = spreadDistance;
      }

      if (innerRadii[C_BL].height > 0 || innerRadii[C_BR].height > 0) {
        borderSizes[NS_SIDE_BOTTOM] = spreadDistance;
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
    nsContextBoxBlur blurringArea;
    gfxContext* shadowContext =
      blurringArea.Init(shadowPaintRect, 0, blurRadius, twipsPerPixel,
                        renderContext, aDirtyRect, &skipGfxRect);
    if (!shadowContext)
      continue;

    // shadowContext is owned by either blurringArea or aRenderingContext.
    MOZ_ASSERT(shadowContext == renderContext ||
               shadowContext == blurringArea.GetContext());

    // Set the shadow color; if not specified, use the foreground color
    nscolor shadowColor;
    if (shadowItem->mHasColor)
      shadowColor = shadowItem->mColor;
    else
      shadowColor = aForFrame->StyleColor()->mColor;

    renderContext->Save();
    renderContext->SetColor(gfxRGBA(shadowColor));

    // Clip the context to the area of the frame's padding rect, so no part of the
    // shadow is painted outside. Also cut out anything beyond where the inset shadow
    // will be.
    gfxRect shadowGfxRect =
      nsLayoutUtils::RectToGfxRect(paddingRect, twipsPerPixel);
    shadowGfxRect.Round();
    renderContext->NewPath();
    if (hasBorderRadius)
      renderContext->RoundedRectangle(shadowGfxRect, innerRadii, false);
    else
      renderContext->Rectangle(shadowGfxRect);
    renderContext->Clip();

    // Fill the surface minus the area within the frame that we should
    // not paint in, and blur and apply it.
    gfxRect shadowPaintGfxRect =
      nsLayoutUtils::RectToGfxRect(shadowPaintRect, twipsPerPixel);
    shadowPaintGfxRect.RoundOut();
    gfxRect shadowClipGfxRect =
      nsLayoutUtils::RectToGfxRect(shadowClipRect, twipsPerPixel);
    shadowClipGfxRect.Round();
    shadowContext->NewPath();
    shadowContext->Rectangle(shadowPaintGfxRect);
    if (hasBorderRadius)
      shadowContext->RoundedRectangle(shadowClipGfxRect, clipRectRadii, false);
    else
      shadowContext->Rectangle(shadowClipGfxRect);
    shadowContext->SetFillRule(FillRule::FILL_EVEN_ODD);
    shadowContext->Fill();

    blurringArea.DoPaint();
    renderContext->Restore();
  }
}

void
nsCSSRendering::PaintBackground(nsPresContext* aPresContext,
                                nsRenderingContext& aRenderingContext,
                                nsIFrame* aForFrame,
                                const nsRect& aDirtyRect,
                                const nsRect& aBorderArea,
                                uint32_t aFlags,
                                nsRect* aBGClipRect,
                                int32_t aLayer)
{
  PROFILER_LABEL("nsCSSRendering", "PaintBackground",
    js::ProfileEntry::Category::GRAPHICS);

  NS_PRECONDITION(aForFrame,
                  "Frame is expected to be provided to PaintBackground");

  nsStyleContext *sc;
  if (!FindBackground(aForFrame, &sc)) {
    // We don't want to bail out if moz-appearance is set on a root
    // node. If it has a parent content node, bail because it's not
    // a root, otherwise keep going in order to let the theme stuff
    // draw the background. The canvas really should be drawing the
    // bg, but there's no way to hook that up via css.
    if (!aForFrame->StyleDisplay()->mAppearance) {
      return;
    }

    nsIContent* content = aForFrame->GetContent();
    if (!content || content->GetParent()) {
      return;
    }

    sc = aForFrame->StyleContext();
  }

  PaintBackgroundWithSC(aPresContext, aRenderingContext, aForFrame,
                        aDirtyRect, aBorderArea, sc,
                        *aForFrame->StyleBorder(), aFlags,
                        aBGClipRect, aLayer);
}

static bool
IsOpaqueBorderEdge(const nsStyleBorder& aBorder, mozilla::css::Side aSide)
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

  nscolor color;
  bool isForeground;
  aBorder.GetBorderColor(aSide, color, isForeground);

  // We don't know the foreground color here, so if it's being used
  // we must assume it might be transparent.
  if (isForeground)
    return false;

  return NS_GET_A(color) == 255;
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
  NS_WARN_IF_FALSE(aDirtyRect->IsEmpty() || !aDirtyRectGfx->IsEmpty(),
                   "converted dirty rect should not be empty");
  NS_ABORT_IF_FALSE(!aDirtyRect->IsEmpty() || aDirtyRectGfx->IsEmpty(),
                    "second should be empty if first is");
}

/* static */ void
nsCSSRendering::GetBackgroundClip(const nsStyleBackground::Layer& aLayer,
                                  nsIFrame* aForFrame, const nsStyleBorder& aBorder,
                                  const nsRect& aBorderArea, const nsRect& aCallerDirtyRect,
                                  bool aWillPaintBorder, nscoord aAppUnitsPerPixel,
                                  /* out */ BackgroundClipState* aClipState)
{
  // Compute the outermost boundary of the area that might be painted.
  // Same coordinate space as aBorderArea.
  Sides skipSides = aForFrame->GetSkipSides();
  nsRect clipBorderArea =
    ::BoxDecorationRectForBorder(aForFrame, aBorderArea, skipSides, &aBorder);

  bool haveRoundedCorners = GetRadii(aForFrame, aBorder, aBorderArea,
                                     clipBorderArea, aClipState->mRadii);

  uint8_t backgroundClip = aLayer.mClip;

  bool isSolidBorder =
      aWillPaintBorder && IsOpaqueBorder(aBorder);
  if (isSolidBorder && backgroundClip == NS_STYLE_BG_CLIP_BORDER) {
    // If we have rounded corners, we need to inflate the background
    // drawing area a bit to avoid seams between the border and
    // background.
    backgroundClip = haveRoundedCorners ?
      NS_STYLE_BG_CLIP_MOZ_ALMOST_PADDING : NS_STYLE_BG_CLIP_PADDING;
  }

  aClipState->mBGClipArea = clipBorderArea;
  aClipState->mHasAdditionalBGClipArea = false;
  aClipState->mCustomClip = false;

  if (aForFrame->GetType() == nsGkAtoms::scrollFrame &&
      NS_STYLE_BG_ATTACHMENT_LOCAL == aLayer.mAttachment) {
    // As of this writing, this is still in discussion in the CSS Working Group
    // http://lists.w3.org/Archives/Public/www-style/2013Jul/0250.html

    // The rectangle for 'background-clip' scrolls with the content,
    // but the background is also clipped at a non-scrolling 'padding-box'
    // like the content. (See below.)
    // Therefore, only 'content-box' makes a difference here.
    if (backgroundClip == NS_STYLE_BG_CLIP_CONTENT) {
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
    backgroundClip = NS_STYLE_BG_CLIP_PADDING;
  }

  if (backgroundClip != NS_STYLE_BG_CLIP_BORDER) {
    nsMargin border = aForFrame->GetUsedBorder();
    if (backgroundClip == NS_STYLE_BG_CLIP_MOZ_ALMOST_PADDING) {
      // Reduce |border| by 1px (device pixels) on all sides, if
      // possible, so that we don't get antialiasing seams between the
      // background and border.
      border.top = std::max(0, border.top - aAppUnitsPerPixel);
      border.right = std::max(0, border.right - aAppUnitsPerPixel);
      border.bottom = std::max(0, border.bottom - aAppUnitsPerPixel);
      border.left = std::max(0, border.left - aAppUnitsPerPixel);
    } else if (backgroundClip != NS_STYLE_BG_CLIP_PADDING) {
      NS_ASSERTION(backgroundClip == NS_STYLE_BG_CLIP_CONTENT,
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
SetupBackgroundClip(nsCSSRendering::BackgroundClipState& aClipState,
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
    gfxRect bgAreaGfx =
      nsLayoutUtils::RectToGfxRect(aClipState.mBGClipArea, aAppUnitsPerPixel);
    bgAreaGfx.Round();
    bgAreaGfx.Condition();

    if (bgAreaGfx.IsEmpty()) {
      // I think it's become possible to hit this since
      // http://hg.mozilla.org/mozilla-central/rev/50e934e4979b landed.
      NS_WARNING("converted background area should not be empty");
      // Make our caller not do anything.
      aClipState.mDirtyRectGfx.SizeTo(gfxSize(0.0, 0.0));
      return;
    }

    aAutoSR->EnsureSaved(aCtx);
    aCtx->NewPath();
    aCtx->RoundedRectangle(bgAreaGfx, aClipState.mClippedRadii);
    aCtx->Clip();
  }
}

static void
DrawBackgroundColor(nsCSSRendering::BackgroundClipState& aClipState,
                    gfxContext *aCtx, nscoord aAppUnitsPerPixel)
{
  if (aClipState.mDirtyRectGfx.IsEmpty()) {
    // Our caller won't draw anything under this condition, so no need
    // to set more up.
    return;
  }

  // We don't support custom clips and rounded corners, arguably a bug, but
  // table painting seems to depend on it.
  if (!aClipState.mHasRoundedCorners || aClipState.mCustomClip) {
    aCtx->NewPath();
    aCtx->Rectangle(aClipState.mDirtyRectGfx, true);
    aCtx->Fill();
    return;
  }

  gfxRect bgAreaGfx =
    nsLayoutUtils::RectToGfxRect(aClipState.mBGClipArea, aAppUnitsPerPixel);
  bgAreaGfx.Round();
  bgAreaGfx.Condition();

  if (bgAreaGfx.IsEmpty()) {
    // I think it's become possible to hit this since
    // http://hg.mozilla.org/mozilla-central/rev/50e934e4979b landed.
    NS_WARNING("converted background area should not be empty");
    // Make our caller not do anything.
    aClipState.mDirtyRectGfx.SizeTo(gfxSize(0.0, 0.0));
    return;
  }

  aCtx->Save();
  gfxRect dirty = bgAreaGfx.Intersect(aClipState.mDirtyRectGfx);

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

  aCtx->NewPath();
  aCtx->RoundedRectangle(bgAreaGfx, aClipState.mClippedRadii);

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

  if (aFrame->HonorPrintBackgroundSettings()) {
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
  if (aDrawBackgroundColor &&
      bg->BottomLayer().mRepeat.mXRepeat == NS_STYLE_BG_REPEAT_REPEAT &&
      bg->BottomLayer().mRepeat.mYRepeat == NS_STYLE_BG_REPEAT_REPEAT &&
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
    NS_ABORT_IF_FALSE(false, "unknown radial gradient sizing method");
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

// Returns aFrac*aC2 + (1 - aFrac)*C1. The interpolation is done
// in unpremultiplied space, which is what SVG gradients and cairo
// gradients expect.
static gfxRGBA
InterpolateColor(const gfxRGBA& aC1, const gfxRGBA& aC2, double aFrac)
{
  double other = 1 - aFrac;
  return gfxRGBA(aC2.r*aFrac + aC1.r*other,
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
                               gfxRGBA* aOutEdgeColor)
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

void
nsCSSRendering::PaintGradient(nsPresContext* aPresContext,
                              nsRenderingContext& aRenderingContext,
                              nsStyleGradient* aGradient,
                              const nsRect& aDirtyRect,
                              const nsRect& aDest,
                              const nsRect& aFillArea,
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
  gfxFloat lineLength = NS_hypot(lineEnd.x - lineStart.x,
                                  lineEnd.y - lineStart.y);

  NS_ABORT_IF_FALSE(aGradient->mStops.Length() >= 2,
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
        stops.AppendElement(ColorStop(0, stop.mColor));
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
      NS_ABORT_IF_FALSE(false, "Unknown stop position type");
    }

    if (i > 0) {
      // Prevent decreasing stop positions by advancing this position
      // to the previous stop position, if necessary
      position = std::max(position, stops[i - 1].mPosition);
    }
    stops.AppendElement(ColorStop(position, stop.mColor));
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
              double frac = (0.0 - pos)/(nextPos - pos);
              stops[i].mColor =
                InterpolateColor(stops[i].mColor, stops[i + 1].mColor, frac);
            }
          }
        }
      }
    }
    firstStop = stops[0].mPosition;
    NS_ABORT_IF_FALSE(firstStop >= 0.0, "Failed to fix stop offsets");
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
  nsRefPtr<gfxPattern> gradientPattern;
  bool forceRepeatToCoverTiles = false;
  gfxMatrix matrix;
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

    // When the gradient line is parallel to the x axis from the left edge
    // to the right edge of a tile, then we can repeat by just repeating the
    // gradient.
    if (!cellContainsFill &&
        ((gradientStopStart.y == gradientStopEnd.y && gradientStopStart.x == 0 &&
          gradientStopEnd.x == srcSize.width) ||
          (gradientStopStart.x == gradientStopEnd.x && gradientStopStart.y == 0 &&
          gradientStopEnd.y == srcSize.height))) {
      forceRepeatToCoverTiles = true;
    }
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
    gfxRGBA firstColor(stops[0].mColor);
    gfxRGBA lastColor(stops.LastElement().mColor);
    stops.Clear();

    if (!aGradient->mRepeating && !zeroRadius) {
      stops.AppendElement(ColorStop(firstStop, firstColor));
    }
    stops.AppendElement(ColorStop(firstStop, lastColor));
  }

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
    rawStops[i].color = gfx::Color(stops[i].mColor.r, stops[i].mColor.g, stops[i].mColor.b, stops[i].mColor.a);
    rawStops[i].offset = stopScale * (stops[i].mPosition - stopOrigin);
  }
  mozilla::RefPtr<mozilla::gfx::GradientStops> gs =
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
  nscoord xStart = FindTileStart(dirty.x, aDest.x, aDest.width);
  nscoord yStart = FindTileStart(dirty.y, aDest.y, aDest.height);
  nscoord xEnd = forceRepeatToCoverTiles ? xStart + aDest.width : dirty.XMost();
  nscoord yEnd = forceRepeatToCoverTiles ? yStart + aDest.height : dirty.YMost();

  // x and y are the top-left corner of the tile to draw
  for (nscoord y = yStart; y < yEnd; y += aDest.height) {
    for (nscoord x = xStart; x < xEnd; x += aDest.width) {
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
      gfxRGBA edgeColor;
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

void
nsCSSRendering::PaintBackgroundWithSC(nsPresContext* aPresContext,
                                      nsRenderingContext& aRenderingContext,
                                      nsIFrame* aForFrame,
                                      const nsRect& aDirtyRect,
                                      const nsRect& aBorderArea,
                                      nsStyleContext* aBackgroundSC,
                                      const nsStyleBorder& aBorder,
                                      uint32_t aFlags,
                                      nsRect* aBGClipRect,
                                      int32_t aLayer)
{
  NS_PRECONDITION(aForFrame,
                  "Frame is expected to be provided to PaintBackground");

  // Check to see if we have an appearance defined.  If so, we let the theme
  // renderer draw the background and bail out.
  // XXXzw this ignores aBGClipRect.
  const nsStyleDisplay* displayData = aForFrame->StyleDisplay();
  if (displayData->mAppearance) {
    nsITheme *theme = aPresContext->GetTheme();
    if (theme && theme->ThemeSupportsWidget(aPresContext, aForFrame,
                                            displayData->mAppearance)) {
      nsRect drawing(aBorderArea);
      theme->GetWidgetOverflow(aPresContext->DeviceContext(),
                               aForFrame, displayData->mAppearance, &drawing);
      drawing.IntersectRect(drawing, aDirtyRect);
      theme->DrawWidgetBackground(&aRenderingContext, aForFrame,
                                  displayData->mAppearance, aBorderArea,
                                  drawing);
      return;
    }
  }

  // For canvas frames (in the CSS sense) we draw the background color using
  // a solid color item that gets added in nsLayoutUtils::PaintFrame,
  // or nsSubDocumentFrame::BuildDisplayList (bug 488242). (The solid
  // color may be moved into nsDisplayCanvasBackground by
  // nsPresShell::AddCanvasBackgroundColorItem, and painted by
  // nsDisplayCanvasBackground directly.) Either way we don't need to
  // paint the background color here.
  bool isCanvasFrame = IsCanvasFrame(aForFrame);

  // Determine whether we are drawing background images and/or
  // background colors.
  bool drawBackgroundImage;
  bool drawBackgroundColor;

  nscolor bgColor = DetermineBackgroundColor(aPresContext,
                                             aBackgroundSC,
                                             aForFrame,
                                             drawBackgroundImage,
                                             drawBackgroundColor);

  // If we're drawing a specific layer, we don't want to draw the
  // background color.
  const nsStyleBackground *bg = aBackgroundSC->StyleBackground();
  if (drawBackgroundColor && aLayer >= 0) {
    drawBackgroundColor = false;
  }

  // At this point, drawBackgroundImage and drawBackgroundColor are
  // true if and only if we are actually supposed to paint an image or
  // color into aDirtyRect, respectively.
  if (!drawBackgroundImage && !drawBackgroundColor)
    return;

  // Compute the outermost boundary of the area that might be painted.
  // Same coordinate space as aBorderArea & aBGClipRect.
  Sides skipSides = aForFrame->GetSkipSides();
  nsRect paintBorderArea =
    ::BoxDecorationRectForBackground(aForFrame, aBorderArea, skipSides, &aBorder);
  nsRect clipBorderArea =
    ::BoxDecorationRectForBorder(aForFrame, aBorderArea, skipSides, &aBorder);

  // The 'bgClipArea' (used only by the image tiling logic, far below)
  // is the caller-provided aBGClipRect if any, or else the area
  // determined by the value of 'background-clip' in
  // SetupCurrentBackgroundClip.  (Arguably it should be the
  // intersection, but that breaks the table painter -- in particular,
  // taking the intersection breaks reftests/bugs/403249-1[ab].)
  gfxContext* ctx = aRenderingContext.ThebesContext();
  nscoord appUnitsPerPixel = aPresContext->AppUnitsPerDevPixel();
  BackgroundClipState clipState;
  if (aBGClipRect) {
    clipState.mBGClipArea = *aBGClipRect;
    clipState.mCustomClip = true;
    clipState.mHasRoundedCorners = false;
    SetupDirtyRects(clipState.mBGClipArea, aDirtyRect, appUnitsPerPixel,
                    &clipState.mDirtyRect, &clipState.mDirtyRectGfx);
  } else {
    GetBackgroundClip(bg->BottomLayer(),
                      aForFrame, aBorder, aBorderArea,
                      aDirtyRect, (aFlags & PAINTBG_WILL_PAINT_BORDER), appUnitsPerPixel,
                      &clipState);
  }

  // If we might be using a background color, go ahead and set it now.
  if (drawBackgroundColor && !isCanvasFrame)
    ctx->SetColor(gfxRGBA(bgColor));

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
    return;
  }

  if (bg->mImageCount < 1) {
    // Return if there are no background layers, all work from this point
    // onwards happens iteratively on these.
    return;
  }

  // Validate the layer range before we start iterating.
  int32_t startLayer = aLayer;
  int32_t nLayers = 1;
  if (startLayer < 0) {
    startLayer = (int32_t)bg->mImageCount - 1;
    nLayers = bg->mImageCount;
  }

  // Ensure we get invalidated for loads of the image.  We need to do
  // this here because this might be the only code that knows about the
  // association of the style data with the frame.
  if (aBackgroundSC != aForFrame->StyleContext()) {
    NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT_WITH_RANGE(i, bg, startLayer, nLayers) {
      aForFrame->AssociateImage(bg->mLayers[i].mImage, aPresContext);
    }
  }

  // The background color is rendered over the entire dirty area,
  // even if the image isn't.
  if (drawBackgroundColor && !isCanvasFrame) {
    DrawBackgroundColor(clipState, ctx, appUnitsPerPixel);
  }

  if (drawBackgroundImage) {
    bool clipSet = false;
    uint8_t currentBackgroundClip = NS_STYLE_BG_CLIP_BORDER;
    NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT_WITH_RANGE(i, bg, bg->mImageCount - 1,
                                                              nLayers + (bg->mImageCount -
                                                                         startLayer - 1)) {
      const nsStyleBackground::Layer &layer = bg->mLayers[i];
      if (!aBGClipRect) {
        if (currentBackgroundClip != layer.mClip || !clipSet) {
          currentBackgroundClip = layer.mClip;
          // If clipSet is false that means this is the bottom layer and we
          // already called GetBackgroundClip above and it stored its results
          // in clipState.
          if (clipSet) {
            autoSR.Restore(); // reset the previous one
            GetBackgroundClip(layer, aForFrame,
                              aBorder, aBorderArea, aDirtyRect, (aFlags & PAINTBG_WILL_PAINT_BORDER),
                              appUnitsPerPixel, &clipState);
          }
          SetupBackgroundClip(clipState, ctx, appUnitsPerPixel, &autoSR);
          clipSet = true;
          if (!clipBorderArea.IsEqualEdges(aBorderArea)) {
            // We're drawing the background for the joined continuation boxes
            // so we need to clip that to the slice that we want for this frame.
            gfxRect clip =
              nsLayoutUtils::RectToGfxRect(aBorderArea, appUnitsPerPixel);
            autoSR.EnsureSaved(ctx);
            ctx->NewPath();
            ctx->SnappedRectangle(clip);
            ctx->Clip();
          }
        }
      }
      if ((aLayer < 0 || i == (uint32_t)startLayer) &&
          !clipState.mDirtyRectGfx.IsEmpty()) {
        nsBackgroundLayerState state = PrepareBackgroundLayer(aPresContext, aForFrame,
            aFlags, paintBorderArea, clipState.mBGClipArea, layer);
        if (!state.mFillArea.IsEmpty()) {
          if (state.mCompositingOp != gfxContext::OPERATOR_OVER) {
            NS_ASSERTION(ctx->CurrentOperator() == gfxContext::OPERATOR_OVER,
                         "It is assumed the initial operator is OPERATOR_OVER, when it is restored later");
            ctx->SetOperator(state.mCompositingOp);
          }
          state.mImageRenderer.DrawBackground(aPresContext, aRenderingContext,
                                              state.mDestArea, state.mFillArea,
                                              state.mAnchor + paintBorderArea.TopLeft(),
                                              clipState.mDirtyRect);
          if (state.mCompositingOp != gfxContext::OPERATOR_OVER) {
            ctx->SetOperator(gfxContext::OPERATOR_OVER);
          }
        }
      }
    }
  }
}

static inline bool
IsTransformed(nsIFrame* aForFrame, nsIFrame* aTopFrame)
{
  for (nsIFrame* f = aForFrame; f != aTopFrame; f = f->GetParent()) {
    if (f->IsTransformed()) {
      return true;
    }
  }
  return false;
}

nsRect
nsCSSRendering::ComputeBackgroundPositioningArea(nsPresContext* aPresContext,
                                                 nsIFrame* aForFrame,
                                                 const nsRect& aBorderArea,
                                                 const nsStyleBackground::Layer& aLayer,
                                                 nsIFrame** aAttachedToFrame)
{
  // Compute background origin area relative to aBorderArea now as we may need
  // it to compute the effective image size for a CSS gradient.
  nsRect bgPositioningArea;

  nsIAtom* frameType = aForFrame->GetType();
  nsIFrame* geometryFrame = aForFrame;
  if (MOZ_UNLIKELY(frameType == nsGkAtoms::scrollFrame &&
                   NS_STYLE_BG_ATTACHMENT_LOCAL == aLayer.mAttachment)) {
    nsIScrollableFrame* scrollableFrame = do_QueryFrame(aForFrame);
    bgPositioningArea = nsRect(
      scrollableFrame->GetScrolledFrame()->GetPosition()
        // For the dir=rtl case:
        + scrollableFrame->GetScrollRange().TopLeft(),
      scrollableFrame->GetScrolledRect().Size());
    // The ScrolledRect’s size does not include the borders or scrollbars,
    // reverse the handling of background-origin
    // compared to the common case below.
    if (aLayer.mOrigin == NS_STYLE_BG_ORIGIN_BORDER) {
      nsMargin border = geometryFrame->GetUsedBorder();
      border.ApplySkipSides(geometryFrame->GetSkipSides());
      bgPositioningArea.Inflate(border);
      bgPositioningArea.Inflate(scrollableFrame->GetActualScrollbarSizes());
    } else if (aLayer.mOrigin != NS_STYLE_BG_ORIGIN_PADDING) {
      nsMargin padding = geometryFrame->GetUsedPadding();
      padding.ApplySkipSides(geometryFrame->GetSkipSides());
      bgPositioningArea.Deflate(padding);
      NS_ASSERTION(aLayer.mOrigin == NS_STYLE_BG_ORIGIN_CONTENT,
                   "unknown background-origin value");
    }
    *aAttachedToFrame = aForFrame;
    return bgPositioningArea;
  }

  if (MOZ_UNLIKELY(frameType == nsGkAtoms::canvasFrame)) {
    geometryFrame = aForFrame->GetFirstPrincipalChild();
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
  if (aLayer.mOrigin != NS_STYLE_BG_ORIGIN_BORDER && geometryFrame) {
    nsMargin border = geometryFrame->GetUsedBorder();
    if (aLayer.mOrigin != NS_STYLE_BG_ORIGIN_PADDING) {
      border += geometryFrame->GetUsedPadding();
      NS_ASSERTION(aLayer.mOrigin == NS_STYLE_BG_ORIGIN_CONTENT,
                   "unknown background-origin value");
    }
    bgPositioningArea.Deflate(border);
  }

  nsIFrame* attachedToFrame = aForFrame;
  if (NS_STYLE_BG_ATTACHMENT_FIXED == aLayer.mAttachment) {
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
  *aAttachedToFrame = attachedToFrame;

  return bgPositioningArea;
}

// Apply the CSS image sizing algorithm as it applies to background images.
// See http://www.w3.org/TR/css3-background/#the-background-size .
// aIntrinsicSize is the size that the background image 'would like to be'.
// It can be found by calling nsImageRenderer::ComputeIntrinsicSize.
static nsSize
ComputeDrawnSizeForBackground(const CSSSizeOrRatio& aIntrinsicSize,
                              const nsSize& aBgPositioningArea,
                              const nsStyleBackground::Size& aLayerSize)
{
  // Size is dictated by cover or contain rules.
  if (aLayerSize.mWidthType == nsStyleBackground::Size::eContain ||
      aLayerSize.mWidthType == nsStyleBackground::Size::eCover) {
    nsImageRenderer::FitType fitType =
      aLayerSize.mWidthType == nsStyleBackground::Size::eCover
        ? nsImageRenderer::COVER
        : nsImageRenderer::CONTAIN;
    return nsImageRenderer::ComputeConstrainedSize(aBgPositioningArea,
                                                   aIntrinsicSize.mRatio,
                                                   fitType);
  }

  // No cover/contain constraint, use default algorithm.
  CSSSizeOrRatio specifiedSize;
  if (aLayerSize.mWidthType == nsStyleBackground::Size::eLengthPercentage) {
    specifiedSize.SetWidth(
      aLayerSize.ResolveWidthLengthPercentage(aBgPositioningArea));
  }
  if (aLayerSize.mHeightType == nsStyleBackground::Size::eLengthPercentage) {
    specifiedSize.SetHeight(
      aLayerSize.ResolveHeightLengthPercentage(aBgPositioningArea));
  }

  return nsImageRenderer::ComputeConcreteSize(specifiedSize,
                                              aIntrinsicSize,
                                              aBgPositioningArea);
}

nsBackgroundLayerState
nsCSSRendering::PrepareBackgroundLayer(nsPresContext* aPresContext,
                                       nsIFrame* aForFrame,
                                       uint32_t aFlags,
                                       const nsRect& aBorderArea,
                                       const nsRect& aBGClipRect,
                                       const nsStyleBackground::Layer& aLayer)
{
  /*
   * The properties we need to keep in mind when drawing background
   * layers are:
   *
   *   background-image
   *   background-repeat
   *   background-attachment
   *   background-position
   *   background-clip
   *   background-origin
   *   background-size
   *   background-blend-mode
   *   box-decoration-break
   *
   * (background-color applies to the entire element and not to individual
   * layers, so it is irrelevant to this method.)
   *
   * These properties have the following dependencies upon each other when
   * determining rendering:
   *
   *   background-image
   *     no dependencies
   *   background-repeat
   *     no dependencies
   *   background-attachment
   *     no dependencies
   *   background-position
   *     depends upon background-size (for the image's scaled size) and
   *     background-break (for the background positioning area)
   *   background-clip
   *     no dependencies
   *   background-origin
   *     depends upon background-attachment (only in the case where that value
   *     is 'fixed')
   *   background-size
   *     depends upon box-decoration-break (for the background positioning area
   *     for resolving percentages), background-image (for the image's intrinsic
   *     size), background-repeat (if that value is 'round'), and
   *     background-origin (for the background painting area, when
   *     background-repeat is 'round')
   *   box-decoration-break
   *     no dependencies
   *
   * As a result of only-if dependencies we don't strictly do a topological
   * sort of the above properties when processing, but it's pretty close to one:
   *
   *   background-clip (by caller)
   *   background-image
   *   box-decoration-break, background-origin
   *   background-attachment (postfix for background-origin if 'fixed')
   *   background-size
   *   background-position
   *   background-repeat
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
    return state;
  }

  // The frame to which the background is attached
  nsIFrame* attachedToFrame = aForFrame;
  // Compute background origin area relative to aBorderArea now as we may need
  // it to compute the effective image size for a CSS gradient.
  nsRect bgPositioningArea =
    ComputeBackgroundPositioningArea(aPresContext, aForFrame, aBorderArea,
                                     aLayer, &attachedToFrame);

  // For background-attachment:fixed backgrounds, we'll limit the area
  // where the background can be drawn to the viewport.
  nsRect bgClipRect = aBGClipRect;

  // Compute the anchor point.
  //
  // relative to aBorderArea.TopLeft() (which is where the top-left
  // of aForFrame's border-box will be rendered)
  nsPoint imageTopLeft;
  if (NS_STYLE_BG_ATTACHMENT_FIXED == aLayer.mAttachment) {
    if ((aFlags & nsCSSRendering::PAINTBG_TO_WINDOW) &&
        !IsTransformed(aForFrame, attachedToFrame)) {
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

  // Scale the image as specified for background-size and as required for
  // proper background positioning when background-position is defined with
  // percentages.
  CSSSizeOrRatio intrinsicSize = state.mImageRenderer.ComputeIntrinsicSize();
  nsSize bgPositionSize = bgPositioningArea.Size();
  nsSize imageSize = ComputeDrawnSizeForBackground(intrinsicSize,
                                                   bgPositionSize,
                                                   aLayer.mSize);
  if (imageSize.width <= 0 || imageSize.height <= 0)
    return state;

  state.mImageRenderer.SetPreferredSize(intrinsicSize,
                                        imageSize);

  // Compute the position of the background now that the background's size is
  // determined.
  ComputeBackgroundAnchorPoint(aLayer, bgPositionSize, imageSize,
                               &imageTopLeft, &state.mAnchor);
  imageTopLeft += bgPositioningArea.TopLeft();
  state.mAnchor += bgPositioningArea.TopLeft();

  state.mDestArea = nsRect(imageTopLeft + aBorderArea.TopLeft(), imageSize);
  state.mFillArea = state.mDestArea;
  int repeatX = aLayer.mRepeat.mXRepeat;
  int repeatY = aLayer.mRepeat.mYRepeat;
  if (repeatX == NS_STYLE_BG_REPEAT_REPEAT) {
    state.mFillArea.x = bgClipRect.x;
    state.mFillArea.width = bgClipRect.width;
  }
  if (repeatY == NS_STYLE_BG_REPEAT_REPEAT) {
    state.mFillArea.y = bgClipRect.y;
    state.mFillArea.height = bgClipRect.height;
  }
  state.mFillArea.IntersectRect(state.mFillArea, bgClipRect);

  state.mCompositingOp = GetGFXBlendMode(aLayer.mBlendMode);

  return state;
}

nsRect
nsCSSRendering::GetBackgroundLayerRect(nsPresContext* aPresContext,
                                       nsIFrame* aForFrame,
                                       const nsRect& aBorderArea,
                                       const nsRect& aClipRect,
                                       const nsStyleBackground::Layer& aLayer,
                                       uint32_t aFlags)
{
  Sides skipSides = aForFrame->GetSkipSides();
  nsRect borderArea =
    ::BoxDecorationRectForBackground(aForFrame, aBorderArea, skipSides);
  nsBackgroundLayerState state =
      PrepareBackgroundLayer(aPresContext, aForFrame, aFlags, borderArea,
                             aClipRect, aLayer);
  return state.mFillArea;
}

/* static */ bool
nsCSSRendering::IsBackgroundImageDecodedForStyleContextAndLayer(
  const nsStyleBackground *aBackground, uint32_t aLayer)
{
  const nsStyleImage* image = &aBackground->mLayers[aLayer].mImage;
  if (image->GetType() == eStyleImageType_Image) {
    nsCOMPtr<imgIContainer> img;
    if (NS_SUCCEEDED(image->GetImageData()->GetImage(getter_AddRefs(img)))) {
      if (!img->IsDecoded()) {
        return false;
      }
    }
  }
  return true;
}

/* static */ bool
nsCSSRendering::AreAllBackgroundImagesDecodedForFrame(nsIFrame* aFrame)
{
  const nsStyleBackground *bg = aFrame->StyleContext()->StyleBackground();
  NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(i, bg) {
    if (!IsBackgroundImageDecodedForStyleContextAndLayer(bg, i)) {
      return false;
    }
  }
  return true;
}

static void
DrawBorderImage(nsPresContext*       aPresContext,
                nsRenderingContext&  aRenderingContext,
                nsIFrame*            aForFrame,
                const nsRect&        aBorderArea,
                const nsStyleBorder& aStyleBorder,
                const nsRect&        aDirtyRect,
                Sides                aSkipSides)
{
  NS_PRECONDITION(aStyleBorder.IsBorderImageLoaded(),
                  "drawing border image that isn't successfully loaded");

  if (aDirtyRect.IsEmpty())
    return;

  nsImageRenderer renderer(aForFrame, &aStyleBorder.mBorderImageSource, 0);

  // Ensure we get invalidated for loads and animations of the image.
  // We need to do this here because this might be the only code that
  // knows about the association of the style data with the frame.
  // XXX We shouldn't really... since if anybody is passing in a
  // different style, they'll potentially have the wrong size for the
  // border too.
  aForFrame->AssociateImage(aStyleBorder.mBorderImageSource, aPresContext);

  if (!renderer.PrepareImage()) {
    return;
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
        Clip(NSRectToRect(clip, aForFrame->PresContext()->AppUnitsPerDevPixel(),
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
      nsIntRect intSubArea = subArea.ToOutsidePixels(nsPresContext::AppUnitsPerCSSPixel());

      renderer.DrawBorderImageComponent(aPresContext,
                                        aRenderingContext, aDirtyRect,
                                        destArea, CSSIntRect(intSubArea.x,
                                                             intSubArea.y,
                                                             intSubArea.width,
                                                             intSubArea.height),
                                        fillStyleH, fillStyleV,
                                        unitSize, j * (RIGHT + 1) + i);
    }
  }
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
DrawSolidBorderSegment(nsRenderingContext& aContext,
                       nsRect               aRect,
                       nscolor              aColor,
                       int32_t              aAppUnitsPerDevPixel,
                       nscoord              aTwipsPerPixel,
                       uint8_t              aStartBevelSide = 0,
                       nscoord              aStartBevelOffset = 0,
                       uint8_t              aEndBevelSide = 0,
                       nscoord              aEndBevelOffset = 0)
{
  DrawTarget* drawTarget = aContext.GetDrawTarget();

  ColorPattern color(ToDeviceColor(aColor));
  DrawOptions drawOptions(1.f, CompositionOp::OP_OVER, AntialiasMode::NONE);

  if ((aRect.width == aTwipsPerPixel) || (aRect.height == aTwipsPerPixel) ||
      ((0 == aStartBevelOffset) && (0 == aEndBevelOffset))) {
    // simple line or rectangle
    if ((NS_SIDE_TOP == aStartBevelSide) || (NS_SIDE_BOTTOM == aStartBevelSide)) {
      if (1 == aRect.height)
        StrokeLineWithSnapping(aRect.TopLeft(), aRect.BottomLeft(),
                               aAppUnitsPerDevPixel, *drawTarget,
                               color, StrokeOptions(), drawOptions);
      else
        drawTarget->FillRect(NSRectToRect(aRect, aAppUnitsPerDevPixel, *drawTarget),
                             color, drawOptions);
    }
    else {
      if (1 == aRect.width)
        StrokeLineWithSnapping(aRect.TopLeft(), aRect.TopRight(),
                               aAppUnitsPerDevPixel, *drawTarget,
                               color, StrokeOptions(), drawOptions);
      else
        drawTarget->FillRect(NSRectToRect(aRect, aAppUnitsPerDevPixel, *drawTarget),
                             color, drawOptions);
    }
  }
  else {
    // polygon with beveling
    Point poly[4];
    SetPoly(NSRectToRect(aRect, aAppUnitsPerDevPixel), poly);

    Float startBevelOffset =
      NSAppUnitsToFloatPixels(aStartBevelOffset, aAppUnitsPerDevPixel);
    switch(aStartBevelSide) {
    case NS_SIDE_TOP:
      poly[0].x += startBevelOffset;
      break;
    case NS_SIDE_BOTTOM:
      poly[3].x += startBevelOffset;
      break;
    case NS_SIDE_RIGHT:
      poly[1].y += startBevelOffset;
      break;
    case NS_SIDE_LEFT:
      poly[0].y += startBevelOffset;
    }

    Float endBevelOffset =
      NSAppUnitsToFloatPixels(aEndBevelOffset, aAppUnitsPerDevPixel);
    switch(aEndBevelSide) {
    case NS_SIDE_TOP:
      poly[1].x -= endBevelOffset;
      break;
    case NS_SIDE_BOTTOM:
      poly[2].x -= endBevelOffset;
      break;
    case NS_SIDE_RIGHT:
      poly[2].y -= endBevelOffset;
      break;
    case NS_SIDE_LEFT:
      poly[3].y -= endBevelOffset;
    }

    RefPtr<PathBuilder> builder = drawTarget->CreatePathBuilder();
    builder->MoveTo(poly[0]);
    builder->LineTo(poly[1]);
    builder->LineTo(poly[2]);
    builder->LineTo(poly[3]);
    builder->Close();
    RefPtr<Path> path = builder->Finish();
    drawTarget->Fill(path, color, drawOptions);
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
nsCSSRendering::DrawTableBorderSegment(nsRenderingContext&     aContext,
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
  bool horizontal = ((NS_SIDE_TOP == aStartBevelSide) || (NS_SIDE_BOTTOM == aStartBevelSide));
  nscoord twipsPerPixel = NSIntPixelsToAppUnits(1, aAppUnitsPerCSSPixel);
  uint8_t ridgeGroove = NS_STYLE_BORDER_STYLE_RIDGE;

  if ((twipsPerPixel >= aBorder.width) || (twipsPerPixel >= aBorder.height) ||
      (NS_STYLE_BORDER_STYLE_DASHED == aBorderStyle) || (NS_STYLE_BORDER_STYLE_DOTTED == aBorderStyle)) {
    // no beveling for 1 pixel border, dash or dot
    aStartBevelOffset = 0;
    aEndBevelOffset = 0;
  }

  gfxContext *ctx = aContext.ThebesContext();
  AntialiasMode oldMode = ctx->CurrentAntialiasMode();
  ctx->SetAntialiasMode(AntialiasMode::NONE);

  ctx->SetColor(aBorderColor);

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
        GetDashInfo(aBorder.width, dashLength, twipsPerPixel, numDashSpaces, startDashLength, endDashLength);
        nsRect rect(aBorder.x, aBorder.y, startDashLength, aBorder.height);
        DrawSolidBorderSegment(aContext, rect, aBorderColor, aAppUnitsPerDevPixel, twipsPerPixel);
        for (int32_t spaceX = 0; spaceX < numDashSpaces; spaceX++) {
          rect.x += rect.width + dashLength;
          rect.width = (spaceX == (numDashSpaces - 1)) ? endDashLength : dashLength;
          DrawSolidBorderSegment(aContext, rect, aBorderColor, aAppUnitsPerDevPixel, twipsPerPixel);
        }
      }
      else {
        GetDashInfo(aBorder.height, dashLength, twipsPerPixel, numDashSpaces, startDashLength, endDashLength);
        nsRect rect(aBorder.x, aBorder.y, aBorder.width, startDashLength);
        DrawSolidBorderSegment(aContext, rect, aBorderColor, aAppUnitsPerDevPixel, twipsPerPixel);
        for (int32_t spaceY = 0; spaceY < numDashSpaces; spaceY++) {
          rect.y += rect.height + dashLength;
          rect.height = (spaceY == (numDashSpaces - 1)) ? endDashLength : dashLength;
          DrawSolidBorderSegment(aContext, rect, aBorderColor, aAppUnitsPerDevPixel, twipsPerPixel);
        }
      }
    }
    break;
  case NS_STYLE_BORDER_STYLE_GROOVE:
    ridgeGroove = NS_STYLE_BORDER_STYLE_GROOVE; // and fall through to ridge
  case NS_STYLE_BORDER_STYLE_RIDGE:
    if ((horizontal && (twipsPerPixel >= aBorder.height)) ||
        (!horizontal && (twipsPerPixel >= aBorder.width))) {
      // a one pixel border
      DrawSolidBorderSegment(aContext, aBorder, aBorderColor, aAppUnitsPerDevPixel, twipsPerPixel,
                             aStartBevelSide, aStartBevelOffset,
                             aEndBevelSide, aEndBevelOffset);
    }
    else {
      nscoord startBevel = (aStartBevelOffset > 0)
                            ? RoundFloatToPixel(0.5f * (float)aStartBevelOffset, twipsPerPixel, true) : 0;
      nscoord endBevel =   (aEndBevelOffset > 0)
                            ? RoundFloatToPixel(0.5f * (float)aEndBevelOffset, twipsPerPixel, true) : 0;
      mozilla::css::Side ridgeGrooveSide = (horizontal) ? NS_SIDE_TOP : NS_SIDE_LEFT;
      // FIXME: In theory, this should use the visited-dependent
      // background color, but I don't care.
      ctx->SetColor(
        MakeBevelColor(ridgeGrooveSide, ridgeGroove, aBGColor->mBackgroundColor, aBorderColor));
      nsRect rect(aBorder);
      nscoord half;
      if (horizontal) { // top, bottom
        half = RoundFloatToPixel(0.5f * (float)aBorder.height, twipsPerPixel);
        rect.height = half;
        if (NS_SIDE_TOP == aStartBevelSide) {
          rect.x += startBevel;
          rect.width -= startBevel;
        }
        if (NS_SIDE_TOP == aEndBevelSide) {
          rect.width -= endBevel;
        }
        DrawSolidBorderSegment(aContext, rect, aBorderColor, aAppUnitsPerDevPixel, twipsPerPixel, aStartBevelSide,
                               startBevel, aEndBevelSide, endBevel);
      }
      else { // left, right
        half = RoundFloatToPixel(0.5f * (float)aBorder.width, twipsPerPixel);
        rect.width = half;
        if (NS_SIDE_LEFT == aStartBevelSide) {
          rect.y += startBevel;
          rect.height -= startBevel;
        }
        if (NS_SIDE_LEFT == aEndBevelSide) {
          rect.height -= endBevel;
        }
        DrawSolidBorderSegment(aContext, rect, aBorderColor, aAppUnitsPerDevPixel, twipsPerPixel, aStartBevelSide,
                               startBevel, aEndBevelSide, endBevel);
      }

      rect = aBorder;
      ridgeGrooveSide = (NS_SIDE_TOP == ridgeGrooveSide) ? NS_SIDE_BOTTOM : NS_SIDE_RIGHT;
      // FIXME: In theory, this should use the visited-dependent
      // background color, but I don't care.
      ctx->SetColor(
        MakeBevelColor(ridgeGrooveSide, ridgeGroove, aBGColor->mBackgroundColor, aBorderColor));
      if (horizontal) {
        rect.y = rect.y + half;
        rect.height = aBorder.height - half;
        if (NS_SIDE_BOTTOM == aStartBevelSide) {
          rect.x += startBevel;
          rect.width -= startBevel;
        }
        if (NS_SIDE_BOTTOM == aEndBevelSide) {
          rect.width -= endBevel;
        }
        DrawSolidBorderSegment(aContext, rect, aBorderColor, aAppUnitsPerDevPixel, twipsPerPixel, aStartBevelSide,
                               startBevel, aEndBevelSide, endBevel);
      }
      else {
        rect.x = rect.x + half;
        rect.width = aBorder.width - half;
        if (NS_SIDE_RIGHT == aStartBevelSide) {
          rect.y += aStartBevelOffset - startBevel;
          rect.height -= startBevel;
        }
        if (NS_SIDE_RIGHT == aEndBevelSide) {
          rect.height -= endBevel;
        }
        DrawSolidBorderSegment(aContext, rect, aBorderColor, aAppUnitsPerDevPixel, twipsPerPixel, aStartBevelSide,
                               startBevel, aEndBevelSide, endBevel);
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
        if (NS_SIDE_TOP == aStartBevelSide) {
          topRect.x += aStartBevelOffset - startBevel;
          topRect.width -= aStartBevelOffset - startBevel;
        }
        if (NS_SIDE_TOP == aEndBevelSide) {
          topRect.width -= aEndBevelOffset - endBevel;
        }
        DrawSolidBorderSegment(aContext, topRect, aBorderColor, aAppUnitsPerDevPixel, twipsPerPixel, aStartBevelSide,
                               startBevel, aEndBevelSide, endBevel);

        // draw the botom line or rect
        nscoord heightOffset = aBorder.height - thirdHeight;
        nsRect bottomRect(aBorder.x, aBorder.y + heightOffset, aBorder.width, aBorder.height - heightOffset);
        if (NS_SIDE_BOTTOM == aStartBevelSide) {
          bottomRect.x += aStartBevelOffset - startBevel;
          bottomRect.width -= aStartBevelOffset - startBevel;
        }
        if (NS_SIDE_BOTTOM == aEndBevelSide) {
          bottomRect.width -= aEndBevelOffset - endBevel;
        }
        DrawSolidBorderSegment(aContext, bottomRect, aBorderColor, aAppUnitsPerDevPixel, twipsPerPixel, aStartBevelSide,
                               startBevel, aEndBevelSide, endBevel);
      }
      else { // left, right
        nscoord thirdWidth = RoundFloatToPixel(0.333333f * (float)aBorder.width, twipsPerPixel);

        nsRect leftRect(aBorder.x, aBorder.y, thirdWidth, aBorder.height);
        if (NS_SIDE_LEFT == aStartBevelSide) {
          leftRect.y += aStartBevelOffset - startBevel;
          leftRect.height -= aStartBevelOffset - startBevel;
        }
        if (NS_SIDE_LEFT == aEndBevelSide) {
          leftRect.height -= aEndBevelOffset - endBevel;
        }
        DrawSolidBorderSegment(aContext, leftRect, aBorderColor, aAppUnitsPerDevPixel, twipsPerPixel, aStartBevelSide,
                               startBevel, aEndBevelSide, endBevel);

        nscoord widthOffset = aBorder.width - thirdWidth;
        nsRect rightRect(aBorder.x + widthOffset, aBorder.y, aBorder.width - widthOffset, aBorder.height);
        if (NS_SIDE_RIGHT == aStartBevelSide) {
          rightRect.y += aStartBevelOffset - startBevel;
          rightRect.height -= aStartBevelOffset - startBevel;
        }
        if (NS_SIDE_RIGHT == aEndBevelSide) {
          rightRect.height -= aEndBevelOffset - endBevel;
        }
        DrawSolidBorderSegment(aContext, rightRect, aBorderColor, aAppUnitsPerDevPixel, twipsPerPixel, aStartBevelSide,
                               startBevel, aEndBevelSide, endBevel);
      }
      break;
    }
    // else fall through to solid
  case NS_STYLE_BORDER_STYLE_SOLID:
    DrawSolidBorderSegment(aContext, aBorder, aBorderColor, aAppUnitsPerDevPixel, twipsPerPixel, aStartBevelSide,
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

  ctx->SetAntialiasMode(oldMode);
}

// End table border-collapsing section

gfxRect
nsCSSRendering::ExpandPaintingRectForDecorationLine(
                  nsIFrame* aFrame,
                  const uint8_t aStyle,
                  const gfxRect& aClippedRect,
                  const gfxFloat aICoordInFrame,
                  const gfxFloat aCycleLength,
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
  gfxFloat framePosInBlock = pc->AppUnitsToGfxUnits(framePosInBlockAppUnits);
  int32_t rectPosInBlock =
    int32_t(NS_round(framePosInBlock + aICoordInFrame));
  int32_t extraStartEdge =
    rectPosInBlock - (rectPosInBlock / int32_t(aCycleLength) * aCycleLength);
  gfxRect rect(aClippedRect);
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
nsCSSRendering::PaintDecorationLine(nsIFrame* aFrame,
                                    gfxContext* aGfxContext,
                                    const gfxRect& aDirtyRect,
                                    const nscolor aColor,
                                    const gfxPoint& aPt,
                                    const gfxFloat aICoordInFrame,
                                    const gfxSize& aLineSize,
                                    const gfxFloat aAscent,
                                    const gfxFloat aOffset,
                                    const uint8_t aDecoration,
                                    const uint8_t aStyle,
                                    bool aVertical,
                                    const gfxFloat aDescentLimit)
{
  NS_ASSERTION(aStyle != NS_STYLE_TEXT_DECORATION_STYLE_NONE, "aStyle is none");

  gfxRect rect =
    GetTextDecorationRectInternal(aPt, aLineSize, aAscent, aOffset,
                                  aDecoration, aStyle, aVertical,
                                  aDescentLimit);
  if (rect.IsEmpty() || !rect.Intersects(aDirtyRect)) {
    return;
  }

  if (aDecoration != NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE &&
      aDecoration != NS_STYLE_TEXT_DECORATION_LINE_OVERLINE &&
      aDecoration != NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH) {
    NS_ERROR("Invalid decoration value!");
    return;
  }

  gfxFloat lineThickness = std::max(NS_round(aLineSize.height), 1.0);
  bool contextIsSaved = false;

  gfxFloat oldLineWidth;
  nsRefPtr<gfxPattern> oldPattern;

  switch (aStyle) {
    case NS_STYLE_TEXT_DECORATION_STYLE_SOLID:
    case NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE:
      oldLineWidth = aGfxContext->CurrentLineWidth();
      oldPattern = aGfxContext->GetPattern();
      break;
    case NS_STYLE_TEXT_DECORATION_STYLE_DASHED: {
      aGfxContext->Save();
      contextIsSaved = true;
      aGfxContext->Clip(rect);
      gfxFloat dashWidth = lineThickness * DOT_LENGTH * DASH_LENGTH;
      gfxFloat dash[2] = { dashWidth, dashWidth };
      aGfxContext->SetLineCap(gfxContext::LINE_CAP_BUTT);
      aGfxContext->SetDash(dash, 2, 0.0);
      rect = ExpandPaintingRectForDecorationLine(aFrame, aStyle, rect,
                                                 aICoordInFrame,
                                                 dashWidth * 2,
                                                 aVertical);
      // We should continue to draw the last dash even if it is not in the rect.
      rect.width += dashWidth;
      break;
    }
    case NS_STYLE_TEXT_DECORATION_STYLE_DOTTED: {
      aGfxContext->Save();
      contextIsSaved = true;
      aGfxContext->Clip(rect);
      gfxFloat dashWidth = lineThickness * DOT_LENGTH;
      gfxFloat dash[2];
      if (lineThickness > 2.0) {
        dash[0] = 0.0;
        dash[1] = dashWidth * 2.0;
        aGfxContext->SetLineCap(gfxContext::LINE_CAP_ROUND);
      } else {
        dash[0] = dashWidth;
        dash[1] = dashWidth;
      }
      aGfxContext->SetDash(dash, 2, 0.0);
      rect = ExpandPaintingRectForDecorationLine(aFrame, aStyle, rect,
                                                 aICoordInFrame,
                                                 dashWidth * 2,
                                                 aVertical);
      // We should continue to draw the last dot even if it is not in the rect.
      rect.width += dashWidth;
      break;
    }
    case NS_STYLE_TEXT_DECORATION_STYLE_WAVY:
      aGfxContext->Save();
      contextIsSaved = true;
      aGfxContext->Clip(rect);
      if (lineThickness > 2.0) {
        aGfxContext->SetAntialiasMode(AntialiasMode::SUBPIXEL);
      } else {
        // Don't use anti-aliasing here.  Because looks like lighter color wavy
        // line at this case.  And probably, users don't think the
        // non-anti-aliased wavy line is not pretty.
        aGfxContext->SetAntialiasMode(AntialiasMode::NONE);
      }
      break;
    default:
      NS_ERROR("Invalid style value!");
      return;
  }

  // The y position should be set to the middle of the line.
  rect.y += lineThickness / 2;

  aGfxContext->SetColor(gfxRGBA(aColor));
  aGfxContext->SetLineWidth(lineThickness);
  switch (aStyle) {
    case NS_STYLE_TEXT_DECORATION_STYLE_SOLID:
    case NS_STYLE_TEXT_DECORATION_STYLE_DOTTED:
    case NS_STYLE_TEXT_DECORATION_STYLE_DASHED:
      aGfxContext->NewPath();
      if (aVertical) {
        aGfxContext->MoveTo(rect.TopLeft());
        aGfxContext->LineTo(rect.BottomLeft());
      } else {
        aGfxContext->MoveTo(rect.TopLeft());
        aGfxContext->LineTo(rect.TopRight());
      }
      aGfxContext->Stroke();
      break;
    case NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE:
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
      aGfxContext->NewPath();
      if (aVertical) {
        aGfxContext->MoveTo(rect.TopLeft());
        aGfxContext->LineTo(rect.BottomLeft());
        rect.width -= lineThickness;
        aGfxContext->MoveTo(rect.TopRight());
        aGfxContext->LineTo(rect.BottomRight());
      } else {
        aGfxContext->MoveTo(rect.TopLeft());
        aGfxContext->LineTo(rect.TopRight());
        rect.height -= lineThickness;
        aGfxContext->MoveTo(rect.BottomLeft());
        aGfxContext->LineTo(rect.BottomRight());
      }
      aGfxContext->Stroke();
      break;
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

      gfxFloat& rectICoord = aVertical ? rect.y : rect.x;
      gfxFloat& rectISize = aVertical ? rect.height : rect.width;
      const gfxFloat rectBSize = aVertical ? rect.width : rect.height;

      const gfxFloat adv = rectBSize - lineThickness;
      const gfxFloat flatLengthAtVertex =
        std::max((lineThickness - 1.0) * 2.0, 1.0);

      // Align the start of wavy lines to the nearest ancestor block.
      const gfxFloat cycleLength = 2 * (adv + flatLengthAtVertex);
      rect = ExpandPaintingRectForDecorationLine(aFrame, aStyle, rect,
                                                 aICoordInFrame, cycleLength,
                                                 aVertical);
      // figure out if we can trim whole cycles from the left and right edges
      // of the line, to try and avoid creating an unnecessarily long and
      // complex path
      const gfxFloat dirtyRectICoord = aVertical ? aDirtyRect.y : aDirtyRect.x;
      int32_t skipCycles = floor((dirtyRectICoord - rectICoord) / cycleLength);
      if (skipCycles > 0) {
        rectICoord += skipCycles * cycleLength;
        rectISize -= skipCycles * cycleLength;
      }

      rectICoord += lineThickness / 2.0;
      gfxPoint pt(rect.TopLeft());
      gfxFloat& ptICoord = aVertical ? pt.y : pt.x;
      gfxFloat& ptBCoord = aVertical ? pt.x : pt.y;
      if (aVertical) {
        ptBCoord += adv + lineThickness / 2.0;
      }
      gfxFloat iCoordLimit = ptICoord + rectISize + lineThickness;

      const gfxFloat dirtyRectIMost = aVertical ?
        aDirtyRect.YMost() : aDirtyRect.XMost();
      skipCycles = floor((iCoordLimit - dirtyRectIMost) / cycleLength);
      if (skipCycles > 0) {
        iCoordLimit -= skipCycles * cycleLength;
      }

      aGfxContext->NewPath();

      ptICoord -= lineThickness;
      aGfxContext->MoveTo(pt); // 1

      ptICoord = rectICoord;
      aGfxContext->LineTo(pt); // 2

      // In vertical mode, to go "down" relative to the text we need to
      // decrease the block coordinate, whereas in horizontal we increase
      // it. So the sense of this flag is effectively inverted.
      bool goDown = aVertical ? false : true;
      uint32_t iter = 0;
      while (ptICoord < iCoordLimit) {
        if (++iter > 1000) {
          // stroke the current path and start again, to avoid pathological
          // behavior in cairo with huge numbers of path segments
          aGfxContext->Stroke();
          aGfxContext->NewPath();
          aGfxContext->MoveTo(pt);
          iter = 0;
        }
        ptICoord += adv;
        ptBCoord += goDown ? adv : -adv;

        aGfxContext->LineTo(pt); // 3 and 5

        ptICoord += flatLengthAtVertex;
        aGfxContext->LineTo(pt); // 4 and 6

        goDown = !goDown;
      }
      aGfxContext->Stroke();
      break;
    }
    default:
      NS_ERROR("Invalid style value!");
      break;
  }

  if (contextIsSaved) {
    aGfxContext->Restore();
  } else {
    aGfxContext->SetPattern(oldPattern);
    aGfxContext->SetLineWidth(oldLineWidth);
  }
}

void
nsCSSRendering::DecorationLineToPath(nsIFrame* aFrame,
                                     gfxContext* aGfxContext,
                                     const gfxRect& aDirtyRect,
                                     const nscolor aColor,
                                     const gfxPoint& aPt,
                                     const gfxFloat aICoordInFrame,
                                     const gfxSize& aLineSize,
                                     const gfxFloat aAscent,
                                     const gfxFloat aOffset,
                                     const uint8_t aDecoration,
                                     const uint8_t aStyle,
                                     bool aVertical,
                                     const gfxFloat aDescentLimit)
{
  NS_ASSERTION(aStyle != NS_STYLE_TEXT_DECORATION_STYLE_NONE, "aStyle is none");

  aGfxContext->NewPath();

  gfxRect rect =
    GetTextDecorationRectInternal(aPt, aLineSize, aAscent, aOffset,
                                  aDecoration, aStyle, aVertical,
                                  aDescentLimit);
  if (rect.IsEmpty() || !rect.Intersects(aDirtyRect)) {
    return;
  }

  if (aDecoration != NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE &&
      aDecoration != NS_STYLE_TEXT_DECORATION_LINE_OVERLINE &&
      aDecoration != NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH) {
    NS_ERROR("Invalid decoration value!");
    return;
  }

  if (aStyle != NS_STYLE_TEXT_DECORATION_STYLE_SOLID) {
    // For the moment, we support only solid text decorations.
    return;
  }

  gfxFloat lineThickness = std::max(NS_round(aLineSize.height), 1.0);

  // The y position should be set to the middle of the line.
  rect.y += lineThickness / 2;

  aGfxContext->Rectangle
    (gfxRect(gfxPoint(rect.TopLeft() - gfxPoint(0.0, lineThickness / 2)),
             gfxSize(rect.Width(), lineThickness)));
}

nsRect
nsCSSRendering::GetTextDecorationRect(nsPresContext* aPresContext,
                                      const gfxSize& aLineSize,
                                      const gfxFloat aAscent,
                                      const gfxFloat aOffset,
                                      const uint8_t aDecoration,
                                      const uint8_t aStyle,
                                      bool aVertical,
                                      const gfxFloat aDescentLimit)
{
  NS_ASSERTION(aPresContext, "aPresContext is null");
  NS_ASSERTION(aStyle != NS_STYLE_TEXT_DECORATION_STYLE_NONE, "aStyle is none");

  gfxRect rect =
    GetTextDecorationRectInternal(gfxPoint(0, 0), aLineSize, aAscent, aOffset,
                                  aDecoration, aStyle, aVertical,
                                  aDescentLimit);
  // The rect values are already rounded to nearest device pixels.
  nsRect r;
  r.x = aPresContext->GfxUnitsToAppUnits(rect.X());
  r.y = aPresContext->GfxUnitsToAppUnits(rect.Y());
  r.width = aPresContext->GfxUnitsToAppUnits(rect.Width());
  r.height = aPresContext->GfxUnitsToAppUnits(rect.Height());
  return r;
}

gfxRect
nsCSSRendering::GetTextDecorationRectInternal(const gfxPoint& aPt,
                                              const gfxSize& aLineSize,
                                              const gfxFloat aAscent,
                                              const gfxFloat aOffset,
                                              const uint8_t aDecoration,
                                              const uint8_t aStyle,
                                              bool aVertical,
                                              const gfxFloat aDescentLimit)
{
  NS_ASSERTION(aStyle <= NS_STYLE_TEXT_DECORATION_STYLE_WAVY,
               "Invalid aStyle value");

  if (aStyle == NS_STYLE_TEXT_DECORATION_STYLE_NONE)
    return gfxRect(0, 0, 0, 0);

  bool canLiftUnderline = aDescentLimit >= 0.0;

  gfxFloat iCoord = aVertical ? aPt.y : aPt.x;
  gfxFloat bCoord = aVertical ? aPt.x : aPt.y;

  // 'left' and 'right' are relative to the line, so for vertical writing modes
  // they will actually become top and bottom of the rendered line.
  // Similarly, aLineSize.width and .height are actually length and thickness
  // of the line, which runs horizontally or vertically according to aVertical.
  const gfxFloat left  = floor(iCoord + 0.5),
                 right = floor(iCoord + aLineSize.width + 0.5);

  // We compute |r| as if for a horizontal text run, and then swap vertical
  // and horizontal coordinates at the end if vertical was requested.
  gfxRect r(left, 0, right - left, 0);

  gfxFloat lineThickness = NS_round(aLineSize.height);
  lineThickness = std::max(lineThickness, 1.0);

  gfxFloat ascent = NS_round(aAscent);
  gfxFloat descentLimit = floor(aDescentLimit);

  gfxFloat suggestedMaxRectHeight = std::max(std::min(ascent, descentLimit), 1.0);
  r.height = lineThickness;
  if (aStyle == NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE) {
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
  } else if (aStyle == NS_STYLE_TEXT_DECORATION_STYLE_WAVY) {
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

  gfxFloat baseline = floor(bCoord + aAscent + 0.5);
  gfxFloat offset = 0.0;
  switch (aDecoration) {
    case NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE:
      offset = aOffset;
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
      offset = aOffset - lineThickness + r.Height();
      break;
    case NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH: {
      gfxFloat extra = floor(r.Height() / 2.0 + 0.5);
      extra = std::max(extra, lineThickness);
      offset = aOffset - lineThickness + extra;
      break;
    }
    default:
      NS_ERROR("Invalid decoration value!");
  }

  if (aVertical) {
    r.y = baseline + floor(aOffset + 0.5); // this will need updating when we
                                           // support sideways-left orientation
    Swap(r.x, r.y);
    Swap(r.width, r.height);
  } else {
    r.y = baseline - floor(aOffset + 0.5);
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
  , mIsReady(false)
  , mSize(0, 0)
  , mFlags(aFlags)
{
}

nsImageRenderer::~nsImageRenderer()
{
}

bool
nsImageRenderer::PrepareImage()
{
  if (mImage->IsEmpty())
    return false;

  if (!mImage->IsComplete()) {
    // Make sure the image is actually decoding
    mImage->StartDecoding();

    // check again to see if we finished
    if (!mImage->IsComplete()) {
      // We can not prepare the image for rendering if it is not fully loaded.
      //
      // Special case: If we requested a sync decode and we have an image, push
      // on through because the Draw() will do a sync decode then
      nsCOMPtr<imgIContainer> img;
      if (!((mFlags & FLAG_SYNC_DECODE_IMAGES) &&
            (mType == eStyleImageType_Image) &&
            (NS_SUCCEEDED(mImage->GetImageData()->GetImage(getter_AddRefs(img))))))
        return false;
    }
  }

  switch (mType) {
    case eStyleImageType_Image:
    {
      nsCOMPtr<imgIContainer> srcImage;
      DebugOnly<nsresult> rv =
        mImage->GetImageData()->GetImage(getter_AddRefs(srcImage));
      NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv) && srcImage,
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
          return false;
        }
        if (isEntireImage) {
          // The cropped image is identical to the source image
          mImageContainer.swap(srcImage);
        } else {
          nsCOMPtr<imgIContainer> subImage = ImageOps::Clip(srcImage, actualCropRect);
          mImageContainer.swap(subImage);
        }
      }
      mIsReady = true;
      break;
    }
    case eStyleImageType_Gradient:
      mGradientData = mImage->GetGradientData();
      mIsReady = true;
      break;
    case eStyleImageType_Element:
    {
      nsAutoString elementId =
        NS_LITERAL_STRING("#") + nsDependentString(mImage->GetElementId());
      nsCOMPtr<nsIURI> targetURI;
      nsCOMPtr<nsIURI> base = mForFrame->GetContent()->GetBaseURI();
      nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), elementId,
                                                mForFrame->GetContent()->GetCurrentDoc(), base);
      nsSVGPaintingProperty* property = nsSVGEffects::GetPaintingPropertyForURI(
          targetURI, mForFrame->FirstContinuation(),
          nsSVGEffects::BackgroundImageProperty());
      if (!property)
        return false;
      mPaintServerFrame = property->GetReferencedFrame();

      // If the referenced element doesn't have a frame we might still be able
      // to paint it if it's an <img>, <canvas>, or <video> element.
      if (!mPaintServerFrame) {
        mImageElementSurface =
          nsLayoutUtils::SurfaceFromElement(property->GetReferencedElement());
        if (!mImageElementSurface.mSourceSurface)
          return false;
      }
      mIsReady = true;
      break;
    }
    case eStyleImageType_Null:
    default:
      break;
  }

  return mIsReady;
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
  NS_ASSERTION(mIsReady, "Ensure PrepareImage() has returned true "
                         "before calling me");

  CSSSizeOrRatio result;
  switch (mType) {
    case eStyleImageType_Image:
    {
      bool haveWidth, haveHeight;
      nsIntSize imageIntSize;
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
      //     Make sure to change nsStyleBackground::Size::DependsOnFrameSize
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
            nsSVGIntegrationUtils::GetContinuationUnionSize(mPaintServerFrame).
              ToNearestPixels(appUnitsPerDevPixel).
              ToAppUnits(appUnitsPerDevPixel));
        }
      } else {
        NS_ASSERTION(mImageElementSurface.mSourceSurface, "Surface should be ready.");
        gfxIntSize surfaceSize = mImageElementSurface.mSize;
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
  } else {
    size.width = NSCoordSaturatingNonnegativeMultiply(
                   aIntrinsicRatio.width, scaleY);
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

void
nsImageRenderer::Draw(nsPresContext*       aPresContext,
                      nsRenderingContext&  aRenderingContext,
                      const nsRect&        aDirtyRect,
                      const nsRect&        aDest,
                      const nsRect&        aFill,
                      const nsPoint&       aAnchor,
                      const CSSIntRect&    aSrc)
{
  if (!mIsReady) {
    NS_NOTREACHED("Ensure PrepareImage() has returned true before calling me");
    return;
  }
  if (aDest.IsEmpty() || aFill.IsEmpty() ||
      mSize.width <= 0 || mSize.height <= 0) {
    return;
  }

  GraphicsFilter filter = nsLayoutUtils::GetGraphicsFilterForFrame(mForFrame);

  switch (mType) {
    case eStyleImageType_Image:
    {
      nsIntSize imageSize(nsPresContext::AppUnitsToIntCSSPixels(mSize.width),
                          nsPresContext::AppUnitsToIntCSSPixels(mSize.height));
      nsLayoutUtils::DrawBackgroundImage(&aRenderingContext, aPresContext,
                                         mImageContainer, imageSize, filter,
                                         aDest, aFill, aAnchor, aDirtyRect,
                                         ConvertImageRendererToDrawFlags(mFlags));
      return;
    }
    case eStyleImageType_Gradient:
    {
      nsCSSRendering::PaintGradient(aPresContext, aRenderingContext,
                                    mGradientData, aDirtyRect,
                                    aDest, aFill, aSrc, mSize);
      return;
    }
    case eStyleImageType_Element:
    {
      nsRefPtr<gfxDrawable> drawable = DrawableForElement(aDest,
                                                          aRenderingContext);
      if (!drawable) {
        NS_WARNING("Could not create drawable for element");
        return;
      }

      gfxContext* ctx = aRenderingContext.ThebesContext();
      gfxContext::GraphicsOperator op = ctx->CurrentOperator();
      if (op != gfxContext::OPERATOR_OVER) {
        ctx->PushGroup(gfxContentType::COLOR_ALPHA);
        ctx->SetOperator(gfxContext::OPERATOR_OVER);
      }

      nsCOMPtr<imgIContainer> image(ImageOps::CreateFromDrawable(drawable));
      nsLayoutUtils::DrawImage(&aRenderingContext, aPresContext, image,
                               filter, aDest, aFill, aAnchor, aDirtyRect,
                               ConvertImageRendererToDrawFlags(mFlags));

      if (op != gfxContext::OPERATOR_OVER) {
        ctx->PopGroupToSource();
        ctx->Paint();
      }

      return;
    }
    case eStyleImageType_Null:
    default:
      return;
  }
}

already_AddRefed<gfxDrawable>
nsImageRenderer::DrawableForElement(const nsRect& aImageRect,
                                    nsRenderingContext&  aRenderingContext)
{
  NS_ASSERTION(mType == eStyleImageType_Element,
               "DrawableForElement only makes sense if backed by an element");
  if (mPaintServerFrame) {
    int32_t appUnitsPerDevPixel = mForFrame->PresContext()->AppUnitsPerDevPixel();
    nsRect destRect = aImageRect - aImageRect.TopLeft();
    nsIntSize roundedOut = destRect.ToOutsidePixels(appUnitsPerDevPixel).Size();
    gfxIntSize imageSize(roundedOut.width, roundedOut.height);
    nsRefPtr<gfxDrawable> drawable =
      nsSVGIntegrationUtils::DrawableFromPaintServer(
        mPaintServerFrame, mForFrame, mSize, imageSize,
        aRenderingContext.GetDrawTarget(),
        aRenderingContext.ThebesContext()->CurrentMatrix(),
        mFlags & FLAG_SYNC_DECODE_IMAGES
          ? nsSVGIntegrationUtils::FLAG_SYNC_DECODE_IMAGES
          : 0);

    return drawable.forget();
  }
  NS_ASSERTION(mImageElementSurface.mSourceSurface, "Surface should be ready.");
  nsRefPtr<gfxDrawable> drawable = new gfxSurfaceDrawable(
                                mImageElementSurface.mSourceSurface,
                                mImageElementSurface.mSize);
  return drawable.forget();
}

void
nsImageRenderer::DrawBackground(nsPresContext*       aPresContext,
                                nsRenderingContext&  aRenderingContext,
                                const nsRect&        aDest,
                                const nsRect&        aFill,
                                const nsPoint&       aAnchor,
                                const nsRect&        aDirty)
{
  if (!mIsReady) {
    NS_NOTREACHED("Ensure PrepareImage() has returned true before calling me");
    return;
  }
  if (aDest.IsEmpty() || aFill.IsEmpty() ||
      mSize.width <= 0 || mSize.height <= 0) {
    return;
  }

  Draw(aPresContext, aRenderingContext,
       aDirty, aDest, aFill, aAnchor,
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
ComputeTile(const nsRect&        aFill,
            uint8_t              aHFill,
            uint8_t              aVFill,
            const nsSize&        aUnitSize)
{
  nsRect tile;
  switch (aHFill) {
  case NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH:
    tile.x = aFill.x;
    tile.width = aFill.width;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT_REPEAT:
    tile.x = aFill.x + aFill.width/2 - aUnitSize.width/2;
    tile.width = aUnitSize.width;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT_ROUND:
    tile.x = aFill.x;
    tile.width = aFill.width / ceil(gfxFloat(aFill.width)/aUnitSize.width);
    break;
  default:
    NS_NOTREACHED("unrecognized border-image fill style");
  }

  switch (aVFill) {
  case NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH:
    tile.y = aFill.y;
    tile.height = aFill.height;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT_REPEAT:
    tile.y = aFill.y + aFill.height/2 - aUnitSize.height/2;
    tile.height = aUnitSize.height;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT_ROUND:
    tile.y = aFill.y;
    tile.height = aFill.height/ceil(gfxFloat(aFill.height)/aUnitSize.height);
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

void
nsImageRenderer::DrawBorderImageComponent(nsPresContext*       aPresContext,
                                          nsRenderingContext&  aRenderingContext,
                                          const nsRect&        aDirtyRect,
                                          const nsRect&        aFill,
                                          const CSSIntRect&    aSrc,
                                          uint8_t              aHFill,
                                          uint8_t              aVFill,
                                          const nsSize&        aUnitSize,
                                          uint8_t              aIndex)
{
  if (!mIsReady) {
    NS_NOTREACHED("Ensure PrepareImage() has returned true before calling me");
    return;
  }
  if (aFill.IsEmpty() || aSrc.IsEmpty()) {
    return;
  }

  if (mType == eStyleImageType_Image || mType == eStyleImageType_Element) {
    nsCOMPtr<imgIContainer> subImage;

    // Retrieve or create the subimage we'll draw.
    nsIntRect srcRect(aSrc.x, aSrc.y, aSrc.width, aSrc.height);
    if (mType == eStyleImageType_Image) {
      if ((subImage = mImage->GetSubImage(aIndex)) == nullptr) {
        subImage = ImageOps::Clip(mImageContainer, srcRect);
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

      nsRefPtr<gfxDrawable> drawable = DrawableForElement(nsRect(nsPoint(), mSize),
                                                          aRenderingContext);
      if (!drawable) {
        NS_WARNING("Could not create drawable for element");
        return;
      }

      nsCOMPtr<imgIContainer> image(ImageOps::CreateFromDrawable(drawable));
      subImage = ImageOps::Clip(image, srcRect);
    }

    GraphicsFilter graphicsFilter =
      nsLayoutUtils::GetGraphicsFilterForFrame(mForFrame);

    if (!RequiresScaling(aFill, aHFill, aVFill, aUnitSize)) {
      nsLayoutUtils::DrawSingleImage(&aRenderingContext,
                                     aPresContext,
                                     subImage,
                                     graphicsFilter,
                                     aFill, aDirtyRect,
                                     nullptr,
                                     imgIContainer::FLAG_NONE);
      return;
    }

    nsRect tile = ComputeTile(aFill, aHFill, aVFill, aUnitSize);
    nsLayoutUtils::DrawImage(&aRenderingContext,
                             aPresContext,
                             subImage,
                             graphicsFilter,
                             tile, aFill, tile.TopLeft(), aDirtyRect,
                             imgIContainer::FLAG_NONE);
    return;
  }

  nsRect destTile = RequiresScaling(aFill, aHFill, aVFill, aUnitSize)
                  ? ComputeTile(aFill, aHFill, aVFill, aUnitSize)
                  : aFill;

  Draw(aPresContext, aRenderingContext, aDirtyRect, destTile,
       aFill, destTile.TopLeft(), aSrc);
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

already_AddRefed<mozilla::layers::ImageContainer>
nsImageRenderer::GetContainer(LayerManager* aManager)
{
  if (mType != eStyleImageType_Image || !mImageContainer)
    return nullptr;

  nsRefPtr<ImageContainer> container;
  nsresult rv = mImageContainer->GetImageContainer(aManager, getter_AddRefs(container));
  NS_ENSURE_SUCCESS(rv, nullptr);
  return container.forget();
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

static inline gfxIntSize
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

  gfxFloat scaleX = 1;
  gfxFloat scaleY = 1;

  // Do blurs in device space when possible.
  // Chrome/Skia always does the blurs in device space
  // and will sometimes get incorrect results (e.g. rotated blurs)
  gfxMatrix transform = aDestinationCtx->CurrentMatrix();
  // XXX: we could probably handle negative scales but for now it's easier just to fallback
  if (transform.HasNonAxisAlignedTransform() || transform._11 <= 0.0 || transform._22 <= 0.0) {
    transform = gfxMatrix();
  } else {
    scaleX = transform._11;
    scaleY = transform._22;
  }

  // compute a large or smaller blur radius
  gfxIntSize blurRadius = ComputeBlurRadius(aBlurRadius, aAppUnitsPerDevPixel, scaleX, scaleY);
  gfxIntSize spreadRadius = gfxIntSize(std::min(int32_t(aSpreadRadius * scaleX / aAppUnitsPerDevPixel),
                                              int32_t(MAX_SPREAD_RADIUS)),
                                       std::min(int32_t(aSpreadRadius * scaleY / aAppUnitsPerDevPixel),
                                              int32_t(MAX_SPREAD_RADIUS)));
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

  rect = transform.TransformBounds(rect);

  mPreTransformed = !transform.IsIdentity();

  // Create the temporary surface for blurring
  dirtyRect = transform.TransformBounds(dirtyRect);
  if (aSkipRect) {
    gfxRect skipRect = transform.TransformBounds(*aSkipRect);
    mContext = mAlphaBoxBlur.Init(rect, spreadRadius,
                                  blurRadius, &dirtyRect, &skipRect);
  } else {
    mContext = mAlphaBoxBlur.Init(rect, spreadRadius,
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
  if (mContext == mDestinationCtx)
    return;

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
  gfxIntSize blurRadius = ComputeBlurRadius(aBlurRadius, aAppUnitsPerDevPixel);

  nsMargin result;
  result.top = result.bottom = blurRadius.height * aAppUnitsPerDevPixel;
  result.left = result.right = blurRadius.width  * aAppUnitsPerDevPixel;
  return result;
}

/* static */ void
nsContextBoxBlur::BlurRectangle(gfxContext* aDestinationCtx,
                                const nsRect& aRect,
                                int32_t aAppUnitsPerDevPixel,
                                gfxCornerSizes* aCornerRadii,
                                nscoord aBlurRadius,
                                const gfxRGBA& aShadowColor,
                                const nsRect& aDirtyRect,
                                const gfxRect& aSkipRect)
{
  if (aRect.IsEmpty()) {
    return;
  }

  gfxRect shadowGfxRect =
    nsLayoutUtils::RectToGfxRect(aRect, aAppUnitsPerDevPixel);

  if (aBlurRadius <= 0) {
    aDestinationCtx->SetColor(aShadowColor);
    aDestinationCtx->NewPath();
    if (aCornerRadii) {
      aDestinationCtx->RoundedRectangle(shadowGfxRect, *aCornerRadii);
    } else {
      aDestinationCtx->Rectangle(shadowGfxRect);
    }

    aDestinationCtx->Fill();
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

  shadowGfxRect = transform.TransformBounds(shadowGfxRect);
  dirtyRect = transform.TransformBounds(dirtyRect);
  gfxRect skipRect = transform.TransformBounds(aSkipRect);

  if (aCornerRadii) {
    aCornerRadii->Scale(scaleX, scaleY);
  }

  gfxAlphaBoxBlur::BlurRectangle(aDestinationCtx,
                                 shadowGfxRect,
                                 aCornerRadii,
                                 blurStdDev,
                                 aShadowColor,
                                 dirtyRect,
                                 skipRect);
}
