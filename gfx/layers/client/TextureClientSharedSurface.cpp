/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureClientSharedSurface.h"

#include "GLContext.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"        // for gfxDebug
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/unused.h"
#include "nsThreadUtils.h"
#include "SharedSurface.h"

#ifdef MOZ_WIDGET_GONK
#include "mozilla/layers/GrallocTextureClient.h"
#include "SharedSurfaceGralloc.h"
#endif

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

SharedSurfaceTextureClient::SharedSurfaceTextureClient(ISurfaceAllocator* aAllocator,
                                                       TextureFlags aFlags,
                                                       UniquePtr<gl::SharedSurface> surf,
                                                       gl::SurfaceFactory* factory)
  : TextureClient(aAllocator,
                  aFlags | TextureFlags::RECYCLE | surf->GetTextureFlags())
  , mSurf(Move(surf))
{
}

SharedSurfaceTextureClient::~SharedSurfaceTextureClient()
{
  // Free the ShSurf implicitly.
}

gfx::IntSize
SharedSurfaceTextureClient::GetSize() const
{
  return mSurf->mSize;
}

bool
SharedSurfaceTextureClient::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  return mSurf->ToSurfaceDescriptor(&aOutDescriptor);
}

void
SharedSurfaceTextureClient::SetReleaseFenceHandle(const FenceHandle& aReleaseFenceHandle)
{
#ifdef MOZ_WIDGET_GONK
  gl::SharedSurface_Gralloc* surf = nullptr;
  if (mSurf->mType == gl::SharedSurfaceType::Gralloc) {
    surf = gl::SharedSurface_Gralloc::Cast(mSurf.get());
  }
  if (surf && surf->GetTextureClient()) {
    surf->GetTextureClient()->SetReleaseFenceHandle(aReleaseFenceHandle);
    return;
  }
#endif
  TextureClient::SetReleaseFenceHandle(aReleaseFenceHandle);
}

FenceHandle
SharedSurfaceTextureClient::GetAndResetReleaseFenceHandle()
{
#ifdef MOZ_WIDGET_GONK
  gl::SharedSurface_Gralloc* surf = nullptr;
  if (mSurf->mType == gl::SharedSurfaceType::Gralloc) {
    surf = gl::SharedSurface_Gralloc::Cast(mSurf.get());
  }
  if (surf && surf->GetTextureClient()) {
    return surf->GetTextureClient()->GetAndResetReleaseFenceHandle();
  }
#endif
  return TextureClient::GetAndResetReleaseFenceHandle();
}

void
SharedSurfaceTextureClient::SetAcquireFenceHandle(const FenceHandle& aAcquireFenceHandle)
{
#ifdef MOZ_WIDGET_GONK
  gl::SharedSurface_Gralloc* surf = nullptr;
  if (mSurf->mType == gl::SharedSurfaceType::Gralloc) {
    surf = gl::SharedSurface_Gralloc::Cast(mSurf.get());
  }
  if (surf && surf->GetTextureClient()) {
    return surf->GetTextureClient()->SetAcquireFenceHandle(aAcquireFenceHandle);
  }
#endif
  TextureClient::SetAcquireFenceHandle(aAcquireFenceHandle);
}

const FenceHandle&
SharedSurfaceTextureClient::GetAcquireFenceHandle() const
{
#ifdef MOZ_WIDGET_GONK
  gl::SharedSurface_Gralloc* surf = nullptr;
  if (mSurf->mType == gl::SharedSurfaceType::Gralloc) {
    surf = gl::SharedSurface_Gralloc::Cast(mSurf.get());
  }
  if (surf && surf->GetTextureClient()) {
    return surf->GetTextureClient()->GetAcquireFenceHandle();
  }
#endif
  return TextureClient::GetAcquireFenceHandle();
}

} // namespace layers
} // namespace mozilla
