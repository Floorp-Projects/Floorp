/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_MACIOSURFACEHELPERS_H
#define GFX_MACIOSURFACEHELPERS_H

class MacIOSurface;
template<class T> struct already_AddRefed;

namespace mozilla {

namespace gfx {
class SourceSurface;
}

namespace layers {

// Unlike MacIOSurface::GetAsSurface, this also handles IOSurface formats
// with multiple planes and does YCbCr to RGB conversion, if necessary.
already_AddRefed<gfx::SourceSurface>
CreateSourceSurfaceFromMacIOSurface(MacIOSurface* aSurface);

} // namespace layers
} // namespace mozilla

#endif // GFX_MACIOSURFACEHELPERS_H
