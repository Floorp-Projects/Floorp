/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JavaScriptParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsJSUtils.h"
#include "jsfriendapi.h"
#include "jswrapper.h"
#include "js/Proxy.h"
#include "js/HeapAPI.h"
#include "xpcprivate.h"
#include "mozilla/Casting.h"
#include "mozilla/Telemetry.h"
#include "nsAutoPtr.h"

using namespace js;
using namespace JS;
using namespace mozilla;
using namespace mozilla::jsipc;
using namespace mozilla::dom;

static void
TraceParent(JSTracer* trc, void* data)
{
    static_cast<JavaScriptParent*>(data)->trace(trc);
}

JavaScriptParent::JavaScriptParent(JSRuntime* rt)
  : JavaScriptShared(rt),
    JavaScriptBase<PJavaScriptParent>(rt)
{
}

JavaScriptParent::~JavaScriptParent()
{
    JS_RemoveExtraGCRootsTracer(rt_, TraceParent, this);
}

bool
JavaScriptParent::init()
{
    if (!WrapperOwner::init())
        return false;

    JS_AddExtraGCRootsTracer(rt_, TraceParent, this);
    return true;
}

static bool
ForbidUnsafeBrowserCPOWs()
{
    static bool result;
    static bool cached = false;
    if (!cached) {
        cached = true;
        Preferences::AddBoolVarCache(&result, "dom.ipc.cpows.forbid-unsafe-from-browser", false);
    }
    return result;
}

bool
JavaScriptParent::allowMessage(JSContext* cx)
{
    MessageChannel* channel = GetIPCChannel();
    if (channel->IsInTransaction())
        return true;

    if (ForbidUnsafeBrowserCPOWs()) {
        nsIGlobalObject* global = dom::GetIncumbentGlobal();
        JSObject* jsGlobal = global ? global->GetGlobalJSObject() : nullptr;
        if (jsGlobal) {
            JSAutoCompartment ac(cx, jsGlobal);
            if (!JS::AddonIdOfObject(jsGlobal) && !xpc::CompartmentPrivate::Get(jsGlobal)->allowCPOWs) {
                Telemetry::Accumulate(Telemetry::BROWSER_SHIM_USAGE_BLOCKED, 1);
                JS_ReportError(cx, "unsafe CPOW usage forbidden");
                return false;
            }
        }
    }

    static bool disableUnsafeCPOWWarnings = PR_GetEnv("DISABLE_UNSAFE_CPOW_WARNINGS");
    if (!disableUnsafeCPOWWarnings) {
        nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
        if (console && cx) {
            nsAutoString filename;
            uint32_t lineno = 0, column = 0;
            nsJSUtils::GetCallingLocation(cx, filename, &lineno, &column);
            nsCOMPtr<nsIScriptError> error(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
            error->Init(NS_LITERAL_STRING("unsafe CPOW usage"), filename,
                        EmptyString(), lineno, column,
                        nsIScriptError::warningFlag, "chrome javascript");
            console->LogMessage(error);
        } else {
            NS_WARNING("Unsafe synchronous IPC message");
        }
    }

    return true;
}

void
JavaScriptParent::trace(JSTracer* trc)
{
    objects_.trace(trc);
    unwaivedObjectIds_.trace(trc);
    waivedObjectIds_.trace(trc);
}

JSObject*
JavaScriptParent::scopeForTargetObjects()
{
    // CPWOWs from the child need to point into the parent's unprivileged junk
    // scope so that a compromised child cannot compromise the parent. In
    // practice, this means that a child process can only (a) hold parent
    // objects alive and (b) invoke them if they are callable.
    return xpc::UnprivilegedJunkScope();
}

mozilla::ipc::IProtocol*
JavaScriptParent::CloneProtocol(Channel* aChannel, ProtocolCloneContext* aCtx)
{
    ContentParent* contentParent = aCtx->GetContentParent();
    nsAutoPtr<PJavaScriptParent> actor(contentParent->AllocPJavaScriptParent());
    if (!actor || !contentParent->RecvPJavaScriptConstructor(actor)) {
        return nullptr;
    }
    return actor.forget();
}

PJavaScriptParent*
mozilla::jsipc::NewJavaScriptParent(JSRuntime* rt)
{
    JavaScriptParent* parent = new JavaScriptParent(rt);
    if (!parent->init()) {
        delete parent;
        return nullptr;
    }
    return parent;
}

void
mozilla::jsipc::ReleaseJavaScriptParent(PJavaScriptParent* parent)
{
    static_cast<JavaScriptParent*>(parent)->decref();
}
