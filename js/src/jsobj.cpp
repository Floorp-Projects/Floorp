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

#include "mozilla/Util.h"

#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h"
#include "jshash.h"
#include "jsdhash.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsgcmark.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsonparser.h"
#include "jsopcode.h"
#include "jsprobes.h"
#include "jsproxy.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstdint.h"
#include "jsstr.h"
#include "jsdbgapi.h"
#include "json.h"
#include "jswatchpoint.h"
#include "jswrapper.h"

#include "frontend/BytecodeCompiler.h"
#include "frontend/BytecodeEmitter.h"
#include "frontend/Parser.h"
#include "js/MemoryMetrics.h"

#include "jsarrayinlines.h"
#include "jsinterpinlines.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"
#include "jsstrinlines.h"

#include "vm/BooleanObject-inl.h"
#include "vm/NumberObject-inl.h"
#include "vm/StringObject-inl.h"

#if JS_HAS_GENERATORS
#include "jsiter.h"
#endif

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#if JS_HAS_XDR
#include "jsxdrapi.h"
#endif

#include "jsatominlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "jsautooplen.h"

using namespace mozilla;
using namespace js;
using namespace js::gc;
using namespace js::types;

JS_STATIC_ASSERT(int32_t((JSObject::NELEMENTS_LIMIT - 1) * sizeof(Value)) == int64_t((JSObject::NELEMENTS_LIMIT - 1) * sizeof(Value)));

Class js::ObjectClass = {
    js_Object_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

JS_FRIEND_API(JSObject *)
JS_ObjectToInnerObject(JSContext *cx, JSObject *obj)
{
    if (!obj) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INACTIVE);
        return NULL;
    }
    OBJ_TO_INNER_OBJECT(cx, obj);
    return obj;
}

JS_FRIEND_API(JSObject *)
JS_ObjectToOuterObject(JSContext *cx, JSObject *obj)
{
    OBJ_TO_OUTER_OBJECT(cx, obj);
    return obj;
}

#if JS_HAS_OBJ_PROTO_PROP

static JSBool
obj_getProto(JSContext *cx, JSObject *obj, jsid id, Value *vp);

static JSBool
obj_setProto(JSContext *cx, JSObject *obj, jsid id, JSBool strict, Value *vp);

JSPropertySpec object_props[] = {
    {js_proto_str, 0, JSPROP_PERMANENT|JSPROP_SHARED, obj_getProto, obj_setProto},
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

size_t sSetProtoCalled = 0;

static JSBool
obj_setProto(JSContext *cx, JSObject *obj, jsid id, JSBool strict, Value *vp)
{
    if (!cx->runningWithTrustedPrincipals())
        ++sSetProtoCalled;

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
            ok = obj->lookupGeneric(cx, id, &obj2, &prop);
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
                ok = obj->getGeneric(cx, id, v.addr());
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
        *sp = InflateString(cx, buf, &len);
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
                    cx->free_(*sp);
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
    MarkRoot((JSTracer *)arg, (JSObject *)he->key, "sharp table entry");
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
    JSBool ok;
    JSIdArray *ida;
    jschar *chars, *ochars, *vsharp;
    const jschar *idstrchars, *vchars;
    size_t nchars, idstrlength, gsoplength, vlength, vsharplength, curlen;
    const char *comma;
    JSObject *obj2;
    JSProperty *prop;
    Value *val;
    JSString *gsop[2];
    JSString *valstr, *str;
    JSLinearString *idstr;

    JS_CHECK_RECURSION(cx, return JS_FALSE);

    Value localroot[4];
    PodArrayZero(localroot);
    AutoArrayRooter tvr(cx, ArrayLength(localroot), localroot);

    /* If outermost, we need parentheses to be an expression, not a block. */
    JSBool outermost = (cx->sharpObjectMap.depth == 0);

    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;

    JSHashEntry *he = js_EnterSharpObject(cx, obj, &ida, &chars);
    if (!he)
        return false;

    if (!ida) {
        /*
         * We didn't enter -- obj is already "sharp", meaning we've visited it
         * already in our depth first search, and therefore chars contains a
         * string of the form "#n#".
         */
        JS_ASSERT(IS_SHARP(he));
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
    JS_ASSERT(!IS_SHARP(he));
    ok = JS_TRUE;

    if (!chars) {
        /* If outermost, allocate 4 + 1 for "({})" and the terminator. */
        chars = (jschar *) cx->malloc_(((outermost ? 4 : 2) + 1) * sizeof(jschar));
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
            cx->realloc_((ochars = chars), (nchars + 2 + 1) * sizeof(jschar));
        if (!chars) {
            Foreground::free_(ochars);
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

        ok = obj->lookupGeneric(cx, id, &obj2, &prop);
        if (!ok)
            goto error;

        /*
         * Convert id to a value and then to a string.  Decide early whether we
         * prefer get/set or old getter/setter syntax.
         */
        JSString *s = ToString(cx, IdToValue(id));
        if (!s || !(idstr = s->ensureLinear(cx))) {
            ok = JS_FALSE;
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
                    gsop[valcnt] = cx->runtime->atomState.getAtom;
                    valcnt++;
                }
                if (attrs & JSPROP_SETTER) {
                    doGet = false;
                    val[valcnt] = shape->setterValue();
                    gsop[valcnt] = cx->runtime->atomState.setAtom;
                    valcnt++;
                }
            }
            if (doGet) {
                valcnt = 1;
                gsop[0] = NULL;
                ok = obj->getGeneric(cx, id, &val[0]);
                if (!ok)
                    goto error;
            }
        }

        /*
         * If id is a string that's not an identifier, or if it's a negative
         * integer, then it must be quoted.
         */
        bool idIsLexicalIdentifier = IsIdentifier(idstr);
        if (JSID_IS_ATOM(id)
            ? !idIsLexicalIdentifier
            : (!JSID_IS_INT(id) || JSID_TO_INT(id) < 0)) {
            s = js_QuoteString(cx, idstr, jschar('\''));
            if (!s || !(idstr = s->ensureLinear(cx))) {
                ok = JS_FALSE;
                goto error;
            }
            vp->setString(idstr);                       /* local root */
        }
        idstrlength = idstr->length();
        idstrchars = idstr->getChars(cx);
        if (!idstrchars) {
            ok = JS_FALSE;
            goto error;
        }

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
            vchars = valstr->getChars(cx);
            if (!vchars) {
                ok = JS_FALSE;
                goto error;
            }
            vlength = valstr->length();

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

                uint8_t parenChomp = 0;
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
            chars = (jschar *) cx->realloc_((ochars = chars), curlen * sizeof(jschar));
            if (!chars) {
                chars = ochars;
                goto overflow;
            }

            if (comma) {
                chars[nchars++] = comma[0];
                chars[nchars++] = comma[1];
            }
            comma = ", ";

            if (gsop[j]) {
                gsoplength = gsop[j]->length();
                const jschar *gsopchars = gsop[j]->getChars(cx);
                if (!gsopchars)
                    goto overflow;
                js_strncpy(&chars[nchars], gsopchars, gsoplength);
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
                cx->free_(vsharp);
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
            Foreground::free_(chars);
        return false;
    }

    if (!chars) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
  make_string:
    str = js_NewString(cx, chars, nchars);
    if (!str) {
        cx->free_(chars);
        return false;
    }
    vp->setString(str);
    return true;

  overflow:
    cx->free_(vsharp);
    cx->free_(chars);
    chars = NULL;
    goto error;
}
#endif /* JS_HAS_TOSOURCE */

namespace js {

JSString *
obj_toStringHelper(JSContext *cx, JSObject *obj)
{
    if (obj->isProxy())
        return Proxy::obj_toString(cx, obj);

    StringBuffer sb(cx);
    const char *className = obj->getClass()->name;
    if (!sb.append("[object ") || !sb.appendInflated(className, strlen(className)) || 
        !sb.append("]"))
    {
        return NULL;
    }
    return sb.finishString();
}

JSObject *
NonNullObject(JSContext *cx, const Value &v)
{
    if (v.isPrimitive()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_NONNULL_OBJECT);
        return NULL;
    }
    return &v.toObject();
}

const char *
InformalValueTypeName(const Value &v)
{
    if (v.isObject())
        return v.toObject().getClass()->name;
    if (v.isString())
        return "string";
    if (v.isNumber())
        return "number";
    if (v.isBoolean())
        return "boolean";
    if (v.isNull())
        return "null";
    if (v.isUndefined())
        return "undefined";
    return "value";
}

} /* namespace js */

/* ES5 15.2.4.2.  Note steps 1 and 2 are errata. */
static JSBool
obj_toString(JSContext *cx, uintN argc, Value *vp)
{
    Value &thisv = vp[1];

    /* Step 1. */
    if (thisv.isUndefined()) {
        vp->setString(cx->runtime->atomState.objectUndefinedAtom);
        return true;
    }

    /* Step 2. */
    if (thisv.isNull()) {
        vp->setString(cx->runtime->atomState.objectNullAtom);
        return true;
    }

    /* Step 3. */
    JSObject *obj = ToObject(cx, &thisv);
    if (!obj)
        return false;

    /* Steps 4-5. */
    JSString *str = js::obj_toStringHelper(cx, obj);
    if (!str)
        return false;
    vp->setString(str);
    return true;
}

/* ES5 15.2.4.3. */
static JSBool
obj_toLocaleString(JSContext *cx, uintN argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    /* Step 1. */
    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;

    /* Steps 2-4. */
    return obj->callMethod(cx, ATOM_TO_JSID(cx->runtime->atomState.toStringAtom), 0, NULL, vp);
}

static JSBool
obj_valueOf(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;
    vp->setObject(*obj);
    return true;
}

/* We should be able to assert this for *any* fp->scopeChain(). */
static void
AssertInnerizedScopeChain(JSContext *cx, JSObject &scopeobj)
{
#ifdef DEBUG
    for (JSObject *o = &scopeobj; o; o = o->enclosingScope()) {
        if (JSObjectOp op = o->getClass()->ext.innerObject)
            JS_ASSERT(op(cx, o) == o);
    }
#endif
}

#ifndef EVAL_CACHE_CHAIN_LIMIT
# define EVAL_CACHE_CHAIN_LIMIT 4
#endif

static inline JSScript **
EvalCacheHash(JSContext *cx, JSLinearString *str)
{
    const jschar *s = str->chars();
    size_t n = str->length();

    if (n > 100)
        n = 100;
    uint32_t h;
    for (h = 0; n; s++, n--)
        h = JS_ROTATE_LEFT32(h, 4) ^ *s;

    h *= JS_GOLDEN_RATIO;
    h >>= 32 - JS_EVAL_CACHE_SHIFT;
    return &cx->compartment->evalCache[h];
}

static JS_ALWAYS_INLINE JSScript *
EvalCacheLookup(JSContext *cx, JSLinearString *str, StackFrame *caller, uintN staticLevel,
                JSPrincipals *principals, JSObject &scopeobj, JSScript **bucket)
{
    /*
     * Cache local eval scripts indexed by source qualified by scope.
     *
     * An eval cache entry should never be considered a hit unless its
     * strictness matches that of the new eval code. The existing code takes
     * care of this, because hits are qualified by the function from which
     * eval was called, whose strictness doesn't change. (We don't cache evals
     * in eval code, so the calling function corresponds to the calling script,
     * and its strictness never varies.) Scripts produced by calls to eval from
     * global code aren't cached.
     *
     * FIXME bug 620141: Qualify hits by calling script rather than function.
     * Then we wouldn't need the unintuitive !isEvalFrame() hack in EvalKernel
     * to avoid caching nested evals in functions (thus potentially mismatching
     * on strict mode), and we could cache evals in global code if desired.
     */
    uintN count = 0;
    JSScript **scriptp = bucket;

    JSVersion version = cx->findVersion();
    JSScript *script;
    while ((script = *scriptp) != NULL) {
        if (script->savedCallerFun &&
            script->staticLevel == staticLevel &&
            script->getVersion() == version &&
            !script->hasSingletons &&
            (script->principals == principals ||
             (principals && script->principals &&
              principals->subsume(principals, script->principals) &&
              script->principals->subsume(script->principals, principals)))) {
            /*
             * Get the prior (cache-filling) eval's saved caller function.
             * See frontend::CompileScript.
             */
            JSFunction *fun = script->getCallerFunction();

            if (fun == caller->fun()) {
                /*
                 * Get the source string passed for safekeeping in the atom map
                 * by the prior eval to frontend::CompileScript.
                 */
                JSAtom *src = script->atoms[0];

                if (src == str || EqualStrings(src, str)) {
                    /*
                     * Source matches. Make sure there are no inner objects
                     * which might use the wrong parent and/or call scope by
                     * reusing the previous eval's script. Skip the script's
                     * first object, which entrains the eval's scope.
                     */
                    JS_ASSERT(script->objects()->length >= 1);
                    if (script->objects()->length == 1 &&
                        !JSScript::isValidOffset(script->regexpsOffset)) {
                        JS_ASSERT(staticLevel == script->staticLevel);
                        *scriptp = script->evalHashLink();
                        script->evalHashLink() = NULL;
                        return script;
                    }
                }
            }
        }

        if (++count == EVAL_CACHE_CHAIN_LIMIT)
            return NULL;
        scriptp = &script->evalHashLink();
    }
    return NULL;
}

/*
 * There are two things we want to do with each script executed in EvalKernel:
 *  1. notify jsdbgapi about script creation/destruction
 *  2. add the script to the eval cache when EvalKernel is finished
 *
 * NB: Although the eval cache keeps a script alive wrt to the JS engine, from
 * a jsdbgapi user's perspective, we want each eval() to create and destroy a
 * script. This hides implementation details and means we don't have to deal
 * with calls to JS_GetScriptObject for scripts in the eval cache (currently,
 * script->object aliases script->evalHashLink()).
 */
class EvalScriptGuard
{
    JSContext *cx_;
    JSLinearString *str_;
    JSScript **bucket_;
    JSScript *script_;

  public:
    EvalScriptGuard(JSContext *cx, JSLinearString *str)
      : cx_(cx),
        str_(str),
        script_(NULL) {
        bucket_ = EvalCacheHash(cx, str);
    }

    ~EvalScriptGuard() {
        if (script_) {
            js_CallDestroyScriptHook(cx_, script_);
            script_->isActiveEval = false;
            script_->isCachedEval = true;
            script_->evalHashLink() = *bucket_;
            *bucket_ = script_;
        }
    }

    void lookupInEvalCache(StackFrame *caller, uintN staticLevel,
                           JSPrincipals *principals, JSObject &scopeobj) {
        if (JSScript *found = EvalCacheLookup(cx_, str_, caller, staticLevel,
                                              principals, scopeobj, bucket_)) {
            js_CallNewScriptHook(cx_, found, NULL);
            script_ = found;
            script_->isCachedEval = false;
            script_->isActiveEval = true;
        }
    }

    void setNewScript(JSScript *script) {
        /* NewScriptFromEmitter has already called js_CallNewScriptHook. */
        JS_ASSERT(!script_ && script);
        script_ = script;
        script_->isActiveEval = true;
    }

    bool foundScript() {
        return !!script_;
    }

    JSScript *script() const {
        JS_ASSERT(script_);
        return script_;
    }
};

/* Define subset of ExecuteType so that casting performs the injection. */
enum EvalType { DIRECT_EVAL = EXECUTE_DIRECT_EVAL, INDIRECT_EVAL = EXECUTE_INDIRECT_EVAL };

/*
 * Common code implementing direct and indirect eval.
 *
 * Evaluate call.argv[2], if it is a string, in the context of the given calling
 * frame, with the provided scope chain, with the semantics of either a direct
 * or indirect eval (see ES5 10.4.2).  If this is an indirect eval, scopeobj
 * must be a global object.
 *
 * On success, store the completion value in call.rval and return true.
 */
static bool
EvalKernel(JSContext *cx, const CallArgs &args, EvalType evalType, StackFrame *caller,
           JSObject &scopeobj)
{
    JS_ASSERT((evalType == INDIRECT_EVAL) == (caller == NULL));
    AssertInnerizedScopeChain(cx, scopeobj);

    if (!scopeobj.global().isRuntimeCodeGenEnabled(cx)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CSP_BLOCKED_EVAL);
        return false;
    }

    /* ES5 15.1.2.1 step 1. */
    if (args.length() < 1) {
        args.rval().setUndefined();
        return true;
    }
    if (!args[0].isString()) {
        args.rval() = args[0];
        return true;
    }
    JSString *str = args[0].toString();

    /* ES5 15.1.2.1 steps 2-8. */

    /*
     * Per ES5, indirect eval runs in the global scope. (eval is specified this
     * way so that the compiler can make assumptions about what bindings may or
     * may not exist in the current frame if it doesn't see 'eval'.)
     */
    uintN staticLevel;
    Value thisv;
    if (evalType == DIRECT_EVAL) {
        staticLevel = caller->script()->staticLevel + 1;

        /*
         * Direct calls to eval are supposed to see the caller's |this|. If we
         * haven't wrapped that yet, do so now, before we make a copy of it for
         * the eval code to use.
         */
        if (!ComputeThis(cx, caller))
            return false;
        thisv = caller->thisValue();

#ifdef DEBUG
        jsbytecode *callerPC = caller->pcQuadratic(cx);
        JS_ASSERT(callerPC && JSOp(*callerPC) == JSOP_EVAL);
#endif
    } else {
        JS_ASSERT(args.callee().global() == scopeobj);
        staticLevel = 0;

        /* Use the global as 'this', modulo outerization. */
        JSObject *thisobj = scopeobj.thisObject(cx);
        if (!thisobj)
            return false;
        thisv = ObjectValue(*thisobj);
    }

    JSLinearString *linearStr = str->ensureLinear(cx);
    if (!linearStr)
        return false;
    const jschar *chars = linearStr->chars();
    size_t length = linearStr->length();

    /*
     * If the eval string starts with '(' or '[' and ends with ')' or ']', it may be JSON.
     * Try the JSON parser first because it's much faster.  If the eval string
     * isn't JSON, JSON parsing will probably fail quickly, so little time
     * will be lost.
     *
     * Don't use the JSON parser if the caller is strict mode code, because in
     * strict mode object literals must not have repeated properties, and the
     * JSON parser cheerfully (and correctly) accepts them.  If you're parsing
     * JSON with eval and using strict mode, you deserve to be slow.
     */
    if (length > 2 &&
        ((chars[0] == '[' && chars[length - 1] == ']') ||
        (chars[0] == '(' && chars[length - 1] == ')')) &&
         (!caller || !caller->script()->strictModeCode))
    {
        /*
         * Remarkably, JavaScript syntax is not a superset of JSON syntax:
         * strings in JavaScript cannot contain the Unicode line and paragraph
         * terminator characters U+2028 and U+2029, but strings in JSON can.
         * Rather than force the JSON parser to handle this quirk when used by
         * eval, we simply don't use the JSON parser when either character
         * appears in the provided string.  See bug 657367.
         */
        for (const jschar *cp = &chars[1], *end = &chars[length - 2]; ; cp++) {
            if (*cp == 0x2028 || *cp == 0x2029)
                break;

            if (cp == end) {
                bool isArray = (chars[0] == '[');
                JSONParser parser(cx, isArray ? chars : chars + 1, isArray ? length : length - 2,
                                  JSONParser::StrictJSON, JSONParser::NoError);
                Value tmp;
                if (!parser.parse(&tmp))
                    return false;
                if (tmp.isUndefined())
                    break;
                args.rval() = tmp;
                return true;
            }
        }
    }

    EvalScriptGuard esg(cx, linearStr);

    JSPrincipals *principals = PrincipalsForCompiledCode(args, cx);

    if (evalType == DIRECT_EVAL && caller->isNonEvalFunctionFrame())
        esg.lookupInEvalCache(caller, staticLevel, principals, scopeobj);

    if (!esg.foundScript()) {
        uintN lineno;
        const char *filename;
        JSPrincipals *originPrincipals;
        CurrentScriptFileLineOrigin(cx, &filename, &lineno, &originPrincipals,
                                    evalType == DIRECT_EVAL ? CALLED_FROM_JSOP_EVAL
                                                            : NOT_CALLED_FROM_JSOP_EVAL);
        uint32_t tcflags = TCF_COMPILE_N_GO | TCF_COMPILE_FOR_EVAL;
        JSScript *compiled = frontend::CompileScript(cx, &scopeobj, caller,
                                                     principals, originPrincipals,
                                                     tcflags, chars, length, filename,
                                                     lineno, cx->findVersion(), linearStr,
                                                     staticLevel);
        if (!compiled)
            return false;

        esg.setNewScript(compiled);
    }

    return ExecuteKernel(cx, esg.script(), scopeobj, thisv, ExecuteType(evalType),
                         NULL /* evalInFrame */, &args.rval());
}

/*
 * We once supported a second argument to eval to use as the scope chain
 * when evaluating the code string.  Warn when such uses are seen so that
 * authors will know that support for eval(s, o) has been removed.
 */
static inline bool
WarnOnTooManyArgs(JSContext *cx, const CallArgs &args)
{
    if (args.length() > 1) {
        if (JSScript *script = cx->stack.currentScript()) {
            if (!script->warnedAboutTwoArgumentEval) {
                static const char TWO_ARGUMENT_WARNING[] =
                    "Support for eval(code, scopeObject) has been removed. "
                    "Use |with (scopeObject) eval(code);| instead.";
                if (!JS_ReportWarning(cx, TWO_ARGUMENT_WARNING))
                    return false;
                script->warnedAboutTwoArgumentEval = true;
            }
        } else {
            /*
             * In the case of an indirect call without a caller frame, avoid a
             * potential warning-flood by doing nothing.
             */
        }
    }

    return true;
}

