/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPDecryptorChild.h"
#include "GMPContentChild.h"
#include "GMPChild.h"
#include "base/task.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Unused.h"
#include "runnable_utils.h"
#include <ctime>

#define ON_GMP_THREAD() (mPlugin->GMPMessageLoop() == MessageLoop::current())

#define CALL_ON_GMP_THREAD(_func, ...) \
  CallOnGMPThread(&GMPDecryptorChild::_func, __VA_ARGS__)

namespace mozilla {
namespace gmp {

static uint32_t sDecryptorCount = 1;

GMPDecryptorChild::GMPDecryptorChild(GMPContentChild* aPlugin)
  : mSession(nullptr)
  , mPlugin(aPlugin)
  , mDecryptorId(sDecryptorCount++)
{
  MOZ_ASSERT(mPlugin);
}

GMPDecryptorChild::~GMPDecryptorChild()
{
}

template <typename MethodType, typename... ParamType>
void
GMPDecryptorChild::CallMethod(MethodType aMethod, ParamType&&... aParams)
{
  MOZ_ASSERT(ON_GMP_THREAD());
  // Don't send IPC messages after tear-down.
  if (mSession) {
    (this->*aMethod)(Forward<ParamType>(aParams)...);
  }
}

template<typename T>
struct AddConstReference {
  typedef const typename RemoveReference<T>::Type& Type;
};

template<typename MethodType, typename... ParamType>
void
GMPDecryptorChild::CallOnGMPThread(MethodType aMethod, ParamType&&... aParams)
{
  if (ON_GMP_THREAD()) {
    // Use forwarding reference when we can.
    CallMethod(aMethod, Forward<ParamType>(aParams)...);
  } else {
    // Use const reference when we have to.
    auto m = &GMPDecryptorChild::CallMethod<
        decltype(aMethod), typename AddConstReference<ParamType>::Type...>;
    RefPtr<mozilla::Runnable> t =
      dont_add_new_uses_of_this::NewRunnableMethod(this, m, aMethod, Forward<ParamType>(aParams)...);
    mPlugin->GMPMessageLoop()->PostTask(t.forget());
  }
}

void
GMPDecryptorChild::Init(GMPDecryptor* aSession)
{
  MOZ_ASSERT(aSession);
  mSession = aSession;
  // The ID of this decryptor is the IPDL actor ID. Note it's unique inside
  // the child process, but not necessarily across all gecko processes. However,
  // since GMPDecryptors are segregated by node ID/origin, we shouldn't end up
  // with clashes in the content process.
  SendSetDecryptorId(DecryptorId());
}

void
GMPDecryptorChild::SetSessionId(uint32_t aCreateSessionToken,
                                const char* aSessionId,
                                uint32_t aSessionIdLength)
{
  CALL_ON_GMP_THREAD(SendSetSessionId,
                     aCreateSessionToken, nsCString(aSessionId, aSessionIdLength));
}

void
GMPDecryptorChild::ResolveLoadSessionPromise(uint32_t aPromiseId,
                                             bool aSuccess)
{
  CALL_ON_GMP_THREAD(SendResolveLoadSessionPromise, aPromiseId, aSuccess);
}

void
GMPDecryptorChild::ResolvePromise(uint32_t aPromiseId)
{
  CALL_ON_GMP_THREAD(SendResolvePromise, aPromiseId);
}

void
GMPDecryptorChild::RejectPromise(uint32_t aPromiseId,
                                 GMPDOMException aException,
                                 const char* aMessage,
                                 uint32_t aMessageLength)
{
  CALL_ON_GMP_THREAD(SendRejectPromise,
                     aPromiseId, aException, nsCString(aMessage, aMessageLength));
}

void
GMPDecryptorChild::SessionMessage(const char* aSessionId,
                                  uint32_t aSessionIdLength,
                                  GMPSessionMessageType aMessageType,
                                  const uint8_t* aMessage,
                                  uint32_t aMessageLength)
{
  nsTArray<uint8_t> msg;
  msg.AppendElements(aMessage, aMessageLength);
  CALL_ON_GMP_THREAD(SendSessionMessage,
                     nsCString(aSessionId, aSessionIdLength),
                     aMessageType, Move(msg));
}

void
GMPDecryptorChild::ExpirationChange(const char* aSessionId,
                                    uint32_t aSessionIdLength,
                                    GMPTimestamp aExpiryTime)
{
  CALL_ON_GMP_THREAD(SendExpirationChange,
                     nsCString(aSessionId, aSessionIdLength), aExpiryTime);
}

void
GMPDecryptorChild::SessionClosed(const char* aSessionId,
                                 uint32_t aSessionIdLength)
{
  CALL_ON_GMP_THREAD(SendSessionClosed,
                     nsCString(aSessionId, aSessionIdLength));
}

void
GMPDecryptorChild::SessionError(const char* aSessionId,
                                uint32_t aSessionIdLength,
                                GMPDOMException aException,
                                uint32_t aSystemCode,
                                const char* aMessage,
                                uint32_t aMessageLength)
{
  CALL_ON_GMP_THREAD(SendSessionError,
                     nsCString(aSessionId, aSessionIdLength),
                     aException, aSystemCode,
                     nsCString(aMessage, aMessageLength));
}

void
GMPDecryptorChild::KeyStatusChanged(const char* aSessionId,
                                    uint32_t aSessionIdLength,
                                    const uint8_t* aKeyId,
                                    uint32_t aKeyIdLength,
                                    GMPMediaKeyStatus aStatus)
{
  AutoTArray<uint8_t, 16> kid;
  kid.AppendElements(aKeyId, aKeyIdLength);

  nsTArray<GMPKeyInformation> keyInfos;
  keyInfos.AppendElement(GMPKeyInformation(kid, aStatus));
  CALL_ON_GMP_THREAD(SendBatchedKeyStatusChanged,
                     nsCString(aSessionId, aSessionIdLength),
                     keyInfos);
}

void
GMPDecryptorChild::BatchedKeyStatusChanged(const char* aSessionId,
                                           uint32_t aSessionIdLength,
                                           const GMPMediaKeyInfo* aKeyInfos,
                                           uint32_t aKeyInfosLength)
{
  nsTArray<GMPKeyInformation> keyInfos;
  for (uint32_t i = 0; i < aKeyInfosLength; i++) {
    nsTArray<uint8_t> keyId;
    keyId.AppendElements(aKeyInfos[i].keyid, aKeyInfos[i].keyid_size);
    keyInfos.AppendElement(GMPKeyInformation(keyId, aKeyInfos[i].status));
  }
  CALL_ON_GMP_THREAD(SendBatchedKeyStatusChanged,
                     nsCString(aSessionId, aSessionIdLength),
                     keyInfos);
}

void
GMPDecryptorChild::Decrypted(GMPBuffer* aBuffer, GMPErr aResult)
{
  if (!ON_GMP_THREAD()) {
    // We should run this whole method on the GMP thread since the buffer needs
    // to be deleted after the SendDecrypted call.
    mPlugin->GMPMessageLoop()->PostTask(NewRunnableMethod
                                        <GMPBuffer*, GMPErr>(this,
                                                             &GMPDecryptorChild::Decrypted,
                                                             aBuffer, aResult));
    return;
  }

  if (!aBuffer) {
    NS_WARNING("GMPDecryptorCallback passed bull GMPBuffer");
    return;
  }

  auto buffer = static_cast<GMPBufferImpl*>(aBuffer);
  if (mSession) {
    SendDecrypted(buffer->mId, aResult, buffer->mData);
  }
  delete buffer;
}

void
GMPDecryptorChild::SetCapabilities(uint64_t aCaps)
{
  // Deprecated.
}

mozilla::ipc::IPCResult
GMPDecryptorChild::RecvInit(const bool& aDistinctiveIdentifierRequired,
                            const bool& aPersistentStateRequired)
{
  if (!mSession) {
    return IPC_FAIL_NO_REASON(this);
  }
  mSession->Init(this, aDistinctiveIdentifierRequired, aPersistentStateRequired);
  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPDecryptorChild::RecvCreateSession(const uint32_t& aCreateSessionToken,
                                     const uint32_t& aPromiseId,
                                     const nsCString& aInitDataType,
                                     InfallibleTArray<uint8_t>&& aInitData,
                                     const GMPSessionType& aSessionType)
{
  if (!mSession) {
    return IPC_FAIL_NO_REASON(this);
  }

  mSession->CreateSession(aCreateSessionToken,
                          aPromiseId,
                          aInitDataType.get(),
                          aInitDataType.Length(),
                          aInitData.Elements(),
                          aInitData.Length(),
                          aSessionType);

  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPDecryptorChild::RecvLoadSession(const uint32_t& aPromiseId,
                                   const nsCString& aSessionId)
{
  if (!mSession) {
    return IPC_FAIL_NO_REASON(this);
  }

  mSession->LoadSession(aPromiseId,
                        aSessionId.get(),
                        aSessionId.Length());

  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPDecryptorChild::RecvUpdateSession(const uint32_t& aPromiseId,
                                     const nsCString& aSessionId,
                                     InfallibleTArray<uint8_t>&& aResponse)
{
  if (!mSession) {
    return IPC_FAIL_NO_REASON(this);
  }

  mSession->UpdateSession(aPromiseId,
                          aSessionId.get(),
                          aSessionId.Length(),
                          aResponse.Elements(),
                          aResponse.Length());

  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPDecryptorChild::RecvCloseSession(const uint32_t& aPromiseId,
                                    const nsCString& aSessionId)
{
  if (!mSession) {
    return IPC_FAIL_NO_REASON(this);
  }

  mSession->CloseSession(aPromiseId,
                         aSessionId.get(),
                         aSessionId.Length());

  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPDecryptorChild::RecvRemoveSession(const uint32_t& aPromiseId,
                                     const nsCString& aSessionId)
{
  if (!mSession) {
    return IPC_FAIL_NO_REASON(this);
  }

  mSession->RemoveSession(aPromiseId,
                          aSessionId.get(),
                          aSessionId.Length());

  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPDecryptorChild::RecvSetServerCertificate(const uint32_t& aPromiseId,
                                            InfallibleTArray<uint8_t>&& aServerCert)
{
  if (!mSession) {
    return IPC_FAIL_NO_REASON(this);
  }

  mSession->SetServerCertificate(aPromiseId,
                                 aServerCert.Elements(),
                                 aServerCert.Length());

  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPDecryptorChild::RecvDecrypt(const uint32_t& aId,
                               InfallibleTArray<uint8_t>&& aBuffer,
                               const GMPDecryptionData& aMetadata)
{
  if (!mSession) {
    return IPC_FAIL_NO_REASON(this);
  }

  // Note: the GMPBufferImpl created here is deleted when the GMP passes
  // it back in the Decrypted() callback above.
  GMPBufferImpl* buffer = new GMPBufferImpl(aId, aBuffer);

  // |metadata| lifetime is managed by |buffer|.
  GMPEncryptedBufferDataImpl* metadata = new GMPEncryptedBufferDataImpl(aMetadata);
  buffer->SetMetadata(metadata);

  mSession->Decrypt(buffer, metadata);
  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPDecryptorChild::RecvDecryptingComplete()
{
  // Reset |mSession| before calling DecryptingComplete(). We should not send
  // any IPC messages during tear-down.
  auto session = mSession;
  mSession = nullptr;

  if (!session) {
    return IPC_FAIL_NO_REASON(this);
  }

  session->DecryptingComplete();

  Unused << Send__delete__(this);

  return IPC_OK();
}

} // namespace gmp
} // namespace mozilla

// avoid redefined macro in unified build
#undef ON_GMP_THREAD
#undef CALL_ON_GMP_THREAD
