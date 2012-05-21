/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GlobalObject_inl_h___
#define GlobalObject_inl_h___

namespace js {

inline void
GlobalObject::setFlags(int32_t flags)
{
    setSlot(FLAGS, Int32Value(flags));
}

inline void
GlobalObject::initFlags(int32_t flags)
{
    initSlot(FLAGS, Int32Value(flags));
}

inline void
GlobalObject::setDetailsForKey(JSProtoKey key, JSObject *ctor, JSObject *proto)
{
    JS_ASSERT(getSlotRef(key).isUndefined());
    JS_ASSERT(getSlotRef(JSProto_LIMIT + key).isUndefined());
    JS_ASSERT(getSlotRef(2 * JSProto_LIMIT + key).isUndefined());
    setSlot(key, ObjectValue(*ctor));
    setSlot(JSProto_LIMIT + key, ObjectValue(*proto));
    setSlot(2 * JSProto_LIMIT + key, ObjectValue(*ctor));
}

inline void
GlobalObject::setObjectClassDetails(JSFunction *ctor, JSObject *proto)
{
    setDetailsForKey(JSProto_Object, ctor, proto);
}

inline void
GlobalObject::setFunctionClassDetails(JSFunction *ctor, JSObject *proto)
{
    setDetailsForKey(JSProto_Function, ctor, proto);
}

void
GlobalObject::setThrowTypeError(JSFunction *fun)
{
    JS_ASSERT(getSlotRef(THROWTYPEERROR).isUndefined());
    setSlot(THROWTYPEERROR, ObjectValue(*fun));
}

void
GlobalObject::setOriginalEval(JSObject *evalobj)
{
    JS_ASSERT(getSlotRef(EVAL).isUndefined());
    setSlot(EVAL, ObjectValue(*evalobj));
}

} // namespace js

#endif
