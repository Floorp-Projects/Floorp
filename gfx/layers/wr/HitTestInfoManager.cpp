/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HitTestInfoManager.h"
#include "HitTestInfo.h"

#include "nsDisplayList.h"

#define DEBUG_HITTEST_INFO 0
#if DEBUG_HITTEST_INFO
#  define HITTEST_INFO_LOG(...) printf_stderr(__VA_ARGS__)
#else
#  define HITTEST_INFO_LOG(...)
#endif

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
                                    nsDisplayItem* aItem, const nsRect& aArea,
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

HitTestInfoManager::HitTestInfoManager()
    : mFlags(gfx::CompositorHitTestInvisibleToHit),
      mViewId(ScrollableLayerGuid::NULL_SCROLL_ID),
      mSpaceAndClipChain(wr::InvalidScrollNodeWithChain()) {}

void HitTestInfoManager::Reset() {
  mArea = nsRect();
  mFlags = gfx::CompositorHitTestInvisibleToHit;
  mViewId = ScrollableLayerGuid::NULL_SCROLL_ID;
  mSpaceAndClipChain = wr::InvalidScrollNodeWithChain();

  HITTEST_INFO_LOG("* HitTestInfoManager::Reset\n");
}

bool HitTestInfoManager::ProcessItem(
    nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
    nsDisplayListBuilder* aDisplayListBuilder) {
  MOZ_ASSERT(aItem);

  HITTEST_INFO_LOG("* HitTestInfoManager::ProcessItem(%d, %s, has=%d)\n",
                   getpid(), aItem->Frame()->ListTag().get(),
                   aItem->HasHitTestInfo());

  if (MOZ_UNLIKELY(aItem->GetType() == DisplayItemType::TYPE_REMOTE)) {
    // Remote frames might contain hit-test-info items inside (but those
    // aren't processed by this process of course), so we can't optimize out the
    // next hit-test info item because it might be on top of the iframe.
    Reset();
  }

  if (!aItem->HasHitTestInfo()) {
    return false;
  }

  const HitTestInfo& hitTestInfo = aItem->GetHitTestInfo();
  const nsRect& area = hitTestInfo.Area();
  const gfx::CompositorHitTestInfo& flags = hitTestInfo.Info();

  if (flags == gfx::CompositorHitTestInvisibleToHit || area.IsEmpty()) {
    return false;
  }

  const auto viewId =
      hitTestInfo.GetViewId(aBuilder, aItem->GetActiveScrolledRoot());
  const auto spaceAndClipChain = aBuilder.CurrentSpaceAndClipChain();

  if (!Update(area, flags, viewId, spaceAndClipChain)) {
    // The previous hit test information is still valid.
    return false;
  }

  HITTEST_INFO_LOG("+ [%d, %d, %d, %d]: flags: 0x%x, viewId: %lu\n", area.x,
                   area.y, area.width, area.height, flags.serialize(), viewId);

  CreateWebRenderCommands(aBuilder, aItem, area, flags, viewId);

  return true;
}

/**
 * Updates the current hit testing information if necessary.
 * Returns true if the hit testing information was changed.
 */
bool HitTestInfoManager::Update(const nsRect& aArea,
                                const gfx::CompositorHitTestInfo& aFlags,
                                const ViewID& aViewId,
                                const wr::WrSpaceAndClipChain& aSpaceAndClip) {
  if (mViewId == aViewId && mFlags == aFlags && mArea.Contains(aArea) &&
      mSpaceAndClipChain == aSpaceAndClip) {
    // The previous hit testing information can be reused.
    HITTEST_INFO_LOG("s [%d, %d, %d, %d]: flags: 0x%x, viewId: %lu\n", aArea.x,
                     aArea.y, aArea.width, aArea.height, aFlags.serialize(),
                     aViewId);
    return false;
  }

  mArea = aArea;
  mFlags = aFlags;
  mViewId = aViewId;
  mSpaceAndClipChain = aSpaceAndClip;
  return true;
}

}  // namespace mozilla::layers
