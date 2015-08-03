/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCSSRenderingBorders.h"

#include "gfxUtils.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/gfx/PathHelpers.h"
#include "nsLayoutUtils.h"
#include "nsStyleConsts.h"
#include "nsCSSColorUtils.h"
#include "GeckoProfiler.h"
#include "nsExpirationTracker.h"
#include "RoundedRect.h"
#include "nsClassHashtable.h"
#include "nsStyleStruct.h"
#include "mozilla/gfx/2D.h"
#include "gfx2DGlue.h"
#include "gfxGradientCache.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::gfx;

/**
 * nsCSSRendering::PaintBorder
 * nsCSSRendering::PaintOutline
 *   -> DrawBorders
 *
 * DrawBorders
 *   -> Ability to use specialized approach?
 *      |- Draw using specialized function
 *   |- separate corners?
 *   |- dashed side mask
 *   |
 *   -> can border be drawn in 1 pass? (e.g., solid border same color all around)
 *      |- DrawBorderSides with all 4 sides
 *   -> more than 1 pass?
 *      |- for each corner
 *         |- clip to DoCornerClipSubPath
 *         |- for each side adjacent to corner
 *            |- clip to GetSideClipSubPath
 *            |- DrawBorderSides with one side
 *      |- for each side
 *         |- GetSideClipWithoutCornersRect
 *         |- DrawDashedSide || DrawBorderSides with one side
 */

static void ComputeBorderCornerDimensions(const Rect& aOuterRect,
                                          const Rect& aInnerRect,
                                          const RectCornerRadii& aRadii,
                                          RectCornerRadii *aDimsResult);

// given a side index, get the previous and next side index
#define NEXT_SIDE(_s) mozilla::css::Side(((_s) + 1) & 3)
#define PREV_SIDE(_s) mozilla::css::Side(((_s) + 3) & 3)

// from the given base color and the background color, turn
// color into a color for the given border pattern style
static Color MakeBorderColor(nscolor aColor,
                             nscolor aBackgroundColor,
                             BorderColorStyle aBorderColorStyle);


// Given a line index (an index starting from the outside of the
// border going inwards) and an array of line styles, calculate the
// color that that stripe of the border should be rendered in.
static Color ComputeColorForLine(uint32_t aLineIndex,
                                   const BorderColorStyle* aBorderColorStyle,
                                   uint32_t aBorderColorStyleCount,
                                   nscolor aBorderColor,
                                   nscolor aBackgroundColor);

static Color ComputeCompositeColorForLine(uint32_t aLineIndex,
                                          const nsBorderColors* aBorderColors);

// little helper function to check if the array of 4 floats given are
// equal to the given value
static bool
CheckFourFloatsEqual(const Float *vals, Float k)
{
  return (vals[0] == k &&
          vals[1] == k &&
          vals[2] == k &&
          vals[3] == k);
}

static bool
IsZeroSize(const Size& sz) {
  return sz.width == 0.0 || sz.height == 0.0;
}

static bool
AllCornersZeroSize(const RectCornerRadii& corners) {
  return IsZeroSize(corners[NS_CORNER_TOP_LEFT]) &&
    IsZeroSize(corners[NS_CORNER_TOP_RIGHT]) &&
    IsZeroSize(corners[NS_CORNER_BOTTOM_RIGHT]) &&
    IsZeroSize(corners[NS_CORNER_BOTTOM_LEFT]);
}

typedef enum {
  // Normal solid square corner.  Will be rectangular, the size of the
  // adjacent sides.  If the corner has a border radius, the corner
  // will always be solid, since we don't do dotted/dashed etc.
  CORNER_NORMAL,

  // Paint the corner in whatever style is not dotted/dashed of the
  // adjacent corners.
  CORNER_SOLID,

  // Paint the corner as a dot, the size of the bigger of the adjacent
  // sides.
  CORNER_DOT
} CornerStyle;

nsCSSBorderRenderer::nsCSSBorderRenderer(nsPresContext::nsPresContextType aPresContextType,
                                         DrawTarget* aDrawTarget,
                                         Rect& aOuterRect,
                                         const uint8_t* aBorderStyles,
                                         const Float* aBorderWidths,
                                         RectCornerRadii& aBorderRadii,
                                         const nscolor* aBorderColors,
                                         nsBorderColors* const* aCompositeColors,
                                         nscolor aBackgroundColor)
  : mPresContextType(aPresContextType),
    mDrawTarget(aDrawTarget),
    mOuterRect(aOuterRect),
    mBorderStyles(aBorderStyles),
    mBorderWidths(aBorderWidths),
    mBorderRadii(aBorderRadii),
    mBorderColors(aBorderColors),
    mCompositeColors(aCompositeColors),
    mBackgroundColor(aBackgroundColor)
{
  if (!mCompositeColors) {
    static nsBorderColors * const noColors[4] = { nullptr };
    mCompositeColors = &noColors[0];
  }

  mInnerRect = mOuterRect;
  mInnerRect.Deflate(
      Margin(mBorderStyles[0] != NS_STYLE_BORDER_STYLE_NONE ? mBorderWidths[0] : 0,
             mBorderStyles[1] != NS_STYLE_BORDER_STYLE_NONE ? mBorderWidths[1] : 0,
             mBorderStyles[2] != NS_STYLE_BORDER_STYLE_NONE ? mBorderWidths[2] : 0,
             mBorderStyles[3] != NS_STYLE_BORDER_STYLE_NONE ? mBorderWidths[3] : 0));

  ComputeBorderCornerDimensions(mOuterRect, mInnerRect,
                                mBorderRadii, &mBorderCornerDimensions);

  mOneUnitBorder = CheckFourFloatsEqual(mBorderWidths, 1.0);
  mNoBorderRadius = AllCornersZeroSize(mBorderRadii);
  mAvoidStroke = false;
}

/* static */ void
nsCSSBorderRenderer::ComputeInnerRadii(const RectCornerRadii& aRadii,
                                       const Float* aBorderSizes,
                                       RectCornerRadii* aInnerRadiiRet)
{
  RectCornerRadii& iRadii = *aInnerRadiiRet;

  iRadii[C_TL].width = std::max(0.f, aRadii[C_TL].width - aBorderSizes[NS_SIDE_LEFT]);
  iRadii[C_TL].height = std::max(0.f, aRadii[C_TL].height - aBorderSizes[NS_SIDE_TOP]);

  iRadii[C_TR].width = std::max(0.f, aRadii[C_TR].width - aBorderSizes[NS_SIDE_RIGHT]);
  iRadii[C_TR].height = std::max(0.f, aRadii[C_TR].height - aBorderSizes[NS_SIDE_TOP]);

  iRadii[C_BR].width = std::max(0.f, aRadii[C_BR].width - aBorderSizes[NS_SIDE_RIGHT]);
  iRadii[C_BR].height = std::max(0.f, aRadii[C_BR].height - aBorderSizes[NS_SIDE_BOTTOM]);

  iRadii[C_BL].width = std::max(0.f, aRadii[C_BL].width - aBorderSizes[NS_SIDE_LEFT]);
  iRadii[C_BL].height = std::max(0.f, aRadii[C_BL].height - aBorderSizes[NS_SIDE_BOTTOM]);
}

/* static */ void
nsCSSBorderRenderer::ComputeOuterRadii(const RectCornerRadii& aRadii,
                                       const Float* aBorderSizes,
                                       RectCornerRadii* aOuterRadiiRet)
{
  RectCornerRadii& oRadii = *aOuterRadiiRet;

  // default all corners to sharp corners
  oRadii = RectCornerRadii(0.f);

  // round the edges that have radii > 0.0 to start with
  if (aRadii[C_TL].width > 0.f && aRadii[C_TL].height > 0.f) {
    oRadii[C_TL].width = std::max(0.f, aRadii[C_TL].width + aBorderSizes[NS_SIDE_LEFT]);
    oRadii[C_TL].height = std::max(0.f, aRadii[C_TL].height + aBorderSizes[NS_SIDE_TOP]);
  }

  if (aRadii[C_TR].width > 0.f && aRadii[C_TR].height > 0.f) {
    oRadii[C_TR].width = std::max(0.f, aRadii[C_TR].width + aBorderSizes[NS_SIDE_RIGHT]);
    oRadii[C_TR].height = std::max(0.f, aRadii[C_TR].height + aBorderSizes[NS_SIDE_TOP]);
  }

  if (aRadii[C_BR].width > 0.f && aRadii[C_BR].height > 0.f) {
    oRadii[C_BR].width = std::max(0.f, aRadii[C_BR].width + aBorderSizes[NS_SIDE_RIGHT]);
    oRadii[C_BR].height = std::max(0.f, aRadii[C_BR].height + aBorderSizes[NS_SIDE_BOTTOM]);
  }

  if (aRadii[C_BL].width > 0.f && aRadii[C_BL].height > 0.f) {
    oRadii[C_BL].width = std::max(0.f, aRadii[C_BL].width + aBorderSizes[NS_SIDE_LEFT]);
    oRadii[C_BL].height = std::max(0.f, aRadii[C_BL].height + aBorderSizes[NS_SIDE_BOTTOM]);
  }
}

