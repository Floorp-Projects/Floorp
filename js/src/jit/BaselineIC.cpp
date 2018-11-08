/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineIC.h"

#include "mozilla/Casting.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Sprintf.h"
#include "mozilla/TemplateLib.h"

#include "jsfriendapi.h"
#include "jslibmath.h"
#include "jstypes.h"

#include "builtin/Eval.h"
#include "gc/Policy.h"
#include "jit/BaselineCacheIRCompiler.h"
#include "jit/BaselineDebugModeOSR.h"
#include "jit/BaselineJIT.h"
#include "jit/InlinableNatives.h"
#include "jit/JitSpewer.h"
#include "jit/Linker.h"
#include "jit/Lowering.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "jit/SharedICHelpers.h"
#include "jit/VMFunctions.h"
#include "js/Conversions.h"
#include "js/GCVector.h"
#include "vm/JSFunction.h"
#include "vm/Opcodes.h"
#include "vm/SelfHosting.h"
#include "vm/TypedArrayObject.h"

#include "builtin/Boolean-inl.h"

#include "jit/JitFrames-inl.h"
#include "jit/MacroAssembler-inl.h"
#include "jit/shared/Lowering-shared-inl.h"
#include "jit/SharedICHelpers-inl.h"
#include "vm/EnvironmentObject-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/StringObject-inl.h"
#include "vm/UnboxedObject-inl.h"

using mozilla::DebugOnly;

namespace js {
namespace jit {


#ifdef JS_JITSPEW
void
FallbackICSpew(JSContext* cx, ICFallbackStub* stub, const char* fmt, ...)
{
    if (JitSpewEnabled(JitSpew_BaselineICFallback)) {
        RootedScript script(cx, GetTopJitJSScript(cx));
        jsbytecode* pc = stub->icEntry()->pc(script);

        char fmtbuf[100];
        va_list args;
        va_start(args, fmt);
        (void) VsprintfLiteral(fmtbuf, fmt, args);
        va_end(args);

        JitSpew(JitSpew_BaselineICFallback,
                "Fallback hit for (%s:%u:%u) (pc=%zu,line=%d,uses=%d,stubs=%zu): %s",
                script->filename(),
                script->lineno(),
                script->column(),
                script->pcToOffset(pc),
                PCToLineNumber(script, pc),
                script->getWarmUpCount(),
                stub->numOptimizedStubs(),
                fmtbuf);
    }
}

void
TypeFallbackICSpew(JSContext* cx, ICTypeMonitor_Fallback* stub, const char* fmt, ...)
{
    if (JitSpewEnabled(JitSpew_BaselineICFallback)) {
        RootedScript script(cx, GetTopJitJSScript(cx));
        jsbytecode* pc = stub->icEntry()->pc(script);

        char fmtbuf[100];
        va_list args;
        va_start(args, fmt);
        (void) VsprintfLiteral(fmtbuf, fmt, args);
        va_end(args);

        JitSpew(JitSpew_BaselineICFallback,
                "Type monitor fallback hit for (%s:%u:%u) (pc=%zu,line=%d,uses=%d,stubs=%d): %s",
                script->filename(),
                script->lineno(),
                script->column(),
                script->pcToOffset(pc),
                PCToLineNumber(script, pc),
                script->getWarmUpCount(),
                (int) stub->numOptimizedMonitorStubs(),
                fmtbuf);
    }
}
#endif // JS_JITSPEW

ICFallbackStub*
ICEntry::fallbackStub() const
{
    return firstStub()->getChainFallback();
}

void
ICEntry::trace(JSTracer* trc)
{
    for (ICStub* stub = firstStub(); stub; stub = stub->next()) {
        stub->trace(trc);
    }
}

ICStubConstIterator&
ICStubConstIterator::operator++()
{
    MOZ_ASSERT(currentStub_ != nullptr);
    currentStub_ = currentStub_->next();
    return *this;
}


ICStubIterator::ICStubIterator(ICFallbackStub* fallbackStub, bool end)
  : icEntry_(fallbackStub->icEntry()),
    fallbackStub_(fallbackStub),
    previousStub_(nullptr),
    currentStub_(end ? fallbackStub : icEntry_->firstStub()),
    unlinked_(false)
{ }

ICStubIterator&
ICStubIterator::operator++()
{
    MOZ_ASSERT(currentStub_->next() != nullptr);
    if (!unlinked_) {
        previousStub_ = currentStub_;
    }
    currentStub_ = currentStub_->next();
    unlinked_ = false;
    return *this;
}

void
ICStubIterator::unlink(JSContext* cx)
{
    MOZ_ASSERT(currentStub_->next() != nullptr);
    MOZ_ASSERT(currentStub_ != fallbackStub_);
    MOZ_ASSERT(!unlinked_);

    fallbackStub_->unlinkStub(cx->zone(), previousStub_, currentStub_);

    // Mark the current iterator position as unlinked, so operator++ works properly.
    unlinked_ = true;
}

/* static */ bool
ICStub::NonCacheIRStubMakesGCCalls(Kind kind)
{
    MOZ_ASSERT(IsValidKind(kind));
    MOZ_ASSERT(!IsCacheIRKind(kind));

    switch (kind) {
      case Call_Fallback:
      case Call_Scripted:
      case Call_AnyScripted:
      case Call_Native:
      case Call_ClassHook:
      case Call_ScriptedApplyArray:
      case Call_ScriptedApplyArguments:
      case Call_ScriptedFunCall:
      case Call_ConstStringSplit:
      case WarmUpCounter_Fallback:
      // These two fallback stubs don't actually make non-tail calls,
      // but the fallback code for the bailout path needs to pop the stub frame
      // pushed during the bailout.
      case GetProp_Fallback:
      case SetProp_Fallback:
        return true;
      default:
        return false;
    }
}

bool
ICStub::makesGCCalls() const
{
    switch (kind()) {
      case CacheIR_Regular:
        return toCacheIR_Regular()->stubInfo()->makesGCCalls();
      case CacheIR_Monitored:
        return toCacheIR_Monitored()->stubInfo()->makesGCCalls();
      case CacheIR_Updated:
        return toCacheIR_Updated()->stubInfo()->makesGCCalls();
      default:
        return NonCacheIRStubMakesGCCalls(kind());
    }
}

void
ICStub::traceCode(JSTracer* trc, const char* name)
{
    JitCode* stubJitCode = jitCode();
    TraceManuallyBarrieredEdge(trc, &stubJitCode, name);
}

void
ICStub::updateCode(JitCode* code)
{
    // Write barrier on the old code.
    JitCode::writeBarrierPre(jitCode());
    stubCode_ = code->raw();
}

/* static */ void
ICStub::trace(JSTracer* trc)
{
    traceCode(trc, "shared-stub-jitcode");

    // If the stub is a monitored fallback stub, then trace the monitor ICs hanging
    // off of that stub.  We don't need to worry about the regular monitored stubs,
    // because the regular monitored stubs will always have a monitored fallback stub
    // that references the same stub chain.
    if (isMonitoredFallback()) {
        ICTypeMonitor_Fallback* lastMonStub =
            toMonitoredFallbackStub()->maybeFallbackMonitorStub();
        if (lastMonStub) {
            for (ICStubConstIterator iter(lastMonStub->firstMonitorStub());
                 !iter.atEnd();
                 iter++)
            {
                MOZ_ASSERT_IF(iter->next() == nullptr, *iter == lastMonStub);
                iter->trace(trc);
            }
        }
    }

    if (isUpdated()) {
        for (ICStubConstIterator iter(toUpdatedStub()->firstUpdateStub()); !iter.atEnd(); iter++) {
            MOZ_ASSERT_IF(iter->next() == nullptr, iter->isTypeUpdate_Fallback());
            iter->trace(trc);
        }
    }

    switch (kind()) {
      case ICStub::Call_Scripted: {
        ICCall_Scripted* callStub = toCall_Scripted();
        TraceEdge(trc, &callStub->callee(), "baseline-callscripted-callee");
        TraceNullableEdge(trc, &callStub->templateObject(), "baseline-callscripted-template");
        break;
      }
      case ICStub::Call_Native: {
        ICCall_Native* callStub = toCall_Native();
        TraceEdge(trc, &callStub->callee(), "baseline-callnative-callee");
        TraceNullableEdge(trc, &callStub->templateObject(), "baseline-callnative-template");
        break;
      }
      case ICStub::Call_ClassHook: {
        ICCall_ClassHook* callStub = toCall_ClassHook();
        TraceNullableEdge(trc, &callStub->templateObject(), "baseline-callclasshook-template");
        break;
      }
      case ICStub::Call_ConstStringSplit: {
        ICCall_ConstStringSplit* callStub = toCall_ConstStringSplit();
        TraceEdge(trc, &callStub->templateObject(), "baseline-callstringsplit-template");
        TraceEdge(trc, &callStub->expectedSep(), "baseline-callstringsplit-sep");
        TraceEdge(trc, &callStub->expectedStr(), "baseline-callstringsplit-str");
        break;
      }
      case ICStub::TypeMonitor_SingleObject: {
        ICTypeMonitor_SingleObject* monitorStub = toTypeMonitor_SingleObject();
        TraceEdge(trc, &monitorStub->object(), "baseline-monitor-singleton");
        break;
      }
      case ICStub::TypeMonitor_ObjectGroup: {
        ICTypeMonitor_ObjectGroup* monitorStub = toTypeMonitor_ObjectGroup();
        TraceEdge(trc, &monitorStub->group(), "baseline-monitor-group");
        break;
      }
      case ICStub::TypeUpdate_SingleObject: {
        ICTypeUpdate_SingleObject* updateStub = toTypeUpdate_SingleObject();
        TraceEdge(trc, &updateStub->object(), "baseline-update-singleton");
        break;
      }
      case ICStub::TypeUpdate_ObjectGroup: {
        ICTypeUpdate_ObjectGroup* updateStub = toTypeUpdate_ObjectGroup();
        TraceEdge(trc, &updateStub->group(), "baseline-update-group");
        break;
      }
      case ICStub::NewArray_Fallback: {
        ICNewArray_Fallback* stub = toNewArray_Fallback();
        TraceNullableEdge(trc, &stub->templateObject(), "baseline-newarray-template");
        TraceEdge(trc, &stub->templateGroup(), "baseline-newarray-template-group");
        break;
      }
      case ICStub::NewObject_Fallback: {
        ICNewObject_Fallback* stub = toNewObject_Fallback();
        TraceNullableEdge(trc, &stub->templateObject(), "baseline-newobject-template");
        break;
      }
      case ICStub::Rest_Fallback: {
        ICRest_Fallback* stub = toRest_Fallback();
        TraceEdge(trc, &stub->templateObject(), "baseline-rest-template");
        break;
      }
      case ICStub::CacheIR_Regular:
        TraceCacheIRStub(trc, this, toCacheIR_Regular()->stubInfo());
        break;
      case ICStub::CacheIR_Monitored:
        TraceCacheIRStub(trc, this, toCacheIR_Monitored()->stubInfo());
        break;
      case ICStub::CacheIR_Updated: {
        ICCacheIR_Updated* stub = toCacheIR_Updated();
        TraceNullableEdge(trc, &stub->updateStubGroup(), "baseline-update-stub-group");
        TraceEdge(trc, &stub->updateStubId(), "baseline-update-stub-id");
        TraceCacheIRStub(trc, this, stub->stubInfo());
        break;
      }
      default:
        break;
    }
}

// This helper handles ICState updates/transitions while attaching CacheIR stubs.
template<typename IRGenerator, typename... Args>
static void
TryAttachStub(const char *name, JSContext* cx, BaselineFrame* frame, ICFallbackStub* stub, BaselineCacheIRStubKind kind, Args&&... args)
{
    if (stub->state().maybeTransition()) {
        stub->discardStubs(cx);
    }

    if (stub->state().canAttachStub()) {
        RootedScript script(cx, frame->script());
        jsbytecode* pc = stub->icEntry()->pc(script);

        bool attached = false;
        IRGenerator gen(cx, script, pc, stub->state().mode(), std::forward<Args>(args)...);
        if (gen.tryAttachStub()) {
            ICStub* newStub = AttachBaselineCacheIRStub(cx, gen.writerRef(), gen.cacheKind(),
                                                        kind, script, stub, &attached);
            if (newStub) {
                JitSpew(JitSpew_BaselineIC, "  %s %s CacheIR stub", attached ? "Attached" : "Failed to attach", name);
            }
        }
        if (!attached) {
            stub->state().trackNotAttached();
        }
    }
}


//
// WarmUpCounter_Fallback
//


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
PrepareOsrTempData(JSContext* cx, BaselineFrame* frame, void* jitcode)
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

    IonOsrTempData* info = (IonOsrTempData*)cx->allocateOsrTempData(totalSpace);
    if (!info) {
        ReportOutOfMemory(cx);
        return nullptr;
    }

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
DoWarmUpCounterFallbackOSR(JSContext* cx, BaselineFrame* frame, ICWarmUpCounter_Fallback* stub,
                           IonOsrTempData** infoPtr)
{
    MOZ_ASSERT(infoPtr);
    *infoPtr = nullptr;

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    MOZ_ASSERT(JSOp(*pc) == JSOP_LOOPENTRY);

    FallbackICSpew(cx, stub, "WarmUpCounter(%d)", int(script->pcToOffset(pc)));

    if (!IonCompileScriptForBaseline(cx, frame, pc)) {
        return false;
    }

    if (!script->hasIonScript() || script->ionScript()->osrPc() != pc ||
        script->ionScript()->bailoutExpected() ||
        frame->isDebuggee())
    {
        return true;
    }

    IonScript* ion = script->ionScript();
    MOZ_ASSERT(cx->runtime()->geckoProfiler().enabled() == ion->hasProfilingInstrumentation());
    MOZ_ASSERT(ion->osrPc() == pc);

    JitSpew(JitSpew_BaselineOSR, "  OSR possible!");
    void* jitcode = ion->method()->raw() + ion->osrEntryOffset();

    // Prepare the temporary heap copy of the fake InterpreterFrame and actual args list.
    JitSpew(JitSpew_BaselineOSR, "Got jitcode.  Preparing for OSR into ion.");
    IonOsrTempData* info = PrepareOsrTempData(cx, frame, jitcode);
    if (!info) {
        return false;
    }
    *infoPtr = info;

    return true;
}

typedef bool (*DoWarmUpCounterFallbackOSRFn)(JSContext*, BaselineFrame*,
                                             ICWarmUpCounter_Fallback*, IonOsrTempData** infoPtr);
static const VMFunction DoWarmUpCounterFallbackOSRInfo =
    FunctionInfo<DoWarmUpCounterFallbackOSRFn>(DoWarmUpCounterFallbackOSR,
                                               "DoWarmUpCounterFallbackOSR");

bool
ICWarmUpCounter_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    // Push a stub frame so that we can perform a non-tail call.
    enterStubFrame(masm, R1.scratchReg());

