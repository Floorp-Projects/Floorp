/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/JitOptions.h"

#include "jsscript.h"

using namespace js;
using namespace js::jit;

namespace js {
namespace jit {

JitOptions js_JitOptions;

JitOptions::JitOptions()
{
    // Whether to perform expensive graph-consistency DEBUG-only assertions.
    // It can be useful to disable this to reduce DEBUG-compile time of large
    // asm.js programs.
    checkGraphConsistency = true;

#ifdef CHECK_OSIPOINT_REGISTERS
    // Emit extra code to verify live regs at the start of a VM call
    // are not modified before its OsiPoint.
    checkOsiPointRegisters = false;
#endif

    // Whether to enable extra code to perform dynamic validation of
    // RangeAnalysis results.
    checkRangeAnalysis = false;

    // Whether Ion should compile try-catch statements.
    compileTryCatch = true;

    // Toggle whether global value numbering is globally disabled.
    disableGvn = false;

    // Toggles whether loop invariant code motion is globally disabled.
    disableLicm = false;

    // Toggles whether inlining is globally disabled.
    disableInlining = false;

    // Toggles whether Edge Case Analysis is gobally disabled.
    disableEdgeCaseAnalysis = false;

    // Toggles whether Range Analysis is globally disabled.
    disableRangeAnalysis = false;

    // Toggles whether Unreachable Code Elimination is globally disabled.
    disableUce = false;

    // Toggles whether Effective Address Analysis is globally disabled.
    disableEaa = false;

    // Whether functions are compiled immediately.
    eagerCompilation = false;

    // Force how many invocation or loop iterations are needed before compiling
    // a function with the highest ionmonkey optimization level.
    // (i.e. OptimizationLevel_Normal)
    forceDefaultIonUsesBeforeCompile = false;
    forcedDefaultIonUsesBeforeCompile = 1000;

    // Force the GVN kind to be optimistic or pessimistic instead of letting
    // the optimization pass decide.
    forceGvnKind = false;
    forcedGvnKind = GVN_Optimistic;

    // Force the used register allocator instead of letting the
    // optimization pass decide.
    forceRegisterAllocator = false;
    forcedRegisterAllocator = RegisterAllocator_LSRA;

    // Toggles whether large scripts are rejected.
    limitScriptSize = true;

    // Toggles whether functions may be entered at loop headers.
    osr = true;

    // How many invocations or loop iterations are needed before functions
    // are compiled with the baseline compiler.
    baselineUsesBeforeCompile = 10;

    // Number of exception bailouts (resuming into catch/finally block) before
    // we invalidate and forbid Ion compilation.
    exceptionBailoutThreshold = 10;

    // Number of bailouts without invalidation before we set
    // JSScript::hadFrequentBailouts and invalidate.
    frequentBailoutThreshold = 10;

    // How many actual arguments are accepted on the C stack.
    maxStackArgs = 4096;

    // How many times we will try to enter a script via OSR before
    // invalidating the script.
    osrPcMismatchesBeforeRecompile = 6000;

    // The bytecode length limit for small function.
    //
    // The default for this was arrived at empirically via benchmarking.
    // We may want to tune it further after other optimizations have gone
    // in.
    smallFunctionMaxBytecodeLength_ = 100;

    // How many uses of a parallel kernel before we attempt compilation.
    usesBeforeCompilePar = 1;

    // Whether to profile inlined functions in Ion or not.
    profileInlineFrames = false;
}

bool
JitOptions::isSmallFunction(JSScript *script) const
{
    return script->length() <= smallFunctionMaxBytecodeLength_;
}

void
JitOptions::setEagerCompilation()
{
    eagerCompilation = true;
    baselineUsesBeforeCompile = 0;
    forceDefaultIonUsesBeforeCompile = true;
    forcedDefaultIonUsesBeforeCompile = 0;
}

void
JitOptions::setUsesBeforeCompile(uint32_t useCount)
{
    forceDefaultIonUsesBeforeCompile = true;
    forcedDefaultIonUsesBeforeCompile = useCount;

    // Undo eager compilation
    if (eagerCompilation && useCount != 0) {
        jit::JitOptions defaultValues;
        eagerCompilation = false;
        baselineUsesBeforeCompile = defaultValues.baselineUsesBeforeCompile;
    }
}

void
JitOptions::resetUsesBeforeCompile()
{
    forceDefaultIonUsesBeforeCompile = false;

    // Undo eager compilation
    if (eagerCompilation) {
        jit::JitOptions defaultValues;
        eagerCompilation = false;
        baselineUsesBeforeCompile = defaultValues.baselineUsesBeforeCompile;
    }
}

} // namespace jit
} // namespace js
