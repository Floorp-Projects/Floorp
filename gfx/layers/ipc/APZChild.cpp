/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/GeckoContentController.h"

#include "mozilla/dom/BrowserChild.h"
#include "mozilla/layers/APZCCallbackHelper.h"

#include "InputData.h"  // for InputData

namespace mozilla {
namespace layers {

APZChild::APZChild(RefPtr<GeckoContentController> aController)
    : mController(aController) {
  MOZ_ASSERT(mController);
}

APZChild::~APZChild() {
  if (mController) {
    mController->Destroy();
    mController = nullptr;
  }
}

mozilla::ipc::IPCResult APZChild::RecvLayerTransforms(
    const nsTArray<MatrixMessage>& aTransforms) {
  mController->NotifyLayerTransforms(aTransforms);
  return IPC_OK();
}

mozilla::ipc::IPCResult APZChild::RecvRequestContentRepaint(
    const RepaintRequest& aRequest) {
  MOZ_ASSERT(mController->IsRepaintThread());

  mController->RequestContentRepaint(aRequest);
  return IPC_OK();
}

mozilla::ipc::IPCResult APZChild::RecvUpdateOverscrollVelocity(
    const float& aX, const float& aY, const bool& aIsRootContent) {
  mController->UpdateOverscrollVelocity(aX, aY, aIsRootContent);
  return IPC_OK();
}

mozilla::ipc::IPCResult APZChild::RecvUpdateOverscrollOffset(
    const float& aX, const float& aY, const bool& aIsRootContent) {
  mController->UpdateOverscrollOffset(aX, aY, aIsRootContent);
  return IPC_OK();
}

mozilla::ipc::IPCResult APZChild::RecvNotifyMozMouseScrollEvent(
    const ViewID& aScrollId, const nsString& aEvent) {
  mController->NotifyMozMouseScrollEvent(aScrollId, aEvent);
  return IPC_OK();
}

mozilla::ipc::IPCResult APZChild::RecvNotifyAPZStateChange(
    const ScrollableLayerGuid& aGuid, const APZStateChange& aChange,
    const int& aArg) {
  mController->NotifyAPZStateChange(aGuid, aChange, aArg);
  return IPC_OK();
}

mozilla::ipc::IPCResult APZChild::RecvNotifyFlushComplete() {
  MOZ_ASSERT(mController->IsRepaintThread());

  mController->NotifyFlushComplete();
  return IPC_OK();
}

mozilla::ipc::IPCResult APZChild::RecvNotifyAsyncScrollbarDragInitiated(
    const uint64_t& aDragBlockId, const ViewID& aScrollId,
    const ScrollDirection& aDirection) {
  mController->NotifyAsyncScrollbarDragInitiated(aDragBlockId, aScrollId,
                                                 aDirection);
  return IPC_OK();
}

mozilla::ipc::IPCResult APZChild::RecvNotifyAsyncScrollbarDragRejected(
    const ViewID& aScrollId) {
  mController->NotifyAsyncScrollbarDragRejected(aScrollId);
  return IPC_OK();
}

mozilla::ipc::IPCResult APZChild::RecvNotifyAsyncAutoscrollRejected(
    const ViewID& aScrollId) {
  mController->NotifyAsyncAutoscrollRejected(aScrollId);
  return IPC_OK();
}

mozilla::ipc::IPCResult APZChild::RecvDestroy() {
  // mController->Destroy will be called in the destructor
  PAPZChild::Send__delete__(this);
  return IPC_OK();
}

}  // namespace layers
}  // namespace mozilla
