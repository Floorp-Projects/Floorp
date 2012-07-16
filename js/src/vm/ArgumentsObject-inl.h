/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ArgumentsObject_inl_h___
#define ArgumentsObject_inl_h___

#include "ArgumentsObject.h"

#include "ScopeObject-inl.h"

namespace js {

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
    const Value &v = getFixedSlot(INITIAL_LENGTH_SLOT);
    return v.toInt32() & LENGTH_OVERRIDDEN_BIT;
}

inline ArgumentsData *
ArgumentsObject::data() const
{
    return reinterpret_cast<ArgumentsData *>(getFixedSlot(DATA_SLOT).toPrivate());
}

inline JSScript *
ArgumentsObject::containingScript() const
{
    return data()->script;
}

inline const Value &
ArgumentsObject::arg(unsigned i) const
{
    JS_ASSERT(i < data()->numArgs);
    const Value &v = data()->args[i];
    JS_ASSERT(!v.isMagic(JS_FORWARD_TO_CALL_OBJECT));
    return v;
}

inline void
ArgumentsObject::setArg(unsigned i, const Value &v)
{
    JS_ASSERT(i < data()->numArgs);
    HeapValue &lhs = data()->args[i];
    JS_ASSERT(!lhs.isMagic(JS_FORWARD_TO_CALL_OBJECT));
    lhs = v;
}

inline const Value &
ArgumentsObject::element(uint32_t i) const
{
    JS_ASSERT(!isElementDeleted(i));
    const Value &v = data()->args[i];
    if (v.isMagic(JS_FORWARD_TO_CALL_OBJECT))
        return getFixedSlot(MAYBE_CALL_SLOT).toObject().asCall().arg(i);
    return v;
}

inline void
ArgumentsObject::setElement(uint32_t i, const Value &v)
{
    JS_ASSERT(!isElementDeleted(i));
    HeapValue &lhs = data()->args[i];
    if (lhs.isMagic(JS_FORWARD_TO_CALL_OBJECT))
        getFixedSlot(MAYBE_CALL_SLOT).toObject().asCall().setArg(i, v);
    else
        lhs = v;
}

inline bool
ArgumentsObject::isElementDeleted(uint32_t i) const
{
    JS_ASSERT(i < data()->numArgs);
    if (i >= initialLength())
        return false;
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

inline bool
ArgumentsObject::maybeGetElement(uint32_t i, Value *vp)
{
    if (i >= initialLength() || isElementDeleted(i))
        return false;
    *vp = element(i);
    return true;
}

inline bool
ArgumentsObject::maybeGetElements(uint32_t start, uint32_t count, Value *vp)
{
    JS_ASSERT(start + count >= start);

    uint32_t length = initialLength();
    if (start > length || start + count > length || isAnyElementDeleted())
        return false;

    for (uint32_t i = start, end = start + count; i < end; ++i, ++vp)
        *vp = element(i);
    return true;
}

inline size_t
ArgumentsObject::sizeOfMisc(JSMallocSizeOfFun mallocSizeOf) const
{
    return mallocSizeOf(data());
}

inline const Value &
NormalArgumentsObject::callee() const
{
    return data()->callee;
}

inline void
NormalArgumentsObject::clearCallee()
{
    data()->callee.set(compartment(), MagicValue(JS_OVERWRITTEN_CALLEE));
}

} /* namespace js */

#endif /* ArgumentsObject_inl_h___ */
