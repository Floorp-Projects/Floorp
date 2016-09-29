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

JavaScriptParent::~JavaScriptParent()
{
    JS_RemoveExtraGCRootsTracer(danger::GetJSContext(), TraceParent, this);
}

bool
JavaScriptParent::init()
{
    if (!WrapperOwner::init())
        return false;

    JS_AddExtraGCRootsTracer(danger::GetJSContext(), TraceParent, this);
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

// Should we allow CPOWs in aAddonId, even though it's marked as multiprocess
// compatible? This is controlled by two prefs:
//   If dom.ipc.cpows.forbid-cpows-in-compat-addons is false, then we allow the CPOW.
//   If dom.ipc.cpows.forbid-cpows-in-compat-addons is true:
//     We check if aAddonId is listed in dom.ipc.cpows.allow-cpows-in-compat-addons
//     (which should be a comma-separated string). If it's present there, we allow
//     the CPOW. Otherwise we forbid the CPOW.
static bool
ForbidCPOWsInCompatibleAddon(const nsACString& aAddonId)
{
    bool forbid = Preferences::GetBool("dom.ipc.cpows.forbid-cpows-in-compat-addons", false);
    if (!forbid) {
        return false;
    }

    nsCString allow;
    allow.Assign(',');
    allow.Append(Preferences::GetCString("dom.ipc.cpows.allow-cpows-in-compat-addons"));
    allow.Append(',');

    nsCString searchString(",");
    searchString.Append(aAddonId);
    searchString.Append(',');
    return allow.Find(searchString) == kNotFound;
}

bool
JavaScriptParent::allowMessage(JSContext* cx)
{
    // If we're running browser code, then we allow all safe CPOWs and forbid
    // unsafe CPOWs based on a pref (which defaults to forbidden). We also allow
    // CPOWs unconditionally in selected globals (based on
    // Cu.permitCPOWsInScope).
    //
    // If we're running add-on code, then we check if the add-on is multiprocess
    // compatible (which eventually translates to a given setting of allowCPOWs
    // on the scopw). If it's not compatible, then we allow the CPOW but
    // warn. If it is marked as compatible, then we check the
    // ForbidCPOWsInCompatibleAddon; see the comment there.

    MessageChannel* channel = GetIPCChannel();
    bool isSafe = channel->IsInTransaction();

    bool warn = !isSafe;
    nsIGlobalObject* global = dom::GetIncumbentGlobal();
    JSObject* jsGlobal = global ? global->GetGlobalJSObject() : nullptr;
    if (jsGlobal) {
        JSAutoCompartment ac(cx, jsGlobal);
        JSAddonId* addonId = JS::AddonIdOfObject(jsGlobal);

        if (!xpc::CompartmentPrivate::Get(jsGlobal)->allowCPOWs) {
            if (!addonId && ForbidUnsafeBrowserCPOWs() && !isSafe) {
                Telemetry::Accumulate(Telemetry::BROWSER_SHIM_USAGE_BLOCKED, 1);
                JS_ReportErrorASCII(cx, "unsafe CPOW usage forbidden");
                return false;
            }

            if (addonId) {
                JSFlatString* flat = JS_ASSERT_STRING_IS_FLAT(JS::StringOfAddonId(addonId));
                nsString addonIdString;
                AssignJSFlatString(addonIdString, flat);
                NS_ConvertUTF16toUTF8 addonIdCString(addonIdString);
                Telemetry::Accumulate(Telemetry::ADDON_FORBIDDEN_CPOW_USAGE, addonIdCString);

                if (ForbidCPOWsInCompatibleAddon(addonIdCString)) {
                    JS_ReportErrorASCII(cx, "CPOW usage forbidden in this add-on");
                    return false;
                }

                warn = true;
            }
        }
    }

    if (!warn)
        return true;

    static bool disableUnsafeCPOWWarnings = PR_GetEnv("DISABLE_UNSAFE_CPOW_WARNINGS");
    if (!disableUnsafeCPOWWarnings) {
        nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
        if (console && cx) {
            nsAutoString filename;
            uint32_t lineno = 0, column = 0;
            nsJSUtils::GetCallingLocation(cx, filename, &lineno, &column);
            nsCOMPtr<nsIScriptError> error(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
            error->Init(NS_LITERAL_STRING("unsafe/forbidden CPOW usage"), filename,
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
mozilla::jsipc::NewJavaScriptParent()
{
    JavaScriptParent* parent = new JavaScriptParent();
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
