/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#ifndef jsscope_h___
#define jsscope_h___
/*
 * JS symbol tables.
 */
#include "jstypes.h"
#include "jshash.h" /* Added by JSIFY */
#include "jsobj.h"
#include "jsprvtd.h"
#include "jspubtd.h"

#ifdef JS_THREADSAFE
# include "jslock.h"
#endif

#ifndef JS_DOUBLE_HASHING
struct JSScopeOps {
    JSSymbol *      (*lookup)(JSContext *cx, JSScope *scope, jsid id,
			      JSHashNumber hash);
    JSSymbol *      (*add)(JSContext *cx, JSScope *scope, jsid id,
			   JSScopeProperty *sprop);
    JSBool          (*remove)(JSContext *cx, JSScope *scope, jsid id);
    void            (*clear)(JSContext *cx, JSScope *scope);
};
#endif

struct JSScope {
    JSObjectMap     map;                /* base class state */
    JSObject        *object;            /* object that owns this scope */
    JSScopeProperty *props;             /* property list in definition order */
    JSScopeProperty **proptail;         /* pointer to pointer to last prop */
#ifdef JS_DOUBLE_HASHING
    uint32          tableLength;
    JSScopeProperty *table;
    uint32          gsTableLength;      /* number of entries in gsTable */
    JSPropertyOp    gsTable[1];         /* actually, gsTableLength ops */
#else
    JSScopeOps      *ops;               /* virtual operations */
    void            *data;              /* private data specific to ops */
#endif
#ifdef JS_THREADSAFE
    JSContext       *ownercx;           /* creating context, NULL if shared */
    JSThinLock      lock;               /* binary semaphore protecting scope */
    union {                             /* union lockful and lock-free state: */
        jsrefcount  count;              /* lock entry count for reentrancy */
        JSScope     *link;              /* next link in rt->scopeSharingTodo */
    } u;
#ifdef DEBUG
    const char      *file[4];           /* file where lock was (re-)taken */
    unsigned int    line[4];            /* line where lock was (re-)taken */
#endif
#endif
};

#define OBJ_SCOPE(obj)              ((JSScope *)(obj)->map)
#define SPROP_GETTER(sprop,obj)     SPROP_GETTER_SCOPE(sprop, OBJ_SCOPE(obj))
#define SPROP_SETTER(sprop,obj)     SPROP_SETTER_SCOPE(sprop, OBJ_SCOPE(obj))

#define SPROP_INVALID_SLOT          0xffffffff
#define SPROP_HAS_VALID_SLOT(sprop) ((sprop)->slot != SPROP_INVALID_SLOT)

#ifdef JS_DOUBLE_HASHING

struct JSScopeProperty {
    jsid            id;
    uint32          slot;               /* index in obj->slots vector */
    uint8           attrs;              /* attributes, see jsapi.h JSPROP_ */
    uint8           getterIndex;        /* getter and setter method indexes */
    uint8           setterIndex;        /* in JSScope.gsTable[] */
    uint8           reserved;
    JSScopeProperty *next;              /* singly-linked list linkage */
};

#define SPROP_GETTER_SCOPE(sprop,scope) ((scope)->gsTable[(sprop)->getterIndex])
#define SPROP_SETTER_SCOPE(sprop,scope) ((scope)->gsTable[(sprop)->setterIndex])

#else  /* !JS_DOUBLE_HASHING */

struct JSSymbol {
    JSHashEntry     entry;              /* base class state */
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

#define SPROP_GETTER_SCOPE(sprop,scope) ((sprop)->getter)
#define SPROP_SETTER_SCOPE(sprop,scope) ((sprop)->setter)

#endif /* !JS_DOUBLE_HASHING */

/*
 * These macros are designed to decouple getter and setter from sprop, by
 * passing obj2 (in whose scope sprop lives, and in whose scope getter and
 * setter might be stored apart from sprop -- say in scope->gsTable[i] for
 * a compressed getter or setter index i that is stored in sprop).
 */
#define SPROP_GET(cx,sprop,obj,obj2,vp)                                       \
    (((sprop)->attrs & JSPROP_GETTER)                                         \
     ? js_InternalCall(cx, obj, OBJECT_TO_JSVAL(SPROP_GETTER(sprop,obj2)),    \
                       0, 0, vp)                                              \
     : (SPROP_GETTER(sprop,obj2) != JS_PropertyStub)                          \
     ? SPROP_GETTER(sprop,obj2)(cx, OBJ_THIS_OBJECT(cx,obj), sprop->id, vp)   \
     : JS_TRUE)

#define SPROP_SET(cx,sprop,obj,obj2,vp)                                       \
    (((sprop)->attrs & JSPROP_SETTER)                                         \
     ? js_InternalCall(cx, obj, OBJECT_TO_JSVAL(SPROP_SETTER(sprop,obj2)),    \
                       1, vp, vp)                                             \
     : ((sprop)->attrs & JSPROP_GETTER)                                       \
     ? (JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,                    \
                             JSMSG_GETTER_ONLY, NULL), JS_FALSE)              \
     : (SPROP_SETTER(sprop,obj2) != JS_PropertyStub)                          \
     ? SPROP_SETTER(sprop,obj2)(cx, OBJ_THIS_OBJECT(cx,obj), sprop->id, vp)   \
     : JS_TRUE)

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

extern JSHashNumber
js_HashId(jsid id);

extern jsval
js_IdToValue(jsid id);

extern JSScopeProperty *
js_NewScopeProperty(JSContext *cx, JSScope *scope, jsid id,
		    JSPropertyOp getter, JSPropertyOp setter, uintN attrs);

extern void
js_DestroyScopeProperty(JSContext *cx, JSScope *scope, JSScopeProperty *sprop);

/*
 * For bogus historical reasons, these functions cope with null sprop params.
 * They hide details of reference or "short-term property hold" accounting, so
 * they're worth the space.  But do use HOLD_SCOPE_PROPERTY to keep a non-null
 * sprop in hand for a scope-locking critical section.
 */
extern JSScopeProperty *
js_HoldScopeProperty(JSContext *cx, JSScope *scope, JSScopeProperty *sprop);

extern JSScopeProperty *
js_DropScopeProperty(JSContext *cx, JSScope *scope, JSScopeProperty *sprop);

#define HOLD_SCOPE_PROPERTY(scope,sprop) ((sprop)->nrefs++)

#endif /* jsscope_h___ */
