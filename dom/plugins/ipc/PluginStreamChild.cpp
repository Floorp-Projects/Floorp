/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginStreamChild.h"
#include "mozilla/plugins/PluginInstanceChild.h"

namespace mozilla {
namespace plugins {

PluginStreamChild::PluginStreamChild()
  : mClosed(false)
{
  memset(&mStream, 0, sizeof(mStream));
  mStream.ndata = static_cast<AStream*>(this);
}

mozilla::ipc::IPCResult
PluginStreamChild::Answer__delete__(const NPReason& reason,
                                    const bool& artificial)
{
  AssertPluginThread();
  if (!artificial)
    NPP_DestroyStream(reason);
  return IPC_OK();
}

int32_t
PluginStreamChild::NPN_Write(int32_t length, void* buffer)
{
  AssertPluginThread();

  int32_t written = 0;
  CallNPN_Write(nsCString(static_cast<char*>(buffer), length),
                &written);
  if (written < 0)
    PPluginStreamChild::Call__delete__(this, NPERR_GENERIC_ERROR, true);
  // careful after here! |this| just got deleted 

  return written;
}

void
PluginStreamChild::NPP_DestroyStream(NPError reason)
{
  AssertPluginThread();

  if (mClosed)
    return;

  mClosed = true;
  Instance()->mPluginIface->destroystream(
    &Instance()->mData, &mStream, reason);
}

PluginInstanceChild*
PluginStreamChild::Instance()
{
  return static_cast<PluginInstanceChild*>(Manager());
}

} // namespace plugins
} // namespace mozilla
