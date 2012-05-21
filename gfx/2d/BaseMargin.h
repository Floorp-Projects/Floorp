/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BASEMARGIN_H_
#define MOZILLA_GFX_BASEMARGIN_H_

#include "Types.h"

namespace mozilla {
namespace gfx {

/**
 * Do not use this class directly. Subclass it, pass that subclass as the
 * Sub parameter, and only use that subclass.
 */
template <class T, class Sub>
struct BaseMargin {
  typedef mozilla::css::Side SideT;

  // Do not change the layout of these members; the Side() methods below
  // depend on this order.
  T top, right, bottom, left;

  // Constructors
  BaseMargin() : top(0), right(0), bottom(0), left(0) {}
  BaseMargin(T aLeft, T aTop, T aRight, T aBottom) :
      top(aTop), right(aRight), bottom(aBottom), left(aLeft) {}

  void SizeTo(T aLeft, T aTop, T aRight, T aBottom)
  {
    left = aLeft; top = aTop; right = aRight; bottom = aBottom;
  }

  T LeftRight() const { return left + right; }
  T TopBottom() const { return top + bottom; }

  T& Side(SideT aSide) {
    // This is ugly!
    return *(&top + aSide);
  }
  T Side(SideT aSide) const {
    // This is ugly!
    return *(&top + aSide);
  }

  // Overloaded operators. Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator
  bool operator==(const Sub& aMargin) const {
    return left == aMargin.left && top == aMargin.top &&
           right == aMargin.right && bottom == aMargin.bottom;
  }
  bool operator!=(const Sub& aMargin) const {
    return !(*this == aMargin);
  }
  Sub operator+(const Sub& aMargin) const {
    return Sub(left + aMargin.left, top + aMargin.top,
             right + aMargin.right, bottom + aMargin.bottom);
  }
  Sub operator-(const Sub& aMargin) const {
    return Sub(left - aMargin.left, top - aMargin.top,
             right - aMargin.right, bottom - aMargin.bottom);
  }
  Sub& operator+=(const Sub& aMargin) {
    left += aMargin.left;
    top += aMargin.top;
    right += aMargin.right;
    bottom += aMargin.bottom;
    return *static_cast<Sub*>(this);
  }
};

}
}

#endif /* MOZILLA_GFX_BASEMARGIN_H_ */
