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
