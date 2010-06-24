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
#include "jsarena.h" /* Added by JSIFY */
#include "jsbit.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jshash.h" /* Added by JSIFY */
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

#include "jsdtracef.h"
#include "jsatominlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "jsautooplen.h"

using namespace js;

JS_FRIEND_DATA(JSObjectOps) js_ObjectOps = {
    NULL,
    js_LookupProperty,
    js_DefineProperty,
    js_GetProperty,
    js_SetProperty,
    js_GetAttributes,
    js_SetAttributes,
    js_DeleteProperty,
    js_Enumerate,
    js_TypeOf,
    js_TraceObject,
    NULL,   /* thisObject */
    js_Call,
    js_Construct,
    js_HasInstance,
    js_Clear
};

JSClass js_ObjectClass = {
    js_Object_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   NULL,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

#if JS_HAS_OBJ_PROTO_PROP

static JSBool
obj_getProto(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

static JSBool
obj_setProto(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

static JSPropertySpec object_props[] = {
    {js_proto_str, 0, JSPROP_PERMANENT|JSPROP_SHARED, obj_getProto, obj_setProto},
    {0,0,0,0,0}
};

static JSBool
obj_getProto(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    /* Let CheckAccess get the slot's value, based on the access mode. */
    uintN attrs;
    id = ATOM_TO_JSID(cx->runtime->atomState.protoAtom);
    return CheckAccess(cx, obj, id, JSACC_PROTO, vp, &attrs);
}

static JSBool
obj_setProto(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    if (!JSVAL_IS_OBJECT(*vp))
        return JS_TRUE;

    JSObject *pobj = JSVAL_TO_OBJECT(*vp);
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
    return JSHashNumber(uintptr_t(key) >> JSVAL_TAGBITS);
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
            AutoValueRooter v(cx, JSVAL_VOID);
            AutoValueRooter setter(cx, JSVAL_VOID);
            if (obj2->isNative()) {
                JSScopeProperty *sprop = (JSScopeProperty *) prop;
                hasGetter = sprop->hasGetterValue();
                hasSetter = sprop->hasSetterValue();
                if (hasGetter)
                    v.set(sprop->getterValue());
                if (hasSetter)
                    setter.set(sprop->setterValue());
                JS_UNLOCK_OBJ(cx, obj2);
            } else {
                hasGetter = hasSetter = false;
            }
            if (hasSetter) {
                /* Mark the getter, then set val to setter. */
                if (hasGetter && !JSVAL_IS_PRIMITIVE(v.value())) {
                    ok = !!MarkSharpObjects(cx, JSVAL_TO_OBJECT(v.value()), NULL);
                    if (!ok)
                        break;
                }
                v.set(setter.value());
            } else if (!hasGetter) {
                ok = obj->getProperty(cx, id, v.addr());
                if (!ok)
                    break;
            }
            if (!JSVAL_IS_PRIMITIVE(v.value()) &&
                !MarkSharpObjects(cx, JSVAL_TO_OBJECT(v.value()), NULL)) {
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
    JS_CALL_OBJECT_TRACER((JSTracer *)arg, (JSObject *)he->key,
                          "sharp table entry");
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
obj_toSource(JSContext *cx, uintN argc, jsval *vp)
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
    jsval *val;
    JSString *gsop[2];
    JSString *idstr, *valstr, *str;

    JS_CHECK_RECURSION(cx, return JS_FALSE);

    jsval localroot[4] = {JSVAL_NULL, JSVAL_NULL, JSVAL_NULL, JSVAL_NULL};
    AutoArrayRooter tvr(cx, JS_ARRAY_LENGTH(localroot), localroot);

    /* If outermost, we need parentheses to be an expression, not a block. */
    outermost = (cx->sharpObjectMap.depth == 0);
    obj = JS_THIS_OBJECT(cx, vp);
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
        chars = (jschar *) js_malloc(((outermost ? 4 : 2) + 1) * sizeof(jschar));
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
         * Convert id to a jsval and then to a string.  Decide early whether we
         * prefer get/set or old getter/setter syntax.
         */
        idstr = js_ValueToString(cx, ID_TO_VALUE(id));
        if (!idstr) {
            ok = JS_FALSE;
            obj2->dropProperty(cx, prop);
            goto error;
        }
        *vp = STRING_TO_JSVAL(idstr);                   /* local root */

        jsint valcnt = 0;
        if (prop) {
            bool doGet = true;
            if (obj2->isNative()) {
                JSScopeProperty *sprop = (JSScopeProperty *) prop;
                unsigned attrs = sprop->attributes();
                if (attrs & JSPROP_GETTER) {
                    doGet = false;
                    val[valcnt] = sprop->getterValue();
                    gsop[valcnt] = ATOM_TO_STRING(cx->runtime->atomState.getAtom);
                    valcnt++;
                }
                if (attrs & JSPROP_SETTER) {
                    doGet = false;
                    val[valcnt] = sprop->setterValue();
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
            *vp = STRING_TO_JSVAL(idstr);               /* local root */
        }
        idstr->getCharsAndLength(idstrchars, idstrlength);

        for (jsint j = 0; j < valcnt; j++) {
            /*
             * Censor an accessor descriptor getter or setter part if it's
             * undefined.
             */
            if (gsop[j] && JSVAL_IS_VOID(val[j]))
                continue;

            /* Convert val[j] to its canonical source form. */
            valstr = js_ValueToSource(cx, val[j]);
            if (!valstr) {
                ok = JS_FALSE;
                goto error;
            }
            localroot[j] = STRING_TO_JSVAL(valstr);     /* local root */
            valstr->getCharsAndLength(vchars, vlength);

            /*
             * If val[j] is a non-sharp object, and we're not serializing an
             * accessor (ECMA syntax can't accommodate sharpened accessors),
             * consider sharpening it.
             */
            vsharp = NULL;
            vsharplength = 0;
#if JS_HAS_SHARP_VARS
            if (!gsop[j] && !JSVAL_IS_PRIMITIVE(val[j]) && vchars[0] != '#') {
                he = js_EnterSharpObject(cx, JSVAL_TO_OBJECT(val[j]), NULL, &vsharp);
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
            if (gsop[j] && VALUE_IS_FUNCTION(cx, val[j])) {
                JSFunction *fun = JS_ValueToFunction(cx, val[j]);
                const jschar *start = vchars;
                const jschar *end = vchars + vlength;

                uint8 parenChomp = 0;
                if (vchars[0] == '(') {
                    vchars++;
                    parenChomp = 1;
                }

                /*
                 * Try to jump "getter" or "setter" keywords, if we suspect
                 * they might appear here.  This code can be confused by people
                 * defining Function.prototype.toString, so let's be cautious.
                 */
                if (JSFUN_GETTER_TEST(fun->flags) || JSFUN_SETTER_TEST(fun->flags)) {
                    /* skip "getter/setter" */
                    const jschar *tmp = js_strchr_limit(vchars, ' ', end);
                    if (tmp)
                        vchars = tmp + 1;
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
    *vp = STRING_TO_JSVAL(str);
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
obj_toString(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return false;

    JSString *str = js::obj_toStringHelper(cx, obj);
    if (!str)
        return false;

    *vp = STRING_TO_JSVAL(str);
    return true;
}

static JSBool
obj_toLocaleString(JSContext *cx, uintN argc, jsval *vp)
{
    jsval thisv;
    JSString *str;

    thisv = JS_THIS(cx, vp);
    if (JSVAL_IS_NULL(thisv))
        return JS_FALSE;

    str = js_ValueToString(cx, thisv);
    if (!str)
        return JS_FALSE;

    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
obj_valueOf(JSContext *cx, uintN argc, jsval *vp)
{
    *vp = JS_THIS(cx, vp);
    return !JSVAL_IS_NULL(*vp);
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

JSObject *
js_CheckScopeChainValidity(JSContext *cx, JSObject *scopeobj, const char *caller)
{
    JSClass *clasp;
    JSExtendedClass *xclasp;
    JSObject *inner;

    if (!scopeobj)
        goto bad;

    OBJ_TO_INNER_OBJECT(cx, scopeobj);
    if (!scopeobj)
        return NULL;

    inner = scopeobj;

    /* XXX This is an awful gross hack. */
    while (scopeobj) {
        clasp = scopeobj->getClass();
        if (clasp->flags & JSCLASS_IS_EXTENDED) {
            xclasp = (JSExtendedClass*)clasp;
            if (xclasp->innerObject &&
                xclasp->innerObject(cx, scopeobj) != scopeobj) {
                goto bad;
            }
        }

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
    flags = JS_GetScriptFilenameFlags(caller->script);
    if ((flags & JSFILENAME_PROTECTED) &&
        principals &&
        strcmp(principals->codebase, "[System Principal]")) {
        *linenop = 0;
        return principals->codebase;
    }

    jsbytecode *pc = caller->pc(cx);
    if (pc && js_GetOpcode(cx, caller->script, pc) == JSOP_EVAL) {
        JS_ASSERT(js_GetOpcode(cx, caller->script, pc + JSOP_EVAL_LENGTH) == JSOP_LINENO);
        *linenop = GET_UINT16(pc + JSOP_EVAL_LENGTH);
    } else {
        *linenop = js_FramePCToLineNumber(cx, caller);
    }
    return caller->script->filename;
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
obj_eval(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc < 1) {
        *vp = JSVAL_VOID;
        return JS_TRUE;
    }

    JSStackFrame *caller = js_GetScriptedCaller(cx, NULL);
    if (!caller) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_INDIRECT_CALL, js_eval_str);
        return JS_FALSE;
    }

    jsbytecode *callerPC = caller->pc(cx);
    bool indirectCall = (callerPC && *callerPC != JSOP_EVAL);

    /*
     * This call to JSObject::wrappedObject is safe because of the security
     * checks we do below. However, the control flow below is confusing, so we
     * double check. There are two cases:
     * - Direct call: This object is never used. So unwrapping can't hurt.
     * - Indirect call: If this object isn't already the scope chain (which
     *   we're guaranteed to be allowed to access) then we do a security
     *   check.
     */
    jsval *argv = JS_ARGV(cx, vp);
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;
    obj = obj->wrappedObject(cx);

    /*
     * Ban all indirect uses of eval (global.foo = eval; global.foo(...)) and
     * calls that attempt to use a non-global object as the "with" object in
     * the former indirect case.
     */
    {
        JSObject *parent = obj->getParent();
        if (indirectCall || parent) {
            uintN flags = parent
                          ? JSREPORT_ERROR
                          : JSREPORT_STRICT | JSREPORT_WARNING;
            if (!JS_ReportErrorFlagsAndNumber(cx, flags, js_GetErrorMessage, NULL,
                                              JSMSG_BAD_INDIRECT_CALL,
                                              js_eval_str)) {
                return JS_FALSE;
            }
        }
    }

    if (!JSVAL_IS_STRING(argv[0])) {
        *vp = argv[0];
        return JS_TRUE;
    }

    /* Accept an optional trailing argument that overrides the scope object. */
    JSObject *scopeobj = NULL;
    if (argc >= 2) {
        if (!js_ValueToObject(cx, argv[1], &scopeobj))
            return JS_FALSE;
        argv[1] = OBJECT_TO_JSVAL(scopeobj);
        JSObject *obj = scopeobj;
        while (obj) {
            if (obj->isDenseArray() && !obj->makeDenseArraySlow(cx))
                return false;
            JSObject *parent = obj->getParent();
            if (!obj->isNative() ||
                (!parent && !(obj->getClass()->flags & JSCLASS_IS_GLOBAL))) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_INVALID_EVAL_SCOPE_ARG);
                return false;
            }
            obj = parent;
        }
    }

    /* Guard for conditionally-created with object below. */
    struct WithGuard {
        JSObject *obj;
        WithGuard() : obj(NULL) {}
        ~WithGuard() { if (obj) obj->setPrivate(NULL); }
    } withGuard;

    /* From here on, control must exit through label out with ok set. */
    MUST_FLOW_THROUGH("out");
    uintN staticLevel = caller->script->staticLevel + 1;
    if (!scopeobj) {
        /*
         * Bring fp->scopeChain up to date. We're either going to use
         * it (direct call) or save it and restore it (indirect call).
         */
        JSObject *callerScopeChain = js_GetScopeChain(cx, caller);
        if (!callerScopeChain)
            return JS_FALSE;

#if JS_HAS_EVAL_THIS_SCOPE
        /*
         * If we see an indirect call, then run eval in the global scope. We do
         * this so the compiler can make assumptions about what bindings may or
         * may not exist in the current frame if it doesn't see 'eval'.
         */
        if (indirectCall) {
            /* Pretend that we're top level. */
            staticLevel = 0;

            OBJ_TO_INNER_OBJECT(cx, obj);
            if (!obj)
                return JS_FALSE;

            if (!js_CheckPrincipalsAccess(cx, obj,
                                          JS_StackFramePrincipals(cx, caller),
                                          cx->runtime->atomState.evalAtom)) {
                return JS_FALSE;
            }

            /* NB: We know obj is a global object here. */
            JS_ASSERT(!obj->getParent());
            scopeobj = obj;
        } else {
            /*
             * Compile using the caller's current scope object.
             *
             * NB: This means that native callers (who reach this point through
             * the C API) must use the two parameter form.
             */
            JS_ASSERT_IF(caller->argv, caller->callobj);
            scopeobj = callerScopeChain;
        }
#endif
    } else {
        scopeobj = scopeobj->wrappedObject(cx);
        OBJ_TO_INNER_OBJECT(cx, scopeobj);
        if (!scopeobj)
            return JS_FALSE;

        if (!js_CheckPrincipalsAccess(cx, scopeobj,
                                      JS_StackFramePrincipals(cx, caller),
                                      cx->runtime->atomState.evalAtom))
            return JS_FALSE;

        /*
         * If scopeobj is not a global object, then we need to wrap it in a
         * with object to maintain invariants in the engine (see bug 520164).
         */
        if (scopeobj->getParent()) {
            JSObject *global = scopeobj->getGlobal();
            withGuard.obj = js_NewWithObject(cx, scopeobj, global, 0);
            if (!withGuard.obj)
                return JS_FALSE;

            scopeobj = withGuard.obj;
            JS_ASSERT(argc >= 2);
            argv[1] = OBJECT_TO_JSVAL(withGuard.obj);
        }

        /* We're pretending that we're in global code. */
        staticLevel = 0;
    }

    /* Ensure we compile this eval with the right object in the scope chain. */
    JSObject *result = js_CheckScopeChainValidity(cx, scopeobj, js_eval_str);
    JS_ASSERT_IF(result, result == scopeobj);
    if (!result)
        return JS_FALSE;

    // CSP check: is eval() allowed at all?
    // report errors via CSP is done in the script security mgr.
    if (!js_CheckContentSecurityPolicy(cx)) {
        JS_ReportError(cx, "call to eval() blocked by CSP");
        return  JS_FALSE;
    }

    JSObject *callee = JSVAL_TO_OBJECT(vp[0]);
    JSPrincipals *principals = js_EvalFramePrincipals(cx, callee, caller);
    uintN line;
    const char *file = js_ComputeFilename(cx, caller, principals, &line);

    JSString *str = JSVAL_TO_STRING(argv[0]);
    JSScript *script = NULL;

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
    if (!indirectCall && caller->fun) {
        uintN count = 0;
        JSScript **scriptp = bucket;

        EVAL_CACHE_METER(probe);
        while ((script = *scriptp) != NULL) {
            if (script->savedCallerFun &&
                script->staticLevel == staticLevel &&
                script->version == cx->version &&
                (script->principals == principals ||
                 (principals->subsume(principals, script->principals) &&
                  script->principals->subsume(script->principals, principals)))) {
                /*
                 * Get the prior (cache-filling) eval's saved caller function.
                 * See Compiler::compileScript in jsparse.cpp.
                 */
                JSFunction *fun = script->getFunction(0);

                if (fun == caller->fun) {
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
        script = Compiler::compileScript(cx, scopeobj, callerFrame,
                                         principals,
                                         TCF_COMPILE_N_GO | TCF_NEED_MUTABLE_SCRIPT,
                                         str->chars(), str->length(),
                                         NULL, file, line, str, staticLevel);
        if (!script)
            return JS_FALSE;
    }

    /*
     * Belt-and-braces: check that the lesser of eval's principals and the
     * caller's principals has access to scopeobj.
     */
    JSBool ok = js_CheckPrincipalsAccess(cx, scopeobj, principals,
                                         cx->runtime->atomState.evalAtom) &&
                js_Execute(cx, scopeobj, script, callerFrame, JSFRAME_EVAL, vp);

    script->u.nextToGC = *bucket;
    *bucket = script;
#ifdef CHECK_SCRIPT_OWNER
    script->owner = NULL;
#endif

    return ok;
}

#if JS_HAS_OBJ_WATCHPOINT

static JSBool
obj_watch_handler(JSContext *cx, JSObject *obj, jsval id, jsval old, jsval *nvp,
                  void *closure)
{
    JSObject *callable;
    JSSecurityCallbacks *callbacks;
    JSStackFrame *caller;
    JSPrincipals *subject, *watcher;
    JSResolvingKey key;
    JSResolvingEntry *entry;
    uint32 generation;
    jsval argv[3];
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
            subject = JS_StackFramePrincipals(cx, caller);

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

    argv[0] = id;
    argv[1] = old;
    argv[2] = *nvp;
    ok = js_InternalCall(cx, obj, OBJECT_TO_JSVAL(callable), 3, argv, nvp);
    js_StopResolving(cx, &key, JSRESFLAG_WATCH, entry, generation);
    return ok;
}

static JSBool
obj_watch(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *callable;
    jsval userid, value;
    jsid propid;
    JSObject *obj;
    uintN attrs;

    if (argc <= 1) {
        js_ReportMissingArg(cx, vp, 1);
        return JS_FALSE;
    }

    callable = js_ValueToCallableObject(cx, &vp[3], 0);
    if (!callable)
        return JS_FALSE;

    /* Compute the unique int/atom symbol id needed by js_LookupProperty. */
    userid = vp[2];
    if (!JS_ValueToId(cx, userid, &propid))
        return JS_FALSE;

    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !CheckAccess(cx, obj, propid, JSACC_WATCH, &value, &attrs))
        return JS_FALSE;

    *vp = JSVAL_VOID;

    if (attrs & JSPROP_READONLY)
        return JS_TRUE;
    if (obj->isDenseArray() && !obj->makeDenseArraySlow(cx))
        return JS_FALSE;
    return JS_SetWatchPoint(cx, obj, userid, obj_watch_handler, callable);
}

static JSBool
obj_unwatch(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;

    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;
    *vp = JSVAL_VOID;
    return JS_ClearWatchPoint(cx, obj, argc != 0 ? vp[2] : JSVAL_VOID,
                              NULL, NULL);
}

#endif /* JS_HAS_OBJ_WATCHPOINT */

/*
 * Prototype and property query methods, to complement the 'in' and
 * 'instanceof' operators.
 */

/* Proposed ECMA 15.2.4.5. */
static JSBool
obj_hasOwnProperty(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;

    obj = JS_THIS_OBJECT(cx, vp);
    return obj &&
           js_HasOwnPropertyHelper(cx, obj->map->ops->lookupProperty, argc, vp);
}

JSBool
js_HasOwnPropertyHelper(JSContext *cx, JSLookupPropOp lookup, uintN argc,
                        jsval *vp)
{
    jsid id;
    if (!JS_ValueToId(cx, argc != 0 ? vp[2] : JSVAL_VOID, &id))
        return JS_FALSE;

    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    JSObject *obj2;
    JSProperty *prop;
    if (!obj)
        return false;
    if (obj->isProxy()) {
        bool has;
        if (!JSProxy::hasOwn(cx, obj, id, &has))
            return false;
        *vp = BOOLEAN_TO_JSVAL(has);
        return true;
    }
    if (!js_HasOwnProperty(cx, lookup, obj, id, &obj2, &prop))
        return JS_FALSE;
    if (prop) {
        *vp = JSVAL_TRUE;
        obj2->dropProperty(cx, prop);
    } else {
        *vp = JSVAL_FALSE;
    }
    return JS_TRUE;
}

JSBool
js_HasOwnProperty(JSContext *cx, JSLookupPropOp lookup, JSObject *obj, jsid id,
                  JSObject **objp, JSProperty **propp)
{
    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED | JSRESOLVE_DETECTING);
    if (!lookup(cx, obj, id, objp, propp))
        return false;
    if (!*propp)
        return true;

    if (*objp == obj)
        return true;

    JSExtendedClass *xclasp;
    JSObject *outer;
    JSClass *clasp = (*objp)->getClass();
    if (!(clasp->flags & JSCLASS_IS_EXTENDED) ||
        !(xclasp = (JSExtendedClass *) clasp)->outerObject) {
        outer = NULL;
    } else {
        outer = xclasp->outerObject(cx, *objp);
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
            JSScopeProperty *sprop = reinterpret_cast<JSScopeProperty *>(*propp);
            if (sprop->isSharedPermanent())
                return true;
        }

        (*objp)->dropProperty(cx, *propp);
        *propp = NULL;
    }
    return true;
}

/* Proposed ECMA 15.2.4.6. */
static JSBool
obj_isPrototypeOf(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;
    JSBool b;
    if (!js_IsDelegate(cx, obj, argc != 0 ? vp[2] : JSVAL_VOID, &b))
        return JS_FALSE;
    *vp = BOOLEAN_TO_JSVAL(b);
    return JS_TRUE;
}

/* Proposed ECMA 15.2.4.7. */
static JSBool
obj_propertyIsEnumerable(JSContext *cx, uintN argc, jsval *vp)
{
    jsid id;
    JSObject *obj;

    if (!JS_ValueToId(cx, argc != 0 ? vp[2] : JSVAL_VOID, &id))
        return JS_FALSE;

    obj = JS_THIS_OBJECT(cx, vp);
    return obj && js_PropertyIsEnumerable(cx, obj, id, vp);
}

JSBool
js_PropertyIsEnumerable(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSObject *pobj;
    JSProperty *prop;
    if (!obj->lookupProperty(cx, id, &pobj, &prop))
        return JS_FALSE;

    if (!prop) {
        *vp = JSVAL_FALSE;
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
        JSScopeProperty *sprop = (JSScopeProperty *) prop;
        shared = sprop->isSharedPermanent();
        attrs = sprop->attributes();
        JS_UNLOCK_OBJ(cx, pobj);
    } else {
        shared = false;
        if (!pobj->getAttributes(cx, id, &attrs))
            return false;
    }
    if (pobj != obj && !shared) {
        *vp = JSVAL_FALSE;
        return true;
    }
    *vp = BOOLEAN_TO_JSVAL((attrs & JSPROP_ENUMERATE) != 0);
    return true;
}

#if OLD_GETTER_SETTER_METHODS

const char js_defineGetter_str[] = "__defineGetter__";
const char js_defineSetter_str[] = "__defineSetter__";
const char js_lookupGetter_str[] = "__lookupGetter__";
const char js_lookupSetter_str[] = "__lookupSetter__";

JS_FRIEND_API(JSBool)
js_obj_defineGetter(JSContext *cx, uintN argc, jsval *vp)
{
    jsval fval, junk;
    jsid id;
    JSObject *obj;
    uintN attrs;

    if (argc <= 1 || !js_IsCallable(vp[3])) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_GETTER_OR_SETTER,
                             js_getter_str);
        return JS_FALSE;
    }
    fval = vp[3];

    if (!JS_ValueToId(cx, vp[2], &id))
        return JS_FALSE;
    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !js_CheckRedeclaration(cx, obj, id, JSPROP_GETTER, NULL, NULL))
        return JS_FALSE;
    /*
     * Getters and setters are just like watchpoints from an access
     * control point of view.
     */
    if (!CheckAccess(cx, obj, id, JSACC_WATCH, &junk, &attrs))
        return JS_FALSE;
    *vp = JSVAL_VOID;
    return obj->defineProperty(cx, id, JSVAL_VOID,
                               CastAsPropertyOp(JSVAL_TO_OBJECT(fval)), JS_PropertyStub,
                               JSPROP_ENUMERATE | JSPROP_GETTER | JSPROP_SHARED);
}

JS_FRIEND_API(JSBool)
js_obj_defineSetter(JSContext *cx, uintN argc, jsval *vp)
{
    jsval fval, junk;
    jsid id;
    JSObject *obj;
    uintN attrs;

    if (argc <= 1 || !js_IsCallable(vp[3])) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_GETTER_OR_SETTER,
                             js_setter_str);
        return JS_FALSE;
    }
    fval = vp[3];

    if (!JS_ValueToId(cx, vp[2], &id))
        return JS_FALSE;
    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !js_CheckRedeclaration(cx, obj, id, JSPROP_SETTER, NULL, NULL))
        return JS_FALSE;
    /*
     * Getters and setters are just like watchpoints from an access
     * control point of view.
     */
    if (!CheckAccess(cx, obj, id, JSACC_WATCH, &junk, &attrs))
        return JS_FALSE;
    *vp = JSVAL_VOID;
    return obj->defineProperty(cx, id, JSVAL_VOID,
                               JS_PropertyStub, CastAsPropertyOp(JSVAL_TO_OBJECT(fval)),
                               JSPROP_ENUMERATE | JSPROP_SETTER | JSPROP_SHARED);
}

static JSBool
obj_lookupGetter(JSContext *cx, uintN argc, jsval *vp)
{
    jsid id;
    JSObject *obj, *pobj;
    JSProperty *prop;
    JSScopeProperty *sprop;

    if (!JS_ValueToId(cx, argc != 0 ? vp[2] : JSVAL_VOID, &id))
        return JS_FALSE;
    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !obj->lookupProperty(cx, id, &pobj, &prop))
        return JS_FALSE;
    *vp = JSVAL_VOID;
    if (prop) {
        if (pobj->isNative()) {
            sprop = (JSScopeProperty *) prop;
            if (sprop->hasGetterValue())
                *vp = sprop->getterValue();
            JS_UNLOCK_OBJ(cx, pobj);
        }
    }
    return JS_TRUE;
}

static JSBool
obj_lookupSetter(JSContext *cx, uintN argc, jsval *vp)
{
    jsid id;
    JSObject *obj, *pobj;
    JSProperty *prop;
    JSScopeProperty *sprop;

    if (!JS_ValueToId(cx, argc != 0 ? vp[2] : JSVAL_VOID, &id))
        return JS_FALSE;
    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !obj->lookupProperty(cx, id, &pobj, &prop))
        return JS_FALSE;
    *vp = JSVAL_VOID;
    if (prop) {
        if (pobj->isNative()) {
            sprop = (JSScopeProperty *) prop;
            if (sprop->hasSetterValue())
                *vp = sprop->setterValue();
            JS_UNLOCK_OBJ(cx, pobj);
        }
    }
    return JS_TRUE;
}
#endif /* OLD_GETTER_SETTER_METHODS */

JSBool
obj_getPrototypeOf(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;
    uintN attrs;

    if (argc == 0) {
        js_ReportMissingArg(cx, vp, 0);
        return JS_FALSE;
    }

    if (JSVAL_IS_PRIMITIVE(vp[2])) {
        char *bytes = js_DecompileValueGenerator(cx, 0 - argc, vp[2], NULL);
        if (!bytes)
            return JS_FALSE;
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_UNEXPECTED_TYPE, bytes, "not an object");
        JS_free(cx, bytes);
        return JS_FALSE;
    }

    obj = JSVAL_TO_OBJECT(vp[2]);
    return CheckAccess(cx, obj, ATOM_TO_JSID(cx->runtime->atomState.protoAtom),
                       JSACC_PROTO, vp, &attrs);
}

extern JSBool
js_NewPropertyDescriptorObject(JSContext *cx, jsid id, uintN attrs, jsval getter, jsval setter,
                               jsval value, jsval *vp)
{
    /* We have our own property, so start creating the descriptor. */
    JSObject *desc = NewBuiltinClassInstance(cx, &js_ObjectClass);
    if (!desc)
        return false;
    *vp = OBJECT_TO_JSVAL(desc); /* Root and return. */

    const JSAtomState &atomState = cx->runtime->atomState;
    if (attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
        if (!desc->defineProperty(cx, ATOM_TO_JSID(atomState.getAtom), getter,
                                  JS_PropertyStub, JS_PropertyStub, JSPROP_ENUMERATE) ||
            !desc->defineProperty(cx, ATOM_TO_JSID(atomState.setAtom), setter,
                                  JS_PropertyStub, JS_PropertyStub, JSPROP_ENUMERATE)) {
            return false;
        }
    } else {
        if (!desc->defineProperty(cx, ATOM_TO_JSID(atomState.valueAtom), value,
                                  JS_PropertyStub, JS_PropertyStub, JSPROP_ENUMERATE) ||
            !desc->defineProperty(cx, ATOM_TO_JSID(atomState.writableAtom),
                                  BOOLEAN_TO_JSVAL((attrs & JSPROP_READONLY) == 0),
                                  JS_PropertyStub, JS_PropertyStub, JSPROP_ENUMERATE)) {
            return false;
        }
    }

    return desc->defineProperty(cx, ATOM_TO_JSID(atomState.enumerableAtom),
                                BOOLEAN_TO_JSVAL((attrs & JSPROP_ENUMERATE) != 0),
                                JS_PropertyStub, JS_PropertyStub, JSPROP_ENUMERATE) &&
           desc->defineProperty(cx, ATOM_TO_JSID(atomState.configurableAtom),
                                BOOLEAN_TO_JSVAL((attrs & JSPROP_PERMANENT) == 0),
                                JS_PropertyStub, JS_PropertyStub, JSPROP_ENUMERATE);
}

JSBool
js_GetOwnPropertyDescriptor(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    if (obj->isProxy()) {
        if (!JSProxy::getOwnPropertyDescriptor(cx, obj, id, vp))
            return false;
    }

    JSObject *pobj;
    JSProperty *prop;
    if (!js_HasOwnProperty(cx, obj->map->ops->lookupProperty, obj, id, &pobj, &prop))
        return false;
    if (!prop) {
        *vp = JSVAL_VOID;
        return true;
    }

    jsval roots[] = { JSVAL_VOID, JSVAL_VOID, JSVAL_VOID };
    AutoArrayRooter tvr(cx, JS_ARRAY_LENGTH(roots), roots);
    unsigned attrs;
    bool doGet = true;
    if (pobj->isNative()) {
        JSScopeProperty *sprop = (JSScopeProperty *) prop;
        attrs = sprop->attributes();
        if (attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
            doGet = false;
            if (attrs & JSPROP_GETTER)
                roots[0] = sprop->getterValue();
            if (attrs & JSPROP_SETTER)
                roots[1] = sprop->setterValue();
        }
        JS_UNLOCK_OBJ(cx, pobj);
    } else if (!pobj->getAttributes(cx, id, &attrs)) {
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
GetFirstArgumentAsObject(JSContext *cx, uintN argc, jsval *vp, const char *method, JSObject **objp)
{
    if (argc == 0) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             method, "0", "s");
        return false;
    }

    jsval v = vp[2];
    if (JSVAL_IS_PRIMITIVE(v)) {
        char *bytes = js_DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, v, NULL);
        if (!bytes)
            return false;
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_UNEXPECTED_TYPE,
                             bytes, "not an object");
        JS_free(cx, bytes);
        return false;
    }

    *objp = JSVAL_TO_OBJECT(v);
    return true;
}

