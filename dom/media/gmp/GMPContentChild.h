/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPContentChild_h_
#define GMPContentChild_h_

#include "mozilla/gmp/PGMPContentChild.h"
#include "GMPSharedMemManager.h"

#if defined(MOZ_SANDBOX) && defined(MOZ_DEBUG) && defined(ENABLE_TESTS)
#  include "mozilla/SandboxTestingChild.h"
#endif

namespace mozilla::gmp {

class GMPChild;

class GMPContentChild : public PGMPContentChild, public GMPSharedMem {
 public:
  // Mark AddRef and Release as `final`, as they overload pure virtual
  // implementations in PGMPContentChild.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPContentChild, final)

  explicit GMPContentChild(GMPChild* aChild) : mGMPChild(aChild) {}

  MessageLoop* GMPMessageLoop();

  mozilla::ipc::IPCResult RecvPGMPVideoDecoderConstructor(
      PGMPVideoDecoderChild* aActor) override;
  mozilla::ipc::IPCResult RecvPGMPVideoEncoderConstructor(
      PGMPVideoEncoderChild* aActor) override;
  mozilla::ipc::IPCResult RecvPChromiumCDMConstructor(
      PChromiumCDMChild* aActor, const nsACString& aKeySystem) override;

  already_AddRefed<PGMPVideoDecoderChild> AllocPGMPVideoDecoderChild();

  already_AddRefed<PGMPVideoEncoderChild> AllocPGMPVideoEncoderChild();

  already_AddRefed<PChromiumCDMChild> AllocPChromiumCDMChild(
      const nsACString& aKeySystem);

#if defined(MOZ_SANDBOX) && defined(MOZ_DEBUG) && defined(ENABLE_TESTS)
  mozilla::ipc::IPCResult RecvInitSandboxTesting(
      Endpoint<PSandboxTestingChild>&& aEndpoint);
#endif

  void ActorDestroy(ActorDestroyReason aWhy) override;
  void ProcessingError(Result aCode, const char* aReason) override;

  // GMPSharedMem
  void CheckThread() override;

  void CloseActive();
  bool IsUsed();

  GMPChild* mGMPChild;

 private:
  ~GMPContentChild() = default;
};

}  // namespace mozilla::gmp

#endif  // GMPContentChild_h_