/*static*/ void
ComputeBorderCornerDimensions(const Rect& aOuterRect,
                              const Rect& aInnerRect,
                              const RectCornerRadii& aRadii,
                              RectCornerRadii* aDimsRet)
{
  Float leftWidth = aInnerRect.X() - aOuterRect.X();
  Float topWidth = aInnerRect.Y() - aOuterRect.Y();
  Float rightWidth = aOuterRect.Width() - aInnerRect.Width() - leftWidth;
  Float bottomWidth = aOuterRect.Height() - aInnerRect.Height() - topWidth;

  if (AllCornersZeroSize(aRadii)) {
    // These will always be in pixel units from CSS
    (*aDimsRet)[C_TL] = Size(leftWidth, topWidth);
    (*aDimsRet)[C_TR] = Size(rightWidth, topWidth);
    (*aDimsRet)[C_BR] = Size(rightWidth, bottomWidth);
    (*aDimsRet)[C_BL] = Size(leftWidth, bottomWidth);
  } else {
    // Always round up to whole pixels for the corners; it's safe to
    // make the corners bigger than necessary, and this way we ensure
    // that we avoid seams.
    (*aDimsRet)[C_TL] = Size(ceil(std::max(leftWidth, aRadii[C_TL].width)),
                             ceil(std::max(topWidth, aRadii[C_TL].height)));
    (*aDimsRet)[C_TR] = Size(ceil(std::max(rightWidth, aRadii[C_TR].width)),
                             ceil(std::max(topWidth, aRadii[C_TR].height)));
    (*aDimsRet)[C_BR] = Size(ceil(std::max(rightWidth, aRadii[C_BR].width)),
                             ceil(std::max(bottomWidth, aRadii[C_BR].height)));
    (*aDimsRet)[C_BL] = Size(ceil(std::max(leftWidth, aRadii[C_BL].width)),
                             ceil(std::max(bottomWidth, aRadii[C_BL].height)));
  }
}

bool
nsCSSBorderRenderer::AreBorderSideFinalStylesSame(uint8_t aSides)
{
  NS_ASSERTION(aSides != 0 && (aSides & ~SIDE_BITS_ALL) == 0,
               "AreBorderSidesSame: invalid whichSides!");

  /* First check if the specified styles and colors are the same for all sides */
  int firstStyle = 0;
  NS_FOR_CSS_SIDES (i) {
    if (firstStyle == i) {
      if (((1 << i) & aSides) == 0)
        firstStyle++;
      continue;
    }

    if (((1 << i) & aSides) == 0) {
      continue;
    }

    if (mBorderStyles[firstStyle] != mBorderStyles[i] ||
        mBorderColors[firstStyle] != mBorderColors[i] ||
        !nsBorderColors::Equal(mCompositeColors[firstStyle],
                               mCompositeColors[i]))
      return false;
  }

  /* Then if it's one of the two-tone styles and we're not
   * just comparing the TL or BR sides */
  switch (mBorderStyles[firstStyle]) {
    case NS_STYLE_BORDER_STYLE_GROOVE:
    case NS_STYLE_BORDER_STYLE_RIDGE:
    case NS_STYLE_BORDER_STYLE_INSET:
    case NS_STYLE_BORDER_STYLE_OUTSET:
      return ((aSides & ~(SIDE_BIT_TOP | SIDE_BIT_LEFT)) == 0 ||
              (aSides & ~(SIDE_BIT_BOTTOM | SIDE_BIT_RIGHT)) == 0);
  }

  return true;
}

bool
nsCSSBorderRenderer::IsSolidCornerStyle(uint8_t aStyle, mozilla::css::Corner aCorner)
{
  switch (aStyle) {
    case NS_STYLE_BORDER_STYLE_DOTTED:
    case NS_STYLE_BORDER_STYLE_DASHED:
    case NS_STYLE_BORDER_STYLE_SOLID:
      return true;

    case NS_STYLE_BORDER_STYLE_INSET:
    case NS_STYLE_BORDER_STYLE_OUTSET:
      return (aCorner == NS_CORNER_TOP_LEFT || aCorner == NS_CORNER_BOTTOM_RIGHT);

    case NS_STYLE_BORDER_STYLE_GROOVE:
    case NS_STYLE_BORDER_STYLE_RIDGE:
      return mOneUnitBorder && (aCorner == NS_CORNER_TOP_LEFT || aCorner == NS_CORNER_BOTTOM_RIGHT);

    case NS_STYLE_BORDER_STYLE_DOUBLE:
      return mOneUnitBorder;

    default:
      return false;
  }
}

BorderColorStyle
nsCSSBorderRenderer::BorderColorStyleForSolidCorner(uint8_t aStyle, mozilla::css::Corner aCorner)
{
  // note that this function assumes that the corner is already solid,
  // as per the earlier function
  switch (aStyle) {
    case NS_STYLE_BORDER_STYLE_DOTTED:
    case NS_STYLE_BORDER_STYLE_DASHED:
    case NS_STYLE_BORDER_STYLE_SOLID:
    case NS_STYLE_BORDER_STYLE_DOUBLE:
      return BorderColorStyleSolid;

    case NS_STYLE_BORDER_STYLE_INSET:
    case NS_STYLE_BORDER_STYLE_GROOVE:
      if (aCorner == NS_CORNER_TOP_LEFT)
        return BorderColorStyleDark;
      else if (aCorner == NS_CORNER_BOTTOM_RIGHT)
        return BorderColorStyleLight;
      break;

    case NS_STYLE_BORDER_STYLE_OUTSET:
    case NS_STYLE_BORDER_STYLE_RIDGE:
      if (aCorner == NS_CORNER_TOP_LEFT)
        return BorderColorStyleLight;
      else if (aCorner == NS_CORNER_BOTTOM_RIGHT)
        return BorderColorStyleDark;
      break;
  }

  return BorderColorStyleNone;
}

Rect
nsCSSBorderRenderer::GetCornerRect(mozilla::css::Corner aCorner)
{
  Point offset(0.f, 0.f);

  if (aCorner == C_TR || aCorner == C_BR)
    offset.x = mOuterRect.Width() - mBorderCornerDimensions[aCorner].width;
  if (aCorner == C_BR || aCorner == C_BL)
    offset.y = mOuterRect.Height() - mBorderCornerDimensions[aCorner].height;

  return Rect(mOuterRect.TopLeft() + offset,
              mBorderCornerDimensions[aCorner]);
}

Rect
nsCSSBorderRenderer::GetSideClipWithoutCornersRect(mozilla::css::Side aSide)
{
  Point offset(0.f, 0.f);

  // The offset from the outside rect to the start of this side's
  // box.  For the top and bottom sides, the height of the box
  // must be the border height; the x start must take into account
  // the corner size (which may be bigger than the right or left
  // side's width).  The same applies to the right and left sides.
  if (aSide == NS_SIDE_TOP) {
    offset.x = mBorderCornerDimensions[C_TL].width;
  } else if (aSide == NS_SIDE_RIGHT) {
    offset.x = mOuterRect.Width() - mBorderWidths[NS_SIDE_RIGHT];
    offset.y = mBorderCornerDimensions[C_TR].height;
  } else if (aSide == NS_SIDE_BOTTOM) {
    offset.x = mBorderCornerDimensions[C_BL].width;
    offset.y = mOuterRect.Height() - mBorderWidths[NS_SIDE_BOTTOM];
  } else if (aSide == NS_SIDE_LEFT) {
    offset.y = mBorderCornerDimensions[C_TL].height;
  }

  // The sum of the width & height of the corners adjacent to the
  // side.  This relies on the relationship between side indexing and
  // corner indexing; that is, 0 == SIDE_TOP and 0 == CORNER_TOP_LEFT,
  // with both proceeding clockwise.
  Size sideCornerSum = mBorderCornerDimensions[mozilla::css::Corner(aSide)]
                     + mBorderCornerDimensions[mozilla::css::Corner(NEXT_SIDE(aSide))];
  Rect rect(mOuterRect.TopLeft() + offset,
            mOuterRect.Size() - sideCornerSum);

  if (aSide == NS_SIDE_TOP || aSide == NS_SIDE_BOTTOM)
    rect.height = mBorderWidths[aSide];
  else
    rect.width = mBorderWidths[aSide];

  return rect;
}

// The side border type and the adjacent border types are
// examined and one of the different types of clipping (listed
// below) is selected.

typedef enum {
  // clip to the trapezoid formed by the corners of the
  // inner and outer rectangles for the given side
  SIDE_CLIP_TRAPEZOID,

  // clip to the trapezoid formed by the outer rectangle
  // corners and the center of the region, making sure
  // that diagonal lines all go directly from the outside
  // corner to the inside corner, but that they then continue on
  // to the middle.
  //
  // This is needed for correctly clipping rounded borders,
  // which might extend past the SIDE_CLIP_TRAPEZOID trap.
  SIDE_CLIP_TRAPEZOID_FULL,

  // clip to the rectangle formed by the given side; a specific
  // overlap algorithm is used; see the function for details.
  // this is currently used for dashing.
  SIDE_CLIP_RECTANGLE
} SideClipType;

// Given three points, p0, p1, and midPoint, move p1 further in to the
// rectangle (of which aMidPoint is the center) so that it reaches the
// closer of the horizontal or vertical lines intersecting the midpoint,
// while maintaing the slope of the line.  If p0 and p1 are the same,
// just move p1 to midPoint (since there's no slope to maintain).
// FIXME: Extending only to the midpoint isn't actually sufficient for
// boxes with asymmetric radii.
static void
MaybeMoveToMidPoint(Point& aP0, Point& aP1, const Point& aMidPoint)
{
  Point ps = aP1 - aP0;

  if (ps.x == 0.0) {
    if (ps.y == 0.0) {
      aP1 = aMidPoint;
    } else {
      aP1.y = aMidPoint.y;
    }
  } else {
    if (ps.y == 0.0) {
      aP1.x = aMidPoint.x;
    } else {
      Float k = std::min((aMidPoint.x - aP0.x) / ps.x,
                         (aMidPoint.y - aP0.y) / ps.y);
      aP1 = aP0 + ps * k;
    }
  }
}

