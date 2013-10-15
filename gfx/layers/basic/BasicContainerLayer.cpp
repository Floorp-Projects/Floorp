/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicContainerLayer.h"
#include <sys/types.h>                  // for int32_t
#include "BasicLayersImpl.h"            // for ToData
#include "basic/BasicImplData.h"        // for BasicImplData
#include "basic/BasicLayers.h"          // for BasicLayerManager
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "mozilla/mozalloc.h"           // for operator new
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxMatrix.h"                  // for gfxMatrix
#include "nsRegion.h"                   // for nsIntRegion

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

BasicContainerLayer::~BasicContainerLayer()
{
  while (mFirstChild) {
    ContainerLayer::RemoveChild(mFirstChild);
  }

  MOZ_COUNT_DTOR(BasicContainerLayer);
}

void
BasicContainerLayer::ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
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

  mEffectiveTransform = SnapTransformTranslation(idealTransform, &residual);
  // We always pass the ideal matrix down to our children, so there is no
  // need to apply any compensation using the residual from SnapTransformTranslation.
  ComputeEffectiveTransformsForChildren(idealTransform);

  ComputeEffectiveTransformForMaskLayer(aTransformToSurface);
  
  Layer* child = GetFirstChild();
  bool hasSingleBlendingChild = false;
  if (!HasMultipleChildren() && child) {
    hasSingleBlendingChild = child->GetMixBlendMode() != gfxContext::OPERATOR_OVER;
  }

  /* If we have a single childand it is not blending,, it can just inherit our opacity,
   * otherwise we need a PushGroup and we need to mark ourselves as using
   * an intermediate surface so our children don't inherit our opacity
   * via GetEffectiveOpacity.
   * Having a mask layer always forces our own push group
   * Having a blend mode also always forces our own push group
   */
  mUseIntermediateSurface =
    GetMaskLayer() ||
    GetForceIsolatedGroup() ||
    (GetMixBlendMode() != gfxContext::OPERATOR_OVER && HasMultipleChildren()) ||
    (GetEffectiveOpacity() != 1.0 && (HasMultipleChildren() || hasSingleBlendingChild));
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

void
BasicContainerLayer::Validate(LayerManager::DrawThebesLayerCallback aCallback,
                              void* aCallbackData)
{
  for (Layer* l = mFirstChild; l; l = l->GetNextSibling()) {
    BasicImplData* data = ToData(l);
    data->Validate(aCallback, aCallbackData);
    if (l->GetMaskLayer()) {
      data = ToData(l->GetMaskLayer());
      data->Validate(aCallback, aCallbackData);
    }
  }
}

already_AddRefed<ContainerLayer>
BasicLayerManager::CreateContainerLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ContainerLayer> layer = new BasicContainerLayer(this);
  return layer.forget();
}

}
}
