/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_RepaintRequestRunnable_h
#define mozilla_layers_RepaintRequestRunnable_h

#include <unordered_map>

#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/RepaintRequest.h"
#include "mozilla/layers/ScrollableLayerGuid.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layers {

class GeckoContentController;

// A runnable invoked in nsRefreshDriver::Tick as an early runnable.
class APZTaskRunnable final : public Runnable {
 public:
  explicit APZTaskRunnable(GeckoContentController* aController)
      : Runnable("RepaintRequestRunnable"),
        mController(aController),
        mRegisteredPresShellId(0),
        mNeedsFlushCompleteNotification(false) {}

  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_DECL_NSIRUNNABLE

  // Queue a RepaintRequest.
  // If there's already a RepaintRequest having the same scroll id, the old
  // one will be discarded.
  void QueueRequest(const RepaintRequest& aRequest);
  void QueueFlushCompleteNotification();
  void Revoke() {
    mController = nullptr;
    mRegisteredPresShellId = 0;
  }

 private:
  void EnsureRegisterAsEarlyRunner();
  bool IsRegistereddWithCurrentPresShell() const;
  bool IsTestControllingRefreshesEnabled() const;

  // Use a GeckoContentController raw pointer here since the owner of the
  // GeckoContentController instance (an APZChild instance) holds a strong
  // reference of this APZTaskRunnable instance and will call Revoke() before
  // the GeckoContentController gets destroyed in the dtor of the APZChild
  // instance.
  GeckoContentController* mController;

  using RepaintRequests =
      std::unordered_map<ScrollableLayerGuid::ViewID, RepaintRequest>;
  RepaintRequests mPendingRepaintRequests;
  // This APZTaskRunnable instance is per APZChild instance, which means its
  // lifetime is tied to the APZChild instance, thus this APZTaskRunnable
  // instance will be (re-)used for different pres shells so we'd need to
  // have to remember the pres shell which is currently tied to the APZChild
  // to deliver queued requests and notifications to the proper pres shell.
  uint32_t mRegisteredPresShellId;
  bool mNeedsFlushCompleteNotification;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_RepaintRequestRunnable_h
