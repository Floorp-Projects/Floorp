/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"
#include "jscompartment.h"

#include "jit/arm/Assembler-arm.h"
#include "jit/Bailouts.h"
#include "jit/JitCompartment.h"

using namespace js;
using namespace js::jit;

namespace js {
namespace jit {

class BailoutStack
{
    uintptr_t frameClassId_;
    // This is pushed in the bailout handler. Both entry points into the handler
    // inserts their own value int lr, which is then placed onto the stack along
    // with frameClassId_ above. This should be migrated to ip.
  public:
    union {
        uintptr_t frameSize_;
        uintptr_t tableOffset_;
    };

  protected: // Silence Clang warning about unused private fields.
    mozilla::Array<double, FloatRegisters::TotalPhys> fpregs_;
    mozilla::Array<uintptr_t, Registers::Total> regs_;

    uintptr_t snapshotOffset_;
    uintptr_t padding_;

  public:
    FrameSizeClass frameClass() const {
        return FrameSizeClass::FromClass(frameClassId_);
    }
    uintptr_t tableOffset() const {
        JS_ASSERT(frameClass() != FrameSizeClass::None());
        return tableOffset_;
    }
    uint32_t frameSize() const {
        if (frameClass() == FrameSizeClass::None())
            return frameSize_;
        return frameClass().frameSize();
    }
    MachineState machine() {
        return MachineState::FromBailout(regs_, fpregs_);
    }
    SnapshotOffset snapshotOffset() const {
        JS_ASSERT(frameClass() == FrameSizeClass::None());
        return snapshotOffset_;
    }
    uint8_t *parentStackPointer() const {
        if (frameClass() == FrameSizeClass::None())
            return (uint8_t *)this + sizeof(BailoutStack);
        return (uint8_t *)this + offsetof(BailoutStack, snapshotOffset_);
    }
};

// Make sure the compiler doesn't add extra padding.
static_assert((sizeof(BailoutStack) % 8) == 0, "BailoutStack should be 8-byte aligned.");

} // namespace jit
} // namespace js

IonBailoutIterator::IonBailoutIterator(const JitActivationIterator &activations,
                                       BailoutStack *bailout)
  : JitFrameIterator(activations),
    machine_(bailout->machine())
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

    if (bailout->frameClass() == FrameSizeClass::None()) {
        snapshotOffset_ = bailout->snapshotOffset();
        return;
    }

    // Compute the snapshot offset from the bailout ID.
    JitActivation *activation = activations.activation()->asJit();
    JSRuntime *rt = activation->compartment()->runtimeFromMainThread();
    JitCode *code = rt->jitRuntime()->getBailoutTable(bailout->frameClass());
    uintptr_t tableOffset = bailout->tableOffset();
    uintptr_t tableStart = reinterpret_cast<uintptr_t>(Assembler::BailoutTableStart(code->raw()));

    JS_ASSERT(tableOffset >= tableStart &&
              tableOffset < tableStart + code->instructionsSize());
    JS_ASSERT((tableOffset - tableStart) % BAILOUT_TABLE_ENTRY_SIZE == 0);

    uint32_t bailoutId = ((tableOffset - tableStart) / BAILOUT_TABLE_ENTRY_SIZE) - 1;
    JS_ASSERT(bailoutId < BAILOUT_TABLE_SIZE);

    snapshotOffset_ = topIonScript_->bailoutToSnapshot(bailoutId);
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
