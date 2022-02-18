/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZCTreeManagerChild_h
#define mozilla_layers_APZCTreeManagerChild_h

#include "mozilla/layers/APZInputBridge.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/PAPZCTreeManagerChild.h"

#include <unordered_map>

namespace mozilla {
namespace layers {

class APZInputBridgeChild;
class RemoteCompositorSession;

class APZCTreeManagerChild : public IAPZCTreeManager,
                             public PAPZCTreeManagerChild {
  friend class PAPZCTreeManagerChild;
  using TapType = GeckoContentController_TapType;

 public:
  APZCTreeManagerChild();

  void SetCompositorSession(RemoteCompositorSession* aSession);
  void SetInputBridge(APZInputBridgeChild* aInputBridge);
  void Destroy();

  void SetKeyboardMap(const KeyboardMap& aKeyboardMap) override;

  void ZoomToRect(const ScrollableLayerGuid& aGuid,
                  const ZoomTarget& aZoomTarget,
                  const uint32_t aFlags = DEFAULT_BEHAVIOR) override;

  void ContentReceivedInputBlock(uint64_t aInputBlockId,
                                 bool aPreventDefault) override;

  void SetTargetAPZC(uint64_t aInputBlockId,
                     const nsTArray<ScrollableLayerGuid>& aTargets) override;

  void UpdateZoomConstraints(
      const ScrollableLayerGuid& aGuid,
      const Maybe<ZoomConstraints>& aConstraints) override;

  void SetDPI(float aDpiValue) override;

  void SetAllowedTouchBehavior(
      uint64_t aInputBlockId,
      const nsTArray<TouchBehaviorFlags>& aValues) override;

  void StartScrollbarDrag(const ScrollableLayerGuid& aGuid,
                          const AsyncDragMetrics& aDragMetrics) override;

  bool StartAutoscroll(const ScrollableLayerGuid& aGuid,
                       const ScreenPoint& aAnchorLocation) override;

  void StopAutoscroll(const ScrollableLayerGuid& aGuid) override;

  void SetLongTapEnabled(bool aTapGestureEnabled) override;

  APZInputBridge* InputBridge() override;

  void AddIPDLReference();
  void ReleaseIPDLReference();
  void ActorDestroy(ActorDestroyReason aWhy) override;

 protected:
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  mozilla::ipc::IPCResult RecvHandleTap(const TapType& aType,
                                        const LayoutDevicePoint& aPoint,
                                        const Modifiers& aModifiers,
                                        const ScrollableLayerGuid& aGuid,
                                        const uint64_t& aInputBlockId);

  mozilla::ipc::IPCResult RecvNotifyPinchGesture(
      const PinchGestureType& aType, const ScrollableLayerGuid& aGuid,
      const LayoutDevicePoint& aFocusPoint,
      const LayoutDeviceCoord& aSpanChange, const Modifiers& aModifiers);

  mozilla::ipc::IPCResult RecvCancelAutoscroll(
      const ScrollableLayerGuid::ViewID& aScrollId);

  virtual ~APZCTreeManagerChild();

 private:
  MOZ_NON_OWNING_REF RemoteCompositorSession* mCompositorSession;
  RefPtr<APZInputBridgeChild> mInputBridge;
  bool mIPCOpen;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_APZCTreeManagerChild_h
