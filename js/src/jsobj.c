/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*
 * JS object implementation.
 */
#include "jsstddef.h"
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jshash.h" /* Added by JSIFY */
#include "jsdhash.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jsopcode.h"

#include "jsdbgapi.h"   /* whether or not JS_HAS_OBJ_WATCHPOINT */

#ifdef JS_THREADSAFE
#define NATIVE_DROP_PROPERTY js_DropProperty

extern void
js_DropProperty(JSContext *cx, JSObject *obj, JSProperty *prop);
#else
#define NATIVE_DROP_PROPERTY NULL
#endif

#ifdef XP_MAC
#pragma export on
#endif

JS_FRIEND_DATA(JSObjectOps) js_ObjectOps = {
    js_NewObjectMap,        js_DestroyObjectMap,
#if defined JS_THREADSAFE && defined DEBUG
    _js_LookupProperty,     js_DefineProperty,
#else
    js_LookupProperty,      js_DefineProperty,
#endif
    js_GetProperty,         js_SetProperty,
    js_GetAttributes,       js_SetAttributes,
    js_DeleteProperty,      js_DefaultValue,
    js_Enumerate,           js_CheckAccess,
    NULL,                   NATIVE_DROP_PROPERTY,
    js_Call,                js_Construct,
    NULL,                   js_HasInstance,
    js_SetProtoOrParent,    js_SetProtoOrParent,
    js_Mark,                js_Clear,
    js_GetRequiredSlot,     js_SetRequiredSlot
};

#ifdef XP_MAC
#pragma export off
#endif

