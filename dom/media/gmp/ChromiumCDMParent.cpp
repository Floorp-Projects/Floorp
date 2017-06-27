/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMParent.h"

#include "ChromiumCDMProxy.h"
#include "content_decryption_module.h"
#include "GMPContentChild.h"
#include "GMPContentParent.h"
#include "GMPLog.h"
#include "GMPUtils.h"
#include "MediaPrefs.h"
#include "mozilla/dom/MediaKeyMessageEventBinding.h"
#include "mozilla/gmp/GMPTypes.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "mp4_demuxer/AnnexB.h"
#include "mp4_demuxer/H264.h"

namespace mozilla {
namespace gmp {

using namespace eme;

ChromiumCDMParent::ChromiumCDMParent(GMPContentParent* aContentParent,
                                     uint32_t aPluginId)
  : mPluginId(aPluginId)
  , mContentParent(aContentParent)
  , mVideoShmemLimit(MediaPrefs::EMEChromiumAPIVideoShmemCount())
{
  GMP_LOG(
    "ChromiumCDMParent::ChromiumCDMParent(this=%p, contentParent=%p, id=%u)",
    this,
    aContentParent,
    aPluginId);
}

bool
ChromiumCDMParent::Init(ChromiumCDMProxy* aProxy,
                        bool aAllowDistinctiveIdentifier,
                        bool aAllowPersistentState)
{
  GMP_LOG("ChromiumCDMParent::Init(this=%p)", this);
  if (!aProxy) {
    return false;
  }
  mProxy = aProxy;
  return SendInit(aAllowDistinctiveIdentifier, aAllowPersistentState);
}

void
ChromiumCDMParent::CreateSession(uint32_t aCreateSessionToken,
                                 uint32_t aSessionType,
                                 uint32_t aInitDataType,
                                 uint32_t aPromiseId,
                                 const nsTArray<uint8_t>& aInitData)
{
  GMP_LOG("ChromiumCDMParent::CreateSession(this=%p)", this);
  if (mIsShutdown) {
    RejectPromise(aPromiseId,
                  NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("CDM is shutdown."));
    return;
  }
  if (!SendCreateSessionAndGenerateRequest(
        aPromiseId, aSessionType, aInitDataType, aInitData)) {
    RejectPromise(
      aPromiseId,
      NS_ERROR_DOM_INVALID_STATE_ERR,
      NS_LITERAL_CSTRING("Failed to send generateRequest to CDM process."));
    return;
  }
  mPromiseToCreateSessionToken.Put(aPromiseId, aCreateSessionToken);
}

void
ChromiumCDMParent::LoadSession(uint32_t aPromiseId,
                               uint32_t aSessionType,
                               nsString aSessionId)
{
  GMP_LOG("ChromiumCDMParent::LoadSession(this=%p, pid=%u, type=%u, sid=%s)",
          this,
          aPromiseId,
          aSessionType,
          NS_ConvertUTF16toUTF8(aSessionId).get());
  if (mIsShutdown) {
    RejectPromise(aPromiseId,
                  NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("CDM is shutdown."));
    return;
  }
  if (!SendLoadSession(
        aPromiseId, aSessionType, NS_ConvertUTF16toUTF8(aSessionId))) {
    RejectPromise(
      aPromiseId,
      NS_ERROR_DOM_INVALID_STATE_ERR,
      NS_LITERAL_CSTRING("Failed to send loadSession to CDM process."));
    return;
  }
}

void
ChromiumCDMParent::SetServerCertificate(uint32_t aPromiseId,
                                        const nsTArray<uint8_t>& aCert)
{
  GMP_LOG("ChromiumCDMParent::SetServerCertificate(this=%p)", this);
  if (mIsShutdown) {
    RejectPromise(aPromiseId,
                  NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("CDM is shutdown."));
    return;
  }
  if (!SendSetServerCertificate(aPromiseId, aCert)) {
    RejectPromise(
      aPromiseId,
      NS_ERROR_DOM_INVALID_STATE_ERR,
      NS_LITERAL_CSTRING("Failed to send setServerCertificate to CDM process"));
  }
}

void
ChromiumCDMParent::UpdateSession(const nsCString& aSessionId,
                                 uint32_t aPromiseId,
                                 const nsTArray<uint8_t>& aResponse)
{
  GMP_LOG("ChromiumCDMParent::UpdateSession(this=%p)", this);
  if (mIsShutdown) {
    RejectPromise(aPromiseId,
                  NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("CDM is shutdown."));
    return;
  }
  if (!SendUpdateSession(aPromiseId, aSessionId, aResponse)) {
    RejectPromise(
      aPromiseId,
      NS_ERROR_DOM_INVALID_STATE_ERR,
      NS_LITERAL_CSTRING("Failed to send updateSession to CDM process"));
  }
}

void
ChromiumCDMParent::CloseSession(const nsCString& aSessionId,
                                uint32_t aPromiseId)
{
  GMP_LOG("ChromiumCDMParent::CloseSession(this=%p)", this);
  if (mIsShutdown) {
    RejectPromise(aPromiseId,
                  NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("CDM is shutdown."));
    return;
  }
  if (!SendCloseSession(aPromiseId, aSessionId)) {
    RejectPromise(
      aPromiseId,
      NS_ERROR_DOM_INVALID_STATE_ERR,
      NS_LITERAL_CSTRING("Failed to send closeSession to CDM process"));
  }
}

void
ChromiumCDMParent::RemoveSession(const nsCString& aSessionId,
                                 uint32_t aPromiseId)
{
  GMP_LOG("ChromiumCDMParent::RemoveSession(this=%p)", this);
  if (mIsShutdown) {
    RejectPromise(aPromiseId,
                  NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("CDM is shutdown."));
    return;
  }
  if (!SendRemoveSession(aPromiseId, aSessionId)) {
    RejectPromise(
      aPromiseId,
      NS_ERROR_DOM_INVALID_STATE_ERR,
      NS_LITERAL_CSTRING("Failed to send removeSession to CDM process"));
  }
}

bool
ChromiumCDMParent::InitCDMInputBuffer(gmp::CDMInputBuffer& aBuffer,
                                      MediaRawData* aSample)
{
  const CryptoSample& crypto = aSample->mCrypto;
  if (crypto.mEncryptedSizes.Length() != crypto.mPlainSizes.Length()) {
    GMP_LOG("InitCDMInputBuffer clear/cipher subsamples don't match");
    return false;
  }

  Shmem shmem;
  if (!AllocShmem(aSample->Size(), Shmem::SharedMemory::TYPE_BASIC, &shmem)) {
    return false;
  }
  memcpy(shmem.get<uint8_t>(), aSample->Data(), aSample->Size());

  aBuffer = gmp::CDMInputBuffer(shmem,
                                crypto.mKeyId,
                                crypto.mIV,
                                aSample->mTime.ToMicroseconds(),
                                aSample->mDuration.ToMicroseconds(),
                                crypto.mPlainSizes,
                                crypto.mEncryptedSizes,
                                crypto.mValid);
  return true;
}

bool
ChromiumCDMParent::SendBufferToCDM(uint32_t aSizeInBytes)
{
  Shmem shmem;
  if (!AllocShmem(aSizeInBytes, Shmem::SharedMemory::TYPE_BASIC, &shmem)) {
    return false;
  }
  if (!SendGiveBuffer(shmem)) {
    DeallocShmem(shmem);
    return false;
  }
  return true;
}

RefPtr<DecryptPromise>
ChromiumCDMParent::Decrypt(MediaRawData* aSample)
{
  if (mIsShutdown) {
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
    GMP_LOG(
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

ipc::IPCResult
ChromiumCDMParent::Recv__delete__()
{
  MOZ_ASSERT(mIsShutdown);
  GMP_LOG("ChromiumCDMParent::Recv__delete__(this=%p)", this);
  if (mContentParent) {
    mContentParent->ChromiumCDMDestroyed(this);
    mContentParent = nullptr;
  }
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvOnResolveNewSessionPromise(const uint32_t& aPromiseId,
                                                  const nsCString& aSessionId)
{
  GMP_LOG("ChromiumCDMParent::RecvOnResolveNewSessionPromise(this=%p, pid=%u, "
          "sid=%s)",
          this,
          aPromiseId,
          aSessionId.get());
  if (!mProxy || mIsShutdown) {
    return IPC_OK();
  }

  Maybe<uint32_t> token = mPromiseToCreateSessionToken.GetAndRemove(aPromiseId);
  if (token.isNothing()) {
    RejectPromise(aPromiseId,
                  NS_ERROR_DOM_INVALID_STATE_ERR,
                  NS_LITERAL_CSTRING("Lost session token for new session."));
    return IPC_OK();
  }

  RefPtr<Runnable> task =
    NewRunnableMethod<uint32_t, nsString>("ChromiumCDMProxy::OnSetSessionId",
                                          mProxy,
                                          &ChromiumCDMProxy::OnSetSessionId,
                                          token.value(),
                                          NS_ConvertUTF8toUTF16(aSessionId));
  NS_DispatchToMainThread(task);

  ResolvePromise(aPromiseId);

  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvResolveLoadSessionPromise(const uint32_t& aPromiseId,
                                                 const bool& aSuccessful)
{
  GMP_LOG("ChromiumCDMParent::RecvResolveLoadSessionPromise(this=%p, pid=%u, "
          "successful=%d)",
          this,
          aPromiseId,
          aSuccessful);
  if (!mProxy || mIsShutdown) {
    return IPC_OK();
  }

  NS_DispatchToMainThread(NewRunnableMethod<uint32_t, bool>(
    "ChromiumCDMProxy::OnResolveLoadSessionPromise",
    mProxy,
    &ChromiumCDMProxy::OnResolveLoadSessionPromise,
    aPromiseId,
    aSuccessful));
  return IPC_OK();
}
void
ChromiumCDMParent::ResolvePromise(uint32_t aPromiseId)
{
  GMP_LOG(
    "ChromiumCDMParent::ResolvePromise(this=%p, pid=%u)", this, aPromiseId);

  // Note: The MediaKeys rejects all pending DOM promises when it
  // initiates shutdown.
  if (!mProxy || mIsShutdown) {
    return;
  }
  NS_DispatchToMainThread(
    NewRunnableMethod<uint32_t>("ChromiumCDMProxy::ResolvePromise",
                                mProxy,
                                &ChromiumCDMProxy::ResolvePromise,
                                aPromiseId));
}

ipc::IPCResult
ChromiumCDMParent::RecvOnResolvePromise(const uint32_t& aPromiseId)
{
  ResolvePromise(aPromiseId);
  return IPC_OK();
}

static nsresult
ToNsresult(uint32_t aError)
{
  switch (static_cast<cdm::Error>(aError)) {
    case cdm::kNotSupportedError:
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    case cdm::kInvalidStateError:
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    case cdm::kInvalidAccessError:
      // Note: Chrome converts kInvalidAccessError to TypeError, since the
      // Chromium CDM API doesn't have a type error enum value. The EME spec
      // requires TypeError in some places, so we do the same conversion.
      // See bug 1313202.
      return NS_ERROR_DOM_TYPE_ERR;
    case cdm::kQuotaExceededError:
      return NS_ERROR_DOM_QUOTA_EXCEEDED_ERR;
    case cdm::kUnknownError:
      return NS_ERROR_DOM_UNKNOWN_ERR; // Note: Unique placeholder.
    case cdm::kClientError:
      return NS_ERROR_DOM_ABORT_ERR; // Note: Unique placeholder.
    case cdm::kOutputError:
      return NS_ERROR_DOM_SECURITY_ERR; // Note: Unique placeholder.
  };
  MOZ_ASSERT_UNREACHABLE("Invalid cdm::Error enum value.");
  return NS_ERROR_DOM_TIMEOUT_ERR; // Note: Unique placeholder.
}

void
ChromiumCDMParent::RejectPromise(uint32_t aPromiseId,
                                 nsresult aError,
                                 const nsCString& aErrorMessage)
{
  GMP_LOG(
    "ChromiumCDMParent::RejectPromise(this=%p, pid=%u)", this, aPromiseId);
  // Note: The MediaKeys rejects all pending DOM promises when it
  // initiates shutdown.
  if (!mProxy || mIsShutdown) {
    return;
  }
  NS_DispatchToMainThread(NewRunnableMethod<uint32_t, nsresult, nsCString>(
    "ChromiumCDMProxy::RejectPromise",
    mProxy,
    &ChromiumCDMProxy::RejectPromise,
    aPromiseId,
    aError,
    aErrorMessage));
}

ipc::IPCResult
ChromiumCDMParent::RecvOnRejectPromise(const uint32_t& aPromiseId,
                                       const uint32_t& aError,
                                       const uint32_t& aSystemCode,
                                       const nsCString& aErrorMessage)
{
  RejectPromise(aPromiseId, ToNsresult(aError), aErrorMessage);
  return IPC_OK();
}

static dom::MediaKeyMessageType
ToDOMMessageType(uint32_t aMessageType)
{
  switch (static_cast<cdm::MessageType>(aMessageType)) {
    case cdm::kLicenseRequest:
      return dom::MediaKeyMessageType::License_request;
    case cdm::kLicenseRenewal:
      return dom::MediaKeyMessageType::License_renewal;
    case cdm::kLicenseRelease:
      return dom::MediaKeyMessageType::License_release;
  }
  MOZ_ASSERT_UNREACHABLE("Invalid cdm::MessageType enum value.");
  return dom::MediaKeyMessageType::License_request;
}

ipc::IPCResult
ChromiumCDMParent::RecvOnSessionMessage(const nsCString& aSessionId,
                                        const uint32_t& aMessageType,
                                        nsTArray<uint8_t>&& aMessage)
{
  GMP_LOG("ChromiumCDMParent::RecvOnSessionMessage(this=%p, sid=%s)",
          this,
          aSessionId.get());
  if (!mProxy || mIsShutdown) {
    return IPC_OK();
  }
  RefPtr<CDMProxy> proxy = mProxy;
  nsString sid = NS_ConvertUTF8toUTF16(aSessionId);
  dom::MediaKeyMessageType messageType = ToDOMMessageType(aMessageType);
  nsTArray<uint8_t> msg(Move(aMessage));
  NS_DispatchToMainThread(
    NS_NewRunnableFunction("gmp::ChromiumCDMParent::RecvOnSessionMessage",
                           [proxy, sid, messageType, msg]() mutable {
                             proxy->OnSessionMessage(sid, messageType, msg);
                           }));
  return IPC_OK();
}

static dom::MediaKeyStatus
ToDOMMediaKeyStatus(uint32_t aStatus)
{
  switch (static_cast<cdm::KeyStatus>(aStatus)) {
    case cdm::kUsable:
      return dom::MediaKeyStatus::Usable;
    case cdm::kInternalError:
      return dom::MediaKeyStatus::Internal_error;
    case cdm::kExpired:
      return dom::MediaKeyStatus::Expired;
    case cdm::kOutputRestricted:
      return dom::MediaKeyStatus::Output_restricted;
    case cdm::kOutputDownscaled:
      return dom::MediaKeyStatus::Output_downscaled;
    case cdm::kStatusPending:
      return dom::MediaKeyStatus::Status_pending;
    case cdm::kReleased:
      return dom::MediaKeyStatus::Released;
  }
  MOZ_ASSERT_UNREACHABLE("Invalid cdm::KeyStatus enum value.");
  return dom::MediaKeyStatus::Internal_error;
}

ipc::IPCResult
ChromiumCDMParent::RecvOnSessionKeysChange(
  const nsCString& aSessionId,
  nsTArray<CDMKeyInformation>&& aKeysInfo)
{
  GMP_LOG("ChromiumCDMParent::RecvOnSessionKeysChange(this=%p)", this);
  if (!mProxy || mIsShutdown) {
    return IPC_OK();
  }
  bool keyStatusesChange = false;
  {
    CDMCaps::AutoLock caps(mProxy->Capabilites());
    for (size_t i = 0; i < aKeysInfo.Length(); i++) {
      keyStatusesChange |=
        caps.SetKeyStatus(aKeysInfo[i].mKeyId(),
                          NS_ConvertUTF8toUTF16(aSessionId),
                          dom::Optional<dom::MediaKeyStatus>(
                            ToDOMMediaKeyStatus(aKeysInfo[i].mStatus())));
    }
  }
  if (keyStatusesChange) {
    NS_DispatchToMainThread(
      NewRunnableMethod<nsString>("ChromiumCDMProxy::OnKeyStatusesChange",
                                  mProxy,
                                  &ChromiumCDMProxy::OnKeyStatusesChange,
                                  NS_ConvertUTF8toUTF16(aSessionId)));
  }
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvOnExpirationChange(const nsCString& aSessionId,
                                          const double& aSecondsSinceEpoch)
{
  GMP_LOG("ChromiumCDMParent::RecvOnExpirationChange(this=%p) time=%lf",
          this,
          aSecondsSinceEpoch);
  if (!mProxy || mIsShutdown) {
    return IPC_OK();
  }
  NS_DispatchToMainThread(NewRunnableMethod<nsString, UnixTime>(
    "ChromiumCDMProxy::OnExpirationChange",
    mProxy,
    &ChromiumCDMProxy::OnExpirationChange,
    NS_ConvertUTF8toUTF16(aSessionId),
    GMPTimestamp(aSecondsSinceEpoch * 1000)));
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvOnSessionClosed(const nsCString& aSessionId)
{
  GMP_LOG("ChromiumCDMParent::RecvOnSessionClosed(this=%p)", this);
  if (!mProxy || mIsShutdown) {
    return IPC_OK();
  }
  NS_DispatchToMainThread(
    NewRunnableMethod<nsString>("ChromiumCDMProxy::OnSessionClosed",
                                mProxy,
                                &ChromiumCDMProxy::OnSessionClosed,
                                NS_ConvertUTF8toUTF16(aSessionId)));
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvOnLegacySessionError(const nsCString& aSessionId,
                                            const uint32_t& aError,
                                            const uint32_t& aSystemCode,
                                            const nsCString& aMessage)
{
  GMP_LOG("ChromiumCDMParent::RecvOnLegacySessionError(this=%p)", this);
  if (!mProxy || mIsShutdown) {
    return IPC_OK();
  }
  NS_DispatchToMainThread(
    NewRunnableMethod<nsString, nsresult, uint32_t, nsString>(
      "ChromiumCDMProxy::OnSessionError",
      mProxy,
      &ChromiumCDMProxy::OnSessionError,
      NS_ConvertUTF8toUTF16(aSessionId),
      ToNsresult(aError),
      aSystemCode,
      NS_ConvertUTF8toUTF16(aMessage)));
  return IPC_OK();
}

DecryptStatus
ToDecryptStatus(uint32_t aError)
{
  switch (static_cast<cdm::Status>(aError)) {
    case cdm::kSuccess:
      return DecryptStatus::Ok;
    case cdm::kNoKey:
      return DecryptStatus::NoKeyErr;
    default:
      return DecryptStatus::GenericErr;
  }
}

ipc::IPCResult
ChromiumCDMParent::RecvDecryptFailed(const uint32_t& aId,
                                     const uint32_t& aStatus)
{
  GMP_LOG("ChromiumCDMParent::RecvDecryptFailed(this=%p, id=%u, status=%u)",
          this,
          aId,
          aStatus);

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

ipc::IPCResult
ChromiumCDMParent::RecvDecrypted(const uint32_t& aId,
                                 const uint32_t& aStatus,
                                 ipc::Shmem&& aShmem)
{
  GMP_LOG("ChromiumCDMParent::RecvDecrypted(this=%p, id=%u, status=%u)",
          this,
          aId,
          aStatus);

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
        MakeSpan<const uint8_t>(aShmem.get<uint8_t>(), aShmem.Size<uint8_t>()));
      mDecrypts.RemoveElementAt(i);
      break;
    }
  }
  return IPC_OK();
}

bool
ChromiumCDMParent::PurgeShmems()
{
  GMP_LOG("ChromiumCDMParent::PurgeShmems(this=%p) frame_size=%" PRIuSIZE
          " limit=%" PRIu32 " active=%" PRIu32,
          this,
          mVideoFrameBufferSize,
          mVideoShmemLimit,
          mVideoShmemsActive);

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

bool
ChromiumCDMParent::EnsureSufficientShmems(size_t aVideoFrameSize)
{
  GMP_LOG("ChromiumCDMParent::EnsureSufficientShmems(this=%p) "
          "size=%" PRIuSIZE " expected_size=%" PRIuSIZE " limit=%" PRIu32
          " active=%" PRIu32,
          this,
          aVideoFrameSize,
          mVideoFrameBufferSize,
          mVideoShmemLimit,
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
  // in an nsTArray<uint8_t> instead of a shmem. Every time this happens,
  // the parent sends an extra shmem to the CDM process for it to add to the
  // set of shmems with which to return output. Via this mechanism we should
  // recover from incorrectly predicting how many shmems to pre-allocate.
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
  } else {
    // Put an upper limit on the number of shmems we tolerate the CDM asking
    // for, to prevent a memory blow-out. In practice, we expect the CDM to
    // need less than 5, but some encodings require more.
    // We'd expect CDMs to not have video frames larger than 720p-1080p
    // (due to DRM robustness requirements), which is about 1.5MB-3MB per
    // frame.
    if (mVideoShmemLimit > 50) {
      return false;
    }
    mVideoShmemLimit++;
  }

  while (mVideoShmemsActive < mVideoShmemLimit) {
    if (!SendBufferToCDM(mVideoFrameBufferSize)) {
      return false;
    }
    mVideoShmemsActive++;
  }

  mMaxVideoShmemsActive =
    std::max(mMaxVideoShmemsActive, mVideoShmemsActive);

  return true;
}

ipc::IPCResult
ChromiumCDMParent::RecvDecodedData(const CDMVideoFrame& aFrame,
                                   nsTArray<uint8_t>&& aData)
{
  GMP_LOG("ChromiumCDMParent::RecvDecodedData(this=%p)", this);

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

  ReorderAndReturnOutput(Move(v));

  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvDecodedShmem(const CDMVideoFrame& aFrame,
                                    ipc::Shmem&& aShmem)
{
  GMP_LOG("ChromiumCDMParent::RecvDecodedShmem(this=%p) time=%" PRId64
          " duration=%" PRId64,
          this,
          aFrame.mTimestamp(),
          aFrame.mDuration());

  // On failure we need to deallocate the shmem we're to return to the
  // CDM. On success we return it to the CDM to be reused.
  auto autoDeallocateShmem =
    MakeScopeExit([&, this] { this->DeallocShmem(aShmem); });

  if (mIsShutdown || mDecodePromise.IsEmpty()) {
    return IPC_OK();
  }

  RefPtr<VideoData> v = CreateVideoFrame(
    aFrame, MakeSpan<uint8_t>(aShmem.get<uint8_t>(), aShmem.Size<uint8_t>()));
  if (!v) {
    mDecodePromise.RejectIfExists(
      MediaResult(NS_ERROR_OUT_OF_MEMORY,
                  RESULT_DETAIL("Can't create VideoData")),
      __func__);
    return IPC_OK();
  }

  // Return the shmem to the CDM so the shmem can be reused to send us
  // another frame.
  if (!SendGiveBuffer(aShmem)) {
    mDecodePromise.RejectIfExists(
      MediaResult(NS_ERROR_OUT_OF_MEMORY,
                  RESULT_DETAIL("Can't return shmem to CDM process")),
      __func__);
    return IPC_OK();
  }

  // Don't need to deallocate the shmem since the CDM process is responsible
  // for it again.
  autoDeallocateShmem.release();

  ReorderAndReturnOutput(Move(v));

  return IPC_OK();
}

void
ChromiumCDMParent::ReorderAndReturnOutput(RefPtr<VideoData>&& aFrame)
{
  if (mMaxRefFrames == 0) {
    mDecodePromise.ResolveIfExists({ Move(aFrame) }, __func__);
    return;
  }
  mReorderQueue.Push(Move(aFrame));
  MediaDataDecoder::DecodedData results;
  while (mReorderQueue.Length() > mMaxRefFrames) {
    results.AppendElement(mReorderQueue.Pop());
  }
  mDecodePromise.Resolve(Move(results), __func__);
}

already_AddRefed<VideoData>
ChromiumCDMParent::CreateVideoFrame(const CDMVideoFrame& aFrame,
                                    Span<uint8_t> aData)
{
  VideoData::YCbCrBuffer b;
  MOZ_ASSERT(aData.Length() > 0);

  b.mPlanes[0].mData = aData.Elements();
  b.mPlanes[0].mWidth = aFrame.mImageWidth();
  b.mPlanes[0].mHeight = aFrame.mImageHeight();
  b.mPlanes[0].mStride = aFrame.mYPlane().mStride();
  b.mPlanes[0].mOffset = aFrame.mYPlane().mPlaneOffset();
  b.mPlanes[0].mSkip = 0;

  b.mPlanes[1].mData = aData.Elements();
  b.mPlanes[1].mWidth = (aFrame.mImageWidth() + 1) / 2;
  b.mPlanes[1].mHeight = (aFrame.mImageHeight() + 1) / 2;
  b.mPlanes[1].mStride = aFrame.mUPlane().mStride();
  b.mPlanes[1].mOffset = aFrame.mUPlane().mPlaneOffset();
  b.mPlanes[1].mSkip = 0;

  b.mPlanes[2].mData = aData.Elements();
  b.mPlanes[2].mWidth = (aFrame.mImageWidth() + 1) / 2;
  b.mPlanes[2].mHeight = (aFrame.mImageHeight() + 1) / 2;
  b.mPlanes[2].mStride = aFrame.mVPlane().mStride();
  b.mPlanes[2].mOffset = aFrame.mVPlane().mPlaneOffset();
  b.mPlanes[2].mSkip = 0;

  gfx::IntRect pictureRegion(0, 0, aFrame.mImageWidth(), aFrame.mImageHeight());
  RefPtr<VideoData> v = VideoData::CreateAndCopyData(
    mVideoInfo,
    mImageContainer,
    mLastStreamOffset,
    media::TimeUnit::FromMicroseconds(aFrame.mTimestamp()),
    media::TimeUnit::FromMicroseconds(aFrame.mDuration()),
    b,
    false,
    media::TimeUnit::FromMicroseconds(-1),
    pictureRegion);

  return v.forget();
}

ipc::IPCResult
ChromiumCDMParent::RecvDecodeFailed(const uint32_t& aStatus)
{
  if (mIsShutdown) {
    MOZ_ASSERT(mDecodePromise.IsEmpty());
    return IPC_OK();
  }

  if (aStatus == cdm::kNeedMoreData) {
    mDecodePromise.ResolveIfExists(nsTArray<RefPtr<MediaData>>(), __func__);
    return IPC_OK();
  }

  mDecodePromise.RejectIfExists(
    MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                RESULT_DETAIL("ChromiumCDMParent::RecvDecodeFailed")),
    __func__);
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvShutdown()
{
  GMP_LOG("ChromiumCDMParent::RecvShutdown(this=%p)", this);
  Shutdown();
  return IPC_OK();
}

void
ChromiumCDMParent::ActorDestroy(ActorDestroyReason aWhy)
{
  GMP_LOG("ChromiumCDMParent::ActorDestroy(this=%p, reason=%d)", this, aWhy);
  MOZ_ASSERT(!mActorDestroyed);
  mActorDestroyed = true;
  // Shutdown() will clear mProxy, so let's keep a reference for later use.
  RefPtr<ChromiumCDMProxy> proxy = mProxy;
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
  bool abnormalShutdown = (aWhy == AbnormalShutdown);
  if (abnormalShutdown && proxy) {
    RefPtr<Runnable> task = NewRunnableMethod(
      "ChromiumCDMProxy::Terminated", proxy, &ChromiumCDMProxy::Terminated);
    NS_DispatchToMainThread(task);
  }
  MaybeDisconnect(abnormalShutdown);
}

RefPtr<MediaDataDecoder::InitPromise>
ChromiumCDMParent::InitializeVideoDecoder(
  const gmp::CDMVideoDecoderConfig& aConfig,
  const VideoInfo& aInfo,
  RefPtr<layers::ImageContainer> aImageContainer)
{
  if (mIsShutdown) {
    return MediaDataDecoder::InitPromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("ChromiumCDMParent is shutdown")),
      __func__);
  }

  const size_t bufferSize =
    I420FrameBufferSizePadded(aInfo.mImage.width, aInfo.mImage.height);
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

  mMaxRefFrames =
    (aConfig.mCodec() == cdm::VideoDecoderConfig::kCodecH264)
      ? mp4_demuxer::H264::HasSPS(aInfo.mExtraData)
          ? mp4_demuxer::H264::ComputeMaxRefFrames(aInfo.mExtraData)
          : 16
      : 0;

  mVideoDecoderInitialized = true;
  mImageContainer = aImageContainer;
  mVideoInfo = aInfo;
  mVideoFrameBufferSize = bufferSize;

  return mInitVideoDecoderPromise.Ensure(__func__);
}

ipc::IPCResult
ChromiumCDMParent::RecvOnDecoderInitDone(const uint32_t& aStatus)
{
  GMP_LOG("ChromiumCDMParent::RecvOnDecoderInitDone(this=%p, status=%u)",
          this,
          aStatus);
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
        RESULT_DETAIL("CDM init decode failed with %" PRIu32, aStatus)),
      __func__);
  }
  return IPC_OK();
}

RefPtr<MediaDataDecoder::DecodePromise>
ChromiumCDMParent::DecryptAndDecodeFrame(MediaRawData* aSample)
{
  if (mIsShutdown) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("ChromiumCDMParent is shutdown")),
      __func__);
  }

  GMP_LOG("ChromiumCDMParent::DecryptAndDecodeFrame t=%" PRId64,
          aSample->mTime.ToMicroseconds());

  CDMInputBuffer buffer;

  if (!InitCDMInputBuffer(buffer, aSample)) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR, "Failed to init CDM buffer."),
      __func__);
  }

  mLastStreamOffset = aSample->mOffset;

  if (!SendDecryptAndDecodeFrame(buffer)) {
    GMP_LOG(
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

RefPtr<MediaDataDecoder::FlushPromise>
ChromiumCDMParent::FlushVideoDecoder()
{
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
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR, "Failed to send flush to CDM."),
      __func__);
  }
  return mFlushDecoderPromise.Ensure(__func__);
}

