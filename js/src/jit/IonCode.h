/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonCode_h
#define jit_IonCode_h

#include "mozilla/Atomics.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"

#include "jsinfer.h"
#include "jstypes.h"

#include "gc/Heap.h"
#include "jit/ExecutableAllocator.h"
#include "jit/IonOptimizationLevels.h"
#include "jit/IonTypes.h"

namespace js {

class AsmJSModule;

namespace jit {

class MacroAssembler;
class CodeOffsetLabel;
class PatchableBackedge;

class JitCode : public gc::BarrieredCell<JitCode>
{
  protected:
    uint8_t *code_;
    ExecutablePool *pool_;
    uint32_t bufferSize_;             // Total buffer size. Does not include headerSize_.
    uint32_t insnSize_;               // Instruction stream size.
    uint32_t dataSize_;               // Size of the read-only data area.
    uint32_t jumpRelocTableBytes_;    // Size of the jump relocation table.
    uint32_t dataRelocTableBytes_;    // Size of the data relocation table.
    uint32_t preBarrierTableBytes_;   // Size of the prebarrier table.
    uint8_t headerSize_ : 5;          // Number of bytes allocated before codeStart.
    uint8_t kind_ : 3;                // jit::CodeKind, for the memory reporters.
    bool invalidated_ : 1;            // Whether the code object has been invalidated.
                                      // This is necessary to prevent GC tracing.
    bool hasBytecodeMap_ : 1;         // Whether the code object has been registered with
                                      // native=>bytecode mapping tables.

#if JS_BITS_PER_WORD == 32
    // Ensure JitCode is gc::Cell aligned.
    uint32_t padding_;
#endif

    JitCode()
      : code_(nullptr),
        pool_(nullptr)
    { }
    JitCode(uint8_t *code, uint32_t bufferSize, uint32_t headerSize, ExecutablePool *pool,
            CodeKind kind)
      : code_(code),
        pool_(pool),
        bufferSize_(bufferSize),
        insnSize_(0),
        dataSize_(0),
        jumpRelocTableBytes_(0),
        dataRelocTableBytes_(0),
        preBarrierTableBytes_(0),
        headerSize_(headerSize),
        kind_(kind),
        invalidated_(false),
        hasBytecodeMap_(false)
    {
        MOZ_ASSERT(CodeKind(kind_) == kind);
        MOZ_ASSERT(headerSize_ == headerSize);
    }

    uint32_t dataOffset() const {
        return insnSize_;
    }
    uint32_t jumpRelocTableOffset() const {
        return dataOffset() + dataSize_;
    }
    uint32_t dataRelocTableOffset() const {
        return jumpRelocTableOffset() + jumpRelocTableBytes_;
    }
    uint32_t preBarrierTableOffset() const {
        return dataRelocTableOffset() + dataRelocTableBytes_;
    }

  public:
    uint8_t *raw() const {
        return code_;
    }
    uint8_t *rawEnd() const {
        return code_ + insnSize_;
    }
    size_t instructionsSize() const {
        return insnSize_;
    }
    void trace(JSTracer *trc);
    void finalize(FreeOp *fop);
    void setInvalidated() {
        invalidated_ = true;
    }

    void setHasBytecodeMap() {
        hasBytecodeMap_ = true;
    }

    void togglePreBarriers(bool enabled);

    // If this JitCode object has been, effectively, corrupted due to
    // invalidation patching, then we have to remember this so we don't try and
    // trace relocation entries that may now be corrupt.
    bool invalidated() const {
        return !!invalidated_;
    }

    template <typename T> T as() const {
        return JS_DATA_TO_FUNC_PTR(T, raw());
    }

    void copyFrom(MacroAssembler &masm);

    static JitCode *FromExecutable(uint8_t *buffer) {
        JitCode *code = *(JitCode **)(buffer - sizeof(JitCode *));
        JS_ASSERT(code->raw() == buffer);
        return code;
    }

    static size_t offsetOfCode() {
        return offsetof(JitCode, code_);
    }

    uint8_t *jumpRelocTable() {
        return code_ + jumpRelocTableOffset();
    }

