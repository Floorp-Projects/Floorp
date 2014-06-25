/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/AsmJSFrameIterator.h"

#include "jit/AsmJS.h"
#include "jit/AsmJSModule.h"

using namespace js;
using namespace js::jit;

static uint8_t *
ReturnAddressForExitCall(uint8_t **psp)
{
    uint8_t *sp = *psp;
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    // For calls to Ion/C++ on x86/x64, the exitSP is the SP right before the call
    // to C++. Since the call instruction pushes the return address, we know
    // that the return address is 1 word below exitSP.
    return *(uint8_t**)(sp - sizeof(void*));
#elif defined(JS_CODEGEN_ARM)
    // For calls to Ion/C++ on ARM, the *caller* pushes the return address on
    // the stack. For Ion, this is just part of the ABI. For C++, the return
    // address is explicitly pushed before the call since we cannot expect the
    // callee to immediately push lr. This means that exitSP points to the
    // return address.
    return *(uint8_t**)sp;
#elif defined(JS_CODEGEN_MIPS)
    // On MIPS we have two cases: an exit to C++ will store the return address
    // at ShadowStackSpace above sp; an exit to Ion will store the return
    // address at sp. To distinguish the two cases, the low bit of sp (which is
    // aligned and therefore zero) is set for Ion exits.
    if (uintptr_t(sp) & 0x1) {
        sp = *psp -= 0x1;  // Clear the low bit
        return *(uint8_t**)sp;
    }
    return *(uint8_t**)(sp + ShadowStackSpace);
#else
# error "Unknown architecture!"
#endif
}

static uint8_t *
ReturnAddressForJitCall(uint8_t *sp)
{
    // Once inside JIT code, sp always points to the word before the return
    // address.
    return *(uint8_t**)(sp - sizeof(void*));
}

AsmJSFrameIterator::AsmJSFrameIterator(const AsmJSActivation *activation)
  : module_(nullptr)
{
    if (!activation || activation->isInterruptedSP())
        return;

    module_ = &activation->module();
    sp_ = activation->exitSP();

    settle(ReturnAddressForExitCall(&sp_));
}

void
AsmJSFrameIterator::operator++()
{
    settle(ReturnAddressForJitCall(sp_));
}

void
AsmJSFrameIterator::settle(uint8_t *returnAddress)
{
    callsite_ = module_->lookupCallSite(returnAddress);
    if (!callsite_ || callsite_->isEntry()) {
        module_ = nullptr;
        return;
    }

    if (callsite_->isEntry()) {
        module_ = nullptr;
        return;
    }

    sp_ += callsite_->stackDepth();

    if (callsite_->isExit())
        return settle(ReturnAddressForJitCall(sp_));

    JS_ASSERT(callsite_->isNormal());
}

JSAtom *
AsmJSFrameIterator::functionDisplayAtom() const
{
    JS_ASSERT(!done());
    return module_->functionName(callsite_->functionNameIndex());
}

unsigned
AsmJSFrameIterator::computeLine(uint32_t *column) const
{
    JS_ASSERT(!done());
    if (column)
        *column = callsite_->column();
    return callsite_->line();
}

