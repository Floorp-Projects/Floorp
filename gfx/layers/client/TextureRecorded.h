/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_TextureRecorded_h
#define mozilla_layers_TextureRecorded_h

#include "TextureClient.h"
#include "mozilla/layers/CanvasChild.h"
#include "mozilla/layers/LayersTypes.h"

namespace mozilla {
namespace layers {

class RecordedTextureData final : public TextureData {
 public:
  RecordedTextureData(already_AddRefed<CanvasChild> aCanvasChild,
                      gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                      TextureType aTextureType);

  void FillInfo(TextureData::Info& aInfo) const final;

  void InvalidateContents() final;

  bool Lock(OpenMode aMode) final;

  void Unlock() final;

  already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() final;

  void EndDraw() final;

  already_AddRefed<gfx::SourceSurface> BorrowSnapshot() final;

  void ReturnSnapshot(already_AddRefed<gfx::SourceSurface> aSnapshot) final;

  void Deallocate(LayersIPCChannel* aAllocator) final;

  bool Serialize(SurfaceDescriptor& aDescriptor) final;

  void OnForwardedToHost() final;

  TextureFlags GetTextureFlags() const final;

  void SetRemoteTextureOwnerId(
      RemoteTextureOwnerId aRemoteTextureOwnerId) final;

  bool RequiresRefresh() const final;

  void UseCompositableForwarder(CompositableForwarder* aForwarder) final;

 private:
  DISALLOW_COPY_AND_ASSIGN(RecordedTextureData);

  ~RecordedTextureData() override;

  int64_t mTextureId;
  RefPtr<CanvasChild> mCanvasChild;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  RefPtr<gfx::DrawTarget> mDT;
  RefPtr<gfx::SourceSurface> mSnapshot;
  ThreadSafeWeakPtr<gfx::SourceSurface> mSnapshotWrapper;
  OpenMode mLockedMode;
  RemoteTextureId mLastRemoteTextureId;
  RemoteTextureOwnerId mRemoteTextureOwnerId;
  RemoteTextureTxnType mLastTxnType = 0;
  RemoteTextureTxnId mLastTxnId = 0;
  bool mUsedRemoteTexture = false;
  bool mInvalidContents = true;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_TextureRecorded_h
