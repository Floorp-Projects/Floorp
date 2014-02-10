/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_mips_Architecture_mips_h
#define jit_mips_Architecture_mips_h

#include <limits.h>
#include <stdint.h>

#include "js/Utility.h"

// gcc appears to use _mips_hard_float to denote
// that the target is a hard-float target.
#ifdef _mips_hard_float
#define JS_CODEGEN_MIPS_HARDFP
#endif
namespace js {
namespace jit {

// Shadow stack space is not required on MIPS.
static const uint32_t ShadowStackSpace = 0;

// These offsets are specific to nunboxing, and capture offsets into the
// components of a js::Value.
// Size of MIPS32 general purpose registers is 32 bits.
static const int32_t NUNBOX32_TYPE_OFFSET = 4;
static const int32_t NUNBOX32_PAYLOAD_OFFSET = 0;

// Size of each bailout table entry.
// For MIPS this is 2 instructions relative call.
static const uint32_t BAILOUT_TABLE_ENTRY_SIZE = 2 * sizeof(void *);

class Registers
{
  public:
    enum RegisterID {
        r0 = 0,
        r1,
        r2,
        r3,
        r4,
        r5,
        r6,
        r7,
        r8,
        r9,
        r10,
        r11,
        r12,
        r13,
        r14,
        r15,
        r16,
        r17,
        r18,
        r19,
        r20,
        r21,
        r22,
        r23,
        r24,
        r25,
        r26,
        r27,
        r28,
        r29,
        r30,
        r31,
        zero = r0,
        at = r1,
        v0 = r2,
        v1 = r3,
        a0 = r4,
        a1 = r5,
        a2 = r6,
        a3 = r7,
        t0 = r8,
        t1 = r9,
        t2 = r10,
        t3 = r11,
        t4 = r12,
        t5 = r13,
        t6 = r14,
        t7 = r15,
        s0 = r16,
        s1 = r17,
        s2 = r18,
        s3 = r19,
        s4 = r20,
        s5 = r21,
        s6 = r22,
        s7 = r23,
        t8 = r24,
        t9 = r25,
        k0 = r26,
        k1 = r27,
        gp = r28,
        sp = r29,
        fp = r30,
        ra = r31,
        invalid_reg
    };
    typedef RegisterID Code;

    static const char *GetName(Code code) {
        static const char * const Names[] = { "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
                                              "t0",   "t1", "t2", "t3", "t4", "t5", "t6", "t7",
                                              "s0",   "s1", "s2", "s3", "s4", "s5", "s6", "s7",
                                              "t8",   "t9", "k0", "k1", "gp", "sp", "fp", "ra"};
        return Names[code];
    }
    static const char *GetName(uint32_t i) {
        MOZ_ASSERT(i < Total);
        return GetName(Code(i));
    }

    static Code FromName(const char *name);

    static const Code StackPointer = sp;
    static const Code Invalid = invalid_reg;

    static const uint32_t Total = 32;
    static const uint32_t Allocatable = 14;

    static const uint32_t AllMask = 0xffffffff;
    static const uint32_t ArgRegMask = (1 << a0) | (1 << a1) | (1 << a2) | (1 << a3);

    static const uint32_t VolatileMask =
        (1 << Registers::v0) |
        (1 << Registers::v1) |
        (1 << Registers::a0) |
        (1 << Registers::a1) |
        (1 << Registers::a2) |
        (1 << Registers::a3) |
        (1 << Registers::t0) |
        (1 << Registers::t1) |
        (1 << Registers::t2) |
        (1 << Registers::t3) |
        (1 << Registers::t4) |
        (1 << Registers::t5) |
        (1 << Registers::t6) |
        (1 << Registers::t7);

    static const uint32_t NonVolatileMask =
        (1 << Registers::s0) |
        (1 << Registers::s1) |
        (1 << Registers::s2) |
        (1 << Registers::s3) |
        (1 << Registers::s4) |
        (1 << Registers::s5) |
        (1 << Registers::s6) |
        (1 << Registers::s7);

