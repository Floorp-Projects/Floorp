/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacIOSurfaceTextureClientOGL.h"
#include "mozilla/gfx/MacIOSurface.h" 

namespace mozilla {
namespace layers {

MacIOSurfaceTextureData::MacIOSurfaceTextureData(MacIOSurface* aSurface)
: mSurface(aSurface)
{
  MOZ_ASSERT(mSurface);
}

MacIOSurfaceTextureData::~MacIOSurfaceTextureData()
{}

void
MacIOSurfaceTextureData::FinalizeOnIPDLThread(TextureClient* aWrapper)
{
  if (mSurface) {
    aWrapper->KeepUntilFullDeallocation(MakeUnique<TKeepAlive<MacIOSurface>>(mSurface));
  }
}

// static
MacIOSurfaceTextureData*
MacIOSurfaceTextureData::Create(MacIOSurface* aSurface)
{
  MOZ_ASSERT(aSurface);
  if (!aSurface) {
    return nullptr;
  }
  return new MacIOSurfaceTextureData(aSurface);
}

bool
MacIOSurfaceTextureData::Serialize(SurfaceDescriptor& aOutDescriptor)
{
  aOutDescriptor = SurfaceDescriptorMacIOSurface(mSurface->GetIOSurfaceID(),
                                                 mSurface->GetContentsScaleFactor(),
                                                 !mSurface->HasAlpha());
  return true;
}

gfx::IntSize
MacIOSurfaceTextureData::GetSize() const
{
  return gfx::IntSize(mSurface->GetDevicePixelWidth(), mSurface->GetDevicePixelHeight());
}

gfx::SurfaceFormat
MacIOSurfaceTextureData::GetFormat() const
{
  return mSurface->GetFormat();
}

already_AddRefed<gfx::DataSourceSurface>
MacIOSurfaceTextureData::GetAsSurface()
{
  RefPtr<gfx::SourceSurface> surf = mSurface->GetAsSurface();
  return surf->GetDataSurface();
}

} // namespace layers
} // namespace mozilla
