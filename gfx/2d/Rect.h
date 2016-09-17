/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RECT_H_
#define MOZILLA_GFX_RECT_H_

#include "BaseRect.h"
#include "BaseMargin.h"
#include "NumericTools.h"
#include "Point.h"
#include "Tools.h"
#include "mozilla/Maybe.h"

#include <cmath>

namespace mozilla {

template <typename> struct IsPixel;

namespace gfx {

template<class units, class F> struct RectTyped;

template<class units>
struct IntMarginTyped:
    public BaseMargin<int32_t, IntMarginTyped<units> >,
    public units {
    static_assert(IsPixel<units>::value,
                  "'units' must be a coordinate system tag");

    typedef BaseMargin<int32_t, IntMarginTyped<units> > Super;

    IntMarginTyped() : Super() {}
    IntMarginTyped(int32_t aTop, int32_t aRight, int32_t aBottom, int32_t aLeft) :
        Super(aTop, aRight, aBottom, aLeft) {}

    // XXX When all of the code is ported, the following functions to convert
    // to and from unknown types should be removed.

    static IntMarginTyped<units> FromUnknownMargin(const IntMarginTyped<UnknownUnits>& aMargin) {
        return IntMarginTyped<units>(aMargin.top, aMargin.right,
                                     aMargin.bottom, aMargin.left);
    }

    IntMarginTyped<UnknownUnits> ToUnknownMargin() const {
        return IntMarginTyped<UnknownUnits>(this->top, this->right,
                                            this->bottom, this->left);
    }
};
typedef IntMarginTyped<UnknownUnits> IntMargin;

template<class units, class F = Float>
struct MarginTyped:
    public BaseMargin<F, MarginTyped<units> >,
    public units {
    static_assert(IsPixel<units>::value,
                  "'units' must be a coordinate system tag");

    typedef BaseMargin<F, MarginTyped<units, F> > Super;

    MarginTyped() : Super() {}
    MarginTyped(F aTop, F aRight, F aBottom, F aLeft) :
        Super(aTop, aRight, aBottom, aLeft) {}
    explicit MarginTyped(const IntMarginTyped<units>& aMargin) :
        Super(F(aMargin.top), F(aMargin.right),
              F(aMargin.bottom), F(aMargin.left)) {}
};
typedef MarginTyped<UnknownUnits> Margin;
typedef MarginTyped<UnknownUnits, double> MarginDouble;

template<class units>
IntMarginTyped<units> RoundedToInt(const MarginTyped<units>& aMargin)
{
  return IntMarginTyped<units>(int32_t(floorf(aMargin.top + 0.5f)),
                               int32_t(floorf(aMargin.right + 0.5f)),
                               int32_t(floorf(aMargin.bottom + 0.5f)),
                               int32_t(floorf(aMargin.left + 0.5f)));
}

template<class units>
struct IntRectTyped :
    public BaseRect<int32_t, IntRectTyped<units>, IntPointTyped<units>, IntSizeTyped<units>, IntMarginTyped<units> >,
    public units {
    static_assert(IsPixel<units>::value,
                  "'units' must be a coordinate system tag");

    typedef BaseRect<int32_t, IntRectTyped<units>, IntPointTyped<units>, IntSizeTyped<units>, IntMarginTyped<units> > Super;
    typedef IntRectTyped<units> Self;
    typedef IntParam<int32_t> ToInt;

    IntRectTyped() : Super() {}
    IntRectTyped(const IntPointTyped<units>& aPos, const IntSizeTyped<units>& aSize) :
        Super(aPos, aSize) {}

    IntRectTyped(ToInt aX, ToInt aY, ToInt aWidth, ToInt aHeight) :
        Super(aX.value, aY.value, aWidth.value, aHeight.value) {}

    static IntRectTyped<units> RoundIn(float aX, float aY, float aW, float aH) {
      return IntRectTyped<units>::RoundIn(RectTyped<units, float>(aX, aY, aW, aH));
    }

    static IntRectTyped<units> RoundOut(float aX, float aY, float aW, float aH) {
      return IntRectTyped<units>::RoundOut(RectTyped<units, float>(aX, aY, aW, aH));
    }

    static IntRectTyped<units> Round(float aX, float aY, float aW, float aH) {
      return IntRectTyped<units>::Round(RectTyped<units, float>(aX, aY, aW, aH));
    }

    static IntRectTyped<units> Truncate(float aX, float aY, float aW, float aH) {
      return IntRectTyped<units>(IntPointTyped<units>::Truncate(aX, aY),
                                 IntSizeTyped<units>::Truncate(aW, aH));
    }

    static IntRectTyped<units> RoundIn(const RectTyped<units, float>& aRect) {
      auto tmp(aRect);
      tmp.RoundIn();
      return IntRectTyped(int32_t(tmp.x), int32_t(tmp.y),
                          int32_t(tmp.width), int32_t(tmp.height));
    }

    static IntRectTyped<units> RoundOut(const RectTyped<units, float>& aRect) {
      auto tmp(aRect);
      tmp.RoundOut();
      return IntRectTyped(int32_t(tmp.x), int32_t(tmp.y),
                          int32_t(tmp.width), int32_t(tmp.height));
    }

    static IntRectTyped<units> Round(const RectTyped<units, float>& aRect) {
      auto tmp(aRect);
      tmp.Round();
      return IntRectTyped(int32_t(tmp.x), int32_t(tmp.y),
                          int32_t(tmp.width), int32_t(tmp.height));
    }

    static IntRectTyped<units> Truncate(const RectTyped<units, float>& aRect) {
      return IntRectTyped::Truncate(aRect.x, aRect.y, aRect.width, aRect.height);
    }

    // Rounding isn't meaningful on an integer rectangle.
    void Round() {}
    void RoundIn() {}
    void RoundOut() {}

    // XXX When all of the code is ported, the following functions to convert
    // to and from unknown types should be removed.

    static IntRectTyped<units> FromUnknownRect(const IntRectTyped<UnknownUnits>& rect) {
        return IntRectTyped<units>(rect.x, rect.y, rect.width, rect.height);
    }

    IntRectTyped<UnknownUnits> ToUnknownRect() const {
        return IntRectTyped<UnknownUnits>(this->x, this->y, this->width, this->height);
    }

    bool Overflows() const {
      CheckedInt<int32_t> xMost = this->x;
      xMost += this->width;
      CheckedInt<int32_t> yMost = this->y;
      yMost += this->height;
      return !xMost.isValid() || !yMost.isValid();
    }

    // Same as Union(), but in the cases where aRect is non-empty, the union is
    // done while guarding against overflow. If an overflow is detected, Nothing
    // is returned.
    MOZ_MUST_USE Maybe<Self> SafeUnion(const Self& aRect) const
    {
      if (this->IsEmpty()) {
        return aRect.Overflows() ? Nothing() : Some(aRect);
      } else if (aRect.IsEmpty()) {
        return Some(*static_cast<const Self*>(this));
      } else {
        return this->SafeUnionEdges(aRect);
      }
    }

    // Same as UnionEdges, but guards against overflow. If an overflow is detected,
    // Nothing is returned.
    MOZ_MUST_USE Maybe<Self> SafeUnionEdges(const Self& aRect) const
    {
      if (this->Overflows() || aRect.Overflows()) {
        return Nothing();
      }
      // If neither |this| nor |aRect| overflow, then their XMost/YMost values
      // should be safe to use.
      CheckedInt<int32_t> newX = std::min(this->x, aRect.x);
      CheckedInt<int32_t> newY = std::min(this->y, aRect.y);
      CheckedInt<int32_t> newXMost = std::max(this->XMost(), aRect.XMost());
      CheckedInt<int32_t> newYMost = std::max(this->YMost(), aRect.YMost());
      CheckedInt<int32_t> newW = newXMost - newX;
      CheckedInt<int32_t> newH = newYMost - newY;
      if (!newW.isValid() || !newH.isValid()) {
        return Nothing();
      }
      return Some(Self(newX.value(), newY.value(), newW.value(), newH.value()));
    }

    // This is here only to keep IPDL-generated code happy. DO NOT USE.
    bool operator==(const IntRectTyped<units>& aRect) const
    {
      return IntRectTyped<units>::IsEqualEdges(aRect);
    }

    void InflateToMultiple(const IntSizeTyped<units>& aTileSize)
    {
      if (this->IsEmpty()) {
        return;
      }

      int32_t yMost = this->YMost();
      int32_t xMost = this->XMost();

      this->x = mozilla::RoundDownToMultiple(this->x, aTileSize.width);
      this->y = mozilla::RoundDownToMultiple(this->y, aTileSize.height);
      xMost = mozilla::RoundUpToMultiple(xMost, aTileSize.width);
      yMost = mozilla::RoundUpToMultiple(yMost, aTileSize.height);

      this->width = xMost - this->x;
      this->height = yMost - this->y;
    }

};
typedef IntRectTyped<UnknownUnits> IntRect;

template<class units, class F = Float>
struct RectTyped :
    public BaseRect<F, RectTyped<units, F>, PointTyped<units, F>, SizeTyped<units, F>, MarginTyped<units, F> >,
    public units {
    static_assert(IsPixel<units>::value,
                  "'units' must be a coordinate system tag");

    typedef BaseRect<F, RectTyped<units, F>, PointTyped<units, F>, SizeTyped<units, F>, MarginTyped<units, F> > Super;

    RectTyped() : Super() {}
    RectTyped(const PointTyped<units, F>& aPos, const SizeTyped<units, F>& aSize) :
        Super(aPos, aSize) {}
    RectTyped(F _x, F _y, F _width, F _height) :
        Super(_x, _y, _width, _height) {}
    explicit RectTyped(const IntRectTyped<units>& rect) :
        Super(F(rect.x), F(rect.y),
              F(rect.width), F(rect.height)) {}

    void NudgeToIntegers()
    {
      NudgeToInteger(&(this->x));
      NudgeToInteger(&(this->y));
      NudgeToInteger(&(this->width));
      NudgeToInteger(&(this->height));
    }

    bool ToIntRect(IntRectTyped<units> *aOut) const
    {
      *aOut = IntRectTyped<units>(int32_t(this->X()), int32_t(this->Y()),
                                  int32_t(this->Width()), int32_t(this->Height()));
      return RectTyped<units, F>(F(aOut->x), F(aOut->y),
                                 F(aOut->width), F(aOut->height))
             .IsEqualEdges(*this);
    }

    // XXX When all of the code is ported, the following functions to convert to and from
    // unknown types should be removed.

    static RectTyped<units, F> FromUnknownRect(const RectTyped<UnknownUnits, F>& rect) {
        return RectTyped<units, F>(rect.x, rect.y, rect.width, rect.height);
    }

    RectTyped<UnknownUnits, F> ToUnknownRect() const {
        return RectTyped<UnknownUnits, F>(this->x, this->y, this->width, this->height);
    }

    // This is here only to keep IPDL-generated code happy. DO NOT USE.
    bool operator==(const RectTyped<units, F>& aRect) const
    {
      return RectTyped<units, F>::IsEqualEdges(aRect);
    }
};
typedef RectTyped<UnknownUnits> Rect;
typedef RectTyped<UnknownUnits, double> RectDouble;

template<class units>
IntRectTyped<units> RoundedToInt(const RectTyped<units>& aRect)
{
  RectTyped<units> copy(aRect);
  copy.Round();
  return IntRectTyped<units>(int32_t(copy.x),
                             int32_t(copy.y),
                             int32_t(copy.width),
                             int32_t(copy.height));
}

template<class units>
IntRectTyped<units> RoundedIn(const RectTyped<units>& aRect)
{
  return IntRectTyped<units>::RoundIn(aRect);
}

template<class units>
IntRectTyped<units> RoundedOut(const RectTyped<units>& aRect)
{
  return IntRectTyped<units>::RoundOut(aRect);
}

template<class units>
IntRectTyped<units> TruncatedToInt(const RectTyped<units>& aRect) {
  return IntRectTyped<units>::Truncate(aRect);
}

template<class units>
RectTyped<units> IntRectToRect(const IntRectTyped<units>& aRect)
{
  return RectTyped<units>(aRect.x, aRect.y, aRect.width, aRect.height);
}

// Convenience function for intersecting two rectangles wrapped in Maybes.
template <typename T>
Maybe<T>
IntersectMaybeRects(const Maybe<T>& a, const Maybe<T>& b)
{
  if (!a) {
    return b;
  } else if (!b) {
    return a;
  } else {
    return Some(a->Intersect(*b));
  }
}

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_RECT_H_ */
