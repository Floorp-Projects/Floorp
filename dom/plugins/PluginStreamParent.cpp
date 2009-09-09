/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */

#include "PluginStreamParent.h"
#include "PluginInstanceParent.h"

namespace mozilla {
namespace plugins {

PluginStreamParent::PluginStreamParent(PluginInstanceParent* npp,
                                       NPStream* stream)
  : mNPP(npp)
  , mStream(stream)
{
  mStream->pdata = this;
}

nsresult
PluginStreamParent::AnswerNPN_RequestRead(const IPCByteRanges& ranges,
                                          NPError* result)
{
  if (!mStream)
    return NS_ERROR_NOT_INITIALIZED;

  if (ranges.size() > PR_INT32_MAX) {
    // TODO: abort all processing!
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoArrayPtr<NPByteRange> rp(new NPByteRange[ranges.size()]);
  for (PRUint32 i = 0; i < ranges.size(); ++i) {
    rp[i].offset = ranges[i].offset;
    rp[i].length = ranges[i].length;
    rp[i].next = &rp[i + 1];
  }
  rp[ranges.size()].next = NULL;

  return mNPP->mNPNIface->requestread(mStream, rp);
}

int32_t
PluginStreamParent::WriteReady()
{
  int32_t result;
  nsresult rv = CallNPP_WriteReady(mStream->end, &result);
  if (NS_FAILED(rv)) {
    mNPP->mNPNIface->destroystream(mNPP->mNPP, mStream, NPRES_NETWORK_ERR);
  }
  return result;
}

int32_t
PluginStreamParent::Write(int32_t offset,
                          int32_t len,
                          void* buffer)
{
  int32_t result;
  nsresult rv = CallNPP_Write(offset,
                           nsDependentCString(static_cast<char*>(buffer), len),
                           &result);
  if (NS_FAILED(rv))
    return -1;

  if (result == -1)
    mNPP->CallPPluginStreamDestructor(this, NPRES_USER_BREAK, true);
  return result;
}

void
PluginStreamParent::StreamAsFile(const char* fname)
{
  CallNPP_StreamAsFile(nsCString(fname));
}

NPError
PluginStreamParent::NPN_DestroyStream(NPReason reason)
{
  return mNPP->mNPNIface->destroystream(mNPP->mNPP, mStream, reason);
}


} // namespace plugins
} // namespace mozilla
