/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RECT_H_
#define MOZILLA_GFX_RECT_H_

#include "BaseRect.h"
#include "BaseMargin.h"
#include "Point.h"

namespace mozilla {
namespace gfx {

struct Margin :
  public BaseMargin<Float, Margin> {
  typedef BaseMargin<Float, Margin> Super;

  // Constructors
  Margin(const Margin& aMargin) : Super(aMargin) {}
  Margin(Float aLeft,  Float aTop, Float aRight, Float aBottom)
    : Super(aLeft, aTop, aRight, aBottom) {}
};

struct IntRect :
    public BaseRect<int32_t, IntRect, IntPoint, IntSize, Margin> {
    typedef BaseRect<int32_t, IntRect, IntPoint, mozilla::gfx::IntSize, Margin> Super;

    IntRect() : Super() {}
    IntRect(IntPoint aPos, mozilla::gfx::IntSize aSize) :
        Super(aPos, aSize) {}
    IntRect(int32_t _x, int32_t _y, int32_t _width, int32_t _height) :
        Super(_x, _y, _width, _height) {}

    // Rounding isn't meaningful on an integer rectangle.
    void Round() {}
    void RoundIn() {}
    void RoundOut() {}
};

struct Rect :
    public BaseRect<Float, Rect, Point, Size, Margin> {
    typedef BaseRect<Float, Rect, Point, mozilla::gfx::Size, Margin> Super;

    Rect() : Super() {}
    Rect(Point aPos, mozilla::gfx::Size aSize) :
        Super(aPos, aSize) {}
    Rect(Float _x, Float _y, Float _width, Float _height) :
        Super(_x, _y, _width, _height) {}
    explicit Rect(const IntRect& rect) :
        Super(float(rect.x), float(rect.y),
              float(rect.width), float(rect.height)) {}

    bool ToIntRect(IntRect *aOut)
    {
      *aOut = IntRect(int32_t(X()), int32_t(Y()),
                    int32_t(Width()), int32_t(Height()));
      return Rect(Float(aOut->x), Float(aOut->y), 
                  Float(aOut->width), Float(aOut->height)).IsEqualEdges(*this);
    }
};

}
}

#endif /* MOZILLA_GFX_RECT_H_ */
