/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * ***** BEGIN LICENSE BLOCK *****
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

#include "PluginStreamChild.h"
#include "PluginInstanceChild.h"

namespace mozilla {
namespace plugins {

PluginStreamChild::PluginStreamChild(PluginInstanceChild* instance,
                                     const nsCString& url,
                                     const uint32_t& length,
                                     const uint32_t& lastmodified,
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
  memset(&mStream, 0, sizeof(mStream));
  mStream.ndata = this;
  if (!mURL.IsEmpty())
    mStream.url = mURL.get();
  mStream.end = length;
  mStream.lastmodified = lastmodified;
  if (!mHeaders.IsEmpty())
    mStream.headers = mHeaders.get();

  *rv = mInstance->mPluginIface->newstream(&mInstance->mData,
                                           const_cast<char*>(mimeType.get()),
                                           &mStream, seekable, stype);
  if (*rv != NPERR_NO_ERROR)
    mClosed = true;
}

nsresult
PluginStreamChild::AnswerNPP_WriteReady(const int32_t& newlength,
                                        int32_t *size)
{
  if (mClosed) {
    *size = 0;
    return NS_OK;
  }

  mStream.end = newlength;

  *size = mInstance->mPluginIface->writeready(&mInstance->mData, &mStream);
  return NS_OK;
}

nsresult
PluginStreamChild::AnswerNPP_Write(const int32_t& offset,
                                   const Buffer& data,
                                   int32_t* consumed)
{
  if (mClosed) {
    *consumed = -1;
    return NS_OK;
  }

  *consumed = mInstance->mPluginIface->write(&mInstance->mData, &mStream,
                                             offset, data.Length(),
                                             const_cast<char*>(data.get()));
  if (*consumed < 0)
    mClosed = true;

  return NS_OK;
}

nsresult
PluginStreamChild::AnswerNPP_StreamAsFile(const nsCString& fname)
{
  if (mClosed)
    return NS_OK;

  mInstance->mPluginIface->asfile(&mInstance->mData, &mStream,
                                  fname.get());
  return NS_OK;
}

} /* namespace plugins */
} /* namespace mozilla */
