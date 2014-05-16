/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JavaScriptChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsContentUtils.h"
#include "xpcprivate.h"
#include "jsfriendapi.h"
#include "nsCxPusher.h"

using namespace JS;
using namespace mozilla;
using namespace mozilla::jsipc;

using mozilla::AutoSafeJSContext;

JavaScriptChild::JavaScriptChild(JSRuntime *rt)
  : lastId_(0),
    rt_(rt)
{
}

static void
Trace(JSTracer *trc, void *data)
{
    reinterpret_cast<JavaScriptChild *>(data)->trace(trc);
}

JavaScriptChild::~JavaScriptChild()
{
    JS_RemoveExtraGCRootsTracer(rt_, Trace, this);
}

void
JavaScriptChild::trace(JSTracer *trc)
{
    objects_.trace(trc);
    cpows_.trace(trc);
    ids_.trace(trc);
}

bool
JavaScriptChild::init()
{
    if (!WrapperOwner::init())
        return false;
    if (!WrapperAnswer::init())
        return false;

    if (!ids_.init())
        return false;

    JS_AddExtraGCRootsTracer(rt_, Trace, this);
    return true;
}

bool
JavaScriptChild::RecvDropObject(const ObjectId &objId)
{
    JSObject *obj = findObjectById(objId);
    if (obj) {
        ids_.remove(obj);
        objects_.remove(objId);
    }
    return true;
}

bool
JavaScriptChild::toObjectVariant(JSContext *cx, JSObject *obj, ObjectVariant *objVarp)
{
    JS_ASSERT(obj);

    ObjectId id = ids_.find(obj);
    if (id) {
        *objVarp = RemoteObject(id);
        return true;
    }

    id = ++lastId_;
    if (id > MAX_CPOW_IDS) {
        JS_ReportError(cx, "CPOW id limit reached");
        return false;
    }

    id <<= OBJECT_EXTRA_BITS;
    if (JS_ObjectIsCallable(cx, obj))
        id |= OBJECT_IS_CALLABLE;

    if (!objects_.add(id, obj))
        return false;
    if (!ids_.add(cx, obj, id))
        return false;

    *objVarp = RemoteObject(id);
    return true;
}

JSObject *
JavaScriptChild::fromObjectVariant(JSContext *cx, ObjectVariant objVar)
{
    JS_ASSERT(objVar.type() == ObjectVariant::TLocalObject);
    ObjectId id = objVar.get_LocalObject().id();
    JSObject *obj = findObjectById(id);
    MOZ_ASSERT(obj);
    return obj;
}

