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
#include "jsfriendapi.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsgcmark.h"
#include "jsweakmap.h"

#include "vm/GlobalObject.h"

#include "jsgcinlines.h"
#include "jsobjinlines.h"

#include "vm/MethodGuard-inl.h"

using namespace js;

namespace js {

bool
WeakMapBase::markAllIteratively(JSTracer *tracer)
{
    bool markedAny = false;
    JSRuntime *rt = tracer->runtime;
    for (WeakMapBase *m = rt->gcWeakMapList; m; m = m->next) {
        if (m->markIteratively(tracer))
            markedAny = true;
    }
    return markedAny;
}

void
WeakMapBase::sweepAll(JSTracer *tracer)
{
    JSRuntime *rt = tracer->runtime;
    for (WeakMapBase *m = rt->gcWeakMapList; m; m = m->next)
        m->sweep(tracer);
}

void
WeakMapBase::traceAllMappings(WeakMapTracer *tracer)
{
    JSRuntime *rt = tracer->runtime;
    for (WeakMapBase *m = rt->gcWeakMapList; m; m = m->next)
        m->traceMappings(tracer);
}

void
WeakMapBase::resetWeakMapList(JSRuntime *rt)
{
    JS_ASSERT(WeakMapNotInList != NULL);

    WeakMapBase *m = rt->gcWeakMapList;
    rt->gcWeakMapList = NULL;
    while (m) {
        WeakMapBase *n = m->next;
        m->next = WeakMapNotInList;
        m = n;
    }
}

bool
WeakMapBase::saveWeakMapList(JSRuntime *rt, WeakMapVector &vector)
{
    WeakMapBase *m = rt->gcWeakMapList;
    while (m) {
        if (!vector.append(m))
            return false;
        m = m->next;
    }
    return true;
}

void
WeakMapBase::restoreWeakMapList(JSRuntime *rt, WeakMapVector &vector)
{
    JS_ASSERT(!rt->gcWeakMapList);
    for (WeakMapBase **p = vector.begin(); p != vector.end(); p++) {
        WeakMapBase *m = *p;
        JS_ASSERT(m->next == WeakMapNotInList);
        m->next = rt->gcWeakMapList;
        rt->gcWeakMapList = m;
    }
}

} /* namespace js */

typedef WeakMap<HeapPtr<JSObject>, HeapValue> ObjectValueMap;

static ObjectValueMap *
GetObjectMap(JSObject *obj)
{
    JS_ASSERT(obj->isWeakMap());
    return (ObjectValueMap *)obj->getPrivate();
}

static JSObject *
GetKeyArg(JSContext *cx, CallArgs &args) 
{
    Value *vp = &args[0];
    if (vp->isPrimitive()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_NONNULL_OBJECT);
        return NULL;
    }
    JSObject &key = vp->toObject();

    // If the key is from another compartment, and we store the wrapper as the key
    // the wrapper might be GC-ed since it is not strong referenced (Bug 673468).
    // To avoid this we always use the unwrapped object as the key instead of its
    // security wrapper. This also means that if the keys are ever exposed they must
    // be re-wrapped (see: JS_NondeterministicGetWeakMapKeys).
    return JS_UnwrapObject(&key);
}

static JSBool
WeakMap_has(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    bool ok;
    JSObject *obj = NonGenericMethodGuard(cx, args, WeakMap_has, &WeakMapClass, &ok);
    if (!obj)
        return ok;

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "WeakMap.has", "0", "s");
        return false;
    }
    JSObject *key = GetKeyArg(cx, args);
    if (!key)
        return false;

    ObjectValueMap *map = GetObjectMap(obj);
    if (map) {
        ObjectValueMap::Ptr ptr = map->lookup(key);
        if (ptr) {
            args.rval() = BooleanValue(true);
            return true;
        }
    }

    args.rval() = BooleanValue(false);
    return true;
}

static JSBool
WeakMap_get(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    bool ok;
    JSObject *obj = NonGenericMethodGuard(cx, args, WeakMap_get, &WeakMapClass, &ok);
    if (!obj)
        return ok;

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "WeakMap.get", "0", "s");
        return false;
    }
    JSObject *key = GetKeyArg(cx, args);
    if (!key)
        return false;

    ObjectValueMap *map = GetObjectMap(obj);
    if (map) {
        ObjectValueMap::Ptr ptr = map->lookup(key);
        if (ptr) {
            args.rval() = ptr->value;
            return true;
        }
    }

    args.rval() = (args.length() > 1) ? args[1] : UndefinedValue();
    return true;
}

