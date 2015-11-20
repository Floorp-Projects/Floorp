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

} // namespace layers
} // namespace mozilla
