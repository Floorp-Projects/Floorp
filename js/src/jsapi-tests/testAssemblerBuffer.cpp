/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>

#include "jsatom.h"

#include "jit/shared/IonAssemblerBufferWithConstantPools.h"

#include "jsapi-tests/tests.h"

// Tests for classes in:
//
//   jit/shared/IonAssemblerBuffer.h
//   jit/shared/IonAssemblerBufferWithConstantPools.h
//
// Classes in js::jit tested:
//
//   BufferOffset
//   BufferSlice (implicitly)
//   AssemblerBuffer
//
//   BranchDeadlineSet
//   Pool (implicitly)
//   AssemblerBufferWithConstantPools
//

BEGIN_TEST(testAssemblerBuffer_BufferOffset)
{
    using js::jit::BufferOffset;

    BufferOffset off1;
    BufferOffset off2(10);

    CHECK(!off1.assigned());
    CHECK(off2.assigned());
    CHECK_EQUAL(off2.getOffset(), 10);
    off1 = off2;
    CHECK(off1.assigned());
    CHECK_EQUAL(off1.getOffset(), 10);

    return true;
}
END_TEST(testAssemblerBuffer_BufferOffset)

BEGIN_TEST(testAssemblerBuffer_AssemblerBuffer)
{
    using js::jit::BufferOffset;
    typedef js::jit::AssemblerBuffer<5 * sizeof(uint32_t), uint32_t> AsmBuf;

    AsmBuf ab;
    CHECK(ab.isAligned(16));
    CHECK_EQUAL(ab.size(), 0u);
    CHECK_EQUAL(ab.nextOffset().getOffset(), 0);
    CHECK(!ab.oom());
    CHECK(!ab.bail());

    BufferOffset off1 = ab.putInt(1000017);
    CHECK_EQUAL(off1.getOffset(), 0);
    CHECK_EQUAL(ab.size(), 4u);
    CHECK_EQUAL(ab.nextOffset().getOffset(), 4);
    CHECK(!ab.isAligned(16));
    CHECK(ab.isAligned(4));
    CHECK(ab.isAligned(1));
    CHECK_EQUAL(*ab.getInst(off1), 1000017u);

    BufferOffset off2 = ab.putInt(1000018);
    CHECK_EQUAL(off2.getOffset(), 4);

    BufferOffset off3 = ab.putInt(1000019);
    CHECK_EQUAL(off3.getOffset(), 8);

    BufferOffset off4 = ab.putInt(1000020);
    CHECK_EQUAL(off4.getOffset(), 12);
    CHECK_EQUAL(ab.size(), 16u);
    CHECK_EQUAL(ab.nextOffset().getOffset(), 16);

    // Last one in the slice.
    BufferOffset off5 = ab.putInt(1000021);
    CHECK_EQUAL(off5.getOffset(), 16);
    CHECK_EQUAL(ab.size(), 20u);
    CHECK_EQUAL(ab.nextOffset().getOffset(), 20);

    BufferOffset off6 = ab.putInt(1000022);
    CHECK_EQUAL(off6.getOffset(), 20);
    CHECK_EQUAL(ab.size(), 24u);
    CHECK_EQUAL(ab.nextOffset().getOffset(), 24);

    // Reference previous slice. Excercise the finger.
    CHECK_EQUAL(*ab.getInst(off1), 1000017u);
    CHECK_EQUAL(*ab.getInst(off6), 1000022u);
    CHECK_EQUAL(*ab.getInst(off1), 1000017u);
    CHECK_EQUAL(*ab.getInst(off5), 1000021u);

    // Too much data for one slice.
    const uint32_t fixdata[] = { 2000036, 2000037, 2000038, 2000039, 2000040, 2000041 };

    // Split payload across multiple slices.
    CHECK_EQUAL(ab.nextOffset().getOffset(), 24);
    BufferOffset good1 = ab.putBytesLarge(sizeof(fixdata), fixdata);
    CHECK_EQUAL(good1.getOffset(), 24);
    CHECK_EQUAL(ab.nextOffset().getOffset(), 48);
    CHECK_EQUAL(*ab.getInst(good1), 2000036u);
    CHECK_EQUAL(*ab.getInst(BufferOffset(32)), 2000038u);
    CHECK_EQUAL(*ab.getInst(BufferOffset(36)), 2000039u);
    CHECK_EQUAL(*ab.getInst(BufferOffset(40)), 2000040u);
    CHECK_EQUAL(*ab.getInst(BufferOffset(44)), 2000041u);

    return true;
}
END_TEST(testAssemblerBuffer_AssemblerBuffer)

