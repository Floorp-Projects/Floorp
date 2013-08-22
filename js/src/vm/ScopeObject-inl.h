/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ScopeObject_inl_h
#define vm_ScopeObject_inl_h

#include "vm/ScopeObject.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"

namespace js {

inline void
ScopeObject::setEnclosingScope(HandleObject obj)
{
    JS_ASSERT_IF(obj->is<CallObject>() || obj->is<DeclEnvObject>() || obj->is<BlockObject>(),
                 obj->isDelegate());
    setFixedSlot(SCOPE_CHAIN_SLOT, ObjectValue(*obj));
}

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

inline void
BlockObject::setSlotValue(unsigned i, const Value &v)
{
    setSlot(RESERVED_SLOTS + i, v);
}

inline void
StaticBlockObject::initPrevBlockChainFromParser(StaticBlockObject *prev)
{
    setReservedSlot(SCOPE_CHAIN_SLOT, ObjectOrNullValue(prev));
}

inline void
StaticBlockObject::resetPrevBlockChainFromParser()
{
    setReservedSlot(SCOPE_CHAIN_SLOT, UndefinedValue());
}

inline void
StaticBlockObject::initEnclosingStaticScope(JSObject *obj)
{
    JS_ASSERT(getReservedSlot(SCOPE_CHAIN_SLOT).isUndefined());
    setReservedSlot(SCOPE_CHAIN_SLOT, ObjectOrNullValue(obj));
}

inline void
StaticBlockObject::setStackDepth(uint32_t depth)
{
    JS_ASSERT(getReservedSlot(DEPTH_SLOT).isUndefined());
    initReservedSlot(DEPTH_SLOT, PrivateUint32Value(depth));
}

inline void
StaticBlockObject::setDefinitionParseNode(unsigned i, frontend::Definition *def)
{
    JS_ASSERT(slotValue(i).isUndefined());
    setSlotValue(i, PrivateValue(def));
}

inline void
StaticBlockObject::setAliased(unsigned i, bool aliased)
{
    JS_ASSERT_IF(i > 0, slotValue(i-1).isBoolean());
    setSlotValue(i, BooleanValue(aliased));
    if (aliased && !needsClone()) {
        setSlotValue(0, MagicValue(JS_BLOCK_NEEDS_CLONE));
        JS_ASSERT(needsClone());
    }
}

inline void
ClonedBlockObject::setVar(unsigned i, const Value &v, MaybeCheckAliasing checkAliasing)
{
    JS_ASSERT_IF(checkAliasing, staticBlock().isAliased(i));
    setSlotValue(i, v);
}

}  /* namespace js */

#endif /* vm_ScopeObject_inl_h */
