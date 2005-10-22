/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
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

#ifndef jsobj_h___
#define jsobj_h___
/*
 * JS object definitions.
 *
 * A JS object consists of a possibly-shared object descriptor containing
 * ordered property names, called the map; and a dense vector of property
 * values, called slots.  The map/slot pointer pair is GC'ed, while the map
 * is reference counted and the slot vector is malloc'ed.
 */
#include "jshash.h" /* Added by JSIFY */
#include "jsprvtd.h"
#include "jspubtd.h"

JS_BEGIN_EXTERN_C

struct JSObjectMap {
    jsrefcount  nrefs;          /* count of all referencing objects */
    JSObjectOps *ops;           /* high level object operation vtable */
    uint32      nslots;         /* length of obj->slots vector */
    uint32      freeslot;       /* index of next free obj->slots element */
};

/* Shorthand macros for frequently-made calls. */
#define OBJ_LOOKUP_PROPERTY(cx,obj,id,objp,propp)                             \
    (obj)->map->ops->lookupProperty(cx,obj,id,objp,propp)
#define OBJ_DEFINE_PROPERTY(cx,obj,id,value,getter,setter,attrs,propp)        \
    (obj)->map->ops->defineProperty(cx,obj,id,value,getter,setter,attrs,propp)
#define OBJ_GET_PROPERTY(cx,obj,id,vp)                                        \
    (obj)->map->ops->getProperty(cx,obj,id,vp)
#define OBJ_SET_PROPERTY(cx,obj,id,vp)                                        \
    (obj)->map->ops->setProperty(cx,obj,id,vp)
#define OBJ_GET_ATTRIBUTES(cx,obj,id,prop,attrsp)                             \
    (obj)->map->ops->getAttributes(cx,obj,id,prop,attrsp)
#define OBJ_SET_ATTRIBUTES(cx,obj,id,prop,attrsp)                             \
    (obj)->map->ops->setAttributes(cx,obj,id,prop,attrsp)
#define OBJ_DELETE_PROPERTY(cx,obj,id,rval)                                   \
    (obj)->map->ops->deleteProperty(cx,obj,id,rval)
#define OBJ_DEFAULT_VALUE(cx,obj,hint,vp)                                     \
    (obj)->map->ops->defaultValue(cx,obj,hint,vp)
#define OBJ_ENUMERATE(cx,obj,enum_op,statep,idp)                              \
    (obj)->map->ops->enumerate(cx,obj,enum_op,statep,idp)
#define OBJ_CHECK_ACCESS(cx,obj,id,mode,vp,attrsp)                            \
    (obj)->map->ops->checkAccess(cx,obj,id,mode,vp,attrsp)

/* These four are time-optimized to avoid stub calls. */
#define OBJ_THIS_OBJECT(cx,obj)                                               \
    ((obj)->map->ops->thisObject                                              \
     ? (obj)->map->ops->thisObject(cx,obj)                                    \
     : (obj))
#define OBJ_DROP_PROPERTY(cx,obj,prop)                                        \
    ((obj)->map->ops->dropProperty                                            \
     ? (obj)->map->ops->dropProperty(cx,obj,prop)                             \
     : (void)0)
#define OBJ_GET_REQUIRED_SLOT(cx,obj,slot)                                    \
    ((obj)->map->ops->getRequiredSlot                                         \
     ? (obj)->map->ops->getRequiredSlot(cx, obj, slot)                        \
     : JSVAL_VOID)
#define OBJ_SET_REQUIRED_SLOT(cx,obj,slot,v)                                  \
    ((obj)->map->ops->setRequiredSlot                                         \
     ? (obj)->map->ops->setRequiredSlot(cx, obj, slot, v)                     \
     : JS_TRUE)

#define OBJ_TO_INNER_OBJECT(cx,obj)                                           \
    JS_BEGIN_MACRO                                                            \
        JSClass *clasp_ = OBJ_GET_CLASS(cx, obj);                             \
        if (clasp_->flags & JSCLASS_IS_EXTENDED) {                            \
            JSExtendedClass *xclasp_ = (JSExtendedClass*)clasp_;              \
            if (xclasp_->innerObject)                                         \
                obj = xclasp_->innerObject(cx, obj);                          \
        }                                                                     \
    JS_END_MACRO