already_AddRefed<Path>
nsCSSBorderRenderer::GetSideClipSubPath(mozilla::css::Side aSide)
{
  // the clip proceeds clockwise from the top left corner;
  // so "start" in each case is the start of the region from that side.
  //
  // the final path will be formed like:
  // s0 ------- e0
  // |         /
  // s1 ----- e1
  //
  // that is, the second point will always be on the inside

  Point start[2];
  Point end[2];

#define IS_DASHED_OR_DOTTED(_s)  ((_s) == NS_STYLE_BORDER_STYLE_DASHED || (_s) == NS_STYLE_BORDER_STYLE_DOTTED)
  bool isDashed      = IS_DASHED_OR_DOTTED(mBorderStyles[aSide]);
  bool startIsDashed = IS_DASHED_OR_DOTTED(mBorderStyles[PREV_SIDE(aSide)]);
  bool endIsDashed   = IS_DASHED_OR_DOTTED(mBorderStyles[NEXT_SIDE(aSide)]);
#undef IS_DASHED_OR_DOTTED

  SideClipType startType = SIDE_CLIP_TRAPEZOID;
  SideClipType endType = SIDE_CLIP_TRAPEZOID;

  if (!IsZeroSize(mBorderRadii[mozilla::css::Corner(aSide)]))
    startType = SIDE_CLIP_TRAPEZOID_FULL;
  else if (startIsDashed && isDashed)
    startType = SIDE_CLIP_RECTANGLE;

  if (!IsZeroSize(mBorderRadii[mozilla::css::Corner(NEXT_SIDE(aSide))]))
    endType = SIDE_CLIP_TRAPEZOID_FULL;
  else if (endIsDashed && isDashed)
    endType = SIDE_CLIP_RECTANGLE;

  Point midPoint = mInnerRect.Center();

  start[0] = mOuterRect.CCWCorner(aSide);
  start[1] = mInnerRect.CCWCorner(aSide);

  end[0] = mOuterRect.CWCorner(aSide);
  end[1] = mInnerRect.CWCorner(aSide);

  if (startType == SIDE_CLIP_TRAPEZOID_FULL) {
    MaybeMoveToMidPoint(start[0], start[1], midPoint);
  } else if (startType == SIDE_CLIP_RECTANGLE) {
    if (aSide == NS_SIDE_TOP || aSide == NS_SIDE_BOTTOM)
      start[1] = Point(mOuterRect.CCWCorner(aSide).x, mInnerRect.CCWCorner(aSide).y);
    else
      start[1] = Point(mInnerRect.CCWCorner(aSide).x, mOuterRect.CCWCorner(aSide).y);
  }

  if (endType == SIDE_CLIP_TRAPEZOID_FULL) {
    MaybeMoveToMidPoint(end[0], end[1], midPoint);
  } else if (endType == SIDE_CLIP_RECTANGLE) {
    if (aSide == NS_SIDE_TOP || aSide == NS_SIDE_BOTTOM)
      end[0] = Point(mInnerRect.CWCorner(aSide).x, mOuterRect.CWCorner(aSide).y);
    else
      end[0] = Point(mOuterRect.CWCorner(aSide).x, mInnerRect.CWCorner(aSide).y);
  }

  RefPtr<PathBuilder> builder = mDrawTarget->CreatePathBuilder();
  builder->MoveTo(start[0]);
  builder->LineTo(end[0]);
  builder->LineTo(end[1]);
  builder->LineTo(start[1]);
  builder->Close();
  return builder->Finish();
}

void
nsCSSBorderRenderer::FillSolidBorder(const Rect& aOuterRect,
                                     const Rect& aInnerRect,
                                     const RectCornerRadii& aBorderRadii,
                                     const Float* aBorderSizes,
                                     int aSides,
                                     const ColorPattern& aColor)
{
  // Note that this function is allowed to draw more than just the
  // requested sides.

  // If we have a border radius, do full rounded rectangles
  // and fill, regardless of what sides we're asked to draw.
  if (!AllCornersZeroSize(aBorderRadii)) {
    RefPtr<PathBuilder> builder = mDrawTarget->CreatePathBuilder();

    RectCornerRadii innerRadii;
    ComputeInnerRadii(aBorderRadii, aBorderSizes, &innerRadii);

    // do the outer border
    AppendRoundedRectToPath(builder, aOuterRect, aBorderRadii, true);

    // then do the inner border CCW
    AppendRoundedRectToPath(builder, aInnerRect, innerRadii, false);

    RefPtr<Path> path = builder->Finish();

    mDrawTarget->Fill(path, aColor);
    return;
  }

  // If we're asked to draw all sides of an equal-sized border,
  // stroking is fastest.  This is a fairly common path, but partial
  // sides is probably second in the list -- there are a bunch of
  // common border styles, such as inset and outset, that are
  // top-left/bottom-right split.
  if (aSides == SIDE_BITS_ALL &&
      CheckFourFloatsEqual(aBorderSizes, aBorderSizes[0]) &&
      !mAvoidStroke)
  {
    Float strokeWidth = aBorderSizes[0];
    Rect r(aOuterRect);
    r.Deflate(strokeWidth / 2.f);
    mDrawTarget->StrokeRect(r, aColor, StrokeOptions(strokeWidth));
    return;
  }

  // Otherwise, we have unequal sized borders or we're only
  // drawing some sides; create rectangles for each side
  // and fill them.

  Rect r[4];

  // compute base rects for each side
  if (aSides & SIDE_BIT_TOP) {
    r[NS_SIDE_TOP] =
        Rect(aOuterRect.X(), aOuterRect.Y(),
             aOuterRect.Width(), aBorderSizes[NS_SIDE_TOP]);
  }

  if (aSides & SIDE_BIT_BOTTOM) {
    r[NS_SIDE_BOTTOM] =
        Rect(aOuterRect.X(), aOuterRect.YMost() - aBorderSizes[NS_SIDE_BOTTOM],
             aOuterRect.Width(), aBorderSizes[NS_SIDE_BOTTOM]);
  }

  if (aSides & SIDE_BIT_LEFT) {
    r[NS_SIDE_LEFT] =
        Rect(aOuterRect.X(), aOuterRect.Y(),
             aBorderSizes[NS_SIDE_LEFT], aOuterRect.Height());
  }

  if (aSides & SIDE_BIT_RIGHT) {
    r[NS_SIDE_RIGHT] =
        Rect(aOuterRect.XMost() - aBorderSizes[NS_SIDE_RIGHT], aOuterRect.Y(),
             aBorderSizes[NS_SIDE_RIGHT], aOuterRect.Height());
  }

  // If two sides meet at a corner that we're rendering, then
  // make sure that we adjust one of the sides to avoid overlap.
  // This is especially important in the case of colors with
  // an alpha channel.

  if ((aSides & (SIDE_BIT_TOP | SIDE_BIT_LEFT)) == (SIDE_BIT_TOP | SIDE_BIT_LEFT)) {
    // adjust the left's top down a bit
    r[NS_SIDE_LEFT].y += aBorderSizes[NS_SIDE_TOP];
    r[NS_SIDE_LEFT].height -= aBorderSizes[NS_SIDE_TOP];
  }

  if ((aSides & (SIDE_BIT_TOP | SIDE_BIT_RIGHT)) == (SIDE_BIT_TOP | SIDE_BIT_RIGHT)) {
    // adjust the top's left a bit
    r[NS_SIDE_TOP].width -= aBorderSizes[NS_SIDE_RIGHT];
  }

  if ((aSides & (SIDE_BIT_BOTTOM | SIDE_BIT_RIGHT)) == (SIDE_BIT_BOTTOM | SIDE_BIT_RIGHT)) {
    // adjust the right's bottom a bit
    r[NS_SIDE_RIGHT].height -= aBorderSizes[NS_SIDE_BOTTOM];
  }

  if ((aSides & (SIDE_BIT_BOTTOM | SIDE_BIT_LEFT)) == (SIDE_BIT_BOTTOM | SIDE_BIT_LEFT)) {
    // adjust the bottom's left a bit
    r[NS_SIDE_BOTTOM].x += aBorderSizes[NS_SIDE_LEFT];
    r[NS_SIDE_BOTTOM].width -= aBorderSizes[NS_SIDE_LEFT];
  }

  // Filling these one by one is faster than filling them all at once.
  for (uint32_t i = 0; i < 4; i++) {
    if (aSides & (1 << i)) {
      MaybeSnapToDevicePixels(r[i], *mDrawTarget, true);
      mDrawTarget->FillRect(r[i], aColor);
    }
  }
}

Color
MakeBorderColor(nscolor aColor, nscolor aBackgroundColor,
                BorderColorStyle aBorderColorStyle)
{
  nscolor colors[2];
  int k = 0;

  switch (aBorderColorStyle) {
    case BorderColorStyleNone:
      return Color(0.f, 0.f, 0.f, 0.f); // transparent black

    case BorderColorStyleLight:
      k = 1;
      /* fall through */
    case BorderColorStyleDark:
      NS_GetSpecial3DColors(colors, aBackgroundColor, aColor);
      return Color::FromABGR(colors[k]);

    case BorderColorStyleSolid:
    default:
      return Color::FromABGR(aColor);
  }
}

Color
ComputeColorForLine(uint32_t aLineIndex,
                    const BorderColorStyle* aBorderColorStyle,
                    uint32_t aBorderColorStyleCount,
                    nscolor aBorderColor,
                    nscolor aBackgroundColor)
{
  NS_ASSERTION(aLineIndex < aBorderColorStyleCount, "Invalid lineIndex given");

  return MakeBorderColor(aBorderColor, aBackgroundColor,
                         aBorderColorStyle[aLineIndex]);
}

Color
ComputeCompositeColorForLine(uint32_t aLineIndex,
                             const nsBorderColors* aBorderColors)
{
  while (aLineIndex-- && aBorderColors->mNext)
    aBorderColors = aBorderColors->mNext;

  return Color::FromABGR(aBorderColors->mColor);
}

