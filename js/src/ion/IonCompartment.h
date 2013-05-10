/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_ion_compartment_h__
#define jsion_ion_compartment_h__

#include "IonCode.h"
#include "jsweakcache.h"
#include "js/Value.h"
#include "vm/Stack.h"
#include "IonFrames.h"
#include "CompileInfo.h"

namespace js {
namespace ion {

class FrameSizeClass;

enum EnterJitType {
    EnterJitBaseline = 0,
    EnterJitOptimized = 1
};

typedef void (*EnterIonCode)(void *code, int argc, Value *argv, StackFrame *fp,
                             CalleeToken calleeToken, JSObject *scopeChain,
                             size_t numStackValues, Value *vp);

class IonActivation;
class IonBuilder;

typedef Vector<IonBuilder*, 0, SystemAllocPolicy> OffThreadCompilationVector;

// ICStubSpace is an abstraction for allocation policy and storage for stub data.
// There are two kinds of stubs: optimized stubs and fallback stubs (the latter
// also includes stubs that can make non-tail calls that can GC).
//
// Optimized stubs are allocated per-compartment and are always purged when
// JIT-code is discarded. Fallback stubs are allocated per BaselineScript and
// are only destroyed when the BaselineScript is destroyed.
struct ICStubSpace
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

    size_t sizeOfExcludingThis(JSMallocSizeOfFun mallocSizeOf) const {
        return allocator_.sizeOfExcludingThis(mallocSizeOf);
    }
};

// Space for optimized stubs. Every IonCompartment has a single
// OptimizedICStubSpace.
struct OptimizedICStubSpace : public ICStubSpace
{
    const static size_t STUB_DEFAULT_CHUNK_SIZE = 4 * 1024;

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
    const static size_t STUB_DEFAULT_CHUNK_SIZE = 256;

  public:
    FallbackICStubSpace()
      : ICStubSpace(STUB_DEFAULT_CHUNK_SIZE)
    {}

    inline void adoptFrom(FallbackICStubSpace *other) {
        allocator_.steal(&(other->allocator_));
    }
};

class IonRuntime
{
    friend class IonCompartment;

    // Executable allocator.
    JSC::ExecutableAllocator *execAlloc_;

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

  private:
    IonCode *generateEnterJIT(JSContext *cx, EnterJitType type);
    IonCode *generateArgumentsRectifier(JSContext *cx, ExecutionMode mode, void **returnAddrOut);
    IonCode *generateBailoutTable(JSContext *cx, uint32_t frameClass);
    IonCode *generateBailoutHandler(JSContext *cx);
    IonCode *generateInvalidator(JSContext *cx);
    IonCode *generatePreBarrier(JSContext *cx, MIRType type);
    IonCode *generateDebugTrapHandler(JSContext *cx);
    IonCode *generateVMWrapper(JSContext *cx, const VMFunction &f);

    IonCode *debugTrapHandler(JSContext *cx);

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
};

class IonCompartment
{
    friend class IonActivation;

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

    // Keep track of offset into baseline ICCall_Scripted stub's code at return
    // point from called script.
    void *baselineCallReturnAddr_;

    // Allocated space for optimized baseline stubs.
    OptimizedICStubSpace optimizedStubSpace_;

    // Stub to concatenate two strings inline. Note that it can't be
    // stored in IonRuntime because masm.newGCString bakes in zone-specific
    // pointers. This has to be a weak pointer to avoid keeping the whole
    // compartment alive.
    ReadBarriered<IonCode> stringConcatStub_;

    IonCode *generateStringConcatStub(JSContext *cx);

  public:
    IonCode *getVMWrapper(const VMFunction &f);

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

    void toggleBaselineStubBarriers(bool enabled);

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

    IonCode *getGenericBailoutHandler() {
        return rt->bailoutHandler_;
    }

    IonCode *getBailoutTable(const FrameSizeClass &frameClass);

