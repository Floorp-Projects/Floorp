/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/IonCaches.h"

#include "mozilla/DebugOnly.h"

#include "jit/CodeGenerator.h"
#include "jit/Ion.h"
#include "jit/IonLinker.h"
#include "jit/IonSpewer.h"
#include "jit/PerfSpewer.h"
#include "jit/VMFunctions.h"
#include "vm/Shape.h"

#include "vm/Interpreter-inl.h"

using namespace js;
using namespace js::ion;

using mozilla::DebugOnly;

void
CodeLocationJump::repoint(IonCode *code, MacroAssembler *masm)
{
    JS_ASSERT(state_ == Relative);
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
    setAbsolute();
}

void
CodeLocationLabel::repoint(IonCode *code, MacroAssembler *masm)
{
     JS_ASSERT(state_ == Relative);
     size_t new_off = (size_t)raw_;
     if (masm != NULL) {
#ifdef JS_CPU_X64
        JS_ASSERT((uint64_t)raw_ <= UINT32_MAX);
#endif
        new_off = masm->actualOffset((uintptr_t)raw_);
     }
     JS_ASSERT(new_off < code->instructionsSize());

     raw_ = code->raw() + new_off;
     setAbsolute();
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

const char *
IonCache::CacheName(IonCache::Kind kind)
{
    static const char * const names[] =
    {
#define NAME(x) #x,
        IONCACHE_KIND_LIST(NAME)
#undef NAME
    };
    return names[kind];
}

IonCache::LinkStatus
IonCache::linkCode(JSContext *cx, MacroAssembler &masm, IonScript *ion, IonCode **code)
{
    Linker linker(masm);
    *code = linker.newCode(cx, JSC::ION_CODE);
    if (!*code)
        return LINK_ERROR;

    if (ion->invalidated())
        return CACHE_FLUSHED;

    return LINK_GOOD;
}

const size_t IonCache::MAX_STUBS = 16;

// Helper class which encapsulates logic to attach a stub to an IC by hooking
// up rejoins and next stub jumps.
//
// The simplest stubs have a single jump to the next stub and look like the
// following:
//
//    branch guard NEXTSTUB
//    ... IC-specific code ...
//    jump REJOIN
//
// This corresponds to:
//
//    attacher.branchNextStub(masm, ...);
//    ... emit IC-specific code ...
//    attacher.jumpRejoin(masm);
//
// Whether the stub needs multiple next stub jumps look like:
//
//   branch guard FAILURES
//   ... IC-specific code ...
//   branch another-guard FAILURES
//   ... IC-specific code ...
//   jump REJOIN
//   FAILURES:
//   jump NEXTSTUB
//
// This corresponds to:
//
//   Label failures;
//   masm.branchX(..., &failures);
//   ... emit IC-specific code ...
//   masm.branchY(..., failures);
//   ... emit more IC-specific code ...
//   attacher.jumpRejoin(masm);
//   masm.bind(&failures);
//   attacher.jumpNextStub(masm);
//
// A convenience function |branchNextStubOrLabel| is provided in the case that
// the stub sometimes has multiple next stub jumps and sometimes a single
// one. If a non-NULL label is passed in, a |branchPtr| will be made to that
// label instead of a |branchPtrWithPatch| to the next stub.
class IonCache::StubAttacher
{
  protected:
    bool hasNextStubOffset_ : 1;
    bool hasStubCodePatchOffset_ : 1;

    CodeLocationLabel rejoinLabel_;
    CodeOffsetJump nextStubOffset_;
    CodeOffsetJump rejoinOffset_;
    CodeOffsetLabel stubCodePatchOffset_;

  public:
    StubAttacher(CodeLocationLabel rejoinLabel)
      : hasNextStubOffset_(false),
        hasStubCodePatchOffset_(false),
        rejoinLabel_(rejoinLabel),
        nextStubOffset_(),
        rejoinOffset_(),
        stubCodePatchOffset_()
    { }

    // Value used instead of the IonCode self-reference of generated
    // stubs. This value is needed for marking calls made inside stubs. This
    // value would be replaced by the attachStub function after the allocation
    // of the IonCode. The self-reference is used to keep the stub path alive
    // even if the IonScript is invalidated or if the IC is flushed.
    static const ImmWord STUB_ADDR;

    template <class T1, class T2>
    void branchNextStub(MacroAssembler &masm, Assembler::Condition cond, T1 op1, T2 op2) {
        JS_ASSERT(!hasNextStubOffset_);
        RepatchLabel nextStub;
        nextStubOffset_ = masm.branchPtrWithPatch(cond, op1, op2, &nextStub);
        hasNextStubOffset_ = true;
        masm.bind(&nextStub);
    }

    template <class T1, class T2>
    void branchNextStubOrLabel(MacroAssembler &masm, Assembler::Condition cond, T1 op1, T2 op2,
                               Label *label)
    {
        if (label != NULL)
            masm.branchPtr(cond, op1, op2, label);
        else
            branchNextStub(masm, cond, op1, op2);
    }

    void jumpRejoin(MacroAssembler &masm) {
        RepatchLabel rejoin;
        rejoinOffset_ = masm.jumpWithPatch(&rejoin);
        masm.bind(&rejoin);
    }

    void jumpNextStub(MacroAssembler &masm) {
        JS_ASSERT(!hasNextStubOffset_);
        RepatchLabel nextStub;
        nextStubOffset_ = masm.jumpWithPatch(&nextStub);
        hasNextStubOffset_ = true;
        masm.bind(&nextStub);
    }

    void pushStubCodePointer(MacroAssembler &masm) {
        // Push the IonCode pointer for the stub we're generating.
        // WARNING:
        // WARNING: If IonCode ever becomes relocatable, the following code is incorrect.
        // WARNING: Note that we're not marking the pointer being pushed as an ImmGCPtr.
        // WARNING: This location will be patched with the pointer of the generated stub,
        // WARNING: such as it can be marked when a call is made with this stub. Be aware
        // WARNING: that ICs are not marked and so this stub will only be kept alive iff
        // WARNING: it is on the stack at the time of the GC. No ImmGCPtr is needed as the
        // WARNING: stubs are flushed on GC.
        // WARNING:
        JS_ASSERT(!hasStubCodePatchOffset_);
        stubCodePatchOffset_ = masm.PushWithPatch(STUB_ADDR);
        hasStubCodePatchOffset_ = true;
    }

    void patchRejoinJump(MacroAssembler &masm, IonCode *code) {
        rejoinOffset_.fixup(&masm);
        CodeLocationJump rejoinJump(code, rejoinOffset_);
        PatchJump(rejoinJump, rejoinLabel_);
    }

    void patchStubCodePointer(MacroAssembler &masm, IonCode *code) {
        if (hasStubCodePatchOffset_) {
            stubCodePatchOffset_.fixup(&masm);
            Assembler::patchDataWithValueCheck(CodeLocationLabel(code, stubCodePatchOffset_),
                                               ImmWord(uintptr_t(code)), STUB_ADDR);
        }
    }

    virtual void patchNextStubJump(MacroAssembler &masm, IonCode *code) = 0;
};

const ImmWord IonCache::StubAttacher::STUB_ADDR = ImmWord(uintptr_t(0xdeadc0de));

class RepatchIonCache::RepatchStubAppender : public IonCache::StubAttacher
{
    RepatchIonCache &cache_;

  public:
    RepatchStubAppender(RepatchIonCache &cache)
      : StubAttacher(cache.rejoinLabel()),
        cache_(cache)
    {
    }

    void patchNextStubJump(MacroAssembler &masm, IonCode *code) {
        // Patch the previous nextStubJump of the last stub, or the jump from the
        // codeGen, to jump into the newly allocated code.
        PatchJump(cache_.lastJump_, CodeLocationLabel(code));

        // If this path is not taken, we are producing an entry which can no
        // longer go back into the update function.
        if (hasNextStubOffset_) {
            nextStubOffset_.fixup(&masm);
            CodeLocationJump nextStubJump(code, nextStubOffset_);
            PatchJump(nextStubJump, cache_.fallbackLabel_);

            // When the last stub fails, it fallback to the ool call which can
            // produce a stub. Next time we generate a stub, we will patch the
            // nextStub jump to try the new stub.
            cache_.lastJump_ = nextStubJump;
        }
    }
};

void
RepatchIonCache::reset()
{
    IonCache::reset();
    PatchJump(initialJump_, fallbackLabel_);
    lastJump_ = initialJump_;
}

void
RepatchIonCache::emitInitialJump(MacroAssembler &masm, AddCacheState &addState)
{
    initialJump_ = masm.jumpWithPatch(&addState.repatchEntry);
    lastJump_ = initialJump_;
}

void
RepatchIonCache::bindInitialJump(MacroAssembler &masm, AddCacheState &addState)
{
    masm.bind(&addState.repatchEntry);
}

void
RepatchIonCache::updateBaseAddress(IonCode *code, MacroAssembler &masm)
{
    IonCache::updateBaseAddress(code, masm);
    initialJump_.repoint(code, &masm);
    lastJump_.repoint(code, &masm);
}

class DispatchIonCache::DispatchStubPrepender : public IonCache::StubAttacher
{
    DispatchIonCache &cache_;

  public:
    DispatchStubPrepender(DispatchIonCache &cache)
      : StubAttacher(cache.rejoinLabel_),
        cache_(cache)
    {
    }

    void patchNextStubJump(MacroAssembler &masm, IonCode *code) {
        JS_ASSERT(hasNextStubOffset_);

        // Jump to the previous entry in the stub dispatch table. We
        // have not yet executed the code we're patching the jump in.
        nextStubOffset_.fixup(&masm);
        CodeLocationJump nextStubJump(code, nextStubOffset_);
        PatchJump(nextStubJump, CodeLocationLabel(cache_.firstStub_));

        // Update the dispatch table. Modification to jumps after the dispatch
        // table is updated is disallowed, lest we race on entry into an
        // unfinalized stub.
        cache_.firstStub_ = code->raw();
    }
};

void
DispatchIonCache::reset()
{
    IonCache::reset();
    firstStub_ = fallbackLabel_.raw();
}
void
DispatchIonCache::emitInitialJump(MacroAssembler &masm, AddCacheState &addState)
{
    Register scratch = addState.dispatchScratch;
    dispatchLabel_ = masm.movWithPatch(ImmWord(uintptr_t(-1)), scratch);
    masm.loadPtr(Address(scratch, 0), scratch);
    masm.jump(scratch);
    rejoinLabel_ = masm.labelForPatch();
}

void
DispatchIonCache::bindInitialJump(MacroAssembler &masm, AddCacheState &addState)
{
    // Do nothing.
}

void
DispatchIonCache::updateBaseAddress(IonCode *code, MacroAssembler &masm)
{
    // The address of firstStub_ should be pointer aligned.
    JS_ASSERT(uintptr_t(&firstStub_) % sizeof(uintptr_t) == 0);

    IonCache::updateBaseAddress(code, masm);
    dispatchLabel_.fixup(&masm);
    Assembler::patchDataWithValueCheck(CodeLocationLabel(code, dispatchLabel_),
                                       ImmWord(uintptr_t(&firstStub_)),
                                       ImmWord(uintptr_t(-1)));
    firstStub_ = fallbackLabel_.raw();
    rejoinLabel_.repoint(code, &masm);
}

void
IonCache::attachStub(MacroAssembler &masm, StubAttacher &attacher, Handle<IonCode *> code)
{
    JS_ASSERT(canAttachStub());
    incrementStubCount();

    // Update the success path to continue after the IC initial jump.
    attacher.patchRejoinJump(masm, code);

    // Update the failure path.
    attacher.patchNextStubJump(masm, code);

    // Replace the STUB_ADDR constant by the address of the generated stub, such
    // as it can be kept alive even if the cache is flushed (see
    // MarkIonExitFrame).
    attacher.patchStubCodePointer(masm, code);
}

bool
IonCache::linkAndAttachStub(JSContext *cx, MacroAssembler &masm, StubAttacher &attacher,
                            IonScript *ion, const char *attachKind)
{
    Rooted<IonCode *> code(cx);
    LinkStatus status = linkCode(cx, masm, ion, code.address());
    if (status != LINK_GOOD)
        return status != LINK_ERROR;

    attachStub(masm, attacher, code);

    if (pc) {
        IonSpew(IonSpew_InlineCaches, "Cache %p(%s:%d/%d) generated %s %s stub at %p",
                this, script->filename(), script->lineno, pc - script->code,
                attachKind, CacheName(kind()), code->raw());
    } else {
        IonSpew(IonSpew_InlineCaches, "Cache %p generated %s %s stub at %p",
                this, attachKind, CacheName(kind()), code->raw());
    }
    return true;
}

void
IonCache::updateBaseAddress(IonCode *code, MacroAssembler &masm)
{
    fallbackLabel_.repoint(code, &masm);
}

void
IonCache::initializeAddCacheState(LInstruction *ins, AddCacheState *addState)
{
}

static bool
IsCacheableDOMProxy(JSObject *obj)
{
    if (!obj->is<ProxyObject>())
        return false;

    BaseProxyHandler *handler = obj->as<ProxyObject>().handler();

    if (handler->family() != GetDOMProxyHandlerFamily())
        return false;

    if (obj->numFixedSlots() <= GetDOMProxyExpandoSlot())
        return false;

    return true;
}

static void
GeneratePrototypeGuards(JSContext *cx, IonScript *ion, MacroAssembler &masm, JSObject *obj,
                        JSObject *holder, Register objectReg, Register scratchReg,
                        Label *failures)
{
    /* The guards here protect against the effects of TradeGuts(). If the prototype chain
     * is directly altered, then TI will toss the jitcode, so we don't have to worry about
     * it, and any other change to the holder, or adding a shadowing property will result
     * in reshaping the holder, and thus the failure of the shape guard.
     */
    JS_ASSERT(obj != holder);

    if (obj->hasUncacheableProto()) {
        // Note: objectReg and scratchReg may be the same register, so we cannot
        // use objectReg in the rest of this function.
        masm.loadPtr(Address(objectReg, JSObject::offsetOfType()), scratchReg);
        Address proto(scratchReg, offsetof(types::TypeObject, proto));
        masm.branchNurseryPtr(Assembler::NotEqual, proto,
                              ImmMaybeNurseryPtr(obj->getProto()), failures);
    }

    JSObject *pobj = IsCacheableDOMProxy(obj)
                     ? obj->getTaggedProto().toObjectOrNull()
                     : obj->getProto();
    if (!pobj)
        return;
    while (pobj != holder) {
        if (pobj->hasUncacheableProto()) {
            JS_ASSERT(!pobj->hasSingletonType());
            masm.moveNurseryPtr(ImmMaybeNurseryPtr(pobj), scratchReg);
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
        JSObject *proto = obj->getProto();
        if (!proto || !proto->isNative())
            return false;
        obj = proto;
    }
    return true;
}

static bool
IsCacheableGetPropReadSlot(JSObject *obj, JSObject *holder, Shape *shape)
{
    if (!shape || !IsCacheableProtoChain(obj, holder))
        return false;

    if (!shape->hasSlot() || !shape->hasDefaultGetter())
        return false;

    return true;
}

static bool
IsCacheableNoProperty(JSObject *obj, JSObject *holder, Shape *shape, jsbytecode *pc,
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
    // lookups may extend beyond the prototype chain (e.g.  for DOMProxy
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
IsOptimizableArgumentsObjectForLength(JSObject *obj)
{
    if (!obj->is<ArgumentsObject>())
        return false;

    if (obj->as<ArgumentsObject>().hasOverriddenLength())
        return false;

    return true;
}

static bool
IsOptimizableArgumentsObjectForGetElem(JSObject *obj, Value idval)
{
    if (!IsOptimizableArgumentsObjectForLength(obj))
        return false;

    ArgumentsObject &argsObj = obj->as<ArgumentsObject>();

    if (argsObj.isAnyElementDeleted())
        return false;

    if (!idval.isInt32())
        return false;

    int32_t idint = idval.toInt32();
    if (idint < 0 || static_cast<uint32_t>(idint) >= argsObj.initialLength())
        return false;

    return true;
}

static bool
IsCacheableGetPropCallNative(JSObject *obj, JSObject *holder, Shape *shape)
{
    if (!shape || !IsCacheableProtoChain(obj, holder))
        return false;

    if (!shape->hasGetterValue() || !shape->getterValue().isObject())
        return false;

    return shape->getterValue().toObject().is<JSFunction>() &&
           shape->getterValue().toObject().as<JSFunction>().isNative();
}

static bool
IsCacheableGetPropCallPropertyOp(JSObject *obj, JSObject *holder, Shape *shape)
{
    if (!shape || !IsCacheableProtoChain(obj, holder))
        return false;

    if (shape->hasSlot() || shape->hasGetterValue() || shape->hasDefaultGetter())
        return false;

    return true;
}

static inline void
EmitLoadSlot(MacroAssembler &masm, JSObject *holder, Shape *shape, Register holderReg,
             TypedOrValueRegister output, Register scratchReg)
{
    JS_ASSERT(holder);
    if (holder->isFixedSlot(shape->slot())) {
        Address addr(holderReg, JSObject::getFixedSlotOffset(shape->slot()));
        masm.loadTypedOrValue(addr, output);
    } else {
        masm.loadPtr(Address(holderReg, JSObject::offsetOfSlots()), scratchReg);

        Address addr(scratchReg, holder->dynamicSlotIndex(shape->slot()) * sizeof(Value));
        masm.loadTypedOrValue(addr, output);
    }
}

static void
GenerateDOMProxyChecks(JSContext *cx, MacroAssembler &masm, JSObject *obj,
                       PropertyName *name, Register object, Label *stubFailure,
                       bool skipExpandoCheck = false)
{
    JS_ASSERT(IsCacheableDOMProxy(obj));

    // Guard the following:
    //      1. The object is a DOMProxy.
    //      2. The object does not have expando properties, or has an expando
    //          which is known to not have the desired property.
    Address handlerAddr(object, ProxyObject::offsetOfHandler());
    Address expandoSlotAddr(object, JSObject::getFixedSlotOffset(GetDOMProxyExpandoSlot()));

    // Check that object is a DOMProxy.
    masm.branchPrivatePtr(Assembler::NotEqual, handlerAddr,
                          ImmWord(obj->as<ProxyObject>().handler()), stubFailure);

    if (skipExpandoCheck)
        return;

    // For the remaining code, we need to reserve some registers to load a value.
    // This is ugly, but unvaoidable.
    RegisterSet domProxyRegSet(RegisterSet::All());
    domProxyRegSet.take(AnyRegister(object));
    ValueOperand tempVal = domProxyRegSet.takeValueOperand();
    masm.pushValue(tempVal);

    Label failDOMProxyCheck;
    Label domProxyOk;

    Value expandoVal = obj->getFixedSlot(GetDOMProxyExpandoSlot());
    masm.loadValue(expandoSlotAddr, tempVal);

    if (!expandoVal.isObject() && !expandoVal.isUndefined()) {
        masm.branchTestValue(Assembler::NotEqual, tempVal, expandoVal, &failDOMProxyCheck);

        ExpandoAndGeneration *expandoAndGeneration = (ExpandoAndGeneration*)expandoVal.toPrivate();
        masm.movePtr(ImmWord(expandoAndGeneration), tempVal.scratchReg());

        masm.branch32(Assembler::NotEqual, Address(tempVal.scratchReg(), sizeof(Value)),
                                                   Imm32(expandoAndGeneration->generation),
                                                   &failDOMProxyCheck);

        expandoVal = expandoAndGeneration->expando;
        masm.loadValue(Address(tempVal.scratchReg(), 0), tempVal);
    }

    // If the incoming object does not have an expando object then we're sure we're not
    // shadowing.
    masm.branchTestUndefined(Assembler::Equal, tempVal, &domProxyOk);

    if (expandoVal.isObject()) {
        JS_ASSERT(!expandoVal.toObject().nativeContains(cx, name));

        // Reference object has an expando object that doesn't define the name. Check that
        // the incoming object has an expando object with the same shape.
        masm.branchTestObject(Assembler::NotEqual, tempVal, &failDOMProxyCheck);
        masm.extractObject(tempVal, tempVal.scratchReg());
        masm.branchPtr(Assembler::Equal,
                       Address(tempVal.scratchReg(), JSObject::offsetOfShape()),
                       ImmGCPtr(expandoVal.toObject().lastProperty()),
                       &domProxyOk);
    }

    // Failure case: restore the tempVal registers and jump to failures.
    masm.bind(&failDOMProxyCheck);
    masm.popValue(tempVal);
    masm.jump(stubFailure);

    // Success case: restore the tempval and proceed.
    masm.bind(&domProxyOk);
    masm.popValue(tempVal);
}

static void
GenerateReadSlot(JSContext *cx, IonScript *ion, MacroAssembler &masm,
                 IonCache::StubAttacher &attacher, JSObject *obj, PropertyName *name,
                 JSObject *holder, Shape *shape, Register object, TypedOrValueRegister output,
                 Label *failures = NULL)
{
    JS_ASSERT(obj->isNative());
    // If there's a single jump to |failures|, we can patch the shape guard
    // jump directly. Otherwise, jump to the end of the stub, so there's a
    // common point to patch.
    bool multipleFailureJumps = (obj != holder) || (failures != NULL && failures->used());

    // If we have multiple failure jumps but didn't get a label from the
    // outside, make one ourselves.
    Label failures_;
    if (multipleFailureJumps && !failures)
        failures = &failures_;

    // Guard on the shape of the object.
    attacher.branchNextStubOrLabel(masm, Assembler::NotEqual,
                                   Address(object, JSObject::offsetOfShape()),
                                   ImmGCPtr(obj->lastProperty()),
                                   failures);

    // If we need a scratch register, use either an output register or the
    // object register. After this point, we cannot jump directly to
    // |failures| since we may still have to pop the object register.
    bool restoreScratch = false;
    Register scratchReg = Register::FromCode(0); // Quell compiler warning.

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

    // Fast path: single failure jump, no prototype guards.
    if (!multipleFailureJumps) {
        EmitLoadSlot(masm, holder, shape, object, output, scratchReg);
        if (restoreScratch)
            masm.pop(scratchReg);
        attacher.jumpRejoin(masm);
        return;
    }

    // Slow path: multiple jumps; generate prototype guards.
    Label prototypeFailures;
    Register holderReg;
    if (obj != holder) {
        // Note: this may clobber the object register if it's used as scratch.
        GeneratePrototypeGuards(cx, ion, masm, obj, holder, object, scratchReg,
                                &prototypeFailures);

        if (holder) {
            // Guard on the holder's shape.
            holderReg = scratchReg;
            masm.moveNurseryPtr(ImmMaybeNurseryPtr(holder), holderReg);
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
    if (holder)
        EmitLoadSlot(masm, holder, shape, holderReg, output, scratchReg);
    else
        masm.moveValue(UndefinedValue(), output.valueReg());

    // Restore scratch on success.
    if (restoreScratch)
        masm.pop(scratchReg);

    attacher.jumpRejoin(masm);

    masm.bind(&prototypeFailures);
    if (restoreScratch)
        masm.pop(scratchReg);
    masm.bind(failures);

    attacher.jumpNextStub(masm);

}

static bool
EmitGetterCall(JSContext *cx, MacroAssembler &masm,
               IonCache::StubAttacher &attacher, JSObject *obj,
               JSObject *holder, HandleShape shape,
               RegisterSet liveRegs, Register object,
               Register scratchReg, TypedOrValueRegister output,
               void *returnAddr)
{
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

    attacher.pushStubCodePointer(masm);

    if (callNative) {
        JS_ASSERT(shape->hasGetterValue() && shape->getterValue().isObject() &&
                  shape->getterValue().toObject().is<JSFunction>());
        JSFunction *target = &shape->getterValue().toObject().as<JSFunction>();

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
        masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

        // Load the outparam vp[0] into output register(s).
        masm.loadValue(
            Address(StackPointer, IonOOLNativeGetterExitFrameLayout::offsetOfResult()),
            JSReturnOperand);
    } else {
        Register argObjReg       = argUintNReg;
        Register argIdReg        = regSet.takeGeneral();

        PropertyOp target = shape->getterOp();
        JS_ASSERT(target);
        // JSPropertyOp: bool fn(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)

        // Push args on stack first so we can take pointers to make handles.
        masm.Push(UndefinedValue());
        masm.movePtr(StackPointer, argVpReg);

        // push canonical jsid from shape instead of propertyname.
        RootedId propId(cx);
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
        masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

        // Load the outparam vp[0] into output register(s).
        masm.loadValue(
            Address(StackPointer, IonOOLPropertyOpExitFrameLayout::offsetOfResult()),
            JSReturnOperand);
    }

    // If generating getter call stubs, then return type MUST have been generalized
    // to MIRType_Value.
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

    return true;
}

static bool
GenerateCallGetter(JSContext *cx, IonScript *ion, MacroAssembler &masm,
                   IonCache::StubAttacher &attacher, JSObject *obj, PropertyName *name,
                   JSObject *holder, HandleShape shape, RegisterSet &liveRegs, Register object,
                   TypedOrValueRegister output, void *returnAddr)
{
    JS_ASSERT(obj->isNative());
    // Initial shape check.
    Label stubFailure;
    masm.branchPtr(Assembler::NotEqual, Address(object, JSObject::offsetOfShape()),
                   ImmGCPtr(obj->lastProperty()), &stubFailure);

    JS_ASSERT(output.hasValue());
    Register scratchReg = output.valueReg().scratchReg();

    // Note: this may clobber the object register if it's used as scratch.
    if (obj != holder)
        GeneratePrototypeGuards(cx, ion, masm, obj, holder, object, scratchReg, &stubFailure);

    // Guard on the holder's shape.
    Register holderReg = scratchReg;
    masm.moveNurseryPtr(ImmMaybeNurseryPtr(holder), holderReg);
    masm.branchPtr(Assembler::NotEqual,
                   Address(holderReg, JSObject::offsetOfShape()),
                   ImmGCPtr(holder->lastProperty()),
                   &stubFailure);

    // Now we're good to go to invoke the native call.
    if (!EmitGetterCall(cx, masm, attacher, obj, holder, shape, liveRegs, object,
                        scratchReg, output, returnAddr))
        return false;

    // Rejoin jump.
    attacher.jumpRejoin(masm);

    // Jump to next stub.
    masm.bind(&stubFailure);
    attacher.jumpNextStub(masm);

    return true;
}

static bool
GenerateArrayLength(JSContext *cx, MacroAssembler &masm, IonCache::StubAttacher &attacher,
                    JSObject *obj, Register object, TypedOrValueRegister output)
{
    JS_ASSERT(obj->is<ArrayObject>());

    Label failures;

    // Guard object is a dense array.
    RootedShape shape(cx, obj->lastProperty());
    if (!shape)
        return false;
    masm.branchTestObjShape(Assembler::NotEqual, object, shape, &failures);

    // Load length.
    Register outReg;
    if (output.hasValue()) {
        outReg = output.valueReg().scratchReg();
    } else {
        JS_ASSERT(output.type() == MIRType_Int32);
        outReg = output.typedReg().gpr();
    }

    masm.loadPtr(Address(object, JSObject::offsetOfElements()), outReg);
    masm.load32(Address(outReg, ObjectElements::offsetOfLength()), outReg);

    // The length is an unsigned int, but the value encodes a signed int.
    JS_ASSERT(object != outReg);
    masm.branchTest32(Assembler::Signed, outReg, outReg, &failures);

    if (output.hasValue())
        masm.tagValue(JSVAL_TYPE_INT32, outReg, output.valueReg());

    /* Success. */
    attacher.jumpRejoin(masm);

    /* Failure. */
    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    return true;
}

static void
GenerateTypedArrayLength(JSContext *cx, MacroAssembler &masm, IonCache::StubAttacher &attacher,
                         JSObject *obj, Register object, TypedOrValueRegister output)
{
    JS_ASSERT(obj->is<TypedArrayObject>());

    Label failures;

    Register tmpReg;
    if (output.hasValue()) {
        tmpReg = output.valueReg().scratchReg();
    } else {
        JS_ASSERT(output.type() == MIRType_Int32);
        tmpReg = output.typedReg().gpr();
    }
    JS_ASSERT(object != tmpReg);

    // Implement the negated version of JSObject::isTypedArray predicate.
    masm.loadObjClass(object, tmpReg);
    masm.branchPtr(Assembler::Below, tmpReg, ImmWord(&TypedArrayObject::classes[0]), &failures);
    masm.branchPtr(Assembler::AboveOrEqual, tmpReg, ImmWord(&TypedArrayObject::classes[TypedArrayObject::TYPE_MAX]), &failures);

    // Load length.
    masm.loadTypedOrValue(Address(object, TypedArrayObject::lengthOffset()), output);

    /* Success. */
    attacher.jumpRejoin(masm);

    /* Failure. */
    masm.bind(&failures);
    attacher.jumpNextStub(masm);
}

template <class GetPropCache>
static GetPropertyIC::NativeGetPropCacheability
CanAttachNativeGetProp(typename GetPropCache::Context cx, const GetPropCache &cache,
                       HandleObject obj, HandlePropertyName name,
                       MutableHandleObject holder, MutableHandleShape shape)
{
    if (!obj || !obj->isNative())
        return GetPropertyIC::CanAttachNone;

    // If the cache is idempotent or parallel, watch out for resolve hooks or
    // non-native objects on the proto chain. We check this before calling
    // lookupProperty, to make sure no effectful lookup hooks or resolve hooks
    // are called.
    if (cache.lookupNeedsIdempotentChain() && !obj->hasIdempotentProtoChain())
        return GetPropertyIC::CanAttachNone;

    if (!GetPropCache::doPropertyLookup(cx, obj, name, holder, shape))
        return GetPropertyIC::CanAttachError;

    RootedScript script(cx);
    jsbytecode *pc;
    cache.getScriptedLocation(&script, &pc);
    if (IsCacheableGetPropReadSlot(obj, holder, shape) ||
        IsCacheableNoProperty(obj, holder, shape, pc, cache.output()))
    {
        // TI infers the possible types of native object properties. There's one
        // edge case though: for singleton objects it does not add the initial
        // "undefined" type, see the propertySet comment in jsinfer.h.
        if (!cache.canMonitorSingletonUndefinedSlot(holder, shape))
            return GetPropertyIC::CanAttachNone;
        return GetPropertyIC::CanAttachReadSlot;
    }

    if (obj->is<ArrayObject>() && cx->names().length == name) {
        // The array length property is non-configurable, which means both that
        // checking the class of the object and the name of the property is enough
        // and that we don't need to worry about monitoring, since we know the
        // return type statically.
        return GetPropertyIC::CanAttachArrayLength;
    }

    if (cache.allowGetters() &&
        (IsCacheableGetPropCallNative(obj, holder, shape) ||
         IsCacheableGetPropCallPropertyOp(obj, holder, shape)))
    {
        // Don't enable getter call if cache is parallel or idempotent, since
        // they can be effectful. This is handled by allowGetters()
        return GetPropertyIC::CanAttachCallGetter;
    }

    return GetPropertyIC::CanAttachNone;
}

bool
GetPropertyIC::canMonitorSingletonUndefinedSlot(HandleObject holder, HandleShape shape) const
{
    // We can't monitor the return type inside an idempotent cache,
    // so we don't handle this case.
    return !(idempotent() &&
             holder &&
             holder->hasSingletonType() &&
             holder->getSlot(shape->slot()).isUndefined());
}

bool
GetPropertyIC::tryAttachNative(JSContext *cx, IonScript *ion, HandleObject obj,
                               HandlePropertyName name, void *returnAddr, bool *emitted)
{
    JS_ASSERT(canAttachStub());
    JS_ASSERT(!*emitted);

    RootedShape shape(cx);
    RootedObject holder(cx);

    NativeGetPropCacheability type =
        CanAttachNativeGetProp(cx, *this, obj, name, &holder, &shape);
    if (type == CanAttachError)
        return false;
    if (type == CanAttachNone)
        return true;

    *emitted = true;

    MacroAssembler masm(cx);
    RepatchStubAppender attacher(*this);
    const char *attachKind;

    switch (type) {
      case CanAttachReadSlot:
        GenerateReadSlot(cx, ion, masm, attacher, obj, name, holder,
                            shape, object(), output());
        attachKind = idempotent() ? "idempotent reading"
                                    : "non idempotent reading";
        break;
      case CanAttachCallGetter:
        masm.setFramePushed(ion->frameSize());
        if (!GenerateCallGetter(cx, ion, masm, attacher, obj, name, holder, shape,
                                liveRegs_, object(), output(), returnAddr))
        {
            return false;
        }
        attachKind = "getter call";
        break;
      case CanAttachArrayLength:
        if (!GenerateArrayLength(cx, masm, attacher, obj, object(), output()))
            return false;

        attachKind = "array length";
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Bad NativeGetPropCacheability");
    }
    return linkAndAttachStub(cx, masm, attacher, ion, attachKind);
}

bool
GetPropertyIC::tryAttachTypedArrayLength(JSContext *cx, IonScript *ion, HandleObject obj,
                                         HandlePropertyName name, bool *emitted)
{
    JS_ASSERT(canAttachStub());
    JS_ASSERT(!*emitted);

    if (!obj->is<TypedArrayObject>())
        return true;

    if (cx->names().length != name)
        return true;

    if (hasTypedArrayLengthStub())
        return true;

    if (output().type() != MIRType_Value && output().type() != MIRType_Int32) {
        // The next execution should cause an invalidation because the type
        // does not fit.
        return true;
    }

    if (idempotent())
        return true;

    *emitted = true;

    MacroAssembler masm(cx);
    RepatchStubAppender attacher(*this);
    GenerateTypedArrayLength(cx, masm, attacher, obj, object(), output());

    JS_ASSERT(!hasTypedArrayLengthStub_);
    hasTypedArrayLengthStub_ = true;
    return linkAndAttachStub(cx, masm, attacher, ion, "typed array length");
}

static bool
EmitCallProxyGet(JSContext *cx, MacroAssembler &masm, IonCache::StubAttacher &attacher,
                 PropertyName *name, RegisterSet liveRegs, Register object,
                 TypedOrValueRegister output, void *returnAddr)
{
    JS_ASSERT(output.hasValue());
    // saveLive()
    masm.PushRegsInMask(liveRegs);

    // Remaining registers should be free, but we need to use |object| still
    // so leave it alone.
    RegisterSet regSet(RegisterSet::All());
    regSet.take(AnyRegister(object));

    // Proxy::get(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id,
    //            MutableHandleValue vp)
    Register argJSContextReg = regSet.takeGeneral();
    Register argProxyReg     = regSet.takeGeneral();
    Register argIdReg        = regSet.takeGeneral();
    Register argVpReg        = regSet.takeGeneral();

    Register scratch         = regSet.takeGeneral();

    DebugOnly<uint32_t> initialStack = masm.framePushed();

    // Push stubCode for marking.
    attacher.pushStubCodePointer(masm);

    // Push args on stack first so we can take pointers to make handles.
    masm.Push(UndefinedValue());
    masm.movePtr(StackPointer, argVpReg);

    RootedId propId(cx, AtomToId(name));
    masm.Push(propId, scratch);
    masm.movePtr(StackPointer, argIdReg);

    // Pushing object and receiver.  Both are the same, so Handle to one is equivalent to
    // handle to other.
    masm.Push(object);
    masm.Push(object);
    masm.movePtr(StackPointer, argProxyReg);

    masm.loadJSContext(argJSContextReg);

    if (!masm.buildOOLFakeExitFrame(returnAddr))
        return false;
    masm.enterFakeExitFrame(ION_FRAME_OOL_PROXY_GET);

    // Make the call.
    masm.setupUnalignedABICall(5, scratch);
    masm.passABIArg(argJSContextReg);
    masm.passABIArg(argProxyReg);
    masm.passABIArg(argProxyReg);
    masm.passABIArg(argIdReg);
    masm.passABIArg(argVpReg);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, Proxy::get));

    // Test for failure.
    masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

    // Load the outparam vp[0] into output register(s).
    masm.loadValue(
        Address(StackPointer, IonOOLProxyGetExitFrameLayout::offsetOfResult()),
        JSReturnOperand);

    masm.storeCallResultValue(output);

    // The next instruction is removing the footer of the exit frame, so there
    // is no need for leaveFakeExitFrame.

    // Move the StackPointer back to its original location, unwinding the exit frame.
    masm.adjustStack(IonOOLProxyGetExitFrameLayout::Size());
    JS_ASSERT(masm.framePushed() == initialStack);

    // restoreLive()
    masm.PopRegsInMask(liveRegs);

    return true;
}

bool
GetPropertyIC::tryAttachDOMProxyShadowed(JSContext *cx, IonScript *ion,
                                         HandleObject obj, void *returnAddr,
                                         bool *emitted)
{
    JS_ASSERT(canAttachStub());
    JS_ASSERT(!*emitted);
    JS_ASSERT(IsCacheableDOMProxy(obj));

    if (idempotent() || !output().hasValue())
        return true;

    *emitted = true;

    Label failures;
    MacroAssembler masm(cx);
    RepatchStubAppender attacher(*this);

    masm.setFramePushed(ion->frameSize());

    // Guard on the shape of the object.
    attacher.branchNextStubOrLabel(masm, Assembler::NotEqual,
                                   Address(object(), JSObject::offsetOfShape()),
                                   ImmGCPtr(obj->lastProperty()),
                                   &failures);

    // Make sure object is a DOMProxy
    GenerateDOMProxyChecks(cx, masm, obj, name(), object(), &failures,
                           /*skipExpandoCheck=*/true);

    if (!EmitCallProxyGet(cx, masm, attacher, name(), liveRegs_, object(), output(), returnAddr))
        return false;

    // Success.
    attacher.jumpRejoin(masm);

    // Failure.
    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "list base shadowed get");
}

bool
GetPropertyIC::tryAttachDOMProxyUnshadowed(JSContext *cx, IonScript *ion, HandleObject obj,
                                           HandlePropertyName name, bool resetNeeded,
                                           void *returnAddr, bool *emitted)
{
    JS_ASSERT(canAttachStub());
    JS_ASSERT(!*emitted);
    JS_ASSERT(IsCacheableDOMProxy(obj));
    JS_ASSERT(output().hasValue());

    RootedObject checkObj(cx, obj->getTaggedProto().toObjectOrNull());
    RootedObject holder(cx);
    RootedShape shape(cx);

    NativeGetPropCacheability canCache =
        CanAttachNativeGetProp(cx, *this, checkObj, name, &holder, &shape);

    if (canCache == CanAttachError)
        return false;
    if (canCache == CanAttachNone)
        return true;

    *emitted = true;

    if (resetNeeded) {
        // If we know that we have a DoesntShadowUnique object, then
        // we reset the cache to clear out an existing IC for the object
        // (if there is one). The generation is a constant in the generated
        // code and we will not have the same generation again for this
        // object, so the generation check in the existing IC would always
        // fail anyway.
        reset();
    }

    Label failures;
    MacroAssembler masm(cx);
    RepatchStubAppender attacher(*this);

    masm.setFramePushed(ion->frameSize());

    // Guard on the shape of the object.
    attacher.branchNextStubOrLabel(masm, Assembler::NotEqual,
                                   Address(object(), JSObject::offsetOfShape()),
                                   ImmGCPtr(obj->lastProperty()),
                                   &failures);

    // Make sure object is a DOMProxy proxy
    GenerateDOMProxyChecks(cx, masm, obj, name, object(), &failures);

    if (holder) {
        // Found the property on the prototype chain. Treat it like a native
        // getprop.
        Register scratchReg = output().valueReg().scratchReg();
        GeneratePrototypeGuards(cx, ion, masm, obj, holder, object(), scratchReg, &failures);

        // Rename scratch for clarity.
        Register holderReg = scratchReg;

        // Guard on the holder of the property
        masm.moveNurseryPtr(ImmMaybeNurseryPtr(holder), holderReg);
        masm.branchPtr(Assembler::NotEqual,
                    Address(holderReg, JSObject::offsetOfShape()),
                    ImmGCPtr(holder->lastProperty()),
                    &failures);

        if (canCache == CanAttachReadSlot) {
            EmitLoadSlot(masm, holder, shape, holderReg, output(), scratchReg);
        } else {
            // EmitGetterCall() expects |obj| to be the object the property is
            // on to do some checks. Since we actually looked at checkObj, and
            // no extra guards will be generated, we can just pass that instead.
            JS_ASSERT_IF(canCache != CanAttachCallGetter, canCache == CanAttachArrayLength);
            if (!EmitGetterCall(cx, masm, attacher, checkObj, holder, shape, liveRegs_,
                                object(), scratchReg, output(), returnAddr))
            {
                return false;
            }
        }
    } else {
        // Property was not found on the prototype chain. Deoptimize down to
        // proxy get call
        if (!EmitCallProxyGet(cx, masm, attacher, name, liveRegs_, object(), output(),
                              returnAddr))
        {
            return false;
        }
    }

    attacher.jumpRejoin(masm);
    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "unshadowed proxy get");
}

bool
GetPropertyIC::tryAttachProxy(JSContext *cx, IonScript *ion, HandleObject obj,
                              HandlePropertyName name, void *returnAddr,
                              bool *emitted)
{
    JS_ASSERT(canAttachStub());
    JS_ASSERT(!*emitted);

    if (!obj->is<ProxyObject>())
        return true;

    if (!output().hasValue())
        return true;

    // Skim off DOM proxies.
    if (IsCacheableDOMProxy(obj)) {
        RootedId id(cx, NameToId(name));
        DOMProxyShadowsResult shadows = GetDOMProxyShadowsCheck()(cx, obj, id);
        if (shadows == ShadowCheckFailed)
            return false;
        if (shadows == Shadows)
            return tryAttachDOMProxyShadowed(cx, ion, obj, returnAddr, emitted);

        return tryAttachDOMProxyUnshadowed(cx, ion, obj, name, shadows == DoesntShadowUnique,
                                           returnAddr, emitted);
    }

    return tryAttachGenericProxy(cx, ion, obj, name, returnAddr, emitted);
}

bool
GetPropertyIC::tryAttachGenericProxy(JSContext *cx, IonScript *ion, HandleObject obj,
                                     HandlePropertyName name, void *returnAddr,
                                     bool *emitted)
{
    JS_ASSERT(canAttachStub());
    JS_ASSERT(!*emitted);
    JS_ASSERT(obj->is<ProxyObject>());

    if (hasGenericProxyStub())
        return true;

    *emitted = true;

    Label failures;
    Label classPass;
    MacroAssembler masm(cx);
    RepatchStubAppender attacher(*this);

    Register scratchReg = output().valueReg().scratchReg();

    masm.setFramePushed(ion->frameSize());

    // The branching around here is a little kludgy. It seems mostly unavoidable.

    // Ensure that the incoming object has one of the magic class pointers, i.e,
    // that it is one of an ObjectProxy, FunctionProxy, or OuterWindowProxy.
    masm.branchTestObjClass(Assembler::Equal, object(), scratchReg,
                            ObjectProxyClassPtr, &classPass);
    masm.branchTestObjClass(Assembler::Equal, object(), scratchReg,
                            FunctionProxyClassPtr, &classPass);
    masm.branchTestObjClass(Assembler::NotEqual, object(), scratchReg,
                            OuterWindowProxyClassPtr, &failures);

    masm.bind(&classPass);
    // Ensure that the incoming object is not a DOM proxy, so that we can get to
    // the specialized stubs
    Address handlerAddr(object(), ProxyObject::offsetOfHandler());
    masm.loadPrivate(handlerAddr, scratchReg);
    Address familyAddr(scratchReg, BaseProxyHandler::offsetOfFamily());
    masm.branchPtr(Assembler::Equal, familyAddr, ImmWord(GetDOMProxyHandlerFamily()),
                   &failures);

    if (!EmitCallProxyGet(cx, masm, attacher, name, liveRegs_, object(), output(), returnAddr))
        return false;

    attacher.jumpRejoin(masm);

    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    JS_ASSERT(!hasGenericProxyStub_);
    hasGenericProxyStub_ = true;

    return linkAndAttachStub(cx, masm, attacher, ion, "Generic Proxy get");
}

bool
GetPropertyIC::tryAttachArgumentsLength(JSContext *cx, IonScript *ion, HandleObject obj,
                                        HandlePropertyName name, bool *emitted)
{
    JS_ASSERT(canAttachStub());
    JS_ASSERT(!*emitted);

    if (name != cx->names().length)
        return true;
    if (!IsOptimizableArgumentsObjectForLength(obj))
        return true;

    MIRType outputType = output().type();
    if (!(outputType == MIRType_Value || outputType == MIRType_Int32))
        return true;

    if (hasArgumentsLengthStub(obj->is<StrictArgumentsObject>()))
        return true;

    *emitted = true;

    JS_ASSERT(!idempotent());

    Label failures;
    MacroAssembler masm(cx);
    RepatchStubAppender attacher(*this);

    Register tmpReg;
    if (output().hasValue()) {
        tmpReg = output().valueReg().scratchReg();
    } else {
        JS_ASSERT(output().type() == MIRType_Int32);
        tmpReg = output().typedReg().gpr();
    }
    JS_ASSERT(object() != tmpReg);

    Class *clasp = obj->is<StrictArgumentsObject>() ? &StrictArgumentsObject::class_
                                                    : &NormalArgumentsObject::class_;

    Label fail;
    Label pass;
    masm.branchTestObjClass(Assembler::NotEqual, object(), tmpReg, clasp, &failures);

    // Get initial ArgsObj length value, test if length has been overridden.
    masm.unboxInt32(Address(object(), ArgumentsObject::getInitialLengthSlotOffset()), tmpReg);
    masm.branchTest32(Assembler::NonZero, tmpReg, Imm32(ArgumentsObject::LENGTH_OVERRIDDEN_BIT),
                      &failures);

    masm.rshiftPtr(Imm32(ArgumentsObject::PACKED_BITS_COUNT), tmpReg);

    // If output is Int32, result is already in right place, otherwise box it into output.
    if (output().hasValue())
        masm.tagValue(JSVAL_TYPE_INT32, tmpReg, output().valueReg());

    // Success.
    attacher.jumpRejoin(masm);

    // Failure.
    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    if (obj->is<StrictArgumentsObject>()) {
        JS_ASSERT(!hasStrictArgumentsLengthStub_);
        hasStrictArgumentsLengthStub_ = true;
        return linkAndAttachStub(cx, masm, attacher, ion, "ArgsObj length (strict)");
    }

    JS_ASSERT(!hasNormalArgumentsLengthStub_);
    hasNormalArgumentsLengthStub_ = true;
    return linkAndAttachStub(cx, masm, attacher, ion, "ArgsObj length (normal)");
}

bool
GetPropertyIC::tryAttachStub(JSContext *cx, IonScript *ion, HandleObject obj,
                             HandlePropertyName name, void *returnAddr, bool *emitted)
{
    JS_ASSERT(!*emitted);

    if (!canAttachStub())
        return true;

    if (!*emitted && !tryAttachArgumentsLength(cx, ion, obj, name, emitted))
        return false;

    if (!*emitted && !tryAttachProxy(cx, ion, obj, name, returnAddr, emitted))
        return false;

    if (!*emitted && !tryAttachNative(cx, ion, obj, name, returnAddr, emitted))
        return false;

    if (!*emitted && !tryAttachTypedArrayLength(cx, ion, obj, name, emitted))
        return false;

    return true;
}

/* static */ bool
GetPropertyIC::update(JSContext *cx, size_t cacheIndex,
                      HandleObject obj, MutableHandleValue vp)
{
    void *returnAddr;
    RootedScript topScript(cx, GetTopIonJSScript(cx, &returnAddr));
    IonScript *ion = topScript->ionScript();

    GetPropertyIC &cache = ion->getCache(cacheIndex).toGetProperty();
    RootedPropertyName name(cx, cache.name());

    AutoFlushCache afc ("GetPropertyCache");

    // Override the return value if we are invalidated (bug 728188).
    AutoDetectInvalidation adi(cx, vp.address(), ion);

    // If the cache is idempotent, we will redo the op in the interpreter.
    if (cache.idempotent())
        adi.disable();

    // For now, just stop generating new stubs once we hit the stub count
    // limit. Once we can make calls from within generated stubs, a new call
    // stub will be generated instead and the previous stubs unlinked.
    bool emitted = false;
    if (!cache.tryAttachStub(cx, ion, obj, name, returnAddr, &emitted))
        return false;

    if (cache.idempotent() && !emitted) {
        // Invalidate the cache if the property was not found, or was found on
        // a non-native object. This ensures:
        // 1) The property read has no observable side-effects.
        // 2) There's no need to dynamically monitor the return type. This would
        //    be complicated since (due to GVN) there can be multiple pc's
        //    associated with a single idempotent cache.
        IonSpew(IonSpew_InlineCaches, "Invalidating from idempotent cache %s:%d",
                topScript->filename(), topScript->lineno);

        topScript->invalidatedIdempotentCache = true;

        // Do not re-invalidate if the lookup already caused invalidation.
        if (!topScript->hasIonScript())
            return true;

        return Invalidate(cx, topScript);
    }

    RootedId id(cx, NameToId(name));
    if (obj->getOps()->getProperty) {
        if (!JSObject::getGeneric(cx, obj, obj, id, vp))
            return false;
    } else {
        if (!GetPropertyHelper(cx, obj, id, 0, vp))
            return false;
    }

    if (!cache.idempotent()) {
        RootedScript script(cx);
        jsbytecode *pc;
        cache.getScriptedLocation(&script, &pc);

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
GetPropertyIC::reset()
{
    RepatchIonCache::reset();
    hasTypedArrayLengthStub_ = false;
    hasStrictArgumentsLengthStub_ = false;
    hasNormalArgumentsLengthStub_ = false;
    hasGenericProxyStub_ = false;
}

bool
ParallelIonCache::initStubbedShapes(JSContext *cx)
{
    JS_ASSERT(isAllocated());
    if (!stubbedShapes_) {
        stubbedShapes_ = cx->new_<ShapeSet>(cx);
        return stubbedShapes_ && stubbedShapes_->init();
    }
    return true;
}

bool
ParallelIonCache::hasOrAddStubbedShape(LockedJSContext &cx, Shape *shape, bool *alreadyStubbed)
{
    // Check if we have already stubbed the current object to avoid
    // attaching a duplicate stub.
    if (!initStubbedShapes(cx))
        return false;
    ShapeSet::AddPtr p = stubbedShapes_->lookupForAdd(shape);
    if ((*alreadyStubbed = !!p))
        return true;
    return stubbedShapes_->add(p, shape);
}

void
ParallelIonCache::reset()
{
    DispatchIonCache::reset();
    if (stubbedShapes_)
        stubbedShapes_->clear();
}

void
ParallelIonCache::destroy()
{
    DispatchIonCache::destroy();
    if (stubbedShapes_)
        js_delete(stubbedShapes_);
}

void
GetPropertyParIC::reset()
{
    ParallelIonCache::reset();
    hasTypedArrayLengthStub_ = false;
}

/* static */ bool
GetPropertyParIC::canAttachReadSlot(LockedJSContext &cx, IonCache &cache,
                                    TypedOrValueRegister output, JSObject *obj,
                                    PropertyName *name, MutableHandleObject holder,
                                    MutableHandleShape shape)
{
    // Bail if we have hooks or are not native.
    if (!obj->hasIdempotentProtoChain())
        return false;

    if (!js::LookupPropertyPure(obj, NameToId(name), holder.address(), shape.address()))
        return false;

    // In parallel execution we can't cache getters due to possible
    // side-effects, so only check if we can cache slot reads.
    RootedScript script(cx);
    jsbytecode *pc;
    cache.getScriptedLocation(&script, &pc);
    return (IsCacheableGetPropReadSlot(obj, holder, shape) ||
            IsCacheableNoProperty(obj, holder, shape, pc, output));
}

bool
GetPropertyParIC::attachReadSlot(LockedJSContext &cx, IonScript *ion, JSObject *obj,
                                 JSObject *holder, Shape *shape)
{
    // Ready to generate the read slot stub.
    DispatchStubPrepender attacher(*this);
    MacroAssembler masm(cx);
    GenerateReadSlot(cx, ion, masm, attacher, obj, name(), holder, shape, object(), output());

    return linkAndAttachStub(cx, masm, attacher, ion, "parallel reading");
}

bool
GetPropertyParIC::attachArrayLength(LockedJSContext &cx, IonScript *ion, JSObject *obj)
{
    MacroAssembler masm(cx);
    DispatchStubPrepender attacher(*this);
    if (!GenerateArrayLength(cx, masm, attacher, obj, object(), output()))
        return false;

    return linkAndAttachStub(cx, masm, attacher, ion, "parallel array length");
}

bool
GetPropertyParIC::attachTypedArrayLength(LockedJSContext &cx, IonScript *ion, JSObject *obj)
{
    MacroAssembler masm(cx);
    DispatchStubPrepender attacher(*this);
    GenerateTypedArrayLength(cx, masm, attacher, obj, object(), output());

    JS_ASSERT(!hasTypedArrayLengthStub_);
    hasTypedArrayLengthStub_ = true;
    return linkAndAttachStub(cx, masm, attacher, ion, "parallel typed array length");
}

ParallelResult
GetPropertyParIC::update(ForkJoinSlice *slice, size_t cacheIndex,
                         HandleObject obj, MutableHandleValue vp)
{
    AutoFlushCache afc("GetPropertyParCache");

    IonScript *ion = GetTopIonJSScript(slice)->parallelIonScript();
    GetPropertyParIC &cache = ion->getCache(cacheIndex).toGetPropertyPar();

    // Grab the property early, as the pure path is fast anyways and doesn't
    // need a lock. If we can't do it purely, bail out of parallel execution.
    if (!GetPropertyPure(slice, obj, NameToId(cache.name()), vp.address()))
        return TP_RETRY_SEQUENTIALLY;

    // Avoid unnecessary locking if cannot attach stubs.
    if (!cache.canAttachStub())
        return TP_SUCCESS;

    {
        // Lock the context before mutating the cache. Ideally we'd like to do
        // finer-grained locking, with one lock per cache. However, generating
        // new jitcode uses a global ExecutableAllocator tied to the runtime.
        LockedJSContext cx(slice);

        if (cache.canAttachStub()) {
            bool alreadyStubbed;
            if (!cache.hasOrAddStubbedShape(cx, obj->lastProperty(), &alreadyStubbed))
                return TP_FATAL;
            if (alreadyStubbed)
                return TP_SUCCESS;

            // See note about the stub limit in GetPropertyCache.
            bool attachedStub = false;

            {
                RootedShape shape(cx);
                RootedObject holder(cx);
                if (canAttachReadSlot(cx, cache, cache.output(), obj, cache.name(),
                                      &holder, &shape))
                {
                    if (!cache.attachReadSlot(cx, ion, obj, holder, shape))
                        return TP_FATAL;
                    attachedStub = true;
                }
            }

            if (!attachedStub && obj->is<ArrayObject>() && slice->names().length == cache.name()) {
                if (!cache.attachArrayLength(cx, ion, obj))
                    return TP_FATAL;
                attachedStub = true;
            }

            if (!attachedStub && !cache.hasTypedArrayLengthStub() &&
                obj->is<TypedArrayObject>() && slice->names().length == cache.name() &&
                (cache.output().type() == MIRType_Value || cache.output().type() == MIRType_Int32))
            {
                if (!cache.attachTypedArrayLength(cx, ion, obj))
                    return TP_FATAL;
                attachedStub = true;
            }
        }
    }

    return TP_SUCCESS;
}

void
IonCache::disable()
{
    reset();
    this->disabled_ = 1;
}

void
IonCache::reset()
{
    this->stubCount_ = 0;
}

void
IonCache::destroy()
{
}

bool
SetPropertyIC::attachNativeExisting(JSContext *cx, IonScript *ion,
                                    HandleObject obj, HandleShape shape)
{
    MacroAssembler masm(cx);
    RepatchStubAppender attacher(*this);

    attacher.branchNextStub(masm, Assembler::NotEqual,
                            Address(object(), JSObject::offsetOfShape()),
                            ImmGCPtr(obj->lastProperty()));

    if (obj->isFixedSlot(shape->slot())) {
        Address addr(object(), JSObject::getFixedSlotOffset(shape->slot()));

        if (cx->zone()->needsBarrier())
            masm.callPreBarrier(addr, MIRType_Value);

        masm.storeConstantOrRegister(value(), addr);
    } else {
        Register slotsReg = object();
        masm.loadPtr(Address(object(), JSObject::offsetOfSlots()), slotsReg);

        Address addr(slotsReg, obj->dynamicSlotIndex(shape->slot()) * sizeof(Value));

        if (cx->zone()->needsBarrier())
            masm.callPreBarrier(addr, MIRType_Value);

        masm.storeConstantOrRegister(value(), addr);
    }

    attacher.jumpRejoin(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "setting");
}

bool
SetPropertyIC::attachSetterCall(JSContext *cx, IonScript *ion,
                                HandleObject obj, HandleObject holder, HandleShape shape,
                                void *returnAddr)
{
    MacroAssembler masm(cx);
    RepatchStubAppender attacher(*this);

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
            GeneratePrototypeGuards(cx, ion, masm, obj, holder, object(), scratchReg, &protoFailure);

        masm.moveNurseryPtr(ImmMaybeNurseryPtr(holder), scratchReg);
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
    masm.PushRegsInMask(liveRegs_);

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

    attacher.pushStubCodePointer(masm);

    StrictPropertyOp target = shape->setterOp();
    JS_ASSERT(target);
    // JSStrictPropertyOp: bool fn(JSContext *cx, HandleObject obj,
    //                               HandleId id, bool strict, MutableHandleValue vp);

    // Push args on stack first so we can take pointers to make handles.
    if (value().constant())
        masm.Push(value().value());
    else
        masm.Push(value().reg());
    masm.movePtr(StackPointer, argVpReg);

    masm.move32(Imm32(strict() ? 1 : 0), argStrictReg);

    // push canonical jsid from shape instead of propertyname.
    RootedId propId(cx);
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
    masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

    // The next instruction is removing the footer of the exit frame, so there
    // is no need for leaveFakeExitFrame.

    // Move the StackPointer back to its original location, unwinding the exit frame.
    masm.adjustStack(IonOOLPropertyOpExitFrameLayout::Size());
    JS_ASSERT(masm.framePushed() == initialStack);

    // restoreLive()
    masm.PopRegsInMask(liveRegs_);

    // Rejoin jump.
    attacher.jumpRejoin(masm);

    // Jump to next stub.
    masm.bind(&failure);
    attacher.jumpNextStub(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "calling");
}

bool
SetPropertyIC::attachNativeAdding(JSContext *cx, IonScript *ion, JSObject *obj,
                                  HandleShape oldShape, HandleShape newShape,
                                  HandleShape propShape)
{
    MacroAssembler masm(cx);
    RepatchStubAppender attacher(*this);

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

        // Ensure that its shape matches.
        masm.branchTestObjShape(Assembler::NotEqual, protoReg, protoShape, &protoFailures);

        proto = proto->getProto();
    }

    masm.pop(object());     // restore object reg

    /* Changing object shape.  Write the object's new shape. */
    Address shapeAddr(object(), JSObject::offsetOfShape());
    if (cx->zone()->needsBarrier())
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
    attacher.jumpRejoin(masm);

    /* Failure. */
    masm.bind(&protoFailures);
    masm.pop(object());
    masm.bind(&failures);

    attacher.jumpNextStub(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "adding");
}

static bool
IsPropertyInlineable(JSObject *obj)
{
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
                               HandleShape shape)
{
    if (!shape)
        return false;

    if (!holder->isNative())
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
IsPropertyAddInlineable(JSContext *cx, HandleObject obj, HandleId id, uint32_t oldSlots,
                        MutableHandleShape pShape)
{
    // This is not a Add, the property exists.
    if (pShape.get())
        return false;

    RootedShape shape(cx, obj->nativeLookup(cx, id));
    if (!shape || shape->inDictionary() || !shape->hasSlot() || !shape->hasDefaultSetter())
        return false;

    // If object has a non-default resolve hook, don't inline
    if (obj->getClass()->resolve != JS_ResolveStub)
        return false;

    // Likewise for a non-default addProperty hook, since we'll need
    // to invoke it.
    if (obj->getClass()->addProperty != JS_PropertyStub)
        return false;

    if (!obj->nonProxyIsExtensible() || !shape->writable())
        return false;

    // walk up the object prototype chain and ensure that all prototypes
    // are native, and that all prototypes have no getter or setter
    // defined on the property
    for (JSObject *proto = obj->getProto(); proto; proto = proto->getProto()) {
        // if prototype is non-native, don't optimize
        if (!proto->isNative())
            return false;

        // if prototype defines this property in a non-plain way, don't optimize
        Shape *protoShape = proto->nativeLookup(cx, id);
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
SetPropertyIC::update(JSContext *cx, size_t cacheIndex, HandleObject obj,
                      HandleValue value)
{
    AutoFlushCache afc ("SetPropertyCache");

    void *returnAddr;
    RootedScript script(cx, GetTopIonJSScript(cx, &returnAddr));
    IonScript *ion = script->ionScript();
    SetPropertyIC &cache = ion->getCache(cacheIndex).toSetProperty();
    RootedPropertyName name(cx, cache.name());
    RootedId id(cx, AtomToId(name));
    RootedShape shape(cx);
    RootedObject holder(cx);

    // Stop generating new stubs once we hit the stub count limit, see
    // GetPropertyCache.
    bool inlinable = cache.canAttachStub() && IsPropertyInlineable(obj);
    bool addedSetterStub = false;
    if (inlinable) {
        RootedShape shape(cx);
        if (IsPropertySetInlineable(cx, obj, id, &shape)) {
            if (!cache.attachNativeExisting(cx, ion, obj, shape))
                return false;
            addedSetterStub = true;
        } else {
            RootedObject holder(cx);
            if (!JSObject::lookupProperty(cx, obj, name, &holder, &shape))
                return false;

            if (IsPropertySetterCallInlineable(cx, obj, holder, shape)) {
                if (!cache.attachSetterCall(cx, ion, obj, holder, shape, returnAddr))
                    return false;
                addedSetterStub = true;
            }
        }
    }

    uint32_t oldSlots = obj->numDynamicSlots();
    RootedShape oldShape(cx, obj->lastProperty());

    // Set/Add the property on the object, the inlined cache are setup for the next execution.
    if (!SetProperty(cx, obj, name, value, cache.strict(), cache.isSetName()))
        return false;

    // The property did not exist before, now we can try to inline the property add.
    if (inlinable && !addedSetterStub && obj->lastProperty() != oldShape &&
        IsPropertyAddInlineable(cx, obj, id, oldSlots, &shape))
    {
        RootedShape newShape(cx, obj->lastProperty());
        if (!cache.attachNativeAdding(cx, ion, obj, oldShape, newShape, shape))
            return false;
    }

    return true;
}

const size_t GetElementIC::MAX_FAILED_UPDATES = 16;

/* static */ bool
GetElementIC::canAttachGetProp(JSObject *obj, const Value &idval, jsid id)
{
    uint32_t dummy;
    return (obj->isNative() &&
            idval.isString() &&
            JSID_IS_ATOM(id) &&
            !JSID_TO_ATOM(id)->isIndex(&dummy));
}

bool
GetElementIC::attachGetProp(JSContext *cx, IonScript *ion, HandleObject obj,
                            const Value &idval, HandlePropertyName name)
{
    JS_ASSERT(index().reg().hasValue());

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

    Label failures;
    MacroAssembler masm(cx);

    // Guard on the index value.
    ValueOperand val = index().reg().valueReg();
    masm.branchTestValue(Assembler::NotEqual, val, idval, &failures);

    RepatchStubAppender attacher(*this);
    GenerateReadSlot(cx, ion, masm, attacher, obj, name, holder, shape, object(), output(),
                     &failures);

    return linkAndAttachStub(cx, masm, attacher, ion, "property");
}

/* static */ bool
GetElementIC::canAttachDenseElement(JSObject *obj, const Value &idval)
{
    return obj->isNative() && idval.isInt32();
}

static bool
GenerateDenseElement(JSContext *cx, MacroAssembler &masm, IonCache::StubAttacher &attacher,
                     JSObject *obj, const Value &idval, Register object,
                     ConstantOrRegister index, TypedOrValueRegister output)
{
    JS_ASSERT(GetElementIC::canAttachDenseElement(obj, idval));

    Label failures;

    // Guard object's shape.
    RootedShape shape(cx, obj->lastProperty());
    if (!shape)
        return false;
    masm.branchTestObjShape(Assembler::NotEqual, object, shape, &failures);

    // Ensure the index is an int32 value.
    Register indexReg = InvalidReg;

    if (index.reg().hasValue()) {
        indexReg = output.scratchReg().gpr();
        JS_ASSERT(indexReg != InvalidReg);
        ValueOperand val = index.reg().valueReg();

        masm.branchTestInt32(Assembler::NotEqual, val, &failures);

        // Unbox the index.
        masm.unboxInt32(val, indexReg);
    } else {
        JS_ASSERT(!index.reg().typedReg().isFloat());
        indexReg = index.reg().typedReg().gpr();
    }

    // Load elements vector.
    masm.push(object);
    masm.loadPtr(Address(object, JSObject::offsetOfElements()), object);

    Label hole;

    // Guard on the initialized length.
    Address initLength(object, ObjectElements::offsetOfInitializedLength());
    masm.branch32(Assembler::BelowOrEqual, initLength, indexReg, &hole);

    // Check for holes & load the value.
    masm.loadElementTypedOrValue(BaseIndex(object, indexReg, TimesEight),
                                 output, true, &hole);

    masm.pop(object);
    attacher.jumpRejoin(masm);

    // All failures flow to here.
    masm.bind(&hole);
    masm.pop(object);
    masm.bind(&failures);

    attacher.jumpNextStub(masm);

    return true;
}

bool
GetElementIC::attachDenseElement(JSContext *cx, IonScript *ion, JSObject *obj, const Value &idval)
{
    MacroAssembler masm(cx);
    RepatchStubAppender attacher(*this);
    if (!GenerateDenseElement(cx, masm, attacher, obj, idval, object(), index(), output()))
        return false;

    setHasDenseStub();
    return linkAndAttachStub(cx, masm, attacher, ion, "dense array");
}

/* static */ bool
GetElementIC::canAttachTypedArrayElement(JSObject *obj, const Value &idval,
                                         TypedOrValueRegister output)
{
    if (!obj->is<TypedArrayObject>() ||
        (!(idval.isInt32()) &&
         !(idval.isString() && GetIndexFromString(idval.toString()) != UINT32_MAX)))
    {
        return false;
    }

    // The output register is not yet specialized as a float register, the only
    // way to accept float typed arrays for now is to return a Value type.
    int arrayType = obj->as<TypedArrayObject>().type();
    bool floatOutput = arrayType == TypedArrayObject::TYPE_FLOAT32 ||
                       arrayType == TypedArrayObject::TYPE_FLOAT64;
    return !floatOutput || output.hasValue();
}

static void
GenerateTypedArrayElement(JSContext *cx, MacroAssembler &masm, IonCache::StubAttacher &attacher,
                          TypedArrayObject *tarr, const Value &idval, Register object,
                          ConstantOrRegister index, TypedOrValueRegister output)
{
    JS_ASSERT(GetElementIC::canAttachTypedArrayElement(tarr, idval, output));

    Label failures;

    // The array type is the object within the table of typed array classes.
    int arrayType = tarr->type();

    // Guard on the shape.
    Shape *shape = tarr->lastProperty();
    masm.branchTestObjShape(Assembler::NotEqual, object, shape, &failures);

    // Decide to what type index the stub should be optimized
    Register tmpReg = output.scratchReg().gpr();
    JS_ASSERT(tmpReg != InvalidReg);
    Register indexReg = tmpReg;
    JS_ASSERT(!index.constant());
    if (idval.isString()) {
        JS_ASSERT(GetIndexFromString(idval.toString()) != UINT32_MAX);

        // Part 1: Get the string into a register
        Register str;
        if (index.reg().hasValue()) {
            ValueOperand val = index.reg().valueReg();
            masm.branchTestString(Assembler::NotEqual, val, &failures);

            str = masm.extractString(val, indexReg);
        } else {
            JS_ASSERT(!index.reg().typedReg().isFloat());
            str = index.reg().typedReg().gpr();
        }

        // Part 2: Call to translate the str into index
        RegisterSet regs = RegisterSet::Volatile();
        masm.PushRegsInMask(regs);
        regs.maybeTake(str);

        Register temp = regs.takeGeneral();

        masm.setupUnalignedABICall(1, temp);
        masm.passABIArg(str);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, GetIndexFromString));
        masm.mov(ReturnReg, indexReg);

        RegisterSet ignore = RegisterSet();
        ignore.add(indexReg);
        masm.PopRegsInMaskIgnore(RegisterSet::Volatile(), ignore);

        masm.branch32(Assembler::Equal, indexReg, Imm32(UINT32_MAX), &failures);

    } else {
        JS_ASSERT(idval.isInt32());

        if (index.reg().hasValue()) {
            ValueOperand val = index.reg().valueReg();
            masm.branchTestInt32(Assembler::NotEqual, val, &failures);

            // Unbox the index.
            masm.unboxInt32(val, indexReg);
        } else {
            JS_ASSERT(!index.reg().typedReg().isFloat());
            indexReg = index.reg().typedReg().gpr();
        }
    }

    // Guard on the initialized length.
    Address length(object, TypedArrayObject::lengthOffset());
    masm.branch32(Assembler::BelowOrEqual, length, indexReg, &failures);

    // Save the object register on the stack in case of failure.
    Label popAndFail;
    Register elementReg = object;
    masm.push(object);

    // Load elements vector.
    masm.loadPtr(Address(object, TypedArrayObject::dataOffset()), elementReg);

    // Load the value. We use an invalid register because the destination
    // register is necessary a non double register.
    int width = TypedArrayObject::slotWidth(arrayType);
    BaseIndex source(elementReg, indexReg, ScaleFromElemWidth(width));
    if (output.hasValue())
        masm.loadFromTypedArray(arrayType, source, output.valueReg(), true,
                                elementReg, &popAndFail);
    else
        masm.loadFromTypedArray(arrayType, source, output.typedReg(),
                                elementReg, &popAndFail);

    masm.pop(object);
    attacher.jumpRejoin(masm);

    // Restore the object before continuing to the next stub.
    masm.bind(&popAndFail);
    masm.pop(object);
    masm.bind(&failures);

    attacher.jumpNextStub(masm);
}

bool
GetElementIC::attachTypedArrayElement(JSContext *cx, IonScript *ion, TypedArrayObject *tarr,
                                      const Value &idval)
{
    MacroAssembler masm(cx);
    RepatchStubAppender attacher(*this);
    GenerateTypedArrayElement(cx, masm, attacher, tarr, idval, object(), index(), output());
    return linkAndAttachStub(cx, masm, attacher, ion, "typed array");
}

bool
GetElementIC::attachArgumentsElement(JSContext *cx, IonScript *ion, JSObject *obj)
{
    JS_ASSERT(obj->is<ArgumentsObject>());

    Label failures;
    MacroAssembler masm(cx);
    RepatchStubAppender attacher(*this);

    Register tmpReg = output().scratchReg().gpr();
    JS_ASSERT(tmpReg != InvalidReg);

    Class *clasp = obj->is<StrictArgumentsObject>() ? &StrictArgumentsObject::class_
                                                    : &NormalArgumentsObject::class_;

    Label fail;
    Label pass;
    masm.branchTestObjClass(Assembler::NotEqual, object(), tmpReg, clasp, &failures);

    // Get initial ArgsObj length value, test if length has been overridden.
    masm.unboxInt32(Address(object(), ArgumentsObject::getInitialLengthSlotOffset()), tmpReg);
    masm.branchTest32(Assembler::NonZero, tmpReg, Imm32(ArgumentsObject::LENGTH_OVERRIDDEN_BIT),
                      &failures);
    masm.rshiftPtr(Imm32(ArgumentsObject::PACKED_BITS_COUNT), tmpReg);

    // Decide to what type index the stub should be optimized
    Register indexReg;
    JS_ASSERT(!index().constant());

    // Check index against length.
    Label failureRestoreIndex;
    if (index().reg().hasValue()) {
        ValueOperand val = index().reg().valueReg();
        masm.branchTestInt32(Assembler::NotEqual, val, &failures);
        indexReg = val.scratchReg();

        masm.unboxInt32(val, indexReg);
        masm.branch32(Assembler::AboveOrEqual, indexReg, tmpReg, &failureRestoreIndex);
    } else {
        JS_ASSERT(index().reg().type() == MIRType_Int32);
        indexReg = index().reg().typedReg().gpr();
        masm.branch32(Assembler::AboveOrEqual, indexReg, tmpReg, &failures);
    }
    // Save indexReg because it needs to be clobbered to check deleted bit.
    Label failurePopIndex;
    masm.push(indexReg);

    // Check if property was deleted on arguments object.
    masm.loadPrivate(Address(object(), ArgumentsObject::getDataSlotOffset()), tmpReg);
    masm.loadPtr(Address(tmpReg, offsetof(ArgumentsData, deletedBits)), tmpReg);

    // In tempReg, calculate index of word containing bit: (idx >> logBitsPerWord)
    masm.rshiftPtr(Imm32(JS_BITS_PER_WORD_LOG2), indexReg);
    masm.loadPtr(BaseIndex(tmpReg, indexReg, ScaleFromElemWidth(sizeof(size_t))), tmpReg);

    // Don't bother testing specific bit, if any bit is set in the word, fail.
    masm.branchPtr(Assembler::NotEqual, tmpReg, ImmWord((size_t)0), &failurePopIndex);

    // Get the address to load from into tmpReg
    masm.loadPrivate(Address(object(), ArgumentsObject::getDataSlotOffset()), tmpReg);
    masm.addPtr(Imm32(ArgumentsData::offsetOfArgs()), tmpReg);

    // Restore original index register value, to use for indexing element.
    masm.pop(indexReg);
    BaseIndex elemIdx(tmpReg, indexReg, ScaleFromElemWidth(sizeof(Value)));

    // Ensure result is not magic value, and type-check result.
    masm.branchTestMagic(Assembler::Equal, elemIdx, &failureRestoreIndex);

    if (output().hasTyped()) {
        JS_ASSERT(!output().typedReg().isFloat());
        JS_ASSERT(index().reg().type() == MIRType_Boolean ||
                  index().reg().type() == MIRType_Int32 ||
                  index().reg().type() == MIRType_String ||
                  index().reg().type() == MIRType_Object);
        masm.branchTestMIRType(Assembler::NotEqual, elemIdx, index().reg().type(),
                               &failureRestoreIndex);
    }

    masm.loadTypedOrValue(elemIdx, output());

    // indexReg may need to be reconstructed if it was originally a value.
    if (index().reg().hasValue())
        masm.tagValue(JSVAL_TYPE_INT32, indexReg, index().reg().valueReg());

    // Success.
    attacher.jumpRejoin(masm);

    // Restore the object before continuing to the next stub.
    masm.bind(&failurePopIndex);
    masm.pop(indexReg);
    masm.bind(&failureRestoreIndex);
    if (index().reg().hasValue())
        masm.tagValue(JSVAL_TYPE_INT32, indexReg, index().reg().valueReg());
    masm.bind(&failures);
    attacher.jumpNextStub(masm);


    if (obj->is<StrictArgumentsObject>()) {
        JS_ASSERT(!hasStrictArgumentsStub_);
        hasStrictArgumentsStub_ = true;
        return linkAndAttachStub(cx, masm, attacher, ion, "ArgsObj element (strict)");
    }

    JS_ASSERT(!hasNormalArgumentsStub_);
    hasNormalArgumentsStub_ = true;
    return linkAndAttachStub(cx, masm, attacher, ion, "ArgsObj element (normal)");
}

bool
GetElementIC::update(JSContext *cx, size_t cacheIndex, HandleObject obj,
                     HandleValue idval, MutableHandleValue res)
{
    IonScript *ion = GetTopIonJSScript(cx)->ionScript();
    GetElementIC &cache = ion->getCache(cacheIndex).toGetElement();
    RootedScript script(cx);
    jsbytecode *pc;
    cache.getScriptedLocation(&script, &pc);
    RootedValue lval(cx, ObjectValue(*obj));

    if (cache.isDisabled()) {
        if (!GetElementOperation(cx, JSOp(*pc), &lval, idval, res))
            return false;
        types::TypeScript::Monitor(cx, script, pc, res);
        return true;
    }

    // Override the return value if we are invalidated (bug 728188).
    AutoFlushCache afc ("GetElementCache");
    AutoDetectInvalidation adi(cx, res.address(), ion);

    RootedId id(cx);
    if (!ValueToId<CanGC>(cx, idval, &id))
        return false;

    bool attachedStub = false;
    if (cache.canAttachStub()) {
        if (IsOptimizableArgumentsObjectForGetElem(obj, idval) &&
            !cache.hasArgumentsStub(obj->is<StrictArgumentsObject>()) &&
            !cache.index().constant() &&
            (cache.index().reg().hasValue() ||
             cache.index().reg().type() == MIRType_Int32) &&
            (cache.output().hasValue() || !cache.output().typedReg().isFloat()))
        {
            if (!cache.attachArgumentsElement(cx, ion, obj))
                return false;
            attachedStub = true;
        }
        if (!attachedStub && cache.monitoredResult() && canAttachGetProp(obj, idval, id)) {
            RootedPropertyName name(cx, JSID_TO_ATOM(id)->asPropertyName());
            if (!cache.attachGetProp(cx, ion, obj, idval, name))
                return false;
            attachedStub = true;
        }
        if (!attachedStub && !cache.hasDenseStub() && canAttachDenseElement(obj, idval)) {
            if (!cache.attachDenseElement(cx, ion, obj, idval))
                return false;
            attachedStub = true;
        }
        if (!attachedStub && canAttachTypedArrayElement(obj, idval, cache.output())) {
            Rooted<TypedArrayObject*> tarr(cx, &obj->as<TypedArrayObject>());
            if (!cache.attachTypedArrayElement(cx, ion, tarr, idval))
                return false;
            attachedStub = true;
        }
    }

    if (!GetElementOperation(cx, JSOp(*pc), &lval, idval, res))
        return false;

    // Disable cache when we reach max stubs or update failed too much.
    if (!attachedStub) {
        cache.incFailedUpdates();
        if (cache.shouldDisable()) {
            IonSpew(IonSpew_InlineCaches, "Disable inline cache");
            cache.disable();
        }
    } else {
        cache.resetFailedUpdates();
    }

    types::TypeScript::Monitor(cx, script, pc, res);
    return true;
}

void
GetElementIC::reset()
{
    RepatchIonCache::reset();
    hasDenseStub_ = false;
    hasStrictArgumentsStub_ = false;
    hasNormalArgumentsStub_ = false;
}

static bool
IsElementSetInlineable(HandleObject obj, HandleValue index)
{
    if (!obj->is<ArrayObject>())
        return false;

    if (obj->watched())
        return false;

    if (!index.isInt32())
        return false;

    // The object may have a setter definition,
    // either directly, or via a prototype, or via the target object for a prototype
    // which is a proxy, that handles a particular integer write.
    // Scan the prototype and shape chain to make sure that this is not the case.
    JSObject *curObj = obj;
    while (curObj) {
        // Ensure object is native.
        if (!curObj->isNative())
            return false;

        // Ensure all indexed properties are stored in dense elements.
        if (curObj->isIndexed())
            return false;

        curObj = curObj->getProto();
    }

    return true;
}

bool
SetElementIC::attachDenseElement(JSContext *cx, IonScript *ion, JSObject *obj, const Value &idval)
{
    JS_ASSERT(obj->isNative());
    JS_ASSERT(idval.isInt32());

    Label failures;
    Label outOfBounds; // index >= capacity || index > initialized length

    MacroAssembler masm(cx);
    RepatchStubAppender attacher(*this);

    // Guard object is a dense array.
    RootedShape shape(cx, obj->lastProperty());
    if (!shape)
        return false;
    masm.branchTestObjShape(Assembler::NotEqual, object(), shape, &failures);

    // Ensure the index is an int32 value.
    ValueOperand indexVal = index();
    masm.branchTestInt32(Assembler::NotEqual, indexVal, &failures);

    // Unbox the index.
    Register index = masm.extractInt32(indexVal, tempToUnboxIndex());

    {
        // Load obj->elements.
        Register elements = temp();
        masm.loadPtr(Address(object(), JSObject::offsetOfElements()), elements);

        // Compute the location of the element.
        BaseIndex target(elements, index, TimesEight);

        // Guard that we can increase the initialized length.
        Address capacity(elements, ObjectElements::offsetOfCapacity());
        masm.branch32(Assembler::BelowOrEqual, capacity, index, &outOfBounds);

        // Guard on the initialized length.
        Address initLength(elements, ObjectElements::offsetOfInitializedLength());
        masm.branch32(Assembler::Below, initLength, index, &outOfBounds);

        // if (initLength == index)
        Label markElem, storeElem;
        masm.branch32(Assembler::NotEqual, initLength, index, &markElem);
        {
            // Increase initialize length.
            Int32Key newLength(index);
            masm.bumpKey(&newLength, 1);
            masm.storeKey(newLength, initLength);

            // Increase length if needed.
            Label bumpedLength;
            Address length(elements, ObjectElements::offsetOfLength());
            masm.branch32(Assembler::AboveOrEqual, length, index, &bumpedLength);
            masm.storeKey(newLength, length);
            masm.bind(&bumpedLength);

            // Restore the index.
            masm.bumpKey(&newLength, -1);
            masm.jump(&storeElem);
        }
        // else
        {
            // Mark old element.
            masm.bind(&markElem);
            if (cx->zone()->needsBarrier())
                masm.callPreBarrier(target, MIRType_Value);
        }

        // Store the value.
        masm.bind(&storeElem);
        masm.storeConstantOrRegister(value(), target);
    }
    attacher.jumpRejoin(masm);

    // All failures flow to here.
    masm.bind(&outOfBounds);
    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    setHasDenseStub();
    return linkAndAttachStub(cx, masm, attacher, ion, "dense array");
}

bool
SetElementIC::update(JSContext *cx, size_t cacheIndex, HandleObject obj,
                     HandleValue idval, HandleValue value)
{
    IonScript *ion = GetTopIonJSScript(cx)->ionScript();
    SetElementIC &cache = ion->getCache(cacheIndex).toSetElement();

    if (cache.canAttachStub() && !cache.hasDenseStub() && IsElementSetInlineable(obj, idval)) {
        if (!cache.attachDenseElement(cx, ion, obj, idval))
            return false;
    }

    if (!SetObjectElement(cx, obj, idval, value, cache.strict()))
        return false;
    return true;
}

void
SetElementIC::reset()
{
    RepatchIonCache::reset();
    hasDenseStub_ = false;
}

bool
GetElementParIC::attachReadSlot(LockedJSContext &cx, IonScript *ion, JSObject *obj,
                                const Value &idval, PropertyName *name, JSObject *holder,
                                Shape *shape)
{
    MacroAssembler masm(cx);
    DispatchStubPrepender attacher(*this);

    // Guard on the index value.
    Label failures;
    ValueOperand val = index().reg().valueReg();
    masm.branchTestValue(Assembler::NotEqual, val, idval, &failures);

    GenerateReadSlot(cx, ion, masm, attacher, obj, name, holder, shape, object(), output(),
                     &failures);

    return linkAndAttachStub(cx, masm, attacher, ion, "parallel getelem reading");
}

bool
GetElementParIC::attachDenseElement(LockedJSContext &cx, IonScript *ion, JSObject *obj,
                                    const Value &idval)
{
    MacroAssembler masm(cx);
    DispatchStubPrepender attacher(*this);
    if (!GenerateDenseElement(cx, masm, attacher, obj, idval, object(), index(), output()))
        return false;

    return linkAndAttachStub(cx, masm, attacher, ion, "parallel dense element");
}

bool
GetElementParIC::attachTypedArrayElement(LockedJSContext &cx, IonScript *ion,
                                         TypedArrayObject *tarr, const Value &idval)
{
    MacroAssembler masm(cx);
    DispatchStubPrepender attacher(*this);
    GenerateTypedArrayElement(cx, masm, attacher, tarr, idval, object(), index(), output());
    return linkAndAttachStub(cx, masm, attacher, ion, "parallel typed array");
}

ParallelResult
GetElementParIC::update(ForkJoinSlice *slice, size_t cacheIndex, HandleObject obj,
                        HandleValue idval, MutableHandleValue vp)
{
    AutoFlushCache afc("GetElementParCache");

    IonScript *ion = GetTopIonJSScript(slice)->parallelIonScript();
    GetElementParIC &cache = ion->getCache(cacheIndex).toGetElementPar();

    // Try to get the element early, as the pure path doesn't need a lock. If
    // we can't do it purely, bail out of parallel execution.
    if (!GetObjectElementOperationPure(slice, obj, idval, vp.address()))
        return TP_RETRY_SEQUENTIALLY;

    // Avoid unnecessary locking if cannot attach stubs.
    if (!cache.canAttachStub())
        return TP_SUCCESS;

    {
        LockedJSContext cx(slice);

        if (cache.canAttachStub()) {
            bool alreadyStubbed;
            if (!cache.hasOrAddStubbedShape(cx, obj->lastProperty(), &alreadyStubbed))
                return TP_FATAL;
            if (alreadyStubbed)
                return TP_SUCCESS;

            jsid id;
            if (!ValueToIdPure(idval, &id))
                return TP_FATAL;

            bool attachedStub = false;

            if (cache.monitoredResult() &&
                GetElementIC::canAttachGetProp(obj, idval, id))
            {
                RootedShape shape(cx);
                RootedObject holder(cx);
                PropertyName *name = JSID_TO_ATOM(id)->asPropertyName();
                if (GetPropertyParIC::canAttachReadSlot(cx, cache, cache.output(), obj,
                                                        name, &holder, &shape))
                {
                    if (!cache.attachReadSlot(cx, ion, obj, idval, name, holder, shape))
                        return TP_FATAL;
                    attachedStub = true;
                }
            }
            if (!attachedStub &&
                GetElementIC::canAttachDenseElement(obj, idval))
            {
                if (!cache.attachDenseElement(cx, ion, obj, idval))
                    return TP_FATAL;
                attachedStub = true;
            }
            if (!attachedStub &&
                GetElementIC::canAttachTypedArrayElement(obj, idval, cache.output()))
            {
                if (!cache.attachTypedArrayElement(cx, ion, &obj->as<TypedArrayObject>(), idval))
                    return TP_FATAL;
                attachedStub = true;
            }
        }
    }

    return TP_SUCCESS;
}

bool
BindNameIC::attachGlobal(JSContext *cx, IonScript *ion, JSObject *scopeChain)
{
    JS_ASSERT(scopeChain->is<GlobalObject>());

    MacroAssembler masm(cx);
    RepatchStubAppender attacher(*this);

    // Guard on the scope chain.
    attacher.branchNextStub(masm, Assembler::NotEqual, scopeChainReg(),
                            ImmGCPtr(scopeChain));
    masm.movePtr(ImmGCPtr(scopeChain), outputReg());

    attacher.jumpRejoin(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "global");
}

static inline void
GenerateScopeChainGuard(MacroAssembler &masm, JSObject *scopeObj,
                        Register scopeObjReg, Shape *shape, Label *failures)
{
    if (scopeObj->is<CallObject>()) {
        // We can skip a guard on the call object if the script's bindings are
        // guaranteed to be immutable (and thus cannot introduce shadowing
        // variables).
        CallObject *callObj = &scopeObj->as<CallObject>();
        if (!callObj->isForEval()) {
            JSFunction *fun = &callObj->callee();
            JSScript *script = fun->nonLazyScript();
            if (!script->funHasExtensibleScope)
                return;
        }
    } else if (scopeObj->is<GlobalObject>()) {
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
        JS_ASSERT(IsCacheableNonGlobalScope(tobj) || tobj->is<GlobalObject>());

        GenerateScopeChainGuard(masm, tobj, outputReg, NULL, failures);
        if (tobj == holder)
            break;

        // Load the next link.
        tobj = &tobj->as<ScopeObject>().enclosingScope();
        masm.extractObject(Address(outputReg, ScopeObject::offsetOfEnclosingScope()), outputReg);
    }
}

bool
BindNameIC::attachNonGlobal(JSContext *cx, IonScript *ion, JSObject *scopeChain, JSObject *holder)
{
    JS_ASSERT(IsCacheableNonGlobalScope(scopeChain));

    MacroAssembler masm(cx);
    RepatchStubAppender attacher(*this);

    // Guard on the shape of the scope chain.
    Label failures;
    attacher.branchNextStubOrLabel(masm, Assembler::NotEqual,
                                   Address(scopeChainReg(), JSObject::offsetOfShape()),
                                   ImmGCPtr(scopeChain->lastProperty()),
                                   holder != scopeChain ? &failures : NULL);

    if (holder != scopeChain) {
        JSObject *parent = &scopeChain->as<ScopeObject>().enclosingScope();
        masm.extractObject(Address(scopeChainReg(), ScopeObject::offsetOfEnclosingScope()), outputReg());

        GenerateScopeChainGuards(masm, parent, holder, outputReg(), &failures);
    } else {
        masm.movePtr(scopeChainReg(), outputReg());
    }

    // At this point outputReg holds the object on which the property
    // was found, so we're done.
    attacher.jumpRejoin(masm);

    // All failures flow to here, so there is a common point to patch.
    if (holder != scopeChain) {
        masm.bind(&failures);
        attacher.jumpNextStub(masm);
    }

    return linkAndAttachStub(cx, masm, attacher, ion, "non-global");
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

        scopeChain = &scopeChain->as<ScopeObject>().enclosingScope();
        if (!scopeChain) {
            IonSpew(IonSpew_InlineCaches, "Scope chain indirect hit");
            return false;
        }
    }

    MOZ_ASSUME_UNREACHABLE("Invalid scope chain");
}

JSObject *
BindNameIC::update(JSContext *cx, size_t cacheIndex, HandleObject scopeChain)
{
    AutoFlushCache afc ("BindNameCache");

    IonScript *ion = GetTopIonJSScript(cx)->ionScript();
    BindNameIC &cache = ion->getCache(cacheIndex).toBindName();
    HandlePropertyName name = cache.name();

    RootedObject holder(cx);
    if (scopeChain->is<GlobalObject>()) {
        holder = scopeChain;
    } else {
        if (!LookupNameWithGlobalDefault(cx, name, scopeChain, &holder))
            return NULL;
    }

    // Stop generating new stubs once we hit the stub count limit, see
    // GetPropertyCache.
    if (cache.canAttachStub()) {
        if (scopeChain->is<GlobalObject>()) {
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
NameIC::attachReadSlot(JSContext *cx, IonScript *ion, HandleObject scopeChain, HandleObject holder,
                       HandleShape shape)
{
    MacroAssembler masm(cx);
    Label failures;
    RepatchStubAppender attacher(*this);

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

    attacher.jumpRejoin(masm);

    if (failures.used()) {
        masm.bind(&failures);
        attacher.jumpNextStub(masm);
    }

    return linkAndAttachStub(cx, masm, attacher, ion, "generic");
}

static bool
IsCacheableNameReadSlot(JSContext *cx, HandleObject scopeChain, HandleObject obj,
                        HandleObject holder, HandleShape shape, jsbytecode *pc,
                        const TypedOrValueRegister &output)
{
    if (!shape)
        return false;
    if (!obj->isNative())
        return false;
    if (obj != holder)
        return false;

    if (obj->is<GlobalObject>()) {
        // Support only simple property lookups.
        if (!IsCacheableGetPropReadSlot(obj, holder, shape) &&
            !IsCacheableNoProperty(obj, holder, shape, pc, output))
            return false;
    } else if (obj->is<CallObject>()) {
        if (!shape->hasDefaultGetter())
            return false;
    } else {
        // We don't yet support lookups on Block or DeclEnv objects.
        return false;
    }

    RootedObject obj2(cx, scopeChain);
    while (obj2) {
        if (!IsCacheableNonGlobalScope(obj2) && !obj2->is<GlobalObject>())
            return false;

        // Stop once we hit the global or target obj.
        if (obj2->is<GlobalObject>() || obj2 == obj)
            break;

        obj2 = obj2->enclosingScope();
    }

    return obj == obj2;
}

bool
NameIC::attachCallGetter(JSContext *cx, IonScript *ion, JSObject *obj, JSObject *holder,
                         HandleShape shape, void *returnAddr)
{
    MacroAssembler masm(cx);

    // Need to set correct framePushed on the masm so that exit frame descriptors are
    // properly constructed.
    masm.setFramePushed(ion->frameSize());

    RepatchStubAppender attacher(*this);
    if (!GenerateCallGetter(cx, ion, masm, attacher, obj, name(), holder, shape, liveRegs_,
                            scopeChainReg(), outputReg(), returnAddr))
    {
         return false;
    }

    const char *attachKind = "name getter";
    return linkAndAttachStub(cx, masm, attacher, ion, attachKind);
}

static bool
IsCacheableNameCallGetter(JSObject *scopeChain, JSObject *obj, JSObject *holder, Shape *shape)
{
    if (obj != scopeChain)
        return false;

    if (!obj->is<GlobalObject>())
        return false;

    return IsCacheableGetPropCallNative(obj, holder, shape) ||
        IsCacheableGetPropCallPropertyOp(obj, holder, shape);
}

bool
NameIC::update(JSContext *cx, size_t cacheIndex, HandleObject scopeChain,
               MutableHandleValue vp)
{
    AutoFlushCache afc ("GetNameCache");

    void *returnAddr;
    IonScript *ion = GetTopIonJSScript(cx, &returnAddr)->ionScript();

    NameIC &cache = ion->getCache(cacheIndex).toName();
    RootedPropertyName name(cx, cache.name());

    RootedScript script(cx);
    jsbytecode *pc;
    cache.getScriptedLocation(&script, &pc);

    RootedObject obj(cx);
    RootedObject holder(cx);
    RootedShape shape(cx);
    if (!LookupName(cx, name, scopeChain, &obj, &holder, &shape))
        return false;

    if (cache.canAttachStub()) {
        if (IsCacheableNameReadSlot(cx, scopeChain, obj, holder, shape, pc, cache.outputReg())) {
            if (!cache.attachReadSlot(cx, ion, scopeChain, obj, shape))
                return false;
        } else if (IsCacheableNameCallGetter(scopeChain, obj, holder, shape)) {
            if (!cache.attachCallGetter(cx, ion, obj, holder, shape, returnAddr))
                return false;
        }
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

bool
CallsiteCloneIC::attach(JSContext *cx, IonScript *ion, HandleFunction original,
                        HandleFunction clone)
{
    MacroAssembler masm(cx);
    RepatchStubAppender attacher(*this);

    // Guard against object identity on the original.
    attacher.branchNextStub(masm, Assembler::NotEqual, calleeReg(), ImmGCPtr(original));

    // Load the clone.
    masm.movePtr(ImmGCPtr(clone), outputReg());

    attacher.jumpRejoin(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "generic");
}

JSObject *
CallsiteCloneIC::update(JSContext *cx, size_t cacheIndex, HandleObject callee)
{
    AutoFlushCache afc ("CallsiteCloneCache");

    // Act as the identity for functions that are not clone-at-callsite, as we
    // generate this cache as long as some callees are clone-at-callsite.
    RootedFunction fun(cx, &callee->as<JSFunction>());
    if (!fun->hasScript() || !fun->nonLazyScript()->shouldCloneAtCallsite)
        return fun;

    IonScript *ion = GetTopIonJSScript(cx)->ionScript();
    CallsiteCloneIC &cache = ion->getCache(cacheIndex).toCallsiteClone();

    RootedFunction clone(cx, CloneFunctionAtCallsite(cx, fun, cache.callScript(), cache.callPc()));
    if (!clone)
        return NULL;

    if (cache.canAttachStub()) {
        if (!cache.attach(cx, ion, fun, clone))
            return NULL;
    }

    return clone;
}
