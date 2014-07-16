/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/Ion.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/ThreadLocal.h"

#include "jscompartment.h"
#include "jsprf.h"

#include "gc/Marking.h"
#include "jit/AliasAnalysis.h"
#include "jit/AsmJSModule.h"
#include "jit/BacktrackingAllocator.h"
#include "jit/BaselineDebugModeOSR.h"
#include "jit/BaselineFrame.h"
#include "jit/BaselineInspector.h"
#include "jit/BaselineJIT.h"
#include "jit/CodeGenerator.h"
#include "jit/EdgeCaseAnalysis.h"
#include "jit/EffectiveAddressAnalysis.h"
#include "jit/IonAnalysis.h"
#include "jit/IonBuilder.h"
#include "jit/IonOptimizationLevels.h"
#include "jit/IonSpewer.h"
#include "jit/JitCommon.h"
#include "jit/JitCompartment.h"
#include "jit/LICM.h"
#include "jit/LinearScan.h"
#include "jit/LIR.h"
#include "jit/Lowering.h"
#include "jit/ParallelSafetyAnalysis.h"
#include "jit/PerfSpewer.h"
#include "jit/RangeAnalysis.h"
#include "jit/StupidAllocator.h"
#include "jit/UnreachableCodeElimination.h"
#include "jit/ValueNumbering.h"
#include "vm/ForkJoin.h"
#include "vm/HelperThreads.h"
#include "vm/TraceLogging.h"

#include "jscompartmentinlines.h"
#include "jsgcinlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "jit/ExecutionMode-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::ThreadLocal;

// Assert that JitCode is gc::Cell aligned.
JS_STATIC_ASSERT(sizeof(JitCode) % gc::CellSize == 0);

static ThreadLocal<IonContext*> TlsIonContext;

static IonContext *
CurrentIonContext()
{
    if (!TlsIonContext.initialized())
        return nullptr;
    return TlsIonContext.get();
}

void
jit::SetIonContext(IonContext *ctx)
{
    TlsIonContext.set(ctx);
}

IonContext *
jit::GetIonContext()
{
    MOZ_ASSERT(CurrentIonContext());
    return CurrentIonContext();
}

IonContext *
jit::MaybeGetIonContext()
{
    return CurrentIonContext();
}

IonContext::IonContext(JSContext *cx, TempAllocator *temp)
  : cx(cx),
    temp(temp),
    runtime(CompileRuntime::get(cx->runtime())),
    compartment(CompileCompartment::get(cx->compartment())),
    prev_(CurrentIonContext()),
    assemblerCount_(0)
{
    SetIonContext(this);
}

IonContext::IonContext(ExclusiveContext *cx, TempAllocator *temp)
  : cx(nullptr),
    temp(temp),
    runtime(CompileRuntime::get(cx->runtime_)),
    compartment(nullptr),
    prev_(CurrentIonContext()),
    assemblerCount_(0)
{
    SetIonContext(this);
}

IonContext::IonContext(CompileRuntime *rt, CompileCompartment *comp, TempAllocator *temp)
  : cx(nullptr),
    temp(temp),
    runtime(rt),
    compartment(comp),
    prev_(CurrentIonContext()),
    assemblerCount_(0)
{
    SetIonContext(this);
}

IonContext::IonContext(CompileRuntime *rt)
  : cx(nullptr),
    temp(nullptr),
    runtime(rt),
    compartment(nullptr),
    prev_(CurrentIonContext()),
    assemblerCount_(0)
{
    SetIonContext(this);
}

IonContext::~IonContext()
{
    SetIonContext(prev_);
}

bool
jit::InitializeIon()
{
    if (!TlsIonContext.initialized() && !TlsIonContext.init())
        return false;
    CheckLogging();
    CheckPerf();
    return true;
}

JitRuntime::JitRuntime()
  : execAlloc_(nullptr),
    ionAlloc_(nullptr),
    exceptionTail_(nullptr),
    bailoutTail_(nullptr),
    enterJIT_(nullptr),
    bailoutHandler_(nullptr),
    argumentsRectifier_(nullptr),
    argumentsRectifierReturnAddr_(nullptr),
    parallelArgumentsRectifier_(nullptr),
    invalidator_(nullptr),
    debugTrapHandler_(nullptr),
    forkJoinGetSliceStub_(nullptr),
    baselineDebugModeOSRHandler_(nullptr),
    functionWrappers_(nullptr),
    osrTempData_(nullptr),
    ionCodeProtected_(false),
    ionReturnOverride_(MagicValue(JS_ARG_POISON))
{
}

JitRuntime::~JitRuntime()
{
    js_delete(functionWrappers_);
    freeOsrTempData();

    // Note: The interrupt lock is not taken here, as JitRuntime is only
    // destroyed along with its containing JSRuntime.
    js_delete(ionAlloc_);
}

bool
JitRuntime::initialize(JSContext *cx)
{
    JS_ASSERT(cx->runtime()->currentThreadHasExclusiveAccess());
    JS_ASSERT(cx->runtime()->currentThreadOwnsInterruptLock());

    AutoCompartment ac(cx, cx->atomsCompartment());

    IonContext ictx(cx, nullptr);

    execAlloc_ = cx->runtime()->getExecAlloc(cx);
    if (!execAlloc_)
        return false;

    if (!cx->compartment()->ensureJitCompartmentExists(cx))
        return false;

    functionWrappers_ = cx->new_<VMWrapperMap>(cx);
    if (!functionWrappers_ || !functionWrappers_->init())
        return false;

    IonSpew(IonSpew_Codegen, "# Emitting exception tail stub");
    exceptionTail_ = generateExceptionTailStub(cx);
    if (!exceptionTail_)
        return false;

    IonSpew(IonSpew_Codegen, "# Emitting bailout tail stub");
    bailoutTail_ = generateBailoutTailStub(cx);
    if (!bailoutTail_)
        return false;

    if (cx->runtime()->jitSupportsFloatingPoint) {
        IonSpew(IonSpew_Codegen, "# Emitting bailout tables");

        // Initialize some Ion-only stubs that require floating-point support.
        if (!bailoutTables_.reserve(FrameSizeClass::ClassLimit().classId()))
            return false;

        for (uint32_t id = 0;; id++) {
            FrameSizeClass class_ = FrameSizeClass::FromClass(id);
            if (class_ == FrameSizeClass::ClassLimit())
                break;
            bailoutTables_.infallibleAppend((JitCode *)nullptr);
            bailoutTables_[id] = generateBailoutTable(cx, id);
            if (!bailoutTables_[id])
                return false;
        }

        IonSpew(IonSpew_Codegen, "# Emitting bailout handler");
        bailoutHandler_ = generateBailoutHandler(cx);
        if (!bailoutHandler_)
            return false;

        IonSpew(IonSpew_Codegen, "# Emitting invalidator");
        invalidator_ = generateInvalidator(cx);
        if (!invalidator_)
            return false;
    }

    IonSpew(IonSpew_Codegen, "# Emitting sequential arguments rectifier");
    argumentsRectifier_ = generateArgumentsRectifier(cx, SequentialExecution, &argumentsRectifierReturnAddr_);
    if (!argumentsRectifier_)
        return false;

#ifdef JS_THREADSAFE
    IonSpew(IonSpew_Codegen, "# Emitting parallel arguments rectifier");
    parallelArgumentsRectifier_ = generateArgumentsRectifier(cx, ParallelExecution, nullptr);
    if (!parallelArgumentsRectifier_)
        return false;
#endif

    IonSpew(IonSpew_Codegen, "# Emitting EnterJIT sequence");
    enterJIT_ = generateEnterJIT(cx, EnterJitOptimized);
    if (!enterJIT_)
        return false;

    IonSpew(IonSpew_Codegen, "# Emitting EnterBaselineJIT sequence");
    enterBaselineJIT_ = generateEnterJIT(cx, EnterJitBaseline);
    if (!enterBaselineJIT_)
        return false;

    IonSpew(IonSpew_Codegen, "# Emitting Pre Barrier for Value");
    valuePreBarrier_ = generatePreBarrier(cx, MIRType_Value);
    if (!valuePreBarrier_)
        return false;

    IonSpew(IonSpew_Codegen, "# Emitting Pre Barrier for Shape");
    shapePreBarrier_ = generatePreBarrier(cx, MIRType_Shape);
    if (!shapePreBarrier_)
        return false;

    IonSpew(IonSpew_Codegen, "# Emitting malloc stub");
    mallocStub_ = generateMallocStub(cx);
    if (!mallocStub_)
        return false;

    IonSpew(IonSpew_Codegen, "# Emitting free stub");
    freeStub_ = generateFreeStub(cx);
    if (!freeStub_)
        return false;

    IonSpew(IonSpew_Codegen, "# Emitting VM function wrappers");
    for (VMFunction *fun = VMFunction::functions; fun; fun = fun->next) {
        if (!generateVMWrapper(cx, *fun))
            return false;
    }

    return true;
}

JitCode *
JitRuntime::debugTrapHandler(JSContext *cx)
{
    if (!debugTrapHandler_) {
        // JitRuntime code stubs are shared across compartments and have to
        // be allocated in the atoms compartment.
        AutoLockForExclusiveAccess lock(cx);
        AutoCompartment ac(cx, cx->runtime()->atomsCompartment());
        debugTrapHandler_ = generateDebugTrapHandler(cx);
    }
    return debugTrapHandler_;
}

bool
JitRuntime::ensureForkJoinGetSliceStubExists(JSContext *cx)
{
    if (!forkJoinGetSliceStub_) {
        IonSpew(IonSpew_Codegen, "# Emitting ForkJoinGetSlice stub");
        AutoLockForExclusiveAccess lock(cx);
        AutoCompartment ac(cx, cx->runtime()->atomsCompartment());
        forkJoinGetSliceStub_ = generateForkJoinGetSliceStub(cx);
    }
    return !!forkJoinGetSliceStub_;
}

uint8_t *
JitRuntime::allocateOsrTempData(size_t size)
{
    osrTempData_ = (uint8_t *)js_realloc(osrTempData_, size);
    return osrTempData_;
}

void
JitRuntime::freeOsrTempData()
{
    js_free(osrTempData_);
    osrTempData_ = nullptr;
}

JSC::ExecutableAllocator *
JitRuntime::createIonAlloc(JSContext *cx)
{
    JS_ASSERT(cx->runtime()->currentThreadOwnsInterruptLock());

    ionAlloc_ = js_new<JSC::ExecutableAllocator>();
    if (!ionAlloc_)
        js_ReportOutOfMemory(cx);
    return ionAlloc_;
}

void
JitRuntime::ensureIonCodeProtected(JSRuntime *rt)
{
    JS_ASSERT(rt->currentThreadOwnsInterruptLock());

    if (!rt->signalHandlersInstalled() || ionCodeProtected_ || !ionAlloc_)
        return;

    // Protect all Ion code in the runtime to trigger an access violation the
    // next time any of it runs on the main thread.
    ionAlloc_->toggleAllCodeAsAccessible(false);
    ionCodeProtected_ = true;
}

bool
JitRuntime::handleAccessViolation(JSRuntime *rt, void *faultingAddress)
{
    if (!rt->signalHandlersInstalled() || !ionAlloc_ || !ionAlloc_->codeContains((char *) faultingAddress))
        return false;

#ifdef JS_THREADSAFE
    // All places where the interrupt lock is taken must either ensure that Ion
    // code memory won't be accessed within, or call ensureIonCodeAccessible to
    // render the memory safe for accessing. Otherwise taking the lock below
    // will deadlock the process.
    JS_ASSERT(!rt->currentThreadOwnsInterruptLock());
#endif

    // Taking this lock is necessary to prevent the interrupting thread from marking
    // the memory as inaccessible while we are patching backedges. This will cause us
    // to SEGV while still inside the signal handler, and the process will terminate.
    JSRuntime::AutoLockForInterrupt lock(rt);

    // Ion code in the runtime faulted after it was made inaccessible. Reset
    // the code privileges and patch all loop backedges to perform an interrupt
    // check instead.
    ensureIonCodeAccessible(rt);
    return true;
}

void
JitRuntime::ensureIonCodeAccessible(JSRuntime *rt)
{
    JS_ASSERT(rt->currentThreadOwnsInterruptLock());

    // This can only be called on the main thread and while handling signals,
    // which happens on a separate thread in OS X.
#ifndef XP_MACOSX
    JS_ASSERT(CurrentThreadCanAccessRuntime(rt));
#endif

    if (ionCodeProtected_) {
        ionAlloc_->toggleAllCodeAsAccessible(true);
        ionCodeProtected_ = false;
    }

    if (rt->interrupt) {
        // The interrupt handler needs to be invoked by this thread, but we may
        // be inside a signal handler and have no idea what is above us on the
        // stack (probably we are executing Ion code at an arbitrary point, but
        // we could be elsewhere, say repatching a jump for an IonCache).
        // Patch all backedges in the runtime so they will invoke the interrupt
        // handler the next time they execute.
        patchIonBackedges(rt, BackedgeInterruptCheck);
    }
}

void
JitRuntime::patchIonBackedges(JSRuntime *rt, BackedgeTarget target)
{
#ifndef XP_MACOSX
    JS_ASSERT(CurrentThreadCanAccessRuntime(rt));
#endif

    // Patch all loop backedges in Ion code so that they either jump to the
    // normal loop header or to an interrupt handler each time they run.
    for (InlineListIterator<PatchableBackedge> iter(backedgeList_.begin());
         iter != backedgeList_.end();
         iter++)
    {
        PatchableBackedge *patchableBackedge = *iter;
        PatchJump(patchableBackedge->backedge, target == BackedgeLoopHeader
                                               ? patchableBackedge->loopHeader
                                               : patchableBackedge->interruptCheck);
    }
}

