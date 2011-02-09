/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef jsscopeinlines_h___
#define jsscopeinlines_h___

#include <new>
#include "jsbool.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsfun.h"
#include "jsobj.h"
#include "jsscope.h"

#include "jscntxtinlines.h"

inline void
js::Shape::freeTable(JSContext *cx)
{
    if (hasTable()) {
        cx->destroy(getTable());
        setTable(NULL);
    }
}

inline js::EmptyShape *
JSObject::getEmptyShape(JSContext *cx, js::Class *aclasp,
                        /* gc::FinalizeKind */ unsigned kind)
{
    JS_ASSERT(kind >= js::gc::FINALIZE_OBJECT0 && kind <= js::gc::FINALIZE_OBJECT_LAST);
    int i = kind - js::gc::FINALIZE_OBJECT0;

    if (!emptyShapes) {
        emptyShapes = (js::EmptyShape**)
            cx->calloc(sizeof(js::EmptyShape*) * js::gc::JS_FINALIZE_OBJECT_LIMIT);
        if (!emptyShapes)
            return NULL;

        /*
         * Always fill in emptyShapes[0], so canProvideEmptyShape works.
         * Other empty shapes are filled in lazily.
         */
        emptyShapes[0] = js::EmptyShape::create(cx, aclasp);
        if (!emptyShapes[0]) {
            cx->free(emptyShapes);
            emptyShapes = NULL;
            return NULL;
        }
    }

    JS_ASSERT(aclasp == emptyShapes[0]->getClass());

    if (!emptyShapes[i]) {
        emptyShapes[i] = js::EmptyShape::create(cx, aclasp);
        if (!emptyShapes[i])
            return NULL;
    }

    return emptyShapes[i];
}

inline bool
JSObject::canProvideEmptyShape(js::Class *aclasp)
{
    return !emptyShapes || emptyShapes[0]->getClass() == aclasp;
}

inline void
JSObject::updateShape(JSContext *cx)
{
    JS_ASSERT(isNative());
    js::LeaveTraceIfGlobalObject(cx, this);
    if (hasOwnShape())
        setOwnShape(js_GenerateShape(cx));
    else
        objShape = lastProp->shape;
}

inline void
JSObject::updateFlags(const js::Shape *shape, bool isDefinitelyAtom)
{
    jsuint index;
    if (!isDefinitelyAtom && js_IdIsIndex(shape->id, &index))
        setIndexed();

    if (shape->isMethod())
        setMethodBarrier();
}

inline void
JSObject::extend(JSContext *cx, const js::Shape *shape, bool isDefinitelyAtom)
{
    setLastProperty(shape);
    updateFlags(shape, isDefinitelyAtom);
    updateShape(cx);
}

inline void
JSObject::trace(JSTracer *trc)
{
    if (!isNative())
        return;

    JSContext *cx = trc->context;
    js::Shape *shape = lastProp;

    if (IS_GC_MARKING_TRACER(trc) && cx->runtime->gcRegenShapes) {
        /*
         * Either this object has its own shape, which must be regenerated, or
         * it must have the same shape as lastProp.
         */
        if (!shape->hasRegenFlag()) {
            shape->shape = js_RegenerateShapeForGC(cx->runtime);
            shape->setRegenFlag();
        }

        uint32 newShape = shape->shape;
        if (hasOwnShape()) {
            newShape = js_RegenerateShapeForGC(cx->runtime);
            JS_ASSERT(newShape != shape->shape);
        }
        objShape = newShape;
    }

    /* Trace our property tree or dictionary ancestor line. */
    do {
        shape->trace(trc);
    } while ((shape = shape->parent) != NULL);
}

