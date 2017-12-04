/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BASERECT_H_
#define MOZILLA_GFX_BASERECT_H_

#include <algorithm>
#include <cmath>
#include <ostream>

#include "mozilla/Assertions.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/TypeTraits.h"
#include "Types.h"

namespace mozilla {
namespace gfx {

/**
 * Rectangles have two interpretations: a set of (zero-size) points,
 * and a rectangular area of the plane. Most rectangle operations behave
 * the same no matter what interpretation is being used, but some operations
 * differ:
 * -- Equality tests behave differently. When a rectangle represents an area,
 * all zero-width and zero-height rectangles are equal to each other since they
 * represent the empty area. But when a rectangle represents a set of
 * mathematical points, zero-width and zero-height rectangles can be unequal.
 * -- The union operation can behave differently. When rectangles represent
 * areas, taking the union of a zero-width or zero-height rectangle with
 * another rectangle can just ignore the empty rectangle. But when rectangles
 * represent sets of mathematical points, we may need to extend the latter
 * rectangle to include the points of a zero-width or zero-height rectangle.
 *
 * To ensure that these interpretations are explicitly disambiguated, we
 * deny access to the == and != operators and require use of IsEqualEdges and
 * IsEqualInterior instead. Similarly we provide separate Union and UnionEdges
 * methods.
 *
 * Do not use this class directly. Subclass it, pass that subclass as the
 * Sub parameter, and only use that subclass.
 */
template <class T, class Sub, class Point, class SizeT, class MarginT>
struct BaseRect {
  T x, y, width, height;

  // Constructors
  BaseRect() : x(0), y(0), width(0), height(0) {}
  BaseRect(const Point& aOrigin, const SizeT &aSize) :
      x(aOrigin.x), y(aOrigin.y), width(aSize.width), height(aSize.height)
  {
  }
  BaseRect(T aX, T aY, T aWidth, T aHeight) :
      x(aX), y(aY), width(aWidth), height(aHeight)
  {
  }

  // Emptiness. An empty rect is one that has no area, i.e. its height or width
  // is <= 0
  bool IsEmpty() const { return height <= 0 || width <= 0; }
  void SetEmpty() { width = height = 0; }

  // "Finite" means not inf and not NaN
  bool IsFinite() const
  {
    typedef typename mozilla::Conditional<mozilla::IsSame<T, float>::value, float, double>::Type FloatType;
    return (mozilla::IsFinite(FloatType(x)) &&
            mozilla::IsFinite(FloatType(y)) &&
            mozilla::IsFinite(FloatType(width)) &&
            mozilla::IsFinite(FloatType(height)));
  }

  // Returns true if this rectangle contains the interior of aRect. Always
  // returns true if aRect is empty, and always returns false is aRect is
  // nonempty but this rect is empty.
  bool Contains(const Sub& aRect) const
  {
    return aRect.IsEmpty() ||
           (x <= aRect.x && aRect.XMost() <= XMost() &&
            y <= aRect.y && aRect.YMost() <= YMost());
  }
  // Returns true if this rectangle contains the point. Points are considered
  // in the rectangle if they are on the left or top edge, but outside if they
  // are on the right or bottom edge.
  bool Contains(T aX, T aY) const
  {
    return x <= aX && aX < XMost() &&
           y <= aY && aY < YMost();
  }
  // Returns true if this rectangle contains the point. Points are considered
  // in the rectangle if they are on the left or top edge, but outside if they
  // are on the right or bottom edge.
  bool Contains(const Point& aPoint) const { return Contains(aPoint.x, aPoint.y); }

