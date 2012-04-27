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

#include "jsinterpinlines.h"

#include "vm/Stack.h"
#include "IonFrames-inl.h"

using namespace js;
using namespace js::ion;

void
CodeLocationJump::repoint(IonCode *code, MacroAssembler *masm)
{
    JS_ASSERT(!absolute);
    size_t new_off = (size_t)raw_;
    if (masm != NULL) {
#ifdef JS_CPU_X64
        JS_ASSERT((uint64_t)raw_ <= UINT32_MAX);
#endif
        new_off = masm->actualOffset((uintptr_t)raw_);
    }
    raw_ = code->raw() + new_off;
#ifdef JS_CPU_X64
    jumpTableEntry_ = Assembler::PatchableJumpAddress(code, (size_t) jumpTableEntry_);
#endif
    markAbsolute(true);
}

void
CodeLocationLabel::repoint(IonCode *code, MacroAssembler *masm)
{
     JS_ASSERT(!absolute);
     size_t new_off = (size_t)raw_;
     if (masm != NULL) {
#ifdef JS_CPU_X64
        JS_ASSERT((uint64_t)raw_ <= UINT32_MAX);
#endif
        new_off = masm->actualOffset((uintptr_t)raw_);
     }
     JS_ASSERT(new_off < code->instructionsSize());

     raw_ = code->raw() + new_off;
     markAbsolute(true);
}

void
CodeOffsetLabel::fixup(MacroAssembler *masm)
{
     offset_ = masm->actualOffset(offset_);
}

static const size_t MAX_STUBS = 16;

static void
GeneratePrototypeGuards(JSContext *cx, MacroAssembler &masm, JSObject *obj, JSObject *holder,
                        Register objectReg, Register scratchReg, Label *failures)
{
    JS_ASSERT(obj != holder);

    if (obj->hasUncacheableProto()) {
        // Note: objectReg and scratchReg may be the same register, so we cannot
        // use objectReg in the rest of this function.
        masm.loadPtr(Address(objectReg, JSObject::offsetOfType()), scratchReg);
        Address proto(scratchReg, offsetof(types::TypeObject, proto));
        masm.branchPtr(Assembler::NotEqual, proto, ImmGCPtr(obj->getProto()), failures);
    }

    JSObject *pobj = obj->getProto();
    while (pobj != holder) {
        if (pobj->hasUncacheableProto()) {
            if (pobj->hasSingletonType()) {
                types::TypeObject *type = pobj->getType(cx);
                masm.movePtr(ImmGCPtr(type), scratchReg);
                Address proto(scratchReg, offsetof(types::TypeObject, proto));
                masm.branchPtr(Assembler::NotEqual, proto, ImmGCPtr(obj->getProto()), failures);
            } else {
                masm.movePtr(ImmGCPtr(pobj), scratchReg);
                Address objType(scratchReg, JSObject::offsetOfType());
                masm.branchPtr(Assembler::NotEqual, objType, ImmGCPtr(pobj->type()), failures);
            }
        }
        pobj = pobj->getProto();
    }
}

struct GetNativePropertyStub
{
    CodeOffsetJump exitOffset;
    CodeOffsetJump rejoinOffset;

