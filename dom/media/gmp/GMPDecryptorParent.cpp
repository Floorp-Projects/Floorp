/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPDecryptorParent.h"
#include "GMPContentParent.h"
#include "MediaData.h"
#include "mozilla/Unused.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

extern LogModule* GetGMPLog();

#define LOGV(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Verbose, msg)
#define LOGD(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Debug, msg)
#define LOG(level, msg) MOZ_LOG(GetGMPLog(), (level), msg)

namespace gmp {

GMPDecryptorParent::GMPDecryptorParent(GMPContentParent* aPlugin)
  : mIsOpen(false)
  , mShuttingDown(false)
  , mActorDestroyed(false)
  , mPlugin(aPlugin)
  , mPluginId(aPlugin->GetPluginId())
  , mCallback(nullptr)
#ifdef DEBUG
  , mGMPThread(aPlugin->GMPThread())
#endif
{
  MOZ_ASSERT(mPlugin && mGMPThread);
}

GMPDecryptorParent::~GMPDecryptorParent()
{
}

nsresult
GMPDecryptorParent::Init(GMPDecryptorProxyCallback* aCallback,
                         bool aDistinctiveIdentifierRequired,
                         bool aPersistentStateRequired)
{
  LOGD(("GMPDecryptorParent[%p]::Init()", this));

  if (mIsOpen) {
    NS_WARNING("Trying to re-use an in-use GMP decrypter!");
    return NS_ERROR_FAILURE;
  }
  mCallback = aCallback;
  if (!SendInit(aDistinctiveIdentifierRequired, aPersistentStateRequired)) {
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
  LOGD(("GMPDecryptorParent[%p]::CreateSession(token=%u, promiseId=%u, aInitData='%s')",
        this, aCreateSessionToken, aPromiseId, ToBase64(aInitData).get()));

  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return;
  }
  // Caller should ensure parameters passed in from JS are valid.
  MOZ_ASSERT(!aInitDataType.IsEmpty() && !aInitData.IsEmpty());
  Unused << SendCreateSession(aCreateSessionToken, aPromiseId, aInitDataType, aInitData, aSessionType);
}

void
GMPDecryptorParent::LoadSession(uint32_t aPromiseId,
                                const nsCString& aSessionId)
{
  LOGD(("GMPDecryptorParent[%p]::LoadSession(sessionId='%s', promiseId=%u)",
        this, aSessionId.get(), aPromiseId));
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return;
  }
  // Caller should ensure parameters passed in from JS are valid.
  MOZ_ASSERT(!aSessionId.IsEmpty());
  Unused << SendLoadSession(aPromiseId, aSessionId);
}

void
GMPDecryptorParent::UpdateSession(uint32_t aPromiseId,
                                  const nsCString& aSessionId,
                                  const nsTArray<uint8_t>& aResponse)
{
  LOGD(("GMPDecryptorParent[%p]::UpdateSession(sessionId='%s', promiseId=%u response='%s')",
        this, aSessionId.get(), aPromiseId, ToBase64(aResponse).get()));

  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return;
  }
  // Caller should ensure parameters passed in from JS are valid.
  MOZ_ASSERT(!aSessionId.IsEmpty() && !aResponse.IsEmpty());
  Unused << SendUpdateSession(aPromiseId, aSessionId, aResponse);
}

void
GMPDecryptorParent::CloseSession(uint32_t aPromiseId,
                                 const nsCString& aSessionId)
{
  LOGD(("GMPDecryptorParent[%p]::CloseSession(sessionId='%s', promiseId=%u)",
         this, aSessionId.get(), aPromiseId));

  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return;
  }
  // Caller should ensure parameters passed in from JS are valid.
  MOZ_ASSERT(!aSessionId.IsEmpty());
  Unused << SendCloseSession(aPromiseId, aSessionId);
}

void
GMPDecryptorParent::RemoveSession(uint32_t aPromiseId,
                                  const nsCString& aSessionId)
{
  LOGD(("GMPDecryptorParent[%p]::RemoveSession(sessionId='%s', promiseId=%u)",
        this, aSessionId.get(), aPromiseId));

  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return;
  }
  // Caller should ensure parameters passed in from JS are valid.
  MOZ_ASSERT(!aSessionId.IsEmpty());
  Unused << SendRemoveSession(aPromiseId, aSessionId);
}