    Label noCompiledCode;
    // Call DoWarmUpCounterFallbackOSR to compile/check-for Ion-compiled function
    {
        // Push IonOsrTempData pointer storage
        masm.subFromStackPtr(Imm32(sizeof(void*)));
        masm.push(masm.getStackPointer());

        // Push stub pointer.
        masm.push(ICStubReg);

        pushStubPayload(masm, R0.scratchReg());

        if (!callVM(DoWarmUpCounterFallbackOSRInfo, masm)) {
            return false;
        }

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
        AbsoluteAddress addressOfEnabled(cx->runtime()->geckoProfiler().addressOfEnabled());
        masm.branch32(Assembler::Equal, addressOfEnabled, Imm32(0), &checkOk);
        masm.loadPtr(AbsoluteAddress((void*)&cx->jitActivation), scratchReg);
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


void
ICFallbackStub::unlinkStub(Zone* zone, ICStub* prev, ICStub* stub)
{
    MOZ_ASSERT(stub->next());

    // If stub is the last optimized stub, update lastStubPtrAddr.
    if (stub->next() == this) {
        MOZ_ASSERT(lastStubPtrAddr_ == stub->addressOfNext());
        if (prev) {
            lastStubPtrAddr_ = prev->addressOfNext();
        } else {
            lastStubPtrAddr_ = icEntry()->addressOfFirstStub();
        }
        *lastStubPtrAddr_ = this;
    } else {
        if (prev) {
            MOZ_ASSERT(prev->next() == stub);
            prev->setNext(stub->next());
        } else {
            MOZ_ASSERT(icEntry()->firstStub() == stub);
            icEntry()->setFirstStub(stub->next());
        }
    }

    state_.trackUnlinkedStub();

    if (zone->needsIncrementalBarrier()) {
        // We are removing edges from ICStub to gcthings. Perform one final trace
        // of the stub for incremental GC, as it must know about those edges.
        stub->trace(zone->barrierTracer());
    }

    if (stub->makesGCCalls() && stub->isMonitored()) {
        // This stub can make calls so we can return to it if it's on the stack.
        // We just have to reset its firstMonitorStub_ field to avoid a stale
        // pointer when purgeOptimizedStubs destroys all optimized monitor
        // stubs (unlinked stubs won't be updated).
        ICTypeMonitor_Fallback* monitorFallback =
            toMonitoredFallbackStub()->maybeFallbackMonitorStub();
        MOZ_ASSERT(monitorFallback);
        stub->toMonitoredStub()->resetFirstMonitorStub(monitorFallback);
    }

#ifdef DEBUG
    // Poison stub code to ensure we don't call this stub again. However, if
    // this stub can make calls, a pointer to it may be stored in a stub frame
    // on the stack, so we can't touch the stubCode_ or GC will crash when
    // tracing this pointer.
    if (!stub->makesGCCalls()) {
        stub->stubCode_ = (uint8_t*)0xbad;
    }
#endif
}

void
ICFallbackStub::unlinkStubsWithKind(JSContext* cx, ICStub::Kind kind)
{
    for (ICStubIterator iter = beginChain(); !iter.atEnd(); iter++) {
        if (iter->kind() == kind) {
            iter.unlink(cx);
        }
    }
}

void
ICFallbackStub::discardStubs(JSContext* cx)
{
    for (ICStubIterator iter = beginChain(); !iter.atEnd(); iter++) {
        iter.unlink(cx);
    }
}

void
ICTypeMonitor_Fallback::resetMonitorStubChain(Zone* zone)
{
    if (zone->needsIncrementalBarrier()) {
        // We are removing edges from monitored stubs to gcthings (JitCode).
        // Perform one final trace of all monitor stubs for incremental GC,
        // as it must know about those edges.
        for (ICStub* s = firstMonitorStub_; !s->isTypeMonitor_Fallback(); s = s->next()) {
            s->trace(zone->barrierTracer());
        }
    }

    firstMonitorStub_ = this;
    numOptimizedMonitorStubs_ = 0;

    if (hasFallbackStub_) {
        lastMonitorStubPtrAddr_ = nullptr;

        // Reset firstMonitorStub_ field of all monitored stubs.
        for (ICStubConstIterator iter = mainFallbackStub_->beginChainConst();
             !iter.atEnd(); iter++)
        {
            if (!iter->isMonitored()) {
                continue;
            }
            iter->toMonitoredStub()->resetFirstMonitorStub(this);
        }
    } else {
        icEntry_->setFirstStub(this);
        lastMonitorStubPtrAddr_ = icEntry_->addressOfFirstStub();
    }
}

void
ICUpdatedStub::resetUpdateStubChain(Zone* zone)
{
    while (!firstUpdateStub_->isTypeUpdate_Fallback()) {
        if (zone->needsIncrementalBarrier()) {
            // We are removing edges from update stubs to gcthings (JitCode).
            // Perform one final trace of all update stubs for incremental GC,
            // as it must know about those edges.
            firstUpdateStub_->trace(zone->barrierTracer());
        }
        firstUpdateStub_ = firstUpdateStub_->next();
    }

    numOptimizedStubs_ = 0;
}

ICMonitoredStub::ICMonitoredStub(Kind kind, JitCode* stubCode, ICStub* firstMonitorStub)
  : ICStub(kind, ICStub::Monitored, stubCode),
    firstMonitorStub_(firstMonitorStub)
{
    // In order to silence Coverity - null pointer dereference checker
    MOZ_ASSERT(firstMonitorStub_);
    // If the first monitored stub is a ICTypeMonitor_Fallback stub, then
    // double check that _its_ firstMonitorStub is the same as this one.
    MOZ_ASSERT_IF(firstMonitorStub_->isTypeMonitor_Fallback(),
                  firstMonitorStub_->toTypeMonitor_Fallback()->firstMonitorStub() ==
                     firstMonitorStub_);
}

bool
ICMonitoredFallbackStub::initMonitoringChain(JSContext* cx, JSScript* script)
{
    MOZ_ASSERT(fallbackMonitorStub_ == nullptr);

    ICTypeMonitor_Fallback::Compiler compiler(cx, this);
    ICStubSpace* space = script->baselineScript()->fallbackStubSpace();
    ICTypeMonitor_Fallback* stub = compiler.getStub(space);
    if (!stub) {
        return false;
    }
    fallbackMonitorStub_ = stub;
    return true;
}

bool
ICMonitoredFallbackStub::addMonitorStubForValue(JSContext* cx, BaselineFrame* frame,
                                                StackTypeSet* types, HandleValue val)
{
    ICTypeMonitor_Fallback* typeMonitorFallback = getFallbackMonitorStub(cx, frame->script());
    if (!typeMonitorFallback) {
        return false;
    }
    return typeMonitorFallback->addMonitorStubForValue(cx, frame, types, val);
}

bool
ICUpdatedStub::initUpdatingChain(JSContext* cx, ICStubSpace* space)
{
    MOZ_ASSERT(firstUpdateStub_ == nullptr);

    ICTypeUpdate_Fallback::Compiler compiler(cx);
    ICTypeUpdate_Fallback* stub = compiler.getStub(space);
    if (!stub) {
        return false;
    }

    firstUpdateStub_ = stub;
    return true;
}

JitCode*
ICStubCompiler::getStubCode()
{
    JitRealm* realm = cx->realm()->jitRealm();

    // Check for existing cached stubcode.
    uint32_t stubKey = getKey();
    JitCode* stubCode = realm->getStubCode(stubKey);
    if (stubCode) {
        return stubCode;
    }

    // Compile new stubcode.
    JitContext jctx(cx, nullptr);
    StackMacroAssembler masm;
#ifndef JS_USE_LINK_REGISTER
    // The first value contains the return addres,
    // which we pull into ICTailCallReg for tail calls.
    masm.adjustFrame(sizeof(intptr_t));
#endif
#ifdef JS_CODEGEN_ARM
    masm.setSecondScratchReg(BaselineSecondScratchReg);
#endif

    if (!generateStubCode(masm)) {
        return nullptr;
    }
    Linker linker(masm);
    AutoFlushICache afc("getStubCode");
    Rooted<JitCode*> newStubCode(cx, linker.newCode(cx, CodeKind::Baseline));
    if (!newStubCode) {
        return nullptr;
    }

    // Cache newly compiled stubcode.
    if (!realm->putStubCode(cx, stubKey, newStubCode)) {
        return nullptr;
    }

    // After generating code, run postGenerateStubCode().  We must not fail
    // after this point.
    postGenerateStubCode(masm, newStubCode);

    MOZ_ASSERT(entersStubFrame_ == ICStub::NonCacheIRStubMakesGCCalls(kind));
    MOZ_ASSERT(!inStubFrame_);

#ifdef JS_ION_PERF
    writePerfSpewerJitCodeProfile(newStubCode, "BaselineIC");
#endif

    return newStubCode;
}

bool
ICStubCompiler::tailCallVM(const VMFunction& fun, MacroAssembler& masm)
{
    TrampolinePtr code = cx->runtime()->jitRuntime()->getVMWrapper(fun);
    MOZ_ASSERT(fun.expectTailCall == TailCall);
    uint32_t argSize = fun.explicitStackSlots() * sizeof(void*);
    EmitBaselineTailCallVM(code, masm, argSize);
    return true;
}

bool
ICStubCompiler::callVM(const VMFunction& fun, MacroAssembler& masm)
{
    MOZ_ASSERT(inStubFrame_);

    TrampolinePtr code = cx->runtime()->jitRuntime()->getVMWrapper(fun);
    MOZ_ASSERT(fun.expectTailCall == NonTailCall);

    EmitBaselineCallVM(code, masm);
    return true;
}

void
ICStubCompiler::enterStubFrame(MacroAssembler& masm, Register scratch)
{
    EmitBaselineEnterStubFrame(masm, scratch);
#ifdef DEBUG
    framePushedAtEnterStubFrame_ = masm.framePushed();
#endif

    MOZ_ASSERT(!inStubFrame_);
    inStubFrame_ = true;

#ifdef DEBUG
    entersStubFrame_ = true;
#endif
}

void
ICStubCompiler::assumeStubFrame()
{
    MOZ_ASSERT(!inStubFrame_);
    inStubFrame_ = true;

#ifdef DEBUG
    entersStubFrame_ = true;

    // |framePushed| isn't tracked precisely in ICStubs, so simply assume it to
    // be STUB_FRAME_SIZE so that assertions don't fail in leaveStubFrame.
    framePushedAtEnterStubFrame_ = STUB_FRAME_SIZE;
#endif
}

void
ICStubCompiler::leaveStubFrame(MacroAssembler& masm, bool calledIntoIon)
{
    MOZ_ASSERT(entersStubFrame_ && inStubFrame_);
    inStubFrame_ = false;

#ifdef DEBUG
    masm.setFramePushed(framePushedAtEnterStubFrame_);
    if (calledIntoIon) {
        masm.adjustFrame(sizeof(intptr_t)); // Calls into ion have this extra.
    }
#endif
    EmitBaselineLeaveStubFrame(masm, calledIntoIon);
}

void
ICStubCompiler::pushStubPayload(MacroAssembler& masm, Register scratch)
{
    if (inStubFrame_) {
        masm.loadPtr(Address(BaselineFrameReg, 0), scratch);
        masm.pushBaselineFramePtr(scratch, scratch);
    } else {
        masm.pushBaselineFramePtr(BaselineFrameReg, scratch);
    }
}

void
ICStubCompiler::PushStubPayload(MacroAssembler& masm, Register scratch)
{
    pushStubPayload(masm, scratch);
    masm.adjustFrame(sizeof(intptr_t));
}

//
void
BaselineScript::noteAccessedGetter(uint32_t pcOffset)
{
    ICEntry& entry = icEntryFromPCOffset(pcOffset);
    ICFallbackStub* stub = entry.fallbackStub();

    if (stub->isGetProp_Fallback()) {
        stub->toGetProp_Fallback()->noteAccessedGetter();
    }
}

// TypeMonitor_Fallback
//

bool
ICTypeMonitor_Fallback::addMonitorStubForValue(JSContext* cx, BaselineFrame* frame,
                                               StackTypeSet* types, HandleValue val)
{
    MOZ_ASSERT(types);

    // Don't attach too many SingleObject/ObjectGroup stubs. If the value is a
    // primitive or if we will attach an any-object stub, we can handle this
    // with a single PrimitiveSet or AnyValue stub so we always optimize.
    if (numOptimizedMonitorStubs_ >= MAX_OPTIMIZED_STUBS &&
        val.isObject() &&
        !types->unknownObject())
    {
        return true;
    }

    bool wasDetachedMonitorChain = lastMonitorStubPtrAddr_ == nullptr;
    MOZ_ASSERT_IF(wasDetachedMonitorChain, numOptimizedMonitorStubs_ == 0);

    if (types->unknown()) {
        // The TypeSet got marked as unknown so attach a stub that always
        // succeeds.

        // Check for existing TypeMonitor_AnyValue stubs.
        for (ICStubConstIterator iter(firstMonitorStub()); !iter.atEnd(); iter++) {
            if (iter->isTypeMonitor_AnyValue()) {
                return true;
            }
        }

        // Discard existing stubs.
        resetMonitorStubChain(cx->zone());
        wasDetachedMonitorChain = (lastMonitorStubPtrAddr_ == nullptr);

        ICTypeMonitor_AnyValue::Compiler compiler(cx);
        ICStub* stub = compiler.getStub(compiler.getStubSpace(frame->script()));
        if (!stub) {
            ReportOutOfMemory(cx);
            return false;
        }

        JitSpew(JitSpew_BaselineIC, "  Added TypeMonitor stub %p for any value", stub);
        addOptimizedMonitorStub(stub);

    } else if (val.isPrimitive() || types->unknownObject()) {
        if (val.isMagic(JS_UNINITIALIZED_LEXICAL)) {
            return true;
        }
        MOZ_ASSERT(!val.isMagic());
        JSValueType type = val.isDouble() ? JSVAL_TYPE_DOUBLE : val.extractNonDoubleType();

        // Check for existing TypeMonitor stub.
        ICTypeMonitor_PrimitiveSet* existingStub = nullptr;
        for (ICStubConstIterator iter(firstMonitorStub()); !iter.atEnd(); iter++) {
            if (iter->isTypeMonitor_PrimitiveSet()) {
                existingStub = iter->toTypeMonitor_PrimitiveSet();
                if (existingStub->containsType(type)) {
                    return true;
                }
            }
        }

        if (val.isObject()) {
            // Check for existing SingleObject/ObjectGroup stubs and discard
            // stubs if we find one. Ideally we would discard just these stubs,
            // but unlinking individual type monitor stubs is somewhat
            // complicated.
            MOZ_ASSERT(types->unknownObject());
            bool hasObjectStubs = false;
            for (ICStubConstIterator iter(firstMonitorStub()); !iter.atEnd(); iter++) {
                if (iter->isTypeMonitor_SingleObject() || iter->isTypeMonitor_ObjectGroup()) {
                    hasObjectStubs = true;
                    break;
                }
            }
            if (hasObjectStubs) {
                resetMonitorStubChain(cx->zone());
                wasDetachedMonitorChain = (lastMonitorStubPtrAddr_ == nullptr);
                existingStub = nullptr;
            }
        }

        ICTypeMonitor_PrimitiveSet::Compiler compiler(cx, existingStub, type);
        ICStub* stub = existingStub
                       ? compiler.updateStub()
                       : compiler.getStub(compiler.getStubSpace(frame->script()));
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
        ICStub* stub = compiler.getStub(compiler.getStubSpace(frame->script()));
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
        ICStub* stub = compiler.getStub(compiler.getStubSpace(frame->script()));
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
            if (!iter->isMonitored()) {
                continue;
            }

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
    JSScript* script = frame->script();
    jsbytecode* pc = stub->icEntry()->pc(script);
    TypeFallbackICSpew(cx, stub, "TypeMonitor");

    // Copy input value to res.
    res.set(value);

    if (MOZ_UNLIKELY(value.isMagic())) {
        // It's possible that we arrived here from bailing out of Ion, and that
        // Ion proved that the value is dead and optimized out. In such cases,
        // do nothing. However, it's also possible that we have an uninitialized
        // this, in which case we should not look for other magic values.

        if (value.whyMagic() == JS_OPTIMIZED_OUT) {
            MOZ_ASSERT(!stub->monitorsThis());
            return true;
        }

        // In derived class constructors (including nested arrows/eval), the
        // |this| argument or GETALIASEDVAR can return the magic TDZ value.
        MOZ_ASSERT(value.isMagic(JS_UNINITIALIZED_LEXICAL));
        MOZ_ASSERT(frame->isFunctionFrame() || frame->isEvalFrame());
        MOZ_ASSERT(stub->monitorsThis() ||
                   *GetNextPc(pc) == JSOP_CHECKTHIS ||
                   *GetNextPc(pc) == JSOP_CHECKTHISREINIT ||
                   *GetNextPc(pc) == JSOP_CHECKRETURN);
        if (stub->monitorsThis()) {
            TypeScript::SetThis(cx, script, TypeSet::UnknownType());
        } else {
            TypeScript::Monitor(cx, script, pc, TypeSet::UnknownType());
        }
        return true;
    }

    StackTypeSet* types;
    uint32_t argument;
    if (stub->monitorsArgument(&argument)) {
        MOZ_ASSERT(pc == script->code());
        types = TypeScript::ArgTypes(script, argument);
        TypeScript::SetArgument(cx, script, argument, value);
    } else if (stub->monitorsThis()) {
        MOZ_ASSERT(pc == script->code());
        types = TypeScript::ThisTypes(script);
        TypeScript::SetThis(cx, script, value);
    } else {
        types = TypeScript::BytecodeTypes(script, pc);
        TypeScript::Monitor(cx, script, pc, types, value);
    }

    if (MOZ_UNLIKELY(stub->invalid())) {
        return true;
    }

    return stub->addMonitorStubForValue(cx, frame, types, value);
}

typedef bool (*DoTypeMonitorFallbackFn)(JSContext*, BaselineFrame*, ICTypeMonitor_Fallback*,
                                        HandleValue, MutableHandleValue);
static const VMFunction DoTypeMonitorFallbackInfo =
    FunctionInfo<DoTypeMonitorFallbackFn>(DoTypeMonitorFallback, "DoTypeMonitorFallback",
                                          TailCall);

bool
ICTypeMonitor_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    masm.pushValue(R0);
    masm.push(ICStubReg);
    masm.pushBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

    return tailCallVM(DoTypeMonitorFallbackInfo, masm);
}

bool
ICTypeMonitor_PrimitiveSet::Compiler::generateStubCode(MacroAssembler& masm)
{
    Label success;
    if ((flags_ & TypeToFlag(JSVAL_TYPE_INT32)) && !(flags_ & TypeToFlag(JSVAL_TYPE_DOUBLE))) {
        masm.branchTestInt32(Assembler::Equal, R0, &success);
    }

    if (flags_ & TypeToFlag(JSVAL_TYPE_DOUBLE)) {
        masm.branchTestNumber(Assembler::Equal, R0, &success);
    }

    if (flags_ & TypeToFlag(JSVAL_TYPE_UNDEFINED)) {
        masm.branchTestUndefined(Assembler::Equal, R0, &success);
    }

    if (flags_ & TypeToFlag(JSVAL_TYPE_BOOLEAN)) {
        masm.branchTestBoolean(Assembler::Equal, R0, &success);
    }

    if (flags_ & TypeToFlag(JSVAL_TYPE_STRING)) {
        masm.branchTestString(Assembler::Equal, R0, &success);
    }

    if (flags_ & TypeToFlag(JSVAL_TYPE_SYMBOL)) {
        masm.branchTestSymbol(Assembler::Equal, R0, &success);
    }

    if (flags_ & TypeToFlag(JSVAL_TYPE_OBJECT)) {
        masm.branchTestObject(Assembler::Equal, R0, &success);
    }

    if (flags_ & TypeToFlag(JSVAL_TYPE_NULL)) {
        masm.branchTestNull(Assembler::Equal, R0, &success);
    }

    EmitStubGuardFailure(masm);

    masm.bind(&success);
    EmitReturnFromIC(masm);
    return true;
}

static void
MaybeWorkAroundAmdBug(MacroAssembler& masm)
{
    // Attempt to work around an AMD bug (see bug 1034706 and bug 1281759), by
    // inserting 32-bytes of NOPs.
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    if (CPUInfo::NeedAmdBugWorkaround()) {
        masm.nop(9);
        masm.nop(9);
        masm.nop(9);
        masm.nop(5);
    }
#endif
}

bool
ICTypeMonitor_SingleObject::Compiler::generateStubCode(MacroAssembler& masm)
{
    Label failure;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);
    MaybeWorkAroundAmdBug(masm);

    // Guard on the object's identity.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    Address expectedObject(ICStubReg, ICTypeMonitor_SingleObject::offsetOfObject());
    masm.branchPtr(Assembler::NotEqual, expectedObject, obj, &failure);
    MaybeWorkAroundAmdBug(masm);

    EmitReturnFromIC(masm);
    MaybeWorkAroundAmdBug(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICTypeMonitor_ObjectGroup::Compiler::generateStubCode(MacroAssembler& masm)
{
    Label failure;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);
    MaybeWorkAroundAmdBug(masm);

    // Guard on the object's ObjectGroup. No Spectre mitigations are needed
    // here: we're just recording type information for Ion compilation and
    // it's safe to speculatively return.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    Address expectedGroup(ICStubReg, ICTypeMonitor_ObjectGroup::offsetOfGroup());
    masm.branchTestObjGroupNoSpectreMitigations(Assembler::NotEqual, obj, expectedGroup,
                                                R1.scratchReg(), &failure);
    MaybeWorkAroundAmdBug(masm);

    EmitReturnFromIC(masm);
    MaybeWorkAroundAmdBug(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICTypeMonitor_AnyValue::Compiler::generateStubCode(MacroAssembler& masm)
{
    EmitReturnFromIC(masm);
    return true;
}

bool
ICUpdatedStub::addUpdateStubForValue(JSContext* cx, HandleScript outerScript, HandleObject obj,
                                     HandleObjectGroup group, HandleId id, HandleValue val)
{
    EnsureTrackPropertyTypes(cx, obj, id);

    // Make sure that undefined values are explicitly included in the property
    // types for an object if generating a stub to write an undefined value.
    if (val.isUndefined() && CanHaveEmptyPropertyTypesForOwnProperty(obj)) {
        MOZ_ASSERT(obj->group() == group);
        AddTypePropertyId(cx, obj, id, val);
    }

    bool unknown = false, unknownObject = false;
    AutoSweepObjectGroup sweep(group);
    if (group->unknownProperties(sweep)) {
        unknown = unknownObject = true;
    } else {
        if (HeapTypeSet* types = group->maybeGetProperty(sweep, id)) {
            unknown = types->unknown();
            unknownObject = types->unknownObject();
        } else {
            // We don't record null/undefined types for certain TypedObject
            // properties. In these cases |types| is allowed to be nullptr
            // without implying unknown types. See DoTypeUpdateFallback.
            MOZ_ASSERT(obj->is<TypedObject>());
            MOZ_ASSERT(val.isNullOrUndefined());
        }
    }
    MOZ_ASSERT_IF(unknown, unknownObject);

    // Don't attach too many SingleObject/ObjectGroup stubs unless we can
    // replace them with a single PrimitiveSet or AnyValue stub.
    if (numOptimizedStubs_ >= MAX_OPTIMIZED_STUBS &&
        val.isObject() &&
        !unknownObject)
    {
        return true;
    }

    if (unknown) {
        // Attach a stub that always succeeds. We should not have a
        // TypeUpdate_AnyValue stub yet.
        MOZ_ASSERT(!hasTypeUpdateStub(TypeUpdate_AnyValue));

        // Discard existing stubs.
        resetUpdateStubChain(cx->zone());

        ICTypeUpdate_AnyValue::Compiler compiler(cx);
        ICStub* stub = compiler.getStub(compiler.getStubSpace(outerScript));
        if (!stub) {
            return false;
        }

        JitSpew(JitSpew_BaselineIC, "  Added TypeUpdate stub %p for any value", stub);
        addOptimizedUpdateStub(stub);

    } else if (val.isPrimitive() || unknownObject) {
        JSValueType type = val.isDouble() ? JSVAL_TYPE_DOUBLE : val.extractNonDoubleType();

        // Check for existing TypeUpdate stub.
        ICTypeUpdate_PrimitiveSet* existingStub = nullptr;
        for (ICStubConstIterator iter(firstUpdateStub_); !iter.atEnd(); iter++) {
            if (iter->isTypeUpdate_PrimitiveSet()) {
                existingStub = iter->toTypeUpdate_PrimitiveSet();
                MOZ_ASSERT(!existingStub->containsType(type));
            }
        }

        if (val.isObject()) {
            // Discard existing ObjectGroup/SingleObject stubs.
            resetUpdateStubChain(cx->zone());
            if (existingStub) {
                addOptimizedUpdateStub(existingStub);
            }
        }

        ICTypeUpdate_PrimitiveSet::Compiler compiler(cx, existingStub, type);
        ICStub* stub = existingStub ? compiler.updateStub()
                                    : compiler.getStub(compiler.getStubSpace(outerScript));
        if (!stub) {
            return false;
        }
        if (!existingStub) {
            MOZ_ASSERT(!hasTypeUpdateStub(TypeUpdate_PrimitiveSet));
            addOptimizedUpdateStub(stub);
        }

        JitSpew(JitSpew_BaselineIC, "  %s TypeUpdate stub %p for primitive type %d",
                existingStub ? "Modified existing" : "Created new", stub, type);

    } else if (val.toObject().isSingleton()) {
        RootedObject obj(cx, &val.toObject());

#ifdef DEBUG
        // We should not have a stub for this object.
        for (ICStubConstIterator iter(firstUpdateStub_); !iter.atEnd(); iter++) {
            MOZ_ASSERT_IF(iter->isTypeUpdate_SingleObject(),
                          iter->toTypeUpdate_SingleObject()->object() != obj);
        }
#endif

        ICTypeUpdate_SingleObject::Compiler compiler(cx, obj);
        ICStub* stub = compiler.getStub(compiler.getStubSpace(outerScript));
        if (!stub) {
            return false;
        }

        JitSpew(JitSpew_BaselineIC, "  Added TypeUpdate stub %p for singleton %p", stub, obj.get());

        addOptimizedUpdateStub(stub);

    } else {
        RootedObjectGroup group(cx, val.toObject().group());

#ifdef DEBUG
        // We should not have a stub for this group.
        for (ICStubConstIterator iter(firstUpdateStub_); !iter.atEnd(); iter++) {
            MOZ_ASSERT_IF(iter->isTypeUpdate_ObjectGroup(),
                          iter->toTypeUpdate_ObjectGroup()->group() != group);
        }
#endif

        ICTypeUpdate_ObjectGroup::Compiler compiler(cx, group);
        ICStub* stub = compiler.getStub(compiler.getStubSpace(outerScript));
        if (!stub) {
            return false;
        }

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
    // This can get called from optimized stubs. Therefore it is not allowed to gc.
    JS::AutoCheckCannotGC nogc;

    FallbackICSpew(cx, stub->getChainFallback(), "TypeUpdate(%s)",
                   ICStub::KindString(stub->kind()));

    MOZ_ASSERT(stub->isCacheIR_Updated());

    RootedScript script(cx, frame->script());
    RootedObject obj(cx, &objval.toObject());

    RootedId id(cx, stub->toCacheIR_Updated()->updateStubId());
    MOZ_ASSERT(id != JSID_EMPTY);

    // The group should match the object's group, except when the object is
    // an unboxed expando object: in that case, the group is the group of
    // the unboxed object.
    RootedObjectGroup group(cx, stub->toCacheIR_Updated()->updateStubGroup());
#ifdef DEBUG
    if (obj->is<UnboxedExpandoObject>()) {
        MOZ_ASSERT(group->clasp() == &UnboxedPlainObject::class_);
    } else {
        MOZ_ASSERT(obj->group() == group);
    }
#endif

    // If we're storing null/undefined to a typed object property, check if
    // we want to include it in this property's type information.
    bool addType = true;
    if (MOZ_UNLIKELY(obj->is<TypedObject>()) && value.isNullOrUndefined()) {
        StructTypeDescr* structDescr = &obj->as<TypedObject>().typeDescr().as<StructTypeDescr>();
        size_t fieldIndex;
        MOZ_ALWAYS_TRUE(structDescr->fieldIndex(id, &fieldIndex));

        TypeDescr* fieldDescr = &structDescr->fieldDescr(fieldIndex);
        ReferenceType type = fieldDescr->as<ReferenceTypeDescr>().type();
        if (type == ReferenceType::TYPE_ANY) {
            // Ignore undefined values, which are included implicitly in type
            // information for this property.
            if (value.isUndefined()) {
                addType = false;
            }
        } else {
            MOZ_ASSERT(type == ReferenceType::TYPE_OBJECT);

            // Ignore null values being written here. Null is included
            // implicitly in type information for this property. Note that
            // non-object, non-null values are not possible here, these
            // should have been filtered out by the IR emitter.
            if (value.isNull()) {
                addType = false;
            }
        }
    }

    if (MOZ_LIKELY(addType)) {
        JSObject* maybeSingleton = obj->isSingleton() ? obj.get() : nullptr;
        AddTypePropertyId(cx, group, maybeSingleton, id, value);
    }

    if (MOZ_UNLIKELY(!stub->addUpdateStubForValue(cx, script, obj, group, id, value))) {
        // The calling JIT code assumes this function is infallible (for
        // instance we may reallocate dynamic slots before calling this),
        // so ignore OOMs if we failed to attach a stub.
        cx->recoverFromOutOfMemory();
    }

    return true;
}

typedef bool (*DoTypeUpdateFallbackFn)(JSContext*, BaselineFrame*, ICUpdatedStub*, HandleValue,
                                       HandleValue);
const VMFunction DoTypeUpdateFallbackInfo =
    FunctionInfo<DoTypeUpdateFallbackFn>(DoTypeUpdateFallback, "DoTypeUpdateFallback", NonTailCall);

bool
ICTypeUpdate_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    // Just store false into R1.scratchReg() and return.
    masm.move32(Imm32(0), R1.scratchReg());
    EmitReturnFromIC(masm);
    return true;
}

bool
ICTypeUpdate_PrimitiveSet::Compiler::generateStubCode(MacroAssembler& masm)
{
    Label success;
    if ((flags_ & TypeToFlag(JSVAL_TYPE_INT32)) && !(flags_ & TypeToFlag(JSVAL_TYPE_DOUBLE))) {
        masm.branchTestInt32(Assembler::Equal, R0, &success);
    }

    if (flags_ & TypeToFlag(JSVAL_TYPE_DOUBLE)) {
        masm.branchTestNumber(Assembler::Equal, R0, &success);
    }

    if (flags_ & TypeToFlag(JSVAL_TYPE_UNDEFINED)) {
        masm.branchTestUndefined(Assembler::Equal, R0, &success);
    }

    if (flags_ & TypeToFlag(JSVAL_TYPE_BOOLEAN)) {
        masm.branchTestBoolean(Assembler::Equal, R0, &success);
    }

    if (flags_ & TypeToFlag(JSVAL_TYPE_STRING)) {
        masm.branchTestString(Assembler::Equal, R0, &success);
    }

    if (flags_ & TypeToFlag(JSVAL_TYPE_SYMBOL)) {
        masm.branchTestSymbol(Assembler::Equal, R0, &success);
    }

    if (flags_ & TypeToFlag(JSVAL_TYPE_OBJECT)) {
        masm.branchTestObject(Assembler::Equal, R0, &success);
    }

    if (flags_ & TypeToFlag(JSVAL_TYPE_NULL)) {
        masm.branchTestNull(Assembler::Equal, R0, &success);
    }

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
    Label failure;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);

    // Guard on the object's ObjectGroup.
    Address expectedGroup(ICStubReg, ICTypeUpdate_ObjectGroup::offsetOfGroup());
    Register scratch1 = R1.scratchReg();
    masm.unboxObject(R0, scratch1);
    masm.branchTestObjGroup(Assembler::NotEqual, scratch1, expectedGroup, scratch1,
                            R0.payloadOrValueReg(), &failure);

    // Group matches, load true into R1.scratchReg() and return.
    masm.mov(ImmWord(1), R1.scratchReg());
    EmitReturnFromIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICTypeUpdate_AnyValue::Compiler::generateStubCode(MacroAssembler& masm)
{
    // AnyValue always matches so return true.
    masm.mov(ImmWord(1), R1.scratchReg());
    EmitReturnFromIC(masm);
    return true;
}

//
// ToBool_Fallback
//

static bool
DoToBoolFallback(JSContext* cx, BaselineFrame* frame, ICToBool_Fallback* stub, HandleValue arg,
                 MutableHandleValue ret)
{
    stub->incrementEnteredCount();
    FallbackICSpew(cx, stub, "ToBool");

    MOZ_ASSERT(!arg.isBoolean());

    TryAttachStub<ToBoolIRGenerator>("ToBool", cx, frame, stub, BaselineCacheIRStubKind::Regular, arg);

    bool cond = ToBoolean(arg);
    ret.setBoolean(cond);

    return true;
}

typedef bool (*pf)(JSContext*, BaselineFrame*, ICToBool_Fallback*, HandleValue,
                   MutableHandleValue);
static const VMFunction fun = FunctionInfo<pf>(DoToBoolFallback, "DoToBoolFallback", TailCall);

bool
ICToBool_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    // Push arguments.
    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushStubPayload(masm, R0.scratchReg());

    return tailCallVM(fun, masm);
}


//
// ToNumber_Fallback
//

static bool
DoToNumberFallback(JSContext* cx, ICToNumber_Fallback* stub, HandleValue arg, MutableHandleValue ret)
{
    stub->incrementEnteredCount();
    FallbackICSpew(cx, stub, "ToNumber");
    ret.set(arg);
    return ToNumber(cx, ret);
}

typedef bool (*DoToNumberFallbackFn)(JSContext*, ICToNumber_Fallback*, HandleValue, MutableHandleValue);
static const VMFunction DoToNumberFallbackInfo =
    FunctionInfo<DoToNumberFallbackFn>(DoToNumberFallback, "DoToNumberFallback", TailCall,
                                       PopValues(1));

bool
ICToNumber_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
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
        if (iter->isCacheIR_Regular() && iter->toCacheIR_Regular()->hasPreliminaryObject()) {
            iter.unlink(cx);
        } else if (iter->isCacheIR_Monitored() && iter->toCacheIR_Monitored()->hasPreliminaryObject()) {
            iter.unlink(cx);
        } else if (iter->isCacheIR_Updated() && iter->toCacheIR_Updated()->hasPreliminaryObject()) {
            iter.unlink(cx);
        }
    }
}

//
// GetElem_Fallback
//

static bool
DoGetElemFallback(JSContext* cx, BaselineFrame* frame, ICGetElem_Fallback* stub_, HandleValue lhs,
                  HandleValue rhs, MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICGetElem_Fallback*> stub(frame, stub_);
    stub->incrementEnteredCount();

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(frame->script());
    StackTypeSet* types = TypeScript::BytecodeTypes(script, pc);

    JSOp op = JSOp(*pc);
    FallbackICSpew(cx, stub, "GetElem(%s)", CodeName[op]);

    MOZ_ASSERT(op == JSOP_GETELEM || op == JSOP_CALLELEM);

    // Don't pass lhs directly, we need it when generating stubs.
    RootedValue lhsCopy(cx, lhs);

    bool isOptimizedArgs = false;
    if (lhs.isMagic(JS_OPTIMIZED_ARGUMENTS)) {
        // Handle optimized arguments[i] access.
        if (!GetElemOptimizedArguments(cx, frame, &lhsCopy, rhs, res, &isOptimizedArgs)) {
            return false;
        }
        if (isOptimizedArgs) {
            TypeScript::Monitor(cx, script, pc, types, res);
        }
    }

    bool attached = false;
    bool isTemporarilyUnoptimizable = false;

    if (stub->state().maybeTransition()) {
        stub->discardStubs(cx);
    }

    if (stub->state().canAttachStub()) {
        GetPropIRGenerator gen(cx, script, pc,
                               CacheKind::GetElem, stub->state().mode(),
                               &isTemporarilyUnoptimizable, lhs, rhs, lhs,
                               GetPropertyResultFlags::All);
        if (gen.tryAttachStub()) {
            ICStub* newStub = AttachBaselineCacheIRStub(cx, gen.writerRef(), gen.cacheKind(),
                                                        BaselineCacheIRStubKind::Monitored,
                                                        script, stub, &attached);
            if (newStub) {
                JitSpew(JitSpew_BaselineIC, "  Attached GetElem CacheIR stub");
                if (gen.shouldNotePreliminaryObjectStub()) {
                    newStub->toCacheIR_Monitored()->notePreliminaryObject();
                } else if (gen.shouldUnlinkPreliminaryObjectStubs()) {
                    StripPreliminaryObjectStubs(cx, stub);
                }
            }
        }
        if (!attached && !isTemporarilyUnoptimizable) {
            stub->state().trackNotAttached();
        }
    }

    if (!isOptimizedArgs) {
        if (!GetElementOperation(cx, op, lhsCopy, rhs, res)) {
            return false;
        }
        TypeScript::Monitor(cx, script, pc, types, res);
    }

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid()) {
        return true;
    }

