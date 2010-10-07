/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79:
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
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsarena.h"
#include "jsbit.h"
#include "jsutil.h"
#include "jshash.h"
#include "jsdhash.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jsbuiltins.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsparse.h"
#include "jsproxy.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstaticcheck.h"
#include "jsstdint.h"
#include "jsstr.h"
#include "jstracer.h"
#include "jsdbgapi.h"
#include "json.h"

#include "jsinterpinlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"
#include "jsobjinlines.h"

#if JS_HAS_GENERATORS
#include "jsiter.h"
#endif

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#if JS_HAS_XDR
#include "jsxdrapi.h"
#endif

#include "jsprobes.h"
#include "jsatominlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "jsautooplen.h"

using namespace js;
using namespace js::gc;

JS_FRIEND_DATA(const JSObjectMap) JSObjectMap::sharedNonNative(JSObjectMap::SHAPELESS);

Class js_ObjectClass = {
    js_Object_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    PropertyStub,   /* addProperty */
    PropertyStub,   /* delProperty */
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub
};

JS_FRIEND_API(JSObject *)
js_ObjectToOuterObject(JSContext *cx, JSObject *obj)
{
    OBJ_TO_OUTER_OBJECT(cx, obj);
    return obj;
}

#if JS_HAS_OBJ_PROTO_PROP

static JSBool
obj_getProto(JSContext *cx, JSObject *obj, jsid id, Value *vp);

static JSBool
obj_setProto(JSContext *cx, JSObject *obj, jsid id, Value *vp);

static JSPropertySpec object_props[] = {
    {js_proto_str, 0, JSPROP_PERMANENT|JSPROP_SHARED, Jsvalify(obj_getProto), Jsvalify(obj_setProto)},
    {0,0,0,0,0}
};

static JSBool
obj_getProto(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    /* Let CheckAccess get the slot's value, based on the access mode. */
    uintN attrs;
    id = ATOM_TO_JSID(cx->runtime->atomState.protoAtom);
    return CheckAccess(cx, obj, id, JSACC_PROTO, vp, &attrs);
}

static JSBool
obj_setProto(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    /* ECMAScript 5 8.6.2 forbids changing [[Prototype]] if not [[Extensible]]. */
    if (!obj->isExtensible()) {
        obj->reportNotExtensible(cx);
        return false;
    }

    if (!vp->isObjectOrNull())
        return JS_TRUE;

    JSObject *pobj = vp->toObjectOrNull();
    if (pobj) {
        /*
         * Innerize pobj here to avoid sticking unwanted properties on the
         * outer object. This ensures that any with statements only grant
         * access to the inner object.
         */
        OBJ_TO_INNER_OBJECT(cx, pobj);
        if (!pobj)
            return JS_FALSE;
    }

    uintN attrs;
    id = ATOM_TO_JSID(cx->runtime->atomState.protoAtom);
    if (!CheckAccess(cx, obj, id, JSAccessMode(JSACC_PROTO|JSACC_WRITE), vp, &attrs))
        return JS_FALSE;

    return SetProto(cx, obj, pobj, JS_TRUE);
}

#else  /* !JS_HAS_OBJ_PROTO_PROP */

#define object_props NULL

#endif /* !JS_HAS_OBJ_PROTO_PROP */

static JSHashNumber
js_hash_object(const void *key)
{
    return JSHashNumber(uintptr_t(key) >> JS_GCTHING_ALIGN);
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
    JSObject *obj2;
    JSProperty *prop;

    JS_CHECK_RECURSION(cx, return NULL);

    map = &cx->sharpObjectMap;
    JS_ASSERT(map->depth >= 1);
    table = map->table;
    hash = js_hash_object(obj);
    hep = JS_HashTableRawLookup(table, hash, obj);
    he = *hep;
    if (!he) {
        sharpid = 0;
        he = JS_HashTableRawAdd(table, hep, hash, obj, (void *) sharpid);
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
            ok = obj->lookupProperty(cx, id, &obj2, &prop);
            if (!ok)
                break;
            if (!prop)
                continue;
            bool hasGetter, hasSetter;
            AutoValueRooter v(cx);
            AutoValueRooter setter(cx);
            if (obj2->isNative()) {
                const Shape *shape = (Shape *) prop;
                hasGetter = shape->hasGetterValue();
                hasSetter = shape->hasSetterValue();
                if (hasGetter)
                    v.set(shape->getterValue());
                if (hasSetter)
                    setter.set(shape->setterValue());
                JS_UNLOCK_OBJ(cx, obj2);
            } else {
                hasGetter = hasSetter = false;
            }
            if (hasSetter) {
                /* Mark the getter, then set val to setter. */
                if (hasGetter && v.value().isObject()) {
                    ok = !!MarkSharpObjects(cx, &v.value().toObject(), NULL);
                    if (!ok)
                        break;
                }
                v.set(setter.value());
            } else if (!hasGetter) {
                ok = obj->getProperty(cx, id, v.addr());
                if (!ok)
                    break;
            }
            if (v.value().isObject() &&
                !MarkSharpObjects(cx, &v.value().toObject(), NULL)) {
                ok = JS_FALSE;
                break;
            }
        }
        if (!ok || !idap)
            JS_DestroyIdArray(cx, ida);
        if (!ok)
            return NULL;
    } else {
        sharpid = uintptr_t(he->value);
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

    if (!JS_CHECK_OPERATION_LIMIT(cx))
        return NULL;

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
        JS_KEEP_ATOMS(cx->runtime);
    }

    /* From this point the control must flow either through out: or bad:. */
    ida = NULL;
    if (map->depth == 0) {
        /*
         * Although MarkSharpObjects tries to avoid invoking getters,
         * it ends up doing so anyway under some circumstances; for
         * example, if a wrapped object has getters, the wrapper will
         * prevent MarkSharpObjects from recognizing them as such.
         * This could lead to js_LeaveSharpObject being called while
         * MarkSharpObjects is still working.
         *
         * Increment map->depth while we call MarkSharpObjects, to
         * ensure that such a call doesn't free the hash table we're
         * still using.
         */
        ++map->depth;
        he = MarkSharpObjects(cx, obj, &ida);
        --map->depth;
        if (!he)
            goto bad;
        JS_ASSERT((uintptr_t(he->value) & SHARP_BIT) == 0);
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
         * converted to strings), i.e., the JSObject::getProperty() call is not
         * idempotent.
         */
        if (!he) {
            he = JS_HashTableRawAdd(table, hep, hash, obj, NULL);
            if (!he) {
                JS_ReportOutOfMemory(cx);
                goto bad;
            }
            sharpid = 0;
            goto out;
        }
    }

    sharpid = uintptr_t(he->value);
    if (sharpid != 0) {
        len = JS_snprintf(buf, sizeof buf, "#%u%c",
                          sharpid >> SHARP_ID_SHIFT,
                          (sharpid & SHARP_BIT) ? '#' : '=');
        *sp = js_InflateString(cx, buf, &len);
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
                    cx->free(*sp);
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
        JS_UNKEEP_ATOMS(cx->runtime);
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
        JS_UNKEEP_ATOMS(cx->runtime);
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

static intN
gc_sharp_table_entry_marker(JSHashEntry *he, intN i, void *arg)
{
    MarkObject((JSTracer *)arg, *(JSObject *)he->key, "sharp table entry");
    return JS_DHASH_NEXT;
}

void
js_TraceSharpMap(JSTracer *trc, JSSharpObjectMap *map)
{
    JS_ASSERT(map->depth > 0);
    JS_ASSERT(map->table);

    /*
     * During recursive calls to MarkSharpObjects a non-native object or
     * object with a custom getProperty method can potentially return an
     * unrooted value or even cut from the object graph an argument of one of
     * MarkSharpObjects recursive invocations. So we must protect map->table
     * entries against GC.
     *
     * We can not simply use JSTempValueRooter to mark the obj argument of
     * MarkSharpObjects during recursion as we have to protect *all* entries
     * in JSSharpObjectMap including those that contains otherwise unreachable
     * objects just allocated through custom getProperty. Otherwise newer
     * allocations can re-use the address of an object stored in the hashtable
     * confusing js_EnterSharpObject. So to address the problem we simply
     * mark all objects from map->table.
     *
     * An alternative "proper" solution is to use JSTempValueRooter in
     * MarkSharpObjects with code to remove during finalization entries
     * with otherwise unreachable objects. But this is way too complex
     * to justify spending efforts.
     */
    JS_HashTableEnumerateEntries(map->table, gc_sharp_table_entry_marker, trc);
}

#if JS_HAS_TOSOURCE
static JSBool
obj_toSource(JSContext *cx, uintN argc, Value *vp)
{
    JSBool ok, outermost;
    JSObject *obj;
    JSHashEntry *he;
    JSIdArray *ida;
    jschar *chars, *ochars, *vsharp;
    const jschar *idstrchars, *vchars;
    size_t nchars, idstrlength, gsoplength, vlength, vsharplength, curlen;
    const char *comma;
    JSObject *obj2;
    JSProperty *prop;
    Value *val;
    JSString *gsop[2];
    JSString *idstr, *valstr, *str;

    JS_CHECK_RECURSION(cx, return JS_FALSE);

    Value localroot[4];
    PodArrayZero(localroot);
    AutoArrayRooter tvr(cx, JS_ARRAY_LENGTH(localroot), localroot);

    /* If outermost, we need parentheses to be an expression, not a block. */
    outermost = (cx->sharpObjectMap.depth == 0);
    obj = ComputeThisFromVp(cx, vp);
    if (!obj || !(he = js_EnterSharpObject(cx, obj, &ida, &chars))) {
        ok = JS_FALSE;
        goto out;
    }
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
        chars = (jschar *) cx->runtime->malloc(((outermost ? 4 : 2) + 1) * sizeof(jschar));
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
            js_realloc((ochars = chars), (nchars + 2 + 1) * sizeof(jschar));
        if (!chars) {
            js_free(ochars);
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

    /*
     * We have four local roots for cooked and raw value GC safety.  Hoist the
     * "localroot + 2" out of the loop using the val local, which refers to
     * the raw (unconverted, "uncooked") values.
     */
    val = localroot + 2;

    for (jsint i = 0, length = ida->length; i < length; i++) {
        /* Get strings for id and value and GC-root them via vp. */
        jsid id = ida->vector[i];

        ok = obj->lookupProperty(cx, id, &obj2, &prop);
        if (!ok)
            goto error;

        /*
         * Convert id to a value and then to a string.  Decide early whether we
         * prefer get/set or old getter/setter syntax.
         */
        idstr = js_ValueToString(cx, IdToValue(id));
        if (!idstr) {
            ok = JS_FALSE;
            obj2->dropProperty(cx, prop);
            goto error;
        }
        vp->setString(idstr);                           /* local root */

        jsint valcnt = 0;
        if (prop) {
            bool doGet = true;
            if (obj2->isNative()) {
                const Shape *shape = (Shape *) prop;
                unsigned attrs = shape->attributes();
                if (attrs & JSPROP_GETTER) {
                    doGet = false;
                    val[valcnt] = shape->getterValue();
                    gsop[valcnt] = ATOM_TO_STRING(cx->runtime->atomState.getAtom);
                    valcnt++;
                }
                if (attrs & JSPROP_SETTER) {
                    doGet = false;
                    val[valcnt] = shape->setterValue();
                    gsop[valcnt] = ATOM_TO_STRING(cx->runtime->atomState.setAtom);
                    valcnt++;
                }
                JS_UNLOCK_OBJ(cx, obj2);
            }
            if (doGet) {
                valcnt = 1;
                gsop[0] = NULL;
                ok = obj->getProperty(cx, id, &val[0]);
                if (!ok)
                    goto error;
            }
        }

        /*
         * If id is a string that's not an identifier, or if it's a negative
         * integer, then it must be quoted.
         */
        bool idIsLexicalIdentifier = !!js_IsIdentifier(idstr);
        if (JSID_IS_ATOM(id)
            ? !idIsLexicalIdentifier
            : (!JSID_IS_INT(id) || JSID_TO_INT(id) < 0)) {
            idstr = js_QuoteString(cx, idstr, jschar('\''));
            if (!idstr) {
                ok = JS_FALSE;
                goto error;
            }
            vp->setString(idstr);                       /* local root */
        }
        idstr->getCharsAndLength(idstrchars, idstrlength);

        for (jsint j = 0; j < valcnt; j++) {
            /*
             * Censor an accessor descriptor getter or setter part if it's
             * undefined.
             */
            if (gsop[j] && val[j].isUndefined())
                continue;

            /* Convert val[j] to its canonical source form. */
            valstr = js_ValueToSource(cx, val[j]);
            if (!valstr) {
                ok = JS_FALSE;
                goto error;
            }
            localroot[j].setString(valstr);             /* local root */
            valstr->getCharsAndLength(vchars, vlength);

            /*
             * If val[j] is a non-sharp object, and we're not serializing an
             * accessor (ECMA syntax can't accommodate sharpened accessors),
             * consider sharpening it.
             */
            vsharp = NULL;
            vsharplength = 0;
#if JS_HAS_SHARP_VARS
            if (!gsop[j] && val[j].isObject() && vchars[0] != '#') {
                he = js_EnterSharpObject(cx, &val[j].toObject(), NULL, &vsharp);
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

            /*
             * Remove '(function ' from the beginning of valstr and ')' from the
             * end so that we can put "get" in front of the function definition.
             */
            if (gsop[j] && IsFunctionObject(val[j])) {
                const jschar *start = vchars;
                const jschar *end = vchars + vlength;

                uint8 parenChomp = 0;
                if (vchars[0] == '(') {
                    vchars++;
                    parenChomp = 1;
                }

                /* Try to jump "function" keyword. */
                if (vchars)
                    vchars = js_strchr_limit(vchars, ' ', end);

                /*
                 * Jump over the function's name: it can't be encoded as part
                 * of an ECMA getter or setter.
                 */
                if (vchars)
                    vchars = js_strchr_limit(vchars, '(', end);

                if (vchars) {
                    if (*vchars == ' ')
                        vchars++;
                    vlength = end - vchars - parenChomp;
                } else {
                    gsop[j] = NULL;
                    vchars = start;
                }
            }

#define SAFE_ADD(n)                                                          \
    JS_BEGIN_MACRO                                                           \
        size_t n_ = (n);                                                     \
        curlen += n_;                                                        \
        if (curlen < n_)                                                     \
            goto overflow;                                                   \
    JS_END_MACRO

            curlen = nchars;
            if (comma)
                SAFE_ADD(2);
            SAFE_ADD(idstrlength + 1);
            if (gsop[j])
                SAFE_ADD(gsop[j]->length() + 1);
            SAFE_ADD(vsharplength);
            SAFE_ADD(vlength);
            /* Account for the trailing null. */
            SAFE_ADD((outermost ? 2 : 1) + 1);
#undef SAFE_ADD

            if (curlen > size_t(-1) / sizeof(jschar))
                goto overflow;

            /* Allocate 1 + 1 at end for closing brace and terminating 0. */
            chars = (jschar *) js_realloc((ochars = chars), curlen * sizeof(jschar));
            if (!chars) {
                /* Save code space on error: let JS_free ignore null vsharp. */
                cx->free(vsharp);
                js_free(ochars);
                goto error;
            }

            if (comma) {
                chars[nchars++] = comma[0];
                chars[nchars++] = comma[1];
            }
            comma = ", ";

            if (gsop[j]) {
                gsoplength = gsop[j]->length();
                js_strncpy(&chars[nchars], gsop[j]->chars(),
                            gsoplength);
                nchars += gsoplength;
                chars[nchars++] = ' ';
            }
            js_strncpy(&chars[nchars], idstrchars, idstrlength);
            nchars += idstrlength;
            /* Extraneous space after id here will be extracted later */
            chars[nchars++] = gsop[j] ? ' ' : ':';

            if (vsharplength) {
                js_strncpy(&chars[nchars], vsharp, vsharplength);
                nchars += vsharplength;
            }
            js_strncpy(&chars[nchars], vchars, vlength);
            nchars += vlength;

            if (vsharp)
                cx->free(vsharp);
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
            js_free(chars);
        goto out;
    }

    if (!chars) {
        JS_ReportOutOfMemory(cx);
        ok = JS_FALSE;
        goto out;
    }
  make_string:
    str = js_NewString(cx, chars, nchars);
    if (!str) {
        js_free(chars);
        ok = JS_FALSE;
        goto out;
    }
    vp->setString(str);
    ok = JS_TRUE;
  out:
    return ok;

  overflow:
    cx->free(vsharp);
    js_free(chars);
    chars = NULL;
    goto error;
}
#endif /* JS_HAS_TOSOURCE */

namespace js {

JSString *
obj_toStringHelper(JSContext *cx, JSObject *obj)
{
    if (obj->isProxy())
        return JSProxy::obj_toString(cx, obj);

    const char *clazz = obj->wrappedObject(cx)->getClass()->name;
    size_t nchars = 9 + strlen(clazz); /* 9 for "[object ]" */
    jschar *chars = (jschar *) cx->malloc((nchars + 1) * sizeof(jschar));
    if (!chars)
        return NULL;

    const char *prefix = "[object ";
    nchars = 0;
    while ((chars[nchars] = (jschar)*prefix) != 0)
        nchars++, prefix++;
    while ((chars[nchars] = (jschar)*clazz) != 0)
        nchars++, clazz++;
    chars[nchars++] = ']';
    chars[nchars] = 0;

    JSString *str = js_NewString(cx, chars, nchars);
    if (!str)
        cx->free(chars);
    return str;
}

}

static JSBool
obj_toString(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ComputeThisFromVp(cx, vp);
    if (!obj)
        return false;

    JSString *str = js::obj_toStringHelper(cx, obj);
    if (!str)
        return false;

    vp->setString(str);
    return true;
}

static JSBool
obj_toLocaleString(JSContext *cx, uintN argc, Value *vp)
{
    if (!ComputeThisFromVp(cx, vp))
        return JS_FALSE;

    JSString *str = js_ValueToString(cx, vp[1]);
    if (!str)
        return JS_FALSE;

    vp->setString(str);
    return JS_TRUE;
}

static JSBool
obj_valueOf(JSContext *cx, uintN argc, Value *vp)
{
    if (!ComputeThisFromVp(cx, vp))
        return JS_FALSE;
    *vp = vp[1];
    return JS_TRUE;
}

/*
 * Check if CSP allows new Function() or eval() to run in the current
 * principals.
 */
JSBool
js_CheckContentSecurityPolicy(JSContext *cx)
{
    JSSecurityCallbacks *callbacks = JS_GetSecurityCallbacks(cx);

    // if there are callbacks, make sure that the CSP callback is installed and
    // that it permits eval().
    if (callbacks && callbacks->contentSecurityPolicyAllows)
        return callbacks->contentSecurityPolicyAllows(cx);

    return JS_TRUE;
}

/*
 * Check whether principals subsumes scopeobj's principals, and return true
 * if so (or if scopeobj has no principals, for backward compatibility with
 * the JS API, which does not require principals), and false otherwise.
 */
JSBool
js_CheckPrincipalsAccess(JSContext *cx, JSObject *scopeobj,
                         JSPrincipals *principals, JSAtom *caller)
{
    JSSecurityCallbacks *callbacks;
    JSPrincipals *scopePrincipals;
    const char *callerstr;

    callbacks = JS_GetSecurityCallbacks(cx);
    if (callbacks && callbacks->findObjectPrincipals) {
        scopePrincipals = callbacks->findObjectPrincipals(cx, scopeobj);
        if (!principals || !scopePrincipals ||
            !principals->subsume(principals, scopePrincipals)) {
            callerstr = js_AtomToPrintableString(cx, caller);
            if (!callerstr)
                return JS_FALSE;
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_INDIRECT_CALL, callerstr);
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

static JSObject *
CheckScopeChainValidity(JSContext *cx, JSObject *scopeobj, const char *caller)
{
    JSObject *inner;

    if (!scopeobj)
        goto bad;

    OBJ_TO_INNER_OBJECT(cx, scopeobj);
    if (!scopeobj)
        return NULL;

    /* XXX This is an awful gross hack. */
    inner = scopeobj;
    while (scopeobj) {
        JSObjectOp op = scopeobj->getClass()->ext.innerObject;
        if (op && op(cx, scopeobj) != scopeobj)
            goto bad;
        scopeobj = scopeobj->getParent();
    }

    return inner;

bad:
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                         JSMSG_BAD_INDIRECT_CALL, caller);
    return NULL;
}

const char *
js_ComputeFilename(JSContext *cx, JSStackFrame *caller,
                   JSPrincipals *principals, uintN *linenop)
{
    uint32 flags;
#ifdef DEBUG
    JSSecurityCallbacks *callbacks = JS_GetSecurityCallbacks(cx);
#endif

    JS_ASSERT(principals || !(callbacks  && callbacks->findObjectPrincipals));
    flags = JS_GetScriptFilenameFlags(caller->script());
    if ((flags & JSFILENAME_PROTECTED) &&
        principals &&
        strcmp(principals->codebase, "[System Principal]")) {
        *linenop = 0;
        return principals->codebase;
    }

    jsbytecode *pc = caller->pc(cx);
    if (pc && js_GetOpcode(cx, caller->script(), pc) == JSOP_EVAL) {
        JS_ASSERT(js_GetOpcode(cx, caller->script(), pc + JSOP_EVAL_LENGTH) == JSOP_LINENO);
        *linenop = GET_UINT16(pc + JSOP_EVAL_LENGTH);
    } else {
        *linenop = js_FramePCToLineNumber(cx, caller);
    }
    return caller->script()->filename;
}

#ifndef EVAL_CACHE_CHAIN_LIMIT
# define EVAL_CACHE_CHAIN_LIMIT 4
#endif

static inline JSScript **
EvalCacheHash(JSContext *cx, JSString *str)
{
    const jschar *s;
    size_t n;
    uint32 h;

    str->getCharsAndLength(s, n);
    if (n > 100)
        n = 100;
    for (h = 0; n; s++, n--)
        h = JS_ROTATE_LEFT32(h, 4) ^ *s;

    h *= JS_GOLDEN_RATIO;
    h >>= 32 - JS_EVAL_CACHE_SHIFT;
    return &JS_SCRIPTS_TO_GC(cx)[h];
}

static JSBool
obj_eval(JSContext *cx, uintN argc, Value *vp)
{
    if (argc < 1) {
        vp->setUndefined();
        return JS_TRUE;
    }

    JSStackFrame *caller = js_GetScriptedCaller(cx, NULL);
    if (!caller) {
        /* Eval code needs to inherit principals from the caller. */
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_INDIRECT_CALL, js_eval_str);
        return JS_FALSE;
    }

    jsbytecode *callerPC = caller->pc(cx);
    bool directCall = (callerPC && js_GetOpcode(cx, caller->script(), callerPC) == JSOP_EVAL);

    /*
     * If the callee was originally a cross-compartment wrapper, this is an
     * indirect call.
     */
    if (directCall && caller->scopeChain().compartment() != vp[0].toObject().compartment())
        directCall = false;

    Value *argv = JS_ARGV(cx, vp);
    if (!argv[0].isString()) {
        *vp = argv[0];
        return JS_TRUE;
    }

    /*
     * We once supported a second argument to eval to use as the scope chain
     * when evaluating the code string.  Warn when such uses are seen so that
     * authors will know that support for eval(s, o) has been removed.
     */
    if (argc > 1 && !caller->script()->warnedAboutTwoArgumentEval) {
        static const char TWO_ARGUMENT_WARNING[] =
            "Support for eval(code, scopeObject) has been removed. "
            "Use |with (scopeObject) eval(code);| instead.";
        if (!JS_ReportWarning(cx, TWO_ARGUMENT_WARNING))
            return JS_FALSE;
        caller->script()->warnedAboutTwoArgumentEval = true;
    }

    /* From here on, control must exit through label out with ok set. */
    MUST_FLOW_THROUGH("out");
    uintN staticLevel = caller->script()->staticLevel + 1;

    JSObject *scopeobj;

    /*
     * Per ES5, if we see an indirect call, then run in the global scope.
     * (eval is specified this way so that the compiler can make assumptions
     * about what bindings may or may not exist in the current frame if it
     * doesn't see 'eval'.)
     */
    if (directCall) {
        /* Compile using the caller's current scope object. */
        scopeobj = js_GetScopeChainFast(cx, caller, JSOP_EVAL,
                                        JSOP_EVAL_LENGTH + JSOP_LINENO_LENGTH);
        if (!scopeobj)
            return JS_FALSE;

        JS_ASSERT_IF(caller->isFunctionFrame(), caller->hasCallObj());
    } else {
        /* Pretend that we're top level. */
        staticLevel = 0;
        scopeobj = vp[0].toObject().getGlobal();
    }

    /* Ensure we compile this eval with the right object in the scope chain. */
    JSObject *result = CheckScopeChainValidity(cx, scopeobj, js_eval_str);
    if (!result)
        return JS_FALSE;
    JS_ASSERT(result == scopeobj);

    /*
     * CSP check: Is eval() allowed at all?
     * Report errors via CSP is done in the script security mgr.
     */
    if (!js_CheckContentSecurityPolicy(cx)) {
        JS_ReportError(cx, "call to eval() blocked by CSP");
        return  JS_FALSE;
    }

    JSObject *callee = &vp[0].toObject();
    JSPrincipals *principals = js_EvalFramePrincipals(cx, callee, caller);
    uintN line;
    const char *file = js_ComputeFilename(cx, caller, principals, &line);

    JSString *str = argv[0].toString();
    JSScript *script = NULL;

    const jschar *chars;
    size_t length;
    str->getCharsAndLength(chars, length);

    /*
     * If the eval string starts with '(' and ends with ')', it may be JSON.
     * Try the JSON parser first because it's much faster.  If the eval string
     * isn't JSON, JSON parsing will probably fail quickly, so little time
     * will be lost.
     */
    if (length > 2 && chars[0] == '(' && chars[length-1] == ')') {
        JSONParser *jp = js_BeginJSONParse(cx, vp, /* suppressErrors = */true);
        JSBool ok = jp != NULL;
        if (ok) {
            /* Run JSON-parser on string inside ( and ). */
            ok = js_ConsumeJSONText(cx, jp, chars+1, length-2);
            ok &= js_FinishJSONParse(cx, jp, NullValue());
            if (ok)
                return JS_TRUE;
        }
    }

    /*
     * Cache local eval scripts indexed by source qualified by scope.
     *
     * An eval cache entry should never be considered a hit unless its
     * strictness matches that of the new eval code. The existing code takes
     * care of this, because hits are qualified by the function from which
     * eval was called, whose strictness doesn't change. Scripts produced by
     * calls to eval from global code are not cached.
     */
    JSScript **bucket = EvalCacheHash(cx, str);
    if (directCall && caller->isFunctionFrame()) {
        uintN count = 0;
        JSScript **scriptp = bucket;

        EVAL_CACHE_METER(probe);
        JSVersion version = cx->findVersion();
        while ((script = *scriptp) != NULL) {
            if (script->savedCallerFun &&
                script->staticLevel == staticLevel &&
                script->version == version &&
                (script->principals == principals ||
                 (principals->subsume(principals, script->principals) &&
                  script->principals->subsume(script->principals, principals)))) {
                /*
                 * Get the prior (cache-filling) eval's saved caller function.
                 * See Compiler::compileScript in jsparse.cpp.
                 */
                JSFunction *fun = script->getFunction(0);

                if (fun == caller->fun()) {
                    /*
                     * Get the source string passed for safekeeping in the
                     * atom map by the prior eval to Compiler::compileScript.
                     */
                    JSString *src = ATOM_TO_STRING(script->atomMap.vector[0]);

                    if (src == str || js_EqualStrings(src, str)) {
                        /*
                         * Source matches, qualify by comparing scopeobj to the
                         * COMPILE_N_GO-memoized parent of the first literal
                         * function or regexp object if any. If none, then this
                         * script has no compiled-in dependencies on the prior
                         * eval's scopeobj.
                         */
                        JSObjectArray *objarray = script->objects();
                        int i = 1;

                        if (objarray->length == 1) {
                            if (script->regexpsOffset != 0) {
                                objarray = script->regexps();
                                i = 0;
                            } else {
                                EVAL_CACHE_METER(noscope);
                                i = -1;
                            }
                        }
                        if (i < 0 ||
                            objarray->vector[i]->getParent() == scopeobj) {
                            JS_ASSERT(staticLevel == script->staticLevel);
                            EVAL_CACHE_METER(hit);
                            *scriptp = script->u.nextToGC;
                            script->u.nextToGC = NULL;
                            break;
                        }
                    }
                }
            }

            if (++count == EVAL_CACHE_CHAIN_LIMIT) {
                script = NULL;
                break;
            }
            EVAL_CACHE_METER(step);
            scriptp = &script->u.nextToGC;
        }
    }

    /*
     * We can't have a callerFrame (down in js_Execute's terms) if we're in
     * global code. This includes indirect eval and direct eval called with a
     * scope object parameter.
     */
    JSStackFrame *callerFrame = (staticLevel != 0) ? caller : NULL;
    if (!script) {
        uint32 tcflags = TCF_COMPILE_N_GO | TCF_NEED_MUTABLE_SCRIPT | TCF_COMPILE_FOR_EVAL;
        script = Compiler::compileScript(cx, scopeobj, callerFrame,
                                         principals, tcflags,
                                         chars, length,
                                         NULL, file, line, str, staticLevel);
        if (!script)
            return JS_FALSE;
    }

    assertSameCompartment(cx, scopeobj, script);

    /*
     * Belt-and-braces: check that the lesser of eval's principals and the
     * caller's principals has access to scopeobj.
     */
    JSBool ok = js_CheckPrincipalsAccess(cx, scopeobj, principals,
                                         cx->runtime->atomState.evalAtom) &&
                Execute(cx, scopeobj, script, callerFrame, JSFRAME_EVAL, vp);

    script->u.nextToGC = *bucket;
    *bucket = script;
#ifdef CHECK_SCRIPT_OWNER
    script->owner = NULL;
#endif

    return ok;
}

#if JS_HAS_OBJ_WATCHPOINT

static JSBool
obj_watch_handler(JSContext *cx, JSObject *obj, jsid id, jsval old,
                  jsval *nvp, void *closure)
{
    JSObject *callable;
    JSSecurityCallbacks *callbacks;
    JSStackFrame *caller;
    JSPrincipals *subject, *watcher;
    JSResolvingKey key;
    JSResolvingEntry *entry;
    uint32 generation;
    Value argv[3];
    JSBool ok;

    callable = (JSObject *) closure;

    callbacks = JS_GetSecurityCallbacks(cx);
    if (callbacks && callbacks->findObjectPrincipals) {
        /* Skip over any obj_watch_* frames between us and the real subject. */
        caller = js_GetScriptedCaller(cx, NULL);
        if (caller) {
            /*
             * Only call the watch handler if the watcher is allowed to watch
             * the currently executing script.
             */
            watcher = callbacks->findObjectPrincipals(cx, callable);
            subject = js_StackFramePrincipals(cx, caller);

            if (watcher && subject && !watcher->subsume(watcher, subject)) {
                /* Silently don't call the watch handler. */
                return JS_TRUE;
            }
        }
    }

    /* Avoid recursion on (obj, id) already being watched on cx. */
    key.obj = obj;
    key.id = id;
    if (!js_StartResolving(cx, &key, JSRESFLAG_WATCH, &entry))
        return JS_FALSE;
    if (!entry)
        return JS_TRUE;
    generation = cx->resolvingTable->generation;

    argv[0] = IdToValue(id);
    argv[1] = Valueify(old);
    argv[2] = Valueify(*nvp);
    ok = ExternalInvoke(cx, obj, ObjectOrNullValue(callable), 3, argv, Valueify(nvp));
    js_StopResolving(cx, &key, JSRESFLAG_WATCH, entry, generation);
    return ok;
}

static JSBool
obj_watch(JSContext *cx, uintN argc, Value *vp)
{
    if (argc <= 1) {
        js_ReportMissingArg(cx, *vp, 1);
        return JS_FALSE;
    }

    JSObject *callable = js_ValueToCallableObject(cx, &vp[3], 0);
    if (!callable)
        return JS_FALSE;

    /* Compute the unique int/atom symbol id needed by js_LookupProperty. */
    jsid propid;
    if (!ValueToId(cx, vp[2], &propid))
        return JS_FALSE;

    JSObject *obj = ComputeThisFromVp(cx, vp);
    Value tmp;
    uintN attrs;
    if (!obj || !CheckAccess(cx, obj, propid, JSACC_WATCH, &tmp, &attrs))
        return JS_FALSE;

    vp->setUndefined();

    if (attrs & JSPROP_READONLY)
        return JS_TRUE;
    if (obj->isDenseArray() && !obj->makeDenseArraySlow(cx))
        return JS_FALSE;
    return JS_SetWatchPoint(cx, obj, propid, obj_watch_handler, callable);
}

static JSBool
obj_unwatch(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ComputeThisFromVp(cx, vp);
    if (!obj)
        return JS_FALSE;
    vp->setUndefined();
    jsid id;
    if (argc != 0) {
        if (!ValueToId(cx, vp[2], &id))
            return JS_FALSE;
    } else {
        id = JSID_VOID;
    }
    return JS_ClearWatchPoint(cx, obj, id, NULL, NULL);
}

#endif /* JS_HAS_OBJ_WATCHPOINT */

/*
 * Prototype and property query methods, to complement the 'in' and
 * 'instanceof' operators.
 */

/* Proposed ECMA 15.2.4.5. */
static JSBool
obj_hasOwnProperty(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ComputeThisFromVp(cx, vp);
    return obj &&
           js_HasOwnPropertyHelper(cx, obj->getOps()->lookupProperty, argc, vp);
}

JSBool
js_HasOwnPropertyHelper(JSContext *cx, LookupPropOp lookup, uintN argc,
                        Value *vp)
{
    jsid id;
    if (!ValueToId(cx, argc != 0 ? vp[2] : UndefinedValue(), &id))
        return JS_FALSE;

    JSObject *obj = ComputeThisFromVp(cx, vp);
    JSObject *obj2;
    JSProperty *prop;
    if (!obj)
        return false;
    if (obj->isProxy()) {
        bool has;
        if (!JSProxy::hasOwn(cx, obj, id, &has))
            return false;
        vp->setBoolean(has);
        return true;
    }
    if (!js_HasOwnProperty(cx, lookup, obj, id, &obj2, &prop))
        return JS_FALSE;
    if (prop) {
        vp->setBoolean(true);
        obj2->dropProperty(cx, prop);
    } else {
        vp->setBoolean(false);
    }
    return JS_TRUE;
}

JSBool
js_HasOwnProperty(JSContext *cx, LookupPropOp lookup, JSObject *obj, jsid id,
                  JSObject **objp, JSProperty **propp)
{
    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED | JSRESOLVE_DETECTING);
    if (!(lookup ? lookup : js_LookupProperty)(cx, obj, id, objp, propp))
        return false;
    if (!*propp)
        return true;

    if (*objp == obj)
        return true;

    Class *clasp = (*objp)->getClass();
    JSObject *outer = NULL;
    if (JSObjectOp op = (*objp)->getClass()->ext.outerObject) {
        outer = op(cx, *objp);
        if (!outer)
            return false;
    }

    if (outer != *objp) {
        if ((*objp)->isNative() && obj->getClass() == clasp) {
            /*
             * The combination of JSPROP_SHARED and JSPROP_PERMANENT in a
             * delegated property makes that property appear to be direct in
             * all delegating instances of the same native class.  This hack
             * avoids bloating every function instance with its own 'length'
             * (AKA 'arity') property.  But it must not extend across class
             * boundaries, to avoid making hasOwnProperty lie (bug 320854).
             *
             * It's not really a hack, of course: a permanent property can't
             * be deleted, and JSPROP_SHARED means "don't allocate a slot in
             * any instance, prototype or delegating".  Without a slot, and
             * without the ability to remove and recreate (with differences)
             * the property, there is no way to tell whether it is directly
             * owned, or indirectly delegated.
             */
            Shape *shape = reinterpret_cast<Shape *>(*propp);
            if (shape->isSharedPermanent())
                return true;
        }

        (*objp)->dropProperty(cx, *propp);
        *propp = NULL;
    }
    return true;
}

/* Proposed ECMA 15.2.4.6. */
static JSBool
obj_isPrototypeOf(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ComputeThisFromVp(cx, vp);
    if (!obj)
        return JS_FALSE;
    const Value &v = argc != 0 ? vp[2] : UndefinedValue();
    vp->setBoolean(js_IsDelegate(cx, obj, v));
    return JS_TRUE;
}

/* Proposed ECMA 15.2.4.7. */
static JSBool
obj_propertyIsEnumerable(JSContext *cx, uintN argc, Value *vp)
{
    jsid id;
    if (!ValueToId(cx, argc != 0 ? vp[2] : UndefinedValue(), &id))
        return JS_FALSE;

    JSObject *obj = ComputeThisFromVp(cx, vp);
    return obj && js_PropertyIsEnumerable(cx, obj, id, vp);
}

JSBool
js_PropertyIsEnumerable(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    JSObject *pobj;
    JSProperty *prop;
    if (!obj->lookupProperty(cx, id, &pobj, &prop))
        return JS_FALSE;

    if (!prop) {
        vp->setBoolean(false);
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
    bool shared;
    uintN attrs;
    if (pobj->isNative()) {
        Shape *shape = (Shape *) prop;
        shared = shape->isSharedPermanent();
        attrs = shape->attributes();
        JS_UNLOCK_OBJ(cx, pobj);
    } else {
        shared = false;
        if (!pobj->getAttributes(cx, id, &attrs))
            return false;
    }
    if (pobj != obj && !shared) {
        vp->setBoolean(false);
        return true;
    }
    vp->setBoolean((attrs & JSPROP_ENUMERATE) != 0);
    return true;
}

#if OLD_GETTER_SETTER_METHODS

const char js_defineGetter_str[] = "__defineGetter__";
const char js_defineSetter_str[] = "__defineSetter__";
const char js_lookupGetter_str[] = "__lookupGetter__";
const char js_lookupSetter_str[] = "__lookupSetter__";

JS_FRIEND_API(JSBool)
js_obj_defineGetter(JSContext *cx, uintN argc, Value *vp)
{
    if (argc <= 1 || !js_IsCallable(vp[3])) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_GETTER_OR_SETTER,
                             js_getter_str);
        return JS_FALSE;
    }
    PropertyOp getter = CastAsPropertyOp(&vp[3].toObject());

    jsid id;
    if (!ValueToId(cx, vp[2], &id))
        return JS_FALSE;
    JSObject *obj = ComputeThisFromVp(cx, vp);
    if (!obj || !CheckRedeclaration(cx, obj, id, JSPROP_GETTER, NULL, NULL))
        return JS_FALSE;
    /*
     * Getters and setters are just like watchpoints from an access
     * control point of view.
     */
    Value junk;
    uintN attrs;
    if (!CheckAccess(cx, obj, id, JSACC_WATCH, &junk, &attrs))
        return JS_FALSE;
    vp->setUndefined();
    return obj->defineProperty(cx, id, UndefinedValue(), getter, PropertyStub,
                               JSPROP_ENUMERATE | JSPROP_GETTER | JSPROP_SHARED);
}

JS_FRIEND_API(JSBool)
js_obj_defineSetter(JSContext *cx, uintN argc, Value *vp)
{
    if (argc <= 1 || !js_IsCallable(vp[3])) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_GETTER_OR_SETTER,
                             js_setter_str);
        return JS_FALSE;
    }
    PropertyOp setter = CastAsPropertyOp(&vp[3].toObject());

    jsid id;
    if (!ValueToId(cx, vp[2], &id))
        return JS_FALSE;
    JSObject *obj = ComputeThisFromVp(cx, vp);
    if (!obj || !CheckRedeclaration(cx, obj, id, JSPROP_SETTER, NULL, NULL))
        return JS_FALSE;
    /*
     * Getters and setters are just like watchpoints from an access
     * control point of view.
     */
    Value junk;
    uintN attrs;
    if (!CheckAccess(cx, obj, id, JSACC_WATCH, &junk, &attrs))
        return JS_FALSE;
    vp->setUndefined();
    return obj->defineProperty(cx, id, UndefinedValue(), PropertyStub, setter,
                               JSPROP_ENUMERATE | JSPROP_SETTER | JSPROP_SHARED);
}

