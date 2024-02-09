/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMParent.h"

#include "ChromiumCDMCallback.h"
#include "ChromiumCDMCallbackProxy.h"
#include "ChromiumCDMProxy.h"
#include "content_decryption_module.h"
#include "GMPContentChild.h"
#include "GMPContentParent.h"
#include "GMPLog.h"
#include "GMPService.h"
#include "GMPUtils.h"
#include "VideoUtils.h"
#include "mozilla/dom/MediaKeyMessageEventBinding.h"
#include "mozilla/gmp/GMPTypes.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/Unused.h"
#include "AnnexB.h"
#include "H264.h"

#define NS_DispatchToMainThread(...) CompileError_UseAbstractMainThreadInstead

namespace mozilla::gmp {

using namespace eme;

ChromiumCDMParent::ChromiumCDMParent(GMPContentParent* aContentParent,
                                     uint32_t aPluginId)
    : mPluginId(aPluginId),
      mContentParent(aContentParent),
      mVideoShmemLimit(StaticPrefs::media_eme_chromium_api_video_shmems())
#ifdef DEBUG
      ,
      mGMPThread(aContentParent->GMPEventTarget())
#endif
{
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG(
      "ChromiumCDMParent::ChromiumCDMParent(this=%p, contentParent=%p, "
      "id=%" PRIu32 ")",
      this, aContentParent, aPluginId);
}

RefPtr<ChromiumCDMParent::InitPromise> ChromiumCDMParent::Init(
    ChromiumCDMCallback* aCDMCallback, bool aAllowDistinctiveIdentifier,
    bool aAllowPersistentState, nsIEventTarget* aMainThread) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG(
      "ChromiumCDMParent::Init(this=%p) shutdown=%s abormalShutdown=%s "
      "actorDestroyed=%s",
      this, mIsShutdown ? "true" : "false",
      mAbnormalShutdown ? "true" : "false", mActorDestroyed ? "true" : "false");
  if (!aCDMCallback || !aMainThread) {
    GMP_LOG_DEBUG(
        "ChromiumCDMParent::Init(this=%p) failed "
        "nullCallback=%s nullMainThread=%s",
        this, !aCDMCallback ? "true" : "false",
        !aMainThread ? "true" : "false");

    return ChromiumCDMParent::InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_FAILURE,
                    nsPrintfCString("ChromiumCDMParent::Init() failed "
                                    "nullCallback=%s nullMainThread=%s",
                                    !aCDMCallback ? "true" : "false",
                                    !aMainThread ? "true" : "false")),
        __func__);
  }

  RefPtr<ChromiumCDMParent::InitPromise> promise =
      mInitPromise.Ensure(__func__);
  RefPtr<ChromiumCDMParent> self = this;
  SendInit(aAllowDistinctiveIdentifier, aAllowPersistentState)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self, aCDMCallback](bool aSuccess) {
            if (!aSuccess) {
              GMP_LOG_DEBUG(
                  "ChromiumCDMParent::Init() failed with callback from "
                  "child indicating CDM failed init");
              self->mInitPromise.RejectIfExists(
                  MediaResult(NS_ERROR_FAILURE,
                              "ChromiumCDMParent::Init() failed with callback "
                              "from child indicating CDM failed init"),
                  __func__);
              return;
            }
            GMP_LOG_DEBUG(
                "ChromiumCDMParent::Init() succeeded with callback from child");
            self->mCDMCallback = aCDMCallback;
            self->mInitPromise.ResolveIfExists(true /* unused */, __func__);
          },
          [self](ResponseRejectReason&& aReason) {
            RefPtr<gmp::GeckoMediaPluginService> service =
                gmp::GeckoMediaPluginService::GetGeckoMediaPluginService();
            bool xpcomWillShutdown =
                service && service->XPCOMWillShutdownReceived();
            GMP_LOG_DEBUG(
                "ChromiumCDMParent::Init(this=%p) failed "
                "shutdown=%s cdmCrash=%s actorDestroyed=%s "
                "browserShutdown=%s promiseRejectReason=%d",
                self.get(), self->mIsShutdown ? "true" : "false",
                self->mAbnormalShutdown ? "true" : "false",
                self->mActorDestroyed ? "true" : "false",
                xpcomWillShutdown ? "true" : "false",
                static_cast<int>(aReason));
            self->mInitPromise.RejectIfExists(
                MediaResult(
                    NS_ERROR_FAILURE,
                    nsPrintfCString("ChromiumCDMParent::Init() failed "
                                    "shutdown=%s cdmCrash=%s actorDestroyed=%s "
                                    "browserShutdown=%s promiseRejectReason=%d",
                                    self->mIsShutdown ? "true" : "false",
                                    self->mAbnormalShutdown ? "true" : "false",
                                    self->mActorDestroyed ? "true" : "false",
                                    xpcomWillShutdown ? "true" : "false",
                                    static_cast<int>(aReason))),
                __func__);
          });
  return promise;
}

void ChromiumCDMParent::CreateSession(uint32_t aCreateSessionToken,
                                      uint32_t aSessionType,
                                      uint32_t aInitDataType,
                                      uint32_t aPromiseId,
                                      const nsTArray<uint8_t>& aInitData) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::CreateSession(this=%p)", this);
  if (mIsShutdown) {
    RejectPromiseShutdown(aPromiseId);
    return;
  }
  if (!SendCreateSessionAndGenerateRequest(aPromiseId, aSessionType,
                                           aInitDataType, aInitData)) {
    RejectPromiseWithStateError(
        aPromiseId, "Failed to send generateRequest to CDM process."_ns);
    return;
  }
  mPromiseToCreateSessionToken.InsertOrUpdate(aPromiseId, aCreateSessionToken);
}

void ChromiumCDMParent::LoadSession(uint32_t aPromiseId, uint32_t aSessionType,
                                    nsString aSessionId) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::LoadSession(this=%p, pid=%" PRIu32
                ", type=%" PRIu32 ", sid=%s)",
                this, aPromiseId, aSessionType,
                NS_ConvertUTF16toUTF8(aSessionId).get());
  if (mIsShutdown) {
    RejectPromiseShutdown(aPromiseId);
    return;
  }
  if (!SendLoadSession(aPromiseId, aSessionType,
                       NS_ConvertUTF16toUTF8(aSessionId))) {
    RejectPromiseWithStateError(
        aPromiseId, "Failed to send loadSession to CDM process."_ns);
    return;
  }
}

