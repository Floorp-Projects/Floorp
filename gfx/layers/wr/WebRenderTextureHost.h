/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_WEBRENDERTEXTUREHOST_H
#define MOZILLA_GFX_WEBRENDERTEXTUREHOST_H

#include "mozilla/layers/TextureHost.h"

namespace mozilla {
namespace layers {

class WebRenderTextureHost : public TextureHost
{
public:
  WebRenderTextureHost(TextureFlags aFlags,
                       TextureHost* aTexture);
  virtual ~WebRenderTextureHost();

  virtual void DeallocateDeviceData() override {}

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual Compositor* GetCompositor() override;

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual gfx::SurfaceFormat GetFormat() const override;

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override;

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override;

  virtual YUVColorSpace GetYUVColorSpace() const override;

  virtual gfx::IntSize GetSize() const override;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() override { return "WebRenderTextureHost"; }
#endif

  virtual WebRenderTextureHost* AsWebRenderTextureHost() override { return this; }

  uint64_t GetExternalImageKey() { return mExternalImageId; }
protected:
  RefPtr<TextureHost> mWrappedTextureHost;
  uint64_t mExternalImageId;

  static uint64_t sSerialCounter;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_WEBRENDERTEXTUREHOST_H
