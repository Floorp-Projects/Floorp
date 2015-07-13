/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RegionTyped_h
#define mozilla_RegionTyped_h

#include "nsRegion.h"
#include "mozilla/gfx/Rect.h"

namespace mozilla {

namespace gfx {

template <class units>
class IntRegionTyped :
    public BaseIntRegion<IntRegionTyped<units>, IntRectTyped<units>, IntPointTyped<units>, IntMarginTyped<units>>
{
  typedef BaseIntRegion<IntRegionTyped<units>, IntRectTyped<units>, IntPointTyped<units>, IntMarginTyped<units>> Super;
public:
  // Forward constructors.
  IntRegionTyped() {}
  MOZ_IMPLICIT IntRegionTyped(const IntRectTyped<units>& aRect) : Super(aRect) {}
  IntRegionTyped(const IntRegionTyped& aRegion) : Super(aRegion) {}
  IntRegionTyped(IntRegionTyped&& aRegion) : Super(mozilla::Move(aRegion)) {}

  // Assignment operators need to be forwarded as well, otherwise the compiler
  // will declare deleted ones.
  IntRegionTyped& operator=(const IntRegionTyped& aRegion)
  {
    return Super::operator=(aRegion);
  }
  IntRegionTyped& operator=(IntRegionTyped&& aRegion)
  {
    return Super::operator=(mozilla::Move(aRegion));
  }

  static IntRegionTyped FromUntyped(const nsIntRegion& aRegion)
  {
    return IntRegionTyped(aRegion.Impl());
  }
private:
  // This is deliberately private, so calling code uses FromUntyped().
  explicit IntRegionTyped(const nsRegion& aRegion) : Super(aRegion) {}
};

} // namespace gfx

} // namespace mozilla



#endif /* mozilla_RegionTyped_h */
