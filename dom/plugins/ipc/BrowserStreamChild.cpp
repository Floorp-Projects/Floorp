/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/plugins/BrowserStreamChild.h"

#include "mozilla/Attributes.h"
#include "mozilla/plugins/PluginInstanceChild.h"
#include "mozilla/plugins/StreamNotifyChild.h"

namespace mozilla {
namespace plugins {

BrowserStreamChild::BrowserStreamChild(PluginInstanceChild* instance,
                                       const nsCString& url,
                                       const uint32_t& length,
                                       const uint32_t& lastmodified,
                                       StreamNotifyChild* notifyData,
                                       const nsCString& headers)
  : mInstance(instance)
  , mStreamStatus(kStreamOpen)
  , mDestroyPending(NOT_DESTROYED)
  , mNotifyPending(false)
  , mStreamAsFilePending(false)
  , mInstanceDying(false)
  , mState(CONSTRUCTING)
  , mURL(url)
  , mHeaders(headers)
  , mStreamNotify(notifyData)
  , mDeliveryTracker(this)
{
  PLUGIN_LOG_DEBUG(("%s (%s, %i, %i, %p, %s)", FULLFUNCTION,
                    url.get(), length, lastmodified, (void*) notifyData,
                    headers.get()));

  AssertPluginThread();

  memset(&mStream, 0, sizeof(mStream));
  mStream.ndata = static_cast<AStream*>(this);
  mStream.url = NullableStringGet(mURL);
  mStream.end = length;
  mStream.lastmodified = lastmodified;
  mStream.headers = NullableStringGet(mHeaders);
  if (notifyData) {
    mStream.notifyData = notifyData->mClosure;
    notifyData->SetAssociatedStream(this);
  }
}

NPError
BrowserStreamChild::StreamConstructed(
            const nsCString& mimeType,
            const bool& seekable,
            uint16_t* stype)
{
  NPError rv = NPERR_NO_ERROR;

  *stype = NP_NORMAL;
  rv = mInstance->mPluginIface->newstream(
    &mInstance->mData, const_cast<char*>(NullableStringGet(mimeType)),
    &mStream, seekable, stype);
  if (rv != NPERR_NO_ERROR) {
    mState = DELETING;
    if (mStreamNotify) {
      mStreamNotify->SetAssociatedStream(nullptr);
      mStreamNotify = nullptr;
    }
  }
  else {
    mState = ALIVE;
  }

  return rv;
}

BrowserStreamChild::~BrowserStreamChild()
{
  NS_ASSERTION(!mStreamNotify, "Should have nulled it by now!");
}

mozilla::ipc::IPCResult
BrowserStreamChild::RecvWrite(const int32_t& offset,
                              const uint32_t& newlength,
                              const Buffer& data)
{
  PLUGIN_LOG_DEBUG_FUNCTION;

  AssertPluginThread();

  if (ALIVE != mState)
    MOZ_CRASH("Unexpected state: received data after NPP_DestroyStream?");

  if (kStreamOpen != mStreamStatus)
    return IPC_OK();

  mStream.end = newlength;

  NS_ASSERTION(data.Length() > 0, "Empty data");

  PendingData* newdata = mPendingData.AppendElement();
  newdata->offset = offset;
  newdata->data = data;
  newdata->curpos = 0;

  EnsureDeliveryPending();

  return IPC_OK();
}

mozilla::ipc::IPCResult
BrowserStreamChild::RecvNPP_StreamAsFile(const nsCString& fname)
{
  PLUGIN_LOG_DEBUG(("%s (fname=%s)", FULLFUNCTION, fname.get()));

  AssertPluginThread();

  if (ALIVE != mState)
    MOZ_CRASH("Unexpected state: received file after NPP_DestroyStream?");

  if (kStreamOpen != mStreamStatus)
    return IPC_OK();

  mStreamAsFilePending = true;
  mStreamAsFileName = fname;
  EnsureDeliveryPending();

  return IPC_OK();
}

mozilla::ipc::IPCResult
BrowserStreamChild::RecvNPP_DestroyStream(const NPReason& reason)
{
  PLUGIN_LOG_DEBUG_METHOD;

  if (ALIVE != mState)
    MOZ_CRASH("Unexpected state: recevied NPP_DestroyStream twice?");

  mState = DYING;
  mDestroyPending = DESTROY_PENDING;
  if (NPRES_DONE != reason)
    mStreamStatus = reason;

  EnsureDeliveryPending();
  return IPC_OK();
}

mozilla::ipc::IPCResult
BrowserStreamChild::Recv__delete__()
{
  AssertPluginThread();

  if (DELETING != mState)
    MOZ_CRASH("Bad state, not DELETING");

  return IPC_OK();
}

NPError
BrowserStreamChild::NPN_RequestRead(NPByteRange* aRangeList)
{
  PLUGIN_LOG_DEBUG_FUNCTION;

  AssertPluginThread();

  if (ALIVE != mState || kStreamOpen != mStreamStatus)
    return NPERR_GENERIC_ERROR;

  IPCByteRanges ranges;
  for (; aRangeList; aRangeList = aRangeList->next) {
    IPCByteRange br = {aRangeList->offset, aRangeList->length};
    ranges.AppendElement(br);
  }

  NPError result;
  CallNPN_RequestRead(ranges, &result);
  return result;
}

void
BrowserStreamChild::NPN_DestroyStream(NPReason reason)
{
  mStreamStatus = reason;
  if (ALIVE == mState)
    SendNPN_DestroyStream(reason);

  EnsureDeliveryPending();
}

void
BrowserStreamChild::EnsureDeliveryPending()
{
  MessageLoop::current()->PostTask(
    mDeliveryTracker.NewRunnableMethod(&BrowserStreamChild::Deliver));
}

void
BrowserStreamChild::Deliver()
{
  while (kStreamOpen == mStreamStatus && mPendingData.Length()) {
    if (DeliverPendingData() && kStreamOpen == mStreamStatus) {
      SetSuspendedTimer();
      return;
    }
  }
  ClearSuspendedTimer();

  NS_ASSERTION(kStreamOpen != mStreamStatus || 0 == mPendingData.Length(),
               "Exit out of the data-delivery loop with pending data");
  mPendingData.Clear();

  // NPP_StreamAsFile() is documented (at MDN) to be called "when the stream
  // is complete" -- i.e. after all calls to NPP_WriteReady() and NPP_Write()
  // have finished.  We make these calls asynchronously (from
  // DeliverPendingData()).  So we need to make sure all the "pending data"
  // has been "delivered" before calling NPP_StreamAsFile() (also
  // asynchronously).  Doing this resolves bug 687610, bug 670036 and possibly
  // also other bugs.
  if (mStreamAsFilePending) {
    if (mStreamStatus == kStreamOpen)
      mInstance->mPluginIface->asfile(&mInstance->mData, &mStream,
                                      mStreamAsFileName.get());
    mStreamAsFilePending = false;
  }

  if (DESTROY_PENDING == mDestroyPending) {
    mDestroyPending = DESTROYED;
    if (mState != DYING)
      MOZ_CRASH("mDestroyPending but state not DYING");

    NS_ASSERTION(NPRES_DONE != mStreamStatus, "Success status set too early!");
    if (kStreamOpen == mStreamStatus)
      mStreamStatus = NPRES_DONE;

    (void) mInstance->mPluginIface
      ->destroystream(&mInstance->mData, &mStream, mStreamStatus);
  }
  if (DESTROYED == mDestroyPending && mNotifyPending) {
    NS_ASSERTION(mStreamNotify, "mDestroyPending but no mStreamNotify?");

    mNotifyPending = false;
    mStreamNotify->NPP_URLNotify(mStreamStatus);
    delete mStreamNotify;
    mStreamNotify = nullptr;
  }
  if (DYING == mState && DESTROYED == mDestroyPending
      && !mStreamNotify && !mInstanceDying) {
    SendStreamDestroyed();
    mState = DELETING;
  }
}

bool
BrowserStreamChild::DeliverPendingData()
{
  if (mState != ALIVE && mState != DYING)
    MOZ_CRASH("Unexpected state");

  NS_ASSERTION(mPendingData.Length(), "Called from Deliver with empty pending");

  while (mPendingData[0].curpos < static_cast<int32_t>(mPendingData[0].data.Length())) {
    int32_t r = mInstance->mPluginIface->writeready(&mInstance->mData, &mStream);
    if (kStreamOpen != mStreamStatus)
      return false;
    if (0 == r) // plugin wants to suspend delivery
      return true;

    r = mInstance->mPluginIface->write(
      &mInstance->mData, &mStream,
      mPendingData[0].offset + mPendingData[0].curpos, // offset
      mPendingData[0].data.Length() - mPendingData[0].curpos, // length
      const_cast<char*>(mPendingData[0].data.BeginReading() + mPendingData[0].curpos));
    if (kStreamOpen != mStreamStatus)
      return false;
    if (0 == r)
      return true;
    if (r < 0) { // error condition
      NPN_DestroyStream(NPRES_NETWORK_ERR);
      return false;
    }
    mPendingData[0].curpos += r;
  }
  mPendingData.RemoveElementAt(0);
  return false;
}

void
BrowserStreamChild::SetSuspendedTimer()
{
  if (mSuspendedTimer.IsRunning())
    return;
  mSuspendedTimer.Start(
    base::TimeDelta::FromMilliseconds(100), // 100ms copied from Mozilla plugin host
    this, &BrowserStreamChild::Deliver);
}

void
BrowserStreamChild::ClearSuspendedTimer()
{
  mSuspendedTimer.Stop();
}

} /* namespace plugins */
} /* namespace mozilla */
