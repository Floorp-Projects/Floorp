/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/IonCaches.h"

#include "mozilla/SizePrintfMacros.h"
#include "mozilla/TemplateLib.h"

#include "jstypes.h"

#include "builtin/TypedObject.h"
#include "jit/BaselineIC.h"
#include "jit/Ion.h"
#include "jit/JitcodeMap.h"
#include "jit/JitSpewer.h"
#include "jit/Linker.h"
#include "jit/Lowering.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "jit/VMFunctions.h"
#include "js/Proxy.h"
#include "proxy/Proxy.h"
#include "vm/Shape.h"
#include "vm/Stack.h"

#include "jit/JitFrames-inl.h"
#include "jit/MacroAssembler-inl.h"
#include "jit/shared/Lowering-shared-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/Shape-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::tl::FloorLog2;

typedef Rooted<TypedArrayObject*> RootedTypedArrayObject;

void
CodeLocationJump::repoint(JitCode* code, MacroAssembler* masm)
{
    MOZ_ASSERT(state_ == Relative);
    size_t new_off = (size_t)raw_;
#ifdef JS_SMALL_BRANCH
    size_t jumpTableEntryOffset = reinterpret_cast<size_t>(jumpTableEntry_);
#endif
    if (masm != nullptr) {
#ifdef JS_CODEGEN_X64
        MOZ_ASSERT((uint64_t)raw_ <= UINT32_MAX);
#endif
        new_off = (uintptr_t)raw_;
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
CodeLocationLabel::repoint(JitCode* code, MacroAssembler* masm)
{
     MOZ_ASSERT(state_ == Relative);
     size_t new_off = (size_t)raw_;
     if (masm != nullptr) {
#ifdef JS_CODEGEN_X64
        MOZ_ASSERT((uint64_t)raw_ <= UINT32_MAX);
#endif
        new_off = (uintptr_t)raw_;
     }
     MOZ_ASSERT(new_off < code->instructionsSize());

     raw_ = code->raw() + new_off;
     setAbsolute();
}

void
CodeOffsetJump::fixup(MacroAssembler* masm)
{
#ifdef JS_SMALL_BRANCH
     jumpTableIndex_ = masm->actualIndex(jumpTableIndex_);
#endif
}

const char*
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
// one. If a non-nullptr label is passed in, a |branchPtr| will be made to
// that label instead of a |branchPtrWithPatch| to the next stub.
class IonCache::StubAttacher
{
  protected:
    bool hasNextStubOffset_ : 1;
    bool hasStubCodePatchOffset_ : 1;

    IonCache& cache_;

    CodeLocationLabel rejoinLabel_;
    CodeOffsetJump nextStubOffset_;
    CodeOffsetJump rejoinOffset_;
    CodeOffset stubCodePatchOffset_;

  public:
    explicit StubAttacher(IonCache& cache)
      : hasNextStubOffset_(false),
        hasStubCodePatchOffset_(false),
        cache_(cache),
        rejoinLabel_(cache.rejoinLabel_),
        nextStubOffset_(),
        rejoinOffset_(),
        stubCodePatchOffset_()
    { }

    // Value used instead of the JitCode self-reference of generated
    // stubs. This value is needed for marking calls made inside stubs. This
    // value would be replaced by the attachStub function after the allocation
    // of the JitCode. The self-reference is used to keep the stub path alive
    // even if the IonScript is invalidated or if the IC is flushed.
    static const void* const STUB_ADDR;

    template <class T1, class T2>
    void branchNextStub(MacroAssembler& masm, Assembler::Condition cond, T1 op1, T2 op2) {
        MOZ_ASSERT(!hasNextStubOffset_);
        RepatchLabel nextStub;
        nextStubOffset_ = masm.branchPtrWithPatch(cond, op1, op2, &nextStub);
        hasNextStubOffset_ = true;
        masm.bind(&nextStub);
    }

    template <class T1, class T2>
    void branchNextStubOrLabel(MacroAssembler& masm, Assembler::Condition cond, T1 op1, T2 op2,
                               Label* label)
    {
        if (label != nullptr)
            masm.branchPtr(cond, op1, op2, label);
        else
            branchNextStub(masm, cond, op1, op2);
    }

    void jumpRejoin(MacroAssembler& masm) {
        RepatchLabel rejoin;
        rejoinOffset_ = masm.jumpWithPatch(&rejoin);
        masm.bind(&rejoin);
    }

    void jumpNextStub(MacroAssembler& masm) {
        MOZ_ASSERT(!hasNextStubOffset_);
        RepatchLabel nextStub;
        nextStubOffset_ = masm.jumpWithPatch(&nextStub);
        hasNextStubOffset_ = true;
        masm.bind(&nextStub);
    }

    void pushStubCodePointer(MacroAssembler& masm) {
        // Push the JitCode pointer for the stub we're generating.
        // WARNING:
        // WARNING: If JitCode ever becomes relocatable, the following code is incorrect.
        // WARNING: Note that we're not marking the pointer being pushed as an ImmGCPtr.
        // WARNING: This location will be patched with the pointer of the generated stub,
        // WARNING: such as it can be marked when a call is made with this stub. Be aware
        // WARNING: that ICs are not marked and so this stub will only be kept alive iff
        // WARNING: it is on the stack at the time of the GC. No ImmGCPtr is needed as the
        // WARNING: stubs are flushed on GC.
        // WARNING:
        MOZ_ASSERT(!hasStubCodePatchOffset_);
        stubCodePatchOffset_ = masm.PushWithPatch(ImmPtr(STUB_ADDR));
        hasStubCodePatchOffset_ = true;
    }

    void patchRejoinJump(MacroAssembler& masm, JitCode* code) {
        rejoinOffset_.fixup(&masm);
        CodeLocationJump rejoinJump(code, rejoinOffset_);
        PatchJump(rejoinJump, rejoinLabel_);
    }

    void patchStubCodePointer(JitCode* code) {
        if (hasStubCodePatchOffset_) {
            Assembler::PatchDataWithValueCheck(CodeLocationLabel(code, stubCodePatchOffset_),
                                               ImmPtr(code), ImmPtr(STUB_ADDR));
        }
    }

    void patchNextStubJump(MacroAssembler& masm, JitCode* code) {
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

const void* const IonCache::StubAttacher::STUB_ADDR = (void*)0xdeadc0de;

void
IonCache::emitInitialJump(MacroAssembler& masm, RepatchLabel& entry)
{
    initialJump_ = masm.jumpWithPatch(&entry);
    lastJump_ = initialJump_;
    Label label;
    masm.bind(&label);
    rejoinLabel_ = CodeOffset(label.offset());
}

void
IonCache::attachStub(MacroAssembler& masm, StubAttacher& attacher, CodeLocationJump lastJump,
                     Handle<JitCode*> code)
{
    MOZ_ASSERT(canAttachStub());
    incrementStubCount();

    // Patch the previous nextStubJump of the last stub, or the jump from the
    // codeGen, to jump into the newly allocated code.
    PatchJump(lastJump, CodeLocationLabel(code), Reprotect);
}

IonCache::LinkStatus
IonCache::linkCode(JSContext* cx, MacroAssembler& masm, StubAttacher& attacher, IonScript* ion,
                   JitCode** code)
{
    Linker linker(masm);
    *code = linker.newCode<CanGC>(cx, ION_CODE);
    if (!*code)
        return LINK_ERROR;

    if (ion->invalidated())
        return CACHE_FLUSHED;

    // Update the success path to continue after the IC initial jump.
    attacher.patchRejoinJump(masm, *code);

    // Replace the STUB_ADDR constant by the address of the generated stub, such
    // as it can be kept alive even if the cache is flushed (see
    // MarkJitExitFrame).
    attacher.patchStubCodePointer(*code);

    // Update the failure path.
    attacher.patchNextStubJump(masm, *code);

    return LINK_GOOD;
}

bool
IonCache::linkAndAttachStub(JSContext* cx, MacroAssembler& masm, StubAttacher& attacher,
                            IonScript* ion, const char* attachKind,
                            JS::TrackedOutcome trackedOutcome)
{
    CodeLocationJump lastJumpBefore = lastJump_;
    Rooted<JitCode*> code(cx);
    {
        // Need to exit the AutoFlushICache context to flush the cache
        // before attaching the stub below.
        AutoFlushICache afc("IonCache");
        LinkStatus status = linkCode(cx, masm, attacher, ion, code.address());
        if (status != LINK_GOOD)
            return status != LINK_ERROR;
    }

    if (pc_) {
        JitSpew(JitSpew_IonIC, "Cache %p(%s:%" PRIuSIZE "/%" PRIuSIZE ") generated %s %s stub at %p",
                this, script_->filename(), script_->lineno(), script_->pcToOffset(pc_),
                attachKind, CacheName(kind()), code->raw());
    } else {
        JitSpew(JitSpew_IonIC, "Cache %p generated %s %s stub at %p",
                this, attachKind, CacheName(kind()), code->raw());
    }

#ifdef JS_ION_PERF
    writePerfSpewerJitCodeProfile(code, "IonCache");
#endif

    attachStub(masm, attacher, lastJumpBefore, code);

    // Add entry to native => bytecode mapping for this stub if needed.
    if (cx->runtime()->jitRuntime()->isProfilerInstrumentationEnabled(cx->runtime())) {
        JitcodeGlobalEntry::IonCacheEntry entry;
        entry.init(code, code->raw(), code->rawEnd(), rejoinAddress(), trackedOutcome);

        // Add entry to the global table.
        JitcodeGlobalTable* globalTable = cx->runtime()->jitRuntime()->getJitcodeGlobalTable();
        if (!globalTable->addEntry(entry, cx->runtime())) {
            entry.destroy();
            ReportOutOfMemory(cx);
            return false;
        }

        // Mark the jitcode as having a bytecode map.
        code->setHasBytecodeMap();
    } else {
        JitcodeGlobalEntry::DummyEntry entry;
        entry.init(code, code->raw(), code->rawEnd());

        // Add entry to the global table.
        JitcodeGlobalTable* globalTable = cx->runtime()->jitRuntime()->getJitcodeGlobalTable();
        if (!globalTable->addEntry(entry, cx->runtime())) {
            entry.destroy();
            ReportOutOfMemory(cx);
            return false;
        }

        // Mark the jitcode as having a bytecode map.
        code->setHasBytecodeMap();
    }

    // Report masm OOM errors here, so all our callers can:
    // return linkAndAttachStub(...);
    if (masm.oom()) {
        ReportOutOfMemory(cx);
        return false;
    }

    return true;
}

void
IonCache::updateBaseAddress(JitCode* code, MacroAssembler& masm)
{
    fallbackLabel_.repoint(code, &masm);
    initialJump_.repoint(code, &masm);
    lastJump_.repoint(code, &masm);
    rejoinLabel_.repoint(code, &masm);
}

void
IonCache::trace(JSTracer* trc)
{
    if (script_)
        TraceManuallyBarrieredEdge(trc, &script_, "IonCache::script_");
}

void*
jit::GetReturnAddressToIonCode(JSContext* cx)
{
    JitFrameIterator iter(cx);
    MOZ_ASSERT(iter.type() == JitFrame_Exit,
               "An exit frame is expected as update functions are called with a VMFunction.");

    void* returnAddr = iter.returnAddress();
#ifdef DEBUG
    ++iter;
    MOZ_ASSERT(iter.isIonJS());
#endif
    return returnAddr;
}

static void
GeneratePrototypeGuards(JSContext* cx, IonScript* ion, MacroAssembler& masm, JSObject* obj,
                        JSObject* holder, Register objectReg, Register scratchReg,
                        Label* failures)
{
    /*
     * The guards here protect against the effects of JSObject::swap(). If the prototype chain
     * is directly altered, then TI will toss the jitcode, so we don't have to worry about
     * it, and any other change to the holder, or adding a shadowing property will result
     * in reshaping the holder, and thus the failure of the shape guard.
     */
    MOZ_ASSERT(obj != holder);

    if (obj->hasUncacheableProto()) {
        // Note: objectReg and scratchReg may be the same register, so we cannot
        // use objectReg in the rest of this function.
        masm.loadPtr(Address(objectReg, JSObject::offsetOfGroup()), scratchReg);
        Address proto(scratchReg, ObjectGroup::offsetOfProto());
        masm.branchPtr(Assembler::NotEqual, proto, ImmGCPtr(obj->staticPrototype()), failures);
    }

    JSObject* pobj = obj->staticPrototype();
    if (!pobj)
        return;
    while (pobj != holder) {
        if (pobj->hasUncacheableProto()) {
            masm.movePtr(ImmGCPtr(pobj), scratchReg);
            Address groupAddr(scratchReg, JSObject::offsetOfGroup());
            if (pobj->isSingleton()) {
                // Singletons can have their group's |proto| mutated directly.
                masm.loadPtr(groupAddr, scratchReg);
                Address protoAddr(scratchReg, ObjectGroup::offsetOfProto());
                masm.branchPtr(Assembler::NotEqual, protoAddr, ImmGCPtr(pobj->staticPrototype()),
                              failures);
            } else {
                masm.branchPtr(Assembler::NotEqual, groupAddr, ImmGCPtr(pobj->group()), failures);
            }
        }

        pobj = pobj->staticPrototype();
    }
}

// Note: This differs from IsCacheableProtoChain in BaselineIC.cpp in that
// Ion caches can deal with objects on the proto chain that have uncacheable
// prototypes.
bool
jit::IsCacheableProtoChainForIonOrCacheIR(JSObject* obj, JSObject* holder)
{
    while (obj != holder) {
        /*
         * We cannot assume that we find the holder object on the prototype
         * chain and must check for null proto. The prototype chain can be
         * altered during the lookupProperty call.
         */
        JSObject* proto = obj->staticPrototype();
        if (!proto || !proto->isNative())
            return false;
        obj = proto;
    }
    return true;
}

bool
jit::IsCacheableGetPropReadSlotForIonOrCacheIR(JSObject* obj, JSObject* holder, PropertyResult prop)
{
    if (!prop || !IsCacheableProtoChainForIonOrCacheIR(obj, holder))
        return false;

    Shape* shape = prop.shape();
    if (!shape->hasSlot() || !shape->hasDefaultGetter())
        return false;

    return true;
}

static bool
IsCacheableNoProperty(JSObject* obj, JSObject* holder, PropertyResult prop, jsbytecode* pc,
                      const TypedOrValueRegister& output)
{
    if (prop)
        return false;

    MOZ_ASSERT(!holder);

    // Just because we didn't find the property on the object doesn't mean it
    // won't magically appear through various engine hacks.
    if (obj->getClass()->getGetProperty())
        return false;

    // Don't generate missing property ICs if we skipped a non-native object, as
    // lookups may extend beyond the prototype chain (e.g. for DOMProxy
    // proxies).
    JSObject* obj2 = obj;
    while (obj2) {
        if (!obj2->isNative())
            return false;
        obj2 = obj2->staticPrototype();
    }

    // The pc is nullptr if the cache is idempotent. We cannot share missing
    // properties between caches because TI can only try to prove that a type is
    // contained, but does not attempts to check if something does not exists.
    // So the infered type of getprop would be missing and would not contain
    // undefined, as expected for missing properties.
    if (!pc)
        return false;

    // TI has not yet monitored an Undefined value. The fallback path will
    // monitor and invalidate the script.
    if (!output.hasValue())
        return false;

    return true;
}

bool
jit::IsCacheableGetPropCallNative(JSObject* obj, JSObject* holder, Shape* shape)
{
    if (!shape || !IsCacheableProtoChainForIonOrCacheIR(obj, holder))
        return false;

    if (!shape->hasGetterValue() || !shape->getterValue().isObject())
        return false;

    if (!shape->getterValue().toObject().is<JSFunction>())
        return false;

    JSFunction& getter = shape->getterValue().toObject().as<JSFunction>();
    if (!getter.isNative())
        return false;

    // Check for a getter that has jitinfo and whose jitinfo says it's
    // OK with both inner and outer objects.
    if (getter.jitInfo() && !getter.jitInfo()->needsOuterizedThisObject())
        return true;

    // For getters that need the WindowProxy (instead of the Window) as this
    // object, don't cache if obj is the Window, since our cache will pass that
    // instead of the WindowProxy.
    return !IsWindow(obj);
}

bool
jit::IsCacheableGetPropCallScripted(JSObject* obj, JSObject* holder, Shape* shape,
                                    bool* isTemporarilyUnoptimizable)
{
    if (!shape || !IsCacheableProtoChainForIonOrCacheIR(obj, holder))
        return false;

    if (!shape->hasGetterValue() || !shape->getterValue().isObject())
        return false;

    if (!shape->getterValue().toObject().is<JSFunction>())
        return false;

    // See IsCacheableGetPropCallNative.
    if (IsWindow(obj))
        return false;

    JSFunction& getter = shape->getterValue().toObject().as<JSFunction>();
    if (getter.isNative())
        return false;

    if (!getter.hasJITCode()) {
        if (isTemporarilyUnoptimizable)
            *isTemporarilyUnoptimizable = true;
        return false;
    }

    return true;
}

static bool
IsCacheableGetPropCallPropertyOp(JSObject* obj, JSObject* holder, Shape* shape)
{
    if (!shape || !IsCacheableProtoChainForIonOrCacheIR(obj, holder))
        return false;

    if (shape->hasSlot() || shape->hasGetterValue() || shape->hasDefaultGetter())
        return false;

    return true;
}

static void
TestMatchingReceiver(MacroAssembler& masm, IonCache::StubAttacher& attacher,
                     Register object, JSObject* obj, Label* failure,
                     bool alwaysCheckGroup = false)
{
    if (obj->is<UnboxedPlainObject>()) {
        MOZ_ASSERT(failure);

        masm.branchTestObjGroup(Assembler::NotEqual, object, obj->group(), failure);
        Address expandoAddress(object, UnboxedPlainObject::offsetOfExpando());
        if (UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando()) {
            masm.branchPtr(Assembler::Equal, expandoAddress, ImmWord(0), failure);
            Label success;
            masm.push(object);
            masm.loadPtr(expandoAddress, object);
            masm.branchTestObjShape(Assembler::Equal, object, expando->lastProperty(),
                                    &success);
            masm.pop(object);
            masm.jump(failure);
            masm.bind(&success);
            masm.pop(object);
        } else {
            masm.branchPtr(Assembler::NotEqual, expandoAddress, ImmWord(0), failure);
        }
    } else if (obj->is<UnboxedArrayObject>()) {
        MOZ_ASSERT(failure);
        masm.branchTestObjGroup(Assembler::NotEqual, object, obj->group(), failure);
    } else if (obj->is<TypedObject>()) {
        attacher.branchNextStubOrLabel(masm, Assembler::NotEqual,
                                       Address(object, JSObject::offsetOfGroup()),
                                       ImmGCPtr(obj->group()), failure);
    } else {
        Shape* shape = obj->maybeShape();
        MOZ_ASSERT(shape);

        attacher.branchNextStubOrLabel(masm, Assembler::NotEqual,
                                       Address(object, ShapedObject::offsetOfShape()),
                                       ImmGCPtr(shape), failure);

        if (alwaysCheckGroup)
            masm.branchTestObjGroup(Assembler::NotEqual, object, obj->group(), failure);
    }
}

static inline void
EmitLoadSlot(MacroAssembler& masm, NativeObject* holder, Shape* shape, Register holderReg,
             TypedOrValueRegister output, Register scratchReg)
{
    MOZ_ASSERT(holder);
    NativeObject::slotsSizeMustNotOverflow();
    if (holder->isFixedSlot(shape->slot())) {
        Address addr(holderReg, NativeObject::getFixedSlotOffset(shape->slot()));
        masm.loadTypedOrValue(addr, output);
    } else {
        masm.loadPtr(Address(holderReg, NativeObject::offsetOfSlots()), scratchReg);

        Address addr(scratchReg, holder->dynamicSlotIndex(shape->slot()) * sizeof(Value));
        masm.loadTypedOrValue(addr, output);
    }
}

// Callers are expected to have already guarded on the shape of the
// object, which guarantees the object is a DOM proxy.
static void
CheckDOMProxyExpandoDoesNotShadow(JSContext* cx, MacroAssembler& masm, JSObject* obj,
                                  jsid id, Register object, Label* stubFailure)
{
    MOZ_ASSERT(IsCacheableDOMProxy(obj));

    // Guard that the object does not have expando properties, or has an expando
    // which is known to not have the desired property.

    // For the remaining code, we need to reserve some registers to load a value.
    // This is ugly, but unvaoidable.
    AllocatableRegisterSet domProxyRegSet(RegisterSet::All());
    domProxyRegSet.take(AnyRegister(object));
    ValueOperand tempVal = domProxyRegSet.takeAnyValue();
    masm.pushValue(tempVal);

    Label failDOMProxyCheck;
    Label domProxyOk;

    Value expandoVal = GetProxyExtra(obj, GetDOMProxyExpandoSlot());

    masm.loadPtr(Address(object, ProxyObject::offsetOfValues()), tempVal.scratchReg());
    masm.loadValue(Address(tempVal.scratchReg(),
                           ProxyObject::offsetOfExtraSlotInValues(GetDOMProxyExpandoSlot())),
                   tempVal);

    if (!expandoVal.isObject() && !expandoVal.isUndefined()) {
        masm.branchTestValue(Assembler::NotEqual, tempVal, expandoVal, &failDOMProxyCheck);

        ExpandoAndGeneration* expandoAndGeneration = (ExpandoAndGeneration*)expandoVal.toPrivate();
        masm.movePtr(ImmPtr(expandoAndGeneration), tempVal.scratchReg());

        masm.branch64(Assembler::NotEqual,
                      Address(tempVal.scratchReg(),
                              ExpandoAndGeneration::offsetOfGeneration()),
                      Imm64(expandoAndGeneration->generation),
                      &failDOMProxyCheck);

        expandoVal = expandoAndGeneration->expando;
        masm.loadValue(Address(tempVal.scratchReg(),
                               ExpandoAndGeneration::offsetOfExpando()),
                       tempVal);
    }

    // If the incoming object does not have an expando object then we're sure we're not
    // shadowing.
    masm.branchTestUndefined(Assembler::Equal, tempVal, &domProxyOk);

    if (expandoVal.isObject()) {
        MOZ_ASSERT(!expandoVal.toObject().as<NativeObject>().contains(cx, id));

        // Reference object has an expando object that doesn't define the name. Check that
        // the incoming object has an expando object with the same shape.
        masm.branchTestObject(Assembler::NotEqual, tempVal, &failDOMProxyCheck);
        masm.extractObject(tempVal, tempVal.scratchReg());
        masm.branchPtr(Assembler::Equal,
                       Address(tempVal.scratchReg(), ShapedObject::offsetOfShape()),
                       ImmGCPtr(expandoVal.toObject().as<NativeObject>().lastProperty()),
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
GenerateReadSlot(JSContext* cx, IonScript* ion, MacroAssembler& masm,
                 IonCache::StubAttacher& attacher, MaybeCheckTDZ checkTDZ,
                 JSObject* obj, JSObject* holder, PropertyResult prop, Register object,
                 TypedOrValueRegister output, Label* failures = nullptr)
{
    // If there's a single jump to |failures|, we can patch the shape guard
    // jump directly. Otherwise, jump to the end of the stub, so there's a
    // common point to patch.
    bool multipleFailureJumps = (obj != holder)
                             || obj->is<UnboxedPlainObject>()
                             || (checkTDZ && output.hasValue())
                             || (failures != nullptr && failures->used());

    // If we have multiple failure jumps but didn't get a label from the
    // outside, make one ourselves.
    Label failures_;
    if (multipleFailureJumps && !failures)
        failures = &failures_;

    TestMatchingReceiver(masm, attacher, object, obj, failures);

    // If we need a scratch register, use either an output register or the
    // object register. After this point, we cannot jump directly to
    // |failures| since we may still have to pop the object register.
    bool restoreScratch = false;
    Register scratchReg = Register::FromCode(0); // Quell compiler warning.

    if (obj != holder ||
        obj->is<UnboxedPlainObject>() ||
        !holder->as<NativeObject>().isFixedSlot(prop.shape()->slot()))
    {
        if (output.hasValue()) {
            scratchReg = output.valueReg().scratchReg();
        } else if (output.type() == MIRType::Double) {
            scratchReg = object;
            masm.push(scratchReg);
            restoreScratch = true;
        } else {
            scratchReg = output.typedReg().gpr();
        }
    }

    // Fast path: single failure jump, no prototype guards.
    if (!multipleFailureJumps) {
        EmitLoadSlot(masm, &holder->as<NativeObject>(), prop.shape(), object, output, scratchReg);
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
            masm.movePtr(ImmGCPtr(holder), holderReg);
            masm.branchPtr(Assembler::NotEqual,
                           Address(holderReg, ShapedObject::offsetOfShape()),
                           ImmGCPtr(holder->as<NativeObject>().lastProperty()),
                           &prototypeFailures);
        } else {
            // The property does not exist. Guard on everything in the
            // prototype chain.
            JSObject* proto = obj->staticPrototype();
            Register lastReg = object;
            MOZ_ASSERT(scratchReg != object);
            while (proto) {
                masm.loadObjProto(lastReg, scratchReg);

                // Guard the shape of the current prototype.
                MOZ_ASSERT(proto->hasStaticPrototype());
                masm.branchPtr(Assembler::NotEqual,
                               Address(scratchReg, ShapedObject::offsetOfShape()),
                               ImmGCPtr(proto->as<NativeObject>().lastProperty()),
                               &prototypeFailures);

                proto = proto->staticPrototype();
                lastReg = scratchReg;
            }

            holderReg = InvalidReg;
        }
    } else if (obj->is<UnboxedPlainObject>()) {
        holder = obj->as<UnboxedPlainObject>().maybeExpando();
        holderReg = scratchReg;
        masm.loadPtr(Address(object, UnboxedPlainObject::offsetOfExpando()), holderReg);
    } else {
        holderReg = object;
    }

    // Slot access.
    if (holder) {
        EmitLoadSlot(masm, &holder->as<NativeObject>(), prop.shape(), holderReg, output,
                     scratchReg);
        if (checkTDZ && output.hasValue())
            masm.branchTestMagic(Assembler::Equal, output.valueReg(), failures);
    } else {
        masm.moveValue(UndefinedValue(), output.valueReg());
    }

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
EmitGetterCall(JSContext* cx, MacroAssembler& masm,
               IonCache::StubAttacher& attacher, JSObject* obj,
               JSObject* holder, HandleShape shape, bool holderIsReceiver,
               LiveRegisterSet liveRegs, Register object,
               TypedOrValueRegister output,
               void* returnAddr)
{
    MOZ_ASSERT(output.hasValue());
    MacroAssembler::AfterICSaveLive aic = masm.icSaveLive(liveRegs);

    MOZ_ASSERT_IF(obj != holder, !holderIsReceiver);

    // Remaining registers should basically be free, but we need to use |object| still
    // so leave it alone.
    AllocatableRegisterSet regSet(RegisterSet::All());
    regSet.take(AnyRegister(object));

    // This is a slower stub path, and we're going to be doing a call anyway.  Don't need
    // to try so hard to not use the stack.  Scratch regs are just taken from the register
    // set not including the input, current value saved on the stack, and restored when
    // we're done with it.
    Register scratchReg = regSet.takeAnyGeneral();

    // Shape has a JSNative, PropertyOp or scripted getter function.
    if (IsCacheableGetPropCallNative(obj, holder, shape)) {
        Register argJSContextReg = regSet.takeAnyGeneral();
        Register argUintNReg     = regSet.takeAnyGeneral();
        Register argVpReg        = regSet.takeAnyGeneral();

        JSFunction* target = &shape->getterValue().toObject().as<JSFunction>();
        MOZ_ASSERT(target);
        MOZ_ASSERT(target->isNative());

        // Native functions have the signature:
        //  bool (*)(JSContext*, unsigned, Value* vp)
        // Where vp[0] is space for an outparam, vp[1] is |this|, and vp[2] onward
        // are the function arguments.

        // Construct vp array:
        // Push object value for |this|
        masm.Push(TypedOrValueRegister(MIRType::Object, AnyRegister(object)));
        // Push callee/outparam.
        masm.Push(ObjectValue(*target));

        // Preload arguments into registers.
        masm.loadJSContext(argJSContextReg);
        masm.move32(Imm32(0), argUintNReg);
        masm.moveStackPtrTo(argVpReg);

        // Push marking data for later use.
        masm.Push(argUintNReg);
        attacher.pushStubCodePointer(masm);

        if (!masm.icBuildOOLFakeExitFrame(returnAddr, aic))
            return false;
        masm.enterFakeExitFrame(IonOOLNativeExitFrameLayoutToken);

        // Construct and execute call.
        masm.setupUnalignedABICall(scratchReg);
        masm.passABIArg(argJSContextReg);
        masm.passABIArg(argUintNReg);
        masm.passABIArg(argVpReg);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, target->native()));

        // Test for failure.
        masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

        // Load the outparam vp[0] into output register(s).
        Address outparam(masm.getStackPointer(), IonOOLNativeExitFrameLayout::offsetOfResult());
        masm.loadTypedOrValue(outparam, output);

        // masm.leaveExitFrame & pop locals
        masm.adjustStack(IonOOLNativeExitFrameLayout::Size(0));
    } else if (IsCacheableGetPropCallPropertyOp(obj, holder, shape)) {
        Register argJSContextReg = regSet.takeAnyGeneral();
        Register argObjReg       = regSet.takeAnyGeneral();
        Register argIdReg        = regSet.takeAnyGeneral();
        Register argVpReg        = regSet.takeAnyGeneral();

        GetterOp target = shape->getterOp();
        MOZ_ASSERT(target);

        // Push stubCode for marking.
        attacher.pushStubCodePointer(masm);

        // JSGetterOp: bool fn(JSContext* cx, HandleObject obj, HandleId id, MutableHandleValue vp)

        // Push args on stack first so we can take pointers to make handles.
        masm.Push(UndefinedValue());
        masm.moveStackPtrTo(argVpReg);

        // Push canonical jsid from shape instead of propertyname.
        masm.Push(shape->propid(), scratchReg);
        masm.moveStackPtrTo(argIdReg);

        // Push the holder.
        if (holderIsReceiver) {
            // When the holder is also the current receiver, we just have a shape guard,
            // so we might end up with a random object which is also guaranteed to have
            // this JSGetterOp.
            masm.Push(object);
        } else {
            // If the holder is on the prototype chain, the prototype-guarding
            // only allows objects with the same holder.
            masm.movePtr(ImmGCPtr(holder), scratchReg);
            masm.Push(scratchReg);
        }
        masm.moveStackPtrTo(argObjReg);

        masm.loadJSContext(argJSContextReg);

        if (!masm.icBuildOOLFakeExitFrame(returnAddr, aic))
            return false;
        masm.enterFakeExitFrame(IonOOLPropertyOpExitFrameLayoutToken);

        // Make the call.
        masm.setupUnalignedABICall(scratchReg);
        masm.passABIArg(argJSContextReg);
        masm.passABIArg(argObjReg);
        masm.passABIArg(argIdReg);
        masm.passABIArg(argVpReg);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, target));

        // Test for failure.
        masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

        // Load the outparam vp[0] into output register(s).
        Address outparam(masm.getStackPointer(), IonOOLPropertyOpExitFrameLayout::offsetOfResult());
        masm.loadTypedOrValue(outparam, output);

        // masm.leaveExitFrame & pop locals.
        masm.adjustStack(IonOOLPropertyOpExitFrameLayout::Size());
    } else {
        MOZ_ASSERT(IsCacheableGetPropCallScripted(obj, holder, shape));

        JSFunction* target = &shape->getterValue().toObject().as<JSFunction>();
        uint32_t framePushedBefore = masm.framePushed();

        // Construct IonICCallFrameLayout.
        uint32_t descriptor = MakeFrameDescriptor(masm.framePushed(), JitFrame_IonJS,
                                                  IonICCallFrameLayout::Size());
        attacher.pushStubCodePointer(masm);
        masm.Push(Imm32(descriptor));
        masm.Push(ImmPtr(returnAddr));

        // The JitFrameLayout pushed below will be aligned to JitStackAlignment,
        // so we just have to make sure the stack is aligned after we push the
        // |this| + argument Values.
        uint32_t argSize = (target->nargs() + 1) * sizeof(Value);
        uint32_t padding = ComputeByteAlignment(masm.framePushed() + argSize, JitStackAlignment);
        MOZ_ASSERT(padding % sizeof(uintptr_t) == 0);
        MOZ_ASSERT(padding < JitStackAlignment);
        masm.reserveStack(padding);

        for (size_t i = 0; i < target->nargs(); i++)
            masm.Push(UndefinedValue());
        masm.Push(TypedOrValueRegister(MIRType::Object, AnyRegister(object)));

        masm.movePtr(ImmGCPtr(target), scratchReg);

        descriptor = MakeFrameDescriptor(argSize + padding, JitFrame_IonICCall,
                                         JitFrameLayout::Size());
        masm.Push(Imm32(0)); // argc
        masm.Push(scratchReg);
        masm.Push(Imm32(descriptor));

        // Check stack alignment. Add sizeof(uintptr_t) for the return address.
        MOZ_ASSERT(((masm.framePushed() + sizeof(uintptr_t)) % JitStackAlignment) == 0);

        // The getter has JIT code now and we will only discard the getter's JIT
        // code when discarding all JIT code in the Zone, so we can assume it'll
        // still have JIT code.
        MOZ_ASSERT(target->hasJITCode());
        masm.loadPtr(Address(scratchReg, JSFunction::offsetOfNativeOrScript()), scratchReg);
        masm.loadBaselineOrIonRaw(scratchReg, scratchReg, nullptr);
        masm.callJit(scratchReg);
        masm.storeCallResultValue(output);

        masm.freeStack(masm.framePushed() - framePushedBefore);
    }

    masm.icRestoreLive(liveRegs, aic);
    return true;
}

static bool
GenerateCallGetter(JSContext* cx, IonScript* ion, MacroAssembler& masm,
                   IonCache::StubAttacher& attacher, JSObject* obj,
                   JSObject* holder, HandleShape shape, LiveRegisterSet& liveRegs, Register object,
                   TypedOrValueRegister output, void* returnAddr, Label* failures = nullptr)
{
    MOZ_ASSERT(output.hasValue());

    // Use the passed in label if there was one. Otherwise, we'll have to make our own.
    Label stubFailure;
    failures = failures ? failures : &stubFailure;

    TestMatchingReceiver(masm, attacher, object, obj, failures);

    Register scratchReg = output.valueReg().scratchReg();
    bool spillObjReg = scratchReg == object;
    Label pop1AndFail;
    Label* maybePopAndFail = failures;

    // If we're calling a getter on the global, inline the logic for the
    // 'this' hook on the global lexical env and manually push the global.
    if (IsGlobalLexicalEnvironment(obj)) {
        masm.extractObject(Address(object, EnvironmentObject::offsetOfEnclosingEnvironment()),
                           object);
    }

    // Save off the object register if it aliases the scratchReg
    if (spillObjReg) {
        masm.push(object);
        maybePopAndFail = &pop1AndFail;
    }

    // Note: this may clobber the object register if it's used as scratch.
    if (obj != holder)
        GeneratePrototypeGuards(cx, ion, masm, obj, holder, object, scratchReg, maybePopAndFail);

    // Guard on the holder's shape.
    Register holderReg = scratchReg;
    masm.movePtr(ImmGCPtr(holder), holderReg);
    masm.branchPtr(Assembler::NotEqual,
                   Address(holderReg, ShapedObject::offsetOfShape()),
                   ImmGCPtr(holder->as<NativeObject>().lastProperty()),
                   maybePopAndFail);

    if (spillObjReg)
        masm.pop(object);

    // Now we're good to go to invoke the native call.
    bool holderIsReceiver = (obj == holder);
    if (!EmitGetterCall(cx, masm, attacher, obj, holder, shape, holderIsReceiver, liveRegs, object,
                        output, returnAddr))
        return false;

    // Rejoin jump.
    attacher.jumpRejoin(masm);

    // Jump to next stub.
    if (spillObjReg) {
        masm.bind(&pop1AndFail);
        masm.pop(object);
    }
    masm.bind(failures);
    attacher.jumpNextStub(masm);

    return true;
}

static void
EmitIdGuard(MacroAssembler& masm, jsid id, TypedOrValueRegister idReg, Register objReg,
            Register scratchReg, Label* failures)
{
    MOZ_ASSERT(JSID_IS_STRING(id) || JSID_IS_SYMBOL(id));

    MOZ_ASSERT(idReg.type() == MIRType::String ||
               idReg.type() == MIRType::Symbol ||
               idReg.type() == MIRType::Value);

    Register payloadReg;
    if (idReg.type() == MIRType::Value) {
        ValueOperand val = idReg.valueReg();
        if (JSID_IS_SYMBOL(id)) {
            masm.branchTestSymbol(Assembler::NotEqual, val, failures);
        } else {
            MOZ_ASSERT(JSID_IS_STRING(id));
            masm.branchTestString(Assembler::NotEqual, val, failures);
        }
        masm.unboxNonDouble(val, scratchReg);
        payloadReg = scratchReg;
    } else {
        payloadReg = idReg.typedReg().gpr();
    }

    if (JSID_IS_SYMBOL(id)) {
        // For symbols, we can just do a pointer comparison.
        masm.branchPtr(Assembler::NotEqual, payloadReg, ImmGCPtr(JSID_TO_SYMBOL(id)), failures);
    } else {
        PropertyName* name = JSID_TO_ATOM(id)->asPropertyName();

        Label equal;
        masm.branchPtr(Assembler::Equal, payloadReg, ImmGCPtr(name), &equal);

        // The pointers are not equal, so if the input string is also an atom it
        // must be a different string.
        masm.branchTest32(Assembler::NonZero, Address(payloadReg, JSString::offsetOfFlags()),
                          Imm32(JSString::ATOM_BIT), failures);

        // Check the length.
        masm.branch32(Assembler::NotEqual, Address(payloadReg, JSString::offsetOfLength()),
                      Imm32(name->length()), failures);

        // We have a non-atomized string with the same length. For now call a helper
        // function to do the comparison.
        LiveRegisterSet volatileRegs(RegisterSet::Volatile());
        masm.PushRegsInMask(volatileRegs);

        if (!volatileRegs.has(objReg))
            masm.push(objReg);

        masm.setupUnalignedABICall(objReg);
        masm.movePtr(ImmGCPtr(name), objReg);
        masm.passABIArg(objReg);
        masm.passABIArg(payloadReg);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, EqualStringsHelper));
        masm.mov(ReturnReg, scratchReg);

        if (!volatileRegs.has(objReg))
            masm.pop(objReg);

        LiveRegisterSet ignore;
        ignore.add(scratchReg);
        masm.PopRegsInMaskIgnore(volatileRegs, ignore);

        masm.branchIfFalseBool(scratchReg, failures);
        masm.bind(&equal);
    }
}

void
SetPropertyIC::emitIdGuard(MacroAssembler& masm, jsid id, Label* fail)
{
    if (this->id().constant())
        return;

    EmitIdGuard(masm, id, this->id().reg(), object(), temp(), fail);
}

static void
PushObjectOpResult(MacroAssembler& masm)
{
    static_assert(sizeof(ObjectOpResult) == sizeof(uintptr_t),
                  "ObjectOpResult size must match size reserved by masm.Push() here");
    masm.Push(ImmWord(ObjectOpResult::Uninitialized));
}

bool
jit::ValueToNameOrSymbolId(JSContext* cx, HandleValue idval, MutableHandleId id,
                           bool* nameOrSymbol)
{
    *nameOrSymbol = false;

    if (!idval.isString() && !idval.isSymbol())
        return true;

    if (!ValueToId<CanGC>(cx, idval, id))
        return false;

    if (!JSID_IS_STRING(id) && !JSID_IS_SYMBOL(id)) {
        id.set(JSID_VOID);
        return true;
    }

    uint32_t dummy;
    if (JSID_IS_STRING(id) && JSID_TO_ATOM(id)->isIndex(&dummy)) {
        id.set(JSID_VOID);
        return true;
    }

    *nameOrSymbol = true;
    return true;
}

void
IonCache::disable()
{
    reset(Reprotect);
    this->disabled_ = 1;
}

void
IonCache::reset(ReprotectCode reprotect)
{
    this->stubCount_ = 0;
    PatchJump(initialJump_, fallbackLabel_, reprotect);
    lastJump_ = initialJump_;
}

// Jump to failure if a value being written is not a property for obj/id.
static void
CheckTypeSetForWrite(MacroAssembler& masm, JSObject* obj, jsid id,
                     Register scratch, const ConstantOrRegister& value, Label* failure)
{
    TypedOrValueRegister valReg = value.reg();
    ObjectGroup* group = obj->group();
    MOZ_ASSERT(!group->unknownProperties());

    HeapTypeSet* propTypes = group->maybeGetProperty(id);
    MOZ_ASSERT(propTypes);

    // guardTypeSet can read from type sets without triggering read barriers.
    TypeSet::readBarrier(propTypes);

    masm.guardTypeSet(valReg, propTypes, BarrierKind::TypeSet, scratch, failure);
}

static void
GenerateSetSlot(JSContext* cx, MacroAssembler& masm, IonCache::StubAttacher& attacher,
                JSObject* obj, Shape* shape, Register object, Register tempReg,
                const ConstantOrRegister& value, bool needsTypeBarrier, bool checkTypeset,
                Label* failures)
{
    TestMatchingReceiver(masm, attacher, object, obj, failures, needsTypeBarrier);

    // Guard that the incoming value is in the type set for the property
    // if a type barrier is required.
    if (checkTypeset) {
        MOZ_ASSERT(needsTypeBarrier);
        CheckTypeSetForWrite(masm, obj, shape->propid(), tempReg, value, failures);
    }

    NativeObject::slotsSizeMustNotOverflow();

    if (obj->is<UnboxedPlainObject>()) {
        obj = obj->as<UnboxedPlainObject>().maybeExpando();
        masm.loadPtr(Address(object, UnboxedPlainObject::offsetOfExpando()), tempReg);
        object = tempReg;
    }

    if (obj->as<NativeObject>().isFixedSlot(shape->slot())) {
        Address addr(object, NativeObject::getFixedSlotOffset(shape->slot()));

        if (cx->zone()->needsIncrementalBarrier())
            masm.callPreBarrier(addr, MIRType::Value);

        masm.storeConstantOrRegister(value, addr);
    } else {
        masm.loadPtr(Address(object, NativeObject::offsetOfSlots()), tempReg);

        Address addr(tempReg, obj->as<NativeObject>().dynamicSlotIndex(shape->slot()) * sizeof(Value));

        if (cx->zone()->needsIncrementalBarrier())
            masm.callPreBarrier(addr, MIRType::Value);

        masm.storeConstantOrRegister(value, addr);
    }

    attacher.jumpRejoin(masm);

    masm.bind(failures);
    attacher.jumpNextStub(masm);
}

bool
SetPropertyIC::attachSetSlot(JSContext* cx, HandleScript outerScript, IonScript* ion,
                             HandleObject obj, HandleShape shape, bool checkTypeset)
{
    MacroAssembler masm(cx, ion, outerScript, profilerLeavePc_);
    StubAttacher attacher(*this);

    Label failures;
    emitIdGuard(masm, shape->propid(), &failures);

    GenerateSetSlot(cx, masm, attacher, obj, shape, object(), temp(), value(), needsTypeBarrier(),
                    checkTypeset, &failures);

    return linkAndAttachStub(cx, masm, attacher, ion, "setting",
                             JS::TrackedOutcome::ICSetPropStub_Slot);
}

bool
jit::IsCacheableSetPropCallNative(JSObject* obj, JSObject* holder, Shape* shape)
{
    if (!shape || !IsCacheableProtoChainForIonOrCacheIR(obj, holder))
        return false;

    if (!shape->hasSetterValue())
        return false;

    if (!shape->setterObject() || !shape->setterObject()->is<JSFunction>())
        return false;

    JSFunction& setter = shape->setterObject()->as<JSFunction>();
    if (!setter.isNative())
        return false;

    if (setter.jitInfo() && !setter.jitInfo()->needsOuterizedThisObject())
        return true;

    return !IsWindow(obj);
}

bool
jit::IsCacheableSetPropCallScripted(JSObject* obj, JSObject* holder, Shape* shape,
                                    bool* isTemporarilyUnoptimizable)
{
    if (!shape || !IsCacheableProtoChainForIonOrCacheIR(obj, holder))
        return false;

    if (IsWindow(obj))
        return false;

    if (!shape->hasSetterValue())
        return false;

    if (!shape->setterObject() || !shape->setterObject()->is<JSFunction>())
        return false;

    JSFunction& setter = shape->setterObject()->as<JSFunction>();
    if (setter.isNative())
        return false;

    if (!setter.hasJITCode()) {
        if (isTemporarilyUnoptimizable)
            *isTemporarilyUnoptimizable = true;
        return false;
    }

    return true;
}

static bool
IsCacheableSetPropCallPropertyOp(HandleObject obj, HandleObject holder, HandleShape shape)
{
    if (!shape)
        return false;

    if (!IsCacheableProtoChainForIonOrCacheIR(obj, holder))
        return false;

    if (shape->hasSlot())
        return false;

    if (shape->hasDefaultSetter())
        return false;

    if (shape->hasSetterValue())
        return false;

    // Despite the vehement claims of Shape.h that writable() is only relevant
    // for data descriptors, some SetterOps care desperately about its
    // value. The flag should be always true, apart from these rare instances.
    if (!shape->writable())
        return false;

    return true;
}

static bool
ReportStrictErrorOrWarning(JSContext* cx, JS::HandleObject obj, JS::HandleId id, bool strict,
                           JS::ObjectOpResult& result)
{
    return result.reportStrictErrorOrWarning(cx, obj, id, strict);
}

template <class FrameLayout>
void
EmitObjectOpResultCheck(MacroAssembler& masm, Label* failure, bool strict,
                        Register scratchReg,
                        Register argJSContextReg,
                        Register argObjReg,
                        Register argIdReg,
                        Register argStrictReg,
                        Register argResultReg)
{
    // if (!result) {
    Label noStrictError;
    masm.branch32(Assembler::Equal,
                  Address(masm.getStackPointer(),
                          FrameLayout::offsetOfObjectOpResult()),
                  Imm32(ObjectOpResult::OkCode),
                  &noStrictError);

    //     if (!ReportStrictErrorOrWarning(cx, obj, id, strict, &result))
    //         goto failure;
    masm.loadJSContext(argJSContextReg);
    masm.computeEffectiveAddress(
        Address(masm.getStackPointer(), FrameLayout::offsetOfObject()),
        argObjReg);
    masm.computeEffectiveAddress(
        Address(masm.getStackPointer(), FrameLayout::offsetOfId()),
        argIdReg);
    masm.move32(Imm32(strict), argStrictReg);
    masm.computeEffectiveAddress(
        Address(masm.getStackPointer(), FrameLayout::offsetOfObjectOpResult()),
        argResultReg);
    masm.setupUnalignedABICall(scratchReg);
    masm.passABIArg(argJSContextReg);
    masm.passABIArg(argObjReg);
    masm.passABIArg(argIdReg);
    masm.passABIArg(argStrictReg);
    masm.passABIArg(argResultReg);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, ReportStrictErrorOrWarning));
    masm.branchIfFalseBool(ReturnReg, failure);

    // }
    masm.bind(&noStrictError);
}

