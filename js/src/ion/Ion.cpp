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
#include "IonCompartment.h"
#include "CodeGenerator.h"

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

#if JS_TRACE_LOGGING
#include "TraceLogging.h"
#endif

using namespace js;
using namespace js::ion;

// Global variables.
IonOptions ion::js_IonOptions;

// Assert that IonCode is gc::Cell aligned.
JS_STATIC_ASSERT(sizeof(IonCode) % gc::Cell::CellSize == 0);

#ifdef JS_THREADSAFE
static bool IonTLSInitialized = false;
static PRUintn IonTLSIndex;

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

IonCompartment::IonCompartment()
  : execAlloc_(NULL),
    enterJIT_(NULL),
    bailoutHandler_(NULL),
    argumentsRectifier_(NULL),
    invalidator_(NULL),
    functionWrappers_(NULL)
{
}

bool
IonCompartment::initialize(JSContext *cx)
{
    execAlloc_ = cx->runtime->getExecAlloc(cx);
    if (!execAlloc_)
        return false;

    functionWrappers_ = cx->new_<VMWrapperMap>(cx);
    if (!functionWrappers_ || !functionWrappers_->init())
        return false;

    return true;
}

void
IonCompartment::mark(JSTracer *trc, JSCompartment *compartment)
{
    // This function marks Ion code objects that must be kept alive if there is
    // any Ion code currently running. These pointers are marked at the start
    // of incremental GC. Entering Ion code in the middle of an incremental GC
    // triggers a read barrier on both these pointers, so they will still be
    // marked in that case.

    bool mustMarkEnterJIT = false;
    for (IonActivationIterator iter(trc->runtime); iter.more(); ++iter) {
        IonActivation *activation = iter.activation();

        if (activation->compartment() != compartment)
            continue;

        // Activations may be tied to the Method JIT, in which case enterJIT
        // is not present on the activation's call stack.
        if (!activation->entryfp() || activation->entryfp()->callingIntoIon())
            continue;

        // Both OSR and normal function calls depend on the EnterJIT code
        // existing for entrance and exit.
        mustMarkEnterJIT = true;
    }

    // These must be available if we could be running JIT code; they are not
    // traced as normal through IonCode or IonScript objects
    if (mustMarkEnterJIT)
        MarkIonCodeRoot(trc, enterJIT_.unsafeGet(), "enterJIT");

    // functionWrappers_ are not marked because this is a WeakCache of VM
    // function implementations.
}

void
IonCompartment::sweep(FreeOp *fop)
{
    if (enterJIT_ && !IsIonCodeMarked(enterJIT_.unsafeGet()))
        enterJIT_ = NULL;
    if (bailoutHandler_ && !IsIonCodeMarked(bailoutHandler_.unsafeGet()))
        bailoutHandler_ = NULL;
    if (argumentsRectifier_ && !IsIonCodeMarked(argumentsRectifier_.unsafeGet()))
        argumentsRectifier_ = NULL;
    if (invalidator_ && !IsIonCodeMarked(invalidator_.unsafeGet()))
        invalidator_ = NULL;
    if (preBarrier_ && !IsIonCodeMarked(preBarrier_.unsafeGet()))
        preBarrier_ = NULL;

    for (size_t i = 0; i < bailoutTables_.length(); i++) {
        if (bailoutTables_[i] && !IsIonCodeMarked(bailoutTables_[i].unsafeGet()))
            bailoutTables_[i] = NULL;
    }

    // Sweep cache of VM function implementations.
    functionWrappers_->sweep(fop);
}

IonCode *
IonCompartment::getBailoutTable(const FrameSizeClass &frameClass)
{
    JS_ASSERT(frameClass != FrameSizeClass::None());
    return bailoutTables_[frameClass.classId()];
}

IonCode *
IonCompartment::getBailoutTable(JSContext *cx, const FrameSizeClass &frameClass)
{
    uint32 id = frameClass.classId();

    if (id >= bailoutTables_.length()) {
        size_t numToPush = id - bailoutTables_.length() + 1;
        if (!bailoutTables_.reserve(bailoutTables_.length() + numToPush))
            return NULL;
        for (size_t i = 0; i < numToPush; i++)
            bailoutTables_.infallibleAppend(NULL);
    }

    if (!bailoutTables_[id])
        bailoutTables_[id] = generateBailoutTable(cx, id);

    return bailoutTables_[id];
}

