/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERCONTAINERLAYER_H
#define GFX_WEBRENDERCONTAINERLAYER_H

#include "gfxPrefs.h"
#include "Layers.h"
#include "mozilla/layers/WebRenderLayer.h"
#include "mozilla/layers/WebRenderLayerManager.h"

namespace mozilla {
namespace layers {

class WebRenderContainerLayer : public WebRenderLayer,
                                public ContainerLayer
{
public:
  explicit WebRenderContainerLayer(WebRenderLayerManager* aManager)
    : ContainerLayer(aManager, static_cast<WebRenderLayer*>(this))
  {
    MOZ_COUNT_CTOR(WebRenderContainerLayer);
  }

protected:
  virtual ~WebRenderContainerLayer()
  {
    if (!GetAnimations().IsEmpty()) {
      mManager->AsWebRenderLayerManager()->
        AddCompositorAnimationsIdForDiscard(GetCompositorAnimationsId());
    }

    ContainerLayer::RemoveAllChildren();
    MOZ_COUNT_DTOR(WebRenderContainerLayer);
  }

  void UpdateTransformDataForAnimation();

public:
  Layer* GetLayer() override { return this; }
  void RenderLayer(wr::DisplayListBuilder& aBuilder,
                   wr::IpcResourceUpdateQueue& aResources,
                   const StackingContextHelper& aSc) override;

  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface) override
  {
    DefaultComputeEffectiveTransforms(aTransformToSurface);
  }
};

class WebRenderRefLayer : public WebRenderLayer,
                          public RefLayer {
public:
  explicit WebRenderRefLayer(WebRenderLayerManager* aManager) :
    RefLayer(aManager, static_cast<WebRenderLayer*>(this))
  {
    MOZ_COUNT_CTOR(WebRenderRefLayer);
  }

protected:
  virtual ~WebRenderRefLayer()
  {
    MOZ_COUNT_DTOR(WebRenderRefLayer);
  }

public:
  Layer* GetLayer() override { return this; }
  void RenderLayer(wr::DisplayListBuilder& aBuilder,
                   wr::IpcResourceUpdateQueue& aResources,
                   const StackingContextHelper& aSc) override;

  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface) override
  {
    DefaultComputeEffectiveTransforms(aTransformToSurface);
  }
};

} // namespace layers
} // namespace mozilla

#endif // GFX_WEBRENDERCONTAINERLAYER_H