static bool
ProxySetProperty(JSContext* cx, HandleObject proxy, HandleId id, HandleValue v, bool strict)
{
    RootedValue receiver(cx, ObjectValue(*proxy));
    ObjectOpResult result;
    return Proxy::set(cx, proxy, id, v, receiver, result)
           && result.checkStrictErrorOrWarning(cx, proxy, id, strict);
}

static bool
EmitCallProxySet(JSContext* cx, MacroAssembler& masm, IonCache::StubAttacher& attacher,
                 HandleId propId, LiveRegisterSet liveRegs, Register object,
                 const ConstantOrRegister& value, void* returnAddr, bool strict)
{
    MacroAssembler::AfterICSaveLive aic = masm.icSaveLive(liveRegs);

    // Remaining registers should be free, but we still need to use |object| so
    // leave it alone.
    //
    // WARNING: We do not take() the register used by |value|, if any, so
    // regSet is going to re-allocate it. Hence the emitted code must not touch
    // any of the registers allocated from regSet until after the last use of
    // |value|. (We can't afford to take it, either, because x86.)
    AllocatableRegisterSet regSet(RegisterSet::All());
    regSet.take(AnyRegister(object));

    // ProxySetProperty(JSContext* cx, HandleObject proxy, HandleId id, HandleValue v,
    //                  bool strict);
    Register argJSContextReg = regSet.takeAnyGeneral();
    Register argProxyReg     = regSet.takeAnyGeneral();
    Register argIdReg        = regSet.takeAnyGeneral();
    Register argValueReg     = regSet.takeAnyGeneral();
    Register argStrictReg    = regSet.takeAnyGeneral();

    Register scratch         = regSet.takeAnyGeneral();

    // Push stubCode for marking.
    attacher.pushStubCodePointer(masm);

    // Push args on stack so we can take pointers to make handles.
    // Push value before touching any other registers (see WARNING above).
    masm.Push(value);
    masm.moveStackPtrTo(argValueReg);

    masm.move32(Imm32(strict), argStrictReg);

    masm.Push(propId, scratch);
    masm.moveStackPtrTo(argIdReg);

    // Push object.
    masm.Push(object);
    masm.moveStackPtrTo(argProxyReg);

    masm.loadJSContext(argJSContextReg);

    if (!masm.icBuildOOLFakeExitFrame(returnAddr, aic))
        return false;
    masm.enterFakeExitFrame(IonOOLProxyExitFrameLayoutToken);

    // Make the call.
    masm.setupUnalignedABICall(scratch);
    masm.passABIArg(argJSContextReg);
    masm.passABIArg(argProxyReg);
    masm.passABIArg(argIdReg);
    masm.passABIArg(argValueReg);
    masm.passABIArg(argStrictReg);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, ProxySetProperty));

    // Test for error.
    masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

    // masm.leaveExitFrame & pop locals
    masm.adjustStack(IonOOLProxyExitFrameLayout::Size());

    masm.icRestoreLive(liveRegs, aic);
    return true;
}

