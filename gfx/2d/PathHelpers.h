/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PATHHELPERS_H_
#define MOZILLA_GFX_PATHHELPERS_H_

#include "2D.h"
#include "mozilla/Constants.h"
#include "UserData.h"

namespace mozilla {
namespace gfx {

template <typename T>
void ArcToBezier(T* aSink, const Point &aOrigin, const Size &aRadius,
                 float aStartAngle, float aEndAngle, bool aAntiClockwise)
{
  Point startPoint(aOrigin.x + cosf(aStartAngle) * aRadius.width,
                   aOrigin.y + sinf(aStartAngle) * aRadius.height);

  aSink->LineTo(startPoint);

  // Clockwise we always sweep from the smaller to the larger angle, ccw
  // it's vice versa.
  if (!aAntiClockwise && (aEndAngle < aStartAngle)) {
    Float correction = Float(ceil((aStartAngle - aEndAngle) / (2.0f * M_PI)));
    aEndAngle += float(correction * 2.0f * M_PI);
  } else if (aAntiClockwise && (aStartAngle < aEndAngle)) {
    Float correction = (Float)ceil((aEndAngle - aStartAngle) / (2.0f * M_PI));
    aStartAngle += float(correction * 2.0f * M_PI);
  }

  // Sweeping more than 2 * pi is a full circle.
  if (!aAntiClockwise && (aEndAngle - aStartAngle > 2 * M_PI)) {
    aEndAngle = float(aStartAngle + 2.0f * M_PI);
  } else if (aAntiClockwise && (aStartAngle - aEndAngle > 2.0f * M_PI)) {
    aEndAngle = float(aStartAngle - 2.0f * M_PI);
  }

  // Calculate the total arc we're going to sweep.
  Float arcSweepLeft = fabs(aEndAngle - aStartAngle);

  Float sweepDirection = aAntiClockwise ? -1.0f : 1.0f;

  Float currentStartAngle = aStartAngle;

  while (arcSweepLeft > 0) {
    // We guarantee here the current point is the start point of the next
    // curve segment.
    Float currentEndAngle;

    if (arcSweepLeft > M_PI / 2.0f) {
      currentEndAngle = Float(currentStartAngle + M_PI / 2.0f * sweepDirection);
    } else {
      currentEndAngle = currentStartAngle + arcSweepLeft * sweepDirection;
    }

    Point currentStartPoint(aOrigin.x + cosf(currentStartAngle) * aRadius.width,
                            aOrigin.y + sinf(currentStartAngle) * aRadius.height);
    Point currentEndPoint(aOrigin.x + cosf(currentEndAngle) * aRadius.width,
                          aOrigin.y + sinf(currentEndAngle) * aRadius.height);

    // Calculate kappa constant for partial curve. The sign of angle in the
    // tangent will actually ensure this is negative for a counter clockwise
    // sweep, so changing signs later isn't needed.
    Float kappaFactor = (4.0f / 3.0f) * tan((currentEndAngle - currentStartAngle) / 4.0f);
    Float kappaX = kappaFactor * aRadius.width;
    Float kappaY = kappaFactor * aRadius.height;

    Point tangentStart(-sin(currentStartAngle), cos(currentStartAngle));
    Point cp1 = currentStartPoint;
    cp1 += Point(tangentStart.x * kappaX, tangentStart.y * kappaY);

    Point revTangentEnd(sin(currentEndAngle), -cos(currentEndAngle));
    Point cp2 = currentEndPoint;
    cp2 += Point(revTangentEnd.x * kappaX, revTangentEnd.y * kappaY);

    aSink->BezierTo(cp1, cp2, currentEndPoint);

    arcSweepLeft -= Float(M_PI / 2.0f);
    currentStartAngle = currentEndAngle;
  }
}

/* This is basically the ArcToBezier with the parameters for drawing a circle
 * inlined which vastly simplifies it and avoids a bunch of transcedental function
 * calls which should make it faster. */
template <typename T>
void EllipseToBezier(T* aSink, const Point &aOrigin, const Size &aRadius)
{
  Point startPoint(aOrigin.x + aRadius.width,
                   aOrigin.y);

  aSink->LineTo(startPoint);

  // Calculate kappa constant for partial curve. The sign of angle in the
  // tangent will actually ensure this is negative for a counter clockwise
  // sweep, so changing signs later isn't needed.
  Float kappaFactor = (4.0f / 3.0f) * tan((M_PI/2.0f) / 4.0f);
  Float kappaX = kappaFactor * aRadius.width;
  Float kappaY = kappaFactor * aRadius.height;
  Float cosStartAngle = 1;
  Float sinStartAngle = 0;
  for (int i = 0; i < 4; i++) {
    // We guarantee here the current point is the start point of the next
    // curve segment.
    Point currentStartPoint(aOrigin.x + cosStartAngle * aRadius.width,
                            aOrigin.y + sinStartAngle * aRadius.height);
    Point currentEndPoint(aOrigin.x + -sinStartAngle * aRadius.width,
                          aOrigin.y + cosStartAngle * aRadius.height);

    Point tangentStart(-sinStartAngle, cosStartAngle);
    Point cp1 = currentStartPoint;
    cp1 += Point(tangentStart.x * kappaX, tangentStart.y * kappaY);

    Point revTangentEnd(cosStartAngle, sinStartAngle);
    Point cp2 = currentEndPoint;
    cp2 += Point(revTangentEnd.x * kappaX, revTangentEnd.y * kappaY);

    aSink->BezierTo(cp1, cp2, currentEndPoint);

    // cos(x+pi/2) == -sin(x)
    // sin(x+pi/2) == cos(x)
    Float tmp = cosStartAngle;
    cosStartAngle = -sinStartAngle;
    sinStartAngle = tmp;
  }
}

/**
 * Appends a path represending a rectangle to the path being built by
 * aPathBuilder.
 *
 * aRect           The rectangle to append.
 * aDrawClockwise  If set to true, the path will start at the left of the top
 *                 left edge and draw clockwise. If set to false the path will
 *                 start at the right of the top left edge and draw counter-
 *                 clockwise.
 */
GFX2D_API void AppendRectToPath(PathBuilder* aPathBuilder,
                                const Rect& aRect,
                                bool aDrawClockwise = true);

inline TemporaryRef<Path> MakePathForRect(const DrawTarget& aDrawTarget,
                                          const Rect& aRect,
                                          bool aDrawClockwise = true)
{
  RefPtr<PathBuilder> builder = aDrawTarget.CreatePathBuilder();
  AppendRectToPath(builder, aRect, aDrawClockwise);
  return builder->Finish();
}

struct RectCornerRadii {
  Size radii[RectCorner::Count];

