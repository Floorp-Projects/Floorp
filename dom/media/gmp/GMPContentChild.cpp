/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPContentChild.h"
#include "GMPChild.h"
#include "GMPVideoDecoderChild.h"
#include "GMPVideoEncoderChild.h"
#include "ChromiumCDMChild.h"
#include "base/task.h"
#include "GMPUtils.h"

namespace mozilla {
namespace gmp {

GMPContentChild::GMPContentChild(GMPChild* aChild)
  : mGMPChild(aChild)
{
  MOZ_COUNT_CTOR(GMPContentChild);
}

GMPContentChild::~GMPContentChild()
{
  MOZ_COUNT_DTOR(GMPContentChild);
}

MessageLoop*
GMPContentChild::GMPMessageLoop()
{
  return mGMPChild->GMPMessageLoop();
}

void
GMPContentChild::CheckThread()
{
  MOZ_ASSERT(mGMPChild->mGMPMessageLoop == MessageLoop::current());
}

void
GMPContentChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mGMPChild->GMPContentChildActorDestroy(this);
}

void
GMPContentChild::ProcessingError(Result aCode, const char* aReason)
{
  mGMPChild->ProcessingError(aCode, aReason);
}

PGMPVideoDecoderChild*
GMPContentChild::AllocPGMPVideoDecoderChild(const uint32_t& aDecryptorId)
{
  GMPVideoDecoderChild* actor = new GMPVideoDecoderChild(this);
  actor->AddRef();
  return actor;
}

bool
GMPContentChild::DeallocPGMPVideoDecoderChild(PGMPVideoDecoderChild* aActor)
{
  static_cast<GMPVideoDecoderChild*>(aActor)->Release();
  return true;
}

PGMPVideoEncoderChild*
GMPContentChild::AllocPGMPVideoEncoderChild()
{
  GMPVideoEncoderChild* actor = new GMPVideoEncoderChild(this);
  actor->AddRef();
  return actor;
}

bool
GMPContentChild::DeallocPGMPVideoEncoderChild(PGMPVideoEncoderChild* aActor)
{
  static_cast<GMPVideoEncoderChild*>(aActor)->Release();
  return true;
}

PChromiumCDMChild*
GMPContentChild::AllocPChromiumCDMChild()
{
  ChromiumCDMChild* actor = new ChromiumCDMChild(this);
  actor->AddRef();
  return actor;
}

bool
GMPContentChild::DeallocPChromiumCDMChild(PChromiumCDMChild* aActor)
{
  static_cast<ChromiumCDMChild*>(aActor)->Release();
  return true;
}

mozilla::ipc::IPCResult
GMPContentChild::RecvPGMPVideoDecoderConstructor(PGMPVideoDecoderChild* aActor,
                                                 const uint32_t& aDecryptorId)
{
  auto vdc = static_cast<GMPVideoDecoderChild*>(aActor);

  void* vd = nullptr;
  GMPErr err = mGMPChild->GetAPI(GMP_API_VIDEO_DECODER, &vdc->Host(), &vd, aDecryptorId);
  if (err != GMPNoErr || !vd) {
    NS_WARNING("GMPGetAPI call failed trying to construct decoder.");
    return IPC_FAIL_NO_REASON(this);
  }

  vdc->Init(static_cast<GMPVideoDecoder*>(vd));

  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPContentChild::RecvPGMPVideoEncoderConstructor(PGMPVideoEncoderChild* aActor)
{
  auto vec = static_cast<GMPVideoEncoderChild*>(aActor);

  void* ve = nullptr;
  GMPErr err = mGMPChild->GetAPI(GMP_API_VIDEO_ENCODER, &vec->Host(), &ve);
  if (err != GMPNoErr || !ve) {
    NS_WARNING("GMPGetAPI call failed trying to construct encoder.");
    return IPC_FAIL_NO_REASON(this);
  }

  vec->Init(static_cast<GMPVideoEncoder*>(ve));

  return IPC_OK();
}


class ChromiumCDM8BackwardsCompat : public cdm::ContentDecryptionModule_9
{
public:
  explicit ChromiumCDM8BackwardsCompat(
    cdm::Host_9* aHost,
    cdm::ContentDecryptionModule_8* aCDM)
      : mCDM(aCDM),
        mHost(aHost) { }

  void Initialize(bool aAllowDistinctiveIdentifier,
                  bool aAllowPersistentState) override
  {
    mCDM->Initialize(aAllowDistinctiveIdentifier, aAllowPersistentState);
  }

  void SetServerCertificate(uint32_t aPromiseId,
                            const uint8_t* aServerCertificateData,
                            uint32_t aServerCertificateDataSize) override
  {
    mCDM->SetServerCertificate(aPromiseId,
                               aServerCertificateData,
                               aServerCertificateDataSize);
  }

  void GetStatusForPolicy(uint32_t aPromiseId,
                          const cdm::Policy& policy) override
  {
    //Only support on version 9 CDM, so rejecting the promise.
    mHost->OnRejectPromise(aPromiseId,
                           cdm::Exception::kExceptionNotSupportedError,
                           0,
                           nullptr,
                           0);

  }

  void CreateSessionAndGenerateRequest(uint32_t aPromiseId,
                                       cdm::SessionType aSessionType,
                                       cdm::InitDataType aInitDataType,
                                       const uint8_t* aInitData,
                                       uint32_t aInitDataSize) override
  {
    mCDM->CreateSessionAndGenerateRequest(
      aPromiseId, aSessionType, aInitDataType, aInitData, aInitDataSize);
  }

  void LoadSession(uint32_t aPromiseId,
                   cdm::SessionType aSessionType,
                   const char* aSessionId,
                   uint32_t aSessionIdSize) override
  {
    mCDM->LoadSession(aPromiseId, aSessionType, aSessionId, aSessionIdSize);
  }

  void UpdateSession(uint32_t aPromiseId,
                     const char* aSessionId,
                     uint32_t aSessionIdSize,
                     const uint8_t* aResponse,
                     uint32_t aResponseSize) override
  {
    mCDM->UpdateSession(aPromiseId,
                        aSessionId,
                        aSessionIdSize,
                        aResponse,
                        aResponseSize);
  }

  void CloseSession(uint32_t aPromiseId,
                    const char* aSessionId,
                    uint32_t aSessionIdSize) override
  {
    mCDM->CloseSession(aPromiseId, aSessionId, aSessionIdSize);
  }

  void RemoveSession(uint32_t aPromiseId,
                     const char* aSessionId,
                     uint32_t aSessionIdSize) override
  {
    mCDM->RemoveSession(aPromiseId, aSessionId, aSessionIdSize);
  }

  void TimerExpired(void* aContext) override { mCDM->TimerExpired(aContext); }

  cdm::Status Decrypt(const cdm::InputBuffer& aEncryptedBuffer,
                      cdm::DecryptedBlock* aDecryptedBuffer) override
  {
    return mCDM->Decrypt(aEncryptedBuffer, aDecryptedBuffer);
  }

  cdm::Status InitializeAudioDecoder(
    const cdm::AudioDecoderConfig& aAudioDecoderConfig) override
  {
    return mCDM->InitializeAudioDecoder(aAudioDecoderConfig);
  }

  cdm::Status InitializeVideoDecoder(
    const cdm::VideoDecoderConfig& aVideoDecoderConfig) override
  {
    return mCDM->InitializeVideoDecoder(aVideoDecoderConfig);
  }

  void DeinitializeDecoder(cdm::StreamType aDecoderType) override
  {
    mCDM->DeinitializeDecoder(aDecoderType);
  }

  void ResetDecoder(cdm::StreamType aDecoderType) override
  {
    mCDM->ResetDecoder(aDecoderType);
  }

  cdm::Status DecryptAndDecodeFrame(const cdm::InputBuffer& aEncryptedBuffer,
                                    cdm::VideoFrame* aVideoFrame) override
  {
    return mCDM->DecryptAndDecodeFrame(aEncryptedBuffer, aVideoFrame);
  }

  cdm::Status DecryptAndDecodeSamples(const cdm::InputBuffer& aEncryptedBuffer,
                                      cdm::AudioFrames* aAudioFrames) override
  {
    return mCDM->DecryptAndDecodeSamples(aEncryptedBuffer, aAudioFrames);
  }

  void OnPlatformChallengeResponse(
      const cdm::PlatformChallengeResponse& aResponse) override
  {
    mCDM->OnPlatformChallengeResponse(aResponse);
  }

  void OnQueryOutputProtectionStatus(cdm::QueryResult aResult,
                                     uint32_t aLinkMask,
                                     uint32_t aOutputProtectionMask) override
  {
    mCDM->OnQueryOutputProtectionStatus(aResult, aLinkMask, aOutputProtectionMask);
  }

  void OnStorageId(const uint8_t* aStorageId,
                   uint32_t aStorageIdSize) override
  {
    //Only support on version 9 CDM.
  }

  void Destroy() override
  {
    mCDM->Destroy();
    delete this;
  }
  cdm::ContentDecryptionModule_8* mCDM;
  cdm::Host_9* mHost;
}; // class ChromiumCDM8BackwardsCompat

mozilla::ipc::IPCResult
GMPContentChild::RecvPChromiumCDMConstructor(PChromiumCDMChild* aActor)
{
  ChromiumCDMChild* child = static_cast<ChromiumCDMChild*>(aActor);
  cdm::Host_9* host9 = child;

  void* cdm = nullptr;
  // Create version 9 CDM first.
  GMPErr err = mGMPChild->GetAPI(CHROMIUM_CDM_API, host9, &cdm);
  if (err != GMPNoErr || !cdm) {
    // Try to create older version 8 CDM.
    cdm::Host_8* host8 = child;
    err = mGMPChild->GetAPI(CHROMIUM_CDM_API_BACKWARD_COMPAT, host8, &cdm);
    cdm =
      new ChromiumCDM8BackwardsCompat(
        host9,
        static_cast<cdm::ContentDecryptionModule_8*>(cdm));
    if (err != GMPNoErr) {
      NS_WARNING("GMPGetAPI call failed trying to get CDM.");
      return IPC_FAIL_NO_REASON(this);
    }
  }

  child->Init(static_cast<cdm::ContentDecryptionModule_9*>(cdm));

  return IPC_OK();
}

void
GMPContentChild::CloseActive()
{
  // Invalidate and remove any remaining API objects.
  const ManagedContainer<PGMPVideoDecoderChild>& videoDecoders =
    ManagedPGMPVideoDecoderChild();
  for (auto iter = videoDecoders.ConstIter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->SendShutdown();
  }

  const ManagedContainer<PGMPVideoEncoderChild>& videoEncoders =
    ManagedPGMPVideoEncoderChild();
  for (auto iter = videoEncoders.ConstIter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->SendShutdown();
  }

  const ManagedContainer<PChromiumCDMChild>& cdms = ManagedPChromiumCDMChild();
  for (auto iter = cdms.ConstIter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->SendShutdown();
  }
}

bool
GMPContentChild::IsUsed()
{
  return !ManagedPGMPVideoDecoderChild().IsEmpty() ||
         !ManagedPGMPVideoEncoderChild().IsEmpty() ||
         !ManagedPChromiumCDMChild().IsEmpty();
}

} // namespace gmp
} // namespace mozilla