void
nsCSSBorderRenderer::DrawBorderSidesCompositeColors(int aSides, const nsBorderColors *aCompositeColors)
{
  RectCornerRadii radii = mBorderRadii;

  // the generic composite colors path; each border is 1px in size
  Rect soRect = mOuterRect;
  Float maxBorderWidth = 0;
  NS_FOR_CSS_SIDES (i) {
    maxBorderWidth = std::max(maxBorderWidth, Float(mBorderWidths[i]));
  }

  Float fakeBorderSizes[4];

  Point itl = mInnerRect.TopLeft();
  Point ibr = mInnerRect.BottomRight();

  for (uint32_t i = 0; i < uint32_t(maxBorderWidth); i++) {
    ColorPattern color(ToDeviceColor(
                         ComputeCompositeColorForLine(i, aCompositeColors)));

    Rect siRect = soRect;
    siRect.Deflate(1.0);

    // now cap the rects to the real mInnerRect
    Point tl = siRect.TopLeft();
    Point br = siRect.BottomRight();

    tl.x = std::min(tl.x, itl.x);
    tl.y = std::min(tl.y, itl.y);

    br.x = std::max(br.x, ibr.x);
    br.y = std::max(br.y, ibr.y);

    siRect = Rect(tl.x, tl.y, br.x - tl.x , br.y - tl.y);

    fakeBorderSizes[NS_SIDE_TOP] = siRect.TopLeft().y - soRect.TopLeft().y;
    fakeBorderSizes[NS_SIDE_RIGHT] = soRect.TopRight().x - siRect.TopRight().x;
    fakeBorderSizes[NS_SIDE_BOTTOM] = soRect.BottomRight().y - siRect.BottomRight().y;
    fakeBorderSizes[NS_SIDE_LEFT] = siRect.BottomLeft().x - soRect.BottomLeft().x;

    FillSolidBorder(soRect, siRect, radii, fakeBorderSizes, aSides, color);

    soRect = siRect;

    ComputeInnerRadii(radii, fakeBorderSizes, &radii);
  }
}

void
nsCSSBorderRenderer::DrawBorderSides(int aSides)
{
  if (aSides == 0 || (aSides & ~SIDE_BITS_ALL) != 0) {
    NS_WARNING("DrawBorderSides: invalid sides!");
    return;
  }

  uint8_t borderRenderStyle = NS_STYLE_BORDER_STYLE_NONE;
  nscolor borderRenderColor;
  const nsBorderColors *compositeColors = nullptr;

  uint32_t borderColorStyleCount = 0;
  BorderColorStyle borderColorStyleTopLeft[3], borderColorStyleBottomRight[3];
  BorderColorStyle *borderColorStyle = nullptr;

  NS_FOR_CSS_SIDES (i) {
    if ((aSides & (1 << i)) == 0)
      continue;
    borderRenderStyle = mBorderStyles[i];
    borderRenderColor = mBorderColors[i];
    compositeColors = mCompositeColors[i];
    break;
  }

  if (borderRenderStyle == NS_STYLE_BORDER_STYLE_NONE ||
      borderRenderStyle == NS_STYLE_BORDER_STYLE_HIDDEN)
    return;

  // -moz-border-colors is a hack; if we have it for a border, then
  // it's always drawn solid, and each color is given 1px.  The last
  // color is used for the remainder of the border's size.  Just
  // hand off to another function to do all that.
  if (compositeColors) {
    DrawBorderSidesCompositeColors(aSides, compositeColors);
    return;
  }

  // We're not doing compositeColors, so we can calculate the
  // borderColorStyle based on the specified style.  The
  // borderColorStyle array goes from the outer to the inner style.
  //
  // If the border width is 1, we need to change the borderRenderStyle
  // a bit to make sure that we get the right colors -- e.g. 'ridge'
  // with a 1px border needs to look like solid, not like 'outset'.
  if (mOneUnitBorder &&
      (borderRenderStyle == NS_STYLE_BORDER_STYLE_RIDGE ||
       borderRenderStyle == NS_STYLE_BORDER_STYLE_GROOVE ||
       borderRenderStyle == NS_STYLE_BORDER_STYLE_DOUBLE))
    borderRenderStyle = NS_STYLE_BORDER_STYLE_SOLID;

  switch (borderRenderStyle) {
    case NS_STYLE_BORDER_STYLE_SOLID:
    case NS_STYLE_BORDER_STYLE_DASHED:
    case NS_STYLE_BORDER_STYLE_DOTTED:
      borderColorStyleTopLeft[0] = BorderColorStyleSolid;

      borderColorStyleBottomRight[0] = BorderColorStyleSolid;

      borderColorStyleCount = 1;
      break;

    case NS_STYLE_BORDER_STYLE_GROOVE:
      borderColorStyleTopLeft[0] = BorderColorStyleDark;
      borderColorStyleTopLeft[1] = BorderColorStyleLight;

      borderColorStyleBottomRight[0] = BorderColorStyleLight;
      borderColorStyleBottomRight[1] = BorderColorStyleDark;

      borderColorStyleCount = 2;
      break;

    case NS_STYLE_BORDER_STYLE_RIDGE:
      borderColorStyleTopLeft[0] = BorderColorStyleLight;
      borderColorStyleTopLeft[1] = BorderColorStyleDark;

      borderColorStyleBottomRight[0] = BorderColorStyleDark;
      borderColorStyleBottomRight[1] = BorderColorStyleLight;

      borderColorStyleCount = 2;
      break;

    case NS_STYLE_BORDER_STYLE_DOUBLE:
      borderColorStyleTopLeft[0] = BorderColorStyleSolid;
      borderColorStyleTopLeft[1] = BorderColorStyleNone;
      borderColorStyleTopLeft[2] = BorderColorStyleSolid;

      borderColorStyleBottomRight[0] = BorderColorStyleSolid;
      borderColorStyleBottomRight[1] = BorderColorStyleNone;
      borderColorStyleBottomRight[2] = BorderColorStyleSolid;

      borderColorStyleCount = 3;
      break;

    case NS_STYLE_BORDER_STYLE_INSET:
      borderColorStyleTopLeft[0] = BorderColorStyleDark;
      borderColorStyleBottomRight[0] = BorderColorStyleLight;

      borderColorStyleCount = 1;
      break;

    case NS_STYLE_BORDER_STYLE_OUTSET:
      borderColorStyleTopLeft[0] = BorderColorStyleLight;
      borderColorStyleBottomRight[0] = BorderColorStyleDark;

      borderColorStyleCount = 1;
      break;

    default:
      NS_NOTREACHED("Unhandled border style!!");
      break;
  }

  // The only way to get to here is by having a
  // borderColorStyleCount < 1 or > 3; this should never happen,
  // since -moz-border-colors doesn't get handled here.
  NS_ASSERTION(borderColorStyleCount > 0 && borderColorStyleCount < 4,
               "Non-border-colors case with borderColorStyleCount < 1 or > 3; what happened?");

  // The caller should never give us anything with a mix
  // of TL/BR if the border style would require a
  // TL/BR split.
  if (aSides & (SIDE_BIT_BOTTOM | SIDE_BIT_RIGHT))
    borderColorStyle = borderColorStyleBottomRight;
  else
    borderColorStyle = borderColorStyleTopLeft;

  // Distribute the border across the available space.
  Float borderWidths[3][4];

  if (borderColorStyleCount == 1) {
    NS_FOR_CSS_SIDES (i) {
      borderWidths[0][i] = mBorderWidths[i];
    }
  } else if (borderColorStyleCount == 2) {
    // with 2 color styles, any extra pixel goes to the outside
    NS_FOR_CSS_SIDES (i) {
      borderWidths[0][i] = int32_t(mBorderWidths[i]) / 2 + int32_t(mBorderWidths[i]) % 2;
      borderWidths[1][i] = int32_t(mBorderWidths[i]) / 2;
    }
  } else if (borderColorStyleCount == 3) {
    // with 3 color styles, any extra pixel (or lack of extra pixel)
    // goes to the middle
    NS_FOR_CSS_SIDES (i) {
      if (mBorderWidths[i] == 1.0) {
        borderWidths[0][i] = 1.f;
        borderWidths[1][i] = borderWidths[2][i] = 0.f;
      } else {
        int32_t rest = int32_t(mBorderWidths[i]) % 3;
        borderWidths[0][i] = borderWidths[2][i] = borderWidths[1][i] = (int32_t(mBorderWidths[i]) - rest) / 3;

        if (rest == 1) {
          borderWidths[1][i] += 1.f;
        } else if (rest == 2) {
          borderWidths[0][i] += 1.f;
          borderWidths[2][i] += 1.f;
        }
      }
    }
  }

  // make a copy that we can modify
  RectCornerRadii radii = mBorderRadii;

  Rect soRect(mOuterRect);
  Rect siRect(mOuterRect);

  for (unsigned int i = 0; i < borderColorStyleCount; i++) {
    // walk siRect inwards at the start of the loop to get the
    // correct inner rect.
    siRect.Deflate(Margin(borderWidths[i][0], borderWidths[i][1],
                          borderWidths[i][2], borderWidths[i][3]));

    if (borderColorStyle[i] != BorderColorStyleNone) {
      Color c = ComputeColorForLine(i, borderColorStyle, borderColorStyleCount,
                                    borderRenderColor, mBackgroundColor);
      ColorPattern color(ToDeviceColor(c));

      FillSolidBorder(soRect, siRect, radii, borderWidths[i], aSides, color);
    }

    ComputeInnerRadii(radii, borderWidths[i], &radii);

    // And now soRect is the same as siRect, for the next line in.
    soRect = siRect;
  }
}

