/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICLAYERSIMPL_H
#define GFX_BASICLAYERSIMPL_H

#include "ipc/ShadowLayerChild.h"
#include "BasicLayers.h"
#include "BasicImplData.h"
#include "ReadbackProcessor.h"

namespace mozilla {
namespace layers {

class BasicContainerLayer;
class ShadowableLayer;

class AutoSetOperator {
public:
  AutoSetOperator(gfxContext* aContext, gfxContext::GraphicsOperator aOperator) {
    if (aOperator != gfxContext::OPERATOR_OVER) {
      aContext->SetOperator(aOperator);
      mContext = aContext;
    }
  }
  ~AutoSetOperator() {
    if (mContext) {
      mContext->SetOperator(gfxContext::OPERATOR_OVER);
    }
  }
private:
  nsRefPtr<gfxContext> mContext;
};

class BasicReadbackLayer : public ReadbackLayer,
                           public BasicImplData
{
public:
  BasicReadbackLayer(BasicLayerManager* aLayerManager) :
    ReadbackLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicReadbackLayer);
  }
  virtual ~BasicReadbackLayer()
  {
    MOZ_COUNT_DTOR(BasicReadbackLayer);
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ReadbackLayer::SetVisibleRegion(aRegion);
  }

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};

/*
 * Extract a mask surface for a mask layer
 * Returns a surface for the mask layer if a mask layer is present and has a
 * valid surface and transform; nsnull otherwise.
 * The transform for the layer will be put in aMaskTransform
 */
already_AddRefed<gfxASurface>
GetMaskSurfaceAndTransform(Layer* aMaskLayer, gfxMatrix* aMaskTransform);

// Paint the current source to a context using a mask, if present
void
PaintWithMask(gfxContext* aContext, float aOpacity, Layer* aMaskLayer);

// Fill the current path with the current source, using a
// mask and opacity, if present
void
FillWithMask(gfxContext* aContext, float aOpacity, Layer* aMaskLayer);

BasicImplData*
ToData(Layer* aLayer);

ShadowableLayer*
ToShadowable(Layer* aLayer);

// Some layers, like ReadbackLayers, can't be shadowed and shadowing
// them doesn't make sense anyway
bool
ShouldShadow(Layer* aLayer);

template<class OpT> BasicShadowableLayer*
GetBasicShadowable(const OpT& op)
{
  return static_cast<BasicShadowableLayer*>(
    static_cast<const ShadowLayerChild*>(op.layerChild())->layer());
}

// Create a shadow layer (PLayerChild) for aLayer, if we're forwarding
// our layer tree to a parent process.  Record the new layer creation
// in the current open transaction as a side effect.
template<typename CreatedMethod> void
MaybeCreateShadowFor(BasicShadowableLayer* aLayer,
                     BasicShadowLayerManager* aMgr,
                     CreatedMethod aMethod)
{
  if (!aMgr->HasShadowManager()) {
    return;
  }

  PLayerChild* shadow = aMgr->ConstructShadowFor(aLayer);
  // XXX error handling
  NS_ABORT_IF_FALSE(shadow, "failed to create shadow");

  aLayer->SetShadow(shadow);
  (aMgr->*aMethod)(aLayer);
  aMgr->Hold(aLayer->AsLayer());
}

#define MAYBE_CREATE_SHADOW(_type)                                      \
  MaybeCreateShadowFor(layer, this,                                     \
                       &ShadowLayerForwarder::Created ## _type ## Layer)

}
}

#endif
