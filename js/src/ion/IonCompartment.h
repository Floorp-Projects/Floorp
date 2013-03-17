/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
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

namespace js {
namespace ion {

class FrameSizeClass;

typedef void (*EnterIonCode)(void *code, int argc, Value *argv, StackFrame *fp,
                             CalleeToken calleeToken, Value *vp);

class IonActivation;
class IonBuilder;

typedef Vector<IonBuilder*, 0, SystemAllocPolicy> OffThreadCompilationVector;

class IonRuntime
{
    friend class IonCompartment;

    // Executable allocator.
    JSC::ExecutableAllocator *execAlloc_;

    // Trampoline for entering JIT code. Contains OSR prologue.
    IonCode *enterJIT_;

    // Vector mapping frame class sizes to bailout tables.
    Vector<IonCode*, 4, SystemAllocPolicy> bailoutTables_;

    // Generic bailout table; used if the bailout table overflows.
    IonCode *bailoutHandler_;

    // Argument-rectifying thunk, in the case of insufficient arguments passed
    // to a function call site. Pads with |undefined|.
    IonCode *argumentsRectifier_;

    // Thunk that invalides an (Ion compiled) caller on the Ion stack.
    IonCode *invalidator_;

    // Thunk that calls the GC pre barrier.
    IonCode *valuePreBarrier_;
    IonCode *shapePreBarrier_;

    // Map VMFunction addresses to the IonCode of the wrapper.
    typedef WeakCache<const VMFunction *, IonCode *> VMWrapperMap;
    VMWrapperMap *functionWrappers_;

    // Keep track of memoryregions that are going to be flushed.
    AutoFlushCache *flusher_;

  private:
    IonCode *generateEnterJIT(JSContext *cx);
    IonCode *generateArgumentsRectifier(JSContext *cx);
    IonCode *generateBailoutTable(JSContext *cx, uint32_t frameClass);
    IonCode *generateBailoutHandler(JSContext *cx);
    IonCode *generateInvalidator(JSContext *cx);
    IonCode *generatePreBarrier(JSContext *cx, MIRType type);
    IonCode *generateVMWrapper(JSContext *cx, const VMFunction &f);

  public:
    IonRuntime();
    ~IonRuntime();
    bool initialize(JSContext *cx);

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

  public:
    IonCode *getVMWrapper(const VMFunction &f);

    OffThreadCompilationVector &finishedOffThreadCompilations() {
        return finishedOffThreadCompilations_;
    }

  public:
    IonCompartment(IonRuntime *rt);

    bool initialize(JSContext *cx);

    void mark(JSTracer *trc, JSCompartment *compartment);
    void sweep(FreeOp *fop);

    JSC::ExecutableAllocator *execAlloc() {
        return rt->execAlloc_;
    }

    IonCode *getGenericBailoutHandler() {
        return rt->bailoutHandler_;
    }

    IonCode *getBailoutTable(const FrameSizeClass &frameClass);

    IonCode *getArgumentsRectifier() {
        return rt->argumentsRectifier_;
    }

    IonCode *getInvalidationThunk() {
        return rt->invalidator_;
    }

    EnterIonCode enterJIT() {
        return rt->enterJIT_->as<EnterIonCode>();
    }

    IonCode *valuePreBarrier() {
        return rt->valuePreBarrier_;
    }

    IonCode *shapePreBarrier() {
        return rt->shapePreBarrier_;
    }

    AutoFlushCache *flusher() {
        return rt->flusher();
    }
    void setFlusher(AutoFlushCache *fl) {
        rt->setFlusher(fl);
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
    // to communicate the calling pc for StackIter.
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
void FinishInvalidation(FreeOp *fop, RawScript script);

} // namespace ion
} // namespace js

#endif // jsion_ion_compartment_h__