/*
 * In the original JS engine design, obj->slots pointed to a vector of length
 * JS_INITIAL_NSLOTS words if obj->map was shared with a prototype object,
 * else of length obj->map->nslots.  With the advent of JS_GetReservedSlot,
 * JS_SetReservedSlot, and JSCLASS_HAS_RESERVED_SLOTS (see jsapi.h), the size
 * of the minimum length slots vector in the case where map is shared cannot
 * be constant.  This length starts at JS_INITIAL_NSLOTS, but may advance to
 * include all the reserved slots.
 *
 * Therefore slots must be self-describing.  Rather than tag its low order bit
 * (a bit is all we need) to distinguish initial length from reserved length,
 * we do "the BSTR thing": over-allocate slots by one jsval, and store the
 * *net* length (counting usable slots, which have non-negative obj->slots[]
 * indices) in obj->slots[-1].  All code that sets obj->slots must be aware of
 * this hack -- you have been warned, and jsobj.c has been updated!
 */
struct JSObject {
    JSObjectMap *map;
    jsval       *slots;
};

#define JSSLOT_PROTO        0
#define JSSLOT_PARENT       1
#define JSSLOT_CLASS        2
#define JSSLOT_PRIVATE      3
#define JSSLOT_START(clasp) (((clasp)->flags & JSCLASS_HAS_PRIVATE)           \
                             ? JSSLOT_PRIVATE + 1                             \
                             : JSSLOT_CLASS + 1)

#define JSSLOT_FREE(clasp)  (JSSLOT_START(clasp)                              \
                             + JSCLASS_RESERVED_SLOTS(clasp))

#define JS_INITIAL_NSLOTS   5

#ifdef DEBUG
#define MAP_CHECK_SLOT(map,slot) \
    JS_ASSERT((uint32)slot < JS_MIN((map)->freeslot, (map)->nslots))
#define OBJ_CHECK_SLOT(obj,slot) \
    MAP_CHECK_SLOT((obj)->map, slot)
#else
#define OBJ_CHECK_SLOT(obj,slot) ((void)0)
#endif

/* Fast macros for accessing obj->slots while obj is locked (if thread-safe). */
#define LOCKED_OBJ_GET_SLOT(obj,slot) \
    (OBJ_CHECK_SLOT(obj, slot), (obj)->slots[slot])
#define LOCKED_OBJ_SET_SLOT(obj,slot,value) \
    (OBJ_CHECK_SLOT(obj, slot), (obj)->slots[slot] = (value))
#define LOCKED_OBJ_GET_PROTO(obj) \
    JSVAL_TO_OBJECT(LOCKED_OBJ_GET_SLOT(obj, JSSLOT_PROTO))
#define LOCKED_OBJ_GET_CLASS(obj) \
    ((JSClass *)JSVAL_TO_PRIVATE(LOCKED_OBJ_GET_SLOT(obj, JSSLOT_CLASS)))

#ifdef JS_THREADSAFE

/* Thread-safe functions and wrapper macros for accessing obj->slots. */
#define OBJ_GET_SLOT(cx,obj,slot)                                             \
    (OBJ_CHECK_SLOT(obj, slot),                                               \
     (OBJ_IS_NATIVE(obj) && OBJ_SCOPE(obj)->ownercx == cx)                    \
     ? LOCKED_OBJ_GET_SLOT(obj, slot)                                         \
     : js_GetSlotThreadSafe(cx, obj, slot))

#define OBJ_SET_SLOT(cx,obj,slot,value)                                       \
    (OBJ_CHECK_SLOT(obj, slot),                                               \
     (OBJ_IS_NATIVE(obj) && OBJ_SCOPE(obj)->ownercx == cx)                    \
     ? (void) LOCKED_OBJ_SET_SLOT(obj, slot, value)                           \
     : js_SetSlotThreadSafe(cx, obj, slot, value))

