/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_shared_IonAssemblerBufferWithConstantPools_h
#define jit_shared_IonAssemblerBufferWithConstantPools_h

#include "mozilla/DebugOnly.h"

#include <algorithm>

#include "jit/JitSpewer.h"
#include "jit/shared/IonAssemblerBuffer.h"

// This code extends the AssemblerBuffer to support the pooling of values loaded
// using program-counter relative addressing modes. This is necessary with the
// ARM instruction set because it has a fixed instruction size that can not
// encode all values as immediate arguments in instructions. Pooling the values
// allows the values to be placed in large chunks which minimizes the number of
// forced branches around them in the code. This is used for loading floating
// point constants, for loading 32 bit constants on the ARMv6, for absolute
// branch targets, and in future will be needed for large branches on the ARMv6.
//
// For simplicity of the implementation, the constant pools are always placed
// after the loads referencing them. When a new constant pool load is added to
// the assembler buffer, a corresponding pool entry is added to the current
// pending pool. The finishPool() method copies the current pending pool entries
// into the assembler buffer at the current offset and patches the pending
// constant pool load instructions.
//
// Before inserting instructions or pool entries, it is necessary to determine
// if doing so would place a pending pool entry out of reach of an instruction,
// and if so then the pool must firstly be dumped. With the allocation algorithm
// used below, the recalculation of all the distances between instructions and
// their pool entries can be avoided by noting that there will be a limiting
// instruction and pool entry pair that does not change when inserting more
// instructions. Adding more instructions makes the same increase to the
// distance, between instructions and their pool entries, for all such
// pairs. This pair is recorded as the limiter, and it is updated when new pool
// entries are added, see updateLimiter()
//
// The pools consist of: a guard instruction that branches around the pool, a
// header word that helps identify a pool in the instruction stream, and then
// the pool entries allocated in units of words. The guard instruction could be
// omitted if control does not reach the pool, and this is referred to as a
// natural guard below, but for simplicity the guard branch is always
// emitted. The pool header is an identifiable word that in combination with the
// guard uniquely identifies a pool in the instruction stream. The header also
// encodes the pool size and a flag indicating if the guard is natural. It is
// possible to iterate through the code instructions skipping or examining the
// pools. E.g. it might be necessary to skip pools when search for, or patching,
// an instruction sequence.
//
// It is often required to keep a reference to a pool entry, to patch it after
// the buffer is finished. Each pool entry is assigned a unique index, counting
// up from zero (see the poolEntryCount slot below). These can be mapped back to
// the offset of the pool entry in the finished buffer, see poolEntryOffset().
//
// The code supports no-pool regions, and for these the size of the region, in
// instructions, must be supplied. This size is used to determine if inserting
// the instructions would place a pool entry out of range, and if so then a pool
// is firstly flushed. The DEBUG code checks that the emitted code is within the
// supplied size to detect programming errors. See enterNoPool() and
// leaveNoPool().

// The only planned instruction sets that require inline constant pools are the
// ARM, ARM64, and MIPS, and these all have fixed 32-bit sized instructions so
// for simplicity the code below is specialized for fixed 32-bit sized
// instructions and makes no attempt to support variable length
// instructions. The base assembler buffer which supports variable width
// instruction is used by the x86 and x64 backends.

namespace js {
namespace jit {

typedef Vector<BufferOffset, 512, OldJitAllocPolicy> LoadOffsets;

// The allocation unit size for pools.
typedef int32_t PoolAllocUnit;

struct Pool : public OldJitAllocPolicy
{
  private:
    // The maximum program-counter relative offset below which the instruction
    // set can encode. Different classes of intructions might support different
    // ranges but for simplicity the minimum is used here, and for the ARM this
    // is constrained to 1024 by the float load instructions.
    const size_t maxOffset_;
    // An offset to apply to program-counter relative offsets. The ARM has a
    // bias of 8.
    const unsigned bias_;

    // The number of PoolAllocUnit sized entires used from the poolData vector.
    unsigned numEntries_;
    // The available size of the poolData vector, in PoolAllocUnits.
    unsigned buffSize;
    // The content of the pool entries.  Invariant: If poolData_ is nullptr then
    // buffSize will be zero and oom_ will return true.
    PoolAllocUnit* poolData_;
    // Flag that tracks OOM conditions.
    bool oom_;

