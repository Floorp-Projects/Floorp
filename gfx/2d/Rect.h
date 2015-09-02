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

#include <cmath>

namespace mozilla {

template <typename> struct IsPixel;

namespace gfx {

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

    IntRectTyped() : Super() {}
    IntRectTyped(const IntPointTyped<units>& aPos, const IntSizeTyped<units>& aSize) :
        Super(aPos, aSize) {}
    IntRectTyped(int32_t _x, int32_t _y, int32_t _width, int32_t _height) :
        Super(_x, _y, _width, _height) {}

    // Rounding isn't meaningful on an integer rectangle.
    void Round() {}
    void RoundIn() {}
    void RoundOut() {}

    // XXX When all of the code is ported, the following functions to convert to and from
    // unknown types should be removed.

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

    // This is here only to keep IPDL-generated code happy. DO NOT USE.
    bool operator==(const IntRectTyped<units>& aRect) const
    {
      return IntRectTyped<units>::IsEqualEdges(aRect);
    }

    void InflateToMultiple(const IntSizeTyped<units>& aTileSize)
    {
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

    // Returns the largest rectangle that can be represented with 32-bit
    // signed integers, centered around a point at 0,0.  As BaseRect's represent
    // the dimensions as a top-left point with a width and height, the width
    // and height will be the largest positive 32-bit value.  The top-left
    // position coordinate is divided by two to center the rectangle around a
    // point at 0,0.
    static RectTyped<units, F> MaxIntRect()
    {
      return RectTyped<units, F>(
        -std::numeric_limits<int32_t>::max() * 0.5,
        -std::numeric_limits<int32_t>::max() * 0.5,
        std::numeric_limits<int32_t>::max(),
        std::numeric_limits<int32_t>::max()
      );
    };

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
      return RectTyped<units>(F(aOut->x), F(aOut->y),
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
  RectTyped<units> copy(aRect);
  copy.RoundIn();
  return IntRectTyped<units>(int32_t(copy.x),
                             int32_t(copy.y),
                             int32_t(copy.width),
                             int32_t(copy.height));
}

template<class units>
IntRectTyped<units> RoundedOut(const RectTyped<units>& aRect)
{
  RectTyped<units> copy(aRect);
  copy.RoundOut();
  return IntRectTyped<units>(int32_t(copy.x),
                             int32_t(copy.y),
                             int32_t(copy.width),
                             int32_t(copy.height));
}

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_RECT_H_ */