    // Allocates a new JitCode object which will be managed by the GC. If no
    // object can be allocated, nullptr is returned. On failure, |pool| is
    // automatically released, so the code may be freed.
    template <AllowGC allowGC>
    static JitCode *New(JSContext *cx, uint8_t *code, uint32_t bufferSize, uint32_t headerSize,
                        ExecutablePool *pool, CodeKind kind);

  public:
    static inline ThingRootKind rootKind() { return THING_ROOT_JIT_CODE; }
};

class SnapshotWriter;
class RecoverWriter;
class SafepointWriter;
class SafepointIndex;
class OsiIndex;
class IonCache;
struct PatchableBackedgeInfo;
struct CacheLocation;

// Describes a single AsmJSModule which jumps (via an FFI exit with the given
// index) directly into an IonScript.
struct DependentAsmJSModuleExit
{
    const AsmJSModule *module;
    size_t exitIndex;

    DependentAsmJSModuleExit(const AsmJSModule *module, size_t exitIndex)
      : module(module),
        exitIndex(exitIndex)
    { }
};

// An IonScript attaches Ion-generated information to a JSScript.
struct IonScript
{
  private:
    // Code pointer containing the actual method.
    PreBarrieredJitCode method_;

    // Deoptimization table used by this method.
    PreBarrieredJitCode deoptTable_;

    // Entrypoint for OSR, or nullptr.
    jsbytecode *osrPc_;

    // Offset to OSR entrypoint from method_->raw(), or 0.
    uint32_t osrEntryOffset_;

    // Offset to entrypoint skipping type arg check from method_->raw().
    uint32_t skipArgCheckEntryOffset_;

    // Offset of the invalidation epilogue (which pushes this IonScript
    // and calls the invalidation thunk).
    uint32_t invalidateEpilogueOffset_;

    // The offset immediately after the IonScript immediate.
    // NOTE: technically a constant delta from
    // |invalidateEpilogueOffset_|, so we could hard-code this
    // per-platform if we want.
    uint32_t invalidateEpilogueDataOffset_;

    // Number of times this script bailed out without invalidation.
    uint32_t numBailouts_;

    // Flag set when it is likely that one of our (transitive) call
    // targets is not compiled.  Used in ForkJoin.cpp to decide when
    // we should add call targets to the worklist.
    mozilla::Atomic<bool, mozilla::Relaxed> hasUncompiledCallTarget_;

    // Flag set when this script is used as an entry script to parallel
    // execution. If this is true, then the parent JSScript must be in its
    // JitCompartment's parallel entry script set.
    bool isParallelEntryScript_;

    // Flag set if IonScript was compiled with SPS profiling enabled.
    bool hasSPSInstrumentation_;

    // Flag for if this script is getting recompiled.
    uint32_t recompiling_;

    // Any kind of data needed by the runtime, these can be either cache
    // information or profiling info.
    uint32_t runtimeData_;
    uint32_t runtimeSize_;

    // State for polymorphic caches in the compiled code. All caches are stored
    // in the runtimeData buffer and indexed by the cacheIndex which give a
    // relative offset in the runtimeData array.
    uint32_t cacheIndex_;
    uint32_t cacheEntries_;

    // Map code displacement to safepoint / OSI-patch-delta.
    uint32_t safepointIndexOffset_;
    uint32_t safepointIndexEntries_;

    // Offset to and length of the safepoint table in bytes.
    uint32_t safepointsStart_;
    uint32_t safepointsSize_;

    // Number of bytes this function reserves on the stack.
    uint32_t frameSlots_;

    // Frame size is the value that can be added to the StackPointer along
    // with the frame prefix to get a valid IonJSFrameLayout.
    uint32_t frameSize_;

    // Table mapping bailout IDs to snapshot offsets.
    uint32_t bailoutTable_;
    uint32_t bailoutEntries_;

    // Map OSI-point displacement to snapshot.
    uint32_t osiIndexOffset_;
    uint32_t osiIndexEntries_;

    // Offset from the start of the code buffer to its snapshot buffer.
    uint32_t snapshots_;
    uint32_t snapshotsListSize_;
    uint32_t snapshotsRVATableSize_;

    // List of instructions needed to recover stack frames.
    uint32_t recovers_;
    uint32_t recoversSize_;

    // Constant table for constants stored in snapshots.
    uint32_t constantTable_;
    uint32_t constantEntries_;

