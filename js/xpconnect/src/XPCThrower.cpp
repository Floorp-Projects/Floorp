/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Code for throwing errors into JavaScript. */

#include "xpcprivate.h"
#include "xpcpublic.h"
#include "XPCWrapper.h"

JSBool XPCThrower::sVerbose = true;

// static
void
XPCThrower::Throw(nsresult rv, JSContext* cx)
{
    const char* format;
    if (JS_IsExceptionPending(cx))
        return;
    if (!nsXPCException::NameAndFormatForNSResult(rv, nullptr, &format))
        format = "";
    BuildAndThrowException(cx, rv, format);
}

namespace xpc {

bool
Throw(JSContext *cx, nsresult rv)
{
    XPCThrower::Throw(rv, cx);
    return false;
}

} // namespace xpc

/*
 * If there has already been an exception thrown, see if we're throwing the
 * same sort of exception, and if we are, don't clobber the old one. ccx
 * should be the current call context.
 */
// static
JSBool
XPCThrower::CheckForPendingException(nsresult result, JSContext *cx)
{
    nsCOMPtr<nsIException> e;
    XPCJSRuntime::Get()->GetPendingException(getter_AddRefs(e));
    if (!e)
        return false;
    XPCJSRuntime::Get()->SetPendingException(nullptr);

    nsresult e_result;
    if (NS_FAILED(e->GetResult(&e_result)) || e_result != result)
        return false;

    if (!ThrowExceptionObject(cx, e))
        JS_ReportOutOfMemory(cx);
    return true;
}

// static
void
XPCThrower::Throw(nsresult rv, XPCCallContext& ccx)
{
    char* sz;
    const char* format;

    if (CheckForPendingException(rv, ccx))
        return;

    if (!nsXPCException::NameAndFormatForNSResult(rv, nullptr, &format))
        format = "";

    sz = (char*) format;

    if (sz && sVerbose)
        Verbosify(ccx, &sz, false);

    BuildAndThrowException(ccx, rv, sz);

    if (sz && sz != format)
        JS_smprintf_free(sz);
}


// static
void
XPCThrower::ThrowBadResult(nsresult rv, nsresult result, XPCCallContext& ccx)
{
    char* sz;
    const char* format;
    const char* name;

    /*
    *  If there is a pending exception when the native call returns and
    *  it has the same error result as returned by the native call, then
    *  the native call may be passing through an error from a previous JS
    *  call. So we'll just throw that exception into our JS.
    */

    if (CheckForPendingException(result, ccx))
        return;

    // else...

    if (!nsXPCException::NameAndFormatForNSResult(rv, nullptr, &format) || !format)
        format = "";

    if (nsXPCException::NameAndFormatForNSResult(result, &name, nullptr) && name)
        sz = JS_smprintf("%s 0x%x (%s)", format, result, name);
    else
        sz = JS_smprintf("%s 0x%x", format, result);

    if (sz && sVerbose)
        Verbosify(ccx, &sz, true);

    BuildAndThrowException(ccx, result, sz);

    if (sz)
        JS_smprintf_free(sz);
}

// static
void
XPCThrower::ThrowBadParam(nsresult rv, unsigned paramNum, XPCCallContext& ccx)
{
    char* sz;
    const char* format;

    if (!nsXPCException::NameAndFormatForNSResult(rv, nullptr, &format))
        format = "";

    sz = JS_smprintf("%s arg %d", format, paramNum);

    if (sz && sVerbose)
        Verbosify(ccx, &sz, true);

    BuildAndThrowException(ccx, rv, sz);

    if (sz)
        JS_smprintf_free(sz);
}


// static
void
XPCThrower::Verbosify(XPCCallContext& ccx,
                      char** psz, bool own)
{
    char* sz = nullptr;

    if (ccx.HasInterfaceAndMember()) {
        XPCNativeInterface* iface = ccx.GetInterface();
        jsid id = ccx.GetMember()->GetName();
        JSAutoByteString bytes;
        const char *name = JSID_IS_VOID(id) ? "Unknown" : bytes.encodeLatin1(ccx, JSID_TO_STRING(id));
        if (!name) {
            name = "";
        }
        sz = JS_smprintf("%s [%s.%s]", *psz, iface->GetNameString(), name);
    }

    if (sz) {
        if (own)
            JS_smprintf_free(*psz);
        *psz = sz;
    }
}

// static
void
XPCThrower::BuildAndThrowException(JSContext* cx, nsresult rv, const char* sz)
{
    JSBool success = false;

    /* no need to set an expection if the security manager already has */
    if (rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO && JS_IsExceptionPending(cx))
        return;
    nsCOMPtr<nsIException> finalException;
    nsCOMPtr<nsIException> defaultException;
    nsXPCException::NewException(sz, rv, nullptr, nullptr, getter_AddRefs(defaultException));

    nsIExceptionManager * exceptionManager = XPCJSRuntime::Get()->GetExceptionManager();
    if (exceptionManager) {
        // Ask the provider for the exception, if there is no provider
        // we expect it to set e to null
        exceptionManager->GetExceptionFromProvider(rv,
                                                   defaultException,
                                                   getter_AddRefs(finalException));
        // We should get at least the defaultException back,
        // but just in case
        if (finalException == nullptr) {
            finalException = defaultException;
        }
    }

    // XXX Should we put the following test and call to JS_ReportOutOfMemory
    // inside this test?
    if (finalException)
        success = ThrowExceptionObject(cx, finalException);
    // If we weren't able to build or throw an exception we're
    // most likely out of memory
    if (!success)
        JS_ReportOutOfMemory(cx);
}

static bool
IsCallerChrome(JSContext* cx)
{
    nsresult rv;

    nsCOMPtr<nsIScriptSecurityManager> secMan;
    secMan = XPCWrapper::GetSecurityManager();

    if (!secMan)
        return false;

    bool isChrome;
    rv = secMan->SubjectPrincipalIsSystem(&isChrome);
    return NS_SUCCEEDED(rv) && isChrome;
}

// static
JSBool
XPCThrower::ThrowExceptionObject(JSContext* cx, nsIException* e)
{
    JSBool success = false;
    if (e) {
        nsCOMPtr<nsIXPCException> xpcEx;
        JS::RootedValue thrown(cx);
        nsXPConnect* xpc;

        // If we stored the original thrown JS value in the exception
        // (see XPCConvert::ConstructException) and we are in a web
        // context (i.e., not chrome), rethrow the original value.
        if (!IsCallerChrome(cx) &&
            (xpcEx = do_QueryInterface(e)) &&
            NS_SUCCEEDED(xpcEx->StealJSVal(thrown.address()))) {
            if (!JS_WrapValue(cx, thrown.address()))
                return false;
            JS_SetPendingException(cx, thrown);
            success = true;
        } else if ((xpc = nsXPConnect::GetXPConnect())) {
            JS::RootedObject glob(cx, JS_GetGlobalForScopeChain(cx));
            if (!glob)
                return false;

            nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
            nsresult rv = xpc->WrapNative(cx, glob, e,
                                          NS_GET_IID(nsIException),
                                          getter_AddRefs(holder));
            if (NS_SUCCEEDED(rv) && holder) {
                JS::RootedObject obj(cx);
                if (NS_SUCCEEDED(holder->GetJSObject(obj.address()))) {
                    JS_SetPendingException(cx, OBJECT_TO_JSVAL(obj));
                    success = true;
                }
            }
        }
    }
    return success;
}
