/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
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
// no Compositor during composition. Instead, we use RendererOGL for composition.
// So, there are some UNREACHABLE asserts for the original Compositor related
// code path in this class. Furthermore, the RendererOGL runs at RenderThead
// instead of Compositor thread. This class is also creating the corresponding
// RenderXXXTextureHost used by RendererOGL at RenderThread.
class WebRenderTextureHost : public TextureHost
{
public:
  WebRenderTextureHost(const SurfaceDescriptor& aDesc,
                       TextureFlags aFlags,
                       TextureHost* aTexture,
                       wr::ExternalImageId& aExternalImageId);
  virtual ~WebRenderTextureHost();

  virtual void DeallocateDeviceData() override {}

  virtual void SetTextureSourceProvider(TextureSourceProvider* aProvider) override;

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual gfx::SurfaceFormat GetFormat() const override;

  // Return the format used for reading the texture. Some hardware specific
  // textureHosts use their special data representation internally, but we could
  // treat these textureHost as the read-format when we read them.
  // Please check TextureHost::GetReadFormat().
  virtual gfx::SurfaceFormat GetReadFormat() const override;

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override;

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override;

  virtual YUVColorSpace GetYUVColorSpace() const override;

  virtual gfx::IntSize GetSize() const override;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() override { return "WebRenderTextureHost"; }
#endif

  virtual WebRenderTextureHost* AsWebRenderTextureHost() override { return this; }

  wr::ExternalImageId GetExternalImageKey() { return mExternalImageId; }

  int32_t GetRGBStride();

  bool IsWrappingNativeHandle() { return mIsWrappingNativeHandle; }

  virtual void GetWRImageKeys(nsTArray<wr::ImageKey>& aImageKeys,
                              const std::function<wr::ImageKey()>& aImageKeyAllocator) override;

  virtual void AddWRImage(wr::WebRenderAPI* aAPI,
                          Range<const wr::ImageKey>& aImageKeys,
                          const wr::ExternalImageId& aExtID) override;

  virtual void PushExternalImage(wr::DisplayListBuilder& aBuilder,
                                 const wr::WrRect& aBounds,
                                 const wr::WrRect& aClip,
                                 wr::ImageRendering aFilter,
                                 Range<const wr::ImageKey>& aImageKeys) override;

protected:
  void CreateRenderTextureHost(const SurfaceDescriptor& aDesc, TextureHost* aTexture);

  RefPtr<TextureHost> mWrappedTextureHost;
  wr::ExternalImageId mExternalImageId;

  bool mIsWrappingNativeHandle;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_WEBRENDERTEXTUREHOST_H
