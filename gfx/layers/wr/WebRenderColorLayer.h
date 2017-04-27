/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERCOLORLAYER_H
#define GFX_WEBRENDERCOLORLAYER_H

#include "Layers.h"
#include "mozilla/layers/WebRenderLayer.h"
#include "mozilla/layers/WebRenderLayerManager.h"

namespace mozilla {
namespace layers {

class WebRenderColorLayer : public WebRenderLayer,
                            public ColorLayer {
public:
  explicit WebRenderColorLayer(WebRenderLayerManager* aLayerManager)
    : ColorLayer(aLayerManager, static_cast<WebRenderLayer*>(this))
  {
    MOZ_COUNT_CTOR(WebRenderColorLayer);
  }

protected:
  virtual ~WebRenderColorLayer()
  {
    MOZ_COUNT_DTOR(WebRenderColorLayer);
  }

public:
  Layer* GetLayer() override { return this; }
  void RenderLayer(wr::DisplayListBuilder& aBuilder) override;
};

} // namespace layers
} // namespace mozilla

#endif // GFX_WEBRENDERCOLORLAYER_H