ipc::IPCResult
ChromiumCDMParent::RecvResetVideoDecoderComplete()
{
  MOZ_ASSERT(mReorderQueue.IsEmpty());
  if (mIsShutdown) {
    MOZ_ASSERT(mFlushDecoderPromise.IsEmpty());
    return IPC_OK();
  }
  mFlushDecoderPromise.ResolveIfExists(true, __func__);
  return IPC_OK();
}

RefPtr<MediaDataDecoder::DecodePromise>
ChromiumCDMParent::Drain()
{
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

ipc::IPCResult
ChromiumCDMParent::RecvDrainComplete()
{
  if (mIsShutdown) {
    MOZ_ASSERT(mDecodePromise.IsEmpty());
    return IPC_OK();
  }

  MediaDataDecoder::DecodedData samples;
  while (!mReorderQueue.IsEmpty()) {
    samples.AppendElement(Move(mReorderQueue.Pop()));
  }

  mDecodePromise.ResolveIfExists(Move(samples), __func__);
  return IPC_OK();
}
RefPtr<ShutdownPromise>
ChromiumCDMParent::ShutdownVideoDecoder()
{
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

  GMP_LOG("ChromiumCDMParent::~ShutdownVideoDecoder(this=%p) "
          "VIDEO_CHROMIUM_CDM_MAX_SHMEMS=%u",
          this,
          mMaxVideoShmemsActive);
  Telemetry::Accumulate(Telemetry::HistogramID::VIDEO_CHROMIUM_CDM_MAX_SHMEMS,
                        mMaxVideoShmemsActive);

  // The ChromiumCDMChild will purge its shmems, so if the decoder is
  // reinitialized the shmems need to be re-allocated, and they may need
  // to be a different size.
  mVideoShmemsActive = 0;
  mVideoFrameBufferSize = 0;
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

void
ChromiumCDMParent::Shutdown()
{
  GMP_LOG("ChromiumCDMParent::Shutdown(this=%p)", this);

  if (mIsShutdown) {
    return;
  }
  mIsShutdown = true;

  // If we're shutting down due to the plugin shutting down due to application
  // shutdown, we should tell the CDM proxy to also shutdown. Otherwise the
  // proxy will shutdown when the owning MediaKeys is destroyed during cycle
  // collection, and that will not shut down cleanly as the GMP thread will be
  // shutdown by then.
  if (mProxy) {
    RefPtr<Runnable> task = NewRunnableMethod(
      "ChromiumCDMProxy::Shutdown", mProxy, &ChromiumCDMProxy::Shutdown);
    NS_DispatchToMainThread(task.forget());
  }

  // We may be called from a task holding the last reference to the proxy, so
  // let's clear our local weak pointer to ensure it will not be used afterward
  // (including from an already-queued task, e.g.: ActorDestroy).
  mProxy = nullptr;

  mReorderQueue.Clear();

  for (RefPtr<DecryptJob>& decrypt : mDecrypts) {
    decrypt->PostResult(eme::AbortedErr);
  }
  mDecrypts.Clear();

  if (mVideoDecoderInitialized && !mActorDestroyed) {
    Unused << SendDeinitializeVideoDecoder();
    mVideoDecoderInitialized = false;
  }

  // Note: MediaKeys rejects all outstanding promises when it initiates shutdown.
  mPromiseToCreateSessionToken.Clear();

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

} // namespace gmp
} // namespace mozilla