  // Intersection. Returns TRUE if the receiver's area has non-empty
  // intersection with aRect's area, and FALSE otherwise.
  // Always returns false if aRect is empty or 'this' is empty.
  bool Intersects(const Sub& aRect) const
  {
    return !IsEmpty() && !aRect.IsEmpty() &&
           x < aRect.XMost() && aRect.x < XMost() &&
           y < aRect.YMost() && aRect.y < YMost();
  }
  // Returns the rectangle containing the intersection of the points
  // (including edges) of *this and aRect. If there are no points in that
  // intersection, returns an empty rectangle with x/y set to the std::max of the x/y
  // of *this and aRect.
  MOZ_MUST_USE Sub Intersect(const Sub& aRect) const
  {
    Sub result;
    result.x = std::max<T>(x, aRect.x);
    result.y = std::max<T>(y, aRect.y);
    result.width = std::min<T>(x - result.x + width, aRect.x - result.x + aRect.width);
    result.height = std::min<T>(y - result.y + height, aRect.y - result.y + aRect.height);
    if (result.width < 0 || result.height < 0) {
      result.SizeTo(0, 0);
    }
    return result;
  }
  // Sets *this to be the rectangle containing the intersection of the points
  // (including edges) of *this and aRect. If there are no points in that
  // intersection, sets *this to be an empty rectangle with x/y set to the std::max
  // of the x/y of *this and aRect.
  //
  // 'this' can be the same object as either aRect1 or aRect2
  bool IntersectRect(const Sub& aRect1, const Sub& aRect2)
  {
    *static_cast<Sub*>(this) = aRect1.Intersect(aRect2);
    return !IsEmpty();
  }

  // Returns the smallest rectangle that contains both the area of both
  // this and aRect2.
  // Thus, empty input rectangles are ignored.
  // If both rectangles are empty, returns this.
  // WARNING! This is not safe against overflow, prefer using SafeUnion instead
  // when dealing with int-based rects.
  MOZ_MUST_USE Sub Union(const Sub& aRect) const
  {
    if (IsEmpty()) {
      return aRect;
    } else if (aRect.IsEmpty()) {
      return *static_cast<const Sub*>(this);
    } else {
      return UnionEdges(aRect);
    }
  }
  // Returns the smallest rectangle that contains both the points (including
  // edges) of both aRect1 and aRect2.
  // Thus, empty input rectangles are allowed to affect the result.
  // WARNING! This is not safe against overflow, prefer using SafeUnionEdges
  // instead when dealing with int-based rects.
  MOZ_MUST_USE Sub UnionEdges(const Sub& aRect) const
  {
    Sub result;
    result.x = std::min(x, aRect.x);
    result.y = std::min(y, aRect.y);
    result.width = std::max(XMost(), aRect.XMost()) - result.x;
    result.height = std::max(YMost(), aRect.YMost()) - result.y;
    return result;
  }
  // Computes the smallest rectangle that contains both the area of both
  // aRect1 and aRect2, and fills 'this' with the result.
  // Thus, empty input rectangles are ignored.
  // If both rectangles are empty, sets 'this' to aRect2.
  //
  // 'this' can be the same object as either aRect1 or aRect2
  void UnionRect(const Sub& aRect1, const Sub& aRect2)
  {
    *static_cast<Sub*>(this) = aRect1.Union(aRect2);
  }

  // Computes the smallest rectangle that contains both the points (including
  // edges) of both aRect1 and aRect2.
  // Thus, empty input rectangles are allowed to affect the result.
  //
  // 'this' can be the same object as either aRect1 or aRect2
  void UnionRectEdges(const Sub& aRect1, const Sub& aRect2)
  {
    *static_cast<Sub*>(this) = aRect1.UnionEdges(aRect2);
  }

  // Expands the rect to include the point
  void ExpandToEnclose(const Point& aPoint)
  {
    if (aPoint.x < x) {
      width = XMost() - aPoint.x;
      x = aPoint.x;
    } else if (aPoint.x > XMost()) {
      width = aPoint.x - x;
    }
    if (aPoint.y < y) {
      height = YMost() - aPoint.y;
      y = aPoint.y;
    } else if (aPoint.y > YMost()) {
      height = aPoint.y - y;
    }
  }

  void SetRect(T aX, T aY, T aWidth, T aHeight)
  {
    x = aX; y = aY; width = aWidth; height = aHeight;
  }
  void SetRect(const Point& aPt, const SizeT& aSize)
  {
    SetRect(aPt.x, aPt.y, aSize.width, aSize.height);
  }
  void GetRect(T* aX, T* aY, T* aWidth, T* aHeight)
  {
    *aX = x; *aY = y; *aWidth = width; *aHeight = height;
  }

