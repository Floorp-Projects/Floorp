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

namespace mozilla::gmp {

MessageLoop* GMPContentChild::GMPMessageLoop() {
  return mGMPChild->GMPMessageLoop();
}

void GMPContentChild::CheckThread() {
  MOZ_ASSERT(mGMPChild->mGMPMessageLoop == MessageLoop::current());
}

#if defined(MOZ_SANDBOX) && defined(MOZ_DEBUG) && defined(ENABLE_TESTS)
mozilla::ipc::IPCResult GMPContentChild::RecvInitSandboxTesting(
    Endpoint<PSandboxTestingChild>&& aEndpoint) {
  if (!SandboxTestingChild::Initialize(std::move(aEndpoint))) {
    return IPC_FAIL(
        this, "InitSandboxTesting failed to initialise the child process.");
  }
  return IPC_OK();
}
#endif

void GMPContentChild::ActorDestroy(ActorDestroyReason aWhy) {
  mGMPChild->GMPContentChildActorDestroy(this);
}

void GMPContentChild::ProcessingError(Result aCode, const char* aReason) {
  mGMPChild->ProcessingError(aCode, aReason);
}

already_AddRefed<PGMPVideoDecoderChild>
GMPContentChild::AllocPGMPVideoDecoderChild() {
  return MakeAndAddRef<GMPVideoDecoderChild>(this);
}

already_AddRefed<PGMPVideoEncoderChild>
GMPContentChild::AllocPGMPVideoEncoderChild() {
  return MakeAndAddRef<GMPVideoEncoderChild>(this);
}

already_AddRefed<PChromiumCDMChild> GMPContentChild::AllocPChromiumCDMChild(
    const nsACString& aKeySystem) {
  return MakeAndAddRef<ChromiumCDMChild>(this);
}

mozilla::ipc::IPCResult GMPContentChild::RecvPGMPVideoDecoderConstructor(
    PGMPVideoDecoderChild* aActor) {
  auto vdc = static_cast<GMPVideoDecoderChild*>(aActor);

  void* vd = nullptr;
  GMPErr err = mGMPChild->GetAPI(GMP_API_VIDEO_DECODER, &vdc->Host(), &vd);
  if (err != GMPNoErr || !vd) {
    return IPC_FAIL(this, "GMPGetAPI call failed trying to construct decoder.");
  }

  vdc->Init(static_cast<GMPVideoDecoder*>(vd));

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPContentChild::RecvPGMPVideoEncoderConstructor(
    PGMPVideoEncoderChild* aActor) {
  auto vec = static_cast<GMPVideoEncoderChild*>(aActor);

  void* ve = nullptr;
  GMPErr err = mGMPChild->GetAPI(GMP_API_VIDEO_ENCODER, &vec->Host(), &ve);
  if (err != GMPNoErr || !ve) {
    return IPC_FAIL(this, "GMPGetAPI call failed trying to construct encoder.");
  }

  vec->Init(static_cast<GMPVideoEncoder*>(ve));

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPContentChild::RecvPChromiumCDMConstructor(
    PChromiumCDMChild* aActor, const nsACString& aKeySystem) {
  ChromiumCDMChild* child = static_cast<ChromiumCDMChild*>(aActor);
  cdm::Host_10* host10 = child;

  void* cdm = nullptr;
  GMPErr err = mGMPChild->GetAPI(CHROMIUM_CDM_API, host10, &cdm, aKeySystem);
  if (err != GMPNoErr || !cdm) {
    return IPC_FAIL(this, "GMPGetAPI call failed trying to get CDM.");
  }

  child->Init(static_cast<cdm::ContentDecryptionModule_10*>(cdm),
              mGMPChild->mStorageId);

  return IPC_OK();
}

void GMPContentChild::CloseActive() {
  // Invalidate and remove any remaining API objects.
  const ManagedContainer<PGMPVideoDecoderChild>& videoDecoders =
      ManagedPGMPVideoDecoderChild();
  for (const auto& key : videoDecoders) {
    key->SendShutdown();
  }

  const ManagedContainer<PGMPVideoEncoderChild>& videoEncoders =
      ManagedPGMPVideoEncoderChild();
  for (const auto& key : videoEncoders) {
    key->SendShutdown();
  }

  const ManagedContainer<PChromiumCDMChild>& cdms = ManagedPChromiumCDMChild();
  for (const auto& key : cdms) {
    key->SendShutdown();
  }
}

bool GMPContentChild::IsUsed() {
  return !ManagedPGMPVideoDecoderChild().IsEmpty() ||
         !ManagedPGMPVideoEncoderChild().IsEmpty() ||
         !ManagedPChromiumCDMChild().IsEmpty();
}

}  // namespace mozilla::gmp