namespace js {

/*
 * ES5 15.1.2.1.
 *
 * NB: This method handles only indirect eval.
 */
JSBool
eval(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return WarnOnTooManyArgs(cx, args) &&
           EvalKernel(cx, args, INDIRECT_EVAL, NULL, args.callee().global());
}

bool
DirectEval(JSContext *cx, const CallArgs &args)
{
    /* Direct eval can assume it was called from an interpreted frame. */
    StackFrame *caller = cx->fp();
    JS_ASSERT(caller->isScriptFrame());
    JS_ASSERT(IsBuiltinEvalForScope(&caller->scopeChain(), args.calleev()));
    JS_ASSERT(JSOp(*cx->regs().pc) == JSOP_EVAL);

    AutoFunctionCallProbe callProbe(cx, args.callee().toFunction(), caller->script());

    JSObject *scopeChain = GetScopeChain(cx, caller);
    if (!scopeChain)
        return false;

    if (!WarnOnTooManyArgs(cx, args))
        return false;

    return EvalKernel(cx, args, DIRECT_EVAL, caller, *scopeChain);
}

bool
IsBuiltinEvalForScope(JSObject *scopeChain, const Value &v)
{
    return scopeChain->global().getOriginalEval() == v;
}

bool
IsAnyBuiltinEval(JSFunction *fun)
{
    return fun->maybeNative() == eval;
}

JSPrincipals *
PrincipalsForCompiledCode(const CallReceiver &call, JSContext *cx)
{
    JS_ASSERT(IsAnyBuiltinEval(call.callee().toFunction()) ||
              IsBuiltinFunctionConstructor(call.callee().toFunction()));

    /*
     * To compute the principals of the compiled eval/Function code, we simply
     * use the callee's principals. To see why the caller's principals are
     * ignored, consider first that, in the capability-model we assume, the
     * high-privileged eval/Function should never have escaped to the
     * low-privileged caller. (For the Mozilla embedding, this is brute-enforced
     * by explicit filtering by wrappers.) Thus, the caller's privileges should
     * subsume the callee's.
     *
     * In the converse situation, where the callee has lower privileges than the
     * caller, we might initially guess that the caller would want to retain
     * their higher privileges in the generated code. However, since the
     * compiled code will be run with the callee's scope chain, this would make
     * fp->script()->compartment() != fp->compartment().
     */

    return call.callee().principals(cx);
}

}  /* namespace js */

#if JS_HAS_OBJ_WATCHPOINT

static JSBool
obj_watch_handler(JSContext *cx, JSObject *obj, jsid id, jsval old,
                  jsval *nvp, void *closure)
{
    JSObject *callable = (JSObject *) closure;
    if (JSPrincipals *watcher = callable->principals(cx)) {
        if (JSObject *scopeChain = cx->stack.currentScriptedScopeChain()) {
            if (JSPrincipals *subject = scopeChain->principals(cx)) {
                if (!watcher->subsume(watcher, subject)) {
                    /* Silently don't call the watch handler. */
                    return JS_TRUE;
                }
            }
        }
    }

    /* Avoid recursion on (obj, id) already being watched on cx. */
    AutoResolving resolving(cx, obj, id, AutoResolving::WATCH);
    if (resolving.alreadyStarted())
        return true;

    Value argv[] = { IdToValue(id), old, *nvp };
    return Invoke(cx, ObjectValue(*obj), ObjectOrNullValue(callable), ArrayLength(argv), argv, nvp);
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

    jsid propid;
    if (!ValueToId(cx, vp[2], &propid))
        return JS_FALSE;

    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;

    Value tmp;
    uintN attrs;
    if (!CheckAccess(cx, obj, propid, JSACC_WATCH, &tmp, &attrs))
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
    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;
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
    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;
    return js_HasOwnPropertyHelper(cx, obj->getOps()->lookupGeneric, argc, vp);
}

JSBool
js_HasOwnPropertyHelper(JSContext *cx, LookupGenericOp lookup, uintN argc,
                        Value *vp)
{
    jsid id;
    if (!ValueToId(cx, argc != 0 ? vp[2] : UndefinedValue(), &id))
        return JS_FALSE;

    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;
    JSObject *obj2;
    JSProperty *prop;
    if (obj->isProxy()) {
        bool has;
        if (!Proxy::hasOwn(cx, obj, id, &has))
            return false;
        vp->setBoolean(has);
        return true;
    }
    if (!js_HasOwnProperty(cx, lookup, obj, id, &obj2, &prop))
        return JS_FALSE;
    vp->setBoolean(!!prop);
    return JS_TRUE;
}

JSBool
js_HasOwnProperty(JSContext *cx, LookupGenericOp lookup, JSObject *obj, jsid id,
                  JSObject **objp, JSProperty **propp)
{
    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED | JSRESOLVE_DETECTING);
    if (!(lookup ? lookup : js_LookupProperty)(cx, obj, id, objp, propp))
        return false;
    if (!*propp)
        return true;

    if (*objp == obj)
        return true;

    JSObject *outer = NULL;
    if (JSObjectOp op = (*objp)->getClass()->ext.outerObject) {
        outer = op(cx, *objp);
        if (!outer)
            return false;
    }

    if (outer != *objp)
        *propp = NULL;
    return true;
}

/* ES5 15.2.4.6. */
static JSBool
obj_isPrototypeOf(JSContext *cx, uintN argc, Value *vp)
{
    /* Step 1. */
    if (argc < 1 || !vp[2].isObject()) {
        vp->setBoolean(false);
        return true;
    }

    /* Step 2. */
    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;

    /* Step 3. */
    vp->setBoolean(js_IsDelegate(cx, obj, vp[2]));
    return true;
}

/* ES5 15.2.4.7. */
static JSBool
obj_propertyIsEnumerable(JSContext *cx, uintN argc, Value *vp)
{
    /* Step 1. */
    jsid id;
    if (!ValueToId(cx, argc != 0 ? vp[2] : UndefinedValue(), &id))
        return false;

    /* Step 2. */
    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;

    /* Steps 3-5. */
    return js_PropertyIsEnumerable(cx, obj, id, vp);
}

JSBool
js_PropertyIsEnumerable(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    JSObject *pobj;
    JSProperty *prop;
    if (!obj->lookupGeneric(cx, id, &pobj, &prop))
        return false;

    if (!prop) {
        vp->setBoolean(false);
        return true;
    }

    /*
     * ECMA spec botch: return false unless hasOwnProperty. Leaving "own" out
     * of propertyIsEnumerable's name was a mistake.
     */
    if (pobj != obj) {
        vp->setBoolean(false);
        return true;
    }

    uintN attrs;
    if (!pobj->getGenericAttributes(cx, id, &attrs))
        return false;

    vp->setBoolean((attrs & JSPROP_ENUMERATE) != 0);
    return true;
}

#if OLD_GETTER_SETTER_METHODS

const char js_defineGetter_str[] = "__defineGetter__";
const char js_defineSetter_str[] = "__defineSetter__";
const char js_lookupGetter_str[] = "__lookupGetter__";
const char js_lookupSetter_str[] = "__lookupSetter__";

JS_FRIEND_API(JSBool)
js::obj_defineGetter(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!BoxNonStrictThis(cx, args))
        return false;
    JSObject *obj = &args.thisv().toObject();

    if (args.length() <= 1 || !js_IsCallable(args[1])) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_GETTER_OR_SETTER,
                             js_getter_str);
        return JS_FALSE;
    }
    PropertyOp getter = CastAsPropertyOp(&args[1].toObject());

    jsid id;
    if (!ValueToId(cx, args[0], &id))
        return JS_FALSE;
    if (!CheckRedeclaration(cx, obj, id, JSPROP_GETTER))
        return JS_FALSE;
    /*
     * Getters and setters are just like watchpoints from an access
     * control point of view.
     */
    Value junk;
    uintN attrs;
    if (!CheckAccess(cx, obj, id, JSACC_WATCH, &junk, &attrs))
        return JS_FALSE;
    args.rval().setUndefined();
    return obj->defineGeneric(cx, id, UndefinedValue(), getter, JS_StrictPropertyStub,
                              JSPROP_ENUMERATE | JSPROP_GETTER | JSPROP_SHARED);
}

JS_FRIEND_API(JSBool)
js::obj_defineSetter(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!BoxNonStrictThis(cx, args))
        return false;
    JSObject *obj = &args.thisv().toObject();

    if (args.length() <= 1 || !js_IsCallable(args[1])) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_GETTER_OR_SETTER,
                             js_setter_str);
        return JS_FALSE;
    }
    StrictPropertyOp setter = CastAsStrictPropertyOp(&args[1].toObject());

    jsid id;
    if (!ValueToId(cx, args[0], &id))
        return JS_FALSE;
    if (!CheckRedeclaration(cx, obj, id, JSPROP_SETTER))
        return JS_FALSE;
    /*
     * Getters and setters are just like watchpoints from an access
     * control point of view.
     */
    Value junk;
    uintN attrs;
    if (!CheckAccess(cx, obj, id, JSACC_WATCH, &junk, &attrs))
        return JS_FALSE;
    args.rval().setUndefined();
    return obj->defineGeneric(cx, id, UndefinedValue(), JS_PropertyStub, setter,
                              JSPROP_ENUMERATE | JSPROP_SETTER | JSPROP_SHARED);
}

static JSBool
obj_lookupGetter(JSContext *cx, uintN argc, Value *vp)
{
    jsid id;
    if (!ValueToId(cx, argc != 0 ? vp[2] : UndefinedValue(), &id))
        return JS_FALSE;
    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return JS_FALSE;
    JSObject *pobj;
    JSProperty *prop;
    if (!obj->lookupGeneric(cx, id, &pobj, &prop))
        return JS_FALSE;
    vp->setUndefined();
    if (prop) {
        if (pobj->isNative()) {
            Shape *shape = (Shape *) prop;
            if (shape->hasGetterValue())
                *vp = shape->getterValue();
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
    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return JS_FALSE;
    JSObject *pobj;
    JSProperty *prop;
    if (!obj->lookupGeneric(cx, id, &pobj, &prop))
        return JS_FALSE;
    vp->setUndefined();
    if (prop) {
        if (pobj->isNative()) {
            Shape *shape = (Shape *) prop;
            if (shape->hasSetterValue())
                *vp = shape->setterValue();
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

namespace js {

bool
NewPropertyDescriptorObject(JSContext *cx, const PropertyDescriptor *desc, Value *vp)
{
    if (!desc->obj) {
        vp->setUndefined();
        return true;
    }

    /* We have our own property, so start creating the descriptor. */
    PropDesc d;
    d.initFromPropertyDescriptor(*desc);
    if (!d.makeObject(cx))
        return false;
    *vp = d.pd;
    return true;
}

void
PropDesc::initFromPropertyDescriptor(const PropertyDescriptor &desc)
{
    pd.setUndefined();
    attrs = uint8_t(desc.attrs);
    JS_ASSERT_IF(attrs & JSPROP_READONLY, !(attrs & (JSPROP_GETTER | JSPROP_SETTER)));
    if (desc.attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
        hasGet = true;
        get = ((desc.attrs & JSPROP_GETTER) && desc.getter)
              ? CastAsObjectJsval(desc.getter)
              : UndefinedValue();
        hasSet = true;
        set = ((desc.attrs & JSPROP_SETTER) && desc.setter)
              ? CastAsObjectJsval(desc.setter)
              : UndefinedValue();
        hasValue = false;
        value.setUndefined();
        hasWritable = false;
    } else {
        hasGet = false;
        get.setUndefined();
        hasSet = false;
        set.setUndefined();
        hasValue = true;
        value = desc.value;
        hasWritable = true;
    }
    hasEnumerable = true;
    hasConfigurable = true;
}

bool
PropDesc::makeObject(JSContext *cx)
{
    JSObject *obj = NewBuiltinClassInstance(cx, &ObjectClass);
    if (!obj)
        return false;

    const JSAtomState &atomState = cx->runtime->atomState;
    if ((hasConfigurable &&
         !obj->defineProperty(cx, atomState.configurableAtom,
                              BooleanValue((attrs & JSPROP_PERMANENT) == 0))) ||
        (hasEnumerable &&
         !obj->defineProperty(cx, atomState.enumerableAtom,
                              BooleanValue((attrs & JSPROP_ENUMERATE) != 0))) ||
        (hasGet &&
         !obj->defineProperty(cx, atomState.getAtom, get)) ||
        (hasSet &&
         !obj->defineProperty(cx, atomState.setAtom, set)) ||
        (hasValue &&
         !obj->defineProperty(cx, atomState.valueAtom, value)) ||
        (hasWritable &&
         !obj->defineProperty(cx, atomState.writableAtom,
                              BooleanValue((attrs & JSPROP_READONLY) == 0))))
    {
        return false;
    }

    pd.setObject(*obj);
    return true;
}

bool
GetOwnPropertyDescriptor(JSContext *cx, JSObject *obj, jsid id, PropertyDescriptor *desc)
{
    if (obj->isProxy())
        return Proxy::getOwnPropertyDescriptor(cx, obj, id, false, desc);

    JSObject *pobj;
    JSProperty *prop;
    if (!js_HasOwnProperty(cx, obj->getOps()->lookupGeneric, obj, id, &pobj, &prop))
        return false;
    if (!prop) {
        desc->obj = NULL;
        return true;
    }

    bool doGet = true;
    if (pobj->isNative()) {
        Shape *shape = (Shape *) prop;
        desc->attrs = shape->attributes();
        if (desc->attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
            doGet = false;
            if (desc->attrs & JSPROP_GETTER)
                desc->getter = CastAsPropertyOp(shape->getterObject());
            if (desc->attrs & JSPROP_SETTER)
                desc->setter = CastAsStrictPropertyOp(shape->setterObject());
        }
    } else {
        if (!pobj->getGenericAttributes(cx, id, &desc->attrs))
            return false;
    }

    if (doGet && !obj->getGeneric(cx, id, &desc->value))
        return false;

    desc->obj = obj;
    return true;
}

bool
GetOwnPropertyDescriptor(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    AutoPropertyDescriptorRooter desc(cx);
    return GetOwnPropertyDescriptor(cx, obj, id, &desc) &&
           NewPropertyDescriptorObject(cx, &desc, vp);
}

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
    return GetOwnPropertyDescriptor(cx, obj, nameidr.id(), vp);
}

static JSBool
obj_keys(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.keys", &obj))
        return false;

    AutoIdVector props(cx);
    if (!GetPropertyNames(cx, obj, JSITER_OWNONLY, &props))
        return false;

    AutoValueVector vals(cx);
    if (!vals.reserve(props.length()))
        return false;
    for (size_t i = 0, len = props.length(); i < len; i++) {
        jsid id = props[i];
        if (JSID_IS_STRING(id)) {
            JS_ALWAYS_TRUE(vals.append(StringValue(JSID_TO_STRING(id))));
        } else if (JSID_IS_INT(id)) {
            JSString *str = js_IntToString(cx, JSID_TO_INT(id));
            if (!str)
                return false;
            vals.infallibleAppend(StringValue(str));
        } else {
            JS_ASSERT(JSID_IS_OBJECT(id));
        }
    }

    JS_ASSERT(props.length() <= UINT32_MAX);
    JSObject *aobj = NewDenseCopiedArray(cx, jsuint(vals.length()), vals.begin());
    if (!aobj)
        return false;
    vp->setObject(*aobj);

    return true;
}

static bool
HasProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp, bool *foundp)
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
     * js::GetOwnPropertyDescriptor, as the getter or setter callable object.
     */
    return !!obj->getGeneric(cx, id, vp);
}

PropDesc::PropDesc()
  : pd(UndefinedValue()),
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
PropDesc::initialize(JSContext *cx, const Value &origval, bool checkAccessors)
{
    Value v = origval;

    /* 8.10.5 step 1 */
    if (v.isPrimitive()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_NONNULL_OBJECT);
        return false;
    }
    JSObject *desc = &v.toObject();

    /* Make a copy of the descriptor. We might need it later. */
    pd = v;

    /* Start with the proper defaults. */
    attrs = JSPROP_PERMANENT | JSPROP_READONLY;

    bool found;

    /* 8.10.5 step 3 */
#ifdef __GNUC__ /* quell GCC overwarning */
    found = false;
#endif
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
        hasGet = true;
        get = v;
        attrs |= JSPROP_GETTER | JSPROP_SHARED;
        attrs &= ~JSPROP_READONLY;
        if (checkAccessors && !checkGetter(cx))
            return false;
    }

    /* 8.10.7 step 8 */
    if (!HasProperty(cx, desc, ATOM_TO_JSID(cx->runtime->atomState.setAtom), &v, &found))
        return false;
    if (found) {
        hasSet = true;
        set = v;
        attrs |= JSPROP_SETTER | JSPROP_SHARED;
        attrs &= ~JSPROP_READONLY;
        if (checkAccessors && !checkSetter(cx))
            return false;
    }

    /* 8.10.7 step 9 */
    if ((hasGet || hasSet) && (hasValue || hasWritable)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INVALID_DESCRIPTOR);
        return false;
    }

    JS_ASSERT_IF(attrs & JSPROP_READONLY, !(attrs & (JSPROP_GETTER | JSPROP_SETTER)));

    return true;
}

