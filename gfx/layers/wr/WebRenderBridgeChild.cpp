/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/WebRenderBridgeChild.h"

#include "mozilla/layers/WebRenderBridgeParent.h"

namespace mozilla {
namespace layers {

WebRenderBridgeChild::WebRenderBridgeChild(const uint64_t& aPipelineId)
  : mIsInTransaction(false)
{
}

void
WebRenderBridgeChild::AddWebRenderCommand(const WebRenderCommand& aCmd)
{
  MOZ_ASSERT(mIsInTransaction);
  mCommands.AppendElement(aCmd);
}

bool
WebRenderBridgeChild::DPBegin(uint32_t aWidth, uint32_t aHeight)
{
  MOZ_ASSERT(!mIsInTransaction);
  bool success = false;
  this->SendDPBegin(aWidth, aHeight, &success);
  if (!success) {
    return false;
  }

  mIsInTransaction = true;
  return true;
}

void
WebRenderBridgeChild::DPEnd()
{
  MOZ_ASSERT(mIsInTransaction);
  this->SendDPEnd(mCommands);
  mCommands.Clear();
  mIsInTransaction = false;
}

} // namespace layers
} // namespace mozilla