    // List of scripts that we call.
    //
    // Currently this is only non-nullptr for parallel IonScripts.
    uint32_t callTargetList_;
    uint32_t callTargetEntries_;

    // List of patchable backedges which are threaded into the runtime's list.
    uint32_t backedgeList_;
    uint32_t backedgeEntries_;

    // Number of references from invalidation records.
    uint32_t refcount_;

    // If this is a parallel script, the number of major GC collections it has
    // been idle, otherwise 0.
    //
    // JSScripts with parallel IonScripts are preserved across GC if the
    // parallel age is < MAX_PARALLEL_AGE.
    uint32_t parallelAge_;

    // Identifier of the compilation which produced this code.
    types::RecompileInfo recompileInfo_;

    // The optimization level this script was compiled in.
    OptimizationLevel optimizationLevel_;

    // Number of times we tried to enter this script via OSR but failed due to
    // a LOOPENTRY pc other than osrPc_.
    uint32_t osrPcMismatchCounter_;

    // If non-null, the list of AsmJSModules
    // that contain an optimized call directly into this IonScript.
    Vector<DependentAsmJSModuleExit> *dependentAsmJSModules;

  private:
    inline uint8_t *bottomBuffer() {
        return reinterpret_cast<uint8_t *>(this);
    }
    inline const uint8_t *bottomBuffer() const {
        return reinterpret_cast<const uint8_t *>(this);
    }

  public:
    SnapshotOffset *bailoutTable() {
        return (SnapshotOffset *) &bottomBuffer()[bailoutTable_];
    }
    PreBarrieredValue *constants() {
        return (PreBarrieredValue *) &bottomBuffer()[constantTable_];
    }
    const SafepointIndex *safepointIndices() const {
        return const_cast<IonScript *>(this)->safepointIndices();
    }
    SafepointIndex *safepointIndices() {
        return (SafepointIndex *) &bottomBuffer()[safepointIndexOffset_];
    }
    const OsiIndex *osiIndices() const {
        return const_cast<IonScript *>(this)->osiIndices();
    }
    OsiIndex *osiIndices() {
        return (OsiIndex *) &bottomBuffer()[osiIndexOffset_];
    }
    uint32_t *cacheIndex() {
        return (uint32_t *) &bottomBuffer()[cacheIndex_];
    }
    uint8_t *runtimeData() {
        return  &bottomBuffer()[runtimeData_];
    }
    JSScript **callTargetList() {
        return (JSScript **) &bottomBuffer()[callTargetList_];
    }
    PatchableBackedge *backedgeList() {
        return (PatchableBackedge *) &bottomBuffer()[backedgeList_];
    }
    bool addDependentAsmJSModule(JSContext *cx, DependentAsmJSModuleExit exit);
    void removeDependentAsmJSModule(DependentAsmJSModuleExit exit) {
        if (!dependentAsmJSModules)
            return;
        for (size_t i = 0; i < dependentAsmJSModules->length(); i++) {
            if (dependentAsmJSModules->begin()[i].module == exit.module &&
                dependentAsmJSModules->begin()[i].exitIndex == exit.exitIndex)
            {
                dependentAsmJSModules->erase(dependentAsmJSModules->begin() + i);
                break;
            }
        }
    }

  private:
    void trace(JSTracer *trc);

  public:
    // Do not call directly, use IonScript::New. This is public for cx->new_.
    IonScript();

    static IonScript *New(JSContext *cx, types::RecompileInfo recompileInfo,
                          uint32_t frameLocals, uint32_t frameSize,
                          size_t snapshotsListSize, size_t snapshotsRVATableSize,
                          size_t recoversSize, size_t bailoutEntries,
                          size_t constants, size_t safepointIndexEntries,
                          size_t osiIndexEntries, size_t cacheEntries,
                          size_t runtimeSize, size_t safepointsSize,
                          size_t callTargetEntries, size_t backedgeEntries,
                          OptimizationLevel optimizationLevel);
    static void Trace(JSTracer *trc, IonScript *script);
    static void Destroy(FreeOp *fop, IonScript *script);

