/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/OMTAController.h"

#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/StaticPrefs_layout.h"

namespace mozilla {
namespace layers {

void OMTAController::NotifyJankedAnimations(
    JankedAnimations&& aJankedAnimations) const {
  if (StaticPrefs::layout_animation_prerender_partial_jank()) {
    return;
  }

  if (!CompositorThread()) {
    return;
  }

  if (!CompositorThread()->IsOnCurrentThread()) {
    CompositorThread()->Dispatch(NewRunnableMethod<JankedAnimations&&>(
        "layers::OMTAController::NotifyJankedAnimations", this,
        &OMTAController::NotifyJankedAnimations, std::move(aJankedAnimations)));
    return;
  }

  if (CompositorBridgeParent* bridge =
          CompositorBridgeParent::GetCompositorBridgeParentFromLayersId(
              mRootLayersId)) {
    bridge->NotifyJankedAnimations(aJankedAnimations);
  }
}

}  // namespace layers
}  // namespace mozilla
