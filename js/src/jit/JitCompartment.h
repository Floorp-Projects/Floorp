/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_JitCompartment_h
#define jit_JitCompartment_h

#ifdef JS_ION

#include "mozilla/MemoryReporting.h"

#include "jsweakcache.h"

#include "jit/CompileInfo.h"
#include "jit/IonCode.h"
#include "jit/IonFrames.h"
#include "jit/shared/Assembler-shared.h"
#include "js/Value.h"
#include "vm/Stack.h"

namespace js {
namespace jit {

class FrameSizeClass;

enum EnterJitType {
    EnterJitBaseline = 0,
    EnterJitOptimized = 1
};

struct EnterJitData
{
    explicit EnterJitData(JSContext *cx)
      : scopeChain(cx),
        result(cx)
    {}

    uint8_t *jitcode;
    InterpreterFrame *osrFrame;

    void *calleeToken;

    Value *maxArgv;
    unsigned maxArgc;
    unsigned numActualArgs;
    unsigned osrNumStackValues;

    RootedObject scopeChain;
    RootedValue result;

    bool constructing;
};

typedef void (*EnterJitCode)(void *code, unsigned argc, Value *argv, InterpreterFrame *fp,
                             CalleeToken calleeToken, JSObject *scopeChain,
                             size_t numStackValues, Value *vp);

class IonBuilder;

// ICStubSpace is an abstraction for allocation policy and storage for stub data.
// There are two kinds of stubs: optimized stubs and fallback stubs (the latter
// also includes stubs that can make non-tail calls that can GC).
//
// Optimized stubs are allocated per-compartment and are always purged when
// JIT-code is discarded. Fallback stubs are allocated per BaselineScript and
// are only destroyed when the BaselineScript is destroyed.
class ICStubSpace
{
  protected:
    LifoAlloc allocator_;

    explicit ICStubSpace(size_t chunkSize)
      : allocator_(chunkSize)
    {}

  public:
    inline void *alloc(size_t size) {
        return allocator_.alloc(size);
    }

    JS_DECLARE_NEW_METHODS(allocate, alloc, inline)

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
        return allocator_.sizeOfExcludingThis(mallocSizeOf);
    }
};

// Space for optimized stubs. Every JitCompartment has a single
// OptimizedICStubSpace.
struct OptimizedICStubSpace : public ICStubSpace
{
    static const size_t STUB_DEFAULT_CHUNK_SIZE = 4 * 1024;

  public:
    OptimizedICStubSpace()
      : ICStubSpace(STUB_DEFAULT_CHUNK_SIZE)
    {}

    void free() {
        allocator_.freeAll();
    }
};

// Space for fallback stubs. Every BaselineScript has a
// FallbackICStubSpace.
struct FallbackICStubSpace : public ICStubSpace
{
    static const size_t STUB_DEFAULT_CHUNK_SIZE = 256;

  public:
    FallbackICStubSpace()
      : ICStubSpace(STUB_DEFAULT_CHUNK_SIZE)
    {}

    inline void adoptFrom(FallbackICStubSpace *other) {
        allocator_.steal(&(other->allocator_));
    }
};

// Information about a loop backedge in the runtime, which can be set to
// point to either the loop header or to an OOL interrupt checking stub,
// if signal handlers are being used to implement interrupts.
class PatchableBackedge : public InlineListNode<PatchableBackedge>
{
    friend class JitRuntime;

    CodeLocationJump backedge;
    CodeLocationLabel loopHeader;
    CodeLocationLabel interruptCheck;

  public:
    PatchableBackedge(CodeLocationJump backedge,
                      CodeLocationLabel loopHeader,
                      CodeLocationLabel interruptCheck)
      : backedge(backedge), loopHeader(loopHeader), interruptCheck(interruptCheck)
    {}
};

class JitRuntime
{
    friend class JitCompartment;

    // Executable allocator for all code except the main code in an IonScript.
    // Shared with the runtime.
    JSC::ExecutableAllocator *execAlloc_;

    // Executable allocator used for allocating the main code in an IonScript.
    // All accesses on this allocator must be protected by the runtime's
    // interrupt lock, as the executable memory may be protected() when
    // requesting an interrupt to force a fault in the Ion code and avoid the
    // need for explicit interrupt checks.
    JSC::ExecutableAllocator *ionAlloc_;

    // Shared post-exception-handler tail
    JitCode *exceptionTail_;

    // Shared post-bailout-handler tail.
    JitCode *bailoutTail_;