    static inline size_t offsetOfMethod() {
        return offsetof(IonScript, method_);
    }
    static inline size_t offsetOfOsrEntryOffset() {
        return offsetof(IonScript, osrEntryOffset_);
    }
    static inline size_t offsetOfSkipArgCheckEntryOffset() {
        return offsetof(IonScript, skipArgCheckEntryOffset_);
    }
    static inline size_t offsetOfRefcount() {
        return offsetof(IonScript, refcount_);
    }
    static inline size_t offsetOfRecompiling() {
        return offsetof(IonScript, recompiling_);
    }

  public:
    JitCode *method() const {
        return method_;
    }
    void setMethod(JitCode *code) {
        JS_ASSERT(!invalidated());
        method_ = code;
    }
    void setDeoptTable(JitCode *code) {
        deoptTable_ = code;
    }
    void setOsrPc(jsbytecode *osrPc) {
        osrPc_ = osrPc;
    }
    jsbytecode *osrPc() const {
        return osrPc_;
    }
    void setOsrEntryOffset(uint32_t offset) {
        JS_ASSERT(!osrEntryOffset_);
        osrEntryOffset_ = offset;
    }
    uint32_t osrEntryOffset() const {
        return osrEntryOffset_;
    }
    void setSkipArgCheckEntryOffset(uint32_t offset) {
        JS_ASSERT(!skipArgCheckEntryOffset_);
        skipArgCheckEntryOffset_ = offset;
    }
    uint32_t getSkipArgCheckEntryOffset() const {
        return skipArgCheckEntryOffset_;
    }
    bool containsCodeAddress(uint8_t *addr) const {
        return method()->raw() <= addr && addr <= method()->raw() + method()->instructionsSize();
    }
    bool containsReturnAddress(uint8_t *addr) const {
        // This accounts for an off by one error caused by the return address of a
        // bailout sitting outside the range of the containing function.
        return method()->raw() <= addr && addr <= method()->raw() + method()->instructionsSize();
    }
    void setInvalidationEpilogueOffset(uint32_t offset) {
        JS_ASSERT(!invalidateEpilogueOffset_);
        invalidateEpilogueOffset_ = offset;
    }
    uint32_t invalidateEpilogueOffset() const {
        JS_ASSERT(invalidateEpilogueOffset_);
        return invalidateEpilogueOffset_;
    }
    void setInvalidationEpilogueDataOffset(uint32_t offset) {
        JS_ASSERT(!invalidateEpilogueDataOffset_);
        invalidateEpilogueDataOffset_ = offset;
    }
    uint32_t invalidateEpilogueDataOffset() const {
        JS_ASSERT(invalidateEpilogueDataOffset_);
        return invalidateEpilogueDataOffset_;
    }
    void incNumBailouts() {
        numBailouts_++;
    }
    uint32_t numBailouts() const {
        return numBailouts_;
    }
    bool bailoutExpected() const {
        return numBailouts_ > 0;
    }
    void setHasUncompiledCallTarget() {
        hasUncompiledCallTarget_ = true;
    }
    void clearHasUncompiledCallTarget() {
        hasUncompiledCallTarget_ = false;
    }
    bool hasUncompiledCallTarget() const {
        return hasUncompiledCallTarget_;
    }
    void setIsParallelEntryScript() {
        isParallelEntryScript_ = true;
    }
    void clearIsParallelEntryScript() {
        isParallelEntryScript_ = false;
    }
    bool isParallelEntryScript() const {
        return isParallelEntryScript_;
    }
    void setHasSPSInstrumentation() {
        hasSPSInstrumentation_ = true;
    }
    void clearHasSPSInstrumentation() {
        hasSPSInstrumentation_ = false;
    }
    bool hasSPSInstrumentation() const {
        return hasSPSInstrumentation_;
    }
    const uint8_t *snapshots() const {
        return reinterpret_cast<const uint8_t *>(this) + snapshots_;
    }
    size_t snapshotsListSize() const {
        return snapshotsListSize_;
    }
    size_t snapshotsRVATableSize() const {
        return snapshotsRVATableSize_;
    }
    const uint8_t *recovers() const {
        return reinterpret_cast<const uint8_t *>(this) + recovers_;
    }
    size_t recoversSize() const {
        return recoversSize_;
    }
    const uint8_t *safepoints() const {
        return reinterpret_cast<const uint8_t *>(this) + safepointsStart_;
    }
    size_t safepointsSize() const {
        return safepointsSize_;
    }
    size_t callTargetEntries() const {
        return callTargetEntries_;
    }
    size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
        return mallocSizeOf(this);
    }
    PreBarrieredValue &getConstant(size_t index) {
        JS_ASSERT(index < numConstants());
        return constants()[index];
    }
    size_t numConstants() const {
        return constantEntries_;
    }
    uint32_t frameSlots() const {
        return frameSlots_;
    }
    uint32_t frameSize() const {
        return frameSize_;
    }
    SnapshotOffset bailoutToSnapshot(uint32_t bailoutId) {
        JS_ASSERT(bailoutId < bailoutEntries_);
        return bailoutTable()[bailoutId];
    }
    const SafepointIndex *getSafepointIndex(uint32_t disp) const;
    const SafepointIndex *getSafepointIndex(uint8_t *retAddr) const {
        JS_ASSERT(containsCodeAddress(retAddr));
        return getSafepointIndex(retAddr - method()->raw());
    }
    const OsiIndex *getOsiIndex(uint32_t disp) const;
    const OsiIndex *getOsiIndex(uint8_t *retAddr) const;
    inline IonCache &getCacheFromIndex(uint32_t index) {
        JS_ASSERT(index < cacheEntries_);
        uint32_t offset = cacheIndex()[index];
        return getCache(offset);
    }
    inline IonCache &getCache(uint32_t offset) {
        JS_ASSERT(offset < runtimeSize_);
        return *(IonCache *) &runtimeData()[offset];
    }
    size_t numCaches() const {
        return cacheEntries_;
    }
    size_t runtimeSize() const {
        return runtimeSize_;
    }
    CacheLocation *getCacheLocs(uint32_t locIndex) {
        JS_ASSERT(locIndex < runtimeSize_);
        return (CacheLocation *) &runtimeData()[locIndex];
    }
    void toggleBarriers(bool enabled);
    void purgeCaches();
    void destroyCaches();
    void unlinkFromRuntime(FreeOp *fop);
    void copySnapshots(const SnapshotWriter *writer);
    void copyRecovers(const RecoverWriter *writer);
    void copyBailoutTable(const SnapshotOffset *table);
    void copyConstants(const Value *vp);
    void copySafepointIndices(const SafepointIndex *firstSafepointIndex, MacroAssembler &masm);
    void copyOsiIndices(const OsiIndex *firstOsiIndex, MacroAssembler &masm);
    void copyRuntimeData(const uint8_t *data);
    void copyCacheEntries(const uint32_t *caches, MacroAssembler &masm);
    void copySafepoints(const SafepointWriter *writer);
    void copyCallTargetEntries(JSScript **callTargets);
    void copyPatchableBackedges(JSContext *cx, JitCode *code,
                                PatchableBackedgeInfo *backedges,
                                MacroAssembler &masm);

