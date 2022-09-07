/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RemoteTextureHostWrapper_H
#define MOZILLA_GFX_RemoteTextureHostWrapper_H

#include "mozilla/layers/TextureHost.h"
#include "mozilla/Mutex.h"

namespace mozilla::layers {

// Current implementation expects the folowing prefs.
// - gfx.canvas.accelerated.async-present = true or
//   webgl.out-of-process.async-present = true
// - webgl.out-of-process.async-present.force-sync = true
//
// This class wraps TextureHost of remote texture.
// It notifies end of WebRender usage to the wrapped remote texture.
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

  void MaybeDestroyRenderTexture() override;

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

  bool CheckIsReadyForRendering();

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
  TextureHost* GetRemoteTextureHost(const MutexAutoLock& aProofOfLock);
  // Called only by RemoteTextureMap
  void SetRemoteTextureHost(const MutexAutoLock& aProofOfLock,
                            TextureHost* aTextureHost);

  // Updated by RemoteTextureMap
  CompositableTextureHostRef mRemoteTextureHost;

  friend class RemoteTextureMap;
};

}  // namespace mozilla::layers

#endif  // MOZILLA_GFX_RemoteTextureHostWrapper_H
