/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined jsjaeger_h__ && defined JS_METHODJIT
#define jsjaeger_h__

#include "mozilla/PodOperations.h"

#ifdef JSGC_INCREMENTAL
#define JSGC_INCREMENTAL_MJ
#endif

#include "jscntxt.h"
#include "jscompartment.h"

#include "assembler/assembler/MacroAssemblerCodeRef.h"
#include "assembler/assembler/CodeLocation.h"

#if !defined JS_CPU_X64 && \
    !defined JS_CPU_X86 && \
    !defined JS_CPU_SPARC && \
    !defined JS_CPU_ARM && \
    !defined JS_CPU_MIPS
# error "Oh no, you should define a platform so this compiles."
#endif

#if !defined(JS_NUNBOX32) && !defined(JS_PUNBOX64)
# error "No boxing format selected."
#endif

namespace js {

namespace mjit {
    struct JITChunk;
    struct JITScript;
}

namespace analyze {
    struct ScriptLiveness;
}

struct VMFrame
{
#if defined(JS_CPU_SPARC)
    void *savedL0;
    void *savedL1;
    void *savedL2;
    void *savedL3;
    void *savedL4;
    void *savedL5;
    void *savedL6;
    void *savedL7;
    void *savedI0;
    void *savedI1;
    void *savedI2;
    void *savedI3;
    void *savedI4;
    void *savedI5;
    void *savedI6;
    void *savedI7;

    void *str_p;

    void *outgoing_p0;
    void *outgoing_p1;
    void *outgoing_p2;
    void *outgoing_p3;
    void *outgoing_p4;
    void *outgoing_p5;

    void *outgoing_p6;

    void *reserve_0;
    void *reserve_1;

#elif defined(JS_CPU_MIPS)
    /* Reserved 16 bytes for a0-a3 space in MIPS O32 ABI */
    void         *unused0;
    void         *unused1;
    void         *unused2;
    void         *unused3;
#endif

    union Arguments {
        struct {
            void *ptr;
            void *ptr2;
        } x;
        struct {
            uint32_t dynamicArgc;
        } call;
    } u;

    static size_t offsetOfDynamicArgc() {
        return offsetof(VMFrame, u.call.dynamicArgc);
    }

    VMFrame      *previous;
    void         *scratch;
    FrameRegs    regs;

    static size_t offsetOfRegsSp() {
        return offsetof(VMFrame, regs.sp);
    }

    static size_t offsetOfRegsPc() {
        return offsetof(VMFrame, regs.pc);
    }

    JSContext    *cx;
    Value        *stackLimit;
    StackFrame   *entryfp;
    FrameRegs    *oldregs;
    FrameRejoinState stubRejoin;  /* How to rejoin if inside a call from an IC stub. */

#if defined(JS_CPU_X86)
    void         *unused0, *unused1;  /* For 16 byte alignment */
#endif

#if defined(JS_CPU_X86)
    void *savedEBX;
    void *savedEDI;
    void *savedESI;
    void *savedEBP;
    void *savedEIP;

# ifdef JS_NO_FASTCALL
    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 5;
    }
# else
    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 1;
    }
# endif

    /* The gap between ebp and esp in JaegerTrampoline frames on X86 platforms. */
    static const uint32_t STACK_BASE_DIFFERENCE = 0x38;

#elif defined(JS_CPU_X64)
    void *savedRBX;
# ifdef _WIN64
    void *savedRSI;
    void *savedRDI;
# endif
    void *savedR15;
    void *savedR14;
    void *savedR13;
    void *savedR12;
    void *savedRBP;
    void *savedRIP;

# ifdef _WIN64
    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 5;
    }
# else
    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 1;
    }
# endif

#elif defined(JS_CPU_ARM)
    void *savedR4;
    void *savedR5;
    void *savedR6;
    void *savedR7;
    void *savedR8;
    void *savedR9;
    void *savedR10;
    void *savedR11;
    void *savedLR;

    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 1;
    }
#elif defined(JS_CPU_SPARC)
    JSStackFrame *topRetrunAddr;
    void* veneerReturn;
    void* _align;
    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(&this->veneerReturn);
    }
#elif defined(JS_CPU_MIPS)
    void *savedS0;
    void *savedS1;
    void *savedS2;
    void *savedS3;
    void *savedS4;
    void *savedS5;
    void *savedS6;
    void *savedS7;
    void *savedGP;
    void *savedRA;
    void *unused4;  // For alignment.

    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 1;
    }
