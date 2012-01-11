/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <dvander@alliedmods.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "jscntxt.h"
#include "jscompartment.h"
#include "ion/Bailouts.h"
#include "ion/IonCompartment.h"
#include "ion/IonFrames-inl.h"

using namespace js;
using namespace js::ion;

#if 0
// no clue what these asserts should be.
JS_STATIC_ASSERT(sizeof(BailoutStack) ==
                 sizeof(uintptr_t) +
                 sizeof(double) * 8 +
                 sizeof(uintptr_t) * 8 +
                 sizeof(uintptr_t));

JS_STATIC_ASSERT(sizeof(ExtendedBailoutStack) ==
                 sizeof(BailoutStack) +
                 sizeof(uintptr_t));

#endif
#if 0
BailoutEnvironment::BailoutEnvironment(IonCompartment *ion, void **sp)
  : sp_(sp)
{
    bailout_ = reinterpret_cast<ExtendedBailoutStack *>(sp);

    if (bailout_->frameClass() != FrameSizeClass::None()) {
        frameSize_ = bailout_->frameSize();
        frame_ = &sp_[sizeof(BailoutStack) / STACK_SLOT_SIZE];

        // Compute the bailout ID.
        IonCode *code = ion->getBailoutTable(bailout_->frameClass());
        uintptr_t tableOffset = bailout_->tableOffset();
        uintptr_t tableStart = reinterpret_cast<uintptr_t>(code->raw());

        JS_ASSERT(tableOffset >= tableStart &&
                  tableOffset < tableStart + code->instructionsSize());
        JS_ASSERT((tableOffset - tableStart) % BAILOUT_TABLE_ENTRY_SIZE == 0);

        bailoutId_ = ((tableOffset - tableStart) / BAILOUT_TABLE_ENTRY_SIZE) - 1;
        JS_ASSERT(bailoutId_ < BAILOUT_TABLE_SIZE);
    } else {
        frameSize_ = bailout_->frameSize();
        frame_ = &sp_[sizeof(ExtendedBailoutStack) / STACK_SLOT_SIZE];
    }
}

IonFramePrefix *
BailoutEnvironment::top() const
{
    return (IonFramePrefix *)&frame_[frameSize_ / STACK_SLOT_SIZE];
}

#endif

namespace js {
namespace ion {

class BailoutStack
{
    uintptr_t frameClassId_;
    // This is pushed in the bailout handler.  Both entry points into the handler
    // inserts their own value int lr, which is then placed onto the stack along
    // with frameClassId_ above.  This should be migrated to ip.
  public:
    union {
        uintptr_t frameSize_;
        uintptr_t tableOffset_;
    };

  private:
    double    fpregs_[FloatRegisters::Total];
    uintptr_t regs_[Registers::Total];

    uintptr_t snapshotOffset_;

  public:
    FrameSizeClass frameClass() const {
        return FrameSizeClass::FromClass(frameClassId_);
    }
    uintptr_t tableOffset() const {
        JS_ASSERT(frameClass() != FrameSizeClass::None());
        return tableOffset_;
    }
    uint32 frameSize() const {
        if (frameClass() == FrameSizeClass::None())
            return frameSize_;
        return frameClass().frameSize();
    }
    MachineState machine() {
        return MachineState(regs_, fpregs_);
    }
    SnapshotOffset snapshotOffset() const {
        JS_ASSERT(frameClass() == FrameSizeClass::None());
        return snapshotOffset_;
    }
    uint8 *parentStackPointer() const {
        if (frameClass() == FrameSizeClass::None())
            return (uint8 *)this + sizeof(BailoutStack);
        return (uint8 *)this + offsetof(BailoutStack, snapshotOffset_);
    }
};

class InvalidationBailoutStack
{
    double fpregs_[FloatRegisters::Total];
    uintptr_t regs_[Registers::Total];
    uintptr_t pad[2];
    uintptr_t frameDescriptor_;

    size_t frameSize() const {
        return frameDescriptor_ >> FRAMETYPE_BITS;
    }
    size_t frameDescriptorOffset() const {
        return offsetof(InvalidationBailoutStack, frameDescriptor_);
    }

  public:
    uint8 *sp() const {
        return (uint8 *) this + frameDescriptorOffset() + sizeof(size_t) + sizeof(size_t);
    }
    uint8 *fp() const {
        return sp() + frameSize();
    }
    MachineState machine() {
        return MachineState(regs_, fpregs_);
    }
  public:
};

} // namespace ion
} // namespace js

FrameRecovery
ion::FrameRecoveryFromBailout(IonCompartment *ion, BailoutStack *bailout)
{
    uint8 *sp = bailout->parentStackPointer();
    uint8 *fp = sp + bailout->frameSize();

    if (bailout->frameClass() == FrameSizeClass::None())
        return FrameRecovery::FromSnapshot(fp, sp, bailout->machine(), bailout->snapshotOffset());

    // Compute the bailout ID.
    IonCode *code = ion->getBailoutTable(bailout->frameClass());
    uintptr_t tableOffset = bailout->tableOffset();
    uintptr_t tableStart = reinterpret_cast<uintptr_t>(code->raw());
    // the table offset is really the absolute
    JS_ASSERT(tableOffset >= tableStart &&
              tableOffset < tableStart + code->instructionsSize());
    JS_ASSERT((tableOffset - tableStart) % BAILOUT_TABLE_ENTRY_SIZE == 0);

    uint32 bailoutId = ((tableOffset - tableStart) / BAILOUT_TABLE_ENTRY_SIZE) - 1;
    JS_ASSERT(bailoutId < BAILOUT_TABLE_SIZE);

    return FrameRecovery::FromBailoutId(fp, sp, bailout->machine(), bailoutId);
}

FrameRecovery
ion::FrameRecoveryFromInvalidation(IonCompartment *ion, InvalidationBailoutStack *bailout)
{
    IonJSFrameLayout *fp = (IonJSFrameLayout *) bailout->fp();
    InvalidationRecord *record = CalleeTokenToInvalidationRecord(fp->calleeToken());
    const IonFrameInfo *exitInfo = record->ionScript->getFrameInfo(record->returnAddress);
    SnapshotOffset snapshotOffset = exitInfo->snapshotOffset();

    return FrameRecovery::FromSnapshot(bailout->fp(), bailout->sp(), bailout->machine(),
                                       snapshotOffset);
}
