/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContext.h"                  // for GLContext, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/TextureClientOGL.h"
#include "mozilla/gfx/Point.h"          // for IntSize
#include "GLLibraryEGL.h"

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

class CompositableForwarder;

////////////////////////////////////////////////////////////////////////
// AndroidSurface

#ifdef MOZ_WIDGET_ANDROID

already_AddRefed<TextureClient>
AndroidSurfaceTextureData::CreateTextureClient(AndroidSurfaceTextureHandle aHandle,
                                               gfx::IntSize aSize,
                                               bool aContinuous,
                                               gl::OriginPos aOriginPos,
                                               LayersIPCChannel* aAllocator,
                                               TextureFlags aFlags)
{
  if (aOriginPos == gl::OriginPos::BottomLeft) {
    aFlags |= TextureFlags::ORIGIN_BOTTOM_LEFT;
  }

  return TextureClient::CreateWithData(
    new AndroidSurfaceTextureData(aHandle, aSize, aContinuous),
    aFlags, aAllocator
  );
}

AndroidSurfaceTextureData::AndroidSurfaceTextureData(AndroidSurfaceTextureHandle aHandle,
                                                     gfx::IntSize aSize, bool aContinuous)
  : mHandle(aHandle)
  , mSize(aSize)
  , mContinuous(aContinuous)
{
  MOZ_ASSERT(mHandle);
}

AndroidSurfaceTextureData::~AndroidSurfaceTextureData()
{}

void
AndroidSurfaceTextureData::FillInfo(TextureData::Info& aInfo) const
{
  aInfo.size = mSize;
  aInfo.format = gfx::SurfaceFormat::UNKNOWN;
  aInfo.hasIntermediateBuffer = false;
  aInfo.hasSynchronization = false;
  aInfo.supportsMoz2D = false;
  aInfo.canExposeMappedData = false;
}

bool
AndroidSurfaceTextureData::Serialize(SurfaceDescriptor& aOutDescriptor)
{
  aOutDescriptor = SurfaceTextureDescriptor(mHandle, mSize, mContinuous);
  return true;
}

#endif // MOZ_WIDGET_ANDROID

} // namespace layers
} // namespace mozilla
