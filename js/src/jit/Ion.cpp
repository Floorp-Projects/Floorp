/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/Ion.h"

#include "mozilla/MemoryReporting.h"

#include "jscompartment.h"
#include "jsworkers.h"
#if JS_TRACE_LOGGING
#include "TraceLogging.h"
#endif

#include "gc/Marking.h"
#include "jit/AliasAnalysis.h"
#include "jit/AsmJSModule.h"
#include "jit/AsmJSSignalHandlers.h"
#include "jit/BacktrackingAllocator.h"
#include "jit/BaselineCompiler.h"
#include "jit/BaselineInspector.h"
#include "jit/BaselineJIT.h"
#include "jit/CodeGenerator.h"
#include "jit/CompilerRoot.h"
#include "jit/EdgeCaseAnalysis.h"
#include "jit/EffectiveAddressAnalysis.h"
#include "jit/ExecutionModeInlines.h"
#include "jit/IonAnalysis.h"
#include "jit/IonBuilder.h"
#include "jit/IonCompartment.h"
#include "jit/IonLinker.h"
#include "jit/IonSpewer.h"
#include "jit/LICM.h"
#include "jit/LinearScan.h"
#include "jit/LIR.h"
#include "jit/ParallelSafetyAnalysis.h"
#include "jit/PerfSpewer.h"
#include "jit/RangeAnalysis.h"
#include "jit/StupidAllocator.h"
#include "jit/UnreachableCodeElimination.h"
#include "jit/ValueNumbering.h"
#if defined(JS_CPU_X86)
# include "jit/x86/Lowering-x86.h"
#elif defined(JS_CPU_X64)
# include "jit/x64/Lowering-x64.h"
#elif defined(JS_CPU_ARM)
# include "jit/arm/Lowering-arm.h"
#endif
#include "vm/ForkJoin.h"

#include "jscompartmentinlines.h"
#include "jsgcinlines.h"
#include "jsinferinlines.h"
#include "jsscriptinlines.h"

#include "vm/Shape-inl.h"

using namespace js;
using namespace js::jit;

// Global variables.
IonOptions jit::js_IonOptions;

// Assert that IonCode is gc::Cell aligned.
JS_STATIC_ASSERT(sizeof(IonCode) % gc::CellSize == 0);

#ifdef JS_THREADSAFE
static bool IonTLSInitialized = false;
static unsigned IonTLSIndex;

static inline IonContext *
CurrentIonContext()
{
    return (IonContext *)PR_GetThreadPrivate(IonTLSIndex);
}

bool
jit::SetIonContext(IonContext *ctx)
{
    return PR_SetThreadPrivate(IonTLSIndex, ctx) == PR_SUCCESS;
}

#else

static IonContext *GlobalIonContext;

static inline IonContext *
CurrentIonContext()
{
    return GlobalIonContext;
}

bool
jit::SetIonContext(IonContext *ctx)
{
    GlobalIonContext = ctx;
    return true;
}
#endif

IonContext *
jit::GetIonContext()
{
    JS_ASSERT(CurrentIonContext());
    return CurrentIonContext();
}

IonContext *
jit::MaybeGetIonContext()
{
    return CurrentIonContext();
}

IonContext::IonContext(JSContext *cx, TempAllocator *temp)
  : runtime(cx->runtime()),
    cx(cx),
    compartment(cx->compartment()),
    temp(temp),
    prev_(CurrentIonContext()),
    assemblerCount_(0)
{
    SetIonContext(this);
}

IonContext::IonContext(ExclusiveContext *cx, TempAllocator *temp)
  : runtime(cx->runtime_),
    cx(NULL),
    compartment(NULL),
    temp(temp),
    prev_(CurrentIonContext()),
    assemblerCount_(0)
{
    SetIonContext(this);
}

IonContext::IonContext(JSRuntime *rt, JSCompartment *comp, TempAllocator *temp)
  : runtime(rt),
    cx(NULL),
    compartment(comp),
    temp(temp),
    prev_(CurrentIonContext()),
    assemblerCount_(0)
{
    SetIonContext(this);
}

IonContext::IonContext(JSRuntime *rt)
  : runtime(rt),
    cx(NULL),
    compartment(NULL),
    temp(NULL),
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
#ifdef JS_THREADSAFE
    if (!IonTLSInitialized) {
        PRStatus status = PR_NewThreadPrivateIndex(&IonTLSIndex, NULL);
        if (status != PR_SUCCESS)
            return false;

        IonTLSInitialized = true;
    }
#endif
    CheckLogging();
    CheckPerf();
    return true;
}

IonRuntime::IonRuntime()
  : execAlloc_(NULL),
    ionAlloc_(NULL),
    exceptionTail_(NULL),
    bailoutTail_(NULL),
    enterJIT_(NULL),
    bailoutHandler_(NULL),
    argumentsRectifier_(NULL),
    argumentsRectifierReturnAddr_(NULL),
    parallelArgumentsRectifier_(NULL),
    invalidator_(NULL),
    debugTrapHandler_(NULL),
    functionWrappers_(NULL),
    osrTempData_(NULL),
    flusher_(NULL),
    ionCodeProtected_(false)
{
}

IonRuntime::~IonRuntime()
{
    js_delete(functionWrappers_);
    freeOsrTempData();

    // Note: the operation callback lock is not taken here as IonRuntime is
    // only destroyed along with its containing JSRuntime.
    js_delete(ionAlloc_);
}

bool
IonRuntime::initialize(JSContext *cx)
{
    JS_ASSERT(cx->runtime()->currentThreadOwnsOperationCallbackLock());

    AutoLockForExclusiveAccess lock(cx);
    AutoCompartment ac(cx, cx->atomsCompartment());

    IonContext ictx(cx, NULL);
    AutoFlushCache afc("IonRuntime::initialize");

    execAlloc_ = cx->runtime()->getExecAlloc(cx);
    if (!execAlloc_)
        return false;

    if (!cx->compartment()->ensureIonCompartmentExists(cx))
        return false;

    functionWrappers_ = cx->new_<VMWrapperMap>(cx);
    if (!functionWrappers_ || !functionWrappers_->init())
        return false;

    exceptionTail_ = generateExceptionTailStub(cx);
    if (!exceptionTail_)
        return false;

    bailoutTail_ = generateBailoutTailStub(cx);
    if (!bailoutTail_)
        return false;

    if (cx->runtime()->jitSupportsFloatingPoint) {
        // Initialize some Ion-only stubs that require floating-point support.
        if (!bailoutTables_.reserve(FrameSizeClass::ClassLimit().classId()))
            return false;

        for (uint32_t id = 0;; id++) {
            FrameSizeClass class_ = FrameSizeClass::FromClass(id);
            if (class_ == FrameSizeClass::ClassLimit())
                break;
            bailoutTables_.infallibleAppend((IonCode *)NULL);
            bailoutTables_[id] = generateBailoutTable(cx, id);
            if (!bailoutTables_[id])
                return false;
        }

        bailoutHandler_ = generateBailoutHandler(cx);
        if (!bailoutHandler_)
            return false;

        invalidator_ = generateInvalidator(cx);
        if (!invalidator_)
            return false;
    }

    argumentsRectifier_ = generateArgumentsRectifier(cx, SequentialExecution, &argumentsRectifierReturnAddr_);
    if (!argumentsRectifier_)
        return false;

#ifdef JS_THREADSAFE
    parallelArgumentsRectifier_ = generateArgumentsRectifier(cx, ParallelExecution, NULL);
    if (!parallelArgumentsRectifier_)
        return false;
#endif

    enterJIT_ = generateEnterJIT(cx, EnterJitOptimized);
    if (!enterJIT_)
        return false;

    enterBaselineJIT_ = generateEnterJIT(cx, EnterJitBaseline);
    if (!enterBaselineJIT_)
        return false;

    valuePreBarrier_ = generatePreBarrier(cx, MIRType_Value);
    if (!valuePreBarrier_)
        return false;

    shapePreBarrier_ = generatePreBarrier(cx, MIRType_Shape);
    if (!shapePreBarrier_)
        return false;

    for (VMFunction *fun = VMFunction::functions; fun; fun = fun->next) {
        if (!generateVMWrapper(cx, *fun))
            return false;
    }

    return true;
}

