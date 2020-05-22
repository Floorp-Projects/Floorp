/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERTEXTUREHOSTWRAPPER_H
#define MOZILLA_GFX_RENDERTEXTUREHOSTWRAPPER_H

#include "RenderTextureHost.h"

namespace mozilla {

namespace wr {

/**
 * RenderTextureHost of GPUVideoTextureHost.
 *
 * GPUVideoTextureHost wraps TextureHost. This class wraps RenderTextureHost of
 * the wrapped TextureHost. Lifetime of the wrapped TextureHost is usually
 * longer than GPUVideoTextureHost and the wrapped TextureHost is used by
 * multiple GPUVideoTextureHosts. This class is used to reduce recreations of
 * the wrappded RenderTextureHost. Initializations of some
 * RenderTextureHosts(RenderDXGITextureHostOGL and
 * RenderDXGIYCbCrTextureHostOGL) have overhead.
 */
class RenderTextureHostWrapper final : public RenderTextureHost {
 public:
  explicit RenderTextureHostWrapper(ExternalImageId aExternalImageId);

  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL,
                           wr::ImageRendering aRendering) override;
  void Unlock() override;
  void ClearCachedResources() override;

 private:
  ~RenderTextureHostWrapper() override;

  const ExternalImageId mExternalImageId;
  RefPtr<RenderTextureHost> mTextureHost;
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDERTEXTUREHOSTWRAPPER_H
