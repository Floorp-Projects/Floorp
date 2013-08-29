/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"
#include "jscompartment.h"
#include "jit/Bailouts.h"
#include "jit/IonCompartment.h"
#include "jit/IonFrames-inl.h"

using namespace js;
using namespace js::jit;

#if defined(_WIN32)
# pragma pack(push, 1)
#endif

namespace js {
namespace jit {

class BailoutStack
{
    double    fpregs_[FloatRegisters::Total];
    uintptr_t regs_[Registers::Total];
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
  : IonFrameIterator(activations),
    machine_(bailout->machineState())
{
    uint8_t *sp = bailout->parentStackPointer();
    uint8_t *fp = sp + bailout->frameSize();

    current_ = fp;
    type_ = IonFrame_OptimizedJS;
    topFrameSize_ = current_ - sp;
    topIonScript_ = script()->ionScript();
    snapshotOffset_ = bailout->snapshotOffset();
}

IonBailoutIterator::IonBailoutIterator(const JitActivationIterator &activations,
                                       InvalidationBailoutStack *bailout)
  : IonFrameIterator(activations),
    machine_(bailout->machine())
{
    returnAddressToFp_ = bailout->osiPointReturnAddress();
    topIonScript_ = bailout->ionScript();
    const OsiIndex *osiIndex = topIonScript_->getOsiIndex(returnAddressToFp_);

    current_ = (uint8_t*) bailout->fp();
    type_ = IonFrame_OptimizedJS;
    topFrameSize_ = current_ - bailout->sp();
    snapshotOffset_ = osiIndex->snapshotOffset();
}
