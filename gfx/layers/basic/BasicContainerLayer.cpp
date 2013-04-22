/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicLayersImpl.h"
#include "BasicContainerLayer.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {



BasicContainerLayer::~BasicContainerLayer()
{
  while (mFirstChild) {
    ContainerRemoveChild(mFirstChild, this);
  }

  MOZ_COUNT_DTOR(BasicContainerLayer);
}

bool
BasicContainerLayer::ChildrenPartitionVisibleRegion(const nsIntRect& aInRect)
{
  gfxMatrix transform;
  if (!GetEffectiveTransform().CanDraw2D(&transform) ||
      transform.HasNonIntegerTranslation())
    return false;

  nsIntPoint offset(int32_t(transform.x0), int32_t(transform.y0));
  nsIntRect rect = aInRect.Intersect(GetEffectiveVisibleRegion().GetBounds() + offset);
  nsIntRegion covered;

  for (Layer* l = mFirstChild; l; l = l->GetNextSibling()) {
    if (ToData(l)->IsHidden())
      continue;

    gfxMatrix childTransform;
    if (!l->GetEffectiveTransform().CanDraw2D(&childTransform) ||
        childTransform.HasNonIntegerTranslation() ||
        l->GetEffectiveOpacity() != 1.0)
      return false;
    nsIntRegion childRegion = l->GetEffectiveVisibleRegion();
    childRegion.MoveBy(int32_t(childTransform.x0), int32_t(childTransform.y0));
    childRegion.And(childRegion, rect);
    if (l->GetClipRect()) {
      childRegion.And(childRegion, *l->GetClipRect() + offset);
    }
    nsIntRegion intersection;
    intersection.And(covered, childRegion);
    if (!intersection.IsEmpty())
      return false;
    covered.Or(covered, childRegion);
  }

  return covered.Contains(rect);
}




class BasicShadowableContainerLayer : public BasicContainerLayer,
                                      public BasicShadowableLayer {
public:
  BasicShadowableContainerLayer(BasicShadowLayerManager* aManager) :
    BasicContainerLayer(aManager)
  {
    MOZ_COUNT_CTOR(BasicShadowableContainerLayer);
  }
  virtual ~BasicShadowableContainerLayer()
  {
    MOZ_COUNT_DTOR(BasicShadowableContainerLayer);
  }

  virtual void InsertAfter(Layer* aChild, Layer* aAfter);
  virtual void RemoveChild(Layer* aChild);
  virtual void RepositionChild(Layer* aChild, Layer* aAfter);

  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  virtual void Disconnect()
  {
    BasicShadowableLayer::Disconnect();
  }

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface) MOZ_OVERRIDE
  {
    if (HasShadow()) {
      DefaultComputeEffectiveTransforms(aTransformToSurface);
    } else {
      BasicContainerLayer::ComputeEffectiveTransforms(aTransformToSurface);
    }
  }

private:
  BasicShadowLayerManager* ShadowManager()
  {
    return static_cast<BasicShadowLayerManager*>(mManager);
  }
};

void
BasicShadowableContainerLayer::InsertAfter(Layer* aChild, Layer* aAfter)
{
  if (HasShadow() && ShouldShadow(aChild)) {
    while (aAfter && !ShouldShadow(aAfter)) {
      aAfter = aAfter->GetPrevSibling();
    }
    ShadowManager()->InsertAfter(ShadowManager()->Hold(this),
                                 ShadowManager()->Hold(aChild),
                                 aAfter ? ShadowManager()->Hold(aAfter) : nullptr);
  }
  BasicContainerLayer::InsertAfter(aChild, aAfter);
}

void
BasicShadowableContainerLayer::RemoveChild(Layer* aChild)
{
  if (HasShadow() && ShouldShadow(aChild)) {
    ShadowManager()->RemoveChild(ShadowManager()->Hold(this),
                                 ShadowManager()->Hold(aChild));
  }
  BasicContainerLayer::RemoveChild(aChild);
}

void
BasicShadowableContainerLayer::RepositionChild(Layer* aChild, Layer* aAfter)
{
  if (HasShadow() && ShouldShadow(aChild)) {
    while (aAfter && !ShouldShadow(aAfter)) {
      aAfter = aAfter->GetPrevSibling();
    }
    ShadowManager()->RepositionChild(ShadowManager()->Hold(this),
                                     ShadowManager()->Hold(aChild),
                                     aAfter ? ShadowManager()->Hold(aAfter) : nullptr);
  }
  BasicContainerLayer::RepositionChild(aChild, aAfter);
}

class BasicShadowableRefLayer : public RefLayer, public BasicImplData,
                                public BasicShadowableLayer {
  template<class Container>
  friend void ContainerComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface,
                                                  Container* aContainer);
public:
  BasicShadowableRefLayer(BasicShadowLayerManager* aManager) :
    RefLayer(aManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicShadowableRefLayer);
  }
  virtual ~BasicShadowableRefLayer()
  {
    MOZ_COUNT_DTOR(BasicShadowableRefLayer);
  }

  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  virtual void Disconnect()
  {
    BasicShadowableLayer::Disconnect();
  }

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    ContainerComputeEffectiveTransforms(aTransformToSurface, this);
  }

private:
  BasicShadowLayerManager* ShadowManager()
  {
    return static_cast<BasicShadowLayerManager*>(mManager);
  }
};

already_AddRefed<ContainerLayer>
BasicLayerManager::CreateContainerLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ContainerLayer> layer = new BasicContainerLayer(this);
  return layer.forget();
}

already_AddRefed<ContainerLayer>
BasicShadowLayerManager::CreateContainerLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<BasicShadowableContainerLayer> layer =
    new BasicShadowableContainerLayer(this);
  MAYBE_CREATE_SHADOW(Container);
  return layer.forget();
}

already_AddRefed<RefLayer>
BasicShadowLayerManager::CreateRefLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<BasicShadowableRefLayer> layer =
    new BasicShadowableRefLayer(this);
  MAYBE_CREATE_SHADOW(Ref);
  return layer.forget();
}


}
}