static JSBool
obj_getOwnPropertyDescriptor(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.getOwnPropertyDescriptor", &obj))
        return JS_FALSE;
    AutoIdRooter nameidr(cx);
    if (!JS_ValueToId(cx, argc >= 2 ? vp[3] : JSVAL_VOID, nameidr.addr()))
        return JS_FALSE;
    return js_GetOwnPropertyDescriptor(cx, obj, nameidr.id(), vp);
}

static JSBool
obj_keys(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.keys", &obj))
        return JS_FALSE;

    AutoValueVector props(cx);
    if (!GetPropertyNames(cx, obj, JSITER_OWNONLY, props))
        return JS_FALSE;

    for (size_t i = 0, len = props.length(); i < len; i++) {
        jsid id = jsid(props[i]);
        if (JSID_IS_INT(id) && !js_ValueToStringId(cx, INT_JSID_TO_JSVAL(id), &props[i]))
            return JS_FALSE;
    }

    JS_ASSERT(props.length() <= UINT32_MAX);
    JSObject *aobj = js_NewArrayObject(cx, jsuint(props.length()), props.begin(), false);
    if (!aobj)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(aobj);

    return JS_TRUE;
}

static JSBool
HasProperty(JSContext* cx, JSObject* obj, jsid id, jsval* vp, JSBool* answerp)
{
    if (!JS_HasPropertyById(cx, obj, id, answerp))
        return JS_FALSE;
    if (!*answerp) {
        *vp = JSVAL_VOID;
        return JS_TRUE;
    }
    return JS_GetPropertyById(cx, obj, id, vp);
}

PropertyDescriptor::PropertyDescriptor()
  : pd(JSVAL_VOID),
    id(INT_JSVAL_TO_JSID(JSVAL_ZERO)),
    value(JSVAL_VOID),
    get(JSVAL_VOID),
    set(JSVAL_VOID),
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
PropertyDescriptor::initialize(JSContext* cx, jsid id, jsval v)
{
    this->id = id;

    /* 8.10.5 step 1 */
    if (JSVAL_IS_PRIMITIVE(v)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_NONNULL_OBJECT);
        return false;
    }
    JSObject* desc = JSVAL_TO_OBJECT(v);

    /* Make a copy of the descriptor. We might need it later. */
    pd = v;

    /* Start with the proper defaults. */
    attrs = JSPROP_PERMANENT | JSPROP_READONLY;

    JSBool hasProperty;

    /* 8.10.5 step 3 */
    if (!HasProperty(cx, desc, ATOM_TO_JSID(cx->runtime->atomState.enumerableAtom), &v,
                     &hasProperty)) {
        return false;
    }
    if (hasProperty) {
        hasEnumerable = JS_TRUE;
        if (js_ValueToBoolean(v))
            attrs |= JSPROP_ENUMERATE;
    }

    /* 8.10.5 step 4 */
    if (!HasProperty(cx, desc, ATOM_TO_JSID(cx->runtime->atomState.configurableAtom), &v,
                     &hasProperty)) {
        return false;
    }
    if (hasProperty) {
        hasConfigurable = JS_TRUE;
        if (js_ValueToBoolean(v))
            attrs &= ~JSPROP_PERMANENT;
    }

    /* 8.10.5 step 5 */
    if (!HasProperty(cx, desc, ATOM_TO_JSID(cx->runtime->atomState.valueAtom), &v, &hasProperty))
        return false;
    if (hasProperty) {
        hasValue = true;
        value = v;
    }

    /* 8.10.6 step 6 */
    if (!HasProperty(cx, desc, ATOM_TO_JSID(cx->runtime->atomState.writableAtom), &v, &hasProperty))
        return false;
    if (hasProperty) {
        hasWritable = JS_TRUE;
        if (js_ValueToBoolean(v))
            attrs &= ~JSPROP_READONLY;
    }

    /* 8.10.7 step 7 */
    if (!HasProperty(cx, desc, ATOM_TO_JSID(cx->runtime->atomState.getAtom), &v, &hasProperty))
        return false;
    if (hasProperty) {
        if ((JSVAL_IS_PRIMITIVE(v) || !js_IsCallable(v)) && v != JSVAL_VOID) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_GET_SET_FIELD,
                                 js_getter_str);
            return false;
        }
        hasGet = true;
        get = v;
        attrs |= JSPROP_GETTER | JSPROP_SHARED;
    }

    /* 8.10.7 step 8 */
    if (!HasProperty(cx, desc, ATOM_TO_JSID(cx->runtime->atomState.setAtom), &v, &hasProperty))
        return false;
    if (hasProperty) {
        if ((JSVAL_IS_PRIMITIVE(v) || !js_IsCallable(v)) && v != JSVAL_VOID) {
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
        if (!js_ValueToStringId(cx, ID_TO_VALUE(id), &idstr))
           return JS_FALSE;
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, errorNumber,
                             JS_GetStringBytes(JSVAL_TO_STRING(ID_TO_VALUE(idstr))));
        return JS_FALSE;
    }

    *rval = false;
    return JS_TRUE;
}

