/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HitTestInfoManager.h"
#include "HitTestInfo.h"

#include "nsDisplayList.h"

namespace mozilla::layers {

using ViewID = ScrollableLayerGuid::ViewID;

/**
 * TODO(miko): This used to be a performance bottle-neck, but it does not show
 * up in profiles anymore, see bugs 1424637 and 1424968.
 * A better way of doing this would be to store current app units per dev pixel
 * in wr::DisplayListBuilder, and update it whenever display items that separate
 * presshell boundaries are encountered.
 */
static int32_t GetAppUnitsFromDisplayItem(nsDisplayItem* aItem) {
  nsIFrame* frame = aItem->Frame();
  MOZ_ASSERT(frame);
  return frame->PresContext()->AppUnitsPerDevPixel();
}

static void CreateWebRenderCommands(wr::DisplayListBuilder& aBuilder,
                                    nsPaintedDisplayItem* aItem,
                                    const nsRect& aArea,
                                    const gfx::CompositorHitTestInfo& aFlags,
                                    const ViewID& aViewId) {
  const Maybe<SideBits> sideBits =
      aBuilder.GetContainingFixedPosSideBits(aItem->GetActiveScrolledRoot());

  const LayoutDeviceRect devRect =
      LayoutDeviceRect::FromAppUnits(aArea, GetAppUnitsFromDisplayItem(aItem));
  const wr::LayoutRect rect = wr::ToLayoutRect(devRect);

  aBuilder.PushHitTest(rect, rect, !aItem->BackfaceIsHidden(), aViewId, aFlags,
                       sideBits.valueOr(SideBits::eNone));
}

void HitTestInfoManager::Reset() {
  mArea = nsRect();
  mFlags = gfx::CompositorHitTestInvisibleToHit;
}

void HitTestInfoManager::SwitchItem(nsPaintedDisplayItem* aItem,
                                    wr::DisplayListBuilder& aBuilder,
                                    nsDisplayListBuilder* aDisplayListBuilder) {
  if (!aItem || aItem->HasChildren()) {
    // Either the item is not a painted display item, or it is a container item.
    return;
  }

  const HitTestInfo& hitTestInfo = aItem->GetHitTestInfo();
  const nsRect& area = hitTestInfo.Area();
  const gfx::CompositorHitTestInfo& flags = hitTestInfo.Info();

  if (area.IsEmpty() || flags == gfx::CompositorHitTestInvisibleToHit) {
    return;
  }

  const auto viewId =
      hitTestInfo.GetViewId(aBuilder, aItem->GetActiveScrolledRoot());
  const auto spaceAndClipChain = aBuilder.CurrentSpaceAndClipChain();

  if (!Update(area, flags, viewId, spaceAndClipChain)) {
    // The previous hit test information is still valid.
    return;
  }

  CreateWebRenderCommands(aBuilder, aItem, area, flags, viewId);
}

bool HitTestInfoManager::Update(const nsRect& aArea,
                                const gfx::CompositorHitTestInfo& aFlags,
                                const ViewID& aViewId,
                                const wr::WrSpaceAndClipChain& aSpaceAndClip) {
  if (mViewId == aViewId && mFlags == aFlags && mArea.Contains(aArea) &&
      mSpaceAndClipChain == aSpaceAndClip) {
    // The previous hit testing information can be reused.
    return false;
  }

  mArea = aArea;
  mFlags = aFlags;
  mViewId = aViewId;
  mSpaceAndClipChain = aSpaceAndClip;
  return true;
}

}  // namespace mozilla::layers
