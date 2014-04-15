/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_Ion_h
#define jit_Ion_h

#ifdef JS_ION

#include "mozilla/MemoryReporting.h"

#include "jscntxt.h"
#include "jscompartment.h"

#include "jit/CompileInfo.h"
#include "jit/CompileWrappers.h"
#include "jit/JitOptions.h"

namespace js {
namespace jit {

class TempAllocator;

enum MethodStatus
{
    Method_Error,
    Method_CantCompile,
    Method_Skipped,
    Method_Compiled
};

enum AbortReason {
    AbortReason_Alloc,
    AbortReason_Inlining,
    AbortReason_Disable,
    AbortReason_Error,
    AbortReason_NoAbort
};

// An Ion context is needed to enter into either an Ion method or an instance
// of the Ion compiler. It points to a temporary allocator and the active
// JSContext, either of which may be nullptr, and the active compartment, which
// will not be nullptr.

class IonContext
{
  public:
    IonContext(JSContext *cx, TempAllocator *temp);
    IonContext(ExclusiveContext *cx, TempAllocator *temp);
    IonContext(CompileRuntime *rt, CompileCompartment *comp, TempAllocator *temp);
    IonContext(CompileRuntime *rt);
    ~IonContext();

    // Running context when executing on the main thread. Not available during
    // compilation.
    JSContext *cx;

    // Allocator for temporary memory during compilation.
    TempAllocator *temp;

    // Wrappers with information about the current runtime/compartment for use
    // during compilation.
    CompileRuntime *runtime;
    CompileCompartment *compartment;

    int getNextAssemblerId() {
        return assemblerCount_++;
    }
  private:
    IonContext *prev_;
    int assemblerCount_;
};

// Initialize Ion statically for all JSRuntimes.
bool InitializeIon();

// Get and set the current Ion context.
IonContext *GetIonContext();
IonContext *MaybeGetIonContext();

void SetIonContext(IonContext *ctx);

bool CanIonCompileScript(JSContext *cx, HandleScript script, bool osr);

MethodStatus CanEnterAtBranch(JSContext *cx, JSScript *script,
                              BaselineFrame *frame, jsbytecode *pc, bool isConstructing);
MethodStatus CanEnter(JSContext *cx, RunState &state);
MethodStatus CompileFunctionForBaseline(JSContext *cx, HandleScript script, BaselineFrame *frame,
                                        bool isConstructing);
MethodStatus CanEnterUsingFastInvoke(JSContext *cx, HandleScript script, uint32_t numActualArgs);

MethodStatus CanEnterInParallel(JSContext *cx, HandleScript script);

MethodStatus
Recompile(JSContext *cx, HandleScript script, BaselineFrame *osrFrame, jsbytecode *osrPc,
          bool constructing);

enum IonExecStatus
{
    // The method call had to be aborted due to a stack limit check. This
    // error indicates that Ion never attempted to clean up frames.
    IonExec_Aborted,

    // The method call resulted in an error, and IonMonkey has cleaned up
    // frames.
    IonExec_Error,

    // The method call succeeed and returned a value.
    IonExec_Ok
};

static inline bool
IsErrorStatus(IonExecStatus status)
{
    return status == IonExec_Error || status == IonExec_Aborted;
}

struct EnterJitData;

bool SetEnterJitData(JSContext *cx, EnterJitData &data, RunState &state, AutoValueVector &vals);

IonExecStatus IonCannon(JSContext *cx, RunState &state);

// Used to enter Ion from C++ natives like Array.map. Called from FastInvokeGuard.
IonExecStatus FastInvoke(JSContext *cx, HandleFunction fun, CallArgs &args);

// Walk the stack and invalidate active Ion frames for the invalid scripts.
void Invalidate(types::TypeZone &types, FreeOp *fop,
                const Vector<types::RecompileInfo> &invalid, bool resetUses = true,
                bool cancelOffThread = true);
void Invalidate(JSContext *cx, const Vector<types::RecompileInfo> &invalid, bool resetUses = true,
                bool cancelOffThread = true);
bool Invalidate(JSContext *cx, JSScript *script, ExecutionMode mode, bool resetUses = true,
                bool cancelOffThread = true);
bool Invalidate(JSContext *cx, JSScript *script, bool resetUses = true,
                bool cancelOffThread = true);

void MarkValueFromIon(JSRuntime *rt, Value *vp);
void MarkShapeFromIon(JSRuntime *rt, Shape **shapep);

void ToggleBarriers(JS::Zone *zone, bool needs);

class IonBuilder;
class MIRGenerator;
class LIRGraph;
class CodeGenerator;

bool OptimizeMIR(MIRGenerator *mir);
LIRGraph *GenerateLIR(MIRGenerator *mir);
CodeGenerator *GenerateCode(MIRGenerator *mir, LIRGraph *lir, MacroAssembler *maybeMasm = nullptr);
CodeGenerator *CompileBackEnd(MIRGenerator *mir, MacroAssembler *maybeMasm = nullptr);

void AttachFinishedCompilations(JSContext *cx);
void FinishOffThreadBuilder(IonBuilder *builder);
void StopAllOffThreadCompilations(JSCompartment *comp);

static inline bool
IsIonEnabled(JSContext *cx)
{
    return cx->runtime()->options().ion() &&
           cx->runtime()->options().baseline() &&
           cx->runtime()->jitSupportsFloatingPoint;
}

inline bool
IsIonInlinablePC(jsbytecode *pc) {
    // CALL, FUNCALL, FUNAPPLY, EVAL, NEW (Normal Callsites)
    // GETPROP, CALLPROP, and LENGTH. (Inlined Getters)
    // SETPROP, SETNAME, SETGNAME (Inlined Setters)
    return IsCallPC(pc) || IsGetPropPC(pc) || IsSetPropPC(pc);
}

inline bool
TooManyArguments(unsigned nargs)
{
    return (nargs >= SNAPSHOT_MAX_NARGS || nargs > js_JitOptions.maxStackArgs);
}

void ForbidCompilation(JSContext *cx, JSScript *script);
void ForbidCompilation(JSContext *cx, JSScript *script, ExecutionMode mode);

void PurgeCaches(JSScript *script, JS::Zone *zone);
size_t SizeOfIonData(JSScript *script, mozilla::MallocSizeOf mallocSizeOf);
void DestroyIonScripts(FreeOp *fop, JSScript *script);
void TraceIonScripts(JSTracer* trc, JSScript *script);

void RequestInterruptForIonCode(JSRuntime *rt, JSRuntime::InterruptMode mode);

} // namespace jit
} // namespace js

#endif // JS_ION

#endif /* jit_Ion_h */