#else
# error "The VMFrame layout isn't defined for your processor architecture!"
#endif

    JSRuntime *runtime() { return cx->runtime; }

    /*
     * Get the current frame and JIT. Note that these are NOT stable in case
     * of recompilations; all code which expects these to be stable should
     * check that cx->recompilations() has not changed across a call that could
     * trigger recompilation (pretty much any time the VM is called into).
     */
    StackFrame *fp() { return regs.fp(); }
    mjit::JITScript *jit() { return fp()->jit(); }

    inline mjit::JITChunk *chunk();
    inline unsigned chunkIndex();

    /* Get the inner script/PC in case of inlining. */
    inline JSScript *script();
    inline jsbytecode *pc();

#if defined(JS_CPU_SPARC)
    static const size_t offsetOfFp = 30 * sizeof(void *) + FrameRegs::offsetOfFp;
    static const size_t offsetOfInlined = 30 * sizeof(void *) + FrameRegs::offsetOfInlined;
#elif defined(JS_CPU_MIPS)
    static const size_t offsetOfFp = 8 * sizeof(void *) + FrameRegs::offsetOfFp;
    static const size_t offsetOfInlined = 8 * sizeof(void *) + FrameRegs::offsetOfInlined;
#else
    static const size_t offsetOfFp = 4 * sizeof(void *) + FrameRegs::offsetOfFp;
    static const size_t offsetOfInlined = 4 * sizeof(void *) + FrameRegs::offsetOfInlined;
#endif

    static void staticAssert() {
        JS_STATIC_ASSERT(offsetOfFp == offsetof(VMFrame, regs) + FrameRegs::offsetOfFp);
        JS_STATIC_ASSERT(offsetOfInlined == offsetof(VMFrame, regs) + FrameRegs::offsetOfInlined);
    }
};

#if defined(JS_CPU_ARM) || defined(JS_CPU_SPARC) || defined(JS_CPU_MIPS)
// WARNING: Do not call this function directly from C(++) code because it is not ABI-compliant.
extern "C" void JaegerStubVeneer(void);
# if defined(JS_CPU_ARM)
extern "C" void IonVeneer(void);
# endif
#endif

namespace mjit {

/*
 * For a C++ or scripted call made from JIT code, indicates properties of the
 * register and stack state after the call finishes, which js_InternalInterpret
 * must use to construct a coherent state for rejoining into the interpreter.
 */
enum RejoinState {
    /*
     * Return value of call at this bytecode is held in ReturnReg_{Data,Type}
     * and needs to be restored before starting the next bytecode. f.regs.pc
     * is *not* intact when rejoining from a scripted call (unlike all other
     * rejoin states). The pc's offset into the script is stored in the upper
     * 31 bits of the rejoin state, and the remaining values for RejoinState
     * are shifted left by one in stack frames to leave the lower bit set only
     * for scripted calls.
     */
    REJOIN_SCRIPTED = 1,

    /* Recompilations and frame expansion are impossible for this call. */
    REJOIN_NONE,

    /* State is coherent for the start of the current bytecode. */
    REJOIN_RESUME,

    /*
     * State is coherent for the start of the current bytecode, which is a TRAP
     * that has already been invoked and should not be invoked again.
     */
    REJOIN_TRAP,

    /* State is coherent for the start of the next (fallthrough) bytecode. */
    REJOIN_FALLTHROUGH,

    /*
     * As for REJOIN_FALLTHROUGH, but holds a reference on the compartment's
     * orphaned native pools which needs to be reclaimed by InternalInterpret.
     * The return value needs to be adjusted if REJOIN_NATIVE_LOWERED, and
     * REJOIN_NATIVE_GETTER is for ABI calls made for property accesses.
     */
    REJOIN_NATIVE,
    REJOIN_NATIVE_LOWERED,
    REJOIN_NATIVE_GETTER,

    /*
     * Dummy rejoin stored in VMFrames to indicate they return into a native
     * stub (and their FASTCALL return address should not be observed) but
     * that they have already been patched and can be ignored.
     */
    REJOIN_NATIVE_PATCHED,

    /* Call returns a payload, which should be pushed before starting next bytecode. */
    REJOIN_PUSH_BOOLEAN,
    REJOIN_PUSH_OBJECT,

    /*
     * During the prologue of constructing scripts, after the function's
     * .prototype property has been fetched.
     */
    REJOIN_THIS_PROTOTYPE,

    /* As above, after the 'this' object has been created. */
    REJOIN_THIS_CREATED,

    /*
     * Type check on arguments failed during prologue, need stack check and
     * the rest of the JIT prologue before the script can execute.
     */
    REJOIN_CHECK_ARGUMENTS,