    void generate(JSContext *cx, MacroAssembler &masm, JSObject *obj, JSObject *holder,
                  const Shape *shape, Register object, TypedOrValueRegister output,
                  Label *failures)
    {
        // If there's a single jump to |failures|, we can patch the shape guard
        // jump directly. Otherwise, jump to the end of the stub, so there's a
        // common point to patch.
        bool multipleFailureJumps = failures->used();
        exitOffset = masm.branchPtrWithPatch(Assembler::NotEqual,
                                             Address(object, JSObject::offsetOfShape()),
                                             ImmGCPtr(obj->lastProperty()),
                                             failures);

        bool restoreScratch = false;
        Register scratchReg = Register::FromCode(0); // Quell compiler warning.

        // If we need a scratch register, use either an output register or the object
        // register (and restore it afterwards). After this point, we cannot jump
        // directly to |failures| since we may still have to pop the object register.
        Label prototypeFailures;
        if (obj != holder || !holder->isFixedSlot(shape->slot())) {
            if (output.hasValue()) {
                scratchReg = output.valueReg().scratchReg();
            } else if (output.type() == MIRType_Double) {
                scratchReg = object;
                masm.push(scratchReg);
                restoreScratch = true;
            } else {
                scratchReg = output.typedReg().gpr();
            }
        }

        Register holderReg;
        if (obj != holder) {
            // Note: this may clobber the object register if it's used as scratch.
            GeneratePrototypeGuards(cx, masm, obj, holder, object, scratchReg, &prototypeFailures);

            // Guard on the holder's shape.
            holderReg = scratchReg;
            masm.movePtr(ImmGCPtr(holder), holderReg);
            masm.branchPtr(Assembler::NotEqual,
                           Address(holderReg, JSObject::offsetOfShape()),
                           ImmGCPtr(holder->lastProperty()),
                           &prototypeFailures);
        } else {
            holderReg = object;
        }

        if (holder->isFixedSlot(shape->slot())) {
            Address addr(holderReg, JSObject::getFixedSlotOffset(shape->slot()));
            masm.loadTypedOrValue(addr, output);
        } else {
            masm.loadPtr(Address(holderReg, JSObject::offsetOfSlots()), scratchReg);

            Address addr(scratchReg, holder->dynamicSlotIndex(shape->slot()) * sizeof(Value));
            masm.loadTypedOrValue(addr, output);
        }

        if (restoreScratch)
            masm.pop(scratchReg);

        Label rejoin_;
        rejoinOffset = masm.jumpWithPatch(&rejoin_);
        masm.bind(&rejoin_);

        if (obj != holder || multipleFailureJumps) {
            masm.bind(&prototypeFailures);
            if (restoreScratch)
                masm.pop(scratchReg);
            masm.bind(failures);
            Label exit_;
            exitOffset = masm.jumpWithPatch(&exit_);
            masm.bind(&exit_);
        } else {
            masm.bind(failures);
        }
    }
};

bool
IonCacheGetProperty::attachNative(JSContext *cx, JSObject *obj, JSObject *holder, const Shape *shape)
{
    MacroAssembler masm;
    Label failures;

    GetNativePropertyStub getprop;
    getprop.generate(cx, masm, obj, holder, shape, object(), output(), &failures);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    CodeLocationJump rejoinJump(code, getprop.rejoinOffset);
    CodeLocationJump exitJump(code, getprop.exitOffset);

    PatchJump(lastJump(), CodeLocationLabel(code));
    PatchJump(rejoinJump, rejoinLabel());
    PatchJump(exitJump, cacheLabel());
    updateLastJump(exitJump);

    IonSpew(IonSpew_InlineCaches, "Generated native GETPROP stub at %p", code->raw());

    return true;
}

static bool
IsCacheableProtoChain(JSObject *obj, JSObject *holder)
{
    while (obj != holder) {
        /*
         * We cannot assume that we find the holder object on the prototype
         * chain and must check for null proto. The prototype chain can be
         * altered during the lookupProperty call.
         */
        JSObject *proto = obj->getProto();
        if (!proto || !proto->isNative())
            return false;
        obj = proto;
    }
    return true;
}

static bool
IsCacheableGetProp(JSObject *obj, JSObject *holder, const Shape *shape)
{
    return (shape &&
            IsCacheableProtoChain(obj, holder) &&
            shape->hasSlot() &&
            shape->hasDefaultGetter());
}

