/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(jsion_ion_h__) && defined(JS_ION)
#define jsion_ion_h__

#include "jscntxt.h"
#include "jscompartment.h"
#include "IonCode.h"
#include "jsinfer.h"
#include "jsinterp.h"

namespace js {
namespace ion {

class TempAllocator;

// Possible register allocators which may be used.
enum IonRegisterAllocator {
    RegisterAllocator_LSRA,
    RegisterAllocator_Stupid
};

struct IonOptions
{
    // Toggles whether global value numbering is used.
    //
    // Default: true
    bool gvn;

    // Toggles whether global value numbering is optimistic (true) or
    // pessimistic (false).
    //
    // Default: true
    bool gvnIsOptimistic;

    // Toggles whether loop invariant code motion is performed.
    //
    // Default: true
    bool licm;

    // Toggles whether functions may be entered at loop headers.
    //
    // Default: true
    bool osr;

    // Toggles whether large scripts are rejected.
    //
    // Default: true
    bool limitScriptSize;

    // Describes which register allocator to use.
    //
    // Default: LSRA
    IonRegisterAllocator registerAllocator;

    // Toggles whether inlining is performed.
    //
    // Default: true
    bool inlining;

    // Toggles whether Edge Case Analysis is used.
    //
    // Default: true
    bool edgeCaseAnalysis;

    // Toggles whether Range Analysis is used.
    //
    // Default: true
    bool rangeAnalysis;

    // Toggles whether Unreachable Code Elimination is performed.
    //
    // Default: true
    bool uce;

    // Toggles whether compilation occurs off the main thread.
    //
    // Default: true iff there are at least two CPUs available
    bool parallelCompilation;

    // How many invocations or loop iterations are needed before functions
    // are compiled.
    //
    // Default: 10,240
    uint32_t usesBeforeCompile;

    // How many invocations or loop iterations are needed before functions
    // are compiled when JM is disabled.
    //
    // Default: 40
    uint32_t usesBeforeCompileNoJaeger;

    // How many invocations or loop iterations are needed before calls
    // are inlined.
    //
    // Default: 10,240
    uint32_t usesBeforeInlining;

    // How many actual arguments are accepted on the C stack.
    //
    // Default: 4,096
    uint32_t maxStackArgs;

    // The maximum inlining depth.
    //
    // Default: 3
    uint32_t maxInlineDepth;

    // The bytecode length limit for small function.
    //
    // The default for this was arrived at empirically via benchmarking.
    // We may want to tune it further after other optimizations have gone
    // in.
    //
    // Default: 100
    uint32_t smallFunctionMaxBytecodeLength;

    // The inlining limit for small functions.
    //
    // This value has been arrived at empirically via benchmarking.
    // We may want to revisit this tuning after other optimizations have
    // gone in.
    //
    // Default: usesBeforeInlining / 4
    uint32_t smallFunctionUsesBeforeInlining;

    // The maximum number of functions to polymorphically inline at a call site.
    //
    // Default: 4
    uint32_t polyInlineMax;

    // The maximum total bytecode size of an inline call site.
    //
    // Default: 800
    uint32_t inlineMaxTotalBytecodeLength;

    // Minimal ratio between the use counts of the caller and the callee to
    // enable inlining of functions.
    //
    // Default: 128
    uint32_t inlineUseCountRatio;

    // Whether functions are compiled immediately.
    //
    // Default: false
    bool eagerCompilation;

    // If a function has attempted to make this many calls to
    // functions that are marked "uncompileable", then
    // stop running this function in IonMonkey. (default 512)
    uint32_t slowCallLimit;

    // When caller runs in IM, but callee not, we take a slow path to the interpreter.
    // This has a significant overhead. In order to decrease the number of times this happens,
    // the useCount gets incremented faster to compile this function in IM and use the fastpath.
    //
    // Default: 5
    uint32_t slowCallIncUseCount;

    void setEagerCompilation() {
        eagerCompilation = true;
        usesBeforeCompile = usesBeforeCompileNoJaeger = 0;

        // Eagerly inline calls to improve test coverage.
        usesBeforeInlining = 0;
        smallFunctionUsesBeforeInlining = 0;

        parallelCompilation = false;
    }