void
jit::RequestInterruptForIonCode(JSRuntime *rt, JSRuntime::InterruptMode mode)
{
    JitRuntime *jitRuntime = rt->jitRuntime();
    if (!jitRuntime)
        return;

    JS_ASSERT(rt->currentThreadOwnsInterruptLock());

    // The mechanism for interrupting normal ion code varies depending on how
    // the interrupt is being requested.
    switch (mode) {
      case JSRuntime::RequestInterruptMainThread:
        // When requesting an interrupt from the main thread, Ion loop
        // backedges can be patched directly. Make sure we don't segv while
        // patching the backedges, to avoid deadlocking inside the signal
        // handler.
        JS_ASSERT(CurrentThreadCanAccessRuntime(rt));
        jitRuntime->ensureIonCodeAccessible(rt);
        break;

      case JSRuntime::RequestInterruptAnyThread:
        // When requesting an interrupt from off the main thread, protect
        // Ion code memory so that the main thread will fault and enter a
        // signal handler when trying to execute the code. The signal
        // handler will unprotect the code and patch loop backedges so
        // that the interrupt handler is invoked afterwards.
        jitRuntime->ensureIonCodeProtected(rt);
        break;

      case JSRuntime::RequestInterruptAnyThreadDontStopIon:
      case JSRuntime::RequestInterruptAnyThreadForkJoin:
        // The caller does not require Ion code to be interrupted.
        // Nothing more needs to be done.
        break;

      default:
        MOZ_ASSUME_UNREACHABLE("Bad interrupt mode");
    }
}

JitCompartment::JitCompartment()
  : stubCodes_(nullptr),
    baselineCallReturnAddr_(nullptr),
    baselineGetPropReturnAddr_(nullptr),
    baselineSetPropReturnAddr_(nullptr),
    stringConcatStub_(nullptr),
    parallelStringConcatStub_(nullptr),
    activeParallelEntryScripts_(nullptr)
{
}

JitCompartment::~JitCompartment()
{
    js_delete(stubCodes_);
    js_delete(activeParallelEntryScripts_);
}

bool
JitCompartment::initialize(JSContext *cx)
{
    stubCodes_ = cx->new_<ICStubCodeMap>(cx);
    if (!stubCodes_ || !stubCodes_->init())
        return false;

    return true;
}

bool
JitCompartment::ensureIonStubsExist(JSContext *cx)
{
    if (!stringConcatStub_) {
        stringConcatStub_.set(generateStringConcatStub(cx, SequentialExecution));
        if (!stringConcatStub_)
            return false;
    }

#ifdef JS_THREADSAFE
    if (!parallelStringConcatStub_) {
        parallelStringConcatStub_.set(generateStringConcatStub(cx, ParallelExecution));
        if (!parallelStringConcatStub_)
            return false;
    }
#endif

    return true;
}

bool
JitCompartment::notifyOfActiveParallelEntryScript(JSContext *cx, HandleScript script)
{
    // Fast path. The isParallelEntryScript bit guarantees that the script is
    // already in the set.
    if (script->parallelIonScript()->isParallelEntryScript()) {
        MOZ_ASSERT(activeParallelEntryScripts_ && activeParallelEntryScripts_->has(script));
        script->parallelIonScript()->resetParallelAge();
        return true;
    }

    if (!activeParallelEntryScripts_) {
        activeParallelEntryScripts_ = cx->new_<ScriptSet>(cx);
        if (!activeParallelEntryScripts_ || !activeParallelEntryScripts_->init())
            return false;
    }

    script->parallelIonScript()->setIsParallelEntryScript();
    ScriptSet::AddPtr p = activeParallelEntryScripts_->lookupForAdd(script);
    return p || activeParallelEntryScripts_->add(p, script);
}

void
jit::FinishOffThreadBuilder(IonBuilder *builder)
{
    ExecutionMode executionMode = builder->info().executionMode();

    // Clear the recompiling flag of the old ionScript, since we continue to
    // use the old ionScript if recompiling fails.
    if (executionMode == SequentialExecution && builder->script()->hasIonScript())
        builder->script()->ionScript()->clearRecompiling();

    // Clean up if compilation did not succeed.
    if (CompilingOffThread(builder->script(), executionMode)) {
        SetIonScript(builder->script(), executionMode,
                     builder->abortReason() == AbortReason_Disable
                     ? ION_DISABLED_SCRIPT
                     : nullptr);
    }

    // The builder is allocated into its LifoAlloc, so destroying that will
    // destroy the builder and all other data accumulated during compilation,
    // except any final codegen (which includes an assembler and needs to be
    // explicitly destroyed).
    js_delete(builder->backgroundCodegen());
    js_delete(builder->alloc().lifoAlloc());
}

static inline void
FinishAllOffThreadCompilations(JSCompartment *comp)
{
#ifdef JS_THREADSAFE
    AutoLockHelperThreadState lock;
    GlobalHelperThreadState::IonBuilderVector &finished = HelperThreadState().ionFinishedList();

    for (size_t i = 0; i < finished.length(); i++) {
        IonBuilder *builder = finished[i];
        if (builder->compartment == CompileCompartment::get(comp)) {
            FinishOffThreadBuilder(builder);
            HelperThreadState().remove(finished, &i);
        }
    }
#endif
}

/* static */ void
JitRuntime::Mark(JSTracer *trc)
{
    JS_ASSERT(!trc->runtime()->isHeapMinorCollecting());
    Zone *zone = trc->runtime()->atomsCompartment()->zone();
    for (gc::ZoneCellIterUnderGC i(zone, gc::FINALIZE_JITCODE); !i.done(); i.next()) {
        JitCode *code = i.get<JitCode>();
        MarkJitCodeRoot(trc, &code, "wrapper");
    }
}

void
JitCompartment::mark(JSTracer *trc, JSCompartment *compartment)
{
    // Cancel any active or pending off thread compilations. Note that the
    // MIR graph does not hold any nursery pointers, so there's no need to
    // do this for minor GCs.
    JS_ASSERT(!trc->runtime()->isHeapMinorCollecting());
    CancelOffThreadIonCompile(compartment, nullptr);
    FinishAllOffThreadCompilations(compartment);

    // Free temporary OSR buffer.
    trc->runtime()->jitRuntime()->freeOsrTempData();

    // Mark scripts with parallel IonScripts if we should preserve them.
    if (activeParallelEntryScripts_) {
        for (ScriptSet::Enum e(*activeParallelEntryScripts_); !e.empty(); e.popFront()) {
            JSScript *script = e.front();

            // If the script has since been invalidated or was attached by an
            // off-thread helper too late (i.e., the ForkJoin finished with
            // warmup doing all the work), remove it.
            if (!script->hasParallelIonScript() ||
                !script->parallelIonScript()->isParallelEntryScript())
            {
                e.removeFront();
                continue;
            }

            // Check and increment the age. If the script is below the max
            // age, mark it.
            //
            // Subtlety: We depend on the tracing of the parallel IonScript's
            // callTargetEntries to propagate the parallel age to the entire
            // call graph.
            if (ShouldPreserveParallelJITCode(trc->runtime(), script, /* increase = */ true)) {
                MarkScript(trc, const_cast<PreBarrieredScript *>(&e.front()), "par-script");
                MOZ_ASSERT(script == e.front());
            }
        }
    }
}

void
JitCompartment::sweep(FreeOp *fop)
{
    stubCodes_->sweep(fop);

    // If the sweep removed the ICCall_Fallback stub, nullptr the baselineCallReturnAddr_ field.
    if (!stubCodes_->lookup(static_cast<uint32_t>(ICStub::Call_Fallback)))
        baselineCallReturnAddr_ = nullptr;
    // Similarly for the ICGetProp_Fallback stub.
    if (!stubCodes_->lookup(static_cast<uint32_t>(ICStub::GetProp_Fallback)))
        baselineGetPropReturnAddr_ = nullptr;
    if (!stubCodes_->lookup(static_cast<uint32_t>(ICStub::SetProp_Fallback)))
        baselineSetPropReturnAddr_ = nullptr;

    if (stringConcatStub_ && !IsJitCodeMarked(stringConcatStub_.unsafeGet()))
        stringConcatStub_.set(nullptr);

    if (parallelStringConcatStub_ && !IsJitCodeMarked(parallelStringConcatStub_.unsafeGet()))
        parallelStringConcatStub_.set(nullptr);

    if (activeParallelEntryScripts_) {
        for (ScriptSet::Enum e(*activeParallelEntryScripts_); !e.empty(); e.popFront()) {
            JSScript *script = e.front();
            if (!IsScriptMarked(&script))
                e.removeFront();
            else
                MOZ_ASSERT(script == e.front());
        }
    }
}

JitCode *
JitRuntime::getBailoutTable(const FrameSizeClass &frameClass) const
{
    JS_ASSERT(frameClass != FrameSizeClass::None());
    return bailoutTables_[frameClass.classId()];
}

JitCode *
JitRuntime::getVMWrapper(const VMFunction &f) const
{
    JS_ASSERT(functionWrappers_);
    JS_ASSERT(functionWrappers_->initialized());
    JitRuntime::VMWrapperMap::Ptr p = functionWrappers_->readonlyThreadsafeLookup(&f);
    JS_ASSERT(p);

    return p->value();
}

template <AllowGC allowGC>
JitCode *
JitCode::New(JSContext *cx, uint8_t *code, uint32_t bufferSize, uint32_t headerSize,
             JSC::ExecutablePool *pool, JSC::CodeKind kind)
{
    JitCode *codeObj = js::NewJitCode<allowGC>(cx);
    if (!codeObj) {
        pool->release(headerSize + bufferSize, kind);
        return nullptr;
    }

    new (codeObj) JitCode(code, bufferSize, headerSize, pool, kind);
    return codeObj;
}

template
JitCode *
JitCode::New<CanGC>(JSContext *cx, uint8_t *code, uint32_t bufferSize, uint32_t headerSize,
                    JSC::ExecutablePool *pool, JSC::CodeKind kind);

template
JitCode *
JitCode::New<NoGC>(JSContext *cx, uint8_t *code, uint32_t bufferSize, uint32_t headerSize,
                   JSC::ExecutablePool *pool, JSC::CodeKind kind);

void
JitCode::copyFrom(MacroAssembler &masm)
{
    // Store the JitCode pointer right before the code buffer, so we can
    // recover the gcthing from relocation tables.
    *(JitCode **)(code_ - sizeof(JitCode *)) = this;
    insnSize_ = masm.instructionsSize();
    masm.executableCopy(code_);

    jumpRelocTableBytes_ = masm.jumpRelocationTableBytes();
    masm.copyJumpRelocationTable(code_ + jumpRelocTableOffset());

    dataRelocTableBytes_ = masm.dataRelocationTableBytes();
    masm.copyDataRelocationTable(code_ + dataRelocTableOffset());

    preBarrierTableBytes_ = masm.preBarrierTableBytes();
    masm.copyPreBarrierTable(code_ + preBarrierTableOffset());

    masm.processCodeLabels(code_);
}

void
JitCode::trace(JSTracer *trc)
{
    // Note that we cannot mark invalidated scripts, since we've basically
    // corrupted the code stream by injecting bailouts.
    if (invalidated())
        return;

    if (jumpRelocTableBytes_) {
        uint8_t *start = code_ + jumpRelocTableOffset();
        CompactBufferReader reader(start, start + jumpRelocTableBytes_);
        MacroAssembler::TraceJumpRelocations(trc, this, reader);
    }
    if (dataRelocTableBytes_) {
        uint8_t *start = code_ + dataRelocTableOffset();
        CompactBufferReader reader(start, start + dataRelocTableBytes_);
        MacroAssembler::TraceDataRelocations(trc, this, reader);
    }
}

void
JitCode::finalize(FreeOp *fop)
{
    // Make sure this can't race with an interrupting thread, which may try
    // to read the contents of the pool we are releasing references in.
    JS_ASSERT(fop->runtime()->currentThreadOwnsInterruptLock());

    // Buffer can be freed at any time hereafter. Catch use-after-free bugs.
    // Don't do this if the Ion code is protected, as the signal handler will
    // deadlock trying to reacquire the interrupt lock.
    if (fop->runtime()->jitRuntime() && !fop->runtime()->jitRuntime()->ionCodeProtected())
        memset(code_, JS_SWEPT_CODE_PATTERN, bufferSize_);
    code_ = nullptr;

    // Code buffers are stored inside JSC pools.
    // Pools are refcounted. Releasing the pool may free it.
    if (pool_) {
        // Horrible hack: if we are using perf integration, we don't
        // want to reuse code addresses, so we just leak the memory instead.
        if (!PerfEnabled())
            pool_->release(headerSize_ + bufferSize_, JSC::CodeKind(kind_));
        pool_ = nullptr;
    }
}

void
JitCode::togglePreBarriers(bool enabled)
{
    uint8_t *start = code_ + preBarrierTableOffset();
    CompactBufferReader reader(start, start + preBarrierTableBytes_);

    while (reader.more()) {
        size_t offset = reader.readUnsigned();
        CodeLocationLabel loc(this, CodeOffsetLabel(offset));
        if (enabled)
            Assembler::ToggleToCmp(loc);
        else
            Assembler::ToggleToJmp(loc);
    }
}

