/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollAnchorContainer.h"

#include "nsGfxScrollFrame.h"
#include "nsLayoutUtils.h"

#define ANCHOR_LOG(...)
// #define ANCHOR_LOG(...) printf_stderr("ANCHOR: " __VA_ARGS__)

namespace mozilla {
namespace layout {

ScrollAnchorContainer::ScrollAnchorContainer(ScrollFrameHelper* aScrollFrame)
    : mScrollFrame(aScrollFrame) {}

ScrollAnchorContainer::~ScrollAnchorContainer() {}

ScrollAnchorContainer* ScrollAnchorContainer::FindFor(nsIFrame* aFrame) {
  aFrame = aFrame->GetParent();
  if (!aFrame) {
    return nullptr;
  }
  nsIScrollableFrame* nearest = nsLayoutUtils::GetNearestScrollableFrame(
      aFrame, nsLayoutUtils::SCROLLABLE_SAME_DOC |
                  nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN);
  if (nearest) {
    return nearest->GetAnchor();
  }
  return nullptr;
}

nsIFrame* ScrollAnchorContainer::Frame() const { return mScrollFrame->mOuter; }

nsIScrollableFrame* ScrollAnchorContainer::ScrollableFrame() const {
  return Frame()->GetScrollTargetFrame();
}

}  // namespace layout
}  // namespace mozilla
