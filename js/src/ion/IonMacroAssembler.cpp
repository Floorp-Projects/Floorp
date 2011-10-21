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

#include "jsinfer.h"
#include "jsinferinlines.h"
#include "IonMacroAssembler.h"
#include "MoveEmitter.h"

using namespace js;
using namespace js::ion;

uint32
MacroAssembler::setupABICall(uint32 args, uint32 returnSize, const MoveOperand *returnOperand)
{
    JS_ASSERT(!inCall_);
    JS_ASSERT_IF(returnSize <= sizeof(void *), !returnOperand);
    inCall_ = true;

    callProperties_ = None;
    uint32 stackForArgs = args > NumArgRegs
                          ? (args - NumArgRegs) * sizeof(void *)
                          : 0;
    uint32 stackForRes = 0;

    // Reserve space for return value larger than a register size.  If this is
    // the case, an additional argument is added at the first position of the
    // argument list expected by the function to store a pointer which targets a
    // memory area used to store the content of the returned value.
    if (returnSize > sizeof(void *)) {
        callProperties_ |= LargeReturnValue;

        // An additional stack slot is added to the shift if the number of
        // argument plus the return value pointer implies the addition of one
        // stack slot.
        //
        // SetAnyABIArg is call before the reserve of the stack with a stack
        // pointer which refer to the state after the reserve is made.  This is
        // safe to do it here since the Move resolution is made inside the
        // callWithABI function.
        if (args >= NumArgRegs) {
            callProperties_ |= ReturnArgConsumeStack;
            stackForRes = sizeof(void *);
        }

        // If an operand is provided with hypothetize that memory has been
        // allocated by the caller and that it should not be allocated on the
        // stack.
        //
        // Otherwise, the location of the value is just after the stack space
        // reserved for the arguments.

        if (returnOperand) {
            setAnyABIArg(0, *returnOperand);
        } else {
            setAnyABIArg(0, MoveOperand(StackPointer, stackForArgs + stackForRes));
            stackForRes += returnSize;
        }
    }

    return stackForArgs + stackForRes;
}

void
MacroAssembler::setupAlignedABICall(uint32 args, uint32 returnSize, const MoveOperand *returnOperand)
{
    uint32 stackForCall = setupABICall(args, returnSize, returnOperand);

    // Find the total number of bytes the stack will have been adjusted by,
    // in order to compute alignment. Include a stack slot for saving the stack
    // pointer, which will be dynamically aligned.
    dynamicAlignment_ = false;
    stackAdjust_ = alignStackForCall(stackForCall);
    reserveStack(stackAdjust_);
}

void
MacroAssembler::setupUnalignedABICall(uint32 args, const Register &scratch, uint32 returnSize,
                                      const MoveOperand *returnOperand)
{
    uint32 stackForCall = setupABICall(args, returnSize, returnOperand);

    // Find the total number of bytes the stack will have been adjusted by,
    // in order to compute alignment.
    dynamicAlignment_ = true;
    stackAdjust_ = dynamicallyAlignStackForCall(stackForCall, scratch);
    reserveStack(stackAdjust_);
}

void
MacroAssembler::setAnyABIArg(uint32 arg, const MoveOperand &from)
{
    MoveOperand to;
    Register dest;
    if (GetArgReg(arg, &dest)) {
        to = MoveOperand(dest);
    } else {
        // There is no register for this argument, so just move it to its
        // stack slot immediately.
        uint32 disp = GetArgStackDisp(arg);
        to = MoveOperand(StackPointer, disp);
    }
    enoughMemory_ &= moveResolver_.addMove(from, to, Move::GENERAL);
}

void
MacroAssembler::callWithABI(void *fun)
{
    JS_ASSERT(inCall_);

    // Perform argument move resolution now.
    if (stackAdjust_ >= sizeof(void *)) {
        enoughMemory_ &= moveResolver_.resolve();
        if (!enoughMemory_)
            return;

        MoveEmitter emitter(*this);
        emitter.emit(moveResolver());
        emitter.finish();
    }

#ifdef DEBUG
    checkCallAlignment();
#endif

    // To avoid relocation issues, just move the C pointer into the return
    // register and call indirectly.
    movePtr(ImmWord(fun), CallReg);
    call(CallReg);
}

