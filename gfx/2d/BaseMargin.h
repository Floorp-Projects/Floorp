/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BASEMARGIN_H_
#define MOZILLA_GFX_BASEMARGIN_H_

#include <ostream>

#include "Types.h"

namespace mozilla {

/**
 * Sides represents a set of physical sides.
 */
struct Sides final {
  Sides() : mBits(SideBits::eNone) {}
  explicit Sides(SideBits aSideBits) {
    MOZ_ASSERT((aSideBits & ~SideBits::eAll) == SideBits::eNone,
               "illegal side bits");
    mBits = aSideBits;
  }
  bool IsEmpty() const { return mBits == SideBits::eNone; }
  bool Top() const { return (mBits & SideBits::eTop) == SideBits::eTop; }
  bool Right() const { return (mBits & SideBits::eRight) == SideBits::eRight; }
  bool Bottom() const {
    return (mBits & SideBits::eBottom) == SideBits::eBottom;
  }
  bool Left() const { return (mBits & SideBits::eLeft) == SideBits::eLeft; }
  bool Contains(SideBits aSideBits) const {
    MOZ_ASSERT(!(aSideBits & ~SideBits::eAll), "illegal side bits");
    return (mBits & aSideBits) == aSideBits;
  }
  Sides operator|(Sides aOther) const {
    return Sides(SideBits(mBits | aOther.mBits));
  }
  Sides operator|(SideBits aSideBits) const { return *this | Sides(aSideBits); }
  Sides& operator|=(Sides aOther) {
    mBits |= aOther.mBits;
    return *this;
  }
  Sides& operator|=(SideBits aSideBits) { return *this |= Sides(aSideBits); }
  bool operator==(Sides aOther) const { return mBits == aOther.mBits; }
  bool operator!=(Sides aOther) const { return !(*this == aOther); }

 private:
  SideBits mBits;
};

namespace gfx {

/**
 * Do not use this class directly. Subclass it, pass that subclass as the
 * Sub parameter, and only use that subclass.
 */
template <class T, class Sub, class Coord = T>
struct BaseMargin {
  typedef mozilla::Side SideT;  // because we have a method named Side

  // Do not change the layout of these members; the Side() methods below
  // depend on this order.
  Coord top, right, bottom, left;

  // Constructors
  BaseMargin() : top(0), right(0), bottom(0), left(0) {}
  BaseMargin(Coord aTop, Coord aRight, Coord aBottom, Coord aLeft)
      : top(aTop), right(aRight), bottom(aBottom), left(aLeft) {}

  void SizeTo(Coord aTop, Coord aRight, Coord aBottom, Coord aLeft) {
    top = aTop;
    right = aRight;
    bottom = aBottom;
    left = aLeft;
  }

  Coord LeftRight() const { return left + right; }
  Coord TopBottom() const { return top + bottom; }

  Coord& Side(SideT aSide) {
    // This is ugly!
    return *(&top + int(aSide));
  }
  Coord Side(SideT aSide) const {
    // This is ugly!
    return *(&top + int(aSide));
  }

  Sub& ApplySkipSides(Sides aSkipSides) {
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
    return *static_cast<Sub*>(this);
  }

  // Ensures that all our sides are at least as big as the argument.
  void EnsureAtLeast(const BaseMargin& aMargin) {
    top = std::max(top, aMargin.top);
    right = std::max(right, aMargin.right);
    bottom = std::max(bottom, aMargin.bottom);
    left = std::max(left, aMargin.left);
  }

  // Ensures that all our sides are at most as big as the argument.
  void EnsureAtMost(const BaseMargin& aMargin) {
    top = std::min(top, aMargin.top);
    right = std::min(right, aMargin.right);
    bottom = std::min(bottom, aMargin.bottom);
    left = std::min(left, aMargin.left);
  }

  // Overloaded operators. Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator
  bool operator==(const Sub& aMargin) const {
    return top == aMargin.top && right == aMargin.right &&
           bottom == aMargin.bottom && left == aMargin.left;
  }
  bool operator!=(const Sub& aMargin) const { return !(*this == aMargin); }
  Sub operator+(const Sub& aMargin) const {
    return Sub(top + aMargin.top, right + aMargin.right,
               bottom + aMargin.bottom, left + aMargin.left);
  }
  Sub operator-(const Sub& aMargin) const {
    return Sub(top - aMargin.top, right - aMargin.right,
               bottom - aMargin.bottom, left - aMargin.left);
  }
  Sub operator-() const { return Sub(-top, -right, -bottom, -left); }
  Sub& operator+=(const Sub& aMargin) {
    top += aMargin.top;
    right += aMargin.right;
    bottom += aMargin.bottom;
    left += aMargin.left;
    return *static_cast<Sub*>(this);
  }
  Sub& operator-=(const Sub& aMargin) {
    top -= aMargin.top;
    right -= aMargin.right;
    bottom -= aMargin.bottom;
    left -= aMargin.left;
    return *static_cast<Sub*>(this);
  }

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const BaseMargin& aMargin) {
    return aStream << "(t=" << aMargin.top << ", r=" << aMargin.right
                   << ", b=" << aMargin.bottom << ", l=" << aMargin.left << ')';
  }
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_BASEMARGIN_H_ */
