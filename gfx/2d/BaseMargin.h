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
  BaseMargin(T aTop, T aRight, T aBottom, T aLeft) :
      top(aTop), right(aRight), bottom(aBottom), left(aLeft) {}

  void SizeTo(T aTop, T aRight, T aBottom, T aLeft)
  {
    top = aTop; right = aRight; bottom = aBottom; left = aLeft;
  }

  T LeftRight() const { return left + right; }
  T TopBottom() const { return top + bottom; }

  T& Side(SideT aSide) {
    // This is ugly!
    return *(&top + T(aSide));
  }
  T Side(SideT aSide) const {
    // This is ugly!
    return *(&top + T(aSide));
  }

  void ApplySkipSides(int aSkipSides)
  {
    if (aSkipSides & (1 << mozilla::css::eSideTop)) {
      top = 0;
    }
    if (aSkipSides & (1 << mozilla::css::eSideRight)) {
      right = 0;
    }
    if (aSkipSides & (1 << mozilla::css::eSideBottom)) {
      bottom = 0;
    }
    if (aSkipSides & (1 << mozilla::css::eSideLeft)) {
      left = 0;
    }
  }

  // Overloaded operators. Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator
  bool operator==(const Sub& aMargin) const {
    return top == aMargin.top && right == aMargin.right &&
           bottom == aMargin.bottom && left == aMargin.left;
  }
  bool operator!=(const Sub& aMargin) const {
    return !(*this == aMargin);
  }
  Sub operator+(const Sub& aMargin) const {
    return Sub(top + aMargin.top, right + aMargin.right,
               bottom + aMargin.bottom, left + aMargin.left);
  }
  Sub operator-(const Sub& aMargin) const {
    return Sub(top - aMargin.top, right - aMargin.right,
               bottom - aMargin.bottom, left - aMargin.left);
  }
  Sub& operator+=(const Sub& aMargin) {
    top += aMargin.top;
    right += aMargin.right;
    bottom += aMargin.bottom;
    left += aMargin.left;
    return *static_cast<Sub*>(this);
  }
};

}
}

#endif /* MOZILLA_GFX_BASEMARGIN_H_ */
