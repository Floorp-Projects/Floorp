/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICCONTAINERLAYER_H
#define GFX_BASICCONTAINERLAYER_H

#include "BasicImplData.h"              // for BasicImplData
#include "BasicLayers.h"                // for BasicLayerManager
#include "Layers.h"                     // for Layer, ContainerLayer
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR
#include "nsISupportsUtils.h"           // for NS_ADDREF, NS_RELEASE
struct nsIntRect;

namespace mozilla {
namespace layers {

class BasicContainerLayer : public ContainerLayer, public BasicImplData {
public:
  BasicContainerLayer(BasicLayerManager* aManager) :
    ContainerLayer(aManager,
                   static_cast<BasicImplData*>(MOZ_THIS_IN_INITIALIZER_LIST()))
  {
    MOZ_COUNT_CTOR(BasicContainerLayer);
    mSupportsComponentAlphaChildren = true;
  }
  virtual ~BasicContainerLayer();

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ContainerLayer::SetVisibleRegion(aRegion);
  }
  virtual bool InsertAfter(Layer* aChild, Layer* aAfter)
  {
    if (!BasicManager()->InConstruction()) {
      NS_ERROR("Can only set properties in construction phase");
      return false;
    }
    return ContainerLayer::InsertAfter(aChild, aAfter);
  }

  virtual bool RemoveChild(Layer* aChild)
  { 
    if (!BasicManager()->InConstruction()) {
      NS_ERROR("Can only set properties in construction phase");
      return false;
    }
    return ContainerLayer::RemoveChild(aChild);
  }

  virtual bool RepositionChild(Layer* aChild, Layer* aAfter)
  {
    if (!BasicManager()->InConstruction()) {
      NS_ERROR("Can only set properties in construction phase");
      return false;
    }
    return ContainerLayer::RepositionChild(aChild, aAfter);
  }

  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface);

  /**
   * Returns true when:
   * a) no (non-hidden) childrens' visible areas overlap in
   * (aInRect intersected with this layer's visible region).
   * b) the (non-hidden) childrens' visible areas cover
   * (aInRect intersected with this layer's visible region).
   * c) this layer and all (non-hidden) children have transforms that are translations
   * by integers.
   * aInRect is in the root coordinate system.
   * Child layers with opacity do not contribute to the covered area in check b).
   * This method can be conservative; it's OK to return false under any
   * circumstances.
   */
  bool ChildrenPartitionVisibleRegion(const nsIntRect& aInRect);

  void ForceIntermediateSurface() { mUseIntermediateSurface = true; }

  void SetSupportsComponentAlphaChildren(bool aSupports) { mSupportsComponentAlphaChildren = aSupports; }

  virtual void Validate(LayerManager::DrawThebesLayerCallback aCallback,
                        void* aCallbackData) MOZ_OVERRIDE;

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};
}
}

#endif