    // Trampoline for entering JIT code. Contains OSR prologue.
    JitCode *enterJIT_;

    // Trampoline for entering baseline JIT code.
    JitCode *enterBaselineJIT_;

    // Vector mapping frame class sizes to bailout tables.
    Vector<JitCode*, 4, SystemAllocPolicy> bailoutTables_;

    // Generic bailout table; used if the bailout table overflows.
    JitCode *bailoutHandler_;

    // Bailout handler for parallel execution.
    JitCode *parallelBailoutHandler_;

    // Argument-rectifying thunk, in the case of insufficient arguments passed
    // to a function call site.
    JitCode *argumentsRectifier_;
    void *argumentsRectifierReturnAddr_;

    // Arguments-rectifying thunk which loads |parallelIon| instead of |ion|.
    JitCode *parallelArgumentsRectifier_;

    // Thunk that invalides an (Ion compiled) caller on the Ion stack.
    JitCode *invalidator_;

    // Thunk that calls the GC pre barrier.
    JitCode *valuePreBarrier_;
    JitCode *shapePreBarrier_;

    // Thunk to call malloc/free.
    JitCode *mallocStub_;
    JitCode *freeStub_;

    // Thunk used by the debugger for breakpoint and step mode.
    JitCode *debugTrapHandler_;

    // Stub used to inline the ForkJoinGetSlice intrinsic.
    JitCode *forkJoinGetSliceStub_;

    // Thunk used to fix up on-stack recompile of baseline scripts.
    JitCode *baselineDebugModeOSRHandler_;
    void *baselineDebugModeOSRHandlerNoFrameRegPopAddr_;

    // Map VMFunction addresses to the JitCode of the wrapper.
    typedef WeakCache<const VMFunction *, JitCode *> VMWrapperMap;
    VMWrapperMap *functionWrappers_;

    // Buffer for OSR from baseline to Ion. To avoid holding on to this for
    // too long, it's also freed in JitCompartment::mark and in EnterBaseline
    // (after returning from JIT code).
    uint8_t *osrTempData_;

    // Whether all Ion code in the runtime is protected, and will fault if it
    // is accessed.
    bool ionCodeProtected_;

    // If signal handlers are installed, this contains all loop backedges for
    // IonScripts in the runtime.
    InlineList<PatchableBackedge> backedgeList_;

    // In certain cases, we want to optimize certain opcodes to typed instructions,
    // to avoid carrying an extra register to feed into an unbox. Unfortunately,
    // that's not always possible. For example, a GetPropertyCacheT could return a
    // typed double, but if it takes its out-of-line path, it could return an
    // object, and trigger invalidation. The invalidation bailout will consider the
    // return value to be a double, and create a garbage Value.
    //
    // To allow the GetPropertyCacheT optimization, we allow the ability for
    // GetPropertyCache to override the return value at the top of the stack - the
    // value that will be temporarily corrupt. This special override value is set
    // only in callVM() targets that are about to return *and* have invalidated
    // their callee.
    js::Value ionReturnOverride_;

  private:
    JitCode *generateExceptionTailStub(JSContext *cx);
    JitCode *generateBailoutTailStub(JSContext *cx);
    JitCode *generateEnterJIT(JSContext *cx, EnterJitType type);
    JitCode *generateArgumentsRectifier(JSContext *cx, ExecutionMode mode, void **returnAddrOut);
    JitCode *generateBailoutTable(JSContext *cx, uint32_t frameClass);
    JitCode *generateBailoutHandler(JSContext *cx, ExecutionMode mode);
    JitCode *generateInvalidator(JSContext *cx);
    JitCode *generatePreBarrier(JSContext *cx, MIRType type);
    JitCode *generateMallocStub(JSContext *cx);
    JitCode *generateFreeStub(JSContext *cx);
    JitCode *generateDebugTrapHandler(JSContext *cx);
    JitCode *generateForkJoinGetSliceStub(JSContext *cx);
    JitCode *generateBaselineDebugModeOSRHandler(JSContext *cx, uint32_t *noFrameRegPopOffsetOut);
    JitCode *generateVMWrapper(JSContext *cx, const VMFunction &f);

    JSC::ExecutableAllocator *createIonAlloc(JSContext *cx);

  public:
    JitRuntime();
    ~JitRuntime();
    bool initialize(JSContext *cx);

    uint8_t *allocateOsrTempData(size_t size);
    void freeOsrTempData();