IonScript::IonScript()
  : method_(nullptr),
    deoptTable_(nullptr),
    osrPc_(nullptr),
    osrEntryOffset_(0),
    skipArgCheckEntryOffset_(0),
    invalidateEpilogueOffset_(0),
    invalidateEpilogueDataOffset_(0),
    numBailouts_(0),
    hasUncompiledCallTarget_(false),
    isParallelEntryScript_(false),
    hasSPSInstrumentation_(false),
    recompiling_(false),
    runtimeData_(0),
    runtimeSize_(0),
    cacheIndex_(0),
    cacheEntries_(0),
    safepointIndexOffset_(0),
    safepointIndexEntries_(0),
    safepointsStart_(0),
    safepointsSize_(0),
    frameSlots_(0),
    frameSize_(0),
    bailoutTable_(0),
    bailoutEntries_(0),
    osiIndexOffset_(0),
    osiIndexEntries_(0),
    snapshots_(0),
    snapshotsListSize_(0),
    snapshotsRVATableSize_(0),
    constantTable_(0),
    constantEntries_(0),
    callTargetList_(0),
    callTargetEntries_(0),
    backedgeList_(0),
    backedgeEntries_(0),
    refcount_(0),
    parallelAge_(0),
    recompileInfo_(),
    osrPcMismatchCounter_(0),
    dependentAsmJSModules(nullptr)
{
}

IonScript *
IonScript::New(JSContext *cx, types::RecompileInfo recompileInfo,
               uint32_t frameSlots, uint32_t frameSize,
               size_t snapshotsListSize, size_t snapshotsRVATableSize,
               size_t recoversSize, size_t bailoutEntries,
               size_t constants, size_t safepointIndices,
               size_t osiIndices, size_t cacheEntries,
               size_t runtimeSize,  size_t safepointsSize,
               size_t callTargetEntries, size_t backedgeEntries,
               OptimizationLevel optimizationLevel)
{
    static const int DataAlignment = sizeof(void *);

    if (snapshotsListSize >= MAX_BUFFER_SIZE ||
        (bailoutEntries >= MAX_BUFFER_SIZE / sizeof(uint32_t)))
    {
        js_ReportOutOfMemory(cx);
        return nullptr;
    }

    // This should not overflow on x86, because the memory is already allocated
    // *somewhere* and if their total overflowed there would be no memory left
    // at all.
    size_t paddedSnapshotsSize = AlignBytes(snapshotsListSize + snapshotsRVATableSize, DataAlignment);
    size_t paddedRecoversSize = AlignBytes(recoversSize, DataAlignment);
    size_t paddedBailoutSize = AlignBytes(bailoutEntries * sizeof(uint32_t), DataAlignment);
    size_t paddedConstantsSize = AlignBytes(constants * sizeof(Value), DataAlignment);
    size_t paddedSafepointIndicesSize = AlignBytes(safepointIndices * sizeof(SafepointIndex), DataAlignment);
    size_t paddedOsiIndicesSize = AlignBytes(osiIndices * sizeof(OsiIndex), DataAlignment);
    size_t paddedCacheEntriesSize = AlignBytes(cacheEntries * sizeof(uint32_t), DataAlignment);
    size_t paddedRuntimeSize = AlignBytes(runtimeSize, DataAlignment);
    size_t paddedSafepointSize = AlignBytes(safepointsSize, DataAlignment);
    size_t paddedCallTargetSize = AlignBytes(callTargetEntries * sizeof(JSScript *), DataAlignment);
    size_t paddedBackedgeSize = AlignBytes(backedgeEntries * sizeof(PatchableBackedge), DataAlignment);
    size_t bytes = paddedSnapshotsSize +
                   paddedRecoversSize +
                   paddedBailoutSize +
                   paddedConstantsSize +
                   paddedSafepointIndicesSize+
                   paddedOsiIndicesSize +
                   paddedCacheEntriesSize +
                   paddedRuntimeSize +
                   paddedSafepointSize +
                   paddedCallTargetSize +
                   paddedBackedgeSize;
    uint8_t *buffer = (uint8_t *)cx->malloc_(sizeof(IonScript) + bytes);
    if (!buffer)
        return nullptr;

    IonScript *script = reinterpret_cast<IonScript *>(buffer);
    new (script) IonScript();

    uint32_t offsetCursor = sizeof(IonScript);

    script->runtimeData_ = offsetCursor;
    script->runtimeSize_ = runtimeSize;
    offsetCursor += paddedRuntimeSize;

    script->cacheIndex_ = offsetCursor;
    script->cacheEntries_ = cacheEntries;
    offsetCursor += paddedCacheEntriesSize;

    script->safepointIndexOffset_ = offsetCursor;
    script->safepointIndexEntries_ = safepointIndices;
    offsetCursor += paddedSafepointIndicesSize;

    script->safepointsStart_ = offsetCursor;
    script->safepointsSize_ = safepointsSize;
    offsetCursor += paddedSafepointSize;

    script->bailoutTable_ = offsetCursor;
    script->bailoutEntries_ = bailoutEntries;
    offsetCursor += paddedBailoutSize;

    script->osiIndexOffset_ = offsetCursor;
    script->osiIndexEntries_ = osiIndices;
    offsetCursor += paddedOsiIndicesSize;

    script->snapshots_ = offsetCursor;
    script->snapshotsListSize_ = snapshotsListSize;
    script->snapshotsRVATableSize_ = snapshotsRVATableSize;
    offsetCursor += paddedSnapshotsSize;

    script->recovers_ = offsetCursor;
    script->recoversSize_ = recoversSize;
    offsetCursor += paddedRecoversSize;

    script->constantTable_ = offsetCursor;
    script->constantEntries_ = constants;
    offsetCursor += paddedConstantsSize;

    script->callTargetList_ = offsetCursor;
    script->callTargetEntries_ = callTargetEntries;
    offsetCursor += paddedCallTargetSize;

    script->backedgeList_ = offsetCursor;
    script->backedgeEntries_ = backedgeEntries;
    offsetCursor += paddedBackedgeSize;

    script->frameSlots_ = frameSlots;
    script->frameSize_ = frameSize;

    script->recompileInfo_ = recompileInfo;
    script->optimizationLevel_ = optimizationLevel;

    return script;
}

void
IonScript::trace(JSTracer *trc)
{
    if (method_)
        MarkJitCode(trc, &method_, "method");

    if (deoptTable_)
        MarkJitCode(trc, &deoptTable_, "deoptimizationTable");

    for (size_t i = 0; i < numConstants(); i++)
        gc::MarkValue(trc, &getConstant(i), "constant");

    // No write barrier is needed for the call target list, as it's attached
    // at compilation time and is read only.
    for (size_t i = 0; i < callTargetEntries(); i++) {
        // Propagate the parallelAge to the call targets.
        if (callTargetList()[i]->hasParallelIonScript())
            callTargetList()[i]->parallelIonScript()->parallelAge_ = parallelAge_;

        gc::MarkScriptUnbarriered(trc, &callTargetList()[i], "callTarget");
    }
}

/* static */ void
IonScript::writeBarrierPre(Zone *zone, IonScript *ionScript)
{
#ifdef JSGC_INCREMENTAL
    if (zone->needsBarrier())
        ionScript->trace(zone->barrierTracer());
#endif
}

void
IonScript::copySnapshots(const SnapshotWriter *writer)
{
    MOZ_ASSERT(writer->listSize() == snapshotsListSize_);
    memcpy((uint8_t *)this + snapshots_,
           writer->listBuffer(), snapshotsListSize_);

    MOZ_ASSERT(snapshotsRVATableSize_);
    MOZ_ASSERT(writer->RVATableSize() == snapshotsRVATableSize_);
    memcpy((uint8_t *)this + snapshots_ + snapshotsListSize_,
           writer->RVATableBuffer(), snapshotsRVATableSize_);
}

void
IonScript::copyRecovers(const RecoverWriter *writer)
{
    MOZ_ASSERT(writer->size() == recoversSize_);
    memcpy((uint8_t *)this + recovers_, writer->buffer(), recoversSize_);
}

void
IonScript::copySafepoints(const SafepointWriter *writer)
{
    JS_ASSERT(writer->size() == safepointsSize_);
    memcpy((uint8_t *)this + safepointsStart_, writer->buffer(), safepointsSize_);
}

void
IonScript::copyBailoutTable(const SnapshotOffset *table)
{
    memcpy(bailoutTable(), table, bailoutEntries_ * sizeof(uint32_t));
}

void
IonScript::copyConstants(const Value *vp)
{
    for (size_t i = 0; i < constantEntries_; i++)
        constants()[i].init(vp[i]);
}

void
IonScript::copyCallTargetEntries(JSScript **callTargets)
{
    for (size_t i = 0; i < callTargetEntries_; i++)
        callTargetList()[i] = callTargets[i];
}

void
IonScript::copyPatchableBackedges(JSContext *cx, JitCode *code,
                                  PatchableBackedgeInfo *backedges)
{
    for (size_t i = 0; i < backedgeEntries_; i++) {
        const PatchableBackedgeInfo &info = backedges[i];
        PatchableBackedge *patchableBackedge = &backedgeList()[i];

        CodeLocationJump backedge(code, info.backedge);
        CodeLocationLabel loopHeader(code, CodeOffsetLabel(info.loopHeader->offset()));
        CodeLocationLabel interruptCheck(code, CodeOffsetLabel(info.interruptCheck->offset()));
        new(patchableBackedge) PatchableBackedge(backedge, loopHeader, interruptCheck);

        // Point the backedge to either of its possible targets, according to
        // whether an interrupt is currently desired, matching the targets
        // established by ensureIonCodeAccessible() above. We don't handle the
        // interrupt immediately as the interrupt lock is held here.
        PatchJump(backedge, cx->runtime()->interrupt ? interruptCheck : loopHeader);

        cx->runtime()->jitRuntime()->addPatchableBackedge(patchableBackedge);
    }
}

void
IonScript::copySafepointIndices(const SafepointIndex *si, MacroAssembler &masm)
{
    // Jumps in the caches reflect the offset of those jumps in the compiled
    // code, not the absolute positions of the jumps. Update according to the
    // final code address now.
    SafepointIndex *table = safepointIndices();
    memcpy(table, si, safepointIndexEntries_ * sizeof(SafepointIndex));
    for (size_t i = 0; i < safepointIndexEntries_; i++)
        table[i].adjustDisplacement(masm.actualOffset(table[i].displacement()));
}

void
IonScript::copyOsiIndices(const OsiIndex *oi, MacroAssembler &masm)
{
    memcpy(osiIndices(), oi, osiIndexEntries_ * sizeof(OsiIndex));
    for (unsigned i = 0; i < osiIndexEntries_; i++)
        osiIndices()[i].fixUpOffset(masm);
}

void
IonScript::copyRuntimeData(const uint8_t *data)
{
    memcpy(runtimeData(), data, runtimeSize());
}

void
IonScript::copyCacheEntries(const uint32_t *caches, MacroAssembler &masm)
{
    memcpy(cacheIndex(), caches, numCaches() * sizeof(uint32_t));

    // Jumps in the caches reflect the offset of those jumps in the compiled
    // code, not the absolute positions of the jumps. Update according to the
    // final code address now.
    for (size_t i = 0; i < numCaches(); i++)
        getCacheFromIndex(i).updateBaseAddress(method_, masm);
}

const SafepointIndex *
IonScript::getSafepointIndex(uint32_t disp) const
{
    JS_ASSERT(safepointIndexEntries_ > 0);

    const SafepointIndex *table = safepointIndices();
    if (safepointIndexEntries_ == 1) {
        JS_ASSERT(disp == table[0].displacement());
        return &table[0];
    }

    size_t minEntry = 0;
    size_t maxEntry = safepointIndexEntries_ - 1;
    uint32_t min = table[minEntry].displacement();
    uint32_t max = table[maxEntry].displacement();

    // Raise if the element is not in the list.
    JS_ASSERT(min <= disp && disp <= max);

    // Approximate the location of the FrameInfo.
    size_t guess = (disp - min) * (maxEntry - minEntry) / (max - min) + minEntry;
    uint32_t guessDisp = table[guess].displacement();

    if (table[guess].displacement() == disp)
        return &table[guess];

    // Doing a linear scan from the guess should be more efficient in case of
    // small group which are equally distributed on the code.
    //
    // such as:  <...      ...    ...  ...  .   ...    ...>
    if (guessDisp > disp) {
        while (--guess >= minEntry) {
            guessDisp = table[guess].displacement();
            JS_ASSERT(guessDisp >= disp);
            if (guessDisp == disp)
                return &table[guess];
        }
    } else {
        while (++guess <= maxEntry) {
            guessDisp = table[guess].displacement();
            JS_ASSERT(guessDisp <= disp);
            if (guessDisp == disp)
                return &table[guess];
        }
    }

    MOZ_ASSUME_UNREACHABLE("displacement not found.");
}

const OsiIndex *
IonScript::getOsiIndex(uint32_t disp) const
{
    for (const OsiIndex *it = osiIndices(), *end = osiIndices() + osiIndexEntries_;
         it != end;
         ++it)
    {
        if (it->returnPointDisplacement() == disp)
            return it;
    }

    MOZ_ASSUME_UNREACHABLE("Failed to find OSI point return address");
}

