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
  if (dt) {
    // Since SkiaGL default to storing drawing command until flush
    // we have to flush it before present.
    dt->Flush();
  }
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
                                       KnowsCompositor* aFwd)
{
  if (!aFwd || !aFwd->GetTextureForwarder()->IPCOpen()) {
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

  RefPtr<PersistentBufferProviderShared> provider =
    new PersistentBufferProviderShared(aSize, aFormat, aFwd, texture);
  return provider.forget();
}

PersistentBufferProviderShared::PersistentBufferProviderShared(gfx::IntSize aSize,
                                                               gfx::SurfaceFormat aFormat,
                                                               KnowsCompositor* aFwd,
                                                               RefPtr<TextureClient>& aTexture)

: mSize(aSize)
, mFormat(aFormat)
, mFwd(aFwd)
, mFront(Nothing())
{
  if (mTextures.append(aTexture)) {
    mBack = Some<uint32_t>(0);
  }
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

bool
PersistentBufferProviderShared::SetForwarder(KnowsCompositor* aFwd)
{
  MOZ_ASSERT(aFwd);
  if (!aFwd) {
    return false;
  }

  if (mFwd == aFwd) {
    // The forwarder should not change most of the time.
    return true;
  }

  if (IsActivityTracked()) {
    mFwd->GetActiveResourceTracker().RemoveObject(this);
  }

  if (mFwd->GetTextureForwarder() != aFwd->GetTextureForwarder() ||
      mFwd->GetCompositorBackendType() != aFwd->GetCompositorBackendType()) {
    // We are going to be used with an different and/or incompatible forwarder.
    // This should be extremely rare. We have to copy the front buffer into a
    // texture that is compatible with the new forwarder.

    // Grab the current front buffer.
    RefPtr<TextureClient> prevTexture = GetTexture(mFront);

    // Get rid of everything else
    Destroy();

    if (prevTexture) {
      RefPtr<TextureClient> newTexture = TextureClient::CreateForDrawing(
        aFwd, mFormat, mSize,
        BackendSelector::Canvas,
        TextureFlags::DEFAULT,
        TextureAllocationFlags::ALLOC_DEFAULT
      );

      MOZ_ASSERT(newTexture);
      if (!newTexture) {
        return false;
      }

      // If we early-return in one of the following branches, we will
      // leave the buffer provider in an empty state, since we called
      // Destroy. Not ideal but at least we won't try to use it with a
      // an incompatible ipc channel.

      if (!newTexture->Lock(OpenMode::OPEN_WRITE)) {
        return false;
      }

      if (!prevTexture->Lock(OpenMode::OPEN_READ)) {
        newTexture->Unlock();
        return false;
      }

      bool success = prevTexture->CopyToTextureClient(newTexture, nullptr, nullptr);

      prevTexture->Unlock();
      newTexture->Unlock();

      if (!success) {
        return false;
      }

      if (!mTextures.append(newTexture)) {
        return false;
      }
      mFront = Some<uint32_t>(mTextures.length() - 1);
      mBack = mFront;
    }
  }

  mFwd = aFwd;

  return true;
}

TextureClient*
PersistentBufferProviderShared::GetTexture(Maybe<uint32_t> aIndex)
{
  if (aIndex.isNothing() || !CheckIndex(aIndex.value())) {
    return nullptr;
  }
  return mTextures[aIndex.value()];
}

already_AddRefed<gfx::DrawTarget>
PersistentBufferProviderShared::BorrowDrawTarget(const gfx::IntRect& aPersistedRect)
{
  if (!mFwd->GetTextureForwarder()->IPCOpen()) {
    return nullptr;
  }

  MOZ_ASSERT(!mSnapshot);

  if (IsActivityTracked()) {
    mFwd->GetActiveResourceTracker().MarkUsed(this);
  } else {
    mFwd->GetActiveResourceTracker().AddObject(this);
  }

  if (mDrawTarget) {
    RefPtr<gfx::DrawTarget> dt(mDrawTarget);
    return dt.forget();
  }

  mFront = Nothing();

  auto previousBackBuffer = mBack;

  TextureClient* tex = GetTexture(mBack);

  // First try to reuse the current back buffer. If we can do that it means
  // we can skip copying its content to the new back buffer.
  if (tex && tex->IsReadLocked()) {
    // The back buffer is currently used by the compositor, we can't draw
    // into it.
    tex = nullptr;
  }

  if (!tex) {
    // Try to grab an already allocated texture if any is available.
    for (uint32_t i = 0; i < mTextures.length(); ++i) {
      if (!mTextures[i]->IsReadLocked()) {
        mBack = Some(i);
        tex = mTextures[i];
        break;
      }
    }
  }

  if (!tex) {
    // We have to allocate a new texture.
    if (mTextures.length() >= 4) {
      // We should never need to buffer that many textures, something's wrong.
      MOZ_ASSERT(false);
      // In theory we throttle the main thread when the compositor can't keep up,
      // so we shoud never get in a situation where we sent 4 textures to the
      // compositor and the latter as not released any of them.
      // This seems to happen, however, in some edge cases such as just after a
      // device reset (cf. Bug 1291163).
      // It would be pretty bad to keep piling textures up at this point so we
      // call NotifyInactive to remove some of our textures.
      NotifyInactive();
      // Give up now. The caller can fall-back to a non-shared buffer provider.
      return nullptr;
    }

    RefPtr<TextureClient> newTexture = TextureClient::CreateForDrawing(
      mFwd, mFormat, mSize,
      BackendSelector::Canvas,
      TextureFlags::DEFAULT,
      TextureAllocationFlags::ALLOC_DEFAULT
    );

    MOZ_ASSERT(newTexture);
    if (newTexture) {
      if (mTextures.append(newTexture)) {
        tex = newTexture;
        mBack = Some<uint32_t>(mTextures.length() - 1);
      }
    }
  }

  if (!tex || !tex->Lock(OpenMode::OPEN_READ_WRITE)) {
    return nullptr;
  }

  if (mBack != previousBackBuffer && !aPersistedRect.IsEmpty()) {
    TextureClient* previous = GetTexture(previousBackBuffer);
    if (previous && previous->Lock(OpenMode::OPEN_READ)) {
      DebugOnly<bool> success = previous->CopyToTextureClient(tex, &aPersistedRect, nullptr);
      MOZ_ASSERT(success);

      previous->Unlock();
    }
  }

  mDrawTarget = tex->BorrowDrawTarget();

  RefPtr<gfx::DrawTarget> dt(mDrawTarget);
  return dt.forget();
}

bool
PersistentBufferProviderShared::ReturnDrawTarget(already_AddRefed<gfx::DrawTarget> aDT)
{
  RefPtr<gfx::DrawTarget> dt(aDT);
  MOZ_ASSERT(mDrawTarget == dt);
  // Can't change the current front buffer while its snapshot is borrowed!
  MOZ_ASSERT(!mSnapshot);

  mDrawTarget = nullptr;
  dt = nullptr;

  TextureClient* back = GetTexture(mBack);
  MOZ_ASSERT(back);

  if (back) {
    back->Unlock();
    mFront = mBack;
  }

  return !!back;
}

TextureClient*
PersistentBufferProviderShared::GetTextureClient()
{
  // Can't access the front buffer while drawing.
  MOZ_ASSERT(!mDrawTarget);
  TextureClient* texture = GetTexture(mFront);
  if (texture) {
    texture->EnableReadLock();
  } else {
    gfxCriticalNote << "PersistentBufferProviderShared: front buffer unavailable";
  }
  return texture;
}

already_AddRefed<gfx::SourceSurface>
PersistentBufferProviderShared::BorrowSnapshot()
{
  MOZ_ASSERT(!mDrawTarget);

  auto front = GetTexture(mFront);
  if (!front || front->IsLocked()) {
    MOZ_ASSERT(false);
    return nullptr;
  }

  if (!front->Lock(OpenMode::OPEN_READ)) {
    return nullptr;
  }

  RefPtr<DrawTarget> dt = front->BorrowDrawTarget();

  if (!dt) {
    front->Unlock();
    return nullptr;
  }

  mSnapshot = dt->Snapshot();

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

  auto front = GetTexture(mFront);
  if (front) {
    front->Unlock();
  }
}

void
PersistentBufferProviderShared::NotifyInactive()
{
  RefPtr<TextureClient> front = GetTexture(mFront);
  RefPtr<TextureClient> back = GetTexture(mBack);

  // Clear all textures (except the front and back ones that we just kept).
  mTextures.clear();

  if (back) {
    if (mTextures.append(back)) {
      mBack = Some<uint32_t>(0);
    }
    if (front == back) {
      mFront = mBack;
    }
  }

  if (front && front != back) {
    if (mTextures.append(front)) {
      mFront = Some<uint32_t>(mTextures.length() - 1);
    }
  }
}

void
PersistentBufferProviderShared::Destroy()
{
  mSnapshot = nullptr;
  mDrawTarget = nullptr;

  for (uint32_t i = 0; i < mTextures.length(); ++i) {
    TextureClient* texture = mTextures[i];
    if (texture && texture->IsLocked()) {
      MOZ_ASSERT(false);
      texture->Unlock();
    }
  }

  mTextures.clear();
}

} // namespace layers
} // namespace mozilla
