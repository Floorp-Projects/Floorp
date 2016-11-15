/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPContentChild.h"
#include "GMPChild.h"
#include "GMPAudioDecoderChild.h"
#include "GMPDecryptorChild.h"
#include "GMPVideoDecoderChild.h"
#include "GMPVideoEncoderChild.h"
#include "base/task.h"

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

PGMPAudioDecoderChild*
GMPContentChild::AllocPGMPAudioDecoderChild()
{
  return new GMPAudioDecoderChild(this);
}

bool
GMPContentChild::DeallocPGMPAudioDecoderChild(PGMPAudioDecoderChild* aActor)
{
  delete aActor;
  return true;
}

PGMPDecryptorChild*
GMPContentChild::AllocPGMPDecryptorChild()
{
  GMPDecryptorChild* actor = new GMPDecryptorChild(this,
                                                   mGMPChild->mPluginVoucher,
                                                   mGMPChild->mSandboxVoucher);
  actor->AddRef();
  return actor;
}

bool
GMPContentChild::DeallocPGMPDecryptorChild(PGMPDecryptorChild* aActor)
{
  static_cast<GMPDecryptorChild*>(aActor)->Release();
  return true;
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

// Adapts GMPDecryptor7 to the current GMPDecryptor version.
class GMPDecryptor7BackwardsCompat : public GMPDecryptor {
public:
  explicit GMPDecryptor7BackwardsCompat(GMPDecryptor7* aDecryptorV7)
    : mDecryptorV7(aDecryptorV7)
  {
  }

  void Init(GMPDecryptorCallback* aCallback,
            bool aDistinctiveIdentifierRequired,
            bool aPersistentStateRequired) override
  {
    // Distinctive identifier and persistent state arguments not present
    // in v7 interface.
    mDecryptorV7->Init(aCallback);
  }

  void CreateSession(uint32_t aCreateSessionToken,
                     uint32_t aPromiseId,
                     const char* aInitDataType,
                     uint32_t aInitDataTypeSize,
                     const uint8_t* aInitData,
                     uint32_t aInitDataSize,
                     GMPSessionType aSessionType) override
  {
    mDecryptorV7->CreateSession(aCreateSessionToken,
                                aPromiseId,
                                aInitDataType,
                                aInitDataTypeSize,
                                aInitData,
                                aInitDataSize,
                                aSessionType);
  }

  void LoadSession(uint32_t aPromiseId,
                   const char* aSessionId,
                   uint32_t aSessionIdLength) override
  {
    mDecryptorV7->LoadSession(aPromiseId, aSessionId, aSessionIdLength);
  }

  void UpdateSession(uint32_t aPromiseId,
                     const char* aSessionId,
                     uint32_t aSessionIdLength,
                     const uint8_t* aResponse,
                     uint32_t aResponseSize) override
  {
    mDecryptorV7->UpdateSession(aPromiseId,
                                aSessionId,
                                aSessionIdLength,
                                aResponse,
                                aResponseSize);
  }

  void CloseSession(uint32_t aPromiseId,
                    const char* aSessionId,
                    uint32_t aSessionIdLength) override
  {
    mDecryptorV7->CloseSession(aPromiseId, aSessionId, aSessionIdLength);
  }

  void RemoveSession(uint32_t aPromiseId,
                     const char* aSessionId,
                     uint32_t aSessionIdLength) override
  {
    mDecryptorV7->RemoveSession(aPromiseId, aSessionId, aSessionIdLength);
  }

  void SetServerCertificate(uint32_t aPromiseId,
                            const uint8_t* aServerCert,
                            uint32_t aServerCertSize) override
  {
    mDecryptorV7->SetServerCertificate(aPromiseId, aServerCert, aServerCertSize);
  }

  void Decrypt(GMPBuffer* aBuffer,
               GMPEncryptedBufferMetadata* aMetadata) override
  {
    mDecryptorV7->Decrypt(aBuffer, aMetadata);
  }

  void DecryptingComplete() override
  {
    mDecryptorV7->DecryptingComplete();
    delete this;
  }
private:
  GMPDecryptor7* mDecryptorV7;
};

mozilla::ipc::IPCResult
GMPContentChild::RecvPGMPDecryptorConstructor(PGMPDecryptorChild* aActor)
{
  GMPDecryptorChild* child = static_cast<GMPDecryptorChild*>(aActor);
  GMPDecryptorHost* host = static_cast<GMPDecryptorHost*>(child);

  void* ptr = nullptr;
  GMPErr err = mGMPChild->GetAPI(GMP_API_DECRYPTOR, host, &ptr, aActor->Id());
  GMPDecryptor* decryptor = nullptr;
  if (GMP_SUCCEEDED(err) && ptr) {
    decryptor = static_cast<GMPDecryptor*>(ptr);
  } else if (err != GMPNoErr) {
    // We Adapt the previous GMPDecryptor version to the current, so that
    // Gecko thinks it's only talking to the current version. v7 differs
    // from v9 in its Init() function arguments, and v9 has extra enumeration
    // members at the end of the key status enumerations.
    err = mGMPChild->GetAPI(GMP_API_DECRYPTOR_BACKWARDS_COMPAT, host, &ptr);
    if (err != GMPNoErr || !ptr) {
      return IPC_FAIL_NO_REASON(this);
    }
    decryptor = new GMPDecryptor7BackwardsCompat(static_cast<GMPDecryptor7*>(ptr));
  }

  child->Init(decryptor);

  return IPC_OK();
}

mozilla::ipc::IPCResult
GMPContentChild::RecvPGMPAudioDecoderConstructor(PGMPAudioDecoderChild* aActor)
{
  auto vdc = static_cast<GMPAudioDecoderChild*>(aActor);

  void* vd = nullptr;
  GMPErr err = mGMPChild->GetAPI(GMP_API_AUDIO_DECODER, &vdc->Host(), &vd);
  if (err != GMPNoErr || !vd) {
    return IPC_FAIL_NO_REASON(this);
  }

  vdc->Init(static_cast<GMPAudioDecoder*>(vd));

  return IPC_OK();
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

void
GMPContentChild::CloseActive()
{
  // Invalidate and remove any remaining API objects.
  const ManagedContainer<PGMPAudioDecoderChild>& audioDecoders =
    ManagedPGMPAudioDecoderChild();
  for (auto iter = audioDecoders.ConstIter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->SendShutdown();
  }

  const ManagedContainer<PGMPDecryptorChild>& decryptors =
    ManagedPGMPDecryptorChild();
  for (auto iter = decryptors.ConstIter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->SendShutdown();
  }

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
}

bool
GMPContentChild::IsUsed()
{
  return !ManagedPGMPAudioDecoderChild().IsEmpty() ||
         !ManagedPGMPDecryptorChild().IsEmpty() ||
         !ManagedPGMPVideoDecoderChild().IsEmpty() ||
         !ManagedPGMPVideoEncoderChild().IsEmpty();
}

} // namespace gmp
} // namespace mozilla
