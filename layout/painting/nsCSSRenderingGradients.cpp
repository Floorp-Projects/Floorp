/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* utility functions for drawing borders and backgrounds */

#include "nsCSSRenderingGradients.h"

#include "gfx2DGlue.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/MathAlgorithms.h"

#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsStyleContext.h"
#include "nsCSSColorUtils.h"
#include "gfxContext.h"
#include "nsRenderingContext.h"
#include "nsStyleStructInlines.h"
#include "nsCSSProps.h"
#include "mozilla/Telemetry.h"
#include "gfxUtils.h"
#include "gfxGradientCache.h"

#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "Units.h"

using namespace mozilla;
using namespace mozilla::gfx;

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

namespace mozilla {

nsCSSGradientRenderer
nsCSSGradientRenderer::Create(nsPresContext* aPresContext,
                             nsStyleGradient* aGradient,
                             const nsSize& aIntrinsicSize)
{
  nscoord appUnitsPerDevPixel = aPresContext->AppUnitsPerDevPixel();
  gfxSize srcSize = gfxSize(gfxFloat(aIntrinsicSize.width)/appUnitsPerDevPixel,
                            gfxFloat(aIntrinsicSize.height)/appUnitsPerDevPixel);

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
      double previousPosition = firstUnsetPosition > 0
        ? stops[firstUnsetPosition - 1].mPosition
        : stops[i - 1].mPosition;
      position = std::max(position, previousPosition);
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

  ResolveMidpoints(stops);

  nsCSSGradientRenderer renderer;
  renderer.mPresContext = aPresContext;
  renderer.mGradient = aGradient;
  renderer.mStops = std::move(stops);
  renderer.mLineStart = lineStart;
  renderer.mLineEnd = lineEnd;
  renderer.mRadiusX = radiusX;
  renderer.mRadiusY = radiusY;
  return renderer;
}

void
nsCSSGradientRenderer::Paint(gfxContext& aContext,
                             const nsRect& aDest,
                             const nsRect& aFillArea,
                             const nsSize& aRepeatSize,
                             const CSSIntRect& aSrc,
                             const nsRect& aDirtyRect,
                             float aOpacity)
{
  PROFILER_LABEL("nsCSSRendering", "PaintGradient",
    js::ProfileEntry::Category::GRAPHICS);
  Telemetry::AutoTimer<Telemetry::GRADIENT_DURATION, Telemetry::Microsecond> gradientTimer;

  if (aDest.IsEmpty() || aFillArea.IsEmpty()) {
    return;
  }

  nscoord appUnitsPerDevPixel = mPresContext->AppUnitsPerDevPixel();

  gfxFloat lineLength = NS_hypot(mLineEnd.x - mLineStart.x,
                                 mLineEnd.y - mLineStart.y);
  bool cellContainsFill = aDest.Contains(aFillArea);

  // If a non-repeating linear gradient is axis-aligned and there are no gaps
  // between tiles, we can optimise away most of the work by converting to a
  // repeating linear gradient and filling the whole destination rect at once.
  bool forceRepeatToCoverTiles =
    mGradient->mShape == NS_STYLE_GRADIENT_SHAPE_LINEAR &&
    (mLineStart.x == mLineEnd.x) != (mLineStart.y == mLineEnd.y) &&
    aRepeatSize.width == aDest.width && aRepeatSize.height == aDest.height &&
    !mGradient->mRepeating && !aSrc.IsEmpty() && !cellContainsFill;

  gfxMatrix matrix;
  if (forceRepeatToCoverTiles) {
    // Length of the source rectangle along the gradient axis.
    double rectLen;
    // The position of the start of the rectangle along the gradient.
    double offset;

    // The gradient line is "backwards". Flip the line upside down to make
    // things easier, and then rotate the matrix to turn everything back the
    // right way up.
    if (mLineStart.x > mLineEnd.x || mLineStart.y > mLineEnd.y) {
      std::swap(mLineStart, mLineEnd);
      matrix.Scale(-1, -1);
    }

    // Fit the gradient line exactly into the source rect.
    // aSrc is relative to aIntrinsincSize.
    // srcRectDev will be relative to srcSize, so in the same coordinate space
    // as lineStart / lineEnd.
    gfxRect srcRectDev = nsLayoutUtils::RectToGfxRect(
      CSSPixel::ToAppUnits(aSrc), appUnitsPerDevPixel);
    if (mLineStart.x != mLineEnd.x) {
      rectLen = srcRectDev.width;
      offset = (srcRectDev.x - mLineStart.x) / lineLength;
      mLineStart.x = srcRectDev.x;
      mLineEnd.x = srcRectDev.XMost();
    } else {
      rectLen = srcRectDev.height;
      offset = (srcRectDev.y - mLineStart.y) / lineLength;
      mLineStart.y = srcRectDev.y;
      mLineEnd.y = srcRectDev.YMost();
    }

    // Adjust gradient stop positions for the new gradient line.
    double scale = lineLength / rectLen;
    for (size_t i = 0; i < mStops.Length(); i++) {
      mStops[i].mPosition = (mStops[i].mPosition - offset) * fabs(scale);
    }

    // Clamp or extrapolate gradient stops to exactly [0, 1].
    ClampColorStops(mStops);

    lineLength = rectLen;
  }

  // Eliminate negative-position stops if the gradient is radial.
  double firstStop = mStops[0].mPosition;
  if (mGradient->mShape != NS_STYLE_GRADIENT_SHAPE_LINEAR && firstStop < 0.0) {
    if (mGradient->mRepeating) {
      // Choose an instance of the repeated pattern that gives us all positive
      // stop-offsets.
      double lastStop = mStops[mStops.Length() - 1].mPosition;
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
        for (uint32_t i = 0; i < mStops.Length(); i++) {
          mStops[i].mPosition += offset;
        }
      }
    } else {
      // Move negative-position stops to position 0.0. We may also need
      // to set the color of the stop to the color the gradient should have
      // at the center of the ellipse.
      for (uint32_t i = 0; i < mStops.Length(); i++) {
        double pos = mStops[i].mPosition;
        if (pos < 0.0) {
          mStops[i].mPosition = 0.0;
          // If this is the last stop, we don't need to adjust the color,
          // it will fill the entire area.
          if (i < mStops.Length() - 1) {
            double nextPos = mStops[i + 1].mPosition;
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
              mStops[i].mColor =
                InterpolateColor(mStops[i].mColor, mStops[i + 1].mColor, frac);
            }
          }
        }
      }
    }
    firstStop = mStops[0].mPosition;
    MOZ_ASSERT(firstStop >= 0.0, "Failed to fix stop offsets");
  }

  if (mGradient->mShape != NS_STYLE_GRADIENT_SHAPE_LINEAR && !mGradient->mRepeating) {
    // Direct2D can only handle a particular class of radial gradients because
    // of the way the it specifies gradients. Setting firstStop to 0, when we
    // can, will help us stay on the fast path. Currently we don't do this
    // for repeating gradients but we could by adjusting the stop collection
    // to start at 0
    firstStop = 0;
  }

  double lastStop = mStops[mStops.Length() - 1].mPosition;
  // Cairo gradients must have stop positions in the range [0, 1]. So,
  // stop positions will be normalized below by subtracting firstStop and then
  // multiplying by stopScale.
  double stopScale;
  double stopOrigin = firstStop;
  double stopEnd = lastStop;
  double stopDelta = lastStop - firstStop;
  bool zeroRadius = mGradient->mShape != NS_STYLE_GRADIENT_SHAPE_LINEAR &&
                      (mRadiusX < 1e-6 || mRadiusY < 1e-6);
  if (stopDelta < 1e-6 || lineLength < 1e-6 || zeroRadius) {
    // Stops are all at the same place. Map all stops to 0.0.
    // For repeating radial gradients, or for any radial gradients with
    // a zero radius, we need to fill with the last stop color, so just set
    // both radii to 0.
    if (mGradient->mRepeating || zeroRadius) {
      mRadiusX = mRadiusY = 0.0;
    }
    stopDelta = 0.0;
    lastStop = firstStop;
  }

  // Don't normalize non-repeating or degenerate gradients below 0..1
  // This keeps the gradient line as large as the box and doesn't
  // lets us avoiding having to get padding correct for stops
  // at 0 and 1
  if (!mGradient->mRepeating || stopDelta == 0.0) {
    stopOrigin = std::min(stopOrigin, 0.0);
    stopEnd = std::max(stopEnd, 1.0);
  }
  stopScale = 1.0/(stopEnd - stopOrigin);

  // Create the gradient pattern.
  RefPtr<gfxPattern> gradientPattern;
  gfxPoint gradientStart;
  gfxPoint gradientEnd;
  if (mGradient->mShape == NS_STYLE_GRADIENT_SHAPE_LINEAR) {
    // Compute the actual gradient line ends we need to pass to cairo after
    // stops have been normalized.
    gradientStart = mLineStart + (mLineEnd - mLineStart)*stopOrigin;
    gradientEnd = mLineStart + (mLineEnd - mLineStart)*stopEnd;

    if (stopDelta == 0.0) {
      // Stops are all at the same place. For repeating gradients, this will
      // just paint the last stop color. We don't need to do anything.
      // For non-repeating gradients, this should render as two colors, one
      // on each "side" of the gradient line segment, which is a point. All
      // our stops will be at 0.0; we just need to set the direction vector
      // correctly.
      gradientEnd = gradientStart + (mLineEnd - mLineStart);
    }

    gradientPattern = new gfxPattern(gradientStart.x, gradientStart.y,
                                      gradientEnd.x, gradientEnd.y);
  } else {
    NS_ASSERTION(firstStop >= 0.0,
                  "Negative stops not allowed for radial gradients");

    // To form an ellipse, we'll stretch a circle vertically, if necessary.
    // So our radii are based on radiusX.
    double innerRadius = mRadiusX*stopOrigin;
    double outerRadius = mRadiusX*stopEnd;
    if (stopDelta == 0.0) {
      // Stops are all at the same place.  See above (except we now have
      // the inside vs. outside of an ellipse).
      outerRadius = innerRadius + 1;
    }
    gradientPattern = new gfxPattern(mLineStart.x, mLineStart.y, innerRadius,
                                     mLineStart.x, mLineStart.y, outerRadius);
    if (mRadiusX != mRadiusY) {
      // Stretch the circles into ellipses vertically by setting a transform
      // in the pattern.
      // Recall that this is the transform from user space to pattern space.
      // So to stretch the ellipse by factor of P vertically, we scale
      // user coordinates by 1/P.
      matrix.Translate(mLineStart);
      matrix.Scale(1.0, mRadiusX/mRadiusY);
      matrix.Translate(-mLineStart);
    }
  }
  // Use a pattern transform to take account of source and dest rects
  matrix.Translate(gfxPoint(mPresContext->CSSPixelsToDevPixels(aSrc.x),
                            mPresContext->CSSPixelsToDevPixels(aSrc.y)));
  matrix.Scale(gfxFloat(mPresContext->CSSPixelsToAppUnits(aSrc.width))/aDest.width,
               gfxFloat(mPresContext->CSSPixelsToAppUnits(aSrc.height))/aDest.height);
  gradientPattern->SetMatrix(matrix);

  if (stopDelta == 0.0) {
    // Non-repeating gradient with all stops in same place -> just add
    // first stop and last stop, both at position 0.
    // Repeating gradient with all stops in the same place, or radial
    // gradient with radius of 0 -> just paint the last stop color.
    // We use firstStop offset to keep |stops| with same units (will later normalize to 0).
    Color firstColor(mStops[0].mColor);
    Color lastColor(mStops.LastElement().mColor);
    mStops.Clear();

    if (!mGradient->mRepeating && !zeroRadius) {
      mStops.AppendElement(ColorStop(firstStop, false, firstColor));
    }
    mStops.AppendElement(ColorStop(firstStop, false, lastColor));
  }

  ResolvePremultipliedAlpha(mStops);

  bool isRepeat = mGradient->mRepeating || forceRepeatToCoverTiles;

  // Now set normalized color stops in pattern.
  // Offscreen gradient surface cache (not a tile):
  // On some backends (e.g. D2D), the GradientStops object holds an offscreen surface
  // which is a lookup table used to evaluate the gradient. This surface can use
  // much memory (ram and/or GPU ram) and can be expensive to create. So we cache it.
  // The cache key correlates 1:1 with the arguments for CreateGradientStops (also the implied backend type)
  // Note that GradientStop is a simple struct with a stop value (while GradientStops has the surface).
  nsTArray<gfx::GradientStop> rawStops(mStops.Length());
  rawStops.SetLength(mStops.Length());
  for(uint32_t i = 0; i < mStops.Length(); i++) {
    rawStops[i].color = mStops[i].mColor;
    rawStops[i].color.a *= aOpacity;
    rawStops[i].offset = stopScale * (mStops[i].mPosition - stopOrigin);
  }
  RefPtr<mozilla::gfx::GradientStops> gs =
    gfxGradientCache::GetOrCreateGradientStops(aContext.GetDrawTarget(),
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

  gfxMatrix ctm = aContext.CurrentMatrix();
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
          aContext.UserToDevicePixelSnapped(snappedFillRectTopLeft, true) &&
          aContext.UserToDevicePixelSnapped(snappedFillRectBottomRight, true) &&
          aContext.UserToDevicePixelSnapped(snappedFillRectTopRight, true)) {
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
        aContext.SetMatrix(transform);
      }
      aContext.NewPath();
      aContext.Rectangle(fillRect);

      gfxRect dirtyFillRect = fillRect.Intersect(dirtyAreaToFill);
      gfxRect fillRectRelativeToTile = dirtyFillRect - tileRect.TopLeft();
      Color edgeColor;
      if (mGradient->mShape == NS_STYLE_GRADIENT_SHAPE_LINEAR && !isRepeat &&
          RectIsBeyondLinearGradientEdge(fillRectRelativeToTile, matrix, mStops,
                                         gradientStart, gradientEnd, &edgeColor)) {
        edgeColor.a *= aOpacity;
        aContext.SetColor(edgeColor);
      } else {
        aContext.SetMatrix(
          aContext.CurrentMatrix().Copy().Translate(tileRect.TopLeft()));
        aContext.SetPattern(gradientPattern);
      }
      aContext.Fill();
      aContext.SetMatrix(ctm);
    }
  }
}