static JSBool
WeakMap_delete(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    bool ok;
    JSObject *obj = NonGenericMethodGuard(cx, args, WeakMap_delete, &WeakMapClass, &ok);
    if (!obj)
        return ok;

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "WeakMap.delete", "0", "s");
        return false;
    }
    JSObject *key = GetKeyArg(cx, args);
    if (!key)
        return false;
    
    ObjectValueMap *map = GetObjectMap(obj);
    if (map) {
        ObjectValueMap::Ptr ptr = map->lookup(key);
        if (ptr) {
            map->remove(ptr);
            args.rval() = BooleanValue(true);
            return true;
        }
    }

    args.rval() = BooleanValue(false);
    return true;
}

static JSBool
WeakMap_set(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    bool ok;
    JSObject *obj = NonGenericMethodGuard(cx, args, WeakMap_set, &WeakMapClass, &ok);
    if (!obj)
        return ok;

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "WeakMap.set", "0", "s");
        return false;
    }
    JSObject *key = GetKeyArg(cx, args);
    if (!key)
        return false;
    
    Value value = (args.length() > 1) ? args[1] : UndefinedValue();

    ObjectValueMap *map = GetObjectMap(obj);
    if (!map) {
        map = cx->new_<ObjectValueMap>(cx, obj);
        if (!map->init()) {
            cx->delete_(map);
            goto out_of_memory;
        }
        obj->setPrivate(map);
    }

    if (!map->put(key, value))
        goto out_of_memory;

    // Preserve wrapped native keys to prevent wrapper optimization.
    if (key->getClass()->ext.isWrappedNative) {
        if (!cx->runtime->preserveWrapperCallback ||
            !cx->runtime->preserveWrapperCallback(cx, key)) {
            JS_ReportWarning(cx, "Failed to preserve wrapper of wrapped native weak map key.");
        }
    }

    args.rval().setUndefined();
    return true;

  out_of_memory:
    JS_ReportOutOfMemory(cx);
    return false;
}

JS_FRIEND_API(JSBool)
JS_NondeterministicGetWeakMapKeys(JSContext *cx, JSObject *obj, JSObject **ret)
{
    if (!obj || !obj->isWeakMap()) {
        *ret = NULL;
        return true;
    }
    JSObject *arr = NewDenseEmptyArray(cx);
    if (!arr)
        return false;
    ObjectValueMap *map = GetObjectMap(obj);
    if (map) {
        for (ObjectValueMap::Range r = map->nondeterministicAll(); !r.empty(); r.popFront()) {
            JSObject *key = r.front().key;
            // Re-wrapping the key (see comment of GetKeyArg)
            if (!JS_WrapObject(cx, &key))
                return false;

            if (!js_NewbornArrayPush(cx, arr, ObjectValue(*key)))
                return false;
        }
    }
    *ret = arr;
    return true;
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
    if (ObjectValueMap *map = GetObjectMap(obj)) {
        map->check();
#ifdef DEBUG
        map->~ObjectValueMap();
        memset(static_cast<void *>(map), 0xdc, sizeof(*map));
        cx->free_(map);
#else
        cx->delete_(map);
#endif
    }
}

static JSBool
WeakMap_construct(JSContext *cx, unsigned argc, Value *vp)
{
    JSObject *obj = NewBuiltinClassInstance(cx, &WeakMapClass);
    if (!obj)
        return false;

    vp->setObject(*obj);
    return true;
}

Class js::WeakMapClass = {
    "WeakMap",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_CACHED_PROTO(JSProto_WeakMap),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    WeakMap_finalize,
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* xdrObject   */
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
    JS_ASSERT(obj->isNative());

    GlobalObject *global = &obj->asGlobal();

    JSObject *weakMapProto = global->createBlankPrototype(cx, &WeakMapClass);
    if (!weakMapProto)
        return NULL;

    JSFunction *ctor = global->createConstructor(cx, WeakMap_construct, &WeakMapClass,
                                                 CLASS_ATOM(cx, WeakMap), 0);
    if (!ctor)
        return NULL;

    if (!LinkConstructorAndPrototype(cx, ctor, weakMapProto))
        return NULL;

    if (!DefinePropertiesAndBrand(cx, weakMapProto, NULL, weak_map_methods))
        return NULL;

    if (!DefineConstructorAndPrototype(cx, global, JSProto_WeakMap, ctor, weakMapProto))
        return NULL;
    return weakMapProto;
}