    IonCode *getArgumentsRectifier(ExecutionMode mode) {
        switch (mode) {
          case SequentialExecution: return rt->argumentsRectifier_;
          case ParallelExecution:   return rt->parallelArgumentsRectifier_;
          default:                  JS_NOT_REACHED("No such execution mode");
        }
    }

    void *getArgumentsRectifierReturnAddr() {
        return rt->argumentsRectifierReturnAddr_;
    }

    IonCode *getInvalidationThunk() {
        return rt->invalidator_;
    }

    EnterIonCode enterJIT() {
        return rt->enterJIT_->as<EnterIonCode>();
    }

    EnterIonCode enterBaselineJIT() {
        return rt->enterBaselineJIT_->as<EnterIonCode>();
    }

    IonCode *valuePreBarrier() {
        return rt->valuePreBarrier_;
    }

    IonCode *shapePreBarrier() {
        return rt->shapePreBarrier_;
    }

    IonCode *debugTrapHandler(JSContext *cx) {
        return rt->debugTrapHandler(cx);
    }

    IonCode *stringConcatStub() {
        return stringConcatStub_;
    }

    AutoFlushCache *flusher() {
        return rt->flusher();
    }
    void setFlusher(AutoFlushCache *fl) {
        rt->setFlusher(fl);
    }
    OptimizedICStubSpace *optimizedStubSpace() {
        return &optimizedStubSpace_;
    }
};

class BailoutClosure;

class IonActivation
{
  private:
    JSContext *cx_;
    JSCompartment *compartment_;
    IonActivation *prev_;
    StackFrame *entryfp_;
    BailoutClosure *bailout_;
    uint8_t *prevIonTop_;
    JSContext *prevIonJSContext_;

    // When creating an activation without a StackFrame, this field is used
    // to communicate the calling pc for ScriptFrameIter.
    jsbytecode *prevpc_;

  public:
    IonActivation(JSContext *cx, StackFrame *fp);
    ~IonActivation();

    StackFrame *entryfp() const {
        return entryfp_;
    }
    IonActivation *prev() const {
        return prev_;
    }
    uint8_t *prevIonTop() const {
        return prevIonTop_;
    }
    jsbytecode *prevpc() const {
        JS_ASSERT_IF(entryfp_, entryfp_->callingIntoIon());
        return prevpc_;
    }
    void setEntryFp(StackFrame *fp) {
        JS_ASSERT_IF(fp, !entryfp_);
        entryfp_ = fp;
    }
    void setPrevPc(jsbytecode *pc) {
        JS_ASSERT_IF(pc, !prevpc_);
        prevpc_ = pc;
    }
    void setBailout(BailoutClosure *bailout) {
        JS_ASSERT(!bailout_);
        bailout_ = bailout;
    }
    BailoutClosure *maybeTakeBailout() {
        BailoutClosure *br = bailout_;
        bailout_ = NULL;
        return br;
    }
    BailoutClosure *takeBailout() {
        JS_ASSERT(bailout_);
        return maybeTakeBailout();
    }
    BailoutClosure *bailout() const {
        JS_ASSERT(bailout_);
        return bailout_;
    }
    JSCompartment *compartment() const {
        return compartment_;
    }
    bool empty() const {
        // If we have an entryfp, this activation is active. However, if
        // FastInvoke is used, entryfp may be NULL and a non-NULL prevpc
        // indicates this activation is not empty.
        return !entryfp_ && !prevpc_;
    }

    static inline size_t offsetOfPrevPc() {
        return offsetof(IonActivation, prevpc_);
    }
    static inline size_t offsetOfEntryFp() {
        return offsetof(IonActivation, entryfp_);
    }
};

// Called from JSCompartment::discardJitCode().
void InvalidateAll(FreeOp *fop, JS::Zone *zone);
void FinishInvalidation(FreeOp *fop, JSScript *script);

} // namespace ion
} // namespace js

#endif // jsion_ion_compartment_h__

