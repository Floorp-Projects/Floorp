/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jsion_coderef_h__
#define jsion_coderef_h__

#include "jscell.h"
#include "IonTypes.h"

namespace JSC {
    class ExecutablePool;
}

struct JSScript;

namespace js {
namespace ion {

// The maximum size of any buffer associated with an assembler or code object.
// This is chosen to not overflow a signed integer, leaving room for an extra
// bit on offsets.
static const uint32 MAX_BUFFER_SIZE = (1 << 30) - 1;

// Maximum number of scripted arg and stack slots.
static const uint32 SNAPSHOT_MAX_NARGS = 127;
static const uint32 SNAPSHOT_MAX_STACK = 127;

class MacroAssembler;

class IonCode : public gc::Cell
{
  protected:
    uint8 *code_;
    JSC::ExecutablePool *pool_;
    uint32 bufferSize_;             // Total buffer size.
    uint32 insnSize_;               // Instruction stream size.
    uint32 dataSize_;               // Size of the read-only data area.
    uint32 jumpRelocTableBytes_;    // Size of the jump relocation table.
    uint32 dataRelocTableBytes_;    // Size of the data relocation table.
    JSBool invalidated_;            // Whether the code object has been invalidated.
                                    // This is necessary to prevent GC tracing.

    IonCode()
      : code_(NULL),
        pool_(NULL)
    { }
    IonCode(uint8 *code, uint32 bufferSize, JSC::ExecutablePool *pool)
      : code_(code),
        pool_(pool),
        bufferSize_(bufferSize),
        insnSize_(0),
        dataSize_(0),
        jumpRelocTableBytes_(0),
        dataRelocTableBytes_(0),
        invalidated_(false)
    { }

    uint32 dataOffset() const {
        return insnSize_;
    }
    uint32 jumpRelocTableOffset() const {
        return dataOffset() + dataSize_;
    }
    uint32 dataRelocTableOffset() const {
        return jumpRelocTableOffset() + jumpRelocTableBytes_;
    }

