/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* Code for throwing errors into JavaScript. */

#include "xpcprivate.h"

XPCJSThrower::XPCJSThrower(JSBool Verbose /*= JS_FALSE*/)
    : mVerbose(Verbose) {}

XPCJSThrower::~XPCJSThrower() {}

#if 0
char*
XPCJSThrower::BuildCallerString(JSContext* cx)
{
    JSStackFrame* iter = nsnull;
    JSStackFrame* fp;

    while(nsnull != (fp = JS_FrameIterator(cx, &iter)) )
    {
        JSScript* script = JS_GetFrameScript(cx, fp);
        jsbytecode* pc = JS_GetFramePC(cx, fp);

        if(!script || !pc || JS_IsNativeFrame(cx, fp))
            continue;

        const char* filename = JS_GetScriptFilename(cx, script);
        return JS_smprintf("{file: %s, line: %d}",
                           filename ? filename : "<unknown>",
                           JS_PCToLineNumber(cx, script, pc));
    }
    return nsnull;
}
#endif

void
XPCJSThrower::Verbosify(JSContext* cx,
                        nsXPCWrappedNativeClass* clazz,
                        const XPCNativeMemberDescriptor* desc,
                        char** psz, PRBool own)
{
    char* sz = nsnull;

    if(clazz && desc)
        sz = JS_smprintf("%s [%s.%s]",
                         *psz,
                         clazz->GetInterfaceName(),
                         clazz->GetMemberName(desc));
    if(sz)
    {
        if(own)
            JS_smprintf_free(*psz);
        *psz = sz;
    }
}

void
XPCJSThrower::ThrowBadResultException(nsresult rv,
                                      JSContext* cx,
                                      nsXPCWrappedNativeClass* clazz,
                                      const XPCNativeMemberDescriptor* desc,
                                      nsresult result)
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

    nsIXPCException* e;
    nsXPConnect* xpc = nsXPConnect::GetXPConnect();
    JSBool success = JS_FALSE;
    if(xpc)
    {
        if(NS_SUCCEEDED(xpc->GetPendingException(&e)) && e)
        {
            xpc->SetPendingException(nsnull);
            nsresult e_result;
        
            if(NS_SUCCEEDED(e->GetResult(&e_result)) && e_result == result)
            {
                if(!ThrowExceptionObject(cx, e))
                    JS_ReportOutOfMemory(cx);
                success = JS_TRUE;
            }
            NS_RELEASE(e);
        }
    }
    NS_IF_RELEASE(xpc);
    if(success)
        return;

    // else...

    if(!nsXPCException::NameAndFormatForNSResult(rv, nsnull, &format) || !format)
        format = "";

    if(nsXPCException::NameAndFormatForNSResult(result, &name, nsnull) && name)
        sz = JS_smprintf("%s 0x%x (%s)", format, result, name);
    else
        sz = JS_smprintf("%s 0x%x", format, result);

    if(sz && mVerbose)
        Verbosify(cx, clazz, desc, &sz, PR_TRUE);

    BuildAndThrowException(cx, result, sz);

    if(sz)
        JS_smprintf_free(sz);
}

void
XPCJSThrower::ThrowBadParamException(nsresult rv,
                            JSContext* cx,
                            nsXPCWrappedNativeClass* clazz,
                            const XPCNativeMemberDescriptor* desc,
                            uintN paramNum)
{
    char* sz;
    const char* format;

    if(!nsXPCException::NameAndFormatForNSResult(rv, nsnull, &format))
        format = "";

    sz = JS_smprintf("%s arg %d", format, paramNum);

    if(sz && mVerbose)
        Verbosify(cx, clazz, desc, &sz, PR_TRUE);

    BuildAndThrowException(cx, rv, sz);

    if(sz)
        JS_smprintf_free(sz);
}

void
XPCJSThrower::ThrowException(nsresult rv,
                    JSContext* cx,
                    nsXPCWrappedNativeClass* clazz /* = nsnull */,
                    const XPCNativeMemberDescriptor* desc /* = nsnull */)
{
    char* sz;
    const char* format;

    if(!nsXPCException::NameAndFormatForNSResult(rv, nsnull, &format))
        format = "";

    sz = (char*) format;

    if(sz && mVerbose)
        Verbosify(cx, clazz, desc, &sz, PR_FALSE);

    BuildAndThrowException(cx, rv, sz);

    if(sz && sz != format)
        JS_smprintf_free(sz);
}

void
XPCJSThrower::BuildAndThrowException(JSContext* cx, nsresult rv, const char* sz)
{
    JSBool success = JS_FALSE;

    /* no need to set an expection if the security manager already has */
    if(rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO && JS_IsExceptionPending(cx))
        return;

    nsIXPCException* e = nsXPCException::NewException(sz, rv, nsnull, nsnull);

    if(e)
    {
        success = ThrowExceptionObject(cx, e);
        NS_RELEASE(e);
    }
    if(!success)
        JS_ReportOutOfMemory(cx);
}

JSBool
XPCJSThrower::ThrowExceptionObject(JSContext* cx, nsIXPCException* e)
{
    JSBool success = JS_FALSE;
    if(e)
    {
        nsCOMPtr<nsXPConnect> xpc = dont_AddRef(nsXPConnect::GetXPConnect());
        if(xpc)
        {
            // XXX funky
            JSObject* glob = JS_GetGlobalObject(cx);
    
            nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
            nsresult rv = xpc->WrapNative(cx, glob, e, 
                                          NS_GET_IID(nsIXPCException), 
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