JSClass js_ObjectClass = {
    js_Object_str,
    0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

#if JS_HAS_OBJ_PROTO_PROP

static JSBool
obj_getSlot(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static JSBool
obj_setSlot(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static JSBool
obj_getCount(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

static JSPropertySpec object_props[] = {
    /* These two must come first; see object_props[slot].name usage below. */
    {js_proto_str, JSSLOT_PROTO, JSPROP_PERMANENT|JSPROP_SHARED,
                                                  obj_getSlot,  obj_setSlot},
    {js_parent_str,JSSLOT_PARENT,JSPROP_READONLY|JSPROP_PERMANENT|JSPROP_SHARED,
                                                  obj_getSlot,  obj_setSlot},
    {js_count_str, 0,            JSPROP_PERMANENT,obj_getCount, obj_getCount},
    {0,0,0,0,0}
};

/* NB: JSSLOT_PROTO and JSSLOT_PARENT are already indexes into object_props. */
#define JSSLOT_COUNT 2

static JSBool
ReportStrictSlot(JSContext *cx, uint32 slot)
{
    if (slot == JSSLOT_PROTO)
        return JS_TRUE;
    return JS_ReportErrorFlagsAndNumber(cx,
                                        JSREPORT_WARNING | JSREPORT_STRICT,
                                        js_GetErrorMessage, NULL,
                                        JSMSG_DEPRECATED_USAGE,
                                        object_props[slot].name);
}

static JSBool
obj_getSlot(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    uint32 slot;
    JSAccessMode mode;
    uintN attrs;

    slot = (uint32) JSVAL_TO_INT(id);
    if (id == INT_TO_JSVAL(JSSLOT_PROTO)) {
        id = (jsid)cx->runtime->atomState.protoAtom;
        mode = JSACC_PROTO;
    } else {
        id = (jsid)cx->runtime->atomState.parentAtom;
        mode = JSACC_PARENT;
    }
    if (!OBJ_CHECK_ACCESS(cx, obj, id, mode, vp, &attrs))
        return JS_FALSE;
    *vp = OBJ_GET_SLOT(cx, obj, slot);
    return JS_TRUE;
}

static JSBool
obj_setSlot(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSObject *pobj;
    uint32 slot;
    uintN attrs;

    if (!JSVAL_IS_OBJECT(*vp))
        return JS_TRUE;
    pobj = JSVAL_TO_OBJECT(*vp);
    slot = (uint32) JSVAL_TO_INT(id);
    if (JS_HAS_STRICT_OPTION(cx) && !ReportStrictSlot(cx, slot))
        return JS_FALSE;

    /* __parent__ is readonly and permanent, only __proto__ may be set. */
    if (!OBJ_CHECK_ACCESS(cx, obj, id, JSACC_PROTO | JSACC_WRITE, vp, &attrs))
        return JS_FALSE;

    return js_SetProtoOrParent(cx, obj, slot, pobj);
}

static JSBool
obj_getCount(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsval iter_state;
    jsid num_properties;
    JSBool ok;

    if (JS_HAS_STRICT_OPTION(cx) && !ReportStrictSlot(cx, JSSLOT_COUNT))
        return JS_FALSE;

    /* Get the number of properties to enumerate. */
    iter_state = JSVAL_NULL;
    ok = OBJ_ENUMERATE(cx, obj, JSENUMERATE_INIT, &iter_state, &num_properties);
    if (!ok)
        goto out;

    if (!JSVAL_IS_INT(num_properties)) {
        JS_ASSERT(0);
        *vp = JSVAL_ZERO;
        goto out;
    }
    *vp = num_properties;

out:
    if (iter_state != JSVAL_NULL)
        ok = OBJ_ENUMERATE(cx, obj, JSENUMERATE_DESTROY, &iter_state, 0);
    return ok;
}

#else  /* !JS_HAS_OBJ_PROTO_PROP */

#define object_props NULL

#endif /* !JS_HAS_OBJ_PROTO_PROP */

JSBool
js_SetProtoOrParent(JSContext *cx, JSObject *obj, uint32 slot, JSObject *pobj)
{
    JSRuntime *rt;
    JSObject *obj2, *oldproto;
    JSScope *scope, *newscope;

    /*
     * Serialize all proto and parent setting in order to detect cycles.
     * We nest locks in this function, and only here, in the following orders:
     *
     * (1)  rt->setSlotLock < pobj's scope lock;
     *      rt->setSlotLock < pobj's proto-or-parent's scope lock;
     *      rt->setSlotLock < pobj's grand-proto-or-parent's scope lock;
     *      etc...
     * (2)  rt->setSlotLock < obj's scope lock < pobj's scope lock.
     *
     * We avoid AB-BA deadlock by restricting obj from being on pobj's parent
     * or proto chain (pobj may already be on obj's parent or proto chain; it
     * could be moving up or down).  We finally order obj with respect to pobj
     * at the bottom of this routine (just before releasing rt->setSlotLock),
     * by making pobj be obj's prototype or parent.
     *
     * After we have set the slot and released rt->setSlotLock, another call
     * to js_SetProtoOrParent could nest locks according to the first order
     * list above, but it cannot deadlock with any other thread.  For there
     * to be a deadlock, other parts of the engine would have to nest scope
     * locks in the opposite order.  XXXbe ensure they don't!
     */
    rt = cx->runtime;
#ifdef JS_THREADSAFE

    JS_ACQUIRE_LOCK(rt->setSlotLock);
    while (rt->setSlotBusy) {
        jsrefcount saveDepth;

        /* Take pains to avoid nesting rt->gcLock inside rt->setSlotLock! */
        JS_RELEASE_LOCK(rt->setSlotLock);
        saveDepth = JS_SuspendRequest(cx);
        JS_ACQUIRE_LOCK(rt->setSlotLock);
        if (rt->setSlotBusy)
            JS_WAIT_CONDVAR(rt->setSlotDone, JS_NO_TIMEOUT);
        JS_RELEASE_LOCK(rt->setSlotLock);
        JS_ResumeRequest(cx, saveDepth);
        JS_ACQUIRE_LOCK(rt->setSlotLock);
    }
    rt->setSlotBusy = JS_TRUE;
    JS_RELEASE_LOCK(rt->setSlotLock);

#define SET_SLOT_DONE(rt)                                                     \
    JS_BEGIN_MACRO                                                            \
        JS_ACQUIRE_LOCK((rt)->setSlotLock);                                   \
        (rt)->setSlotBusy = JS_FALSE;                                         \
        JS_NOTIFY_ALL_CONDVAR((rt)->setSlotDone);                             \
        JS_RELEASE_LOCK((rt)->setSlotLock);                                   \
    JS_END_MACRO

#else

#define SET_SLOT_DONE(rt)       /* nothing */

#endif

    obj2 = pobj;
    while (obj2) {
        if (obj2 == obj) {
            SET_SLOT_DONE(rt);
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_CYCLIC_VALUE,
#if JS_HAS_OBJ_PROTO_PROP
                                 object_props[slot].name
#else
                                 (slot == JSSLOT_PROTO) ? js_proto_str
                                                        : js_parent_str
#endif
                                 );
            return JS_FALSE;
        }
        obj2 = JSVAL_TO_OBJECT(OBJ_GET_SLOT(cx, obj2, slot));
    }

    if (slot == JSSLOT_PROTO && OBJ_IS_NATIVE(obj)) {
        /* Check to see whether obj shares its prototype's scope. */
        JS_LOCK_OBJ(cx, obj);
        scope = OBJ_SCOPE(obj);
        oldproto = JSVAL_TO_OBJECT(LOCKED_OBJ_GET_SLOT(obj, JSSLOT_PROTO));
        if (oldproto && OBJ_SCOPE(oldproto) == scope) {
            /* Either obj needs a new empty scope, or it should share pobj's. */
            if (!pobj ||
                !OBJ_IS_NATIVE(pobj) ||
                OBJ_GET_CLASS(cx, pobj) != LOCKED_OBJ_GET_CLASS(oldproto)) {
                /*
                 * With no proto and no scope of its own, obj is truly empty.
                 *
                 * If pobj is not native, obj needs its own empty scope -- it
                 * should not continue to share oldproto's scope once oldproto
                 * is not on obj's prototype chain.  That would put properties
                 * from oldproto's scope ahead of properties defined by pobj,
                 * in lookup order.
                 *
                 * If pobj's class differs from oldproto's, we may need a new
                 * scope to handle differences in private and reserved slots,
                 * so we suboptimally but safely make one.
                 */
                scope = js_GetMutableScope(cx, obj);
                if (!scope) {
                    JS_UNLOCK_OBJ(cx, obj);
                    SET_SLOT_DONE(rt);
                    return JS_FALSE;
                }
            } else if (OBJ_SCOPE(pobj) != scope) {
#ifdef JS_THREADSAFE
                /*
                 * We are about to nest scope locks.  Help jslock.c:ShareScope
                 * keep scope->u.count balanced for the JS_UNLOCK_SCOPE, while
                 * avoiding deadlock, by recording scope in rt->setSlotScope.
                 */
                if (scope->ownercx) {
                    JS_ASSERT(scope->ownercx == cx);
                    rt->setSlotScope = scope;
                }
#endif

                /* We can't deadlock because we checked for cycles above (2). */
                JS_LOCK_OBJ(cx, pobj);
                newscope = (JSScope *) js_HoldObjectMap(cx, pobj->map);
                obj->map = &newscope->map;
                js_DropObjectMap(cx, &scope->map, obj);
                JS_TRANSFER_SCOPE_LOCK(cx, scope, newscope);
                scope = newscope;
#ifdef JS_THREADSAFE
                rt->setSlotScope = NULL;
#endif
            }
        }
        LOCKED_OBJ_SET_SLOT(obj, JSSLOT_PROTO, OBJECT_TO_JSVAL(pobj));
        JS_UNLOCK_SCOPE(cx, scope);
    } else {
        OBJ_SET_SLOT(cx, obj, slot, OBJECT_TO_JSVAL(pobj));
    }

    SET_SLOT_DONE(rt);
    return JS_TRUE;

#undef SET_SLOT_DONE
}

JS_STATIC_DLL_CALLBACK(JSHashNumber)
js_hash_object(const void *key)
{
    return (JSHashNumber)key >> JSVAL_TAGBITS;
}

static JSHashEntry *
MarkSharpObjects(JSContext *cx, JSObject *obj, JSIdArray **idap)
{
    JSSharpObjectMap *map;
    JSHashTable *table;
    JSHashNumber hash;
    JSHashEntry **hep, *he;
    jsatomid sharpid;
    JSIdArray *ida;
    JSBool ok;
    jsint i, length;
    jsid id;
#if JS_HAS_GETTER_SETTER
    JSObject *obj2;
    JSProperty *prop;
    uintN attrs;
#endif
    jsval val;

    map = &cx->sharpObjectMap;
    table = map->table;
    hash = js_hash_object(obj);
    hep = JS_HashTableRawLookup(table, hash, obj);
    he = *hep;
    if (!he) {
        sharpid = 0;
        he = JS_HashTableRawAdd(table, hep, hash, obj, (void *)sharpid);
        if (!he) {
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
        ida = JS_Enumerate(cx, obj);
        if (!ida)
            return NULL;
        ok = JS_TRUE;
        for (i = 0, length = ida->length; i < length; i++) {
            id = ida->vector[i];
#if JS_HAS_GETTER_SETTER
            ok = OBJ_LOOKUP_PROPERTY(cx, obj, id, &obj2, &prop);
            if (!ok)
                break;
            if (prop) {
                ok = OBJ_GET_ATTRIBUTES(cx, obj2, id, prop, &attrs);
                if (ok) {
                    if (OBJ_IS_NATIVE(obj2) &&
                        (attrs & (JSPROP_GETTER | JSPROP_SETTER))) {
                        val = JSVAL_NULL;
                        if (attrs & JSPROP_GETTER)
                            val = (jsval) ((JSScopeProperty*)prop)->getter;
                        if (attrs & JSPROP_SETTER) {
                            if (val != JSVAL_NULL) {
                                /* Mark the getter, then set val to setter. */
                                ok = (MarkSharpObjects(cx, JSVAL_TO_OBJECT(val),
                                                       NULL)
                                      != NULL);
                            }
                            val = (jsval) ((JSScopeProperty*)prop)->setter;
                        }
                    } else {
                        ok = OBJ_GET_PROPERTY(cx, obj, id, &val);
                    }
                }
                OBJ_DROP_PROPERTY(cx, obj2, prop);
            }
#else
            ok = OBJ_GET_PROPERTY(cx, obj, id, &val);
#endif
            if (!ok)
                break;
            if (!JSVAL_IS_PRIMITIVE(val) &&
                !MarkSharpObjects(cx, JSVAL_TO_OBJECT(val), NULL)) {
                ok = JS_FALSE;
                break;
            }
        }
        if (!ok || !idap)
            JS_DestroyIdArray(cx, ida);
        if (!ok)
            return NULL;
    } else {
        sharpid = (jsatomid) he->value;
        if (sharpid == 0) {
            sharpid = ++map->sharpgen << SHARP_ID_SHIFT;
            he->value = (void *) sharpid;
        }
        ida = NULL;
    }
    if (idap)
        *idap = ida;
    return he;
}

JSHashEntry *
js_EnterSharpObject(JSContext *cx, JSObject *obj, JSIdArray **idap,
                    jschar **sp)
{
    JSSharpObjectMap *map;
    JSHashTable *table;
    JSIdArray *ida;
    JSHashNumber hash;
    JSHashEntry *he, **hep;
    jsatomid sharpid;
    char buf[20];
    size_t len;

    /* Set to null in case we return an early error. */
    *sp = NULL;
    map = &cx->sharpObjectMap;
    table = map->table;
    if (!table) {
        table = JS_NewHashTable(8, js_hash_object, JS_CompareValues,
                                JS_CompareValues, NULL, NULL);
        if (!table) {
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
        map->table = table;
    }

    ida = NULL;
    if (map->depth == 0) {
        he = MarkSharpObjects(cx, obj, &ida);
        if (!he)
            goto bad;
        JS_ASSERT((((jsatomid) he->value) & SHARP_BIT) == 0);
        if (!idap) {
            JS_DestroyIdArray(cx, ida);
            ida = NULL;
        }
    } else {
        hash = js_hash_object(obj);
        hep = JS_HashTableRawLookup(table, hash, obj);
        he = *hep;

        /*
         * It's possible that the value of a property has changed from the
         * first time the object's properties are traversed (when the property
         * ids are entered into the hash table) to the second (when they are
         * converted to strings), i.e., the OBJ_GET_PROPERTY() call is not
         * idempotent.
         */
        if (!he) {
            he = JS_HashTableRawAdd(table, hep, hash, obj, NULL);
            if (!he) {
                JS_ReportOutOfMemory(cx);
                goto bad;
            }
            *sp = NULL;
            sharpid = 0;
            goto out;
        }
    }

    sharpid = (jsatomid) he->value;
    if (sharpid == 0) {
        *sp = NULL;
    } else {
        len = JS_snprintf(buf, sizeof buf, "#%u%c",
                          sharpid >> SHARP_ID_SHIFT,
                          (sharpid & SHARP_BIT) ? '#' : '=');
        *sp = js_InflateString(cx, buf, len);
        if (!*sp) {
            if (ida)
                JS_DestroyIdArray(cx, ida);
            goto bad;
        }
    }

out:
    JS_ASSERT(he);
    if ((sharpid & SHARP_BIT) == 0) {
        if (idap && !ida) {
            ida = JS_Enumerate(cx, obj);
            if (!ida) {
                if (*sp) {
                    JS_free(cx, *sp);
                    *sp = NULL;
                }
                goto bad;
            }
        }
        map->depth++;
    }

    if (idap)
        *idap = ida;
    return he;

bad:
    /* Clean up the sharpObjectMap table on outermost error. */
    if (map->depth == 0) {
        map->sharpgen = 0;
        JS_HashTableDestroy(map->table);
        map->table = NULL;
    }
    return NULL;
}

void
js_LeaveSharpObject(JSContext *cx, JSIdArray **idap)
{
    JSSharpObjectMap *map;
    JSIdArray *ida;

    map = &cx->sharpObjectMap;
    JS_ASSERT(map->depth > 0);
    if (--map->depth == 0) {
        map->sharpgen = 0;
        JS_HashTableDestroy(map->table);
        map->table = NULL;
    }
    if (idap) {
        ida = *idap;
        if (ida) {
            JS_DestroyIdArray(cx, ida);
            *idap = NULL;
        }
    }
}

#define OBJ_TOSTRING_EXTRA      3       /* for 3 local GC roots */

#if JS_HAS_INITIALIZERS || JS_HAS_TOSOURCE
JSBool
js_obj_toSource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
    JSBool ok, outermost;
    JSHashEntry *he;
    JSIdArray *ida;
    jschar *chars, *ochars, *vsharp;
    const jschar *idstrchars, *vchars;
    size_t nchars, idstrlength, gsoplength, vlength, vsharplength;
    char *comma;
    jsint i, j, length, valcnt;
    jsid id;
#if JS_HAS_GETTER_SETTER
    JSObject *obj2;
    JSProperty *prop;
    uintN attrs;
#endif
    jsval val[2];
    JSString *gsop[2];
    JSAtom *atom;
    JSString *idstr, *valstr, *str;
    int stackDummy;

    if (!JS_CHECK_STACK_SIZE(cx, stackDummy)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_OVER_RECURSED);
        return JS_FALSE;
    }

    /*
     * obj_toString for 1.2 calls toSource, and doesn't want the extra parens
     * on the outside.
     */
    outermost = (cx->version != JSVERSION_1_2 && cx->sharpObjectMap.depth == 0);
    he = js_EnterSharpObject(cx, obj, &ida, &chars);
    if (!he)
        return JS_FALSE;
    if (IS_SHARP(he)) {
        /*
         * We didn't enter -- obj is already "sharp", meaning we've visited it
         * already in our depth first search, and therefore chars contains a
         * string of the form "#n#".
         */
        JS_ASSERT(!ida);
#if JS_HAS_SHARP_VARS
        nchars = js_strlen(chars);
#else
        chars[0] = '{';
        chars[1] = '}';
        chars[2] = 0;
        nchars = 2;
#endif
        goto make_string;
    }
    JS_ASSERT(ida);
    ok = JS_TRUE;

    if (!chars) {
        /* If outermost, allocate 4 + 1 for "({})" and the terminator. */
        chars = (jschar *) malloc(((outermost ? 4 : 2) + 1) * sizeof(jschar));
        nchars = 0;
        if (!chars)
            goto error;
        if (outermost)
            chars[nchars++] = '(';
    } else {
        /* js_EnterSharpObject returned a string of the form "#n=" in chars. */
        MAKE_SHARP(he);
        nchars = js_strlen(chars);
        chars = (jschar *)
            realloc((ochars = chars), (nchars + 2 + 1) * sizeof(jschar));
        if (!chars) {
            free(ochars);
            goto error;
        }
        if (outermost) {
            /*
             * No need for parentheses around the whole shebang, because #n=
             * unambiguously begins an object initializer, and never a block
             * statement.
             */
            outermost = JS_FALSE;
        }
    }

#ifdef DUMP_CALL_TABLE
    if (cx->options & JSOPTION_LOGCALL_TOSOURCE) {
        const char *classname = OBJ_GET_CLASS(cx, obj)->name;
        size_t classnchars = strlen(classname);
        static const char classpropid[] = "C";
        const char *cp;
        size_t onchars = nchars;

        /* 2 for ': ', 2 quotes around classname, 2 for ', ' after. */
        classnchars += sizeof classpropid - 1 + 2 + 2;
        if (ida->length)
            classnchars += 2;

        /* 2 for the braces, 1 for the terminator */
        chars = (jschar *)
            realloc((ochars = chars),
                    (nchars + classnchars + 2 + 1) * sizeof(jschar));
        if (!chars) {
            free(ochars);
            goto error;
        }

        chars[nchars++] = '{';          /* 1 from the 2 braces */
        for (cp = classpropid; *cp; cp++)
            chars[nchars++] = (jschar) *cp;
        chars[nchars++] = ':';
        chars[nchars++] = ' ';          /* 2 for ': ' */
        chars[nchars++] = '"';
        for (cp = classname; *cp; cp++)
            chars[nchars++] = (jschar) *cp;
        chars[nchars++] = '"';          /* 2 quotes */
        if (ida->length) {
            chars[nchars++] = ',';
            chars[nchars++] = ' ';      /* 2 for ', ' */
        }

        JS_ASSERT(nchars - onchars == 1 + classnchars);
    } else
#endif
    chars[nchars++] = '{';

    comma = NULL;

    for (i = 0, length = ida->length; i < length; i++) {
        /* Get strings for id and value and GC-root them via argv. */
        id = ida->vector[i];

#if JS_HAS_GETTER_SETTER

        ok = OBJ_LOOKUP_PROPERTY(cx, obj, id, &obj2, &prop);
        if (!ok)
            goto error;
        valcnt = 0;
        if (prop) {
            ok = OBJ_GET_ATTRIBUTES(cx, obj2, id, prop, &attrs);
            if (!ok) {
                OBJ_DROP_PROPERTY(cx, obj2, prop);
                goto error;
            }
            if (OBJ_IS_NATIVE(obj2) &&
                (attrs & (JSPROP_GETTER | JSPROP_SETTER))) {
                if (attrs & JSPROP_GETTER) {
                    val[valcnt] = (jsval) ((JSScopeProperty *)prop)->getter;
#ifdef OLD_GETTER_SETTER
                    gsop[valcnt] =
                        ATOM_TO_STRING(cx->runtime->atomState.getterAtom);
#else
                    gsop[valcnt] =
                        ATOM_TO_STRING(cx->runtime->atomState.getAtom);
#endif
                    valcnt++;
                }
                if (attrs & JSPROP_SETTER) {
                    val[valcnt] = (jsval) ((JSScopeProperty *)prop)->setter;
#ifdef OLD_GETTER_SETTER
                    gsop[valcnt] =
                        ATOM_TO_STRING(cx->runtime->atomState.setterAtom);
#else
                    gsop[valcnt] =
                        ATOM_TO_STRING(cx->runtime->atomState.setAtom);
#endif
                    valcnt++;
                }
            } else {
                valcnt = 1;
                gsop[0] = NULL;
                ok = OBJ_GET_PROPERTY(cx, obj, id, &val[0]);
            }
            OBJ_DROP_PROPERTY(cx, obj2, prop);
        }

#else  /* !JS_HAS_GETTER_SETTER */

        valcnt = 1;
        gsop[0] = NULL;
        ok = OBJ_GET_PROPERTY(cx, obj, id, &val[0]);

#endif /* !JS_HAS_GETTER_SETTER */

        if (!ok)
            goto error;

        /* Convert id to a jsval and then to a string. */
        atom = JSVAL_IS_INT(id) ? NULL : (JSAtom *)id;
        id = ID_TO_VALUE(id);
        idstr = js_ValueToString(cx, id);
        if (!idstr) {
            ok = JS_FALSE;
            goto error;
        }
        argv[0] = STRING_TO_JSVAL(idstr);

        /*
         * If id is a string that's a reserved identifier, or else id is not
         * an identifier at all, then it needs to be quoted.  Also, negative
         * integer ids must be quoted.
         */
        if (atom
            ? (ATOM_KEYWORD(atom) || !js_IsIdentifier(idstr))
            : JSVAL_TO_INT(id) < 0) {
            idstr = js_QuoteString(cx, idstr, (jschar)'\'');
            if (!idstr) {
                ok = JS_FALSE;
                goto error;
            }
            argv[0] = STRING_TO_JSVAL(idstr);
        }
        idstrchars = JSSTRING_CHARS(idstr);
        idstrlength = JSSTRING_LENGTH(idstr);

        for (j = 0; j < valcnt; j++) {
            /* Convert val[j] to its canonical source form. */
            valstr = js_ValueToSource(cx, val[j]);
            if (!valstr) {
                ok = JS_FALSE;
                goto error;
            }
            argv[1+j] = STRING_TO_JSVAL(valstr);
            vchars = JSSTRING_CHARS(valstr);
            vlength = JSSTRING_LENGTH(valstr);

#ifndef OLD_GETTER_SETTER
            /* Remove 'function ' from beginning of valstr. */
            if (gsop[j]) {
                int n = strlen(js_function_str) + 1;
                vchars += n;
                vlength -= n;
            }
#endif

            /* If val[j] is a non-sharp object, consider sharpening it. */
            vsharp = NULL;
            vsharplength = 0;
#if JS_HAS_SHARP_VARS
            if (!JSVAL_IS_PRIMITIVE(val[j]) && vchars[0] != '#') {
                he = js_EnterSharpObject(cx, JSVAL_TO_OBJECT(val[j]), NULL,
                                         &vsharp);
                if (!he) {
                    ok = JS_FALSE;
                    goto error;
                }
                if (IS_SHARP(he)) {
                    vchars = vsharp;
                    vlength = js_strlen(vchars);
                } else {
                    if (vsharp) {
                        vsharplength = js_strlen(vsharp);
                        MAKE_SHARP(he);
                    }
                    js_LeaveSharpObject(cx, NULL);
                }
            }
#endif

            /* Allocate 1 + 1 at end for closing brace and terminating 0. */
            chars = (jschar *)
                realloc((ochars = chars),
                        (nchars + (comma ? 2 : 0) +
                         idstrlength + 1 +
                         (gsop[j] ? 1 + JSSTRING_LENGTH(gsop[j]) : 0) +
                         vsharplength + vlength +
                         (outermost ? 2 : 1) + 1) * sizeof(jschar));
            if (!chars) {
                /* Save code space on error: let JS_free ignore null vsharp. */
                JS_free(cx, vsharp);
                free(ochars);
                goto error;
            }

            if (comma) {
                chars[nchars++] = comma[0];
                chars[nchars++] = comma[1];
            }
            comma = ", ";

#ifdef OLD_GETTER_SETTER
            js_strncpy(&chars[nchars], idstrchars, idstrlength);
            nchars += idstrlength;
            if (gsop[j]) {
                chars[nchars++] = ' ';
                gsoplength = JSSTRING_LENGTH(gsop[j]);
                js_strncpy(&chars[nchars], JSSTRING_CHARS(gsop[j]), gsoplength);
                nchars += gsoplength;
            }
            chars[nchars++] = ':';
#else
            if (gsop[j]) {
                gsoplength = JSSTRING_LENGTH(gsop[j]);
                js_strncpy(&chars[nchars], JSSTRING_CHARS(gsop[j]), gsoplength);
                nchars += gsoplength;
                chars[nchars++] = ' ';
            }
            js_strncpy(&chars[nchars], idstrchars, idstrlength);
            nchars += idstrlength;
            if (!gsop[j])
                chars[nchars++] = ':';
#endif
            if (vsharplength) {
                js_strncpy(&chars[nchars], vsharp, vsharplength);
                nchars += vsharplength;
            }
            js_strncpy(&chars[nchars], vchars, vlength);
            nchars += vlength;

            if (vsharp)
                JS_free(cx, vsharp);
#ifdef DUMP_CALL_TABLE
            if (outermost && nchars >= js_LogCallToSourceLimit)
                break;
#endif
        }
    }

    chars[nchars++] = '}';
    if (outermost)
        chars[nchars++] = ')';
    chars[nchars] = 0;

  error:
    js_LeaveSharpObject(cx, &ida);

    if (!ok) {
        if (chars)
            free(chars);
        return ok;
    }

    if (!chars) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
  make_string:
    str = js_NewString(cx, chars, nchars, 0);
    if (!str) {
        free(chars);
        return JS_FALSE;
    }
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}
#endif /* JS_HAS_INITIALIZERS || JS_HAS_TOSOURCE */

JSBool
js_obj_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
    jschar *chars;
    size_t nchars;
    const char *clazz, *prefix;
    JSString *str;

#if JS_HAS_INITIALIZERS
    if (cx->version == JSVERSION_1_2)
        return js_obj_toSource(cx, obj, argc, argv, rval);
#endif

    clazz = OBJ_GET_CLASS(cx, obj)->name;
    nchars = 9 + strlen(clazz);         /* 9 for "[object ]" */
    chars = (jschar *) JS_malloc(cx, (nchars + 1) * sizeof(jschar));
    if (!chars)
        return JS_FALSE;

    prefix = "[object ";
    nchars = 0;
    while ((chars[nchars] = (jschar)*prefix) != 0)
        nchars++, prefix++;
    while ((chars[nchars] = (jschar)*clazz) != 0)
        nchars++, clazz++;
    chars[nchars++] = ']';
    chars[nchars] = 0;

    str = js_NewString(cx, chars, nchars, 0);
    if (!str) {
        JS_free(cx, chars);
        return JS_FALSE;
    }
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
obj_valueOf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    *rval = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

static JSBool
obj_eval(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSStackFrame *fp, *caller;
    JSBool indirectCall;
    JSObject *scopeobj;
    JSString *str;
    const char *file;
    uintN line;
    JSPrincipals *principals;
    JSScript *script;
    JSBool ok;
#if JS_HAS_EVAL_THIS_SCOPE
    JSObject *callerScopeChain = NULL, *callerVarObj = NULL;
    JSBool setCallerScopeChain = JS_FALSE, setCallerVarObj = JS_FALSE;
#endif

    fp = cx->fp;
    caller = JS_GetScriptedCaller(cx, fp);
    indirectCall = (caller && caller->pc && *caller->pc != JSOP_EVAL);

    if (JSVERSION_IS_ECMA(cx->version) &&
        indirectCall &&
        !JS_ReportErrorFlagsAndNumber(cx,
                                      JSREPORT_WARNING | JSREPORT_STRICT,
                                      js_GetErrorMessage, NULL,
                                      JSMSG_BAD_INDIRECT_CALL,
                                      js_eval_str)) {
        return JS_FALSE;
    }

    if (!JSVAL_IS_STRING(argv[0])) {
        *rval = argv[0];
        return JS_TRUE;
    }

#if JS_HAS_SCRIPT_OBJECT
    /*
     * Script.prototype.compile/exec and Object.prototype.eval all take an
     * optional trailing argument that overrides the scope object.
     */
    scopeobj = NULL;
    if (argc >= 2) {
        if (!js_ValueToObject(cx, argv[1], &scopeobj))
            return JS_FALSE;
        argv[1] = OBJECT_TO_JSVAL(scopeobj);
    }
    if (!scopeobj)
#endif
    {
#if JS_HAS_EVAL_THIS_SCOPE
        /* If obj.eval(str), emulate 'with (obj) eval(str)' in the caller. */
        if (indirectCall) {
            callerScopeChain = caller->scopeChain;
            if (obj != callerScopeChain) {
                scopeobj = js_NewObject(cx, &js_WithClass, obj,
                                        callerScopeChain);
                if (!scopeobj)
                    return JS_FALSE;

                /* Set fp->scopeChain too, for the compiler. */
                caller->scopeChain = fp->scopeChain = scopeobj;
                setCallerScopeChain = JS_TRUE;
            }

            callerVarObj = caller->varobj;
            if (obj != callerVarObj) {
                /* Set fp->varobj too, for the compiler. */
                caller->varobj = fp->varobj = obj;
                setCallerVarObj = JS_TRUE;
            }
        }
        /* From here on, control must exit through label out with ok set. */
#endif

#if JS_BUG_EVAL_THIS_SCOPE
        /* An old version used the object in which eval was found for scope. */
        scopeobj = obj;
#else
        /* Compile using caller's current scope object. */
        if (caller)
            scopeobj = caller->scopeChain;
#endif
    }

    str = JSVAL_TO_STRING(argv[0]);
    if (caller) {
        file = caller->script->filename;
        line = js_PCToLineNumber(cx, caller->script, caller->pc);
        principals = JS_EvalFramePrincipals(cx, fp, caller);
    } else {
        file = NULL;
        line = 0;
        principals = NULL;
    }

    fp->flags |= JSFRAME_EVAL;
    script = JS_CompileUCScriptForPrincipals(cx, scopeobj, principals,
                                             JSSTRING_CHARS(str),
                                             JSSTRING_LENGTH(str),
                                             file, line);
    if (!script) {
        ok = JS_FALSE;
        goto out;
    }

#if !JS_BUG_EVAL_THIS_SCOPE
#if JS_HAS_SCRIPT_OBJECT
    if (argc < 2)
#endif
    {
        /* Execute using caller's new scope object (might be a Call object). */
        if (caller)
            scopeobj = caller->scopeChain;
    }
#endif
    ok = js_Execute(cx, scopeobj, script, caller, JSFRAME_EVAL, rval);
    JS_DestroyScript(cx, script);

out:
#if JS_HAS_EVAL_THIS_SCOPE
    /* Restore OBJ_GET_PARENT(scopeobj) not callerScopeChain in case of Call. */
    if (setCallerScopeChain)
        caller->scopeChain = callerScopeChain;
    if (setCallerVarObj)
        caller->varobj = callerVarObj;
#endif
    return ok;
}

#if JS_HAS_OBJ_WATCHPOINT

static JSBool
obj_watch_handler(JSContext *cx, JSObject *obj, jsval id, jsval old, jsval *nvp,
                  void *closure)
{
    JSResolvingKey key;
    JSResolvingEntry *entry;
    uint32 generation;
    JSObject *funobj;
    jsval argv[3];
    JSBool ok;

    /* Avoid recursion on (obj, id) already being watched on cx. */
    key.obj = obj;
    key.id = id;
    if (!js_StartResolving(cx, &key, JSRESFLAG_WATCH, &entry))
        return JS_FALSE;
    if (!entry)
        return JS_TRUE;
    generation = cx->resolvingTable->generation;

    funobj = (JSObject *) closure;
    argv[0] = id;
    argv[1] = old;
    argv[2] = *nvp;
    ok = js_InternalCall(cx, obj, OBJECT_TO_JSVAL(funobj), 3, argv, nvp);
    js_StopResolving(cx, &key, JSRESFLAG_WATCH, entry, generation);
    return ok;
}

static JSBool
obj_watch(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *funobj;
    JSFunction *fun;
    jsval userid, value;
    jsid propid;
    uintN attrs;

    if (JSVAL_IS_FUNCTION(cx, argv[1])) {
        funobj = JSVAL_TO_OBJECT(argv[1]);
    } else {
        fun = js_ValueToFunction(cx, &argv[1], 0);
        if (!fun)
            return JS_FALSE;
        funobj = fun->object;
    }
    argv[1] = OBJECT_TO_JSVAL(funobj);

    /* Compute the unique int/atom symbol id needed by js_LookupProperty. */
    userid = argv[0];
    if (!JS_ValueToId(cx, userid, &propid))
        return JS_FALSE;

    if (!OBJ_CHECK_ACCESS(cx, obj, propid, JSACC_WATCH, &value, &attrs))
        return JS_FALSE;
    if (attrs & JSPROP_READONLY)
        return JS_TRUE;
    return JS_SetWatchPoint(cx, obj, userid, obj_watch_handler, funobj);
}

static JSBool
obj_unwatch(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    return JS_ClearWatchPoint(cx, obj, argv[0], NULL, NULL);
}

#endif /* JS_HAS_OBJ_WATCHPOINT */

#if JS_HAS_NEW_OBJ_METHODS
/*
 * Prototype and property query methods, to complement the 'in' and
 * 'instanceof' operators.
 */

/* Proposed ECMA 15.2.4.5. */
static JSBool
obj_hasOwnProperty(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                   jsval *rval)
{
    jsid id;
    JSObject *obj2;
    JSProperty *prop;
    JSScopeProperty *sprop;

    if (!JS_ValueToId(cx, argv[0], &id))
        return JS_FALSE;
    if (!OBJ_LOOKUP_PROPERTY(cx, obj, id, &obj2, &prop))
        return JS_FALSE;
    if (!prop) {
        *rval = JSVAL_FALSE;
    } else if (obj2 == obj) {
        *rval = JSVAL_TRUE;
    } else if (OBJ_IS_NATIVE(obj2)) {
        sprop = (JSScopeProperty *)prop;
        *rval = BOOLEAN_TO_JSVAL(SPROP_IS_SHARED_PERMANENT(sprop));
    } else {
        *rval = JSVAL_FALSE;
    }
    if (prop)
        OBJ_DROP_PROPERTY(cx, obj2, prop);
    return JS_TRUE;
}

/* Proposed ECMA 15.2.4.6. */
static JSBool
obj_isPrototypeOf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *rval)
{
    JSBool b;

    if (!js_IsDelegate(cx, obj, *argv, &b))
        return JS_FALSE;
    *rval = BOOLEAN_TO_JSVAL(b);
    return JS_TRUE;
}

/* Proposed ECMA 15.2.4.7. */
static JSBool
obj_propertyIsEnumerable(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                         jsval *rval)
{
    jsid id;
    uintN attrs;
    JSObject *obj2;
    JSProperty *prop;
    JSBool ok;

    if (!JS_ValueToId(cx, argv[0], &id))
        return JS_FALSE;

    if (!OBJ_LOOKUP_PROPERTY(cx, obj, id, &obj2, &prop))
        return JS_FALSE;

    if (!prop) {
        *rval = JSVAL_FALSE;
        return JS_TRUE;
    }

    /*
     * XXX ECMA spec error compatible: return false unless hasOwnProperty.
     * The ECMA spec really should be fixed so propertyIsEnumerable and the
     * for..in loop agree on whether prototype properties are enumerable,
     * obviously by fixing this method (not by breaking the for..in loop!).
     *
     * We check here for shared permanent prototype properties, which should
     * be treated as if they are local to obj.  They are an implementation
     * technique used to satisfy ECMA requirements; users should not be able
     * to distinguish a shared permanent proto-property from a local one.
     */
    if (obj2 != obj &&
        !(OBJ_IS_NATIVE(obj2) &&
          SPROP_IS_SHARED_PERMANENT((JSScopeProperty *)prop))) {
        OBJ_DROP_PROPERTY(cx, obj2, prop);
        *rval = JSVAL_FALSE;
        return JS_TRUE;
    }

    ok = OBJ_GET_ATTRIBUTES(cx, obj2, id, prop, &attrs);
    OBJ_DROP_PROPERTY(cx, obj2, prop);
    if (ok)
        *rval = BOOLEAN_TO_JSVAL((attrs & JSPROP_ENUMERATE) != 0);
    return ok;
}
#endif /* JS_HAS_NEW_OBJ_METHODS */

#if JS_HAS_GETTER_SETTER
static JSBool
obj_defineGetter(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
    jsval fval, junk;
    jsid id;
    uintN attrs;

    fval = argv[1];
    if (JS_TypeOfValue(cx, fval) != JSTYPE_FUNCTION) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_GETTER_OR_SETTER,
                             js_getter_str);
        return JS_FALSE;
    }

    if (!JS_ValueToId(cx, argv[0], &id))
        return JS_FALSE;
    if (!js_CheckRedeclaration(cx, obj, id, JSPROP_GETTER, NULL, NULL))
        return JS_FALSE;
    /*
     * Getters and setters are just like watchpoints from an access
     * control point of view.
     */
    if (!OBJ_CHECK_ACCESS(cx, obj, id, JSACC_WATCH, &junk, &attrs))
        return JS_FALSE;
    return OBJ_DEFINE_PROPERTY(cx, obj, id, JSVAL_VOID,
                               (JSPropertyOp) JSVAL_TO_OBJECT(fval), NULL,
                               JSPROP_ENUMERATE | JSPROP_GETTER | JSPROP_SHARED,
                               NULL);
}