    /*
     * The script's jitcode was discarded during one of the following steps of
     * a frame's prologue.
     */
    REJOIN_EVAL_PROLOGUE,
    REJOIN_FUNCTION_PROLOGUE,

    /*
     * State after calling a stub which returns a JIT code pointer for a call
     * or NULL for an already-completed call.
     */
    REJOIN_CALL_PROLOGUE,
    REJOIN_CALL_PROLOGUE_LOWERED_CALL,
    REJOIN_CALL_PROLOGUE_LOWERED_APPLY,

    /* Triggered a recompilation while placing the arguments to an apply on the stack. */
    REJOIN_CALL_SPLAT,

    /* Like REJOIN_FALLTHROUGH, but handles getprop used as part of JSOP_INSTANCEOF. */
    REJOIN_GETTER,

    /*
     * For an opcode fused with IFEQ/IFNE, call returns a boolean indicating
     * the result of the comparison and whether to take or not take the branch.
     */
    REJOIN_BRANCH
};

/* Get the rejoin state for a StackFrame after returning from a scripted call. */
static inline FrameRejoinState
ScriptedRejoin(uint32_t pcOffset)
{
    return REJOIN_SCRIPTED | (pcOffset << 1);
}

/* Get the rejoin state for a StackFrame after returning from a stub call. */
static inline FrameRejoinState
StubRejoin(RejoinState rejoin)
{
    return rejoin << 1;
}

/* Helper to watch for recompilation and frame expansion activity on a compartment. */
struct RecompilationMonitor
{
    JSContext *cx;

    /*
     * If either inline frame expansion or recompilation occurs, then ICs and
     * stubs should not depend on the frame or JITs being intact. The two are
     * separated for logging.
     */
    unsigned recompilations;
    unsigned frameExpansions;

    /* If a GC occurs it may discard jit code on the stack. */
    uint64_t gcNumber;

    RecompilationMonitor(JSContext *cx)
        : cx(cx),
          recompilations(cx->compartment->types.recompilations),
          frameExpansions(cx->compartment->types.frameExpansions),
          gcNumber(cx->runtime->gcNumber)
    {}

    bool recompiled() {
        return cx->compartment->types.recompilations != recompilations
            || cx->compartment->types.frameExpansions != frameExpansions
            || cx->runtime->gcNumber != gcNumber;
    }
};

/*
 * Trampolines to force returns from jit code.
 * See also TrampolineCompiler::generateForceReturn(Fast).
 */
struct Trampolines {
    typedef void (*TrampolinePtr)();

    TrampolinePtr       forceReturn;
    JSC::ExecutablePool *forceReturnPool;

#if (defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)) || defined(_WIN64)
    TrampolinePtr       forceReturnFast;
    JSC::ExecutablePool *forceReturnFastPool;
#endif
};

/* Result status of executing mjit code on a frame. */
enum JaegerStatus
{
    /* Entry frame finished, and is throwing an exception. */
    Jaeger_Throwing = 0,

    /* Entry frame finished, and is returning. */
    Jaeger_Returned = 1,

    /*
     * Entry frame did not finish. cx->regs reflects where to resume execution.
     * This result is only possible if 'partial' is passed as true below.
     */
    Jaeger_Unfinished = 2,

    /*
     * As for Unfinished, but stopped after a TRAP triggered recompilation.
     * The trap has been reinstalled, but should not execute again when
     * resuming execution.
     */
    Jaeger_UnfinishedAtTrap = 3,

    /*
     * An exception was thrown before entering jit code, so the caller should
     * 'goto error'.
     */
    Jaeger_ThrowBeforeEnter = 4
};

static inline bool
JaegerStatusToSuccess(JaegerStatus status)
{
    JS_ASSERT(status != Jaeger_Unfinished);
    JS_ASSERT(status != Jaeger_UnfinishedAtTrap);
    return status == Jaeger_Returned;
}

/* Method JIT data associated with the JSRuntime. */
class JaegerRuntime
{
    Trampolines              trampolines;    // force-return trampolines
    VMFrame                  *activeFrame_;  // current active VMFrame
    JaegerStatus             lastUnfinished_;// result status of last VM frame,
                                             // if unfinished

    void finish();

  public:
    bool init(JSContext *cx);

    JaegerRuntime();
    ~JaegerRuntime() { finish(); }

    VMFrame *activeFrame() {
        return activeFrame_;
    }

