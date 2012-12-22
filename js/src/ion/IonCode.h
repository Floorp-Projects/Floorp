/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_coderef_h__
#define jsion_coderef_h__

#include "IonTypes.h"
#include "gc/Heap.h"

// For RecompileInfo
#include "jsinfer.h"

namespace JSC {
    class ExecutablePool;
}

class JSScript;

namespace js {
namespace ion {

// The maximum size of any buffer associated with an assembler or code object.
// This is chosen to not overflow a signed integer, leaving room for an extra
// bit on offsets.
static const uint32_t MAX_BUFFER_SIZE = (1 << 30) - 1;

// Maximum number of scripted arg and stack slots.
static const uint32_t SNAPSHOT_MAX_NARGS = 127;
static const uint32_t SNAPSHOT_MAX_STACK = 127;

class MacroAssembler;
class CodeOffsetLabel;

class IonCode : public gc::Cell
{
  protected:
    uint8_t *code_;
    JSC::ExecutablePool *pool_;
    uint32_t bufferSize_;             // Total buffer size.
    uint32_t insnSize_;               // Instruction stream size.
    uint32_t dataSize_;               // Size of the read-only data area.
    uint32_t jumpRelocTableBytes_;    // Size of the jump relocation table.
    uint32_t dataRelocTableBytes_;    // Size of the data relocation table.
    JSBool invalidated_;            // Whether the code object has been invalidated.
                                    // This is necessary to prevent GC tracing.

    IonCode()
      : code_(NULL),
        pool_(NULL)
    { }
    IonCode(uint8_t *code, uint32_t bufferSize, JSC::ExecutablePool *pool)
      : code_(code),
        pool_(pool),
        bufferSize_(bufferSize),
        insnSize_(0),
        dataSize_(0),
        jumpRelocTableBytes_(0),
        dataRelocTableBytes_(0),
        invalidated_(false)
    { }

    uint32_t dataOffset() const {
        return insnSize_;
    }
    uint32_t jumpRelocTableOffset() const {
        return dataOffset() + dataSize_;
    }
    uint32_t dataRelocTableOffset() const {
        return jumpRelocTableOffset() + jumpRelocTableBytes_;
    }

  public:
    uint8_t *raw() const {
        return code_;
    }
    size_t instructionsSize() const {
        return insnSize_;
    }
    void trace(JSTracer *trc);
    void finalize(FreeOp *fop);
    void setInvalidated() {
        invalidated_ = true;
    }

    // If this IonCode object has been, effectively, corrupted due to
    // invalidation patching, then we have to remember this so we don't try and
    // trace relocation entries that may now be corrupt.
    bool invalidated() const {
        return !!invalidated_;
    }

    template <typename T> T as() const {
        return JS_DATA_TO_FUNC_PTR(T, raw());
    }

    void copyFrom(MacroAssembler &masm);

    static IonCode *FromExecutable(uint8_t *buffer) {
        IonCode *code = *(IonCode **)(buffer - sizeof(IonCode *));
        JS_ASSERT(code->raw() == buffer);
        return code;
    }

    static size_t offsetOfCode() {
        return offsetof(IonCode, code_);
    }

    uint8_t *jumpRelocTable() {
        return code_ + jumpRelocTableOffset();
    }

    // Allocates a new IonCode object which will be managed by the GC. If no
    // object can be allocated, NULL is returned. On failure, |pool| is
    // automatically released, so the code may be freed.
    static IonCode *New(JSContext *cx, uint8_t *code, uint32_t bufferSize, JSC::ExecutablePool *pool);

  public:
    static void readBarrier(IonCode *code);
    static void writeBarrierPre(IonCode *code);
    static void writeBarrierPost(IonCode *code, void *addr);
};

class SnapshotWriter;
class SafepointWriter;
class SafepointIndex;
class OsiIndex;
class IonCache;

// An IonScript attaches Ion-generated information to a JSScript.
struct IonScript
{
  private:
    // Code pointer containing the actual method.
    HeapPtr<IonCode> method_;

    // Deoptimization table used by this method.
    HeapPtr<IonCode> deoptTable_;

    // Entrypoint for OSR, or NULL.
    jsbytecode *osrPc_;

    // Offset to OSR entrypoint from method_->raw(), or 0.
    uint32_t osrEntryOffset_;

    // Offset of the invalidation epilogue (which pushes this IonScript
    // and calls the invalidation thunk).
    uint32_t invalidateEpilogueOffset_;

    // The offset immediately after the IonScript immediate.
    // NOTE: technically a constant delta from
    // |invalidateEpilogueOffset_|, so we could hard-code this
    // per-platform if we want.
    uint32_t invalidateEpilogueDataOffset_;