    // The limiting instruction and pool-entry pair. The instruction program
    // counter relative offset of this limiting instruction will go out of range
    // first as the pool position moves forward. It is more efficient to track
    // just this limiting pair than to recheck all offsets when testing if the
    // pool needs to be dumped.
    //
    // 1. The actual offset of the limiting instruction referencing the limiting
    // pool entry.
    BufferOffset limitingUser;
    // 2. The pool entry index of the limiting pool entry.
    unsigned limitingUsee;

  public:
    // A record of the code offset of instructions that reference pool
    // entries. These instructions need to be patched when the actual position
    // of the instructions and pools are known, and for the code below this
    // occurs when each pool is finished, see finishPool().
    LoadOffsets loadOffsets;

    explicit Pool(size_t maxOffset, unsigned bias, LifoAlloc& lifoAlloc)
      : maxOffset_(maxOffset),
        bias_(bias),
        numEntries_(0),
        buffSize(8),
        poolData_(lifoAlloc.newArrayUninitialized<PoolAllocUnit>(buffSize)),
        oom_(false),
        limitingUser(),
        limitingUsee(INT_MIN),
        loadOffsets()
    {
        if (poolData_ == nullptr) {
            buffSize = 0;
            oom_ = true;
        }
    }

    static const unsigned Garbage = 0xa5a5a5a5;
    Pool()
      : maxOffset_(Garbage), bias_(Garbage)
    { }

    // If poolData() returns nullptr then oom_ will also be true.
    PoolAllocUnit* poolData() const {
        return poolData_;
    }

    unsigned numEntries() const {
        return numEntries_;
    }

    size_t getPoolSize() const {
        return numEntries_ * sizeof(PoolAllocUnit);
    }

    bool oom() const {
        return oom_;
    }

    // Update the instruction/pool-entry pair that limits the position of the
    // pool. The nextInst is the actual offset of the new instruction being
    // allocated.
    //
    // This is comparing the offsets, see checkFull() below for the equation,
    // but common expressions on both sides have been canceled from the ranges
    // being compared. Notably, the poolOffset cancels out, so the limiting pair
    // does not depend on where the pool is placed.
    void updateLimiter(BufferOffset nextInst) {
        ptrdiff_t oldRange = limitingUsee * sizeof(PoolAllocUnit) - limitingUser.getOffset();
        ptrdiff_t newRange = numEntries_ * sizeof(PoolAllocUnit) - nextInst.getOffset();
        if (!limitingUser.assigned() || newRange > oldRange) {
            // We have a new largest range!
            limitingUser = nextInst;
            limitingUsee = numEntries_;
        }
    }

    // Check if inserting a pool at the actual offset poolOffset would place
    // pool entries out of reach. This is called before inserting instructions
    // to check that doing so would not push pool entries out of reach, and if
    // so then the pool would need to be firstly dumped. The poolOffset is the
    // first word of the pool, after the guard and header and alignment fill.
    bool checkFull(size_t poolOffset) const {
        // Not full if there are no uses.
        if (!limitingUser.assigned())
            return false;
        size_t offset = poolOffset + limitingUsee * sizeof(PoolAllocUnit)
                        - (limitingUser.getOffset() + bias_);
        return offset >= maxOffset_;
    }

    static const unsigned OOM_FAIL = unsigned(-1);

    unsigned insertEntry(unsigned num, uint8_t* data, BufferOffset off, LifoAlloc& lifoAlloc) {
        if (oom_)
            return OOM_FAIL;
        if (numEntries_ + num >= buffSize) {
            // Grow the poolData_ vector.
            unsigned newSize = buffSize*2;
            PoolAllocUnit* tmp = lifoAlloc.newArrayUninitialized<PoolAllocUnit>(newSize);
            if (tmp == nullptr) {
                oom_ = true;
                return OOM_FAIL;
            }
            mozilla::PodCopy(tmp, poolData_, numEntries_);
            poolData_ = tmp;
            buffSize = newSize;
        }
        mozilla::PodCopy(&poolData_[numEntries_], (PoolAllocUnit*)data, num);
        loadOffsets.append(off.getOffset());
        unsigned ret = numEntries_;
        numEntries_ += num;
        return ret;
    }