    // Add a type monitor stub for the resulting value.
    if (!stub->addMonitorStubForValue(cx, frame, types, res)) {
        return false;
    }

    if (attached) {
        return true;
    }

    // GetElem operations which could access negative indexes generally can't
    // be optimized without the potential for bailouts, as we can't statically
    // determine that an object has no properties on such indexes.
    if (rhs.isNumber() && rhs.toNumber() < 0) {
        stub->noteNegativeIndex();
    }

    return true;
}

static bool
DoGetElemSuperFallback(JSContext* cx, BaselineFrame* frame, ICGetElem_Fallback* stub_,
                       HandleValue lhs, HandleValue rhs, HandleValue receiver,
                       MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICGetElem_Fallback*> stub(frame, stub_);
    stub->incrementEnteredCount();

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(frame->script());
    StackTypeSet* types = TypeScript::BytecodeTypes(script, pc);

    JSOp op = JSOp(*pc);
    FallbackICSpew(cx, stub, "GetElemSuper(%s)", CodeName[op]);

    MOZ_ASSERT(op == JSOP_GETELEM_SUPER);

    bool attached = false;
    bool isTemporarilyUnoptimizable = false;

    if (stub->state().maybeTransition()) {
        stub->discardStubs(cx);
    }

    if (stub->state().canAttachStub()) {
        GetPropIRGenerator gen(cx, script, pc, CacheKind::GetElemSuper, stub->state().mode(),
                               &isTemporarilyUnoptimizable, lhs, rhs, receiver,
                               GetPropertyResultFlags::All);
        if (gen.tryAttachStub()) {
            ICStub* newStub = AttachBaselineCacheIRStub(cx, gen.writerRef(), gen.cacheKind(),
                                                        BaselineCacheIRStubKind::Monitored,
                                                        script, stub, &attached);
            if (newStub) {
                JitSpew(JitSpew_BaselineIC, "  Attached GetElemSuper CacheIR stub");
                if (gen.shouldNotePreliminaryObjectStub()) {
                    newStub->toCacheIR_Monitored()->notePreliminaryObject();
                } else if (gen.shouldUnlinkPreliminaryObjectStubs()) {
                    StripPreliminaryObjectStubs(cx, stub);
                }
            }
        }
        if (!attached && !isTemporarilyUnoptimizable) {
            stub->state().trackNotAttached();
        }
    }

    // |lhs| is [[HomeObject]].[[Prototype]] which must be Object
    RootedObject lhsObj(cx, &lhs.toObject());
    if (!GetObjectElementOperation(cx, op, lhsObj, receiver, rhs, res)) {
        return false;
    }
    TypeScript::Monitor(cx, script, pc, types, res);

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid()) {
        return true;
    }

    // Add a type monitor stub for the resulting value.
    if (!stub->addMonitorStubForValue(cx, frame, types, res)) {
        return false;
    }

    if (attached) {
        return true;
    }

    // GetElem operations which could access negative indexes generally can't
    // be optimized without the potential for bailouts, as we can't statically
    // determine that an object has no properties on such indexes.
    if (rhs.isNumber() && rhs.toNumber() < 0) {
        stub->noteNegativeIndex();
    }

    return true;
}

typedef bool (*DoGetElemFallbackFn)(JSContext*, BaselineFrame*, ICGetElem_Fallback*,
                                    HandleValue, HandleValue, MutableHandleValue);
static const VMFunction DoGetElemFallbackInfo =
    FunctionInfo<DoGetElemFallbackFn>(DoGetElemFallback, "DoGetElemFallback", TailCall,
                                      PopValues(2));

typedef bool (*DoGetElemSuperFallbackFn)(JSContext*, BaselineFrame*, ICGetElem_Fallback*,
                                         HandleValue, HandleValue, HandleValue,
                                         MutableHandleValue);
static const VMFunction DoGetElemSuperFallbackInfo =
    FunctionInfo<DoGetElemSuperFallbackFn>(DoGetElemSuperFallback, "DoGetElemSuperFallback",
                                           TailCall, PopValues(3));

bool
ICGetElem_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    // Super property getters use a |this| that differs from base object
    if (hasReceiver_) {
        // State: receiver in R0, index in R1, obj on the stack

        // Ensure stack is fully synced for the expression decompiler.
        // We need: receiver, index, obj
        masm.pushValue(R0);
        masm.pushValue(R1);
        masm.pushValue(Address(masm.getStackPointer(), sizeof(Value) * 2));

        // Push arguments.
        masm.pushValue(R0); // Receiver
        masm.pushValue(R1); // Index
        masm.pushValue(Address(masm.getStackPointer(), sizeof(Value) * 5)); // Obj
        masm.push(ICStubReg);
        pushStubPayload(masm, R0.scratchReg());

        return tailCallVM(DoGetElemSuperFallbackInfo, masm);
    }

    // Ensure stack is fully synced for the expression decompiler.
    masm.pushValue(R0);
    masm.pushValue(R1);

    // Push arguments.
    masm.pushValue(R1);
    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushStubPayload(masm, R0.scratchReg());

    return tailCallVM(DoGetElemFallbackInfo, masm);
}

static void
SetUpdateStubData(ICCacheIR_Updated* stub, const PropertyTypeCheckInfo* info)
{
    if (info->isSet()) {
        stub->updateStubGroup() = info->group();
        stub->updateStubId() = info->id();
    }
}

static bool
DoSetElemFallback(JSContext* cx, BaselineFrame* frame, ICSetElem_Fallback* stub_, Value* stack,
                  HandleValue objv, HandleValue index, HandleValue rhs)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICSetElem_Fallback*> stub(frame, stub_);
    stub->incrementEnteredCount();

    RootedScript script(cx, frame->script());
    RootedScript outerScript(cx, script);
    jsbytecode* pc = stub->icEntry()->pc(script);
    JSOp op = JSOp(*pc);
    FallbackICSpew(cx, stub, "SetElem(%s)", CodeName[JSOp(*pc)]);

    MOZ_ASSERT(op == JSOP_SETELEM ||
               op == JSOP_STRICTSETELEM ||
               op == JSOP_INITELEM ||
               op == JSOP_INITHIDDENELEM ||
               op == JSOP_INITELEM_ARRAY ||
               op == JSOP_INITELEM_INC);

    RootedObject obj(cx, ToObjectFromStackForPropertyAccess(cx, objv, index));
    if (!obj) {
        return false;
    }

    RootedShape oldShape(cx, obj->maybeShape());
    RootedObjectGroup oldGroup(cx, JSObject::getGroup(cx, obj));
    if (!oldGroup) {
        return false;
    }

    if (obj->is<UnboxedPlainObject>()) {
        MOZ_ASSERT(!oldShape);
        if (UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando()) {
            oldShape = expando->lastProperty();
        }
    }

    bool isTemporarilyUnoptimizable = false;
    bool attached = false;

    if (stub->state().maybeTransition()) {
        stub->discardStubs(cx);
    }

    if (stub->state().canAttachStub()) {
        SetPropIRGenerator gen(cx, script, pc, CacheKind::SetElem, stub->state().mode(),
                               &isTemporarilyUnoptimizable, objv, index, rhs);
        if (gen.tryAttachStub()) {
            ICStub* newStub = AttachBaselineCacheIRStub(cx, gen.writerRef(), gen.cacheKind(),
                                                        BaselineCacheIRStubKind::Updated,
                                                        frame->script(), stub, &attached);
            if (newStub) {
                JitSpew(JitSpew_BaselineIC, "  Attached SetElem CacheIR stub");

                SetUpdateStubData(newStub->toCacheIR_Updated(), gen.typeCheckInfo());

                if (gen.shouldNotePreliminaryObjectStub()) {
                    newStub->toCacheIR_Updated()->notePreliminaryObject();
                } else if (gen.shouldUnlinkPreliminaryObjectStubs()) {
                    StripPreliminaryObjectStubs(cx, stub);
                }

                if (gen.attachedTypedArrayOOBStub()) {
                    stub->noteHasTypedArrayOOB();
                }
            }
        }
    }

    if (op == JSOP_INITELEM || op == JSOP_INITHIDDENELEM) {
        if (!InitElemOperation(cx, pc, obj, index, rhs)) {
            return false;
        }
    } else if (op == JSOP_INITELEM_ARRAY) {
        MOZ_ASSERT(uint32_t(index.toInt32()) <= INT32_MAX,
                   "the bytecode emitter must fail to compile code that would "
                   "produce JSOP_INITELEM_ARRAY with an index exceeding "
                   "int32_t range");
        MOZ_ASSERT(uint32_t(index.toInt32()) == GET_UINT32(pc));
        if (!InitArrayElemOperation(cx, pc, obj, index.toInt32(), rhs)) {
            return false;
        }
    } else if (op == JSOP_INITELEM_INC) {
        if (!InitArrayElemOperation(cx, pc, obj, index.toInt32(), rhs)) {
            return false;
        }
    } else {
        if (!SetObjectElement(cx, obj, index, rhs, objv, JSOp(*pc) == JSOP_STRICTSETELEM, script, pc)) {
            return false;
        }
    }

    // Don't try to attach stubs that wish to be hidden. We don't know how to
    // have different enumerability in the stubs for the moment.
    if (op == JSOP_INITHIDDENELEM) {
        return true;
    }

    // Overwrite the object on the stack (pushed for the decompiler) with the rhs.
    MOZ_ASSERT(stack[2] == objv);
    stack[2] = rhs;

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid()) {
        return true;
    }

    if (attached) {
        return true;
    }

    // The SetObjectElement call might have entered this IC recursively, so try
    // to transition.
    if (stub->state().maybeTransition()) {
        stub->discardStubs(cx);
    }

    if (stub->state().canAttachStub()) {
        SetPropIRGenerator gen(cx, script, pc, CacheKind::SetElem, stub->state().mode(),
                               &isTemporarilyUnoptimizable, objv, index, rhs);
        if (gen.tryAttachAddSlotStub(oldGroup, oldShape)) {
            ICStub* newStub = AttachBaselineCacheIRStub(cx, gen.writerRef(), gen.cacheKind(),
                                                        BaselineCacheIRStubKind::Updated,
                                                        frame->script(), stub, &attached);
            if (newStub) {
                JitSpew(JitSpew_BaselineIC, "  Attached SetElem CacheIR stub");

                SetUpdateStubData(newStub->toCacheIR_Updated(), gen.typeCheckInfo());

                if (gen.shouldNotePreliminaryObjectStub()) {
                    newStub->toCacheIR_Updated()->notePreliminaryObject();
                } else if (gen.shouldUnlinkPreliminaryObjectStubs()) {
                    StripPreliminaryObjectStubs(cx, stub);
                }
                return true;
            }
        } else {
            gen.trackAttached(IRGenerator::NotAttached);
        }
        if (!attached && !isTemporarilyUnoptimizable) {
            stub->state().trackNotAttached();
        }
    }

    return true;
}

