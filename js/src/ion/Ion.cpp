/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "jscompartment.h"
#include "jsworkers.h"
#include "IonCompartment.h"
#include "CodeGenerator.h"
#include "BacktrackingAllocator.h"
#include "StupidAllocator.h"
#include "UnreachableCodeElimination.h"

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
#include "methodjit/Retcon.h"
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

IonContext::IonContext(JSContext *cx, JSCompartment *compartment, TempAllocator *temp)
  : cx(cx),
    compartment(compartment),
    temp(temp),
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
    return true;
}

IonRuntime::IonRuntime()
  : execAlloc_(NULL),
    enterJIT_(NULL),
    bailoutHandler_(NULL),
    argumentsRectifier_(NULL),
    invalidator_(NULL),
    functionWrappers_(NULL)
{
}

IonRuntime::~IonRuntime()
{
    js_delete(functionWrappers_);
}

bool
IonRuntime::initialize(JSContext *cx)
{
    AutoEnterAtomsCompartment ac(cx);

    if (!cx->compartment->ensureIonCompartmentExists(cx))
        return false;

    IonContext ictx(cx, cx->compartment, NULL);
    AutoFlushCache afc("IonRuntime::initialize");

    execAlloc_ = cx->runtime->getExecAlloc(cx);
    if (!execAlloc_)
        return false;

    functionWrappers_ = cx->new_<VMWrapperMap>(cx);
    if (!functionWrappers_ || !functionWrappers_->init())
        return false;

    if (!bailoutTables_.reserve(FrameSizeClass::ClassLimit().classId()))
        return false;

    for (uint32_t id = 0;; id++) {
        FrameSizeClass class_ = FrameSizeClass::FromClass(id);
        if (class_ == FrameSizeClass::ClassLimit())
            break;
        bailoutTables_.infallibleAppend(NULL);
        bailoutTables_[id] = generateBailoutTable(cx, id);
        if (!bailoutTables_[id])
            return false;
    }

    bailoutHandler_ = generateBailoutHandler(cx);
    if (!bailoutHandler_)
        return false;

    argumentsRectifier_ = generateArgumentsRectifier(cx);
    if (!argumentsRectifier_)
        return false;

    invalidator_ = generateInvalidator(cx);
    if (!invalidator_)
        return false;

    enterJIT_ = generateEnterJIT(cx);
    if (!enterJIT_)
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

IonCompartment::IonCompartment(IonRuntime *rt)
  : rt(rt),
    flusher_(NULL)
{
}

bool
IonCompartment::initialize(JSContext *cx)
{
    return true;
}

void
ion::FinishOffThreadBuilder(IonBuilder *builder)
{
    // Clean up if compilation did not succeed.
    if (builder->script()->isIonCompilingOffThread()) {
        types::TypeCompartment &types = builder->script()->compartment()->types;
        builder->recompileInfo.compilerOutput(types)->invalidate();
        builder->script()->ion = NULL;
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
    for (gc::CellIterUnderGC i(trc->runtime->atomsCompartment, gc::FINALIZE_IONCODE); !i.done(); i.next()) {
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
}

void
IonCompartment::sweep(FreeOp *fop)
{
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
    typedef MoveResolver::MoveOperand MoveOperand;

    JS_ASSERT(rt->functionWrappers_);
    JS_ASSERT(rt->functionWrappers_->initialized());
    IonRuntime::VMWrapperMap::Ptr p = rt->functionWrappers_->lookup(&f);
    JS_ASSERT(p);

    return p->value;
}

IonActivation::IonActivation(JSContext *cx, StackFrame *fp)
  : cx_(cx),
    compartment_(cx->compartment),
    prev_(cx->runtime->ionActivation),
    entryfp_(fp),
    bailout_(NULL),
    prevIonTop_(cx->runtime->ionTop),
    prevIonJSContext_(cx->runtime->ionJSContext),
    prevpc_(NULL)
{
    if (fp)
        fp->setRunningInIon();
    cx->runtime->ionJSContext = cx;
    cx->runtime->ionActivation = this;
    cx->runtime->ionStackLimit = cx->runtime->nativeStackLimit;
}

IonActivation::~IonActivation()
{
    JS_ASSERT(cx_->runtime->ionActivation == this);
    JS_ASSERT(!bailout_);

    if (entryfp_)
        entryfp_->clearRunningInIon();
    cx_->runtime->ionActivation = prev();
    cx_->runtime->ionTop = prevIonTop_;
    cx_->runtime->ionJSContext = prevIonJSContext_;
}

IonCode *
IonCode::New(JSContext *cx, uint8_t *code, uint32_t bufferSize, JSC::ExecutablePool *pool)
{
    AssertCanGC();

    IonCode *codeObj = gc::NewGCThing<IonCode>(cx, gc::FINALIZE_IONCODE, sizeof(IonCode));
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

    dataSize_ = masm.dataSize();
    masm.processDeferredData(this, code_ + dataOffset());

    jumpRelocTableBytes_ = masm.jumpRelocationTableBytes();
    masm.copyJumpRelocationTable(code_ + jumpRelocTableOffset());
    dataRelocTableBytes_ = masm.dataRelocationTableBytes();
    masm.copyDataRelocationTable(code_ + dataRelocTableOffset());

    masm.processCodeLabels(this);
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

    // Code buffers are stored inside JSC pools.
    // Pools are refcounted. Releasing the pool may free it.
    if (pool_)
        pool_->release();
}

void
IonCode::readBarrier(IonCode *code)
{
#ifdef JSGC_INCREMENTAL
    if (!code)
        return;

    JSCompartment *comp = code->compartment();
    if (comp->needsBarrier())
        MarkIonCodeUnbarriered(comp->barrierTracer(), &code, "ioncode read barrier");
#endif
}

void
IonCode::writeBarrierPre(IonCode *code)
{
#ifdef JSGC_INCREMENTAL
    if (!code)
        return;

    JSCompartment *comp = code->compartment();
    if (comp->needsBarrier())
        MarkIonCodeUnbarriered(comp->barrierTracer(), &code, "ioncode write barrier");
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
    invalidateEpilogueOffset_(0),
    invalidateEpilogueDataOffset_(0),
    bailoutExpected_(false),
    snapshots_(0),
    snapshotsSize_(0),
    bailoutTable_(0),
    bailoutEntries_(0),
    constantTable_(0),
    constantEntries_(0),
    safepointIndexOffset_(0),
    safepointIndexEntries_(0),
    frameSlots_(0),
    frameSize_(0),
    osiIndexOffset_(0),
    osiIndexEntries_(0),
    cacheList_(0),
    cacheEntries_(0),
    prebarrierList_(0),
    prebarrierEntries_(0),
    safepointsStart_(0),
    safepointsSize_(0),
    scriptList_(0),
    scriptEntries_(0),
    refcount_(0),
    recompileInfo_(),
    slowCallCount(0)
{
}

static const int DataAlignment = 4;

IonScript *
IonScript::New(JSContext *cx, uint32_t frameSlots, uint32_t frameSize, size_t snapshotsSize,
               size_t bailoutEntries, size_t constants, size_t safepointIndices,
               size_t osiIndices, size_t cacheEntries, size_t prebarrierEntries,
               size_t safepointsSize, size_t scriptEntries)
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
    size_t paddedCacheEntriesSize = AlignBytes(cacheEntries * sizeof(IonCache), DataAlignment);
    size_t paddedPrebarrierEntriesSize =
        AlignBytes(prebarrierEntries * sizeof(CodeOffsetLabel), DataAlignment);
    size_t paddedSafepointSize = AlignBytes(safepointsSize, DataAlignment);
    size_t paddedScriptSize = AlignBytes(scriptEntries * sizeof(RawScript), DataAlignment);
    size_t bytes = paddedSnapshotsSize +
                   paddedBailoutSize +
                   paddedConstantsSize +
                   paddedSafepointIndicesSize+
                   paddedOsiIndicesSize +
                   paddedCacheEntriesSize +
                   paddedPrebarrierEntriesSize +
                   paddedSafepointSize +
                   paddedScriptSize;
    uint8_t *buffer = (uint8_t *)cx->malloc_(sizeof(IonScript) + bytes);
    if (!buffer)
        return NULL;

    IonScript *script = reinterpret_cast<IonScript *>(buffer);
    new (script) IonScript();

    uint32_t offsetCursor = sizeof(IonScript);

    script->snapshots_ = offsetCursor;
    script->snapshotsSize_ = snapshotsSize;
    offsetCursor += paddedSnapshotsSize;

    script->bailoutTable_ = offsetCursor;
    script->bailoutEntries_ = bailoutEntries;
    offsetCursor += paddedBailoutSize;

    script->constantTable_ = offsetCursor;
    script->constantEntries_ = constants;
    offsetCursor += paddedConstantsSize;

    script->safepointIndexOffset_ = offsetCursor;
    script->safepointIndexEntries_ = safepointIndices;
    offsetCursor += paddedSafepointIndicesSize;

    script->osiIndexOffset_ = offsetCursor;
    script->osiIndexEntries_ = osiIndices;
    offsetCursor += paddedOsiIndicesSize;

    script->cacheList_ = offsetCursor;
    script->cacheEntries_ = cacheEntries;
    offsetCursor += paddedCacheEntriesSize;

    script->prebarrierList_ = offsetCursor;
    script->prebarrierEntries_ = prebarrierEntries;
    offsetCursor += paddedPrebarrierEntriesSize;

    script->safepointsStart_ = offsetCursor;
    script->safepointsSize_ = safepointsSize;
    offsetCursor += paddedSafepointSize;

    script->scriptList_ = offsetCursor;
    script->scriptEntries_ = scriptEntries;
    offsetCursor += paddedScriptSize;

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
IonScript::copyConstants(const HeapValue *vp)
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
IonScript::copySafepointIndices(const SafepointIndex *si, MacroAssembler &masm)
{
    /*
     * Jumps in the caches reflect the offset of those jumps in the compiled
     * code, not the absolute positions of the jumps. Update according to the
     * final code address now.
     */
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
IonScript::copyCacheEntries(const IonCache *caches, MacroAssembler &masm)
{
    memcpy(cacheList(), caches, numCaches() * sizeof(IonCache));

    /*
     * Jumps in the caches reflect the offset of those jumps in the compiled
     * code, not the absolute positions of the jumps. Update according to the
     * final code address now.
     */
    for (size_t i = 0; i < numCaches(); i++)
        getCache(i).updateBaseAddress(method_, masm);
}

inline CodeOffsetLabel &
IonScript::getPrebarrier(size_t index)
{
    JS_ASSERT(index < numPrebarriers());
    return prebarrierList()[index];
}

void
IonScript::copyPrebarrierEntries(const CodeOffsetLabel *barriers, MacroAssembler &masm)
{
    memcpy(prebarrierList(), barriers, numPrebarriers() * sizeof(CodeOffsetLabel));

    // On ARM, the saved offset may be wrong due to shuffling code buffers. Correct it.
    for (size_t i = 0; i < numPrebarriers(); i++)
        getPrebarrier(i).fixup(&masm);
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
    fop->free_(script);
}

void
IonScript::toggleBarriers(bool enabled)
{
    for (size_t i = 0; i < numPrebarriers(); i++) {
        CodeLocationLabel loc(method(), getPrebarrier(i));

        if (enabled)
            Assembler::ToggleToCmp(loc);
        else
            Assembler::ToggleToJmp(loc);
    }
}

void
IonScript::purgeCaches(JSCompartment *c)
{
    // Don't reset any ICs if we're invalidated, otherwise, repointing the
    // inline jump could overwrite an invalidation marker. These ICs can
    // no longer run, however, the IC slow paths may be active on the stack.
    // ICs therefore are required to check for invalidation before patching,
    // to ensure the same invariant.
    if (invalidated())
        return;

    // This is necessary because AutoFlushCache::updateTop()
    // looks up the current flusher in the IonContext.  Without one
    // it cannot work.
    js::ion::IonContext ictx(NULL, c, NULL);
    AutoFlushCache afc("purgeCaches");
    for (size_t i = 0; i < numCaches(); i++)
        getCache(i).reset();
}

void
ion::ToggleBarriers(JSCompartment *comp, bool needs)
{
    IonContext ictx(NULL, comp, NULL);
    AutoFlushCache afc("ToggleBarriers");
    for (gc::CellIterUnderGC i(comp, gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        UnrootedScript script = i.get<JSScript>();
        if (script->hasIonScript())
            script->ion->toggleBarriers(needs);
    }
}

namespace js {
namespace ion {

CodeGenerator *
CompileBackEnd(MIRGenerator *mir)
{
    IonSpewPass("BuildSSA");
    // Note: don't call AssertGraphCoherency before SplitCriticalEdges,
    // the graph is not in RPO at this point.

    MIRGraph &graph = mir->graph();

    if (mir->shouldCancel("Start"))
        return NULL;

    if (!SplitCriticalEdges(graph))
        return NULL;
    IonSpewPass("Split Critical Edges");
    AssertGraphCoherency(graph);

    if (mir->shouldCancel("Split Critical Edges"))
        return NULL;

    if (!RenumberBlocks(graph))
        return NULL;
    IonSpewPass("Renumber Blocks");
    AssertGraphCoherency(graph);

    if (mir->shouldCancel("Renumber Blocks"))
        return NULL;

    if (!BuildDominatorTree(graph))
        return NULL;
    // No spew: graph not changed.

    if (mir->shouldCancel("Dominator Tree"))
        return NULL;

    // This must occur before any code elimination.
    if (!EliminatePhis(mir, graph, AggressiveObservability))
        return NULL;
    IonSpewPass("Eliminate phis");
    AssertGraphCoherency(graph);

    if (mir->shouldCancel("Eliminate phis"))
        return NULL;

    if (!BuildPhiReverseMapping(graph))
        return NULL;
    AssertExtendedGraphCoherency(graph);
    // No spew: graph not changed.

    if (mir->shouldCancel("Phi reverse mapping"))
        return NULL;

    // This pass also removes copies.
    if (!ApplyTypeInformation(mir, graph))
        return NULL;
    IonSpewPass("Apply types");
    AssertExtendedGraphCoherency(graph);

    if (mir->shouldCancel("Apply types"))
        return NULL;

    // Alias analysis is required for LICM and GVN so that we don't move
    // loads across stores.
    if (js_IonOptions.licm || js_IonOptions.gvn) {
        AliasAnalysis analysis(mir, graph);
        if (!analysis.analyze())
            return NULL;
        IonSpewPass("Alias analysis");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("Alias analysis"))
            return NULL;

        // Eliminating dead resume point operands requires basic block
        // instructions to be numbered. Reuse the numbering computed during
        // alias analysis.
        if (!EliminateDeadResumePointOperands(mir, graph))
            return NULL;

        if (mir->shouldCancel("Eliminate dead resume point operands"))
            return NULL;
    }

    if (js_IonOptions.edgeCaseAnalysis) {
        EdgeCaseAnalysis edgeCaseAnalysis(mir, graph);
        if (!edgeCaseAnalysis.analyzeEarly())
            return NULL;
        IonSpewPass("Edge Case Analysis (Early)");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("Edge Case Analysis (Early)"))
            return NULL;
    }

    if (js_IonOptions.gvn) {
        ValueNumberer gvn(mir, graph, js_IonOptions.gvnIsOptimistic);
        if (!gvn.analyze())
            return NULL;
        IonSpewPass("GVN");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("GVN"))
            return NULL;
    }

    if (js_IonOptions.uce) {
        UnreachableCodeElimination uce(mir, graph);
        if (!uce.analyze())
            return NULL;
        IonSpewPass("UCE");
        AssertExtendedGraphCoherency(graph);
    }

    if (mir->shouldCancel("UCE"))
        return NULL;

    if (js_IonOptions.licm) {
        LICM licm(mir, graph);
        if (!licm.analyze())
            return NULL;
        IonSpewPass("LICM");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("LICM"))
            return NULL;
    }

    if (js_IonOptions.rangeAnalysis) {
        RangeAnalysis r(graph);
        if (!r.addBetaNobes())
            return NULL;
        IonSpewPass("Beta");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("RA Beta"))
            return NULL;

        if (!r.analyze())
            return NULL;
        IonSpewPass("Range Analysis");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("Range Analysis"))
            return NULL;

        if (!r.removeBetaNobes())
            return NULL;
        IonSpewPass("De-Beta");
        AssertExtendedGraphCoherency(graph);

        if (mir->shouldCancel("RA De-Beta"))
            return NULL;
    }

    if (!EliminateDeadCode(mir, graph))
        return NULL;
    IonSpewPass("DCE");
    AssertExtendedGraphCoherency(graph);

    if (mir->shouldCancel("DCE"))
        return NULL;

    // Passes after this point must not move instructions; these analyses
    // depend on knowing the final order in which instructions will execute.

    if (js_IonOptions.edgeCaseAnalysis) {
        EdgeCaseAnalysis edgeCaseAnalysis(mir, graph);
        if (!edgeCaseAnalysis.analyzeLate())
            return NULL;
        IonSpewPass("Edge Case Analysis (Late)");
        AssertGraphCoherency(graph);

        if (mir->shouldCancel("Edge Case Analysis (Late)"))
            return NULL;
    }

    // Note: check elimination has to run after all other passes that move
    // instructions. Since check uses are replaced with the actual index, code
    // motion after this pass could incorrectly move a load or store before its
    // bounds check.
    if (!EliminateRedundantChecks(graph))
        return NULL;
    IonSpewPass("Bounds Check Elimination");
    AssertGraphCoherency(graph);

    if (mir->shouldCancel("Bounds Check Elimination"))
        return NULL;

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

    CodeGenerator *codegen = js_new<CodeGenerator>(mir, lir);
    if (!codegen || !codegen->generate()) {
        js_delete(codegen);
        return NULL;
    }

    return codegen;
}

class AutoDestroyAllocator
{
    LifoAlloc *alloc;

  public:
    AutoDestroyAllocator(LifoAlloc *alloc) : alloc(alloc) {}

    void cancel()
    {
        alloc = NULL;
    }

    ~AutoDestroyAllocator()
    {
        if (alloc)
            js_delete(alloc);
    }
};

class SequentialCompileContext {
public:
    ExecutionMode executionMode() {
        return SequentialExecution;
    }

    bool compile(IonBuilder *builder, MIRGraph *graph,
                 AutoDestroyAllocator &autoDestroy);
};

void
AttachFinishedCompilations(JSContext *cx)
{
#ifdef JS_THREADSAFE
    AssertCanGC();
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
            IonContext ictx(cx, cx->compartment, &builder->temp());

            // Root the assembler until the builder is finished below. As it
            // was constructed off thread, the assembler has not been rooted
            // previously, though any GC activity would discard the builder.
            codegen->masm.constructRoot(cx);

            types::AutoEnterTypeInference enterTypes(cx);

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

            if (success) {
                if (script->hasIonScript())
                    mjit::DisableScriptCodeForIon(script, script->ionScript()->osrPc());
            } else {
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
static bool
IonCompile(JSContext *cx, HandleScript script, HandleFunction fun, jsbytecode *osrPc, bool constructing,
           CompileContext &compileContext)
{
    AssertCanGC();
#if JS_TRACE_LOGGING
    AutoTraceLog logger(TraceLogging::defaultLogger(),
                        TraceLogging::ION_COMPILE_START,
                        TraceLogging::ION_COMPILE_STOP,
                        script);
#endif

    LifoAlloc *alloc = cx->new_<LifoAlloc>(BUILDER_LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
    if (!alloc)
        return false;

    AutoDestroyAllocator autoDestroy(alloc);

    TempAllocator *temp = alloc->new_<TempAllocator>(alloc);
    if (!temp)
        return false;

    IonContext ictx(cx, cx->compartment, temp);

    if (!cx->compartment->ensureIonCompartmentExists(cx))
        return false;

    MIRGraph *graph = alloc->new_<MIRGraph>(temp);
    ExecutionMode executionMode = compileContext.executionMode();
    CompileInfo *info = alloc->new_<CompileInfo>(script, fun, osrPc, constructing,
                                                 executionMode);
    if (!info)
        return false;

    types::AutoEnterTypeInference enter(cx, true);
    TypeInferenceOracle oracle;

    if (!oracle.init(cx, script))
        return false;

    AutoFlushCache afc("IonCompile");

    types::AutoEnterCompilation enterCompiler(cx, CompilerOutputKind(executionMode));
    if (!enterCompiler.init(script, false, 0))
        return false;

    AutoTempAllocatorRooter root(cx, temp);

    IonBuilder *builder = alloc->new_<IonBuilder>(cx, temp, graph, &oracle, info);
    if (!compileContext.compile(builder, graph, autoDestroy)) {
        IonSpew(IonSpew_Abort, "IM Compilation failed.");
        return false;
    }

    return true;
}

bool
SequentialCompileContext::compile(IonBuilder *builder, MIRGraph *graph,
                                  AutoDestroyAllocator &autoDestroy)
{
    JS_ASSERT(!builder->script()->ion);
    JSContext *cx = GetIonContext()->cx;

    RootedScript builderScript(cx, builder->script());
    IonSpewNewFunction(graph, builderScript);

    if (!builder->build()) {
        IonSpew(IonSpew_Abort, "Builder failed to build.");
        return false;
    }
    builder->clearForBackEnd();

    // Try to compile the script off thread, if possible. Compilation cannot be
    // performed off thread during an incremental GC, as doing so may trip
    // incremental read barriers. Also skip off thread compilation if script
    // execution is being profiled, as CodeGenerator::maybeCreateScriptCounts
    // will not attach script profiles when running off thread.
    if (js_IonOptions.parallelCompilation &&
        OffThreadCompilationAvailable(cx) &&
        cx->runtime->gcIncrementalState == gc::NO_INCREMENTAL &&
        !cx->runtime->profilingScripts)
    {
        builder->script()->ion = ION_COMPILING_SCRIPT;

        if (!StartOffThreadIonCompile(cx, builder)) {
            IonSpew(IonSpew_Abort, "Unable to start off-thread ion compilation.");
            return false;
        }

        // The allocator and associated data will be destroyed after being
        // processed in the finishedOffThreadCompilations list.
        autoDestroy.cancel();

        return true;
    }

    CodeGenerator *codegen = CompileBackEnd(builder);
    if (!codegen) {
        IonSpew(IonSpew_Abort, "Failed during back-end compilation.");
        return false;
    }

    bool success = codegen->link();
    js_delete(codegen);

    IonSpewEndFunction();

    return success;
}

bool
TestIonCompile(JSContext *cx, HandleScript script, HandleFunction fun, jsbytecode *osrPc, bool constructing)
{
    SequentialCompileContext compileContext;
    if (!IonCompile(cx, script, fun, osrPc, constructing, compileContext)) {
        if (!cx->isExceptionPending())
            ForbidCompilation(cx, script);
        return false;
    }
    return true;
}

static bool
CheckFrame(StackFrame *fp)
{
    if (fp->isEvalFrame()) {
        // Eval frames are not yet supported. Supporting this will require new
        // logic in pushBailoutFrame to deal with linking prev.
        // Additionally, JSOP_DEFVAR support will require baking in isEvalFrame().
        IonSpew(IonSpew_Abort, "eval frame");
        return false;
    }

    if (fp->isGeneratorFrame()) {
        // Err... no.
        IonSpew(IonSpew_Abort, "generator frame");
        return false;
    }

    if (fp->isDebuggerFrame()) {
        IonSpew(IonSpew_Abort, "debugger frame");
        return false;
    }

    if (fp->annotation()) {
        IonSpew(IonSpew_Abort, "frame is annotated");
        return false;
    }

    // This check is to not overrun the stack. Eventually, we will want to
    // handle this when we support JSOP_ARGUMENTS or function calls.
    if (fp->isFunctionFrame() &&
        (fp->numActualArgs() >= SNAPSHOT_MAX_NARGS ||
         fp->numActualArgs() > js_IonOptions.maxStackArgs))
    {
        IonSpew(IonSpew_Abort, "too many actual args");
        return false;
    }

    return true;
}

static bool
CheckScript(UnrootedScript script)
{
    if (script->needsArgsObj()) {
        // Functions with arguments objects, are not supported yet.
        IonSpew(IonSpew_Abort, "script has argsobj");
        return false;
    }

    if (!script->compileAndGo) {
        IonSpew(IonSpew_Abort, "not compile-and-go");
        return false;
    }

    return true;
}

static bool
CheckScriptSize(UnrootedScript script)
{
    if (!js_IonOptions.limitScriptSize)
        return true;

    static const uint32_t MAX_SCRIPT_SIZE = 2000;
    static const uint32_t MAX_LOCALS_AND_ARGS = 256;

    if (script->length > MAX_SCRIPT_SIZE) {
        IonSpew(IonSpew_Abort, "Script too large (%u bytes)", script->length);
        return false;
    }

    uint32_t numLocalsAndArgs = analyze::TotalSlots(script);
    if (numLocalsAndArgs > MAX_LOCALS_AND_ARGS) {
        IonSpew(IonSpew_Abort, "Too many locals and arguments (%u)", numLocalsAndArgs);
        return false;
    }

    return true;
}

static MethodStatus
Compile(JSContext *cx, HandleScript script, HandleFunction fun, jsbytecode *osrPc, bool constructing)
{
    JS_ASSERT(ion::IsEnabled(cx));
    JS_ASSERT_IF(osrPc != NULL, (JSOp)*osrPc == JSOP_LOOPENTRY);

    if (cx->compartment->debugMode()) {
        IonSpew(IonSpew_Abort, "debugging");
        return Method_CantCompile;
    }

    if (!CheckScript(script) || !CheckScriptSize(script)) {
        IonSpew(IonSpew_Abort, "Aborted compilation of %s:%d", script->filename, script->lineno);
        return Method_CantCompile;
    }

    if (script->ion) {
        if (!script->ion->method())
            return Method_CantCompile;
        return Method_Compiled;
    }

    if (cx->methodJitEnabled) {
        // If JM is enabled we use getUseCount instead of incUseCount to avoid
        // bumping the use count twice.
        if (script->getUseCount() < js_IonOptions.usesBeforeCompile)
            return Method_Skipped;
    } else {
        if (script->incUseCount() < js_IonOptions.usesBeforeCompileNoJaeger)
            return Method_Skipped;
    }

    SequentialCompileContext compileContext;
    if (!IonCompile(cx, script, fun, osrPc, constructing, compileContext))
        return Method_CantCompile;

    // Compilation succeeded, but we invalidated right away.
    return script->hasIonScript() ? Method_Compiled : Method_Skipped;
}

} // namespace ion
} // namespace js

// Decide if a transition from interpreter execution to Ion code should occur.
// May compile or recompile the target JSScript.
MethodStatus
ion::CanEnterAtBranch(JSContext *cx, HandleScript script, StackFrame *fp, jsbytecode *pc)
{
    JS_ASSERT(ion::IsEnabled(cx));
    JS_ASSERT((JSOp)*pc == JSOP_LOOPENTRY);

    // Skip if the script has been disabled.
    if (script->ion == ION_DISABLED_SCRIPT)
        return Method_Skipped;

    // Skip if the script is being compiled off thread.
    if (script->ion == ION_COMPILING_SCRIPT)
        return Method_Skipped;

    // Skip if the code is expected to result in a bailout.
    if (script->ion && script->ion->bailoutExpected())
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
    RootedFunction fun(cx, fp->isFunctionFrame() ? fp->fun() : NULL);
    MethodStatus status = Compile(cx, script, fun, pc, fp->isConstructing());
    if (status != Method_Compiled) {
        if (status == Method_CantCompile)
            ForbidCompilation(cx, script);
        return status;
    }

    if (script->ion->osrPc() != pc)
        return Method_Skipped;

    return Method_Compiled;
}

MethodStatus
ion::CanEnter(JSContext *cx, HandleScript script, StackFrame *fp, bool newType)
{
    JS_ASSERT(ion::IsEnabled(cx));

    // Skip if the script has been disabled.
    if (script->ion == ION_DISABLED_SCRIPT)
        return Method_Skipped;

    // Skip if the script is being compiled off thread.
    if (script->ion == ION_COMPILING_SCRIPT)
        return Method_Skipped;

    // Skip if the code is expected to result in a bailout.
    if (script->ion && script->ion->bailoutExpected())
        return Method_Skipped;

    // If constructing, allocate a new |this| object before building Ion.
    // Creating |this| is done before building Ion because it may change the
    // type information and invalidate compilation results.
    if (fp->isConstructing() && fp->functionThis().isPrimitive()) {
        RootedObject callee(cx, &fp->callee());
        RootedObject obj(cx, js_CreateThisForFunction(cx, callee, newType));
        if (!obj)
            return Method_Skipped;
        fp->functionThis().setObject(*obj);
    }

    // Mark as forbidden if frame can't be handled.
    if (!CheckFrame(fp)) {
        ForbidCompilation(cx, script);
        return Method_CantCompile;
    }

    // Attempt compilation. Returns Method_Compiled if already compiled.
    RootedFunction fun(cx, fp->isFunctionFrame() ? fp->fun() : NULL);
    MethodStatus status = Compile(cx, script, fun, NULL, fp->isConstructing());
    if (status != Method_Compiled) {
        if (status == Method_CantCompile)
            ForbidCompilation(cx, script);
        return status;
    }

    return Method_Compiled;
}

MethodStatus
ion::CanEnterUsingFastInvoke(JSContext *cx, HandleScript script, uint32_t numActualArgs)
{
    JS_ASSERT(ion::IsEnabled(cx));

    // Skip if the code is expected to result in a bailout.
    if (!script->hasIonScript() || script->ion->bailoutExpected())
        return Method_Skipped;

    // Don't handle arguments underflow, to make this work we would have to pad
    // missing arguments with |undefined|.
    if (numActualArgs < script->function()->nargs)
        return Method_Skipped;

    if (!cx->compartment->ensureIonCompartmentExists(cx))
        return Method_Error;

    // This can GC, so afterward, script->ion is not guaranteed to be valid.
    AssertCanGC();
    if (!cx->compartment->ionCompartment()->enterJIT())
        return Method_Error;

    if (!script->ion)
        return Method_Skipped;

    return Method_Compiled;
}

static IonExecStatus
EnterIon(JSContext *cx, StackFrame *fp, void *jitcode)
{
    AssertCanGC();
    JS_CHECK_RECURSION(cx, return IonExec_Aborted);
    JS_ASSERT(ion::IsEnabled(cx));
    JS_ASSERT(CheckFrame(fp));
    JS_ASSERT(!fp->script()->ion->bailoutExpected());

    EnterIonCode enter = cx->compartment->ionCompartment()->enterJIT();

    // maxArgc is the maximum of arguments between the number of actual
    // arguments and the number of formal arguments. It accounts for |this|.
    int maxArgc = 0;
    Value *maxArgv = NULL;
    int numActualArgs = 0;
    RootedValue thisv(cx);

    void *calleeToken;
    if (fp->isFunctionFrame()) {
        // CountArgSlot include |this| and the |scopeChain|.
        maxArgc = CountArgSlots(fp->fun()) - 1; // -1 = discard |scopeChain|
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
    Value result = Int32Value(numActualArgs);
    {
        AssertCompartmentUnchanged pcc(cx);
        IonContext ictx(cx, cx->compartment, NULL);
        IonActivation activation(cx, fp);
        JSAutoResolveFlags rf(cx, RESOLVE_INFER);
        AutoFlushInhibitor afi(cx->compartment->ionCompartment());
        // Single transition point from Interpreter to Ion.
        enter(jitcode, maxArgc, maxArgv, fp, calleeToken, &result);
    }

    if (result.isMagic() && result.whyMagic() == JS_ION_BAILOUT) {
        if (!EnsureHasScopeObjects(cx, cx->fp()))
            return IonExec_Error;
        return IonExec_Bailout;
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
    AssertCanGC();
    RootedScript script(cx, fp->script());
    IonScript *ion = script->ion;
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
ion::SideCannon(JSContext *cx, StackFrame *fp, jsbytecode *pc)
{
    AssertCanGC();
    RootedScript script(cx, fp->script());
    IonScript *ion = script->ion;
    IonCode *code = ion->method();
    void *osrcode = code->raw() + ion->osrEntryOffset();

    JS_ASSERT(ion->osrPc() == pc);

#if JS_TRACE_LOGGING
    TraceLog(TraceLogging::defaultLogger(),
             TraceLogging::ION_SIDE_CANNON_START,
             script);
#endif

    IonExecStatus status = EnterIon(cx, fp, osrcode);

#if JS_TRACE_LOGGING
    if (status == IonExec_Bailout) {
        TraceLog(TraceLogging::defaultLogger(),
                 TraceLogging::ION_SIDE_CANNON_BAIL,
                 script);
    } else {
        TraceLog(TraceLogging::defaultLogger(),
                 TraceLogging::ION_SIDE_CANNON_STOP,
                 script);
    }
#endif

    return status;
}

IonExecStatus
ion::FastInvoke(JSContext *cx, HandleFunction fun, CallArgsList &args)
{
    JS_CHECK_RECURSION(cx, return IonExec_Error);

    RootedScript script(cx, fun->nonLazyScript());
    IonScript *ion = script->ionScript();
    IonCode *code = ion->method();
    void *jitcode = code->raw();

    JS_ASSERT(ion::IsEnabled(cx));
    JS_ASSERT(!script->ion->bailoutExpected());

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

    Value result = Int32Value(args.length());
    JS_ASSERT(args.length() >= fun->nargs);

    JSAutoResolveFlags rf(cx, RESOLVE_INFER);
    args.setActive();
    enter(jitcode, args.length() + 1, args.array() - 1, fp, calleeToken, &result);
    args.setInactive();

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
    AutoAssertNoGC nogc;
    IonSpew(IonSpew_Invalidate, "BEGIN invalidating activation");

    size_t frameno = 1;

    for (IonFrameIterator it(ionTop); !it.done(); ++it, ++frameno) {
        JS_ASSERT_IF(frameno == 1, it.type() == IonFrame_Exit);

#ifdef DEBUG
        switch (it.type()) {
          case IonFrame_Exit:
            IonSpew(IonSpew_Invalidate, "#%d exit frame @ %p", frameno, it.fp());
            break;
          case IonFrame_OptimizedJS:
          {
            JS_ASSERT(it.isScripted());
            IonSpew(IonSpew_Invalidate, "#%d JS frame @ %p, %s:%d (fun: %p, script: %p, pc %p)",
                    frameno, it.fp(), it.script()->filename, it.script()->lineno,
                    it.maybeCallee(), (RawScript)it.script(), it.returnAddressToFp());
            break;
          }
          case IonFrame_Rectifier:
            IonSpew(IonSpew_Invalidate, "#%d rectifier frame @ %p", frameno, it.fp());
            break;
          case IonFrame_Bailed_JS:
            JS_NOT_REACHED("invalid");
            break;
          case IonFrame_Bailed_Rectifier:
            IonSpew(IonSpew_Invalidate, "#%d bailed rectifier frame @ %p", frameno, it.fp());
            break;
          case IonFrame_Osr:
            IonSpew(IonSpew_Invalidate, "#%d osr frame @ %p", frameno, it.fp());
            break;
          case IonFrame_Entry:
            IonSpew(IonSpew_Invalidate, "#%d entry frame @ %p", frameno, it.fp());
            break;
        }
#endif

        if (!it.isScripted())
            continue;

        // See if the frame has already been invalidated.
        if (it.checkInvalidation())
            continue;

        RawScript script = it.script();
        if (!script->hasIonScript())
            continue;

        if (!invalidateAll && !script->ion->invalidated())
            continue;

        IonScript *ionScript = script->ion;

        // Purge ICs before we mark this script as invalidated. This will
        // prevent lastJump_ from appearing to be a bogus pointer, just
        // in case anyone tries to read it.
        ionScript->purgeCaches(script->compartment());

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

        JSCompartment *compartment = script->compartment();
        if (compartment->needsBarrier()) {
            // We're about to remove edges from the JSScript to gcthings
            // embedded in the IonCode. Perform one final trace of the
            // IonCode for the incremental GC, as it must know about
            // those edges.
            ionCode->trace(compartment->barrierTracer());
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
ion::InvalidateAll(FreeOp *fop, JSCompartment *c)
{
    if (!c->ionCompartment())
        return;

    CancelOffThreadIonCompile(c, NULL);

    FinishAllOffThreadCompilations(c->ionCompartment());
    for (IonActivationIterator iter(fop->runtime()); iter.more(); ++iter) {
        if (iter.activation()->compartment() == c) {
            IonContext ictx(NULL, c, NULL);
            AutoFlushCache afc ("InvalidateAll", c->ionCompartment());
            IonSpew(IonSpew_Invalidate, "Invalidating all frames for GC");
            InvalidateActivation(fop, iter.top(), true);
        }
    }
}


void
ion::Invalidate(types::TypeCompartment &types, FreeOp *fop,
                const Vector<types::RecompileInfo> &invalid, bool resetUses)
{
    AutoAssertNoGC nogc;
    IonSpew(IonSpew_Invalidate, "Start invalidation.");
    AutoFlushCache afc ("Invalidate");

    // Add an invalidation reference to all invalidated IonScripts to indicate
    // to the traversal which frames have been invalidated.
    bool anyInvalidation = false;
    for (size_t i = 0; i < invalid.length(); i++) {
        const types::CompilerOutput &co = *invalid[i].compilerOutput(types);
        switch (co.kind()) {
          case types::CompilerOutput::MethodJIT:
            break;
          case types::CompilerOutput::Ion:
          case types::CompilerOutput::ParallelIon:
            JS_ASSERT(co.isValid());
            IonSpew(IonSpew_Invalidate, " Invalidate %s:%u, IonScript %p",
                    co.script->filename, co.script->lineno, co.ion());

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
          case types::CompilerOutput::MethodJIT:
            continue;
          case types::CompilerOutput::Ion:
            break;
          case types::CompilerOutput::ParallelIon:
            executionMode = ParallelExecution;
            break;
        }
        JS_ASSERT(co.isValid());
        UnrootedScript script = co.script;
        IonScript *ionScript = GetIonScript(script, executionMode);

        JSCompartment *compartment = script->compartment();
        if (compartment->needsBarrier()) {
            // We're about to remove edges from the JSScript to gcthings
            // embedded in the IonScript. Perform one final trace of the
            // IonScript for the incremental GC, as it must know about
            // those edges.
            IonScript::Trace(compartment->barrierTracer(), ionScript);
        }

        ionScript->decref(fop);
        SetIonScript(script, executionMode, NULL);
        co.invalidate();

        // Wait for the scripts to get warm again before doing another
        // compile, unless we are recompiling *because* a script got hot.
        if (resetUses)
            script->resetUseCount();
    }
}

void
ion::Invalidate(JSContext *cx, const Vector<types::RecompileInfo> &invalid, bool resetUses)
{
    AutoAssertNoGC nogc;
    ion::Invalidate(cx->compartment->types, cx->runtime->defaultFreeOp(), invalid, resetUses);
}

bool
ion::Invalidate(JSContext *cx, UnrootedScript script, bool resetUses)
{
    AutoAssertNoGC nogc;
    JS_ASSERT(script->hasIonScript());

    Vector<types::RecompileInfo> scripts(cx);
    if (!scripts.append(script->ionScript()->recompileInfo()))
        return false;

    Invalidate(cx, scripts, resetUses);
    return true;
}

void
ion::FinishInvalidation(FreeOp *fop, UnrootedScript script)
{
    if (!script->hasIonScript())
        return;

    /*
     * If this script has Ion code on the stack, invalidation() will return
     * true. In this case we have to wait until destroying it.
     */
    if (!script->ion->invalidated()) {
        types::TypeCompartment &types = script->compartment()->types;
        script->ion->recompileInfo().compilerOutput(types)->invalidate();

        ion::IonScript::Destroy(fop, script->ion);
    }

    /* In all cases, NULL out script->ion to avoid re-entry. */
    script->ion = NULL;
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
ion::ForbidCompilation(JSContext *cx, UnrootedScript script)
{
    IonSpew(IonSpew_Abort, "Disabling Ion compilation of script %s:%d",
            script->filename, script->lineno);

    CancelOffThreadIonCompile(cx->compartment, script);

    if (script->hasIonScript()) {
        // It is only safe to modify script->ion if the script is not currently
        // running, because IonFrameIterator needs to tell what ionScript to
        // use (either the one on the JSScript, or the one hidden in the
        // breadcrumbs Invalidation() leaves). Therefore, if invalidation
        // fails, we cannot disable the script.
        if (!Invalidate(cx, script, false))
            return;
    }

    script->ion = ION_DISABLED_SCRIPT;
}

uint32_t
ion::UsesBeforeIonRecompile(UnrootedScript script, jsbytecode *pc)
{
    JS_ASSERT(pc == script->code || JSOp(*pc) == JSOP_LOOPENTRY);

    uint32_t minUses = js_IonOptions.usesBeforeCompile;
    if (JSOp(*pc) != JSOP_LOOPENTRY || !script->hasAnalysis() || js_IonOptions.eagerCompilation)
        return minUses;

    analyze::LoopAnalysis *loop = script->analysis()->getLoop(pc);
    if (!loop)
        return minUses;

    // It's more efficient to enter outer loops, rather than inner loops, via OSR.
    // To accomplish this, we use a slightly higher threshold for inner loops.
    // Note that we use +1 to prefer non-OSR over OSR.
    return minUses + (loop->depth + 1) * 100;
}

void
AutoFlushCache::updateTop(uintptr_t p, size_t len)
{
    IonContext *ictx = GetIonContext();
    IonCompartment *icmp = ictx->compartment->ionCompartment();
    AutoFlushCache *afc = icmp->flusher();
    afc->update(p, len);
}

AutoFlushCache::AutoFlushCache(const char *nonce, IonCompartment *comp)
  : start_(0),
    stop_(0),
    name_(nonce),
    used_(false)
{
    if (CurrentIonContext() != NULL)
        comp = GetIonContext()->compartment->ionCompartment();
    // If a compartment isn't available, then be a nop, nobody will ever see this flusher
    if (comp) {
        if (comp->flusher())
            IonSpew(IonSpew_CacheFlush, "<%s ", nonce);
        else
            IonSpewCont(IonSpew_CacheFlush, "<%s ", nonce);
        comp->setFlusher(this);
    } else {
        IonSpew(IonSpew_CacheFlush, "<%s DEAD>\n", nonce);
    }
    myCompartment_ = comp;
}

AutoFlushInhibitor::AutoFlushInhibitor(IonCompartment *ic) : ic_(ic), afc(NULL)
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
ion::PurgeCaches(UnrootedScript script, JSCompartment *c) {
    if (script->hasIonScript())
        script->ion->purgeCaches(c);

    if (script->hasParallelIonScript())
        script->ion->purgeCaches(c);
}

size_t
ion::MemoryUsed(UnrootedScript script, JSMallocSizeOfFun mallocSizeOf) {
    size_t result = 0;

    if (script->hasIonScript())
        result += script->ion->sizeOfIncludingThis(mallocSizeOf);

    if (script->hasParallelIonScript())
        result += script->parallelIon->sizeOfIncludingThis(mallocSizeOf);

    return result;
}

void
ion::DestroyIonScripts(FreeOp *fop, UnrootedScript script) {
    if (script->hasIonScript())
        ion::IonScript::Destroy(fop, script->ion);

    if (script->hasParallelIonScript())
        ion::IonScript::Destroy(fop, script->parallelIon);
}

void
ion::TraceIonScripts(JSTracer* trc, UnrootedScript script) {
    if (script->hasIonScript())
        ion::IonScript::Trace(trc, script->ion);

    if (script->hasParallelIonScript())
        ion::IonScript::Trace(trc, script->parallelIon);
}
