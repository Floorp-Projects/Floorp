/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_JitFrames_inl_h
#define jit_JitFrames_inl_h

#include "jit/JitFrames.h"

#include "jit/JitFrameIterator.h"
#include "jit/LIR.h"

#include "jit/JitFrameIterator-inl.h"

namespace js {
namespace jit {

inline void
SafepointIndex::resolve()
{
    MOZ_ASSERT(!resolved);
    safepointOffset_ = safepoint_->offset();
#ifdef DEBUG
    resolved = true;
#endif
}

inline uint8_t*
JitFrameIterator::returnAddress() const
{
    CommonFrameLayout* current = (CommonFrameLayout*) current_;
    return current->returnAddress();
}

inline size_t
JitFrameIterator::prevFrameLocalSize() const
{
    CommonFrameLayout* current = (CommonFrameLayout*) current_;
    return current->prevFrameLocalSize();
}

inline FrameType
JitFrameIterator::prevType() const
{
    CommonFrameLayout* current = (CommonFrameLayout*) current_;
    return current->prevType();
}

inline ExitFrameLayout*
JitFrameIterator::exitFrame() const
{
    MOZ_ASSERT(isExitFrame());
    return (ExitFrameLayout*) fp();
}

inline BaselineFrame*
GetTopBaselineFrame(JSContext* cx)
{
    JitFrameIterator iter(cx);
    MOZ_ASSERT(iter.type() == JitFrame_Exit);
    ++iter;
    if (iter.isBaselineStub())
        ++iter;
    MOZ_ASSERT(iter.isBaselineJS());
    return iter.baselineFrame();
}

} // namespace jit
} // namespace js

#endif /* jit_JitFrames_inl_h */