    bool invalidated() const {
        return refcount_ != 0;
    }
    size_t refcount() const {
        return refcount_;
    }
    void incref() {
        refcount_++;
    }
    void decref(FreeOp *fop) {
        JS_ASSERT(refcount_);
        refcount_--;
        if (!refcount_)
            Destroy(fop, this);
    }
    const types::RecompileInfo& recompileInfo() const {
        return recompileInfo_;
    }
    types::RecompileInfo& recompileInfoRef() {
        return recompileInfo_;
    }
    OptimizationLevel optimizationLevel() const {
        return optimizationLevel_;
    }
    uint32_t incrOsrPcMismatchCounter() {
        return ++osrPcMismatchCounter_;
    }
    void resetOsrPcMismatchCounter() {
        osrPcMismatchCounter_ = 0;
    }

    void setRecompiling() {
        recompiling_ = true;
    }

    bool isRecompiling() const {
        return recompiling_;
    }

    void clearRecompiling() {
        recompiling_ = false;
    }

    static const uint32_t MAX_PARALLEL_AGE = 5;

    enum ShouldIncreaseAge {
        IncreaseAge = true,
        KeepAge = false
    };

    void resetParallelAge() {
        MOZ_ASSERT(isParallelEntryScript());
        parallelAge_ = 0;
    }
    uint32_t parallelAge() const {
        return parallelAge_;
    }
    uint32_t shouldPreserveParallelCode(ShouldIncreaseAge increaseAge = KeepAge) {
        MOZ_ASSERT(isParallelEntryScript());
        return (increaseAge ? ++parallelAge_ : parallelAge_) < MAX_PARALLEL_AGE;
    }

