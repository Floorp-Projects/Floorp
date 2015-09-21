/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacIOSurfaceTextureClientOGL.h"
#include "mozilla/gfx/MacIOSurface.h"

namespace mozilla {
namespace layers {

MacIOSurfaceTextureClientOGL::MacIOSurfaceTextureClientOGL(ISurfaceAllocator* aAllcator,
                                                           TextureFlags aFlags)
  : TextureClient(aAllcator, aFlags)
  , mIsLocked(false)
{}

MacIOSurfaceTextureClientOGL::~MacIOSurfaceTextureClientOGL()
{
}

void
MacIOSurfaceTextureClientOGL::FinalizeOnIPDLThread()
{
  if (mActor && mSurface) {
    KeepUntilFullDeallocation(MakeUnique<TKeepAlive<MacIOSurface>>(mSurface));
  }
}

// static
already_AddRefed<MacIOSurfaceTextureClientOGL>
MacIOSurfaceTextureClientOGL::Create(ISurfaceAllocator* aAllocator,
                                     TextureFlags aFlags,
                                     MacIOSurface* aSurface)
{
  RefPtr<MacIOSurfaceTextureClientOGL> texture =
      new MacIOSurfaceTextureClientOGL(aAllocator, aFlags);
  MOZ_ASSERT(texture->IsValid());
  MOZ_ASSERT(!texture->IsAllocated());
  texture->mSurface = aSurface;
  return texture.forget();
}

bool
MacIOSurfaceTextureClientOGL::Lock(OpenMode aMode)
{
  MOZ_ASSERT(!mIsLocked);
  mIsLocked = true;
  return IsValid() && IsAllocated();
}

void
MacIOSurfaceTextureClientOGL::Unlock()
{
  MOZ_ASSERT(mIsLocked);
  mIsLocked = false;
}

bool
MacIOSurfaceTextureClientOGL::IsLocked() const
{
  return mIsLocked;
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
                                                 !mSurface->HasAlpha());
  return true;
}

gfx::IntSize
MacIOSurfaceTextureClientOGL::GetSize() const
{
  return gfx::IntSize(mSurface->GetDevicePixelWidth(), mSurface->GetDevicePixelHeight());
}

already_AddRefed<gfx::DataSourceSurface>
MacIOSurfaceTextureClientOGL::GetAsSurface()
{
  RefPtr<gfx::SourceSurface> surf = mSurface->GetAsSurface();
  return surf->GetDataSurface();
}

} // namespace layers
} // namespace mozilla