void ChromiumCDMParent::SetServerCertificate(uint32_t aPromiseId,
                                             const nsTArray<uint8_t>& aCert) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::SetServerCertificate(this=%p)", this);
  if (mIsShutdown) {
    RejectPromiseShutdown(aPromiseId);
    return;
  }
  if (!SendSetServerCertificate(aPromiseId, aCert)) {
    RejectPromiseWithStateError(
        aPromiseId, "Failed to send setServerCertificate to CDM process"_ns);
  }
}

void ChromiumCDMParent::UpdateSession(const nsCString& aSessionId,
                                      uint32_t aPromiseId,
                                      const nsTArray<uint8_t>& aResponse) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::UpdateSession(this=%p)", this);
  if (mIsShutdown) {
    RejectPromiseShutdown(aPromiseId);
    return;
  }
  if (!SendUpdateSession(aPromiseId, aSessionId, aResponse)) {
    RejectPromiseWithStateError(
        aPromiseId, "Failed to send updateSession to CDM process"_ns);
  }
}

void ChromiumCDMParent::CloseSession(const nsCString& aSessionId,
                                     uint32_t aPromiseId) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::CloseSession(this=%p)", this);
  if (mIsShutdown) {
    RejectPromiseShutdown(aPromiseId);
    return;
  }
  if (!SendCloseSession(aPromiseId, aSessionId)) {
    RejectPromiseWithStateError(
        aPromiseId, "Failed to send closeSession to CDM process"_ns);
  }
}

void ChromiumCDMParent::RemoveSession(const nsCString& aSessionId,
                                      uint32_t aPromiseId) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::RemoveSession(this=%p)", this);
  if (mIsShutdown) {
    RejectPromiseShutdown(aPromiseId);
    return;
  }
  if (!SendRemoveSession(aPromiseId, aSessionId)) {
    RejectPromiseWithStateError(
        aPromiseId, "Failed to send removeSession to CDM process"_ns);
  }
}

void ChromiumCDMParent::NotifyOutputProtectionStatus(bool aSuccess,
                                                     uint32_t aLinkMask,
                                                     uint32_t aProtectionMask) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::NotifyOutputProtectionStatus(this=%p)",
                this);
  if (mIsShutdown) {
    return;
  }
  const bool haveCachedValue = mOutputProtectionLinkMask.isSome();
  if (mAwaitingOutputProtectionInformation && !aSuccess) {
    MOZ_DIAGNOSTIC_ASSERT(
        !haveCachedValue,
        "Should not have a cached value if we're still awaiting infomation");
    // We're awaiting info and don't yet have a cached value, and the check
    // failed, don't cache the result, just forward the failure.
    CompleteQueryOutputProtectionStatus(false, aLinkMask, aProtectionMask);
    return;
  }
  if (!mAwaitingOutputProtectionInformation && haveCachedValue && !aSuccess) {
    // We're not awaiting info, already have a cached value, and the check
    // failed. Ignore this, we'll update our info from any future successful
    // checks.
    return;
  }
  MOZ_ASSERT(aSuccess, "Failed checks should be handled by this point");
  // Update our protection information.
  mOutputProtectionLinkMask = Some(aLinkMask);

  if (mAwaitingOutputProtectionInformation) {
    // If we have an outstanding query, service that query with this
    // information.
    mAwaitingOutputProtectionInformation = false;
    MOZ_ASSERT(!haveCachedValue,
               "If we were waiting on information, we shouldn't have yet "
               "cached a value");
    CompleteQueryOutputProtectionStatus(true, mOutputProtectionLinkMask.value(),
                                        aProtectionMask);
  }
}

void ChromiumCDMParent::CompleteQueryOutputProtectionStatus(
    bool aSuccess, uint32_t aLinkMask, uint32_t aProtectionMask) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG(
      "ChromiumCDMParent::CompleteQueryOutputProtectionStatus(this=%p) "
      "mIsShutdown=%s aSuccess=%s aLinkMask=%" PRIu32,
      this, mIsShutdown ? "true" : "false", aSuccess ? "true" : "false",
      aLinkMask);
  if (mIsShutdown) {
    return;
  }
  Unused << SendCompleteQueryOutputProtectionStatus(aSuccess, aLinkMask,
                                                    aProtectionMask);
}

// See
// https://cs.chromium.org/chromium/src/media/blink/webcontentdecryptionmodule_impl.cc?l=33-66&rcl=d49aa59ac8c2925d5bec229f3f1906537b6b4547
static Result<cdm::HdcpVersion, nsresult> ToCDMHdcpVersion(
    const nsCString& aMinHdcpVersion) {
  if (aMinHdcpVersion.IsEmpty()) {
    return cdm::HdcpVersion::kHdcpVersionNone;
  }
  if (aMinHdcpVersion.EqualsIgnoreCase("1.0")) {
    return cdm::HdcpVersion::kHdcpVersion1_0;
  }
  if (aMinHdcpVersion.EqualsIgnoreCase("1.1")) {
    return cdm::HdcpVersion::kHdcpVersion1_1;
  }
  if (aMinHdcpVersion.EqualsIgnoreCase("1.2")) {
    return cdm::HdcpVersion::kHdcpVersion1_2;
  }
  if (aMinHdcpVersion.EqualsIgnoreCase("1.3")) {
    return cdm::HdcpVersion::kHdcpVersion1_3;
  }
  if (aMinHdcpVersion.EqualsIgnoreCase("1.4")) {
    return cdm::HdcpVersion::kHdcpVersion1_4;
  }
  if (aMinHdcpVersion.EqualsIgnoreCase("2.0")) {
    return cdm::HdcpVersion::kHdcpVersion2_0;
  }
  if (aMinHdcpVersion.EqualsIgnoreCase("2.1")) {
    return cdm::HdcpVersion::kHdcpVersion2_1;
  }
  if (aMinHdcpVersion.EqualsIgnoreCase("2.2")) {
    return cdm::HdcpVersion::kHdcpVersion2_2;
  }
  // When adding another version remember to update GMPMessageUtils so that we
  // can serialize it correctly and have correct bounds on the enum!

  // Invalid hdcp version string.
  return Err(NS_ERROR_INVALID_ARG);
}

void ChromiumCDMParent::GetStatusForPolicy(uint32_t aPromiseId,
                                           const nsCString& aMinHdcpVersion) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::GetStatusForPolicy(this=%p)", this);
  if (mIsShutdown) {
    RejectPromiseShutdown(aPromiseId);
    return;
  }
  auto hdcpVersionResult = ToCDMHdcpVersion(aMinHdcpVersion);
  if (hdcpVersionResult.isErr()) {
    ErrorResult rv;
    // XXXbz there's no spec for this yet, and
    // <https://github.com/WICG/hdcp-detection/blob/master/explainer.md>
    // does not define what exceptions get thrown.  Let's assume
    // TypeError for invalid args, as usual.
    constexpr auto err =
        "getStatusForPolicy failed due to bad hdcp version argument"_ns;
    rv.ThrowTypeError(err);
    RejectPromise(aPromiseId, std::move(rv), err);
    return;
  }

  if (!SendGetStatusForPolicy(aPromiseId, hdcpVersionResult.unwrap())) {
    RejectPromiseWithStateError(
        aPromiseId, "Failed to send getStatusForPolicy to CDM process"_ns);
  }
}

