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
 * The Original Code is SpiderMonkey call object code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Paul Biggar <pbiggar@mozilla.com> (original author)
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

#ifndef ScopeObject_inl_h___
#define ScopeObject_inl_h___

#include "ScopeObject.h"

namespace js {

inline
ScopeCoordinate::ScopeCoordinate(jsbytecode *pc)
  : hops(GET_UINT16(pc)), binding(GET_UINT16(pc + 2))
{
    JS_ASSERT(JOF_OPTYPE(*pc) == JOF_SCOPECOORD);
}

inline JSAtom *
ScopeCoordinateAtom(JSScript *script, jsbytecode *pc)
{
    JS_ASSERT(JOF_OPTYPE(*pc) == JOF_SCOPECOORD);
    return script->getAtom(GET_UINT32_INDEX(pc + 2 * sizeof(uint16_t)));
}

inline JSObject &
ScopeObject::enclosingScope() const
{
    return getReservedSlot(SCOPE_CHAIN_SLOT).toObject();
}

inline bool
ScopeObject::setEnclosingScope(JSContext *cx, HandleObject obj)
{
    RootedVarObject self(cx, this);
    if (!obj->setDelegate(cx))
        return false;
    self->setFixedSlot(SCOPE_CHAIN_SLOT, ObjectValue(*obj));
    return true;
}

inline StackFrame *
ScopeObject::maybeStackFrame() const
{
    JS_ASSERT(!isStaticBlock());
    return reinterpret_cast<StackFrame *>(JSObject::getPrivate());
}

inline void
ScopeObject::setStackFrame(StackFrame *frame)
{
    return setPrivate(frame);
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

inline void
CallObject::setCallee(JSObject *callee)
{
    JS_ASSERT_IF(callee, callee->isFunction());
    setFixedSlot(CALLEE_SLOT, ObjectOrNullValue(callee));
}

inline JSObject *
CallObject::getCallee() const
{
    return getReservedSlot(CALLEE_SLOT).toObjectOrNull();
}

inline JSFunction *
CallObject::getCalleeFunction() const
{
    return getReservedSlot(CALLEE_SLOT).toObject().toFunction();
}

inline const Value &
CallObject::arg(unsigned i) const
{
    JS_ASSERT(i < getCalleeFunction()->nargs);
    return getSlot(RESERVED_SLOTS + i);
}

inline void
CallObject::setArg(unsigned i, const Value &v)
{
    JS_ASSERT(i < getCalleeFunction()->nargs);
    setSlot(RESERVED_SLOTS + i, v);
}

inline void
CallObject::initArgUnchecked(unsigned i, const Value &v)
{
    JS_ASSERT(i < getCalleeFunction()->nargs);
    initSlotUnchecked(RESERVED_SLOTS + i, v);
}

inline const Value &
CallObject::var(unsigned i) const
{
    JSFunction *fun = getCalleeFunction();
    JS_ASSERT(fun->nargs == fun->script()->bindings.numArgs());
    JS_ASSERT(i < fun->script()->bindings.numVars());
    return getSlot(RESERVED_SLOTS + fun->nargs + i);
}

inline void
CallObject::setVar(unsigned i, const Value &v)
{
    JSFunction *fun = getCalleeFunction();
    JS_ASSERT(fun->nargs == fun->script()->bindings.numArgs());
    JS_ASSERT(i < fun->script()->bindings.numVars());
    setSlot(RESERVED_SLOTS + fun->nargs + i, v);
}

inline void
CallObject::initVarUnchecked(unsigned i, const Value &v)
{
    JSFunction *fun = getCalleeFunction();
    JS_ASSERT(fun->nargs == fun->script()->bindings.numArgs());
    JS_ASSERT(i < fun->script()->bindings.numVars());
    initSlotUnchecked(RESERVED_SLOTS + fun->nargs + i, v);
}

inline void
CallObject::copyValues(unsigned nargs, Value *argv, unsigned nvars, Value *slots)
{
    JS_ASSERT(slotInRange(RESERVED_SLOTS + nargs + nvars, SENTINEL_ALLOWED));
    copySlotRange(RESERVED_SLOTS, argv, nargs);
    copySlotRange(RESERVED_SLOTS + nargs, slots, nvars);
}

inline HeapSlotArray
CallObject::argArray()
{
    DebugOnly<JSFunction*> fun = getCalleeFunction();
    JS_ASSERT(hasContiguousSlots(RESERVED_SLOTS, fun->nargs));
    return HeapSlotArray(getSlotAddress(RESERVED_SLOTS));
}

inline HeapSlotArray
CallObject::varArray()
{
    JSFunction *fun = getCalleeFunction();
    JS_ASSERT(hasContiguousSlots(RESERVED_SLOTS + fun->nargs,
                                 fun->script()->bindings.numVars()));
    return HeapSlotArray(getSlotAddress(RESERVED_SLOTS + fun->nargs));
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

inline HeapSlot &
BlockObject::slotValue(unsigned i)
{
    JS_ASSERT(i < slotCount());
    return getSlotRef(RESERVED_SLOTS + i);
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
    slotValue(i).init(this, i, PrivateValue(def));
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
    slotValue(i).init(this, i, BooleanValue(aliased));
}

inline bool
StaticBlockObject::isAliased(unsigned i)
{
    return slotValue(i).isTrue();
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
ClonedBlockObject::closedSlot(unsigned i)
{
    JS_ASSERT(!maybeStackFrame());
    return slotValue(i);
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

#endif /* CallObject_inl_h___ */
