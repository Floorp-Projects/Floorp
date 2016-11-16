/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERCANVASLAYER_H
#define GFX_WEBRENDERCANVASLAYER_H

#include "CopyableCanvasLayer.h"
#include "WebRenderLayerManager.h"

namespace mozilla {
namespace gfx {
class SourceSurface;
}; // namespace gfx

namespace layers {

class WebRenderCanvasLayer : public WebRenderLayer,
                             public CopyableCanvasLayer
{
public:
  explicit WebRenderCanvasLayer(WebRenderLayerManager* aLayerManager)
    : CopyableCanvasLayer(aLayerManager, static_cast<WebRenderLayer*>(this))
  {
    MOZ_COUNT_CTOR(WebRenderCanvasLayer);
  }

protected:
  virtual ~WebRenderCanvasLayer();
  WebRenderLayerManager* Manager()
  {
    return static_cast<WebRenderLayerManager*>(mManager);
  }

public:
  Layer* GetLayer() override { return this; }
  void RenderLayer() override;
};

} // namespace layers
} // namespace mozilla

#endif // GFX_WEBRENDERCANVASLAYER_H