static JSBool
obj_lookupGetter(JSContext *cx, uintN argc, Value *vp)
{
    jsid id;
    if (!ValueToId(cx, argc != 0 ? vp[2] : UndefinedValue(), &id))
        return JS_FALSE;
    JSObject *obj = ComputeThisFromVp(cx, vp);
    JSObject *pobj;
    JSProperty *prop;
    if (!obj || !obj->lookupProperty(cx, id, &pobj, &prop))
        return JS_FALSE;
    vp->setUndefined();
    if (prop) {
        if (pobj->isNative()) {
            Shape *shape = (Shape *) prop;
            if (shape->hasGetterValue())
                *vp = shape->getterValue();
            JS_UNLOCK_OBJ(cx, pobj);
        }
    }
    return JS_TRUE;
}

static JSBool
obj_lookupSetter(JSContext *cx, uintN argc, Value *vp)
{
    jsid id;
    if (!ValueToId(cx, argc != 0 ? vp[2] : UndefinedValue(), &id))
        return JS_FALSE;
    JSObject *obj = ComputeThisFromVp(cx, vp);
    JSObject *pobj;
    JSProperty *prop;
    if (!obj || !obj->lookupProperty(cx, id, &pobj, &prop))
        return JS_FALSE;
    vp->setUndefined();
    if (prop) {
        if (pobj->isNative()) {
            Shape *shape = (Shape *) prop;
            if (shape->hasSetterValue())
                *vp = shape->setterValue();
            JS_UNLOCK_OBJ(cx, pobj);
        }
    }
    return JS_TRUE;
}
#endif /* OLD_GETTER_SETTER_METHODS */

JSBool
obj_getPrototypeOf(JSContext *cx, uintN argc, Value *vp)
{
    if (argc == 0) {
        js_ReportMissingArg(cx, *vp, 0);
        return JS_FALSE;
    }

    if (vp[2].isPrimitive()) {
        char *bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, vp[2], NULL);
        if (!bytes)
            return JS_FALSE;
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_UNEXPECTED_TYPE, bytes, "not an object");
        JS_free(cx, bytes);
        return JS_FALSE;
    }

    JSObject *obj = &vp[2].toObject();
    uintN attrs;
    return CheckAccess(cx, obj, ATOM_TO_JSID(cx->runtime->atomState.protoAtom),
                       JSACC_PROTO, vp, &attrs);
}

extern JSBool
js_NewPropertyDescriptorObject(JSContext *cx, jsid id, uintN attrs,
                               const Value &getter, const Value &setter,
                               const Value &value, Value *vp)
{
    /* We have our own property, so start creating the descriptor. */
    JSObject *desc = NewBuiltinClassInstance(cx, &js_ObjectClass);
    if (!desc)
        return false;
    vp->setObject(*desc);    /* Root and return. */

    const JSAtomState &atomState = cx->runtime->atomState;
    if (attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
        if (!desc->defineProperty(cx, ATOM_TO_JSID(atomState.getAtom), getter,
                                  PropertyStub, PropertyStub, JSPROP_ENUMERATE) ||
            !desc->defineProperty(cx, ATOM_TO_JSID(atomState.setAtom), setter,
                                  PropertyStub, PropertyStub, JSPROP_ENUMERATE)) {
            return false;
        }
    } else {
        if (!desc->defineProperty(cx, ATOM_TO_JSID(atomState.valueAtom), value,
                                  PropertyStub, PropertyStub, JSPROP_ENUMERATE) ||
            !desc->defineProperty(cx, ATOM_TO_JSID(atomState.writableAtom),
                                  BooleanValue((attrs & JSPROP_READONLY) == 0),
                                  PropertyStub, PropertyStub, JSPROP_ENUMERATE)) {
            return false;
        }
    }

    return desc->defineProperty(cx, ATOM_TO_JSID(atomState.enumerableAtom),
                                BooleanValue((attrs & JSPROP_ENUMERATE) != 0),
                                PropertyStub, PropertyStub, JSPROP_ENUMERATE) &&
           desc->defineProperty(cx, ATOM_TO_JSID(atomState.configurableAtom),
                                BooleanValue((attrs & JSPROP_PERMANENT) == 0),
                                PropertyStub, PropertyStub, JSPROP_ENUMERATE);
}

JSBool
js_GetOwnPropertyDescriptor(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    if (obj->isProxy()) {
        if (!JSProxy::getOwnPropertyDescriptor(cx, obj, id, vp))
            return false;
    }

    JSObject *pobj;
    JSProperty *prop;
    if (!js_HasOwnProperty(cx, obj->getOps()->lookupProperty, obj, id, &pobj, &prop))
        return false;
    if (!prop) {
        vp->setUndefined();
        return true;
    }

    Value roots[] = { UndefinedValue(), UndefinedValue(), UndefinedValue() };
    AutoArrayRooter tvr(cx, JS_ARRAY_LENGTH(roots), roots);
    unsigned attrs;
    bool doGet = true;
    if (pobj->isNative()) {
        Shape *shape = (Shape *) prop;
        attrs = shape->attributes();
        if (attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
            doGet = false;
            if (attrs & JSPROP_GETTER)
                roots[0] = shape->getterValue();
            if (attrs & JSPROP_SETTER)
                roots[1] = shape->setterValue();
        }
        JS_UNLOCK_OBJ(cx, pobj);
    } else {
        if (!pobj->getAttributes(cx, id, &attrs))
            return false;
    }

    if (doGet && !obj->getProperty(cx, id, &roots[2]))
        return false;

    return js_NewPropertyDescriptorObject(cx, id,
                                          attrs,
                                          roots[0], /* getter */
                                          roots[1], /* setter */
                                          roots[2], /* value */
                                          vp);
}

static bool
GetFirstArgumentAsObject(JSContext *cx, uintN argc, Value *vp, const char *method, JSObject **objp)
{
    if (argc == 0) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             method, "0", "s");
        return false;
    }

    const Value &v = vp[2];
    if (!v.isObject()) {
        char *bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, v, NULL);
        if (!bytes)
            return false;
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_UNEXPECTED_TYPE,
                             bytes, "not an object");
        JS_free(cx, bytes);
        return false;
    }

    *objp = &v.toObject();
    return true;
}

static JSBool
obj_getOwnPropertyDescriptor(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.getOwnPropertyDescriptor", &obj))
        return JS_FALSE;
    AutoIdRooter nameidr(cx);
    if (!ValueToId(cx, argc >= 2 ? vp[3] : UndefinedValue(), nameidr.addr()))
        return JS_FALSE;
    return js_GetOwnPropertyDescriptor(cx, obj, nameidr.id(), vp);
}

static JSBool
obj_keys(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.keys", &obj))
        return JS_FALSE;

    AutoIdVector props(cx);
    if (!GetPropertyNames(cx, obj, JSITER_OWNONLY, &props))
        return JS_FALSE;

    AutoValueVector vals(cx);
    vals.resize(props.length());
    for (size_t i = 0, len = props.length(); i < len; i++) {
        jsid id = props[i];
        if (JSID_IS_STRING(id)) {
            vals[i].setString(JSID_TO_STRING(id));
        } else {
            JS_ASSERT(JSID_IS_INT(id));
            JSString *str = js_IntToString(cx, JSID_TO_INT(id));
            if (!str)
                return JS_FALSE;
            vals[i].setString(str);
        }
    }

    JS_ASSERT(props.length() <= UINT32_MAX);
    JSObject *aobj = js_NewArrayObject(cx, jsuint(vals.length()), vals.begin());
    if (!aobj)
        return JS_FALSE;
    vp->setObject(*aobj);

    return JS_TRUE;
}

static bool
HasProperty(JSContext* cx, JSObject* obj, jsid id, Value* vp, bool *foundp)
{
    if (!obj->hasProperty(cx, id, foundp, JSRESOLVE_QUALIFIED | JSRESOLVE_DETECTING))
        return false;
    if (!*foundp) {
        vp->setUndefined();
        return true;
    }

    /*
     * We must go through the method read barrier in case id is 'get' or 'set'.
     * There is no obvious way to defer cloning a joined function object whose
     * identity will be used by DefinePropertyOnObject, e.g., or reflected via
     * js_GetOwnPropertyDescriptor, as the getter or setter callable object.
     */
    return !!obj->getProperty(cx, id, vp);
}

PropDesc::PropDesc()
  : pd(UndefinedValue()),
    id(INT_TO_JSID(0)),
    value(UndefinedValue()),
    get(UndefinedValue()),
    set(UndefinedValue()),
    attrs(0),
    hasGet(false),
    hasSet(false),
    hasValue(false),
    hasWritable(false),
    hasEnumerable(false),
    hasConfigurable(false)
{
}

bool
PropDesc::initialize(JSContext* cx, jsid id, const Value &origval)
{
    Value v = origval;
    this->id = id;

    /* 8.10.5 step 1 */
    if (v.isPrimitive()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_NONNULL_OBJECT);
        return false;
    }
    JSObject* desc = &v.toObject();

    /* Make a copy of the descriptor. We might need it later. */
    pd = v;

    /* Start with the proper defaults. */
    attrs = JSPROP_PERMANENT | JSPROP_READONLY;

    bool found;

    /* 8.10.5 step 3 */
    if (!HasProperty(cx, desc, ATOM_TO_JSID(cx->runtime->atomState.enumerableAtom), &v, &found))
        return false;
    if (found) {
        hasEnumerable = JS_TRUE;
        if (js_ValueToBoolean(v))
            attrs |= JSPROP_ENUMERATE;
    }

    /* 8.10.5 step 4 */
    if (!HasProperty(cx, desc, ATOM_TO_JSID(cx->runtime->atomState.configurableAtom), &v, &found))
        return false;
    if (found) {
        hasConfigurable = JS_TRUE;
        if (js_ValueToBoolean(v))
            attrs &= ~JSPROP_PERMANENT;
    }

    /* 8.10.5 step 5 */
    if (!HasProperty(cx, desc, ATOM_TO_JSID(cx->runtime->atomState.valueAtom), &v, &found))
        return false;
    if (found) {
        hasValue = true;
        value = v;
    }

    /* 8.10.6 step 6 */
    if (!HasProperty(cx, desc, ATOM_TO_JSID(cx->runtime->atomState.writableAtom), &v, &found))
        return false;
    if (found) {
        hasWritable = JS_TRUE;
        if (js_ValueToBoolean(v))
            attrs &= ~JSPROP_READONLY;
    }

    /* 8.10.7 step 7 */
    if (!HasProperty(cx, desc, ATOM_TO_JSID(cx->runtime->atomState.getAtom), &v, &found))
        return false;
    if (found) {
        if ((v.isPrimitive() || !js_IsCallable(v)) && !v.isUndefined()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_GET_SET_FIELD,
                                 js_getter_str);
            return false;
        }
        hasGet = true;
        get = v;
        attrs |= JSPROP_GETTER | JSPROP_SHARED;
    }

    /* 8.10.7 step 8 */
    if (!HasProperty(cx, desc, ATOM_TO_JSID(cx->runtime->atomState.setAtom), &v, &found))
        return false;
    if (found) {
        if ((v.isPrimitive() || !js_IsCallable(v)) && !v.isUndefined()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_GET_SET_FIELD,
                                 js_setter_str);
            return false;
        }
        hasSet = true;
        set = v;
        attrs |= JSPROP_SETTER | JSPROP_SHARED;
    }

    /* 8.10.7 step 9 */
    if ((hasGet || hasSet) && (hasValue || hasWritable)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INVALID_DESCRIPTOR);
        return false;
    }

    return true;
}

static JSBool
Reject(JSContext *cx, uintN errorNumber, bool throwError, jsid id, bool *rval)
{
    if (throwError) {
        jsid idstr;
        if (!js_ValueToStringId(cx, IdToValue(id), &idstr))
           return JS_FALSE;
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, errorNumber,
                             JS_GetStringBytes(JSID_TO_STRING(idstr)));
        return JS_FALSE;
    }

    *rval = false;
    return JS_TRUE;
}

static JSBool
Reject(JSContext *cx, JSObject *obj, uintN errorNumber, bool throwError, bool *rval)
{
    if (throwError) {
        if (js_ErrorFormatString[errorNumber].argCount == 1) {
            js_ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber,
                                     JSDVG_IGNORE_STACK, ObjectValue(*obj),
                                     NULL, NULL, NULL);
        } else {
            JS_ASSERT(js_ErrorFormatString[errorNumber].argCount == 0);
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, errorNumber);
        }
        return JS_FALSE;
    }

    *rval = false;
    return JS_TRUE;
}