static JSBool
obj_defineSetter(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
    jsval fval, junk;
    jsid id;
    uintN attrs;

    fval = argv[1];
    if (JS_TypeOfValue(cx, fval) != JSTYPE_FUNCTION) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_GETTER_OR_SETTER,
                             js_setter_str);
        return JS_FALSE;
    }

    if (!JS_ValueToId(cx, argv[0], &id))
        return JS_FALSE;
    if (!js_CheckRedeclaration(cx, obj, id, JSPROP_SETTER, NULL, NULL))
        return JS_FALSE;
    /*
     * Getters and setters are just like watchpoints from an access
     * control point of view.
     */
    if (!OBJ_CHECK_ACCESS(cx, obj, id, JSACC_WATCH, &junk, &attrs))
        return JS_FALSE;
    return OBJ_DEFINE_PROPERTY(cx, obj, id, JSVAL_VOID,
                               NULL, (JSPropertyOp) JSVAL_TO_OBJECT(fval),
                               JSPROP_ENUMERATE | JSPROP_SETTER | JSPROP_SHARED,
                               NULL);
}

static JSBool
obj_lookupGetter(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
    jsid id;
    JSObject *pobj;
    JSProperty *prop;
    JSScopeProperty *sprop;

    if (!JS_ValueToId(cx, argv[0], &id))
        return JS_FALSE;
    if (!OBJ_LOOKUP_PROPERTY(cx, obj, id, &pobj, &prop))
        return JS_FALSE;
    if (prop) {
        if (OBJ_IS_NATIVE(pobj)) {
            sprop = (JSScopeProperty *) prop;
            if (sprop->attrs & JSPROP_GETTER)
                *rval = OBJECT_TO_JSVAL(sprop->getter);
        }
        OBJ_DROP_PROPERTY(cx, pobj, prop);
    }
    return JS_TRUE;
}

static JSBool
obj_lookupSetter(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
    jsid id;
    JSObject *pobj;
    JSProperty *prop;
    JSScopeProperty *sprop;

    if (!JS_ValueToId(cx, argv[0], &id))
        return JS_FALSE;
    if (!OBJ_LOOKUP_PROPERTY(cx, obj, id, &pobj, &prop))
        return JS_FALSE;
    if (prop) {
        if (OBJ_IS_NATIVE(pobj)) {
            sprop = (JSScopeProperty *) prop;
            if (sprop->attrs & JSPROP_SETTER)
                *rval = OBJECT_TO_JSVAL(sprop->setter);
        }
        OBJ_DROP_PROPERTY(cx, pobj, prop);
    }
    return JS_TRUE;
}
#endif /* JS_HAS_GETTER_SETTER */

#if JS_HAS_OBJ_WATCHPOINT
const char js_watch_str[] = "watch";
const char js_unwatch_str[] = "unwatch";
#endif
#if JS_HAS_NEW_OBJ_METHODS
const char js_hasOwnProperty_str[] = "hasOwnProperty";
const char js_isPrototypeOf_str[] = "isPrototypeOf";
const char js_propertyIsEnumerable_str[] = "propertyIsEnumerable";
#endif
#if JS_HAS_GETTER_SETTER
const char js_defineGetter_str[] = "__defineGetter__";
const char js_defineSetter_str[] = "__defineSetter__";
const char js_lookupGetter_str[] = "__lookupGetter__";
const char js_lookupSetter_str[] = "__lookupSetter__";
#endif

static JSFunctionSpec object_methods[] = {
#if JS_HAS_TOSOURCE
    {js_toSource_str,             js_obj_toSource,    0, 0, OBJ_TOSTRING_EXTRA},
#endif
    {js_toString_str,             js_obj_toString,    0, 0, OBJ_TOSTRING_EXTRA},
    {js_toLocaleString_str,       js_obj_toString,    0, 0, OBJ_TOSTRING_EXTRA},
    {js_valueOf_str,              obj_valueOf,        0,0,0},
    {js_eval_str,                 obj_eval,           1,0,0},
#if JS_HAS_OBJ_WATCHPOINT
    {js_watch_str,                obj_watch,          2,0,0},
    {js_unwatch_str,              obj_unwatch,        1,0,0},
#endif
#if JS_HAS_NEW_OBJ_METHODS
    {js_hasOwnProperty_str,       obj_hasOwnProperty, 1,0,0},
    {js_isPrototypeOf_str,        obj_isPrototypeOf,  1,0,0},
    {js_propertyIsEnumerable_str, obj_propertyIsEnumerable, 1,0,0},
#endif
#if JS_HAS_GETTER_SETTER
    {js_defineGetter_str,         obj_defineGetter,   2,0,0},
    {js_defineSetter_str,         obj_defineSetter,   2,0,0},
    {js_lookupGetter_str,         obj_lookupGetter,   1,0,0},
    {js_lookupSetter_str,         obj_lookupSetter,   1,0,0},
#endif
    {0,0,0,0,0}
};

static JSBool
Object(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (argc == 0) {
        /* Trigger logic below to construct a blank object. */
        obj = NULL;
    } else {
        /* If argv[0] is null or undefined, obj comes back null. */
        if (!js_ValueToObject(cx, argv[0], &obj))
            return JS_FALSE;
    }
    if (!obj) {
        JS_ASSERT(!argc || JSVAL_IS_NULL(argv[0]) || JSVAL_IS_VOID(argv[0]));
        if (cx->fp->flags & JSFRAME_CONSTRUCTING)
            return JS_TRUE;
        obj = js_NewObject(cx, &js_ObjectClass, NULL, NULL);
        if (!obj)
            return JS_FALSE;
    }
    *rval = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

/*
 * ObjectOps and Class for with-statement stack objects.
 */
static JSBool
with_LookupProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                    JSProperty **propp
#if defined JS_THREADSAFE && defined DEBUG
                    , const char *file, uintN line
#endif
                    )
{
    JSObject *proto;
    JSScopeProperty *sprop;
    JSStackFrame *fp;

    proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
        return js_LookupProperty(cx, obj, id, objp, propp);
    if (!OBJ_LOOKUP_PROPERTY(cx, proto, id, objp, propp))
        return JS_FALSE;

    /*
     * Check whether id names an argument or local variable in an active
     * function.  If so, pretend we didn't find it, so that the real arg or
     * var property can be found in the function's call object, later on in
     * the scope chain.  But skip unshared arg and var properties -- those
     * result when a script explicitly sets a function "static" property of
     * the same name.  See jsinterp.c:SetFunctionSlot.
     *
     * XXX blame pre-ECMA reflection of function args and vars as properties
     */
    if ((sprop = (JSScopeProperty *) *propp) &&
        (proto = *objp, OBJ_IS_NATIVE(proto)) &&
        (sprop->getter == js_GetArgument ||
         sprop->getter == js_GetLocalVariable) &&
        (sprop->attrs & JSPROP_SHARED)) {
        JS_ASSERT(OBJ_GET_CLASS(cx, proto) == &js_FunctionClass);
        for (fp = cx->fp; fp && (!fp->fun || !fp->fun->interpreted);
             fp = fp->down) {
            continue;
        }
        if (fp && fp->fun == (JSFunction *) JS_GetPrivate(cx, proto)) {
            OBJ_DROP_PROPERTY(cx, proto, *propp);
            *objp = NULL;
            *propp = NULL;
        }
    }
    return JS_TRUE;
}

static JSBool
with_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
        return js_GetProperty(cx, obj, id, vp);
    return OBJ_GET_PROPERTY(cx, proto, id, vp);
}

static JSBool
with_SetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
        return js_SetProperty(cx, obj, id, vp);
    return OBJ_SET_PROPERTY(cx, proto, id, vp);
}

static JSBool
with_GetAttributes(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
                   uintN *attrsp)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
        return js_GetAttributes(cx, obj, id, prop, attrsp);
    return OBJ_GET_ATTRIBUTES(cx, proto, id, prop, attrsp);
}

static JSBool
with_SetAttributes(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
                   uintN *attrsp)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
        return js_SetAttributes(cx, obj, id, prop, attrsp);
    return OBJ_SET_ATTRIBUTES(cx, proto, id, prop, attrsp);
}

static JSBool
with_DeleteProperty(JSContext *cx, JSObject *obj, jsid id, jsval *rval)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
        return js_DeleteProperty(cx, obj, id, rval);
    return OBJ_DELETE_PROPERTY(cx, proto, id, rval);
}

static JSBool
with_DefaultValue(JSContext *cx, JSObject *obj, JSType hint, jsval *vp)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
        return js_DefaultValue(cx, obj, hint, vp);
    return OBJ_DEFAULT_VALUE(cx, proto, hint, vp);
}

static JSBool
with_Enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
               jsval *statep, jsid *idp)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
        return js_Enumerate(cx, obj, enum_op, statep, idp);
    return OBJ_ENUMERATE(cx, proto, enum_op, statep, idp);
}

static JSBool
with_CheckAccess(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
                 jsval *vp, uintN *attrsp)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
        return js_CheckAccess(cx, obj, id, mode, vp, attrsp);
    return OBJ_CHECK_ACCESS(cx, proto, id, mode, vp, attrsp);
}

static JSObject *
with_ThisObject(JSContext *cx, JSObject *obj)
{
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
        return obj;
    return OBJ_THIS_OBJECT(cx, proto);
}

JS_FRIEND_DATA(JSObjectOps) js_WithObjectOps = {
    js_NewObjectMap,        js_DestroyObjectMap,
    with_LookupProperty,    js_DefineProperty,
    with_GetProperty,       with_SetProperty,
    with_GetAttributes,     with_SetAttributes,
    with_DeleteProperty,    with_DefaultValue,
    with_Enumerate,         with_CheckAccess,
    with_ThisObject,        NATIVE_DROP_PROPERTY,
    NULL,                   NULL,
    NULL,                   NULL,
    js_SetProtoOrParent,    js_SetProtoOrParent,
    js_Mark,                js_Clear,
    NULL,                   NULL
};

