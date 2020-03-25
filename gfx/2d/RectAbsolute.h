/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RECT_ABSOLUTE_H_
#define MOZILLA_GFX_RECT_ABSOLUTE_H_

#include <algorithm>
#include <cstdint>

#include "mozilla/Attributes.h"
#include "Point.h"
#include "Rect.h"
#include "Types.h"

namespace mozilla {

template <typename>
struct IsPixel;

namespace gfx {

/**
 * A RectAbsolute is similar to a Rect (see BaseRect.h), but represented as
 * (x1, y1, x2, y2) instead of (x, y, width, height).
 *
 * Unless otherwise indicated, methods on this class correspond
 * to methods on BaseRect.
 *
 * The API is currently very bare-bones; it may be extended as needed.
 *
 * Do not use this class directly. Subclass it, pass that subclass as the
 * Sub parameter, and only use that subclass.
 */
template <class T, class Sub, class Point, class Rect>
struct BaseRectAbsolute {
 protected:
  T left, top, right, bottom;

 public:
  BaseRectAbsolute() : left(0), top(0), right(0), bottom(0) {}
  BaseRectAbsolute(T aLeft, T aTop, T aRight, T aBottom)
      : left(aLeft), top(aTop), right(aRight), bottom(aBottom) {}

  MOZ_ALWAYS_INLINE T X() const { return left; }
  MOZ_ALWAYS_INLINE T Y() const { return top; }
  MOZ_ALWAYS_INLINE T Width() const { return right - left; }
  MOZ_ALWAYS_INLINE T Height() const { return bottom - top; }
  MOZ_ALWAYS_INLINE T XMost() const { return right; }
  MOZ_ALWAYS_INLINE T YMost() const { return bottom; }
  MOZ_ALWAYS_INLINE const T& Left() const { return left; }
  MOZ_ALWAYS_INLINE const T& Right() const { return right; }
  MOZ_ALWAYS_INLINE const T& Top() const { return top; }
  MOZ_ALWAYS_INLINE const T& Bottom() const { return bottom; }
  MOZ_ALWAYS_INLINE T& Left() { return left; }
  MOZ_ALWAYS_INLINE T& Right() { return right; }
  MOZ_ALWAYS_INLINE T& Top() { return top; }
  MOZ_ALWAYS_INLINE T& Bottom() { return bottom; }
  T Area() const { return Width() * Height(); }

  void Inflate(T aD) { Inflate(aD, aD); }
  void Inflate(T aDx, T aDy) {
    left -= aDx;
    top -= aDy;
    right += aDx;
    bottom += aDy;
  }

  MOZ_ALWAYS_INLINE void SetBox(T aLeft, T aTop, T aRight, T aBottom) {
    left = aLeft;
    top = aTop;
    right = aRight;
    bottom = aBottom;
  }
  void SetLeftEdge(T aLeft) { left = aLeft; }
  void SetRightEdge(T aRight) { right = aRight; }
  void SetTopEdge(T aTop) { top = aTop; }
  void SetBottomEdge(T aBottom) { bottom = aBottom; }

  static Sub FromRect(const Rect& aRect) {
    if (aRect.Overflows()) {
      return Sub();
    }
    return Sub(aRect.x, aRect.y, aRect.XMost(), aRect.YMost());
  }

  [[nodiscard]] Sub Intersect(const Sub& aOther) const {
    Sub result;
    result.left = std::max<T>(left, aOther.left);
    result.top = std::max<T>(top, aOther.top);
    result.right = std::min<T>(right, aOther.right);
    result.bottom = std::min<T>(bottom, aOther.bottom);
    if (result.right < result.left || result.bottom < result.top) {
      result.SizeTo(0, 0);
    }
    return result;
  }

  bool IsEmpty() const { return right <= left || bottom <= top; }

  bool IsEqualEdges(const Sub& aOther) const {
    return left == aOther.left && top == aOther.top && right == aOther.right &&
           bottom == aOther.bottom;
  }

  bool IsEqualInterior(const Sub& aRect) const {
    return IsEqualEdges(aRect) || (IsEmpty() && aRect.IsEmpty());
  }

