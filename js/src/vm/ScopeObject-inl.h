/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScopeObject_inl_h___
#define ScopeObject_inl_h___

#include "ScopeObject.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

namespace js {

inline
ScopeCoordinate::ScopeCoordinate(jsbytecode *pc)
  : hops(GET_UINT16(pc)), slot(GET_UINT16(pc + 2))
{
    JS_ASSERT(JOF_OPTYPE(*pc) == JOF_SCOPECOORD);
}

inline JSObject &
ScopeObject::enclosingScope() const
{
    return getReservedSlot(SCOPE_CHAIN_SLOT).toObject();
}

inline void
ScopeObject::setEnclosingScope(HandleObject obj)
{
    JS_ASSERT_IF(obj->is<CallObject>() || obj->is<DeclEnvObject>() || obj->is<BlockObject>(),
                 obj->isDelegate());
    setFixedSlot(SCOPE_CHAIN_SLOT, ObjectValue(*obj));
}

inline const Value &
ScopeObject::aliasedVar(ScopeCoordinate sc)
{
    JS_ASSERT(is<CallObject>() || isClonedBlock());
    return getSlot(sc.slot);
}

inline void
ScopeObject::setAliasedVar(JSContext *cx, ScopeCoordinate sc, PropertyName *name, const Value &v)
{
    JS_ASSERT(is<CallObject>() || isClonedBlock());
    JS_STATIC_ASSERT(CallObject::RESERVED_SLOTS == BlockObject::RESERVED_SLOTS);

    // name may be null for non-singletons, whose types do not need to be tracked.
    JS_ASSERT_IF(hasSingletonType(), name);

    setSlot(sc.slot, v);
    if (hasSingletonType())
        types::AddTypePropertyId(cx, this, NameToId(name), v);
}

/*static*/ inline size_t
ScopeObject::offsetOfEnclosingScope()
{
    return getFixedSlotOffset(SCOPE_CHAIN_SLOT);
}

inline bool
CallObject::isForEval() const
{
    JS_ASSERT(getReservedSlot(CALLEE_SLOT).isObjectOrNull());
    JS_ASSERT_IF(getReservedSlot(CALLEE_SLOT).isObject(),
                 getReservedSlot(CALLEE_SLOT).toObject().isFunction());
    return getReservedSlot(CALLEE_SLOT).isNull();
}

inline JSFunction &
CallObject::callee() const
{
    return *getReservedSlot(CALLEE_SLOT).toObject().toFunction();
}

inline const Value &
CallObject::aliasedVar(AliasedFormalIter fi)
{
    return getSlot(fi.scopeSlot());
}

inline void
CallObject::setAliasedVar(JSContext *cx, AliasedFormalIter fi, PropertyName *name, const Value &v)
{
    JS_ASSERT(name == fi->name());
    setSlot(fi.scopeSlot(), v);
    if (hasSingletonType())
        types::AddTypePropertyId(cx, this, NameToId(name), v);
}

/*static*/ inline size_t
CallObject::offsetOfCallee()
{
    return getFixedSlotOffset(CALLEE_SLOT);
}

inline uint32_t
NestedScopeObject::stackDepth() const
{
    return getReservedSlot(DEPTH_SLOT).toPrivateUint32();
}

inline JSObject &
WithObject::withThis() const
{
    return getReservedSlot(THIS_SLOT).toObject();
}

inline JSObject &
WithObject::object() const
{
    return *JSObject::getProto();
}

inline uint32_t
BlockObject::slotCount() const
{
    return propertyCount();
}

inline unsigned
BlockObject::slotToLocalIndex(const Bindings &bindings, unsigned slot)
{
    JS_ASSERT(slot < RESERVED_SLOTS + slotCount());
    return bindings.numVars() + stackDepth() + (slot - RESERVED_SLOTS);
}

inline unsigned
BlockObject::localIndexToSlot(const Bindings &bindings, unsigned i)
{
    return RESERVED_SLOTS + (i - (bindings.numVars() + stackDepth()));
}

inline const Value &
BlockObject::slotValue(unsigned i)
{
    return getSlotRef(RESERVED_SLOTS + i);
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

inline StaticBlockObject *
StaticBlockObject::enclosingBlock() const
{
    JSObject *obj = getReservedSlot(SCOPE_CHAIN_SLOT).toObjectOrNull();
    return obj && obj->isStaticBlock() ? &obj->asStaticBlock() : NULL;
}

inline JSObject *
StaticBlockObject::enclosingStaticScope() const
{
    return getReservedSlot(SCOPE_CHAIN_SLOT).toObjectOrNull();
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

inline frontend::Definition *
StaticBlockObject::maybeDefinitionParseNode(unsigned i)
{
    Value v = slotValue(i);
    return v.isUndefined() ? NULL : reinterpret_cast<frontend::Definition *>(v.toPrivate());
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

inline bool
StaticBlockObject::isAliased(unsigned i)
{
    return slotValue(i).isTrue();
}

inline bool
StaticBlockObject::needsClone()
{
    return !slotValue(0).isFalse();
}

inline bool
StaticBlockObject::containsVarAtDepth(uint32_t depth)
{
    return depth >= stackDepth() && depth < stackDepth() + slotCount();
}

inline StaticBlockObject &
ClonedBlockObject::staticBlock() const
{
    return getProto()->asStaticBlock();
}

inline const Value &
ClonedBlockObject::var(unsigned i, MaybeCheckAliasing checkAliasing)
{
    JS_ASSERT_IF(checkAliasing, staticBlock().isAliased(i));
    return slotValue(i);
}

inline void
ClonedBlockObject::setVar(unsigned i, const Value &v, MaybeCheckAliasing checkAliasing)
{
    JS_ASSERT_IF(checkAliasing, staticBlock().isAliased(i));
    setSlotValue(i, v);
}

}  /* namespace js */

inline js::ScopeObject &
JSObject::asScope()
{
    JS_ASSERT(isScope());
    return *static_cast<js::ScopeObject *>(this);
}

inline js::NestedScopeObject &
JSObject::asNestedScope()
{
    JS_ASSERT(isWith() || is<js::BlockObject>());
    return *static_cast<js::NestedScopeObject *>(this);
}

inline js::WithObject &
JSObject::asWith()
{
    JS_ASSERT(isWith());
    return *static_cast<js::WithObject *>(this);
}

inline js::StaticBlockObject &
JSObject::asStaticBlock()
{
    JS_ASSERT(isStaticBlock());
    return *static_cast<js::StaticBlockObject *>(this);
}

inline js::ClonedBlockObject &
JSObject::asClonedBlock()
{
    JS_ASSERT(isClonedBlock());
    return *static_cast<js::ClonedBlockObject *>(this);
}

inline js::DebugScopeObject &
JSObject::asDebugScope()
{
    JS_ASSERT(isDebugScope());
    return *static_cast<js::DebugScopeObject *>(this);
}

#endif /* ScopeObject_inl_h___ */
