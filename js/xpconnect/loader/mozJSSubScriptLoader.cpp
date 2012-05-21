/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozJSSubScriptLoader.h"
#include "mozJSLoaderUtils.h"

#include "nsIServiceManager.h"
#include "nsIXPConnect.h"

#include "nsIURI.h"
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsNetCID.h"
#include "nsDependentString.h"
#include "nsAutoPtr.h"
#include "nsNetUtil.h"
#include "nsIProtocolHandler.h"
#include "nsIFileURL.h"
#include "nsScriptLoader.h"

#include "jsapi.h"
#include "jsdbgapi.h"
#include "jsfriendapi.h"
#include "nsJSPrincipals.h"

#include "mozilla/FunctionTimer.h"
#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheUtils.h"

using namespace mozilla::scache;

/* load() error msgs, XXX localize? */
#define LOAD_ERROR_NOSERVICE "Error creating IO Service."
#define LOAD_ERROR_NOURI "Error creating URI (invalid URL scheme?)"
#define LOAD_ERROR_NOSCHEME "Failed to get URI scheme.  This is bad."
#define LOAD_ERROR_URI_NOT_LOCAL "Trying to load a non-local URI."
#define LOAD_ERROR_NOSTREAM  "Error opening input stream (invalid filename?)"
#define LOAD_ERROR_NOCONTENT "ContentLength not available (not a local URL?)"
#define LOAD_ERROR_BADCHARSET "Error converting to specified charset"
#define LOAD_ERROR_BADREAD   "File Read Error."
#define LOAD_ERROR_READUNDERFLOW "File Read Error (underflow.)"
#define LOAD_ERROR_NOPRINCIPALS "Failed to get principals."
#define LOAD_ERROR_NOSPEC "Failed to get URI spec.  This is bad."

// We just use the same reporter as the component loader
extern void
mozJSLoaderErrorReporter(JSContext *cx, const char *message, JSErrorReport *rep);

mozJSSubScriptLoader::mozJSSubScriptLoader() : mSystemPrincipal(nsnull)
{
}

mozJSSubScriptLoader::~mozJSSubScriptLoader()
{
    /* empty */
}

NS_IMPL_THREADSAFE_ISUPPORTS1(mozJSSubScriptLoader, mozIJSSubScriptLoader)

static nsresult
ReportError(JSContext *cx, const char *msg)
{
    JS_SetPendingException(cx, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, msg)));
    return NS_OK;
}

nsresult
mozJSSubScriptLoader::ReadScript(nsIURI *uri, JSContext *cx, JSObject *target_obj,
                                 const nsAString& charset, const char *uriStr,
                                 nsIIOService *serv, nsIPrincipal *principal,
                                 JSScript **scriptp)
{
    nsCOMPtr<nsIChannel>     chan;
    nsCOMPtr<nsIInputStream> instream;
    JSErrorReporter  er;

    nsresult rv;
    // Instead of calling NS_OpenURI, we create the channel ourselves and call
    // SetContentType, to avoid expensive MIME type lookups (bug 632490).
    rv = NS_NewChannel(getter_AddRefs(chan), uri, serv,
                       nsnull, nsnull, nsIRequest::LOAD_NORMAL);
    if (NS_SUCCEEDED(rv)) {
        chan->SetContentType(NS_LITERAL_CSTRING("application/javascript"));
        rv = chan->Open(getter_AddRefs(instream));
    }

    if (NS_FAILED(rv)) {
        return ReportError(cx, LOAD_ERROR_NOSTREAM);
    }

    PRInt32 len = -1;

    rv = chan->GetContentLength(&len);
    if (NS_FAILED(rv) || len == -1) {
        return ReportError(cx, LOAD_ERROR_NOCONTENT);
    }

    nsCString buf;
    rv = NS_ReadInputStreamToString(instream, buf, len);
    if (NS_FAILED(rv))
        return rv;

    /* set our own error reporter so we can report any bad things as catchable
     * exceptions, including the source/line number */
    er = JS_SetErrorReporter(cx, mozJSLoaderErrorReporter);

    if (!charset.IsVoid()) {
        nsString script;
        rv = nsScriptLoader::ConvertToUTF16(nsnull, reinterpret_cast<const PRUint8*>(buf.get()), len,
                                            charset, nsnull, script);

        if (NS_FAILED(rv)) {
            return ReportError(cx, LOAD_ERROR_BADCHARSET);
        }

        *scriptp =
            JS_CompileUCScriptForPrincipals(cx, target_obj, nsJSPrincipals::get(principal),
                                            reinterpret_cast<const jschar*>(script.get()),
                                            script.Length(), uriStr, 1);
    } else {
        *scriptp = JS_CompileScriptForPrincipals(cx, target_obj, nsJSPrincipals::get(principal),
                                                 buf.get(), len, uriStr, 1);
    }

    /* repent for our evil deeds */
    JS_SetErrorReporter(cx, er);

    return NS_OK;
}

