/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsscope.h"

#include "CodeGenerator.h"
#include "Ion.h"
#include "IonCaches.h"
#include "IonLinker.h"
#include "IonSpewer.h"
#include "VMFunctions.h"

#include "jsinterpinlines.h"

#include "vm/Stack.h"
#include "IonFrames-inl.h"

using namespace js;
using namespace js::ion;

void
CodeLocationJump::repoint(IonCode *code, MacroAssembler *masm)
{
    JS_ASSERT(!absolute_);
    size_t new_off = (size_t)raw_;
#ifdef JS_SMALL_BRANCH
    size_t jumpTableEntryOffset = reinterpret_cast<size_t>(jumpTableEntry_);
#endif
    if (masm != NULL) {
#ifdef JS_CPU_X64
        JS_ASSERT((uint64_t)raw_ <= UINT32_MAX);
#endif
        new_off = masm->actualOffset((uintptr_t)raw_);
#ifdef JS_SMALL_BRANCH
        jumpTableEntryOffset = masm->actualIndex(jumpTableEntryOffset);
#endif
    }
    raw_ = code->raw() + new_off;
#ifdef JS_SMALL_BRANCH
    jumpTableEntry_ = Assembler::PatchableJumpAddress(code, (size_t) jumpTableEntryOffset);
#endif
    absolute_ = true;
}

void
CodeLocationLabel::repoint(IonCode *code, MacroAssembler *masm)
{
     JS_ASSERT(!absolute_);
     size_t new_off = (size_t)raw_;
     if (masm != NULL) {
#ifdef JS_CPU_X64
        JS_ASSERT((uint64_t)raw_ <= UINT32_MAX);
#endif
        new_off = masm->actualOffset((uintptr_t)raw_);
     }
     JS_ASSERT(new_off < code->instructionsSize());

     raw_ = code->raw() + new_off;
     absolute_ = true;
}

void
CodeOffsetLabel::fixup(MacroAssembler *masm)
{
     offset_ = masm->actualOffset(offset_);
}