bool
js::ion::GetPropertyCache(JSContext *cx, size_t cacheIndex, JSObject *obj, Value *vp)
{
    JSScript *script = GetTopIonJSScript(cx);
    IonScript *ion = script->ionScript();

    IonCacheGetProperty &cache = ion->getCache(cacheIndex).toGetProperty();
    JSAtom *atom = cache.atom();

    // Override the return value if we are invalidated (bug 728188).
    AutoDetectInvalidation adi(cx, vp, script);

    // For now, just stop generating new stubs once we hit the stub count
    // limit. Once we can make calls from within generated stubs, a new call
    // stub will be generated instead and the previous stubs unlinked.
    if (cache.stubCount() < MAX_STUBS && obj->isNative()) {
        cache.incrementStubCount();

        jsid id = ATOM_TO_JSID(atom);
        id = js_CheckForStringIndex(id);

        JSObject *holder;
        JSProperty *prop;
        if (!obj->lookupProperty(cx, atom->asPropertyName(), &holder, &prop))
            return false;

        const Shape *shape = (const Shape *)prop;
        if (IsCacheableGetProp(obj, holder, shape)) {
            if (!cache.attachNative(cx, obj, holder, shape))
                return false;
        }
    }

    if (!obj->getGeneric(cx, obj, ATOM_TO_JSID(atom), vp))
        return false;

    {
        JSScript *script;
        jsbytecode *pc;
        cache.getScriptedLocation(&script, &pc);
        types::TypeScript::Monitor(cx, script, pc, *vp);
    }

    return true;
}

void
IonCache::updateBaseAddress(IonCode *code, MacroAssembler &masm)
{
    initialJump_.repoint(code, &masm);
    lastJump_.repoint(code, &masm);
    cacheLabel_.repoint(code, &masm);
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

        masm.loadPtr(Address(object(), JSObject::offsetOfSlots()), slotsReg);

        Address addr(slotsReg, obj->dynamicSlotIndex(shape->slot()) * sizeof(Value));
        masm.storeConstantOrRegister(value(), addr);
    }

    Label rejoin_;
    CodeOffsetJump rejoinOffset = masm.jumpWithPatch(&rejoin_);
    masm.bind(&rejoin_);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    CodeLocationJump rejoinJump(code, rejoinOffset);
    CodeLocationJump exitJump(code, exitOffset);

    PatchJump(lastJump(), CodeLocationLabel(code));
    PatchJump(rejoinJump, rejoinLabel());
    PatchJump(exitJump, cacheLabel());
    updateLastJump(exitJump);

    IonSpew(IonSpew_InlineCaches, "Generated native SETPROP setting case stub at %p", code->raw());

    return true;
}

bool
IonCacheSetProperty::attachNativeAdding(JSContext *cx, JSObject *obj, const Shape *oldShape, const Shape *newShape,
                                        const Shape *propShape)
{
    MacroAssembler masm;

    Label failures;

    /* Guard shapes along prototype chain. */
    masm.branchTestObjShape(Assembler::NotEqual, object(), oldShape, &failures);

    Label protoFailures;
    masm.push(object());    // save object reg because we clobber it

    JSObject *proto = obj->getProto();
    Register protoReg = object();
    while (proto) {
        Shape *protoShape = proto->lastProperty();

        // load next prototype
        masm.loadPtr(Address(protoReg, JSObject::offsetOfType()), protoReg);
        masm.loadPtr(Address(protoReg, offsetof(types::TypeObject, proto)), protoReg);

        // ensure that the prototype is not NULL and that its shape matches
        masm.branchTestPtr(Assembler::Zero, protoReg, protoReg, &protoFailures);
        masm.branchTestObjShape(Assembler::NotEqual, protoReg, protoShape, &protoFailures);

        proto = proto->getProto();
    }

    masm.pop(object());     // restore object reg

    /* Changing object shape.  Write the object's new shape. */
    masm.storePtr(ImmGCPtr(newShape), Address(object(), JSObject::offsetOfShape()));

    /* Set the value on the object. */
    if (obj->isFixedSlot(propShape->slot())) {
        Address addr(object(), JSObject::getFixedSlotOffset(propShape->slot()));
        masm.storeConstantOrRegister(value(), addr);
    } else {
        Register slotsReg = object();

        masm.loadPtr(Address(object(), JSObject::offsetOfSlots()), slotsReg);

        Address addr(slotsReg, obj->dynamicSlotIndex(propShape->slot()) * sizeof(Value));
        masm.storeConstantOrRegister(value(), addr);
    }

    /* Success. */
    Label rejoin_;
    CodeOffsetJump rejoinOffset = masm.jumpWithPatch(&rejoin_);
    masm.bind(&rejoin_);

    /* Failure. */
    masm.bind(&protoFailures);
    masm.pop(object());
    masm.bind(&failures);

    Label exit_;
    CodeOffsetJump exitOffset = masm.jumpWithPatch(&exit_);
    masm.bind(&exit_);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    CodeLocationJump rejoinJump(code, rejoinOffset);
    CodeLocationJump exitJump(code, exitOffset);

    PatchJump(lastJump(), CodeLocationLabel(code));
    PatchJump(rejoinJump, rejoinLabel());
    PatchJump(exitJump, cacheLabel());
    updateLastJump(exitJump);

    IonSpew(IonSpew_InlineCaches, "Generated native SETPROP adding case stub at %p", code->raw());

    return true;
}