static JSBool
Reject(JSContext *cx, JSObject *obj, JSProperty *prop, uintN errorNumber, bool throwError,
       jsid id, bool *rval)
{
    obj->dropProperty(cx, prop);
    return Reject(cx, errorNumber, throwError, id, rval);
}

static JSBool
DefinePropertyOnObject(JSContext *cx, JSObject *obj, const PropDesc &desc,
                       bool throwError, bool *rval)
{
    /* 8.12.9 step 1. */
    JSProperty *current;
    JSObject *obj2;
    JS_ASSERT(!obj->getOps()->lookupProperty);
    if (!js_HasOwnProperty(cx, NULL, obj, desc.id, &obj2, &current))
        return JS_FALSE;

    JS_ASSERT(!obj->getOps()->defineProperty);

    /* 8.12.9 steps 2-4. */
    if (!current) {
        if (!obj->isExtensible())
            return Reject(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE, throwError, rval);

        *rval = true;

        if (desc.isGenericDescriptor() || desc.isDataDescriptor()) {
            JS_ASSERT(!obj->getOps()->defineProperty);
            return js_DefineProperty(cx, obj, desc.id, &desc.value,
                                     PropertyStub, PropertyStub, desc.attrs);
        }

        JS_ASSERT(desc.isAccessorDescriptor());

        /*
         * Getters and setters are just like watchpoints from an access
         * control point of view.
         */
        Value dummy;
        uintN dummyAttrs;
        if (!CheckAccess(cx, obj, desc.id, JSACC_WATCH, &dummy, &dummyAttrs))
            return JS_FALSE;

        Value tmp = UndefinedValue();
        return js_DefineProperty(cx, obj, desc.id, &tmp,
                                 desc.getter(), desc.setter(), desc.attrs);
    }

    /* 8.12.9 steps 5-6 (note 5 is merely a special case of 6). */
    Value v = UndefinedValue();

    /*
     * In the special case of shared permanent properties, the "own" property
     * can be found on a different object.  In that case the returned property
     * might not be native, except: the shared permanent property optimization
     * is not applied if the objects have different classes (bug 320854), as
     * must be enforced by js_HasOwnProperty for the Shape cast below to be
     * safe.
     */
    JS_ASSERT(obj->getClass() == obj2->getClass());

    const Shape *shape = reinterpret_cast<Shape *>(current);
    do {
        if (desc.isAccessorDescriptor()) {
            if (!shape->isAccessorDescriptor())
                break;

            if (desc.hasGet &&
                !SameValue(desc.getterValue(), shape->getterOrUndefined(), cx)) {
                break;
            }

            if (desc.hasSet &&
                !SameValue(desc.setterValue(), shape->setterOrUndefined(), cx)) {
                break;
            }
        } else {
            /*
             * Determine the current value of the property once, if the current
             * value might actually need to be used or preserved later.  NB: we
             * guard on whether the current property is a data descriptor to
             * avoid calling a getter; we won't need the value if it's not a
             * data descriptor.
             */
            if (shape->isDataDescriptor()) {
                /*
                 * Non-standard: if the property is non-configurable and is
                 * represented by a native getter or setter, don't permit
                 * redefinition.  We expose properties with native getter/setter
                 * as though they were data properties, for the most part, but
                 * in this particular case we must worry about integrity
                 * concerns for JSAPI users who expected that
                 * permanent+getter/setter means precisely controlled behavior.
                 * If we permitted such redefinitions, such a property could be
                 * "fixed" to some specific previous value, no longer varying
                 * according to the intent of the native getter/setter for the
                 * property.
                 *
                 * Other engines expose properties of this nature using ECMA
                 * getter/setter pairs, but we can't because we use them even
                 * for properties which ECMA specifies as being true data
                 * descriptors ([].length, Function.length, /regex/.lastIndex,
                 * &c.).  Longer-term perhaps we should convert such properties
                 * to use data descriptors (at which point representing a
                 * descriptor with native getter/setter as an accessor
                 * descriptor would be fine) and take a small memory hit, but
                 * for now we'll simply forbid their redefinition.
                 */
                if (!shape->configurable() &&
                    (!shape->hasDefaultGetter() || !shape->hasDefaultSetter())) {
                    return Reject(cx, obj2, current, JSMSG_CANT_REDEFINE_PROP, throwError,
                                  desc.id, rval);
                }

                if (!js_NativeGet(cx, obj, obj2, shape, JSGET_NO_METHOD_BARRIER, &v)) {
                    /* current was dropped when the failure occurred. */
                    return JS_FALSE;
                }
            }

            if (desc.isDataDescriptor()) {
                if (!shape->isDataDescriptor())
                    break;

                if (desc.hasValue && !SameValue(desc.value, v, cx))
                    break;
                if (desc.hasWritable && desc.writable() != shape->writable())
                    break;
            } else {
                /* The only fields in desc will be handled below. */
                JS_ASSERT(desc.isGenericDescriptor());
            }
        }

        if (desc.hasConfigurable && desc.configurable() != shape->configurable())
            break;
        if (desc.hasEnumerable && desc.enumerable() != shape->enumerable())
            break;

        /* The conditions imposed by step 5 or step 6 apply. */
        obj2->dropProperty(cx, current);
        *rval = true;
        return JS_TRUE;
    } while (0);

    /* 8.12.9 step 7. */
    if (!shape->configurable()) {
        /*
         * Since [[Configurable]] defaults to false, we don't need to check
         * whether it was specified.  We can't do likewise for [[Enumerable]]
         * because its putative value is used in a comparison -- a comparison
         * whose result must always be false per spec if the [[Enumerable]]
         * field is not present.  Perfectly pellucid logic, eh?
         */
        JS_ASSERT_IF(!desc.hasConfigurable, !desc.configurable());
        if (desc.configurable() ||
            (desc.hasEnumerable && desc.enumerable() != shape->enumerable())) {
            return Reject(cx, obj2, current, JSMSG_CANT_REDEFINE_PROP, throwError, desc.id, rval);
        }
    }

    bool callDelProperty = false;

    if (desc.isGenericDescriptor()) {
        /* 8.12.9 step 8, no validation required */
    } else if (desc.isDataDescriptor() != shape->isDataDescriptor()) {
        /* 8.12.9 step 9. */
        if (!shape->configurable())
            return Reject(cx, obj2, current, JSMSG_CANT_REDEFINE_PROP, throwError, desc.id, rval);
    } else if (desc.isDataDescriptor()) {
        /* 8.12.9 step 10. */
        JS_ASSERT(shape->isDataDescriptor());
        if (!shape->configurable() && !shape->writable()) {
            if ((desc.hasWritable && desc.writable()) ||
                (desc.hasValue && !SameValue(desc.value, v, cx))) {
                return Reject(cx, obj2, current, JSMSG_CANT_REDEFINE_PROP, throwError, desc.id,
                              rval);
            }
        }

        callDelProperty = !shape->hasDefaultGetter() || !shape->hasDefaultSetter();
    } else {
        /* 8.12.9 step 11. */
        JS_ASSERT(desc.isAccessorDescriptor() && shape->isAccessorDescriptor());
        if (!shape->configurable()) {
            if ((desc.hasSet &&
                 !SameValue(desc.setterValue(), shape->setterOrUndefined(), cx)) ||
                (desc.hasGet &&
                 !SameValue(desc.getterValue(), shape->getterOrUndefined(), cx))) {
                return Reject(cx, obj2, current, JSMSG_CANT_REDEFINE_PROP, throwError, desc.id,
                              rval);
            }
        }
    }

    /* 8.12.9 step 12. */
    uintN attrs;
    PropertyOp getter, setter;
    if (desc.isGenericDescriptor()) {
        uintN changed = 0;
        if (desc.hasConfigurable)
            changed |= JSPROP_PERMANENT;
        if (desc.hasEnumerable)
            changed |= JSPROP_ENUMERATE;

        attrs = (shape->attributes() & ~changed) | (desc.attrs & changed);
        if (shape->isMethod()) {
            JS_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));
            getter = setter = PropertyStub;
        } else {
            getter = shape->getter();
            setter = shape->setter();
        }
    } else if (desc.isDataDescriptor()) {
        uintN unchanged = 0;
        if (!desc.hasConfigurable)
            unchanged |= JSPROP_PERMANENT;
        if (!desc.hasEnumerable)
            unchanged |= JSPROP_ENUMERATE;
        if (!desc.hasWritable)
            unchanged |= JSPROP_READONLY;

        if (desc.hasValue)
            v = desc.value;
        attrs = (desc.attrs & ~unchanged) | (shape->attributes() & unchanged);
        getter = setter = PropertyStub;
    } else {
        JS_ASSERT(desc.isAccessorDescriptor());

        /*
         * Getters and setters are just like watchpoints from an access
         * control point of view.
         */
        Value dummy;
        if (!CheckAccess(cx, obj2, desc.id, JSACC_WATCH, &dummy, &attrs)) {
             obj2->dropProperty(cx, current);
             return JS_FALSE;
        }

        JS_ASSERT_IF(shape->isMethod(), !(attrs & (JSPROP_GETTER | JSPROP_SETTER)));

        /* 8.12.9 step 12. */
        uintN changed = 0;
        if (desc.hasConfigurable)
            changed |= JSPROP_PERMANENT;
        if (desc.hasEnumerable)
            changed |= JSPROP_ENUMERATE;
        if (desc.hasGet)
            changed |= JSPROP_GETTER | JSPROP_SHARED;
        if (desc.hasSet)
            changed |= JSPROP_SETTER | JSPROP_SHARED;

        attrs = (desc.attrs & changed) | (shape->attributes() & ~changed);
        if (desc.hasGet) {
            getter = desc.getter();
        } else {
            getter = (shape->isMethod() || (shape->hasDefaultGetter() && !shape->hasGetterValue()))
                     ? PropertyStub
                     : shape->getter();
        }
        if (desc.hasSet) {
            setter = desc.setter();
        } else {
            setter = (shape->hasDefaultSetter() && !shape->hasSetterValue())
                     ? PropertyStub
                     : shape->setter();
        }
    }

    *rval = true;
    obj2->dropProperty(cx, current);

    /*
     * Since "data" properties implemented using native C functions may rely on
     * side effects during setting, we must make them aware that they have been
     * "assigned"; deleting the property before redefining it does the trick.
     * See bug 539766, where we ran into problems when we redefined
     * arguments.length without making the property aware that its value had
     * been changed (which would have happened if we had deleted it before
     * redefining it or we had invoked its setter to change its value).
     */
    if (callDelProperty) {
        Value dummy;
        if (!CallJSPropertyOp(cx, obj2->getClass()->delProperty, obj2, desc.id, &dummy))
            return false;
    }

    return js_DefineProperty(cx, obj, desc.id, &v, getter, setter, attrs);
}

static JSBool
DefinePropertyOnArray(JSContext *cx, JSObject *obj, const PropDesc &desc,
                      bool throwError, bool *rval)
{
    /*
     * We probably should optimize dense array property definitions where
     * the descriptor describes a traditional array property (enumerable,
     * configurable, writable, numeric index or length without altering its
     * attributes).  Such definitions are probably unlikely, so we don't bother
     * for now.
     */
    if (obj->isDenseArray() && !obj->makeDenseArraySlow(cx))
        return JS_FALSE;

    jsuint oldLen = obj->getArrayLength();

    if (JSID_IS_ATOM(desc.id, cx->runtime->atomState.lengthAtom)) {
        /*
         * Our optimization of storage of the length property of arrays makes
         * it very difficult to properly implement defining the property.  For
         * now simply throw an exception (NB: not merely Reject) on any attempt
         * to define the "length" property, rather than attempting to implement
         * some difficult-for-authors-to-grasp subset of that functionality.
         */
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_DEFINE_ARRAY_LENGTH);
        return JS_FALSE;
    }

    uint32 index;
    if (js_IdIsIndex(desc.id, &index)) {
        /*
        // Disabled until we support defining "length":
        if (index >= oldLen && lengthPropertyNotWritable())
            return ThrowTypeError(cx, JSMSG_CANT_APPEND_TO_ARRAY);
         */
        if (!DefinePropertyOnObject(cx, obj, desc, false, rval))
            return JS_FALSE;
        if (!*rval)
            return Reject(cx, obj, JSMSG_CANT_DEFINE_ARRAY_INDEX, throwError, rval);

        if (index >= oldLen) {
            JS_ASSERT(index != UINT32_MAX);
            obj->setArrayLength(index + 1);
        }

        *rval = true;
        return JS_TRUE;
    }

    return DefinePropertyOnObject(cx, obj, desc, throwError, rval);
}

static JSBool
DefineProperty(JSContext *cx, JSObject *obj, const PropDesc &desc, bool throwError,
               bool *rval)
{
    if (obj->isArray())
        return DefinePropertyOnArray(cx, obj, desc, throwError, rval);

    if (obj->getOps()->lookupProperty) {
        if (obj->isProxy())
            return JSProxy::defineProperty(cx, obj, desc.id, desc.pd);
        return Reject(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE, throwError, rval);
    }

    return DefinePropertyOnObject(cx, obj, desc, throwError, rval);
}

JSBool
js_DefineOwnProperty(JSContext *cx, JSObject *obj, jsid id,
                     const Value &descriptor, JSBool *bp)
{
    AutoPropDescArrayRooter descs(cx);
    PropDesc *desc = descs.append();
    if (!desc || !desc->initialize(cx, id, descriptor))
        return false;

    bool rval;
    if (!DefineProperty(cx, obj, *desc, true, &rval))
        return false;
    *bp = !!rval;
    return true;
}

/* ES5 15.2.3.6: Object.defineProperty(O, P, Attributes) */
static JSBool
obj_defineProperty(JSContext* cx, uintN argc, Value* vp)
{
    /* 15.2.3.6 steps 1 and 5. */
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.defineProperty", &obj))
        return JS_FALSE;
    vp->setObject(*obj);

    /* 15.2.3.6 step 2. */
    AutoIdRooter nameidr(cx);
    if (!ValueToId(cx, argc >= 2 ? vp[3] : UndefinedValue(), nameidr.addr()))
        return JS_FALSE;

    /* 15.2.3.6 step 3. */
    const Value &descval = argc >= 3 ? vp[4] : UndefinedValue();

    /* 15.2.3.6 step 4 */
    JSBool junk;
    return js_DefineOwnProperty(cx, obj, nameidr.id(), descval, &junk);
}

static bool
DefineProperties(JSContext *cx, JSObject *obj, JSObject *props)
{
    AutoIdArray ida(cx, JS_Enumerate(cx, props));
    if (!ida)
        return false;

     AutoPropDescArrayRooter descs(cx);
     size_t len = ida.length();
     for (size_t i = 0; i < len; i++) {
         jsid id = ida[i];
         PropDesc* desc = descs.append();
         AutoValueRooter tvr(cx);
         if (!desc ||
             !JS_GetPropertyById(cx, props, id, tvr.jsval_addr()) ||
             !desc->initialize(cx, id, tvr.value())) {
             return false;
         }
     }

     bool dummy;
     for (size_t i = 0; i < len; i++) {
         if (!DefineProperty(cx, obj, descs[i], true, &dummy))
             return false;
     }

     return true;
}

extern JSBool
js_PopulateObject(JSContext *cx, JSObject *newborn, JSObject *props)
{
    return DefineProperties(cx, newborn, props);
}

/* ES5 15.2.3.7: Object.defineProperties(O, Properties) */
static JSBool
obj_defineProperties(JSContext* cx, uintN argc, Value* vp)
{
    /* 15.2.3.6 steps 1 and 5. */
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.defineProperties", &obj))
        return false;
    vp->setObject(*obj);

    if (argc < 2) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "Object.defineProperties", "0", "s");
        return false;
    }

    JSObject* props = js_ValueToNonNullObject(cx, vp[3]);
    if (!props)
        return false;
    vp[3].setObject(*props);

    return DefineProperties(cx, obj, props);
}

/* ES5 15.2.3.5: Object.create(O [, Properties]) */
static JSBool
obj_create(JSContext *cx, uintN argc, Value *vp)
{
    if (argc == 0) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "Object.create", "0", "s");
        return JS_FALSE;
    }

    const Value &v = vp[2];
    if (!v.isObjectOrNull()) {
        char *bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, v, NULL);
        if (!bytes)
            return JS_FALSE;
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_UNEXPECTED_TYPE,
                             bytes, "not an object or null");
        JS_free(cx, bytes);
        return JS_FALSE;
    }

    /*
     * Use the callee's global as the parent of the new object to avoid dynamic
     * scoping (i.e., using the caller's global).
     */
    JSObject *obj = NewNonFunction<WithProto::Given>(cx, &js_ObjectClass, v.toObjectOrNull(),
                                                        vp->toObject().getGlobal());
    if (!obj)
        return JS_FALSE;
    vp->setObject(*obj); /* Root and prepare for eventual return. */

    /* 15.2.3.5 step 4. */
    if (argc > 1 && !vp[3].isUndefined()) {
        if (vp[3].isPrimitive()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_NONNULL_OBJECT);
            return JS_FALSE;
        }

        JSObject *props = &vp[3].toObject();
        AutoIdArray ida(cx, JS_Enumerate(cx, props));
        if (!ida)
            return JS_FALSE;

        AutoPropDescArrayRooter descs(cx);
        size_t len = ida.length();
        for (size_t i = 0; i < len; i++) {
            jsid id = ida[i];
            PropDesc *desc = descs.append();
            if (!desc || !JS_GetPropertyById(cx, props, id, Jsvalify(&vp[1])) ||
                !desc->initialize(cx, id, vp[1])) {
                return JS_FALSE;
            }
        }

        bool dummy;
        for (size_t i = 0; i < len; i++) {
            if (!DefineProperty(cx, obj, descs[i], true, &dummy))
                return JS_FALSE;
        }
    }

    /* 5. Return obj. */
    return JS_TRUE;
}

static JSBool
obj_getOwnPropertyNames(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.getOwnPropertyNames", &obj))
        return false;

    AutoIdVector keys(cx);
    if (!GetPropertyNames(cx, obj, JSITER_OWNONLY | JSITER_HIDDEN, &keys))
        return false;

    AutoValueVector vals(cx);
    if (!vals.resize(keys.length()))
        return false;

    for (size_t i = 0, len = keys.length(); i < len; i++) {
         jsid id = keys[i];
         if (JSID_IS_INT(id)) {
             JSString *str = js_ValueToString(cx, Int32Value(JSID_TO_INT(id)));
             if (!str)
                 return false;
             vals[i].setString(str);
         } else if (JSID_IS_ATOM(id)) {
             vals[i].setString(JSID_TO_STRING(id));
         } else {
             vals[i].setObject(*JSID_TO_OBJECT(id));
         }
    }

    JSObject *aobj = js_NewArrayObject(cx, vals.length(), vals.begin());
    if (!aobj)
        return false;

    vp->setObject(*aobj);
    return true;
}

static JSBool
obj_isExtensible(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.isExtensible", &obj))
        return false;

    vp->setBoolean(obj->isExtensible());
    return true;
}

static JSBool
obj_preventExtensions(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.preventExtensions", &obj))
        return false;

    vp->setObject(*obj);

    AutoIdVector props(cx);
    return obj->preventExtensions(cx, &props);
}

bool
JSObject::sealOrFreeze(JSContext *cx, bool freeze)
{
    assertSameCompartment(cx, this);

    AutoIdVector props(cx);
    if (isExtensible()) {
        if (!preventExtensions(cx, &props))
            return false;
    } else {
        if (!GetPropertyNames(cx, this, JSITER_HIDDEN | JSITER_OWNONLY, &props))
            return false;
    }

    /* preventExtensions must slowify dense arrays, so we can assign to holes without checks. */
    JS_ASSERT(!isDenseArray());

    for (size_t i = 0, len = props.length(); i < len; i++) {
        jsid id = props[i];

        uintN attrs;
        if (!getAttributes(cx, id, &attrs))
            return false;

        /* Make all attributes permanent; if freezing, make data attributes read-only. */
        uintN new_attrs;
        if (freeze && !(attrs & (JSPROP_GETTER | JSPROP_SETTER)))
            new_attrs = JSPROP_PERMANENT | JSPROP_READONLY;
        else
            new_attrs = JSPROP_PERMANENT;

        /* If we already have the attributes we need, skip the setAttributes call. */
        if ((attrs | new_attrs) == attrs)
            continue;

        attrs |= new_attrs;
        if (!setAttributes(cx, id, &attrs))
            return false;
    }

    return true;
}

static JSBool
obj_freeze(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.freeze", &obj))
        return false;

    vp->setObject(*obj);

    return obj->freeze(cx);
}

static JSBool
obj_isFrozen(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.preventExtensions", &obj))
        return false;

    vp->setBoolean(false);

    if (obj->isExtensible())
        return true; /* The JavaScript value returned is false. */

    AutoIdVector props(cx);
    if (!GetPropertyNames(cx, obj, JSITER_HIDDEN | JSITER_OWNONLY, &props))
        return false;

    for (size_t i = 0, len = props.length(); i < len; i++) {
        jsid id = props[i];

        uintN attrs = 0;
        if (!obj->getAttributes(cx, id, &attrs))
            return false;

        /* The property must be non-configurable and either read-only or an accessor. */
        if (!(attrs & JSPROP_PERMANENT) ||
            !(attrs & (JSPROP_READONLY | JSPROP_GETTER | JSPROP_SETTER)))
            return true; /* The JavaScript value returned is false. */
    }

    /* It really was sealed, so return true. */
    vp->setBoolean(true);
    return true;
}

static JSBool
obj_seal(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.seal", &obj))
        return false;

    vp->setObject(*obj);

    return obj->seal(cx);
}

static JSBool
obj_isSealed(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.isSealed", &obj))
        return false;

    /* Assume not sealed until proven otherwise. */
    vp->setBoolean(false);

    if (obj->isExtensible())
        return true; /* The JavaScript value returned is false. */

    AutoIdVector props(cx);
    if (!GetPropertyNames(cx, obj, JSITER_HIDDEN | JSITER_OWNONLY, &props))
        return false;

    for (size_t i = 0, len = props.length(); i < len; i++) {
        jsid id = props[i];

        uintN attrs;
        if (!obj->getAttributes(cx, id, &attrs))
            return false;

        if (!(attrs & JSPROP_PERMANENT))
            return true; /* The JavaScript value returned is false. */
    }

    /* It really was sealed, so return true. */
    vp->setBoolean(true);
    return true;
}

#if JS_HAS_OBJ_WATCHPOINT
const char js_watch_str[] = "watch";
const char js_unwatch_str[] = "unwatch";
#endif
const char js_hasOwnProperty_str[] = "hasOwnProperty";
const char js_isPrototypeOf_str[] = "isPrototypeOf";
const char js_propertyIsEnumerable_str[] = "propertyIsEnumerable";

static JSFunctionSpec object_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,             obj_toSource,                0,0),
#endif
    JS_FN(js_toString_str,             obj_toString,                0,0),
    JS_FN(js_toLocaleString_str,       obj_toLocaleString,          0,0),
    JS_FN(js_valueOf_str,              obj_valueOf,                 0,0),
#if JS_HAS_OBJ_WATCHPOINT
    JS_FN(js_watch_str,                obj_watch,                   2,0),
    JS_FN(js_unwatch_str,              obj_unwatch,                 1,0),
#endif
    JS_FN(js_hasOwnProperty_str,       obj_hasOwnProperty,          1,0),
    JS_FN(js_isPrototypeOf_str,        obj_isPrototypeOf,           1,0),
    JS_FN(js_propertyIsEnumerable_str, obj_propertyIsEnumerable,    1,0),
#if OLD_GETTER_SETTER_METHODS
    JS_FN(js_defineGetter_str,         js_obj_defineGetter,         2,0),
    JS_FN(js_defineSetter_str,         js_obj_defineSetter,         2,0),
    JS_FN(js_lookupGetter_str,         obj_lookupGetter,            1,0),
    JS_FN(js_lookupSetter_str,         obj_lookupSetter,            1,0),
#endif
    JS_FS_END
};

static JSFunctionSpec object_static_methods[] = {
    JS_FN("getPrototypeOf",            obj_getPrototypeOf,          1,0),
    JS_FN("getOwnPropertyDescriptor",  obj_getOwnPropertyDescriptor,2,0),
    JS_FN("keys",                      obj_keys,                    1,0),
    JS_FN("defineProperty",            obj_defineProperty,          3,0),
    JS_FN("defineProperties",          obj_defineProperties,        2,0),
    JS_FN("create",                    obj_create,                  2,0),
    JS_FN("getOwnPropertyNames",       obj_getOwnPropertyNames,     1,0),
    JS_FN("isExtensible",              obj_isExtensible,            1,0),
    JS_FN("preventExtensions",         obj_preventExtensions,       1,0),
    JS_FN("freeze",                    obj_freeze,                  1,0),
    JS_FN("isFrozen",                  obj_isFrozen,                1,0),
    JS_FN("seal",                      obj_seal,                    1,0),
    JS_FN("isSealed",                  obj_isSealed,                1,0),
    JS_FS_END
};

JSBool
js_Object(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj;
    if (argc == 0) {
        /* Trigger logic below to construct a blank object. */
        obj = NULL;
    } else {
        /* If argv[0] is null or undefined, obj comes back null. */
        if (!js_ValueToObjectOrNull(cx, vp[2], &obj))
            return JS_FALSE;
    }
    if (!obj) {
        /* Make an object whether this was called with 'new' or not. */
        JS_ASSERT(!argc || vp[2].isNull() || vp[2].isUndefined());
        obj = NewBuiltinClassInstance(cx, &js_ObjectClass);
        if (!obj)
            return JS_FALSE;
    }
    vp->setObject(*obj);
    return JS_TRUE;
}

JSObject*
js_CreateThis(JSContext *cx, JSObject *callee)
{
    Class *clasp = callee->getClass();

    Class *newclasp = &js_ObjectClass;
    if (clasp == &js_FunctionClass) {
        JSFunction *fun = callee->getFunctionPrivate();
        if (fun->isNative() && fun->u.n.clasp)
            newclasp = fun->u.n.clasp;
    }

    Value protov;
    if (!callee->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom), &protov))
        return NULL;

    JSObject *proto = protov.isObjectOrNull() ? protov.toObjectOrNull() : NULL;
    JSObject *parent = callee->getParent();
    return NewObject<WithProto::Class>(cx, newclasp, proto, parent);
}

