/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_RematerializedFrame_h
#define jit_RematerializedFrame_h

#ifdef JS_ION

#include "jsfun.h"

#include "jit/JitFrameIterator.h"

#include "vm/Stack.h"

namespace js {
namespace jit {

//
// An optimized frame that has been rematerialized with values read out of
// Snapshots.
//
class RematerializedFrame
{
    // See DebugScopes::updateLiveScopes.
    bool prevUpToDate_;

    // The fp of the top frame associated with this possibly inlined frame.
    uint8_t *top_;

    size_t frameNo_;
    unsigned numActualArgs_;

    JSScript *script_;
    JSObject *scopeChain_;
    ArgumentsObject *argsObj_;

    Value returnValue_;
    Value thisValue_;
    Value slots_[1];

    RematerializedFrame(ThreadSafeContext *cx, uint8_t *top, InlineFrameIterator &iter);

  public:
    static RematerializedFrame *New(ThreadSafeContext *cx, uint8_t *top,
                                    InlineFrameIterator &iter);

    bool prevUpToDate() const {
        return prevUpToDate_;
    }
    void setPrevUpToDate() {
        prevUpToDate_ = true;
    }

    uint8_t *top() const {
        return top_;
    }
    size_t frameNo() const {
        return frameNo_;
    }
    bool inlined() const {
        return frameNo_ > 0;
    }

    JSObject *scopeChain() const {
        return scopeChain_;
    }
    bool hasCallObj() const {
        return maybeFun() && fun()->isHeavyweight();
    }
    CallObject &callObj() const;

    bool hasArgsObj() const {
        return !!argsObj_;
    }
    ArgumentsObject &argsObj() const {
        MOZ_ASSERT(hasArgsObj());
        MOZ_ASSERT(script()->needsArgsObj());
        return *argsObj_;
    }

    bool isFunctionFrame() const {
        return !!script_->functionNonDelazifying();
    }
    bool isGlobalFrame() const {
        return !isFunctionFrame();
    }
    bool isNonEvalFunctionFrame() const {
        // Ion doesn't support eval frames.
        return isFunctionFrame();
    }

    JSScript *script() const {
        return script_;
    }
    JSFunction *fun() const {
        MOZ_ASSERT(isFunctionFrame());
        return script_->functionNonDelazifying();
    }
    JSFunction *maybeFun() const {
        return isFunctionFrame() ? fun() : nullptr;
    }
    JSFunction *callee() const {
        return fun();
    }
    Value calleev() const {
        return ObjectValue(*fun());
    }
    Value &thisValue() {
        return thisValue_;
    }

    unsigned numFormalArgs() const {
        return maybeFun() ? fun()->nargs() : 0;
    }
    unsigned numActualArgs() const {
        return numActualArgs_;
    }

    Value *argv() {
        return slots_;
    }
    Value *locals() {
        return slots_ + numActualArgs_;
    }

    Value &unaliasedVar(unsigned i, MaybeCheckAliasing checkAliasing = CHECK_ALIASING) {
        JS_ASSERT_IF(checkAliasing, !script()->varIsAliased(i));
        JS_ASSERT(i < script()->nfixed());
        return locals()[i];
    }
    Value &unaliasedLocal(unsigned i, MaybeCheckAliasing checkAliasing = CHECK_ALIASING) {
        JS_ASSERT(i < script()->nfixed());
#ifdef DEBUG
        CheckLocalUnaliased(checkAliasing, script(), i);
#endif
        return locals()[i];
    }
    Value &unaliasedFormal(unsigned i, MaybeCheckAliasing checkAliasing = CHECK_ALIASING) {
        JS_ASSERT(i < numFormalArgs());
        JS_ASSERT_IF(checkAliasing, !script()->argsObjAliasesFormals() &&
                                    !script()->formalIsAliased(i));
        return argv()[i];
    }
    Value &unaliasedActual(unsigned i, MaybeCheckAliasing checkAliasing = CHECK_ALIASING) {
        JS_ASSERT(i < numActualArgs());
        JS_ASSERT_IF(checkAliasing, !script()->argsObjAliasesFormals());
        JS_ASSERT_IF(checkAliasing && i < numFormalArgs(), !script()->formalIsAliased(i));
        return argv()[i];
    }

    Value returnValue() const {
        return returnValue_;
    }

    void mark(JSTracer *trc);
    void dump();
};

} // namespace jit
} // namespace js

#endif // JS_ION
#endif // jit_RematerializedFrame_h
