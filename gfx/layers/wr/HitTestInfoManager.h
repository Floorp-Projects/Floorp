/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_HITTESTINFOMANAGER_H
#define GFX_HITTESTINFOMANAGER_H

#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/layers/ScrollableLayerGuid.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "nsRect.h"

namespace mozilla {

class nsDisplayItem;
class nsDisplayListBuilder;

namespace wr {
class DisplayListBuilder;
}

namespace layers {

/**
 * This class extracts the hit testing information (area, flags, ViewId) from
 * Gecko display items and pushes them into WebRender display list.
 *
 * The hit testing information is deduplicated: a new hit test item is only
 * added if the new area is not contained in the previous area, or if the flags,
 * ViewId, or current spatial id is different.
 */
class HitTestInfoManager {
 public:
  HitTestInfoManager();

  /**
   * Resets the previous hit testing information.
   */
  void Reset();

  /**
   * Extracts the hit testing information from |aItem|, and if necessary, adds
   * a new WebRender hit test item using |aBuilder|.
   *
   * Returns true if a hit test item was pushed.
   */
  bool ProcessItem(nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
                   nsDisplayListBuilder* aDisplayListBuilder);

 private:
  bool Update(const nsRect& aArea, const gfx::CompositorHitTestInfo& aFlags,
              const ScrollableLayerGuid::ViewID& aViewId,
              const wr::WrSpaceAndClipChain& aSpaceAndClip);

  nsRect mArea;
  gfx::CompositorHitTestInfo mFlags;
  ScrollableLayerGuid::ViewID mViewId;
  wr::WrSpaceAndClipChain mSpaceAndClipChain;
};

}  // namespace layers
}  // namespace mozilla

#endif
