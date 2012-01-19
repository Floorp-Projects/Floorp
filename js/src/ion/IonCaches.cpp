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
 *   Brian Hackett <bhackett@mozilla.com>
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

#include "jsscope.h"

#include "CodeGenerator.h"
#include "Ion.h"
#include "IonCaches.h"
#include "IonLinker.h"
#include "IonSpewer.h"

#include "vm/Stack.h"

using namespace js;
using namespace js::ion;

void
CodeLocationJump::repoint(IonCode *code)
{
    JS_ASSERT(!absolute);
    JS_ASSERT(size_t(raw_) <= code->instructionsSize());
    raw_ = code->raw() + size_t(raw_);
#ifdef JS_CPU_X64
    jumpTableEntry_ = Assembler::PatchableJumpAddress(code, (size_t) jumpTableEntry_);
#endif
    markAbsolute(true);
}

static const size_t MAX_STUBS = 16;

bool
IonCacheGetProperty::attachNative(JSContext *cx, JSObject *obj, const Shape *shape)
{
    MacroAssembler masm;

    Label exit_;
    CodeOffsetJump exitOffset =
        masm.branchPtrWithPatch(Assembler::NotEqual,
                                Address(object(), JSObject::offsetOfShape()),
                                ImmGCPtr(obj->lastProperty()),
                                &exit_);
    masm.bind(&exit_);

    if (obj->isFixedSlot(shape->slot())) {
        Address addr(object(), JSObject::getFixedSlotOffset(shape->slot()));
        masm.loadTypedOrValue(addr, output());
    } else {
        bool restoreSlots = false;
        Register slotsReg;

        if (output().hasValue()) {
            slotsReg = output().valueReg().scratchReg();
        } else if (output().type() == MIRType_Double) {
            slotsReg = object();
            masm.Push(slotsReg);
            restoreSlots = true;
        } else {
            slotsReg = output().typedReg().gpr();
        }

        masm.loadPtr(Address(object(), JSObject::offsetOfSlots()), slotsReg);

        Address addr(slotsReg, obj->dynamicSlotIndex(shape->slot()) * sizeof(Value));
        masm.loadTypedOrValue(addr, output());

        if (restoreSlots)
            masm.Pop(slotsReg);
    }

    Label rejoin_;
    CodeOffsetJump rejoinOffset = masm.jumpWithPatch(&rejoin_);
    masm.bind(&rejoin_);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);

    CodeLocationJump rejoinJump(code, rejoinOffset);
    CodeLocationJump exitJump(code, exitOffset);

    PatchJump(lastJump(), CodeLocationLabel(code));
    PatchJump(rejoinJump, rejoinLabel());
    PatchJump(exitJump, cacheLabel());
    updateLastJump(exitJump);

    IonSpew(IonSpew_InlineCaches, "Generated native GETPROP stub at %p", code->raw());

    return true;
}

bool
js::ion::GetPropertyCache(JSContext *cx, size_t cacheIndex, JSObject *obj, Value *vp)
{
    IonScript *ion = GetTopIonFrame(cx);
    IonCacheGetProperty &cache = ion->getCache(cacheIndex).toGetProperty();
    JSAtom *atom = cache.atom();

    // For now, just stop generating new stubs once we hit the stub count
    // limit. Once we can make calls from within generated stubs, a new call
    // stub will be generated instead and the previous stubs unlinked.
    if (cache.stubCount() < MAX_STUBS && obj->isNative()) {
        cache.incrementStubCount();

        const Shape *shape = obj->nativeLookup(cx, ATOM_TO_JSID(atom));
        if (shape && shape->hasSlot() && shape->hasDefaultGetter() && !shape->isMethod()) {
            if (!cache.attachNative(cx, obj, shape))
                return false;
        }
    }

    if (!obj->getGeneric(cx, obj, ATOM_TO_JSID(atom), vp))
        return false;

    JSScript *script;
    jsbytecode *pc;
    cache.getScriptedLocation(&script, &pc);

    // For now, all caches are impure.
    JS_ASSERT(script);

    types::TypeScript::Monitor(cx, script, pc, *vp);

    return true;
}

bool
IonCacheSetProperty::attachNativeExisting(JSContext *cx, JSObject *obj, const Shape *shape)
{
    MacroAssembler masm;

    Label exit_;
    CodeOffsetJump exitOffset =
        masm.branchPtrWithPatch(Assembler::NotEqual,
                                Address(object(), JSObject::offsetOfShape()),
                                ImmGCPtr(obj->lastProperty()),
                                &exit_);
    masm.bind(&exit_);

    if (obj->isFixedSlot(shape->slot())) {
        Address addr(object(), JSObject::getFixedSlotOffset(shape->slot()));
        masm.storeConstantOrRegister(value(), addr);
    } else {
        Register slotsReg = object();
        masm.Push(slotsReg);

        masm.loadPtr(Address(object(), JSObject::offsetOfSlots()), slotsReg);

        Address addr(slotsReg, obj->dynamicSlotIndex(shape->slot()) * sizeof(Value));
        masm.storeConstantOrRegister(value(), addr);

        masm.Pop(slotsReg);
    }

    Label rejoin_;
    CodeOffsetJump rejoinOffset = masm.jumpWithPatch(&rejoin_);
    masm.bind(&rejoin_);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);

    CodeLocationJump rejoinJump(code, rejoinOffset);
    CodeLocationJump exitJump(code, exitOffset);

    PatchJump(lastJump(), CodeLocationLabel(code));
    PatchJump(rejoinJump, rejoinLabel());
    PatchJump(exitJump, cacheLabel());
    updateLastJump(exitJump);

    IonSpew(IonSpew_InlineCaches, "Generated native SETPROP stub at %p", code->raw());

    return true;
}

bool
js::ion::SetPropertyCache(JSContext *cx, size_t cacheIndex, JSObject *obj, Value value)
{
    IonScript *ion = GetTopIonFrame(cx);
    IonCacheSetProperty &cache = ion->getCache(cacheIndex).toSetProperty();
    JSAtom *atom = cache.atom();

    // Stop generating new stubs once we hit the stub count limit, see
    // GetPropertyCache.
    if (cache.stubCount() < MAX_STUBS) {
        cache.incrementStubCount();

        const Shape *shape = obj->nativeLookup(cx, ATOM_TO_JSID(atom));
        if (shape && shape->hasSlot() && shape->hasDefaultSetter() && !shape->isMethod()) {
            if (!cache.attachNativeExisting(cx, obj, shape))
                return false;
        }
    }

    return obj->setGeneric(cx, ATOM_TO_JSID(atom), &value, cache.strict());
}
