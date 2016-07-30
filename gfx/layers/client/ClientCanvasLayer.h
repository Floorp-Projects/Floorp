/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CLIENTCANVASLAYER_H
#define GFX_CLIENTCANVASLAYER_H

#include "CanvasClient.h"               // for CanvasClient, etc
#include "ClientLayerManager.h"         // for ClientLayerManager, etc
#include "CopyableCanvasLayer.h"        // for CopyableCanvasLayer
#include "Layers.h"                     // for CanvasLayer, etc
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/layers/LayersMessages.h"  // for CanvasLayerAttributes, etc
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsRegion.h"                   // for nsIntRegion

namespace mozilla {
namespace gl {
class SurfaceFactory;
} // namespace gl

namespace layers {

class CompositableClient;
class ShadowableLayer;

class ClientCanvasLayer : public CopyableCanvasLayer,
                          public ClientLayer
{
  typedef CanvasClient::CanvasClientType CanvasClientType;
public:
  explicit ClientCanvasLayer(ClientLayerManager* aLayerManager) :
    CopyableCanvasLayer(aLayerManager, static_cast<ClientLayer*>(this))
  {
    MOZ_COUNT_CTOR(ClientCanvasLayer);
  }

protected:
  virtual ~ClientCanvasLayer();

public:
  virtual void SetVisibleRegion(const LayerIntRegion& aRegion) override
  {
    NS_ASSERTION(ClientManager()->InConstruction(),
                 "Can only set properties in construction phase");
    CanvasLayer::SetVisibleRegion(aRegion);
  }

  virtual void Initialize(const Data& aData) override;

  virtual void RenderLayer() override;

  virtual void ClearCachedResources() override
  {
    if (mCanvasClient) {
      mCanvasClient->Clear();
    }
  }

  virtual void HandleMemoryPressure() override
  {
    if (mCanvasClient) {
      mCanvasClient->HandleMemoryPressure();
    }
  }

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs) override
  {
    aAttrs = CanvasLayerAttributes(mSamplingFilter, mBounds);
  }

  virtual Layer* AsLayer()  override { return this; }
  virtual ShadowableLayer* AsShadowableLayer()  override { return this; }

  virtual void Disconnect() override
  {
    mCanvasClient = nullptr;
    ClientLayer::Disconnect();
  }

  virtual CompositableClient* GetCompositableClient() override
  {
    return mCanvasClient;
  }

  const TextureFlags& Flags() const { return mFlags; }

protected:

  bool UpdateTarget(gfx::DrawTarget* aDestTarget = nullptr);

  ClientLayerManager* ClientManager()
  {
    return static_cast<ClientLayerManager*>(mManager);
  }

  CanvasClientType GetCanvasClientType();

  RefPtr<CanvasClient> mCanvasClient;

  UniquePtr<gl::SurfaceFactory> mFactory;

  TextureFlags mFlags;

  friend class CanvasClient2D;
  friend class CanvasClientSharedSurface;
};

} // namespace layers
} // namespace mozilla

#endif