void
MacroAssembler::getABIRes(uint32 offset, const MoveOperand &to)
{
    JS_ASSERT(inCall_);

    // Reading a volatile register after a call is unsafe.
    JS_ASSERT_IF(to.isMemory(), to.base().code() & Registers::NonVolatileMask);

    MoveOperand from;

    callProperties_ |= HasGetRes;
    if (callProperties_ & LargeReturnValue) {
        // large return values are stored where the return register is pointing at.
        from = MoveOperand(ReturnReg, offset);
    } else {
        // The return value is inside the return register.
        JS_ASSERT(!offset);
        from = MoveOperand(ReturnReg);
    }

    enoughMemory_ &= moveResolver_.addMove(from, to, Move::GENERAL);
}

void
MacroAssembler::finishABICall()
{
    JS_ASSERT(inCall_);

    // Perform result move resolution now.
    if (callProperties_ & HasGetRes) {
        enoughMemory_ &= moveResolver_.resolve();
        if (!enoughMemory_)
            return;

        MoveEmitter emitter(*this);
        emitter.emit(moveResolver());
        emitter.finish();
    }

    // When return value is larger than register size, the callee unwind the first
    // argument (return value pointer) and leave the rest of the unwind to the
    // caller.
    stackAdjust_ -= callProperties_ & ReturnArgConsumeStack ? sizeof(void *) : 0;

    freeStack(stackAdjust_);
    if (dynamicAlignment_)
        restoreStackFromDynamicAlignment();

    JS_ASSERT(inCall_);
    inCall_ = false;
}

template <typename T> void
MacroAssembler::guardTypeSet(const T &address, types::TypeSet *types,
                             Register scratch, Label *mismatched)
{
    JS_ASSERT(!types->unknown());

    Label matched;
    Register tag = extractTag(address, scratch);

    if (types->hasType(types::Type::DoubleType())) {
        // The double type also implies Int32.
        JS_ASSERT(types->hasType(types::Type::Int32Type()));
        branchTestNumber(Equal, tag, &matched);
    } else if (types->hasType(types::Type::Int32Type())) {
        branchTestInt32(Equal, tag, &matched);
    }

    if (types->hasType(types::Type::UndefinedType()))
        branchTestUndefined(Equal, tag, &matched);
    if (types->hasType(types::Type::BooleanType()))
        branchTestBoolean(Equal, tag, &matched);
    if (types->hasType(types::Type::StringType()))
        branchTestString(Equal, tag, &matched);
    if (types->hasType(types::Type::NullType()))
        branchTestNull(Equal, tag, &matched);

    if (types->hasType(types::Type::AnyObjectType())) {
        branchTestObject(Equal, tag, &matched);
    } else if (types->getObjectCount()) {
        branchTestObject(NotEqual, tag, mismatched);
        Register obj = extractObject(address, scratch);

        Label notSingleton;
        branchTest32(Zero, Address(obj, offsetof(JSObject, flags)),
                     Imm32(JSObject::SINGLETON_TYPE), &notSingleton);

        unsigned count = types->getObjectCount();
        for (unsigned i = 0; i < count; i++) {
            if (JSObject *object = types->getSingleObject(i))
                branchPtr(Equal, obj, ImmGCPtr(object), &matched);
        }
        jump(mismatched);

        bind(&notSingleton);
        loadPtr(Address(obj, JSObject::offsetOfType()), scratch);
        for (unsigned i = 0; i < count; i++) {
            if (types::TypeObject *object = types->getTypeObject(i))
                branchPtr(Equal, obj, ImmGCPtr(object), &matched);
        }
    }

    jump(mismatched);
    bind(&matched);
}

template void MacroAssembler::guardTypeSet(const Address &address, types::TypeSet *types,
                                           Register scratch, Label *mismatched);
template void MacroAssembler::guardTypeSet(const ValueOperand &value, types::TypeSet *types,
                                           Register scratch, Label *mismatched);
