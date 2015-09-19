/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineIC.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/SizePrintfMacros.h"
#include "mozilla/TemplateLib.h"

#include "jslibmath.h"
#include "jstypes.h"

#include "builtin/Eval.h"
#include "builtin/SIMD.h"
#include "jit/BaselineDebugModeOSR.h"
#include "jit/BaselineJIT.h"
#include "jit/JitSpewer.h"
#include "jit/Linker.h"
#include "jit/Lowering.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "jit/SharedICHelpers.h"
#include "jit/VMFunctions.h"
#include "js/Conversions.h"
#include "js/TraceableVector.h"
#include "vm/Opcodes.h"
#include "vm/TypedArrayCommon.h"

#include "jsboolinlines.h"
#include "jsscriptinlines.h"

#include "jit/JitFrames-inl.h"
#include "jit/MacroAssembler-inl.h"
#include "jit/shared/Lowering-shared-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/ScopeObject-inl.h"
#include "vm/StringObject-inl.h"
#include "vm/UnboxedObject-inl.h"

using mozilla::DebugOnly;

namespace js {
namespace jit {

static void
GuardReceiverObject(MacroAssembler& masm, ReceiverGuard guard,
                    Register object, Register scratch,
                    size_t receiverGuardOffset, Label* failure)
{
    Address groupAddress(ICStubReg, receiverGuardOffset + HeapReceiverGuard::offsetOfGroup());
    Address shapeAddress(ICStubReg, receiverGuardOffset + HeapReceiverGuard::offsetOfShape());
    Address expandoAddress(object, UnboxedPlainObject::offsetOfExpando());

    if (guard.group) {
        masm.loadPtr(groupAddress, scratch);
        masm.branchTestObjGroup(Assembler::NotEqual, object, scratch, failure);

        if (guard.group->clasp() == &UnboxedPlainObject::class_ && !guard.shape) {
            // Guard the unboxed object has no expando object.
            masm.branchPtr(Assembler::NotEqual, expandoAddress, ImmWord(0), failure);
        }
    }

    if (guard.shape) {
        masm.loadPtr(shapeAddress, scratch);
        if (guard.group && guard.group->clasp() == &UnboxedPlainObject::class_) {
            // Guard the unboxed object has a matching expando object.
            masm.branchPtr(Assembler::Equal, expandoAddress, ImmWord(0), failure);
            Label done;
            masm.push(object);
            masm.loadPtr(expandoAddress, object);
            masm.branchTestObjShape(Assembler::Equal, object, scratch, &done);
            masm.pop(object);
            masm.jump(failure);
            masm.bind(&done);
            masm.pop(object);
        } else {
            masm.branchTestObjShape(Assembler::NotEqual, object, scratch, failure);
        }
    }
}

//
// WarmUpCounter_Fallback
//

static bool
EnsureCanEnterIon(JSContext* cx, ICWarmUpCounter_Fallback* stub, BaselineFrame* frame,
                  HandleScript script, jsbytecode* pc, void** jitcodePtr)
{
    MOZ_ASSERT(jitcodePtr);
    MOZ_ASSERT(!*jitcodePtr);

    bool isLoopEntry = (JSOp(*pc) == JSOP_LOOPENTRY);

    MethodStatus stat;
    if (isLoopEntry) {
        MOZ_ASSERT(LoopEntryCanIonOsr(pc));
        JitSpew(JitSpew_BaselineOSR, "  Compile at loop entry!");
        stat = CanEnterAtBranch(cx, script, frame, pc);
    } else if (frame->isFunctionFrame()) {
        JitSpew(JitSpew_BaselineOSR, "  Compile function from top for later entry!");
        stat = CompileFunctionForBaseline(cx, script, frame);
    } else {
        return true;
    }

    if (stat == Method_Error) {
        JitSpew(JitSpew_BaselineOSR, "  Compile with Ion errored!");
        return false;
    }

    if (stat == Method_CantCompile)
        JitSpew(JitSpew_BaselineOSR, "  Can't compile with Ion!");
    else if (stat == Method_Skipped)
        JitSpew(JitSpew_BaselineOSR, "  Skipped compile with Ion!");
    else if (stat == Method_Compiled)
        JitSpew(JitSpew_BaselineOSR, "  Compiled with Ion!");
    else
        MOZ_CRASH("Invalid MethodStatus!");

    // Failed to compile.  Reset warm-up counter and return.
    if (stat != Method_Compiled) {
        // TODO: If stat == Method_CantCompile, insert stub that just skips the
        // warm-up counter entirely, instead of resetting it.
        bool bailoutExpected = script->hasIonScript() && script->ionScript()->bailoutExpected();
        if (stat == Method_CantCompile || bailoutExpected) {
            JitSpew(JitSpew_BaselineOSR, "  Reset WarmUpCounter cantCompile=%s bailoutExpected=%s!",
                    stat == Method_CantCompile ? "yes" : "no",
                    bailoutExpected ? "yes" : "no");
            script->resetWarmUpCounter();
        }
        return true;
    }

    if (isLoopEntry) {
        IonScript* ion = script->ionScript();
        MOZ_ASSERT(cx->runtime()->spsProfiler.enabled() == ion->hasProfilingInstrumentation());
        MOZ_ASSERT(ion->osrPc() == pc);

        JitSpew(JitSpew_BaselineOSR, "  OSR possible!");
        *jitcodePtr = ion->method()->raw() + ion->osrEntryOffset();
    }

    return true;
}

//
// The following data is kept in a temporary heap-allocated buffer, stored in
// JitRuntime (high memory addresses at top, low at bottom):
//
//     +----->+=================================+  --      <---- High Address
//     |      |                                 |   |
//     |      |     ...BaselineFrame...         |   |-- Copy of BaselineFrame + stack values
//     |      |                                 |   |
//     |      +---------------------------------+   |
//     |      |                                 |   |
//     |      |     ...Locals/Stack...          |   |
//     |      |                                 |   |
//     |      +=================================+  --
//     |      |     Padding(Maybe Empty)        |
//     |      +=================================+  --
//     +------|-- baselineFrame                 |   |-- IonOsrTempData
//            |   jitcode                       |   |
//            +=================================+  --      <---- Low Address
//
// A pointer to the IonOsrTempData is returned.

struct IonOsrTempData
{
    void* jitcode;
    uint8_t* baselineFrame;
};

static IonOsrTempData*
PrepareOsrTempData(JSContext* cx, ICWarmUpCounter_Fallback* stub, BaselineFrame* frame,
                   HandleScript script, jsbytecode* pc, void* jitcode)
{
    size_t numLocalsAndStackVals = frame->numValueSlots();

    // Calculate the amount of space to allocate:
    //      BaselineFrame space:
    //          (sizeof(Value) * (numLocals + numStackVals))
    //        + sizeof(BaselineFrame)
    //
    //      IonOsrTempData space:
    //          sizeof(IonOsrTempData)

    size_t frameSpace = sizeof(BaselineFrame) + sizeof(Value) * numLocalsAndStackVals;
    size_t ionOsrTempDataSpace = sizeof(IonOsrTempData);

    size_t totalSpace = AlignBytes(frameSpace, sizeof(Value)) +
                        AlignBytes(ionOsrTempDataSpace, sizeof(Value));

    IonOsrTempData* info = (IonOsrTempData*)cx->runtime()->getJitRuntime(cx)->allocateOsrTempData(totalSpace);
    if (!info)
        return nullptr;

    memset(info, 0, totalSpace);

    info->jitcode = jitcode;

    // Copy the BaselineFrame + local/stack Values to the buffer. Arguments and
    // |this| are not copied but left on the stack: the Baseline and Ion frame
    // share the same frame prefix and Ion won't clobber these values. Note
    // that info->baselineFrame will point to the *end* of the frame data, like
    // the frame pointer register in baseline frames.
    uint8_t* frameStart = (uint8_t*)info + AlignBytes(ionOsrTempDataSpace, sizeof(Value));
    info->baselineFrame = frameStart + frameSpace;

    memcpy(frameStart, (uint8_t*)frame - numLocalsAndStackVals * sizeof(Value), frameSpace);

    JitSpew(JitSpew_BaselineOSR, "Allocated IonOsrTempData at %p", (void*) info);
    JitSpew(JitSpew_BaselineOSR, "Jitcode is %p", info->jitcode);

    // All done.
    return info;
}

static bool
DoWarmUpCounterFallback(JSContext* cx, BaselineFrame* frame, ICWarmUpCounter_Fallback* stub,
                        IonOsrTempData** infoPtr)
{
    MOZ_ASSERT(infoPtr);
    *infoPtr = nullptr;

    // A TI OOM will disable TI and Ion.
    if (!jit::IsIonEnabled(cx))
        return true;

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    bool isLoopEntry = JSOp(*pc) == JSOP_LOOPENTRY;

    MOZ_ASSERT(!isLoopEntry || LoopEntryCanIonOsr(pc));

    FallbackICSpew(cx, stub, "WarmUpCounter(%d)", isLoopEntry ? int(script->pcToOffset(pc)) : int(-1));

    if (!script->canIonCompile()) {
        // TODO: ASSERT that ion-compilation-disabled checker stub doesn't exist.
        // TODO: Clear all optimized stubs.
        // TODO: Add a ion-compilation-disabled checker IC stub
        script->resetWarmUpCounter();
        return true;
    }

    MOZ_ASSERT(!script->isIonCompilingOffThread());

    // If Ion script exists, but PC is not at a loop entry, then Ion will be entered for
    // this script at an appropriate LOOPENTRY or the next time this function is called.
    if (script->hasIonScript() && !isLoopEntry) {
        JitSpew(JitSpew_BaselineOSR, "IonScript exists, but not at loop entry!");
        // TODO: ASSERT that a ion-script-already-exists checker stub doesn't exist.
        // TODO: Clear all optimized stubs.
        // TODO: Add a ion-script-already-exists checker stub.
        return true;
    }

    // Ensure that Ion-compiled code is available.
    JitSpew(JitSpew_BaselineOSR,
            "WarmUpCounter for %s:%" PRIuSIZE " reached %d at pc %p, trying to switch to Ion!",
            script->filename(), script->lineno(), (int) script->getWarmUpCount(), (void*) pc);
    void* jitcode = nullptr;
    if (!EnsureCanEnterIon(cx, stub, frame, script, pc, &jitcode))
        return false;

    // Jitcode should only be set here if not at loop entry.
    MOZ_ASSERT_IF(!isLoopEntry, !jitcode);
    if (!jitcode)
        return true;

    // Prepare the temporary heap copy of the fake InterpreterFrame and actual args list.
    JitSpew(JitSpew_BaselineOSR, "Got jitcode.  Preparing for OSR into ion.");
    IonOsrTempData* info = PrepareOsrTempData(cx, stub, frame, script, pc, jitcode);
    if (!info)
        return false;
    *infoPtr = info;

    return true;
}

typedef bool (*DoWarmUpCounterFallbackFn)(JSContext*, BaselineFrame*,
                                          ICWarmUpCounter_Fallback*, IonOsrTempData** infoPtr);
static const VMFunction DoWarmUpCounterFallbackInfo =
    FunctionInfo<DoWarmUpCounterFallbackFn>(DoWarmUpCounterFallback);

bool
ICWarmUpCounter_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    // Push a stub frame so that we can perform a non-tail call.
    enterStubFrame(masm, R1.scratchReg());

    Label noCompiledCode;
    // Call DoWarmUpCounterFallback to compile/check-for Ion-compiled function
    {
        // Push IonOsrTempData pointer storage
        masm.subFromStackPtr(Imm32(sizeof(void*)));
        masm.push(masm.getStackPointer());

        // Push stub pointer.
        masm.push(ICStubReg);

        pushFramePtr(masm, R0.scratchReg());

        if (!callVM(DoWarmUpCounterFallbackInfo, masm))
            return false;

        // Pop IonOsrTempData pointer.
        masm.pop(R0.scratchReg());

        leaveStubFrame(masm);

        // If no JitCode was found, then skip just exit the IC.
        masm.branchPtr(Assembler::Equal, R0.scratchReg(), ImmPtr(nullptr), &noCompiledCode);
    }

    // Get a scratch register.
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));
    Register osrDataReg = R0.scratchReg();
    regs.take(osrDataReg);
    regs.takeUnchecked(OsrFrameReg);

    Register scratchReg = regs.takeAny();

    // At this point, stack looks like:
    //  +-> [...Calling-Frame...]
    //  |   [...Actual-Args/ThisV/ArgCount/Callee...]
    //  |   [Descriptor]
    //  |   [Return-Addr]
    //  +---[Saved-FramePtr]            <-- BaselineFrameReg points here.
    //      [...Baseline-Frame...]

    // Restore the stack pointer to point to the saved frame pointer.
    masm.moveToStackPtr(BaselineFrameReg);

    // Discard saved frame pointer, so that the return address is on top of
    // the stack.
    masm.pop(scratchReg);

#ifdef DEBUG
    // If profiler instrumentation is on, ensure that lastProfilingFrame is
    // the frame currently being OSR-ed
    {
        Label checkOk;
        AbsoluteAddress addressOfEnabled(cx->runtime()->spsProfiler.addressOfEnabled());
        masm.branch32(Assembler::Equal, addressOfEnabled, Imm32(0), &checkOk);
        masm.loadPtr(AbsoluteAddress((void*)&cx->runtime()->jitActivation), scratchReg);
        masm.loadPtr(Address(scratchReg, JitActivation::offsetOfLastProfilingFrame()), scratchReg);

        // It may be the case that we entered the baseline frame with
        // profiling turned off on, then in a call within a loop (i.e. a
        // callee frame), turn on profiling, then return to this frame,
        // and then OSR with profiling turned on.  In this case, allow for
        // lastProfilingFrame to be null.
        masm.branchPtr(Assembler::Equal, scratchReg, ImmWord(0), &checkOk);

        masm.branchStackPtr(Assembler::Equal, scratchReg, &checkOk);
        masm.assumeUnreachable("Baseline OSR lastProfilingFrame mismatch.");
        masm.bind(&checkOk);
    }
#endif

    // Jump into Ion.
    masm.loadPtr(Address(osrDataReg, offsetof(IonOsrTempData, jitcode)), scratchReg);
    masm.loadPtr(Address(osrDataReg, offsetof(IonOsrTempData, baselineFrame)), OsrFrameReg);
    masm.jump(scratchReg);

    // No jitcode available, do nothing.
    masm.bind(&noCompiledCode);
    EmitReturnFromIC(masm);
    return true;
}


//
// TypeMonitor_Fallback
//

bool
ICTypeMonitor_Fallback::addMonitorStubForValue(JSContext* cx, JSScript* script, HandleValue val)
{
    bool wasDetachedMonitorChain = lastMonitorStubPtrAddr_ == nullptr;
    MOZ_ASSERT_IF(wasDetachedMonitorChain, numOptimizedMonitorStubs_ == 0);

    if (numOptimizedMonitorStubs_ >= MAX_OPTIMIZED_STUBS) {
        // TODO: if the TypeSet becomes unknown or has the AnyObject type,
        // replace stubs with a single stub to handle these.
        return true;
    }

    if (val.isPrimitive()) {
        MOZ_ASSERT(!val.isMagic());
        JSValueType type = val.isDouble() ? JSVAL_TYPE_DOUBLE : val.extractNonDoubleType();

        // Check for existing TypeMonitor stub.
        ICTypeMonitor_PrimitiveSet* existingStub = nullptr;
        for (ICStubConstIterator iter(firstMonitorStub()); !iter.atEnd(); iter++) {
            if (iter->isTypeMonitor_PrimitiveSet()) {
                existingStub = iter->toTypeMonitor_PrimitiveSet();
                if (existingStub->containsType(type))
                    return true;
            }
        }

        ICTypeMonitor_PrimitiveSet::Compiler compiler(cx, existingStub, type);
        ICStub* stub = existingStub ? compiler.updateStub()
                                    : compiler.getStub(compiler.getStubSpace(script));
        if (!stub) {
            ReportOutOfMemory(cx);
            return false;
        }

        JitSpew(JitSpew_BaselineIC, "  %s TypeMonitor stub %p for primitive type %d",
                existingStub ? "Modified existing" : "Created new", stub, type);

        if (!existingStub) {
            MOZ_ASSERT(!hasStub(TypeMonitor_PrimitiveSet));
            addOptimizedMonitorStub(stub);
        }

    } else if (val.toObject().isSingleton()) {
        RootedObject obj(cx, &val.toObject());

        // Check for existing TypeMonitor stub.
        for (ICStubConstIterator iter(firstMonitorStub()); !iter.atEnd(); iter++) {
            if (iter->isTypeMonitor_SingleObject() &&
                iter->toTypeMonitor_SingleObject()->object() == obj)
            {
                return true;
            }
        }

        ICTypeMonitor_SingleObject::Compiler compiler(cx, obj);
        ICStub* stub = compiler.getStub(compiler.getStubSpace(script));
        if (!stub) {
            ReportOutOfMemory(cx);
            return false;
        }

        JitSpew(JitSpew_BaselineIC, "  Added TypeMonitor stub %p for singleton %p",
                stub, obj.get());

        addOptimizedMonitorStub(stub);

    } else {
        RootedObjectGroup group(cx, val.toObject().group());

        // Check for existing TypeMonitor stub.
        for (ICStubConstIterator iter(firstMonitorStub()); !iter.atEnd(); iter++) {
            if (iter->isTypeMonitor_ObjectGroup() &&
                iter->toTypeMonitor_ObjectGroup()->group() == group)
            {
                return true;
            }
        }

        ICTypeMonitor_ObjectGroup::Compiler compiler(cx, group);
        ICStub* stub = compiler.getStub(compiler.getStubSpace(script));
        if (!stub) {
            ReportOutOfMemory(cx);
            return false;
        }

        JitSpew(JitSpew_BaselineIC, "  Added TypeMonitor stub %p for ObjectGroup %p",
                stub, group.get());

        addOptimizedMonitorStub(stub);
    }

    bool firstMonitorStubAdded = wasDetachedMonitorChain && (numOptimizedMonitorStubs_ > 0);

    if (firstMonitorStubAdded) {
        // Was an empty monitor chain before, but a new stub was added.  This is the
        // only time that any main stubs' firstMonitorStub fields need to be updated to
        // refer to the newly added monitor stub.
        ICStub* firstStub = mainFallbackStub_->icEntry()->firstStub();
        for (ICStubConstIterator iter(firstStub); !iter.atEnd(); iter++) {
            // Non-monitored stubs are used if the result has always the same type,
            // e.g. a StringLength stub will always return int32.
            if (!iter->isMonitored())
                continue;

            // Since we just added the first optimized monitoring stub, any
            // existing main stub's |firstMonitorStub| MUST be pointing to the fallback
            // monitor stub (i.e. this stub).
            MOZ_ASSERT(iter->toMonitoredStub()->firstMonitorStub() == this);
            iter->toMonitoredStub()->updateFirstMonitorStub(firstMonitorStub_);
        }
    }

    return true;
}

static bool
DoTypeMonitorFallback(JSContext* cx, BaselineFrame* frame, ICTypeMonitor_Fallback* stub,
                      HandleValue value, MutableHandleValue res)
{
    // It's possible that we arrived here from bailing out of Ion, and that
    // Ion proved that the value is dead and optimized out. In such cases, do
    // nothing.
    if (value.isMagic(JS_OPTIMIZED_OUT)) {
        res.set(value);
        return true;
    }

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    TypeFallbackICSpew(cx, stub, "TypeMonitor");

    uint32_t argument;
    if (stub->monitorsThis()) {
        MOZ_ASSERT(pc == script->code());
        TypeScript::SetThis(cx, script, value);
    } else if (stub->monitorsArgument(&argument)) {
        MOZ_ASSERT(pc == script->code());
        TypeScript::SetArgument(cx, script, argument, value);
    } else {
        TypeScript::Monitor(cx, script, pc, value);
    }

    if (!stub->addMonitorStubForValue(cx, script, value))
        return false;

    // Copy input value to res.
    res.set(value);
    return true;
}

typedef bool (*DoTypeMonitorFallbackFn)(JSContext*, BaselineFrame*, ICTypeMonitor_Fallback*,
                                        HandleValue, MutableHandleValue);
static const VMFunction DoTypeMonitorFallbackInfo =
    FunctionInfo<DoTypeMonitorFallbackFn>(DoTypeMonitorFallback, TailCall);

bool
ICTypeMonitor_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);
    MOZ_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushFramePtr(masm, R0.scratchReg());

    return tailCallVM(DoTypeMonitorFallbackInfo, masm);
}

bool
ICTypeMonitor_PrimitiveSet::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label success;
    if ((flags_ & TypeToFlag(JSVAL_TYPE_INT32)) && !(flags_ & TypeToFlag(JSVAL_TYPE_DOUBLE)))
        masm.branchTestInt32(Assembler::Equal, R0, &success);

    if (flags_ & TypeToFlag(JSVAL_TYPE_DOUBLE))
        masm.branchTestNumber(Assembler::Equal, R0, &success);

    if (flags_ & TypeToFlag(JSVAL_TYPE_UNDEFINED))
        masm.branchTestUndefined(Assembler::Equal, R0, &success);

    if (flags_ & TypeToFlag(JSVAL_TYPE_BOOLEAN))
        masm.branchTestBoolean(Assembler::Equal, R0, &success);

    if (flags_ & TypeToFlag(JSVAL_TYPE_STRING))
        masm.branchTestString(Assembler::Equal, R0, &success);

    if (flags_ & TypeToFlag(JSVAL_TYPE_SYMBOL))
        masm.branchTestSymbol(Assembler::Equal, R0, &success);

    // Currently, we will never generate primitive stub checks for object.  However,
    // when we do get to the point where we want to collapse our monitor chains of
    // objects and singletons down (when they get too long) to a generic "any object"
    // in coordination with the typeset doing the same thing, this will need to
    // be re-enabled.
    /*
    if (flags_ & TypeToFlag(JSVAL_TYPE_OBJECT))
        masm.branchTestObject(Assembler::Equal, R0, &success);
    */
    MOZ_ASSERT(!(flags_ & TypeToFlag(JSVAL_TYPE_OBJECT)));

    if (flags_ & TypeToFlag(JSVAL_TYPE_NULL))
        masm.branchTestNull(Assembler::Equal, R0, &success);

    EmitStubGuardFailure(masm);

    masm.bind(&success);
    EmitReturnFromIC(masm);
    return true;
}

bool
ICTypeMonitor_SingleObject::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    // Guard on the object's identity.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    Address expectedObject(ICStubReg, ICTypeMonitor_SingleObject::offsetOfObject());
    masm.branchPtr(Assembler::NotEqual, expectedObject, obj, &failure);

    EmitReturnFromIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICTypeMonitor_ObjectGroup::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    // Guard on the object's ObjectGroup.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    masm.loadPtr(Address(obj, JSObject::offsetOfGroup()), R1.scratchReg());

    Address expectedGroup(ICStubReg, ICTypeMonitor_ObjectGroup::offsetOfGroup());
    masm.branchPtr(Assembler::NotEqual, expectedGroup, R1.scratchReg(), &failure);

    EmitReturnFromIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICUpdatedStub::addUpdateStubForValue(JSContext* cx, HandleScript script, HandleObject obj,
                                     HandleId id, HandleValue val)
{
    if (numOptimizedStubs_ >= MAX_OPTIMIZED_STUBS) {
        // TODO: if the TypeSet becomes unknown or has the AnyObject type,
        // replace stubs with a single stub to handle these.
        return true;
    }

    EnsureTrackPropertyTypes(cx, obj, id);

    // Make sure that undefined values are explicitly included in the property
    // types for an object if generating a stub to write an undefined value.
    if (val.isUndefined() && CanHaveEmptyPropertyTypesForOwnProperty(obj))
        AddTypePropertyId(cx, obj, id, val);

    if (val.isPrimitive()) {
        JSValueType type = val.isDouble() ? JSVAL_TYPE_DOUBLE : val.extractNonDoubleType();

        // Check for existing TypeUpdate stub.
        ICTypeUpdate_PrimitiveSet* existingStub = nullptr;
        for (ICStubConstIterator iter(firstUpdateStub_); !iter.atEnd(); iter++) {
            if (iter->isTypeUpdate_PrimitiveSet()) {
                existingStub = iter->toTypeUpdate_PrimitiveSet();
                if (existingStub->containsType(type))
                    return true;
            }
        }

        ICTypeUpdate_PrimitiveSet::Compiler compiler(cx, existingStub, type);
        ICStub* stub = existingStub ? compiler.updateStub()
                                    : compiler.getStub(compiler.getStubSpace(script));
        if (!stub)
            return false;
        if (!existingStub) {
            MOZ_ASSERT(!hasTypeUpdateStub(TypeUpdate_PrimitiveSet));
            addOptimizedUpdateStub(stub);
        }

        JitSpew(JitSpew_BaselineIC, "  %s TypeUpdate stub %p for primitive type %d",
                existingStub ? "Modified existing" : "Created new", stub, type);

    } else if (val.toObject().isSingleton()) {
        RootedObject obj(cx, &val.toObject());

        // Check for existing TypeUpdate stub.
        for (ICStubConstIterator iter(firstUpdateStub_); !iter.atEnd(); iter++) {
            if (iter->isTypeUpdate_SingleObject() &&
                iter->toTypeUpdate_SingleObject()->object() == obj)
            {
                return true;
            }
        }

        ICTypeUpdate_SingleObject::Compiler compiler(cx, obj);
        ICStub* stub = compiler.getStub(compiler.getStubSpace(script));
        if (!stub)
            return false;

        JitSpew(JitSpew_BaselineIC, "  Added TypeUpdate stub %p for singleton %p", stub, obj.get());

        addOptimizedUpdateStub(stub);

    } else {
        RootedObjectGroup group(cx, val.toObject().group());

        // Check for existing TypeUpdate stub.
        for (ICStubConstIterator iter(firstUpdateStub_); !iter.atEnd(); iter++) {
            if (iter->isTypeUpdate_ObjectGroup() &&
                iter->toTypeUpdate_ObjectGroup()->group() == group)
            {
                return true;
            }
        }

        ICTypeUpdate_ObjectGroup::Compiler compiler(cx, group);
        ICStub* stub = compiler.getStub(compiler.getStubSpace(script));
        if (!stub)
            return false;

        JitSpew(JitSpew_BaselineIC, "  Added TypeUpdate stub %p for ObjectGroup %p",
                stub, group.get());

        addOptimizedUpdateStub(stub);
    }

    return true;
}

//
// TypeUpdate_Fallback
//
static bool
DoTypeUpdateFallback(JSContext* cx, BaselineFrame* frame, ICUpdatedStub* stub, HandleValue objval,
                     HandleValue value)
{
    FallbackICSpew(cx, stub->getChainFallback(), "TypeUpdate(%s)",
                   ICStub::KindString(stub->kind()));

    RootedScript script(cx, frame->script());
    RootedObject obj(cx, &objval.toObject());
    RootedId id(cx);

    switch(stub->kind()) {
      case ICStub::SetElem_DenseOrUnboxedArray:
      case ICStub::SetElem_DenseOrUnboxedArrayAdd: {
        id = JSID_VOID;
        AddTypePropertyId(cx, obj, id, value);
        break;
      }
      case ICStub::SetProp_Native:
      case ICStub::SetProp_NativeAdd:
      case ICStub::SetProp_Unboxed: {
        MOZ_ASSERT(obj->isNative() || obj->is<UnboxedPlainObject>());
        jsbytecode* pc = stub->getChainFallback()->icEntry()->pc(script);
        if (*pc == JSOP_SETALIASEDVAR || *pc == JSOP_INITALIASEDLEXICAL)
            id = NameToId(ScopeCoordinateName(cx->runtime()->scopeCoordinateNameCache, script, pc));
        else
            id = NameToId(script->getName(pc));
        AddTypePropertyId(cx, obj, id, value);
        break;
      }
      case ICStub::SetProp_TypedObject: {
        MOZ_ASSERT(obj->is<TypedObject>());
        jsbytecode* pc = stub->getChainFallback()->icEntry()->pc(script);
        id = NameToId(script->getName(pc));
        if (stub->toSetProp_TypedObject()->isObjectReference()) {
            // Ignore all values being written except plain objects. Null
            // is included implicitly in type information for this property,
            // and non-object non-null values will cause the stub to fail to
            // match shortly and we will end up doing the assignment in the VM.
            if (value.isObject())
                AddTypePropertyId(cx, obj, id, value);
        } else {
            // Ignore undefined values, which are included implicitly in type
            // information for this property.
            if (!value.isUndefined())
                AddTypePropertyId(cx, obj, id, value);
        }
        break;
      }
      default:
        MOZ_CRASH("Invalid stub");
    }

    return stub->addUpdateStubForValue(cx, script, obj, id, value);
}

typedef bool (*DoTypeUpdateFallbackFn)(JSContext*, BaselineFrame*, ICUpdatedStub*, HandleValue,
                                       HandleValue);
const VMFunction DoTypeUpdateFallbackInfo =
    FunctionInfo<DoTypeUpdateFallbackFn>(DoTypeUpdateFallback, NonTailCall);

bool
ICTypeUpdate_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    // Just store false into R1.scratchReg() and return.
    masm.move32(Imm32(0), R1.scratchReg());
    EmitReturnFromIC(masm);
    return true;
}

bool
ICTypeUpdate_PrimitiveSet::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label success;
    if ((flags_ & TypeToFlag(JSVAL_TYPE_INT32)) && !(flags_ & TypeToFlag(JSVAL_TYPE_DOUBLE)))
        masm.branchTestInt32(Assembler::Equal, R0, &success);

    if (flags_ & TypeToFlag(JSVAL_TYPE_DOUBLE))
        masm.branchTestNumber(Assembler::Equal, R0, &success);

    if (flags_ & TypeToFlag(JSVAL_TYPE_UNDEFINED))
        masm.branchTestUndefined(Assembler::Equal, R0, &success);

    if (flags_ & TypeToFlag(JSVAL_TYPE_BOOLEAN))
        masm.branchTestBoolean(Assembler::Equal, R0, &success);

    if (flags_ & TypeToFlag(JSVAL_TYPE_STRING))
        masm.branchTestString(Assembler::Equal, R0, &success);

    if (flags_ & TypeToFlag(JSVAL_TYPE_SYMBOL))
        masm.branchTestSymbol(Assembler::Equal, R0, &success);

    // Currently, we will never generate primitive stub checks for object.  However,
    // when we do get to the point where we want to collapse our monitor chains of
    // objects and singletons down (when they get too long) to a generic "any object"
    // in coordination with the typeset doing the same thing, this will need to
    // be re-enabled.
    /*
    if (flags_ & TypeToFlag(JSVAL_TYPE_OBJECT))
        masm.branchTestObject(Assembler::Equal, R0, &success);
    */
    MOZ_ASSERT(!(flags_ & TypeToFlag(JSVAL_TYPE_OBJECT)));

    if (flags_ & TypeToFlag(JSVAL_TYPE_NULL))
        masm.branchTestNull(Assembler::Equal, R0, &success);

    EmitStubGuardFailure(masm);

    // Type matches, load true into R1.scratchReg() and return.
    masm.bind(&success);
    masm.mov(ImmWord(1), R1.scratchReg());
    EmitReturnFromIC(masm);

    return true;
}

bool
ICTypeUpdate_SingleObject::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    // Guard on the object's identity.
    Register obj = masm.extractObject(R0, R1.scratchReg());
    Address expectedObject(ICStubReg, ICTypeUpdate_SingleObject::offsetOfObject());
    masm.branchPtr(Assembler::NotEqual, expectedObject, obj, &failure);

    // Identity matches, load true into R1.scratchReg() and return.
    masm.mov(ImmWord(1), R1.scratchReg());
    EmitReturnFromIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICTypeUpdate_ObjectGroup::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    // Guard on the object's ObjectGroup.
    Register obj = masm.extractObject(R0, R1.scratchReg());
    masm.loadPtr(Address(obj, JSObject::offsetOfGroup()), R1.scratchReg());

    Address expectedGroup(ICStubReg, ICTypeUpdate_ObjectGroup::offsetOfGroup());
    masm.branchPtr(Assembler::NotEqual, expectedGroup, R1.scratchReg(), &failure);

    // Group matches, load true into R1.scratchReg() and return.
    masm.mov(ImmWord(1), R1.scratchReg());
    EmitReturnFromIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// VM function to help call native getters.
//

static bool
DoCallNativeGetter(JSContext* cx, HandleFunction callee, HandleObject obj,
                   MutableHandleValue result)
{
    MOZ_ASSERT(callee->isNative());
    JSNative natfun = callee->native();

    JS::AutoValueArray<2> vp(cx);
    vp[0].setObject(*callee.get());
    vp[1].setObject(*obj.get());

    if (!natfun(cx, 0, vp.begin()))
        return false;

    result.set(vp[0]);
    return true;
}

typedef bool (*DoCallNativeGetterFn)(JSContext*, HandleFunction, HandleObject, MutableHandleValue);
static const VMFunction DoCallNativeGetterInfo =
    FunctionInfo<DoCallNativeGetterFn>(DoCallNativeGetter);

//
// This_Fallback
//

static bool
DoThisFallback(JSContext* cx, ICThis_Fallback* stub, HandleValue thisv, MutableHandleValue ret)
{
    FallbackICSpew(cx, stub, "This");

    JSObject* thisObj = BoxNonStrictThis(cx, thisv);
    if (!thisObj)
        return false;

    ret.setObject(*thisObj);
    return true;
}

typedef bool (*DoThisFallbackFn)(JSContext*, ICThis_Fallback*, HandleValue, MutableHandleValue);
static const VMFunction DoThisFallbackInfo = FunctionInfo<DoThisFallbackFn>(DoThisFallback, TailCall);

bool
ICThis_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);
    MOZ_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    masm.pushValue(R0);
    masm.push(ICStubReg);

    return tailCallVM(DoThisFallbackInfo, masm);
}

//
// NewArray_Fallback
//

static bool
DoNewArray(JSContext* cx, BaselineFrame* frame, ICNewArray_Fallback* stub, uint32_t length,
           MutableHandleValue res)
{
    FallbackICSpew(cx, stub, "NewArray");

    RootedObject obj(cx);
    if (stub->templateObject()) {
        RootedObject templateObject(cx, stub->templateObject());
        obj = NewArrayOperationWithTemplate(cx, templateObject);
        if (!obj)
            return false;
    } else {
        RootedScript script(cx, frame->script());
        jsbytecode* pc = stub->icEntry()->pc(script);
        obj = NewArrayOperation(cx, script, pc, length);
        if (!obj)
            return false;

        if (obj && !obj->isSingleton() && !obj->group()->maybePreliminaryObjects()) {
            JSObject* templateObject = NewArrayOperation(cx, script, pc, length, TenuredObject);
            if (!templateObject)
                return false;
            stub->setTemplateObject(templateObject);
        }
    }

    res.setObject(*obj);
    return true;
}

typedef bool(*DoNewArrayFn)(JSContext*, BaselineFrame*, ICNewArray_Fallback*, uint32_t,
                            MutableHandleValue);
static const VMFunction DoNewArrayInfo = FunctionInfo<DoNewArrayFn>(DoNewArray, TailCall);

bool
ICNewArray_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    EmitRestoreTailCallReg(masm);

    masm.push(R0.scratchReg()); // length
    masm.push(ICStubReg); // stub.
    pushFramePtr(masm, R0.scratchReg());

    return tailCallVM(DoNewArrayInfo, masm);
}

//
// NewObject_Fallback
//

// Unlike typical baseline IC stubs, the code for NewObject_WithTemplate is
// specialized for the template object being allocated.
static JitCode*
GenerateNewObjectWithTemplateCode(JSContext* cx, JSObject* templateObject)
{
    JitContext jctx(cx, nullptr);
    MacroAssembler masm;
#ifdef JS_CODEGEN_ARM
    masm.setSecondScratchReg(BaselineSecondScratchReg);
#endif

    Label failure;
    Register objReg = R0.scratchReg();
    Register tempReg = R1.scratchReg();
    masm.movePtr(ImmGCPtr(templateObject->group()), tempReg);
    masm.branchTest32(Assembler::NonZero, Address(tempReg, ObjectGroup::offsetOfFlags()),
                      Imm32(OBJECT_FLAG_PRE_TENURE), &failure);
    masm.branchPtr(Assembler::NotEqual, AbsoluteAddress(cx->compartment()->addressOfMetadataCallback()),
                   ImmWord(0), &failure);
    masm.createGCObject(objReg, tempReg, templateObject, gc::DefaultHeap, &failure);
    masm.tagValue(JSVAL_TYPE_OBJECT, objReg, R0);

    EmitReturnFromIC(masm);
    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    Linker linker(masm);
    AutoFlushICache afc("GenerateNewObjectWithTemplateCode");
    return linker.newCode<CanGC>(cx, BASELINE_CODE);
}

static bool
DoNewObject(JSContext* cx, BaselineFrame* frame, ICNewObject_Fallback* stub, MutableHandleValue res)
{
    FallbackICSpew(cx, stub, "NewObject");

    RootedObject obj(cx);

    RootedObject templateObject(cx, stub->templateObject());
    if (templateObject) {
        MOZ_ASSERT(!templateObject->group()->maybePreliminaryObjects());
        obj = NewObjectOperationWithTemplate(cx, templateObject);
    } else {
        RootedScript script(cx, frame->script());
        jsbytecode* pc = stub->icEntry()->pc(script);
        obj = NewObjectOperation(cx, script, pc);

        if (obj && !obj->isSingleton() && !obj->group()->maybePreliminaryObjects()) {
            JSObject* templateObject = NewObjectOperation(cx, script, pc, TenuredObject);
            if (!templateObject)
                return false;

            if (templateObject->is<UnboxedPlainObject>() ||
                !templateObject->as<PlainObject>().hasDynamicSlots())
            {
                JitCode* code = GenerateNewObjectWithTemplateCode(cx, templateObject);
                if (!code)
                    return false;

                ICStubSpace* space =
                    ICStubCompiler::StubSpaceForKind(ICStub::NewObject_WithTemplate, script);
                ICStub* templateStub = ICStub::New<ICNewObject_WithTemplate>(cx, space, code);
                if (!templateStub)
                    return false;

                stub->addNewStub(templateStub);
            }

            stub->setTemplateObject(templateObject);
        }
    }

    if (!obj)
        return false;

    res.setObject(*obj);
    return true;
}

typedef bool(*DoNewObjectFn)(JSContext*, BaselineFrame*, ICNewObject_Fallback*, MutableHandleValue);
static const VMFunction DoNewObjectInfo = FunctionInfo<DoNewObjectFn>(DoNewObject, TailCall);

bool
ICNewObject_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    EmitRestoreTailCallReg(masm);

    masm.push(ICStubReg); // stub.
    pushFramePtr(masm, R0.scratchReg());

    return tailCallVM(DoNewObjectInfo, masm);
}

//
// ToBool_Fallback
//

static bool
DoToBoolFallback(JSContext* cx, BaselineFrame* frame, ICToBool_Fallback* stub, HandleValue arg,
                 MutableHandleValue ret)
{
    FallbackICSpew(cx, stub, "ToBool");

    bool cond = ToBoolean(arg);
    ret.setBoolean(cond);

    // Check to see if a new stub should be generated.
    if (stub->numOptimizedStubs() >= ICToBool_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard all stubs in this IC and replace with inert megamorphic stub.
        // But for now we just bail.
        return true;
    }

    MOZ_ASSERT(!arg.isBoolean());

    JSScript* script = frame->script();

    // Try to generate new stubs.
    if (arg.isInt32()) {
        JitSpew(JitSpew_BaselineIC, "  Generating ToBool(Int32) stub.");
        ICToBool_Int32::Compiler compiler(cx);
        ICStub* int32Stub = compiler.getStub(compiler.getStubSpace(script));
        if (!int32Stub)
            return false;

        stub->addNewStub(int32Stub);
        return true;
    }

    if (arg.isDouble() && cx->runtime()->jitSupportsFloatingPoint) {
        JitSpew(JitSpew_BaselineIC, "  Generating ToBool(Double) stub.");
        ICToBool_Double::Compiler compiler(cx);
        ICStub* doubleStub = compiler.getStub(compiler.getStubSpace(script));
        if (!doubleStub)
            return false;

        stub->addNewStub(doubleStub);
        return true;
    }

    if (arg.isString()) {
        JitSpew(JitSpew_BaselineIC, "  Generating ToBool(String) stub");
        ICToBool_String::Compiler compiler(cx);
        ICStub* stringStub = compiler.getStub(compiler.getStubSpace(script));
        if (!stringStub)
            return false;

        stub->addNewStub(stringStub);
        return true;
    }

    if (arg.isNull() || arg.isUndefined()) {
        ICToBool_NullUndefined::Compiler compiler(cx);
        ICStub* nilStub = compiler.getStub(compiler.getStubSpace(script));
        if (!nilStub)
            return false;

        stub->addNewStub(nilStub);
        return true;
    }

    if (arg.isObject()) {
        JitSpew(JitSpew_BaselineIC, "  Generating ToBool(Object) stub.");
        ICToBool_Object::Compiler compiler(cx);
        ICStub* objStub = compiler.getStub(compiler.getStubSpace(script));
        if (!objStub)
            return false;

        stub->addNewStub(objStub);
        return true;
    }

    return true;
}

typedef bool (*pf)(JSContext*, BaselineFrame*, ICToBool_Fallback*, HandleValue,
                   MutableHandleValue);
static const VMFunction fun = FunctionInfo<pf>(DoToBoolFallback, TailCall);

bool
ICToBool_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);
    MOZ_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    // Push arguments.
    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushFramePtr(masm, R0.scratchReg());

    return tailCallVM(fun, masm);
}

//
// ToBool_Int32
//

bool
ICToBool_Int32::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    masm.branchTestInt32(Assembler::NotEqual, R0, &failure);

    Label ifFalse;
    masm.branchTestInt32Truthy(false, R0, &ifFalse);

    masm.moveValue(BooleanValue(true), R0);
    EmitReturnFromIC(masm);

    masm.bind(&ifFalse);
    masm.moveValue(BooleanValue(false), R0);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// ToBool_String
//

bool
ICToBool_String::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    masm.branchTestString(Assembler::NotEqual, R0, &failure);

    Label ifFalse;
    masm.branchTestStringTruthy(false, R0, &ifFalse);

    masm.moveValue(BooleanValue(true), R0);
    EmitReturnFromIC(masm);

    masm.bind(&ifFalse);
    masm.moveValue(BooleanValue(false), R0);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// ToBool_NullUndefined
//

bool
ICToBool_NullUndefined::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure, ifFalse;
    masm.branchTestNull(Assembler::Equal, R0, &ifFalse);
    masm.branchTestUndefined(Assembler::NotEqual, R0, &failure);

    masm.bind(&ifFalse);
    masm.moveValue(BooleanValue(false), R0);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// ToBool_Double
//

bool
ICToBool_Double::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure, ifTrue;
    masm.branchTestDouble(Assembler::NotEqual, R0, &failure);
    masm.unboxDouble(R0, FloatReg0);
    masm.branchTestDoubleTruthy(true, FloatReg0, &ifTrue);

    masm.moveValue(BooleanValue(false), R0);
    EmitReturnFromIC(masm);

    masm.bind(&ifTrue);
    masm.moveValue(BooleanValue(true), R0);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// ToBool_Object
//

bool
ICToBool_Object::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure, ifFalse, slowPath;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    Register objReg = masm.extractObject(R0, ExtractTemp0);
    Register scratch = R1.scratchReg();
    masm.branchTestObjectTruthy(false, objReg, scratch, &slowPath, &ifFalse);

    // If object doesn't emulate undefined, it evaulates to true.
    masm.moveValue(BooleanValue(true), R0);
    EmitReturnFromIC(masm);

    masm.bind(&ifFalse);
    masm.moveValue(BooleanValue(false), R0);
    EmitReturnFromIC(masm);

    masm.bind(&slowPath);
    masm.setupUnalignedABICall(scratch);
    masm.passABIArg(objReg);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, js::EmulatesUndefined));
    masm.convertBoolToInt32(ReturnReg, ReturnReg);
    masm.xor32(Imm32(1), ReturnReg);
    masm.tagValue(JSVAL_TYPE_BOOLEAN, ReturnReg, R0);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// ToNumber_Fallback
//

static bool
DoToNumberFallback(JSContext* cx, ICToNumber_Fallback* stub, HandleValue arg, MutableHandleValue ret)
{
    FallbackICSpew(cx, stub, "ToNumber");
    ret.set(arg);
    return ToNumber(cx, ret);
}

typedef bool (*DoToNumberFallbackFn)(JSContext*, ICToNumber_Fallback*, HandleValue, MutableHandleValue);
static const VMFunction DoToNumberFallbackInfo =
    FunctionInfo<DoToNumberFallbackFn>(DoToNumberFallback, TailCall, PopValues(1));

bool
ICToNumber_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);
    MOZ_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    // Ensure stack is fully synced for the expression decompiler.
    masm.pushValue(R0);

    // Push arguments.
    masm.pushValue(R0);
    masm.push(ICStubReg);

    return tailCallVM(DoToNumberFallbackInfo, masm);
}

//
// GetElem_Fallback
//

static void GetFixedOrDynamicSlotOffset(Shape* shape, bool* isFixed, uint32_t* offset)
{
    MOZ_ASSERT(isFixed);
    MOZ_ASSERT(offset);
    *isFixed = shape->slot() < shape->numFixedSlots();
    *offset = *isFixed ? NativeObject::getFixedSlotOffset(shape->slot())
                       : (shape->slot() - shape->numFixedSlots()) * sizeof(Value);
}

static JSObject*
GetDOMProxyProto(JSObject* obj)
{
    MOZ_ASSERT(IsCacheableDOMProxy(obj));
    return obj->getTaggedProto().toObjectOrNull();
}

// Callers are expected to have already guarded on the shape of the
// object, which guarantees the object is a DOM proxy.
static void
CheckDOMProxyExpandoDoesNotShadow(JSContext* cx, MacroAssembler& masm, Register object,
                                  const Address& checkExpandoShapeAddr,
                                  Address* expandoAndGenerationAddr,
                                  Address* generationAddr,
                                  Register scratch,
                                  AllocatableGeneralRegisterSet& domProxyRegSet,
                                  Label* checkFailed)
{
    // Guard that the object does not have expando properties, or has an expando
    // which is known to not have the desired property.

    // For the remaining code, we need to reserve some registers to load a value.
    // This is ugly, but unavoidable.
    ValueOperand tempVal = domProxyRegSet.takeAnyValue();
    masm.pushValue(tempVal);

    Label failDOMProxyCheck;
    Label domProxyOk;

    masm.loadPtr(Address(object, ProxyObject::offsetOfValues()), scratch);
    Address expandoAddr(scratch, ProxyObject::offsetOfExtraSlotInValues(GetDOMProxyExpandoSlot()));

    if (expandoAndGenerationAddr) {
        MOZ_ASSERT(generationAddr);

        masm.loadPtr(*expandoAndGenerationAddr, tempVal.scratchReg());
        masm.branchPrivatePtr(Assembler::NotEqual, expandoAddr, tempVal.scratchReg(),
                              &failDOMProxyCheck);

        masm.load32(*generationAddr, scratch);
        masm.branch32(Assembler::NotEqual,
                      Address(tempVal.scratchReg(), offsetof(ExpandoAndGeneration, generation)),
                      scratch, &failDOMProxyCheck);

        masm.loadValue(Address(tempVal.scratchReg(), 0), tempVal);
    } else {
        masm.loadValue(expandoAddr, tempVal);
    }

    // If the incoming object does not have an expando object then we're sure we're not
    // shadowing.
    masm.branchTestUndefined(Assembler::Equal, tempVal, &domProxyOk);

    // The reference object used to generate this check may not have had an
    // expando object at all, in which case the presence of a non-undefined
    // expando value in the incoming object is automatically a failure.
    masm.loadPtr(checkExpandoShapeAddr, scratch);
    masm.branchPtr(Assembler::Equal, scratch, ImmPtr(nullptr), &failDOMProxyCheck);

    // Otherwise, ensure that the incoming object has an object for its expando value and that
    // the shape matches.
    masm.branchTestObject(Assembler::NotEqual, tempVal, &failDOMProxyCheck);
    Register objReg = masm.extractObject(tempVal, tempVal.scratchReg());
    masm.branchTestObjShape(Assembler::Equal, objReg, scratch, &domProxyOk);

    // Failure case: restore the tempVal registers and jump to failures.
    masm.bind(&failDOMProxyCheck);
    masm.popValue(tempVal);
    masm.jump(checkFailed);

    // Success case: restore the tempval and proceed.
    masm.bind(&domProxyOk);
    masm.popValue(tempVal);
}

// Look up a property's shape on an object, being careful never to do any effectful
// operations.  This procedure not yielding a shape should not be taken as a lack of
// existence of the property on the object.
static bool
EffectlesslyLookupProperty(JSContext* cx, HandleObject obj, HandleId id,
                           MutableHandleObject holder, MutableHandleShape shape,
                           bool* checkDOMProxy=nullptr,
                           DOMProxyShadowsResult* shadowsResult=nullptr,
                           bool* domProxyHasGeneration=nullptr)
{
    shape.set(nullptr);
    holder.set(nullptr);

    if (checkDOMProxy) {
        *checkDOMProxy = false;
        *shadowsResult = ShadowCheckFailed;
    }

    // Check for list base if asked to.
    RootedObject checkObj(cx, obj);
    if (checkDOMProxy && IsCacheableDOMProxy(obj)) {
        MOZ_ASSERT(domProxyHasGeneration);
        MOZ_ASSERT(shadowsResult);

        *checkDOMProxy = true;
        if (obj->hasUncacheableProto())
            return true;

        *shadowsResult = GetDOMProxyShadowsCheck()(cx, obj, id);
        if (*shadowsResult == ShadowCheckFailed)
            return false;

        if (DOMProxyIsShadowing(*shadowsResult)) {
            holder.set(obj);
            return true;
        }

        *domProxyHasGeneration = (*shadowsResult == DoesntShadowUnique);

        checkObj = GetDOMProxyProto(obj);
        if (!checkObj)
            return true;
    }

    if (LookupPropertyPure(cx, checkObj, id, holder.address(), shape.address()))
        return true;

    holder.set(nullptr);
    shape.set(nullptr);
    return true;
}

static bool
CheckHasNoSuchProperty(JSContext* cx, HandleObject obj, HandlePropertyName name,
                       MutableHandleObject lastProto, size_t* protoChainDepthOut)
{
    MOZ_ASSERT(protoChainDepthOut != nullptr);

    size_t depth = 0;
    RootedObject curObj(cx, obj);
    while (curObj) {
        if (curObj->isNative()) {
            // Don't handle proto chains with resolve hooks.
            if (ClassMayResolveId(cx->names(), curObj->getClass(), NameToId(name), curObj))
                return false;
            if (curObj->as<NativeObject>().contains(cx, NameToId(name)))
                return false;
        } else if (curObj != obj) {
            // Non-native objects are only handled as the original receiver.
            return false;
        } else if (curObj->is<UnboxedPlainObject>()) {
            if (curObj->as<UnboxedPlainObject>().containsUnboxedOrExpandoProperty(cx, NameToId(name)))
                return false;
        } else if (curObj->is<UnboxedArrayObject>()) {
            if (name == cx->names().length)
                return false;
        } else if (curObj->is<TypedObject>()) {
            if (curObj->as<TypedObject>().typeDescr().hasProperty(cx->names(), NameToId(name)))
                return false;
        } else {
            return false;
        }

        JSObject* proto = curObj->getTaggedProto().toObjectOrNull();
        if (!proto)
            break;

        curObj = proto;
        depth++;
    }

    lastProto.set(curObj);
    *protoChainDepthOut = depth;
    return true;
}

static bool
IsCacheableProtoChain(JSObject* obj, JSObject* holder, bool isDOMProxy=false)
{
    MOZ_ASSERT_IF(isDOMProxy, IsCacheableDOMProxy(obj));

    if (!isDOMProxy && !obj->isNative()) {
        if (obj == holder)
            return false;
        if (!obj->is<UnboxedPlainObject>() &&
            !obj->is<UnboxedArrayObject>() &&
            !obj->is<TypedObject>())
        {
            return false;
        }
    }

    // Don't handle objects which require a prototype guard. This should
    // be uncommon so handling it is likely not worth the complexity.
    if (obj->hasUncacheableProto())
        return false;

    JSObject* cur = obj;
    while (cur != holder) {
        // We cannot assume that we find the holder object on the prototype
        // chain and must check for null proto. The prototype chain can be
        // altered during the lookupProperty call.
        JSObject* proto;
        if (isDOMProxy && cur == obj)
            proto = cur->getTaggedProto().toObjectOrNull();
        else
            proto = cur->getProto();

        if (!proto || !proto->isNative())
            return false;

        if (proto->hasUncacheableProto())
            return false;

        cur = proto;
    }
    return true;
}

static bool
IsCacheableGetPropReadSlot(JSObject* obj, JSObject* holder, Shape* shape, bool isDOMProxy=false)
{
    if (!shape || !IsCacheableProtoChain(obj, holder, isDOMProxy))
        return false;

    if (!shape->hasSlot() || !shape->hasDefaultGetter())
        return false;

    return true;
}

static bool
IsCacheableGetPropCall(JSContext* cx, JSObject* obj, JSObject* holder, Shape* shape,
                       bool* isScripted, bool* isTemporarilyUnoptimizable, bool isDOMProxy=false)
{
    MOZ_ASSERT(isScripted);

    if (!shape || !IsCacheableProtoChain(obj, holder, isDOMProxy))
        return false;

    if (shape->hasSlot() || shape->hasDefaultGetter())
        return false;

    if (!shape->hasGetterValue())
        return false;

    if (!shape->getterValue().isObject() || !shape->getterObject()->is<JSFunction>())
        return false;

    JSFunction* func = &shape->getterObject()->as<JSFunction>();
    if (func->isNative()) {
        *isScripted = false;
        return true;
    }

    if (!func->hasJITCode()) {
        *isTemporarilyUnoptimizable = true;
        return false;
    }

    *isScripted = true;
    return true;
}

static Shape*
LastPropertyForSetProp(JSObject* obj)
{
    if (obj->isNative())
        return obj->as<NativeObject>().lastProperty();

    if (obj->is<UnboxedPlainObject>()) {
        UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando();
        return expando ? expando->lastProperty() : nullptr;
    }

    return nullptr;
}

static bool
IsCacheableSetPropWriteSlot(JSObject* obj, Shape* oldShape, Shape* propertyShape)
{
    // Object shape must not have changed during the property set.
    if (LastPropertyForSetProp(obj) != oldShape)
        return false;

    if (!propertyShape->hasSlot() ||
        !propertyShape->hasDefaultSetter() ||
        !propertyShape->writable())
    {
        return false;
    }

    return true;
}

static bool
IsCacheableSetPropAddSlot(JSContext* cx, JSObject* obj, Shape* oldShape,
                          jsid id, Shape* propertyShape, size_t* protoChainDepth)
{
    // The property must be the last added property of the object.
    if (LastPropertyForSetProp(obj) != propertyShape)
        return false;

    // Object must be extensible, oldShape must be immediate parent of current shape.
    if (!obj->nonProxyIsExtensible() || propertyShape->previous() != oldShape)
        return false;

    // Basic shape checks.
    if (propertyShape->inDictionary() ||
        !propertyShape->hasSlot() ||
        !propertyShape->hasDefaultSetter() ||
        !propertyShape->writable())
    {
        return false;
    }

    // Watch out for resolve or addProperty hooks.
    if (ClassMayResolveId(cx->names(), obj->getClass(), id, obj) ||
        obj->getClass()->addProperty)
    {
        return false;
    }

    size_t chainDepth = 0;
    // Walk up the object prototype chain and ensure that all prototypes are
    // native, and that all prototypes have no setter defined on the property.
    for (JSObject* proto = obj->getProto(); proto; proto = proto->getProto()) {
        chainDepth++;
        // if prototype is non-native, don't optimize
        if (!proto->isNative())
            return false;

        // if prototype defines this property in a non-plain way, don't optimize
        Shape* protoShape = proto->as<NativeObject>().lookup(cx, id);
        if (protoShape && !protoShape->hasDefaultSetter())
            return false;

        // Otherwise, if there's no such property, watch out for a resolve hook
        // that would need to be invoked and thus prevent inlining of property
        // addition.
        if (ClassMayResolveId(cx->names(), proto->getClass(), id, proto))
            return false;
    }

    // Only add a IC entry if the dynamic slots didn't change when the shapes
    // changed.  Need to ensure that a shape change for a subsequent object
    // won't involve reallocating the slot array.
    if (NativeObject::dynamicSlotsCount(propertyShape) != NativeObject::dynamicSlotsCount(oldShape))
        return false;

    *protoChainDepth = chainDepth;
    return true;
}

static bool
IsCacheableSetPropCall(JSContext* cx, JSObject* obj, JSObject* holder, Shape* shape,
                       bool* isScripted, bool* isTemporarilyUnoptimizable)
{
    MOZ_ASSERT(isScripted);

    if (!shape || !IsCacheableProtoChain(obj, holder))
        return false;

    if (shape->hasSlot() || shape->hasDefaultSetter())
        return false;

    if (!shape->hasSetterValue())
        return false;

    if (!shape->setterValue().isObject() || !shape->setterObject()->is<JSFunction>())
        return false;

    JSFunction* func = &shape->setterObject()->as<JSFunction>();

    if (func->isNative()) {
        *isScripted = false;
        return true;
    }

    if (!func->hasJITCode()) {
        *isTemporarilyUnoptimizable = true;
        return false;
    }

    *isScripted = true;
    return true;
}

static bool
LookupNoSuchMethodHandler(JSContext* cx, HandleObject obj, HandleValue id,
                          MutableHandleValue result)
{
    return OnUnknownMethod(cx, obj, id, result);
}

typedef bool (*LookupNoSuchMethodHandlerFn)(JSContext*, HandleObject, HandleValue,
                                            MutableHandleValue);
static const VMFunction LookupNoSuchMethodHandlerInfo =
    FunctionInfo<LookupNoSuchMethodHandlerFn>(LookupNoSuchMethodHandler);


template <class T>
static bool
GetElemNativeStubExists(ICGetElem_Fallback* stub, HandleObject obj, HandleObject holder,
                        Handle<T> key, bool needsAtomize)
{
    bool indirect = (obj.get() != holder.get());
    MOZ_ASSERT_IF(indirect, holder->isNative());

    for (ICStubConstIterator iter = stub->beginChainConst(); !iter.atEnd(); iter++) {
        if (iter->kind() != ICStub::GetElem_NativeSlotName &&
            iter->kind() != ICStub::GetElem_NativeSlotSymbol &&
            iter->kind() != ICStub::GetElem_NativePrototypeSlotName &&
            iter->kind() != ICStub::GetElem_NativePrototypeSlotSymbol &&
            iter->kind() != ICStub::GetElem_NativePrototypeCallNativeName &&
            iter->kind() != ICStub::GetElem_NativePrototypeCallNativeSymbol &&
            iter->kind() != ICStub::GetElem_NativePrototypeCallScriptedName &&
            iter->kind() != ICStub::GetElem_NativePrototypeCallScriptedSymbol)
        {
            continue;
        }

        if (indirect && (iter->kind() != ICStub::GetElem_NativePrototypeSlotName &&
                         iter->kind() != ICStub::GetElem_NativePrototypeSlotSymbol &&
                         iter->kind() != ICStub::GetElem_NativePrototypeCallNativeName &&
                         iter->kind() != ICStub::GetElem_NativePrototypeCallNativeSymbol &&
                         iter->kind() != ICStub::GetElem_NativePrototypeCallScriptedName &&
                         iter->kind() != ICStub::GetElem_NativePrototypeCallScriptedSymbol))
        {
            continue;
        }

        if(mozilla::IsSame<T, JS::Symbol*>::value !=
           static_cast<ICGetElemNativeStub*>(*iter)->isSymbol())
        {
            continue;
        }

        ICGetElemNativeStubImpl<T>* getElemNativeStub =
            reinterpret_cast<ICGetElemNativeStubImpl<T>*>(*iter);
        if (key != getElemNativeStub->key())
            continue;

        if (ReceiverGuard(obj) != getElemNativeStub->receiverGuard())
            continue;

        // If the new stub needs atomization, and the old stub doesn't atomize, then
        // an appropriate stub doesn't exist.
        if (needsAtomize && !getElemNativeStub->needsAtomize())
            continue;

        // For prototype gets, check the holder and holder shape.
        if (indirect) {
            if (iter->isGetElem_NativePrototypeSlotName() ||
                iter->isGetElem_NativePrototypeSlotSymbol()) {
                ICGetElem_NativePrototypeSlot<T>* protoStub =
                    reinterpret_cast<ICGetElem_NativePrototypeSlot<T>*>(*iter);

                if (holder != protoStub->holder())
                    continue;

                if (holder->as<NativeObject>().lastProperty() != protoStub->holderShape())
                    continue;
            } else {
                MOZ_ASSERT(iter->isGetElem_NativePrototypeCallNativeName() ||
                           iter->isGetElem_NativePrototypeCallNativeSymbol() ||
                           iter->isGetElem_NativePrototypeCallScriptedName() ||
                           iter->isGetElem_NativePrototypeCallScriptedSymbol());

                ICGetElemNativePrototypeCallStub<T>* protoStub =
                    reinterpret_cast<ICGetElemNativePrototypeCallStub<T>*>(*iter);

                if (holder != protoStub->holder())
                    continue;

                if (holder->as<NativeObject>().lastProperty() != protoStub->holderShape())
                    continue;
            }
        }

        return true;
    }
    return false;
}

template <class T>
static void
RemoveExistingGetElemNativeStubs(JSContext* cx, ICGetElem_Fallback* stub, HandleObject obj,
                                 HandleObject holder, Handle<T> key, bool needsAtomize)
{
    bool indirect = (obj.get() != holder.get());
    MOZ_ASSERT_IF(indirect, holder->isNative());

    for (ICStubIterator iter = stub->beginChain(); !iter.atEnd(); iter++) {
        switch (iter->kind()) {
          case ICStub::GetElem_NativeSlotName:
          case ICStub::GetElem_NativeSlotSymbol:
            if (indirect)
                continue;
          case ICStub::GetElem_NativePrototypeSlotName:
          case ICStub::GetElem_NativePrototypeSlotSymbol:
          case ICStub::GetElem_NativePrototypeCallNativeName:
          case ICStub::GetElem_NativePrototypeCallNativeSymbol:
          case ICStub::GetElem_NativePrototypeCallScriptedName:
          case ICStub::GetElem_NativePrototypeCallScriptedSymbol:
            break;
          default:
            continue;
        }

        if(mozilla::IsSame<T, JS::Symbol*>::value !=
           static_cast<ICGetElemNativeStub*>(*iter)->isSymbol())
        {
            continue;
        }

        ICGetElemNativeStubImpl<T>* getElemNativeStub =
            reinterpret_cast<ICGetElemNativeStubImpl<T>*>(*iter);
        if (key != getElemNativeStub->key())
            continue;

        if (ReceiverGuard(obj) != getElemNativeStub->receiverGuard())
            continue;

        // For prototype gets, check the holder and holder shape.
        if (indirect) {
            if (iter->isGetElem_NativePrototypeSlotName() ||
                iter->isGetElem_NativePrototypeSlotSymbol()) {
                ICGetElem_NativePrototypeSlot<T>* protoStub =
                    reinterpret_cast<ICGetElem_NativePrototypeSlot<T>*>(*iter);

                if (holder != protoStub->holder())
                    continue;

                // If the holder matches, but the holder's lastProperty doesn't match, then
                // this stub is invalid anyway.  Unlink it.
                if (holder->as<NativeObject>().lastProperty() != protoStub->holderShape()) {
                    iter.unlink(cx);
                    continue;
                }
            } else {
                MOZ_ASSERT(iter->isGetElem_NativePrototypeCallNativeName() ||
                           iter->isGetElem_NativePrototypeCallNativeSymbol() ||
                           iter->isGetElem_NativePrototypeCallScriptedName() ||
                           iter->isGetElem_NativePrototypeCallScriptedSymbol());
                ICGetElemNativePrototypeCallStub<T>* protoStub =
                    reinterpret_cast<ICGetElemNativePrototypeCallStub<T>*>(*iter);

                if (holder != protoStub->holder())
                    continue;

                // If the holder matches, but the holder's lastProperty doesn't match, then
                // this stub is invalid anyway.  Unlink it.
                if (holder->as<NativeObject>().lastProperty() != protoStub->holderShape()) {
                    iter.unlink(cx);
                    continue;
                }
            }
        }

        // If the new stub needs atomization, and the old stub doesn't atomize, then
        // remove the old stub.
        if (needsAtomize && !getElemNativeStub->needsAtomize()) {
            iter.unlink(cx);
            continue;
        }

        // Should never get here, because this means a matching stub exists, and if
        // a matching stub exists, this procedure should never have been called.
        MOZ_CRASH("Procedure should never have been called.");
    }
}

static bool
TypedArrayGetElemStubExists(ICGetElem_Fallback* stub, HandleObject obj)
{
    for (ICStubConstIterator iter = stub->beginChainConst(); !iter.atEnd(); iter++) {
        if (!iter->isGetElem_TypedArray())
            continue;
        if (obj->maybeShape() == iter->toGetElem_TypedArray()->shape())
            return true;
    }
    return false;
}

static bool
ArgumentsGetElemStubExists(ICGetElem_Fallback* stub, ICGetElem_Arguments::Which which)
{
    for (ICStubConstIterator iter = stub->beginChainConst(); !iter.atEnd(); iter++) {
        if (!iter->isGetElem_Arguments())
            continue;
        if (iter->toGetElem_Arguments()->which() == which)
            return true;
    }
    return false;
}

template <class T>
static T
getKey(jsid id)
{
    MOZ_ASSERT_UNREACHABLE("Key has to be PropertyName or Symbol");
    return false;
}

template <>
JS::Symbol* getKey<JS::Symbol*>(jsid id)
{
    if (!JSID_IS_SYMBOL(id))
        return nullptr;
    return JSID_TO_SYMBOL(id);
}

template <>
PropertyName* getKey<PropertyName*>(jsid id)
{
    uint32_t dummy;
    if (!JSID_IS_ATOM(id) || JSID_TO_ATOM(id)->isIndex(&dummy))
        return nullptr;
    return JSID_TO_ATOM(id)->asPropertyName();
}

static bool
IsOptimizableElementPropertyName(JSContext* cx, HandleValue key, MutableHandleId idp)
{
    if (!key.isString())
        return false;

    // Convert to interned property name.
    if (!ValueToId<CanGC>(cx, key, idp))
        return false;

    uint32_t dummy;
    if (!JSID_IS_ATOM(idp) || JSID_TO_ATOM(idp)->isIndex(&dummy))
        return false;

    return true;
}

template <class T>
static bool
checkAtomize(HandleValue key)
{
    MOZ_ASSERT_UNREACHABLE("Key has to be PropertyName or Symbol");
    return false;
}

template <>
bool checkAtomize<JS::Symbol*>(HandleValue key)
{
    return false;
}

template <>
bool checkAtomize<PropertyName*>(HandleValue key)
{
    return !key.toString()->isAtom();
}

template <class T>
static bool
TryAttachNativeOrUnboxedGetValueElemStub(JSContext* cx, HandleScript script, jsbytecode* pc,
                                         ICGetElem_Fallback* stub, HandleObject obj,
                                         HandleValue keyVal, bool* attached)
{
    MOZ_ASSERT(keyVal.isString() || keyVal.isSymbol());

    // Convert to id.
    RootedId id(cx);
    if (!ValueToId<CanGC>(cx, keyVal, &id))
        return false;

    Rooted<T> key(cx, getKey<T>(id));
    if (!key)
        return true;
    bool needsAtomize = checkAtomize<T>(keyVal);
    bool isCallElem = (JSOp(*pc) == JSOP_CALLELEM);

    RootedShape shape(cx);
    RootedObject holder(cx);
    if (!EffectlesslyLookupProperty(cx, obj, id, &holder, &shape))
        return false;
    if (!holder || (holder != obj && !holder->isNative()))
        return true;

    // If a suitable stub already exists, nothing else to do.
    if (GetElemNativeStubExists<T>(stub, obj, holder, key, needsAtomize))
        return true;

    // Remove any existing stubs that may interfere with the new stub being added.
    RemoveExistingGetElemNativeStubs<T>(cx, stub, obj, holder, key, needsAtomize);

    ICStub* monitorStub = stub->fallbackMonitorStub()->firstMonitorStub();

    if (obj->is<UnboxedPlainObject>() && holder == obj) {
        const UnboxedLayout::Property* property = obj->as<UnboxedPlainObject>().layout().lookup(id);

        // Once unboxed objects support symbol-keys, we need to change the following accordingly
        MOZ_ASSERT_IF(!keyVal.isString(), !property);

        if (property) {
            if (!cx->runtime()->jitSupportsFloatingPoint)
                return true;

            RootedPropertyName name(cx, JSID_TO_ATOM(id)->asPropertyName());
            ICGetElemNativeCompiler<PropertyName*> compiler(cx, ICStub::GetElem_UnboxedPropertyName,
                                                            isCallElem, monitorStub, obj, holder,
                                                            name,
                                                            ICGetElemNativeStub::UnboxedProperty,
                                                            needsAtomize, property->offset +
                                                            UnboxedPlainObject::offsetOfData(),
                                                            property->type);
            ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
            if (!newStub)
                return false;

            stub->addNewStub(newStub);
            *attached = true;
            return true;
        }

        Shape* shape = obj->as<UnboxedPlainObject>().maybeExpando()->lookup(cx, id);
        if (!shape->hasDefaultGetter() || !shape->hasSlot())
            return true;

        bool isFixedSlot;
        uint32_t offset;
        GetFixedOrDynamicSlotOffset(shape, &isFixedSlot, &offset);

        ICGetElemNativeStub::AccessType acctype =
            isFixedSlot ? ICGetElemNativeStub::FixedSlot
                        : ICGetElemNativeStub::DynamicSlot;
        ICGetElemNativeCompiler<T> compiler(cx, getGetElemStubKind<T>(ICStub::GetElem_NativeSlotName),
                                            isCallElem, monitorStub, obj, holder, key,
                                            acctype, needsAtomize, offset);
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;

        stub->addNewStub(newStub);
        *attached = true;
        return true;
    }

    if (!holder->isNative())
        return true;

    if (IsCacheableGetPropReadSlot(obj, holder, shape)) {
        bool isFixedSlot;
        uint32_t offset;
        GetFixedOrDynamicSlotOffset(shape, &isFixedSlot, &offset);

        ICStub::Kind kind = (obj == holder) ? ICStub::GetElem_NativeSlotName
                                            : ICStub::GetElem_NativePrototypeSlotName;
        kind = getGetElemStubKind<T>(kind);

        JitSpew(JitSpew_BaselineIC, "  Generating GetElem(Native %s%s slot) stub "
                                    "(obj=%p, holder=%p, holderShape=%p)",
                    (obj == holder) ? "direct" : "prototype",
                    needsAtomize ? " atomizing" : "",
                obj.get(), holder.get(), holder->as<NativeObject>().lastProperty());

        AccType acctype = isFixedSlot ? ICGetElemNativeStub::FixedSlot
                                      : ICGetElemNativeStub::DynamicSlot;
        ICGetElemNativeCompiler<T> compiler(cx, kind, isCallElem, monitorStub, obj, holder, key,
                                            acctype, needsAtomize, offset);
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;

        stub->addNewStub(newStub);
        *attached = true;
        return true;
    }

    return true;
}

template <class T>
static bool
TryAttachNativeGetAccessorElemStub(JSContext* cx, HandleScript script, jsbytecode* pc,
                                   ICGetElem_Fallback* stub, HandleNativeObject obj,
                                   HandleValue keyVal, bool* attached,
                                   bool* isTemporarilyUnoptimizable)
{
    MOZ_ASSERT(!*attached);
    MOZ_ASSERT(keyVal.isString() || keyVal.isSymbol());

    RootedId id(cx);
    if (!ValueToId<CanGC>(cx, keyVal, &id))
        return false;

    Rooted<T> key(cx, getKey<T>(id));
    if (!key)
        return true;
    bool needsAtomize = checkAtomize<T>(keyVal);
    bool isCallElem = (JSOp(*pc) == JSOP_CALLELEM);

    RootedShape shape(cx);
    RootedObject baseHolder(cx);
    if (!EffectlesslyLookupProperty(cx, obj, id, &baseHolder, &shape))
        return false;
    if (!baseHolder || baseHolder->isNative())
        return true;

    HandleNativeObject holder = baseHolder.as<NativeObject>();

    bool getterIsScripted = false;
    if (IsCacheableGetPropCall(cx, obj, baseHolder, shape, &getterIsScripted,
                               isTemporarilyUnoptimizable, /*isDOMProxy=*/false))
    {
        RootedFunction getter(cx, &shape->getterObject()->as<JSFunction>());

#if JS_HAS_NO_SUCH_METHOD
        // It's unlikely that a getter function will be used in callelem locations.
        // Just don't attach stubs in that case to avoid issues with __noSuchMethod__ handling.
        if (isCallElem)
            return true;
#endif

        // For now, we do not handle own property getters
        if (obj == holder)
            return true;

        // If a suitable stub already exists, nothing else to do.
        if (GetElemNativeStubExists<T>(stub, obj, holder, key, needsAtomize))
            return true;

        // Remove any existing stubs that may interfere with the new stub being added.
        RemoveExistingGetElemNativeStubs<T>(cx, stub, obj, holder, key, needsAtomize);

        ICStub* monitorStub = stub->fallbackMonitorStub()->firstMonitorStub();
        ICStub::Kind kind = getterIsScripted ? ICStub::GetElem_NativePrototypeCallScriptedName
                                             : ICStub::GetElem_NativePrototypeCallNativeName;
        kind = getGetElemStubKind<T>(kind);

        if (getterIsScripted) {
            JitSpew(JitSpew_BaselineIC,
                    "  Generating GetElem(Native %s%s call scripted %s:%" PRIuSIZE ") stub "
                    "(obj=%p, shape=%p, holder=%p, holderShape=%p)",
                        (obj == holder) ? "direct" : "prototype",
                        needsAtomize ? " atomizing" : "",
                        getter->nonLazyScript()->filename(), getter->nonLazyScript()->lineno(),
                        obj.get(), obj->lastProperty(), holder.get(), holder->lastProperty());
        } else {
            JitSpew(JitSpew_BaselineIC,
                    "  Generating GetElem(Native %s%s call native) stub "
                    "(obj=%p, shape=%p, holder=%p, holderShape=%p)",
                        (obj == holder) ? "direct" : "prototype",
                        needsAtomize ? " atomizing" : "",
                        obj.get(), obj->lastProperty(), holder.get(), holder->lastProperty());
        }

        AccType acctype = getterIsScripted ? ICGetElemNativeStub::ScriptedGetter
                                           : ICGetElemNativeStub::NativeGetter;
        ICGetElemNativeCompiler<T> compiler(cx, kind, monitorStub, obj, holder, key, acctype,
                                         needsAtomize, getter, script->pcToOffset(pc), isCallElem);
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;

        stub->addNewStub(newStub);
        *attached = true;
        return true;
    }

    return true;
}

static bool
IsPrimitiveArrayTypedObject(JSObject* obj)
{
    if (!obj->is<TypedObject>())
        return false;
    TypeDescr& descr = obj->as<TypedObject>().typeDescr();
    return descr.is<ArrayTypeDescr>() &&
           descr.as<ArrayTypeDescr>().elementType().is<ScalarTypeDescr>();
}

static Scalar::Type
PrimitiveArrayTypedObjectType(JSObject* obj)
{
    MOZ_ASSERT(IsPrimitiveArrayTypedObject(obj));
    TypeDescr& descr = obj->as<TypedObject>().typeDescr();
    return descr.as<ArrayTypeDescr>().elementType().as<ScalarTypeDescr>().type();
}

static Scalar::Type
TypedThingElementType(JSObject* obj)
{
    return IsAnyTypedArray(obj)
           ? AnyTypedArrayType(obj)
           : PrimitiveArrayTypedObjectType(obj);
}

static bool
TypedThingRequiresFloatingPoint(JSObject* obj)
{
    Scalar::Type type = TypedThingElementType(obj);
    return type == Scalar::Uint32 ||
           type == Scalar::Float32 ||
           type == Scalar::Float64;
}

static bool
IsNativeDenseElementAccess(HandleObject obj, HandleValue key)
{
    if (obj->isNative() && key.isInt32() && key.toInt32() >= 0 && !IsAnyTypedArray(obj.get()))
        return true;
    return false;
}

static bool
IsNativeOrUnboxedDenseElementAccess(HandleObject obj, HandleValue key)
{
    if (!obj->isNative() && !obj->is<UnboxedArrayObject>())
        return false;
    if (key.isInt32() && key.toInt32() >= 0 && !IsAnyTypedArray(obj.get()))
        return true;
    return false;
}

static bool
TryAttachGetElemStub(JSContext* cx, JSScript* script, jsbytecode* pc, ICGetElem_Fallback* stub,
                     HandleValue lhs, HandleValue rhs, HandleValue res, bool* attached)
{
    bool isCallElem = (JSOp(*pc) == JSOP_CALLELEM);

    // Check for String[i] => Char accesses.
    if (lhs.isString() && rhs.isInt32() && res.isString() &&
        !stub->hasStub(ICStub::GetElem_String))
    {
        // NoSuchMethod handling doesn't apply to string targets.

        JitSpew(JitSpew_BaselineIC, "  Generating GetElem(String[Int32]) stub");
        ICGetElem_String::Compiler compiler(cx);
        ICStub* stringStub = compiler.getStub(compiler.getStubSpace(script));
        if (!stringStub)
            return false;

        stub->addNewStub(stringStub);
        *attached = true;
        return true;
    }

    if (lhs.isMagic(JS_OPTIMIZED_ARGUMENTS) && rhs.isInt32() &&
        !ArgumentsGetElemStubExists(stub, ICGetElem_Arguments::Magic))
    {
        // Any script with a CALLPROP on arguments (arguments.foo())
        // should not have optimized arguments.
        MOZ_ASSERT(!isCallElem);

        JitSpew(JitSpew_BaselineIC, "  Generating GetElem(MagicArgs[Int32]) stub");
        ICGetElem_Arguments::Compiler compiler(cx, stub->fallbackMonitorStub()->firstMonitorStub(),
                                               ICGetElem_Arguments::Magic, false);
        ICStub* argsStub = compiler.getStub(compiler.getStubSpace(script));
        if (!argsStub)
            return false;

        stub->addNewStub(argsStub);
        *attached = true;
        return true;
    }

    // Otherwise, GetElem is only optimized on objects.
    if (!lhs.isObject())
        return true;
    RootedObject obj(cx, &lhs.toObject());

    // Check for ArgumentsObj[int] accesses
    if (obj->is<ArgumentsObject>() && rhs.isInt32()) {
        ICGetElem_Arguments::Which which = ICGetElem_Arguments::Mapped;
        if (obj->is<UnmappedArgumentsObject>())
            which = ICGetElem_Arguments::Unmapped;
        if (!ArgumentsGetElemStubExists(stub, which)) {
            JitSpew(JitSpew_BaselineIC, "  Generating GetElem(ArgsObj[Int32]) stub");
            ICGetElem_Arguments::Compiler compiler(
                cx, stub->fallbackMonitorStub()->firstMonitorStub(), which, isCallElem);
            ICStub* argsStub = compiler.getStub(compiler.getStubSpace(script));
            if (!argsStub)
                return false;

            stub->addNewStub(argsStub);
            *attached = true;
            return true;
        }
    }

    // Check for NativeObject[int] dense accesses.
    if (IsNativeDenseElementAccess(obj, rhs)) {
        JitSpew(JitSpew_BaselineIC, "  Generating GetElem(Native[Int32] dense) stub");
        ICGetElem_Dense::Compiler compiler(cx, stub->fallbackMonitorStub()->firstMonitorStub(),
                                           obj->as<NativeObject>().lastProperty(), isCallElem);
        ICStub* denseStub = compiler.getStub(compiler.getStubSpace(script));
        if (!denseStub)
            return false;

        stub->addNewStub(denseStub);
        *attached = true;
        return true;
    }

    // Check for NativeObject[id] and UnboxedPlainObject[id] shape-optimizable accesses.
    if (obj->isNative() || obj->is<UnboxedPlainObject>()) {
        RootedScript rootedScript(cx, script);
        if (rhs.isString()) {
            if (!TryAttachNativeOrUnboxedGetValueElemStub<PropertyName*>(cx, rootedScript, pc, stub,
                                                                         obj, rhs, attached))
            {
                return false;
            }
        } else if (rhs.isSymbol()) {
            if (!TryAttachNativeOrUnboxedGetValueElemStub<JS::Symbol*>(cx, rootedScript, pc, stub,
                                                                       obj, rhs, attached))
            {
                return false;
            }
        }
        if (*attached)
            return true;
        script = rootedScript;
    }

    // Check for UnboxedArray[int] accesses.
    if (obj->is<UnboxedArrayObject>() && rhs.isInt32() && rhs.toInt32() >= 0) {
        JitSpew(JitSpew_BaselineIC, "  Generating GetElem(UnboxedArray[Int32]) stub");
        ICGetElem_UnboxedArray::Compiler compiler(cx, stub->fallbackMonitorStub()->firstMonitorStub(),
                                                  obj->group());
        ICStub* unboxedStub = compiler.getStub(compiler.getStubSpace(script));
        if (!unboxedStub)
            return false;

        stub->addNewStub(unboxedStub);
        *attached = true;
        return true;
    }

    // Check for TypedArray[int] => Number and TypedObject[int] => Number accesses.
    if ((IsAnyTypedArray(obj.get()) || IsPrimitiveArrayTypedObject(obj)) &&
        rhs.isNumber() &&
        res.isNumber() &&
        !TypedArrayGetElemStubExists(stub, obj))
    {
        // Don't attach CALLELEM stubs for accesses on typed array expected to yield numbers.
#if JS_HAS_NO_SUCH_METHOD
        if (isCallElem)
            return true;
#endif

        if (!cx->runtime()->jitSupportsFloatingPoint &&
            (TypedThingRequiresFloatingPoint(obj) || rhs.isDouble()))
        {
            return true;
        }

        // Don't attach typed object stubs if they might be neutered, as the
        // stub will always bail out.
        if (IsPrimitiveArrayTypedObject(obj) && cx->compartment()->neuteredTypedObjects)
            return true;

        JitSpew(JitSpew_BaselineIC, "  Generating GetElem(TypedArray[Int32]) stub");
        ICGetElem_TypedArray::Compiler compiler(cx, obj->maybeShape(), TypedThingElementType(obj));
        ICStub* typedArrayStub = compiler.getStub(compiler.getStubSpace(script));
        if (!typedArrayStub)
            return false;

        stub->addNewStub(typedArrayStub);
        *attached = true;
        return true;
    }

    // GetElem operations on non-native objects cannot be cached by either
    // Baseline or Ion. Indicate this in the cache so that Ion does not
    // generate a cache for this op.
    if (!obj->isNative())
        stub->noteNonNativeAccess();

    // GetElem operations which could access negative indexes generally can't
    // be optimized without the potential for bailouts, as we can't statically
    // determine that an object has no properties on such indexes.
    if (rhs.isNumber() && rhs.toNumber() < 0)
        stub->noteNegativeIndex();

    return true;
}

static bool
DoGetElemFallback(JSContext* cx, BaselineFrame* frame, ICGetElem_Fallback* stub_, HandleValue lhs,
                  HandleValue rhs, MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICGetElem_Fallback*> stub(frame, stub_);

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(frame->script());
    JSOp op = JSOp(*pc);
    FallbackICSpew(cx, stub, "GetElem(%s)", js_CodeName[op]);

    MOZ_ASSERT(op == JSOP_GETELEM || op == JSOP_CALLELEM);

    // Don't pass lhs directly, we need it when generating stubs.
    RootedValue lhsCopy(cx, lhs);

    bool isOptimizedArgs = false;
    if (lhs.isMagic(JS_OPTIMIZED_ARGUMENTS)) {
        // Handle optimized arguments[i] access.
        if (!GetElemOptimizedArguments(cx, frame, &lhsCopy, rhs, res, &isOptimizedArgs))
            return false;
        if (isOptimizedArgs)
            TypeScript::Monitor(cx, frame->script(), pc, res);
    }

    bool attached = false;
    if (stub->numOptimizedStubs() >= ICGetElem_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard all stubs in this IC and replace with inert megamorphic stub.
        // But for now we just bail.
        stub->noteUnoptimizableAccess();
        attached = true;
    }

    // Try to attach an optimized getter stub.
    bool isTemporarilyUnoptimizable = false;
    if (!attached && lhs.isObject() && lhs.toObject().isNative()){
        if (rhs.isString()) {
            RootedScript rootedScript(cx, frame->script());
            RootedNativeObject obj(cx, &lhs.toObject().as<NativeObject>());
            if (!TryAttachNativeGetAccessorElemStub<PropertyName*>(cx, rootedScript, pc, stub,
                                                                   obj, rhs, &attached,
                                                                   &isTemporarilyUnoptimizable))
            {
                return false;
            }
            script = rootedScript;
        } else if (rhs.isSymbol()) {
            RootedScript rootedScript(cx, frame->script());
            RootedNativeObject obj(cx, &lhs.toObject().as<NativeObject>());
            if (!TryAttachNativeGetAccessorElemStub<JS::Symbol*>(cx, rootedScript, pc, stub,
                                                                 obj, rhs, &attached,
                                                                 &isTemporarilyUnoptimizable))
            {
                return false;
            }
            script = rootedScript;
        }
    }

    if (!isOptimizedArgs) {
        if (!GetElementOperation(cx, op, &lhsCopy, rhs, res))
            return false;
        TypeScript::Monitor(cx, frame->script(), pc, res);
    }

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid())
        return true;

    // Add a type monitor stub for the resulting value.
    if (!stub->addMonitorStubForValue(cx, frame->script(), res))
        return false;

    if (attached)
        return true;

    // Try to attach an optimized stub.
    if (!TryAttachGetElemStub(cx, frame->script(), pc, stub, lhs, rhs, res, &attached))
        return false;

    if (!attached && !isTemporarilyUnoptimizable)
        stub->noteUnoptimizableAccess();

    return true;
}

typedef bool (*DoGetElemFallbackFn)(JSContext*, BaselineFrame*, ICGetElem_Fallback*,
                                    HandleValue, HandleValue, MutableHandleValue);
static const VMFunction DoGetElemFallbackInfo =
    FunctionInfo<DoGetElemFallbackFn>(DoGetElemFallback, TailCall, PopValues(2));

bool
ICGetElem_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);
    MOZ_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    // Ensure stack is fully synced for the expression decompiler.
    masm.pushValue(R0);
    masm.pushValue(R1);

    // Push arguments.
    masm.pushValue(R1);
    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushFramePtr(masm, R0.scratchReg());

    return tailCallVM(DoGetElemFallbackInfo, masm);
}

//
// GetElem_NativeSlot
//

static bool
DoAtomizeString(JSContext* cx, HandleString string, MutableHandleValue result)
{
    JitSpew(JitSpew_BaselineIC, "  AtomizeString called");

    RootedValue key(cx, StringValue(string));

    // Convert to interned property name.
    RootedId id(cx);
    if (!ValueToId<CanGC>(cx, key, &id))
        return false;

    if (!JSID_IS_ATOM(id)) {
        result.set(key);
        return true;
    }

    result.set(StringValue(JSID_TO_ATOM(id)));
    return true;
}

typedef bool (*DoAtomizeStringFn)(JSContext*, HandleString, MutableHandleValue);
static const VMFunction DoAtomizeStringInfo = FunctionInfo<DoAtomizeStringFn>(DoAtomizeString);

template <class T>
bool
ICGetElemNativeCompiler<T>::emitCallNative(MacroAssembler& masm, Register objReg)
{
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));
    regs.takeUnchecked(objReg);
    regs.takeUnchecked(ICTailCallReg);

    enterStubFrame(masm, regs.getAny());

    // Push object.
    masm.push(objReg);

    // Push native callee.
    masm.loadPtr(Address(ICStubReg, ICGetElemNativeGetterStub<T>::offsetOfGetter()), objReg);
    masm.push(objReg);

    regs.add(objReg);

    // Call helper.
    if (!callVM(DoCallNativeGetterInfo, masm))
        return false;

    leaveStubFrame(masm);

    return true;
}

template <class T>
bool
ICGetElemNativeCompiler<T>::emitCallScripted(MacroAssembler& masm, Register objReg)
{
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));
    regs.takeUnchecked(objReg);
    regs.takeUnchecked(ICTailCallReg);

    // Enter stub frame.
    enterStubFrame(masm, regs.getAny());

    // Align the stack such that the JitFrameLayout is aligned on
    // JitStackAlignment.
    masm.alignJitStackBasedOnNArgs(0);

    // Push |this| for getter (target object).
    {
        ValueOperand val = regs.takeAnyValue();
        masm.tagValue(JSVAL_TYPE_OBJECT, objReg, val);
        masm.Push(val);
        regs.add(val);
    }

    regs.add(objReg);

    Register callee = regs.takeAny();
    masm.loadPtr(Address(ICStubReg, ICGetElemNativeGetterStub<T>::offsetOfGetter()), callee);

    // Push argc, callee, and descriptor.
    {
        Register callScratch = regs.takeAny();
        EmitBaselineCreateStubFrameDescriptor(masm, callScratch);
        masm.Push(Imm32(0));  // ActualArgc is 0
        masm.Push(callee);
        masm.Push(callScratch);
        regs.add(callScratch);
    }

    Register code = regs.takeAnyExcluding(ArgumentsRectifierReg);
    masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrScript()), code);
    masm.loadBaselineOrIonRaw(code, code, nullptr);

    Register scratch = regs.takeAny();

    // Handle arguments underflow.
    Label noUnderflow;
    masm.load16ZeroExtend(Address(callee, JSFunction::offsetOfNargs()), scratch);
    masm.branch32(Assembler::Equal, scratch, Imm32(0), &noUnderflow);
    {
        // Call the arguments rectifier.
        MOZ_ASSERT(ArgumentsRectifierReg != code);

        JitCode* argumentsRectifier =
            cx->runtime()->jitRuntime()->getArgumentsRectifier();

        masm.movePtr(ImmGCPtr(argumentsRectifier), code);
        masm.loadPtr(Address(code, JitCode::offsetOfCode()), code);
        masm.movePtr(ImmWord(0), ArgumentsRectifierReg);
    }

    masm.bind(&noUnderflow);
    masm.callJit(code);

    leaveStubFrame(masm, true);

    return true;
}

template <class T>
bool
ICGetElemNativeCompiler<T>::emitCheckKey(MacroAssembler& masm, Label& failure)
{
    MOZ_ASSERT_UNREACHABLE("Key has to be PropertyName or Symbol");
    return false;
}

template <>
bool
ICGetElemNativeCompiler<JS::Symbol*>::emitCheckKey(MacroAssembler& masm, Label& failure)
{
    MOZ_ASSERT(!needsAtomize_);
    masm.branchTestSymbol(Assembler::NotEqual, R1, &failure);
    Address symbolAddr(ICStubReg, ICGetElemNativeStubImpl<JS::Symbol*>::offsetOfKey());
    Register symExtract = masm.extractObject(R1, ExtractTemp1);
    masm.branchPtr(Assembler::NotEqual, symbolAddr, symExtract, &failure);
    return true;
}

template <>
bool
ICGetElemNativeCompiler<PropertyName*>::emitCheckKey(MacroAssembler& masm, Label& failure)
{
    masm.branchTestString(Assembler::NotEqual, R1, &failure);
    // Check key identity.  Don't automatically fail if this fails, since the incoming
    // key maybe a non-interned string.  Switch to a slowpath vm-call based check.
    Address nameAddr(ICStubReg, ICGetElemNativeStubImpl<PropertyName*>::offsetOfKey());
    Register strExtract = masm.extractString(R1, ExtractTemp1);

    // If needsAtomize_ is true, and the string is not already an atom, then atomize the
    // string before proceeding.
    if (needsAtomize_) {
        Label skipAtomize;

        // If string is already an atom, skip the atomize.
        masm.branchTest32(Assembler::NonZero,
                          Address(strExtract, JSString::offsetOfFlags()),
                          Imm32(JSString::ATOM_BIT),
                          &skipAtomize);

        // Stow R0.
        EmitStowICValues(masm, 1);

        enterStubFrame(masm, R0.scratchReg());

        // Atomize the string into a new value.
        masm.push(strExtract);
        if (!callVM(DoAtomizeStringInfo, masm))
            return false;

        // Atomized string is now in JSReturnOperand (R0).
        // Leave stub frame, move atomized string into R1.
        MOZ_ASSERT(R0 == JSReturnOperand);
        leaveStubFrame(masm);
        masm.moveValue(JSReturnOperand, R1);

        // Unstow R0
        EmitUnstowICValues(masm, 1);

        // Extract string from R1 again.
        DebugOnly<Register> strExtract2 = masm.extractString(R1, ExtractTemp1);
        MOZ_ASSERT(Register(strExtract2) == strExtract);

        masm.bind(&skipAtomize);
    }

    // Key has been atomized if necessary.  Do identity check on string pointer.
    masm.branchPtr(Assembler::NotEqual, nameAddr, strExtract, &failure);
    return true;
}

template <class T>
bool
ICGetElemNativeCompiler<T>::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    Label failurePopR1;
    bool popR1 = false;

    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratchReg = regs.takeAny();

    // Unbox object.
    Register objReg = masm.extractObject(R0, ExtractTemp0);

    // Check object shape/group.
    GuardReceiverObject(masm, ReceiverGuard(obj_), objReg, scratchReg,
                        ICGetElemNativeStub::offsetOfReceiverGuard(), &failure);

    // Since this stub sometimes enters a stub frame, we manually set this to true (lie).
#ifdef DEBUG
    entersStubFrame_ = true;
#endif

    if (!emitCheckKey(masm, failure))
        return false;

    Register holderReg;
    if (obj_ == holder_) {
        holderReg = objReg;

        if (obj_->is<UnboxedPlainObject>() && acctype_ != ICGetElemNativeStub::UnboxedProperty) {
            // The property will be loaded off the unboxed expando.
            masm.push(R1.scratchReg());
            popR1 = true;
            holderReg = R1.scratchReg();
            masm.loadPtr(Address(objReg, UnboxedPlainObject::offsetOfExpando()), holderReg);
        }
    } else {
        // Shape guard holder.
        if (regs.empty()) {
            masm.push(R1.scratchReg());
            popR1 = true;
            holderReg = R1.scratchReg();
        } else {
            holderReg = regs.takeAny();
        }

        if (kind == ICStub::GetElem_NativePrototypeCallNativeName ||
            kind == ICStub::GetElem_NativePrototypeCallNativeSymbol ||
            kind == ICStub::GetElem_NativePrototypeCallScriptedName ||
            kind == ICStub::GetElem_NativePrototypeCallScriptedSymbol)
        {
            masm.loadPtr(Address(ICStubReg,
                                 ICGetElemNativePrototypeCallStub<T>::offsetOfHolder()),
                         holderReg);
            masm.loadPtr(Address(ICStubReg,
                                 ICGetElemNativePrototypeCallStub<T>::offsetOfHolderShape()),
                         scratchReg);
        } else {
            masm.loadPtr(Address(ICStubReg,
                                 ICGetElem_NativePrototypeSlot<T>::offsetOfHolder()),
                         holderReg);
            masm.loadPtr(Address(ICStubReg,
                                 ICGetElem_NativePrototypeSlot<T>::offsetOfHolderShape()),
                         scratchReg);
        }
        masm.branchTestObjShape(Assembler::NotEqual, holderReg, scratchReg,
                                popR1 ? &failurePopR1 : &failure);
    }

    if (acctype_ == ICGetElemNativeStub::DynamicSlot ||
        acctype_ == ICGetElemNativeStub::FixedSlot)
    {
        masm.load32(Address(ICStubReg, ICGetElemNativeSlotStub<T>::offsetOfOffset()),
                    scratchReg);

        // Load from object.
        if (acctype_ == ICGetElemNativeStub::DynamicSlot)
            masm.addPtr(Address(holderReg, NativeObject::offsetOfSlots()), scratchReg);
        else
            masm.addPtr(holderReg, scratchReg);

        Address valAddr(scratchReg, 0);

        // Check if __noSuchMethod__ needs to be called.
#if JS_HAS_NO_SUCH_METHOD
        if (isCallElem_) {
            Label afterNoSuchMethod;
            Label skipNoSuchMethod;

            masm.branchTestUndefined(Assembler::NotEqual, valAddr, &skipNoSuchMethod);

            AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));
            regs.take(R1);
            regs.take(R0);
            regs.takeUnchecked(objReg);
            if (popR1)
                masm.pop(R1.scratchReg());

            // Box and push obj and key onto baseline frame stack for decompiler.
            masm.tagValue(JSVAL_TYPE_OBJECT, objReg, R0);
            EmitStowICValues(masm, 2);

            regs.add(R0);
            regs.takeUnchecked(objReg);

            enterStubFrame(masm, regs.getAnyExcluding(ICTailCallReg));

            masm.pushValue(R1);
            masm.push(objReg);
            if (!callVM(LookupNoSuchMethodHandlerInfo, masm))
                return false;

            leaveStubFrame(masm);

            // Pop pushed obj and key from baseline stack.
            EmitUnstowICValues(masm, 2, /* discard = */ true);

            // Result is already in R0
            masm.jump(&afterNoSuchMethod);
            masm.bind(&skipNoSuchMethod);

            if (popR1)
                masm.pop(R1.scratchReg());
            masm.loadValue(valAddr, R0);
            masm.bind(&afterNoSuchMethod);
        } else {
            masm.loadValue(valAddr, R0);
            if (popR1)
                masm.addToStackPtr(ImmWord(sizeof(size_t)));
        }
#else
        masm.loadValue(valAddr, R0);
        if (popR1)
            masm.addToStackPtr(ImmWord(sizeof(size_t)));
#endif

    } else if (acctype_ == ICGetElemNativeStub::UnboxedProperty) {
        masm.load32(Address(ICStubReg, ICGetElemNativeSlotStub<T>::offsetOfOffset()),
                    scratchReg);
        masm.loadUnboxedProperty(BaseIndex(objReg, scratchReg, TimesOne), unboxedType_,
                                 TypedOrValueRegister(R0));
        if (popR1)
            masm.addToStackPtr(ImmWord(sizeof(size_t)));
    } else {
        MOZ_ASSERT(acctype_ == ICGetElemNativeStub::NativeGetter ||
                   acctype_ == ICGetElemNativeStub::ScriptedGetter);
        MOZ_ASSERT(kind == ICStub::GetElem_NativePrototypeCallNativeName ||
                   kind == ICStub::GetElem_NativePrototypeCallNativeSymbol ||
                   kind == ICStub::GetElem_NativePrototypeCallScriptedName ||
                   kind == ICStub::GetElem_NativePrototypeCallScriptedSymbol);

        if (acctype_ == ICGetElemNativeStub::NativeGetter) {
            // If calling a native getter, there is no chance of failure now.

            // GetElem key (R1) is no longer needed.
            if (popR1)
                masm.addToStackPtr(ImmWord(sizeof(size_t)));

            emitCallNative(masm, objReg);

        } else {
            MOZ_ASSERT(acctype_ == ICGetElemNativeStub::ScriptedGetter);

            // Load function in scratchReg and ensure that it has a jit script.
            masm.loadPtr(Address(ICStubReg, ICGetElemNativeGetterStub<T>::offsetOfGetter()),
                         scratchReg);
            masm.branchIfFunctionHasNoScript(scratchReg, popR1 ? &failurePopR1 : &failure);
            masm.loadPtr(Address(scratchReg, JSFunction::offsetOfNativeOrScript()), scratchReg);
            masm.loadBaselineOrIonRaw(scratchReg, scratchReg, popR1 ? &failurePopR1 : &failure);

            // At this point, we are guaranteed to successfully complete.
            if (popR1)
                masm.addToStackPtr(Imm32(sizeof(size_t)));

            emitCallScripted(masm, objReg);
        }
    }

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    // Failure case - jump to next stub
    if (popR1) {
        masm.bind(&failurePopR1);
        masm.pop(R1.scratchReg());
    }
    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    return true;
}

//
// GetElem_String
//

bool
ICGetElem_String::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    masm.branchTestString(Assembler::NotEqual, R0, &failure);
    masm.branchTestInt32(Assembler::NotEqual, R1, &failure);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratchReg = regs.takeAny();

    // Unbox string in R0.
    Register str = masm.extractString(R0, ExtractTemp0);

    // Check for non-linear strings.
    masm.branchIfRope(str, &failure);

    // Unbox key.
    Register key = masm.extractInt32(R1, ExtractTemp1);

    // Bounds check.
    masm.branch32(Assembler::BelowOrEqual, Address(str, JSString::offsetOfLength()),
                  key, &failure);

    // Get char code.
    masm.loadStringChar(str, key, scratchReg);

    // Check if char code >= UNIT_STATIC_LIMIT.
    masm.branch32(Assembler::AboveOrEqual, scratchReg, Imm32(StaticStrings::UNIT_STATIC_LIMIT),
                  &failure);

    // Load static string.
    masm.movePtr(ImmPtr(&cx->staticStrings().unitStaticTable), str);
    masm.loadPtr(BaseIndex(str, scratchReg, ScalePointer), str);

    // Return.
    masm.tagValue(JSVAL_TYPE_STRING, str, R0);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// GetElem_Dense
//

bool
ICGetElem_Dense::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);
    masm.branchTestInt32(Assembler::NotEqual, R1, &failure);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratchReg = regs.takeAny();

    // Unbox R0 and shape guard.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    masm.loadPtr(Address(ICStubReg, ICGetElem_Dense::offsetOfShape()), scratchReg);
    masm.branchTestObjShape(Assembler::NotEqual, obj, scratchReg, &failure);

    // Load obj->elements.
    masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratchReg);

    // Unbox key.
    Register key = masm.extractInt32(R1, ExtractTemp1);

    // Bounds check.
    Address initLength(scratchReg, ObjectElements::offsetOfInitializedLength());
    masm.branch32(Assembler::BelowOrEqual, initLength, key, &failure);

    // Hole check and load value.
    BaseObjectElementIndex element(scratchReg, key);
    masm.branchTestMagic(Assembler::Equal, element, &failure);

    // Check if __noSuchMethod__ should be called.
#if JS_HAS_NO_SUCH_METHOD
#ifdef DEBUG
    entersStubFrame_ = true;
#endif
    if (isCallElem_) {
        Label afterNoSuchMethod;
        Label skipNoSuchMethod;
        regs = availableGeneralRegs(0);
        regs.takeUnchecked(obj);
        regs.takeUnchecked(key);
        regs.takeUnchecked(ICTailCallReg);
        ValueOperand val = regs.takeAnyValue();

        masm.loadValue(element, val);
        masm.branchTestUndefined(Assembler::NotEqual, val, &skipNoSuchMethod);

        // Box and push obj and key onto baseline frame stack for decompiler.
        EmitRestoreTailCallReg(masm);
        masm.tagValue(JSVAL_TYPE_OBJECT, obj, val);
        masm.pushValue(val);
        masm.tagValue(JSVAL_TYPE_INT32, key, val);
        masm.pushValue(val);
        EmitRepushTailCallReg(masm);

        regs.add(val);

        // Call __noSuchMethod__ checker.  Object pointer is in objReg.
        enterStubFrame(masm, regs.getAnyExcluding(ICTailCallReg));

        regs.take(val);

        masm.tagValue(JSVAL_TYPE_INT32, key, val);
        masm.pushValue(val);
        masm.push(obj);
        if (!callVM(LookupNoSuchMethodHandlerInfo, masm))
            return false;

        leaveStubFrame(masm);

        // Pop pushed obj and key from baseline stack.
        EmitUnstowICValues(masm, 2, /* discard = */ true);

        // Result is already in R0
        masm.jump(&afterNoSuchMethod);
        masm.bind(&skipNoSuchMethod);

        masm.moveValue(val, R0);
        masm.bind(&afterNoSuchMethod);
    } else {
        masm.loadValue(element, R0);
    }
#else
    // Load value from element location.
    masm.loadValue(element, R0);
#endif

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// GetElem_UnboxedArray
//

bool
ICGetElem_UnboxedArray::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);
    masm.branchTestInt32(Assembler::NotEqual, R1, &failure);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratchReg = regs.takeAny();

    // Unbox R0 and group guard.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    masm.loadPtr(Address(ICStubReg, ICGetElem_UnboxedArray::offsetOfGroup()), scratchReg);
    masm.branchTestObjGroup(Assembler::NotEqual, obj, scratchReg, &failure);

    // Unbox key.
    Register key = masm.extractInt32(R1, ExtractTemp1);

    // Bounds check.
    masm.load32(Address(obj, UnboxedArrayObject::offsetOfCapacityIndexAndInitializedLength()),
                scratchReg);
    masm.and32(Imm32(UnboxedArrayObject::InitializedLengthMask), scratchReg);
    masm.branch32(Assembler::BelowOrEqual, scratchReg, key, &failure);

    // Load obj->elements.
    masm.loadPtr(Address(obj, UnboxedArrayObject::offsetOfElements()), scratchReg);

    // Load value.
    size_t width = UnboxedTypeSize(elementType_);
    BaseIndex addr(scratchReg, key, ScaleFromElemWidth(width));
    masm.loadUnboxedProperty(addr, elementType_, R0);

    // Only monitor the result if its type might change.
    if (elementType_ == JSVAL_TYPE_OBJECT)
        EmitEnterTypeMonitorIC(masm);
    else
        EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// GetElem_TypedArray
//

static void
LoadTypedThingLength(MacroAssembler& masm, TypedThingLayout layout, Register obj, Register result)
{
    switch (layout) {
      case Layout_TypedArray:
        masm.unboxInt32(Address(obj, TypedArrayLayout::lengthOffset()), result);
        break;
      case Layout_OutlineTypedObject:
      case Layout_InlineTypedObject:
        masm.loadPtr(Address(obj, JSObject::offsetOfGroup()), result);
        masm.loadPtr(Address(result, ObjectGroup::offsetOfAddendum()), result);
        masm.unboxInt32(Address(result, ArrayTypeDescr::offsetOfLength()), result);
        break;
      default:
        MOZ_CRASH();
    }
}

static void
LoadTypedThingData(MacroAssembler& masm, TypedThingLayout layout, Register obj, Register result)
{
    switch (layout) {
      case Layout_TypedArray:
        masm.loadPtr(Address(obj, TypedArrayLayout::dataOffset()), result);
        break;
      case Layout_OutlineTypedObject:
        masm.loadPtr(Address(obj, OutlineTypedObject::offsetOfData()), result);
        break;
      case Layout_InlineTypedObject:
        masm.computeEffectiveAddress(Address(obj, InlineTypedObject::offsetOfDataStart()), result);
        break;
      default:
        MOZ_CRASH();
    }
}

static void
CheckForNeuteredTypedObject(JSContext* cx, MacroAssembler& masm, Label* failure)
{
    // All stubs which manipulate typed objects need to check the compartment
    // wide flag indicating whether the objects are neutered, and bail out in
    // this case.
    int32_t* address = &cx->compartment()->neuteredTypedObjects;
    masm.branch32(Assembler::NotEqual, AbsoluteAddress(address), Imm32(0), failure);
}

bool
ICGetElem_TypedArray::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;

    if (layout_ != Layout_TypedArray)
        CheckForNeuteredTypedObject(cx, masm, &failure);

    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratchReg = regs.takeAny();

    // Unbox R0 and shape guard.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    masm.loadPtr(Address(ICStubReg, ICGetElem_TypedArray::offsetOfShape()), scratchReg);
    masm.branchTestObjShape(Assembler::NotEqual, obj, scratchReg, &failure);

    // Ensure the index is an integer.
    if (cx->runtime()->jitSupportsFloatingPoint) {
        Label isInt32;
        masm.branchTestInt32(Assembler::Equal, R1, &isInt32);
        {
            // If the index is a double, try to convert it to int32. It's okay
            // to convert -0 to 0: the shape check ensures the object is a typed
            // array so the difference is not observable.
            masm.branchTestDouble(Assembler::NotEqual, R1, &failure);
            masm.unboxDouble(R1, FloatReg0);
            masm.convertDoubleToInt32(FloatReg0, scratchReg, &failure, /* negZeroCheck = */false);
            masm.tagValue(JSVAL_TYPE_INT32, scratchReg, R1);
        }
        masm.bind(&isInt32);
    } else {
        masm.branchTestInt32(Assembler::NotEqual, R1, &failure);
    }

    // Unbox key.
    Register key = masm.extractInt32(R1, ExtractTemp1);

    // Bounds check.
    LoadTypedThingLength(masm, layout_, obj, scratchReg);
    masm.branch32(Assembler::BelowOrEqual, scratchReg, key, &failure);

    // Load the elements vector.
    LoadTypedThingData(masm, layout_, obj, scratchReg);

    // Load the value.
    BaseIndex source(scratchReg, key, ScaleFromElemWidth(Scalar::byteSize(type_)));
    masm.loadFromTypedArray(type_, source, R0, false, scratchReg, &failure);

    // Todo: Allow loading doubles from uint32 arrays, but this requires monitoring.
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// GetElem_Arguments
//
bool
ICGetElem_Arguments::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    // Variants of GetElem_Arguments can enter stub frames if entered in CallProp
    // context when noSuchMethod support is on.
#if JS_HAS_NO_SUCH_METHOD
#ifdef DEBUG
    entersStubFrame_ = true;
#endif
#endif

    Label failure;
    if (which_ == ICGetElem_Arguments::Magic) {
        MOZ_ASSERT(!isCallElem_);

        // Ensure that this is a magic arguments value.
        masm.branchTestMagicValue(Assembler::NotEqual, R0, JS_OPTIMIZED_ARGUMENTS, &failure);

        // Ensure that frame has not loaded different arguments object since.
        masm.branchTest32(Assembler::NonZero,
                          Address(BaselineFrameReg, BaselineFrame::reverseOffsetOfFlags()),
                          Imm32(BaselineFrame::HAS_ARGS_OBJ),
                          &failure);

        // Ensure that index is an integer.
        masm.branchTestInt32(Assembler::NotEqual, R1, &failure);
        Register idx = masm.extractInt32(R1, ExtractTemp1);

        AllocatableGeneralRegisterSet regs(availableGeneralRegs(2));
        Register scratch = regs.takeAny();

        // Load num actual arguments
        Address actualArgs(BaselineFrameReg, BaselineFrame::offsetOfNumActualArgs());
        masm.loadPtr(actualArgs, scratch);

        // Ensure idx < argc
        masm.branch32(Assembler::AboveOrEqual, idx, scratch, &failure);

        // Load argval
        masm.movePtr(BaselineFrameReg, scratch);
        masm.addPtr(Imm32(BaselineFrame::offsetOfArg(0)), scratch);
        BaseValueIndex element(scratch, idx);
        masm.loadValue(element, R0);

        // Enter type monitor IC to type-check result.
        EmitEnterTypeMonitorIC(masm);

        masm.bind(&failure);
        EmitStubGuardFailure(masm);
        return true;
    }

    MOZ_ASSERT(which_ == ICGetElem_Arguments::Mapped ||
               which_ == ICGetElem_Arguments::Unmapped);

    const Class* clasp = (which_ == ICGetElem_Arguments::Mapped)
                         ? &MappedArgumentsObject::class_
                         : &UnmappedArgumentsObject::class_;

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratchReg = regs.takeAny();

    // Guard on input being an arguments object.
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);
    Register objReg = masm.extractObject(R0, ExtractTemp0);
    masm.branchTestObjClass(Assembler::NotEqual, objReg, scratchReg, clasp, &failure);

    // Guard on index being int32
    masm.branchTestInt32(Assembler::NotEqual, R1, &failure);
    Register idxReg = masm.extractInt32(R1, ExtractTemp1);

    // Get initial ArgsObj length value.
    masm.unboxInt32(Address(objReg, ArgumentsObject::getInitialLengthSlotOffset()), scratchReg);

    // Test if length has been overridden.
    masm.branchTest32(Assembler::NonZero,
                      scratchReg,
                      Imm32(ArgumentsObject::LENGTH_OVERRIDDEN_BIT),
                      &failure);

    // Length has not been overridden, ensure that R1 is an integer and is <= length.
    masm.rshiftPtr(Imm32(ArgumentsObject::PACKED_BITS_COUNT), scratchReg);
    masm.branch32(Assembler::AboveOrEqual, idxReg, scratchReg, &failure);

    // Length check succeeded, now check the correct bit.  We clobber potential type regs
    // now.  Inputs will have to be reconstructed if we fail after this point, but that's
    // unlikely.
    Label failureReconstructInputs;
    regs = availableGeneralRegs(0);
    regs.takeUnchecked(objReg);
    regs.takeUnchecked(idxReg);
    regs.take(scratchReg);
    Register argData = regs.takeAny();
    Register tempReg = regs.takeAny();

    // Load ArgumentsData
    masm.loadPrivate(Address(objReg, ArgumentsObject::getDataSlotOffset()), argData);

    // Load deletedBits bitArray pointer into scratchReg
    masm.loadPtr(Address(argData, offsetof(ArgumentsData, deletedBits)), scratchReg);

    // In tempReg, calculate index of word containing bit: (idx >> logBitsPerWord)
    masm.movePtr(idxReg, tempReg);
    const uint32_t shift = mozilla::tl::FloorLog2<(sizeof(size_t) * JS_BITS_PER_BYTE)>::value;
    MOZ_ASSERT(shift == 5 || shift == 6);
    masm.rshiftPtr(Imm32(shift), tempReg);
    masm.loadPtr(BaseIndex(scratchReg, tempReg, ScaleFromElemWidth(sizeof(size_t))), scratchReg);

    // Don't bother testing specific bit, if any bit is set in the word, fail.
    masm.branchPtr(Assembler::NotEqual, scratchReg, ImmPtr(nullptr), &failureReconstructInputs);

    // Load the value.  use scratchReg and tempReg to form a ValueOperand to load into.
    masm.addPtr(Imm32(ArgumentsData::offsetOfArgs()), argData);
    regs.add(scratchReg);
    regs.add(tempReg);
    ValueOperand tempVal = regs.takeAnyValue();
    masm.loadValue(BaseValueIndex(argData, idxReg), tempVal);

    // Makesure that this is not a FORWARD_TO_CALL_SLOT magic value.
    masm.branchTestMagic(Assembler::Equal, tempVal, &failureReconstructInputs);

#if JS_HAS_NO_SUCH_METHOD
    if (isCallElem_) {
        Label afterNoSuchMethod;
        Label skipNoSuchMethod;

        masm.branchTestUndefined(Assembler::NotEqual, tempVal, &skipNoSuchMethod);

        // Call __noSuchMethod__ checker.  Object pointer is in objReg.
        regs = availableGeneralRegs(0);
        regs.takeUnchecked(objReg);
        regs.takeUnchecked(idxReg);
        regs.takeUnchecked(ICTailCallReg);
        ValueOperand val = regs.takeAnyValue();

        // Box and push obj and key onto baseline frame stack for decompiler.
        EmitRestoreTailCallReg(masm);
        masm.tagValue(JSVAL_TYPE_OBJECT, objReg, val);
        masm.pushValue(val);
        masm.tagValue(JSVAL_TYPE_INT32, idxReg, val);
        masm.pushValue(val);
        EmitRepushTailCallReg(masm);

        regs.add(val);
        enterStubFrame(masm, regs.getAnyExcluding(ICTailCallReg));
        regs.take(val);

        masm.pushValue(val);
        masm.push(objReg);
        if (!callVM(LookupNoSuchMethodHandlerInfo, masm))
            return false;

        leaveStubFrame(masm);

        // Pop pushed obj and key from baseline stack.
        EmitUnstowICValues(masm, 2, /* discard = */ true);

        // Result is already in R0
        masm.jump(&afterNoSuchMethod);
        masm.bind(&skipNoSuchMethod);

        masm.moveValue(tempVal, R0);
        masm.bind(&afterNoSuchMethod);
    } else {
        masm.moveValue(tempVal, R0);
    }
#else
    // Copy value from temp to R0.
    masm.moveValue(tempVal, R0);
#endif

    // Type-check result
    EmitEnterTypeMonitorIC(masm);

    // Failed, but inputs are deconstructed into object and int, and need to be
    // reconstructed into values.
    masm.bind(&failureReconstructInputs);
    masm.tagValue(JSVAL_TYPE_OBJECT, objReg, R0);
    masm.tagValue(JSVAL_TYPE_INT32, idxReg, R1);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// SetElem_Fallback
//

static bool
SetElemAddHasSameShapes(ICSetElem_DenseOrUnboxedArrayAdd* stub, JSObject* obj)
{
    static const size_t MAX_DEPTH = ICSetElem_DenseOrUnboxedArrayAdd::MAX_PROTO_CHAIN_DEPTH;
    ICSetElem_DenseOrUnboxedArrayAddImpl<MAX_DEPTH>* nstub = stub->toImplUnchecked<MAX_DEPTH>();

    if (obj->maybeShape() != nstub->shape(0))
        return false;

    JSObject* proto = obj->getProto();
    for (size_t i = 0; i < stub->protoChainDepth(); i++) {
        if (!proto->isNative())
            return false;
        if (proto->as<NativeObject>().lastProperty() != nstub->shape(i + 1))
            return false;
        proto = proto->getProto();
        if (!proto) {
            if (i != stub->protoChainDepth() - 1)
                return false;
            break;
        }
    }

    return true;
}

static bool
DenseOrUnboxedArraySetElemStubExists(JSContext* cx, ICStub::Kind kind,
                                     ICSetElem_Fallback* stub, HandleObject obj)
{
    MOZ_ASSERT(kind == ICStub::SetElem_DenseOrUnboxedArray ||
               kind == ICStub::SetElem_DenseOrUnboxedArrayAdd);

    if (obj->isSingleton())
        return false;

    for (ICStubConstIterator iter = stub->beginChainConst(); !iter.atEnd(); iter++) {
        if (kind == ICStub::SetElem_DenseOrUnboxedArray && iter->isSetElem_DenseOrUnboxedArray()) {
            ICSetElem_DenseOrUnboxedArray* nstub = iter->toSetElem_DenseOrUnboxedArray();
            if (obj->maybeShape() == nstub->shape() && obj->group() == nstub->group())
                return true;
        }

        if (kind == ICStub::SetElem_DenseOrUnboxedArrayAdd && iter->isSetElem_DenseOrUnboxedArrayAdd()) {
            ICSetElem_DenseOrUnboxedArrayAdd* nstub = iter->toSetElem_DenseOrUnboxedArrayAdd();
            if (obj->group() == nstub->group() && SetElemAddHasSameShapes(nstub, obj))
                return true;
        }
    }
    return false;
}

static void
RemoveMatchingDenseOrUnboxedArraySetElemAddStub(JSContext* cx,
                                                ICSetElem_Fallback* stub, HandleObject obj)
{
    if (obj->isSingleton())
        return;

    // Before attaching a new stub to add elements to a dense or unboxed array,
    // remove any other stub with the same group/shape but different prototype
    // shapes.
    for (ICStubIterator iter = stub->beginChain(); !iter.atEnd(); iter++) {
        if (!iter->isSetElem_DenseOrUnboxedArrayAdd())
            continue;

        ICSetElem_DenseOrUnboxedArrayAdd* nstub = iter->toSetElem_DenseOrUnboxedArrayAdd();
        if (obj->group() != nstub->group())
            continue;

        static const size_t MAX_DEPTH = ICSetElem_DenseOrUnboxedArrayAdd::MAX_PROTO_CHAIN_DEPTH;
        ICSetElem_DenseOrUnboxedArrayAddImpl<MAX_DEPTH>* nostub = nstub->toImplUnchecked<MAX_DEPTH>();

        if (obj->maybeShape() == nostub->shape(0)) {
            iter.unlink(cx);
            return;
        }
    }
}

static bool
TypedArraySetElemStubExists(ICSetElem_Fallback* stub, HandleObject obj, bool expectOOB)
{
    for (ICStubConstIterator iter = stub->beginChainConst(); !iter.atEnd(); iter++) {
        if (!iter->isSetElem_TypedArray())
            continue;
        ICSetElem_TypedArray* taStub = iter->toSetElem_TypedArray();
        if (obj->maybeShape() == taStub->shape() && taStub->expectOutOfBounds() == expectOOB)
            return true;
    }
    return false;
}

static bool
RemoveExistingTypedArraySetElemStub(JSContext* cx, ICSetElem_Fallback* stub, HandleObject obj)
{
    for (ICStubIterator iter = stub->beginChain(); !iter.atEnd(); iter++) {
        if (!iter->isSetElem_TypedArray())
            continue;

        if (obj->maybeShape() != iter->toSetElem_TypedArray()->shape())
            continue;

        // TypedArraySetElem stubs are only removed using this procedure if
        // being replaced with one that expects out of bounds index.
        MOZ_ASSERT(!iter->toSetElem_TypedArray()->expectOutOfBounds());
        iter.unlink(cx);
        return true;
    }
    return false;
}

static bool
CanOptimizeDenseOrUnboxedArraySetElem(JSObject* obj, uint32_t index,
                                      Shape* oldShape, uint32_t oldCapacity, uint32_t oldInitLength,
                                      bool* isAddingCaseOut, size_t* protoDepthOut)
{
    uint32_t initLength = GetAnyBoxedOrUnboxedInitializedLength(obj);
    uint32_t capacity = GetAnyBoxedOrUnboxedCapacity(obj);

    *isAddingCaseOut = false;
    *protoDepthOut = 0;

    // Some initial sanity checks.
    if (initLength < oldInitLength || capacity < oldCapacity)
        return false;

    // Unboxed arrays need to be able to emit floating point code.
    if (obj->is<UnboxedArrayObject>() && !obj->runtimeFromMainThread()->jitSupportsFloatingPoint)
        return false;

    Shape* shape = obj->maybeShape();

    // Cannot optimize if the shape changed.
    if (oldShape != shape)
        return false;

    // Cannot optimize if the capacity changed.
    if (oldCapacity != capacity)
        return false;

    // Cannot optimize if the index doesn't fit within the new initialized length.
    if (index >= initLength)
        return false;

    // Cannot optimize if the value at position after the set is a hole.
    if (obj->isNative() && !obj->as<NativeObject>().containsDenseElement(index))
        return false;

    // At this point, if we know that the initLength did not change, then
    // an optimized set is possible.
    if (oldInitLength == initLength)
        return true;

    // If it did change, ensure that it changed specifically by incrementing by 1
    // to accomodate this particular indexed set.
    if (oldInitLength + 1 != initLength)
        return false;
    if (index != oldInitLength)
        return false;

    // The checks are not complete.  The object may have a setter definition,
    // either directly, or via a prototype, or via the target object for a prototype
    // which is a proxy, that handles a particular integer write.
    // Scan the prototype and shape chain to make sure that this is not the case.
    if (obj->isIndexed())
        return false;
    JSObject* curObj = obj->getProto();
    while (curObj) {
        ++*protoDepthOut;
        if (!curObj->isNative() || curObj->isIndexed())
            return false;
        curObj = curObj->getProto();
    }

    if (*protoDepthOut > ICSetElem_DenseOrUnboxedArrayAdd::MAX_PROTO_CHAIN_DEPTH)
        return false;

    *isAddingCaseOut = true;
    return true;
}

static bool
DoSetElemFallback(JSContext* cx, BaselineFrame* frame, ICSetElem_Fallback* stub_, Value* stack,
                  HandleValue objv, HandleValue index, HandleValue rhs)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICSetElem_Fallback*> stub(frame, stub_);

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    JSOp op = JSOp(*pc);
    FallbackICSpew(cx, stub, "SetElem(%s)", js_CodeName[JSOp(*pc)]);

    MOZ_ASSERT(op == JSOP_SETELEM ||
               op == JSOP_STRICTSETELEM ||
               op == JSOP_INITELEM ||
               op == JSOP_INITELEM_ARRAY ||
               op == JSOP_INITELEM_INC);

    RootedObject obj(cx, ToObjectFromStack(cx, objv));
    if (!obj)
        return false;

    RootedShape oldShape(cx, obj->maybeShape());

    // Check the old capacity
    uint32_t oldCapacity = 0;
    uint32_t oldInitLength = 0;
    if (index.isInt32() && index.toInt32() >= 0) {
        oldCapacity = GetAnyBoxedOrUnboxedCapacity(obj);
        oldInitLength = GetAnyBoxedOrUnboxedInitializedLength(obj);
    }

    if (op == JSOP_INITELEM) {
        if (!InitElemOperation(cx, obj, index, rhs))
            return false;
    } else if (op == JSOP_INITELEM_ARRAY) {
        MOZ_ASSERT(uint32_t(index.toInt32()) == GET_UINT24(pc));
        if (!InitArrayElemOperation(cx, pc, obj, index.toInt32(), rhs))
            return false;
    } else if (op == JSOP_INITELEM_INC) {
        if (!InitArrayElemOperation(cx, pc, obj, index.toInt32(), rhs))
            return false;
    } else {
        if (!SetObjectElement(cx, obj, index, rhs, JSOp(*pc) == JSOP_STRICTSETELEM, script, pc))
            return false;
    }

    // Overwrite the object on the stack (pushed for the decompiler) with the rhs.
    MOZ_ASSERT(stack[2] == objv);
    stack[2] = rhs;

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid())
        return true;

    if (stub->numOptimizedStubs() >= ICSetElem_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard all stubs in this IC and replace with inert megamorphic stub.
        // But for now we just bail.
        return true;
    }

    // Try to generate new stubs.
    if (IsNativeOrUnboxedDenseElementAccess(obj, index) && !rhs.isMagic(JS_ELEMENTS_HOLE)) {
        bool addingCase;
        size_t protoDepth;

        if (CanOptimizeDenseOrUnboxedArraySetElem(obj, index.toInt32(),
                                                  oldShape, oldCapacity, oldInitLength,
                                                  &addingCase, &protoDepth))
        {
            RootedShape shape(cx, obj->maybeShape());
            RootedObjectGroup group(cx, obj->getGroup(cx));
            if (!group)
                return false;

            if (addingCase &&
                !DenseOrUnboxedArraySetElemStubExists(cx, ICStub::SetElem_DenseOrUnboxedArrayAdd,
                                                      stub, obj))
            {
                RemoveMatchingDenseOrUnboxedArraySetElemAddStub(cx, stub, obj);

                JitSpew(JitSpew_BaselineIC,
                        "  Generating SetElem_DenseOrUnboxedArrayAdd stub "
                        "(shape=%p, group=%p, protoDepth=%u)",
                        shape.get(), group.get(), protoDepth);
                ICSetElemDenseOrUnboxedArrayAddCompiler compiler(cx, obj, protoDepth);
                ICUpdatedStub* newStub = compiler.getStub(compiler.getStubSpace(script));
                if (!newStub)
                    return false;
                if (compiler.needsUpdateStubs() &&
                    !newStub->addUpdateStubForValue(cx, script, obj, JSID_VOIDHANDLE, rhs))
                {
                    return false;
                }

                stub->addNewStub(newStub);
            } else if (!addingCase &&
                       !DenseOrUnboxedArraySetElemStubExists(cx,
                                                             ICStub::SetElem_DenseOrUnboxedArray,
                                                             stub, obj))
            {
                JitSpew(JitSpew_BaselineIC,
                        "  Generating SetElem_DenseOrUnboxedArray stub (shape=%p, group=%p)",
                        shape.get(), group.get());
                ICSetElem_DenseOrUnboxedArray::Compiler compiler(cx, shape, group);
                ICUpdatedStub* newStub = compiler.getStub(compiler.getStubSpace(script));
                if (!newStub)
                    return false;
                if (compiler.needsUpdateStubs() &&
                    !newStub->addUpdateStubForValue(cx, script, obj, JSID_VOIDHANDLE, rhs))
                {
                    return false;
                }

                stub->addNewStub(newStub);
            }
        }

        return true;
    }

    if ((IsAnyTypedArray(obj.get()) || IsPrimitiveArrayTypedObject(obj)) &&
        index.isNumber() &&
        rhs.isNumber())
    {
        if (!cx->runtime()->jitSupportsFloatingPoint &&
            (TypedThingRequiresFloatingPoint(obj) || index.isDouble()))
        {
            return true;
        }

        bool expectOutOfBounds;
        double idx = index.toNumber();
        if (IsAnyTypedArray(obj)) {
            expectOutOfBounds = (idx < 0 || idx >= double(AnyTypedArrayLength(obj)));
        } else {
            // Typed objects throw on out of bounds accesses. Don't attach
            // a stub in this case.
            if (idx < 0 || idx >= double(obj->as<TypedObject>().length()))
                return true;
            expectOutOfBounds = false;

            // Don't attach stubs if typed objects in the compartment might be
            // neutered, as the stub will always bail out.
            if (cx->compartment()->neuteredTypedObjects)
                return true;
        }

        if (!TypedArraySetElemStubExists(stub, obj, expectOutOfBounds)) {
            // Remove any existing TypedArraySetElemStub that doesn't handle out-of-bounds
            if (expectOutOfBounds)
                RemoveExistingTypedArraySetElemStub(cx, stub, obj);

            Shape* shape = obj->maybeShape();
            Scalar::Type type = TypedThingElementType(obj);

            JitSpew(JitSpew_BaselineIC,
                    "  Generating SetElem_TypedArray stub (shape=%p, type=%u, oob=%s)",
                    shape, type, expectOutOfBounds ? "yes" : "no");
            ICSetElem_TypedArray::Compiler compiler(cx, shape, type, expectOutOfBounds);
            ICStub* typedArrayStub = compiler.getStub(compiler.getStubSpace(script));
            if (!typedArrayStub)
                return false;

            stub->addNewStub(typedArrayStub);
            return true;
        }
    }

    return true;
}

typedef bool (*DoSetElemFallbackFn)(JSContext*, BaselineFrame*, ICSetElem_Fallback*, Value*,
                                    HandleValue, HandleValue, HandleValue);
static const VMFunction DoSetElemFallbackInfo =
    FunctionInfo<DoSetElemFallbackFn>(DoSetElemFallback, TailCall, PopValues(2));

bool
ICSetElem_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);
    MOZ_ASSERT(R0 == JSReturnOperand);

    EmitRestoreTailCallReg(masm);

    // State: R0: object, R1: index, stack: rhs.
    // For the decompiler, the stack has to be: object, index, rhs,
    // so we push the index, then overwrite the rhs Value with R0
    // and push the rhs value.
    masm.pushValue(R1);
    masm.loadValue(Address(masm.getStackPointer(), sizeof(Value)), R1);
    masm.storeValue(R0, Address(masm.getStackPointer(), sizeof(Value)));
    masm.pushValue(R1);

    // Push arguments.
    masm.pushValue(R1); // RHS

    // Push index. On x86 and ARM two push instructions are emitted so use a
    // separate register to store the old stack pointer.
    masm.moveStackPtrTo(R1.scratchReg());
    masm.pushValue(Address(R1.scratchReg(), 2 * sizeof(Value)));
    masm.pushValue(R0); // Object.

    // Push pointer to stack values, so that the stub can overwrite the object
    // (pushed for the decompiler) with the rhs.
    masm.computeEffectiveAddress(Address(masm.getStackPointer(), 3 * sizeof(Value)), R0.scratchReg());
    masm.push(R0.scratchReg());

    masm.push(ICStubReg);
    pushFramePtr(masm, R0.scratchReg());

    return tailCallVM(DoSetElemFallbackInfo, masm);
}

void
BaselineScript::noteArrayWriteHole(uint32_t pcOffset)
{
    ICEntry& entry = icEntryFromPCOffset(pcOffset);
    ICFallbackStub* stub = entry.fallbackStub();

    if (stub->isSetElem_Fallback())
        stub->toSetElem_Fallback()->noteArrayWriteHole();
}

//
// SetElem_DenseOrUnboxedArray
//

template <typename T>
void
EmitUnboxedPreBarrierForBaseline(MacroAssembler &masm, T address, JSValueType type)
{
    if (type == JSVAL_TYPE_OBJECT)
        EmitPreBarrier(masm, address, MIRType_Object);
    else if (type == JSVAL_TYPE_STRING)
        EmitPreBarrier(masm, address, MIRType_String);
    else
        MOZ_ASSERT(!UnboxedTypeNeedsPreBarrier(type));
}

bool
ICSetElem_DenseOrUnboxedArray::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    // R0 = object
    // R1 = key
    // Stack = { ... rhs-value, <return-addr>? }
    Label failure, failurePopR0;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);
    masm.branchTestInt32(Assembler::NotEqual, R1, &failure);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratchReg = regs.takeAny();

    // Unbox R0 and guard on its group and, if this is a native access, its shape.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    masm.loadPtr(Address(ICStubReg, ICSetElem_DenseOrUnboxedArray::offsetOfGroup()),
                 scratchReg);
    masm.branchTestObjGroup(Assembler::NotEqual, obj, scratchReg, &failure);
    if (unboxedType_ == JSVAL_TYPE_MAGIC) {
        masm.loadPtr(Address(ICStubReg, ICSetElem_DenseOrUnboxedArray::offsetOfShape()),
                     scratchReg);
        masm.branchTestObjShape(Assembler::NotEqual, obj, scratchReg, &failure);
    }

    if (needsUpdateStubs()) {
        // Stow both R0 and R1 (object and key)
        // But R0 and R1 still hold their values.
        EmitStowICValues(masm, 2);

        // Stack is now: { ..., rhs-value, object-value, key-value, maybe?-RET-ADDR }
        // Load rhs-value into R0
        masm.loadValue(Address(masm.getStackPointer(), 2 * sizeof(Value) + ICStackValueOffset), R0);

        // Call the type-update stub.
        if (!callTypeUpdateIC(masm, sizeof(Value)))
            return false;

        // Unstow R0 and R1 (object and key)
        EmitUnstowICValues(masm, 2);

        // Restore object.
        obj = masm.extractObject(R0, ExtractTemp0);

        // Trigger post barriers here on the value being written. Fields which
        // objects can be written to also need update stubs.
        masm.Push(R1);
        masm.loadValue(Address(masm.getStackPointer(), sizeof(Value) + ICStackValueOffset), R1);

        LiveGeneralRegisterSet saveRegs;
        saveRegs.add(R0);
        saveRegs.addUnchecked(obj);
        saveRegs.add(ICStubReg);
        emitPostWriteBarrierSlot(masm, obj, R1, scratchReg, saveRegs);

        masm.Pop(R1);
    }

    // Unbox key.
    Register key = masm.extractInt32(R1, ExtractTemp1);

    if (unboxedType_ == JSVAL_TYPE_MAGIC) {
        // Set element on a native object.

        // Load obj->elements in scratchReg.
        masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratchReg);

        // Bounds check.
        Address initLength(scratchReg, ObjectElements::offsetOfInitializedLength());
        masm.branch32(Assembler::BelowOrEqual, initLength, key, &failure);

        // Hole check.
        BaseIndex element(scratchReg, key, TimesEight);
        masm.branchTestMagic(Assembler::Equal, element, &failure);

        // Perform a single test to see if we either need to convert double
        // elements or clone the copy on write elements in the object.
        Label noSpecialHandling;
        Address elementsFlags(scratchReg, ObjectElements::offsetOfFlags());
        masm.branchTest32(Assembler::Zero, elementsFlags,
                          Imm32(ObjectElements::CONVERT_DOUBLE_ELEMENTS |
                                ObjectElements::COPY_ON_WRITE),
                          &noSpecialHandling);

        // Fail if we need to clone copy on write elements.
        masm.branchTest32(Assembler::NonZero, elementsFlags,
                          Imm32(ObjectElements::COPY_ON_WRITE),
                          &failure);

        // Failure is not possible now.  Free up registers.
        regs.add(R0);
        regs.add(R1);
        regs.takeUnchecked(obj);
        regs.takeUnchecked(key);

        Address valueAddr(masm.getStackPointer(), ICStackValueOffset);

        // We need to convert int32 values being stored into doubles. In this case
        // the heap typeset is guaranteed to contain both int32 and double, so it's
        // okay to store a double. Note that double arrays are only created by
        // IonMonkey, so if we have no floating-point support Ion is disabled and
        // there should be no double arrays.
        if (cx->runtime()->jitSupportsFloatingPoint)
            masm.convertInt32ValueToDouble(valueAddr, regs.getAny(), &noSpecialHandling);
        else
            masm.assumeUnreachable("There shouldn't be double arrays when there is no FP support.");

        masm.bind(&noSpecialHandling);

        ValueOperand tmpVal = regs.takeAnyValue();
        masm.loadValue(valueAddr, tmpVal);
        EmitPreBarrier(masm, element, MIRType_Value);
        masm.storeValue(tmpVal, element);
    } else {
        // Set element on an unboxed array.

        // Bounds check.
        Address initLength(obj, UnboxedArrayObject::offsetOfCapacityIndexAndInitializedLength());
        masm.load32(initLength, scratchReg);
        masm.and32(Imm32(UnboxedArrayObject::InitializedLengthMask), scratchReg);
        masm.branch32(Assembler::BelowOrEqual, scratchReg, key, &failure);

        // Load obj->elements.
        masm.loadPtr(Address(obj, UnboxedArrayObject::offsetOfElements()), scratchReg);

        // Compute the address being written to.
        BaseIndex address(scratchReg, key, ScaleFromElemWidth(UnboxedTypeSize(unboxedType_)));

        EmitUnboxedPreBarrierForBaseline(masm, address, unboxedType_);

        Address valueAddr(masm.getStackPointer(), ICStackValueOffset + sizeof(Value));
        masm.Push(R0);
        masm.loadValue(valueAddr, R0);
        masm.storeUnboxedProperty(address, unboxedType_,
                                  ConstantOrRegister(TypedOrValueRegister(R0)), &failurePopR0);
        masm.Pop(R0);
    }

    EmitReturnFromIC(masm);

    if (failurePopR0.used()) {
        masm.bind(&failurePopR0);
        masm.Pop(R0);
    }

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

static bool
GetProtoShapes(JSObject* obj, size_t protoChainDepth, MutableHandle<ShapeVector> shapes)
{
    JSObject* curProto = obj->getProto();
    for (size_t i = 0; i < protoChainDepth; i++) {
        if (!shapes.append(curProto->as<NativeObject>().lastProperty()))
            return false;
        curProto = curProto->getProto();
    }
    MOZ_ASSERT(!curProto);
    return true;
}

//
// SetElem_DenseOrUnboxedArrayAdd
//

ICUpdatedStub*
ICSetElemDenseOrUnboxedArrayAddCompiler::getStub(ICStubSpace* space)
{
    Rooted<ShapeVector> shapes(cx, ShapeVector(cx));
    if (!shapes.append(obj_->maybeShape()))
        return nullptr;

    if (!GetProtoShapes(obj_, protoChainDepth_, &shapes))
        return nullptr;

    JS_STATIC_ASSERT(ICSetElem_DenseOrUnboxedArrayAdd::MAX_PROTO_CHAIN_DEPTH == 4);

    ICUpdatedStub* stub = nullptr;
    switch (protoChainDepth_) {
      case 0: stub = getStubSpecific<0>(space, shapes); break;
      case 1: stub = getStubSpecific<1>(space, shapes); break;
      case 2: stub = getStubSpecific<2>(space, shapes); break;
      case 3: stub = getStubSpecific<3>(space, shapes); break;
      case 4: stub = getStubSpecific<4>(space, shapes); break;
      default: MOZ_CRASH("ProtoChainDepth too high.");
    }
    if (!stub || !stub->initUpdatingChain(cx, space))
        return nullptr;
    return stub;
}

bool
ICSetElemDenseOrUnboxedArrayAddCompiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    // R0 = object
    // R1 = key
    // Stack = { ... rhs-value, <return-addr>? }
    Label failure, failurePopR0, failureUnstow;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);
    masm.branchTestInt32(Assembler::NotEqual, R1, &failure);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratchReg = regs.takeAny();

    // Unbox R0 and guard on its group and, if this is a native access, its shape.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    masm.loadPtr(Address(ICStubReg, ICSetElem_DenseOrUnboxedArrayAdd::offsetOfGroup()),
                 scratchReg);
    masm.branchTestObjGroup(Assembler::NotEqual, obj, scratchReg, &failure);
    if (unboxedType_ == JSVAL_TYPE_MAGIC) {
        masm.loadPtr(Address(ICStubReg, ICSetElem_DenseOrUnboxedArrayAddImpl<0>::offsetOfShape(0)),
                     scratchReg);
        masm.branchTestObjShape(Assembler::NotEqual, obj, scratchReg, &failure);
    }

    // Stow both R0 and R1 (object and key)
    // But R0 and R1 still hold their values.
    EmitStowICValues(masm, 2);

    // We may need to free up some registers.
    regs = availableGeneralRegs(0);
    regs.take(R0);
    regs.take(scratchReg);

    // Shape guard objects on the proto chain.
    Register protoReg = regs.takeAny();
    for (size_t i = 0; i < protoChainDepth_; i++) {
        masm.loadObjProto(i == 0 ? obj : protoReg, protoReg);
        masm.branchTestPtr(Assembler::Zero, protoReg, protoReg, &failureUnstow);
        masm.loadPtr(Address(ICStubReg, ICSetElem_DenseOrUnboxedArrayAddImpl<0>::offsetOfShape(i + 1)),
                     scratchReg);
        masm.branchTestObjShape(Assembler::NotEqual, protoReg, scratchReg, &failureUnstow);
    }
    regs.add(protoReg);
    regs.add(scratchReg);

    if (needsUpdateStubs()) {
        // Stack is now: { ..., rhs-value, object-value, key-value, maybe?-RET-ADDR }
        // Load rhs-value in to R0
        masm.loadValue(Address(masm.getStackPointer(), 2 * sizeof(Value) + ICStackValueOffset), R0);

        // Call the type-update stub.
        if (!callTypeUpdateIC(masm, sizeof(Value)))
            return false;
    }

    // Unstow R0 and R1 (object and key)
    EmitUnstowICValues(masm, 2);

    // Restore object.
    obj = masm.extractObject(R0, ExtractTemp0);

    if (needsUpdateStubs()) {
        // Trigger post barriers here on the value being written. Fields which
        // objects can be written to also need update stubs.
        masm.Push(R1);
        masm.loadValue(Address(masm.getStackPointer(), sizeof(Value) + ICStackValueOffset), R1);

        LiveGeneralRegisterSet saveRegs;
        saveRegs.add(R0);
        saveRegs.addUnchecked(obj);
        saveRegs.add(ICStubReg);
        emitPostWriteBarrierSlot(masm, obj, R1, scratchReg, saveRegs);

        masm.Pop(R1);
    }

    // Reset register set.
    regs = availableGeneralRegs(2);
    scratchReg = regs.takeAny();

    // Unbox key.
    Register key = masm.extractInt32(R1, ExtractTemp1);

    if (unboxedType_ == JSVAL_TYPE_MAGIC) {
        // Adding element to a native object.

        // Load obj->elements in scratchReg.
        masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratchReg);

        // Bounds check (key == initLength)
        Address initLength(scratchReg, ObjectElements::offsetOfInitializedLength());
        masm.branch32(Assembler::NotEqual, initLength, key, &failure);

        // Capacity check.
        Address capacity(scratchReg, ObjectElements::offsetOfCapacity());
        masm.branch32(Assembler::BelowOrEqual, capacity, key, &failure);

        // Check for copy on write elements.
        Address elementsFlags(scratchReg, ObjectElements::offsetOfFlags());
        masm.branchTest32(Assembler::NonZero, elementsFlags,
                          Imm32(ObjectElements::COPY_ON_WRITE),
                          &failure);

        // Failure is not possible now.  Free up registers.
        regs.add(R0);
        regs.add(R1);
        regs.takeUnchecked(obj);
        regs.takeUnchecked(key);

        // Increment initLength before write.
        masm.add32(Imm32(1), initLength);

        // If length is now <= key, increment length before write.
        Label skipIncrementLength;
        Address length(scratchReg, ObjectElements::offsetOfLength());
        masm.branch32(Assembler::Above, length, key, &skipIncrementLength);
        masm.add32(Imm32(1), length);
        masm.bind(&skipIncrementLength);

        // Convert int32 values to double if convertDoubleElements is set. In this
        // case the heap typeset is guaranteed to contain both int32 and double, so
        // it's okay to store a double.
        Label dontConvertDoubles;
        masm.branchTest32(Assembler::Zero, elementsFlags,
                          Imm32(ObjectElements::CONVERT_DOUBLE_ELEMENTS),
                          &dontConvertDoubles);

        Address valueAddr(masm.getStackPointer(), ICStackValueOffset);

        // Note that double arrays are only created by IonMonkey, so if we have no
        // floating-point support Ion is disabled and there should be no double arrays.
        if (cx->runtime()->jitSupportsFloatingPoint)
            masm.convertInt32ValueToDouble(valueAddr, regs.getAny(), &dontConvertDoubles);
        else
            masm.assumeUnreachable("There shouldn't be double arrays when there is no FP support.");
        masm.bind(&dontConvertDoubles);

        // Write the value.  No need for pre-barrier since we're not overwriting an old value.
        ValueOperand tmpVal = regs.takeAnyValue();
        BaseIndex element(scratchReg, key, TimesEight);
        masm.loadValue(valueAddr, tmpVal);
        masm.storeValue(tmpVal, element);
    } else {
        // Adding element to an unboxed array.

        // Bounds check (key == initLength)
        Address initLengthAddr(obj, UnboxedArrayObject::offsetOfCapacityIndexAndInitializedLength());
        masm.load32(initLengthAddr, scratchReg);
        masm.and32(Imm32(UnboxedArrayObject::InitializedLengthMask), scratchReg);
        masm.branch32(Assembler::NotEqual, scratchReg, key, &failure);

        // Capacity check.
        masm.checkUnboxedArrayCapacity(obj, Int32Key(key), scratchReg, &failure);

        // Load obj->elements.
        masm.loadPtr(Address(obj, UnboxedArrayObject::offsetOfElements()), scratchReg);

        // Write the value first, since this can fail. No need for pre-barrier
        // since we're not overwriting an old value.
        masm.Push(R0);
        Address valueAddr(masm.getStackPointer(), ICStackValueOffset + sizeof(Value));
        masm.loadValue(valueAddr, R0);
        BaseIndex address(scratchReg, key, ScaleFromElemWidth(UnboxedTypeSize(unboxedType_)));
        masm.storeUnboxedProperty(address, unboxedType_,
                                  ConstantOrRegister(TypedOrValueRegister(R0)), &failurePopR0);
        masm.Pop(R0);

        // Increment initialized length.
        masm.add32(Imm32(1), initLengthAddr);

        // If length is now <= key, increment length.
        Address lengthAddr(obj, UnboxedArrayObject::offsetOfLength());
        Label skipIncrementLength;
        masm.branch32(Assembler::Above, lengthAddr, key, &skipIncrementLength);
        masm.add32(Imm32(1), lengthAddr);
        masm.bind(&skipIncrementLength);
    }

    EmitReturnFromIC(masm);

    if (failurePopR0.used()) {
        masm.bind(&failurePopR0);
        masm.Pop(R0);
        masm.jump(&failure);
    }

    // Failure case - fail but first unstow R0 and R1
    masm.bind(&failureUnstow);
    EmitUnstowICValues(masm, 2);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// SetElem_TypedArray
//

// Write an arbitrary value to a typed array or typed object address at dest.
// If the value could not be converted to the appropriate format, jump to
// failure or failureModifiedScratch.
template <typename T>
static void
StoreToTypedArray(JSContext* cx, MacroAssembler& masm, Scalar::Type type, Address value, T dest,
                  Register scratch, Label* failure, Label* failureModifiedScratch)
{
    Label done;

    if (type == Scalar::Float32 || type == Scalar::Float64) {
        masm.ensureDouble(value, FloatReg0, failure);
        if (type == Scalar::Float32) {
            masm.convertDoubleToFloat32(FloatReg0, ScratchFloat32Reg);
            masm.storeToTypedFloatArray(type, ScratchFloat32Reg, dest);
        } else {
            masm.storeToTypedFloatArray(type, FloatReg0, dest);
        }
    } else if (type == Scalar::Uint8Clamped) {
        Label notInt32;
        masm.branchTestInt32(Assembler::NotEqual, value, &notInt32);
        masm.unboxInt32(value, scratch);
        masm.clampIntToUint8(scratch);

        Label clamped;
        masm.bind(&clamped);
        masm.storeToTypedIntArray(type, scratch, dest);
        masm.jump(&done);

        // If the value is a double, clamp to uint8 and jump back.
        // Else, jump to failure.
        masm.bind(&notInt32);
        if (cx->runtime()->jitSupportsFloatingPoint) {
            masm.branchTestDouble(Assembler::NotEqual, value, failure);
            masm.unboxDouble(value, FloatReg0);
            masm.clampDoubleToUint8(FloatReg0, scratch);
            masm.jump(&clamped);
        } else {
            masm.jump(failure);
        }
    } else {
        Label notInt32;
        masm.branchTestInt32(Assembler::NotEqual, value, &notInt32);
        masm.unboxInt32(value, scratch);

        Label isInt32;
        masm.bind(&isInt32);
        masm.storeToTypedIntArray(type, scratch, dest);
        masm.jump(&done);

        // If the value is a double, truncate and jump back.
        // Else, jump to failure.
        masm.bind(&notInt32);
        if (cx->runtime()->jitSupportsFloatingPoint) {
            masm.branchTestDouble(Assembler::NotEqual, value, failure);
            masm.unboxDouble(value, FloatReg0);
            masm.branchTruncateDouble(FloatReg0, scratch, failureModifiedScratch);
            masm.jump(&isInt32);
        } else {
            masm.jump(failure);
        }
    }

    masm.bind(&done);
}

bool
ICSetElem_TypedArray::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;

    if (layout_ != Layout_TypedArray)
        CheckForNeuteredTypedObject(cx, masm, &failure);

    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratchReg = regs.takeAny();

    // Unbox R0 and shape guard.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    masm.loadPtr(Address(ICStubReg, ICSetElem_TypedArray::offsetOfShape()), scratchReg);
    masm.branchTestObjShape(Assembler::NotEqual, obj, scratchReg, &failure);

    // Ensure the index is an integer.
    if (cx->runtime()->jitSupportsFloatingPoint) {
        Label isInt32;
        masm.branchTestInt32(Assembler::Equal, R1, &isInt32);
        {
            // If the index is a double, try to convert it to int32. It's okay
            // to convert -0 to 0: the shape check ensures the object is a typed
            // array so the difference is not observable.
            masm.branchTestDouble(Assembler::NotEqual, R1, &failure);
            masm.unboxDouble(R1, FloatReg0);
            masm.convertDoubleToInt32(FloatReg0, scratchReg, &failure, /* negZeroCheck = */false);
            masm.tagValue(JSVAL_TYPE_INT32, scratchReg, R1);
        }
        masm.bind(&isInt32);
    } else {
        masm.branchTestInt32(Assembler::NotEqual, R1, &failure);
    }

    // Unbox key.
    Register key = masm.extractInt32(R1, ExtractTemp1);

    // Bounds check.
    Label oobWrite;
    LoadTypedThingLength(masm, layout_, obj, scratchReg);
    masm.branch32(Assembler::BelowOrEqual, scratchReg, key,
                  expectOutOfBounds_ ? &oobWrite : &failure);

    // Load the elements vector.
    LoadTypedThingData(masm, layout_, obj, scratchReg);

    BaseIndex dest(scratchReg, key, ScaleFromElemWidth(Scalar::byteSize(type_)));
    Address value(masm.getStackPointer(), ICStackValueOffset);

    // We need a second scratch register. It's okay to clobber the type tag of
    // R0 or R1, as long as it's restored before jumping to the next stub.
    regs = availableGeneralRegs(0);
    regs.takeUnchecked(obj);
    regs.takeUnchecked(key);
    regs.take(scratchReg);
    Register secondScratch = regs.takeAny();

    Label failureModifiedSecondScratch;
    StoreToTypedArray(cx, masm, type_, value, dest,
                      secondScratch, &failure, &failureModifiedSecondScratch);
    EmitReturnFromIC(masm);

    if (failureModifiedSecondScratch.used()) {
        // Writing to secondScratch may have clobbered R0 or R1, restore them
        // first.
        masm.bind(&failureModifiedSecondScratch);
        masm.tagValue(JSVAL_TYPE_OBJECT, obj, R0);
        masm.tagValue(JSVAL_TYPE_INT32, key, R1);
    }

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    if (expectOutOfBounds_) {
        MOZ_ASSERT(layout_ == Layout_TypedArray);
        masm.bind(&oobWrite);
        EmitReturnFromIC(masm);
    }
    return true;
}

//
// In_Fallback
//

static bool
TryAttachDenseInStub(JSContext* cx, HandleScript script, ICIn_Fallback* stub,
                     HandleValue key, HandleObject obj, bool* attached)
{
    MOZ_ASSERT(!*attached);

    if (!IsNativeDenseElementAccess(obj, key))
        return true;

    JitSpew(JitSpew_BaselineIC, "  Generating In(Native[Int32] dense) stub");
    ICIn_Dense::Compiler compiler(cx, obj->as<NativeObject>().lastProperty());
    ICStub* denseStub = compiler.getStub(compiler.getStubSpace(script));
    if (!denseStub)
        return false;

    *attached = true;
    stub->addNewStub(denseStub);
    return true;
}

static bool
TryAttachNativeInStub(JSContext* cx, HandleScript script, ICIn_Fallback* stub,
                      HandleValue key, HandleObject obj, bool* attached)
{
    MOZ_ASSERT(!*attached);

    RootedId id(cx);
    if (!IsOptimizableElementPropertyName(cx, key, &id))
        return true;

    RootedPropertyName name(cx, JSID_TO_ATOM(id)->asPropertyName());
    RootedShape shape(cx);
    RootedObject holder(cx);
    if (!EffectlesslyLookupProperty(cx, obj, id, &holder, &shape))
        return false;

    if (IsCacheableGetPropReadSlot(obj, holder, shape)) {
        ICStub::Kind kind = (obj == holder) ? ICStub::In_Native
                                            : ICStub::In_NativePrototype;
        JitSpew(JitSpew_BaselineIC, "  Generating In(Native %s) stub",
                    (obj == holder) ? "direct" : "prototype");
        ICInNativeCompiler compiler(cx, kind, obj, holder, name);
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;

        *attached = true;
        stub->addNewStub(newStub);
        return true;
    }

    return true;
}

static bool
TryAttachNativeInDoesNotExistStub(JSContext* cx, HandleScript script,
                                  ICIn_Fallback* stub, HandleValue key,
                                  HandleObject obj, bool* attached)
{
    MOZ_ASSERT(!*attached);

    RootedId id(cx);
    if (!IsOptimizableElementPropertyName(cx, key, &id))
        return true;

    // Check if does-not-exist can be confirmed on property.
    RootedPropertyName name(cx, JSID_TO_ATOM(id)->asPropertyName());
    RootedObject lastProto(cx);
    size_t protoChainDepth = SIZE_MAX;
    if (!CheckHasNoSuchProperty(cx, obj, name, &lastProto, &protoChainDepth))
        return true;
    MOZ_ASSERT(protoChainDepth < SIZE_MAX);

    if (protoChainDepth > ICIn_NativeDoesNotExist::MAX_PROTO_CHAIN_DEPTH)
        return true;

    // Confirmed no-such-property. Add stub.
    JitSpew(JitSpew_BaselineIC, "  Generating In_NativeDoesNotExist stub");
    ICInNativeDoesNotExistCompiler compiler(cx, obj, name, protoChainDepth);
    ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
    if (!newStub)
        return false;

    *attached = true;
    stub->addNewStub(newStub);
    return true;
}

static bool
DoInFallback(JSContext* cx, BaselineFrame* frame, ICIn_Fallback* stub_,
             HandleValue key, HandleValue objValue, MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICIn_Fallback*> stub(frame, stub_);

    FallbackICSpew(cx, stub, "In");

    if (!objValue.isObject()) {
        ReportValueError(cx, JSMSG_IN_NOT_OBJECT, -1, objValue, nullptr);
        return false;
    }

    RootedObject obj(cx, &objValue.toObject());

    bool cond = false;
    if (!OperatorIn(cx, key, obj, &cond))
        return false;
    res.setBoolean(cond);

    if (stub.invalid())
        return true;

    if (stub->numOptimizedStubs() >= ICIn_Fallback::MAX_OPTIMIZED_STUBS)
        return true;

    if (obj->isNative()) {
        RootedScript script(cx, frame->script());
        bool attached = false;
        if (cond) {
            if (!TryAttachDenseInStub(cx, script, stub, key, obj, &attached))
                return false;
            if (attached)
                return true;
            if (!TryAttachNativeInStub(cx, script, stub, key, obj, &attached))
                return false;
            if (attached)
                return true;
        } else {
            if (!TryAttachNativeInDoesNotExistStub(cx, script, stub, key, obj, &attached))
                return false;
            if (attached)
                return true;
        }
    }

    return true;
}

typedef bool (*DoInFallbackFn)(JSContext*, BaselineFrame*, ICIn_Fallback*, HandleValue,
                               HandleValue, MutableHandleValue);
static const VMFunction DoInFallbackInfo =
    FunctionInfo<DoInFallbackFn>(DoInFallback, TailCall, PopValues(2));

bool
ICIn_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    EmitRestoreTailCallReg(masm);

    // Sync for the decompiler.
    masm.pushValue(R0);
    masm.pushValue(R1);

    // Push arguments.
    masm.pushValue(R1);
    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushFramePtr(masm, R0.scratchReg());

    return tailCallVM(DoInFallbackInfo, masm);
}

bool
ICInNativeCompiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure, failurePopR0Scratch;

    masm.branchTestString(Assembler::NotEqual, R0, &failure);
    masm.branchTestObject(Assembler::NotEqual, R1, &failure);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratch = regs.takeAny();

    // Check key identity.
    Register strExtract = masm.extractString(R0, ExtractTemp0);
    masm.loadPtr(Address(ICStubReg, ICInNativeStub::offsetOfName()), scratch);
    masm.branchPtr(Assembler::NotEqual, strExtract, scratch, &failure);

    // Unbox and shape guard object.
    Register objReg = masm.extractObject(R1, ExtractTemp0);
    masm.loadPtr(Address(ICStubReg, ICInNativeStub::offsetOfShape()), scratch);
    masm.branchTestObjShape(Assembler::NotEqual, objReg, scratch, &failure);

    if (kind == ICStub::In_NativePrototype) {
        // Shape guard holder. Use R0 scrachReg since on x86 there're not enough registers.
        Register holderReg = R0.scratchReg();
        masm.push(R0.scratchReg());
        masm.loadPtr(Address(ICStubReg, ICIn_NativePrototype::offsetOfHolder()),
                     holderReg);
        masm.loadPtr(Address(ICStubReg, ICIn_NativePrototype::offsetOfHolderShape()),
                     scratch);
        masm.branchTestObjShape(Assembler::NotEqual, holderReg, scratch, &failurePopR0Scratch);
        masm.addToStackPtr(Imm32(sizeof(size_t)));
    }

    masm.moveValue(BooleanValue(true), R0);

    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failurePopR0Scratch);
    masm.pop(R0.scratchReg());
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

ICStub*
ICInNativeDoesNotExistCompiler::getStub(ICStubSpace* space)
{
    Rooted<ShapeVector> shapes(cx, ShapeVector(cx));
    if (!shapes.append(obj_->as<NativeObject>().lastProperty()))
        return nullptr;

    if (!GetProtoShapes(obj_, protoChainDepth_, &shapes))
        return nullptr;

    JS_STATIC_ASSERT(ICIn_NativeDoesNotExist::MAX_PROTO_CHAIN_DEPTH == 8);

    ICStub* stub = nullptr;
    switch (protoChainDepth_) {
      case 0: stub = getStubSpecific<0>(space, shapes); break;
      case 1: stub = getStubSpecific<1>(space, shapes); break;
      case 2: stub = getStubSpecific<2>(space, shapes); break;
      case 3: stub = getStubSpecific<3>(space, shapes); break;
      case 4: stub = getStubSpecific<4>(space, shapes); break;
      case 5: stub = getStubSpecific<5>(space, shapes); break;
      case 6: stub = getStubSpecific<6>(space, shapes); break;
      case 7: stub = getStubSpecific<7>(space, shapes); break;
      case 8: stub = getStubSpecific<8>(space, shapes); break;
      default: MOZ_CRASH("ProtoChainDepth too high.");
    }
    if (!stub)
        return nullptr;
    return stub;
}

bool
ICInNativeDoesNotExistCompiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure, failurePopR0Scratch;

    masm.branchTestString(Assembler::NotEqual, R0, &failure);
    masm.branchTestObject(Assembler::NotEqual, R1, &failure);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratch = regs.takeAny();

#ifdef DEBUG
    // Ensure that protoChainDepth_ matches the protoChainDepth stored on the stub.
    {
        Label ok;
        masm.load16ZeroExtend(Address(ICStubReg, ICStub::offsetOfExtra()), scratch);
        masm.branch32(Assembler::Equal, scratch, Imm32(protoChainDepth_), &ok);
        masm.assumeUnreachable("Non-matching proto chain depth on stub.");
        masm.bind(&ok);
    }
#endif // DEBUG

    // Check key identity.
    Register strExtract = masm.extractString(R0, ExtractTemp0);
    masm.loadPtr(Address(ICStubReg, ICIn_NativeDoesNotExist::offsetOfName()), scratch);
    masm.branchPtr(Assembler::NotEqual, strExtract, scratch, &failure);

    // Unbox and guard against old shape.
    Register objReg = masm.extractObject(R1, ExtractTemp0);
    masm.loadPtr(Address(ICStubReg, ICIn_NativeDoesNotExist::offsetOfShape(0)),
                 scratch);
    masm.branchTestObjShape(Assembler::NotEqual, objReg, scratch, &failure);

    // Check the proto chain.
    Register protoReg = R0.scratchReg();
    masm.push(R0.scratchReg());
    for (size_t i = 0; i < protoChainDepth_; ++i) {
        masm.loadObjProto(i == 0 ? objReg : protoReg, protoReg);
        masm.branchTestPtr(Assembler::Zero, protoReg, protoReg, &failurePopR0Scratch);
        size_t shapeOffset = ICIn_NativeDoesNotExistImpl<0>::offsetOfShape(i + 1);
        masm.loadPtr(Address(ICStubReg, shapeOffset), scratch);
        masm.branchTestObjShape(Assembler::NotEqual, protoReg, scratch, &failurePopR0Scratch);
    }
    masm.addToStackPtr(Imm32(sizeof(size_t)));

    // Shape and type checks succeeded, ok to proceed.
    masm.moveValue(BooleanValue(false), R0);

    EmitReturnFromIC(masm);

    masm.bind(&failurePopR0Scratch);
    masm.pop(R0.scratchReg());
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICIn_Dense::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;

    masm.branchTestInt32(Assembler::NotEqual, R0, &failure);
    masm.branchTestObject(Assembler::NotEqual, R1, &failure);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratch = regs.takeAny();

    // Unbox and shape guard object.
    Register obj = masm.extractObject(R1, ExtractTemp0);
    masm.loadPtr(Address(ICStubReg, ICIn_Dense::offsetOfShape()), scratch);
    masm.branchTestObjShape(Assembler::NotEqual, obj, scratch, &failure);

    // Load obj->elements.
    masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);

    // Unbox key and bounds check.
    Address initLength(scratch, ObjectElements::offsetOfInitializedLength());
    Register key = masm.extractInt32(R0, ExtractTemp0);
    masm.branch32(Assembler::BelowOrEqual, initLength, key, &failure);

    // Hole check.
    JS_STATIC_ASSERT(sizeof(Value) == 8);
    BaseIndex element(scratch, key, TimesEight);
    masm.branchTestMagic(Assembler::Equal, element, &failure);

    masm.moveValue(BooleanValue(true), R0);

    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

// Try to update all existing GetProp/GetName getter call stubs that match the
// given holder in place with a new shape and getter.  fallbackStub can be
// either an ICGetProp_Fallback or an ICGetName_Fallback.
//
// If 'getter' is an own property, holder == receiver must be true.
static bool
UpdateExistingGetPropCallStubs(ICFallbackStub* fallbackStub,
                               ICStub::Kind kind,
                               HandleNativeObject holder,
                               HandleObject receiver,
                               HandleFunction getter)
{
    MOZ_ASSERT(kind == ICStub::GetProp_CallScripted ||
               kind == ICStub::GetProp_CallNative);
    MOZ_ASSERT(fallbackStub->isGetName_Fallback() ||
               fallbackStub->isGetProp_Fallback());
    MOZ_ASSERT(holder);
    MOZ_ASSERT(receiver);

    bool isOwnGetter = (holder == receiver);
    bool foundMatchingStub = false;
    ReceiverGuard receiverGuard(receiver);
    for (ICStubConstIterator iter = fallbackStub->beginChainConst(); !iter.atEnd(); iter++) {
        if (iter->kind() == kind) {
            ICGetPropCallGetter* getPropStub = static_cast<ICGetPropCallGetter*>(*iter);
            if (getPropStub->holder() == holder && getPropStub->isOwnGetter() == isOwnGetter) {
                // If this is an own getter, update the receiver guard as well,
                // since that's the shape we'll be guarding on. Furthermore,
                // isOwnGetter() relies on holderShape_ and receiverGuard_ being
                // the same shape.
                if (isOwnGetter)
                    getPropStub->receiverGuard().update(receiverGuard);

                MOZ_ASSERT(getPropStub->holderShape() != holder->lastProperty() ||
                           !getPropStub->receiverGuard().matches(receiverGuard),
                           "Why didn't we end up using this stub?");

                // We want to update the holder shape to match the new one no
                // matter what, even if the receiver shape is different.
                getPropStub->holderShape() = holder->lastProperty();

                // Make sure to update the getter, since a shape change might
                // have changed which getter we want to use.
                getPropStub->getter() = getter;

                if (getPropStub->receiverGuard().matches(receiverGuard))
                    foundMatchingStub = true;
            }
        }
    }

    return foundMatchingStub;
}

// Try to update existing SetProp setter call stubs for the given holder in
// place with a new shape and setter.
static bool
UpdateExistingSetPropCallStubs(ICSetProp_Fallback* fallbackStub,
                               ICStub::Kind kind,
                               NativeObject* holder,
                               JSObject* receiver,
                               JSFunction* setter)
{
    MOZ_ASSERT(kind == ICStub::SetProp_CallScripted ||
               kind == ICStub::SetProp_CallNative);
    MOZ_ASSERT(holder);
    MOZ_ASSERT(receiver);

    bool isOwnSetter = (holder == receiver);
    bool foundMatchingStub = false;
    ReceiverGuard receiverGuard(receiver);
    for (ICStubConstIterator iter = fallbackStub->beginChainConst(); !iter.atEnd(); iter++) {
        if (iter->kind() == kind) {
            ICSetPropCallSetter* setPropStub = static_cast<ICSetPropCallSetter*>(*iter);
            if (setPropStub->holder() == holder && setPropStub->isOwnSetter() == isOwnSetter) {
                // If this is an own setter, update the receiver guard as well,
                // since that's the shape we'll be guarding on. Furthermore,
                // isOwnSetter() relies on holderShape_ and receiverGuard_ being
                // the same shape.
                if (isOwnSetter)
                    setPropStub->receiverGuard().update(receiverGuard);

                MOZ_ASSERT(setPropStub->holderShape() != holder->lastProperty() ||
                           !setPropStub->receiverGuard().matches(receiverGuard),
                           "Why didn't we end up using this stub?");

                // We want to update the holder shape to match the new one no
                // matter what, even if the receiver shape is different.
                setPropStub->holderShape() = holder->lastProperty();

                // Make sure to update the setter, since a shape change might
                // have changed which setter we want to use.
                setPropStub->setter() = setter;
                if (setPropStub->receiverGuard().matches(receiverGuard))
                    foundMatchingStub = true;
            }
        }
    }

    return foundMatchingStub;
}

// Attach an optimized stub for a GETGNAME/CALLGNAME slot-read op.
static bool
TryAttachGlobalNameValueStub(JSContext* cx, HandleScript script, jsbytecode* pc,
                             ICGetName_Fallback* stub, Handle<GlobalObject*> global,
                             HandlePropertyName name, bool* attached)
{
    MOZ_ASSERT(global->is<GlobalObject>());
    MOZ_ASSERT(!*attached);

    RootedId id(cx, NameToId(name));

    // The property must be found, and it must be found as a normal data property.
    RootedShape shape(cx);
    RootedNativeObject current(cx, global);
    while (true) {
        shape = current->lookup(cx, id);
        if (shape)
            break;
        JSObject* proto = current->getProto();
        if (!proto || !proto->is<NativeObject>())
            return true;
        current = &proto->as<NativeObject>();
    }

    // Instantiate this global property, for use during Ion compilation.
    if (IsIonEnabled(cx))
        EnsureTrackPropertyTypes(cx, current, id);

    if (shape->hasDefaultGetter() && shape->hasSlot()) {

        // TODO: if there's a previous stub discard it, or just update its Shape + slot?

        ICStub* monitorStub = stub->fallbackMonitorStub()->firstMonitorStub();
        ICStub* newStub;
        if (current == global) {
            MOZ_ASSERT(shape->slot() >= current->numFixedSlots());
            uint32_t slot = shape->slot() - current->numFixedSlots();

            JitSpew(JitSpew_BaselineIC, "  Generating GetName(GlobalName) stub");
            ICGetName_Global::Compiler compiler(cx, monitorStub, current->lastProperty(), slot);
            newStub = compiler.getStub(compiler.getStubSpace(script));
        } else {
            bool isFixedSlot;
            uint32_t offset;
            GetFixedOrDynamicSlotOffset(shape, &isFixedSlot, &offset);
            if (!IsCacheableGetPropReadSlot(global, current, shape))
                return true;

            JitSpew(JitSpew_BaselineIC, "  Generating GetName(GlobalName prototype) stub");
            ICGetPropNativeCompiler compiler(cx, ICStub::GetProp_NativePrototype, false, monitorStub,
                                             global, current, name, isFixedSlot, offset,
                                             /* inputDefinitelyObject = */ true);
            newStub = compiler.getStub(compiler.getStubSpace(script));
        }
        if (!newStub)
            return false;

        stub->addNewStub(newStub);
        *attached = true;
    }
    return true;
}

// Attach an optimized stub for a GETGNAME/CALLGNAME getter op.
static bool
TryAttachGlobalNameAccessorStub(JSContext* cx, HandleScript script, jsbytecode* pc,
                                ICGetName_Fallback* stub, Handle<GlobalObject*> global,
                                HandlePropertyName name, bool* attached,
                                bool* isTemporarilyUnoptimizable)
{
    MOZ_ASSERT(global->is<GlobalObject>());

    RootedId id(cx, NameToId(name));

    // The property must be found, and it must be found as a normal data property.
    RootedShape shape(cx);
    RootedNativeObject current(cx, global);
    while (true) {
        shape = current->lookup(cx, id);
        if (shape)
            break;
        JSObject* proto = current->getProto();
        if (!proto || !proto->is<NativeObject>())
            return true;
        current = &proto->as<NativeObject>();
    }

    // Instantiate this global property, for use during Ion compilation.
    if (IsIonEnabled(cx))
        EnsureTrackPropertyTypes(cx, current, id);

    // Try to add a getter stub. We don't handle scripted getters yet; if this
    // changes we need to make sure IonBuilder::getPropTryCommonGetter (which
    // requires a Baseline stub) handles non-outerized this objects correctly.
    bool isScripted;
    if (IsCacheableGetPropCall(cx, global, current, shape, &isScripted, isTemporarilyUnoptimizable) &&
        !isScripted)
    {
        ICStub* monitorStub = stub->fallbackMonitorStub()->firstMonitorStub();
        RootedFunction getter(cx, &shape->getterObject()->as<JSFunction>());

        JitSpew(JitSpew_BaselineIC, "  Generating GetName(GlobalName/NativeGetter) stub");
        if (UpdateExistingGetPropCallStubs(stub, ICStub::GetProp_CallNative, current, global,
                                           getter))
        {
            *attached = true;
            return true;
        }
        ICGetProp_CallNative::Compiler compiler(cx, monitorStub, global, current,
                                                getter, script->pcToOffset(pc),
                                                /* outerClass = */ nullptr,
                                                /* inputDefinitelyObject = */ true);

        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;

        stub->addNewStub(newStub);
        *attached = true;
    }
    return true;
}

static bool
TryAttachScopeNameStub(JSContext* cx, HandleScript script, ICGetName_Fallback* stub,
                       HandleObject initialScopeChain, HandlePropertyName name, bool* attached)
{
    MOZ_ASSERT(!*attached);

    Rooted<ShapeVector> shapes(cx, ShapeVector(cx));
    RootedId id(cx, NameToId(name));
    RootedObject scopeChain(cx, initialScopeChain);

    Shape* shape = nullptr;
    while (scopeChain) {
        if (!shapes.append(scopeChain->maybeShape()))
            return false;

        if (scopeChain->is<GlobalObject>()) {
            shape = scopeChain->as<GlobalObject>().lookup(cx, id);
            if (shape)
                break;
            return true;
        }

        if (!scopeChain->is<ScopeObject>() || scopeChain->is<DynamicWithObject>())
            return true;

        // Check for an 'own' property on the scope. There is no need to
        // check the prototype as non-with scopes do not inherit properties
        // from any prototype.
        shape = scopeChain->as<NativeObject>().lookup(cx, id);
        if (shape)
            break;

        scopeChain = scopeChain->enclosingScope();
    }

    // We don't handle getters here. When this changes, we need to make sure
    // IonBuilder::getPropTryCommonGetter (which requires a Baseline stub to
    // work) handles non-outerized this objects correctly.

    if (!IsCacheableGetPropReadSlot(scopeChain, scopeChain, shape))
        return true;

    bool isFixedSlot;
    uint32_t offset;
    GetFixedOrDynamicSlotOffset(shape, &isFixedSlot, &offset);

    ICStub* monitorStub = stub->fallbackMonitorStub()->firstMonitorStub();
    ICStub* newStub;

    switch (shapes.length()) {
      case 1: {
        ICGetName_Scope<0>::Compiler compiler(cx, monitorStub, Move(shapes.get()), isFixedSlot,
                                              offset);
        newStub = compiler.getStub(compiler.getStubSpace(script));
        break;
      }
      case 2: {
        ICGetName_Scope<1>::Compiler compiler(cx, monitorStub, Move(shapes.get()), isFixedSlot,
                                              offset);
        newStub = compiler.getStub(compiler.getStubSpace(script));
        break;
      }
      case 3: {
        ICGetName_Scope<2>::Compiler compiler(cx, monitorStub, Move(shapes.get()), isFixedSlot,
                                              offset);
        newStub = compiler.getStub(compiler.getStubSpace(script));
        break;
      }
      case 4: {
        ICGetName_Scope<3>::Compiler compiler(cx, monitorStub, Move(shapes.get()), isFixedSlot,
                                              offset);
        newStub = compiler.getStub(compiler.getStubSpace(script));
        break;
      }
      case 5: {
        ICGetName_Scope<4>::Compiler compiler(cx, monitorStub, Move(shapes.get()), isFixedSlot,
                                              offset);
        newStub = compiler.getStub(compiler.getStubSpace(script));
        break;
      }
      case 6: {
        ICGetName_Scope<5>::Compiler compiler(cx, monitorStub, Move(shapes.get()), isFixedSlot,
                                              offset);
        newStub = compiler.getStub(compiler.getStubSpace(script));
        break;
      }
      case 7: {
        ICGetName_Scope<6>::Compiler compiler(cx, monitorStub, Move(shapes.get()), isFixedSlot,
                                              offset);
        newStub = compiler.getStub(compiler.getStubSpace(script));
        break;
      }
      default:
        return true;
    }

    if (!newStub)
        return false;

    stub->addNewStub(newStub);
    *attached = true;
    return true;
}

static bool
DoGetNameFallback(JSContext* cx, BaselineFrame* frame, ICGetName_Fallback* stub_,
                  HandleObject scopeChain, MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICGetName_Fallback*> stub(frame, stub_);

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    mozilla::DebugOnly<JSOp> op = JSOp(*pc);
    FallbackICSpew(cx, stub, "GetName(%s)", js_CodeName[JSOp(*pc)]);

    MOZ_ASSERT(op == JSOP_GETNAME || op == JSOP_GETGNAME);

    RootedPropertyName name(cx, script->getName(pc));
    bool attached = false;
    bool isTemporarilyUnoptimizable = false;

    // Attach new stub.
    if (stub->numOptimizedStubs() >= ICGetName_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard all stubs in this IC and replace with generic stub.
        attached = true;
    }

    if (!attached && IsGlobalOp(JSOp(*pc)) && !script->hasNonSyntacticScope()) {
        if (!TryAttachGlobalNameAccessorStub(cx, script, pc, stub, scopeChain.as<GlobalObject>(),
            name, &attached, &isTemporarilyUnoptimizable))
        {
            return false;
        }
    }

    static_assert(JSOP_GETGNAME_LENGTH == JSOP_GETNAME_LENGTH,
                  "Otherwise our check for JSOP_TYPEOF isn't ok");
    if (JSOp(pc[JSOP_GETGNAME_LENGTH]) == JSOP_TYPEOF) {
        if (!GetScopeNameForTypeOf(cx, scopeChain, name, res))
            return false;
    } else {
        if (!GetScopeName(cx, scopeChain, name, res))
            return false;
    }

    TypeScript::Monitor(cx, script, pc, res);

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid())
        return true;

    // Add a type monitor stub for the resulting value.
    if (!stub->addMonitorStubForValue(cx, script, res))
        return false;
    if (attached)
        return true;

    if (IsGlobalOp(JSOp(*pc)) && !script->hasNonSyntacticScope()) {
        Handle<GlobalObject*> global = scopeChain.as<GlobalObject>();
        if (!TryAttachGlobalNameValueStub(cx, script, pc, stub, global, name, &attached))
            return false;
    } else {
        if (!TryAttachScopeNameStub(cx, script, stub, scopeChain, name, &attached))
            return false;
    }

    if (!attached && !isTemporarilyUnoptimizable)
        stub->noteUnoptimizableAccess();
    return true;
}

typedef bool (*DoGetNameFallbackFn)(JSContext*, BaselineFrame*, ICGetName_Fallback*,
                                    HandleObject, MutableHandleValue);
static const VMFunction DoGetNameFallbackInfo = FunctionInfo<DoGetNameFallbackFn>(DoGetNameFallback, TailCall);

bool
ICGetName_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);
    MOZ_ASSERT(R0 == JSReturnOperand);

    EmitRestoreTailCallReg(masm);

    masm.push(R0.scratchReg());
    masm.push(ICStubReg);
    pushFramePtr(masm, R0.scratchReg());

    return tailCallVM(DoGetNameFallbackInfo, masm);
}

bool
ICGetName_Global::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    Register obj = R0.scratchReg();
    Register scratch = R1.scratchReg();

    // Shape guard.
    masm.loadPtr(Address(ICStubReg, ICGetName_Global::offsetOfShape()), scratch);
    masm.branchTestObjShape(Assembler::NotEqual, obj, scratch, &failure);

    // Load dynamic slot.
    masm.loadPtr(Address(obj, NativeObject::offsetOfSlots()), obj);
    masm.load32(Address(ICStubReg, ICGetName_Global::offsetOfSlot()), scratch);
    masm.loadValue(BaseIndex(obj, scratch, TimesEight), R0);

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

template <size_t NumHops>
bool
ICGetName_Scope<NumHops>::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(1));
    Register obj = R0.scratchReg();
    Register walker = regs.takeAny();
    Register scratch = regs.takeAny();

    // Use a local to silence Clang tautological-compare warning if NumHops is 0.
    size_t numHops = NumHops;

    for (size_t index = 0; index < NumHops + 1; index++) {
        Register scope = index ? walker : obj;

        // Shape guard.
        masm.loadPtr(Address(ICStubReg, ICGetName_Scope::offsetOfShape(index)), scratch);
        masm.branchTestObjShape(Assembler::NotEqual, scope, scratch, &failure);

        if (index < numHops)
            masm.extractObject(Address(scope, ScopeObject::offsetOfEnclosingScope()), walker);
    }

    Register scope = NumHops ? walker : obj;

    if (!isFixedSlot_) {
        masm.loadPtr(Address(scope, NativeObject::offsetOfSlots()), walker);
        scope = walker;
    }

    masm.load32(Address(ICStubReg, ICGetName_Scope::offsetOfOffset()), scratch);
    masm.loadValue(BaseIndex(scope, scratch, TimesOne), R0);

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// BindName_Fallback
//

static bool
DoBindNameFallback(JSContext* cx, BaselineFrame* frame, ICBindName_Fallback* stub,
                   HandleObject scopeChain, MutableHandleValue res)
{
    jsbytecode* pc = stub->icEntry()->pc(frame->script());
    mozilla::DebugOnly<JSOp> op = JSOp(*pc);
    FallbackICSpew(cx, stub, "BindName(%s)", js_CodeName[JSOp(*pc)]);

    MOZ_ASSERT(op == JSOP_BINDNAME || op == JSOP_BINDGNAME);

    RootedPropertyName name(cx, frame->script()->getName(pc));

    RootedObject scope(cx);
    if (!LookupNameUnqualified(cx, name, scopeChain, &scope))
        return false;

    res.setObject(*scope);
    return true;
}

typedef bool (*DoBindNameFallbackFn)(JSContext*, BaselineFrame*, ICBindName_Fallback*,
                                     HandleObject, MutableHandleValue);
static const VMFunction DoBindNameFallbackInfo =
    FunctionInfo<DoBindNameFallbackFn>(DoBindNameFallback, TailCall);

bool
ICBindName_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);
    MOZ_ASSERT(R0 == JSReturnOperand);

    EmitRestoreTailCallReg(masm);

    masm.push(R0.scratchReg());
    masm.push(ICStubReg);
    pushFramePtr(masm, R0.scratchReg());

    return tailCallVM(DoBindNameFallbackInfo, masm);
}

//
// GetIntrinsic_Fallback
//

static bool
DoGetIntrinsicFallback(JSContext* cx, BaselineFrame* frame, ICGetIntrinsic_Fallback* stub_,
                       MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICGetIntrinsic_Fallback*> stub(frame, stub_);

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    mozilla::DebugOnly<JSOp> op = JSOp(*pc);
    FallbackICSpew(cx, stub, "GetIntrinsic(%s)", js_CodeName[JSOp(*pc)]);

    MOZ_ASSERT(op == JSOP_GETINTRINSIC);

    if (!GetIntrinsicOperation(cx, pc, res))
        return false;

    // An intrinsic operation will always produce the same result, so only
    // needs to be monitored once. Attach a stub to load the resulting constant
    // directly.

    TypeScript::Monitor(cx, script, pc, res);

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid())
        return true;

    JitSpew(JitSpew_BaselineIC, "  Generating GetIntrinsic optimized stub");
    ICGetIntrinsic_Constant::Compiler compiler(cx, res);
    ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
    if (!newStub)
        return false;

    stub->addNewStub(newStub);
    return true;
}

typedef bool (*DoGetIntrinsicFallbackFn)(JSContext*, BaselineFrame*, ICGetIntrinsic_Fallback*,
                                         MutableHandleValue);
static const VMFunction DoGetIntrinsicFallbackInfo =
    FunctionInfo<DoGetIntrinsicFallbackFn>(DoGetIntrinsicFallback, TailCall);

bool
ICGetIntrinsic_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    EmitRestoreTailCallReg(masm);

    masm.push(ICStubReg);
    pushFramePtr(masm, R0.scratchReg());

    return tailCallVM(DoGetIntrinsicFallbackInfo, masm);
}

bool
ICGetIntrinsic_Constant::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    masm.loadValue(Address(ICStubReg, ICGetIntrinsic_Constant::offsetOfValue()), R0);

    EmitReturnFromIC(masm);
    return true;
}

//
// GetProp_Fallback
//

static bool
TryAttachMagicArgumentsGetPropStub(JSContext* cx, JSScript* script, ICGetProp_Fallback* stub,
                                   HandlePropertyName name, HandleValue val, HandleValue res,
                                   bool* attached)
{
    MOZ_ASSERT(!*attached);

    if (!val.isMagic(JS_OPTIMIZED_ARGUMENTS))
        return true;

    // Try handling arguments.callee on optimized arguments.
    if (name == cx->names().callee) {
        MOZ_ASSERT(script->hasMappedArgsObj());

        JitSpew(JitSpew_BaselineIC, "  Generating GetProp(MagicArgs.callee) stub");

        // Unlike ICGetProp_ArgumentsLength, only magic argument stubs are
        // supported at the moment.
        ICStub* monitorStub = stub->fallbackMonitorStub()->firstMonitorStub();
        ICGetProp_ArgumentsCallee::Compiler compiler(cx, monitorStub);
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;
        stub->addNewStub(newStub);

        *attached = true;
        return true;
    }

    return true;
}

static bool
TryAttachLengthStub(JSContext* cx, JSScript* script, ICGetProp_Fallback* stub, HandleValue val,
                    HandleValue res, bool* attached)
{
    MOZ_ASSERT(!*attached);

    if (val.isString()) {
        MOZ_ASSERT(res.isInt32());
        JitSpew(JitSpew_BaselineIC, "  Generating GetProp(String.length) stub");
        ICGetProp_StringLength::Compiler compiler(cx);
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;

        *attached = true;
        stub->addNewStub(newStub);
        return true;
    }

    if (val.isMagic(JS_OPTIMIZED_ARGUMENTS) && res.isInt32()) {
        JitSpew(JitSpew_BaselineIC, "  Generating GetProp(MagicArgs.length) stub");
        ICGetProp_ArgumentsLength::Compiler compiler(cx, ICGetProp_ArgumentsLength::Magic);
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;

        *attached = true;
        stub->addNewStub(newStub);
        return true;
    }

    if (!val.isObject())
        return true;

    RootedObject obj(cx, &val.toObject());

    if (obj->is<ArrayObject>() && res.isInt32()) {
        JitSpew(JitSpew_BaselineIC, "  Generating GetProp(Array.length) stub");
        ICGetProp_ArrayLength::Compiler compiler(cx);
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;

        *attached = true;
        stub->addNewStub(newStub);
        return true;
    }

    if (obj->is<UnboxedArrayObject>() && res.isInt32()) {
        JitSpew(JitSpew_BaselineIC, "  Generating GetProp(UnboxedArray.length) stub");
        ICGetProp_UnboxedArrayLength::Compiler compiler(cx);
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;

        *attached = true;
        stub->addNewStub(newStub);
        return true;
    }

    if (obj->is<ArgumentsObject>() && res.isInt32()) {
        JitSpew(JitSpew_BaselineIC, "  Generating GetProp(ArgsObj.length %s) stub",
                obj->is<MappedArgumentsObject>() ? "Mapped" : "Unmapped");
        ICGetProp_ArgumentsLength::Which which = ICGetProp_ArgumentsLength::Mapped;
        if (obj->is<UnmappedArgumentsObject>())
            which = ICGetProp_ArgumentsLength::Unmapped;
        ICGetProp_ArgumentsLength::Compiler compiler(cx, which);
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;

        *attached = true;
        stub->addNewStub(newStub);
        return true;
    }

    return true;
}

static bool
UpdateExistingGenerationalDOMProxyStub(ICGetProp_Fallback* stub,
                                       HandleObject obj)
{
    Value expandoSlot = GetProxyExtra(obj, GetDOMProxyExpandoSlot());
    MOZ_ASSERT(!expandoSlot.isObject() && !expandoSlot.isUndefined());
    ExpandoAndGeneration* expandoAndGeneration = (ExpandoAndGeneration*)expandoSlot.toPrivate();
    for (ICStubConstIterator iter = stub->beginChainConst(); !iter.atEnd(); iter++) {
        if (iter->isGetProp_CallDOMProxyWithGenerationNative()) {
            ICGetProp_CallDOMProxyWithGenerationNative* updateStub =
                iter->toGetProp_CallDOMProxyWithGenerationNative();
            if (updateStub->expandoAndGeneration() == expandoAndGeneration) {
                // Update generation
                uint32_t generation = expandoAndGeneration->generation;
                JitSpew(JitSpew_BaselineIC,
                        "  Updating existing stub with generation, old value: %i, "
                        "new value: %i", updateStub->generation(),
                        generation);
                updateStub->setGeneration(generation);
                return true;
            }
        }
    }
    return false;
}

// Return whether obj is in some PreliminaryObjectArray and has a structure
// that might change in the future.
static bool
IsPreliminaryObject(JSObject* obj)
{
    if (obj->isSingleton())
        return false;

    TypeNewScript* newScript = obj->group()->newScript();
    if (newScript && !newScript->analyzed())
        return true;

    if (obj->group()->maybePreliminaryObjects())
        return true;

    return false;
}

static void
StripPreliminaryObjectStubs(JSContext* cx, ICFallbackStub* stub)
{
    // Before the new script properties analysis has been performed on a type,
    // all instances of that type have the maximum number of fixed slots.
    // Afterwards, the objects (even the preliminary ones) might be changed
    // to reduce the number of fixed slots they have. If we generate stubs for
    // both the old and new number of fixed slots, the stub will look
    // polymorphic to IonBuilder when it is actually monomorphic. To avoid
    // this, strip out any stubs for preliminary objects before attaching a new
    // stub which isn't on a preliminary object.

    for (ICStubIterator iter = stub->beginChain(); !iter.atEnd(); iter++) {
        if (iter->isGetProp_Native() && iter->toGetProp_Native()->hasPreliminaryObject())
            iter.unlink(cx);
        else if (iter->isSetProp_Native() && iter->toSetProp_Native()->hasPreliminaryObject())
            iter.unlink(cx);
    }
}

static bool
TryAttachNativeGetValuePropStub(JSContext* cx, HandleScript script, jsbytecode* pc,
                                ICGetProp_Fallback* stub, HandlePropertyName name,
                                HandleValue val, HandleShape oldShape,
                                HandleValue res, bool* attached)
{
    MOZ_ASSERT(!*attached);

    if (!val.isObject())
        return true;

    RootedObject obj(cx, &val.toObject());

    if (obj->isNative() && oldShape != obj->as<NativeObject>().lastProperty()) {
        // No point attaching anything, since we know the shape guard will fail
        return true;
    }

    RootedShape shape(cx);
    RootedObject holder(cx);
    RootedId id(cx, NameToId(name));
    if (!EffectlesslyLookupProperty(cx, obj, id, &holder, &shape))
        return false;

    bool isCallProp = (JSOp(*pc) == JSOP_CALLPROP);

    ICStub* monitorStub = stub->fallbackMonitorStub()->firstMonitorStub();
    if (IsCacheableGetPropReadSlot(obj, holder, shape)) {
        bool isFixedSlot;
        uint32_t offset;
        GetFixedOrDynamicSlotOffset(shape, &isFixedSlot, &offset);

        // Instantiate this property for singleton holders, for use during Ion compilation.
        if (IsIonEnabled(cx))
            EnsureTrackPropertyTypes(cx, holder, NameToId(name));

        ICStub::Kind kind =
            (obj == holder) ? ICStub::GetProp_Native : ICStub::GetProp_NativePrototype;

        JitSpew(JitSpew_BaselineIC, "  Generating GetProp(Native %s) stub",
                    (obj == holder) ? "direct" : "prototype");
        ICGetPropNativeCompiler compiler(cx, kind, isCallProp, monitorStub, obj, holder,
                                         name, isFixedSlot, offset);
        ICGetPropNativeStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;

        if (IsPreliminaryObject(obj))
            newStub->notePreliminaryObject();
        else
            StripPreliminaryObjectStubs(cx, stub);

        stub->addNewStub(newStub);
        *attached = true;
        return true;
    }
    return true;
}


static bool
TryAttachNativeGetAccessorPropStub(JSContext* cx, HandleScript script, jsbytecode* pc,
                                   ICGetProp_Fallback* stub, HandlePropertyName name,
                                   HandleValue val, HandleValue res, bool* attached,
                                   bool* isTemporarilyUnoptimizable)
{
    MOZ_ASSERT(!*attached);
    MOZ_ASSERT(!*isTemporarilyUnoptimizable);

    if (!val.isObject())
        return true;

    RootedObject obj(cx, &val.toObject());

    bool isDOMProxy;
    bool domProxyHasGeneration;
    DOMProxyShadowsResult domProxyShadowsResult;
    RootedShape shape(cx);
    RootedObject holder(cx);
    RootedId id(cx, NameToId(name));
    if (!EffectlesslyLookupProperty(cx, obj, id, &holder, &shape, &isDOMProxy,
                                    &domProxyShadowsResult, &domProxyHasGeneration))
    {
        return false;
    }

    bool isCallProp = (JSOp(*pc) == JSOP_CALLPROP);

    ICStub* monitorStub = stub->fallbackMonitorStub()->firstMonitorStub();

    bool isScripted = false;
    bool cacheableCall = IsCacheableGetPropCall(cx, obj, holder, shape, &isScripted,
                                                isTemporarilyUnoptimizable);

    // Try handling scripted getters.
    if (cacheableCall && isScripted && !isDOMProxy) {
#if JS_HAS_NO_SUCH_METHOD
        // It's hard to keep the original object alive through a call, and it's unlikely
        // that a getter will be used to generate functions for calling in CALLPROP locations.
        // Just don't attach stubs in that case.
        if (isCallProp)
            return true;
#endif

        RootedFunction callee(cx, &shape->getterObject()->as<JSFunction>());
        MOZ_ASSERT(callee->hasScript());

        if (UpdateExistingGetPropCallStubs(stub, ICStub::GetProp_CallScripted,
                                           holder.as<NativeObject>(), obj, callee)) {
            *attached = true;
            return true;
        }

        JitSpew(JitSpew_BaselineIC, "  Generating GetProp(NativeObj/ScriptedGetter %s:%" PRIuSIZE ") stub",
                callee->nonLazyScript()->filename(), callee->nonLazyScript()->lineno());

        ICGetProp_CallScripted::Compiler compiler(cx, monitorStub, obj, holder, callee,
                                                  script->pcToOffset(pc));
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;

        stub->addNewStub(newStub);
        *attached = true;
        return true;
    }

    // If it's a shadowed listbase proxy property, attach stub to call Proxy::get instead.
    if (isDOMProxy && DOMProxyIsShadowing(domProxyShadowsResult)) {
        MOZ_ASSERT(obj == holder);
#if JS_HAS_NO_SUCH_METHOD
        if (isCallProp)
            return true;
#endif

        JitSpew(JitSpew_BaselineIC, "  Generating GetProp(DOMProxyProxy) stub");
        Rooted<ProxyObject*> proxy(cx, &obj->as<ProxyObject>());
        ICGetProp_DOMProxyShadowed::Compiler compiler(cx, monitorStub, proxy, name,
                                                      script->pcToOffset(pc));
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;
        stub->addNewStub(newStub);
        *attached = true;
        return true;
    }

    const Class* outerClass = nullptr;
    if (!isDOMProxy && !obj->isNative()) {
        outerClass = obj->getClass();
        DebugOnly<JSObject*> outer = obj.get();
        obj = GetInnerObject(obj);
        MOZ_ASSERT(script->global().isNative());
        if (obj != &script->global())
            return true;
        // ICGetProp_CallNative*::Compiler::generateStubCode depends on this.
        MOZ_ASSERT(&((GetProxyDataLayout(outer)->values->privateSlot).toObject()) == obj);
        if (!EffectlesslyLookupProperty(cx, obj, id, &holder, &shape, &isDOMProxy,
                                        &domProxyShadowsResult, &domProxyHasGeneration))
        {
            return false;
        }
        cacheableCall = IsCacheableGetPropCall(cx, obj, holder, shape, &isScripted,
                                               isTemporarilyUnoptimizable, isDOMProxy);
    }

    // Try handling JSNative getters.
    if (!cacheableCall || isScripted)
        return true;

    if (!shape || !shape->hasGetterValue() || !shape->getterValue().isObject() ||
        !shape->getterObject()->is<JSFunction>())
    {
        return true;
    }

    RootedFunction callee(cx, &shape->getterObject()->as<JSFunction>());
    MOZ_ASSERT(callee->isNative());

    if (outerClass && (!callee->jitInfo() || callee->jitInfo()->needsOuterizedThisObject()))
        return true;

#if JS_HAS_NO_SUCH_METHOD
    // It's unlikely that a getter function will be used to generate functions for calling
    // in CALLPROP locations.  Just don't attach stubs in that case to avoid issues with
    // __noSuchMethod__ handling.
    if (isCallProp)
        return true;
#endif

    JitSpew(JitSpew_BaselineIC, "  Generating GetProp(%s%s/NativeGetter %p) stub",
            isDOMProxy ? "DOMProxyObj" : "NativeObj",
            isDOMProxy && domProxyHasGeneration ? "WithGeneration" : "",
            callee->native());

    ICStub* newStub = nullptr;
    if (isDOMProxy) {
        MOZ_ASSERT(obj != holder);
        ICStub::Kind kind;
        if (domProxyHasGeneration) {
            if (UpdateExistingGenerationalDOMProxyStub(stub, obj)) {
                *attached = true;
                return true;
            }
            kind = ICStub::GetProp_CallDOMProxyWithGenerationNative;
        } else {
            kind = ICStub::GetProp_CallDOMProxyNative;
        }
        Rooted<ProxyObject*> proxy(cx, &obj->as<ProxyObject>());
        ICGetPropCallDOMProxyNativeCompiler compiler(cx, kind, monitorStub, proxy, holder, callee,
                                                     script->pcToOffset(pc));
        newStub = compiler.getStub(compiler.getStubSpace(script));
    } else {
        if (UpdateExistingGetPropCallStubs(stub, ICStub::GetProp_CallNative,
                                           holder.as<NativeObject>(), obj, callee))
        {
            *attached = true;
            return true;
        }

        ICGetProp_CallNative::Compiler compiler(cx, monitorStub, obj, holder, callee,
                                                script->pcToOffset(pc), outerClass);
        newStub = compiler.getStub(compiler.getStubSpace(script));
    }
    if (!newStub)
        return false;
    stub->addNewStub(newStub);
    *attached = true;
    return true;
}

static bool
TryAttachUnboxedGetPropStub(JSContext* cx, HandleScript script,
                            ICGetProp_Fallback* stub, HandlePropertyName name, HandleValue val,
                            bool* attached)
{
    MOZ_ASSERT(!*attached);

    if (!cx->runtime()->jitSupportsFloatingPoint)
        return true;

    if (!val.isObject() || !val.toObject().is<UnboxedPlainObject>())
        return true;
    Rooted<UnboxedPlainObject*> obj(cx, &val.toObject().as<UnboxedPlainObject>());

    const UnboxedLayout::Property* property = obj->layout().lookup(name);
    if (!property)
        return true;

    ICStub* monitorStub = stub->fallbackMonitorStub()->firstMonitorStub();

    ICGetProp_Unboxed::Compiler compiler(cx, monitorStub, obj->group(),
                                         property->offset + UnboxedPlainObject::offsetOfData(),
                                         property->type);
    ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
    if (!newStub)
        return false;
    stub->addNewStub(newStub);

    StripPreliminaryObjectStubs(cx, stub);

    *attached = true;
    return true;
}

static bool
TryAttachUnboxedExpandoGetPropStub(JSContext* cx, HandleScript script, jsbytecode* pc,
                                   ICGetProp_Fallback* stub, HandlePropertyName name, HandleValue val,
                                   bool* attached)
{
    MOZ_ASSERT(!*attached);

    if (!val.isObject() || !val.toObject().is<UnboxedPlainObject>())
        return true;
    Rooted<UnboxedPlainObject*> obj(cx, &val.toObject().as<UnboxedPlainObject>());

    Rooted<UnboxedExpandoObject*> expando(cx, obj->maybeExpando());
    if (!expando)
        return true;

    Shape* shape = expando->lookup(cx, name);
    if (!shape || !shape->hasDefaultGetter() || !shape->hasSlot())
        return true;

    bool isFixedSlot;
    uint32_t offset;
    GetFixedOrDynamicSlotOffset(shape, &isFixedSlot, &offset);

    ICStub* monitorStub = stub->fallbackMonitorStub()->firstMonitorStub();

    bool isCallProp = (JSOp(*pc) == JSOP_CALLPROP);
    ICGetPropNativeCompiler compiler(cx, ICStub::GetProp_Native, isCallProp, monitorStub, obj, obj,
                                     name, isFixedSlot, offset);
    ICGetPropNativeStub* newStub = compiler.getStub(compiler.getStubSpace(script));
    if (!newStub)
        return false;

    StripPreliminaryObjectStubs(cx, stub);

    stub->addNewStub(newStub);
    *attached = true;
    return true;
}

static bool
TryAttachTypedObjectGetPropStub(JSContext* cx, HandleScript script,
                                ICGetProp_Fallback* stub, HandlePropertyName name, HandleValue val,
                                bool* attached)
{
    MOZ_ASSERT(!*attached);

    if (!cx->runtime()->jitSupportsFloatingPoint)
        return true;

    if (!val.isObject() || !val.toObject().is<TypedObject>())
        return true;
    Rooted<TypedObject*> obj(cx, &val.toObject().as<TypedObject>());

    if (!obj->typeDescr().is<StructTypeDescr>())
        return true;
    Rooted<StructTypeDescr*> structDescr(cx, &obj->typeDescr().as<StructTypeDescr>());

    size_t fieldIndex;
    if (!structDescr->fieldIndex(NameToId(name), &fieldIndex))
        return true;

    Rooted<TypeDescr*> fieldDescr(cx, &structDescr->fieldDescr(fieldIndex));
    if (!fieldDescr->is<SimpleTypeDescr>())
        return true;

    uint32_t fieldOffset = structDescr->fieldOffset(fieldIndex);
    ICStub* monitorStub = stub->fallbackMonitorStub()->firstMonitorStub();

    ICGetProp_TypedObject::Compiler compiler(cx, monitorStub, obj->maybeShape(),
                                             fieldOffset, &fieldDescr->as<SimpleTypeDescr>());
    ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
    if (!newStub)
        return false;
    stub->addNewStub(newStub);

    *attached = true;
    return true;
}

static bool
TryAttachPrimitiveGetPropStub(JSContext* cx, HandleScript script, jsbytecode* pc,
                              ICGetProp_Fallback* stub, HandlePropertyName name, HandleValue val,
                              HandleValue res, bool* attached)
{
    MOZ_ASSERT(!*attached);

    JSValueType primitiveType;
    RootedNativeObject proto(cx);
    Rooted<GlobalObject*> global(cx, &script->global());
    if (val.isString()) {
        primitiveType = JSVAL_TYPE_STRING;
        proto = GlobalObject::getOrCreateStringPrototype(cx, global);
    } else if (val.isSymbol()) {
        primitiveType = JSVAL_TYPE_SYMBOL;
        proto = GlobalObject::getOrCreateSymbolPrototype(cx, global);
    } else if (val.isNumber()) {
        primitiveType = JSVAL_TYPE_DOUBLE;
        proto = GlobalObject::getOrCreateNumberPrototype(cx, global);
    } else {
        MOZ_ASSERT(val.isBoolean());
        primitiveType = JSVAL_TYPE_BOOLEAN;
        proto = GlobalObject::getOrCreateBooleanPrototype(cx, global);
    }
    if (!proto)
        return false;

    // Instantiate this property, for use during Ion compilation.
    RootedId id(cx, NameToId(name));
    if (IsIonEnabled(cx))
        EnsureTrackPropertyTypes(cx, proto, id);

    // For now, only look for properties directly set on the prototype.
    RootedShape shape(cx, proto->lookup(cx, id));
    if (!shape || !shape->hasSlot() || !shape->hasDefaultGetter())
        return true;

    bool isFixedSlot;
    uint32_t offset;
    GetFixedOrDynamicSlotOffset(shape, &isFixedSlot, &offset);

    ICStub* monitorStub = stub->fallbackMonitorStub()->firstMonitorStub();

    JitSpew(JitSpew_BaselineIC, "  Generating GetProp_Primitive stub");
    ICGetProp_Primitive::Compiler compiler(cx, monitorStub, primitiveType, proto,
                                           isFixedSlot, offset);
    ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
    if (!newStub)
        return false;

    stub->addNewStub(newStub);
    *attached = true;
    return true;
}

static bool
TryAttachNativeGetPropDoesNotExistStub(JSContext* cx, HandleScript script,
                                       jsbytecode* pc, ICGetProp_Fallback* stub,
                                       HandlePropertyName name, HandleValue val,
                                       bool* attached)
{
    MOZ_ASSERT(!*attached);

    if (!val.isObject())
        return true;

    RootedObject obj(cx, &val.toObject());

    // Don't attach stubs for CALLPROP since those need NoSuchMethod handling.
    if (JSOp(*pc) == JSOP_CALLPROP)
        return true;

    // Check if does-not-exist can be confirmed on property.
    RootedObject lastProto(cx);
    size_t protoChainDepth = SIZE_MAX;
    if (!CheckHasNoSuchProperty(cx, obj, name, &lastProto, &protoChainDepth))
        return true;
    MOZ_ASSERT(protoChainDepth < SIZE_MAX);

    if (protoChainDepth > ICGetProp_NativeDoesNotExist::MAX_PROTO_CHAIN_DEPTH)
        return true;

    ICStub* monitorStub = stub->fallbackMonitorStub()->firstMonitorStub();

    // Confirmed no-such-property.  Add stub.
    JitSpew(JitSpew_BaselineIC, "  Generating GetProp_NativeDoesNotExist stub");
    ICGetPropNativeDoesNotExistCompiler compiler(cx, monitorStub, obj, protoChainDepth);
    ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
    if (!newStub)
        return false;

    stub->addNewStub(newStub);
    *attached = true;
    return true;
}

static bool
ComputeGetPropResult(JSContext* cx, BaselineFrame* frame, JSOp op, HandlePropertyName name,
                     MutableHandleValue val, MutableHandleValue res)
{
    // Handle arguments.length and arguments.callee on optimized arguments, as
    // it is not an object.
    if (val.isMagic(JS_OPTIMIZED_ARGUMENTS) && IsOptimizedArguments(frame, val)) {
        if (op == JSOP_LENGTH) {
            res.setInt32(frame->numActualArgs());
        } else {
            MOZ_ASSERT(name == cx->names().callee);
            MOZ_ASSERT(frame->script()->hasMappedArgsObj());
            res.setObject(*frame->callee());
        }
    } else {
        if (op == JSOP_GETPROP || op == JSOP_LENGTH) {
            if (!GetProperty(cx, val, name, res))
                return false;
        } else if (op == JSOP_CALLPROP) {
            if (!CallProperty(cx, val, name, res))
                return false;
        } else {
            MOZ_ASSERT(op == JSOP_GETXPROP);
            RootedObject obj(cx, &val.toObject());
            RootedId id(cx, NameToId(name));
            if (!GetPropertyForNameLookup(cx, obj, id, res))
                return false;
        }
    }

    return true;
}

static bool
DoGetPropFallback(JSContext* cx, BaselineFrame* frame, ICGetProp_Fallback* stub_,
                  MutableHandleValue val, MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICGetProp_Fallback*> stub(frame, stub_);

    jsbytecode* pc = stub->icEntry()->pc(frame->script());
    JSOp op = JSOp(*pc);
    FallbackICSpew(cx, stub, "GetProp(%s)", js_CodeName[op]);

    MOZ_ASSERT(op == JSOP_GETPROP || op == JSOP_CALLPROP || op == JSOP_LENGTH || op == JSOP_GETXPROP);

    // Grab our old shape before it goes away.
    RootedShape oldShape(cx);
    if (val.isObject())
        oldShape = val.toObject().maybeShape();

    bool attached = false;
    // There are some reasons we can fail to attach a stub that are temporary.
    // We want to avoid calling noteUnoptimizableAccess() if the reason we
    // failed to attach a stub is one of those temporary reasons, since we might
    // end up attaching a stub for the exact same access later.
    bool isTemporarilyUnoptimizable = false;

    RootedScript script(cx, frame->script());
    RootedPropertyName name(cx, frame->script()->getName(pc));

    // After the  Genericstub was added, we should never reach the Fallbackstub again.
    MOZ_ASSERT(!stub->hasStub(ICStub::GetProp_Generic));

    if (stub->numOptimizedStubs() >= ICGetProp_Fallback::MAX_OPTIMIZED_STUBS) {
        // Discard all stubs in this IC and replace with generic getprop stub.
        for(ICStubIterator iter = stub->beginChain(); !iter.atEnd(); iter++)
            iter.unlink(cx);
        ICGetProp_Generic::Compiler compiler(cx, stub->fallbackMonitorStub()->firstMonitorStub());
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;
        stub->addNewStub(newStub);
        attached = true;
    }

    if (!attached && !TryAttachNativeGetAccessorPropStub(cx, script, pc, stub, name, val, res,
                                                         &attached, &isTemporarilyUnoptimizable))
    {
        return false;
    }

    if (!ComputeGetPropResult(cx, frame, op, name, val, res))
        return false;

    TypeScript::Monitor(cx, script, pc, res);

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid())
        return true;

    // Add a type monitor stub for the resulting value.
    if (!stub->addMonitorStubForValue(cx, script, res))
        return false;

    if (attached)
        return true;

    if (op == JSOP_LENGTH) {
        if (!TryAttachLengthStub(cx, script, stub, val, res, &attached))
            return false;
        if (attached)
            return true;
    }

    if (!TryAttachMagicArgumentsGetPropStub(cx, script, stub, name, val, res, &attached))
        return false;
    if (attached)
        return true;

    if (!TryAttachNativeGetValuePropStub(cx, script, pc, stub, name, val, oldShape,
                                         res, &attached))
        return false;
    if (attached)
        return true;

    if (!TryAttachUnboxedGetPropStub(cx, script, stub, name, val, &attached))
        return false;
    if (attached)
        return true;

    if (!TryAttachUnboxedExpandoGetPropStub(cx, script, pc, stub, name, val, &attached))
        return false;
    if (attached)
        return true;

    if (!TryAttachTypedObjectGetPropStub(cx, script, stub, name, val, &attached))
        return false;
    if (attached)
        return true;

    if (val.isString() || val.isNumber() || val.isBoolean()) {
        if (!TryAttachPrimitiveGetPropStub(cx, script, pc, stub, name, val, res, &attached))
            return false;
        if (attached)
            return true;
    }

    if (res.isUndefined()) {
        // Try attaching property-not-found optimized stub for undefined results.
        if (!TryAttachNativeGetPropDoesNotExistStub(cx, script, pc, stub, name, val, &attached))
            return false;
        if (attached)
            return true;
    }

    MOZ_ASSERT(!attached);
    if (!isTemporarilyUnoptimizable)
        stub->noteUnoptimizableAccess();

    return true;
}

typedef bool (*DoGetPropFallbackFn)(JSContext*, BaselineFrame*, ICGetProp_Fallback*,
                                    MutableHandleValue, MutableHandleValue);
static const VMFunction DoGetPropFallbackInfo =
    FunctionInfo<DoGetPropFallbackFn>(DoGetPropFallback, TailCall, PopValues(1));

bool
ICGetProp_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);
    MOZ_ASSERT(R0 == JSReturnOperand);

    EmitRestoreTailCallReg(masm);

    // Ensure stack is fully synced for the expression decompiler.
    masm.pushValue(R0);

    // Push arguments.
    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushFramePtr(masm, R0.scratchReg());

    if (!tailCallVM(DoGetPropFallbackInfo, masm))
        return false;

    // What follows is bailout for inlined scripted getters.
    // The return address pointed to by the baseline stack points here.
    returnOffset_ = masm.currentOffset();

    // Even though the fallback frame doesn't enter a stub frame, the CallScripted
    // frame that we are emulating does. Again, we lie.
    inStubFrame_ = true;
#ifdef DEBUG
    entersStubFrame_ = true;
#endif

    leaveStubFrame(masm, true);

    // When we get here, ICStubReg contains the ICGetProp_Fallback stub,
    // which we can't use to enter the TypeMonitor IC, because it's a MonitoredFallbackStub
    // instead of a MonitoredStub. So, we cheat.
    masm.loadPtr(Address(ICStubReg, ICMonitoredFallbackStub::offsetOfFallbackMonitorStub()),
                 ICStubReg);
    EmitEnterTypeMonitorIC(masm, ICTypeMonitor_Fallback::offsetOfFirstMonitorStub());

    return true;
}

bool
ICGetProp_Fallback::Compiler::postGenerateStubCode(MacroAssembler& masm, Handle<JitCode*> code)
{
    CodeOffsetLabel offset(returnOffset_);
    offset.fixup(&masm);
    cx->compartment()->jitCompartment()->initBaselineGetPropReturnAddr(code->raw() + offset.offset());
    return true;
}

bool
ICGetProp_ArrayLength::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    Register scratch = R1.scratchReg();

    // Unbox R0 and guard it's an array.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    masm.branchTestObjClass(Assembler::NotEqual, obj, scratch, &ArrayObject::class_, &failure);

    // Load obj->elements->length.
    masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);
    masm.load32(Address(scratch, ObjectElements::offsetOfLength()), scratch);

    // Guard length fits in an int32.
    masm.branchTest32(Assembler::Signed, scratch, scratch, &failure);

    masm.tagValue(JSVAL_TYPE_INT32, scratch, R0);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICGetProp_UnboxedArrayLength::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    Register scratch = R1.scratchReg();

    // Unbox R0 and guard it's an unboxed array.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    masm.branchTestObjClass(Assembler::NotEqual, obj, scratch, &UnboxedArrayObject::class_, &failure);

    // Load obj->length.
    masm.load32(Address(obj, UnboxedArrayObject::offsetOfLength()), scratch);

    masm.tagValue(JSVAL_TYPE_INT32, scratch, R0);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICGetProp_StringLength::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    masm.branchTestString(Assembler::NotEqual, R0, &failure);

    // Unbox string and load its length.
    Register string = masm.extractString(R0, ExtractTemp0);
    masm.loadStringLength(string, string);

    masm.tagValue(JSVAL_TYPE_INT32, string, R0);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICGetProp_Primitive::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    switch (primitiveType_) {
      case JSVAL_TYPE_STRING:
        masm.branchTestString(Assembler::NotEqual, R0, &failure);
        break;
      case JSVAL_TYPE_SYMBOL:
        masm.branchTestSymbol(Assembler::NotEqual, R0, &failure);
        break;
      case JSVAL_TYPE_DOUBLE: // Also used for int32.
        masm.branchTestNumber(Assembler::NotEqual, R0, &failure);
        break;
      case JSVAL_TYPE_BOOLEAN:
        masm.branchTestBoolean(Assembler::NotEqual, R0, &failure);
        break;
      default:
        MOZ_CRASH("unexpected type");
    }

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(1));
    Register holderReg = regs.takeAny();
    Register scratchReg = regs.takeAny();

    // Verify the shape of the prototype.
    masm.movePtr(ImmGCPtr(prototype_.get()), holderReg);

    Address shapeAddr(ICStubReg, ICGetProp_Primitive::offsetOfProtoShape());
    masm.loadPtr(Address(holderReg, JSObject::offsetOfShape()), scratchReg);
    masm.branchPtr(Assembler::NotEqual, shapeAddr, scratchReg, &failure);

    if (!isFixedSlot_)
        masm.loadPtr(Address(holderReg, NativeObject::offsetOfSlots()), holderReg);

    masm.load32(Address(ICStubReg, ICGetProp_Primitive::offsetOfOffset()), scratchReg);
    masm.loadValue(BaseIndex(holderReg, scratchReg, TimesOne), R0);

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

ICGetPropNativeStub*
ICGetPropNativeCompiler::getStub(ICStubSpace* space)
{
    ReceiverGuard guard(obj_);

    switch (kind) {
      case ICStub::GetProp_Native: {
        MOZ_ASSERT(obj_ == holder_);
        return newStub<ICGetProp_Native>(space, getStubCode(), firstMonitorStub_, guard, offset_);
      }

      case ICStub::GetProp_NativePrototype: {
        MOZ_ASSERT(obj_ != holder_);
        Shape* holderShape = holder_->as<NativeObject>().lastProperty();
        return newStub<ICGetProp_NativePrototype>(space, getStubCode(), firstMonitorStub_, guard,
                                                  offset_, holder_, holderShape);
      }

      default:
        MOZ_CRASH("Bad stub kind");
    }
}

bool
ICGetPropNativeCompiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));
    Register objReg = InvalidReg;

    if (inputDefinitelyObject_) {
        objReg = R0.scratchReg();
    } else {
        regs.take(R0);
        // Guard input is an object and unbox.
        masm.branchTestObject(Assembler::NotEqual, R0, &failure);
        objReg = masm.extractObject(R0, ExtractTemp0);
    }
    regs.takeUnchecked(objReg);

    Register scratch = regs.takeAnyExcluding(ICTailCallReg);

    // Shape/group guard.
    GuardReceiverObject(masm, ReceiverGuard(obj_), objReg, scratch,
                        ICGetPropNativeStub::offsetOfReceiverGuard(), &failure);

    Register holderReg;
    if (obj_ == holder_) {
        if (obj_->is<UnboxedPlainObject>()) {
            // We are loading off the expando object, so use that for the holder.
            holderReg = regs.takeAny();
            masm.loadPtr(Address(objReg, UnboxedPlainObject::offsetOfExpando()), holderReg);
        } else {
            holderReg = objReg;
        }
    } else {
        // Shape guard holder.
        holderReg = regs.takeAny();
        masm.loadPtr(Address(ICStubReg, ICGetProp_NativePrototype::offsetOfHolder()),
                     holderReg);
        masm.loadPtr(Address(ICStubReg, ICGetProp_NativePrototype::offsetOfHolderShape()),
                     scratch);
        masm.branchTestObjShape(Assembler::NotEqual, holderReg, scratch, &failure);
    }

    if (!isFixedSlot_) {
        // Don't overwrite actual holderReg if we need to load a dynamic slots object.
        // May need to preserve object for noSuchMethod check later.
        Register nextHolder = regs.takeAny();
        masm.loadPtr(Address(holderReg, NativeObject::offsetOfSlots()), nextHolder);
        holderReg = nextHolder;
    }

    masm.load32(Address(ICStubReg, ICGetPropNativeStub::offsetOfOffset()), scratch);
    BaseIndex result(holderReg, scratch, TimesOne);

#if JS_HAS_NO_SUCH_METHOD
#ifdef DEBUG
    entersStubFrame_ = true;
#endif
    if (isCallProp_) {
        // Check for __noSuchMethod__ invocation.
        Label afterNoSuchMethod;
        Label skipNoSuchMethod;

        masm.push(objReg);
        masm.loadValue(result, R0);
        masm.branchTestUndefined(Assembler::NotEqual, R0, &skipNoSuchMethod);

        masm.pop(objReg);

        // Call __noSuchMethod__ checker.  Object pointer is in objReg.
        regs = availableGeneralRegs(0);
        regs.takeUnchecked(objReg);
        regs.takeUnchecked(ICTailCallReg);
        ValueOperand val = regs.takeAnyValue();

        // Box and push obj onto baseline frame stack for decompiler.
        EmitRestoreTailCallReg(masm);
        masm.tagValue(JSVAL_TYPE_OBJECT, objReg, val);
        masm.pushValue(val);
        EmitRepushTailCallReg(masm);

        enterStubFrame(masm, regs.getAnyExcluding(ICTailCallReg));

        masm.movePtr(ImmGCPtr(propName_.get()), val.scratchReg());
        masm.tagValue(JSVAL_TYPE_STRING, val.scratchReg(), val);
        masm.pushValue(val);
        masm.push(objReg);
        if (!callVM(LookupNoSuchMethodHandlerInfo, masm))
            return false;

        leaveStubFrame(masm);

        // Pop pushed obj from baseline stack.
        EmitUnstowICValues(masm, 1, /* discard = */ true);

        masm.jump(&afterNoSuchMethod);
        masm.bind(&skipNoSuchMethod);

        // Pop pushed objReg.
        masm.addToStackPtr(Imm32(sizeof(void*)));
        masm.bind(&afterNoSuchMethod);
    } else {
        masm.loadValue(result, R0);
    }
#else
    masm.loadValue(result, R0);
#endif

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

ICStub*
ICGetPropNativeDoesNotExistCompiler::getStub(ICStubSpace* space)
{
    Rooted<ShapeVector> shapes(cx, ShapeVector(cx));

    if (!GetProtoShapes(obj_, protoChainDepth_, &shapes))
        return nullptr;

    JS_STATIC_ASSERT(ICGetProp_NativeDoesNotExist::MAX_PROTO_CHAIN_DEPTH == 8);

    ICStub* stub = nullptr;
    switch(protoChainDepth_) {
      case 0: stub = getStubSpecific<0>(space, shapes); break;
      case 1: stub = getStubSpecific<1>(space, shapes); break;
      case 2: stub = getStubSpecific<2>(space, shapes); break;
      case 3: stub = getStubSpecific<3>(space, shapes); break;
      case 4: stub = getStubSpecific<4>(space, shapes); break;
      case 5: stub = getStubSpecific<5>(space, shapes); break;
      case 6: stub = getStubSpecific<6>(space, shapes); break;
      case 7: stub = getStubSpecific<7>(space, shapes); break;
      case 8: stub = getStubSpecific<8>(space, shapes); break;
      default: MOZ_CRASH("ProtoChainDepth too high.");
    }
    if (!stub)
        return nullptr;
    return stub;
}

bool
ICGetPropNativeDoesNotExistCompiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(1));
    Register scratch = regs.takeAny();

#ifdef DEBUG
    // Ensure that protoChainDepth_ matches the protoChainDepth stored on the stub.
    {
        Label ok;
        masm.load16ZeroExtend(Address(ICStubReg, ICStub::offsetOfExtra()), scratch);
        masm.branch32(Assembler::Equal, scratch, Imm32(protoChainDepth_), &ok);
        masm.assumeUnreachable("Non-matching proto chain depth on stub.");
        masm.bind(&ok);
    }
#endif // DEBUG

    // Guard input is an object.
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    // Unbox and guard against old shape/group.
    Register objReg = masm.extractObject(R0, ExtractTemp0);
    GuardReceiverObject(masm, ReceiverGuard(obj_), objReg, scratch,
                        ICGetProp_NativeDoesNotExist::offsetOfGuard(), &failure);

    Register protoReg = regs.takeAny();
    // Check the proto chain.
    for (size_t i = 0; i < protoChainDepth_; i++) {
        masm.loadObjProto(i == 0 ? objReg : protoReg, protoReg);
        masm.branchTestPtr(Assembler::Zero, protoReg, protoReg, &failure);
        size_t shapeOffset = ICGetProp_NativeDoesNotExistImpl<0>::offsetOfShape(i);
        masm.loadPtr(Address(ICStubReg, shapeOffset), scratch);
        masm.branchTestObjShape(Assembler::NotEqual, protoReg, scratch, &failure);
    }

    // Shape and type checks succeeded, ok to proceed.
    masm.moveValue(UndefinedValue(), R0);

    // Normally for this op, the result would have to be monitored by TI.
    // However, since this stub ALWAYS returns UndefinedValue(), and we can be sure
    // that undefined is already registered with the type-set, this can be avoided.
    EmitReturnFromIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICGetProp_CallScripted::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    Label failureLeaveStubFrame;
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(1));
    Register scratch = regs.takeAnyExcluding(ICTailCallReg);

    // Guard input is an object.
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    // Unbox and shape guard.
    Register objReg = masm.extractObject(R0, ExtractTemp0);
    GuardReceiverObject(masm, ReceiverGuard(receiver_), objReg, scratch,
                        ICGetProp_CallScripted::offsetOfReceiverGuard(), &failure);

    if (receiver_ != holder_) {
        Register holderReg = regs.takeAny();
        masm.loadPtr(Address(ICStubReg, ICGetProp_CallScripted::offsetOfHolder()), holderReg);
        masm.loadPtr(Address(ICStubReg, ICGetProp_CallScripted::offsetOfHolderShape()), scratch);
        masm.branchTestObjShape(Assembler::NotEqual, holderReg, scratch, &failure);
        regs.add(holderReg);
    }

    // Push a stub frame so that we can perform a non-tail call.
    enterStubFrame(masm, scratch);

    // Load callee function and code.  To ensure that |code| doesn't end up being
    // ArgumentsRectifierReg, if it's available we assign it to |callee| instead.
    Register callee;
    if (regs.has(ArgumentsRectifierReg)) {
        callee = ArgumentsRectifierReg;
        regs.take(callee);
    } else {
        callee = regs.takeAny();
    }
    Register code = regs.takeAny();
    masm.loadPtr(Address(ICStubReg, ICGetProp_CallScripted::offsetOfGetter()), callee);
    masm.branchIfFunctionHasNoScript(callee, &failureLeaveStubFrame);
    masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrScript()), code);
    masm.loadBaselineOrIonRaw(code, code, &failureLeaveStubFrame);

    // Align the stack such that the JitFrameLayout is aligned on
    // JitStackAlignment.
    masm.alignJitStackBasedOnNArgs(0);

    // Getter is called with 0 arguments, just |obj| as thisv.
    // Note that we use Push, not push, so that callJit will align the stack
    // properly on ARM.
    masm.Push(R0);
    EmitBaselineCreateStubFrameDescriptor(masm, scratch);
    masm.Push(Imm32(0));  // ActualArgc is 0
    masm.Push(callee);
    masm.Push(scratch);

    // Handle arguments underflow.
    Label noUnderflow;
    masm.load16ZeroExtend(Address(callee, JSFunction::offsetOfNargs()), scratch);
    masm.branch32(Assembler::Equal, scratch, Imm32(0), &noUnderflow);
    {
        // Call the arguments rectifier.
        MOZ_ASSERT(ArgumentsRectifierReg != code);

        JitCode* argumentsRectifier =
            cx->runtime()->jitRuntime()->getArgumentsRectifier();

        masm.movePtr(ImmGCPtr(argumentsRectifier), code);
        masm.loadPtr(Address(code, JitCode::offsetOfCode()), code);
        masm.movePtr(ImmWord(0), ArgumentsRectifierReg);
    }

    masm.bind(&noUnderflow);
    masm.callJit(code);

    leaveStubFrame(masm, true);

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    // Leave stub frame and go to next stub.
    masm.bind(&failureLeaveStubFrame);
    inStubFrame_ = true;
    leaveStubFrame(masm, false);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICGetProp_CallNative::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(1));
    Register objReg = InvalidReg;

    MOZ_ASSERT(!(inputDefinitelyObject_ && outerClass_));
    if (inputDefinitelyObject_) {
        objReg = R0.scratchReg();
    } else {
        // Guard input is an object and unbox.
        masm.branchTestObject(Assembler::NotEqual, R0, &failure);
        objReg = masm.extractObject(R0, ExtractTemp0);
        if (outerClass_) {
            ValueOperand val = regs.takeAnyValue();
            Register tmp = regs.takeAny();
            masm.branchTestObjClass(Assembler::NotEqual, objReg, tmp, outerClass_, &failure);
            masm.loadPtr(Address(objReg, ProxyDataOffset + offsetof(ProxyDataLayout, values)), tmp);
            masm.loadValue(Address(tmp, offsetof(ProxyValueArray, privateSlot)), val);
            masm.movePtr(masm.extractObject(val, ExtractTemp0), objReg);
            regs.add(val);
            regs.add(tmp);
        }
    }

    Register scratch = regs.takeAnyExcluding(ICTailCallReg);

    // Shape guard.
    GuardReceiverObject(masm, ReceiverGuard(receiver_), objReg, scratch,
                        ICGetProp_CallNative::offsetOfReceiverGuard(), &failure);

    if (receiver_ != holder_ ) {
        Register holderReg = regs.takeAny();
        masm.loadPtr(Address(ICStubReg, ICGetProp_CallNative::offsetOfHolder()), holderReg);
        masm.loadPtr(Address(ICStubReg, ICGetProp_CallNative::offsetOfHolderShape()), scratch);
        masm.branchTestObjShape(Assembler::NotEqual, holderReg, scratch, &failure);
        regs.add(holderReg);
    }

    // Box and push obj onto baseline frame stack for decompiler
    if (inputDefinitelyObject_)
        masm.tagValue(JSVAL_TYPE_OBJECT, objReg, R0);
    EmitStowICValues(masm, 1);
    if (inputDefinitelyObject_)
        objReg = masm.extractObject(R0, ExtractTemp0);

    // Push a stub frame so that we can perform a non-tail call.
    enterStubFrame(masm, scratch);

    // Load callee function.
    Register callee = regs.takeAny();
    masm.loadPtr(Address(ICStubReg, ICGetProp_CallNative::offsetOfGetter()), callee);

    // Push args for vm call.
    masm.push(objReg);
    masm.push(callee);

    regs.add(R0);

    if (!callVM(DoCallNativeGetterInfo, masm))
        return false;
    leaveStubFrame(masm);

    EmitUnstowICValues(masm, 1, /* discard = */true);

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICGetPropCallDOMProxyNativeCompiler::generateStubCode(MacroAssembler& masm,
                                                      Address* expandoAndGenerationAddr,
                                                      Address* generationAddr)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(1));
    Register scratch = regs.takeAnyExcluding(ICTailCallReg);

    // Guard input is an object.
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    // Unbox.
    Register objReg = masm.extractObject(R0, ExtractTemp0);

    // Shape guard.
    static const size_t receiverShapeOffset =
        ICGetProp_CallDOMProxyNative::offsetOfReceiverGuard() +
        HeapReceiverGuard::offsetOfShape();
    masm.loadPtr(Address(ICStubReg, receiverShapeOffset), scratch);
    masm.branchTestObjShape(Assembler::NotEqual, objReg, scratch, &failure);

    // Guard that our expando object hasn't started shadowing this property.
    {
        AllocatableGeneralRegisterSet domProxyRegSet(GeneralRegisterSet::All());
        domProxyRegSet.take(ICStubReg);
        domProxyRegSet.take(objReg);
        domProxyRegSet.take(scratch);
        Address expandoShapeAddr(ICStubReg, ICGetProp_CallDOMProxyNative::offsetOfExpandoShape());
        CheckDOMProxyExpandoDoesNotShadow(
                cx, masm, objReg,
                expandoShapeAddr, expandoAndGenerationAddr, generationAddr,
                scratch,
                domProxyRegSet,
                &failure);
    }

    Register holderReg = regs.takeAny();
    masm.loadPtr(Address(ICStubReg, ICGetProp_CallDOMProxyNative::offsetOfHolder()),
                 holderReg);
    masm.loadPtr(Address(ICStubReg, ICGetProp_CallDOMProxyNative::offsetOfHolderShape()),
                 scratch);
    masm.branchTestObjShape(Assembler::NotEqual, holderReg, scratch, &failure);
    regs.add(holderReg);

    // Push a stub frame so that we can perform a non-tail call.
    enterStubFrame(masm, scratch);

    // Load callee function.
    Register callee = regs.takeAny();
    masm.loadPtr(Address(ICStubReg, ICGetProp_CallDOMProxyNative::offsetOfGetter()), callee);

    // Push args for vm call.
    masm.push(objReg);
    masm.push(callee);

    // Don't have to preserve R0 anymore.
    regs.add(R0);

    if (!callVM(DoCallNativeGetterInfo, masm))
        return false;
    leaveStubFrame(masm);

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICGetPropCallDOMProxyNativeCompiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    if (kind == ICStub::GetProp_CallDOMProxyNative)
        return generateStubCode(masm, nullptr, nullptr);

    Address internalStructAddress(ICStubReg,
        ICGetProp_CallDOMProxyWithGenerationNative::offsetOfInternalStruct());
    Address generationAddress(ICStubReg,
        ICGetProp_CallDOMProxyWithGenerationNative::offsetOfGeneration());
    return generateStubCode(masm, &internalStructAddress, &generationAddress);
}

ICStub*
ICGetPropCallDOMProxyNativeCompiler::getStub(ICStubSpace* space)
{
    RootedShape shape(cx, proxy_->maybeShape());
    RootedShape holderShape(cx, holder_->as<NativeObject>().lastProperty());

    Value expandoSlot = GetProxyExtra(proxy_, GetDOMProxyExpandoSlot());
    RootedShape expandoShape(cx, nullptr);
    ExpandoAndGeneration* expandoAndGeneration;
    int32_t generation;
    Value expandoVal;
    if (kind == ICStub::GetProp_CallDOMProxyNative) {
        expandoVal = expandoSlot;
        expandoAndGeneration = nullptr;  // initialize to silence GCC warning
        generation = 0;  // initialize to silence GCC warning
    } else {
        MOZ_ASSERT(kind == ICStub::GetProp_CallDOMProxyWithGenerationNative);
        MOZ_ASSERT(!expandoSlot.isObject() && !expandoSlot.isUndefined());
        expandoAndGeneration = (ExpandoAndGeneration*)expandoSlot.toPrivate();
        expandoVal = expandoAndGeneration->expando;
        generation = expandoAndGeneration->generation;
    }

    if (expandoVal.isObject())
        expandoShape = expandoVal.toObject().as<NativeObject>().lastProperty();

    if (kind == ICStub::GetProp_CallDOMProxyNative) {
        return newStub<ICGetProp_CallDOMProxyNative>(
            space, getStubCode(), firstMonitorStub_, shape,
            expandoShape, holder_, holderShape, getter_, pcOffset_);
    }

    return newStub<ICGetProp_CallDOMProxyWithGenerationNative>(
        space, getStubCode(), firstMonitorStub_, shape,
        expandoAndGeneration, generation, expandoShape, holder_, holderShape, getter_,
        pcOffset_);
}

ICStub*
ICGetProp_DOMProxyShadowed::Compiler::getStub(ICStubSpace* space)
{
    RootedShape shape(cx, proxy_->maybeShape());
    return New<ICGetProp_DOMProxyShadowed>(cx, space, getStubCode(), firstMonitorStub_, shape,
                                           proxy_->handler(), name_, pcOffset_);
}

static bool
ProxyGet(JSContext* cx, HandleObject proxy, HandlePropertyName name, MutableHandleValue vp)
{
    RootedValue receiver(cx, ObjectValue(*proxy));
    RootedId id(cx, NameToId(name));
    return Proxy::get(cx, proxy, receiver, id, vp);
}

typedef bool (*ProxyGetFn)(JSContext* cx, HandleObject proxy, HandlePropertyName name,
                           MutableHandleValue vp);
static const VMFunction ProxyGetInfo = FunctionInfo<ProxyGetFn>(ProxyGet);

bool
ICGetProp_DOMProxyShadowed::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(1));
    // Need to reserve a scratch register, but the scratch register should not be
    // ICTailCallReg, because it's used for |enterStubFrame| which needs a
    // non-ICTailCallReg scratch reg.
    Register scratch = regs.takeAnyExcluding(ICTailCallReg);

    // Guard input is an object.
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    // Unbox.
    Register objReg = masm.extractObject(R0, ExtractTemp0);

    // Shape guard.
    masm.loadPtr(Address(ICStubReg, ICGetProp_DOMProxyShadowed::offsetOfShape()), scratch);
    masm.branchTestObjShape(Assembler::NotEqual, objReg, scratch, &failure);

    // No need to do any more guards; it's safe to call ProxyGet even
    // if we've since stopped shadowing.

    // Call ProxyGet(JSContext* cx, HandleObject proxy, HandlePropertyName name, MutableHandleValue vp);

    // Push a stub frame so that we can perform a non-tail call.
    enterStubFrame(masm, scratch);

    // Push property name and proxy object.
    masm.loadPtr(Address(ICStubReg, ICGetProp_DOMProxyShadowed::offsetOfName()), scratch);
    masm.push(scratch);
    masm.push(objReg);

    // Don't have to preserve R0 anymore.
    regs.add(R0);

    if (!callVM(ProxyGetInfo, masm))
        return false;
    leaveStubFrame(masm);

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICGetProp_ArgumentsLength::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    if (which_ == ICGetProp_ArgumentsLength::Magic) {
        // Ensure that this is lazy arguments.
        masm.branchTestMagicValue(Assembler::NotEqual, R0, JS_OPTIMIZED_ARGUMENTS, &failure);

        // Ensure that frame has not loaded different arguments object since.
        masm.branchTest32(Assembler::NonZero,
                          Address(BaselineFrameReg, BaselineFrame::reverseOffsetOfFlags()),
                          Imm32(BaselineFrame::HAS_ARGS_OBJ),
                          &failure);

        Address actualArgs(BaselineFrameReg, BaselineFrame::offsetOfNumActualArgs());
        masm.loadPtr(actualArgs, R0.scratchReg());
        masm.tagValue(JSVAL_TYPE_INT32, R0.scratchReg(), R0);
        EmitReturnFromIC(masm);

        masm.bind(&failure);
        EmitStubGuardFailure(masm);
        return true;
    }
    MOZ_ASSERT(which_ == ICGetProp_ArgumentsLength::Mapped ||
               which_ == ICGetProp_ArgumentsLength::Unmapped);

    const Class* clasp = (which_ == ICGetProp_ArgumentsLength::Mapped)
                         ? &MappedArgumentsObject::class_
                         : &UnmappedArgumentsObject::class_;

    Register scratchReg = R1.scratchReg();

    // Guard on input being an arguments object.
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);
    Register objReg = masm.extractObject(R0, ExtractTemp0);
    masm.branchTestObjClass(Assembler::NotEqual, objReg, scratchReg, clasp, &failure);

    // Get initial length value.
    masm.unboxInt32(Address(objReg, ArgumentsObject::getInitialLengthSlotOffset()), scratchReg);

    // Test if length has been overridden.
    masm.branchTest32(Assembler::NonZero,
                      scratchReg,
                      Imm32(ArgumentsObject::LENGTH_OVERRIDDEN_BIT),
                      &failure);

    // Nope, shift out arguments length and return it.
    // No need to type monitor because this stub always returns Int32.
    masm.rshiftPtr(Imm32(ArgumentsObject::PACKED_BITS_COUNT), scratchReg);
    masm.tagValue(JSVAL_TYPE_INT32, scratchReg, R0);
    EmitReturnFromIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

ICGetProp_ArgumentsCallee::ICGetProp_ArgumentsCallee(JitCode* stubCode, ICStub* firstMonitorStub)
  : ICMonitoredStub(GetProp_ArgumentsCallee, stubCode, firstMonitorStub)
{ }

bool
ICGetProp_ArgumentsCallee::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;

    // Ensure that this is lazy arguments.
    masm.branchTestMagicValue(Assembler::NotEqual, R0, JS_OPTIMIZED_ARGUMENTS, &failure);

    // Ensure that frame has not loaded different arguments object since.
    masm.branchTest32(Assembler::NonZero,
                      Address(BaselineFrameReg, BaselineFrame::reverseOffsetOfFlags()),
                      Imm32(BaselineFrame::HAS_ARGS_OBJ),
                      &failure);

    Address callee(BaselineFrameReg, BaselineFrame::offsetOfCalleeToken());
    masm.loadFunctionFromCalleeToken(callee, R0.scratchReg());
    masm.tagValue(JSVAL_TYPE_OBJECT, R0.scratchReg(), R0);

    EmitEnterTypeMonitorIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

/* static */ ICGetProp_Generic*
ICGetProp_Generic::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                         ICGetProp_Generic& other)
{
    return New<ICGetProp_Generic>(cx, space, other.jitCode(), firstMonitorStub);
}

static bool
DoGetPropGeneric(JSContext* cx, BaselineFrame* frame, ICGetProp_Generic* stub, MutableHandleValue val, MutableHandleValue res)
{
    jsbytecode* pc = stub->getChainFallback()->icEntry()->pc(frame->script());
    JSOp op = JSOp(*pc);
    RootedPropertyName name(cx, frame->script()->getName(pc));
    return ComputeGetPropResult(cx, frame, op, name, val, res);
}

typedef bool (*DoGetPropGenericFn)(JSContext*, BaselineFrame*, ICGetProp_Generic*, MutableHandleValue, MutableHandleValue);
static const VMFunction DoGetPropGenericInfo = FunctionInfo<DoGetPropGenericFn>(DoGetPropGeneric);

bool
ICGetProp_Generic::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(1));

    Register scratch = regs.takeAnyExcluding(ICTailCallReg);

    // Sync for the decompiler.
    EmitStowICValues(masm, 1);

    enterStubFrame(masm, scratch);

    // Push arguments.
    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushFramePtr(masm, R0.scratchReg());

    if(!callVM(DoGetPropGenericInfo, masm))
        return false;

    leaveStubFrame(masm);
    EmitUnstowICValues(masm, 1, /* discard = */ true);
    EmitEnterTypeMonitorIC(masm);
    return true;
}

bool
ICGetProp_Unboxed::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(1));

    Register scratch = regs.takeAnyExcluding(ICTailCallReg);

    // Object and group guard.
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);
    Register object = masm.extractObject(R0, ExtractTemp0);
    masm.loadPtr(Address(ICStubReg, ICGetProp_Unboxed::offsetOfGroup()), scratch);
    masm.branchPtr(Assembler::NotEqual, Address(object, JSObject::offsetOfGroup()), scratch,
                   &failure);

    // Get the address being read from.
    masm.load32(Address(ICStubReg, ICGetProp_Unboxed::offsetOfFieldOffset()), scratch);

    masm.loadUnboxedProperty(BaseIndex(object, scratch, TimesOne), fieldType_, TypedOrValueRegister(R0));

    // Only monitor the result if its type might change.
    if (fieldType_ == JSVAL_TYPE_OBJECT)
        EmitEnterTypeMonitorIC(masm);
    else
        EmitReturnFromIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    return true;
}

bool
ICGetProp_TypedObject::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;

    CheckForNeuteredTypedObject(cx, masm, &failure);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(1));

    Register scratch1 = regs.takeAnyExcluding(ICTailCallReg);
    Register scratch2 = regs.takeAnyExcluding(ICTailCallReg);

    // Object and shape guard.
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);
    Register object = masm.extractObject(R0, ExtractTemp0);
    masm.loadPtr(Address(ICStubReg, ICGetProp_TypedObject::offsetOfShape()), scratch1);
    masm.branchTestObjShape(Assembler::NotEqual, object, scratch1, &failure);

    // Get the object's data pointer.
    LoadTypedThingData(masm, layout_, object, scratch1);

    // Get the address being written to.
    masm.load32(Address(ICStubReg, ICGetProp_TypedObject::offsetOfFieldOffset()), scratch2);
    masm.addPtr(scratch2, scratch1);

    // Only monitor the result if the type produced by this stub might vary.
    bool monitorLoad;

    if (fieldDescr_->is<ScalarTypeDescr>()) {
        Scalar::Type type = fieldDescr_->as<ScalarTypeDescr>().type();
        monitorLoad = type == Scalar::Uint32;

        masm.loadFromTypedArray(type, Address(scratch1, 0), R0, /* allowDouble = */ true,
                                scratch2, nullptr);
    } else {
        ReferenceTypeDescr::Type type = fieldDescr_->as<ReferenceTypeDescr>().type();
        monitorLoad = type != ReferenceTypeDescr::TYPE_STRING;

        switch (type) {
          case ReferenceTypeDescr::TYPE_ANY:
            masm.loadValue(Address(scratch1, 0), R0);
            break;

          case ReferenceTypeDescr::TYPE_OBJECT: {
            Label notNull, done;
            masm.loadPtr(Address(scratch1, 0), scratch1);
            masm.branchTestPtr(Assembler::NonZero, scratch1, scratch1, &notNull);
            masm.moveValue(NullValue(), R0);
            masm.jump(&done);
            masm.bind(&notNull);
            masm.tagValue(JSVAL_TYPE_OBJECT, scratch1, R0);
            masm.bind(&done);
            break;
          }

          case ReferenceTypeDescr::TYPE_STRING:
            masm.loadPtr(Address(scratch1, 0), scratch1);
            masm.tagValue(JSVAL_TYPE_STRING, scratch1, R0);
            break;

          default:
            MOZ_CRASH();
        }
    }

    if (monitorLoad)
        EmitEnterTypeMonitorIC(masm);
    else
        EmitReturnFromIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    return true;
}

void
BaselineScript::noteAccessedGetter(uint32_t pcOffset)
{
    ICEntry& entry = icEntryFromPCOffset(pcOffset);
    ICFallbackStub* stub = entry.fallbackStub();

    if (stub->isGetProp_Fallback())
        stub->toGetProp_Fallback()->noteAccessedGetter();
}

//
// SetProp_Fallback
//

// Attach an optimized property set stub for a SETPROP/SETGNAME/SETNAME op on a
// value property.
static bool
TryAttachSetValuePropStub(JSContext* cx, HandleScript script, jsbytecode* pc, ICSetProp_Fallback* stub,
                          HandleObject obj, HandleShape oldShape, HandleObjectGroup oldGroup,
                          HandlePropertyName name, HandleId id, HandleValue rhs, bool* attached)
{
    MOZ_ASSERT(!*attached);

    if (obj->watched())
        return true;

    RootedShape shape(cx);
    RootedObject holder(cx);
    if (!EffectlesslyLookupProperty(cx, obj, id, &holder, &shape))
        return false;
    if (obj != holder)
        return true;

    if (!obj->isNative()) {
        if (obj->is<UnboxedPlainObject>()) {
            UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando();
            if (expando) {
                shape = expando->lookup(cx, name);
                if (!shape)
                    return true;
            } else {
                return true;
            }
        } else {
            return true;
        }
    }

    size_t chainDepth;
    if (IsCacheableSetPropAddSlot(cx, obj, oldShape, id, shape, &chainDepth)) {
        // Don't attach if proto chain depth is too high.
        if (chainDepth > ICSetProp_NativeAdd::MAX_PROTO_CHAIN_DEPTH)
            return true;

        // Don't attach if we are adding a property to an object which the new
        // script properties analysis hasn't been performed for yet, as there
        // may be a shape change required here afterwards. Pretend we attached
        // a stub, though, so the access is not marked as unoptimizable.
        if (oldGroup->newScript() && !oldGroup->newScript()->analyzed()) {
            *attached = true;
            return true;
        }

        bool isFixedSlot;
        uint32_t offset;
        GetFixedOrDynamicSlotOffset(shape, &isFixedSlot, &offset);

        JitSpew(JitSpew_BaselineIC, "  Generating SetProp(NativeObject.ADD) stub");
        ICSetPropNativeAddCompiler compiler(cx, obj, oldShape, oldGroup,
                                            chainDepth, isFixedSlot, offset);
        ICUpdatedStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;
        if (!newStub->addUpdateStubForValue(cx, script, obj, id, rhs))
            return false;

        stub->addNewStub(newStub);
        *attached = true;
        return true;
    }

    if (IsCacheableSetPropWriteSlot(obj, oldShape, shape)) {
        // For some property writes, such as the initial overwrite of global
        // properties, TI will not mark the property as having been
        // overwritten. Don't attach a stub in this case, so that we don't
        // execute another write to the property without TI seeing that write.
        EnsureTrackPropertyTypes(cx, obj, id);
        if (!PropertyHasBeenMarkedNonConstant(obj, id)) {
            *attached = true;
            return true;
        }

        bool isFixedSlot;
        uint32_t offset;
        GetFixedOrDynamicSlotOffset(shape, &isFixedSlot, &offset);

        JitSpew(JitSpew_BaselineIC, "  Generating SetProp(NativeObject.PROP) stub");
        MOZ_ASSERT(LastPropertyForSetProp(obj) == oldShape,
                   "Should this really be a SetPropWriteSlot?");
        ICSetProp_Native::Compiler compiler(cx, obj, isFixedSlot, offset);
        ICSetProp_Native* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;
        if (!newStub->addUpdateStubForValue(cx, script, obj, id, rhs))
            return false;

        if (IsPreliminaryObject(obj))
            newStub->notePreliminaryObject();
        else
            StripPreliminaryObjectStubs(cx, stub);

        stub->addNewStub(newStub);
        *attached = true;
        return true;
    }

    return true;
}

// Attach an optimized property set stub for a SETPROP/SETGNAME/SETNAME op on
// an accessor property.
static bool
TryAttachSetAccessorPropStub(JSContext* cx, HandleScript script, jsbytecode* pc,
                             ICSetProp_Fallback* stub,
                             HandleObject obj, const RootedReceiverGuard& receiverGuard,
                             HandlePropertyName name,
                             HandleId id, HandleValue rhs, bool* attached,
                             bool* isTemporarilyUnoptimizable)
{
    MOZ_ASSERT(!*attached);
    MOZ_ASSERT(!*isTemporarilyUnoptimizable);

    if (obj->watched())
        return true;

    RootedShape shape(cx);
    RootedObject holder(cx);
    if (!EffectlesslyLookupProperty(cx, obj, id, &holder, &shape))
        return false;

    bool isScripted = false;
    bool cacheableCall = IsCacheableSetPropCall(cx, obj, holder, shape,
                                                &isScripted, isTemporarilyUnoptimizable);

    // Try handling scripted setters.
    if (cacheableCall && isScripted) {
        RootedFunction callee(cx, &shape->setterObject()->as<JSFunction>());
        MOZ_ASSERT(callee->hasScript());

        if (UpdateExistingSetPropCallStubs(stub, ICStub::SetProp_CallScripted,
                                           &holder->as<NativeObject>(), obj, callee)) {
            *attached = true;
            return true;
        }

        JitSpew(JitSpew_BaselineIC, "  Generating SetProp(NativeObj/ScriptedSetter %s:%" PRIuSIZE ") stub",
                    callee->nonLazyScript()->filename(), callee->nonLazyScript()->lineno());

        ICSetProp_CallScripted::Compiler compiler(cx, obj, holder, callee, script->pcToOffset(pc));
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;

        stub->addNewStub(newStub);
        *attached = true;
        return true;
    }

    // Try handling JSNative setters.
    if (cacheableCall && !isScripted) {
        RootedFunction callee(cx, &shape->setterObject()->as<JSFunction>());
        MOZ_ASSERT(callee->isNative());

        if (UpdateExistingSetPropCallStubs(stub, ICStub::SetProp_CallNative,
                                           &holder->as<NativeObject>(), obj, callee)) {
            *attached = true;
            return true;
        }

        JitSpew(JitSpew_BaselineIC, "  Generating SetProp(NativeObj/NativeSetter %p) stub",
                    callee->native());

        ICSetProp_CallNative::Compiler compiler(cx, obj, holder, callee, script->pcToOffset(pc));
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;

        stub->addNewStub(newStub);
        *attached = true;
        return true;
    }

    return true;
}

static bool
TryAttachUnboxedSetPropStub(JSContext* cx, HandleScript script,
                            ICSetProp_Fallback* stub, HandleId id,
                            HandleObject obj, HandleValue rhs, bool* attached)
{
    MOZ_ASSERT(!*attached);

    if (!cx->runtime()->jitSupportsFloatingPoint)
        return true;

    if (!obj->is<UnboxedPlainObject>())
        return true;

    const UnboxedLayout::Property* property = obj->as<UnboxedPlainObject>().layout().lookup(id);
    if (!property)
        return true;

    ICSetProp_Unboxed::Compiler compiler(cx, obj->group(),
                                         property->offset + UnboxedPlainObject::offsetOfData(),
                                         property->type);
    ICUpdatedStub* newStub = compiler.getStub(compiler.getStubSpace(script));
    if (!newStub)
        return false;
    if (compiler.needsUpdateStubs() && !newStub->addUpdateStubForValue(cx, script, obj, id, rhs))
        return false;

    stub->addNewStub(newStub);

    StripPreliminaryObjectStubs(cx, stub);

    *attached = true;
    return true;
}

static bool
TryAttachTypedObjectSetPropStub(JSContext* cx, HandleScript script,
                                ICSetProp_Fallback* stub, HandleId id,
                                HandleObject obj, HandleValue rhs, bool* attached)
{
    MOZ_ASSERT(!*attached);

    if (!cx->runtime()->jitSupportsFloatingPoint)
        return true;

    if (!obj->is<TypedObject>())
        return true;

    if (!obj->as<TypedObject>().typeDescr().is<StructTypeDescr>())
        return true;
    Rooted<StructTypeDescr*> structDescr(cx);
    structDescr = &obj->as<TypedObject>().typeDescr().as<StructTypeDescr>();

    size_t fieldIndex;
    if (!structDescr->fieldIndex(id, &fieldIndex))
        return true;

    Rooted<TypeDescr*> fieldDescr(cx, &structDescr->fieldDescr(fieldIndex));
    if (!fieldDescr->is<SimpleTypeDescr>())
        return true;

    uint32_t fieldOffset = structDescr->fieldOffset(fieldIndex);

    ICSetProp_TypedObject::Compiler compiler(cx, obj->maybeShape(), obj->group(), fieldOffset,
                                             &fieldDescr->as<SimpleTypeDescr>());
    ICUpdatedStub* newStub = compiler.getStub(compiler.getStubSpace(script));
    if (!newStub)
        return false;
    if (compiler.needsUpdateStubs() && !newStub->addUpdateStubForValue(cx, script, obj, id, rhs))
        return false;

    stub->addNewStub(newStub);

    *attached = true;
    return true;
}

static bool
DoSetPropFallback(JSContext* cx, BaselineFrame* frame, ICSetProp_Fallback* stub_,
                  HandleValue lhs, HandleValue rhs, MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICSetProp_Fallback*> stub(frame, stub_);

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    JSOp op = JSOp(*pc);
    FallbackICSpew(cx, stub, "SetProp(%s)", js_CodeName[op]);

    MOZ_ASSERT(op == JSOP_SETPROP ||
               op == JSOP_STRICTSETPROP ||
               op == JSOP_SETNAME ||
               op == JSOP_STRICTSETNAME ||
               op == JSOP_SETGNAME ||
               op == JSOP_STRICTSETGNAME ||
               op == JSOP_INITPROP ||
               op == JSOP_INITLOCKEDPROP ||
               op == JSOP_INITHIDDENPROP ||
               op == JSOP_SETALIASEDVAR ||
               op == JSOP_INITALIASEDLEXICAL);

    RootedPropertyName name(cx);
    if (op == JSOP_SETALIASEDVAR || op == JSOP_INITALIASEDLEXICAL)
        name = ScopeCoordinateName(cx->runtime()->scopeCoordinateNameCache, script, pc);
    else
        name = script->getName(pc);
    RootedId id(cx, NameToId(name));

    RootedObject obj(cx, ToObjectFromStack(cx, lhs));
    if (!obj)
        return false;
    RootedShape oldShape(cx, obj->maybeShape());
    RootedObjectGroup oldGroup(cx, obj->getGroup(cx));
    if (!oldGroup)
        return false;
    RootedReceiverGuard oldGuard(cx, ReceiverGuard(obj));

    if (obj->is<UnboxedPlainObject>()) {
        MOZ_ASSERT(!oldShape);
        if (UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando())
            oldShape = expando->lastProperty();
    }

    bool attached = false;
    // There are some reasons we can fail to attach a stub that are temporary.
    // We want to avoid calling noteUnoptimizableAccess() if the reason we
    // failed to attach a stub is one of those temporary reasons, since we might
    // end up attaching a stub for the exact same access later.
    bool isTemporarilyUnoptimizable = false;
    if (stub->numOptimizedStubs() < ICSetProp_Fallback::MAX_OPTIMIZED_STUBS &&
        lhs.isObject() &&
        !TryAttachSetAccessorPropStub(cx, script, pc, stub, obj, oldGuard, name, id,
                                      rhs, &attached, &isTemporarilyUnoptimizable))
    {
        return false;
    }

    if (op == JSOP_INITPROP ||
        op == JSOP_INITLOCKEDPROP ||
        op == JSOP_INITHIDDENPROP)
    {
        if (!InitPropertyOperation(cx, op, obj, id, rhs))
            return false;
    } else if (op == JSOP_SETNAME ||
               op == JSOP_STRICTSETNAME ||
               op == JSOP_SETGNAME ||
               op == JSOP_STRICTSETGNAME)
    {
        if (!SetNameOperation(cx, script, pc, obj, rhs))
            return false;
    } else if (op == JSOP_SETALIASEDVAR || op == JSOP_INITALIASEDLEXICAL) {
        obj->as<ScopeObject>().setAliasedVar(cx, ScopeCoordinate(pc), name, rhs);
    } else {
        MOZ_ASSERT(op == JSOP_SETPROP || op == JSOP_STRICTSETPROP);

        RootedValue v(cx, rhs);
        if (!PutProperty(cx, obj, id, v, op == JSOP_STRICTSETPROP))
            return false;
    }

    // Leave the RHS on the stack.
    res.set(rhs);

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid())
        return true;

    if (stub->numOptimizedStubs() >= ICSetProp_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard all stubs in this IC and replace with generic setprop stub.
        return true;
    }

    if (!attached &&
        lhs.isObject() &&
        !TryAttachSetValuePropStub(cx, script, pc, stub, obj, oldShape, oldGroup,
                                   name, id, rhs, &attached))
    {
        return false;
    }
    if (attached)
        return true;

    if (!attached &&
        lhs.isObject() &&
        !TryAttachUnboxedSetPropStub(cx, script, stub, id, obj, rhs, &attached))
    {
        return false;
    }
    if (attached)
        return true;

    if (!attached &&
        lhs.isObject() &&
        !TryAttachTypedObjectSetPropStub(cx, script, stub, id, obj, rhs, &attached))
    {
        return false;
    }
    if (attached)
        return true;

    MOZ_ASSERT(!attached);
    if (!isTemporarilyUnoptimizable)
        stub->noteUnoptimizableAccess();

    return true;
}

typedef bool (*DoSetPropFallbackFn)(JSContext*, BaselineFrame*, ICSetProp_Fallback*,
                                    HandleValue, HandleValue, MutableHandleValue);
static const VMFunction DoSetPropFallbackInfo =
    FunctionInfo<DoSetPropFallbackFn>(DoSetPropFallback, TailCall, PopValues(2));

bool
ICSetProp_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);
    MOZ_ASSERT(R0 == JSReturnOperand);

    EmitRestoreTailCallReg(masm);

    // Ensure stack is fully synced for the expression decompiler.
    masm.pushValue(R0);
    masm.pushValue(R1);

    // Push arguments.
    masm.pushValue(R1);
    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushFramePtr(masm, R0.scratchReg());

    if (!tailCallVM(DoSetPropFallbackInfo, masm))
        return false;

    // What follows is bailout-only code for inlined script getters.
    // The return address pointed to by the baseline stack points here.
    returnOffset_ = masm.currentOffset();

    // Even though the fallback frame doesn't enter a stub frame, the CallScripted
    // frame that we are emulating does. Again, we lie.
    inStubFrame_ = true;
#ifdef DEBUG
    entersStubFrame_ = true;
#endif

    leaveStubFrame(masm, true);

    // Retrieve the stashed initial argument from the caller's frame before returning
    EmitUnstowICValues(masm, 1);
    EmitReturnFromIC(masm);

    return true;
}

bool
ICSetProp_Fallback::Compiler::postGenerateStubCode(MacroAssembler& masm, Handle<JitCode*> code)
{
    CodeOffsetLabel offset(returnOffset_);
    offset.fixup(&masm);
    cx->compartment()->jitCompartment()->initBaselineSetPropReturnAddr(code->raw() + offset.offset());
    return true;
}

static void
GuardGroupAndShapeMaybeUnboxedExpando(MacroAssembler& masm, JSObject* obj,
                                      Register object, Register scratch,
                                      size_t offsetOfGroup, size_t offsetOfShape, Label* failure)
{
    // Guard against object group.
    masm.loadPtr(Address(ICStubReg, offsetOfGroup), scratch);
    masm.branchPtr(Assembler::NotEqual, Address(object, JSObject::offsetOfGroup()), scratch,
                   failure);

    // Guard against shape or expando shape.
    masm.loadPtr(Address(ICStubReg, offsetOfShape), scratch);
    if (obj->is<UnboxedPlainObject>()) {
        Address expandoAddress(object, UnboxedPlainObject::offsetOfExpando());
        masm.branchPtr(Assembler::Equal, expandoAddress, ImmWord(0), failure);
        Label done;
        masm.push(object);
        masm.loadPtr(expandoAddress, object);
        masm.branchTestObjShape(Assembler::Equal, object, scratch, &done);
        masm.pop(object);
        masm.jump(failure);
        masm.bind(&done);
        masm.pop(object);
    } else {
        masm.branchTestObjShape(Assembler::NotEqual, object, scratch, failure);
    }
}

bool
ICSetProp_Native::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;

    // Guard input is an object.
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);
    Register objReg = masm.extractObject(R0, ExtractTemp0);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratch = regs.takeAny();

    GuardGroupAndShapeMaybeUnboxedExpando(masm, obj_, objReg, scratch,
                                          ICSetProp_Native::offsetOfGroup(),
                                          ICSetProp_Native::offsetOfShape(),
                                          &failure);

    // Stow both R0 and R1 (object and value).
    EmitStowICValues(masm, 2);

    // Type update stub expects the value to check in R0.
    masm.moveValue(R1, R0);

    // Call the type-update stub.
    if (!callTypeUpdateIC(masm, sizeof(Value)))
        return false;

    // Unstow R0 and R1 (object and key)
    EmitUnstowICValues(masm, 2);

    regs.add(R0);
    regs.takeUnchecked(objReg);

    Register holderReg;
    if (obj_->is<UnboxedPlainObject>()) {
        // We are loading off the expando object, so use that for the holder.
        holderReg = regs.takeAny();
        masm.loadPtr(Address(objReg, UnboxedPlainObject::offsetOfExpando()), holderReg);
        if (!isFixedSlot_)
            masm.loadPtr(Address(holderReg, NativeObject::offsetOfSlots()), holderReg);
    } else if (isFixedSlot_) {
        holderReg = objReg;
    } else {
        holderReg = regs.takeAny();
        masm.loadPtr(Address(objReg, NativeObject::offsetOfSlots()), holderReg);
    }

    // Perform the store.
    masm.load32(Address(ICStubReg, ICSetProp_Native::offsetOfOffset()), scratch);
    EmitPreBarrier(masm, BaseIndex(holderReg, scratch, TimesOne), MIRType_Value);
    masm.storeValue(R1, BaseIndex(holderReg, scratch, TimesOne));
    if (holderReg != objReg)
        regs.add(holderReg);
    if (cx->runtime()->gc.nursery.exists()) {
        Register scr = regs.takeAny();
        LiveGeneralRegisterSet saveRegs;
        saveRegs.add(R1);
        emitPostWriteBarrierSlot(masm, objReg, R1, scr, saveRegs);
        regs.add(scr);
    }

    // The RHS has to be in R0.
    masm.moveValue(R1, R0);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

ICUpdatedStub*
ICSetPropNativeAddCompiler::getStub(ICStubSpace* space)
{
    Rooted<ShapeVector> shapes(cx, ShapeVector(cx));
    if (!shapes.append(oldShape_))
        return nullptr;

    if (!GetProtoShapes(obj_, protoChainDepth_, &shapes))
        return nullptr;

    JS_STATIC_ASSERT(ICSetProp_NativeAdd::MAX_PROTO_CHAIN_DEPTH == 4);

    ICUpdatedStub* stub = nullptr;
    switch(protoChainDepth_) {
      case 0: stub = getStubSpecific<0>(space, shapes); break;
      case 1: stub = getStubSpecific<1>(space, shapes); break;
      case 2: stub = getStubSpecific<2>(space, shapes); break;
      case 3: stub = getStubSpecific<3>(space, shapes); break;
      case 4: stub = getStubSpecific<4>(space, shapes); break;
      default: MOZ_CRASH("ProtoChainDepth too high.");
    }
    if (!stub || !stub->initUpdatingChain(cx, space))
        return nullptr;
    return stub;
}

bool
ICSetPropNativeAddCompiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    Label failureUnstow;

    // Guard input is an object.
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);
    Register objReg = masm.extractObject(R0, ExtractTemp0);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratch = regs.takeAny();

    GuardGroupAndShapeMaybeUnboxedExpando(masm, obj_, objReg, scratch,
                                          ICSetProp_NativeAdd::offsetOfGroup(),
                                          ICSetProp_NativeAddImpl<0>::offsetOfShape(0),
                                          &failure);

    // Stow both R0 and R1 (object and value).
    EmitStowICValues(masm, 2);

    regs = availableGeneralRegs(1);
    scratch = regs.takeAny();
    Register protoReg = regs.takeAny();
    // Check the proto chain.
    for (size_t i = 0; i < protoChainDepth_; i++) {
        masm.loadObjProto(i == 0 ? objReg : protoReg, protoReg);
        masm.branchTestPtr(Assembler::Zero, protoReg, protoReg, &failureUnstow);
        masm.loadPtr(Address(ICStubReg, ICSetProp_NativeAddImpl<0>::offsetOfShape(i + 1)),
                     scratch);
        masm.branchTestObjShape(Assembler::NotEqual, protoReg, scratch, &failureUnstow);
    }

    // Shape and type checks succeeded, ok to proceed.

    // Load RHS into R0 for TypeUpdate check.
    // Stack is currently: [..., ObjValue, RHSValue, MaybeReturnAddr? ]
    masm.loadValue(Address(masm.getStackPointer(), ICStackValueOffset), R0);

    // Call the type-update stub.
    if (!callTypeUpdateIC(masm, sizeof(Value)))
        return false;

    // Unstow R0 and R1 (object and key)
    EmitUnstowICValues(masm, 2);
    regs = availableGeneralRegs(2);
    scratch = regs.takeAny();

    if (obj_->is<PlainObject>()) {
        // Try to change the object's group.
        Label noGroupChange;

        // Check if the cache has a new group to change to.
        masm.loadPtr(Address(ICStubReg, ICSetProp_NativeAdd::offsetOfNewGroup()), scratch);
        masm.branchTestPtr(Assembler::Zero, scratch, scratch, &noGroupChange);

        // Check if the old group still has a newScript.
        masm.loadPtr(Address(objReg, JSObject::offsetOfGroup()), scratch);
        masm.branchPtr(Assembler::Equal,
                       Address(scratch, ObjectGroup::offsetOfAddendum()),
                       ImmWord(0),
                       &noGroupChange);

        // Reload the new group from the cache.
        masm.loadPtr(Address(ICStubReg, ICSetProp_NativeAdd::offsetOfNewGroup()), scratch);

        // Change the object's group.
        Address groupAddr(objReg, JSObject::offsetOfGroup());
        EmitPreBarrier(masm, groupAddr, MIRType_ObjectGroup);
        masm.storePtr(scratch, groupAddr);

        masm.bind(&noGroupChange);
    }

    Register holderReg;
    regs.add(R0);
    regs.takeUnchecked(objReg);

    if (obj_->is<UnboxedPlainObject>()) {
        holderReg = regs.takeAny();
        masm.loadPtr(Address(objReg, UnboxedPlainObject::offsetOfExpando()), holderReg);

        // Write the expando object's new shape.
        Address shapeAddr(holderReg, JSObject::offsetOfShape());
        EmitPreBarrier(masm, shapeAddr, MIRType_Shape);
        masm.loadPtr(Address(ICStubReg, ICSetProp_NativeAdd::offsetOfNewShape()), scratch);
        masm.storePtr(scratch, shapeAddr);

        if (!isFixedSlot_)
            masm.loadPtr(Address(holderReg, NativeObject::offsetOfSlots()), holderReg);
    } else {
        // Write the object's new shape.
        Address shapeAddr(objReg, JSObject::offsetOfShape());
        EmitPreBarrier(masm, shapeAddr, MIRType_Shape);
        masm.loadPtr(Address(ICStubReg, ICSetProp_NativeAdd::offsetOfNewShape()), scratch);
        masm.storePtr(scratch, shapeAddr);

        if (isFixedSlot_) {
            holderReg = objReg;
        } else {
            holderReg = regs.takeAny();
            masm.loadPtr(Address(objReg, NativeObject::offsetOfSlots()), holderReg);
        }
    }

    // Perform the store.  No write barrier required since this is a new
    // initialization.
    masm.load32(Address(ICStubReg, ICSetProp_NativeAdd::offsetOfOffset()), scratch);
    masm.storeValue(R1, BaseIndex(holderReg, scratch, TimesOne));

    if (holderReg != objReg)
        regs.add(holderReg);

    if (cx->runtime()->gc.nursery.exists()) {
        Register scr = regs.takeAny();
        LiveGeneralRegisterSet saveRegs;
        saveRegs.add(R1);
        emitPostWriteBarrierSlot(masm, objReg, R1, scr, saveRegs);
    }

    // The RHS has to be in R0.
    masm.moveValue(R1, R0);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failureUnstow);
    EmitUnstowICValues(masm, 2);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICSetProp_Unboxed::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;

    // Guard input is an object.
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratch = regs.takeAny();

    // Unbox and group guard.
    Register object = masm.extractObject(R0, ExtractTemp0);
    masm.loadPtr(Address(ICStubReg, ICSetProp_Unboxed::offsetOfGroup()), scratch);
    masm.branchPtr(Assembler::NotEqual, Address(object, JSObject::offsetOfGroup()), scratch,
                   &failure);

    if (needsUpdateStubs()) {
        // Stow both R0 and R1 (object and value).
        masm.push(object);
        masm.push(ICStubReg);
        EmitStowICValues(masm, 2);

        // Move RHS into R0 for TypeUpdate check.
        masm.moveValue(R1, R0);

        // Call the type update stub.
        if (!callTypeUpdateIC(masm, sizeof(Value)))
            return false;

        // Unstow R0 and R1 (object and key)
        EmitUnstowICValues(masm, 2);
        masm.pop(ICStubReg);
        masm.pop(object);

        // Trigger post barriers here on the values being written. Fields which
        // objects can be written to also need update stubs.
        LiveGeneralRegisterSet saveRegs;
        saveRegs.add(R0);
        saveRegs.add(R1);
        saveRegs.addUnchecked(object);
        saveRegs.add(ICStubReg);
        emitPostWriteBarrierSlot(masm, object, R1, scratch, saveRegs);
    }

    // Compute the address being written to.
    masm.load32(Address(ICStubReg, ICSetProp_Unboxed::offsetOfFieldOffset()), scratch);
    BaseIndex address(object, scratch, TimesOne);

    EmitUnboxedPreBarrierForBaseline(masm, address, fieldType_);
    masm.storeUnboxedProperty(address, fieldType_,
                              ConstantOrRegister(TypedOrValueRegister(R1)), &failure);

    // The RHS has to be in R0.
    masm.moveValue(R1, R0);

    EmitReturnFromIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICSetProp_TypedObject::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;

    CheckForNeuteredTypedObject(cx, masm, &failure);

    // Guard input is an object.
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratch = regs.takeAny();

    // Unbox and shape guard.
    Register object = masm.extractObject(R0, ExtractTemp0);
    masm.loadPtr(Address(ICStubReg, ICSetProp_TypedObject::offsetOfShape()), scratch);
    masm.branchTestObjShape(Assembler::NotEqual, object, scratch, &failure);

    // Guard that the object group matches.
    masm.loadPtr(Address(ICStubReg, ICSetProp_TypedObject::offsetOfGroup()), scratch);
    masm.branchPtr(Assembler::NotEqual, Address(object, JSObject::offsetOfGroup()), scratch,
                   &failure);

    if (needsUpdateStubs()) {
        // Stow both R0 and R1 (object and value).
        masm.push(object);
        masm.push(ICStubReg);
        EmitStowICValues(masm, 2);

        // Move RHS into R0 for TypeUpdate check.
        masm.moveValue(R1, R0);

        // Call the type update stub.
        if (!callTypeUpdateIC(masm, sizeof(Value)))
            return false;

        // Unstow R0 and R1 (object and key)
        EmitUnstowICValues(masm, 2);
        masm.pop(ICStubReg);
        masm.pop(object);

        // Trigger post barriers here on the values being written. Descriptors
        // which can write objects also need update stubs.
        LiveGeneralRegisterSet saveRegs;
        saveRegs.add(R0);
        saveRegs.add(R1);
        saveRegs.addUnchecked(object);
        saveRegs.add(ICStubReg);
        emitPostWriteBarrierSlot(masm, object, R1, scratch, saveRegs);
    }

    // Save the rhs on the stack so we can get a second scratch register.
    Label failurePopRHS;
    masm.pushValue(R1);
    regs = availableGeneralRegs(1);
    regs.takeUnchecked(object);
    regs.take(scratch);
    Register secondScratch = regs.takeAny();

    // Get the object's data pointer.
    LoadTypedThingData(masm, layout_, object, scratch);

    // Compute the address being written to.
    masm.load32(Address(ICStubReg, ICSetProp_TypedObject::offsetOfFieldOffset()), secondScratch);
    masm.addPtr(secondScratch, scratch);

    Address dest(scratch, 0);
    Address value(masm.getStackPointer(), 0);

    if (fieldDescr_->is<ScalarTypeDescr>()) {
        Scalar::Type type = fieldDescr_->as<ScalarTypeDescr>().type();
        StoreToTypedArray(cx, masm, type, value, dest,
                          secondScratch, &failurePopRHS, &failurePopRHS);
        masm.popValue(R1);
        EmitReturnFromIC(masm);
    } else {
        ReferenceTypeDescr::Type type = fieldDescr_->as<ReferenceTypeDescr>().type();

        masm.popValue(R1);

        switch (type) {
          case ReferenceTypeDescr::TYPE_ANY:
            EmitPreBarrier(masm, dest, MIRType_Value);
            masm.storeValue(R1, dest);
            break;

          case ReferenceTypeDescr::TYPE_OBJECT: {
            EmitPreBarrier(masm, dest, MIRType_Object);
            Label notObject;
            masm.branchTestObject(Assembler::NotEqual, R1, &notObject);
            Register rhsObject = masm.extractObject(R1, ExtractTemp0);
            masm.storePtr(rhsObject, dest);
            EmitReturnFromIC(masm);
            masm.bind(&notObject);
            masm.branchTestNull(Assembler::NotEqual, R1, &failure);
            masm.storePtr(ImmWord(0), dest);
            break;
          }

          case ReferenceTypeDescr::TYPE_STRING: {
            EmitPreBarrier(masm, dest, MIRType_String);
            masm.branchTestString(Assembler::NotEqual, R1, &failure);
            Register rhsString = masm.extractString(R1, ExtractTemp0);
            masm.storePtr(rhsString, dest);
            break;
          }

          default:
            MOZ_CRASH();
        }

        EmitReturnFromIC(masm);
    }

    masm.bind(&failurePopRHS);
    masm.popValue(R1);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICSetProp_CallScripted::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    Label failureUnstow;
    Label failureLeaveStubFrame;

    // Guard input is an object.
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    // Stow R0 and R1 to free up registers.
    EmitStowICValues(masm, 2);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(1));
    Register scratch = regs.takeAnyExcluding(ICTailCallReg);

    // Unbox and shape guard.
    Register objReg = masm.extractObject(R0, ExtractTemp0);
    GuardReceiverObject(masm, ReceiverGuard(receiver_), objReg, scratch,
                        ICSetProp_CallScripted::offsetOfReceiverGuard(), &failureUnstow);

    if (receiver_ != holder_) {
        Register holderReg = regs.takeAny();
        masm.loadPtr(Address(ICStubReg, ICSetProp_CallScripted::offsetOfHolder()), holderReg);
        masm.loadPtr(Address(ICStubReg, ICSetProp_CallScripted::offsetOfHolderShape()), scratch);
        masm.branchTestObjShape(Assembler::NotEqual, holderReg, scratch, &failureUnstow);
        regs.add(holderReg);
    }

    // Push a stub frame so that we can perform a non-tail call.
    enterStubFrame(masm, scratch);

    // Load callee function and code.  To ensure that |code| doesn't end up being
    // ArgumentsRectifierReg, if it's available we assign it to |callee| instead.
    Register callee;
    if (regs.has(ArgumentsRectifierReg)) {
        callee = ArgumentsRectifierReg;
        regs.take(callee);
    } else {
        callee = regs.takeAny();
    }
    Register code = regs.takeAny();
    masm.loadPtr(Address(ICStubReg, ICSetProp_CallScripted::offsetOfSetter()), callee);
    masm.branchIfFunctionHasNoScript(callee, &failureLeaveStubFrame);
    masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrScript()), code);
    masm.loadBaselineOrIonRaw(code, code, &failureLeaveStubFrame);

    // Align the stack such that the JitFrameLayout is aligned on
    // JitStackAlignment.
    masm.alignJitStackBasedOnNArgs(1);

    // Setter is called with the new value as the only argument, and |obj| as thisv.
    // Note that we use Push, not push, so that callJit will align the stack
    // properly on ARM.

    // To Push R1, read it off of the stowed values on stack.
    // Stack: [ ..., R0, R1, ..STUBFRAME-HEADER.., padding? ]
    masm.PushValue(Address(BaselineFrameReg, STUB_FRAME_SIZE));
    masm.Push(R0);
    EmitBaselineCreateStubFrameDescriptor(masm, scratch);
    masm.Push(Imm32(1));  // ActualArgc is 1
    masm.Push(callee);
    masm.Push(scratch);

    // Handle arguments underflow.
    Label noUnderflow;
    masm.load16ZeroExtend(Address(callee, JSFunction::offsetOfNargs()), scratch);
    masm.branch32(Assembler::BelowOrEqual, scratch, Imm32(1), &noUnderflow);
    {
        // Call the arguments rectifier.
        MOZ_ASSERT(ArgumentsRectifierReg != code);

        JitCode* argumentsRectifier =
            cx->runtime()->jitRuntime()->getArgumentsRectifier();

        masm.movePtr(ImmGCPtr(argumentsRectifier), code);
        masm.loadPtr(Address(code, JitCode::offsetOfCode()), code);
        masm.movePtr(ImmWord(1), ArgumentsRectifierReg);
    }

    masm.bind(&noUnderflow);
    masm.callJit(code);

    leaveStubFrame(masm, true);
    // Do not care about return value from function. The original RHS should be returned
    // as the result of this operation.
    EmitUnstowICValues(masm, 2);
    masm.moveValue(R1, R0);
    EmitReturnFromIC(masm);

    // Leave stub frame and go to next stub.
    masm.bind(&failureLeaveStubFrame);
    inStubFrame_ = true;
    leaveStubFrame(masm, false);

    // Unstow R0 and R1
    masm.bind(&failureUnstow);
    EmitUnstowICValues(masm, 2);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

static bool
DoCallNativeSetter(JSContext* cx, HandleFunction callee, HandleObject obj, HandleValue val)
{
    MOZ_ASSERT(callee->isNative());
    JSNative natfun = callee->native();

    JS::AutoValueArray<3> vp(cx);
    vp[0].setObject(*callee.get());
    vp[1].setObject(*obj.get());
    vp[2].set(val);

    return natfun(cx, 1, vp.begin());
}

typedef bool (*DoCallNativeSetterFn)(JSContext*, HandleFunction, HandleObject, HandleValue);
static const VMFunction DoCallNativeSetterInfo =
    FunctionInfo<DoCallNativeSetterFn>(DoCallNativeSetter);

bool
ICSetProp_CallNative::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    Label failureUnstow;

    // Guard input is an object.
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    // Stow R0 and R1 to free up registers.
    EmitStowICValues(masm, 2);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(1));
    Register scratch = regs.takeAnyExcluding(ICTailCallReg);

    // Unbox and shape guard.
    Register objReg = masm.extractObject(R0, ExtractTemp0);
    GuardReceiverObject(masm, ReceiverGuard(receiver_), objReg, scratch,
                        ICSetProp_CallNative::offsetOfReceiverGuard(), &failureUnstow);

    if (receiver_ != holder_) {
        Register holderReg = regs.takeAny();
        masm.loadPtr(Address(ICStubReg, ICSetProp_CallNative::offsetOfHolder()), holderReg);
        masm.loadPtr(Address(ICStubReg, ICSetProp_CallNative::offsetOfHolderShape()), scratch);
        masm.branchTestObjShape(Assembler::NotEqual, holderReg, scratch, &failureUnstow);
        regs.add(holderReg);
    }

    // Push a stub frame so that we can perform a non-tail call.
    enterStubFrame(masm, scratch);

    // Load callee function and code.  To ensure that |code| doesn't end up being
    // ArgumentsRectifierReg, if it's available we assign it to |callee| instead.
    Register callee = regs.takeAny();
    masm.loadPtr(Address(ICStubReg, ICSetProp_CallNative::offsetOfSetter()), callee);

    // To Push R1, read it off of the stowed values on stack.
    // Stack: [ ..., R0, R1, ..STUBFRAME-HEADER.. ]
    masm.moveStackPtrTo(scratch);
    masm.pushValue(Address(scratch, STUB_FRAME_SIZE));
    masm.push(objReg);
    masm.push(callee);

    // Don't need to preserve R0 anymore.
    regs.add(R0);

    if (!callVM(DoCallNativeSetterInfo, masm))
        return false;
    leaveStubFrame(masm);

    // Do not care about return value from function. The original RHS should be returned
    // as the result of this operation.
    EmitUnstowICValues(masm, 2);
    masm.moveValue(R1, R0);
    EmitReturnFromIC(masm);

    // Unstow R0 and R1
    masm.bind(&failureUnstow);
    EmitUnstowICValues(masm, 2);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// Call_Fallback
//

static bool
TryAttachFunApplyStub(JSContext* cx, ICCall_Fallback* stub, HandleScript script, jsbytecode* pc,
                      HandleValue thisv, uint32_t argc, Value* argv, bool* attached)
{
    if (argc != 2)
        return true;

    if (!thisv.isObject() || !thisv.toObject().is<JSFunction>())
        return true;
    RootedFunction target(cx, &thisv.toObject().as<JSFunction>());

    bool isScripted = target->hasJITCode();

    // right now, only handle situation where second argument is |arguments|
    if (argv[1].isMagic(JS_OPTIMIZED_ARGUMENTS) && !script->needsArgsObj()) {
        if (isScripted && !stub->hasStub(ICStub::Call_ScriptedApplyArguments)) {
            JitSpew(JitSpew_BaselineIC, "  Generating Call_ScriptedApplyArguments stub");

            ICCall_ScriptedApplyArguments::Compiler compiler(
                cx, stub->fallbackMonitorStub()->firstMonitorStub(), script->pcToOffset(pc));
            ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
            if (!newStub)
                return false;

            stub->addNewStub(newStub);
            *attached = true;
            return true;
        }

        // TODO: handle FUNAPPLY for native targets.
    }

    if (argv[1].isObject() && argv[1].toObject().is<ArrayObject>()) {
        if (isScripted && !stub->hasStub(ICStub::Call_ScriptedApplyArray)) {
            JitSpew(JitSpew_BaselineIC, "  Generating Call_ScriptedApplyArray stub");

            ICCall_ScriptedApplyArray::Compiler compiler(
                cx, stub->fallbackMonitorStub()->firstMonitorStub(), script->pcToOffset(pc));
            ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
            if (!newStub)
                return false;

            stub->addNewStub(newStub);
            *attached = true;
            return true;
        }
    }
    return true;
}

static bool
TryAttachFunCallStub(JSContext* cx, ICCall_Fallback* stub, HandleScript script, jsbytecode* pc,
                     HandleValue thisv, bool* attached)
{
    // Try to attach a stub for Function.prototype.call with scripted |this|.

    *attached = false;
    if (!thisv.isObject() || !thisv.toObject().is<JSFunction>())
        return true;
    RootedFunction target(cx, &thisv.toObject().as<JSFunction>());

    // Attach a stub if the script can be Baseline-compiled. We do this also
    // if the script is not yet compiled to avoid attaching a CallNative stub
    // that handles everything, even after the callee becomes hot.
    if (target->hasScript() && target->nonLazyScript()->canBaselineCompile() &&
        !stub->hasStub(ICStub::Call_ScriptedFunCall))
    {
        JitSpew(JitSpew_BaselineIC, "  Generating Call_ScriptedFunCall stub");

        ICCall_ScriptedFunCall::Compiler compiler(cx, stub->fallbackMonitorStub()->firstMonitorStub(),
                                                  script->pcToOffset(pc));
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;

        *attached = true;
        stub->addNewStub(newStub);
        return true;
    }

    return true;
}

static bool
GetTemplateObjectForNative(JSContext* cx, Native native, const CallArgs& args,
                           MutableHandleObject res, bool* skipAttach)
{
    // Check for natives to which template objects can be attached. This is
    // done to provide templates to Ion for inlining these natives later on.

    if (native == ArrayConstructor) {
        // Note: the template array won't be used if its length is inaccurately
        // computed here.  (We allocate here because compilation may occur on a
        // separate thread where allocation is impossible.)
        size_t count = 0;
        if (args.length() != 1)
            count = args.length();
        else if (args.length() == 1 && args[0].isInt32() && args[0].toInt32() >= 0)
            count = args[0].toInt32();

        if (count <= ArrayObject::EagerAllocationMaxLength) {
            ObjectGroup* group = ObjectGroup::callingAllocationSiteGroup(cx, JSProto_Array);
            if (!group)
                return false;
            if (group->maybePreliminaryObjects()) {
                *skipAttach = true;
                return true;
            }

            // With this and other array templates, set forceAnalyze so that we
            // don't end up with a template whose structure might change later.
            res.set(NewFullyAllocatedArrayForCallingAllocationSite(cx, count, TenuredObject));
            if (!res)
                return false;
            return true;
        }
    }

    if (native == js::array_concat || native == js::array_slice) {
        if (args.thisv().isObject()) {
            JSObject* obj = &args.thisv().toObject();
            if (!obj->isSingleton()) {
                if (obj->group()->maybePreliminaryObjects()) {
                    *skipAttach = true;
                    return true;
                }
                res.set(NewFullyAllocatedArrayTryReuseGroup(cx, &args.thisv().toObject(), 0,
                                                            TenuredObject));
                return !!res;
            }
        }
    }

    if (native == js::str_split && args.length() == 1 && args[0].isString()) {
        ObjectGroup* group = ObjectGroup::callingAllocationSiteGroup(cx, JSProto_Array);
        if (!group)
            return false;
        if (group->maybePreliminaryObjects()) {
            *skipAttach = true;
            return true;
        }

        res.set(NewFullyAllocatedArrayForCallingAllocationSite(cx, 0, TenuredObject));
        if (!res)
            return false;
        return true;
    }

    if (native == StringConstructor) {
        RootedString emptyString(cx, cx->runtime()->emptyString);
        res.set(StringObject::create(cx, emptyString, TenuredObject));
        return !!res;
    }

    if (native == obj_create && args.length() == 1 && args[0].isObjectOrNull()) {
        RootedObject proto(cx, args[0].toObjectOrNull());
        res.set(ObjectCreateImpl(cx, proto, TenuredObject));
        return !!res;
    }

    if (JitSupportsSimd()) {
#define ADD_INT32X4_SIMD_OP_NAME_(OP) || native == js::simd_int32x4_##OP
#define ADD_FLOAT32X4_SIMD_OP_NAME_(OP) || native == js::simd_float32x4_##OP
       if (false
           ION_COMMONX4_SIMD_OP(ADD_INT32X4_SIMD_OP_NAME_)
           COMP_COMMONX4_TO_INT32X4_SIMD_OP(ADD_INT32X4_SIMD_OP_NAME_)
           COMP_COMMONX4_TO_INT32X4_SIMD_OP(ADD_FLOAT32X4_SIMD_OP_NAME_)
           FOREACH_INT32X4_SIMD_OP(ADD_INT32X4_SIMD_OP_NAME_)
           ION_ONLY_INT32X4_SIMD_OP(ADD_INT32X4_SIMD_OP_NAME_))
       {
            Rooted<SimdTypeDescr*> descr(cx, &cx->global()->int32x4TypeDescr().as<SimdTypeDescr>());
            res.set(cx->compartment()->jitCompartment()->getSimdTemplateObjectFor(cx, descr));
            return !!res;
       }
       if (false
           FOREACH_FLOAT32X4_SIMD_OP(ADD_FLOAT32X4_SIMD_OP_NAME_)
           ION_COMMONX4_SIMD_OP(ADD_FLOAT32X4_SIMD_OP_NAME_))
       {
            Rooted<SimdTypeDescr*> descr(cx, &cx->global()->float32x4TypeDescr().as<SimdTypeDescr>());
            res.set(cx->compartment()->jitCompartment()->getSimdTemplateObjectFor(cx, descr));
            return !!res;
       }
#undef ADD_INT32X4_SIMD_OP_NAME_
#undef ADD_FLOAT32X4_SIMD_OP_NAME_
    }

    return true;
}

static bool
GetTemplateObjectForClassHook(JSContext* cx, JSNative hook, CallArgs& args,
                              MutableHandleObject templateObject)
{
    if (hook == TypedObject::construct) {
        Rooted<TypeDescr*> descr(cx, &args.callee().as<TypeDescr>());
        templateObject.set(TypedObject::createZeroed(cx, descr, 1, gc::TenuredHeap));
        return !!templateObject;
    }

    if (hook == SimdTypeDescr::call && JitSupportsSimd()) {
        Rooted<SimdTypeDescr*> descr(cx, &args.callee().as<SimdTypeDescr>());
        templateObject.set(cx->compartment()->jitCompartment()->getSimdTemplateObjectFor(cx, descr));
        return !!templateObject;
    }

    return true;
}

static bool
IsOptimizableCallStringSplit(Value callee, Value thisv, int argc, Value* args)
{
    if (argc != 1 || !thisv.isString() || !args[0].isString())
        return false;

    if (!thisv.toString()->isAtom() || !args[0].toString()->isAtom())
        return false;

    if (!callee.isObject() || !callee.toObject().is<JSFunction>())
        return false;

    JSFunction& calleeFun = callee.toObject().as<JSFunction>();
    if (!calleeFun.isNative() || calleeFun.native() != js::str_split)
        return false;

    return true;
}

static bool
TryAttachCallStub(JSContext* cx, ICCall_Fallback* stub, HandleScript script, jsbytecode* pc,
                  JSOp op, uint32_t argc, Value* vp, bool constructing, bool isSpread,
                  bool createSingleton, bool* handled)
{
    if (createSingleton || op == JSOP_EVAL || op == JSOP_STRICTEVAL)
        return true;

    if (stub->numOptimizedStubs() >= ICCall_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard all stubs in this IC and replace with inert megamorphic stub.
        // But for now we just bail.
        return true;
    }

    RootedValue callee(cx, vp[0]);
    RootedValue thisv(cx, vp[1]);

    // Don't attach an optimized call stub if we could potentially attach an
    // optimized StringSplit stub.
    if (stub->numOptimizedStubs() == 0 && IsOptimizableCallStringSplit(callee, thisv, argc, vp + 2))
        return true;

    MOZ_ASSERT_IF(stub->hasStub(ICStub::Call_StringSplit), stub->numOptimizedStubs() == 1);

    stub->unlinkStubsWithKind(cx, ICStub::Call_StringSplit);

    if (!callee.isObject())
        return true;

    RootedObject obj(cx, &callee.toObject());
    if (!obj->is<JSFunction>()) {
        // Try to attach a stub for a call/construct hook on the object.
        // Ignore proxies, which are special cased by callHook/constructHook.
        if (obj->is<ProxyObject>())
            return true;
        if (JSNative hook = constructing ? obj->constructHook() : obj->callHook()) {
            if (op != JSOP_FUNAPPLY && !isSpread && !createSingleton) {
                RootedObject templateObject(cx);
                CallArgs args = CallArgsFromVp(argc, vp);
                if (!GetTemplateObjectForClassHook(cx, hook, args, &templateObject))
                    return false;

                JitSpew(JitSpew_BaselineIC, "  Generating Call_ClassHook stub");
                ICCall_ClassHook::Compiler compiler(cx, stub->fallbackMonitorStub()->firstMonitorStub(),
                                                    obj->getClass(), hook, templateObject,
                                                    script->pcToOffset(pc), constructing);
                ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
                if (!newStub)
                    return false;

                stub->addNewStub(newStub);
                *handled = true;
                return true;
            }
        }
        return true;
    }

    RootedFunction fun(cx, &obj->as<JSFunction>());

    if (fun->hasScript()) {
        // Never attach optimized scripted call stubs for JSOP_FUNAPPLY.
        // MagicArguments may escape the frame through them.
        if (op == JSOP_FUNAPPLY)
            return true;

        // If callee is not an interpreted constructor, we have to throw.
        if (constructing && !fun->isConstructor())
            return true;

        // Likewise, if the callee is a class constructor, we have to throw.
        if (!constructing && fun->isClassConstructor())
            return true;

        if (!fun->hasJITCode()) {
            // Don't treat this as an unoptimizable case, as we'll add a stub
            // when the callee becomes hot.
            *handled = true;
            return true;
        }

        // Check if this stub chain has already generalized scripted calls.
        if (stub->scriptedStubsAreGeneralized()) {
            JitSpew(JitSpew_BaselineIC, "  Chain already has generalized scripted call stub!");
            return true;
        }

        if (stub->scriptedStubCount() >= ICCall_Fallback::MAX_SCRIPTED_STUBS) {
            // Create a Call_AnyScripted stub.
            JitSpew(JitSpew_BaselineIC, "  Generating Call_AnyScripted stub (cons=%s, spread=%s)",
                    constructing ? "yes" : "no", isSpread ? "yes" : "no");
            ICCallScriptedCompiler compiler(cx, stub->fallbackMonitorStub()->firstMonitorStub(),
                                            constructing, isSpread, script->pcToOffset(pc));
            ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
            if (!newStub)
                return false;

            // Before adding new stub, unlink all previous Call_Scripted.
            stub->unlinkStubsWithKind(cx, ICStub::Call_Scripted);

            // Add new generalized stub.
            stub->addNewStub(newStub);
            *handled = true;
            return true;
        }

        // Keep track of the function's |prototype| property in type
        // information, for use during Ion compilation.
        if (IsIonEnabled(cx))
            EnsureTrackPropertyTypes(cx, fun, NameToId(cx->names().prototype));

        // Remember the template object associated with any script being called
        // as a constructor, for later use during Ion compilation.
        RootedObject templateObject(cx);
        if (constructing) {
            // If we are calling a constructor for which the new script
            // properties analysis has not been performed yet, don't attach a
            // stub. After the analysis is performed, CreateThisForFunction may
            // start returning objects with a different type, and the Ion
            // compiler will get confused.

            // Only attach a stub if the function already has a prototype and
            // we can look it up without causing side effects.
            RootedValue protov(cx);
            if (!GetPropertyPure(cx, fun, NameToId(cx->names().prototype), protov.address())) {
                JitSpew(JitSpew_BaselineIC, "  Can't purely lookup function prototype");
                return true;
            }

            if (protov.isObject()) {
                TaggedProto proto(&protov.toObject());
                ObjectGroup* group = ObjectGroup::defaultNewGroup(cx, nullptr, proto, fun);
                if (!group)
                    return false;

                if (group->newScript() && !group->newScript()->analyzed()) {
                    JitSpew(JitSpew_BaselineIC, "  Function newScript has not been analyzed");

                    // This is temporary until the analysis is perfomed, so
                    // don't treat this as unoptimizable.
                    *handled = true;
                    return true;
                }
            }

            JSObject* thisObject = CreateThisForFunction(cx, fun, TenuredObject);
            if (!thisObject)
                return false;

            if (thisObject->is<PlainObject>() || thisObject->is<UnboxedPlainObject>())
                templateObject = thisObject;
        }

        JitSpew(JitSpew_BaselineIC,
                "  Generating Call_Scripted stub (fun=%p, %s:%" PRIuSIZE ", cons=%s, spread=%s)",
                fun.get(), fun->nonLazyScript()->filename(), fun->nonLazyScript()->lineno(),
                constructing ? "yes" : "no", isSpread ? "yes" : "no");
        ICCallScriptedCompiler compiler(cx, stub->fallbackMonitorStub()->firstMonitorStub(),
                                        fun, templateObject,
                                        constructing, isSpread, script->pcToOffset(pc));
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;

        stub->addNewStub(newStub);
        *handled = true;
        return true;
    }

    if (fun->isNative() && (!constructing || (constructing && fun->isConstructor()))) {
        // Generalized native call stubs are not here yet!
        MOZ_ASSERT(!stub->nativeStubsAreGeneralized());

        // Check for JSOP_FUNAPPLY
        if (op == JSOP_FUNAPPLY) {
            if (fun->native() == fun_apply)
                return TryAttachFunApplyStub(cx, stub, script, pc, thisv, argc, vp + 2, handled);

            // Don't try to attach a "regular" optimized call stubs for FUNAPPLY ops,
            // since MagicArguments may escape through them.
            return true;
        }

        if (op == JSOP_FUNCALL && fun->native() == fun_call) {
            if (!TryAttachFunCallStub(cx, stub, script, pc, thisv, handled))
                return false;
            if (*handled)
                return true;
        }

        if (stub->nativeStubCount() >= ICCall_Fallback::MAX_NATIVE_STUBS) {
            JitSpew(JitSpew_BaselineIC,
                    "  Too many Call_Native stubs. TODO: add Call_AnyNative!");
            return true;
        }

        if (fun->native() == intrinsic_IsSuspendedStarGenerator) {
            // This intrinsic only appears in self-hosted code.
            MOZ_ASSERT(op != JSOP_NEW);
            MOZ_ASSERT(argc == 1);
            JitSpew(JitSpew_BaselineIC, "  Generating Call_IsSuspendedStarGenerator stub");

            ICCall_IsSuspendedStarGenerator::Compiler compiler(cx);
            ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
            if (!newStub)
                return false;

            stub->addNewStub(newStub);
            *handled = true;
            return true;
        }

        RootedObject templateObject(cx);
        if (MOZ_LIKELY(!isSpread)) {
            bool skipAttach = false;
            CallArgs args = CallArgsFromVp(argc, vp);
            if (!GetTemplateObjectForNative(cx, fun->native(), args, &templateObject, &skipAttach))
                return false;
            if (skipAttach) {
                *handled = true;
                return true;
            }
            MOZ_ASSERT_IF(templateObject, !templateObject->group()->maybePreliminaryObjects());
        }

        JitSpew(JitSpew_BaselineIC, "  Generating Call_Native stub (fun=%p, cons=%s, spread=%s)",
                fun.get(), constructing ? "yes" : "no", isSpread ? "yes" : "no");
        ICCall_Native::Compiler compiler(cx, stub->fallbackMonitorStub()->firstMonitorStub(),
                                         fun, templateObject, constructing, isSpread,
                                         script->pcToOffset(pc));
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub)
            return false;

        stub->addNewStub(newStub);
        *handled = true;
        return true;
    }

    return true;
}

static bool
CopyArray(JSContext* cx, HandleObject obj, MutableHandleValue result)
{
    uint32_t length = GetAnyBoxedOrUnboxedArrayLength(obj);
    JSObject* nobj = NewFullyAllocatedArrayTryReuseGroup(cx, obj, length, TenuredObject,
                                                         /* forceAnalyze = */ true);
    if (!nobj)
        return false;
    CopyAnyBoxedOrUnboxedDenseElements(cx, nobj, obj, 0, 0, length);

    result.setObject(*nobj);
    return true;
}

static bool
TryAttachStringSplit(JSContext* cx, ICCall_Fallback* stub, HandleScript script,
                     uint32_t argc, Value* vp, jsbytecode* pc, HandleValue res,
                     bool* attached)
{
    if (stub->numOptimizedStubs() != 0)
        return true;

    RootedValue callee(cx, vp[0]);
    RootedValue thisv(cx, vp[1]);
    Value* args = vp + 2;

    // String.prototype.split will not yield a constructable.
    if (JSOp(*pc) == JSOP_NEW)
        return true;

    if (!IsOptimizableCallStringSplit(callee, thisv, argc, args))
        return true;

    MOZ_ASSERT(callee.isObject());
    MOZ_ASSERT(callee.toObject().is<JSFunction>());

    RootedString thisString(cx, thisv.toString());
    RootedString argString(cx, args[0].toString());
    RootedObject obj(cx, &res.toObject());
    RootedValue arr(cx);

    // Copy the array before storing in stub.
    if (!CopyArray(cx, obj, &arr))
        return false;

    // Atomize all elements of the array.
    RootedObject arrObj(cx, &arr.toObject());
    uint32_t initLength = GetAnyBoxedOrUnboxedArrayLength(arrObj);
    for (uint32_t i = 0; i < initLength; i++) {
        JSAtom* str = js::AtomizeString(cx, GetAnyBoxedOrUnboxedDenseElement(arrObj, i).toString());
        if (!str)
            return false;

        if (!SetAnyBoxedOrUnboxedDenseElement(cx, arrObj, i, StringValue(str))) {
            // The value could not be stored to an unboxed dense element.
            return true;
        }
    }

    ICCall_StringSplit::Compiler compiler(cx, stub->fallbackMonitorStub()->firstMonitorStub(),
                                          script->pcToOffset(pc), thisString, argString,
                                          arr);
    ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
    if (!newStub)
        return false;

    stub->addNewStub(newStub);
    *attached = true;
    return true;
}

static bool
DoCallFallback(JSContext* cx, BaselineFrame* frame, ICCall_Fallback* stub_, uint32_t argc,
               Value* vp, MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICCall_Fallback*> stub(frame, stub_);

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    JSOp op = JSOp(*pc);
    FallbackICSpew(cx, stub, "Call(%s)", js_CodeName[op]);

    MOZ_ASSERT(argc == GET_ARGC(pc));
    bool constructing = (op == JSOP_NEW);

    // Ensure vp array is rooted - we may GC in here.
    AutoArrayRooter vpRoot(cx, argc + 2 + constructing, vp);

    RootedValue callee(cx, vp[0]);
    RootedValue thisv(cx, vp[1]);

    Value* args = vp + 2;

    // Handle funapply with JSOP_ARGUMENTS
    if (op == JSOP_FUNAPPLY && argc == 2 && args[1].isMagic(JS_OPTIMIZED_ARGUMENTS)) {
        CallArgs callArgs = CallArgsFromVp(argc, vp);
        if (!GuardFunApplyArgumentsOptimization(cx, frame, callArgs))
            return false;
    }

    bool createSingleton = ObjectGroup::useSingletonForNewObject(cx, script, pc);

    // Try attaching a call stub.
    bool handled = false;
    if (!TryAttachCallStub(cx, stub, script, pc, op, argc, vp, constructing, false,
                           createSingleton, &handled))
    {
        return false;
    }

    if (op == JSOP_NEW) {
        // Callees from the stack could have any old non-constructor callee.
        if (!IsConstructor(callee)) {
            ReportValueError(cx, JSMSG_NOT_CONSTRUCTOR, JSDVG_IGNORE_STACK, callee, nullptr);
            return false;
        }

        ConstructArgs cargs(cx);
        if (!cargs.init(argc))
            return false;

        for (uint32_t i = 0; i < argc; i++)
            cargs[i].set(args[i]);

        RootedValue newTarget(cx, args[argc]);
        MOZ_ASSERT(IsConstructor(newTarget),
                   "either callee == newTarget, or the initial |new| checked "
                   "that IsConstructor(newTarget)");

        if (!Construct(cx, callee, cargs, newTarget, res))
            return false;
    } else if ((op == JSOP_EVAL || op == JSOP_STRICTEVAL) &&
               frame->scopeChain()->global().valueIsEval(callee))
    {
        if (!DirectEval(cx, CallArgsFromVp(argc, vp)))
            return false;
        res.set(vp[0]);
    } else {
        MOZ_ASSERT(op == JSOP_CALL ||
                   op == JSOP_FUNCALL ||
                   op == JSOP_FUNAPPLY ||
                   op == JSOP_EVAL ||
                   op == JSOP_STRICTEVAL);
        if (!Invoke(cx, thisv, callee, argc, args, res))
            return false;
    }

    TypeScript::Monitor(cx, script, pc, res);

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid())
        return true;

    // Attach a new TypeMonitor stub for this value.
    ICTypeMonitor_Fallback* typeMonFbStub = stub->fallbackMonitorStub();
    if (!typeMonFbStub->addMonitorStubForValue(cx, script, res))
        return false;
    // Add a type monitor stub for the resulting value.
    if (!stub->addMonitorStubForValue(cx, script, res))
        return false;

    // If 'callee' is a potential Call_StringSplit, try to attach an
    // optimized StringSplit stub.
    if (!TryAttachStringSplit(cx, stub, script, argc, vp, pc, res, &handled))
        return false;

    if (!handled)
        stub->noteUnoptimizableCall();
    return true;
}

static bool
DoSpreadCallFallback(JSContext* cx, BaselineFrame* frame, ICCall_Fallback* stub_, Value* vp,
                     MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICCall_Fallback*> stub(frame, stub_);

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    JSOp op = JSOp(*pc);
    bool constructing = (op == JSOP_SPREADNEW);
    FallbackICSpew(cx, stub, "SpreadCall(%s)", js_CodeName[op]);

    // Ensure vp array is rooted - we may GC in here.
    AutoArrayRooter vpRoot(cx, 3 + constructing, vp);

    RootedValue callee(cx, vp[0]);
    RootedValue thisv(cx, vp[1]);
    RootedValue arr(cx, vp[2]);
    RootedValue newTarget(cx, constructing ? vp[3] : NullValue());

    // Try attaching a call stub.
    bool handled = false;
    if (op != JSOP_SPREADEVAL && op != JSOP_STRICTSPREADEVAL &&
        !TryAttachCallStub(cx, stub, script, pc, op, 1, vp, constructing, true, false,
                           &handled))
    {
        return false;
    }

    if (!SpreadCallOperation(cx, script, pc, thisv, callee, arr, newTarget, res))
        return false;

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid())
        return true;

    // Attach a new TypeMonitor stub for this value.
    ICTypeMonitor_Fallback* typeMonFbStub = stub->fallbackMonitorStub();
    if (!typeMonFbStub->addMonitorStubForValue(cx, script, res))
        return false;
    // Add a type monitor stub for the resulting value.
    if (!stub->addMonitorStubForValue(cx, script, res))
        return false;

    if (!handled)
        stub->noteUnoptimizableCall();
    return true;
}

void
ICCallStubCompiler::pushCallArguments(MacroAssembler& masm, AllocatableGeneralRegisterSet regs,
                                      Register argcReg, bool isJitCall, bool isConstructing)
{
    MOZ_ASSERT(!regs.has(argcReg));

    // Account for new.target
    Register count = regs.takeAny();

    masm.move32(argcReg, count);

    // If we are setting up for a jitcall, we have to align the stack taking
    // into account the args and newTarget. We could also count callee and |this|,
    // but it's a waste of stack space. Because we want to keep argcReg unchanged,
    // just account for newTarget initially, and add the other 2 after assuring
    // allignment.
    if (isJitCall) {
        if (isConstructing)
            masm.add32(Imm32(1), count);
    } else {
        masm.add32(Imm32(2 + isConstructing), count);
    }

    // argPtr initially points to the last argument.
    Register argPtr = regs.takeAny();
    masm.moveStackPtrTo(argPtr);

    // Skip 4 pointers pushed on top of the arguments: the frame descriptor,
    // return address, old frame pointer and stub reg.
    masm.addPtr(Imm32(STUB_FRAME_SIZE), argPtr);

    // Align the stack such that the JitFrameLayout is aligned on the
    // JitStackAlignment.
    if (isJitCall) {
        masm.alignJitStackBasedOnNArgs(count);

        // Account for callee and |this|, skipped earlier
        masm.add32(Imm32(2), count);
    }

    // Push all values, starting at the last one.
    Label loop, done;
    masm.bind(&loop);
    masm.branchTest32(Assembler::Zero, count, count, &done);
    {
        masm.pushValue(Address(argPtr, 0));
        masm.addPtr(Imm32(sizeof(Value)), argPtr);

        masm.sub32(Imm32(1), count);
        masm.jump(&loop);
    }
    masm.bind(&done);
}

void
ICCallStubCompiler::guardSpreadCall(MacroAssembler& masm, Register argcReg, Label* failure,
                                    bool isConstructing)
{
    masm.unboxObject(Address(masm.getStackPointer(),
                     isConstructing * sizeof(Value) + ICStackValueOffset), argcReg);
    masm.loadPtr(Address(argcReg, NativeObject::offsetOfElements()), argcReg);
    masm.load32(Address(argcReg, ObjectElements::offsetOfLength()), argcReg);

    // Limit actual argc to something reasonable (huge number of arguments can
    // blow the stack limit).
    static_assert(ICCall_Scripted::MAX_ARGS_SPREAD_LENGTH <= ARGS_LENGTH_MAX,
                  "maximum arguments length for optimized stub should be <= ARGS_LENGTH_MAX");
    masm.branch32(Assembler::Above, argcReg, Imm32(ICCall_Scripted::MAX_ARGS_SPREAD_LENGTH),
                  failure);
}

void
ICCallStubCompiler::pushSpreadCallArguments(MacroAssembler& masm,
                                            AllocatableGeneralRegisterSet regs,
                                            Register argcReg, bool isJitCall,
                                            bool isConstructing)
{
    // Pull the array off the stack before aligning.
    Register startReg = regs.takeAny();
    masm.unboxObject(Address(masm.getStackPointer(),
                             (isConstructing * sizeof(Value)) + STUB_FRAME_SIZE), startReg);
    masm.loadPtr(Address(startReg, NativeObject::offsetOfElements()), startReg);

    // Align the stack such that the JitFrameLayout is aligned on the
    // JitStackAlignment.
    if (isJitCall) {
        Register alignReg = argcReg;
        if (isConstructing) {
            alignReg = regs.takeAny();
            masm.movePtr(argcReg, alignReg);
            masm.addPtr(Imm32(1), alignReg);
        }
        masm.alignJitStackBasedOnNArgs(alignReg);
        if (isConstructing) {
            MOZ_ASSERT(alignReg != argcReg);
            regs.add(alignReg);
        }
    }

    // Push newTarget, if necessary
    if (isConstructing)
        masm.pushValue(Address(BaselineFrameReg, STUB_FRAME_SIZE));

    // Push arguments: set up endReg to point to &array[argc]
    Register endReg = regs.takeAny();
    masm.movePtr(argcReg, endReg);
    static_assert(sizeof(Value) == 8, "Value must be 8 bytes");
    masm.lshiftPtr(Imm32(3), endReg);
    masm.addPtr(startReg, endReg);

    // Copying pre-decrements endReg by 8 until startReg is reached
    Label copyDone;
    Label copyStart;
    masm.bind(&copyStart);
    masm.branchPtr(Assembler::Equal, endReg, startReg, &copyDone);
    masm.subPtr(Imm32(sizeof(Value)), endReg);
    masm.pushValue(Address(endReg, 0));
    masm.jump(&copyStart);
    masm.bind(&copyDone);

    regs.add(startReg);
    regs.add(endReg);

    // Push the callee and |this|.
    masm.pushValue(Address(BaselineFrameReg, STUB_FRAME_SIZE + (1 + isConstructing) * sizeof(Value)));
    masm.pushValue(Address(BaselineFrameReg, STUB_FRAME_SIZE + (2 + isConstructing) * sizeof(Value)));
}

// (see Bug 1149377 comment 31) MSVC 2013 PGO miss-compiles branchTestObjClass
// calls from this function.
#if defined(_MSC_VER) && _MSC_VER == 1800
# pragma optimize("g", off)
#endif
Register
ICCallStubCompiler::guardFunApply(MacroAssembler& masm, AllocatableGeneralRegisterSet regs,
                                  Register argcReg, bool checkNative, FunApplyThing applyThing,
                                  Label* failure)
{
    // Ensure argc == 2
    masm.branch32(Assembler::NotEqual, argcReg, Imm32(2), failure);

    // Stack looks like:
    //      [..., CalleeV, ThisV, Arg0V, Arg1V <MaybeReturnReg>]

    Address secondArgSlot(masm.getStackPointer(), ICStackValueOffset);
    if (applyThing == FunApply_MagicArgs) {
        // Ensure that the second arg is magic arguments.
        masm.branchTestMagic(Assembler::NotEqual, secondArgSlot, failure);

        // Ensure that this frame doesn't have an arguments object.
        masm.branchTest32(Assembler::NonZero,
                          Address(BaselineFrameReg, BaselineFrame::reverseOffsetOfFlags()),
                          Imm32(BaselineFrame::HAS_ARGS_OBJ),
                          failure);
    } else {
        MOZ_ASSERT(applyThing == FunApply_Array);

        AllocatableGeneralRegisterSet regsx = regs;

        // Ensure that the second arg is an array.
        ValueOperand secondArgVal = regsx.takeAnyValue();
        masm.loadValue(secondArgSlot, secondArgVal);

        masm.branchTestObject(Assembler::NotEqual, secondArgVal, failure);
        Register secondArgObj = masm.extractObject(secondArgVal, ExtractTemp1);

        regsx.add(secondArgVal);
        regsx.takeUnchecked(secondArgObj);

        masm.branchTestObjClass(Assembler::NotEqual, secondArgObj, regsx.getAny(),
                                &ArrayObject::class_, failure);

        // Get the array elements and ensure that initializedLength == length
        masm.loadPtr(Address(secondArgObj, NativeObject::offsetOfElements()), secondArgObj);

        Register lenReg = regsx.takeAny();
        masm.load32(Address(secondArgObj, ObjectElements::offsetOfLength()), lenReg);

        masm.branch32(Assembler::NotEqual,
                      Address(secondArgObj, ObjectElements::offsetOfInitializedLength()),
                      lenReg, failure);

        // Limit the length to something reasonable (huge number of arguments can
        // blow the stack limit).
        masm.branch32(Assembler::Above, lenReg,
                      Imm32(ICCall_ScriptedApplyArray::MAX_ARGS_ARRAY_LENGTH),
                      failure);

        // Ensure no holes.  Loop through values in array and make sure none are magic.
        // Start address is secondArgObj, end address is secondArgObj + (lenReg * sizeof(Value))
        JS_STATIC_ASSERT(sizeof(Value) == 8);
        masm.lshiftPtr(Imm32(3), lenReg);
        masm.addPtr(secondArgObj, lenReg);

        Register start = secondArgObj;
        Register end = lenReg;
        Label loop;
        Label endLoop;
        masm.bind(&loop);
        masm.branchPtr(Assembler::AboveOrEqual, start, end, &endLoop);
        masm.branchTestMagic(Assembler::Equal, Address(start, 0), failure);
        masm.addPtr(Imm32(sizeof(Value)), start);
        masm.jump(&loop);
        masm.bind(&endLoop);
    }

    // Stack now confirmed to be like:
    //      [..., CalleeV, ThisV, Arg0V, MagicValue(Arguments), <MaybeReturnAddr>]

    // Load the callee, ensure that it's fun_apply
    ValueOperand val = regs.takeAnyValue();
    Address calleeSlot(masm.getStackPointer(), ICStackValueOffset + (3 * sizeof(Value)));
    masm.loadValue(calleeSlot, val);

    masm.branchTestObject(Assembler::NotEqual, val, failure);
    Register callee = masm.extractObject(val, ExtractTemp1);

    masm.branchTestObjClass(Assembler::NotEqual, callee, regs.getAny(), &JSFunction::class_,
                            failure);
    masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrScript()), callee);

    masm.branchPtr(Assembler::NotEqual, callee, ImmPtr(fun_apply), failure);

    // Load the |thisv|, ensure that it's a scripted function with a valid baseline or ion
    // script, or a native function.
    Address thisSlot(masm.getStackPointer(), ICStackValueOffset + (2 * sizeof(Value)));
    masm.loadValue(thisSlot, val);

    masm.branchTestObject(Assembler::NotEqual, val, failure);
    Register target = masm.extractObject(val, ExtractTemp1);
    regs.add(val);
    regs.takeUnchecked(target);

    masm.branchTestObjClass(Assembler::NotEqual, target, regs.getAny(), &JSFunction::class_,
                            failure);

    if (checkNative) {
        masm.branchIfInterpreted(target, failure);
    } else {
        masm.branchIfFunctionHasNoScript(target, failure);
        Register temp = regs.takeAny();
        masm.loadPtr(Address(target, JSFunction::offsetOfNativeOrScript()), temp);
        masm.loadBaselineOrIonRaw(temp, temp, failure);
        regs.add(temp);
    }
    return target;
}
#if defined(_MSC_VER) && _MSC_VER == 1800
# pragma optimize("", on)
#endif

void
ICCallStubCompiler::pushCallerArguments(MacroAssembler& masm, AllocatableGeneralRegisterSet regs)
{
    // Initialize copyReg to point to start caller arguments vector.
    // Initialize argcReg to poitn to the end of it.
    Register startReg = regs.takeAny();
    Register endReg = regs.takeAny();
    masm.loadPtr(Address(BaselineFrameReg, 0), startReg);
    masm.loadPtr(Address(startReg, BaselineFrame::offsetOfNumActualArgs()), endReg);
    masm.addPtr(Imm32(BaselineFrame::offsetOfArg(0)), startReg);
    masm.alignJitStackBasedOnNArgs(endReg);
    masm.lshiftPtr(Imm32(ValueShift), endReg);
    masm.addPtr(startReg, endReg);

    // Copying pre-decrements endReg by 8 until startReg is reached
    Label copyDone;
    Label copyStart;
    masm.bind(&copyStart);
    masm.branchPtr(Assembler::Equal, endReg, startReg, &copyDone);
    masm.subPtr(Imm32(sizeof(Value)), endReg);
    masm.pushValue(Address(endReg, 0));
    masm.jump(&copyStart);
    masm.bind(&copyDone);
}

void
ICCallStubCompiler::pushArrayArguments(MacroAssembler& masm, Address arrayVal,
                                       AllocatableGeneralRegisterSet regs)
{
    // Load start and end address of values to copy.
    // guardFunApply has already gauranteed that the array is packed and contains
    // no holes.
    Register startReg = regs.takeAny();
    Register endReg = regs.takeAny();
    masm.extractObject(arrayVal, startReg);
    masm.loadPtr(Address(startReg, NativeObject::offsetOfElements()), startReg);
    masm.load32(Address(startReg, ObjectElements::offsetOfInitializedLength()), endReg);
    masm.alignJitStackBasedOnNArgs(endReg);
    masm.lshiftPtr(Imm32(ValueShift), endReg);
    masm.addPtr(startReg, endReg);

    // Copying pre-decrements endReg by 8 until startReg is reached
    Label copyDone;
    Label copyStart;
    masm.bind(&copyStart);
    masm.branchPtr(Assembler::Equal, endReg, startReg, &copyDone);
    masm.subPtr(Imm32(sizeof(Value)), endReg);
    masm.pushValue(Address(endReg, 0));
    masm.jump(&copyStart);
    masm.bind(&copyDone);
}

typedef bool (*DoCallFallbackFn)(JSContext*, BaselineFrame*, ICCall_Fallback*,
                                 uint32_t, Value*, MutableHandleValue);
static const VMFunction DoCallFallbackInfo = FunctionInfo<DoCallFallbackFn>(DoCallFallback);

typedef bool (*DoSpreadCallFallbackFn)(JSContext*, BaselineFrame*, ICCall_Fallback*,
                                       Value*, MutableHandleValue);
static const VMFunction DoSpreadCallFallbackInfo =
    FunctionInfo<DoSpreadCallFallbackFn>(DoSpreadCallFallback);

bool
ICCall_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    MOZ_ASSERT(R0 == JSReturnOperand);

    // Values are on the stack left-to-right. Calling convention wants them
    // right-to-left so duplicate them on the stack in reverse order.
    // |this| and callee are pushed last.

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));

    if (MOZ_UNLIKELY(isSpread_)) {
        // Push a stub frame so that we can perform a non-tail call.
        enterStubFrame(masm, R1.scratchReg());

        // Use BaselineFrameReg instead of BaselineStackReg, because
        // BaselineFrameReg and BaselineStackReg hold the same value just after
        // calling enterStubFrame.

        // newTarget
        if (isConstructing_)
            masm.pushValue(Address(BaselineFrameReg, STUB_FRAME_SIZE));

        // array
        uint32_t valueOffset = isConstructing_;
        masm.pushValue(Address(BaselineFrameReg, valueOffset++ * sizeof(Value) + STUB_FRAME_SIZE));

        // this
        masm.pushValue(Address(BaselineFrameReg, valueOffset++ * sizeof(Value) + STUB_FRAME_SIZE));

        // callee
        masm.pushValue(Address(BaselineFrameReg, valueOffset++ * sizeof(Value) + STUB_FRAME_SIZE));

        masm.push(masm.getStackPointer());
        masm.push(ICStubReg);

        pushFramePtr(masm, R0.scratchReg());

        if (!callVM(DoSpreadCallFallbackInfo, masm))
            return false;

        leaveStubFrame(masm);
        EmitReturnFromIC(masm);

        // SPREADCALL is not yet supported in Ion, so do not generate asmcode for
        // bailout.
        return true;
    }

    // Push a stub frame so that we can perform a non-tail call.
    enterStubFrame(masm, R1.scratchReg());

    regs.take(R0.scratchReg()); // argc.

    pushCallArguments(masm, regs, R0.scratchReg(), /* isJitCall = */ false, isConstructing_);

    masm.push(masm.getStackPointer());
    masm.push(R0.scratchReg());
    masm.push(ICStubReg);

    pushFramePtr(masm, R0.scratchReg());

    if (!callVM(DoCallFallbackInfo, masm))
        return false;

    leaveStubFrame(masm);
    EmitReturnFromIC(masm);

    // The following asmcode is only used when an Ion inlined frame bails out
    // into into baseline jitcode. The return address pushed onto the
    // reconstructed baseline stack points here.
    returnOffset_ = masm.currentOffset();

    // Here we are again in a stub frame. Marking as so.
    inStubFrame_ = true;

    // Load passed-in ThisV into R1 just in case it's needed.  Need to do this before
    // we leave the stub frame since that info will be lost.
    // Current stack:  [...., ThisV, ActualArgc, CalleeToken, Descriptor ]
    masm.loadValue(Address(masm.getStackPointer(), 3 * sizeof(size_t)), R1);

    leaveStubFrame(masm, true);

    // If this is a |constructing| call, if the callee returns a non-object, we replace it with
    // the |this| object passed in.
    if (isConstructing_) {
        MOZ_ASSERT(JSReturnOperand == R0);
        Label skipThisReplace;

        masm.branchTestObject(Assembler::Equal, JSReturnOperand, &skipThisReplace);
        masm.moveValue(R1, R0);
#ifdef DEBUG
        masm.branchTestObject(Assembler::Equal, JSReturnOperand, &skipThisReplace);
        masm.assumeUnreachable("Failed to return object in constructing call.");
#endif
        masm.bind(&skipThisReplace);
    }

    // At this point, ICStubReg points to the ICCall_Fallback stub, which is NOT
    // a MonitoredStub, but rather a MonitoredFallbackStub.  To use EmitEnterTypeMonitorIC,
    // first load the ICTypeMonitor_Fallback stub into ICStubReg.  Then, use
    // EmitEnterTypeMonitorIC with a custom struct offset.
    masm.loadPtr(Address(ICStubReg, ICMonitoredFallbackStub::offsetOfFallbackMonitorStub()),
                 ICStubReg);
    EmitEnterTypeMonitorIC(masm, ICTypeMonitor_Fallback::offsetOfFirstMonitorStub());

    return true;
}

bool
ICCall_Fallback::Compiler::postGenerateStubCode(MacroAssembler& masm, Handle<JitCode*> code)
{
    if (MOZ_UNLIKELY(isSpread_))
        return true;

    CodeOffsetLabel offset(returnOffset_);
    offset.fixup(&masm);
    cx->compartment()->jitCompartment()->initBaselineCallReturnAddr(code->raw() + offset.offset(),
                                                                    isConstructing_);
    return true;
}

typedef bool (*CreateThisFn)(JSContext* cx, HandleObject callee, MutableHandleValue rval);
static const VMFunction CreateThisInfoBaseline = FunctionInfo<CreateThisFn>(CreateThis);

bool
ICCallScriptedCompiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));
    bool canUseTailCallReg = regs.has(ICTailCallReg);

    Register argcReg = R0.scratchReg();
    MOZ_ASSERT(argcReg != ArgumentsRectifierReg);

    regs.take(argcReg);
    regs.take(ArgumentsRectifierReg);
    regs.takeUnchecked(ICTailCallReg);

    if (isSpread_)
        guardSpreadCall(masm, argcReg, &failure, isConstructing_);

    // Load the callee in R1, accounting for newTarget, if necessary
    // Stack Layout: [ ..., CalleeVal, ThisVal, Arg0Val, ..., ArgNVal, [newTarget] +ICStackValueOffset+ ]
    if (isSpread_) {
        unsigned skipToCallee = (2 + isConstructing_) * sizeof(Value);
        masm.loadValue(Address(masm.getStackPointer(), skipToCallee + ICStackValueOffset), R1);
    } else {
        // Account for newTarget, if necessary
        unsigned nonArgsSkip = (1 + isConstructing_) * sizeof(Value);
        BaseValueIndex calleeSlot(masm.getStackPointer(), argcReg, ICStackValueOffset + nonArgsSkip);
        masm.loadValue(calleeSlot, R1);
    }
    regs.take(R1);

    // Ensure callee is an object.
    masm.branchTestObject(Assembler::NotEqual, R1, &failure);

    // Ensure callee is a function.
    Register callee = masm.extractObject(R1, ExtractTemp0);

    // If calling a specific script, check if the script matches.  Otherwise, ensure that
    // callee function is scripted.  Leave calleeScript in |callee| reg.
    if (callee_) {
        MOZ_ASSERT(kind == ICStub::Call_Scripted);

        // Check if the object matches this callee.
        Address expectedCallee(ICStubReg, ICCall_Scripted::offsetOfCallee());
        masm.branchPtr(Assembler::NotEqual, expectedCallee, callee, &failure);

        // Guard against relazification.
        masm.branchIfFunctionHasNoScript(callee, &failure);
    } else {
        // Ensure the object is a function.
        masm.branchTestObjClass(Assembler::NotEqual, callee, regs.getAny(), &JSFunction::class_,
                                &failure);
        if (isConstructing_) {
            masm.branchIfNotInterpretedConstructor(callee, regs.getAny(), &failure);
        } else {
            masm.branchIfFunctionHasNoScript(callee, &failure);
            masm.branchFunctionKind(Assembler::Equal, JSFunction::ClassConstructor, callee,
                                    regs.getAny(), &failure);
        }
    }

    // Load the JSScript.
    masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrScript()), callee);

    // Load the start of the target JitCode.
    Register code;
    if (!isConstructing_) {
        code = regs.takeAny();
        masm.loadBaselineOrIonRaw(callee, code, &failure);
    } else {
        Address scriptCode(callee, JSScript::offsetOfBaselineOrIonRaw());
        masm.branchPtr(Assembler::Equal, scriptCode, ImmPtr(nullptr), &failure);
    }

    // We no longer need R1.
    regs.add(R1);

    // Push a stub frame so that we can perform a non-tail call.
    enterStubFrame(masm, regs.getAny());
    if (canUseTailCallReg)
        regs.add(ICTailCallReg);

    Label failureLeaveStubFrame;

    if (isConstructing_) {
        // Save argc before call.
        masm.push(argcReg);

        // Stack now looks like:
        //      [..., Callee, ThisV, Arg0V, ..., ArgNV, NewTarget, StubFrameHeader, ArgC ]
        if (isSpread_) {
            masm.loadValue(Address(masm.getStackPointer(),
                                   3 * sizeof(Value) + STUB_FRAME_SIZE + sizeof(size_t)), R1);
        } else {
            BaseValueIndex calleeSlot2(masm.getStackPointer(), argcReg,
                                       2 * sizeof(Value) + STUB_FRAME_SIZE + sizeof(size_t));
            masm.loadValue(calleeSlot2, R1);
        }
        masm.push(masm.extractObject(R1, ExtractTemp0));
        if (!callVM(CreateThisInfoBaseline, masm))
            return false;

        // Return of CreateThis must be an object.
#ifdef DEBUG
        Label createdThisIsObject;
        masm.branchTestObject(Assembler::Equal, JSReturnOperand, &createdThisIsObject);
        masm.assumeUnreachable("The return of CreateThis must be an object.");
        masm.bind(&createdThisIsObject);
#endif

        // Reset the register set from here on in.
        MOZ_ASSERT(JSReturnOperand == R0);
        regs = availableGeneralRegs(0);
        regs.take(R0);
        regs.take(ArgumentsRectifierReg);
        argcReg = regs.takeAny();

        // Restore saved argc so we can use it to calculate the address to save
        // the resulting this object to.
        masm.pop(argcReg);

        // Save "this" value back into pushed arguments on stack.  R0 can be clobbered after that.
        // Stack now looks like:
        //      [..., Callee, ThisV, Arg0V, ..., ArgNV, [NewTarget], StubFrameHeader ]
        if (isSpread_) {
            masm.storeValue(R0, Address(masm.getStackPointer(),
                                        (1 + isConstructing_) * sizeof(Value) + STUB_FRAME_SIZE));
        } else {
            BaseValueIndex thisSlot(masm.getStackPointer(), argcReg,
                                    STUB_FRAME_SIZE + isConstructing_ * sizeof(Value));
            masm.storeValue(R0, thisSlot);
        }

        // Restore the stub register from the baseline stub frame.
        masm.loadPtr(Address(masm.getStackPointer(), STUB_FRAME_SAVED_STUB_OFFSET), ICStubReg);

        // Reload callee script. Note that a GC triggered by CreateThis may
        // have destroyed the callee BaselineScript and IonScript. CreateThis is
        // safely repeatable though, so in this case we just leave the stub frame
        // and jump to the next stub.

        // Just need to load the script now.
        if (isSpread_) {
            unsigned skipForCallee = (2 + isConstructing_) * sizeof(Value);
            masm.loadValue(Address(masm.getStackPointer(), skipForCallee + STUB_FRAME_SIZE), R0);
        } else {
            // Account for newTarget, if necessary
            unsigned nonArgsSkip = (1 + isConstructing_) * sizeof(Value);
            BaseValueIndex calleeSlot3(masm.getStackPointer(), argcReg, nonArgsSkip + STUB_FRAME_SIZE);
            masm.loadValue(calleeSlot3, R0);
        }
        callee = masm.extractObject(R0, ExtractTemp0);
        regs.add(R0);
        regs.takeUnchecked(callee);
        masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrScript()), callee);

        code = regs.takeAny();
        masm.loadBaselineOrIonRaw(callee, code, &failureLeaveStubFrame);

        // Release callee register, but don't add ExtractTemp0 back into the pool
        // ExtractTemp0 is used later, and if it's allocated to some other register at that
        // point, it will get clobbered when used.
        if (callee != ExtractTemp0)
            regs.add(callee);

        if (canUseTailCallReg)
            regs.addUnchecked(ICTailCallReg);
    }
    Register scratch = regs.takeAny();

    // Values are on the stack left-to-right. Calling convention wants them
    // right-to-left so duplicate them on the stack in reverse order.
    // |this| and callee are pushed last.
    if (isSpread_)
        pushSpreadCallArguments(masm, regs, argcReg, /* isJitCall = */ true, isConstructing_);
    else
        pushCallArguments(masm, regs, argcReg, /* isJitCall = */ true, isConstructing_);

    // The callee is on top of the stack. Pop and unbox it.
    ValueOperand val = regs.takeAnyValue();
    masm.popValue(val);
    callee = masm.extractObject(val, ExtractTemp0);

    EmitBaselineCreateStubFrameDescriptor(masm, scratch);

    // Note that we use Push, not push, so that callJit will align the stack
    // properly on ARM.
    masm.Push(argcReg);
    masm.PushCalleeToken(callee, isConstructing_);
    masm.Push(scratch);

    // Handle arguments underflow.
    Label noUnderflow;
    masm.load16ZeroExtend(Address(callee, JSFunction::offsetOfNargs()), callee);
    masm.branch32(Assembler::AboveOrEqual, argcReg, callee, &noUnderflow);
    {
        // Call the arguments rectifier.
        MOZ_ASSERT(ArgumentsRectifierReg != code);
        MOZ_ASSERT(ArgumentsRectifierReg != argcReg);

        JitCode* argumentsRectifier =
            cx->runtime()->jitRuntime()->getArgumentsRectifier();

        masm.movePtr(ImmGCPtr(argumentsRectifier), code);
        masm.loadPtr(Address(code, JitCode::offsetOfCode()), code);
        masm.movePtr(argcReg, ArgumentsRectifierReg);
    }

    masm.bind(&noUnderflow);
    masm.callJit(code);

    // If this is a constructing call, and the callee returns a non-object, replace it with
    // the |this| object passed in.
    if (isConstructing_) {
        Label skipThisReplace;
        masm.branchTestObject(Assembler::Equal, JSReturnOperand, &skipThisReplace);

        // Current stack: [ Padding?, ARGVALS..., ThisVal, ActualArgc, Callee, Descriptor ]
        // However, we can't use this ThisVal, because it hasn't been traced.  We need to use
        // The ThisVal higher up the stack:
        // Current stack: [ ThisVal, ARGVALS..., ...STUB FRAME...,
        //                  Padding?, ARGVALS..., ThisVal, ActualArgc, Callee, Descriptor ]

        // Restore the BaselineFrameReg based on the frame descriptor.
        //
        // BaselineFrameReg = BaselineStackReg
        //                  + sizeof(Descriptor) + sizeof(Callee) + sizeof(ActualArgc)
        //                  + stubFrameSize(Descriptor)
        //                  - sizeof(ICStubReg) - sizeof(BaselineFrameReg)
        Address descriptorAddr(masm.getStackPointer(), 0);
        masm.loadPtr(descriptorAddr, BaselineFrameReg);
        masm.rshiftPtr(Imm32(FRAMESIZE_SHIFT), BaselineFrameReg);
        masm.addPtr(Imm32((3 - 2) * sizeof(size_t)), BaselineFrameReg);
        masm.addStackPtrTo(BaselineFrameReg);

        // Load the number of arguments present before the stub frame.
        Register argcReg = JSReturnOperand.scratchReg();
        if (isSpread_) {
            // Account for the Array object.
            masm.move32(Imm32(1), argcReg);
        } else {
            Address argcAddr(masm.getStackPointer(), 2 * sizeof(size_t));
            masm.loadPtr(argcAddr, argcReg);
        }

        // Current stack: [ ThisVal, ARGVALS..., ...STUB FRAME..., <-- BaselineFrameReg
        //                  Padding?, ARGVALS..., ThisVal, ActualArgc, Callee, Descriptor ]
        //
        // &ThisVal = BaselineFrameReg + argc * sizeof(Value) + STUB_FRAME_SIZE + sizeof(Value)
        // This last sizeof(Value) accounts for the newTarget on the end of the arguments vector
        // which is not reflected in actualArgc
        BaseValueIndex thisSlotAddr(BaselineFrameReg, argcReg, STUB_FRAME_SIZE + sizeof(Value));
        masm.loadValue(thisSlotAddr, JSReturnOperand);
#ifdef DEBUG
        masm.branchTestObject(Assembler::Equal, JSReturnOperand, &skipThisReplace);
        masm.assumeUnreachable("Return of constructing call should be an object.");
#endif
        masm.bind(&skipThisReplace);
    }

    leaveStubFrame(masm, true);

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    // Leave stub frame and restore argc for the next stub.
    masm.bind(&failureLeaveStubFrame);
    inStubFrame_ = true;
    leaveStubFrame(masm, false);
    if (argcReg != R0.scratchReg())
        masm.movePtr(argcReg, R0.scratchReg());

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

typedef bool (*CopyArrayFn)(JSContext*, HandleObject, MutableHandleValue);
static const VMFunction CopyArrayInfo = FunctionInfo<CopyArrayFn>(CopyArray);

bool
ICCall_StringSplit::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    // Stack Layout: [ ..., CalleeVal, ThisVal, Arg0Val, +ICStackValueOffset+ ]
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));
    Label failureRestoreArgc;
#ifdef DEBUG
    Label oneArg;
    Register argcReg = R0.scratchReg();
    masm.branch32(Assembler::Equal, argcReg, Imm32(1), &oneArg);
    masm.assumeUnreachable("Expected argc == 1");
    masm.bind(&oneArg);
#endif
    Register scratchReg = regs.takeAny();

    // Guard that callee is native function js::str_split.
    {
        Address calleeAddr(masm.getStackPointer(), ICStackValueOffset + (2 * sizeof(Value)));
        ValueOperand calleeVal = regs.takeAnyValue();

        // Ensure that callee is an object.
        masm.loadValue(calleeAddr, calleeVal);
        masm.branchTestObject(Assembler::NotEqual, calleeVal, &failureRestoreArgc);

        // Ensure that callee is a function.
        Register calleeObj = masm.extractObject(calleeVal, ExtractTemp0);
        masm.branchTestObjClass(Assembler::NotEqual, calleeObj, scratchReg,
                                &JSFunction::class_, &failureRestoreArgc);

        // Ensure that callee's function impl is the native str_split.
        masm.loadPtr(Address(calleeObj, JSFunction::offsetOfNativeOrScript()), scratchReg);
        masm.branchPtr(Assembler::NotEqual, scratchReg, ImmPtr(js::str_split), &failureRestoreArgc);

        regs.add(calleeVal);
    }

    // Guard argument.
    {
        // Ensure that arg is a string.
        Address argAddr(masm.getStackPointer(), ICStackValueOffset);
        ValueOperand argVal = regs.takeAnyValue();

        masm.loadValue(argAddr, argVal);
        masm.branchTestString(Assembler::NotEqual, argVal, &failureRestoreArgc);

        Register argString = masm.extractString(argVal, ExtractTemp0);
        masm.branchPtr(Assembler::NotEqual, Address(ICStubReg, offsetOfExpectedArg()),
                       argString, &failureRestoreArgc);
        regs.add(argVal);
    }

    // Guard this-value.
    {
        // Ensure that thisv is a string.
        Address thisvAddr(masm.getStackPointer(), ICStackValueOffset + sizeof(Value));
        ValueOperand thisvVal = regs.takeAnyValue();

        masm.loadValue(thisvAddr, thisvVal);
        masm.branchTestString(Assembler::NotEqual, thisvVal, &failureRestoreArgc);

        Register thisvString = masm.extractString(thisvVal, ExtractTemp0);
        masm.branchPtr(Assembler::NotEqual, Address(ICStubReg, offsetOfExpectedThis()),
                       thisvString, &failureRestoreArgc);
        regs.add(thisvVal);
    }

    // Main stub body.
    {
        Register paramReg = regs.takeAny();

        // Push arguments.
        enterStubFrame(masm, scratchReg);
        masm.loadPtr(Address(ICStubReg, offsetOfTemplateObject()), paramReg);
        masm.push(paramReg);

        if (!callVM(CopyArrayInfo, masm))
            return false;
        leaveStubFrame(masm);
        regs.add(paramReg);
    }

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    // Guard failure path.
    masm.bind(&failureRestoreArgc);
    masm.move32(Imm32(1), R0.scratchReg());
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICCall_IsSuspendedStarGenerator::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    // The IsSuspendedStarGenerator intrinsic is only called in self-hosted
    // code, so it's safe to assume we have a single argument and the callee
    // is our intrinsic.

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));

    // Load the argument.
    Address argAddr(masm.getStackPointer(), ICStackValueOffset);
    ValueOperand argVal = regs.takeAnyValue();
    masm.loadValue(argAddr, argVal);

    // Check if it's an object.
    Label returnFalse;
    Register genObj = regs.takeAny();
    masm.branchTestObject(Assembler::NotEqual, argVal, &returnFalse);
    masm.unboxObject(argVal, genObj);

    // Check if it's a StarGeneratorObject.
    Register scratch = regs.takeAny();
    masm.branchTestObjClass(Assembler::NotEqual, genObj, scratch, &StarGeneratorObject::class_,
                            &returnFalse);

    // If the yield index slot holds an int32 value < YIELD_INDEX_CLOSING,
    // the generator is suspended.
    masm.loadValue(Address(genObj, GeneratorObject::offsetOfYieldIndexSlot()), argVal);
    masm.branchTestInt32(Assembler::NotEqual, argVal, &returnFalse);
    masm.unboxInt32(argVal, scratch);
    masm.branch32(Assembler::AboveOrEqual, scratch, Imm32(StarGeneratorObject::YIELD_INDEX_CLOSING),
                  &returnFalse);

    masm.moveValue(BooleanValue(true), R0);
    EmitReturnFromIC(masm);

    masm.bind(&returnFalse);
    masm.moveValue(BooleanValue(false), R0);
    EmitReturnFromIC(masm);
    return true;
}

bool
ICCall_Native::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));

    Register argcReg = R0.scratchReg();
    regs.take(argcReg);
    regs.takeUnchecked(ICTailCallReg);

    if (isSpread_)
        guardSpreadCall(masm, argcReg, &failure, isConstructing_);

    // Load the callee in R1.
    if (isSpread_) {
        masm.loadValue(Address(masm.getStackPointer(), ICStackValueOffset + 2 * sizeof(Value)), R1);
    } else {
        unsigned nonArgsSlots = (1 + isConstructing_) * sizeof(Value);
        BaseValueIndex calleeSlot(masm.getStackPointer(), argcReg, ICStackValueOffset + nonArgsSlots);
        masm.loadValue(calleeSlot, R1);
    }
    regs.take(R1);

    masm.branchTestObject(Assembler::NotEqual, R1, &failure);

    // Ensure callee matches this stub's callee.
    Register callee = masm.extractObject(R1, ExtractTemp0);
    Address expectedCallee(ICStubReg, ICCall_Native::offsetOfCallee());
    masm.branchPtr(Assembler::NotEqual, expectedCallee, callee, &failure);

    regs.add(R1);
    regs.takeUnchecked(callee);

    // Push a stub frame so that we can perform a non-tail call.
    // Note that this leaves the return address in TailCallReg.
    enterStubFrame(masm, regs.getAny());

    // Values are on the stack left-to-right. Calling convention wants them
    // right-to-left so duplicate them on the stack in reverse order.
    // |this| and callee are pushed last.
    if (isSpread_)
        pushSpreadCallArguments(masm, regs, argcReg, /* isJitCall = */ false, isConstructing_);
    else
        pushCallArguments(masm, regs, argcReg, /* isJitCall = */ false, isConstructing_);

    if (isConstructing_) {
        // Stack looks like: [ ..., Arg0Val, ThisVal, CalleeVal ]
        // Replace ThisVal with MagicValue(JS_IS_CONSTRUCTING)
        masm.storeValue(MagicValue(JS_IS_CONSTRUCTING), Address(masm.getStackPointer(), sizeof(Value)));
    }


    // Native functions have the signature:
    //
    //    bool (*)(JSContext*, unsigned, Value* vp)
    //
    // Where vp[0] is space for callee/return value, vp[1] is |this|, and vp[2] onward
    // are the function arguments.

    // Initialize vp.
    Register vpReg = regs.takeAny();
    masm.moveStackPtrTo(vpReg);

    // Construct a native exit frame.
    masm.push(argcReg);

    Register scratch = regs.takeAny();
    EmitBaselineCreateStubFrameDescriptor(masm, scratch);
    masm.push(scratch);
    masm.push(ICTailCallReg);
    masm.enterFakeExitFrame(NativeExitFrameLayoutToken);

    // Execute call.
    masm.setupUnalignedABICall(scratch);
    masm.loadJSContext(scratch);
    masm.passABIArg(scratch);
    masm.passABIArg(argcReg);
    masm.passABIArg(vpReg);

#ifdef JS_SIMULATOR
    // The simulator requires VM calls to be redirected to a special swi
    // instruction to handle them, so we store the redirected pointer in the
    // stub and use that instead of the original one.
    masm.callWithABI(Address(ICStubReg, ICCall_Native::offsetOfNative()));
#else
    masm.callWithABI(Address(callee, JSFunction::offsetOfNativeOrScript()));
#endif

    // Test for failure.
    masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

    // Load the return value into R0.
    masm.loadValue(Address(masm.getStackPointer(), NativeExitFrameLayout::offsetOfResult()), R0);

    leaveStubFrame(masm);

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICCall_ClassHook::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));

    Register argcReg = R0.scratchReg();
    regs.take(argcReg);
    regs.takeUnchecked(ICTailCallReg);

    // Load the callee in R1.
    unsigned nonArgSlots = (1 + isConstructing_) * sizeof(Value);
    BaseValueIndex calleeSlot(masm.getStackPointer(), argcReg, ICStackValueOffset + nonArgSlots);
    masm.loadValue(calleeSlot, R1);
    regs.take(R1);

    masm.branchTestObject(Assembler::NotEqual, R1, &failure);

    // Ensure the callee's class matches the one in this stub.
    Register callee = masm.extractObject(R1, ExtractTemp0);
    Register scratch = regs.takeAny();
    masm.loadObjClass(callee, scratch);
    masm.branchPtr(Assembler::NotEqual,
                   Address(ICStubReg, ICCall_ClassHook::offsetOfClass()),
                   scratch, &failure);

    regs.add(R1);
    regs.takeUnchecked(callee);

    // Push a stub frame so that we can perform a non-tail call.
    // Note that this leaves the return address in TailCallReg.
    enterStubFrame(masm, regs.getAny());

    regs.add(scratch);
    pushCallArguments(masm, regs, argcReg, /* isJitCall = */ false, isConstructing_);
    regs.take(scratch);

    if (isConstructing_) {
        // Stack looks like: [ ..., Arg0Val, ThisVal, CalleeVal ]
        // Replace ThisVal with MagicValue(JS_IS_CONSTRUCTING)
        masm.storeValue(MagicValue(JS_IS_CONSTRUCTING), Address(masm.getStackPointer(), sizeof(Value)));
    }

    masm.checkStackAlignment();

    // Native functions have the signature:
    //
    //    bool (*)(JSContext*, unsigned, Value* vp)
    //
    // Where vp[0] is space for callee/return value, vp[1] is |this|, and vp[2] onward
    // are the function arguments.

    // Initialize vp.
    Register vpReg = regs.takeAny();
    masm.moveStackPtrTo(vpReg);

    // Construct a native exit frame.
    masm.push(argcReg);

    EmitBaselineCreateStubFrameDescriptor(masm, scratch);
    masm.push(scratch);
    masm.push(ICTailCallReg);
    masm.enterFakeExitFrame(NativeExitFrameLayoutToken);

    // Execute call.
    masm.setupUnalignedABICall(scratch);
    masm.loadJSContext(scratch);
    masm.passABIArg(scratch);
    masm.passABIArg(argcReg);
    masm.passABIArg(vpReg);
    masm.callWithABI(Address(ICStubReg, ICCall_ClassHook::offsetOfNative()));

    // Test for failure.
    masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

    // Load the return value into R0.
    masm.loadValue(Address(masm.getStackPointer(), NativeExitFrameLayout::offsetOfResult()), R0);

    leaveStubFrame(masm);

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICCall_ScriptedApplyArray::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));

    Register argcReg = R0.scratchReg();
    regs.take(argcReg);
    regs.takeUnchecked(ICTailCallReg);
    regs.takeUnchecked(ArgumentsRectifierReg);

    //
    // Validate inputs
    //

    Register target = guardFunApply(masm, regs, argcReg, /*checkNative=*/false,
                                    FunApply_Array, &failure);
    if (regs.has(target)) {
        regs.take(target);
    } else {
        // If target is already a reserved reg, take another register for it, because it's
        // probably currently an ExtractTemp, which might get clobbered later.
        Register targetTemp = regs.takeAny();
        masm.movePtr(target, targetTemp);
        target = targetTemp;
    }

    // Push a stub frame so that we can perform a non-tail call.
    enterStubFrame(masm, regs.getAny());

    //
    // Push arguments
    //

    // Stack now looks like:
    //                                      BaselineFrameReg -------------------.
    //                                                                          v
    //      [..., fun_apply, TargetV, TargetThisV, ArgsArrayV, StubFrameHeader]

    // Push all array elements onto the stack:
    Address arrayVal(BaselineFrameReg, STUB_FRAME_SIZE);
    pushArrayArguments(masm, arrayVal, regs);

    // Stack now looks like:
    //                                      BaselineFrameReg -------------------.
    //                                                                          v
    //      [..., fun_apply, TargetV, TargetThisV, ArgsArrayV, StubFrameHeader,
    //       PushedArgN, ..., PushedArg0]
    // Can't fail after this, so it's ok to clobber argcReg.

    // Push actual argument 0 as |thisv| for call.
    masm.pushValue(Address(BaselineFrameReg, STUB_FRAME_SIZE + sizeof(Value)));

    // All pushes after this use Push instead of push to make sure ARM can align
    // stack properly for call.
    Register scratch = regs.takeAny();
    EmitBaselineCreateStubFrameDescriptor(masm, scratch);

    // Reload argc from length of array.
    masm.extractObject(arrayVal, argcReg);
    masm.loadPtr(Address(argcReg, NativeObject::offsetOfElements()), argcReg);
    masm.load32(Address(argcReg, ObjectElements::offsetOfInitializedLength()), argcReg);

    masm.Push(argcReg);
    masm.Push(target);
    masm.Push(scratch);

    // Load nargs into scratch for underflow check, and then load jitcode pointer into target.
    masm.load16ZeroExtend(Address(target, JSFunction::offsetOfNargs()), scratch);
    masm.loadPtr(Address(target, JSFunction::offsetOfNativeOrScript()), target);
    masm.loadBaselineOrIonRaw(target, target, nullptr);

    // Handle arguments underflow.
    Label noUnderflow;
    masm.branch32(Assembler::AboveOrEqual, argcReg, scratch, &noUnderflow);
    {
        // Call the arguments rectifier.
        MOZ_ASSERT(ArgumentsRectifierReg != target);
        MOZ_ASSERT(ArgumentsRectifierReg != argcReg);

        JitCode* argumentsRectifier =
            cx->runtime()->jitRuntime()->getArgumentsRectifier();

        masm.movePtr(ImmGCPtr(argumentsRectifier), target);
        masm.loadPtr(Address(target, JitCode::offsetOfCode()), target);
        masm.movePtr(argcReg, ArgumentsRectifierReg);
    }
    masm.bind(&noUnderflow);
    regs.add(argcReg);

    // Do call
    masm.callJit(target);
    leaveStubFrame(masm, true);

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICCall_ScriptedApplyArguments::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));

    Register argcReg = R0.scratchReg();
    regs.take(argcReg);
    regs.takeUnchecked(ICTailCallReg);
    regs.takeUnchecked(ArgumentsRectifierReg);

    //
    // Validate inputs
    //

    Register target = guardFunApply(masm, regs, argcReg, /*checkNative=*/false,
                                    FunApply_MagicArgs, &failure);
    if (regs.has(target)) {
        regs.take(target);
    } else {
        // If target is already a reserved reg, take another register for it, because it's
        // probably currently an ExtractTemp, which might get clobbered later.
        Register targetTemp = regs.takeAny();
        masm.movePtr(target, targetTemp);
        target = targetTemp;
    }

    // Push a stub frame so that we can perform a non-tail call.
    enterStubFrame(masm, regs.getAny());

    //
    // Push arguments
    //

    // Stack now looks like:
    //      [..., fun_apply, TargetV, TargetThisV, MagicArgsV, StubFrameHeader]

    // Push all arguments supplied to caller function onto the stack.
    pushCallerArguments(masm, regs);

    // Stack now looks like:
    //                                      BaselineFrameReg -------------------.
    //                                                                          v
    //      [..., fun_apply, TargetV, TargetThisV, MagicArgsV, StubFrameHeader,
    //       PushedArgN, ..., PushedArg0]
    // Can't fail after this, so it's ok to clobber argcReg.

    // Push actual argument 0 as |thisv| for call.
    masm.pushValue(Address(BaselineFrameReg, STUB_FRAME_SIZE + sizeof(Value)));

    // All pushes after this use Push instead of push to make sure ARM can align
    // stack properly for call.
    Register scratch = regs.takeAny();
    EmitBaselineCreateStubFrameDescriptor(masm, scratch);

    masm.loadPtr(Address(BaselineFrameReg, 0), argcReg);
    masm.loadPtr(Address(argcReg, BaselineFrame::offsetOfNumActualArgs()), argcReg);
    masm.Push(argcReg);
    masm.Push(target);
    masm.Push(scratch);

    // Load nargs into scratch for underflow check, and then load jitcode pointer into target.
    masm.load16ZeroExtend(Address(target, JSFunction::offsetOfNargs()), scratch);
    masm.loadPtr(Address(target, JSFunction::offsetOfNativeOrScript()), target);
    masm.loadBaselineOrIonRaw(target, target, nullptr);

    // Handle arguments underflow.
    Label noUnderflow;
    masm.branch32(Assembler::AboveOrEqual, argcReg, scratch, &noUnderflow);
    {
        // Call the arguments rectifier.
        MOZ_ASSERT(ArgumentsRectifierReg != target);
        MOZ_ASSERT(ArgumentsRectifierReg != argcReg);

        JitCode* argumentsRectifier =
            cx->runtime()->jitRuntime()->getArgumentsRectifier();

        masm.movePtr(ImmGCPtr(argumentsRectifier), target);
        masm.loadPtr(Address(target, JitCode::offsetOfCode()), target);
        masm.movePtr(argcReg, ArgumentsRectifierReg);
    }
    masm.bind(&noUnderflow);
    regs.add(argcReg);

    // Do call
    masm.callJit(target);
    leaveStubFrame(masm, true);

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICCall_ScriptedFunCall::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));
    bool canUseTailCallReg = regs.has(ICTailCallReg);

    Register argcReg = R0.scratchReg();
    MOZ_ASSERT(argcReg != ArgumentsRectifierReg);

    regs.take(argcReg);
    regs.take(ArgumentsRectifierReg);
    regs.takeUnchecked(ICTailCallReg);

    // Load the callee in R1.
    // Stack Layout: [ ..., CalleeVal, ThisVal, Arg0Val, ..., ArgNVal, +ICStackValueOffset+ ]
    BaseValueIndex calleeSlot(masm.getStackPointer(), argcReg, ICStackValueOffset + sizeof(Value));
    masm.loadValue(calleeSlot, R1);
    regs.take(R1);

    // Ensure callee is fun_call.
    masm.branchTestObject(Assembler::NotEqual, R1, &failure);

    Register callee = masm.extractObject(R1, ExtractTemp0);
    masm.branchTestObjClass(Assembler::NotEqual, callee, regs.getAny(), &JSFunction::class_,
                            &failure);
    masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrScript()), callee);
    masm.branchPtr(Assembler::NotEqual, callee, ImmPtr(fun_call), &failure);

    // Ensure |this| is a scripted function with JIT code.
    BaseIndex thisSlot(masm.getStackPointer(), argcReg, TimesEight, ICStackValueOffset);
    masm.loadValue(thisSlot, R1);

    masm.branchTestObject(Assembler::NotEqual, R1, &failure);
    callee = masm.extractObject(R1, ExtractTemp0);

    masm.branchTestObjClass(Assembler::NotEqual, callee, regs.getAny(), &JSFunction::class_,
                            &failure);
    masm.branchIfFunctionHasNoScript(callee, &failure);
    masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrScript()), callee);

    // Load the start of the target JitCode.
    Register code = regs.takeAny();
    masm.loadBaselineOrIonRaw(callee, code, &failure);

    // We no longer need R1.
    regs.add(R1);

    // Push a stub frame so that we can perform a non-tail call.
    enterStubFrame(masm, regs.getAny());
    if (canUseTailCallReg)
        regs.add(ICTailCallReg);

    // Decrement argc if argc > 0. If argc == 0, push |undefined| as |this|.
    Label zeroArgs, done;
    masm.branchTest32(Assembler::Zero, argcReg, argcReg, &zeroArgs);

    // Avoid the copy of the callee (function.call).
    masm.sub32(Imm32(1), argcReg);

    // Values are on the stack left-to-right. Calling convention wants them
    // right-to-left so duplicate them on the stack in reverse order.

    pushCallArguments(masm, regs, argcReg, /* isJitCall = */ true);

    // Pop scripted callee (the original |this|).
    ValueOperand val = regs.takeAnyValue();
    masm.popValue(val);

    masm.jump(&done);
    masm.bind(&zeroArgs);

    // Copy scripted callee (the original |this|).
    Address thisSlotFromStubFrame(BaselineFrameReg, STUB_FRAME_SIZE);
    masm.loadValue(thisSlotFromStubFrame, val);

    // Align the stack.
    masm.alignJitStackBasedOnNArgs(0);

    // Store the new |this|.
    masm.pushValue(UndefinedValue());

    masm.bind(&done);

    // Unbox scripted callee.
    callee = masm.extractObject(val, ExtractTemp0);

    Register scratch = regs.takeAny();
    EmitBaselineCreateStubFrameDescriptor(masm, scratch);

    // Note that we use Push, not push, so that callJit will align the stack
    // properly on ARM.
    masm.Push(argcReg);
    masm.Push(callee);
    masm.Push(scratch);

    // Handle arguments underflow.
    Label noUnderflow;
    masm.load16ZeroExtend(Address(callee, JSFunction::offsetOfNargs()), callee);
    masm.branch32(Assembler::AboveOrEqual, argcReg, callee, &noUnderflow);
    {
        // Call the arguments rectifier.
        MOZ_ASSERT(ArgumentsRectifierReg != code);
        MOZ_ASSERT(ArgumentsRectifierReg != argcReg);

        JitCode* argumentsRectifier =
            cx->runtime()->jitRuntime()->getArgumentsRectifier();

        masm.movePtr(ImmGCPtr(argumentsRectifier), code);
        masm.loadPtr(Address(code, JitCode::offsetOfCode()), code);
        masm.movePtr(argcReg, ArgumentsRectifierReg);
    }

    masm.bind(&noUnderflow);
    masm.callJit(code);

    leaveStubFrame(masm, true);

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

static bool
DoubleValueToInt32ForSwitch(Value* v)
{
    double d = v->toDouble();
    int32_t truncated = int32_t(d);
    if (d != double(truncated))
        return false;

    v->setInt32(truncated);
    return true;
}

bool
ICTableSwitch::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label isInt32, notInt32, outOfRange;
    Register scratch = R1.scratchReg();

    masm.branchTestInt32(Assembler::NotEqual, R0, &notInt32);

    Register key = masm.extractInt32(R0, ExtractTemp0);

    masm.bind(&isInt32);

    masm.load32(Address(ICStubReg, offsetof(ICTableSwitch, min_)), scratch);
    masm.sub32(scratch, key);
    masm.branch32(Assembler::BelowOrEqual,
                  Address(ICStubReg, offsetof(ICTableSwitch, length_)), key, &outOfRange);

    masm.loadPtr(Address(ICStubReg, offsetof(ICTableSwitch, table_)), scratch);
    masm.loadPtr(BaseIndex(scratch, key, ScalePointer), scratch);

    EmitChangeICReturnAddress(masm, scratch);
    EmitReturnFromIC(masm);

    masm.bind(&notInt32);

    masm.branchTestDouble(Assembler::NotEqual, R0, &outOfRange);
    if (cx->runtime()->jitSupportsFloatingPoint) {
        masm.unboxDouble(R0, FloatReg0);

        // N.B. -0 === 0, so convert -0 to a 0 int32.
        masm.convertDoubleToInt32(FloatReg0, key, &outOfRange, /* negativeZeroCheck = */ false);
    } else {
        // Pass pointer to double value.
        masm.pushValue(R0);
        masm.moveStackPtrTo(R0.scratchReg());

        masm.setupUnalignedABICall(scratch);
        masm.passABIArg(R0.scratchReg());
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, DoubleValueToInt32ForSwitch));

        // If the function returns |true|, the value has been converted to
        // int32.
        masm.movePtr(ReturnReg, scratch);
        masm.popValue(R0);
        masm.branchIfFalseBool(scratch, &outOfRange);
        masm.unboxInt32(R0, key);
    }
    masm.jump(&isInt32);

    masm.bind(&outOfRange);

    masm.loadPtr(Address(ICStubReg, offsetof(ICTableSwitch, defaultTarget_)), scratch);

    EmitChangeICReturnAddress(masm, scratch);
    EmitReturnFromIC(masm);
    return true;
}

ICStub*
ICTableSwitch::Compiler::getStub(ICStubSpace* space)
{
    JitCode* code = getStubCode();
    if (!code)
        return nullptr;

    jsbytecode* pc = pc_;
    pc += JUMP_OFFSET_LEN;
    int32_t low = GET_JUMP_OFFSET(pc);
    pc += JUMP_OFFSET_LEN;
    int32_t high = GET_JUMP_OFFSET(pc);
    int32_t length = high - low + 1;
    pc += JUMP_OFFSET_LEN;

    void** table = (void**) space->alloc(sizeof(void*) * length);
    if (!table)
        return nullptr;

    jsbytecode* defaultpc = pc_ + GET_JUMP_OFFSET(pc_);

    for (int32_t i = 0; i < length; i++) {
        int32_t off = GET_JUMP_OFFSET(pc);
        if (off)
            table[i] = pc_ + off;
        else
            table[i] = defaultpc;
        pc += JUMP_OFFSET_LEN;
    }

    return newStub<ICTableSwitch>(space, code, table, low, length, defaultpc);
}

void
ICTableSwitch::fixupJumpTable(JSScript* script, BaselineScript* baseline)
{
    defaultTarget_ = baseline->nativeCodeForPC(script, (jsbytecode*) defaultTarget_);

    for (int32_t i = 0; i < length_; i++)
        table_[i] = baseline->nativeCodeForPC(script, (jsbytecode*) table_[i]);
}

//
// IteratorNew_Fallback
//

static bool
DoIteratorNewFallback(JSContext* cx, BaselineFrame* frame, ICIteratorNew_Fallback* stub,
                      HandleValue value, MutableHandleValue res)
{
    jsbytecode* pc = stub->icEntry()->pc(frame->script());
    FallbackICSpew(cx, stub, "IteratorNew");

    uint8_t flags = GET_UINT8(pc);
    res.set(value);
    return ValueToIterator(cx, flags, res);
}

typedef bool (*DoIteratorNewFallbackFn)(JSContext*, BaselineFrame*, ICIteratorNew_Fallback*,
                                        HandleValue, MutableHandleValue);
static const VMFunction DoIteratorNewFallbackInfo =
    FunctionInfo<DoIteratorNewFallbackFn>(DoIteratorNewFallback, TailCall, PopValues(1));

bool
ICIteratorNew_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    EmitRestoreTailCallReg(masm);

    // Sync stack for the decompiler.
    masm.pushValue(R0);

    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushFramePtr(masm, R0.scratchReg());

    return tailCallVM(DoIteratorNewFallbackInfo, masm);
}

//
// IteratorMore_Fallback
//

static bool
DoIteratorMoreFallback(JSContext* cx, BaselineFrame* frame, ICIteratorMore_Fallback* stub_,
                       HandleObject iterObj, MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICIteratorMore_Fallback*> stub(frame, stub_);

    FallbackICSpew(cx, stub, "IteratorMore");

    if (!IteratorMore(cx, iterObj, res))
        return false;

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid())
        return true;

    if (!res.isMagic(JS_NO_ITER_VALUE) && !res.isString())
        stub->setHasNonStringResult();

    if (iterObj->is<PropertyIteratorObject>() &&
        !stub->hasStub(ICStub::IteratorMore_Native))
    {
        ICIteratorMore_Native::Compiler compiler(cx);
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(frame->script()));
        if (!newStub)
            return false;
        stub->addNewStub(newStub);
    }

    return true;
}

typedef bool (*DoIteratorMoreFallbackFn)(JSContext*, BaselineFrame*, ICIteratorMore_Fallback*,
                                         HandleObject, MutableHandleValue);
static const VMFunction DoIteratorMoreFallbackInfo =
    FunctionInfo<DoIteratorMoreFallbackFn>(DoIteratorMoreFallback, TailCall);

bool
ICIteratorMore_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    EmitRestoreTailCallReg(masm);

    masm.unboxObject(R0, R0.scratchReg());
    masm.push(R0.scratchReg());
    masm.push(ICStubReg);
    pushFramePtr(masm, R0.scratchReg());

    return tailCallVM(DoIteratorMoreFallbackInfo, masm);
}

//
// IteratorMore_Native
//

bool
ICIteratorMore_Native::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;

    Register obj = masm.extractObject(R0, ExtractTemp0);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(1));
    Register nativeIterator = regs.takeAny();
    Register scratch = regs.takeAny();

    masm.branchTestObjClass(Assembler::NotEqual, obj, scratch,
                            &PropertyIteratorObject::class_, &failure);
    masm.loadObjPrivate(obj, JSObject::ITER_CLASS_NFIXED_SLOTS, nativeIterator);

    masm.branchTest32(Assembler::NonZero, Address(nativeIterator, offsetof(NativeIterator, flags)),
                      Imm32(JSITER_FOREACH), &failure);

    // If props_cursor < props_end, load the next string and advance the cursor.
    // Else, return MagicValue(JS_NO_ITER_VALUE).
    Label iterDone;
    Address cursorAddr(nativeIterator, offsetof(NativeIterator, props_cursor));
    Address cursorEndAddr(nativeIterator, offsetof(NativeIterator, props_end));
    masm.loadPtr(cursorAddr, scratch);
    masm.branchPtr(Assembler::BelowOrEqual, cursorEndAddr, scratch, &iterDone);

    // Get next string.
    masm.loadPtr(Address(scratch, 0), scratch);

    // Increase the cursor.
    masm.addPtr(Imm32(sizeof(JSString*)), cursorAddr);

    masm.tagValue(JSVAL_TYPE_STRING, scratch, R0);
    EmitReturnFromIC(masm);

    masm.bind(&iterDone);
    masm.moveValue(MagicValue(JS_NO_ITER_VALUE), R0);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// IteratorClose_Fallback
//

static bool
DoIteratorCloseFallback(JSContext* cx, ICIteratorClose_Fallback* stub, HandleValue iterValue)
{
    FallbackICSpew(cx, stub, "IteratorClose");

    RootedObject iteratorObject(cx, &iterValue.toObject());
    return CloseIterator(cx, iteratorObject);
}

typedef bool (*DoIteratorCloseFallbackFn)(JSContext*, ICIteratorClose_Fallback*, HandleValue);
static const VMFunction DoIteratorCloseFallbackInfo =
    FunctionInfo<DoIteratorCloseFallbackFn>(DoIteratorCloseFallback, TailCall);

bool
ICIteratorClose_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    EmitRestoreTailCallReg(masm);

    masm.pushValue(R0);
    masm.push(ICStubReg);

    return tailCallVM(DoIteratorCloseFallbackInfo, masm);
}

//
// InstanceOf_Fallback
//

static bool
TryAttachInstanceOfStub(JSContext* cx, BaselineFrame* frame, ICInstanceOf_Fallback* stub,
                        HandleFunction fun, bool* attached)
{
    MOZ_ASSERT(!*attached);
    if (fun->isBoundFunction())
        return true;

    Shape* shape = fun->lookupPure(cx->names().prototype);
    if (!shape || !shape->hasSlot() || !shape->hasDefaultGetter())
        return true;

    uint32_t slot = shape->slot();
    MOZ_ASSERT(fun->numFixedSlots() == 0, "Stub code relies on this");

    if (!fun->getSlot(slot).isObject())
        return true;

    JSObject* protoObject = &fun->getSlot(slot).toObject();

    JitSpew(JitSpew_BaselineIC, "  Generating InstanceOf(Function) stub");
    ICInstanceOf_Function::Compiler compiler(cx, fun->lastProperty(), protoObject, slot);
    ICStub* newStub = compiler.getStub(compiler.getStubSpace(frame->script()));
    if (!newStub)
        return false;

    stub->addNewStub(newStub);
    *attached = true;
    return true;
}

static bool
DoInstanceOfFallback(JSContext* cx, BaselineFrame* frame, ICInstanceOf_Fallback* stub,
                     HandleValue lhs, HandleValue rhs, MutableHandleValue res)
{
    FallbackICSpew(cx, stub, "InstanceOf");

    if (!rhs.isObject()) {
        ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS, -1, rhs, nullptr);
        return false;
    }

    RootedObject obj(cx, &rhs.toObject());
    bool cond = false;
    if (!HasInstance(cx, obj, lhs, &cond))
        return false;

    res.setBoolean(cond);

    if (!obj->is<JSFunction>()) {
        stub->noteUnoptimizableAccess();
        return true;
    }

    // For functions, keep track of the |prototype| property in type information,
    // for use during Ion compilation.
    EnsureTrackPropertyTypes(cx, obj, NameToId(cx->names().prototype));

    if (stub->numOptimizedStubs() >= ICInstanceOf_Fallback::MAX_OPTIMIZED_STUBS)
        return true;

    RootedFunction fun(cx, &obj->as<JSFunction>());
    bool attached = false;
    if (!TryAttachInstanceOfStub(cx, frame, stub, fun, &attached))
        return false;
    if (!attached)
        stub->noteUnoptimizableAccess();
    return true;
}

typedef bool (*DoInstanceOfFallbackFn)(JSContext*, BaselineFrame*, ICInstanceOf_Fallback*,
                                       HandleValue, HandleValue, MutableHandleValue);
static const VMFunction DoInstanceOfFallbackInfo =
    FunctionInfo<DoInstanceOfFallbackFn>(DoInstanceOfFallback, TailCall, PopValues(2));

bool
ICInstanceOf_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    EmitRestoreTailCallReg(masm);

    // Sync stack for the decompiler.
    masm.pushValue(R0);
    masm.pushValue(R1);

    masm.pushValue(R1);
    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushFramePtr(masm, R0.scratchReg());

    return tailCallVM(DoInstanceOfFallbackInfo, masm);
}

bool
ICInstanceOf_Function::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    Label failure;

    // Ensure RHS is an object.
    masm.branchTestObject(Assembler::NotEqual, R1, &failure);
    Register rhsObj = masm.extractObject(R1, ExtractTemp0);

    // Allow using R1's type register as scratch. We have to restore it when
    // we want to jump to the next stub.
    Label failureRestoreR1;
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(1));
    regs.takeUnchecked(rhsObj);

    Register scratch1 = regs.takeAny();
    Register scratch2 = regs.takeAny();

    // Shape guard.
    masm.loadPtr(Address(ICStubReg, ICInstanceOf_Function::offsetOfShape()), scratch1);
    masm.branchTestObjShape(Assembler::NotEqual, rhsObj, scratch1, &failureRestoreR1);

    // Guard on the .prototype object.
    masm.loadPtr(Address(rhsObj, NativeObject::offsetOfSlots()), scratch1);
    masm.load32(Address(ICStubReg, ICInstanceOf_Function::offsetOfSlot()), scratch2);
    BaseValueIndex prototypeSlot(scratch1, scratch2);
    masm.branchTestObject(Assembler::NotEqual, prototypeSlot, &failureRestoreR1);
    masm.unboxObject(prototypeSlot, scratch1);
    masm.branchPtr(Assembler::NotEqual,
                   Address(ICStubReg, ICInstanceOf_Function::offsetOfPrototypeObject()),
                   scratch1, &failureRestoreR1);

    // If LHS is a primitive, return false.
    Label returnFalse, returnTrue;
    masm.branchTestObject(Assembler::NotEqual, R0, &returnFalse);

    // LHS is an object. Load its proto.
    masm.unboxObject(R0, scratch2);
    masm.loadObjProto(scratch2, scratch2);

    {
        // Walk the proto chain until we either reach the target object,
        // nullptr or LazyProto.
        Label loop;
        masm.bind(&loop);

        masm.branchPtr(Assembler::Equal, scratch2, scratch1, &returnTrue);
        masm.branchTestPtr(Assembler::Zero, scratch2, scratch2, &returnFalse);

        MOZ_ASSERT(uintptr_t(TaggedProto::LazyProto) == 1);
        masm.branchPtr(Assembler::Equal, scratch2, ImmWord(1), &failureRestoreR1);

        masm.loadObjProto(scratch2, scratch2);
        masm.jump(&loop);
    }

    EmitReturnFromIC(masm);

    masm.bind(&returnFalse);
    masm.moveValue(BooleanValue(false), R0);
    EmitReturnFromIC(masm);

    masm.bind(&returnTrue);
    masm.moveValue(BooleanValue(true), R0);
    EmitReturnFromIC(masm);

    masm.bind(&failureRestoreR1);
    masm.tagValue(JSVAL_TYPE_OBJECT, rhsObj, R1);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

//
// TypeOf_Fallback
//

static bool
DoTypeOfFallback(JSContext* cx, BaselineFrame* frame, ICTypeOf_Fallback* stub, HandleValue val,
                 MutableHandleValue res)
{
    FallbackICSpew(cx, stub, "TypeOf");
    JSType type = js::TypeOfValue(val);
    RootedString string(cx, TypeName(type, cx->names()));

    res.setString(string);

    MOZ_ASSERT(type != JSTYPE_NULL);
    if (type != JSTYPE_OBJECT && type != JSTYPE_FUNCTION) {
        // Create a new TypeOf stub.
        JitSpew(JitSpew_BaselineIC, "  Generating TypeOf stub for JSType (%d)", (int) type);
        ICTypeOf_Typed::Compiler compiler(cx, type, string);
        ICStub* typeOfStub = compiler.getStub(compiler.getStubSpace(frame->script()));
        if (!typeOfStub)
            return false;
        stub->addNewStub(typeOfStub);
    }

    return true;
}

typedef bool (*DoTypeOfFallbackFn)(JSContext*, BaselineFrame* frame, ICTypeOf_Fallback*,
                                   HandleValue, MutableHandleValue);
static const VMFunction DoTypeOfFallbackInfo =
    FunctionInfo<DoTypeOfFallbackFn>(DoTypeOfFallback, TailCall);

bool
ICTypeOf_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    EmitRestoreTailCallReg(masm);

    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushFramePtr(masm, R0.scratchReg());

    return tailCallVM(DoTypeOfFallbackInfo, masm);
}

bool
ICTypeOf_Typed::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);
    MOZ_ASSERT(type_ != JSTYPE_NULL);
    MOZ_ASSERT(type_ != JSTYPE_FUNCTION);
    MOZ_ASSERT(type_ != JSTYPE_OBJECT);

    Label failure;
    switch(type_) {
      case JSTYPE_VOID:
        masm.branchTestUndefined(Assembler::NotEqual, R0, &failure);
        break;

      case JSTYPE_STRING:
        masm.branchTestString(Assembler::NotEqual, R0, &failure);
        break;

      case JSTYPE_NUMBER:
        masm.branchTestNumber(Assembler::NotEqual, R0, &failure);
        break;

      case JSTYPE_BOOLEAN:
        masm.branchTestBoolean(Assembler::NotEqual, R0, &failure);
        break;

      case JSTYPE_SYMBOL:
        masm.branchTestSymbol(Assembler::NotEqual, R0, &failure);
        break;

      default:
        MOZ_CRASH("Unexpected type");
    }

    masm.movePtr(ImmGCPtr(typeString_), R0.scratchReg());
    masm.tagValue(JSVAL_TYPE_STRING, R0.scratchReg(), R0);
    EmitReturnFromIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

static bool
DoRetSubFallback(JSContext* cx, BaselineFrame* frame, ICRetSub_Fallback* stub,
                 HandleValue val, uint8_t** resumeAddr)
{
    FallbackICSpew(cx, stub, "RetSub");

    // |val| is the bytecode offset where we should resume.

    MOZ_ASSERT(val.isInt32());
    MOZ_ASSERT(val.toInt32() >= 0);

    JSScript* script = frame->script();
    uint32_t offset = uint32_t(val.toInt32());

    *resumeAddr = script->baselineScript()->nativeCodeForPC(script, script->offsetToPC(offset));

    if (stub->numOptimizedStubs() >= ICRetSub_Fallback::MAX_OPTIMIZED_STUBS)
        return true;

    // Attach an optimized stub for this pc offset.
    JitSpew(JitSpew_BaselineIC, "  Generating RetSub stub for pc offset %u", offset);
    ICRetSub_Resume::Compiler compiler(cx, offset, *resumeAddr);
    ICStub* optStub = compiler.getStub(compiler.getStubSpace(script));
    if (!optStub)
        return false;

    stub->addNewStub(optStub);
    return true;
}

typedef bool(*DoRetSubFallbackFn)(JSContext* cx, BaselineFrame*, ICRetSub_Fallback*,
                                  HandleValue, uint8_t**);
static const VMFunction DoRetSubFallbackInfo = FunctionInfo<DoRetSubFallbackFn>(DoRetSubFallback);

typedef bool (*ThrowFn)(JSContext*, HandleValue);
static const VMFunction ThrowInfoBaseline = FunctionInfo<ThrowFn>(js::Throw, TailCall);

bool
ICRetSub_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    // If R0 is BooleanValue(true), rethrow R1.
    Label rethrow;
    masm.branchTestBooleanTruthy(true, R0, &rethrow);
    {
        // Call a stub to get the native code address for the pc offset in R1.
        AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));
        regs.take(R1);
        regs.takeUnchecked(ICTailCallReg);
        Register scratch = regs.getAny();

        enterStubFrame(masm, scratch);

        masm.pushValue(R1);
        masm.push(ICStubReg);
        pushFramePtr(masm, scratch);

        if (!callVM(DoRetSubFallbackInfo, masm))
            return false;

        leaveStubFrame(masm);

        EmitChangeICReturnAddress(masm, ReturnReg);
        EmitReturnFromIC(masm);
    }

    masm.bind(&rethrow);
    EmitRestoreTailCallReg(masm);
    masm.pushValue(R1);
    return tailCallVM(ThrowInfoBaseline, masm);
}

bool
ICRetSub_Resume::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    // If R0 is BooleanValue(true), rethrow R1.
    Label fail, rethrow;
    masm.branchTestBooleanTruthy(true, R0, &rethrow);

    // R1 is the pc offset. Ensure it matches this stub's offset.
    Register offset = masm.extractInt32(R1, ExtractTemp0);
    masm.branch32(Assembler::NotEqual,
                  Address(ICStubReg, ICRetSub_Resume::offsetOfPCOffset()),
                  offset,
                  &fail);

    // pc offset matches, resume at the target pc.
    masm.loadPtr(Address(ICStubReg, ICRetSub_Resume::offsetOfAddr()), R0.scratchReg());
    EmitChangeICReturnAddress(masm, R0.scratchReg());
    EmitReturnFromIC(masm);

    // Rethrow the Value stored in R1.
    masm.bind(&rethrow);
    EmitRestoreTailCallReg(masm);
    masm.pushValue(R1);
    if (!tailCallVM(ThrowInfoBaseline, masm))
        return false;

    masm.bind(&fail);
    EmitStubGuardFailure(masm);
    return true;
}

ICTypeMonitor_SingleObject::ICTypeMonitor_SingleObject(JitCode* stubCode, JSObject* obj)
  : ICStub(TypeMonitor_SingleObject, stubCode),
    obj_(obj)
{ }

ICTypeMonitor_ObjectGroup::ICTypeMonitor_ObjectGroup(JitCode* stubCode, ObjectGroup* group)
  : ICStub(TypeMonitor_ObjectGroup, stubCode),
    group_(group)
{ }

ICTypeUpdate_SingleObject::ICTypeUpdate_SingleObject(JitCode* stubCode, JSObject* obj)
  : ICStub(TypeUpdate_SingleObject, stubCode),
    obj_(obj)
{ }

ICTypeUpdate_ObjectGroup::ICTypeUpdate_ObjectGroup(JitCode* stubCode, ObjectGroup* group)
  : ICStub(TypeUpdate_ObjectGroup, stubCode),
    group_(group)
{ }

ICGetElemNativeStub::ICGetElemNativeStub(ICStub::Kind kind, JitCode* stubCode,
                                         ICStub* firstMonitorStub,
                                         ReceiverGuard guard, AccessType acctype,
                                         bool needsAtomize, bool isSymbol)
  : ICMonitoredStub(kind, stubCode, firstMonitorStub),
    receiverGuard_(guard)
{
    extra_ = (static_cast<uint16_t>(acctype) << ACCESSTYPE_SHIFT) |
             (static_cast<uint16_t>(needsAtomize) << NEEDS_ATOMIZE_SHIFT) |
             (static_cast<uint16_t>(isSymbol) << ISSYMBOL_SHIFT);
}

ICGetElemNativeStub::~ICGetElemNativeStub()
{ }

template <class T>
ICGetElemNativeGetterStub<T>::ICGetElemNativeGetterStub(
                           ICStub::Kind kind, JitCode* stubCode, ICStub* firstMonitorStub,
                           ReceiverGuard guard, const T* key, AccType acctype, bool needsAtomize,
                           JSFunction* getter, uint32_t pcOffset)
  : ICGetElemNativeStubImpl<T>(kind, stubCode, firstMonitorStub, guard, key, acctype, needsAtomize),
    getter_(getter),
    pcOffset_(pcOffset)
{
    MOZ_ASSERT(kind == ICStub::GetElem_NativePrototypeCallNativeName ||
               kind == ICStub::GetElem_NativePrototypeCallNativeSymbol ||
               kind == ICStub::GetElem_NativePrototypeCallScriptedName ||
               kind == ICStub::GetElem_NativePrototypeCallScriptedSymbol);
    MOZ_ASSERT(acctype == ICGetElemNativeStub::NativeGetter ||
               acctype == ICGetElemNativeStub::ScriptedGetter);
}

template <class T>
ICGetElem_NativePrototypeSlot<T>::ICGetElem_NativePrototypeSlot(
                               JitCode* stubCode, ICStub* firstMonitorStub, ReceiverGuard guard,
                               const T* key, AccType acctype, bool needsAtomize, uint32_t offset,
                               JSObject* holder, Shape* holderShape)
  : ICGetElemNativeSlotStub<T>(getGetElemStubKind<T>(ICStub::GetElem_NativePrototypeSlotName),
                               stubCode, firstMonitorStub, guard, key, acctype, needsAtomize, offset),
    holder_(holder),
    holderShape_(holderShape)
{ }

template <class T>
ICGetElemNativePrototypeCallStub<T>::ICGetElemNativePrototypeCallStub(
                                  ICStub::Kind kind, JitCode* stubCode, ICStub* firstMonitorStub,
                                  ReceiverGuard guard, const T* key, AccType acctype,
                                  bool needsAtomize, JSFunction* getter, uint32_t pcOffset,
                                  JSObject* holder, Shape* holderShape)
  : ICGetElemNativeGetterStub<T>(kind, stubCode, firstMonitorStub, guard, key, acctype, needsAtomize,
                                 getter, pcOffset),
    holder_(holder),
    holderShape_(holderShape)
{}

template <class T>
/* static */ ICGetElem_NativePrototypeCallNative<T>*
ICGetElem_NativePrototypeCallNative<T>::Clone(JSContext* cx,
                                              ICStubSpace* space,
                                              ICStub* firstMonitorStub,
                                              ICGetElem_NativePrototypeCallNative<T>& other)
{
    return ICStub::New<ICGetElem_NativePrototypeCallNative<T>>(cx, space, other.jitCode(),
                firstMonitorStub, other.receiverGuard(), other.key().unsafeGet(), other.accessType(),
                other.needsAtomize(), other.getter(), other.pcOffset_, other.holder(),
                other.holderShape());
}

template ICGetElem_NativePrototypeCallNative<JS::Symbol*>*
ICGetElem_NativePrototypeCallNative<JS::Symbol*>::Clone(JSContext*, ICStubSpace*, ICStub*,
                                          ICGetElem_NativePrototypeCallNative<JS::Symbol*>&);
template ICGetElem_NativePrototypeCallNative<js::PropertyName*>*
ICGetElem_NativePrototypeCallNative<js::PropertyName*>::Clone(JSContext*, ICStubSpace*, ICStub*,
                                          ICGetElem_NativePrototypeCallNative<js::PropertyName*>&);

template <class T>
/* static */ ICGetElem_NativePrototypeCallScripted<T>*
ICGetElem_NativePrototypeCallScripted<T>::Clone(JSContext* cx,
                                                ICStubSpace* space,
                                                ICStub* firstMonitorStub,
                                                ICGetElem_NativePrototypeCallScripted<T>& other)
{
    return ICStub::New<ICGetElem_NativePrototypeCallScripted<T>>(cx, space, other.jitCode(),
                firstMonitorStub, other.receiverGuard(), other.key().unsafeGet(), other.accessType(),
                other.needsAtomize(), other.getter(), other.pcOffset_, other.holder(),
                other.holderShape());
}

template ICGetElem_NativePrototypeCallScripted<JS::Symbol*>*
ICGetElem_NativePrototypeCallScripted<JS::Symbol*>::Clone(JSContext*, ICStubSpace*, ICStub*,
                                        ICGetElem_NativePrototypeCallScripted<JS::Symbol*>&);
template ICGetElem_NativePrototypeCallScripted<js::PropertyName*>*
ICGetElem_NativePrototypeCallScripted<js::PropertyName*>::Clone(JSContext*, ICStubSpace*, ICStub*,
                                        ICGetElem_NativePrototypeCallScripted<js::PropertyName*>&);

ICGetElem_Dense::ICGetElem_Dense(JitCode* stubCode, ICStub* firstMonitorStub, Shape* shape)
    : ICMonitoredStub(GetElem_Dense, stubCode, firstMonitorStub),
      shape_(shape)
{ }

/* static */ ICGetElem_Dense*
ICGetElem_Dense::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                       ICGetElem_Dense& other)
{
    return New<ICGetElem_Dense>(cx, space, other.jitCode(), firstMonitorStub, other.shape_);
}

ICGetElem_UnboxedArray::ICGetElem_UnboxedArray(JitCode* stubCode, ICStub* firstMonitorStub,
                                               ObjectGroup *group)
  : ICMonitoredStub(GetElem_UnboxedArray, stubCode, firstMonitorStub),
    group_(group)
{ }

/* static */ ICGetElem_UnboxedArray*
ICGetElem_UnboxedArray::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                              ICGetElem_UnboxedArray& other)
{
    return New<ICGetElem_UnboxedArray>(cx, space, other.jitCode(), firstMonitorStub, other.group_);
}

ICGetElem_TypedArray::ICGetElem_TypedArray(JitCode* stubCode, Shape* shape, Scalar::Type type)
  : ICStub(GetElem_TypedArray, stubCode),
    shape_(shape)
{
    extra_ = uint16_t(type);
    MOZ_ASSERT(extra_ == type);
}

/* static */ ICGetElem_Arguments*
ICGetElem_Arguments::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                           ICGetElem_Arguments& other)
{
    return New<ICGetElem_Arguments>(cx, space, other.jitCode(), firstMonitorStub, other.which());
}

ICSetElem_DenseOrUnboxedArray::ICSetElem_DenseOrUnboxedArray(JitCode* stubCode, Shape* shape, ObjectGroup* group)
  : ICUpdatedStub(SetElem_DenseOrUnboxedArray, stubCode),
    shape_(shape),
    group_(group)
{ }

ICSetElem_DenseOrUnboxedArrayAdd::ICSetElem_DenseOrUnboxedArrayAdd(JitCode* stubCode, ObjectGroup* group,
                                                                   size_t protoChainDepth)
  : ICUpdatedStub(SetElem_DenseOrUnboxedArrayAdd, stubCode),
    group_(group)
{
    MOZ_ASSERT(protoChainDepth <= MAX_PROTO_CHAIN_DEPTH);
    extra_ = protoChainDepth;
}

template <size_t ProtoChainDepth>
ICUpdatedStub*
ICSetElemDenseOrUnboxedArrayAddCompiler::getStubSpecific(ICStubSpace* space,
                                                         Handle<ShapeVector> shapes)
{
    RootedObjectGroup group(cx, obj_->getGroup(cx));
    if (!group)
        return nullptr;
    Rooted<JitCode*> stubCode(cx, getStubCode());
    return newStub<ICSetElem_DenseOrUnboxedArrayAddImpl<ProtoChainDepth>>(space, stubCode, group, shapes);
}

ICSetElem_TypedArray::ICSetElem_TypedArray(JitCode* stubCode, Shape* shape, Scalar::Type type,
                                           bool expectOutOfBounds)
  : ICStub(SetElem_TypedArray, stubCode),
    shape_(shape)
{
    extra_ = uint8_t(type);
    MOZ_ASSERT(extra_ == type);
    extra_ |= (static_cast<uint16_t>(expectOutOfBounds) << 8);
}

ICInNativeStub::ICInNativeStub(ICStub::Kind kind, JitCode* stubCode, HandleShape shape,
                               HandlePropertyName name)
  : ICStub(kind, stubCode),
    shape_(shape),
    name_(name)
{ }

ICIn_NativePrototype::ICIn_NativePrototype(JitCode* stubCode, HandleShape shape,
                                           HandlePropertyName name, HandleObject holder,
                                           HandleShape holderShape)
  : ICInNativeStub(In_NativePrototype, stubCode, shape, name),
    holder_(holder),
    holderShape_(holderShape)
{ }

ICIn_NativeDoesNotExist::ICIn_NativeDoesNotExist(JitCode* stubCode, size_t protoChainDepth,
                                                 HandlePropertyName name)
  : ICStub(In_NativeDoesNotExist, stubCode),
    name_(name)
{
    MOZ_ASSERT(protoChainDepth <= MAX_PROTO_CHAIN_DEPTH);
    extra_ = protoChainDepth;
}

/* static */ size_t
ICIn_NativeDoesNotExist::offsetOfShape(size_t idx)
{
    MOZ_ASSERT(ICIn_NativeDoesNotExistImpl<0>::offsetOfShape(idx) ==
               ICIn_NativeDoesNotExistImpl<
                    ICIn_NativeDoesNotExist::MAX_PROTO_CHAIN_DEPTH>::offsetOfShape(idx));
    return ICIn_NativeDoesNotExistImpl<0>::offsetOfShape(idx);
}

template <size_t ProtoChainDepth>
ICIn_NativeDoesNotExistImpl<ProtoChainDepth>::ICIn_NativeDoesNotExistImpl(
        JitCode* stubCode, Handle<ShapeVector> shapes, HandlePropertyName name)
  : ICIn_NativeDoesNotExist(stubCode, ProtoChainDepth, name)
{
    MOZ_ASSERT(shapes.length() == NumShapes);
    for (size_t i = 0; i < NumShapes; i++)
        shapes_[i].init(shapes[i]);
}

ICInNativeDoesNotExistCompiler::ICInNativeDoesNotExistCompiler(
        JSContext* cx, HandleObject obj, HandlePropertyName name, size_t protoChainDepth)
  : ICStubCompiler(cx, ICStub::In_NativeDoesNotExist, Engine::Baseline),
    obj_(cx, obj),
    name_(cx, name),
    protoChainDepth_(protoChainDepth)
{
    MOZ_ASSERT(protoChainDepth_ <= ICIn_NativeDoesNotExist::MAX_PROTO_CHAIN_DEPTH);
}

ICIn_Dense::ICIn_Dense(JitCode* stubCode, HandleShape shape)
  : ICStub(In_Dense, stubCode),
    shape_(shape)
{ }

ICGetName_Global::ICGetName_Global(JitCode* stubCode, ICStub* firstMonitorStub, Shape* shape,
                                   uint32_t slot)
  : ICMonitoredStub(GetName_Global, stubCode, firstMonitorStub),
    shape_(shape),
    slot_(slot)
{ }

template <size_t NumHops>
ICGetName_Scope<NumHops>::ICGetName_Scope(JitCode* stubCode, ICStub* firstMonitorStub,
                                          Handle<ShapeVector> shapes, uint32_t offset)
  : ICMonitoredStub(GetStubKind(), stubCode, firstMonitorStub),
    offset_(offset)
{
    JS_STATIC_ASSERT(NumHops <= MAX_HOPS);
    MOZ_ASSERT(shapes.length() == NumHops + 1);
    for (size_t i = 0; i < NumHops + 1; i++)
        shapes_[i].init(shapes[i]);
}

ICGetIntrinsic_Constant::ICGetIntrinsic_Constant(JitCode* stubCode, const Value& value)
  : ICStub(GetIntrinsic_Constant, stubCode),
    value_(value)
{ }

ICGetIntrinsic_Constant::~ICGetIntrinsic_Constant()
{ }

ICGetProp_Primitive::ICGetProp_Primitive(JitCode* stubCode, ICStub* firstMonitorStub,
                                         JSValueType primitiveType, Shape* protoShape,
                                         uint32_t offset)
  : ICMonitoredStub(GetProp_Primitive, stubCode, firstMonitorStub),
    protoShape_(protoShape),
    offset_(offset)
{
    extra_ = uint16_t(primitiveType);
    MOZ_ASSERT(JSValueType(extra_) == primitiveType);
}

ICGetPropNativeStub::ICGetPropNativeStub(ICStub::Kind kind, JitCode* stubCode,
                                         ICStub* firstMonitorStub,
                                         ReceiverGuard guard, uint32_t offset)
  : ICMonitoredStub(kind, stubCode, firstMonitorStub),
    receiverGuard_(guard),
    offset_(offset)
{ }

/* static */ ICGetProp_Native*
ICGetProp_Native::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                        ICGetProp_Native& other)
{
    return New<ICGetProp_Native>(cx, space, other.jitCode(), firstMonitorStub,
                                 other.receiverGuard(), other.offset());
}

ICGetProp_NativePrototype::ICGetProp_NativePrototype(JitCode* stubCode, ICStub* firstMonitorStub,
                                                     ReceiverGuard guard, uint32_t offset,
                                                     JSObject* holder, Shape* holderShape)
  : ICGetPropNativeStub(GetProp_NativePrototype, stubCode, firstMonitorStub, guard, offset),
    holder_(holder),
    holderShape_(holderShape)
{ }

/* static */ ICGetProp_NativePrototype*
ICGetProp_NativePrototype::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                                 ICGetProp_NativePrototype& other)
{
    return New<ICGetProp_NativePrototype>(cx, space, other.jitCode(), firstMonitorStub,
                                          other.receiverGuard(), other.offset(),
                                          other.holder_, other.holderShape_);
}

ICGetProp_NativeDoesNotExist::ICGetProp_NativeDoesNotExist(
    JitCode* stubCode, ICStub* firstMonitorStub, ReceiverGuard guard,
    size_t protoChainDepth)
  : ICMonitoredStub(GetProp_NativeDoesNotExist, stubCode, firstMonitorStub),
    guard_(guard)
{
    MOZ_ASSERT(protoChainDepth <= MAX_PROTO_CHAIN_DEPTH);
    extra_ = protoChainDepth;
}

/* static */ size_t
ICGetProp_NativeDoesNotExist::offsetOfShape(size_t idx)
{
    MOZ_ASSERT(ICGetProp_NativeDoesNotExistImpl<0>::offsetOfShape(idx) ==
               ICGetProp_NativeDoesNotExistImpl<
                    ICGetProp_NativeDoesNotExist::MAX_PROTO_CHAIN_DEPTH>::offsetOfShape(idx));
    return ICGetProp_NativeDoesNotExistImpl<0>::offsetOfShape(idx);
}

template <size_t ProtoChainDepth>
ICGetProp_NativeDoesNotExistImpl<ProtoChainDepth>::ICGetProp_NativeDoesNotExistImpl(
        JitCode* stubCode, ICStub* firstMonitorStub, ReceiverGuard guard,
        Handle<ShapeVector> shapes)
  : ICGetProp_NativeDoesNotExist(stubCode, firstMonitorStub, guard, ProtoChainDepth)
{
    MOZ_ASSERT(shapes.length() == NumShapes);

    // Note: using int32_t here to avoid gcc warning.
    for (int32_t i = 0; i < int32_t(NumShapes); i++)
        shapes_[i].init(shapes[i]);
}

ICGetPropNativeDoesNotExistCompiler::ICGetPropNativeDoesNotExistCompiler(
        JSContext* cx, ICStub* firstMonitorStub, HandleObject obj, size_t protoChainDepth)
  : ICStubCompiler(cx, ICStub::GetProp_NativeDoesNotExist, Engine::Baseline),
    firstMonitorStub_(firstMonitorStub),
    obj_(cx, obj),
    protoChainDepth_(protoChainDepth)
{
    MOZ_ASSERT(protoChainDepth_ <= ICGetProp_NativeDoesNotExist::MAX_PROTO_CHAIN_DEPTH);
}

ICGetPropCallGetter::ICGetPropCallGetter(Kind kind, JitCode* stubCode, ICStub* firstMonitorStub,
                                         ReceiverGuard receiverGuard, JSObject* holder,
                                         Shape* holderShape, JSFunction* getter,
                                         uint32_t pcOffset)
  : ICMonitoredStub(kind, stubCode, firstMonitorStub),
    receiverGuard_(receiverGuard),
    holder_(holder),
    holderShape_(holderShape),
    getter_(getter),
    pcOffset_(pcOffset)
{
    MOZ_ASSERT(kind == ICStub::GetProp_CallScripted  ||
               kind == ICStub::GetProp_CallNative    ||
               kind == ICStub::GetProp_CallDOMProxyNative ||
               kind == ICStub::GetProp_CallDOMProxyWithGenerationNative);
}

ICInstanceOf_Function::ICInstanceOf_Function(JitCode* stubCode, Shape* shape,
                                             JSObject* prototypeObj, uint32_t slot)
  : ICStub(InstanceOf_Function, stubCode),
    shape_(shape),
    prototypeObj_(prototypeObj),
    slot_(slot)
{ }

/* static */ ICGetProp_CallScripted*
ICGetProp_CallScripted::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                              ICGetProp_CallScripted& other)
{
    return New<ICGetProp_CallScripted>(cx, space, other.jitCode(), firstMonitorStub,
                                       other.receiverGuard(),
                                       other.holder_, other.holderShape_,
                                       other.getter_, other.pcOffset_);
}

/* static */ ICGetProp_CallNative*
ICGetProp_CallNative::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                            ICGetProp_CallNative& other)
{
    return New<ICGetProp_CallNative>(cx, space, other.jitCode(), firstMonitorStub,
                                     other.receiverGuard(), other.holder_,
                                     other.holderShape_, other.getter_, other.pcOffset_);
}

ICSetProp_Native::ICSetProp_Native(JitCode* stubCode, ObjectGroup* group, Shape* shape,
                                   uint32_t offset)
  : ICUpdatedStub(SetProp_Native, stubCode),
    group_(group),
    shape_(shape),
    offset_(offset)
{ }

ICSetProp_Native*
ICSetProp_Native::Compiler::getStub(ICStubSpace* space)
{
    RootedObjectGroup group(cx, obj_->getGroup(cx));
    if (!group)
        return nullptr;

    RootedShape shape(cx, LastPropertyForSetProp(obj_));
    ICSetProp_Native* stub = newStub<ICSetProp_Native>(space, getStubCode(), group, shape, offset_);
    if (!stub || !stub->initUpdatingChain(cx, space))
        return nullptr;
    return stub;
}

ICSetProp_NativeAdd::ICSetProp_NativeAdd(JitCode* stubCode, ObjectGroup* group,
                                         size_t protoChainDepth,
                                         Shape* newShape,
                                         ObjectGroup* newGroup,
                                         uint32_t offset)
  : ICUpdatedStub(SetProp_NativeAdd, stubCode),
    group_(group),
    newShape_(newShape),
    newGroup_(newGroup),
    offset_(offset)
{
    MOZ_ASSERT(protoChainDepth <= MAX_PROTO_CHAIN_DEPTH);
    extra_ = protoChainDepth;
}

template <size_t ProtoChainDepth>
ICSetProp_NativeAddImpl<ProtoChainDepth>::ICSetProp_NativeAddImpl(JitCode* stubCode,
                                                                  ObjectGroup* group,
                                                                  Handle<ShapeVector> shapes,
                                                                  Shape* newShape,
                                                                  ObjectGroup* newGroup,
                                                                  uint32_t offset)
  : ICSetProp_NativeAdd(stubCode, group, ProtoChainDepth, newShape, newGroup, offset)
{
    MOZ_ASSERT(shapes.length() == NumShapes);
    for (size_t i = 0; i < NumShapes; i++)
        shapes_[i].init(shapes[i]);
}

ICSetPropNativeAddCompiler::ICSetPropNativeAddCompiler(JSContext* cx, HandleObject obj,
                                                       HandleShape oldShape,
                                                       HandleObjectGroup oldGroup,
                                                       size_t protoChainDepth,
                                                       bool isFixedSlot,
                                                       uint32_t offset)
  : ICStubCompiler(cx, ICStub::SetProp_NativeAdd, Engine::Baseline),
    obj_(cx, obj),
    oldShape_(cx, oldShape),
    oldGroup_(cx, oldGroup),
    protoChainDepth_(protoChainDepth),
    isFixedSlot_(isFixedSlot),
    offset_(offset)
{
    MOZ_ASSERT(protoChainDepth_ <= ICSetProp_NativeAdd::MAX_PROTO_CHAIN_DEPTH);
}

ICSetPropCallSetter::ICSetPropCallSetter(Kind kind, JitCode* stubCode, ReceiverGuard receiverGuard,
                                         JSObject* holder, Shape* holderShape,
                                         JSFunction* setter, uint32_t pcOffset)
  : ICStub(kind, stubCode),
    receiverGuard_(receiverGuard),
    holder_(holder),
    holderShape_(holderShape),
    setter_(setter),
    pcOffset_(pcOffset)
{
    MOZ_ASSERT(kind == ICStub::SetProp_CallScripted || kind == ICStub::SetProp_CallNative);
}

/* static */ ICSetProp_CallScripted*
ICSetProp_CallScripted::Clone(JSContext* cx, ICStubSpace* space, ICStub*,
                              ICSetProp_CallScripted& other)
{
    return New<ICSetProp_CallScripted>(cx, space, other.jitCode(), other.receiverGuard(),
                                       other.holder_, other.holderShape_, other.setter_,
                                       other.pcOffset_);
}

/* static */ ICSetProp_CallNative*
ICSetProp_CallNative::Clone(JSContext* cx, ICStubSpace* space, ICStub*, ICSetProp_CallNative& other)
{
    return New<ICSetProp_CallNative>(cx, space, other.jitCode(), other.receiverGuard(),
                                     other.holder_, other.holderShape_, other.setter_,
                                     other.pcOffset_);
}

ICCall_Scripted::ICCall_Scripted(JitCode* stubCode, ICStub* firstMonitorStub,
                                 JSFunction* callee, JSObject* templateObject,
                                 uint32_t pcOffset)
  : ICMonitoredStub(ICStub::Call_Scripted, stubCode, firstMonitorStub),
    callee_(callee),
    templateObject_(templateObject),
    pcOffset_(pcOffset)
{ }

/* static */ ICCall_Scripted*
ICCall_Scripted::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                       ICCall_Scripted& other)
{
    return New<ICCall_Scripted>(cx, space, other.jitCode(), firstMonitorStub, other.callee_,
                                other.templateObject_, other.pcOffset_);
}

/* static */ ICCall_AnyScripted*
ICCall_AnyScripted::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                          ICCall_AnyScripted& other)
{
    return New<ICCall_AnyScripted>(cx, space, other.jitCode(), firstMonitorStub, other.pcOffset_);
}

ICCall_Native::ICCall_Native(JitCode* stubCode, ICStub* firstMonitorStub,
                             JSFunction* callee, JSObject* templateObject,
                             uint32_t pcOffset)
  : ICMonitoredStub(ICStub::Call_Native, stubCode, firstMonitorStub),
    callee_(callee),
    templateObject_(templateObject),
    pcOffset_(pcOffset)
{
#ifdef JS_SIMULATOR
    // The simulator requires VM calls to be redirected to a special swi
    // instruction to handle them. To make this work, we store the redirected
    // pointer in the stub.
    native_ = Simulator::RedirectNativeFunction(JS_FUNC_TO_DATA_PTR(void*, callee->native()),
                                                Args_General3);
#endif
}

/* static */ ICCall_Native*
ICCall_Native::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                     ICCall_Native& other)
{
    return New<ICCall_Native>(cx, space, other.jitCode(), firstMonitorStub, other.callee_,
                              other.templateObject_, other.pcOffset_);
}

ICCall_ClassHook::ICCall_ClassHook(JitCode* stubCode, ICStub* firstMonitorStub,
                                   const Class* clasp, Native native,
                                   JSObject* templateObject, uint32_t pcOffset)
  : ICMonitoredStub(ICStub::Call_ClassHook, stubCode, firstMonitorStub),
    clasp_(clasp),
    native_(JS_FUNC_TO_DATA_PTR(void*, native)),
    templateObject_(templateObject),
    pcOffset_(pcOffset)
{
#ifdef JS_SIMULATOR
    // The simulator requires VM calls to be redirected to a special swi
    // instruction to handle them. To make this work, we store the redirected
    // pointer in the stub.
    native_ = Simulator::RedirectNativeFunction(native_, Args_General3);
#endif
}

/* static */ ICCall_ClassHook*
ICCall_ClassHook::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                        ICCall_ClassHook& other)
{
    ICCall_ClassHook* res = New<ICCall_ClassHook>(cx, space, other.jitCode(), firstMonitorStub,
                                                  other.clasp(), nullptr, other.templateObject_,
                                                  other.pcOffset_);
    if (res)
        res->native_ = other.native();
    return res;
}

/* static */ ICCall_ScriptedApplyArray*
ICCall_ScriptedApplyArray::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                                 ICCall_ScriptedApplyArray& other)
{
    return New<ICCall_ScriptedApplyArray>(cx, space, other.jitCode(), firstMonitorStub,
                                          other.pcOffset_);
}

/* static */ ICCall_ScriptedApplyArguments*
ICCall_ScriptedApplyArguments::Clone(JSContext* cx,
                                     ICStubSpace* space,
                                     ICStub* firstMonitorStub,
                                     ICCall_ScriptedApplyArguments& other)
{
    return New<ICCall_ScriptedApplyArguments>(cx, space, other.jitCode(), firstMonitorStub,
                                              other.pcOffset_);
}

/* static */ ICCall_ScriptedFunCall*
ICCall_ScriptedFunCall::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                              ICCall_ScriptedFunCall& other)
{
    return New<ICCall_ScriptedFunCall>(cx, space, other.jitCode(), firstMonitorStub,
                                       other.pcOffset_);
}

ICGetPropCallDOMProxyNativeStub::ICGetPropCallDOMProxyNativeStub(Kind kind, JitCode* stubCode,
                                                                 ICStub* firstMonitorStub,
                                                                 Shape* shape,
                                                                 Shape* expandoShape,
                                                                 JSObject* holder,
                                                                 Shape* holderShape,
                                                                 JSFunction* getter,
                                                                 uint32_t pcOffset)
  : ICGetPropCallGetter(kind, stubCode, firstMonitorStub, ReceiverGuard(nullptr, shape),
                        holder, holderShape, getter, pcOffset),
    expandoShape_(expandoShape)
{ }

ICGetPropCallDOMProxyNativeCompiler::ICGetPropCallDOMProxyNativeCompiler(JSContext* cx,
                                                                         ICStub::Kind kind,
                                                                         ICStub* firstMonitorStub,
                                                                         Handle<ProxyObject*> proxy,
                                                                         HandleObject holder,
                                                                         HandleFunction getter,
                                                                         uint32_t pcOffset)
  : ICStubCompiler(cx, kind, Engine::Baseline),
    firstMonitorStub_(firstMonitorStub),
    proxy_(cx, proxy),
    holder_(cx, holder),
    getter_(cx, getter),
    pcOffset_(pcOffset)
{
    MOZ_ASSERT(kind == ICStub::GetProp_CallDOMProxyNative ||
               kind == ICStub::GetProp_CallDOMProxyWithGenerationNative);
    MOZ_ASSERT(proxy_->handler()->family() == GetDOMProxyHandlerFamily());
}

/* static */ ICGetProp_CallDOMProxyNative*
ICGetProp_CallDOMProxyNative::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                                    ICGetProp_CallDOMProxyNative& other)
{
    return New<ICGetProp_CallDOMProxyNative>(cx, space, other.jitCode(), firstMonitorStub,
                                             other.receiverGuard_.shape(), other.expandoShape_,
                                             other.holder_, other.holderShape_, other.getter_,
                                             other.pcOffset_);
}

/* static */ ICGetProp_CallDOMProxyWithGenerationNative*
ICGetProp_CallDOMProxyWithGenerationNative::Clone(JSContext* cx,
                                                  ICStubSpace* space,
                                                  ICStub* firstMonitorStub,
                                                  ICGetProp_CallDOMProxyWithGenerationNative& other)
{
    return New<ICGetProp_CallDOMProxyWithGenerationNative>(cx, space, other.jitCode(),
                                                           firstMonitorStub,
                                                           other.receiverGuard_.shape(),
                                                           other.expandoAndGeneration_,
                                                           other.generation_,
                                                           other.expandoShape_, other.holder_,
                                                           other.holderShape_, other.getter_,
                                                           other.pcOffset_);
}

ICGetProp_DOMProxyShadowed::ICGetProp_DOMProxyShadowed(JitCode* stubCode,
                                                       ICStub* firstMonitorStub,
                                                       Shape* shape,
                                                       const BaseProxyHandler* proxyHandler,
                                                       PropertyName* name,
                                                       uint32_t pcOffset)
  : ICMonitoredStub(ICStub::GetProp_DOMProxyShadowed, stubCode, firstMonitorStub),
    shape_(shape),
    proxyHandler_(proxyHandler),
    name_(name),
    pcOffset_(pcOffset)
{ }

/* static */ ICGetProp_DOMProxyShadowed*
ICGetProp_DOMProxyShadowed::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                                  ICGetProp_DOMProxyShadowed& other)
{
    return New<ICGetProp_DOMProxyShadowed>(cx, space, other.jitCode(), firstMonitorStub,
                                           other.shape_, other.proxyHandler_, other.name_,
                                           other.pcOffset_);
}

//
// Rest_Fallback
//

static bool DoRestFallback(JSContext* cx, BaselineFrame* frame, ICRest_Fallback* stub,
                           MutableHandleValue res)
{
    unsigned numFormals = frame->numFormalArgs() - 1;
    unsigned numActuals = frame->numActualArgs();
    unsigned numRest = numActuals > numFormals ? numActuals - numFormals : 0;
    Value* rest = frame->argv() + numFormals;

    JSObject* obj = ObjectGroup::newArrayObject(cx, rest, numRest, GenericObject,
                                                ObjectGroup::NewArrayKind::UnknownIndex);
    if (!obj)
        return false;
    res.setObject(*obj);
    return true;
}

typedef bool (*DoRestFallbackFn)(JSContext*, BaselineFrame*, ICRest_Fallback*,
                                 MutableHandleValue);
static const VMFunction DoRestFallbackInfo =
    FunctionInfo<DoRestFallbackFn>(DoRestFallback, TailCall);

bool
ICRest_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);

    EmitRestoreTailCallReg(masm);

    masm.push(ICStubReg);
    pushFramePtr(masm, R0.scratchReg());

    return tailCallVM(DoRestFallbackInfo, masm);
}

} // namespace jit
} // namespace js
