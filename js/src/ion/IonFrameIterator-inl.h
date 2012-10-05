/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_frame_iterator_inl_h__
#define jsion_frame_iterator_inl_h__

#include "ion/IonFrameIterator.h"

namespace js {
namespace ion {

template <class Op>
inline void
SnapshotIterator::readFrameArgs(Op op, const Value *argv, Value *scopeChain, Value *thisv,
                                unsigned start, unsigned formalEnd, unsigned iterEnd)
{
    if (scopeChain)
        *scopeChain = read();
    else
        skip();

    if (thisv)
        *thisv = read();
    else
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

template <class Op>
inline void
InlineFrameIterator::forEachCanonicalActualArg(Op op, unsigned start, unsigned count) const
{
    unsigned nactual = numActualArgs();
    if (count == unsigned(-1))
        count = nactual - start;

    unsigned end = start + count;
    unsigned nformal = callee()->nargs;

    JS_ASSERT(start <= end && end <= nactual);

    // Currently inlining does not support overflow of arguments, we have to
    // add this feature in IonBuilder.cpp and in Bailouts.cpp before
    // continuing. We need to add it to Bailouts.cpp because we need to know
    // how to walk over the oveflow of arguments.
    JS_ASSERT_IF(more(), end <= nformal);

    SnapshotIterator s(si_);
    Value *argv = frame_->actualArgs();
    s.readFrameArgs(op, argv, NULL, NULL, start, nformal, end);
}

} // namespace ion
} // namespace js

#endif // jsion_frame_iterator_inl_h__
