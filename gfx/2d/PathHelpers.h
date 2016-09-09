/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PATHHELPERS_H_
#define MOZILLA_GFX_PATHHELPERS_H_

#include "2D.h"
#include "UserData.h"

#include <cmath>

namespace mozilla {
namespace gfx {

// Kappa constant for 90-degree angle
const Float kKappaFactor = 0.55191497064665766025f;

// Calculate kappa constant for partial curve. The sign of angle in the
// tangent will actually ensure this is negative for a counter clockwise
// sweep, so changing signs later isn't needed.
inline Float ComputeKappaFactor(Float aAngle)
{
  return (4.0f / 3.0f) * tanf(aAngle / 4.0f);
}

/**
 * Draws a partial arc <= 90 degrees given exact start and end points.
 * Assumes that it is continuing from an already specified start point.
 */
template <typename T>
inline void PartialArcToBezier(T* aSink,
                               const Point& aStartOffset, const Point& aEndOffset,
                               const Matrix& aTransform,
                               Float aKappaFactor = kKappaFactor)
{
  Point cp1 =
    aStartOffset + Point(-aStartOffset.y, aStartOffset.x) * aKappaFactor;

  Point cp2 =
    aEndOffset + Point(aEndOffset.y, -aEndOffset.x) * aKappaFactor;

  aSink->BezierTo(aTransform.TransformPoint(cp1),
                  aTransform.TransformPoint(cp2),
                  aTransform.TransformPoint(aEndOffset));
}

/**
 * Draws an acute arc (<= 90 degrees) given exact start and end points.
 * Specialized version avoiding kappa calculation.
 */
template <typename T>
inline void AcuteArcToBezier(T* aSink,
                             const Point& aOrigin, const Size& aRadius,
                             const Point& aStartPoint, const Point& aEndPoint,
                             Float aKappaFactor = kKappaFactor)
{
  aSink->LineTo(aStartPoint);
  if (!aRadius.IsEmpty()) {
    Float kappaX = aKappaFactor * aRadius.width / aRadius.height;
    Float kappaY = aKappaFactor * aRadius.height / aRadius.width;
    Point startOffset = aStartPoint - aOrigin;
    Point endOffset = aEndPoint - aOrigin;
    aSink->BezierTo(aStartPoint + Point(-startOffset.y * kappaX, startOffset.x * kappaY),
                    aEndPoint + Point(endOffset.y * kappaX, -endOffset.x * kappaY),
                    aEndPoint);
  } else if (aEndPoint != aStartPoint) {
    aSink->LineTo(aEndPoint);
  }
}

/**
 * Draws an acute arc (<= 90 degrees) given exact start and end points.
 */
template <typename T>
inline void AcuteArcToBezier(T* aSink,
                             const Point& aOrigin, const Size& aRadius,
                             const Point& aStartPoint, const Point& aEndPoint,
                             Float aStartAngle, Float aEndAngle)
{
  AcuteArcToBezier(aSink, aOrigin, aRadius, aStartPoint, aEndPoint,
                   ComputeKappaFactor(aEndAngle - aStartAngle));
}

template <typename T>
void ArcToBezier(T* aSink, const Point &aOrigin, const Size &aRadius,
                 float aStartAngle, float aEndAngle, bool aAntiClockwise,
                 float aRotation = 0.0f)
{
  Float sweepDirection = aAntiClockwise ? -1.0f : 1.0f;

  // Calculate the total arc we're going to sweep.
  Float arcSweepLeft = (aEndAngle - aStartAngle) * sweepDirection;

  // Clockwise we always sweep from the smaller to the larger angle, ccw
  // it's vice versa.
  if (arcSweepLeft < 0) {
    // Rerverse sweep is modulo'd into range rather than clamped.
    arcSweepLeft = Float(2.0f * M_PI) + fmodf(arcSweepLeft, Float(2.0f * M_PI));
    // Recalculate the start angle to land closer to end angle.
    aStartAngle = aEndAngle - arcSweepLeft * sweepDirection;
  } else if (arcSweepLeft > Float(2.0f * M_PI)) {
    // Sweeping more than 2 * pi is a full circle.
    arcSweepLeft = Float(2.0f * M_PI);
  }

  Float currentStartAngle = aStartAngle;
  Point currentStartOffset(cosf(aStartAngle), sinf(aStartAngle));
  Matrix transform = Matrix::Scaling(aRadius.width, aRadius.height);
  if (aRotation != 0.0f) {
    transform *= Matrix::Rotation(aRotation);
  }
  transform.PostTranslate(aOrigin);
  aSink->LineTo(transform.TransformPoint(currentStartOffset));

  while (arcSweepLeft > 0) {
    Float currentEndAngle =
      currentStartAngle + std::min(arcSweepLeft, Float(M_PI / 2.0f)) * sweepDirection;
    Point currentEndOffset(cosf(currentEndAngle), sinf(currentEndAngle));

    PartialArcToBezier(aSink, currentStartOffset, currentEndOffset, transform,
                       ComputeKappaFactor(currentEndAngle - currentStartAngle));

    // We guarantee here the current point is the start point of the next
    // curve segment.
    arcSweepLeft -= Float(M_PI / 2.0f);
    currentStartAngle = currentEndAngle;
    currentStartOffset = currentEndOffset;
  }
}

/* This is basically the ArcToBezier with the parameters for drawing a circle
 * inlined which vastly simplifies it and avoids a bunch of transcedental function
 * calls which should make it faster. */
template <typename T>
void EllipseToBezier(T* aSink, const Point &aOrigin, const Size &aRadius)
{
  Matrix transform(aRadius.width, 0, 0, aRadius.height, aOrigin.x, aOrigin.y);
  Point currentStartOffset(1, 0);

  aSink->LineTo(transform.TransformPoint(currentStartOffset));

  for (int i = 0; i < 4; i++) {
    // cos(x+pi/2) == -sin(x)
    // sin(x+pi/2) == cos(x)
    Point currentEndOffset(-currentStartOffset.y, currentStartOffset.x);

    PartialArcToBezier(aSink, currentStartOffset, currentEndOffset, transform);

    // We guarantee here the current point is the start point of the next
    // curve segment.
    currentStartOffset = currentEndOffset;
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

inline already_AddRefed<Path> MakePathForRect(const DrawTarget& aDrawTarget,
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

  bool operator==(const RectCornerRadii& aOther) const {
    for (size_t i = 0; i < RectCorner::Count; i++) {
      if (radii[i] != aOther.radii[i]) return false;
    }
    return true;
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

inline already_AddRefed<Path> MakePathForRoundedRect(const DrawTarget& aDrawTarget,
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

inline already_AddRefed<Path> MakePathForEllipse(const DrawTarget& aDrawTarget,
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
                                                 const DrawTarget& aDrawTarget,
                                                 Float aLineWidth);

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

/**
 * Return the margin, in device space, by which a stroke can extend beyond the
 * rendered shape.
 * @param  aStrokeOptions The stroke options that the stroke is drawn with.
 * @param  aTransform     The user space to device space transform.
 * @return                The stroke margin.
 */
GFX2D_API Margin MaxStrokeExtents(const StrokeOptions& aStrokeOptions,
                                  const Matrix& aTransform);

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
 *
 * Empty snaps are those which result in a rectangle of 0 area.  If they are
 * disallowed, an axis is left unsnapped if the rounding process results in a
 * length of 0.
 */
inline bool UserToDevicePixelSnapped(Rect& aRect, const DrawTarget& aDrawTarget,
                                     bool aAllowScaleOr90DegreeRotate = false,
                                     bool aAllowEmptySnaps = true)
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

  Point p1 = mat.TransformPoint(aRect.TopLeft());
  Point p2 = mat.TransformPoint(aRect.TopRight());
  Point p3 = mat.TransformPoint(aRect.BottomRight());

  // Check that the rectangle is axis-aligned. For an axis-aligned rectangle,
  // two opposite corners define the entire rectangle. So check if
  // the axis-aligned rectangle with opposite corners p1 and p3
  // define an axis-aligned rectangle whose other corners are p2 and p4.
  // We actually only need to check one of p2 and p4, since an affine
  // transform maps parallelograms to parallelograms.
  if (p2 == Point(p1.x, p3.y) || p2 == Point(p3.x, p1.y)) {
      Point p1r = p1;
      Point p3r = p3;
      p1r.Round();
      p3r.Round();
      if (aAllowEmptySnaps || p1r.x != p3r.x) {
          p1.x = p1r.x;
          p3.x = p3r.x;
      }
      if (aAllowEmptySnaps || p1r.y != p3r.y) {
          p1.y = p1r.y;
          p3.y = p3r.y;
      }

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
inline bool MaybeSnapToDevicePixels(Rect& aRect, const DrawTarget& aDrawTarget,
                                    bool aAllowScaleOr90DegreeRotate = false,
                                    bool aAllowEmptySnaps = true)
{
  if (UserToDevicePixelSnapped(aRect, aDrawTarget,
                               aAllowScaleOr90DegreeRotate, aAllowEmptySnaps)) {
    // Since UserToDevicePixelSnapped returned true we know there is no
    // rotation/skew in 'mat', so we can just use TransformBounds() here.
    Matrix mat = aDrawTarget.GetTransform();
    mat.Invert();
    aRect = mat.TransformBounds(aRect);
    return true;
  }
  return false;
}

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_PATHHELPERS_H_ */
