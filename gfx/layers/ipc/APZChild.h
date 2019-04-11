/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZChild_h
#define mozilla_layers_APZChild_h

#include "mozilla/layers/PAPZChild.h"

namespace mozilla {

namespace layers {

class GeckoContentController;

/**
 * APZChild implements PAPZChild and is used to remote a GeckoContentController
 * that lives in a different process than where APZ lives.
 */
class APZChild final : public PAPZChild {
 public:
  explicit APZChild(RefPtr<GeckoContentController> aController);
  virtual ~APZChild();

  mozilla::ipc::IPCResult RecvLayerTransforms(
      const nsTArray<MatrixMessage>& aTransforms);

  mozilla::ipc::IPCResult RecvRequestContentRepaint(
      const RepaintRequest& aRequest);

  mozilla::ipc::IPCResult RecvUpdateOverscrollVelocity(
      const float& aX, const float& aY, const bool& aIsRootContent);

  mozilla::ipc::IPCResult RecvUpdateOverscrollOffset(
      const float& aX, const float& aY, const bool& aIsRootContent);

  mozilla::ipc::IPCResult RecvNotifyMozMouseScrollEvent(const ViewID& aScrollId,
                                                        const nsString& aEvent);

  mozilla::ipc::IPCResult RecvNotifyAPZStateChange(
      const ScrollableLayerGuid& aGuid, const APZStateChange& aChange,
      const int& aArg);

  mozilla::ipc::IPCResult RecvNotifyFlushComplete();

  mozilla::ipc::IPCResult RecvNotifyAsyncScrollbarDragInitiated(
      const uint64_t& aDragBlockId, const ViewID& aScrollId,
      const ScrollDirection& aDirection);
  mozilla::ipc::IPCResult RecvNotifyAsyncScrollbarDragRejected(
      const ViewID& aScrollId);

  mozilla::ipc::IPCResult RecvNotifyAsyncAutoscrollRejected(
      const ViewID& aScrollId);

  mozilla::ipc::IPCResult RecvDestroy();

 private:
  RefPtr<GeckoContentController> mController;
};

}  // namespace layers

}  // namespace mozilla

#endif  // mozilla_layers_APZChild_h
