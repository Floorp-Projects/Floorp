/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureRecorded.h"
#include "mozilla/layers/CompositableForwarder.h"

#include "RecordedCanvasEventImpl.h"

namespace mozilla {
namespace layers {

// The texture ID is used in the GPU process both to lookup the real texture in
// the canvas threads and to lookup the SurfaceDescriptor for that texture in
// the compositor thread. It is therefore important that the ID is unique (per
// recording process), otherwise an old descriptor can be picked up. This means
// we can't use the pointer in the recording process as an ID like we do for
// other objects.
static int64_t sNextRecordedTextureId = 0;

RecordedTextureData::RecordedTextureData(
    already_AddRefed<CanvasChild> aCanvasChild, gfx::IntSize aSize,
    gfx::SurfaceFormat aFormat, TextureType aTextureType)
    : mCanvasChild(aCanvasChild), mSize(aSize), mFormat(aFormat) {
  mCanvasChild->EnsureRecorder(aSize, aFormat, aTextureType);
}

RecordedTextureData::~RecordedTextureData() {
  // We need the translator to drop its reference for the DrawTarget first,
  // because the TextureData might need to destroy its DrawTarget within a lock.
  mDT = nullptr;
  mCanvasChild->CleanupTexture(mTextureId);
  mCanvasChild->RecordEvent(
      RecordedTextureDestruction(mTextureId, mLastTxnType, mLastTxnId));
}

void RecordedTextureData::FillInfo(TextureData::Info& aInfo) const {
  aInfo.size = mSize;
  aInfo.format = mFormat;
  aInfo.supportsMoz2D = true;
  aInfo.hasSynchronization = true;
}

void RecordedTextureData::SetRemoteTextureOwnerId(
    RemoteTextureOwnerId aRemoteTextureOwnerId) {
  mRemoteTextureOwnerId = aRemoteTextureOwnerId;
}

void RecordedTextureData::InvalidateContents() { mInvalidContents = true; }

bool RecordedTextureData::Lock(OpenMode aMode) {
  if (!mCanvasChild->EnsureBeginTransaction()) {
    return false;
  }

  if (!mRemoteTextureOwnerId.IsValid()) {
    MOZ_ASSERT(false);
    return false;
  }

  // If modifying the texture, then allocate a new remote texture id.
  if (aMode & OpenMode::OPEN_WRITE) {
    mUsedRemoteTexture = false;
  }

  bool wasInvalidContents = mInvalidContents;
  mInvalidContents = false;

  if (!mDT) {
    mTextureId = sNextRecordedTextureId++;
    mDT = mCanvasChild->CreateDrawTarget(mTextureId, mRemoteTextureOwnerId,
                                         mSize, mFormat);
    if (!mDT) {
      return false;
    }

    // We lock the TextureData when we create it to get the remote DrawTarget.
    mLockedMode = aMode;
    return true;
  }

  mCanvasChild->RecordEvent(
      RecordedTextureLock(mTextureId, aMode, wasInvalidContents));
  mLockedMode = aMode;
  return true;
}

void RecordedTextureData::Unlock() {
  if ((mLockedMode == OpenMode::OPEN_READ_WRITE) &&
      mCanvasChild->ShouldCacheDataSurface()) {
    mSnapshot = mDT->Snapshot();
    mDT->DetachAllSnapshots();
    mCanvasChild->RecordEvent(RecordedCacheDataSurface(mSnapshot.get()));
  }

  mCanvasChild->RecordEvent(RecordedTextureUnlock(mTextureId));

  mLockedMode = OpenMode::OPEN_NONE;
}

already_AddRefed<gfx::DrawTarget> RecordedTextureData::BorrowDrawTarget() {
  mSnapshot = nullptr;
  if (RefPtr<gfx::SourceSurface> wrapper = do_AddRef(mSnapshotWrapper)) {
    mCanvasChild->DetachSurface(wrapper);
    mSnapshotWrapper = nullptr;
  }
  return do_AddRef(mDT);
}

void RecordedTextureData::EndDraw() {
  MOZ_ASSERT(mDT->hasOneRef());
  MOZ_ASSERT(mLockedMode == OpenMode::OPEN_READ_WRITE);

  if (mCanvasChild->ShouldCacheDataSurface()) {
    mSnapshot = mDT->Snapshot();
    mCanvasChild->RecordEvent(RecordedCacheDataSurface(mSnapshot.get()));
  }
}

already_AddRefed<gfx::SourceSurface> RecordedTextureData::BorrowSnapshot() {
  if (RefPtr<gfx::SourceSurface> wrapper = do_AddRef(mSnapshotWrapper)) {
    return wrapper.forget();
  }

  // There are some failure scenarios where we have no DrawTarget and
  // BorrowSnapshot is called in an attempt to copy to a new texture.
  if (!mDT) {
    return nullptr;
  }

  RefPtr<gfx::SourceSurface> wrapper = mCanvasChild->WrapSurface(
      mSnapshot ? mSnapshot : mDT->Snapshot(), mTextureId);
  mSnapshotWrapper = wrapper;
  return wrapper.forget();
}

void RecordedTextureData::ReturnSnapshot(
    already_AddRefed<gfx::SourceSurface> aSnapshot) {
  RefPtr<gfx::SourceSurface> snapshot = aSnapshot;
  if (RefPtr<gfx::SourceSurface> wrapper = do_AddRef(mSnapshotWrapper)) {
    mCanvasChild->DetachSurface(wrapper);
  }
}

void RecordedTextureData::Deallocate(LayersIPCChannel* aAllocator) {}

bool RecordedTextureData::Serialize(SurfaceDescriptor& aDescriptor) {
  if (!mRemoteTextureOwnerId.IsValid()) {
    MOZ_ASSERT_UNREACHABLE("Missing remote texture owner id!");
    return false;
  }

  // If something is querying the id, assume it is going to be composited.
  if (!mUsedRemoteTexture) {
    mLastRemoteTextureId = RemoteTextureId::GetNext();
    mCanvasChild->RecordEvent(
        RecordedPresentTexture(mTextureId, mLastRemoteTextureId));
    mUsedRemoteTexture = true;
  } else if (!mLastRemoteTextureId.IsValid()) {
    MOZ_ASSERT_UNREACHABLE("Missing remote texture id!");
    return false;
  }

  aDescriptor = SurfaceDescriptorRemoteTexture(mLastRemoteTextureId,
                                               mRemoteTextureOwnerId);
  return true;
}

void RecordedTextureData::UseCompositableForwarder(
    CompositableForwarder* aForwarder) {
  mLastTxnType = (RemoteTextureTxnType)aForwarder->GetFwdTransactionType();
  mLastTxnId = (RemoteTextureTxnId)aForwarder->GetFwdTransactionId();
}

void RecordedTextureData::OnForwardedToHost() {
  // Compositing with RecordedTextureData requires RemoteTextureMap.
  MOZ_CRASH("OnForwardedToHost not supported!");
}

TextureFlags RecordedTextureData::GetTextureFlags() const {
  // With WebRender, resource open happens asynchronously on RenderThread.
  // Use WAIT_HOST_USAGE_END to keep TextureClient alive during host side usage.
  return TextureFlags::WAIT_HOST_USAGE_END;
}

bool RecordedTextureData::RequiresRefresh() const {
  return mCanvasChild->RequiresRefresh(mTextureId);
}

}  // namespace layers
}  // namespace mozilla