    void pushActiveFrame(VMFrame *f) {
        JS_ASSERT(!lastUnfinished_);
        f->previous = activeFrame_;
        f->scratch = NULL;
        activeFrame_ = f;
    }

    void popActiveFrame() {
        JS_ASSERT(activeFrame_);
        activeFrame_ = activeFrame_->previous;
    }

    void setLastUnfinished(JaegerStatus status) {
        JS_ASSERT(!lastUnfinished_);
        lastUnfinished_ = status;
    }

    JaegerStatus lastUnfinished() {
        JaegerStatus result = lastUnfinished_;
        lastUnfinished_ = (JaegerStatus) 0;
        return result;
    }

    /*
     * To force the top StackFrame in a VMFrame to return, when that VMFrame
     * has called an extern "C" function (say, js_InternalThrow or
     * js_InternalInterpret), change the extern "C" function's return address
     * to the value this method returns.
     */
    void *forceReturnFromExternC() const {
        return JS_FUNC_TO_DATA_PTR(void *, trampolines.forceReturn);
    }

    /*
     * To force the top StackFrame in a VMFrame to return, when that VMFrame has
     * called a fastcall function (say, most stubs:: functions), change the
     * fastcall function's return address to the value this method returns.
     */
    void *forceReturnFromFastCall() const {
#if (defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)) || defined(_WIN64)
        return JS_FUNC_TO_DATA_PTR(void *, trampolines.forceReturnFast);
#else
        return JS_FUNC_TO_DATA_PTR(void *, trampolines.forceReturn);
#endif
    }

    /*
     * References held on pools created for native ICs, where the IC was
     * destroyed and we are waiting for the pool to finish use and jump
     * into the interpoline.
     */
    Vector<StackFrame *, 8, SystemAllocPolicy> orphanedNativeFrames;
    Vector<JSC::ExecutablePool *, 8, SystemAllocPolicy> orphanedNativePools;
};

/*
 * Allocation policy for compiler jstl objects. The goal is to free the
 * compiler from having to check and propagate OOM after every time we
 * append to a vector. We do this by reporting OOM to the engine and
 * setting a flag on the compiler when OOM occurs. The compiler is required
 * to check for OOM only before trying to use the contents of the list.
 */
class CompilerAllocPolicy : public TempAllocPolicy
{
    bool *oomFlag;

    void *checkAlloc(void *p) {
        if (!p)
            *oomFlag = true;
        return p;
    }

  public:
    CompilerAllocPolicy(JSContext *cx, bool *oomFlag)
    : TempAllocPolicy(cx), oomFlag(oomFlag) {}
    CompilerAllocPolicy(JSContext *cx, Compiler &compiler);

    void *malloc_(size_t bytes) { return checkAlloc(TempAllocPolicy::malloc_(bytes)); }
    void *realloc_(void *p, size_t oldBytes, size_t bytes) {
        return checkAlloc(TempAllocPolicy::realloc_(p, oldBytes, bytes));
    }
};

namespace ic {
# if defined JS_POLYIC
    struct PICInfo;
    struct GetElementIC;
    struct SetElementIC;
# endif
# if defined JS_MONOIC
    struct GetGlobalNameIC;
    struct SetGlobalNameIC;
    struct EqualityICInfo;
    struct CallICInfo;
# endif
}
}