static JSObjectOps *
with_getObjectOps(JSContext *cx, JSClass *clasp)
{
    return &js_WithObjectOps;
}

JSClass js_WithClass = {
    "With",
    0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub,
    with_getObjectOps,
    0,0,0,0,0,0,0
};

#if JS_HAS_OBJ_PROTO_PROP
static JSBool
With(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *parent, *proto;
    jsval v;

    if (!JS_ReportErrorFlagsAndNumber(cx,
                                      JSREPORT_WARNING | JSREPORT_STRICT,
                                      js_GetErrorMessage, NULL,
                                      JSMSG_DEPRECATED_USAGE,
                                      js_WithClass.name)) {
        return JS_FALSE;
    }

    if (!(cx->fp->flags & JSFRAME_CONSTRUCTING)) {
        obj = js_NewObject(cx, &js_WithClass, NULL, NULL);
        if (!obj)
            return JS_FALSE;
        *rval = OBJECT_TO_JSVAL(obj);
    }

    parent = cx->fp->scopeChain;
    if (argc > 0) {
        if (!js_ValueToObject(cx, argv[0], &proto))
            return JS_FALSE;
        v = OBJECT_TO_JSVAL(proto);
        if (!obj_setSlot(cx, obj, INT_TO_JSVAL(JSSLOT_PROTO), &v))
            return JS_FALSE;
        if (argc > 1) {
            if (!js_ValueToObject(cx, argv[1], &parent))
                return JS_FALSE;
        }
    }
    v = OBJECT_TO_JSVAL(parent);
    return obj_setSlot(cx, obj, INT_TO_JSVAL(JSSLOT_PARENT), &v);
}
#endif

JSObject *
js_InitObjectClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto;
    jsval eval;

#if JS_HAS_SHARP_VARS
    JS_ASSERT(sizeof(jsatomid) * JS_BITS_PER_BYTE >= ATOM_INDEX_LIMIT_LOG2 + 1);
#endif

    proto = JS_InitClass(cx, obj, NULL, &js_ObjectClass, Object, 1,
                         object_props, object_methods, NULL, NULL);
    if (!proto)
        return NULL;

#if JS_HAS_OBJ_PROTO_PROP
    if (!JS_InitClass(cx, obj, NULL, &js_WithClass, With, 0,
                      NULL, NULL, NULL, NULL)) {
        return NULL;
    }
#endif

    /* ECMA (15.1.2.1) says 'eval' is also a property of the global object. */
    if (!OBJ_GET_PROPERTY(cx, proto, (jsid)cx->runtime->atomState.evalAtom,
                          &eval)) {
        return NULL;
    }
    if (!OBJ_DEFINE_PROPERTY(cx, obj, (jsid)cx->runtime->atomState.evalAtom,
                             eval, NULL, NULL, 0, NULL)) {
        return NULL;
    }

    return proto;
}

void
js_InitObjectMap(JSObjectMap *map, jsrefcount nrefs, JSObjectOps *ops,
                 JSClass *clasp)
{
    map->nrefs = nrefs;
    map->ops = ops;
    map->nslots = JS_INITIAL_NSLOTS;
    map->freeslot = JSSLOT_FREE(clasp);
}

JSObjectMap *
js_NewObjectMap(JSContext *cx, jsrefcount nrefs, JSObjectOps *ops,
                JSClass *clasp, JSObject *obj)
{
    return (JSObjectMap *) js_NewScope(cx, nrefs, ops, clasp, obj);
}

void
js_DestroyObjectMap(JSContext *cx, JSObjectMap *map)
{
    js_DestroyScope(cx, (JSScope *)map);
}

JSObjectMap *
js_HoldObjectMap(JSContext *cx, JSObjectMap *map)
{
    JS_ASSERT(map->nrefs >= 0);
    JS_ATOMIC_INCREMENT(&map->nrefs);
    return map;
}

JSObjectMap *
js_DropObjectMap(JSContext *cx, JSObjectMap *map, JSObject *obj)
{
    JS_ASSERT(map->nrefs > 0);
    JS_ATOMIC_DECREMENT(&map->nrefs);
    if (map->nrefs == 0) {
        map->ops->destroyObjectMap(cx, map);
        return NULL;
    }
    if (MAP_IS_NATIVE(map) && ((JSScope *)map)->object == obj)
        ((JSScope *)map)->object = NULL;
    return map;
}

static JSBool
GetClassPrototype(JSContext *cx, JSObject *scope, const char *name,
                  JSObject **protop);

JSObject *
js_NewObject(JSContext *cx, JSClass *clasp, JSObject *proto, JSObject *parent)
{
    JSObject *obj, *ctor;
    JSObjectOps *ops;
    JSObjectMap *map;
    JSClass *protoclasp;
    jsval cval;
    uint32 nslots, i;
    jsval *newslots;

    /* Allocate an object from the GC heap and zero it. */
    obj = (JSObject *) js_AllocGCThing(cx, GCX_OBJECT);
    if (!obj)
        return NULL;

    /* Bootstrap the ur-object, and make it the default prototype object. */
    if (!proto) {
        if (!GetClassPrototype(cx, parent, clasp->name, &proto))
            goto bad;
        if (!proto && !GetClassPrototype(cx, parent, js_Object_str, &proto))
            goto bad;
    }

    /* Always call the class's getObjectOps hook if it has one. */
    ops = clasp->getObjectOps
          ? clasp->getObjectOps(cx, clasp)
          : &js_ObjectOps;

    /*
     * Share proto's map only if it has the same JSObjectOps, and only if
     * proto's class has the same private and reserved slots as obj's map
     * and class have.  We assume that if prototype and object are of the
     * same class, they always have the same number of computed reserved
     * slots (returned via clasp->reserveSlots); otherwise, prototype and
     * object classes must have the same (null or not) reserveSlots hook.
     */
    if (proto &&
        (map = proto->map)->ops == ops &&
        ((protoclasp = OBJ_GET_CLASS(cx, proto)) == clasp ||
         (!((protoclasp->flags ^ clasp->flags) &
            (JSCLASS_HAS_PRIVATE |
             (JSCLASS_RESERVED_SLOTS_MASK << JSCLASS_RESERVED_SLOTS_SHIFT))) &&
          protoclasp->reserveSlots == clasp->reserveSlots)))
    {
        /* Default parent to the parent of the prototype's constructor. */
        if (!parent) {
            if (!OBJ_GET_PROPERTY(cx, proto,
                                  (jsid)cx->runtime->atomState.constructorAtom,
                                  &cval)) {
                goto bad;
            }
            if (JSVAL_IS_OBJECT(cval) && (ctor = JSVAL_TO_OBJECT(cval)) != NULL)
                parent = OBJ_GET_PARENT(cx, ctor);
        }

        /* Share the given prototype's map. */
        obj->map = js_HoldObjectMap(cx, map);

        /* Ensure that obj starts with the minimum slots for clasp. */
        nslots = JS_INITIAL_NSLOTS;
    } else {
        /* Leave parent alone.  Allocate a new map for obj. */
        map = ops->newObjectMap(cx, 1, ops, clasp, obj);
        if (!map)
            goto bad;
        obj->map = map;

        /* Let ops->newObjectMap set nslots so as to reserve slots. */
        nslots = map->nslots;
    }

    /* Allocate a slots vector, with a -1'st element telling its length. */
    newslots = (jsval *) JS_malloc(cx, (nslots + 1) * sizeof(jsval));
    if (!newslots) {
        js_DropObjectMap(cx, obj->map, obj);
        obj->map = NULL;
        goto bad;
    }
    newslots[0] = nslots;
    newslots++;

    /* Set the proto, parent, and class properties. */
    newslots[JSSLOT_PROTO] = OBJECT_TO_JSVAL(proto);
    newslots[JSSLOT_PARENT] = OBJECT_TO_JSVAL(parent);
    newslots[JSSLOT_CLASS] = PRIVATE_TO_JSVAL(clasp);

    /* Clear above JSSLOT_CLASS so the GC doesn't load uninitialized memory. */
    for (i = JSSLOT_CLASS + 1; i < nslots; i++)
        newslots[i] = JSVAL_VOID;

    /* Store newslots after initializing all of 'em, just in case. */
    obj->slots = newslots;

    if (cx->runtime->objectHook) {
        JS_KEEP_ATOMS(cx->runtime);
        cx->runtime->objectHook(cx, obj, JS_TRUE, cx->runtime->objectHookData);
        JS_UNKEEP_ATOMS(cx->runtime);
    }

    return obj;

bad:
    cx->newborn[GCX_OBJECT] = NULL;
    return NULL;
}

static JSBool
FindConstructor(JSContext *cx, JSObject *start, const char *name, jsval *vp)
{
    JSAtom *atom;
    JSObject *obj, *pobj;
    JSProperty *prop;
    JSScopeProperty *sprop;

    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom)
        return JS_FALSE;

    if (start || (cx->fp && (start = cx->fp->scopeChain) != NULL)) {
        /* Find the topmost object in the scope chain. */
        do {
            obj = start;
            start = OBJ_GET_PARENT(cx, obj);
        } while (start);
    } else {
        obj = cx->globalObject;
        if (!obj) {
            *vp = JSVAL_VOID;
            return JS_TRUE;
        }
    }

    JS_ASSERT(OBJ_IS_NATIVE(obj));
    if (!js_LookupPropertyWithFlags(cx, obj, (jsid)atom, JSRESOLVE_CLASSNAME,
                                    &pobj, &prop
#if defined JS_THREADSAFE && defined DEBUG
                                    , __FILE__, __LINE__
#endif
                                    )) {
        return JS_FALSE;
    }
    if (!prop)  {
        *vp = JSVAL_VOID;
        return JS_TRUE;
    }

    JS_ASSERT(OBJ_IS_NATIVE(pobj));
    sprop = (JSScopeProperty *) prop;
    JS_ASSERT(SPROP_HAS_VALID_SLOT(sprop, OBJ_SCOPE(pobj)));
    *vp = OBJ_GET_SLOT(cx, pobj, sprop->slot);
    OBJ_DROP_PROPERTY(cx, pobj, prop);
    return JS_TRUE;
}

JSObject *
js_ConstructObject(JSContext *cx, JSClass *clasp, JSObject *proto,
                   JSObject *parent, uintN argc, jsval *argv)
{
    jsval cval, rval;
    JSObject *obj, *ctor;

    if (!FindConstructor(cx, parent, clasp->name, &cval))
        return NULL;
    if (JSVAL_IS_PRIMITIVE(cval)) {
        js_ReportIsNotFunction(cx, &cval, JSV2F_CONSTRUCT | JSV2F_SEARCH_STACK);
        return NULL;
    }

    /*
     * If proto or parent are NULL, set them to Constructor.prototype and/or
     * Constructor.__parent__, just like JSOP_NEW does.
     */
    ctor = JSVAL_TO_OBJECT(cval);
    if (!parent)
        parent = OBJ_GET_PARENT(cx, ctor);
    if (!proto) {
        if (!OBJ_GET_PROPERTY(cx, ctor,
                              (jsid)cx->runtime->atomState.classPrototypeAtom,
                              &rval)) {
            return NULL;
        }
        if (JSVAL_IS_OBJECT(rval))
            proto = JSVAL_TO_OBJECT(rval);
    }

    obj = js_NewObject(cx, clasp, proto, parent);
    if (!obj)
        return NULL;

    if (!js_InternalConstruct(cx, obj, cval, argc, argv, &rval))
        goto bad;
    return JSVAL_IS_OBJECT(rval) ? JSVAL_TO_OBJECT(rval) : obj;
bad:
    cx->newborn[GCX_OBJECT] = NULL;
    return NULL;
}

void
js_FinalizeObject(JSContext *cx, JSObject *obj)
{
    JSObjectMap *map;

    /* Cope with stillborn objects that have no map. */
    map = obj->map;
    if (!map)
        return;
    JS_ASSERT(obj->slots);

    if (cx->runtime->objectHook)
        cx->runtime->objectHook(cx, obj, JS_FALSE, cx->runtime->objectHookData);

    /* Remove all watchpoints with weak links to obj. */
    JS_ClearWatchPointsForObject(cx, obj);

    /*
     * Finalize obj first, in case it needs map and slots.  Optimized to use
     * LOCKED_OBJ_GET_CLASS instead of OBJ_GET_CLASS, so we avoid "promoting"
     * obj's scope from lock-free to lock-full (see jslock.c:ClaimScope) when
     * we're called from the GC.  Only the GC should call js_FinalizeObject,
     * and no other threads run JS (and possibly racing to update obj->slots)
     * while the GC is running.
     */
    LOCKED_OBJ_GET_CLASS(obj)->finalize(cx, obj);

    /* Drop map and free slots. */
    js_DropObjectMap(cx, map, obj);
    obj->map = NULL;
    JS_free(cx, obj->slots - 1);
    obj->slots = NULL;
}

/* XXXbe if one adds props, deletes earlier props, adds more, the last added
         won't recycle the deleted props' slots. */
JSBool
js_AllocSlot(JSContext *cx, JSObject *obj, uint32 *slotp)
{
    JSObjectMap *map;
    JSClass *clasp;
    uint32 nslots, i;
    jsval *newslots;

    map = obj->map;
    JS_ASSERT(!MAP_IS_NATIVE(map) || ((JSScope *)map)->object == obj);
    clasp = LOCKED_OBJ_GET_CLASS(obj);
    if (map->freeslot == JSSLOT_FREE(clasp)) {
        /* Adjust map->freeslot to include computed reserved slots, if any. */
        if (clasp->reserveSlots)
            map->freeslot += clasp->reserveSlots(cx, obj);
    }
    nslots = map->nslots;
    if (map->freeslot >= nslots) {
        nslots = map->freeslot;
        JS_ASSERT(nslots >= JS_INITIAL_NSLOTS);
        nslots += (nslots + 1) / 2;

        newslots = (jsval *)
            JS_realloc(cx, obj->slots - 1, (nslots + 1) * sizeof(jsval));
        if (!newslots)
            return JS_FALSE;
        for (i = 1 + newslots[0]; i <= nslots; i++)
            newslots[i] = JSVAL_VOID;
        newslots[0] = map->nslots = nslots;
        obj->slots = newslots + 1;
    }

#ifdef TOO_MUCH_GC
    obj->slots[map->freeslot] = JSVAL_VOID;
#endif
    *slotp = map->freeslot++;
    return JS_TRUE;
}

void
js_FreeSlot(JSContext *cx, JSObject *obj, uint32 slot)
{
    JSObjectMap *map;
    uint32 nslots;
    jsval *newslots;

    OBJ_CHECK_SLOT(obj, slot);
    obj->slots[slot] = JSVAL_VOID;
    map = obj->map;
    JS_ASSERT(!MAP_IS_NATIVE(map) || ((JSScope *)map)->object == obj);
    if (map->freeslot == slot + 1)
        map->freeslot = slot;
    nslots = map->nslots;
    if (nslots > JS_INITIAL_NSLOTS && map->freeslot < nslots / 2) {
        nslots = map->freeslot;
        nslots += nslots / 2;
        if (nslots < JS_INITIAL_NSLOTS)
            nslots = JS_INITIAL_NSLOTS;

        newslots = (jsval *)
            JS_realloc(cx, obj->slots - 1, (nslots + 1) * sizeof(jsval));
        if (!newslots)
            return;
        newslots[0] = map->nslots = nslots;
        obj->slots = newslots + 1;
    }
}

#if JS_BUG_EMPTY_INDEX_ZERO
#define CHECK_FOR_EMPTY_INDEX(id)                                             \
    JS_BEGIN_MACRO                                                            \
        if (JSSTRING_LENGTH(_str) == 0)                                       \
            id = JSVAL_ZERO;                                                  \
    JS_END_MACRO
#else
#define CHECK_FOR_EMPTY_INDEX(id) /* nothing */
#endif

/* JSVAL_INT_MAX as a string */
#define JSVAL_INT_MAX_STRING "1073741823"

#define CHECK_FOR_FUNNY_INDEX(id)                                             \
    JS_BEGIN_MACRO                                                            \
        if (!JSVAL_IS_INT(id)) {                                              \
            JSAtom *atom_ = (JSAtom *)id;                                     \
            JSString *str_ = ATOM_TO_STRING(atom_);                           \
            const jschar *cp_ = str_->chars;                                  \
            JSBool negative_ = (*cp_ == '-');                                 \
            if (negative_) cp_++;                                             \
            if (JS7_ISDEC(*cp_) &&                                            \
                str_->length - negative_ <= sizeof(JSVAL_INT_MAX_STRING)-1) { \
                id = CheckForFunnyIndex(id, cp_, negative_);                  \
            } else {                                                          \
                CHECK_FOR_EMPTY_INDEX(id);                                    \
            }                                                                 \
        }                                                                     \
    JS_END_MACRO