bool
SetPropertyIC::attachGenericProxy(JSContext* cx, HandleScript outerScript, IonScript* ion,
                                  HandleId id, void* returnAddr)
{
    MOZ_ASSERT(!hasGenericProxyStub());

    MacroAssembler masm(cx, ion, outerScript, profilerLeavePc_);
    StubAttacher attacher(*this);

    Label failures;
    emitIdGuard(masm, id, &failures);
    {
        masm.branchTestObjectIsProxy(false, object(), temp(), &failures);

        // Remove the DOM proxies. They'll take care of themselves so this stub doesn't
        // catch too much. The failure case is actually Equal. Fall through to the failure code.
        masm.branchTestProxyHandlerFamily(Assembler::Equal, object(), temp(),
                                          GetDOMProxyHandlerFamily(), &failures);
    }

    if (!EmitCallProxySet(cx, masm, attacher, id, liveRegs_, object(), value(),
                          returnAddr, strict()))
    {
        return false;
    }

    attacher.jumpRejoin(masm);

    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    MOZ_ASSERT(!hasGenericProxyStub_);
    hasGenericProxyStub_ = true;

    return linkAndAttachStub(cx, masm, attacher, ion, "generic proxy set",
                             JS::TrackedOutcome::ICSetPropStub_GenericProxy);
}

