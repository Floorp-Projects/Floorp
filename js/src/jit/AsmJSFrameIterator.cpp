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
ReturnAddressFromFP(uint8_t *fp)
{
    // In asm.js code, the "frame" consists of a single word: the saved
    // return address of the caller.
    static_assert(AsmJSFrameSize == sizeof(void*), "Frame size mismatch");
    return *(uint8_t**)fp;
}

AsmJSFrameIterator::AsmJSFrameIterator(const AsmJSActivation &activation)
  : module_(&activation.module()),
    fp_(activation.exitFP())
{
    if (!fp_)
        return;
    settle(ReturnAddressFromFP(fp_));
}

void
AsmJSFrameIterator::operator++()
{
    JS_ASSERT(!done());
    fp_ += callsite_->stackDepth();
    settle(ReturnAddressFromFP(fp_));
}

void
AsmJSFrameIterator::settle(uint8_t *returnAddress)
{
    callsite_ = module_->lookupCallSite(returnAddress);
    JS_ASSERT(callsite_);

    if (callsite_->isEntry()) {
        fp_ = nullptr;
        JS_ASSERT(done());
        return;
    }

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

