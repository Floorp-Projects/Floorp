/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(jsion_frame_iterator_inl_h__) && defined(JS_ION)
#define jsion_frame_iterator_inl_h__

#include "ion/BaselineFrame.h"
#include "ion/IonFrameIterator.h"
#include "ion/Bailouts.h"
#include "ion/Ion.h"

namespace js {
namespace ion {

template <class Op>
inline void
SnapshotIterator::readFrameArgs(Op &op, const Value *argv, Value *scopeChain, Value *thisv,
                                unsigned start, unsigned formalEnd, unsigned iterEnd,
                                JSScript *script)
{
    if (scopeChain)
        *scopeChain = read();
    else
        skip();

    if (thisv)
        *thisv = read();
    else
        skip();

    // Skip slot for arguments object.
    if (script->argumentsHasVarBinding())
        skip();

    unsigned i = 0;
    if (formalEnd < start)
        i = start;

    for (; i < start; i++)
        skip();
    for (; i < formalEnd && i < iterEnd; i++) {
        // We are not always able to read values from the snapshots, some values
        // such as non-gc things may still be live in registers and cause an
        // error while reading the machine state.
        Value v = maybeRead();
        op(v);
    }
    if (iterEnd >= formalEnd) {
        for (; i < iterEnd; i++)
            op(argv[i]);
    }
}

template <AllowGC allowGC>
inline
InlineFrameIteratorMaybeGC<allowGC>::InlineFrameIteratorMaybeGC(
                                JSContext *cx, const IonFrameIterator *iter)
  : callee_(cx),
    script_(cx)
{
    resetOn(iter);
}

template <AllowGC allowGC>
inline
InlineFrameIteratorMaybeGC<allowGC>::InlineFrameIteratorMaybeGC(
        JSContext *cx,
        const InlineFrameIteratorMaybeGC<allowGC> *iter)
  : frame_(iter ? iter->frame_ : NULL),
    framesRead_(0),
    callee_(cx),
    script_(cx)
{
    if (frame_) {
        start_ = SnapshotIterator(*frame_);
        // findNextFrame will iterate to the next frame and init. everything.
        // Therefore to settle on the same frame, we report one frame less readed.
        framesRead_ = iter->framesRead_ - 1;
        findNextFrame();
    }
}

template <AllowGC allowGC>
template <class Op>
inline void
InlineFrameIteratorMaybeGC<allowGC>::forEachCanonicalActualArg(
                JSContext *cx, Op op, unsigned start, unsigned count) const
{
    unsigned nactual = numActualArgs();
    if (count == unsigned(-1))
        count = nactual - start;

    unsigned end = start + count;
    unsigned nformal = callee()->nargs;

    JS_ASSERT(start <= end && end <= nactual);

    if (more()) {
        // There is still a parent frame of this inlined frame.
        // The not overflown arguments are taken from the inlined frame,
        // because it will have the updated value when JSOP_SETARG is done.
        // All arguments (also the overflown) are the last pushed values in the parent frame.
        // To get the overflown arguments, we need to take them from there.

        // Get the non overflown arguments
        unsigned formal_end = (end < nformal) ? end : nformal;
        SnapshotIterator s(si_);
        s.readFrameArgs(op, NULL, NULL, NULL, start, nformal, formal_end, script());

        // The overflown arguments are not available in current frame.
        // They are the last pushed arguments in the parent frame of this inlined frame.
        InlineFrameIteratorMaybeGC it(cx, this);
        ++it;
        unsigned argsObjAdj = it.script()->argumentsHasVarBinding() ? 1 : 0;
        SnapshotIterator parent_s(it.snapshotIterator());

        // Skip over all slots untill we get to the last slots (= arguments slots of callee)
        // the +2 is for [this] and [scopechain], and maybe +1 for [argsObj]
        JS_ASSERT(parent_s.slots() >= nactual + 2 + argsObjAdj);
        unsigned skip = parent_s.slots() - nactual - 2 - argsObjAdj;
        for (unsigned j = 0; j < skip; j++)
            parent_s.skip();

        // Get the overflown arguments
        parent_s.readFrameArgs(op, NULL, NULL, NULL, nformal, nactual, end, it.script());
    } else {
        SnapshotIterator s(si_);
        Value *argv = frame_->actualArgs();
        s.readFrameArgs(op, argv, NULL, NULL, start, nformal, end, script());
    }
}
 
template <AllowGC allowGC>
inline JSObject *
InlineFrameIteratorMaybeGC<allowGC>::scopeChain() const
{
    SnapshotIterator s(si_);

    // scopeChain
    Value v = s.read();
    if (v.isObject()) {
        JS_ASSERT_IF(script()->hasAnalysis(), script()->analysis()->usesScopeChain());
        return &v.toObject();
    }

    return callee()->environment();
}

template <AllowGC allowGC>
inline JSObject *
InlineFrameIteratorMaybeGC<allowGC>::thisObject() const
{
    // JS_ASSERT(isConstructing(...));
    SnapshotIterator s(si_);

    // scopeChain
    s.skip();

    // In strict modes, |this| may not be an object and thus may not be
    // readable which can either segv in read or trigger the assertion.
    Value v = s.read();
    JS_ASSERT(v.isObject());
    return &v.toObject();
}

template <AllowGC allowGC>
inline unsigned
InlineFrameIteratorMaybeGC<allowGC>::numActualArgs() const
{
    // The number of actual arguments of inline frames is recovered by the
    // iteration process. It is recovered from the bytecode because this
    // property still hold since the for inlined frames. This property does not
    // hold for the parent frame because it can have optimize a call to
    // js_fun_call or js_fun_apply.
    if (more())
        return numActualArgs_;

    return frame_->numActualArgs();
}

template <AllowGC allowGC>
inline
InlineFrameIteratorMaybeGC<allowGC>::InlineFrameIteratorMaybeGC(
                                                JSContext *cx, const IonBailoutIterator *iter)
  : frame_(iter),
    framesRead_(0),
    callee_(cx),
    script_(cx)
{
    if (iter) {
        start_ = SnapshotIterator(*iter);
        findNextFrame();
    }
}

template <AllowGC allowGC>
inline InlineFrameIteratorMaybeGC<allowGC> &
InlineFrameIteratorMaybeGC<allowGC>::operator++()
{
    findNextFrame();
    return *this;
}

template <class Op>
inline void
IonFrameIterator::forEachCanonicalActualArg(Op op, unsigned start, unsigned count) const
{
    JS_ASSERT(isBaselineJS());

    unsigned nactual = numActualArgs();
    if (count == unsigned(-1))
        count = nactual - start;

    unsigned end = start + count;
    JS_ASSERT(start <= end && end <= nactual);

    Value *argv = actualArgs();
    for (unsigned i = start; i < end; i++)
        op(argv[i]);
}

inline BaselineFrame *
IonFrameIterator::baselineFrame() const
{
    JS_ASSERT(isBaselineJS());
    return (BaselineFrame *)(fp() - BaselineFrame::FramePointerOffset - BaselineFrame::Size());
}

} // namespace ion
} // namespace js

#endif // jsion_frame_iterator_inl_h__