typedef bool (*DoSetElemFallbackFn)(JSContext*, BaselineFrame*, ICSetElem_Fallback*, Value*,
                                    HandleValue, HandleValue, HandleValue);
static const VMFunction DoSetElemFallbackInfo =
    FunctionInfo<DoSetElemFallbackFn>(DoSetElemFallback, "DoSetElemFallback", TailCall,
                                      PopValues(2));

bool
ICSetElem_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
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
    pushStubPayload(masm, R0.scratchReg());

    return tailCallVM(DoSetElemFallbackInfo, masm);
}

void
BaselineScript::noteHasDenseAdd(uint32_t pcOffset)
{
    ICEntry& entry = icEntryFromPCOffset(pcOffset);
    ICFallbackStub* stub = entry.fallbackStub();

    if (stub->isSetElem_Fallback()) {
        stub->toSetElem_Fallback()->noteHasDenseAdd();
    }
}

template <typename T>
void
EmitICUnboxedPreBarrier(MacroAssembler& masm, const T& address, JSValueType type)
{
    if (type == JSVAL_TYPE_OBJECT) {
        EmitPreBarrier(masm, address, MIRType::Object);
    } else if (type == JSVAL_TYPE_STRING) {
        EmitPreBarrier(masm, address, MIRType::String);
    } else {
        MOZ_ASSERT(!UnboxedTypeNeedsPreBarrier(type));
    }
}

template void
EmitICUnboxedPreBarrier(MacroAssembler& masm, const Address& address, JSValueType type);

template void
EmitICUnboxedPreBarrier(MacroAssembler& masm, const BaseIndex& address, JSValueType type);

template <typename T>
void
StoreToTypedArray(JSContext* cx, MacroAssembler& masm, Scalar::Type type,
                  const ValueOperand& value, const T& dest, Register scratch,
                  Label* failure)
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
        masm.branchTestDouble(Assembler::NotEqual, value, failure);
        masm.unboxDouble(value, FloatReg0);
        masm.clampDoubleToUint8(FloatReg0, scratch);
        masm.jump(&clamped);
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
        masm.branchTestDouble(Assembler::NotEqual, value, failure);
        masm.unboxDouble(value, FloatReg0);
        masm.branchTruncateDoubleMaybeModUint32(FloatReg0, scratch, failure);
        masm.jump(&isInt32);
    }

    masm.bind(&done);
}

template void
StoreToTypedArray(JSContext* cx, MacroAssembler& masm, Scalar::Type type,
                  const ValueOperand& value, const Address& dest, Register scratch,
                  Label* failure);

template void
StoreToTypedArray(JSContext* cx, MacroAssembler& masm, Scalar::Type type,
                  const ValueOperand& value, const BaseIndex& dest, Register scratch,
                  Label* failure);

//
// In_Fallback
//

static bool
DoInFallback(JSContext* cx, BaselineFrame* frame, ICIn_Fallback* stub_,
             HandleValue key, HandleValue objValue, MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICIn_Fallback*> stub(frame, stub_);
    stub->incrementEnteredCount();

    FallbackICSpew(cx, stub, "In");

    if (!objValue.isObject()) {
        ReportInNotObjectError(cx, key, -2, objValue, -1);
        return false;
    }

    TryAttachStub<HasPropIRGenerator>("In", cx, frame, stub, BaselineCacheIRStubKind::Regular, CacheKind::In, key, objValue);

    RootedObject obj(cx, &objValue.toObject());
    bool cond = false;
    if (!OperatorIn(cx, key, obj, &cond)) {
        return false;
    }
    res.setBoolean(cond);

    return true;
}

typedef bool (*DoInFallbackFn)(JSContext*, BaselineFrame*, ICIn_Fallback*, HandleValue,
                               HandleValue, MutableHandleValue);
static const VMFunction DoInFallbackInfo =
    FunctionInfo<DoInFallbackFn>(DoInFallback, "DoInFallback", TailCall, PopValues(2));

bool
ICIn_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    EmitRestoreTailCallReg(masm);

    // Sync for the decompiler.
    masm.pushValue(R0);
    masm.pushValue(R1);

    // Push arguments.
    masm.pushValue(R1);
    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushStubPayload(masm, R0.scratchReg());

    return tailCallVM(DoInFallbackInfo, masm);
}

//
// HasOwn_Fallback
//

static bool
DoHasOwnFallback(JSContext* cx, BaselineFrame* frame, ICHasOwn_Fallback* stub_,
                 HandleValue keyValue, HandleValue objValue, MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICIn_Fallback*> stub(frame, stub_);
    stub->incrementEnteredCount();

    FallbackICSpew(cx, stub, "HasOwn");

    TryAttachStub<HasPropIRGenerator>("HasOwn", cx, frame, stub,
        BaselineCacheIRStubKind::Regular, CacheKind::HasOwn,
        keyValue, objValue);

    bool found;
    if (!HasOwnProperty(cx, objValue, keyValue, &found)) {
        return false;
    }

    res.setBoolean(found);
    return true;
}

typedef bool (*DoHasOwnFallbackFn)(JSContext*, BaselineFrame*, ICHasOwn_Fallback*, HandleValue,
                               HandleValue, MutableHandleValue);
static const VMFunction DoHasOwnFallbackInfo =
    FunctionInfo<DoHasOwnFallbackFn>(DoHasOwnFallback, "DoHasOwnFallback", TailCall, PopValues(2));

bool
ICHasOwn_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    EmitRestoreTailCallReg(masm);

    // Sync for the decompiler.
    masm.pushValue(R0);
    masm.pushValue(R1);

    // Push arguments.
    masm.pushValue(R1);
    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushStubPayload(masm, R0.scratchReg());

    return tailCallVM(DoHasOwnFallbackInfo, masm);
}


//
// GetName_Fallback
//

static bool
DoGetNameFallback(JSContext* cx, BaselineFrame* frame, ICGetName_Fallback* stub_,
                  HandleObject envChain, MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICGetName_Fallback*> stub(frame, stub_);
    stub->incrementEnteredCount();

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    mozilla::DebugOnly<JSOp> op = JSOp(*pc);
    FallbackICSpew(cx, stub, "GetName(%s)", CodeName[JSOp(*pc)]);

    MOZ_ASSERT(op == JSOP_GETNAME || op == JSOP_GETGNAME);

    RootedPropertyName name(cx, script->getName(pc));

    TryAttachStub<GetNameIRGenerator>("GetName", cx, frame, stub, BaselineCacheIRStubKind::Monitored, envChain, name);

    static_assert(JSOP_GETGNAME_LENGTH == JSOP_GETNAME_LENGTH,
                  "Otherwise our check for JSOP_TYPEOF isn't ok");
    if (JSOp(pc[JSOP_GETGNAME_LENGTH]) == JSOP_TYPEOF) {
        if (!GetEnvironmentName<GetNameMode::TypeOf>(cx, envChain, name, res)) {
            return false;
        }
    } else {
        if (!GetEnvironmentName<GetNameMode::Normal>(cx, envChain, name, res)) {
            return false;
        }
    }

    StackTypeSet* types = TypeScript::BytecodeTypes(script, pc);
    TypeScript::Monitor(cx, script, pc, types, res);

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid()) {
        return true;
    }

    // Add a type monitor stub for the resulting value.
    if (!stub->addMonitorStubForValue(cx, frame, types, res)) {
        return false;
    }

    return true;
}

typedef bool (*DoGetNameFallbackFn)(JSContext*, BaselineFrame*, ICGetName_Fallback*,
                                    HandleObject, MutableHandleValue);
static const VMFunction DoGetNameFallbackInfo =
    FunctionInfo<DoGetNameFallbackFn>(DoGetNameFallback, "DoGetNameFallback", TailCall);

bool
ICGetName_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(R0 == JSReturnOperand);

    EmitRestoreTailCallReg(masm);

    masm.push(R0.scratchReg());
    masm.push(ICStubReg);
    pushStubPayload(masm, R0.scratchReg());

    return tailCallVM(DoGetNameFallbackInfo, masm);
}

//
// BindName_Fallback
//

static bool
DoBindNameFallback(JSContext* cx, BaselineFrame* frame, ICBindName_Fallback* stub,
                   HandleObject envChain, MutableHandleValue res)
{
    stub->incrementEnteredCount();

    jsbytecode* pc = stub->icEntry()->pc(frame->script());
    mozilla::DebugOnly<JSOp> op = JSOp(*pc);
    FallbackICSpew(cx, stub, "BindName(%s)", CodeName[JSOp(*pc)]);

    MOZ_ASSERT(op == JSOP_BINDNAME || op == JSOP_BINDGNAME);

    RootedPropertyName name(cx, frame->script()->getName(pc));

    TryAttachStub<BindNameIRGenerator>("BindName", cx, frame, stub, BaselineCacheIRStubKind::Regular, envChain, name);

    RootedObject scope(cx);
    if (!LookupNameUnqualified(cx, name, envChain, &scope)) {
        return false;
    }

    res.setObject(*scope);
    return true;
}

typedef bool (*DoBindNameFallbackFn)(JSContext*, BaselineFrame*, ICBindName_Fallback*,
                                     HandleObject, MutableHandleValue);
static const VMFunction DoBindNameFallbackInfo =
    FunctionInfo<DoBindNameFallbackFn>(DoBindNameFallback, "DoBindNameFallback", TailCall);

bool
ICBindName_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(R0 == JSReturnOperand);

    EmitRestoreTailCallReg(masm);

    masm.push(R0.scratchReg());
    masm.push(ICStubReg);
    pushStubPayload(masm, R0.scratchReg());

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
    stub->incrementEnteredCount();

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    mozilla::DebugOnly<JSOp> op = JSOp(*pc);
    FallbackICSpew(cx, stub, "GetIntrinsic(%s)", CodeName[JSOp(*pc)]);

    MOZ_ASSERT(op == JSOP_GETINTRINSIC);

    if (!GetIntrinsicOperation(cx, script, pc, res)) {
        return false;
    }

    // An intrinsic operation will always produce the same result, so only
    // needs to be monitored once. Attach a stub to load the resulting constant
    // directly.

    TypeScript::Monitor(cx, script, pc, res);

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid()) {
        return true;
    }

    TryAttachStub<GetIntrinsicIRGenerator>("GetIntrinsic", cx, frame, stub, BaselineCacheIRStubKind::Regular, res);

    return true;
}

typedef bool (*DoGetIntrinsicFallbackFn)(JSContext*, BaselineFrame*, ICGetIntrinsic_Fallback*,
                                         MutableHandleValue);
static const VMFunction DoGetIntrinsicFallbackInfo =
    FunctionInfo<DoGetIntrinsicFallbackFn>(DoGetIntrinsicFallback, "DoGetIntrinsicFallback",
                                           TailCall);

bool
ICGetIntrinsic_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    EmitRestoreTailCallReg(masm);

    masm.push(ICStubReg);
    pushStubPayload(masm, R0.scratchReg());

    return tailCallVM(DoGetIntrinsicFallbackInfo, masm);
}

//
// GetProp_Fallback
//

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
        if (op == JSOP_GETBOUNDNAME) {
            RootedObject env(cx, &val.toObject());
            RootedId id(cx, NameToId(name));
            if (!GetNameBoundInEnvironment(cx, env, id, res)) {
                return false;
            }
        } else {
            MOZ_ASSERT(op == JSOP_GETPROP || op == JSOP_CALLPROP || op == JSOP_LENGTH);
            if (!GetProperty(cx, val, name, res)) {
                return false;
            }
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
    stub->incrementEnteredCount();

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub_->icEntry()->pc(script);
    JSOp op = JSOp(*pc);
    FallbackICSpew(cx, stub, "GetProp(%s)", CodeName[op]);

    MOZ_ASSERT(op == JSOP_GETPROP ||
               op == JSOP_CALLPROP ||
               op == JSOP_LENGTH ||
               op == JSOP_GETBOUNDNAME);

    RootedPropertyName name(cx, script->getName(pc));

    // There are some reasons we can fail to attach a stub that are temporary.
    // We want to avoid calling noteUnoptimizableAccess() if the reason we
    // failed to attach a stub is one of those temporary reasons, since we might
    // end up attaching a stub for the exact same access later.
    bool isTemporarilyUnoptimizable = false;

    if (stub->state().maybeTransition()) {
        stub->discardStubs(cx);
    }

    bool attached = false;
    if (stub->state().canAttachStub()) {
        RootedValue idVal(cx, StringValue(name));
        GetPropIRGenerator gen(cx, script, pc, CacheKind::GetProp, stub->state().mode(),
                               &isTemporarilyUnoptimizable, val, idVal, val,
                               GetPropertyResultFlags::All);
        if (gen.tryAttachStub()) {
            ICStub* newStub = AttachBaselineCacheIRStub(cx, gen.writerRef(), gen.cacheKind(),
                                                        BaselineCacheIRStubKind::Monitored,
                                                        script, stub, &attached);
            if (newStub) {
                JitSpew(JitSpew_BaselineIC, "  Attached GetProp CacheIR stub");
                if (gen.shouldNotePreliminaryObjectStub()) {
                    newStub->toCacheIR_Monitored()->notePreliminaryObject();
                } else if (gen.shouldUnlinkPreliminaryObjectStubs()) {
                    StripPreliminaryObjectStubs(cx, stub);
                }
            }
        }
        if (!attached && !isTemporarilyUnoptimizable) {
            stub->state().trackNotAttached();
        }
    }

    if (!ComputeGetPropResult(cx, frame, op, name, val, res)) {
        return false;
    }

    StackTypeSet* types = TypeScript::BytecodeTypes(script, pc);
    TypeScript::Monitor(cx, script, pc, types, res);

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid()) {
        return true;
    }

    // Add a type monitor stub for the resulting value.
    if (!stub->addMonitorStubForValue(cx, frame, types, res)) {
        return false;
    }
    return true;
}

static bool
DoGetPropSuperFallback(JSContext* cx, BaselineFrame* frame, ICGetProp_Fallback* stub_,
                       HandleValue receiver, MutableHandleValue val, MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICGetProp_Fallback*> stub(frame, stub_);
    stub->incrementEnteredCount();

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub_->icEntry()->pc(script);
    FallbackICSpew(cx, stub, "GetPropSuper(%s)", CodeName[JSOp(*pc)]);

    MOZ_ASSERT(JSOp(*pc) == JSOP_GETPROP_SUPER);

    RootedPropertyName name(cx, script->getName(pc));

    // There are some reasons we can fail to attach a stub that are temporary.
    // We want to avoid calling noteUnoptimizableAccess() if the reason we
    // failed to attach a stub is one of those temporary reasons, since we might
    // end up attaching a stub for the exact same access later.
    bool isTemporarilyUnoptimizable = false;

    if (stub->state().maybeTransition()) {
        stub->discardStubs(cx);
    }

    bool attached = false;
    if (stub->state().canAttachStub()) {
        RootedValue idVal(cx, StringValue(name));
        GetPropIRGenerator gen(cx, script, pc, CacheKind::GetPropSuper, stub->state().mode(),
                               &isTemporarilyUnoptimizable, val, idVal, receiver,
                               GetPropertyResultFlags::All);
        if (gen.tryAttachStub()) {
            ICStub* newStub = AttachBaselineCacheIRStub(cx, gen.writerRef(), gen.cacheKind(),
                                                        BaselineCacheIRStubKind::Monitored,
                                                        script, stub, &attached);
            if (newStub) {
                JitSpew(JitSpew_BaselineIC, "  Attached GetPropSuper CacheIR stub");
                if (gen.shouldNotePreliminaryObjectStub()) {
                    newStub->toCacheIR_Monitored()->notePreliminaryObject();
                } else if (gen.shouldUnlinkPreliminaryObjectStubs()) {
                    StripPreliminaryObjectStubs(cx, stub);
                }
            }
        }
        if (!attached && !isTemporarilyUnoptimizable) {
            stub->state().trackNotAttached();
        }
    }

    // |val| is [[HomeObject]].[[Prototype]] which must be Object
    RootedObject valObj(cx, &val.toObject());
    if (!GetProperty(cx, valObj, receiver, name, res)) {
        return false;
    }

    StackTypeSet* types = TypeScript::BytecodeTypes(script, pc);
    TypeScript::Monitor(cx, script, pc, types, res);

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid()) {
        return true;
    }

    // Add a type monitor stub for the resulting value.
    if (!stub->addMonitorStubForValue(cx, frame, types, res)) {
        return false;
    }

    return true;
}

typedef bool (*DoGetPropFallbackFn)(JSContext*, BaselineFrame*, ICGetProp_Fallback*,
                                    MutableHandleValue, MutableHandleValue);
static const VMFunction DoGetPropFallbackInfo =
    FunctionInfo<DoGetPropFallbackFn>(DoGetPropFallback, "DoGetPropFallback", TailCall,
                                      PopValues(1));

typedef bool (*DoGetPropSuperFallbackFn)(JSContext*, BaselineFrame*, ICGetProp_Fallback*,
                                         HandleValue, MutableHandleValue, MutableHandleValue);
static const VMFunction DoGetPropSuperFallbackInfo =
    FunctionInfo<DoGetPropSuperFallbackFn>(DoGetPropSuperFallback, "DoGetPropSuperFallback",
                                           TailCall);

bool
ICGetProp_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(R0 == JSReturnOperand);

    EmitRestoreTailCallReg(masm);

    // Super property getters use a |this| that differs from base object
    if (hasReceiver_) {
        // Push arguments.
        masm.pushValue(R0);
        masm.pushValue(R1);
        masm.push(ICStubReg);
        masm.pushBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

        if (!tailCallVM(DoGetPropSuperFallbackInfo, masm)) {
            return false;
        }
    } else {
        // Ensure stack is fully synced for the expression decompiler.
        masm.pushValue(R0);

        // Push arguments.
        masm.pushValue(R0);
        masm.push(ICStubReg);
        masm.pushBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

        if (!tailCallVM(DoGetPropFallbackInfo, masm)) {
            return false;
        }
    }

    // This is the resume point used when bailout rewrites call stack to undo
    // Ion inlined frames. The return address pushed onto reconstructed stack
    // will point here.
    assumeStubFrame();
    bailoutReturnOffset_.bind(masm.currentOffset());

    leaveStubFrame(masm, true);

    // When we get here, ICStubReg contains the ICGetProp_Fallback stub,
    // which we can't use to enter the TypeMonitor IC, because it's a MonitoredFallbackStub
    // instead of a MonitoredStub. So, we cheat. Note that we must have a
    // non-null fallbackMonitorStub here because InitFromBailout delazifies.
    masm.loadPtr(Address(ICStubReg, ICMonitoredFallbackStub::offsetOfFallbackMonitorStub()),
                 ICStubReg);
    EmitEnterTypeMonitorIC(masm, ICTypeMonitor_Fallback::offsetOfFirstMonitorStub());

    return true;
}