IonCode *
IonRuntime::debugTrapHandler(JSContext *cx)
{
    if (!debugTrapHandler_) {
        // IonRuntime code stubs are shared across compartments and have to
        // be allocated in the atoms compartment.
        AutoLockForExclusiveAccess lock(cx);
        AutoCompartment ac(cx, cx->runtime()->atomsCompartment());
        debugTrapHandler_ = generateDebugTrapHandler(cx);
    }
    return debugTrapHandler_;
}

uint8_t *
IonRuntime::allocateOsrTempData(size_t size)
{
    osrTempData_ = (uint8_t *)js_realloc(osrTempData_, size);
    return osrTempData_;
}

void
IonRuntime::freeOsrTempData()
{
    js_free(osrTempData_);
    osrTempData_ = NULL;
}

JSC::ExecutableAllocator *
IonRuntime::createIonAlloc(JSContext *cx)
{
    JS_ASSERT(cx->runtime()->currentThreadOwnsOperationCallbackLock());

    JSC::AllocationBehavior randomize =
        cx->runtime()->jitHardening ? JSC::AllocationCanRandomize : JSC::AllocationDeterministic;
    ionAlloc_ = js_new<JSC::ExecutableAllocator>(randomize);
    if (!ionAlloc_)
        js_ReportOutOfMemory(cx);
    return ionAlloc_;
}

void
IonRuntime::ensureIonCodeProtected(JSRuntime *rt)
{
    JS_ASSERT(rt->currentThreadOwnsOperationCallbackLock());

    if (!rt->signalHandlersInstalled() || ionCodeProtected_ || !ionAlloc_)
        return;

    // Protect all Ion code in the runtime to trigger an access violation the
    // next time any of it runs on the main thread.
    ionAlloc_->toggleAllCodeAsAccessible(false);
    ionCodeProtected_ = true;
}

bool
IonRuntime::handleAccessViolation(JSRuntime *rt, void *faultingAddress)
{
    if (!rt->signalHandlersInstalled() || !ionAlloc_ || !ionAlloc_->codeContains((char *) faultingAddress))
        return false;

#ifdef JS_THREADSAFE
    // All places where the operation callback lock is taken must either ensure
    // that Ion code memory won't be accessed within, or call ensureIonCodeAccessible
    // to render the memory safe for accessing. Otherwise taking the lock below
    // will deadlock the process.
    JS_ASSERT(!rt->currentThreadOwnsOperationCallbackLock());
#endif

    // Taking this lock is necessary to prevent the interrupting thread from marking
    // the memory as inaccessible while we are patching backedges. This will cause us
    // to SEGV while still inside the signal handler, and the process will terminate.
    JSRuntime::AutoLockForOperationCallback lock(rt);

    ensureIonCodeAccessible(rt);
    return true;
}

void
IonRuntime::ensureIonCodeAccessible(JSRuntime *rt)
{
    JS_ASSERT(rt->currentThreadOwnsOperationCallbackLock());

    // This can only be called on the main thread and while handling signals,
    // which happens on a separate thread in OS X.
#ifndef XP_MACOSX
    JS_ASSERT(CurrentThreadCanAccessRuntime(rt));
#endif

    if (!ionCodeProtected_)
        return;

    // Ion code in the runtime faulted after it was made inaccessible. Reset
    // the code privileges and patch all loop backedges to perform an interrupt
    // check instead.
    ionAlloc_->toggleAllCodeAsAccessible(true);
    ionCodeProtected_ = false;

    if (rt->interrupt) {
        // The interrupt handler needs to be invoked by this thread, but we
        // are inside a signal handler and have no idea what is above us on the
        // stack (probably we are executing Ion code at an arbitrary point, but
        // we could be elsewhere, say repatching a jump for an IonCache).
        // Patch all backedges in the runtime so they will invoke the interrupt
        // handler the next time they execute.
        patchIonBackedges(rt, BackedgeInterruptCheck);
    }
}

void
IonRuntime::patchIonBackedges(JSRuntime *rt, BackedgeTarget target)
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
jit::TriggerOperationCallbackForIonCode(JSRuntime *rt,
                                        JSRuntime::OperationCallbackTrigger trigger)
{
    IonRuntime *ion = rt->ionRuntime();
    if (!ion)
        return;

    JS_ASSERT(rt->currentThreadOwnsOperationCallbackLock());

    // The mechanism for interrupting normal ion code varies between how the
    // interrupt is being triggered.
    switch (trigger) {
      case JSRuntime::TriggerCallbackMainThread:
        // When triggering an interrupt from the main thread, Ion loop
        // backedges can be patched directly. Make sure we don't segv while
        // patching the backedges, to avoid deadlocking inside the signal
        // handler.
        JS_ASSERT(CurrentThreadCanAccessRuntime(rt));
        ion->ensureIonCodeAccessible(rt);
        break;

      case JSRuntime::TriggerCallbackAnyThread:
        // When triggering an interrupt from off the main thread, protect
        // Ion code memory so that the main thread will fault and enter a
        // signal handler when trying to execute the code. The signal
        // handler will unprotect the code and patch loop backedges so
        // that the interrupt handler is invoked afterwards.
        ion->ensureIonCodeProtected(rt);
        break;

      case JSRuntime::TriggerCallbackAnyThreadDontStopIon:
        // When the trigger does not require Ion code to be interrupted,
        // nothing more needs to be done.
        break;

      default:
        MOZ_ASSUME_UNREACHABLE("Bad trigger");
    }
}

IonCompartment::IonCompartment(IonRuntime *rt)
  : rt(rt),
    stubCodes_(NULL),
    baselineCallReturnAddr_(NULL),
    baselineGetPropReturnAddr_(NULL),
    baselineSetPropReturnAddr_(NULL),
    stringConcatStub_(NULL),
    parallelStringConcatStub_(NULL)
{
}

IonCompartment::~IonCompartment()
{
    if (stubCodes_)
        js_delete(stubCodes_);
}

bool
IonCompartment::initialize(JSContext *cx)
{
    stubCodes_ = cx->new_<ICStubCodeMap>(cx);
    if (!stubCodes_ || !stubCodes_->init())
        return false;

    return true;
}

bool
IonCompartment::ensureIonStubsExist(JSContext *cx)
{
    if (!stringConcatStub_) {
        stringConcatStub_ = generateStringConcatStub(cx, SequentialExecution);
        if (!stringConcatStub_)
            return false;
    }

#ifdef JS_THREADSAFE
    if (!parallelStringConcatStub_) {
        parallelStringConcatStub_ = generateStringConcatStub(cx, ParallelExecution);
        if (!parallelStringConcatStub_)
            return false;
    }
#endif

    return true;
}

void
jit::FinishOffThreadBuilder(IonBuilder *builder)
{
    ExecutionMode executionMode = builder->info().executionMode();

    // Clean up if compilation did not succeed.
    if (CompilingOffThread(builder->script(), executionMode)) {
        types::TypeCompartment &types = builder->script()->compartment()->types;
        builder->recompileInfo.compilerOutput(types)->invalidate();
        SetIonScript(builder->script(), executionMode, NULL);
    }

    // The builder is allocated into its LifoAlloc, so destroying that will
    // destroy the builder and all other data accumulated during compilation,
    // except any final codegen (which includes an assembler and needs to be
    // explicitly destroyed).
    js_delete(builder->backgroundCodegen());
    js_delete(builder->temp().lifoAlloc());
}

static inline void
FinishAllOffThreadCompilations(IonCompartment *ion)
{
    OffThreadCompilationVector &compilations = ion->finishedOffThreadCompilations();

    for (size_t i = 0; i < compilations.length(); i++) {
        IonBuilder *builder = compilations[i];
        FinishOffThreadBuilder(builder);
    }
    compilations.clear();
}

/* static */ void
IonRuntime::Mark(JSTracer *trc)
{
    JS_ASSERT(!trc->runtime->isHeapMinorCollecting());
    Zone *zone = trc->runtime->atomsCompartment()->zone();
    for (gc::CellIterUnderGC i(zone, gc::FINALIZE_IONCODE); !i.done(); i.next()) {
        IonCode *code = i.get<IonCode>();
        MarkIonCodeRoot(trc, &code, "wrapper");
    }
}

