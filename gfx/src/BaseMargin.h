/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2011
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

#ifndef MOZILLA_BASEMARGIN_H_
#define MOZILLA_BASEMARGIN_H_

#include "gfxCore.h"

namespace mozilla {

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
    NS_PRECONDITION(aSide <= NS_SIDE_LEFT, "Out of range side");
    // This is ugly!
    return *(&top + aSide);
  }
  T Side(SideT aSide) const {
    NS_PRECONDITION(aSide <= NS_SIDE_LEFT, "Out of range side");
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

#endif /* MOZILLA_BASEMARGIN_H_ */