void
GMPDecryptorParent::SetServerCertificate(uint32_t aPromiseId,
                                         const nsTArray<uint8_t>& aServerCert)
{
  LOGD(("GMPDecryptorParent[%p]::SetServerCertificate(promiseId=%u)",
        this, aPromiseId));

  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return;
  }
  // Caller should ensure parameters passed in from JS are valid.
  MOZ_ASSERT(!aServerCert.IsEmpty());
  Unused << SendSetServerCertificate(aPromiseId, aServerCert);
}

void
GMPDecryptorParent::Decrypt(uint32_t aId,
                            const CryptoSample& aCrypto,
                            const nsTArray<uint8_t>& aBuffer)
{
  LOGV(("GMPDecryptorParent[%p]::Decrypt(id=%d)", this, aId));

  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return;
  }

  // Caller should ensure parameters passed in are valid.
  MOZ_ASSERT(!aBuffer.IsEmpty());

  if (aCrypto.mValid) {
    GMPDecryptionData data(aCrypto.mKeyId,
                           aCrypto.mIV,
                           aCrypto.mPlainSizes,
                           aCrypto.mEncryptedSizes,
                           aCrypto.mSessionIds);

    Unused << SendDecrypt(aId, aBuffer, data);
  } else {
    GMPDecryptionData data;
    Unused << SendDecrypt(aId, aBuffer, data);
  }
}

bool
GMPDecryptorParent::RecvSetSessionId(const uint32_t& aCreateSessionId,
                                     const nsCString& aSessionId)
{
  LOGD(("GMPDecryptorParent[%p]::RecvSetSessionId(token=%u, sessionId='%s')",
        this, aCreateSessionId, aSessionId.get()));

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
  LOGD(("GMPDecryptorParent[%p]::RecvResolveLoadSessionPromise(promiseId=%u)",
        this, aPromiseId));

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
  LOGD(("GMPDecryptorParent[%p]::RecvResolvePromise(promiseId=%u)",
        this, aPromiseId));

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
  LOGD(("GMPDecryptorParent[%p]::RecvRejectPromise(promiseId=%u, exception=%d, msg='%s')",
        this, aPromiseId, aException, aMessage.get()));

  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return false;
  }
  mCallback->RejectPromise(aPromiseId, GMPExToNsresult(aException), aMessage);
  return true;
}


static dom::MediaKeyMessageType
ToMediaKeyMessageType(GMPSessionMessageType aMessageType) {
  switch (aMessageType) {
    case kGMPLicenseRequest: return dom::MediaKeyMessageType::License_request;
    case kGMPLicenseRenewal: return dom::MediaKeyMessageType::License_renewal;
    case kGMPLicenseRelease: return dom::MediaKeyMessageType::License_release;
    case kGMPIndividualizationRequest: return dom::MediaKeyMessageType::Individualization_request;
    default: return dom::MediaKeyMessageType::License_request;
  };
};

bool
GMPDecryptorParent::RecvSessionMessage(const nsCString& aSessionId,
                                       const GMPSessionMessageType& aMessageType,
                                       nsTArray<uint8_t>&& aMessage)
{
  LOGD(("GMPDecryptorParent[%p]::RecvSessionMessage(sessionId='%s', type=%d, msg='%s')",
        this, aSessionId.get(), aMessageType, ToBase64(aMessage).get()));

  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return false;
  }
  mCallback->SessionMessage(aSessionId, ToMediaKeyMessageType(aMessageType), aMessage);
  return true;
}