static jsid
CheckForFunnyIndex(jsid id, const jschar *cp, JSBool negative)
{
    jsuint index = JS7_UNDEC(*cp++);
    jsuint oldIndex = 0;
    jsuint c = 0;

    if (index != 0) {
        while (JS7_ISDEC(*cp)) {
            oldIndex = index;
            c = JS7_UNDEC(*cp);
            index = 10 * index + c;
            cp++;
        }
    }
    if (*cp == 0 &&
        (oldIndex < (JSVAL_INT_MAX / 10) ||
         (oldIndex == (JSVAL_INT_MAX / 10) &&
          c <= (JSVAL_INT_MAX % 10)))) {
        if (negative)
            index = 0 - index;
        id = INT_TO_JSVAL((jsint)index);
    }
    return id;
}

JSScopeProperty *
js_AddNativeProperty(JSContext *cx, JSObject *obj, jsid id,
                     JSPropertyOp getter, JSPropertyOp setter, uint32 slot,
                     uintN attrs, uintN flags, intN shortid)
{
    JSScope *scope;
    JSScopeProperty *sprop;

    JS_LOCK_OBJ(cx, obj);
    scope = js_GetMutableScope(cx, obj);
    if (!scope) {
        sprop = NULL;
    } else {
        /*
         * Handle old bug that took empty string as zero index.  Also convert
         * string indices to integers if appropriate.
         */
        CHECK_FOR_FUNNY_INDEX(id);
        sprop = js_AddScopeProperty(cx, scope, id, getter, setter, slot, attrs,
                                    flags, shortid);
    }
    JS_UNLOCK_OBJ(cx, obj);
    return sprop;
}

JSScopeProperty *
js_ChangeNativePropertyAttrs(JSContext *cx, JSObject *obj,
                             JSScopeProperty *sprop, uintN attrs, uintN mask,
                             JSPropertyOp getter, JSPropertyOp setter)
{
    JSScope *scope;

    JS_LOCK_OBJ(cx, obj);
    scope = js_GetMutableScope(cx, obj);
    if (!scope) {
        sprop = NULL;
    } else {
        sprop = js_ChangeScopePropertyAttrs(cx, scope, sprop, attrs, mask,
                                            getter, setter);
        if (sprop) {
            PROPERTY_CACHE_FILL(&cx->runtime->propertyCache, obj, sprop->id,
                                sprop);
        }
    }
    JS_UNLOCK_OBJ(cx, obj);
    return sprop;
}

JSBool
js_DefineProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                  JSPropertyOp getter, JSPropertyOp setter, uintN attrs,
                  JSProperty **propp)
{
    return js_DefineNativeProperty(cx, obj, id, value, getter, setter, attrs,
                                   0, 0, propp);
}

JSBool
js_DefineNativeProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                        JSPropertyOp getter, JSPropertyOp setter, uintN attrs,
                        uintN flags, intN shortid, JSProperty **propp)
{
    JSClass *clasp;
    JSScope *scope;
    JSProperty *prop;
    JSScopeProperty *sprop;

    /*
     * Handle old bug that took empty string as zero index.  Also convert
     * string indices to integers if appropriate.
     */
    CHECK_FOR_FUNNY_INDEX(id);

#if JS_HAS_GETTER_SETTER
    /*
     * If defining a getter or setter, we must check for its counterpart and
     * update the attributes and property ops.  A getter or setter is really
     * only half of a property.
     */
    if (attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
        JSObject *pobj;

        /*
         * If JS_THREADSAFE and id is found, js_LookupProperty returns with
         * sprop non-null and pobj locked.  If pobj == obj, the property is
         * already in obj and obj has its own (mutable) scope.  So if we are
         * defining a getter whose setter was already defined, or vice versa,
         * finish the job via js_ChangeScopePropertyAttributes, and refresh
         * the property cache line for (obj, id) to map sprop.
         */
        if (!js_LookupProperty(cx, obj, id, &pobj, &prop))
            return JS_FALSE;
        sprop = (JSScopeProperty *) prop;
        if (sprop &&
            pobj == obj &&
            (sprop->attrs & (JSPROP_GETTER | JSPROP_SETTER))) {
            sprop = js_ChangeScopePropertyAttrs(cx, OBJ_SCOPE(obj), sprop,
                                                attrs, sprop->attrs,
                                                (attrs & JSPROP_GETTER)
                                                ? getter
                                                : sprop->getter,
                                                (attrs & JSPROP_SETTER)
                                                ? setter
                                                : sprop->setter);

            /* NB: obj == pobj, so we can share unlock code at the bottom. */
            if (!sprop)
                goto bad;
            goto out;
        }

        if (prop) {
            /* NB: call OBJ_DROP_PROPERTY, as pobj might not be native. */
            OBJ_DROP_PROPERTY(cx, pobj, prop);
            prop = NULL;
        }
    }
#endif /* JS_HAS_GETTER_SETTER */

    /* Lock if object locking is required by this implementation. */
    JS_LOCK_OBJ(cx, obj);

    /* Use the object's class getter and setter by default. */
    clasp = LOCKED_OBJ_GET_CLASS(obj);
    if (!getter)
        getter = clasp->getProperty;
    if (!setter)
        setter = clasp->setProperty;

    /* Get obj's own scope if it has one, or create a new one for obj. */
    scope = js_GetMutableScope(cx, obj);
    if (!scope)
        goto bad;

    /* Add the property to scope, or replace an existing one of the same id. */
    if (clasp->flags & JSCLASS_SHARE_ALL_PROPERTIES)
        attrs |= JSPROP_SHARED;
    sprop = js_AddScopeProperty(cx, scope, id, getter, setter,
                                SPROP_INVALID_SLOT, attrs, flags, shortid);
    if (!sprop)
        goto bad;

    /* XXXbe called with lock held */
    if (!clasp->addProperty(cx, obj, SPROP_USERID(sprop), &value)) {
        (void) js_RemoveScopeProperty(cx, scope, id);
        goto bad;
    }

    if (SPROP_HAS_VALID_SLOT(sprop, scope))
        LOCKED_OBJ_SET_SLOT(obj, sprop->slot, value);

#if JS_HAS_GETTER_SETTER
out:
#endif
    PROPERTY_CACHE_FILL(&cx->runtime->propertyCache, obj, id, sprop);
    if (propp)
        *propp = (JSProperty *) sprop;
    else
        JS_UNLOCK_OBJ(cx, obj);
    return JS_TRUE;

bad:
    JS_UNLOCK_OBJ(cx, obj);
    return JS_FALSE;
}

/*
 * Given pc pointing after a property accessing bytecode, return true if the
 * access is a "object-detecting" in the sense used by web pages, e.g., when
 * checking whether document.all is defined.
 */
static JSBool
Detecting(JSContext *cx, jsbytecode *pc)
{
    JSScript *script;
    jsbytecode *endpc;
    JSOp op;
    JSAtom *atom;

    script = cx->fp->script;
    for (endpc = script->code + script->length; pc < endpc; pc++) {
        /* General case: a branch or equality op follows the access. */
        op = (JSOp) *pc;
        if (js_CodeSpec[op].format & JOF_DETECTING)
            return JS_TRUE;

        /*
         * Special case #1: handle (document.all == null).  Don't sweat about
         * JS1.2's revision of the equality operators here.
         */
        if (op == JSOP_NULL) {
            if (++pc < endpc)
                return *pc == JSOP_EQ || *pc == JSOP_NE;
            break;
        }

        /*
         * Special case #2: handle (document.all == undefined).  Don't worry
         * about someone redefining undefined, which was added by Edition 3,
         * so was read/write for backward compatibility.
         */
        if (op == JSOP_NAME) {
            atom = GET_ATOM(cx, script, pc);
            if (atom == cx->runtime->atomState.typeAtoms[JSTYPE_VOID] &&
                (pc += js_CodeSpec[op].length) < endpc) {
                op = (JSOp) *pc;
                return op == JSOP_EQ || op == JSOP_NE ||
                       op == JSOP_NEW_EQ || op == JSOP_NEW_NE;
            }
            break;
        }

        /* At this point, anything but grouping means we're not detecting. */
        if (op != JSOP_GROUP)
            break;
    }
    return JS_FALSE;
}

JSBool
js_LookupPropertyWithFlags(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                           JSObject **objp, JSProperty **propp
#if defined JS_THREADSAFE && defined DEBUG
                           , const char *file, uintN line
#endif
                           )
{
    JSObject *start, *obj2, *proto;
    JSScope *scope;
    JSScopeProperty *sprop;
    JSClass *clasp;
    JSResolveOp resolve;
    JSResolvingKey key;
    JSResolvingEntry *entry;
    uint32 generation;
    JSNewResolveOp newresolve;
    jsbytecode *pc;
    const JSCodeSpec *cs;
    uint32 format;
    JSBool ok;

    /*
     * Handle old bug that took empty string as zero index.  Also convert
     * string indices to integers if appropriate.
     */
    CHECK_FOR_FUNNY_INDEX(id);

    /* Search scopes starting with obj and following the prototype link. */
    start = obj;
    for (;;) {
        JS_LOCK_OBJ(cx, obj);
        SET_OBJ_INFO(obj, file, line);
        scope = OBJ_SCOPE(obj);
        if (scope->object == obj) {
            sprop = SCOPE_GET_PROPERTY(scope, id);
        } else {
            /* Shared prototype scope: try resolve before lookup. */
            sprop = NULL;
        }

        /* Try obj's class resolve hook if id was not found in obj's scope. */
        if (!sprop) {
            clasp = LOCKED_OBJ_GET_CLASS(obj);
            resolve = clasp->resolve;
            if (resolve != JS_ResolveStub) {
                /* Avoid recursion on (obj, id) already being resolved on cx. */
                key.obj = obj;
                key.id = id;

                /*
                 * Once we have successfully added an entry for (obj, key) to
                 * cx->resolvingTable, control must go through cleanup: before
                 * returning.  But note that JS_DHASH_ADD may find an existing
                 * entry, in which case we bail to suppress runaway recursion.
                 */
                if (!js_StartResolving(cx, &key, JSRESFLAG_LOOKUP, &entry)) {
                    JS_UNLOCK_OBJ(cx, obj);
                    return JS_FALSE;
                }
                if (!entry) {
                    /* Already resolving id in obj -- dampen recursion. */
                    JS_UNLOCK_OBJ(cx, obj);
                    goto out;
                }
                generation = cx->resolvingTable->generation;

                /* Null *propp here so we can test it at cleanup: safely. */
                *propp = NULL;

                if (clasp->flags & JSCLASS_NEW_RESOLVE) {
                    newresolve = (JSNewResolveOp)resolve;
                    if (cx->fp && (pc = cx->fp->pc)) {
                        cs = &js_CodeSpec[*pc];
                        format = cs->format;
                        if ((format & JOF_MODEMASK) != JOF_NAME)
                            flags |= JSRESOLVE_QUALIFIED;
                        if ((format & JOF_ASSIGNING) ||
                            (cx->fp->flags & JSFRAME_ASSIGNING)) {
                            flags |= JSRESOLVE_ASSIGNING;
                        } else {
                            pc += cs->length;
                            if (Detecting(cx, pc))
                                flags |= JSRESOLVE_DETECTING;
                        }
                        if (format & JOF_DECLARING)
                            flags |= JSRESOLVE_DECLARING;
                    }
                    obj2 = (clasp->flags & JSCLASS_NEW_RESOLVE_GETS_START)
                           ? start
                           : NULL;
                    JS_UNLOCK_OBJ(cx, obj);

                    /* Protect id and all atoms from a GC nested in resolve. */
                    JS_KEEP_ATOMS(cx->runtime);
                    ok = newresolve(cx, obj, ID_TO_VALUE(id), flags, &obj2);
                    JS_UNKEEP_ATOMS(cx->runtime);
                    if (!ok)
                        goto cleanup;

                    JS_LOCK_OBJ(cx, obj);
                    SET_OBJ_INFO(obj, file, line);
                    if (obj2) {
                        /* Resolved: juggle locks and lookup id again. */
                        if (obj2 != obj) {
                            JS_UNLOCK_OBJ(cx, obj);
                            JS_LOCK_OBJ(cx, obj2);
                        }
                        scope = OBJ_SCOPE(obj2);
                        if (!MAP_IS_NATIVE(&scope->map)) {
                            /* Whoops, newresolve handed back a foreign obj2. */
                            JS_ASSERT(obj2 != obj);
                            JS_UNLOCK_OBJ(cx, obj2);
                            ok = OBJ_LOOKUP_PROPERTY(cx, obj2, id, objp, propp);
                            if (!ok || *propp)
                                goto cleanup;
                            JS_LOCK_OBJ(cx, obj2);
                        } else {
                            /*
                             * Require that obj2 have its own scope now, as we
                             * do for old-style resolve.  If it doesn't, then
                             * id was not truly resolved, and we'll find it in
                             * the proto chain, or miss it if obj2's proto is
                             * not on obj's proto chain.  That last case is a
                             * "too bad!" case.
                             */
                            if (scope->object == obj2)
                                sprop = SCOPE_GET_PROPERTY(scope, id);
                        }
                        if (obj2 != obj && !sprop) {
                            JS_UNLOCK_OBJ(cx, obj2);
                            JS_LOCK_OBJ(cx, obj);
                        }
                    }
                } else {
                    /*
                     * Old resolve always requires id re-lookup if obj owns
                     * its scope after resolve returns.
                     */
                    JS_UNLOCK_OBJ(cx, obj);
                    ok = resolve(cx, obj, ID_TO_VALUE(id));
                    if (!ok)
                        goto cleanup;
                    JS_LOCK_OBJ(cx, obj);
                    SET_OBJ_INFO(obj, file, line);
                    scope = OBJ_SCOPE(obj);
                    JS_ASSERT(MAP_IS_NATIVE(&scope->map));
                    if (scope->object == obj)
                        sprop = SCOPE_GET_PROPERTY(scope, id);
                }

            cleanup:
                js_StopResolving(cx, &key, JSRESFLAG_LOOKUP, entry, generation);
                if (!ok || *propp)
                    return ok;
            }
        }

        if (sprop) {
            JS_ASSERT(OBJ_SCOPE(obj) == scope);
            *objp = scope->object;      /* XXXbe hide in jsscope.[ch] */

            *propp = (JSProperty *) sprop;
            return JS_TRUE;
        }

        proto = LOCKED_OBJ_GET_PROTO(obj);
        JS_UNLOCK_OBJ(cx, obj);
        if (!proto)
            break;
        if (!OBJ_IS_NATIVE(proto))
            return OBJ_LOOKUP_PROPERTY(cx, proto, id, objp, propp);
        obj = proto;
    }

out:
    *objp = NULL;
    *propp = NULL;
    return JS_TRUE;
}

#if defined JS_THREADSAFE && defined DEBUG
JS_FRIEND_API(JSBool)
_js_LookupProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                   JSProperty **propp, const char *file, uintN line)
{
    return js_LookupPropertyWithFlags(cx, obj, id, 0, objp, propp, file, line);
}
#else
JS_FRIEND_API(JSBool)
js_LookupProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                  JSProperty **propp)
{
    return js_LookupPropertyWithFlags(cx, obj, id, 0, objp, propp);
}
#endif

JS_FRIEND_API(JSBool)
js_FindProperty(JSContext *cx, jsid id, JSObject **objp, JSObject **pobjp,
                JSProperty **propp)
{
    JSRuntime *rt;
    JSObject *obj, *pobj, *lastobj;
    JSScopeProperty *sprop;
    JSProperty *prop;

    rt = cx->runtime;
    obj = cx->fp->scopeChain;
    do {
        /* Try the property cache and return immediately on cache hit. */
        if (OBJ_IS_NATIVE(obj)) {
            JS_LOCK_OBJ(cx, obj);
            PROPERTY_CACHE_TEST(&rt->propertyCache, obj, id, sprop);
            if (sprop) {
                JS_ASSERT(OBJ_IS_NATIVE(obj));
                *objp = obj;
                *pobjp = obj;
                *propp = (JSProperty *) sprop;
                return JS_TRUE;
            }
            JS_UNLOCK_OBJ(cx, obj);
        }

        /* If cache miss, take the slow path. */
        if (!OBJ_LOOKUP_PROPERTY(cx, obj, id, &pobj, &prop))
            return JS_FALSE;
        if (prop) {
            if (OBJ_IS_NATIVE(pobj)) {
                sprop = (JSScopeProperty *) prop;
                PROPERTY_CACHE_FILL(&rt->propertyCache, pobj, id, sprop);
            }
            *objp = obj;
            *pobjp = pobj;
            *propp = prop;
            return JS_TRUE;
        }
        lastobj = obj;
    } while ((obj = OBJ_GET_PARENT(cx, obj)) != NULL);

    *objp = lastobj;
    *pobjp = NULL;
    *propp = NULL;
    return JS_TRUE;
}

