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
    JSSymbol *      (*lookup)(JSContext *cx, JSScope *scope, jsid id,
			      PRHashNumber hash);
    JSSymbol *      (*add)(JSContext *cx, JSScope *scope, jsid id,
			   JSScopeProperty *sprop);
    JSBool          (*remove)(JSContext *cx, JSScope *scope, jsid id);
    void            (*clear)(JSContext *cx, JSScope *scope);
};

struct JSScope {
    JSObjectMap     map;                /* base class state */
    JSObject        *object;            /* object that owns this scope */
    JSScopeProperty *props;             /* property list in definition order */
    JSScopeProperty **proptail;         /* pointer to pointer to last prop */
    JSScopeOps      *ops;               /* virtual operations */
    void            *data;              /* private data specific to ops */
#ifdef JS_THREADSAFE
    JSThinLock      lock;              /* binary semaphore protecting scope */
    int32           count;              /* entry count for reentrancy */
#ifdef DEBUG
    const char      *file[4];           /* file where lock was (re-)taken */
    unsigned int    line[4];            /* line where lock was (re-)taken */
#endif
#endif
};

struct JSSymbol {
    PRHashEntry     entry;              /* base class state */
    JSScope         *scope;             /* pointer to owning scope */
    JSSymbol        *next;              /* next in type-specific list */
};

#define sym_id(sym)             ((jsid)(sym)->entry.key)
#define sym_atom(sym)           ((JSAtom *)(sym)->entry.key)
#define sym_property(sym)       ((JSScopeProperty *)(sym)->entry.value)

struct JSScopeProperty {
    jsrefcount      nrefs;              /* number of referencing symbols */
    jsval           id;                 /* id passed to getter and setter */
    JSPropertyOp    getter;             /* getter and setter methods */
    JSPropertyOp    setter;
    uint32          slot;               /* index in obj->slots vector */
    uint8           attrs;              /* attributes, see jsapi.h JSPROP_ */
    uint8           spare;              /* reserved for future use */
    JSSymbol        *symbols;           /* list of aliasing symbols */
    JSScopeProperty *next;              /* doubly-linked list linkage */
    JSScopeProperty **prevp;
};

/*
 * These macros are designed to decouple getter and setter from sprop, by
 * passing obj2 (in whose scope sprop lives, and in whose scope getter and
 * setter might be stored apart from sprop -- say in scope->opTable[i] for
 * a compressed getter or setter index i that is stored in sprop).
 */
#define SPROP_GET(cx,sprop,obj,obj2,vp) ((sprop)->getter(cx,obj,sprop->id,vp))
#define SPROP_SET(cx,sprop,obj,obj2,vp) ((sprop)->setter(cx,obj,sprop->id,vp))

extern JSScope *
js_GetMutableScope(JSContext *cx, JSObject *obj);

extern JSScope *
js_MutateScope(JSContext *cx, JSObject *obj, jsid id,
	       JSPropertyOp getter, JSPropertyOp setter, uintN attrs,
	       JSScopeProperty **propp);

extern JSScope *
js_NewScope(JSContext *cx, jsrefcount nrefs, JSObjectOps *ops, JSClass *clasp,
	    JSObject *obj);

extern void
js_DestroyScope(JSContext *cx, JSScope *scope);

extern PRHashNumber
js_HashValue(jsval v);

extern jsval
js_IdToValue(jsid id);

extern JSScopeProperty *
js_NewScopeProperty(JSContext *cx, JSScope *scope, jsid id,
		    JSPropertyOp getter, JSPropertyOp setter, uintN attrs);

extern void
js_DestroyScopeProperty(JSContext *cx, JSScope *scope, JSScopeProperty *sprop);

extern JSScopeProperty *
js_HoldScopeProperty(JSContext *cx, JSScope *scope, JSScopeProperty *sprop);

extern JSScopeProperty *
js_DropScopeProperty(JSContext *cx, JSScope *scope, JSScopeProperty *sprop);

#endif /* jsscope_h___ */