BEGIN_TEST(testAssemblerBuffer_BranchDeadlineSet)
{
    typedef js::jit::BranchDeadlineSet<3> DLSet;
    using js::jit::BufferOffset;

    js::LifoAlloc alloc(1024);
    DLSet dls(alloc);

    CHECK(dls.empty());
    CHECK(alloc.isEmpty()); // Constructor must be infallible.
    CHECK_EQUAL(dls.size(), 0u);
    CHECK_EQUAL(dls.maxRangeSize(), 0u);

    // Removing non-existant deadline is OK.
    dls.removeDeadline(1, BufferOffset(7));

    // Add deadlines in increasing order as intended. This is optimal.
    dls.addDeadline(1, BufferOffset(10));
    CHECK(!dls.empty());
    CHECK_EQUAL(dls.size(), 1u);
    CHECK_EQUAL(dls.maxRangeSize(), 1u);
    CHECK_EQUAL(dls.earliestDeadline().getOffset(), 10);
    CHECK_EQUAL(dls.earliestDeadlineRange(), 1u);

    // Removing non-existant deadline is OK.
    dls.removeDeadline(1, BufferOffset(7));
    dls.removeDeadline(1, BufferOffset(17));
    dls.removeDeadline(0, BufferOffset(10));
    CHECK_EQUAL(dls.size(), 1u);
    CHECK_EQUAL(dls.maxRangeSize(), 1u);

    // Two identical deadlines for different ranges.
    dls.addDeadline(2, BufferOffset(10));
    CHECK(!dls.empty());
    CHECK_EQUAL(dls.size(), 2u);
    CHECK_EQUAL(dls.maxRangeSize(), 1u);
    CHECK_EQUAL(dls.earliestDeadline().getOffset(), 10);

    // It doesn't matter which range earliestDeadlineRange() reports first,
    // but it must report both.
    if (dls.earliestDeadlineRange() == 1) {
        dls.removeDeadline(1, BufferOffset(10));
        CHECK_EQUAL(dls.earliestDeadline().getOffset(), 10);
        CHECK_EQUAL(dls.earliestDeadlineRange(), 2u);
    } else {
        CHECK_EQUAL(dls.earliestDeadlineRange(), 2u);
        dls.removeDeadline(2, BufferOffset(10));
        CHECK_EQUAL(dls.earliestDeadline().getOffset(), 10);
        CHECK_EQUAL(dls.earliestDeadlineRange(), 1u);
    }

    // Add deadline which is the front of range 0, but not the global earliest.
    dls.addDeadline(0, BufferOffset(20));
    CHECK_EQUAL(dls.earliestDeadline().getOffset(), 10);
    CHECK(dls.earliestDeadlineRange() > 0);

    // Non-optimal add to front of single-entry range 0.
    dls.addDeadline(0, BufferOffset(15));
    CHECK_EQUAL(dls.earliestDeadline().getOffset(), 10);
    CHECK(dls.earliestDeadlineRange() > 0);

    // Append to 2-entry range 0.
    dls.addDeadline(0, BufferOffset(30));
    CHECK_EQUAL(dls.earliestDeadline().getOffset(), 10);
    CHECK(dls.earliestDeadlineRange() > 0);

    // Add penultimate entry.
    dls.addDeadline(0, BufferOffset(25));
    CHECK_EQUAL(dls.earliestDeadline().getOffset(), 10);
    CHECK(dls.earliestDeadlineRange() > 0);

    // Prepend, stealing earliest from other range.
    dls.addDeadline(0, BufferOffset(5));
    CHECK_EQUAL(dls.earliestDeadline().getOffset(), 5);
    CHECK_EQUAL(dls.earliestDeadlineRange(), 0u);

    // Remove central element.
    dls.removeDeadline(0, BufferOffset(20));
    CHECK_EQUAL(dls.earliestDeadline().getOffset(), 5);
    CHECK_EQUAL(dls.earliestDeadlineRange(), 0u);

    // Remove front, giving back the lead.
    dls.removeDeadline(0, BufferOffset(5));
    CHECK_EQUAL(dls.earliestDeadline().getOffset(), 10);
    CHECK(dls.earliestDeadlineRange() > 0);

    // Remove front, giving back earliest to range 0.
    dls.removeDeadline(dls.earliestDeadlineRange(), BufferOffset(10));
    CHECK_EQUAL(dls.earliestDeadline().getOffset(), 15);
    CHECK_EQUAL(dls.earliestDeadlineRange(), 0u);

    // Remove tail.
    dls.removeDeadline(0, BufferOffset(30));
    CHECK_EQUAL(dls.earliestDeadline().getOffset(), 15);
    CHECK_EQUAL(dls.earliestDeadlineRange(), 0u);

    // Now range 0 = [15, 25].
    CHECK_EQUAL(dls.size(), 2u);
    dls.removeDeadline(0, BufferOffset(25));
    dls.removeDeadline(0, BufferOffset(15));
    CHECK(dls.empty());

    return true;
}
END_TEST(testAssemblerBuffer_BranchDeadlineSet)