const OsiIndex *
IonScript::getOsiIndex(uint8_t *retAddr) const
{
    IonSpew(IonSpew_Invalidate, "IonScript %p has method %p raw %p", (void *) this, (void *)
            method(), method()->raw());

    JS_ASSERT(containsCodeAddress(retAddr));
    uint32_t disp = retAddr - method()->raw();
    return getOsiIndex(disp);
}

void
IonScript::Trace(JSTracer *trc, IonScript *script)
{
    if (script != ION_DISABLED_SCRIPT)
        script->trace(trc);
}

void
IonScript::Destroy(FreeOp *fop, IonScript *script)
{
    script->destroyCaches();
    script->unlinkFromRuntime(fop);
    fop->free_(script);
}

void
IonScript::toggleBarriers(bool enabled)
{
    method()->togglePreBarriers(enabled);
}

void
IonScript::purgeCaches()
{
    // Don't reset any ICs if we're invalidated, otherwise, repointing the
    // inline jump could overwrite an invalidation marker. These ICs can
    // no longer run, however, the IC slow paths may be active on the stack.
    // ICs therefore are required to check for invalidation before patching,
    // to ensure the same invariant.
    if (invalidated())
        return;

    for (size_t i = 0; i < numCaches(); i++)
        getCacheFromIndex(i).reset();
}

void
IonScript::destroyCaches()
{
    for (size_t i = 0; i < numCaches(); i++)
        getCacheFromIndex(i).destroy();
}

bool
IonScript::addDependentAsmJSModule(JSContext *cx, DependentAsmJSModuleExit exit)
{
    if (!dependentAsmJSModules) {
        dependentAsmJSModules = cx->new_<Vector<DependentAsmJSModuleExit> >(cx);
        if (!dependentAsmJSModules)
            return false;
    }
    return dependentAsmJSModules->append(exit);
}

void
IonScript::unlinkFromRuntime(FreeOp *fop)
{
    // Remove any links from AsmJSModules that contain optimized FFI calls into
    // this IonScript.
    if (dependentAsmJSModules) {
        for (size_t i = 0; i < dependentAsmJSModules->length(); i++) {
            DependentAsmJSModuleExit exit = dependentAsmJSModules->begin()[i];
            exit.module->detachIonCompilation(exit.exitIndex);
        }

        fop->delete_(dependentAsmJSModules);
        dependentAsmJSModules = nullptr;
    }

    // The writes to the executable buffer below may clobber backedge jumps, so
    // make sure that those backedges are unlinked from the runtime and not
    // reclobbered with garbage if an interrupt is requested.
    JSRuntime *rt = fop->runtime();
    for (size_t i = 0; i < backedgeEntries_; i++) {
        PatchableBackedge *backedge = &backedgeList()[i];
        rt->jitRuntime()->removePatchableBackedge(backedge);
    }

    // Clear the list of backedges, so that this method is idempotent. It is
    // called during destruction, and may be additionally called when the
    // script is invalidated.
    backedgeEntries_ = 0;
}

void
jit::ToggleBarriers(JS::Zone *zone, bool needs)
{
    JSRuntime *rt = zone->runtimeFromMainThread();
    if (!rt->hasJitRuntime())
        return;

    for (gc::ZoneCellIterUnderGC i(zone, gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        if (script->hasIonScript())
            script->ionScript()->toggleBarriers(needs);
        if (script->hasBaselineScript())
            script->baselineScript()->toggleBarriers(needs);
    }

    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
        if (comp->jitCompartment())
            comp->jitCompartment()->toggleBaselineStubBarriers(needs);
    }
}

namespace js {
namespace jit {

bool
OptimizeMIR(MIRGenerator *mir)
{
    MIRGraph &graph = mir->graph();
    TraceLogger *logger;
    if (GetIonContext()->runtime->onMainThread())
        logger = TraceLoggerForMainThread(GetIonContext()->runtime);
    else
        logger = TraceLoggerForCurrentThread();

    if (!mir->compilingAsmJS()) {
        if (!MakeMRegExpHoistable(graph))
            return false;
    }

    IonSpewPass("BuildSSA");
    AssertBasicGraphCoherency(graph);

    if (mir->shouldCancel("Start"))
        return false;

    {
        AutoTraceLog log(logger, TraceLogger::SplitCriticalEdges);
        if (!SplitCriticalEdges(graph))
            return false;
        IonSpewPass("Split Critical Edges");
        AssertGraphCoherency(graph);

        if (mir->shouldCancel("Split Critical Edges"))
            return false;
    }

    {
        AutoTraceLog log(logger, TraceLogger::RenumberBlocks);
        if (!RenumberBlocks(graph))
            return false;
        IonSpewPass("Renumber Blocks");
        AssertGraphCoherency(graph);

        if (mir->shouldCancel("Renumber Blocks"))
            return false;
    }

    {
        AutoTraceLog log(logger, TraceLogger::DominatorTree);
        if (!BuildDominatorTree(graph))
            return false;
        // No spew: graph not changed.

        if (mir->shouldCancel("Dominator Tree"))
            return false;
    }

    {
        AutoTraceLog log(logger, TraceLogger::PhiAnalysis);
        // Aggressive phi elimination must occur before any code elimination. If the
        // script contains a try-statement, we only compiled the try block and not
        // the catch or finally blocks, so in this case it's also invalid to use
        // aggressive phi elimination.
        Observability observability = graph.hasTryBlock()
                                      ? ConservativeObservability
                                      : AggressiveObservability;
        if (!EliminatePhis(mir, graph, observability))
            return false;
        IonSpewPass("Eliminate phis");
        AssertGraphCoherency(graph);

        if (mir->shouldCancel("Eliminate phis"))
            return false;

        if (!BuildPhiReverseMapping(graph))
            return false;
        AssertExtendedGraphCoherency(graph);
        // No spew: graph not changed.

        if (mir->shouldCancel("Phi reverse mapping"))
            return false;
    }

    if (!mir->compilingAsmJS()) {
        AutoTraceLog log(logger, TraceLogger::ApplyTypes);
        if (!ApplyTypeInformation(mir, graph))
            return false;
        IonSpewPass("Apply types");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("Apply types"))
            return false;
    }

    if (graph.entryBlock()->info().executionMode() == ParallelExecution) {
        AutoTraceLog log(logger, TraceLogger::ParallelSafetyAnalysis);
        ParallelSafetyAnalysis analysis(mir, graph);
        if (!analysis.analyze())
            return false;
    }

    // Alias analysis is required for LICM and GVN so that we don't move
    // loads across stores.
    if (mir->optimizationInfo().licmEnabled() ||
        mir->optimizationInfo().gvnEnabled())
    {
        AutoTraceLog log(logger, TraceLogger::AliasAnalysis);
        AliasAnalysis analysis(mir, graph);
        if (!analysis.analyze())
            return false;
        IonSpewPass("Alias analysis");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("Alias analysis"))
            return false;

        // Eliminating dead resume point operands requires basic block
        // instructions to be numbered. Reuse the numbering computed during
        // alias analysis.
        if (!EliminateDeadResumePointOperands(mir, graph))
            return false;

        if (mir->shouldCancel("Eliminate dead resume point operands"))
            return false;
    }

    if (mir->optimizationInfo().gvnEnabled()) {
        AutoTraceLog log(logger, TraceLogger::GVN);
        ValueNumberer gvn(mir, graph, mir->optimizationInfo().gvnKind() == GVN_Optimistic);
        if (!gvn.analyze())
            return false;
        IonSpewPass("GVN");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("GVN"))
            return false;
    }

    if (mir->optimizationInfo().uceEnabled()) {
        AutoTraceLog log(logger, TraceLogger::UCE);
        UnreachableCodeElimination uce(mir, graph);
        if (!uce.analyze())
            return false;
        IonSpewPass("UCE");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("UCE"))
            return false;
    }

    if (mir->optimizationInfo().licmEnabled()) {
        AutoTraceLog log(logger, TraceLogger::LICM);
        // LICM can hoist instructions from conditional branches and trigger
        // repeated bailouts. Disable it if this script is known to bailout
        // frequently.
        JSScript *script = mir->info().script();
        if (!script || !script->hadFrequentBailouts()) {
            if (!LICM(mir, graph))
                return false;
            IonSpewPass("LICM");
            AssertExtendedGraphCoherency(graph);

            if (mir->shouldCancel("LICM"))
                return false;
        }
    }

    if (mir->optimizationInfo().rangeAnalysisEnabled()) {
        AutoTraceLog log(logger, TraceLogger::RangeAnalysis);
        RangeAnalysis r(mir, graph);
        if (!r.addBetaNodes())
            return false;
        IonSpewPass("Beta");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("RA Beta"))
            return false;

        if (!r.analyze() || !r.addRangeAssertions())
            return false;
        IonSpewPass("Range Analysis");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("Range Analysis"))
            return false;

        if (!r.removeBetaNodes())
            return false;
        IonSpewPass("De-Beta");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("RA De-Beta"))
            return false;

        if (mir->optimizationInfo().uceEnabled()) {
            bool shouldRunUCE = false;
            if (!r.prepareForUCE(&shouldRunUCE))
                return false;
            IonSpewPass("RA check UCE");
            AssertExtendedGraphCoherency(graph);

            if (mir->shouldCancel("RA check UCE"))
                return false;

            if (shouldRunUCE) {
                UnreachableCodeElimination uce(mir, graph);
                uce.disableAliasAnalysis();
                if (!uce.analyze())
                    return false;
                IonSpewPass("UCE After RA");
                AssertExtendedGraphCoherency(graph);

                if (mir->shouldCancel("UCE After RA"))
                    return false;
            }
        }

        if (mir->optimizationInfo().autoTruncateEnabled()) {
            if (!r.truncate())
                return false;
            IonSpewPass("Truncate Doubles");
            AssertExtendedGraphCoherency(graph);

            if (mir->shouldCancel("Truncate Doubles"))
                return false;
        }
    }

    if (mir->optimizationInfo().eaaEnabled()) {
        AutoTraceLog log(logger, TraceLogger::EffectiveAddressAnalysis);
        EffectiveAddressAnalysis eaa(graph);
        if (!eaa.analyze())
            return false;
        IonSpewPass("Effective Address Analysis");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("Effective Address Analysis"))
            return false;
    }

    {
        AutoTraceLog log(logger, TraceLogger::EliminateDeadCode);
        if (!EliminateDeadCode(mir, graph))
            return false;
        IonSpewPass("DCE");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("DCE"))
            return false;
    }

    // Make loops contiguious. We do this after GVN/UCE and range analysis,
    // which can remove CFG edges, exposing more blocks that can be moved.
    // We also disable this when profiling, since reordering blocks appears
    // to make the profiler unhappy.
    {
        AutoTraceLog log(logger, TraceLogger::MakeLoopsContiguous);
        if (!MakeLoopsContiguous(graph))
            return false;
        IonSpewPass("Make loops contiguous");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("Make loops contiguous"))
            return false;
    }

    // Passes after this point must not move instructions; these analyses
    // depend on knowing the final order in which instructions will execute.

    if (mir->optimizationInfo().edgeCaseAnalysisEnabled()) {
        AutoTraceLog log(logger, TraceLogger::EdgeCaseAnalysis);
        EdgeCaseAnalysis edgeCaseAnalysis(mir, graph);
        if (!edgeCaseAnalysis.analyzeLate())
            return false;
        IonSpewPass("Edge Case Analysis (Late)");
        AssertGraphCoherency(graph);

        if (mir->shouldCancel("Edge Case Analysis (Late)"))
            return false;
    }

    if (mir->optimizationInfo().eliminateRedundantChecksEnabled()) {
        AutoTraceLog log(logger, TraceLogger::EliminateRedundantChecks);
        // Note: check elimination has to run after all other passes that move
        // instructions. Since check uses are replaced with the actual index,
        // code motion after this pass could incorrectly move a load or store
        // before its bounds check.
        if (!EliminateRedundantChecks(graph))
            return false;
        IonSpewPass("Bounds Check Elimination");
        AssertGraphCoherency(graph);
    }

    return true;
}

LIRGraph *
GenerateLIR(MIRGenerator *mir)
{
    MIRGraph &graph = mir->graph();

    TraceLogger *logger;
    if (GetIonContext()->runtime->onMainThread())
        logger = TraceLoggerForMainThread(GetIonContext()->runtime);
    else
        logger = TraceLoggerForCurrentThread();

    LIRGraph *lir = mir->alloc().lifoAlloc()->new_<LIRGraph>(&graph);
    if (!lir || !lir->init())
        return nullptr;

    LIRGenerator lirgen(mir, graph, *lir);
    {
        AutoTraceLog log(logger, TraceLogger::GenerateLIR);
        if (!lirgen.generate())
            return nullptr;
        IonSpewPass("Generate LIR");

        if (mir->shouldCancel("Generate LIR"))
            return nullptr;
    }

    AllocationIntegrityState integrity(*lir);

    {
        AutoTraceLog log(logger, TraceLogger::RegisterAllocation);

        switch (mir->optimizationInfo().registerAllocator()) {
          case RegisterAllocator_LSRA: {
#ifdef DEBUG
            if (!integrity.record())
                return nullptr;
#endif

            LinearScanAllocator regalloc(mir, &lirgen, *lir);
            if (!regalloc.go())
                return nullptr;

#ifdef DEBUG
            if (!integrity.check(false))
                return nullptr;
#endif

            IonSpewPass("Allocate Registers [LSRA]", &regalloc);
            break;
          }

          case RegisterAllocator_Backtracking: {
#ifdef DEBUG
            if (!integrity.record())
                return nullptr;
#endif

            BacktrackingAllocator regalloc(mir, &lirgen, *lir);
            if (!regalloc.go())
                return nullptr;

#ifdef DEBUG
            if (!integrity.check(false))
                return nullptr;
#endif

            IonSpewPass("Allocate Registers [Backtracking]");
            break;
          }

          case RegisterAllocator_Stupid: {
            // Use the integrity checker to populate safepoint information, so
            // run it in all builds.
            if (!integrity.record())
                return nullptr;

            StupidAllocator regalloc(mir, &lirgen, *lir);
            if (!regalloc.go())
                return nullptr;
            if (!integrity.check(true))
                return nullptr;
            IonSpewPass("Allocate Registers [Stupid]");
            break;
          }

          default:
            MOZ_ASSUME_UNREACHABLE("Bad regalloc");
        }

        if (mir->shouldCancel("Allocate Registers"))
            return nullptr;
    }

    return lir;
}

