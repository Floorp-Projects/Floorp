/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceSurfaceCapture.h"
#include "DrawCommand.h"
#include "DrawTargetCapture.h"
#include "MainThreadUtils.h"
#include "mozilla/gfx/Logging.h"

namespace mozilla {
namespace gfx {

SourceSurfaceCapture::SourceSurfaceCapture(DrawTargetCaptureImpl* aOwner)
 : mOwner(aOwner),
   mHasCommandList(false),
   mShouldResolveToLuminance{false},
   mLuminanceType{LuminanceType::LUMINANCE},
   mOpacity{1.0f},
   mLock("SourceSurfaceCapture.mLock")
{
  mSize = mOwner->GetSize();
  mFormat = mOwner->GetFormat();
  mRefDT = mOwner->mRefDT;
  mStride = mOwner->mStride;
  mSurfaceAllocationSize = mOwner->mSurfaceAllocationSize;
}

SourceSurfaceCapture::SourceSurfaceCapture(DrawTargetCaptureImpl* aOwner,
                                           LuminanceType aLuminanceType /* = LuminanceType::LINEARRGB */,
                                           Float aOpacity /* = 1.0f */)
  : mOwner{aOwner}
  , mHasCommandList{false}
  , mShouldResolveToLuminance{true}
  , mLuminanceType{aLuminanceType}
  , mOpacity{aOpacity}
  , mLock{"SourceSurfaceCapture.mLock"}
{
  mSize = mOwner->GetSize();
  mFormat = mOwner->GetFormat();
  mRefDT = mOwner->mRefDT;
  mStride = mOwner->mStride;
  mSurfaceAllocationSize = mOwner->mSurfaceAllocationSize;

  // In this case our DrawTarget will not track us, so copy its drawing commands.
  DrawTargetWillChange();
}

SourceSurfaceCapture::~SourceSurfaceCapture()
{
}

bool
SourceSurfaceCapture::IsValid() const
{
  // We must either be able to source a command list, or we must have a cached
  // and rasterized surface.
  MutexAutoLock lock(mLock);
  return (mOwner || mHasCommandList) || mResolved;
}

RefPtr<SourceSurface>
SourceSurfaceCapture::Resolve(BackendType aBackendType)
{
  MutexAutoLock lock(mLock);

  if (!mOwner && !mHasCommandList) {
    // There is no way we can rasterize anything, we don't have a source
    // DrawTarget and we don't have a command list. Return whatever our
    // cached surface is.
    return mResolved;
  }

  BackendType backendType = aBackendType;
  if (backendType == BackendType::NONE) {
    backendType = mRefDT->GetBackendType();
  }

  // If on the paint thread, we require that the owning DrawTarget be detached
  // from this snapshot. This roughly approximates an assert that nothing can
  // mutate the snapshot.
  MOZ_RELEASE_ASSERT(NS_IsMainThread() || !mOwner);

  // Note: SurfaceType is not 1:1 with BackendType, so we can't easily decide
  // that they match. Instead we just cache the first thing to be requested.
  if (!mResolved) {
    mResolved = ResolveImpl(backendType);
  }
  return mResolved;
}

RefPtr<SourceSurface>
SourceSurfaceCapture::ResolveImpl(BackendType aBackendType)
{
  RefPtr<DrawTarget> dt;
  uint8_t* data = nullptr;
  if (!mSurfaceAllocationSize) {
    if (aBackendType == mRefDT->GetBackendType()) {
      dt = mRefDT->CreateSimilarDrawTarget(mSize, mFormat);
    } else {
      dt = Factory::CreateDrawTarget(aBackendType, mSize, mFormat);
    }
  } else {
    data = static_cast<uint8_t*>(calloc(1, mSurfaceAllocationSize));
    if (!data) {
      return nullptr;
    }
    BackendType type = Factory::DoesBackendSupportDataDrawtarget(aBackendType)
                       ? aBackendType
                       : BackendType::SKIA;
    dt = Factory::CreateDrawTargetForData(type, data, mSize, mStride, mFormat);
    if (!dt) {
      free(data);
      return nullptr;
    }
  }

  if (!dt) {
    // Make sure we haven't allocated and aren't leaking something, the code right
    // anove here should have guaranteed that.
    MOZ_ASSERT(!data);
    return nullptr;
  }

  // If we're still attached to a DrawTarget, use its command list rather than
  // our own (which will be empty).
  CaptureCommandList& commands = mHasCommandList
                                 ? mCommands
                                 : mOwner->mCommands;
  for (CaptureCommandList::iterator iter(commands); !iter.Done(); iter.Next()) {
    DrawingCommand* cmd = iter.Get();
    cmd->ExecuteOnDT(dt, nullptr);
  }

  RefPtr<SourceSurface> surf;
  if (!mShouldResolveToLuminance) {
    surf = dt->Snapshot();
  } else {
    surf = dt->IntoLuminanceSource(mLuminanceType, mOpacity);
  }

  if (data) {
    surf->AddUserData(reinterpret_cast<UserDataKey*>(dt.get()), data, free);
  }

  return surf.forget();
}

already_AddRefed<DataSourceSurface>
SourceSurfaceCapture::GetDataSurface()
{
  RefPtr<SourceSurface> surface = Resolve();
  if (!surface) {
    return nullptr;
  }
  return surface->GetDataSurface();
}

void
SourceSurfaceCapture::DrawTargetWillDestroy()
{
  MutexAutoLock lock(mLock);

  // The source DrawTarget is going away, so we can just steal its commands.
  mCommands = std::move(mOwner->mCommands);
  mHasCommandList = true;
  mOwner = nullptr;
}

void
SourceSurfaceCapture::DrawTargetWillChange()
{
  MutexAutoLock lock(mLock);

  for (CaptureCommandList::iterator iter(mOwner->mCommands); !iter.Done(); iter.Next()) {
    DrawingCommand* cmd = iter.Get();
    cmd->CloneInto(&mCommands);
  }

  mHasCommandList = true;
  mOwner = nullptr;
}

} // namespace gfx
} // namespace mozilla
