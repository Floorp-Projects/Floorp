/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_WEBRENDERTEXTUREHOST_H
#define MOZILLA_GFX_WEBRENDERTEXTUREHOST_H

#include "mozilla/layers/TextureHost.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace layers {

class SurfaceDescriptor;

// This textureHost is specialized for WebRender usage. With WebRender, there is
// no Compositor during composition. Instead, we use RendererOGL for
// composition. So, there are some UNREACHABLE asserts for the original
// Compositor related code path in this class. Furthermore, the RendererOGL runs
// at RenderThead instead of Compositor thread. This class is also creating the
// corresponding RenderXXXTextureHost used by RendererOGL at RenderThread.
class WebRenderTextureHost : public TextureHost {
 public:
  WebRenderTextureHost(TextureFlags aFlags, TextureHost* aTexture,
                       const wr::ExternalImageId& aExternalImageId);
  virtual ~WebRenderTextureHost();

  void DeallocateDeviceData() override {}

  void UnbindTextureSource() override;

  gfx::SurfaceFormat GetFormat() const override;

  virtual void NotifyNotUsed() override;

  virtual bool IsValid() override;

  // Return the format used for reading the texture. Some hardware specific
  // textureHosts use their special data representation internally, but we could
  // treat these textureHost as the read-format when we read them.
  // Please check TextureHost::GetReadFormat().
  gfx::SurfaceFormat GetReadFormat() const override;

  already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override;

  gfx::ColorDepth GetColorDepth() const override;
  gfx::YUVColorSpace GetYUVColorSpace() const override;
  gfx::ColorRange GetColorRange() const override;

  gfx::IntSize GetSize() const override;

#ifdef MOZ_LAYERS_HAVE_LOG
  const char* Name() override { return "WebRenderTextureHost"; }
#endif

  void MaybeDestroyRenderTexture() override;

  WebRenderTextureHost* AsWebRenderTextureHost() override { return this; }

  RemoteTextureHostWrapper* AsRemoteTextureHostWrapper() override {
    return mWrappedTextureHost->AsRemoteTextureHostWrapper();
  }

  BufferTextureHost* AsBufferTextureHost() override {
    return mWrappedTextureHost->AsBufferTextureHost();
  }

  bool IsWrappingSurfaceTextureHost() override;

  virtual void PrepareForUse() override;

  wr::ExternalImageId GetExternalImageKey();

  int32_t GetRGBStride();

  bool NeedsDeferredDeletion() const override;

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

  void SetAcquireFence(mozilla::ipc::FileDescriptor&& aFenceFd) override;

  void SetReleaseFence(mozilla::ipc::FileDescriptor&& aFenceFd) override;

  mozilla::ipc::FileDescriptor GetAndResetReleaseFence() override;

  AndroidHardwareBuffer* GetAndroidHardwareBuffer() const override;

  void MaybeNotifyForUse(wr::TransactionBuilder& aTxn);

  TextureHostType GetTextureHostType() override;

  const RefPtr<TextureHost> mWrappedTextureHost;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_WEBRENDERTEXTUREHOST_H
