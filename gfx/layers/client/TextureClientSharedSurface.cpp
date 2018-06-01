/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureClientSharedSurface.h"

#include "GLContext.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"        // for gfxDebug
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/Unused.h"
#include "nsThreadUtils.h"
#include "SharedSurface.h"

using namespace mozilla::gl;

namespace mozilla {
namespace layers {


SharedSurfaceTextureData::SharedSurfaceTextureData(UniquePtr<gl::SharedSurface> surf)
  : mSurf(std::move(surf))
{}

SharedSurfaceTextureData::~SharedSurfaceTextureData()
{}

void
SharedSurfaceTextureData::Deallocate(LayersIPCChannel*)
{}

void
SharedSurfaceTextureData::FillInfo(TextureData::Info& aInfo) const
{
  aInfo.size = mSurf->mSize;
  aInfo.format = gfx::SurfaceFormat::UNKNOWN;
  aInfo.hasIntermediateBuffer = false;
  aInfo.hasSynchronization = false;
  aInfo.supportsMoz2D = false;
  aInfo.canExposeMappedData = false;
}

bool
SharedSurfaceTextureData::Serialize(SurfaceDescriptor& aOutDescriptor)
{
  return mSurf->ToSurfaceDescriptor(&aOutDescriptor);
}


SharedSurfaceTextureClient::SharedSurfaceTextureClient(SharedSurfaceTextureData* aData,
                                                       TextureFlags aFlags,
                                                       LayersIPCChannel* aAllocator)
: TextureClient(aData, aFlags, aAllocator)
{
  mWorkaroundAnnoyingSharedSurfaceLifetimeIssues = true;
}

already_AddRefed<SharedSurfaceTextureClient>
SharedSurfaceTextureClient::Create(UniquePtr<gl::SharedSurface> surf, gl::SurfaceFactory* factory,
                                   LayersIPCChannel* aAllocator, TextureFlags aFlags)
{
  if (!surf) {
    return nullptr;
  }
  TextureFlags flags = aFlags | TextureFlags::RECYCLE | surf->GetTextureFlags();
  SharedSurfaceTextureData* data = new SharedSurfaceTextureData(std::move(surf));
  return MakeAndAddRef<SharedSurfaceTextureClient>(data, flags, aAllocator);
}

SharedSurfaceTextureClient::~SharedSurfaceTextureClient()
{
  // XXX - Things break when using the proper destruction handshake with
  // SharedSurfaceTextureData because the TextureData outlives its gl
  // context. Having a strong reference to the gl context creates a cycle.
  // This needs to be fixed in a better way, though, because deleting
  // the TextureData here can race with the compositor and cause flashing.
  TextureData* data = mData;
  mData = nullptr;

  Destroy();

  if (data) {
    // Destroy mData right away without doing the proper deallocation handshake,
    // because SharedSurface depends on things that may not outlive the texture's
    // destructor so we can't wait until we know the compositor isn't using the
    // texture anymore.
    // It goes without saying that this is really bad and we should fix the bugs
    // that block doing the right thing such as bug 1224199 sooner rather than
    // later.
    delete data;
  }
}

} // namespace layers
} // namespace mozilla