static JSBool
Reject(JSContext *cx, uintN errorNumber, bool throwError, jsid id, bool *rval)
{
    if (throwError) {
        jsid idstr;
        if (!js_ValueToStringId(cx, IdToValue(id), &idstr))
           return JS_FALSE;
        JSAutoByteString bytes(cx, JSID_TO_STRING(idstr));
        if (!bytes)
            return JS_FALSE;
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, errorNumber, bytes.ptr());
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
DefinePropertyOnObject(JSContext *cx, JSObject *obj, const jsid &id, const PropDesc &desc,
                       bool throwError, bool *rval)
{
    /* 8.12.9 step 1. */
    JSProperty *current;
    JSObject *obj2;
    JS_ASSERT(!obj->getOps()->lookupGeneric);
    if (!js_HasOwnProperty(cx, NULL, obj, id, &obj2, &current))
        return JS_FALSE;

    JS_ASSERT(!obj->getOps()->defineProperty);

    /* 8.12.9 steps 2-4. */
    if (!current) {
        if (!obj->isExtensible())
            return Reject(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE, throwError, rval);

        *rval = true;

        if (desc.isGenericDescriptor() || desc.isDataDescriptor()) {
            JS_ASSERT(!obj->getOps()->defineProperty);
            return js_DefineProperty(cx, obj, id, &desc.value,
                                     JS_PropertyStub, JS_StrictPropertyStub, desc.attrs);
        }

        JS_ASSERT(desc.isAccessorDescriptor());

        /*
         * Getters and setters are just like watchpoints from an access
         * control point of view.
         */
        Value dummy;
        uintN dummyAttrs;
        if (!CheckAccess(cx, obj, id, JSACC_WATCH, &dummy, &dummyAttrs))
            return JS_FALSE;

        Value tmp = UndefinedValue();
        return js_DefineProperty(cx, obj, id, &tmp,
                                 desc.getter(), desc.setter(), desc.attrs);
    }

    /* 8.12.9 steps 5-6 (note 5 is merely a special case of 6). */
    Value v = UndefinedValue();

    JS_ASSERT(obj == obj2);

    const Shape *shape = reinterpret_cast<Shape *>(current);
    do {
        if (desc.isAccessorDescriptor()) {
            if (!shape->isAccessorDescriptor())
                break;

            if (desc.hasGet) {
                JSBool same;
                if (!SameValue(cx, desc.getterValue(), shape->getterOrUndefined(), &same))
                    return JS_FALSE;
                if (!same)
                    break;
            }

            if (desc.hasSet) {
                JSBool same;
                if (!SameValue(cx, desc.setterValue(), shape->setterOrUndefined(), &same))
                    return JS_FALSE;
                if (!same)
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
                 * We must rule out a non-configurable js::PropertyOp-guarded
                 * property becoming a writable unguarded data property, since
                 * such a property can have its value changed to one the getter
                 * and setter preclude.
                 *
                 * A desc lacking writable but with value is a data descriptor
                 * and we must reject it as if it had writable: true if current
                 * is writable.
                 */
                if (!shape->configurable() &&
                    (!shape->hasDefaultGetter() || !shape->hasDefaultSetter()) &&
                    desc.isDataDescriptor() &&
                    (desc.hasWritable ? desc.writable() : shape->writable()))
                {
                    return Reject(cx, JSMSG_CANT_REDEFINE_PROP, throwError, id, rval);
                }

                if (!js_NativeGet(cx, obj, obj2, shape, JSGET_NO_METHOD_BARRIER, &v))
                    return JS_FALSE;
            }

            if (desc.isDataDescriptor()) {
                if (!shape->isDataDescriptor())
                    break;

                JSBool same;
                if (desc.hasValue) {
                    if (!SameValue(cx, desc.value, v, &same))
                        return JS_FALSE;
                    if (!same) {
                        /*
                         * Insist that a non-configurable js::PropertyOp data
                         * property is frozen at exactly the last-got value.
                         *
                         * Duplicate the first part of the big conjunction that
                         * we tested above, rather than add a local bool flag.
                         * Likewise, don't try to keep shape->writable() in a
                         * flag we veto from true to false for non-configurable
                         * PropertyOp-based data properties and test before the
                         * SameValue check later on in order to re-use that "if
                         * (!SameValue) Reject" logic.
                         *
                         * This function is large and complex enough that it
                         * seems best to repeat a small bit of code and return
                         * Reject(...) ASAP, instead of being clever.
                         */
                        if (!shape->configurable() &&
                            (!shape->hasDefaultGetter() || !shape->hasDefaultSetter()))
                        {
                            return Reject(cx, JSMSG_CANT_REDEFINE_PROP, throwError, id, rval);
                        }
                        break;
                    }
                }
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
            return Reject(cx, JSMSG_CANT_REDEFINE_PROP, throwError, id, rval);
        }
    }

    bool callDelProperty = false;

    if (desc.isGenericDescriptor()) {
        /* 8.12.9 step 8, no validation required */
    } else if (desc.isDataDescriptor() != shape->isDataDescriptor()) {
        /* 8.12.9 step 9. */
        if (!shape->configurable())
            return Reject(cx, JSMSG_CANT_REDEFINE_PROP, throwError, id, rval);
    } else if (desc.isDataDescriptor()) {
        /* 8.12.9 step 10. */
        JS_ASSERT(shape->isDataDescriptor());
        if (!shape->configurable() && !shape->writable()) {
            if (desc.hasWritable && desc.writable())
                return Reject(cx, JSMSG_CANT_REDEFINE_PROP, throwError, id, rval);
            if (desc.hasValue) {
                JSBool same;
                if (!SameValue(cx, desc.value, v, &same))
                    return JS_FALSE;
                if (!same)
                    return Reject(cx, JSMSG_CANT_REDEFINE_PROP, throwError, id, rval);
            }
        }

        callDelProperty = !shape->hasDefaultGetter() || !shape->hasDefaultSetter();
    } else {
        /* 8.12.9 step 11. */
        JS_ASSERT(desc.isAccessorDescriptor() && shape->isAccessorDescriptor());
        if (!shape->configurable()) {
            if (desc.hasSet) {
                JSBool same;
                if (!SameValue(cx, desc.setterValue(), shape->setterOrUndefined(), &same))
                    return JS_FALSE;
                if (!same)
                    return Reject(cx, JSMSG_CANT_REDEFINE_PROP, throwError, id, rval);
            }

            if (desc.hasGet) {
                JSBool same;
                if (!SameValue(cx, desc.getterValue(), shape->getterOrUndefined(), &same))
                    return JS_FALSE;
                if (!same)
                    return Reject(cx, JSMSG_CANT_REDEFINE_PROP, throwError, id, rval);
            }
        }
    }

    /* 8.12.9 step 12. */
    uintN attrs;
    PropertyOp getter;
    StrictPropertyOp setter;
    if (desc.isGenericDescriptor()) {
        uintN changed = 0;
        if (desc.hasConfigurable)
            changed |= JSPROP_PERMANENT;
        if (desc.hasEnumerable)
            changed |= JSPROP_ENUMERATE;

        attrs = (shape->attributes() & ~changed) | (desc.attrs & changed);
        if (shape->isMethod()) {
            JS_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));
            getter = JS_PropertyStub;
            setter = JS_StrictPropertyStub;
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
        /* Watch out for accessor -> data transformations here. */
        if (!desc.hasWritable && shape->isDataDescriptor())
            unchanged |= JSPROP_READONLY;

        if (desc.hasValue)
            v = desc.value;
        attrs = (desc.attrs & ~unchanged) | (shape->attributes() & unchanged);
        getter = JS_PropertyStub;
        setter = JS_StrictPropertyStub;
    } else {
        JS_ASSERT(desc.isAccessorDescriptor());

        /*
         * Getters and setters are just like watchpoints from an access
         * control point of view.
         */
        Value dummy;
        if (!CheckAccess(cx, obj2, id, JSACC_WATCH, &dummy, &attrs))
             return JS_FALSE;

        JS_ASSERT_IF(shape->isMethod(), !(attrs & (JSPROP_GETTER | JSPROP_SETTER)));

        /* 8.12.9 step 12. */
        uintN changed = 0;
        if (desc.hasConfigurable)
            changed |= JSPROP_PERMANENT;
        if (desc.hasEnumerable)
            changed |= JSPROP_ENUMERATE;
        if (desc.hasGet)
            changed |= JSPROP_GETTER | JSPROP_SHARED | JSPROP_READONLY;
        if (desc.hasSet)
            changed |= JSPROP_SETTER | JSPROP_SHARED | JSPROP_READONLY;

        attrs = (desc.attrs & changed) | (shape->attributes() & ~changed);
        if (desc.hasGet) {
            getter = desc.getter();
        } else {
            getter = (shape->isMethod() || (shape->hasDefaultGetter() && !shape->hasGetterValue()))
                     ? JS_PropertyStub
                     : shape->getter();
        }
        if (desc.hasSet) {
            setter = desc.setter();
        } else {
            setter = (shape->hasDefaultSetter() && !shape->hasSetterValue())
                     ? JS_StrictPropertyStub
                     : shape->setter();
        }
    }

    *rval = true;

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
        Value dummy = UndefinedValue();
        if (!CallJSPropertyOp(cx, obj2->getClass()->delProperty, obj2, id, &dummy))
            return false;
    }

    return js_DefineProperty(cx, obj, id, &v, getter, setter, attrs);
}

static JSBool
DefinePropertyOnArray(JSContext *cx, JSObject *obj, const jsid &id, const PropDesc &desc,
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

    if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
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

    uint32_t index;
    if (js_IdIsIndex(id, &index)) {
        /*
        // Disabled until we support defining "length":
        if (index >= oldLen && lengthPropertyNotWritable())
            return ThrowTypeError(cx, JSMSG_CANT_APPEND_TO_ARRAY);
         */
        if (!DefinePropertyOnObject(cx, obj, id, desc, false, rval))
            return JS_FALSE;
        if (!*rval)
            return Reject(cx, obj, JSMSG_CANT_DEFINE_ARRAY_INDEX, throwError, rval);

        if (index >= oldLen) {
            JS_ASSERT(index != UINT32_MAX);
            obj->setArrayLength(cx, index + 1);
        }

        *rval = true;
        return JS_TRUE;
    }

    return DefinePropertyOnObject(cx, obj, id, desc, throwError, rval);
}

namespace js {

bool
DefineProperty(JSContext *cx, JSObject *obj, const jsid &id, const PropDesc &desc, bool throwError,
               bool *rval)
{
    if (obj->isArray())
        return DefinePropertyOnArray(cx, obj, id, desc, throwError, rval);

    if (obj->getOps()->lookupGeneric) {
        if (obj->isProxy())
            return Proxy::defineProperty(cx, obj, id, desc.pd);
        return Reject(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE, throwError, rval);
    }

    return DefinePropertyOnObject(cx, obj, id, desc, throwError, rval);
}

} /* namespace js */

JSBool
js_DefineOwnProperty(JSContext *cx, JSObject *obj, jsid id, const Value &descriptor, JSBool *bp)
{
    AutoPropDescArrayRooter descs(cx);
    PropDesc *desc = descs.append();
    if (!desc || !desc->initialize(cx, descriptor))
        return false;

    bool rval;
    if (!DefineProperty(cx, obj, id, *desc, true, &rval))
        return false;
    *bp = !!rval;
    return true;
}

/* ES5 15.2.3.6: Object.defineProperty(O, P, Attributes) */
static JSBool
obj_defineProperty(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.defineProperty", &obj))
        return false;

    jsid id;
    if (!ValueToId(cx, argc >= 2 ? vp[3] : UndefinedValue(), &id))
        return JS_FALSE;

    const Value descval = argc >= 3 ? vp[4] : UndefinedValue();

    JSBool junk;
    if (!js_DefineOwnProperty(cx, obj, id, descval, &junk))
        return false;

    vp->setObject(*obj);
    return true;
}

namespace js {

bool
ReadPropertyDescriptors(JSContext *cx, JSObject *props, bool checkAccessors,
                        AutoIdVector *ids, AutoPropDescArrayRooter *descs)
{
    if (!GetPropertyNames(cx, props, JSITER_OWNONLY, ids))
        return false;

    for (size_t i = 0, len = ids->length(); i < len; i++) {
        jsid id = (*ids)[i];
        PropDesc* desc = descs->append();
        Value v;
        if (!desc || !props->getGeneric(cx, id, &v) || !desc->initialize(cx, v, checkAccessors))
            return false;
    }
    return true;
}

} /* namespace js */

static bool
DefineProperties(JSContext *cx, JSObject *obj, JSObject *props)
{
    AutoIdVector ids(cx);
    AutoPropDescArrayRooter descs(cx);
    if (!ReadPropertyDescriptors(cx, props, true, &ids, &descs))
        return false;

    bool dummy;
    for (size_t i = 0, len = ids.length(); i < len; i++) {
        if (!DefineProperty(cx, obj, ids[i], descs[i], true, &dummy))
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
obj_defineProperties(JSContext *cx, uintN argc, Value *vp)
{
    /* Steps 1 and 7. */
    JSObject *obj;
    if (!GetFirstArgumentAsObject(cx, argc, vp, "Object.defineProperties", &obj))
        return false;
    vp->setObject(*obj);

    /* Step 2. */
    if (argc < 2) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "Object.defineProperties", "0", "s");
        return false;
    }
    JSObject *props = ToObject(cx, &vp[3]);
    if (!props)
        return false;

    /* Steps 3-6. */
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

    JSObject *proto = v.toObjectOrNull();
    if (proto && proto->isXML()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_XML_PROTO_FORBIDDEN);
        return false;
    }

    /*
     * Use the callee's global as the parent of the new object to avoid dynamic
     * scoping (i.e., using the caller's global).
     */
    JSObject *obj = NewObjectWithGivenProto(cx, &ObjectClass, proto, &vp->toObject().global());
    if (!obj)
        return JS_FALSE;
    vp->setObject(*obj); /* Root and prepare for eventual return. */

    /* Don't track types or array-ness for objects created here. */
    MarkTypeObjectUnknownProperties(cx, obj->type());

    /* 15.2.3.5 step 4. */
    if (argc > 1 && !vp[3].isUndefined()) {
        if (vp[3].isPrimitive()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_NONNULL_OBJECT);
            return JS_FALSE;
        }

        if (!DefineProperties(cx, obj, &vp[3].toObject()))
            return JS_FALSE;
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
             JSString *str = js_IntToString(cx, JSID_TO_INT(id));
             if (!str)
                 return false;
             vals[i].setString(str);
         } else if (JSID_IS_ATOM(id)) {
             vals[i].setString(JSID_TO_STRING(id));
         } else {
             vals[i].setObject(*JSID_TO_OBJECT(id));
         }
    }

    JSObject *aobj = NewDenseCopiedArray(cx, vals.length(), vals.begin());
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
    if (!obj->isExtensible())
        return true;

    AutoIdVector props(cx);
    return obj->preventExtensions(cx, &props);
}

bool
JSObject::sealOrFreeze(JSContext *cx, ImmutabilityType it)
{
    assertSameCompartment(cx, this);
    JS_ASSERT(it == SEAL || it == FREEZE);

    RootedVarObject self(cx, this);

    AutoIdVector props(cx);
    if (isExtensible()) {
        if (!preventExtensions(cx, &props))
            return false;
    } else {
        if (!GetPropertyNames(cx, this, JSITER_HIDDEN | JSITER_OWNONLY, &props))
            return false;
    }

    /* preventExtensions must slowify dense arrays, so we can assign to holes without checks. */
    JS_ASSERT(!self->isDenseArray());

    for (size_t i = 0, len = props.length(); i < len; i++) {
        jsid id = props[i];

        uintN attrs;
        if (!self->getGenericAttributes(cx, id, &attrs))
            return false;

        /* Make all attributes permanent; if freezing, make data attributes read-only. */
        uintN new_attrs;
        if (it == FREEZE && !(attrs & (JSPROP_GETTER | JSPROP_SETTER)))
            new_attrs = JSPROP_PERMANENT | JSPROP_READONLY;
        else
            new_attrs = JSPROP_PERMANENT;

        /* If we already have the attributes we need, skip the setAttributes call. */
        if ((attrs | new_attrs) == attrs)
            continue;

        attrs |= new_attrs;
        if (!self->setGenericAttributes(cx, id, &attrs))
            return false;
    }

    return true;
}

bool
JSObject::isSealedOrFrozen(JSContext *cx, ImmutabilityType it, bool *resultp)
{
    if (isExtensible()) {
        *resultp = false;
        return true;
    }

    AutoIdVector props(cx);
    if (!GetPropertyNames(cx, this, JSITER_HIDDEN | JSITER_OWNONLY, &props))
        return false;

    for (size_t i = 0, len = props.length(); i < len; i++) {
        jsid id = props[i];

        uintN attrs;
        if (!getGenericAttributes(cx, id, &attrs))
            return false;

        /*
         * If the property is configurable, this object is neither sealed nor
         * frozen. If the property is a writable data property, this object is
         * not frozen.
         */
        if (!(attrs & JSPROP_PERMANENT) ||
            (it == FREEZE && !(attrs & (JSPROP_READONLY | JSPROP_GETTER | JSPROP_SETTER))))
        {
            *resultp = false;
            return true;
        }
    }

    /* All properties checked out. This object is sealed/frozen. */
    *resultp = true;
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

    bool frozen;
    if (!obj->isFrozen(cx, &frozen))
        return false;
    vp->setBoolean(frozen);
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

    bool sealed;
    if (!obj->isSealed(cx, &sealed))
        return false;
    vp->setBoolean(sealed);
    return true;
}

#if JS_HAS_OBJ_WATCHPOINT
const char js_watch_str[] = "watch";
const char js_unwatch_str[] = "unwatch";
#endif
const char js_hasOwnProperty_str[] = "hasOwnProperty";
const char js_isPrototypeOf_str[] = "isPrototypeOf";
const char js_propertyIsEnumerable_str[] = "propertyIsEnumerable";

JSFunctionSpec object_methods[] = {
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
    JS_FN(js_defineGetter_str,         js::obj_defineGetter,        2,0),
    JS_FN(js_defineSetter_str,         js::obj_defineSetter,        2,0),
    JS_FN(js_lookupGetter_str,         obj_lookupGetter,            1,0),
    JS_FN(js_lookupSetter_str,         obj_lookupSetter,            1,0),
#endif
    JS_FS_END
};

JSFunctionSpec object_static_methods[] = {
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
        gc::AllocKind kind = NewObjectGCKind(cx, &ObjectClass);
        obj = NewBuiltinClassInstance(cx, &ObjectClass, kind);
        if (!obj)
            return JS_FALSE;
        TypeObject *type = GetTypeCallerInitObject(cx, JSProto_Object);
        if (!type)
            return JS_FALSE;
        obj->setType(type);
    }
    vp->setObject(*obj);
    return JS_TRUE;
}

static inline JSObject *
NewObject(JSContext *cx, Class *clasp, types::TypeObject *type, JSObject *parent,
          gc::AllocKind kind)
{
    JS_ASSERT(clasp != &ArrayClass);
    JS_ASSERT_IF(clasp == &FunctionClass,
                 kind == JSFunction::FinalizeKind || kind == JSFunction::ExtendedFinalizeKind);

    RootTypeObject typeRoot(cx, &type);

    RootedVarShape shape(cx);
    shape = EmptyShape::getInitialShape(cx, clasp, type->proto, parent, kind);
    if (!shape)
        return NULL;

    HeapValue *slots;
    if (!PreallocateObjectDynamicSlots(cx, shape, &slots))
        return NULL;

    JSObject *obj = JSObject::create(cx, kind, shape, typeRoot, slots);
    if (!obj)
        return NULL;

    Probes::createObject(cx, obj);
    return obj;
}

JSObject *
js::NewObjectWithGivenProto(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
                            gc::AllocKind kind)
{
    if (CanBeFinalizedInBackground(kind, clasp))
        kind = GetBackgroundAllocKind(kind);

    NewObjectCache &cache = cx->compartment->newObjectCache;

    NewObjectCache::EntryIndex entry = -1;
    if (proto && (!parent || parent == proto->getParent()) && !proto->isGlobal()) {
        if (cache.lookupProto(clasp, proto, kind, &entry))
            return cache.newObjectFromHit(cx, entry);
    }

    RootObject protoRoot(cx, &proto);
    RootObject parentRoot(cx, &parent);

    types::TypeObject *type = proto ? proto->getNewType(cx) : cx->compartment->getEmptyType(cx);
    if (!type)
        return NULL;

    /*
     * Default parent to the parent of the prototype, which was set from
     * the parent of the prototype's constructor.
     */
    if (!parent && proto)
        parent = proto->getParent();

    JSObject *obj = NewObject(cx, clasp, type, parent, kind);
    if (!obj)
        return NULL;

    if (entry != -1 && !obj->hasDynamicSlots())
        cache.fillProto(entry, clasp, proto, kind, obj);

    return obj;
}

JSObject *
js::NewObjectWithClassProto(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
                            gc::AllocKind kind)
{
    if (proto)
        return NewObjectWithGivenProto(cx, clasp, proto, parent, kind);

    if (CanBeFinalizedInBackground(kind, clasp))
        kind = GetBackgroundAllocKind(kind);

    if (!parent)
        parent = GetCurrentGlobal(cx);

    /*
     * Use the object cache, except for classes without a cached proto key.
     * On these objects, FindProto will do a dynamic property lookup to get
     * global[className].prototype, where changes to either the className or
     * prototype property would render the cached lookup incorrect. For classes
     * with a proto key, the prototype created during class initialization is
     * stored in an immutable slot on the global (except for ClearScope, which
     * will flush the new object cache).
     */
    JSProtoKey protoKey = GetClassProtoKey(clasp);

    NewObjectCache &cache = cx->compartment->newObjectCache;

    NewObjectCache::EntryIndex entry = -1;
    if (parent->isGlobal() && protoKey != JSProto_Null) {
        if (cache.lookupGlobal(clasp, &parent->asGlobal(), kind, &entry))
            return cache.newObjectFromHit(cx, entry);
    }

    RootObject parentRoot(cx, &parent);

    if (!FindProto(cx, clasp, parentRoot, &proto))
        return NULL;

    types::TypeObject *type = proto->getNewType(cx);
    if (!type)
        return NULL;

    JSObject *obj = NewObject(cx, clasp, type, parent, kind);
    if (!obj)
        return NULL;

    if (entry != -1 && !obj->hasDynamicSlots())
        cache.fillGlobal(entry, clasp, &parent->asGlobal(), kind, obj);

    return obj;
}

JSObject *
js::NewObjectWithType(JSContext *cx, types::TypeObject *type, JSObject *parent, gc::AllocKind kind)
{
    JS_ASSERT(type->proto->hasNewType(type));
    JS_ASSERT(parent);

    if (CanBeFinalizedInBackground(kind, &ObjectClass))
        kind = GetBackgroundAllocKind(kind);

    NewObjectCache &cache = cx->compartment->newObjectCache;

    NewObjectCache::EntryIndex entry = -1;
    if (parent == type->proto->getParent()) {
        if (cache.lookupType(&ObjectClass, type, kind, &entry))
            return cache.newObjectFromHit(cx, entry);
    }

    JSObject *obj = NewObject(cx, &ObjectClass, type, parent, kind);
    if (!obj)
        return NULL;

    if (entry != -1 && !obj->hasDynamicSlots())
        cache.fillType(entry, &ObjectClass, type, kind, obj);

    return obj;
}

JSObject *
js::NewReshapedObject(JSContext *cx, TypeObject *type, JSObject *parent,
                      gc::AllocKind kind, const Shape *shape)
{
    JSObject *res = NewObjectWithType(cx, type, parent, kind);
    if (!res)
        return NULL;

    if (shape->isEmptyShape())
        return res;

    /* Get all the ids in the object, in order. */
    js::AutoIdVector ids(cx);
    for (unsigned i = 0; i <= shape->slot(); i++) {
        if (!ids.append(JSID_VOID))
            return NULL;
    }
    const js::Shape *nshape = shape;
    while (!nshape->isEmptyShape()) {
        ids[nshape->slot()] = nshape->propid();
        nshape = nshape->previous();
    }

    /* Construct the new shape. */
    for (unsigned i = 0; i < ids.length(); i++) {
        if (!DefineNativeProperty(cx, res, ids[i], js::UndefinedValue(), NULL, NULL,
                                  JSPROP_ENUMERATE, 0, 0, DNP_SKIP_TYPE)) {
            return NULL;
        }
    }
    JS_ASSERT(!res->inDictionaryMode());

    return res;
}

JSObject*
js_CreateThis(JSContext *cx, JSObject *callee)
{
    Class *clasp = callee->getClass();

    Class *newclasp = &ObjectClass;
    if (clasp == &FunctionClass) {
        JSFunction *fun = callee->toFunction();
        if (fun->isNative() && fun->u.n.clasp)
            newclasp = fun->u.n.clasp;
    }

    Value protov;
    if (!callee->getProperty(cx, cx->runtime->atomState.classPrototypeAtom, &protov))
        return NULL;

    JSObject *proto = protov.isObjectOrNull() ? protov.toObjectOrNull() : NULL;
    JSObject *parent = callee->getParent();
    gc::AllocKind kind = NewObjectGCKind(cx, newclasp);
    return NewObjectWithClassProto(cx, newclasp, proto, parent, kind);
}

static inline JSObject *
CreateThisForFunctionWithType(JSContext *cx, types::TypeObject *type, JSObject *parent)
{
    if (type->newScript) {
        /*
         * Make an object with the type's associated finalize kind and shape,
         * which reflects any properties that will definitely be added to the
         * object before it is read from.
         */
        gc::AllocKind kind = type->newScript->allocKind;
        JSObject *res = NewObjectWithType(cx, type, parent, kind);
        if (res)
            JS_ALWAYS_TRUE(res->setLastProperty(cx, (Shape *) type->newScript->shape.get()));
        return res;
    }

    gc::AllocKind kind = NewObjectGCKind(cx, &ObjectClass);
    return NewObjectWithType(cx, type, parent, kind);
}

