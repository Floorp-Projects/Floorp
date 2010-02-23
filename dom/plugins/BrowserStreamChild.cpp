/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Benjamin Smedberg <benjamin@smedbergs.us>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "BrowserStreamChild.h"
#include "PluginInstanceChild.h"
#include "StreamNotifyChild.h"

namespace mozilla {
namespace plugins {

BrowserStreamChild::BrowserStreamChild(PluginInstanceChild* instance,
                                       const nsCString& url,
                                       const uint32_t& length,
                                       const uint32_t& lastmodified,
                                       const PStreamNotifyChild* notifyData,
                                       const nsCString& headers,
                                       const nsCString& mimeType,
                                       const bool& seekable,
                                       NPError* rv,
                                       uint16_t* stype)
  : mInstance(instance)
  , mClosed(false)
  , mState(CONSTRUCTING)
  , mURL(url)
  , mHeaders(headers)
{
  PLUGIN_LOG_DEBUG(("%s (%s, %i, %i, %p, %s, %s)", FULLFUNCTION,
                    url.get(), length, lastmodified, (void*) notifyData,
                    headers.get(), mimeType.get()));

  AssertPluginThread();

  memset(&mStream, 0, sizeof(mStream));
  mStream.ndata = static_cast<AStream*>(this);
  mStream.url = NullableStringGet(mURL);
  mStream.end = length;
  mStream.lastmodified = lastmodified;
  if (notifyData)
    mStream.notifyData =
      static_cast<const StreamNotifyChild*>(notifyData)->mClosure;
  mStream.headers = NullableStringGet(mHeaders);
}

NPError
BrowserStreamChild::StreamConstructed(
            const nsCString& url,
            const uint32_t& length,
            const uint32_t& lastmodified,
            PStreamNotifyChild* notifyData,
            const nsCString& headers,
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
    mClosed = true;
    mState = DELETING;
  }
  else {
    mState = ALIVE;
  }

  return rv;
}

bool
BrowserStreamChild::RecvWrite(const int32_t& offset,
                              const Buffer& data,
                              const uint32_t& newlength)
{
  PLUGIN_LOG_DEBUG_FUNCTION;

  AssertPluginThread();

  if (ALIVE != mState)
    NS_RUNTIMEABORT("Unexpected state: received data after NPP_DestroyStream?");

  if (mClosed)
    return true;

  mStream.end = newlength;

  NS_ASSERTION(data.Length() > 0, "Empty data");

  PendingData* newdata = mPendingData.AppendElement();
  newdata->offset = offset;
  newdata->data = data;
  newdata->curpos = 0;

  DeliverData();

  return true;
}

bool
BrowserStreamChild::AnswerNPP_StreamAsFile(const nsCString& fname)
{
  PLUGIN_LOG_DEBUG(("%s (fname=%s)", FULLFUNCTION, fname.get()));

  AssertPluginThread();

  if (ALIVE != mState)
    NS_RUNTIMEABORT("Unexpected state: received file after NPP_DestroyStream?");

  if (mClosed)
    return true;

  mInstance->mPluginIface->asfile(&mInstance->mData, &mStream,
                                  fname.get());
  return true;
}

bool
BrowserStreamChild::RecvNPP_DestroyStream(const NPReason& reason)
{
  PLUGIN_LOG_DEBUG_METHOD;

  if (ALIVE != mState)
    NS_RUNTIMEABORT("Unexpected state: recevied NPP_DestroyStream twice?");

  mState = DYING;

  mClosed = true;
  mInstance->mPluginIface->destroystream(&mInstance->mData, &mStream, reason);

  SendStreamDestroyed();
  mState = DELETING;

  return true;
}

bool
BrowserStreamChild::Recv__delete__()
{
  AssertPluginThread();

  if (DELETING != mState)
    NS_RUNTIMEABORT("Bad state, not DELETING");

  return true;
}

NPError
BrowserStreamChild::NPN_RequestRead(NPByteRange* aRangeList)
{
  PLUGIN_LOG_DEBUG_FUNCTION;

  AssertPluginThread();

  if (ALIVE != mState || mClosed)
    return NPERR_GENERIC_ERROR;

  IPCByteRanges ranges;
  for (; aRangeList; aRangeList = aRangeList->next) {
    IPCByteRange br = {aRangeList->offset, aRangeList->length};
    ranges.push_back(br);
  }

  NPError result;
  CallNPN_RequestRead(ranges, &result);
  return result;
}

void
BrowserStreamChild::DeliverData()
{
  if (ALIVE != mState || mClosed) {
    ClearSuspendedTimer();
    return;
  }

  while (mPendingData.Length()) {
    PendingData& cur = mPendingData[0];
    while (cur.curpos < cur.data.Length()) {
      int32_t r = mInstance->mPluginIface->writeready(&mInstance->mData, &mStream);
      if (r == 0) {
        SetSuspendedTimer();
        return;
      }
      r = mInstance->mPluginIface->write(
        &mInstance->mData, &mStream,
        cur.offset + cur.curpos, // offset
        cur.data.Length() - cur.curpos, // length
        const_cast<char*>(cur.data.BeginReading() + cur.curpos));
      if (r == 0) {
        SetSuspendedTimer();
        return;
      }
      if (r < 0) { // error condition
        if (ALIVE == mState && !mClosed) { // re-check
          mClosed = true;
          SendNPN_DestroyStream(NPRES_NETWORK_ERR);
        }
        ClearSuspendedTimer();
        return;
      }
      cur.curpos += r;
    }
    mPendingData.RemoveElementAt(0);
  }

  ClearSuspendedTimer();
}

void
BrowserStreamChild::SetSuspendedTimer()
{
  if (mSuspendedTimer.IsRunning())
    return;
  mSuspendedTimer.Start(
    base::TimeDelta::FromMilliseconds(100),
    this, &BrowserStreamChild::DeliverData);
}

void
BrowserStreamChild::ClearSuspendedTimer()
{
  mSuspendedTimer.Stop();
}

} /* namespace plugins */
} /* namespace mozilla */