void
nsCSSBorderRenderer::DrawDashedSide(mozilla::css::Side aSide)
{
  Float dashWidth;
  Float dash[2];

  uint8_t style = mBorderStyles[aSide];
  Float borderWidth = mBorderWidths[aSide];
  nscolor borderColor = mBorderColors[aSide];

  if (borderWidth == 0.0)
    return;

  if (style == NS_STYLE_BORDER_STYLE_NONE ||
      style == NS_STYLE_BORDER_STYLE_HIDDEN)
    return;

  StrokeOptions strokeOptions(borderWidth);

  if (style == NS_STYLE_BORDER_STYLE_DASHED) {
    dashWidth = Float(borderWidth * DOT_LENGTH * DASH_LENGTH);

    dash[0] = dashWidth;
    dash[1] = dashWidth;
  } else if (style == NS_STYLE_BORDER_STYLE_DOTTED) {
    dashWidth = Float(borderWidth * DOT_LENGTH);

    if (borderWidth > 2.0) {
      dash[0] = 0.0;
      dash[1] = dashWidth * 2.0;
      strokeOptions.mLineCap = CapStyle::ROUND;
    } else {
      dash[0] = dashWidth;
      dash[1] = dashWidth;
    }
  } else {
    PrintAsFormatString("DrawDashedSide: style: %d!!\n", style);
    NS_ERROR("DrawDashedSide called with style other than DASHED or DOTTED; someone's not playing nice");
    return;
  }

  PrintAsFormatString("dash: %f %f\n", dash[0], dash[1]);

  strokeOptions.mDashPattern = dash;
  strokeOptions.mDashLength = MOZ_ARRAY_LENGTH(dash);

  Point start = mOuterRect.CCWCorner(aSide);
  Point end = mOuterRect.CWCorner(aSide);

  if (aSide == NS_SIDE_TOP) {
    start.x += mBorderCornerDimensions[C_TL].width;
    end.x -= mBorderCornerDimensions[C_TR].width;

    start.y += borderWidth / 2.0;
    end.y += borderWidth / 2.0;
  } else if (aSide == NS_SIDE_RIGHT) {
    start.x -= borderWidth / 2.0;
    end.x -= borderWidth / 2.0;

    start.y += mBorderCornerDimensions[C_TR].height;
    end.y -= mBorderCornerDimensions[C_BR].height;
  } else if (aSide == NS_SIDE_BOTTOM) {
    start.x -= mBorderCornerDimensions[C_BR].width;
    end.x += mBorderCornerDimensions[C_BL].width;

    start.y -= borderWidth / 2.0;
    end.y -= borderWidth / 2.0;
  } else if (aSide == NS_SIDE_LEFT) {
    start.x += borderWidth / 2.0;
    end.x += borderWidth / 2.0;

    start.y -= mBorderCornerDimensions[C_BL].height;
    end.y += mBorderCornerDimensions[C_TL].height;
  }

  mDrawTarget->StrokeLine(start, end, ColorPattern(ToDeviceColor(borderColor)),
                          strokeOptions);
}

bool
nsCSSBorderRenderer::AllBordersSameWidth()
{
  if (mBorderWidths[0] == mBorderWidths[1] &&
      mBorderWidths[0] == mBorderWidths[2] &&
      mBorderWidths[0] == mBorderWidths[3])
  {
    return true;
  }

  return false;
}

bool
nsCSSBorderRenderer::AllBordersSolid(bool *aHasCompositeColors)
{
  *aHasCompositeColors = false;
  NS_FOR_CSS_SIDES(i) {
    if (mCompositeColors[i] != nullptr) {
      *aHasCompositeColors = true;
    }
    if (mBorderStyles[i] == NS_STYLE_BORDER_STYLE_SOLID ||
        mBorderStyles[i] == NS_STYLE_BORDER_STYLE_NONE ||
        mBorderStyles[i] == NS_STYLE_BORDER_STYLE_HIDDEN)
    {
      continue;
    }
    return false;
  }

  return true;
}

bool IsVisible(int aStyle)
{
  if (aStyle != NS_STYLE_BORDER_STYLE_NONE &&
      aStyle != NS_STYLE_BORDER_STYLE_HIDDEN) {
        return true;
  }
  return false;
}

struct twoFloats
{
    Float a, b;

    twoFloats operator*(const Size& aSize) const {
      return { a * aSize.width, b * aSize.height };
    }

    twoFloats operator*(Float aScale) const {
      return { a * aScale, b * aScale };
    }

    twoFloats operator+(const Point& aPoint) const {
      return { a + aPoint.x, b + aPoint.y };
    }

    operator Point() const {
      return Point(a, b);
    }
};

void
nsCSSBorderRenderer::DrawSingleWidthSolidBorder()
{
  // Easy enough to deal with.
  Rect rect = mOuterRect;
  rect.Deflate(0.5);

  const twoFloats cornerAdjusts[4] = { { +0.5,  0   },
                                       {    0, +0.5 },
                                       { -0.5,  0   },
                                       {    0, -0.5 } };
  NS_FOR_CSS_SIDES(side) {
    Point firstCorner = rect.CCWCorner(side) + cornerAdjusts[side];
    Point secondCorner = rect.CWCorner(side) + cornerAdjusts[side];

    ColorPattern color(ToDeviceColor(mBorderColors[side]));

    mDrawTarget->StrokeLine(firstCorner, secondCorner, color);
  }
}

// Intersect a ray from the inner corner to the outer corner
// with the border radius, yielding the intersection point.
static Point
IntersectBorderRadius(const Point& aCenter, const Size& aRadius,
                      const Point& aInnerCorner,
                      const Point& aCornerDirection)
{
  Point toCorner = aCornerDirection;
  // transform to-corner ray to unit-circle space
  toCorner.x /= aRadius.width;
  toCorner.y /= aRadius.height;
  // normalize to-corner ray
  Float cornerDist = toCorner.Length();
  if (cornerDist < 1.0e-6f) {
    return aInnerCorner;
  }
  toCorner = toCorner / cornerDist;
  // ray from inner corner to border radius center
  Point toCenter = aCenter - aInnerCorner;
  // transform to-center ray to unit-circle space
  toCenter.x /= aRadius.width;
  toCenter.y /= aRadius.height;
  // compute offset of intersection with border radius unit circle
  Float offset = toCenter.DotProduct(toCorner);
  // compute discriminant to check for intersections
  Float discrim = 1.0f - toCenter.DotProduct(toCenter) + offset * offset;
  // choose farthest intersection
  offset += sqrtf(std::max(discrim, 0.0f));
  // transform to-corner ray back out of unit-circle space
  toCorner.x *= aRadius.width;
  toCorner.y *= aRadius.height;
  return aInnerCorner + toCorner * offset;
}

// Calculate the split point and split angle for a border radius with
// differing sides.
static inline void
SplitBorderRadius(const Point& aCenter, const Size& aRadius,
                  const Point& aOuterCorner, const Point& aInnerCorner,
                  const twoFloats& aCornerMults, Float aStartAngle,
                  Point& aSplit, Float& aSplitAngle)
{
  Point cornerDir = aOuterCorner - aInnerCorner;
  if (cornerDir.x == cornerDir.y && aRadius.IsSquare()) {
    // optimize 45-degree intersection with circle since we can assume
    // the circle center lies along the intersection edge
    aSplit = aCenter - aCornerMults * (aRadius * Float(1.0f / M_SQRT2));
    aSplitAngle = aStartAngle + 0.5f * M_PI / 2.0f;
  } else {
    aSplit = IntersectBorderRadius(aCenter, aRadius, aInnerCorner, cornerDir);
    aSplitAngle =
      atan2f((aSplit.y - aCenter.y) / aRadius.height,
             (aSplit.x - aCenter.x) / aRadius.width);
  }
}

// Compute the size of the skirt needed, given the color alphas
// of each corner side and the slope between them.
static void
ComputeCornerSkirtSize(Float aAlpha1, Float aAlpha2,
                       Float aSlopeY, Float aSlopeX,
                       Float& aSizeResult, Float& aSlopeResult)
{
  // If either side is (almost) invisible or there is no diagonal edge,
  // then don't try to render a skirt.
  if (aAlpha1 < 0.01f || aAlpha2 < 0.01f) {
    return;
  }
  aSlopeX = fabs(aSlopeX);
  aSlopeY = fabs(aSlopeY);
  if (aSlopeX < 1.0e-6f || aSlopeY < 1.0e-6f) {
    return;
  }

  // If first and second color don't match, we need to split the corner in
  // half. The diagonal edges created may not have full pixel coverage given
  // anti-aliasing, so we need to compute a small subpixel skirt edge. This
  // assumes each half has half coverage to start with, and that coverage
  // increases as the skirt is pushed over, with the end result that we want
  // to roughly preserve the alpha value along this edge.
  // Given slope m, alphas a and A, use quadratic formula to solve for S in:
  //   a*(1 - 0.5*(1-S)*(1-mS))*(1 - 0.5*A) + 0.5*A = A
  // yielding:
  //   S = ((1+m) - sqrt((1+m)*(1+m) + 4*m*(1 - A/(a*(1-0.5*A))))) / (2*m)
  // and substitute k = (1+m)/(2*m):
  //   S = k - sqrt(k*k + (1 - A/(a*(1-0.5*A)))/m)
  Float slope = aSlopeY / aSlopeX;
  Float slopeScale = (1.0f + slope) / (2.0f * slope);
  Float discrim =
    slopeScale*slopeScale +
      (1 - aAlpha2 / (aAlpha1 * (1.0f - 0.49f * aAlpha2))) / slope;
  if (discrim >= 0) {
    aSizeResult = slopeScale - sqrtf(discrim);
    aSlopeResult = slope;
  }
}

