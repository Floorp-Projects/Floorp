/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPDecryptorChild.h"
#include "GMPChild.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/unused.h"
#include <ctime>

namespace mozilla {
namespace gmp {

GMPDecryptorChild::GMPDecryptorChild(GMPChild* aPlugin)
  : mSession(nullptr)
#ifdef DEBUG
  , mPlugin(aPlugin)
#endif
{
  MOZ_ASSERT(mPlugin);
}

GMPDecryptorChild::~GMPDecryptorChild()
{
}

void
GMPDecryptorChild::Init(GMPDecryptor* aSession)
{
  MOZ_ASSERT(aSession);
  mSession = aSession;
}

void
GMPDecryptorChild::ResolveNewSessionPromise(uint32_t aPromiseId,
                                            const char* aSessionId,
                                            uint32_t aSessionIdLength)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  nsAutoCString id(aSessionId, aSessionIdLength);
  SendResolveNewSessionPromise(aPromiseId, id);
}

void
GMPDecryptorChild::ResolvePromise(uint32_t aPromiseId)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  SendResolvePromise(aPromiseId);
}

void
GMPDecryptorChild::RejectPromise(uint32_t aPromiseId,
                                 GMPDOMException aException,
                                 const char* aMessage,
                                 uint32_t aMessageLength)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  nsAutoCString msg(aMessage, aMessageLength);
  SendRejectPromise(aPromiseId, aException, msg);
}

void
GMPDecryptorChild::SessionMessage(const char* aSessionId,
                                  uint32_t aSessionIdLength,
                                  const uint8_t* aMessage,
                                  uint32_t aMessageLength,
                                  const char* aDestinationURL,
                                  uint32_t aDestinationURLLength)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  nsAutoCString id(aSessionId, aSessionIdLength);
  nsTArray<uint8_t> msg;
  msg.AppendElements(aMessage, aMessageLength);
  nsAutoCString url(aDestinationURL, aDestinationURLLength);
  SendSessionMessage(id, msg, url);
}

void
GMPDecryptorChild::ExpirationChange(const char* aSessionId,
                                    uint32_t aSessionIdLength,
                                    GMPTimestamp aExpiryTime)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  nsAutoCString id(aSessionId, aSessionIdLength);
  SendExpirationChange(id, aExpiryTime);
}

void
GMPDecryptorChild::SessionClosed(const char* aSessionId,
                                 uint32_t aSessionIdLength)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  nsAutoCString id(aSessionId, aSessionIdLength);
  SendSessionClosed(id);
}

void
GMPDecryptorChild::SessionError(const char* aSessionId,
                                uint32_t aSessionIdLength,
                                GMPDOMException aException,
                                uint32_t aSystemCode,
                                const char* aMessage,
                                uint32_t aMessageLength)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  nsAutoCString id(aSessionId, aSessionIdLength);
  nsAutoCString msg(aMessage, aMessageLength);
  SendSessionError(id, aException, aSystemCode, msg);
}

void
GMPDecryptorChild::KeyIdUsable(const char* aSessionId,
                               uint32_t aSessionIdLength,
                               const uint8_t* aKeyId,
                               uint32_t aKeyIdLength)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  nsAutoCString sid(aSessionId, aSessionIdLength);
  nsAutoTArray<uint8_t, 16> kid;
  kid.AppendElements(aKeyId, aKeyIdLength);
  SendKeyIdUsable(sid, kid);
}

void
GMPDecryptorChild::KeyIdNotUsable(const char* aSessionId,
                                  uint32_t aSessionIdLength,
                                  const uint8_t* aKeyId,
                                  uint32_t aKeyIdLength)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  nsAutoCString sid(aSessionId, aSessionIdLength);
  nsAutoTArray<uint8_t, 16> kid;
  kid.AppendElements(aKeyId, aKeyIdLength);
  SendKeyIdNotUsable(sid, kid);
}

void
GMPDecryptorChild::Decrypted(GMPBuffer* aBuffer, GMPErr aResult)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  auto buffer = static_cast<GMPBufferImpl*>(aBuffer);
  SendDecrypted(buffer->mId, aResult, buffer->mData);
}

