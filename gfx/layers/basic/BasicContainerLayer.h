/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICCONTAINERLAYER_H
#define GFX_BASICCONTAINERLAYER_H

#include "BasicLayers.h"
#include "BasicImplData.h"

namespace mozilla {
namespace layers {

template<class Container> void
ContainerInsertAfter(Layer* aChild, Layer* aAfter, Container* aContainer)
{
  NS_ASSERTION(aChild->Manager() == aContainer->Manager(),
               "Child has wrong manager");
  NS_ASSERTION(!aChild->GetParent(),
               "aChild already in the tree");
  NS_ASSERTION(!aChild->GetNextSibling() && !aChild->GetPrevSibling(),
               "aChild already has siblings?");
  NS_ASSERTION(!aAfter ||
               (aAfter->Manager() == aContainer->Manager() &&
                aAfter->GetParent() == aContainer),
               "aAfter is not our child");

  aChild->SetParent(aContainer);
  if (aAfter == aContainer->mLastChild) {
    aContainer->mLastChild = aChild;
  }
  if (!aAfter) {
    aChild->SetNextSibling(aContainer->mFirstChild);
    if (aContainer->mFirstChild) {
      aContainer->mFirstChild->SetPrevSibling(aChild);
    }
    aContainer->mFirstChild = aChild;
    NS_ADDREF(aChild);
    aContainer->DidInsertChild(aChild);
    return;
  }

  Layer* next = aAfter->GetNextSibling();
  aChild->SetNextSibling(next);
  aChild->SetPrevSibling(aAfter);
  if (next) {
    next->SetPrevSibling(aChild);
  }
  aAfter->SetNextSibling(aChild);
  NS_ADDREF(aChild);
  aContainer->DidInsertChild(aChild);
}

template<class Container> void
ContainerRemoveChild(Layer* aChild, Container* aContainer)
{
  NS_ASSERTION(aChild->Manager() == aContainer->Manager(),
               "Child has wrong manager");
  NS_ASSERTION(aChild->GetParent() == aContainer,
               "aChild not our child");

  Layer* prev = aChild->GetPrevSibling();
  Layer* next = aChild->GetNextSibling();
  if (prev) {
    prev->SetNextSibling(next);
  } else {
    aContainer->mFirstChild = next;
  }
  if (next) {
    next->SetPrevSibling(prev);
  } else {
    aContainer->mLastChild = prev;
  }

  aChild->SetNextSibling(nsnull);
  aChild->SetPrevSibling(nsnull);
  aChild->SetParent(nsnull);

  aContainer->DidRemoveChild(aChild);
  NS_RELEASE(aChild);
}

template<class Container>
static void
ContainerComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface,
                                    Container* aContainer)
{
  // We push groups for container layers if we need to, which always
  // are aligned in device space, so it doesn't really matter how we snap
  // containers.
  gfxMatrix residual;
  gfx3DMatrix idealTransform = aContainer->GetLocalTransform()*aTransformToSurface;
  idealTransform.ProjectTo2D();

  if (!idealTransform.CanDraw2D()) {
    aContainer->mEffectiveTransform = idealTransform;
    aContainer->ComputeEffectiveTransformsForChildren(gfx3DMatrix());
    aContainer->ComputeEffectiveTransformForMaskLayer(gfx3DMatrix());
    aContainer->mUseIntermediateSurface = true;
    return;
  }

  aContainer->mEffectiveTransform =
    aContainer->SnapTransform(idealTransform, gfxRect(0, 0, 0, 0), &residual);
  // We always pass the ideal matrix down to our children, so there is no
  // need to apply any compensation using the residual from SnapTransform.
  aContainer->ComputeEffectiveTransformsForChildren(idealTransform);

  aContainer->ComputeEffectiveTransformForMaskLayer(aTransformToSurface);

  /* If we have a single child, it can just inherit our opacity,
   * otherwise we need a PushGroup and we need to mark ourselves as using
   * an intermediate surface so our children don't inherit our opacity
   * via GetEffectiveOpacity.
   * Having a mask layer always forces our own push group
   */
  aContainer->mUseIntermediateSurface =
    aContainer->GetMaskLayer() || (aContainer->GetEffectiveOpacity() != 1.0 &&
                                   aContainer->HasMultipleChildren());
}

class BasicContainerLayer : public ContainerLayer, public BasicImplData {
  template<class Container>
  friend void ContainerInsertAfter(Layer* aChild, Layer* aAfter, Container* aContainer);
  template<class Container>
  friend void ContainerRemoveChild(Layer* aChild, Container* aContainer);
  template<class Container>
  friend void ContainerComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface,
                                                  Container* aContainer);

public:
  BasicContainerLayer(BasicLayerManager* aManager) :
    ContainerLayer(aManager, static_cast<BasicImplData*>(this))
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
  virtual void InsertAfter(Layer* aChild, Layer* aAfter)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ContainerInsertAfter(aChild, aAfter, this);
  }

  virtual void RemoveChild(Layer* aChild)
  { 
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ContainerRemoveChild(aChild, this);
  }

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    ContainerComputeEffectiveTransforms(aTransformToSurface, this);
  }

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

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};
}
}

#endif