bool ChromiumCDMParent::InitCDMInputBuffer(gmp::CDMInputBuffer& aBuffer,
                                           MediaRawData* aSample) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  const CryptoSample& crypto = aSample->mCrypto;
  if (crypto.mEncryptedSizes.Length() != crypto.mPlainSizes.Length()) {
    GMP_LOG_DEBUG("InitCDMInputBuffer clear/cipher subsamples don't match");
    return false;
  }

  Shmem shmem;
  if (!AllocShmem(aSample->Size(), &shmem)) {
    return false;
  }
  memcpy(shmem.get<uint8_t>(), aSample->Data(), aSample->Size());
  cdm::EncryptionScheme encryptionScheme = cdm::EncryptionScheme::kUnencrypted;
  switch (crypto.mCryptoScheme) {
    case CryptoScheme::None:
      break;  // Default to none
    case CryptoScheme::Cenc:
      encryptionScheme = cdm::EncryptionScheme::kCenc;
      break;
    case CryptoScheme::Cbcs:
      encryptionScheme = cdm::EncryptionScheme::kCbcs;
      break;
    default:
      GMP_LOG_DEBUG(
          "InitCDMInputBuffer got unexpected encryption scheme with "
          "value of %" PRIu8 ". Treating as no encryption.",
          static_cast<uint8_t>(crypto.mCryptoScheme));
      MOZ_ASSERT_UNREACHABLE("Should not have unrecognized encryption type");
      break;
  }

  const nsTArray<uint8_t>& iv = encryptionScheme != cdm::EncryptionScheme::kCbcs
                                    ? crypto.mIV
                                    : crypto.mConstantIV;
  aBuffer = gmp::CDMInputBuffer(
      shmem, crypto.mKeyId, iv, aSample->mTime.ToMicroseconds(),
      aSample->mDuration.ToMicroseconds(), crypto.mPlainSizes,
      crypto.mEncryptedSizes, crypto.mCryptByteBlock, crypto.mSkipByteBlock,
      encryptionScheme);
  return true;
}

bool ChromiumCDMParent::SendBufferToCDM(uint32_t aSizeInBytes) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::SendBufferToCDM() size=%" PRIu32,
                aSizeInBytes);
  Shmem shmem;
  if (!AllocShmem(aSizeInBytes, &shmem)) {
    return false;
  }
  if (!SendGiveBuffer(std::move(shmem))) {
    DeallocShmem(shmem);
    return false;
  }
  return true;
}

RefPtr<DecryptPromise> ChromiumCDMParent::Decrypt(MediaRawData* aSample) {
  if (mIsShutdown) {
    MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
    return DecryptPromise::CreateAndReject(DecryptResult(GenericErr, aSample),
                                           __func__);
  }
  CDMInputBuffer buffer;
  if (!InitCDMInputBuffer(buffer, aSample)) {
    return DecryptPromise::CreateAndReject(DecryptResult(GenericErr, aSample),
                                           __func__);
  }
  // Send a buffer to the CDM to store the output. The CDM will either fill
  // it with the decrypted sample and return it, or deallocate it on failure.
  if (!SendBufferToCDM(aSample->Size())) {
    DeallocShmem(buffer.mData());
    return DecryptPromise::CreateAndReject(DecryptResult(GenericErr, aSample),
                                           __func__);
  }

  RefPtr<DecryptJob> job = new DecryptJob(aSample);
  if (!SendDecrypt(job->mId, buffer)) {
    GMP_LOG_DEBUG(
        "ChromiumCDMParent::Decrypt(this=%p) failed to send decrypt message",
        this);
    DeallocShmem(buffer.mData());
    return DecryptPromise::CreateAndReject(DecryptResult(GenericErr, aSample),
                                           __func__);
  }
  RefPtr<DecryptPromise> promise = job->Ensure();
  mDecrypts.AppendElement(job);
  return promise;
}

ipc::IPCResult ChromiumCDMParent::Recv__delete__() {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  MOZ_ASSERT(mIsShutdown);
  GMP_LOG_DEBUG("ChromiumCDMParent::Recv__delete__(this=%p)", this);
  if (mContentParent) {
    mContentParent->ChromiumCDMDestroyed(this);
    mContentParent = nullptr;
  }
  return IPC_OK();
}

ipc::IPCResult ChromiumCDMParent::RecvOnResolvePromiseWithKeyStatus(
    const uint32_t& aPromiseId, const uint32_t& aKeyStatus) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG(
      "ChromiumCDMParent::RecvOnResolvePromiseWithKeyStatus(this=%p, "
      "pid=%" PRIu32 ", keystatus=%" PRIu32 ")",
      this, aPromiseId, aKeyStatus);
  if (!mCDMCallback || mIsShutdown) {
    return IPC_OK();
  }

  mCDMCallback->ResolvePromiseWithKeyStatus(aPromiseId, aKeyStatus);

  return IPC_OK();
}

ipc::IPCResult ChromiumCDMParent::RecvOnResolveNewSessionPromise(
    const uint32_t& aPromiseId, const nsCString& aSessionId) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG(
      "ChromiumCDMParent::RecvOnResolveNewSessionPromise(this=%p, pid=%" PRIu32
      ", sid=%s)",
      this, aPromiseId, aSessionId.get());
  if (!mCDMCallback || mIsShutdown) {
    return IPC_OK();
  }

  Maybe<uint32_t> token = mPromiseToCreateSessionToken.Extract(aPromiseId);
  if (token.isNothing()) {
    RejectPromiseWithStateError(aPromiseId,
                                "Lost session token for new session."_ns);
    return IPC_OK();
  }

  mCDMCallback->SetSessionId(token.value(), aSessionId);

  ResolvePromise(aPromiseId);

  return IPC_OK();
}

ipc::IPCResult ChromiumCDMParent::RecvResolveLoadSessionPromise(
    const uint32_t& aPromiseId, const bool& aSuccessful) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG(
      "ChromiumCDMParent::RecvResolveLoadSessionPromise(this=%p, pid=%" PRIu32
      ", successful=%d)",
      this, aPromiseId, aSuccessful);
  if (!mCDMCallback || mIsShutdown) {
    return IPC_OK();
  }

  mCDMCallback->ResolveLoadSessionPromise(aPromiseId, aSuccessful);

  return IPC_OK();
}

