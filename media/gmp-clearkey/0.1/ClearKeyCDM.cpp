/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClearKeyCDM.h"

#include "ClearKeyUtils.h"

using namespace cdm;

ClearKeyCDM::ClearKeyCDM(Host_10* aHost) {
  mHost = aHost;
  mSessionManager = new ClearKeySessionManager(mHost);
}

void ClearKeyCDM::Initialize(bool aAllowDistinctiveIdentifier,
                             bool aAllowPersistentState,
                             bool aUseHardwareSecureCodecs) {
  mSessionManager->Init(aAllowDistinctiveIdentifier, aAllowPersistentState);
  // We call mHost->OnInitialized() in the session manager once it has
  // initialized.
}

void ClearKeyCDM::GetStatusForPolicy(uint32_t aPromiseId,
                                     const Policy& aPolicy) {
  // MediaKeys::GetStatusForPolicy checks the keysystem and
  // reject the promise with NS_ERROR_DOM_NOT_SUPPORTED_ERR without calling CDM.
  // This function should never be called and is not supported.
  assert(false);
}
void ClearKeyCDM::SetServerCertificate(uint32_t aPromiseId,
                                       const uint8_t* aServerCertificateData,
                                       uint32_t aServerCertificateDataSize) {
  mSessionManager->SetServerCertificate(aPromiseId, aServerCertificateData,
                                        aServerCertificateDataSize);
}

void ClearKeyCDM::CreateSessionAndGenerateRequest(uint32_t aPromiseId,
                                                  SessionType aSessionType,
                                                  InitDataType aInitDataType,
                                                  const uint8_t* aInitData,
                                                  uint32_t aInitDataSize) {
  mSessionManager->CreateSession(aPromiseId, aInitDataType, aInitData,
                                 aInitDataSize, aSessionType);
}

void ClearKeyCDM::LoadSession(uint32_t aPromiseId, SessionType aSessionType,
                              const char* aSessionId, uint32_t aSessionIdSize) {
  mSessionManager->LoadSession(aPromiseId, aSessionId, aSessionIdSize);
}

void ClearKeyCDM::UpdateSession(uint32_t aPromiseId, const char* aSessionId,
                                uint32_t aSessionIdSize,
                                const uint8_t* aResponse,
                                uint32_t aResponseSize) {
  mSessionManager->UpdateSession(aPromiseId, aSessionId, aSessionIdSize,
                                 aResponse, aResponseSize);
}

void ClearKeyCDM::CloseSession(uint32_t aPromiseId, const char* aSessionId,
                               uint32_t aSessionIdSize) {
  mSessionManager->CloseSession(aPromiseId, aSessionId, aSessionIdSize);
}

void ClearKeyCDM::RemoveSession(uint32_t aPromiseId, const char* aSessionId,
                                uint32_t aSessionIdSize) {
  mSessionManager->RemoveSession(aPromiseId, aSessionId, aSessionIdSize);
}

void ClearKeyCDM::TimerExpired(void* aContext) {
  // Clearkey is not interested in timers, so this method has not been
  // implemented.
  assert(false);
}

Status ClearKeyCDM::Decrypt(const InputBuffer_2& aEncryptedBuffer,
                            DecryptedBlock* aDecryptedBuffer) {
  return mSessionManager->Decrypt(aEncryptedBuffer, aDecryptedBuffer);
}

Status ClearKeyCDM::InitializeAudioDecoder(
    const AudioDecoderConfig_2& aAudioDecoderConfig) {
  // Audio decoding is not supported by Clearkey because Widevine doesn't
  // support it and Clearkey's raison d'etre is to provide test coverage
  // for paths that Widevine will exercise in the wild.
  return Status::kDecodeError;
}

Status ClearKeyCDM::InitializeVideoDecoder(
    const VideoDecoderConfig_2& aVideoDecoderConfig) {
#ifdef ENABLE_WMF
  mVideoDecoder = new VideoDecoder(mHost);
  return mVideoDecoder->InitDecode(aVideoDecoderConfig);
#else
  return Status::kDecodeError;
#endif
}

void ClearKeyCDM::DeinitializeDecoder(StreamType aDecoderType) {
#ifdef ENABLE_WMF
  if (aDecoderType == StreamType::kStreamTypeVideo) {
    mVideoDecoder->DecodingComplete();
    mVideoDecoder = nullptr;
  }
#endif
}

void ClearKeyCDM::ResetDecoder(StreamType aDecoderType) {
#ifdef ENABLE_WMF
  if (aDecoderType == StreamType::kStreamTypeVideo) {
    mVideoDecoder->Reset();
  }
#endif
}

Status ClearKeyCDM::DecryptAndDecodeFrame(const InputBuffer_2& aEncryptedBuffer,
                                          VideoFrame* aVideoFrame) {
#ifdef ENABLE_WMF
  return mVideoDecoder->Decode(aEncryptedBuffer, aVideoFrame);
#else
  return Status::kDecodeError;
#endif
}

Status ClearKeyCDM::DecryptAndDecodeSamples(
    const InputBuffer_2& aEncryptedBuffer, AudioFrames* aAudioFrame) {
  // Audio decoding is not supported by Clearkey because Widevine doesn't
  // support it and Clearkey's raison d'etre is to provide test coverage
  // for paths that Widevine will exercise in the wild.
  return Status::kDecodeError;
}

void ClearKeyCDM::OnPlatformChallengeResponse(
    const PlatformChallengeResponse& aResponse) {
  // This function should never be called and is not supported.
  assert(false);
}

void ClearKeyCDM::OnQueryOutputProtectionStatus(
    QueryResult aResult, uint32_t aLinkMask, uint32_t aOutputProtectionMask) {
  // This function should never be called and is not supported.
  assert(false);
}

void ClearKeyCDM::OnStorageId(uint32_t aVersion, const uint8_t* aStorageId,
                              uint32_t aStorageIdSize) {
  // This function should never be called and is not supported.
  assert(false);
}

void ClearKeyCDM::Destroy() {
  mSessionManager->DecryptingComplete();
#ifdef ENABLE_WMF
  // If we have called 'DeinitializeDecoder' mVideoDecoder will be null.
  if (mVideoDecoder) {
    mVideoDecoder->DecodingComplete();
  }
#endif
  delete this;
}

void ClearKeyCDM::EnableProtectionQuery() {
  MOZ_ASSERT(!mIsProtectionQueryEnabled,
             "Should not be called more than once per CDM");
  mIsProtectionQueryEnabled = true;
}
