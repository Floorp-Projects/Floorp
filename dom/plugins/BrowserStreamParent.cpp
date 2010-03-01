/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */

#include "BrowserStreamParent.h"
#include "PluginInstanceParent.h"
#include "PluginModuleParent.h"

template<>
struct RunnableMethodTraits<mozilla::plugins::BrowserStreamParent>
{
  typedef mozilla::plugins::BrowserStreamParent Cls;
    static void RetainCallee(Cls* obj) { }
    static void ReleaseCallee(Cls* obj) { }
};

namespace mozilla {
namespace plugins {

BrowserStreamParent::BrowserStreamParent(PluginInstanceParent* npp,
                                         NPStream* stream)
  : mNPP(npp)
  , mStream(stream)
  , mDeleteTask(nsnull)
{
  mStream->pdata = static_cast<AStream*>(this);
}

BrowserStreamParent::~BrowserStreamParent()
{
  if (mDeleteTask) {
    mDeleteTask->Cancel();
    // MessageLoop::current() owns this
    mDeleteTask = nsnull;
  }
}

NPError
BrowserStreamParent::NPP_DestroyStream(NPReason reason)
{
  PLUGIN_LOG_DEBUG(("%s (reason=%i)", FULLFUNCTION, reason));

  if (!mDeleteTask) {
    mDeleteTask = NewRunnableMethod(this, &BrowserStreamParent::Delete);
    MessageLoop::current()->PostTask(FROM_HERE, mDeleteTask);
  }

  NPError err = NPERR_NO_ERROR;
  CallNPP_DestroyStream(reason, &err);
  return err;
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

bool
BrowserStreamParent::AnswerNPN_DestroyStream(const NPReason& reason,
                                             NPError* result)
{
  PLUGIN_LOG_DEBUG_FUNCTION;

  *result = mNPP->mNPNIface->destroystream(mNPP->mNPP, mStream, reason);
  return true;
}

void
BrowserStreamParent::Delete()
{
  PBrowserStreamParent::Send__delete__(this);
  // |this| just got deleted
}

} // namespace plugins
} // namespace mozilla