    bool reset(LifoAlloc& a) {
        numEntries_ = 0;
        buffSize = 8;
        poolData_ = static_cast<PoolAllocUnit*>(a.alloc(buffSize * sizeof(PoolAllocUnit)));
        if (poolData_ == nullptr) {
            oom_ = true;
            buffSize = 0;
            return false;
        }

        new (&loadOffsets) LoadOffsets;

        limitingUser = BufferOffset();
        limitingUsee = -1;
        return true;
    }
};


// The InstSize is the sizeof(Inst) but is needed here because the buffer is
// defined before the Instruction.
template <size_t SliceSize, size_t InstSize, class Inst, class Asm>
struct AssemblerBufferWithConstantPools : public AssemblerBuffer<SliceSize, Inst>
{
  private:
    // The PoolEntry index counter. Each PoolEntry is given a unique index,
    // counting up from zero, and these can be mapped back to the actual pool
    // entry offset after finishing the buffer, see poolEntryOffset().
    size_t poolEntryCount;

  public:
    class PoolEntry
    {
        size_t index_;

      public:
        explicit PoolEntry(size_t index)
          : index_(index)
        { }

        PoolEntry()
          : index_(-1)
        { }

        size_t index() const {
            return index_;
        }
    };

  private:
    typedef AssemblerBuffer<SliceSize, Inst> Parent;
    using typename Parent::Slice;

    // The size of a pool guard, in instructions. A branch around the pool.
    const unsigned guardSize_;
    // The size of the header that is put at the beginning of a full pool, in
    // instruction sized units.
    const unsigned headerSize_;

    // The maximum pc relative offset encoded in instructions that reference
    // pool entries. This is generally set to the maximum offset that can be
    // encoded by the instructions, but for testing can be lowered to affect the
    // pool placement and frequency of pool placement.
    const size_t poolMaxOffset_;

    // The bias on pc relative addressing mode offsets, in units of bytes. The
    // ARM has a bias of 8 bytes.
    const unsigned pcBias_;

    // The current working pool. Copied out as needed before resetting.
    Pool pool_;

    // The buffer should be aligned to this address.
    const size_t instBufferAlign_;

    struct PoolInfo {
        // The index of the first entry in this pool.
        // Pool entries are numbered uniquely across all pools, starting from 0.
        unsigned firstEntryIndex;

        // The location of this pool's first entry in the main assembler buffer.
        // Note that the pool guard and header come before this offset which
        // points directly at the data.
        BufferOffset offset;

        explicit PoolInfo(unsigned index, BufferOffset data)
          : firstEntryIndex(index)
          , offset(data)
        {
        }
    };

    // Info for each pool that has already been dumped. This does not include any entries in pool_.
    Vector<PoolInfo, 8, LifoAllocPolicy<Fallible>> poolInfo_;

    // When true dumping pools is inhibited.
    bool canNotPlacePool_;

#ifdef DEBUG
    // State for validating the 'maxInst' argument to enterNoPool().
    // The buffer offset when entering the no-pool region.
    size_t canNotPlacePoolStartOffset_;
    // The maximum number of word sized instructions declared for the no-pool
    // region.
    size_t canNotPlacePoolMaxInst_;
#endif

    // Instruction to use for alignment fill.
    const uint32_t alignFillInst_;

    // Insert a number of NOP instructions between each requested instruction at
    // all locations at which a pool can potentially spill. This is useful for
    // checking that instruction locations are correctly referenced and/or
    // followed.
    const uint32_t nopFillInst_;
    const unsigned nopFill_;
    // For inhibiting the insertion of fill NOPs in the dynamic context in which
    // they are being inserted.
    bool inhibitNops_;

  public:
    // A unique id within each JitContext, to identify pools in the debug
    // spew. Set by the MacroAssembler, see getNextAssemblerId().
    int id;

  private:
    // The buffer slices are in a double linked list.
    Slice* getHead() const {
        return this->head;
    }
    Slice* getTail() const {
        return this->tail;
    }

  public:
    // Create an assembler buffer.
    // Note that this constructor is not allowed to actually allocate memory from this->lifoAlloc_
    // because the MacroAssembler constructor has not yet created an AutoJitContextAlloc.
    AssemblerBufferWithConstantPools(unsigned guardSize, unsigned headerSize,
                                     size_t instBufferAlign, size_t poolMaxOffset,
                                     unsigned pcBias, uint32_t alignFillInst, uint32_t nopFillInst,
                                     unsigned nopFill = 0)
      : poolEntryCount(0),
        guardSize_(guardSize),
        headerSize_(headerSize),
        poolMaxOffset_(poolMaxOffset),
        pcBias_(pcBias),
        pool_(),
        instBufferAlign_(instBufferAlign),
        poolInfo_(this->lifoAlloc_),  // OK: Vector() doesn't allocate, append() does.
        canNotPlacePool_(false),
#ifdef DEBUG
        canNotPlacePoolStartOffset_(0),
        canNotPlacePoolMaxInst_(0),
#endif
        alignFillInst_(alignFillInst),
        nopFillInst_(nopFillInst),
        nopFill_(nopFill),
        inhibitNops_(false),
        id(-1)
    { }