typedef void (JS_FASTCALL *VoidStub)(VMFrame &);
typedef void (JS_FASTCALL *VoidVpStub)(VMFrame &, Value *);
typedef void (JS_FASTCALL *VoidStubUInt32)(VMFrame &, uint32_t);
typedef void (JS_FASTCALL *VoidStubInt32)(VMFrame &, int32_t);
typedef JSBool (JS_FASTCALL *BoolStub)(VMFrame &);
typedef void * (JS_FASTCALL *VoidPtrStub)(VMFrame &);
typedef void * (JS_FASTCALL *VoidPtrStubPC)(VMFrame &, jsbytecode *);
typedef void * (JS_FASTCALL *VoidPtrStubUInt32)(VMFrame &, uint32_t);
typedef JSObject * (JS_FASTCALL *JSObjStub)(VMFrame &);
typedef JSObject * (JS_FASTCALL *JSObjStubUInt32)(VMFrame &, uint32_t);
typedef JSObject * (JS_FASTCALL *JSObjStubFun)(VMFrame &, JSFunction *);
typedef void (JS_FASTCALL *VoidStubFun)(VMFrame &, JSFunction *);
typedef JSObject * (JS_FASTCALL *JSObjStubJSObj)(VMFrame &, JSObject *);
typedef void (JS_FASTCALL *VoidStubName)(VMFrame &, PropertyName *);
typedef JSString * (JS_FASTCALL *JSStrStub)(VMFrame &);
typedef JSString * (JS_FASTCALL *JSStrStubUInt32)(VMFrame &, uint32_t);
typedef void (JS_FASTCALL *VoidStubJSObj)(VMFrame &, JSObject *);
typedef void (JS_FASTCALL *VoidStubPC)(VMFrame &, jsbytecode *);
typedef JSBool (JS_FASTCALL *BoolStubUInt32)(VMFrame &f, uint32_t);
#ifdef JS_MONOIC
typedef void (JS_FASTCALL *VoidStubCallIC)(VMFrame &, js::mjit::ic::CallICInfo *);
typedef void * (JS_FASTCALL *VoidPtrStubCallIC)(VMFrame &, js::mjit::ic::CallICInfo *);
typedef void (JS_FASTCALL *VoidStubGetGlobal)(VMFrame &, js::mjit::ic::GetGlobalNameIC *);
typedef void (JS_FASTCALL *VoidStubSetGlobal)(VMFrame &, js::mjit::ic::SetGlobalNameIC *);
typedef JSBool (JS_FASTCALL *BoolStubEqualityIC)(VMFrame &, js::mjit::ic::EqualityICInfo *);
#endif
#ifdef JS_POLYIC
typedef void (JS_FASTCALL *VoidStubPIC)(VMFrame &, js::mjit::ic::PICInfo *);
typedef void (JS_FASTCALL *VoidStubGetElemIC)(VMFrame &, js::mjit::ic::GetElementIC *);
typedef void (JS_FASTCALL *VoidStubSetElemIC)(VMFrame &f, js::mjit::ic::SetElementIC *);
#endif

namespace mjit {

struct InlineFrame;
struct CallSite;
struct CompileTrigger;

struct NativeMapEntry {
    size_t          bcOff;  /* bytecode offset in script */
    void            *ncode; /* pointer to native code */
};

/* Per-op counts of performance metrics. */
struct PCLengthEntry {
    double          inlineLength; /* amount of inline code generated */
    double          picsLength;   /* amount of PIC stub code generated */
    double          stubLength;   /* amount of stubcc code generated */
    double          codeLengthAugment; /* augment to inlineLength to be added
                                          at runtime, represents instrumentation
                                          taken out or common stubcc accounted
                                          for (instead of just adding inlineLength) */
};

/*
 * Pools and patch locations for managing stubs for non-FASTCALL C++ calls made
 * from native call and PropertyOp stubs. Ownership of these may be transferred
 * into the orphanedNativePools for the compartment.
 */
struct NativeCallStub {
    /* PC for the stub. Native call stubs cannot be added for inline frames. */
    jsbytecode *pc;

    /* Pool for the stub, NULL if it has been removed from the script. */
    JSC::ExecutablePool *pool;

    /*
     * Fallthrough jump returning to jitcode which may be patched during
     * recompilation. On x64 this is an indirect jump to avoid issues with far
     * jumps on relative branches.
     */
#ifdef JS_CPU_X64
    JSC::CodeLocationDataLabelPtr jump;
#else
    JSC::CodeLocationJump jump;
#endif
};

struct JITChunk
{
    typedef JSC::MacroAssemblerCodeRef CodeRef;
    CodeRef         code;       /* pool & code addresses */

    PCLengthEntry   *pcLengths;         /* lengths for outer and inline frames */

    /*
     * This struct has several variable-length sections that are allocated on
     * the end:  nmaps, MICs, callICs, etc.  To save space -- worthwhile
     * because JITScripts are common -- we only record their lengths.  We can
     * find any of the sections from the lengths because we know their order.
     * Therefore, do not change the section ordering in finishThisUp() without
     * changing nMICs() et al as well.
     */
    uint32_t        nNmapPairs : 31;    /* The NativeMapEntrys are sorted by .bcOff.
                                           .ncode values may not be NULL. */
    uint32_t        nInlineFrames;
    uint32_t        nCallSites;
    uint32_t        nCompileTriggers;
    uint32_t        nRootedTemplates;
    uint32_t        nRootedRegExps;
    uint32_t        nMonitoredBytecodes;
    uint32_t        nTypeBarrierBytecodes;
#ifdef JS_MONOIC
    uint32_t        nGetGlobalNames;
    uint32_t        nSetGlobalNames;
    uint32_t        nCallICs;
    uint32_t        nEqualityICs;
#endif
#ifdef JS_POLYIC
    uint32_t        nGetElems;
    uint32_t        nSetElems;
    uint32_t        nPICs;
#endif

#ifdef JS_MONOIC
    // Additional ExecutablePools that IC stubs were generated into.
    typedef Vector<JSC::ExecutablePool *, 0, SystemAllocPolicy> ExecPoolVector;
    ExecPoolVector execPools;
#endif

