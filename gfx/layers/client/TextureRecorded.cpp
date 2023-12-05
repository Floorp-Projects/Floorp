/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureRecorded.h"

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
  mCanvasChild->EnsureRecorder(aTextureType);
}

RecordedTextureData::~RecordedTextureData() {
  // We need the translator to drop its reference for the DrawTarget first,
  // because the TextureData might need to destroy its DrawTarget within a lock.
  mDT = nullptr;
  mCanvasChild->RecordEvent(RecordedTextureDestruction(mTextureId));
}

void RecordedTextureData::FillInfo(TextureData::Info& aInfo) const {
  aInfo.size = mSize;
  aInfo.format = mFormat;
  aInfo.supportsMoz2D = true;
  aInfo.hasSynchronization = true;
}

bool RecordedTextureData::Lock(OpenMode aMode) {
  if (!mCanvasChild->EnsureBeginTransaction()) {
    return false;
  }

  if (!mDT) {
    mTextureId = sNextRecordedTextureId++;
    mCanvasChild->RecordEvent(RecordedNextTextureId(mTextureId));
    mDT = mCanvasChild->CreateDrawTarget(mSize, mFormat);
    if (!mDT) {
      return false;
    }

    // We lock the TextureData when we create it to get the remote DrawTarget.
    mCanvasChild->OnTextureWriteLock();
    mLockedMode = aMode;
    return true;
  }

  mCanvasChild->RecordEvent(RecordedTextureLock(mTextureId, aMode));
  if (aMode & OpenMode::OPEN_WRITE) {
    mCanvasChild->OnTextureWriteLock();
  }
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
  // There are some failure scenarios where we have no DrawTarget and
  // BorrowSnapshot is called in an attempt to copy to a new texture.
  if (!mDT) {
    return nullptr;
  }

  if (mSnapshot) {
    return mCanvasChild->WrapSurface(mSnapshot);
  }

  return mCanvasChild->WrapSurface(mDT->Snapshot());
}

void RecordedTextureData::Deallocate(LayersIPCChannel* aAllocator) {}

bool RecordedTextureData::Serialize(SurfaceDescriptor& aDescriptor) {
  aDescriptor = SurfaceDescriptorRecorded(mTextureId);
  return true;
}

void RecordedTextureData::OnForwardedToHost() {
  mCanvasChild->OnTextureForwarded();
}

TextureFlags RecordedTextureData::GetTextureFlags() const {
  // With WebRender, resource open happens asynchronously on RenderThread.
  // Use WAIT_HOST_USAGE_END to keep TextureClient alive during host side usage.
  return TextureFlags::WAIT_HOST_USAGE_END;
}

}  // namespace layers
}  // namespace mozilla
