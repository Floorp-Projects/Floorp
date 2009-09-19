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
  mStream->pdata = this;
}

bool
BrowserStreamParent::AnswerNPN_RequestRead(const IPCByteRanges& ranges,
                                           NPError* result)
{
  if (!mStream)
    return false;

  if (ranges.size() > PR_INT32_MAX) {
    // TODO: abort all processing!
    return false;
  }

  nsAutoArrayPtr<NPByteRange> rp(new NPByteRange[ranges.size()]);
  for (PRUint32 i = 0; i < ranges.size(); ++i) {
    rp[i].offset = ranges[i].offset;
    rp[i].length = ranges[i].length;
    rp[i].next = &rp[i + 1];
  }
  rp[ranges.size()].next = NULL;

  return NPERR_NO_ERROR == mNPP->mNPNIface->requestread(mStream, rp);
}

int32_t
BrowserStreamParent::WriteReady()
{
  int32_t result;
  if (!CallNPP_WriteReady(mStream->end, &result)) {
    mNPP->mNPNIface->destroystream(mNPP->mNPP, mStream, NPRES_NETWORK_ERR);
    // XXX is this right?
    return -1;
  }
  return result;
}

int32_t
BrowserStreamParent::Write(int32_t offset,
                           int32_t len,
                           void* buffer)
{
  int32_t result;
  if (!CallNPP_Write(offset,
                     nsDependentCString(static_cast<char*>(buffer), len),
                     &result)) {
    return -1;
  }

  if (result == -1)
    mNPP->CallPBrowserStreamDestructor(this, NPRES_USER_BREAK, true);
  return result;
}

void
BrowserStreamParent::StreamAsFile(const char* fname)
{
  CallNPP_StreamAsFile(nsCString(fname));
}

NPError
BrowserStreamParent::NPN_DestroyStream(NPReason reason)
{
  return mNPP->mNPNIface->destroystream(mNPP->mNPP, mStream, reason);
}

} // namespace plugins
} // namespace mozilla