JSObject *
js_CreateThisForFunctionWithProto(JSContext *cx, JSObject *callee, JSObject *proto)
{
    JSObject *res;

    if (proto) {
        types::TypeObject *type = proto->getNewType(cx, callee->toFunction());
        if (!type)
            return NULL;
        res = CreateThisForFunctionWithType(cx, type, callee->getParent());
    } else {
        gc::AllocKind kind = NewObjectGCKind(cx, &ObjectClass);
        res = NewObjectWithClassProto(cx, &ObjectClass, proto, callee->getParent(), kind);
    }

    if (res && cx->typeInferenceEnabled())
        TypeScript::SetThis(cx, callee->toFunction()->script(), types::Type::ObjectType(res));

    return res;
}

JSObject *
js_CreateThisForFunction(JSContext *cx, JSObject *callee, bool newType)
{
    Value protov;
    if (!callee->getProperty(cx, cx->runtime->atomState.classPrototypeAtom, &protov))
        return NULL;
    JSObject *proto;
    if (protov.isObject())
        proto = &protov.toObject();
    else
        proto = NULL;
    JSObject *obj = js_CreateThisForFunctionWithProto(cx, callee, proto);

    if (obj && newType) {
        /*
         * Reshape the object and give it a (lazily instantiated) singleton
         * type before passing it as the 'this' value for the call.
         */
        obj->clear(cx);
        if (!obj->setSingletonType(cx))
            return NULL;

        JSScript *calleeScript = callee->toFunction()->script();
        TypeScript::SetThis(cx, calleeScript, types::Type::ObjectType(obj));
    }

    return obj;
}

/*
 * Given pc pointing after a property accessing bytecode, return true if the
 * access is "object-detecting" in the sense used by web scripts, e.g., when
 * checking whether document.all is defined.
 */
JSBool
Detecting(JSContext *cx, jsbytecode *pc)
{
    jsbytecode *endpc;
    JSOp op;
    JSAtom *atom;

    JSScript *script = cx->stack.currentScript();
    endpc = script->code + script->length;
    for (;; pc += js_CodeSpec[op].length) {
        JS_ASSERT(script->code <= pc && pc < endpc);

        /* General case: a branch or equality op follows the access. */
        op = JSOp(*pc);
        if (js_CodeSpec[op].format & JOF_DETECTING)
            return JS_TRUE;

        switch (op) {
          case JSOP_NULL:
            /*
             * Special case #1: handle (document.all == null).  Don't sweat
             * about JS1.2's revision of the equality operators here.
             */
            if (++pc < endpc) {
                op = JSOp(*pc);
                return op == JSOP_EQ || op == JSOP_NE;
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
                op = JSOp(*pc);
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
    const JSCodeSpec *cs;
    uint32_t format;
    uintN flags = 0;

    jsbytecode *pc;
    JSScript *script = cx->stack.currentScript(&pc);
    if (!script || !pc)
        return defaultFlags;

    cs = &js_CodeSpec[*pc];
    format = cs->format;
    if (JOF_MODE(format) != JOF_NAME)
        flags |= JSRESOLVE_QUALIFIED;
    if (format & JOF_SET) {
        flags |= JSRESOLVE_ASSIGNING;
    } else if (cs->length >= 0) {
        pc += cs->length;
        if (pc < script->code + script->length && Detecting(cx, pc))
            flags |= JSRESOLVE_DETECTING;
    }
    if (format & JOF_DECLARING)
        flags |= JSRESOLVE_DECLARING;
    return flags;
}

JSBool
JSObject::nonNativeSetProperty(JSContext *cx, jsid id, js::Value *vp, JSBool strict)
{
    if (JS_UNLIKELY(watched())) {
        id = js_CheckForStringIndex(id);
        WatchpointMap *wpmap = cx->compartment->watchpointMap;
        if (wpmap && !wpmap->triggerWatchpoint(cx, this, id, vp))
            return false;
    }
    return getOps()->setGeneric(cx, this, id, vp, strict);
}

JSBool
JSObject::nonNativeSetElement(JSContext *cx, uint32_t index, js::Value *vp, JSBool strict)
{
    if (JS_UNLIKELY(watched())) {
        jsid id;
        if (!IndexToId(cx, index, &id))
            return false;
        JS_ASSERT(id == js_CheckForStringIndex(id));
        WatchpointMap *wpmap = cx->compartment->watchpointMap;
        if (wpmap && !wpmap->triggerWatchpoint(cx, this, id, vp))
            return false;
    }
    return getOps()->setElement(cx, this, index, vp, strict);
}

JS_FRIEND_API(bool)
JS_CopyPropertiesFrom(JSContext *cx, JSObject *target, JSObject *obj)
{
    // If we're not native, then we cannot copy properties.
    JS_ASSERT(target->isNative() == obj->isNative());
    if (!target->isNative())
        return true;

    AutoShapeVector shapes(cx);
    for (Shape::Range r(obj->lastProperty()); !r.empty(); r.popFront()) {
        if (!shapes.append(&r.front()))
            return false;
    }

    size_t n = shapes.length();
    while (n > 0) {
        const Shape *shape = shapes[--n];
        uintN attrs = shape->attributes();
        PropertyOp getter = shape->getter();
        if ((attrs & JSPROP_GETTER) && !cx->compartment->wrap(cx, &getter))
            return false;
        StrictPropertyOp setter = shape->setter();
        if ((attrs & JSPROP_SETTER) && !cx->compartment->wrap(cx, &setter))
            return false;
        Value v = shape->hasSlot() ? obj->getSlot(shape->slot()) : UndefinedValue();
        if (!cx->compartment->wrap(cx, &v))
            return false;
        if (!target->defineGeneric(cx, shape->propid(), v, getter, setter, attrs))
            return false;
    }
    return true;
}

static bool
CopySlots(JSContext *cx, JSObject *from, JSObject *to)
{
    JS_ASSERT(!from->isNative() && !to->isNative());
    JS_ASSERT(from->getClass() == to->getClass());

    size_t n = 0;
    if (from->isWrapper() &&
        (Wrapper::wrapperHandler(from)->flags() & Wrapper::CROSS_COMPARTMENT)) {
        to->setSlot(0, from->getSlot(0));
        to->setSlot(1, from->getSlot(1));
        n = 2;
    }

    size_t span = JSCLASS_RESERVED_SLOTS(from->getClass());
    for (; n < span; ++n) {
        Value v = from->getSlot(n);
        if (!cx->compartment->wrap(cx, &v))
            return false;
        to->setSlot(n, v);
    }
    return true;
}

JS_FRIEND_API(JSObject *)
JS_CloneObject(JSContext *cx, JSObject *obj, JSObject *proto, JSObject *parent)
{
    /*
     * We can only clone native objects and proxies. Dense arrays are slowified if
     * we try to clone them.
     */
    if (!obj->isNative()) {
        if (obj->isDenseArray()) {
            if (!obj->makeDenseArraySlow(cx))
                return NULL;
        } else if (!obj->isProxy()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_CANT_CLONE_OBJECT);
            return NULL;
        }
    }
    JSObject *clone = NewObjectWithGivenProto(cx, obj->getClass(), proto, parent, obj->getAllocKind());
    if (!clone)
        return NULL;
    if (obj->isNative()) {
        if (clone->isFunction() && (obj->compartment() != clone->compartment())) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_CANT_CLONE_OBJECT);
            return NULL;
        }

        if (obj->hasPrivate())
            clone->setPrivate(obj->getPrivate());
    } else {
        JS_ASSERT(obj->isProxy());
        if (!CopySlots(cx, obj, clone))
            return NULL;
    }

    return clone;
}

struct JSObject::TradeGutsReserved {
    JSContext *cx;
    Vector<Value> avals;
    Vector<Value> bvals;
    int newafixed;
    int newbfixed;
    Shape *newashape;
    Shape *newbshape;
    HeapValue *newaslots;
    HeapValue *newbslots;

    TradeGutsReserved(JSContext *cx)
        : cx(cx), avals(cx), bvals(cx),
          newafixed(0), newbfixed(0),
          newashape(NULL), newbshape(NULL),
          newaslots(NULL), newbslots(NULL)
    {}

    ~TradeGutsReserved()
    {
        if (newaslots)
            cx->free_(newaslots);
        if (newbslots)
            cx->free_(newbslots);
    }
};

bool
JSObject::ReserveForTradeGuts(JSContext *cx, JSObject *a, JSObject *b,
                              TradeGutsReserved &reserved)
{
    /*
     * When performing multiple swaps between objects which may have different
     * numbers of fixed slots, we reserve all space ahead of time so that the
     * swaps can be performed infallibly.
     */

    if (a->structSize() == b->structSize())
        return true;

    /*
     * If either object is native, it needs a new shape to preserve the
     * invariant that objects with the same shape have the same number of
     * inline slots. The fixed slots will be updated in place during TradeGuts.
     * Non-native objects need to be reshaped according to the new count.
     */
    if (a->isNative()) {
        if (!a->generateOwnShape(cx))
            return false;
    } else {
        reserved.newbshape = EmptyShape::getInitialShape(cx, a->getClass(),
                                                         a->getProto(), a->getParent(),
                                                         b->getAllocKind());
        if (!reserved.newbshape)
            return false;
    }
    if (b->isNative()) {
        if (!b->generateOwnShape(cx))
            return false;
    } else {
        reserved.newashape = EmptyShape::getInitialShape(cx, b->getClass(),
                                                         b->getProto(), b->getParent(),
                                                         a->getAllocKind());
        if (!reserved.newashape)
            return false;
    }

    /* The avals/bvals vectors hold all original values from the objects. */

    if (!reserved.avals.reserve(a->slotSpan()))
        return false;
    if (!reserved.bvals.reserve(b->slotSpan()))
        return false;

    JS_ASSERT(a->elements == emptyObjectElements);
    JS_ASSERT(b->elements == emptyObjectElements);

    /*
     * The newafixed/newbfixed hold the number of fixed slots in the objects
     * after the swap. Adjust these counts according to whether the objects
     * use their last fixed slot for storing private data.
     */

    reserved.newafixed = a->numFixedSlots();
    reserved.newbfixed = b->numFixedSlots();

    if (a->hasPrivate()) {
        reserved.newafixed++;
        reserved.newbfixed--;
    }
    if (b->hasPrivate()) {
        reserved.newbfixed++;
        reserved.newafixed--;
    }

    JS_ASSERT(reserved.newafixed >= 0);
    JS_ASSERT(reserved.newbfixed >= 0);

    /*
     * The newaslots/newbslots arrays hold any dynamic slots for the objects
     * if they do not have enough fixed slots to accomodate the slots in the
     * other object.
     */

    unsigned adynamic = dynamicSlotsCount(reserved.newafixed, b->slotSpan());
    unsigned bdynamic = dynamicSlotsCount(reserved.newbfixed, a->slotSpan());

    if (adynamic) {
        reserved.newaslots = (HeapValue *) cx->malloc_(sizeof(HeapValue) * adynamic);
        if (!reserved.newaslots)
            return false;
        Debug_SetValueRangeToCrashOnTouch(reserved.newaslots, adynamic);
    }
    if (bdynamic) {
        reserved.newbslots = (HeapValue *) cx->malloc_(sizeof(HeapValue) * bdynamic);
        if (!reserved.newbslots)
            return false;
        Debug_SetValueRangeToCrashOnTouch(reserved.newbslots, bdynamic);
    }

    return true;
}

void
JSObject::TradeGuts(JSContext *cx, JSObject *a, JSObject *b, TradeGutsReserved &reserved)
{
    JS_ASSERT(a->compartment() == b->compartment());
    JS_ASSERT(a->isFunction() == b->isFunction());

    /* Don't try to swap a JSFunction for a plain function JSObject. */
    JS_ASSERT_IF(a->isFunction(), a->structSize() == b->structSize());

    /*
     * Regexp guts are more complicated -- we would need to migrate the
     * refcounted JIT code blob for them across compartments instead of just
     * swapping guts.
     */
    JS_ASSERT(!a->isRegExp() && !b->isRegExp());

    /*
     * Callers should not try to swap dense arrays or ArrayBuffer objects,
     * these use a different slot representation from other objects.
     */
    JS_ASSERT(!a->isDenseArray() && !b->isDenseArray());
    JS_ASSERT(!a->isArrayBuffer() && !b->isArrayBuffer());

#ifdef JSGC_INCREMENTAL
    /*
     * We need a write barrier here. If |a| was marked and |b| was not, then
     * after the swap, |b|'s guts would never be marked. The write barrier
     * solves this.
     */
    JSCompartment *comp = a->compartment();
    if (comp->needsBarrier()) {
        MarkChildren(comp->barrierTracer(), a);
        MarkChildren(comp->barrierTracer(), b);
    }
#endif

    /* Trade the guts of the objects. */
    const size_t size = a->structSize();
    if (size == b->structSize()) {
        /*
         * If the objects are the same size, then we make no assumptions about
         * whether they have dynamically allocated slots and instead just copy
         * them over wholesale.
         */
        char tmp[tl::Max<sizeof(JSFunction), sizeof(JSObject_Slots16)>::result];
        JS_ASSERT(size <= sizeof(tmp));

        memcpy(tmp, a, size);
        memcpy(a, b, size);
        memcpy(b, tmp, size);
    } else {
        /*
         * If the objects are of differing sizes, use the space we reserved
         * earlier to save the slots from each object and then copy them into
         * the new layout for the other object.
         */

        unsigned acap = a->slotSpan();
        unsigned bcap = b->slotSpan();

        for (size_t i = 0; i < acap; i++)
            reserved.avals.infallibleAppend(a->getSlot(i));

        for (size_t i = 0; i < bcap; i++)
            reserved.bvals.infallibleAppend(b->getSlot(i));

        /* Done with the dynamic slots. */
        if (a->hasDynamicSlots())
            cx->free_(a->slots);
        if (b->hasDynamicSlots())
            cx->free_(b->slots);

        void *apriv = a->hasPrivate() ? a->getPrivate() : NULL;
        void *bpriv = b->hasPrivate() ? b->getPrivate() : NULL;

        char tmp[sizeof(JSObject)];
        memcpy(&tmp, a, sizeof tmp);
        memcpy(a, b, sizeof tmp);
        memcpy(b, &tmp, sizeof tmp);

        if (a->isNative())
            a->shape_->setNumFixedSlots(reserved.newafixed);
        else
            a->shape_ = reserved.newashape;

        a->slots = reserved.newaslots;
        a->initSlotRange(0, reserved.bvals.begin(), bcap);
        if (a->hasPrivate())
            a->setPrivate(bpriv);

        if (b->isNative())
            b->shape_->setNumFixedSlots(reserved.newbfixed);
        else
            b->shape_ = reserved.newbshape;

        b->slots = reserved.newbslots;
        b->initSlotRange(0, reserved.avals.begin(), acap);
        if (b->hasPrivate())
            b->setPrivate(apriv);

        /* Make sure the destructor for reserved doesn't free the slots. */
        reserved.newaslots = NULL;
        reserved.newbslots = NULL;
    }
}

/*
 * Use this method with extreme caution. It trades the guts of two objects and updates
 * scope ownership. This operation is not thread-safe, just as fast array to slow array
 * transitions are inherently not thread-safe. Don't perform a swap operation on objects
 * shared across threads or, or bad things will happen. You have been warned.
 */
bool
JSObject::swap(JSContext *cx, JSObject *other)
{
    if (this->compartment() == other->compartment()) {
        TradeGutsReserved reserved(cx);
        if (!ReserveForTradeGuts(cx, this, other, reserved))
            return false;
        TradeGuts(cx, this, other, reserved);
        return true;
    }

    JSObject *thisClone;
    JSObject *otherClone;
    {
        AutoCompartment ac(cx, other);
        if (!ac.enter())
            return false;
        thisClone = JS_CloneObject(cx, this, other->getProto(), other->getParent());
        if (!thisClone || !JS_CopyPropertiesFrom(cx, thisClone, this))
            return false;
    }
    {
        AutoCompartment ac(cx, this);
        if (!ac.enter())
            return false;
        otherClone = JS_CloneObject(cx, other, other->getProto(), other->getParent());
        if (!otherClone || !JS_CopyPropertiesFrom(cx, otherClone, other))
            return false;
    }

    TradeGutsReserved reservedThis(cx);
    TradeGutsReserved reservedOther(cx);

    if (!ReserveForTradeGuts(cx, this, otherClone, reservedThis) ||
        !ReserveForTradeGuts(cx, other, thisClone, reservedOther)) {
        return false;
    }

    TradeGuts(cx, this, otherClone, reservedThis);
    TradeGuts(cx, other, thisClone, reservedOther);

    return true;
}

static bool
DefineStandardSlot(JSContext *cx, JSObject *obj, JSProtoKey key, JSAtom *atom,
                   const Value &v, uint32_t attrs, bool &named)
{
    jsid id = ATOM_TO_JSID(atom);

    if (key != JSProto_Null) {
        /*
         * Initializing an actual standard class on a global object. If the
         * property is not yet present, force it into a new one bound to a
         * reserved slot. Otherwise, go through the normal property path.
         */
        JS_ASSERT(obj->isGlobal());
        JS_ASSERT(obj->isNative());

        const Shape *shape = obj->nativeLookup(cx, id);
        if (!shape) {
            uint32_t slot = 2 * JSProto_LIMIT + key;
            if (!js_SetReservedSlot(cx, obj, slot, v))
                return false;
            if (!obj->addProperty(cx, id, JS_PropertyStub, JS_StrictPropertyStub, slot, attrs, 0, 0))
                return false;
            AddTypePropertyId(cx, obj, id, v);

            named = true;
            return true;
        }
    }

    named = obj->defineGeneric(cx, id, v, JS_PropertyStub, JS_StrictPropertyStub, attrs);
    return named;
}

namespace js {

static bool
SetClassObject(JSContext *cx, JSObject *obj, JSProtoKey key, JSObject *cobj, JSObject *proto)
{
    JS_ASSERT(!obj->getParent());
    if (!obj->isGlobal())
        return true;

    return js_SetReservedSlot(cx, obj, key, ObjectOrNullValue(cobj)) &&
           js_SetReservedSlot(cx, obj, JSProto_LIMIT + key, ObjectOrNullValue(proto));
}

static void
ClearClassObject(JSContext *cx, JSObject *obj, JSProtoKey key)
{
    JS_ASSERT(!obj->getParent());
    if (!obj->isGlobal())
        return;

    obj->setSlot(key, UndefinedValue());
    obj->setSlot(JSProto_LIMIT + key, UndefinedValue());
}

JSObject *
DefineConstructorAndPrototype(JSContext *cx, HandleObject obj, JSProtoKey key, HandleAtom atom,
                              JSObject *protoProto, Class *clasp,
                              Native constructor, uintN nargs,
                              JSPropertySpec *ps, JSFunctionSpec *fs,
                              JSPropertySpec *static_ps, JSFunctionSpec *static_fs,
                              JSObject **ctorp, AllocKind ctorKind)
{
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
     *    &FunctionClass, not a JSObject-sized (smaller) GC-thing.
     *
     * The JS_NewObjectForGivenProto and JS_NewObject APIs also allow clasp to
     * be &FunctionClass (we could break compatibility easily). But fixing
     * (3) is not enough without addressing the bootstrapping dependency on (1)
     * and (2).
     */

    /*
     * Create the prototype object.  (GlobalObject::createBlankPrototype isn't
     * used because it parents the prototype object to the global and because
     * it uses WithProto::Given.  FIXME: Undo dependencies on this parentage
     * [which already needs to happen for bug 638316], figure out nicer
     * semantics for null-protoProto, and use createBlankPrototype.)
     */
    RootedVarObject proto(cx);
    proto = NewObjectWithClassProto(cx, clasp, protoProto, obj);
    if (!proto)
        return NULL;

    if (!proto->setSingletonType(cx))
        return NULL;

    if (clasp == &ArrayClass && !proto->makeDenseArraySlow(cx))
        return NULL;

    /* After this point, control must exit via label bad or out. */
    RootedVarObject ctor(cx);
    bool named = false;
    bool cached = false;
    if (!constructor) {
        /*
         * Lacking a constructor, name the prototype (e.g., Math) unless this
         * class (a) is anonymous, i.e. for internal use only; (b) the class
         * of obj (the global object) is has a reserved slot indexed by key;
         * and (c) key is not the null key.
         */
        if (!(clasp->flags & JSCLASS_IS_ANONYMOUS) || !obj->isGlobal() || key == JSProto_Null) {
            uint32_t attrs = (clasp->flags & JSCLASS_IS_ANONYMOUS)
                           ? JSPROP_READONLY | JSPROP_PERMANENT
                           : 0;
            if (!DefineStandardSlot(cx, obj, key, atom, ObjectValue(*proto), attrs, named))
                goto bad;
        }

        ctor = proto;
    } else {
        /*
         * Create the constructor, not using GlobalObject::createConstructor
         * because the constructor currently must have |obj| as its parent.
         * (FIXME: remove this dependency on the exact identity of the parent,
         * perhaps as part of bug 638316.)
         */
        RootedVarFunction fun(cx);
        fun = js_NewFunction(cx, NULL, constructor, nargs, JSFUN_CONSTRUCTOR, obj, atom,
                             ctorKind);
        if (!fun)
            goto bad;
        fun->setConstructorClass(clasp);

        /*
         * Set the class object early for standard class constructors. Type
         * inference may need to access these, and js_GetClassPrototype will
         * fail if it tries to do a reentrant reconstruction of the class.
         */
        if (key != JSProto_Null) {
            if (!SetClassObject(cx, obj, key, fun, proto))
                goto bad;
            cached = true;
        }

        AutoValueRooter tvr2(cx, ObjectValue(*fun));
        if (!DefineStandardSlot(cx, obj, key, atom, tvr2.value(), 0, named))
            goto bad;

        /*
         * Optionally construct the prototype object, before the class has
         * been fully initialized.  Allow the ctor to replace proto with a
         * different object, as is done for operator new -- and as at least
         * XML support requires.
         */
        ctor = fun;
        if (!LinkConstructorAndPrototype(cx, ctor, proto))
            goto bad;

        /* Bootstrap Function.prototype (see also JS_InitStandardClasses). */
        if (ctor->getClass() == clasp && !ctor->splicePrototype(cx, proto))
            goto bad;
    }

    if (!DefinePropertiesAndBrand(cx, proto, ps, fs) ||
        (ctor != proto && !DefinePropertiesAndBrand(cx, ctor, static_ps, static_fs)))
    {
        goto bad;
    }

    if (clasp->flags & (JSCLASS_FREEZE_PROTO|JSCLASS_FREEZE_CTOR)) {
        JS_ASSERT_IF(ctor == proto, !(clasp->flags & JSCLASS_FREEZE_CTOR));
        if (proto && (clasp->flags & JSCLASS_FREEZE_PROTO) && !proto->freeze(cx))
            goto bad;
        if (ctor && (clasp->flags & JSCLASS_FREEZE_CTOR) && !ctor->freeze(cx))
            goto bad;
    }

    /* If this is a standard class, cache its prototype. */
    if (!cached && key != JSProto_Null && !SetClassObject(cx, obj, key, ctor, proto))
        goto bad;

    if (ctorp)
        *ctorp = ctor;
    return proto;

bad:
    if (named) {
        Value rval;
        obj->deleteGeneric(cx, ATOM_TO_JSID(atom), &rval, false);
    }
    if (cached)
        ClearClassObject(cx, obj, key);
    return NULL;
}

/*
 * Lazy standard classes need a way to indicate if they have been initialized.
 * Otherwise, when we delete them, we might accidentally recreate them via a
 * lazy initialization. We use the presence of a ctor or proto in the
 * globalObject's slot to indicate that they've been constructed, but this only
 * works for classes which have a proto and ctor. Classes which don't have one
 * can call MarkStandardClassInitializedNoProto(), and we can always check
 * whether a class is initialized by calling IsStandardClassResolved().
 */
bool
IsStandardClassResolved(JSObject *obj, js::Class *clasp)
{
    JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(clasp);

    /* If the constructor is undefined, then it hasn't been initialized. */
    return (obj->getReservedSlot(key) != UndefinedValue());
}

void
MarkStandardClassInitializedNoProto(JSObject *obj, js::Class *clasp)
{
    JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(clasp);

    /*
     * We use True so that it's obvious what we're doing (instead of, say,
     * Null, which might be miscontrued as an error in setting Undefined).
     */
    if (obj->getReservedSlot(key) == UndefinedValue())
        obj->setSlot(key, BooleanValue(true));
}

}