void
ICGetProp_Fallback::Compiler::postGenerateStubCode(MacroAssembler& masm, Handle<JitCode*> code)
{
    BailoutReturnStub kind = hasReceiver_ ? BailoutReturnStub::GetPropSuper
                                            : BailoutReturnStub::GetProp;
    void* address = code->raw() + bailoutReturnOffset_.offset();
    cx->realm()->jitRealm()->initBailoutReturnAddr(address, getKey(), kind);
}

//
// SetProp_Fallback
//

static bool
DoSetPropFallback(JSContext* cx, BaselineFrame* frame, ICSetProp_Fallback* stub_, Value* stack,
                  HandleValue lhs, HandleValue rhs)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICSetProp_Fallback*> stub(frame, stub_);
    stub->incrementEnteredCount();

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    JSOp op = JSOp(*pc);
    FallbackICSpew(cx, stub, "SetProp(%s)", CodeName[op]);

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
               op == JSOP_INITALIASEDLEXICAL ||
               op == JSOP_INITGLEXICAL);

    RootedPropertyName name(cx);
    if (op == JSOP_SETALIASEDVAR || op == JSOP_INITALIASEDLEXICAL) {
        name = EnvironmentCoordinateName(cx->caches().envCoordinateNameCache, script, pc);
    } else {
        name = script->getName(pc);
    }
    RootedId id(cx, NameToId(name));

    RootedObject obj(cx, ToObjectFromStackForPropertyAccess(cx, lhs, id));
    if (!obj) {
        return false;
    }
    RootedShape oldShape(cx, obj->maybeShape());
    RootedObjectGroup oldGroup(cx, JSObject::getGroup(cx, obj));
    if (!oldGroup) {
        return false;
    }

    if (obj->is<UnboxedPlainObject>()) {
        MOZ_ASSERT(!oldShape);
        if (UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando()) {
            oldShape = expando->lastProperty();
        }
    }

    // There are some reasons we can fail to attach a stub that are temporary.
    // We want to avoid calling noteUnoptimizableAccess() if the reason we
    // failed to attach a stub is one of those temporary reasons, since we might
    // end up attaching a stub for the exact same access later.
    bool isTemporarilyUnoptimizable = false;

    bool attached = false;
    if (stub->state().maybeTransition()) {
        stub->discardStubs(cx);
    }

    if (stub->state().canAttachStub()) {
        RootedValue idVal(cx, StringValue(name));
        SetPropIRGenerator gen(cx, script, pc, CacheKind::SetProp, stub->state().mode(),
                               &isTemporarilyUnoptimizable, lhs, idVal, rhs);
        if (gen.tryAttachStub()) {
            ICStub* newStub = AttachBaselineCacheIRStub(cx, gen.writerRef(), gen.cacheKind(),
                                                        BaselineCacheIRStubKind::Updated,
                                                        frame->script(), stub, &attached);
            if (newStub) {
                JitSpew(JitSpew_BaselineIC, "  Attached SetProp CacheIR stub");

                SetUpdateStubData(newStub->toCacheIR_Updated(), gen.typeCheckInfo());

                if (gen.shouldNotePreliminaryObjectStub()) {
                    newStub->toCacheIR_Updated()->notePreliminaryObject();
                } else if (gen.shouldUnlinkPreliminaryObjectStubs()) {
                    StripPreliminaryObjectStubs(cx, stub);
                }
            }
        }
    }

    if (op == JSOP_INITPROP ||
        op == JSOP_INITLOCKEDPROP ||
        op == JSOP_INITHIDDENPROP)
    {
        if (!InitPropertyOperation(cx, op, obj, name, rhs)) {
            return false;
        }
    } else if (op == JSOP_SETNAME ||
               op == JSOP_STRICTSETNAME ||
               op == JSOP_SETGNAME ||
               op == JSOP_STRICTSETGNAME)
    {
        if (!SetNameOperation(cx, script, pc, obj, rhs)) {
            return false;
        }
    } else if (op == JSOP_SETALIASEDVAR || op == JSOP_INITALIASEDLEXICAL) {
        obj->as<EnvironmentObject>().setAliasedBinding(cx, EnvironmentCoordinate(pc), name, rhs);
    } else if (op == JSOP_INITGLEXICAL) {
        RootedValue v(cx, rhs);
        LexicalEnvironmentObject* lexicalEnv;
        if (script->hasNonSyntacticScope()) {
            lexicalEnv = &NearestEnclosingExtensibleLexicalEnvironment(frame->environmentChain());
        } else {
            lexicalEnv = &cx->global()->lexicalEnvironment();
        }
        InitGlobalLexicalOperation(cx, lexicalEnv, script, pc, v);
    } else {
        MOZ_ASSERT(op == JSOP_SETPROP || op == JSOP_STRICTSETPROP);

        ObjectOpResult result;
        if (!SetProperty(cx, obj, id, rhs, lhs, result) ||
            !result.checkStrictErrorOrWarning(cx, obj, id, op == JSOP_STRICTSETPROP))
        {
            return false;
        }
    }

    // Overwrite the LHS on the stack (pushed for the decompiler) with the RHS.
    MOZ_ASSERT(stack[1] == lhs);
    stack[1] = rhs;

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid()) {
        return true;
    }

    if (attached) {
        return true;
    }

    // The SetProperty call might have entered this IC recursively, so try
    // to transition.
    if (stub->state().maybeTransition()) {
        stub->discardStubs(cx);
    }

    if (stub->state().canAttachStub()) {
        RootedValue idVal(cx, StringValue(name));
        SetPropIRGenerator gen(cx, script, pc, CacheKind::SetProp, stub->state().mode(),
                               &isTemporarilyUnoptimizable, lhs, idVal, rhs);
        if (gen.tryAttachAddSlotStub(oldGroup, oldShape)) {
            ICStub* newStub = AttachBaselineCacheIRStub(cx, gen.writerRef(), gen.cacheKind(),
                                                        BaselineCacheIRStubKind::Updated,
                                                        frame->script(), stub, &attached);
            if (newStub) {
                JitSpew(JitSpew_BaselineIC, "  Attached SetProp CacheIR stub");

                SetUpdateStubData(newStub->toCacheIR_Updated(), gen.typeCheckInfo());

                if (gen.shouldNotePreliminaryObjectStub()) {
                    newStub->toCacheIR_Updated()->notePreliminaryObject();
                } else if (gen.shouldUnlinkPreliminaryObjectStubs()) {
                    StripPreliminaryObjectStubs(cx, stub);
                }
            }
        } else {
            gen.trackAttached(IRGenerator::NotAttached);
        }
        if (!attached && !isTemporarilyUnoptimizable) {
            stub->state().trackNotAttached();
        }
    }

    return true;
}

typedef bool (*DoSetPropFallbackFn)(JSContext*, BaselineFrame*, ICSetProp_Fallback*, Value*,
                                    HandleValue, HandleValue);
static const VMFunction DoSetPropFallbackInfo =
    FunctionInfo<DoSetPropFallbackFn>(DoSetPropFallback, "DoSetPropFallback", TailCall,
                                      PopValues(1));

bool
ICSetProp_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(R0 == JSReturnOperand);

    EmitRestoreTailCallReg(masm);

    // Ensure stack is fully synced for the expression decompiler.
    // Overwrite the RHS value on top of the stack with the object, then push
    // the RHS in R1 on top of that.
    masm.storeValue(R0, Address(masm.getStackPointer(), 0));
    masm.pushValue(R1);

    // Push arguments.
    masm.pushValue(R1);
    masm.pushValue(R0);

    // Push pointer to stack values, so that the stub can overwrite the object
    // (pushed for the decompiler) with the RHS.
    masm.computeEffectiveAddress(Address(masm.getStackPointer(), 2 * sizeof(Value)),
                                 R0.scratchReg());
    masm.push(R0.scratchReg());

    masm.push(ICStubReg);
    pushStubPayload(masm, R0.scratchReg());

    if (!tailCallVM(DoSetPropFallbackInfo, masm)) {
        return false;
    }

    // This is the resume point used when bailout rewrites call stack to undo
    // Ion inlined frames. The return address pushed onto reconstructed stack
    // will point here.
    assumeStubFrame();
    bailoutReturnOffset_.bind(masm.currentOffset());

    leaveStubFrame(masm, true);
    EmitReturnFromIC(masm);

    return true;
}

void
ICSetProp_Fallback::Compiler::postGenerateStubCode(MacroAssembler& masm, Handle<JitCode*> code)
{
    BailoutReturnStub kind = BailoutReturnStub::SetProp;
    void* address = code->raw() + bailoutReturnOffset_.offset();
    cx->realm()->jitRealm()->initBailoutReturnAddr(address, getKey(), kind);
}

//
// Call_Fallback
//

static bool
TryAttachFunApplyStub(JSContext* cx, ICCall_Fallback* stub, HandleScript script, jsbytecode* pc,
                      HandleValue thisv, uint32_t argc, Value* argv,
                      ICTypeMonitor_Fallback* typeMonitorFallback, bool* attached)
{
    if (argc != 2) {
        return true;
    }

    if (!thisv.isObject() || !thisv.toObject().is<JSFunction>()) {
        return true;
    }
    RootedFunction target(cx, &thisv.toObject().as<JSFunction>());

    // right now, only handle situation where second argument is |arguments|
    if (argv[1].isMagic(JS_OPTIMIZED_ARGUMENTS) && !script->needsArgsObj()) {
        if (target->hasJitEntry() && !stub->hasStub(ICStub::Call_ScriptedApplyArguments)) {
            JitSpew(JitSpew_BaselineIC, "  Generating Call_ScriptedApplyArguments stub");

            ICCall_ScriptedApplyArguments::Compiler compiler(
                cx, typeMonitorFallback->firstMonitorStub(), script->pcToOffset(pc));
            ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
            if (!newStub) {
                return false;
            }

            stub->addNewStub(newStub);
            *attached = true;
            return true;
        }

        // TODO: handle FUNAPPLY for native targets.
    }

    if (argv[1].isObject() && argv[1].toObject().is<ArrayObject>()) {
        if (target->hasJitEntry() && !stub->hasStub(ICStub::Call_ScriptedApplyArray)) {
            JitSpew(JitSpew_BaselineIC, "  Generating Call_ScriptedApplyArray stub");

            ICCall_ScriptedApplyArray::Compiler compiler(
                cx, typeMonitorFallback->firstMonitorStub(), script->pcToOffset(pc));
            ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
            if (!newStub) {
                return false;
            }

            stub->addNewStub(newStub);
            *attached = true;
            return true;
        }
    }
    return true;
}

static bool
TryAttachFunCallStub(JSContext* cx, ICCall_Fallback* stub, HandleScript script, jsbytecode* pc,
                     HandleValue thisv, ICTypeMonitor_Fallback* typeMonitorFallback,
                     bool* attached)
{
    // Try to attach a stub for Function.prototype.call with scripted |this|.

    *attached = false;
    if (!thisv.isObject() || !thisv.toObject().is<JSFunction>()) {
        return true;
    }
    RootedFunction target(cx, &thisv.toObject().as<JSFunction>());

    // Attach a stub if the script can be Baseline-compiled. We do this also
    // if the script is not yet compiled to avoid attaching a CallNative stub
    // that handles everything, even after the callee becomes hot.
    if (((target->hasScript() && target->nonLazyScript()->canBaselineCompile()) ||
        (target->isNativeWithJitEntry())) &&
        !stub->hasStub(ICStub::Call_ScriptedFunCall))
    {
        JitSpew(JitSpew_BaselineIC, "  Generating Call_ScriptedFunCall stub");

        ICCall_ScriptedFunCall::Compiler compiler(cx, typeMonitorFallback->firstMonitorStub(),
                                                  script->pcToOffset(pc));
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub) {
            return false;
        }

        *attached = true;
        stub->addNewStub(newStub);
        return true;
    }

    return true;
}

static bool
GetTemplateObjectForNative(JSContext* cx, HandleFunction target, const CallArgs& args,
                           MutableHandleObject res, bool* skipAttach)
{
    Native native = target->native();

    // Check for natives to which template objects can be attached. This is
    // done to provide templates to Ion for inlining these natives later on.

    if (native == ArrayConstructor || native == array_construct) {
        // Note: the template array won't be used if its length is inaccurately
        // computed here.  (We allocate here because compilation may occur on a
        // separate thread where allocation is impossible.)
        size_t count = 0;
        if (args.length() != 1) {
            count = args.length();
        } else if (args.length() == 1 && args[0].isInt32() && args[0].toInt32() >= 0) {
            count = args[0].toInt32();
        }

        if (count <= ArrayObject::EagerAllocationMaxLength) {
            ObjectGroup* group = ObjectGroup::callingAllocationSiteGroup(cx, JSProto_Array);
            if (!group) {
                return false;
            }
            if (group->maybePreliminaryObjectsDontCheckGeneration()) {
                *skipAttach = true;
                return true;
            }

            // With this and other array templates, analyze the group so that
            // we don't end up with a template whose structure might change later.
            res.set(NewFullyAllocatedArrayForCallingAllocationSite(cx, count, TenuredObject));
            return !!res;
        }
    }

    if (args.length() == 1) {
        size_t len = 0;

        if (args[0].isInt32() && args[0].toInt32() >= 0) {
            len = args[0].toInt32();
        }

        if (!TypedArrayObject::GetTemplateObjectForNative(cx, native, len, res)) {
            return false;
        }
        if (res) {
            return true;
        }
    }

    if (native == js::array_slice) {
        if (args.thisv().isObject()) {
            RootedObject obj(cx, &args.thisv().toObject());
            if (!obj->isSingleton()) {
                if (obj->group()->maybePreliminaryObjectsDontCheckGeneration()) {
                    *skipAttach = true;
                    return true;
                }
                res.set(NewFullyAllocatedArrayTryReuseGroup(cx, obj, 0, TenuredObject));
                return !!res;
            }
        }
    }

    if (native == StringConstructor) {
        RootedString emptyString(cx, cx->runtime()->emptyString);
        res.set(StringObject::create(cx, emptyString, /* proto = */ nullptr, TenuredObject));
        return !!res;
    }

    if (native == obj_create && args.length() == 1 && args[0].isObjectOrNull()) {
        RootedObject proto(cx, args[0].toObjectOrNull());
        res.set(ObjectCreateImpl(cx, proto, TenuredObject));
        return !!res;
    }

    if (native == js::intrinsic_NewArrayIterator) {
        res.set(NewArrayIteratorObject(cx, TenuredObject));
        return !!res;
    }

    if (native == js::intrinsic_NewStringIterator) {
        res.set(NewStringIteratorObject(cx, TenuredObject));
        return !!res;
    }

    return true;
}

static bool
GetTemplateObjectForClassHook(JSContext* cx, JSNative hook, CallArgs& args,
                              MutableHandleObject templateObject)
{
    if (args.callee().nonCCWRealm() != cx->realm()) {
        return true;
    }

    if (hook == TypedObject::construct) {
        Rooted<TypeDescr*> descr(cx, &args.callee().as<TypeDescr>());
        templateObject.set(TypedObject::createZeroed(cx, descr, gc::TenuredHeap));
        return !!templateObject;
    }

    return true;
}

static bool
IsOptimizableConstStringSplit(Realm* callerRealm, const Value& callee, int argc, Value* args)
{
    if (argc != 2 || !args[0].isString() || !args[1].isString()) {
        return false;
    }

    if (!args[0].toString()->isAtom() || !args[1].toString()->isAtom()) {
        return false;
    }

    if (!callee.isObject() || !callee.toObject().is<JSFunction>()) {
        return false;
    }

    JSFunction& calleeFun = callee.toObject().as<JSFunction>();
    if (calleeFun.realm() != callerRealm) {
        return false;
    }
    if (!calleeFun.isNative() || calleeFun.native() != js::intrinsic_StringSplitString) {
        return false;
    }

    return true;
}

