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

#if JS_HAS_OBJ_WATCHPOINT
#include "jsdbgapi.h"
#endif

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
    if (JS_HAS_STRICT_OPTION(cx) && !ReportStrictSlot(cx, slot))
        return JS_FALSE;
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

    if (!JSVAL_IS_OBJECT(*vp))
        return JS_TRUE;
    pobj = JSVAL_TO_OBJECT(*vp);
    slot = (uint32) JSVAL_TO_INT(id);
    if (JS_HAS_STRICT_OPTION(cx) && !ReportStrictSlot(cx, slot))
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
                                 JSMSG_CYCLIC_VALUE, object_props[slot].name);
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
            if (!pobj) {
                /* With no proto and no scope of its own, obj is truly empty. */
                scope = js_GetMutableScope(cx, obj);
                if (!scope) {
                    JS_UNLOCK_OBJ(cx, obj);
                    SET_SLOT_DONE(rt);
                    return JS_FALSE;
                }
            } else if (OBJ_IS_NATIVE(pobj) && OBJ_SCOPE(pobj) != scope) {
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
                        if (attrs & JSPROP_GETTER) {
                            val = (jsval)
                                SPROP_GETTER((JSScopeProperty*)prop, obj2);
                        }
                        if (attrs & JSPROP_SETTER) {
                            if (val != JSVAL_NULL) {
                                /* Mark the getter, then set val to setter. */
                                ok = (MarkSharpObjects(cx, JSVAL_TO_OBJECT(val),
                                                       NULL)
                                      != NULL);
                            }
                            val = (jsval)
                                SPROP_SETTER((JSScopeProperty*)prop, obj2);
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
            sharpid = ++map->sharpgen << 1;
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

    *sp = NULL; /* Set to null in case we return an early error. */
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
                          sharpid >> 1, (sharpid & SHARP_BIT) ? '#' : '=');
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
    const jschar *vchars;
    size_t nchars, vlength, vsharplength;
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

    chars[nchars++] = '{';

    comma = NULL;

    for (i = 0, length = ida->length; i < length; i++) {
        /* Get strings for id and value and GC-root them via argv. */
        id = ida->vector[i];

#if JS_HAS_GETTER_SETTER

        ok = OBJ_LOOKUP_PROPERTY(cx, obj, id, &obj2, &prop);
        if (!ok)
            goto error;
        if (prop) {
            ok = OBJ_GET_ATTRIBUTES(cx, obj2, id, prop, &attrs);
            if (!ok)
                goto error;
            if (OBJ_IS_NATIVE(obj2) &&
                (attrs & (JSPROP_GETTER | JSPROP_SETTER))) {
                valcnt = 0;
                if (attrs & JSPROP_GETTER) {
                    val[valcnt] = (jsval)
                        SPROP_GETTER((JSScopeProperty *)prop, obj2);
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
                    val[valcnt] = (jsval)
                        SPROP_SETTER((JSScopeProperty *)prop, obj2);
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
        id = js_IdToValue(id);
        idstr = js_ValueToString(cx, id);
        if (!idstr) {
            ok = JS_FALSE;
            goto error;
        }
        argv[0] = STRING_TO_JSVAL(idstr);

        /*
         * If id is a string that's a reserved identifier, or else id is not
         * an identifier at all, then it needs to be quoted.
         */
        if (atom && (ATOM_KEYWORD(atom) || !js_IsIdentifier(idstr))) {
            idstr = js_QuoteString(cx, idstr, (jschar)'\'');
            if (!idstr) {
                ok = JS_FALSE;
                goto error;
            }
            argv[0] = STRING_TO_JSVAL(idstr);
        }

        for (j = 0; j < valcnt; j++) {
            /* Convert val[j] to its canonical source form. */
            valstr = js_ValueToSource(cx, val[j]);
            if (!valstr) {
                ok = JS_FALSE;
                goto error;
            }
            argv[1+j] = STRING_TO_JSVAL(valstr);
            vchars = valstr->chars;
            vlength = valstr->length;

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
                         idstr->length + 1 +
                         (gsop[j] ? 1 + gsop[j]->length : 0) +
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
            js_strncpy(&chars[nchars], idstr->chars, idstr->length);
            nchars += idstr->length;
            if (gsop[j]) {
                chars[nchars++] = ' ';
                js_strncpy(&chars[nchars], gsop[j]->chars, gsop[j]->length);
                nchars += gsop[j]->length;
            }
            chars[nchars++] = ':';
#else
            if (gsop[j]) {
                js_strncpy(&chars[nchars], gsop[j]->chars, gsop[j]->length);
                nchars += gsop[j]->length;
                chars[nchars++] = ' ';
            }
            js_strncpy(&chars[nchars], idstr->chars, idstr->length);
            nchars += idstr->length;
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
    caller = fp->down;
    indirectCall = (!caller->pc || *caller->pc != JSOP_EVAL);

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
        scopeobj = caller->scopeChain;
#endif
    }

    str = JSVAL_TO_STRING(argv[0]);
    if (caller->script) {
        file = caller->script->filename;
        line = js_PCToLineNumber(caller->script, caller->pc);
        principals = caller->script->principals;
    } else {
        file = NULL;
        line = 0;
        principals = NULL;
    }

    fp->flags |= JSFRAME_EVAL;
    script = JS_CompileUCScriptForPrincipals(cx, scopeobj, principals,
                                             str->chars, str->length,
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
    JSObject *funobj;
    jsval argv[3];

    funobj = (JSObject *) closure;
    argv[0] = id;
    argv[1] = old;
    argv[2] = *nvp;
    return js_InternalCall(cx, obj, OBJECT_TO_JSVAL(funobj), 3, argv, nvp);
}

static JSBool
obj_watch(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFunction *fun;
    jsval userid, value;
    jsid symid;
    uintN attrs;

    fun = js_ValueToFunction(cx, &argv[1], JS_FALSE);
    if (!fun)
        return JS_FALSE;
    argv[1] = OBJECT_TO_JSVAL(fun->object);

    /* Compute the unique int/atom symbol id needed by js_LookupProperty. */
    userid = argv[0];
    if (!JS_ValueToId(cx, userid, &symid))
        return JS_FALSE;

    if (!OBJ_CHECK_ACCESS(cx, obj, symid, JSACC_WATCH, &value, &attrs))
        return JS_FALSE;
    if (attrs & JSPROP_READONLY)
        return JS_TRUE;
    return JS_SetWatchPoint(cx, obj, userid, obj_watch_handler, fun->object);
}

static JSBool
obj_unwatch(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JS_ClearWatchPoint(cx, obj, argv[0], NULL, NULL);
    return JS_TRUE;
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
    JSBool sharedPermanent;

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
        sharedPermanent =
            (~sprop->attrs & (JSPROP_SHARED | JSPROP_PERMANENT)) == 0;
        *rval = BOOLEAN_TO_JSVAL(sharedPermanent);
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

    if (!JS_ValueToId(cx, argv[0], &id))
        return JS_FALSE;
    /* XXX ECMA spec error compatible: return false unless hasOwnProperty. */
    if (!OBJ_LOOKUP_PROPERTY(cx, obj, id, &obj2, &prop))
        return JS_FALSE;
    if (prop && obj2 != obj) {
        *rval = JSVAL_FALSE;
        return JS_TRUE;
    }
    if (!OBJ_GET_ATTRIBUTES(cx, obj, id, NULL, &attrs))
        return JS_FALSE;
    *rval = BOOLEAN_TO_JSVAL((attrs & JSPROP_ENUMERATE) != 0);
    return JS_TRUE;
}
#endif /* JS_HAS_NEW_OBJ_METHODS */

#if JS_HAS_GETTER_SETTER
static JSBool
obj_defineGetter(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
    jsval fval, junk;
    jsid id;
    JSBool found;
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
    if (!js_CheckRedeclaration(cx, obj, id, JSPROP_GETTER, &found))
        return JS_FALSE;
    /*
     * Getters and setters are just like watchpoints from an access
     * control point of view.
     */
    if (!OBJ_CHECK_ACCESS(cx, obj, id, JSACC_WATCH, &junk, &attrs))
        return JS_FALSE;
    return OBJ_DEFINE_PROPERTY(cx, obj, id, JSVAL_VOID,
                               (JSPropertyOp) JSVAL_TO_OBJECT(fval), NULL,
                               JSPROP_GETTER, NULL);
}

static JSBool
obj_defineSetter(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
    jsval fval, junk;
    jsid id;
    JSBool found;
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
    if (!js_CheckRedeclaration(cx, obj, id, JSPROP_SETTER, &found))
        return JS_FALSE;
    /*
     * Getters and setters are just like watchpoints from an access
     * control point of view.
     */
    if (!OBJ_CHECK_ACCESS(cx, obj, id, JSACC_WATCH, &junk, &attrs))
        return JS_FALSE;
    return OBJ_DEFINE_PROPERTY(cx, obj, id, JSVAL_VOID,
                               NULL, (JSPropertyOp) JSVAL_TO_OBJECT(fval),
                               JSPROP_SETTER, NULL);
}

static JSBool
obj_lookupGetter(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
    jsid id;
    JSObject *pobj;
    JSScopeProperty *sprop;

    if (!JS_ValueToId(cx, argv[0], &id))
        return JS_FALSE;
    if (!OBJ_LOOKUP_PROPERTY(cx, obj, id, &pobj, (JSProperty **) &sprop))
        return JS_FALSE;
    if (sprop) {
        if (sprop->attrs & JSPROP_GETTER)
            *rval = OBJECT_TO_JSVAL(SPROP_GETTER(sprop, pobj));
        OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
    }
    return JS_TRUE;
}

static JSBool
obj_lookupSetter(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                 jsval *rval)
{
    jsid id;
    JSObject *pobj;
    JSScopeProperty *sprop;

    if (!JS_ValueToId(cx, argv[0], &id))
        return JS_FALSE;
    if (!OBJ_LOOKUP_PROPERTY(cx, obj, id, &pobj, (JSProperty **) &sprop))
        return JS_FALSE;
    if (sprop) {
        if (sprop->attrs & JSPROP_SETTER)
            *rval = OBJECT_TO_JSVAL(SPROP_SETTER(sprop, pobj));
        OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
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
    JSObject *proto = OBJ_GET_PROTO(cx, obj);
    if (!proto)
        return js_LookupProperty(cx, obj, id, objp, propp);
    return OBJ_LOOKUP_PROPERTY(cx, proto, id, objp, propp);
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

    if (JS_HAS_STRICT_OPTION(cx)) {
        if (!JS_ReportErrorFlagsAndNumber(cx,
                                          JSREPORT_WARNING | JSREPORT_STRICT,
                                          js_GetErrorMessage, NULL,
                                          JSMSG_DEPRECATED_USAGE,
                                          js_WithClass.name)) {
            return JS_FALSE;
        }
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

    if (proto && (map = proto->map)->ops == ops) {
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
    if (!newslots)
        goto bad;
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

    if (cx->runtime->objectHook)
        cx->runtime->objectHook(cx, obj, JS_TRUE, cx->runtime->objectHookData);

    return obj;

bad:
    cx->newborn[GCX_OBJECT] = NULL;
    return NULL;
}

static JSBool
FindConstructor(JSContext *cx, JSObject *scope, const char *name, jsval *vp)
{
    JSAtom *atom;
    JSObject *obj;
    JSObject *pobj;
    JSScopeProperty *sprop;

    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom)
        return JS_FALSE;

    if (scope || (cx->fp && (scope = cx->fp->scopeChain) != NULL)) {
        /* Find the topmost object in the scope chain. */
        do {
            obj = scope;
            scope = OBJ_GET_PARENT(cx, obj);
        } while (scope);
    } else {
        obj = cx->globalObject;
        if (!obj) {
            *vp = JSVAL_VOID;
            return JS_TRUE;
        }
    }

    if (!OBJ_LOOKUP_PROPERTY(cx, obj, (jsid)atom, &pobj, (JSProperty**)&sprop))
        return JS_FALSE;
    if (!sprop)  {
        *vp = JSVAL_VOID;
        return JS_TRUE;
    }

    JS_ASSERT(OBJ_IS_NATIVE(pobj));
    JS_ASSERT(SPROP_HAS_VALID_SLOT(sprop));
    *vp = OBJ_GET_SLOT(cx, pobj, sprop->slot);
    OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
    return JS_TRUE;
}

JSObject *
js_ConstructObject(JSContext *cx, JSClass *clasp, JSObject *proto,
                   JSObject *parent)
{
    jsval cval, rval;
    JSObject *obj, *ctor;

    if (!FindConstructor(cx, parent, clasp->name, &cval))
        return NULL;

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

    if (!js_InternalConstruct(cx, obj, cval, 0, NULL, &rval))
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

#if JS_HAS_OBJ_WATCHPOINT
    /* Remove all watchpoints with weak links to obj. */
    JS_ClearWatchPointsForObject(cx, obj);
#endif

    /* Finalize obj first, in case it needs map and slots. */
    OBJ_GET_CLASS(cx, obj)->finalize(cx, obj);

    /* Drop map and free slots. */
    js_DropObjectMap(cx, map, obj);
    obj->map = NULL;
    JS_free(cx, obj->slots - 1);
    obj->slots = NULL;
}

JSBool
js_AllocSlot(JSContext *cx, JSObject *obj, uint32 *slotp)
{
    JSObjectMap *map;
    uint32 nslots, i;
    size_t nbytes;
    jsval *newslots;

    map = obj->map;
    JS_ASSERT(!MAP_IS_NATIVE(map) || ((JSScope *)map)->object == obj);
    nslots = map->nslots;
    if (map->freeslot >= nslots) {
        nslots = map->freeslot;
        JS_ASSERT(nslots >= JS_INITIAL_NSLOTS);
        nslots += (nslots + 1) / 2;

        nbytes = (nslots + 1) * sizeof(jsval);
#if defined(XP_PC) && defined _MSC_VER && _MSC_VER <= 800
        if (nbytes > 60000U) {
            JS_ReportOutOfMemory(cx);
            return JS_FALSE;
        }
#endif

        newslots = (jsval *) JS_realloc(cx, obj->slots - 1, nbytes);
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
    size_t nbytes;
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
        nbytes = (nslots + 1) * sizeof(jsval);
        newslots = (jsval *) JS_realloc(cx, obj->slots - 1, nbytes);
        if (!newslots)
            return;
        newslots[0] = map->nslots = nslots;
        obj->slots = newslots + 1;
    }
}

#if JS_BUG_EMPTY_INDEX_ZERO
#define CHECK_FOR_EMPTY_INDEX(id)                                             \
    JS_BEGIN_MACRO                                                            \
        if (_str->length == 0)                                                \
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
            JSAtom *_atom = (JSAtom *)id;                                     \
            JSString *_str = ATOM_TO_STRING(_atom);                           \
            const jschar *_cp = _str->chars;                                  \
            JSBool _negative = (*_cp == '-');                                 \
            if (_negative) _cp++;                                             \
            if (JS7_ISDEC(*_cp) &&                                            \
                _str->length - _negative <= sizeof(JSVAL_INT_MAX_STRING) - 1) \
            {                                                                 \
                jsuint _index = JS7_UNDEC(*_cp++);                            \
                jsuint _oldIndex = 0;                                         \
                jsuint _c = 0;                                                \
                if (_index != 0) {                                            \
                    while (JS7_ISDEC(*_cp)) {                                 \
                        _oldIndex = _index;                                   \
                        _c = JS7_UNDEC(*_cp);                                 \
                        _index = 10 * _index + _c;                            \
                        _cp++;                                                \
                    }                                                         \
                }                                                             \
                if (*_cp == 0 &&                                              \
                    (_oldIndex < (JSVAL_INT_MAX / 10) ||                      \
                     (_oldIndex == (JSVAL_INT_MAX / 10) &&                    \
                      _c <= (JSVAL_INT_MAX % 10)))) {                         \
                    if (_negative) _index = 0 - _index;                       \
                    id = INT_TO_JSVAL((jsint)_index);                         \
                }                                                             \
            } else {                                                          \
                CHECK_FOR_EMPTY_INDEX(id);                                    \
            }                                                                 \
        }                                                                     \
    JS_END_MACRO


JSBool
js_DefineProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                  JSPropertyOp getter, JSPropertyOp setter, uintN attrs,
                  JSProperty **propp)
{
    JSClass *clasp;
    JSScope *scope;
    JSScopeProperty *sprop;

    /*
     * Handle old bug that took empty string as zero index.  Also convert
     * string indices to integers if appropriate.
     */
    CHECK_FOR_FUNNY_INDEX(id);

    /* Lock if object locking is required by this implementation. */
    JS_LOCK_OBJ(cx, obj);

#if JS_HAS_GETTER_SETTER
    /*
     * If defining a getter or setter, we must check for its counterpart and
     * update the attributes and property ops.  A getter or setter is really
     * only half of a property.
     */
    if (attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
        JSObject *pobj;

        if (!js_LookupProperty(cx, obj, id, &pobj, (JSProperty **)&sprop))
            goto bad;
        if (sprop &&
            pobj == obj &&
            (sprop->attrs & (JSPROP_GETTER | JSPROP_SETTER))) {
            sprop->attrs |= attrs;
            if (attrs & JSPROP_GETTER)
                SPROP_GETTER(sprop, pobj) = getter;
            else
                SPROP_SETTER(sprop, pobj) = setter;
            if (propp)
                *propp = (JSProperty *) sprop;
#ifdef JS_THREADSAFE
            else {
                /* Release sprop and the lock acquired by js_LookupProperty. */
                js_DropProperty(cx, obj, (JSProperty *)sprop);
            }
#endif
            /* Release our lock on obj, in which js_LookupProperty's nested. */
            JS_UNLOCK_OBJ(cx, obj);
            return JS_TRUE;
        }

        if (sprop) {
            /* NB: call OBJ_DROP_PROPERTY, as pobj might not be native. */
            OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
        }
    }
#endif /* JS_HAS_GETTER_SETTER */

    /* Use the object's class getter and setter by default. */
    clasp = LOCKED_OBJ_GET_CLASS(obj);
    if (!getter)
        getter = clasp->getProperty;
    if (!setter)
        setter = clasp->setProperty;

    /* Find a sharable scope, or get a new one for obj. */
    scope = js_MutateScope(cx, obj, id, getter, setter, attrs, &sprop);
    if (!scope)
        goto bad;

    /* Add the property only if MutateScope didn't find a shared scope. */
    if (!sprop) {
        if (clasp->flags & JSCLASS_SHARE_ALL_PROPERTIES)
            attrs |= JSPROP_SHARED;
        sprop = js_NewScopeProperty(cx, scope, id, getter, setter, attrs);
        if (!sprop)
            goto bad;
        /* XXXbe called with lock held */
        if (!clasp->addProperty(cx, obj, sprop->id, &value) ||
            !scope->ops->add(cx, scope, id, sprop)) {
            js_DestroyScopeProperty(cx, scope, sprop);
            goto bad;
        }
        PROPERTY_CACHE_FILL(cx, &cx->runtime->propertyCache, obj, id,
                            (JSProperty *)sprop);
    }

    if (SPROP_HAS_VALID_SLOT(sprop))
        LOCKED_OBJ_SET_SLOT(obj, sprop->slot, value);
    if (propp) {
#ifdef JS_THREADSAFE
        HOLD_SCOPE_PROPERTY(scope, sprop);
#endif
        *propp = (JSProperty *) sprop;
    } else {
        JS_UNLOCK_OBJ(cx, obj);
    }
    return JS_TRUE;

bad:
    JS_UNLOCK_OBJ(cx, obj);
    return JS_FALSE;
}

JS_STATIC_DLL_CALLBACK(const void *)
resolving_GetKey(JSDHashTable *table, JSDHashEntryHdr *hdr)
{
    JSResolvingEntry *entry = (JSResolvingEntry *)hdr;

    return &entry->key;
}

JS_STATIC_DLL_CALLBACK(JSDHashNumber)
resolving_HashKey(JSDHashTable *table, const void *ptr)
{
    const JSResolvingKey *key = (const JSResolvingKey *)ptr;

    return ((JSDHashNumber)key->obj >> JSVAL_TAGBITS) ^ key->id;
}

JS_PUBLIC_API(JSBool)
resolving_MatchEntry(JSDHashTable *table,
                     const JSDHashEntryHdr *hdr,
                     const void *ptr)
{
    const JSResolvingEntry *entry = (const JSResolvingEntry *)hdr;
    const JSResolvingKey *key = (const JSResolvingKey *)ptr;

    return entry->key.obj == key->obj && entry->key.id == key->id;
}

static JSDHashTableOps resolving_dhash_ops = {
    JS_DHashAllocTable,
    JS_DHashFreeTable,
    resolving_GetKey,
    resolving_HashKey,
    resolving_MatchEntry,
    JS_DHashMoveEntryStub,
    JS_DHashClearEntryStub,
    JS_DHashFinalizeStub,
    NULL
};

#if defined JS_THREADSAFE && defined DEBUG
JS_FRIEND_API(JSBool)
_js_LookupProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                   JSProperty **propp, const char *file, uintN line)
#else
JS_FRIEND_API(JSBool)
js_LookupProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                  JSProperty **propp)
#endif
{
    JSHashNumber hash;
    JSScope *scope;
    JSSymbol *sym;
    JSClass *clasp;
    JSResolveOp resolve;
    JSResolvingKey key;
    JSDHashTable *table;
    JSDHashEntryHdr *entry;
    uint32 generation;
    JSNewResolveOp newresolve;
    uintN flags;
    uint32 format;
    JSObject *obj2, *proto;
    JSBool ok;
    JSScopeProperty *sprop;

    /*
     * Handle old bug that took empty string as zero index.  Also convert
     * string indices to integers if appropriate.
     */
    CHECK_FOR_FUNNY_INDEX(id);

    /* Search scopes starting with obj and following the prototype link. */
    hash = js_HashId(id);
    for (;;) {
        JS_LOCK_OBJ(cx, obj);
        SET_OBJ_INFO(obj, file, line);
        scope = OBJ_SCOPE(obj);
        if (scope->object == obj) {
            sym = scope->ops->lookup(cx, scope, id, hash);
        } else {
            /* Shared prototype scope: try resolve before lookup. */
            sym = NULL;
        }

        /* Try obj's class resolve hook if id was not found in obj's scope. */
        if (!sym) {
            clasp = LOCKED_OBJ_GET_CLASS(obj);
            resolve = clasp->resolve;
            if (resolve != JS_ResolveStub) {
                /* Avoid recursion on (obj, id) already being resolved on cx. */
                key.obj = obj;
                key.id = id;
                if (cx->resolving != 0) {
                    table = cx->resolvingTable;
                    entry = JS_DHashTableOperate(table, &key, JS_DHASH_LOOKUP);
                    if (JS_DHASH_ENTRY_IS_BUSY(entry)) {
                        JS_UNLOCK_OBJ(cx, obj);
                        goto out;
                    }
                } else if (!(table = cx->resolvingTable)) {
                    table = JS_NewDHashTable(&resolving_dhash_ops,
                                             NULL,
                                             sizeof(JSResolvingEntry),
                                             JS_DHASH_MIN_SIZE);
                    if (!table)
                        goto outofmem;
                    cx->resolvingTable = table;
                } else {
                    /* cx->resolving is 0, so the table had better be empty! */
                    JS_ASSERT(table->entryCount == 0);
                }

                /*
                 * Once we have successfully added an entry for key and bumped
                 * cx->resolving, control flow must go through cleanup: before
                 * returning.
                 */
                entry = JS_DHashTableOperate(table, &key, JS_DHASH_ADD);
                if (!entry) {
            outofmem:
                    JS_UNLOCK_OBJ(cx, obj);
                    JS_ReportOutOfMemory(cx);
                    return JS_FALSE;
                }
                ((JSResolvingEntry *)entry)->key = key;
                generation = table->generation;
                cx->resolving++;

                /* Null *propp here so we can test it at cleanup: safely. */
                *propp = NULL;

                if (clasp->flags & JSCLASS_NEW_RESOLVE) {
                    newresolve = (JSNewResolveOp)resolve;
                    flags = 0;
                    if (cx->fp && cx->fp->pc) {
                        format = js_CodeSpec[*cx->fp->pc].format;
                        if ((format & JOF_MODEMASK) != JOF_NAME)
                            flags |= JSRESOLVE_QUALIFIED;
                        if ((format & JOF_ASSIGNING) ||
                            (cx->fp->flags & JSFRAME_ASSIGNING)) {
                            flags |= JSRESOLVE_ASSIGNING;
                        }
                    }
                    obj2 = NULL;
                    JS_UNLOCK_OBJ(cx, obj);
                    ok = newresolve(cx, obj, js_IdToValue(id), flags, &obj2);
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
                                sym = scope->ops->lookup(cx, scope, id, hash);
                        }
                        if (obj2 != obj && (!sym || !sym_property(sym))) {
                            JS_UNLOCK_OBJ(cx, obj2);
                            JS_LOCK_OBJ(cx, obj);
                        }
#ifdef DEBUG
                        /*
                         * Set obj so we can assert that its map points to
                         * scope, before we return scope->object in *objp.
                         */
                        if (sym && sym_property(sym))
                            obj = obj2;
#endif
                    }
                } else {
                    /*
                     * Old resolve always requires id re-lookup if obj owns
                     * its scope after resolve returns.
                     */
                    JS_UNLOCK_OBJ(cx, obj);
                    ok = resolve(cx, obj, js_IdToValue(id));
                    if (!ok)
                        goto cleanup;
                    JS_LOCK_OBJ(cx, obj);
                    SET_OBJ_INFO(obj, file, line);
                    scope = OBJ_SCOPE(obj);
                    JS_ASSERT(MAP_IS_NATIVE(&scope->map));
                    if (scope->object == obj)
                        sym = scope->ops->lookup(cx, scope, id, hash);
                }

            cleanup:
                /*
                 * Do a raw remove only if the table hasn't changed since entry
                 * was added, and only if fewer entries were removed than would
                 * cause alpha to be  < .5 (alpha is at most .75).  Otherwise,
                 * use JS_DHashTableOperate to re-lookup the key and remove its
                 * entry, compressing or shrinking the table as needed.
                 */
                if (table->generation == generation &&
                    table->removedCount < JS_DHASH_TABLE_SIZE(table) >> 2) {
                    JS_DHashTableRawRemove(table, entry);
                } else {
                    JS_DHashTableOperate(table, &key, JS_DHASH_REMOVE);
                }
                cx->resolving--;
                if (!ok || *propp)
                    return ok;
            }
        }

        if (sym && (sprop = sym_property(sym)) != NULL) {
            JS_ASSERT(OBJ_SCOPE(obj) == scope);
            *objp = scope->object;      /* XXXbe hide in jsscope.[ch] */
#ifdef JS_THREADSAFE
            HOLD_SCOPE_PROPERTY(scope, sprop);
#endif
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

JS_FRIEND_API(JSBool)
js_FindProperty(JSContext *cx, jsid id, JSObject **objp, JSObject **pobjp,
                JSProperty **propp)
{
    JSRuntime *rt;
    JSObject *obj, *pobj, *lastobj;
    JSProperty *prop;

    rt = cx->runtime;
    obj = cx->fp->scopeChain;
    do {
        /* Try the property cache and return immediately on cache hit. */
        JS_LOCK_OBJ(cx, obj);
        PROPERTY_CACHE_TEST(&rt->propertyCache, obj, id, prop);
        if (prop) {
#ifdef JS_THREADSAFE
            JS_ASSERT(OBJ_IS_NATIVE(obj));
            HOLD_SCOPE_PROPERTY(OBJ_SCOPE(obj), (JSScopeProperty *)prop);
#endif
            *objp = obj;
            *pobjp = obj;
            *propp = prop;
            return JS_TRUE;
        }
        JS_UNLOCK_OBJ(cx, obj);

        /* If cache miss, take the slow path. */
        if (!OBJ_LOOKUP_PROPERTY(cx, obj, id, &pobj, &prop))
            return JS_FALSE;
        if (prop) {
            PROPERTY_CACHE_FILL(cx, &rt->propertyCache, pobj, id, prop);
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

JSBool
js_FindVariable(JSContext *cx, jsid id, JSObject **objp, JSObject **pobjp,
                JSProperty **propp)
{
    JSObject *obj;
    JSProperty *prop;

    /*
     * First look for id's property along the "with" statement and the
     * statically-linked scope chains.
     */
    if (!js_FindProperty(cx, id, objp, pobjp, propp))
        return JS_FALSE;
    if (*propp)
        return JS_TRUE;

    /*
     * Use the top-level scope from the scope chain, which won't end in the
     * same scope as cx->globalObject for cross-context function calls.
     */
    obj = *objp;
    JS_ASSERT(obj);

    /*
     * Make a top-level variable.
     */
    if (JS_HAS_STRICT_OPTION(cx)) {
        JSString *str = JSVAL_TO_STRING(js_IdToValue(id));
        if (!JS_ReportErrorFlagsAndNumber(cx,
                                          JSREPORT_WARNING | JSREPORT_STRICT,
                                          js_GetErrorMessage, NULL,
                                          JSMSG_UNDECLARED_VAR,
                                          JS_GetStringBytes(str))) {
            return JS_FALSE;
        }
    }
    if (!OBJ_DEFINE_PROPERTY(cx, obj, id, JSVAL_VOID, NULL, NULL,
                             JSPROP_ENUMERATE, &prop)) {
        return JS_FALSE;
    }
    *pobjp = obj;
    *propp = prop;
    return JS_TRUE;
}

JSBool
js_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSObject *obj2;
    JSScopeProperty *sprop;
    JSScope *scope;
    uint32 slot;

    /*
     * Handle old bug that took empty string as zero index.  Also convert
     * string indices to integers if appropriate.
     */
    CHECK_FOR_FUNNY_INDEX(id);

    if (!js_LookupProperty(cx, obj, id, &obj2, (JSProperty **)&sprop))
        return JS_FALSE;
    if (!sprop) {
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

        if (!OBJ_GET_CLASS(cx, obj)->getProperty(cx, obj, js_IdToValue(id), vp))
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
            jsbytecode *pc, *endpc;
            JSString *str;

            /* Kludge to allow (typeof foo == "undefined") tests. */
            JS_ASSERT(cx->fp->script);
            pc = cx->fp->pc;
            pc += js_CodeSpec[*pc].length;
            endpc = cx->fp->script->code + cx->fp->script->length;
            while (pc < endpc) {
                if (*pc == JSOP_TYPEOF)
                    return JS_TRUE;
                if (*pc != JSOP_GROUP)
                    break;
                pc++;
            }

            /* Ok, bad undefined property reference: whine about it. */
            str = js_DecompileValueGenerator(cx, JS_FALSE, js_IdToValue(id),
                                             NULL);
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
        OBJ_DROP_PROPERTY(cx, obj2, (JSProperty *)sprop);
        return OBJ_GET_PROPERTY(cx, obj2, id, vp);
    }

    /* Unlock obj2 before calling getter, relock after to avoid deadlock. */
    scope = OBJ_SCOPE(obj2);
    slot = sprop->slot;
    *vp = (slot != SPROP_INVALID_SLOT)
          ? LOCKED_OBJ_GET_SLOT(obj2, slot)
          : JSVAL_VOID;
#ifndef JS_THREADSAFE
    HOLD_SCOPE_PROPERTY(scope, sprop);
#endif
    JS_UNLOCK_SCOPE(cx, scope);
    if (!SPROP_GET(cx, sprop, obj, obj2, vp)) {
        JS_LOCK_OBJ_VOID(cx, obj2, js_DropScopeProperty(cx, scope, sprop));
        return JS_FALSE;
    }
    JS_LOCK_SCOPE(cx, scope);
    sprop = js_DropScopeProperty(cx, scope, sprop);
    if (sprop && SPROP_HAS_VALID_SLOT(sprop)) {
        LOCKED_OBJ_SET_SLOT(obj2, slot, *vp);
        PROPERTY_CACHE_FILL(cx, &cx->runtime->propertyCache, obj2, id,
                            (JSProperty *)sprop);
    }
    JS_UNLOCK_SCOPE(cx, scope);
    return JS_TRUE;
}

JSBool
js_SetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSObject *pobj;
    JSScopeProperty *sprop;
    JSRuntime *rt;
    JSClass *clasp;
    JSHashNumber hash;
    JSScope *scope;
    JSSymbol *sym;
    uintN attrs;
    jsval userid;
    JSPropertyOp getter, setter;
    JSBool ok;
    jsval pval;
    uint32 slot;

    /*
     * Handle old bug that took empty string as zero index.  Also convert
     * string indices to integers if appropriate.
     */
    CHECK_FOR_FUNNY_INDEX(id);

    if (!js_LookupProperty(cx, obj, id, &pobj, (JSProperty **)&sprop))
        return JS_FALSE;

    if (sprop && !OBJ_IS_NATIVE(pobj)) {
        OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
        sprop = NULL;
    }

    rt = cx->runtime;
    clasp = OBJ_GET_CLASS(cx, obj);
    hash = js_HashId(id);

#if JS_HAS_OBJ_WATCHPOINT
    if (!sprop) {
        JS_LOCK_OBJ(cx, obj);
        scope = OBJ_SCOPE(obj);
        sym = scope->ops->lookup(cx, scope, id, hash);
        if (sym) {
            sprop = sym_property(sym);
            if (!sprop && scope->object == obj) {
                /*
                 * Deleted property place-holder, could have a watchpoint that
                 * holds the deleted-but-watched property.  If so, obj->slots
                 * may have shrunk, or at least obj->map->freeslot may have
                 * shrunk due to a delete operation destroying the property.
                 */
                sprop = js_FindWatchPoint(rt, obj, js_IdToValue(id));
                if (sprop) {
                    slot = sprop->slot;
                    if (slot != SPROP_INVALID_SLOT &&
                        slot >= scope->map.freeslot) {
                        if (slot >= scope->map.nslots) {
                            uint32 nslots;
                            jsval *slots;

                            nslots = slot + slot / 2;
                            slots = (jsval *)
                                JS_realloc(cx, obj->slots - 1,
                                           (nslots + 1) * sizeof(jsval));
                            if (!slots) {
                                JS_UNLOCK_OBJ(cx, obj);
                                return JS_FALSE;
                            }
                            slots[0] = scope->map.nslots = nslots;
                            obj->slots = slots + 1;
                        }
                        scope->map.freeslot = slot + 1;
                    }

                    /* Reconnect sprop to its symbol as sym_property(sym). */
                    sym->entry.value = sprop;
                    HOLD_SCOPE_PROPERTY(scope, sprop);
                }
            }
            if (sprop) {
#ifdef JS_THREADSAFE
                HOLD_SCOPE_PROPERTY(scope, sprop);
#endif
                pobj = obj;
            }
        }
        if (!sprop)
            JS_UNLOCK_OBJ(cx, obj);
    }
#endif

    /*
     * Now either sprop is null, meaning id was not found in obj or one of its
     * prototypes; or sprop is non-null, meaning id was found in pobj's scope.
     * If JS_THREADSAFE and sprop is non-null, then scope is locked, and sprop
     * is held: it needs a js_DropScopeProperty followed by a JS_UNLOCK_OBJ (or
     * what is equivalent to those two function calls, an OBJ_DROP_PROPERTY),
     * before we return.
     */
    if (!sprop) {
        attrs = JSPROP_ENUMERATE;
        userid = JSVAL_NULL;
        getter = clasp->getProperty;
        setter = clasp->setProperty;
    } else {
        attrs = sprop->attrs;
        if (attrs & JSPROP_READONLY) {
            /* XXXbe ECMA violation: readonly proto-property stops set cold. */
            OBJ_DROP_PROPERTY(cx, pobj, (JSProperty *)sprop);
            if (!JSVERSION_IS_ECMA(cx->version)) {
                JSString *str = js_DecompileValueGenerator(cx,
                                                           JSDVG_IGNORE_STACK,
                                                           js_IdToValue(id),
                                                           NULL);
                if (str) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                         JSMSG_READ_ONLY,
                                         JS_GetStringBytes(str));
                }
                return JS_FALSE;
            }
            return JS_TRUE;
        }

        /*
         * Set scope for use below.  It was locked by js_LookupProperty, and
         * we know pobj owns it (i.e., scope->object == pobj).  Therefore we
         * optimize JS_UNLOCK_OBJ(cx, pobj) into JS_UNLOCK_SCOPE(cx, scope).
         */
        scope = OBJ_SCOPE(pobj);

        if (pobj != obj) {
            /* Don't clone a setter or shared prototype property. */
            if (attrs & (JSPROP_SETTER | JSPROP_SHARED)) {
#ifndef JS_THREADSAFE
                HOLD_SCOPE_PROPERTY(scope, sprop);
#endif
                JS_UNLOCK_SCOPE(cx, scope);

                ok = SPROP_SET(cx, sprop, obj, pobj, vp);
                JS_LOCK_OBJ_VOID(cx, pobj,
                                 js_DropScopeProperty(cx, scope, sprop));
                return ok;
            }

            /* XXXbe ECMA violation: inherit attrs, getter, setter. */
            userid = sprop->id;
            getter = SPROP_GETTER_SCOPE(sprop, scope);
            setter = SPROP_SETTER_SCOPE(sprop, scope);
            JS_UNLOCK_SCOPE(cx, scope);
            sprop = NULL;
        }
    }

    if (!sprop) {
        /* Find or make a property descriptor with the right heritage. */
        JS_LOCK_OBJ(cx, obj);
        scope = js_MutateScope(cx, obj, id, getter, setter, attrs, &sprop);
        if (!scope) {
            JS_UNLOCK_OBJ(cx, obj);
            return JS_FALSE;
        }
        if (!sprop) {
            if (clasp->flags & JSCLASS_SHARE_ALL_PROPERTIES)
                attrs |= JSPROP_SHARED;
            sprop = js_NewScopeProperty(cx, scope, id, getter, setter, attrs);
            if (!sprop) {
                JS_UNLOCK_OBJ(cx, obj);
                return JS_FALSE;
            }
            if (!JSVAL_IS_NULL(userid))
                sprop->id = userid;
        }

        /* XXXbe called with obj locked */
        if (!clasp->addProperty(cx, obj, sprop->id, vp)) {
            js_DestroyScopeProperty(cx, scope, sprop);
            JS_UNLOCK_SCOPE(cx, scope);
            return JS_FALSE;
        }

        /* Initialize new property value (passed to setter) to undefined. */
        if (SPROP_HAS_VALID_SLOT(sprop))
            LOCKED_OBJ_SET_SLOT(obj, sprop->slot, JSVAL_VOID);

        /* Need a new symbol as well as a new property. */
        sym = scope->ops->add(cx, scope, id, sprop);
        if (!sym) {
            js_DestroyScopeProperty(cx, scope, sprop);
            JS_UNLOCK_SCOPE(cx, scope);
            return JS_FALSE;
        }
#if JS_BUG_AUTO_INDEX_PROPS
        if (SPROP_HAS_VALID_SLOT(sprop)) {
            jsid index_id = (jsid) INT_TO_JSVAL(sprop->slot - JSSLOT_START);
            if (!scope->ops->add(cx, scope, index_id, sprop)) {
                scope->ops->remove(cx, scope, id);
                JS_UNLOCK_SCOPE(cx, scope);
                return JS_FALSE;
            }
            PROPERTY_CACHE_FILL(cx, &rt->propertyCache, obj, index_id,
                                (JSProperty *)sprop);
        }
#endif
        PROPERTY_CACHE_FILL(cx, &rt->propertyCache, obj, id,
                            (JSProperty *)sprop);

#ifdef JS_THREADSAFE
        /* Hold sprop as if it were returned from js_LookupProperty. */
        HOLD_SCOPE_PROPERTY(scope, sprop);
#endif
    }

    /* Get the current property value from its slot. */
    slot = sprop->slot;
    if (slot != SPROP_INVALID_SLOT) {
        JS_ASSERT(slot < obj->map->freeslot);
        pval = LOCKED_OBJ_GET_SLOT(obj, slot);
    }

#ifndef JS_THREADSAFE
    /* Hold sprop across setter callout, and drop after, in case of delete. */
    HOLD_SCOPE_PROPERTY(scope, sprop);
#endif

    /* Avoid deadlock by unlocking obj while calling sprop's setter. */
    JS_UNLOCK_OBJ(cx, obj);

    /* Let the setter modify vp before copying from it to obj->slots[slot]. */
    if (!SPROP_SET(cx, sprop, obj, obj, vp)) {
        JS_LOCK_OBJ_VOID(cx, obj, js_DropScopeProperty(cx, scope, sprop));
        return JS_FALSE;
    }

    /* Relock obj until we are done with sprop. */
    JS_LOCK_OBJ(cx, obj);

    sprop = js_DropScopeProperty(cx, scope, sprop);

    /*
     * Check whether sprop is still around (was not deleted), and whether it
     * has a slot (it may never have had one, or we may have lost a race with
     * someone who cleared scope).
     */
    if (sprop && SPROP_HAS_VALID_SLOT(sprop)) {
        GC_POKE(cx, pval);
        LOCKED_OBJ_SET_SLOT(obj, slot, *vp);

#if JS_BUG_SET_ENUMERATE
        /* Setting a property makes it enumerable. */
        sprop->attrs |= JSPROP_ENUMERATE;
#endif
    }
    JS_UNLOCK_OBJ(cx, obj);
    return JS_TRUE;
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
    sprop->attrs = *attrsp;
    if (noprop)
        OBJ_DROP_PROPERTY(cx, obj, prop);
    return JS_TRUE;
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
    JSSymbol *sym;

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
                if ((~sprop->attrs & (JSPROP_SHARED | JSPROP_PERMANENT)) == 0)
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
        return OBJ_GET_CLASS(cx, obj)->delProperty(cx, obj, js_IdToValue(id),
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
                                         js_IdToValue(id), NULL);
        if (str) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_PERMANENT, JS_GetStringBytes(str));
        }
        return JS_FALSE;
    }

    /* XXXbe called with obj locked */
    if (!LOCKED_OBJ_GET_CLASS(obj)->delProperty(cx, obj, sprop->id, rval)) {
        OBJ_DROP_PROPERTY(cx, obj, prop);
        return JS_FALSE;
    }

    if (SPROP_HAS_VALID_SLOT(sprop))
        GC_POKE(cx, LOCKED_OBJ_GET_SLOT(obj, sprop->slot));
    scope = OBJ_SCOPE(obj);

    /*
     * Purge cache only if prop is not about to be destroyed (since
     * js_DestroyScopeProperty purges for us).
     */
    if (sprop->nrefs != 1) {
        PROPERTY_CACHE_FILL(cx, &cx->runtime->propertyCache, obj, id, NULL);
    }

#if JS_HAS_OBJ_WATCHPOINT
    if (SPROP_SETTER_SCOPE(sprop, scope) == js_watch_set) {
        /*
         * Keep the symbol around with null value in case of re-set.
         * The watchpoint will hold the "deleted" property until it
         * is removed by obj_unwatch or a native JS_ClearWatchPoint.
         * See js_SetProperty for the re-set logic.
         */
        for (sym = sprop->symbols; sym; sym = sym->next) {
            if (sym_id(sym) == id) {
                sym->entry.value = NULL;
                sprop = js_DropScopeProperty(cx, scope, sprop);
                JS_ASSERT(sprop);
                goto out;
            }
        }
    }
#endif /* JS_HAS_OBJ_WATCHPOINT */

    scope->ops->remove(cx, scope, id);
out:
    OBJ_DROP_PROPERTY(cx, obj, prop);
    return JS_TRUE;
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

extern JSIdArray *
js_NewIdArray(JSContext *cx, jsint length)
{
    JSIdArray *ida;

    ida = (JSIdArray *)
        JS_malloc(cx, sizeof(JSIdArray) + (length - 1) * sizeof(jsval));
    if (ida)
        ida->length = length;
    return ida;
}

extern JSIdArray *
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
    JSScopeProperty *sprop;
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
            for (sprop = scope->props; sprop; sprop = sprop->next) {
                if ((sprop->attrs & JSPROP_ENUMERATE) && sprop->symbols)
                    length++;
            }
            ida = js_NewIdArray(cx, length);
            if (!ida) {
                JS_UNLOCK_OBJ(cx, obj);
                goto init_error;
            }
            i = 0;
            for (sprop = scope->props; sprop; sprop = sprop->next) {
                if ((sprop->attrs & JSPROP_ENUMERATE) && sprop->symbols) {
                    JS_ASSERT(i < length);
                    ida->vector[i++] = sym_id(sprop->symbols);
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
               clasp->checkAccess(cx, obj, js_IdToValue(id), mode, vp);
    }
    if (!OBJ_IS_NATIVE(pobj)) {
        OBJ_DROP_PROPERTY(cx, pobj, prop);
        return OBJ_CHECK_ACCESS(cx, pobj, id, mode, vp, attrsp);
    }
    sprop = (JSScopeProperty *)prop;
    *vp = (SPROP_HAS_VALID_SLOT(sprop))
          ? LOCKED_OBJ_GET_SLOT(pobj, sprop->slot)
          : JSVAL_VOID;
    *attrsp = sprop->attrs;
    clasp = LOCKED_OBJ_GET_CLASS(obj);
    if (clasp->checkAccess) {
        JS_UNLOCK_OBJ(cx, pobj);
        ok = clasp->checkAccess(cx, obj, js_IdToValue(id), mode, vp);
        JS_LOCK_OBJ(cx, pobj);
    } else {
        ok = JS_TRUE;
    }
    OBJ_DROP_PROPERTY(cx, pobj, prop);
    return ok;
}

static void
ReportIsNotFunction(JSContext *cx, jsval *vp, JSBool constructing)
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

    js_ReportIsNotFunction(cx, vp, constructing);

    if (fp->down) {
        JS_ASSERT(cx->dormantFrameChain == fp);
        cx->dormantFrameChain = fp->dormantNext;
        fp->dormantNext = NULL;
        cx->fp = fp;
    }
}

