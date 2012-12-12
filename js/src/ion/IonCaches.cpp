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

using mozilla::DebugOnly;

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
CodeLocationCall::repoint(IonCode *code, MacroAssembler *masm)
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

static bool
IsCacheableListBase(JSObject *obj)
{
    if (!obj->isProxy())
        return false;

    BaseProxyHandler *handler = GetProxyHandler(obj);

    if (handler->family() != GetListBaseHandlerFamily())
        return false;

    if (obj->numFixedSlots() <= GetListBaseExpandoSlot())
        return false;

    return true;
}

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

    JSObject *pobj = IsCacheableListBase(obj)
                     ? obj->getTaggedProto().toObjectOrNull()
                     : obj->getProto();
    if (!pobj)
        return;
    while (pobj != holder) {
        if (pobj->hasUncacheableProto()) {
            JS_ASSERT(!pobj->hasSingletonType());
            masm.movePtr(ImmGCPtr(pobj), scratchReg);
            Address objType(scratchReg, JSObject::offsetOfType());
            masm.branchPtr(Assembler::NotEqual, objType, ImmGCPtr(pobj->type()), failures);
        }
        pobj = pobj->getProto();
    }
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
        JSObject *proto = IsCacheableListBase(obj)
                     ? obj->getTaggedProto().toObjectOrNull()
                     : obj->getProto();
        if (!proto || !proto->isNative())
            return false;
        obj = proto;
    }
    return true;
}

static bool
IsCacheableGetPropReadSlot(JSObject *obj, JSObject *holder, const Shape *shape)
{
    if (!shape || !IsCacheableProtoChain(obj, holder))
        return false;

    if (!shape->hasSlot() || !shape->hasDefaultGetter())
        return false;

    return true;
}

static bool
IsCacheableNoProperty(JSObject *obj, JSObject *holder, const Shape *shape, jsbytecode *pc,
                      const TypedOrValueRegister &output)
{
    if (shape)
        return false;

    JS_ASSERT(!holder);

    // Just because we didn't find the property on the object doesn't mean it
    // won't magically appear through various engine hacks:
    if (obj->getClass()->getProperty && obj->getClass()->getProperty != JS_PropertyStub)
        return false;

    // Don't generate missing property ICs if we skipped a non-native object, as
    // lookups may extend beyond the prototype chain (e.g.  for ListBase
    // proxies).
    JSObject *obj2 = obj;
    while (obj2) {
        if (!obj2->isNative())
            return false;
        obj2 = obj2->getProto();
    }

    // The pc is NULL if the cache is idempotent. We cannot share missing
    // properties between caches because TI can only try to prove that a type is
    // contained, but does not attempts to check if something does not exists.
    // So the infered type of getprop would be missing and would not contain
    // undefined, as expected for missing properties.
    if (!pc)
        return false;

#if JS_HAS_NO_SUCH_METHOD
    // The __noSuchMethod__ hook may substitute in a valid method.  Since,
    // if o.m is missing, o.m() will probably be an error, just mark all
    // missing callprops as uncacheable.
    if (JSOp(*pc) == JSOP_CALLPROP ||
        JSOp(*pc) == JSOP_CALLELEM)
    {
        return false;
    }
#endif

    // TI has not yet monitored an Undefined value. The fallback path will
    // monitor and invalidate the script.
    if (!output.hasValue())
        return false;

    return true;
}

static bool
IsCacheableGetPropCallNative(JSObject *obj, JSObject *holder, const Shape *shape)
{
    if (!shape || !IsCacheableProtoChain(obj, holder))
        return false;

    if (!shape->hasGetterValue() || !shape->getterValue().isObject())
        return false;

    return shape->getterValue().toObject().isFunction() &&
           shape->getterValue().toObject().toFunction()->isNative();
}

static bool
IsCacheableGetPropCallPropertyOp(JSObject *obj, JSObject *holder, const Shape *shape)
{
    if (!shape || !IsCacheableProtoChain(obj, holder))
        return false;

    if (shape->hasSlot() || shape->hasGetterValue() || shape->hasDefaultGetter())
        return false;

    return true;
}

struct GetNativePropertyStub
{
    CodeOffsetJump exitOffset;
    CodeOffsetJump rejoinOffset;
    CodeOffsetLabel stubCodePatchOffset;