static JSBool
Reject(JSContext *cx, uintN errorNumber, bool throwError, bool *rval)
{
    if (throwError) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, errorNumber);
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
DefinePropertyOnObject(JSContext *cx, JSObject *obj, const PropertyDescriptor &desc,
                       bool throwError, bool *rval)
{
    /* 8.12.9 step 1. */
    JSProperty *current;
    JSObject *obj2;
    JS_ASSERT(obj->map->ops->lookupProperty == js_LookupProperty);
    if (!js_HasOwnProperty(cx, js_LookupProperty, obj, desc.id, &obj2, &current))
        return JS_FALSE;

    JS_ASSERT(obj->map->ops->defineProperty == js_DefineProperty);

    /* 8.12.9 steps 2-4. */
    JSScope *scope = obj->scope();
    if (!current) {
        if (scope->sealed())
            return Reject(cx, JSMSG_OBJECT_NOT_EXTENSIBLE, throwError, rval);

        *rval = true;

        if (desc.isGenericDescriptor() || desc.isDataDescriptor()) {
            JS_ASSERT(obj->map->ops->defineProperty == js_DefineProperty);
            return js_DefineProperty(cx, obj, desc.id, desc.value,
                                     JS_PropertyStub, JS_PropertyStub, desc.attrs);
        }

        JS_ASSERT(desc.isAccessorDescriptor());

        /*
         * Getters and setters are just like watchpoints from an access
         * control point of view.
         */
        jsval dummy;
        uintN dummyAttrs;
        if (!CheckAccess(cx, obj, desc.id, JSACC_WATCH, &dummy, &dummyAttrs))
            return JS_FALSE;

        return js_DefineProperty(cx, obj, desc.id, JSVAL_VOID,
                                 desc.getter(), desc.setter(), desc.attrs);
    }

    /* 8.12.9 steps 5-6 (note 5 is merely a special case of 6). */
    jsval v = JSVAL_VOID;

    /*
     * In the special case of shared permanent properties, the "own" property
     * can be found on a different object.  In that case the returned property
     * might not be native, except: the shared permanent property optimization
     * is not applied if the objects have different classes (bug 320854), as
     * must be enforced by js_HasOwnProperty for the JSScopeProperty cast below
     * to be safe.
     */
    JS_ASSERT(obj->getClass() == obj2->getClass());

    JSScopeProperty *sprop = reinterpret_cast<JSScopeProperty *>(current);
    do {
        if (desc.isAccessorDescriptor()) {
            if (!sprop->isAccessorDescriptor())
                break;

            if (desc.hasGet &&
                !js_SameValue(desc.getterValue(),
                              sprop->hasGetterValue() ? sprop->getterValue() : JSVAL_VOID,
                              cx)) {
                break;
            }

            if (desc.hasSet &&
                !js_SameValue(desc.setterValue(),
                              sprop->hasSetterValue() ? sprop->setterValue() : JSVAL_VOID,
                              cx)) {
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
            if (sprop->isDataDescriptor()) {
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
                if (!sprop->configurable() &&
                    (!sprop->hasDefaultGetter() || !sprop->hasDefaultSetter())) {
                    return Reject(cx, obj2, current, JSMSG_CANT_REDEFINE_UNCONFIGURABLE_PROP,
                                  throwError, desc.id, rval);
                }

                if (!js_NativeGet(cx, obj, obj2, sprop, JSGET_NO_METHOD_BARRIER, &v)) {
                    /* current was dropped when the failure occurred. */
                    return JS_FALSE;
                }
            }

            if (desc.isDataDescriptor()) {
                if (!sprop->isDataDescriptor())
                    break;

                if (desc.hasValue && !js_SameValue(desc.value, v, cx))
                    break;
                if (desc.hasWritable && desc.writable() != sprop->writable())
                    break;
            } else {
                /* The only fields in desc will be handled below. */
                JS_ASSERT(desc.isGenericDescriptor());
            }
        }

        if (desc.hasConfigurable && desc.configurable() != sprop->configurable())
            break;
        if (desc.hasEnumerable && desc.enumerable() != sprop->enumerable())
            break;

        /* The conditions imposed by step 5 or step 6 apply. */
        obj2->dropProperty(cx, current);
        *rval = true;
        return JS_TRUE;
    } while (0);

    /* 8.12.9 step 7. */
    if (!sprop->configurable()) {
        /*
         * Since [[Configurable]] defaults to false, we don't need to check
         * whether it was specified.  We can't do likewise for [[Enumerable]]
         * because its putative value is used in a comparison -- a comparison
         * whose result must always be false per spec if the [[Enumerable]]
         * field is not present.  Perfectly pellucid logic, eh?
         */
        JS_ASSERT_IF(!desc.hasConfigurable, !desc.configurable());
        if (desc.configurable() ||
            (desc.hasEnumerable && desc.enumerable() != sprop->enumerable())) {
            return Reject(cx, obj2, current, JSMSG_CANT_REDEFINE_UNCONFIGURABLE_PROP, throwError,
                          desc.id, rval);
        }
    }

    if (desc.isGenericDescriptor()) {
        /* 8.12.9 step 8, no validation required */
    } else if (desc.isDataDescriptor() != sprop->isDataDescriptor()) {
        /* 8.12.9 step 9. */
        if (!sprop->configurable()) {
            return Reject(cx, obj2, current, JSMSG_CANT_REDEFINE_UNCONFIGURABLE_PROP,
                          throwError, desc.id, rval);
        }
    } else if (desc.isDataDescriptor()) {
        /* 8.12.9 step 10. */
        JS_ASSERT(sprop->isDataDescriptor());
        if (!sprop->configurable() && !sprop->writable()) {
            if ((desc.hasWritable && desc.writable()) ||
                (desc.hasValue && !js_SameValue(desc.value, v, cx))) {
                return Reject(cx, obj2, current, JSMSG_CANT_REDEFINE_UNCONFIGURABLE_PROP,
                              throwError, desc.id, rval);
            }
        }
    } else {
        /* 8.12.9 step 11. */
        JS_ASSERT(desc.isAccessorDescriptor() && sprop->isAccessorDescriptor());
        if (!sprop->configurable()) {
            if ((desc.hasSet &&
                 !js_SameValue(desc.setterValue(),
                               sprop->hasSetterValue() ? sprop->setterValue() : JSVAL_VOID,
                               cx)) ||
                (desc.hasGet &&
                 !js_SameValue(desc.getterValue(),
                               sprop->hasGetterValue() ? sprop->getterValue() : JSVAL_VOID,
                               cx)))
            {
                return Reject(cx, obj2, current, JSMSG_CANT_REDEFINE_UNCONFIGURABLE_PROP,
                              throwError, desc.id, rval);
            }
        }
    }

    /* 8.12.9 step 12. */
    uintN attrs;
    JSPropertyOp getter, setter;
    if (desc.isGenericDescriptor()) {
        uintN changed = 0;
        if (desc.hasConfigurable)
            changed |= JSPROP_PERMANENT;
        if (desc.hasEnumerable)
            changed |= JSPROP_ENUMERATE;

        attrs = (sprop->attributes() & ~changed) | (desc.attrs & changed);
        if (sprop->isMethod()) {
            JS_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));
            getter = setter = JS_PropertyStub;
        } else {
            getter = sprop->getter();
            setter = sprop->setter();
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
        attrs = (desc.attrs & ~unchanged) | (sprop->attributes() & unchanged);
        getter = setter = JS_PropertyStub;
    } else {
        JS_ASSERT(desc.isAccessorDescriptor());

        /*
         * Getters and setters are just like watchpoints from an access
         * control point of view.
         */
        jsval dummy;
        if (!CheckAccess(cx, obj2, desc.id, JSACC_WATCH, &dummy, &attrs)) {
             obj2->dropProperty(cx, current);
             return JS_FALSE;
        }

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

        attrs = (desc.attrs & changed) | (sprop->attributes() & ~changed);
        JS_ASSERT_IF(sprop->isMethod(), !(attrs & (JSPROP_GETTER | JSPROP_SETTER)));
        if (desc.hasGet) {
            getter = desc.getter();
        } else {
            getter = (sprop->isMethod() || (sprop->hasDefaultGetter() && !sprop->hasGetterValue()))
                     ? JS_PropertyStub
                     : sprop->getter();
        }
        if (desc.hasSet) {
            setter = desc.setter();
        } else {
            setter = (sprop->hasDefaultSetter() && !sprop->hasSetterValue())
                     ? JS_PropertyStub
                     : sprop->setter();
        }
    }

    *rval = true;
    obj2->dropProperty(cx, current);
    return js_DefineProperty(cx, obj, desc.id, v, getter, setter, attrs);
}

static JSBool
DefinePropertyOnArray(JSContext *cx, JSObject *obj, const PropertyDescriptor &desc,
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

    if (desc.id == ATOM_TO_JSID(cx->runtime->atomState.lengthAtom)) {
        /*
         * Our optimization of storage of the length property of arrays makes
         * it very difficult to properly implement defining the property.  For
         * now simply throw an exception (NB: not merely Reject) on any attempt
         * to define the "length" property, rather than attempting to implement
         * some difficult-for-authors-to-grasp subset of that functionality.
         */
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEFINE_ARRAY_LENGTH_UNSUPPORTED);
        return JS_FALSE;
    }

    uint32 index;
    if (js_IdIsIndex(desc.id, &index)) {
        /*
        // Disabled until we support defining "length":
        if (index >= oldLen && lengthPropertyNotWritable())
            return ThrowTypeError(cx, JSMSG_CANT_APPEND_PROPERTIES_TO_UNWRITABLE_LENGTH_ARRAY);
         */
        if (!DefinePropertyOnObject(cx, obj, desc, false, rval))
            return JS_FALSE;
        if (!*rval)
            return Reject(cx, JSMSG_CANT_DEFINE_ARRAY_INDEX, throwError, rval);

        if (index >= oldLen) {
            JS_ASSERT(index != UINT32_MAX);
            obj->setSlowArrayLength(index + 1);
        }

        *rval = true;
        return JS_TRUE;
    }

    return DefinePropertyOnObject(cx, obj, desc, throwError, rval);
}

static JSBool
DefineProperty(JSContext *cx, JSObject *obj, const PropertyDescriptor &desc, bool throwError,
               bool *rval)
{
    if (obj->isArray())
        return DefinePropertyOnArray(cx, obj, desc, throwError, rval);

    if (obj->map->ops->lookupProperty != js_LookupProperty) {
        if (obj->isProxy())
            return JSProxy::defineProperty(cx, obj, desc.id, desc.pd);
        return Reject(cx, JSMSG_OBJECT_NOT_EXTENSIBLE, throwError, rval);
    }

    return DefinePropertyOnObject(cx, obj, desc, throwError, rval);
}