  void MoveTo(T aX, T aY) { x = aX; y = aY; }
  void MoveTo(const Point& aPoint) { x = aPoint.x; y = aPoint.y; }
  void MoveBy(T aDx, T aDy) { x += aDx; y += aDy; }
  void MoveBy(const Point& aPoint) { x += aPoint.x; y += aPoint.y; }
  void SizeTo(T aWidth, T aHeight) { width = aWidth; height = aHeight; }
  void SizeTo(const SizeT& aSize) { width = aSize.width; height = aSize.height; }

  void Inflate(T aD) { Inflate(aD, aD); }
  void Inflate(T aDx, T aDy)
  {
    x -= aDx;
    y -= aDy;
    width += 2 * aDx;
    height += 2 * aDy;
  }
  void Inflate(const MarginT& aMargin)
  {
    x -= aMargin.left;
    y -= aMargin.top;
    width += aMargin.LeftRight();
    height += aMargin.TopBottom();
  }
  void Inflate(const SizeT& aSize) { Inflate(aSize.width, aSize.height); }

  void Deflate(T aD) { Deflate(aD, aD); }
  void Deflate(T aDx, T aDy)
  {
    x += aDx;
    y += aDy;
    width = std::max(T(0), width - 2 * aDx);
    height = std::max(T(0), height - 2 * aDy);
  }
  void Deflate(const MarginT& aMargin)
  {
    x += aMargin.left;
    y += aMargin.top;
    width = std::max(T(0), width - aMargin.LeftRight());
    height = std::max(T(0), height - aMargin.TopBottom());
  }
  void Deflate(const SizeT& aSize) { Deflate(aSize.width, aSize.height); }

  // Return true if the rectangles contain the same set of points, including
  // points on the edges.
  // Use when we care about the exact x/y/width/height values being
  // equal (i.e. we care about differences in empty rectangles).
  bool IsEqualEdges(const Sub& aRect) const
  {
    return x == aRect.x && y == aRect.y &&
           width == aRect.width && height == aRect.height;
  }
  // Return true if the rectangles contain the same area of the plane.
  // Use when we do not care about differences in empty rectangles.
  bool IsEqualInterior(const Sub& aRect) const
  {
    return IsEqualEdges(aRect) || (IsEmpty() && aRect.IsEmpty());
  }

  friend Sub operator+(Sub aSub, const Point& aPoint)
  {
    aSub += aPoint;
    return aSub;
  }
  friend Sub operator-(Sub aSub, const Point& aPoint)
  {
    aSub -= aPoint;
    return aSub;
  }
  friend Sub operator+(Sub aSub, const SizeT& aSize)
  {
    aSub += aSize;
    return aSub;
  }
  friend Sub operator-(Sub aSub, const SizeT& aSize)
  {
    aSub -= aSize;
    return aSub;
  }
  Sub& operator+=(const Point& aPoint)
  {
    MoveBy(aPoint);
    return *static_cast<Sub*>(this);
  }
  Sub& operator-=(const Point& aPoint)
  {
    MoveBy(-aPoint);
    return *static_cast<Sub*>(this);
  }
  Sub& operator+=(const SizeT& aSize)
  {
    width += aSize.width;
    height += aSize.height;
    return *static_cast<Sub*>(this);
  }
  Sub& operator-=(const SizeT& aSize)
  {
    width -= aSize.width;
    height -= aSize.height;
    return *static_cast<Sub*>(this);
  }
  // Find difference as a Margin
  MarginT operator-(const Sub& aRect) const
  {
    return MarginT(aRect.y - y,
                   XMost() - aRect.XMost(),
                   YMost() - aRect.YMost(),
                   aRect.x - x);
  }

