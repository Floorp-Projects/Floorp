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
#include "nsRegion.h"                   // for nsIntRegion
#include "ReadbackProcessor.h"

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
BasicContainerLayer::ComputeEffectiveTransforms(const Matrix4x4& aTransformToSurface)
{
  // We push groups for container layers if we need to, which always
  // are aligned in device space, so it doesn't really matter how we snap
  // containers.
  Matrix residual;
  Matrix4x4 idealTransform = GetLocalTransform() * aTransformToSurface;
  idealTransform.ProjectTo2D();

  if (!idealTransform.CanDraw2D()) {
    mEffectiveTransform = idealTransform;
    ComputeEffectiveTransformsForChildren(Matrix4x4());
    ComputeEffectiveTransformForMaskLayer(Matrix4x4());
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
    hasSingleBlendingChild = child->GetMixBlendMode() != CompositionOp::OP_OVER;
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
    (GetMixBlendMode() != CompositionOp::OP_OVER && HasMultipleChildren()) ||
    (GetEffectiveOpacity() != 1.0 && (HasMultipleChildren() || hasSingleBlendingChild));
}

bool
BasicContainerLayer::ChildrenPartitionVisibleRegion(const gfx::IntRect& aInRect)
{
  Matrix transform;
  if (!GetEffectiveTransform().CanDraw2D(&transform) ||
      ThebesMatrix(transform).HasNonIntegerTranslation())
    return false;

  nsIntPoint offset(int32_t(transform._31), int32_t(transform._32));
  gfx::IntRect rect = aInRect.Intersect(GetEffectiveVisibleRegion().GetBounds() + offset);
  nsIntRegion covered;

  for (Layer* l = mFirstChild; l; l = l->GetNextSibling()) {
    if (ToData(l)->IsHidden())
      continue;

    Matrix childTransform;
    if (!l->GetEffectiveTransform().CanDraw2D(&childTransform) ||
        ThebesMatrix(childTransform).HasNonIntegerTranslation() ||
        l->GetEffectiveOpacity() != 1.0)
      return false;
    nsIntRegion childRegion = l->GetEffectiveVisibleRegion();
    childRegion.MoveBy(int32_t(childTransform._31), int32_t(childTransform._32));
    childRegion.And(childRegion, rect);
    if (l->GetClipRect()) {
      childRegion.And(childRegion, ParentLayerIntRect::ToUntyped(*l->GetClipRect()) + offset);
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
BasicContainerLayer::Validate(LayerManager::DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              ReadbackProcessor* aReadback)
{
  ReadbackProcessor readback;
  if (BasicManager()->IsRetained()) {
    readback.BuildUpdates(this);
  }
  for (Layer* l = mFirstChild; l; l = l->GetNextSibling()) {
    BasicImplData* data = ToData(l);
    data->Validate(aCallback, aCallbackData, &readback);
    if (l->GetMaskLayer()) {
      data = ToData(l->GetMaskLayer());
      data->Validate(aCallback, aCallbackData, nullptr);
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