static bool
IsEligibleForInlinePropertyAdd(JSContext *cx, JSObject *obj, jsid propId, uint32_t oldSlots,
                               const Shape **propShapeOut)
{
    const Shape *propShape = obj->nativeLookup(cx, propId);
    if (!propShape || propShape->inDictionary() || !propShape->hasSlot() || !propShape->hasDefaultSetter())
        return false;

    // If object has a non-default resolve hook, don't inline
    if (obj->getClass()->resolve != JS_ResolveStub)
        return false;

    // walk up the object prototype chain and ensure that all prototypes
    // are native, and that all prototypes have no getter or setter
    // defined on the property
    for (JSObject *proto = obj->getProto(); proto; proto = proto->getProto()) {
        // if prototype is non-native, don't optimize
        if (!proto->isNative()) 
            return false;

        // if prototype defines this property in a non-plain way, don't optimize
        const Shape *protoShape = proto->nativeLookup(cx, propId);
        if (protoShape) {
            // If the prototype has a property with this name, we may be able to
            // inline-add if the property has a default setter.  If there's no
            // default setter, then setting would potentially cause special
            // behaviour, so we choose not to optimize.
            if (protoShape->hasDefaultSetter()) {
                *propShapeOut = propShape;
                return true;
            }
            return false;
        }

        // Otherise, if there's no such property, watch out for a resolve hook that would need
        // to be invoked and thus prevent inlining of property addition.
        if (proto->getClass()->resolve != JS_ResolveStub)
            return false;
    }

    // Only add a IC entry if the dynamic slots didn't change when the shapes
    // changed.  Need to ensure that a shape change for a subsequent object
    // won't involve reallocating the slot array.
    if (obj->numDynamicSlots() != oldSlots)
        return false;

    *propShapeOut = propShape;

    return true;
}

bool
js::ion::SetPropertyCache(JSContext *cx, size_t cacheIndex, JSObject *obj, const Value& value)
{
    IonScript *ion = GetTopIonJSScript(cx)->ion;
    IonCacheSetProperty &cache = ion->getCache(cacheIndex).toSetProperty();
    JSAtom *atom = cache.atom();
    Value v = value;
    // Stop generating new stubs once we hit the stub count limit, see
    // GetPropertyCache.
    if (cache.stubCount() < MAX_STUBS &&
        obj->isNative() &&
        !obj->watched())
    {
        cache.incrementStubCount();

        jsid id = ATOM_TO_JSID(atom);
        id = js_CheckForStringIndex(id);

        const Shape *shape = obj->nativeLookup(cx, id);
        if (shape && shape->hasSlot() && shape->hasDefaultSetter()) {
            if (!cache.attachNativeExisting(cx, obj, shape))
                return false;
        } else if (!shape) {
            uint32_t oldSlots = obj->numDynamicSlots();
            const Shape *oldShape = obj->lastProperty();

            // try adding slot manually and seeing if it produces a usable shape
            // for inlining via IC.
            if (!obj->setGeneric(cx, ATOM_TO_JSID(atom), &v, cache.strict()))
                return false;

            const Shape *propShape = NULL;
            if (IsEligibleForInlinePropertyAdd(cx, obj, id, oldSlots, &propShape)) {
                const Shape *newShape = obj->lastProperty();
                if (!cache.attachNativeAdding(cx, obj, oldShape, newShape, propShape))
                    return false;
            }

            // Return true here because we've already done the setGeneric.  Don't want
            // to fall through and call setGeneric again (both for performance reasons,
            // and because the double-set may be non-idempotent).
            return true;
        }
    }

    return obj->setGeneric(cx, ATOM_TO_JSID(atom), &v, cache.strict());
}