// Draws a border radius with possibly different sides.
// A skirt is drawn underneath the corner intersection to hide possible
// seams when anti-aliased drawing is used.
// As an optimization, this tries to combine the drawing of the side itself
// with the drawing of the border radius where possible.
static void
DrawBorderRadius(DrawTarget* aDrawTarget,
                 const Point& aSideStart, Float aSideWidth,
                 mozilla::css::Corner c,
                 const Point& aOuterCorner, const Point& aInnerCorner,
                 const twoFloats& aCornerMultPrev, const twoFloats& aCornerMultNext,
                 const Size& aCornerDims,
                 const Size& aOuterRadius, const Size& aInnerRadius,
                 const Color& aFirstColor, const Color& aSecondColor,
                 Float aSkirtSize, Float aSkirtSlope)
{
  // Connect edge to outer arc start point
  Point outerCornerStart = aOuterCorner + aCornerMultPrev * aCornerDims;
  // Connect edge to outer arc end point
  Point outerCornerEnd = aOuterCorner + aCornerMultNext * aCornerDims;
  // Connect edge to inner arc start point
  Point innerCornerStart =
    outerCornerStart +
      aCornerMultNext * (aCornerDims - aInnerRadius);
  // Connect edge to inner arc end point
  Point innerCornerEnd =
    outerCornerEnd +
      aCornerMultPrev * (aCornerDims - aInnerRadius);

  // Outer arc start point
  Point outerArcStart = aOuterCorner + aCornerMultPrev * aOuterRadius;
  // Outer arc end point
  Point outerArcEnd = aOuterCorner + aCornerMultNext * aOuterRadius;
  // Inner arc start point
  Point innerArcStart = aInnerCorner + aCornerMultPrev * aInnerRadius;
  // Inner arc end point
  Point innerArcEnd = aInnerCorner + aCornerMultNext * aInnerRadius;

  // Outer radius center
  Point outerCenter = aOuterCorner + (aCornerMultPrev + aCornerMultNext) * aOuterRadius;
  // Inner radius center
  Point innerCenter = aInnerCorner + (aCornerMultPrev + aCornerMultNext) * aInnerRadius;

  RefPtr<PathBuilder> builder;
  RefPtr<Path> path;

  if (aFirstColor.a > 0) {
    builder = aDrawTarget->CreatePathBuilder();
    // Combine stroke with corner if color matches.
    if (aSideWidth > 0) {
      builder->MoveTo(aSideStart + aCornerMultNext * aSideWidth);
      builder->LineTo(aSideStart);
      builder->LineTo(outerCornerStart);
    } else {
      builder->MoveTo(outerCornerStart);
    }
  }

  if (aFirstColor != aSecondColor) {
    // Start and end angles of corner quadrant
    Float startAngle = (c * M_PI) / 2.0f - M_PI,
          endAngle = startAngle + M_PI / 2.0f,
          outerSplitAngle, innerSplitAngle;
    Point outerSplit, innerSplit;

    // Outer half-way point
    SplitBorderRadius(outerCenter, aOuterRadius, aOuterCorner, aInnerCorner,
                      aCornerMultPrev + aCornerMultNext, startAngle,
                      outerSplit, outerSplitAngle);
    // Inner half-way point
    if (aInnerRadius.IsEmpty()) {
      innerSplit = aInnerCorner;
      innerSplitAngle = endAngle;
    } else {
      SplitBorderRadius(innerCenter, aInnerRadius, aOuterCorner, aInnerCorner,
                        aCornerMultPrev + aCornerMultNext, startAngle,
                        innerSplit, innerSplitAngle);
    }

    // Draw first half with first color
    if (aFirstColor.a > 0) {
      AcuteArcToBezier(builder.get(), outerCenter, aOuterRadius,
                       outerArcStart, outerSplit, startAngle, outerSplitAngle);
      // Draw skirt as part of first half
      if (aSkirtSize > 0) {
        builder->LineTo(outerSplit + aCornerMultNext * aSkirtSize);
        builder->LineTo(innerSplit - aCornerMultPrev * (aSkirtSize * aSkirtSlope));
      }
      AcuteArcToBezier(builder.get(), innerCenter, aInnerRadius,
                       innerSplit, innerArcStart, innerSplitAngle, startAngle);
      if ((innerCornerStart - innerArcStart).DotProduct(aCornerMultPrev) > 0) {
        builder->LineTo(innerCornerStart);
      }
      builder->Close();
      path = builder->Finish();
      aDrawTarget->Fill(path, ColorPattern(aFirstColor));
    }

    // Draw second half with second color
    if (aSecondColor.a > 0) {
      builder = aDrawTarget->CreatePathBuilder();
      builder->MoveTo(outerCornerEnd);
      if ((innerArcEnd - innerCornerEnd).DotProduct(aCornerMultNext) < 0) {
        builder->LineTo(innerCornerEnd);
      }
      AcuteArcToBezier(builder.get(), innerCenter, aInnerRadius,
                       innerArcEnd, innerSplit, endAngle, innerSplitAngle);
      AcuteArcToBezier(builder.get(), outerCenter, aOuterRadius,
                       outerSplit, outerArcEnd, outerSplitAngle, endAngle);
      builder->Close();
      path = builder->Finish();
      aDrawTarget->Fill(path, ColorPattern(aSecondColor));
    }
  } else if (aFirstColor.a > 0) {
    // Draw corner with single color
    AcuteArcToBezier(builder.get(), outerCenter, aOuterRadius,
                     outerArcStart, outerArcEnd);
    builder->LineTo(outerCornerEnd);
    if ((innerArcEnd - innerCornerEnd).DotProduct(aCornerMultNext) < 0) {
      builder->LineTo(innerCornerEnd);
    }
    AcuteArcToBezier(builder.get(), innerCenter, aInnerRadius,
                     innerArcEnd, innerArcStart, -kKappaFactor);
    if ((innerCornerStart - innerArcStart).DotProduct(aCornerMultPrev) > 0) {
      builder->LineTo(innerCornerStart);
    }
    builder->Close();
    path = builder->Finish();
    aDrawTarget->Fill(path, ColorPattern(aFirstColor));
  }
}

// Draw a corner with possibly different sides.
// A skirt is drawn underneath the corner intersection to hide possible
// seams when anti-aliased drawing is used.
// As an optimization, this tries to combine the drawing of the side itself
// with the drawing of the corner where possible.
static void
DrawCorner(DrawTarget* aDrawTarget,
           const Point& aSideStart, Float aSideWidth,
           mozilla::css::Corner c,
           const Point& aOuterCorner, const Point& aInnerCorner,
           const twoFloats& aCornerMultPrev, const twoFloats& aCornerMultNext,
           const Size& aCornerDims,
           const Color& aFirstColor, const Color& aSecondColor,
           Float aSkirtSize, Float aSkirtSlope)
{
  // Corner box start point
  Point cornerStart = aOuterCorner + aCornerMultPrev * aCornerDims;
  // Corner box end point
  Point cornerEnd = aOuterCorner + aCornerMultNext * aCornerDims;

  RefPtr<PathBuilder> builder;
  RefPtr<Path> path;

  if (aFirstColor.a > 0) {
    builder = aDrawTarget->CreatePathBuilder();
    // Combine stroke with corner if color matches.
    if (aSideWidth > 0) {
      builder->MoveTo(aSideStart + aCornerMultNext * aSideWidth);
      builder->LineTo(aSideStart);
    } else {
      builder->MoveTo(cornerStart);
    }
  }

  if (aFirstColor != aSecondColor) {
    // Draw first half with first color
    if (aFirstColor.a > 0) {
      builder->LineTo(aOuterCorner);
      // Draw skirt as part of first half
      if (aSkirtSize > 0) {
        builder->LineTo(aOuterCorner + aCornerMultNext * aSkirtSize);
        builder->LineTo(aInnerCorner - aCornerMultPrev * (aSkirtSize * aSkirtSlope));
      }
      builder->LineTo(aInnerCorner);
      builder->Close();
      path = builder->Finish();
      aDrawTarget->Fill(path, ColorPattern(aFirstColor));
    }

    // Draw second half with second color
    if (aSecondColor.a > 0) {
      builder = aDrawTarget->CreatePathBuilder();
      builder->MoveTo(cornerEnd);
      builder->LineTo(aInnerCorner);
      builder->LineTo(aOuterCorner);
      builder->Close();
      path = builder->Finish();
      aDrawTarget->Fill(path, ColorPattern(aSecondColor));
    }
  } else if (aFirstColor.a > 0) {
    // Draw corner with single color
    builder->LineTo(aOuterCorner);
    builder->LineTo(cornerEnd);
    builder->LineTo(aInnerCorner);
    builder->Close();
    path = builder->Finish();
    aDrawTarget->Fill(path, ColorPattern(aFirstColor));
  }
}

