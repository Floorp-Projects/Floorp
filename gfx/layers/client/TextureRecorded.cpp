/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureRecorded.h"

#include "mozilla/gfx/gfxVars.h"
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
  mCanvasChild->RecordEvent(RecordedTextureDestruction(mTextureId));
}

void RecordedTextureData::FillInfo(TextureData::Info& aInfo) const {
  aInfo.size = mSize;
  aInfo.format = mFormat;
  aInfo.supportsMoz2D = true;
  aInfo.hasIntermediateBuffer = false;
  aInfo.hasSynchronization = true;
}

bool RecordedTextureData::Lock(OpenMode aMode) {
  mCanvasChild->EnsureBeginTransaction();
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
    mSnapshot = nullptr;
  }
  mLockedMode = aMode;
  return true;
}

void RecordedTextureData::Unlock() {
  if ((mLockedMode & OpenMode::OPEN_WRITE) &&
      mCanvasChild->ShouldCacheDataSurface()) {
    mSnapshot = mDT->Snapshot();
    mDT->DetachAllSnapshots();
  }

  mCanvasChild->RecordEvent(RecordedTextureUnlock(mTextureId));
  mLockedMode = OpenMode::OPEN_NONE;
}

already_AddRefed<gfx::DrawTarget> RecordedTextureData::BorrowDrawTarget() {
  return do_AddRef(mDT);
}

already_AddRefed<gfx::SourceSurface> RecordedTextureData::BorrowSnapshot() {
  MOZ_ASSERT(mDT);

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
  if (mSnapshot && mCanvasChild->ShouldCacheDataSurface()) {
    mCanvasChild->RecordEvent(RecordedCacheDataSurface(mSnapshot.get()));
  }
}

TextureFlags RecordedTextureData::GetTextureFlags() const {
  TextureFlags flags = TextureFlags::NO_FLAGS;
  // With WebRender, resource open happens asynchronously on RenderThread.
  // Use WAIT_HOST_USAGE_END to keep TextureClient alive during host side usage.
  if (gfx::gfxVars::UseWebRender()) {
    flags |= TextureFlags::WAIT_HOST_USAGE_END;
  }
  return flags;
}

}  // namespace layers
}  // namespace mozilla
