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
  , mIPCOpen(false)
  , mDestroyed(false)
{
}

void
WebRenderBridgeChild::Destroy()
{
  if (!IPCOpen()) {
    return;
  }
  // mDestroyed is used to prevent calling Send__delete__() twice.
  // When this function is called from CompositorBridgeChild::Destroy().
  mDestroyed = true;

  SendShutdown();
}

void
WebRenderBridgeChild::ActorDestroy(ActorDestroyReason why)
{
  mDestroyed = true;
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
  MOZ_ASSERT(!mDestroyed);
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
WebRenderBridgeChild::DPEnd(bool aIsSync)
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(mIsInTransaction);
  if (aIsSync) {
    this->SendDPSyncEnd(mCommands);
  } else {
    this->SendDPEnd(mCommands);
  }

  mCommands.Clear();
  mIsInTransaction = false;
}

uint64_t
WebRenderBridgeChild::AllocExternalImageId(uint64_t aAsyncContainerID)
{
  MOZ_ASSERT(!mDestroyed);

  static uint32_t sNextID = 1;
  ++sNextID;
  MOZ_RELEASE_ASSERT(sNextID != UINT32_MAX);

  // XXX replace external image id allocation with webrender's id allocation.
  // Use proc id as IdNamespace for now.
  uint32_t procId = static_cast<uint32_t>(base::GetCurrentProcId());
  uint64_t imageId = procId;
  imageId = imageId << 32 | sNextID;

  SendAddExternalImageId(imageId, aAsyncContainerID);
  return imageId;
}

void
WebRenderBridgeChild::DeallocExternalImageId(uint64_t aImageId)
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(aImageId);
  SendRemoveExternalImageId(aImageId);
}

} // namespace layers
} // namespace mozilla