void
nsCSSBorderRenderer::DrawNoCompositeColorSolidBorder()
{
  const twoFloats cornerMults[4] = { { -1,  0 },
                                     {  0, -1 },
                                     { +1,  0 },
                                     {  0, +1 } };

  const twoFloats centerAdjusts[4] = { { 0, +0.5 },
                                       { -0.5, 0 },
                                       { 0, -0.5 },
                                       { +0.5, 0 } };

  RectCornerRadii innerRadii;
  ComputeInnerRadii(mBorderRadii, mBorderWidths, &innerRadii);

  Rect strokeRect = mOuterRect;
  strokeRect.Deflate(Margin(mBorderWidths[0] / 2.0, mBorderWidths[1] / 2.0,
                            mBorderWidths[2] / 2.0, mBorderWidths[3] / 2.0));

  NS_FOR_CSS_SIDES(i) {
    // We now draw the current side and the CW corner following it.
    // The CCW corner of this side was already drawn in the previous iteration.
    // The side will either be drawn as an explicit stroke or combined
    // with the drawing of the CW corner.
    // If the next side does not have a matching color, then we split the
    // corner into two halves, one of each side's color and draw both.
    // Thus, the CCW corner of the next side will end up drawn here.

    // the corner index -- either 1 2 3 0 (cw) or 0 3 2 1 (ccw)
    mozilla::css::Corner c = mozilla::css::Corner((i+1) % 4);
    mozilla::css::Corner prevCorner = mozilla::css::Corner(i);

    // i+2 and i+3 respectively.  These are used to index into the corner
    // multiplier table, and were deduced by calculating out the long form
    // of each corner and finding a pattern in the signs and values.
    int i1 = (i+1) % 4;
    int i2 = (i+2) % 4;
    int i3 = (i+3) % 4;

    Float sideWidth = 0.0f;
    Color firstColor, secondColor;
    if (IsVisible(mBorderStyles[i]) && mBorderWidths[i]) {
      // draw the side since it is visible
      sideWidth = mBorderWidths[i];
      firstColor = ToDeviceColor(mBorderColors[i]);
      // if the next side is visible, use its color for corner
      secondColor =
        IsVisible(mBorderStyles[i1]) && mBorderWidths[i1] ?
          ToDeviceColor(mBorderColors[i1]) :
          firstColor;
    } else if (IsVisible(mBorderStyles[i1]) && mBorderWidths[i1]) {
      // assign next side's color to both corner sides
      firstColor = ToDeviceColor(mBorderColors[i1]);
      secondColor = firstColor;
    } else {
      // neither side is visible, so nothing to do
      continue;
    }

    Point outerCorner = mOuterRect.AtCorner(c);
    Point innerCorner = mInnerRect.AtCorner(c);

    // start and end points of border side stroke between corners
    Point sideStart =
      mOuterRect.AtCorner(prevCorner) +
        cornerMults[i2] * mBorderCornerDimensions[prevCorner];
    Point sideEnd = outerCorner + cornerMults[i] * mBorderCornerDimensions[c];
    // if the side is inverted, don't draw it
    if (-(sideEnd - sideStart).DotProduct(cornerMults[i]) <= 0) {
      sideWidth = 0.0f;
    }

    Float skirtSize = 0.0f, skirtSlope = 0.0f;
    // the sides don't match, so compute a skirt
    if (firstColor != secondColor &&
        mPresContextType != nsPresContext::eContext_Print) {
      Point cornerDir = outerCorner - innerCorner;
      ComputeCornerSkirtSize(firstColor.a, secondColor.a,
                             cornerDir.DotProduct(cornerMults[i]),
                             cornerDir.DotProduct(cornerMults[i3]),
                             skirtSize, skirtSlope);
    }

    if (!mBorderRadii[c].IsEmpty()) {
      // the corner has a border radius
      DrawBorderRadius(mDrawTarget,
                       sideStart, sideWidth,
                       c, outerCorner, innerCorner,
                       cornerMults[i], cornerMults[i3],
                       mBorderCornerDimensions[c],
                       mBorderRadii[c], innerRadii[c],
                       firstColor, secondColor, skirtSize, skirtSlope);
    } else if (!mBorderCornerDimensions[c].IsEmpty()) {
      // a corner with no border radius
      DrawCorner(mDrawTarget,
                 sideStart, sideWidth,
                 c, outerCorner, innerCorner,
                 cornerMults[i], cornerMults[i3],
                 mBorderCornerDimensions[c],
                 firstColor, secondColor, skirtSize, skirtSlope);
    } else if (sideWidth > 0 && firstColor.a > 0) {
      // if there is no corner, then stroke the border side separately
      mDrawTarget->StrokeLine(sideStart + centerAdjusts[i] * sideWidth,
                              sideEnd + centerAdjusts[i] * sideWidth,
                              ColorPattern(firstColor),
                              StrokeOptions(sideWidth));
    }
  }
}

void
nsCSSBorderRenderer::DrawRectangularCompositeColors()
{
  nsBorderColors *currentColors[4];
  memcpy(currentColors, mCompositeColors, sizeof(nsBorderColors*) * 4);
  Rect rect = mOuterRect;
  rect.Deflate(0.5);

  const twoFloats cornerAdjusts[4] = { { +0.5,  0   },
                                        {    0, +0.5 },
                                        { -0.5,  0   },
                                        {    0, -0.5 } };

  for (int i = 0; i < mBorderWidths[0]; i++) {
    NS_FOR_CSS_SIDES(side) {
      int sideNext = (side + 1) % 4;

      Point firstCorner = rect.CCWCorner(side) + cornerAdjusts[side];
      Point secondCorner = rect.CWCorner(side) - cornerAdjusts[side];

      Color currentColor = Color::FromABGR(
        currentColors[side] ? currentColors[side]->mColor
                            : mBorderColors[side]);

      mDrawTarget->StrokeLine(firstCorner, secondCorner,
                              ColorPattern(ToDeviceColor(currentColor)));

      Point cornerTopLeft = rect.CWCorner(side) - Point(0.5, 0.5);
      Color nextColor = Color::FromABGR(
        currentColors[sideNext] ? currentColors[sideNext]->mColor
                                : mBorderColors[sideNext]);

      Color cornerColor((currentColor.r + nextColor.r) / 2.f,
                        (currentColor.g + nextColor.g) / 2.f,
                        (currentColor.b + nextColor.b) / 2.f,
                        (currentColor.a + nextColor.a) / 2.f);
      mDrawTarget->FillRect(Rect(cornerTopLeft, Size(1, 1)),
                            ColorPattern(ToDeviceColor(cornerColor)));

      if (side != 0) {
        // We'll have to keep side 0 for the color averaging on side 3.
        if (currentColors[side] && currentColors[side]->mNext) {
          currentColors[side] = currentColors[side]->mNext;
        }
      }
    }
    // Now advance the color for side 0.
    if (currentColors[0] && currentColors[0]->mNext) {
      currentColors[0] = currentColors[0]->mNext;
    }
    rect.Deflate(1);
  }
}