/*
 * If thread-safe, define an OBJ_GET_SLOT wrapper that bypasses, for a native
 * object, the lock-free "fast path" test of (OBJ_SCOPE(obj)->ownercx == cx),
 * to avoid needlessly switching from lock-free to lock-full scope when doing
 * GC on a different context from the last one to own the scope.  The caller
 * in this case is probably a JSClass.mark function, e.g., fun_mark, or maybe
 * a finalizer.
 *
 * The GC runs only when all threads except the one on which the GC is active
 * are suspended at GC-safe points, so there is no hazard in directly accessing
 * obj->slots[slot] from the GC's thread, once rt->gcRunning has been set.  See
 * jsgc.c for details.
 */
#define THREAD_IS_RUNNING_GC(rt, thread)                                      \
    ((rt)->gcRunning && (rt)->gcThread == (thread))

#define CX_THREAD_IS_RUNNING_GC(cx)                                           \
    THREAD_IS_RUNNING_GC((cx)->runtime, (cx)->thread)

#define GC_AWARE_GET_SLOT(cx, obj, slot)                                      \
    ((OBJ_IS_NATIVE(obj) && CX_THREAD_IS_RUNNING_GC(cx))                      \
     ? (obj)->slots[slot]                                                     \
     : OBJ_GET_SLOT(cx, obj, slot))

#else   /* !JS_THREADSAFE */

#define OBJ_GET_SLOT(cx,obj,slot)       LOCKED_OBJ_GET_SLOT(obj,slot)
#define OBJ_SET_SLOT(cx,obj,slot,value) LOCKED_OBJ_SET_SLOT(obj,slot,value)
#define GC_AWARE_GET_SLOT(cx,obj,slot)  LOCKED_OBJ_GET_SLOT(obj,slot)

#endif /* !JS_THREADSAFE */

/* Thread-safe proto, parent, and class access macros. */
#define OBJ_GET_PROTO(cx,obj) \
    JSVAL_TO_OBJECT(OBJ_GET_SLOT(cx, obj, JSSLOT_PROTO))
#define OBJ_SET_PROTO(cx,obj,proto) \
    OBJ_SET_SLOT(cx, obj, JSSLOT_PROTO, OBJECT_TO_JSVAL(proto))

#define OBJ_GET_PARENT(cx,obj) \
    JSVAL_TO_OBJECT(OBJ_GET_SLOT(cx, obj, JSSLOT_PARENT))
#define OBJ_SET_PARENT(cx,obj,parent) \
    OBJ_SET_SLOT(cx, obj, JSSLOT_PARENT, OBJECT_TO_JSVAL(parent))

#define OBJ_GET_CLASS(cx,obj) \
    ((JSClass *)JSVAL_TO_PRIVATE(OBJ_GET_SLOT(cx, obj, JSSLOT_CLASS)))

/* Test whether a map or object is native. */
#define MAP_IS_NATIVE(map)                                                    \
    ((map)->ops == &js_ObjectOps ||                                           \
     ((map)->ops && (map)->ops->newObjectMap == js_ObjectOps.newObjectMap))

#define OBJ_IS_NATIVE(obj)  MAP_IS_NATIVE((obj)->map)

extern JS_FRIEND_DATA(JSObjectOps) js_ObjectOps;
extern JS_FRIEND_DATA(JSObjectOps) js_WithObjectOps;
extern JSClass  js_ObjectClass;
extern JSClass  js_WithClass;

struct JSSharpObjectMap {
    jsrefcount  depth;
    jsatomid    sharpgen;
    JSHashTable *table;
};

#define SHARP_BIT       ((jsatomid) 1)
#define BUSY_BIT        ((jsatomid) 2)
#define SHARP_ID_SHIFT  2
#define IS_SHARP(he)    (JS_PTR_TO_UINT32((he)->value) & SHARP_BIT)
#define MAKE_SHARP(he)  ((he)->value = JS_UINT32_TO_PTR(JS_PTR_TO_UINT32((he)->value)|SHARP_BIT))
#define IS_BUSY(he)     (JS_PTR_TO_UINT32((he)->value) & BUSY_BIT)
#define MAKE_BUSY(he)   ((he)->value = JS_UINT32_TO_PTR(JS_PTR_TO_UINT32((he)->value)|BUSY_BIT))
#define CLEAR_BUSY(he)  ((he)->value = JS_UINT32_TO_PTR(JS_PTR_TO_UINT32((he)->value)&~BUSY_BIT))

