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

#ifndef jsscope_h___
#define jsscope_h___
/*
 * JS symbol tables.
 */
#include "prtypes.h"
#ifndef NSPR20
#include "prhash.h"
#else
#include "plhash.h"
#endif
#include "jsobj.h"
#include "jsprvtd.h"
#include "jspubtd.h"

struct JSScopeOps {
    JSSymbol *      (*lookup)(JSContext *cx, JSScope *scope, jsval id,
			      PRHashNumber hash);
    JSSymbol *      (*add)(JSContext *cx, JSScope *scope, jsval id,
			   JSProperty *prop);
    JSBool          (*remove)(JSContext *cx, JSScope *scope, jsval id);
    void            (*clear)(JSContext *cx, JSScope *scope);
};

struct JSScope {
    JSObjectMap     map;                /* base class state */
    JSObject        *object;            /* object that owns this scope */
    JSProperty      **proptail;         /* pointer to pointer to last prop */
    JSScopeOps      *ops;               /* virtual operations */
    void            *data;              /* private data specific to ops */
#if SCOPE_TABLE
    PRHashEntry     entry;
#endif
};

struct JSSymbol {
    PRHashEntry     entry;              /* base class state */
    JSScope         *scope;             /* pointer to owning scope */
    JSSymbol        *next;              /* next in type-specific list */
};

#define sym_id(sym)             ((jsval)(sym)->entry.key)
#define sym_atom(sym)           ((JSAtom *)(sym)->entry.key)
#define sym_property(sym)       ((JSProperty *)(sym)->entry.value)

struct JSProperty {
    jsrefcount      nrefs;              /* number of referencing symbols */
    jsval           id;                 /* id passed to getter and setter */
    JSPropertyOp    getter;             /* getter and setter methods */
    JSPropertyOp    setter;
    uint32          slot;               /* index in obj->slots vector */
    uint8           flags;              /* flags -- see JSPROP_* in jsapi.h */
    uint8           spare;              /* reserved for future use */
    JSObject        *object;            /* object that owns this property */
    JSSymbol        *symbols;           /* list of aliasing symbols */
    JSProperty      *next;              /* doubly-linked list linkage */
    JSProperty      **prevp;
};

extern JSScope *
js_GetMutableScope(JSContext *cx, JSObject *obj);

extern JSScope *
js_MutateScope(JSContext *cx, JSObject *obj, jsval id,
	       JSPropertyOp getter, JSPropertyOp setter, uintN flags,
	       JSProperty **propp);

extern JSScope *
js_NewScope(JSContext *cx, JSClass *clasp, JSObject *obj);

extern void
js_DestroyScope(JSContext *cx, JSScope *scope);

extern JSScope *
js_HoldScope(JSContext *cx, JSScope *scope);

extern JSScope *
js_DropScope(JSContext *cx, JSScope *scope);

extern PRHashNumber
js_HashValue(jsval v);

extern jsval
js_IdToValue(jsval id);

extern JSProperty *
js_NewProperty(JSContext *cx, JSScope *scope, jsval id,
	       JSPropertyOp getter, JSPropertyOp setter, uintN flags);

extern void
js_DestroyProperty(JSContext *cx, JSProperty *prop);

extern JSProperty *
js_HoldProperty(JSContext *cx, JSProperty *prop);

extern JSProperty *
js_DropProperty(JSContext *cx, JSProperty *prop);

#endif /* jsscope_h___ */