  RectCornerRadii() {}

  explicit RectCornerRadii(Float radius) {
    for (int i = 0; i < RectCorner::Count; i++) {
      radii[i].SizeTo(radius, radius);
    }
  }

  explicit RectCornerRadii(Float radiusX, Float radiusY) {
    for (int i = 0; i < RectCorner::Count; i++) {
      radii[i].SizeTo(radiusX, radiusY);
    }
  }

  RectCornerRadii(Float tl, Float tr, Float br, Float bl) {
    radii[RectCorner::TopLeft].SizeTo(tl, tl);
    radii[RectCorner::TopRight].SizeTo(tr, tr);
    radii[RectCorner::BottomRight].SizeTo(br, br);
    radii[RectCorner::BottomLeft].SizeTo(bl, bl);
  }

  RectCornerRadii(const Size& tl, const Size& tr,
                  const Size& br, const Size& bl) {
    radii[RectCorner::TopLeft] = tl;
    radii[RectCorner::TopRight] = tr;
    radii[RectCorner::BottomRight] = br;
    radii[RectCorner::BottomLeft] = bl;
  }

  const Size& operator[](size_t aCorner) const {
    return radii[aCorner];
  }

  Size& operator[](size_t aCorner) {
    return radii[aCorner];
  }

  void Scale(Float aXScale, Float aYScale) {
    for (int i = 0; i < RectCorner::Count; i++) {
      radii[i].Scale(aXScale, aYScale);
    }
  }

  const Size TopLeft() const { return radii[RectCorner::TopLeft]; }
  Size& TopLeft() { return radii[RectCorner::TopLeft]; }

  const Size TopRight() const { return radii[RectCorner::TopRight]; }
  Size& TopRight() { return radii[RectCorner::TopRight]; }

  const Size BottomRight() const { return radii[RectCorner::BottomRight]; }
  Size& BottomRight() { return radii[RectCorner::BottomRight]; }