JSObject *
js_FindIdentifierBase(JSContext *cx, jsid id)
{
    JSObject *obj, *pobj;
    JSProperty *prop;

    /*
     * Look for id's property along the "with" statement chain and the
     * statically-linked scope chain.
     */
    if (!js_FindProperty(cx, id, &obj, &pobj, &prop))
        return NULL;
    if (prop) {
        OBJ_DROP_PROPERTY(cx, pobj, prop);
        return obj;
    }

    /*
     * Use the top-level scope from the scope chain, which won't end in the
     * same scope as cx->globalObject for cross-context function calls.
     */
    JS_ASSERT(obj);

    /*
     * Property not found.  Give a strict warning if binding an undeclared
     * top-level variable.
     */
    if (JS_HAS_STRICT_OPTION(cx)) {
        JSString *str = JSVAL_TO_STRING(ID_TO_VALUE(id));
        if (!JS_ReportErrorFlagsAndNumber(cx,
                                          JSREPORT_WARNING | JSREPORT_STRICT,
                                          js_GetErrorMessage, NULL,
                                          JSMSG_UNDECLARED_VAR,
                                          JS_GetStringBytes(str))) {
            return NULL;
        }
    }
    return obj;
}

JSBool
js_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSObject *obj2;
    JSProperty *prop;
    JSScope *scope;
    JSScopeProperty *sprop;
    uint32 slot;

    /*
     * Handle old bug that took empty string as zero index.  Also convert
     * string indices to integers if appropriate.
     */
    CHECK_FOR_FUNNY_INDEX(id);

    if (!js_LookupProperty(cx, obj, id, &obj2, &prop))
        return JS_FALSE;
    if (!prop) {
        jsval default_val;

#if JS_BUG_NULL_INDEX_PROPS
        /* Indexed properties defaulted to null in old versions. */
        default_val = (JSVAL_IS_INT(id) && JSVAL_TO_INT(id) >= 0)
                      ? JSVAL_NULL
                      : JSVAL_VOID;
#else
        default_val = JSVAL_VOID;
#endif
        *vp = default_val;

        if (!OBJ_GET_CLASS(cx, obj)->getProperty(cx, obj, ID_TO_VALUE(id), vp))
            return JS_FALSE;

        /*
         * Give a strict warning if foo.bar is evaluated by a script for an
         * object foo with no property named 'bar'.
         */
        if (JS_HAS_STRICT_OPTION(cx) &&
            *vp == default_val &&
            cx->fp && cx->fp->pc &&
            (*cx->fp->pc == JSOP_GETPROP || *cx->fp->pc == JSOP_GETELEM))
        {
            jsbytecode *pc;
            JSString *str;

            /* Kludge to allow (typeof foo == "undefined") tests. */
            JS_ASSERT(cx->fp->script);
            pc = cx->fp->pc;
            pc += js_CodeSpec[*pc].length;
            if (Detecting(cx, pc))
                return JS_TRUE;

            /* Ok, bad undefined property reference: whine about it. */
            str = js_DecompileValueGenerator(cx, JSDVG_IGNORE_STACK,
                                             ID_TO_VALUE(id), NULL);
            if (!str ||
                !JS_ReportErrorFlagsAndNumber(cx,
                                              JSREPORT_WARNING|JSREPORT_STRICT,
                                              js_GetErrorMessage, NULL,
                                              JSMSG_UNDEFINED_PROP,
                                              JS_GetStringBytes(str))) {
                return JS_FALSE;
            }
        }
        return JS_TRUE;
    }

    if (!OBJ_IS_NATIVE(obj2)) {
        OBJ_DROP_PROPERTY(cx, obj2, prop);
        return OBJ_GET_PROPERTY(cx, obj2, id, vp);
    }

    /* Unlock obj2 before calling getter, relock after to avoid deadlock. */
    scope = OBJ_SCOPE(obj2);
    sprop = (JSScopeProperty *) prop;
    slot = sprop->slot;
    if (slot != SPROP_INVALID_SLOT) {
        JS_ASSERT(slot < obj2->map->freeslot);
        *vp = LOCKED_OBJ_GET_SLOT(obj2, slot);

        /* If sprop has a stub getter, we're done. */
        if (!sprop->getter)
            goto out;
    } else {
        *vp = JSVAL_VOID;
    }

    JS_UNLOCK_SCOPE(cx, scope);
    if (!SPROP_GET(cx, sprop, obj, obj2, vp))
        return JS_FALSE;
    JS_LOCK_SCOPE(cx, scope);

    if (SPROP_HAS_VALID_SLOT(sprop, scope)) {
        LOCKED_OBJ_SET_SLOT(obj2, slot, *vp);
        PROPERTY_CACHE_FILL(&cx->runtime->propertyCache, obj2, id, sprop);
    }

out:
    JS_UNLOCK_SCOPE(cx, scope);
    return JS_TRUE;
}

JSBool
js_SetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSObject *pobj;
    JSProperty *prop;
    JSScopeProperty *sprop;
    JSScope *scope;
    uintN attrs, flags;
    intN shortid;
    JSClass *clasp;
    JSPropertyOp getter, setter;
    jsval pval;
    uint32 slot;

    /*
     * Handle old bug that took empty string as zero index.  Also convert
     * string indices to integers if appropriate.
     */
    CHECK_FOR_FUNNY_INDEX(id);

    if (!js_LookupProperty(cx, obj, id, &pobj, &prop))
        return JS_FALSE;

    if (prop && !OBJ_IS_NATIVE(pobj)) {
        OBJ_DROP_PROPERTY(cx, pobj, prop);
        prop = NULL;
    }
    sprop = (JSScopeProperty *) prop;

    /*
     * Now either sprop is null, meaning id was not found in obj or one of its
     * prototypes; or sprop is non-null, meaning id was found in pobj's scope.
     * If JS_THREADSAFE and sprop is non-null, then scope is locked, and sprop
     * is held: we must OBJ_DROP_PROPERTY or JS_UNLOCK_SCOPE before we return
     * (the two are equivalent for native objects, but we use JS_UNLOCK_SCOPE
     * because it is cheaper).
     */
    attrs = JSPROP_ENUMERATE;
    flags = 0;
    shortid = 0;
    clasp = OBJ_GET_CLASS(cx, obj);
    getter = clasp->getProperty;
    setter = clasp->setProperty;

    if (sprop) {
        /*
         * Set scope for use below.  It was locked by js_LookupProperty, and
         * we know pobj owns it (i.e., scope->object == pobj).  Therefore we
         * optimize JS_UNLOCK_OBJ(cx, pobj) into JS_UNLOCK_SCOPE(cx, scope).
         */
        scope = OBJ_SCOPE(pobj);

        attrs = sprop->attrs;
        if ((attrs & JSPROP_READONLY) ||
            (SCOPE_IS_SEALED(scope) && pobj == obj)) {
            JS_UNLOCK_SCOPE(cx, scope);
            if ((attrs & JSPROP_READONLY) && JSVERSION_IS_ECMA(cx->version))
                return JS_TRUE;
            goto read_only_error;
        }

        if (pobj != obj) {
            /*
             * We found id in a prototype object: prepare to share or shadow.
             * NB: Thanks to the immutable, garbage-collected property tree
             * maintained by jsscope.c in cx->runtime, we needn't worry about
             * sprop going away behind our back after we've unlocked scope.
             */
            JS_UNLOCK_SCOPE(cx, scope);

            /* Don't clone a shared prototype property. */
            if (attrs & JSPROP_SHARED)
                return SPROP_SET(cx, sprop, obj, pobj, vp);

            /* Restore attrs to the ECMA default for new properties. */
            attrs = JSPROP_ENUMERATE;

            /*
             * Preserve the shortid, getter, and setter when shadowing any
             * property that has a shortid.  An old API convention requires
             * that the property's getter and setter functions receive the
             * shortid, not id, when they are called on the shadow we are
             * about to create in obj's scope.
             */
            if (sprop->flags & SPROP_HAS_SHORTID) {
                flags = SPROP_HAS_SHORTID;
                shortid = sprop->shortid;
                getter = sprop->getter;
                setter = sprop->setter;
            }

            /*
             * Forget we found the proto-property now that we've copied any
             * needed member values.
             */
            sprop = NULL;
        }
#ifdef __GNUC__ /* suppress bogus gcc warnings */
    } else {
        scope = NULL;
#endif
    }

    if (!sprop) {
        if (SCOPE_IS_SEALED(OBJ_SCOPE(obj)) && OBJ_SCOPE(obj)->object == obj)
            goto read_only_error;

        /* Find or make a property descriptor with the right heritage. */
        JS_LOCK_OBJ(cx, obj);
        scope = js_GetMutableScope(cx, obj);
        if (!scope) {
            JS_UNLOCK_OBJ(cx, obj);
            return JS_FALSE;
        }
        if (clasp->flags & JSCLASS_SHARE_ALL_PROPERTIES)
            attrs |= JSPROP_SHARED;
        sprop = js_AddScopeProperty(cx, scope, id, getter, setter,
                                    SPROP_INVALID_SLOT, attrs, flags, shortid);
        if (!sprop) {
            JS_UNLOCK_SCOPE(cx, scope);
            return JS_FALSE;
        }

        /* XXXbe called with obj locked */
        if (!clasp->addProperty(cx, obj, SPROP_USERID(sprop), vp)) {
            (void) js_RemoveScopeProperty(cx, scope, id);
            JS_UNLOCK_SCOPE(cx, scope);
            return JS_FALSE;
        }

        /* Initialize new property value (passed to setter) to undefined. */
        if (SPROP_HAS_VALID_SLOT(sprop, scope))
            LOCKED_OBJ_SET_SLOT(obj, sprop->slot, JSVAL_VOID);

        PROPERTY_CACHE_FILL(&cx->runtime->propertyCache, obj, id, sprop);
    }

    /* Get the current property value from its slot. */
    slot = sprop->slot;
    if (slot != SPROP_INVALID_SLOT) {
        JS_ASSERT(slot < obj->map->freeslot);
        pval = LOCKED_OBJ_GET_SLOT(obj, slot);

        /* If sprop has a stub setter, keep scope locked and just store *vp. */
        if (!sprop->setter)
            goto set_slot;
    }

    /* Avoid deadlock by unlocking obj's scope while calling sprop's setter. */
    JS_UNLOCK_SCOPE(cx, scope);

    /* Let the setter modify vp before copying from it to obj->slots[slot]. */
    if (!SPROP_SET(cx, sprop, obj, obj, vp))
        return JS_FALSE;

    /* Relock obj's scope until we are done with sprop. */
    JS_LOCK_SCOPE(cx, scope);

    /*
     * Check whether sprop is still around (was not deleted), and whether it
     * has a slot (it may never have had one, or we may have lost a race with
     * someone who cleared scope).
     */
    if (SPROP_HAS_VALID_SLOT(sprop, scope)) {
  set_slot:
        GC_POKE(cx, pval);
        LOCKED_OBJ_SET_SLOT(obj, slot, *vp);
    }
    JS_UNLOCK_SCOPE(cx, scope);
    return JS_TRUE;

  read_only_error: {
    JSString *str = js_DecompileValueGenerator(cx,
                                               JSDVG_IGNORE_STACK,
                                               ID_TO_VALUE(id),
                                               NULL);
    if (str) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_READ_ONLY,
                             JS_GetStringBytes(str));
    }
    return JS_FALSE;
  }
}

JSBool
js_GetAttributes(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
                 uintN *attrsp)
{
    JSBool noprop, ok;
    JSScopeProperty *sprop;

    noprop = !prop;
    if (noprop) {
        if (!js_LookupProperty(cx, obj, id, &obj, &prop))
            return JS_FALSE;
        if (!prop) {
            *attrsp = 0;
            return JS_TRUE;
        }
        if (!OBJ_IS_NATIVE(obj)) {
            ok = OBJ_GET_ATTRIBUTES(cx, obj, id, prop, attrsp);
            OBJ_DROP_PROPERTY(cx, obj, prop);
            return ok;
        }
    }
    sprop = (JSScopeProperty *)prop;
    *attrsp = sprop->attrs;
    if (noprop)
        OBJ_DROP_PROPERTY(cx, obj, prop);
    return JS_TRUE;
}

JSBool
js_SetAttributes(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
                 uintN *attrsp)
{
    JSBool noprop, ok;
    JSScopeProperty *sprop;

    noprop = !prop;
    if (noprop) {
        if (!js_LookupProperty(cx, obj, id, &obj, &prop))
            return JS_FALSE;
        if (!prop)
            return JS_TRUE;
        if (!OBJ_IS_NATIVE(obj)) {
            ok = OBJ_SET_ATTRIBUTES(cx, obj, id, prop, attrsp);
            OBJ_DROP_PROPERTY(cx, obj, prop);
            return ok;
        }
    }
    sprop = (JSScopeProperty *)prop;
    sprop = js_ChangeNativePropertyAttrs(cx, obj, sprop,
                                         *attrsp &
                                         ~(JSPROP_GETTER | JSPROP_SETTER), 0,
                                         sprop->getter, sprop->setter);
    if (noprop)
        OBJ_DROP_PROPERTY(cx, obj, prop);
    return (sprop != NULL);
}

JSBool
js_DeleteProperty(JSContext *cx, JSObject *obj, jsid id, jsval *rval)
{
#if JS_HAS_PROP_DELETE

    JSObject *proto;
    JSProperty *prop;
    JSScopeProperty *sprop;
    JSString *str;
    JSScope *scope;
    JSBool ok;

    *rval = JSVERSION_IS_ECMA(cx->version) ? JSVAL_TRUE : JSVAL_VOID;

    /*
     * Handle old bug that took empty string as zero index.  Also convert
     * string indices to integers if appropriate.
     */
    CHECK_FOR_FUNNY_INDEX(id);

    if (!js_LookupProperty(cx, obj, id, &proto, &prop))
        return JS_FALSE;
    if (!prop || proto != obj) {
        /*
         * If the property was found in a native prototype, check whether it's
         * shared and permanent.  Such a property stands for direct properties
         * in all delegating objects, matching ECMA semantics without bloating
         * each delegating object.
         */
        if (prop) {
            if (OBJ_IS_NATIVE(proto)) {
                sprop = (JSScopeProperty *)prop;
                if (SPROP_IS_SHARED_PERMANENT(sprop))
                    *rval = JSVAL_FALSE;
            }
            OBJ_DROP_PROPERTY(cx, proto, prop);
            if (*rval == JSVAL_FALSE)
                return JS_TRUE;
        }

        /*
         * If no property, or the property comes unshared or impermanent from
         * a prototype, call the class's delProperty hook, passing rval as the
         * result parameter.
         */
        return OBJ_GET_CLASS(cx, obj)->delProperty(cx, obj, ID_TO_VALUE(id),
                                                   rval);
    }

    sprop = (JSScopeProperty *)prop;
    if (sprop->attrs & JSPROP_PERMANENT) {
        OBJ_DROP_PROPERTY(cx, obj, prop);
        if (JSVERSION_IS_ECMA(cx->version)) {
            *rval = JSVAL_FALSE;
            return JS_TRUE;
        }
        str = js_DecompileValueGenerator(cx, JSDVG_IGNORE_STACK,
                                         ID_TO_VALUE(id), NULL);
        if (str) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_PERMANENT, JS_GetStringBytes(str));
        }
        return JS_FALSE;
    }

    /* XXXbe called with obj locked */
    if (!LOCKED_OBJ_GET_CLASS(obj)->delProperty(cx, obj, SPROP_USERID(sprop),
                                                rval)) {
        OBJ_DROP_PROPERTY(cx, obj, prop);
        return JS_FALSE;
    }

    scope = OBJ_SCOPE(obj);
    if (SPROP_HAS_VALID_SLOT(sprop, scope))
        GC_POKE(cx, LOCKED_OBJ_GET_SLOT(obj, sprop->slot));

    PROPERTY_CACHE_FILL(&cx->runtime->propertyCache, obj, id, NULL);
    ok = js_RemoveScopeProperty(cx, scope, id);
    OBJ_DROP_PROPERTY(cx, obj, prop);
    return ok;

#else  /* !JS_HAS_PROP_DELETE */

    jsval null = JSVAL_NULL;

    *rval = JSVAL_VOID;
    return js_SetProperty(cx, obj, id, &null);

#endif /* !JS_HAS_PROP_DELETE */
}