namespace js {

inline
Shape::Shape(jsid id, js::PropertyOp getter, js::StrictPropertyOp setter, uint32 slot, uintN attrs,
             uintN flags, intN shortid, uint32 shape, uint32 slotSpan)
  : JSObjectMap(shape, slotSpan),
    numLinearSearches(0), id(id), rawGetter(getter), rawSetter(setter), slot(slot),
    attrs(uint8(attrs)), flags(uint8(flags)), shortid(int16(shortid)), parent(NULL)
{
    JS_ASSERT_IF(slotSpan != SHAPE_INVALID_SLOT, slotSpan < JSObject::NSLOTS_LIMIT);
    JS_ASSERT_IF(getter && (attrs & JSPROP_GETTER), getterObj->isCallable());
    JS_ASSERT_IF(setter && (attrs & JSPROP_SETTER), setterObj->isCallable());
    kids.setNull();
}

inline
Shape::Shape(JSCompartment *comp, Class *aclasp)
  : JSObjectMap(js_GenerateShape(comp->rt), JSSLOT_FREE(aclasp)),
    numLinearSearches(0),
    id(JSID_EMPTY),
    clasp(aclasp),
    rawSetter(NULL),
    slot(SHAPE_INVALID_SLOT),
    attrs(0),
    flags(SHARED_EMPTY),
    shortid(0),
    parent(NULL)
{
    kids.setNull();
}

inline JSDHashNumber
Shape::hash() const
{
    JSDHashNumber hash = 0;

    /* Accumulate from least to most random so the low bits are most random. */
    JS_ASSERT_IF(isMethod(), !rawSetter || rawSetter == js_watch_set);
    if (rawGetter)
        hash = JS_ROTATE_LEFT32(hash, 4) ^ jsuword(rawGetter);
    if (rawSetter)
        hash = JS_ROTATE_LEFT32(hash, 4) ^ jsuword(rawSetter);
    hash = JS_ROTATE_LEFT32(hash, 4) ^ (flags & PUBLIC_FLAGS);
    hash = JS_ROTATE_LEFT32(hash, 4) ^ attrs;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ shortid;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ slot;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ JSID_BITS(id);
    return hash;
}

inline bool
Shape::matches(const js::Shape *other) const
{
    JS_ASSERT(!JSID_IS_VOID(id));
    JS_ASSERT(!JSID_IS_VOID(other->id));
    return id == other->id &&
           matchesParamsAfterId(other->rawGetter, other->rawSetter, other->slot, other->attrs,
                                other->flags, other->shortid);
}

inline bool
Shape::matchesParamsAfterId(js::PropertyOp agetter, js::StrictPropertyOp asetter, uint32 aslot,
                            uintN aattrs, uintN aflags, intN ashortid) const
{
    JS_ASSERT(!JSID_IS_VOID(id));
    return rawGetter == agetter &&
           rawSetter == asetter &&
           slot == aslot &&
           attrs == aattrs &&
           ((flags ^ aflags) & PUBLIC_FLAGS) == 0 &&
           shortid == ashortid;
}

inline bool
Shape::get(JSContext* cx, JSObject *receiver, JSObject* obj, JSObject *pobj, js::Value* vp) const
{
    JS_ASSERT(!JSID_IS_VOID(this->id));
    JS_ASSERT(!hasDefaultGetter());

    if (hasGetterValue()) {
        JS_ASSERT(!isMethod());
        js::Value fval = getterValue();
        return js::ExternalGetOrSet(cx, receiver, id, fval, JSACC_READ, 0, 0, vp);
    }

    if (isMethod()) {
        vp->setObject(methodObject());
        return pobj->methodReadBarrier(cx, *this, vp);
    }

    /*
     * |with (it) color;| ends up here, as do XML filter-expressions.
     * Avoid exposing the With object to native getters.
     */
    if (obj->getClass() == &js_WithClass)
        obj = js_UnwrapWithObject(cx, obj);
    return js::CallJSPropertyOp(cx, getterOp(), obj, SHAPE_USERID(this), vp);
}

inline bool
Shape::set(JSContext* cx, JSObject* obj, bool strict, js::Value* vp) const
{
    JS_ASSERT_IF(hasDefaultSetter(), hasGetterValue());

    if (attrs & JSPROP_SETTER) {
        js::Value fval = setterValue();
        return js::ExternalGetOrSet(cx, obj, id, fval, JSACC_WRITE, 1, vp, vp);
    }

    if (attrs & JSPROP_GETTER)
        return js_ReportGetterOnlyAssignment(cx);

    /* See the comment in js::Shape::get as to why we check for With. */
    if (obj->getClass() == &js_WithClass)
        obj = js_UnwrapWithObject(cx, obj);
    return js::CallJSPropertyOpSetter(cx, setterOp(), obj, SHAPE_USERID(this), strict, vp);
}

inline
EmptyShape::EmptyShape(JSCompartment *comp, js::Class *aclasp)
  : js::Shape(comp, aclasp)
{
#ifdef DEBUG
    if (comp->rt->meterEmptyShapes())
        comp->emptyShapes.put(this);
#endif
}

} /* namespace js */

#endif /* jsscopeinlines_h___ */
