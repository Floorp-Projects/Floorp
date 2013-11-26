/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CLIENTCONTAINERLAYER_H
#define GFX_CLIENTCONTAINERLAYER_H

#include <stdint.h>                     // for uint32_t
#include "ClientLayerManager.h"         // for ClientLayerManager, etc
#include "Layers.h"                     // for Layer, ContainerLayer, etc
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxMatrix.h"                  // for gfxMatrix
#include "gfxPlatform.h"                // for gfxPlatform
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsUtils.h"           // for NS_ADDREF, NS_RELEASE
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsAutoTArray
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc

namespace mozilla {
namespace layers {

class ShadowableLayer;

class ClientContainerLayer : public ContainerLayer,
                             public ClientLayer
{
public:
  ClientContainerLayer(ClientLayerManager* aManager) :
    ContainerLayer(aManager,
                   static_cast<ClientLayer*>(MOZ_THIS_IN_INITIALIZER_LIST()))
  {
    MOZ_COUNT_CTOR(ClientContainerLayer);
    mSupportsComponentAlphaChildren = true;
  }
  virtual ~ClientContainerLayer()
  {
    while (mFirstChild) {
      ContainerLayer::RemoveChild(mFirstChild);
    }

    MOZ_COUNT_DTOR(ClientContainerLayer);
  }

  virtual void RenderLayer()
  {
    if (GetMaskLayer()) {
      ToClientLayer(GetMaskLayer())->RenderLayer();
    }
    
    // Setup mSupportsComponentAlphaChildren in the same way 
    // that ContainerLayerComposite will do.
    if (UseIntermediateSurface()) {
      if (GetEffectiveVisibleRegion().GetNumRects() != 1 ||
          !(GetContentFlags() & Layer::CONTENT_OPAQUE))
      {
        const gfx3DMatrix& transform3D = GetEffectiveTransform();
        gfxMatrix transform;
        if (HasOpaqueAncestorLayer(this) &&
            transform3D.Is2D(&transform) && 
            !transform.HasNonIntegerTranslation()) {
          SetSupportsComponentAlphaChildren(
            gfxPlatform::ComponentAlphaEnabled());
        }
      }
    } else {
      SetSupportsComponentAlphaChildren(
        (GetContentFlags() & Layer::CONTENT_OPAQUE) ||
        (GetParent() && GetParent()->SupportsComponentAlphaChildren()));
    }

    nsAutoTArray<Layer*, 12> children;
    SortChildrenBy3DZOrder(children);

    for (uint32_t i = 0; i < children.Length(); i++) {
      if (children.ElementAt(i)->GetEffectiveVisibleRegion().IsEmpty()) {
        continue;
      }

      ToClientLayer(children.ElementAt(i))->RenderLayer();
    }
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(ClientManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ContainerLayer::SetVisibleRegion(aRegion);
  }
  virtual void InsertAfter(Layer* aChild, Layer* aAfter) MOZ_OVERRIDE
  {
    NS_ASSERTION(ClientManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ClientManager()->InsertAfter(ClientManager()->Hold(this),
                                 ClientManager()->Hold(aChild),
                                 aAfter ? ClientManager()->Hold(aAfter) : nullptr);
    ContainerLayer::InsertAfter(aChild, aAfter);
  }

  virtual void RemoveChild(Layer* aChild) MOZ_OVERRIDE
  { 
    NS_ASSERTION(ClientManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ClientManager()->RemoveChild(ClientManager()->Hold(this),
                                 ClientManager()->Hold(aChild));
    ContainerLayer::RemoveChild(aChild);
  }

  virtual void RepositionChild(Layer* aChild, Layer* aAfter) MOZ_OVERRIDE
  {
    NS_ASSERTION(ClientManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ClientManager()->RepositionChild(ClientManager()->Hold(this),
                                     ClientManager()->Hold(aChild),
                                     aAfter ? ClientManager()->Hold(aAfter) : nullptr);
    ContainerLayer::RepositionChild(aChild, aAfter);
  }
  
  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    DefaultComputeEffectiveTransforms(aTransformToSurface);
  }

  void ForceIntermediateSurface() { mUseIntermediateSurface = true; }

  void SetSupportsComponentAlphaChildren(bool aSupports) { mSupportsComponentAlphaChildren = aSupports; }

protected:
  ClientLayerManager* ClientManager()
  {
    return static_cast<ClientLayerManager*>(mManager);
  }
};

class ClientRefLayer : public RefLayer,
                       public ClientLayer {
public:
  ClientRefLayer(ClientLayerManager* aManager) :
    RefLayer(aManager,
             static_cast<ClientLayer*>(MOZ_THIS_IN_INITIALIZER_LIST()))
  {
    MOZ_COUNT_CTOR(ClientRefLayer);
  }
  virtual ~ClientRefLayer()
  {
    MOZ_COUNT_DTOR(ClientRefLayer);
  }

  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  virtual void Disconnect()
  {
    ClientLayer::Disconnect();
  }

  virtual void RenderLayer() { }

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    DefaultComputeEffectiveTransforms(aTransformToSurface);
  }

private:
  ClientLayerManager* ClientManager()
  {
    return static_cast<ClientLayerManager*>(mManager);
  }
};

}
}

#endif
