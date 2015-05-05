/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPDecryptorParent.h"
#include "GMPContentParent.h"
#include "MediaData.h"
#include "mozilla/unused.h"

namespace mozilla {
namespace gmp {

GMPDecryptorParent::GMPDecryptorParent(GMPContentParent* aPlugin)
  : mIsOpen(false)
  , mShuttingDown(false)
  , mActorDestroyed(false)
  , mPlugin(aPlugin)
  , mCallback(nullptr)
#ifdef DEBUG
  , mGMPThread(aPlugin->GMPThread())
#endif
{
  MOZ_ASSERT(mPlugin && mGMPThread);
  mPluginId = aPlugin->GetPluginId();
}

GMPDecryptorParent::~GMPDecryptorParent()
{
}

const uint32_t
GMPDecryptorParent::GetPluginId() const
{
  return mPluginId;
}

nsresult
GMPDecryptorParent::Init(GMPDecryptorProxyCallback* aCallback)
{
  if (mIsOpen) {
    NS_WARNING("Trying to re-use an in-use GMP decrypter!");
    return NS_ERROR_FAILURE;
  }
  mCallback = aCallback;
  if (!SendInit()) {
    return NS_ERROR_FAILURE;
  }
  mIsOpen = true;
  return NS_OK;
}

void
GMPDecryptorParent::CreateSession(uint32_t aCreateSessionToken,
                                  uint32_t aPromiseId,
                                  const nsCString& aInitDataType,
                                  const nsTArray<uint8_t>& aInitData,
                                  GMPSessionType aSessionType)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return;
  }
  // Caller should ensure parameters passed in from JS are valid.
  MOZ_ASSERT(!aInitDataType.IsEmpty() && !aInitData.IsEmpty());
  unused << SendCreateSession(aCreateSessionToken, aPromiseId, aInitDataType, aInitData, aSessionType);
}

void
GMPDecryptorParent::LoadSession(uint32_t aPromiseId,
                                const nsCString& aSessionId)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return;
  }
  // Caller should ensure parameters passed in from JS are valid.
  MOZ_ASSERT(!aSessionId.IsEmpty());
  unused << SendLoadSession(aPromiseId, aSessionId);
}

void
GMPDecryptorParent::UpdateSession(uint32_t aPromiseId,
                                  const nsCString& aSessionId,
                                  const nsTArray<uint8_t>& aResponse)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return;
  }
  // Caller should ensure parameters passed in from JS are valid.
  MOZ_ASSERT(!aSessionId.IsEmpty() && !aResponse.IsEmpty());
  unused << SendUpdateSession(aPromiseId, aSessionId, aResponse);
}

void
GMPDecryptorParent::CloseSession(uint32_t aPromiseId,
                                 const nsCString& aSessionId)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return;
  }
  // Caller should ensure parameters passed in from JS are valid.
  MOZ_ASSERT(!aSessionId.IsEmpty());
  unused << SendCloseSession(aPromiseId, aSessionId);
}

void
GMPDecryptorParent::RemoveSession(uint32_t aPromiseId,
                                  const nsCString& aSessionId)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return;
  }
  // Caller should ensure parameters passed in from JS are valid.
  MOZ_ASSERT(!aSessionId.IsEmpty());
  unused << SendRemoveSession(aPromiseId, aSessionId);
}

void
GMPDecryptorParent::SetServerCertificate(uint32_t aPromiseId,
                                         const nsTArray<uint8_t>& aServerCert)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return;
  }
  // Caller should ensure parameters passed in from JS are valid.
  MOZ_ASSERT(!aServerCert.IsEmpty());
  unused << SendSetServerCertificate(aPromiseId, aServerCert);
}

void
GMPDecryptorParent::Decrypt(uint32_t aId,
                            const CryptoSample& aCrypto,
                            const nsTArray<uint8_t>& aBuffer)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return;
  }

  // Caller should ensure parameters passed in are valid.
  MOZ_ASSERT(!aBuffer.IsEmpty() && aCrypto.mValid);

  GMPDecryptionData data(aCrypto.mKeyId,
                         aCrypto.mIV,
                         aCrypto.mPlainSizes,
                         aCrypto.mEncryptedSizes,
                         aCrypto.mSessionIds);

  unused << SendDecrypt(aId, aBuffer, data);
}

bool
GMPDecryptorParent::RecvSetSessionId(const uint32_t& aCreateSessionId,
                                     const nsCString& aSessionId)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return false;
  }
  mCallback->SetSessionId(aCreateSessionId, aSessionId);
  return true;
}

bool
GMPDecryptorParent::RecvResolveLoadSessionPromise(const uint32_t& aPromiseId,
                                                  const bool& aSuccess)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return false;
  }
  mCallback->ResolveLoadSessionPromise(aPromiseId, aSuccess);
  return true;
}

bool
GMPDecryptorParent::RecvResolvePromise(const uint32_t& aPromiseId)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return false;
  }
  mCallback->ResolvePromise(aPromiseId);
  return true;
}