void
IonCompartment::mark(JSTracer *trc, JSCompartment *compartment)
{
    // Cancel any active or pending off thread compilations. Note that the
    // MIR graph does not hold any nursery pointers, so there's no need to
    // do this for minor GCs.
    JS_ASSERT(!trc->runtime->isHeapMinorCollecting());
    CancelOffThreadIonCompile(compartment, NULL);
    FinishAllOffThreadCompilations(this);

    // Free temporary OSR buffer.
    rt->freeOsrTempData();
}

void
IonCompartment::sweep(FreeOp *fop)
{
    stubCodes_->sweep(fop);

    // If the sweep removed the ICCall_Fallback stub, NULL the baselineCallReturnAddr_ field.
    if (!stubCodes_->lookup(static_cast<uint32_t>(ICStub::Call_Fallback)))
        baselineCallReturnAddr_ = NULL;
    // Similarly for the ICGetProp_Fallback stub.
    if (!stubCodes_->lookup(static_cast<uint32_t>(ICStub::GetProp_Fallback)))
        baselineGetPropReturnAddr_ = NULL;
    if (!stubCodes_->lookup(static_cast<uint32_t>(ICStub::SetProp_Fallback)))
        baselineSetPropReturnAddr_ = NULL;

    if (stringConcatStub_ && !IsIonCodeMarked(stringConcatStub_.unsafeGet()))
        stringConcatStub_ = NULL;

    if (parallelStringConcatStub_ && !IsIonCodeMarked(parallelStringConcatStub_.unsafeGet()))
        parallelStringConcatStub_ = NULL;
}

IonCode *
IonRuntime::getBailoutTable(const FrameSizeClass &frameClass)
{
    JS_ASSERT(frameClass != FrameSizeClass::None());
    return bailoutTables_[frameClass.classId()];
}

IonCode *
IonRuntime::getVMWrapper(const VMFunction &f)
{
    JS_ASSERT(functionWrappers_);
    JS_ASSERT(functionWrappers_->initialized());
    IonRuntime::VMWrapperMap::Ptr p = functionWrappers_->readonlyThreadsafeLookup(&f);
    JS_ASSERT(p);

    return p->value;
}

IonCode *
IonCode::New(JSContext *cx, uint8_t *code, uint32_t bufferSize, JSC::ExecutablePool *pool)
{
    IonCode *codeObj = gc::NewGCThing<IonCode, CanGC>(cx, gc::FINALIZE_IONCODE, sizeof(IonCode), gc::DefaultHeap);
    if (!codeObj) {
        pool->release();
        return NULL;
    }

    new (codeObj) IonCode(code, bufferSize, pool);
    return codeObj;
}

