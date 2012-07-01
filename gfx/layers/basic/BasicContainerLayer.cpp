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

  nsIntPoint offset(PRInt32(transform.x0), PRInt32(transform.y0));
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
    childRegion.MoveBy(PRInt32(childTransform.x0), PRInt32(childTransform.y0));
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

  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  virtual void Disconnect()
  {
    BasicShadowableLayer::Disconnect();
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
                                 aAfter ? ShadowManager()->Hold(aAfter) : nsnull);
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

class BasicShadowContainerLayer : public ShadowContainerLayer, public BasicImplData {
  template<class Container>
  friend void ContainerInsertAfter(Layer* aChild, Layer* aAfter, Container* aContainer);
  template<class Container>
  friend void ContainerRemoveChild(Layer* aChild, Container* aContainer);

public:
  BasicShadowContainerLayer(BasicShadowLayerManager* aLayerManager) :
    ShadowContainerLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicShadowContainerLayer);
  }
  virtual ~BasicShadowContainerLayer()
  {
    while (mFirstChild) {
      ContainerRemoveChild(mFirstChild, this);
    }

    MOZ_COUNT_DTOR(BasicShadowContainerLayer);
  }

  virtual void InsertAfter(Layer* aChild, Layer* aAfter)
  { ContainerInsertAfter(aChild, aAfter, this); }
  virtual void RemoveChild(Layer* aChild)
  { ContainerRemoveChild(aChild, this); }

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    // We push groups for container layers if we need to, which always
    // are aligned in device space, so it doesn't really matter how we snap
    // containers.
    gfxMatrix residual;
    gfx3DMatrix idealTransform = GetLocalTransform()*aTransformToSurface;
    idealTransform.ProjectTo2D();

    if (!idealTransform.CanDraw2D()) {
      mEffectiveTransform = idealTransform;
      ComputeEffectiveTransformsForChildren(gfx3DMatrix());
      ComputeEffectiveTransformForMaskLayer(gfx3DMatrix());
      mUseIntermediateSurface = true;
      return;
    }

    mEffectiveTransform = SnapTransform(idealTransform, gfxRect(0, 0, 0, 0), &residual);
    // We always pass the ideal matrix down to our children, so there is no
    // need to apply any compensation using the residual from SnapTransform.
    ComputeEffectiveTransformsForChildren(idealTransform);

    ComputeEffectiveTransformForMaskLayer(aTransformToSurface);

    /* If we have a single child, it can just inherit our opacity,
     * otherwise we need a PushGroup and we need to mark ourselves as using
     * an intermediate surface so our children don't inherit our opacity
     * via GetEffectiveOpacity.
     * Having a mask layer always forces our own push group
     */
    mUseIntermediateSurface = GetMaskLayer() ||
                              (GetEffectiveOpacity() != 1.0 && HasMultipleChildren());
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

already_AddRefed<ShadowContainerLayer>
BasicShadowLayerManager::CreateShadowContainerLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ShadowContainerLayer> layer = new BasicShadowContainerLayer(this);
  return layer.forget();
}

}
}
