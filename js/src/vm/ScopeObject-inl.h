/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ScopeObject_inl_h
#define vm_ScopeObject_inl_h

#include "vm/ScopeObject.h"

#include "jsinferinlines.h"

namespace js {

inline void
ScopeObject::setAliasedVar(JSContext *cx, ScopeCoordinate sc, PropertyName *name, const Value &v)
{
    JS_ASSERT(is<CallObject>() || is<ClonedBlockObject>());
    JS_STATIC_ASSERT(CallObject::RESERVED_SLOTS == BlockObject::RESERVED_SLOTS);

    // name may be null for non-singletons, whose types do not need to be tracked.
    JS_ASSERT_IF(hasSingletonType(), name);

    setSlot(sc.slot, v);
    if (hasSingletonType())
        types::AddTypePropertyId(cx, this, NameToId(name), v);
}

inline void
CallObject::setAliasedVar(JSContext *cx, AliasedFormalIter fi, PropertyName *name, const Value &v)
{
    JS_ASSERT(name == fi->name());
    setSlot(fi.scopeSlot(), v);
    if (hasSingletonType())
        types::AddTypePropertyId(cx, this, NameToId(name), v);
}

}  /* namespace js */

#endif /* vm_ScopeObject_inl_h */