JSBool
js_DefineOwnProperty(JSContext *cx, JSObject *obj, jsid id, jsval descriptor, JSBool *bp)
{
    AutoDescriptorArray descs(cx);
    PropertyDescriptor *desc = descs.append();
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
obj_defineProperty(JSContext* cx, uintN argc, jsval* vp)
{
    /* 15.2.3.6 steps 1 and 5. */
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.defineProperty", &obj))
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(obj);

    /* 15.2.3.6 step 2. */
    AutoIdRooter nameidr(cx);
    if (!JS_ValueToId(cx, argc >= 2 ? vp[3] : JSVAL_VOID, nameidr.addr()))
        return JS_FALSE;

    /* 15.2.3.6 step 3. */
    jsval descval = argc >= 3 ? vp[4] : JSVAL_VOID;

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

     AutoDescriptorArray descs(cx);
     size_t len = ida.length();
     for (size_t i = 0; i < len; i++) {
         jsid id = ida[i];
         PropertyDescriptor* desc = descs.append();
         AutoValueRooter tvr(cx);
         if (!desc ||
             !JS_GetPropertyById(cx, props, id, tvr.addr()) ||
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
obj_defineProperties(JSContext* cx, uintN argc, jsval* vp)
{
    /* 15.2.3.6 steps 1 and 5. */
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.defineProperties", &obj))
        return false;
    *vp = OBJECT_TO_JSVAL(obj);

    if (argc < 2) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "Object.defineProperties", "0", "s");
        return false;
    }

    JSObject* props = js_ValueToNonNullObject(cx, vp[3]);
    if (!props)
        return false;
    vp[3] = OBJECT_TO_JSVAL(props);

    return DefineProperties(cx, obj, props);
}

/* ES5 15.2.3.5: Object.create(O [, Properties]) */
static JSBool
obj_create(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc == 0) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "Object.create", "0", "s");
        return JS_FALSE;
    }

    jsval v = vp[2];
    if (!JSVAL_IS_OBJECT(v)) {
        char *bytes = js_DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, v, NULL);
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
    JSObject *obj = NewObjectWithGivenProto(cx, &js_ObjectClass, JSVAL_TO_OBJECT(v),
                                            JSVAL_TO_OBJECT(*vp)->getGlobal());
    if (!obj)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(obj); /* Root and prepare for eventual return. */

    /* 15.2.3.5 step 4. */
    if (argc > 1 && vp[3] != JSVAL_VOID) {
        if (JSVAL_IS_PRIMITIVE(vp[3])) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_NONNULL_OBJECT);
            return JS_FALSE;
        }

        JSObject *props = JSVAL_TO_OBJECT(vp[3]);
        AutoIdArray ida(cx, JS_Enumerate(cx, props));
        if (!ida)
            return JS_FALSE;

        AutoDescriptorArray descs(cx);
        size_t len = ida.length();
        for (size_t i = 0; i < len; i++) {
            jsid id = ida[i];
            PropertyDescriptor *desc = descs.append();
            if (!desc || !JS_GetPropertyById(cx, props, id, &vp[1]) ||
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
obj_getOwnPropertyNames(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.getOwnPropertyNames", &obj))
        return false;

    AutoValueVector props(cx);
    if (!GetPropertyNames(cx, obj, JSITER_OWNONLY | JSITER_HIDDEN, props))
        return false;

    for (size_t i = 0, len = props.length(); i < len; i++) {
         jsval v = props[i];
         if (JSVAL_IS_INT(v)) {
             JSString *str = js_ValueToString(cx, v);
             if (!str)
                 return false;
             props[i] = STRING_TO_JSVAL(str);
         }
    }

    JSObject *aobj = js_NewArrayObject(cx, props.length(), props.begin());
    if (!aobj)
        return false;

    *vp = OBJECT_TO_JSVAL(aobj);
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
    JS_FS_END
};

JSBool
js_Object(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
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
        if (JS_IsConstructing(cx))
            return JS_TRUE;
        obj = NewBuiltinClassInstance(cx, &js_ObjectClass);
        if (!obj)
            return JS_FALSE;
    }
    *rval = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

#ifdef JS_TRACER

JSObject*
js_NewObjectWithClassProto(JSContext *cx, JSClass *clasp, JSObject *proto,
                           jsval privateSlotValue)
{
    JS_ASSERT(!clasp->getObjectOps);
    JS_ASSERT(proto->map->ops == &js_ObjectOps);

    JSObject* obj = js_NewGCObject(cx);
    if (!obj)
        return NULL;

    obj->initSharingEmptyScope(clasp, proto, proto->getParent(), privateSlotValue);
    return obj;
}

JSObject* FASTCALL
js_Object_tn(JSContext* cx, JSObject* proto)
{
    JS_ASSERT(!(js_ObjectClass.flags & JSCLASS_HAS_PRIVATE));
    return js_NewObjectWithClassProto(cx, &js_ObjectClass, proto, JSVAL_VOID);
}

JS_DEFINE_TRCINFO_1(js_Object,
    (2, (extern, CONSTRUCTOR_RETRY, js_Object_tn, CONTEXT, CALLEE_PROTOTYPE, 0,
         nanojit::ACC_STORE_ANY)))

JSObject* FASTCALL
js_NonEmptyObject(JSContext* cx, JSObject* proto)
{
    JS_ASSERT(!(js_ObjectClass.flags & JSCLASS_HAS_PRIVATE));
    JSObject *obj = js_NewObjectWithClassProto(cx, &js_ObjectClass, proto, JSVAL_VOID);
    if (!obj)
        return NULL;
    JS_LOCK_OBJ(cx, obj);
    JSScope *scope = js_GetMutableScope(cx, obj);
    if (!scope) {
        JS_UNLOCK_OBJ(cx, obj);
        return NULL;
    }

    /*
     * See comments in the JSOP_NEWINIT case of jsops.cpp why we cannot
     * assume that cx owns the scope and skip the unlock call.
     */
    JS_UNLOCK_SCOPE(cx, scope);
    return obj;
}

JS_DEFINE_CALLINFO_2(extern, CONSTRUCTOR_RETRY, js_NonEmptyObject, CONTEXT, CALLEE_PROTOTYPE, 0,
                     nanojit::ACC_STORE_ANY)

JSObject* FASTCALL
js_NewInstance(JSContext *cx, JSClass *clasp, JSObject *ctor)
{
    JS_ASSERT(JS_ON_TRACE(cx));
    JS_ASSERT(ctor->isFunction());

    JSAtom *atom = cx->runtime->atomState.classPrototypeAtom;

    JSScope *scope = ctor->scope();
#ifdef JS_THREADSAFE
    if (scope->title.ownercx != cx)
        return NULL;
#endif
    if (scope->isSharedEmpty()) {
        scope = js_GetMutableScope(cx, ctor);
        if (!scope)
            return NULL;
    }

    JSScopeProperty *sprop = scope->lookup(ATOM_TO_JSID(atom));
    jsval pval = sprop ? ctor->getSlot(sprop->slot) : JSVAL_HOLE;

    JSObject *parent = ctor->getParent();
    JSObject *proto;
    if (!JSVAL_IS_PRIMITIVE(pval)) {
        /* An object in ctor.prototype, let's use it as the new instance's proto. */
        proto = JSVAL_TO_OBJECT(pval);
    } else {
        /* A hole or a primitive: either way, we need to get Object.prototype. */
        if (!js_GetClassPrototype(cx, parent, JSProto_Object, &proto))
            return NULL;

        if (pval == JSVAL_HOLE) {
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
    return NewObjectWithGivenProto(cx, clasp, proto, parent);
}

JS_DEFINE_CALLINFO_3(extern, CONSTRUCTOR_RETRY, js_NewInstance, CONTEXT, CLASS, OBJECT, 0,
                     nanojit::ACC_STORE_ANY)

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

    script = cx->fp->script;
    endpc = script->code + script->length;
    for (;; pc += js_CodeSpec[op].length) {
        JS_ASSERT_IF(!cx->fp->imacpc, script->code <= pc && pc < endpc);

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
    cs = &js_CodeSpec[js_GetOpcode(cx, fp->script, pc)];
    format = cs->format;
    if (JOF_MODE(format) != JOF_NAME)
        flags |= JSRESOLVE_QUALIFIED;
    if ((format & (JOF_SET | JOF_FOR)) ||
        (fp->flags & JSFRAME_ASSIGNING)) {
        flags |= JSRESOLVE_ASSIGNING;
    } else if (cs->length >= 0) {
        pc += cs->length;
        if (pc < cx->fp->script->code + cx->fp->script->length && Detecting(cx, pc))
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
with_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return obj->getProto()->getProperty(cx, id, vp);
}

static JSBool
with_SetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return obj->getProto()->setProperty(cx, id, vp);
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
with_DeleteProperty(JSContext *cx, JSObject *obj, jsid id, jsval *rval)
{
    return obj->getProto()->deleteProperty(cx, id, rval);
}

static JSBool
with_Enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
               jsval *statep, jsid *idp)
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

JS_FRIEND_DATA(JSObjectOps) js_WithObjectOps = {
    NULL,
    with_LookupProperty,
    js_DefineProperty,
    with_GetProperty,
    with_SetProperty,
    with_GetAttributes,
    with_SetAttributes,
    with_DeleteProperty,
    with_Enumerate,
    with_TypeOf,
    js_TraceObject,
    with_ThisObject,
    NULL,   /* call */
    NULL,   /* construct */
    NULL,   /* hasInstance */
    js_Clear
};

static JSObjectOps *
with_getObjectOps(JSContext *cx, JSClass *clasp)
{
    return &js_WithObjectOps;
}

JSClass js_WithClass = {
    "With",
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(2) | JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   NULL,
    with_getObjectOps,
    0,0,0,0,0,0,0
};

JS_REQUIRES_STACK JSObject *
js_NewWithObject(JSContext *cx, JSObject *proto, JSObject *parent, jsint depth)
{
    JSObject *obj;

    obj = js_NewGCObject(cx);
    if (!obj)
        return NULL;
    obj->init(&js_WithClass, proto, parent,
              reinterpret_cast<jsval>(js_FloatingFrameIfGenerator(cx, cx->fp)));
    OBJ_SET_BLOCK_DEPTH(cx, obj, depth);
    obj->map = cx->runtime->emptyWithScope->hold();

    AutoObjectRooter tvr(cx, obj);
    JSObject *thisp = proto->thisObject(cx);
    if (!thisp)
        return NULL;
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
    blockObj->init(&js_BlockClass, NULL, NULL, JSVAL_NULL);
    blockObj->map = cx->runtime->emptyBlockScope->hold();
    return blockObj;
}

JSObject *
js_CloneBlockObject(JSContext *cx, JSObject *proto, JSStackFrame *fp)
{
    JS_ASSERT(!OBJ_IS_CLONED_BLOCK(proto));
    JS_ASSERT(proto->getClass() == &js_BlockClass);

    JSObject *clone = js_NewGCObject(cx);
    if (!clone)
        return NULL;

    /* The caller sets parent on its own. */
    jsval priv = (jsval)js_FloatingFrameIfGenerator(cx, fp);
    clone->init(&js_BlockClass, proto, NULL, priv);
    clone->fslots[JSSLOT_BLOCK_DEPTH] = proto->fslots[JSSLOT_BLOCK_DEPTH];

    JS_ASSERT(cx->runtime->emptyBlockScope->freeslot == JSSLOT_BLOCK_DEPTH + 1);
    clone->map = cx->runtime->emptyBlockScope->hold();
    JS_ASSERT(OBJ_IS_CLONED_BLOCK(clone));

    if (!js_EnsureReservedSlots(cx, clone, OBJ_BLOCK_COUNT(cx, proto)))
        return NULL;
    return clone;
}

JS_REQUIRES_STACK JSBool
js_PutBlockObject(JSContext *cx, JSBool normalUnwind)
{
    /* Blocks have one fixed slot available for the first local.*/
    JS_STATIC_ASSERT(JS_INITIAL_NSLOTS == JSSLOT_BLOCK_DEPTH + 2);

    JSStackFrame *const fp = cx->fp;
    JSObject *obj = fp->scopeChain;
    JS_ASSERT(obj->getClass() == &js_BlockClass);
    JS_ASSERT(obj->getPrivate() == js_FloatingFrameIfGenerator(cx, cx->fp));
    JS_ASSERT(OBJ_IS_CLONED_BLOCK(obj));

    /*
     * Block objects should never be exposed to scripts. Thus the clone should
     * not own the property map and rather always share it with the prototype
     * object. This allows us to skip updating obj->scope()->freeslot after
     * we copy the stack slots into reserved slots.
     */
    JS_ASSERT(obj->scope()->object != obj);

    /* Block objects should have all reserved slots allocated early. */
    uintN count = OBJ_BLOCK_COUNT(cx, obj);
    JS_ASSERT(obj->numSlots() == JSSLOT_BLOCK_DEPTH + 1 + count);

    /* The block and its locals must be on the current stack for GC safety. */
    uintN depth = OBJ_BLOCK_DEPTH(cx, obj);
    JS_ASSERT(depth <= (size_t) (cx->regs->sp - StackBase(fp)));
    JS_ASSERT(count <= (size_t) (cx->regs->sp - StackBase(fp) - depth));

    /* See comments in CheckDestructuring from jsparse.cpp. */
    JS_ASSERT(count >= 1);

    depth += fp->script->nfixed;
    obj->fslots[JSSLOT_BLOCK_DEPTH + 1] = fp->slots()[depth];
    if (normalUnwind && count > 1) {
        --count;
        memcpy(obj->dslots, fp->slots() + depth + 1, count * sizeof(jsval));
    }

    /* We must clear the private slot even with errors. */
    obj->setPrivate(NULL);
    fp->scopeChain = obj->getParent();
    return normalUnwind;
}

static JSBool
block_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    /*
     * Block objects are never exposed to script, and the engine handles them
     * with care. So unlike other getters, this one can assert (rather than
     * check) certain invariants about obj.
     */
    JS_ASSERT(obj->getClass() == &js_BlockClass);
    JS_ASSERT(OBJ_IS_CLONED_BLOCK(obj));
    uintN index = (uintN) JSVAL_TO_INT(id);
    JS_ASSERT(index < OBJ_BLOCK_COUNT(cx, obj));

    JSStackFrame *fp = (JSStackFrame *) obj->getPrivate();
    if (fp) {
        fp = js_LiveFrameIfGenerator(fp);
        index += fp->script->nfixed + OBJ_BLOCK_DEPTH(cx, obj);
        JS_ASSERT(index < fp->script->nslots);
        *vp = fp->slots()[index];
        return true;
    }

    /* Values are in reserved slots immediately following DEPTH. */
    uint32 slot = JSSLOT_BLOCK_DEPTH + 1 + index;
    JS_LOCK_OBJ(cx, obj);
    JS_ASSERT(slot < obj->numSlots());
    *vp = obj->getSlot(slot);
    JS_UNLOCK_OBJ(cx, obj);
    return true;
}

static JSBool
block_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JS_ASSERT(obj->getClass() == &js_BlockClass);
    JS_ASSERT(OBJ_IS_CLONED_BLOCK(obj));
    uintN index = (uintN) JSVAL_TO_INT(id);
    JS_ASSERT(index < OBJ_BLOCK_COUNT(cx, obj));

    JSStackFrame *fp = (JSStackFrame *) obj->getPrivate();
    if (fp) {
        fp = js_LiveFrameIfGenerator(fp);
        index += fp->script->nfixed + OBJ_BLOCK_DEPTH(cx, obj);
        JS_ASSERT(index < fp->script->nslots);
        fp->slots()[index] = *vp;
        return true;
    }

    /* Values are in reserved slots immediately following DEPTH. */
    uint32 slot = JSSLOT_BLOCK_DEPTH + 1 + index;
    JS_LOCK_OBJ(cx, obj);
    JS_ASSERT(slot < obj->numSlots());
    obj->setSlot(slot, *vp);
    JS_UNLOCK_OBJ(cx, obj);
    return true;
}

JSBool
js_DefineBlockVariable(JSContext *cx, JSObject *obj, jsid id, intN index)
{
    JS_ASSERT(obj->getClass() == &js_BlockClass);
    JS_ASSERT(!OBJ_IS_CLONED_BLOCK(obj));

    /* Use JSPROP_ENUMERATE to aid the disassembler. */
    return js_DefineNativeProperty(cx, obj, id, JSVAL_VOID,
                                   block_getProperty, block_setProperty,
                                   JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED,
                                   JSScopeProperty::HAS_SHORTID, index, NULL);
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
    /* For both objects determine whether they own their respective scopes. */
    bool thisOwns = this->isNative() && scope()->object == this;
    bool otherOwns = other->isNative() && other->scope()->object == other;

    size_t size = GetObjectSize(this);
    JS_ASSERT(size == GetObjectSize(other));

    /* Trade the guts of the objects. */
    union {
        JSFunction fun;
        JSObject obj;
    } tmp;
    memcpy(&tmp, this, size);
    memcpy(this, other, size);
    memcpy(other, &tmp, size);

    /* Fixup scope ownerships. */
    if (otherOwns)
        scope()->object = this;
    if (thisOwns)
        other->scope()->object = other;
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
    uint16 depth, count, i;
    uint32 tmp;
    JSScopeProperty *sprop;
    jsid propid;
    JSAtom *atom;
    int16 shortid;
    JSBool ok;

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
        tmp = (uint32)(depth << 16) | count;
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

    AutoValueRooter tvr(cx, obj);

    if (!JS_XDRUint32(xdr, &tmp))
        return false;

    if (xdr->mode == JSXDR_DECODE) {
        depth = (uint16)(tmp >> 16);
        count = (uint16)tmp;
        obj->setSlot(JSSLOT_BLOCK_DEPTH, INT_TO_JSVAL(depth));
    }

    /*
     * XDR the block object's properties. We know that there are 'count'
     * properties to XDR, stored as id/shortid pairs. We do not XDR any
     * non-native properties, only those that the compiler created.
     */
    sprop = NULL;
    ok = JS_TRUE;
    for (i = 0; i < count; i++) {
        if (xdr->mode == JSXDR_ENCODE) {
            /* Find a property to XDR. */
            do {
                /* If sprop is NULL, this is the first property. */
                sprop = sprop ? sprop->parent : obj->scope()->lastProperty();
            } while (!sprop->hasShortID());

            JS_ASSERT(sprop->getter() == block_getProperty);
            propid = sprop->id;
            JS_ASSERT(JSID_IS_ATOM(propid));
            atom = JSID_TO_ATOM(propid);
            shortid = sprop->shortid;
            JS_ASSERT(shortid >= 0);
        }

        /* XDR the real id, then the shortid. */
        if (!js_XDRStringAtom(xdr, &atom) ||
            !JS_XDRUint16(xdr, (uint16 *)&shortid)) {
            return false;
        }

        if (xdr->mode == JSXDR_DECODE) {
            if (!js_DefineBlockVariable(cx, obj, ATOM_TO_JSID(atom), shortid))
                return false;
        }
    }
    return true;
}

#endif

JSClass js_BlockClass = {
    "Block",
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1) | JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,   JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,    NULL,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *
js_InitObjectClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto = js_InitClass(cx, obj, NULL, &js_ObjectClass, js_Object, 1,
                                   object_props, object_methods, NULL, object_static_methods);
    if (!proto)
        return NULL;

    /* ECMA (15.1.2.1) says 'eval' is a property of the global object. */
    if (!js_DefineFunction(cx, obj, cx->runtime->atomState.evalAtom,
                           (JSNative)obj_eval, 1,
                           JSFUN_FAST_NATIVE | JSFUN_STUB_GSOPS)) {
        return NULL;
    }

    return proto;
}

static bool
DefineStandardSlot(JSContext *cx, JSObject *obj, JSProtoKey key, JSAtom *atom,
                   jsval v, uint32 attrs, bool &named)
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

        JSScope *scope = js_GetMutableScope(cx, obj);
        if (!scope) {
            JS_UNLOCK_OBJ(cx, obj);
            return false;
        }

        JSScopeProperty *sprop = scope->lookup(id);
        if (!sprop) {
            uint32 index = 2 * JSProto_LIMIT + key;
            if (!js_SetReservedSlot(cx, obj, index, v)) {
                JS_UNLOCK_SCOPE(cx, scope);
                return false;
            }

            uint32 slot = JSSLOT_START(obj->getClass()) + index;
            sprop = scope->addProperty(cx, id, JS_PropertyStub, JS_PropertyStub,
                                       slot, attrs, 0, 0);

            JS_UNLOCK_SCOPE(cx, scope);
            if (!sprop)
                return false;

            named = true;
            return true;
        }
        JS_UNLOCK_SCOPE(cx, scope);
    }

    named = obj->defineProperty(cx, id, v, JS_PropertyStub, JS_PropertyStub, attrs);
    return named;
}

JSObject *
js_InitClass(JSContext *cx, JSObject *obj, JSObject *parent_proto,
             JSClass *clasp, JSNative constructor, uintN nargs,
             JSPropertySpec *ps, JSFunctionSpec *fs,
             JSPropertySpec *static_ps, JSFunctionSpec *static_fs)
{
    JSAtom *atom;
    JSProtoKey key;
    JSObject *proto, *ctor;
    jsval cval, rval;
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
    proto = NewObject(cx, clasp, parent_proto, obj);
    if (!proto)
        return NULL;

    /* After this point, control must exit via label bad or out. */
    AutoValueRooter tvr(cx, proto);

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
            if (!DefineStandardSlot(cx, obj, key, atom, tvr.value(), attrs, named))
                goto bad;
        }

        ctor = proto;
    } else {
        fun = js_NewFunction(cx, NULL, constructor, nargs, 0, obj, atom);
        if (!fun)
            goto bad;

        AutoValueRooter tvr2(cx, OBJECT_TO_JSVAL(fun));
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
            cval = OBJECT_TO_JSVAL(ctor);
            if (!js_InternalConstruct(cx, proto, cval, 0, NULL, &rval))
                goto bad;
            if (!JSVAL_IS_PRIMITIVE(rval) && JSVAL_TO_OBJECT(rval) != proto)
                proto = JSVAL_TO_OBJECT(rval);
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
     * Make sure proto's scope's emptyScope is available to be shared by
     * objects of this class.  JSScope::emptyScope is a one-slot cache. If we
     * omit this, some other class could snap it up. (The risk is particularly
     * great for Object.prototype.)
     *
     * All callers of JSObject::initSharingEmptyScope depend on this.
     */
    if (!proto->scope()->ensureEmptyScope(cx, clasp))
        goto bad;

    /* If this is a standard class, cache its prototype. */
    if (key != JSProto_Null && !js_SetClassObject(cx, obj, key, ctor, proto))
        goto bad;

out:
    return proto;

bad:
    if (named)
        (void) obj->deleteProperty(cx, ATOM_TO_JSID(atom), &rval);
    proto = NULL;
    goto out;
}

bool
JSObject::allocSlots(JSContext *cx, size_t nslots)
{
    JS_ASSERT(!dslots);
    JS_ASSERT(nslots > JS_INITIAL_NSLOTS);

    size_t nwords = slotsToDynamicWords(nslots);
    dslots = (jsval*) cx->malloc(nwords * sizeof(jsval));
    if (!dslots)
        return false;

    dslots++;
    dslots[-1] = nslots;
    /* clear the newly allocated cells. */
    for (jsuint i = JS_INITIAL_NSLOTS; i < nslots; i++)
        dslots[i - JS_INITIAL_NSLOTS] = JSVAL_VOID;

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
        return JS_TRUE;

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

    size_t oldnslots = size_t(dslots[-1]);

    jsval *tmpdslots = (jsval*) cx->realloc(dslots - 1, nwords * sizeof(jsval));
    if (!tmpdslots)
        return false;   /* Leave dslots at its old size. */
    dslots = tmpdslots;

    dslots++;
    dslots[-1] = nslots;

    /* Initialize the additional slots we added. */
    JS_ASSERT(nslots > oldnslots);
    for (size_t i = oldnslots; i < nslots; i++)
        dslots[i - JS_INITIAL_NSLOTS] = JSVAL_VOID;

    return true;
}

void
JSObject::shrinkSlots(JSContext *cx, size_t nslots)
{
    /* Nothing to shrink? */
    if (!dslots)
        return;

    JS_ASSERT(size_t(dslots[-1]) > JS_INITIAL_NSLOTS);
    JS_ASSERT(nslots <= size_t(dslots[-1]));

    if (nslots <= JS_INITIAL_NSLOTS) {
        freeSlotsArray(cx);
        dslots = NULL;
    } else {
        size_t nwords = slotsToDynamicWords(nslots);
        jsval *tmpdslots = (jsval*) cx->realloc(dslots - 1, nwords * sizeof(jsval));
        if (!tmpdslots)
            return;  /* Leave dslots at its old size. */
        dslots = tmpdslots;

        dslots++;
        dslots[-1] = nslots;
    }
}