JSObject *
js_CreateThisForFunctionWithProto(JSContext *cx, JSObject *callee, JSObject *proto)
{
    return NewNonFunction<WithProto::Class>(cx, &js_ObjectClass, proto, callee->getParent());
}

JSObject *
js_CreateThisForFunction(JSContext *cx, JSObject *callee)
{
    Value protov;
    if (!callee->getProperty(cx,
                             ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom),
                             &protov)) {
        return NULL;
    }
    JSObject *proto = protov.isObject() ? &protov.toObject() : NULL;
    return js_CreateThisForFunctionWithProto(cx, callee, proto);
}

#ifdef JS_TRACER

static JS_ALWAYS_INLINE JSObject*
NewObjectWithClassProto(JSContext *cx, Class *clasp, JSObject *proto,
                        const Value &privateSlotValue)
{
    JS_ASSERT(clasp->isNative());

    JSObject* obj = js_NewGCObject(cx);
    if (!obj)
        return NULL;

    obj->initSharingEmptyShape(clasp, proto, proto->getParent(), privateSlotValue, cx);
    return obj;
}

JSObject* FASTCALL
js_Object_tn(JSContext* cx, JSObject* proto)
{
    JS_ASSERT(!(js_ObjectClass.flags & JSCLASS_HAS_PRIVATE));

    return NewObjectWithClassProto(cx, &js_ObjectClass, proto, UndefinedValue());
}

JS_DEFINE_TRCINFO_1(js_Object,
    (2, (extern, CONSTRUCTOR_RETRY, js_Object_tn, CONTEXT, CALLEE_PROTOTYPE, 0,
         nanojit::ACCSET_STORE_ANY)))

JSObject* FASTCALL
js_NonEmptyObject(JSContext* cx, JSObject* proto)
{
    JS_ASSERT(!(js_ObjectClass.flags & JSCLASS_HAS_PRIVATE));

    JSObject *obj = NewObjectWithClassProto(cx, &js_ObjectClass, proto, UndefinedValue());
    return (obj && obj->ensureClassReservedSlotsForEmptyObject(cx)) ? obj : NULL;
}

JS_DEFINE_CALLINFO_2(extern, CONSTRUCTOR_RETRY, js_NonEmptyObject, CONTEXT, CALLEE_PROTOTYPE, 0,
                     nanojit::ACCSET_STORE_ANY)

JSObject* FASTCALL
js_String_tn(JSContext* cx, JSObject* proto, JSString* str)
{
    JS_ASSERT(JS_ON_TRACE(cx));
    return NewObjectWithClassProto(cx, &js_StringClass, proto, StringValue(str));
}
JS_DEFINE_CALLINFO_3(extern, OBJECT, js_String_tn, CONTEXT, CALLEE_PROTOTYPE, STRING, 0,
                     nanojit::ACCSET_STORE_ANY)

JSObject* FASTCALL
js_CreateThisFromTrace(JSContext *cx, Class *clasp, JSObject *ctor)
{
    JS_ASSERT(JS_ON_TRACE(cx));
    JS_ASSERT(ctor->isFunction());
#ifdef JS_THREADSAFE
    if (ctor->title.ownercx != cx)
        return NULL;
#endif

    if (!ctor->ensureClassReservedSlots(cx))
        return NULL;

    jsid classPrototypeId = ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
    const Shape *shape = ctor->nativeLookup(classPrototypeId);
    Value pval = shape ? ctor->getSlot(shape->slot) : MagicValue(JS_GENERIC_MAGIC);

    JSObject *parent = ctor->getParent();
    JSObject *proto;
    if (pval.isObject()) {
        /* An object in ctor.prototype, let's use it as the new instance's proto. */
        proto = &pval.toObject();
    } else {
        /* A hole or a primitive: either way, we need to get Object.prototype. */
        if (!js_GetClassPrototype(cx, parent, JSProto_Object, &proto))
            return NULL;

        if (pval.isMagic(JS_GENERIC_MAGIC)) {
            /*
             * No ctor.prototype was set, so we inline-expand and optimize
             * fun_resolve's prototype creation code.
             */
            proto = NewNativeClassInstance(cx, clasp, proto, parent);
            if (!proto)
                return NULL;
            if (!js_SetClassPrototype(cx, ctor, proto, JSPROP_ENUMERATE | JSPROP_PERMANENT))
                return NULL;
        } else {
            /*
             * A primitive value in .prototype means to use Object.prototype
             * for proto. See ES5 13.2.2 step 7.
             */
        }
    }

    /*
     * FIXME: 561785 at least. Quasi-natives including XML objects prevent us
     * from easily or unconditionally calling NewNativeClassInstance here.
     */
    return NewNonFunction<WithProto::Given>(cx, clasp, proto, parent);
}

JS_DEFINE_CALLINFO_3(extern, CONSTRUCTOR_RETRY, js_CreateThisFromTrace, CONTEXT, CLASS, OBJECT, 0,
                     nanojit::ACCSET_STORE_ANY)

#else  /* !JS_TRACER */

# define js_Object_trcinfo NULL

#endif /* !JS_TRACER */

/*
 * Given pc pointing after a property accessing bytecode, return true if the
 * access is "object-detecting" in the sense used by web scripts, e.g., when
 * checking whether document.all is defined.
 */
JS_REQUIRES_STACK JSBool
Detecting(JSContext *cx, jsbytecode *pc)
{
    JSScript *script;
    jsbytecode *endpc;
    JSOp op;
    JSAtom *atom;

    script = cx->fp()->script();
    endpc = script->code + script->length;
    for (;; pc += js_CodeSpec[op].length) {
        JS_ASSERT_IF(!cx->fp()->hasImacropc(), script->code <= pc && pc < endpc);

        /* General case: a branch or equality op follows the access. */
        op = js_GetOpcode(cx, script, pc);
        if (js_CodeSpec[op].format & JOF_DETECTING)
            return JS_TRUE;

        switch (op) {
          case JSOP_NULL:
            /*
             * Special case #1: handle (document.all == null).  Don't sweat
             * about JS1.2's revision of the equality operators here.
             */
            if (++pc < endpc) {
                op = js_GetOpcode(cx, script, pc);
                return *pc == JSOP_EQ || *pc == JSOP_NE;
            }
            return JS_FALSE;

          case JSOP_GETGNAME:
          case JSOP_NAME:
            /*
             * Special case #2: handle (document.all == undefined).  Don't
             * worry about someone redefining undefined, which was added by
             * Edition 3, so is read/write for backward compatibility.
             */
            GET_ATOM_FROM_BYTECODE(script, pc, 0, atom);
            if (atom == cx->runtime->atomState.typeAtoms[JSTYPE_VOID] &&
                (pc += js_CodeSpec[op].length) < endpc) {
                op = js_GetOpcode(cx, script, pc);
                return op == JSOP_EQ || op == JSOP_NE ||
                       op == JSOP_STRICTEQ || op == JSOP_STRICTNE;
            }
            return JS_FALSE;

          default:
            /*
             * At this point, anything but an extended atom index prefix means
             * we're not detecting.
             */
            if (!(js_CodeSpec[op].format & JOF_INDEXBASE))
                return JS_FALSE;
            break;
        }
    }
}

/*
 * Infer lookup flags from the currently executing bytecode. This does
 * not attempt to infer JSRESOLVE_WITH, because the current bytecode
 * does not indicate whether we are in a with statement. Return defaultFlags
 * if a currently executing bytecode cannot be determined.
 */
uintN
js_InferFlags(JSContext *cx, uintN defaultFlags)
{
#ifdef JS_TRACER
    if (JS_ON_TRACE(cx))
        return cx->bailExit->lookupFlags;
#endif

    JS_ASSERT_NOT_ON_TRACE(cx);

    jsbytecode *pc;
    const JSCodeSpec *cs;
    uint32 format;
    uintN flags = 0;

    JSStackFrame *const fp = js_GetTopStackFrame(cx);
    if (!fp || !(pc = cx->regs->pc))
        return defaultFlags;
    cs = &js_CodeSpec[js_GetOpcode(cx, fp->script(), pc)];
    format = cs->format;
    if (JOF_MODE(format) != JOF_NAME)
        flags |= JSRESOLVE_QUALIFIED;
    if ((format & (JOF_SET | JOF_FOR)) || fp->isAssigning()) {
        flags |= JSRESOLVE_ASSIGNING;
    } else if (cs->length >= 0) {
        pc += cs->length;
        JSScript *script = cx->fp()->script();
        if (pc < script->code + script->length && Detecting(cx, pc))
            flags |= JSRESOLVE_DETECTING;
    }
    if (format & JOF_DECLARING)
        flags |= JSRESOLVE_DECLARING;
    return flags;
}

/*
 * ObjectOps and Class for with-statement stack objects.
 */
static JSBool
with_LookupProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                    JSProperty **propp)
{
    /* Fixes bug 463997 */
    uintN flags = cx->resolveFlags;
    if (flags == JSRESOLVE_INFER)
        flags = js_InferFlags(cx, flags);
    flags |= JSRESOLVE_WITH;
    JSAutoResolveFlags rf(cx, flags);
    return obj->getProto()->lookupProperty(cx, id, objp, propp);
}

static JSBool
with_GetProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    return obj->getProto()->getProperty(cx, id, vp);
}

static JSBool
with_SetProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp, JSBool strict)
{
    return obj->getProto()->setProperty(cx, id, vp, strict);
}

static JSBool
with_GetAttributes(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp)
{
    return obj->getProto()->getAttributes(cx, id, attrsp);
}

static JSBool
with_SetAttributes(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp)
{
    return obj->getProto()->setAttributes(cx, id, attrsp);
}

static JSBool
with_DeleteProperty(JSContext *cx, JSObject *obj, jsid id, Value *rval, JSBool strict)
{
    return obj->getProto()->deleteProperty(cx, id, rval, strict);
}

static JSBool
with_Enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
               Value *statep, jsid *idp)
{
    return obj->getProto()->enumerate(cx, enum_op, statep, idp);
}

static JSType
with_TypeOf(JSContext *cx, JSObject *obj)
{
    return JSTYPE_OBJECT;
}

static JSObject *
with_ThisObject(JSContext *cx, JSObject *obj)
{
    return obj->getWithThis();
}

Class js_WithClass = {
    "With",
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(2) | JSCLASS_IS_ANONYMOUS,
    PropertyStub,   /* addProperty */
    PropertyStub,   /* delProperty */
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub,
    NULL,           /* finalize */
    NULL,           /* reserved    */
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* construct   */
    NULL,           /* xdrObject   */
    NULL,           /* hasInstance */
    NULL,           /* mark        */
    JS_NULL_CLASS_EXT,
    {
        with_LookupProperty,
        NULL,       /* defineProperty */
        with_GetProperty,
        with_SetProperty,
        with_GetAttributes,
        with_SetAttributes,
        with_DeleteProperty,
        with_Enumerate,
        with_TypeOf,
        NULL,       /* trace */
        NULL,       /* fix   */
        with_ThisObject,
        NULL,       /* clear */
    }
};

JS_REQUIRES_STACK JSObject *
js_NewWithObject(JSContext *cx, JSObject *proto, JSObject *parent, jsint depth)
{
    JSObject *obj;

    obj = js_NewGCObject(cx);
    if (!obj)
        return NULL;

    obj->init(&js_WithClass, proto, parent, js_FloatingFrameIfGenerator(cx, cx->fp()), cx);
    obj->setMap(cx->runtime->emptyWithShape);
    OBJ_SET_BLOCK_DEPTH(cx, obj, depth);

    AutoObjectRooter tvr(cx, obj);
    JSObject *thisp = proto->thisObject(cx);
    if (!thisp)
        return NULL;

    assertSameCompartment(cx, obj, thisp);

    obj->setWithThis(thisp);
    return obj;
}

JSObject *
js_NewBlockObject(JSContext *cx)
{
    /*
     * Null obj's proto slot so that Object.prototype.* does not pollute block
     * scopes and to give the block object its own scope.
     */
    JSObject *blockObj = js_NewGCObject(cx);
    if (!blockObj)
        return NULL;

    blockObj->init(&js_BlockClass, NULL, NULL, NullValue(), cx);
    blockObj->setMap(cx->runtime->emptyBlockShape);
    return blockObj;
}

JSObject *
js_CloneBlockObject(JSContext *cx, JSObject *proto, JSStackFrame *fp)
{
    JS_ASSERT(proto->isStaticBlock());

    JSObject *clone = js_NewGCObject(cx);
    if (!clone)
        return NULL;

    JSStackFrame *priv = js_FloatingFrameIfGenerator(cx, fp);

    /* The caller sets parent on its own. */
    clone->init(&js_BlockClass, proto, NULL, priv, cx);
    clone->fslots[JSSLOT_BLOCK_DEPTH] = proto->fslots[JSSLOT_BLOCK_DEPTH];

    clone->setMap(proto->map);
    if (!clone->ensureInstanceReservedSlots(cx, OBJ_BLOCK_COUNT(cx, proto)))
        return NULL;

    JS_ASSERT(clone->isClonedBlock());
    return clone;
}

JS_REQUIRES_STACK JSBool
js_PutBlockObject(JSContext *cx, JSBool normalUnwind)
{
    /* Blocks have one fixed slot available for the first local.*/
    JS_STATIC_ASSERT(JS_INITIAL_NSLOTS == JSSLOT_BLOCK_DEPTH + 2);

    JSStackFrame *const fp = cx->fp();
    JSObject *obj = &fp->scopeChain();
    JS_ASSERT(obj->isClonedBlock());
    JS_ASSERT(obj->getPrivate() == js_FloatingFrameIfGenerator(cx, cx->fp()));

    /* Block objects should have all reserved slots allocated early. */
    uintN count = OBJ_BLOCK_COUNT(cx, obj);
    JS_ASSERT(obj->numSlots() == JSSLOT_BLOCK_DEPTH + 1 + count);

    /* The block and its locals must be on the current stack for GC safety. */
    uintN depth = OBJ_BLOCK_DEPTH(cx, obj);
    JS_ASSERT(depth <= size_t(cx->regs->sp - fp->base()));
    JS_ASSERT(count <= size_t(cx->regs->sp - fp->base() - depth));

    /* See comments in CheckDestructuring from jsparse.cpp. */
    JS_ASSERT(count >= 1);

    if (normalUnwind) {
        uintN slot = JSSLOT_BLOCK_FIRST_FREE_SLOT;
        uintN flen = JS_MIN(count, JS_INITIAL_NSLOTS - slot);
        uintN stop = slot + flen;

        depth += fp->numFixed();
        while (slot < stop)
            obj->fslots[slot++] = fp->slots()[depth++];
        count -= flen;
        if (count != 0)
            memcpy(obj->dslots, fp->slots() + depth, count * sizeof(Value));
    }

    /* We must clear the private slot even with errors. */
    obj->setPrivate(NULL);
    fp->setScopeChainNoCallObj(*obj->getParent());
    return normalUnwind;
}

static JSBool
block_getProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    /*
     * Block objects are never exposed to script, and the engine handles them
     * with care. So unlike other getters, this one can assert (rather than
     * check) certain invariants about obj.
     */
    JS_ASSERT(obj->isClonedBlock());
    uintN index = (uintN) JSID_TO_INT(id);
    JS_ASSERT(index < OBJ_BLOCK_COUNT(cx, obj));

    JSStackFrame *fp = (JSStackFrame *) obj->getPrivate();
    if (fp) {
        fp = js_LiveFrameIfGenerator(fp);
        index += fp->numFixed() + OBJ_BLOCK_DEPTH(cx, obj);
        JS_ASSERT(index < fp->numSlots());
        *vp = fp->slots()[index];
        return true;
    }

    /* Values are in slots immediately following the class-reserved ones. */
    JS_ASSERT(obj->getSlot(JSSLOT_FREE(&js_BlockClass) + index) == *vp);
    return true;
}

static JSBool
block_setProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    JS_ASSERT(obj->isClonedBlock());
    uintN index = (uintN) JSID_TO_INT(id);
    JS_ASSERT(index < OBJ_BLOCK_COUNT(cx, obj));

    JSStackFrame *fp = (JSStackFrame *) obj->getPrivate();
    if (fp) {
        fp = js_LiveFrameIfGenerator(fp);
        index += fp->numFixed() + OBJ_BLOCK_DEPTH(cx, obj);
        JS_ASSERT(index < fp->numSlots());
        fp->slots()[index] = *vp;
        return true;
    }

    /*
     * The value in *vp will be written back to the slot in obj that was
     * allocated when this let binding was defined.
     */
    return true;
}

const Shape *
JSObject::defineBlockVariable(JSContext *cx, jsid id, intN index)
{
    JS_ASSERT(isStaticBlock());

    /* Use JSPROP_ENUMERATE to aid the disassembler. */
    uint32 slot = JSSLOT_FREE(&js_BlockClass) + index;
    const Shape *shape = addProperty(cx, id,
                                     block_getProperty, block_setProperty,
                                     slot, JSPROP_ENUMERATE | JSPROP_PERMANENT,
                                     Shape::HAS_SHORTID, index);
    if (!shape)
        return NULL;
    if (slot >= numSlots() && !growSlots(cx, slot + 1))
        return NULL;
    return shape;
}

static size_t
GetObjectSize(JSObject *obj)
{
    return (obj->isFunction() && !obj->getPrivate())
           ? sizeof(JSFunction)
           : sizeof(JSObject);
}

/*
 * Use this method with extreme caution. It trades the guts of two objects and updates
 * scope ownership. This operation is not thread-safe, just as fast array to slow array
 * transitions are inherently not thread-safe. Don't perform a swap operation on objects
 * shared across threads or, or bad things will happen. You have been warned.
 */
void
JSObject::swap(JSObject *other)
{
    size_t size = GetObjectSize(this);
    JS_ASSERT(size == GetObjectSize(other));

    /* Trade the guts of the objects. */
    char tmp[tl::Max<sizeof(JSFunction), sizeof(JSObject)>::result];
    memcpy(tmp, this, size);
    memcpy(this, other, size);
    memcpy(other, tmp, size);
}

#if JS_HAS_XDR

#define NO_PARENT_INDEX ((uint32)-1)

uint32
FindObjectIndex(JSObjectArray *array, JSObject *obj)
{
    size_t i;

    if (array) {
        i = array->length;
        do {

            if (array->vector[--i] == obj)
                return i;
        } while (i != 0);
    }

    return NO_PARENT_INDEX;
}

JSBool
js_XDRBlockObject(JSXDRState *xdr, JSObject **objp)
{
    JSContext *cx;
    uint32 parentId;
    JSObject *obj, *parent;
    uintN depth, count;
    uint32 depthAndCount;
    const Shape *shape;

    cx = xdr->cx;
#ifdef __GNUC__
    obj = NULL;         /* quell GCC overwarning */
#endif

    if (xdr->mode == JSXDR_ENCODE) {
        obj = *objp;
        parent = obj->getParent();
        parentId = (xdr->script->objectsOffset == 0)
                   ? NO_PARENT_INDEX
                   : FindObjectIndex(xdr->script->objects(), parent);
        depth = (uint16)OBJ_BLOCK_DEPTH(cx, obj);
        count = (uint16)OBJ_BLOCK_COUNT(cx, obj);
        depthAndCount = (uint32)(depth << 16) | count;
    }
#ifdef __GNUC__ /* suppress bogus gcc warnings */
    else count = 0;
#endif

    /* First, XDR the parent atomid. */
    if (!JS_XDRUint32(xdr, &parentId))
        return JS_FALSE;

    if (xdr->mode == JSXDR_DECODE) {
        obj = js_NewBlockObject(cx);
        if (!obj)
            return JS_FALSE;
        *objp = obj;

        /*
         * If there's a parent id, then get the parent out of our script's
         * object array. We know that we XDR block object in outer-to-inner
         * order, which means that getting the parent now will work.
         */
        if (parentId == NO_PARENT_INDEX)
            parent = NULL;
        else
            parent = xdr->script->getObject(parentId);
        obj->setParent(parent);
    }

    AutoObjectRooter tvr(cx, obj);

    if (!JS_XDRUint32(xdr, &depthAndCount))
        return false;

    if (xdr->mode == JSXDR_DECODE) {
        depth = (uint16)(depthAndCount >> 16);
        count = (uint16)depthAndCount;
        obj->setSlot(JSSLOT_BLOCK_DEPTH, Value(Int32Value(depth)));

        /*
         * XDR the block object's properties. We know that there are 'count'
         * properties to XDR, stored as id/shortid pairs.
         */
        for (uintN i = 0; i < count; i++) {
            JSAtom *atom;
            uint16 shortid;

            /* XDR the real id, then the shortid. */
            if (!js_XDRAtom(xdr, &atom) || !JS_XDRUint16(xdr, &shortid))
                return false;

            if (!obj->defineBlockVariable(cx, ATOM_TO_JSID(atom), shortid))
                return false;
        }
    } else {
        Vector<const Shape *, 8> shapes(cx);
        shapes.growByUninitialized(count);

        for (Shape::Range r(obj->lastProperty()); !r.empty(); r.popFront()) {
            shape = &r.front();
            shapes[shape->shortid] = shape;
        }

        /*
         * XDR the block object's properties. We know that there are 'count'
         * properties to XDR, stored as id/shortid pairs.
         */
        for (uintN i = 0; i < count; i++) {
            shape = shapes[i];
            JS_ASSERT(shape->getter() == block_getProperty);

            jsid propid = shape->id;
            JS_ASSERT(JSID_IS_ATOM(propid));
            JSAtom *atom = JSID_TO_ATOM(propid);

            uint16 shortid = uint16(shape->shortid);
            JS_ASSERT(shortid == i);

            /* XDR the real id, then the shortid. */
            if (!js_XDRAtom(xdr, &atom) || !JS_XDRUint16(xdr, &shortid))
                return false;
        }
    }
    return true;
}

#endif

Class js_BlockClass = {
    "Block",
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1) | JSCLASS_IS_ANONYMOUS,
    PropertyStub,   /* addProperty */
    PropertyStub,   /* delProperty */
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub
};

JSObject *
js_InitObjectClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto = js_InitClass(cx, obj, NULL, &js_ObjectClass, js_Object, 1,
                                   object_props, object_methods, NULL, object_static_methods);
    if (!proto)
        return NULL;

    /* ECMA (15.1.2.1) says 'eval' is a property of the global object. */
    if (!js_DefineFunction(cx, obj, cx->runtime->atomState.evalAtom, obj_eval, 1,
                           JSFUN_STUB_GSOPS)) {
        return NULL;
    }

    return proto;
}

static bool
DefineStandardSlot(JSContext *cx, JSObject *obj, JSProtoKey key, JSAtom *atom,
                   const Value &v, uint32 attrs, bool &named)
{
    jsid id = ATOM_TO_JSID(atom);

    if (key != JSProto_Null) {
        /*
         * Initializing an actual standard class on a global object. If the
         * property is not yet present, force it into a new one bound to a
         * reserved slot. Otherwise, go through the normal property path.
         */
        JS_ASSERT(obj->getClass()->flags & JSCLASS_IS_GLOBAL);
        JS_ASSERT(obj->isNative());

        JS_LOCK_OBJ(cx, obj);
        if (!obj->ensureClassReservedSlots(cx)) {
            JS_UNLOCK_OBJ(cx, obj);
            return false;
        }

        const Shape *shape = obj->nativeLookup(id);
        if (!shape) {
            uint32 index = 2 * JSProto_LIMIT + key;
            if (!js_SetReservedSlot(cx, obj, index, v)) {
                JS_UNLOCK_OBJ(cx, obj);
                return false;
            }

            uint32 slot = JSSLOT_START(obj->getClass()) + index;
            shape = obj->addProperty(cx, id, PropertyStub, PropertyStub, slot, attrs, 0, 0);

            JS_UNLOCK_OBJ(cx, obj);
            if (!shape)
                return false;

            named = true;
            return true;
        }
        JS_UNLOCK_OBJ(cx, obj);
    }

    named = obj->defineProperty(cx, id, v, PropertyStub, PropertyStub, attrs);
    return named;
}

