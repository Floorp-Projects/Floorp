/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientLayerManager.h"
#include "mozilla/layers/LayerTransaction.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

class ClientColorLayer : public ColorLayer, 
                         public ClientLayer {
public:
  ClientColorLayer(ClientLayerManager* aLayerManager) :
    ColorLayer(aLayerManager, static_cast<ClientLayer*>(this))
  {
    MOZ_COUNT_CTOR(ClientColorLayer);
  }
  virtual ~ClientColorLayer()
  {
    MOZ_COUNT_DTOR(ClientColorLayer);
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(ClientManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ColorLayer::SetVisibleRegion(aRegion);
  }

  virtual void RenderLayer()
  {
    if (GetMaskLayer()) {
      ToClientLayer(GetMaskLayer())->RenderLayer();
    }
  }

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
  {
    aAttrs = ColorLayerAttributes(GetColor());
  }

  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  virtual void Disconnect()
  {
    ClientLayer::Disconnect();
  }

protected:
  ClientLayerManager* ClientManager()
  {
    return static_cast<ClientLayerManager*>(mManager);
  }
};

already_AddRefed<ColorLayer>
ClientLayerManager::CreateColorLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ClientColorLayer> layer =
    new ClientColorLayer(this);
  CREATE_SHADOW(Color);
  return layer.forget();
}

}
}
