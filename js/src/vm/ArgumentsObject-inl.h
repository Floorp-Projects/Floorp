/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ArgumentsObject_inl_h___
#define ArgumentsObject_inl_h___

#include "ArgumentsObject.h"

namespace js {

inline void
ArgumentsObject::initInitialLength(uint32_t length)
{
    JS_ASSERT(getFixedSlot(INITIAL_LENGTH_SLOT).isUndefined());
    initFixedSlot(INITIAL_LENGTH_SLOT, Int32Value(length << PACKED_BITS_COUNT));
    JS_ASSERT((getFixedSlot(INITIAL_LENGTH_SLOT).toInt32() >> PACKED_BITS_COUNT) == int32_t(length));
    JS_ASSERT(!hasOverriddenLength());
}

inline uint32_t
ArgumentsObject::initialLength() const
{
    uint32_t argc = uint32_t(getFixedSlot(INITIAL_LENGTH_SLOT).toInt32()) >> PACKED_BITS_COUNT;
    JS_ASSERT(argc <= StackSpace::ARGS_LENGTH_MAX);
    return argc;
}

inline void
ArgumentsObject::markLengthOverridden()
{
    uint32_t v = getFixedSlot(INITIAL_LENGTH_SLOT).toInt32() | LENGTH_OVERRIDDEN_BIT;
    setFixedSlot(INITIAL_LENGTH_SLOT, Int32Value(v));
}

inline bool
ArgumentsObject::hasOverriddenLength() const
{
    const js::Value &v = getFixedSlot(INITIAL_LENGTH_SLOT);
    return v.toInt32() & LENGTH_OVERRIDDEN_BIT;
}

inline void
ArgumentsObject::initData(ArgumentsData *data)
{
    JS_ASSERT(getFixedSlot(DATA_SLOT).isUndefined());
    initFixedSlot(DATA_SLOT, PrivateValue(data));
}

inline ArgumentsData *
ArgumentsObject::data() const
{
    return reinterpret_cast<js::ArgumentsData *>(getFixedSlot(DATA_SLOT).toPrivate());
}

inline bool
ArgumentsObject::isElementDeleted(uint32_t i) const
{
    return IsBitArrayElementSet(data()->deletedBits, initialLength(), i);
}

inline bool
ArgumentsObject::isAnyElementDeleted() const
{
    return IsAnyBitArrayElementSet(data()->deletedBits, initialLength());
}

inline void
ArgumentsObject::markElementDeleted(uint32_t i)
{
    SetBitArrayElement(data()->deletedBits, initialLength(), i);
}

inline const js::Value &
ArgumentsObject::element(uint32_t i) const
{
    JS_ASSERT(!isElementDeleted(i));
    return data()->slots[i];
}

inline void
ArgumentsObject::setElement(uint32_t i, const js::Value &v)
{
    JS_ASSERT(!isElementDeleted(i));
    data()->slots[i] = v;
}

inline bool
ArgumentsObject::getElement(uint32_t i, Value *vp)
{
    if (i >= initialLength() || isElementDeleted(i))
        return false;

    /*
     * If this arguments object has an associated stack frame, that contains
     * the canonical argument value.  Note that strict arguments objects do not
     * alias named arguments and never have a stack frame.
     */
    StackFrame *fp = maybeStackFrame();
    JS_ASSERT_IF(isStrictArguments(), !fp);
    if (fp)
        *vp = fp->canonicalActualArg(i);
    else
        *vp = element(i);
    return true;
}

namespace detail {

struct STATIC_SKIP_INFERENCE CopyNonHoleArgsTo
{
    CopyNonHoleArgsTo(ArgumentsObject *argsobj, Value *dst) : argsobj(*argsobj), dst(dst) {}
    ArgumentsObject &argsobj;
    Value *dst;
    bool operator()(uint32_t argi, Value *src) {
        *dst++ = *src;
        return true;
    }
};

} /* namespace detail */

inline bool
ArgumentsObject::getElements(uint32_t start, uint32_t count, Value *vp)
{
    JS_ASSERT(start + count >= start);

    uint32_t length = initialLength();
    if (start > length || start + count > length || isAnyElementDeleted())
        return false;

    StackFrame *fp = maybeStackFrame();

    /* If there's no stack frame for this, argument values are in elements(). */
    if (!fp) {
        const Value *srcbeg = Valueify(data()->slots) + start;
        const Value *srcend = srcbeg + count;
        const Value *src = srcbeg;
        for (Value *dst = vp; src < srcend; ++dst, ++src)
            *dst = *src;
        return true;
    }

    /* Otherwise, element values are on the stack. */
    JS_ASSERT(fp->numActualArgs() <= StackSpace::ARGS_LENGTH_MAX);
    return fp->forEachCanonicalActualArg(detail::CopyNonHoleArgsTo(this, vp), start, count);
}

inline js::StackFrame *
ArgumentsObject::maybeStackFrame() const
{
    return reinterpret_cast<js::StackFrame *>(getFixedSlot(STACK_FRAME_SLOT).toPrivate());
}

inline void
ArgumentsObject::setStackFrame(StackFrame *frame)
{
    setFixedSlot(STACK_FRAME_SLOT, PrivateValue(frame));
}

inline size_t
ArgumentsObject::sizeOfMisc(JSMallocSizeOfFun mallocSizeOf) const
{
    return mallocSizeOf(data());
}

inline const js::Value &
NormalArgumentsObject::callee() const
{
    return data()->callee;
}

inline void
NormalArgumentsObject::clearCallee()
{
    data()->callee.set(compartment(), MagicValue(JS_OVERWRITTEN_CALLEE));
}

} // namespace js

#endif /* ArgumentsObject_inl_h___ */