extern JSHashEntry *
js_EnterSharpObject(JSContext *cx, JSObject *obj, JSIdArray **idap,
                    jschar **sp);

extern void
js_LeaveSharpObject(JSContext *cx, JSIdArray **idap);

extern JSBool
js_obj_toSource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval);

extern JSBool
js_obj_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval);

extern JSBool
js_HasOwnPropertyHelper(JSContext *cx, JSObject *obj, JSLookupPropOp lookup,
                        uintN argc, jsval *argv, jsval *rval);

extern JSObject *
js_InitObjectClass(JSContext *cx, JSObject *obj);

/* Select Object.prototype method names shared between jsapi.c and jsobj.c. */
extern const char js_watch_str[];
extern const char js_unwatch_str[];
extern const char js_hasOwnProperty_str[];
extern const char js_isPrototypeOf_str[];
extern const char js_propertyIsEnumerable_str[];
extern const char js_defineGetter_str[];
extern const char js_defineSetter_str[];
extern const char js_lookupGetter_str[];
extern const char js_lookupSetter_str[];

extern void
js_InitObjectMap(JSObjectMap *map, jsrefcount nrefs, JSObjectOps *ops,
                 JSClass *clasp);

extern JSObjectMap *
js_NewObjectMap(JSContext *cx, jsrefcount nrefs, JSObjectOps *ops,
                JSClass *clasp, JSObject *obj);

extern void
js_DestroyObjectMap(JSContext *cx, JSObjectMap *map);

extern JSObjectMap *
js_HoldObjectMap(JSContext *cx, JSObjectMap *map);

extern JSObjectMap *
js_DropObjectMap(JSContext *cx, JSObjectMap *map, JSObject *obj);

extern JSObject *
js_NewObject(JSContext *cx, JSClass *clasp, JSObject *proto, JSObject *parent);

extern JSBool
js_FindConstructor(JSContext *cx, JSObject *start, const char *name, jsval *vp);

extern JSObject *
js_ConstructObject(JSContext *cx, JSClass *clasp, JSObject *proto,
                   JSObject *parent, uintN argc, jsval *argv);

extern void
js_FinalizeObject(JSContext *cx, JSObject *obj);

extern JSBool
js_AllocSlot(JSContext *cx, JSObject *obj, uint32 *slotp);

extern void
js_FreeSlot(JSContext *cx, JSObject *obj, uint32 slot);

/*
 * Native property add and lookup variants that hide id in the hidden atom
 * subspace, so as to avoid collisions between internal properties such as
 * formal arguments and local variables in function objects, and externally
 * set properties with the same ids.
 */
extern JSScopeProperty *
js_AddHiddenProperty(JSContext *cx, JSObject *obj, jsid id,
                     JSPropertyOp getter, JSPropertyOp setter, uint32 slot,
                     uintN attrs, uintN flags, intN shortid);

extern JSBool
js_LookupHiddenProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                        JSProperty **propp);

/*
 * Find or create a property named by id in obj's scope, with the given getter
 * and setter, slot, attributes, and other members.
 */
extern JSScopeProperty *
js_AddNativeProperty(JSContext *cx, JSObject *obj, jsid id,
                     JSPropertyOp getter, JSPropertyOp setter, uint32 slot,
                     uintN attrs, uintN flags, intN shortid);

/*
 * Change sprop to have the given attrs, getter, and setter in scope, morphing
 * it into a potentially new JSScopeProperty.  Return a pointer to the changed
 * or identical property.
 */
extern JSScopeProperty *
js_ChangeNativePropertyAttrs(JSContext *cx, JSObject *obj,
                             JSScopeProperty *sprop, uintN attrs, uintN mask,
                             JSPropertyOp getter, JSPropertyOp setter);

/*
 * On error, return false.  On success, if propp is non-null, return true with
 * obj locked and with a held property in *propp; if propp is null, return true
 * but release obj's lock first.  Therefore all callers who pass non-null propp
 * result parameters must later call OBJ_DROP_PROPERTY(cx, obj, *propp) both to
 * drop the held property, and to release the lock on obj.
 */
