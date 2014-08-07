/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureDIB.h"
#include "gfx2DGlue.h"

namespace mozilla {

using namespace gfx;

namespace layers {

DIBTextureClient::DIBTextureClient(gfx::SurfaceFormat aFormat, TextureFlags aFlags)
  : TextureClient(aFlags)
  , mFormat(aFormat)
  , mIsLocked(false)
{
  MOZ_COUNT_CTOR(DIBTextureClient);
}

DIBTextureClient::~DIBTextureClient()
{
  MOZ_COUNT_DTOR(DIBTextureClient);
}

TemporaryRef<TextureClient>
DIBTextureClient::CreateSimilar(TextureFlags aFlags,
                                TextureAllocationFlags aAllocFlags) const
{
  RefPtr<TextureClient> tex = new DIBTextureClient(mFormat, mFlags | aFlags);

  if (!tex->AllocateForSurface(mSize, aAllocFlags)) {
    return nullptr;
  }

  return tex;
}

bool
DIBTextureClient::Lock(OpenMode)
{
  MOZ_ASSERT(!mIsLocked);
  if (!IsValid()) {
    return false;
  }
  mIsLocked = true;
  return true;
}

void
DIBTextureClient::Unlock()
{
  MOZ_ASSERT(mIsLocked, "Unlocked called while the texture is not locked!");
  if (mDrawTarget) {
    if (mReadbackSink) {
      RefPtr<SourceSurface> snapshot = mDrawTarget->Snapshot();
      RefPtr<DataSourceSurface> dataSurf = snapshot->GetDataSurface();
      mReadbackSink->ProcessReadback(dataSurf);
    }

    mDrawTarget->Flush();
    mDrawTarget = nullptr;
  }

  mIsLocked = false;
}

bool
DIBTextureClient::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(IsValid());
  if (!IsAllocated()) {
    return false;
  }
  MOZ_ASSERT(mSurface);
  // The host will release this ref when it receives the surface descriptor.
  // We AddRef in case we die before the host receives the pointer.
  aOutDescriptor = SurfaceDescriptorDIB(reinterpret_cast<uintptr_t>(mSurface.get()));
  mSurface->AddRef();
  return true;
}

gfx::DrawTarget*
DIBTextureClient::BorrowDrawTarget()
{
  MOZ_ASSERT(mIsLocked && IsAllocated());

  if (!mDrawTarget) {
    mDrawTarget =
      gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(mSurface, mSize);
  }

  return mDrawTarget;
}

bool
DIBTextureClient::AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags aFlags)
{
  MOZ_ASSERT(!IsAllocated());
  mSize = aSize;

  mSurface = new gfxWindowsSurface(gfxIntSize(aSize.width, aSize.height),
                                   SurfaceFormatToImageFormat(mFormat));
  if (!mSurface || mSurface->CairoStatus())
  {
    NS_WARNING("Could not create surface");
    mSurface = nullptr;
    return false;
  }

  return true;
}

DIBTextureHost::DIBTextureHost(TextureFlags aFlags,
                               const SurfaceDescriptorDIB& aDescriptor)
  : TextureHost(aFlags)
  , mIsLocked(false)
{
  // We added an extra ref for transport, so we shouldn't AddRef now.
  mSurface =
    dont_AddRef(reinterpret_cast<gfxWindowsSurface*>(aDescriptor.surface()));
  MOZ_ASSERT(mSurface);

  mSize = ToIntSize(mSurface->GetSize());
  mFormat = ImageFormatToSurfaceFormat(
    gfxPlatform::GetPlatform()->OptimalFormatForContent(mSurface->GetContentType()));
}

NewTextureSource*
DIBTextureHost::GetTextureSources()
{
  if (!mTextureSource) {
    Updated();
  }

  return mTextureSource;
}

void
DIBTextureHost::Updated(const nsIntRegion* aRegion)
{
  if (!mTextureSource) {
    mTextureSource = mCompositor->CreateDataTextureSource(mFlags);
  }

  nsRefPtr<gfxImageSurface> imgSurf = mSurface->GetAsImageSurface();

  RefPtr<DataSourceSurface> surf = Factory::CreateWrappingDataSourceSurface(imgSurf->Data(), imgSurf->Stride(), mSize, mFormat);

  if (!mTextureSource->Update(surf, const_cast<nsIntRegion*>(aRegion))) {
    mTextureSource = nullptr;
  }
}

bool
DIBTextureHost::Lock()
{
  MOZ_ASSERT(!mIsLocked);
  mIsLocked = true;
  return true;
}

void
DIBTextureHost::Unlock()
{
  MOZ_ASSERT(mIsLocked);
  mIsLocked = false;
}

void
DIBTextureHost::SetCompositor(Compositor* aCompositor)
{
  mCompositor = aCompositor;
}

void
DIBTextureHost::DeallocateDeviceData()
{
  if (mTextureSource) {
    mTextureSource->DeallocateDeviceData();
  }
}

}
}