/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMParent.h"
#include "mozilla/gmp/GMPTypes.h"
#include "GMPContentChild.h"
#include "mozilla/Unused.h"
#include "ChromiumCDMProxy.h"
#include "mozilla/dom/MediaKeyMessageEventBinding.h"
#include "content_decryption_module.h"
#include "GMPLog.h"

namespace mozilla {
namespace gmp {

ChromiumCDMParent::ChromiumCDMParent(GMPContentParent* aContentParent,
                                     uint32_t aPluginId)
  : mPluginId(aPluginId)
  , mContentParent(aContentParent)
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
ChromiumCDMParent::SetServerCertificate(uint32_t aPromiseId,
                                        const nsTArray<uint8_t>& aCert)
{
  GMP_LOG("ChromiumCDMParent::SetServerCertificate(this=%p)", this);
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
  if (!SendRemoveSession(aPromiseId, aSessionId)) {
    RejectPromise(
      aPromiseId,
      NS_ERROR_DOM_INVALID_STATE_ERR,
      NS_LITERAL_CSTRING("Failed to send removeSession to CDM process"));
  }
}

static bool
InitCDMInputBuffer(gmp::CDMInputBuffer& aBuffer, MediaRawData* aSample)
{
  const CryptoSample& crypto = aSample->mCrypto;
  if (crypto.mEncryptedSizes.Length() != crypto.mPlainSizes.Length()) {
    GMP_LOG("InitCDMInputBuffer clear/cipher subsamples don't match");
    return false;
  }

  nsTArray<uint8_t> data;
  data.AppendElements(aSample->Data(), aSample->Size());

  aBuffer = gmp::CDMInputBuffer(data,
                                crypto.mKeyId,
                                crypto.mIV,
                                aSample->mTime,
                                aSample->mDuration,
                                crypto.mPlainSizes,
                                crypto.mEncryptedSizes,
                                crypto.mValid);
  return true;
}

RefPtr<DecryptPromise>
ChromiumCDMParent::Decrypt(MediaRawData* aSample)
{
  CDMInputBuffer buffer;
  if (!InitCDMInputBuffer(buffer, aSample)) {
    return DecryptPromise::CreateAndReject(DecryptResult(GenericErr, aSample),
                                           __func__);
  }
  RefPtr<DecryptJob> job = new DecryptJob(aSample);
  if (!SendDecrypt(job->mId, buffer)) {
    GMP_LOG(
      "ChromiumCDMParent::Decrypt(this=%p) failed to send decrypt message",
      this);
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
  GMP_LOG("ChromiumCDMParent::Recv__delete__(this=%p)", this);
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
  if (!mProxy) {
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
    NewRunnableMethod<uint32_t, nsString>(mProxy,
                                          &ChromiumCDMProxy::OnSetSessionId,
                                          token.value(),
                                          NS_ConvertUTF8toUTF16(aSessionId));
  NS_DispatchToMainThread(task);

  ResolvePromise(aPromiseId);

  return IPC_OK();
}

void
ChromiumCDMParent::ResolvePromise(uint32_t aPromiseId)
{
  GMP_LOG(
    "ChromiumCDMParent::ResolvePromise(this=%p, pid=%u)", this, aPromiseId);

  if (!mProxy) {
    return;
  }
  NS_DispatchToMainThread(NewRunnableMethod<uint32_t>(
    mProxy, &ChromiumCDMProxy::ResolvePromise, aPromiseId));
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
  if (!mProxy) {
    return;
  }
  NS_DispatchToMainThread(NewRunnableMethod<uint32_t, nsresult, nsCString>(
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
  if (!mProxy) {
    return IPC_OK();
  }
  RefPtr<CDMProxy> proxy = mProxy;
  nsString sid = NS_ConvertUTF8toUTF16(aSessionId);
  dom::MediaKeyMessageType messageType = ToDOMMessageType(aMessageType);
  nsTArray<uint8_t> msg(Move(aMessage));
  NS_DispatchToMainThread(
    NS_NewRunnableFunction([proxy, sid, messageType, msg]() mutable {
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
  if (!mProxy) {
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
      NewRunnableMethod<nsString>(mProxy,
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
  if (!mProxy) {
    return IPC_OK();
  }
  NS_DispatchToMainThread(NewRunnableMethod<nsString, UnixTime>(
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
  if (!mProxy) {
    return IPC_OK();
  }
  NS_DispatchToMainThread(
    NewRunnableMethod<nsString>(mProxy,
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
  if (!mProxy) {
    return IPC_OK();
  }
  NS_DispatchToMainThread(
    NewRunnableMethod<nsString, nsresult, uint32_t, nsString>(
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
ChromiumCDMParent::RecvDecrypted(const uint32_t& aId,
                                 const uint32_t& aStatus,
                                 nsTArray<uint8_t>&& aData)
{
  GMP_LOG("ChromiumCDMParent::RecvDecrypted(this=%p, id=%u, status=%u)",
          this,
          aId,
          aStatus);
  for (size_t i = 0; i < mDecrypts.Length(); i++) {
    if (mDecrypts[i]->mId == aId) {
      mDecrypts[i]->PostResult(ToDecryptStatus(aStatus), aData);
      mDecrypts.RemoveElementAt(i);
      break;
    }
  }
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvDecoded(const CDMVideoFrame& aFrame)
{
  if (mDecodePromise.IsEmpty()) {
    return IPC_OK();
  }
  VideoData::YCbCrBuffer b;
  nsTArray<uint8_t> data;
  data = aFrame.mData();

  if (data.IsEmpty()) {
    mDecodePromise.ResolveIfExists(nsTArray<RefPtr<MediaData>>(), __func__);
    return IPC_OK();
  }

  b.mPlanes[0].mData = data.Elements();
  b.mPlanes[0].mWidth = aFrame.mImageWidth();
  b.mPlanes[0].mHeight = aFrame.mImageHeight();
  b.mPlanes[0].mStride = aFrame.mYPlane().mStride();
  b.mPlanes[0].mOffset = aFrame.mYPlane().mPlaneOffset();
  b.mPlanes[0].mSkip = 0;

  b.mPlanes[1].mData = data.Elements();
  b.mPlanes[1].mWidth = (aFrame.mImageWidth() + 1) / 2;
  b.mPlanes[1].mHeight = (aFrame.mImageHeight() + 1) / 2;
  b.mPlanes[1].mStride = aFrame.mUPlane().mStride();
  b.mPlanes[1].mOffset = aFrame.mUPlane().mPlaneOffset();
  b.mPlanes[1].mSkip = 0;

  b.mPlanes[2].mData = data.Elements();
  b.mPlanes[2].mWidth = (aFrame.mImageWidth() + 1) / 2;
  b.mPlanes[2].mHeight = (aFrame.mImageHeight() + 1) / 2;
  b.mPlanes[2].mStride = aFrame.mVPlane().mStride();
  b.mPlanes[2].mOffset = aFrame.mVPlane().mPlaneOffset();
  b.mPlanes[2].mSkip = 0;

  gfx::IntRect pictureRegion(0, 0, aFrame.mImageWidth(), aFrame.mImageHeight());
  RefPtr<VideoData> v = VideoData::CreateAndCopyData(mVideoInfo,
                                                     mImageContainer,
                                                     mLastStreamOffset,
                                                     aFrame.mTimestamp(),
                                                     aFrame.mDuration(),
                                                     b,
                                                     false,
                                                     -1,
                                                     pictureRegion);

  RefPtr<ChromiumCDMParent> self = this;
  if (v) {
    mDecodePromise.ResolveIfExists({ Move(v) }, __func__);
  } else {
    mDecodePromise.RejectIfExists(
      MediaResult(NS_ERROR_OUT_OF_MEMORY,
                  RESULT_DETAIL("CallBack::CreateAndCopyData")),
      __func__);
  }

  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvDecodeFailed(const uint32_t& aStatus)
{
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
  // TODO: SendDestroy(), call Terminated.
  return IPC_OK();
}

void
ChromiumCDMParent::ActorDestroy(ActorDestroyReason aWhy)
{
  GMP_LOG("ChromiumCDMParent::ActorDestroy(this=%p, reason=%d)", this, aWhy);
}

RefPtr<MediaDataDecoder::InitPromise>
ChromiumCDMParent::InitializeVideoDecoder(
  const gmp::CDMVideoDecoderConfig& aConfig,
  const VideoInfo& aInfo,
  RefPtr<layers::ImageContainer> aImageContainer)
{
  if (!SendInitializeVideoDecoder(aConfig)) {
    return MediaDataDecoder::InitPromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("Failed to send init video decoder to CDM")),
      __func__);
  }

  mImageContainer = aImageContainer;
  mVideoInfo = aInfo;

  return mInitVideoDecoderPromise.Ensure(__func__);
}

ipc::IPCResult
ChromiumCDMParent::RecvOnDecoderInitDone(const uint32_t& aStatus)
{
  GMP_LOG("ChromiumCDMParent::RecvOnDecoderInitDone(this=%p, status=%u)",
          this,
          aStatus);
  if (aStatus == static_cast<uint32_t>(cdm::kSuccess)) {
    mInitVideoDecoderPromise.ResolveIfExists(TrackInfo::kVideoTrack, __func__);
  } else {
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
  mFlushDecoderPromise.Resolve(true, __func__);
  return IPC_OK();
}

RefPtr<MediaDataDecoder::DecodePromise>
ChromiumCDMParent::Drain()
{
  MOZ_ASSERT(mDecodePromise.IsEmpty(), "Must wait for decoding to complete");

  RefPtr<MediaDataDecoder::DecodePromise> p = mDecodePromise.Ensure(__func__);
  if (!SendDrain()) {
    mDecodePromise.Resolve(MediaDataDecoder::DecodedData(), __func__);
  }
  return p;
}

ipc::IPCResult
ChromiumCDMParent::RecvDrainComplete()
{
  mDecodePromise.ResolveIfExists(MediaDataDecoder::DecodedData(), __func__);
  return IPC_OK();
}
} // namespace gmp
} // namespace mozilla
