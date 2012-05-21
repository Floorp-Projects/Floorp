/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined trampolines_h__ && defined JS_METHODJIT
#define trampolines_h__

#include "assembler/jit/ExecutableAllocator.h"
#include "methodjit/CodeGenIncludes.h"

namespace js {
namespace mjit {

class TrampolineCompiler
{
    typedef bool (*TrampolineGenerator)(Assembler &masm);

public:
    TrampolineCompiler(JSC::ExecutableAllocator *alloc, Trampolines *tramps)
      : execAlloc(alloc), trampolines(tramps)
    { }

    bool compile();
    static void release(Trampolines *tramps);

private:
    bool compileTrampoline(Trampolines::TrampolinePtr *where, JSC::ExecutablePool **pool,
                           TrampolineGenerator generator);

    /* Generators for trampolines. */
    static bool generateForceReturn(Assembler &masm);

#if (defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)) || defined(_WIN64)
    static bool generateForceReturnFast(Assembler &masm);
#endif

    JSC::ExecutableAllocator *execAlloc;
    Trampolines *trampolines;
};

} /* namespace mjit */
} /* namespace js */

#endif

