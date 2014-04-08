/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonFrameIterator_inl_h
#define jit_IonFrameIterator_inl_h

#ifdef JS_ION

#include "jit/IonFrameIterator.h"

#include "jit/Bailouts.h"
#include "jit/BaselineFrame.h"

namespace js {
namespace jit {

template <AllowGC allowGC>
inline
InlineFrameIteratorMaybeGC<allowGC>::InlineFrameIteratorMaybeGC(
                                                JSContext *cx, const IonBailoutIterator *iter)
  : frame_(iter),
    framesRead_(0),
    frameCount_(UINT32_MAX),
    callee_(cx),
    script_(cx)
{
    if (iter) {
        start_ = SnapshotIterator(*iter);
        findNextFrame();
    }
}

inline BaselineFrame *
IonFrameIterator::baselineFrame() const
{
    JS_ASSERT(isBaselineJS());
    return (BaselineFrame *)(fp() - BaselineFrame::FramePointerOffset - BaselineFrame::Size());
}

} // namespace jit
} // namespace js

#endif // JS_ION

#endif /* jit_IonFrameIterator_inl_h */
