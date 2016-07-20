/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_ipc_CompositorWidgetVsyncObserver_h
#define mozilla_gfx_ipc_CompositorWidgetVsyncObserver_h

#include "mozilla/VsyncDispatcher.h"

namespace mozilla {
namespace gfx {
class VsyncBridgeChild;
} // namespace gfx

namespace widget {

class CompositorWidgetVsyncObserver : public VsyncObserver
{
  typedef gfx::VsyncBridgeChild VsyncBridgeChild;

 public:
  CompositorWidgetVsyncObserver(RefPtr<VsyncBridgeChild> aVsyncBridge,
                                const uint64_t& aRootLayerTreeId);

  bool NotifyVsync(TimeStamp aVsyncTimestamp) override;

 private:
  RefPtr<VsyncBridgeChild> mVsyncBridge;
  uint64_t mRootLayerTreeId;
};

} // namespace widget
} // namespace mozilla

#endif // mozilla_gfx_ipc_CompositorWidgetVsyncObserver_h