IonCompartment::~IonCompartment()
{
    Foreground::delete_(functionWrappers_);
}

IonActivation::IonActivation(JSContext *cx, StackFrame *fp)
  : cx_(cx),
    compartment_(cx->compartment),
    prev_(cx->runtime->ionActivation),
    entryfp_(fp),
    bailout_(NULL),
    prevIonTop_(cx->runtime->ionTop),
    prevIonJSContext_(cx->runtime->ionJSContext)
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
IonCode::New(JSContext *cx, uint8 *code, uint32 bufferSize, JSC::ExecutablePool *pool)
{
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
    if (invalidated()) {
        // Note that since we're invalidated, we won't mark the precious
        // invalidator thunk referenced in the epilogue. We don't move
        // executable code so the actual reference is okay, we just need to
        // make sure it says alive before we return.
        IonCompartment *ion = compartment()->ionCompartment();
        MarkIonCodeUnbarriered(trc, ion->getInvalidationThunkAddr(), "invalidator");
        return;
    }

    if (jumpRelocTableBytes_) {
        uint8 *start = code_ + jumpRelocTableOffset();
        CompactBufferReader reader(start, start + jumpRelocTableBytes_);
        MacroAssembler::TraceJumpRelocations(trc, this, reader);
    }
    if (dataRelocTableBytes_) {
        uint8 *start = code_ + dataRelocTableOffset();
        CompactBufferReader reader(start, start + dataRelocTableBytes_);
        MacroAssembler::TraceDataRelocations(trc, this, reader);
    }
}

