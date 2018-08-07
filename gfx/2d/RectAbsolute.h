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
#include "Rect.h"
#include "Types.h"

namespace mozilla {

template <typename> struct IsPixel;

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
template <class T, class Sub, class Rect>
struct BaseRectAbsolute {
protected:
  T left, top, right, bottom;

public:
  BaseRectAbsolute() : left(0), top(0), right(0), bottom(0) {}
  BaseRectAbsolute(T aLeft, T aTop, T aRight, T aBottom) :
    left(aLeft), top(aTop), right(aRight), bottom(aBottom) {}

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

  MOZ_ALWAYS_INLINE void SetBox(T aLeft, T aTop, T aRight, T aBottom)
  {
    left = aLeft; top = aTop; right = aRight; bottom = aBottom;
  }
  void SetLeftEdge(T aLeft)
  {
    left = aLeft;
  }
  void SetRightEdge(T aRight)
  {
    right = aRight;
  }
  void SetTopEdge(T aTop)
  {
    top = aTop;
  }
  void SetBottomEdge(T aBottom)
  {
    bottom = aBottom;
  }

  static Sub FromRect(const Rect& aRect)
  {
    MOZ_ASSERT(!aRect.Overflows());
    return Sub(aRect.x, aRect.y, aRect.XMost(), aRect.YMost());
  }

  MOZ_MUST_USE Sub Intersect(const Sub& aOther) const
  {
    Sub result;
    result.left = std::max<T>(left, aOther.left);
    result.top = std::max<T>(top, aOther.top);
    result.right = std::min<T>(right, aOther.right);
    result.bottom = std::min<T>(bottom, aOther.bottom);
    return result;
  }

  bool IsEqualEdges(const Sub& aOther) const
  {
    return left == aOther.left && top == aOther.top &&
           right == aOther.right && bottom == aOther.bottom;
  }
};

template <class Units>
struct IntRectAbsoluteTyped :
    public BaseRectAbsolute<int32_t, IntRectAbsoluteTyped<Units>, IntRectTyped<Units>>,
    public Units {
  static_assert(IsPixel<Units>::value,
                "'units' must be a coordinate system tag");
  typedef BaseRectAbsolute<int32_t, IntRectAbsoluteTyped<Units>, IntRectTyped<Units>> Super;
  typedef IntParam<int32_t> ToInt;

  IntRectAbsoluteTyped() : Super() {}
  IntRectAbsoluteTyped(ToInt aLeft, ToInt aTop, ToInt aRight, ToInt aBottom) :
      Super(aLeft.value, aTop.value, aRight.value, aBottom.value) {}
};

template <class Units>
struct RectAbsoluteTyped :
    public BaseRectAbsolute<Float, RectAbsoluteTyped<Units>, RectTyped<Units>>,
    public Units {
  static_assert(IsPixel<Units>::value,
                "'units' must be a coordinate system tag");
  typedef BaseRectAbsolute<Float, RectAbsoluteTyped<Units>, RectTyped<Units>> Super;

  RectAbsoluteTyped() : Super() {}
  RectAbsoluteTyped(Float aLeft, Float aTop, Float aRight, Float aBottom) :
      Super(aLeft, aTop, aRight, aBottom) {}
};

typedef IntRectAbsoluteTyped<UnknownUnits> IntRectAbsolute;

}
}

#endif /* MOZILLA_GFX_RECT_ABSOLUTE_H_ */
