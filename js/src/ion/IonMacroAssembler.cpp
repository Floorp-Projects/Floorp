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

using namespace js;
using namespace js::ion;

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

        unsigned count = types->getObjectCount();
        for (unsigned i = 0; i < count; i++) {
            if (JSObject *object = types->getSingleObject(i))
                branchPtr(Equal, obj, ImmGCPtr(object), &matched);
        }

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

void
MacroAssembler::PushRegsInMask(RegisterSet set)
{
    size_t diff = 0;
    for (AnyRegisterIterator iter(set); iter.more(); iter++) {
        AnyRegister reg = *iter;
        if (reg.isFloat())
            diff += sizeof(double);
        else
            diff += STACK_SLOT_SIZE;
    }
    // It has been decreed that the stack shall always be 8 byte aligned on ARM.
    // maintain this invariant.  It can't hurt other platforms.
    size_t new_diff = (diff + 7) & ~7;
    reserveStack(new_diff);

    diff = new_diff - diff;
    for (AnyRegisterIterator iter(set); iter.more(); iter++) {
        AnyRegister reg = *iter;
        if (reg.isFloat()) {
            storeDouble(reg.fpu(), Address(StackPointer, diff));
            diff += sizeof(double);
        } else {
            storePtr(reg.gpr(), Address(StackPointer, diff));
            diff += STACK_SLOT_SIZE;
        }
    }
}

void
MacroAssembler::PopRegsInMask(RegisterSet set)
{
    size_t diff = 0;
    // Undo the alignment that was done in PushRegsInMask.
    for (AnyRegisterIterator iter(set); iter.more(); iter++) {
        AnyRegister reg = *iter;
        if (reg.isFloat())
            diff += sizeof(double);
        else
            diff += STACK_SLOT_SIZE;
    }
    size_t new_diff = (diff + 7) & ~7;

    diff = new_diff - diff;
    for (AnyRegisterIterator iter(set); iter.more(); iter++) {
        AnyRegister reg = *iter;
        if (reg.isFloat()) {
            loadDouble(Address(StackPointer, diff), reg.fpu());
            diff += sizeof(double);
        } else {
            loadPtr(Address(StackPointer, diff), reg.gpr());
            diff += STACK_SLOT_SIZE;
        }
    }
    freeStack(new_diff);
}