    static void Mark(JSTracer *trc);

    JSC::ExecutableAllocator *execAlloc() const {
        return execAlloc_;
    }

    JSC::ExecutableAllocator *getIonAlloc(JSContext *cx) {
        JS_ASSERT(cx->runtime()->currentThreadOwnsInterruptLock());
        return ionAlloc_ ? ionAlloc_ : createIonAlloc(cx);
    }

    JSC::ExecutableAllocator *ionAlloc(JSRuntime *rt) {
        JS_ASSERT(rt->currentThreadOwnsInterruptLock());
        return ionAlloc_;
    }

    bool hasIonAlloc() const {
        return !!ionAlloc_;
    }

    bool ionCodeProtected() {
        return ionCodeProtected_;
    }

    void addPatchableBackedge(PatchableBackedge *backedge) {
        backedgeList_.pushFront(backedge);
    }
    void removePatchableBackedge(PatchableBackedge *backedge) {
        backedgeList_.remove(backedge);
    }

    enum BackedgeTarget {
        BackedgeLoopHeader,
        BackedgeInterruptCheck
    };

    void ensureIonCodeProtected(JSRuntime *rt);
    void ensureIonCodeAccessible(JSRuntime *rt);
    void patchIonBackedges(JSRuntime *rt, BackedgeTarget target);

    bool handleAccessViolation(JSRuntime *rt, void *faultingAddress);

    JitCode *getVMWrapper(const VMFunction &f) const;
    JitCode *debugTrapHandler(JSContext *cx);
    JitCode *getBaselineDebugModeOSRHandler(JSContext *cx);
    void *getBaselineDebugModeOSRHandlerAddress(JSContext *cx, bool popFrameReg);

    JitCode *getGenericBailoutHandler(ExecutionMode mode) const {
        switch (mode) {
          case SequentialExecution: return bailoutHandler_;
          case ParallelExecution:   return parallelBailoutHandler_;
          default:                  MOZ_ASSUME_UNREACHABLE("No such execution mode");
        }
    }

    JitCode *getExceptionTail() const {
        return exceptionTail_;
    }

    JitCode *getBailoutTail() const {
        return bailoutTail_;
    }

    JitCode *getBailoutTable(const FrameSizeClass &frameClass) const;

    JitCode *getArgumentsRectifier(ExecutionMode mode) const {
        switch (mode) {
          case SequentialExecution: return argumentsRectifier_;
          case ParallelExecution:   return parallelArgumentsRectifier_;
          default:                  MOZ_ASSUME_UNREACHABLE("No such execution mode");
        }
    }

    void *getArgumentsRectifierReturnAddr() const {
        return argumentsRectifierReturnAddr_;
    }

    JitCode *getInvalidationThunk() const {
        return invalidator_;
    }

    EnterJitCode enterIon() const {
        return enterJIT_->as<EnterJitCode>();
    }

    EnterJitCode enterBaseline() const {
        return enterBaselineJIT_->as<EnterJitCode>();
    }

    JitCode *valuePreBarrier() const {
        return valuePreBarrier_;
    }

    JitCode *shapePreBarrier() const {
        return shapePreBarrier_;
    }

    JitCode *mallocStub() const {
        return mallocStub_;
    }

    JitCode *freeStub() const {
        return freeStub_;
    }

    bool ensureForkJoinGetSliceStubExists(JSContext *cx);
    JitCode *forkJoinGetSliceStub() const {
        return forkJoinGetSliceStub_;
    }

    bool hasIonReturnOverride() const {
        return !ionReturnOverride_.isMagic(JS_ARG_POISON);
    }
    js::Value takeIonReturnOverride() {
        js::Value v = ionReturnOverride_;
        ionReturnOverride_ = js::MagicValue(JS_ARG_POISON);
        return v;
    }
    void setIonReturnOverride(const js::Value &v) {
        JS_ASSERT(!hasIonReturnOverride());
        JS_ASSERT(!v.isMagic());
        ionReturnOverride_ = v;
    }
};

class JitZone
{
    // Allocated space for optimized baseline stubs.
    OptimizedICStubSpace optimizedStubSpace_;

  public:
    OptimizedICStubSpace *optimizedStubSpace() {
        return &optimizedStubSpace_;
    }
};

class JitCompartment
{
    friend class JitActivation;

    // Map ICStub keys to ICStub shared code objects.
    typedef WeakValueCache<uint32_t, ReadBarrieredJitCode> ICStubCodeMap;
    ICStubCodeMap *stubCodes_;