JSObject *
js_InitClass(JSContext *cx, HandleObject obj, JSObject *protoProto,
             Class *clasp, Native constructor, uintN nargs,
             JSPropertySpec *ps, JSFunctionSpec *fs,
             JSPropertySpec *static_ps, JSFunctionSpec *static_fs,
             JSObject **ctorp, AllocKind ctorKind)
{
    RootObject rootProto(cx, &protoProto);

    RootedVarAtom atom(cx);
    atom = js_Atomize(cx, clasp->name, strlen(clasp->name));
    if (!atom)
        return NULL;

    /*
     * All instances of the class will inherit properties from the prototype
     * object we are about to create (in DefineConstructorAndPrototype), which
     * in turn will inherit from protoProto.
     *
     * When initializing a standard class (other than Object), if protoProto is
     * null, default to the Object prototype object. The engine's internal uses
     * of js_InitClass depend on this nicety. Note that in
     * js_InitFunctionAndObjectClasses, we specially hack the resolving table
     * and then depend on js_GetClassPrototype here leaving protoProto NULL and
     * returning true.
     */
    JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(clasp);
    if (key != JSProto_Null &&
        !protoProto &&
        !js_GetClassPrototype(cx, obj, JSProto_Object, &protoProto)) {
        return NULL;
    }

    return DefineConstructorAndPrototype(cx, obj, key, atom, protoProto, clasp, constructor, nargs,
                                         ps, fs, static_ps, static_fs, ctorp, ctorKind);
}

void
JSObject::getSlotRange(size_t start, size_t length,
                       HeapValue **fixedStart, HeapValue **fixedEnd,
                       HeapValue **slotsStart, HeapValue **slotsEnd)
{
    JS_ASSERT(!isDenseArray());
    JS_ASSERT(slotInRange(start + length, SENTINEL_ALLOWED));

    size_t fixed = numFixedSlots();
    if (start < fixed) {
        if (start + length < fixed) {
            *fixedStart = &fixedSlots()[start];
            *fixedEnd = &fixedSlots()[start + length];
            *slotsStart = *slotsEnd = NULL;
        } else {
            size_t localCopy = fixed - start;
            *fixedStart = &fixedSlots()[start];
            *fixedEnd = &fixedSlots()[start + localCopy];
            *slotsStart = &slots[0];
            *slotsEnd = &slots[length - localCopy];
        }
    } else {
        *fixedStart = *fixedEnd = NULL;
        *slotsStart = &slots[start - fixed];
        *slotsEnd = &slots[start - fixed + length];
    }
}

void
JSObject::initSlotRange(size_t start, const Value *vector, size_t length)
{
    JSCompartment *comp = compartment();
    HeapValue *fixedStart, *fixedEnd, *slotsStart, *slotsEnd;
    getSlotRange(start, length, &fixedStart, &fixedEnd, &slotsStart, &slotsEnd);
    for (HeapValue *vp = fixedStart; vp != fixedEnd; vp++)
        vp->init(comp, *vector++);
    for (HeapValue *vp = slotsStart; vp != slotsEnd; vp++)
        vp->init(comp, *vector++);
}

void
JSObject::copySlotRange(size_t start, const Value *vector, size_t length)
{
    JSCompartment *comp = compartment();
    HeapValue *fixedStart, *fixedEnd, *slotsStart, *slotsEnd;
    getSlotRange(start, length, &fixedStart, &fixedEnd, &slotsStart, &slotsEnd);
    for (HeapValue *vp = fixedStart; vp != fixedEnd; vp++)
        vp->set(comp, *vector++);
    for (HeapValue *vp = slotsStart; vp != slotsEnd; vp++)
        vp->set(comp, *vector++);
}

inline void
JSObject::invalidateSlotRange(size_t start, size_t length)
{
#ifdef DEBUG
    JS_ASSERT(!isDenseArray());

    size_t fixed = numFixedSlots();

    /* No bounds checks, allocated space has been updated but not the shape. */
    if (start < fixed) {
        if (start + length < fixed) {
            Debug_SetValueRangeToCrashOnTouch(fixedSlots() + start, length);
        } else {
            size_t localClear = fixed - start;
            Debug_SetValueRangeToCrashOnTouch(fixedSlots() + start, localClear);
            Debug_SetValueRangeToCrashOnTouch(slots, length - localClear);
        }
    } else {
        Debug_SetValueRangeToCrashOnTouch(slots + start - fixed, length);
    }
#endif /* DEBUG */
}

inline bool
JSObject::updateSlotsForSpan(JSContext *cx, size_t oldSpan, size_t newSpan)
{
    JS_ASSERT(oldSpan != newSpan);

    size_t oldCount = dynamicSlotsCount(numFixedSlots(), oldSpan);
    size_t newCount = dynamicSlotsCount(numFixedSlots(), newSpan);

    if (oldSpan < newSpan) {
        if (oldCount < newCount && !growSlots(cx, oldCount, newCount))
            return false;

        if (newSpan == oldSpan + 1)
            initSlotUnchecked(oldSpan, UndefinedValue());
        else
            initializeSlotRange(oldSpan, newSpan - oldSpan);
    } else {
        /* Trigger write barriers on the old slots before reallocating. */
        prepareSlotRangeForOverwrite(newSpan, oldSpan);
        invalidateSlotRange(newSpan, oldSpan - newSpan);

        if (oldCount > newCount)
            shrinkSlots(cx, oldCount, newCount);
    }

    return true;
}

bool
JSObject::setLastProperty(JSContext *cx, const js::Shape *shape)
{
    JS_ASSERT(!inDictionaryMode());
    JS_ASSERT(!shape->inDictionary());
    JS_ASSERT(shape->compartment() == compartment());
    JS_ASSERT(shape->numFixedSlots() == numFixedSlots());

    size_t oldSpan = lastProperty()->slotSpan();
    size_t newSpan = shape->slotSpan();

    if (oldSpan == newSpan) {
        shape_ = const_cast<js::Shape *>(shape);
        return true;
    }

    if (!updateSlotsForSpan(cx, oldSpan, newSpan))
        return false;

    shape_ = const_cast<js::Shape *>(shape);
    return true;
}

bool
JSObject::setSlotSpan(JSContext *cx, uint32_t span)
{
    JS_ASSERT(inDictionaryMode());
    js::BaseShape *base = lastProperty()->base();

    size_t oldSpan = base->slotSpan();

    if (oldSpan == span)
        return true;

    if (!updateSlotsForSpan(cx, oldSpan, span))
        return false;

    base->setSlotSpan(span);
    return true;
}

bool
JSObject::growSlots(JSContext *cx, uint32_t oldCount, uint32_t newCount)
{
    JS_ASSERT(newCount > oldCount);
    JS_ASSERT(newCount >= SLOT_CAPACITY_MIN);
    JS_ASSERT(!isDenseArray());

    /*
     * Slots are only allocated for call objects when new properties are
     * added to them, which can only happen while the call is still on the
     * stack (and an eval, DEFFUN, etc. happens). We thus do not need to
     * worry about updating any active outer function args/vars.
     */
    JS_ASSERT_IF(isCall(), asCall().maybeStackFrame() != NULL);

    /*
     * Slot capacities are determined by the span of allocated objects. Due to
     * the limited number of bits to store shape slots, object growth is
     * throttled well before the slot capacity can overflow.
     */
    JS_ASSERT(newCount < NELEMENTS_LIMIT);

    size_t oldSize = Probes::objectResizeActive() ? slotsAndStructSize() : 0;
    size_t newSize = oldSize + (newCount - oldCount) * sizeof(Value);

    /*
     * If we are allocating slots for an object whose type is always created
     * by calling 'new' on a particular script, bump the GC kind for that
     * type to give these objects a larger number of fixed slots when future
     * objects are constructed.
     */
    if (!hasLazyType() && !oldCount && type()->newScript) {
        gc::AllocKind kind = type()->newScript->allocKind;
        unsigned newScriptSlots = gc::GetGCKindSlots(kind);
        if (newScriptSlots == numFixedSlots() && gc::TryIncrementAllocKind(&kind)) {
            JSObject *obj = NewReshapedObject(cx, type(), getParent(), kind,
                                              type()->newScript->shape);
            if (!obj)
                return false;

            type()->newScript->allocKind = kind;
            type()->newScript->shape = obj->lastProperty();
            type()->markStateChange(cx);
        }
    }

    if (!oldCount) {
        slots = (HeapValue *) cx->malloc_(newCount * sizeof(HeapValue));
        if (!slots)
            return false;
        Debug_SetValueRangeToCrashOnTouch(slots, newCount);
        if (Probes::objectResizeActive())
            Probes::resizeObject(cx, this, oldSize, newSize);
        return true;
    }

    HeapValue *newslots = (HeapValue*) cx->realloc_(slots, oldCount * sizeof(HeapValue),
                                                    newCount * sizeof(HeapValue));
    if (!newslots)
        return false;  /* Leave slots at its old size. */

    bool changed = slots != newslots;
    slots = newslots;

    Debug_SetValueRangeToCrashOnTouch(slots + oldCount, newCount - oldCount);

    /* Changes in the slots of global objects can trigger recompilation. */
    if (changed && isGlobal())
        types::MarkObjectStateChange(cx, this);

    if (Probes::objectResizeActive())
        Probes::resizeObject(cx, this, oldSize, newSize);

    return true;
}

void
JSObject::shrinkSlots(JSContext *cx, uint32_t oldCount, uint32_t newCount)
{
    JS_ASSERT(newCount < oldCount);
    JS_ASSERT(!isDenseArray());

    /*
     * Refuse to shrink slots for call objects. This only happens in a very
     * obscure situation (deleting names introduced by a direct 'eval') and
     * allowing the slots pointer to change may require updating pointers in
     * the function's active args/vars information.
     */
    if (isCall())
        return;

    size_t oldSize = Probes::objectResizeActive() ? slotsAndStructSize() : 0;
    size_t newSize = oldSize - (oldCount - newCount) * sizeof(Value);

    if (newCount == 0) {
        cx->free_(slots);
        slots = NULL;
        if (Probes::objectResizeActive())
            Probes::resizeObject(cx, this, oldSize, newSize);
        return;
    }

    JS_ASSERT(newCount >= SLOT_CAPACITY_MIN);

    HeapValue *newslots = (HeapValue*) cx->realloc_(slots, newCount * sizeof(HeapValue));
    if (!newslots)
        return;  /* Leave slots at its old size. */

    bool changed = slots != newslots;
    slots = newslots;

    /* Watch for changes in global object slots, as for growSlots. */
    if (changed && isGlobal())
        types::MarkObjectStateChange(cx, this);

    if (Probes::objectResizeActive())
        Probes::resizeObject(cx, this, oldSize, newSize);
}

bool
JSObject::growElements(JSContext *cx, uintN newcap)
{
    JS_ASSERT(isDenseArray());

    /*
     * When an object with CAPACITY_DOUBLING_MAX or fewer elements needs to
     * grow, double its capacity, to add N elements in amortized O(N) time.
     *
     * Above this limit, grow by 12.5% each time. Speed is still amortized
     * O(N), with a higher constant factor, and we waste less space.
     */
    static const size_t CAPACITY_DOUBLING_MAX = 1024 * 1024;
    static const size_t CAPACITY_CHUNK = CAPACITY_DOUBLING_MAX / sizeof(Value);

    uint32_t oldcap = getDenseArrayCapacity();
    JS_ASSERT(oldcap <= newcap);

    size_t oldSize = Probes::objectResizeActive() ? slotsAndStructSize() : 0;

    uint32_t nextsize = (oldcap <= CAPACITY_DOUBLING_MAX)
                      ? oldcap * 2
                      : oldcap + (oldcap >> 3);

    uint32_t actualCapacity = JS_MAX(newcap, nextsize);
    if (actualCapacity >= CAPACITY_CHUNK)
        actualCapacity = JS_ROUNDUP(actualCapacity, CAPACITY_CHUNK);
    else if (actualCapacity < SLOT_CAPACITY_MIN)
        actualCapacity = SLOT_CAPACITY_MIN;

    /* Don't let nelements get close to wrapping around uint32_t. */
    if (actualCapacity >= NELEMENTS_LIMIT || actualCapacity < oldcap || actualCapacity < newcap) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    uint32_t initlen = getDenseArrayInitializedLength();
    uint32_t newAllocated = actualCapacity + ObjectElements::VALUES_PER_HEADER;

    ObjectElements *newheader;
    if (hasDynamicElements()) {
        uint32_t oldAllocated = oldcap + ObjectElements::VALUES_PER_HEADER;
        newheader = (ObjectElements *)
            cx->realloc_(getElementsHeader(), oldAllocated * sizeof(Value),
                         newAllocated * sizeof(Value));
        if (!newheader)
            return false;  /* Leave elements as its old size. */
    } else {
        newheader = (ObjectElements *) cx->malloc_(newAllocated * sizeof(Value));
        if (!newheader)
            return false;  /* Ditto. */
        memcpy(newheader, getElementsHeader(),
               (ObjectElements::VALUES_PER_HEADER + initlen) * sizeof(Value));
    }

    newheader->capacity = actualCapacity;
    elements = newheader->elements();

    Debug_SetValueRangeToCrashOnTouch(elements + initlen, actualCapacity - initlen);

    if (Probes::objectResizeActive())
        Probes::resizeObject(cx, this, oldSize, slotsAndStructSize());

    return true;
}

void
JSObject::shrinkElements(JSContext *cx, uintN newcap)
{
    JS_ASSERT(isDenseArray());

    uint32_t oldcap = getDenseArrayCapacity();
    JS_ASSERT(newcap <= oldcap);

    size_t oldSize = Probes::objectResizeActive() ? slotsAndStructSize() : 0;

    /* Don't shrink elements below the minimum capacity. */
    if (oldcap <= SLOT_CAPACITY_MIN || !hasDynamicElements())
        return;

    newcap = Max(newcap, SLOT_CAPACITY_MIN);

    uint32_t newAllocated = newcap + ObjectElements::VALUES_PER_HEADER;

    ObjectElements *newheader = (ObjectElements *)
        cx->realloc_(getElementsHeader(), newAllocated * sizeof(Value));
    if (!newheader)
        return;  /* Leave elements at its old size. */

    newheader->capacity = newcap;
    elements = newheader->elements();

    if (Probes::objectResizeActive())
        Probes::resizeObject(cx, this, oldSize, slotsAndStructSize());
}

#ifdef DEBUG
bool
JSObject::slotInRange(uintN slot, SentinelAllowed sentinel) const
{
    size_t capacity = numFixedSlots() + numDynamicSlots();
    if (sentinel == SENTINEL_ALLOWED)
        return slot <= capacity;
    return slot < capacity;
}
#endif /* DEBUG */

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

    if (proto && proto->isXML()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_XML_PROTO_FORBIDDEN);
        return false;
    }

    /*
     * Regenerate shapes for all of the scopes along the old prototype chain,
     * in case any entries were filled by looking up through obj. Stop when a
     * non-native object is found, prototype lookups will not be cached across
     * these.
     *
     * How this shape change is done is very delicate; the change can be made
     * either by marking the object's prototype as uncacheable (such that the
     * property cache and JIT'ed ICs cannot assume the shape determines the
     * prototype) or by just generating a new shape for the object. Choosing
     * the former is bad if the object is on the prototype chain of other
     * objects, as the uncacheable prototype can inhibit iterator caches on
     * those objects and slow down prototype accesses. Choosing the latter is
     * bad if there are many similar objects to this one which will have their
     * prototype mutated, as the generateOwnShape forces the object into
     * dictionary mode and similar property lineages will be repeatedly cloned.
     *
     * :XXX: bug 707717 make this code less brittle.
     */
    JSObject *oldproto = obj;
    while (oldproto && oldproto->isNative()) {
        if (oldproto->hasSingletonType()) {
            if (!oldproto->generateOwnShape(cx))
                return false;
        } else {
            if (!oldproto->setUncacheableProto(cx))
                return false;
        }
        oldproto = oldproto->getProto();
    }

    if (checkForCycles) {
        for (JSObject *obj2 = proto; obj2; obj2 = obj2->getProto()) {
            if (obj2 == obj) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CYCLIC_VALUE,
                                     js_proto_str);
                return false;
            }
        }
    }

    if (obj->hasSingletonType()) {
        /*
         * Just splice the prototype, but mark the properties as unknown for
         * consistent behavior.
         */
        if (!obj->splicePrototype(cx, proto))
            return false;
        MarkTypeObjectUnknownProperties(cx, obj->type());
        return true;
    }

    if (proto && !proto->setNewTypeUnknown(cx))
        return false;

    TypeObject *type = proto
        ? proto->getNewType(cx, NULL)
        : cx->compartment->getEmptyType(cx);
    if (!type)
        return false;

    /*
     * Setting __proto__ on an object that has escaped and may be referenced by
     * other heap objects can only be done if the properties of both objects
     * are unknown. Type sets containing this object will contain the original
     * type but not the new type of the object, so we need to go and scan the
     * entire compartment for type sets which have these objects and mark them
     * as containing generic objects.
     */
    MarkTypeObjectUnknownProperties(cx, obj->type(), true);
    MarkTypeObjectUnknownProperties(cx, type, true);

    obj->setType(type);
    return true;
}

}

JSBool
js_GetClassObject(JSContext *cx, JSObject *obj, JSProtoKey key,
                  JSObject **objp)
{
    RootObject objRoot(cx, &obj);

    obj = &obj->global();
    if (!obj->isGlobal()) {
        *objp = NULL;
        return true;
    }

    Value v = obj->getReservedSlot(key);
    if (v.isObject()) {
        *objp = &v.toObject();
        return true;
    }

    AutoResolving resolving(cx, obj, ATOM_TO_JSID(cx->runtime->atomState.classAtoms[key]));
    if (resolving.alreadyStarted()) {
        /* Already caching id in obj -- suppress recursion. */
        *objp = NULL;
        return true;
    }

    JSObject *cobj = NULL;
    if (JSObjectOp init = lazy_prototype_init[key]) {
        if (!init(cx, obj))
            return false;
        v = obj->getReservedSlot(key);
        if (v.isObject())
            cobj = &v.toObject();
    }

    *objp = cobj;
    return true;
}