  const Size BottomLeft() const { return radii[RectCorner::BottomLeft]; }
  Size& BottomLeft() { return radii[RectCorner::BottomLeft]; }
};

/**
 * Appends a path represending a rounded rectangle to the path being built by
 * aPathBuilder.
 *
 * aRect           The rectangle to append.
 * aCornerRadii    Contains the radii of the top-left, top-right, bottom-right
 *                 and bottom-left corners, in that order.
 * aDrawClockwise  If set to true, the path will start at the left of the top
 *                 left edge and draw clockwise. If set to false the path will
 *                 start at the right of the top left edge and draw counter-
 *                 clockwise.
 */
GFX2D_API void AppendRoundedRectToPath(PathBuilder* aPathBuilder,
                                       const Rect& aRect,
                                       const RectCornerRadii& aRadii,
                                       bool aDrawClockwise = true);

inline TemporaryRef<Path> MakePathForRoundedRect(const DrawTarget& aDrawTarget,
                                                 const Rect& aRect,
                                                 const RectCornerRadii& aRadii,
                                                 bool aDrawClockwise = true)
{
  RefPtr<PathBuilder> builder = aDrawTarget.CreatePathBuilder();
  AppendRoundedRectToPath(builder, aRect, aRadii, aDrawClockwise);
  return builder->Finish();
}

/**
 * Appends a path represending an ellipse to the path being built by
 * aPathBuilder.
 *
 * The ellipse extends aDimensions.width / 2.0 in the horizontal direction
 * from aCenter, and aDimensions.height / 2.0 in the vertical direction.
 */
GFX2D_API void AppendEllipseToPath(PathBuilder* aPathBuilder,
                                   const Point& aCenter,
                                   const Size& aDimensions);

inline TemporaryRef<Path> MakePathForEllipse(const DrawTarget& aDrawTarget,
                                             const Point& aCenter,
                                             const Size& aDimensions)
{
  RefPtr<PathBuilder> builder = aDrawTarget.CreatePathBuilder();
  AppendEllipseToPath(builder, aCenter, aDimensions);
  return builder->Finish();
}

/**
 * If aDrawTarget's transform only contains a translation, and if this line is
 * a horizontal or vertical line, this function will snap the line's vertices
 * to align with the device pixel grid so that stroking the line with a one
 * pixel wide stroke will result in a crisp line that is not antialiased over
 * two pixels across its width.
 *
 * @return Returns true if this function snaps aRect's vertices, else returns
 *   false.
 */
GFX2D_API bool SnapLineToDevicePixelsForStroking(Point& aP1, Point& aP2,
                                                 const DrawTarget& aDrawTarget);

/**
 * This function paints each edge of aRect separately, snapping the edges using
 * SnapLineToDevicePixelsForStroking. Stroking the edges as separate paths
 * helps ensure not only that the stroke spans a single row of device pixels if
 * possible, but also that the ends of stroke dashes start and end on device
 * pixels too.
 */
GFX2D_API void StrokeSnappedEdgesOfRect(const Rect& aRect,
                                        DrawTarget& aDrawTarget,
                                        const ColorPattern& aColor,
                                        const StrokeOptions& aStrokeOptions);

extern UserDataKey sDisablePixelSnapping;

/**
 * If aDrawTarget's transform only contains a translation or, if
 * aAllowScaleOr90DegreeRotate is true, and/or a scale/90 degree rotation, this
 * function will convert aRect to device space and snap it to device pixels.
 * This function returns true if aRect is modified, otherwise it returns false.
 *
 * Note that the snapping is such that filling the rect using a DrawTarget
 * which has the identity matrix as its transform will result in crisp edges.
 * (That is, aRect will have integer values, aligning its edges between pixel
 * boundaries.)  If on the other hand you stroking the rect with an odd valued
 * stroke width then the edges of the stroke will be antialiased (assuming an
 * AntialiasMode that does antialiasing).
 */
inline bool UserToDevicePixelSnapped(Rect& aRect, const DrawTarget& aDrawTarget,
                                     bool aAllowScaleOr90DegreeRotate = false)
{
  if (aDrawTarget.GetUserData(&sDisablePixelSnapping)) {
    return false;
  }

  Matrix mat = aDrawTarget.GetTransform();

  const Float epsilon = 0.0000001f;
#define WITHIN_E(a,b) (fabs((a)-(b)) < epsilon)
  if (!aAllowScaleOr90DegreeRotate &&
      (!WITHIN_E(mat._11, 1.f) || !WITHIN_E(mat._22, 1.f) ||
       !WITHIN_E(mat._12, 0.f) || !WITHIN_E(mat._21, 0.f))) {
    // We have non-translation, but only translation is allowed.
    return false;
  }
#undef WITHIN_E

  Point p1 = mat * aRect.TopLeft();
  Point p2 = mat * aRect.TopRight();
  Point p3 = mat * aRect.BottomRight();

  // Check that the rectangle is axis-aligned. For an axis-aligned rectangle,
  // two opposite corners define the entire rectangle. So check if
  // the axis-aligned rectangle with opposite corners p1 and p3
  // define an axis-aligned rectangle whose other corners are p2 and p4.
  // We actually only need to check one of p2 and p4, since an affine
  // transform maps parallelograms to parallelograms.
  if (p2 == Point(p1.x, p3.y) || p2 == Point(p3.x, p1.y)) {
      p1.Round();
      p3.Round();

      aRect.MoveTo(Point(std::min(p1.x, p3.x), std::min(p1.y, p3.y)));
      aRect.SizeTo(Size(std::max(p1.x, p3.x) - aRect.X(),
                        std::max(p1.y, p3.y) - aRect.Y()));
      return true;
  }

  return false;
}

/**
 * This function has the same behavior as UserToDevicePixelSnapped except that
 * aRect is not transformed to device space.
 */
inline void MaybeSnapToDevicePixels(Rect& aRect, const DrawTarget& aDrawTarget,
                                    bool aIgnoreScale = false)
{
  if (UserToDevicePixelSnapped(aRect, aDrawTarget, aIgnoreScale)) {
    // Since UserToDevicePixelSnapped returned true we know there is no
    // rotation/skew in 'mat', so we can just use TransformBounds() here.
    Matrix mat = aDrawTarget.GetTransform();
    mat.Invert();
    aRect = mat.TransformBounds(aRect);
  }
}

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_PATHHELPERS_H_ */
