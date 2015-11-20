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
// EGLImage

EGLImageTextureData::EGLImageTextureData(EGLImageImage* aImage, gfx::IntSize aSize)
: mImage(aImage)
, mSize(aSize)
{
  MOZ_ASSERT(aImage);
}

already_AddRefed<TextureClient>
EGLImageTextureData::CreateTextureClient(EGLImageImage* aImage, gfx::IntSize aSize,
                                         ISurfaceAllocator* aAllocator, TextureFlags aFlags)
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Can't pass an `EGLImage` between processes.");

  if (!aImage || !XRE_IsParentProcess()) {
    return nullptr;
  }

  // XXX - This is quite sad and slow.
  aFlags |= TextureFlags::DEALLOCATE_CLIENT;

  if (aImage->GetOriginPos() == gl::OriginPos::BottomLeft) {
    aFlags |= TextureFlags::ORIGIN_BOTTOM_LEFT;
  }

  return TextureClient::CreateWithData(
    new EGLImageTextureData(aImage, aSize),
    aFlags, aAllocator
  );
}

bool
EGLImageTextureData::Serialize(SurfaceDescriptor& aOutDescriptor)
{
  const bool hasAlpha = true;
  aOutDescriptor =
    EGLImageDescriptor((uintptr_t)mImage->GetImage(),
                       (uintptr_t)mImage->GetSync(),
                       mImage->GetSize(), hasAlpha);
  return true;
}

////////////////////////////////////////////////////////////////////////
// AndroidSurface

#ifdef MOZ_WIDGET_ANDROID

already_AddRefed<TextureClient>
AndroidSurfaceTextureData::CreateTextureClient(AndroidSurfaceTexture* aSurfTex,
                                               gfx::IntSize aSize,
                                               gl::OriginPos aOriginPos,
                                               ISurfaceAllocator* aAllocator,
                                               TextureFlags aFlags)
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Can't pass an android surfaces between processes.");

  if (!aSurfTex || !XRE_IsParentProcess()) {
    return nullptr;
  }

  // XXX - This is quite sad and slow.
  aFlags |= TextureFlags::DEALLOCATE_CLIENT;

  if (aOriginPos == gl::OriginPos::BottomLeft) {
    aFlags |= TextureFlags::ORIGIN_BOTTOM_LEFT;
  }

  return TextureClient::CreateWithData(
    new AndroidSurfaceTextureData(aSurfTex, aSize),
    aFlags, aAllocator
  );
}

AndroidSurfaceTextureData::AndroidSurfaceTextureData(AndroidSurfaceTexture* aSurfTex,
                                                     gfx::IntSize aSize)
  : mSurfTex(aSurfTex)
  , mSize(aSize)
{}

AndroidSurfaceTextureData::~AndroidSurfaceTextureData()
{}

bool
AndroidSurfaceTextureData::Serialize(SurfaceDescriptor& aOutDescriptor)
{
  aOutDescriptor = SurfaceTextureDescriptor((uintptr_t)mSurfTex.get(),
                                            mSize);
  return true;
}

#endif // MOZ_WIDGET_ANDROID

} // namespace layers
} // namespace mozilla
