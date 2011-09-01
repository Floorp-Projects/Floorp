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

#ifndef jsion_bailouts_arm_h__
#define jsion_bailouts_arm_h__

#include "ion/IonFrames.h"
#include "ion/IonMacroAssembler.h"

namespace js {
namespace ion {

class IonCompartment;

#if defined(_WIN32)
# pragma pack(push, 1)
#endif

class BailoutStack
{
    uintptr_t frameClassId_;
    double    fpregs_[FloatRegisters::Total];
    uintptr_t regs_[Registers::Total];
    union {
        uintptr_t frameSize_;
        uintptr_t tableOffset_;
    };

  public:
    FrameSizeClass frameClass() const {
        return FrameSizeClass::FromClass(frameClassId_);
    }
    double readFloatReg(const FloatRegister &reg) const {
        return fpregs_[reg.code()];
    }
    uintptr_t readReg(const Register &reg) const {
        return regs_[reg.code()];
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
};

class ExtendedBailoutStack : public BailoutStack
{
    uintptr_t snapshotOffset_;

  public:
    SnapshotOffset snapshotOffset() const {
        JS_ASSERT(frameClass() == FrameSizeClass::None());
        return snapshotOffset_;
    }
};

class BailoutEnvironment
{
    void **sp_;
    void **frame_;
    const ExtendedBailoutStack *bailout_;
    uint32 frameSize_;
    uint32 bailoutId_;

  public:
    BailoutEnvironment(IonCompartment *ion, void **esp);

    IonFramePrefix *top() const;
    FrameSizeClass frameClass() const {
        return bailout_->frameClass();
    }
    uint32 bailoutId() const {
        JS_ASSERT(frameClass() != FrameSizeClass::None());
        return bailoutId_;
    }
    uint32 snapshotOffset() const {
        return bailout_->snapshotOffset();
    }
    uintptr_t readSlot(uint32 offset) const {
        JS_ASSERT(offset % STACK_SLOT_SIZE == 0);
        return *(uintptr_t *)((uint8 *)frame_ + offset);
    }
    double readDoubleSlot(uint32 offset) const {
        JS_ASSERT(offset % STACK_SLOT_SIZE == 0);
        return *(double *)((uint8 *)frame_ + offset);
    }
    uintptr_t readReg(const Register &reg) const {
        return bailout_->readReg(reg);
    }
    double readFloatReg(const FloatRegister &reg) const {
        return bailout_->readFloatReg(reg);
    }
};

} // namespace ion
} // namespace js

#endif // jsion_bailouts_arm_h__

