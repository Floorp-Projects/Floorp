/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureHostBasic.h"
#include "MacIOSurfaceTextureHostBasic.h"

using namespace mozilla::gl;
using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

TemporaryRef<TextureHost>
CreateTextureHostBasic(const SurfaceDescriptor& aDesc,
                       ISurfaceAllocator* aDeallocator,
                       TextureFlags aFlags)
{
#ifdef XP_MACOSX
  if (aDesc.type() == SurfaceDescriptor::TSurfaceDescriptorMacIOSurface) {
    const SurfaceDescriptorMacIOSurface& desc =
      aDesc.get_SurfaceDescriptorMacIOSurface();
    RefPtr<TextureHost> result = new MacIOSurfaceTextureHostBasic(aFlags, desc);
    return result;
  }
#endif
  return CreateBackendIndependentTextureHost(aDesc, aDeallocator, aFlags);
}

} // namespace layers
} // namespace gfx
