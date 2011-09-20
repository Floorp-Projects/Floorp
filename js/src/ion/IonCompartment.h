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
#include "jsvalue.h"
#include "jsvector.h"
#include "vm/Stack.h"
#include "IonFrames.h"

namespace js {
namespace ion {

class FrameSizeClass;

typedef JSBool (*EnterIonCode)(void *code, int argc, Value *argv, Value *vp,
                               CalleeToken calleeToken);

class IonActivation;

class IonCompartment
{
    friend class IonActivation;

    JSC::ExecutableAllocator *execAlloc_;

    // Current activation of ion::Cannon.
    IonActivation *active_;

    // Trampoline for entering JIT code.
    IonCode *enterJIT_;

    // Vector mapping frame class sizes to bailout tables.
    js::Vector<IonCode *, 4, SystemAllocPolicy> bailoutTables_;

    // Generic bailout table; used if the bailout table overflows.
    IonCode *bailoutHandler_;

    // Error-returning thunk.
    IonCode *returnError_;

    // Argument-rectifying thunk, in the case of insufficient arguments passed
    // to a function call site. Pads with |undefined|.
    IonCode *argumentsRectifier_;

    IonCode *generateEnterJIT(JSContext *cx);
    IonCode *generateReturnError(JSContext *cx);
    IonCode *generateArgumentsRectifier(JSContext *cx);
    IonCode *generateBailoutTable(JSContext *cx, uint32 frameClass);
    IonCode *generateBailoutHandler(JSContext *cx);

  public:
    bool initialize(JSContext *cx);
    IonCompartment();
    ~IonCompartment();

    void mark(JSTracer *trc, JSCompartment *compartment);
    void sweep(JSContext *cx);

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

    EnterIonCode enterJIT(JSContext *cx) {
        if (!enterJIT_) {
            enterJIT_ = generateEnterJIT(cx);
            if (!enterJIT_)
                return NULL;
        }
        if (!returnError_) {
            returnError_ = generateReturnError(cx);
            if (!returnError_)
                return NULL;
        }
        return enterJIT_->as<EnterIonCode>();
    }
    IonCode *returnError() const {
        JS_ASSERT(returnError_);
        return returnError_;
    }

    IonActivation *activation() const {
        return active_;
    }
};

class BailoutClosure;

class IonActivation
{
    JSContext *cx_;
    IonActivation *prev_;
    StackFrame *entryfp_;
    FrameRegs &oldFrameRegs_;
    BailoutClosure *bailout_;

  public:
    IonActivation(JSContext *cx, StackFrame *fp);
    ~IonActivation();
    StackFrame *entryfp() const {
        return entryfp_;
    }
    IonActivation *prev() const {
        return prev_;
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
    FrameRegs &oldFrameRegs() {
        return oldFrameRegs_;
    }
};

} // namespace ion
} // namespace js

#endif // jsion_ion_compartment_h__

