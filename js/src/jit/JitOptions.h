/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_JitOptions_h
#define jit_JitOptions_h

#include "jit/IonTypes.h"
#include "js/TypeDecls.h"

#ifdef JS_ION

namespace js {
namespace jit {

// Possible register allocators which may be used.
enum IonRegisterAllocator {
    RegisterAllocator_LSRA,
    RegisterAllocator_Backtracking,
    RegisterAllocator_Stupid
};

enum IonGvnKind {
    GVN_Optimistic,
    GVN_Pessimistic
};

struct JitOptions
{
    bool checkGraphConsistency;
#ifdef CHECK_OSIPOINT_REGISTERS
    bool checkOsiPointRegisters;
#endif
    bool checkRangeAnalysis;
    bool checkThreadSafety;
    bool compileTryCatch;
    bool disableGvn;
    bool disableLicm;
    bool disableInlining;
    bool disableEdgeCaseAnalysis;
    bool disableRangeAnalysis;
    bool disableUce;
    bool disableEaa;
    bool eagerCompilation;
    bool forceDefaultIonUsesBeforeCompile;
    uint32_t forcedDefaultIonUsesBeforeCompile;
    bool forceGvnKind;
    IonGvnKind forcedGvnKind;
    bool forceRegisterAllocator;
    IonRegisterAllocator forcedRegisterAllocator;
    bool limitScriptSize;
    bool osr;
    uint32_t baselineUsesBeforeCompile;
    uint32_t exceptionBailoutThreshold;
    uint32_t frequentBailoutThreshold;
    uint32_t maxStackArgs;
    uint32_t osrPcMismatchesBeforeRecompile;
    uint32_t smallFunctionMaxBytecodeLength_;
    uint32_t usesBeforeCompilePar;

    JitOptions();
    bool isSmallFunction(JSScript *script) const;
    void setEagerCompilation();
    void setUsesBeforeCompile(uint32_t useCount);
    void resetUsesBeforeCompile();
};

extern JitOptions js_JitOptions;

} // namespace jit
} // namespace js

#endif // JS_ION

#endif /* jit_JitOptions_h */
