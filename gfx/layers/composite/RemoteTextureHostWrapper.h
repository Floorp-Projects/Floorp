/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RemoteTextureHostWrapper_H
#define MOZILLA_GFX_RemoteTextureHostWrapper_H

#include "mozilla/layers/TextureHost.h"
#include "mozilla/Monitor.h"

namespace mozilla::layers {

// This class wraps TextureHost of remote texture.
// In sync mode, mRemoteTextureForDisplayList holds TextureHost of mTextureId.
// In async mode, it could be previous remote texture's TextureHost that is
// compatible to the mTextureId's TextureHost.
class RemoteTextureHostWrapper : public TextureHost {
 public:
  static RefPtr<TextureHost> Create(const RemoteTextureId aTextureId,
                                    const RemoteTextureOwnerId aOwnerId,
                                    const base::ProcessId aForPid,
                                    const gfx::IntSize aSize,
                                    const TextureFlags aFlags);

  void DeallocateDeviceData() override {}

  gfx::SurfaceFormat GetFormat() const override;

  already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override {
    return nullptr;
  }

  gfx::YUVColorSpace GetYUVColorSpace() const override;
  gfx::ColorDepth GetColorDepth() const override;
  gfx::ColorRange GetColorRange() const override;

  gfx::IntSize GetSize() const override;

  bool IsValid() override;

#ifdef MOZ_LAYERS_HAVE_LOG
  const char* Name() override { return "RemoteTextureHostWrapper"; }
#endif

  void CreateRenderTexture(
      const wr::ExternalImageId& aExternalImageId) override;

  uint32_t NumSubTextures() override;

  void PushResourceUpdates(wr::TransactionBuilder& aResources,
                           ResourceUpdateOp aOp,
                           const Range<wr::ImageKey>& aImageKeys,
                           const wr::ExternalImageId& aExtID) override;

  void PushDisplayItems(wr::DisplayListBuilder& aBuilder,
                        const wr::LayoutRect& aBounds,
                        const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
                        const Range<wr::ImageKey>& aImageKeys,
                        PushDisplayItemFlagSet aFlags) override;

  bool SupportsExternalCompositing(WebRenderBackend aBackend) override;

  void UnbindTextureSource() override;

  void NotifyNotUsed() override;

  RemoteTextureHostWrapper* AsRemoteTextureHostWrapper() override {
    return this;
  }

  TextureHostType GetTextureHostType() override;

  bool IsWrappingSurfaceTextureHost() override;

  bool NeedsDeferredDeletion() const override;

  AndroidHardwareBuffer* GetAndroidHardwareBuffer() const override;

  bool IsReadyForRendering();

  void ApplyTextureFlagsToRemoteTexture();

  const RemoteTextureId mTextureId;
  const RemoteTextureOwnerId mOwnerId;
  const base::ProcessId mForPid;
  const gfx::IntSize mSize;

 protected:
  RemoteTextureHostWrapper(const RemoteTextureId aTextureId,
                           const RemoteTextureOwnerId aOwnerId,
                           const base::ProcessId aForPid,
                           const gfx::IntSize aSize, const TextureFlags aFlags);
  virtual ~RemoteTextureHostWrapper();

  // Called only by RemoteTextureMap
  TextureHost* GetRemoteTextureHostForDisplayList(
      const MonitorAutoLock& aProofOfLock);
  // Called only by RemoteTextureMap
  void SetRemoteTextureHostForDisplayList(const MonitorAutoLock& aProofOfLock,
                                          TextureHost* aTextureHost,
                                          bool aIsSyncMode);
  void ClearRemoteTextureHostForDisplayList(
      const MonitorAutoLock& aProofOfLock);

  // Updated by RemoteTextureMap
  //
  // Hold compositable ref of remote texture's TextureHost that is used for
  // building WebRender display list. In sync mode, it is TextureHost of
  // mTextureId. In async mode, it could be previous TextureHost that is
  // compatible to the mTextureId's TextureHost.
  CompositableTextureHostRef mRemoteTextureForDisplayList;

  bool mIsSyncMode = true;

  friend class RemoteTextureMap;
};

}  // namespace mozilla::layers

#endif  // MOZILLA_GFX_RemoteTextureHostWrapper_H
