/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CompositorSession.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "base/process_util.h"

namespace mozilla {
namespace layers {

using namespace widget;


CompositorSession::CompositorSession(CompositorWidgetDelegate* aDelegate,
                                     CompositorBridgeChild* aChild,
                                     const uint64_t& aRootLayerTreeId)
 : mCompositorWidgetDelegate(aDelegate),
   mCompositorBridgeChild(aChild),
   mRootLayerTreeId(aRootLayerTreeId)
{
}

CompositorSession::~CompositorSession()
{
}

CompositorBridgeChild*
CompositorSession::GetCompositorBridgeChild()
{
  return mCompositorBridgeChild;
}

} // namespace layers
} // namespace mozilla
