/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScopeObject_inl_h___
#define ScopeObject_inl_h___

#include "ScopeObject.h"

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

inline bool
ScopeObject::setEnclosingScope(JSContext *cx, HandleObject obj)
{
    RootedObject self(cx, this);
    if (!obj->setDelegate(cx))
        return false;
    self->setFixedSlot(SCOPE_CHAIN_SLOT, ObjectValue(*obj));
    return true;
}

inline const Value &
ScopeObject::aliasedVar(ScopeCoordinate sc)
{
    JS_ASSERT(isCall() || isClonedBlock());
    return getSlot(sc.slot);
}

inline void
ScopeObject::setAliasedVar(ScopeCoordinate sc, const Value &v)
{
    JS_ASSERT(isCall() || isClonedBlock());
    JS_STATIC_ASSERT(CallObject::RESERVED_SLOTS == BlockObject::RESERVED_SLOTS);
    setSlot(sc.slot, v);
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
CallObject::arg(unsigned i, MaybeCheckAliasing checkAliasing) const
{
    JS_ASSERT_IF(checkAliasing, callee().script()->formalLivesInCallObject(i));
    return getSlot(RESERVED_SLOTS + i);
}

inline void
CallObject::setArg(unsigned i, const Value &v, MaybeCheckAliasing checkAliasing)
{
    JS_ASSERT_IF(checkAliasing, callee().script()->formalLivesInCallObject(i));
    setSlot(RESERVED_SLOTS + i, v);
}

inline const Value &
CallObject::var(unsigned i, MaybeCheckAliasing checkAliasing) const
{
    JSFunction &fun = callee();
    JS_ASSERT_IF(checkAliasing, fun.script()->varIsAliased(i));
    return getSlot(RESERVED_SLOTS + fun.nargs + i);
}

inline void
CallObject::setVar(unsigned i, const Value &v, MaybeCheckAliasing checkAliasing)
{
    JSFunction &fun = callee();
    JS_ASSERT_IF(checkAliasing, fun.script()->varIsAliased(i));
    setSlot(RESERVED_SLOTS + fun.nargs + i, v);
}

inline HeapSlotArray
CallObject::argArray()
{
    JSFunction &fun = callee();
    JS_ASSERT(hasContiguousSlots(RESERVED_SLOTS, fun.nargs));
    return HeapSlotArray(getSlotAddress(RESERVED_SLOTS));
}

inline HeapSlotArray
CallObject::varArray()
{
    JSFunction &fun = callee();
    JS_ASSERT(hasContiguousSlots(RESERVED_SLOTS + fun.nargs,
                                 fun.script()->bindings.numVars()));
    return HeapSlotArray(getSlotAddress(RESERVED_SLOTS + fun.nargs));
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
    JS_ASSERT(i < slotCount());
    return getSlotRef(RESERVED_SLOTS + i);
}

inline void
BlockObject::setSlotValue(unsigned i, const Value &v)
{
    JS_ASSERT(i < slotCount());
    setSlot(RESERVED_SLOTS + i, v);
}

inline StaticBlockObject *
StaticBlockObject::enclosingBlock() const
{
    JSObject *obj = getReservedSlot(SCOPE_CHAIN_SLOT).toObjectOrNull();
    return obj ? &obj->asStaticBlock() : NULL;
}

inline void
StaticBlockObject::setEnclosingBlock(StaticBlockObject *blockObj)
{
    setFixedSlot(SCOPE_CHAIN_SLOT, ObjectOrNullValue(blockObj));
}

inline void
StaticBlockObject::setStackDepth(uint32_t depth)
{
    JS_ASSERT(getReservedSlot(DEPTH_SLOT).isUndefined());
    initReservedSlot(DEPTH_SLOT, PrivateUint32Value(depth));
}

inline void
StaticBlockObject::setDefinitionParseNode(unsigned i, Definition *def)
{
    JS_ASSERT(slotValue(i).isUndefined());
    setSlotValue(i, PrivateValue(def));
}

inline Definition *
StaticBlockObject::maybeDefinitionParseNode(unsigned i)
{
    Value v = slotValue(i);
    return v.isUndefined() ? NULL : reinterpret_cast<Definition *>(v.toPrivate());
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

inline js::CallObject &
JSObject::asCall()
{
    JS_ASSERT(isCall());
    return *static_cast<js::CallObject *>(this);
}

inline js::DeclEnvObject &
JSObject::asDeclEnv()
{
    JS_ASSERT(isDeclEnv());
    return *static_cast<js::DeclEnvObject *>(this);
}

inline js::NestedScopeObject &
JSObject::asNestedScope()
{
    JS_ASSERT(isWith() || isBlock());
    return *static_cast<js::NestedScopeObject *>(this);
}

inline js::WithObject &
JSObject::asWith()
{
    JS_ASSERT(isWith());
    return *static_cast<js::WithObject *>(this);
}

inline js::BlockObject &
JSObject::asBlock()
{
    JS_ASSERT(isBlock());
    return *static_cast<js::BlockObject *>(this);
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

#endif /* CallObject_inl_h___ */