static bool
TryAttachCallStub(JSContext* cx, ICCall_Fallback* stub, HandleScript script, jsbytecode* pc,
                  JSOp op, uint32_t argc, Value* vp, bool constructing, bool isSpread,
                  bool createSingleton, bool* handled)
{
    bool isSuper = op == JSOP_SUPERCALL || op == JSOP_SPREADSUPERCALL;

    if (createSingleton || op == JSOP_EVAL || op == JSOP_STRICTEVAL) {
        return true;
    }

    if (stub->numOptimizedStubs() >= ICCall_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard all stubs in this IC and replace with inert megamorphic stub.
        // But for now we just bail.
        return true;
    }

    RootedValue callee(cx, vp[0]);
    RootedValue thisv(cx, vp[1]);

    // Don't attach an optimized call stub if we could potentially attach an
    // optimized ConstStringSplit stub.
    if (stub->numOptimizedStubs() == 0 &&
        IsOptimizableConstStringSplit(cx->realm(), callee, argc, vp + 2))
    {
        return true;
    }

    stub->unlinkStubsWithKind(cx, ICStub::Call_ConstStringSplit);

    if (!callee.isObject()) {
        return true;
    }

    ICTypeMonitor_Fallback* typeMonitorFallback = stub->getFallbackMonitorStub(cx, script);
    if (!typeMonitorFallback) {
        return false;
    }

    RootedObject obj(cx, &callee.toObject());
    if (!obj->is<JSFunction>()) {
        // Try to attach a stub for a call/construct hook on the object.
        if (JSNative hook = constructing ? obj->constructHook() : obj->callHook()) {
            if (op != JSOP_FUNAPPLY && !isSpread && !createSingleton) {
                RootedObject templateObject(cx);
                CallArgs args = CallArgsFromVp(argc, vp);
                if (!GetTemplateObjectForClassHook(cx, hook, args, &templateObject)) {
                    return false;
                }

                JitSpew(JitSpew_BaselineIC, "  Generating Call_ClassHook stub");
                ICCall_ClassHook::Compiler compiler(cx, typeMonitorFallback->firstMonitorStub(),
                                                    obj->getClass(), hook, templateObject,
                                                    script->pcToOffset(pc), constructing);
                ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
                if (!newStub) {
                    return false;
                }

                stub->addNewStub(newStub);
                *handled = true;
                return true;
            }
        }
        return true;
    }

    RootedFunction fun(cx, &obj->as<JSFunction>());

    bool nativeWithJitEntry = fun->isNativeWithJitEntry();
    if (fun->isInterpreted() || nativeWithJitEntry) {
        // Never attach optimized scripted call stubs for JSOP_FUNAPPLY.
        // MagicArguments may escape the frame through them.
        if (op == JSOP_FUNAPPLY) {
            return true;
        }

        // If callee is not an interpreted constructor, we have to throw.
        if (constructing && !fun->isConstructor()) {
            return true;
        }

        // Likewise, if the callee is a class constructor, we have to throw.
        if (!constructing && fun->isClassConstructor()) {
            return true;
        }

        if (!fun->hasJitEntry()) {
            // Don't treat this as an unoptimizable case, as we'll add a stub
            // when the callee is delazified.
            *handled = true;
            return true;
        }

        // If we're constructing, require the callee to have JIT code. This
        // isn't required for correctness but avoids allocating a template
        // object below for constructors that aren't hot. See bug 1419758.
        if (constructing && !fun->hasJITCode()) {
            *handled = true;
            return true;
        }

        // Check if this stub chain has already generalized scripted calls.
        if (stub->scriptedStubsAreGeneralized()) {
            JitSpew(JitSpew_BaselineIC, "  Chain already has generalized scripted call stub!");
            return true;
        }

        if (stub->state().mode() == ICState::Mode::Megamorphic) {
            // Create a Call_AnyScripted stub.
            JitSpew(JitSpew_BaselineIC, "  Generating Call_AnyScripted stub (cons=%s, spread=%s)",
                    constructing ? "yes" : "no", isSpread ? "yes" : "no");
            ICCallScriptedCompiler compiler(cx, typeMonitorFallback->firstMonitorStub(),
                                            constructing, isSpread, script->pcToOffset(pc));
            ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
            if (!newStub) {
                return false;
            }

            // Before adding new stub, unlink all previous Call_Scripted.
            stub->unlinkStubsWithKind(cx, ICStub::Call_Scripted);

            // Add new generalized stub.
            stub->addNewStub(newStub);
            *handled = true;
            return true;
        }

        // Keep track of the function's |prototype| property in type
        // information, for use during Ion compilation.
        if (IsIonEnabled(cx)) {
            EnsureTrackPropertyTypes(cx, fun, NameToId(cx->names().prototype));
        }

        // Remember the template object associated with any script being called
        // as a constructor, for later use during Ion compilation. This is unsound
        // for super(), as a single callsite can have multiple possible prototype object
        // created (via different newTargets)
        RootedObject templateObject(cx);
        if (constructing && !isSuper) {
            // If we are calling a constructor for which the new script
            // properties analysis has not been performed yet, don't attach a
            // stub. After the analysis is performed, CreateThisForFunction may
            // start returning objects with a different type, and the Ion
            // compiler will get confused.

            // Only attach a stub if the function already has a prototype and
            // we can look it up without causing side effects.
            RootedObject newTarget(cx, &vp[2 + argc].toObject());
            RootedValue protov(cx);
            if (!GetPropertyPure(cx, newTarget, NameToId(cx->names().prototype), protov.address())) {
                JitSpew(JitSpew_BaselineIC, "  Can't purely lookup function prototype");
                return true;
            }

            if (protov.isObject()) {
                TaggedProto proto(&protov.toObject());
                ObjectGroup* group = ObjectGroup::defaultNewGroup(cx, nullptr, proto, newTarget);
                if (!group) {
                    return false;
                }

                AutoSweepObjectGroup sweep(group);
                if (group->newScript(sweep) && !group->newScript(sweep)->analyzed()) {
                    JitSpew(JitSpew_BaselineIC, "  Function newScript has not been analyzed");

                    // This is temporary until the analysis is perfomed, so
                    // don't treat this as unoptimizable.
                    *handled = true;
                    return true;
                }
            }

            if (cx->realm() == fun->realm()) {
                JSObject* thisObject = CreateThisForFunction(cx, fun, newTarget, TenuredObject);
                if (!thisObject) {
                    return false;
                }

                if (thisObject->is<PlainObject>() || thisObject->is<UnboxedPlainObject>()) {
                    templateObject = thisObject;
                }
            }
        }

        if (nativeWithJitEntry) {
            JitSpew(JitSpew_BaselineIC,
                    "  Generating Call_Scripted stub (native=%p with jit entry, cons=%s, spread=%s)",
                    fun->native(), constructing ? "yes" : "no", isSpread ? "yes" : "no");
        } else {
            JitSpew(JitSpew_BaselineIC,
                    "  Generating Call_Scripted stub (fun=%p, %s:%u:%u, cons=%s, spread=%s)",
                    fun.get(), fun->nonLazyScript()->filename(), fun->nonLazyScript()->lineno(),
                    fun->nonLazyScript()->column(), constructing ? "yes" : "no", isSpread ? "yes" : "no");
        }

        bool isCrossRealm = cx->realm() != fun->realm();
        ICCallScriptedCompiler compiler(cx, typeMonitorFallback->firstMonitorStub(),
                                        fun, templateObject,
                                        constructing, isSpread, isCrossRealm,
                                        script->pcToOffset(pc));
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub) {
            return false;
        }

        stub->addNewStub(newStub);
        *handled = true;
        return true;
    }

    if (fun->isNative() && (!constructing || (constructing && fun->isConstructor()))) {
        // Generalized native call stubs are not here yet!
        MOZ_ASSERT(!stub->nativeStubsAreGeneralized());

        // Check for JSOP_FUNAPPLY
        if (op == JSOP_FUNAPPLY) {
            if (fun->native() == fun_apply) {
                return TryAttachFunApplyStub(cx, stub, script, pc, thisv, argc, vp + 2,
                                             typeMonitorFallback, handled);
            }

            // Don't try to attach a "regular" optimized call stubs for FUNAPPLY ops,
            // since MagicArguments may escape through them.
            return true;
        }

        if (op == JSOP_FUNCALL && fun->native() == fun_call) {
            if (!TryAttachFunCallStub(cx, stub, script, pc, thisv, typeMonitorFallback, handled)) {
                return false;
            }
            if (*handled) {
                return true;
            }
        }

        if (stub->state().mode() == ICState::Mode::Megamorphic) {
            JitSpew(JitSpew_BaselineIC,
                    "  Megamorphic Call_Native stubs. TODO: add Call_AnyNative!");
            return true;
        }

        if (fun->native() == intrinsic_IsSuspendedGenerator) {
            // This intrinsic only appears in self-hosted code.
            MOZ_ASSERT(op != JSOP_NEW);
            MOZ_ASSERT(argc == 1);
            JitSpew(JitSpew_BaselineIC, "  Generating Call_IsSuspendedGenerator stub");

            ICCall_IsSuspendedGenerator::Compiler compiler(cx);
            ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
            if (!newStub) {
                return false;
            }

            stub->addNewStub(newStub);
            *handled = true;
            return true;
        }

        bool isCrossRealm = cx->realm() != fun->realm();

        RootedObject templateObject(cx);
        if (MOZ_LIKELY(!isSpread && !isSuper && !isCrossRealm)) {
            bool skipAttach = false;
            CallArgs args = CallArgsFromVp(argc, vp);
            if (!GetTemplateObjectForNative(cx, fun, args, &templateObject, &skipAttach)) {
                return false;
            }
            if (skipAttach) {
                *handled = true;
                return true;
            }
            MOZ_ASSERT_IF(templateObject,
                          !templateObject->group()->maybePreliminaryObjectsDontCheckGeneration());
        }

        bool ignoresReturnValue = op == JSOP_CALL_IGNORES_RV &&
                                  fun->isNative() &&
                                  fun->hasJitInfo() &&
                                  fun->jitInfo()->type() == JSJitInfo::IgnoresReturnValueNative;

        JitSpew(JitSpew_BaselineIC, "  Generating Call_Native stub (fun=%p, cons=%s, spread=%s)",
                fun.get(), constructing ? "yes" : "no", isSpread ? "yes" : "no");
        ICCall_Native::Compiler compiler(cx, typeMonitorFallback->firstMonitorStub(),
                                         fun, templateObject, constructing, ignoresReturnValue,
                                         isSpread, isCrossRealm, script->pcToOffset(pc));
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub) {
            return false;
        }

        stub->addNewStub(newStub);
        *handled = true;
        return true;
    }

    return true;
}

static bool
CopyStringSplitArray(JSContext* cx, HandleArrayObject arr, MutableHandleValue result)
{
    MOZ_ASSERT(arr->isTenured(), "ConstStringSplit needs a tenured template object");

    uint32_t length = arr->getDenseInitializedLength();
    MOZ_ASSERT(length == arr->length(), "template object is a fully initialized array");

    ArrayObject* nobj = NewFullyAllocatedArrayTryReuseGroup(cx, arr, length);
    if (!nobj) {
        return false;
    }
    nobj->initDenseElements(arr, 0, length);

    result.setObject(*nobj);
    return true;
}

static bool
TryAttachConstStringSplit(JSContext* cx, ICCall_Fallback* stub, HandleScript script,
                          uint32_t argc, HandleValue callee, Value* vp, jsbytecode* pc,
                          HandleValue res, bool* attached)
{
    if (stub->numOptimizedStubs() != 0) {
        return true;
    }

    Value* args = vp + 2;

    if (!IsOptimizableConstStringSplit(cx->realm(), callee, argc, args)) {
        return true;
    }

    RootedString str(cx, args[0].toString());
    RootedString sep(cx, args[1].toString());
    RootedArrayObject obj(cx, &res.toObject().as<ArrayObject>());
    uint32_t initLength = obj->getDenseInitializedLength();
    MOZ_ASSERT(initLength == obj->length(), "string-split result is a fully initialized array");

    // Copy the array before storing in stub.
    RootedArrayObject arrObj(cx);
    arrObj = NewFullyAllocatedArrayTryReuseGroup(cx, obj, initLength, TenuredObject);
    if (!arrObj) {
        return false;
    }
    arrObj->ensureDenseInitializedLength(cx, 0, initLength);

    // Atomize all elements of the array.
    if (initLength > 0) {
        // Mimic NewFullyAllocatedStringArray() and directly inform TI about
        // the element type.
        AddTypePropertyId(cx, arrObj, JSID_VOID, TypeSet::StringType());

        for (uint32_t i = 0; i < initLength; i++) {
            JSAtom* str = js::AtomizeString(cx, obj->getDenseElement(i).toString());
            if (!str) {
                return false;
            }

            arrObj->initDenseElement(i, StringValue(str));
        }
    }

    ICTypeMonitor_Fallback* typeMonitorFallback = stub->getFallbackMonitorStub(cx, script);
    if (!typeMonitorFallback) {
        return false;
    }

    ICCall_ConstStringSplit::Compiler compiler(cx, typeMonitorFallback->firstMonitorStub(),
                                               script->pcToOffset(pc), str, sep, arrObj);
    ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
    if (!newStub) {
        return false;
    }

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
    stub->incrementEnteredCount();

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    JSOp op = JSOp(*pc);
    FallbackICSpew(cx, stub, "Call(%s)", CodeName[op]);

    MOZ_ASSERT(argc == GET_ARGC(pc));
    bool constructing = (op == JSOP_NEW || op == JSOP_SUPERCALL);
    bool ignoresReturnValue = (op == JSOP_CALL_IGNORES_RV);

    // Ensure vp array is rooted - we may GC in here.
    size_t numValues = argc + 2 + constructing;
    AutoArrayRooter vpRoot(cx, numValues, vp);

    CallArgs callArgs = CallArgsFromSp(argc + constructing, vp + numValues, constructing,
                                       ignoresReturnValue);
    RootedValue callee(cx, vp[0]);

    // Handle funapply with JSOP_ARGUMENTS
    if (op == JSOP_FUNAPPLY && argc == 2 && callArgs[1].isMagic(JS_OPTIMIZED_ARGUMENTS)) {
        if (!GuardFunApplyArgumentsOptimization(cx, frame, callArgs)) {
            return false;
        }
    }

    // Transition stub state to megamorphic or generic if warranted.
    if (stub->state().maybeTransition()) {
        stub->discardStubs(cx);
    }

    bool canAttachStub = stub->state().canAttachStub();
    bool handled = false;

    // Only bother to try optimizing JSOP_CALL with CacheIR if the chain is still
    // allowed to attach stubs.
    if (canAttachStub) {
        CallIRGenerator gen(cx, script, pc, op, stub->state().mode(), argc,
                            callee, callArgs.thisv(),
                            HandleValueArray::fromMarkedLocation(argc, vp+2));
        if (gen.tryAttachStub()) {
            ICStub* newStub = AttachBaselineCacheIRStub(cx, gen.writerRef(), gen.cacheKind(),
                                                        gen.cacheIRStubKind(), script,
                                                        stub, &handled);

            if (newStub) {
                JitSpew(JitSpew_BaselineIC, "  Attached Call CacheIR stub");

                // If it's an updated stub, initialize it.
                if (gen.cacheIRStubKind() == BaselineCacheIRStubKind::Updated) {
                    SetUpdateStubData(newStub->toCacheIR_Updated(), gen.typeCheckInfo());
                }
            }
        }

        // Try attaching a regular call stub, but only if the CacheIR attempt didn't add
        // any stubs.
        if (!handled) {
            bool createSingleton = ObjectGroup::useSingletonForNewObject(cx, script, pc);
            if (!TryAttachCallStub(cx, stub, script, pc, op, argc, vp, constructing, false,
                                   createSingleton, &handled))
            {
                return false;
            }
        }
    }

    if (constructing) {
        if (!ConstructFromStack(cx, callArgs)) {
            return false;
        }
        res.set(callArgs.rval());
    } else if ((op == JSOP_EVAL || op == JSOP_STRICTEVAL) && cx->global()->valueIsEval(callee)) {
        if (!DirectEval(cx, callArgs.get(0), res)) {
            return false;
        }
    } else {
        MOZ_ASSERT(op == JSOP_CALL ||
                   op == JSOP_CALL_IGNORES_RV ||
                   op == JSOP_CALLITER ||
                   op == JSOP_FUNCALL ||
                   op == JSOP_FUNAPPLY ||
                   op == JSOP_EVAL ||
                   op == JSOP_STRICTEVAL);
        if (op == JSOP_CALLITER && callee.isPrimitive()) {
            MOZ_ASSERT(argc == 0, "thisv must be on top of the stack");
            ReportValueError(cx, JSMSG_NOT_ITERABLE, -1, callArgs.thisv(), nullptr);
            return false;
        }

        if (!CallFromStack(cx, callArgs)) {
            return false;
        }

        res.set(callArgs.rval());
    }

    StackTypeSet* types = TypeScript::BytecodeTypes(script, pc);
    TypeScript::Monitor(cx, script, pc, types, res);

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid()) {
        return true;
    }

    // Add a type monitor stub for the resulting value.
    if (!stub->addMonitorStubForValue(cx, frame, types, res)) {
        return false;
    }

    // Try to transition again in case we called this IC recursively.
    if (stub->state().maybeTransition()) {
        stub->discardStubs(cx);
    }
    canAttachStub = stub->state().canAttachStub();

    if (!handled && canAttachStub && !constructing) {
        // If 'callee' is a potential Call_ConstStringSplit, try to attach an
        // optimized ConstStringSplit stub. Note that vp[0] now holds the return value
        // instead of the callee, so we pass the callee as well.
        if (!TryAttachConstStringSplit(cx, stub, script, argc, callee, vp, pc, res, &handled)) {
            return false;
        }
    }

    if (!handled) {
        if (canAttachStub) {
            stub->state().trackNotAttached();
        }
    }
    return true;
}

static bool
DoSpreadCallFallback(JSContext* cx, BaselineFrame* frame, ICCall_Fallback* stub_, Value* vp,
                     MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICCall_Fallback*> stub(frame, stub_);
    stub->incrementEnteredCount();

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    JSOp op = JSOp(*pc);
    bool constructing = (op == JSOP_SPREADNEW || op == JSOP_SPREADSUPERCALL);
    FallbackICSpew(cx, stub, "SpreadCall(%s)", CodeName[op]);

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

    if (!SpreadCallOperation(cx, script, pc, thisv, callee, arr, newTarget, res)) {
        return false;
    }

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid()) {
        return true;
    }

    // Add a type monitor stub for the resulting value.
    StackTypeSet* types = TypeScript::BytecodeTypes(script, pc);
    if (!stub->addMonitorStubForValue(cx, frame, types, res)) {
        return false;
    }

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
        if (isConstructing) {
            masm.add32(Imm32(1), count);
        }
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
    if (isConstructing) {
        masm.pushValue(Address(BaselineFrameReg, STUB_FRAME_SIZE));
    }

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

Register
ICCallStubCompiler::guardFunApply(MacroAssembler& masm, AllocatableGeneralRegisterSet regs,
                                  Register argcReg, FunApplyThing applyThing,
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

        // Limit the length to something reasonable.
        masm.branch32(Assembler::Above,
                      Address(BaselineFrameReg, BaselineFrame::offsetOfNumActualArgs()),
                      Imm32(ICCall_ScriptedApplyArray::MAX_ARGS_ARRAY_LENGTH),
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

        masm.branchTestObjClass(Assembler::NotEqual, secondArgObj, &ArrayObject::class_,
                                regsx.getAny(), secondArgObj, failure);

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
        static_assert(sizeof(Value) == 8, "shift by 3 below assumes Value is 8 bytes");
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

    masm.branchTestObjClass(Assembler::NotEqual, callee, &JSFunction::class_, regs.getAny(),
                            callee, failure);
    masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrEnv()), callee);

    masm.branchPtr(Assembler::NotEqual, callee, ImmPtr(fun_apply), failure);

    // Load the |thisv|, ensure that it's a scripted function with a valid baseline or ion
    // script, or a native function.
    Address thisSlot(masm.getStackPointer(), ICStackValueOffset + (2 * sizeof(Value)));
    masm.loadValue(thisSlot, val);

    masm.branchTestObject(Assembler::NotEqual, val, failure);
    Register target = masm.extractObject(val, ExtractTemp1);
    regs.add(val);
    regs.takeUnchecked(target);

    masm.branchTestObjClass(Assembler::NotEqual, target, &JSFunction::class_, regs.getAny(),
                            target, failure);

    Register temp = regs.takeAny();
    masm.branchIfFunctionHasNoJitEntry(target, /* constructing */ false, failure);
    masm.branchFunctionKind(Assembler::Equal, JSFunction::ClassConstructor, callee, temp, failure);
    regs.add(temp);
    return target;
}

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
    masm.unboxObject(arrayVal, startReg);
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
static const VMFunction DoCallFallbackInfo =
    FunctionInfo<DoCallFallbackFn>(DoCallFallback, "DoCallFallback");

typedef bool (*DoSpreadCallFallbackFn)(JSContext*, BaselineFrame*, ICCall_Fallback*,
                                       Value*, MutableHandleValue);
static const VMFunction DoSpreadCallFallbackInfo =
    FunctionInfo<DoSpreadCallFallbackFn>(DoSpreadCallFallback, "DoSpreadCallFallback");

bool
ICCall_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
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
        if (isConstructing_) {
            masm.pushValue(Address(BaselineFrameReg, STUB_FRAME_SIZE));
        }

        // array
        uint32_t valueOffset = isConstructing_;
        masm.pushValue(Address(BaselineFrameReg, valueOffset++ * sizeof(Value) + STUB_FRAME_SIZE));

        // this
        masm.pushValue(Address(BaselineFrameReg, valueOffset++ * sizeof(Value) + STUB_FRAME_SIZE));

        // callee
        masm.pushValue(Address(BaselineFrameReg, valueOffset++ * sizeof(Value) + STUB_FRAME_SIZE));

        masm.push(masm.getStackPointer());
        masm.push(ICStubReg);

        PushStubPayload(masm, R0.scratchReg());

        if (!callVM(DoSpreadCallFallbackInfo, masm)) {
            return false;
        }

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

    PushStubPayload(masm, R0.scratchReg());

    if (!callVM(DoCallFallbackInfo, masm)) {
        return false;
    }

    leaveStubFrame(masm);
    EmitReturnFromIC(masm);

    // This is the resume point used when bailout rewrites call stack to undo
    // Ion inlined frames. The return address pushed onto reconstructed stack
    // will point here.
    assumeStubFrame();
    bailoutReturnOffset_.bind(masm.currentOffset());

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
    // EmitEnterTypeMonitorIC with a custom struct offset. Note that we must
    // have a non-null fallbackMonitorStub here because InitFromBailout
    // delazifies.
    masm.loadPtr(Address(ICStubReg, ICMonitoredFallbackStub::offsetOfFallbackMonitorStub()),
                 ICStubReg);
    EmitEnterTypeMonitorIC(masm, ICTypeMonitor_Fallback::offsetOfFirstMonitorStub());

    return true;
}

void
ICCall_Fallback::Compiler::postGenerateStubCode(MacroAssembler& masm, Handle<JitCode*> code)
{
    if (MOZ_UNLIKELY(isSpread_)) {
        return;
    }

    void* address = code->raw() + bailoutReturnOffset_.offset();
    BailoutReturnStub kind = isConstructing_ ? BailoutReturnStub::New
                                             : BailoutReturnStub::Call;
    cx->realm()->jitRealm()->initBailoutReturnAddr(address, getKey(), kind);
}

typedef bool (*CreateThisFn)(JSContext* cx, HandleObject callee, HandleObject newTarget,
                             MutableHandleValue rval);
static const VMFunction CreateThisInfoBaseline =
    FunctionInfo<CreateThisFn>(CreateThis, "CreateThis");

