/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
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
 * The Original Code is SpiderMonkey arguments object code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu> (original author)
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

#ifndef ArgumentsObject_inl_h___
#define ArgumentsObject_inl_h___

#include "ArgumentsObject.h"

namespace js {

inline void
ArgumentsObject::initInitialLength(uint32 length)
{
    JS_ASSERT(getFixedSlot(INITIAL_LENGTH_SLOT).isUndefined());
    initFixedSlot(INITIAL_LENGTH_SLOT, Int32Value(length << PACKED_BITS_COUNT));
    JS_ASSERT((getFixedSlot(INITIAL_LENGTH_SLOT).toInt32() >> PACKED_BITS_COUNT) == int32(length));
    JS_ASSERT(!hasOverriddenLength());
}

inline uint32
ArgumentsObject::initialLength() const
{
    uint32 argc = uint32(getFixedSlot(INITIAL_LENGTH_SLOT).toInt32()) >> PACKED_BITS_COUNT;
    JS_ASSERT(argc <= StackSpace::ARGS_LENGTH_MAX);
    return argc;
}

inline void
ArgumentsObject::markLengthOverridden()
{
    uint32 v = getFixedSlot(INITIAL_LENGTH_SLOT).toInt32() | LENGTH_OVERRIDDEN_BIT;
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

inline const js::Value &
ArgumentsObject::element(uint32 i) const
{
    JS_ASSERT(i < initialLength());
    return data()->slots[i];
}

inline const js::Value *
ArgumentsObject::elements() const
{
    return Valueify(data()->slots);
}

inline void
ArgumentsObject::setElement(uint32 i, const js::Value &v)
{
    JS_ASSERT(i < initialLength());
    data()->slots[i] = v;
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

inline const js::Value &
NormalArgumentsObject::callee() const
{
    return data()->callee;
}

inline void
NormalArgumentsObject::clearCallee()
{
    data()->callee.set(compartment(), MagicValue(JS_ARGS_HOLE));
}

} // namespace js

#endif /* ArgumentsObject_inl_h___ */