CodeGenerator *
GenerateCode(MIRGenerator *mir, LIRGraph *lir)
{
    TraceLogger *logger;
    if (GetIonContext()->runtime->onMainThread())
        logger = TraceLoggerForMainThread(GetIonContext()->runtime);
    else
        logger = TraceLoggerForCurrentThread();
    AutoTraceLog log(logger, TraceLogger::GenerateCode);

    CodeGenerator *codegen = js_new<CodeGenerator>(mir, lir);
    if (!codegen)
        return nullptr;

    if (!codegen->generate()) {
        js_delete(codegen);
        return nullptr;
    }

    return codegen;
}

CodeGenerator *
CompileBackEnd(MIRGenerator *mir)
{
    if (!OptimizeMIR(mir))
        return nullptr;

    LIRGraph *lir = GenerateLIR(mir);
    if (!lir)
        return nullptr;

    return GenerateCode(mir, lir);
}

void
AttachFinishedCompilations(JSContext *cx)
{
#ifdef JS_THREADSAFE
    JitCompartment *ion = cx->compartment()->jitCompartment();
    if (!ion)
        return;

    types::AutoEnterAnalysis enterTypes(cx);
    AutoLockHelperThreadState lock;

    GlobalHelperThreadState::IonBuilderVector &finished = HelperThreadState().ionFinishedList();

    TraceLogger *logger = TraceLoggerForMainThread(cx->runtime());

    // Incorporate any off thread compilations for the compartment which have
    // finished, failed or have been cancelled.
    while (true) {
        IonBuilder *builder = nullptr;

        // Find a finished builder for the compartment.
        for (size_t i = 0; i < finished.length(); i++) {
            IonBuilder *testBuilder = finished[i];
            if (testBuilder->compartment == CompileCompartment::get(cx->compartment())) {
                builder = testBuilder;
                HelperThreadState().remove(finished, &i);
                break;
            }
        }
        if (!builder)
            break;

        if (CodeGenerator *codegen = builder->backgroundCodegen()) {
            RootedScript script(cx, builder->script());
            IonContext ictx(cx, &builder->alloc());
            AutoTraceLog logScript(logger, TraceLogCreateTextId(logger, script));
            AutoTraceLog logLink(logger, TraceLogger::IonLinking);

            // Root the assembler until the builder is finished below. As it
            // was constructed off thread, the assembler has not been rooted
            // previously, though any GC activity would discard the builder.
            codegen->masm.constructRoot(cx);

            bool success;
            {
                // Release the helper thread lock and root the compiler for GC.
                AutoTempAllocatorRooter root(cx, &builder->alloc());
                AutoUnlockHelperThreadState unlock;
                success = codegen->link(cx, builder->constraints());
            }

            if (!success) {
                // Silently ignore OOM during code generation. The caller is
                // InvokeInterruptCallback, which always runs at a
                // nondeterministic time. It's not OK to throw a catchable
                // exception from there.
                cx->clearPendingException();
            }
        }

        FinishOffThreadBuilder(builder);
    }
#endif
}

static const size_t BUILDER_LIFO_ALLOC_PRIMARY_CHUNK_SIZE = 1 << 12;

static inline bool
OffThreadCompilationAvailable(JSContext *cx)
{
#ifdef JS_THREADSAFE
    // Even if off thread compilation is enabled, compilation must still occur
    // on the main thread in some cases. Do not compile off thread during an
    // incremental GC, as this may trip incremental read barriers.
    //
    // Require cpuCount > 1 so that Ion compilation jobs and main-thread
    // execution are not competing for the same resources.
    return cx->runtime()->canUseParallelIonCompilation()
        && HelperThreadState().cpuCount > 1
        && cx->runtime()->gc.incrementalState == gc::NO_INCREMENTAL;
#else
    return false;
#endif
}

static void
TrackAllProperties(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->hasSingletonType());

    for (Shape::Range<NoGC> range(obj->lastProperty()); !range.empty(); range.popFront())
        types::EnsureTrackPropertyTypes(cx, obj, range.front().propid());
}

static void
TrackPropertiesForSingletonScopes(JSContext *cx, JSScript *script, BaselineFrame *baselineFrame)
{
    // Ensure that all properties of singleton call objects which the script
    // could access are tracked. These are generally accessed through
    // ALIASEDVAR operations in baseline and will not be tracked even if they
    // have been accessed in baseline code.
    JSObject *environment = script->functionNonDelazifying()
                            ? script->functionNonDelazifying()->environment()
                            : nullptr;

    while (environment && !environment->is<GlobalObject>()) {
        if (environment->is<CallObject>() && environment->hasSingletonType())
            TrackAllProperties(cx, environment);
        environment = environment->enclosingScope();
    }

    if (baselineFrame) {
        JSObject *scope = baselineFrame->scopeChain();
        if (scope->is<CallObject>() && scope->hasSingletonType())
            TrackAllProperties(cx, scope);
    }
}

static AbortReason
IonCompile(JSContext *cx, JSScript *script,
           BaselineFrame *baselineFrame, jsbytecode *osrPc, bool constructing,
           ExecutionMode executionMode, bool recompile,
           OptimizationLevel optimizationLevel)
{
    TraceLogger *logger = TraceLoggerForMainThread(cx->runtime());
    AutoTraceLog logScript(logger, TraceLogCreateTextId(logger, script));
    AutoTraceLog logCompile(logger, TraceLogger::IonCompilation);

    JS_ASSERT(optimizationLevel > Optimization_DontCompile);

    // Make sure the script's canonical function isn't lazy. We can't de-lazify
    // it in a helper thread.
    script->ensureNonLazyCanonicalFunction(cx);

    TrackPropertiesForSingletonScopes(cx, script, baselineFrame);

    LifoAlloc *alloc = cx->new_<LifoAlloc>(BUILDER_LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
    if (!alloc)
        return AbortReason_Alloc;

    ScopedJSDeletePtr<LifoAlloc> autoDelete(alloc);

    TempAllocator *temp = alloc->new_<TempAllocator>(alloc);
    if (!temp)
        return AbortReason_Alloc;

    IonContext ictx(cx, temp);

    types::AutoEnterAnalysis enter(cx);

    if (!cx->compartment()->ensureJitCompartmentExists(cx))
        return AbortReason_Alloc;

    if (!cx->compartment()->jitCompartment()->ensureIonStubsExist(cx))
        return AbortReason_Alloc;

    if (executionMode == ParallelExecution &&
        LIRGenerator::allowInlineForkJoinGetSlice() &&
        !cx->runtime()->jitRuntime()->ensureForkJoinGetSliceStubExists(cx))
    {
        return AbortReason_Alloc;
    }

    MIRGraph *graph = alloc->new_<MIRGraph>(temp);
    if (!graph)
        return AbortReason_Alloc;

    InlineScriptTree *inlineScriptTree = InlineScriptTree::New(temp, nullptr, nullptr, script);
    if (!inlineScriptTree)
        return AbortReason_Alloc;

    CompileInfo *info = alloc->new_<CompileInfo>(script, script->functionNonDelazifying(), osrPc,
                                                 constructing, executionMode,
                                                 script->needsArgsObj(), inlineScriptTree);
    if (!info)
        return AbortReason_Alloc;

    BaselineInspector *inspector = alloc->new_<BaselineInspector>(script);
    if (!inspector)
        return AbortReason_Alloc;

    BaselineFrameInspector *baselineFrameInspector = nullptr;
    if (baselineFrame) {
        baselineFrameInspector = NewBaselineFrameInspector(temp, baselineFrame, info);
        if (!baselineFrameInspector)
            return AbortReason_Alloc;
    }

    AutoTempAllocatorRooter root(cx, temp);
    types::CompilerConstraintList *constraints = types::NewCompilerConstraintList(*temp);
    if (!constraints)
        return AbortReason_Alloc;

    const OptimizationInfo *optimizationInfo = js_IonOptimizations.get(optimizationLevel);
    const JitCompileOptions options(cx);

    IonBuilder *builder = alloc->new_<IonBuilder>((JSContext *) nullptr,
                                                  CompileCompartment::get(cx->compartment()),
                                                  options, temp, graph, constraints,
                                                  inspector, info, optimizationInfo,
                                                  baselineFrameInspector);
    if (!builder)
        return AbortReason_Alloc;

    JS_ASSERT(recompile == HasIonScript(builder->script(), executionMode));
    JS_ASSERT(CanIonCompile(builder->script(), executionMode));

    RootedScript builderScript(cx, builder->script());

    if (recompile) {
        JS_ASSERT(executionMode == SequentialExecution);
        builderScript->ionScript()->setRecompiling();
    }

    IonSpewNewFunction(graph, builderScript);

    bool succeeded = builder->build();
    builder->clearForBackEnd();

    if (!succeeded)
        return builder->abortReason();

    // If possible, compile the script off thread.
    if (OffThreadCompilationAvailable(cx)) {
        if (!recompile)
            SetIonScript(builderScript, executionMode, ION_COMPILING_SCRIPT);

        IonSpew(IonSpew_Logs, "Can't log script %s:%d. (Compiled on background thread.)",
                              builderScript->filename(), builderScript->lineno());

        if (!StartOffThreadIonCompile(cx, builder)) {
            IonSpew(IonSpew_Abort, "Unable to start off-thread ion compilation.");
            return AbortReason_Alloc;
        }

        // The allocator and associated data will be destroyed after being
        // processed in the finishedOffThreadCompilations list.
        autoDelete.forget();

        return AbortReason_NoAbort;
    }

    ScopedJSDeletePtr<CodeGenerator> codegen(CompileBackEnd(builder));
    if (!codegen) {
        IonSpew(IonSpew_Abort, "Failed during back-end compilation.");
        return AbortReason_Disable;
    }

    bool success = codegen->link(cx, builder->constraints());

    IonSpewEndFunction();

    return success ? AbortReason_NoAbort : AbortReason_Disable;
}

static bool
CheckFrame(BaselineFrame *frame)
{
    JS_ASSERT(!frame->isGeneratorFrame());
    JS_ASSERT(!frame->isDebuggerFrame());

    // This check is to not overrun the stack.
    if (frame->isFunctionFrame() && TooManyArguments(frame->numActualArgs())) {
        IonSpew(IonSpew_Abort, "too many actual args");
        return false;
    }

    return true;
}

static bool
CheckScript(JSContext *cx, JSScript *script, bool osr)
{
    if (script->isForEval()) {
        // Eval frames are not yet supported. Supporting this will require new
        // logic in pushBailoutFrame to deal with linking prev.
        // Additionally, JSOP_DEFVAR support will require baking in isEvalFrame().
        IonSpew(IonSpew_Abort, "eval script");
        return false;
    }

    if (!script->compileAndGo()) {
        IonSpew(IonSpew_Abort, "not compile-and-go");
        return false;
    }

    return true;
}

static MethodStatus
CheckScriptSize(JSContext *cx, JSScript* script)
{
    if (!js_JitOptions.limitScriptSize)
        return Method_Compiled;

    if (script->length() > MAX_OFF_THREAD_SCRIPT_SIZE) {
        // Some scripts are so large we never try to Ion compile them.
        IonSpew(IonSpew_Abort, "Script too large (%u bytes)", script->length());
        return Method_CantCompile;
    }

    uint32_t numLocalsAndArgs = NumLocalsAndArgs(script);

    if (script->length() > MAX_MAIN_THREAD_SCRIPT_SIZE ||
        numLocalsAndArgs > MAX_MAIN_THREAD_LOCALS_AND_ARGS)
    {
#ifdef JS_THREADSAFE
        size_t cpuCount = HelperThreadState().cpuCount;
#else
        size_t cpuCount = 1;
#endif
        if (cx->runtime()->canUseParallelIonCompilation() && cpuCount > 1) {
            // Even if off thread compilation is enabled, there are cases where
            // compilation must still occur on the main thread. Don't compile
            // in these cases, but do not forbid compilation so that the script
            // may be compiled later.
            if (!OffThreadCompilationAvailable(cx)) {
                IonSpew(IonSpew_Abort,
                        "Script too large for main thread, skipping (%u bytes) (%u locals/args)",
                        script->length(), numLocalsAndArgs);
                return Method_Skipped;
            }
        } else {
            IonSpew(IonSpew_Abort, "Script too large (%u bytes) (%u locals/args)",
                    script->length(), numLocalsAndArgs);
            return Method_CantCompile;
        }
    }

    return Method_Compiled;
}

bool
CanIonCompileScript(JSContext *cx, JSScript *script, bool osr)
{
    if (!script->canIonCompile() || !CheckScript(cx, script, osr))
        return false;

    return CheckScriptSize(cx, script) == Method_Compiled;
}

static OptimizationLevel
GetOptimizationLevel(HandleScript script, jsbytecode *pc, ExecutionMode executionMode)
{
    if (executionMode == ParallelExecution)
        return Optimization_Normal;

    JS_ASSERT(executionMode == SequentialExecution);

    return js_IonOptimizations.levelForScript(script, pc);
}

static MethodStatus
Compile(JSContext *cx, HandleScript script, BaselineFrame *osrFrame, jsbytecode *osrPc,
        bool constructing, ExecutionMode executionMode)
{
    JS_ASSERT(jit::IsIonEnabled(cx));
    JS_ASSERT(jit::IsBaselineEnabled(cx));
    JS_ASSERT_IF(osrPc != nullptr, LoopEntryCanIonOsr(osrPc));
    JS_ASSERT_IF(executionMode == ParallelExecution, !osrFrame && !osrPc);
    JS_ASSERT_IF(executionMode == ParallelExecution, !HasIonScript(script, executionMode));

    if (!script->hasBaselineScript())
        return Method_Skipped;

    if (cx->compartment()->debugMode()) {
        IonSpew(IonSpew_Abort, "debugging");
        return Method_CantCompile;
    }

    if (!CheckScript(cx, script, bool(osrPc))) {
        IonSpew(IonSpew_Abort, "Aborted compilation of %s:%d", script->filename(), script->lineno());
        return Method_CantCompile;
    }

    MethodStatus status = CheckScriptSize(cx, script);
    if (status != Method_Compiled) {
        IonSpew(IonSpew_Abort, "Aborted compilation of %s:%d", script->filename(), script->lineno());
        return status;
    }

    bool recompile = false;
    OptimizationLevel optimizationLevel = GetOptimizationLevel(script, osrPc, executionMode);
    if (optimizationLevel == Optimization_DontCompile)
        return Method_Skipped;

    IonScript *scriptIon = GetIonScript(script, executionMode);
    if (scriptIon) {
        if (!scriptIon->method())
            return Method_CantCompile;

        MethodStatus failedState = Method_Compiled;

        // If we keep failing to enter the script due to an OSR pc mismatch,
        // recompile with the right pc.
        if (osrPc && script->ionScript()->osrPc() != osrPc) {
            uint32_t count = script->ionScript()->incrOsrPcMismatchCounter();
            if (count <= js_JitOptions.osrPcMismatchesBeforeRecompile)
                return Method_Skipped;

            failedState = Method_Skipped;
        }

        // Don't recompile/overwrite higher optimized code,
        // with a lower optimization level.
        if (optimizationLevel < scriptIon->optimizationLevel())
            return failedState;

        if (optimizationLevel == scriptIon->optimizationLevel() &&
            (!osrPc || script->ionScript()->osrPc() == osrPc))
        {
            return failedState;
        }

        // Don't start compiling if already compiling
        if (scriptIon->isRecompiling())
            return failedState;

        if (osrPc)
            script->ionScript()->resetOsrPcMismatchCounter();

        recompile = true;
    }

    AbortReason reason = IonCompile(cx, script, osrFrame, osrPc, constructing, executionMode,
                                    recompile, optimizationLevel);
    if (reason == AbortReason_Error)
        return Method_Error;

    if (reason == AbortReason_Disable)
        return Method_CantCompile;

    if (reason == AbortReason_Alloc) {
        js_ReportOutOfMemory(cx);
        return Method_Error;
    }

    // Compilation succeeded or we invalidated right away or an inlining/alloc abort
    if (HasIonScript(script, executionMode)) {
        if (osrPc && script->ionScript()->osrPc() != osrPc)
            return Method_Skipped;
        return Method_Compiled;
    }
    return Method_Skipped;
}

} // namespace jit
} // namespace js

