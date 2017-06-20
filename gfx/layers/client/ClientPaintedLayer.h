/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CLIENTPAINTEDLAYER_H
#define GFX_CLIENTPAINTEDLAYER_H

#include "ClientLayerManager.h"         // for ClientLayerManager, etc
#include "Layers.h"                     // for PaintedLayer, etc
#include "RotatedBuffer.h"              // for RotatedContentBuffer, etc
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/layers/ContentClient.h"  // for ContentClient
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsRegion.h"                   // for nsIntRegion
#include "mozilla/layers/PLayerTransaction.h" // for PaintedLayerAttributes

namespace mozilla {
namespace gfx {
  class DrawEventRecorderMemory;
};

namespace layers {
class CompositableClient;
class ShadowableLayer;
class SpecificLayerAttributes;

class ClientPaintedLayer : public PaintedLayer,
                           public ClientLayer {
public:
  typedef RotatedContentBuffer::PaintState PaintState;
  typedef RotatedContentBuffer::ContentType ContentType;

  explicit ClientPaintedLayer(ClientLayerManager* aLayerManager,
                             LayerManager::PaintedLayerCreationHint aCreationHint = LayerManager::NONE) :
    PaintedLayer(aLayerManager, static_cast<ClientLayer*>(this), aCreationHint),
    mContentClient(nullptr)
  {
    MOZ_COUNT_CTOR(ClientPaintedLayer);
  }

protected:
  virtual ~ClientPaintedLayer()
  {
    if (mContentClient) {
      mContentClient->OnDetach();
      mContentClient = nullptr;
    }
    MOZ_COUNT_DTOR(ClientPaintedLayer);
  }

public:
  virtual void SetVisibleRegion(const LayerIntRegion& aRegion) override
  {
    NS_ASSERTION(ClientManager()->InConstruction(),
                 "Can only set properties in construction phase");
    PaintedLayer::SetVisibleRegion(aRegion);
  }
  virtual void InvalidateRegion(const nsIntRegion& aRegion) override
  {
    NS_ASSERTION(ClientManager()->InConstruction(),
                 "Can only set properties in construction phase");
    mInvalidRegion.Add(aRegion);
    UpdateValidRegionAfterInvalidRegionChanged();
  }

  virtual void RenderLayer() override { RenderLayerWithReadback(nullptr); }

  virtual void RenderLayerWithReadback(ReadbackProcessor *aReadback) override;

  virtual void ClearCachedResources() override
  {
    if (mContentClient) {
      mContentClient->Clear();
    }
    ClearValidRegion();
    DestroyBackBuffer();
  }

  virtual void HandleMemoryPressure() override
  {
    if (mContentClient) {
      mContentClient->HandleMemoryPressure();
    }
  }

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs) override
  {
    aAttrs = PaintedLayerAttributes(GetValidRegion());
  }
  
  ClientLayerManager* ClientManager()
  {
    return static_cast<ClientLayerManager*>(mManager);
  }
  
  virtual Layer* AsLayer() override { return this; }
  virtual ShadowableLayer* AsShadowableLayer() override { return this; }

  virtual CompositableClient* GetCompositableClient() override
  {
    return mContentClient;
  }

  virtual void Disconnect() override
  {
    mContentClient = nullptr;
  }

protected:
  void PaintThebes(nsTArray<ReadbackProcessor::Update>* aReadbackUpdates);
  void RecordThebes();
  bool CanRecordLayer(ReadbackProcessor* aReadback);
  bool HasMaskLayers();
  already_AddRefed<gfx::DrawEventRecorderMemory> RecordPaintedLayer();
  bool EnsureContentClient();
  uint32_t GetPaintFlags();
  void UpdateContentClient(PaintState& aState);
  bool UpdatePaintRegion(PaintState& aState);
  void PaintOffMainThread(DrawEventRecorderMemory* aRecorder);

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

  void DestroyBackBuffer()
  {
    mContentClient = nullptr;
  }

  RefPtr<ContentClient> mContentClient;
};

} // namespace layers
} // namespace mozilla

#endif