  // Helpers for accessing the vertices
  Point TopLeft() const { return Point(x, y); }
  Point TopRight() const { return Point(XMost(), y); }
  Point BottomLeft() const { return Point(x, YMost()); }
  Point BottomRight() const { return Point(XMost(), YMost()); }
  Point AtCorner(Corner aCorner) const {
    switch (aCorner) {
      case eCornerTopLeft: return TopLeft();
      case eCornerTopRight: return TopRight();
      case eCornerBottomRight: return BottomRight();
      case eCornerBottomLeft: return BottomLeft();
    }
    MOZ_CRASH("GFX: Incomplete switch");
  }
  Point CCWCorner(mozilla::Side side) const {
    switch (side) {
      case eSideTop: return TopLeft();
      case eSideRight: return TopRight();
      case eSideBottom: return BottomRight();
      case eSideLeft: return BottomLeft();
    }
    MOZ_CRASH("GFX: Incomplete switch");
  }
  Point CWCorner(mozilla::Side side) const {
    switch (side) {
      case eSideTop: return TopRight();
      case eSideRight: return BottomRight();
      case eSideBottom: return BottomLeft();
      case eSideLeft: return TopLeft();
    }
    MOZ_CRASH("GFX: Incomplete switch");
  }
  Point Center() const { return Point(x, y) + Point(width, height)/2; }
  SizeT Size() const { return SizeT(width, height); }

  T Area() const { return width * height; }

  // Helper methods for computing the extents
  MOZ_ALWAYS_INLINE T X() const { return x; }
  MOZ_ALWAYS_INLINE T Y() const { return y; }
  MOZ_ALWAYS_INLINE T Width() const { return width; }
  MOZ_ALWAYS_INLINE T Height() const { return height; }
  MOZ_ALWAYS_INLINE T XMost() const { return x + width; }
  MOZ_ALWAYS_INLINE T YMost() const { return y + height; }

  // Set width and height. SizeTo() sets them together.
  MOZ_ALWAYS_INLINE void SetWidth(T aWidth) { width = aWidth; }
  MOZ_ALWAYS_INLINE void SetHeight(T aHeight) { height = aHeight; }

  // Get the coordinate of the edge on the given side.
  T Edge(mozilla::Side aSide) const
  {
    switch (aSide) {
      case eSideTop: return Y();
      case eSideRight: return XMost();
      case eSideBottom: return YMost();
      case eSideLeft: return X();
    }
    MOZ_CRASH("GFX: Incomplete switch");
  }

  // Moves one edge of the rect without moving the opposite edge.
  void SetLeftEdge(T aX) {
    MOZ_ASSERT(aX <= XMost());
    width = XMost() - aX;
    x = aX;
  }
  void SetRightEdge(T aXMost) { 
    MOZ_ASSERT(aXMost >= x);
    width = aXMost - x; 
  }
  void SetTopEdge(T aY) {
    MOZ_ASSERT(aY <= YMost());
    height = YMost() - aY;
    y = aY;
  }
  void SetBottomEdge(T aYMost) { 
    MOZ_ASSERT(aYMost >= y);
    height = aYMost - y; 
  }
  void Swap() {
    std::swap(x, y);
    std::swap(width, height);
  }

  // Round the rectangle edges to integer coordinates, such that the rounded
  // rectangle has the same set of pixel centers as the original rectangle.
  // Edges at offset 0.5 round up.
  // Suitable for most places where integral device coordinates
  // are needed, but note that any translation should be applied first to
  // avoid pixel rounding errors.
  // Note that this is *not* rounding to nearest integer if the values are negative.
  // They are always rounding as floor(n + 0.5).
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=410748#c14
  // If you need similar method which is using NS_round(), you should create
  // new |RoundAwayFromZero()| method.
  void Round()
  {
    T x0 = static_cast<T>(std::floor(T(X()) + 0.5f));
    T y0 = static_cast<T>(std::floor(T(Y()) + 0.5f));
    T x1 = static_cast<T>(std::floor(T(XMost()) + 0.5f));
    T y1 = static_cast<T>(std::floor(T(YMost()) + 0.5f));

    x = x0;
    y = y0;

    width = x1 - x0;
    height = y1 - y0;
  }

