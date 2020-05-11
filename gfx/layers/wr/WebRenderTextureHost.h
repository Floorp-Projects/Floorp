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
  WebRenderTextureHost(const SurfaceDescriptor& aDesc, TextureFlags aFlags,
                       TextureHost* aTexture,
                       wr::ExternalImageId& aExternalImageId);
  virtual ~WebRenderTextureHost();

  void DeallocateDeviceData() override {}

  bool Lock() override;

  void Unlock() override;

  void PrepareTextureSource(CompositableTextureSourceRef& aTexture) override;
  bool BindTextureSource(CompositableTextureSourceRef& aTexture) override;
  void UnbindTextureSource() override;
  void SetTextureSourceProvider(TextureSourceProvider* aProvider) override;

  gfx::SurfaceFormat GetFormat() const override;

  virtual void NotifyNotUsed() override;

  // Return the format used for reading the texture. Some hardware specific
  // textureHosts use their special data representation internally, but we could
  // treat these textureHost as the read-format when we read them.
  // Please check TextureHost::GetReadFormat().
  gfx::SurfaceFormat GetReadFormat() const override;

  already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override;

  gfx::YUVColorSpace GetYUVColorSpace() const override;
  gfx::ColorRange GetColorRange() const override;

  gfx::IntSize GetSize() const override;

#ifdef MOZ_LAYERS_HAVE_LOG
  const char* Name() override { return "WebRenderTextureHost"; }
#endif

  WebRenderTextureHost* AsWebRenderTextureHost() override { return this; }

  virtual void PrepareForUse() override;

  wr::ExternalImageId GetExternalImageKey() { return mExternalImageId; }

  int32_t GetRGBStride();

  bool HasIntermediateBuffer() const override;

  uint32_t NumSubTextures() override;

  void PushResourceUpdates(wr::TransactionBuilder& aResources,
                           ResourceUpdateOp aOp,
                           const Range<wr::ImageKey>& aImageKeys,
                           const wr::ExternalImageId& aExtID) override;

  void PushDisplayItems(wr::DisplayListBuilder& aBuilder,
                        const wr::LayoutRect& aBounds,
                        const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
                        const Range<wr::ImageKey>& aImageKeys,
                        const bool aPreferCompositorSurface) override;

  bool NeedsYFlip() const override;

  void MaybeNofityForUse(wr::TransactionBuilder& aTxn);

 protected:
  void CreateRenderTextureHost(const SurfaceDescriptor& aDesc,
                               TextureHost* aTexture);

  RefPtr<TextureHost> mWrappedTextureHost;
  wr::ExternalImageId mExternalImageId;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_WEBRENDERTEXTUREHOST_H