    // We need to wait until an AutoJitContextAlloc is created by the
    // MacroAssembler before allocating any space.
    void initWithAllocator() {
        // We hand out references to lifoAlloc_ in the constructor.
        // Check that no allocations were made then.
        MOZ_ASSERT(this->lifoAlloc_.isEmpty(), "Illegal LIFO allocations before AutoJitContextAlloc");

        new (&pool_) Pool (poolMaxOffset_, pcBias_, this->lifoAlloc_);
        if (pool_.poolData() == nullptr)
            this->fail_oom();
    }

  private:
    size_t sizeExcludingCurrentPool() const {
        // Return the actual size of the buffer, excluding the current pending
        // pool.
        return this->nextOffset().getOffset();
    }

  public:
    size_t size() const {
        // Return the current actual size of the buffer. This is only accurate
        // if there are no pending pool entries to dump, check.
        MOZ_ASSERT_IF(!this->oom(), pool_.numEntries() == 0);
        return sizeExcludingCurrentPool();
    }

  private:
    void insertNopFill() {
        // Insert fill for testing.
        if (nopFill_ > 0 && !inhibitNops_ && !canNotPlacePool_) {
            inhibitNops_ = true;

            // Fill using a branch-nop rather than a NOP so this can be
            // distinguished and skipped.
            for (size_t i = 0; i < nopFill_; i++)
                putInt(nopFillInst_);

            inhibitNops_ = false;
        }
    }

    static const unsigned OOM_FAIL = unsigned(-1);
    static const unsigned NO_DATA = unsigned(-2);

    unsigned insertEntryForwards(unsigned numInst, unsigned numPoolEntries, uint8_t* inst, uint8_t* data) {
        size_t nextOffset = sizeExcludingCurrentPool();
        size_t poolOffset = nextOffset + (numInst + guardSize_ + headerSize_) * InstSize;

        // Perform the necessary range checks.

        // If inserting pool entries then find a new limiter before we do the
        // range check.
        if (numPoolEntries)
            pool_.updateLimiter(BufferOffset(nextOffset));

        if (pool_.checkFull(poolOffset)) {
            if (numPoolEntries)
                JitSpew(JitSpew_Pools, "[%d] Inserting pool entry caused a spill", id);
            else
                JitSpew(JitSpew_Pools, "[%d] Inserting instruction(%d) caused a spill", id,
                        sizeExcludingCurrentPool());

            finishPool();
            if (this->oom())
                return OOM_FAIL;
            return insertEntryForwards(numInst, numPoolEntries, inst, data);
        }
        if (numPoolEntries) {
            unsigned result = pool_.insertEntry(numPoolEntries, data, this->nextOffset(), this->lifoAlloc_);
            if (result == Pool::OOM_FAIL) {
                this->fail_oom();
                return OOM_FAIL;
            }
            return result;
        }

        // The pool entry index is returned above when allocating an entry, but
        // when not allocating an entry a dummy value is returned - it is not
        // expected to be used by the caller.
        return NO_DATA;
    }

  public:
    // Get the next buffer offset where an instruction would be inserted.
    // This may flush the current constant pool before returning nextOffset().
    BufferOffset nextInstrOffset()
    {
        size_t nextOffset = sizeExcludingCurrentPool();
        // Is there room for a single instruction more?
        size_t poolOffset =
          nextOffset + (1 + guardSize_ + headerSize_) * InstSize;
        if (pool_.checkFull(poolOffset)) {
            JitSpew(JitSpew_Pools,
                    "[%d] nextInstrOffset @ %d caused a constant pool spill",
                    id, nextOffset);
            finishPool();
        }
        return this->nextOffset();
    }