bool
js_EnsureReservedSlots(JSContext *cx, JSObject *obj, size_t nreserved)
{
    uintN nslots = JSSLOT_FREE(obj->getClass()) + nreserved;
    if (nslots > obj->numSlots() && !obj->allocSlots(cx, nslots))
        return false;

    if (obj->isNative()) {
        JSScope *scope = obj->scope();
        if (!scope->isSharedEmpty()) {
#ifdef JS_THREADSAFE
            JS_ASSERT(scope->title.ownercx->thread == cx->thread);
#endif
            JS_ASSERT(scope->freeslot == JSSLOT_FREE(obj->getClass()));
            if (scope->freeslot < nslots)
                scope->freeslot = nslots;
        }
    }
    return true;
}

JS_BEGIN_EXTERN_C

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

JS_END_EXTERN_C

namespace js {

bool
SetProto(JSContext *cx, JSObject *obj, JSObject *proto, bool checkForCycles)
{
    JS_ASSERT_IF(!checkForCycles, obj != proto);

    if (obj->isNative()) {
        JS_LOCK_OBJ(cx, obj);
        bool ok = !!js_GetMutableScope(cx, obj);
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
        JSScope *scope = oldproto->scope();
        scope->protoShapeChange(cx);
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
    jsval v;

    while ((tmp = obj->getParent()) != NULL)
        obj = tmp;
    if (!(obj->getClass()->flags & JSCLASS_IS_GLOBAL)) {
        *objp = NULL;
        return JS_TRUE;
    }

    v = obj->getReservedSlot(key);
    if (!JSVAL_IS_PRIMITIVE(v)) {
        *objp = JSVAL_TO_OBJECT(v);
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
            if (!JSVAL_IS_PRIMITIVE(v))
                cobj = JSVAL_TO_OBJECT(v);
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

    return js_SetReservedSlot(cx, obj, key, OBJECT_TO_JSVAL(cobj)) &&
           js_SetReservedSlot(cx, obj, JSProto_LIMIT + key, OBJECT_TO_JSVAL(proto));
}

JSBool
js_FindClassObject(JSContext *cx, JSObject *start, JSProtoKey protoKey,
                   jsval *vp, JSClass *clasp)
{
    JSStackFrame *fp;
    JSObject *obj, *cobj, *pobj;
    jsid id;
    JSProperty *prop;
    jsval v;
    JSScopeProperty *sprop;

    /*
     * Find the global object. Use cx->fp directly to avoid falling off
     * trace; all JIT-elided stack frames have the same global object as
     * cx->fp.
     */
    VOUCH_DOES_NOT_REQUIRE_STACK();
    if (!start && (fp = cx->fp) != NULL)
        start = fp->scopeChain;

    if (start) {
        /* Find the topmost object in the scope chain. */
        do {
            obj = start;
            start = obj->getParent();
        } while (start);
    } else {
        obj = cx->globalObject;
        if (!obj) {
            *vp = JSVAL_VOID;
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
            *vp = OBJECT_TO_JSVAL(cobj);
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
    v = JSVAL_VOID;
    if (prop && pobj->isNative()) {
        sprop = (JSScopeProperty *) prop;
        if (SPROP_HAS_VALID_SLOT(sprop, pobj->scope())) {
            v = pobj->lockedGetSlot(sprop->slot);
            if (JSVAL_IS_PRIMITIVE(v))
                v = JSVAL_VOID;
        }
        JS_UNLOCK_OBJ(cx, pobj);
    }
    *vp = v;
    return JS_TRUE;
}

JSObject *
js_ConstructObject(JSContext *cx, JSClass *clasp, JSObject *proto,
                   JSObject *parent, uintN argc, jsval *argv)
{
    jsval cval, rval;
    JSObject *obj, *ctor;

    AutoArrayRooter argtvr(cx, argc, argv);

    JSProtoKey protoKey = GetClassProtoKey(clasp);
    if (!js_FindClassObject(cx, parent, protoKey, &cval, clasp))
        return NULL;

    if (JSVAL_IS_PRIMITIVE(cval)) {
        js_ReportIsNotFunction(cx, &cval, JSV2F_CONSTRUCT | JSV2F_SEARCH_STACK);
        return NULL;
    }

    /* Protect cval in case a crazy getter for .prototype uproots it. */
    AutoValueRooter tvr(cx, cval);

    /*
     * If proto is NULL, set it to Constructor.prototype, just like JSOP_NEW
     * does, likewise for the new object's parent.
     */
    ctor = JSVAL_TO_OBJECT(cval);
    if (!parent)
        parent = ctor->getParent();
    if (!proto) {
        if (!ctor->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom),
                               &rval)) {
            return NULL;
        }
        if (JSVAL_IS_OBJECT(rval))
            proto = JSVAL_TO_OBJECT(rval);
    }

    obj = NewObject(cx, clasp, proto, parent);
    if (!obj)
        return NULL;

    if (!js_InternalConstruct(cx, obj, cval, argc, argv, &rval))
        return NULL;

    if (JSVAL_IS_PRIMITIVE(rval))
        return obj;

    /*
     * If the instance's class differs from what was requested, throw a type
     * error.  If the given class has both the JSCLASS_HAS_PRIVATE and the
     * JSCLASS_CONSTRUCT_PROTOTYPE flags, and the instance does not have its
     * private data set at this point, then the constructor was replaced and
     * we should throw a type error.
     */
    obj = JSVAL_TO_OBJECT(rval);
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

/*
 * FIXME bug 535629: If one adds props, deletes earlier props, adds more, the
 * last added won't recycle the deleted props' slots.
 */
JSBool
js_AllocSlot(JSContext *cx, JSObject *obj, uint32 *slotp)
{
    JSScope *scope = obj->scope();
    JS_ASSERT(scope->object == obj);

    if (scope->freeslot >= obj->numSlots() &&
        !obj->growSlots(cx, scope->freeslot + 1)) {
        return JS_FALSE;
    }

    /* js_ReallocSlots or js_FreeSlot should set the free slots to void. */
    JS_ASSERT(JSVAL_IS_VOID(obj->getSlot(scope->freeslot)));
    *slotp = scope->freeslot++;
    return JS_TRUE;
}

void
js_FreeSlot(JSContext *cx, JSObject *obj, uint32 slot)
{
    JSScope *scope = obj->scope();
    JS_ASSERT(scope->object == obj);
    obj->lockedSetSlot(slot, JSVAL_VOID);
    if (scope->freeslot == slot + 1)
        scope->freeslot = slot;
}


/* JSVAL_INT_MAX as a string */
#define JSVAL_INT_MAX_STRING "1073741823"

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
    if (n > sizeof(JSVAL_INT_MAX_STRING) - 1)
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
     * index "-0" from "0", because JSVAL_INT cannot.
     */
    if (cp != end || (negative && index == 0))
        return id;

    if (oldIndex < JSVAL_INT_MAX / 10 ||
        (oldIndex == JSVAL_INT_MAX / 10 && c <= (JSVAL_INT_MAX % 10))) {
        if (negative)
            index = 0 - index;
        id = INT_TO_JSID((jsint)index);
    }

    return id;
}

static JSBool
PurgeProtoChain(JSContext *cx, JSObject *obj, jsid id)
{
    JSScope *scope;
    JSScopeProperty *sprop;

    while (obj) {
        if (!obj->isNative()) {
            obj = obj->getProto();
            continue;
        }
        JS_LOCK_OBJ(cx, obj);
        scope = obj->scope();
        sprop = scope->lookup(id);
        if (sprop) {
            PCMETER(JS_PROPERTY_CACHE(cx).pcpurges++);
            scope->shadowingShapeChange(cx, sprop);
            JS_UNLOCK_SCOPE(cx, scope);

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
        obj = obj->getProto();
        JS_UNLOCK_SCOPE(cx, scope);
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
    if (obj->getClass() == &js_CallClass) {
        while ((obj = obj->getParent()) != NULL) {
            if (PurgeProtoChain(cx, obj, id))
                break;
        }
    }
}

JSScopeProperty *
js_AddNativeProperty(JSContext *cx, JSObject *obj, jsid id,
                     JSPropertyOp getter, JSPropertyOp setter, uint32 slot,
                     uintN attrs, uintN flags, intN shortid)
{
    JSScope *scope;
    JSScopeProperty *sprop;

    JS_ASSERT(!(flags & JSScopeProperty::METHOD));

    /*
     * Purge the property cache of now-shadowed id in obj's scope chain. Do
     * this optimistically (assuming no failure below) before locking obj, so
     * we can lock the shadowed scope.
     */
    js_PurgeScopeChain(cx, obj, id);

    JS_LOCK_OBJ(cx, obj);
    scope = js_GetMutableScope(cx, obj);
    if (!scope) {
        sprop = NULL;
    } else {
        /* Convert string indices to integers if appropriate. */
        id = js_CheckForStringIndex(id);
        sprop = scope->putProperty(cx, id, getter, setter, slot, attrs, flags, shortid);
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
        sprop = scope->changeProperty(cx, sprop, attrs, mask, getter, setter);
    }
    JS_UNLOCK_OBJ(cx, obj);
    return sprop;
}

JSBool
js_DefineProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                  JSPropertyOp getter, JSPropertyOp setter, uintN attrs)
{
    return js_DefineNativeProperty(cx, obj, id, value, getter, setter, attrs,
                                   0, 0, NULL);
}

/*
 * Backward compatibility requires allowing addProperty hooks to mutate the
 * nominal initial value of a slot-full property, while GC safety wants that
 * value to be stored before the call-out through the hook.  Optimize to do
 * both while saving cycles for classes that stub their addProperty hook.
 */
static inline bool
AddPropertyHelper(JSContext *cx, JSClass *clasp, JSObject *obj, JSScope *scope,
                  JSScopeProperty *sprop, jsval *vp)
{
    if (clasp->addProperty != JS_PropertyStub) {
        jsval nominal = *vp;

        if (!callJSPropertyOp(cx, clasp->addProperty, obj, SPROP_USERID(sprop), vp))
            return false;
        if (*vp != nominal) {
            if (SPROP_HAS_VALID_SLOT(sprop, scope))
                obj->lockedSetSlot(sprop->slot, *vp);
        }
    }
    return true;
}

JSBool
js_DefineNativeProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                        JSPropertyOp getter, JSPropertyOp setter, uintN attrs,
                        uintN flags, intN shortid, JSProperty **propp,
                        uintN defineHow /* = 0 */)
{
    JSClass *clasp;
    JSScope *scope;
    JSScopeProperty *sprop;
    JSBool added;

    JS_ASSERT((defineHow & ~(JSDNP_CACHE_RESULT | JSDNP_DONT_PURGE | JSDNP_SET_METHOD)) == 0);
    LeaveTraceIfGlobalObject(cx, obj);

    /* Convert string indices to integers if appropriate. */
    id = js_CheckForStringIndex(id);

    /*
     * If defining a getter or setter, we must check for its counterpart and
     * update the attributes and property ops.  A getter or setter is really
     * only half of a property.
     */
    sprop = NULL;
    if (attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
        JSObject *pobj;
        JSProperty *prop;

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
        if (sprop && pobj == obj && sprop->isAccessorDescriptor()) {
            sprop = obj->scope()->changeProperty(cx, sprop, attrs,
                                                 JSPROP_GETTER | JSPROP_SETTER,
                                                 (attrs & JSPROP_GETTER)
                                                 ? getter
                                                 : sprop->getter(),
                                                 (attrs & JSPROP_SETTER)
                                                 ? setter
                                                 : sprop->setter());

            /* NB: obj == pobj, so we can share unlock code at the bottom. */
            if (!sprop)
                goto error;
        } else if (prop) {
            pobj->dropProperty(cx, prop);
            prop = NULL;
            sprop = NULL;
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
    scope = js_GetMutableScope(cx, obj);
    if (!scope)
        goto error;

    added = false;
    if (!sprop) {
        /* Add a new property, or replace an existing one of the same id. */
        if (defineHow & JSDNP_SET_METHOD) {
            JS_ASSERT(clasp == &js_ObjectClass);
            JS_ASSERT(VALUE_IS_FUNCTION(cx, value));
            JS_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));
            JS_ASSERT(!getter && !setter);

            JSObject *funobj = JSVAL_TO_OBJECT(value);
            if (FUN_OBJECT(GET_FUNCTION_PRIVATE(cx, funobj)) == funobj) {
                flags |= JSScopeProperty::METHOD;
                getter = CastAsPropertyOp(funobj);
            }
        }

        added = !scope->hasProperty(id);
        uint32 oldShape = scope->shape;
        sprop = scope->putProperty(cx, id, getter, setter, SPROP_INVALID_SLOT,
                                   attrs, flags, shortid);
        if (!sprop)
            goto error;

        /*
         * If sprop is a method, the above call to putProperty suffices to
         * update the shape if necessary. But if scope->branded(), the shape
         * may not have changed and we may be overwriting a function-valued
         * property. See bug 560998.
         */
        if (scope->shape == oldShape && scope->branded() && sprop->slot != SPROP_INVALID_SLOT)
            scope->methodWriteBarrier(cx, sprop->slot, value);
    }

    /* Store value before calling addProperty, in case the latter GC's. */
    if (SPROP_HAS_VALID_SLOT(sprop, scope))
        obj->lockedSetSlot(sprop->slot, value);

    /* XXXbe called with lock held */
    if (!AddPropertyHelper(cx, clasp, obj, scope, sprop, &value)) {
        scope->removeProperty(cx, id);
        goto error;
    }

    if (defineHow & JSDNP_CACHE_RESULT) {
        JS_ASSERT_NOT_ON_TRACE(cx);
        PropertyCacheEntry *entry =
            JS_PROPERTY_CACHE(cx).fill(cx, obj, 0, 0, obj, sprop, added);
        TRACE_2(SetPropHit, entry, sprop);
    }
    if (propp)
        *propp = (JSProperty *) sprop;
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
    JSClass *clasp = obj->getClass();
    JSResolveOp resolve = clasp->resolve;
    JSScope *scope = obj->scope();

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
    JSScopeProperty *sprop = NULL;
    if (clasp->flags & JSCLASS_NEW_RESOLVE) {
        JSNewResolveOp newresolve = (JSNewResolveOp)resolve;
        if (flags == JSRESOLVE_INFER)
            flags = js_InferFlags(cx, flags);
        JSObject *obj2 = (clasp->flags & JSCLASS_NEW_RESOLVE_GETS_START) ? start : NULL;
        JS_UNLOCK_OBJ(cx, obj);

        {
            /* Protect id and all atoms from a GC nested in resolve. */
            AutoKeepAtoms keep(cx->runtime);
            ok = newresolve(cx, obj, ID_TO_VALUE(id), flags, &obj2);
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
                 * Require that obj2 have its own scope now, as we
                 * do for old-style resolve.  If it doesn't, then
                 * id was not truly resolved, and we'll find it in
                 * the proto chain, or miss it if obj2's proto is
                 * not on obj's proto chain.  That last case is a
                 * "too bad!" case.
                 */
                scope = obj2->scope();
                if (!scope->isSharedEmpty())
                    sprop = scope->lookup(id);
            }
            if (sprop) {
                JS_ASSERT(scope == obj2->scope());
                JS_ASSERT(!scope->isSharedEmpty());
                obj = obj2;
            } else if (obj2 != obj) {
                if (obj2->isNative())
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
        JS_ASSERT(obj->isNative());
        scope = obj->scope();
        if (!scope->isSharedEmpty())
            sprop = scope->lookup(id);
    }

cleanup:
    if (ok && sprop) {
        JS_ASSERT(obj->scope() == scope);
        *objp = obj;
        *propp = (JSProperty *) sprop;
    }
    js_StopResolving(cx, &key, JSRESFLAG_LOOKUP, entry, generation);
    return ok;
}

int
js_LookupPropertyWithFlags(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                           JSObject **objp, JSProperty **propp)
{
    /* Convert string indices to integers if appropriate. */
    id = js_CheckForStringIndex(id);

    /* Search scopes starting with obj and following the prototype link. */
    JSObject *start = obj;
    int protoIndex;
    for (protoIndex = 0; ; protoIndex++) {
        JS_LOCK_OBJ(cx, obj);
        JSScopeProperty *sprop = obj->scope()->lookup(id);
        if (sprop) {
            SCOPE_DEPTH_ACCUM(&cx->runtime->protoLookupDepthStats, protoIndex);
            *objp = obj;
            *propp = (JSProperty *) sprop;
            return protoIndex;
        }

        /* Try obj's class resolve hook if id was not found in obj's scope. */
        if (!sprop && obj->getClass()->resolve != JS_ResolveStub) {
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

PropertyCacheEntry *
js_FindPropertyHelper(JSContext *cx, jsid id, JSBool cacheResult,
                      JSObject **objp, JSObject **pobjp, JSProperty **propp)
{
    JSObject *scopeChain, *obj, *parent, *pobj;
    PropertyCacheEntry *entry;
    int scopeIndex, protoIndex;
    JSProperty *prop;

    JS_ASSERT_IF(cacheResult, !JS_ON_TRACE(cx));
    scopeChain = js_GetTopStackFrame(cx)->scopeChain;

    /* Scan entries on the scope chain that we can cache across. */
    entry = JS_NO_PROP_CACHE_FILL;
    obj = scopeChain;
    parent = obj->getParent();
    for (scopeIndex = 0;
         parent
         ? js_IsCacheableNonGlobalScope(obj)
         : obj->map->ops->lookupProperty == js_LookupProperty;
         ++scopeIndex) {
        protoIndex =
            js_LookupPropertyWithFlags(cx, obj, id, cx->resolveFlags,
                                       &pobj, &prop);
        if (protoIndex < 0)
            return NULL;

        if (prop) {
#ifdef DEBUG
            if (parent) {
                JSClass *clasp = obj->getClass();
                JS_ASSERT(pobj->isNative());
                JS_ASSERT(pobj->getClass() == clasp);
                if (clasp == &js_BlockClass) {
                    /*
                     * A block instance on the scope chain is immutable and
                     * the compile-time prototype provides all its properties.
                     */
                    JS_ASSERT(pobj == obj->getProto());
                    JS_ASSERT(protoIndex == 1);
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
                                                   (JSScopeProperty *) prop);
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
                                           (JSScopeProperty *) prop);
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

JSBool
js_NativeGet(JSContext *cx, JSObject *obj, JSObject *pobj,
             JSScopeProperty *sprop, uintN getHow, jsval *vp)
{
    LeaveTraceIfGlobalObject(cx, pobj);

    JSScope *scope;
    uint32 slot;
    int32 sample;

    JS_ASSERT(pobj->isNative());
    JS_ASSERT(JS_IS_OBJ_LOCKED(cx, pobj));
    scope = pobj->scope();

    slot = sprop->slot;
    *vp = (slot != SPROP_INVALID_SLOT)
          ? pobj->lockedGetSlot(slot)
          : JSVAL_VOID;
    if (sprop->hasDefaultGetter())
        return true;

    if (JS_UNLIKELY(sprop->isMethod()) && (getHow & JSGET_NO_METHOD_BARRIER)) {
        JS_ASSERT(sprop->methodValue() == *vp);
        return true;
    }

    sample = cx->runtime->propertyRemovals;
    JS_UNLOCK_SCOPE(cx, scope);
    {
        AutoScopePropertyRooter tvr(cx, sprop);
        AutoValueRooter tvr2(cx, pobj);
        if (!sprop->get(cx, obj, pobj, vp))
            return false;
    }
    JS_LOCK_SCOPE(cx, scope);

    if (SLOT_IN_SCOPE(slot, scope) &&
        (JS_LIKELY(cx->runtime->propertyRemovals == sample) ||
         scope->hasProperty(sprop))) {
        jsval v = *vp;
        if (!scope->methodWriteBarrier(cx, sprop, v)) {
            JS_UNLOCK_SCOPE(cx, scope);
            return false;
        }
        pobj->lockedSetSlot(slot, v);
    }

    return true;
}

JSBool
js_NativeSet(JSContext *cx, JSObject *obj, JSScopeProperty *sprop, bool added,
             jsval *vp)
{
    LeaveTraceIfGlobalObject(cx, obj);

    JSScope *scope;
    uint32 slot;
    int32 sample;

    JS_ASSERT(obj->isNative());
    JS_ASSERT(JS_IS_OBJ_LOCKED(cx, obj));
    scope = obj->scope();

    slot = sprop->slot;
    if (slot != SPROP_INVALID_SLOT) {
        OBJ_CHECK_SLOT(obj, slot);

        /* If sprop has a stub setter, keep scope locked and just store *vp. */
        if (sprop->hasDefaultSetter()) {
            if (!added && !scope->methodWriteBarrier(cx, sprop, *vp)) {
                JS_UNLOCK_SCOPE(cx, scope);
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
        if (!sprop->hasGetterValue() && sprop->hasDefaultSetter())
            return js_ReportGetterOnlyAssignment(cx);
    }

    sample = cx->runtime->propertyRemovals;
    JS_UNLOCK_SCOPE(cx, scope);
    {
        AutoScopePropertyRooter tvr(cx, sprop);
        if (!sprop->set(cx, obj, vp))
            return false;
    }

    JS_LOCK_SCOPE(cx, scope);
    if (SLOT_IN_SCOPE(slot, scope) &&
        (JS_LIKELY(cx->runtime->propertyRemovals == sample) ||
         scope->hasProperty(sprop))) {
        jsval v = *vp;
        if (!added && !scope->methodWriteBarrier(cx, sprop, v)) {
            JS_UNLOCK_SCOPE(cx, scope);
            return false;
        }
        obj->lockedSetSlot(slot, v);
    }

    return true;
}

JSBool
js_GetPropertyHelper(JSContext *cx, JSObject *obj, jsid id, uintN getHow,
                     jsval *vp)
{
    JSObject *aobj, *obj2;
    int protoIndex;
    JSProperty *prop;
    JSScopeProperty *sprop;

    JS_ASSERT_IF(getHow & JSGET_CACHE_RESULT, !JS_ON_TRACE(cx));

    /* Convert string indices to integers if appropriate. */
    id = js_CheckForStringIndex(id);

    aobj = js_GetProtoIfDenseArray(obj);
    protoIndex = js_LookupPropertyWithFlags(cx, aobj, id, cx->resolveFlags,
                                            &obj2, &prop);
    if (protoIndex < 0)
        return JS_FALSE;
    if (!prop) {
        *vp = JSVAL_VOID;

        if (!callJSPropertyOp(cx, obj->getClass()->getProperty, obj, ID_TO_VALUE(id), vp))
            return JS_FALSE;

        PCMETER(getHow & JSGET_CACHE_RESULT && JS_PROPERTY_CACHE(cx).nofills++);

        /*
         * Give a strict warning if foo.bar is evaluated by a script for an
         * object foo with no property named 'bar'.
         */
        jsbytecode *pc;
        if (JSVAL_IS_VOID(*vp) && ((pc = js_GetCurrentBytecodePC(cx)) != NULL)) {
            JSOp op;
            uintN flags;

            op = (JSOp) *pc;
            if (op == JSOP_TRAP) {
                JS_ASSERT_NOT_ON_TRACE(cx);
                op = JS_GetTrapOpcode(cx, cx->fp->script, pc);
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
                if (id == ATOM_TO_JSID(cx->runtime->atomState.iteratorAtom))
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
                                          JSDVG_IGNORE_STACK, ID_TO_VALUE(id),
                                          NULL, NULL, NULL)) {
                return JS_FALSE;
            }
        }
        return JS_TRUE;
    }

    if (!obj2->isNative())
        return obj2->getProperty(cx, id, vp);

    sprop = (JSScopeProperty *) prop;

    if (getHow & JSGET_CACHE_RESULT) {
        JS_ASSERT_NOT_ON_TRACE(cx);
        JS_PROPERTY_CACHE(cx).fill(cx, aobj, 0, protoIndex, obj2, sprop);
    }

    if (!js_NativeGet(cx, obj, obj2, sprop, getHow, vp))
        return JS_FALSE;

    JS_UNLOCK_OBJ(cx, obj2);
    return JS_TRUE;
}

JSBool
js_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return js_GetPropertyHelper(cx, obj, id, JSGET_METHOD_BARRIER, vp);
}

JSBool
js_GetMethod(JSContext *cx, JSObject *obj, jsid id, uintN getHow, jsval *vp)
{
    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED);

    if (obj->map->ops == &js_ObjectOps ||
        obj->map->ops->getProperty == js_GetProperty) {
        return js_GetPropertyHelper(cx, obj, id, getHow, vp);
    }
    JS_ASSERT_IF(getHow & JSGET_CACHE_RESULT, obj->isDenseArray());
#if JS_HAS_XML_SUPPORT
    if (obj->isXML())
        return js_GetXMLMethod(cx, obj, id, vp);
#endif
    return obj->getProperty(cx, id, vp);
}

JS_FRIEND_API(bool)
js_CheckUndeclaredVarAssignment(JSContext *cx, jsval propname)
{
    JSStackFrame *const fp = js_GetTopStackFrame(cx);
    if (!fp)
        return true;

    /* If neither cx nor the code is strict, then no check is needed. */
    if (!(fp->script && fp->script->strictModeCode) &&
        !JS_HAS_STRICT_OPTION(cx)) {
        return true;
    }

    const char *bytes = js_GetStringBytes(cx, JSVAL_TO_STRING(propname));
    return bytes &&
           JS_ReportErrorFlagsAndNumber(cx,
                                        (JSREPORT_WARNING | JSREPORT_STRICT
                                         | JSREPORT_STRICT_MODE_ERROR),
                                        js_GetErrorMessage, NULL,
                                        JSMSG_UNDECLARED_VAR, bytes);
}

namespace js {

JSBool
ReportReadOnly(JSContext* cx, jsid id, uintN flags)
{
    return js_ReportValueErrorFlags(cx, flags, JSMSG_READ_ONLY,
                                    JSDVG_IGNORE_STACK, ID_TO_VALUE(id), NULL,
                                    NULL, NULL);
}

}

/*
 * Note: all non-error exits in this function must notify the tracer using
 * SetPropHit when called from the interpreter, which is detected by testing
 * (defineHow & JSDNP_CACHE_RESULT).
 */
JSBool
js_SetPropertyHelper(JSContext *cx, JSObject *obj, jsid id, uintN defineHow,
                     jsval *vp)
{
    int protoIndex;
    JSObject *pobj;
    JSProperty *prop;
    JSScopeProperty *sprop;
    JSScope *scope;
    uintN attrs, flags;
    intN shortid;
    JSClass *clasp;
    JSPropertyOp getter, setter;
    bool added;

    JS_ASSERT((defineHow &
               ~(JSDNP_CACHE_RESULT | JSDNP_SET_METHOD | JSDNP_UNQUALIFIED)) == 0);
    if (defineHow & JSDNP_CACHE_RESULT)
        JS_ASSERT_NOT_ON_TRACE(cx);

    /* Convert string indices to integers if appropriate. */
    id = js_CheckForStringIndex(id);

    /*
     * We peek at obj->scope() without locking obj. Any race means a failure
     * to seal before sharing, which is inherently ambiguous.
     */
    if (obj->scope()->sealed() && obj->scope()->object == obj)
        return ReportReadOnly(cx, id, JSREPORT_ERROR);

    protoIndex = js_LookupPropertyWithFlags(cx, obj, id, cx->resolveFlags,
                                            &pobj, &prop);
    if (protoIndex < 0)
        return JS_FALSE;
    if (prop) {
        if (!pobj->isNative())
            prop = NULL;
    } else {
        /* We should never add properties to lexical blocks.  */
        JS_ASSERT(obj->getClass() != &js_BlockClass);

        if (!obj->getParent() &&
            (defineHow & JSDNP_UNQUALIFIED) &&
            !js_CheckUndeclaredVarAssignment(cx, ID_TO_VALUE(id))) {
            return JS_FALSE;
        }
    }
    sprop = (JSScopeProperty *) prop;

    /*
     * Now either sprop is null, meaning id was not found in obj or one of its
     * prototypes; or sprop is non-null, meaning id was found in pobj's scope.
     * If JS_THREADSAFE and sprop is non-null, then scope is locked, and sprop
     * is held: we must JSObject::dropProperty or JS_UNLOCK_SCOPE before we
     * return (the two are equivalent for native objects, but we use
     * JS_UNLOCK_SCOPE because it is cheaper).
     */
    attrs = JSPROP_ENUMERATE;
    flags = 0;
    shortid = 0;
    clasp = obj->getClass();
    getter = clasp->getProperty;
    setter = clasp->setProperty;

    if (sprop) {
        /*
         * Set scope for use below.  It was locked by js_LookupProperty, and
         * we know pobj owns it (i.e., scope->object == pobj).  Therefore we
         * optimize JS_UNLOCK_OBJ(cx, pobj) into JS_UNLOCK_SCOPE(cx, scope).
         */
        scope = pobj->scope();

        /* ES5 8.12.4 [[Put]] step 2. */
        if (sprop->isAccessorDescriptor()) {
            if (sprop->hasDefaultSetter()) {
                JS_UNLOCK_SCOPE(cx, scope);
                if (defineHow & JSDNP_CACHE_RESULT)
                    TRACE_2(SetPropHit, JS_NO_PROP_CACHE_FILL, sprop);
                return js_ReportGetterOnlyAssignment(cx);
            }
        } else {
            JS_ASSERT(sprop->isDataDescriptor());

            if (!sprop->writable()) {
                JS_UNLOCK_SCOPE(cx, scope);

                PCMETER((defineHow & JSDNP_CACHE_RESULT) && JS_PROPERTY_CACHE(cx).rofills++);
                if (defineHow & JSDNP_CACHE_RESULT) {
                    JS_ASSERT_NOT_ON_TRACE(cx);
                    TRACE_2(SetPropHit, JS_NO_PROP_CACHE_FILL, sprop);
                }

                /* Warn in strict mode, otherwise do nothing. */
                if (JS_HAS_STRICT_OPTION(cx))
                    return ReportReadOnly(cx, id, JSREPORT_STRICT | JSREPORT_WARNING);
                return JS_TRUE;

#ifdef JS_TRACER
              error: // TRACE_2 jumps here in case of error.
                return JS_FALSE;
#endif
            }
        }
        if (scope->sealed() && !sprop->hasSlot()) {
            JS_UNLOCK_SCOPE(cx, scope);
            return ReportReadOnly(cx, id, JSREPORT_ERROR);
        }

        attrs = sprop->attributes();
        if (pobj != obj) {
            /*
             * We found id in a prototype object: prepare to share or shadow.
             *
             * NB: Thanks to the immutable, garbage-collected property tree
             * maintained by jsscope.c in cx->runtime, we needn't worry about
             * sprop going away behind our back after we've unlocked scope.
             */
            JS_UNLOCK_SCOPE(cx, scope);

            /* Don't clone a prototype property that doesn't have a slot. */
            if (!sprop->hasSlot()) {
                if (defineHow & JSDNP_CACHE_RESULT) {
                    JS_ASSERT_NOT_ON_TRACE(cx);
                    PropertyCacheEntry *entry =
                        JS_PROPERTY_CACHE(cx).fill(cx, obj, 0, protoIndex, pobj, sprop);
                    TRACE_2(SetPropHit, entry, sprop);
                }

                if (sprop->hasDefaultSetter() && !sprop->hasGetterValue())
                    return JS_TRUE;

                return sprop->set(cx, obj, vp);
            }

            /* Restore attrs to the ECMA default for new properties. */
            attrs = JSPROP_ENUMERATE;

            /*
             * Preserve the shortid, getter, and setter when shadowing any
             * property that has a shortid.  An old API convention requires
             * that the property's getter and setter functions receive the
             * shortid, not id, when they are called on the shadow we are
             * about to create in obj's scope.
             */
            if (sprop->hasShortID()) {
                flags = JSScopeProperty::HAS_SHORTID;
                shortid = sprop->shortid;
                getter = sprop->getter();
                setter = sprop->setter();
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

    added = false;
    if (!sprop) {
        /*
         * Purge the property cache of now-shadowed id in obj's scope chain.
         * Do this early, before locking obj to avoid nesting locks.
         */
        js_PurgeScopeChain(cx, obj, id);

        /* Find or make a property descriptor with the right heritage. */
        JS_LOCK_OBJ(cx, obj);
        scope = js_GetMutableScope(cx, obj);
        if (!scope) {
            JS_UNLOCK_OBJ(cx, obj);
            return JS_FALSE;
        }

        /*
         * Check for Object class here to avoid defining a method on a class
         * with magic resolve, addProperty, getProperty, etc. hooks.
         */
        if ((defineHow & JSDNP_SET_METHOD) &&
            obj->getClass() == &js_ObjectClass) {
            JS_ASSERT(VALUE_IS_FUNCTION(cx, *vp));
            JS_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));

            JSObject *funobj = JSVAL_TO_OBJECT(*vp);
            if (FUN_OBJECT(GET_FUNCTION_PRIVATE(cx, funobj)) == funobj) {
                flags |= JSScopeProperty::METHOD;
                getter = CastAsPropertyOp(funobj);
            }
        }

        sprop = scope->putProperty(cx, id, getter, setter, SPROP_INVALID_SLOT,
                                   attrs, flags, shortid);
        if (!sprop) {
            JS_UNLOCK_SCOPE(cx, scope);
            return JS_FALSE;
        }

        /*
         * Initialize the new property value (passed to setter) to undefined.
         * Note that we store before calling addProperty, to match the order
         * in js_DefineNativeProperty.
         */
        if (SPROP_HAS_VALID_SLOT(sprop, scope))
            obj->lockedSetSlot(sprop->slot, JSVAL_VOID);

        /* XXXbe called with obj locked */
        if (!AddPropertyHelper(cx, clasp, obj, scope, sprop, vp)) {
            scope->removeProperty(cx, id);
            JS_UNLOCK_SCOPE(cx, scope);
            return JS_FALSE;
        }
        added = true;
    }

    if (defineHow & JSDNP_CACHE_RESULT) {
        JS_ASSERT_NOT_ON_TRACE(cx);
        PropertyCacheEntry *entry = JS_PROPERTY_CACHE(cx).fill(cx, obj, 0, 0, obj, sprop, added);
        TRACE_2(SetPropHit, entry, sprop);
    }

    if (!js_NativeSet(cx, obj, sprop, added, vp))
        return NULL;

    JS_UNLOCK_SCOPE(cx, scope);
    return JS_TRUE;
}

JSBool
js_SetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return js_SetPropertyHelper(cx, obj, id, 0, vp);
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

    JSScopeProperty *sprop = (JSScopeProperty *)prop;
    *attrsp = sprop->attributes();
    JS_UNLOCK_OBJ(cx, obj);
    return true;
}

JSBool
js_SetNativeAttributes(JSContext *cx, JSObject *obj, JSScopeProperty *sprop,
                       uintN attrs)
{
    JS_ASSERT(obj->isNative());
    sprop = js_ChangeNativePropertyAttrs(cx, obj, sprop, attrs, 0,
                                         sprop->getter(), sprop->setter());
    JS_UNLOCK_OBJ(cx, obj);
    return (sprop != NULL);
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
           ? js_SetNativeAttributes(cx, obj, (JSScopeProperty *) prop, *attrsp)
           : obj->setAttributes(cx, id, attrsp);
}

JSBool
js_DeleteProperty(JSContext *cx, JSObject *obj, jsid id, jsval *rval)
{
    JSObject *proto;
    JSProperty *prop;
    JSScopeProperty *sprop;
    JSScope *scope;
    JSBool ok;

    *rval = JSVAL_TRUE;

    /* Convert string indices to integers if appropriate. */
    id = js_CheckForStringIndex(id);

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
            if (proto->isNative()) {
                sprop = (JSScopeProperty *)prop;
                if (sprop->isSharedPermanent())
                    *rval = JSVAL_FALSE;
                JS_UNLOCK_OBJ(cx, proto);
            }
            if (*rval == JSVAL_FALSE)
                return JS_TRUE;
        }

        /*
         * If no property, or the property comes unshared or impermanent from
         * a prototype, call the class's delProperty hook, passing rval as the
         * result parameter.
         */
        return callJSPropertyOp(cx, obj->getClass()->delProperty, obj, ID_TO_VALUE(id), rval);
    }

    sprop = (JSScopeProperty *)prop;
    if (!sprop->configurable()) {
        JS_UNLOCK_OBJ(cx, obj);
        *rval = JSVAL_FALSE;
        return JS_TRUE;
    }

    /* XXXbe called with obj locked */
    if (!callJSPropertyOp(cx, obj->getClass()->delProperty, obj, SPROP_USERID(sprop), rval)) {
        JS_UNLOCK_OBJ(cx, obj);
        return JS_FALSE;
    }

    scope = obj->scope();
    if (SPROP_HAS_VALID_SLOT(sprop, scope))
        GC_POKE(cx, obj->lockedGetSlot(sprop->slot));

    ok = scope->removeProperty(cx, id);
    JS_UNLOCK_OBJ(cx, obj);

    return ok && js_SuppressDeletedProperty(cx, obj, id);
}

namespace js {

JSBool
DefaultValue(JSContext *cx, JSObject *obj, JSType hint, jsval *vp)
{
    jsval v, save;
    JSString *str;

    JS_ASSERT(hint != JSTYPE_OBJECT && hint != JSTYPE_FUNCTION);

    v = save = OBJECT_TO_JSVAL(obj);
    switch (hint) {
      case JSTYPE_STRING:
        /*
         * Optimize for String objects with standard toString methods. Support
         * new String(...) instances whether mutated to have their own scope or
         * not, as well as direct String.prototype references.
         */
        if (obj->getClass() == &js_StringClass) {
            jsid toStringId = ATOM_TO_JSID(cx->runtime->atomState.toStringAtom);

            JS_LOCK_OBJ(cx, obj);
            JSScope *scope = obj->scope();
            JSScopeProperty *sprop = scope->lookup(toStringId);
            JSObject *pobj = obj;

            if (!sprop) {
                pobj = obj->getProto();

                if (pobj && pobj->getClass() == &js_StringClass) {
                    JS_UNLOCK_SCOPE(cx, scope);
                    JS_LOCK_OBJ(cx, pobj);
                    scope = pobj->scope();
                    sprop = scope->lookup(toStringId);
                }
            }

            if (sprop && sprop->hasDefaultGetter() && SPROP_HAS_VALID_SLOT(sprop, scope)) {
                jsval fval = pobj->lockedGetSlot(sprop->slot);

                if (VALUE_IS_FUNCTION(cx, fval)) {
                    JSObject *funobj = JSVAL_TO_OBJECT(fval);
                    JSFunction *fun = GET_FUNCTION_PRIVATE(cx, funobj);

                    if (FUN_FAST_NATIVE(fun) == js_str_toString) {
                        JS_UNLOCK_SCOPE(cx, scope);
                        *vp = obj->getPrimitiveThis();
                        return JS_TRUE;
                    }
                }
            }
            JS_UNLOCK_SCOPE(cx, scope);
        }

        /*
         * Propagate the exception if js_TryMethod finds an appropriate
         * method, and calling that method returned failure.
         */
        if (!js_TryMethod(cx, obj, cx->runtime->atomState.toStringAtom, 0, NULL,
                          &v)) {
            return JS_FALSE;
        }

        if (!JSVAL_IS_PRIMITIVE(v)) {
            if (!obj->getClass()->convert(cx, obj, hint, &v))
                return JS_FALSE;
        }
        break;

      default:
        if (!obj->getClass()->convert(cx, obj, hint, &v))
            return JS_FALSE;
        if (!JSVAL_IS_PRIMITIVE(v)) {
            JS_ASSERT(hint != JS_TypeOfValue(cx, v));
            if (!js_TryMethod(cx, obj, cx->runtime->atomState.toStringAtom, 0, NULL, &v))
                return JS_FALSE;
        }
        break;
    }
    if (!JSVAL_IS_PRIMITIVE(v)) {
        /* Avoid recursive death when decompiling in js_ReportValueError. */
        if (hint == JSTYPE_STRING) {
            str = JS_InternString(cx, obj->getClass()->name);
            if (!str)
                return JS_FALSE;
        } else {
            str = NULL;
        }
        *vp = OBJECT_TO_JSVAL(obj);
        js_ReportValueError2(cx, JSMSG_CANT_CONVERT_TO,
                             JSDVG_SEARCH_STACK, save, str,
                             (hint == JSTYPE_VOID)
                             ? "primitive type"
                             : JS_TYPE_STR(hint));
        return JS_FALSE;
    }
    *vp = v;
    return JS_TRUE;
}

} /* namespace js */

JSBool
js_Enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op, jsval *statep, jsid *idp)
{
    /* If the class has a custom JSCLASS_NEW_ENUMERATE hook, call it. */
    JSClass *clasp = obj->getClass();
    JSEnumerateOp enumerate = clasp->enumerate;
    if (clasp->flags & JSCLASS_NEW_ENUMERATE) {
        JS_ASSERT(enumerate != JS_EnumerateStub);
        return ((JSNewEnumerateOp) enumerate)(cx, obj, enum_op, statep, idp);
    }

    if (!enumerate(cx, obj))
        return false;

    /* Tell InitNativeIterator to treat us like a native object. */
    JS_ASSERT(enum_op == JSENUMERATE_INIT || enum_op == JSENUMERATE_INIT_ALL);
    *statep = JSVAL_NATIVE_ENUMERATE_COOKIE;
    return true;
}

namespace js {

JSBool
CheckAccess(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
            jsval *vp, uintN *attrsp)
{
    JSBool writing;
    JSObject *pobj;
    JSProperty *prop;
    JSClass *clasp;
    JSScopeProperty *sprop;
    JSSecurityCallbacks *callbacks;
    JSCheckAccessOp check;

    while (JS_UNLIKELY(obj->getClass() == &js_WithClass))
        obj = obj->getProto();

    writing = (mode & JSACC_WRITE) != 0;
    switch (mode & JSACC_TYPEMASK) {
      case JSACC_PROTO:
        pobj = obj;
        if (!writing)
            *vp = OBJECT_TO_JSVAL(obj->getProto());
        *attrsp = JSPROP_PERMANENT;
        break;

      case JSACC_PARENT:
        JS_ASSERT(!writing);
        pobj = obj;
        *vp = OBJECT_TO_JSVAL(obj->getParent());
        *attrsp = JSPROP_READONLY | JSPROP_PERMANENT;
        break;

      default:
        if (!obj->lookupProperty(cx, id, &pobj, &prop))
            return JS_FALSE;
        if (!prop) {
            if (!writing)
                *vp = JSVAL_VOID;
            *attrsp = 0;
            pobj = obj;
            break;
        }

        if (!pobj->isNative()) {
            if (!writing) {
                *vp = JSVAL_VOID;
                *attrsp = 0;
            }
            break;
        }

        sprop = (JSScopeProperty *)prop;
        *attrsp = sprop->attributes();
        if (!writing) {
            *vp = (SPROP_HAS_VALID_SLOT(sprop, pobj->scope()))
                  ? pobj->lockedGetSlot(sprop->slot)
                  : JSVAL_VOID;
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
        check = callbacks ? callbacks->checkObjectAccess : NULL;
    }
    return !check || check(cx, pobj, ID_TO_VALUE(id), mode, vp);
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

#ifdef NARCISSUS
    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED);
    jsval v;

    if (!obj->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.__call__Atom), &v)) {
        JS_ClearPendingException(cx);
    } else {
        if (VALUE_IS_FUNCTION(cx, v))
            return JSTYPE_FUNCTION;
    }
#endif

    return JSTYPE_OBJECT;
}

#ifdef JS_THREADSAFE
void
js_DropProperty(JSContext *cx, JSObject *obj, JSProperty *prop)
{
    JS_UNLOCK_OBJ(cx, obj);
}
#endif

#ifdef NARCISSUS
static JSBool
GetCurrentExecutionContext(JSContext *cx, JSObject *obj, jsval *rval)
{
    JSObject *tmp;
    jsval xcval;

    while ((tmp = obj->getParent()) != NULL)
        obj = tmp;
    if (!obj->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.ExecutionContextAtom), &xcval))
        return JS_FALSE;
    if (JSVAL_IS_PRIMITIVE(xcval)) {
        JS_ReportError(cx, "invalid ExecutionContext in global object");
        return JS_FALSE;
    }
    if (!JSVAL_TO_OBJECT(xcval)->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.currentAtom),
                                             rval)) {
        return JS_FALSE;
    }
    return JS_TRUE;
}
#endif

JSBool
js_Call(JSContext *cx, uintN argc, jsval *vp)
{
    JSStackFrame *fp = cx->fp;
    JSObject *obj = fp->getThisObject(cx);
    if (!obj)
        return false;
    JS_ASSERT(OBJECT_TO_JSVAL(obj) == fp->thisv);

    JSObject *callee = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));
    JSClass *clasp = callee->getClass();
    if (!clasp->call) {
#ifdef NARCISSUS
        JSObject *args;
        jsval fval, nargv[3];
        JSBool ok;

        if (!callee->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.__call__Atom), &fval))
            return JS_FALSE;
        if (VALUE_IS_FUNCTION(cx, fval)) {
            if (!GetCurrentExecutionContext(cx, obj, &nargv[2]))
                return JS_FALSE;
            args = js_GetArgsObject(cx, js_GetTopStackFrame(cx));
            if (!args)
                return JS_FALSE;
            nargv[0] = OBJECT_TO_JSVAL(obj);
            nargv[1] = OBJECT_TO_JSVAL(args);
            return js_InternalCall(cx, callee, fval, 3, nargv, rval);
        }
        if (JSVAL_IS_OBJECT(fval) && JSVAL_TO_OBJECT(fval) != callee) {
            vp[0] = fval;
            ok = js_Call(cx, argc, vp);
            vp[0] = OBJECT_TO_JSVAL(callee);
            return ok;
        }
