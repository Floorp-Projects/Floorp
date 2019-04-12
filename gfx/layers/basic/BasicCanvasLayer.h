/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICCANVASLAYER_H
#define GFX_BASICCANVASLAYER_H

#include "BasicImplData.h"  // for BasicImplData
#include "BasicLayers.h"    // for BasicLayerManager
#include "Layers.h"         // for CanvasLayer, etc
#include "nsDebug.h"        // for NS_ASSERTION
#include "nsRegion.h"       // for nsIntRegion

namespace mozilla {
namespace layers {

class BasicCanvasLayer : public CanvasLayer, public BasicImplData {
 public:
  explicit BasicCanvasLayer(BasicLayerManager* aLayerManager)
      : CanvasLayer(aLayerManager, static_cast<BasicImplData*>(this)) {}

  void SetVisibleRegion(const LayerIntRegion& aRegion) override {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    CanvasLayer::SetVisibleRegion(aRegion);
  }

  void Paint(gfx::DrawTarget* aDT, const gfx::Point& aDeviceOffset,
             Layer* aMaskLayer) override;

 protected:
  BasicLayerManager* BasicManager() {
    return static_cast<BasicLayerManager*>(mManager);
  }

  CanvasRenderer* CreateCanvasRendererInternal() override;
};

}  // namespace layers
}  // namespace mozilla

#endif