void
CodeOffsetJump::fixup(MacroAssembler *masm)
{
     offset_ = masm->actualOffset(offset_);
#ifdef JS_SMALL_BRANCH
     jumpTableIndex_ = masm->actualIndex(jumpTableIndex_);
#endif
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
                  RepatchLabel *failures, Label *nonRepatchFailures = NULL)
    {
        // If there's a single jump to |failures|, we can patch the shape guard
        // jump directly. Otherwise, jump to the end of the stub, so there's a
        // common point to patch.
        bool multipleFailureJumps = (nonRepatchFailures != NULL) && nonRepatchFailures->used();
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

        RepatchLabel rejoin_;
        rejoinOffset = masm.jumpWithPatch(&rejoin_);
        masm.bind(&rejoin_);

        if (obj != holder || multipleFailureJumps) {
            masm.bind(&prototypeFailures);
            if (restoreScratch)
                masm.pop(scratchReg);
            masm.bind(failures);
            if (multipleFailureJumps)
                masm.bind(nonRepatchFailures);
            RepatchLabel exit_;
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
    RepatchLabel failures;

    GetNativePropertyStub getprop;
    getprop.generate(cx, masm, obj, holder, shape, object(), output(), &failures);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    getprop.rejoinOffset.fixup(&masm);
    getprop.exitOffset.fixup(&masm);

    CodeLocationJump rejoinJump(code, getprop.rejoinOffset);
    CodeLocationJump exitJump(code, getprop.exitOffset);
    CodeLocationJump lastJump_ = lastJump();
    PatchJump(lastJump_, CodeLocationLabel(code));
    PatchJump(rejoinJump, rejoinLabel());
    PatchJump(exitJump, cacheLabel());
    updateLastJump(exitJump);

    IonSpew(IonSpew_InlineCaches, "Generated native GETPROP stub at %p %s", code->raw(),
            idempotent() ? "(idempotent)" : "(not idempotent)");

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

static bool
TryAttachNativeStub(JSContext *cx, IonCacheGetProperty &cache, HandleObject obj,
                    HandlePropertyName name, bool *isCacheableNative)
{
    JS_ASSERT(!*isCacheableNative);

    if (!obj->isNative())
        return true;

    // If the cache is idempotent, watch out for resolve hooks or non-native
    // objects on the proto chain. We check this before calling lookupProperty,
    // to make sure no effectful lookup hooks or resolve hooks are called.
    if (cache.idempotent() && !obj->hasIdempotentProtoChain())
        return true;

    RootedShape shape(cx);
    RootedObject holder(cx);
    if (!obj->lookupProperty(cx, name, &holder, &shape))
        return false;

    if (!IsCacheableGetProp(obj, holder, shape))
        return true;

    *isCacheableNative = true;

    if (cache.stubCount() < MAX_STUBS) {
        cache.incrementStubCount();

        if (!cache.attachNative(cx, obj, holder, shape))
            return false;
    }

    return true;
}

bool
js::ion::GetPropertyCache(JSContext *cx, size_t cacheIndex, HandleObject obj, MutableHandleValue vp)
{
    JSScript *topScript = GetTopIonJSScript(cx);
    IonScript *ion = topScript->ionScript();

    IonCacheGetProperty &cache = ion->getCache(cacheIndex).toGetProperty();
    RootedPropertyName name(cx, cache.name());

    JSScript *script;
    jsbytecode *pc;
    cache.getScriptedLocation(&script, &pc);

    // Override the return value if we are invalidated (bug 728188).
    AutoDetectInvalidation adi(cx, vp.address(), ion);

    // If the cache is idempotent, we will redo the op in the interpreter.
    if (cache.idempotent())
        adi.disable();

    // For now, just stop generating new stubs once we hit the stub count
    // limit. Once we can make calls from within generated stubs, a new call
    // stub will be generated instead and the previous stubs unlinked.
    bool isCacheableNative = false;
    if (!TryAttachNativeStub(cx, cache, obj, name, &isCacheableNative))
        return false;

    if (cache.idempotent() && !isCacheableNative) {
        // Invalidate the cache if the property was not found, or was found on
        // a non-native object. This ensures:
        // 1) The property read has no observable side-effects.
        // 2) There's no need to dynamically monitor the return type. This would
        //    be complicated since (due to GVN) there can be multiple pc's
        //    associated with a single idempotent cache.
        IonSpew(IonSpew_InlineCaches, "Invalidating from idempotent cache %s:%d",
                topScript->filename, topScript->lineno);

        topScript->invalidatedIdempotentCache = true;

        // Do not re-invalidate if the lookup already caused invalidation.
        if (!topScript->hasIonScript())
            return true;

        return Invalidate(cx, topScript);
    }

    RootedId id(cx, NameToId(name));
    if (obj->getOps()->getProperty) {
        if (!GetPropertyGenericMaybeCallXML(cx, JSOp(*pc), obj, id, vp))
            return false;
    } else {
        if (!GetPropertyHelper(cx, obj, id, 0, vp))
            return false;
    }

    if (!cache.idempotent()) {
        // If the cache is idempotent, the property exists so we don't have to
        // call __noSuchMethod__.

#if JS_HAS_NO_SUCH_METHOD
        // Handle objects with __noSuchMethod__.
        if (JSOp(*pc) == JSOP_CALLPROP && JS_UNLIKELY(vp.isPrimitive())) {
            if (!OnUnknownMethod(cx, obj, IdToValue(id), vp))
                return false;
        }
#endif

        // Monitor changes to cache entry.
        types::TypeScript::Monitor(cx, script, pc, vp);
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

void
IonCache::reset()
{
    PatchJump(initialJump_, cacheLabel_);

    this->stubCount_ = 0;
    this->lastJump_ = initialJump_;
}

bool
IonCacheSetProperty::attachNativeExisting(JSContext *cx, JSObject *obj, const Shape *shape)
{
    MacroAssembler masm;

    RepatchLabel exit_;
    CodeOffsetJump exitOffset =
        masm.branchPtrWithPatch(Assembler::NotEqual,
                                Address(object(), JSObject::offsetOfShape()),
                                ImmGCPtr(obj->lastProperty()),
                                &exit_);
    masm.bind(&exit_);

    if (obj->isFixedSlot(shape->slot())) {
        Address addr(object(), JSObject::getFixedSlotOffset(shape->slot()));

        if (cx->compartment->needsBarrier())
            masm.callPreBarrier(addr, MIRType_Value);

        masm.storeConstantOrRegister(value(), addr);
    } else {
        Register slotsReg = object();
        masm.loadPtr(Address(object(), JSObject::offsetOfSlots()), slotsReg);

        Address addr(slotsReg, obj->dynamicSlotIndex(shape->slot()) * sizeof(Value));

        if (cx->compartment->needsBarrier())
            masm.callPreBarrier(addr, MIRType_Value);

        masm.storeConstantOrRegister(value(), addr);
    }

    RepatchLabel rejoin_;
    CodeOffsetJump rejoinOffset = masm.jumpWithPatch(&rejoin_);
    masm.bind(&rejoin_);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    rejoinOffset.fixup(&masm);
    exitOffset.fixup(&masm);

    CodeLocationJump rejoinJump(code, rejoinOffset);
    CodeLocationJump exitJump(code, exitOffset);
    CodeLocationJump lastJump_ = lastJump();
    PatchJump(lastJump_, CodeLocationLabel(code));
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
    RepatchLabel rejoin_;
    CodeOffsetJump rejoinOffset = masm.jumpWithPatch(&rejoin_);
    masm.bind(&rejoin_);

    /* Failure. */
    masm.bind(&protoFailures);
    masm.pop(object());
    masm.bind(&failures);

    RepatchLabel exit_;
    CodeOffsetJump exitOffset = masm.jumpWithPatch(&exit_);
    masm.bind(&exit_);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    rejoinOffset.fixup(&masm);
    exitOffset.fixup(&masm);

    CodeLocationJump rejoinJump(code, rejoinOffset);
    CodeLocationJump exitJump(code, exitOffset);
    CodeLocationJump lastJump_ = lastJump();
    PatchJump(lastJump_, CodeLocationLabel(code));
    PatchJump(rejoinJump, rejoinLabel());
    PatchJump(exitJump, cacheLabel());
    updateLastJump(exitJump);

    IonSpew(IonSpew_InlineCaches, "Generated native SETPROP adding case stub at %p", code->raw());

    return true;
}

static bool
IsPropertyInlineable(JSObject *obj, IonCacheSetProperty &cache)
{
    // Stop generating new stubs once we hit the stub count limit, see
    // GetPropertyCache.
    if (cache.stubCount() >= MAX_STUBS)
        return false;

    if (!obj->isNative())
        return false;

    if (obj->watched())
        return false;

    return true;
}

static bool
IsPropertySetInlineable(JSContext *cx, HandleObject obj, JSAtom *atom, jsid *pId, const Shape **pShape)
{
    jsid id = AtomToId(atom);
    *pId = id;

    const Shape *shape = obj->nativeLookup(cx, id);
    *pShape = shape;

    if (!shape)
        return false;

    if (!shape->hasSlot())
        return false;

    if (!shape->hasDefaultSetter())
        return false;

    if (!shape->writable())
        return false;

    return true;
}

static bool
IsPropertyAddInlineable(JSContext *cx, HandleObject obj, jsid id, uint32_t oldSlots,
                       const Shape **pShape)
{
    // This is not a Add, the property exists.
    if (*pShape)
        return false;

    const Shape *shape = obj->nativeLookup(cx, id);
    if (!shape || shape->inDictionary() || !shape->hasSlot() || !shape->hasDefaultSetter())
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
        const Shape *protoShape = proto->nativeLookup(cx, id);
        if (protoShape && !protoShape->hasDefaultSetter())
            return false;

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

    *pShape = shape;
    return true;
}

bool
js::ion::SetPropertyCache(JSContext *cx, size_t cacheIndex, HandleObject obj, HandleValue value,
                          bool isSetName)
{
    IonScript *ion = GetTopIonJSScript(cx)->ion;
    IonCacheSetProperty &cache = ion->getCache(cacheIndex).toSetProperty();
    RootedPropertyName name(cx, cache.name());
    jsid id;
    const Shape *shape = NULL;

    bool inlinable = IsPropertyInlineable(obj, cache);
    if (inlinable && IsPropertySetInlineable(cx, obj, name, &id, &shape)) {
        cache.incrementStubCount();
        if (!cache.attachNativeExisting(cx, obj, shape))
            return false;
    }

    uint32_t oldSlots = obj->numDynamicSlots();
    const Shape *oldShape = obj->lastProperty();

    // Set/Add the property on the object, the inlined cache are setup for the next execution.
    if (!SetProperty(cx, obj, name, value, cache.strict(), isSetName))
        return false;

    // The property did not exists before, now we can try again to inline the
    // procedure which is adding the property.
    if (inlinable && IsPropertyAddInlineable(cx, obj, id, oldSlots, &shape)) {
        const Shape *newShape = obj->lastProperty();
        cache.incrementStubCount();
        if (!cache.attachNativeAdding(cx, obj, oldShape, newShape, shape))
            return false;
    }

    return true;
}

bool
IonCacheGetElement::attachGetProp(JSContext *cx, JSObject *obj, const Value &idval, PropertyName *name)
{
    RootedObject holder(cx);
    RootedShape shape(cx);
    if (!obj->lookupProperty(cx, name, &holder, &shape))
        return false;

    if (!IsCacheableGetProp(obj, holder, shape)) {
        IonSpew(IonSpew_InlineCaches, "GETELEM uncacheable property");
        return true;
    }

    JS_ASSERT(idval.isString());

    RepatchLabel failures;
    Label nonRepatchFailures;
    MacroAssembler masm;

    // Guard on the index value.
    ValueOperand val = index().reg().valueReg();
    masm.branchTestValue(Assembler::NotEqual, val, idval, &nonRepatchFailures);

    GetNativePropertyStub getprop;
    getprop.generate(cx, masm, obj, holder, shape, object(), output(), &failures, &nonRepatchFailures);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    getprop.rejoinOffset.fixup(&masm);
    getprop.exitOffset.fixup(&masm);

    CodeLocationJump rejoinJump(code, getprop.rejoinOffset);
    CodeLocationJump exitJump(code, getprop.exitOffset);
    CodeLocationJump lastJump_ = lastJump();
    PatchJump(lastJump_, CodeLocationLabel(code));
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
IonCacheGetElement::attachDenseArray(JSContext *cx, JSObject *obj, const Value &idval)
{
    JS_ASSERT(obj->isDenseArray());
    JS_ASSERT(idval.isInt32());

    Label failures;
    MacroAssembler masm;

    // Guard object is a dense array.
    RootedShape shape(cx, GetDenseArrayShape(cx, &script->global()));
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
    RepatchLabel rejoin_;
    CodeOffsetJump rejoinOffset = masm.jumpWithPatch(&rejoin_);
    masm.bind(&rejoin_);

    // All failures flow to here.
    masm.bind(&hole);
    masm.pop(object());
    masm.bind(&failures);

    RepatchLabel exit_;
    CodeOffsetJump exitOffset = masm.jumpWithPatch(&exit_);
    masm.bind(&exit_);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    rejoinOffset.fixup(&masm);
    exitOffset.fixup(&masm);

    CodeLocationJump rejoinJump(code, rejoinOffset);
    CodeLocationJump exitJump(code, exitOffset);
    CodeLocationJump lastJump_ = lastJump();
    PatchJump(lastJump_, CodeLocationLabel(code));
    PatchJump(rejoinJump, rejoinLabel());
    PatchJump(exitJump, cacheLabel());
    updateLastJump(exitJump);

    setHasDenseArrayStub();
    IonSpew(IonSpew_InlineCaches, "Generated GETELEM dense array stub at %p", code->raw());

    return true;
}

bool
js::ion::GetElementCache(JSContext *cx, size_t cacheIndex, JSObject *obj, HandleValue idval,
                         MutableHandleValue res)
{
    IonScript *ion = GetTopIonJSScript(cx)->ionScript();

    IonCacheGetElement &cache = ion->getCache(cacheIndex).toGetElement();

    // Override the return value if we are invalidated (bug 728188).
    AutoDetectInvalidation adi(cx, res.address(), ion);

    RootedId id(cx);
    if (!FetchElementId(cx, obj, idval, id.address(), res))
        return false;

    if (cache.stubCount() < MAX_STUBS) {
        if (obj->isNative() && cache.monitoredResult()) {
            cache.incrementStubCount();

            uint32_t dummy;
            if (idval.isString() && JSID_IS_ATOM(id) && !JSID_TO_ATOM(id)->isIndex(&dummy)) {
                if (!cache.attachGetProp(cx, obj, idval, JSID_TO_ATOM(id)->asPropertyName()))
                    return false;
            }
        } else if (!cache.hasDenseArrayStub() && obj->isDenseArray() && idval.isInt32()) {
            // Generate at most one dense array stub.
            cache.incrementStubCount();

            if (!cache.attachDenseArray(cx, obj, idval))
                return false;
        }
    }

    JSScript *script;
    jsbytecode *pc;
    cache.getScriptedLocation(&script, &pc);

    RootedValue lval(cx, ObjectValue(*obj));
    if (!GetElementOperation(cx, JSOp(*pc), lval, idval, res))
        return false;

    types::TypeScript::Monitor(cx, script, pc, res);
    return true;
}

bool
IonCacheBindName::attachGlobal(JSContext *cx, JSObject *scopeChain)
{
    JS_ASSERT(scopeChain->isGlobal());

    MacroAssembler masm;

    // Guard on the scope chain.
    RepatchLabel exit_;
    CodeOffsetJump exitOffset = masm.branchPtrWithPatch(Assembler::NotEqual, scopeChainReg(),
                                                        ImmGCPtr(scopeChain), &exit_);
    masm.bind(&exit_);
    masm.movePtr(ImmGCPtr(scopeChain), outputReg());

    RepatchLabel rejoin_;
    CodeOffsetJump rejoinOffset = masm.jumpWithPatch(&rejoin_);
    masm.bind(&rejoin_);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    rejoinOffset.fixup(&masm);
    exitOffset.fixup(&masm);

    CodeLocationJump rejoinJump(code, rejoinOffset);
    CodeLocationJump exitJump(code, exitOffset);
    CodeLocationJump lastJump_ = lastJump();
    PatchJump(lastJump_, CodeLocationLabel(code));
    PatchJump(rejoinJump, rejoinLabel());
    PatchJump(exitJump, cacheLabel());
    updateLastJump(exitJump);

    IonSpew(IonSpew_InlineCaches, "Generated BINDNAME global stub at %p", code->raw());
    return true;
}

static inline void
GenerateScopeChainGuard(MacroAssembler &masm, JSObject *scopeObj,
                        Register scopeObjReg, Shape *shape, Label *failures)
{
    if (scopeObj->isCall()) {
        // We can skip a guard on the call object if the script's bindings are
        // guaranteed to be immutable (and thus cannot introduce shadowing
        // variables).
        CallObject *callObj = &scopeObj->asCall();
        if (!callObj->isForEval()) {
            JSFunction *fun = &callObj->callee();
            JSScript *script = fun->script();
            if (!script->bindings.extensibleParents() && !script->funHasExtensibleScope)
                return;
        }
    } else if (scopeObj->isGlobal()) {
        // If this is the last object on the scope walk, and the property we've
        // found is not configurable, then we don't need a shape guard because
        // the shape cannot be removed.
        if (shape && !shape->configurable())
            return;
    }

    Address shapeAddr(scopeObjReg, JSObject::offsetOfShape());
    masm.branchPtr(Assembler::NotEqual, shapeAddr, ImmGCPtr(scopeObj->lastProperty()), failures);
}

static void
GenerateScopeChainGuards(MacroAssembler &masm, JSObject *scopeChain, JSObject *holder,
                         Register outputReg, Label *failures)
{
    JSObject *tobj = scopeChain;

    // Walk up the scope chain. Note that IsCacheableScopeChain guarantees the
    // |tobj == holder| condition terminates the loop.
    while (true) {
        JS_ASSERT(IsCacheableNonGlobalScope(tobj) || tobj->isGlobal());

        GenerateScopeChainGuard(masm, tobj, outputReg, NULL, failures);
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
    RepatchLabel failures;
    Label nonRepatchFailures;
    CodeOffsetJump exitOffset =
        masm.branchPtrWithPatch(Assembler::NotEqual,
                                Address(scopeChainReg(), JSObject::offsetOfShape()),
                                ImmGCPtr(scopeChain->lastProperty()),
                                &failures);

    if (holder != scopeChain) {
        JSObject *parent = &scopeChain->asScope().enclosingScope();
        masm.extractObject(Address(scopeChainReg(), ScopeObject::offsetOfEnclosingScope()), outputReg());

        GenerateScopeChainGuards(masm, parent, holder, outputReg(), &nonRepatchFailures);
    } else {
        masm.movePtr(scopeChainReg(), outputReg());
    }

    // At this point outputReg holds the object on which the property
    // was found, so we're done.
    RepatchLabel rejoin_;
    CodeOffsetJump rejoinOffset = masm.jumpWithPatch(&rejoin_);
    masm.bind(&rejoin_);

    // All failures flow to here, so there is a common point to patch.
    masm.bind(&failures);
    masm.bind(&nonRepatchFailures);
    if (holder != scopeChain) {
        RepatchLabel exit_;
        exitOffset = masm.jumpWithPatch(&exit_);
        masm.bind(&exit_);
    }

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    rejoinOffset.fixup(&masm);
    exitOffset.fixup(&masm);

    CodeLocationJump rejoinJump(code, rejoinOffset);
    CodeLocationJump exitJump(code, exitOffset);
    CodeLocationJump lastJump_ = lastJump();
    PatchJump(lastJump_, CodeLocationLabel(code));
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
js::ion::BindNameCache(JSContext *cx, size_t cacheIndex, HandleObject scopeChain)
{
    IonScript *ion = GetTopIonJSScript(cx)->ionScript();
    IonCacheBindName &cache = ion->getCache(cacheIndex).toBindName();
    HandlePropertyName name = cache.name();

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

bool
IonCacheName::attach(JSContext *cx, HandleObject scopeChain, HandleObject holder, Shape *shape)
{
    MacroAssembler masm;
    Label failures;

    Register scratchReg = outputReg().valueReg().scratchReg();

    masm.mov(scopeChainReg(), scratchReg);
    GenerateScopeChainGuards(masm, scopeChain, holder, scratchReg, &failures);

    unsigned slot = shape->slot();
    if (holder->isFixedSlot(slot)) {
        Address addr(scratchReg, JSObject::getFixedSlotOffset(slot));
        masm.loadTypedOrValue(addr, outputReg());
    } else {
        masm.loadPtr(Address(scratchReg, JSObject::offsetOfSlots()), scratchReg);

        Address addr(scratchReg, holder->dynamicSlotIndex(slot) * sizeof(Value));
        masm.loadTypedOrValue(addr, outputReg());
    }

    RepatchLabel rejoin;
    CodeOffsetJump rejoinOffset = masm.jumpWithPatch(&rejoin);
    masm.bind(&rejoin);

    CodeOffsetJump exitOffset;

    if (failures.used()) {
        masm.bind(&failures);

        RepatchLabel exit;
        exitOffset = masm.jumpWithPatch(&exit);
        masm.bind(&exit);
    }

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    rejoinOffset.fixup(&masm);
    if (failures.bound())
        exitOffset.fixup(&masm);

    CodeLocationJump rejoinJump(code, rejoinOffset);
    CodeLocationJump lastJump_ = lastJump();
    PatchJump(lastJump_, CodeLocationLabel(code));
    PatchJump(rejoinJump, rejoinLabel());
    if (failures.bound()) {
        CodeLocationJump exitJump(code, exitOffset);
        PatchJump(exitJump, cacheLabel());
        updateLastJump(exitJump);
    }

    IonSpew(IonSpew_InlineCaches, "Generated NAME stub at %p", code->raw());
    return true;
}

static bool
IsCacheableName(JSContext *cx, HandleObject scopeChain, HandleObject obj, HandleObject holder,
                HandleShape shape)
{
    if (!shape)
        return false;
    if (!obj->isNative())
        return false;
    if (obj != holder)
        return false;

    if (obj->isGlobal()) {
        // Support only simple property lookups.
        if (!IsCacheableGetProp(obj, holder, shape))
            return false;
    } else if (obj->isCall()) {
        if (!shape->hasDefaultGetter())
            return false;
    } else {
        // We don't yet support lookups on Block or DeclEnv objects.
        return false;
    }

    RootedObject obj2(cx, scopeChain);
    while (obj2) {
        if (!IsCacheableNonGlobalScope(obj2) && !obj2->isGlobal())
            return false;

        // Stop once we hit the global or target obj.
        if (obj2->isGlobal() || obj2 == obj)
            break;

        obj2 = obj2->enclosingScope();
    }

    return obj == obj2;
}

bool
js::ion::GetNameCache(JSContext *cx, size_t cacheIndex, HandleObject scopeChain, MutableHandleValue vp)
{
    IonScript *ion = GetTopIonJSScript(cx)->ionScript();

    IonCacheName &cache = ion->getCache(cacheIndex).toName();
    RootedPropertyName name(cx, cache.name());

    JSScript *script;
    jsbytecode *pc;
    cache.getScriptedLocation(&script, &pc);

    RootedObject obj(cx);
    RootedObject holder(cx);
    RootedShape shape(cx);
    if (!FindProperty(cx, name, scopeChain, &obj, &holder, &shape))
        return false;

    if (cache.stubCount() < MAX_STUBS &&
        IsCacheableName(cx, scopeChain, obj, holder, shape))
    {
        if (!cache.attach(cx, scopeChain, obj, shape))
            return false;
        cache.incrementStubCount();
    }

    if (cache.isTypeOf()) {
        if (!FetchName<true>(cx, obj, holder, name, shape, vp))
            return false;
    } else {
        if (!FetchName<false>(cx, obj, holder, name, shape, vp))
            return false;
    }

    // Monitor changes to cache entry.
    types::TypeScript::Monitor(cx, script, pc, vp);

    return true;
}

