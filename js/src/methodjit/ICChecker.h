/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined jsjaeger_icchecker_h__ && defined JS_METHODJIT
#define jsjaeger_icchecker_h__

#include "assembler/assembler/MacroAssembler.h"

namespace js {
namespace mjit {

#if defined DEBUG && defined JS_CPU_ARM
static inline void
CheckInstMask(void *addr, uint32_t mask, uint32_t expected)
{
    uint32_t inst = *static_cast<uint32_t *>(addr);
    JS_ASSERT((inst & mask) == expected);
}

static inline void
CheckIsLDR(JSC::CodeLocationLabel label, uint8_t rd)
{
    JS_ASSERT((rd & 0xf) == rd);
    CheckInstMask(label.executableAddress(), 0xfc50f000, 0xe4100000 | (rd << 12));
}

static inline void
CheckIsBLX(JSC::CodeLocationLabel label, uint8_t rsrc)
{
    JS_ASSERT((rsrc & 0xf) == rsrc);
    CheckInstMask(label.executableAddress(), 0xfff000ff, 0xe1200030 | rsrc);
}

static inline void
CheckIsStubCall(JSC::CodeLocationLabel label)
{
    CheckIsLDR(label.labelAtOffset(-4), JSC::ARMRegisters::ip);
    CheckIsLDR(label.labelAtOffset(0), JSC::ARMRegisters::r8);
    CheckIsBLX(label.labelAtOffset(4), JSC::ARMRegisters::r8);
}
#else
static inline void CheckIsStubCall(JSC::CodeLocationLabel label) {}
#endif

} /* namespace mjit */
} /* namespace js */

#endif