JSBool
js_FindClassObject(JSContext *cx, JSObject *start, JSProtoKey protoKey,
                   Value *vp, Class *clasp)
{
    JSObject *cobj, *pobj;
    jsid id;
    JSProperty *prop;
    const Shape *shape;

    RootedVarObject obj(cx);

    if (start) {
        obj = &start->global();
        OBJ_TO_INNER_OBJECT(cx, *obj.address());
    } else {
        obj = GetGlobalForScopeChain(cx);
    }
    if (!obj)
        return false;

    if (protoKey != JSProto_Null) {
        JS_ASSERT(JSProto_Null < protoKey);
        JS_ASSERT(protoKey < JSProto_LIMIT);
        if (!js_GetClassObject(cx, obj, protoKey, &cobj))
            return false;
        if (cobj) {
            vp->setObject(*cobj);
            return JS_TRUE;
        }
        id = ATOM_TO_JSID(cx->runtime->atomState.classAtoms[protoKey]);
    } else {
        JSAtom *atom = js_Atomize(cx, clasp->name, strlen(clasp->name));
        if (!atom)
            return false;
        id = ATOM_TO_JSID(atom);
    }

    JS_ASSERT(obj->isNative());
    if (!LookupPropertyWithFlags(cx, obj, id, JSRESOLVE_CLASSNAME, &pobj, &prop))
        return false;
    Value v = UndefinedValue();
    if (prop && pobj->isNative()) {
        shape = (Shape *) prop;
        if (shape->hasSlot()) {
            v = pobj->nativeGetSlot(shape->slot());
            if (v.isPrimitive())
                v.setUndefined();
        }
    }
    *vp = v;
    return true;
}

bool
JSObject::allocSlot(JSContext *cx, uint32_t *slotp)
{
    uint32_t slot = slotSpan();
    JS_ASSERT(slot >= JSSLOT_FREE(getClass()));

    /*
     * If this object is in dictionary mode, try to pull a free slot from the
     * property table's slot-number freelist.
     */
    if (inDictionaryMode()) {
        PropertyTable &table = lastProperty()->table();
        uint32_t last = table.freelist;
        if (last != SHAPE_INVALID_SLOT) {
#ifdef DEBUG
            JS_ASSERT(last < slot);
            uint32_t next = getSlot(last).toPrivateUint32();
            JS_ASSERT_IF(next != SHAPE_INVALID_SLOT, next < slot);
#endif

            *slotp = last;

            const Value &vref = getSlot(last);
            table.freelist = vref.toPrivateUint32();
            setSlot(last, UndefinedValue());
            return true;
        }
    }

    if (slot >= SHAPE_MAXIMUM_SLOT) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    *slotp = slot;

    if (inDictionaryMode() && !setSlotSpan(cx, slot + 1))
        return false;

    return true;
}

void
JSObject::freeSlot(JSContext *cx, uint32_t slot)
{
    JS_ASSERT(slot < slotSpan());

    if (inDictionaryMode()) {
        uint32_t &last = lastProperty()->table().freelist;

        /* Can't afford to check the whole freelist, but let's check the head. */
        JS_ASSERT_IF(last != SHAPE_INVALID_SLOT, last < slotSpan() && last != slot);

        /*
         * Place all freed slots other than reserved slots (bug 595230) on the
         * dictionary's free list.
         */
        if (JSSLOT_FREE(getClass()) <= slot) {
            JS_ASSERT_IF(last != SHAPE_INVALID_SLOT, last < slotSpan());
            setSlot(slot, PrivateUint32Value(last));
            last = slot;
            return;
        }
    }
    setSlot(slot, UndefinedValue());
}

static bool
PurgeProtoChain(JSContext *cx, JSObject *obj, jsid id)
{
    const Shape *shape;

    RootObject objRoot(cx, &obj);
    RootId idRoot(cx, &id);

    while (obj) {
        if (!obj->isNative()) {
            obj = obj->getProto();
            continue;
        }
        shape = obj->nativeLookup(cx, id);
        if (shape) {
            PCMETER(JS_PROPERTY_CACHE(cx).pcpurges++);
            if (!obj->shadowingShapeChange(cx, *shape))
                return false;

            obj->shadowingShapeChange(cx, *shape);
            return true;
        }
        obj = obj->getProto();
    }

    return true;
}

bool
js_PurgeScopeChainHelper(JSContext *cx, JSObject *obj, jsid id)
{
    RootObject objRoot(cx, &obj);
    RootId idRoot(cx, &id);

    JS_ASSERT(obj->isDelegate());
    PurgeProtoChain(cx, obj->getProto(), id);

    /*
     * We must purge the scope chain only for Call objects as they are the only
     * kind of cacheable non-global object that can gain properties after outer
     * properties with the same names have been cached or traced. Call objects
     * may gain such properties via eval introducing new vars; see bug 490364.
     */
    if (obj->isCall()) {
        while ((obj = obj->enclosingScope()) != NULL) {
            if (!PurgeProtoChain(cx, obj, id))
                return false;
        }
    }

    return true;
}

Shape *
js_AddNativeProperty(JSContext *cx, JSObject *obj, jsid id,
                     PropertyOp getter, StrictPropertyOp setter, uint32_t slot,
                     uintN attrs, uintN flags, intN shortid)
{
    JS_ASSERT(!(flags & Shape::METHOD));

    /* Convert string indices to integers if appropriate. */
    id = js_CheckForStringIndex(id);

    /*
     * Purge the property cache of now-shadowed id in obj's scope chain. Do
     * this optimistically (assuming no failure below) before locking obj, so
     * we can lock the shadowed scope.
     */
    if (!js_PurgeScopeChain(cx, obj, id))
        return NULL;

    return obj->putProperty(cx, id, getter, setter, slot, attrs, flags, shortid);
}

Shape *
js_ChangeNativePropertyAttrs(JSContext *cx, JSObject *obj,
                             Shape *shape, uintN attrs, uintN mask,
                             PropertyOp getter, StrictPropertyOp setter)
{
    /*
     * Check for freezing an object with shape-memoized methods here, on a
     * shape-by-shape basis.
     */
    if ((attrs & JSPROP_READONLY) && shape->isMethod()) {
        Value v = ObjectValue(*obj->nativeGetMethod(shape));

        shape = obj->methodReadBarrier(cx, *shape, &v);
        if (!shape)
            return NULL;
    }

    return obj->changeProperty(cx, shape, attrs, mask, getter, setter);
}

JSBool
js_DefineProperty(JSContext *cx, JSObject *obj, jsid id, const Value *value,
                  PropertyOp getter, StrictPropertyOp setter, uintN attrs)
{
    return !!DefineNativeProperty(cx, obj, id, *value, getter, setter, attrs, 0, 0);
}

JSBool
js_DefineElement(JSContext *cx, JSObject *obj, uint32_t index, const Value *value,
                 PropertyOp getter, StrictPropertyOp setter, uintN attrs)
{
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;
    return !!DefineNativeProperty(cx, obj, id, *value, getter, setter, attrs, 0, 0);
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
    if (clasp->addProperty != JS_PropertyStub) {
        Value nominal = *vp;

        if (!CallJSPropertyOp(cx, clasp->addProperty, obj, shape->propid(), vp))
            return false;
        if (*vp != nominal) {
            if (shape->hasSlot())
                obj->nativeSetSlotWithType(cx, shape, *vp);
        }
    }
    return true;
}

namespace js {

const Shape *
DefineNativeProperty(JSContext *cx, JSObject *obj, jsid id, const Value &value_,
                     PropertyOp getter, StrictPropertyOp setter, uintN attrs,
                     uintN flags, intN shortid, uintN defineHow /* = 0 */)
{
    JS_ASSERT((defineHow & ~(DNP_CACHE_RESULT | DNP_DONT_PURGE |
                             DNP_SET_METHOD | DNP_SKIP_TYPE)) == 0);

    RootObject objRoot(cx, &obj);
    RootId idRoot(cx, &id);

    /*
     * Make a local copy of value, in case a method barrier needs to update the
     * value to define, and just so addProperty can mutate its inout parameter.
     */
    RootedVarValue value(cx);
    value = value_;

    /* Convert string indices to integers if appropriate. */
    id = js_CheckForStringIndex(id);

    /*
     * If defining a getter or setter, we must check for its counterpart and
     * update the attributes and property ops.  A getter or setter is really
     * only half of a property.
     */
    Shape *shape = NULL;
    if (attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
        JSObject *pobj;
        JSProperty *prop;

        /* Type information for getter/setter properties is unknown. */
        AddTypePropertyId(cx, obj, id, types::Type::UnknownType());
        MarkTypePropertyConfigured(cx, obj, id);

        /*
         * If we are defining a getter whose setter was already defined, or
         * vice versa, finish the job via obj->changeProperty, and refresh the
         * property cache line for (obj, id) to map shape.
         */
        if (!js_LookupProperty(cx, obj, id, &pobj, &prop))
            return NULL;
        if (prop && pobj == obj) {
            shape = (Shape *) prop;
            if (shape->isAccessorDescriptor()) {
                shape = obj->changeProperty(cx, shape, attrs,
                                            JSPROP_GETTER | JSPROP_SETTER,
                                            (attrs & JSPROP_GETTER)
                                            ? getter
                                            : shape->getter(),
                                            (attrs & JSPROP_SETTER)
                                            ? setter
                                            : shape->setter());
                if (!shape)
                    return NULL;
            } else {
                shape = NULL;
            }
        }
    }

    /*
     * Purge the property cache of any properties named by id that are about
     * to be shadowed in obj's scope chain unless it is known a priori that it
     * is not possible. We do this before locking obj to avoid nesting locks.
     */
    if (!(defineHow & DNP_DONT_PURGE)) {
        if (!js_PurgeScopeChain(cx, obj, id))
            return NULL;
    }

    /* Use the object's class getter and setter by default. */
    Class *clasp = obj->getClass();
    if (!(defineHow & DNP_SET_METHOD)) {
        if (!getter && !(attrs & JSPROP_GETTER))
            getter = clasp->getProperty;
        if (!setter && !(attrs & JSPROP_SETTER))
            setter = clasp->setProperty;
    }

    if (((defineHow & DNP_SET_METHOD) || getter == JS_PropertyStub) &&
        !(defineHow & DNP_SKIP_TYPE)) {
        /*
         * Type information for normal native properties should reflect the
         * initial value of the property.
         */
        AddTypePropertyId(cx, obj, id, value);
        if (attrs & JSPROP_READONLY)
            MarkTypePropertyConfigured(cx, obj, id);
    }

    if (!shape) {
        /* Add a new property, or replace an existing one of the same id. */
        if (defineHow & DNP_SET_METHOD) {
            JS_ASSERT(clasp == &ObjectClass);
            JS_ASSERT(IsFunctionObject(value));
            JS_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));
            JS_ASSERT(!getter && !setter);

            JSObject *funobj = &value.raw().toObject();
            if (!funobj->toFunction()->isClonedMethod())
                flags |= Shape::METHOD;
        }

        if (const Shape *existingShape = obj->nativeLookup(cx, id)) {
            if (existingShape->isMethod() &&
                ObjectValue(*obj->nativeGetMethod(existingShape)) == value)
            {
                /*
                 * Redefining an existing shape-memoized method object without
                 * changing the property's value, perhaps to change attributes.
                 * Clone now via the method read barrier.
                 */
                if (!obj->methodReadBarrier(cx, *existingShape, value.address()))
                    return NULL;
            }
        }

        shape = obj->putProperty(cx, id, getter, setter, SHAPE_INVALID_SLOT,
                                 attrs, flags, shortid);
        if (!shape)
            return NULL;
    }

    /* Store valueCopy before calling addProperty, in case the latter GC's. */
    if (shape->hasSlot())
        obj->nativeSetSlot(shape->slot(), value);

    /* XXXbe called with lock held */
    if (!CallAddPropertyHook(cx, clasp, obj, shape, value.address())) {
        obj->removeProperty(cx, id);
        return NULL;
    }

    return shape;
}

} /* namespace js */

/*
 * Call obj's resolve hook.
 *
 * cx, start, id, and flags are the parameters initially passed to the ongoing
 * lookup; objp and propp are its out parameters. obj is an object along
 * start's prototype chain.
 *
 * There are four possible outcomes:
 *
 *   - On failure, report an error or exception and return false.
 *
 *   - If we are already resolving a property of *curobjp, set *recursedp = true,
 *     and return true.
 *
 *   - If the resolve hook finds or defines the sought property, set *objp and
 *     *propp appropriately, set *recursedp = false, and return true.
 *
 *   - Otherwise no property was resolved. Set *propp = NULL and *recursedp = false
 *     and return true.
 */
static JSBool
CallResolveOp(JSContext *cx, JSObject *start, HandleObject obj, HandleId id, uintN flags,
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
    AutoResolving resolving(cx, obj, id);
    if (resolving.alreadyStarted()) {
        /* Already resolving id in obj -- suppress recursion. */
        *recursedp = true;
        return true;
    }
    *recursedp = false;

    *propp = NULL;

    if (clasp->flags & JSCLASS_NEW_RESOLVE) {
        JSNewResolveOp newresolve = reinterpret_cast<JSNewResolveOp>(resolve);
        if (flags == RESOLVE_INFER)
            flags = js_InferFlags(cx, 0);

        RootedVarObject obj2(cx);
        obj2 = (clasp->flags & JSCLASS_NEW_RESOLVE_GETS_START) ? start : NULL;
        if (!newresolve(cx, obj, id, flags, obj2.address()))
            return false;

        /*
         * We trust the new style resolve hook to set obj2 to NULL when
         * the id cannot be resolved. But, when obj2 is not null, we do
         * not assume that id must exist and do full nativeLookup for
         * compatibility.
         */
        if (!obj2)
            return true;

        if (!obj2->isNative()) {
            /* Whoops, newresolve handed back a foreign obj2. */
            JS_ASSERT(obj2 != obj);
            return obj2->lookupGeneric(cx, id, objp, propp);
        }
        obj = obj2;
    } else {
        if (!resolve(cx, obj, id))
            return false;
    }

    if (!obj->nativeEmpty()) {
        if (const Shape *shape = obj->nativeLookup(cx, id)) {
            *objp = obj;
            *propp = (JSProperty *) shape;
        }
    }

    return true;
}

static JS_ALWAYS_INLINE bool
LookupPropertyWithFlagsInline(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                              JSObject **objp, JSProperty **propp)
{
    /* We should not get string indices which aren't already integers here. */
    JS_ASSERT(id == js_CheckForStringIndex(id));

    RootObject objRoot(cx, &obj);
    RootId idRoot(cx, &id);

    /* Search scopes starting with obj and following the prototype link. */
    JSObject *start = obj;
    while (true) {
        const Shape *shape = obj->nativeLookup(cx, id);
        if (shape) {
            *objp = obj;
            *propp = (JSProperty *) shape;
            return true;
        }

        /* Try obj's class resolve hook if id was not found in obj's scope. */
        if (obj->getClass()->resolve != JS_ResolveStub) {
            bool recursed;
            if (!CallResolveOp(cx, start, objRoot, idRoot, flags, objp, propp, &recursed))
                return false;
            if (recursed)
                break;
            if (*propp) {
                /*
                 * For stats we do not recalculate protoIndex even if it was
                 * resolved on some other object.
                 */
                return true;
            }
        }

        JSObject *proto = obj->getProto();
        if (!proto)
            break;
        if (!proto->isNative()) {
            if (!proto->lookupGeneric(cx, id, objp, propp))
                return false;
#ifdef DEBUG
            /*
             * Non-native objects must have either non-native lookup results,
             * or else native results from the non-native's prototype chain.
             *
             * See StackFrame::getValidCalleeObject, where we depend on this
             * fact to force a prototype-delegated joined method accessed via
             * arguments.callee through the delegating |this| object's method
             * read barrier.
             */
            if (*propp && (*objp)->isNative()) {
                while ((proto = proto->getProto()) != *objp)
                    JS_ASSERT(proto);
            }
#endif
            return true;
        }

        obj = proto;
    }

    *objp = NULL;
    *propp = NULL;
    return true;
}

JS_FRIEND_API(JSBool)
js_LookupProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                  JSProperty **propp)
{
    /* Convert string indices to integers if appropriate. */
    id = js_CheckForStringIndex(id);

    return LookupPropertyWithFlagsInline(cx, obj, id, cx->resolveFlags, objp, propp);
}

JS_FRIEND_API(JSBool)
js_LookupElement(JSContext *cx, JSObject *obj, uint32_t index, JSObject **objp, JSProperty **propp)
{
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;

    return LookupPropertyWithFlagsInline(cx, obj, id, cx->resolveFlags, objp, propp);
}

namespace js {

bool
LookupPropertyWithFlags(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                        JSObject **objp, JSProperty **propp)
{
    /* Convert string indices to integers if appropriate. */
    id = js_CheckForStringIndex(id);

    return LookupPropertyWithFlagsInline(cx, obj, id, flags, objp, propp);
}

} /* namespace js */

