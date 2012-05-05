/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (Sub) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
      return Rect(aOut->x, aOut->y, aOut->width, aOut->height).IsEqualEdges(*this);
    }
};

}
}

#endif /* MOZILLA_GFX_RECT_H_ */
