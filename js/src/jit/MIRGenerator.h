/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_MIRGenerator_h
#define jit_MIRGenerator_h

// This file declares the data structures used to build a control-flow graph
// containing MIR.

#include <stdarg.h>

#include "jscntxt.h"
#include "jscompartment.h"

#include "jit/CompileInfo.h"
#include "jit/IonAllocPolicy.h"
#include "jit/IonCompartment.h"
#if defined(JS_ION_PERF)
# include "jit/PerfSpewer.h"
#endif
#include "jit/RegisterSets.h"

namespace js {
namespace jit {

class MBasicBlock;
class MIRGraph;
class MStart;

struct AsmJSGlobalAccess
{
    unsigned offset;
    unsigned globalDataOffset;

    AsmJSGlobalAccess(unsigned offset, unsigned globalDataOffset)
      : offset(offset), globalDataOffset(globalDataOffset)
    {}
};

typedef Vector<AsmJSGlobalAccess, 0, IonAllocPolicy> AsmJSGlobalAccessVector;

class MIRGenerator
{
  public:
    MIRGenerator(JSCompartment *compartment, TempAllocator *temp, MIRGraph *graph, CompileInfo *info);

    TempAllocator &temp() {
        return *temp_;
    }
    MIRGraph &graph() {
        return *graph_;
    }
    bool ensureBallast() {
        return temp().ensureBallast();
    }
    IonCompartment *ionCompartment() const {
        return compartment->ionCompartment();
    }
    IonRuntime *ionRuntime() const {
        return GetIonContext()->runtime->ionRuntime();
    }
    CompileInfo &info() {
        return *info_;
    }

    template <typename T>
    T * allocate(size_t count = 1) {
        return reinterpret_cast<T *>(temp().allocate(sizeof(T) * count));
    }

    // Set an error state and prints a message. Returns false so errors can be
    // propagated up.
    bool abort(const char *message, ...);
    bool abortFmt(const char *message, va_list ap);

    bool errored() const {
        return error_;
    }

    bool instrumentedProfiling() {
        return GetIonContext()->runtime->spsProfiler.enabled();
    }

    // Whether the main thread is trying to cancel this build.
    bool shouldCancel(const char *why) {
        return cancelBuild_;
    }
    void cancel() {
        cancelBuild_ = 1;
    }

    bool compilingAsmJS() const {
        return info_->script() == NULL;
    }

    uint32_t maxAsmJSStackArgBytes() const {
        JS_ASSERT(compilingAsmJS());
        return maxAsmJSStackArgBytes_;
    }
    uint32_t resetAsmJSMaxStackArgBytes() {
        JS_ASSERT(compilingAsmJS());
        uint32_t old = maxAsmJSStackArgBytes_;
        maxAsmJSStackArgBytes_ = 0;
        return old;
    }
    void setAsmJSMaxStackArgBytes(uint32_t n) {
        JS_ASSERT(compilingAsmJS());
        maxAsmJSStackArgBytes_ = n;
    }
    void setPerformsAsmJSCall() {
        JS_ASSERT(compilingAsmJS());
        performsAsmJSCall_ = true;
    }
    bool performsAsmJSCall() const {
        JS_ASSERT(compilingAsmJS());
        return performsAsmJSCall_;
    }
    bool noteHeapAccess(AsmJSHeapAccess heapAccess) {
        return asmJSHeapAccesses_.append(heapAccess);
    }
    const Vector<AsmJSHeapAccess, 0, IonAllocPolicy> &heapAccesses() const {
        return asmJSHeapAccesses_;
    }
    bool noteGlobalAccess(unsigned offset, unsigned globalDataOffset) {
        return asmJSGlobalAccesses_.append(AsmJSGlobalAccess(offset, globalDataOffset));
    }
    const Vector<AsmJSGlobalAccess, 0, IonAllocPolicy> &globalAccesses() const {
        return asmJSGlobalAccesses_;
    }

  public:
    JSCompartment *compartment;

  protected:
    CompileInfo *info_;
    TempAllocator *temp_;
    JSFunction *fun_;
    uint32_t nslots_;
    MIRGraph *graph_;
    bool error_;
    size_t cancelBuild_;

    uint32_t maxAsmJSStackArgBytes_;
    bool performsAsmJSCall_;
    AsmJSHeapAccessVector asmJSHeapAccesses_;
    AsmJSGlobalAccessVector asmJSGlobalAccesses_;

#if defined(JS_ION_PERF)
    AsmJSPerfSpewer asmJSPerfSpewer_;

  public:
    AsmJSPerfSpewer &perfSpewer() { return asmJSPerfSpewer_; }
#endif
};

} // namespace jit
} // namespace js

#endif /* jit_MIRGenerator_h */
