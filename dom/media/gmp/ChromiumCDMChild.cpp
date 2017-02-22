/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMChild.h"
#include "GMPContentChild.h"

namespace mozilla {
namespace gmp {

ChromiumCDMChild::ChromiumCDMChild(GMPContentChild* aPlugin)
  : mPlugin(aPlugin)
{
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvInit(const bool& aAllowDistinctiveIdentifier,
                           const bool& aAllowPersistentState)
{
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvSetServerCertificate(const uint32_t& aPromiseId,
                                           nsTArray<uint8_t>&& aServerCert)

{
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvCreateSessionAndGenerateRequest(
  const uint32_t& aPromiseId,
  const uint32_t& aSessionType,
  const uint32_t& aInitDataType,
  nsTArray<uint8_t>&& aInitData)
{
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvUpdateSession(const uint32_t& aPromiseId,
                                    const nsCString& aSessionId,
                                    nsTArray<uint8_t>&& aResponse)
{
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvCloseSession(const uint32_t& aPromiseId,
                                   const nsCString& aSessionId)
{
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvRemoveSession(const uint32_t& aPromiseId,
                                    const nsCString& aSessionId)
{
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvDecrypt(const CDMInputBuffer& aBuffer)
{
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvInitializeVideoDecoder(
  const CDMVideoDecoderConfig& aConfig)
{
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvDeinitializeVideoDecoder()
{
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvResetVideoDecoder()
{
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvDecryptAndDecodeFrame(const CDMInputBuffer& aBuffer)
{
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvDestroy()
{
  return IPC_OK();
}

} // namespace gmp
} // namespace mozilla
