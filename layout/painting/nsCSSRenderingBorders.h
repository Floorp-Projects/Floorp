/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_CSS_RENDERING_BORDERS_H
#define NS_CSS_RENDERING_BORDERS_H

#include "gfxRect.h"
#include "mozilla/Attributes.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/BezierUtils.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/RefPtr.h"
#include "nsColor.h"
#include "nsCOMPtr.h"
#include "nsStyleConsts.h"
#include "nsStyleStruct.h"
#include "nsPresContext.h"

struct nsBorderColors;
class nsDisplayBorder;

namespace mozilla {
namespace gfx {
class GradientStops;
} // namespace gfx
} // namespace mozilla

// define this to enable a bunch of debug dump info
#undef DEBUG_NEW_BORDERS

/*
 * Helper class that handles border rendering.
 *
 * aDrawTarget -- the DrawTarget to which the border should be rendered
 * outsideRect -- the rectangle on the outer edge of the border
 *
 * For any parameter where an array of side values is passed in,
 * they are in top, right, bottom, left order.
 *
 * borderStyles -- one border style enum per side
 * borderWidths -- one border width per side
 * borderRadii -- a RectCornerRadii struct describing the w/h for each rounded corner.
 *    If the corner doesn't have a border radius, 0,0 should be given for it.
 * borderColors -- one nscolor per side
 * compositeColors -- a pointer to an array of composite color structs, or
 *    nullptr if none.
 *
 * skipSides -- a bit mask specifying which sides, if any, to skip
 * backgroundColor -- the background color of the element.
 *    Used in calculating colors for 2-tone borders, such as inset and outset
 * gapRect - a rectangle that should be clipped out to leave a gap in a border,
 *    or nullptr if none.
 */

typedef enum {
  BorderColorStyleNone,
  BorderColorStyleSolid,
  BorderColorStyleLight,
  BorderColorStyleDark
} BorderColorStyle;

class nsIDocument;
class nsPresContext;

class nsCSSBorderRenderer final
{
  typedef mozilla::gfx::Bezier Bezier;
  typedef mozilla::gfx::ColorPattern ColorPattern;
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::Float Float;
  typedef mozilla::gfx::Path Path;
  typedef mozilla::gfx::Point Point;
  typedef mozilla::gfx::Rect Rect;
  typedef mozilla::gfx::RectCornerRadii RectCornerRadii;
  typedef mozilla::gfx::StrokeOptions StrokeOptions;

  friend class nsDisplayBorder;

public:

  nsCSSBorderRenderer(nsPresContext* aPresContext,
                      const nsIDocument* aDocument,
                      DrawTarget* aDrawTarget,
                      const Rect& aDirtyRect,
                      Rect& aOuterRect,
                      const uint8_t* aBorderStyles,
                      const Float* aBorderWidths,
                      RectCornerRadii& aBorderRadii,
                      const nscolor* aBorderColors,
                      nsBorderColors* const* aCompositeColors,
                      nscolor aBackgroundColor);

  // draw the entire border
  void DrawBorders();

  // utility function used for background painting as well as borders
  static void ComputeInnerRadii(const RectCornerRadii& aRadii,
                                const Float* aBorderSizes,
                                RectCornerRadii* aInnerRadiiRet);

  // Given aRadii as the border radii for a rectangle, compute the
  // appropriate radii for another rectangle *outside* that rectangle
  // by increasing the radii, except keeping sharp corners sharp.
  // Used for spread box-shadows
  static void ComputeOuterRadii(const RectCornerRadii& aRadii,
                                const Float* aBorderSizes,
                                RectCornerRadii* aOuterRadiiRet);

  static bool AllCornersZeroSize(const RectCornerRadii& corners);

private:

  RectCornerRadii mBorderCornerDimensions;

  // Target document to report warning
  nsPresContext* mPresContext;
  const nsIDocument* mDocument;

  // destination DrawTarget and dirty rect
  DrawTarget* mDrawTarget;
  const Rect mDirtyRect;

  // the rectangle of the outside and the inside of the border
  Rect mOuterRect;
  Rect mInnerRect;

  // the style and size of the border
  uint8_t mBorderStyles[4];
  Float mBorderWidths[4];
  RectCornerRadii mBorderRadii;

  // colors
  nscolor mBorderColors[4];
  nsBorderColors* mCompositeColors[4];

  // the background color
  nscolor mBackgroundColor;

  // calculated values
  bool mOneUnitBorder;
  bool mNoBorderRadius;
  bool mAvoidStroke;

  // For all the sides in the bitmask, would they be rendered
  // in an identical color and style?
  bool AreBorderSideFinalStylesSame(uint8_t aSides);

  // For the given style, is the given corner a solid color?
  bool IsSolidCornerStyle(uint8_t aStyle, mozilla::css::Corner aCorner);

  // For the given corner, is the given corner mergeable into one dot?
  bool IsCornerMergeable(mozilla::css::Corner aCorner);

  // For the given solid corner, what color style should be used?
  BorderColorStyle BorderColorStyleForSolidCorner(uint8_t aStyle, mozilla::css::Corner aCorner);

  //
  // Path generation functions
  //

  // Get the Rect for drawing the given corner
  Rect GetCornerRect(mozilla::css::Corner aCorner);
  // add the path for drawing the given side without any adjacent corners to the context
  Rect GetSideClipWithoutCornersRect(mozilla::css::Side aSide);