bool
SetPropertyIC::attachDOMProxyShadowed(JSContext* cx, HandleScript outerScript, IonScript* ion,
                                      HandleObject obj, HandleId id, void* returnAddr)
{
    MOZ_ASSERT(IsCacheableDOMProxy(obj));

    Label failures;
    MacroAssembler masm(cx, ion, outerScript, profilerLeavePc_);
    StubAttacher attacher(*this);

    emitIdGuard(masm, id, &failures);

    // Guard on the shape of the object.
    masm.branchPtr(Assembler::NotEqual,
                   Address(object(), ShapedObject::offsetOfShape()),
                   ImmGCPtr(obj->maybeShape()), &failures);

    // No need for more guards: we know this is a DOM proxy, since the shape
    // guard enforces a given JSClass, so just go ahead and emit the call to
    // ProxySet.

    if (!EmitCallProxySet(cx, masm, attacher, id, liveRegs_, object(),
                          value(), returnAddr, strict()))
    {
        return false;
    }

    // Success.
    attacher.jumpRejoin(masm);

    // Failure.
    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "DOM proxy shadowed set",
                             JS::TrackedOutcome::ICSetPropStub_DOMProxyShadowed);
}

static bool
GenerateCallSetter(JSContext* cx, IonScript* ion, MacroAssembler& masm,
                   IonCache::StubAttacher& attacher, HandleObject obj, HandleObject holder,
                   HandleShape shape, bool strict, Register object, Register tempReg,
                   const ConstantOrRegister& value, Label* failure, LiveRegisterSet liveRegs,
                   void* returnAddr)
{
    // Generate prototype guards if needed.
    {
        // Generate prototype/shape guards.
        if (obj != holder)
            GeneratePrototypeGuards(cx, ion, masm, obj, holder, object, tempReg, failure);

        masm.movePtr(ImmGCPtr(holder), tempReg);
        masm.branchPtr(Assembler::NotEqual,
                       Address(tempReg, ShapedObject::offsetOfShape()),
                       ImmGCPtr(holder->as<NativeObject>().lastProperty()),
                       failure);
    }

    // Good to go for invoking setter.

    MacroAssembler::AfterICSaveLive aic = masm.icSaveLive(liveRegs);

    // Remaining registers should basically be free, but we need to use |object| still
    // so leave it alone.  And of course we need our value, if it's not a constant.
    AllocatableRegisterSet regSet(RegisterSet::All());
    if (!value.constant())
        regSet.take(value.reg());
    bool valueAliasesObject = !regSet.has(object);
    if (!valueAliasesObject)
        regSet.take(object);

    regSet.take(tempReg);

    // This is a slower stub path, and we're going to be doing a call anyway.  Don't need
    // to try so hard to not use the stack.  Scratch regs are just taken from the register
    // set not including the input, current value saved on the stack, and restored when
    // we're done with it.
    //
    // Be very careful not to use any of these before value is pushed, since they
    // might shadow.

    if (IsCacheableSetPropCallNative(obj, holder, shape)) {
        Register argJSContextReg = regSet.takeAnyGeneral();
        Register argVpReg        = regSet.takeAnyGeneral();

        MOZ_ASSERT(shape->hasSetterValue() && shape->setterObject() &&
                   shape->setterObject()->is<JSFunction>());
        JSFunction* target = &shape->setterObject()->as<JSFunction>();

        MOZ_ASSERT(target->isNative());

        Register argUintNReg = regSet.takeAnyGeneral();

        // Set up the call:
        //  bool (*)(JSContext*, unsigned, Value* vp)
        // vp[0] is callee/outparam
        // vp[1] is |this|
        // vp[2] is the value

        // Build vp and move the base into argVpReg.
        masm.Push(value);
        masm.Push(TypedOrValueRegister(MIRType::Object, AnyRegister(object)));
        masm.Push(ObjectValue(*target));
        masm.moveStackPtrTo(argVpReg);

        // Preload other regs
        masm.loadJSContext(argJSContextReg);
        masm.move32(Imm32(1), argUintNReg);

        // Push data for GC marking
        masm.Push(argUintNReg);
        attacher.pushStubCodePointer(masm);

        if (!masm.icBuildOOLFakeExitFrame(returnAddr, aic))
            return false;
        masm.enterFakeExitFrame(IonOOLNativeExitFrameLayoutToken);

        // Make the call
        masm.setupUnalignedABICall(tempReg);
        masm.passABIArg(argJSContextReg);
        masm.passABIArg(argUintNReg);
        masm.passABIArg(argVpReg);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, target->native()));

        // Test for failure.
        masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

        // masm.leaveExitFrame & pop locals.
        masm.adjustStack(IonOOLNativeExitFrameLayout::Size(1));
    } else if (IsCacheableSetPropCallPropertyOp(obj, holder, shape)) {
        // We can't take all our registers up front, because on x86 we need 2
        // for the value, one for scratch, 5 for the arguments, which makes 8,
        // but we only have 7 to work with.  So only grab the ones we need
        // before we push value and release its reg back into the set.
        Register argResultReg = regSet.takeAnyGeneral();

        SetterOp target = shape->setterOp();
        MOZ_ASSERT(target);

        // JSSetterOp: bool fn(JSContext* cx, HandleObject obj,
        //                     HandleId id, HandleValue value, ObjectOpResult& result);

        // First, allocate an ObjectOpResult on the stack. We push this before
        // the stubCode pointer in order to match the layout of
        // IonOOLSetterOpExitFrameLayout.
        PushObjectOpResult(masm);
        masm.moveStackPtrTo(argResultReg);

        attacher.pushStubCodePointer(masm);

        // Push args on stack so we can take pointers to make handles.
        if (value.constant()) {
            masm.Push(value.value());
        } else {
            masm.Push(value.reg());
            if (!valueAliasesObject)
                regSet.add(value.reg());
        }

        // OK, now we can grab our remaining registers and grab the pointer to
        // what we just pushed into one of them.
        Register argJSContextReg = regSet.takeAnyGeneral();
        Register argValueReg     = regSet.takeAnyGeneral();
        // We can just reuse the "object" register for argObjReg
        Register argObjReg       = object;
        Register argIdReg        = regSet.takeAnyGeneral();
        masm.moveStackPtrTo(argValueReg);

        // push canonical jsid from shape instead of propertyname.
        masm.Push(shape->propid(), argIdReg);
        masm.moveStackPtrTo(argIdReg);

        masm.Push(object);
        masm.moveStackPtrTo(argObjReg);

        masm.loadJSContext(argJSContextReg);

        if (!masm.icBuildOOLFakeExitFrame(returnAddr, aic))
            return false;
        masm.enterFakeExitFrame(IonOOLSetterOpExitFrameLayoutToken);

        // Make the call.
        masm.setupUnalignedABICall(tempReg);
        masm.passABIArg(argJSContextReg);
        masm.passABIArg(argObjReg);
        masm.passABIArg(argIdReg);
        masm.passABIArg(argValueReg);
        masm.passABIArg(argResultReg);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, target));

        // Test for error.
        masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

        // Test for strict failure. We emit the check even in non-strict mode
        // in order to pick up the warning if extraWarnings is enabled.
        EmitObjectOpResultCheck<IonOOLSetterOpExitFrameLayout>(masm, masm.exceptionLabel(),
                                                               strict, tempReg,
                                                               argJSContextReg, argObjReg,
                                                               argIdReg, argValueReg,
                                                               argResultReg);

        // masm.leaveExitFrame & pop locals.
        masm.adjustStack(IonOOLSetterOpExitFrameLayout::Size());
    } else {
        MOZ_ASSERT(IsCacheableSetPropCallScripted(obj, holder, shape));

        JSFunction* target = &shape->setterValue().toObject().as<JSFunction>();
        uint32_t framePushedBefore = masm.framePushed();

        // Construct IonICCallFrameLayout.
        uint32_t descriptor = MakeFrameDescriptor(masm.framePushed(), JitFrame_IonJS,
                                                  IonICCallFrameLayout::Size());
        attacher.pushStubCodePointer(masm);
        masm.Push(Imm32(descriptor));
        masm.Push(ImmPtr(returnAddr));

        // The JitFrameLayout pushed below will be aligned to JitStackAlignment,
        // so we just have to make sure the stack is aligned after we push the
        // |this| + argument Values.
        uint32_t numArgs = Max(size_t(1), target->nargs());
        uint32_t argSize = (numArgs + 1) * sizeof(Value);
        uint32_t padding = ComputeByteAlignment(masm.framePushed() + argSize, JitStackAlignment);
        MOZ_ASSERT(padding % sizeof(uintptr_t) == 0);
        MOZ_ASSERT(padding < JitStackAlignment);
        masm.reserveStack(padding);

        for (size_t i = 1; i < target->nargs(); i++)
            masm.Push(UndefinedValue());
        masm.Push(value);
        masm.Push(TypedOrValueRegister(MIRType::Object, AnyRegister(object)));

        masm.movePtr(ImmGCPtr(target), tempReg);

        descriptor = MakeFrameDescriptor(argSize + padding, JitFrame_IonICCall,
                                         JitFrameLayout::Size());
        masm.Push(Imm32(1)); // argc
        masm.Push(tempReg);
        masm.Push(Imm32(descriptor));

        // Check stack alignment. Add sizeof(uintptr_t) for the return address.
        MOZ_ASSERT(((masm.framePushed() + sizeof(uintptr_t)) % JitStackAlignment) == 0);

        // The setter has JIT code now and we will only discard the setter's JIT
        // code when discarding all JIT code in the Zone, so we can assume it'll
        // still have JIT code.
        MOZ_ASSERT(target->hasJITCode());
        masm.loadPtr(Address(tempReg, JSFunction::offsetOfNativeOrScript()), tempReg);
        masm.loadBaselineOrIonRaw(tempReg, tempReg, nullptr);
        masm.callJit(tempReg);

        masm.freeStack(masm.framePushed() - framePushedBefore);
    }

    masm.icRestoreLive(liveRegs, aic);
    return true;
}