// Mock Assembler class for testing the AssemblerBufferWithConstantPools
// callbacks.
namespace {

struct TestAssembler;

typedef js::jit::AssemblerBufferWithConstantPools<
  /* SliceSize */ 5 * sizeof(uint32_t),
  /* InstSize */ 4,
  /* Inst */ uint32_t,
  /* Asm */ TestAssembler,
  /* NumShortBranchRanges */ 3> AsmBufWithPool;

struct TestAssembler
{
    // Mock instruction set:
    //
    //   0x1111xxxx - align filler instructions.
    //   0x2222xxxx - manually inserted 'arith' instructions.
    //   0xaaaaxxxx - noop filler instruction.
    //   0xb0bbxxxx - branch xxxx bytes forward. (Pool guard).
    //   0xb1bbxxxx - branch xxxx bytes forward. (Short-range branch).
    //   0xb2bbxxxx - branch xxxx bytes forward. (Veneer branch).
    //   0xb3bbxxxx - branch xxxx bytes forward. (Patched short-range branch).
    //   0xc0ccxxxx - constant pool load (uninitialized).
    //   0xc1ccxxxx - constant pool load to index xxxx.
    //   0xc2ccxxxx - constant pool load xxxx bytes ahead.
    //   0xffffxxxx - pool header with xxxx bytes.

    static const unsigned BranchRange = 36;

    static void InsertIndexIntoTag(uint8_t* load_, uint32_t index)
    {
        uint32_t* load = reinterpret_cast<uint32_t*>(load_);
        MOZ_ASSERT(*load == 0xc0cc0000, "Expected uninitialized constant pool load");
        MOZ_ASSERT(index < 0x10000);
        *load = 0xc1cc0000 + index;
    }

    static void PatchConstantPoolLoad(void* loadAddr, void* constPoolAddr)
    {
        uint32_t* load = reinterpret_cast<uint32_t*>(loadAddr);
        uint32_t index = *load & 0xffff;
        MOZ_ASSERT(*load == (0xc1cc0000 | index), "Expected constant pool load(index)");
        ptrdiff_t offset =
          reinterpret_cast<uint8_t*>(constPoolAddr) - reinterpret_cast<uint8_t*>(loadAddr);
        offset += index * 4;
        MOZ_ASSERT(offset % 4 == 0, "Unaligned constant pool");
        MOZ_ASSERT(offset > 0 && offset < 0x10000, "Pool out of range");
        *load = 0xc2cc0000 + offset;
    }

    static void WritePoolGuard(js::jit::BufferOffset branch, uint32_t* dest,
                               js::jit::BufferOffset afterPool)
    {
        MOZ_ASSERT(branch.assigned());
        MOZ_ASSERT(afterPool.assigned());
        size_t branchOff = branch.getOffset();
        size_t afterPoolOff = afterPool.getOffset();
        MOZ_ASSERT(afterPoolOff > branchOff);
        uint32_t delta = afterPoolOff - branchOff;
        *dest = 0xb0bb0000 + delta;
    }

