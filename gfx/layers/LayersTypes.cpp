/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayersTypes.h"

namespace mozilla {
namespace layers {

const char* kCompositionPayloadTypeNames[kCompositionPayloadTypeCount] = {
    "KeyPress",
    "APZScroll",
    "APZPinchZoom",
    "ContentPaint",
};

const char* GetLayersBackendName(LayersBackend aBackend) {
  switch (aBackend) {
    case LayersBackend::LAYERS_NONE:
      return "none";
    case LayersBackend::LAYERS_OPENGL:
      return "opengl";
    case LayersBackend::LAYERS_D3D11:
      return "d3d11";
    case LayersBackend::LAYERS_CLIENT:
      return "client";
    case LayersBackend::LAYERS_WR:
      return "webrender";
    case LayersBackend::LAYERS_BASIC:
      return "basic";
    default:
      MOZ_ASSERT_UNREACHABLE("unknown layers backend");
      return "unknown";
  }
}

EventRegions::EventRegions() : mDTCRequiresTargetConfirmation(false) {}

EventRegions::EventRegions(nsIntRegion aHitRegion)
    : mHitRegion(aHitRegion), mDTCRequiresTargetConfirmation(false) {}

EventRegions::EventRegions(const nsIntRegion& aHitRegion,
                           const nsIntRegion& aMaybeHitRegion,
                           const nsIntRegion& aDispatchToContentRegion,
                           const nsIntRegion& aNoActionRegion,
                           const nsIntRegion& aHorizontalPanRegion,
                           const nsIntRegion& aVerticalPanRegion,
                           bool aDTCRequiresTargetConfirmation) {
  mHitRegion = aHitRegion;
  mNoActionRegion = aNoActionRegion;
  mHorizontalPanRegion = aHorizontalPanRegion;
  mVerticalPanRegion = aVerticalPanRegion;
  // Points whose hit-region status we're not sure about need to be dispatched
  // to the content thread. If a point is in both maybeHitRegion and hitRegion
  // then it's not a "maybe" any more, and doesn't go into the dispatch-to-
  // content region.
  mDispatchToContentHitRegion.Sub(aMaybeHitRegion, mHitRegion);
  mDispatchToContentHitRegion.OrWith(aDispatchToContentRegion);
  mHitRegion.OrWith(aMaybeHitRegion);
  mDTCRequiresTargetConfirmation = aDTCRequiresTargetConfirmation;
}

bool EventRegions::operator==(const EventRegions& aRegions) const {
  return mHitRegion == aRegions.mHitRegion &&
         mDispatchToContentHitRegion == aRegions.mDispatchToContentHitRegion &&
         mNoActionRegion == aRegions.mNoActionRegion &&
         mHorizontalPanRegion == aRegions.mHorizontalPanRegion &&
         mVerticalPanRegion == aRegions.mVerticalPanRegion &&
         mDTCRequiresTargetConfirmation ==
             aRegions.mDTCRequiresTargetConfirmation;
}

bool EventRegions::operator!=(const EventRegions& aRegions) const {
  return !(*this == aRegions);
}

void EventRegions::ApplyTranslationAndScale(float aXTrans, float aYTrans,
                                            float aXScale, float aYScale) {
  mHitRegion.ScaleRoundOut(aXScale, aYScale);
  mDispatchToContentHitRegion.ScaleRoundOut(aXScale, aYScale);
  mNoActionRegion.ScaleRoundOut(aXScale, aYScale);
  mHorizontalPanRegion.ScaleRoundOut(aXScale, aYScale);
  mVerticalPanRegion.ScaleRoundOut(aXScale, aYScale);

  mHitRegion.MoveBy(aXTrans, aYTrans);
  mDispatchToContentHitRegion.MoveBy(aXTrans, aYTrans);
  mNoActionRegion.MoveBy(aXTrans, aYTrans);
  mHorizontalPanRegion.MoveBy(aXTrans, aYTrans);
  mVerticalPanRegion.MoveBy(aXTrans, aYTrans);
}

void EventRegions::Transform(const gfx::Matrix4x4& aTransform) {
  mHitRegion.Transform(aTransform);
  mDispatchToContentHitRegion.Transform(aTransform);
  mNoActionRegion.Transform(aTransform);
  mHorizontalPanRegion.Transform(aTransform);
  mVerticalPanRegion.Transform(aTransform);
}

void EventRegions::OrWith(const EventRegions& aOther) {
  mHitRegion.OrWith(aOther.mHitRegion);
  mDispatchToContentHitRegion.OrWith(aOther.mDispatchToContentHitRegion);
  // See the comment in nsDisplayList::AddFrame, where the touch action
  // regions are handled. The same thing applies here.
  bool alreadyHadRegions = !mNoActionRegion.IsEmpty() ||
                           !mHorizontalPanRegion.IsEmpty() ||
                           !mVerticalPanRegion.IsEmpty();
  mNoActionRegion.OrWith(aOther.mNoActionRegion);
  mHorizontalPanRegion.OrWith(aOther.mHorizontalPanRegion);
  mVerticalPanRegion.OrWith(aOther.mVerticalPanRegion);
  if (alreadyHadRegions) {
    nsIntRegion combinedActionRegions;
    combinedActionRegions.Or(mHorizontalPanRegion, mVerticalPanRegion);
    combinedActionRegions.OrWith(mNoActionRegion);
    mDispatchToContentHitRegion.OrWith(combinedActionRegions);
  }
  mDTCRequiresTargetConfirmation |= aOther.mDTCRequiresTargetConfirmation;
}

bool EventRegions::IsEmpty() const {
  return mHitRegion.IsEmpty() && mDispatchToContentHitRegion.IsEmpty() &&
         mNoActionRegion.IsEmpty() && mHorizontalPanRegion.IsEmpty() &&
         mVerticalPanRegion.IsEmpty();
}

void EventRegions::SetEmpty() {
  mHitRegion.SetEmpty();
  mDispatchToContentHitRegion.SetEmpty();
  mNoActionRegion.SetEmpty();
  mHorizontalPanRegion.SetEmpty();
  mVerticalPanRegion.SetEmpty();
}

nsCString EventRegions::ToString() const {
  nsCString result = mHitRegion.ToString();
  result.AppendLiteral(";dispatchToContent=");
  result.Append(mDispatchToContentHitRegion.ToString());
  return result;
}

}  // namespace layers
}  // namespace mozilla