static bool
IsCacheableDOMProxyUnshadowedSetterCall(JSContext* cx, HandleObject obj, HandleId id,
                                        MutableHandleObject holder, MutableHandleShape shape)
{
    MOZ_ASSERT(IsCacheableDOMProxy(obj));

    RootedObject checkObj(cx, obj->staticPrototype());
    if (!checkObj)
        return false;

    PropertyResult prop;
    if (!LookupPropertyPure(cx, obj, id, holder.address(), &prop))
        return false;

    if (!holder || !holder->isNative())
        return false;

    shape.set(prop.shape());
    return IsCacheableSetPropCallNative(checkObj, holder, shape) ||
           IsCacheableSetPropCallPropertyOp(checkObj, holder, shape) ||
           IsCacheableSetPropCallScripted(checkObj, holder, shape);
}

bool
SetPropertyIC::attachDOMProxyUnshadowed(JSContext* cx, HandleScript outerScript, IonScript* ion,
                                        HandleObject obj, HandleId id, void* returnAddr)
{
    MOZ_ASSERT(IsCacheableDOMProxy(obj));

    Label failures;
    MacroAssembler masm(cx, ion, outerScript, profilerLeavePc_);
    StubAttacher attacher(*this);

    emitIdGuard(masm, id, &failures);

    // Guard on the shape of the object.
    masm.branchPtr(Assembler::NotEqual,
                   Address(object(), ShapedObject::offsetOfShape()),
                   ImmGCPtr(obj->maybeShape()), &failures);

    // Guard that our expando object hasn't started shadowing this property.
    CheckDOMProxyExpandoDoesNotShadow(cx, masm, obj, id, object(), &failures);

    RootedObject holder(cx);
    RootedShape shape(cx);
    if (IsCacheableDOMProxyUnshadowedSetterCall(cx, obj, id, &holder, &shape)) {
        if (!GenerateCallSetter(cx, ion, masm, attacher, obj, holder, shape, strict(),
                                object(), temp(), value(), &failures, liveRegs_, returnAddr))
        {
            return false;
        }
    } else {
        // Either there was no proto, or the property wasn't appropriately found on it.
        // Drop back to just a call to Proxy::set().
        if (!EmitCallProxySet(cx, masm, attacher, id, liveRegs_, object(),
                            value(), returnAddr, strict()))
        {
            return false;
        }
    }

    // Success.
    attacher.jumpRejoin(masm);

    // Failure.
    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "DOM proxy unshadowed set",
                             JS::TrackedOutcome::ICSetPropStub_DOMProxyUnshadowed);
}

bool
SetPropertyIC::attachCallSetter(JSContext* cx, HandleScript outerScript, IonScript* ion,
                                HandleObject obj, HandleObject holder, HandleShape shape,
                                void* returnAddr)
{
    MacroAssembler masm(cx, ion, outerScript, profilerLeavePc_);
    StubAttacher attacher(*this);

    Label failure;
    emitIdGuard(masm, shape->propid(), &failure);
    TestMatchingReceiver(masm, attacher, object(), obj, &failure);

    if (!GenerateCallSetter(cx, ion, masm, attacher, obj, holder, shape, strict(),
                            object(), temp(), value(), &failure, liveRegs_, returnAddr))
    {
        return false;
    }

    // Rejoin jump.
    attacher.jumpRejoin(masm);

    // Jump to next stub.
    masm.bind(&failure);
    attacher.jumpNextStub(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "setter call",
                             JS::TrackedOutcome::ICSetPropStub_CallSetter);
}

static void
GenerateAddSlot(JSContext* cx, MacroAssembler& masm, IonCache::StubAttacher& attacher,
                JSObject* obj, Shape* oldShape, ObjectGroup* oldGroup,
                Register object, Register tempReg, const ConstantOrRegister& value,
                bool checkTypeset, Label* failures)
{
    // Use a modified version of TestMatchingReceiver that uses the old shape and group.
    masm.branchTestObjGroup(Assembler::NotEqual, object, oldGroup, failures);
    if (obj->maybeShape()) {
        masm.branchTestObjShape(Assembler::NotEqual, object, oldShape, failures);
    } else {
        MOZ_ASSERT(obj->is<UnboxedPlainObject>());

        Address expandoAddress(object, UnboxedPlainObject::offsetOfExpando());
        masm.branchPtr(Assembler::Equal, expandoAddress, ImmWord(0), failures);

        masm.loadPtr(expandoAddress, tempReg);
        masm.branchTestObjShape(Assembler::NotEqual, tempReg, oldShape, failures);
    }

    Shape* newShape = obj->maybeShape();
    if (!newShape)
        newShape = obj->as<UnboxedPlainObject>().maybeExpando()->lastProperty();

    // Guard that the incoming value is in the type set for the property
    // if a type barrier is required.
    if (checkTypeset)
        CheckTypeSetForWrite(masm, obj, newShape->propid(), tempReg, value, failures);

    // Guard shapes along prototype chain.
    JSObject* proto = obj->staticPrototype();
    Register protoReg = tempReg;
    bool first = true;
    while (proto) {
        Shape* protoShape = proto->as<NativeObject>().lastProperty();

        // Load next prototype.
        masm.loadObjProto(first ? object : protoReg, protoReg);
        first = false;

        // Ensure that its shape matches.
        masm.branchTestObjShape(Assembler::NotEqual, protoReg, protoShape, failures);

        proto = proto->staticPrototype();
    }

    // Call a stub to (re)allocate dynamic slots, if necessary.
    uint32_t newNumDynamicSlots = obj->is<UnboxedPlainObject>()
                                  ? obj->as<UnboxedPlainObject>().maybeExpando()->numDynamicSlots()
                                  : obj->as<NativeObject>().numDynamicSlots();
    if (NativeObject::dynamicSlotsCount(oldShape) != newNumDynamicSlots) {
        AllocatableRegisterSet regs(RegisterSet::Volatile());
        LiveRegisterSet save(regs.asLiveSet());
        masm.PushRegsInMask(save);

        // Get 2 temp registers, without clobbering the object register.
        regs.takeUnchecked(object);
        Register temp1 = regs.takeAnyGeneral();
        Register temp2 = regs.takeAnyGeneral();

        if (obj->is<UnboxedPlainObject>()) {
            // Pass the expando object to the stub.
            masm.Push(object);
            masm.loadPtr(Address(object, UnboxedPlainObject::offsetOfExpando()), object);
        }

        masm.setupUnalignedABICall(temp1);
        masm.loadJSContext(temp1);
        masm.passABIArg(temp1);
        masm.passABIArg(object);
        masm.move32(Imm32(newNumDynamicSlots), temp2);
        masm.passABIArg(temp2);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, NativeObject::growSlotsDontReportOOM));

        // Branch on ReturnReg before restoring volatile registers, so
        // ReturnReg isn't clobbered.
        uint32_t framePushedAfterCall = masm.framePushed();
        Label allocFailed, allocDone;
        masm.branchIfFalseBool(ReturnReg, &allocFailed);
        masm.jump(&allocDone);

        masm.bind(&allocFailed);
        if (obj->is<UnboxedPlainObject>())
            masm.Pop(object);
        masm.PopRegsInMask(save);
        masm.jump(failures);

        masm.bind(&allocDone);
        masm.setFramePushed(framePushedAfterCall);
        if (obj->is<UnboxedPlainObject>())
            masm.Pop(object);
        masm.PopRegsInMask(save);
    }

    bool popObject = false;

    if (obj->is<UnboxedPlainObject>()) {
        masm.push(object);
        popObject = true;
        obj = obj->as<UnboxedPlainObject>().maybeExpando();
        masm.loadPtr(Address(object, UnboxedPlainObject::offsetOfExpando()), object);
    }

    // Write the object or expando object's new shape.
    Address shapeAddr(object, ShapedObject::offsetOfShape());
    if (cx->zone()->needsIncrementalBarrier())
        masm.callPreBarrier(shapeAddr, MIRType::Shape);
    masm.storePtr(ImmGCPtr(newShape), shapeAddr);

    if (oldGroup != obj->group()) {
        MOZ_ASSERT(!obj->is<UnboxedPlainObject>());

        // Changing object's group from a partially to fully initialized group,
        // per the acquired properties analysis. Only change the group if the
        // old group still has a newScript.
        Label noTypeChange, skipPop;

        masm.loadPtr(Address(object, JSObject::offsetOfGroup()), tempReg);
        masm.branchPtr(Assembler::Equal,
                       Address(tempReg, ObjectGroup::offsetOfAddendum()),
                       ImmWord(0),
                       &noTypeChange);

        Address groupAddr(object, JSObject::offsetOfGroup());
        if (cx->zone()->needsIncrementalBarrier())
            masm.callPreBarrier(groupAddr, MIRType::ObjectGroup);
        masm.storePtr(ImmGCPtr(obj->group()), groupAddr);

        masm.bind(&noTypeChange);
    }

    // Set the value on the object. Since this is an add, obj->lastProperty()
    // must be the shape of the property we are adding.
    NativeObject::slotsSizeMustNotOverflow();
    if (obj->as<NativeObject>().isFixedSlot(newShape->slot())) {
        Address addr(object, NativeObject::getFixedSlotOffset(newShape->slot()));
        masm.storeConstantOrRegister(value, addr);
    } else {
        masm.loadPtr(Address(object, NativeObject::offsetOfSlots()), tempReg);

        Address addr(tempReg, obj->as<NativeObject>().dynamicSlotIndex(newShape->slot()) * sizeof(Value));
        masm.storeConstantOrRegister(value, addr);
    }

    if (popObject)
        masm.pop(object);

    // Success.
    attacher.jumpRejoin(masm);

    // Failure.
    masm.bind(failures);

    attacher.jumpNextStub(masm);
}

bool
SetPropertyIC::attachAddSlot(JSContext* cx, HandleScript outerScript, IonScript* ion,
                             HandleObject obj, HandleId id, HandleShape oldShape,
                             HandleObjectGroup oldGroup, bool checkTypeset)
{
    MOZ_ASSERT_IF(!needsTypeBarrier(), !checkTypeset);

    MacroAssembler masm(cx, ion, outerScript, profilerLeavePc_);
    StubAttacher attacher(*this);

    Label failures;
    emitIdGuard(masm, id, &failures);

    GenerateAddSlot(cx, masm, attacher, obj, oldShape, oldGroup, object(), temp(), value(),
                    checkTypeset, &failures);
    return linkAndAttachStub(cx, masm, attacher, ion, "adding",
                             JS::TrackedOutcome::ICSetPropStub_AddSlot);
}

static bool
CanInlineSetPropTypeCheck(JSObject* obj, jsid id, const ConstantOrRegister& val,
                          bool* checkTypeset)
{
    bool shouldCheck = false;
    ObjectGroup* group = obj->group();
    if (!group->unknownProperties()) {
        HeapTypeSet* propTypes = group->maybeGetProperty(id);
        if (!propTypes)
            return false;
        if (!propTypes->unknown()) {
            if (obj->isSingleton() && !propTypes->nonConstantProperty())
                return false;
            shouldCheck = true;
            if (val.constant()) {
                // If the input is a constant, then don't bother if the barrier will always fail.
                if (!propTypes->hasType(TypeSet::GetValueType(val.value())))
                    return false;
                shouldCheck = false;
            } else {
                TypedOrValueRegister reg = val.reg();
                // We can do the same trick as above for primitive types of specialized registers.
                // TIs handling of objects is complicated enough to warrant a runtime
                // check, as we can't statically handle the case where the typeset
                // contains the specific object, but doesn't have ANYOBJECT set.
                if (reg.hasTyped() && reg.type() != MIRType::Object) {
                    JSValueType valType = ValueTypeFromMIRType(reg.type());
                    if (!propTypes->hasType(TypeSet::PrimitiveType(valType)))
                        return false;
                    shouldCheck = false;
                }
            }
        }
    }

    *checkTypeset = shouldCheck;
    return true;
}

static bool
IsPropertySetInlineable(NativeObject* obj, HandleId id, MutableHandleShape pshape,
                        const ConstantOrRegister& val, bool needsTypeBarrier, bool* checkTypeset)
{
    // CanInlineSetPropTypeCheck assumes obj has a non-lazy group.
    MOZ_ASSERT(!obj->hasLazyGroup());

    // Do a pure non-proto chain climbing lookup. See note in
    // CanAttachNativeGetProp.
    pshape.set(obj->lookupPure(id));

    if (!pshape)
        return false;

    if (!pshape->hasSlot())
        return false;

    if (!pshape->hasDefaultSetter())
        return false;

    if (!pshape->writable())
        return false;

    *checkTypeset = false;
    if (needsTypeBarrier && !CanInlineSetPropTypeCheck(obj, id, val, checkTypeset))
        return false;

    return true;
}

