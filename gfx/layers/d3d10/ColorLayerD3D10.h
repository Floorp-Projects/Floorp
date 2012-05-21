/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_COLORLAYERD3D10_H
#define GFX_COLORLAYERD3D10_H

#include "LayerManagerD3D10.h"

namespace mozilla {
namespace layers {

class ColorLayerD3D10 : public ColorLayer,
                        public LayerD3D10
{
public:
  ColorLayerD3D10(LayerManagerD3D10 *aManager);

  /* LayerD3D10 implementation */
  virtual Layer* GetLayer();
  virtual void RenderLayer();
};

} /* layers */
} /* mozilla */
#endif /* GFX_THEBESLAYERD3D10_H */
