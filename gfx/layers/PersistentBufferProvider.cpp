/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PersistentBufferProvider.h"

#include "Layers.h"
#include "mozilla/gfx/Logging.h"
#include "pratom.h"
#include "gfxPlatform.h"

namespace mozilla {

using namespace gfx;

namespace layers {

PersistentBufferProviderBasic::PersistentBufferProviderBasic(DrawTarget* aDt)
: mDrawTarget(aDt)
{
  MOZ_COUNT_CTOR(PersistentBufferProviderBasic);
}

PersistentBufferProviderBasic::~PersistentBufferProviderBasic()
{
  MOZ_COUNT_DTOR(PersistentBufferProviderBasic);
}

already_AddRefed<gfx::DrawTarget>
PersistentBufferProviderBasic::BorrowDrawTarget(const gfx::IntRect& aPersistedRect)
{
  MOZ_ASSERT(!mSnapshot);
  RefPtr<gfx::DrawTarget> dt(mDrawTarget);
  return dt.forget();
}

bool
PersistentBufferProviderBasic::ReturnDrawTarget(already_AddRefed<gfx::DrawTarget> aDT)
{
  RefPtr<gfx::DrawTarget> dt(aDT);
  MOZ_ASSERT(mDrawTarget == dt);
  return true;
}

already_AddRefed<gfx::SourceSurface>
PersistentBufferProviderBasic::BorrowSnapshot()
{
  mSnapshot = mDrawTarget->Snapshot();
  RefPtr<SourceSurface> snapshot = mSnapshot;
  return snapshot.forget();
}

void
PersistentBufferProviderBasic::ReturnSnapshot(already_AddRefed<gfx::SourceSurface> aSnapshot)
{
  RefPtr<SourceSurface> snapshot = aSnapshot;
  MOZ_ASSERT(!snapshot || snapshot == mSnapshot);
  mSnapshot = nullptr;
}

//static
already_AddRefed<PersistentBufferProviderBasic>
PersistentBufferProviderBasic::Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                                      gfx::BackendType aBackend)
{
  RefPtr<DrawTarget> dt = gfxPlatform::GetPlatform()->CreateDrawTargetForBackend(aBackend, aSize, aFormat);

  if (!dt) {
    return nullptr;
  }

  RefPtr<PersistentBufferProviderBasic> provider =
    new PersistentBufferProviderBasic(dt);

  return provider.forget();
}


//static
already_AddRefed<PersistentBufferProviderShared>
PersistentBufferProviderShared::Create(gfx::IntSize aSize,
                                       gfx::SurfaceFormat aFormat,
                                       CompositableForwarder* aFwd)
{
  if (!aFwd || !aFwd->IPCOpen()) {
    return nullptr;
  }

  RefPtr<TextureClient> texture = TextureClient::CreateForDrawing(
    aFwd, aFormat, aSize,
    BackendSelector::Canvas,
    TextureFlags::IMMEDIATE_UPLOAD,
    TextureAllocationFlags::ALLOC_DEFAULT
  );

  if (!texture) {
    return nullptr;
  }

  RefPtr<PersistentBufferProviderShared> provider =
    new PersistentBufferProviderShared(aSize, aFormat, aFwd, texture);
  return provider.forget();
}

PersistentBufferProviderShared::PersistentBufferProviderShared(gfx::IntSize aSize,
                                                               gfx::SurfaceFormat aFormat,
                                                               CompositableForwarder* aFwd,
                                                               RefPtr<TextureClient>& aTexture)

: mSize(aSize)
, mFormat(aFormat)
, mFwd(aFwd)
, mBack(aTexture.forget())
{
  MOZ_COUNT_CTOR(PersistentBufferProviderShared);
}

PersistentBufferProviderShared::~PersistentBufferProviderShared()
{
  MOZ_COUNT_DTOR(PersistentBufferProviderShared);

  mDrawTarget = nullptr;
  if (mBack && mBack->IsLocked()) {
    mBack->Unlock();
  }
  if (mFront && mFront->IsLocked()) {
    mFront->Unlock();
  }
}

already_AddRefed<gfx::DrawTarget>
PersistentBufferProviderShared::BorrowDrawTarget(const gfx::IntRect& aPersistedRect)
{
  if (!mFwd->IPCOpen()) {
    return nullptr;
  }

  if (!mDrawTarget) {
    if (!mBack) {
      mBack = TextureClient::CreateForDrawing(
        mFwd, mFormat, mSize,
        BackendSelector::Canvas,
        TextureFlags::IMMEDIATE_UPLOAD,
        TextureAllocationFlags::ALLOC_DEFAULT
      );
    }

    if (!mBack) {
      return nullptr;
    }

    if (!mBack->Lock(OpenMode::OPEN_READ_WRITE)) {
      return nullptr;
    }
    if (!aPersistedRect.IsEmpty() && mFront
        && mFront->Lock(OpenMode::OPEN_READ)) {

      DebugOnly<bool> success = mFront->CopyToTextureClient(mBack, &aPersistedRect, nullptr);
      MOZ_ASSERT(success);

      mFront->Unlock();
    }

    mDrawTarget = mBack->BorrowDrawTarget();
    if (!mDrawTarget) {
      return nullptr;
    }
  }

  RefPtr<gfx::DrawTarget> dt(mDrawTarget);
  return dt.forget();
}

bool
PersistentBufferProviderShared::ReturnDrawTarget(already_AddRefed<gfx::DrawTarget> aDT)
{
  RefPtr<gfx::DrawTarget> dt(aDT);
  MOZ_ASSERT(mDrawTarget == dt);

  mDrawTarget = nullptr;
  dt = nullptr;

  mBack->Unlock();

  mFront = mBack;
  mBack = nullptr;

  return true;
}

already_AddRefed<gfx::SourceSurface>
PersistentBufferProviderShared::BorrowSnapshot()
{
  if (!mFront || mFront->IsLocked()) {
    MOZ_ASSERT(false);
    return nullptr;
  }

  if (!mFront->Lock(OpenMode::OPEN_READ)) {
    return nullptr;
  }

  mDrawTarget = mFront->BorrowDrawTarget();

  if (!mDrawTarget) {
    mFront->Unlock();
  }

  mSnapshot = mDrawTarget->Snapshot();

  RefPtr<SourceSurface> snapshot = mSnapshot;
  return snapshot.forget();
}

void
PersistentBufferProviderShared::ReturnSnapshot(already_AddRefed<gfx::SourceSurface> aSnapshot)
{
  RefPtr<SourceSurface> snapshot = aSnapshot;
  MOZ_ASSERT(!snapshot || snapshot == mSnapshot);

  mSnapshot = nullptr;
  snapshot = nullptr;

  mDrawTarget = nullptr;

  mFront->Unlock();
}

} // namespace layers
} // namespace mozilla