static bool
PrototypeChainShadowsPropertyAdd(JSContext* cx, JSObject* obj, jsid id)
{
    // Walk up the object prototype chain and ensure that all prototypes
    // are native, and that all prototypes have no getter or setter
    // defined on the property
    for (JSObject* proto = obj->staticPrototype(); proto; proto = proto->staticPrototype()) {
        // If prototype is non-native, don't optimize
        if (!proto->isNative())
            return true;

        // If prototype defines this property in a non-plain way, don't optimize
        Shape* protoShape = proto->as<NativeObject>().lookupPure(id);
        if (protoShape && !protoShape->hasDefaultSetter())
            return true;

        // Otherwise, if there's no such property, watch out for a resolve
        // hook that would need to be invoked and thus prevent inlining of
        // property addition.
        if (ClassMayResolveId(cx->names(), proto->getClass(), id, proto))
             return true;
    }

    return false;
}

static bool
IsPropertyAddInlineable(JSContext* cx, NativeObject* obj, HandleId id,
                        const ConstantOrRegister& val,
                        HandleShape oldShape, bool needsTypeBarrier, bool* checkTypeset)
{
    // If the shape of the object did not change, then this was not an add.
    if (obj->lastProperty() == oldShape)
        return false;

    Shape* shape = obj->lookupPure(id);
    if (!shape || shape->inDictionary() || !shape->hasSlot() || !shape->hasDefaultSetter())
        return false;

    // If we have a shape at this point and the object's shape changed, then
    // the shape must be the one we just added.
    MOZ_ASSERT(shape == obj->lastProperty());

    // Watch out for resolve hooks.
    if (ClassMayResolveId(cx->names(), obj->getClass(), id, obj))
        return false;

    // Likewise for an addProperty hook, since we'll need to invoke it.
    if (obj->getClass()->getAddProperty())
        return false;

    if (!obj->nonProxyIsExtensible() || !shape->writable())
        return false;

    if (PrototypeChainShadowsPropertyAdd(cx, obj, id))
        return false;

    // Don't attach if we are adding a property to an object which the new
    // script properties analysis hasn't been performed for yet, as there
    // may be a group change required here afterwards.
    if (obj->group()->newScript() && !obj->group()->newScript()->analyzed())
        return false;

    *checkTypeset = false;
    if (needsTypeBarrier && !CanInlineSetPropTypeCheck(obj, id, val, checkTypeset))
        return false;

    return true;
}

static SetPropertyIC::NativeSetPropCacheability
CanAttachNativeSetProp(JSContext* cx, HandleObject obj, HandleId id, const ConstantOrRegister& val,
                       bool needsTypeBarrier, MutableHandleObject holder,
                       MutableHandleShape shape, bool* checkTypeset)
{
    // See if the property exists on the object.
    if (obj->isNative() && IsPropertySetInlineable(&obj->as<NativeObject>(), id, shape, val,
                                                   needsTypeBarrier, checkTypeset))
    {
        return SetPropertyIC::CanAttachSetSlot;
    }

    // If we couldn't find the property on the object itself, do a full, but
    // still pure lookup for setters.
    Rooted<PropertyResult> prop(cx);
    if (!LookupPropertyPure(cx, obj, id, holder.address(), prop.address()))
        return SetPropertyIC::CanAttachNone;

    // If the object doesn't have the property, we don't know if we can attach
    // a stub to add the property until we do the VM call to add. If the
    // property exists as a data property on the prototype, we should add
    // a new, shadowing property.
    if (obj->isNative() &&
        (!prop || (obj != holder && holder->isNative() &&
                   prop.shape()->hasDefaultSetter() && prop.shape()->hasSlot())))
    {
        shape.set(prop.maybeShape());
        return SetPropertyIC::MaybeCanAttachAddSlot;
    }

    if (prop.isNonNativeProperty())
        return SetPropertyIC::CanAttachNone;

    shape.set(prop.maybeShape());
    if (IsCacheableSetPropCallPropertyOp(obj, holder, shape) ||
        IsCacheableSetPropCallNative(obj, holder, shape) ||
        IsCacheableSetPropCallScripted(obj, holder, shape))
    {
        return SetPropertyIC::CanAttachCallSetter;
    }

    return SetPropertyIC::CanAttachNone;
}

static void
GenerateSetUnboxed(JSContext* cx, MacroAssembler& masm, IonCache::StubAttacher& attacher,
                   JSObject* obj, jsid id, uint32_t unboxedOffset, JSValueType unboxedType,
                   Register object, Register tempReg, const ConstantOrRegister& value,
                   bool checkTypeset, Label* failures)
{
    // Guard on the type of the object.
    masm.branchPtr(Assembler::NotEqual,
                   Address(object, JSObject::offsetOfGroup()),
                   ImmGCPtr(obj->group()), failures);

    if (checkTypeset)
        CheckTypeSetForWrite(masm, obj, id, tempReg, value, failures);

    Address address(object, UnboxedPlainObject::offsetOfData() + unboxedOffset);

    if (cx->zone()->needsIncrementalBarrier()) {
        if (unboxedType == JSVAL_TYPE_OBJECT)
            masm.callPreBarrier(address, MIRType::Object);
        else if (unboxedType == JSVAL_TYPE_STRING)
            masm.callPreBarrier(address, MIRType::String);
        else
            MOZ_ASSERT(!UnboxedTypeNeedsPreBarrier(unboxedType));
    }

    masm.storeUnboxedProperty(address, unboxedType, value, failures);

    attacher.jumpRejoin(masm);

    masm.bind(failures);
    attacher.jumpNextStub(masm);
}

static bool
CanAttachSetUnboxed(JSContext* cx, HandleObject obj, HandleId id, const ConstantOrRegister& val,
                    bool needsTypeBarrier, bool* checkTypeset,
                    uint32_t* unboxedOffset, JSValueType* unboxedType)
{
    if (!obj->is<UnboxedPlainObject>())
        return false;

    const UnboxedLayout::Property* property = obj->as<UnboxedPlainObject>().layout().lookup(id);
    if (property) {
        *checkTypeset = false;
        if (needsTypeBarrier && !CanInlineSetPropTypeCheck(obj, id, val, checkTypeset))
            return false;
        *unboxedOffset = property->offset;
        *unboxedType = property->type;
        return true;
    }

    return false;
}

static bool
CanAttachSetUnboxedExpando(JSContext* cx, HandleObject obj, HandleId id,
                           const ConstantOrRegister& val,
                           bool needsTypeBarrier, bool* checkTypeset, Shape** pshape)
{
    if (!obj->is<UnboxedPlainObject>())
        return false;

    Rooted<UnboxedExpandoObject*> expando(cx, obj->as<UnboxedPlainObject>().maybeExpando());
    if (!expando)
        return false;

    Shape* shape = expando->lookupPure(id);
    if (!shape || !shape->hasDefaultSetter() || !shape->hasSlot() || !shape->writable())
        return false;

    *checkTypeset = false;
    if (needsTypeBarrier && !CanInlineSetPropTypeCheck(obj, id, val, checkTypeset))
        return false;

    *pshape = shape;
    return true;
}

static bool
CanAttachAddUnboxedExpando(JSContext* cx, HandleObject obj, HandleShape oldShape,
                           HandleId id, const ConstantOrRegister& val,
                           bool needsTypeBarrier, bool* checkTypeset)
{
    if (!obj->is<UnboxedPlainObject>())
        return false;

    Rooted<UnboxedExpandoObject*> expando(cx, obj->as<UnboxedPlainObject>().maybeExpando());
    if (!expando || expando->inDictionaryMode())
        return false;

    Shape* newShape = expando->lastProperty();
    if (newShape->isEmptyShape() || newShape->propid() != id || newShape->previous() != oldShape)
        return false;

    MOZ_ASSERT(newShape->hasDefaultSetter() && newShape->hasSlot() && newShape->writable());

    if (PrototypeChainShadowsPropertyAdd(cx, obj, id))
        return false;

    *checkTypeset = false;
    if (needsTypeBarrier && !CanInlineSetPropTypeCheck(obj, id, val, checkTypeset))
        return false;

    return true;
}

bool
SetPropertyIC::tryAttachUnboxed(JSContext* cx, HandleScript outerScript, IonScript* ion,
                                HandleObject obj, HandleId id, bool* emitted)
{
    MOZ_ASSERT(!*emitted);

    bool checkTypeset = false;
    uint32_t unboxedOffset;
    JSValueType unboxedType;
    if (!CanAttachSetUnboxed(cx, obj, id, value(), needsTypeBarrier(), &checkTypeset,
                             &unboxedOffset, &unboxedType))
    {
        return true;
    }

    *emitted = true;

    MacroAssembler masm(cx, ion, outerScript, profilerLeavePc_);
    StubAttacher attacher(*this);

    Label failures;
    emitIdGuard(masm, id, &failures);

    GenerateSetUnboxed(cx, masm, attacher, obj, id, unboxedOffset, unboxedType,
                       object(), temp(), value(), checkTypeset, &failures);
    return linkAndAttachStub(cx, masm, attacher, ion, "set_unboxed",
                             JS::TrackedOutcome::ICSetPropStub_SetUnboxed);
}

bool
SetPropertyIC::tryAttachProxy(JSContext* cx, HandleScript outerScript, IonScript* ion,
                              HandleObject obj, HandleId id, bool* emitted)
{
    MOZ_ASSERT(!*emitted);

    if (!obj->is<ProxyObject>())
        return true;

    void* returnAddr = GetReturnAddressToIonCode(cx);
    if (IsCacheableDOMProxy(obj)) {
        DOMProxyShadowsResult shadows = GetDOMProxyShadowsCheck()(cx, obj, id);
        if (shadows == ShadowCheckFailed)
            return false;

        if (DOMProxyIsShadowing(shadows)) {
            if (!attachDOMProxyShadowed(cx, outerScript, ion, obj, id, returnAddr))
                return false;
            *emitted = true;
            return true;
        }

        MOZ_ASSERT(shadows == DoesntShadow || shadows == DoesntShadowUnique);
        if (shadows == DoesntShadowUnique)
            reset(Reprotect);
        if (!attachDOMProxyUnshadowed(cx, outerScript, ion, obj, id, returnAddr))
            return false;
        *emitted = true;
        return true;
    }

    if (hasGenericProxyStub())
        return true;

    if (!attachGenericProxy(cx, outerScript, ion, id, returnAddr))
        return false;
    *emitted = true;
    return true;
}

bool
SetPropertyIC::tryAttachNative(JSContext* cx, HandleScript outerScript, IonScript* ion,
                               HandleObject obj, HandleId id, bool* emitted, bool* tryNativeAddSlot)
{
    MOZ_ASSERT(!*emitted);
    MOZ_ASSERT(!*tryNativeAddSlot);

    RootedShape shape(cx);
    RootedObject holder(cx);
    bool checkTypeset = false;
    NativeSetPropCacheability canCache = CanAttachNativeSetProp(cx, obj, id, value(), needsTypeBarrier(),
                                                                &holder, &shape, &checkTypeset);
    switch (canCache) {
      case CanAttachNone:
        return true;

      case CanAttachSetSlot: {
        RootedNativeObject nobj(cx, &obj->as<NativeObject>());
        if (!attachSetSlot(cx, outerScript, ion, nobj, shape, checkTypeset))
            return false;
        *emitted = true;
        return true;
      }

      case CanAttachCallSetter: {
        void* returnAddr = GetReturnAddressToIonCode(cx);
        if (!attachCallSetter(cx, outerScript, ion, obj, holder, shape, returnAddr))
            return false;
        *emitted = true;
        return true;
      }

      case MaybeCanAttachAddSlot:
        *tryNativeAddSlot = true;
        return true;
    }

    MOZ_CRASH("Unreachable");
}

bool
SetPropertyIC::tryAttachUnboxedExpando(JSContext* cx, HandleScript outerScript, IonScript* ion,
                                       HandleObject obj, HandleId id, bool* emitted)
{
    MOZ_ASSERT(!*emitted);

    RootedShape shape(cx);
    bool checkTypeset = false;
    if (!CanAttachSetUnboxedExpando(cx, obj, id, value(), needsTypeBarrier(),
                                    &checkTypeset, shape.address()))
    {
        return true;
    }

    if (!attachSetSlot(cx, outerScript, ion, obj, shape, checkTypeset))
        return false;
    *emitted = true;
    return true;
}

bool
SetPropertyIC::tryAttachStub(JSContext* cx, HandleScript outerScript, IonScript* ion,
                             HandleObject obj, HandleValue idval, HandleValue value,
                             MutableHandleId id, bool* emitted, bool* tryNativeAddSlot)
{
    MOZ_ASSERT(!*emitted);
    MOZ_ASSERT(!*tryNativeAddSlot);

    if (!canAttachStub() || obj->watched())
        return true;

    // Fail cache emission if the object is frozen
    if (obj->is<NativeObject>() && obj->as<NativeObject>().getElementsHeader()->isFrozen())
        return true;

    bool nameOrSymbol;
    if (!ValueToNameOrSymbolId(cx, idval, id, &nameOrSymbol))
        return false;

    if (nameOrSymbol) {
        if (!*emitted && !tryAttachProxy(cx, outerScript, ion, obj, id, emitted))
            return false;

        if (!*emitted && !tryAttachNative(cx, outerScript, ion, obj, id, emitted, tryNativeAddSlot))
            return false;

        if (!*emitted && !tryAttachUnboxed(cx, outerScript, ion, obj, id, emitted))
            return false;

        if (!*emitted && !tryAttachUnboxedExpando(cx, outerScript, ion, obj, id, emitted))
            return false;
    }

    if (idval.isInt32()) {
        if (!*emitted && !tryAttachDenseElement(cx, outerScript, ion, obj, idval, emitted))
            return false;
        if (!*emitted &&
            !tryAttachTypedArrayElement(cx, outerScript, ion, obj, idval, value, emitted))
        {
            return false;
        }
    }

    return true;
}

bool
SetPropertyIC::tryAttachAddSlot(JSContext* cx, HandleScript outerScript, IonScript* ion,
                                HandleObject obj, HandleId id, HandleObjectGroup oldGroup,
                                HandleShape oldShape, bool tryNativeAddSlot, bool* emitted)
{
    MOZ_ASSERT(!*emitted);

    if (!canAttachStub())
        return true;

    if (!JSID_IS_STRING(id) && !JSID_IS_SYMBOL(id))
        return true;

    // Fail cache emission if the object is frozen
    if (obj->is<NativeObject>() && obj->as<NativeObject>().getElementsHeader()->isFrozen())
        return true;

    // A GC may have caused cache.value() to become stale as it is not traced.
    // In this case the IonScript will have been invalidated, so check for that.
    // Assert no further GC is possible past this point.
    JS::AutoAssertNoGC nogc;
    if (ion->invalidated())
        return true;

    // The property did not exist before, now we can try to inline the property add.
    bool checkTypeset = false;
    if (tryNativeAddSlot &&
        IsPropertyAddInlineable(cx, &obj->as<NativeObject>(), id, value(), oldShape,
                                needsTypeBarrier(), &checkTypeset))
    {
        if (!attachAddSlot(cx, outerScript, ion, obj, id, oldShape, oldGroup, checkTypeset))
            return false;
        *emitted = true;
        return true;
    }

    checkTypeset = false;
    if (CanAttachAddUnboxedExpando(cx, obj, oldShape, id, value(), needsTypeBarrier(),
                                   &checkTypeset))
    {
        if (!attachAddSlot(cx, outerScript, ion, obj, id, oldShape, oldGroup, checkTypeset))
            return false;
        *emitted = true;
        return true;
    }

    return true;
}