    BufferOffset allocEntry(size_t numInst, unsigned numPoolEntries,
                            uint8_t* inst, uint8_t* data, PoolEntry* pe = nullptr,
                            bool markAsBranch = false)
    {
        // The alloction of pool entries is not supported in a no-pool region,
        // check.
        MOZ_ASSERT_IF(numPoolEntries, !canNotPlacePool_);

        if (this->oom() && !this->bail())
            return BufferOffset();

        insertNopFill();

#ifdef DEBUG
        if (numPoolEntries) {
            JitSpew(JitSpew_Pools, "[%d] Inserting %d entries into pool", id, numPoolEntries);
            JitSpewStart(JitSpew_Pools, "[%d] data is: 0x", id);
            size_t length = numPoolEntries * sizeof(PoolAllocUnit);
            for (unsigned idx = 0; idx < length; idx++) {
                JitSpewCont(JitSpew_Pools, "%02x", data[length - idx - 1]);
                if (((idx & 3) == 3) && (idx + 1 != length))
                    JitSpewCont(JitSpew_Pools, "_");
            }
            JitSpewFin(JitSpew_Pools);
        }
#endif

        // Insert the pool value.
        unsigned index = insertEntryForwards(numInst, numPoolEntries, inst, data);
        if (this->oom())
            return BufferOffset();

        // Now to get an instruction to write.
        PoolEntry retPE;
        if (numPoolEntries) {
            JitSpew(JitSpew_Pools, "[%d] Entry has index %u, offset %u", id, index,
                    sizeExcludingCurrentPool());
            Asm::InsertIndexIntoTag(inst, index);
            // Figure out the offset within the pool entries.
            retPE = PoolEntry(poolEntryCount);
            poolEntryCount += numPoolEntries;
        }
        // Now inst is a valid thing to insert into the instruction stream.
        if (pe != nullptr)
            *pe = retPE;
        return this->putBytes(numInst * InstSize, inst);
    }

    BufferOffset putInt(uint32_t value, bool markAsBranch = false) {
        return allocEntry(1, 0, (uint8_t*)&value, nullptr, nullptr, markAsBranch);
    }

  private:
    void finishPool() {
        JitSpew(JitSpew_Pools, "[%d] Attempting to finish pool %d with %d entries.", id,
                poolInfo_.length(), pool_.numEntries());

        if (pool_.numEntries() == 0) {
            // If there is no data in the pool being dumped, don't dump anything.
            JitSpew(JitSpew_Pools, "[%d] Aborting because the pool is empty", id);
            return;
        }

        // Should not be placing a pool in a no-pool region, check.
        MOZ_ASSERT(!canNotPlacePool_);

        // Dump the pool with a guard branch around the pool.
        BufferOffset guard = this->putBytes(guardSize_ * InstSize, nullptr);
        BufferOffset header = this->putBytes(headerSize_ * InstSize, nullptr);
        BufferOffset data =
          this->putBytesLarge(pool_.getPoolSize(), (const uint8_t*)pool_.poolData());
        if (this->oom())
            return;

        // We only reserved space for the guard branch and pool header.
        // Fill them in.
        BufferOffset afterPool = this->nextOffset();
        Asm::WritePoolGuard(guard, this->getInst(guard), afterPool);
        Asm::WritePoolHeader((uint8_t*)this->getInst(header), &pool_, false);

        // With the pool's final position determined it is now possible to patch
        // the instructions that reference entries in this pool, and this is
        // done incrementally as each pool is finished.
        size_t poolOffset = data.getOffset();

        unsigned idx = 0;
        for (BufferOffset* iter = pool_.loadOffsets.begin();
             iter != pool_.loadOffsets.end();
             ++iter, ++idx)
        {
            // All entries should be before the pool.
            MOZ_ASSERT(iter->getOffset() < guard.getOffset());

            // Everything here is known so we can safely do the necessary
            // substitutions.
            Inst* inst = this->getInst(*iter);
            size_t codeOffset = poolOffset - iter->getOffset();

            // That is, PatchConstantPoolLoad wants to be handed the address of
            // the pool entry that is being loaded.  We need to do a non-trivial
            // amount of math here, since the pool that we've made does not
            // actually reside there in memory.
            JitSpew(JitSpew_Pools, "[%d] Fixing entry %d offset to %u", id, idx, codeOffset);
            Asm::PatchConstantPoolLoad(inst, (uint8_t*)inst + codeOffset);
        }

        // Record the pool info.
        unsigned firstEntry = poolEntryCount - pool_.numEntries();
        if (!poolInfo_.append(PoolInfo(firstEntry, data))) {
            this->fail_oom();
            return;
        }

        // Reset everything to the state that it was in when we started.
        if (!pool_.reset(this->lifoAlloc_)) {
            this->fail_oom();
            return;
        }
    }

  public:
    void flushPool() {
        if (this->oom())
            return;
        JitSpew(JitSpew_Pools, "[%d] Requesting a pool flush", id);
        finishPool();
    }

