/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPDecryptorChild.h"
#include "GMPContentChild.h"
#include "GMPChild.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/unused.h"
#include "runnable_utils.h"
#include <ctime>

#define ON_GMP_THREAD() (mPlugin->GMPMessageLoop() == MessageLoop::current())

#define CALL_ON_GMP_THREAD(_func, ...) \
  CallOnGMPThread(&GMPDecryptorChild::_func, __VA_ARGS__)

namespace mozilla {
namespace gmp {

GMPDecryptorChild::GMPDecryptorChild(GMPContentChild* aPlugin,
                                     const nsTArray<uint8_t>& aPluginVoucher,
                                     const nsTArray<uint8_t>& aSandboxVoucher)
  : mSession(nullptr)
  , mPlugin(aPlugin)
  , mPluginVoucher(aPluginVoucher)
  , mSandboxVoucher(aSandboxVoucher)
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
    auto t = NewRunnableMethod(this, m, aMethod, aParams...);
    mPlugin->GMPMessageLoop()->PostTask(FROM_HERE, t);
  }
}

void
GMPDecryptorChild::Init(GMPDecryptor* aSession)
{
  MOZ_ASSERT(aSession);
  mSession = aSession;
}

void
GMPDecryptorChild::SetSessionId(uint32_t aCreateSessionToken,
                                const char* aSessionId,
                                uint32_t aSessionIdLength)
{
  CALL_ON_GMP_THREAD(SendSetSessionId,
                     aCreateSessionToken, nsAutoCString(aSessionId, aSessionIdLength));
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
                     aPromiseId, aException, nsAutoCString(aMessage, aMessageLength));
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
                     nsAutoCString(aSessionId, aSessionIdLength),
                     aMessageType, msg);
}

void
GMPDecryptorChild::ExpirationChange(const char* aSessionId,
                                    uint32_t aSessionIdLength,
                                    GMPTimestamp aExpiryTime)
{
  CALL_ON_GMP_THREAD(SendExpirationChange,
                     nsAutoCString(aSessionId, aSessionIdLength), aExpiryTime);
}

void
GMPDecryptorChild::SessionClosed(const char* aSessionId,
                                 uint32_t aSessionIdLength)
{
  CALL_ON_GMP_THREAD(SendSessionClosed,
                     nsAutoCString(aSessionId, aSessionIdLength));
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
                     nsAutoCString(aSessionId, aSessionIdLength),
                     aException, aSystemCode,
                     nsAutoCString(aMessage, aMessageLength));
}

void
GMPDecryptorChild::KeyStatusChanged(const char* aSessionId,
                                    uint32_t aSessionIdLength,
                                    const uint8_t* aKeyId,
                                    uint32_t aKeyIdLength,
                                    GMPMediaKeyStatus aStatus)
{
  nsAutoTArray<uint8_t, 16> kid;
  kid.AppendElements(aKeyId, aKeyIdLength);
  CALL_ON_GMP_THREAD(SendKeyStatusChanged,
                     nsAutoCString(aSessionId, aSessionIdLength), kid,
                     aStatus);
}

void
GMPDecryptorChild::Decrypted(GMPBuffer* aBuffer, GMPErr aResult)
{
  if (!ON_GMP_THREAD()) {
    // We should run this whole method on the GMP thread since the buffer needs
    // to be deleted after the SendDecrypted call.
    auto t = NewRunnableMethod(this, &GMPDecryptorChild::Decrypted, aBuffer, aResult);
    mPlugin->GMPMessageLoop()->PostTask(FROM_HERE, t);
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
  CALL_ON_GMP_THREAD(SendSetCaps, aCaps);
}

void
GMPDecryptorChild::GetSandboxVoucher(const uint8_t** aVoucher,
                                     uint32_t* aVoucherLength)
{
  if (!aVoucher || !aVoucherLength) {
    return;
  }
  *aVoucher = mSandboxVoucher.Elements();
  *aVoucherLength = mSandboxVoucher.Length();
}

void
GMPDecryptorChild::GetPluginVoucher(const uint8_t** aVoucher,
                                    uint32_t* aVoucherLength)
{
  if (!aVoucher || !aVoucherLength) {
    return;
  }
  *aVoucher = mPluginVoucher.Elements();
  *aVoucherLength = mPluginVoucher.Length();
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
GMPDecryptorChild::RecvCreateSession(const uint32_t& aCreateSessionToken,
                                     const uint32_t& aPromiseId,
                                     const nsCString& aInitDataType,
                                     InfallibleTArray<uint8_t>&& aInitData,
                                     const GMPSessionType& aSessionType)
{
  if (!mSession) {
    return false;
  }

  mSession->CreateSession(aCreateSessionToken,
                          aPromiseId,
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
                                     InfallibleTArray<uint8_t>&& aResponse)
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
                                            InfallibleTArray<uint8_t>&& aServerCert)
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
                               InfallibleTArray<uint8_t>&& aBuffer,
                               const GMPDecryptionData& aMetadata)
{
  if (!mSession) {
    return false;
  }

  // Note: the GMPBufferImpl created here is deleted when the GMP passes
  // it back in the Decrypted() callback above.
  GMPBufferImpl* buffer = new GMPBufferImpl(aId, aBuffer);

  // |metadata| lifetime is managed by |buffer|.
  GMPEncryptedBufferDataImpl* metadata = new GMPEncryptedBufferDataImpl(aMetadata);
  buffer->SetMetadata(metadata);

  mSession->Decrypt(buffer, metadata);
  return true;
}

bool
GMPDecryptorChild::RecvDecryptingComplete()
{
  // Reset |mSession| before calling DecryptingComplete(). We should not send
  // any IPC messages during tear-down.
  auto session = mSession;
  mSession = nullptr;

  if (!session) {
    return false;
  }

  session->DecryptingComplete();

  unused << Send__delete__(this);

  return true;
}

} // namespace gmp
} // namespace mozilla

// avoid redefined macro in unified build
#undef ON_GMP_THREAD
#undef CALL_ON_GMP_THREAD
