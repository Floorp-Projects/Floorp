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

bool
JSObject::isWeakMap() const
{
    return getClass() == &WeakMap::jsclass;
}

namespace js {

WeakMap::WeakMap(JSContext *cx) :
    map(cx),
    next(NULL)
{
}

WeakMap *
WeakMap::fromJSObject(JSObject *obj)
{
    JS_ASSERT(obj->getClass() == &WeakMap::jsclass);
    return (WeakMap *)obj->getPrivate();
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

JSBool
WeakMap::has(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;
    if (!obj->isWeakMap()) {
        ReportIncompatibleMethod(cx, vp, &WeakMap::jsclass);
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
    WeakMap *weakmap = fromJSObject(obj);
    if (weakmap) {
        js::HashMap<JSObject *, Value>::Ptr ptr = weakmap->map.lookup(key);
        if (ptr) {
            *vp = BooleanValue(true);
            return true;
        }
    }

    *vp = BooleanValue(false);
    return true;
}

JSBool
WeakMap::get(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;
    if (!obj->isWeakMap()) {
        ReportIncompatibleMethod(cx, vp, &WeakMap::jsclass);
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
    WeakMap *weakmap = fromJSObject(obj);
    if (weakmap) {
        js::HashMap<JSObject *, Value>::Ptr ptr = weakmap->map.lookup(key);
        if (ptr) {
            *vp = ptr->value;
            return true;
        }
    }

    *vp = (argc > 1) ? vp[3] : UndefinedValue();
    return true;
}

JSBool
WeakMap::delete_(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;
    if (!obj->isWeakMap()) {
        ReportIncompatibleMethod(cx, vp, &WeakMap::jsclass);
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
    WeakMap *weakmap = fromJSObject(obj);
    if (weakmap) {
        js::HashMap<JSObject *, Value>::Ptr ptr = weakmap->map.lookup(key);
        if (ptr) {
            weakmap->map.remove(ptr);
            *vp = BooleanValue(true);
            return true;
        }
    }

    *vp = BooleanValue(false);
    return true;
}

JSBool
WeakMap::set(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;
    if (!obj->isWeakMap()) {
        ReportIncompatibleMethod(cx, vp, &WeakMap::jsclass);
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

    WeakMap *table = (WeakMap *)obj->getPrivate();
    if (!table) {
        table = cx->new_<WeakMap>(cx);
        if (!table->map.init()) {
            cx->delete_(table);
            goto out_of_memory;
        }
        obj->setPrivate(table);
    }

    *vp = UndefinedValue();
    return table->map.put(key, value) != NULL;

  out_of_memory:
    JS_ReportOutOfMemory(cx);
    return false;
}

void
WeakMap::mark(JSTracer *trc, JSObject *obj)
{
    WeakMap *table = fromJSObject(obj);
    if (table) {
        if (IS_GC_MARKING_TRACER(trc)) {
            if (table->map.empty()) {
                trc->context->delete_(table);
                obj->setPrivate(NULL);
                return;
            }
            JSRuntime *rt = trc->context->runtime;
            table->next = rt->gcWeakMapList;
            rt->gcWeakMapList = obj;
        } else {
            for (js::HashMap<JSObject *, Value>::Range r = table->map.all(); !r.empty(); r.popFront()) {
                JSObject *key = r.front().key;
                Value &value = r.front().value;
                js::gc::MarkObject(trc, *key, "key");
                js::gc::MarkValue(trc, value, "value");
            }
        }
    }
}

/*
 * Walk through the previously collected list of tables and mark rows
 * iteratively.
 */
bool
WeakMap::markIteratively(JSTracer *trc)
{
    JSContext *cx = trc->context;
    JSRuntime *rt = cx->runtime;

    bool again = false;
    JSObject *obj = rt->gcWeakMapList;
    while (obj) {
        WeakMap *table = fromJSObject(obj);
        for (js::HashMap<JSObject *, Value>::Range r = table->map.all(); !r.empty(); r.popFront()) {
            JSObject *key = r.front().key;
            Value &value = r.front().value;
            if (value.isMarkable() && !IsAboutToBeFinalized(cx, key)) {
                /* If the key is alive, mark the value if needed. */
                if (IsAboutToBeFinalized(cx, value.toGCThing())) {
                    js::gc::MarkValue(trc, value, "value");
                    /* We revived a value with children, we have to iterate again. */
                    if (value.isGCThing())
                        again = true;
                }
            }
        }
        obj = table->next;
    }
    return again;
}

void
WeakMap::sweep(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;

    JSObject *obj = rt->gcWeakMapList;
    while (obj) {
        WeakMap *table = fromJSObject(obj);
        for (js::HashMap<JSObject *, Value>::Enum e(table->map); !e.empty(); e.popFront()) {
            if (IsAboutToBeFinalized(cx, e.front().key))
                e.removeFront();
        }
        obj = table->next;
    }

    rt->gcWeakMapList = NULL;
}

void
WeakMap::finalize(JSContext *cx, JSObject *obj)
{
    WeakMap *table = fromJSObject(obj);
    if (table)
        cx->delete_(table);
}

JSBool
WeakMap::construct(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = NewBuiltinClassInstance(cx, &WeakMap::jsclass);
    if (!obj)
        return false;

    obj->setPrivate(NULL);

    vp->setObject(*obj);
    return true;
}

Class WeakMap::jsclass = {
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
    WeakMap::finalize,
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    NULL,                 /* hasInstance */
    WeakMap::mark
};

}

JSFunctionSpec WeakMap::methods[] = {
    JS_FN("has",    WeakMap::has, 1, 0),
    JS_FN("get",    WeakMap::get, 2, 0),
    JS_FN("delete", WeakMap::delete_, 1, 0),
    JS_FN("set",    WeakMap::set, 2, 0),
    JS_FS_END
};

JSObject *
js_InitWeakMapClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto = js_InitClass(cx, obj, NULL, &WeakMap::jsclass, WeakMap::construct, 0,
                                   NULL, WeakMap::methods, NULL, NULL);
    if (!proto)
        return NULL;

    proto->setPrivate(NULL);

    return proto;
}
