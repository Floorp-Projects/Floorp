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
    TextureFlags::DEFAULT,
    TextureAllocationFlags::ALLOC_DEFAULT
  );

  if (!texture) {
    return nullptr;
  }

  texture->EnableReadLock();

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

  if (IsActivityTracked()) {
    mFwd->GetActiveResourceTracker().RemoveObject(this);
  }

  Destroy();
}

already_AddRefed<gfx::DrawTarget>
PersistentBufferProviderShared::BorrowDrawTarget(const gfx::IntRect& aPersistedRect)
{
  if (!mFwd->IPCOpen()) {
    return nullptr;
  }

  MOZ_ASSERT(!mSnapshot);

  if (IsActivityTracked()) {
    mFwd->GetActiveResourceTracker().MarkUsed(this);
  } else {
    mFwd->GetActiveResourceTracker().AddObject(this);
  }

  if (!mDrawTarget) {
    bool changedBackBuffer = false;
    if (!mBack || mBack->IsReadLocked()) {
      if (mBuffer && !mBuffer->IsReadLocked()) {
        mBack.swap(mBuffer);
      } else {
        mBack = TextureClient::CreateForDrawing(
          mFwd, mFormat, mSize,
          BackendSelector::Canvas,
          TextureFlags::DEFAULT,
          TextureAllocationFlags::ALLOC_DEFAULT
        );
        if (mBack) {
          mBack->EnableReadLock();
        }
      }
      changedBackBuffer = true;
    } else {
      // Fast path, our front buffer is already writable because the texture upload
      // has completed on the compositor side.
      if (mBack->HasIntermediateBuffer()) {
        // No need to keep an extra buffer around
        mBuffer = nullptr;
      }
    }

    if (!mBack || !mBack->Lock(OpenMode::OPEN_READ_WRITE)) {
      return nullptr;
    }

    if (changedBackBuffer && !aPersistedRect.IsEmpty()
        && mFront && mFront->Lock(OpenMode::OPEN_READ)) {

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
  MOZ_ASSERT(!mSnapshot);

  mDrawTarget = nullptr;
  dt = nullptr;

  mBack->Unlock();

  if (!mBuffer && mFront && !mFront->IsLocked()) {
    mBuffer.swap(mFront);
  }

  mFront = mBack;

  return true;
}

already_AddRefed<gfx::SourceSurface>
PersistentBufferProviderShared::BorrowSnapshot()
{
  // TODO[nical] currently we can't snapshot while drawing, looks like it does
  // the job but I am not sure whether we want to be able to do that.
  MOZ_ASSERT(!mDrawTarget);

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

void
PersistentBufferProviderShared::NotifyInactive()
{
  if (mBuffer && mBuffer->IsLocked()) {
    // mBuffer should never be locked
    MOZ_ASSERT(false);
    mBuffer->Unlock();
  }
  mBuffer = nullptr;
}

void
PersistentBufferProviderShared::Destroy()
{
  mSnapshot = nullptr;
  mDrawTarget = nullptr;

  if (mFront && mFront->IsLocked()) {
    mFront->Unlock();
  }

  if (mBack && mBack->IsLocked()) {
    mBack->Unlock();
  }

  if (mBuffer && mBuffer->IsLocked()) {
    mBuffer->Unlock();
  }

  mFront = nullptr;
  mBack = nullptr;
  mBuffer = nullptr;
}

} // namespace layers
} // namespace mozilla