  public:
    uint8 *raw() const {
        return code_;
    }
    size_t instructionsSize() const {
        return insnSize_;
    }
    size_t bufferSize() const {
        return bufferSize_;
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

    static IonCode *FromExecutable(uint8 *buffer) {
        IonCode *code = *(IonCode **)(buffer - sizeof(IonCode *));
        JS_ASSERT(code->raw() == buffer);
        return code;
    }

    static size_t OffsetOfCode() {
        return offsetof(IonCode, code_);
    }

    uint8 *jumpRelocTable() {
        return code_ + jumpRelocTableOffset();
    }

    // Allocates a new IonCode object which will be managed by the GC. If no
    // object can be allocated, NULL is returned. On failure, |pool| is
    // automatically released, so the code may be freed.
    static IonCode *New(JSContext *cx, uint8 *code, uint32 bufferSize, JSC::ExecutablePool *pool);

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
    // Code pointer containing the actual method.
    HeapPtr<IonCode> method_;

    // Deoptimization table used by this method.
    HeapPtr<IonCode> deoptTable_;

    // Entrypoint for OSR, or NULL.
    jsbytecode *osrPc_;

    // Offset to OSR entrypoint from method_->raw(), or 0.
    uint32 osrEntryOffset_;

    // Offset of the invalidation epilogue (which pushes this IonScript
    // and calls the invalidation thunk).
    uint32 invalidateEpilogueOffset_;

    // The offset immediately after the IonScript immediate.
    // NOTE: technically a constant delta from
    // |invalidateEpilogueOffset_|, so we could hard-code this
    // per-platform if we want.
    uint32 invalidateEpilogueDataOffset_;

    // Forbid entering into Ion code from a branch.
    // Useful when a bailout is expected.
    bool forbidOsr_;

    // Offset from the start of the code buffer to its snapshot buffer.
    uint32 snapshots_;
    uint32 snapshotsSize_;

    // Table mapping bailout IDs to snapshot offsets.
    uint32 bailoutTable_;
    uint32 bailoutEntries_;

    // Constant table for constants stored in snapshots.
    uint32 constantTable_;
    uint32 constantEntries_;

    // Map code displacement to safepoint / OSI-patch-delta.
    uint32 safepointIndexOffset_;
    uint32 safepointIndexEntries_;

    // Map OSI-point displacement to snapshot.
    uint32 osiIndexOffset_;
    uint32 osiIndexEntries_;

    // Local slot count and frame size. Frame size is the value that can
    // be added to the StackPointer along with the frame prefix to get a
    // valid IonJSFrameLayout.
    uint32 frameLocals_;
    uint32 frameSize_;

    // Offset to and length of the safepoint table in bytes.
    uint32 safepointsStart_;
    uint32 safepointsSize_;

    // State for polymorphic caches in the compiled code.
    uint32 cacheList_;
    uint32 cacheEntries_;

    // Number of references from invalidation records.
    size_t refcount_;

    SnapshotOffset *bailoutTable() {
        return (SnapshotOffset *)(reinterpret_cast<uint8 *>(this) + bailoutTable_);
    }
    HeapValue *constants() {
        return (HeapValue *)(reinterpret_cast<uint8 *>(this) + constantTable_);
    }
    const SafepointIndex *safepointIndices() const {
        return const_cast<IonScript *>(this)->safepointIndices();
    }
    SafepointIndex *safepointIndices() {
        return (SafepointIndex *)(reinterpret_cast<uint8 *>(this) + safepointIndexOffset_);
    }
    const OsiIndex *osiIndices() const {
        return const_cast<IonScript *>(this)->osiIndices();
    }
    OsiIndex *osiIndices() {
        return (OsiIndex *)(reinterpret_cast<uint8 *>(this) + osiIndexOffset_);
    }
    IonCache *cacheList() {
        return (IonCache *)(reinterpret_cast<uint8 *>(this) + cacheList_);
    }

  private:
    void trace(JSTracer *trc);

  public:
    // Do not call directly, use IonScript::New. This is public for cx->new_.
    IonScript();

    static IonScript *New(JSContext *cx, uint32 frameLocals, uint32 frameSize,
                          size_t snapshotsSize, size_t snapshotEntries,
                          size_t constants, size_t safepointIndexEntries, size_t osiIndexEntries,
                          size_t cacheEntries, size_t safepointsSize);
    static void Trace(JSTracer *trc, IonScript *script);
    static void Destroy(FreeOp *fop, IonScript *script);

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
    void setOsrEntryOffset(uint32 offset) {
        JS_ASSERT(!osrEntryOffset_);
        osrEntryOffset_ = offset;
    }
    uint32 osrEntryOffset() const {
        return osrEntryOffset_;
    }
    bool containsCodeAddress(uint8 *addr) const {
        return method()->raw() <= addr && addr <= method()->raw() + method()->instructionsSize();
    }
    bool containsReturnAddress(uint8 *addr) const {
        // This accounts for an off by one error caused by the return address of a
        // bailout sitting outside the range of the containing function.
        return method()->raw() <= addr && addr <= method()->raw() + method()->instructionsSize();
    }
    void setInvalidationEpilogueOffset(uint32 offset) {
        JS_ASSERT(!invalidateEpilogueOffset_);
        invalidateEpilogueOffset_ = offset;
    }
    uint32 invalidateEpilogueOffset() const {
        JS_ASSERT(invalidateEpilogueOffset_);
        return invalidateEpilogueOffset_;
    }
    void setInvalidationEpilogueDataOffset(uint32 offset) {
        JS_ASSERT(!invalidateEpilogueDataOffset_);
        invalidateEpilogueDataOffset_ = offset;
    }
    uint32 invalidateEpilogueDataOffset() const {
        JS_ASSERT(invalidateEpilogueDataOffset_);
        return invalidateEpilogueDataOffset_;
    }
    void forbidOsr() {
        forbidOsr_ = true;
    }
    bool isOsrForbidden() const {
        return forbidOsr_;
    }
    const uint8 *snapshots() const {
        return reinterpret_cast<const uint8 *>(this) + snapshots_;
    }
    size_t snapshotsSize() const {
        return snapshotsSize_;
    }
    const uint8 *safepoints() const {
        return reinterpret_cast<const uint8 *>(this) + safepointsStart_;
    }
    size_t safepointsSize() const {
        return safepointsSize_;
    }
    size_t size() const {
        return safepointsStart_ + safepointsSize_;
    }
    HeapValue &getConstant(size_t index) {
        JS_ASSERT(index < numConstants());
        return constants()[index];
    }
    size_t numConstants() const {
        return constantEntries_;
    }
    uint32 frameLocals() const {
        return frameLocals_;
    }
    uint32 frameSize() const {
        return frameSize_;
    }
    SnapshotOffset bailoutToSnapshot(uint32 bailoutId) {
        JS_ASSERT(bailoutId < bailoutEntries_);
        return bailoutTable()[bailoutId];
    }
    const SafepointIndex *getSafepointIndex(uint32 disp) const;
    const SafepointIndex *getSafepointIndex(uint8 *retAddr) const {
        JS_ASSERT(containsCodeAddress(retAddr));
        return getSafepointIndex(retAddr - method()->raw());
    }
    const OsiIndex *getOsiIndex(uint32 disp) const;
    const OsiIndex *getOsiIndex(uint8 *retAddr) const;
    inline IonCache &getCache(size_t index);
    size_t numCaches() const {
        return cacheEntries_;
    }
    void copySnapshots(const SnapshotWriter *writer);
    void copyBailoutTable(const SnapshotOffset *table);
    void copyConstants(const HeapValue *vp);
    void copySafepointIndices(const SafepointIndex *firstSafepointIndex, MacroAssembler &masm);
    void copyOsiIndices(const OsiIndex *firstOsiIndex, MacroAssembler &masm);
    void copyCacheEntries(const IonCache *caches, MacroAssembler &masm);
    void copySafepoints(const SafepointWriter *writer);

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
};

struct VMFunction;

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