    // Flag set when we bailout, to avoid frequent bailouts.
    bool bailoutExpected_;

    // Offset from the start of the code buffer to its snapshot buffer.
    uint32_t snapshots_;
    uint32_t snapshotsSize_;

    // Table mapping bailout IDs to snapshot offsets.
    uint32_t bailoutTable_;
    uint32_t bailoutEntries_;

    // Constant table for constants stored in snapshots.
    uint32_t constantTable_;
    uint32_t constantEntries_;

    // Map code displacement to safepoint / OSI-patch-delta.
    uint32_t safepointIndexOffset_;
    uint32_t safepointIndexEntries_;

    // Number of STACK_SLOT_SIZE-length slots this function reserves on the
    // stack.
    uint32_t frameSlots_;

    // Frame size is the value that can be added to the StackPointer along
    // with the frame prefix to get a valid IonJSFrameLayout.
    uint32_t frameSize_;

    // Map OSI-point displacement to snapshot.
    uint32_t osiIndexOffset_;
    uint32_t osiIndexEntries_;

    // State for polymorphic caches in the compiled code.
    uint32_t cacheList_;
    uint32_t cacheEntries_;

    // Offset list for patchable pre-barriers.
    uint32_t prebarrierList_;
    uint32_t prebarrierEntries_;

    // Offset to and length of the safepoint table in bytes.
    uint32_t safepointsStart_;
    uint32_t safepointsSize_;

    // List of compiled/inlined JSScript's.
    uint32_t scriptList_;
    uint32_t scriptEntries_;

    // Number of references from invalidation records.
    size_t refcount_;

    types::RecompileInfo recompileInfo_;

  public:
    // Number of times this function has tried to call a non-IM compileable function
    uint32_t slowCallCount;

    SnapshotOffset *bailoutTable() {
        return (SnapshotOffset *)(reinterpret_cast<uint8_t *>(this) + bailoutTable_);
    }
    HeapValue *constants() {
        return (HeapValue *)(reinterpret_cast<uint8_t *>(this) + constantTable_);
    }
    const SafepointIndex *safepointIndices() const {
        return const_cast<IonScript *>(this)->safepointIndices();
    }
    SafepointIndex *safepointIndices() {
        return (SafepointIndex *)(reinterpret_cast<uint8_t *>(this) + safepointIndexOffset_);
    }
    const OsiIndex *osiIndices() const {
        return const_cast<IonScript *>(this)->osiIndices();
    }
    OsiIndex *osiIndices() {
        return (OsiIndex *)(reinterpret_cast<uint8_t *>(this) + osiIndexOffset_);
    }
    IonCache *cacheList() {
        return (IonCache *)(reinterpret_cast<uint8_t *>(this) + cacheList_);
    }
    CodeOffsetLabel *prebarrierList() {
        return (CodeOffsetLabel *)(reinterpret_cast<uint8_t *>(this) + prebarrierList_);
    }
    JSScript **scriptList() const {
        return (JSScript **)(reinterpret_cast<const uint8_t *>(this) + scriptList_);
    }

  private:
    void trace(JSTracer *trc);

  public:
    // Do not call directly, use IonScript::New. This is public for cx->new_.
    IonScript();

    static IonScript *New(JSContext *cx, uint32_t frameLocals, uint32_t frameSize,
                          size_t snapshotsSize, size_t snapshotEntries,
                          size_t constants, size_t safepointIndexEntries, size_t osiIndexEntries,
                          size_t cacheEntries, size_t prebarrierEntries, size_t safepointsSize,
                          size_t scriptEntries);
    static void Trace(JSTracer *trc, IonScript *script);
    static void Destroy(FreeOp *fop, IonScript *script);

    static inline size_t offsetOfMethod() {
        return offsetof(IonScript, method_);
    }
    static inline size_t offsetOfOsrEntryOffset() {
        return offsetof(IonScript, osrEntryOffset_);
    }

