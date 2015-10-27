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

} // namespace layers
} // namespace mozilla