void ChromiumCDMParent::ResolvePromise(uint32_t aPromiseId) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::ResolvePromise(this=%p, pid=%" PRIu32 ")",
                this, aPromiseId);

  // Note: The MediaKeys rejects all pending DOM promises when it
  // initiates shutdown.
  if (!mCDMCallback || mIsShutdown) {
    return;
  }

  mCDMCallback->ResolvePromise(aPromiseId);
}

ipc::IPCResult ChromiumCDMParent::RecvOnResolvePromise(
    const uint32_t& aPromiseId) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  ResolvePromise(aPromiseId);
  return IPC_OK();
}

void ChromiumCDMParent::RejectPromise(uint32_t aPromiseId,
                                      ErrorResult&& aException,
                                      const nsCString& aErrorMessage) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::RejectPromise(this=%p, pid=%" PRIu32 ")",
                this, aPromiseId);
  // Note: The MediaKeys rejects all pending DOM promises when it
  // initiates shutdown.
  if (!mCDMCallback || mIsShutdown) {
    // Suppress the exception as it will not be explicitly handled due to the
    // early return.
    aException.SuppressException();
    return;
  }

  mCDMCallback->RejectPromise(aPromiseId, std::move(aException), aErrorMessage);
}

void ChromiumCDMParent::RejectPromiseShutdown(uint32_t aPromiseId) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  RejectPromiseWithStateError(aPromiseId, "CDM is shutdown"_ns);
}

void ChromiumCDMParent::RejectPromiseWithStateError(
    uint32_t aPromiseId, const nsCString& aErrorMessage) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  ErrorResult rv;
  rv.ThrowInvalidStateError(aErrorMessage);
  RejectPromise(aPromiseId, std::move(rv), aErrorMessage);
}

static ErrorResult ToErrorResult(uint32_t aException,
                                 const nsCString& aErrorMessage) {
  // XXXbz could we have a CopyableErrorResult sent to us with a better error
  // message?
  ErrorResult rv;
  switch (static_cast<cdm::Exception>(aException)) {
    case cdm::Exception::kExceptionNotSupportedError:
      rv.ThrowNotSupportedError(aErrorMessage);
      break;
    case cdm::Exception::kExceptionInvalidStateError:
      rv.ThrowInvalidStateError(aErrorMessage);
      break;
    case cdm::Exception::kExceptionTypeError:
      rv.ThrowTypeError(aErrorMessage);
      break;
    case cdm::Exception::kExceptionQuotaExceededError:
      rv.ThrowQuotaExceededError(aErrorMessage);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid cdm::Exception enum value.");
      // Note: Unique placeholder.
      rv.ThrowTimeoutError(aErrorMessage);
  };
  return rv;
}

ipc::IPCResult ChromiumCDMParent::RecvOnRejectPromise(
    const uint32_t& aPromiseId, const uint32_t& aException,
    const uint32_t& aSystemCode, const nsCString& aErrorMessage) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  RejectPromise(aPromiseId, ToErrorResult(aException, aErrorMessage),
                aErrorMessage);
  return IPC_OK();
}

ipc::IPCResult ChromiumCDMParent::RecvOnSessionMessage(
    const nsCString& aSessionId, const uint32_t& aMessageType,
    nsTArray<uint8_t>&& aMessage) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::RecvOnSessionMessage(this=%p, sid=%s)",
                this, aSessionId.get());
  if (!mCDMCallback || mIsShutdown) {
    return IPC_OK();
  }

  mCDMCallback->SessionMessage(aSessionId, aMessageType, std::move(aMessage));
  return IPC_OK();
}

ipc::IPCResult ChromiumCDMParent::RecvOnSessionKeysChange(
    const nsCString& aSessionId, nsTArray<CDMKeyInformation>&& aKeysInfo) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::RecvOnSessionKeysChange(this=%p)", this);
  if (!mCDMCallback || mIsShutdown) {
    return IPC_OK();
  }

  mCDMCallback->SessionKeysChange(aSessionId, std::move(aKeysInfo));
  return IPC_OK();
}

ipc::IPCResult ChromiumCDMParent::RecvOnExpirationChange(
    const nsCString& aSessionId, const double& aSecondsSinceEpoch) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::RecvOnExpirationChange(this=%p) time=%lf",
                this, aSecondsSinceEpoch);
  if (!mCDMCallback || mIsShutdown) {
    return IPC_OK();
  }

  mCDMCallback->ExpirationChange(aSessionId, aSecondsSinceEpoch);
  return IPC_OK();
}

ipc::IPCResult ChromiumCDMParent::RecvOnSessionClosed(
    const nsCString& aSessionId) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::RecvOnSessionClosed(this=%p)", this);
  if (!mCDMCallback || mIsShutdown) {
    return IPC_OK();
  }

  mCDMCallback->SessionClosed(aSessionId);
  return IPC_OK();
}

ipc::IPCResult ChromiumCDMParent::RecvOnQueryOutputProtectionStatus() {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG(
      "ChromiumCDMParent::RecvOnQueryOutputProtectionStatus(this=%p) "
      "mIsShutdown=%s mCDMCallback=%s mAwaitingOutputProtectionInformation=%s",
      this, mIsShutdown ? "true" : "false", mCDMCallback ? "true" : "false",
      mAwaitingOutputProtectionInformation ? "true" : "false");
  if (mIsShutdown) {
    // We're shutdown, don't try to service the query.
    return IPC_OK();
  }
  if (!mCDMCallback) {
    // We don't have a callback. We're not yet outputting anything so can report
    // we're safe.
    CompleteQueryOutputProtectionStatus(true, uint32_t{}, uint32_t{});
    return IPC_OK();
  }

  if (mOutputProtectionLinkMask.isSome()) {
    MOZ_DIAGNOSTIC_ASSERT(
        !mAwaitingOutputProtectionInformation,
        "If we have a cached value we should not be awaiting information");
    // We have a cached value, use that.
    CompleteQueryOutputProtectionStatus(true, mOutputProtectionLinkMask.value(),
                                        uint32_t{});
    return IPC_OK();
  }

  // We need to call up the stack to get the info. The CDM proxy will finish
  // the request via `NotifyOutputProtectionStatus`.
  mAwaitingOutputProtectionInformation = true;
  mCDMCallback->QueryOutputProtectionStatus();
  return IPC_OK();
}