  public:
    IonCode *method() const {
        return method_;
    }
    void setMethod(IonCode *code) {
        JS_ASSERT(!invalidated());
        method_ = code;
    }
    void setDeoptTable(IonCode *code) {
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
    void setBailoutExpected() {
        bailoutExpected_ = true;
    }
    bool bailoutExpected() const {
        return bailoutExpected_;
    }
    const uint8_t *snapshots() const {
        return reinterpret_cast<const uint8_t *>(this) + snapshots_;
    }
    size_t snapshotsSize() const {
        return snapshotsSize_;
    }
    const uint8_t *safepoints() const {
        return reinterpret_cast<const uint8_t *>(this) + safepointsStart_;
    }
    size_t safepointsSize() const {
        return safepointsSize_;
    }
    UnrootedScript getScript(size_t i) const {
        JS_ASSERT(i < scriptEntries_);
        return scriptList()[i];
    }
    size_t scriptEntries() const {
        return scriptEntries_;
    }
    size_t sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf) const {
        return mallocSizeOf(this);
    }
    HeapValue &getConstant(size_t index) {
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
    inline IonCache &getCache(size_t index);
    size_t numCaches() const {
        return cacheEntries_;
    }
    inline CodeOffsetLabel &getPrebarrier(size_t index);
    size_t numPrebarriers() const {
        return prebarrierEntries_;
    }
    void toggleBarriers(bool enabled);
    void purgeCaches(JSCompartment *c);
    void copySnapshots(const SnapshotWriter *writer);
    void copyBailoutTable(const SnapshotOffset *table);
    void copyConstants(const HeapValue *vp);
    void copySafepointIndices(const SafepointIndex *firstSafepointIndex, MacroAssembler &masm);
    void copyOsiIndices(const OsiIndex *firstOsiIndex, MacroAssembler &masm);
    void copyCacheEntries(const IonCache *caches, MacroAssembler &masm);
    void copyPrebarrierEntries(const CodeOffsetLabel *barriers, MacroAssembler &masm);
    void copySafepoints(const SafepointWriter *writer);
    void copyScriptEntries(JSScript **scripts);

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

    // ids for successors of this block.
    uint32_t numSuccessors_;
    uint32_t *successors_;

    // Hit count for this block.
    uint64_t hitCount_;

    // Text information about the code generated for this block.
    char *code_;

    // Number of bytes of code generated in this block. Spill code is counted
    // separately from other, instruction implementing code.
    uint32_t instructionBytes_;
    uint32_t spillBytes_;

  public:

    bool init(uint32_t id, uint32_t offset, uint32_t numSuccessors) {
        id_ = id;
        offset_ = offset;
        numSuccessors_ = numSuccessors;
        if (numSuccessors) {
            successors_ = (uint32_t *) js_calloc(numSuccessors * sizeof(uint32_t));
            if (!successors_)
                return false;
        }
        return true;
    }

    void destroy() {
        if (successors_)
            js_free(successors_);
        if (code_)
            js_free(code_);
    }

    uint32_t id() const {
        return id_;
    }

    uint32_t offset() const {
        return offset_;
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

    void setInstructionBytes(uint32_t bytes) {
        instructionBytes_ = bytes;
    }

    uint32_t instructionBytes() const {
        return instructionBytes_;
    }

    void setSpillBytes(uint32_t bytes) {
        spillBytes_ = bytes;
    }

    uint32_t spillBytes() const {
        return spillBytes_;
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
        PodZero(this);
    }

    ~IonScriptCounts() {
        for (size_t i = 0; i < numBlocks_; i++)
            blocks_[i].destroy();
        js_free(blocks_);
        if (previous_)
            js_delete(previous_);
    }

    bool init(size_t numBlocks) {
        numBlocks_ = numBlocks;
        blocks_ = (IonBlockCounts *) js_calloc(numBlocks * sizeof(IonBlockCounts));
        return blocks_ != NULL;
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

class IonCompartment;

struct AutoFlushCache {

  private:
    uintptr_t start_;
    uintptr_t stop_;
    const char *name_;
    IonCompartment *myCompartment_;
    bool used_;

  public:
    void update(uintptr_t p, size_t len);
    static void updateTop(uintptr_t p, size_t len);
    ~AutoFlushCache();
    AutoFlushCache(const char * nonce, IonCompartment *comp = NULL);
    void flushAnyway();
};

// If you are currently in the middle of modifing Ion-compiled code, which
// is going to be flushed at *some* point, but determine that you *must*
// call a function *right* *now*, two things can go wrong:
//   1)  The flusher that you were using is still active, but you are about to
//       enter jitted code, so it needs to be flushed
//   2) the called function can re-enter a compilation/modification path which
//       will use your AFC, and thus not flush when his compilation is done

struct AutoFlushInhibitor {
  private:
    IonCompartment *ic_;
    AutoFlushCache *afc;
  public:
    AutoFlushInhibitor(IonCompartment *ic);
    ~AutoFlushInhibitor();
};
} // namespace ion

namespace gc {

inline bool
IsMarked(const ion::VMFunction *)
{
    // VMFunction are only static objects which are used by WeakMaps as keys.
    // It is considered as a root object which is always marked.
    return true;
}

} // namespace gc

} // namespace js

#endif // jsion_coderef_h__