    // Keep track of offset into various baseline stubs' code at return
    // point from called script.
    void *baselineCallReturnAddr_;
    void *baselineGetPropReturnAddr_;
    void *baselineSetPropReturnAddr_;

    // Stub to concatenate two strings inline. Note that it can't be
    // stored in JitRuntime because masm.newGCString bakes in zone-specific
    // pointers. These are weak pointers, but are not declared as ReadBarriered
    // since they are only read from during Ion compilation, which may occur
    // off thread and whose barriers are captured during CodeGenerator::link.
    JitCode *stringConcatStub_;
    JitCode *parallelStringConcatStub_;

    // Set of JSScripts invoked by ForkJoin (i.e. the entry script). These
    // scripts are marked if their respective parallel IonScripts' age is less
    // than a certain amount. See IonScript::parallelAge_.
    typedef HashSet<PreBarrieredScript> ScriptSet;
    ScriptSet *activeParallelEntryScripts_;

    JitCode *generateStringConcatStub(JSContext *cx, ExecutionMode mode);

  public:
    JitCode *getStubCode(uint32_t key) {
        ICStubCodeMap::AddPtr p = stubCodes_->lookupForAdd(key);
        if (p)
            return p->value();
        return nullptr;
    }
    bool putStubCode(uint32_t key, Handle<JitCode *> stubCode) {
        // Make sure to do a lookupForAdd(key) and then insert into that slot, because
        // that way if stubCode gets moved due to a GC caused by lookupForAdd, then
        // we still write the correct pointer.
        JS_ASSERT(!stubCodes_->has(key));
        ICStubCodeMap::AddPtr p = stubCodes_->lookupForAdd(key);
        return stubCodes_->add(p, key, stubCode.get());
    }
    void initBaselineCallReturnAddr(void *addr) {
        JS_ASSERT(baselineCallReturnAddr_ == nullptr);
        baselineCallReturnAddr_ = addr;
    }
    void *baselineCallReturnAddr() {
        JS_ASSERT(baselineCallReturnAddr_ != nullptr);
        return baselineCallReturnAddr_;
    }
    void initBaselineGetPropReturnAddr(void *addr) {
        JS_ASSERT(baselineGetPropReturnAddr_ == nullptr);
        baselineGetPropReturnAddr_ = addr;
    }
    void *baselineGetPropReturnAddr() {
        JS_ASSERT(baselineGetPropReturnAddr_ != nullptr);
        return baselineGetPropReturnAddr_;
    }
    void initBaselineSetPropReturnAddr(void *addr) {
        JS_ASSERT(baselineSetPropReturnAddr_ == nullptr);
        baselineSetPropReturnAddr_ = addr;
    }
    void *baselineSetPropReturnAddr() {
        JS_ASSERT(baselineSetPropReturnAddr_ != nullptr);
        return baselineSetPropReturnAddr_;
    }

    bool notifyOfActiveParallelEntryScript(JSContext *cx, HandleScript script);
    bool hasRecentParallelActivity() const;

    void toggleBaselineStubBarriers(bool enabled);

    JSC::ExecutableAllocator *createIonAlloc();

  public:
    JitCompartment();
    ~JitCompartment();

    bool initialize(JSContext *cx);

    // Initialize code stubs only used by Ion, not Baseline.
    bool ensureIonStubsExist(JSContext *cx);

    void mark(JSTracer *trc, JSCompartment *compartment);
    void sweep(FreeOp *fop, JSCompartment *compartment);

    JitCode *stringConcatStubNoBarrier(ExecutionMode mode) const {
        switch (mode) {
          case SequentialExecution: return stringConcatStub_;
          case ParallelExecution:   return parallelStringConcatStub_;
          default:                  MOZ_ASSUME_UNREACHABLE("No such execution mode");
        }
    }
};

// Called from JSCompartment::discardJitCode().
void InvalidateAll(FreeOp *fop, JS::Zone *zone);
template <ExecutionMode mode>
void FinishInvalidation(FreeOp *fop, JSScript *script);

// On windows systems, really large frames need to be incrementally touched.
// The following constant defines the minimum increment of the touch.
#ifdef XP_WIN
const unsigned WINDOWS_BIG_FRAME_TOUCH_INCREMENT = 4096 - 1;
#endif

} // namespace jit
} // namespace js

#endif // JS_ION

#endif /* jit_JitCompartment_h */