JSBool
js_DefaultValue(JSContext *cx, JSObject *obj, JSType hint, jsval *vp)
{
    jsval v;
    JSString *str;

    v = OBJECT_TO_JSVAL(obj);
    switch (hint) {
      case JSTYPE_STRING:
        /*
         * Propagate the exception if js_TryMethod finds an appropriate
         * method, and calling that method returned failure.
         */
        if (!js_TryMethod(cx, obj, cx->runtime->atomState.toStringAtom, 0, NULL,
                          &v))
            return JS_FALSE;

        if (!JSVAL_IS_PRIMITIVE(v)) {
            if (!OBJ_GET_CLASS(cx, obj)->convert(cx, obj, hint, &v))
                return JS_FALSE;

            /*
             * JS1.2 never failed (except for malloc failure) to convert an
             * object to a string.  ECMA requires an error if both toString
             * and valueOf fail to produce a primitive value.
             */
            if (!JSVAL_IS_PRIMITIVE(v) && cx->version == JSVERSION_1_2) {
                char *bytes = JS_smprintf("[object %s]",
                                          OBJ_GET_CLASS(cx, obj)->name);
                if (!bytes)
                    return JS_FALSE;
                str = JS_NewString(cx, bytes, strlen(bytes));
                if (!str) {
                    free(bytes);
                    return JS_FALSE;
                }
                v = STRING_TO_JSVAL(str);
                goto out;
            }
        }
        break;

      default:
        if (!OBJ_GET_CLASS(cx, obj)->convert(cx, obj, hint, &v))
            return JS_FALSE;
        if (!JSVAL_IS_PRIMITIVE(v)) {
            JSType type = JS_TypeOfValue(cx, v);
            if (type == hint ||
                (type == JSTYPE_FUNCTION && hint == JSTYPE_OBJECT)) {
                goto out;
            }
            /* Don't convert to string (source object literal) for JS1.2. */
            if (cx->version == JSVERSION_1_2 && hint == JSTYPE_BOOLEAN)
                goto out;
            if (!js_TryMethod(cx, obj, cx->runtime->atomState.toStringAtom, 0,
                              NULL, &v))
                return JS_FALSE;
        }
        break;
    }
    if (!JSVAL_IS_PRIMITIVE(v)) {
        /* Avoid recursive death through js_DecompileValueGenerator. */
        if (hint == JSTYPE_STRING) {
            str = JS_InternString(cx, OBJ_GET_CLASS(cx, obj)->name);
            if (!str)
                return JS_FALSE;
        } else {
            str = NULL;
        }
        *vp = OBJECT_TO_JSVAL(obj);
        str = js_DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, v, str);
        if (str) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_CANT_CONVERT_TO,
                                 JS_GetStringBytes(str),
                                 (hint == JSTYPE_VOID)
                                 ? "primitive type"
                                 : js_type_str[hint]);
        }
        return JS_FALSE;
    }
out:
    *vp = v;
    return JS_TRUE;
}

JSIdArray *
js_NewIdArray(JSContext *cx, jsint length)
{
    JSIdArray *ida;

    ida = (JSIdArray *)
        JS_malloc(cx, sizeof(JSIdArray) + (length - 1) * sizeof(jsval));
    if (ida)
        ida->length = length;
    return ida;
}

JSIdArray *
js_GrowIdArray(JSContext *cx, JSIdArray *ida, jsint length)
{
    ida = (JSIdArray *)
        JS_realloc(cx, ida, sizeof(JSIdArray) + (length - 1) * sizeof(jsval));
    if (ida)
        ida->length = length;
    return ida;
}

/* Private type used to iterate over all properties of a native JS object */
typedef struct JSNativeIteratorState {
    jsint next_index;   /* index into jsid array */
    JSIdArray *ida;     /* All property ids in enumeration */
} JSNativeIteratorState;

/*
 * This function is used to enumerate the properties of native JSObjects
 * and those host objects that do not define a JSNewEnumerateOp-style iterator
 * function.
 */
JSBool
js_Enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
             jsval *statep, jsid *idp)
{
    JSObject *proto_obj;
    JSClass *clasp;
    JSEnumerateOp enumerate;
    JSScopeProperty *sprop, *lastProp;
    jsint i, length;
    JSScope *scope;
    JSIdArray *ida;
    JSNativeIteratorState *state;

    clasp = OBJ_GET_CLASS(cx, obj);
    enumerate = clasp->enumerate;
    if (clasp->flags & JSCLASS_NEW_ENUMERATE)
        return ((JSNewEnumerateOp) enumerate)(cx, obj, enum_op, statep, idp);

    switch (enum_op) {

    case JSENUMERATE_INIT:
        if (!enumerate(cx, obj))
            goto init_error;
        length = 0;

        /*
         * The set of all property ids is pre-computed when the iterator
         * is initialized so as to avoid problems with properties being
         * deleted during the iteration.
         */
        JS_LOCK_OBJ(cx, obj);
        scope = OBJ_SCOPE(obj);

        /*
         * If this object shares a scope with its prototype, don't enumerate
         * its properties.  Otherwise they will be enumerated a second time
         * when the prototype object is enumerated.
         */
        proto_obj = OBJ_GET_PROTO(cx, obj);
        if (proto_obj && scope == OBJ_SCOPE(proto_obj)) {
            ida = js_NewIdArray(cx, 0);
            if (!ida) {
                JS_UNLOCK_OBJ(cx, obj);
                goto init_error;
            }
        } else {
            /* Object has a private scope; Enumerate all props in scope. */
            for (sprop = lastProp = SCOPE_LAST_PROP(scope); sprop;
                 sprop = sprop->parent) {
                if ((
#ifdef DUMP_CALL_TABLE
                     (cx->options & JSOPTION_LOGCALL_TOSOURCE) ||
#endif
                     (sprop->attrs & JSPROP_ENUMERATE)) &&
                    !(sprop->flags & SPROP_IS_ALIAS) &&
                    (!SCOPE_HAD_MIDDLE_DELETE(scope) ||
                     SCOPE_HAS_PROPERTY(scope, sprop))) {
                    length++;
                }
            }
            ida = js_NewIdArray(cx, length);
            if (!ida) {
                JS_UNLOCK_OBJ(cx, obj);
                goto init_error;
            }
            i = length;
            for (sprop = lastProp; sprop; sprop = sprop->parent) {
                if ((
#ifdef DUMP_CALL_TABLE
                     (cx->options & JSOPTION_LOGCALL_TOSOURCE) ||
#endif
                     (sprop->attrs & JSPROP_ENUMERATE)) &&
                    !(sprop->flags & SPROP_IS_ALIAS) &&
                    (!SCOPE_HAD_MIDDLE_DELETE(scope) ||
                     SCOPE_HAS_PROPERTY(scope, sprop))) {
                    JS_ASSERT(i > 0);
                    ida->vector[--i] = sprop->id;
                }
            }
        }
        JS_UNLOCK_OBJ(cx, obj);

        state = (JSNativeIteratorState *)
            JS_malloc(cx, sizeof(JSNativeIteratorState));
        if (!state) {
            JS_DestroyIdArray(cx, ida);
            goto init_error;
        }
        state->ida = ida;
        state->next_index = 0;
        *statep = PRIVATE_TO_JSVAL(state);
        if (idp)
            *idp = INT_TO_JSVAL(length);
        return JS_TRUE;

    case JSENUMERATE_NEXT:
        state = (JSNativeIteratorState *) JSVAL_TO_PRIVATE(*statep);
        ida = state->ida;
        length = ida->length;
        if (state->next_index != length) {
            *idp = ida->vector[state->next_index++];
            return JS_TRUE;
        }

        /* Fall through ... */

    case JSENUMERATE_DESTROY:
        state = (JSNativeIteratorState *) JSVAL_TO_PRIVATE(*statep);
        JS_DestroyIdArray(cx, state->ida);
        JS_free(cx, state);
        *statep = JSVAL_NULL;
        return JS_TRUE;

    default:
        JS_ASSERT(0);
        return JS_FALSE;
    }

init_error:
    *statep = JSVAL_NULL;
    return JS_FALSE;
}

JSBool
js_CheckAccess(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
               jsval *vp, uintN *attrsp)
{
    JSObject *pobj;
    JSProperty *prop;
    JSScopeProperty *sprop;
    JSClass *clasp;
    JSBool ok;

    if (!js_LookupProperty(cx, obj, id, &pobj, &prop))
        return JS_FALSE;
    if (!prop) {
        *vp = JSVAL_VOID;
        *attrsp = 0;
        clasp = OBJ_GET_CLASS(cx, obj);
        return !clasp->checkAccess ||
               clasp->checkAccess(cx, obj, ID_TO_VALUE(id), mode, vp);
    }
    if (!OBJ_IS_NATIVE(pobj)) {
        OBJ_DROP_PROPERTY(cx, pobj, prop);
        return OBJ_CHECK_ACCESS(cx, pobj, id, mode, vp, attrsp);
    }
    sprop = (JSScopeProperty *)prop;
    *vp = (SPROP_HAS_VALID_SLOT(sprop, OBJ_SCOPE(pobj)))
          ? LOCKED_OBJ_GET_SLOT(pobj, sprop->slot)
          : JSVAL_VOID;
    *attrsp = sprop->attrs;
    clasp = LOCKED_OBJ_GET_CLASS(obj);
    if (clasp->checkAccess) {
        JS_UNLOCK_OBJ(cx, pobj);
        ok = clasp->checkAccess(cx, obj, ID_TO_VALUE(id), mode, vp);
        JS_LOCK_OBJ(cx, pobj);
    } else {
        ok = JS_TRUE;
    }
    OBJ_DROP_PROPERTY(cx, pobj, prop);
    return ok;
}

#ifdef JS_THREADSAFE
void
js_DropProperty(JSContext *cx, JSObject *obj, JSProperty *prop)
{
    JS_UNLOCK_OBJ(cx, obj);
}
#endif

static void
ReportIsNotFunction(JSContext *cx, jsval *vp, uintN flags)
{
    /*
     * The decompiler may need to access the args of the function in
     * progress rather than the one we had hoped to call.
     * So we switch the cx->fp to the frame below us. We stick the
     * current frame in the dormantFrameChain to protect it from gc.
     */

    JSStackFrame *fp = cx->fp;
    if (fp->down) {
        JS_ASSERT(!fp->dormantNext);
        fp->dormantNext = cx->dormantFrameChain;
        cx->dormantFrameChain = fp;
        cx->fp = fp->down;
    }

    js_ReportIsNotFunction(cx, vp, flags);

    if (fp->down) {
        JS_ASSERT(cx->dormantFrameChain == fp);
        cx->dormantFrameChain = fp->dormantNext;
        fp->dormantNext = NULL;
        cx->fp = fp;
    }
}

#ifdef NARCISSUS
static JSBool
GetCurrentExecutionContext(JSContext *cx, JSObject *obj, jsval *rval)
{
    JSObject *tmp;
    jsval xcval;

    while ((tmp = OBJ_GET_PARENT(cx, obj)) != NULL)
        obj = tmp;
    if (!OBJ_GET_PROPERTY(cx, obj,
                          (jsid)cx->runtime->atomState.ExecutionContextAtom,
                          &xcval)) {
        return JS_FALSE;
    }
    if (JSVAL_IS_PRIMITIVE(xcval)) {
        JS_ReportError(cx, "invalid ExecutionContext in global object");
        return JS_FALSE;
    }
    if (!OBJ_GET_PROPERTY(cx, JSVAL_TO_OBJECT(xcval),
                          (jsid)cx->runtime->atomState.currentAtom,
                          rval)) {
        return JS_FALSE;
    }
    return JS_TRUE;
}
#endif

JSBool
js_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSClass *clasp;

    clasp = OBJ_GET_CLASS(cx, JSVAL_TO_OBJECT(argv[-2]));
    if (!clasp->call) {
#ifdef NARCISSUS
        JSObject *callee, *args;
        jsval fval, nargv[3];
        JSBool ok;

        callee = JSVAL_TO_OBJECT(argv[-2]);
        if (!OBJ_GET_PROPERTY(cx, callee,
                              (jsid)cx->runtime->atomState.callAtom,
                              &fval)) {
            return JS_FALSE;
        }
        if (JSVAL_IS_FUNCTION(cx, fval)) {
            if (!GetCurrentExecutionContext(cx, obj, &nargv[2]))
                return JS_FALSE;
            args = js_GetArgsObject(cx, cx->fp);
            if (!args)
                return JS_FALSE;
            nargv[0] = OBJECT_TO_JSVAL(obj);
            nargv[1] = OBJECT_TO_JSVAL(args);
            return js_InternalCall(cx, callee, fval, 3, nargv, rval);
        }
        if (JSVAL_IS_OBJECT(fval) && JSVAL_TO_OBJECT(fval) != callee) {
            argv[-2] = fval;
            ok = js_Call(cx, obj, argc, argv, rval);
            argv[-2] = OBJECT_TO_JSVAL(callee);
            return ok;
        }
#endif
        ReportIsNotFunction(cx, &argv[-2], 0);
        return JS_FALSE;
    }
    return clasp->call(cx, obj, argc, argv, rval);
}

JSBool
js_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval)
{
    JSClass *clasp;

    clasp = OBJ_GET_CLASS(cx, JSVAL_TO_OBJECT(argv[-2]));
    if (!clasp->construct) {
#ifdef NARCISSUS
        JSObject *callee, *args;
        jsval cval, nargv[2];
        JSBool ok;

        callee = JSVAL_TO_OBJECT(argv[-2]);
        if (!OBJ_GET_PROPERTY(cx, callee,
                              (jsid)cx->runtime->atomState.constructAtom,
                              &cval)) {
            return JS_FALSE;
        }
        if (JSVAL_IS_FUNCTION(cx, cval)) {
            if (!GetCurrentExecutionContext(cx, obj, &nargv[1]))
                return JS_FALSE;
            args = js_GetArgsObject(cx, cx->fp);
            if (!args)
                return JS_FALSE;
            nargv[0] = OBJECT_TO_JSVAL(args);
            return js_InternalCall(cx, callee, cval, 2, nargv, rval);
        }
        if (JSVAL_IS_OBJECT(cval) && JSVAL_TO_OBJECT(cval) != callee) {
            argv[-2] = cval;
            ok = js_Call(cx, obj, argc, argv, rval);
            argv[-2] = OBJECT_TO_JSVAL(callee);
            return ok;
        }
#endif
        ReportIsNotFunction(cx, &argv[-2], JSV2F_CONSTRUCT);
        return JS_FALSE;
    }
    return clasp->construct(cx, obj, argc, argv, rval);
}

JSBool
js_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    JSClass *clasp;

    clasp = OBJ_GET_CLASS(cx, obj);
    if (clasp->hasInstance)
        return clasp->hasInstance(cx, obj, v, bp);
#ifdef NARCISSUS
    {
        jsval fval, rval;

        if (!OBJ_GET_PROPERTY(cx, obj,
                              (jsid)cx->runtime->atomState.hasInstanceAtom,
                              &fval)) {
            return JS_FALSE;
        }
        if (JSVAL_IS_FUNCTION(cx, fval)) {
            return js_InternalCall(cx, obj, fval, 1, &v, &rval) &&
                   js_ValueToBoolean(cx, rval, bp);
        }
    }
#endif
    *bp = JS_FALSE;
    return JS_TRUE;
}

JSBool
js_IsDelegate(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    JSObject *obj2;

    *bp = JS_FALSE;
    if (JSVAL_IS_PRIMITIVE(v))
        return JS_TRUE;
    obj2 = JSVAL_TO_OBJECT(v);
    while ((obj2 = OBJ_GET_PROTO(cx, obj2)) != NULL) {
        if (obj2 == obj) {
            *bp = JS_TRUE;
            break;
        }
    }
    return JS_TRUE;
}

JSBool
js_GetClassPrototype(JSContext *cx, const char *name, JSObject **protop)
{
    return GetClassPrototype(cx, NULL, name, protop);
}

static JSBool
GetClassPrototype(JSContext *cx, JSObject *scope, const char *name,
                  JSObject **protop)
{
    jsval v;
    JSObject *ctor;

    if (!FindConstructor(cx, scope, name, &v))
        return JS_FALSE;
    if (JSVAL_IS_FUNCTION(cx, v)) {
        ctor = JSVAL_TO_OBJECT(v);
        if (!OBJ_GET_PROPERTY(cx, ctor,
                              (jsid)cx->runtime->atomState.classPrototypeAtom,
                              &v)) {
            return JS_FALSE;
        }
    }
    *protop = JSVAL_IS_OBJECT(v) ? JSVAL_TO_OBJECT(v) : NULL;
    return JS_TRUE;
}

JSBool
js_SetClassPrototype(JSContext *cx, JSObject *ctor, JSObject *proto,
                     uintN attrs)
{
    /*
     * Use the given attributes for the prototype property of the constructor,
     * as user-defined constructors have a DontDelete prototype (which may be
     * reset), while native or "system" constructors have DontEnum | ReadOnly |
     * DontDelete.
     */
    if (!OBJ_DEFINE_PROPERTY(cx, ctor,
                             (jsid)cx->runtime->atomState.classPrototypeAtom,
                             OBJECT_TO_JSVAL(proto), NULL, NULL,
                             attrs, NULL)) {
        return JS_FALSE;
    }

    /*
     * ECMA says that Object.prototype.constructor, or f.prototype.constructor
     * for a user-defined function f, is DontEnum.
     */
    return OBJ_DEFINE_PROPERTY(cx, proto,
                               (jsid)cx->runtime->atomState.constructorAtom,
                               OBJECT_TO_JSVAL(ctor), NULL, NULL,
                               0, NULL);
}

