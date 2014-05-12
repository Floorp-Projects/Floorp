/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICCANVASLAYER_H
#define GFX_BASICCANVASLAYER_H

#include "BasicImplData.h"              // for BasicImplData
#include "BasicLayers.h"                // for BasicLayerManager
#include "CopyableCanvasLayer.h"        // for CopyableCanvasLayer
#include "Layers.h"                     // for CanvasLayer, etc
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsRegion.h"                   // for nsIntRegion

class gfxContext;

namespace mozilla {
namespace layers {

class BasicCanvasLayer : public CopyableCanvasLayer,
                         public BasicImplData
{
public:
  BasicCanvasLayer(BasicLayerManager* aLayerManager) :
    CopyableCanvasLayer(aLayerManager,
                        static_cast<BasicImplData*>(MOZ_THIS_IN_INITIALIZER_LIST()))
  { }
  
  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    CanvasLayer::SetVisibleRegion(aRegion);
  }
  
  virtual void Paint(gfx::DrawTarget* aDT,
                     const gfx::Point& aDeviceOffset,
                     Layer* aMaskLayer) MOZ_OVERRIDE;
 
protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};

}
}

#endif