void
nsCSSBorderRenderer::DrawBorders()
{
  bool forceSeparateCorners = false;

  // Examine the border style to figure out if we can draw it in one
  // go or not.
  bool tlBordersSame = AreBorderSideFinalStylesSame(SIDE_BIT_TOP | SIDE_BIT_LEFT);
  bool brBordersSame = AreBorderSideFinalStylesSame(SIDE_BIT_BOTTOM | SIDE_BIT_RIGHT);
  bool allBordersSame = AreBorderSideFinalStylesSame(SIDE_BITS_ALL);
  if (allBordersSame &&
      ((mCompositeColors[0] == nullptr &&
       (mBorderStyles[0] == NS_STYLE_BORDER_STYLE_NONE ||
        mBorderStyles[0] == NS_STYLE_BORDER_STYLE_HIDDEN ||
        mBorderColors[0] == NS_RGBA(0,0,0,0))) ||
       (mCompositeColors[0] &&
        (mCompositeColors[0]->mColor == NS_RGBA(0,0,0,0) &&
         !mCompositeColors[0]->mNext))))
  {
    // All borders are the same style, and the style is either none or hidden, or the color
    // is transparent.
    // This also checks if the first composite color is transparent, and there are
    // no others. It doesn't check if there are subsequent transparent ones, because
    // that would be very silly.
    return;
  }

  AutoRestoreTransform autoRestoreTransform;
  Matrix mat = mDrawTarget->GetTransform();

  // Clamp the CTM to be pixel-aligned; we do this only
  // for translation-only matrices now, but we could do it
  // if the matrix has just a scale as well.  We should not
  // do it if there's a rotation.
  if (mat.HasNonTranslation()) {
    if (!mat.HasNonAxisAlignedTransform()) {
      // Scale + transform. Avoid stroke fast-paths so that we have a chance
      // of snapping to pixel boundaries.
      mAvoidStroke = true;
    }
  } else {
    mat._31 = floor(mat._31 + 0.5);
    mat._32 = floor(mat._32 + 0.5);
    autoRestoreTransform.Init(mDrawTarget);
    mDrawTarget->SetTransform(mat);

    // round mOuterRect and mInnerRect; they're already an integer
    // number of pixels apart and should stay that way after
    // rounding. We don't do this if there's a scale in the current transform
    // since this loses information that might be relevant when we're scaling.
    mOuterRect.Round();
    mInnerRect.Round();
  }

  bool allBordersSameWidth = AllBordersSameWidth();

  if (allBordersSameWidth && mBorderWidths[0] == 0.0) {
    // Some of the allBordersSameWidth codepaths depend on the border
    // width being greater than zero.
    return;
  }

  // Initial values only used when the border colors/widths are all the same:
  ColorPattern color(ToDeviceColor(mBorderColors[NS_SIDE_TOP]));
  StrokeOptions strokeOptions(mBorderWidths[NS_SIDE_TOP]); // stroke width

  bool allBordersSolid;

  // First there's a couple of 'special cases' that have specifically optimized
  // drawing paths, when none of these can be used we move on to the generalized
  // border drawing code.
  if (allBordersSame &&
      mCompositeColors[0] == nullptr &&
      allBordersSameWidth &&
      mBorderStyles[0] == NS_STYLE_BORDER_STYLE_SOLID &&
      mNoBorderRadius &&
      !mAvoidStroke)
  {
    // Very simple case.
    Rect rect = mOuterRect;
    rect.Deflate(mBorderWidths[0] / 2.0);
    mDrawTarget->StrokeRect(rect, color, strokeOptions);
    return;
  }

  if (allBordersSame &&
      mCompositeColors[0] == nullptr &&
      allBordersSameWidth &&
      mBorderStyles[0] == NS_STYLE_BORDER_STYLE_DOTTED &&
      mBorderWidths[0] < 3 &&
      mNoBorderRadius &&
      !mAvoidStroke)
  {
    // Very simple case. We draw this rectangular dotted borner without
    // antialiasing. The dots should be pixel aligned.
    Rect rect = mOuterRect;
    rect.Deflate(mBorderWidths[0] / 2.0);
    Float dash = mBorderWidths[0];
    strokeOptions.mDashPattern = &dash;
    strokeOptions.mDashLength = 1;
    strokeOptions.mDashOffset = 0.5f;
    DrawOptions drawOptions;
    drawOptions.mAntialiasMode = AntialiasMode::NONE;
    mDrawTarget->StrokeRect(rect, color, strokeOptions, drawOptions);
    return;
  }


  if (allBordersSame &&
      mCompositeColors[0] == nullptr &&
      mBorderStyles[0] == NS_STYLE_BORDER_STYLE_SOLID &&
      !mAvoidStroke &&
      !mNoBorderRadius)
  {
    // Relatively simple case.
    gfxRect outerRect = ThebesRect(mOuterRect);
    RoundedRect borderInnerRect(outerRect, mBorderRadii);
    borderInnerRect.Deflate(mBorderWidths[NS_SIDE_TOP],
                            mBorderWidths[NS_SIDE_BOTTOM],
                            mBorderWidths[NS_SIDE_LEFT],
                            mBorderWidths[NS_SIDE_RIGHT]);

    // Instead of stroking we just use two paths: an inner and an outer.
    // This allows us to draw borders that we couldn't when stroking. For example,
    // borders with a border width >= the border radius. (i.e. when there are
    // square corners on the inside)
    //
    // Further, this approach can be more efficient because the backend
    // doesn't need to compute an offset curve to stroke the path. We know that
    // the rounded parts are elipses we can offset exactly and can just compute
    // a new cubic approximation.
    RefPtr<PathBuilder> builder = mDrawTarget->CreatePathBuilder();
    AppendRoundedRectToPath(builder, mOuterRect, mBorderRadii, true);
    AppendRoundedRectToPath(builder, ToRect(borderInnerRect.rect), borderInnerRect.corners, false);
    RefPtr<Path> path = builder->Finish();
    mDrawTarget->Fill(path, color);
    return;
  }

  bool hasCompositeColors;

  allBordersSolid = AllBordersSolid(&hasCompositeColors);
  // This leaves the border corners non-interpolated for single width borders.
  // Doing this is slightly faster and shouldn't be a problem visually.
  if (allBordersSolid &&
      allBordersSameWidth &&
      mCompositeColors[0] == nullptr &&
      mBorderWidths[0] == 1 &&
      mNoBorderRadius &&
      !mAvoidStroke)
  {
    DrawSingleWidthSolidBorder();
    return;
  }

  if (allBordersSolid && !hasCompositeColors &&
      !mAvoidStroke)
  {
    DrawNoCompositeColorSolidBorder();
    return;
  }

  if (allBordersSolid &&
      allBordersSameWidth &&
      mNoBorderRadius &&
      !mAvoidStroke)
  {
    // Easy enough to deal with.
    DrawRectangularCompositeColors();
    return;
  }

  // If we have composite colors -and- border radius,
  // then use separate corners so we get OPERATOR_ADD for the corners.
  // Otherwise, we'll get artifacts as we draw stacked 1px-wide curves.
  if (allBordersSame && mCompositeColors[0] != nullptr && !mNoBorderRadius)
    forceSeparateCorners = true;

  PrintAsString(" mOuterRect: "), PrintAsString(mOuterRect), PrintAsStringNewline();
  PrintAsString(" mInnerRect: "), PrintAsString(mInnerRect), PrintAsStringNewline();
  PrintAsFormatString(" mBorderColors: 0x%08x 0x%08x 0x%08x 0x%08x\n", mBorderColors[0], mBorderColors[1], mBorderColors[2], mBorderColors[3]);

  // if conditioning the outside rect failed, then bail -- the outside
  // rect is supposed to enclose the entire border
  {
    gfxRect outerRect = ThebesRect(mOuterRect);
    outerRect.Condition();
    if (outerRect.IsEmpty())
      return;
    mOuterRect = ToRect(outerRect);

    gfxRect innerRect = ThebesRect(mInnerRect);
    innerRect.Condition();
    mInnerRect = ToRect(innerRect);
  }

  int dashedSides = 0;

  NS_FOR_CSS_SIDES(i) {
    uint8_t style = mBorderStyles[i];
    if (style == NS_STYLE_BORDER_STYLE_DASHED ||
        style == NS_STYLE_BORDER_STYLE_DOTTED)
    {
      // pretend that all borders aren't the same; we need to draw
      // things separately for dashed/dotting
      allBordersSame = false;
      dashedSides |= (1 << i);
    }
  }

  PrintAsFormatString(" allBordersSame: %d dashedSides: 0x%02x\n", allBordersSame, dashedSides);

  if (allBordersSame && !forceSeparateCorners) {
    /* Draw everything in one go */
    DrawBorderSides(SIDE_BITS_ALL);
    PrintAsStringNewline("---------------- (1)");
  } else {
    PROFILER_LABEL("nsCSSBorderRenderer", "DrawBorders::multipass",
      js::ProfileEntry::Category::GRAPHICS);

    /* We have more than one pass to go.  Draw the corners separately from the sides. */

    /*
     * If we have a 1px-wide border, the corners are going to be
     * negligible, so don't bother doing anything fancy.  Just extend
     * the top and bottom borders to the right 1px and the left border
     * to the bottom 1px.  We do this by twiddling the corner dimensions,
     * which causes the right to happen later on.  Only do this if we have
     * a 1.0 unit border all around and no border radius.
     */

    NS_FOR_CSS_CORNERS(corner) {
      const mozilla::css::Side sides[2] = { mozilla::css::Side(corner), PREV_SIDE(corner) };

      if (!IsZeroSize(mBorderRadii[corner]))
        continue;

      if (mBorderWidths[sides[0]] == 1.0 && mBorderWidths[sides[1]] == 1.0) {
        if (corner == NS_CORNER_TOP_LEFT || corner == NS_CORNER_TOP_RIGHT)
          mBorderCornerDimensions[corner].width = 0.0;
        else
          mBorderCornerDimensions[corner].height = 0.0;
      }
    }

    // First, the corners
    NS_FOR_CSS_CORNERS(corner) {
      // if there's no corner, don't do all this work for it
      if (IsZeroSize(mBorderCornerDimensions[corner]))
        continue;

      const int sides[2] = { corner, PREV_SIDE(corner) };
      int sideBits = (1 << sides[0]) | (1 << sides[1]);

      bool simpleCornerStyle = mCompositeColors[sides[0]] == nullptr &&
                                 mCompositeColors[sides[1]] == nullptr &&
                                 AreBorderSideFinalStylesSame(sideBits);

      // If we don't have anything complex going on in this corner,
      // then we can just fill the corner with a solid color, and avoid
      // the potentially expensive clip.
      if (simpleCornerStyle &&
          IsZeroSize(mBorderRadii[corner]) &&
          IsSolidCornerStyle(mBorderStyles[sides[0]], corner))
      {
        Color color = MakeBorderColor(mBorderColors[sides[0]],
                                      mBackgroundColor,
                                      BorderColorStyleForSolidCorner(mBorderStyles[sides[0]], corner));
        mDrawTarget->FillRect(GetCornerRect(corner),
                              ColorPattern(ToDeviceColor(color)));
        continue;
      }

      // clip to the corner
      mDrawTarget->PushClipRect(GetCornerRect(corner));

      if (simpleCornerStyle) {
        // we don't need a group for this corner, the sides are the same,
        // but we weren't able to render just a solid block for the corner.
        DrawBorderSides(sideBits);
      } else {
        // Sides are different.  We could draw using OPERATOR_ADD to
        // get correct color blending behaviour at the seam.  We'd need
        // to do it in an offscreen surface to ensure that we're
        // always compositing on transparent black.  If the colors
        // don't have transparency and the current destination surface
        // has an alpha channel, we could just clear the region and
        // avoid the temporary, but that situation doesn't happen all
        // that often in practice (we double buffer to no-alpha
        // surfaces). We choose just to seam though, as the performance
        // advantages outway the modest easthetic improvement.

        for (int cornerSide = 0; cornerSide < 2; cornerSide++) {
          mozilla::css::Side side = mozilla::css::Side(sides[cornerSide]);
          uint8_t style = mBorderStyles[side];

          PrintAsFormatString("corner: %d cornerSide: %d side: %d style: %d\n", corner, cornerSide, side, style);

          RefPtr<Path> path = GetSideClipSubPath(side);
          mDrawTarget->PushClip(path);

          DrawBorderSides(1 << side);

          mDrawTarget->PopClip();
        }
      }

      mDrawTarget->PopClip();

      PrintAsStringNewline();
    }

    // in the case of a single-unit border, we already munged the
    // corners up above; so we can just draw the top left and bottom
    // right sides separately, if they're the same.
    //
    // We need to check for mNoBorderRadius, because when there is
    // one, FillSolidBorder always draws the full rounded rectangle
    // and expects there to be a clip in place.
    int alreadyDrawnSides = 0;
    if (mOneUnitBorder &&
        mNoBorderRadius &&
        (dashedSides & (SIDE_BIT_TOP | SIDE_BIT_LEFT)) == 0)
    {
      if (tlBordersSame) {
        DrawBorderSides(SIDE_BIT_TOP | SIDE_BIT_LEFT);
        alreadyDrawnSides |= (SIDE_BIT_TOP | SIDE_BIT_LEFT);
      }

      if (brBordersSame && (dashedSides & (SIDE_BIT_BOTTOM | SIDE_BIT_RIGHT)) == 0) {
        DrawBorderSides(SIDE_BIT_BOTTOM | SIDE_BIT_RIGHT);
        alreadyDrawnSides |= (SIDE_BIT_BOTTOM | SIDE_BIT_RIGHT);
      }
    }

    // We're done with the corners, now draw the sides.
    NS_FOR_CSS_SIDES (side) {
      // if we drew it above, skip it
      if (alreadyDrawnSides & (1 << side))
        continue;

      // If there's no border on this side, skip it
      if (mBorderWidths[side] == 0.0 ||
          mBorderStyles[side] == NS_STYLE_BORDER_STYLE_HIDDEN ||
          mBorderStyles[side] == NS_STYLE_BORDER_STYLE_NONE)
        continue;


      if (dashedSides & (1 << side)) {
        // Dashed sides will always draw just the part ignoring the
        // corners for the side, so no need to clip.
        DrawDashedSide (side);

        PrintAsStringNewline("---------------- (d)");
        continue;
      }

      // Undashed sides will currently draw the entire side,
      // including parts that would normally be covered by a corner,
      // so we need to clip.
      //
      // XXX Optimization -- it would be good to make this work like
      // DrawDashedSide, and have a DrawOneSide function that just
      // draws one side and not the corners, because then we can
      // avoid the potentially expensive clip.
      mDrawTarget->PushClipRect(GetSideClipWithoutCornersRect(side));

      DrawBorderSides(1 << side);

      mDrawTarget->PopClip();

      PrintAsStringNewline("---------------- (*)");
    }
  }
}
