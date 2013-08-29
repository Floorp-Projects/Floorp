/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonLinker_h
#define jit_IonLinker_h

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsgc.h"

#include "assembler/jit/ExecutableAllocator.h"
#include "jit/IonCode.h"
#include "jit/IonCompartment.h"
#include "jit/IonMacroAssembler.h"

namespace js {
namespace jit {

class Linker
{
    MacroAssembler &masm;

    IonCode *fail(JSContext *cx) {
        js_ReportOutOfMemory(cx);
        return NULL;
    }

    IonCode *newCode(JSContext *cx, IonCompartment *comp, JSC::CodeKind kind) {
        JS_ASSERT(kind == JSC::ION_CODE ||
                  kind == JSC::BASELINE_CODE ||
                  kind == JSC::OTHER_CODE);
        gc::AutoSuppressGC suppressGC(cx);
        if (masm.oom())
            return fail(cx);

        JSC::ExecutablePool *pool;
        size_t bytesNeeded = masm.bytesNeeded() + sizeof(IonCode *) + CodeAlignment;
        if (bytesNeeded >= MAX_BUFFER_SIZE)
            return fail(cx);

        uint8_t *result = (uint8_t *)comp->execAlloc()->alloc(bytesNeeded, &pool, kind);
        if (!result)
            return fail(cx);

        // The IonCode pointer will be stored right before the code buffer.
        uint8_t *codeStart = result + sizeof(IonCode *);

        // Bump the code up to a nice alignment.
        codeStart = (uint8_t *)AlignBytes((uintptr_t)codeStart, CodeAlignment);
        uint32_t headerSize = codeStart - result;
        IonCode *code = IonCode::New(cx, codeStart,
                                     bytesNeeded - headerSize, pool);
        if (!code)
            return NULL;
        if (masm.oom())
            return fail(cx);
        code->copyFrom(masm);
        masm.link(code);
#ifdef JSGC_GENERATIONAL
        if (masm.embedsNurseryPointers())
            cx->runtime()->gcStoreBuffer.putWholeCell(code);
#endif
        return code;
    }

  public:
    Linker(MacroAssembler &masm)
      : masm(masm)
    {
        masm.finish();
    }

    IonCode *newCode(JSContext *cx, JSC::CodeKind kind) {
        return newCode(cx, cx->compartment()->ionCompartment(), kind);
    }
};

} // namespace jit
} // namespace js

#endif /* jit_IonLinker_h */
