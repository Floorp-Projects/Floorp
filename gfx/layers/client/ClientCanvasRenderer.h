/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CLIENTCANVASRENDERER_H
#define GFX_CLIENTCANVASRENDERER_H

#include "ShareableCanvasRenderer.h"

namespace mozilla {
namespace layers {

class ClientCanvasLayer;
class ClientCanvasRenderer : public ShareableCanvasRenderer
{
public:
  explicit ClientCanvasRenderer(ClientCanvasLayer* aLayer)
    : mLayer(aLayer)
  { }

  ClientCanvasRenderer* AsClientCanvasRenderer() override { return this; }

  CompositableForwarder* GetForwarder() override;

  bool CreateCompositable() override;

protected:
  ClientCanvasLayer* mLayer;
};

} // namespace layers
} // namespace mozilla

#endif
