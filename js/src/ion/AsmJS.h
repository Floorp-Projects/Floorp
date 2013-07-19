/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ion_AsmJS_h
#define ion_AsmJS_h

#ifdef XP_MACOSX
# include <pthread.h>
# include <mach/mach.h>
#endif

namespace js {

class ScriptSource;
class SPSProfiler;
class AsmJSModule;
namespace frontend {
    template <typename ParseHandler> struct Parser;
    template <typename ParseHandler> struct ParseContext;
    class FullParseHandler;
    struct ParseNode;
}
namespace ion {
    class MIRGenerator;
    class LIRGraph;
}

typedef frontend::Parser<frontend::FullParseHandler> AsmJSParser;
typedef frontend::ParseContext<frontend::FullParseHandler> AsmJSParseContext;

// Takes over parsing of a function starting with "use asm". The return value
// indicates whether an error was reported which the caller should propagate.
// If no error was reported, the function may still fail to validate as asm.js.
// In this case, the parser.tokenStream has been advanced an indeterminate
// amount and the entire function should be reparsed from the beginning.
extern bool
CompileAsmJS(JSContext *cx, AsmJSParser &parser, frontend::ParseNode *stmtList, bool *validated);

// Implements the semantics of an asm.js module function that has been successfully validated.
// A successfully validated asm.js module does not have bytecode emitted, but rather a list of
// dynamic constraints that must be satisfied by the arguments passed by the caller. If these
// constraints are satisfied, then LinkAsmJS can return CallAsmJS native functions that trampoline
// into compiled code. If any of the constraints fails, LinkAsmJS reparses the entire asm.js module
// from source so that it can be run as plain bytecode.
extern JSBool
LinkAsmJS(JSContext *cx, unsigned argc, JS::Value *vp);

// The js::Native for the functions nested in an asm.js module. Calling this
// native will trampoline into generated code.
extern JSBool
CallAsmJS(JSContext *cx, unsigned argc, JS::Value *vp);

// Force any currently-executing asm.js code to call
// js_HandleExecutionInterrupt.
void
TriggerOperationCallbackForAsmJSCode(JSRuntime *rt);

// The JSRuntime maintains a stack of AsmJSModule activations. An "activation"
// of module A is an initial call from outside A into a function inside A,
// followed by a sequence of calls inside A, and terminated by a call that
// leaves A. The AsmJSActivation stack serves three purposes:
//  - record the correct cx to pass to VM calls from asm.js;
//  - record enough information to pop all the frames of an activation if an
//    exception is thrown;
//  - record the information necessary for asm.js signal handlers to safely
//    recover from (expected) out-of-bounds access, the operation callback,
//    stack overflow, division by zero, etc.
class AsmJSActivation
{
    JSContext *cx_;
    AsmJSModule &module_;
    AsmJSActivation *prev_;
    void *errorRejoinSP_;
    SPSProfiler *profiler_;
    void *resumePC_;

  public:
    AsmJSActivation(JSContext *cx, AsmJSModule &module);
    ~AsmJSActivation();

    JSContext *cx() { return cx_; }
    AsmJSModule &module() const { return module_; }

    // Read by JIT code:
    static unsigned offsetOfContext() { return offsetof(AsmJSActivation, cx_); }
    static unsigned offsetOfResumePC() { return offsetof(AsmJSActivation, resumePC_); }

    // Initialized by JIT code:
    static unsigned offsetOfErrorRejoinSP() { return offsetof(AsmJSActivation, errorRejoinSP_); }

    // Set from SIGSEGV handler:
    void setResumePC(void *pc) { resumePC_ = pc; }
};

// The assumed page size; dynamically checked in CompileAsmJS.
const size_t AsmJSPageSize = 4096;

// The asm.js spec requires that the ArrayBuffer's byteLength be a multiple of 4096.
static const size_t AsmJSAllocationGranularity = 4096;

#ifdef JS_CPU_X64
// On x64, the internal ArrayBuffer data array is inflated to 4GiB (only the
// byteLength portion of which is accessible) so that out-of-bounds accesses
// (made using a uint32 index) are guaranteed to raise a SIGSEGV.
static const size_t AsmJSBufferProtectedSize = 4 * 1024ULL * 1024ULL * 1024ULL;
#endif

#ifdef XP_MACOSX
class AsmJSMachExceptionHandler
{
    bool installed_;
    pthread_t thread_;
    mach_port_t port_;

    void release();

  public:
    AsmJSMachExceptionHandler();
    ~AsmJSMachExceptionHandler() { release(); }
    mach_port_t port() const { return port_; }
    bool installed() const { return installed_; }
    bool install(JSRuntime *rt);
    void clearCurrentThread();
    void setCurrentThread();
};
#endif

struct DependentAsmJSModuleExit
{
    const AsmJSModule *module;
    size_t exitIndex;

    DependentAsmJSModuleExit(const AsmJSModule *module, size_t exitIndex)
      : module(module),
        exitIndex(exitIndex)
    { }
};

// Struct type for passing parallel compilation data between the main thread
// and compilation workers.
struct AsmJSParallelTask
{
    LifoAlloc lifo;         // Provider of all heap memory used for compilation.

    void *func;             // Really, a ModuleCompiler::Func*
    ion::MIRGenerator *mir; // Passed from main thread to worker.
    ion::LIRGraph *lir;     // Passed from worker to main thread.
    unsigned compileTime;

    AsmJSParallelTask(size_t defaultChunkSize)
      : lifo(defaultChunkSize), func(NULL), mir(NULL), lir(NULL), compileTime(0)
    { }

    void init(void *func, ion::MIRGenerator *mir) {
        this->func = func;
        this->mir = mir;
        this->lir = NULL;
    }
};

// Returns true if the given native is the one that is used to implement asm.js
// module functions.
#ifdef JS_ION
extern bool
IsAsmJSModuleNative(js::Native native);
#else
inline bool
IsAsmJSModuleNative(js::Native native)
{
    return false;
}
#endif

// Exposed for shell testing:

// Return whether asm.js optimization is inhibitted by the platform or
// dynamically disabled:
extern JSBool
IsAsmJSCompilationAvailable(JSContext *cx, unsigned argc, Value *vp);

// Return whether the given value is a function containing "use asm" that has
// been validated according to the asm.js spec.
extern JSBool
IsAsmJSModule(JSContext *cx, unsigned argc, Value *vp);

// Return whether the given value is a nested function in an asm.js module that
// has been both compile- and link-time validated.
extern JSBool
IsAsmJSFunction(JSContext *cx, unsigned argc, Value *vp);

} // namespace js

#endif /* ion_AsmJS_h */