bool
ICCallScriptedCompiler::generateStubCode(MacroAssembler& masm)
{
    Label failure;
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));
    bool canUseTailCallReg = regs.has(ICTailCallReg);

    Register argcReg = R0.scratchReg();
    regs.take(argcReg);
    regs.takeUnchecked(ICTailCallReg);

    if (isSpread_) {
        guardSpreadCall(masm, argcReg, &failure, isConstructing_);
    }

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
        masm.branchIfFunctionHasNoJitEntry(callee, isConstructing_, &failure);
    } else {
        // Ensure the object is a function.
        masm.branchTestObjClass(Assembler::NotEqual, callee, &JSFunction::class_, regs.getAny(),
                                callee, &failure);
        if (isConstructing_) {
            masm.branchIfNotInterpretedConstructor(callee, regs.getAny(), &failure);
        } else {
            masm.branchIfFunctionHasNoJitEntry(callee, /* constructing */ false, &failure);
            masm.branchFunctionKind(Assembler::Equal, JSFunction::ClassConstructor, callee,
                                    regs.getAny(), &failure);
        }
    }

    // Load the start of the target JitCode.
    Register code;
    if (!isConstructing_) {
        code = regs.takeAny();
        masm.loadJitCodeRaw(callee, code);
    }

    // We no longer need R1.
    regs.add(R1);

    // Push a stub frame so that we can perform a non-tail call.
    enterStubFrame(masm, regs.getAny());
    if (canUseTailCallReg) {
        regs.add(ICTailCallReg);
    }

    if (maybeCrossRealm_) {
        masm.switchToObjectRealm(callee, regs.getAny());
    }

    if (isConstructing_) {
        // Save argc before call.
        masm.push(argcReg);

        // Stack now looks like:
        //      [..., Callee, ThisV, Arg0V, ..., ArgNV, NewTarget, StubFrameHeader, ArgC ]
        masm.loadValue(Address(masm.getStackPointer(), STUB_FRAME_SIZE + sizeof(size_t)), R1);
        masm.push(masm.extractObject(R1, ExtractTemp0));

        if (isSpread_) {
            masm.loadValue(Address(masm.getStackPointer(),
                                   3 * sizeof(Value) + STUB_FRAME_SIZE + sizeof(size_t) +
                                   sizeof(JSObject*)),
                                   R1);
        } else {
            BaseValueIndex calleeSlot2(masm.getStackPointer(), argcReg,
                                       2 * sizeof(Value) + STUB_FRAME_SIZE + sizeof(size_t) +
                                       sizeof(JSObject*));
            masm.loadValue(calleeSlot2, R1);
        }
        masm.push(masm.extractObject(R1, ExtractTemp0));
        if (!callVM(CreateThisInfoBaseline, masm)) {
            return false;
        }

        // Return of CreateThis must be an object or uninitialized.
#ifdef DEBUG
        Label createdThisOK;
        masm.branchTestObject(Assembler::Equal, JSReturnOperand, &createdThisOK);
        masm.branchTestMagic(Assembler::Equal, JSReturnOperand, &createdThisOK);
        masm.assumeUnreachable("The return of CreateThis must be an object or uninitialized.");
        masm.bind(&createdThisOK);
#endif

        // Reset the register set from here on in.
        static_assert(JSReturnOperand == R0, "The code below needs to be adapted.");
        regs = availableGeneralRegs(0);
        regs.take(R0);
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
        // have destroyed the callee BaselineScript and IonScript. CreateThis
        // is safely repeatable though, so in this case we just leave the stub
        // frame and jump to the next stub.

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

        code = regs.takeAny();
        masm.loadJitCodeRaw(callee, code);

        // Release callee register, but don't add ExtractTemp0 back into the pool
        // ExtractTemp0 is used later, and if it's allocated to some other register at that
        // point, it will get clobbered when used.
        if (callee != ExtractTemp0) {
            regs.add(callee);
        }

        if (canUseTailCallReg) {
            regs.addUnchecked(ICTailCallReg);
        }
    }
    Register scratch = regs.takeAny();

    // Values are on the stack left-to-right. Calling convention wants them
    // right-to-left so duplicate them on the stack in reverse order.
    // |this| and callee are pushed last.
    if (isSpread_) {
        pushSpreadCallArguments(masm, regs, argcReg, /* isJitCall = */ true, isConstructing_);
    } else {
        pushCallArguments(masm, regs, argcReg, /* isJitCall = */ true, isConstructing_);
    }

    // The callee is on top of the stack. Pop and unbox it.
    ValueOperand val = regs.takeAnyValue();
    masm.popValue(val);
    callee = masm.extractObject(val, ExtractTemp0);

    EmitBaselineCreateStubFrameDescriptor(masm, scratch, JitFrameLayout::Size());

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
        TrampolinePtr argumentsRectifier = cx->runtime()->jitRuntime()->getArgumentsRectifier();
        masm.movePtr(argumentsRectifier, code);
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

    if (maybeCrossRealm_) {
        masm.switchToBaselineFrameRealm(R1.scratchReg());
    }

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

typedef bool (*CopyStringSplitArrayFn)(JSContext*, HandleArrayObject, MutableHandleValue);
static const VMFunction CopyStringSplitArrayInfo =
    FunctionInfo<CopyStringSplitArrayFn>(CopyStringSplitArray, "CopyStringSplitArray");

bool
ICCall_ConstStringSplit::Compiler::generateStubCode(MacroAssembler& masm)
{
    // Stack Layout: [ ..., CalleeVal, ThisVal, strVal, sepVal, +ICStackValueOffset+ ]
    static const size_t SEP_DEPTH = 0;
    static const size_t STR_DEPTH = sizeof(Value);
    static const size_t CALLEE_DEPTH = 3 * sizeof(Value);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));
    Label failureRestoreArgc;
#ifdef DEBUG
    Label twoArg;
    Register argcReg = R0.scratchReg();
    masm.branch32(Assembler::Equal, argcReg, Imm32(2), &twoArg);
    masm.assumeUnreachable("Expected argc == 2");
    masm.bind(&twoArg);
#endif
    Register scratchReg = regs.takeAny();

    // Guard that callee is native function js::intrinsic_StringSplitString.
    {
        Address calleeAddr(masm.getStackPointer(), ICStackValueOffset + CALLEE_DEPTH);
        ValueOperand calleeVal = regs.takeAnyValue();

        // Ensure that callee is an object.
        masm.loadValue(calleeAddr, calleeVal);
        masm.branchTestObject(Assembler::NotEqual, calleeVal, &failureRestoreArgc);

        // Ensure that callee is a function.
        Register calleeObj = masm.extractObject(calleeVal, ExtractTemp0);
        masm.branchTestObjClass(Assembler::NotEqual, calleeObj, &JSFunction::class_, scratchReg,
                                calleeObj, &failureRestoreArgc);

        // Ensure that callee's function impl is the native intrinsic_StringSplitString.
        masm.loadPtr(Address(calleeObj, JSFunction::offsetOfNativeOrEnv()), scratchReg);
        masm.branchPtr(Assembler::NotEqual, scratchReg, ImmPtr(js::intrinsic_StringSplitString),
                       &failureRestoreArgc);

        regs.add(calleeVal);
    }

    // Guard sep.
    {
        // Ensure that sep is a string.
        Address sepAddr(masm.getStackPointer(), ICStackValueOffset + SEP_DEPTH);
        ValueOperand sepVal = regs.takeAnyValue();

        masm.loadValue(sepAddr, sepVal);
        masm.branchTestString(Assembler::NotEqual, sepVal, &failureRestoreArgc);

        Register sep = sepVal.scratchReg();
        masm.unboxString(sepVal, sep);
        masm.branchPtr(Assembler::NotEqual, Address(ICStubReg, offsetOfExpectedSep()),
                       sep, &failureRestoreArgc);
        regs.add(sepVal);
    }

    // Guard str.
    {
        // Ensure that str is a string.
        Address strAddr(masm.getStackPointer(), ICStackValueOffset + STR_DEPTH);
        ValueOperand strVal = regs.takeAnyValue();

        masm.loadValue(strAddr, strVal);
        masm.branchTestString(Assembler::NotEqual, strVal, &failureRestoreArgc);

        Register str = strVal.scratchReg();
        masm.unboxString(strVal, str);
        masm.branchPtr(Assembler::NotEqual, Address(ICStubReg, offsetOfExpectedStr()),
                       str, &failureRestoreArgc);
        regs.add(strVal);
    }

    // Main stub body.
    {
        Register paramReg = regs.takeAny();

        // Push arguments.
        enterStubFrame(masm, scratchReg);
        masm.loadPtr(Address(ICStubReg, offsetOfTemplateObject()), paramReg);
        masm.push(paramReg);

        if (!callVM(CopyStringSplitArrayInfo, masm)) {
            return false;
        }
        leaveStubFrame(masm);
        regs.add(paramReg);
    }

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    // Guard failure path.
    masm.bind(&failureRestoreArgc);
    masm.move32(Imm32(2), R0.scratchReg());
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICCall_IsSuspendedGenerator::Compiler::generateStubCode(MacroAssembler& masm)
{
    // The IsSuspendedGenerator intrinsic is only called in self-hosted code,
    // so it's safe to assume we have a single argument and the callee is our
    // intrinsic.

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

    // Check if it's a GeneratorObject.
    Register scratch = regs.takeAny();
    masm.branchTestObjClass(Assembler::NotEqual, genObj, &GeneratorObject::class_, scratch,
                            genObj, &returnFalse);

    // If the resumeIndex slot holds an int32 value < RESUME_INDEX_CLOSING,
    // the generator is suspended.
    masm.loadValue(Address(genObj, GeneratorObject::offsetOfResumeIndexSlot()), argVal);
    masm.branchTestInt32(Assembler::NotEqual, argVal, &returnFalse);
    masm.unboxInt32(argVal, scratch);
    masm.branch32(Assembler::AboveOrEqual, scratch,
                  Imm32(GeneratorObject::RESUME_INDEX_CLOSING),
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
    Label failure;
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));

    Register argcReg = R0.scratchReg();
    regs.take(argcReg);
    regs.takeUnchecked(ICTailCallReg);

    if (isSpread_) {
        guardSpreadCall(masm, argcReg, &failure, isConstructing_);
    }

    // Load the callee in R1.
    if (isSpread_) {
        unsigned skipToCallee = (2 + isConstructing_) * sizeof(Value);
        masm.loadValue(Address(masm.getStackPointer(), skipToCallee + ICStackValueOffset), R1);
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

    if (isCrossRealm_) {
        masm.switchToObjectRealm(callee, regs.getAny());
    }

    // Values are on the stack left-to-right. Calling convention wants them
    // right-to-left so duplicate them on the stack in reverse order.
    // |this| and callee are pushed last.
    if (isSpread_) {
        pushSpreadCallArguments(masm, regs, argcReg, /* isJitCall = */ false, isConstructing_);
    } else {
        pushCallArguments(masm, regs, argcReg, /* isJitCall = */ false, isConstructing_);
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
    EmitBaselineCreateStubFrameDescriptor(masm, scratch, ExitFrameLayout::Size());
    masm.push(scratch);
    masm.push(ICTailCallReg);
    masm.loadJSContext(scratch);
    masm.enterFakeExitFrameForNative(scratch, scratch, isConstructing_);

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
    if (ignoresReturnValue_) {
        MOZ_ASSERT(callee_->hasJitInfo());
        masm.loadPtr(Address(callee, JSFunction::offsetOfJitInfo()), callee);
        masm.callWithABI(Address(callee, JSJitInfo::offsetOfIgnoresReturnValueNative()));
    } else {
        masm.callWithABI(Address(callee, JSFunction::offsetOfNative()));
    }
#endif

    // Test for failure.
    masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

    // Load the return value into R0.
    masm.loadValue(Address(masm.getStackPointer(), NativeExitFrameLayout::offsetOfResult()), R0);

    leaveStubFrame(masm);

    if (isCrossRealm_) {
        masm.switchToBaselineFrameRealm(R1.scratchReg());
    }

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICCall_ClassHook::Compiler::generateStubCode(MacroAssembler& masm)
{
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
    // We use |Address(ICStubReg, ICCall_ClassHook::offsetOfNative())| below
    // instead of extracting the hook from callee. As a result the callee
    // register is no longer used and we must use spectreRegToZero := ICStubReg
    // instead.
    Register callee = masm.extractObject(R1, ExtractTemp0);
    Register scratch = regs.takeAny();
    masm.branchTestObjClass(Assembler::NotEqual, callee,
                            Address(ICStubReg, ICCall_ClassHook::offsetOfClass()),
                            scratch, ICStubReg, &failure);
    regs.add(R1);
    regs.takeUnchecked(callee);

    // Push a stub frame so that we can perform a non-tail call.
    // Note that this leaves the return address in TailCallReg.
    enterStubFrame(masm, regs.getAny());

    masm.switchToObjectRealm(callee, regs.getAny());

    regs.add(scratch);
    pushCallArguments(masm, regs, argcReg, /* isJitCall = */ false, isConstructing_);
    regs.take(scratch);

    masm.assertStackAlignment(sizeof(Value), 0);

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

    EmitBaselineCreateStubFrameDescriptor(masm, scratch, ExitFrameLayout::Size());
    masm.push(scratch);
    masm.push(ICTailCallReg);
    masm.loadJSContext(scratch);
    masm.enterFakeExitFrameForNative(scratch, scratch, isConstructing_);

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

    masm.switchToBaselineFrameRealm(R1.scratchReg());

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICCall_ScriptedApplyArray::Compiler::generateStubCode(MacroAssembler& masm)
{
    Label failure;
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));

    Register argcReg = R0.scratchReg();
    regs.take(argcReg);
    regs.takeUnchecked(ICTailCallReg);

    //
    // Validate inputs
    //

    Register target = guardFunApply(masm, regs, argcReg, FunApply_Array, &failure);
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
    EmitBaselineCreateStubFrameDescriptor(masm, scratch, JitFrameLayout::Size());

    // Reload argc from length of array.
    masm.unboxObject(arrayVal, argcReg);
    masm.loadPtr(Address(argcReg, NativeObject::offsetOfElements()), argcReg);
    masm.load32(Address(argcReg, ObjectElements::offsetOfInitializedLength()), argcReg);

    masm.Push(argcReg);
    masm.Push(target);
    masm.Push(scratch);

    masm.switchToObjectRealm(target, scratch);

    // Load nargs into scratch for underflow check, and then load jitcode pointer into target.
    masm.load16ZeroExtend(Address(target, JSFunction::offsetOfNargs()), scratch);
    masm.loadJitCodeRaw(target, target);

    // Handle arguments underflow.
    Label noUnderflow;
    masm.branch32(Assembler::AboveOrEqual, argcReg, scratch, &noUnderflow);
    {
        // Call the arguments rectifier.
        TrampolinePtr argumentsRectifier = cx->runtime()->jitRuntime()->getArgumentsRectifier();
        masm.movePtr(argumentsRectifier, target);
    }
    masm.bind(&noUnderflow);
    regs.add(argcReg);

    // Do call.
    masm.callJit(target);
    leaveStubFrame(masm, true);

    masm.switchToBaselineFrameRealm(R1.scratchReg());

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICCall_ScriptedApplyArguments::Compiler::generateStubCode(MacroAssembler& masm)
{
    Label failure;
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));

    Register argcReg = R0.scratchReg();
    regs.take(argcReg);
    regs.takeUnchecked(ICTailCallReg);

    //
    // Validate inputs
    //

    Register target = guardFunApply(masm, regs, argcReg, FunApply_MagicArgs, &failure);
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
    EmitBaselineCreateStubFrameDescriptor(masm, scratch, JitFrameLayout::Size());

    masm.loadPtr(Address(BaselineFrameReg, 0), argcReg);
    masm.loadPtr(Address(argcReg, BaselineFrame::offsetOfNumActualArgs()), argcReg);
    masm.Push(argcReg);
    masm.Push(target);
    masm.Push(scratch);

    masm.switchToObjectRealm(target, scratch);

    // Load nargs into scratch for underflow check, and then load jitcode pointer into target.
    masm.load16ZeroExtend(Address(target, JSFunction::offsetOfNargs()), scratch);
    masm.loadJitCodeRaw(target, target);

    // Handle arguments underflow.
    Label noUnderflow;
    masm.branch32(Assembler::AboveOrEqual, argcReg, scratch, &noUnderflow);
    {
        // Call the arguments rectifier.
        TrampolinePtr argumentsRectifier = cx->runtime()->jitRuntime()->getArgumentsRectifier();
        masm.movePtr(argumentsRectifier, target);
    }
    masm.bind(&noUnderflow);
    regs.add(argcReg);

    // Do call
    masm.callJit(target);
    leaveStubFrame(masm, true);

    masm.switchToBaselineFrameRealm(R1.scratchReg());

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICCall_ScriptedFunCall::Compiler::generateStubCode(MacroAssembler& masm)
{
    Label failure;
    AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));
    bool canUseTailCallReg = regs.has(ICTailCallReg);

    Register argcReg = R0.scratchReg();
    regs.take(argcReg);
    regs.takeUnchecked(ICTailCallReg);

    // Load the callee in R1.
    // Stack Layout: [ ..., CalleeVal, ThisVal, Arg0Val, ..., ArgNVal, +ICStackValueOffset+ ]
    BaseValueIndex calleeSlot(masm.getStackPointer(), argcReg, ICStackValueOffset + sizeof(Value));
    masm.loadValue(calleeSlot, R1);
    regs.take(R1);

    // Ensure callee is fun_call.
    masm.branchTestObject(Assembler::NotEqual, R1, &failure);

    Register callee = masm.extractObject(R1, ExtractTemp0);
    masm.branchTestObjClass(Assembler::NotEqual, callee, &JSFunction::class_, regs.getAny(),
                            callee, &failure);
    masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrEnv()), callee);
    masm.branchPtr(Assembler::NotEqual, callee, ImmPtr(fun_call), &failure);

    // Ensure |this| is a function with a jit entry.
    BaseIndex thisSlot(masm.getStackPointer(), argcReg, TimesEight, ICStackValueOffset);
    masm.loadValue(thisSlot, R1);

    masm.branchTestObject(Assembler::NotEqual, R1, &failure);
    callee = masm.extractObject(R1, ExtractTemp0);

    masm.branchTestObjClass(Assembler::NotEqual, callee, &JSFunction::class_, regs.getAny(),
                            callee, &failure);
    masm.branchIfFunctionHasNoJitEntry(callee, /* constructing */ false, &failure);
    masm.branchFunctionKind(Assembler::Equal, JSFunction::ClassConstructor,
                            callee, regs.getAny(), &failure);

    // Load the start of the target JitCode.
    Register code = regs.takeAny();
    masm.loadJitCodeRaw(callee, code);

    // We no longer need R1.
    regs.add(R1);

    // Push a stub frame so that we can perform a non-tail call.
    enterStubFrame(masm, regs.getAny());
    if (canUseTailCallReg) {
        regs.add(ICTailCallReg);
    }

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
    masm.switchToObjectRealm(callee, scratch);
    EmitBaselineCreateStubFrameDescriptor(masm, scratch, JitFrameLayout::Size());

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
        TrampolinePtr argumentsRectifier = cx->runtime()->jitRuntime()->getArgumentsRectifier();
        masm.movePtr(argumentsRectifier, code);
    }

    masm.bind(&noUnderflow);
    masm.callJit(code);

    leaveStubFrame(masm, true);

    masm.switchToBaselineFrameRealm(R1.scratchReg());

    // Enter type monitor IC to type-check result.
    EmitEnterTypeMonitorIC(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICTableSwitch::Compiler::generateStubCode(MacroAssembler& masm)
{
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
    masm.unboxDouble(R0, FloatReg0);

    // N.B. -0 === 0, so convert -0 to a 0 int32.
    masm.convertDoubleToInt32(FloatReg0, key, &outOfRange, /* negativeZeroCheck = */ false);
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
    if (!code) {
        return nullptr;
    }

    jsbytecode* pc = pc_;
    pc += JUMP_OFFSET_LEN;
    int32_t low = GET_JUMP_OFFSET(pc);
    pc += JUMP_OFFSET_LEN;
    int32_t high = GET_JUMP_OFFSET(pc);
    int32_t length = high - low + 1;
    pc += JUMP_OFFSET_LEN;

    void** table = (void**) space->alloc(sizeof(void*) * length);
    if (!table) {
        ReportOutOfMemory(cx);
        return nullptr;
    }

    jsbytecode* defaultpc = pc_ + GET_JUMP_OFFSET(pc_);

    for (int32_t i = 0; i < length; i++) {
        table[i] = script_->tableSwitchCasePC(pc_, i);
    }

    return newStub<ICTableSwitch>(space, code, table, low, length, defaultpc);
}

void
ICTableSwitch::fixupJumpTable(JSScript* script, BaselineScript* baseline)
{
    PCMappingSlotInfo slotInfo;
    defaultTarget_ = baseline->nativeCodeForPC(script, (jsbytecode*) defaultTarget_, &slotInfo);
    MOZ_ASSERT(slotInfo.isStackSynced());

    for (int32_t i = 0; i < length_; i++) {
        table_[i] = baseline->nativeCodeForPC(script, (jsbytecode*) table_[i], &slotInfo);
        MOZ_ASSERT(slotInfo.isStackSynced());
    }
}

//
// GetIterator_Fallback
//

static bool
DoGetIteratorFallback(JSContext* cx, BaselineFrame* frame, ICGetIterator_Fallback* stub,
                      HandleValue value, MutableHandleValue res)
{
    stub->incrementEnteredCount();
    FallbackICSpew(cx, stub, "GetIterator");

    TryAttachStub<GetIteratorIRGenerator>("GetIterator", cx, frame, stub, BaselineCacheIRStubKind::Regular, value);

    JSObject* iterobj = ValueToIterator(cx, value);
    if (!iterobj) {
        return false;
    }

    res.setObject(*iterobj);
    return true;
}

typedef bool (*DoGetIteratorFallbackFn)(JSContext*, BaselineFrame*, ICGetIterator_Fallback*,
                                        HandleValue, MutableHandleValue);
static const VMFunction DoGetIteratorFallbackInfo =
    FunctionInfo<DoGetIteratorFallbackFn>(DoGetIteratorFallback, "DoGetIteratorFallback",
                                          TailCall, PopValues(1));

bool
ICGetIterator_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    EmitRestoreTailCallReg(masm);

    // Sync stack for the decompiler.
    masm.pushValue(R0);

    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushStubPayload(masm, R0.scratchReg());

    return tailCallVM(DoGetIteratorFallbackInfo, masm);
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
    stub->incrementEnteredCount();

    FallbackICSpew(cx, stub, "IteratorMore");

    if (!IteratorMore(cx, iterObj, res)) {
        return false;
    }

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid()) {
        return true;
    }

    if (!res.isMagic(JS_NO_ITER_VALUE) && !res.isString()) {
        stub->setHasNonStringResult();
    }

    if (iterObj->is<PropertyIteratorObject>() &&
        !stub->hasStub(ICStub::IteratorMore_Native))
    {
        ICIteratorMore_Native::Compiler compiler(cx);
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(frame->script()));
        if (!newStub) {
            return false;
        }
        stub->addNewStub(newStub);
    }

    return true;
}

