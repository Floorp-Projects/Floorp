/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */
#include "nsCodebasePrincipal.h"
#include "nsJSPrincipals.h"
#include "plstr.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "jsapi.h"
#include "jsxdrapi.h"
#include "nsIJSRuntimeService.h"
#include "nsIServiceManager.h"
#include "nsMemory.h"

JS_STATIC_DLL_CALLBACK(void *)
nsGetPrincipalArray(JSContext *cx, struct JSPrincipals *prin)
{
    return nsnull;
}

JS_STATIC_DLL_CALLBACK(JSBool)
nsGlobalPrivilegesEnabled(JSContext *cx , struct JSPrincipals *jsprin)
{
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(void)
nsDestroyJSPrincipals(JSContext *cx, struct JSPrincipals *jsprin)
{
    nsJSPrincipals *nsjsprin = NS_STATIC_CAST(nsJSPrincipals *, jsprin);

    // We need to destroy the nsIPrincipal. We'll do this by adding
    // to the refcount and calling release

    // Note that we don't want to use NS_IF_RELEASE because it will try
    // to set nsjsprin->nsIPrincipalPtr to nsnull *after* nsjsprin has
    // already been destroyed.
#ifdef NS_BUILD_REFCNT_LOGGING
    // The refcount logging considers AddRef-to-1 to indicate creation,
    // so trick it into thinking it's otherwise, but balance the
    // Release() we do below.
    nsjsprin->refcount++;
    nsjsprin->nsIPrincipalPtr->AddRef();
    nsjsprin->refcount--;
#else
    nsjsprin->refcount++;
#endif
    if (nsjsprin->nsIPrincipalPtr)
        nsjsprin->nsIPrincipalPtr->Release();
    // The nsIPrincipal that we release owns the JSPrincipal struct,
    // so we don't need to worry about "codebase"
}

JS_STATIC_DLL_CALLBACK(JSBool)
nsTranscodeJSPrincipals(JSXDRState *xdr, JSPrincipals **jsprinp)
{
    nsresult rv;

    if (xdr->mode == JSXDR_ENCODE) {
        nsIObjectOutputStream *stream =
            NS_REINTERPRET_CAST(nsIObjectOutputStream*, xdr->userdata);

        // Flush xdr'ed data to the underlying object output stream.
        uint32 size;
        char *data = (char*) ::JS_XDRMemGetData(xdr, &size);

        rv = stream->Write32(size);
        if (NS_SUCCEEDED(rv)) {
            rv = stream->WriteBytes(data, size);
            if (NS_SUCCEEDED(rv)) {
                ::JS_XDRMemResetData(xdr);

                // Require that GetJSPrincipals has been called already by the
                // code that compiled the script that owns the principals.
                nsJSPrincipals *nsjsprin =
                    NS_STATIC_CAST(nsJSPrincipals*, *jsprinp);

                rv = stream->WriteObject(nsjsprin->nsIPrincipalPtr, PR_TRUE);
            }
        }
    } else {
        NS_ASSERTION(JS_XDRMemDataLeft(xdr) == 0, "XDR out of sync?!");
        nsIObjectInputStream *stream =
            NS_REINTERPRET_CAST(nsIObjectInputStream*, xdr->userdata);

        nsCOMPtr<nsIPrincipal> prin;
        rv = stream->ReadObject(PR_TRUE, getter_AddRefs(prin));
        if (NS_SUCCEEDED(rv)) {
            PRUint32 size;
            rv = stream->Read32(&size);
            if (NS_SUCCEEDED(rv)) {
                char *data = nsnull;
                if (size != 0)
                    rv = stream->ReadBytes(&data, size);
                if (NS_SUCCEEDED(rv)) {
                    char *olddata;
                    uint32 oldsize;

                    // Any decode-mode JSXDRState whose userdata points to an
                    // nsIObjectInputStream instance must use nsMemory to Alloc
                    // and Free its data buffer.  Swap the new buffer we just
                    // read for the old, exhausted data.
                    olddata = (char*) ::JS_XDRMemGetData(xdr, &oldsize);
                    nsMemory::Free(olddata);
                    ::JS_XDRMemSetData(xdr, data, size);

                    prin->GetJSPrincipals(jsprinp);
                }
            }
        }
    }

    if (NS_FAILED(rv)) {
        ::JS_ReportError(xdr->cx, "can't %scode principals (failure code %x)",
                         (xdr->mode == JSXDR_ENCODE) ? "en" : "de",
                         (unsigned int) rv);
        return JS_FALSE;
    }
    return JS_TRUE;
}

nsresult
nsJSPrincipals::Startup()
{
    static const char rtsvc_id[] = "@mozilla.org/js/xpc/RuntimeService;1";
    nsCOMPtr<nsIJSRuntimeService> rtsvc(do_GetService(rtsvc_id));
    if (!rtsvc)
        return NS_ERROR_FAILURE;

    JSRuntime *rt;
    rtsvc->GetRuntime(&rt);
    NS_ASSERTION(rt != nsnull, "no JSRuntime?!");

    JSPrincipalsTranscoder oldpx;
    oldpx = ::JS_SetPrincipalsTranscoder(rt, nsTranscodeJSPrincipals);
    NS_ASSERTION(oldpx == nsnull, "oops, JS_SetPrincipalsTranscoder wars!");

    return NS_OK;
}

nsJSPrincipals::nsJSPrincipals()
{
    codebase = nsnull;
    getPrincipalArray = nsGetPrincipalArray;
    globalPrivilegesEnabled = nsGlobalPrivilegesEnabled;
    refcount = 0;
    destroy = nsDestroyJSPrincipals;
    nsIPrincipalPtr = nsnull;
}

nsresult
nsJSPrincipals::Init(char *aCodebase)
{
    codebase = aCodebase;
    return NS_OK;
}

nsJSPrincipals::~nsJSPrincipals()
{
    if (codebase)
        PL_strfree(codebase);
}
