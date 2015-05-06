/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureClientSharedSurface.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"        // for gfxDebug
#include "mozilla/layers/ISurfaceAllocator.h"
#include "SharedSurface.h"
#include "GLContext.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layers {

SharedSurfaceTextureClient::SharedSurfaceTextureClient(ISurfaceAllocator* aAllocator,
                                                       TextureFlags aFlags,
                                                       UniquePtr<gl::SharedSurface> surf,
                                                       gl::SurfaceFactory* factory)
  : TextureClient(aAllocator, aFlags | TextureFlags::RECYCLE)
  , mSurf(Move(surf))
  , mFactory(factory)
  , mRecyclingRef(this)
{
  // AtomicRefCountedWithFinalize is a little strange. It attempts to recycle when
  // Release drops the ref count to 1. The idea is that the recycler holds a strong ref.
  // Here, we want to keep it simple, and always have a recycle callback, but only recycle
  // if the WeakPtr mFactory is still non-null. Therefore, we keep mRecyclingRef as ref to
  // ourself! Call StopRecycling() to end the cycle.
  mRecyclingRef->SetRecycleCallback(&gl::SurfaceFactory::RecycleCallback, nullptr);
}

SharedSurfaceTextureClient::~SharedSurfaceTextureClient()
{
  // Free the ShSurf implicitly.

  // Tricky issue! We will often call the dtor via Release via StopRecycling, where we
  // null out mRecyclingRef. Right now, we call Release before actually clearing the
  // value. This means the dtor will see that mRecyclingRef is non-null, and try to
  // Release it, even though we're mid-Release! We need to clear mRecyclingRef so the dtor
  // doesn't try to Release it.
  mozilla::unused << mRecyclingRef.forget().take();
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
SharedSurfaceTextureClient::StopRecycling()
{
  mRecyclingRef->ClearRecycleCallback();
  mRecyclingRef = nullptr;
}

} // namespace layers
} // namespace mozilla