    static void WritePoolHeader(void* start, js::jit::Pool* p, bool isNatural)
    {
        MOZ_ASSERT(!isNatural, "Natural pool guards not implemented.");
        uint32_t* hdr = reinterpret_cast<uint32_t*>(start);
        *hdr = 0xffff0000 + p->getPoolSize();
    }

    static void PatchShortRangeBranchToVeneer(AsmBufWithPool* buffer, unsigned rangeIdx,
                                              js::jit::BufferOffset deadline,
                                              js::jit::BufferOffset veneer)
    {
        size_t branchOff = deadline.getOffset() - BranchRange;
        size_t veneerOff = veneer.getOffset();
        uint32_t *branch = buffer->getInst(js::jit::BufferOffset(branchOff));

        MOZ_ASSERT((*branch & 0xffff0000) == 0xb1bb0000,
                   "Expected short-range branch instruction");
        // Copy branch offset to veneer. A real instruction set would require
        // some adjustment of the label linked-list.
        *buffer->getInst(veneer) = 0xb2bb0000 | (*branch & 0xffff);
        MOZ_ASSERT(veneerOff > branchOff, "Veneer should follow branch");
        *branch = 0xb3bb0000 + (veneerOff - branchOff);
    }
};
}

BEGIN_TEST(testAssemblerBuffer_AssemblerBufferWithConstantPools)
{
    using js::jit::BufferOffset;

    AsmBufWithPool ab(/* guardSize= */ 1,
                      /* headerSize= */ 1,
                      /* instBufferAlign(unused)= */ 0,
                      /* poolMaxOffset= */ 17,
                      /* pcBias= */ 0,
                      /* alignFillInst= */ 0x11110000,
                      /* nopFillInst= */ 0xaaaa0000,
                      /* nopFill= */ 0);

    CHECK(ab.isAligned(16));
    CHECK_EQUAL(ab.size(), 0u);
    CHECK_EQUAL(ab.nextOffset().getOffset(), 0);
    CHECK(!ab.oom());
    CHECK(!ab.bail());

    // Each slice holds 5 instructions. Trigger a constant pool inside the slice.
    uint32_t poolLoad[] = { 0xc0cc0000 };
    uint32_t poolData[] = { 0xdddd0000, 0xdddd0001, 0xdddd0002, 0xdddd0003 };
    AsmBufWithPool::PoolEntry pe;
    BufferOffset load = ab.allocEntry(1, 1, (uint8_t*)poolLoad, (uint8_t*)poolData, &pe);
    CHECK_EQUAL(pe.index(), 0u);
    CHECK_EQUAL(load.getOffset(), 0);

    // Pool hasn't been emitted yet. Load has been patched by
    // InsertIndexIntoTag.
    CHECK_EQUAL(*ab.getInst(load), 0xc1cc0000);

    // Expected layout:
    //
    //   0: load [pc+16]
    //   4: 0x22220001
    //   8: guard branch pc+12
    //  12: pool header
    //  16: poolData
    //  20: 0x22220002
    //
    ab.putInt(0x22220001);
    // One could argue that the pool should be flushed here since there is no
    // more room. However, the current implementation doesn't dump pool until
    // asked to add data:
    ab.putInt(0x22220002);

    CHECK_EQUAL(*ab.getInst(BufferOffset(0)), 0xc2cc0010u);
    CHECK_EQUAL(*ab.getInst(BufferOffset(4)), 0x22220001u);
    CHECK_EQUAL(*ab.getInst(BufferOffset(8)), 0xb0bb000cu);
    CHECK_EQUAL(*ab.getInst(BufferOffset(12)), 0xffff0004u);
    CHECK_EQUAL(*ab.getInst(BufferOffset(16)), 0xdddd0000u);
    CHECK_EQUAL(*ab.getInst(BufferOffset(20)), 0x22220002u);

    // allocEntry() overwrites the load instruction! Restore the original.
    poolLoad[0] = 0xc0cc0000;

    // Now try with load and pool data on separate slices.
    load = ab.allocEntry(1, 1, (uint8_t*)poolLoad, (uint8_t*)poolData, &pe);
    CHECK_EQUAL(pe.index(), 1u); // Global pool entry index.
    CHECK_EQUAL(load.getOffset(), 24);
    CHECK_EQUAL(*ab.getInst(load), 0xc1cc0000); // Index into current pool.
    ab.putInt(0x22220001);
    ab.putInt(0x22220002);
    CHECK_EQUAL(*ab.getInst(BufferOffset(24)), 0xc2cc0010u);
    CHECK_EQUAL(*ab.getInst(BufferOffset(28)), 0x22220001u);
    CHECK_EQUAL(*ab.getInst(BufferOffset(32)), 0xb0bb000cu);
    CHECK_EQUAL(*ab.getInst(BufferOffset(36)), 0xffff0004u);
    CHECK_EQUAL(*ab.getInst(BufferOffset(40)), 0xdddd0000u);
    CHECK_EQUAL(*ab.getInst(BufferOffset(44)), 0x22220002u);

    // Two adjacent loads to the same pool.
    poolLoad[0] = 0xc0cc0000;
    load = ab.allocEntry(1, 1, (uint8_t*)poolLoad, (uint8_t*)poolData, &pe);
    CHECK_EQUAL(pe.index(), 2u); // Global pool entry index.
    CHECK_EQUAL(load.getOffset(), 48);
    CHECK_EQUAL(*ab.getInst(load), 0xc1cc0000); // Index into current pool.

    poolLoad[0] = 0xc0cc0000;
    load = ab.allocEntry(1, 1, (uint8_t*)poolLoad, (uint8_t*)(poolData + 1), &pe);
    CHECK_EQUAL(pe.index(), 3u); // Global pool entry index.
    CHECK_EQUAL(load.getOffset(), 52);
    CHECK_EQUAL(*ab.getInst(load), 0xc1cc0001); // Index into current pool.

    ab.putInt(0x22220005);

    CHECK_EQUAL(*ab.getInst(BufferOffset(48)), 0xc2cc0010u); // load pc+16.
    CHECK_EQUAL(*ab.getInst(BufferOffset(52)), 0xc2cc0010u); // load pc+16.
    CHECK_EQUAL(*ab.getInst(BufferOffset(56)), 0xb0bb0010u); // guard branch pc+16.
    CHECK_EQUAL(*ab.getInst(BufferOffset(60)), 0xffff0008u); // header 8 bytes.
    CHECK_EQUAL(*ab.getInst(BufferOffset(64)), 0xdddd0000u); // datum 1.
    CHECK_EQUAL(*ab.getInst(BufferOffset(68)), 0xdddd0001u); // datum 2.
    CHECK_EQUAL(*ab.getInst(BufferOffset(72)), 0x22220005u); // putInt(0x22220005)

    // Two loads as above, but the first load has an 8-byte pool entry, and the
    // second load wouldn't be able to reach its data. This must produce two
    // pools.
    poolLoad[0] = 0xc0cc0000;
    load = ab.allocEntry(1, 2, (uint8_t*)poolLoad, (uint8_t*)(poolData+2), &pe);
    CHECK_EQUAL(pe.index(), 4u); // Global pool entry index.
    CHECK_EQUAL(load.getOffset(), 76);
    CHECK_EQUAL(*ab.getInst(load), 0xc1cc0000); // Index into current pool.

    poolLoad[0] = 0xc0cc0000;
    load = ab.allocEntry(1, 1, (uint8_t*)poolLoad, (uint8_t*)poolData, &pe);
    CHECK_EQUAL(pe.index(), 6u); // Global pool entry index. (Prev one is two indexes).
    CHECK_EQUAL(load.getOffset(), 96);
    CHECK_EQUAL(*ab.getInst(load), 0xc1cc0000); // Index into current pool.

    CHECK_EQUAL(*ab.getInst(BufferOffset(76)), 0xc2cc000cu); // load pc+12.
    CHECK_EQUAL(*ab.getInst(BufferOffset(80)), 0xb0bb0010u); // guard branch pc+16.
    CHECK_EQUAL(*ab.getInst(BufferOffset(84)), 0xffff0008u); // header 8 bytes.
    CHECK_EQUAL(*ab.getInst(BufferOffset(88)), 0xdddd0002u); // datum 1.
    CHECK_EQUAL(*ab.getInst(BufferOffset(92)), 0xdddd0003u); // datum 2.

    // Second pool is not flushed yet, and there is room for one instruction
    // after the load. Test the keep-together feature.
    ab.enterNoPool(2);
    ab.putInt(0x22220006);
    ab.putInt(0x22220007);
    ab.leaveNoPool();

    CHECK_EQUAL(*ab.getInst(BufferOffset( 96)), 0xc2cc000cu); // load pc+16.
    CHECK_EQUAL(*ab.getInst(BufferOffset(100)), 0xb0bb000cu); // guard branch pc+12.
    CHECK_EQUAL(*ab.getInst(BufferOffset(104)), 0xffff0004u); // header 4 bytes.
    CHECK_EQUAL(*ab.getInst(BufferOffset(108)), 0xdddd0000u); // datum 1.
    CHECK_EQUAL(*ab.getInst(BufferOffset(112)), 0x22220006u);
    CHECK_EQUAL(*ab.getInst(BufferOffset(116)), 0x22220007u);

    return true;
}
END_TEST(testAssemblerBuffer_AssemblerBufferWithConstantPools)

