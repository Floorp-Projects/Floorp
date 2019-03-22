/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SHAREABLECANVASRENDERER_H
#define GFX_SHAREABLECANVASRENDERER_H

#include "CompositorTypes.h"
#include "CopyableCanvasRenderer.h"
#include "mozilla/layers/CanvasClient.h"

namespace mozilla {
namespace gl {
class SurfaceFactory;
}  // namespace gl

namespace gfx {
class DrawTarget;
}  // namespace gfx

namespace layers {

class ShareableCanvasRenderer : public CopyableCanvasRenderer {
  typedef CanvasClient::CanvasClientType CanvasClientType;

 public:
  ShareableCanvasRenderer();
  virtual ~ShareableCanvasRenderer();

 public:
  void Initialize(const CanvasInitializeData& aData) override;

  virtual CompositableForwarder* GetForwarder() = 0;

  virtual bool CreateCompositable() = 0;

  void ClearCachedResources() override;
  void Destroy() override;

  void UpdateCompositableClient(
      wr::RenderRoot aRenderRoot = wr::RenderRoot::Default);

  const TextureFlags& Flags() const { return mFlags; }

  CanvasClient* GetCanvasClient() { return mCanvasClient; }

 protected:
  bool UpdateTarget(gfx::DrawTarget* aDestTarget);

  CanvasClientType GetCanvasClientType();

  RefPtr<CanvasClient> mCanvasClient;

  UniquePtr<gl::SurfaceFactory> mFactory;

  TextureFlags mFlags;

  friend class CanvasClient2D;
  friend class CanvasClientSharedSurface;
};

}  // namespace layers
}  // namespace mozilla

#endif
