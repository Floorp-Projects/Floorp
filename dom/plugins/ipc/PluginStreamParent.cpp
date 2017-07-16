/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginStreamParent.h"
#include "PluginInstanceParent.h"

namespace mozilla {
namespace plugins {

PluginStreamParent::PluginStreamParent(PluginInstanceParent* npp,
                                       const nsCString& mimeType,
                                       const nsCString& target,
                                       NPError* result)
  : mInstance(npp)
  , mClosed(false)
{
  *result = mInstance->mNPNIface->newstream(mInstance->mNPP,
                                            const_cast<char*>(mimeType.get()),
                                            NullableStringGet(target),
                                            &mStream);
  if (*result == NPERR_NO_ERROR)
    mStream->pdata = static_cast<AStream*>(this);
  else
    mStream = nullptr;
}

void
PluginStreamParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Implement me! Bug 1005166
}

mozilla::ipc::IPCResult
PluginStreamParent::AnswerNPN_Write(const Buffer& data, int32_t* written)
{
  if (mClosed) {
    *written = -1;
    return IPC_OK();
  }

  *written = mInstance->mNPNIface->write(mInstance->mNPP, mStream,
                                         data.Length(),
                                         const_cast<char*>(data.get()));
  if (*written < 0)
    mClosed = true;

  return IPC_OK();
}

mozilla::ipc::IPCResult
PluginStreamParent::Answer__delete__(const NPError& reason,
                                     const bool& artificial)
{
  if (!artificial)
    this->NPN_DestroyStream(reason);
  return IPC_OK();
}

void
PluginStreamParent::NPN_DestroyStream(NPReason reason)
{
  if (mClosed)
    return;

  mInstance->mNPNIface->destroystream(mInstance->mNPP, mStream, reason);
  mClosed = true;
}

} // namespace plugins
} // namespace mozilla
