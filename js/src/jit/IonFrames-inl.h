/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonFrames_inl_h
#define jit_IonFrames_inl_h

#ifdef JS_ION

#include "jit/IonFrames.h"
#include "jit/IonFrameIterator.h"
#include "jit/LIR.h"

namespace js {
namespace jit {

inline void
SafepointIndex::resolve()
{
    JS_ASSERT(!resolved);
    safepointOffset_ = safepoint_->offset();
    resolved = true;
}

static inline size_t
SizeOfFramePrefix(FrameType type)
{
    switch (type) {
      case IonFrame_Entry:
        return IonEntryFrameLayout::Size();
      case IonFrame_BaselineJS:
      case IonFrame_OptimizedJS:
      case IonFrame_Unwound_OptimizedJS:
        return IonJSFrameLayout::Size();
      case IonFrame_BaselineStub:
        return IonBaselineStubFrameLayout::Size();
      case IonFrame_Rectifier:
        return IonRectifierFrameLayout::Size();
      case IonFrame_Unwound_Rectifier:
        return IonUnwoundRectifierFrameLayout::Size();
      case IonFrame_Exit:
        return IonExitFrameLayout::Size();
      case IonFrame_Osr:
        return IonOsrFrameLayout::Size();
      default:
        JS_NOT_REACHED("unknown frame type");
    }
    return 0;
}

inline IonCommonFrameLayout *
IonFrameIterator::current() const
{
    return (IonCommonFrameLayout *)current_;
}

inline uint8_t *
IonFrameIterator::returnAddress() const
{
    IonCommonFrameLayout *current = (IonCommonFrameLayout *) current_;
    return current->returnAddress();
}

inline size_t
IonFrameIterator::prevFrameLocalSize() const
{
    IonCommonFrameLayout *current = (IonCommonFrameLayout *) current_;
    return current->prevFrameLocalSize();
}

inline FrameType
IonFrameIterator::prevType() const
{
    IonCommonFrameLayout *current = (IonCommonFrameLayout *) current_;
    return current->prevType();
}

inline bool
IonFrameIterator::isFakeExitFrame() const
{
    bool res = (prevType() == IonFrame_Unwound_Rectifier ||
                prevType() == IonFrame_Unwound_OptimizedJS ||
                prevType() == IonFrame_Unwound_BaselineStub);
    JS_ASSERT_IF(res, type() == IonFrame_Exit || type() == IonFrame_BaselineJS);
    return res;
}

inline IonExitFrameLayout *
IonFrameIterator::exitFrame() const
{
    JS_ASSERT(type() == IonFrame_Exit);
    JS_ASSERT(!isFakeExitFrame());
    return (IonExitFrameLayout *) fp();
}

size_t
IonFrameIterator::frameSize() const
{
    JS_ASSERT(type_ != IonFrame_Exit);
    return frameSize_;
}

// Returns the JSScript associated with the topmost Ion frame.
inline JSScript *
GetTopIonJSScript(PerThreadData *pt, const SafepointIndex **safepointIndexOut, void **returnAddrOut)
{
    IonFrameIterator iter(pt->ionTop);
    JS_ASSERT(iter.type() == IonFrame_Exit);
    ++iter;

    // If needed, grab the safepoint index.
    if (safepointIndexOut)
        *safepointIndexOut = iter.safepoint();

    JS_ASSERT(iter.returnAddressToFp() != NULL);
    if (returnAddrOut)
        *returnAddrOut = (void *) iter.returnAddressToFp();

    if (iter.isBaselineStub()) {
        ++iter;
        JS_ASSERT(iter.isBaselineJS());
    }

    JS_ASSERT(iter.isScripted());
    return iter.script();
}


inline JSScript *
GetTopIonJSScript(JSContext *cx, const SafepointIndex **safepointIndexOut, void **returnAddrOut)
{
    return GetTopIonJSScript(&cx->mainThread(), safepointIndexOut, returnAddrOut);
}

inline BaselineFrame *
GetTopBaselineFrame(JSContext *cx)
{
    IonFrameIterator iter(cx->mainThread().ionTop);
    JS_ASSERT(iter.type() == IonFrame_Exit);
    ++iter;
    if (iter.isBaselineStub())
        ++iter;
    JS_ASSERT(iter.isBaselineJS());
    return iter.baselineFrame();
}

} // namespace jit
} // namespace js

#endif // JS_ION

#endif /* jit_IonFrames_inl_h */
