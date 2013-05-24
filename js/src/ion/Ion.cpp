/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaselineJIT.h"
#include "BaselineCompiler.h"
#include "BaselineInspector.h"
#include "Ion.h"
#include "IonAnalysis.h"
#include "IonBuilder.h"
#include "IonLinker.h"
#include "IonSpewer.h"
#include "LIR.h"
#include "AliasAnalysis.h"
#include "LICM.h"
#include "ValueNumbering.h"
#include "EdgeCaseAnalysis.h"
#include "RangeAnalysis.h"
#include "LinearScan.h"
#include "ParallelArrayAnalysis.h"
#include "jscompartment.h"
#include "vm/ThreadPool.h"
#include "vm/ForkJoin.h"
#include "IonCompartment.h"
#include "PerfSpewer.h"
#include "CodeGenerator.h"
#include "jsworkers.h"
#include "BacktrackingAllocator.h"
#include "StupidAllocator.h"
#include "UnreachableCodeElimination.h"
#include "EffectiveAddressAnalysis.h"

#if defined(JS_CPU_X86)
# include "x86/Lowering-x86.h"
#elif defined(JS_CPU_X64)
# include "x64/Lowering-x64.h"
#elif defined(JS_CPU_ARM)
# include "arm/Lowering-arm.h"
#endif
#include "gc/Marking.h"
#include "jsgcinlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "vm/Stack-inl.h"
#include "ion/IonFrames-inl.h"
#include "ion/CompilerRoot.h"
#include "ExecutionModeInlines.h"

#if JS_TRACE_LOGGING
#include "TraceLogging.h"
#endif

using namespace js;
using namespace js::ion;

// Global variables.
IonOptions ion::js_IonOptions;

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
ion::SetIonContext(IonContext *ctx)
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
ion::SetIonContext(IonContext *ctx)
{
    GlobalIonContext = ctx;
    return true;
}
#endif

IonContext *
ion::GetIonContext()
{
    JS_ASSERT(CurrentIonContext());
    return CurrentIonContext();
}

IonContext *
ion::MaybeGetIonContext()
{
    return CurrentIonContext();
}

IonContext::IonContext(JSContext *cx, TempAllocator *temp)
  : runtime(cx->runtime),
    cx(cx),
    compartment(cx->compartment),
    temp(temp),
    prev_(CurrentIonContext()),
    assemblerCount_(0)
{
    SetIonContext(this);
}

IonContext::IonContext(JSCompartment *comp, TempAllocator *temp)
  : runtime(comp->rt),
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
ion::InitializeIon()
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
    enterJIT_(NULL),
    bailoutHandler_(NULL),
    argumentsRectifier_(NULL),
    argumentsRectifierReturnAddr_(NULL),
    parallelArgumentsRectifier_(NULL),
    invalidator_(NULL),
    debugTrapHandler_(NULL),
    functionWrappers_(NULL),
    osrTempData_(NULL),
    flusher_(NULL)
{
}

IonRuntime::~IonRuntime()
{
    js_delete(functionWrappers_);
    freeOsrTempData();
}

bool
IonRuntime::initialize(JSContext *cx)
{
    AutoEnterAtomsCompartment ac(cx);

    IonContext ictx(cx, NULL);
    AutoFlushCache afc("IonRuntime::initialize");

    execAlloc_ = cx->runtime->getExecAlloc(cx);
    if (!execAlloc_)
        return false;

    if (!cx->compartment->ensureIonCompartmentExists(cx))
        return false;

    functionWrappers_ = cx->new_<VMWrapperMap>(cx);
    if (!functionWrappers_ || !functionWrappers_->init())
        return false;

    if (cx->runtime->jitSupportsFloatingPoint) {
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
        AutoEnterAtomsCompartment ac(cx);
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

IonCompartment::IonCompartment(IonRuntime *rt)
  : rt(rt),
    stubCodes_(NULL),
    baselineCallReturnAddr_(NULL),
    stringConcatStub_(NULL)
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
        stringConcatStub_ = generateStringConcatStub(cx);
        if (!stringConcatStub_)
            return false;
    }

    return true;
}