// Decide if a transition from interpreter execution to Ion code should occur.
// May compile or recompile the target JSScript.
MethodStatus
jit::CanEnterAtBranch(JSContext *cx, JSScript *script, BaselineFrame *osrFrame,
                      jsbytecode *pc, bool isConstructing)
{
    JS_ASSERT(jit::IsIonEnabled(cx));
    JS_ASSERT((JSOp)*pc == JSOP_LOOPENTRY);
    JS_ASSERT(LoopEntryCanIonOsr(pc));

    // Skip if the script has been disabled.
    if (!script->canIonCompile())
        return Method_Skipped;

    // Skip if the script is being compiled off thread.
    if (script->isIonCompilingOffThread())
        return Method_Skipped;

    // Skip if the code is expected to result in a bailout.
    if (script->hasIonScript() && script->ionScript()->bailoutExpected())
        return Method_Skipped;

    // Optionally ignore on user request.
    if (!js_JitOptions.osr)
        return Method_Skipped;

    // Mark as forbidden if frame can't be handled.
    if (!CheckFrame(osrFrame)) {
        ForbidCompilation(cx, script);
        return Method_CantCompile;
    }

    // Attempt compilation.
    // - Returns Method_Compiled if the right ionscript is present
    //   (Meaning it was present or a sequantial compile finished)
    // - Returns Method_Skipped if pc doesn't match
    //   (This means a background thread compilation with that pc could have started or not.)
    RootedScript rscript(cx, script);
    MethodStatus status = Compile(cx, rscript, osrFrame, pc, isConstructing, SequentialExecution);
    if (status != Method_Compiled) {
        if (status == Method_CantCompile)
            ForbidCompilation(cx, script);
        return status;
    }

    return Method_Compiled;
}

MethodStatus
jit::CanEnter(JSContext *cx, RunState &state)
{
    JS_ASSERT(jit::IsIonEnabled(cx));

    JSScript *script = state.script();

    // Skip if the script has been disabled.
    if (!script->canIonCompile())
        return Method_Skipped;

    // Skip if the script is being compiled off thread.
    if (script->isIonCompilingOffThread())
        return Method_Skipped;

    // Skip if the code is expected to result in a bailout.
    if (script->hasIonScript() && script->ionScript()->bailoutExpected())
        return Method_Skipped;

    // If constructing, allocate a new |this| object before building Ion.
    // Creating |this| is done before building Ion because it may change the
    // type information and invalidate compilation results.
    if (state.isInvoke()) {
        InvokeState &invoke = *state.asInvoke();

        if (TooManyArguments(invoke.args().length())) {
            IonSpew(IonSpew_Abort, "too many actual args");
            ForbidCompilation(cx, script);
            return Method_CantCompile;
        }

        if (TooManyArguments(invoke.args().callee().as<JSFunction>().nargs())) {
            IonSpew(IonSpew_Abort, "too many args");
            ForbidCompilation(cx, script);
            return Method_CantCompile;
        }

        if (invoke.constructing() && invoke.args().thisv().isPrimitive()) {
            RootedScript scriptRoot(cx, script);
            RootedObject callee(cx, &invoke.args().callee());
            RootedObject obj(cx, CreateThisForFunction(cx, callee,
                                                       invoke.useNewType()
                                                       ? SingletonObject
                                                       : GenericObject));
            if (!obj || !jit::IsIonEnabled(cx)) // Note: OOM under CreateThis can disable TI.
                return Method_Skipped;
            invoke.args().setThis(ObjectValue(*obj));
            script = scriptRoot;
        }
    } else if (state.isGenerator()) {
        IonSpew(IonSpew_Abort, "generator frame");
        ForbidCompilation(cx, script);
        return Method_CantCompile;
    }

    // If --ion-eager is used, compile with Baseline first, so that we
    // can directly enter IonMonkey.
    RootedScript rscript(cx, script);
    if (js_JitOptions.eagerCompilation && !rscript->hasBaselineScript()) {
        MethodStatus status = CanEnterBaselineMethod(cx, state);
        if (status != Method_Compiled)
            return status;
    }

    // Attempt compilation. Returns Method_Compiled if already compiled.
    bool constructing = state.isInvoke() && state.asInvoke()->constructing();
    MethodStatus status =
        Compile(cx, rscript, nullptr, nullptr, constructing, SequentialExecution);
    if (status != Method_Compiled) {
        if (status == Method_CantCompile)
            ForbidCompilation(cx, rscript);
        return status;
    }

    return Method_Compiled;
}

MethodStatus
jit::CompileFunctionForBaseline(JSContext *cx, HandleScript script, BaselineFrame *frame,
                                bool isConstructing)
{
    JS_ASSERT(jit::IsIonEnabled(cx));
    JS_ASSERT(frame->fun()->nonLazyScript()->canIonCompile());
    JS_ASSERT(!frame->fun()->nonLazyScript()->isIonCompilingOffThread());
    JS_ASSERT(!frame->fun()->nonLazyScript()->hasIonScript());
    JS_ASSERT(frame->isFunctionFrame());

    // Mark as forbidden if frame can't be handled.
    if (!CheckFrame(frame)) {
        ForbidCompilation(cx, script);
        return Method_CantCompile;
    }

    // Attempt compilation. Returns Method_Compiled if already compiled.
    MethodStatus status =
        Compile(cx, script, frame, nullptr, isConstructing, SequentialExecution);
    if (status != Method_Compiled) {
        if (status == Method_CantCompile)
            ForbidCompilation(cx, script);
        return status;
    }

    return Method_Compiled;
}

MethodStatus
jit::Recompile(JSContext *cx, HandleScript script, BaselineFrame *osrFrame, jsbytecode *osrPc,
               bool constructing)
{
    JS_ASSERT(script->hasIonScript());
    if (script->ionScript()->isRecompiling())
        return Method_Compiled;

    MethodStatus status =
        Compile(cx, script, osrFrame, osrPc, constructing, SequentialExecution);
    if (status != Method_Compiled) {
        if (status == Method_CantCompile)
            ForbidCompilation(cx, script);
        return status;
    }

    return Method_Compiled;
}

MethodStatus
jit::CanEnterInParallel(JSContext *cx, HandleScript script)
{
    // Skip if the script has been disabled.
    //
    // Note: We return Method_Skipped in this case because the other
    // CanEnter() methods do so. However, ForkJoin.cpp detects this
    // condition differently treats it more like an error.
    if (!script->canParallelIonCompile())
        return Method_Skipped;

    // Skip if the script is being compiled off thread.
    if (script->isParallelIonCompilingOffThread())
        return Method_Skipped;

    MethodStatus status = Compile(cx, script, nullptr, nullptr, false, ParallelExecution);
    if (status != Method_Compiled) {
        if (status == Method_CantCompile)
            ForbidCompilation(cx, script, ParallelExecution);
        return status;
    }

    // This can GC, so afterward, script->parallelIon is
    // not guaranteed to be valid.
    if (!cx->runtime()->jitRuntime()->enterIon())
        return Method_Error;

    // Subtle: it is possible for GC to occur during
    // compilation of one of the invoked functions, which
    // would cause the earlier functions (such as the
    // kernel itself) to be collected.  In this event, we
    // give up and fallback to sequential for now.
    if (!script->hasParallelIonScript()) {
        parallel::Spew(
            parallel::SpewCompile,
            "Script %p:%s:%u was garbage-collected or invalidated",
            script.get(), script->filename(), script->lineno());
        return Method_Skipped;
    }

    return Method_Compiled;
}

MethodStatus
jit::CanEnterUsingFastInvoke(JSContext *cx, HandleScript script, uint32_t numActualArgs)
{
    JS_ASSERT(jit::IsIonEnabled(cx));

    // Skip if the code is expected to result in a bailout.
    if (!script->hasIonScript() || script->ionScript()->bailoutExpected())
        return Method_Skipped;

    // Don't handle arguments underflow, to make this work we would have to pad
    // missing arguments with |undefined|.
    if (numActualArgs < script->functionNonDelazifying()->nargs())
        return Method_Skipped;

    if (!cx->compartment()->ensureJitCompartmentExists(cx))
        return Method_Error;

    // This can GC, so afterward, script->ion is not guaranteed to be valid.
    if (!cx->runtime()->jitRuntime()->enterIon())
        return Method_Error;

    if (!script->hasIonScript())
        return Method_Skipped;

    return Method_Compiled;
}

static IonExecStatus
EnterIon(JSContext *cx, EnterJitData &data)
{
    JS_CHECK_RECURSION(cx, return IonExec_Aborted);
    JS_ASSERT(jit::IsIonEnabled(cx));
    JS_ASSERT(!data.osrFrame);

    EnterJitCode enter = cx->runtime()->jitRuntime()->enterIon();

    // Caller must construct |this| before invoking the Ion function.
    JS_ASSERT_IF(data.constructing, data.maxArgv[0].isObject());

    data.result.setInt32(data.numActualArgs);
    {
        AssertCompartmentUnchanged pcc(cx);
        JitActivation activation(cx, data.constructing);

        CALL_GENERATED_CODE(enter, data.jitcode, data.maxArgc, data.maxArgv, /* osrFrame = */nullptr, data.calleeToken,
                            /* scopeChain = */ nullptr, 0, data.result.address());
    }

    JS_ASSERT(!cx->runtime()->jitRuntime()->hasIonReturnOverride());

    // Jit callers wrap primitive constructor return.
    if (!data.result.isMagic() && data.constructing && data.result.isPrimitive())
        data.result = data.maxArgv[0];

    // Release temporary buffer used for OSR into Ion.
    cx->runtime()->getJitRuntime(cx)->freeOsrTempData();

    JS_ASSERT_IF(data.result.isMagic(), data.result.isMagic(JS_ION_ERROR));
    return data.result.isMagic() ? IonExec_Error : IonExec_Ok;
}

