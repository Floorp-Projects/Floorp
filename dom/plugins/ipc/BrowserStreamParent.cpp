/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "BrowserStreamParent.h"
#include "PluginInstanceParent.h"
#include "nsNPAPIPlugin.h"

#include "mozilla/unused.h"

// How much data are we willing to send across the wire
// in one chunk?
static const int32_t kSendDataChunk = 0x4000;

namespace mozilla {
namespace plugins {

BrowserStreamParent::BrowserStreamParent(PluginInstanceParent* npp,
                                         NPStream* stream)
  : mNPP(npp)
  , mStream(stream)
  , mState(ALIVE)
{
  mStream->pdata = static_cast<AStream*>(this);
}

BrowserStreamParent::~BrowserStreamParent()
{
}

bool
BrowserStreamParent::AnswerNPN_RequestRead(const IPCByteRanges& ranges,
                                           NPError* result)
{
  PLUGIN_LOG_DEBUG_FUNCTION;

  switch (mState) {
  case ALIVE:
    break;

  case DYING:
    *result = NPERR_GENERIC_ERROR;
    return true;

  default:
    NS_ERROR("Unexpected state");
    return false;
  }

  if (!mStream)
    return false;

  if (ranges.size() > PR_INT32_MAX)
    return false;

  nsAutoArrayPtr<NPByteRange> rp(new NPByteRange[ranges.size()]);
  for (PRUint32 i = 0; i < ranges.size(); ++i) {
    rp[i].offset = ranges[i].offset;
    rp[i].length = ranges[i].length;
    rp[i].next = &rp[i + 1];
  }
  rp[ranges.size() - 1].next = NULL;

  *result = mNPP->mNPNIface->requestread(mStream, rp);
  return true;
}

bool
BrowserStreamParent::RecvNPN_DestroyStream(const NPReason& reason)
{
  switch (mState) {
  case ALIVE:
    break;

  case DYING:
    return true;

  default:
    NS_ERROR("Unexpected state");
    return false;
  };

  mNPP->mNPNIface->destroystream(mNPP->mNPP, mStream, reason);
  return true;
}

void
BrowserStreamParent::NPP_DestroyStream(NPReason reason)
{
  NS_ASSERTION(ALIVE == mState, "NPP_DestroyStream called twice?");
  mState = DYING;
  unused << SendNPP_DestroyStream(reason);
}

bool
BrowserStreamParent::RecvStreamDestroyed()
{
  if (DYING != mState) {
    NS_ERROR("Unexpected state");
    return false;
  }

  mStreamPeer = NULL;

  mState = DELETING;
  return Send__delete__(this);
}

int32_t
BrowserStreamParent::WriteReady()
{
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
                   nsCString(static_cast<char*>(buffer), len),
                   mStream->end) ?
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

  unused << SendNPP_StreamAsFile(nsCString(fname));
  return;
}

} // namespace plugins
} // namespace mozilla