extern JSBool
js_DefineProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                  JSPropertyOp getter, JSPropertyOp setter, uintN attrs,
                  JSProperty **propp);

extern JSBool
js_DefineNativeProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                        JSPropertyOp getter, JSPropertyOp setter, uintN attrs,
                        uintN flags, intN shortid, JSProperty **propp);

/*
 * Unlike js_DefineProperty, propp must be non-null.  On success, and if id was
 * found, return true with *objp non-null and locked, and with a held property
 * stored in *propp.  If successful but id was not found, return true with both
 * *objp and *propp null.  Therefore all callers who receive a non-null *propp
 * must later call OBJ_DROP_PROPERTY(cx, *objp, *propp).
 */
extern JS_FRIEND_API(JSBool)
js_LookupProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                  JSProperty **propp);

/*
 * Specialized subroutine that allows caller to preset JSRESOLVE_* flags.
 */
extern JSBool
js_LookupPropertyWithFlags(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                           JSObject **objp, JSProperty **propp);

extern JS_FRIEND_API(JSBool)
js_FindProperty(JSContext *cx, jsid id, JSObject **objp, JSObject **pobjp,
                JSProperty **propp);

extern JSObject *
js_FindIdentifierBase(JSContext *cx, jsid id);

extern JSObject *
js_FindVariableScope(JSContext *cx, JSFunction **funp);

extern JSBool
js_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

extern JSBool
js_SetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

extern JSBool
js_GetAttributes(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
                 uintN *attrsp);

extern JSBool
js_SetAttributes(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
                 uintN *attrsp);

extern JSBool
js_DeleteProperty(JSContext *cx, JSObject *obj, jsid id, jsval *rval);

extern JSBool
js_DefaultValue(JSContext *cx, JSObject *obj, JSType hint, jsval *vp);

extern JSIdArray *
js_NewIdArray(JSContext *cx, jsint length);

/*
 * Unlike realloc(3), this function frees ida on failure.
 */
extern JSIdArray *
js_SetIdArrayLength(JSContext *cx, JSIdArray *ida, jsint length);

extern JSBool
js_Enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
             jsval *statep, jsid *idp);

extern JSBool
js_CheckAccess(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
               jsval *vp, uintN *attrsp);

extern JSBool
js_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

extern JSBool
js_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval);

extern JSBool
js_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp);

extern JSBool
js_SetProtoOrParent(JSContext *cx, JSObject *obj, uint32 slot, JSObject *pobj);

extern JSBool
js_IsDelegate(JSContext *cx, JSObject *obj, jsval v, JSBool *bp);

extern JSBool
js_GetClassPrototype(JSContext *cx, const char *name, JSObject **protop);

extern JSBool
js_SetClassPrototype(JSContext *cx, JSObject *ctor, JSObject *proto,
                     uintN attrs);

extern JSBool
js_ValueToObject(JSContext *cx, jsval v, JSObject **objp);

extern JSObject *
js_ValueToNonNullObject(JSContext *cx, jsval v);

extern JSBool
js_TryValueOf(JSContext *cx, JSObject *obj, JSType type, jsval *rval);

extern JSBool
js_TryMethod(JSContext *cx, JSObject *obj, JSAtom *atom,
             uintN argc, jsval *argv, jsval *rval);

extern JSBool
js_XDRObject(JSXDRState *xdr, JSObject **objp);

extern uint32
js_Mark(JSContext *cx, JSObject *obj, void *arg);

extern void
js_Clear(JSContext *cx, JSObject *obj);

extern jsval
js_GetRequiredSlot(JSContext *cx, JSObject *obj, uint32 slot);

extern JSBool
js_SetRequiredSlot(JSContext *cx, JSObject *obj, uint32 slot, jsval v);

extern JSObject *
js_CheckScopeChainValidity(JSContext *cx, JSObject *scopeobj, const char *caller);

extern JSBool
js_CheckPrincipalsAccess(JSContext *cx, JSObject *scopeobj,
                         JSPrincipals *principals, const char *caller);
JS_END_EXTERN_C

#endif /* jsobj_h___ */