DecryptStatus ToDecryptStatus(uint32_t aStatus) {
  switch (static_cast<cdm::Status>(aStatus)) {
    case cdm::kSuccess:
      return DecryptStatus::Ok;
    case cdm::kNoKey:
      return DecryptStatus::NoKeyErr;
    default:
      return DecryptStatus::GenericErr;
  }
}

ipc::IPCResult ChromiumCDMParent::RecvDecryptFailed(const uint32_t& aId,
                                                    const uint32_t& aStatus) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::RecvDecryptFailed(this=%p, id=%" PRIu32
                ", status=%" PRIu32 ")",
                this, aId, aStatus);

  if (mIsShutdown) {
    MOZ_ASSERT(mDecrypts.IsEmpty());
    return IPC_OK();
  }

  for (size_t i = 0; i < mDecrypts.Length(); i++) {
    if (mDecrypts[i]->mId == aId) {
      mDecrypts[i]->PostResult(ToDecryptStatus(aStatus));
      mDecrypts.RemoveElementAt(i);
      break;
    }
  }
  return IPC_OK();
}

ipc::IPCResult ChromiumCDMParent::RecvDecrypted(const uint32_t& aId,
                                                const uint32_t& aStatus,
                                                ipc::Shmem&& aShmem) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::RecvDecrypted(this=%p, id=%" PRIu32
                ", status=%" PRIu32 ")",
                this, aId, aStatus);

  // We must deallocate the shmem once we've copied the result out of it
  // in PostResult below.
  auto autoDeallocateShmem = MakeScopeExit([&, this] { DeallocShmem(aShmem); });

  if (mIsShutdown) {
    MOZ_ASSERT(mDecrypts.IsEmpty());
    return IPC_OK();
  }
  for (size_t i = 0; i < mDecrypts.Length(); i++) {
    if (mDecrypts[i]->mId == aId) {
      mDecrypts[i]->PostResult(
          ToDecryptStatus(aStatus),
          Span<const uint8_t>(aShmem.get<uint8_t>(), aShmem.Size<uint8_t>()));
      mDecrypts.RemoveElementAt(i);
      break;
    }
  }
  return IPC_OK();
}

ipc::IPCResult ChromiumCDMParent::RecvIncreaseShmemPoolSize() {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("%s(this=%p) limit=%" PRIu32 " active=%" PRIu32, __func__, this,
                mVideoShmemLimit, mVideoShmemsActive);

  // Put an upper limit on the number of shmems we tolerate the CDM asking
  // for, to prevent a memory blow-out. In practice, we expect the CDM to
  // need less than 5, but some encodings require more.
  // We'd expect CDMs to not have video frames larger than 720p-1080p
  // (due to DRM robustness requirements), which is about 1.5MB-3MB per
  // frame.
  if (mVideoShmemLimit > 50) {
    mDecodePromise.RejectIfExists(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Failled to ensure CDM has enough shmems.")),
        __func__);
    Shutdown();
    return IPC_OK();
  }
  mVideoShmemLimit++;

  EnsureSufficientShmems(mVideoFrameBufferSize);

  return IPC_OK();
}

bool ChromiumCDMParent::PurgeShmems() {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG(
      "ChromiumCDMParent::PurgeShmems(this=%p) frame_size=%zu"
      " limit=%" PRIu32 " active=%" PRIu32,
      this, mVideoFrameBufferSize, mVideoShmemLimit, mVideoShmemsActive);

  if (mVideoShmemsActive == 0) {
    // We haven't allocated any shmems, nothing to do here.
    return true;
  }
  if (!SendPurgeShmems()) {
    return false;
  }
  mVideoShmemsActive = 0;
  return true;
}

bool ChromiumCDMParent::EnsureSufficientShmems(size_t aVideoFrameSize) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG(
      "ChromiumCDMParent::EnsureSufficientShmems(this=%p) "
      "size=%zu expected_size=%zu limit=%" PRIu32 " active=%" PRIu32,
      this, aVideoFrameSize, mVideoFrameBufferSize, mVideoShmemLimit,
      mVideoShmemsActive);

  // The Chromium CDM API requires us to implement a synchronous
  // interface to supply buffers to the CDM for it to write decrypted samples
  // into. We want our buffers to be backed by shmems, in order to reduce
  // the overhead of transferring decoded frames. However due to sandboxing
  // restrictions, the CDM process cannot allocate shmems itself.
  // We don't want to be doing synchronous IPC to request shmems from the
  // content process, nor do we want to have to do intr IPC or make async
  // IPC conform to the sync allocation interface. So instead we have the
  // content process pre-allocate a set of shmems and give them to the CDM
  // process in advance of them being needed.
  //
  // When the CDM needs to allocate a buffer for storing a decoded video
  // frame, the CDM host gives it one of these shmems' buffers. When this
  // is sent back to the content process, we upload it to a GPU surface,
  // and send the shmem back to the CDM process so it can reuse it.
  //
  // Normally the CDM won't allocate more than one buffer at once, but
  // we've seen cases where it allocates multiple buffers, returns one and
  // holds onto the rest. So we need to ensure we have several extra
  // shmems pre-allocated for the CDM. This threshold is set by the pref
  // media.eme.chromium-api.video-shmems.
  //
  // We also have a failure recovery mechanism; if the CDM asks for more
  // buffers than we have shmem's available, ChromiumCDMChild gives the
  // CDM a non-shared memory buffer, and returns the frame to the parent
  // in an nsTArray<uint8_t> instead of a shmem. The child then sends a
  // message to the parent asking it to increase the number of shmems in
  // the pool. Via this mechanism we should recover from incorrectly
  // predicting how many shmems to pre-allocate.
  //
  // At decoder start up, we guess how big the shmems need to be based on
  // the video frame dimensions. If we guess wrong, the CDM will follow
  // the non-shmem path, and we'll re-create the shmems of the correct size.
  // This meanns we can recover from guessing the shmem size wrong.
  // We must re-take this path after every decoder de-init/re-init, as the
  // frame sizes should change every time we switch video stream.

  if (mVideoFrameBufferSize < aVideoFrameSize) {
    if (!PurgeShmems()) {
      return false;
    }
    mVideoFrameBufferSize = aVideoFrameSize;
  }

  while (mVideoShmemsActive < mVideoShmemLimit) {
    if (!SendBufferToCDM(mVideoFrameBufferSize)) {
      return false;
    }
    mVideoShmemsActive++;
  }

  return true;
}

