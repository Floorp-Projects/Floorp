/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMChild.h"
#include "GMPContentChild.h"
#include "WidevineUtils.h"
#include "GMPLog.h"
#include "GMPPlatform.h"
#include "mozilla/Unused.h"
#include "base/time.h"

namespace mozilla {
namespace gmp {

ChromiumCDMChild::ChromiumCDMChild(GMPContentChild* aPlugin)
  : mPlugin(aPlugin)
{
}

void
ChromiumCDMChild::Init(cdm::ContentDecryptionModule_8* aCDM)
{
  mCDM = aCDM;
  MOZ_ASSERT(mCDM);
}

void
ChromiumCDMChild::TimerExpired(void* aContext)
{
  if (mCDM) {
    mCDM->TimerExpired(aContext);
  }
}

cdm::Buffer*
ChromiumCDMChild::Allocate(uint32_t aCapacity)
{
  return new WidevineBuffer(aCapacity);
}

void
ChromiumCDMChild::SetTimer(int64_t aDelayMs, void* aContext)
{
  RefPtr<ChromiumCDMChild> self(this);
  SetTimerOnMainThread(NewGMPTask([self, aContext]() {
    self->TimerExpired(aContext);
  }), aDelayMs);
}

cdm::Time
ChromiumCDMChild::GetCurrentWallTime()
{
  return base::Time::Now().ToDoubleT();
}

void
ChromiumCDMChild::OnResolveNewSessionPromise(uint32_t aPromiseId,
                                             const char* aSessionId,
                                             uint32_t aSessionIdSize)
{
  Unused << SendOnResolveNewSessionPromise(aPromiseId,
                                           nsCString(aSessionId, aSessionIdSize));
}

void ChromiumCDMChild::OnResolvePromise(uint32_t aPromiseId)
{
  Unused << SendOnResolvePromise(aPromiseId);
}

void
ChromiumCDMChild::OnRejectPromise(uint32_t aPromiseId,
                                  cdm::Error aError,
                                  uint32_t aSystemCode,
                                  const char* aErrorMessage,
                                  uint32_t aErrorMessageSize)
{
  Unused << SendOnRejectPromise(aPromiseId,
                                aError,
                                aSystemCode,
                                nsCString(aErrorMessage, aErrorMessageSize));
}

void
ChromiumCDMChild::OnSessionMessage(const char* aSessionId,
                                   uint32_t aSessionIdSize,
                                   cdm::MessageType aMessageType,
                                   const char* aMessage,
                                   uint32_t aMessageSize,
                                   const char* aLegacyDestinationUrl,
                                   uint32_t aLegacyDestinationUrlLength)
{
  nsTArray<uint8_t> message;
  message.AppendElements(aMessage, aMessageSize);
  Unused << SendOnSessionMessage(nsCString(aSessionId, aSessionIdSize),
                                 aMessageType,
                                 message);
}

void
ChromiumCDMChild::OnSessionKeysChange(const char *aSessionId,
                                      uint32_t aSessionIdSize,
                                      bool aHasAdditionalUsableKey,
                                      const cdm::KeyInformation* aKeysInfo,
                                      uint32_t aKeysInfoCount)
{
  nsTArray<CDMKeyInformation> keys;
  keys.SetCapacity(aKeysInfoCount);
  for (uint32_t i = 0; i < aKeysInfoCount; i++) {
    const cdm::KeyInformation& key = aKeysInfo[i];
    nsTArray<uint8_t> kid;
    kid.AppendElements(key.key_id, key.key_id_size);
    keys.AppendElement(CDMKeyInformation(kid, key.status, key.system_code));
  }
  Unused << SendOnSessionKeysChange(nsCString(aSessionId, aSessionIdSize),
                                    keys);
}

void
ChromiumCDMChild::OnExpirationChange(const char* aSessionId,
                                     uint32_t aSessionIdSize,
                                     cdm::Time aNewExpiryTime)
{
  Unused << SendOnExpirationChange(nsCString(aSessionId, aSessionIdSize),
                                   aNewExpiryTime);
}

void
ChromiumCDMChild::OnSessionClosed(const char* aSessionId,
                                  uint32_t aSessionIdSize)
{
  Unused << SendOnSessionClosed(nsCString(aSessionId, aSessionIdSize));
}

void
ChromiumCDMChild::OnLegacySessionError(const char* aSessionId,
                                       uint32_t aSessionIdLength,
                                       cdm::Error aError,
                                       uint32_t aSystemCode,
                                       const char* aErrorMessage,
                                       uint32_t aErrorMessageLength)
{
  Unused << SendOnLegacySessionError(nsCString(aSessionId, aSessionIdLength),
                                     aError,
                                     aSystemCode,
                                     nsCString(aErrorMessage, aErrorMessageLength));
}

cdm::FileIO*
ChromiumCDMChild::CreateFileIO(cdm::FileIOClient * aClient)
{
  return nullptr;
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvInit(const bool& aAllowDistinctiveIdentifier,
                           const bool& aAllowPersistentState)
{
  if (mCDM) {
    mCDM->Initialize(aAllowDistinctiveIdentifier, aAllowPersistentState);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvSetServerCertificate(const uint32_t& aPromiseId,
                                           nsTArray<uint8_t>&& aServerCert)

{
  if (mCDM) {
    mCDM->SetServerCertificate(aPromiseId,
                               aServerCert.Elements(),
                               aServerCert.Length());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvCreateSessionAndGenerateRequest(
  const uint32_t& aPromiseId,
  const uint32_t& aSessionType,
  const uint32_t& aInitDataType,
  nsTArray<uint8_t>&& aInitData)
{
  MOZ_ASSERT(aSessionType <= cdm::SessionType::kPersistentKeyRelease);
  MOZ_ASSERT(aInitDataType <= cdm::InitDataType::kWebM);
  if (mCDM) {
    mCDM->CreateSessionAndGenerateRequest(aPromiseId,
                                          static_cast<cdm::SessionType>(aSessionType),
                                          static_cast<cdm::InitDataType>(aInitDataType),
                                          aInitData.Elements(),
                                          aInitData.Length());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvUpdateSession(const uint32_t& aPromiseId,
                                    const nsCString& aSessionId,
                                    nsTArray<uint8_t>&& aResponse)
{
  if (mCDM) {
    mCDM->UpdateSession(aPromiseId,
                        aSessionId.get(),
                        aSessionId.Length(),
                        aResponse.Elements(),
                        aResponse.Length());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvCloseSession(const uint32_t& aPromiseId,
                                   const nsCString& aSessionId)
{
  if (mCDM) {
    mCDM->CloseSession(aPromiseId, aSessionId.get(), aSessionId.Length());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvRemoveSession(const uint32_t& aPromiseId,
                                    const nsCString& aSessionId)
{
  if (mCDM) {
    mCDM->RemoveSession(aPromiseId, aSessionId.get(), aSessionId.Length());
  }
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
  if (mCDM) {
    mCDM->Destroy();
    mCDM = nullptr;
  }

  Unused << Send__delete__(this);

  return IPC_OK();
}

} // namespace gmp
} // namespace mozilla
