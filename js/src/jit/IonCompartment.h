/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonCompartment_h
#define jit_IonCompartment_h

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
    StackFrame *osrFrame;

    void *calleeToken;

    Value *maxArgv;
    unsigned maxArgc;
    unsigned numActualArgs;
    unsigned osrNumStackValues;

    RootedObject scopeChain;
    RootedValue result;

    bool constructing;
};

typedef void (*EnterIonCode)(void *code, unsigned argc, Value *argv, StackFrame *fp,
                             CalleeToken calleeToken, JSObject *scopeChain,
                             size_t numStackValues, Value *vp);

class IonBuilder;

typedef Vector<IonBuilder*, 0, SystemAllocPolicy> OffThreadCompilationVector;

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

// Space for optimized stubs. Every IonCompartment has a single
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
    friend class IonRuntime;

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

class IonRuntime
{
    friend class IonCompartment;

    // Executable allocator for all code except the main code in an IonScript.
    // Shared with the runtime.
    JSC::ExecutableAllocator *execAlloc_;

    // Executable allocator used for allocating the main code in an IonScript.
    // All accesses on this allocator must be protected by the runtime's
    // operation callback lock, as the executable memory may be protected()
    // when triggering a callback to force a fault in the Ion code and avoid
    // the neeed for explicit interrupt checks.
    JSC::ExecutableAllocator *ionAlloc_;

    // Shared post-exception-handler tail
    IonCode *exceptionTail_;

    // Shared post-bailout-handler tail.
    IonCode *bailoutTail_;

    // Trampoline for entering JIT code. Contains OSR prologue.
    IonCode *enterJIT_;

    // Trampoline for entering baseline JIT code.
    IonCode *enterBaselineJIT_;

    // Vector mapping frame class sizes to bailout tables.
    Vector<IonCode*, 4, SystemAllocPolicy> bailoutTables_;

    // Generic bailout table; used if the bailout table overflows.
    IonCode *bailoutHandler_;

    // Argument-rectifying thunk, in the case of insufficient arguments passed
    // to a function call site.
    IonCode *argumentsRectifier_;
    void *argumentsRectifierReturnAddr_;

    // Arguments-rectifying thunk which loads |parallelIon| instead of |ion|.
    IonCode *parallelArgumentsRectifier_;

    // Thunk that invalides an (Ion compiled) caller on the Ion stack.
    IonCode *invalidator_;

    // Thunk that calls the GC pre barrier.
    IonCode *valuePreBarrier_;
    IonCode *shapePreBarrier_;

    // Thunk used by the debugger for breakpoint and step mode.
    IonCode *debugTrapHandler_;

    // Map VMFunction addresses to the IonCode of the wrapper.
    typedef WeakCache<const VMFunction *, IonCode *> VMWrapperMap;
    VMWrapperMap *functionWrappers_;

    // Buffer for OSR from baseline to Ion. To avoid holding on to this for
    // too long, it's also freed in IonCompartment::mark and in EnterBaseline
    // (after returning from JIT code).
    uint8_t *osrTempData_;

    // Keep track of memoryregions that are going to be flushed.
    AutoFlushCache *flusher_;

    // Whether all Ion code in the runtime is protected, and will fault if it
    // is accessed.
    bool ionCodeProtected_;

    // If signal handlers are installed, this contains all loop backedges for
    // IonScripts in the runtime.
    InlineList<PatchableBackedge> backedgeList_;

  private:
    IonCode *generateExceptionTailStub(JSContext *cx);
    IonCode *generateBailoutTailStub(JSContext *cx);
    IonCode *generateEnterJIT(JSContext *cx, EnterJitType type);
    IonCode *generateArgumentsRectifier(JSContext *cx, ExecutionMode mode, void **returnAddrOut);
    IonCode *generateBailoutTable(JSContext *cx, uint32_t frameClass);
    IonCode *generateBailoutHandler(JSContext *cx);
    IonCode *generateInvalidator(JSContext *cx);
    IonCode *generatePreBarrier(JSContext *cx, MIRType type);
    IonCode *generateDebugTrapHandler(JSContext *cx);
    IonCode *generateVMWrapper(JSContext *cx, const VMFunction &f);

    JSC::ExecutableAllocator *createIonAlloc(JSContext *cx);

  public:
    IonRuntime();
    ~IonRuntime();
    bool initialize(JSContext *cx);

    uint8_t *allocateOsrTempData(size_t size);
    void freeOsrTempData();

    static void Mark(JSTracer *trc);

    AutoFlushCache *flusher() {
        return flusher_;
    }
    void setFlusher(AutoFlushCache *fl) {
        if (!flusher_ || !fl)
            flusher_ = fl;
    }

    JSC::ExecutableAllocator *getIonAlloc(JSContext *cx) {
        JS_ASSERT(cx->runtime()->currentThreadOwnsOperationCallbackLock());
        return ionAlloc_ ? ionAlloc_ : createIonAlloc(cx);
    }