ipc::IPCResult ChromiumCDMParent::RecvDecodedData(const CDMVideoFrame& aFrame,
                                                  nsTArray<uint8_t>&& aData) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::RecvDecodedData(this=%p) time=%" PRId64,
                this, aFrame.mTimestamp());

  if (mIsShutdown || mDecodePromise.IsEmpty()) {
    return IPC_OK();
  }

  if (!EnsureSufficientShmems(aData.Length())) {
    mDecodePromise.RejectIfExists(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Failled to ensure CDM has enough shmems.")),
        __func__);
    return IPC_OK();
  }

  RefPtr<VideoData> v = CreateVideoFrame(aFrame, aData);
  if (!v) {
    mDecodePromise.RejectIfExists(
        MediaResult(NS_ERROR_OUT_OF_MEMORY,
                    RESULT_DETAIL("Can't create VideoData")),
        __func__);
    return IPC_OK();
  }

  ReorderAndReturnOutput(std::move(v));

  return IPC_OK();
}

ipc::IPCResult ChromiumCDMParent::RecvDecodedShmem(const CDMVideoFrame& aFrame,
                                                   ipc::Shmem&& aShmem) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::RecvDecodedShmem(this=%p) time=%" PRId64
                " duration=%" PRId64,
                this, aFrame.mTimestamp(), aFrame.mDuration());

  // On failure we need to deallocate the shmem we're to return to the
  // CDM. On success we return it to the CDM to be reused.
  auto autoDeallocateShmem =
      MakeScopeExit([&, this] { this->DeallocShmem(aShmem); });

  if (mIsShutdown || mDecodePromise.IsEmpty()) {
    return IPC_OK();
  }

  RefPtr<VideoData> v = CreateVideoFrame(
      aFrame, Span<uint8_t>(aShmem.get<uint8_t>(), aShmem.Size<uint8_t>()));
  if (!v) {
    mDecodePromise.RejectIfExists(
        MediaResult(NS_ERROR_OUT_OF_MEMORY,
                    RESULT_DETAIL("Can't create VideoData")),
        __func__);
    return IPC_OK();
  }

  // Return the shmem to the CDM so the shmem can be reused to send us
  // another frame.
  if (!SendGiveBuffer(std::move(aShmem))) {
    mDecodePromise.RejectIfExists(
        MediaResult(NS_ERROR_OUT_OF_MEMORY,
                    RESULT_DETAIL("Can't return shmem to CDM process")),
        __func__);
    return IPC_OK();
  }

  // Don't need to deallocate the shmem since the CDM process is responsible
  // for it again.
  autoDeallocateShmem.release();

  ReorderAndReturnOutput(std::move(v));

  return IPC_OK();
}

void ChromiumCDMParent::ReorderAndReturnOutput(RefPtr<VideoData>&& aFrame) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  if (mMaxRefFrames == 0) {
    mDecodePromise.ResolveIfExists(
        MediaDataDecoder::DecodedData({std::move(aFrame)}), __func__);
    return;
  }
  mReorderQueue.Push(std::move(aFrame));
  MediaDataDecoder::DecodedData results;
  while (mReorderQueue.Length() > mMaxRefFrames) {
    results.AppendElement(mReorderQueue.Pop());
  }
  mDecodePromise.Resolve(std::move(results), __func__);
}

already_AddRefed<VideoData> ChromiumCDMParent::CreateVideoFrame(
    const CDMVideoFrame& aFrame, Span<uint8_t> aData) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  MOZ_ASSERT(aData.Length() > 0);
  GMP_LOG_DEBUG(
      "ChromiumCDMParent::CreateVideoFrame(this=%p aFrame.mFormat=%" PRIu32 ")",
      this, aFrame.mFormat());

  if (aFrame.mFormat() == cdm::VideoFormat::kUnknownVideoFormat) {
    GMP_LOG_DEBUG(
        "ChromiumCDMParent::CreateVideoFrame(this=%p) Got kUnknownVideoFormat, "
        "bailing.",
        this);
    return nullptr;
  }

  if (aFrame.mFormat() == cdm::VideoFormat::kYUV420P9 ||
      aFrame.mFormat() == cdm::VideoFormat::kYUV422P9 ||
      aFrame.mFormat() == cdm::VideoFormat::kYUV444P9) {
    // If we ever hit this we can reconsider support, but 9 bit formats
    // should be so rare as to be non-existent.
    GMP_LOG_DEBUG(
        "ChromiumCDMParent::CreateVideoFrame(this=%p) Got a 9 bit depth pixel "
        "format. We don't support these, bailing.",
        this);
    return nullptr;
  }

  VideoData::YCbCrBuffer b;

  // Determine the dimensions of our chroma planes, color depth and chroma
  // subsampling.
  uint32_t chromaWidth = (aFrame.mImageWidth() + 1) / 2;
  uint32_t chromaHeight = (aFrame.mImageHeight() + 1) / 2;
  gfx::ColorDepth colorDepth = gfx::ColorDepth::COLOR_8;
  gfx::ChromaSubsampling chromaSubsampling =
      gfx::ChromaSubsampling::HALF_WIDTH_AND_HEIGHT;
  switch (aFrame.mFormat()) {
    case cdm::VideoFormat::kYv12:
    case cdm::VideoFormat::kI420:
      break;
    case cdm::VideoFormat::kYUV420P10:
      colorDepth = gfx::ColorDepth::COLOR_10;
      break;
    case cdm::VideoFormat::kYUV422P10:
      chromaHeight = aFrame.mImageHeight();
      colorDepth = gfx::ColorDepth::COLOR_10;
      chromaSubsampling = gfx::ChromaSubsampling::HALF_WIDTH;
      break;
    case cdm::VideoFormat::kYUV444P10:
      chromaWidth = aFrame.mImageWidth();
      chromaHeight = aFrame.mImageHeight();
      colorDepth = gfx::ColorDepth::COLOR_10;
      chromaSubsampling = gfx::ChromaSubsampling::FULL;
      break;
    case cdm::VideoFormat::kYUV420P12:
      colorDepth = gfx::ColorDepth::COLOR_12;
      break;
    case cdm::VideoFormat::kYUV422P12:
      chromaHeight = aFrame.mImageHeight();
      colorDepth = gfx::ColorDepth::COLOR_12;
      chromaSubsampling = gfx::ChromaSubsampling::HALF_WIDTH;
      break;
    case cdm::VideoFormat::kYUV444P12:
      chromaWidth = aFrame.mImageWidth();
      chromaHeight = aFrame.mImageHeight();
      colorDepth = gfx::ColorDepth::COLOR_12;
      chromaSubsampling = gfx::ChromaSubsampling::FULL;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Should handle all formats");
      return nullptr;
  }

  // Since we store each plane separately we can just roll the offset
  // into our pointer to that plane and store that.
  b.mPlanes[0].mData = aData.Elements() + aFrame.mYPlane().mPlaneOffset();
  b.mPlanes[0].mWidth = aFrame.mImageWidth();
  b.mPlanes[0].mHeight = aFrame.mImageHeight();
  b.mPlanes[0].mStride = aFrame.mYPlane().mStride();
  b.mPlanes[0].mSkip = 0;

  b.mPlanes[1].mData = aData.Elements() + aFrame.mUPlane().mPlaneOffset();
  b.mPlanes[1].mWidth = chromaWidth;
  b.mPlanes[1].mHeight = chromaHeight;
  b.mPlanes[1].mStride = aFrame.mUPlane().mStride();
  b.mPlanes[1].mSkip = 0;

  b.mPlanes[2].mData = aData.Elements() + aFrame.mVPlane().mPlaneOffset();
  b.mPlanes[2].mWidth = chromaWidth;
  b.mPlanes[2].mHeight = chromaHeight;
  b.mPlanes[2].mStride = aFrame.mVPlane().mStride();
  b.mPlanes[2].mSkip = 0;

  b.mColorDepth = colorDepth;
  b.mChromaSubsampling = chromaSubsampling;

  // We unfortunately can't know which colorspace the video is using at this
  // stage.
  b.mYUVColorSpace =
      DefaultColorSpace({aFrame.mImageWidth(), aFrame.mImageHeight()});

  gfx::IntRect pictureRegion(0, 0, aFrame.mImageWidth(), aFrame.mImageHeight());
  RefPtr<VideoData> v = VideoData::CreateAndCopyData(
      mVideoInfo, mImageContainer, mLastStreamOffset,
      media::TimeUnit::FromMicroseconds(aFrame.mTimestamp()),
      media::TimeUnit::FromMicroseconds(aFrame.mDuration()), b, false,
      media::TimeUnit::FromMicroseconds(-1), pictureRegion, mKnowsCompositor);

  if (!v || !v->mImage) {
    NS_WARNING("Failed to decode video frame.");
    return v.forget();
  }

  // This is a DRM image.
  v->mImage->SetIsDRM(true);

  return v.forget();
}

