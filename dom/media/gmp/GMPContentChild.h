/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPContentChild_h_
#define GMPContentChild_h_

#include "mozilla/gmp/PGMPContentChild.h"
#include "GMPSharedMemManager.h"

namespace mozilla {
namespace gmp {

class GMPChild;

class GMPContentChild : public PGMPContentChild
                      , public GMPSharedMem
{
public:
  explicit GMPContentChild(GMPChild* aChild);
  virtual ~GMPContentChild();

  MessageLoop* GMPMessageLoop();

  mozilla::ipc::IPCResult RecvPGMPAudioDecoderConstructor(PGMPAudioDecoderChild* aActor) override;
  mozilla::ipc::IPCResult RecvPGMPDecryptorConstructor(PGMPDecryptorChild* aActor) override;
  mozilla::ipc::IPCResult RecvPGMPVideoDecoderConstructor(PGMPVideoDecoderChild* aActor, const uint32_t& aDecryptorId) override;
  mozilla::ipc::IPCResult RecvPGMPVideoEncoderConstructor(PGMPVideoEncoderChild* aActor) override;

  PGMPAudioDecoderChild* AllocPGMPAudioDecoderChild() override;
  bool DeallocPGMPAudioDecoderChild(PGMPAudioDecoderChild* aActor) override;

  PGMPDecryptorChild* AllocPGMPDecryptorChild() override;
  bool DeallocPGMPDecryptorChild(PGMPDecryptorChild* aActor) override;

  PGMPVideoDecoderChild* AllocPGMPVideoDecoderChild(const uint32_t& aDecryptorId) override;
  bool DeallocPGMPVideoDecoderChild(PGMPVideoDecoderChild* aActor) override;

  PGMPVideoEncoderChild* AllocPGMPVideoEncoderChild() override;
  bool DeallocPGMPVideoEncoderChild(PGMPVideoEncoderChild* aActor) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
  void ProcessingError(Result aCode, const char* aReason) override;

  // GMPSharedMem
  void CheckThread() override;

  void CloseActive();
  bool IsUsed();

  GMPChild* mGMPChild;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPContentChild_h_
