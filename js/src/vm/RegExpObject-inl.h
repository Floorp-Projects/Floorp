/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RegExpObject_inl_h___
#define RegExpObject_inl_h___

#include "mozilla/Util.h"

#include "RegExpObject.h"

#include "jsstrinlines.h"

#include "String-inl.h"

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

/* This function should be deleted once bad Android platforms phase out. See bug 604774. */
inline bool
RegExpShared::isJITRuntimeEnabled(JSContext *cx)
{
#if ENABLE_YARR_JIT
# if defined(ANDROID)
    return !cx->jitIsBroken;
# else
    return true;
# endif
#else
    return false;
#endif
}

inline bool
RegExpToShared(JSContext *cx, HandleObject obj, RegExpGuard *g)
{
    if (obj->is<RegExpObject>())
        return obj->as<RegExpObject>().getShared(cx, g);
    return Proxy::regexp_toShared(cx, obj, g);
}

inline void
RegExpShared::prepareForUse(JSContext *cx)
{
    gcNumberWhenUsed = cx->runtime()->gcNumber;
}

RegExpGuard::RegExpGuard(JSContext *cx)
  : re_(NULL), source_(cx)
{
}

RegExpGuard::RegExpGuard(JSContext *cx, RegExpShared &re)
  : re_(&re), source_(cx, re.source)
{
    re_->incRef();
}

RegExpGuard::~RegExpGuard()
{
    release();
}

inline void
RegExpGuard::init(RegExpShared &re)
{
    JS_ASSERT(!initialized());
    re_ = &re;
    re_->incRef();
    source_ = re_->source;
}

inline void
RegExpGuard::release()
{
    if (re_) {
        re_->decRef();
        re_ = NULL;
        source_ = NULL;
    }
}

inline void
MatchPairs::checkAgainst(size_t inputLength)
{
#ifdef DEBUG
    for (size_t i = 0; i < pairCount_; i++) {
        const MatchPair &p = pair(i);
        JS_ASSERT(p.check());
        if (p.isUndefined())
            continue;
        JS_ASSERT(size_t(p.limit) <= inputLength);
    }
#endif
}

} /* namespace js */

#endif