    types::RecompileInfo recompileInfo;

    // Additional ExecutablePools for native call and getter stubs.
    Vector<NativeCallStub, 0, SystemAllocPolicy> nativeCallStubs;

    NativeMapEntry *nmap() const;
    js::mjit::InlineFrame *inlineFrames() const;
    js::mjit::CallSite *callSites() const;
    js::mjit::CompileTrigger *compileTriggers() const;
    JSObject **rootedTemplates() const;
    RegExpShared **rootedRegExps() const;

    /*
     * Offsets of bytecodes which were monitored or had type barriers at the
     * point of compilation. Used to avoid unnecessary recompilation after
     * analysis purges.
     */
    uint32_t *monitoredBytecodes() const;
    uint32_t *typeBarrierBytecodes() const;

#ifdef JS_MONOIC
    ic::GetGlobalNameIC *getGlobalNames() const;
    ic::SetGlobalNameIC *setGlobalNames() const;
    ic::CallICInfo *callICs() const;
    ic::EqualityICInfo *equalityICs() const;
#endif
#ifdef JS_POLYIC
    ic::GetElementIC *getElems() const;
    ic::SetElementIC *setElems() const;
    ic::PICInfo     *pics() const;
#endif

    bool isValidCode(void *ptr) {
        char *jitcode = (char *)code.m_code.executableAddress();
        char *jcheck = (char *)ptr;
        return jcheck >= jitcode && jcheck < jitcode + code.m_size;
    }

    size_t computedSizeOfIncludingThis();
    size_t sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf);

    ~JITChunk();

    void trace(JSTracer *trc);
    void purgeCaches();

  private:
    /* Helpers used to navigate the variable-length sections. */
    char *commonSectionLimit() const;
    char *monoICSectionsLimit() const;
    char *polyICSectionsLimit() const;
};

void
SetChunkLimit(uint32_t limit);

/* Information about a compilation chunk within a script. */
struct ChunkDescriptor
{
    /* Bytecode range of the chunk: [begin,end) */
    uint32_t begin;
    uint32_t end;

    /* Use counter for the chunk. */
    uint32_t counter;

    /* Optional compiled code for the chunk. */
    JITChunk *chunk;

    ChunkDescriptor() { mozilla::PodZero(this); }
};

/* Jump or fallthrough edge in the bytecode which crosses a chunk boundary. */
struct CrossChunkEdge
{
    /* Bytecode offsets of the source and target of the edge. */
    uint32_t source;
    uint32_t target;

    /* Locations of the jump(s) for the source, NULL if not compiled. */
    void *sourceJump1;
    void *sourceJump2;

#ifdef JS_CPU_X64
    /*
     * Location of a trampoline for the edge to perform an indirect jump if
     * out of range, NULL if the source is not compiled.
     */
    void *sourceTrampoline;
#endif

    /* Any jump table entries along this edge. */
    typedef Vector<void**,4,SystemAllocPolicy> JumpTableEntryVector;
    JumpTableEntryVector *jumpTableEntries;

    /* Location of the label for the target, NULL if not compiled. */
    void *targetLabel;

    /*
     * Location of a shim which will transfer control to the interpreter at the
     * target bytecode. The source jumps are patched to jump to this label if
     * the source is compiled but not the target.
     */
    void *shimLabel;

    CrossChunkEdge() { mozilla::PodZero(this); }
};

struct JITScript
{
    JSScript        *script;

    void            *invokeEntry;       /* invoke address */
    void            *fastEntry;         /* cached entry, fastest */
    void            *arityCheckEntry;   /* arity check address */
    void            *argsCheckEntry;    /* arguments check address */

    /* List of inline caches jumping to the fastEntry. */
    JSCList         callers;

    uint32_t        nchunks;
    uint32_t        nedges;

    /*
     * Pool for shims which transfer control to the interpreter on cross chunk
     * edges to chunks which do not have compiled code.
     */
    JSC::ExecutablePool *shimPool;

    /*
     * Optional liveness information attached to the JITScript if the analysis
     * information is purged while retaining JIT info.
     */
    analyze::ScriptLiveness *liveness;

    /*
     * Number of calls made to IonMonkey functions, used to avoid slow
     * JM -> Ion calls.
     */
    uint32_t        ionCalls;

