/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implement global service to track stack of JSContext. */

#include "xpcprivate.h"
#include "XPCWrapper.h"
#include "mozilla/Mutex.h"
#include "nsDOMJSUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsNullPrincipal.h"
#include "mozilla/dom/BindingUtils.h"

using namespace mozilla;
using namespace JS;
using namespace xpc;
using mozilla::dom::DestroyProtoAndIfaceCache;

/***************************************************************************/

XPCJSContextStack::~XPCJSContextStack()
{
    if (mOwnSafeJSContext) {
        JS_DestroyContext(mOwnSafeJSContext);
        mOwnSafeJSContext = nullptr;
    }
}

JSContext*
XPCJSContextStack::Pop()
{
    MOZ_ASSERT(!mStack.IsEmpty());

    uint32_t idx = mStack.Length() - 1; // The thing we're popping

    JSContext *cx = mStack[idx].cx;

    mStack.RemoveElementAt(idx);
    if (idx == 0)
        return cx;

    --idx; // Advance to new top of the stack

    XPCJSContextInfo &e = mStack[idx];
    if (e.cx && e.savedFrameChain) {
        // Pop() can be called outside any request for e.cx.
        JSAutoRequest ar(e.cx);
        JS_RestoreFrameChain(e.cx);
        e.savedFrameChain = false;
    }
    return cx;
}

bool
XPCJSContextStack::Push(JSContext *cx)
{
    if (mStack.Length() == 0) {
        mStack.AppendElement(cx);
        return true;
    }

    XPCJSContextInfo &e = mStack[mStack.Length() - 1];
    if (e.cx) {
        // The cx we're pushing is also stack-top. In general we still need to
        // call JS_SaveFrameChain here. But if that would put us in a
        // compartment that's same-origin with the current one, we can skip it.
        nsIScriptSecurityManager* ssm = XPCWrapper::GetSecurityManager();
        if ((e.cx == cx) && ssm) {
            RootedObject defaultGlobal(cx, js::GetDefaultGlobalForContext(cx));
            nsIPrincipal *currentPrincipal =
              GetCompartmentPrincipal(js::GetContextCompartment(cx));
            nsIPrincipal *defaultPrincipal = GetObjectPrincipal(defaultGlobal);
            bool equal = false;
            currentPrincipal->Equals(defaultPrincipal, &equal);
            if (equal) {
                mStack.AppendElement(cx);
                return true;
            }
        }

        {
            // Push() can be called outside any request for e.cx.
            JSAutoRequest ar(e.cx);
            if (!JS_SaveFrameChain(e.cx))
                return false;
            e.savedFrameChain = true;
        }
    }

    mStack.AppendElement(cx);
    return true;
}

bool
XPCJSContextStack::HasJSContext(JSContext *cx)
{
    for (uint32_t i = 0; i < mStack.Length(); i++)
        if (cx == mStack[i].cx)
            return true;
    return false;
}

static JSBool
SafeGlobalResolve(JSContext *cx, JSHandleObject obj, JSHandleId id)
{
    JSBool resolved;
    return JS_ResolveStandardClass(cx, obj, id, &resolved);
}

static void
SafeFinalize(JSFreeOp *fop, JSObject* obj)
{
    SandboxPrivate* sop =
        static_cast<SandboxPrivate*>(xpc_GetJSPrivate(obj));
    sop->ForgetGlobalObject();
    NS_IF_RELEASE(sop);
    DestroyProtoAndIfaceCache(obj);
}

static JSClass global_class = {
    "global_for_XPCJSContextStack_SafeJSContext",
    XPCONNECT_GLOBAL_FLAGS,
    JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, SafeGlobalResolve, JS_ConvertStub, SafeFinalize,
    NULL, NULL, NULL, NULL, TraceXPCGlobal
};

// We just use the same reporter as the component loader
// XXX #include angels cry.
extern void
mozJSLoaderErrorReporter(JSContext *cx, const char *message, JSErrorReport *rep);

JSContext*
XPCJSContextStack::GetSafeJSContext()
{
    if (mSafeJSContext)
        return mSafeJSContext;

    // Start by getting the principal holder and principal for this
    // context.  If we can't manage that, don't bother with the rest.
    nsRefPtr<nsNullPrincipal> principal = new nsNullPrincipal();
    nsresult rv = principal->Init();
    if (NS_FAILED(rv))
        return NULL;

    nsXPConnect* xpc = nsXPConnect::XPConnect();
    XPCJSRuntime* xpcrt = xpc->GetRuntime();
    if (!xpcrt)
        return NULL;

    JSRuntime *rt = xpcrt->Runtime();
    if (!rt)
        return NULL;

    mSafeJSContext = JS_NewContext(rt, 8192);
    if (!mSafeJSContext)
        return NULL;

    JS::RootedObject glob(mSafeJSContext);
    {
        // scoped JS Request
        JSAutoRequest req(mSafeJSContext);

        JS_SetErrorReporter(mSafeJSContext, mozJSLoaderErrorReporter);

        glob = xpc::CreateGlobalObject(mSafeJSContext, &global_class, principal, JS::SystemZone);

        if (glob) {
            // Make sure the context is associated with a proper compartment
            // and not the default compartment.
            JS_SetGlobalObject(mSafeJSContext, glob);

            // Note: make sure to set the private before calling
            // InitClasses
            nsCOMPtr<nsIScriptObjectPrincipal> sop = new SandboxPrivate(principal, glob);
            JS_SetPrivate(glob, sop.forget().get());
        }

        // After this point either glob is null and the
        // nsIScriptObjectPrincipal ownership is either handled by the
        // nsCOMPtr or dealt with, or we'll release in the finalize
        // hook.
        if (glob && NS_FAILED(xpc->InitClasses(mSafeJSContext, glob))) {
            glob = nullptr;
        }
    }
    if (mSafeJSContext && !glob) {
        // Destroy the context outside the scope of JSAutoRequest that
        // uses the context in its destructor.
        JS_DestroyContext(mSafeJSContext);
        mSafeJSContext = nullptr;
    }

    // Save it off so we can destroy it later.
    mOwnSafeJSContext = mSafeJSContext;

    return mSafeJSContext;
}
