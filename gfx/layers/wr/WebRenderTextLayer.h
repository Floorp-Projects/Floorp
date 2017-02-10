/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef GFX_WEBRENDERTEXTLAYER_H
#define GFX_WEBRENDERTEXTLAYER_H

#include "Layers.h"
#include "WebRenderLayerManager.h"

namespace mozilla {
namespace layers {

class WebRenderTextLayer : public WebRenderLayer,
                           public TextLayer {
public:
    explicit WebRenderTextLayer(WebRenderLayerManager* aLayerManager)
        : TextLayer(aLayerManager, static_cast<WebRenderLayer*>(this))
        , mFontData(nullptr)
        , mFontDataLength(0)
        , mIndex(0)
        , mGlyphSize(0.0)
    {
        MOZ_COUNT_CTOR(WebRenderTextLayer);
    }

protected:
    virtual ~WebRenderTextLayer()
    {
        MOZ_COUNT_DTOR(WebRenderTextLayer);
        if (mFontData) {
            free(mFontData);
        }
    }

public:
  Layer* GetLayer() override { return this; }
  void RenderLayer() override;

  uint8_t* mFontData;
  uint32_t mFontDataLength;
  uint32_t mIndex;
  float mGlyphSize;
};

} // namespace layers
} // namespace mozilla

#endif // GFX_WEBRENDERTEXTLAYER_H