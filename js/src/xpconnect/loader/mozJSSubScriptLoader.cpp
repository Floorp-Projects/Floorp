/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Robert Ginda <rginda@netscape.com>
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

#if !defined(XPCONNECT_STANDALONE) && !defined(NO_SUBSCRIPT_LOADER)

#include "mozJSSubScriptLoader.h"

#include "nsIServiceManager.h"
#include "nsIXPConnect.h"

#include "nsIURI.h"
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsNetCID.h"

#include "jsapi.h"

static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);

/* load() error msgs, XXX localize? */
#define LOAD_ERROR_NOSERVICE "Error creating IO Service."
#define LOAD_ERROR_NOCHANNEL "Error creating channel (invalid URL scheme?)"
#define LOAD_ERROR_NOSTREAM  "Error opening input stream (invalid filename?)"
#define LOAD_ERROR_NOCONTENT "ContentLength not available (not a local URL?)"
#define LOAD_ERROR_BADREAD   "File Read Error."
#define LOAD_ERROR_READUNDERFLOW "File Read Error (underflow.)"

/* turn ALL JS Runtime errors into exceptions */
JS_STATIC_DLL_CALLBACK(void)
ExceptionalErrorReporter (JSContext *cx, const char *message,
                          JSErrorReport *report)
{
    JSObject *ex;
    JSString *jstr;
    JSBool ok;

    if (report && JSREPORT_IS_EXCEPTION (report->flags))
        /* if it's already an exception, our job is done. */
        return;
    
    ex = JS_NewObject (cx, nsnull, nsnull, nsnull);
    /* create a jsobject to throw */
    if (!ex)
        goto panic;

    /* decorate the exception */
    if (message)
    {
        jstr = JS_NewStringCopyZ (cx, message);
        if (!jstr)
            goto panic;
        ok = JS_DefineProperty (cx, ex, "message", STRING_TO_JSVAL(jstr),
                                nsnull, nsnull, JSPROP_ENUMERATE);
        if (!ok)
            goto panic;
    }

    if (report)
    {
        jstr = JS_NewStringCopyZ (cx, report->filename);
        if (!jstr)
            goto panic;
        ok = JS_DefineProperty (cx, ex, "fileName", STRING_TO_JSVAL(jstr),
                                nsnull, nsnull, JSPROP_ENUMERATE);
        if (!ok)
            goto panic;

        ok = JS_DefineProperty (cx, ex, "lineNumber",
                                INT_TO_JSVAL(NS_STATIC_CAST(uintN,
                                                            report->lineno)),
                                nsnull, nsnull, JSPROP_ENUMERATE);
        if (!ok)
            goto panic;
    }

    JS_SetPendingException (cx, OBJECT_TO_JSVAL(ex));

    return;
    
  panic:
#ifdef DEBUG
    fprintf (stderr,
             "mozJSSubScriptLoader: Error occurred while reporting error :/\n")
#endif
    ;
}

mozJSSubScriptLoader::mozJSSubScriptLoader() : mSystemPrincipal(nsnull)
{
    NS_INIT_ISUPPORTS();
}

mozJSSubScriptLoader::~mozJSSubScriptLoader()    
{
    /* empty */
}

NS_IMPL_THREADSAFE_ISUPPORTS1(mozJSSubScriptLoader, mozIJSSubScriptLoader)

