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
// EGLImageTextureClient

EGLImageTextureClient::EGLImageTextureClient(ISurfaceAllocator* aAllocator,
                                             TextureFlags aFlags,
                                             EGLImageImage* aImage,
                                             gfx::IntSize aSize)
  : TextureClient(aAllocator, aFlags)
  , mImage(aImage)
  , mSize(aSize)
  , mIsLocked(false)
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Can't pass an `EGLImage` between processes.");

  AddFlags(TextureFlags::DEALLOCATE_CLIENT);

  if (aImage->GetData()->mOriginPos == gl::OriginPos::BottomLeft) {
    AddFlags(TextureFlags::ORIGIN_BOTTOM_LEFT);
  }
}

bool
EGLImageTextureClient::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(IsAllocated());

  const EGLImageImage::Data* data = mImage->GetData();
  const bool hasAlpha = true;
  aOutDescriptor = EGLImageDescriptor((uintptr_t)data->mImage, (uintptr_t)data->mSync,
                                      mSize, hasAlpha);
  return true;
}

bool
EGLImageTextureClient::Lock(OpenMode mode)
  {
    MOZ_ASSERT(!mIsLocked);
    if (!IsValid() || !IsAllocated()) {
      return false;
    }
    mIsLocked = true;
    return true;
  }

void
EGLImageTextureClient::Unlock()
{
  MOZ_ASSERT(mIsLocked);
  mIsLocked = false;
}

////////////////////////////////////////////////////////////////////////
// SurfaceTextureClient

#ifdef MOZ_WIDGET_ANDROID

SurfaceTextureClient::SurfaceTextureClient(ISurfaceAllocator* aAllocator,
                                           TextureFlags aFlags,
                                           AndroidSurfaceTexture* aSurfTex,
                                           gfx::IntSize aSize,
                                           gl::OriginPos aOriginPos)
  : TextureClient(aAllocator, aFlags)
  , mSurfTex(aSurfTex)
  , mSize(aSize)
  , mIsLocked(false)
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Can't pass pointers between processes.");

  // Our data is always owned externally.
  AddFlags(TextureFlags::DEALLOCATE_CLIENT);

  if (aOriginPos == gl::OriginPos::BottomLeft) {
    AddFlags(TextureFlags::ORIGIN_BOTTOM_LEFT);
  }
}

SurfaceTextureClient::~SurfaceTextureClient()
{
  // Our data is always owned externally.
}

bool
SurfaceTextureClient::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(IsAllocated());

  aOutDescriptor = SurfaceTextureDescriptor((uintptr_t)mSurfTex.get(),
                                            mSize);
  return true;
}

bool
SurfaceTextureClient::Lock(OpenMode mode)
{
  MOZ_ASSERT(!mIsLocked);
  if (!IsValid() || !IsAllocated()) {
    return false;
  }
  mIsLocked = true;
  return true;
}

void
SurfaceTextureClient::Unlock()
{
  MOZ_ASSERT(mIsLocked);
  mIsLocked = false;
}

#endif // MOZ_WIDGET_ANDROID

} // namespace
} // namespace
