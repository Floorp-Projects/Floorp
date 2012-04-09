/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

class IonCompartment
{
    typedef WeakCache<const VMFunction *, ReadBarriered<IonCode> > VMWrapperMap;

    friend class IonActivation;

    // Executable allocator (owned by the runtime).
    JSC::ExecutableAllocator *execAlloc_;

    // Trampoline for entering JIT code. Contains OSR prologue.
    ReadBarriered<IonCode> enterJIT_;

    // Vector mapping frame class sizes to bailout tables.
    js::Vector<ReadBarriered<IonCode>, 4, SystemAllocPolicy> bailoutTables_;

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

    IonCode *getOrCreateInvalidationThunk(JSContext *cx) {
        if (!invalidator_) {
            invalidator_ = generateInvalidator(cx);
            if (!invalidator_)
                return NULL;
        }
        return invalidator_;
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
};

} // namespace ion
} // namespace js

#endif // jsion_ion_compartment_h__

