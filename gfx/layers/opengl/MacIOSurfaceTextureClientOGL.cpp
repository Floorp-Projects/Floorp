/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacIOSurfaceTextureClientOGL.h"
#include "mozilla/gfx/MacIOSurface.h" 
#include "MacIOSurfaceHelpers.h"
#include "gfxPlatform.h"

namespace mozilla {
namespace layers {

using namespace gfx;

MacIOSurfaceTextureData::MacIOSurfaceTextureData(MacIOSurface* aSurface,
                                                 BackendType aBackend)
  : mSurface(aSurface)
  , mBackend(aBackend)
{
  MOZ_ASSERT(mSurface);
}

MacIOSurfaceTextureData::~MacIOSurfaceTextureData()
{}

// static
MacIOSurfaceTextureData*
MacIOSurfaceTextureData::Create(MacIOSurface* aSurface, BackendType aBackend)
{
  MOZ_ASSERT(aSurface);
  if (!aSurface) {
    return nullptr;
  }
  return new MacIOSurfaceTextureData(aSurface, aBackend);
}

MacIOSurfaceTextureData*
MacIOSurfaceTextureData::Create(const IntSize& aSize,
                                SurfaceFormat aFormat,
                                BackendType aBackend)
{
  if (aFormat != SurfaceFormat::B8G8R8A8 &&
      aFormat != SurfaceFormat::B8G8R8X8) {
    return nullptr;
  }

  RefPtr<MacIOSurface> surf = MacIOSurface::CreateIOSurface(aSize.width, aSize.height,
                                                            1.0,
                                                            aFormat == SurfaceFormat::B8G8R8A8);
  if (!surf) {
    return nullptr;
  }

  return Create(surf, aBackend);
}

bool
MacIOSurfaceTextureData::Serialize(SurfaceDescriptor& aOutDescriptor)
{
  aOutDescriptor = SurfaceDescriptorMacIOSurface(mSurface->GetIOSurfaceID(),
                                                 mSurface->GetContentsScaleFactor(),
                                                 !mSurface->HasAlpha());
  return true;
}

void
MacIOSurfaceTextureData::FillInfo(TextureData::Info& aInfo) const
{
  aInfo.size = gfx::IntSize(mSurface->GetDevicePixelWidth(), mSurface->GetDevicePixelHeight());
  aInfo.format = mSurface->HasAlpha() ? SurfaceFormat::B8G8R8A8 : SurfaceFormat::B8G8R8X8;
  aInfo.hasIntermediateBuffer = false;
  aInfo.hasSynchronization = false;
  aInfo.supportsMoz2D = true;
  aInfo.canExposeMappedData = false;
}

bool
MacIOSurfaceTextureData::Lock(OpenMode, FenceHandle*)
{
  mSurface->Lock(false);
  return true;
}

void
MacIOSurfaceTextureData::Unlock()
{
  mSurface->Unlock(false);
}

already_AddRefed<DataSourceSurface>
MacIOSurfaceTextureData::GetAsSurface()
{
  RefPtr<SourceSurface> surf = CreateSourceSurfaceFromMacIOSurface(mSurface);
  return surf->GetDataSurface();
}

already_AddRefed<DrawTarget>
MacIOSurfaceTextureData::BorrowDrawTarget()
{
  MOZ_ASSERT(mBackend != BackendType::NONE);
  if (mBackend == BackendType::NONE) {
    // shouldn't happen, but degrade gracefully
    return nullptr;
  }
  return Factory::CreateDrawTargetForData(
      mBackend,
      (unsigned char*)mSurface->GetBaseAddress(),
      IntSize(mSurface->GetWidth(), mSurface->GetHeight()),
      mSurface->GetBytesPerRow(),
      mSurface->HasAlpha() ? SurfaceFormat::B8G8R8A8 : SurfaceFormat::B8G8R8X8,
      true);
}

void
MacIOSurfaceTextureData::Deallocate(ClientIPCAllocator*)
{
  mSurface = nullptr;
}

void
MacIOSurfaceTextureData::Forget(ClientIPCAllocator*)
{
  mSurface = nullptr;
}

bool
MacIOSurfaceTextureData::UpdateFromSurface(gfx::SourceSurface* aSurface)
{
  RefPtr<DrawTarget> dt = BorrowDrawTarget();
  if (!dt) {
    return false;
  }

  dt->CopySurface(aSurface, IntRect(IntPoint(), aSurface->GetSize()), IntPoint());
  return true;
}

} // namespace layers
} // namespace mozilla
