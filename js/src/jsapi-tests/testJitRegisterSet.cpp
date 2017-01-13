/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/RegisterSets.h"

#include "jsapi-tests/tests.h"

using namespace js;
using namespace js::jit;

static bool
CoPrime(size_t a, size_t b)
{
    if (b <= 1)
        return a == 1 || b == 1;
    return CoPrime(b, a % b);
}

// This macros are use to iterave over all registers in a large number of
// non-looping sequences, which does not rely on the getFirst / getLast
// functions.
#define BEGIN_INDEX_WALK(RegTotal)                          \
    static const size_t Total = RegTotal;                   \
    for (size_t walk = 1; walk < RegTotal; walk += 2) {     \
        if (!CoPrime(RegTotal, walk))                       \
            continue;                                       \
        for (size_t start = 0; start < RegTotal; start++) { \
            size_t index = start;

#define END_INDEX_WALK                                      \
        }                                                   \
    }

#define FOR_ALL_REGISTERS(Register, reg)            \
    do {                                            \
        Register reg = Register::FromCode(index);

#define END_FOR_ALL_REGISTERS                    \
        index = (index + walk) % Total;          \
    } while(index != start)

BEGIN_TEST(testJitRegisterSet_GPR)
{
    BEGIN_INDEX_WALK(Registers::Total)

    LiveGeneralRegisterSet liveRegs;
    AllocatableGeneralRegisterSet pool(GeneralRegisterSet::All());
    CHECK(liveRegs.empty());
    CHECK(pool.set() == GeneralRegisterSet::All());

    FOR_ALL_REGISTERS(Register, reg) {

        CHECK(!pool.has(reg) || !liveRegs.has(reg));
        if (pool.has(reg)) {
            CHECK(!liveRegs.has(reg));
            pool.take(reg);
            liveRegs.add(reg);
            CHECK(liveRegs.has(reg));
            CHECK(!pool.has(reg));
        }
        CHECK(!pool.has(reg) || !liveRegs.has(reg));

    } END_FOR_ALL_REGISTERS;

    CHECK(pool.empty());

    FOR_ALL_REGISTERS(Register, reg) {

        CHECK(!pool.has(reg) || !liveRegs.has(reg));
        if (liveRegs.has(reg)) {
            CHECK(!pool.has(reg));
            liveRegs.take(reg);
            pool.add(reg);
            CHECK(pool.has(reg));
            CHECK(!liveRegs.has(reg));
        }
        CHECK(!pool.has(reg) || !liveRegs.has(reg));

    } END_FOR_ALL_REGISTERS;

    CHECK(liveRegs.empty());
    CHECK(pool.set() == GeneralRegisterSet::All());

    END_INDEX_WALK
    return true;
}
END_TEST(testJitRegisterSet_GPR)

BEGIN_TEST(testJitRegisterSet_FPU)
{
    BEGIN_INDEX_WALK(FloatRegisters::Total)

    LiveFloatRegisterSet liveRegs;
    AllocatableFloatRegisterSet pool(FloatRegisterSet::All());
    CHECK(liveRegs.empty());
    CHECK(pool.set() == FloatRegisterSet::All());

    FOR_ALL_REGISTERS(FloatRegister, reg) {

        CHECK(!pool.has(reg) || !liveRegs.has(reg));
        if (pool.has(reg)) {
            CHECK(!liveRegs.has(reg));
            pool.take(reg);
            liveRegs.add(reg);
            CHECK(liveRegs.has(reg));
            CHECK(!pool.has(reg));
        }
        CHECK(!pool.has(reg) || !liveRegs.has(reg));

    } END_FOR_ALL_REGISTERS;

    CHECK(pool.empty());

    FOR_ALL_REGISTERS(FloatRegister, reg) {

        CHECK(!pool.has(reg) || !liveRegs.has(reg));
        if (liveRegs.has(reg)) {
            CHECK(!pool.has(reg));
            liveRegs.take(reg);
            pool.add(reg);
            CHECK(pool.has(reg));
            CHECK(!liveRegs.has(reg));
        }
        CHECK(!pool.has(reg) || !liveRegs.has(reg));

    } END_FOR_ALL_REGISTERS;

    CHECK(liveRegs.empty());
    CHECK(pool.set() == FloatRegisterSet::All());

    END_INDEX_WALK
    return true;
}
END_TEST(testJitRegisterSet_FPU)