    static void writeBarrierPre(Zone *zone, IonScript *ionScript);
};

// Execution information for a basic block which may persist after the
// accompanying IonScript is destroyed, for use during profiling.
struct IonBlockCounts
{
  private:
    uint32_t id_;

    // Approximate bytecode in the outer (not inlined) script this block
    // was generated from.
    uint32_t offset_;

    // File and line of the inner script this block was generated from.
    char *description_;

    // ids for successors of this block.
    uint32_t numSuccessors_;
    uint32_t *successors_;

    // Hit count for this block.
    uint64_t hitCount_;

    // Text information about the code generated for this block.
    char *code_;

  public:

    bool init(uint32_t id, uint32_t offset, char *description, uint32_t numSuccessors) {
        id_ = id;
        offset_ = offset;
        description_ = description;
        numSuccessors_ = numSuccessors;
        if (numSuccessors) {
            successors_ = js_pod_calloc<uint32_t>(numSuccessors);
            if (!successors_)
                return false;
        }
        return true;
    }

    void destroy() {
        js_free(description_);
        js_free(successors_);
        js_free(code_);
    }

    uint32_t id() const {
        return id_;
    }

    uint32_t offset() const {
        return offset_;
    }

    const char *description() const {
        return description_;
    }

    size_t numSuccessors() const {
        return numSuccessors_;
    }

    void setSuccessor(size_t i, uint32_t id) {
        JS_ASSERT(i < numSuccessors_);
        successors_[i] = id;
    }

    uint32_t successor(size_t i) const {
        JS_ASSERT(i < numSuccessors_);
        return successors_[i];
    }

    uint64_t *addressOfHitCount() {
        return &hitCount_;
    }

    uint64_t hitCount() const {
        return hitCount_;
    }

    void setCode(const char *code) {
        char *ncode = (char *) js_malloc(strlen(code) + 1);
        if (ncode) {
            strcpy(ncode, code);
            code_ = ncode;
        }
    }

    const char *code() const {
        return code_;
    }
};

// Execution information for a compiled script which may persist after the
// IonScript is destroyed, for use during profiling.
struct IonScriptCounts
{
  private:
    // Any previous invalidated compilation(s) for the script.
    IonScriptCounts *previous_;

    // Information about basic blocks in this script.
    size_t numBlocks_;
    IonBlockCounts *blocks_;

  public:

    IonScriptCounts() {
        mozilla::PodZero(this);
    }

    ~IonScriptCounts() {
        for (size_t i = 0; i < numBlocks_; i++)
            blocks_[i].destroy();
        js_free(blocks_);
        js_delete(previous_);
    }

    bool init(size_t numBlocks) {
        numBlocks_ = numBlocks;
        blocks_ = js_pod_calloc<IonBlockCounts>(numBlocks);
        return blocks_ != nullptr;
    }

    size_t numBlocks() const {
        return numBlocks_;
    }

    IonBlockCounts &block(size_t i) {
        JS_ASSERT(i < numBlocks_);
        return blocks_[i];
    }

    void setPrevious(IonScriptCounts *previous) {
        previous_ = previous;
    }

    IonScriptCounts *previous() const {
        return previous_;
    }
};

struct VMFunction;

class JitCompartment;
class JitRuntime;

struct AutoFlushICache
{
  private:
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS)
    uintptr_t start_;
    uintptr_t stop_;
    const char *name_;
    bool inhibit_;
    AutoFlushICache *prev_;
#endif

  public:
    static void setRange(uintptr_t p, size_t len);
    static void flush(uintptr_t p, size_t len);
    static void setInhibit();
    ~AutoFlushICache();
    explicit AutoFlushICache(const char *nonce, bool inhibit=false);
};

} // namespace jit

namespace gc {

inline bool
IsMarked(const jit::VMFunction *)
{
    // VMFunction are only static objects which are used by WeakMaps as keys.
    // It is considered as a root object which is always marked.
    return true;
}

} // namespace gc

} // namespace js

#endif /* jit_IonCode_h */
