/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureRecorded.h"

#include "RecordedCanvasEventImpl.h"

namespace mozilla {
namespace layers {

RecordedTextureData::RecordedTextureData(
    already_AddRefed<CanvasChild> aCanvasChild, gfx::IntSize aSize,
    gfx::SurfaceFormat aFormat, TextureType aTextureType)
    : mCanvasChild(aCanvasChild), mSize(aSize), mFormat(aFormat) {
  mCanvasChild->EnsureRecorder(aTextureType);
}

RecordedTextureData::~RecordedTextureData() {}

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
    mDT = mCanvasChild->CreateDrawTarget(mSize, mFormat);

    // We lock the TextureData when we create it to get the remote DrawTarget.
    mCanvasChild->OnTextureWriteLock();
    return true;
  }

  mCanvasChild->RecordEvent(RecordedTextureLock(mDT.get(), aMode));
  if (aMode & OpenMode::OPEN_WRITE) {
    mCanvasChild->OnTextureWriteLock();
    mSnapshot = nullptr;
  }
  return true;
}

void RecordedTextureData::Unlock() {
  mCanvasChild->RecordEvent(RecordedTextureUnlock(mDT.get()));
}

already_AddRefed<gfx::DrawTarget> RecordedTextureData::BorrowDrawTarget() {
  return do_AddRef(mDT);
}

already_AddRefed<gfx::SourceSurface> RecordedTextureData::BorrowSnapshot() {
  MOZ_ASSERT(mDT);

  mSnapshot = mDT->Snapshot();
  return mCanvasChild->WrapSurface(mSnapshot);
}

void RecordedTextureData::Deallocate(LayersIPCChannel* aAllocator) {}

bool RecordedTextureData::Serialize(SurfaceDescriptor& aDescriptor) {
  SurfaceDescriptorRecorded descriptor;
  descriptor.drawTarget() = reinterpret_cast<uintptr_t>(mDT.get());
  aDescriptor = std::move(descriptor);
  return true;
}

void RecordedTextureData::OnForwardedToHost() {
  mCanvasChild->OnTextureForwarded();
  if (mSnapshot && mCanvasChild->ShouldCacheDataSurface()) {
    mCanvasChild->RecordEvent(RecordedCacheDataSurface(mSnapshot.get()));
  }
}

}  // namespace layers
}  // namespace mozilla
