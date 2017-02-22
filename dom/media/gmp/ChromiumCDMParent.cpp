/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMParent.h"
#include "mozilla/gmp/GMPTypes.h"
#include "GMPContentChild.h"

namespace mozilla {
namespace gmp {

ChromiumCDMParent::ChromiumCDMParent(GMPContentParent* aContentParent)
  : mContentParent(aContentParent)
{
}

ipc::IPCResult
ChromiumCDMParent::Recv__delete__()
{
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvOnResolveNewSessionPromise(const uint32_t& aPromiseId,
                                                  const nsCString& aSessionId)
{
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvOnResolvePromise(const uint32_t& aPromiseId)
{
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvOnRejectPromise(const uint32_t& aPromiseId,
                                       const uint32_t& aError,
                                       const uint32_t& aSystemCode,
                                       const nsCString& aErrorMessage)
{
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvOnSessionMessage(const nsCString& aSessionId,
                                        const uint32_t& aMessageType,
                                        nsTArray<uint8_t>&& aMessage)
{
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvOnSessionKeysChange(
  const nsCString& aSessionId,
  nsTArray<CDMKeyInformation>&& aKeysInfo)
{
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvOnExpirationChange(const nsCString& aSessionId,
                                          const double& aSecondsSinceEpoch)
{
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvOnSessionClosed(const nsCString& aSessionId)
{
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvOnLegacySessionError(const nsCString& aSessionId,
                                            const uint32_t& aError,
                                            const uint32_t& aSystemCode,
                                            const nsCString& aMessage)
{
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvDecrypted(const uint32_t& aStatus,
                                 nsTArray<uint8_t>&& aData)
{
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvDecoded(const CDMVideoFrame& aFrame)
{
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvDecodeFailed(const uint32_t& aStatus)
{
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvShutdown()
{
  // TODO: SendDestroy(), call Terminated.
  return IPC_OK();
}

void
ChromiumCDMParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

} // namespace gmp
} // namespace mozilla
