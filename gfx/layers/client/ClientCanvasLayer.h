/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CLIENTCANVASLAYER_H
#define GFX_CLIENTCANVASLAYER_H

#include "mozilla/layers/CanvasClient.h"  // for CanvasClient, etc
#include "ClientLayerManager.h"         // for ClientLayerManager, etc
#include "CopyableCanvasLayer.h"        // for CopyableCanvasLayer
#include "Layers.h"                     // for CanvasLayer, etc
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/layers/LayersMessages.h"  // for CanvasLayerAttributes, etc
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsRegion.h"                   // for nsIntRegion

namespace mozilla {
namespace gl {
class SharedSurface;
class SurfaceFactory;
}

namespace layers {

class CompositableClient;
class ShadowableLayer;

class ClientCanvasLayer : public CopyableCanvasLayer,
                          public ClientLayer
{
  typedef CanvasClient::CanvasClientType CanvasClientType;
public:
  explicit ClientCanvasLayer(ClientLayerManager* aLayerManager) :
    CopyableCanvasLayer(aLayerManager,
                        static_cast<ClientLayer*>(MOZ_THIS_IN_INITIALIZER_LIST()))
  {
    MOZ_COUNT_CTOR(ClientCanvasLayer);
  }

protected:
  virtual ~ClientCanvasLayer();

public:
  virtual void SetVisibleRegion(const nsIntRegion& aRegion) MOZ_OVERRIDE
  {
    NS_ASSERTION(ClientManager()->InConstruction(),
                 "Can only set properties in construction phase");
    CanvasLayer::SetVisibleRegion(aRegion);
  }

  virtual void Initialize(const Data& aData) MOZ_OVERRIDE;

  virtual void RenderLayer() MOZ_OVERRIDE;

  virtual void ClearCachedResources() MOZ_OVERRIDE
  {
    if (mCanvasClient) {
      mCanvasClient->Clear();
    }
  }

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs) MOZ_OVERRIDE
  {
    aAttrs = CanvasLayerAttributes(mFilter, mBounds);
  }

  virtual Layer* AsLayer()  MOZ_OVERRIDE { return this; }
  virtual ShadowableLayer* AsShadowableLayer()  MOZ_OVERRIDE { return this; }

  virtual void Disconnect() MOZ_OVERRIDE
  {
    mCanvasClient = nullptr;
    ClientLayer::Disconnect();
  }

  virtual CompositableClient* GetCompositableClient() MOZ_OVERRIDE
  {
    return mCanvasClient;
  }
protected:
  ClientLayerManager* ClientManager()
  {
    return static_cast<ClientLayerManager*>(mManager);
  }

  CanvasClientType GetCanvasClientType();

  RefPtr<CanvasClient> mCanvasClient;

  UniquePtr<gl::SurfaceFactory> mFactory;

  friend class DeprecatedCanvasClient2D;
  friend class CanvasClient2D;
  friend class CanvasClientSharedSurface;
};
}
}

#endif
