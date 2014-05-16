/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JavaScriptParent.h"
#include "mozilla/dom/ContentParent.h"
#include "nsJSUtils.h"
#include "jsfriendapi.h"
#include "jsproxy.h"
#include "jswrapper.h"
#include "HeapAPI.h"
#include "xpcprivate.h"
#include "mozilla/Casting.h"

using namespace js;
using namespace JS;
using namespace mozilla;
using namespace mozilla::jsipc;
using namespace mozilla::dom;

JavaScriptParent::JavaScriptParent()
  : refcount_(1)
{
}

void
JavaScriptParent::drop(JSObject *obj)
{
    ObjectId objId = idOf(obj);

    cpows_.remove(objId);
    if (active() && !SendDropObject(objId))
        (void)0;
    decref();
}

bool
JavaScriptParent::init()
{
    if (!WrapperOwner::init())
        return false;

    return true;
}

bool
JavaScriptParent::toId(JSContext *cx, JSObject *obj, ObjectId *idp)
{
    obj = js::CheckedUnwrap(obj, false);
    if (!obj || !IsCPOW(obj)) {
        JS_ReportError(cx, "cannot ipc non-cpow object");
        return false;
    }

    *idp = idOf(obj);
    return true;
}

JSObject *
JavaScriptParent::fromId(JSContext *cx, ObjectId objId)
{
    RootedObject obj(cx, findCPOWById(objId));
    if (obj) {
        if (!JS_WrapObject(cx, &obj))
            return nullptr;
        return obj;
    }

    if (objId > MAX_CPOW_IDS) {
        JS_ReportError(cx, "unusable CPOW id");
        return nullptr;
    }

    bool callable = !!(objId & OBJECT_IS_CALLABLE);

    RootedObject global(cx, JS::CurrentGlobalOrNull(cx));

    RootedValue v(cx, UndefinedValue());
    ProxyOptions options;
    options.selectDefaultClass(callable);
    obj = NewProxyObject(cx,
                         ProxyHandler(),
                         v,
                         nullptr,
                         global,
                         options);
    if (!obj)
        return nullptr;

    if (!cpows_.add(objId, obj))
        return nullptr;

    // Incref once we know the decref will be called.
    incref();

    SetProxyExtra(obj, 0, PrivateValue(this));
    SetProxyExtra(obj, 1, DoubleValue(BitwiseCast<double>(objId)));
    return obj;
}

void
JavaScriptParent::decref()
{
    refcount_--;
    if (!refcount_)
        delete this;
}

void
JavaScriptParent::incref()
{
    refcount_++;
}

mozilla::ipc::IProtocol*
JavaScriptParent::CloneProtocol(Channel* aChannel, ProtocolCloneContext* aCtx)
{
    ContentParent *contentParent = aCtx->GetContentParent();
    nsAutoPtr<PJavaScriptParent> actor(contentParent->AllocPJavaScriptParent());
    if (!actor || !contentParent->RecvPJavaScriptConstructor(actor)) {
        return nullptr;
    }
    return actor.forget();
}