  // Create a clip path for the wedge that this side of
  // the border should take up.  This is only called
  // when we're drawing separate border sides, so we know
  // that ADD compositing is taking place.
  //
  // This code needs to make sure that the individual pieces
  // don't ever (mathematically) overlap; the pixel overlap
  // is taken care of by the ADD compositing.
  already_AddRefed<Path> GetSideClipSubPath(mozilla::css::Side aSide);

  // Return start or end point for dashed/dotted side
  Point GetStraightBorderPoint(mozilla::css::Side aSide,
                               mozilla::css::Corner aCorner,
                               bool* aIsUnfilled);

  // Return bezier control points for the outer and the inner curve for given
  // corner
  void GetOuterAndInnerBezier(Bezier* aOuterBezier,
                              Bezier* aInnerBezier,
                              mozilla::css::Corner aCorner);

  // Given a set of sides to fill and a color, do so in the fastest way.
  //
  // Stroke tends to be faster for smaller borders because it doesn't go
  // through the tessellator, which has initialization overhead.  If
  // we're rendering all sides, we can use stroke at any thickness; we
  // also do TL/BR pairs at 1px thickness using stroke.
  //
  // If we can't stroke, then if it's a TL/BR pair, we use the specific
  // TL/BR paths.  Otherwise, we do the full path and fill.
  //
  // Calling code is expected to only set up a clip as necessary; no
  // clip is needed if we can render the entire border in 1 or 2 passes.
  void FillSolidBorder(const Rect& aOuterRect,
                       const Rect& aInnerRect,
                       const RectCornerRadii& aBorderRadii,
                       const Float* aBorderSizes,
                       int aSides,
                       const ColorPattern& aColor);

  //
  // core rendering
  //

  // draw the border for the given sides, using the style of the first side
  // present in the bitmask
  void DrawBorderSides (int aSides);

  // function used by the above to handle -moz-border-colors
  void DrawBorderSidesCompositeColors(int aSides, const nsBorderColors *compositeColors);

  // Setup the stroke options for the given dashed/dotted side
  void SetupDashedOptions(StrokeOptions* aStrokeOptions,
                          Float aDash[2], mozilla::css::Side aSide,
                          Float aBorderLength, bool isCorner);

  // Draw the given dashed/dotte side
  void DrawDashedOrDottedSide(mozilla::css::Side aSide);

  // Draw the given dotted side, each dot separately
  void DrawDottedSideSlow(mozilla::css::Side aSide);

  // Draw the given dashed/dotted corner
  void DrawDashedOrDottedCorner(mozilla::css::Side aSide,
                                mozilla::css::Corner aCorner);

  // Draw the given dotted corner, each segment separately
  void DrawDottedCornerSlow(mozilla::css::Side aSide,
                            mozilla::css::Corner aCorner);

  // Draw the given dashed corner, each dot separately
  void DrawDashedCornerSlow(mozilla::css::Side aSide,
                            mozilla::css::Corner aCorner);

  // Draw the given dashed/dotted corner with solid style
  void DrawFallbackSolidCorner(mozilla::css::Side aSide,
                               mozilla::css::Corner aCorner);

  // Analyze if all border sides have the same width.
  bool AllBordersSameWidth();

  // Analyze if all borders are 'solid' this also considers hidden or 'none'
  // borders because they can be considered 'solid' borders of 0 width and
  // with no color effect.
  bool AllBordersSolid(bool *aHasCompositeColors);

  // Draw a solid color border that is uniformly the same width.
  void DrawSingleWidthSolidBorder();

  // Draw any border which is solid on all sides and does not use
  // CompositeColors.
  void DrawNoCompositeColorSolidBorder();

  // Draw a solid border that has no border radius (i.e. is rectangular) and
  // uses CompositeColors.
  void DrawRectangularCompositeColors();
};

namespace mozilla {
#ifdef DEBUG_NEW_BORDERS
#include <stdarg.h>

static inline void PrintAsString(const mozilla::gfx::Point& p) {
  fprintf (stderr, "[%f,%f]", p.x, p.y);
}

static inline void PrintAsString(const mozilla::gfx::Size& s) {
  fprintf (stderr, "[%f %f]", s.width, s.height);
}

static inline void PrintAsString(const mozilla::gfx::Rect& r) {
  fprintf (stderr, "[%f %f %f %f]", r.X(), r.Y(), r.Width(), r.Height());
}

static inline void PrintAsString(const mozilla::gfx::Float f) {
  fprintf (stderr, "%f", f);
}

static inline void PrintAsString(const char *s) {
  fprintf (stderr, "%s", s);
}

static inline void PrintAsStringNewline(const char *s = nullptr) {
  if (s)
    fprintf (stderr, "%s", s);
  fprintf (stderr, "\n");
  fflush (stderr);
}

static inline void PrintAsFormatString(const char *fmt, ...) {
  va_list vl;
  va_start(vl, fmt);
  vfprintf (stderr, fmt, vl);
  va_end(vl);
}

#else
static inline void PrintAsString(const mozilla::gfx::Point& p) {}
static inline void PrintAsString(const mozilla::gfx::Size& s) {}
static inline void PrintAsString(const mozilla::gfx::Rect& r) {}
static inline void PrintAsString(const mozilla::gfx::Float f) {}
static inline void PrintAsString(const char *s) {}
static inline void PrintAsStringNewline(const char *s = nullptr) {}
static inline void PrintAsFormatString(const char *fmt, ...) {}
#endif

} // namespace mozilla

#endif /* NS_CSS_RENDERING_BORDERS_H */