    /*
     * If set, we decided to keep the JITChunk so that Ion can access its caches.
     * The chunk has to be destroyed the next time the script runs in JM.
     * Note that this flag implies nchunks == 1.
     */
    bool mustDestroyEntryChunk;

#ifdef JS_MONOIC
    /* Inline cache at function entry for checking this/argument types. */
    JSC::CodeLocationLabel argsCheckStub;
    JSC::CodeLocationLabel argsCheckFallthrough;
    JSC::CodeLocationJump  argsCheckJump;
    JSC::ExecutablePool *argsCheckPool;
    void resetArgsCheck();
#endif

    ChunkDescriptor &chunkDescriptor(unsigned i) {
        JS_ASSERT(i < nchunks);
        ChunkDescriptor *descs = (ChunkDescriptor *) ((char *) this + sizeof(JITScript));
        return descs[i];
    }

    unsigned chunkIndex(jsbytecode *pc) {
        unsigned offset = pc - script->code;
        JS_ASSERT(offset < script->length);
        for (unsigned i = 0; i < nchunks; i++) {
            const ChunkDescriptor &desc = chunkDescriptor(i);
            JS_ASSERT(desc.begin <= offset);
            if (offset < desc.end)
                return i;
        }
        JS_NOT_REACHED("Bad chunk layout");
        return 0;
    }

    JITChunk *chunk(jsbytecode *pc) {
        return chunkDescriptor(chunkIndex(pc)).chunk;
    }

    JITChunk *findCodeChunk(void *addr);

    CrossChunkEdge *edges() {
        return (CrossChunkEdge *) (&chunkDescriptor(0) + nchunks);
    }

    /* Patch any compiled sources in edge to jump to label. */
    void patchEdge(const CrossChunkEdge &edge, void *label);

    jsbytecode *nativeToPC(void *returnAddress, CallSite **pinline);

    size_t sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf);

    void destroy(FreeOp *fop);
    void destroyChunk(FreeOp *fop, unsigned chunkIndex, bool resetUses = true);

    void trace(JSTracer *trc);
    void purgeCaches();

    void disableScriptEntry();
};

/*
 * Execute the given mjit code. This is a low-level call and callers must
 * provide the same guarantees as JaegerShot/CheckStackAndEnterMethodJIT.
 */
JaegerStatus EnterMethodJIT(JSContext *cx, StackFrame *fp, void *code, Value *stackLimit,
                            bool partial);

/* Execute a method that has been JIT compiled. */
JaegerStatus JaegerShot(JSContext *cx, bool partial);

/* Drop into the middle of a method at an arbitrary point, and execute. */
JaegerStatus JaegerShotAtSafePoint(JSContext *cx, void *safePoint, bool partial);

enum CompileStatus
{
    Compile_Okay,
    Compile_Abort,        // abort compilation
    Compile_InlineAbort,  // inlining attempt failed, continue compilation
    Compile_Retry,        // static overflow or failed inline, try to recompile
    Compile_Error,        // OOM
    Compile_Skipped
};

void JS_FASTCALL
ProfileStubCall(VMFrame &f);

enum CompileRequest
{
    CompileRequest_Interpreter,
    CompileRequest_JIT
};

CompileStatus
CanMethodJIT(JSContext *cx, JSScript *script, jsbytecode *pc,
             bool construct, CompileRequest request, StackFrame *sp);

inline void
ReleaseScriptCode(FreeOp *fop, JSScript *script)
{
    if (!script->hasMJITInfo())
        return;

    for (int constructing = 0; constructing <= 1; constructing++) {
        for (int barriers = 0; barriers <= 1; barriers++) {
            JSScript::JITScriptHandle *jith = script->jitHandle((bool) constructing, (bool) barriers);
            if (jith && jith->isValid())
                JSScript::ReleaseCode(fop, jith);
        }
    }

    script->destroyMJITInfo(fop);
}

// Cripple any JIT code for the specified script, such that the next time
// execution reaches the script's entry or the OSR PC the script's code will
// be destroyed.
void
DisableScriptCodeForIon(JSScript *script, jsbytecode *osrPC);

// Expand all stack frames inlined by the JIT within a compartment.
void
ExpandInlineFrames(JS::Zone *zone);

// Return all VMFrames in a compartment to the interpreter. This must be
// followed by destroying all JIT code in the compartment.
void
ClearAllFrames(JS::Zone *zone);

// Information about a frame inlined during compilation.
struct InlineFrame
{
    InlineFrame *parent;
    jsbytecode *parentpc;
    HeapPtrFunction fun;