#endif
        js_ReportIsNotFunction(cx, &vp[0], 0);
        return JS_FALSE;
    }
    AutoValueRooter rval(cx);
    JSBool ok = clasp->call(cx, obj, argc, JS_ARGV(cx, vp), rval.addr());
    if (ok)
        JS_SET_RVAL(cx, vp, rval.value());
    return ok;
}

JSBool
js_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSClass *clasp;

    clasp = JSVAL_TO_OBJECT(argv[-2])->getClass();
    if (!clasp->construct) {
#ifdef NARCISSUS
        JSObject *callee, *args;
        jsval cval, nargv[2];
        JSBool ok;

        callee = JSVAL_TO_OBJECT(argv[-2]);
        if (!callee->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.__construct__Atom),
                                 &cval)) {
            return JS_FALSE;
        }
        if (VALUE_IS_FUNCTION(cx, cval)) {
            if (!GetCurrentExecutionContext(cx, obj, &nargv[1]))
                return JS_FALSE;
            args = js_GetArgsObject(cx, js_GetTopStackFrame(cx));
            if (!args)
                return JS_FALSE;
            nargv[0] = OBJECT_TO_JSVAL(args);
            return js_InternalCall(cx, callee, cval, 2, nargv, rval);
        }
        if (JSVAL_IS_OBJECT(cval) && JSVAL_TO_OBJECT(cval) != callee) {
            argv[-2] = cval;
            ok = js_Call(cx, argc, argv - 2);
            if (ok)
                *rval = argv[-2];
            argv[-2] = OBJECT_TO_JSVAL(callee);
            return ok;
        }
