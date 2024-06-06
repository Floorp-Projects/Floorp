/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_MACIOSURFACEHELPERS_H
#define GFX_MACIOSURFACEHELPERS_H

#include "ImageContainer.h"

class MacIOSurface;
template <class T>
struct already_AddRefed;

namespace mozilla {

namespace gfx {
class SourceSurface;
}

namespace layers {

// Unlike MacIOSurface::GetAsSurface, this also handles IOSurface formats
// with multiple planes and does YCbCr to RGB conversion, if necessary.
already_AddRefed<gfx::SourceSurface> CreateSourceSurfaceFromMacIOSurface(
    MacIOSurface* aSurface);

nsresult CreateSurfaceDescriptorBufferFromMacIOSurface(
    MacIOSurface* aSurface, SurfaceDescriptorBuffer& aSdBuffer,
    Image::BuildSdbFlags aFlags,
    const std::function<MemoryOrShmem(uint32_t)>& aAllocate);

}  // namespace layers
}  // namespace mozilla

#endif  // GFX_MACIOSURFACEHELPERS_H