JSObject *
js_InitClass(JSContext *cx, JSObject *obj, JSObject *parent_proto,
             Class *clasp, Native constructor, uintN nargs,
             JSPropertySpec *ps, JSFunctionSpec *fs,
             JSPropertySpec *static_ps, JSFunctionSpec *static_fs)
{
    JSAtom *atom;
    JSProtoKey key;
    JSFunction *fun;
    bool named = false;

    atom = js_Atomize(cx, clasp->name, strlen(clasp->name), 0);
    if (!atom)
        return NULL;

    /*
     * When initializing a standard class, if no parent_proto (grand-proto of
     * instances of the class, parent-proto of the class's prototype object)
     * is given, we must use Object.prototype if it is available.  Otherwise,
     * we could look up the wrong binding for a class name in obj.  Example:
     *
     *   String = Array;
     *   print("hi there".join);
     *
     * should print undefined, not Array.prototype.join.  This is required by
     * ECMA-262, alas.  It might have been better to make String readonly and
     * permanent in the global object, instead -- but that's too big a change
     * to swallow at this point.
     */
    key = JSCLASS_CACHED_PROTO_KEY(clasp);
    if (key != JSProto_Null &&
        !parent_proto &&
        !js_GetClassPrototype(cx, obj, JSProto_Object, &parent_proto)) {
        return NULL;
    }

    /*
     * Create a prototype object for this class.
     *
     * FIXME: lazy standard (built-in) class initialization and even older
     * eager boostrapping code rely on all of these properties:
     *
     * 1. NewObject attempting to compute a default prototype object when
     *    passed null for proto; and
     *
     * 2. NewObject tolerating no default prototype (null proto slot value)
     *    due to this js_InitClass call coming from js_InitFunctionClass on an
     *    otherwise-uninitialized global.
     *
     * 3. NewObject allocating a JSFunction-sized GC-thing when clasp is
     *    &js_FunctionClass, not a JSObject-sized (smaller) GC-thing.
     *
     * The JS_NewObjectForGivenProto and JS_NewObject APIs also allow clasp to
     * be &js_FunctionClass (we could break compatibility easily). But fixing
     * (3) is not enough without addressing the bootstrapping dependency on (1)
     * and (2).
     */
    JSObject *proto = NewObject<WithProto::Class>(cx, clasp, parent_proto, obj);
    if (!proto)
        return NULL;

    /* After this point, control must exit via label bad or out. */
    AutoObjectRooter tvr(cx, proto);

    JSObject *ctor;
    if (!constructor) {
        /*
         * Lacking a constructor, name the prototype (e.g., Math) unless this
         * class (a) is anonymous, i.e. for internal use only; (b) the class
         * of obj (the global object) is has a reserved slot indexed by key;
         * and (c) key is not the null key.
         */
        if (!(clasp->flags & JSCLASS_IS_ANONYMOUS) ||
            !(obj->getClass()->flags & JSCLASS_IS_GLOBAL) ||
            key == JSProto_Null)
        {
            uint32 attrs = (clasp->flags & JSCLASS_IS_ANONYMOUS)
                           ? JSPROP_READONLY | JSPROP_PERMANENT
                           : 0;
            if (!DefineStandardSlot(cx, obj, key, atom, ObjectValue(*proto), attrs, named))
                goto bad;
        }

        ctor = proto;
    } else {
        fun = js_NewFunction(cx, NULL, constructor, nargs, JSFUN_CONSTRUCTOR, obj, atom);
        if (!fun)
            goto bad;

        AutoValueRooter tvr2(cx, ObjectValue(*fun));
        if (!DefineStandardSlot(cx, obj, key, atom, tvr2.value(), 0, named))
            goto bad;

        /*
         * Remember the class this function is a constructor for so that
         * we know to create an object of this class when we call the
         * constructor.
         */
        FUN_CLASP(fun) = clasp;

        /*
         * Optionally construct the prototype object, before the class has
         * been fully initialized.  Allow the ctor to replace proto with a
         * different object, as is done for operator new -- and as at least
         * XML support requires.
         */
        ctor = FUN_OBJECT(fun);
        if (clasp->flags & JSCLASS_CONSTRUCT_PROTOTYPE) {
            Value rval;
            if (!InvokeConstructorWithGivenThis(cx, proto, ObjectOrNullValue(ctor),
                                                0, NULL, &rval)) {
                goto bad;
            }
            if (rval.isObject() && &rval.toObject() != proto)
                proto = &rval.toObject();
        }

        /* Connect constructor and prototype by named properties. */
        if (!js_SetClassPrototype(cx, ctor, proto,
                                  JSPROP_READONLY | JSPROP_PERMANENT)) {
            goto bad;
        }

        /* Bootstrap Function.prototype (see also JS_InitStandardClasses). */
        if (ctor->getClass() == clasp)
            ctor->setProto(proto);
    }

    /* Add properties and methods to the prototype and the constructor. */
    if ((ps && !JS_DefineProperties(cx, proto, ps)) ||
        (fs && !JS_DefineFunctions(cx, proto, fs)) ||
        (static_ps && !JS_DefineProperties(cx, ctor, static_ps)) ||
        (static_fs && !JS_DefineFunctions(cx, ctor, static_fs))) {
        goto bad;
    }

    /*
     * Make sure proto's emptyShape is available to be shared by objects of
     * this class.  JSObject::emptyShape is a one-slot cache. If we omit this,
     * some other class could snap it up. (The risk is particularly great for
     * Object.prototype.)
     *
     * All callers of JSObject::initSharingEmptyShape depend on this.
     *
     * FIXME: bug 592296 -- js_InitArrayClass should pass &js_SlowArrayClass
     * and make the Array.prototype slow from the start.
     */
    JS_ASSERT_IF(proto->clasp != clasp,
                 clasp == &js_ArrayClass && proto->clasp == &js_SlowArrayClass);
    if (!proto->getEmptyShape(cx, proto->clasp))
        goto bad;

    /* If this is a standard class, cache its prototype. */
    if (key != JSProto_Null && !js_SetClassObject(cx, obj, key, ctor, proto))
        goto bad;

    return proto;

bad:
    if (named) {
        Value rval;
        obj->deleteProperty(cx, ATOM_TO_JSID(atom), &rval, false);
    }
    return NULL;
}

bool
JSObject::allocSlots(JSContext *cx, size_t nslots)
{
    JS_ASSERT(!dslots);
    JS_ASSERT(nslots > JS_INITIAL_NSLOTS);

    size_t nwords = slotsToDynamicWords(nslots);
    dslots = (Value*) cx->malloc(nwords * sizeof(Value));
    if (!dslots)
        return false;

    dslots++;
    dslots[-1].setPrivateUint32(nslots);
    SetValueRangeToUndefined(dslots, nslots - JS_INITIAL_NSLOTS);
    return true;
}

bool
JSObject::growSlots(JSContext *cx, size_t nslots)
{
    /*
     * Minimal number of dynamic slots to allocate.
     */
    const size_t MIN_DYNAMIC_WORDS = 4;

    /*
     * The limit to switch to linear allocation strategy from the power of 2
     * growth no to waste too much memory.
     */
    const size_t LINEAR_GROWTH_STEP = JS_BIT(16);

    /* If we are allocating fslots, there is nothing to do. */
    if (nslots <= JS_INITIAL_NSLOTS)
        return true;

    /* Don't let nslots get close to wrapping around uint32. */
    if (nslots >= NSLOTS_LIMIT) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    size_t nwords = slotsToDynamicWords(nslots);

    /*
     * Round up nslots so the number of bytes in dslots array is power
     * of 2 to ensure exponential grouth.
     */
    uintN log;
    if (nwords <= MIN_DYNAMIC_WORDS) {
        nwords = MIN_DYNAMIC_WORDS;
    } else if (nwords < LINEAR_GROWTH_STEP) {
        JS_CEILING_LOG2(log, nwords);
        nwords = JS_BIT(log);
    } else {
        nwords = JS_ROUNDUP(nwords, LINEAR_GROWTH_STEP);
    }
    nslots = dynamicWordsToSlots(nwords);

    /*
     * If nothing was allocated yet, treat it as initial allocation (but with
     * the exponential growth algorithm applied).
     */
    if (!dslots)
        return allocSlots(cx, nslots);

    size_t oldnslots = dslots[-1].toPrivateUint32();

    Value *tmpdslots = (Value*) cx->realloc(dslots - 1, nwords * sizeof(Value));
    if (!tmpdslots)
        return false;   /* leave dslots at its old size */

    dslots = tmpdslots;
    dslots++;
    dslots[-1].setPrivateUint32(nslots);

    /* Initialize the additional slots we added. */
    JS_ASSERT(nslots > oldnslots);
    Value *beg = dslots + (oldnslots - JS_INITIAL_NSLOTS);
    Value *end = dslots + (nslots - JS_INITIAL_NSLOTS);
    SetValueRangeToUndefined(beg, end);

    return true;
}

void
JSObject::shrinkSlots(JSContext *cx, size_t nslots)
{
    /* Nothing to shrink? */
    if (!dslots)
        return;

    JS_ASSERT(dslots[-1].toPrivateUint32() > JS_INITIAL_NSLOTS);
    JS_ASSERT(nslots <= dslots[-1].toPrivateUint32());

    if (nslots <= JS_INITIAL_NSLOTS) {
        freeSlotsArray(cx);
        dslots = NULL;
    } else {
        size_t nwords = slotsToDynamicWords(nslots);
        Value *tmpdslots = (Value*) cx->realloc(dslots - 1, nwords * sizeof(Value));
        if (!tmpdslots)
            return;     /* leave dslots at its old size */

        dslots = tmpdslots;
        dslots++;
        dslots[-1].setPrivateUint32(nslots);
    }
}

bool
JSObject::ensureInstanceReservedSlots(JSContext *cx, size_t nreserved)
{
    JS_ASSERT_IF(isNative(),
                 isBlock() || isCall() || (isFunction() && isBoundFunction()));

    uintN nslots = JSSLOT_FREE(clasp) + nreserved;
    return nslots <= numSlots() || allocSlots(cx, nslots);
}

static JSObject *
js_InitNullClass(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(0);
    return NULL;
}

#define JS_PROTO(name,code,init) extern JSObject *init(JSContext *, JSObject *);
#include "jsproto.tbl"
#undef JS_PROTO

static JSObjectOp lazy_prototype_init[JSProto_LIMIT] = {
#define JS_PROTO(name,code,init) init,
#include "jsproto.tbl"
#undef JS_PROTO
};

namespace js {

bool
SetProto(JSContext *cx, JSObject *obj, JSObject *proto, bool checkForCycles)
{
    JS_ASSERT_IF(!checkForCycles, obj != proto);
    JS_ASSERT(obj->isExtensible());

    if (obj->isNative()) {
        JS_LOCK_OBJ(cx, obj);
        bool ok = obj->ensureClassReservedSlots(cx);
        JS_UNLOCK_OBJ(cx, obj);
        if (!ok)
            return false;
    }

    /*
     * Regenerate property cache shape ids for all of the scopes along the
     * old prototype chain to invalidate their property cache entries, in
     * case any entries were filled by looking up through obj.
     */
    JSObject *oldproto = obj;
    while (oldproto && oldproto->isNative()) {
        JS_LOCK_OBJ(cx, oldproto);
        oldproto->protoShapeChange(cx);
        JSObject *tmp = oldproto->getProto();
        JS_UNLOCK_OBJ(cx, oldproto);
        oldproto = tmp;
    }

    if (!proto || !checkForCycles) {
        obj->setProto(proto);
    } else if (!SetProtoCheckingForCycles(cx, obj, proto)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CYCLIC_VALUE, js_proto_str);
        return false;
    }
    return true;
}

}

JSBool
js_GetClassObject(JSContext *cx, JSObject *obj, JSProtoKey key,
                  JSObject **objp)
{
    JSObject *tmp, *cobj;
    JSResolvingKey rkey;
    JSResolvingEntry *rentry;
    uint32 generation;
    JSObjectOp init;
    Value v;

    while ((tmp = obj->getParent()) != NULL)
        obj = tmp;
    if (!(obj->getClass()->flags & JSCLASS_IS_GLOBAL)) {
        *objp = NULL;
        return JS_TRUE;
    }

    v = obj->getReservedSlot(key);
    if (v.isObject()) {
        *objp = &v.toObject();
        return JS_TRUE;
    }

    rkey.obj = obj;
    rkey.id = ATOM_TO_JSID(cx->runtime->atomState.classAtoms[key]);
    if (!js_StartResolving(cx, &rkey, JSRESFLAG_LOOKUP, &rentry))
        return JS_FALSE;
    if (!rentry) {
        /* Already caching key in obj -- suppress recursion. */
        *objp = NULL;
        return JS_TRUE;
    }
    generation = cx->resolvingTable->generation;

    JSBool ok = true;
    cobj = NULL;
    init = lazy_prototype_init[key];
    if (init) {
        if (!init(cx, obj)) {
            ok = JS_FALSE;
        } else {
            v = obj->getReservedSlot(key);
            if (v.isObject())
                cobj = &v.toObject();
        }
    }

    js_StopResolving(cx, &rkey, JSRESFLAG_LOOKUP, rentry, generation);
    *objp = cobj;
    return ok;
}

JSBool
js_SetClassObject(JSContext *cx, JSObject *obj, JSProtoKey key, JSObject *cobj, JSObject *proto)
{
    JS_ASSERT(!obj->getParent());
    if (!(obj->getClass()->flags & JSCLASS_IS_GLOBAL))
        return JS_TRUE;

    return js_SetReservedSlot(cx, obj, key, ObjectOrNullValue(cobj)) &&
           js_SetReservedSlot(cx, obj, JSProto_LIMIT + key, ObjectOrNullValue(proto));
}

JSBool
js_FindClassObject(JSContext *cx, JSObject *start, JSProtoKey protoKey,
                   Value *vp, Class *clasp)
{
    JSStackFrame *fp;
    JSObject *obj, *cobj, *pobj;
    jsid id;
    JSProperty *prop;
    const Shape *shape;

    /*
     * Find the global object. Use cx->fp() directly to avoid falling off
     * trace; all JIT-elided stack frames have the same global object as
     * cx->fp().
     */
    VOUCH_DOES_NOT_REQUIRE_STACK();
    if (!start && (fp = cx->maybefp()) != NULL)
        start = &fp->scopeChain();

    if (start) {
        /* Find the topmost object in the scope chain. */
        do {
            obj = start;
            start = obj->getParent();
        } while (start);
    } else {
        obj = cx->globalObject;
        if (!obj) {
            vp->setUndefined();
            return JS_TRUE;
        }
    }

    OBJ_TO_INNER_OBJECT(cx, obj);
    if (!obj)
        return JS_FALSE;

    if (protoKey != JSProto_Null) {
        JS_ASSERT(JSProto_Null < protoKey);
        JS_ASSERT(protoKey < JSProto_LIMIT);
        if (!js_GetClassObject(cx, obj, protoKey, &cobj))
            return JS_FALSE;
        if (cobj) {
            vp->setObject(*cobj);
            return JS_TRUE;
        }
        id = ATOM_TO_JSID(cx->runtime->atomState.classAtoms[protoKey]);
    } else {
        JSAtom *atom = js_Atomize(cx, clasp->name, strlen(clasp->name), 0);
        if (!atom)
            return false;
        id = ATOM_TO_JSID(atom);
    }

    JS_ASSERT(obj->isNative());
    if (js_LookupPropertyWithFlags(cx, obj, id, JSRESOLVE_CLASSNAME,
                                   &pobj, &prop) < 0) {
        return JS_FALSE;
    }
    Value v = UndefinedValue();
    if (prop && pobj->isNative()) {
        shape = (Shape *) prop;
        if (pobj->containsSlot(shape->slot)) {
            v = pobj->lockedGetSlot(shape->slot);
            if (v.isPrimitive())
                v.setUndefined();
        }
        JS_UNLOCK_OBJ(cx, pobj);
    }
    *vp = v;
    return JS_TRUE;
}

JSObject *
js_ConstructObject(JSContext *cx, Class *clasp, JSObject *proto, JSObject *parent,
                   uintN argc, Value *argv)
{
    AutoArrayRooter argtvr(cx, argc, argv);

    JSProtoKey protoKey = GetClassProtoKey(clasp);

    /* Protect constructor in case a crazy getter for .prototype uproots it. */
    AutoValueRooter tvr(cx);
    if (!js_FindClassObject(cx, parent, protoKey, tvr.addr(), clasp))
        return NULL;

    const Value &cval = tvr.value();
    if (tvr.value().isPrimitive()) {
        js_ReportIsNotFunction(cx, tvr.addr(), JSV2F_CONSTRUCT | JSV2F_SEARCH_STACK);
        return NULL;
    }

    /*
     * If proto is NULL, set it to Constructor.prototype, just like JSOP_NEW
     * does, likewise for the new object's parent.
     */
    JSObject *ctor = &cval.toObject();
    if (!parent)
        parent = ctor->getParent();
    if (!proto) {
        Value rval;
        if (!ctor->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom),
                               &rval)) {
            return NULL;
        }
        if (rval.isObjectOrNull())
            proto = rval.toObjectOrNull();
    }

    JSObject *obj = NewObject<WithProto::Class>(cx, clasp, proto, parent);
    if (!obj)
        return NULL;

    Value rval;
    if (!InvokeConstructorWithGivenThis(cx, obj, cval, argc, argv, &rval))
        return NULL;

    if (rval.isPrimitive())
        return obj;

    /*
     * If the instance's class differs from what was requested, throw a type
     * error.  If the given class has both the JSCLASS_HAS_PRIVATE and the
     * JSCLASS_CONSTRUCT_PROTOTYPE flags, and the instance does not have its
     * private data set at this point, then the constructor was replaced and
     * we should throw a type error.
     */
    obj = &rval.toObject();
    if (obj->getClass() != clasp ||
        (!(~clasp->flags & (JSCLASS_HAS_PRIVATE |
                            JSCLASS_CONSTRUCT_PROTOTYPE)) &&
         !obj->getPrivate())) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_WRONG_CONSTRUCTOR, clasp->name);
        return NULL;
    }
    return obj;
}

bool
JSObject::allocSlot(JSContext *cx, uint32 *slotp)
{
    uint32 slot = slotSpan();
    JS_ASSERT(slot >= JSSLOT_FREE(clasp));

    /*
     * If this object is in dictionary mode and it has a property table, try to
     * pull a free slot from the property table's slot-number freelist.
     */
    if (inDictionaryMode() && lastProp->table) {
        uint32 &last = lastProp->table->freelist;
        if (last != SHAPE_INVALID_SLOT) {
#ifdef DEBUG
            JS_ASSERT(last < slot);
            uint32 next = getSlot(last).toPrivateUint32();
            JS_ASSERT_IF(next != SHAPE_INVALID_SLOT, next < slot);
#endif

            *slotp = last;

            Value &vref = getSlotRef(last);
            last = vref.toPrivateUint32();
            vref.setUndefined();
            return true;
        }
    }

    if (slot >= numSlots() && !growSlots(cx, slot + 1))
        return false;

    /* JSObject::growSlots or JSObject::freeSlot should set the free slots to void. */
    JS_ASSERT(getSlot(slot).isUndefined());
    *slotp = slot;
    return true;
}

void
JSObject::freeSlot(JSContext *cx, uint32 slot)
{
    uint32 limit = slotSpan();
    JS_ASSERT(slot < limit);

    Value &vref = getSlotRef(slot);
    if (inDictionaryMode() && lastProp->table) {
        uint32 &last = lastProp->table->freelist;

        /* Can't afford to check the whole freelist, but let's check the head. */
        JS_ASSERT_IF(last != SHAPE_INVALID_SLOT, last < limit && last != slot);

        /*
         * Freeing a slot other than the last one mapped by this object's
         * shape (and not a reserved slot; see bug 595230): push the slot onto
         * the dictionary property table's freelist. We want to let the last
         * slot be freed by shrinking the dslots vector; see js_TraceObject.
         */
        if (JSSLOT_FREE(clasp) <= slot && slot + 1 < limit) {
            JS_ASSERT_IF(last != SHAPE_INVALID_SLOT, last < slotSpan());
            vref.setPrivateUint32(last);
            last = slot;
            return;
        }
    }
    vref.setUndefined();
}

/* JSBOXEDWORD_INT_MAX as a string */
#define JSBOXEDWORD_INT_MAX_STRING "1073741823"

/*
 * Convert string indexes that convert to int jsvals as ints to save memory.
 * Care must be taken to use this macro every time a property name is used, or
 * else double-sets, incorrect property cache misses, or other mistakes could
 * occur.
 */
jsid
js_CheckForStringIndex(jsid id)
{
    if (!JSID_IS_ATOM(id))
        return id;

    JSAtom *atom = JSID_TO_ATOM(id);
    JSString *str = ATOM_TO_STRING(atom);
    const jschar *s = str->flatChars();
    jschar ch = *s;

    JSBool negative = (ch == '-');
    if (negative)
        ch = *++s;

    if (!JS7_ISDEC(ch))
        return id;

    size_t n = str->flatLength() - negative;
    if (n > sizeof(JSBOXEDWORD_INT_MAX_STRING) - 1)
        return id;

    const jschar *cp = s;
    const jschar *end = s + n;

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

    /*
     * Non-integer indexes can't be represented as integers.  Also, distinguish
     * index "-0" from "0", because JSBOXEDWORD_INT cannot.
     */
    if (cp != end || (negative && index == 0))
        return id;

    if (negative) {
        if (oldIndex < -(JSID_INT_MIN / 10) ||
            (oldIndex == -(JSID_INT_MIN / 10) && c <= (-JSID_INT_MIN % 10)))
        {
            id = INT_TO_JSID(jsint(-index));
        }
    } else {
        if (oldIndex < JSID_INT_MAX / 10 ||
            (oldIndex == JSID_INT_MAX / 10 && c <= (JSID_INT_MAX % 10)))
        {
            id = INT_TO_JSID(jsint(index));
        }
    }

    return id;
}

static JSBool
PurgeProtoChain(JSContext *cx, JSObject *obj, jsid id)
{
    const Shape *shape;

    while (obj) {
        if (!obj->isNative()) {
            obj = obj->getProto();
            continue;
        }
        JS_LOCK_OBJ(cx, obj);
        shape = obj->nativeLookup(id);
        if (shape) {
            PCMETER(JS_PROPERTY_CACHE(cx).pcpurges++);
            obj->shadowingShapeChange(cx, *shape);
            JS_UNLOCK_OBJ(cx, obj);

            if (!obj->getParent()) {
                /*
                 * All scope chains end in a global object, so this will change
                 * the global shape. jstracer.cpp assumes that the global shape
                 * never changes on trace, so we must deep-bail here.
                 */
                LeaveTrace(cx);
            }
            return JS_TRUE;
        }
#ifdef JS_THREADSAFE
        JSObject *pobj = obj;
#endif
        obj = obj->getProto();
        JS_UNLOCK_OBJ(cx, pobj);
    }
    return JS_FALSE;
}

void
js_PurgeScopeChainHelper(JSContext *cx, JSObject *obj, jsid id)
{
    JS_ASSERT(obj->isDelegate());
    PurgeProtoChain(cx, obj->getProto(), id);

    /*
     * We must purge the scope chain only for Call objects as they are the only
     * kind of cacheable non-global object that can gain properties after outer
     * properties with the same names have been cached or traced. Call objects
     * may gain such properties via eval introducing new vars; see bug 490364.
     */
    if (obj->isCall()) {
        while ((obj = obj->getParent()) != NULL) {
            if (PurgeProtoChain(cx, obj, id))
                break;
        }
    }
}

const Shape *
js_AddNativeProperty(JSContext *cx, JSObject *obj, jsid id,
                     PropertyOp getter, PropertyOp setter, uint32 slot,
                     uintN attrs, uintN flags, intN shortid)
{
    const Shape *shape;

    JS_ASSERT(!(flags & Shape::METHOD));

    /*
     * Purge the property cache of now-shadowed id in obj's scope chain. Do
     * this optimistically (assuming no failure below) before locking obj, so
     * we can lock the shadowed scope.
     */
    js_PurgeScopeChain(cx, obj, id);

    JS_LOCK_OBJ(cx, obj);
    if (!obj->ensureClassReservedSlots(cx)) {
        shape = NULL;
    } else {
        /* Convert string indices to integers if appropriate. */
        id = js_CheckForStringIndex(id);
        shape = obj->putProperty(cx, id, getter, setter, slot, attrs, flags, shortid);
    }
    JS_UNLOCK_OBJ(cx, obj);
    return shape;
}

const Shape *
js_ChangeNativePropertyAttrs(JSContext *cx, JSObject *obj,
                             const Shape *shape, uintN attrs, uintN mask,
                             PropertyOp getter, PropertyOp setter)
{
    JS_LOCK_OBJ(cx, obj);
    shape = obj->ensureClassReservedSlots(cx)
            ? obj->changeProperty(cx, shape, attrs, mask, getter, setter)
            : NULL;
    JS_UNLOCK_OBJ(cx, obj);
    return shape;
}

JSBool
js_DefineProperty(JSContext *cx, JSObject *obj, jsid id, const Value *value,
                  PropertyOp getter, PropertyOp setter, uintN attrs)
{
    return js_DefineNativeProperty(cx, obj, id, *value, getter, setter, attrs,
                                   0, 0, NULL);
}

/*
 * Backward compatibility requires allowing addProperty hooks to mutate the
 * nominal initial value of a slotful property, while GC safety wants that
 * value to be stored before the call-out through the hook.  Optimize to do
 * both while saving cycles for classes that stub their addProperty hook.
 */
static inline bool
CallAddPropertyHook(JSContext *cx, Class *clasp, JSObject *obj, const Shape *shape, Value *vp)
{
    if (clasp->addProperty != PropertyStub) {
        Value nominal = *vp;

        if (!CallJSPropertyOp(cx, clasp->addProperty, obj, SHAPE_USERID(shape), vp))
            return false;
        if (*vp != nominal) {
            if (obj->containsSlot(shape->slot))
                obj->lockedSetSlot(shape->slot, *vp);
        }
    }
    return true;
}

