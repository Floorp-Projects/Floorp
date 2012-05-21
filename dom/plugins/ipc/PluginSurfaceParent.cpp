/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/plugins/PluginSurfaceParent.h"
#include "mozilla/gfx/SharedDIBSurface.h"

using mozilla::gfx::SharedDIBSurface;

namespace mozilla {
namespace plugins {

PluginSurfaceParent::PluginSurfaceParent(const WindowsSharedMemoryHandle& handle,
                                         const gfxIntSize& size,
                                         bool transparent)
{
  SharedDIBSurface* dibsurf = new SharedDIBSurface();
  if (dibsurf->Attach(handle, size.width, size.height, transparent))
    mSurface = dibsurf;
}

}
}