ipc::IPCResult ChromiumCDMParent::RecvDecodeFailed(const uint32_t& aStatus) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::RecvDecodeFailed(this=%p status=%" PRIu32
                ")",
                this, aStatus);
  if (mIsShutdown) {
    MOZ_ASSERT(mDecodePromise.IsEmpty());
    return IPC_OK();
  }

  if (aStatus == cdm::kNeedMoreData) {
    mDecodePromise.ResolveIfExists(nsTArray<RefPtr<MediaData>>(), __func__);
    return IPC_OK();
  }

  mDecodePromise.RejectIfExists(
      MediaResult(
          NS_ERROR_DOM_MEDIA_FATAL_ERR,
          RESULT_DETAIL(
              "ChromiumCDMParent::RecvDecodeFailed with status %s (%" PRIu32
              ")",
              CdmStatusToString(aStatus), aStatus)),
      __func__);
  return IPC_OK();
}

ipc::IPCResult ChromiumCDMParent::RecvShutdown() {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::RecvShutdown(this=%p)", this);
  Shutdown();
  return IPC_OK();
}

void ChromiumCDMParent::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::ActorDestroy(this=%p, reason=%d)", this,
                aWhy);
  MOZ_ASSERT(!mActorDestroyed);
  mActorDestroyed = true;
  // Shutdown() will clear mCDMCallback, so let's keep a reference for later
  // use.
  auto callback = mCDMCallback;
  if (!mIsShutdown) {
    // Plugin crash.
    MOZ_ASSERT(aWhy == AbnormalShutdown);
    Shutdown();
  }
  MOZ_ASSERT(mIsShutdown);
  RefPtr<ChromiumCDMParent> kungFuDeathGrip(this);
  if (mContentParent) {
    mContentParent->ChromiumCDMDestroyed(this);
    mContentParent = nullptr;
  }
  mAbnormalShutdown = (aWhy == AbnormalShutdown);
  if (mAbnormalShutdown && callback) {
    callback->Terminated();
  }
  MaybeDisconnect(mAbnormalShutdown);
}

RefPtr<MediaDataDecoder::InitPromise> ChromiumCDMParent::InitializeVideoDecoder(
    const gmp::CDMVideoDecoderConfig& aConfig, const VideoInfo& aInfo,
    RefPtr<layers::ImageContainer> aImageContainer,
    RefPtr<layers::KnowsCompositor> aKnowsCompositor) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  if (mIsShutdown) {
    return MediaDataDecoder::InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("ChromiumCDMParent is shutdown")),
        __func__);
  }

  // The Widevine CDM version 1.4.8.970 and above contain a video decoder that
  // does not optimally allocate video frames; it requests buffers much larger
  // than required. The exact formula the CDM uses to calculate their frame
  // sizes isn't obvious, but they normally request around or slightly more
  // than 1.5X the optimal amount. So pad the size of buffers we allocate so
  // that we're likely to have buffers big enough to accomodate the CDM's weird
  // frame size calculation.
  const size_t bufferSize =
      1.7 * I420FrameBufferSizePadded(aInfo.mImage.width, aInfo.mImage.height);
  if (bufferSize <= 0) {
    return MediaDataDecoder::InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Video frame buffer size is invalid.")),
        __func__);
  }

  if (!EnsureSufficientShmems(bufferSize)) {
    return MediaDataDecoder::InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Failed to init shmems for video decoder")),
        __func__);
  }

  if (!SendInitializeVideoDecoder(aConfig)) {
    return MediaDataDecoder::InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Failed to send init video decoder to CDM")),
        __func__);
  }

  mMaxRefFrames = (aConfig.mCodec() == cdm::VideoCodec::kCodecH264)
                      ? H264::HasSPS(aInfo.mExtraData)
                            ? H264::ComputeMaxRefFrames(aInfo.mExtraData)
                            : 16
                      : 0;

  mVideoDecoderInitialized = true;
  mImageContainer = aImageContainer;
  mKnowsCompositor = aKnowsCompositor;
  mVideoInfo = aInfo;
  mVideoFrameBufferSize = bufferSize;

  return mInitVideoDecoderPromise.Ensure(__func__);
}

