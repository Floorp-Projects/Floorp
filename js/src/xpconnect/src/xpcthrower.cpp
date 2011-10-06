/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
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

/* Code for throwing errors into JavaScript. */

#include "xpcprivate.h"
#include "XPCWrapper.h"

JSBool XPCThrower::sVerbose = JS_TRUE;

// static
void
XPCThrower::Throw(nsresult rv, JSContext* cx)
{
    const char* format;
    if(JS_IsExceptionPending(cx))
        return;
    if(!nsXPCException::NameAndFormatForNSResult(rv, nsnull, &format))
        format = "";
    BuildAndThrowException(cx, rv, format);
}

/*
 * If there has already been an exception thrown, see if we're throwing the
 * same sort of exception, and if we are, don't clobber the old one. ccx
 * should be the current call context.
 */
// static
JSBool
XPCThrower::CheckForPendingException(nsresult result, JSContext *cx)
{
    nsXPConnect* xpc = nsXPConnect::GetXPConnect();
    if(!xpc)
        return JS_FALSE;

    nsCOMPtr<nsIException> e;
    xpc->GetPendingException(getter_AddRefs(e));
    if(!e)
        return JS_FALSE;
    xpc->SetPendingException(nsnull);

    nsresult e_result;
    if(NS_FAILED(e->GetResult(&e_result)) || e_result != result)
        return JS_FALSE;

    if(!ThrowExceptionObject(cx, e))
        JS_ReportOutOfMemory(cx);
    return JS_TRUE;
}

// static
void
XPCThrower::Throw(nsresult rv, XPCCallContext& ccx)
{
    char* sz;
    const char* format;

    if(CheckForPendingException(rv, ccx))
        return;

    if(!nsXPCException::NameAndFormatForNSResult(rv, nsnull, &format))
        format = "";

    sz = (char*) format;

    if(sz && sVerbose)
        Verbosify(ccx, &sz, PR_FALSE);

    BuildAndThrowException(ccx, rv, sz);

    if(sz && sz != format)
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

    if(CheckForPendingException(result, ccx))
        return;

    // else...

    if(!nsXPCException::NameAndFormatForNSResult(rv, nsnull, &format) || !format)
        format = "";

    if(nsXPCException::NameAndFormatForNSResult(result, &name, nsnull) && name)
        sz = JS_smprintf("%s 0x%x (%s)", format, result, name);
    else
        sz = JS_smprintf("%s 0x%x", format, result);

    if(sz && sVerbose)
        Verbosify(ccx, &sz, PR_TRUE);

    BuildAndThrowException(ccx, result, sz);

    if(sz)
        JS_smprintf_free(sz);
}

// static
void
XPCThrower::ThrowBadParam(nsresult rv, uintN paramNum, XPCCallContext& ccx)
{
    char* sz;
    const char* format;

    if(!nsXPCException::NameAndFormatForNSResult(rv, nsnull, &format))
        format = "";

    sz = JS_smprintf("%s arg %d", format, paramNum);

    if(sz && sVerbose)
        Verbosify(ccx, &sz, PR_TRUE);

    BuildAndThrowException(ccx, rv, sz);

    if(sz)
        JS_smprintf_free(sz);
}


// static
void
XPCThrower::Verbosify(XPCCallContext& ccx,
                      char** psz, bool own)
{
    char* sz = nsnull;

    if(ccx.HasInterfaceAndMember())
    {
        XPCNativeInterface* iface = ccx.GetInterface();
        jsid id = ccx.GetMember()->GetName();
        JSAutoByteString bytes;
        const char *name = JSID_IS_VOID(id) ? "Unknown" : bytes.encode(ccx, JSID_TO_STRING(id));
        if(!name)
        {
            name = "";
        }
        sz = JS_smprintf("%s [%s.%s]", *psz, iface->GetNameString(), name);
    }

    if(sz)
    {
        if(own)
            JS_smprintf_free(*psz);
        *psz = sz;
    }
}