NS_IMETHODIMP /* args and return value are delt with using XPConnect and JSAPI */
mozJSSubScriptLoader::LoadSubScript (const PRUnichar * /*url*/
                                     /* [, JSObject *target_obj] */)
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
    
    /* gotta define most of this stuff up here because of all the gotos,
     * defined the rest up here to be consistent */
    nsresult  rv;
    JSBool    ok;

    /* get JS things from the CallContext */
    nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
    if (!xpc) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIXPCNativeCallContext> cc;
    rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    JSContext *cx;
    rv = cc->GetJSContext (&cx);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    PRUint32 argc;
    rv = cc->GetArgc (&argc);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    jsval *argv;
    rv = cc->GetArgvPtr (&argv);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    jsval *rval;
    rv = cc->GetRetValPtr (&rval);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;    

    /* set mJSPrincipals if it's not here already */
    if (!mSystemPrincipal)
    {
        nsCOMPtr<nsIScriptSecurityManager> secman =
            do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
        if (!secman)
            return rv;

        rv = secman->GetSystemPrincipal(getter_AddRefs(mSystemPrincipal));
        if (NS_FAILED(rv) || !mSystemPrincipal)
            return rv;

    }
    
    char     *url;
    JSObject *target_obj = nsnull;
    ok = JS_ConvertArguments (cx, argc, argv, "s / o", &url, &target_obj);
    if (!ok)
    {
        cc->SetExceptionWasThrown (JS_TRUE);
        /* let the exception raised by JS_ConvertArguments show through */
        return NS_OK;
    }
    
    if (!target_obj)
    {
        /* if the user didn't provide an object to eval onto, find the global
         * object by walking the parent chain of the calling object */

#ifdef DEBUG_rginda
        JSObject *got_glob = JS_GetGlobalObject (cx);
        fprintf (stderr, "JS_GetGlobalObject says glob is %p.\n", got_glob);
        target_obj = JS_GetPrototype (cx, got_glob);
        fprintf (stderr, "That glob's prototype is %p.\n", target_obj);
        target_obj = JS_GetParent (cx, got_glob);
        fprintf (stderr, "That glob's parent is %p.\n", target_obj);
#endif
        
        nsCOMPtr<nsIXPConnectWrappedNative> wn;
        rv = cc->GetCalleeWrapper (getter_AddRefs(wn));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;    

        rv = wn->GetJSObject (&target_obj);
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

#ifdef DEBUG_rginda
        fprintf (stderr, "Parent chain: %p", target_obj);
#endif
        JSObject *maybe_glob = JS_GetParent (cx, target_obj);
        while (maybe_glob != nsnull)
        {
#ifdef DEBUG_rginda
            fprintf (stderr, ", %p", maybe_glob);
#endif
            target_obj = maybe_glob;
            maybe_glob = JS_GetParent (cx, maybe_glob);
        }
#ifdef DEBUG_rginda
        fprintf (stderr, "\n");
#endif  
        
    }

    /* load up the url.  From here on, failures are reflected as ``custom''
     * js exceptions */
    PRInt32   len = -1;
    PRUint32  readcount;
    char     *buf = nsnull;
    
    JSString        *errmsg;
    JSErrorReporter  er;
    JSPrincipals    *jsPrincipals;
    
    nsCOMPtr<nsIChannel>     chan;
    nsCOMPtr<nsIInputStream> instream;

    nsCOMPtr<nsIIOService> serv = do_GetService(kIOServiceCID);
    if (!serv)
    {
        errmsg = JS_NewStringCopyZ (cx, LOAD_ERROR_NOSERVICE);
        goto return_exception;
    }

    rv = serv->NewChannel(url, NS_STATIC_CAST(nsIURI *, nsnull),
                          getter_AddRefs(chan));
    if (NS_FAILED(rv))
    {
        errmsg = JS_NewStringCopyZ (cx, LOAD_ERROR_NOCHANNEL);
        goto return_exception;
    }

    rv = chan->Open (getter_AddRefs(instream));
    if (NS_FAILED(rv))
    {
        errmsg = JS_NewStringCopyZ (cx, LOAD_ERROR_NOSTREAM);
        goto return_exception;
    }
    
    rv = chan->GetContentLength (&len);
    if (NS_FAILED(rv))
    {
        errmsg = JS_NewStringCopyZ (cx, LOAD_ERROR_NOCONTENT);
        goto return_exception;
    }

    buf = new char[len + 1];
    if (!buf)
        return NS_ERROR_OUT_OF_MEMORY;
    
    rv = instream->Read (buf, len, &readcount);
    if (NS_FAILED(rv))
    {
        errmsg = JS_NewStringCopyZ (cx, LOAD_ERROR_BADREAD);
        goto return_exception;
    }
    
    if (NS_STATIC_CAST(PRUint32, len) != readcount)
    {
        errmsg = JS_NewStringCopyZ (cx, LOAD_ERROR_READUNDERFLOW);
        goto return_exception;
    }

    /* we can't hold onto jsPrincipals as a module var because the
     * JSPRINCIPALS_DROP macro takes a JSContext, which we won't have in the
     * destructor */
    rv = mSystemPrincipal->GetJSPrincipals(&jsPrincipals);
    if (NS_FAILED(rv) || !jsPrincipals)
        return rv;

    /* set our own error reporter so we can report any bad things as catchable
     * exceptions, including the source/line number */
    er = JS_SetErrorReporter (cx, ExceptionalErrorReporter);

    ok = JS_EvaluateScriptForPrincipals (cx, target_obj, jsPrincipals,
                                         buf, len, url, 1, rval);        
    /* repent for our evil deeds */
    JS_SetErrorReporter (cx, er);

    cc->SetExceptionWasThrown (!ok);
    cc->SetReturnValueWasSet (ok);

    delete[] buf;

    return NS_OK;

 return_exception:
    if (buf)
        delete[] buf;

    JS_SetPendingException (cx, STRING_TO_JSVAL(errmsg));
    cc->SetExceptionWasThrown (JS_TRUE);
    return NS_OK;

}

#endif /* NO_SUBSCRIPT_LOADER */
