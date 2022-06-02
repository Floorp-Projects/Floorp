/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/Bailouts.h"
#include "jit/JitFrames.h"
#include "jit/JitRuntime.h"
#include "jit/SafepointIndex.h"
#include "jit/ScriptFromCalleeToken.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"

#include "vm/JSScript-inl.h"

using namespace js;
using namespace js::jit;

#if defined(_WIN32)
#  pragma pack(push, 1)
#endif

namespace js {
namespace jit {

class BailoutStack {
  RegisterDump::FPUArray fpregs_;
  RegisterDump::GPRArray regs_;
  uintptr_t frameSize_;
  uintptr_t snapshotOffset_;

 public:
  uint32_t frameSize() const { return frameSize_; }
  MachineState machine() { return MachineState::FromBailout(regs_, fpregs_); }
  SnapshotOffset snapshotOffset() const { return snapshotOffset_; }
  uint8_t* parentStackPointer() const {
    return (uint8_t*)this + sizeof(BailoutStack);
  }
};

}  // namespace jit
}  // namespace js

#if defined(_WIN32)
#  pragma pack(pop)
#endif

BailoutFrameInfo::BailoutFrameInfo(const JitActivationIterator& activations,
                                   BailoutStack* bailout)
    : machine_(bailout->machine()) {
  uint8_t* sp = bailout->parentStackPointer();
  framePointer_ = sp + bailout->frameSize();
  topFrameSize_ = framePointer_ - sp;

  JSScript* script =
      ScriptFromCalleeToken(((JitFrameLayout*)framePointer_)->calleeToken());
  topIonScript_ = script->ionScript();

  attachOnJitActivation(activations);
  snapshotOffset_ = bailout->snapshotOffset();
}

BailoutFrameInfo::BailoutFrameInfo(const JitActivationIterator& activations,
                                   InvalidationBailoutStack* bailout)
    : machine_(bailout->machine()) {
  framePointer_ = (uint8_t*)bailout->fp();
  topFrameSize_ = framePointer_ - bailout->sp();
  topIonScript_ = bailout->ionScript();
  attachOnJitActivation(activations);

  uint8_t* returnAddressToFp_ = bailout->osiPointReturnAddress();
  const OsiIndex* osiIndex = topIonScript_->getOsiIndex(returnAddressToFp_);
  snapshotOffset_ = osiIndex->snapshotOffset();
}
