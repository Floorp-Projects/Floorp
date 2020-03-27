/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZCTreeManagerParent_h
#define mozilla_layers_APZCTreeManagerParent_h

#include "mozilla/layers/PAPZCTreeManagerParent.h"

namespace mozilla {
namespace layers {

class APZCTreeManager;
class APZUpdater;

class APZCTreeManagerParent : public PAPZCTreeManagerParent {
 public:
  APZCTreeManagerParent(LayersId aLayersId,
                        RefPtr<APZCTreeManager> aAPZCTreeManager,
                        RefPtr<APZUpdater> mAPZUpdater);
  virtual ~APZCTreeManagerParent();

  LayersId GetLayersId() const { return mLayersId; }

  /**
   * Called when the layer tree that this protocol is connected to
   * is adopted by another compositor, and we need to switch APZCTreeManagers.
   */
  void ChildAdopted(RefPtr<APZCTreeManager> aAPZCTreeManager,
                    RefPtr<APZUpdater> aAPZUpdater);

  mozilla::ipc::IPCResult RecvSetKeyboardMap(const KeyboardMap& aKeyboardMap);

  mozilla::ipc::IPCResult RecvZoomToRect(const SLGuidAndRenderRoot& aGuid,
                                         const CSSRect& aRect,
                                         const uint32_t& aFlags);

  mozilla::ipc::IPCResult RecvContentReceivedInputBlock(
      const uint64_t& aInputBlockId, const bool& aPreventDefault);

  mozilla::ipc::IPCResult RecvSetTargetAPZC(
      const uint64_t& aInputBlockId, nsTArray<SLGuidAndRenderRoot>&& aTargets);

  mozilla::ipc::IPCResult RecvUpdateZoomConstraints(
      const SLGuidAndRenderRoot& aGuid,
      const MaybeZoomConstraints& aConstraints);

  mozilla::ipc::IPCResult RecvSetDPI(const float& aDpiValue);

  mozilla::ipc::IPCResult RecvSetAllowedTouchBehavior(
      const uint64_t& aInputBlockId, nsTArray<TouchBehaviorFlags>&& aValues);

  mozilla::ipc::IPCResult RecvStartScrollbarDrag(
      const SLGuidAndRenderRoot& aGuid, const AsyncDragMetrics& aDragMetrics);

  mozilla::ipc::IPCResult RecvStartAutoscroll(
      const SLGuidAndRenderRoot& aGuid, const ScreenPoint& aAnchorLocation);

  mozilla::ipc::IPCResult RecvStopAutoscroll(const SLGuidAndRenderRoot& aGuid);

  mozilla::ipc::IPCResult RecvSetLongTapEnabled(const bool& aTapGestureEnabled);

  void ActorDestroy(ActorDestroyReason aWhy) override {}

 private:
  bool IsGuidValid(const SLGuidAndRenderRoot& aGuid);

  LayersId mLayersId;
  RefPtr<APZCTreeManager> mTreeManager;
  RefPtr<APZUpdater> mUpdater;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_APZCTreeManagerParent_h