void
ion::FinishOffThreadBuilder(IonBuilder *builder)
{
    JS_ASSERT(builder->info().executionMode() == SequentialExecution);

    // Clean up if compilation did not succeed.
    if (builder->script()->isIonCompilingOffThread()) {
        types::TypeCompartment &types = builder->script()->compartment()->types;
        builder->recompileInfo.compilerOutput(types)->invalidate();
        builder->script()->setIonScript(NULL);
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
    Zone *zone = trc->runtime->atomsCompartment->zone();
    for (gc::CellIterUnderGC i(zone, gc::FINALIZE_IONCODE); !i.done(); i.next()) {
        IonCode *code = i.get<IonCode>();
        MarkIonCodeRoot(trc, &code, "wrapper");
    }
}

void
IonCompartment::mark(JSTracer *trc, JSCompartment *compartment)
{
    // Cancel any active or pending off thread compilations.
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

    if (stringConcatStub_ && !IsIonCodeMarked(stringConcatStub_.unsafeGet()))
        stringConcatStub_ = NULL;
}

IonCode *
IonCompartment::getBailoutTable(const FrameSizeClass &frameClass)
{
    JS_ASSERT(frameClass != FrameSizeClass::None());
    return rt->bailoutTables_[frameClass.classId()];
}

IonCode *
IonCompartment::getVMWrapper(const VMFunction &f)
{
    JS_ASSERT(rt->functionWrappers_);
    JS_ASSERT(rt->functionWrappers_->initialized());
    IonRuntime::VMWrapperMap::Ptr p = rt->functionWrappers_->readonlyThreadsafeLookup(&f);
    JS_ASSERT(p);

    return p->value;
}

IonActivation::IonActivation(JSContext *cx, StackFrame *fp)
  : cx_(cx),
    compartment_(cx->compartment),
    prev_(cx->mainThread().ionActivation),
    entryfp_(fp),
    prevIonTop_(cx->mainThread().ionTop),
    prevIonJSContext_(cx->mainThread().ionJSContext),
    prevpc_(NULL)
{
    if (fp)
        fp->setRunningInIon();
    cx->mainThread().ionJSContext = cx;
    cx->mainThread().ionActivation = this;
}

IonActivation::~IonActivation()
{
    JS_ASSERT(cx_->mainThread().ionActivation == this);

    if (entryfp_)
        entryfp_->clearRunningInIon();
    cx_->mainThread().ionActivation = prev();
    cx_->mainThread().ionTop = prevIonTop_;
    cx_->mainThread().ionJSContext = prevIonJSContext_;
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
    // Buffer can be freed at any time hereafter. Catch use-after-free bugs.
    JS_POISON(code_, JS_FREE_PATTERN, bufferSize_);

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
    if (!code || !code->runtime()->needsBarrier())
        return;

    Zone *zone = code->zone();
    if (zone->needsBarrier())
        MarkIonCodeUnbarriered(zone->barrierTracer(), &code, "ioncode write barrier");
#endif
}

void
IonCode::writeBarrierPost(IonCode *code, void *addr)
{
#ifdef JSGC_INCREMENTAL
    // Nothing to do.
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
    hasInvalidatedCallTarget_(false),
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
    refcount_(0),
    recompileInfo_(),
    osrPcMismatchCounter_(0)
{
}

static const int DataAlignment = 4;

IonScript *
IonScript::New(JSContext *cx, uint32_t frameSlots, uint32_t frameSize, size_t snapshotsSize,
               size_t bailoutEntries, size_t constants, size_t safepointIndices,
               size_t osiIndices, size_t cacheEntries, size_t runtimeSize,
               size_t safepointsSize, size_t scriptEntries,
               size_t callTargetEntries)
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
    size_t bytes = paddedSnapshotsSize +
                   paddedBailoutSize +
                   paddedConstantsSize +
                   paddedSafepointIndicesSize+
                   paddedOsiIndicesSize +
                   paddedCacheEntriesSize +
                   paddedRuntimeSize +
                   paddedSafepointSize +
                   paddedScriptSize +
                   paddedCallTargetSize;
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

    script->frameSlots_ = frameSlots;
    script->frameSize_ = frameSize;

    script->recompileInfo_ = cx->compartment->types.compiledInfo;

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

    JS_NOT_REACHED("displacement not found.");
    return NULL;
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

    JS_NOT_REACHED("Failed to find OSI point return address");
    return NULL;
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

    IonContext ictx(zone->rt);
    AutoFlushCache afc("purgeCaches", zone->rt->ionRuntime());
    for (size_t i = 0; i < numCaches(); i++)
        getCache(i).reset();
}

void
IonScript::destroyCaches()
{
    for (size_t i = 0; i < numCaches(); i++)
        getCache(i).destroy();
}

void
ion::ToggleBarriers(JS::Zone *zone, bool needs)
{
    IonContext ictx(zone->rt);
    if (!zone->rt->hasIonRuntime())
        return;

    AutoFlushCache afc("ToggleBarriers", zone->rt->ionRuntime());
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
namespace ion {

bool
OptimizeMIR(MIRGenerator *mir)
{
    IonSpewPass("BuildSSA");
    // Note: don't call AssertGraphCoherency before SplitCriticalEdges,
    // the graph is not in RPO at this point.

    MIRGraph &graph = mir->graph();

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

    // This must occur before any code elimination.
    if (!EliminatePhis(mir, graph, AggressiveObservability))
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

        if (!r.analyze())
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
        JS_NOT_REACHED("Bad regalloc");
    }

    if (mir->shouldCancel("Allocate Registers"))
        return NULL;

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

class SequentialCompileContext {
public:
    ExecutionMode executionMode() {
        return SequentialExecution;
    }

    MethodStatus checkScriptSize(JSContext *cx, JSScript *script);
    AbortReason compile(IonBuilder *builder, MIRGraph *graph,
                        ScopedJSDeletePtr<LifoAlloc> &autoDelete);
};

void
AttachFinishedCompilations(JSContext *cx)
{
#ifdef JS_THREADSAFE
    IonCompartment *ion = cx->compartment->ionCompartment();
    if (!ion || !cx->runtime->workerThreadState)
        return;

    AutoLockWorkerThreadState lock(cx->runtime);

    OffThreadCompilationVector &compilations = ion->finishedOffThreadCompilations();

    // Incorporate any off thread compilations which have finished, failed or
    // have been cancelled, and destroy JM jitcode for any compilations which
    // succeeded, to allow entering the Ion code from the interpreter.
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
                AutoUnlockWorkerThreadState unlock(cx->runtime);
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

template <typename CompileContext>
static AbortReason
IonCompile(JSContext *cx, JSScript *script,
           AbstractFramePtr fp, jsbytecode *osrPc, bool constructing,
           CompileContext &compileContext)
{
#if JS_TRACE_LOGGING
    AutoTraceLog logger(TraceLogging::defaultLogger(),
                        TraceLogging::ION_COMPILE_START,
                        TraceLogging::ION_COMPILE_STOP,
                        script);
#endif

    if (!script->ensureRanAnalysis(cx))
        return AbortReason_Alloc;

    LifoAlloc *alloc = cx->new_<LifoAlloc>(BUILDER_LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
    if (!alloc)
        return AbortReason_Alloc;

    ScopedJSDeletePtr<LifoAlloc> autoDelete(alloc);

    TempAllocator *temp = alloc->new_<TempAllocator>(alloc);
    if (!temp)
        return AbortReason_Alloc;

    IonContext ictx(cx, temp);

    types::AutoEnterAnalysis enter(cx);

    if (!cx->compartment->ensureIonCompartmentExists(cx))
        return AbortReason_Alloc;

    if (!cx->compartment->ionCompartment()->ensureIonStubsExist(cx))
        return AbortReason_Alloc;

    MIRGraph *graph = alloc->new_<MIRGraph>(temp);
    ExecutionMode executionMode = compileContext.executionMode();
    CompileInfo *info = alloc->new_<CompileInfo>(script, script->function(), osrPc, constructing,
                                                 executionMode);
    if (!info)
        return AbortReason_Alloc;

    BaselineInspector inspector(cx, script);

    AutoFlushCache afc("IonCompile");

    types::AutoEnterCompilation enterCompiler(cx, CompilerOutputKind(executionMode));
    if (!enterCompiler.init(script, false, 0))
        return AbortReason_Disable;

    AutoTempAllocatorRooter root(cx, temp);

    IonBuilder *builder = alloc->new_<IonBuilder>(cx, temp, graph, &inspector, info, fp);
    if (!builder)
        return AbortReason_Alloc;

    AbortReason abortReason  = compileContext.compile(builder, graph, autoDelete);
    if (abortReason != AbortReason_NoAbort)
        IonSpew(IonSpew_Abort, "IM Compilation failed.");

    return abortReason;
}

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
    return OffThreadCompilationEnabled(cx)
        && cx->runtime->gcIncrementalState == gc::NO_INCREMENTAL
        && !cx->runtime->profilingScripts
        && !cx->runtime->spsProfiler.enabled();
}

AbortReason
SequentialCompileContext::compile(IonBuilder *builder, MIRGraph *graph,
                                  ScopedJSDeletePtr<LifoAlloc> &autoDelete)
{
    JS_ASSERT(!builder->script()->hasIonScript());
    JS_ASSERT(builder->script()->canIonCompile());

    JSContext *cx = GetIonContext()->cx;
    RootedScript builderScript(cx, builder->script());
    IonSpewNewFunction(graph, builderScript);

    if (!builder->build()) {
        IonSpew(IonSpew_Abort, "Builder failed to build.");
        return builder->abortReason();
    }
    builder->clearForBackEnd();

    // If possible, compile the script off thread.
    if (OffThreadCompilationAvailable(cx)) {
        builder->script()->setIonScript(ION_COMPILING_SCRIPT);

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
CheckFrame(AbstractFramePtr fp)
{
    if (fp.isEvalFrame()) {
        // Eval frames are not yet supported. Supporting this will require new
        // logic in pushBailoutFrame to deal with linking prev.
        // Additionally, JSOP_DEFVAR support will require baking in isEvalFrame().
        IonSpew(IonSpew_Abort, "eval frame");
        return false;
    }

    if (fp.isGeneratorFrame()) {
        // Err... no.
        IonSpew(IonSpew_Abort, "generator frame");
        return false;
    }

    if (fp.isDebuggerFrame()) {
        IonSpew(IonSpew_Abort, "debugger frame");
        return false;
    }

    // This check is to not overrun the stack. Eventually, we will want to
    // handle this when we support JSOP_ARGUMENTS or function calls.
    if (fp.isFunctionFrame() &&
        (fp.numActualArgs() >= SNAPSHOT_MAX_NARGS ||
         fp.numActualArgs() > js_IonOptions.maxStackArgs))
    {
        IonSpew(IonSpew_Abort, "too many actual args");
        return false;
    }

    return true;
}

static bool
CheckScript(JSContext *cx, JSScript *script, bool osr)
{
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

MethodStatus
SequentialCompileContext::checkScriptSize(JSContext *cx, JSScript *script)
{
    if (!js_IonOptions.limitScriptSize)
        return Method_Compiled;

    // Longer scripts can only be compiled off thread, as these compilations
    // can be expensive and stall the main thread for too long.
    static const uint32_t MAX_MAIN_THREAD_SCRIPT_SIZE = 2000;
    static const uint32_t MAX_OFF_THREAD_SCRIPT_SIZE = 20000;
    static const uint32_t MAX_LOCALS_AND_ARGS = 256;

    if (script->length > MAX_OFF_THREAD_SCRIPT_SIZE) {
        IonSpew(IonSpew_Abort, "Script too large (%u bytes)", script->length);
        return Method_CantCompile;
    }

    if (script->length > MAX_MAIN_THREAD_SCRIPT_SIZE) {
        if (OffThreadCompilationEnabled(cx)) {
            // Even if off thread compilation is enabled, there are cases where
            // compilation must still occur on the main thread. Don't compile
            // in these cases (except when profiling scripts, as compilations
            // occurring with profiling should reflect those without), but do
            // not forbid compilation so that the script may be compiled later.
            if (!OffThreadCompilationAvailable(cx) && !cx->runtime->profilingScripts) {
                IonSpew(IonSpew_Abort, "Script too large for main thread, skipping (%u bytes)", script->length);
                return Method_Skipped;
            }
        } else {
            IonSpew(IonSpew_Abort, "Script too large (%u bytes)", script->length);
            return Method_CantCompile;
        }
    }

    uint32_t numLocalsAndArgs = analyze::TotalSlots(script);
    if (numLocalsAndArgs > MAX_LOCALS_AND_ARGS) {
        IonSpew(IonSpew_Abort, "Too many locals and arguments (%u)", numLocalsAndArgs);
        return Method_CantCompile;
    }

    return Method_Compiled;
}

bool
CanIonCompileScript(JSContext *cx, HandleScript script, bool osr)
{
    if (!script->canIonCompile() || !CheckScript(cx, script, osr))
        return false;

    SequentialCompileContext compileContext;
    return compileContext.checkScriptSize(cx, script) == Method_Compiled;
}

template <typename CompileContext>
static MethodStatus
Compile(JSContext *cx, HandleScript script, AbstractFramePtr fp, jsbytecode *osrPc,
        bool constructing, CompileContext &compileContext)
{
    JS_ASSERT(ion::IsEnabled(cx));
    JS_ASSERT(ion::IsBaselineEnabled(cx));
    JS_ASSERT_IF(osrPc != NULL, (JSOp)*osrPc == JSOP_LOOPENTRY);

    ExecutionMode executionMode = compileContext.executionMode();

    if (executionMode == SequentialExecution && !script->hasBaselineScript())
        return Method_Skipped;

    if (cx->compartment->debugMode()) {
        IonSpew(IonSpew_Abort, "debugging");
        return Method_CantCompile;
    }

    if (!CheckScript(cx, script, bool(osrPc))) {
        IonSpew(IonSpew_Abort, "Aborted compilation of %s:%d", script->filename(), script->lineno);
        return Method_CantCompile;
    }

    MethodStatus status = compileContext.checkScriptSize(cx, script);
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

    AbortReason reason = IonCompile(cx, script, fp, osrPc, constructing, compileContext);
    if (reason == AbortReason_Disable)
        return Method_CantCompile;

    // Compilation succeeded or we invalidated right away or an inlining/alloc abort
    return HasIonScript(script, executionMode) ? Method_Compiled : Method_Skipped;
}

} // namespace ion
} // namespace js

// Decide if a transition from interpreter execution to Ion code should occur.
// May compile or recompile the target JSScript.
MethodStatus
ion::CanEnterAtBranch(JSContext *cx, JSScript *script, AbstractFramePtr fp,
                      jsbytecode *pc, bool isConstructing)
{
    JS_ASSERT(ion::IsEnabled(cx));
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
    if (!CheckFrame(fp)) {
        ForbidCompilation(cx, script);
        return Method_CantCompile;
    }

    // Attempt compilation. Returns Method_Compiled if already compiled.
    SequentialCompileContext compileContext;
    RootedScript rscript(cx, script);
    MethodStatus status = Compile(cx, rscript, fp, pc, isConstructing, compileContext);
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
ion::CanEnter(JSContext *cx, JSScript *script, AbstractFramePtr fp, bool isConstructing)
{
    JS_ASSERT(ion::IsEnabled(cx));

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
    if (isConstructing && fp.thisValue().isPrimitive()) {
        RootedScript scriptRoot(cx, script);
        RootedObject callee(cx, fp.callee());
        RootedObject obj(cx, CreateThisForFunction(cx, callee, fp.useNewType()));
        if (!obj || !ion::IsEnabled(cx)) // Note: OOM under CreateThis can disable TI.
            return Method_Skipped;
        fp.thisValue().setObject(*obj);
        script = scriptRoot;
    }

    // Mark as forbidden if frame can't be handled.
    if (!CheckFrame(fp)) {
        ForbidCompilation(cx, script);
        return Method_CantCompile;
    }

    // If --ion-eager is used, compile with Baseline first, so that we
    // can directly enter IonMonkey.
    if (js_IonOptions.eagerCompilation && !script->hasBaselineScript()) {
        bool newType = isConstructing && fp.asStackFrame()->useNewType();
        MethodStatus status = CanEnterBaselineJIT(cx, script, fp.asStackFrame(), newType);
        if (status != Method_Compiled)
            return status;
    }

    // Attempt compilation. Returns Method_Compiled if already compiled.
    SequentialCompileContext compileContext;
    RootedScript rscript(cx, script);
    MethodStatus status = Compile(cx, rscript, fp, NULL, isConstructing, compileContext);
    if (status != Method_Compiled) {
        if (status == Method_CantCompile)
            ForbidCompilation(cx, script);
        return status;
    }

    return Method_Compiled;
}

MethodStatus
ion::CompileFunctionForBaseline(JSContext *cx, HandleScript script, AbstractFramePtr fp,
                                bool isConstructing)
{
    JS_ASSERT(ion::IsEnabled(cx));
    JS_ASSERT(fp.fun()->nonLazyScript()->canIonCompile());
    JS_ASSERT(!fp.fun()->nonLazyScript()->isIonCompilingOffThread());
    JS_ASSERT(!fp.fun()->nonLazyScript()->hasIonScript());
    JS_ASSERT(fp.isFunctionFrame());

    // Mark as forbidden if frame can't be handled.
    if (!CheckFrame(fp)) {
        ForbidCompilation(cx, script);
        return Method_CantCompile;
    }

    // Attempt compilation. Returns Method_Compiled if already compiled.
    SequentialCompileContext compileContext;
    MethodStatus status = Compile(cx, script, fp, NULL, isConstructing, compileContext);
    if (status != Method_Compiled) {
        if (status == Method_CantCompile)
            ForbidCompilation(cx, script);
        return status;
    }

    return Method_Compiled;
}

MethodStatus
ParallelCompileContext::checkScriptSize(JSContext *cx, JSScript *script)
{
    if (!js_IonOptions.limitScriptSize)
        return Method_Compiled;

    // When compiling for parallel execution we don't have off-thread
    // compilation. We also up the max script size of the kernels.
    static const uint32_t MAX_SCRIPT_SIZE = 5000;
    static const uint32_t MAX_LOCALS_AND_ARGS = 256;

    if (script->length > MAX_SCRIPT_SIZE) {
        IonSpew(IonSpew_Abort, "Script too large (%u bytes)", script->length);
        return Method_CantCompile;
    }

    uint32_t numLocalsAndArgs = analyze::TotalSlots(script);
    if (numLocalsAndArgs > MAX_LOCALS_AND_ARGS) {
        IonSpew(IonSpew_Abort, "Too many locals and arguments (%u)", numLocalsAndArgs);
        return Method_CantCompile;
    }

    return Method_Compiled;
}

class AutoEnterTransitiveCompilation
{
    JSContext *cx_;

  public:
    AutoEnterTransitiveCompilation(JSContext *cx, AutoScriptVector &worklist)
      : cx_(cx)
    {
        JS_ASSERT(!cx->compartment->types.transitiveCompilationWorklist);
        cx->compartment->types.transitiveCompilationWorklist = &worklist;
    }

    ~AutoEnterTransitiveCompilation()
    {
        JS_ASSERT(cx_->compartment->types.transitiveCompilationWorklist);
        cx_->compartment->types.transitiveCompilationWorklist = NULL;
    }
};

MethodStatus
ParallelCompileContext::compileTransitively()
{
    using parallel::SpewBeginCompile;
    using parallel::SpewEndCompile;

    if (worklist_.empty())
        return Method_Skipped;

    AutoEnterTransitiveCompilation transCompile(cx_, worklist_);

    RootedFunction fun(cx_);
    RootedScript script(cx_);
    while (!worklist_.empty()) {
        script = worklist_.popCopy();
        fun = script->function();

        SpewBeginCompile(script);

        // If we had invalidations last time the parallel script ran, the bit
        // to check its call targets will be set.
        if (script->hasParallelIonScript()) {
            IonScript *ion = script->parallelIonScript();
            if (ion->hasInvalidatedCallTarget()) {
                ion->clearHasInvalidatedCallTarget();
                RootedScript target(cx_);
                for (uint32_t i = 0; i < ion->callTargetEntries(); i++) {
                    target = ion->callTargetList()[i];
                    parallel::Spew(parallel::SpewCompile,
                                   "Adding previously invalidated function %p:%s:%u",
                                   fun.get(), target->filename(), target->lineno);
                    appendToWorklist(target);
                }
            }
        }

        // Attempt compilation. Returns Method_Compiled if already compiled.
        MethodStatus status = Compile(cx_, script, AbstractFramePtr(), NULL, false, *this);
        if (status != Method_Compiled) {
            if (status == Method_CantCompile)
                ForbidCompilation(cx_, script, ParallelExecution);
            return SpewEndCompile(status);
        }

        // This can GC, so afterward, script->parallelIon is not guaranteed to be valid.
        if (!cx_->compartment->ionCompartment()->enterJIT())
            return SpewEndCompile(Method_Error);

        // Subtle: it is possible for GC to occur during compilation of
        // one of the invoked functions, which would cause the earlier
        // functions (such as the kernel itself) to be collected.  In this
        // event, we give up and fallback to sequential for now.
        if (!script->hasParallelIonScript()) {
            parallel::Spew(parallel::SpewCompile,
                           "Function %p:%s:%u was garbage-collected or invalidated",
                           fun.get(), script->filename(), script->lineno);
            return SpewEndCompile(Method_Skipped);
        }

        SpewEndCompile(Method_Compiled);
    }

    return Method_Compiled;
}

AbortReason
ParallelCompileContext::compile(IonBuilder *builder,
                                MIRGraph *graph,
                                ScopedJSDeletePtr<LifoAlloc> &autoDelete)
{
    JS_ASSERT(!builder->script()->hasParallelIonScript());
    JS_ASSERT(builder->script()->canParallelIonCompile());

    RootedScript builderScript(cx_, builder->script());
    IonSpewNewFunction(graph, builderScript);

    if (!builder->build())
        return builder->abortReason();
    builder->clearForBackEnd();

    // For the time being, we do not enable parallel compilation.

    if (!OptimizeMIR(builder)) {
        IonSpew(IonSpew_Abort, "Failed during MIR optimization.");
        return AbortReason_Disable;
    }

    if (!analyzeAndGrowWorklist(builder, *graph)) {
        return AbortReason_Disable;
    }

    LIRGraph *lir = GenerateLIR(builder);
    if (!lir) {
        IonSpew(IonSpew_Abort, "Failed during LIR generation.");
        return AbortReason_Disable;
    }

    CodeGenerator *codegen = GenerateCode(builder, lir);
    if (!codegen) {
        IonSpew(IonSpew_Abort, "Failed during code generation.");
        return AbortReason_Disable;
    }

    bool success = codegen->link();
    js_delete(codegen);

    IonSpewEndFunction();

    return success ? AbortReason_NoAbort : AbortReason_Disable;
}

MethodStatus
ion::CanEnterUsingFastInvoke(JSContext *cx, HandleScript script, uint32_t numActualArgs)
{
    JS_ASSERT(ion::IsEnabled(cx));

    // Skip if the code is expected to result in a bailout.
    if (!script->hasIonScript() || script->ionScript()->bailoutExpected())
        return Method_Skipped;

    // Don't handle arguments underflow, to make this work we would have to pad
    // missing arguments with |undefined|.
    if (numActualArgs < script->function()->nargs)
        return Method_Skipped;

    if (!cx->compartment->ensureIonCompartmentExists(cx))
        return Method_Error;

    // This can GC, so afterward, script->ion is not guaranteed to be valid.
    if (!cx->compartment->ionCompartment()->enterJIT())
        return Method_Error;

    if (!script->hasIonScript())
        return Method_Skipped;

    return Method_Compiled;
}

static IonExecStatus
EnterIon(JSContext *cx, StackFrame *fp, void *jitcode)
{
    JS_CHECK_RECURSION(cx, return IonExec_Aborted);
    JS_ASSERT(ion::IsEnabled(cx));
    JS_ASSERT(CheckFrame(fp));
    JS_ASSERT(!fp->script()->ionScript()->bailoutExpected());

    EnterIonCode enter = cx->compartment->ionCompartment()->enterJIT();

    // maxArgc is the maximum of arguments between the number of actual
    // arguments and the number of formal arguments. It accounts for |this|.
    int maxArgc = 0;
    Value *maxArgv = NULL;
    int numActualArgs = 0;
    RootedValue thisv(cx);

    void *calleeToken;
    if (fp->isFunctionFrame()) {
        // CountArgSlot include |this| and the |scopeChain| and maybe |argumentsObj|.
        // Keep |this|, but discard the others.
        maxArgc = CountArgSlots(fp->script(), fp->fun()) - StartArgSlot(fp->script(), fp->fun());
        maxArgv = fp->formals() - 1;            // -1 = include |this|

        // Formal arguments are the argument corresponding to the function
        // definition and actual arguments are corresponding to the call-site
        // arguments.
        numActualArgs = fp->numActualArgs();

        // We do not need to handle underflow because formal arguments are pad
        // with |undefined| values but we need to distinguish between the
        if (fp->hasOverflowArgs()) {
            int formalArgc = maxArgc;
            Value *formalArgv = maxArgv;
            maxArgc = numActualArgs + 1; // +1 = include |this|
            maxArgv = fp->actuals() - 1; // -1 = include |this|

            // The beginning of the actual args is not updated, so we just copy
            // the formal args into the actual args to get a linear vector which
            // can be copied by generateEnterJit.
            memcpy(maxArgv, formalArgv, formalArgc * sizeof(Value));
        }
        calleeToken = CalleeToToken(&fp->callee());
    } else {
        calleeToken = CalleeToToken(fp->script());
        thisv = fp->thisValue();
        maxArgc = 1;
        maxArgv = thisv.address();
    }

    // Caller must construct |this| before invoking the Ion function.
    JS_ASSERT_IF(fp->isConstructing(), fp->functionThis().isObject());
    RootedValue result(cx, Int32Value(numActualArgs));
    {
        AssertCompartmentUnchanged pcc(cx);
        IonContext ictx(cx, NULL);
        IonActivation activation(cx, fp);
        JSAutoResolveFlags rf(cx, RESOLVE_INFER);
        AutoFlushInhibitor afi(cx->compartment->ionCompartment());
        // Single transition point from Interpreter to Ion.
        enter(jitcode, maxArgc, maxArgv, fp, calleeToken, /* scopeChain = */ NULL, 0,
              result.address());
    }

    JS_ASSERT(fp == cx->fp());
    JS_ASSERT(!cx->runtime->hasIonReturnOverride());

    // The trampoline wrote the return value but did not set the HAS_RVAL flag.
    fp->setReturnValue(result);

    // Ion callers wrap primitive constructor return.
    if (!result.isMagic() && fp->isConstructing() && fp->returnValue().isPrimitive())
        fp->setReturnValue(ObjectValue(fp->constructorThis()));

    JS_ASSERT_IF(result.isMagic(), result.isMagic(JS_ION_ERROR));
    return result.isMagic() ? IonExec_Error : IonExec_Ok;
}

IonExecStatus
ion::Cannon(JSContext *cx, StackFrame *fp)
{
    RootedScript script(cx, fp->script());
    IonScript *ion = script->ionScript();
    IonCode *code = ion->method();
    void *jitcode = code->raw();

#if JS_TRACE_LOGGING
    TraceLog(TraceLogging::defaultLogger(),
             TraceLogging::ION_CANNON_START,
             script);
#endif

    IonExecStatus status = EnterIon(cx, fp, jitcode);

#if JS_TRACE_LOGGING
    if (status == IonExec_Bailout) {
        TraceLog(TraceLogging::defaultLogger(),
                 TraceLogging::ION_CANNON_BAIL,
                 script);
    } else {
        TraceLog(TraceLogging::defaultLogger(),
                 TraceLogging::ION_CANNON_STOP,
                 script);
    }
#endif

    return status;
}

IonExecStatus
ion::FastInvoke(JSContext *cx, HandleFunction fun, CallArgs &args)
{
    JS_CHECK_RECURSION(cx, return IonExec_Error);

    IonScript *ion = fun->nonLazyScript()->ionScript();
    IonCode *code = ion->method();
    void *jitcode = code->raw();

    JS_ASSERT(ion::IsEnabled(cx));
    JS_ASSERT(!ion->bailoutExpected());

    bool clearCallingIntoIon = false;
    StackFrame *fp = cx->fp();

    // Two cases we have to handle:
    //
    // (1) fp does not begin an Ion activation. This works exactly
    //     like invoking Ion from JM: entryfp is set to fp and fp
    //     has the callingIntoIon flag set.
    //
    // (2) fp already begins another IonActivation, for instance:
    //        JM -> Ion -> array_sort -> Ion
    //     In this cas we use an IonActivation with entryfp == NULL
    //     and prevpc != NULL.
    IonActivation activation(cx, NULL);
    if (!fp->beginsIonActivation()) {
        fp->setCallingIntoIon();
        clearCallingIntoIon = true;
        activation.setEntryFp(fp);
    } else {
        JS_ASSERT(!activation.entryfp());
    }

    activation.setPrevPc(cx->regs().pc);

    EnterIonCode enter = cx->compartment->ionCompartment()->enterJIT();
    void *calleeToken = CalleeToToken(fun);

    RootedValue result(cx, Int32Value(args.length()));
    JS_ASSERT(args.length() >= fun->nargs);

    JSAutoResolveFlags rf(cx, RESOLVE_INFER);
    enter(jitcode, args.length() + 1, args.array() - 1, fp, calleeToken,
          /* scopeChain = */ NULL, 0, result.address());

    if (clearCallingIntoIon)
        fp->clearCallingIntoIon();

    JS_ASSERT(fp == cx->fp());
    JS_ASSERT(!cx->runtime->hasIonReturnOverride());

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
            JS_NOT_REACHED("invalid");
            break;
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
ion::InvalidateAll(FreeOp *fop, Zone *zone)
{
    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
        if (!comp->ionCompartment())
            continue;
        CancelOffThreadIonCompile(comp, NULL);
        FinishAllOffThreadCompilations(comp->ionCompartment());
    }

    for (IonActivationIterator iter(fop->runtime()); iter.more(); ++iter) {
        if (iter.activation()->compartment()->zone() == zone) {
            IonContext ictx(zone->rt);
            AutoFlushCache afc("InvalidateAll", zone->rt->ionRuntime());
            IonSpew(IonSpew_Invalidate, "Invalidating all frames for GC");
            InvalidateActivation(fop, iter.top(), true);
        }
    }
}


void
ion::Invalidate(types::TypeCompartment &types, FreeOp *fop,
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

    for (IonActivationIterator iter(fop->runtime()); iter.more(); ++iter)
        InvalidateActivation(fop, iter.top(), false);

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

        Zone *zone = script->zone();
        if (zone->needsBarrier()) {
            // We're about to remove edges from the JSScript to gcthings
            // embedded in the IonScript. Perform one final trace of the
            // IonScript for the incremental GC, as it must know about
            // those edges.
            IonScript::Trace(zone->barrierTracer(), ionScript);
        }

        ionScript->decref(fop);
        SetIonScript(script, executionMode, NULL);
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
ion::Invalidate(JSContext *cx, const Vector<types::RecompileInfo> &invalid, bool resetUses)
{
    ion::Invalidate(cx->compartment->types, cx->runtime->defaultFreeOp(), invalid, resetUses);
}

bool
ion::Invalidate(JSContext *cx, JSScript *script, ExecutionMode mode, bool resetUses)
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
ion::Invalidate(JSContext *cx, JSScript *script, bool resetUses)
{
    return Invalidate(cx, script, SequentialExecution, resetUses);
}

static void
FinishInvalidationOf(FreeOp *fop, JSScript *script, IonScript *ionScript, bool parallel)
{
    // If this script has Ion code on the stack, invalidation() will return
    // true. In this case we have to wait until destroying it.
    if (!ionScript->invalidated()) {
        types::TypeCompartment &types = script->compartment()->types;
        ionScript->recompileInfo().compilerOutput(types)->invalidate();

        ion::IonScript::Destroy(fop, ionScript);
    }

    // In all cases, NULL out script->ion or script->parallelIon to avoid
    // re-entry.
    if (parallel)
        script->setParallelIonScript(NULL);
    else
        script->setIonScript(NULL);
}

void
ion::FinishInvalidation(FreeOp *fop, JSScript *script)
{
    if (script->hasIonScript())
        FinishInvalidationOf(fop, script, script->ionScript(), false);

    if (script->hasParallelIonScript())
        FinishInvalidationOf(fop, script, script->parallelIonScript(), true);
}

void
ion::MarkValueFromIon(JSRuntime *rt, Value *vp)
{
    gc::MarkValueUnbarriered(&rt->gcMarker, vp, "write barrier");
}

void
ion::MarkShapeFromIon(JSRuntime *rt, Shape **shapep)
{
    gc::MarkShapeUnbarriered(&rt->gcMarker, shapep, "write barrier");
}

void
ion::ForbidCompilation(JSContext *cx, JSScript *script)
{
    ForbidCompilation(cx, script, SequentialExecution);
}

void
ion::ForbidCompilation(JSContext *cx, JSScript *script, ExecutionMode mode)
{
    IonSpew(IonSpew_Abort, "Disabling Ion mode %d compilation of script %s:%d",
            mode, script->filename(), script->lineno);

    CancelOffThreadIonCompile(cx->compartment, script);

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

    JS_NOT_REACHED("No such execution mode");
}

uint32_t
ion::UsesBeforeIonRecompile(JSScript *script, jsbytecode *pc)
{
    JS_ASSERT(pc == script->code || JSOp(*pc) == JSOP_LOOPENTRY);

    uint32_t minUses = js_IonOptions.usesBeforeCompile;
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
    AutoFlushCache *afc = irt->flusher();
    afc->update(p, len);
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

int js::ion::LabelBase::id_count = 0;

void
ion::PurgeCaches(JSScript *script, Zone *zone)
{
    if (script->hasIonScript())
        script->ionScript()->purgeCaches(zone);

    if (script->hasParallelIonScript())
        script->parallelIonScript()->purgeCaches(zone);
}

size_t
ion::SizeOfIonData(JSScript *script, JSMallocSizeOfFun mallocSizeOf)
{
    size_t result = 0;

    if (script->hasIonScript())
        result += script->ionScript()->sizeOfIncludingThis(mallocSizeOf);

    if (script->hasParallelIonScript())
        result += script->parallelIonScript()->sizeOfIncludingThis(mallocSizeOf);

    return result;
}

void
ion::DestroyIonScripts(FreeOp *fop, JSScript *script)
{
    if (script->hasIonScript())
        ion::IonScript::Destroy(fop, script->ionScript());

    if (script->hasParallelIonScript())
        ion::IonScript::Destroy(fop, script->parallelIonScript());

    if (script->hasBaselineScript())
        ion::BaselineScript::Destroy(fop, script->baselineScript());
}

void
ion::TraceIonScripts(JSTracer* trc, JSScript *script)
{
    if (script->hasIonScript())
        ion::IonScript::Trace(trc, script->ionScript());

    if (script->hasParallelIonScript())
        ion::IonScript::Trace(trc, script->parallelIonScript());

    if (script->hasBaselineScript())
        ion::BaselineScript::Trace(trc, script->baselineScript());
}
