/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_MetricsSharingController_h
#define mozilla_layers_MetricsSharingController_h

#include "FrameMetrics.h"                   // for FrameMetrics
#include "mozilla/ipc/CrossProcessMutex.h"  // for CrossProcessMutexHandle
#include "mozilla/ipc/SharedMemoryBasic.h"  // for SharedMemoryBasic
#include "nsISupportsImpl.h"  // for NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

namespace mozilla {
namespace layers {

class MetricsSharingController {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual base::ProcessId RemotePid() = 0;
  virtual bool StartSharingMetrics(
      mozilla::ipc::SharedMemoryBasic::Handle aHandle,
      CrossProcessMutexHandle aMutexHandle, LayersId aLayersId,
      uint32_t aApzcId) = 0;
  virtual bool StopSharingMetrics(ScrollableLayerGuid::ViewID aScrollId,
                                  uint32_t aApzcId) = 0;

 protected:
  virtual ~MetricsSharingController() = default;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_MetricsSharingController_h
