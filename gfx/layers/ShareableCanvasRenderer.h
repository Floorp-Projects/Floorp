/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SHAREABLECANVASRENDERER_H
#define GFX_SHAREABLECANVASRENDERER_H

#include "CompositorTypes.h"
#include "CanvasRenderer.h"
#include "mozilla/layers/CanvasClient.h"

namespace mozilla {
namespace gl {
class SurfaceFactory;
}  // namespace gl

namespace gfx {
class DrawTarget;
}  // namespace gfx

namespace layers {

class ShareableCanvasRenderer : public CanvasRenderer {
  friend class CanvasClient2D;
  friend class CanvasClientSharedSurface;

 protected:
  RefPtr<CanvasClient> mCanvasClient;

 private:
  layers::SurfaceDescriptor mFrontBufferDesc;
  RefPtr<TextureClient> mFrontBufferFromDesc;

 public:
  ShareableCanvasRenderer();
  virtual ~ShareableCanvasRenderer();

 public:
  void Initialize(const CanvasRendererData&) override;

  virtual CompositableForwarder* GetForwarder() = 0;

  virtual bool CreateCompositable() = 0;
  virtual void EnsurePipeline() = 0;
  virtual bool HasPipeline() { return false; };

  void ClearCachedResources() override;
  void DisconnectClient() override;

  void UpdateCompositableClient();

  CanvasClient* GetCanvasClient() { return mCanvasClient; }

 private:
  RefPtr<TextureClient> GetFrontBufferFromDesc(const layers::SurfaceDescriptor&,
                                               TextureFlags);
};

}  // namespace layers
}  // namespace mozilla

#endif
