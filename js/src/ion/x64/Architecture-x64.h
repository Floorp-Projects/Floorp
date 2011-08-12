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

#ifndef jsion_architecture_x64_h__
#define jsion_architecture_x64_h__

#include "assembler/assembler/X86Assembler.h"

namespace js {
namespace ion {

static const ptrdiff_t STACK_SLOT_SIZE       = 8;
static const uint32 MAX_STACK_SLOTS          = 256;

// In bytes: slots needed for potential memory->memory move spills.
//   +8 for cycles
//   +8 for gpr spills
//   +8 for double spills
static const uint32 ION_FRAME_SLACK_SIZE     = 24;

#ifdef _WIN64
static const uint32 ShadowStackSpace = 32;
#else
static const uint32 ShadowStackSpace = 0;
#endif

// An offset that is illegal for a local variable's stack allocation.
static const int32 INVALID_STACK_SLOT       = -1;

class Registers {
  public:
    typedef JSC::X86Registers::RegisterID Code;

    static const char *GetName(Code code) {
        static const char *Names[] = { "rax", "rcx", "rdx", "rbx",
                                       "rsp", "rbp", "rsi", "rdi",
                                       "r8",  "r9",  "r10", "r11",
                                       "r12", "r13", "r14", "r15" };
        return Names[code];
    }

    static const Code StackPointer = JSC::X86Registers::esp;
    static const Code Invalid = JSC::X86Registers::invalid_reg;

    static const uint32 Total = 16;
    static const uint32 Allocatable = 14;

    static const uint32 AllMask = (1 << Total) - 1;

    static const uint32 VolatileMask =
        (1 << JSC::X86Registers::eax) |
        (1 << JSC::X86Registers::ecx) |
        (1 << JSC::X86Registers::edx) |
# if !defined(_WIN64)
        (1 << JSC::X86Registers::esi) |
        (1 << JSC::X86Registers::edi) |
# endif
        (1 << JSC::X86Registers::r8) |
        (1 << JSC::X86Registers::r9) |
        (1 << JSC::X86Registers::r10) |
        (1 << JSC::X86Registers::r11);

    static const uint32 NonVolatileMask =
        (1 << JSC::X86Registers::ebx) |
#if defined(_WIN64)
        (1 << JSC::X86Registers::esi) |
        (1 << JSC::X86Registers::edi) |
#endif
        (1 << JSC::X86Registers::ebp) |
        (1 << JSC::X86Registers::r12) |
        (1 << JSC::X86Registers::r13) |
        (1 << JSC::X86Registers::r14) |
        (1 << JSC::X86Registers::r15);

    static const uint32 SingleByteRegs = VolatileMask | NonVolatileMask;

    static const uint32 NonAllocatableMask =
        (1 << JSC::X86Registers::esp) |
        (1 << JSC::X86Registers::r11);      // This is ScratchReg.

    static const uint32 AllocatableMask = AllMask & ~NonAllocatableMask;

    static const uint32 JSCallClobberMask =
        AllocatableMask & ~(1 << JSC::X86Registers::ecx);
};

class FloatRegisters {
  public:
    typedef JSC::X86Registers::XMMRegisterID Code;

    static const char *GetName(Code code) {
        static const char *Names[] = { "xmm0",  "xmm1",  "xmm2",  "xmm3",
                                       "xmm4",  "xmm5",  "xmm6",  "xmm7",
                                       "xmm8",  "xmm9",  "xmm10", "xmm11",
                                       "xmm12", "xmm13", "xmm14", "xmm15" };
        return Names[code];
    }

    static const Code Invalid = JSC::X86Registers::invalid_xmm;

    static const uint32 Total = 16;
    static const uint32 Allocatable = 15;

    static const uint32 AllMask = (1 << Total) - 1;

    static const uint32 VolatileMask = 
#if defined(_WIN64)
        (1 << JSC::X86Registers::xmm0) |
        (1 << JSC::X86Registers::xmm1) |
        (1 << JSC::X86Registers::xmm2) |
        (1 << JSC::X86Registers::xmm3) |
        (1 << JSC::X86Registers::xmm4) |
        (1 << JSC::X86Registers::xmm5);
#else
        AllMask;
#endif


    static const uint32 NonVolatileMask = AllMask & ~VolatileMask;

    static const uint32 NonAllocatableMask =
        (1 << JSC::X86Registers::xmm15);    // This is ScratchFloatReg.

    static const uint32 AllocatableMask = AllMask & ~NonAllocatableMask;

    static const uint32 JSCallClobberMask = AllocatableMask;
};

} // namespace ion
} // namespace js

#endif // jsion_architecture_x64_h__

