/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_HITTESTINFO_H
#define GFX_HITTESTINFO_H

#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/layers/ScrollableLayerGuid.h"
#include "nsRect.h"

class nsIFrame;

namespace mozilla {

class nsDisplayListBuilder;
struct ActiveScrolledRoot;

namespace wr {
class DisplayListBuilder;
}  // namespace wr

/**
 * A helper class that manages compositor hit testing information.
 */
class HitTestInfo {
 public:
  using CompositorHitTestInfo = gfx::CompositorHitTestInfo;
  using ViewID = layers::ScrollableLayerGuid::ViewID;

  ViewID GetViewId(wr::DisplayListBuilder& aBuilder,
                   const ActiveScrolledRoot* aASR) const;

  void Initialize(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame);
  void InitializeScrollTarget(nsDisplayListBuilder* aBuilder);

  void SetAreaAndInfo(const nsRect& aArea, const CompositorHitTestInfo& aInfo) {
    mArea = aArea;
    mInfo = aInfo;
  }

  static void Shutdown();

  const nsRect& Area() const { return mArea; }
  const CompositorHitTestInfo& Info() const { return mInfo; }

  static const HitTestInfo& Empty();

 private:
  nsRect mArea;
  CompositorHitTestInfo mInfo;
  mozilla::Maybe<ViewID> mScrollTarget;
};

}  // namespace mozilla

#endif
