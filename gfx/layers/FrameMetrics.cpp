/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameMetrics.h"
#include "gfxPrefs.h"

namespace mozilla {
namespace layers {

const FrameMetrics::ViewID FrameMetrics::NULL_SCROLL_ID = 0;

void
ScrollMetadata::SetUsesContainerScrolling(bool aValue) {
  MOZ_ASSERT_IF(aValue, gfxPrefs::LayoutUseContainersForRootFrames());
  mUsesContainerScrolling = aValue;
}

StaticAutoPtr<const ScrollMetadata> ScrollMetadata::sNullMetadata;

}
}