JSBool
js_DefineNativeProperty(JSContext *cx, JSObject *obj, jsid id, const Value &value,
                        PropertyOp getter, PropertyOp setter, uintN attrs,
                        uintN flags, intN shortid, JSProperty **propp,
                        uintN defineHow /* = 0 */)
{
    Class *clasp;
    const Shape *shape;
    JSBool added;
    Value valueCopy;

    JS_ASSERT((defineHow & ~(JSDNP_CACHE_RESULT | JSDNP_DONT_PURGE | JSDNP_SET_METHOD)) == 0);
    LeaveTraceIfGlobalObject(cx, obj);

    /* Convert string indices to integers if appropriate. */
    id = js_CheckForStringIndex(id);

    /*
     * If defining a getter or setter, we must check for its counterpart and
     * update the attributes and property ops.  A getter or setter is really
     * only half of a property.
     */
    shape = NULL;
    if (attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
        JSObject *pobj;
        JSProperty *prop;

        /*
         * If JS_THREADSAFE and id is found, js_LookupProperty returns with
         * shape non-null and pobj locked.  If pobj == obj, the property is
         * already in obj and obj has its own (mutable) scope.  So if we are
         * defining a getter whose setter was already defined, or vice versa,
         * finish the job via obj->changeProperty, and refresh the property
         * cache line for (obj, id) to map shape.
         */
        if (!js_LookupProperty(cx, obj, id, &pobj, &prop))
            return JS_FALSE;
        shape = (Shape *) prop;
        if (shape && pobj == obj && shape->isAccessorDescriptor()) {
            shape = obj->changeProperty(cx, shape, attrs,
                                        JSPROP_GETTER | JSPROP_SETTER,
                                        (attrs & JSPROP_GETTER)
                                        ? getter
                                        : shape->getter(),
                                        (attrs & JSPROP_SETTER)
                                        ? setter
                                        : shape->setter());

            /* NB: obj == pobj, so we can share unlock code at the bottom. */
            if (!shape)
                goto error;
        } else if (prop) {
            pobj->dropProperty(cx, prop);
            prop = NULL;
            shape = NULL;
        }
    }

    /*
     * Purge the property cache of any properties named by id that are about
     * to be shadowed in obj's scope chain unless it is known a priori that it
     * is not possible. We do this before locking obj to avoid nesting locks.
     */
    if (!(defineHow & JSDNP_DONT_PURGE))
        js_PurgeScopeChain(cx, obj, id);

    /*
     * Check whether a readonly property or setter is being defined on a known
     * prototype object. See the comment in jscntxt.h before protoHazardShape's
     * member declaration.
     */
    if (obj->isDelegate() && (attrs & (JSPROP_READONLY | JSPROP_SETTER)))
        cx->runtime->protoHazardShape = js_GenerateShape(cx, false);

    /* Lock if object locking is required by this implementation. */
    JS_LOCK_OBJ(cx, obj);

    /* Use the object's class getter and setter by default. */
    clasp = obj->getClass();
    if (!(defineHow & JSDNP_SET_METHOD)) {
        if (!getter && !(attrs & JSPROP_GETTER))
            getter = clasp->getProperty;
        if (!setter && !(attrs & JSPROP_SETTER))
            setter = clasp->setProperty;
    }

    /* Get obj's own scope if it has one, or create a new one for obj. */
    if (!obj->ensureClassReservedSlots(cx))
        goto error;

    added = false;
    if (!shape) {
        /* Add a new property, or replace an existing one of the same id. */
        if (defineHow & JSDNP_SET_METHOD) {
            JS_ASSERT(clasp == &js_ObjectClass);
            JS_ASSERT(IsFunctionObject(value));
            JS_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));
            JS_ASSERT(!getter && !setter);

            JSObject *funobj = &value.toObject();
            if (FUN_OBJECT(GET_FUNCTION_PRIVATE(cx, funobj)) == funobj) {
                flags |= Shape::METHOD;
                getter = CastAsPropertyOp(funobj);
            }
        }

        added = !obj->nativeContains(id);
        uint32 oldShape = obj->shape();
        shape = obj->putProperty(cx, id, getter, setter, SHAPE_INVALID_SLOT,
                                 attrs, flags, shortid);
        if (!shape)
            goto error;

        /*
         * If shape is a method, the above call to putProperty suffices to
         * update the shape if necessary. But if scope->branded(), the shape
         * may not have changed and we may be overwriting a function-valued
         * property. See bug 560998.
         */
        if (obj->shape() == oldShape && obj->branded() && shape->slot != SHAPE_INVALID_SLOT)
            obj->methodWriteBarrier(cx, shape->slot, value);
    }

    /* Store value before calling addProperty, in case the latter GC's. */
    if (obj->containsSlot(shape->slot))
        obj->lockedSetSlot(shape->slot, value);

    /* XXXbe called with lock held */
    valueCopy = value;
    if (!CallAddPropertyHook(cx, clasp, obj, shape, &valueCopy)) {
        obj->removeProperty(cx, id);
        goto error;
    }

    if (defineHow & JSDNP_CACHE_RESULT) {
#ifdef JS_TRACER
        JS_ASSERT_NOT_ON_TRACE(cx);
        PropertyCacheEntry *entry =
#endif
            JS_PROPERTY_CACHE(cx).fill(cx, obj, 0, 0, obj, shape, added);
        TRACE_2(SetPropHit, entry, shape);
    }
    if (propp)
        *propp = (JSProperty *) shape;
    else
        JS_UNLOCK_OBJ(cx, obj);
    return JS_TRUE;

error: // TRACE_2 jumps here on error, as well.
    JS_UNLOCK_OBJ(cx, obj);
    return JS_FALSE;
}

JS_FRIEND_API(JSBool)
js_LookupProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                  JSProperty **propp)
{
    return js_LookupPropertyWithFlags(cx, obj, id, cx->resolveFlags,
                                      objp, propp) >= 0;
}

#define SCOPE_DEPTH_ACCUM(bs,val)                                             \
    JS_SCOPE_DEPTH_METERING(JS_BASIC_STATS_ACCUM(bs, val))

/*
 * Call obj's resolve hook. obj is a native object and the caller holds its
 * scope lock.
 *
 * cx, start, id, and flags are the parameters initially passed to the ongoing
 * lookup; objp and propp are its out parameters. obj is an object along
 * start's prototype chain.
 *
 * There are four possible outcomes:
 *
 *   - On failure, report an error or exception, unlock obj, and return false.
 *
 *   - If we are alrady resolving a property of *curobjp, set *recursedp = true,
 *     unlock obj, and return true.
 *
 *   - If the resolve hook finds or defines the sought property, set *objp and
 *     *propp appropriately, set *recursedp = false, and return true with *objp's
 *     lock held.
 *
 *   - Otherwise no property was resolved. Set *propp = NULL and *recursedp = false
 *     and return true.
 */
static JSBool
CallResolveOp(JSContext *cx, JSObject *start, JSObject *obj, jsid id, uintN flags,
              JSObject **objp, JSProperty **propp, bool *recursedp)
{
    Class *clasp = obj->getClass();
    JSResolveOp resolve = clasp->resolve;

    /*
     * Avoid recursion on (obj, id) already being resolved on cx.
     *
     * Once we have successfully added an entry for (obj, key) to
     * cx->resolvingTable, control must go through cleanup: before
     * returning.  But note that JS_DHASH_ADD may find an existing
     * entry, in which case we bail to suppress runaway recursion.
     */
    JSResolvingKey key = {obj, id};
    JSResolvingEntry *entry;
    if (!js_StartResolving(cx, &key, JSRESFLAG_LOOKUP, &entry)) {
        JS_UNLOCK_OBJ(cx, obj);
        return false;
    }
    if (!entry) {
        /* Already resolving id in obj -- suppress recursion. */
        JS_UNLOCK_OBJ(cx, obj);
        *recursedp = true;
        return true;
    }
    uint32 generation = cx->resolvingTable->generation;
    *recursedp = false;

    *propp = NULL;

    JSBool ok;
    const Shape *shape = NULL;
    if (clasp->flags & JSCLASS_NEW_RESOLVE) {
        JSNewResolveOp newresolve = (JSNewResolveOp)resolve;
        if (flags == JSRESOLVE_INFER)
            flags = js_InferFlags(cx, 0);
        JSObject *obj2 = (clasp->flags & JSCLASS_NEW_RESOLVE_GETS_START) ? start : NULL;
        JS_UNLOCK_OBJ(cx, obj);

        {
            /* Protect id and all atoms from a GC nested in resolve. */
            AutoKeepAtoms keep(cx->runtime);
            ok = newresolve(cx, obj, id, flags, &obj2);
        }
        if (!ok)
            goto cleanup;

        JS_LOCK_OBJ(cx, obj);
        if (obj2) {
            /* Resolved: juggle locks and lookup id again. */
            if (obj2 != obj) {
                JS_UNLOCK_OBJ(cx, obj);
                if (obj2->isNative())
                    JS_LOCK_OBJ(cx, obj2);
            }
            if (!obj2->isNative()) {
                /* Whoops, newresolve handed back a foreign obj2. */
                JS_ASSERT(obj2 != obj);
                ok = obj2->lookupProperty(cx, id, objp, propp);
                if (!ok || *propp)
                    goto cleanup;
                JS_LOCK_OBJ(cx, obj2);
            } else {
                /*
                 * Require that obj2 not be empty now, as we do for old-style
                 * resolve.  If it doesn't, then id was not truly resolved, and
                 * we'll find it in the proto chain, or miss it if obj2's proto
                 * is not on obj's proto chain.  That last case is a "too bad!"
                 * case.
                 */
                if (!obj2->nativeEmpty())
                    shape = obj2->nativeLookup(id);
            }
            if (shape) {
                JS_ASSERT(!obj2->nativeEmpty());
                obj = obj2;
            } else if (obj2 != obj) {
                if (obj2->isNative())
                    JS_UNLOCK_OBJ(cx, obj2);
                JS_LOCK_OBJ(cx, obj);
            }
        }
    } else {
        /*
         * Old resolve always requires id re-lookup if obj is not empty after
         * resolve returns.
         */
        JS_UNLOCK_OBJ(cx, obj);
        ok = resolve(cx, obj, id);
        if (!ok)
            goto cleanup;
        JS_LOCK_OBJ(cx, obj);
        JS_ASSERT(obj->isNative());
        if (!obj->nativeEmpty())
            shape = obj->nativeLookup(id);
    }

cleanup:
    if (ok && shape) {
        *objp = obj;
        *propp = (JSProperty *) shape;
    }
    js_StopResolving(cx, &key, JSRESFLAG_LOOKUP, entry, generation);
    return ok;
}

static JS_ALWAYS_INLINE int
js_LookupPropertyWithFlagsInline(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                                 JSObject **objp, JSProperty **propp)
{
    /* Convert string indices to integers if appropriate. */
    id = js_CheckForStringIndex(id);

    /* Search scopes starting with obj and following the prototype link. */
    JSObject *start = obj;
    int protoIndex;
    for (protoIndex = 0; ; protoIndex++) {
        JS_LOCK_OBJ(cx, obj);
        const Shape *shape = obj->nativeLookup(id);
        if (shape) {
            SCOPE_DEPTH_ACCUM(&cx->runtime->protoLookupDepthStats, protoIndex);
            *objp = obj;
            *propp = (JSProperty *) shape;
            return protoIndex;
        }

        /* Try obj's class resolve hook if id was not found in obj's scope. */
        if (!shape && obj->getClass()->resolve != JS_ResolveStub) {
            bool recursed;
            if (!CallResolveOp(cx, start, obj, id, flags, objp, propp, &recursed))
                return -1;
            if (recursed)
                break;
            if (*propp) {
                /* Recalculate protoIndex in case it was resolved on some other object. */
                protoIndex = 0;
                for (JSObject *proto = start; proto && proto != *objp; proto = proto->getProto())
                    protoIndex++;
                SCOPE_DEPTH_ACCUM(&cx->runtime->protoLookupDepthStats, protoIndex);
                return protoIndex;
            }
        }

        JSObject *proto = obj->getProto();
        JS_UNLOCK_OBJ(cx, obj);
        if (!proto)
            break;
        if (!proto->isNative()) {
            if (!proto->lookupProperty(cx, id, objp, propp))
                return -1;
            return protoIndex + 1;
        }

        obj = proto;
    }

    *objp = NULL;
    *propp = NULL;
    return protoIndex;
}

int
js_LookupPropertyWithFlags(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                           JSObject **objp, JSProperty **propp)
{
    return js_LookupPropertyWithFlagsInline(cx, obj, id, flags, objp, propp);
}

PropertyCacheEntry *
js_FindPropertyHelper(JSContext *cx, jsid id, JSBool cacheResult,
                      JSObject **objp, JSObject **pobjp, JSProperty **propp)
{
    JSObject *scopeChain, *obj, *parent, *pobj;
    PropertyCacheEntry *entry;
    int scopeIndex, protoIndex;
    JSProperty *prop;

    JS_ASSERT_IF(cacheResult, !JS_ON_TRACE(cx));
    scopeChain = &js_GetTopStackFrame(cx)->scopeChain();

    /* Scan entries on the scope chain that we can cache across. */
    entry = JS_NO_PROP_CACHE_FILL;
    obj = scopeChain;
    parent = obj->getParent();
    for (scopeIndex = 0;
         parent
         ? js_IsCacheableNonGlobalScope(obj)
         : !obj->getOps()->lookupProperty;
         ++scopeIndex) {
        protoIndex =
            js_LookupPropertyWithFlags(cx, obj, id, cx->resolveFlags,
                                       &pobj, &prop);
        if (protoIndex < 0)
            return NULL;

        if (prop) {
#ifdef DEBUG
            if (parent) {
                Class *clasp = obj->getClass();
                JS_ASSERT(pobj->isNative());
                JS_ASSERT(pobj->getClass() == clasp);
                if (clasp == &js_BlockClass) {
                    /*
                     * A block instance on the scope chain is immutable and it
                     * shares its shapes with its compile-time prototype.
                     */
                    JS_ASSERT(pobj == obj);
                    JS_ASSERT(pobj->isClonedBlock());
                    JS_ASSERT(protoIndex == 0);
                } else {
                    /* Call and DeclEnvClass objects have no prototypes. */
                    JS_ASSERT(!obj->getProto());
                    JS_ASSERT(protoIndex == 0);
                }
            } else {
                JS_ASSERT(obj->isNative());
            }
#endif
            /*
             * We must check if pobj is native as a global object can have
             * non-native prototype.
             */
            if (cacheResult && pobj->isNative()) {
                entry = JS_PROPERTY_CACHE(cx).fill(cx, scopeChain, scopeIndex,
                                                   protoIndex, pobj,
                                                   (Shape *) prop);
            }
            SCOPE_DEPTH_ACCUM(&cx->runtime->scopeSearchDepthStats, scopeIndex);
            goto out;
        }

        if (!parent) {
            pobj = NULL;
            goto out;
        }
        obj = parent;
        parent = obj->getParent();
    }

    for (;;) {
        if (!obj->lookupProperty(cx, id, &pobj, &prop))
            return NULL;
        if (prop) {
            PCMETER(JS_PROPERTY_CACHE(cx).nofills++);
            goto out;
        }

        /*
         * We conservatively assume that a resolve hook could mutate the scope
         * chain during JSObject::lookupProperty. So we read parent here again.
         */
        parent = obj->getParent();
        if (!parent) {
            pobj = NULL;
            break;
        }
        obj = parent;
    }

  out:
    JS_ASSERT(!!pobj == !!prop);
    *objp = obj;
    *pobjp = pobj;
    *propp = prop;
    return entry;
}

JS_FRIEND_API(JSBool)
js_FindProperty(JSContext *cx, jsid id, JSObject **objp, JSObject **pobjp,
                JSProperty **propp)
{
    return !!js_FindPropertyHelper(cx, id, false, objp, pobjp, propp);
}

JSObject *
js_FindIdentifierBase(JSContext *cx, JSObject *scopeChain, jsid id)
{
    /*
     * This function should not be called for a global object or from the
     * trace and should have a valid cache entry for native scopeChain.
     */
    JS_ASSERT(scopeChain->getParent());
    JS_ASSERT(!JS_ON_TRACE(cx));

    JSObject *obj = scopeChain;

    /*
     * Loop over cacheable objects on the scope chain until we find a
     * property. We also stop when we reach the global object skipping any
     * farther checks or lookups. For details see the JSOP_BINDNAME case of
     * js_Interpret.
     *
     * The test order here matters because js_IsCacheableNonGlobalScope
     * must not be passed a global object (i.e. one with null parent).
     */
    for (int scopeIndex = 0;
         !obj->getParent() || js_IsCacheableNonGlobalScope(obj);
         scopeIndex++) {
        JSObject *pobj;
        JSProperty *prop;
        int protoIndex = js_LookupPropertyWithFlags(cx, obj, id,
                                                    cx->resolveFlags,
                                                    &pobj, &prop);
        if (protoIndex < 0)
            return NULL;
        if (prop) {
            if (!pobj->isNative()) {
                JS_ASSERT(!obj->getParent());
                return obj;
            }
            JS_ASSERT_IF(obj->getParent(), pobj->getClass() == obj->getClass());
#ifdef DEBUG
            PropertyCacheEntry *entry =
#endif
                JS_PROPERTY_CACHE(cx).fill(cx, scopeChain, scopeIndex, protoIndex, pobj,
                                           (Shape *) prop);
            JS_ASSERT(entry);
            JS_UNLOCK_OBJ(cx, pobj);
            return obj;
        }

        JSObject *parent = obj->getParent();
        if (!parent)
            return obj;
        obj = parent;
    }

    /* Loop until we find a property or reach the global object. */
    do {
        JSObject *pobj;
        JSProperty *prop;
        if (!obj->lookupProperty(cx, id, &pobj, &prop))
            return NULL;
        if (prop) {
            pobj->dropProperty(cx, prop);
            break;
        }

        /*
         * We conservatively assume that a resolve hook could mutate the scope
         * chain during JSObject::lookupProperty. So we must check if parent is
         * not null here even if it wasn't before the lookup.
         */
        JSObject *parent = obj->getParent();
        if (!parent)
            break;
        obj = parent;
    } while (obj->getParent());
    return obj;
}

static JS_ALWAYS_INLINE JSBool
js_NativeGetInline(JSContext *cx, JSObject *obj, JSObject *pobj, const Shape *shape, uintN getHow,
                   Value *vp)
{
    LeaveTraceIfGlobalObject(cx, pobj);

    uint32 slot;
    int32 sample;

    JS_ASSERT(pobj->isNative());
    JS_ASSERT(JS_IS_OBJ_LOCKED(cx, pobj));

    slot = shape->slot;
    if (slot != SHAPE_INVALID_SLOT)
        *vp = pobj->lockedGetSlot(slot);
    else
        vp->setUndefined();
    if (shape->hasDefaultGetter())
        return true;

    if (JS_UNLIKELY(shape->isMethod()) && (getHow & JSGET_NO_METHOD_BARRIER)) {
        JS_ASSERT(&shape->methodObject() == &vp->toObject());
        return true;
    }

    sample = cx->runtime->propertyRemovals;
    JS_UNLOCK_OBJ(cx, pobj);
    {
        AutoShapeRooter tvr(cx, shape);
        AutoObjectRooter tvr2(cx, pobj);
        if (!shape->get(cx, obj, pobj, vp))
            return false;
    }
    JS_LOCK_OBJ(cx, pobj);

    if (pobj->containsSlot(slot) &&
        (JS_LIKELY(cx->runtime->propertyRemovals == sample) ||
         pobj->nativeContains(*shape))) {
        if (!pobj->methodWriteBarrier(cx, *shape, *vp)) {
            JS_UNLOCK_OBJ(cx, pobj);
            return false;
        }
        pobj->lockedSetSlot(slot, *vp);
    }

    return true;
}

JSBool
js_NativeGet(JSContext *cx, JSObject *obj, JSObject *pobj, const Shape *shape, uintN getHow,
             Value *vp)
{
    return js_NativeGetInline(cx, obj, pobj, shape, getHow, vp);
}

JSBool
js_NativeSet(JSContext *cx, JSObject *obj, const Shape *shape, bool added, Value *vp)
{
    LeaveTraceIfGlobalObject(cx, obj);

    uint32 slot;
    int32 sample;

    JS_ASSERT(obj->isNative());
    JS_ASSERT(JS_IS_OBJ_LOCKED(cx, obj));

    slot = shape->slot;
    if (slot != SHAPE_INVALID_SLOT) {
        OBJ_CHECK_SLOT(obj, slot);

        /* If shape has a stub setter, keep obj locked and just store *vp. */
        if (shape->hasDefaultSetter()) {
            if (!added && !obj->methodWriteBarrier(cx, *shape, *vp)) {
                JS_UNLOCK_OBJ(cx, obj);
                return false;
            }
            obj->lockedSetSlot(slot, *vp);
            return true;
        }
    } else {
        /*
         * Allow API consumers to create shared properties with stub setters.
         * Such properties effectively function as data descriptors which are
         * not writable, so attempting to set such a property should do nothing
         * or throw if we're in strict mode.
         */
        if (!shape->hasGetterValue() && shape->hasDefaultSetter())
            return js_ReportGetterOnlyAssignment(cx);
    }

    sample = cx->runtime->propertyRemovals;
    JS_UNLOCK_OBJ(cx, obj);
    {
        AutoShapeRooter tvr(cx, shape);
        if (!shape->set(cx, obj, vp))
            return false;
    }

    JS_LOCK_OBJ(cx, obj);
    if (obj->containsSlot(slot) &&
        (JS_LIKELY(cx->runtime->propertyRemovals == sample) ||
         obj->nativeContains(*shape))) {
        if (!added && !obj->methodWriteBarrier(cx, *shape, *vp)) {
            JS_UNLOCK_OBJ(cx, obj);
            return false;
        }
        obj->lockedSetSlot(slot, *vp);
    }

    return true;
}

static JS_ALWAYS_INLINE bool
js_GetPropertyHelperWithShapeInline(JSContext *cx, JSObject *obj, jsid id,
                                    uintN getHow, Value *vp,
                                    const Shape **shapeOut, JSObject **holderOut)
{
    JSObject *aobj, *obj2;
    int protoIndex;
    JSProperty *prop;
    const Shape *shape;

    JS_ASSERT_IF(getHow & JSGET_CACHE_RESULT, !JS_ON_TRACE(cx));

    *shapeOut = NULL;

    /* Convert string indices to integers if appropriate. */
    id = js_CheckForStringIndex(id);

    aobj = js_GetProtoIfDenseArray(obj);
    /* This call site is hot -- use the always-inlined variant of js_LookupPropertyWithFlags(). */
    protoIndex = js_LookupPropertyWithFlagsInline(cx, aobj, id, cx->resolveFlags,
                                                  &obj2, &prop);
    if (protoIndex < 0)
        return JS_FALSE;

    *holderOut = obj2;

    if (!prop) {
        vp->setUndefined();

        if (!CallJSPropertyOp(cx, obj->getClass()->getProperty, obj, id, vp))
            return JS_FALSE;

        PCMETER(getHow & JSGET_CACHE_RESULT && JS_PROPERTY_CACHE(cx).nofills++);

        /*
         * Give a strict warning if foo.bar is evaluated by a script for an
         * object foo with no property named 'bar'.
         */
        jsbytecode *pc;
        if (vp->isUndefined() && ((pc = js_GetCurrentBytecodePC(cx)) != NULL)) {
            JSOp op;
            uintN flags;

            op = (JSOp) *pc;
            if (op == JSOP_TRAP) {
                JS_ASSERT_NOT_ON_TRACE(cx);
                op = JS_GetTrapOpcode(cx, cx->fp()->script(), pc);
            }
            if (op == JSOP_GETXPROP) {
                flags = JSREPORT_ERROR;
            } else {
                if (!JS_HAS_STRICT_OPTION(cx) ||
                    (op != JSOP_GETPROP && op != JSOP_GETELEM) ||
                    js_CurrentPCIsInImacro(cx)) {
                    return JS_TRUE;
                }

                /*
                 * XXX do not warn about missing __iterator__ as the function
                 * may be called from JS_GetMethodById. See bug 355145.
                 */
                if (JSID_IS_ATOM(id, cx->runtime->atomState.iteratorAtom))
                    return JS_TRUE;

                /* Do not warn about tests like (obj[prop] == undefined). */
                if (cx->resolveFlags == JSRESOLVE_INFER) {
                    LeaveTrace(cx);
                    pc += js_CodeSpec[op].length;
                    if (Detecting(cx, pc))
                        return JS_TRUE;
                } else if (cx->resolveFlags & JSRESOLVE_DETECTING) {
                    return JS_TRUE;
                }

                flags = JSREPORT_WARNING | JSREPORT_STRICT;
            }

            /* Ok, bad undefined property reference: whine about it. */
            if (!js_ReportValueErrorFlags(cx, flags, JSMSG_UNDEFINED_PROP,
                                          JSDVG_IGNORE_STACK, IdToValue(id),
                                          NULL, NULL, NULL)) {
                return JS_FALSE;
            }
        }
        return JS_TRUE;
    }

    if (!obj2->isNative())
        return obj2->getProperty(cx, id, vp);

    shape = (Shape *) prop;
    *shapeOut = shape;

    if (getHow & JSGET_CACHE_RESULT) {
        JS_ASSERT_NOT_ON_TRACE(cx);
        JS_PROPERTY_CACHE(cx).fill(cx, aobj, 0, protoIndex, obj2, shape);
    }

    /* This call site is hot -- use the always-inlined variant of js_NativeGet(). */
    if (!js_NativeGetInline(cx, obj, obj2, shape, getHow, vp))
        return JS_FALSE;

    JS_UNLOCK_OBJ(cx, obj2);
    return JS_TRUE;
}

extern bool
js_GetPropertyHelperWithShape(JSContext *cx, JSObject *obj, jsid id,
                              uint32 getHow, Value *vp,
                              const Shape **shapeOut, JSObject **holderOut)
{
    return js_GetPropertyHelperWithShapeInline(cx, obj, id, getHow, vp, shapeOut, holderOut);
}

static JS_ALWAYS_INLINE JSBool
js_GetPropertyHelperInline(JSContext *cx, JSObject *obj, jsid id, uint32 getHow, Value *vp)
{
    const Shape *shape;
    JSObject *holder;
    return js_GetPropertyHelperWithShapeInline(cx, obj, id, getHow, vp, &shape, &holder);
}

extern JSBool
js_GetPropertyHelper(JSContext *cx, JSObject *obj, jsid id, uint32 getHow, Value *vp)
{
    return js_GetPropertyHelperInline(cx, obj, id, getHow, vp);
}

JSBool
js_GetProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    /* This call site is hot -- use the always-inlined variant of js_GetPropertyHelper(). */
    return js_GetPropertyHelperInline(cx, obj, id, JSGET_METHOD_BARRIER, vp);
}

JSBool
js_GetMethod(JSContext *cx, JSObject *obj, jsid id, uintN getHow, Value *vp)
{
    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED);

    PropertyIdOp op = obj->getOps()->getProperty;
    if (!op) {
#if JS_HAS_XML_SUPPORT
        JS_ASSERT(!obj->isXML());
#endif
        return js_GetPropertyHelper(cx, obj, id, getHow, vp);
    }
    JS_ASSERT_IF(getHow & JSGET_CACHE_RESULT, obj->isDenseArray());
#if JS_HAS_XML_SUPPORT
    if (obj->isXML())
        return js_GetXMLMethod(cx, obj, id, vp);
#endif
    return op(cx, obj, id, vp);
}

JS_FRIEND_API(bool)
js_CheckUndeclaredVarAssignment(JSContext *cx, JSString *propname)
{
    JSStackFrame *const fp = js_GetTopStackFrame(cx);
    if (!fp)
        return true;

    /* If neither cx nor the code is strict, then no check is needed. */
    if (!(fp->isScriptFrame() && fp->script()->strictModeCode) &&
        !JS_HAS_STRICT_OPTION(cx)) {
        return true;
    }

    const char *bytes = js_GetStringBytes(cx, propname);
    return bytes &&
           JS_ReportErrorFlagsAndNumber(cx,
                                        (JSREPORT_WARNING | JSREPORT_STRICT
                                         | JSREPORT_STRICT_MODE_ERROR),
                                        js_GetErrorMessage, NULL,
                                        JSMSG_UNDECLARED_VAR, bytes);
}

bool
JSObject::reportReadOnly(JSContext* cx, jsid id, uintN report)
{
    return js_ReportValueErrorFlags(cx, report, JSMSG_READ_ONLY,
                                    JSDVG_IGNORE_STACK, IdToValue(id), NULL,
                                    NULL, NULL);
}