  // Snap the rectangle edges to integer coordinates, such that the
  // original rectangle contains the resulting rectangle.
  void RoundIn()
  {
    T x0 = static_cast<T>(std::ceil(T(X())));
    T y0 = static_cast<T>(std::ceil(T(Y())));
    T x1 = static_cast<T>(std::floor(T(XMost())));
    T y1 = static_cast<T>(std::floor(T(YMost())));

    x = x0;
    y = y0;

    width = x1 - x0;
    height = y1 - y0;
  }

  // Snap the rectangle edges to integer coordinates, such that the
  // resulting rectangle contains the original rectangle.
  void RoundOut()
  {
    T x0 = static_cast<T>(std::floor(T(X())));
    T y0 = static_cast<T>(std::floor(T(Y())));
    T x1 = static_cast<T>(std::ceil(T(XMost())));
    T y1 = static_cast<T>(std::ceil(T(YMost())));

    x = x0;
    y = y0;

    width = x1 - x0;
    height = y1 - y0;
  }

  // Scale 'this' by aScale without doing any rounding.
  void Scale(T aScale) { Scale(aScale, aScale); }
  // Scale 'this' by aXScale and aYScale, without doing any rounding.
  void Scale(T aXScale, T aYScale)
  {
    T right = XMost() * aXScale;
    T bottom = YMost() * aYScale;
    x = x * aXScale;
    y = y * aYScale;
    width = right - x;
    height = bottom - y;
  }
  // Scale 'this' by aScale, converting coordinates to integers so that the result is
  // the smallest integer-coordinate rectangle containing the unrounded result.
  // Note: this can turn an empty rectangle into a non-empty rectangle
  void ScaleRoundOut(double aScale) { ScaleRoundOut(aScale, aScale); }
  // Scale 'this' by aXScale and aYScale, converting coordinates to integers so
  // that the result is the smallest integer-coordinate rectangle containing the
  // unrounded result.
  // Note: this can turn an empty rectangle into a non-empty rectangle
  void ScaleRoundOut(double aXScale, double aYScale)
  {
    T right = static_cast<T>(ceil(double(XMost()) * aXScale));
    T bottom = static_cast<T>(ceil(double(YMost()) * aYScale));
    x = static_cast<T>(floor(double(x) * aXScale));
    y = static_cast<T>(floor(double(y) * aYScale));
    width = right - x;
    height = bottom - y;
  }
  // Scale 'this' by aScale, converting coordinates to integers so that the result is
  // the largest integer-coordinate rectangle contained by the unrounded result.
  void ScaleRoundIn(double aScale) { ScaleRoundIn(aScale, aScale); }
  // Scale 'this' by aXScale and aYScale, converting coordinates to integers so
  // that the result is the largest integer-coordinate rectangle contained by the
  // unrounded result.
  void ScaleRoundIn(double aXScale, double aYScale)
  {
    T right = static_cast<T>(floor(double(XMost()) * aXScale));
    T bottom = static_cast<T>(floor(double(YMost()) * aYScale));
    x = static_cast<T>(ceil(double(x) * aXScale));
    y = static_cast<T>(ceil(double(y) * aYScale));
    width = std::max<T>(0, right - x);
    height = std::max<T>(0, bottom - y);
  }
  // Scale 'this' by 1/aScale, converting coordinates to integers so that the result is
  // the smallest integer-coordinate rectangle containing the unrounded result.
  // Note: this can turn an empty rectangle into a non-empty rectangle
  void ScaleInverseRoundOut(double aScale) { ScaleInverseRoundOut(aScale, aScale); }
  // Scale 'this' by 1/aXScale and 1/aYScale, converting coordinates to integers so
  // that the result is the smallest integer-coordinate rectangle containing the
  // unrounded result.
  // Note: this can turn an empty rectangle into a non-empty rectangle
  void ScaleInverseRoundOut(double aXScale, double aYScale)
  {
    T right = static_cast<T>(ceil(double(XMost()) / aXScale));
    T bottom = static_cast<T>(ceil(double(YMost()) / aYScale));
    x = static_cast<T>(floor(double(x) / aXScale));
    y = static_cast<T>(floor(double(y) / aYScale));
    width = right - x;
    height = bottom - y;
  }
  // Scale 'this' by 1/aScale, converting coordinates to integers so that the result is
  // the largest integer-coordinate rectangle contained by the unrounded result.
  void ScaleInverseRoundIn(double aScale) { ScaleInverseRoundIn(aScale, aScale); }
  // Scale 'this' by 1/aXScale and 1/aYScale, converting coordinates to integers so
  // that the result is the largest integer-coordinate rectangle contained by the
  // unrounded result.
  void ScaleInverseRoundIn(double aXScale, double aYScale)
  {
    T right = static_cast<T>(floor(double(XMost()) / aXScale));
    T bottom = static_cast<T>(floor(double(YMost()) / aYScale));
    x = static_cast<T>(ceil(double(x) / aXScale));
    y = static_cast<T>(ceil(double(y) / aYScale));
    width = std::max<T>(0, right - x);
    height = std::max<T>(0, bottom - y);
  }