bool
jit::SetEnterJitData(JSContext *cx, EnterJitData &data, RunState &state, AutoValueVector &vals)
{
    data.osrFrame = nullptr;

    if (state.isInvoke()) {
        CallArgs &args = state.asInvoke()->args();
        unsigned numFormals = state.script()->functionNonDelazifying()->nargs();
        data.constructing = state.asInvoke()->constructing();
        data.numActualArgs = args.length();
        data.maxArgc = Max(args.length(), numFormals) + 1;
        data.scopeChain = nullptr;
        data.calleeToken = CalleeToToken(&args.callee().as<JSFunction>());

        if (data.numActualArgs >= numFormals) {
            data.maxArgv = args.base() + 1;
        } else {
            // Pad missing arguments with |undefined|.
            for (size_t i = 1; i < args.length() + 2; i++) {
                if (!vals.append(args.base()[i]))
                    return false;
            }

            while (vals.length() < numFormals + 1) {
                if (!vals.append(UndefinedValue()))
                    return false;
            }

            JS_ASSERT(vals.length() >= numFormals + 1);
            data.maxArgv = vals.begin();
        }
    } else {
        data.constructing = false;
        data.numActualArgs = 0;
        data.maxArgc = 1;
        data.maxArgv = state.asExecute()->addressOfThisv();
        data.scopeChain = state.asExecute()->scopeChain();

        data.calleeToken = CalleeToToken(state.script());

        if (state.script()->isForEval() &&
            !(state.asExecute()->type() & InterpreterFrame::GLOBAL))
        {
            ScriptFrameIter iter(cx);
            if (iter.isFunctionFrame())
                data.calleeToken = CalleeToToken(iter.callee());
        }
    }

    return true;
}

IonExecStatus
jit::IonCannon(JSContext *cx, RunState &state)
{
    IonScript *ion = state.script()->ionScript();

    EnterJitData data(cx);
    data.jitcode = ion->method()->raw();

    AutoValueVector vals(cx);
    if (!SetEnterJitData(cx, data, state, vals))
        return IonExec_Error;

    IonExecStatus status = EnterIon(cx, data);

    if (status == IonExec_Ok)
        state.setReturnValue(data.result);

    return status;
}

IonExecStatus
jit::FastInvoke(JSContext *cx, HandleFunction fun, CallArgs &args)
{
    JS_CHECK_RECURSION(cx, return IonExec_Error);

    IonScript *ion = fun->nonLazyScript()->ionScript();
    JitCode *code = ion->method();
    void *jitcode = code->raw();

    JS_ASSERT(jit::IsIonEnabled(cx));
    JS_ASSERT(!ion->bailoutExpected());

    JitActivation activation(cx, /* firstFrameIsConstructing = */false);

    EnterJitCode enter = cx->runtime()->jitRuntime()->enterIon();
    void *calleeToken = CalleeToToken(fun);

    RootedValue result(cx, Int32Value(args.length()));
    JS_ASSERT(args.length() >= fun->nargs());

    CALL_GENERATED_CODE(enter, jitcode, args.length() + 1, args.array() - 1, /* osrFrame = */nullptr,
                        calleeToken, /* scopeChain = */ nullptr, 0, result.address());

    JS_ASSERT(!cx->runtime()->jitRuntime()->hasIonReturnOverride());

    args.rval().set(result);

    JS_ASSERT_IF(result.isMagic(), result.isMagic(JS_ION_ERROR));
    return result.isMagic() ? IonExec_Error : IonExec_Ok;
}

static void
InvalidateActivation(FreeOp *fop, uint8_t *jitTop, bool invalidateAll)
{
    IonSpew(IonSpew_Invalidate, "BEGIN invalidating activation");

    size_t frameno = 1;

    for (JitFrameIterator it(jitTop, SequentialExecution); !it.done(); ++it, ++frameno) {
        JS_ASSERT_IF(frameno == 1, it.type() == JitFrame_Exit);

#ifdef DEBUG
        switch (it.type()) {
          case JitFrame_Exit:
            IonSpew(IonSpew_Invalidate, "#%d exit frame @ %p", frameno, it.fp());
            break;
          case JitFrame_BaselineJS:
          case JitFrame_IonJS:
          {
            JS_ASSERT(it.isScripted());
            const char *type = it.isIonJS() ? "Optimized" : "Baseline";
            IonSpew(IonSpew_Invalidate, "#%d %s JS frame @ %p, %s:%d (fun: %p, script: %p, pc %p)",
                    frameno, type, it.fp(), it.script()->filename(), it.script()->lineno(),
                    it.maybeCallee(), (JSScript *)it.script(), it.returnAddressToFp());
            break;
          }
          case JitFrame_BaselineStub:
            IonSpew(IonSpew_Invalidate, "#%d baseline stub frame @ %p", frameno, it.fp());
            break;
          case JitFrame_Rectifier:
            IonSpew(IonSpew_Invalidate, "#%d rectifier frame @ %p", frameno, it.fp());
            break;
          case JitFrame_Unwound_IonJS:
          case JitFrame_Unwound_BaselineStub:
            MOZ_ASSUME_UNREACHABLE("invalid");
          case JitFrame_Unwound_Rectifier:
            IonSpew(IonSpew_Invalidate, "#%d unwound rectifier frame @ %p", frameno, it.fp());
            break;
          case JitFrame_Entry:
            IonSpew(IonSpew_Invalidate, "#%d entry frame @ %p", frameno, it.fp());
            break;
        }
#endif

        if (!it.isIonJS())
            continue;

        // See if the frame has already been invalidated.
        if (it.checkInvalidation())
            continue;

        JSScript *script = it.script();
        if (!script->hasIonScript())
            continue;

        if (!invalidateAll && !script->ionScript()->invalidated())
            continue;

        IonScript *ionScript = script->ionScript();

        // Purge ICs before we mark this script as invalidated. This will
        // prevent lastJump_ from appearing to be a bogus pointer, just
        // in case anyone tries to read it.
        ionScript->purgeCaches();

        // Clean up any pointers from elsewhere in the runtime to this IonScript
        // which is about to become disconnected from its JSScript.
        ionScript->unlinkFromRuntime(fop);

        // This frame needs to be invalidated. We do the following:
        //
        // 1. Increment the reference counter to keep the ionScript alive
        //    for the invalidation bailout or for the exception handler.
        // 2. Determine safepoint that corresponds to the current call.
        // 3. From safepoint, get distance to the OSI-patchable offset.
        // 4. From the IonScript, determine the distance between the
        //    call-patchable offset and the invalidation epilogue.
        // 5. Patch the OSI point with a call-relative to the
        //    invalidation epilogue.
        //
        // The code generator ensures that there's enough space for us
        // to patch in a call-relative operation at each invalidation
        // point.
        //
        // Note: you can't simplify this mechanism to "just patch the
        // instruction immediately after the call" because things may
        // need to move into a well-defined register state (using move
        // instructions after the call) in to capture an appropriate
        // snapshot after the call occurs.

        ionScript->incref();

        const SafepointIndex *si = ionScript->getSafepointIndex(it.returnAddressToFp());
        JitCode *ionCode = ionScript->method();

        JS::Zone *zone = script->zone();
        if (zone->needsBarrier()) {
            // We're about to remove edges from the JSScript to gcthings
            // embedded in the JitCode. Perform one final trace of the
            // JitCode for the incremental GC, as it must know about
            // those edges.
            ionCode->trace(zone->barrierTracer());
        }
        ionCode->setInvalidated();

        // Write the delta (from the return address offset to the
        // IonScript pointer embedded into the invalidation epilogue)
        // where the safepointed call instruction used to be. We rely on
        // the call sequence causing the safepoint being >= the size of
        // a uint32, which is checked during safepoint index
        // construction.
        CodeLocationLabel dataLabelToMunge(it.returnAddressToFp());
        ptrdiff_t delta = ionScript->invalidateEpilogueDataOffset() -
                          (it.returnAddressToFp() - ionCode->raw());
        Assembler::PatchWrite_Imm32(dataLabelToMunge, Imm32(delta));

        CodeLocationLabel osiPatchPoint = SafepointReader::InvalidationPatchPoint(ionScript, si);
        CodeLocationLabel invalidateEpilogue(ionCode, CodeOffsetLabel(ionScript->invalidateEpilogueOffset()));

        IonSpew(IonSpew_Invalidate, "   ! Invalidate ionScript %p (ref %u) -> patching osipoint %p",
                ionScript, ionScript->refcount(), (void *) osiPatchPoint.raw());
        Assembler::PatchWrite_NearCall(osiPatchPoint, invalidateEpilogue);
    }

    IonSpew(IonSpew_Invalidate, "END invalidating activation");
}

void
jit::StopAllOffThreadCompilations(JSCompartment *comp)
{
    if (!comp->jitCompartment())
        return;
    CancelOffThreadIonCompile(comp, nullptr);
    FinishAllOffThreadCompilations(comp);
}

void
jit::InvalidateAll(FreeOp *fop, Zone *zone)
{
    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next())
        StopAllOffThreadCompilations(comp);

    for (JitActivationIterator iter(fop->runtime()); !iter.done(); ++iter) {
        if (iter->compartment()->zone() == zone) {
            IonSpew(IonSpew_Invalidate, "Invalidating all frames for GC");
            InvalidateActivation(fop, iter.jitTop(), true);
        }
    }
}


void
jit::Invalidate(types::TypeZone &types, FreeOp *fop,
                const Vector<types::RecompileInfo> &invalid, bool resetUses,
                bool cancelOffThread)
{
    IonSpew(IonSpew_Invalidate, "Start invalidation.");

    // Add an invalidation reference to all invalidated IonScripts to indicate
    // to the traversal which frames have been invalidated.
    size_t numInvalidations = 0;
    for (size_t i = 0; i < invalid.length(); i++) {
        const types::CompilerOutput &co = *invalid[i].compilerOutput(types);
        if (!co.isValid())
            continue;

        if (cancelOffThread)
            CancelOffThreadIonCompile(co.script()->compartment(), co.script());

        if (!co.ion())
            continue;

        IonSpew(IonSpew_Invalidate, " Invalidate %s:%u, IonScript %p",
                co.script()->filename(), co.script()->lineno(), co.ion());

        // Keep the ion script alive during the invalidation and flag this
        // ionScript as being invalidated.  This increment is removed by the
        // loop after the calls to InvalidateActivation.
        co.ion()->incref();
        numInvalidations++;
    }

    if (!numInvalidations) {
        IonSpew(IonSpew_Invalidate, " No IonScript invalidation.");
        return;
    }

    for (JitActivationIterator iter(fop->runtime()); !iter.done(); ++iter)
        InvalidateActivation(fop, iter.jitTop(), false);

    // Drop the references added above. If a script was never active, its
    // IonScript will be immediately destroyed. Otherwise, it will be held live
    // until its last invalidated frame is destroyed.
    for (size_t i = 0; i < invalid.length(); i++) {
        types::CompilerOutput &co = *invalid[i].compilerOutput(types);
        if (!co.isValid())
            continue;

        ExecutionMode executionMode = co.mode();
        JSScript *script = co.script();
        IonScript *ionScript = co.ion();
        if (!ionScript)
            continue;

        SetIonScript(script, executionMode, nullptr);
        ionScript->decref(fop);
        co.invalidate();
        numInvalidations--;

        // Wait for the scripts to get warm again before doing another
        // compile, unless either:
        // (1) we are recompiling *because* a script got hot;
        //     (resetUses is false); or,
        // (2) we are invalidating a parallel script.  This is because
        //     the useCount only applies to sequential uses.  Parallel
        //     execution *requires* ion, and so we don't limit it to
        //     methods with a high usage count (though we do check that
        //     the useCount is at least 1 when compiling the transitive
        //     closure of potential callees, to avoid compiling things
        //     that are never run at all).
        if (resetUses && executionMode != ParallelExecution)
            script->resetUseCount();
    }

    // Make sure we didn't leak references by invalidating the same IonScript
    // multiple times in the above loop.
    JS_ASSERT(!numInvalidations);
}

void
jit::Invalidate(JSContext *cx, const Vector<types::RecompileInfo> &invalid, bool resetUses,
                bool cancelOffThread)
{
    jit::Invalidate(cx->zone()->types, cx->runtime()->defaultFreeOp(), invalid, resetUses,
                    cancelOffThread);
}

bool
jit::Invalidate(JSContext *cx, JSScript *script, ExecutionMode mode, bool resetUses,
                bool cancelOffThread)
{
    JS_ASSERT(script->hasIonScript());

    if (cx->runtime()->spsProfiler.enabled()) {
        // Register invalidation with profiler.
        // Format of event payload string:
        //      "<filename>:<lineno>"

        // Get the script filename, if any, and its length.
        const char *filename = script->filename();
        if (filename == nullptr)
            filename = "<unknown>";

        size_t len = strlen(filename) + 20;
        char *buf = js_pod_malloc<char>(len);
        if (!buf)
            return false;

        // Construct the descriptive string.
        JS_snprintf(buf, len, "Invalidate %s:%u", filename, (unsigned int)script->lineno());
        cx->runtime()->spsProfiler.markEvent(buf);
        js_free(buf);
    }

    Vector<types::RecompileInfo> scripts(cx);

    switch (mode) {
      case SequentialExecution:
        JS_ASSERT(script->hasIonScript());
        if (!scripts.append(script->ionScript()->recompileInfo()))
            return false;
        break;
      case ParallelExecution:
        JS_ASSERT(script->hasParallelIonScript());
        if (!scripts.append(script->parallelIonScript()->recompileInfo()))
            return false;
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
    }

    Invalidate(cx, scripts, resetUses, cancelOffThread);
    return true;
}

