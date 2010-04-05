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

#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsfun.h"
#include "jsobj.h"
#include "jsscope.h"

inline JSEmptyScope *
JSScope::createEmptyScope(JSContext *cx, JSClass *clasp)
{
    JS_ASSERT(!emptyScope);
    emptyScope = cx->create<JSEmptyScope>(cx, ops, clasp);
    return emptyScope;
}

inline JSEmptyScope *
JSScope::getEmptyScope(JSContext *cx, JSClass *clasp)
{
    if (emptyScope) {
        JS_ASSERT(clasp == emptyScope->clasp);
        emptyScope->hold();
        return emptyScope;
    }
    return createEmptyScope(cx, clasp);
}

inline bool
JSScope::ensureEmptyScope(JSContext *cx, JSClass *clasp)
{
    if (emptyScope) {
        JS_ASSERT(clasp == emptyScope->clasp);
        return true;
    }
    if (!createEmptyScope(cx, clasp))
        return false;

    /* We are going to have only single ref to the scope. */
    JS_ASSERT(emptyScope->nrefs == 2);
    emptyScope->nrefs = 1;
    return true;
}

inline void
JSScope::updateShape(JSContext *cx)
{
    JS_ASSERT(object);
    js::LeaveTraceIfGlobalObject(cx, object);
    shape = (hasOwnShape() || !lastProp) ? js_GenerateShape(cx, false) : lastProp->shape;
}

inline void
JSScope::updateFlags(const JSScopeProperty *sprop)
{
    jsuint index;
    if (js_IdIsIndex(sprop->id, &index))
        setIndexedProperties();

    if (sprop->isMethod())
        setMethodBarrier();
}

inline void
JSScope::extend(JSContext *cx, JSScopeProperty *sprop)
{
    ++entryCount;
    setLastProperty(sprop);
    updateShape(cx);
    updateFlags(sprop);
}

/*
 * Property read barrier for deferred cloning of compiler-created function
 * objects optimized as typically non-escaping, ad-hoc methods in obj.
 */
inline bool
JSScope::methodReadBarrier(JSContext *cx, JSScopeProperty *sprop, jsval *vp)
{
    JS_ASSERT(hasMethodBarrier());
    JS_ASSERT(hasProperty(sprop));
    JS_ASSERT(sprop->isMethod());
    JS_ASSERT(sprop->methodValue() == *vp);
    JS_ASSERT(object->getClass() == &js_ObjectClass);

    JSObject *funobj = JSVAL_TO_OBJECT(*vp);
    JSFunction *fun = GET_FUNCTION_PRIVATE(cx, funobj);
    JS_ASSERT(FUN_OBJECT(fun) == funobj && FUN_NULL_CLOSURE(fun));

    funobj = CloneFunctionObject(cx, fun, funobj->getParent());
    if (!funobj)
        return false;
    *vp = OBJECT_TO_JSVAL(funobj);
    return !!js_SetPropertyHelper(cx, object, sprop->id, 0, vp);
}

inline bool
JSScope::methodWriteBarrier(JSContext *cx, JSScopeProperty *sprop, jsval v)
{
    if (flags & (BRANDED | METHOD_BARRIER)) {
        jsval prev = LOCKED_OBJ_GET_SLOT(object, sprop->slot);

        if (prev != v && VALUE_IS_FUNCTION(cx, prev))
            return methodShapeChange(cx, sprop, v);
    }
    return true;
}

inline bool
JSScope::methodWriteBarrier(JSContext *cx, uint32 slot, jsval v)
{
    if (flags & (BRANDED | METHOD_BARRIER)) {
        jsval prev = LOCKED_OBJ_GET_SLOT(object, slot);

        if (prev != v && VALUE_IS_FUNCTION(cx, prev))
            return methodShapeChange(cx, slot, v);
    }
    return true;
}

inline void
JSScope::trace(JSTracer *trc)
{
    JSContext *cx = trc->context;
    JSScopeProperty *sprop = lastProp;
    uint8 regenFlag = cx->runtime->gcRegenShapesScopeFlag;

    if (IS_GC_MARKING_TRACER(trc) && cx->runtime->gcRegenShapes && !hasRegenFlag(regenFlag)) {
        /*
         * Either this scope has its own shape, which must be regenerated, or
         * it must have the same shape as lastProp.
         */
        uint32 newShape;

        if (sprop) {
            if (!sprop->hasRegenFlag()) {
                sprop->shape = js_RegenerateShapeForGC(cx);
                sprop->setRegenFlag();
            }
            newShape = sprop->shape;
        }
        if (!sprop || hasOwnShape()) {
            newShape = js_RegenerateShapeForGC(cx);
            JS_ASSERT_IF(sprop, newShape != sprop->shape);
        }
        shape = newShape;
        flags ^= JSScope::SHAPE_REGEN;

        /* Also regenerate the shapes of this scope's empty scope, if there is one. */
        JSScope *empty = emptyScope;
        if (empty) {
            JS_ASSERT(!empty->emptyScope);
            if (!empty->hasRegenFlag(regenFlag)) {
                uint32 newEmptyShape = js_RegenerateShapeForGC(cx);

                JS_PROPERTY_TREE(cx).emptyShapeChange(empty->shape, newEmptyShape);
                empty->shape = newEmptyShape;
                empty->flags ^= JSScope::SHAPE_REGEN;
            }
        }
    }

    if (sprop) {
        JS_ASSERT(hasProperty(sprop));

        /* Trace scope's property tree ancestor line. */
        do {
            sprop->trace(trc);
        } while ((sprop = sprop->parent) != NULL);
    }
}

inline JSDHashNumber
JSScopeProperty::hash() const
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
    hash = JS_ROTATE_LEFT32(hash, 4) ^ id;
    return hash;
}

inline bool
JSScopeProperty::matches(const JSScopeProperty *p) const
{
    JS_ASSERT(!JSVAL_IS_NULL(id));
    JS_ASSERT(!JSVAL_IS_NULL(p->id));
    return id == p->id &&
           matchesParamsAfterId(p->rawGetter, p->rawSetter, p->slot, p->attrs, p->flags,
                                p->shortid);
}

inline bool
JSScopeProperty::matchesParamsAfterId(JSPropertyOp agetter, JSPropertyOp asetter, uint32 aslot,
                                      uintN aattrs, uintN aflags, intN ashortid) const
{
    JS_ASSERT(!JSVAL_IS_NULL(id));
    return rawGetter == agetter &&
           rawSetter == asetter &&
           slot == aslot &&
           attrs == aattrs &&
           ((flags ^ aflags) & PUBLIC_FLAGS) == 0 &&
           shortid == ashortid;
}

#endif /* jsscopeinlines_h___ */