JSBool
js_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSClass *clasp;

    clasp = OBJ_GET_CLASS(cx, JSVAL_TO_OBJECT(argv[-2]));
    if (!clasp->call) {
        ReportIsNotFunction(cx, &argv[-2], JS_FALSE);
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
        ReportIsNotFunction(cx, &argv[-2], JS_TRUE);
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

#ifdef JS_THREADSAFE
void
js_DropProperty(JSContext *cx, JSObject *obj, JSProperty *prop)
{
    js_DropScopeProperty(cx, OBJ_SCOPE(obj), (JSScopeProperty *)prop);
    JS_UNLOCK_OBJ(cx, obj);
}
#endif

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
     * as user-defined constructors have a DontEnum | DontDelete prototype (it
     * may be reset), while native or "system" constructors require DontEnum |
     * ReadOnly | DontDelete.
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
    if (OBJ_GET_PROPERTY(cx, obj, (jsid)atom, &fval) &&
        !JSVAL_IS_PRIMITIVE(fval)) {
        ok = js_InternalCall(cx, obj, fval, argc, argv, rval);
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

uint32
js_Mark(JSContext *cx, JSObject *obj, void *arg)
{
    JSScope *scope;
    JSScopeProperty *sprop;
    JSSymbol *sym;
    JSClass *clasp;

    JS_ASSERT(OBJ_IS_NATIVE(obj));
    scope = OBJ_SCOPE(obj);

    for (sprop = scope->props; sprop; sprop = sprop->next) {
        for (sym = sprop->symbols; sym; sym = sym->next) {
            if (JSVAL_IS_INT(sym_id(sym)))
                continue;
            GC_MARK_ATOM(cx, sym_atom(sym), arg);
        }
#if JS_HAS_GETTER_SETTER
        if (sprop->attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
#ifdef GC_MARK_DEBUG
            char buf[64];
            JSAtom *atom = sym_atom(sprop->symbols);
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
                        JSVAL_TO_GCTHING((jsval)
                            SPROP_GETTER_SCOPE(sprop, scope)),
                        buf,
                        arg);
            }
            if (sprop->attrs & JSPROP_SETTER) {
#ifdef GC_MARK_DEBUG
                JS_snprintf(buf, sizeof buf, "%s %s",
                            id, js_setter_str);
#endif
                GC_MARK(cx,
                        JSVAL_TO_GCTHING((jsval)
                            SPROP_SETTER_SCOPE(sprop, scope)),
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
    return JS_MIN(obj->map->freeslot, obj->map->nslots);
}

void
js_Clear(JSContext *cx, JSObject *obj)
{
    JSScope *scope;
    uint32 i, n;

    /*
     * Clear our scope of all symbols and properties, only if we own the scope
     * (i.e., not if obj is unmutated and sharing its prototype's scope).  We
     * do not clear any reserved slots that lie below JSSLOT_FREE(clasp).
     */
    JS_LOCK_OBJ(cx, obj);
    scope = OBJ_SCOPE(obj);
    if (scope->object == obj) {
        scope->ops->clear(cx, scope);

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

void
js_SetRequiredSlot(JSContext *cx, JSObject *obj, uint32 slot, jsval v)
{
    uint32 nslots, rlimit, i;
    JSClass *clasp;
    jsval *newslots;

    JS_LOCK_OBJ(cx, obj);
    nslots = (uint32) obj->slots[-1];
    if (slot >= nslots) {
        clasp = LOCKED_OBJ_GET_CLASS(obj);
        rlimit = JSSLOT_START(clasp) + JSCLASS_RESERVED_SLOTS(clasp);
        JS_ASSERT(slot < rlimit);
        if (rlimit > nslots)
            nslots = rlimit;

        newslots = (jsval *)
            JS_realloc(cx, obj->slots - 1, (nslots + 1) * sizeof(jsval));
        if (!newslots) {
            JS_UNLOCK_OBJ(cx, obj);
            return;
        }
        for (i = 1 + newslots[0]; i <= rlimit; i++)
            newslots[i] = JSVAL_VOID;
        newslots[0] = nslots;
        if (OBJ_SCOPE(obj)->object == obj)
            obj->map->nslots = nslots;
        obj->slots = newslots + 1;
    }

    obj->slots[slot] = v;
    JS_UNLOCK_OBJ(cx, obj);
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
    jsuint i;
    fprintf(stderr, "string (0x%p) \"", (void *)str);
    for (i=0; i < str->length; i++)
        fputc(str->chars[i], stderr);
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
    printVal(cx, js_IdToValue(id));
}

void printAtom(JSAtom *atom) {
    printString(ATOM_TO_STRING(atom));
}

#endif