bool
IonCacheGetElement::attachGetProp(JSContext *cx, JSObject *obj, const Value &idval, PropertyName *name,
                                  Value *res)
{
    JSObject *holder;
    JSProperty *prop;
    if (!obj->lookupProperty(cx, name, &holder, &prop))
        return false;

    const Shape *shape = (const Shape *)prop;
    if (!IsCacheableGetProp(obj, holder, shape)) {
        IonSpew(IonSpew_InlineCaches, "GETELEM uncacheable property");
        return true;
    }

    JS_ASSERT(idval.isString());

    Label failures;
    MacroAssembler masm;

    // Guard on the index value.
    ValueOperand val = index().reg().valueReg();
    masm.branchTestValue(Assembler::NotEqual, val, idval, &failures);

    GetNativePropertyStub getprop;
    getprop.generate(cx, masm, obj, holder, shape, object(), output(), &failures);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    CodeLocationJump rejoinJump(code, getprop.rejoinOffset);
    CodeLocationJump exitJump(code, getprop.exitOffset);

    PatchJump(lastJump(), CodeLocationLabel(code));
    PatchJump(rejoinJump, rejoinLabel());
    PatchJump(exitJump, cacheLabel());
    updateLastJump(exitJump);

    IonSpew(IonSpew_InlineCaches, "Generated GETELEM property stub at %p", code->raw());
    return true;
}

// Get the common shape used by all dense arrays with a prototype at globalObj.
static inline Shape *
GetDenseArrayShape(JSContext *cx, JSObject *globalObj)
{
    JSObject *proto = globalObj->global().getOrCreateArrayPrototype(cx);
    if (!proto)
        return NULL;

    return EmptyShape::getInitialShape(cx, &ArrayClass, proto,
                                       proto->getParent(), gc::FINALIZE_OBJECT0);
}

bool
IonCacheGetElement::attachDenseArray(JSContext *cx, JSObject *obj, const Value &idval, Value *res)
{
    JS_ASSERT(obj->isDenseArray());
    JS_ASSERT(idval.isInt32());

    Label failures;
    MacroAssembler masm;

    // Guard object is a dense array.
    Shape *shape = GetDenseArrayShape(cx, script->global());
    if (!shape)
        return false;
    masm.branchTestObjShape(Assembler::NotEqual, object(), shape, &failures);

    // Ensure the index is an int32 value.
    ValueOperand val = index().reg().valueReg();
    masm.branchTestInt32(Assembler::NotEqual, val, &failures);

    // Load elements vector.
    masm.push(object());
    masm.loadPtr(Address(object(), JSObject::offsetOfElements()), object());

    // Unbox the index.
    ValueOperand out = output().valueReg();
    Register scratchReg = out.scratchReg();
    masm.unboxInt32(val, scratchReg);

    Label hole;

    // Guard on the initialized length.
    Address initLength(object(), ObjectElements::offsetOfInitializedLength());
    masm.branch32(Assembler::BelowOrEqual, initLength, scratchReg, &hole);

    // Load the value.
    masm.loadValue(BaseIndex(object(), scratchReg, TimesEight), out);

    // Hole check.
    masm.branchTestMagic(Assembler::Equal, out, &hole);

    masm.pop(object());
    Label rejoin_;
    CodeOffsetJump rejoinOffset = masm.jumpWithPatch(&rejoin_);
    masm.bind(&rejoin_);

    // All failures flow to here.
    masm.bind(&hole);
    masm.pop(object());
    masm.bind(&failures);

    Label exit_;
    CodeOffsetJump exitOffset = masm.jumpWithPatch(&exit_);
    masm.bind(&exit_);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    CodeLocationJump rejoinJump(code, rejoinOffset);
    CodeLocationJump exitJump(code, exitOffset);

    PatchJump(lastJump(), CodeLocationLabel(code));
    PatchJump(rejoinJump, rejoinLabel());
    PatchJump(exitJump, cacheLabel());
    updateLastJump(exitJump);

    setHasDenseArrayStub();
    IonSpew(IonSpew_InlineCaches, "Generated GETELEM dense array stub at %p", code->raw());

    return true;
}

