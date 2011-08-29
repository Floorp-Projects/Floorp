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
 *   Matt Woodrow <mwoodrow@mozilla.com>
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

#ifndef MOZILLA_BASEPOINT3D_H_
#define MOZILLA_BASEPOINT3D_H_

namespace mozilla {
namespace gfx {

/**
 * Do not use this class directly. Subclass it, pass that subclass as the
 * Sub parameter, and only use that subclass. This allows methods to safely
 * cast 'this' to 'Sub*'.
 */
template <class T, class Sub>
struct BasePoint3D {
  T x, y, z;

  // Constructors
  BasePoint3D() : x(0), y(0), z(0) {}
  BasePoint3D(T aX, T aY, T aZ) : x(aX), y(aY), z(aZ) {}

  void MoveTo(T aX, T aY, T aZ) { x = aX; y = aY; z = aZ; }
  void MoveBy(T aDx, T aDy, T aDz) { x += aDx; y += aDy; z += aDz; }

  // Note that '=' isn't defined so we'll get the
  // compiler generated default assignment operator

  T& operator[](int aIndex) {
    NS_ABORT_IF_FALSE(aIndex >= 0 && aIndex <= 2, "Invalid array index");
    return *((&x)+aIndex);
  }

  const T& operator[](int aIndex) const {
    NS_ABORT_IF_FALSE(aIndex >= 0 && aIndex <= 2, "Invalid array index");
    return *((&x)+aIndex);
  }

  bool operator==(const Sub& aPoint) const {
    return x == aPoint.x && y == aPoint.y && z == aPoint.z;
  }
  bool operator!=(const Sub& aPoint) const {
    return x != aPoint.x || y != aPoint.y || z != aPoint.z;
  }

  Sub operator+(const Sub& aPoint) const {
    return Sub(x + aPoint.x, y + aPoint.y, z + aPoint.z);
  }
  Sub operator-(const Sub& aPoint) const {
    return Sub(x - aPoint.x, y - aPoint.y, z - aPoint.z);
  }
  Sub& operator+=(const Sub& aPoint) {
    x += aPoint.x;
    y += aPoint.y;
    z += aPoint.z;
    return *static_cast<Sub*>(this);
  }
  Sub& operator-=(const Sub& aPoint) {
    x -= aPoint.x;
    y -= aPoint.y;
    z -= aPoint.z;
    return *static_cast<Sub*>(this);
  }

  Sub operator*(T aScale) const {
    return Sub(x * aScale, y * aScale, z * aScale);
  }
  Sub operator/(T aScale) const {
    return Sub(x / aScale, y / aScale, z / aScale);
  }

  Sub& operator*=(T aScale) {
    x *= aScale;
    y *= aScale;
    z *= aScale;
    return *static_cast<Sub*>(this);
  }

  Sub& operator/=(T aScale) {
      x /= aScale;
      y /= aScale;
      z /= aScale;
      return *static_cast<Sub*>(this);
  }

  Sub operator-() const {
    return Sub(-x, -y, -z);
  }

  Sub CrossProduct(const Sub& aPoint) const {
      return Sub(y * aPoint.z - aPoint.y * z,
                 z * aPoint.x - aPoint.z * x,
                 x * aPoint.y - aPoint.x * y);
  }

  T DotProduct(const Sub& aPoint) const {
      return x * aPoint.x + y * aPoint.y + z * aPoint.z;
  }

  T Length() const {
      return sqrt(x*x + y*y + z*z);
  }

  // Invalid for points with distance from origin of 0.
  void Normalize() {
      *this /= Length();
  }
};

}
}

#endif /* MOZILLA_BASEPOINT3D_H_ */