    static const uint32_t WrapperMask =
        VolatileMask |         // = arguments
        (1 << Registers::t0) | // = outReg
        (1 << Registers::t1);  // = argBase

    static const uint32_t NonAllocatableMask =
        (1 << Registers::zero) |
        (1 << Registers::at) | // at = scratch
        (1 << Registers::t8) | // t8 = scratch
        (1 << Registers::t9) | // t9 = scratch
        (1 << Registers::k0) |
        (1 << Registers::k1) |
        (1 << Registers::gp) |
        (1 << Registers::sp) |
        (1 << Registers::fp) |
        (1 << Registers::ra);

    // Registers that can be allocated without being saved, generally.
    static const uint32_t TempMask = VolatileMask & ~NonAllocatableMask;

    // Registers returned from a JS -> JS call.
    static const uint32_t JSCallMask =
        (1 << Registers::v0) |
        (1 << Registers::v1);

    // Registers returned from a JS -> C call.
    static const uint32_t CallMask =
        (1 << Registers::v0) |
        (1 << Registers::v1);  // used for double-size returns

    static const uint32_t AllocatableMask = AllMask & ~NonAllocatableMask;
};

// Smallest integer type that can hold a register bitmask.
typedef uint32_t PackedRegisterMask;


// MIPS32 uses pairs of even and odd float registers as double precision
// registers. Example: f0 (double) is composed of f0 and f1 (single).
// This port only uses even registers to avoid allocation problems.
class FloatRegisters
{
  public:
    enum FPRegisterID {
        f0 = 0, // f0, f2 - Return values
        f2,
        f4, // f4 - f10, f16, f18 - Temporaries
        f6,
        f8,
        f10,
        f12, // f12, f14 - Arguments
        f14,
        f16,
        f18,
        f20, // f20 - f30 - Saved registers
        f22,
        f24,
        f26,
        f28,
        f30,
        invalid_freg
    };
    typedef FPRegisterID Code;

    static const char *GetName(Code code) {
        static const char * const Names[] = { "f0",  "f2",  "f4",  "f6",
                                              "f8",  "f10", "f12", "f14",
                                              "f16", "f18", "f20", "f22",
                                              "f24", "f26", "f28", "f30"};
        return Names[code];
    }
    static const char *GetName(uint32_t i) {
        JS_ASSERT(i < Total);
        return GetName(Code(i));
    }

    static Code FromName(const char *name);

    static const Code Invalid = invalid_freg;

    static const uint32_t Total = 16;
    static const uint32_t Allocatable = 14;

    static const uint32_t AllMask = 0xffff;

    static const uint32_t VolatileMask =
        (1 << FloatRegisters::f0) |
        (1 << FloatRegisters::f2) |
        (1 << FloatRegisters::f4) |
        (1 << FloatRegisters::f6) |
        (1 << FloatRegisters::f8) |
        (1 << FloatRegisters::f10) |
        (1 << FloatRegisters::f12) |
        (1 << FloatRegisters::f14) |
        (1 << FloatRegisters::f16) |
        (1 << FloatRegisters::f18);
    static const uint32_t NonVolatileMask =
        (1 << FloatRegisters::f20) |
        (1 << FloatRegisters::f22) |
        (1 << FloatRegisters::f24) |
        (1 << FloatRegisters::f26) |
        (1 << FloatRegisters::f28) |
        (1 << FloatRegisters::f30);

    static const uint32_t WrapperMask = VolatileMask;

    // f18 and f16 are MIPS scratch float registers.
    static const uint32_t NonAllocatableMask =
        (1 << f16) |
        (1 << f18);

    // Registers that can be allocated without being saved, generally.
    static const uint32_t TempMask = VolatileMask & ~NonAllocatableMask;

    static const uint32_t AllocatableMask = AllMask & ~NonAllocatableMask;
};

uint32_t GetMIPSFlags();
bool hasFPU();

} // namespace jit
} // namespace js

#endif /* jit_mips_Architecture_mips_h */