bool
JSObject::reportNotConfigurable(JSContext* cx, jsid id, uintN report)
{
    return js_ReportValueErrorFlags(cx, report, JSMSG_CANT_DELETE,
                                    JSDVG_IGNORE_STACK, IdToValue(id), NULL,
                                    NULL, NULL);
}

bool
JSObject::reportNotExtensible(JSContext *cx, uintN report)
{
    return js_ReportValueErrorFlags(cx, report, JSMSG_OBJECT_NOT_EXTENSIBLE,
                                    JSDVG_IGNORE_STACK, ObjectValue(*this),
                                    NULL, NULL, NULL);
}

/*
 * Note: all non-error exits in this function must notify the tracer using
 * SetPropHit when called from the interpreter, which is detected by testing
 * (defineHow & JSDNP_CACHE_RESULT).
 */
JSBool
js_SetPropertyHelper(JSContext *cx, JSObject *obj, jsid id, uintN defineHow,
                     Value *vp, JSBool strict)
{
    int protoIndex;
    JSObject *pobj;
    JSProperty *prop;
    const Shape *shape;
    uintN attrs, flags;
    intN shortid;
    Class *clasp;
    PropertyOp getter, setter;
    bool added;

    JS_ASSERT((defineHow &
               ~(JSDNP_CACHE_RESULT | JSDNP_SET_METHOD | JSDNP_UNQUALIFIED)) == 0);
    if (defineHow & JSDNP_CACHE_RESULT)
        JS_ASSERT_NOT_ON_TRACE(cx);

    /* Convert string indices to integers if appropriate. */
    id = js_CheckForStringIndex(id);

    protoIndex = js_LookupPropertyWithFlags(cx, obj, id, cx->resolveFlags,
                                            &pobj, &prop);
    if (protoIndex < 0)
        return JS_FALSE;
    if (prop) {
        if (!pobj->isNative())
            prop = NULL;
    } else {
        /* We should never add properties to lexical blocks.  */
        JS_ASSERT(!obj->isBlock());

        if (!obj->getParent() &&
            (defineHow & JSDNP_UNQUALIFIED) &&
            !js_CheckUndeclaredVarAssignment(cx, JSID_TO_STRING(id))) {
            return JS_FALSE;
        }
    }
    shape = (Shape *) prop;

    /*
     * Now either shape is null, meaning id was not found in obj or one of its
     * prototypes; or shape is non-null, meaning id was found directly in pobj.
     * If JS_THREADSAFE and shape is non-null, then pobj is locked, and shape
     * is held: we must JSObject::dropProperty or else JS_UNLOCK_OBJ before we
     * return (the two are equivalent for native objects; we use JS_UNLOCK_OBJ
     * because it is cheaper).
     */
    attrs = JSPROP_ENUMERATE;
    flags = 0;
    shortid = 0;
    clasp = obj->getClass();
    getter = clasp->getProperty;
    setter = clasp->setProperty;

    if (shape) {
        /* ES5 8.12.4 [[Put]] step 2. */
        if (shape->isAccessorDescriptor()) {
            if (shape->hasDefaultSetter()) {
                JS_UNLOCK_OBJ(cx, pobj);
                if (defineHow & JSDNP_CACHE_RESULT)
                    TRACE_2(SetPropHit, JS_NO_PROP_CACHE_FILL, shape);
                return js_ReportGetterOnlyAssignment(cx);
            }
        } else {
            JS_ASSERT(shape->isDataDescriptor());

            if (!shape->writable()) {
                JS_UNLOCK_OBJ(cx, pobj);

                PCMETER((defineHow & JSDNP_CACHE_RESULT) && JS_PROPERTY_CACHE(cx).rofills++);
                if (defineHow & JSDNP_CACHE_RESULT) {
                    JS_ASSERT_NOT_ON_TRACE(cx);
                    TRACE_2(SetPropHit, JS_NO_PROP_CACHE_FILL, shape);
                }

                /* Error in strict mode code, warn with strict option, otherwise do nothing. */
                if (strict)
                    return obj->reportReadOnly(cx, id);
                if (JS_HAS_STRICT_OPTION(cx))
                    return obj->reportReadOnly(cx, id, JSREPORT_STRICT | JSREPORT_WARNING);
                return JS_TRUE;

#ifdef JS_TRACER
              error: // TRACE_2 jumps here in case of error.
                return JS_FALSE;
#endif
            }
        }

        attrs = shape->attributes();
        if (pobj != obj) {
            /*
             * We found id in a prototype object: prepare to share or shadow.
             *
             * NB: Thanks to the immutable, garbage-collected property tree
             * maintained by jsscope.c in cx->runtime, we needn't worry about
             * shape going away behind our back after we've unlocked pobj.
             */
            JS_UNLOCK_OBJ(cx, pobj);

            /* Don't clone a prototype property that doesn't have a slot. */
            if (!shape->hasSlot()) {
                if (defineHow & JSDNP_CACHE_RESULT) {
#ifdef JS_TRACER
                    JS_ASSERT_NOT_ON_TRACE(cx);
                    PropertyCacheEntry *entry =
#endif
                        JS_PROPERTY_CACHE(cx).fill(cx, obj, 0, protoIndex, pobj, shape);
                    TRACE_2(SetPropHit, entry, shape);
                }

                if (shape->hasDefaultSetter() && !shape->hasGetterValue())
                    return JS_TRUE;

                return shape->set(cx, obj, vp);
            }

            /* Restore attrs to the ECMA default for new properties. */
            attrs = JSPROP_ENUMERATE;

            /*
             * Preserve the shortid, getter, and setter when shadowing any
             * property that has a shortid.  An old API convention requires
             * that the property's getter and setter functions receive the
             * shortid, not id, when they are called on the shadow we are
             * about to create in obj.
             */
            if (shape->hasShortID()) {
                flags = Shape::HAS_SHORTID;
                shortid = shape->shortid;
                getter = shape->getter();
                setter = shape->setter();
            }

            /*
             * Forget we found the proto-property now that we've copied any
             * needed member values.
             */
            shape = NULL;
        }

        if (shape) {
            if (shape->isMethod()) {
                JS_ASSERT(pobj->hasMethodBarrier());
            } else if ((defineHow & JSDNP_SET_METHOD) && obj->canHaveMethodBarrier()) {
                JS_ASSERT(IsFunctionObject(*vp));
                JS_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));

                JSObject *funobj = &vp->toObject();
                JSFunction *fun = GET_FUNCTION_PRIVATE(cx, funobj);
                if (fun == funobj) {
                    funobj = CloneFunctionObject(cx, fun, fun->parent);
                    if (!funobj)
                        return JS_FALSE;
                    vp->setObject(*funobj);
                }
            }
        }
    }

    added = false;
    if (!shape) {
        if (!obj->isExtensible()) {
            /* Error in strict mode code, warn with strict option, otherwise do nothing. */
            if (strict)
                return obj->reportNotExtensible(cx);
            if (JS_HAS_STRICT_OPTION(cx))
                return obj->reportNotExtensible(cx, JSREPORT_STRICT | JSREPORT_WARNING);
            return JS_TRUE;
        }

        /*
         * Purge the property cache of now-shadowed id in obj's scope chain.
         * Do this early, before locking obj to avoid nesting locks.
         */
        js_PurgeScopeChain(cx, obj, id);

        /* Find or make a property descriptor with the right heritage. */
        JS_LOCK_OBJ(cx, obj);
        if (!obj->ensureClassReservedSlots(cx)) {
            JS_UNLOCK_OBJ(cx, obj);
            return JS_FALSE;
        }

        /*
         * Check for Object class here to avoid defining a method on a class
         * with magic resolve, addProperty, getProperty, etc. hooks.
         */
        if ((defineHow & JSDNP_SET_METHOD) && obj->canHaveMethodBarrier()) {
            JS_ASSERT(IsFunctionObject(*vp));
            JS_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));

            JSObject *funobj = &vp->toObject();
            JSFunction *fun = GET_FUNCTION_PRIVATE(cx, funobj);
            if (fun == funobj) {
                flags |= Shape::METHOD;
                getter = CastAsPropertyOp(funobj);
            }
        }

        shape = obj->putProperty(cx, id, getter, setter, SHAPE_INVALID_SLOT,
                                 attrs, flags, shortid);
        if (!shape) {
            JS_UNLOCK_OBJ(cx, obj);
            return JS_FALSE;
        }

        /*
         * Initialize the new property value (passed to setter) to undefined.
         * Note that we store before calling addProperty, to match the order
         * in js_DefineNativeProperty.
         */
        if (obj->containsSlot(shape->slot))
            obj->lockedSetSlot(shape->slot, UndefinedValue());

        /* XXXbe called with obj locked */
        if (!CallAddPropertyHook(cx, clasp, obj, shape, vp)) {
            obj->removeProperty(cx, id);
            JS_UNLOCK_OBJ(cx, obj);
            return JS_FALSE;
        }
        added = true;
    }

    if (defineHow & JSDNP_CACHE_RESULT) {
#ifdef JS_TRACER
        JS_ASSERT_NOT_ON_TRACE(cx);
        PropertyCacheEntry *entry =
#endif
            JS_PROPERTY_CACHE(cx).fill(cx, obj, 0, 0, obj, shape, added);
        TRACE_2(SetPropHit, entry, shape);
    }

    if (!js_NativeSet(cx, obj, shape, added, vp))
        return NULL;

    JS_UNLOCK_OBJ(cx, obj);
    return JS_TRUE;
}

JSBool
js_SetProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp, JSBool strict)
{
    return js_SetPropertyHelper(cx, obj, id, 0, vp, strict);
}

JSBool
js_GetAttributes(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp)
{
    JSProperty *prop;
    if (!js_LookupProperty(cx, obj, id, &obj, &prop))
        return false;
    if (!prop) {
        *attrsp = 0;
        return true;
    }
    if (!obj->isNative())
        return obj->getAttributes(cx, id, attrsp);

    const Shape *shape = (Shape *)prop;
    *attrsp = shape->attributes();
    JS_UNLOCK_OBJ(cx, obj);
    return true;
}

JSBool
js_SetNativeAttributes(JSContext *cx, JSObject *obj, Shape *shape, uintN attrs)
{
    JS_ASSERT(obj->isNative());
    bool ok = !!js_ChangeNativePropertyAttrs(cx, obj, shape, attrs, 0,
                                             shape->getter(), shape->setter());
    JS_UNLOCK_OBJ(cx, obj);
    return ok;
}

JSBool
js_SetAttributes(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp)
{
    JSProperty *prop;
    if (!js_LookupProperty(cx, obj, id, &obj, &prop))
        return false;
    if (!prop)
        return true;
    return obj->isNative()
           ? js_SetNativeAttributes(cx, obj, (Shape *) prop, *attrsp)
           : obj->setAttributes(cx, id, attrsp);
}

JSBool
js_DeleteProperty(JSContext *cx, JSObject *obj, jsid id, Value *rval, JSBool strict)
{
    JSObject *proto;
    JSProperty *prop;
    const Shape *shape;
    JSBool ok;

    rval->setBoolean(true);

    /* Convert string indices to integers if appropriate. */
    id = js_CheckForStringIndex(id);

    if (!js_LookupProperty(cx, obj, id, &proto, &prop))
        return false;
    if (!prop || proto != obj) {
        /*
         * If the property was found in a native prototype, check whether it's
         * shared and permanent.  Such a property stands for direct properties
         * in all delegating objects, matching ECMA semantics without bloating
         * each delegating object.
         */
        if (prop && proto->isNative()) {
            shape = (Shape *)prop;
            if (shape->isSharedPermanent()) {
                JS_UNLOCK_OBJ(cx, proto);
                if (strict)
                    return obj->reportNotConfigurable(cx, id);
                rval->setBoolean(false);
                return true;
            }
            JS_UNLOCK_OBJ(cx, proto);
        }

        /*
         * If no property, or the property comes unshared or impermanent from
         * a prototype, call the class's delProperty hook, passing rval as the
         * result parameter.
         */
        return CallJSPropertyOp(cx, obj->getClass()->delProperty, obj, id, rval);
    }

    shape = (Shape *)prop;
    if (!shape->configurable()) {
        JS_UNLOCK_OBJ(cx, obj);
        if (strict)
            return obj->reportNotConfigurable(cx, id);
        rval->setBoolean(false);
        return true;
    }

    /* XXXbe called with obj locked */
    if (!CallJSPropertyOp(cx, obj->getClass()->delProperty, obj, SHAPE_USERID(shape), rval)) {
        JS_UNLOCK_OBJ(cx, obj);
        return false;
    }

    if (obj->containsSlot(shape->slot)) {
        const Value &v = obj->lockedGetSlot(shape->slot);
        GC_POKE(cx, v);

        /*
         * Delete is rare enough that we can take the hit of checking for an
         * active cloned method function object that must be homed to a callee
         * slot on the active stack frame before this delete completes, in case
         * someone saved the clone and checks it against foo.caller for a foo
         * called from the active method.
         *
         * We do not check suspended frames. They can't be reached via caller,
         * so the only way they could have the method's joined function object
         * as callee is through an API abusage. We break any such edge case.
         */
        if (obj->hasMethodBarrier()) {
            JSObject *funobj;

            if (IsFunctionObject(v, &funobj)) {
                JSFunction *fun = GET_FUNCTION_PRIVATE(cx, funobj);

                if (fun != funobj) {
                    for (JSStackFrame *fp = cx->maybefp(); fp; fp = fp->prev()) {
                        if (fp->isFunctionFrame() &&
                            &fp->callee() == &fun->compiledFunObj() &&
                            fp->thisValue().isObject() &&
                            &fp->thisValue().toObject() == obj) {
                            fp->calleeValue().setObject(*funobj);
                        }
                    }
                }
            }
        }
    }

    ok = obj->removeProperty(cx, id);
    JS_UNLOCK_OBJ(cx, obj);

    return ok && js_SuppressDeletedProperty(cx, obj, id);
}

namespace js {

JSBool
DefaultValue(JSContext *cx, JSObject *obj, JSType hint, Value *vp)
{
    JS_ASSERT(hint != JSTYPE_OBJECT && hint != JSTYPE_FUNCTION);

    Value v = ObjectValue(*obj);
    if (hint == JSTYPE_STRING) {
        /*
         * Optimize for String objects with standard toString methods. Support
         * new String(...) instances whether mutated to have their own scope or
         * not, as well as direct String.prototype references.
         */
        if (obj->getClass() == &js_StringClass) {
            jsid toStringId = ATOM_TO_JSID(cx->runtime->atomState.toStringAtom);

            JS_LOCK_OBJ(cx, obj);
            JSObject *lockedobj = obj;
            const Shape *shape = obj->nativeLookup(toStringId);
            JSObject *pobj = obj;

            if (!shape) {
                pobj = obj->getProto();

                if (pobj && pobj->getClass() == &js_StringClass) {
                    JS_UNLOCK_OBJ(cx, obj);
                    JS_LOCK_OBJ(cx, pobj);
                    lockedobj = pobj;
                    shape = pobj->nativeLookup(toStringId);
                }
            }

            if (shape && shape->hasDefaultGetter() && pobj->containsSlot(shape->slot)) {
                const Value &fval = pobj->lockedGetSlot(shape->slot);

                JSObject *funobj;
                if (IsFunctionObject(fval, &funobj)) {
                    JSFunction *fun = funobj->getFunctionPrivate();
                    if (fun->maybeNative() == js_str_toString) {
                        JS_UNLOCK_OBJ(cx, lockedobj);
                        *vp = obj->getPrimitiveThis();
                        return JS_TRUE;
                    }
                }
            }
            JS_UNLOCK_OBJ(cx, lockedobj);
        }

        /*
         * Propagate the exception if js_TryMethod finds an appropriate
         * method, and calling that method returned failure.
         */
        if (!js_TryMethod(cx, obj, cx->runtime->atomState.toStringAtom,
                          0, NULL, &v)) {
            return JS_FALSE;
        }

        if (!v.isPrimitive()) {
            if (!obj->getClass()->convert(cx, obj, hint, &v))
                return JS_FALSE;
        }
    } else {
        if (!obj->getClass()->convert(cx, obj, hint, &v))
            return JS_FALSE;
        if (v.isObject()) {
            JS_ASSERT(hint != TypeOfValue(cx, v));
            if (!js_TryMethod(cx, obj, cx->runtime->atomState.toStringAtom, 0, NULL, &v))
                return JS_FALSE;
        }
    }
    if (v.isObject()) {
        /* Avoid recursive death when decompiling in js_ReportValueError. */
        JSString *str;
        if (hint == JSTYPE_STRING) {
            str = JS_InternString(cx, obj->getClass()->name);
            if (!str)
                return JS_FALSE;
        } else {
            str = NULL;
        }
        vp->setObject(*obj);
        js_ReportValueError2(cx, JSMSG_CANT_CONVERT_TO,
                             JSDVG_SEARCH_STACK, *vp, str,
                             (hint == JSTYPE_VOID)
                             ? "primitive type"
                             : JS_TYPE_STR(hint));
        return JS_FALSE;
    }
    *vp = v;
    return JS_TRUE;
}

} /* namespace js */

JS_FRIEND_API(JSBool)
js_Enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op, Value *statep, jsid *idp)
{
    /* If the class has a custom JSCLASS_NEW_ENUMERATE hook, call it. */
    Class *clasp = obj->getClass();
    JSEnumerateOp enumerate = clasp->enumerate;
    if (clasp->flags & JSCLASS_NEW_ENUMERATE) {
        JS_ASSERT(enumerate != JS_EnumerateStub);
        return ((NewEnumerateOp) enumerate)(cx, obj, enum_op, statep, idp);
    }

    if (!enumerate(cx, obj))
        return false;

    /* Tell InitNativeIterator to treat us like a native object. */
    JS_ASSERT(enum_op == JSENUMERATE_INIT || enum_op == JSENUMERATE_INIT_ALL);
    statep->setMagic(JS_NATIVE_ENUMERATE);
    return true;
}

namespace js {

JSBool
CheckAccess(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
            Value *vp, uintN *attrsp)
{
    JSBool writing;
    JSObject *pobj;
    JSProperty *prop;
    Class *clasp;
    const Shape *shape;
    JSSecurityCallbacks *callbacks;
    CheckAccessOp check;

    while (JS_UNLIKELY(obj->getClass() == &js_WithClass))
        obj = obj->getProto();

    writing = (mode & JSACC_WRITE) != 0;
    switch (mode & JSACC_TYPEMASK) {
      case JSACC_PROTO:
        pobj = obj;
        if (!writing)
            vp->setObjectOrNull(obj->getProto());
        *attrsp = JSPROP_PERMANENT;
        break;

      case JSACC_PARENT:
        JS_ASSERT(!writing);
        pobj = obj;
        vp->setObject(*obj->getParent());
        *attrsp = JSPROP_READONLY | JSPROP_PERMANENT;
        break;

      default:
        if (!obj->lookupProperty(cx, id, &pobj, &prop))
            return JS_FALSE;
        if (!prop) {
            if (!writing)
                vp->setUndefined();
            *attrsp = 0;
            pobj = obj;
            break;
        }

        if (!pobj->isNative()) {
            if (!writing) {
                    vp->setUndefined();
                *attrsp = 0;
            }
            break;
        }

        shape = (Shape *)prop;
        *attrsp = shape->attributes();
        if (!writing) {
            if (pobj->containsSlot(shape->slot))
                *vp = pobj->lockedGetSlot(shape->slot);
            else
                vp->setUndefined();
        }
        JS_UNLOCK_OBJ(cx, pobj);
    }

    /*
     * If obj's class has a stub (null) checkAccess hook, use the per-runtime
     * checkObjectAccess callback, if configured.
     *
     * We don't want to require all classes to supply a checkAccess hook; we
     * need that hook only for certain classes used when precompiling scripts
     * and functions ("brutal sharing").  But for general safety of built-in
     * magic properties like __proto__, we route all access checks, even for
     * classes that stub out checkAccess, through the global checkObjectAccess
     * hook.  This covers precompilation-based sharing and (possibly
     * unintended) runtime sharing across trust boundaries.
     */
    clasp = pobj->getClass();
    check = clasp->checkAccess;
    if (!check) {
        callbacks = JS_GetSecurityCallbacks(cx);
        check = callbacks ? Valueify(callbacks->checkObjectAccess) : NULL;
    }
    return !check || check(cx, pobj, id, mode, vp);
}

}

JSType
js_TypeOf(JSContext *cx, JSObject *obj)
{
    /*
     * Unfortunately we have wrappers that are native objects and thus don't
     * overwrite js_TypeOf (i.e. XPCCrossOriginWrapper), so we have to
     * unwrap here.
     */
    obj = obj->wrappedObject(cx);

    /*
     * ECMA 262, 11.4.3 says that any native object that implements
     * [[Call]] should be of type "function". However, RegExp is of
     * type "object", not "function", for Web compatibility.
     */
    if (obj->isCallable()) {
        return (obj->getClass() != &js_RegExpClass)
               ? JSTYPE_FUNCTION
               : JSTYPE_OBJECT;
    }

    return JSTYPE_OBJECT;
}

#ifdef JS_THREADSAFE
void
js_DropProperty(JSContext *cx, JSObject *obj, JSProperty *prop)
{
    JS_UNLOCK_OBJ(cx, obj);
}
#endif

bool
js_IsDelegate(JSContext *cx, JSObject *obj, const Value &v)
{
    if (v.isPrimitive())
        return false;
    JSObject *obj2 = v.toObject().wrappedObject(cx);
    while ((obj2 = obj2->getProto()) != NULL) {
        if (obj2 == obj)
            return true;
    }
    return false;
}

bool
js::FindClassPrototype(JSContext *cx, JSObject *scopeobj, JSProtoKey protoKey,
                       JSObject **protop, Class *clasp)
{
    Value v;
    if (!js_FindClassObject(cx, scopeobj, protoKey, &v, clasp))
        return false;

    if (IsFunctionObject(v)) {
        JSObject *ctor = &v.toObject();
        if (!ctor->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom), &v))
            return false;
    }

    *protop = v.isObject() ? &v.toObject() : NULL;
    return true;
}

/*
 * The first part of this function has been hand-expanded and optimized into
 * NewBuiltinClassInstance in jsobjinlines.h.
 */
JSBool
js_GetClassPrototype(JSContext *cx, JSObject *scopeobj, JSProtoKey protoKey,
                     JSObject **protop, Class *clasp)
{
    VOUCH_DOES_NOT_REQUIRE_STACK();
    JS_ASSERT(JSProto_Null <= protoKey);
    JS_ASSERT(protoKey < JSProto_LIMIT);

    if (protoKey != JSProto_Null) {
        if (!scopeobj) {
            if (cx->hasfp())
                scopeobj = &cx->fp()->scopeChain();
            if (!scopeobj) {
                scopeobj = cx->globalObject;
                if (!scopeobj) {
                    *protop = NULL;
                    return true;
                }
            }
        }
        scopeobj = scopeobj->getGlobal();
        if (scopeobj->getClass()->flags & JSCLASS_IS_GLOBAL) {
            const Value &v = scopeobj->getReservedSlot(JSProto_LIMIT + protoKey);
            if (v.isObject()) {
                *protop = &v.toObject();
                return true;
            }
        }
    }

    return FindClassPrototype(cx, scopeobj, protoKey, protop, clasp);
}

/*
 * For shared precompilation of function objects, we support cloning on entry
 * to an execution context in which the function declaration or expression
 * should be processed as if it were not precompiled, where the precompiled
 * function's scope chain does not match the execution context's.  The cloned
 * function object carries its execution-context scope in its parent slot; it
 * links to the precompiled function (the "clone-parent") via its proto slot.
 *
 * Note that this prototype-based delegation leaves an unchecked access path
 * from the clone to the clone-parent's 'constructor' property.  If the clone
 * lives in a less privileged or shared scope than the clone-parent, this is
 * a security hole, a sharing hazard, or both.  Therefore we check all such
 * accesses with the following getter/setter pair, which we use when defining
 * 'constructor' in f.prototype for all function objects f.
 */
static JSBool
CheckCtorGetAccess(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    JSAtom *atom = cx->runtime->atomState.constructorAtom;
    JS_ASSERT(id == ATOM_TO_JSID(atom));
    uintN attrs;
    return CheckAccess(cx, obj, ATOM_TO_JSID(atom), JSACC_READ, vp, &attrs);
}

static JSBool
CheckCtorSetAccess(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    JSAtom *atom = cx->runtime->atomState.constructorAtom;
    JS_ASSERT(id == ATOM_TO_JSID(atom));
    uintN attrs;
    return CheckAccess(cx, obj, ATOM_TO_JSID(atom), JSACC_WRITE, vp, &attrs);
}

JSBool
js_SetClassPrototype(JSContext *cx, JSObject *ctor, JSObject *proto, uintN attrs)
{
    /*
     * Use the given attributes for the prototype property of the constructor,
     * as user-defined constructors have a DontDelete prototype (which may be
     * reset), while native or "system" constructors have DontEnum | ReadOnly |
     * DontDelete.
     */
    if (!ctor->defineProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom),
                              ObjectOrNullValue(proto), PropertyStub, PropertyStub, attrs)) {
        return JS_FALSE;
    }

    /*
     * ECMA says that Object.prototype.constructor, or f.prototype.constructor
     * for a user-defined function f, is DontEnum.
     */
    return proto->defineProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.constructorAtom),
                                 ObjectOrNullValue(ctor), CheckCtorGetAccess, CheckCtorSetAccess, 0);
}

JSBool
js_PrimitiveToObject(JSContext *cx, Value *vp)
{
    Value v = *vp;
    JS_ASSERT(v.isPrimitive());

    Class *clasp;
    if (v.isNumber())
        clasp = &js_NumberClass;
    else if (v.isString())
        clasp = &js_StringClass;
    else
        clasp = &js_BooleanClass;

    JSObject *obj = NewBuiltinClassInstance(cx, clasp);
    if (!obj)
        return JS_FALSE;

    obj->setPrimitiveThis(v);
    vp->setObject(*obj);
    return JS_TRUE;
}

JSBool
js_ValueToObjectOrNull(JSContext *cx, const Value &v, JSObject **objp)
{
    JSObject *obj;

    if (v.isObjectOrNull()) {
        obj = v.toObjectOrNull();
    } else if (v.isUndefined()) {
        obj = NULL;
    } else {
        Value tmp = v;
        if (!js_PrimitiveToObject(cx, &tmp))
            return JS_FALSE;
        obj = &tmp.toObject();
    }
    *objp = obj;
    return JS_TRUE;
}

