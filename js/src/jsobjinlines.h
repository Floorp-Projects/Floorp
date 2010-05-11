/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef jsobjinlines_h___
#define jsobjinlines_h___

#include "jsobj.h"
#include "jsscope.h"

inline const js::Value &
JSObject::getSlotMT(JSContext *cx, uintN slot)
{
#ifdef JS_THREADSAFE
    /*
     * If thread-safe, define a getSlotMT() that bypasses, for a native
     * object, the lock-free "fast path" test of
     * (obj->scope()->ownercx == cx), to avoid needlessly switching from
     * lock-free to lock-full scope when doing GC on a different context
     * from the last one to own the scope.  The caller in this case is
     * probably a Class.mark function, e.g., fun_mark, or maybe a
     * finalizer.
     */
    OBJ_CHECK_SLOT(this, slot);
    return (scope()->title.ownercx == cx)
           ? this->lockedGetSlot(slot)
           : js_GetSlotThreadSafe(cx, this, slot);
#else
    return this->lockedGetSlot(slot);
#endif
}

inline void
JSObject::setSlotMT(JSContext *cx, uintN slot, const js::Value &value)
{
#ifdef JS_THREADSAFE
    /* Thread-safe way to set a slot. */
    OBJ_CHECK_SLOT(this, slot);
    if (scope()->title.ownercx == cx)
        this->lockedSetSlot(slot, value);
    else
        js_SetSlotThreadSafe(cx, this, slot, value);
#else
    this->lockedSetSlot(slot, value);
#endif
}

inline void JSObject::staticAssertArrayLengthIsInPrivateSlot()
{
    JS_STATIC_ASSERT(JSSLOT_ARRAY_LENGTH == JSSLOT_PRIVATE);
}

inline uint32
JSObject::getArrayLength() const
{
    JS_ASSERT(isArray());
    return fslots[JSSLOT_ARRAY_LENGTH].asPrivateUint32();
}

inline uint32 
JSObject::getArrayCount() const
{
    JS_ASSERT(isArray());
    return fslots[JSSLOT_ARRAY_COUNT].asPrivateUint32();
}

inline void 
JSObject::setArrayLength(uint32 length)
{
    JS_ASSERT(isArray());
    fslots[JSSLOT_ARRAY_LENGTH].setPrivateUint32(length);
}

inline void 
JSObject::setArrayCount(uint32 count)
{
    JS_ASSERT(isArray());
    fslots[JSSLOT_ARRAY_COUNT].setPrivateUint32(count);
}

inline void 
JSObject::voidDenseArrayCount()
{
    JS_ASSERT(isDenseArray());
    fslots[JSSLOT_ARRAY_COUNT].setUndefined();
}

inline void 
JSObject::incArrayCountBy(uint32 posDelta)
{
    JS_ASSERT(isArray());
    fslots[JSSLOT_ARRAY_COUNT].asPrivateUint32Ref() += posDelta;
}

inline void 
JSObject::decArrayCountBy(uint32 negDelta)
{
    JS_ASSERT(isArray());
    fslots[JSSLOT_ARRAY_COUNT].asPrivateUint32Ref() -= negDelta;
}

inline void
JSObject::initSharingEmptyScope(js::Class *clasp,
                                js::ObjPtr proto, JSObject *parent,
                                const js::Value &privateSlotValue)
{
    init(clasp, proto, parent, privateSlotValue);

    JSEmptyScope *emptyScope = proto->scope()->emptyScope;
    JS_ASSERT(emptyScope->clasp == clasp);
    emptyScope->hold();
    map = emptyScope;
}

inline void
JSObject::freeSlotsArray(JSContext *cx)
{
    JS_ASSERT(hasSlotsArray());
    JS_ASSERT(dslotLength() > JS_INITIAL_NSLOTS);
    cx->free(dslots - 1);
}