bool
SetPropertyIC::update(JSContext* cx, HandleScript outerScript, size_t cacheIndex, HandleObject obj,
                      HandleValue idval, HandleValue value)
{
    IonScript* ion = outerScript->ionScript();
    SetPropertyIC& cache = ion->getCache(cacheIndex).toSetProperty();

    // Remember the old group and shape if we may attach an add-property stub.
    // Also, some code under tryAttachStub depends on obj having a non-lazy
    // group, see for instance CanInlineSetPropTypeCheck.
    RootedObjectGroup oldGroup(cx);
    RootedShape oldShape(cx);
    if (cache.canAttachStub()) {
        oldGroup = JSObject::getGroup(cx, obj);
        if (!oldGroup)
            return false;

        oldShape = obj->maybeShape();
        if (obj->is<UnboxedPlainObject>()) {
            MOZ_ASSERT(!oldShape);
            if (UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando())
                oldShape = expando->lastProperty();
        }
    }

    RootedId id(cx);
    bool emitted = false;
    bool tryNativeAddSlot = false;
    if (!cache.tryAttachStub(cx, outerScript, ion, obj, idval, value, &id, &emitted,
                             &tryNativeAddSlot))
    {
        return false;
    }

    // Set/Add the property on the object, the inlined cache are setup for the next execution.
    if (JSOp(*cache.pc()) == JSOP_INITGLEXICAL) {
        RootedScript script(cx);
        jsbytecode* pc;
        cache.getScriptedLocation(&script, &pc);
        MOZ_ASSERT(!script->hasNonSyntacticScope());
        InitGlobalLexicalOperation(cx, &cx->global()->lexicalEnvironment(), script, pc, value);
    } else if (*cache.pc() == JSOP_SETELEM || *cache.pc() == JSOP_STRICTSETELEM) {
        if (!SetObjectElement(cx, obj, idval, value, cache.strict()))
            return false;
    } else {
        RootedPropertyName name(cx, idval.toString()->asAtom().asPropertyName());
        if (!SetProperty(cx, obj, name, value, cache.strict(), cache.pc()))
            return false;
    }

    if (!emitted &&
        !cache.tryAttachAddSlot(cx, outerScript, ion, obj, id, oldGroup, oldShape,
                                tryNativeAddSlot, &emitted))
    {
        return false;
    }

    if (!emitted)
        JitSpew(JitSpew_IonIC, "Failed to attach SETPROP cache");

    return true;
}

void
SetPropertyIC::reset(ReprotectCode reprotect)
{
    IonCache::reset(reprotect);
    hasGenericProxyStub_ = false;
    hasDenseStub_ = false;
}

static bool
IsDenseElementSetInlineable(JSObject* obj, const Value& idval, const ConstantOrRegister& val,
                            bool needsTypeBarrier, bool* checkTypeset)
{
    if (!obj->is<ArrayObject>())
        return false;

    if (obj->watched())
        return false;

    if (!idval.isInt32())
        return false;

    // The object may have a setter definition,
    // either directly, or via a prototype, or via the target object for a prototype
    // which is a proxy, that handles a particular integer write.
    // Scan the prototype and shape chain to make sure that this is not the case.
    JSObject* curObj = obj;
    while (curObj) {
        // Ensure object is native.  (This guarantees static prototype below.)
        if (!curObj->isNative())
            return false;

        // Ensure all indexed properties are stored in dense elements.
        if (curObj->isIndexed())
            return false;

        curObj = curObj->staticPrototype();
    }

    *checkTypeset = false;
    if (needsTypeBarrier && !CanInlineSetPropTypeCheck(obj, JSID_VOID, val, checkTypeset))
        return false;

    return true;
}

static bool
IsTypedArrayElementSetInlineable(JSObject* obj, const Value& idval, const Value& value)
{
    // Don't bother attaching stubs for assigning strings, objects or symbols.
    return obj->is<TypedArrayObject>() && idval.isInt32() &&
           !value.isString() && !value.isObject() && !value.isSymbol();
}

static void
StoreDenseElement(MacroAssembler& masm, const ConstantOrRegister& value, Register elements,
                  BaseObjectElementIndex target)
{
    // If the ObjectElements::CONVERT_DOUBLE_ELEMENTS flag is set, int32 values
    // have to be converted to double first. If the value is not int32, it can
    // always be stored directly.

    Address elementsFlags(elements, ObjectElements::offsetOfFlags());
    if (value.constant()) {
        Value v = value.value();
        Label done;
        if (v.isInt32()) {
            Label dontConvert;
            masm.branchTest32(Assembler::Zero, elementsFlags,
                              Imm32(ObjectElements::CONVERT_DOUBLE_ELEMENTS),
                              &dontConvert);
            masm.storeValue(DoubleValue(v.toInt32()), target);
            masm.jump(&done);
            masm.bind(&dontConvert);
        }
        masm.storeValue(v, target);
        masm.bind(&done);
        return;
    }

    TypedOrValueRegister reg = value.reg();
    if (reg.hasTyped() && reg.type() != MIRType::Int32) {
        masm.storeTypedOrValue(reg, target);
        return;
    }

    Label convert, storeValue, done;
    masm.branchTest32(Assembler::NonZero, elementsFlags,
                      Imm32(ObjectElements::CONVERT_DOUBLE_ELEMENTS),
                      &convert);
    masm.bind(&storeValue);
    masm.storeTypedOrValue(reg, target);
    masm.jump(&done);

    masm.bind(&convert);
    if (reg.hasValue()) {
        masm.branchTestInt32(Assembler::NotEqual, reg.valueReg(), &storeValue);
        masm.int32ValueToDouble(reg.valueReg(), ScratchDoubleReg);
        masm.storeDouble(ScratchDoubleReg, target);
    } else {
        MOZ_ASSERT(reg.type() == MIRType::Int32);
        masm.convertInt32ToDouble(reg.typedReg().gpr(), ScratchDoubleReg);
        masm.storeDouble(ScratchDoubleReg, target);
    }

    masm.bind(&done);
}

static bool
GenerateSetDenseElement(JSContext* cx, MacroAssembler& masm, IonCache::StubAttacher& attacher,
                        JSObject* obj, const Value& idval, bool guardHoles, Register object,
                        TypedOrValueRegister index, const ConstantOrRegister& value,
                        Register tempToUnboxIndex, Register temp,
                        bool needsTypeBarrier, bool checkTypeset)
{
    MOZ_ASSERT(obj->isNative());
    MOZ_ASSERT(idval.isInt32());

    Label failures;

    // Guard object is a dense array.
    Shape* shape = obj->as<NativeObject>().lastProperty();
    if (!shape)
        return false;
    masm.branchTestObjShape(Assembler::NotEqual, object, shape, &failures);

    // Guard that the incoming value is in the type set for the property
    // if a type barrier is required.
    if (needsTypeBarrier) {
        masm.branchTestObjGroup(Assembler::NotEqual, object, obj->group(), &failures);
        if (checkTypeset)
            CheckTypeSetForWrite(masm, obj, JSID_VOID, temp, value, &failures);
    }

    // Ensure the index is an int32 value.
    Register indexReg;
    if (index.hasValue()) {
        ValueOperand val = index.valueReg();
        masm.branchTestInt32(Assembler::NotEqual, val, &failures);

        indexReg = masm.extractInt32(val, tempToUnboxIndex);
    } else {
        MOZ_ASSERT(!index.typedReg().isFloat());
        indexReg = index.typedReg().gpr();
    }

    {
        // Load obj->elements.
        Register elements = temp;
        masm.loadPtr(Address(object, NativeObject::offsetOfElements()), elements);

        // Compute the location of the element.
        BaseObjectElementIndex target(elements, indexReg);

        Label storeElement;

        // If TI cannot help us deal with HOLES by preventing indexed properties
        // on the prototype chain, we have to be very careful to check for ourselves
        // to avoid stomping on what should be a setter call. Start by only allowing things
        // within the initialized length.
        if (guardHoles) {
            Address initLength(elements, ObjectElements::offsetOfInitializedLength());
            masm.branch32(Assembler::BelowOrEqual, initLength, indexReg, &failures);
        } else {
            // Guard that we can increase the initialized length.
            Address capacity(elements, ObjectElements::offsetOfCapacity());
            masm.branch32(Assembler::BelowOrEqual, capacity, indexReg, &failures);

            // Guard on the initialized length.
            Address initLength(elements, ObjectElements::offsetOfInitializedLength());
            masm.branch32(Assembler::Below, initLength, indexReg, &failures);

            // if (initLength == index)
            Label inBounds;
            masm.branch32(Assembler::NotEqual, initLength, indexReg, &inBounds);
            {
                // Increase initialize length.
                Register newLength = indexReg;
                masm.add32(Imm32(1), newLength);
                masm.store32(newLength, initLength);

                // Increase length if needed.
                Label bumpedLength;
                Address length(elements, ObjectElements::offsetOfLength());
                masm.branch32(Assembler::AboveOrEqual, length, indexReg, &bumpedLength);
                masm.store32(newLength, length);
                masm.bind(&bumpedLength);

                // Restore the index.
                masm.add32(Imm32(-1), newLength);
                masm.jump(&storeElement);
            }
            // else
            masm.bind(&inBounds);
        }

        if (cx->zone()->needsIncrementalBarrier())
            masm.callPreBarrier(target, MIRType::Value);

        // Store the value.
        if (guardHoles)
            masm.branchTestMagic(Assembler::Equal, target, &failures);
        else
            masm.bind(&storeElement);
        StoreDenseElement(masm, value, elements, target);
    }
    attacher.jumpRejoin(masm);

    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    return true;
}

bool
SetPropertyIC::tryAttachDenseElement(JSContext* cx, HandleScript outerScript, IonScript* ion,
                                     HandleObject obj, const Value& idval, bool* emitted)
{
    MOZ_ASSERT(!*emitted);
    MOZ_ASSERT(canAttachStub());

    if (hasDenseStub())
        return true;

    bool checkTypeset = false;
    if (!IsDenseElementSetInlineable(obj, idval, value(), needsTypeBarrier(), &checkTypeset))
        return true;

    *emitted = true;

    MacroAssembler masm(cx, ion, outerScript, profilerLeavePc_);
    StubAttacher attacher(*this);
    if (!GenerateSetDenseElement(cx, masm, attacher, obj, idval,
                                 guardHoles(), object(), id().reg(),
                                 value(), tempToUnboxIndex(), temp(),
                                 needsTypeBarrier(), checkTypeset))
    {
        return false;
    }

    setHasDenseStub();
    const char* message = guardHoles() ?  "dense array (holes)" : "dense array";
    return linkAndAttachStub(cx, masm, attacher, ion, message,
                             JS::TrackedOutcome::ICSetElemStub_Dense);
}

static bool
GenerateSetTypedArrayElement(JSContext* cx, MacroAssembler& masm, IonCache::StubAttacher& attacher,
                             HandleObject tarr, Register object, TypedOrValueRegister index,
                             const ConstantOrRegister& value, Register tempUnbox, Register temp,
                             FloatRegister tempDouble, FloatRegister tempFloat32)
{
    Label failures, done, popObjectAndFail;

    // Guard on the shape.
    Shape* shape = tarr->as<TypedArrayObject>().lastProperty();
    if (!shape)
        return false;
    masm.branchTestObjShape(Assembler::NotEqual, object, shape, &failures);

    // Ensure the index is an int32.
    Register indexReg;
    if (index.hasValue()) {
        ValueOperand val = index.valueReg();
        masm.branchTestInt32(Assembler::NotEqual, val, &failures);

        indexReg = masm.extractInt32(val, tempUnbox);
    } else {
        MOZ_ASSERT(!index.typedReg().isFloat());
        indexReg = index.typedReg().gpr();
    }

    // Guard on the length.
    Address length(object, TypedArrayObject::lengthOffset());
    masm.unboxInt32(length, temp);
    masm.branch32(Assembler::BelowOrEqual, temp, indexReg, &done);

    // Load the elements vector.
    Register elements = temp;
    masm.loadPtr(Address(object, TypedArrayObject::dataOffset()), elements);

    // Set the value.
    Scalar::Type arrayType = tarr->as<TypedArrayObject>().type();
    int width = Scalar::byteSize(arrayType);
    BaseIndex target(elements, indexReg, ScaleFromElemWidth(width));

    if (arrayType == Scalar::Float32) {
        MOZ_ASSERT_IF(hasUnaliasedDouble(), tempFloat32 != InvalidFloatReg);
        FloatRegister tempFloat = hasUnaliasedDouble() ? tempFloat32 : tempDouble;
        if (!masm.convertConstantOrRegisterToFloat(cx, value, tempFloat, &failures))
            return false;
        masm.storeToTypedFloatArray(arrayType, tempFloat, target);
    } else if (arrayType == Scalar::Float64) {
        if (!masm.convertConstantOrRegisterToDouble(cx, value, tempDouble, &failures))
            return false;
        masm.storeToTypedFloatArray(arrayType, tempDouble, target);
    } else {
        // On x86 we only have 6 registers available to use, so reuse the object
        // register to compute the intermediate value to store and restore it
        // afterwards.
        masm.push(object);

        if (arrayType == Scalar::Uint8Clamped) {
            if (!masm.clampConstantOrRegisterToUint8(cx, value, tempDouble, object,
                                                     &popObjectAndFail))
            {
                return false;
            }
        } else {
            if (!masm.truncateConstantOrRegisterToInt32(cx, value, tempDouble, object,
                                                        &popObjectAndFail))
            {
                return false;
            }
        }
        masm.storeToTypedIntArray(arrayType, object, target);

        masm.pop(object);
    }

    // Out-of-bound writes jump here as they are no-ops.
    masm.bind(&done);
    attacher.jumpRejoin(masm);

    if (popObjectAndFail.used()) {
        masm.bind(&popObjectAndFail);
        masm.pop(object);
    }

    masm.bind(&failures);
    attacher.jumpNextStub(masm);
    return true;
}

