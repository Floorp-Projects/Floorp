/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Bas Schouten <bschouten@mozilla.com>
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

#ifndef MOZILLA_GFX_POINT_H_
#define MOZILLA_GFX_POINT_H_

#include "Types.h"
#include "BasePoint.h"
#include "BaseSize.h"

namespace mozilla {
namespace gfx {

struct IntPoint :
  public BasePoint<int32_t, IntPoint> {
  typedef BasePoint<int32_t, IntPoint> Super;

  IntPoint() : Super() {}
  IntPoint(int32_t aX, int32_t aY) : Super(aX, aY) {}
};

struct Point :
  public BasePoint<Float, Point> {
  typedef BasePoint<Float, Point> Super;

  Point() : Super() {}
  Point(Float aX, Float aY) : Super(aX, aY) {}
  Point(const IntPoint& point) : Super(float(point.x), float(point.y)) {}
};

struct IntSize :
  public BaseSize<int32_t, IntSize> {
  typedef BaseSize<int32_t, IntSize> Super;

  IntSize() : Super() {}
  IntSize(int32_t aWidth, int32_t aHeight) : Super(aWidth, aHeight) {}
};

struct Size :
  public BaseSize<Float, Size> {
  typedef BaseSize<Float, Size> Super;

  Size() : Super() {}
  Size(Float aWidth, Float aHeight) : Super(aWidth, aHeight) {}
  explicit Size(const IntSize& size) :
    Super(float(size.width), float(size.height)) {}
};

}
}

#endif /* MOZILLA_GFX_POINT_H_ */