void
GMPDecryptorChild::SetCapabilities(uint64_t aCaps)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  SendSetCaps(aCaps);
}

void
GMPDecryptorChild::GetNodeId(const char** aOutNodeId,
                             uint32_t* aOutNodeIdLength)
{
  static const char* id = "placeholder_node_id";
  *aOutNodeId = id;
  *aOutNodeIdLength = strlen(id);
}

void
GMPDecryptorChild::GetSandboxVoucher(const uint8_t** aVoucher,
                                     uint8_t* aVoucherLength)
{
  const char* voucher = "placeholder_sandbox_voucher.";
  *aVoucher = (uint8_t*)voucher;
  *aVoucherLength = strlen(voucher);
}

void
GMPDecryptorChild::GetPluginVoucher(const uint8_t** aVoucher,
                                    uint8_t* aVoucherLength)
{
  const char* voucher = "placeholder_plugin_voucher.";
  *aVoucher = (uint8_t*)voucher;
  *aVoucherLength = strlen(voucher);
}

bool
GMPDecryptorChild::RecvInit()
{
  if (!mSession) {
    return false;
  }
  mSession->Init(this);
  return true;
}

bool
GMPDecryptorChild::RecvCreateSession(const uint32_t& aPromiseId,
                                     const nsCString& aInitDataType,
                                     const nsTArray<uint8_t>& aInitData,
                                     const GMPSessionType& aSessionType)
{
  if (!mSession) {
    return false;
  }

  mSession->CreateSession(aPromiseId,
                          aInitDataType.get(),
                          aInitDataType.Length(),
                          aInitData.Elements(),
                          aInitData.Length(),
                          aSessionType);

  return true;
}

bool
GMPDecryptorChild::RecvLoadSession(const uint32_t& aPromiseId,
                                   const nsCString& aSessionId)
{
  if (!mSession) {
    return false;
  }

  mSession->LoadSession(aPromiseId,
                        aSessionId.get(),
                        aSessionId.Length());

  return true;
}

bool
GMPDecryptorChild::RecvUpdateSession(const uint32_t& aPromiseId,
                                     const nsCString& aSessionId,
                                     const nsTArray<uint8_t>& aResponse)
{
  if (!mSession) {
    return false;
  }

  mSession->UpdateSession(aPromiseId,
                          aSessionId.get(),
                          aSessionId.Length(),
                          aResponse.Elements(),
                          aResponse.Length());

  return true;
}

bool
GMPDecryptorChild::RecvCloseSession(const uint32_t& aPromiseId,
                                    const nsCString& aSessionId)
{
  if (!mSession) {
    return false;
  }

  mSession->CloseSession(aPromiseId,
                         aSessionId.get(),
                         aSessionId.Length());

  return true;
}

bool
GMPDecryptorChild::RecvRemoveSession(const uint32_t& aPromiseId,
                                     const nsCString& aSessionId)
{
  if (!mSession) {
    return false;
  }

  mSession->RemoveSession(aPromiseId,
                          aSessionId.get(),
                          aSessionId.Length());

  return true;
}

bool
GMPDecryptorChild::RecvSetServerCertificate(const uint32_t& aPromiseId,
                                            const nsTArray<uint8_t>& aServerCert)
{
  if (!mSession) {
    return false;
  }

  mSession->SetServerCertificate(aPromiseId,
                                 aServerCert.Elements(),
                                 aServerCert.Length());

  return true;
}

bool
GMPDecryptorChild::RecvDecrypt(const uint32_t& aId,
                               const nsTArray<uint8_t>& aBuffer,
                               const GMPDecryptionData& aMetadata)
{
  if (!mSession) {
    return false;
  }

  GMPEncryptedBufferDataImpl metadata(aMetadata);

  mSession->Decrypt(new GMPBufferImpl(aId, aBuffer), &metadata);
  return true;
}

bool
GMPDecryptorChild::RecvDecryptingComplete()
{
  if (!mSession) {
    return false;
  }

  mSession->DecryptingComplete();
  mSession = nullptr;

  unused << Send__delete__(this);

  return true;
}

} // namespace gmp
} // namespace mozilla
