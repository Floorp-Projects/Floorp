/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(jsion_baseline_jit_h__) && defined(JS_ION)
#define jsion_baseline_jit_h__

#include "jscntxt.h"
#include "jscompartment.h"
#include "IonCode.h"
#include "ion/IonMacroAssembler.h"

namespace js {
namespace ion {

struct ICEntry;

struct BaselineScript
{
  private:
    // Code pointer containing the actual method.
    HeapPtr<IonCode> method_;

  private:
    void trace(JSTracer *trc);

    uint32_t icEntriesOffset_;
    uint32_t icEntries_;

  public:
    // Do not call directly, use BaselineScript::New. This is public for cx->new_.
    BaselineScript();

    static BaselineScript *New(JSContext *cx, size_t icEntries);
    static void Trace(JSTracer *trc, BaselineScript *script);
    static void Destroy(FreeOp *fop, BaselineScript *script);

    static inline size_t offsetOfMethod() {
        return offsetof(BaselineScript, method_);
    }

    ICEntry *icEntryList() {
        return (ICEntry *)(reinterpret_cast<uint8_t *>(this) + icEntriesOffset_);
    }

    IonCode *method() const {
        return method_;
    }
    void setMethod(IonCode *code) {
        //JS_ASSERT(!invalidated());
        method_ = code;
    }

    ICEntry &icEntry(size_t index);
    ICEntry &icEntryFromReturnOffset(CodeOffsetLabel returnOffset);

    size_t numICEntries() const {
        return icEntries_;
    }

    void copyICEntries(const ICEntry *entries, MacroAssembler &masm);
};

MethodStatus
CanEnterBaselineJIT(JSContext *cx, HandleScript script, StackFrame *fp);

IonExecStatus
EnterBaselineMethod(JSContext *cx, StackFrame *fp);

} // namespace ion
} // namespace js

#endif