  MOZ_ALWAYS_INLINE void MoveBy(T aDx, T aDy) {
    left += aDx;
    right += aDx;
    top += aDy;
    bottom += aDy;
  }
  MOZ_ALWAYS_INLINE void MoveBy(const Point& aPoint) {
    left += aPoint.x;
    right += aPoint.x;
    top += aPoint.y;
    bottom += aPoint.y;
  }
  MOZ_ALWAYS_INLINE void SizeTo(T aWidth, T aHeight) {
    right = left + aWidth;
    bottom = top + aHeight;
  }

  bool Contains(const Sub& aRect) const {
    return aRect.IsEmpty() || (left <= aRect.left && aRect.right <= right &&
                               top <= aRect.top && aRect.bottom <= bottom);
  }
  bool Contains(T aX, T aY) const {
    return (left <= aX && aX < right && top <= aY && aY < bottom);
  }

  bool Intersects(const Sub& aRect) const {
    return !IsEmpty() && !aRect.IsEmpty() && left < aRect.right &&
           aRect.left < right && top < aRect.bottom && aRect.top < bottom;
  }

  void SetEmpty() { left = right = top = bottom = 0; }

  // Returns the smallest rectangle that contains both the area of both
  // this and aRect2.
  // Thus, empty input rectangles are ignored.
  // If both rectangles are empty, returns this.
  // WARNING! This is not safe against overflow, prefer using SafeUnion instead
  // when dealing with int-based rects.
  [[nodiscard]] Sub Union(const Sub& aRect) const {
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
  [[nodiscard]] Sub UnionEdges(const Sub& aRect) const {
    Sub result;
    result.left = std::min(left, aRect.left);
    result.top = std::min(top, aRect.top);
    result.right = std::max(XMost(), aRect.XMost());
    result.bottom = std::max(YMost(), aRect.YMost());
    return result;
  }

  // Scale 'this' by aScale without doing any rounding.
  void Scale(T aScale) { Scale(aScale, aScale); }
  // Scale 'this' by aXScale and aYScale, without doing any rounding.
  void Scale(T aXScale, T aYScale) {
    right = XMost() * aXScale;
    bottom = YMost() * aYScale;
    left = left * aXScale;
    top = top * aYScale;
  }
  // Scale 'this' by aScale, converting coordinates to integers so that the
  // result is the smallest integer-coordinate rectangle containing the
  // unrounded result. Note: this can turn an empty rectangle into a non-empty
  // rectangle
  void ScaleRoundOut(double aScale) { ScaleRoundOut(aScale, aScale); }
  // Scale 'this' by aXScale and aYScale, converting coordinates to integers so
  // that the result is the smallest integer-coordinate rectangle containing the
  // unrounded result.
  // Note: this can turn an empty rectangle into a non-empty rectangle
  void ScaleRoundOut(double aXScale, double aYScale) {
    right = static_cast<T>(ceil(double(XMost()) * aXScale));
    bottom = static_cast<T>(ceil(double(YMost()) * aYScale));
    left = static_cast<T>(floor(double(left) * aXScale));
    top = static_cast<T>(floor(double(top) * aYScale));
  }
  // Scale 'this' by aScale, converting coordinates to integers so that the
  // result is the largest integer-coordinate rectangle contained by the
  // unrounded result.
  void ScaleRoundIn(double aScale) { ScaleRoundIn(aScale, aScale); }
  // Scale 'this' by aXScale and aYScale, converting coordinates to integers so
  // that the result is the largest integer-coordinate rectangle contained by
  // the unrounded result.
  void ScaleRoundIn(double aXScale, double aYScale) {
    right = static_cast<T>(floor(double(XMost()) * aXScale));
    bottom = static_cast<T>(floor(double(YMost()) * aYScale));
    left = static_cast<T>(ceil(double(left) * aXScale));
    top = static_cast<T>(ceil(double(top) * aYScale));
  }
  // Scale 'this' by 1/aScale, converting coordinates to integers so that the
  // result is the smallest integer-coordinate rectangle containing the
  // unrounded result. Note: this can turn an empty rectangle into a non-empty
  // rectangle
  void ScaleInverseRoundOut(double aScale) {
    ScaleInverseRoundOut(aScale, aScale);
  }
  // Scale 'this' by 1/aXScale and 1/aYScale, converting coordinates to integers
  // so that the result is the smallest integer-coordinate rectangle containing
  // the unrounded result. Note: this can turn an empty rectangle into a
  // non-empty rectangle
  void ScaleInverseRoundOut(double aXScale, double aYScale) {
    right = static_cast<T>(ceil(double(XMost()) / aXScale));
    bottom = static_cast<T>(ceil(double(YMost()) / aYScale));
    left = static_cast<T>(floor(double(left) / aXScale));
    top = static_cast<T>(floor(double(top) / aYScale));
  }
  // Scale 'this' by 1/aScale, converting coordinates to integers so that the
  // result is the largest integer-coordinate rectangle contained by the
  // unrounded result.
  void ScaleInverseRoundIn(double aScale) {
    ScaleInverseRoundIn(aScale, aScale);
  }
  // Scale 'this' by 1/aXScale and 1/aYScale, converting coordinates to integers
  // so that the result is the largest integer-coordinate rectangle contained by
  // the unrounded result.
  void ScaleInverseRoundIn(double aXScale, double aYScale) {
    right = static_cast<T>(floor(double(XMost()) / aXScale));
    bottom = static_cast<T>(floor(double(YMost()) / aYScale));
    left = static_cast<T>(ceil(double(left) / aXScale));
    top = static_cast<T>(ceil(double(top) / aYScale));
  }

  /**
   * Translate this rectangle to be inside aRect. If it doesn't fit inside
   * aRect then the dimensions that don't fit will be shrunk so that they
   * do fit. The resulting rect is returned.
   */
  [[nodiscard]] Sub MoveInsideAndClamp(const Sub& aRect) const {
    T newLeft = std::max(aRect.left, left);
    T newTop = std::max(aRect.top, top);
    T width = std::min(aRect.Width(), Width());
    T height = std::min(aRect.Height(), Height());
    Sub rect(newLeft, newTop, newLeft + width, newTop + height);
    newLeft = std::min(rect.right, aRect.right) - width;
    newTop = std::min(rect.bottom, aRect.bottom) - height;
    rect.MoveBy(newLeft - rect.left, newTop - rect.top);
    return rect;
  }

  friend std::ostream& operator<<(
      std::ostream& stream,
      const BaseRectAbsolute<T, Sub, Point, Rect>& aRect) {
    return stream << '(' << aRect.left << ',' << aRect.top << ',' << aRect.right
                  << ',' << aRect.bottom << ')';
  }
};

template <class Units>
struct IntRectAbsoluteTyped
    : public BaseRectAbsolute<int32_t, IntRectAbsoluteTyped<Units>,
                              IntPointTyped<Units>, IntRectTyped<Units>>,
      public Units {
  static_assert(IsPixel<Units>::value,
                "'units' must be a coordinate system tag");
  typedef BaseRectAbsolute<int32_t, IntRectAbsoluteTyped<Units>,
                           IntPointTyped<Units>, IntRectTyped<Units>>
      Super;
  typedef IntParam<int32_t> ToInt;

  IntRectAbsoluteTyped() : Super() {}
  IntRectAbsoluteTyped(ToInt aLeft, ToInt aTop, ToInt aRight, ToInt aBottom)
      : Super(aLeft.value, aTop.value, aRight.value, aBottom.value) {}
};

template <class Units>
struct RectAbsoluteTyped
    : public BaseRectAbsolute<Float, RectAbsoluteTyped<Units>,
                              PointTyped<Units>, RectTyped<Units>>,
      public Units {
  static_assert(IsPixel<Units>::value,
                "'units' must be a coordinate system tag");
  typedef BaseRectAbsolute<Float, RectAbsoluteTyped<Units>, PointTyped<Units>,
                           RectTyped<Units>>
      Super;

  RectAbsoluteTyped() : Super() {}
  RectAbsoluteTyped(Float aLeft, Float aTop, Float aRight, Float aBottom)
      : Super(aLeft, aTop, aRight, aBottom) {}
};

typedef IntRectAbsoluteTyped<UnknownUnits> IntRectAbsolute;

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_RECT_ABSOLUTE_H_ */