BEGIN_TEST(testAssemblerBuffer_AssemblerBufferWithConstantPools_ShortBranch)
{
    using js::jit::BufferOffset;

    AsmBufWithPool ab(/* guardSize= */ 1,
                      /* headerSize= */ 1,
                      /* instBufferAlign(unused)= */ 0,
                      /* poolMaxOffset= */ 17,
                      /* pcBias= */ 0,
                      /* alignFillInst= */ 0x11110000,
                      /* nopFillInst= */ 0xaaaa0000,
                      /* nopFill= */ 0);

    // Insert short-range branch.
    BufferOffset br1 = ab.putInt(0xb1bb00cc);
    ab.registerBranchDeadline(1, BufferOffset(br1.getOffset() + TestAssembler::BranchRange));
    ab.putInt(0x22220001);
    BufferOffset off = ab.putInt(0x22220002);
    ab.registerBranchDeadline(1, BufferOffset(off.getOffset() + TestAssembler::BranchRange));
    ab.putInt(0x22220003);
    ab.putInt(0x22220004);

    // Second short-range branch that will be swiped up by hysteresis.
    BufferOffset br2 = ab.putInt(0xb1bb0d2d);
    ab.registerBranchDeadline(1, BufferOffset(br2.getOffset() + TestAssembler::BranchRange));

    // Branch should not have been patched yet here.
    CHECK_EQUAL(*ab.getInst(br1), 0xb1bb00cc);
    CHECK_EQUAL(*ab.getInst(br2), 0xb1bb0d2d);

    // Cancel one of the pending branches.
    // This is what will happen to most branches as they are bound before
    // expiring by Assembler::bind().
    ab.unregisterBranchDeadline(1, BufferOffset(off.getOffset() + TestAssembler::BranchRange));

    off = ab.putInt(0x22220006);
    // Here we may or may not have patched the branch yet, but it is inevitable now:
    //
    //  0: br1 pc+36
    //  4: 0x22220001
    //  8: 0x22220002 (unpatched)
    // 12: 0x22220003
    // 16: 0x22220004
    // 20: br2 pc+20
    // 24: 0x22220006
    CHECK_EQUAL(off.getOffset(), 24);
    // 28: guard branch pc+16
    // 32: pool header
    // 36: veneer1
    // 40: veneer2
    // 44: 0x22220007

    off = ab.putInt(0x22220007);
    CHECK_EQUAL(off.getOffset(), 44);

    // Now the branch must have been patched.
    CHECK_EQUAL(*ab.getInst(br1), 0xb3bb0000 + 36);         // br1 pc+36 (patched)
    CHECK_EQUAL(*ab.getInst(BufferOffset(8)), 0x22220002u);  // 0x22220002 (unpatched)
    CHECK_EQUAL(*ab.getInst(br2), 0xb3bb0000 + 20);         // br2 pc+20 (patched)
    CHECK_EQUAL(*ab.getInst(BufferOffset(28)), 0xb0bb0010u); // br pc+16 (guard)
    CHECK_EQUAL(*ab.getInst(BufferOffset(32)), 0xffff0000u); // pool header 0 bytes.
    CHECK_EQUAL(*ab.getInst(BufferOffset(36)), 0xb2bb00ccu); // veneer1 w/ original 'cc' offset.
    CHECK_EQUAL(*ab.getInst(BufferOffset(40)), 0xb2bb0d2du); // veneer2 w/ original 'd2d' offset.
    CHECK_EQUAL(*ab.getInst(BufferOffset(44)), 0x22220007u);

    return true;
}
END_TEST(testAssemblerBuffer_AssemblerBufferWithConstantPools_ShortBranch)