PropertyCacheEntry *
js_FindPropertyHelper(JSContext *cx, jsid id, bool cacheResult, bool global,
                      JSObject **objp, JSObject **pobjp, JSProperty **propp)
{
    JSObject *scopeChain, *obj, *parent, *pobj;
    PropertyCacheEntry *entry;
    int scopeIndex;
    JSProperty *prop;

    scopeChain = cx->stack.currentScriptedScopeChain();

    if (global) {
        /*
         * Skip along the scope chain to the enclosing global object. This is
         * used for GNAME opcodes where the bytecode emitter has determined a
         * name access must be on the global. It also insulates us from bugs
         * in the emitter: type inference will assume that GNAME opcodes are
         * accessing the global object, and the inferred behavior should match
         * the actual behavior even if the id could be found on the scope chain
         * before the global object.
         */
        scopeChain = &scopeChain->global();
    }

    /* Scan entries on the scope chain that we can cache across. */
    entry = JS_NO_PROP_CACHE_FILL;
    obj = scopeChain;
    parent = obj->enclosingScope();
    for (scopeIndex = 0;
         parent
         ? IsCacheableNonGlobalScope(obj)
         : !obj->getOps()->lookupProperty;
         ++scopeIndex) {
        if (!LookupPropertyWithFlags(cx, obj, id, cx->resolveFlags, &pobj, &prop))
            return NULL;

        if (prop) {
#ifdef DEBUG
            if (parent) {
                JS_ASSERT(pobj->isNative());
                JS_ASSERT(pobj->getClass() == obj->getClass());
                if (obj->isBlock()) {
                    /*
                     * A block instance on the scope chain is immutable and
                     * shares its shape with the compile-time prototype. Thus
                     * we cannot find any property on the prototype.
                     */
                    JS_ASSERT(pobj->isClonedBlock());
                } else {
                    /* Call and DeclEnvClass objects have no prototypes. */
                    JS_ASSERT(!obj->getProto());
                }
                JS_ASSERT(pobj == obj);
            } else {
                JS_ASSERT(obj->isNative());
            }
#endif
            /*
             * We must check if pobj is native as a global object can have
             * non-native prototype.
             */
            if (cacheResult && pobj->isNative()) {
                entry = JS_PROPERTY_CACHE(cx).fill(cx, scopeChain, scopeIndex, pobj,
                                                   (Shape *) prop);
            }
            goto out;
        }

        if (!parent) {
            pobj = NULL;
            goto out;
        }
        obj = parent;
        parent = obj->enclosingScope();
    }

    for (;;) {
        if (!obj->lookupGeneric(cx, id, &pobj, &prop))
            return NULL;
        if (prop) {
            PCMETER(JS_PROPERTY_CACHE(cx).nofills++);
            goto out;
        }

        /*
         * We conservatively assume that a resolve hook could mutate the scope
         * chain during JSObject::lookupGeneric. So we read parent here again.
         */
        parent = obj->enclosingScope();
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

/*
 * On return, if |*pobjp| is a native object, then |*propp| is a |Shape *|.
 * Otherwise, its type and meaning depends on the host object's implementation.
 */
JS_FRIEND_API(JSBool)
js_FindProperty(JSContext *cx, jsid id, bool global,
                JSObject **objp, JSObject **pobjp, JSProperty **propp)
{
    return !!js_FindPropertyHelper(cx, id, false, global, objp, pobjp, propp);
}

JSObject *
js_FindIdentifierBase(JSContext *cx, JSObject *scopeChain, jsid id)
{
    /*
     * This function should not be called for a global object or from the
     * trace and should have a valid cache entry for native scopeChain.
     */
    JS_ASSERT(scopeChain->enclosingScope() != NULL);

    JSObject *obj = scopeChain;

    /*
     * Loop over cacheable objects on the scope chain until we find a
     * property. We also stop when we reach the global object skipping any
     * farther checks or lookups. For details see the JSOP_BINDNAME case of
     * js_Interpret.
     *
     * The test order here matters because IsCacheableNonGlobalScope
     * must not be passed a global object (i.e. one with null parent).
     */
    for (int scopeIndex = 0;
         obj->isGlobal() || IsCacheableNonGlobalScope(obj);
         scopeIndex++) {
        JSObject *pobj;
        JSProperty *prop;
        if (!LookupPropertyWithFlags(cx, obj, id, cx->resolveFlags, &pobj, &prop))
            return NULL;
        if (prop) {
            if (!pobj->isNative()) {
                JS_ASSERT(obj->isGlobal());
                return obj;
            }
            JS_ASSERT_IF(obj->isScope(), pobj->getClass() == obj->getClass());
            DebugOnly<PropertyCacheEntry*> entry =
                JS_PROPERTY_CACHE(cx).fill(cx, scopeChain, scopeIndex, pobj, (Shape *) prop);
            JS_ASSERT(entry);
            return obj;
        }

        JSObject *parent = obj->enclosingScope();
        if (!parent)
            return obj;
        obj = parent;
    }

    /* Loop until we find a property or reach the global object. */
    do {
        JSObject *pobj;
        JSProperty *prop;
        if (!obj->lookupGeneric(cx, id, &pobj, &prop))
            return NULL;
        if (prop)
            break;

        /*
         * We conservatively assume that a resolve hook could mutate the scope
         * chain during JSObject::lookupGeneric. So we must check if parent is
         * not null here even if it wasn't before the lookup.
         */
        JSObject *parent = obj->enclosingScope();
        if (!parent)
            break;
        obj = parent;
    } while (!obj->isGlobal());
    return obj;
}

static JS_ALWAYS_INLINE JSBool
js_NativeGetInline(JSContext *cx, JSObject *receiver, JSObject *obj, JSObject *pobj,
                   const Shape *shape, uintN getHow, Value *vp)
{
    JS_ASSERT(pobj->isNative());

    if (shape->hasSlot()) {
        *vp = pobj->nativeGetSlot(shape->slot());
        JS_ASSERT(!vp->isMagic());
        JS_ASSERT_IF(!pobj->hasSingletonType() && shape->hasDefaultGetterOrIsMethod(),
                     js::types::TypeHasProperty(cx, pobj->type(), shape->propid(), *vp));
    } else {
        vp->setUndefined();
    }
    if (shape->hasDefaultGetter())
        return true;

    if (JS_UNLIKELY(shape->isMethod()) && (getHow & JSGET_NO_METHOD_BARRIER))
        return true;

    jsbytecode *pc;
    JSScript *script = cx->stack.currentScript(&pc);
    if (script && script->hasAnalysis()) {
        analyze::Bytecode *code = script->analysis()->maybeCode(pc);
        if (code)
            code->accessGetter = true;
    }

    if (!shape->get(cx, receiver, obj, pobj, vp))
        return false;

    /* Update slotful shapes according to the value produced by the getter. */
    if (shape->hasSlot() && pobj->nativeContains(cx, *shape)) {
        /* Method shapes were removed by methodReadBarrier under shape->get(). */
        JS_ASSERT(!shape->isMethod());
        pobj->nativeSetSlot(shape->slot(), *vp);
    }

    return true;
}

JSBool
js_NativeGet(JSContext *cx, JSObject *obj, JSObject *pobj, const Shape *shape, uintN getHow,
             Value *vp)
{
    return js_NativeGetInline(cx, obj, obj, pobj, shape, getHow, vp);
}

JSBool
js_NativeSet(JSContext *cx, JSObject *obj, const Shape *shape, bool added, bool strict, Value *vp)
{
    AddTypePropertyId(cx, obj, shape->propid(), *vp);

    JS_ASSERT(obj->isNative());

    if (shape->hasSlot()) {
        uint32_t slot = shape->slot();

        /* If shape has a stub setter, just store *vp. */
        if (shape->hasDefaultSetter()) {
            if (!added) {
                if (shape->isMethod() && !obj->methodShapeChange(cx, *shape))
                    return false;
            }
            obj->nativeSetSlot(slot, *vp);
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

    int32_t sample = cx->runtime->propertyRemovals;
    if (!shape->set(cx, obj, strict, vp))
        return false;

    /*
     * Update any slot for the shape with the value produced by the setter,
     * unless the setter deleted the shape.
     */
    if (shape->hasSlot() &&
        (JS_LIKELY(cx->runtime->propertyRemovals == sample) ||
         obj->nativeContains(cx, *shape))) {
        obj->setSlot(shape->slot(), *vp);
    }

    return true;
}

static JS_ALWAYS_INLINE JSBool
js_GetPropertyHelperInline(JSContext *cx, JSObject *obj, JSObject *receiver, jsid id,
                           uint32_t getHow, Value *vp)
{
    JSObject *aobj, *obj2;
    JSProperty *prop;
    const Shape *shape;

    /* Convert string indices to integers if appropriate. */
    id = js_CheckForStringIndex(id);

    aobj = js_GetProtoIfDenseArray(obj);
    /* This call site is hot -- use the always-inlined variant of LookupPropertyWithFlags(). */
    if (!LookupPropertyWithFlagsInline(cx, aobj, id, cx->resolveFlags, &obj2, &prop))
        return false;

    if (!prop) {
        vp->setUndefined();

        if (!CallJSPropertyOp(cx, obj->getClass()->getProperty, obj, id, vp))
            return JS_FALSE;

        PCMETER(getHow & JSGET_CACHE_RESULT && JS_PROPERTY_CACHE(cx).nofills++);

        /* Record non-undefined values produced by the class getter hook. */
        if (!vp->isUndefined())
            AddTypePropertyId(cx, obj, id, *vp);

        /*
         * Give a strict warning if foo.bar is evaluated by a script for an
         * object foo with no property named 'bar'.
         */
        jsbytecode *pc;
        if (vp->isUndefined() && ((pc = js_GetCurrentBytecodePC(cx)) != NULL)) {
            JSOp op = (JSOp) *pc;

            if (op == JSOP_GETXPROP) {
                /* Undefined property during a name lookup, report an error. */
                JSAutoByteString printable;
                if (js_ValueToPrintable(cx, IdToValue(id), &printable))
                    js_ReportIsNotDefined(cx, printable.ptr());
                return false;
            }

            if (!cx->hasStrictOption() ||
                cx->stack.currentScript()->warnedAboutUndefinedProp ||
                (op != JSOP_GETPROP && op != JSOP_GETELEM)) {
                return JS_TRUE;
            }

            /*
             * XXX do not warn about missing __iterator__ as the function
             * may be called from JS_GetMethodById. See bug 355145.
             */
            if (JSID_IS_ATOM(id, cx->runtime->atomState.iteratorAtom))
                return JS_TRUE;

            /* Do not warn about tests like (obj[prop] == undefined). */
            if (cx->resolveFlags == RESOLVE_INFER) {
                pc += js_CodeSpec[op].length;
                if (Detecting(cx, pc))
                    return JS_TRUE;
            } else if (cx->resolveFlags & JSRESOLVE_DETECTING) {
                return JS_TRUE;
            }

            uintN flags = JSREPORT_WARNING | JSREPORT_STRICT;
            cx->stack.currentScript()->warnedAboutUndefinedProp = true;

            /* Ok, bad undefined property reference: whine about it. */
            if (!js_ReportValueErrorFlags(cx, flags, JSMSG_UNDEFINED_PROP,
                                          JSDVG_IGNORE_STACK, IdToValue(id),
                                          NULL, NULL, NULL))
            {
                return false;
            }
        }
        return JS_TRUE;
    }

    if (!obj2->isNative()) {
        return obj2->isProxy()
               ? Proxy::get(cx, obj2, receiver, id, vp)
               : obj2->getGeneric(cx, id, vp);
    }

    shape = (Shape *) prop;

    if (getHow & JSGET_CACHE_RESULT)
        JS_PROPERTY_CACHE(cx).fill(cx, aobj, 0, obj2, shape);

    /* This call site is hot -- use the always-inlined variant of js_NativeGet(). */
    if (!js_NativeGetInline(cx, receiver, obj, obj2, shape, getHow, vp))
        return JS_FALSE;

    return JS_TRUE;
}

JSBool
js_GetPropertyHelper(JSContext *cx, JSObject *obj, jsid id, uint32_t getHow, Value *vp)
{
    return js_GetPropertyHelperInline(cx, obj, obj, id, getHow, vp);
}

JSBool
js_GetProperty(JSContext *cx, JSObject *obj, JSObject *receiver, jsid id, Value *vp)
{
    /* This call site is hot -- use the always-inlined variant of js_GetPropertyHelper(). */
    return js_GetPropertyHelperInline(cx, obj, receiver, id, JSGET_METHOD_BARRIER, vp);
}

JSBool
js_GetElement(JSContext *cx, JSObject *obj, JSObject *receiver, uint32_t index, Value *vp)
{
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;

    /* This call site is hot -- use the always-inlined variant of js_GetPropertyHelper(). */
    return js_GetPropertyHelperInline(cx, obj, receiver, id, JSGET_METHOD_BARRIER, vp);
}

JSBool
js::GetPropertyDefault(JSContext *cx, JSObject *obj, jsid id, const Value &def, Value *vp)
{
    JSProperty *prop;
    JSObject *obj2;
    if (!LookupPropertyWithFlags(cx, obj, id, JSRESOLVE_QUALIFIED, &obj2, &prop))
        return false;

    if (!prop) {
        *vp = def;
        return true;
    }

    return js_GetProperty(cx, obj2, id, vp);
}

JSBool
js_GetMethod(JSContext *cx, JSObject *obj, jsid id, uintN getHow, Value *vp)
{
    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED);

    GenericIdOp op = obj->getOps()->getGeneric;
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
    return op(cx, obj, obj, id, vp);
}

JS_FRIEND_API(bool)
js::CheckUndeclaredVarAssignment(JSContext *cx, JSString *propname)
{
    StackFrame *const fp = js_GetTopStackFrame(cx, FRAME_EXPAND_ALL);
    if (!fp)
        return true;

    /* If neither cx nor the code is strict, then no check is needed. */
    if (!(fp->isScriptFrame() && fp->script()->strictModeCode) &&
        !cx->hasStrictOption()) {
        return true;
    }

    JSAutoByteString bytes(cx, propname);
    return !!bytes &&
           JS_ReportErrorFlagsAndNumber(cx,
                                        (JSREPORT_WARNING | JSREPORT_STRICT
                                         | JSREPORT_STRICT_MODE_ERROR),
                                        js_GetErrorMessage, NULL,
                                        JSMSG_UNDECLARED_VAR, bytes.ptr());
}

bool
JSObject::reportReadOnly(JSContext *cx, jsid id, uintN report)
{
    return js_ReportValueErrorFlags(cx, report, JSMSG_READ_ONLY,
                                    JSDVG_IGNORE_STACK, IdToValue(id), NULL,
                                    NULL, NULL);
}

bool
JSObject::reportNotConfigurable(JSContext *cx, jsid id, uintN report)
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

bool
JSObject::callMethod(JSContext *cx, jsid id, uintN argc, Value *argv, Value *vp)
{
    Value fval;
    return js_GetMethod(cx, this, id, JSGET_NO_METHOD_BARRIER, &fval) &&
           Invoke(cx, ObjectValue(*this), fval, argc, argv, vp);
}

static bool
CloneFunctionForSetMethod(JSContext *cx, Value *vp)
{
    JSFunction *fun = vp->toObject().toFunction();

    /* Clone the fun unless it already has been. */
    if (!fun->isClonedMethod()) {
        fun = CloneFunctionObject(cx, fun);
        if (!fun)
            return false;
        vp->setObject(*fun);
    }
    return true;
}

JSBool
js_SetPropertyHelper(JSContext *cx, JSObject *obj, jsid id, uintN defineHow,
                     Value *vp, JSBool strict)
{
    JSObject *pobj;
    JSProperty *prop;
    const Shape *shape;
    uintN attrs, flags;
    intN shortid;
    Class *clasp;
    PropertyOp getter;
    StrictPropertyOp setter;
    bool added;

    JS_ASSERT((defineHow & ~(DNP_CACHE_RESULT | DNP_SET_METHOD | DNP_UNQUALIFIED)) == 0);

    /* Convert string indices to integers if appropriate. */
    id = js_CheckForStringIndex(id);

    if (JS_UNLIKELY(obj->watched())) {
        /* Fire watchpoints, if any. */
        WatchpointMap *wpmap = cx->compartment->watchpointMap;
        if (wpmap && !wpmap->triggerWatchpoint(cx, obj, id, vp))
            return false;

        /* A watchpoint handler may set *vp to a non-function value. */
        defineHow &= ~DNP_SET_METHOD;
    }

    if (!LookupPropertyWithFlags(cx, obj, id, cx->resolveFlags, &pobj, &prop))
        return false;
    if (prop) {
        if (!pobj->isNative()) {
            if (pobj->isProxy()) {
                AutoPropertyDescriptorRooter pd(cx);
                if (!Proxy::getPropertyDescriptor(cx, pobj, id, true, &pd))
                    return false;

                if ((pd.attrs & (JSPROP_SHARED | JSPROP_SHADOWABLE)) == JSPROP_SHARED) {
                    return !pd.setter ||
                           CallSetter(cx, obj, id, pd.setter, pd.attrs, pd.shortid, strict, vp);
                }

                if (pd.attrs & JSPROP_READONLY) {
                    if (strict)
                        return obj->reportReadOnly(cx, id);
                    if (cx->hasStrictOption())
                        return obj->reportReadOnly(cx, id, JSREPORT_STRICT | JSREPORT_WARNING);
                    return true;
                }
            }

            prop = NULL;
        }
    } else {
        /* We should never add properties to lexical blocks.  */
        JS_ASSERT(!obj->isBlock());

        if (obj->isGlobal() &&
            (defineHow & DNP_UNQUALIFIED) &&
            !js::CheckUndeclaredVarAssignment(cx, JSID_TO_STRING(id))) {
            return JS_FALSE;
        }
    }
    shape = (Shape *) prop;

    /*
     * Now either shape is null, meaning id was not found in obj or one of its
     * prototypes; or shape is non-null, meaning id was found directly in pobj.
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
            if (shape->hasDefaultSetter())
                return js_ReportGetterOnlyAssignment(cx);
        } else {
            JS_ASSERT(shape->isDataDescriptor());

            if (!shape->writable()) {
                PCMETER((defineHow & JSDNP_CACHE_RESULT) && JS_PROPERTY_CACHE(cx).rofills++);

                /* Error in strict mode code, warn with strict option, otherwise do nothing. */
                if (strict)
                    return obj->reportReadOnly(cx, id);
                if (cx->hasStrictOption())
                    return obj->reportReadOnly(cx, id, JSREPORT_STRICT | JSREPORT_WARNING);
                return JS_TRUE;
            }
        }

        attrs = shape->attributes();
        if (pobj != obj) {
            /*
             * We found id in a prototype object: prepare to share or shadow.
             */
            if (!shape->shadowable()) {
                if (defineHow & DNP_SET_METHOD) {
                    JS_ASSERT(!shape->isMethod());
                    if (!CloneFunctionForSetMethod(cx, vp))
                        return false;
                }

                if (defineHow & DNP_CACHE_RESULT)
                    JS_PROPERTY_CACHE(cx).fill(cx, obj, 0, pobj, shape);

                if (shape->hasDefaultSetter() && !shape->hasGetterValue())
                    return JS_TRUE;

                return shape->set(cx, obj, strict, vp);
            }

            /*
             * Preserve attrs except JSPROP_SHARED, getter, and setter when
             * shadowing any property that has no slot (is shared). We must
             * clear the shared attribute for the shadowing shape so that the
             * property in obj that it defines has a slot to retain the value
             * being set, in case the setter simply cannot operate on instances
             * of obj's class by storing the value in some class-specific
             * location.
             *
             * A subset of slotless shared properties is the set of properties
             * with shortids, which must be preserved too. An old API requires
             * that the property's getter and setter receive the shortid, not
             * id, when they are called on the shadowing property that we are
             * about to create in obj.
             */
            if (!shape->hasSlot()) {
                defineHow &= ~DNP_SET_METHOD;
                if (shape->hasShortID()) {
                    flags = Shape::HAS_SHORTID;
                    shortid = shape->shortid();
                }
                attrs &= ~JSPROP_SHARED;
                getter = shape->getter();
                setter = shape->setter();
            } else {
                /* Restore attrs to the ECMA default for new properties. */
                attrs = JSPROP_ENUMERATE;
            }

            /*
             * Forget we found the proto-property now that we've copied any
             * needed member values.
             */
            shape = NULL;
        }

        if (shape && (defineHow & DNP_SET_METHOD)) {
            /*
             * JSOP_SETMETHOD is assigning to an existing own property. If it
             * is an identical method property, do nothing. Otherwise downgrade
             * to ordinary assignment. Either way, do not fill the property
             * cache, as the interpreter has no fast path for these unusual
             * cases.
             */
            if (shape->isMethod()) {
                if (obj->nativeGetMethod(shape) == &vp->toObject())
                    return true;
                shape = obj->methodShapeChange(cx, *shape);
                if (!shape)
                    return false;
            }
            if (!CloneFunctionForSetMethod(cx, vp))
                return false;
            return js_NativeSet(cx, obj, shape, false, strict, vp);
        }
    }

    added = false;
    if (!shape) {
        if (!obj->isExtensible()) {
            /* Error in strict mode code, warn with strict option, otherwise do nothing. */
            if (strict)
                return obj->reportNotExtensible(cx);
            if (cx->hasStrictOption())
                return obj->reportNotExtensible(cx, JSREPORT_STRICT | JSREPORT_WARNING);
            return JS_TRUE;
        }

        /*
         * Purge the property cache of now-shadowed id in obj's scope chain.
         * Do this early, before locking obj to avoid nesting locks.
         */
        if (!js_PurgeScopeChain(cx, obj, id))
            return JS_FALSE;

        /*
         * Check for Object class here to avoid defining a method on a class
         * with magic resolve, addProperty, getProperty, etc. hooks.
         */
        if ((defineHow & DNP_SET_METHOD) && obj->canHaveMethodBarrier()) {
            JS_ASSERT(IsFunctionObject(*vp));
            JS_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));

            JSObject *funobj = &vp->toObject();
            if (!funobj->toFunction()->isClonedMethod())
                flags |= Shape::METHOD;
        }

        shape = obj->putProperty(cx, id, getter, setter, SHAPE_INVALID_SLOT,
                                 attrs, flags, shortid);
        if (!shape)
            return JS_FALSE;

        /*
         * Initialize the new property value (passed to setter) to undefined.
         * Note that we store before calling addProperty, to match the order
         * in DefineNativeProperty.
         */
        if (shape->hasSlot())
            obj->nativeSetSlot(shape->slot(), UndefinedValue());

        /* XXXbe called with obj locked */
        if (!CallAddPropertyHook(cx, clasp, obj, shape, vp)) {
            obj->removeProperty(cx, id);
            return JS_FALSE;
        }
        added = true;
    }

    if ((defineHow & DNP_CACHE_RESULT) && !added)
        JS_PROPERTY_CACHE(cx).fill(cx, obj, 0, obj, shape);

    return js_NativeSet(cx, obj, shape, added, strict, vp);
}

JSBool
js_SetElementHelper(JSContext *cx, JSObject *obj, uint32_t index, uintN defineHow,
                    Value *vp, JSBool strict)
{
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;
    return js_SetPropertyHelper(cx, obj, id, defineHow, vp, strict);
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
        return obj->getGenericAttributes(cx, id, attrsp);

    const Shape *shape = (Shape *)prop;
    *attrsp = shape->attributes();
    return true;
}

JSBool
js_GetElementAttributes(JSContext *cx, JSObject *obj, uint32_t index, uintN *attrsp)
{
    JSProperty *prop;
    if (!js_LookupElement(cx, obj, index, &obj, &prop))
        return false;
    if (!prop) {
        *attrsp = 0;
        return true;
    }
    if (!obj->isNative())
        return obj->getElementAttributes(cx, index, attrsp);

    const Shape *shape = (Shape *)prop;
    *attrsp = shape->attributes();
    return true;
}

JSBool
js_SetNativeAttributes(JSContext *cx, JSObject *obj, Shape *shape, uintN attrs)
{
    JS_ASSERT(obj->isNative());
    return !!js_ChangeNativePropertyAttrs(cx, obj, shape, attrs, 0,
                                          shape->getter(), shape->setter());
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
           : obj->setGenericAttributes(cx, id, attrsp);
}

JSBool
js_SetElementAttributes(JSContext *cx, JSObject *obj, uint32_t index, uintN *attrsp)
{
    JSProperty *prop;
    if (!js_LookupElement(cx, obj, index, &obj, &prop))
        return false;
    if (!prop)
        return true;
    return obj->isNative()
           ? js_SetNativeAttributes(cx, obj, (Shape *) prop, *attrsp)
           : obj->setElementAttributes(cx, index, attrsp);
}

JSBool
js_DeleteProperty(JSContext *cx, JSObject *obj, jsid id, Value *rval, JSBool strict)
{
    JSObject *proto;
    JSProperty *prop;
    const Shape *shape;

    rval->setBoolean(true);

    /* Convert string indices to integers if appropriate. */
    id = js_CheckForStringIndex(id);

    if (!js_LookupProperty(cx, obj, id, &proto, &prop))
        return false;
    if (!prop || proto != obj) {
        /*
         * If no property, or the property comes from a prototype, call the
         * class's delProperty hook, passing rval as the result parameter.
         */
        return CallJSPropertyOp(cx, obj->getClass()->delProperty, obj, id, rval);
    }

    shape = (Shape *)prop;
    if (!shape->configurable()) {
        if (strict)
            return obj->reportNotConfigurable(cx, id);
        rval->setBoolean(false);
        return true;
    }

    if (shape->hasSlot()) {
        const Value &v = obj->nativeGetSlot(shape->slot());
        GCPoke(cx, v);

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
        JSFunction *fun;
        if (IsFunctionObject(v, &fun) && fun->isClonedMethod()) {
            for (StackFrame *fp = cx->maybefp(); fp; fp = fp->prev()) {
                if (fp->isFunctionFrame() &&
                    fp->fun()->script() == fun->script() &&
                    fp->thisValue().isObject())
                {
                    JSObject *tmp = &fp->thisValue().toObject();
                    do {
                        if (tmp == obj) {
                            fp->overwriteCallee(*fun);
                            break;
                        }
                    } while ((tmp = tmp->getProto()) != NULL);
                }
            }
        }
    }

    if (!CallJSPropertyOp(cx, obj->getClass()->delProperty, obj, shape->getUserId(), rval))
        return false;
    if (rval->isFalse())
        return true;

    return obj->removeProperty(cx, id) && js_SuppressDeletedProperty(cx, obj, id);
}

