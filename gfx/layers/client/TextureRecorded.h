/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_TextureRecorded_h
#define mozilla_layers_TextureRecorded_h

#include "TextureClient.h"
#include "mozilla/Mutex.h"
#include "mozilla/layers/LayersTypes.h"

namespace mozilla::layers {
class CanvasChild;
class CanvasDrawEventRecorder;

class RecordedTextureData final : public TextureData {
 public:
  RecordedTextureData(gfx::IntSize aSize, gfx::SurfaceFormat aFormat);

  bool Init(TextureType aTextureType);

  void DestroyOnOwningThread();

  void FillInfo(TextureData::Info& aInfo) const final;

  bool Lock(OpenMode aMode) final;

  void Unlock() final;

  already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() final;

  void EndDraw() final;

  already_AddRefed<gfx::SourceSurface> BorrowSnapshot() final;

  void ReturnSnapshot(already_AddRefed<gfx::SourceSurface> aSnapshot) final;

  void Forget(LayersIPCChannel* aAllocator) final { Deallocate(aAllocator); }

  void Deallocate(LayersIPCChannel* aAllocator) final;

  bool Serialize(SurfaceDescriptor& aDescriptor) final;

  void OnForwardedToHost() final;

  TextureFlags GetTextureFlags() const final;

  void SetRemoteTextureOwnerId(
      RemoteTextureOwnerId aRemoteTextureOwnerId) final;

  bool RequiresRefresh() const final;

 private:
  friend class TextureData;

  DISALLOW_COPY_AND_ASSIGN(RecordedTextureData);

  ~RecordedTextureData() override;

  void DestroyOnOwningThreadLocked() MOZ_REQUIRES(mMutex);

  int64_t mTextureId = 0;

  Mutex mMutex;
  RefPtr<CanvasChild> mCanvasChild;
  RefPtr<CanvasDrawEventRecorder> mRecorder MOZ_GUARDED_BY(mMutex);
  RefPtr<gfx::DrawTarget> mDT;
  RefPtr<gfx::SourceSurface> mSnapshot;
  ThreadSafeWeakPtr<gfx::SourceSurface> mSnapshotWrapper;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  OpenMode mLockedMode = OpenMode::OPEN_NONE;
  layers::RemoteTextureId mLastRemoteTextureId;
  layers::RemoteTextureOwnerId mRemoteTextureOwnerId;
};

}  // namespace mozilla::layers

#endif  // mozilla_layers_TextureRecorded_h
