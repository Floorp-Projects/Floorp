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

static void
TraceParent(JSTracer *trc, void *data)
{
    static_cast<JavaScriptParent *>(data)->trace(trc);
}

static void
FixupParentAfterMovingGC(JSRuntime *rt, void *data)
{
    static_cast<JavaScriptParent *>(data)->fixupAfterMovingGC();
}

JavaScriptParent::JavaScriptParent(JSRuntime *rt)
  : JavaScriptShared(rt),
    JavaScriptBase<PJavaScriptParent>(rt)
{
}

JavaScriptParent::~JavaScriptParent()
{
    JS_RemoveExtraGCRootsTracer(rt_, TraceParent, this);
    JS_RemoveMovingGCCallback(rt_, FixupParentAfterMovingGC);
}

bool
JavaScriptParent::init()
{
    if (!WrapperOwner::init())
        return false;

    JS_AddExtraGCRootsTracer(rt_, TraceParent, this);
    JS_AddMovingGCCallback(rt_, FixupParentAfterMovingGC, this);
    return true;
}

void
JavaScriptParent::trace(JSTracer *trc)
{
    if (active())
        objects_.trace(trc);
}

JSObject *
JavaScriptParent::defaultScope()
{
    return xpc::UnprivilegedJunkScope();
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