JSBool
js_DeleteElement(JSContext *cx, JSObject *obj, uint32_t index, Value *rval, JSBool strict)
{
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;
    return js_DeleteProperty(cx, obj, id, rval, strict);
}

namespace js {

bool
HasDataProperty(JSContext *cx, JSObject *obj, jsid methodid, Value *vp)
{
    if (const Shape *shape = obj->nativeLookup(cx, methodid)) {
        if (shape->hasDefaultGetterOrIsMethod() && shape->hasSlot()) {
            *vp = obj->nativeGetSlot(shape->slot());
            return true;
        }
    }

    return false;
}

/*
 * Gets |obj[id]|.  If that value's not callable, returns true and stores a
 * non-primitive value in *vp.  If it's callable, calls it with no arguments
 * and |obj| as |this|, returning the result in *vp.
 *
 * This is a mini-abstraction for ES5 8.12.8 [[DefaultValue]], either steps 1-2
 * or steps 3-4.
 */
static bool
MaybeCallMethod(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    if (!js_GetMethod(cx, obj, id, JSGET_NO_METHOD_BARRIER, vp))
        return false;
    if (!js_IsCallable(*vp)) {
        *vp = ObjectValue(*obj);
        return true;
    }
    return Invoke(cx, ObjectValue(*obj), *vp, 0, NULL, vp);
}

JSBool
DefaultValue(JSContext *cx, JSObject *obj, JSType hint, Value *vp)
{
    JS_ASSERT(hint == JSTYPE_NUMBER || hint == JSTYPE_STRING || hint == JSTYPE_VOID);
    JS_ASSERT(!obj->isXML());

    Class *clasp = obj->getClass();
    if (hint == JSTYPE_STRING) {
        /* Optimize (new String(...)).toString(). */
        if (clasp == &StringClass &&
            ClassMethodIsNative(cx, obj,
                                 &StringClass,
                                 ATOM_TO_JSID(cx->runtime->atomState.toStringAtom),
                                 js_str_toString)) {
            *vp = obj->getPrimitiveThis();
            return true;
        }

        if (!MaybeCallMethod(cx, obj, ATOM_TO_JSID(cx->runtime->atomState.toStringAtom), vp))
            return false;
        if (vp->isPrimitive())
            return true;

        if (!MaybeCallMethod(cx, obj, ATOM_TO_JSID(cx->runtime->atomState.valueOfAtom), vp))
            return false;
        if (vp->isPrimitive())
            return true;
    } else {
        /* Optimize (new String(...)).valueOf(). */
        if ((clasp == &StringClass &&
             ClassMethodIsNative(cx, obj, &StringClass,
                                 ATOM_TO_JSID(cx->runtime->atomState.valueOfAtom),
                                 js_str_toString)) ||
            (clasp == &NumberClass &&
             ClassMethodIsNative(cx, obj, &NumberClass,
                                 ATOM_TO_JSID(cx->runtime->atomState.valueOfAtom),
                                 js_num_valueOf))) {
            *vp = obj->getPrimitiveThis();
            return true;
        }

        if (!MaybeCallMethod(cx, obj, ATOM_TO_JSID(cx->runtime->atomState.valueOfAtom), vp))
            return false;
        if (vp->isPrimitive())
            return true;

        if (!MaybeCallMethod(cx, obj, ATOM_TO_JSID(cx->runtime->atomState.toStringAtom), vp))
            return false;
        if (vp->isPrimitive())
            return true;
    }

    /* Avoid recursive death when decompiling in js_ReportValueError. */
    JSString *str;
    if (hint == JSTYPE_STRING) {
        str = JS_InternString(cx, clasp->name);
        if (!str)
            return false;
    } else {
        str = NULL;
    }

    js_ReportValueError2(cx, JSMSG_CANT_CONVERT_TO, JSDVG_SEARCH_STACK, ObjectValue(*obj), str,
                         (hint == JSTYPE_VOID) ? "primitive type" : JS_TYPE_STR(hint));
    return false;
}

} /* namespace js */

JS_FRIEND_API(JSBool)
JS_EnumerateState(JSContext *cx, JSObject *obj, JSIterateOp enum_op, Value *statep, jsid *idp)
{
    /* If the class has a custom JSCLASS_NEW_ENUMERATE hook, call it. */
    Class *clasp = obj->getClass();
    JSEnumerateOp enumerate = clasp->enumerate;
    if (clasp->flags & JSCLASS_NEW_ENUMERATE) {
        JS_ASSERT(enumerate != JS_EnumerateStub);
        return ((JSNewEnumerateOp) enumerate)(cx, obj, enum_op, statep, idp);
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
    JSCheckAccessOp check;

    while (JS_UNLIKELY(obj->isWith()))
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
        if (!obj->lookupGeneric(cx, id, &pobj, &prop))
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
            if (shape->hasSlot())
                *vp = pobj->nativeGetSlot(shape->slot());
            else
                vp->setUndefined();
        }
    }

    JS_ASSERT_IF(*attrsp & JSPROP_READONLY, !(*attrsp & (JSPROP_GETTER | JSPROP_SETTER)));

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
    return !check || check(cx, pobj, id, mode, vp);
}

}

JSType
js_TypeOf(JSContext *cx, JSObject *obj)
{
    return obj->isCallable() ? JSTYPE_FUNCTION : JSTYPE_OBJECT;
}

bool
js_IsDelegate(JSContext *cx, JSObject *obj, const Value &v)
{
    if (v.isPrimitive())
        return false;
    JSObject *obj2 = &v.toObject();
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
        if (!ctor->getProperty(cx, cx->runtime->atomState.classPrototypeAtom, &v))
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
    JS_ASSERT(JSProto_Null <= protoKey);
    JS_ASSERT(protoKey < JSProto_LIMIT);

    if (protoKey != JSProto_Null) {
        GlobalObject *global;
        if (scopeobj) {
            global = &scopeobj->global();
        } else {
            global = GetCurrentGlobal(cx);
            if (!global) {
                *protop = NULL;
                return true;
            }
        }
        const Value &v = global->getReservedSlot(JSProto_LIMIT + protoKey);
        if (v.isObject()) {
            *protop = &v.toObject();
            return true;
        }
    }

    return FindClassPrototype(cx, scopeobj, protoKey, protop, clasp);
}

JSObject *
PrimitiveToObject(JSContext *cx, const Value &v)
{
    if (v.isString())
        return StringObject::create(cx, v.toString());
    if (v.isNumber())
        return NumberObject::create(cx, v.toNumber());

    JS_ASSERT(v.isBoolean());
    return BooleanObject::create(cx, v.toBoolean());
}

JSBool
js_PrimitiveToObject(JSContext *cx, Value *vp)
{
    JSObject *obj = PrimitiveToObject(cx, *vp);
    if (!obj)
        return false;

    vp->setObject(*obj);
    return true;
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
        obj = PrimitiveToObject(cx, v);
        if (!obj)
            return false;
    }
    *objp = obj;
    return true;
}

namespace js {

/* Callers must handle the already-object case . */
JSObject *
ToObjectSlow(JSContext *cx, Value *vp)
{
    JS_ASSERT(!vp->isMagic());
    JS_ASSERT(!vp->isObject());

    if (vp->isNullOrUndefined()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_CONVERT_TO,
                            vp->isNull() ? "null" : "undefined", "object");
        return NULL;
    }

    JSObject *obj = PrimitiveToObject(cx, *vp);
    if (obj)
        vp->setObject(*obj);
    return obj;
}

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

#if JS_HAS_XDR

JSBool
js_XDRObject(JSXDRState *xdr, JSObject **objp)
{
    JSContext *cx;
    JSAtom *atom;
    Class *clasp;
    uint32_t classId, classDef;
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
                atom = js_Atomize(cx, clasp->name, strlen(clasp->name));
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

#ifdef DEBUG
void
js_PrintObjectSlotName(JSTracer *trc, char *buf, size_t bufsize)
{
    JS_ASSERT(trc->debugPrinter == js_PrintObjectSlotName);

    JSObject *obj = (JSObject *)trc->debugPrintArg;
    uint32_t slot = uint32_t(trc->debugPrintIndex);

    const Shape *shape;
    if (obj->isNative()) {
        shape = obj->lastProperty();
        while (shape && (!shape->hasSlot() || shape->slot() != slot))
            shape = shape->previous();
    } else {
        shape = NULL;
    }

    if (!shape) {
        const char *slotname = NULL;
        if (obj->isGlobal()) {
#define JS_PROTO(name,code,init)                                              \
    if ((code) == slot) { slotname = js_##name##_str; goto found; }
#include "jsproto.tbl"
#undef JS_PROTO
        }
      found:
        if (slotname)
            JS_snprintf(buf, bufsize, "CLASS_OBJECT(%s)", slotname);
        else
            JS_snprintf(buf, bufsize, "**UNKNOWN SLOT %ld**", (long)slot);
    } else {
        jsid propid = shape->propid();
        if (JSID_IS_INT(propid)) {
            JS_snprintf(buf, bufsize, "%ld", (long)JSID_TO_INT(propid));
        } else if (JSID_IS_ATOM(propid)) {
            PutEscapedString(buf, bufsize, JSID_TO_ATOM(propid), 0);
        } else {
            JS_snprintf(buf, bufsize, "**FINALIZED ATOM KEY**");
        }
    }
}
#endif

static const Shape *
LastConfigurableShape(JSObject *obj)
{
    for (Shape::Range r(obj->lastProperty()->all()); !r.empty(); r.popFront()) {
        const Shape *shape = &r.front();
        if (shape->configurable())
            return shape;
    }
    return NULL;
}

bool
js_ClearNative(JSContext *cx, JSObject *obj)
{
    /* Remove all configurable properties from obj. */
    while (const Shape *shape = LastConfigurableShape(obj)) {
        if (!obj->removeProperty(cx, shape->propid()))
            return false;
    }

    /* Set all remaining writable plain data properties to undefined. */
    for (Shape::Range r(obj->lastProperty()->all()); !r.empty(); r.popFront()) {
        const Shape *shape = &r.front();
        if (shape->isDataDescriptor() &&
            shape->writable() &&
            shape->hasDefaultSetter() &&
            shape->hasSlot()) {
            obj->nativeSetSlot(shape->slot(), UndefinedValue());
        }
    }
    return true;
}

bool
js_GetReservedSlot(JSContext *cx, JSObject *obj, uint32_t slot, Value *vp)
{
    if (!obj->isNative()) {
        vp->setUndefined();
        return true;
    }

    JS_ASSERT(slot < JSSLOT_FREE(obj->getClass()));
    *vp = obj->getSlot(slot);
    return true;
}

bool
js_SetReservedSlot(JSContext *cx, JSObject *obj, uint32_t slot, const Value &v)
{
    if (!obj->isNative())
        return true;

    JS_ASSERT(slot < JSSLOT_FREE(obj->getClass()));
    obj->setSlot(slot, v);
    GCPoke(cx, NullValue());
    return true;
}

static ObjectElements emptyObjectHeader(0, 0);
HeapValue *js::emptyObjectElements =
    (HeapValue *) (jsuword(&emptyObjectHeader) + sizeof(ObjectElements));

JSBool
js_ReportGetterOnlyAssignment(JSContext *cx)
{
    return JS_ReportErrorFlagsAndNumber(cx,
                                        JSREPORT_WARNING | JSREPORT_STRICT |
                                        JSREPORT_STRICT_MODE_ERROR,
                                        js_GetErrorMessage, NULL,
                                        JSMSG_GETTER_ONLY);
}

JS_FRIEND_API(JSBool)
js_GetterOnlyPropertyStub(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_GETTER_ONLY);
    return JS_FALSE;
}

void
js::ReportIncompatibleMethod(JSContext *cx, CallReceiver call, Class *clasp)
{
    Value &thisv = call.thisv();

#ifdef DEBUG
    if (thisv.isObject()) {
        JS_ASSERT(thisv.toObject().getClass() != clasp);
    } else if (thisv.isString()) {
        JS_ASSERT(clasp != &StringClass);
    } else if (thisv.isNumber()) {
        JS_ASSERT(clasp != &NumberClass);
    } else if (thisv.isBoolean()) {
        JS_ASSERT(clasp != &BooleanClass);
    } else {
        JS_ASSERT(thisv.isUndefined() || thisv.isNull());
    }
#endif

    if (JSFunction *fun = js_ValueToFunction(cx, &call.calleev(), 0)) {
        JSAutoByteString funNameBytes;
        if (const char *funName = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                                 clasp->name, funName, InformalValueTypeName(thisv));
        }
    }
}

bool
js::HandleNonGenericMethodClassMismatch(JSContext *cx, CallArgs args, Native native, Class *clasp)
{
    if (args.thisv().isObject()) {
        JSObject &thisObj = args.thisv().toObject();
        if (thisObj.isProxy())
            return Proxy::nativeCall(cx, &thisObj, clasp, native, args);
    }

    ReportIncompatibleMethod(cx, args, clasp);
    return false;
}

JS_PUBLIC_API(size_t)
JS::SizeOfObjectDynamicSlots(JSObject *obj, JSMallocSizeOfFun mallocSizeOf)
{
    return obj->dynamicSlotSize(mallocSizeOf);
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
    if (const jschar *chars = str->getChars(NULL))
        dumpChars(chars, str->length());
    else
        fprintf(stderr, "(oom in dumpString)");
}

JS_FRIEND_API(void)
js_DumpString(JSString *str)
{
    if (const jschar *chars = str->getChars(NULL)) {
        fprintf(stderr, "JSString* (%p) = jschar * (%p) = ",
                (void *) str, (void *) chars);
        dumpString(str);
    } else {
        fprintf(stderr, "(oom in JS_DumpString)");
    }
    fputc('\n', stderr);
}

JS_FRIEND_API(void)
js_DumpAtom(JSAtom *atom)
{
    fprintf(stderr, "JSAtom* (%p) = ", (void *) atom);
    js_DumpString(atom);
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
        JSFunction *fun = v.toObject().toFunction();
        if (fun->atom) {
            fputs("<function ", stderr);
            FileEscapedString(stderr, fun->atom, 0);
        } else {
            fputs("<unnamed function", stderr);
        }
        if (fun->isInterpreted()) {
            JSScript *script = fun->script();
            fprintf(stderr, " (%s:%u)",
                    script->filename ? script->filename : "", script->lineno);
        }
        fprintf(stderr, " at %p>", (void *) fun);
    } else if (v.isObject()) {
        JSObject *obj = &v.toObject();
        Class *clasp = obj->getClass();
        fprintf(stderr, "<%s%s at %p>",
                clasp->name,
                (clasp == &ObjectClass) ? "" : " object",
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
DumpProperty(JSObject *obj, const Shape &shape)
{
    jsid id = shape.propid();
    uint8_t attrs = shape.attributes();

    fprintf(stderr, "    ((Shape *) %p) ", (void *) &shape);
    if (attrs & JSPROP_ENUMERATE) fprintf(stderr, "enumerate ");
    if (attrs & JSPROP_READONLY) fprintf(stderr, "readonly ");
    if (attrs & JSPROP_PERMANENT) fprintf(stderr, "permanent ");
    if (attrs & JSPROP_SHARED) fprintf(stderr, "shared ");
    if (shape.isMethod()) fprintf(stderr, "method ");

    if (shape.hasGetterValue())
        fprintf(stderr, "getterValue=%p ", (void *) shape.getterObject());
    else if (!shape.hasDefaultGetter())
        fprintf(stderr, "getterOp=%p ", JS_FUNC_TO_DATA_PTR(void *, shape.getterOp()));

    if (shape.hasSetterValue())
        fprintf(stderr, "setterValue=%p ", (void *) shape.setterObject());
    else if (!shape.hasDefaultSetter())
        fprintf(stderr, "setterOp=%p ", JS_FUNC_TO_DATA_PTR(void *, shape.setterOp()));

    if (JSID_IS_ATOM(id))
        dumpString(JSID_TO_STRING(id));
    else if (JSID_IS_INT(id))
        fprintf(stderr, "%d", (int) JSID_TO_INT(id));
    else
        fprintf(stderr, "unknown jsid %p", (void *) JSID_BITS(id));

    uint32_t slot = shape.hasSlot() ? shape.maybeSlot() : SHAPE_INVALID_SLOT;
    fprintf(stderr, ": slot %d", slot);
    if (shape.hasSlot()) {
        fprintf(stderr, " = ");
        dumpValue(obj->getSlot(slot));
    } else if (slot != SHAPE_INVALID_SLOT) {
        fprintf(stderr, " (INVALID!)");
    }
    fprintf(stderr, "\n");
}

JS_FRIEND_API(void)
js_DumpObject(JSObject *obj)
{
    fprintf(stderr, "object %p\n", (void *) obj);
    Class *clasp = obj->getClass();
    fprintf(stderr, "class %p %s\n", (void *)clasp, clasp->name);

    fprintf(stderr, "flags:");
    if (obj->isDelegate()) fprintf(stderr, " delegate");
    if (obj->isSystem()) fprintf(stderr, " system");
    if (!obj->isExtensible()) fprintf(stderr, " not_extensible");
    if (obj->isIndexed()) fprintf(stderr, " indexed");

    if (obj->isNative()) {
        if (obj->inDictionaryMode())
            fprintf(stderr, " inDictionaryMode");
        if (obj->hasPropertyTable())
            fprintf(stderr, " hasPropertyTable");
    }
    fprintf(stderr, "\n");

    if (obj->isDenseArray()) {
        unsigned slots = obj->getDenseArrayInitializedLength();
        fprintf(stderr, "elements\n");
        for (unsigned i = 0; i < slots; i++) {
            fprintf(stderr, " %3d: ", i);
            dumpValue(obj->getDenseArrayElement(i));
            fprintf(stderr, "\n");
            fflush(stderr);
        }
        return;
    }

    fprintf(stderr, "proto ");
    dumpValue(ObjectOrNullValue(obj->getProto()));
    fputc('\n', stderr);

    fprintf(stderr, "parent ");
    dumpValue(ObjectOrNullValue(obj->getParent()));
    fputc('\n', stderr);

    if (clasp->flags & JSCLASS_HAS_PRIVATE)
        fprintf(stderr, "private %p\n", obj->getPrivate());

    if (!obj->isNative())
        fprintf(stderr, "not native\n");

    unsigned reservedEnd = JSCLASS_RESERVED_SLOTS(clasp);
    unsigned slots = obj->slotSpan();
    unsigned stop = obj->isNative() ? reservedEnd : slots;
    if (stop > 0)
        fprintf(stderr, obj->isNative() ? "reserved slots:\n" : "slots:\n");
    for (unsigned i = 0; i < stop; i++) {
        fprintf(stderr, " %3d ", i);
        if (i < reservedEnd)
            fprintf(stderr, "(reserved) ");
        fprintf(stderr, "= ");
        dumpValue(obj->getSlot(i));
        fputc('\n', stderr);
    }

    if (obj->isNative()) {
        fprintf(stderr, "properties:\n");
        Vector<const Shape *, 8, SystemAllocPolicy> props;
        for (Shape::Range r = obj->lastProperty()->all(); !r.empty(); r.popFront())
            props.append(&r.front());
        for (size_t i = props.length(); i-- != 0;)
            DumpProperty(obj, *props[i]);
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
js_DumpStackFrame(JSContext *cx, StackFrame *start)
{
    /* This should only called during live debugging. */
    FrameRegsIter i(cx, StackIter::GO_THROUGH_SAVED);
    if (!start) {
        if (i.done()) {
            fprintf(stderr, "no stack for cx = %p\n", (void*) cx);
            return;
        }
    } else {
        while (!i.done() && i.fp() != start)
            ++i;

        if (i.done()) {
            fprintf(stderr, "fp = %p not found in cx = %p\n",
                    (void *)start, (void *)cx);
            return;
        }
    }

    for (; !i.done(); ++i) {
        StackFrame *const fp = i.fp();

        fprintf(stderr, "StackFrame at %p\n", (void *) fp);
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
            fprintf(stderr, "  pc = %p\n", pc);
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
        if (fp->hasArgs()) {
            fprintf(stderr, "  actuals: %p (%u) ", (void *) fp->actualArgs(), (unsigned) fp->numActualArgs());
            fprintf(stderr, "  formals: %p (%u)\n", (void *) fp->formalArgs(), (unsigned) fp->numFormalArgs());
        }
        if (fp->hasCallObj()) {
            fprintf(stderr, "  has call obj: ");
            dumpValue(ObjectValue(fp->callObj()));
            fprintf(stderr, "\n");
        }
        MaybeDumpObject("argsobj", fp->maybeArgsObj());
        MaybeDumpObject("blockChain", fp->maybeBlockChain());
        if (!fp->isDummyFrame()) {
            MaybeDumpValue("this", fp->thisValue());
            fprintf(stderr, "  rval: ");
            dumpValue(fp->returnValue());
        } else {
            fprintf(stderr, "dummy frame");
        }
        fputc('\n', stderr);

        fprintf(stderr, "  flags:");
        if (fp->isConstructing())
            fprintf(stderr, " constructing");
        if (fp->hasOverriddenArgs())
            fprintf(stderr, " overridden_args");
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

#endif /* DEBUG */

