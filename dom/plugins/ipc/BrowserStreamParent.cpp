/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "BrowserStreamParent.h"
#include "PluginAsyncSurrogate.h"
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
  , mDeferredDestroyReason(NPRES_DONE)
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

mozilla::ipc::IPCResult
BrowserStreamParent::RecvAsyncNPP_NewStreamResult(const NPError& rv,
                                                  const uint16_t& stype)
{
  PLUGIN_LOG_DEBUG_FUNCTION;
  PluginAsyncSurrogate* surrogate = mNPP->GetAsyncSurrogate();
  MOZ_ASSERT(surrogate);
  surrogate->AsyncCallArriving();
  if (mState == DEFERRING_DESTROY) {
    // We've been asked to destroy ourselves before init was complete.
    mState = DYING;
    Unused << SendNPP_DestroyStream(mDeferredDestroyReason);
    return IPC_OK();
  }

  NPError error = rv;
  if (error == NPERR_NO_ERROR) {
    if (!mStreamListener) {
      return IPC_FAIL_NO_REASON(this);
    }
    if (mStreamListener->SetStreamType(stype)) {
      mState = ALIVE;
    } else {
      error = NPERR_GENERIC_ERROR;
    }
  }

  if (error != NPERR_NO_ERROR) {
    surrogate->DestroyAsyncStream(mStream);
    Unused << PBrowserStreamParent::Send__delete__(this);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
BrowserStreamParent::AnswerNPN_RequestRead(const IPCByteRanges& ranges,
                                           NPError* result)
{
  PLUGIN_LOG_DEBUG_FUNCTION;

  switch (mState) {
  case INITIALIZING:
    NS_ERROR("Requesting a read before initialization has completed");
    *result = NPERR_GENERIC_ERROR;
    return IPC_FAIL_NO_REASON(this);

  case ALIVE:
    break;

  case DYING:
    *result = NPERR_GENERIC_ERROR;
    return IPC_OK();

  default:
    NS_ERROR("Unexpected state");
    return IPC_FAIL_NO_REASON(this);
  }

  if (!mStream)
    return IPC_FAIL_NO_REASON(this);

  if (ranges.Length() > INT32_MAX)
    return IPC_FAIL_NO_REASON(this);

  UniquePtr<NPByteRange[]> rp(new NPByteRange[ranges.Length()]);
  for (uint32_t i = 0; i < ranges.Length(); ++i) {
    rp[i].offset = ranges[i].offset;
    rp[i].length = ranges[i].length;
    rp[i].next = &rp[i + 1];
  }
  rp[ranges.Length() - 1].next = nullptr;

  *result = mNPP->mNPNIface->requestread(mStream, rp.get());
  return IPC_OK();
}

mozilla::ipc::IPCResult
BrowserStreamParent::RecvNPN_DestroyStream(const NPReason& reason)
{
  switch (mState) {
  case ALIVE:
    break;

  case DYING:
    return IPC_OK();

  default:
    NS_ERROR("Unexpected state");
    return IPC_FAIL_NO_REASON(this);
  }

  mNPP->mNPNIface->destroystream(mNPP->mNPP, mStream, reason);
  return IPC_OK();
}

void
BrowserStreamParent::NPP_DestroyStream(NPReason reason)
{
  NS_ASSERTION(ALIVE == mState || INITIALIZING == mState,
               "NPP_DestroyStream called twice?");
  bool stillInitializing = INITIALIZING == mState;
  if (stillInitializing) {
    mState = DEFERRING_DESTROY;
    mDeferredDestroyReason = reason;
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
  if (!Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(this);
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

void
BrowserStreamParent::StreamAsFile(const char* fname)
{
  PLUGIN_LOG_DEBUG_FUNCTION;

  NS_ASSERTION(ALIVE == mState,
               "Calling streamasfile after NPP_DestroyStream?");

  // Make sure our stream survives until the plugin process tells us we've
  // been destroyed (until RecvStreamDestroyed() is called).  Since we retain
  // mStreamPeer at most once, we won't get in trouble if StreamAsFile() is
  // called more than once.
  if (!mStreamPeer) {
    nsNPAPIPlugin::RetainStream(mStream, getter_AddRefs(mStreamPeer));
  }

  Unused << SendNPP_StreamAsFile(nsCString(fname));
  return;
}

} // namespace plugins
} // namespace mozilla
