/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "BrowserStreamParent.h"
#include "PluginInstanceParent.h"
#include "nsNPAPIPlugin.h"

#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"

// How much data are we willing to send across the wire
// in one chunk?
static const int32_t kSendDataChunk = 0xffff;

namespace mozilla {
namespace plugins {

BrowserStreamParent::BrowserStreamParent(PluginInstanceParent* npp,
                                         NPStream* stream)
  : mNPP(npp)
  , mStream(stream)
  , mState(INITIALIZING)
{
  mStream->pdata = static_cast<AStream*>(this);
  nsNPAPIStreamWrapper* wrapper =
    reinterpret_cast<nsNPAPIStreamWrapper*>(mStream->ndata);
  if (wrapper) {
    mStreamListener = wrapper->GetStreamListener();
  }
}

BrowserStreamParent::~BrowserStreamParent()
{
  mStream->pdata = nullptr;
}

void
BrowserStreamParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Implement me! Bug 1005159
}

void
BrowserStreamParent::NPP_DestroyStream(NPReason reason)
{
  NS_ASSERTION(ALIVE == mState || INITIALIZING == mState,
               "NPP_DestroyStream called twice?");
  bool stillInitializing = INITIALIZING == mState;
  if (stillInitializing) {
    mState = DEFERRING_DESTROY;
  } else {
    mState = DYING;
    Unused << SendNPP_DestroyStream(reason);
  }
}

mozilla::ipc::IPCResult
BrowserStreamParent::RecvStreamDestroyed()
{
  if (DYING != mState) {
    NS_ERROR("Unexpected state");
    return IPC_FAIL_NO_REASON(this);
  }

  mStreamPeer = nullptr;

  mState = DELETING;
  IProtocol* mgr = Manager();
  if (!Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

int32_t
BrowserStreamParent::WriteReady()
{
  if (mState == INITIALIZING) {
    return 0;
  }
  return kSendDataChunk;
}

int32_t
BrowserStreamParent::Write(int32_t offset,
                           int32_t len,
                           void* buffer)
{
  PLUGIN_LOG_DEBUG_FUNCTION;

  NS_ASSERTION(ALIVE == mState, "Sending data after NPP_DestroyStream?");
  NS_ASSERTION(len > 0, "Non-positive length to NPP_Write");

  if (len > kSendDataChunk)
    len = kSendDataChunk;

  return SendWrite(offset,
                   mStream->end,
                   nsCString(static_cast<char*>(buffer), len)) ?
    len : -1;
}

} // namespace plugins
} // namespace mozilla