bool
js::ion::GetElementCache(JSContext *cx, size_t cacheIndex, JSObject *obj, const Value &idval, Value *res)
{
    JSScript *script = GetTopIonJSScript(cx);
    IonScript *ion = script->ionScript();

    IonCacheGetElement &cache = ion->getCache(cacheIndex).toGetElement();

    // Override the return value if we are invalidated (bug 728188).
    AutoDetectInvalidation adi(cx, res, script);

    jsid id;
    if (!FetchElementId(cx, obj, idval, id, res))
        return false;

    if (cache.stubCount() < MAX_STUBS) {

        if (obj->isNative() && cache.monitoredResult()) {
            cache.incrementStubCount();

            uint32_t dummy;
            if (idval.isString() && JSID_IS_ATOM(id) && !JSID_TO_ATOM(id)->isIndex(&dummy)) {
                if (!cache.attachGetProp(cx, obj, idval, JSID_TO_ATOM(id)->asPropertyName(), res))
                    return false;
            }
        } else if (!cache.hasDenseArrayStub() && obj->isDenseArray() && idval.isInt32()) {
            // Generate at most one dense array stub.
            cache.incrementStubCount();

            if (!cache.attachDenseArray(cx, obj, idval, res))
                return false;
        }
    }

    JSScript *script_;
    jsbytecode *pc;
    cache.getScriptedLocation(&script_, &pc);

    if (!GetElementOperation(cx, JSOp(*pc), ObjectValue(*obj), idval, res))
        return false;

    types::TypeScript::Monitor(cx, script_, pc, *res);
    return true;
}

bool
IonCacheBindName::attachGlobal(JSContext *cx, JSObject *scopeChain)
{
    JS_ASSERT(scopeChain->isGlobal());

    MacroAssembler masm;

    // Guard on the scope chain.
    Label exit_;
    CodeOffsetJump exitOffset = masm.branchPtrWithPatch(Assembler::NotEqual, scopeChainReg(),
                                                        ImmGCPtr(scopeChain), &exit_);
    masm.bind(&exit_);
    masm.movePtr(ImmGCPtr(scopeChain), outputReg());

    Label rejoin_;
    CodeOffsetJump rejoinOffset = masm.jumpWithPatch(&rejoin_);
    masm.bind(&rejoin_);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    CodeLocationJump rejoinJump(code, rejoinOffset);
    CodeLocationJump exitJump(code, exitOffset);

    PatchJump(lastJump(), CodeLocationLabel(code));
    PatchJump(rejoinJump, rejoinLabel());
    PatchJump(exitJump, cacheLabel());
    updateLastJump(exitJump);

    IonSpew(IonSpew_InlineCaches, "Generated BINDNAME global stub at %p", code->raw());
    return true;
}

static void
GenerateScopeChainGuards(MacroAssembler &masm, JSObject *scopeChain, JSObject *holder,
                         Register scopeChainReg, Register outputReg, Label *failures)
{
    JS_ASSERT(scopeChain != holder);

    // Load the parent of the scope object into outputReg.
    JSObject *tobj = &scopeChain->asScope().enclosingScope();
    masm.extractObject(Address(scopeChainReg, ScopeObject::offsetOfEnclosingScope()), outputReg);

    // Walk up the scope chain. Note that IsCacheableScopeChain guarantees the
    // |tobj == holder| condition terminates the loop.
    while (true) {
        JS_ASSERT(IsCacheableNonGlobalScope(tobj));

        // Test intervening shapes.
        Address shapeAddr(outputReg, JSObject::offsetOfShape());
        masm.branchPtr(Assembler::NotEqual, shapeAddr, ImmGCPtr(tobj->lastProperty()), failures);
        if (tobj == holder)
            break;

        // Load the next link.
        tobj = &tobj->asScope().enclosingScope();
        masm.extractObject(Address(outputReg, ScopeObject::offsetOfEnclosingScope()), outputReg);
    }
}