    JSC::ExecutableAllocator *ionAlloc(JSRuntime *rt) {
        JS_ASSERT(rt->currentThreadOwnsOperationCallbackLock());
        return ionAlloc_;
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

    IonCode *getVMWrapper(const VMFunction &f);
    IonCode *debugTrapHandler(JSContext *cx);

    IonCode *getGenericBailoutHandler() const {
        return bailoutHandler_;
    }

    IonCode *getExceptionTail() const {
        return exceptionTail_;
    }

    IonCode *getBailoutTail() const {
        return bailoutTail_;
    }

    IonCode *getBailoutTable(const FrameSizeClass &frameClass);

    IonCode *getArgumentsRectifier(ExecutionMode mode) const {
        switch (mode) {
          case SequentialExecution: return argumentsRectifier_;
          case ParallelExecution:   return parallelArgumentsRectifier_;
          default:                  MOZ_ASSUME_UNREACHABLE("No such execution mode");
        }
    }

    void *getArgumentsRectifierReturnAddr() const {
        return argumentsRectifierReturnAddr_;
    }

    IonCode *getInvalidationThunk() const {
        return invalidator_;
    }

    EnterIonCode enterIon() const {
        return enterJIT_->as<EnterIonCode>();
    }

    EnterIonCode enterBaseline() const {
        return enterBaselineJIT_->as<EnterIonCode>();
    }

    IonCode *valuePreBarrier() const {
        return valuePreBarrier_;
    }

    IonCode *shapePreBarrier() const {
        return shapePreBarrier_;
    }
};

class IonCompartment
{
    friend class JitActivation;

    // Ion state for the compartment's runtime.
    IonRuntime *rt;

    // Any scripts for which off thread compilation has successfully finished,
    // failed, or been cancelled. All off thread compilations which are started
    // will eventually appear in this list asynchronously. Protected by the
    // runtime's analysis lock.
    OffThreadCompilationVector finishedOffThreadCompilations_;

    // Map ICStub keys to ICStub shared code objects.
    typedef WeakValueCache<uint32_t, ReadBarriered<IonCode> > ICStubCodeMap;
    ICStubCodeMap *stubCodes_;

    // Keep track of offset into various baseline stubs' code at return
    // point from called script.
    void *baselineCallReturnAddr_;
    void *baselineGetPropReturnAddr_;
    void *baselineSetPropReturnAddr_;

    // Allocated space for optimized baseline stubs.
    OptimizedICStubSpace optimizedStubSpace_;

    // Stub to concatenate two strings inline. Note that it can't be
    // stored in IonRuntime because masm.newGCString bakes in zone-specific
    // pointers. This has to be a weak pointer to avoid keeping the whole
    // compartment alive.
    ReadBarriered<IonCode> stringConcatStub_;
    ReadBarriered<IonCode> parallelStringConcatStub_;

    IonCode *generateStringConcatStub(JSContext *cx, ExecutionMode mode);

  public:
    OffThreadCompilationVector &finishedOffThreadCompilations() {
        return finishedOffThreadCompilations_;
    }

    IonCode *getStubCode(uint32_t key) {
        ICStubCodeMap::AddPtr p = stubCodes_->lookupForAdd(key);
        if (p)
            return p->value;
        return NULL;
    }
    bool putStubCode(uint32_t key, Handle<IonCode *> stubCode) {
        // Make sure to do a lookupForAdd(key) and then insert into that slot, because
        // that way if stubCode gets moved due to a GC caused by lookupForAdd, then
        // we still write the correct pointer.
        JS_ASSERT(!stubCodes_->has(key));
        ICStubCodeMap::AddPtr p = stubCodes_->lookupForAdd(key);
        return stubCodes_->add(p, key, stubCode.get());
    }
    void initBaselineCallReturnAddr(void *addr) {
        JS_ASSERT(baselineCallReturnAddr_ == NULL);
        baselineCallReturnAddr_ = addr;
    }
    void *baselineCallReturnAddr() {
        JS_ASSERT(baselineCallReturnAddr_ != NULL);
        return baselineCallReturnAddr_;
    }
    void initBaselineGetPropReturnAddr(void *addr) {
        JS_ASSERT(baselineGetPropReturnAddr_ == NULL);
        baselineGetPropReturnAddr_ = addr;
    }
    void *baselineGetPropReturnAddr() {
        JS_ASSERT(baselineGetPropReturnAddr_ != NULL);
        return baselineGetPropReturnAddr_;
    }
    void initBaselineSetPropReturnAddr(void *addr) {
        JS_ASSERT(baselineSetPropReturnAddr_ == NULL);
        baselineSetPropReturnAddr_ = addr;
    }
    void *baselineSetPropReturnAddr() {
        JS_ASSERT(baselineSetPropReturnAddr_ != NULL);
        return baselineSetPropReturnAddr_;
    }

    void toggleBaselineStubBarriers(bool enabled);

    JSC::ExecutableAllocator *createIonAlloc();

  public:
    IonCompartment(IonRuntime *rt);
    ~IonCompartment();

    bool initialize(JSContext *cx);

    // Initialize code stubs only used by Ion, not Baseline.
    bool ensureIonStubsExist(JSContext *cx);

    void mark(JSTracer *trc, JSCompartment *compartment);
    void sweep(FreeOp *fop);

    JSC::ExecutableAllocator *execAlloc() {
        return rt->execAlloc_;
    }

    IonCode *stringConcatStub(ExecutionMode mode) {
        switch (mode) {
          case SequentialExecution: return stringConcatStub_;
          case ParallelExecution:   return parallelStringConcatStub_;
          default:                  MOZ_ASSUME_UNREACHABLE("No such execution mode");
        }
    }

    OptimizedICStubSpace *optimizedStubSpace() {
        return &optimizedStubSpace_;
    }
};

// Called from JSCompartment::discardJitCode().
void InvalidateAll(FreeOp *fop, JS::Zone *zone);
void FinishInvalidation(FreeOp *fop, JSScript *script);

// On windows systems, really large frames need to be incrementally touched.
// The following constant defines the minimum increment of the touch.
#ifdef XP_WIN
const unsigned WINDOWS_BIG_FRAME_TOUCH_INCREMENT = 4096 - 1;
#endif

} // namespace jit
} // namespace js

#endif // JS_ION

#endif /* jit_IonCompartment_h */
