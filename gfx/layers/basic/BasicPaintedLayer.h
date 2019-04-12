/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICPAINTEDLAYER_H
#define GFX_BASICPAINTEDLAYER_H

#include "Layers.h"                 // for PaintedLayer, LayerManager, etc
#include "RotatedBuffer.h"          // for RotatedBuffer, etc
#include "BasicImplData.h"          // for BasicImplData
#include "BasicLayers.h"            // for BasicLayerManager
#include "gfxPoint.h"               // for gfxPoint
#include "mozilla/RefPtr.h"         // for RefPtr
#include "mozilla/gfx/BasePoint.h"  // for BasePoint
#include "mozilla/layers/ContentClient.h"  // for ContentClientBasic
#include "mozilla/mozalloc.h"              // for operator delete
#include "nsDebug.h"                       // for NS_ASSERTION
#include "nsISupportsImpl.h"               // for MOZ_COUNT_CTOR, etc
#include "nsRegion.h"                      // for nsIntRegion
class gfxContext;

namespace mozilla {
namespace layers {

class ReadbackProcessor;

class BasicPaintedLayer : public PaintedLayer, public BasicImplData {
 public:
  typedef ContentClient::PaintState PaintState;
  typedef ContentClient::ContentType ContentType;

  BasicPaintedLayer(BasicLayerManager* aLayerManager, gfx::BackendType aBackend)
      : PaintedLayer(aLayerManager, static_cast<BasicImplData*>(this)),
        mContentClient(nullptr),
        mBackend(aBackend) {
    MOZ_COUNT_CTOR(BasicPaintedLayer);
  }

 protected:
  virtual ~BasicPaintedLayer() { MOZ_COUNT_DTOR(BasicPaintedLayer); }

 public:
  void SetVisibleRegion(const LayerIntRegion& aRegion) override {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    PaintedLayer::SetVisibleRegion(aRegion);
  }
  void InvalidateRegion(const nsIntRegion& aRegion) override {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    mInvalidRegion.Add(aRegion);
    UpdateValidRegionAfterInvalidRegionChanged();
  }

  void PaintThebes(gfxContext* aContext, Layer* aMaskLayer,
                   LayerManager::DrawPaintedLayerCallback aCallback,
                   void* aCallbackData) override;

  void Validate(LayerManager::DrawPaintedLayerCallback aCallback,
                void* aCallbackData, ReadbackProcessor* aReadback) override;

  void ClearCachedResources() override {
    if (mContentClient) {
      mContentClient->Clear();
    }
    ClearValidRegion();
  }

  void ComputeEffectiveTransforms(
      const gfx::Matrix4x4& aTransformToSurface) override {
    if (!BasicManager()->IsRetained()) {
      // Don't do any snapping of our transform, since we're just going to
      // draw straight through without intermediate buffers.
      mEffectiveTransform = GetLocalTransform() * aTransformToSurface;
      if (gfxPoint(0, 0) != mResidualTranslation) {
        mResidualTranslation = gfxPoint(0, 0);
        ClearValidRegion();
      }
      ComputeEffectiveTransformForMaskLayers(aTransformToSurface);
      return;
    }
    PaintedLayer::ComputeEffectiveTransforms(aTransformToSurface);
  }

  BasicLayerManager* BasicManager() {
    return static_cast<BasicLayerManager*>(mManager);
  }

 protected:
  virtual void PaintBuffer(gfxContext* aContext,
                           const nsIntRegion& aRegionToDraw,
                           const nsIntRegion& aExtendedRegionToDraw,
                           const nsIntRegion& aRegionToInvalidate,
                           DrawRegionClip aClip,
                           LayerManager::DrawPaintedLayerCallback aCallback,
                           void* aCallbackData) {
    if (!aCallback) {
      BasicManager()->SetTransactionIncomplete();
      return;
    }
    aCallback(this, aContext, aExtendedRegionToDraw, aExtendedRegionToDraw,
              aClip, aRegionToInvalidate, aCallbackData);
    // Everything that's visible has been validated. Do this instead of just
    // OR-ing with aRegionToDraw, since that can lead to a very complex region
    // here (OR doesn't automatically simplify to the simplest possible
    // representation of a region.)
    nsIntRegion tmp;
    tmp.Or(mVisibleRegion.ToUnknownRegion(), aExtendedRegionToDraw);
    AddToValidRegion(tmp);
  }

  RefPtr<ContentClientBasic> mContentClient;
  gfx::BackendType mBackend;
};

}  // namespace layers
}  // namespace mozilla

#endif
