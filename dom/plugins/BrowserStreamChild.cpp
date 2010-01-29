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
  if (rv != NPERR_NO_ERROR)
    mClosed = true;

  return rv;
}

bool
BrowserStreamChild::AnswerNPP_WriteReady(const int32_t& newlength,
                                         int32_t *size)
{
  PLUGIN_LOG_DEBUG_FUNCTION;

  AssertPluginThread();

  if (mClosed) {
    *size = 0;
    return true;
  }

  mStream.end = newlength;

  *size = mInstance->mPluginIface->writeready(&mInstance->mData, &mStream);
  return true;
}

bool
BrowserStreamChild::AnswerNPP_Write(const int32_t& offset,
                                    const Buffer& data,
                                    int32_t* consumed)
{
  PLUGIN_LOG_DEBUG(("%s (offset=%i, data.length=%i)", FULLFUNCTION,
                    offset, data.Length()));

  AssertPluginThread();

  if (mClosed) {
    *consumed = -1;
    return true;
  }

  *consumed = mInstance->mPluginIface->write(&mInstance->mData, &mStream,
                                             offset, data.Length(),
                                             const_cast<char*>(data.get()));
  return true;
}

bool
BrowserStreamChild::AnswerNPP_StreamAsFile(const nsCString& fname)
{
  PLUGIN_LOG_DEBUG(("%s (fname=%s)", FULLFUNCTION, fname.get()));

  AssertPluginThread();

  if (mClosed)
    return true;

  mInstance->mPluginIface->asfile(&mInstance->mData, &mStream,
                                  fname.get());
  return true;
}

bool
BrowserStreamChild::Answer__delete__(const NPError& reason,
                                     const bool& artificial)
{
  AssertPluginThread();
  if (!artificial)
    NPP_DestroyStream(reason);
  return true;
}

NPError
BrowserStreamChild::NPN_RequestRead(NPByteRange* aRangeList)
{
  PLUGIN_LOG_DEBUG_FUNCTION;

  AssertPluginThread();

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
BrowserStreamChild::NPP_DestroyStream(NPError reason)
{
  PLUGIN_LOG_DEBUG(("%s (reason=%i)", FULLFUNCTION, reason));

  AssertPluginThread();

  if (mClosed)
    return;

  mInstance->mPluginIface->destroystream(&mInstance->mData, &mStream, reason);
  mClosed = true;
}

} /* namespace plugins */
} /* namespace mozilla */
