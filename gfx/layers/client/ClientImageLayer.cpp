/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientLayerManager.h"         // for ClientLayerManager, etc
#include "ImageContainer.h"             // for AutoLockImage, etc
#include "ImageLayers.h"                // for ImageLayer
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/ImageClient.h"  // for ImageClient, etc
#include "mozilla/layers/LayersMessages.h"  // for ImageLayerAttributes, etc
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsAutoPtr.h"                  // for nsRefPtr, getter_AddRefs, etc
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsRegion.h"                   // for nsIntRegion

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

class ClientImageLayer : public ImageLayer, 
                         public ClientLayer {
public:
  explicit ClientImageLayer(ClientLayerManager* aLayerManager)
    : ImageLayer(aLayerManager,
                 static_cast<ClientLayer*>(MOZ_THIS_IN_INITIALIZER_LIST()))
    , mImageClientTypeContainer(CompositableType::BUFFER_UNKNOWN)
  {
    MOZ_COUNT_CTOR(ClientImageLayer);
  }

protected:
  virtual ~ClientImageLayer()
  {
    DestroyBackBuffer();
    MOZ_COUNT_DTOR(ClientImageLayer);
  }

  virtual void SetContainer(ImageContainer* aContainer) MOZ_OVERRIDE
  {
    ImageLayer::SetContainer(aContainer);
    mImageClientTypeContainer = CompositableType::BUFFER_UNKNOWN;
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
    if (mImageClientTypeContainer != CompositableType::BUFFER_UNKNOWN) {
      return mImageClientTypeContainer;
    }

    if (mContainer->IsAsync()) {
      mImageClientTypeContainer = CompositableType::BUFFER_BRIDGE;
      return mImageClientTypeContainer;
    }

    AutoLockImage autoLock(mContainer);

#ifdef MOZ_WIDGET_GONK
    // gralloc buffer needs CompositableType::BUFFER_IMAGE_BUFFERED to prevent
    // the buffer's usage conflict.
    if (autoLock.GetImage()->GetFormat() == ImageFormat::OVERLAY_IMAGE) {
      mImageClientTypeContainer = CompositableType::IMAGE_OVERLAY;
      return mImageClientTypeContainer;
    }

    mImageClientTypeContainer = autoLock.GetImage() ?
                                  CompositableType::BUFFER_IMAGE_BUFFERED : CompositableType::BUFFER_UNKNOWN;
#else
    mImageClientTypeContainer = autoLock.GetImage() ?
                                  CompositableType::BUFFER_IMAGE_SINGLE : CompositableType::BUFFER_UNKNOWN;
#endif
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
    if (type == CompositableType::BUFFER_UNKNOWN) {
      return;
    }
    TextureFlags flags = TextureFlags::FRONT;
    if (mDisallowBigImage) {
      flags |= TextureFlags::DISALLOW_BIGIMAGE;
    }
    mImageClient = ImageClient::CreateImageClient(type,
                                                  ClientManager()->AsShadowForwarder(),
                                                  flags);
    if (type == CompositableType::BUFFER_BRIDGE) {
      static_cast<ImageClientBridge*>(mImageClient.get())->SetLayer(this);
    }

    if (!mImageClient) {
      return;
    }
    if (HasShadow() && !mContainer->IsAsync()) {
      mImageClient->Connect();
      ClientManager()->AsShadowForwarder()->Attach(mImageClient, this);
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
