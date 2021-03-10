/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayersTypes.h"

#include <cinttypes>
#include "nsPrintfCString.h"
#include "mozilla/gfx/gfxVars.h"

#ifdef XP_WIN
#  include "gfxConfig.h"
#  include "mozilla/StaticPrefs_gfx.h"
#endif

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
      MOZ_ASSERT(gfx::gfxVars::UseWebRender());
      if (gfx::gfxVars::UseSoftwareWebRender()) {
#ifdef XP_WIN
        if (gfx::gfxVars::AllowSoftwareWebRenderD3D11() &&
            gfx::gfxConfig::IsEnabled(gfx::Feature::D3D11_COMPOSITING)) {
          return "webrender_software_d3d11";
        }
#endif
        return "webrender_software";
      }
      return "webrender";
    case LayersBackend::LAYERS_BASIC:
      return "basic";
    default:
      MOZ_ASSERT_UNREACHABLE("unknown layers backend");
      return "unknown";
  }
}

std::ostream& operator<<(std::ostream& aStream, const LayersId& aId) {
  return aStream << nsPrintfCString("0x%" PRIx64, aId.mId).get();
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

std::ostream& operator<<(std::ostream& aStream, const EventRegions& e) {
  aStream << "{";
  if (!e.mHitRegion.IsEmpty()) {
    aStream << " Hit=" << e.mHitRegion;
  }
  if (!e.mDispatchToContentHitRegion.IsEmpty()) {
    aStream << " DispatchToContent=" << e.mDispatchToContentHitRegion;
  }
  if (!e.mNoActionRegion.IsEmpty()) {
    aStream << " NoAction=" << e.mNoActionRegion;
  }
  if (!e.mHorizontalPanRegion.IsEmpty()) {
    aStream << " HorizontalPan=" << e.mHorizontalPanRegion;
  }
  if (!e.mVerticalPanRegion.IsEmpty()) {
    aStream << " VerticalPan=" << e.mVerticalPanRegion;
  }
  aStream << " }";
  return aStream;
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

}  // namespace layers
}  // namespace mozilla
