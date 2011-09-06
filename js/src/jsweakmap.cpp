/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andreas Gal <gal@mozilla.com>
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

#include <string.h>
#include "jsapi.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jshashtable.h"
#include "jsobj.h"
#include "jsgc.h"
#include "jsgcmark.h"
#include "jsweakmap.h"

#include "jsgcinlines.h"
#include "jsobjinlines.h"

using namespace js;

namespace js {

bool
WeakMapBase::markAllIteratively(JSTracer *tracer)
{
    bool markedAny = false;
    JSRuntime *rt = tracer->context->runtime;
    for (WeakMapBase *m = rt->gcWeakMapList; m; m = m->next) {
        if (m->markIteratively(tracer))
            markedAny = true;
    }
    return markedAny;
}

void
WeakMapBase::sweepAll(JSTracer *tracer)
{
    JSRuntime *rt = tracer->context->runtime;
    for (WeakMapBase *m = rt->gcWeakMapList; m; m = m->next)
        m->sweep(tracer);
}

} /* namespace js */

typedef WeakMap<JSObject *, Value> ObjectValueMap;

static ObjectValueMap *
GetObjectMap(JSObject *obj)
{
    JS_ASSERT(obj->isWeakMap());
    return (ObjectValueMap *)obj->getPrivate();
}

static JSObject *
NonNullObject(JSContext *cx, Value *vp)
{
    if (vp->isPrimitive()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_NONNULL_OBJECT);
        return NULL;
    }
    return &vp->toObject();
}

static JSBool
WeakMap_has(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;
    if (!obj->isWeakMap()) {
        ReportIncompatibleMethod(cx, vp, &WeakMapClass);
        return false;
    }
    if (argc < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "WeakMap.has", "0", "s");
        return false;
    }
    JSObject *key = NonNullObject(cx, &vp[2]);
    if (!key)
        return false;
    ObjectValueMap *map = GetObjectMap(obj);
    if (map) {
        ObjectValueMap::Ptr ptr = map->lookup(key);
        if (ptr) {
            *vp = BooleanValue(true);
            return true;
        }
    }

    *vp = BooleanValue(false);
    return true;
}

static JSBool
WeakMap_get(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;
    if (!obj->isWeakMap()) {
        ReportIncompatibleMethod(cx, vp, &WeakMapClass);
        return false;
    }
    if (argc < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "WeakMap.get", "0", "s");
        return false;
    }
    JSObject *key = NonNullObject(cx, &vp[2]);
    if (!key)
        return false;
    ObjectValueMap *map = GetObjectMap(obj);
    if (map) {
        ObjectValueMap::Ptr ptr = map->lookup(key);
        if (ptr) {
            *vp = ptr->value;
            return true;
        }
    }

    *vp = (argc > 1) ? vp[3] : UndefinedValue();
    return true;
}

static JSBool
WeakMap_delete(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;
    if (!obj->isWeakMap()) {
        ReportIncompatibleMethod(cx, vp, &WeakMapClass);
        return false;
    }
    if (argc < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "WeakMap.delete", "0", "s");
        return false;
    }
    JSObject *key = NonNullObject(cx, &vp[2]);
    if (!key)
        return false;
    ObjectValueMap *map = GetObjectMap(obj);
    if (map) {
        ObjectValueMap::Ptr ptr = map->lookup(key);
        if (ptr) {
            map->remove(ptr);
            *vp = BooleanValue(true);
            return true;
        }
    }

    *vp = BooleanValue(false);
    return true;
}

static JSBool
WeakMap_set(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;
    if (!obj->isWeakMap()) {
        ReportIncompatibleMethod(cx, vp, &WeakMapClass);
        return false;
    }
    if (argc < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "WeakMap.set", "0", "s");
        return false;
    }
    JSObject *key = NonNullObject(cx, &vp[2]);
    if (!key)
        return false;
    Value value = (argc > 1) ? vp[3] : UndefinedValue();

    ObjectValueMap *map = GetObjectMap(obj);
    if (!map) {
        map = cx->new_<ObjectValueMap>(cx);
        if (!map->init()) {
            cx->delete_(map);
            goto out_of_memory;
        }
        obj->setPrivate(map);
    }

    *vp = UndefinedValue();
    if (!map->put(key, value))
        goto out_of_memory;
    return true;

  out_of_memory:
    JS_ReportOutOfMemory(cx);
    return false;
}

static void
WeakMap_mark(JSTracer *trc, JSObject *obj)
{
    if (ObjectValueMap *map = GetObjectMap(obj))
        map->trace(trc);
}

static void
WeakMap_finalize(JSContext *cx, JSObject *obj)
{
    ObjectValueMap *map = GetObjectMap(obj);
    cx->delete_(map);
}

static JSBool
WeakMap_construct(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = NewBuiltinClassInstance(cx, &WeakMapClass);
    if (!obj)
        return false;

    obj->setPrivate(NULL);

    vp->setObject(*obj);
    return true;
}

Class js::WeakMapClass = {
    "WeakMap",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_WeakMap),
    PropertyStub,         /* addProperty */
    PropertyStub,         /* delProperty */
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub,
    WeakMap_finalize,
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    NULL,                 /* hasInstance */
    WeakMap_mark
};

static JSFunctionSpec weak_map_methods[] = {
    JS_FN("has",    WeakMap_has, 1, 0),
    JS_FN("get",    WeakMap_get, 2, 0),
    JS_FN("delete", WeakMap_delete, 1, 0),
    JS_FN("set",    WeakMap_set, 2, 0),
    JS_FS_END
};

JSObject *
js_InitWeakMapClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto = js_InitClass(cx, obj, NULL, &WeakMapClass, WeakMap_construct, 0,
                                   NULL, weak_map_methods, NULL, NULL);
    if (!proto)
        return NULL;

    proto->setPrivate(NULL);

    return proto;
}
