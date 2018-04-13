/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderSharedSurfaceTextureHost.h"

#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/SourceSurfaceSharedData.h"

namespace mozilla {
namespace wr {

RenderSharedSurfaceTextureHost::RenderSharedSurfaceTextureHost(gfx::SourceSurfaceSharedDataWrapper* aSurface)
  : mSurface(aSurface)
  , mLocked(false)
{
  MOZ_COUNT_CTOR_INHERITED(RenderSharedSurfaceTextureHost, RenderTextureHost);
  MOZ_ASSERT(aSurface);
}

RenderSharedSurfaceTextureHost::~RenderSharedSurfaceTextureHost()
{
  MOZ_COUNT_DTOR_INHERITED(RenderSharedSurfaceTextureHost, RenderTextureHost);
}

wr::WrExternalImage
RenderSharedSurfaceTextureHost::Lock(uint8_t aChannelIndex, gl::GLContext* aGL)
{
  if (!mLocked) {
    if (NS_WARN_IF(!mSurface->Map(gfx::DataSourceSurface::MapType::READ_WRITE,
                                  &mMap))) {
      return InvalidToWrExternalImage();
    }
    mLocked = true;
  }

  return RawDataToWrExternalImage(mMap.mData,
                                  mMap.mStride * mSurface->GetSize().height);
}

void
RenderSharedSurfaceTextureHost::Unlock()
{
  if (mLocked) {
    mSurface->Unmap();
    mLocked = false;
  }
}

} // namespace wr
} // namespace mozilla