ipc::IPCResult ChromiumCDMParent::RecvOnDecoderInitDone(
    const uint32_t& aStatus) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG(
      "ChromiumCDMParent::RecvOnDecoderInitDone(this=%p, status=%" PRIu32 ")",
      this, aStatus);
  if (mIsShutdown) {
    MOZ_ASSERT(mInitVideoDecoderPromise.IsEmpty());
    return IPC_OK();
  }
  if (aStatus == static_cast<uint32_t>(cdm::kSuccess)) {
    mInitVideoDecoderPromise.ResolveIfExists(TrackInfo::kVideoTrack, __func__);
  } else {
    mVideoDecoderInitialized = false;
    mInitVideoDecoderPromise.RejectIfExists(
        MediaResult(
            NS_ERROR_DOM_MEDIA_FATAL_ERR,
            RESULT_DETAIL("CDM init decode failed with status %s (%" PRIu32 ")",
                          CdmStatusToString(aStatus), aStatus)),
        __func__);
  }
  return IPC_OK();
}

RefPtr<MediaDataDecoder::DecodePromise>
ChromiumCDMParent::DecryptAndDecodeFrame(MediaRawData* aSample) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  if (mIsShutdown) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("ChromiumCDMParent is shutdown")),
        __func__);
  }

  GMP_LOG_DEBUG("ChromiumCDMParent::DecryptAndDecodeFrame t=%" PRId64,
                aSample->mTime.ToMicroseconds());

  CDMInputBuffer buffer;

  if (!InitCDMInputBuffer(buffer, aSample)) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR, "Failed to init CDM buffer."),
        __func__);
  }

  mLastStreamOffset = aSample->mOffset;

  if (!SendDecryptAndDecodeFrame(buffer)) {
    GMP_LOG_DEBUG(
        "ChromiumCDMParent::Decrypt(this=%p) failed to send decrypt message.",
        this);
    DeallocShmem(buffer.mData());
    return MediaDataDecoder::DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    "Failed to send decrypt to CDM process."),
        __func__);
  }

  return mDecodePromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::FlushPromise> ChromiumCDMParent::FlushVideoDecoder() {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  if (mIsShutdown) {
    MOZ_ASSERT(mReorderQueue.IsEmpty());
    return MediaDataDecoder::FlushPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("ChromiumCDMParent is shutdown")),
        __func__);
  }

  mReorderQueue.Clear();

  mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  if (!SendResetVideoDecoder()) {
    return MediaDataDecoder::FlushPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    "Failed to send flush to CDM."),
        __func__);
  }
  return mFlushDecoderPromise.Ensure(__func__);
}

ipc::IPCResult ChromiumCDMParent::RecvResetVideoDecoderComplete() {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  MOZ_ASSERT(mReorderQueue.IsEmpty());
  if (mIsShutdown) {
    MOZ_ASSERT(mFlushDecoderPromise.IsEmpty());
    return IPC_OK();
  }
  mFlushDecoderPromise.ResolveIfExists(true, __func__);
  return IPC_OK();
}

RefPtr<MediaDataDecoder::DecodePromise> ChromiumCDMParent::Drain() {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  MOZ_ASSERT(mDecodePromise.IsEmpty(), "Must wait for decoding to complete");
  if (mIsShutdown) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("ChromiumCDMParent is shutdown")),
        __func__);
  }

  RefPtr<MediaDataDecoder::DecodePromise> p = mDecodePromise.Ensure(__func__);
  if (!SendDrain()) {
    mDecodePromise.Resolve(MediaDataDecoder::DecodedData(), __func__);
  }
  return p;
}

ipc::IPCResult ChromiumCDMParent::RecvDrainComplete() {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  if (mIsShutdown) {
    MOZ_ASSERT(mDecodePromise.IsEmpty());
    return IPC_OK();
  }

  MediaDataDecoder::DecodedData samples;
  while (!mReorderQueue.IsEmpty()) {
    samples.AppendElement(mReorderQueue.Pop());
  }

  mDecodePromise.ResolveIfExists(std::move(samples), __func__);
  return IPC_OK();
}
RefPtr<ShutdownPromise> ChromiumCDMParent::ShutdownVideoDecoder() {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  if (mIsShutdown || !mVideoDecoderInitialized) {
    return ShutdownPromise::CreateAndResolve(true, __func__);
  }
  mInitVideoDecoderPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED,
                                          __func__);
  mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  MOZ_ASSERT(mFlushDecoderPromise.IsEmpty());
  if (!SendDeinitializeVideoDecoder()) {
    return ShutdownPromise::CreateAndResolve(true, __func__);
  }
  mVideoDecoderInitialized = false;

  GMP_LOG_DEBUG("ChromiumCDMParent::~ShutdownVideoDecoder(this=%p) ", this);

  // The ChromiumCDMChild will purge its shmems, so if the decoder is
  // reinitialized the shmems need to be re-allocated, and they may need
  // to be a different size.
  mVideoShmemsActive = 0;
  mVideoFrameBufferSize = 0;
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

void ChromiumCDMParent::Shutdown() {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  GMP_LOG_DEBUG("ChromiumCDMParent::Shutdown(this=%p)", this);

  if (mIsShutdown) {
    return;
  }
  mIsShutdown = true;

  // If we're shutting down due to the plugin shutting down due to application
  // shutdown, we should tell the CDM proxy to also shutdown. Otherwise the
  // proxy will shutdown when the owning MediaKeys is destroyed during cycle
  // collection, and that will not shut down cleanly as the GMP thread will be
  // shutdown by then.
  if (mCDMCallback) {
    mCDMCallback->Shutdown();
  }

  // We may be called from a task holding the last reference to the CDM
  // callback, so let's clear our local weak pointer to ensure it will not be
  // used afterward (including from an already-queued task, e.g.: ActorDestroy).
  mCDMCallback = nullptr;

  mReorderQueue.Clear();

  for (RefPtr<DecryptJob>& decrypt : mDecrypts) {
    decrypt->PostResult(eme::AbortedErr);
  }
  mDecrypts.Clear();

  if (mVideoDecoderInitialized && !mActorDestroyed) {
    Unused << SendDeinitializeVideoDecoder();
    mVideoDecoderInitialized = false;
  }

  // Note: MediaKeys rejects all outstanding promises when it initiates
  // shutdown.
  mPromiseToCreateSessionToken.Clear();

  mInitPromise.RejectIfExists(
      MediaResult(NS_ERROR_DOM_ABORT_ERR,
                  RESULT_DETAIL("ChromiumCDMParent is shutdown")),
      __func__);

  mInitVideoDecoderPromise.RejectIfExists(
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("ChromiumCDMParent is shutdown")),
      __func__);
  mDecodePromise.RejectIfExists(
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("ChromiumCDMParent is shutdown")),
      __func__);
  mFlushDecoderPromise.RejectIfExists(
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("ChromiumCDMParent is shutdown")),
      __func__);

  if (!mActorDestroyed) {
    Unused << SendDestroy();
  }
}

}  // namespace mozilla::gmp

#undef NS_DispatchToMainThread
