/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */

#include "BrowserStreamParent.h"
#include "PluginInstanceParent.h"

namespace mozilla {
namespace plugins {

BrowserStreamParent::BrowserStreamParent(PluginInstanceParent* npp,
                                         NPStream* stream)
  : mNPP(npp)
  , mStream(stream)
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
BrowserStreamParent::Answer__delete__(const NPError& reason,
                                      const bool& artificial)
{
  if (!artificial)
    NPN_DestroyStream(reason);
  return true;
}

int32_t
BrowserStreamParent::WriteReady()
{
  PLUGIN_LOG_DEBUG_FUNCTION;

  int32_t result;
  if (!CallNPP_WriteReady(mStream->end, &result))
    return -1;

  return result;
}

int32_t
BrowserStreamParent::Write(int32_t offset,
                           int32_t len,
                           void* buffer)
{
  PLUGIN_LOG_DEBUG_FUNCTION;

  int32_t result;
  if (!CallNPP_Write(offset,
                     nsCString(static_cast<char*>(buffer), len),
                     &result))
    return -1;

  return result;
}

void
BrowserStreamParent::StreamAsFile(const char* fname)
{
  PLUGIN_LOG_DEBUG_FUNCTION;

  CallNPP_StreamAsFile(nsCString(fname));
}

NPError
BrowserStreamParent::NPN_DestroyStream(NPReason reason)
{
  PLUGIN_LOG_DEBUG_FUNCTION;

  return mNPP->mNPNIface->destroystream(mNPP->mNPP, mStream, reason);
}

} // namespace plugins
} // namespace mozilla
