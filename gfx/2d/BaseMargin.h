/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BASEMARGIN_H_
#define MOZILLA_GFX_BASEMARGIN_H_

#include "Types.h"

namespace mozilla {

/**
 * Sides represents a set of physical sides.
 */
struct Sides final {
  Sides() : mBits(0) {}
  explicit Sides(SideBits aSideBits)
  {
    MOZ_ASSERT((aSideBits & ~eSideBitsAll) == 0, "illegal side bits");
    mBits = aSideBits;
  }
  bool IsEmpty() const { return mBits == 0; }
  bool Top()     const { return mBits & eSideBitsTop; }
  bool Right()   const { return mBits & eSideBitsRight; }
  bool Bottom()  const { return mBits & eSideBitsBottom; }
  bool Left()    const { return mBits & eSideBitsLeft; }
  bool Contains(SideBits aSideBits) const
  {
    MOZ_ASSERT((aSideBits & ~eSideBitsAll) == 0, "illegal side bits");
    return (mBits & aSideBits) == aSideBits;
  }
  Sides operator|(Sides aOther) const
  {
    return Sides(SideBits(mBits | aOther.mBits));
  }
  Sides operator|(SideBits aSideBits) const
  {
    return *this | Sides(aSideBits);
  }
  Sides& operator|=(Sides aOther)
  {
    mBits |= aOther.mBits;
    return *this;
  }
  Sides& operator|=(SideBits aSideBits)
  {
    return *this |= Sides(aSideBits);
  }
  bool operator==(Sides aOther) const
  {
    return mBits == aOther.mBits;
  }
  bool operator!=(Sides aOther) const
  {
    return !(*this == aOther);
  }

private:
  uint8_t mBits;
};

namespace gfx {

/**
 * Do not use this class directly. Subclass it, pass that subclass as the
 * Sub parameter, and only use that subclass.
 */
template <class T, class Sub>
struct BaseMargin {
  typedef mozilla::Side SideT; // because we have a method named Side

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
    return *(&top + int(aSide));
  }
  T Side(SideT aSide) const {
    // This is ugly!
    return *(&top + int(aSide));
  }

  void ApplySkipSides(Sides aSkipSides)
  {
    if (aSkipSides.Top()) {
      top = 0;
    }
    if (aSkipSides.Right()) {
      right = 0;
    }
    if (aSkipSides.Bottom()) {
      bottom = 0;
    }
    if (aSkipSides.Left()) {
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
