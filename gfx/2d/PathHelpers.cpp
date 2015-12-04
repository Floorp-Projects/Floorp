/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PathHelpers.h"

namespace mozilla {
namespace gfx {

UserDataKey sDisablePixelSnapping;

void
AppendRectToPath(PathBuilder* aPathBuilder,
                 const Rect& aRect,
                 bool aDrawClockwise)
{
  if (aDrawClockwise) {
    aPathBuilder->MoveTo(aRect.TopLeft());
    aPathBuilder->LineTo(aRect.TopRight());
    aPathBuilder->LineTo(aRect.BottomRight());
    aPathBuilder->LineTo(aRect.BottomLeft());
  } else {
    aPathBuilder->MoveTo(aRect.TopRight());
    aPathBuilder->LineTo(aRect.TopLeft());
    aPathBuilder->LineTo(aRect.BottomLeft());
    aPathBuilder->LineTo(aRect.BottomRight());
  }
  aPathBuilder->Close();
}

void
AppendRoundedRectToPath(PathBuilder* aPathBuilder,
                        const Rect& aRect,
                        const RectCornerRadii& aRadii,
                        bool aDrawClockwise)
{
  // For CW drawing, this looks like:
  //
  //  ...******0**      1    C
  //              ****
  //                  ***    2
  //                     **
  //                       *
  //                        *
  //                         3
  //                         *
  //                         *
  //
  // Where 0, 1, 2, 3 are the control points of the Bezier curve for
  // the corner, and C is the actual corner point.
  //
  // At the start of the loop, the current point is assumed to be
  // the point adjacent to the top left corner on the top
  // horizontal.  Note that corner indices start at the top left and
  // continue clockwise, whereas in our loop i = 0 refers to the top
  // right corner.
  //
  // When going CCW, the control points are swapped, and the first
  // corner that's drawn is the top left (along with the top segment).
  //
  // There is considerable latitude in how one chooses the four
  // control points for a Bezier curve approximation to an ellipse.
  // For the overall path to be continuous and show no corner at the
  // endpoints of the arc, points 0 and 3 must be at the ends of the
  // straight segments of the rectangle; points 0, 1, and C must be
  // collinear; and points 3, 2, and C must also be collinear.  This
  // leaves only two free parameters: the ratio of the line segments
  // 01 and 0C, and the ratio of the line segments 32 and 3C.  See
  // the following papers for extensive discussion of how to choose
  // these ratios:
  //
  //   Dokken, Tor, et al. "Good approximation of circles by
  //      curvature-continuous Bezier curves."  Computer-Aided
  //      Geometric Design 7(1990) 33--41.
  //   Goldapp, Michael. "Approximation of circular arcs by cubic
  //      polynomials." Computer-Aided Geometric Design 8(1991) 227--238.
  //   Maisonobe, Luc. "Drawing an elliptical arc using polylines,
  //      quadratic, or cubic Bezier curves."
  //      http://www.spaceroots.org/documents/ellipse/elliptical-arc.pdf
  //
  // We follow the approach in section 2 of Goldapp (least-error,
  // Hermite-type approximation) and make both ratios equal to
  //
  //          2   2 + n - sqrt(2n + 28)
  //  alpha = - * ---------------------
  //          3           n - 4
  //
  // where n = 3( cbrt(sqrt(2)+1) - cbrt(sqrt(2)-1) ).
  //
  // This is the result of Goldapp's equation (10b) when the angle
  // swept out by the arc is pi/2, and the parameter "a-bar" is the
  // expression given immediately below equation (21).
  //
  // Using this value, the maximum radial error for a circle, as a
  // fraction of the radius, is on the order of 0.2 x 10^-3.
  // Neither Dokken nor Goldapp discusses error for a general
  // ellipse; Maisonobe does, but his choice of control points
  // follows different constraints, and Goldapp's expression for
  // 'alpha' gives much smaller radial error, even for very flat
  // ellipses, than Maisonobe's equivalent.
  //
  // For the various corners and for each axis, the sign of this
  // constant changes, or it might be 0 -- it's multiplied by the
  // appropriate multiplier from the list before using.

  const Float alpha = Float(0.55191497064665766025);

  typedef struct { Float a, b; } twoFloats;

  twoFloats cwCornerMults[4] = { { -1,  0 },    // cc == clockwise
                                 {  0, -1 },
                                 { +1,  0 },
                                 {  0, +1 } };
  twoFloats ccwCornerMults[4] = { { +1,  0 },   // ccw == counter-clockwise
                                  {  0, -1 },
                                  { -1,  0 },
                                  {  0, +1 } };

  twoFloats *cornerMults = aDrawClockwise ? cwCornerMults : ccwCornerMults;

  Point cornerCoords[] = { aRect.TopLeft(), aRect.TopRight(),
                           aRect.BottomRight(), aRect.BottomLeft() };

  Point pc, p0, p1, p2, p3;

  if (aDrawClockwise) {
    aPathBuilder->MoveTo(Point(aRect.X() + aRadii[RectCorner::TopLeft].width,
                               aRect.Y()));
  } else {
    aPathBuilder->MoveTo(Point(aRect.X() + aRect.Width() - aRadii[RectCorner::TopRight].width,
                               aRect.Y()));
  }

  for (int i = 0; i < 4; ++i) {
    // the corner index -- either 1 2 3 0 (cw) or 0 3 2 1 (ccw)
    int c = aDrawClockwise ? ((i+1) % 4) : ((4-i) % 4);

    // i+2 and i+3 respectively.  These are used to index into the corner
    // multiplier table, and were deduced by calculating out the long form
    // of each corner and finding a pattern in the signs and values.
    int i2 = (i+2) % 4;
    int i3 = (i+3) % 4;

    pc = cornerCoords[c];

    if (aRadii[c].width > 0.0 && aRadii[c].height > 0.0) {
      p0.x = pc.x + cornerMults[i].a * aRadii[c].width;
      p0.y = pc.y + cornerMults[i].b * aRadii[c].height;

      p3.x = pc.x + cornerMults[i3].a * aRadii[c].width;
      p3.y = pc.y + cornerMults[i3].b * aRadii[c].height;

      p1.x = p0.x + alpha * cornerMults[i2].a * aRadii[c].width;
      p1.y = p0.y + alpha * cornerMults[i2].b * aRadii[c].height;

      p2.x = p3.x - alpha * cornerMults[i3].a * aRadii[c].width;
      p2.y = p3.y - alpha * cornerMults[i3].b * aRadii[c].height;

      aPathBuilder->LineTo(p0);
      aPathBuilder->BezierTo(p1, p2, p3);
    } else {
      aPathBuilder->LineTo(pc);
    }
  }

  aPathBuilder->Close();
}

void
AppendEllipseToPath(PathBuilder* aPathBuilder,
                    const Point& aCenter,
                    const Size& aDimensions)
{
  Size halfDim = aDimensions / 2.f;
  Rect rect(aCenter - Point(halfDim.width, halfDim.height), aDimensions);
  RectCornerRadii radii(halfDim.width, halfDim.height);

  AppendRoundedRectToPath(aPathBuilder, rect, radii);
}

bool
SnapLineToDevicePixelsForStroking(Point& aP1, Point& aP2,
                                  const DrawTarget& aDrawTarget,
                                  Float aLineWidth)
{
  Matrix mat = aDrawTarget.GetTransform();
  if (mat.HasNonTranslation()) {
    return false;
  }
  if (aP1.x != aP2.x && aP1.y != aP2.y) {
    return false; // not a horizontal or vertical line
  }
  Point p1 = aP1 + mat.GetTranslation(); // into device space
  Point p2 = aP2 + mat.GetTranslation();
  p1.Round();
  p2.Round();
  p1 -= mat.GetTranslation(); // back into user space
  p2 -= mat.GetTranslation();

  aP1 = p1;
  aP2 = p2;

  bool lineWidthIsOdd = (int(aLineWidth) % 2) == 1;
  if (lineWidthIsOdd) {
    if (aP1.x == aP2.x) {
      // snap vertical line, adding 0.5 to align it to be mid-pixel:
      aP1 += Point(0.5, 0);
      aP2 += Point(0.5, 0);
    } else {
      // snap horizontal line, adding 0.5 to align it to be mid-pixel:
      aP1 += Point(0, 0.5);
      aP2 += Point(0, 0.5);
    }
  }
  return true;
}

void
StrokeSnappedEdgesOfRect(const Rect& aRect, DrawTarget& aDrawTarget,
                        const ColorPattern& aColor,
                        const StrokeOptions& aStrokeOptions)
{
  if (aRect.IsEmpty()) {
    return;
  }

  Point p1 = aRect.TopLeft();
  Point p2 = aRect.BottomLeft();
  SnapLineToDevicePixelsForStroking(p1, p2, aDrawTarget,
                                    aStrokeOptions.mLineWidth);
  aDrawTarget.StrokeLine(p1, p2, aColor, aStrokeOptions);

  p1 = aRect.BottomLeft();
  p2 = aRect.BottomRight();
  SnapLineToDevicePixelsForStroking(p1, p2, aDrawTarget,
                                    aStrokeOptions.mLineWidth);
  aDrawTarget.StrokeLine(p1, p2, aColor, aStrokeOptions);

  p1 = aRect.TopLeft();
  p2 = aRect.TopRight();
  SnapLineToDevicePixelsForStroking(p1, p2, aDrawTarget,
                                    aStrokeOptions.mLineWidth);
  aDrawTarget.StrokeLine(p1, p2, aColor, aStrokeOptions);

  p1 = aRect.TopRight();
  p2 = aRect.BottomRight();
  SnapLineToDevicePixelsForStroking(p1, p2, aDrawTarget,
                                    aStrokeOptions.mLineWidth);
  aDrawTarget.StrokeLine(p1, p2, aColor, aStrokeOptions);
}

// The logic for this comes from _cairo_stroke_style_max_distance_from_path
Margin
MaxStrokeExtents(const StrokeOptions& aStrokeOptions,
                 const Matrix& aTransform)
{
  double styleExpansionFactor = 0.5f;

  if (aStrokeOptions.mLineCap == CapStyle::SQUARE) {
    styleExpansionFactor = M_SQRT1_2;
  }

  if (aStrokeOptions.mLineJoin == JoinStyle::MITER &&
      styleExpansionFactor < M_SQRT2 * aStrokeOptions.mMiterLimit) {
    styleExpansionFactor = M_SQRT2 * aStrokeOptions.mMiterLimit;
  }

  styleExpansionFactor *= aStrokeOptions.mLineWidth;

  double dx = styleExpansionFactor * hypot(aTransform._11, aTransform._21);
  double dy = styleExpansionFactor * hypot(aTransform._22, aTransform._12);
  return Margin(dy, dx, dy, dx);
}

} // namespace gfx
} // namespace mozilla
