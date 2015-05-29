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
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new DeleteTask<Transport>(GetTransport()));
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
GMPContentChild::AllocPGMPVideoDecoderChild()
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
  return new GMPVideoEncoderChild(this);
}

bool
GMPContentChild::DeallocPGMPVideoEncoderChild(PGMPVideoEncoderChild* aActor)
{
  delete aActor;
  return true;
}

bool
GMPContentChild::RecvPGMPDecryptorConstructor(PGMPDecryptorChild* aActor)
{
  GMPDecryptorChild* child = static_cast<GMPDecryptorChild*>(aActor);
  GMPDecryptorHost* host = static_cast<GMPDecryptorHost*>(child);

  void* session = nullptr;
  GMPErr err = mGMPChild->GetAPI(GMP_API_DECRYPTOR, host, &session);
  if (err != GMPNoErr && !session) {
    // XXX to remove in bug 1147692
    err = mGMPChild->GetAPI(GMP_API_DECRYPTOR_COMPAT, host, &session);
  }

  if (err != GMPNoErr || !session) {
    return false;
  }

  child->Init(static_cast<GMPDecryptor*>(session));

  return true;
}

bool
GMPContentChild::RecvPGMPAudioDecoderConstructor(PGMPAudioDecoderChild* aActor)
{
  auto vdc = static_cast<GMPAudioDecoderChild*>(aActor);

  void* vd = nullptr;
  GMPErr err = mGMPChild->GetAPI(GMP_API_AUDIO_DECODER, &vdc->Host(), &vd);
  if (err != GMPNoErr || !vd) {
    return false;
  }

  vdc->Init(static_cast<GMPAudioDecoder*>(vd));

  return true;
}

bool
GMPContentChild::RecvPGMPVideoDecoderConstructor(PGMPVideoDecoderChild* aActor)
{
  auto vdc = static_cast<GMPVideoDecoderChild*>(aActor);

  void* vd = nullptr;
  GMPErr err = mGMPChild->GetAPI(GMP_API_VIDEO_DECODER, &vdc->Host(), &vd);
  if (err != GMPNoErr || !vd) {
    NS_WARNING("GMPGetAPI call failed trying to construct decoder.");
    return false;
  }

  vdc->Init(static_cast<GMPVideoDecoder*>(vd));

  return true;
}

bool
GMPContentChild::RecvPGMPVideoEncoderConstructor(PGMPVideoEncoderChild* aActor)
{
  auto vec = static_cast<GMPVideoEncoderChild*>(aActor);

  void* ve = nullptr;
  GMPErr err = mGMPChild->GetAPI(GMP_API_VIDEO_ENCODER, &vec->Host(), &ve);
  if (err != GMPNoErr || !ve) {
    NS_WARNING("GMPGetAPI call failed trying to construct encoder.");
    return false;
  }

  vec->Init(static_cast<GMPVideoEncoder*>(ve));

  return true;
}

void
GMPContentChild::CloseActive()
{
  // Invalidate and remove any remaining API objects.
  const nsTArray<PGMPAudioDecoderChild*>& audioDecoders =
    ManagedPGMPAudioDecoderChild();
  for (uint32_t i = audioDecoders.Length(); i > 0; i--) {
    audioDecoders[i - 1]->SendShutdown();
  }

  const nsTArray<PGMPDecryptorChild*>& decryptors =
    ManagedPGMPDecryptorChild();
  for (uint32_t i = decryptors.Length(); i > 0; i--) {
    decryptors[i - 1]->SendShutdown();
  }

  const nsTArray<PGMPVideoDecoderChild*>& videoDecoders =
    ManagedPGMPVideoDecoderChild();
  for (uint32_t i = videoDecoders.Length(); i > 0; i--) {
    videoDecoders[i - 1]->SendShutdown();
  }

  const nsTArray<PGMPVideoEncoderChild*>& videoEncoders =
    ManagedPGMPVideoEncoderChild();
  for (uint32_t i = videoEncoders.Length(); i > 0; i--) {
    videoEncoders[i - 1]->SendShutdown();
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