bool
GMPDecryptorParent::RecvExpirationChange(const nsCString& aSessionId,
                                         const double& aExpiryTime)
{
  LOGD(("GMPDecryptorParent[%p]::RecvExpirationChange(sessionId='%s', expiry=%lf)",
        this, aSessionId.get(), aExpiryTime));

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
  LOGD(("GMPDecryptorParent[%p]::RecvSessionClosed(sessionId='%s')",
        this, aSessionId.get()));

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
  LOGD(("GMPDecryptorParent[%p]::RecvSessionError(sessionId='%s', exception=%d, sysCode=%d, msg='%s')",
        this, aSessionId.get(),
        aException, aSystemCode, aMessage.get()));

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

static dom::MediaKeyStatus
ToMediaKeyStatus(GMPMediaKeyStatus aStatus) {
  switch (aStatus) {
    case kGMPUsable: return dom::MediaKeyStatus::Usable;
    case kGMPExpired: return dom::MediaKeyStatus::Expired;
    case kGMPOutputDownscaled: return dom::MediaKeyStatus::Output_downscaled;
    case kGMPOutputRestricted: return dom::MediaKeyStatus::Output_restricted;
    case kGMPInternalError: return dom::MediaKeyStatus::Internal_error;
    case kGMPReleased: return dom::MediaKeyStatus::Released;
    case kGMPStatusPending: return dom::MediaKeyStatus::Status_pending;
    default: return dom::MediaKeyStatus::Internal_error;
  }
}

bool
GMPDecryptorParent::RecvKeyStatusChanged(const nsCString& aSessionId,
                                         InfallibleTArray<uint8_t>&& aKeyId,
                                         const GMPMediaKeyStatus& aStatus)
{
  LOGD(("GMPDecryptorParent[%p]::RecvKeyStatusChanged(sessionId='%s', keyId=%s, status=%d)",
        this, aSessionId.get(), ToBase64(aKeyId).get(), aStatus));

  if (mIsOpen) {
    mCallback->KeyStatusChanged(aSessionId, aKeyId, ToMediaKeyStatus(aStatus));
  }
  return true;
}

bool
GMPDecryptorParent::RecvForgetKeyStatus(const nsCString& aSessionId,
                                        InfallibleTArray<uint8_t>&& aKeyId)
{
  LOGD(("GMPDecryptorParent[%p]::RecvForgetKeyStatus(sessionId='%s', keyId=%s)",
        this, aSessionId.get(), ToBase64(aKeyId).get()));

  if (mIsOpen) {
    mCallback->ForgetKeyStatus(aSessionId, aKeyId);
  }
  return true;
}

DecryptStatus
ToDecryptStatus(GMPErr aError)
{
  switch (aError) {
    case GMPNoErr: return Ok;
    case GMPNoKeyErr: return NoKeyErr;
    case GMPAbortedErr: return AbortedErr;
    default: return GenericErr;
  }
}

bool
GMPDecryptorParent::RecvDecrypted(const uint32_t& aId,
                                  const GMPErr& aErr,
                                  InfallibleTArray<uint8_t>&& aBuffer)
{
  LOGV(("GMPDecryptorParent[%p]::RecvDecrypted(id=%d, err=%d)",
        this, aId, aErr));

  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP decrypter!");
    return false;
  }
  mCallback->Decrypted(aId, ToDecryptStatus(aErr), aBuffer);
  return true;
}

bool
GMPDecryptorParent::RecvShutdown()
{
  LOGD(("GMPDecryptorParent[%p]::RecvShutdown()", this));

  Shutdown();
  return true;
}

// Note: may be called via Terminated()
void
GMPDecryptorParent::Close()
{
  LOGD(("GMPDecryptorParent[%p]::Close()", this));
  MOZ_ASSERT(mGMPThread == NS_GetCurrentThread());

  // Consumer is done with us; we can shut down.  No more callbacks should
  // be made to mCallback. Note: do this before Shutdown()!
  mCallback = nullptr;
  // Let Shutdown mark us as dead so it knows if we had been alive

  // In case this is the last reference
  RefPtr<GMPDecryptorParent> kungfudeathgrip(this);
  this->Release();
  Shutdown();
}

void
GMPDecryptorParent::Shutdown()
{
  LOGD(("GMPDecryptorParent[%p]::Shutdown()", this));
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
    Unused << SendDecryptingComplete();
  }
}

// Note: Keep this sync'd up with Shutdown
void
GMPDecryptorParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGD(("GMPDecryptorParent[%p]::ActorDestroy(reason=%d)", this, aWhy));

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
  MaybeDisconnect(aWhy == AbnormalShutdown);
}

bool
GMPDecryptorParent::Recv__delete__()
{
  LOGD(("GMPDecryptorParent[%p]::Recv__delete__()", this));

  if (mPlugin) {
    mPlugin->DecryptorDestroyed(this);
    mPlugin = nullptr;
  }
  return true;
}

} // namespace gmp
} // namespace mozilla
