/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_architecture_x86_h__
#define jsion_architecture_x86_h__

#include "assembler/assembler/MacroAssembler.h"

namespace js {
namespace ion {
static const ptrdiff_t STACK_SLOT_SIZE       = 4;
static const uint32_t DOUBLE_STACK_ALIGNMENT   = 2;

// In bytes: slots needed for potential memory->memory move spills.
//   +8 for cycles
//   +4 for gpr spills
//   +8 for double spills
static const uint32_t ION_FRAME_SLACK_SIZE    = 20;

// Only Win64 requires shadow stack space.
static const uint32_t ShadowStackSpace = 0;

// An offset that is illegal for a local variable's stack allocation.
static const int32_t INVALID_STACK_SLOT       = -1;

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

    static const char *GetName(Code code) {
        static const char *Names[] = { "eax", "ecx", "edx", "ebx",
                                       "esp", "ebp", "esi", "edi" };
        return Names[code];
    }

    static const Code StackPointer = JSC::X86Registers::esp;
    static const Code Invalid = JSC::X86Registers::invalid_reg;

    static const uint32_t Total = 8;
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

    typedef JSC::MacroAssembler::RegisterID RegisterID;
};

// Smallest integer type that can hold a register bitmask.
typedef uint8_t PackedRegisterMask;

class FloatRegisters {
  public:
    typedef JSC::X86Registers::XMMRegisterID Code;

    static const char *GetName(Code code) {
        static const char *Names[] = { "xmm0", "xmm1", "xmm2", "xmm3",
                                       "xmm4", "xmm5", "xmm6", "xmm7" };
        return Names[code];
    }

    static const Code Invalid = JSC::X86Registers::invalid_xmm;

    static const uint32_t Total = 8;
    static const uint32_t Allocatable = 7;

    static const uint32_t AllMask = (1 << Total) - 1;

    static const uint32_t VolatileMask = AllMask;
    static const uint32_t NonVolatileMask = 0;

    static const uint32_t WrapperMask = VolatileMask;

    static const uint32_t NonAllocatableMask =
        (1 << JSC::X86Registers::xmm7);

    static const uint32_t AllocatableMask = AllMask & ~NonAllocatableMask;
};

} // namespace ion
} // namespace js

#endif // jsion_architecture_x86_h__

