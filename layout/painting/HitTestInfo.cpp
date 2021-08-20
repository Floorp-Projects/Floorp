/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HitTestInfo.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "nsDisplayList.h"
#include "nsIFrame.h"

namespace mozilla {

static const HitTestInfo* gEmptyHitTestInfo = nullptr;

const HitTestInfo& HitTestInfo::Empty() {
  if (gEmptyHitTestInfo) {
    gEmptyHitTestInfo = new HitTestInfo();
  }

  return *gEmptyHitTestInfo;
}

void HitTestInfo::Shutdown() {
  delete gEmptyHitTestInfo;
  gEmptyHitTestInfo = nullptr;
}

using ViewID = layers::ScrollableLayerGuid::ViewID;

ViewID HitTestInfo::GetViewId(wr::DisplayListBuilder& aBuilder,
                              const ActiveScrolledRoot* aASR) const {
  if (mScrollTarget) {
    return *mScrollTarget;
  }

  Maybe<ViewID> fixedTarget = aBuilder.GetContainingFixedPosScrollTarget(aASR);

  if (fixedTarget) {
    return *fixedTarget;
  }

  if (aASR) {
    return aASR->GetViewId();
  }

  return ScrollableLayerGuid::NULL_SCROLL_ID;
}

void HitTestInfo::Initialize(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame) {
  if (!aBuilder->BuildCompositorHitTestInfo()) {
    return;
  }

  mInfo = aFrame->GetCompositorHitTestInfo(aBuilder);
  if (mInfo != gfx::CompositorHitTestInvisibleToHit) {
    mArea = aFrame->GetCompositorHitTestArea(aBuilder);
    InitializeScrollTarget(aBuilder);
  }
}

void HitTestInfo::InitializeScrollTarget(nsDisplayListBuilder* aBuilder) {
  if (aBuilder->GetCurrentScrollbarDirection().isSome()) {
    // In the case of scrollbar frames, we use the scrollbar's target
    // scrollframe instead of the scrollframe with which the scrollbar actually
    // moves.
    MOZ_ASSERT(Info().contains(CompositorHitTestFlags::eScrollbar));
    mScrollTarget = Some(aBuilder->GetCurrentScrollbarTarget());
  }
}

}  // namespace mozilla