NS_IMETHODIMP
mozJSSubScriptLoader::LoadSubScript(const nsAString& url,
                                    const JS::Value& target,
                                    const nsAString& charset,
                                    JSContext* cx,
                                    JS::Value* retval)
{
    /*
     * Loads a local url and evals it into the current cx
     * Synchronous (an async version would be cool too.)
     *   url: The url to load.  Must be local so that it can be loaded
     *        synchronously.
     *   target_obj: Optional object to eval the script onto (defaults to context
     *               global)
     *   returns: Whatever jsval the script pointed to by the url returns.
     * Should ONLY (O N L Y !) be called from JavaScript code.
     */

    nsresult rv = NS_OK;

#ifdef NS_FUNCTION_TIMER
    NS_TIME_FUNCTION_FMT("%s (line %d) (url: %s)", MOZ_FUNCTION_NAME,
                         __LINE__, NS_LossyConvertUTF16toASCII(url).get());
#endif

    /* set the system principal if it's not here already */
    if (!mSystemPrincipal) {
        nsCOMPtr<nsIScriptSecurityManager> secman =
            do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
        if (!secman)
            return NS_OK;

        rv = secman->GetSystemPrincipal(getter_AddRefs(mSystemPrincipal));
        if (NS_FAILED(rv) || !mSystemPrincipal)
            return rv;
    }

    JSAutoRequest ar(cx);

    JSObject* targetObj;
    if (!JS_ValueToObject(cx, target, &targetObj))
        return NS_ERROR_ILLEGAL_VALUE;


    if (!targetObj) {
        // If the user didn't provide an object to eval onto, find the global
        // object by walking the parent chain of the calling object.
        nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
        NS_ENSURE_TRUE(xpc, NS_ERROR_FAILURE);

        nsAXPCNativeCallContext *cc = nsnull;
        rv = xpc->GetCurrentNativeCallContext(&cc);
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

        nsCOMPtr<nsIXPConnectWrappedNative> wn;
        rv = cc->GetCalleeWrapper(getter_AddRefs(wn));
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

        rv = wn->GetJSObject(&targetObj);
        NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

        targetObj = JS_GetGlobalForObject(cx, targetObj);
    }

    // Remember an object out of the calling compartment so that we
    // can properly wrap the result later.
    nsCOMPtr<nsIPrincipal> principal = mSystemPrincipal;
    JSObject *result_obj = targetObj;
    targetObj = JS_FindCompilationScope(cx, targetObj);
    if (!targetObj)
        return NS_ERROR_FAILURE;

    if (targetObj != result_obj) {
        nsCOMPtr<nsIScriptSecurityManager> secman =
            do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
        if (!secman)
            return NS_ERROR_FAILURE;

        rv = secman->GetObjectPrincipal(cx, targetObj, getter_AddRefs(principal));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, targetObj))
        return NS_ERROR_UNEXPECTED;

    /* load up the url.  From here on, failures are reflected as ``custom''
     * js exceptions */
    nsCOMPtr<nsIURI> uri;
    nsCAutoString uriStr;
    nsCAutoString scheme;

    JSScript* script = nsnull;

    // Figure out who's calling us
    if (!JS_DescribeScriptedCaller(cx, &script, nsnull)) {
        // No scripted frame means we don't know who's calling, bail.
        return NS_ERROR_FAILURE;
    }

    // Suppress caching if we're compiling as content.
    StartupCache* cache = (principal == mSystemPrincipal)
                          ? StartupCache::GetSingleton()
                          : nsnull;
    nsCOMPtr<nsIIOService> serv = do_GetService(NS_IOSERVICE_CONTRACTID);
    if (!serv) {
        return ReportError(cx, LOAD_ERROR_NOSERVICE);
    }

    // Make sure to explicitly create the URI, since we'll need the
    // canonicalized spec.
    rv = NS_NewURI(getter_AddRefs(uri), NS_LossyConvertUTF16toASCII(url).get(), nsnull, serv);
    if (NS_FAILED(rv)) {
        return ReportError(cx, LOAD_ERROR_NOURI);
    }

    rv = uri->GetSpec(uriStr);
    if (NS_FAILED(rv)) {
        return ReportError(cx, LOAD_ERROR_NOSPEC);
    }

    rv = uri->GetScheme(scheme);
    if (NS_FAILED(rv)) {
        return ReportError(cx, LOAD_ERROR_NOSCHEME);
    }

    if (!scheme.EqualsLiteral("chrome")) {
        // This might be a URI to a local file, though!
        nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(uri);
        nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(innerURI);
        if (!fileURL) {
            return ReportError(cx, LOAD_ERROR_URI_NOT_LOCAL);
        }

        // For file URIs prepend the filename with the filename of the
        // calling script, and " -> ". See bug 418356.
        nsCAutoString tmp(JS_GetScriptFilename(cx, script));
        tmp.AppendLiteral(" -> ");
        tmp.Append(uriStr);

        uriStr = tmp;
    }

    bool writeScript = false;
    JSVersion version = JS_GetVersion(cx);
    nsCAutoString cachePath;
    cachePath.AppendPrintf("jssubloader/%d", version);
    PathifyURI(uri, cachePath);

    script = nsnull;
    if (cache)
        rv = ReadCachedScript(cache, cachePath, cx, mSystemPrincipal, &script);
    if (!script) {
        rv = ReadScript(uri, cx, targetObj, charset,
                        static_cast<const char*>(uriStr.get()), serv,
                        principal, &script);
        writeScript = true;
    }

    if (NS_FAILED(rv) || !script)
        return rv;

    bool ok = JS_ExecuteScriptVersion(cx, targetObj, script, retval, version);

    if (ok) {
        JSAutoEnterCompartment rac;
        if (!rac.enter(cx, result_obj) || !JS_WrapValue(cx, retval))
            return NS_ERROR_UNEXPECTED;
    }

    if (cache && ok && writeScript) {
        WriteCachedScript(cache, cachePath, cx, mSystemPrincipal, script);
    }

    return NS_OK;
}