    void generateReadSlot(JSContext *cx, MacroAssembler &masm, JSObject *obj, PropertyName *propName,
                          JSObject *holder, const Shape *shape, Register object, TypedOrValueRegister output,
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

        // Generate prototype guards.
        Register holderReg;
        if (obj != holder) {
            // Note: this may clobber the object register if it's used as scratch.
            GeneratePrototypeGuards(cx, masm, obj, holder, object, scratchReg, &prototypeFailures);

            if (holder) {
                // Guard on the holder's shape.
                holderReg = scratchReg;
                masm.movePtr(ImmGCPtr(holder), holderReg);
                masm.branchPtr(Assembler::NotEqual,
                               Address(holderReg, JSObject::offsetOfShape()),
                               ImmGCPtr(holder->lastProperty()),
                               &prototypeFailures);
            } else {
                // The property does not exist. Guard on everything in the
                // prototype chain.
                JSObject *proto = obj->getTaggedProto().toObjectOrNull();
                Register lastReg = object;
                JS_ASSERT(scratchReg != object);
                while (proto) {
                    Address addrType(lastReg, JSObject::offsetOfType());
                    masm.loadPtr(addrType, scratchReg);
                    Address addrProto(scratchReg, offsetof(types::TypeObject, proto));
                    masm.loadPtr(addrProto, scratchReg);
                    Address addrShape(scratchReg, JSObject::offsetOfShape());

                    // Guard the shape of the current prototype.
                    masm.branchPtr(Assembler::NotEqual,
                                   Address(scratchReg, JSObject::offsetOfShape()),
                                   ImmGCPtr(proto->lastProperty()),
                                   &prototypeFailures);

                    proto = proto->getProto();
                    lastReg = scratchReg;
                }

                holderReg = InvalidReg;
            }
        } else {
            holderReg = object;
        }

        // Slot access.
        if (holder && holder->isFixedSlot(shape->slot())) {
            Address addr(holderReg, JSObject::getFixedSlotOffset(shape->slot()));
            masm.loadTypedOrValue(addr, output);
        } else if (holder) {
            masm.loadPtr(Address(holderReg, JSObject::offsetOfSlots()), scratchReg);

            Address addr(scratchReg, holder->dynamicSlotIndex(shape->slot()) * sizeof(Value));
            masm.loadTypedOrValue(addr, output);
        } else {
            JS_ASSERT(!holder);
            masm.moveValue(UndefinedValue(), output.valueReg());
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

    bool generateCallGetter(JSContext *cx, MacroAssembler &masm, JSObject *obj,
                            PropertyName *propName, JSObject *holder, const Shape *shape,
                            RegisterSet &liveRegs, Register object, TypedOrValueRegister output,
                            void *returnAddr, jsbytecode *pc,
                            RepatchLabel *failures, Label *nonRepatchFailures = NULL)
    {
        // Initial shape check.
        Label stubFailure;
        masm.branchPtr(Assembler::NotEqual, Address(object, JSObject::offsetOfShape()),
                       ImmGCPtr(obj->lastProperty()), &stubFailure);

        // If this is a stub for a ListBase object, guard the following:
        //      1. The object is a ListBase.
        //      2. The object does not have expando properties, or has an expando
        //          which is known to not have the desired property.
        if (IsCacheableListBase(obj)) {
            Address handlerAddr(object, JSObject::getFixedSlotOffset(JSSLOT_PROXY_HANDLER));
            Address expandoAddr(object, JSObject::getFixedSlotOffset(GetListBaseExpandoSlot()));

            // Check that object is a ListBase.
            masm.branchPrivatePtr(Assembler::NotEqual, handlerAddr, ImmWord(GetProxyHandler(obj)), &stubFailure);

            // For the remaining code, we need to reserve some registers to load a value.
            // This is ugly, but unvaoidable.
            RegisterSet listBaseRegSet(RegisterSet::All());
            listBaseRegSet.take(AnyRegister(object));
            ValueOperand tempVal = listBaseRegSet.takeValueOperand();
            masm.pushValue(tempVal);

            Label failListBaseCheck;
            Label listBaseOk;

            Value expandoVal = obj->getFixedSlot(GetListBaseExpandoSlot());
            JSObject *expando = expandoVal.isObject() ? &(expandoVal.toObject()) : NULL;
            JS_ASSERT_IF(expando, expando->isNative() && expando->getProto() == NULL);

            masm.loadValue(expandoAddr, tempVal);
            if (expando && expando->nativeLookupNoAllocation(propName)) {
                // Reference object has an expando that doesn't define the name.
                // Check incoming object's expando and make sure it's an object.

                // If checkExpando is true, we'll temporarily use register(s) for a ValueOperand.
                // If we do that, we save the register(s) on stack before use and pop them
                // on both exit paths.

                masm.branchTestObject(Assembler::NotEqual, tempVal, &failListBaseCheck);
                masm.extractObject(tempVal, tempVal.scratchReg());
                masm.branchPtr(Assembler::Equal,
                               Address(tempVal.scratchReg(), JSObject::offsetOfShape()),
                               ImmGCPtr(expando->lastProperty()),
                               &listBaseOk);
            } else {
                // Reference object has no expando.  Check incoming object and ensure
                // it has no expando.
                masm.branchTestUndefined(Assembler::Equal, tempVal, &listBaseOk);
            }

            // Failure case: restore the tempVal registers and jump to failures.
            masm.bind(&failListBaseCheck);
            masm.popValue(tempVal);
            masm.jump(&stubFailure);

            // Success case: restore the tempval and proceed.
            masm.bind(&listBaseOk);
            masm.popValue(tempVal);
        }

        // Reserve scratch register for prototype guards.
        bool restoreScratch = false;
        Register scratchReg = Register::FromCode(0); // Quell compiler warning.

        // If we need a scratch register, use either an output register or the object
        // register (and restore it afterwards). After this point, we cannot jump
        // directly to |stubFailure| since we may still have to pop the object register.
        Label prototypeFailures;
        JS_ASSERT(output.hasValue());
        scratchReg = output.valueReg().scratchReg();

        // Note: this may clobber the object register if it's used as scratch.
        if (obj != holder)
            GeneratePrototypeGuards(cx, masm, obj, holder, object, scratchReg, &prototypeFailures);

        // Guard on the holder's shape.
        Register holderReg = scratchReg;
        masm.movePtr(ImmGCPtr(holder), holderReg);
        masm.branchPtr(Assembler::NotEqual,
                       Address(holderReg, JSObject::offsetOfShape()),
                       ImmGCPtr(holder->lastProperty()),
                       &prototypeFailures);

        if (restoreScratch)
            masm.pop(scratchReg);

        // Now we're good to go to invoke the native call.

        // saveLive()
        masm.PushRegsInMask(liveRegs);

        // Remaining registers should basically be free, but we need to use |object| still
        // so leave it alone.
        RegisterSet regSet(RegisterSet::All());
        regSet.take(AnyRegister(object));

        // This is a slower stub path, and we're going to be doing a call anyway.  Don't need
        // to try so hard to not use the stack.  Scratch regs are just taken from the register
        // set not including the input, current value saved on the stack, and restored when
        // we're done with it.
        scratchReg               = regSet.takeGeneral();
        Register argJSContextReg = regSet.takeGeneral();
        Register argUintNReg     = regSet.takeGeneral();
        Register argVpReg        = regSet.takeGeneral();

        // Shape has a getter function.
        bool callNative = IsCacheableGetPropCallNative(obj, holder, shape);
        JS_ASSERT_IF(!callNative, IsCacheableGetPropCallPropertyOp(obj, holder, shape));

        // TODO: ensure stack is aligned?
        DebugOnly<uint32_t> initialStack = masm.framePushed();

        Label success, exception;

        // Push the IonCode pointer for the stub we're generating.
        // WARNING:
        // WARNING: If IonCode ever becomes relocatable, the following code is incorrect.
        // WARNING: Note that we're not marking the pointer being pushed as an ImmGCPtr.
        // WARNING: This is not a marking issue since the stub IonCode won't be collected
        // WARNING: between the time it's called and when we get here, but it would fail
        // WARNING: if the IonCode object ever moved, since we'd be rooting a nonsense
        // WARNING: value here.
        // WARNING:
        stubCodePatchOffset = masm.PushWithPatch(ImmWord(uintptr_t(-1)));

        if (callNative) {
            JS_ASSERT(shape->hasGetterValue() && shape->getterValue().isObject() &&
                      shape->getterValue().toObject().isFunction());
            JSFunction *target = shape->getterValue().toObject().toFunction();

            JS_ASSERT(target);
            JS_ASSERT(target->isNative());

            // Native functions have the signature:
            //  bool (*)(JSContext *, unsigned, Value *vp)
            // Where vp[0] is space for an outparam, vp[1] is |this|, and vp[2] onward
            // are the function arguments.

            // Construct vp array:
            // Push object value for |this|
            masm.Push(TypedOrValueRegister(MIRType_Object, AnyRegister(object)));
            // Push callee/outparam.
            masm.Push(ObjectValue(*target));

            // Preload arguments into registers.
            masm.loadJSContext(argJSContextReg);
            masm.move32(Imm32(0), argUintNReg);
            masm.movePtr(StackPointer, argVpReg);

            if (!masm.buildOOLFakeExitFrame(returnAddr))
                return false;
            masm.enterFakeExitFrame(ION_FRAME_OOL_NATIVE_GETTER);

            // Construct and execute call.
            masm.setupUnalignedABICall(3, scratchReg);
            masm.passABIArg(argJSContextReg);
            masm.passABIArg(argUintNReg);
            masm.passABIArg(argVpReg);
            masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, target->native()));

            // Test for failure.
            masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, &exception);

            // Load the outparam vp[0] into output register(s).
            masm.loadValue(
                Address(StackPointer, IonOOLNativeGetterExitFrameLayout::offsetOfResult()),
                JSReturnOperand);
        } else {
            Register argObjReg       = argUintNReg;
            Register argIdReg        = regSet.takeGeneral();

            PropertyOp target = shape->getterOp();
            JS_ASSERT(target);
            // JSPropertyOp: JSBool fn(JSContext *cx, JSHandleObject obj, JSHandleId id, JSMutableHandleValue vp)

            // Push args on stack first so we can take pointers to make handles.
            masm.Push(UndefinedValue());
            masm.movePtr(StackPointer, argVpReg);

            // push canonical jsid from shape instead of propertyname.
            jsid propId;
            if (!shape->getUserId(cx, &propId))
                return false;
            masm.Push(propId, scratchReg);
            masm.movePtr(StackPointer, argIdReg);

            masm.Push(object);
            masm.movePtr(StackPointer, argObjReg);

            masm.loadJSContext(argJSContextReg);

            if (!masm.buildOOLFakeExitFrame(returnAddr))
                return false;
            masm.enterFakeExitFrame(ION_FRAME_OOL_PROPERTY_OP);

            // Make the call.
            masm.setupUnalignedABICall(4, scratchReg);
            masm.passABIArg(argJSContextReg);
            masm.passABIArg(argObjReg);
            masm.passABIArg(argIdReg);
            masm.passABIArg(argVpReg);
            masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, target));

            // Test for failure.
            masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, &exception);

            // Load the outparam vp[0] into output register(s).
            masm.loadValue(
                Address(StackPointer, IonOOLPropertyOpExitFrameLayout::offsetOfResult()),
                JSReturnOperand);
        }

        // If generating getter call stubs, then return type MUST have been generalized
        // to MIRType_Value.
        masm.jump(&success);

        // Handle exception case.
        masm.bind(&exception);
        masm.handleException();

        // Handle success case.
        masm.bind(&success);
        masm.storeCallResultValue(output);

        // The next instruction is removing the footer of the exit frame, so there
        // is no need for leaveFakeExitFrame.

        // Move the StackPointer back to its original location, unwinding the native exit frame.
        if (callNative)
            masm.adjustStack(IonOOLNativeGetterExitFrameLayout::Size());
        else
            masm.adjustStack(IonOOLPropertyOpExitFrameLayout::Size());
        JS_ASSERT(masm.framePushed() == initialStack);

        // restoreLive()
        masm.PopRegsInMask(liveRegs);

        // Rejoin jump.
        RepatchLabel rejoin_;
        rejoinOffset = masm.jumpWithPatch(&rejoin_);
        masm.bind(&rejoin_);

        // Exit jump.
        masm.bind(&prototypeFailures);
        if (restoreScratch)
            masm.pop(scratchReg);
        masm.bind(&stubFailure);
        if (nonRepatchFailures)
            masm.bind(nonRepatchFailures);
        RepatchLabel exit_;
        exitOffset = masm.jumpWithPatch(&exit_);
        masm.bind(&exit_);

        return true;
    }
};

