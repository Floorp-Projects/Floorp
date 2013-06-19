/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BASERECT_H_
#define MOZILLA_GFX_BASERECT_H_

#include <cmath>
#include <mozilla/Assertions.h>
#include <algorithm>

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
template <class T, class Sub, class Point, class SizeT, class Margin>
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

  // Returns true if this rectangle contains the interior of aRect. Always
  // returns true if aRect is empty, and always returns false is aRect is
  // nonempty but this rect is empty.
  bool Contains(const Sub& aRect) const
  {
    return aRect.IsEmpty() ||
           (x <= aRect.x && aRect.XMost() <= XMost() &&
            y <= aRect.y && aRect.YMost() <= YMost());
  }
  // Returns true if this rectangle contains the rectangle (aX,aY,1,1).
  bool Contains(T aX, T aY) const
  {
    return x <= aX && aX + 1 <= XMost() &&
           y <= aY && aY + 1 <= YMost();
  }
  // Returns true if this rectangle contains the rectangle (aPoint.x,aPoint.y,1,1).
  bool Contains(const Point& aPoint) const { return Contains(aPoint.x, aPoint.y); }

  // Intersection. Returns TRUE if the receiver's area has non-empty
  // intersection with aRect's area, and FALSE otherwise.
  // Always returns false if aRect is empty or 'this' is empty.
  bool Intersects(const Sub& aRect) const
  {
    return x < aRect.XMost() && aRect.x < XMost() &&
           y < aRect.YMost() && aRect.y < YMost();
  }
  // Returns the rectangle containing the intersection of the points
  // (including edges) of *this and aRect. If there are no points in that
  // intersection, returns an empty rectangle with x/y set to the std::max of the x/y
  // of *this and aRect.
  Sub Intersect(const Sub& aRect) const
  {
    Sub result;
    result.x = std::max(x, aRect.x);
    result.y = std::max(y, aRect.y);
    result.width = std::min(XMost(), aRect.XMost()) - result.x;
    result.height = std::min(YMost(), aRect.YMost()) - result.y;
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
  Sub Union(const Sub& aRect) const
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
  Sub UnionEdges(const Sub& aRect) const
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

  void SetRect(T aX, T aY, T aWidth, T aHeight)
  {
    x = aX; y = aY; width = aWidth; height = aHeight;
  }
  void SetRect(const Point& aPt, const SizeT& aSize)
  {
    SetRect(aPt.x, aPt.y, aSize.width, aSize.height);
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
  void Inflate(const Margin& aMargin)
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
  void Deflate(const Margin& aMargin)
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

  Sub operator+(const Point& aPoint) const
  {
    return Sub(x + aPoint.x, y + aPoint.y, width, height);
  }
  Sub operator-(const Point& aPoint) const
  {
    return Sub(x - aPoint.x, y - aPoint.y, width, height);
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

  // Find difference as a Margin
  Margin operator-(const Sub& aRect) const
  {
    return Margin(aRect.y - y,
                  XMost() - aRect.XMost(),
                  YMost() - aRect.YMost(),
                  aRect.x - x);
  }

  // Helpers for accessing the vertices
  Point TopLeft() const { return Point(x, y); }
  Point TopRight() const { return Point(XMost(), y); }
  Point BottomLeft() const { return Point(x, YMost()); }
  Point BottomRight() const { return Point(XMost(), YMost()); }
  Point Center() const { return Point(x, y) + Point(width, height)/2; }
  SizeT Size() const { return SizeT(width, height); }

  // Helper methods for computing the extents
  T X() const { return x; }
  T Y() const { return y; }
  T Width() const { return width; }
  T Height() const { return height; }
  T XMost() const { return x + width; }
  T YMost() const { return y + height; }

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
    T x0 = static_cast<T>(floor(T(X()) + 0.5));
    T y0 = static_cast<T>(floor(T(Y()) + 0.5));
    T x1 = static_cast<T>(floor(T(XMost()) + 0.5));
    T y1 = static_cast<T>(floor(T(YMost()) + 0.5));

    x = x0;
    y = y0;

    width = x1 - x0;
    height = y1 - y0;
  }

  // Snap the rectangle edges to integer coordinates, such that the
  // original rectangle contains the resulting rectangle.
  void RoundIn()
  {
    T x0 = static_cast<T>(ceil(T(X())));
    T y0 = static_cast<T>(ceil(T(Y())));
    T x1 = static_cast<T>(floor(T(XMost())));
    T y1 = static_cast<T>(floor(T(YMost())));

    x = x0;
    y = y0;

    width = x1 - x0;
    height = y1 - y0;
  }

  // Snap the rectangle edges to integer coordinates, such that the
  // resulting rectangle contains the original rectangle.
  void RoundOut()
  {
    T x0 = static_cast<T>(floor(T(X())));
    T y0 = static_cast<T>(floor(T(Y())));
    T x1 = static_cast<T>(ceil(T(XMost())));
    T y1 = static_cast<T>(ceil(T(YMost())));

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
  Point ClampPoint(const Point& aPoint) const
  {
    return Point(std::max(x, std::min(XMost(), aPoint.x)),
                 std::max(y, std::min(YMost(), aPoint.y)));
  }

  /**
   * Clamp aRect to this rectangle. This returns aRect after it is forced
   * inside the bounds of this rectangle. It will attempt to retain the size
   * but will shrink the dimensions that don't fit.
   */
  Sub ClampRect(const Sub& aRect) const
  {
    Sub rect(std::max(aRect.x, x),
             std::max(aRect.y, y),
             std::min(aRect.width, width),
             std::min(aRect.height, height));
    rect.x = std::min(rect.XMost(), XMost()) - rect.width;
    rect.y = std::min(rect.YMost(), YMost()) - rect.height;
    return rect;
  }

private:
  // Do not use the default operator== or operator!= !
  // Use IsEqualEdges or IsEqualInterior explicitly.
  bool operator==(const Sub& aRect) const { return false; }
  bool operator!=(const Sub& aRect) const { return false; }
};

}
}

#endif /* MOZILLA_GFX_BASERECT_H_ */
