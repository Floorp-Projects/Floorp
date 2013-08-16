/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientLayerManager.h"
#include "mozilla/layers/LayerTransaction.h"
#include "mozilla/layers/ImageClient.h"
#include "ImageContainer.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

class ClientImageLayer : public ImageLayer, 
                         public ClientLayer {
public:
  ClientImageLayer(ClientLayerManager* aLayerManager)
    : ImageLayer(aLayerManager,
                 static_cast<ClientLayer*>(MOZ_THIS_IN_INITIALIZER_LIST()))
    , mImageClientTypeContainer(BUFFER_UNKNOWN)
  {
    MOZ_COUNT_CTOR(ClientImageLayer);
  }
  virtual ~ClientImageLayer()
  {
    DestroyBackBuffer();
    MOZ_COUNT_DTOR(ClientImageLayer);
  }
  
  virtual void SetContainer(ImageContainer* aContainer) MOZ_OVERRIDE
  {
    ImageLayer::SetContainer(aContainer);
    mImageClientTypeContainer = BUFFER_UNKNOWN;
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(ClientManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ImageLayer::SetVisibleRegion(aRegion);
  }

  virtual void RenderLayer();
  
  virtual void ClearCachedResources() MOZ_OVERRIDE
  {
    DestroyBackBuffer();
  }

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
  {
    aAttrs = ImageLayerAttributes(mFilter, mScaleToSize, mScaleMode);
  }

  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  virtual void Disconnect()
  {
    DestroyBackBuffer();
    ClientLayer::Disconnect();
  }

  void DestroyBackBuffer()
  {
    if (mImageClient) {
      mImageClient->OnDetach();
      mImageClient = nullptr;
    }
  }

  virtual CompositableClient* GetCompositableClient() MOZ_OVERRIDE
  {
    return mImageClient;
  }

protected:
  ClientLayerManager* ClientManager()
  {
    return static_cast<ClientLayerManager*>(mManager);
  }

  CompositableType GetImageClientType()
  {
    if (mImageClientTypeContainer != BUFFER_UNKNOWN) {
      return mImageClientTypeContainer;
    }

    if (mContainer->IsAsync()) {
      mImageClientTypeContainer = BUFFER_BRIDGE;
      return mImageClientTypeContainer;
    }

    nsRefPtr<gfxASurface> surface;
    AutoLockImage autoLock(mContainer, getter_AddRefs(surface));

    mImageClientTypeContainer = autoLock.GetImage() ?
                                  BUFFER_IMAGE_SINGLE : BUFFER_UNKNOWN;
    return mImageClientTypeContainer;
  }

  RefPtr<ImageClient> mImageClient;
  CompositableType mImageClientTypeContainer;
};

void
ClientImageLayer::RenderLayer()
{
  if (GetMaskLayer()) {
    ToClientLayer(GetMaskLayer())->RenderLayer();
  }

  if (!mContainer) {
     return;
  }

  if (mImageClient) {
    mImageClient->OnTransaction();
  }

  if (!mImageClient ||
      !mImageClient->UpdateImage(mContainer, GetContentFlags())) {
    CompositableType type = GetImageClientType();
    if (type == BUFFER_UNKNOWN) {
      return;
    }
    mImageClient = ImageClient::CreateImageClient(type,
                                                  ClientManager(),
                                                  mDisallowBigImage
                                                    ? TEXTURE_DISALLOW_BIGIMAGE
                                                    : 0);
    if (type == BUFFER_BRIDGE) {
      static_cast<ImageClientBridge*>(mImageClient.get())->SetLayer(this);
    }

    if (!mImageClient) {
      return;
    }
    if (HasShadow() && !mContainer->IsAsync()) {
      mImageClient->Connect();
      ClientManager()->Attach(mImageClient, this);
    }
    if (!mImageClient->UpdateImage(mContainer, GetContentFlags())) {
      return;
    }
  }
  if (mImageClient) {
    mImageClient->OnTransaction();
  }
  ClientManager()->Hold(this);
}

already_AddRefed<ImageLayer>
ClientLayerManager::CreateImageLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ClientImageLayer> layer =
    new ClientImageLayer(this);
  CREATE_SHADOW(Image);
  return layer.forget();
}
}
}