bool
IonCacheGetProperty::attachReadSlot(JSContext *cx, IonScript *ion, JSObject *obj, JSObject *holder,
                                    const Shape *shape)
{
    MacroAssembler masm;
    RepatchLabel failures;

    GetNativePropertyStub getprop;
    getprop.generateReadSlot(cx, masm, obj, name(), holder, shape, object(), output(), &failures);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    getprop.rejoinOffset.fixup(&masm);
    getprop.exitOffset.fixup(&masm);

    if (ion->invalidated())
        return true;

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

bool
IonCacheGetProperty::attachCallGetter(JSContext *cx, IonScript *ion, JSObject *obj,
                                      JSObject *holder, const Shape *shape,
                                      const SafepointIndex *safepointIndex, void *returnAddr)
{
    MacroAssembler masm;
    RepatchLabel failures;

    JS_ASSERT(!idempotent());
    JS_ASSERT(allowGetters());

    // Need to set correct framePushed on the masm so that exit frame descriptors are
    // properly constructed.
    masm.setFramePushed(ion->frameSize());

    GetNativePropertyStub getprop;
    if (!getprop.generateCallGetter(cx, masm, obj, name(), holder, shape, liveRegs,
                                    object(), output(), returnAddr, pc, &failures))
    {
         return false;
    }

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    getprop.rejoinOffset.fixup(&masm);
    getprop.exitOffset.fixup(&masm);
    getprop.stubCodePatchOffset.fixup(&masm);

    if (ion->invalidated())
        return true;

    Assembler::patchDataWithValueCheck(CodeLocationLabel(code, getprop.stubCodePatchOffset),
                                       ImmWord(uintptr_t(code)), ImmWord(uintptr_t(-1)));

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
TryAttachNativeGetPropStub(JSContext *cx, IonScript *ion,
                           IonCacheGetProperty &cache, HandleObject obj,
                           HandlePropertyName name,
                           const SafepointIndex *safepointIndex,
                           void *returnAddr, bool *isCacheable)
{
    JS_ASSERT(!*isCacheable);

    RootedObject checkObj(cx, obj);
    bool isListBase = IsCacheableListBase(obj);
    if (isListBase)
        checkObj = obj->getTaggedProto().toObjectOrNull();

    if (!checkObj || !checkObj->isNative())
        return true;

    // If the cache is idempotent, watch out for resolve hooks or non-native
    // objects on the proto chain. We check this before calling lookupProperty,
    // to make sure no effectful lookup hooks or resolve hooks are called.
    if (cache.idempotent() && !checkObj->hasIdempotentProtoChain())
        return true;

    RootedShape shape(cx);
    RootedObject holder(cx);
    if (!JSObject::lookupProperty(cx, checkObj, name, &holder, &shape))
        return false;

    // Check what kind of cache stub we can emit: either a slot read,
    // or a getter call.
    bool readSlot = false;
    bool callGetter = false;

    RootedScript script(cx);
    jsbytecode *pc;
    cache.getScriptedLocation(&script, &pc);

    if (IsCacheableGetPropReadSlot(checkObj, holder, shape) ||
        IsCacheableNoProperty(checkObj, holder, shape, pc, cache.output()))
    {
        readSlot = true;
    } else if (IsCacheableGetPropCallNative(checkObj, holder, shape) ||
               IsCacheableGetPropCallPropertyOp(checkObj, holder, shape))
    {
        // Don't enable getter call if cache is idempotent, since
        // they can be effectful.
        if (!cache.idempotent() && cache.allowGetters())
            callGetter = true;
    }

    // Only continue if one of the cache methods is viable.
    if (!readSlot && !callGetter)
        return true;

    // TI infers the possible types of native object properties. There's one
    // edge case though: for singleton objects it does not add the initial
    // "undefined" type, see the propertySet comment in jsinfer.h. We can't
    // monitor the return type inside an idempotent cache though, so we don't
    // handle this case.
    if (cache.idempotent() &&
        holder &&
        holder->hasSingletonType() &&
        holder->getSlot(shape->slot()).isUndefined())
    {
        return true;
    }

    *isCacheable = true;

    // readSlot and callGetter are mutually exclusive
    JS_ASSERT_IF(readSlot, !callGetter);
    JS_ASSERT_IF(callGetter, !readSlot);

    if (cache.stubCount() < MAX_STUBS) {
        cache.incrementStubCount();

        if (readSlot)
            return cache.attachReadSlot(cx, ion, obj, holder, shape);
        else
            return cache.attachCallGetter(cx, ion, obj, holder, shape, safepointIndex, returnAddr);
    }

    return true;
}

bool
js::ion::GetPropertyCache(JSContext *cx, size_t cacheIndex, HandleObject obj, MutableHandleValue vp)
{
    AutoFlushCache afc ("GetPropertyCache");
    const SafepointIndex *safepointIndex;
    void *returnAddr;
    JSScript *topScript = GetTopIonJSScript(cx, &safepointIndex, &returnAddr);
    IonScript *ion = topScript->ionScript();

    IonCacheGetProperty &cache = ion->getCache(cacheIndex).toGetProperty();
    RootedPropertyName name(cx, cache.name());

    RootedScript script(cx);
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
    bool isCacheable = false;
    if (!TryAttachNativeGetPropStub(cx, ion, cache, obj, name,
                                    safepointIndex, returnAddr,
                                    &isCacheable))
    {
        return false;
    }

    if (cache.idempotent() && !isCacheable) {
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
IonCacheSetProperty::attachNativeExisting(JSContext *cx, IonScript *ion,
                                          HandleObject obj, HandleShape shape)
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

    if (ion->invalidated())
        return true;

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
IonCacheSetProperty::attachSetterCall(JSContext *cx, IonScript *ion,
                                      HandleObject obj, HandleObject holder, HandleShape shape,
                                      void *returnAddr)
{
    MacroAssembler masm;

    // Need to set correct framePushed on the masm so that exit frame descriptors are
    // properly constructed.
    masm.setFramePushed(ion->frameSize());

    Label failure;
    masm.branchPtr(Assembler::NotEqual,
                   Address(object(), JSObject::offsetOfShape()),
                   ImmGCPtr(obj->lastProperty()),
                   &failure);

    // Generate prototype guards if needed.
    // Take a scratch register for use, save on stack.
    {
        RegisterSet regSet(RegisterSet::All());
        regSet.take(AnyRegister(object()));
        if (!value().constant())
            regSet.maybeTake(value().reg());
        Register scratchReg = regSet.takeGeneral();
        masm.push(scratchReg);

        Label protoFailure;
        Label protoSuccess;

        // Generate prototype/shape guards.
        if (obj != holder)
            GeneratePrototypeGuards(cx, masm, obj, holder, object(), scratchReg, &protoFailure);

        masm.movePtr(ImmGCPtr(holder), scratchReg);
        masm.branchPtr(Assembler::NotEqual,
                       Address(scratchReg, JSObject::offsetOfShape()),
                       ImmGCPtr(holder->lastProperty()),
                       &protoFailure);

        masm.jump(&protoSuccess);

        masm.bind(&protoFailure);
        masm.pop(scratchReg);
        masm.jump(&failure);

        masm.bind(&protoSuccess);
        masm.pop(scratchReg);
    }

    // Good to go for invoking setter.

    // saveLive()
    masm.PushRegsInMask(liveRegs);

    // Remaining registers should basically be free, but we need to use |object| still
    // so leave it alone.
    RegisterSet regSet(RegisterSet::All());
    regSet.take(AnyRegister(object()));

    // This is a slower stub path, and we're going to be doing a call anyway.  Don't need
    // to try so hard to not use the stack.  Scratch regs are just taken from the register
    // set not including the input, current value saved on the stack, and restored when
    // we're done with it.
    Register scratchReg     = regSet.takeGeneral();
    Register argJSContextReg = regSet.takeGeneral();
    Register argObjReg       = regSet.takeGeneral();
    Register argIdReg        = regSet.takeGeneral();
    Register argStrictReg    = regSet.takeGeneral();
    Register argVpReg        = regSet.takeGeneral();

    // Ensure stack is aligned.
    DebugOnly<uint32_t> initialStack = masm.framePushed();

    Label success, exception;

    // Push the IonCode pointer for the stub we're generating.
    // WARNING:
    // WARNING: If IonCode ever becomes relocatable, the following code is incorrect.
    // WARNING: Note that we're not marking the pointer being pushed as an ImmGCPtr.
    // WARNING: This is not a marking issue since the stub IonCode won't be collected
    // WARNING: between the time it's called and when we get here, but it would fail
    // WARNING: if the IonCode object ever moved, since we'd be rooting a nonsense
    // WARNING: value here.
    // WARNING:
    CodeOffsetLabel stubCodePatchOffset = masm.PushWithPatch(ImmWord(uintptr_t(-1)));

    StrictPropertyOp target = shape->setterOp();
    JS_ASSERT(target);
    // JSStrictPropertyOp: JSBool fn(JSContext *cx, JSHandleObject obj,
    //                               JSHandleId id, JSBool strict, JSMutableHandleValue vp);

    // Push args on stack first so we can take pointers to make handles.
    if (value().constant())
        masm.Push(value().value());
    else
        masm.Push(value().reg());
    masm.movePtr(StackPointer, argVpReg);

    masm.move32(Imm32(strict() ? 1 : 0), argStrictReg);

    // push canonical jsid from shape instead of propertyname.
    jsid propId;
    if (!shape->getUserId(cx, &propId))
        return false;
    masm.Push(propId, argIdReg);
    masm.movePtr(StackPointer, argIdReg);

    masm.Push(object());
    masm.movePtr(StackPointer, argObjReg);

    masm.loadJSContext(argJSContextReg);

    if (!masm.buildOOLFakeExitFrame(returnAddr))
        return false;
    masm.enterFakeExitFrame(ION_FRAME_OOL_PROPERTY_OP);

    // Make the call.
    masm.setupUnalignedABICall(5, scratchReg);
    masm.passABIArg(argJSContextReg);
    masm.passABIArg(argObjReg);
    masm.passABIArg(argIdReg);
    masm.passABIArg(argStrictReg);
    masm.passABIArg(argVpReg);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, target));

    // Test for failure.
    masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, &exception);

    masm.jump(&success);

    // Handle exception case.
    masm.bind(&exception);
    masm.handleException();

    // Handle success case.
    masm.bind(&success);

    // The next instruction is removing the footer of the exit frame, so there
    // is no need for leaveFakeExitFrame.

    // Move the StackPointer back to its original location, unwinding the exit frame.
    masm.adjustStack(IonOOLPropertyOpExitFrameLayout::Size());
    JS_ASSERT(masm.framePushed() == initialStack);

    // restoreLive()
    masm.PopRegsInMask(liveRegs);

    // Rejoin jump.
    RepatchLabel rejoin;
    CodeOffsetJump rejoinOffset = masm.jumpWithPatch(&rejoin);
    masm.bind(&rejoin);

    // Exit jump.
    masm.bind(&failure);
    RepatchLabel exit;
    CodeOffsetJump exitOffset = masm.jumpWithPatch(&exit);
    masm.bind(&exit);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    rejoinOffset.fixup(&masm);
    exitOffset.fixup(&masm);
    stubCodePatchOffset.fixup(&masm);

    if (ion->invalidated())
        return true;

    Assembler::patchDataWithValueCheck(CodeLocationLabel(code, stubCodePatchOffset),
                                       ImmWord(uintptr_t(code)), ImmWord(uintptr_t(-1)));

    CodeLocationJump rejoinJump(code, rejoinOffset);
    CodeLocationJump exitJump(code, exitOffset);
    CodeLocationJump lastJump_ = lastJump();
    PatchJump(lastJump_, CodeLocationLabel(code));
    PatchJump(rejoinJump, rejoinLabel());
    PatchJump(exitJump, cacheLabel());
    updateLastJump(exitJump);

    IonSpew(IonSpew_InlineCaches, "Generated SETPROP calling case stub at %p", code->raw());

    return true;
}

bool
IonCacheSetProperty::attachNativeAdding(JSContext *cx, IonScript *ion, JSObject *obj,
                                        const Shape *oldShape, const Shape *newShape,
                                        const Shape *propShape)
{
    MacroAssembler masm;

    Label failures;

    /* Guard the type of the object */
    masm.branchPtr(Assembler::NotEqual, Address(object(), JSObject::offsetOfType()),
                   ImmGCPtr(obj->type()), &failures);

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
    Address shapeAddr(object(), JSObject::offsetOfShape());
    if (cx->compartment->needsBarrier())
        masm.callPreBarrier(shapeAddr, MIRType_Shape);
    masm.storePtr(ImmGCPtr(newShape), shapeAddr);

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

    if (ion->invalidated())
        return true;

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
IsPropertySetInlineable(JSContext *cx, HandleObject obj, HandleId id, MutableHandleShape pshape)
{
    Shape *shape = obj->nativeLookup(cx, id);

    if (!shape)
        return false;

    if (!shape->hasSlot())
        return false;

    if (!shape->hasDefaultSetter())
        return false;

    if (!shape->writable())
        return false;

    pshape.set(shape);

    return true;
}

static bool
IsPropertySetterCallInlineable(JSContext *cx, HandleObject obj, HandleObject holder,
                               jsid id, HandleShape shape)
{
    if (!shape)
        return false;

    if (shape->hasSlot())
        return false;

    if (shape->hasDefaultSetter())
        return false;

    if (!shape->writable())
        return false;

    // We only handle propertyOps for now, so fail if we have SetterValue
    // (which implies JSNative setter).
    if (shape->hasSetterValue())
        return false;

    return true;
}

static bool
IsPropertyAddInlineable(JSContext *cx, HandleObject obj, jsid id, uint32_t oldSlots,
                        MutableHandleShape pShape)
{
    // This is not a Add, the property exists.
    if (pShape.get())
        return false;

    Shape *shape = obj->nativeLookup(cx, id);
    if (!shape || shape->inDictionary() || !shape->hasSlot() || !shape->hasDefaultSetter())
        return false;

    // If object has a non-default resolve hook, don't inline
    if (obj->getClass()->resolve != JS_ResolveStub)
        return false;

    if (!obj->isExtensible() || !shape->writable())
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

    pShape.set(shape);
    return true;
}

bool
js::ion::SetPropertyCache(JSContext *cx, size_t cacheIndex, HandleObject obj, HandleValue value,
                          bool isSetName)
{
    AutoFlushCache afc ("SetPropertyCache");

    void *returnAddr;
    const SafepointIndex *safepointIndex;
    JSScript *script = GetTopIonJSScript(cx, &safepointIndex, &returnAddr);
    IonScript *ion = script->ion;
    IonCacheSetProperty &cache = ion->getCache(cacheIndex).toSetProperty();
    RootedPropertyName name(cx, cache.name());
    RootedId id(cx, AtomToId(name));
    RootedShape shape(cx);
    RootedObject holder(cx);

    bool inlinable = IsPropertyInlineable(obj, cache);
    if (inlinable) {
        RootedShape shape(cx);
        if (IsPropertySetInlineable(cx, obj, id, &shape)) {
            cache.incrementStubCount();
            if (!cache.attachNativeExisting(cx, ion, obj, shape))
                return false;
        } else {
            RootedObject holder(cx);
            if (!JSObject::lookupProperty(cx, obj, name, &holder, &shape))
                return false;

            if (IsPropertySetterCallInlineable(cx, obj, holder, id, shape)) {
                cache.incrementStubCount();
                if (!cache.attachSetterCall(cx, ion, obj, holder, shape, returnAddr))
                    return false;
            }
        }
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
        if (!cache.attachNativeAdding(cx, ion, obj, oldShape, newShape, shape))
            return false;
    }

    return true;
}

bool
IonCacheGetElement::attachGetProp(JSContext *cx, IonScript *ion, HandleObject obj,
                                  const Value &idval, PropertyName *name)
{
    RootedObject holder(cx);
    RootedShape shape(cx);
    if (!JSObject::lookupProperty(cx, obj, name, &holder, &shape))
        return false;

    RootedScript script(cx);
    jsbytecode *pc;
    getScriptedLocation(&script, &pc);

    if (!IsCacheableGetPropReadSlot(obj, holder, shape) &&
        !IsCacheableNoProperty(obj, holder, shape, pc, output())) {
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
    getprop.generateReadSlot(cx, masm, obj, name, holder, shape, object(), output(), &failures, &nonRepatchFailures);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    if (!code)
        return false;

    getprop.rejoinOffset.fixup(&masm);
    getprop.exitOffset.fixup(&masm);

    if (ion->invalidated())
        return true;

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

bool
IonCacheGetElement::attachDenseArray(JSContext *cx, IonScript *ion, JSObject *obj, const Value &idval)
{
    JS_ASSERT(obj->isDenseArray());
    JS_ASSERT(idval.isInt32());

    Label failures;
    MacroAssembler masm;

    // Guard object is a dense array.
    RootedObject globalObj(cx, &script->global());
    RootedShape shape(cx, GetDenseArrayShape(cx, globalObj));
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

    if (ion->invalidated())
        return true;

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
js::ion::GetElementCache(JSContext *cx, size_t cacheIndex, HandleObject obj, HandleValue idval,
                         MutableHandleValue res)
{
    AutoFlushCache afc ("GetElementCache");

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
                if (!cache.attachGetProp(cx, ion, obj, idval, JSID_TO_ATOM(id)->asPropertyName()))
                    return false;
            }
        } else if (!cache.hasDenseArrayStub() && obj->isDenseArray() && idval.isInt32()) {
            // Generate at most one dense array stub.
            cache.incrementStubCount();

            if (!cache.attachDenseArray(cx, ion, obj, idval))
                return false;
        }
    }

    RootedScript script(cx);
    jsbytecode *pc;
    cache.getScriptedLocation(&script, &pc);

    RootedValue lval(cx, ObjectValue(*obj));
    if (!GetElementOperation(cx, JSOp(*pc), lval, idval, res))
        return false;

    types::TypeScript::Monitor(cx, script, pc, res);
    return true;
}

bool
IonCacheBindName::attachGlobal(JSContext *cx, IonScript *ion, JSObject *scopeChain)
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

    if (ion->invalidated())
        return true;

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
    AutoAssertNoGC nogc;
    if (scopeObj->isCall()) {
        // We can skip a guard on the call object if the script's bindings are
        // guaranteed to be immutable (and thus cannot introduce shadowing
        // variables).
        CallObject *callObj = &scopeObj->asCall();
        if (!callObj->isForEval()) {
            RawFunction fun = &callObj->callee();
            UnrootedScript script = fun->nonLazyScript();
            if (!script->funHasExtensibleScope)
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
IonCacheBindName::attachNonGlobal(JSContext *cx, IonScript *ion, JSObject *scopeChain, JSObject *holder)
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

    if (ion->invalidated())
        return true;

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
    AutoFlushCache afc ("BindNameCache");

    IonScript *ion = GetTopIonJSScript(cx)->ionScript();
    IonCacheBindName &cache = ion->getCache(cacheIndex).toBindName();
    HandlePropertyName name = cache.name();

    RootedObject holder(cx);
    if (scopeChain->isGlobal()) {
        holder = scopeChain;
    } else {
        if (!LookupNameWithGlobalDefault(cx, name, scopeChain, &holder))
            return NULL;
    }

    // Stop generating new stubs once we hit the stub count limit, see
    // GetPropertyCache.
    if (cache.stubCount() < MAX_STUBS) {
        cache.incrementStubCount();

        if (scopeChain->isGlobal()) {
            if (!cache.attachGlobal(cx, ion, scopeChain))
                return NULL;
        } else if (IsCacheableScopeChain(scopeChain, holder)) {
            if (!cache.attachNonGlobal(cx, ion, scopeChain, holder))
                return NULL;
        } else {
            IonSpew(IonSpew_InlineCaches, "BINDNAME uncacheable scope chain");
        }
    }

    return holder;
}

bool
IonCacheName::attach(JSContext *cx, IonScript *ion, HandleObject scopeChain, HandleObject holder, Shape *shape)
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

    if (ion->invalidated())
        return true;

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
                HandleShape shape, jsbytecode *pc, const TypedOrValueRegister &output)
{
    if (!shape)
        return false;
    if (!obj->isNative())
        return false;
    if (obj != holder)
        return false;

    if (obj->isGlobal()) {
        // Support only simple property lookups.
        if (!IsCacheableGetPropReadSlot(obj, holder, shape) &&
            !IsCacheableNoProperty(obj, holder, shape, pc, output))
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
    AutoFlushCache afc ("GetNameCache");

    IonScript *ion = GetTopIonJSScript(cx)->ionScript();

    IonCacheName &cache = ion->getCache(cacheIndex).toName();
    RootedPropertyName name(cx, cache.name());

    RootedScript script(cx);
    jsbytecode *pc;
    cache.getScriptedLocation(&script, &pc);

    RootedObject obj(cx);
    RootedObject holder(cx);
    RootedShape shape(cx);
    if (!LookupName(cx, name, scopeChain, &obj, &holder, &shape))
        return false;

    if (cache.stubCount() < MAX_STUBS &&
        IsCacheableName(cx, scopeChain, obj, holder, shape, pc, cache.outputReg()))
    {
        if (!cache.attach(cx, ion, scopeChain, obj, shape))
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