inline bool
JSObject::unbrand(JSContext *cx)
{
    if (this->isNative()) {
        JS_LOCK_OBJ(cx, this);
        JSScope *scope = this->scope();
        if (scope->isSharedEmpty()) {
            scope = js_GetMutableScope(cx, this);
            if (!scope) {
                JS_UNLOCK_OBJ(cx, this);
                return false;
            }
        }
        scope->setGeneric();
        JS_UNLOCK_SCOPE(cx, scope);
    }
    return true;
}

inline void
JSObject::initArrayClass()
{
    clasp = &js_ArrayClass;
}

inline void
JSObject::changeClassToSlowArray()
{
    JS_ASSERT(clasp == &js_ArrayClass);
    clasp = &js_SlowArrayClass;
}

inline void
JSObject::changeClassToFastArray()
{
    JS_ASSERT(clasp == &js_SlowArrayClass);
    clasp = &js_ArrayClass;
}

inline JSObject *
JSObject::thisObject(JSContext *cx)
{
    if (JSObjectOp thisOp = map->ops->thisObject)
        return thisOp(cx, this);
    return this;
}

inline js::ObjPtr
JSObject::thisObject(JSContext *cx, js::ObjPtr obj)
{
    if (JSObjectOp thisOp = obj->map->ops->thisObject) {
        JSObject *o = thisOp(cx, obj);
        if (!o)
            return js::NullObjPtr();
        SetObject(&obj, *o);
    }
    return obj;
}

namespace js {

typedef Vector<PropertyDescriptor, 1> PropertyDescriptorArray;

class AutoDescriptorArray : private AutoGCRooter
{
  public:
    AutoDescriptorArray(JSContext *cx)
      : AutoGCRooter(cx, DESCRIPTORS), descriptors(cx)
    { }

    PropertyDescriptor *append() {
        if (!descriptors.append(PropertyDescriptor()))
            return NULL;
        return &descriptors.back();
    }

    PropertyDescriptor& operator[](size_t i) {
        JS_ASSERT(i < descriptors.length());
        return descriptors[i];
    }

    friend void AutoGCRooter::trace(JSTracer *trc);

  private:
    PropertyDescriptorArray descriptors;
};

JS_ALWAYS_INLINE ObjPtr
ToObjPtr(JSObject *pobj)
{
    if (pobj)
        return NullObjPtr();
    if (pobj->isFunction())
        return FunObjPtr(*pobj);
    return NonFunObjPtr(*pobj);
}

JS_ALWAYS_INLINE ObjPtr
ToObjPtr(JSObject &obj)
{
    if (obj.isFunction())
        return FunObjPtr(obj);
    return NonFunObjPtr(obj);
}

JS_ALWAYS_INLINE Value
ToValue(JSObject *pobj)
{
    if (pobj)
        return NullValue();
    if (pobj->isFunction())
        return FunObjValue(*pobj);
    return NonFunObjValue(*pobj);
}

JS_ALWAYS_INLINE Value
ToValue(JSObject &obj)
{
    if (obj.isFunction())
        return FunObjValue(obj);
    return NonFunObjValue(obj);
}

JS_ALWAYS_INLINE void
SetObject(ObjPtr *vp, JSObject *pobj)
{
    if (!pobj)
        vp->setNull();
    else if (pobj->isFunction())
        vp->setFunObj(*pobj);
    else
        vp->setNonFunObj(*pobj);
}

JS_ALWAYS_INLINE void
SetObject(Value *vp, JSObject *pobj)
{
    if (!pobj)
        vp->setNull();
    if (pobj->isFunction())
        vp->setFunObj(*pobj);
    else
        vp->setNonFunObj(*pobj);
}

JS_ALWAYS_INLINE void
SetObject(ObjPtr *vp, JSObject &obj)
{
    if (obj.isFunction())
        vp->setFunObj(obj);
    else
        vp->setNonFunObj(obj);
}

JS_ALWAYS_INLINE void
SetObject(Value *vp, JSObject &obj)
{
    if (obj.isFunction())
        vp->setFunObj(obj);
    else
        vp->setNonFunObj(obj);
}

} /* namespace js */

#endif /* jsobjinlines_h___ */
