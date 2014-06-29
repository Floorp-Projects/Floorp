/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_x86_Architecture_x86_h
#define jit_x86_Architecture_x86_h

#include "assembler/assembler/MacroAssembler.h"

namespace js {
namespace jit {

// In bytes: slots needed for potential memory->memory move spills.
//   +8 for cycles
//   +4 for gpr spills
//   +8 for double spills
static const uint32_t ION_FRAME_SLACK_SIZE    = 20;

// Only Win64 requires shadow stack space.
static const uint32_t ShadowStackSpace = 0;

// These offsets are specific to nunboxing, and capture offsets into the
// components of a js::Value.
static const int32_t NUNBOX32_TYPE_OFFSET         = 4;
static const int32_t NUNBOX32_PAYLOAD_OFFSET      = 0;

////
// These offsets are related to bailouts.
////

// Size of each bailout table entry. On x86 this is a 5-byte relative call.
static const uint32_t BAILOUT_TABLE_ENTRY_SIZE    = 5;

class Registers {
  public:
    typedef JSC::X86Registers::RegisterID Code;
    typedef uint8_t SetType;
    static uint32_t SetSize(SetType x) {
        static_assert(sizeof(SetType) == 1, "SetType must be 8 bits");
        return mozilla::CountPopulation32(x);
    }
    static const char *GetName(Code code) {
        static const char * const Names[] = { "eax", "ecx", "edx", "ebx",
                                              "esp", "ebp", "esi", "edi" };
        return Names[code];
    }

    static Code FromName(const char *name) {
        for (size_t i = 0; i < Total; i++) {
            if (strcmp(GetName(Code(i)), name) == 0)
                return Code(i);
        }
        return Invalid;
    }

    static const Code StackPointer = JSC::X86Registers::esp;
    static const Code Invalid = JSC::X86Registers::invalid_reg;

    static const uint32_t Total = 8;
    static const uint32_t TotalPhys = 8;
    static const uint32_t Allocatable = 7;

    static const uint32_t AllMask = (1 << Total) - 1;

    static const uint32_t ArgRegMask = 0;

    static const uint32_t VolatileMask =
        (1 << JSC::X86Registers::eax) |
        (1 << JSC::X86Registers::ecx) |
        (1 << JSC::X86Registers::edx);

    static const uint32_t NonVolatileMask =
        (1 << JSC::X86Registers::ebx) |
        (1 << JSC::X86Registers::esi) |
        (1 << JSC::X86Registers::edi) |
        (1 << JSC::X86Registers::ebp);

    static const uint32_t WrapperMask =
        VolatileMask |
        (1 << JSC::X86Registers::ebx);

    static const uint32_t SingleByteRegs =
        (1 << JSC::X86Registers::eax) |
        (1 << JSC::X86Registers::ecx) |
        (1 << JSC::X86Registers::edx) |
        (1 << JSC::X86Registers::ebx);

    static const uint32_t NonAllocatableMask =
        (1 << JSC::X86Registers::esp);

    static const uint32_t AllocatableMask = AllMask & ~NonAllocatableMask;

    // Registers that can be allocated without being saved, generally.
    static const uint32_t TempMask = VolatileMask & ~NonAllocatableMask;

    // Registers returned from a JS -> JS call.
    static const uint32_t JSCallMask =
        (1 << JSC::X86Registers::ecx) |
        (1 << JSC::X86Registers::edx);

    // Registers returned from a JS -> C call.
    static const uint32_t CallMask =
        (1 << JSC::X86Registers::eax);
};

// Smallest integer type that can hold a register bitmask.
typedef uint8_t PackedRegisterMask;

class FloatRegisters {
  public:
    typedef JSC::X86Registers::XMMRegisterID Code;
    typedef uint32_t SetType;
    static const char *GetName(Code code) {
        static const char * const Names[] = { "xmm0", "xmm1", "xmm2", "xmm3",
                                              "xmm4", "xmm5", "xmm6", "xmm7" };
        return Names[code];
    }

