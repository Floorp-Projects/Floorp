/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/Bailouts.h"

using namespace js;
using namespace js::jit;

#if defined(_WIN32)
# pragma pack(push, 1)
#endif

namespace js {
namespace jit {

class BailoutStack
{
    mozilla::Array<double, FloatRegisters::Total> fpregs_;
    mozilla::Array<uintptr_t, Registers::Total> regs_;
    uintptr_t frameSize_;
    uintptr_t snapshotOffset_;

  public:
    MachineState machineState() {
        return MachineState::FromBailout(regs_, fpregs_);
    }
    uint32_t snapshotOffset() const {
        return snapshotOffset_;
    }
    uint32_t frameSize() const {
        return frameSize_;
    }
    uint8_t *parentStackPointer() {
        return (uint8_t *)this + sizeof(BailoutStack);
    }
};

} // namespace jit
} // namespace js

#if defined(_WIN32)
# pragma pack(pop)
#endif

IonBailoutIterator::IonBailoutIterator(const JitActivationIterator &activations,
                                       BailoutStack *bailout)
  : JitFrameIterator(activations),
    machine_(bailout->machineState())
{
    uint8_t *sp = bailout->parentStackPointer();
    uint8_t *fp = sp + bailout->frameSize();

    kind_ = Kind_BailoutIterator;
    current_ = fp;
    type_ = JitFrame_IonJS;
    topFrameSize_ = current_ - sp;
    switch (mode_) {
      case SequentialExecution: topIonScript_ = script()->ionScript(); break;
      case ParallelExecution: topIonScript_ = script()->parallelIonScript(); break;
      default: MOZ_CRASH("No such execution mode");
    }
    snapshotOffset_ = bailout->snapshotOffset();
}

IonBailoutIterator::IonBailoutIterator(const JitActivationIterator &activations,
                                       InvalidationBailoutStack *bailout)
  : JitFrameIterator(activations),
    machine_(bailout->machine())
{
    kind_ = Kind_BailoutIterator;
    returnAddressToFp_ = bailout->osiPointReturnAddress();
    topIonScript_ = bailout->ionScript();
    const OsiIndex *osiIndex = topIonScript_->getOsiIndex(returnAddressToFp_);

    current_ = (uint8_t*) bailout->fp();
    type_ = JitFrame_IonJS;
    topFrameSize_ = current_ - bailout->sp();
    snapshotOffset_ = osiIndex->snapshotOffset();
}