void
nsCSSGradientRenderer::BuildWebRenderParameters(float aOpacity,
                                                WrGradientExtendMode& aMode,
                                                nsTArray<WrGradientStop>& aStops,
                                                LayoutDevicePoint& aLineStart,
                                                LayoutDevicePoint& aLineEnd,
                                                LayoutDeviceSize& aGradientRadius)
{
  aMode = mGradient->mRepeating ? WrGradientExtendMode::Repeat : WrGradientExtendMode::Clamp;

  aStops.SetLength(mStops.Length());
  for(uint32_t i = 0; i < mStops.Length(); i++) {
    Float alpha = mStops[i].mColor.a * aOpacity;
    aStops[i].color.r = mStops[i].mColor.r * alpha;
    aStops[i].color.g = mStops[i].mColor.g * alpha;
    aStops[i].color.b = mStops[i].mColor.b * alpha;
    aStops[i].color.a = alpha;
    aStops[i].offset = mStops[i].mPosition;
  }

  aLineStart = LayoutDevicePoint(mLineStart.x, mLineStart.y);
  aLineEnd = LayoutDevicePoint(mLineEnd.x, mLineEnd.y);
  aGradientRadius = LayoutDeviceSize(mRadiusX, mRadiusY);
}

void
nsCSSGradientRenderer::BuildWebRenderDisplayItems(wr::DisplayListBuilder& aBuilder,
                                                  layers::WebRenderDisplayItemLayer* aLayer,
                                                  const nsRect& aDest,
                                                  const nsRect& aFillArea,
                                                  const nsSize& aRepeatSize,
                                                  const CSSIntRect& aSrc,
                                                  float aOpacity)
{
  if (aDest.IsEmpty() || aFillArea.IsEmpty()) {
    return;
  }

  WrGradientExtendMode extendMode;
  nsTArray<WrGradientStop> stops;
  LayoutDevicePoint lineStart;
  LayoutDevicePoint lineEnd;
  LayoutDeviceSize gradientRadius;
  BuildWebRenderParameters(aOpacity, extendMode, stops, lineStart, lineEnd, gradientRadius);

  nscoord appUnitsPerDevPixel = mPresContext->AppUnitsPerDevPixel();

  // Translate the parameters into device coordinates
  LayoutDeviceRect clipBounds = LayoutDevicePixel::FromAppUnits(aFillArea, appUnitsPerDevPixel);
  LayoutDeviceRect firstTileBounds = LayoutDevicePixel::FromAppUnits(aDest, appUnitsPerDevPixel);
  LayoutDeviceSize tileRepeat = LayoutDevicePixel::FromAppUnits(aRepeatSize, appUnitsPerDevPixel);

  // Calculate the bounds of the gradient display item, which starts at the first
  // tile and extends to the end of clip bounds
  LayoutDevicePoint tileToClip = clipBounds.BottomRight() - firstTileBounds.TopLeft();
  LayoutDeviceRect gradientBounds = LayoutDeviceRect(firstTileBounds.TopLeft(),
                                                     LayoutDeviceSize(tileToClip.x, tileToClip.y));

  // Calculate the tile spacing, which is the repeat size minus the tile size
  LayoutDeviceSize tileSpacing = tileRepeat - firstTileBounds.Size();

  // Make the rects relative to the parent stacking context
  LayerRect layerClipBounds = aLayer->RelativeToParent(clipBounds);
  LayerRect layerFirstTileBounds = aLayer->RelativeToParent(firstTileBounds);
  LayerRect layerGradientBounds = aLayer->RelativeToParent(gradientBounds);

  // srcTransform is used for scaling the gradient to match aSrc
  LayoutDeviceRect srcTransform = LayoutDeviceRect(mPresContext->CSSPixelsToAppUnits(aSrc.x),
                                                   mPresContext->CSSPixelsToAppUnits(aSrc.y),
                                                   aDest.width / ((float)mPresContext->CSSPixelsToAppUnits(aSrc.width)),
                                                   aDest.height / ((float)mPresContext->CSSPixelsToAppUnits(aSrc.height)));

  lineStart.x = (lineStart.x - srcTransform.x) * srcTransform.width;
  lineStart.y = (lineStart.y - srcTransform.y) * srcTransform.height;

  if (mGradient->mShape == NS_STYLE_GRADIENT_SHAPE_LINEAR) {
    lineEnd.x = (lineEnd.x - srcTransform.x) * srcTransform.width;
    lineEnd.y = (lineEnd.y - srcTransform.y) * srcTransform.height;

    aBuilder.PushLinearGradient(
      mozilla::wr::ToWrRect(layerGradientBounds),
      aBuilder.BuildClipRegion(mozilla::wr::ToWrRect(layerClipBounds)),
      mozilla::wr::ToWrPoint(lineStart),
      mozilla::wr::ToWrPoint(lineEnd),
      stops,
      extendMode,
      mozilla::wr::ToWrSize(layerFirstTileBounds.Size()),
      mozilla::wr::ToWrSize(tileSpacing));
  } else {
    gradientRadius.width *= srcTransform.width;
    gradientRadius.height *= srcTransform.height;

    aBuilder.PushRadialGradient(
      mozilla::wr::ToWrRect(layerGradientBounds),
      aBuilder.BuildClipRegion(mozilla::wr::ToWrRect(layerClipBounds)),
      mozilla::wr::ToWrPoint(lineStart),
      mozilla::wr::ToWrSize(gradientRadius),
      stops,
      extendMode,
      mozilla::wr::ToWrSize(layerFirstTileBounds.Size()),
      mozilla::wr::ToWrSize(tileSpacing));
  }
}

} // namespace mozilla
