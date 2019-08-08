/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZInputBridgeChild_h
#define mozilla_layers_APZInputBridgeChild_h

#include "mozilla/layers/APZInputBridge.h"
#include "mozilla/layers/PAPZInputBridgeChild.h"

namespace mozilla {
namespace layers {

class APZInputBridgeChild : public PAPZInputBridgeChild, public APZInputBridge {
  NS_INLINE_DECL_REFCOUNTING(APZInputBridgeChild, final)

 public:
  APZInputBridgeChild();
  void Destroy();

  nsEventStatus ReceiveInputEvent(InputData& aEvent,
                                  ScrollableLayerGuid* aOutTargetGuid,
                                  uint64_t* aOutInputBlockId) override;

 protected:
  void ProcessUnhandledEvent(LayoutDeviceIntPoint* aRefPoint,
                             ScrollableLayerGuid* aOutTargetGuid,
                             uint64_t* aOutFocusSequenceNumber,
                             LayersId* aOutLayersId) override;

  void UpdateWheelTransaction(LayoutDeviceIntPoint aRefPoint,
                              EventMessage aEventMessage) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual ~APZInputBridgeChild();

 private:
  bool mDestroyed;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_APZInputBridgeChild_h
