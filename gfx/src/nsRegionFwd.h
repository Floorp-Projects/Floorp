/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsRegionFwd_h__
#define nsRegionFwd_h__

// Forward declare enough things to define the typedef |nsIntRegion|.

namespace mozilla {
namespace gfx {

struct UnknownUnits;

template <class units>
class IntRegionTyped;

typedef IntRegionTyped<UnknownUnits> IntRegion;

} // namespace gfx
} // namespace mozilla

typedef mozilla::gfx::IntRegion nsIntRegion;

#endif