    // Total distance between the start of the outer JSStackFrame and the start
    // of this frame, in multiples of sizeof(Value).
    uint32_t depth;
};

struct CallSite
{
    uint32_t codeOffset;
    uint32_t inlineIndex;
    uint32_t pcOffset;
    RejoinState rejoin;

    void initialize(uint32_t codeOffset, uint32_t inlineIndex, uint32_t pcOffset,
                    RejoinState rejoin) {
        this->codeOffset = codeOffset;
        this->inlineIndex = inlineIndex;
        this->pcOffset = pcOffset;
        this->rejoin = rejoin;
    }

    bool isTrap() const {
        return rejoin == REJOIN_TRAP;
    }
};

// Information about a check inserted into the script for triggering Ion
// compilation at a function or loop entry point.
struct CompileTrigger
{
    uint32_t pcOffset;

    // Offsets into the generated code of the conditional jump in the inline
    // path and the start of the sync code for the trigger call in the out of
    // line path.
    JSC::CodeLocationJump inlineJump;
    JSC::CodeLocationLabel stubLabel;

    void initialize(uint32_t pcOffset, JSC::CodeLocationJump inlineJump, JSC::CodeLocationLabel stubLabel) {
        this->pcOffset = pcOffset;
        this->inlineJump = inlineJump;
        this->stubLabel = stubLabel;
    }
};

void
DumpAllProfiles(JSContext *cx);

inline void * bsearch_nmap(NativeMapEntry *nmap, size_t nPairs, size_t bcOff)
{
    size_t lo = 1, hi = nPairs;
    while (1) {
        /* current unsearched space is from lo-1 to hi-1, inclusive. */
        if (lo > hi)
            return NULL; /* not found */
        size_t mid       = (lo + hi) / 2;
        size_t bcOff_mid = nmap[mid-1].bcOff;
        if (bcOff < bcOff_mid) {
            hi = mid-1;
            continue;
        }
        if (bcOff > bcOff_mid) {
            lo = mid+1;
            continue;
        }
        return nmap[mid-1].ncode;
    }
}

static inline bool
IsLowerableFunCallOrApply(jsbytecode *pc)
{
#ifdef JS_MONOIC
    return (*pc == JSOP_FUNCALL && GET_ARGC(pc) >= 1) ||
           (*pc == JSOP_FUNAPPLY && GET_ARGC(pc) == 2);
#else
    return false;
#endif
}

RawShape
GetPICSingleShape(JSContext *cx, JSScript *script, jsbytecode *pc, bool constructing);

static inline void
PurgeCaches(JSScript *script)
{
    for (int constructing = 0; constructing <= 1; constructing++) {
        for (int barriers = 0; barriers <= 1; barriers++) {
            mjit::JITScript *jit = script->getJIT((bool) constructing, (bool) barriers);
            if (jit)
                jit->purgeCaches();
        }
    }
}

} /* namespace mjit */

inline mjit::JITChunk *
VMFrame::chunk()
{
    return jit()->chunk(regs.pc);
}

inline unsigned
VMFrame::chunkIndex()
{
    return jit()->chunkIndex(regs.pc);
}

inline JSScript *
VMFrame::script()
{
    if (regs.inlined())
        return chunk()->inlineFrames()[regs.inlined()->inlineIndex].fun->nonLazyScript();
    return fp()->script();
}

inline jsbytecode *
VMFrame::pc()
{
    if (regs.inlined())
        return script()->code + regs.inlined()->pcOffset;
    return regs.pc;
}

} /* namespace js */

inline void *
JSScript::nativeCodeForPC(bool constructing, jsbytecode *pc)
{
    js::mjit::JITScript *jit = getJIT(constructing, zone()->compileBarriers());
    if (!jit)
        return NULL;
    js::mjit::JITChunk *chunk = jit->chunk(pc);
    if (!chunk)
        return NULL;
    return bsearch_nmap(chunk->nmap(), chunk->nNmapPairs, (size_t)(pc - code));
}

extern "C" void JaegerTrampolineReturn();
extern "C" void JaegerInterpoline();
extern "C" void JaegerInterpolineScripted();

#if defined(_MSC_VER) || defined(_WIN64)
extern "C" void *JaegerThrowpoline(js::VMFrame *vmFrame);
#else
extern "C" void JaegerThrowpoline();
#endif

#if (defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)) || defined(_WIN64)
extern "C" void JaegerInterpolinePatched();
#endif

#endif /* jsjaeger_h__ */