// Test that everything is put together correctly in the ARM64 assembler.
#if defined(JS_CODEGEN_ARM64)

#include "jit/MacroAssembler-inl.h"

BEGIN_TEST(testAssemblerBuffer_ARM64)
{
    using namespace js::jit;

    js::LifoAlloc lifo(4096);
    TempAllocator alloc(&lifo);
    JitContext jc(cx, &alloc);
    cx->getJitRuntime(cx);
    MacroAssembler masm;

    // Branches to an unbound label.
    Label lab1;
    masm.branch(Assembler::Equal, &lab1);
    masm.branch(Assembler::LessThan, &lab1);
    masm.bind(&lab1);
    masm.branch(Assembler::Equal, &lab1);

    CHECK_EQUAL(masm.getInstructionAt(BufferOffset(0))->InstructionBits(),
                vixl::B_cond | vixl::Assembler::ImmCondBranch(2) | vixl::eq);
    CHECK_EQUAL(masm.getInstructionAt(BufferOffset(4))->InstructionBits(),
                vixl::B_cond | vixl::Assembler::ImmCondBranch(1) | vixl::lt);
    CHECK_EQUAL(masm.getInstructionAt(BufferOffset(8))->InstructionBits(),
                vixl::B_cond | vixl::Assembler::ImmCondBranch(0) | vixl::eq);

    // Branches can reach the label, but the linked list of uses needs to be
    // rearranged. The final conditional branch cannot reach the first branch.
    Label lab2a;
    Label lab2b;
    masm.bind(&lab2a);
    masm.B(&lab2b);
    // Generate 1,100,000 bytes of NOPs.
    for (unsigned n = 0; n < 1100000; n += 4)
        masm.Nop();
    masm.branch(Assembler::LessThan, &lab2b);
    masm.bind(&lab2b);
    CHECK_EQUAL(masm.getInstructionAt(BufferOffset(lab2a.offset()))->InstructionBits(),
                vixl::B | vixl::Assembler::ImmUncondBranch(1100000 / 4 + 2));
    CHECK_EQUAL(masm.getInstructionAt(BufferOffset(lab2b.offset() - 4))->InstructionBits(),
                vixl::B_cond | vixl::Assembler::ImmCondBranch(1) | vixl::lt);

    // Generate a conditional branch that can't reach its label.
    Label lab3a;
    Label lab3b;
    masm.bind(&lab3a);
    masm.branch(Assembler::LessThan, &lab3b);
    for (unsigned n = 0; n < 1100000; n += 4)
        masm.Nop();
    masm.bind(&lab3b);
    masm.B(&lab3a);
    Instruction* bcond3 = masm.getInstructionAt(BufferOffset(lab3a.offset()));
    CHECK_EQUAL(bcond3->BranchType(), vixl::CondBranchType);
    ptrdiff_t delta = bcond3->ImmPCRawOffset() * 4;
    Instruction* veneer = masm.getInstructionAt(BufferOffset(lab3a.offset() + delta));
    CHECK_EQUAL(veneer->BranchType(), vixl::UncondBranchType);
    delta += veneer->ImmPCRawOffset() * 4;
    CHECK_EQUAL(delta, lab3b.offset() - lab3a.offset());
    Instruction* b3 = masm.getInstructionAt(BufferOffset(lab3b.offset()));
    CHECK_EQUAL(b3->BranchType(), vixl::UncondBranchType);
    CHECK_EQUAL(4 * b3->ImmPCRawOffset(), -delta);

    return true;
}
END_TEST(testAssemblerBuffer_ARM64)
#endif /* JS_CODEGEN_ARM64 */