  /**
   * Clamp aPoint to this rectangle. It is allowed to end up on any
   * edge of the rectangle.
   */
  MOZ_MUST_USE Point ClampPoint(const Point& aPoint) const
  {
    return Point(std::max(x, std::min(XMost(), aPoint.x)),
                 std::max(y, std::min(YMost(), aPoint.y)));
  }

  /**
   * Translate this rectangle to be inside aRect. If it doesn't fit inside
   * aRect then the dimensions that don't fit will be shrunk so that they
   * do fit. The resulting rect is returned.
   */
  MOZ_MUST_USE Sub MoveInsideAndClamp(const Sub& aRect) const
  {
    Sub rect(std::max(aRect.x, x),
             std::max(aRect.y, y),
             std::min(aRect.width, width),
             std::min(aRect.height, height));
    rect.x = std::min(rect.XMost(), aRect.XMost()) - rect.width;
    rect.y = std::min(rect.YMost(), aRect.YMost()) - rect.height;
    return rect;
  }

  // Returns the largest rectangle that can be represented with 32-bit
  // signed integers, centered around a point at 0,0.  As BaseRect's represent
  // the dimensions as a top-left point with a width and height, the width
  // and height will be the largest positive 32-bit value.  The top-left
  // position coordinate is divided by two to center the rectangle around a
  // point at 0,0.
  static Sub MaxIntRect()
  {
    return Sub(
      static_cast<T>(-std::numeric_limits<int32_t>::max() * 0.5),
      static_cast<T>(-std::numeric_limits<int32_t>::max() * 0.5),
      static_cast<T>(std::numeric_limits<int32_t>::max()),
      static_cast<T>(std::numeric_limits<int32_t>::max())
    );
  };

  // Returns a point representing the distance, along each dimension, of the
  // given point from this rectangle. The distance along a dimension is defined
  // as zero if the point is within the bounds of the rectangle in that
  // dimension; otherwise, it's the distance to the closer endpoint of the
  // rectangle in that dimension.
  Point DistanceTo(const Point& aPoint) const
  {
    return {DistanceFromInterval(aPoint.x, x, XMost()),
            DistanceFromInterval(aPoint.y, y, YMost())};
  }

  friend std::ostream& operator<<(std::ostream& stream,
      const BaseRect<T, Sub, Point, SizeT, MarginT>& aRect) {
    return stream << '(' << aRect.x << ',' << aRect.y << ','
                  << aRect.width << ',' << aRect.height << ')';
  }

private:
  // Do not use the default operator== or operator!= !
  // Use IsEqualEdges or IsEqualInterior explicitly.
  bool operator==(const Sub& aRect) const { return false; }
  bool operator!=(const Sub& aRect) const { return false; }

  // Helper function for DistanceTo() that computes the distance of a
  // coordinate along one dimension from an interval in that dimension.
  static T DistanceFromInterval(T aCoord, T aIntervalStart, T aIntervalEnd)
  {
    if (aCoord < aIntervalStart) {
      return aIntervalStart - aCoord;
    }
    if (aCoord > aIntervalEnd) {
      return aCoord - aIntervalEnd;
    }
    return 0;
  }
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_BASERECT_H_ */
