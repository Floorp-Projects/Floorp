/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */

#include "BrowserStreamParent.h"
#include "PluginInstanceParent.h"

// How much data are we willing to send across the wire
// in one chunk?
static const int32_t kSendDataChunk = 0x1000;

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
  SendNPP_DestroyStream(reason);
}

bool
BrowserStreamParent::RecvStreamDestroyed()
{
  if (DYING != mState) {
    NS_ERROR("Unexpected state");
    return false;
  }

  mState = DELETING;
  Send__delete__(this);
  return true;
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

  SendWrite(offset,
    nsCString(static_cast<char*>(buffer), len),
    mStream->end);

  return len;
}

void
BrowserStreamParent::StreamAsFile(const char* fname)
{
  PLUGIN_LOG_DEBUG_FUNCTION;

  NS_ASSERTION(ALIVE == mState,
               "Calling streamasfile after NPP_DestroyStream?");

  CallNPP_StreamAsFile(nsCString(fname));
  return;
}

} // namespace plugins
} // namespace mozilla
