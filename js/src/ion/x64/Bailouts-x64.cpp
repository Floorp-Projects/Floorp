/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"
#include "jscompartment.h"
#include "ion/Bailouts.h"
#include "ion/IonCompartment.h"
#include "ion/IonFrames-inl.h"

using namespace js;
using namespace js::ion;

#if defined(_WIN32)
# pragma pack(push, 1)
#endif

namespace js {
namespace ion {

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
    uint32 snapshotOffset() const {
        return snapshotOffset_;
    }
    uint32 frameSize() const {
        return frameSize_;
    }
    uint8 *parentStackPointer() {
        return (uint8 *)this + sizeof(BailoutStack);
    }
};

} // namespace ion
} // namespace js

#if defined(_WIN32)
# pragma pack(pop)
#endif

IonBailoutIterator::IonBailoutIterator(const IonActivationIterator &activations,
                                       BailoutStack *bailout)
  : IonFrameIterator(activations),
    machine_(bailout->machineState())
{
    uint8 *sp = bailout->parentStackPointer();
    uint8 *fp = sp + bailout->frameSize();

    current_ = fp;
    type_ = IonFrame_JS;
    topFrameSize_ = current_ - sp;
    topIonScript_ = script()->ion;
    snapshotOffset_ = bailout->snapshotOffset();
}

IonBailoutIterator::IonBailoutIterator(const IonActivationIterator &activations,
                                       InvalidationBailoutStack *bailout)
  : IonFrameIterator(activations),
    machine_(bailout->machine())
{
    returnAddressToFp_ = bailout->osiPointReturnAddress();
    topIonScript_ = bailout->ionScript();
    const OsiIndex *osiIndex = topIonScript_->getOsiIndex(returnAddressToFp_);

    current_ = (uint8*) bailout->fp();
    type_ = IonFrame_JS;
    topFrameSize_ = current_ - bailout->sp();
    snapshotOffset_ = osiIndex->snapshotOffset();
}
