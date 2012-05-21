/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RegExpObject_inl_h___
#define RegExpObject_inl_h___

#include "mozilla/Util.h"

#include "RegExpObject.h"
#include "RegExpStatics.h"

#include "jsobjinlines.h"
#include "jsstrinlines.h"

#include "RegExpStatics-inl.h"

inline js::RegExpObject &
JSObject::asRegExp()
{
    JS_ASSERT(isRegExp());
    return *static_cast<js::RegExpObject *>(this);
}

namespace js {

inline RegExpShared *
RegExpObject::maybeShared() const
{
    return static_cast<RegExpShared *>(JSObject::getPrivate());
}

inline void
RegExpObject::shared(RegExpGuard *g) const
{
    JS_ASSERT(maybeShared() != NULL);
    g->init(*maybeShared());
}

inline bool
RegExpObject::getShared(JSContext *cx, RegExpGuard *g)
{
    if (RegExpShared *shared = maybeShared()) {
        g->init(*shared);
        return true;
    }
    return createShared(cx, g);
}

inline void
RegExpObject::setShared(JSContext *cx, RegExpShared &shared)
{
    shared.prepareForUse(cx);
    JSObject::setPrivate(&shared);
}

inline void
RegExpObject::setLastIndex(const Value &v)
{
    setSlot(LAST_INDEX_SLOT, v);
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

#if ENABLE_YARR_JIT
/* This function should be deleted once bad Android platforms phase out. See bug 604774. */
inline bool
detail::RegExpCode::isJITRuntimeEnabled(JSContext *cx)
{
#if defined(ANDROID) && defined(JS_METHODJIT)
    return cx->methodJitEnabled;
#else
    return true;
#endif
}
#endif

inline bool
RegExpToShared(JSContext *cx, JSObject &obj, RegExpGuard *g)
{
    JS_ASSERT(ObjectClassIs(obj, ESClass_RegExp, cx));
    if (obj.isRegExp())
        return obj.asRegExp().getShared(cx, g);
    return Proxy::regexp_toShared(cx, &obj, g);
}

inline void
RegExpShared::prepareForUse(JSContext *cx)
{
    gcNumberWhenUsed = cx->runtime->gcNumber;
}

} /* namespace js */

#endif