bool
jit::Invalidate(JSContext *cx, JSScript *script, bool resetUses, bool cancelOffThread)
{
    return Invalidate(cx, script, SequentialExecution, resetUses, cancelOffThread);
}

static void
FinishInvalidationOf(FreeOp *fop, JSScript *script, IonScript *ionScript)
{
    types::TypeZone &types = script->zone()->types;

    // Note: If the script is about to be swept, the compiler output may have
    // already been destroyed.
    if (types::CompilerOutput *output = ionScript->recompileInfo().compilerOutput(types))
        output->invalidate();

    // If this script has Ion code on the stack, invalidated() will return
    // true. In this case we have to wait until destroying it.
    if (!ionScript->invalidated())
        jit::IonScript::Destroy(fop, ionScript);
}

template <ExecutionMode mode>
void
jit::FinishInvalidation(FreeOp *fop, JSScript *script)
{
    // In all cases, nullptr out script->ion or script->parallelIon to avoid
    // re-entry.
    switch (mode) {
      case SequentialExecution:
        if (script->hasIonScript()) {
            IonScript *ion = script->ionScript();
            script->setIonScript(nullptr);
            FinishInvalidationOf(fop, script, ion);
        }
        return;

      case ParallelExecution:
        if (script->hasParallelIonScript()) {
            IonScript *parallelIon = script->parallelIonScript();
            script->setParallelIonScript(nullptr);
            FinishInvalidationOf(fop, script, parallelIon);
        }
        return;

      default:
        MOZ_ASSUME_UNREACHABLE("bad execution mode");
    }
}

template void
jit::FinishInvalidation<SequentialExecution>(FreeOp *fop, JSScript *script);

template void
jit::FinishInvalidation<ParallelExecution>(FreeOp *fop, JSScript *script);

void
jit::MarkValueFromIon(JSRuntime *rt, Value *vp)
{
    gc::MarkValueUnbarriered(&rt->gc.marker, vp, "write barrier");
}

void
jit::MarkShapeFromIon(JSRuntime *rt, Shape **shapep)
{
    gc::MarkShapeUnbarriered(&rt->gc.marker, shapep, "write barrier");
}

void
jit::ForbidCompilation(JSContext *cx, JSScript *script)
{
    ForbidCompilation(cx, script, SequentialExecution);
}

void
jit::ForbidCompilation(JSContext *cx, JSScript *script, ExecutionMode mode)
{
    IonSpew(IonSpew_Abort, "Disabling Ion mode %d compilation of script %s:%d",
            mode, script->filename(), script->lineno());

    CancelOffThreadIonCompile(cx->compartment(), script);

    switch (mode) {
      case SequentialExecution:
        if (script->hasIonScript()) {
            // It is only safe to modify script->ion if the script is not currently
            // running, because JitFrameIterator needs to tell what ionScript to
            // use (either the one on the JSScript, or the one hidden in the
            // breadcrumbs Invalidation() leaves). Therefore, if invalidation
            // fails, we cannot disable the script.
            if (!Invalidate(cx, script, mode, false))
                return;
        }

        script->setIonScript(ION_DISABLED_SCRIPT);
        return;

      case ParallelExecution:
        if (script->hasParallelIonScript()) {
            if (!Invalidate(cx, script, mode, false))
                return;
        }

        script->setParallelIonScript(ION_DISABLED_SCRIPT);
        return;

      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
    }

    MOZ_ASSUME_UNREACHABLE("No such execution mode");
}

AutoFlushICache *
PerThreadData::autoFlushICache() const
{
    return autoFlushICache_;
}

void
PerThreadData::setAutoFlushICache(AutoFlushICache *afc)
{
    autoFlushICache_ = afc;
}

// Set the range for the merging of flushes.  The flushing is deferred until the end of
// the AutoFlushICache context.  Subsequent flushing within this range will is also
// deferred.  This is only expected to be defined once for each AutoFlushICache
// context.  It assumes the range will be flushed is required to be within an
// AutoFlushICache context.
void
AutoFlushICache::setRange(uintptr_t start, size_t len)
{
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS)
    AutoFlushICache *afc = TlsPerThreadData.get()->PerThreadData::autoFlushICache();
    JS_ASSERT(afc);
    JS_ASSERT(!afc->start_);
    IonSpewCont(IonSpew_CacheFlush, "(%x %x):", start, len);

    uintptr_t stop = start + len;
    afc->start_ = start;
    afc->stop_ = stop;
#endif
}

// Flush the instruction cache.
//
// If called within a dynamic AutoFlushICache context and if the range is already pending
// flushing for this AutoFlushICache context then the request is ignored with the
// understanding that it will be flushed on exit from the AutoFlushICache context.
// Otherwise the range is flushed immediately.
//
// Updates outside the current code object are typically the exception so they are flushed
// immediately rather than attempting to merge them.
//
// For efficiency it is expected that all large ranges will be flushed within an
// AutoFlushICache, so check.  If this assertion is hit then it does not necessarily
// indicate a progam fault but it might indicate a lost opportunity to merge cache
// flushing.  It can be corrected by wrapping the call in an AutoFlushICache to context.
void
AutoFlushICache::flush(uintptr_t start, size_t len)
{
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS)
    AutoFlushICache *afc = TlsPerThreadData.get()->PerThreadData::autoFlushICache();
    if (!afc) {
        IonSpewCont(IonSpew_CacheFlush, "#");
        JSC::ExecutableAllocator::cacheFlush((void*)start, len);
        JS_ASSERT(len <= 16);
        return;
    }

    uintptr_t stop = start + len;
    if (start >= afc->start_ && stop <= afc->stop_) {
        // Update is within the pending flush range, so defer to the end of the context.
        IonSpewCont(IonSpew_CacheFlush, afc->inhibit_ ? "-" : "=");
        return;
    }

    IonSpewCont(IonSpew_CacheFlush, afc->inhibit_ ? "x" : "*");
    JSC::ExecutableAllocator::cacheFlush((void *)start, len);
#endif
}

// Flag the current dynamic AutoFlushICache as inhibiting flushing. Useful in error paths
// where the changes are being abandoned.
void
AutoFlushICache::setInhibit()
{
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS)
    AutoFlushICache *afc = TlsPerThreadData.get()->PerThreadData::autoFlushICache();
    JS_ASSERT(afc);
    JS_ASSERT(afc->start_);
    IonSpewCont(IonSpew_CacheFlush, "I");
    afc->inhibit_ = true;
#endif
}

// The common use case is merging cache flushes when preparing a code object.  In this
// case the entire range of the code object is being flushed and as the code is patched
// smaller redundant flushes could occur.  The design allows an AutoFlushICache dynamic
// thread local context to be declared in which the range of the code object can be set
// which defers flushing until the end of this dynamic context.  The redundant flushing
// within this code range is also deferred avoiding redundant flushing.  Flushing outside
// this code range is not affected and proceeds immediately.
//
// In some cases flushing is not necessary, such as when compiling an asm.js module which
// is flushed again when dynamically linked, and also in error paths that abandon the
// code.  Flushing within the set code range can be inhibited within the AutoFlushICache
// dynamic context by setting an inhibit flag.
//
// The JS compiler can be re-entered while within an AutoFlushICache dynamic context and
// it is assumed that code being assembled or patched is not executed before the exit of
// the respective AutoFlushICache dynamic context.
//
AutoFlushICache::AutoFlushICache(const char *nonce, bool inhibit)
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS)
  : start_(0),
    stop_(0),
    name_(nonce),
    inhibit_(inhibit)
#endif
{
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS)
    PerThreadData *pt = TlsPerThreadData.get();
    AutoFlushICache *afc = pt->PerThreadData::autoFlushICache();
    if (afc)
        IonSpew(IonSpew_CacheFlush, "<%s,%s%s ", nonce, afc->name_, inhibit ? " I" : "");
    else
        IonSpewCont(IonSpew_CacheFlush, "<%s%s ", nonce, inhibit ? " I" : "");

    prev_ = afc;
    pt->PerThreadData::setAutoFlushICache(this);
#endif
}

AutoFlushICache::~AutoFlushICache()
{
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS)
    PerThreadData *pt = TlsPerThreadData.get();
    JS_ASSERT(pt->PerThreadData::autoFlushICache() == this);

    if (!inhibit_ && start_)
        JSC::ExecutableAllocator::cacheFlush((void *)start_, size_t(stop_ - start_));

    IonSpewCont(IonSpew_CacheFlush, "%s%s>", name_, start_ ? "" : " U");
    IonSpewFin(IonSpew_CacheFlush);
    pt->PerThreadData::setAutoFlushICache(prev_);
#endif
}

void
jit::PurgeCaches(JSScript *script)
{
    if (script->hasIonScript())
        script->ionScript()->purgeCaches();

    if (script->hasParallelIonScript())
        script->parallelIonScript()->purgeCaches();
}

size_t
jit::SizeOfIonData(JSScript *script, mozilla::MallocSizeOf mallocSizeOf)
{
    size_t result = 0;

    if (script->hasIonScript())
        result += script->ionScript()->sizeOfIncludingThis(mallocSizeOf);

    if (script->hasParallelIonScript())
        result += script->parallelIonScript()->sizeOfIncludingThis(mallocSizeOf);

    return result;
}

void
jit::DestroyIonScripts(FreeOp *fop, JSScript *script)
{
    if (script->hasIonScript())
        jit::IonScript::Destroy(fop, script->ionScript());

    if (script->hasParallelIonScript())
        jit::IonScript::Destroy(fop, script->parallelIonScript());

    if (script->hasBaselineScript())
        jit::BaselineScript::Destroy(fop, script->baselineScript());
}

void
jit::TraceIonScripts(JSTracer* trc, JSScript *script)
{
    if (script->hasIonScript())
        jit::IonScript::Trace(trc, script->ionScript());

    if (script->hasParallelIonScript())
        jit::IonScript::Trace(trc, script->parallelIonScript());

    if (script->hasBaselineScript())
        jit::BaselineScript::Trace(trc, script->baselineScript());
}

bool
jit::RematerializeAllFrames(JSContext *cx, JSCompartment *comp)
{
    for (JitActivationIterator iter(comp->runtimeFromMainThread()); !iter.done(); ++iter) {
        if (iter.activation()->compartment() == comp) {
            for (JitFrameIterator frameIter(iter); !frameIter.done(); ++frameIter) {
                if (!frameIter.isIonJS())
                    continue;
                if (!iter.activation()->asJit()->getRematerializedFrame(cx, frameIter))
                    return false;
            }
        }
    }
    return true;
}

bool
jit::UpdateForDebugMode(JSContext *maybecx, JSCompartment *comp,
                     AutoDebugModeInvalidation &invalidate)
{
    MOZ_ASSERT(invalidate.isFor(comp));

    // Schedule invalidation of all optimized JIT code since debug mode
    // invalidates assumptions.
    invalidate.scheduleInvalidation(comp->debugMode());

    // Recompile on-stack baseline scripts if we have a cx.
    if (maybecx) {
        IonContext ictx(maybecx, nullptr);
        if (!RecompileOnStackBaselineScriptsForDebugMode(maybecx, comp)) {
            js_ReportOutOfMemory(maybecx);
            return false;
        }
    }

    return true;
}

AutoDebugModeInvalidation::~AutoDebugModeInvalidation()
{
    MOZ_ASSERT(!!comp_ != !!zone_);

    if (needInvalidation_ == NoNeed)
        return;

    Zone *zone = zone_ ? zone_ : comp_->zone();
    JSRuntime *rt = zone->runtimeFromMainThread();
    FreeOp *fop = rt->defaultFreeOp();

    if (comp_) {
        StopAllOffThreadCompilations(comp_);
    } else {
        for (CompartmentsInZoneIter comp(zone_); !comp.done(); comp.next())
            StopAllOffThreadCompilations(comp);
    }

    // Don't discard active baseline scripts. They are recompiled for debug
    // mode.
    jit::MarkActiveBaselineScripts(zone);

    for (JitActivationIterator iter(rt); !iter.done(); ++iter) {
        JSCompartment *comp = iter->compartment();
        if (comp_ == comp || zone_ == comp->zone()) {
            IonContext ictx(CompileRuntime::get(rt));
            IonSpew(IonSpew_Invalidate, "Invalidating frames for debug mode toggle");
            InvalidateActivation(fop, iter.jitTop(), true);
        }
    }

    for (gc::ZoneCellIter i(zone, gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        if (script->compartment() == comp_ || zone_) {
            FinishInvalidation<SequentialExecution>(fop, script);
            FinishInvalidation<ParallelExecution>(fop, script);
            FinishDiscardBaselineScript(fop, script);
            script->resetUseCount();
        } else if (script->hasBaselineScript()) {
            script->baselineScript()->resetActive();
        }
    }
}
