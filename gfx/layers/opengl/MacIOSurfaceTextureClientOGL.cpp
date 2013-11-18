/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacIOSurfaceTextureClientOGL.h"
#include "mozilla/gfx/MacIOSurface.h"

namespace mozilla {
namespace layers {

MacIOSurfaceTextureClientOGL::MacIOSurfaceTextureClientOGL(TextureFlags aFlags)
  : TextureClient(aFlags)
{}

MacIOSurfaceTextureClientOGL::~MacIOSurfaceTextureClientOGL()
{}

void
MacIOSurfaceTextureClientOGL::InitWith(MacIOSurface* aSurface)
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(!IsAllocated());
  mSurface = aSurface;
}

bool
MacIOSurfaceTextureClientOGL::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(IsValid());
  if (!IsAllocated()) {
    return false;
  }
  aOutDescriptor = SurfaceDescriptorMacIOSurface(mSurface->GetIOSurfaceID(),
                                                 mSurface->GetContentsScaleFactor(),
                                                 mSurface->HasAlpha());
  return true;
}

gfx::IntSize
MacIOSurfaceTextureClientOGL::GetSize() const
{
  return gfx::IntSize(mSurface->GetDevicePixelWidth(), mSurface->GetDevicePixelHeight());
}

TextureClientData*
MacIOSurfaceTextureClientOGL::DropTextureData()
{
  // MacIOSurface has proper cross-process refcounting so we can just drop
  // our reference now, and the data will stay alive (at least) until the host
  // has also been torn down.
  mSurface = nullptr;
  MarkInvalid();
  return nullptr;
}

}
}
