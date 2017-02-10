/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERBORDERLAYER_H
#define GFX_WEBRENDERBORDERLAYER_H

#include "Layers.h"
#include "WebRenderLayerManager.h"

namespace mozilla {
namespace layers {

class WebRenderBorderLayer : public WebRenderLayer,
                             public BorderLayer {
public:
  explicit WebRenderBorderLayer(WebRenderLayerManager* aLayerManager)
    : BorderLayer(aLayerManager, static_cast<WebRenderLayer*>(this))
  {
    MOZ_COUNT_CTOR(WebRenderBorderLayer);
  }

protected:
  virtual ~WebRenderBorderLayer()
  {
    MOZ_COUNT_DTOR(WebRenderBorderLayer);
  }

public:
  Layer* GetLayer() override { return this; }
  void RenderLayer() override;
};

} // namespace layers
} // namespace mozilla

#endif // GFX_WEBRENDERBORDERLAYER_H