bool
SetPropertyIC::tryAttachTypedArrayElement(JSContext* cx, HandleScript outerScript, IonScript* ion,
                                          HandleObject obj, HandleValue idval, HandleValue val,
                                          bool* emitted)
{
    MOZ_ASSERT(!*emitted);
    MOZ_ASSERT(canAttachStub());

    if (!IsTypedArrayElementSetInlineable(obj, idval, val))
        return true;

    *emitted = true;

    MacroAssembler masm(cx, ion, outerScript, profilerLeavePc_);
    StubAttacher attacher(*this);
    if (!GenerateSetTypedArrayElement(cx, masm, attacher, obj,
                                      object(), id().reg(), value(),
                                      tempToUnboxIndex(), temp(), tempDouble(), tempFloat32()))
    {
        return false;
    }

    return linkAndAttachStub(cx, masm, attacher, ion, "typed array",
                             JS::TrackedOutcome::ICSetElemStub_TypedArray);
}

bool
BindNameIC::attachGlobal(JSContext* cx, HandleScript outerScript, IonScript* ion,
                         HandleObject envChain)
{
    MOZ_ASSERT(envChain->is<GlobalObject>());

    MacroAssembler masm(cx, ion, outerScript, profilerLeavePc_);
    StubAttacher attacher(*this);

    // Guard on the env chain.
    attacher.branchNextStub(masm, Assembler::NotEqual, environmentChainReg(),
                            ImmGCPtr(envChain));
    masm.movePtr(ImmGCPtr(envChain), outputReg());

    attacher.jumpRejoin(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "global");
}

static inline void
GenerateEnvironmentChainGuard(MacroAssembler& masm, JSObject* envObj,
                              Register envObjReg, Shape* shape, Label* failures)
{
    if (envObj->is<CallObject>()) {
        // We can skip a guard on the call object if the script's bindings are
        // guaranteed to be immutable (and thus cannot introduce shadowing
        // variables).
        CallObject* callObj = &envObj->as<CallObject>();
        JSFunction* fun = &callObj->callee();
        // The function might have been relazified under rare conditions.
        // In that case, we pessimistically create the guard, as we'd
        // need to root various pointers to delazify,
        if (fun->hasScript()) {
            JSScript* script = fun->nonLazyScript();
            if (!script->funHasExtensibleScope())
                return;
        }
    } else if (envObj->is<GlobalObject>()) {
        // If this is the last object on the scope walk, and the property we've
        // found is not configurable, then we don't need a shape guard because
        // the shape cannot be removed.
        if (shape && !shape->configurable())
            return;
    }

    Address shapeAddr(envObjReg, ShapedObject::offsetOfShape());
    masm.branchPtr(Assembler::NotEqual, shapeAddr,
                   ImmGCPtr(envObj->as<NativeObject>().lastProperty()), failures);
}

static void
GenerateEnvironmentChainGuards(MacroAssembler& masm, JSObject* envChain, JSObject* holder,
                               Register outputReg, Label* failures, bool skipLastGuard = false)
{
    JSObject* tobj = envChain;

    // Walk up the env chain. Note that IsCacheableEnvironmentChain guarantees the
    // |tobj == holder| condition terminates the loop.
    while (true) {
        MOZ_ASSERT(IsCacheableEnvironment(tobj) || tobj->is<GlobalObject>());

        if (skipLastGuard && tobj == holder)
            break;

        GenerateEnvironmentChainGuard(masm, tobj, outputReg, nullptr, failures);

        if (tobj == holder)
            break;

        // Load the next link.
        tobj = &tobj->as<EnvironmentObject>().enclosingEnvironment();
        masm.extractObject(Address(outputReg, EnvironmentObject::offsetOfEnclosingEnvironment()),
                           outputReg);
    }
}

bool
BindNameIC::attachNonGlobal(JSContext* cx, HandleScript outerScript, IonScript* ion,
                            HandleObject envChain, HandleObject holder)
{
    MOZ_ASSERT(IsCacheableEnvironment(envChain));

    MacroAssembler masm(cx, ion, outerScript, profilerLeavePc_);
    StubAttacher attacher(*this);

    // Guard on the shape of the env chain.
    Label failures;
    attacher.branchNextStubOrLabel(masm, Assembler::NotEqual,
                                   Address(environmentChainReg(), ShapedObject::offsetOfShape()),
                                   ImmGCPtr(envChain->as<NativeObject>().lastProperty()),
                                   holder != envChain ? &failures : nullptr);

    if (holder != envChain) {
        JSObject* parent = &envChain->as<EnvironmentObject>().enclosingEnvironment();
        masm.extractObject(Address(environmentChainReg(),
                                   EnvironmentObject::offsetOfEnclosingEnvironment()),
                           outputReg());

        GenerateEnvironmentChainGuards(masm, parent, holder, outputReg(), &failures);
    } else {
        masm.movePtr(environmentChainReg(), outputReg());
    }

    // At this point outputReg holds the object on which the property
    // was found, so we're done.
    attacher.jumpRejoin(masm);

    // All failures flow to here, so there is a common point to patch.
    if (holder != envChain) {
        masm.bind(&failures);
        attacher.jumpNextStub(masm);
    }

    return linkAndAttachStub(cx, masm, attacher, ion, "non-global");
}

static bool
IsCacheableNonGlobalEnvironmentChain(JSObject* envChain, JSObject* holder)
{
    while (true) {
        if (!IsCacheableEnvironment(envChain)) {
            JitSpew(JitSpew_IonIC, "Non-cacheable object on env chain");
            return false;
        }

        if (envChain == holder)
            return true;

        envChain = &envChain->as<EnvironmentObject>().enclosingEnvironment();
        if (!envChain) {
            JitSpew(JitSpew_IonIC, "env chain indirect hit");
            return false;
        }
    }

    MOZ_CRASH("Invalid env chain");
}

JSObject*
BindNameIC::update(JSContext* cx, HandleScript outerScript, size_t cacheIndex,
                   HandleObject envChain)
{
    IonScript* ion = outerScript->ionScript();
    BindNameIC& cache = ion->getCache(cacheIndex).toBindName();
    HandlePropertyName name = cache.name();

    RootedObject holder(cx);
    if (!LookupNameUnqualified(cx, name, envChain, &holder))
        return nullptr;

    // Stop generating new stubs once we hit the stub count limit, see
    // GetPropertyCache.
    if (cache.canAttachStub()) {
        if (envChain->is<GlobalObject>()) {
            if (!cache.attachGlobal(cx, outerScript, ion, envChain))
                return nullptr;
        } else if (IsCacheableNonGlobalEnvironmentChain(envChain, holder)) {
            if (!cache.attachNonGlobal(cx, outerScript, ion, envChain, holder))
                return nullptr;
        } else {
            JitSpew(JitSpew_IonIC, "BINDNAME uncacheable env chain");
        }
    }

    return holder;
}

bool
NameIC::attachReadSlot(JSContext* cx, HandleScript outerScript, IonScript* ion,
                       HandleObject envChain, HandleObject holderBase,
                       HandleNativeObject holder, Handle<PropertyResult> prop)
{
    MacroAssembler masm(cx, ion, outerScript, profilerLeavePc_);
    Label failures;
    StubAttacher attacher(*this);

    Register scratchReg = outputReg().valueReg().scratchReg();

    // Don't guard the base of the proto chain the name was found on. It will be guarded
    // by GenerateReadSlot().
    masm.mov(environmentChainReg(), scratchReg);
    GenerateEnvironmentChainGuards(masm, envChain, holderBase, scratchReg, &failures,
                             /* skipLastGuard = */true);

    // GenerateEnvironmentChain leaves the last env chain in scratchReg, even though it
    // doesn't generate the extra guard.
    //
    // NAME ops must do their own TDZ checks.
    GenerateReadSlot(cx, ion, masm, attacher, CheckTDZ, holderBase, holder, prop, scratchReg,
                     outputReg(), failures.used() ? &failures : nullptr);

    return linkAndAttachStub(cx, masm, attacher, ion, "generic",
                             JS::TrackedOutcome::ICNameStub_ReadSlot);
}

static bool
IsCacheableEnvironmentChain(JSObject* envChain, JSObject* obj)
{
    JSObject* obj2 = envChain;
    while (obj2) {
        if (!IsCacheableEnvironment(obj2) && !obj2->is<GlobalObject>())
            return false;

        // Stop once we hit the global or target obj.
        if (obj2->is<GlobalObject>() || obj2 == obj)
            break;

        obj2 = obj2->enclosingEnvironment();
    }

    return obj == obj2;
}

static bool
IsCacheableNameReadSlot(JSContext* cx, HandleObject envChain, HandleObject obj,
                        HandleObject holder, Handle<PropertyResult> prop, jsbytecode* pc,
                        const TypedOrValueRegister& output)
{
    if (!prop)
        return false;
    if (!obj->isNative())
        return false;

    if (obj->is<GlobalObject>()) {
        // Support only simple property lookups.
        if (!IsCacheableGetPropReadSlotForIonOrCacheIR(obj, holder, prop) &&
            !IsCacheableNoProperty(obj, holder, prop, pc, output))
            return false;
    } else if (obj->is<ModuleEnvironmentObject>()) {
        // We don't yet support lookups in a module environment.
        return false;
    } else if (obj->is<CallObject>()) {
        MOZ_ASSERT(obj == holder);
        if (!prop.shape()->hasDefaultGetter())
            return false;
    } else {
        // We don't yet support lookups on Block or DeclEnv objects.
        return false;
    }

    return IsCacheableEnvironmentChain(envChain, obj);
}

bool
NameIC::attachCallGetter(JSContext* cx, HandleScript outerScript, IonScript* ion,
                         HandleObject envChain, HandleObject obj, HandleObject holder,
                         HandleShape shape, void* returnAddr)
{
    MacroAssembler masm(cx, ion, outerScript, profilerLeavePc_);
    StubAttacher attacher(*this);

    Label failures;
    Register scratchReg = outputReg().valueReg().scratchReg();

    // Don't guard the base of the proto chain the name was found on. It will be guarded
    // by GenerateCallGetter().
    masm.mov(environmentChainReg(), scratchReg);
    GenerateEnvironmentChainGuards(masm, envChain, obj, scratchReg, &failures,
                             /* skipLastGuard = */true);

    // GenerateEnvironmentChain leaves the last env chain in scratchReg, even though it
    // doesn't generate the extra guard.
    if (!GenerateCallGetter(cx, ion, masm, attacher, obj, holder, shape, liveRegs_,
                            scratchReg, outputReg(), returnAddr,
                            failures.used() ? &failures : nullptr))
    {
         return false;
    }

    const char* attachKind = "name getter";
    return linkAndAttachStub(cx, masm, attacher, ion, attachKind,
                             JS::TrackedOutcome::ICNameStub_CallGetter);
}

static bool
IsCacheableNameCallGetter(HandleObject envChain, HandleObject obj, HandleObject holder,
                          Handle<PropertyResult> prop)
{
    if (!prop)
        return false;
    if (!obj->is<GlobalObject>())
        return false;

    if (!IsCacheableEnvironmentChain(envChain, obj))
        return false;

    if (!prop || !IsCacheableProtoChainForIonOrCacheIR(obj, holder))
        return false;

    Shape* shape = prop.shape();
    return IsCacheableGetPropCallNative(obj, holder, shape) ||
        IsCacheableGetPropCallPropertyOp(obj, holder, shape) ||
        IsCacheableGetPropCallScripted(obj, holder, shape);
}

bool
NameIC::attachTypeOfNoProperty(JSContext* cx, HandleScript outerScript, IonScript* ion,
                               HandleObject envChain)
{
    MacroAssembler masm(cx, ion, outerScript, profilerLeavePc_);
    Label failures;
    StubAttacher attacher(*this);

    Register scratchReg = outputReg().valueReg().scratchReg();

    masm.movePtr(environmentChainReg(), scratchReg);

    // Generate env chain guards.
    // Since the property was not defined on any object, iterate until reaching the global.
    JSObject* tobj = envChain;
    while (true) {
        GenerateEnvironmentChainGuard(masm, tobj, scratchReg, nullptr, &failures);

        if (tobj->is<GlobalObject>())
            break;

        // Load the next link.
        tobj = &tobj->as<EnvironmentObject>().enclosingEnvironment();
        masm.extractObject(Address(scratchReg, EnvironmentObject::offsetOfEnclosingEnvironment()),
                           scratchReg);
    }

    masm.moveValue(UndefinedValue(), outputReg().valueReg());
    attacher.jumpRejoin(masm);

    masm.bind(&failures);
    attacher.jumpNextStub(masm);

    return linkAndAttachStub(cx, masm, attacher, ion, "generic",
                             JS::TrackedOutcome::ICNameStub_TypeOfNoProperty);
}

static bool
IsCacheableNameNoProperty(HandleObject envChain, HandleObject obj,
                          HandleObject holder, Handle<PropertyResult> prop, jsbytecode* pc,
                          NameIC& cache)
{
    if (cache.isTypeOf() && !prop) {
        MOZ_ASSERT(!obj);
        MOZ_ASSERT(!holder);
        MOZ_ASSERT(envChain);

        // Assert those extra things checked by IsCacheableNoProperty().
        MOZ_ASSERT(cache.outputReg().hasValue());
        MOZ_ASSERT(pc != nullptr);

        return true;
    }

    return false;
}

bool
NameIC::update(JSContext* cx, HandleScript outerScript, size_t cacheIndex, HandleObject envChain,
               MutableHandleValue vp)
{
    IonScript* ion = outerScript->ionScript();

    NameIC& cache = ion->getCache(cacheIndex).toName();
    RootedPropertyName name(cx, cache.name());

    RootedScript script(cx);
    jsbytecode* pc;
    cache.getScriptedLocation(&script, &pc);

    RootedObject obj(cx);
    RootedObject holder(cx);
    Rooted<PropertyResult> prop(cx);
    if (!LookupName(cx, name, envChain, &obj, &holder, &prop))
        return false;

    // Look first. Don't generate cache entries if the lookup fails.
    if (cache.isTypeOf()) {
        if (!FetchName<true>(cx, obj, holder, name, prop, vp))
            return false;
    } else {
        if (!FetchName<false>(cx, obj, holder, name, prop, vp))
            return false;
    }

    if (cache.canAttachStub()) {
        if (IsCacheableNameReadSlot(cx, envChain, obj, holder, prop, pc, cache.outputReg())) {
            if (!cache.attachReadSlot(cx, outerScript, ion, envChain, obj,
                                      holder.as<NativeObject>(), prop))
            {
                return false;
            }
        } else if (IsCacheableNameCallGetter(envChain, obj, holder, prop)) {
            void* returnAddr = GetReturnAddressToIonCode(cx);
            RootedShape shape(cx, prop.shape());
            if (!cache.attachCallGetter(cx, outerScript, ion, envChain, obj, holder, shape,
                                        returnAddr))
            {
                return false;
            }
        } else if (IsCacheableNameNoProperty(envChain, obj, holder, prop, pc, cache)) {
            if (!cache.attachTypeOfNoProperty(cx, outerScript, ion, envChain))
                return false;
        }
    }

    // Monitor changes to cache entry.
    TypeScript::Monitor(cx, script, pc, vp);

    return true;
}