void
IonCode::finalize(FreeOp *fop)
{
    JS_ASSERT(!fop->onBackgroundThread());

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
    refcount_(0),
    slowCallCount(0)
{
}
static const int DataAlignment = 4;
IonScript *
IonScript::New(JSContext *cx, uint32 frameSlots, uint32 frameSize, size_t snapshotsSize,
               size_t bailoutEntries, size_t constants, size_t safepointIndices,
               size_t osiIndices, size_t cacheEntries, size_t prebarrierEntries,
               size_t safepointsSize)
{
    if (snapshotsSize >= MAX_BUFFER_SIZE ||
        (bailoutEntries >= MAX_BUFFER_SIZE / sizeof(uint32)))
    {
        js_ReportOutOfMemory(cx);
        return NULL;
    }

    // This should not overflow on x86, because the memory is already allocated
    // *somewhere* and if their total overflowed there would be no memory left
    // at all.
    size_t paddedSnapshotsSize = AlignBytes(snapshotsSize, DataAlignment);
    size_t paddedBailoutSize = AlignBytes(bailoutEntries * sizeof(uint32), DataAlignment);
    size_t paddedConstantsSize = AlignBytes(constants * sizeof(Value), DataAlignment);
    size_t paddedSafepointIndicesSize = AlignBytes(safepointIndices * sizeof(SafepointIndex), DataAlignment);
    size_t paddedOsiIndicesSize = AlignBytes(osiIndices * sizeof(OsiIndex), DataAlignment);
    size_t paddedCacheEntriesSize = AlignBytes(cacheEntries * sizeof(IonCache), DataAlignment);
    size_t paddedPrebarrierEntriesSize =
        AlignBytes(prebarrierEntries * sizeof(CodeOffsetLabel), DataAlignment);
    size_t paddedSafepointSize = AlignBytes(safepointsSize, DataAlignment);
    size_t bytes = paddedSnapshotsSize +
                   paddedBailoutSize +
                   paddedConstantsSize +
                   paddedSafepointIndicesSize+
                   paddedOsiIndicesSize +
                   paddedCacheEntriesSize +
                   paddedPrebarrierEntriesSize +
                   paddedSafepointSize;
    uint8 *buffer = (uint8 *)cx->malloc_(sizeof(IonScript) + bytes);
    if (!buffer)
        return NULL;

    IonScript *script = reinterpret_cast<IonScript *>(buffer);
    new (script) IonScript();

    uint32 offsetCursor = sizeof(IonScript);

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

    script->frameSlots_ = frameSlots;
    script->frameSize_ = frameSize;

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
    memcpy((uint8 *)this + snapshots_, writer->buffer(), snapshotsSize_);
}

void
IonScript::copySafepoints(const SafepointWriter *writer)
{
    JS_ASSERT(writer->size() == safepointsSize_);
    memcpy((uint8 *)this + safepointsStart_, writer->buffer(), safepointsSize_);
}

void
IonScript::copyBailoutTable(const SnapshotOffset *table)
{
    memcpy(bailoutTable(), table, bailoutEntries_ * sizeof(uint32));
}

void
IonScript::copyConstants(const HeapValue *vp)
{
    for (size_t i = 0; i < constantEntries_; i++)
        constants()[i].init(vp[i]);
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
IonScript::getSafepointIndex(uint32 disp) const
{
    JS_ASSERT(safepointIndexEntries_ > 0);

    const SafepointIndex *table = safepointIndices();
    if (safepointIndexEntries_ == 1) {
        JS_ASSERT(disp == table[0].displacement());
        return &table[0];
    }

    size_t minEntry = 0;
    size_t maxEntry = safepointIndexEntries_ - 1;
    uint32 min = table[minEntry].displacement();
    uint32 max = table[maxEntry].displacement();

    // Raise if the element is not in the list.
    JS_ASSERT(min <= disp && disp <= max);

    // Approximate the location of the FrameInfo.
    size_t guess = (disp - min) * (maxEntry - minEntry) / (max - min) + minEntry;
    uint32 guessDisp = table[guess].displacement();

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
IonScript::getOsiIndex(uint32 disp) const
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
IonScript::getOsiIndex(uint8 *retAddr) const
{
    IonSpew(IonSpew_Invalidate, "IonScript %p has method %p raw %p", (void *) this, (void *)
            method(), method()->raw());

    JS_ASSERT(containsCodeAddress(retAddr));
    uint32 disp = retAddr - method()->raw();
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
IonScript::purgeCaches()
{
    for (size_t i = 0; i < numCaches(); i++)
        getCache(i).reset();
}

void
ion::ToggleBarriers(JSCompartment *comp, bool needs)
{
    for (gc::CellIterUnderGC i(comp, gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        if (script->hasIonScript())
            script->ion->toggleBarriers(needs);
    }
}

namespace js {
namespace ion {

static bool
BuildMIR(IonBuilder &builder, MIRGraph &graph)
{
    if (!builder.build())
        return false;
    IonSpewPass("BuildSSA");
    // Note: don't call AssertGraphCoherency before SplitCriticalEdges,
    // the graph is not in RPO at this point.

    if (!SplitCriticalEdges(&builder, graph))
        return false;
    IonSpewPass("Split Critical Edges");
    AssertGraphCoherency(graph);

    if (!RenumberBlocks(graph))
        return false;
    IonSpewPass("Renumber Blocks");
    AssertGraphCoherency(graph);

    if (!BuildDominatorTree(graph))
        return false;
    // No spew: graph not changed.

    // This must occur before any code elimination.
    if (!EliminatePhis(graph))
        return false;
    IonSpewPass("Eliminate phis");
    AssertGraphCoherency(graph);

    if (!BuildPhiReverseMapping(graph))
        return false;
    // No spew: graph not changed.

    // This pass also removes copies.
    if (!ApplyTypeInformation(graph))
        return false;
    IonSpewPass("Apply types");
    AssertGraphCoherency(graph);

    // Alias analysis is required for LICM and GVN so that we don't move
    // loads across stores.
    if (js_IonOptions.licm || js_IonOptions.gvn) {
        AliasAnalysis analysis(graph);
        if (!analysis.analyze())
            return false;
        IonSpewPass("Alias analysis");
        AssertGraphCoherency(graph);
    }

    if (js_IonOptions.edgeCaseAnalysis) {
        EdgeCaseAnalysis edgeCaseAnalysis(graph);
        if (!edgeCaseAnalysis.analyzeEarly())
            return false;
        IonSpewPass("Edge Case Analysis (Early)");
        AssertGraphCoherency(graph);
    }

    if (js_IonOptions.gvn) {
        ValueNumberer gvn(graph, js_IonOptions.gvnIsOptimistic);
        if (!gvn.analyze())
            return false;
        IonSpewPass("GVN");
        AssertGraphCoherency(graph);
    }

    if (js_IonOptions.rangeAnalysis) {
        RangeAnalysis r(graph);
        if (!r.addBetaNobes())
            return false;
        IonSpewPass("Beta");
        AssertGraphCoherency(graph);

        if (!r.analyze())
            return false;
        IonSpewPass("Range Analysis");
        AssertGraphCoherency(graph);

        if (!r.removeBetaNobes())
            return false;
        IonSpewPass("De-Beta");
        AssertGraphCoherency(graph);
    }

    if (!EliminateDeadCode(graph))
        return false;
    IonSpewPass("DCE");
    AssertGraphCoherency(graph);

    if (js_IonOptions.licm) {
        LICM licm(graph);
        if (!licm.analyze())
            return false;
        IonSpewPass("LICM");
        AssertGraphCoherency(graph);
    }

    if (js_IonOptions.edgeCaseAnalysis) {
        EdgeCaseAnalysis edgeCaseAnalysis(graph);
        if (!edgeCaseAnalysis.analyzeLate())
            return false;
        IonSpewPass("Edge Case Analysis (Late)");
        AssertGraphCoherency(graph);
    }

    // Note: bounds check elimination has to run after all other passes that
    // move instructions. Since bounds check uses are replaced with the actual
    // index, code motion after this pass could incorrectly move a load or
    // store before its bounds check.
    if (!EliminateRedundantBoundsChecks(graph))
        return false;
    IonSpewPass("Bounds Check Elimination");
    AssertGraphCoherency(graph);

    return true;
}

static bool
GenerateCode(IonBuilder &builder, MIRGraph &graph)
{
    LIRGraph lir(graph);
    LIRGenerator lirgen(&builder, graph, lir);
    if (!lirgen.generate())
        return false;
    IonSpewPass("Generate LIR");

    if (js_IonOptions.lsra) {
        LinearScanAllocator regalloc(&lirgen, lir);
        if (!regalloc.go())
            return false;
        IonSpewPass("Allocate Registers", &regalloc);
    }

    CodeGenerator codegen(&builder, lir);
    if (!codegen.generate())
        return false;
    // No spew: graph not changed.

    return true;
}

/* static */ bool
TestCompiler(IonBuilder &builder, MIRGraph &graph)
{
    IonSpewNewFunction(&graph, builder.script);

    if (!BuildMIR(builder, graph))
        return false;

    if (!GenerateCode(builder, graph))
        return false;

    IonSpewEndFunction();

    return true;
}

template <bool Compiler(IonBuilder &, MIRGraph &)>
static bool
IonCompile(JSContext *cx, JSScript *script, JSFunction *fun, jsbytecode *osrPc, bool constructing)
{
#if JS_TRACE_LOGGING
    AutoTraceLog logger(TraceLogging::defaultLogger(),
                        TraceLogging::ION_COMPILE_START,
                        TraceLogging::ION_COMPILE_STOP,
                        script);
#endif

    TempAllocator temp(&cx->tempLifoAlloc());
    IonContext ictx(cx, cx->compartment, &temp);

    if (!cx->compartment->ensureIonCompartmentExists(cx))
        return false;

    MIRGraph graph(&temp);
    CompileInfo *info = cx->tempLifoAlloc().new_<CompileInfo>(script, fun, osrPc, constructing);
    if (!info)
        return false;

    types::AutoEnterTypeInference enter(cx, true);
    TypeInferenceOracle oracle;

    if (!oracle.init(cx, script))
        return false;

    types::AutoEnterCompilation enterCompiler(cx, types::AutoEnterCompilation::Ion);
    enterCompiler.init(script, false, 0);
    AutoCompilerRoots roots(script->compartment()->rt);

    IonBuilder builder(cx, &temp, &graph, &oracle, info);
    if (!Compiler(builder, graph)) {
        IonSpew(IonSpew_Abort, "IM Compilation failed.");
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
CheckScript(JSScript *script)
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
CheckScriptSize(JSScript *script)
{
    if (!js_IonOptions.limitScriptSize)
        return true;

    static const uint32_t MAX_SCRIPT_SIZE = 1500;
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

template <bool Compiler(IonBuilder &, MIRGraph &)>
static MethodStatus
Compile(JSContext *cx, JSScript *script, JSFunction *fun, jsbytecode *osrPc, bool constructing)
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
        if (script->length < js_IonOptions.smallFunctionMaxBytecodeLength) {
            if (script->getUseCount() < js_IonOptions.smallFunctionUsesBeforeCompile)
                return Method_Skipped;
        } else {
            if (script->getUseCount() < js_IonOptions.usesBeforeCompile)
                return Method_Skipped;
        }
    } else {
        if (script->incUseCount() < js_IonOptions.usesBeforeCompileNoJaeger)
            return Method_Skipped;
    }

    if (!IonCompile<Compiler>(cx, script, fun, osrPc, constructing))
        return Method_CantCompile;

    // Compilation succeeded, but we invalidated right away.
    return script->hasIonScript() ? Method_Compiled : Method_Skipped;
}

} // namespace ion
} // namespace js

// Decide if a transition from interpreter execution to Ion code should occur.
// May compile or recompile the target JSScript.
MethodStatus
ion::CanEnterAtBranch(JSContext *cx, JSScript *script, StackFrame *fp, jsbytecode *pc)
{
    JS_ASSERT(ion::IsEnabled(cx));
    JS_ASSERT((JSOp)*pc == JSOP_LOOPENTRY);

    // Skip if the script has been disabled.
    if (script->ion == ION_DISABLED_SCRIPT)
        return Method_Skipped;

    // Skip if the code is expected to result in a bailout.
    if (script->ion && script->ion->bailoutExpected())
        return Method_Skipped;

    // Optionally ignore on user request.
    if (!js_IonOptions.osr)
        return Method_Skipped;

    // Mark as forbidden if frame can't be handled.
    if (!CheckFrame(fp)) {
        ForbidCompilation(script);
        return Method_CantCompile;
    }

    // Attempt compilation. Returns Method_Compiled if already compiled.
    JSFunction *fun = fp->isFunctionFrame() ? fp->fun() : NULL;
    MethodStatus status = Compile<TestCompiler>(cx, script, fun, pc, false);
    if (status != Method_Compiled) {
        if (status == Method_CantCompile)
            ForbidCompilation(script);
        return status;
    }

    if (script->ion->osrPc() != pc)
        return Method_Skipped;

    // This can GC, so afterward, script->ion is not guaranteed to be valid.
    if (!cx->compartment->ionCompartment()->enterJIT(cx))
        return Method_Error;

    if (!script->ion)
        return Method_Skipped;

    return Method_Compiled;
}

MethodStatus
ion::CanEnter(JSContext *cx, JSScript *script, StackFrame *fp, bool newType)
{
    JS_ASSERT(ion::IsEnabled(cx));

    // Skip if the script has been disabled.
    if (script->ion == ION_DISABLED_SCRIPT)
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
        ForbidCompilation(script);
        return Method_CantCompile;
    }

    // Attempt compilation. Returns Method_Compiled if already compiled.
    JSFunction *fun = fp->isFunctionFrame() ? fp->fun() : NULL;
    MethodStatus status = Compile<TestCompiler>(cx, script, fun, NULL, fp->isConstructing());
    if (status != Method_Compiled) {
        if (status == Method_CantCompile)
            ForbidCompilation(script);
        return status;
    }

    // This can GC, so afterward, script->ion is not guaranteed to be valid.
    if (!cx->compartment->ionCompartment()->enterJIT(cx))
        return Method_Error;

    if (!script->ion)
        return Method_Skipped;

    return Method_Compiled;
}

static IonExecStatus
EnterIon(JSContext *cx, StackFrame *fp, void *jitcode)
{
    JS_CHECK_RECURSION(cx, return IonExec_Error);
    JS_ASSERT(ion::IsEnabled(cx));
    JS_ASSERT(CheckFrame(fp));
    JS_ASSERT(!fp->script()->ion->bailoutExpected());

    EnterIonCode enter = cx->compartment->ionCompartment()->enterJITInfallible();

    // maxArgc is the maximum of arguments between the number of actual
    // arguments and the number of formal arguments. It accounts for |this|.
    int maxArgc = 0;
    Value *maxArgv = NULL;
    int numActualArgs = 0;

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
    }

    // Caller must construct |this| before invoking the Ion function.
    JS_ASSERT_IF(fp->isConstructing(), fp->functionThis().isObject());

    Value result = Int32Value(numActualArgs);
    {
        AssertCompartmentUnchanged pcc(cx);
        IonContext ictx(cx, cx->compartment, NULL);
        IonActivation activation(cx, fp);
        JSAutoResolveFlags rf(cx, RESOLVE_INFER);

        // Single transition point from Interpreter to Ion.
        enter(jitcode, maxArgc, maxArgv, fp, calleeToken, &result);
    }

    if (result.isMagic() && result.whyMagic() == JS_ION_BAILOUT) {
        if (!EnsureHasCallObject(cx, cx->fp()))
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
    JSScript *script = fp->script();
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
    JSScript *script = fp->script();
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

static void
InvalidateActivation(FreeOp *fop, uint8 *ionTop, bool invalidateAll)
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
          case IonFrame_JS:
          {
            JS_ASSERT(it.isScripted());
            IonSpew(IonSpew_Invalidate, "#%d JS frame @ %p, %s:%d (fun: %p, script: %p, pc %p)",
                    frameno, it.fp(), it.script()->filename, it.script()->lineno,
                    it.maybeCallee(), it.script(), it.returnAddressToFp());
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

        JSScript *script = it.script();
        if (!script->hasIonScript())
            continue;

        if (!invalidateAll && !script->ion->invalidated())
            continue;

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

        IonScript *ionScript = script->ion;
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
    for (IonActivationIterator iter(fop->runtime()); iter.more(); ++iter) {
        if (iter.activation()->compartment() == c) {
            IonSpew(IonSpew_Invalidate, "Invalidating all frames for GC");
            InvalidateActivation(fop, iter.top(), true);
        }
    }
}

void
ion::Invalidate(FreeOp *fop, const Vector<types::CompilerOutput> &invalid, bool resetUses)
{
    IonSpew(IonSpew_Invalidate, "Start invalidation.");
    // Add an invalidation reference to all invalidated IonScripts to indicate
    // to the traversal which frames have been invalidated.
    bool anyInvalidation = false;
    for (size_t i = 0; i < invalid.length(); i++) {
        const types::CompilerOutput &co = invalid[i];
        JS_ASSERT(co.isValid());
        if (co.isIon()) {
            JS_ASSERT(co.script->hasIonScript() && co.out.ion == co.script->ionScript());
            IonSpew(IonSpew_Invalidate, " Invalidate %s:%u, IonScript %p",
                    co.script->filename, co.script->lineno, co.out.ion);

            // Keep the ion script alive during the invalidation and flag this
            // ionScript as being invalidated.  This increment is removed by the
            // loop after the calls to InvalidateActivation.
            co.out.ion->incref();
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
        const types::CompilerOutput &co = invalid[i];
        if (co.isIon()) {
            JSScript *script = co.script;
            IonScript *ionScript = co.out.ion;

            JSCompartment *compartment = script->compartment();
            if (compartment->needsBarrier()) {
                // We're about to remove edges from the JSScript to gcthings
                // embedded in the IonScript. Perform one final trace of the
                // IonScript for the incremental GC, as it must know about
                // those edges.
                IonScript::Trace(compartment->barrierTracer(), ionScript);
            }

            co.out.ion->decref(fop);
            script->ion = NULL;
        }
    }

    // Wait for the scripts to get warm again before doing another compile,
    // unless we are recompiling *because* a script got hot.
    if (resetUses) {
        for (size_t i = 0; i < invalid.length(); i++)
            invalid[i].script->resetUseCount();
    }
}

bool
ion::Invalidate(JSContext *cx, JSScript *script, bool resetUses)
{
    JS_ASSERT(script->hasIonScript());

    Vector<types::CompilerOutput> scripts(cx);
    types::CompilerOutput co(script);
    if (!scripts.append(co))
        return false;

    Invalidate(cx->runtime->defaultFreeOp(), scripts, resetUses);
    return true;
}

void
ion::FinishInvalidation(FreeOp *fop, JSScript *script)
{
    if (!script->hasIonScript())
        return;

    /*
     * If this script has Ion code on the stack, invalidation() will return
     * true. In this case we have to wait until destroying it.
     */
    if (!script->ion->invalidated())
        ion::IonScript::Destroy(fop, script->ion);

    /* In all cases, NULL out script->ion to avoid re-entry. */
    script->ion = NULL;
}

void
ion::MarkFromIon(JSCompartment *comp, Value *vp)
{
    gc::MarkValueUnbarriered(comp->barrierTracer(), vp, "write barrier");
}

void
ion::ForbidCompilation(JSScript *script)
{
    IonSpew(IonSpew_Abort, "Disabling Ion compilation of script %s:%d",
            script->filename, script->lineno);
    script->ion = ION_DISABLED_SCRIPT;
}

uint32_t
ion::UsesBeforeIonRecompile(JSScript *script, jsbytecode *pc)
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

int js::ion::LabelBase::id_count = 0;

