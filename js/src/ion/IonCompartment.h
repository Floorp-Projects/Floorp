/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_ion_compartment_h__
#define jsion_ion_compartment_h__

#include "IonCode.h"
#include "jsval.h"
#include "jsweakcache.h"
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

class IonCompartment
{
    typedef WeakCache<const VMFunction *, ReadBarriered<IonCode> > VMWrapperMap;

    friend class IonActivation;

    // Executable allocator (owned by the runtime).
    JSC::ExecutableAllocator *execAlloc_;

    // Trampoline for entering JIT code. Contains OSR prologue.
    ReadBarriered<IonCode> enterJIT_;

    // Vector mapping frame class sizes to bailout tables.
    Vector<ReadBarriered<IonCode>, 4, SystemAllocPolicy> bailoutTables_;

    // Generic bailout table; used if the bailout table overflows.
    ReadBarriered<IonCode> bailoutHandler_;

    // Argument-rectifying thunk, in the case of insufficient arguments passed
    // to a function call site. Pads with |undefined|.
    ReadBarriered<IonCode> argumentsRectifier_;

    // Thunk that invalides an (Ion compiled) caller on the Ion stack.
    ReadBarriered<IonCode> invalidator_;

    // Thunk that calls the GC pre barrier.
    ReadBarriered<IonCode> preBarrier_;

    // Map VMFunction addresses to the IonCode of the wrapper.
    VMWrapperMap *functionWrappers_;

    // Any scripts for which off thread compilation has successfully finished,
    // failed, or been cancelled. All off thread compilations which are started
    // will eventually appear in this list asynchronously. Protected by the
    // runtime's analysis lock.
    OffThreadCompilationVector finishedOffThreadCompilations_;

    // Keep track of memoryregions that are going to be flushed.
    AutoFlushCache *flusher_;

  private:
    IonCode *generateEnterJIT(JSContext *cx);
    IonCode *generateReturnError(JSContext *cx);
    IonCode *generateArgumentsRectifier(JSContext *cx);
    IonCode *generateBailoutTable(JSContext *cx, uint32 frameClass);
    IonCode *generateBailoutHandler(JSContext *cx);
    IonCode *generateInvalidator(JSContext *cx);
    IonCode *generatePreBarrier(JSContext *cx);

  public:
    IonCode *generateVMWrapper(JSContext *cx, const VMFunction &f);

    OffThreadCompilationVector &finishedOffThreadCompilations() {
        return finishedOffThreadCompilations_;
    }

  public:
    bool initialize(JSContext *cx);
    IonCompartment();
    ~IonCompartment();

    void mark(JSTracer *trc, JSCompartment *compartment);
    void sweep(FreeOp *fop);

    JSC::ExecutableAllocator *execAlloc() {
        return execAlloc_;
    }

    IonCode *getBailoutTable(JSContext *cx, const FrameSizeClass &frameClass);
    IonCode *getGenericBailoutHandler(JSContext *cx) {
        if (!bailoutHandler_) {
            bailoutHandler_ = generateBailoutHandler(cx);
            if (!bailoutHandler_)
                return NULL;
        }
        return bailoutHandler_;
    }

    // Infallible; does not generate a table.
    IonCode *getBailoutTable(const FrameSizeClass &frameClass);

    // Fallible; generates a thunk and returns the target.
    IonCode *getArgumentsRectifier(JSContext *cx) {
        if (!argumentsRectifier_) {
            argumentsRectifier_ = generateArgumentsRectifier(cx);
            if (!argumentsRectifier_)
                return NULL;
        }
        return argumentsRectifier_;
    }
    IonCode **getArgumentsRectifierAddr() {
        return argumentsRectifier_.unsafeGet();
    }

    IonCode *getOrCreateInvalidationThunk(JSContext *cx) {
        if (!invalidator_) {
            invalidator_ = generateInvalidator(cx);
            if (!invalidator_)
                return NULL;
        }
        return invalidator_;
    }
    IonCode **getInvalidationThunkAddr() {
        return invalidator_.unsafeGet();
    }

    EnterIonCode enterJITInfallible() {
        JS_ASSERT(enterJIT_);
        return enterJIT_.get()->as<EnterIonCode>();
    }

    EnterIonCode enterJIT(JSContext *cx) {
        if (!enterJIT_) {
            enterJIT_ = generateEnterJIT(cx);
            if (!enterJIT_)
                return NULL;
        }
        return enterJIT_.get()->as<EnterIonCode>();
    }

    IonCode *preBarrier(JSContext *cx) {
        if (!preBarrier_) {
            preBarrier_ = generatePreBarrier(cx);
            if (!preBarrier_)
                return NULL;
        }
        return preBarrier_;
    }
    AutoFlushCache *flusher() {
        return flusher_;
    }
    void setFlusher(AutoFlushCache *fl) {
        if (!flusher_ || !fl)
            flusher_ = fl;
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
    uint8 *prevIonTop_;
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
    uint8 *prevIonTop() const {
        return prevIonTop_;
    }
    jsbytecode *prevpc() const {
        JS_ASSERT(entryfp_->callingIntoIon());
        return prevpc_;
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

    static inline size_t offsetOfPrevPc() {
        return offsetof(IonActivation, prevpc_);
    }
    static inline size_t offsetOfEntryFp() {
        return offsetof(IonActivation, entryfp_);
    }
};

// Called from JSCompartment::discardJitCode().
void InvalidateAll(FreeOp *fop, JSCompartment *comp);
void FinishInvalidation(FreeOp *fop, JSScript *script);

} // namespace ion
} // namespace js

#endif // jsion_ion_compartment_h__