JSBool
js_ValueToObject(JSContext *cx, jsval v, JSObject **objp)
{
    JSObject *obj;

    if (JSVAL_IS_NULL(v) || JSVAL_IS_VOID(v)) {
        obj = NULL;
    } else if (JSVAL_IS_OBJECT(v)) {
        obj = JSVAL_TO_OBJECT(v);
        if (!OBJ_DEFAULT_VALUE(cx, obj, JSTYPE_OBJECT, &v))
            return JS_FALSE;
        if (JSVAL_IS_OBJECT(v))
            obj = JSVAL_TO_OBJECT(v);
    } else {
        if (JSVAL_IS_STRING(v)) {
            obj = js_StringToObject(cx, JSVAL_TO_STRING(v));
        } else if (JSVAL_IS_INT(v)) {
            obj = js_NumberToObject(cx, (jsdouble)JSVAL_TO_INT(v));
        } else if (JSVAL_IS_DOUBLE(v)) {
            obj = js_NumberToObject(cx, *JSVAL_TO_DOUBLE(v));
        } else {
            JS_ASSERT(JSVAL_IS_BOOLEAN(v));
            obj = js_BooleanToObject(cx, JSVAL_TO_BOOLEAN(v));
        }
        if (!obj)
            return JS_FALSE;
    }
    *objp = obj;
    return JS_TRUE;
}

JSObject *
js_ValueToNonNullObject(JSContext *cx, jsval v)
{
    JSObject *obj;
    JSString *str;

    if (!js_ValueToObject(cx, v, &obj))
        return NULL;
    if (!obj) {
        str = js_DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, v, NULL);
        if (str) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_NO_PROPERTIES, JS_GetStringBytes(str));
        }
    }
    return obj;
}

JSBool
js_TryValueOf(JSContext *cx, JSObject *obj, JSType type, jsval *rval)
{
#if JS_HAS_VALUEOF_HINT
    jsval argv[1];

    argv[0] = ATOM_KEY(cx->runtime->atomState.typeAtoms[type]);
    return js_TryMethod(cx, obj, cx->runtime->atomState.valueOfAtom, 1, argv,
                        rval);
#else
    return js_TryMethod(cx, obj, cx->runtime->atomState.valueOfAtom, 0, NULL,
                        rval);
#endif
}

JSBool
js_TryMethod(JSContext *cx, JSObject *obj, JSAtom *atom,
             uintN argc, jsval *argv, jsval *rval)
{
    JSErrorReporter older;
    jsval fval;
    JSBool ok;

    /*
     * Report failure only if an appropriate method was found, and calling it
     * returned failure.  We propagate failure in this case to make exceptions
     * behave properly.
     */
    older = JS_SetErrorReporter(cx, NULL);
    if (!OBJ_GET_PROPERTY(cx, obj, (jsid)atom, &fval)) {
        JS_ClearPendingException(cx);
        ok = JS_TRUE;
    } else if (!JSVAL_IS_PRIMITIVE(fval)) {
        ok = js_InternalCall(cx, obj, fval, argc, argv, rval);
        if (!ok)
            JS_ClearPendingException(cx);
    } else {
        ok = JS_TRUE;
    }
    JS_SetErrorReporter(cx, older);
    return ok;
}

#if JS_HAS_XDR

#include "jsxdrapi.h"

JSBool
js_XDRObject(JSXDRState *xdr, JSObject **objp)
{
    JSContext *cx;
    JSClass *clasp;
    const char *className;
    uint32 classId, classDef;
    JSBool ok;
    JSObject *proto;

    cx = xdr->cx;
    if (xdr->mode == JSXDR_ENCODE) {
        clasp = OBJ_GET_CLASS(cx, *objp);
        className = clasp->name;
        classId = JS_XDRFindClassIdByName(xdr, className);
        classDef = !classId;
        if (classDef && !JS_XDRRegisterClass(xdr, clasp, &classId))
            return JS_FALSE;
    } else {
        classDef = 0;
        className = NULL;
        clasp = NULL;           /* quell GCC overwarning */
    }

    /* XDR a flag word followed (if true) by the class name. */
    if (!JS_XDRUint32(xdr, &classDef))
        return JS_FALSE;
    if (classDef && !JS_XDRCString(xdr, (char **) &className))
        return JS_FALSE;

    /* From here on, return through out: to free className if it was set. */
    ok = JS_XDRUint32(xdr, &classId);
    if (!ok)
        goto out;

    if (xdr->mode != JSXDR_ENCODE) {
        if (classDef) {
            ok = js_GetClassPrototype(cx, className, &proto);
            if (!ok)
                goto out;
            clasp = OBJ_GET_CLASS(cx, proto);
            ok = JS_XDRRegisterClass(xdr, clasp, &classId);
            if (!ok)
                goto out;
        } else {
            clasp = JS_XDRFindClassById(xdr, classId);
            if (!clasp) {
                char numBuf[12];
                JS_snprintf(numBuf, sizeof numBuf, "%ld", (long)classId);
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_CANT_FIND_CLASS, numBuf);
                ok = JS_FALSE;
                goto out;
            }
        }
    }

    if (!clasp->xdrObject) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_CANT_XDR_CLASS, clasp->name);
        ok = JS_FALSE;
    } else {
        ok = clasp->xdrObject(xdr, objp);
    }
out:
    if (xdr->mode != JSXDR_ENCODE && className)
        JS_free(cx, (void *)className);
    return ok;
}

#endif /* JS_HAS_XDR */

#ifdef DEBUG_brendan

#include <stdio.h>
#include <math.h>

uint32 js_entry_count_max;
uint32 js_entry_count_sum;
double js_entry_count_sqsum;
uint32 js_entry_count_hist[11];

static void
MeterEntryCount(uintN count)
{
    if (count) {
        js_entry_count_sum += count;
        js_entry_count_sqsum += (double)count * count;
        if (count > js_entry_count_max)
            js_entry_count_max = count;
    }
    js_entry_count_hist[JS_MIN(count, 10)]++;
}

void
js_DumpScopeMeters(JSRuntime *rt)
{
    static FILE *logfp;
    if (!logfp)
        logfp = fopen("/tmp/scope.stats", "a");

    {
        double mean = 0., var = 0., sigma = 0.;
        double nscopes = rt->liveScopes;
        double nentrys = js_entry_count_sum;
        if (nscopes > 0 && nentrys >= 0) {
            mean = nentrys / nscopes;
            var = nscopes * js_entry_count_sqsum - nentrys * nentrys;
            if (var < 0.0 || nscopes <= 1)
                var = 0.0;
            else
                var /= nscopes * (nscopes - 1);

            /* Windows says sqrt(0.0) is "-1.#J" (?!) so we must test. */
            sigma = (var != 0.) ? sqrt(var) : 0.;
        }

        fprintf(logfp,
                "scopes %g entries %g mean %g sigma %g max %u",
                nscopes, nentrys, mean, sigma, js_entry_count_max);
    }

    fprintf(logfp, " histogram %u %u %u %u %u %u %u %u %u %u %u\n",
            js_entry_count_hist[0], js_entry_count_hist[1],
            js_entry_count_hist[2], js_entry_count_hist[3],
            js_entry_count_hist[4], js_entry_count_hist[5],
            js_entry_count_hist[6], js_entry_count_hist[7],
            js_entry_count_hist[8], js_entry_count_hist[9],
            js_entry_count_hist[10]);
    js_entry_count_sum = js_entry_count_max = 0;
    js_entry_count_sqsum = 0;
    memset(js_entry_count_hist, 0, sizeof js_entry_count_hist);
    fflush(logfp);
}

#endif /* DEBUG_brendan */

uint32
js_Mark(JSContext *cx, JSObject *obj, void *arg)
{
    JSScope *scope;
    JSScopeProperty *sprop;
    JSClass *clasp;

    JS_ASSERT(OBJ_IS_NATIVE(obj));
    scope = OBJ_SCOPE(obj);
#ifdef DEBUG_brendan
    if (scope->object == obj)
        MeterEntryCount(scope->entryCount);
#endif

    JS_ASSERT(!SCOPE_LAST_PROP(scope) ||
              SCOPE_HAS_PROPERTY(scope, SCOPE_LAST_PROP(scope)));

    for (sprop = SCOPE_LAST_PROP(scope); sprop; sprop = sprop->parent) {
        if (SCOPE_HAD_MIDDLE_DELETE(scope) && !SCOPE_HAS_PROPERTY(scope, sprop))
            continue;
        MARK_SCOPE_PROPERTY(sprop);
        if (!JSVAL_IS_INT(sprop->id))
            GC_MARK_ATOM(cx, (JSAtom *)sprop->id, arg);

#if JS_HAS_GETTER_SETTER
        if (sprop->attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
#ifdef GC_MARK_DEBUG
            char buf[64];
            JSAtom *atom = (JSAtom *)sprop->id;
            const char *id = (atom && ATOM_IS_STRING(atom))
                             ? JS_GetStringBytes(ATOM_TO_STRING(atom))
                             : "unknown";
#endif

            if (sprop->attrs & JSPROP_GETTER) {
#ifdef GC_MARK_DEBUG
                JS_snprintf(buf, sizeof buf, "%s %s",
                            id, js_getter_str);
#endif
                GC_MARK(cx,
                        JSVAL_TO_GCTHING((jsval) sprop->getter),
                        buf,
                        arg);
            }
            if (sprop->attrs & JSPROP_SETTER) {
#ifdef GC_MARK_DEBUG
                JS_snprintf(buf, sizeof buf, "%s %s",
                            id, js_setter_str);
#endif
                GC_MARK(cx,
                        JSVAL_TO_GCTHING((jsval) sprop->setter),
                        buf,
                        arg);
            }
        }
#endif /* JS_HAS_GETTER_SETTER */
    }

    /* No one runs while the GC is running, so we can use LOCKED_... here. */
    clasp = LOCKED_OBJ_GET_CLASS(obj);
    if (clasp->mark)
        (void) clasp->mark(cx, obj, arg);

    if (scope->object != obj) {
        /*
         * An unmutated object that shares a prototype's scope.  We can't tell
         * how many slots are allocated and in use at obj->slots by looking at
         * scope, so we get obj->slots' length from its -1'st element.
         */
        return (uint32) obj->slots[-1];
    }
    return JS_MIN(scope->map.freeslot, scope->map.nslots);
}

void
js_Clear(JSContext *cx, JSObject *obj)
{
    JSScope *scope;
    JSRuntime *rt;
    JSScopeProperty *sprop;
    uint32 i, n;

    /*
     * Clear our scope and the property cache of all obj's properties only if
     * obj owns the scope (i.e., not if obj is unmutated and therefore sharing
     * its prototype's scope).  NB: we do not clear any reserved slots lying
     * below JSSLOT_FREE(clasp).
     */
    JS_LOCK_OBJ(cx, obj);
    scope = OBJ_SCOPE(obj);
    if (scope->object == obj) {
        /* Clear the property cache before we clear the scope. */
        rt = cx->runtime;
        for (sprop = SCOPE_LAST_PROP(scope); sprop; sprop = sprop->parent) {
            if (!SCOPE_HAD_MIDDLE_DELETE(scope) ||
                SCOPE_HAS_PROPERTY(scope, sprop)) {
                PROPERTY_CACHE_FILL(&rt->propertyCache, obj, sprop->id, NULL);
            }
        }

        /* Now that we're done using scope->lastProp/table, clear scope. */
        js_ClearScope(cx, scope);

        /* Clear slot values and reset freeslot so we're consistent. */
        i = scope->map.nslots;
        n = JSSLOT_FREE(LOCKED_OBJ_GET_CLASS(obj));
        while (--i >= n)
            obj->slots[i] = JSVAL_VOID;
        scope->map.freeslot = n;
    }
    JS_UNLOCK_OBJ(cx, obj);
}

jsval
js_GetRequiredSlot(JSContext *cx, JSObject *obj, uint32 slot)
{
    jsval v;

    JS_LOCK_OBJ(cx, obj);
    v = (slot < (uint32) obj->slots[-1]) ? obj->slots[slot] : JSVAL_VOID;
    JS_UNLOCK_OBJ(cx, obj);
    return v;
}

JSBool
js_SetRequiredSlot(JSContext *cx, JSObject *obj, uint32 slot, jsval v)
{
    JSScope *scope;
    uint32 nslots, i;
    JSClass *clasp;
    jsval *newslots;

    JS_LOCK_OBJ(cx, obj);
    scope = OBJ_SCOPE(obj);
    nslots = (uint32) obj->slots[-1];
    if (slot >= nslots) {
        /*
         * At this point, obj may or may not own scope.  If some path calls
         * js_GetMutableScope but does not add a slot-owning property, then
         * scope->object == obj but nslots will be nominal.  If obj shares a
         * prototype's scope, then we cannot update scope->map here, but we
         * must update obj->slots[-1] when we grow obj->slots.
         *
         * See js_Mark, before the last return, where we make a special case
         * for unmutated (scope->object != obj) objects.
         */
        JS_ASSERT(nslots == JS_INITIAL_NSLOTS);
        clasp = LOCKED_OBJ_GET_CLASS(obj);
        nslots = JSSLOT_FREE(clasp);
        if (clasp->reserveSlots)
            nslots += clasp->reserveSlots(cx, obj);
        JS_ASSERT(slot < nslots);

        newslots = (jsval *)
            JS_realloc(cx, obj->slots - 1, (nslots + 1) * sizeof(jsval));
        if (!newslots) {
            JS_UNLOCK_SCOPE(cx, scope);
            return JS_FALSE;
        }
        for (i = 1 + newslots[0]; i <= nslots; i++)
            newslots[i] = JSVAL_VOID;
        if (scope->object == obj)
            scope->map.nslots = nslots;
        newslots[0] = nslots;
        obj->slots = newslots + 1;
    }

    /* Whether or not we grew nslots, we may need to advance freeslot. */
    if (scope->object == obj && slot >= scope->map.freeslot)
        scope->map.freeslot = slot + 1;

    obj->slots[slot] = v;
    JS_UNLOCK_SCOPE(cx, scope);
    return JS_TRUE;
}

#ifdef DEBUG

/* Routines to print out values during debugging. */

void printChar(jschar *cp) {
    fprintf(stderr, "jschar* (0x%p) \"", (void *)cp);
    while (*cp)
        fputc(*cp++, stderr);
    fputc('"', stderr);
    fputc('\n', stderr);
}

void printString(JSString *str) {
    size_t i, n;
    jschar *s;
    fprintf(stderr, "string (0x%p) \"", (void *)str);
    s = JSSTRING_CHARS(str);
    for (i=0, n=JSSTRING_LENGTH(str); i < n; i++)
        fputc(s[i], stderr);
    fputc('"', stderr);
    fputc('\n', stderr);
}

void printVal(JSContext *cx, jsval val);

void printObj(JSContext *cx, JSObject *jsobj) {
    jsuint i;
    jsval val;
    JSClass *clasp;

    fprintf(stderr, "object 0x%p\n", (void *)jsobj);
    clasp = OBJ_GET_CLASS(cx, jsobj);
    fprintf(stderr, "class 0x%p %s\n", (void *)clasp, clasp->name);
    for (i=0; i < jsobj->map->nslots; i++) {
        fprintf(stderr, "slot %3d ", i);
        val = jsobj->slots[i];
        if (JSVAL_IS_OBJECT(val))
            fprintf(stderr, "object 0x%p\n", (void *)JSVAL_TO_OBJECT(val));
        else
            printVal(cx, val);
    }
}

void printVal(JSContext *cx, jsval val) {
    fprintf(stderr, "val %d (0x%p) = ", (int)val, (void *)val);
    if (JSVAL_IS_NULL(val)) {
        fprintf(stderr, "null\n");
    } else if (JSVAL_IS_VOID(val)) {
        fprintf(stderr, "undefined\n");
    } else if (JSVAL_IS_OBJECT(val)) {
        printObj(cx, JSVAL_TO_OBJECT(val));
    } else if (JSVAL_IS_INT(val)) {
        fprintf(stderr, "(int) %d\n", JSVAL_TO_INT(val));
    } else if (JSVAL_IS_STRING(val)) {
        printString(JSVAL_TO_STRING(val));
    } else if (JSVAL_IS_DOUBLE(val)) {
        fprintf(stderr, "(double) %g\n", *JSVAL_TO_DOUBLE(val));
    } else {
        JS_ASSERT(JSVAL_IS_BOOLEAN(val));
        fprintf(stderr, "(boolean) %s\n",
                JSVAL_TO_BOOLEAN(val) ? "true" : "false");
    }
    fflush(stderr);
}

void printId(JSContext *cx, jsid id) {
    fprintf(stderr, "id %d (0x%p) is ", (int)id, (void *)id);
    printVal(cx, ID_TO_VALUE(id));
}

void printAtom(JSAtom *atom) {
    printString(ATOM_TO_STRING(atom));
}

#endif