void
IonCode::copyFrom(MacroAssembler &masm)
{
    // Store the IonCode pointer right before the code buffer, so we can
    // recover the gcthing from relocation tables.
    *(IonCode **)(code_ - sizeof(IonCode *)) = this;
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
IonCode::trace(JSTracer *trc)
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
IonCode::finalize(FreeOp *fop)
{
    // Make sure this can't race with an interrupting thread, which may try
    // to read the contents of the pool we are releasing references in.
    JS_ASSERT(fop->runtime()->currentThreadOwnsOperationCallbackLock());

#ifdef DEBUG
    // Buffer can be freed at any time hereafter. Catch use-after-free bugs.
    // Don't do this if the Ion code is protected, as the signal handler will
    // deadlock trying to reaqcuire the operation callback lock.
    if (!fop->runtime()->ionRuntime()->ionCodeProtected())
        JS_POISON(code_, JS_FREE_PATTERN, bufferSize_);
#endif

    // Horrible hack: if we are using perf integration, we don't
    // want to reuse code addresses, so we just leak the memory instead.
    if (PerfEnabled())
        return;

    // Code buffers are stored inside JSC pools.
    // Pools are refcounted. Releasing the pool may free it.
    if (pool_)
        pool_->release();
}

void
IonCode::togglePreBarriers(bool enabled)
{
    uint8_t *start = code_ + preBarrierTableOffset();
    CompactBufferReader reader(start, start + preBarrierTableBytes_);

    while (reader.more()) {
        size_t offset = reader.readUnsigned();
        CodeLocationLabel loc(this, offset);
        if (enabled)
            Assembler::ToggleToCmp(loc);
        else
            Assembler::ToggleToJmp(loc);
    }
}

void
IonCode::readBarrier(IonCode *code)
{
#ifdef JSGC_INCREMENTAL
    if (!code)
        return;

    Zone *zone = code->zone();
    if (zone->needsBarrier())
        MarkIonCodeUnbarriered(zone->barrierTracer(), &code, "ioncode read barrier");
#endif
}

void
IonCode::writeBarrierPre(IonCode *code)
{
#ifdef JSGC_INCREMENTAL
    if (!code || !code->runtimeFromMainThread()->needsBarrier())
        return;

    Zone *zone = code->zone();
    if (zone->needsBarrier())
        MarkIonCodeUnbarriered(zone->barrierTracer(), &code, "ioncode write barrier");
#endif
}

IonScript::IonScript()
  : method_(NULL),
    deoptTable_(NULL),
    osrPc_(NULL),
    osrEntryOffset_(0),
    skipArgCheckEntryOffset_(0),
    invalidateEpilogueOffset_(0),
    invalidateEpilogueDataOffset_(0),
    numBailouts_(0),
    numExceptionBailouts_(0),
    hasUncompiledCallTarget_(false),
    hasSPSInstrumentation_(false),
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
    snapshotsSize_(0),
    constantTable_(0),
    constantEntries_(0),
    scriptList_(0),
    scriptEntries_(0),
    callTargetList_(0),
    callTargetEntries_(0),
    backedgeList_(0),
    backedgeEntries_(0),
    refcount_(0),
    recompileInfo_(),
    osrPcMismatchCounter_(0),
    dependentAsmJSModules(NULL)
{
}

static const int DataAlignment = sizeof(void *);

IonScript *
IonScript::New(JSContext *cx, uint32_t frameSlots, uint32_t frameSize, size_t snapshotsSize,
               size_t bailoutEntries, size_t constants, size_t safepointIndices,
               size_t osiIndices, size_t cacheEntries, size_t runtimeSize,
               size_t safepointsSize, size_t scriptEntries,
               size_t callTargetEntries, size_t backedgeEntries)
{
    if (snapshotsSize >= MAX_BUFFER_SIZE ||
        (bailoutEntries >= MAX_BUFFER_SIZE / sizeof(uint32_t)))
    {
        js_ReportOutOfMemory(cx);
        return NULL;
    }

    // This should not overflow on x86, because the memory is already allocated
    // *somewhere* and if their total overflowed there would be no memory left
    // at all.
    size_t paddedSnapshotsSize = AlignBytes(snapshotsSize, DataAlignment);
    size_t paddedBailoutSize = AlignBytes(bailoutEntries * sizeof(uint32_t), DataAlignment);
    size_t paddedConstantsSize = AlignBytes(constants * sizeof(Value), DataAlignment);
    size_t paddedSafepointIndicesSize = AlignBytes(safepointIndices * sizeof(SafepointIndex), DataAlignment);
    size_t paddedOsiIndicesSize = AlignBytes(osiIndices * sizeof(OsiIndex), DataAlignment);
    size_t paddedCacheEntriesSize = AlignBytes(cacheEntries * sizeof(uint32_t), DataAlignment);
    size_t paddedRuntimeSize = AlignBytes(runtimeSize, DataAlignment);
    size_t paddedSafepointSize = AlignBytes(safepointsSize, DataAlignment);
    size_t paddedScriptSize = AlignBytes(scriptEntries * sizeof(JSScript *), DataAlignment);
    size_t paddedCallTargetSize = AlignBytes(callTargetEntries * sizeof(JSScript *), DataAlignment);
    size_t paddedBackedgeSize = AlignBytes(backedgeEntries * sizeof(PatchableBackedge), DataAlignment);
    size_t bytes = paddedSnapshotsSize +
                   paddedBailoutSize +
                   paddedConstantsSize +
                   paddedSafepointIndicesSize+
                   paddedOsiIndicesSize +
                   paddedCacheEntriesSize +
                   paddedRuntimeSize +
                   paddedSafepointSize +
                   paddedScriptSize +
                   paddedCallTargetSize +
                   paddedBackedgeSize;
    uint8_t *buffer = (uint8_t *)cx->malloc_(sizeof(IonScript) + bytes);
    if (!buffer)
        return NULL;

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
    script->snapshotsSize_ = snapshotsSize;
    offsetCursor += paddedSnapshotsSize;

    script->constantTable_ = offsetCursor;
    script->constantEntries_ = constants;
    offsetCursor += paddedConstantsSize;

    script->scriptList_ = offsetCursor;
    script->scriptEntries_ = scriptEntries;
    offsetCursor += paddedScriptSize;

    script->callTargetList_ = offsetCursor;
    script->callTargetEntries_ = callTargetEntries;
    offsetCursor += paddedCallTargetSize;

    script->backedgeList_ = offsetCursor;
    script->backedgeEntries_ = backedgeEntries;
    offsetCursor += paddedBackedgeSize;

    script->frameSlots_ = frameSlots;
    script->frameSize_ = frameSize;

    script->recompileInfo_ = cx->compartment()->types.compiledInfo;

    return script;
}

void
IonScript::trace(JSTracer *trc)
{
    if (method_)
        MarkIonCode(trc, &method_, "method");

    if (deoptTable_)
        MarkIonCode(trc, &deoptTable_, "deoptimizationTable");

    for (size_t i = 0; i < numConstants(); i++)
        gc::MarkValue(trc, &getConstant(i), "constant");

    // No write barrier is needed for the call target list, as it's attached
    // at compilation time and is read only.
    for (size_t i = 0; i < callTargetEntries(); i++)
        gc::MarkScriptUnbarriered(trc, &callTargetList()[i], "callTarget");
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
    JS_ASSERT(writer->size() == snapshotsSize_);
    memcpy((uint8_t *)this + snapshots_, writer->buffer(), snapshotsSize_);
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
IonScript::copyScriptEntries(JSScript **scripts)
{
    for (size_t i = 0; i < scriptEntries_; i++)
        scriptList()[i] = scripts[i];
}

void
IonScript::copyCallTargetEntries(JSScript **callTargets)
{
    for (size_t i = 0; i < callTargetEntries_; i++)
        callTargetList()[i] = callTargets[i];
}

void
IonScript::copyPatchableBackedges(JSContext *cx, IonCode *code,
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
        // interrupt immediately as the operation callback lock is held here.
        PatchJump(backedge, cx->runtime()->interrupt ? interruptCheck : loopHeader);

        cx->runtime()->ionRuntime()->addPatchableBackedge(patchableBackedge);
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
        getCache(i).updateBaseAddress(method_, masm);
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
    script->destroyBackedges(fop->runtime());
    script->detachDependentAsmJSModules(fop);
    fop->free_(script);
}

void
IonScript::toggleBarriers(bool enabled)
{
    method()->togglePreBarriers(enabled);
}

void
IonScript::purgeCaches(Zone *zone)
{
    // Don't reset any ICs if we're invalidated, otherwise, repointing the
    // inline jump could overwrite an invalidation marker. These ICs can
    // no longer run, however, the IC slow paths may be active on the stack.
    // ICs therefore are required to check for invalidation before patching,
    // to ensure the same invariant.
    if (invalidated())
        return;

    JSRuntime *rt = zone->runtimeFromMainThread();
    IonContext ictx(rt);
    AutoFlushCache afc("purgeCaches", rt->ionRuntime());
    for (size_t i = 0; i < numCaches(); i++)
        getCache(i).reset();
}

void
IonScript::destroyCaches()
{
    for (size_t i = 0; i < numCaches(); i++)
        getCache(i).destroy();
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
IonScript::detachDependentAsmJSModules(FreeOp *fop) {
    if (!dependentAsmJSModules)
        return;
    for (size_t i = 0; i < dependentAsmJSModules->length(); i++) {
        DependentAsmJSModuleExit exit = dependentAsmJSModules->begin()[i];
        exit.module->detachIonCompilation(exit.exitIndex);
    }
    fop->delete_(dependentAsmJSModules);
    dependentAsmJSModules = NULL;
}

void
IonScript::destroyBackedges(JSRuntime *rt)
{
    for (size_t i = 0; i < backedgeEntries_; i++) {
        PatchableBackedge *backedge = &backedgeList()[i];
        rt->ionRuntime()->removePatchableBackedge(backedge);
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
    IonContext ictx(rt);
    if (!rt->hasIonRuntime())
        return;

    AutoFlushCache afc("ToggleBarriers", rt->ionRuntime());
    for (gc::CellIterUnderGC i(zone, gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        if (script->hasIonScript())
            script->ionScript()->toggleBarriers(needs);
        if (script->hasBaselineScript())
            script->baselineScript()->toggleBarriers(needs);
    }

    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
        if (comp->ionCompartment())
            comp->ionCompartment()->toggleBaselineStubBarriers(needs);
    }
}

namespace js {
namespace jit {

bool
OptimizeMIR(MIRGenerator *mir)
{
    MIRGraph &graph = mir->graph();

    IonSpewPass("BuildSSA");
    AssertBasicGraphCoherency(graph);

    if (mir->shouldCancel("Start"))
        return false;

    if (!SplitCriticalEdges(graph))
        return false;
    IonSpewPass("Split Critical Edges");
    AssertGraphCoherency(graph);

    if (mir->shouldCancel("Split Critical Edges"))
        return false;

    if (!RenumberBlocks(graph))
        return false;
    IonSpewPass("Renumber Blocks");
    AssertGraphCoherency(graph);

    if (mir->shouldCancel("Renumber Blocks"))
        return false;

    if (!BuildDominatorTree(graph))
        return false;
    // No spew: graph not changed.

    if (mir->shouldCancel("Dominator Tree"))
        return false;

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

    // This pass also removes copies.
    if (!ApplyTypeInformation(mir, graph))
        return false;
    IonSpewPass("Apply types");
    AssertExtendedGraphCoherency(graph);

    if (mir->shouldCancel("Apply types"))
        return false;

    if (graph.entryBlock()->info().executionMode() == ParallelExecution) {
        ParallelSafetyAnalysis analysis(mir, graph);
        if (!analysis.analyze())
            return false;
    }

    // Alias analysis is required for LICM and GVN so that we don't move
    // loads across stores.
    if (js_IonOptions.licm || js_IonOptions.gvn) {
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

    if (js_IonOptions.gvn) {
        ValueNumberer gvn(mir, graph, js_IonOptions.gvnIsOptimistic);
        if (!gvn.analyze())
            return false;
        IonSpewPass("GVN");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("GVN"))
            return false;
    }

    if (js_IonOptions.uce) {
        UnreachableCodeElimination uce(mir, graph);
        if (!uce.analyze())
            return false;
        IonSpewPass("UCE");
        AssertExtendedGraphCoherency(graph);
    }

    if (mir->shouldCancel("UCE"))
        return false;

    if (js_IonOptions.licm) {
        // LICM can hoist instructions from conditional branches and trigger
        // repeated bailouts. Disable it if this script is known to bailout
        // frequently.
        JSScript *script = mir->info().script();
        if (!script || !script->hadFrequentBailouts) {
            LICM licm(mir, graph);
            if (!licm.analyze())
                return false;
            IonSpewPass("LICM");
            AssertExtendedGraphCoherency(graph);

            if (mir->shouldCancel("LICM"))
                return false;
        }
    }

    if (js_IonOptions.rangeAnalysis) {
        RangeAnalysis r(graph);
        if (!r.addBetaNobes())
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

        if (!r.removeBetaNobes())
            return false;
        IonSpewPass("De-Beta");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("RA De-Beta"))
            return false;

        if (!r.truncate())
            return false;
        IonSpewPass("Truncate Doubles");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("Truncate Doubles"))
            return false;
    }

    if (js_IonOptions.eaa) {
        EffectiveAddressAnalysis eaa(graph);
        if (!eaa.analyze())
            return false;
        IonSpewPass("Effective Address Analysis");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("Effective Address Analysis"))
            return false;
    }

    if (!EliminateDeadCode(mir, graph))
        return false;
    IonSpewPass("DCE");
    AssertExtendedGraphCoherency(graph);

    if (mir->shouldCancel("DCE"))
        return false;

    // Passes after this point must not move instructions; these analyses
    // depend on knowing the final order in which instructions will execute.

    if (js_IonOptions.edgeCaseAnalysis) {
        EdgeCaseAnalysis edgeCaseAnalysis(mir, graph);
        if (!edgeCaseAnalysis.analyzeLate())
            return false;
        IonSpewPass("Edge Case Analysis (Late)");
        AssertGraphCoherency(graph);

        if (mir->shouldCancel("Edge Case Analysis (Late)"))
            return false;
    }

    // Note: check elimination has to run after all other passes that move
    // instructions. Since check uses are replaced with the actual index, code
    // motion after this pass could incorrectly move a load or store before its
    // bounds check.
    if (!EliminateRedundantChecks(graph))
        return false;
    IonSpewPass("Bounds Check Elimination");
    AssertGraphCoherency(graph);

    return true;
}

LIRGraph *
GenerateLIR(MIRGenerator *mir)
{
    MIRGraph &graph = mir->graph();

    LIRGraph *lir = mir->temp().lifoAlloc()->new_<LIRGraph>(&graph);
    if (!lir)
        return NULL;

    LIRGenerator lirgen(mir, graph, *lir);
    if (!lirgen.generate())
        return NULL;
    IonSpewPass("Generate LIR");

    if (mir->shouldCancel("Generate LIR"))
        return NULL;

    AllocationIntegrityState integrity(*lir);

    switch (js_IonOptions.registerAllocator) {
      case RegisterAllocator_LSRA: {
#ifdef DEBUG
        integrity.record();
#endif

        LinearScanAllocator regalloc(mir, &lirgen, *lir);
        if (!regalloc.go())
            return NULL;

#ifdef DEBUG
        integrity.check(false);
#endif

        IonSpewPass("Allocate Registers [LSRA]", &regalloc);
        break;
      }

      case RegisterAllocator_Backtracking: {
#ifdef DEBUG
        integrity.record();
#endif

        BacktrackingAllocator regalloc(mir, &lirgen, *lir);
        if (!regalloc.go())
            return NULL;

#ifdef DEBUG
        integrity.check(false);
#endif

        IonSpewPass("Allocate Registers [Backtracking]");
        break;
      }

      case RegisterAllocator_Stupid: {
        // Use the integrity checker to populate safepoint information, so
        // run it in all builds.
        integrity.record();

        StupidAllocator regalloc(mir, &lirgen, *lir);
        if (!regalloc.go())
            return NULL;
        if (!integrity.check(true))
            return NULL;
        IonSpewPass("Allocate Registers [Stupid]");
        break;
      }

      default:
        MOZ_ASSUME_UNREACHABLE("Bad regalloc");
    }

    if (mir->shouldCancel("Allocate Registers"))
        return NULL;

    // Now that all optimization and register allocation is done, re-introduce
    // critical edges to avoid unnecessary jumps.
    if (!UnsplitEdges(lir))
        return NULL;
    IonSpewPass("Unsplit Critical Edges");
    AssertBasicGraphCoherency(graph);

    return lir;
}

CodeGenerator *
GenerateCode(MIRGenerator *mir, LIRGraph *lir, MacroAssembler *maybeMasm)
{
    CodeGenerator *codegen = js_new<CodeGenerator>(mir, lir, maybeMasm);
    if (!codegen)
        return NULL;

    if (mir->compilingAsmJS()) {
        if (!codegen->generateAsmJS()) {
            js_delete(codegen);
            return NULL;
        }
    } else {
        if (!codegen->generate()) {
            js_delete(codegen);
            return NULL;
        }
    }

    return codegen;
}

CodeGenerator *
CompileBackEnd(MIRGenerator *mir, MacroAssembler *maybeMasm)
{
    if (!OptimizeMIR(mir))
        return NULL;

    LIRGraph *lir = GenerateLIR(mir);
    if (!lir)
        return NULL;

    return GenerateCode(mir, lir, maybeMasm);
}

void
AttachFinishedCompilations(JSContext *cx)
{
#ifdef JS_THREADSAFE
    IonCompartment *ion = cx->compartment()->ionCompartment();
    if (!ion || !cx->runtime()->workerThreadState)
        return;

    AutoLockWorkerThreadState lock(*cx->runtime()->workerThreadState);

    OffThreadCompilationVector &compilations = ion->finishedOffThreadCompilations();

    // Incorporate any off thread compilations which have finished, failed or
    // have been cancelled.
    while (!compilations.empty()) {
        IonBuilder *builder = compilations.popCopy();

        if (CodeGenerator *codegen = builder->backgroundCodegen()) {
            RootedScript script(cx, builder->script());
            IonContext ictx(cx, &builder->temp());

            // Root the assembler until the builder is finished below. As it
            // was constructed off thread, the assembler has not been rooted
            // previously, though any GC activity would discard the builder.
            codegen->masm.constructRoot(cx);

            types::AutoEnterAnalysis enterTypes(cx);

            ExecutionMode executionMode = builder->info().executionMode();
            types::AutoEnterCompilation enterCompiler(cx, CompilerOutputKind(executionMode));
            enterCompiler.initExisting(builder->recompileInfo);

            bool success;
            {
                // Release the worker thread lock and root the compiler for GC.
                AutoTempAllocatorRooter root(cx, &builder->temp());
                AutoUnlockWorkerThreadState unlock(cx->runtime());
                AutoFlushCache afc("AttachFinishedCompilations");
                success = codegen->link();
            }

            if (!success) {
                // Silently ignore OOM during code generation, we're at an
                // operation callback and can't propagate failures.
                cx->clearPendingException();
            }
        }

        FinishOffThreadBuilder(builder);
    }

    compilations.clear();
#endif
}

static const size_t BUILDER_LIFO_ALLOC_PRIMARY_CHUNK_SIZE = 1 << 12;

static inline bool
OffThreadCompilationAvailable(JSContext *cx)
{
    // Even if off thread compilation is enabled, compilation must still occur
    // on the main thread in some cases. Do not compile off thread during an
    // incremental GC, as this may trip incremental read barriers.
    //
    // Skip off thread compilation if PC count profiling is enabled, as
    // CodeGenerator::maybeCreateScriptCounts will not attach script profiles
    // when running off thread.
    //
    // Also skip off thread compilation if the SPS profiler is enabled, as it
    // stores strings in the spsProfiler data structure, which is not protected
    // by a lock.
    return OffThreadIonCompilationEnabled(cx->runtime())
        && cx->runtime()->gcIncrementalState == gc::NO_INCREMENTAL
        && !cx->runtime()->profilingScripts
        && !cx->runtime()->spsProfiler.enabled();
}

static AbortReason
IonCompile(JSContext *cx, JSScript *script,
           BaselineFrame *baselineFrame, jsbytecode *osrPc, bool constructing,
           ExecutionMode executionMode)
{
#if JS_TRACE_LOGGING
    AutoTraceLog logger(TraceLogging::defaultLogger(),
                        TraceLogging::ION_COMPILE_START,
                        TraceLogging::ION_COMPILE_STOP,
                        script);
#endif

    if (!script->ensureRanAnalysis(cx))
        return AbortReason_Alloc;

    // Try-finally is not yet supported.
    if (script->analysis()->hasTryFinally()) {
        IonSpew(IonSpew_Abort, "Has try-finally.");
        return AbortReason_Disable;
    }

    LifoAlloc *alloc = cx->new_<LifoAlloc>(BUILDER_LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
    if (!alloc)
        return AbortReason_Alloc;

    ScopedJSDeletePtr<LifoAlloc> autoDelete(alloc);

    TempAllocator *temp = alloc->new_<TempAllocator>(alloc);
    if (!temp)
        return AbortReason_Alloc;

    IonContext ictx(cx, temp);

    types::AutoEnterAnalysis enter(cx);

    if (!cx->compartment()->ensureIonCompartmentExists(cx))
        return AbortReason_Alloc;

    if (!cx->compartment()->ionCompartment()->ensureIonStubsExist(cx))
        return AbortReason_Alloc;

    MIRGraph *graph = alloc->new_<MIRGraph>(temp);
    CompileInfo *info = alloc->new_<CompileInfo>(script, script->function(), osrPc, constructing,
                                                 executionMode);
    if (!info)
        return AbortReason_Alloc;

    BaselineInspector inspector(cx, script);

    AutoFlushCache afc("IonCompile");

    types::AutoEnterCompilation enterCompiler(cx, CompilerOutputKind(executionMode));
    if (!enterCompiler.init(script))
        return AbortReason_Disable;

    AutoTempAllocatorRooter root(cx, temp);

    IonBuilder *builder = alloc->new_<IonBuilder>(cx, temp, graph, &inspector, info, baselineFrame);
    if (!builder)
        return AbortReason_Alloc;

    JS_ASSERT(!GetIonScript(builder->script(), executionMode));
    JS_ASSERT(CanIonCompile(builder->script(), executionMode));

    RootedScript builderScript(cx, builder->script());
    IonSpewNewFunction(graph, builderScript);

    if (!builder->build()) {
        IonSpew(IonSpew_Abort, "Builder failed to build.");
        return builder->abortReason();
    }
    builder->clearForBackEnd();

    // If possible, compile the script off thread.
    if (OffThreadCompilationAvailable(cx)) {
        SetIonScript(builder->script(), executionMode, ION_COMPILING_SCRIPT);

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

    bool success = codegen->link();

    IonSpewEndFunction();

    return success ? AbortReason_NoAbort : AbortReason_Disable;
}

static bool
TooManyArguments(unsigned nargs)
{
    return (nargs >= SNAPSHOT_MAX_NARGS || nargs > js_IonOptions.maxStackArgs);
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

    if (!script->analyzedArgsUsage() && !script->ensureRanAnalysis(cx)) {
        IonSpew(IonSpew_Abort, "OOM under ensureRanAnalysis");
        return false;
    }

    if (osr && script->needsArgsObj()) {
        // OSR-ing into functions with arguments objects is not supported.
        IonSpew(IonSpew_Abort, "OSR script has argsobj");
        return false;
    }

    if (!script->compileAndGo) {
        IonSpew(IonSpew_Abort, "not compile-and-go");
        return false;
    }

    return true;
}

// Longer scripts can only be compiled off thread, as these compilations
// can be expensive and stall the main thread for too long.
static const uint32_t MAX_OFF_THREAD_SCRIPT_SIZE = 100000;
static const uint32_t MAX_MAIN_THREAD_SCRIPT_SIZE = 2000;
static const uint32_t MAX_MAIN_THREAD_LOCALS_AND_ARGS = 256;

static MethodStatus
CheckScriptSize(JSContext *cx, JSScript* script)
{
    if (!js_IonOptions.limitScriptSize)
        return Method_Compiled;

    if (script->length > MAX_OFF_THREAD_SCRIPT_SIZE) {
        // Some scripts are so large we never try to Ion compile them.
        IonSpew(IonSpew_Abort, "Script too large (%u bytes)", script->length);
        return Method_CantCompile;
    }

    uint32_t numLocalsAndArgs = analyze::TotalSlots(script);
    if (script->length > MAX_MAIN_THREAD_SCRIPT_SIZE ||
        numLocalsAndArgs > MAX_MAIN_THREAD_LOCALS_AND_ARGS)
    {
        if (OffThreadIonCompilationEnabled(cx->runtime())) {
            // Even if off thread compilation is enabled, there are cases where
            // compilation must still occur on the main thread. Don't compile
            // in these cases (except when profiling scripts, as compilations
            // occurring with profiling should reflect those without), but do
            // not forbid compilation so that the script may be compiled later.
            if (!OffThreadCompilationAvailable(cx) && !cx->runtime()->profilingScripts) {
                IonSpew(IonSpew_Abort,
                        "Script too large for main thread, skipping (%u bytes) (%u locals/args)",
                        script->length, numLocalsAndArgs);
                return Method_Skipped;
            }
        } else {
            IonSpew(IonSpew_Abort, "Script too large (%u bytes) (%u locals/args)",
                    script->length, numLocalsAndArgs);
            return Method_CantCompile;
        }
    }

    return Method_Compiled;
}

bool
CanIonCompileScript(JSContext *cx, HandleScript script, bool osr)
{
    if (!script->canIonCompile() || !CheckScript(cx, script, osr))
        return false;

    return CheckScriptSize(cx, script) == Method_Compiled;
}

static MethodStatus
Compile(JSContext *cx, HandleScript script, BaselineFrame *osrFrame, jsbytecode *osrPc,
        bool constructing, ExecutionMode executionMode)
{
    JS_ASSERT(jit::IsIonEnabled(cx));
    JS_ASSERT(jit::IsBaselineEnabled(cx));
    JS_ASSERT_IF(osrPc != NULL, (JSOp)*osrPc == JSOP_LOOPENTRY);

    if (executionMode == SequentialExecution && !script->hasBaselineScript())
        return Method_Skipped;

    if (cx->compartment()->debugMode()) {
        IonSpew(IonSpew_Abort, "debugging");
        return Method_CantCompile;
    }

    if (!CheckScript(cx, script, bool(osrPc))) {
        IonSpew(IonSpew_Abort, "Aborted compilation of %s:%d", script->filename(), script->lineno);
        return Method_CantCompile;
    }

    MethodStatus status = CheckScriptSize(cx, script);
    if (status != Method_Compiled) {
        IonSpew(IonSpew_Abort, "Aborted compilation of %s:%d", script->filename(), script->lineno);
        return status;
    }

    IonScript *scriptIon = GetIonScript(script, executionMode);
    if (scriptIon) {
        if (!scriptIon->method())
            return Method_CantCompile;
        return Method_Compiled;
    }

    if (executionMode == SequentialExecution) {
        // Use getUseCount instead of incUseCount to avoid bumping the
        // use count twice.
        if (script->getUseCount() < js_IonOptions.usesBeforeCompile)
            return Method_Skipped;
    }

    AbortReason reason = IonCompile(cx, script, osrFrame, osrPc, constructing, executionMode);
    if (reason == AbortReason_Disable)
        return Method_CantCompile;

    // Compilation succeeded or we invalidated right away or an inlining/alloc abort
    return HasIonScript(script, executionMode) ? Method_Compiled : Method_Skipped;
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
    if (!js_IonOptions.osr)
        return Method_Skipped;

    // Mark as forbidden if frame can't be handled.
    if (!CheckFrame(osrFrame)) {
        ForbidCompilation(cx, script);
        return Method_CantCompile;
    }

    // Attempt compilation. Returns Method_Compiled if already compiled.
    RootedScript rscript(cx, script);
    MethodStatus status = Compile(cx, rscript, osrFrame, pc, isConstructing, SequentialExecution);
    if (status != Method_Compiled) {
        if (status == Method_CantCompile)
            ForbidCompilation(cx, script);
        return status;
    }

    if (script->ionScript()->osrPc() != pc) {
        // If we keep failing to enter the script due to an OSR pc mismatch,
        // invalidate the script to force a recompile.
        uint32_t count = script->ionScript()->incrOsrPcMismatchCounter();

        if (count > js_IonOptions.osrPcMismatchesBeforeRecompile) {
            if (!Invalidate(cx, script, SequentialExecution, true))
                return Method_Error;
        }
        return Method_Skipped;
    }

    script->ionScript()->resetOsrPcMismatchCounter();

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

        if (invoke.constructing() && invoke.args().thisv().isPrimitive()) {
            RootedScript scriptRoot(cx, script);
            RootedObject callee(cx, &invoke.args().callee());
            RootedObject obj(cx, CreateThisForFunction(cx, callee, invoke.useNewType()));
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
    if (js_IonOptions.eagerCompilation && !rscript->hasBaselineScript()) {
        MethodStatus status = CanEnterBaselineMethod(cx, state);
        if (status != Method_Compiled)
            return status;
    }

    // Attempt compilation. Returns Method_Compiled if already compiled.
    bool constructing = state.isInvoke() && state.asInvoke()->constructing();
    MethodStatus status = Compile(cx, rscript, NULL, NULL, constructing, SequentialExecution);
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
    MethodStatus status = Compile(cx, script, frame, NULL, isConstructing, SequentialExecution);
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

    MethodStatus status = Compile(cx, script, NULL, NULL, false, ParallelExecution);
    if (status != Method_Compiled) {
        if (status == Method_CantCompile)
            ForbidCompilation(cx, script, ParallelExecution);
        return status;
    }

    // This can GC, so afterward, script->parallelIon is
    // not guaranteed to be valid.
    if (!cx->runtime()->ionRuntime()->enterIon())
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
            script.get(), script->filename(), script->lineno);
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
    if (numActualArgs < script->function()->nargs)
        return Method_Skipped;

    if (!cx->compartment()->ensureIonCompartmentExists(cx))
        return Method_Error;

    // This can GC, so afterward, script->ion is not guaranteed to be valid.
    if (!cx->runtime()->ionRuntime()->enterIon())
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

    EnterIonCode enter = cx->runtime()->ionRuntime()->enterIon();

    // Caller must construct |this| before invoking the Ion function.
    JS_ASSERT_IF(data.constructing, data.maxArgv[0].isObject());

    data.result.setInt32(data.numActualArgs);
    {
        AssertCompartmentUnchanged pcc(cx);
        IonContext ictx(cx, NULL);
        JitActivation activation(cx, data.constructing);
        JSAutoResolveFlags rf(cx, RESOLVE_INFER);
        AutoFlushInhibitor afi(cx->compartment()->ionCompartment());

        // Single transition point from Interpreter to Baseline.
        enter(data.jitcode, data.maxArgc, data.maxArgv, /* osrFrame = */NULL, data.calleeToken,
              /* scopeChain = */ NULL, 0, data.result.address());
    }

    JS_ASSERT(!cx->runtime()->hasIonReturnOverride());

    // Jit callers wrap primitive constructor return.
    if (!data.result.isMagic() && data.constructing && data.result.isPrimitive())
        data.result = data.maxArgv[0];

    // Release temporary buffer used for OSR into Ion.
    cx->runtime()->getIonRuntime(cx)->freeOsrTempData();

    JS_ASSERT_IF(data.result.isMagic(), data.result.isMagic(JS_ION_ERROR));
    return data.result.isMagic() ? IonExec_Error : IonExec_Ok;
}

bool
jit::SetEnterJitData(JSContext *cx, EnterJitData &data, RunState &state, AutoValueVector &vals)
{
    data.osrFrame = NULL;

    if (state.isInvoke()) {
        CallArgs &args = state.asInvoke()->args();
        unsigned numFormals = state.script()->function()->nargs;
        data.constructing = state.asInvoke()->constructing();
        data.numActualArgs = args.length();
        data.maxArgc = Max(args.length(), numFormals) + 1;
        data.scopeChain = NULL;
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
            !(state.asExecute()->type() & StackFrame::GLOBAL))
        {
            ScriptFrameIter iter(cx);
            if (iter.isFunctionFrame())
                data.calleeToken = CalleeToToken(iter.callee());
        }
    }

    return true;
}

IonExecStatus
jit::Cannon(JSContext *cx, RunState &state)
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
    IonCode *code = ion->method();
    void *jitcode = code->raw();

    JS_ASSERT(jit::IsIonEnabled(cx));
    JS_ASSERT(!ion->bailoutExpected());

    JitActivation activation(cx, /* firstFrameIsConstructing = */false);

    EnterIonCode enter = cx->runtime()->ionRuntime()->enterIon();
    void *calleeToken = CalleeToToken(fun);

    RootedValue result(cx, Int32Value(args.length()));
    JS_ASSERT(args.length() >= fun->nargs);

    JSAutoResolveFlags rf(cx, RESOLVE_INFER);
    enter(jitcode, args.length() + 1, args.array() - 1, NULL, calleeToken,
          /* scopeChain = */ NULL, 0, result.address());

    JS_ASSERT(!cx->runtime()->hasIonReturnOverride());

    args.rval().set(result);

    JS_ASSERT_IF(result.isMagic(), result.isMagic(JS_ION_ERROR));
    return result.isMagic() ? IonExec_Error : IonExec_Ok;
}

static void
InvalidateActivation(FreeOp *fop, uint8_t *ionTop, bool invalidateAll)
{
    IonSpew(IonSpew_Invalidate, "BEGIN invalidating activation");

    size_t frameno = 1;

    for (IonFrameIterator it(ionTop); !it.done(); ++it, ++frameno) {
        JS_ASSERT_IF(frameno == 1, it.type() == IonFrame_Exit);

#ifdef DEBUG
        switch (it.type()) {
          case IonFrame_Exit:
            IonSpew(IonSpew_Invalidate, "#%d exit frame @ %p", frameno, it.fp());
            break;
          case IonFrame_BaselineJS:
          case IonFrame_OptimizedJS:
          {
            JS_ASSERT(it.isScripted());
            const char *type = it.isOptimizedJS() ? "Optimized" : "Baseline";
            IonSpew(IonSpew_Invalidate, "#%d %s JS frame @ %p, %s:%d (fun: %p, script: %p, pc %p)",
                    frameno, type, it.fp(), it.script()->filename(), it.script()->lineno,
                    it.maybeCallee(), (JSScript *)it.script(), it.returnAddressToFp());
            break;
          }
          case IonFrame_BaselineStub:
            IonSpew(IonSpew_Invalidate, "#%d baseline stub frame @ %p", frameno, it.fp());
            break;
          case IonFrame_Rectifier:
            IonSpew(IonSpew_Invalidate, "#%d rectifier frame @ %p", frameno, it.fp());
            break;
          case IonFrame_Unwound_OptimizedJS:
          case IonFrame_Unwound_BaselineStub:
            MOZ_ASSUME_UNREACHABLE("invalid");
          case IonFrame_Unwound_Rectifier:
            IonSpew(IonSpew_Invalidate, "#%d unwound rectifier frame @ %p", frameno, it.fp());
            break;
          case IonFrame_Osr:
            IonSpew(IonSpew_Invalidate, "#%d osr frame @ %p", frameno, it.fp());
            break;
          case IonFrame_Entry:
            IonSpew(IonSpew_Invalidate, "#%d entry frame @ %p", frameno, it.fp());
            break;
        }
#endif

        if (!it.isOptimizedJS())
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
        ionScript->purgeCaches(script->zone());

        // The writes to the executable buffer below may clobber backedge
        // jumps, so make sure that those backedges are unlinked from the
        // runtime and not reclobbered with garbage if an interrupt is
        // triggered.
        ionScript->destroyBackedges(fop->runtime());

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
        IonCode *ionCode = ionScript->method();

        JS::Zone *zone = script->zone();
        if (zone->needsBarrier()) {
            // We're about to remove edges from the JSScript to gcthings
            // embedded in the IonCode. Perform one final trace of the
            // IonCode for the incremental GC, as it must know about
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
        Assembler::patchWrite_Imm32(dataLabelToMunge, Imm32(delta));

        CodeLocationLabel osiPatchPoint = SafepointReader::InvalidationPatchPoint(ionScript, si);
        CodeLocationLabel invalidateEpilogue(ionCode, ionScript->invalidateEpilogueOffset());

        IonSpew(IonSpew_Invalidate, "   ! Invalidate ionScript %p (ref %u) -> patching osipoint %p",
                ionScript, ionScript->refcount(), (void *) osiPatchPoint.raw());
        Assembler::patchWrite_NearCall(osiPatchPoint, invalidateEpilogue);
    }

    IonSpew(IonSpew_Invalidate, "END invalidating activation");
}

void
jit::InvalidateAll(FreeOp *fop, Zone *zone)
{
    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
        if (!comp->ionCompartment())
            continue;
        CancelOffThreadIonCompile(comp, NULL);
        FinishAllOffThreadCompilations(comp->ionCompartment());
    }

    for (JitActivationIterator iter(fop->runtime()); !iter.done(); ++iter) {
        if (iter.activation()->compartment()->zone() == zone) {
            IonContext ictx(fop->runtime());
            AutoFlushCache afc("InvalidateAll", fop->runtime()->ionRuntime());
            IonSpew(IonSpew_Invalidate, "Invalidating all frames for GC");
            InvalidateActivation(fop, iter.jitTop(), true);
        }
    }
}


void
jit::Invalidate(types::TypeCompartment &types, FreeOp *fop,
                const Vector<types::RecompileInfo> &invalid, bool resetUses)
{
    IonSpew(IonSpew_Invalidate, "Start invalidation.");
    AutoFlushCache afc ("Invalidate");

    // Add an invalidation reference to all invalidated IonScripts to indicate
    // to the traversal which frames have been invalidated.
    bool anyInvalidation = false;
    for (size_t i = 0; i < invalid.length(); i++) {
        const types::CompilerOutput &co = *invalid[i].compilerOutput(types);
        switch (co.kind()) {
          case types::CompilerOutput::Ion:
          case types::CompilerOutput::ParallelIon:
            JS_ASSERT(co.isValid());
            IonSpew(IonSpew_Invalidate, " Invalidate %s:%u, IonScript %p",
                    co.script->filename(), co.script->lineno, co.ion());

            // Keep the ion script alive during the invalidation and flag this
            // ionScript as being invalidated.  This increment is removed by the
            // loop after the calls to InvalidateActivation.
            co.ion()->incref();
            anyInvalidation = true;
        }
    }

    if (!anyInvalidation) {
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
        ExecutionMode executionMode = SequentialExecution;
        switch (co.kind()) {
          case types::CompilerOutput::Ion:
            break;
          case types::CompilerOutput::ParallelIon:
            executionMode = ParallelExecution;
            break;
        }
        JS_ASSERT(co.isValid());
        JSScript *script = co.script;
        IonScript *ionScript = GetIonScript(script, executionMode);

        SetIonScript(script, executionMode, NULL);
        ionScript->detachDependentAsmJSModules(fop);
        ionScript->decref(fop);
        co.invalidate();

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
}

void
jit::Invalidate(JSContext *cx, const Vector<types::RecompileInfo> &invalid, bool resetUses)
{
    jit::Invalidate(cx->compartment()->types, cx->runtime()->defaultFreeOp(), invalid, resetUses);
}

bool
jit::Invalidate(JSContext *cx, JSScript *script, ExecutionMode mode, bool resetUses)
{
    JS_ASSERT(script->hasIonScript());

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
    }

    Invalidate(cx, scripts, resetUses);
    return true;
}

bool
jit::Invalidate(JSContext *cx, JSScript *script, bool resetUses)
{
    return Invalidate(cx, script, SequentialExecution, resetUses);
}

static void
FinishInvalidationOf(FreeOp *fop, JSScript *script, IonScript *ionScript, bool parallel)
{
    // In all cases, NULL out script->ion or script->parallelIon to avoid
    // re-entry.
    if (parallel)
        script->setParallelIonScript(NULL);
    else
        script->setIonScript(NULL);

    // If this script has Ion code on the stack, invalidation() will return
    // true. In this case we have to wait until destroying it.
    if (!ionScript->invalidated()) {
        types::TypeCompartment &types = script->compartment()->types;
        ionScript->recompileInfo().compilerOutput(types)->invalidate();

        jit::IonScript::Destroy(fop, ionScript);
    }
}

void
jit::FinishInvalidation(FreeOp *fop, JSScript *script)
{
    if (script->hasIonScript())
        FinishInvalidationOf(fop, script, script->ionScript(), false);

    if (script->hasParallelIonScript())
        FinishInvalidationOf(fop, script, script->parallelIonScript(), true);
}

void
jit::MarkValueFromIon(JSRuntime *rt, Value *vp)
{
    gc::MarkValueUnbarriered(&rt->gcMarker, vp, "write barrier");
}

void
jit::MarkShapeFromIon(JSRuntime *rt, Shape **shapep)
{
    gc::MarkShapeUnbarriered(&rt->gcMarker, shapep, "write barrier");
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
            mode, script->filename(), script->lineno);

    CancelOffThreadIonCompile(cx->compartment(), script);

    switch (mode) {
      case SequentialExecution:
        if (script->hasIonScript()) {
            // It is only safe to modify script->ion if the script is not currently
            // running, because IonFrameIterator needs to tell what ionScript to
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
    }

    MOZ_ASSUME_UNREACHABLE("No such execution mode");
}

uint32_t
jit::UsesBeforeIonRecompile(JSScript *script, jsbytecode *pc)
{
    JS_ASSERT(pc == script->code || JSOp(*pc) == JSOP_LOOPENTRY);

    uint32_t minUses = js_IonOptions.usesBeforeCompile;

    // If the script is too large to compile on the main thread, we can still
    // compile it off thread. In these cases, increase the use count threshold
    // to improve the compilation's type information and hopefully avoid later
    // recompilation.

    if (script->length > MAX_MAIN_THREAD_SCRIPT_SIZE)
        minUses = minUses * (script->length / (double) MAX_MAIN_THREAD_SCRIPT_SIZE);

    uint32_t numLocalsAndArgs = analyze::TotalSlots(script);
    if (numLocalsAndArgs > MAX_MAIN_THREAD_LOCALS_AND_ARGS)
        minUses = minUses * (numLocalsAndArgs / (double) MAX_MAIN_THREAD_LOCALS_AND_ARGS);

    if (JSOp(*pc) != JSOP_LOOPENTRY || js_IonOptions.eagerCompilation)
        return minUses;

    // It's more efficient to enter outer loops, rather than inner loops, via OSR.
    // To accomplish this, we use a slightly higher threshold for inner loops.
    // Note that the loop depth is always > 0 so we will prefer non-OSR over OSR.
    uint32_t loopDepth = GET_UINT8(pc);
    JS_ASSERT(loopDepth > 0);
    return minUses + loopDepth * 100;
}

void
AutoFlushCache::updateTop(uintptr_t p, size_t len)
{
    IonContext *ictx = GetIonContext();
    IonRuntime *irt = ictx->runtime->ionRuntime();
    if (!irt || !irt->flusher())
        JSC::ExecutableAllocator::cacheFlush((void*)p, len);
    else
        irt->flusher()->update(p, len);
}

AutoFlushCache::AutoFlushCache(const char *nonce, IonRuntime *rt)
  : start_(0),
    stop_(0),
    name_(nonce),
    used_(false)
{
    if (CurrentIonContext() != NULL)
        rt = GetIonContext()->runtime->ionRuntime();

    // If a compartment isn't available, then be a nop, nobody will ever see this flusher
    if (rt) {
        if (rt->flusher())
            IonSpew(IonSpew_CacheFlush, "<%s ", nonce);
        else
            IonSpewCont(IonSpew_CacheFlush, "<%s ", nonce);
        rt->setFlusher(this);
    } else {
        IonSpew(IonSpew_CacheFlush, "<%s DEAD>\n", nonce);
    }
    runtime_ = rt;
}

AutoFlushInhibitor::AutoFlushInhibitor(IonCompartment *ic)
  : ic_(ic),
    afc(NULL)
{
    if (!ic)
        return;
    afc = ic->flusher();
    // Ensure that called functions get a fresh flusher
    ic->setFlusher(NULL);
    // Ensure the current flusher has been flushed
    if (afc) {
        afc->flushAnyway();
        IonSpewCont(IonSpew_CacheFlush, "}");
    }
}
AutoFlushInhibitor::~AutoFlushInhibitor()
{
    if (!ic_)
        return;
    JS_ASSERT(ic_->flusher() == NULL);
    // Ensure any future modifications are recorded
    ic_->setFlusher(afc);
    if (afc)
        IonSpewCont(IonSpew_CacheFlush, "{");
}

void
jit::PurgeCaches(JSScript *script, Zone *zone)
{
    if (script->hasIonScript())
        script->ionScript()->purgeCaches(zone);

    if (script->hasParallelIonScript())
        script->parallelIonScript()->purgeCaches(zone);
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
