/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_READBACKLAYERD3D10_H
#define GFX_READBACKLAYERD3D10_H

#include "LayerManagerD3D10.h"
#include "ReadbackLayer.h"

namespace mozilla {
namespace layers {

class ReadbackLayerD3D10 :
  public ReadbackLayer,
  public LayerD3D10
{
public:
    ReadbackLayerD3D10(LayerManagerD3D10 *aManager)
    : ReadbackLayer(aManager, nullptr),
      LayerD3D10(aManager)
  {
      mImplData = static_cast<LayerD3D10*>(this);
  }

  virtual Layer* GetLayer() { return this; }
  virtual void RenderLayer() {}
};

} /* layers */
} /* mozilla */
#endif /* GFX_READBACKLAYERD3D10_H */
