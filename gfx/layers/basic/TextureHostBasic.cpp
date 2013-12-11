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
CreateTextureHostBasic(uint64_t aID,
                       const SurfaceDescriptor& aDesc,
                       ISurfaceAllocator* aDeallocator,
                       TextureFlags aFlags)
{
  RefPtr<TextureHost> result;
  switch (aDesc.type()) {
#ifdef XP_MACOSX
    case SurfaceDescriptor::TSurfaceDescriptorMacIOSurface: {
      const SurfaceDescriptorMacIOSurface& desc =
        aDesc.get_SurfaceDescriptorMacIOSurface();
      result = new MacIOSurfaceTextureHostBasic(aID, aFlags, desc);
      break;
    }
#endif
    default: {
      result = CreateBackendIndependentTextureHost(aID, aDesc, aDeallocator, aFlags);
      break;
    }
  }

  return result;
}

} // namespace layers
} // namespace gfx