#endif
        js_ReportIsNotFunction(cx, &argv[-2], JSV2F_CONSTRUCT);
        return JS_FALSE;
    }
    return clasp->construct(cx, obj, argc, argv, rval);
}

JSBool
js_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    JSClass *clasp;

    clasp = obj->getClass();
    if (clasp->hasInstance)
        return clasp->hasInstance(cx, obj, v, bp);
#ifdef NARCISSUS
    {
        jsval fval, rval;

        if (!obj->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.__hasInstance__Atom), &fval))
            return JS_FALSE;
        if (VALUE_IS_FUNCTION(cx, fval)) {
            if (!js_InternalCall(cx, obj, fval, 1, &v, &rval))
                return JS_FALSE;
            *bp = js_ValueToBoolean(rval);
            return JS_TRUE;
        }
    }
#endif
    js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS,
                        JSDVG_SEARCH_STACK, OBJECT_TO_JSVAL(obj), NULL);
    return JS_FALSE;
}

JSBool
js_IsDelegate(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    JSObject *obj2;

    *bp = JS_FALSE;
    if (JSVAL_IS_PRIMITIVE(v))
        return JS_TRUE;
    obj2 = JSVAL_TO_OBJECT(v)->wrappedObject(cx);
    while ((obj2 = obj2->getProto()) != NULL) {
        if (obj2 == obj) {
            *bp = JS_TRUE;
            break;
        }
    }
    return JS_TRUE;
}

bool
js::FindClassPrototype(JSContext *cx, JSObject *scope, JSProtoKey protoKey, JSObject **protop,
                       JSClass *clasp)
{
    jsval v;
    if (!js_FindClassObject(cx, scope, protoKey, &v, clasp))
        return false;

    if (VALUE_IS_FUNCTION(cx, v)) {
        JSObject *ctor = JSVAL_TO_OBJECT(v);
        if (!ctor->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom), &v))
            return false;
    }

    *protop = JSVAL_IS_OBJECT(v) ? JSVAL_TO_OBJECT(v) : NULL;
    return true;
}

/*
 * The first part of this function has been hand-expanded and optimized into
 * NewBuiltinClassInstance in jsobjinlines.h.
 */
JSBool
js_GetClassPrototype(JSContext *cx, JSObject *scope, JSProtoKey protoKey,
                     JSObject **protop, JSClass *clasp)
{
    VOUCH_DOES_NOT_REQUIRE_STACK();
    JS_ASSERT(JSProto_Null <= protoKey);
    JS_ASSERT(protoKey < JSProto_LIMIT);

    if (protoKey != JSProto_Null) {
        if (!scope) {
            if (cx->fp)
                scope = cx->fp->scopeChain;
            if (!scope) {
                scope = cx->globalObject;
                if (!scope) {
                    *protop = NULL;
                    return true;
                }
            }
        }
        scope = scope->getGlobal();
        if (scope->getClass()->flags & JSCLASS_IS_GLOBAL) {
            jsval v = scope->getReservedSlot(JSProto_LIMIT + protoKey);
            if (!JSVAL_IS_PRIMITIVE(v)) {
                *protop = JSVAL_TO_OBJECT(v);
                return true;
            }
        }
    }

    return FindClassPrototype(cx, scope, protoKey, protop, clasp);
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
CheckCtorGetAccess(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSAtom *atom;
    uintN attrs;

    atom = cx->runtime->atomState.constructorAtom;
    JS_ASSERT(id == ATOM_TO_JSID(atom));
    return CheckAccess(cx, obj, ATOM_TO_JSID(atom), JSACC_READ, vp, &attrs);
}

static JSBool
CheckCtorSetAccess(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSAtom *atom;
    uintN attrs;

    atom = cx->runtime->atomState.constructorAtom;
    JS_ASSERT(id == ATOM_TO_JSID(atom));
    return CheckAccess(cx, obj, ATOM_TO_JSID(atom), JSACC_WRITE, vp, &attrs);
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
    if (!ctor->defineProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom),
                              OBJECT_TO_JSVAL(proto), JS_PropertyStub, JS_PropertyStub,
                              attrs)) {
        return JS_FALSE;
    }

    /*
     * ECMA says that Object.prototype.constructor, or f.prototype.constructor
     * for a user-defined function f, is DontEnum.
     */
    return proto->defineProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.constructorAtom),
                                 OBJECT_TO_JSVAL(ctor), CheckCtorGetAccess, CheckCtorSetAccess, 0);
}