typedef bool (*DoIteratorMoreFallbackFn)(JSContext*, BaselineFrame*, ICIteratorMore_Fallback*,
                                         HandleObject, MutableHandleValue);
static const VMFunction DoIteratorMoreFallbackInfo =
    FunctionInfo<DoIteratorMoreFallbackFn>(DoIteratorMoreFallback, "DoIteratorMoreFallback",
                                           TailCall);

bool
ICIteratorMore_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    EmitRestoreTailCallReg(masm);

    masm.unboxObject(R0, R0.scratchReg());
    masm.push(R0.scratchReg());
    masm.push(ICStubReg);
    pushStubPayload(masm, R0.scratchReg());

    return tailCallVM(DoIteratorMoreFallbackInfo, masm);
}

//
// IteratorMore_Native
//

bool
ICIteratorMore_Native::Compiler::generateStubCode(MacroAssembler& masm)
{
    Label failure;

    Register obj = masm.extractObject(R0, ExtractTemp0);

    AllocatableGeneralRegisterSet regs(availableGeneralRegs(1));
    Register nativeIterator = regs.takeAny();
    Register scratch = regs.takeAny();

    masm.branchTestObjClass(Assembler::NotEqual, obj, &PropertyIteratorObject::class_, scratch,
                            obj, &failure);
    masm.loadObjPrivate(obj, JSObject::ITER_CLASS_NFIXED_SLOTS, nativeIterator);

    // If propertyCursor_ < propertiesEnd_, load the next string and advance
    // the cursor.  Otherwise return MagicValue(JS_NO_ITER_VALUE).
    Label iterDone;
    Address cursorAddr(nativeIterator, NativeIterator::offsetOfPropertyCursor());
    Address cursorEndAddr(nativeIterator, NativeIterator::offsetOfPropertiesEnd());
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

static void
DoIteratorCloseFallback(JSContext* cx, ICIteratorClose_Fallback* stub, HandleValue iterValue)
{
    FallbackICSpew(cx, stub, "IteratorClose");

    CloseIterator(&iterValue.toObject());
}

typedef void (*DoIteratorCloseFallbackFn)(JSContext*, ICIteratorClose_Fallback*, HandleValue);
static const VMFunction DoIteratorCloseFallbackInfo =
    FunctionInfo<DoIteratorCloseFallbackFn>(DoIteratorCloseFallback, "DoIteratorCloseFallback",
                                            TailCall);

bool
ICIteratorClose_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    EmitRestoreTailCallReg(masm);

    masm.pushValue(R0);
    masm.push(ICStubReg);

    return tailCallVM(DoIteratorCloseFallbackInfo, masm);
}

//
// InstanceOf_Fallback
//

static bool
DoInstanceOfFallback(JSContext* cx, BaselineFrame* frame, ICInstanceOf_Fallback* stub_,
                     HandleValue lhs, HandleValue rhs, MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICInstanceOf_Fallback*> stub(frame, stub_);
    stub->incrementEnteredCount();

    FallbackICSpew(cx, stub, "InstanceOf");

    if (!rhs.isObject()) {
        ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS, -1, rhs, nullptr);
        return false;
    }

    RootedObject obj(cx, &rhs.toObject());
    bool cond = false;
    if (!HasInstance(cx, obj, lhs, &cond)) {
        return false;
    }

    res.setBoolean(cond);

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid()) {
        return true;
    }

    if (!obj->is<JSFunction>()) {
        // ensure we've recorded at least one failure, so we can detect there was a non-optimizable case
        if (!stub->state().hasFailures()) {
            stub->state().trackNotAttached();
        }
        return true;
    }

    // For functions, keep track of the |prototype| property in type information,
    // for use during Ion compilation.
    EnsureTrackPropertyTypes(cx, obj, NameToId(cx->names().prototype));

    TryAttachStub<InstanceOfIRGenerator>("InstanceOf", cx, frame, stub, BaselineCacheIRStubKind::Regular, lhs, obj);
    return true;
}

typedef bool (*DoInstanceOfFallbackFn)(JSContext*, BaselineFrame*, ICInstanceOf_Fallback*,
                                       HandleValue, HandleValue, MutableHandleValue);
static const VMFunction DoInstanceOfFallbackInfo =
    FunctionInfo<DoInstanceOfFallbackFn>(DoInstanceOfFallback, "DoInstanceOfFallback", TailCall,
                                         PopValues(2));

bool
ICInstanceOf_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    EmitRestoreTailCallReg(masm);

    // Sync stack for the decompiler.
    masm.pushValue(R0);
    masm.pushValue(R1);

    masm.pushValue(R1);
    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushStubPayload(masm, R0.scratchReg());

    return tailCallVM(DoInstanceOfFallbackInfo, masm);
}

//
// TypeOf_Fallback
//

static bool
DoTypeOfFallback(JSContext* cx, BaselineFrame* frame, ICTypeOf_Fallback* stub, HandleValue val,
                 MutableHandleValue res)
{
    stub->incrementEnteredCount();
    FallbackICSpew(cx, stub, "TypeOf");

    TryAttachStub<TypeOfIRGenerator>("TypeOf", cx, frame, stub, BaselineCacheIRStubKind::Regular, val);

    JSType type = js::TypeOfValue(val);
    RootedString string(cx, TypeName(type, cx->names()));
    res.setString(string);
    return true;
}

typedef bool (*DoTypeOfFallbackFn)(JSContext*, BaselineFrame* frame, ICTypeOf_Fallback*,
                                   HandleValue, MutableHandleValue);
static const VMFunction DoTypeOfFallbackInfo =
    FunctionInfo<DoTypeOfFallbackFn>(DoTypeOfFallback, "DoTypeOfFallback", TailCall);

bool
ICTypeOf_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    EmitRestoreTailCallReg(masm);

    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushStubPayload(masm, R0.scratchReg());

    return tailCallVM(DoTypeOfFallbackInfo, masm);
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
    if (res) {
        res->native_ = other.native();
    }
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

//
// Rest_Fallback
//

static bool
DoRestFallback(JSContext* cx, BaselineFrame* frame, ICRest_Fallback* stub,
               MutableHandleValue res)
{
    unsigned numFormals = frame->numFormalArgs() - 1;
    unsigned numActuals = frame->numActualArgs();
    unsigned numRest = numActuals > numFormals ? numActuals - numFormals : 0;
    Value* rest = frame->argv() + numFormals;

    ArrayObject* obj = ObjectGroup::newArrayObject(cx, rest, numRest, GenericObject,
                                                   ObjectGroup::NewArrayKind::UnknownIndex);
    if (!obj) {
        return false;
    }
    res.setObject(*obj);
    return true;
}

typedef bool (*DoRestFallbackFn)(JSContext*, BaselineFrame*, ICRest_Fallback*,
                                 MutableHandleValue);
static const VMFunction DoRestFallbackInfo =
    FunctionInfo<DoRestFallbackFn>(DoRestFallback, "DoRestFallback", TailCall);

bool
ICRest_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    EmitRestoreTailCallReg(masm);

    masm.push(ICStubReg);
    pushStubPayload(masm, R0.scratchReg());

    return tailCallVM(DoRestFallbackInfo, masm);
}

//
// UnaryArith_Fallback
//

static bool
DoUnaryArithFallback(JSContext* cx, BaselineFrame* frame, ICUnaryArith_Fallback* stub,
                     HandleValue val, MutableHandleValue res)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICUnaryArith_Fallback*> debug_stub(frame, stub);
    stub->incrementEnteredCount();

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    JSOp op = JSOp(*pc);
    FallbackICSpew(cx, stub, "UnaryArith(%s)", CodeName[op]);

    switch (op) {
      case JSOP_BITNOT: {
        RootedValue valCopy(cx, val);
        if (!BitNot(cx, &valCopy, res)) {
            return false;
        }
        break;
      }
      case JSOP_NEG: {
        // We copy val here because the original value is needed below.
        RootedValue valCopy(cx, val);
        if (!NegOperation(cx, &valCopy, res)) {
            return false;
        }
        break;
      }
      default:
        MOZ_CRASH("Unexpected op");
    }

    // Check if debug mode toggling made the stub invalid.
    if (debug_stub.invalid()) {
        return true;
    }

    if (res.isDouble()) {
        stub->setSawDoubleResult();
    }

    TryAttachStub<UnaryArithIRGenerator>("UniaryArith", cx, frame, stub, BaselineCacheIRStubKind::Regular, op, val, res);
    return true;
}

typedef bool (*DoUnaryArithFallbackFn)(JSContext*, BaselineFrame*, ICUnaryArith_Fallback*,
                                       HandleValue, MutableHandleValue);
static const VMFunction DoUnaryArithFallbackInfo =
    FunctionInfo<DoUnaryArithFallbackFn>(DoUnaryArithFallback, "DoUnaryArithFallback", TailCall,
                                         PopValues(1));

bool
ICUnaryArith_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    // Ensure stack is fully synced for the expression decompiler.
    masm.pushValue(R0);

    // Push arguments.
    masm.pushValue(R0);
    masm.push(ICStubReg);
    pushStubPayload(masm, R0.scratchReg());

    return tailCallVM(DoUnaryArithFallbackInfo, masm);
}

//
// BinaryArith_Fallback
//

static bool
DoBinaryArithFallback(JSContext* cx, BaselineFrame* frame, ICBinaryArith_Fallback* stub_,
                      HandleValue lhs, HandleValue rhs, MutableHandleValue ret)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICBinaryArith_Fallback*> stub(frame, stub_);
    stub->incrementEnteredCount();

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    JSOp op = JSOp(*pc);
    FallbackICSpew(cx, stub, "CacheIRBinaryArith(%s,%d,%d)", CodeName[op],
            int(lhs.isDouble() ? JSVAL_TYPE_DOUBLE : lhs.extractNonDoubleType()),
            int(rhs.isDouble() ? JSVAL_TYPE_DOUBLE : rhs.extractNonDoubleType()));

    // Don't pass lhs/rhs directly, we need the original values when
    // generating stubs.
    RootedValue lhsCopy(cx, lhs);
    RootedValue rhsCopy(cx, rhs);

    // Perform the compare operation.
    switch(op) {
      case JSOP_ADD:
        // Do an add.
        if (!AddValues(cx, &lhsCopy, &rhsCopy, ret)) {
            return false;
        }
        break;
      case JSOP_SUB:
        if (!SubValues(cx, &lhsCopy, &rhsCopy, ret)) {
            return false;
        }
        break;
      case JSOP_MUL:
        if (!MulValues(cx, &lhsCopy, &rhsCopy, ret)) {
            return false;
        }
        break;
      case JSOP_DIV:
        if (!DivValues(cx, &lhsCopy, &rhsCopy, ret)) {
            return false;
        }
        break;
      case JSOP_MOD:
        if (!ModValues(cx, &lhsCopy, &rhsCopy, ret)) {
            return false;
        }
        break;
      case JSOP_POW:
        if (!PowValues(cx, &lhsCopy, &rhsCopy, ret)) {
            return false;
        }
        break;
      case JSOP_BITOR: {
        if (!BitOr(cx, &lhsCopy, &rhsCopy, ret)) {
            return false;
        }
        break;
      }
      case JSOP_BITXOR: {
        if (!BitXor(cx, &lhsCopy, &rhsCopy, ret)) {
            return false;
        }
        break;
      }
      case JSOP_BITAND: {
        if (!BitAnd(cx, &lhsCopy, &rhsCopy, ret)) {
            return false;
        }
        break;
      }
      case JSOP_LSH: {
        if (!BitLsh(cx, &lhsCopy, &rhsCopy, ret)) {
            return false;
        }
        break;
      }
      case JSOP_RSH: {
        if (!BitRsh(cx, &lhsCopy, &rhsCopy, ret)) {
            return false;
        }
        break;
      }
      case JSOP_URSH: {
        if (!UrshOperation(cx, &lhsCopy, &rhsCopy, ret)) {
            return false;
        }
        break;
      }
      default:
        MOZ_CRASH("Unhandled baseline arith op");
    }

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid()) {
        return true;
    }

    if (ret.isDouble()) {
        stub->setSawDoubleResult();
    }

    TryAttachStub<BinaryArithIRGenerator>("BinaryArith", cx, frame, stub, BaselineCacheIRStubKind::Regular, op, lhs, rhs, ret);
    return true;
}

typedef bool (*DoBinaryArithFallbackFn)(JSContext*, BaselineFrame*, ICBinaryArith_Fallback*,
                                        HandleValue, HandleValue, MutableHandleValue);
static const VMFunction DoBinaryArithFallbackInfo =
    FunctionInfo<DoBinaryArithFallbackFn>(DoBinaryArithFallback, "DoBinaryArithFallback",
                                          TailCall, PopValues(2));

bool
ICBinaryArith_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
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
    pushStubPayload(masm, R0.scratchReg());

    return tailCallVM(DoBinaryArithFallbackInfo, masm);
}

//
// Compare_Fallback
//
static bool
DoCompareFallback(JSContext* cx, BaselineFrame* frame, ICCompare_Fallback* stub_, HandleValue lhs,
                  HandleValue rhs, MutableHandleValue ret)
{
    // This fallback stub may trigger debug mode toggling.
    DebugModeOSRVolatileStub<ICCompare_Fallback*> stub(frame, stub_);
    stub->incrementEnteredCount();

    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    JSOp op = JSOp(*pc);

    FallbackICSpew(cx, stub, "Compare(%s)", CodeName[op]);

    // Don't pass lhs/rhs directly, we need the original values when
    // generating stubs.
    RootedValue lhsCopy(cx, lhs);
    RootedValue rhsCopy(cx, rhs);

    // Perform the compare operation.
    bool out;
    switch (op) {
      case JSOP_LT:
        if (!LessThan(cx, &lhsCopy, &rhsCopy, &out)) {
            return false;
        }
        break;
      case JSOP_LE:
        if (!LessThanOrEqual(cx, &lhsCopy, &rhsCopy, &out)) {
            return false;
        }
        break;
      case JSOP_GT:
        if (!GreaterThan(cx, &lhsCopy, &rhsCopy, &out)) {
            return false;
        }
        break;
      case JSOP_GE:
        if (!GreaterThanOrEqual(cx, &lhsCopy, &rhsCopy, &out)) {
            return false;
        }
        break;
      case JSOP_EQ:
        if (!LooselyEqual<true>(cx, &lhsCopy, &rhsCopy, &out)) {
            return false;
        }
        break;
      case JSOP_NE:
        if (!LooselyEqual<false>(cx, &lhsCopy, &rhsCopy, &out)) {
            return false;
        }
        break;
      case JSOP_STRICTEQ:
        if (!StrictlyEqual<true>(cx, &lhsCopy, &rhsCopy, &out)) {
            return false;
        }
        break;
      case JSOP_STRICTNE:
        if (!StrictlyEqual<false>(cx, &lhsCopy, &rhsCopy, &out)) {
            return false;
        }
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unhandled baseline compare op");
        return false;
    }

    ret.setBoolean(out);

    // Check if debug mode toggling made the stub invalid.
    if (stub.invalid()) {
        return true;
    }

    TryAttachStub<CompareIRGenerator>("Compare", cx, frame, stub, BaselineCacheIRStubKind::Regular, op, lhs, rhs);
    return true;
}

typedef bool (*DoCompareFallbackFn)(JSContext*, BaselineFrame*, ICCompare_Fallback*,
                                    HandleValue, HandleValue, MutableHandleValue);
static const VMFunction DoCompareFallbackInfo =
    FunctionInfo<DoCompareFallbackFn>(DoCompareFallback, "DoCompareFallback", TailCall,
                                      PopValues(2));

bool
ICCompare_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
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
    pushStubPayload(masm, R0.scratchReg());
    return tailCallVM(DoCompareFallbackInfo, masm);
}

//
// NewArray_Fallback
//

static bool
DoNewArray(JSContext* cx, BaselineFrame* frame, ICNewArray_Fallback* stub, uint32_t length,
           MutableHandleValue res)
{
    stub->incrementEnteredCount();
    FallbackICSpew(cx, stub, "NewArray");

    RootedObject obj(cx);
    if (stub->templateObject()) {
        RootedObject templateObject(cx, stub->templateObject());
        obj = NewArrayOperationWithTemplate(cx, templateObject);
        if (!obj) {
            return false;
        }
    } else {
        RootedScript script(cx, frame->script());
        jsbytecode* pc = stub->icEntry()->pc(script);

        obj = NewArrayOperation(cx, script, pc, length);
        if (!obj) {
            return false;
        }

        if (!obj->isSingleton() && !obj->group()->maybePreliminaryObjectsDontCheckGeneration()) {
            JSObject* templateObject = NewArrayOperation(cx, script, pc, length, TenuredObject);
            if (!templateObject) {
                return false;
            }
            stub->setTemplateObject(templateObject);
        }
    }

    res.setObject(*obj);
    return true;
}

typedef bool(*DoNewArrayFn)(JSContext*, BaselineFrame*, ICNewArray_Fallback*, uint32_t,
                            MutableHandleValue);
static const VMFunction DoNewArrayInfo =
    FunctionInfo<DoNewArrayFn>(DoNewArray, "DoNewArray", TailCall);

bool
ICNewArray_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    EmitRestoreTailCallReg(masm);

    masm.push(R0.scratchReg()); // length
    masm.push(ICStubReg); // stub.
    masm.pushBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

    return tailCallVM(DoNewArrayInfo, masm);
}

//
// NewObject_Fallback
//
static bool
DoNewObject(JSContext* cx, BaselineFrame* frame, ICNewObject_Fallback* stub, MutableHandleValue res)
{
    stub->incrementEnteredCount();
    FallbackICSpew(cx, stub, "NewObject");

    RootedObject obj(cx);

    RootedObject templateObject(cx, stub->templateObject());
    if (templateObject) {
        MOZ_ASSERT(!templateObject->group()->maybePreliminaryObjectsDontCheckGeneration());
        obj = NewObjectOperationWithTemplate(cx, templateObject);
    } else {
        RootedScript script(cx, frame->script());
        jsbytecode* pc = stub->icEntry()->pc(script);
        obj = NewObjectOperation(cx, script, pc);

        if (obj && !obj->isSingleton() &&
            !obj->group()->maybePreliminaryObjectsDontCheckGeneration())
        {
            templateObject = NewObjectOperation(cx, script, pc, TenuredObject);
            if (!templateObject) {
                return false;
            }

            TryAttachStub<NewObjectIRGenerator>("NewObject", cx, frame, stub, BaselineCacheIRStubKind::Regular, JSOp(*pc), templateObject);

            stub->setTemplateObject(templateObject);
        }
    }

    if (!obj) {
        return false;
    }

    res.setObject(*obj);
    return true;
}

typedef bool(*DoNewObjectFn)(JSContext*, BaselineFrame*, ICNewObject_Fallback*, MutableHandleValue);
static const VMFunction DoNewObjectInfo =
    FunctionInfo<DoNewObjectFn>(DoNewObject, "DoNewObject", TailCall);

bool
ICNewObject_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    EmitRestoreTailCallReg(masm);

    masm.push(ICStubReg); // stub.
    pushStubPayload(masm, R0.scratchReg());

    return tailCallVM(DoNewObjectInfo, masm);
}

} // namespace jit
} // namespace js
