/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERCANVASLAYER_H
#define GFX_WEBRENDERCANVASLAYER_H

#include "ShareableCanvasLayer.h"
#include "WebRenderLayerManager.h"

namespace mozilla {
namespace gfx {
class SourceSurface;
}; // namespace gfx

namespace layers {

class WebRenderCanvasLayer : public WebRenderLayer,
                             public ShareableCanvasLayer
{
public:
  explicit WebRenderCanvasLayer(WebRenderLayerManager* aLayerManager)
    : ShareableCanvasLayer(aLayerManager, static_cast<WebRenderLayer*>(this))
    , mExternalImageId(0)
  {
    MOZ_COUNT_CTOR(WebRenderCanvasLayer);
  }

  virtual void Initialize(const Data& aData) override;

  virtual CompositableForwarder* GetForwarder() override
  {
    return Manager()->WrBridge();
  }

  virtual void AttachCompositable() override;

protected:
  virtual ~WebRenderCanvasLayer();
  WebRenderLayerManager* Manager()
  {
    return static_cast<WebRenderLayerManager*>(mManager);
  }

public:
  Layer* GetLayer() override { return this; }
  void RenderLayer() override;

protected:
  uint64_t mExternalImageId;
};

} // namespace layers
} // namespace mozilla

#endif // GFX_WEBRENDERCANVASLAYER_H