bool
IonCacheBindName::attachNonGlobal(JSContext *cx, JSObject *scopeChain, JSObject *holder)
{
    JS_ASSERT(IsCacheableNonGlobalScope(scopeChain));

    MacroAssembler masm;

    // Guard on the shape of the scope chain.
    Label failures;
    CodeOffsetJump exitOffset =
        masm.branchPtrWithPatch(Assembler::NotEqual,
                                Address(scopeChainReg(), JSObject::offsetOfShape()),
                                ImmGCPtr(scopeChain->lastProperty()),
                                &failures);

    if (holder != scopeChain)
        GenerateScopeChainGuards(masm, scopeChain, holder, scopeChainReg(), outputReg(), &failures);
    else
        masm.movePtr(scopeChainReg(), outputReg());

    // At this point outputReg holds the object on which the property
    // was found, so we're done.
    Label rejoin_;
    CodeOffsetJump rejoinOffset = masm.jumpWithPatch(&rejoin_);
    masm.bind(&rejoin_);

    // All failures flow to here, so there is a common point to patch.
    masm.bind(&failures);
    if (holder != scopeChain) {
        Label exit_;
        exitOffset = masm.jumpWithPatch(&exit_);
        masm.bind(&exit_);
    }

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    CodeLocationJump rejoinJump(code, rejoinOffset);
    CodeLocationJump exitJump(code, exitOffset);

    PatchJump(lastJump(), CodeLocationLabel(code));
    PatchJump(rejoinJump, rejoinLabel());
    PatchJump(exitJump, cacheLabel());
    updateLastJump(exitJump);

    IonSpew(IonSpew_InlineCaches, "Generated BINDNAME non-global stub at %p", code->raw());
    return true;
}

static bool
IsCacheableScopeChain(JSObject *scopeChain, JSObject *holder)
{
    while (true) {
        if (!IsCacheableNonGlobalScope(scopeChain)) {
            IonSpew(IonSpew_InlineCaches, "Non-cacheable object on scope chain");
            return false;
        }

        if (scopeChain == holder)
            return true;

        scopeChain = &scopeChain->asScope().enclosingScope();
        if (!scopeChain) {
            IonSpew(IonSpew_InlineCaches, "Scope chain indirect hit");
            return false;
        }
    }

    JS_NOT_REACHED("Shouldn't get here");
    return false;
}

JSObject *
js::ion::BindNameCache(JSContext *cx, size_t cacheIndex, JSObject *scopeChain)
{
    IonScript *ion = GetTopIonJSScript(cx)->ionScript();
    IonCacheBindName &cache = ion->getCache(cacheIndex).toBindName();
    PropertyName *name = cache.name();

    JSObject *holder;
    if (scopeChain->isGlobal()) {
        holder = scopeChain;
    } else {
        holder = FindIdentifierBase(cx, scopeChain, name);
        if (!holder)
            return NULL;
    }

    // Stop generating new stubs once we hit the stub count limit, see
    // GetPropertyCache.
    if (cache.stubCount() < MAX_STUBS) {
        cache.incrementStubCount();

        if (scopeChain->isGlobal()) {
            if (!cache.attachGlobal(cx, scopeChain))
                return NULL;
        } else if (IsCacheableScopeChain(scopeChain, holder)) {
            if (!cache.attachNonGlobal(cx, scopeChain, holder))
                return NULL;
        } else {
            IonSpew(IonSpew_InlineCaches, "BINDNAME uncacheable scope chain");
        }
    }

    return holder;
}