    static Code FromName(const char *name) {
        for (size_t i = 0; i < Total; i++) {
            if (strcmp(GetName(Code(i)), name) == 0)
                return Code(i);
        }
        return Invalid;
    }

    static const Code Invalid = JSC::X86Registers::invalid_xmm;

    static const uint32_t Total = 8;
    static const uint32_t TotalPhys = 8;
    static const uint32_t Allocatable = 7;

    static const uint32_t AllMask = (1 << Total) - 1;
    static const uint32_t AllDoubleMask = AllMask;
    static const uint32_t VolatileMask = AllMask;
    static const uint32_t NonVolatileMask = 0;

    static const uint32_t WrapperMask = VolatileMask;

    static const uint32_t NonAllocatableMask =
        (1 << JSC::X86Registers::xmm7);

    static const uint32_t AllocatableMask = AllMask & ~NonAllocatableMask;
};

template <typename T>
class TypedRegisterSet;

struct FloatRegister {
    typedef FloatRegisters Codes;
    typedef Codes::Code Code;
    typedef Codes::SetType SetType;
    static uint32_t SetSize(SetType x) {
        static_assert(sizeof(SetType) == 4, "SetType must be 32 bits");
        return mozilla::CountPopulation32(x);
    }

    Code code_;

    static FloatRegister FromCode(uint32_t i) {
        JS_ASSERT(i < FloatRegisters::Total);
        FloatRegister r = { (FloatRegisters::Code)i };
        return r;
    }
    Code code() const {
        JS_ASSERT((uint32_t)code_ < FloatRegisters::Total);
        return code_;
    }
    const char *name() const {
        return FloatRegisters::GetName(code());
    }
    bool volatile_() const {
        return !!((1 << code()) & FloatRegisters::VolatileMask);
    }
    bool operator != (FloatRegister other) const {
        return other.code_ != code_;
    }
    bool operator == (FloatRegister other) const {
        return other.code_ == code_;
    }
    bool aliases(FloatRegister other) const {
        return other.code_ == code_;
    }
    uint32_t numAliased() const {
        return 1;
    }
    void aliased(uint32_t aliasIdx, FloatRegister *ret) {
        JS_ASSERT(aliasIdx == 0);
        *ret = *this;
    }
    // This function mostly exists for the ARM backend.  It is to ensure that two
    // floating point registers' types are equivalent.  e.g. S0 is not equivalent
    // to D16, since S0 holds a float32, and D16 holds a Double.
    // Since all floating point registers on x86 and x64 are equivalent, it is
    // reasonable for this function to do the same.
    bool equiv(FloatRegister other) const {
        return true;
    }
    uint32_t size() const {
        return sizeof(double);
    }
    uint32_t numAlignedAliased() const {
        return 1;
    }
    void alignedAliased(uint32_t aliasIdx, FloatRegister *ret) {
        JS_ASSERT(aliasIdx == 0);
        *ret = *this;
    }
    static TypedRegisterSet<FloatRegister> ReduceSetForPush(const TypedRegisterSet<FloatRegister> &s);
    static uint32_t GetSizeInBytes(const TypedRegisterSet<FloatRegister> &s);
    static uint32_t GetPushSizeInBytes(const TypedRegisterSet<FloatRegister> &s);
    uint32_t getRegisterDumpOffsetInBytes();


};

// Arm/D32 has double registers that can NOT be treated as float32
// and this requires some dances in lowering.
inline bool
hasUnaliasedDouble()
{
    return false;
}

// On ARM, Dn aliases both S2n and S2n+1, so if you need to convert a float32
// to a double as a temporary, you need a temporary double register.
inline bool
hasMultiAlias()
{
    return false;
}

} // namespace jit
} // namespace js

#endif /* jit_x86_Architecture_x86_h */
