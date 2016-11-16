/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERIMAGELAYER_H
#define GFX_WEBRENDERIMAGELAYER_H

#include "ImageLayers.h"
#include "WebRenderLayerManager.h"

namespace mozilla {
namespace layers {

class WebRenderImageLayer : public WebRenderLayer,
                            public ImageLayer {
public:
  explicit WebRenderImageLayer(WebRenderLayerManager* aLayerManager)
    : ImageLayer(aLayerManager, static_cast<WebRenderLayer*>(this))
  {
    MOZ_COUNT_CTOR(WebRenderImageLayer);
  }

  virtual already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;
protected:
  virtual ~WebRenderImageLayer()
  {
    MOZ_COUNT_DTOR(WebRenderImageLayer);
  }

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

#endif // GFX_WEBRENDERIMAGELAYER_H