    IonOptions()
      : gvn(true),
        gvnIsOptimistic(true),
        licm(true),
        osr(true),
        limitScriptSize(true),
        registerAllocator(RegisterAllocator_LSRA),
        inlining(true),
        edgeCaseAnalysis(true),
        rangeAnalysis(true),
        uce(true),
        parallelCompilation(false),
        usesBeforeCompile(10240),
        usesBeforeCompileNoJaeger(40),
        usesBeforeInlining(usesBeforeCompile),
        maxStackArgs(4096),
        maxInlineDepth(3),
        smallFunctionMaxBytecodeLength(100),
        smallFunctionUsesBeforeInlining(usesBeforeInlining / 4),
        polyInlineMax(4),
        inlineMaxTotalBytecodeLength(800),
        inlineUseCountRatio(128),
        eagerCompilation(false),
        slowCallLimit(512),
        slowCallIncUseCount(5)
    {
    }
};

enum MethodStatus
{
    Method_Error,
    Method_CantCompile,
    Method_Skipped,
    Method_Compiled
};

// An Ion context is needed to enter into either an Ion method or an instance
// of the Ion compiler. It points to a temporary allocator and the active
// JSContext, either of which may be NULL, and the active compartment, which
// will not be NULL.

class IonContext
{
  public:
    IonContext(JSContext *cx, JSCompartment *compartment, TempAllocator *temp);
    ~IonContext();

    JSContext *cx;
    JSCompartment *compartment;
    TempAllocator *temp;
    int getNextAssemblerId() {
        return assemblerCount_++;
    }
  private:
    IonContext *prev_;
    int assemblerCount_;
};

extern IonOptions js_IonOptions;

// Initialize Ion statically for all JSRuntimes.
bool InitializeIon();

// Get and set the current Ion context.
IonContext *GetIonContext();

bool SetIonContext(IonContext *ctx);

bool CanIonCompileScript(JSScript *script);

MethodStatus CanEnterAtBranch(JSContext *cx, HandleScript script,
                              StackFrame *fp, jsbytecode *pc);
MethodStatus CanEnter(JSContext *cx, HandleScript script, StackFrame *fp, bool newType);
MethodStatus CanEnterUsingFastInvoke(JSContext *cx, HandleScript script, uint32_t numActualArgs);

enum IonExecStatus
{
    // The method call had to be aborted due to a stack limit check. This
    // error indicates that Ion never attempted to clean up frames.
    IonExec_Aborted,

    // The method call resulted in an error, and IonMonkey has cleaned up
    // frames.
    IonExec_Error,

    // The method call succeeed and returned a value.
    IonExec_Ok,

    // A guard triggered in IonMonkey and we must resume execution in
    // the interpreter.
    IonExec_Bailout
};

static inline bool
IsErrorStatus(IonExecStatus status)
{
    return status == IonExec_Error || status == IonExec_Aborted;
}

IonExecStatus Cannon(JSContext *cx, StackFrame *fp);
IonExecStatus SideCannon(JSContext *cx, StackFrame *fp, jsbytecode *pc);

// Used to enter Ion from C++ natives like Array.map. Called from FastInvokeGuard.
IonExecStatus FastInvoke(JSContext *cx, HandleFunction fun, CallArgsList &args);

// Walk the stack and invalidate active Ion frames for the invalid scripts.
void Invalidate(types::TypeCompartment &types, FreeOp *fop,
                const Vector<types::RecompileInfo> &invalid, bool resetUses = true);
void Invalidate(JSContext *cx, const Vector<types::RecompileInfo> &invalid, bool resetUses = true);
bool Invalidate(JSContext *cx, JSScript *script, bool resetUses = true);

void MarkValueFromIon(JSRuntime *rt, Value *vp);
void MarkShapeFromIon(JSRuntime *rt, Shape **shapep);

void ToggleBarriers(JSCompartment *comp, bool needs);

class IonBuilder;
class MIRGenerator;
class CodeGenerator;

CodeGenerator *CompileBackEnd(MIRGenerator *mir);
void AttachFinishedCompilations(JSContext *cx);
void FinishOffThreadBuilder(IonBuilder *builder);
bool TestIonCompile(JSContext *cx, JSScript *script, JSFunction *fun, jsbytecode *osrPc, bool constructing);

static inline bool IsEnabled(JSContext *cx)
{
    return cx->hasRunOption(JSOPTION_ION) && cx->typeInferenceEnabled();
}

void ForbidCompilation(JSContext *cx, JSScript *script);
uint32_t UsesBeforeIonRecompile(JSScript *script, jsbytecode *pc);

void PurgeCaches(JSScript *script, JSCompartment *c);
size_t MemoryUsed(JSScript *script, JSMallocSizeOfFun mallocSizeOf);
void DestroyIonScripts(FreeOp *fop, JSScript *script);
void TraceIonScripts(JSTracer* trc, JSScript *script);

} // namespace ion
} // namespace js

#endif // jsion_ion_h__