// static
void
XPCThrower::BuildAndThrowException(JSContext* cx, nsresult rv, const char* sz)
{
    JSBool success = JS_FALSE;

    /* no need to set an expection if the security manager already has */
    if(rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO && JS_IsExceptionPending(cx))
        return;
    nsCOMPtr<nsIException> finalException;
    nsCOMPtr<nsIException> defaultException;
    nsXPCException::NewException(sz, rv, nsnull, nsnull, getter_AddRefs(defaultException));
    XPCPerThreadData* tls = XPCPerThreadData::GetData(cx);
    if(tls)
    {
        nsIExceptionManager * exceptionManager = tls->GetExceptionManager();
        if(exceptionManager)
        {
           // Ask the provider for the exception, if there is no provider
           // we expect it to set e to null
            exceptionManager->GetExceptionFromProvider(
               rv,
               defaultException,
               getter_AddRefs(finalException));
            // We should get at least the defaultException back, 
            // but just in case
            if(finalException == nsnull)
            {
                finalException = defaultException;
            }
        }
    }
    // XXX Should we put the following test and call to JS_ReportOutOfMemory
    // inside this test?
    if(finalException)
        success = ThrowExceptionObject(cx, finalException);
    // If we weren't able to build or throw an exception we're
    // most likely out of memory 
    if(!success)
        JS_ReportOutOfMemory(cx);
}

static bool
IsCallerChrome(JSContext* cx)
{
    nsresult rv;

    nsCOMPtr<nsIScriptSecurityManager> secMan;
    if(XPCPerThreadData::IsMainThread(cx))
    {
        secMan = XPCWrapper::GetSecurityManager();
    }
    else
    {
        nsXPConnect* xpc = nsXPConnect::GetXPConnect();
        if(!xpc)
            return PR_FALSE;

        nsCOMPtr<nsIXPCSecurityManager> xpcSecMan;
        PRUint16 flags = 0;
        rv = xpc->GetSecurityManagerForJSContext(cx, getter_AddRefs(xpcSecMan),
                                                 &flags);
        if(NS_FAILED(rv) || !xpcSecMan)
            return PR_FALSE;

        secMan = do_QueryInterface(xpcSecMan);
    }

    if(!secMan)
        return PR_FALSE;

    bool isChrome;
    rv = secMan->SubjectPrincipalIsSystem(&isChrome);
    return NS_SUCCEEDED(rv) && isChrome;
}

// static
JSBool
XPCThrower::ThrowExceptionObject(JSContext* cx, nsIException* e)
{
    JSBool success = JS_FALSE;
    if(e)
    {
        nsCOMPtr<nsIXPCException> xpcEx;
        jsval thrown;
        nsXPConnect* xpc;

        // If we stored the original thrown JS value in the exception
        // (see XPCConvert::ConstructException) and we are in a web
        // context (i.e., not chrome), rethrow the original value.
        if(!IsCallerChrome(cx) &&
           (xpcEx = do_QueryInterface(e)) &&
           NS_SUCCEEDED(xpcEx->StealJSVal(&thrown)))
        {
            if (!JS_WrapValue(cx, &thrown))
                return JS_FALSE;
            JS_SetPendingException(cx, thrown);
            success = JS_TRUE;
        }
        else if((xpc = nsXPConnect::GetXPConnect()))
        {
            JSObject* glob = JS_GetScopeChain(cx);
            if(!glob)
                return JS_FALSE;
            glob = JS_GetGlobalForObject(cx, glob);

            nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
            nsresult rv = xpc->WrapNative(cx, glob, e,
                                          NS_GET_IID(nsIException),
                                          getter_AddRefs(holder));
            if(NS_SUCCEEDED(rv) && holder)
            {
                JSObject* obj;
                if(NS_SUCCEEDED(holder->GetJSObject(&obj)))
                {
                    JS_SetPendingException(cx, OBJECT_TO_JSVAL(obj));
                    success = JS_TRUE;
                }
            }
        }
    }
    return success;
}