nsresult
GMPExToNsresult(GMPDOMException aDomException) {
  switch (aDomException) {
    case kGMPNoModificationAllowedError: return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
    case kGMPNotFoundError: return NS_ERROR_DOM_NOT_FOUND_ERR;
    case kGMPNotSupportedError: return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    case kGMPInvalidStateError: return NS_ERROR_DOM_INVALID_STATE_ERR;
    case kGMPSyntaxError: return NS_ERROR_DOM_SYNTAX_ERR;
    case kGMPInvalidModificationError: return NS_ERROR_DOM_INVALID_MODIFICATION_ERR;
    case kGMPInvalidAccessError: return NS_ERROR_DOM_INVALID_ACCESS_ERR;
    case kGMPSecurityError: return NS_ERROR_DOM_SECURITY_ERR;
    case kGMPAbortError: return NS_ERROR_DOM_ABORT_ERR;
    case kGMPQuotaExceededError: return NS_ERROR_DOM_QUOTA_EXCEEDED_ERR;
    case kGMPTimeoutError: return NS_ERROR_DOM_TIMEOUT_ERR;
    default: return NS_ERROR_DOM_UNKNOWN_ERR;
  }
}

bool
GMPDecryptorParent::RecvRejectPromise(const uint32_t& aPromiseId,
                                      const GMPDOMException& aException,
                                      const nsCString& aMessage)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return false;
  }
  mCallback->RejectPromise(aPromiseId, GMPExToNsresult(aException), aMessage);
  return true;
}

bool
GMPDecryptorParent::RecvSessionMessage(const nsCString& aSessionId,
                                       const GMPSessionMessageType& aMessageType,
                                       nsTArray<uint8_t>&& aMessage)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return false;
  }
  mCallback->SessionMessage(aSessionId, aMessageType, aMessage);
  return true;
}

bool
GMPDecryptorParent::RecvExpirationChange(const nsCString& aSessionId,
                                         const double& aExpiryTime)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return false;
  }
  mCallback->ExpirationChange(aSessionId, aExpiryTime);
  return true;
}

bool
GMPDecryptorParent::RecvSessionClosed(const nsCString& aSessionId)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return false;
  }
  mCallback->SessionClosed(aSessionId);
  return true;
}

bool
GMPDecryptorParent::RecvSessionError(const nsCString& aSessionId,
                                     const GMPDOMException& aException,
                                     const uint32_t& aSystemCode,
                                     const nsCString& aMessage)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return false;
  }
  mCallback->SessionError(aSessionId,
                          GMPExToNsresult(aException),
                          aSystemCode,
                          aMessage);
  return true;
}

bool
GMPDecryptorParent::RecvKeyStatusChanged(const nsCString& aSessionId,
                                         InfallibleTArray<uint8_t>&& aKeyId,
                                         const GMPMediaKeyStatus& aStatus)
{
  if (mIsOpen) {
    mCallback->KeyStatusChanged(aSessionId, aKeyId, aStatus);
  }
  return true;
}

bool
GMPDecryptorParent::RecvSetCaps(const uint64_t& aCaps)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return false;
  }
  mCallback->SetCaps(aCaps);
  return true;
}

bool
GMPDecryptorParent::RecvDecrypted(const uint32_t& aId,
                                  const GMPErr& aErr,
                                  InfallibleTArray<uint8_t>&& aBuffer)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return false;
  }
  mCallback->Decrypted(aId, aErr, aBuffer);
  return true;
}

bool
GMPDecryptorParent::RecvShutdown()
{
  Shutdown();
  return true;
}

// Note: may be called via Terminated()
void
GMPDecryptorParent::Close()
{
  MOZ_ASSERT(mGMPThread == NS_GetCurrentThread());
  // Consumer is done with us; we can shut down.  No more callbacks should
  // be made to mCallback. Note: do this before Shutdown()!
  mCallback = nullptr;
  // Let Shutdown mark us as dead so it knows if we had been alive

  // In case this is the last reference
  nsRefPtr<GMPDecryptorParent> kungfudeathgrip(this);
  this->Release();
  Shutdown();
}

void
GMPDecryptorParent::Shutdown()
{
  MOZ_ASSERT(mGMPThread == NS_GetCurrentThread());

  if (mShuttingDown) {
    return;
  }
  mShuttingDown = true;

  // Notify client we're gone!  Won't occur after Close()
  if (mCallback) {
    mCallback->Terminated();
    mCallback = nullptr;
  }

  mIsOpen = false;
  if (!mActorDestroyed) {
    unused << SendDecryptingComplete();
  }
}

// Note: Keep this sync'd up with Shutdown
void
GMPDecryptorParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mIsOpen = false;
  mActorDestroyed = true;
  if (mCallback) {
    // May call Close() (and Shutdown()) immediately or with a delay
    mCallback->Terminated();
    mCallback = nullptr;
  }
  if (mPlugin) {
    mPlugin->DecryptorDestroyed(this);
    mPlugin = nullptr;
  }
}

bool
GMPDecryptorParent::Recv__delete__()
{
  if (mPlugin) {
    mPlugin->DecryptorDestroyed(this);
    mPlugin = nullptr;
  }
  return true;
}

} // namespace gmp
} // namespace mozilla
