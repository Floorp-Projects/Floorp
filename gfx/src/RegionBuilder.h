/* This Source Code Form is subject to the terms of the Mozilla Public
 *  * License, v. 2.0. If a copy of the MPL was not distributed with this
 *  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RegionBuilder_h__
#define RegionBuilder_h__

#include <nsTArray.h>

template <typename RegionType>
class RegionBuilder
{
public:
  typedef typename RegionType::RectType RectType;

  RegionBuilder()
  {}

  void Or(const RectType& aRect) {
    pixman_box32_t box = { aRect.x, aRect.y, aRect.XMost(), aRect.YMost() };
    mRects.AppendElement(box);
  }

  RegionType ToRegion() const {
    return RegionType(mRects);
  }

private:
  nsTArray<pixman_box32_t> mRects;
};

#endif // RegionBuilder_h__
