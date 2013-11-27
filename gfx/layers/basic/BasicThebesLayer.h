/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICTHEBESLAYER_H
#define GFX_BASICTHEBESLAYER_H

#include "Layers.h"                     // for ThebesLayer, LayerManager, etc
#include "RotatedBuffer.h"              // for RotatedContentBuffer, etc
#include "BasicImplData.h"              // for BasicImplData
#include "BasicLayers.h"                // for BasicLayerManager
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxPoint.h"                   // for gfxPoint
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/layers/ContentClient.h"  // for ContentClientBasic
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc
class gfxContext;

namespace mozilla {
namespace layers {

class ReadbackProcessor;

class BasicThebesLayer : public ThebesLayer, public BasicImplData {
public:
  typedef RotatedContentBuffer::PaintState PaintState;
  typedef RotatedContentBuffer::ContentType ContentType;

  BasicThebesLayer(BasicLayerManager* aLayerManager) :
    ThebesLayer(aLayerManager,
                static_cast<BasicImplData*>(MOZ_THIS_IN_INITIALIZER_LIST())),
    mContentClient(nullptr)
  {
    MOZ_COUNT_CTOR(BasicThebesLayer);
  }
  virtual ~BasicThebesLayer()
  {
    MOZ_COUNT_DTOR(BasicThebesLayer);
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ThebesLayer::SetVisibleRegion(aRegion);
  }
  virtual void InvalidateRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    mInvalidRegion.Or(mInvalidRegion, aRegion);
    mInvalidRegion.SimplifyOutward(10);
    mValidRegion.Sub(mValidRegion, mInvalidRegion);
  }

  virtual void PaintThebes(gfxContext* aContext,
                           Layer* aMaskLayer,
                           LayerManager::DrawThebesLayerCallback aCallback,
                           void* aCallbackData,
                           ReadbackProcessor* aReadback);

  virtual void Validate(LayerManager::DrawThebesLayerCallback aCallback,
                        void* aCallbackData) MOZ_OVERRIDE;

  virtual void ClearCachedResources()
  {
    if (mContentClient) {
      mContentClient->Clear();
    }
    mValidRegion.SetEmpty();
  }
  
  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    if (!BasicManager()->IsRetained()) {
      // Don't do any snapping of our transform, since we're just going to
      // draw straight through without intermediate buffers.
      mEffectiveTransform = GetLocalTransform()*aTransformToSurface;
      if (gfxPoint(0,0) != mResidualTranslation) {
        mResidualTranslation = gfxPoint(0,0);
        mValidRegion.SetEmpty();
      }
      ComputeEffectiveTransformForMaskLayer(aTransformToSurface);
      return;
    }
    ThebesLayer::ComputeEffectiveTransforms(aTransformToSurface);
  }

  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }

protected:
  virtual void
  PaintBuffer(gfxContext* aContext,
              const nsIntRegion& aRegionToDraw,
              const nsIntRegion& aExtendedRegionToDraw,
              const nsIntRegion& aRegionToInvalidate,
              bool aDidSelfCopy,
              DrawRegionClip aClip,
              LayerManager::DrawThebesLayerCallback aCallback,
              void* aCallbackData)
  {
    if (!aCallback) {
      BasicManager()->SetTransactionIncomplete();
      return;
    }
    aCallback(this, aContext, aExtendedRegionToDraw, aClip,
              aRegionToInvalidate, aCallbackData);
    // Everything that's visible has been validated. Do this instead of just
    // OR-ing with aRegionToDraw, since that can lead to a very complex region
    // here (OR doesn't automatically simplify to the simplest possible
    // representation of a region.)
    nsIntRegion tmp;
    tmp.Or(mVisibleRegion, aExtendedRegionToDraw);
    mValidRegion.Or(mValidRegion, tmp);
  }

  RefPtr<ContentClientBasic> mContentClient;
};

}
}

#endif
