/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_RegExpObject_inl_h
#define vm_RegExpObject_inl_h

#include "vm/RegExpObject.h"

#include "mozilla/Util.h"

namespace js {

inline void
RegExpObject::setShared(ExclusiveContext *cx, RegExpShared &shared)
{
    shared.prepareForUse(cx);
    JSObject::setPrivate(&shared);
}

inline void
RegExpObject::setLastIndex(double d)
{
    setSlot(LAST_INDEX_SLOT, NumberValue(d));
}

inline void
RegExpObject::zeroLastIndex()
{
    setSlot(LAST_INDEX_SLOT, Int32Value(0));
}

inline void
RegExpObject::setSource(JSAtom *source)
{
    setSlot(SOURCE_SLOT, StringValue(source));
}

inline void
RegExpObject::setIgnoreCase(bool enabled)
{
    setSlot(IGNORE_CASE_FLAG_SLOT, BooleanValue(enabled));
}

inline void
RegExpObject::setGlobal(bool enabled)
{
    setSlot(GLOBAL_FLAG_SLOT, BooleanValue(enabled));
}

inline void
RegExpObject::setMultiline(bool enabled)
{
    setSlot(MULTILINE_FLAG_SLOT, BooleanValue(enabled));
}

inline void
RegExpObject::setSticky(bool enabled)
{
    setSlot(STICKY_FLAG_SLOT, BooleanValue(enabled));
}

} /* namespace js */

#endif /* vm_RegExpObject_inl_h */
