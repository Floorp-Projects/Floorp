/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
#ifdef NETSCAPE_INTERNAL
#ifndef NSPR20
#include "prhash.h"
#else
#include "plhash.h"
#endif
#endif
#include "jsprvtd.h"
#include "jspubtd.h"

PR_BEGIN_EXTERN_C

struct JSObjectMap {
    jsrefcount  nrefs;          /* count of all referencing objects */
    uint32      nslots;         /* length of obj->slots vector */
    uint32      freeslot;       /* index of next free obj->slots element */
    JSClass     *clasp;         /* class name and operations */
    JSProperty  *props;         /* property list in definition order */
};

struct JSObject {
    JSObjectMap *map;
    jsval       *slots;
};

#define JSSLOT_PROTO        0
#define JSSLOT_PARENT       1
#define JSSLOT_PRIVATE      2
#define JSSLOT_START        2

#define JS_INITIAL_NSLOTS   5

/*
 * Fast get and set macros for well-known slots.  These must be called within
 * JS_LOCK(cx) and JS_UNLOCK(cx).
 */
#ifdef DEBUG
#define MAP_CHECK_SLOT(m,s) PR_ASSERT(s < PR_MAX((m)->nslots, (m)->freeslot))
#define OBJ_CHECK_SLOT(o,s) MAP_CHECK_SLOT((o)->map,s)
#else
#define OBJ_CHECK_SLOT(o,s) ((void)0)
#endif
#define OBJ_GET_SLOT(o,s)   (OBJ_CHECK_SLOT(o,s), (o)->slots[s])
#define OBJ_SET_SLOT(o,s,v) (OBJ_CHECK_SLOT(o,s), (o)->slots[s] = (v))
#define OBJ_GET_PROTO(o)    JSVAL_TO_OBJECT(OBJ_GET_SLOT(o,JSSLOT_PROTO))
#define OBJ_SET_PROTO(o,p)  OBJ_SET_SLOT(o,JSSLOT_PROTO,OBJECT_TO_JSVAL(p))
#define OBJ_GET_PARENT(o)   JSVAL_TO_OBJECT(OBJ_GET_SLOT(o,JSSLOT_PARENT))
#define OBJ_SET_PARENT(o,p) OBJ_SET_SLOT(o,JSSLOT_PARENT,OBJECT_TO_JSVAL(p))

extern JSClass js_ObjectClass;
extern JSClass js_WithClass;

struct JSSharpObjectMap {
    jsrefcount  depth;
    jsatomid    sharpgen;
    PRHashTable *table;
};

#define SHARP_BIT       1
#define IS_SHARP(he)	((jsatomid)(he)->value & SHARP_BIT)
#define MAKE_SHARP(he)  ((he)->value = (void*)((jsatomid)(he)->value|SHARP_BIT))

extern PRHashEntry *
js_EnterSharpObject(JSContext *cx, JSObject *obj, jschar **sp);

extern void
js_LeaveSharpObject(JSContext *cx);

extern JSBool
js_obj_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	        jsval *rval);

extern JSObject *
js_InitObjectClass(JSContext *cx, JSObject *obj);

extern JSObject *
js_NewObject(JSContext *cx, JSClass *clasp, JSObject *proto, JSObject *parent);

extern JSObject *
js_ConstructObject(JSContext *cx, JSClass *clasp, JSObject *proto,
		   JSObject *parent);

extern void
js_FinalizeObject(JSContext *cx, JSObject *obj);

extern jsval
js_GetSlot(JSContext *cx, JSObject *obj, uint32 slot);

extern JSBool
js_SetSlot(JSContext *cx, JSObject *obj, uint32 slot, jsval value);

extern JSBool
js_AllocSlot(JSContext *cx, JSObject *obj, uint32 *slotp);

extern void
js_FreeSlot(JSContext *cx, JSObject *obj, uint32 slot);

extern JSProperty *
js_DefineProperty(JSContext *cx, JSObject *obj, jsval id, jsval value,
		  JSPropertyOp getter, JSPropertyOp setter, uintN flags);

/*
 * Pass &obj for objp if you want to resolve lazily bound properties.  In that
 * case, *objp may differ from obj on return; caveat callers.
 */
extern JS_FRIEND_API(JSBool)
js_LookupProperty(JSContext *cx, JSObject *obj, jsval id, JSObject **objp,
		  JSProperty **propp);

extern JSBool
js_FindProperty(JSContext *cx, jsval id, JSObject **objp, JSProperty **propp);

extern JSBool
js_FindVariable(JSContext *cx, jsval id, JSObject **objp, JSProperty **propp);

extern JSObject *
js_FindVariableScope(JSContext *cx, JSFunction **funp);

extern JSProperty *
js_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSProperty *
js_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
js_DeleteProperty(JSContext *cx, JSObject *obj, jsval id, jsval *rval);

extern JSBool
js_DeleteProperty2(JSContext *cx, JSObject *obj, JSProperty *prop, jsval id,
		   jsval *rval);

extern JSBool
js_GetClassPrototype(JSContext *cx, JSClass *clasp, JSObject **protop);

extern JSBool
js_SetClassPrototype(JSContext *cx, JSFunction *fun, JSObject *obj);

extern JSString *
js_ObjectToString(JSContext *cx, JSObject *obj);

extern JSBool
js_ValueToObject(JSContext *cx, jsval v, JSObject **objp);

extern JSObject *
js_ValueToNonNullObject(JSContext *cx, jsval v);

extern void
js_TryValueOf(JSContext *cx, JSObject *obj, JSType type, jsval *rval);

extern void
js_TryMethod(JSContext *cx, JSObject *obj, JSAtom *atom,
	     uintN argc, jsval *argv, jsval *rval);

PR_END_EXTERN_C

#endif /* jsobj_h___ */