    void enterNoPool(size_t maxInst) {
        // Don't allow re-entry.
        MOZ_ASSERT(!canNotPlacePool_);
        insertNopFill();

        // Check if the pool will spill by adding maxInst instructions, and if
        // so then finish the pool before entering the no-pool region. It is
        // assumed that no pool entries are allocated in a no-pool region and
        // this is asserted when allocating entries.
        size_t poolOffset = sizeExcludingCurrentPool() + (maxInst + guardSize_ + headerSize_) * InstSize;

        if (pool_.checkFull(poolOffset)) {
            JitSpew(JitSpew_Pools, "[%d] No-Pool instruction(%d) caused a spill.", id,
                    sizeExcludingCurrentPool());
            finishPool();
        }

#ifdef DEBUG
        // Record the buffer position to allow validating maxInst when leaving
        // the region.
        canNotPlacePoolStartOffset_ = this->nextOffset().getOffset();
        canNotPlacePoolMaxInst_ = maxInst;
#endif

        canNotPlacePool_ = true;
    }

    void leaveNoPool() {
        MOZ_ASSERT(canNotPlacePool_);
        canNotPlacePool_ = false;

        // Validate the maxInst argument supplied to enterNoPool().
        MOZ_ASSERT(this->nextOffset().getOffset() - canNotPlacePoolStartOffset_ <= canNotPlacePoolMaxInst_ * InstSize);
    }

    size_t poolSizeBefore(size_t offset) const {
        // Pools are emitted inline, no adjustment required.
        return 0;
    }

    void align(unsigned alignment) {
        MOZ_ASSERT(IsPowerOfTwo(alignment));

        // A pool many need to be dumped at this point, so insert NOP fill here.
        insertNopFill();

        // Check if the code position can be aligned without dumping a pool.
        unsigned requiredFill = sizeExcludingCurrentPool() & (alignment - 1);
        if (requiredFill == 0)
            return;
        requiredFill = alignment - requiredFill;

        // Add an InstSize because it is probably not useful for a pool to be
        // dumped at the aligned code position.
        uint32_t poolOffset = sizeExcludingCurrentPool() + requiredFill
                              + (1 + guardSize_ + headerSize_) * InstSize;
        if (pool_.checkFull(poolOffset)) {
            // Alignment would cause a pool dump, so dump the pool now.
            JitSpew(JitSpew_Pools, "[%d] Alignment of %d at %d caused a spill.", id, alignment,
                    sizeExcludingCurrentPool());
            finishPool();
        }

        inhibitNops_ = true;
        while ((sizeExcludingCurrentPool() & (alignment - 1)) && !this->oom())
            putInt(alignFillInst_);
        inhibitNops_ = false;
    }

  public:
    void executableCopy(uint8_t* dest) {
        if (this->oom())
            return;
        // The pools should have all been flushed, check.
        MOZ_ASSERT(pool_.numEntries() == 0);
        for (Slice* cur = getHead(); cur != nullptr; cur = cur->getNext()) {
            memcpy(dest, &cur->instructions[0], cur->length());
            dest += cur->length();
        }
    }

  public:
    size_t poolEntryOffset(PoolEntry pe) const {
        MOZ_ASSERT(pe.index() < poolEntryCount - pool_.numEntries(),
                   "Invalid pool entry, or not flushed yet.");
        // Find the pool containing pe.index().
        // The array is sorted, so we can use a binary search.
        auto b = poolInfo_.begin(), e = poolInfo_.end();
        // A note on asymmetric types in the upper_bound comparator:
        // http://permalink.gmane.org/gmane.comp.compilers.clang.devel/10101
        auto i = std::upper_bound(b, e, pe.index(), [](size_t value, const PoolInfo& entry) {
            return value < entry.firstEntryIndex;
        });
        // Since upper_bound finds the first pool greater than pe,
        // we want the previous one which is the last one less than or equal.
        MOZ_ASSERT(i != b, "PoolInfo not sorted or empty?");
        --i;
        // The i iterator now points to the pool containing pe.index.
        MOZ_ASSERT(i->firstEntryIndex <= pe.index() &&
                   (i + 1 == e || (i + 1)->firstEntryIndex > pe.index()));
        // Compute the byte offset into the pool.
        unsigned relativeIndex = pe.index() - i->firstEntryIndex;
        return i->offset.getOffset() + relativeIndex * sizeof(PoolAllocUnit);
    }
};

} // namespace ion
} // namespace js

#endif // jit_shared_IonAssemblerBufferWithConstantPools_h
