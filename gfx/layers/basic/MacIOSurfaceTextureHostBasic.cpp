/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacIOSurfaceTextureHostBasic.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "MacIOSurfaceHelpers.h"

namespace mozilla {
namespace layers {

MacIOSurfaceTextureSourceBasic::MacIOSurfaceTextureSourceBasic(
    MacIOSurface* aSurface)
    : mSurface(aSurface) {
  MOZ_COUNT_CTOR(MacIOSurfaceTextureSourceBasic);
}

MacIOSurfaceTextureSourceBasic::~MacIOSurfaceTextureSourceBasic() {
  MOZ_COUNT_DTOR(MacIOSurfaceTextureSourceBasic);
}

gfx::IntSize MacIOSurfaceTextureSourceBasic::GetSize() const {
  return gfx::IntSize(mSurface->GetDevicePixelWidth(),
                      mSurface->GetDevicePixelHeight());
}

gfx::SurfaceFormat MacIOSurfaceTextureSourceBasic::GetFormat() const {
  // Set the format the same way as CreateSourceSurfaceFromMacIOSurface.
  return mSurface->GetFormat() == gfx::SurfaceFormat::NV12
             ? gfx::SurfaceFormat::B8G8R8X8
             : gfx::SurfaceFormat::B8G8R8A8;
}

MacIOSurfaceTextureHostBasic::MacIOSurfaceTextureHostBasic(
    TextureFlags aFlags, const SurfaceDescriptorMacIOSurface& aDescriptor)
    : TextureHost(aFlags) {
  mSurface = MacIOSurface::LookupSurface(aDescriptor.surfaceId(),
                                         !aDescriptor.isOpaque(),
                                         aDescriptor.yUVColorSpace());
}

gfx::SourceSurface* MacIOSurfaceTextureSourceBasic::GetSurface(
    gfx::DrawTarget* aTarget) {
  if (!mSourceSurface) {
    mSourceSurface = CreateSourceSurfaceFromMacIOSurface(mSurface);
  }
  return mSourceSurface;
}

bool MacIOSurfaceTextureHostBasic::Lock() {
  if (!mProvider) {
    return false;
  }

  if (!mTextureSource) {
    mTextureSource = new MacIOSurfaceTextureSourceBasic(mSurface);
  }
  return true;
}

void MacIOSurfaceTextureHostBasic::SetTextureSourceProvider(
    TextureSourceProvider* aProvider) {
  if (!aProvider->AsCompositor() ||
      !aProvider->AsCompositor()->AsBasicCompositor()) {
    mTextureSource = nullptr;
    return;
  }

  mProvider = aProvider;

  if (mTextureSource) {
    mTextureSource->SetTextureSourceProvider(aProvider);
  }
}

gfx::SurfaceFormat MacIOSurfaceTextureHostBasic::GetFormat() const {
  return mSurface->HasAlpha() ? gfx::SurfaceFormat::R8G8B8A8
                              : gfx::SurfaceFormat::B8G8R8X8;
}

gfx::IntSize MacIOSurfaceTextureHostBasic::GetSize() const {
  return gfx::IntSize(mSurface->GetDevicePixelWidth(),
                      mSurface->GetDevicePixelHeight());
}

}  // namespace layers
}  // namespace mozilla