JSBool
js_PrimitiveToObject(JSContext *cx, jsval *vp)
{
    JSClass *clasp;
    JSObject *obj;

    /* Table to map primitive value's tag into the corresponding class. */
    JS_STATIC_ASSERT(JSVAL_INT == 1);
    JS_STATIC_ASSERT(JSVAL_DOUBLE == 2);
    JS_STATIC_ASSERT(JSVAL_STRING == 4);
    JS_STATIC_ASSERT(JSVAL_SPECIAL == 6);
    static JSClass *const PrimitiveClasses[] = {
        &js_NumberClass,    /* INT     */
        &js_NumberClass,    /* DOUBLE  */
        &js_NumberClass,    /* INT     */
        &js_StringClass,    /* STRING  */
        &js_NumberClass,    /* INT     */
        &js_BooleanClass,   /* BOOLEAN */
        &js_NumberClass     /* INT     */
    };

    JS_ASSERT(!JSVAL_IS_OBJECT(*vp));
    JS_ASSERT(!JSVAL_IS_VOID(*vp));
    clasp = PrimitiveClasses[JSVAL_TAG(*vp) - 1];
    obj = NewBuiltinClassInstance(cx, clasp);
    if (!obj)
        return JS_FALSE;
    obj->setPrimitiveThis(*vp);
    *vp = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

JSBool
js_ValueToObject(JSContext *cx, jsval v, JSObject **objp)
{
    JSObject *obj;

    if (JSVAL_IS_OBJECT(v)) {
        obj = JSVAL_TO_OBJECT(v);
    } else if (JSVAL_IS_VOID(v)) {
        obj = NULL;
    } else {
        if (!js_PrimitiveToObject(cx, &v))
            return JS_FALSE;
        obj = JSVAL_TO_OBJECT(v);
    }
    *objp = obj;
    return JS_TRUE;
}

JSObject *
js_ValueToNonNullObject(JSContext *cx, jsval v)
{
    JSObject *obj;

    if (!js_ValueToObject(cx, v, &obj))
        return NULL;
    if (!obj)
        js_ReportIsNullOrUndefined(cx, JSDVG_SEARCH_STACK, v, NULL);
    return obj;
}

JSBool
js_TryValueOf(JSContext *cx, JSObject *obj, JSType type, jsval *rval)
{
    jsval argv[1];

    argv[0] = ATOM_KEY(cx->runtime->atomState.typeAtoms[type]);
    return js_TryMethod(cx, obj, cx->runtime->atomState.valueOfAtom, 1, argv,
                        rval);
}

JSBool
js_TryMethod(JSContext *cx, JSObject *obj, JSAtom *atom,
             uintN argc, jsval *argv, jsval *rval)
{
    JSErrorReporter older;
    jsid id;
    jsval fval;
    JSBool ok;

    JS_CHECK_RECURSION(cx, return JS_FALSE);

    /*
     * Report failure only if an appropriate method was found, and calling it
     * returned failure.  We propagate failure in this case to make exceptions
     * behave properly.
     */
    older = JS_SetErrorReporter(cx, NULL);
    id = ATOM_TO_JSID(atom);
    fval = JSVAL_VOID;
    ok = js_GetMethod(cx, obj, id, JSGET_NO_METHOD_BARRIER, &fval);
    JS_SetErrorReporter(cx, older);
    if (!ok)
        return false;

    if (JSVAL_IS_PRIMITIVE(fval))
        return JS_TRUE;
    return js_InternalCall(cx, obj, fval, argc, argv, rval);
}

#if JS_HAS_XDR

JSBool
js_XDRObject(JSXDRState *xdr, JSObject **objp)
{
    JSContext *cx;
    JSAtom *atom;
    JSClass *clasp;
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
            if (!JS_XDRRegisterClass(xdr, clasp, &classId))
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
    if (classDef == 1 && !js_XDRStringAtom(xdr, &atom))
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
            if (!JS_XDRRegisterClass(xdr, clasp, &classId))
                return JS_FALSE;
        } else {
            clasp = JS_XDRFindClassById(xdr, classId);
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

    JSScopeProperty *sprop;
    if (obj->isNative()) {
        JSScope *scope = obj->scope();
        sprop = scope->lastProperty();
        while (sprop && sprop->slot != slot)
            sprop = sprop->parent;
    } else {
        sprop = NULL;
    }

    if (!sprop) {
        const char *slotname = NULL;
        JSClass *clasp = obj->getClass();
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
        jsval nval = ID_TO_VALUE(sprop->id);
        if (JSVAL_IS_INT(nval)) {
            JS_snprintf(buf, bufsize, "%ld", (long)JSVAL_TO_INT(nval));
        } else if (JSVAL_IS_STRING(nval)) {
            js_PutEscapedString(buf, bufsize, JSVAL_TO_STRING(nval), 0);
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
    JSScope *scope = obj->scope();
    if (!scope->isSharedEmpty() && IS_GC_MARKING_TRACER(trc)) {
        /*
         * Check whether we should shrink the object's slots. Skip this check
         * if the scope is shared, since for Block objects and flat closures
         * that share their scope, scope->freeslot can be an underestimate.
         */
        size_t slots = scope->freeslot;
        if (obj->numSlots() != slots)
            obj->shrinkSlots(cx, slots);
    }

#ifdef JS_DUMP_SCOPE_METERS
    MeterEntryCount(scope->entryCount);
#endif

    scope->trace(trc);

    if (!JS_CLIST_IS_EMPTY(&cx->runtime->watchPointList))
        js_TraceWatchPoints(trc, obj);

    /* No one runs while the GC is running, so we can use LOCKED_... here. */
    JSClass *clasp = obj->getClass();
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

    obj->traceProtoAndParent(trc);

    /*
     * An unmutated object that shares a prototype object's scope. We can't
     * tell how many slots are in use in obj by looking at its scope, so we
     * use obj->numSlots().
     *
     * NB: In case clasp->mark mutates something, leave this code here --
     * don't move it up and unify it with the |if (!traceScope)| section
     * above.
     */
    uint32 nslots = obj->numSlots();
    if (!scope->isSharedEmpty() && scope->freeslot < nslots)
        nslots = scope->freeslot;
    JS_ASSERT(nslots >= JSSLOT_START(clasp));

    for (uint32 i = JSSLOT_START(clasp); i != nslots; ++i) {
        jsval v = obj->getSlot(i);
        if (JSVAL_IS_TRACEABLE(v)) {
            JS_SET_TRACING_DETAILS(trc, js_PrintObjectSlotName, obj, i);
            js_CallGCMarker(trc, JSVAL_TO_TRACEABLE(v), JSVAL_TRACE_KIND(v));
        }
    }
}

void
js_Clear(JSContext *cx, JSObject *obj)
{
    JSScope *scope;
    uint32 i, n;

    /*
     * Clear our scope and the property cache of all obj's properties only if
     * obj owns the scope (i.e., not if obj is sharing another object's scope).
     * NB: we do not clear any reserved slots lying below JSSLOT_FREE(clasp).
     */
    JS_LOCK_OBJ(cx, obj);
    scope = obj->scope();
    if (!scope->isSharedEmpty()) {
        /* Now that we're done using scope->lastProp/table, clear scope. */
        scope->clear(cx);

        /* Clear slot values and reset freeslot so we're consistent. */
        i = obj->numSlots();
        n = JSSLOT_FREE(obj->getClass());
        while (--i >= n)
            obj->setSlot(i, JSVAL_VOID);
        scope->freeslot = n;
    }
    JS_UNLOCK_OBJ(cx, obj);
}

bool
js_GetReservedSlot(JSContext *cx, JSObject *obj, uint32 index, jsval *vp)
{
    if (!obj->isNative()) {
        *vp = JSVAL_VOID;
        return true;
    }

    uint32 slot = JSSLOT_START(obj->getClass()) + index;
    JS_LOCK_OBJ(cx, obj);
    *vp = (slot < obj->numSlots()) ? obj->getSlot(slot) : JSVAL_VOID;
    JS_UNLOCK_OBJ(cx, obj);
    return true;
}

bool
js_SetReservedSlot(JSContext *cx, JSObject *obj, uint32 index, jsval v)
{
    if (!obj->isNative())
        return true;

    JSClass *clasp = obj->getClass();
    uint32 slot = JSSLOT_START(clasp) + index;

    JS_LOCK_OBJ(cx, obj);
    if (slot >= obj->numSlots()) {
        /*
         * At this point, obj may or may not own scope, and we may or may not
         * need to allocate slots. If scope is shared, scope->freeslot may not
         * be accurate for obj (see comment below).
         */
        uint32 nslots = JSSLOT_FREE(clasp);
        JS_ASSERT(slot < nslots);
        if (!obj->allocSlots(cx, nslots)) {
            JS_UNLOCK_OBJ(cx, obj);
            return false;
        }
    }

    /*
     * Whether or not we grew nslots, we may need to advance freeslot.
     *
     * If scope is shared, do not modify scope->freeslot. It is OK for freeslot
     * to be an underestimate in objects with shared scopes, as they will get
     * their own scopes before mutating, and elsewhere (e.g. js_TraceObject) we
     * use obj->numSlots() rather than rely on freeslot.
     */
    JSScope *scope = obj->scope();
    if (!scope->isSharedEmpty() && slot >= scope->freeslot)
        scope->freeslot = slot + 1;

    obj->setSlot(slot, v);
    GC_POKE(cx, JS_NULL);
    JS_UNLOCK_SCOPE(cx, scope);
    return true;
}

JSObject *
JSObject::wrappedObject(JSContext *cx) const
{
    JSClass *clasp = getClass();
    if (clasp->flags & JSCLASS_IS_EXTENDED) {
        if (JSObjectOp wrappedObject = reinterpret_cast<JSExtendedClass *>(clasp)->wrappedObject) {
            if (JSObject *obj = wrappedObject(cx, const_cast<JSObject *>(this)))
                return obj;
        }
    }
    return const_cast<JSObject *>(this);
}

JSObject *
JSObject::getGlobal()
{
    JSObject *obj = this;
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

    JSClass *clasp = obj->getClass();
    if (!(clasp->flags & JSCLASS_IS_GLOBAL)) {
        // The magic AnyName object is runtime-wide.
        if (clasp == &js_AnyNameClass)
            return cx->runtime->defaultCompartment;

        // The magic function namespace object is runtime-wide.
        if (clasp == &js_NamespaceClass.base &&
            obj->getNameURI() == ATOM_KEY(cx->runtime->atomState.lazy.functionNamespaceURIAtom)) {
            return cx->runtime->defaultCompartment;
        }

        // Compile-time Function, Block, and RegExp objects are not parented.
        if (clasp == &js_FunctionClass || clasp == &js_BlockClass || clasp == &js_RegExpClass) {
            // This is a bogus answer, but it'll do for now.
            return cx->runtime->defaultCompartment;
        }
        JS_NOT_REACHED("non-global object at end of scope chain");
    }
    jsval v = obj->getReservedSlot(JSRESERVED_GLOBAL_COMPARTMENT);
    return (JSCompartment *) JSVAL_TO_PRIVATE(v);
}

JS_FRIEND_API(JSBool)
js_GetterOnlyPropertyStub(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
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
    js_DumpValue(ATOM_KEY(atom));
}

void
dumpValue(jsval val)
{
    if (JSVAL_IS_NULL(val)) {
        fprintf(stderr, "null");
    } else if (JSVAL_IS_VOID(val)) {
        fprintf(stderr, "undefined");
    } else if (JSVAL_IS_OBJECT(val) && JSVAL_TO_OBJECT(val)->isFunction()) {
        JSObject *funobj = JSVAL_TO_OBJECT(val);
        JSFunction *fun = GET_FUNCTION_PRIVATE(cx, funobj);
        fprintf(stderr, "<%s %s at %p (JSFunction at %p)>",
                fun->atom ? "function" : "unnamed",
                fun->atom ? JS_GetStringBytes(ATOM_TO_STRING(fun->atom)) : "function",
                (void *) funobj,
                (void *) fun);
    } else if (JSVAL_IS_OBJECT(val)) {
        JSObject *obj = JSVAL_TO_OBJECT(val);
        JSClass *clasp = obj->getClass();
        fprintf(stderr, "<%s%s at %p>",
                clasp->name,
                clasp == &js_ObjectClass ? "" : " object",
                (void *) obj);
    } else if (JSVAL_IS_INT(val)) {
        fprintf(stderr, "%d", JSVAL_TO_INT(val));
    } else if (JSVAL_IS_STRING(val)) {
        dumpString(JSVAL_TO_STRING(val));
    } else if (JSVAL_IS_DOUBLE(val)) {
        fprintf(stderr, "%g", *JSVAL_TO_DOUBLE(val));
    } else if (val == JSVAL_TRUE) {
        fprintf(stderr, "true");
    } else if (val == JSVAL_FALSE) {
        fprintf(stderr, "false");
    } else if (val == JSVAL_HOLE) {
        fprintf(stderr, "hole");
    } else {
        /* jsvals are pointer-sized, and %p is portable */
        fprintf(stderr, "unrecognized jsval %p", (void *) val);
    }
}

JS_FRIEND_API(void)
js_DumpValue(jsval val)
{
    fprintf(stderr, "jsval %p = ", (void *) val);
    dumpValue(val);
    fputc('\n', stderr);
}

JS_FRIEND_API(void)
js_DumpId(jsid id)
{
    fprintf(stderr, "jsid %p = ", (void *) id);
    dumpValue(ID_TO_VALUE(id));
    fputc('\n', stderr);
}

static void
dumpScopeProp(JSScopeProperty *sprop)
{
    jsid id = sprop->id;
    uint8 attrs = sprop->attributes();

    fprintf(stderr, "    ");
    if (attrs & JSPROP_ENUMERATE) fprintf(stderr, "enumerate ");
    if (attrs & JSPROP_READONLY) fprintf(stderr, "readonly ");
    if (attrs & JSPROP_PERMANENT) fprintf(stderr, "permanent ");
    if (attrs & JSPROP_GETTER) fprintf(stderr, "getter ");
    if (attrs & JSPROP_SETTER) fprintf(stderr, "setter ");
    if (attrs & JSPROP_SHARED) fprintf(stderr, "shared ");
    if (sprop->isAlias()) fprintf(stderr, "alias ");
    if (JSID_IS_ATOM(id))
        dumpString(JSVAL_TO_STRING(ID_TO_VALUE(id)));
    else if (JSID_IS_INT(id))
        fprintf(stderr, "%d", (int) JSID_TO_INT(id));
    else
        fprintf(stderr, "unknown jsid %p", (void *) id);
    fprintf(stderr, ": slot %d", sprop->slot);
    fprintf(stderr, "\n");
}

JS_FRIEND_API(void)
js_DumpObject(JSObject *obj)
{
    uint32 i, slots;
    JSClass *clasp;
    jsuint reservedEnd;

    fprintf(stderr, "object %p\n", (void *) obj);
    clasp = obj->getClass();
    fprintf(stderr, "class %p %s\n", (void *)clasp, clasp->name);

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
        JSScope *scope = obj->scope();
        if (scope->sealed())
            fprintf(stderr, "sealed\n");

        fprintf(stderr, "properties:\n");
        for (JSScopeProperty *sprop = scope->lastProperty(); sprop;
             sprop = sprop->parent) {
            dumpScopeProp(sprop);
        }
    } else {
        if (!obj->isNative())
            fprintf(stderr, "not native\n");
    }

    fprintf(stderr, "proto ");
    dumpValue(OBJECT_TO_JSVAL(obj->getProto()));
    fputc('\n', stderr);

    fprintf(stderr, "parent ");
    dumpValue(OBJECT_TO_JSVAL(obj->getParent()));
    fputc('\n', stderr);

    i = JSSLOT_PRIVATE;
    if (clasp->flags & JSCLASS_HAS_PRIVATE) {
        i = JSSLOT_PRIVATE + 1;
        fprintf(stderr, "private %p\n", obj->getPrivate());
    }

    fprintf(stderr, "slots:\n");
    reservedEnd = i + JSCLASS_RESERVED_SLOTS(clasp);
    slots = (obj->isNative() && !obj->scope()->isSharedEmpty())
            ? obj->scope()->freeslot
            : obj->numSlots();
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
        dumpValue(OBJECT_TO_JSVAL(obj));
        fputc('\n', stderr);
    }
}

static void
MaybeDumpValue(const char *name, jsval v)
{
    if (!JSVAL_IS_NULL(v)) {
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
        start = cx->fp;
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
        if (fp->argv) {
            fprintf(stderr, "callee: ");
            dumpValue(fp->argv[-2]);
        } else {
            fprintf(stderr, "global frame, no callee");
        }
        fputc('\n', stderr);

        if (fp->script)
            fprintf(stderr, "file %s line %u\n", fp->script->filename, (unsigned) fp->script->lineno);

        if (jsbytecode *pc = i.pc()) {
            if (!fp->script) {
                fprintf(stderr, "*** pc && !script, skipping frame\n\n");
                continue;
            }
            if (fp->imacpc) {
                fprintf(stderr, "  pc in imacro at %p\n  called from ", pc);
                pc = fp->imacpc;
            } else {
                fprintf(stderr, "  ");
            }
            fprintf(stderr, "pc = %p\n", pc);
            fprintf(stderr, "  current op: %s\n", js_CodeName[*pc]);
        }
        jsval *sp = i.sp();
        fprintf(stderr, "  slots: %p\n", (void *) fp->slots());
        fprintf(stderr, "  sp:    %p = slots + %u\n", (void *) sp, (unsigned) (sp - fp->slots()));
        if (sp - fp->slots() < 10000) { // sanity
            for (jsval *p = fp->slots(); p < sp; p++) {
                fprintf(stderr, "    %p: ", (void *) p);
                dumpValue(*p);
                fputc('\n', stderr);
            }
        }
        fprintf(stderr, "  argv:  %p (argc: %u)\n", (void *) fp->argv, (unsigned) fp->argc);
        MaybeDumpObject("callobj", fp->callobj);
        MaybeDumpObject("argsobj", JSVAL_TO_OBJECT(fp->argsobj));
        MaybeDumpValue("this", fp->thisv);
        fprintf(stderr, "  rval: ");
        dumpValue(fp->rval);
        fputc('\n', stderr);

        fprintf(stderr, "  flags:");
        if (fp->flags == 0)
            fprintf(stderr, " none");
        if (fp->flags & JSFRAME_CONSTRUCTING)
            fprintf(stderr, " constructing");
        if (fp->flags & JSFRAME_COMPUTED_THIS)
            fprintf(stderr, " computed_this");
        if (fp->flags & JSFRAME_ASSIGNING)
            fprintf(stderr, " assigning");
        if (fp->flags & JSFRAME_DEBUGGER)
            fprintf(stderr, " debugger");
        if (fp->flags & JSFRAME_EVAL)
            fprintf(stderr, " eval");
        if (fp->flags & JSFRAME_YIELDING)
            fprintf(stderr, " yielding");
        if (fp->flags & JSFRAME_GENERATOR)
            fprintf(stderr, " generator");
        if (fp->flags & JSFRAME_OVERRIDE_ARGS)
            fprintf(stderr, " overridden_args");
        fputc('\n', stderr);

        if (fp->scopeChain)
            fprintf(stderr, "  scopeChain: (JSObject *) %p\n", (void *) fp->scopeChain);
        if (fp->blockChain)
            fprintf(stderr, "  blockChain: (JSObject *) %p\n", (void *) fp->blockChain);

        if (fp->displaySave)
            fprintf(stderr, "  displaySave: (JSStackFrame *) %p\n", (void *) fp->displaySave);

        fputc('\n', stderr);
    }
}

#endif