JSObject *
js_ValueToNonNullObject(JSContext *cx, const Value &v)
{
    JSObject *obj;

    if (!js_ValueToObjectOrNull(cx, v, &obj))
        return NULL;
    if (!obj)
        js_ReportIsNullOrUndefined(cx, JSDVG_SEARCH_STACK, v, NULL);
    return obj;
}

JSBool
js_TryValueOf(JSContext *cx, JSObject *obj, JSType type, Value *rval)
{
    Value argv[1];

    argv[0].setString(ATOM_TO_STRING(cx->runtime->atomState.typeAtoms[type]));
    return js_TryMethod(cx, obj, cx->runtime->atomState.valueOfAtom,
                        1, argv, rval);
}

JSBool
js_TryMethod(JSContext *cx, JSObject *obj, JSAtom *atom,
             uintN argc, Value *argv, Value *rval)
{
    JS_CHECK_RECURSION(cx, return JS_FALSE);

    /*
     * Report failure only if an appropriate method was found, and calling it
     * returned failure.  We propagate failure in this case to make exceptions
     * behave properly.
     */
    JSErrorReporter older = JS_SetErrorReporter(cx, NULL);
    jsid id = ATOM_TO_JSID(atom);
    Value fval;
    JSBool ok = js_GetMethod(cx, obj, id, JSGET_NO_METHOD_BARRIER, &fval);
    JS_SetErrorReporter(cx, older);
    if (!ok)
        return false;

    if (fval.isPrimitive())
        return JS_TRUE;
    return ExternalInvoke(cx, obj, fval, argc, argv, rval);
}

#if JS_HAS_XDR

JSBool
js_XDRObject(JSXDRState *xdr, JSObject **objp)
{
    JSContext *cx;
    JSAtom *atom;
    Class *clasp;
    uint32 classId, classDef;
    JSProtoKey protoKey;
    JSObject *proto;

    cx = xdr->cx;
    atom = NULL;
    if (xdr->mode == JSXDR_ENCODE) {
        clasp = (*objp)->getClass();
        classId = JS_XDRFindClassIdByName(xdr, clasp->name);
        classDef = !classId;
        if (classDef) {
            if (!JS_XDRRegisterClass(xdr, Jsvalify(clasp), &classId))
                return JS_FALSE;
            protoKey = JSCLASS_CACHED_PROTO_KEY(clasp);
            if (protoKey != JSProto_Null) {
                classDef |= (protoKey << 1);
            } else {
                atom = js_Atomize(cx, clasp->name, strlen(clasp->name), 0);
                if (!atom)
                    return JS_FALSE;
            }
        }
    } else {
        clasp = NULL;           /* quell GCC overwarning */
        classDef = 0;
    }

    /*
     * XDR a flag word, which could be 0 for a class use, in which case no
     * name follows, only the id in xdr's class registry; 1 for a class def,
     * in which case the flag word is followed by the class name transferred
     * from or to atom; or a value greater than 1, an odd number that when
     * divided by two yields the JSProtoKey for class.  In the last case, as
     * in the 0 classDef case, no name is transferred via atom.
     */
    if (!JS_XDRUint32(xdr, &classDef))
        return JS_FALSE;
    if (classDef == 1 && !js_XDRAtom(xdr, &atom))
        return JS_FALSE;

    if (!JS_XDRUint32(xdr, &classId))
        return JS_FALSE;

    if (xdr->mode == JSXDR_DECODE) {
        if (classDef) {
            /* NB: we know that JSProto_Null is 0 here, for backward compat. */
            protoKey = (JSProtoKey) (classDef >> 1);
            if (!js_GetClassPrototype(cx, NULL, protoKey, &proto, clasp))
                return JS_FALSE;
            clasp = proto->getClass();
            if (!JS_XDRRegisterClass(xdr, Jsvalify(clasp), &classId))
                return JS_FALSE;
        } else {
            clasp = Valueify(JS_XDRFindClassById(xdr, classId));
            if (!clasp) {
                char numBuf[12];
                JS_snprintf(numBuf, sizeof numBuf, "%ld", (long)classId);
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_CANT_FIND_CLASS, numBuf);
                return JS_FALSE;
            }
        }
    }

    if (!clasp->xdrObject) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_CANT_XDR_CLASS, clasp->name);
        return JS_FALSE;
    }
    return clasp->xdrObject(xdr, objp);
}

#endif /* JS_HAS_XDR */

#ifdef JS_DUMP_SCOPE_METERS

#include <stdio.h>

JSBasicStats js_entry_count_bs = JS_INIT_STATIC_BASIC_STATS;

static void
MeterEntryCount(uintN count)
{
    JS_BASIC_STATS_ACCUM(&js_entry_count_bs, count);
}

void
js_DumpScopeMeters(JSRuntime *rt)
{
    static FILE *logfp;
    if (!logfp)
        logfp = fopen("/tmp/scope.stats", "a");

    {
        double mean, sigma;

        mean = JS_MeanAndStdDevBS(&js_entry_count_bs, &sigma);

        fprintf(logfp, "scopes %u entries %g mean %g sigma %g max %u",
                js_entry_count_bs.num, js_entry_count_bs.sum, mean, sigma,
                js_entry_count_bs.max);
    }

    JS_DumpHistogram(&js_entry_count_bs, logfp);
    JS_BASIC_STATS_INIT(&js_entry_count_bs);
    fflush(logfp);
}
#endif

#ifdef DEBUG
void
js_PrintObjectSlotName(JSTracer *trc, char *buf, size_t bufsize)
{
    JS_ASSERT(trc->debugPrinter == js_PrintObjectSlotName);

    JSObject *obj = (JSObject *)trc->debugPrintArg;
    uint32 slot = (uint32)trc->debugPrintIndex;
    JS_ASSERT(slot >= JSSLOT_START(obj->getClass()));

    const Shape *shape;
    if (obj->isNative()) {
        shape = obj->lastProperty();
        while (shape->previous() && shape->slot != slot)
            shape = shape->previous();
    } else {
        shape = NULL;
    }

    if (!shape) {
        const char *slotname = NULL;
        Class *clasp = obj->getClass();
        if (clasp->flags & JSCLASS_IS_GLOBAL) {
            uint32 key = slot - JSSLOT_START(clasp);
#define JS_PROTO(name,code,init)                                              \
    if ((code) == key) { slotname = js_##name##_str; goto found; }
#include "jsproto.tbl"
#undef JS_PROTO
        }
      found:
        if (slotname)
            JS_snprintf(buf, bufsize, "CLASS_OBJECT(%s)", slotname);
        else
            JS_snprintf(buf, bufsize, "**UNKNOWN SLOT %ld**", (long)slot);
    } else {
        jsid id = shape->id;
        if (JSID_IS_INT(id)) {
            JS_snprintf(buf, bufsize, "%ld", (long)JSID_TO_INT(id));
        } else if (JSID_IS_ATOM(id)) {
            js_PutEscapedString(buf, bufsize, JSID_TO_STRING(id), 0);
        } else {
            JS_snprintf(buf, bufsize, "**FINALIZED ATOM KEY**");
        }
    }
}
#endif

void
js_TraceObject(JSTracer *trc, JSObject *obj)
{
    JS_ASSERT(obj->isNative());

    JSContext *cx = trc->context;
    if (!obj->nativeEmpty() && IS_GC_MARKING_TRACER(trc)) {
        /*
         * Trim overlong dslots allocations from the GC, to avoid thrashing in
         * case of delete-happy code that settles down at a given population.
         * The !obj->nativeEmpty() guard above is due to the bug described by
         * the FIXME comment below.
         */
        size_t slots = obj->slotSpan();
        if (obj->numSlots() != slots)
            obj->shrinkSlots(cx, slots);
    }

#ifdef JS_DUMP_SCOPE_METERS
    MeterEntryCount(obj->propertyCount);
#endif

    obj->trace(trc);

    if (!JS_CLIST_IS_EMPTY(&cx->runtime->watchPointList))
        js_TraceWatchPoints(trc, obj);

    /* No one runs while the GC is running, so we can use LOCKED_... here. */
    Class *clasp = obj->getClass();
    if (clasp->mark) {
        if (clasp->flags & JSCLASS_MARK_IS_TRACE)
            ((JSTraceOp) clasp->mark)(trc, obj);
        else if (IS_GC_MARKING_TRACER(trc))
            (void) clasp->mark(cx, obj, trc);
    }
    if (clasp->flags & JSCLASS_IS_GLOBAL) {
        JSCompartment *compartment = obj->getCompartment(cx);
        compartment->marked = true;
    }

    /*
     * NB: In case clasp->mark mutates something (which would be a bug, but we
     * want to be defensive), leave this code here -- don't move it up and
     * unify it with the |if (!traceScope)| section above.
     *
     * FIXME: We minimize nslots against obj->slotSpan because native objects
     * such as Date instances may have failed to advance slotSpan to cover all
     * reserved slots (this Date issue may be a bug in JSObject::growSlots, but
     * the general problem occurs in other built-in class implementations).
     */
    uint32 nslots = obj->numSlots();
    if (!obj->nativeEmpty() && obj->slotSpan() < nslots)
        nslots = obj->slotSpan();
    JS_ASSERT(nslots >= JSSLOT_START(clasp));

    for (uint32 i = JSSLOT_START(clasp); i != nslots; ++i) {
        const Value &v = obj->getSlot(i);
        JS_SET_TRACING_DETAILS(trc, js_PrintObjectSlotName, obj, i);
        MarkValueRaw(trc, v);
    }
}

void
js_ClearNative(JSContext *cx, JSObject *obj)
{
    /*
     * Clear obj of all obj's properties. FIXME: we do not clear reserved slots
     * lying below JSSLOT_FREE(clasp). JS_ClearScope does that.
     */
    JS_LOCK_OBJ(cx, obj);
    if (!obj->nativeEmpty()) {
        /* Now that we're done using real properties, clear obj. */
        obj->clear(cx);

        /* Clear slot values since obj->clear reset our shape to empty. */
        uint32 freeslot = JSSLOT_FREE(obj->getClass());
        uint32 n = obj->numSlots();
        for (uint32 i = freeslot; i < n; ++i)
            obj->setSlot(i, UndefinedValue());
    }
    JS_UNLOCK_OBJ(cx, obj);
}

bool
js_GetReservedSlot(JSContext *cx, JSObject *obj, uint32 index, Value *vp)
{
    if (!obj->isNative()) {
        vp->setUndefined();
        return true;
    }

    uint32 slot = JSSLOT_START(obj->getClass()) + index;
    JS_LOCK_OBJ(cx, obj);
    if (slot < obj->numSlots())
        *vp = obj->getSlot(slot);
    else
        vp->setUndefined();
    JS_UNLOCK_OBJ(cx, obj);
    return true;
}

bool
js_SetReservedSlot(JSContext *cx, JSObject *obj, uint32 index, const Value &v)
{
    if (!obj->isNative())
        return true;

    Class *clasp = obj->getClass();
    uint32 slot = JSSLOT_START(clasp) + index;

    JS_LOCK_OBJ(cx, obj);
    if (slot >= obj->numSlots()) {
        uint32 nslots = JSSLOT_FREE(clasp);
        JS_ASSERT(slot < nslots);
        if (!obj->allocSlots(cx, nslots)) {
            JS_UNLOCK_OBJ(cx, obj);
            return false;
        }
    }

    obj->setSlot(slot, v);
    GC_POKE(cx, JS_NULL);
    JS_UNLOCK_OBJ(cx, obj);
    return true;
}

JSObject *
JSObject::wrappedObject(JSContext *cx) const
{
    if (JSObjectOp op = getClass()->ext.wrappedObject) {
        if (JSObject *obj = op(cx, const_cast<JSObject *>(this)))
            return obj;
    }
    return const_cast<JSObject *>(this);
}

JSObject *
JSObject::getGlobal() const
{
    JSObject *obj = const_cast<JSObject *>(this);
    while (JSObject *parent = obj->getParent())
        obj = parent;
    return obj;
}

JSBool
js_ReportGetterOnlyAssignment(JSContext *cx)
{
    return JS_ReportErrorFlagsAndNumber(cx,
                                        JSREPORT_WARNING | JSREPORT_STRICT |
                                        JSREPORT_STRICT_MODE_ERROR,
                                        js_GetErrorMessage, NULL,
                                        JSMSG_GETTER_ONLY);
}

JSCompartment *
JSObject::getCompartment(JSContext *cx)
{
    JSObject *obj = getGlobal();

    Class *clasp = obj->getClass();
    if (!(clasp->flags & JSCLASS_IS_GLOBAL)) {
#if JS_HAS_XML_SUPPORT
        // The magic AnyName object is runtime-wide.
        if (clasp == &js_AnyNameClass)
            return cx->runtime->defaultCompartment;

        // The magic function namespace object is runtime-wide.
        if (clasp == &js_NamespaceClass &&
            obj->getNameURI() == ATOM_TO_JSVAL(cx->runtime->
                                               atomState.functionNamespaceURIAtom)) {
            return cx->runtime->defaultCompartment;
        }
#endif

        /*
         * Script objects and compile-time Function, Block, RegExp objects
         * are not parented.
         */
        if (clasp == &js_FunctionClass || clasp == &js_BlockClass || clasp == &js_RegExpClass ||
            clasp == &js_ScriptClass) {
            // This is a bogus answer, but it'll do for now.
            return cx->runtime->defaultCompartment;
        }
        JS_NOT_REACHED("non-global object at end of scope chain");
    }
    const Value &v = obj->getReservedSlot(JSRESERVED_GLOBAL_COMPARTMENT);
    return (JSCompartment *)v.toPrivate();
}

JS_FRIEND_API(JSBool)
js_GetterOnlyPropertyStub(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_GETTER_ONLY);
    return JS_FALSE;
}

#ifdef DEBUG

/*
 * Routines to print out values during debugging.  These are FRIEND_API to help
 * the debugger find them and to support temporarily hacking js_Dump* calls
 * into other code.
 */

void
dumpChars(const jschar *s, size_t n)
{
    size_t i;

    if (n == (size_t) -1) {
        while (s[++n]) ;
    }

    fputc('"', stderr);
    for (i = 0; i < n; i++) {
        if (s[i] == '\n')
            fprintf(stderr, "\\n");
        else if (s[i] == '\t')
            fprintf(stderr, "\\t");
        else if (s[i] >= 32 && s[i] < 127)
            fputc(s[i], stderr);
        else if (s[i] <= 255)
            fprintf(stderr, "\\x%02x", (unsigned int) s[i]);
        else
            fprintf(stderr, "\\u%04x", (unsigned int) s[i]);
    }
    fputc('"', stderr);
}

JS_FRIEND_API(void)
js_DumpChars(const jschar *s, size_t n)
{
    fprintf(stderr, "jschar * (%p) = ", (void *) s);
    dumpChars(s, n);
    fputc('\n', stderr);
}

void
dumpString(JSString *str)
{
    dumpChars(str->chars(), str->length());
}

JS_FRIEND_API(void)
js_DumpString(JSString *str)
{
    fprintf(stderr, "JSString* (%p) = jschar * (%p) = ",
            (void *) str, (void *) str->chars());
    dumpString(str);
    fputc('\n', stderr);
}

JS_FRIEND_API(void)
js_DumpAtom(JSAtom *atom)
{
    fprintf(stderr, "JSAtom* (%p) = ", (void *) atom);
    js_DumpString(ATOM_TO_STRING(atom));
}

void
dumpValue(const Value &v)
{
    if (v.isNull())
        fprintf(stderr, "null");
    else if (v.isUndefined())
        fprintf(stderr, "undefined");
    else if (v.isInt32())
        fprintf(stderr, "%d", v.toInt32());
    else if (v.isDouble())
        fprintf(stderr, "%g", v.toDouble());
    else if (v.isString())
        dumpString(v.toString());
    else if (v.isObject() && v.toObject().isFunction()) {
        JSObject *funobj = &v.toObject();
        JSFunction *fun = GET_FUNCTION_PRIVATE(cx, funobj);
        fprintf(stderr, "<%s %s at %p (JSFunction at %p)>",
                fun->atom ? "function" : "unnamed",
                fun->atom ? JS_GetStringBytes(ATOM_TO_STRING(fun->atom)) : "function",
                (void *) funobj,
                (void *) fun);
    } else if (v.isObject()) {
        JSObject *obj = &v.toObject();
        Class *clasp = obj->getClass();
        fprintf(stderr, "<%s%s at %p>",
                clasp->name,
                (clasp == &js_ObjectClass) ? "" : " object",
                (void *) obj);
    } else if (v.isBoolean()) {
        if (v.toBoolean())
            fprintf(stderr, "true");
        else
            fprintf(stderr, "false");
    } else if (v.isMagic()) {
        fprintf(stderr, "<invalid");
#ifdef DEBUG
        switch (v.whyMagic()) {
          case JS_ARRAY_HOLE:        fprintf(stderr, " array hole");         break;
          case JS_ARGS_HOLE:         fprintf(stderr, " args hole");          break;
          case JS_NATIVE_ENUMERATE:  fprintf(stderr, " native enumeration"); break;
          case JS_NO_ITER_VALUE:     fprintf(stderr, " no iter value");      break;
          case JS_GENERATOR_CLOSING: fprintf(stderr, " generator closing");  break;
          default:                   fprintf(stderr, " ?!");                 break;
        }
#endif
        fprintf(stderr, ">");
    } else {
        fprintf(stderr, "unexpected value");
    }
}

JS_FRIEND_API(void)
js_DumpValue(const Value &val)
{
    dumpValue(val);
    fputc('\n', stderr);
}

JS_FRIEND_API(void)
js_DumpId(jsid id)
{
    fprintf(stderr, "jsid %p = ", (void *) JSID_BITS(id));
    dumpValue(IdToValue(id));
    fputc('\n', stderr);
}

static void
DumpShape(const Shape &shape)
{
    jsid id = shape.id;
    uint8 attrs = shape.attributes();

    fprintf(stderr, "    ");
    if (attrs & JSPROP_ENUMERATE) fprintf(stderr, "enumerate ");
    if (attrs & JSPROP_READONLY) fprintf(stderr, "readonly ");
    if (attrs & JSPROP_PERMANENT) fprintf(stderr, "permanent ");
    if (attrs & JSPROP_GETTER) fprintf(stderr, "getter ");
    if (attrs & JSPROP_SETTER) fprintf(stderr, "setter ");
    if (attrs & JSPROP_SHARED) fprintf(stderr, "shared ");
    if (shape.isAlias()) fprintf(stderr, "alias ");
    if (JSID_IS_ATOM(id))
        dumpString(JSID_TO_STRING(id));
    else if (JSID_IS_INT(id))
        fprintf(stderr, "%d", (int) JSID_TO_INT(id));
    else
        fprintf(stderr, "unknown jsid %p", (void *) JSID_BITS(id));
    fprintf(stderr, ": slot %d", shape.slot);
    fprintf(stderr, "\n");
}

JS_FRIEND_API(void)
js_DumpObject(JSObject *obj)
{
    uint32 i, slots;
    Class *clasp;
    jsuint reservedEnd;

    fprintf(stderr, "object %p\n", (void *) obj);
    clasp = obj->getClass();
    fprintf(stderr, "class %p %s\n", (void *)clasp, clasp->name);

    fprintf(stderr, "flags:");
    uint32 flags = obj->flags;
    if (flags & JSObject::DELEGATE) fprintf(stderr, " delegate");
    if (flags & JSObject::SYSTEM) fprintf(stderr, " system");
    if (flags & JSObject::NOT_EXTENSIBLE) fprintf(stderr, " not extensible");
    if (flags & JSObject::BRANDED) fprintf(stderr, " branded");
    if (flags & JSObject::GENERIC) fprintf(stderr, " generic");
    if (flags & JSObject::METHOD_BARRIER) fprintf(stderr, " method_barrier");
    if (flags & JSObject::INDEXED) fprintf(stderr, " indexed");
    if (flags & JSObject::OWN_SHAPE) fprintf(stderr, " own_shape");

    bool anyFlags = flags != 0;
    if (obj->isNative()) {
        if (obj->inDictionaryMode()) {
            fprintf(stderr, " inDictionaryMode");
            anyFlags = true;
        }
        if (obj->hasPropertyTable()) {
            fprintf(stderr, " hasPropertyTable");
            anyFlags = true;
        }
    }
    if (!anyFlags)
        fprintf(stderr, " none");
    fprintf(stderr, "\n");

    if (obj->isDenseArray()) {
        slots = JS_MIN(obj->getArrayLength(), obj->getDenseArrayCapacity());
        fprintf(stderr, "elements\n");
        for (i = 0; i < slots; i++) {
            fprintf(stderr, " %3d: ", i);
            dumpValue(obj->getDenseArrayElement(i));
            fprintf(stderr, "\n");
            fflush(stderr);
        }
        return;
    }

    if (obj->isNative()) {
        fprintf(stderr, "properties:\n");
        for (Shape::Range r = obj->lastProperty()->all(); !r.empty(); r.popFront())
            DumpShape(r.front());
    } else {
        if (!obj->isNative())
            fprintf(stderr, "not native\n");
    }

    fprintf(stderr, "proto ");
    dumpValue(ObjectOrNullValue(obj->getProto()));
    fputc('\n', stderr);

    fprintf(stderr, "parent ");
    dumpValue(ObjectOrNullValue(obj->getParent()));
    fputc('\n', stderr);

    i = JSSLOT_PRIVATE;
    if (clasp->flags & JSCLASS_HAS_PRIVATE) {
        i = JSSLOT_PRIVATE + 1;
        fprintf(stderr, "private %p\n", obj->getPrivate());
    }

    fprintf(stderr, "slots:\n");
    reservedEnd = i + JSCLASS_RESERVED_SLOTS(clasp);
    slots = obj->slotSpan();
    for (; i < slots; i++) {
        fprintf(stderr, " %3d ", i);
        if (i < reservedEnd)
            fprintf(stderr, "(reserved) ");
        fprintf(stderr, "= ");
        dumpValue(obj->getSlot(i));
        fputc('\n', stderr);
    }
    fputc('\n', stderr);
}

static void
MaybeDumpObject(const char *name, JSObject *obj)
{
    if (obj) {
        fprintf(stderr, "  %s: ", name);
        dumpValue(ObjectValue(*obj));
        fputc('\n', stderr);
    }
}

static void
MaybeDumpValue(const char *name, const Value &v)
{
    if (!v.isNull()) {
        fprintf(stderr, "  %s: ", name);
        dumpValue(v);
        fputc('\n', stderr);
    }
}

JS_FRIEND_API(void)
js_DumpStackFrame(JSContext *cx, JSStackFrame *start)
{
    /* This should only called during live debugging. */
    VOUCH_DOES_NOT_REQUIRE_STACK();

    if (!start)
        start = cx->maybefp();
    FrameRegsIter i(cx);
    while (!i.done() && i.fp() != start)
        ++i;

    if (i.done()) {
        fprintf(stderr, "fp = %p not found in cx = %p\n", (void *)start, (void *)cx);
        return;
    }

    for (; !i.done(); ++i) {
        JSStackFrame *const fp = i.fp();

        fprintf(stderr, "JSStackFrame at %p\n", (void *) fp);
        if (fp->isFunctionFrame()) {
            fprintf(stderr, "callee fun: ");
            dumpValue(ObjectValue(fp->callee()));
        } else {
            fprintf(stderr, "global frame, no callee");
        }
        fputc('\n', stderr);

        if (fp->isScriptFrame()) {
            fprintf(stderr, "file %s line %u\n",
                    fp->script()->filename, (unsigned) fp->script()->lineno);
        }

        if (jsbytecode *pc = i.pc()) {
            if (!fp->isScriptFrame()) {
                fprintf(stderr, "*** pc && !script, skipping frame\n\n");
                continue;
            }
            if (fp->hasImacropc()) {
                fprintf(stderr, "  pc in imacro at %p\n  called from ", pc);
                pc = fp->imacropc();
            } else {
                fprintf(stderr, "  ");
            }
            fprintf(stderr, "pc = %p\n", pc);
            fprintf(stderr, "  current op: %s\n", js_CodeName[*pc]);
        }
        Value *sp = i.sp();
        fprintf(stderr, "  slots: %p\n", (void *) fp->slots());
        fprintf(stderr, "  sp:    %p = slots + %u\n", (void *) sp, (unsigned) (sp - fp->slots()));
        if (sp - fp->slots() < 10000) { // sanity
            for (Value *p = fp->slots(); p < sp; p++) {
                fprintf(stderr, "    %p: ", (void *) p);
                dumpValue(*p);
                fputc('\n', stderr);
            }
        }
        if (fp->isFunctionFrame() && !fp->isEvalFrame()) {
            fprintf(stderr, "  actuals: %p (%u) ", (void *) fp->actualArgs(), (unsigned) fp->numActualArgs());
            fprintf(stderr, "  formals: %p (%u)\n", (void *) fp->formalArgs(), (unsigned) fp->numFormalArgs());
        }
        MaybeDumpObject("callobj", fp->maybeCallObj());
        MaybeDumpObject("argsobj", fp->maybeArgsObj());
        MaybeDumpValue("this", fp->thisValue());
        fprintf(stderr, "  rval: ");
        dumpValue(fp->returnValue());
        fputc('\n', stderr);

        fprintf(stderr, "  flags:");
        if (fp->isConstructing())
            fprintf(stderr, " constructing");
        if (fp->hasOverriddenArgs())
            fprintf(stderr, " overridden_args");
        if (fp->isAssigning())
            fprintf(stderr, " assigning");
        if (fp->isDebuggerFrame())
            fprintf(stderr, " debugger");
        if (fp->isEvalFrame())
            fprintf(stderr, " eval");
        if (fp->isYielding())
            fprintf(stderr, " yielding");
        if (fp->isGeneratorFrame())
            fprintf(stderr, " generator");
        fputc('\n', stderr);

        fprintf(stderr, "  scopeChain: (JSObject *) %p\n", (void *) &fp->scopeChain());

        fputc('\n', stderr);
    }
}

#ifdef DEBUG
bool
IsSaneThisObject(JSObject &obj)
{
    Class *clasp = obj.getClass();
    return clasp != &js_CallClass &&
           clasp != &js_BlockClass &&
           clasp != &js_DeclEnvClass &&
           clasp != &js_WithClass;
}
#endif

#endif /* DEBUG */

