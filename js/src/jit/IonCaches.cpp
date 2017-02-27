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
        masm.enterFakeExitFrame(scratchReg, IonOOLNativeExitFrameLayoutToken);

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
        masm.enterFakeExitFrame(scratchReg, IonOOLPropertyOpExitFrameLayoutToken);

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

void
jit::EmitIonStoreDenseElement(MacroAssembler& masm, const ConstantOrRegister& value,
                              Register elements, BaseObjectElementIndex target)
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
        if (!FetchName<GetNameMode::TypeOf>(cx, obj, holder, name, prop, vp))
            return false;
    } else {
        if (!FetchName<GetNameMode::Normal>(cx, obj, holder, name, prop, vp))
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
