/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
#include "jsapi.h"
#include "prmem.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsIInputStream.h"
#include "nsIStringStream.h"
#include "nsIURI.h"
#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectData.h"
#include "nsJSProtocolHandler.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsProxyObjectManager.h"
#include "nsIWebShell.h"
#include "nsDOMError.h"
#include "nsIInterfaceRequestor.h"
#include "nsIEvaluateStringProxy.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

// XXXbe why isn't there a static GetCID on nsIComponentManager?
static NS_DEFINE_CID(kComponentManagerCID,  NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kJSProtocolHandlerCID, NS_JSPROTOCOLHANDLER_CID);

/***************************************************************************/
/* nsEvaluateStringProxy                                                   */
/* This private class will allow us to evaluate js on another thread       */
/***************************************************************************/
class nsEvaluateStringProxy : public nsIEvaluateStringProxy {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIEVALUATESTRINGPROXY

    nsEvaluateStringProxy();
    virtual ~nsEvaluateStringProxy();
};


nsEvaluateStringProxy::nsEvaluateStringProxy()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

nsEvaluateStringProxy::~nsEvaluateStringProxy()
{
}

NS_IMPL_ISUPPORTS1(nsEvaluateStringProxy, nsIEvaluateStringProxy)

NS_IMETHODIMP
nsEvaluateStringProxy::EvaluateString(nsIScriptContext *scriptContext,
                                      const char *script,
                                      void * aObj,
                                      nsIPrincipal *principal,
                                      const char *aURL,
                                      PRInt32 aLineNo,
                                      char **aRetValue,
                                      PRBool *aIsUndefined)
{
    nsString string;
    nsresult rv;

    rv =   scriptContext->EvaluateString(nsString(script),
                                         aObj,
                                         principal,
                                         aURL,
                                         aLineNo,
                                         nsnull,
                                         string,
                                         aIsUndefined);
    // XXXbe this should not decimate! pass back UCS-2 to necko
    *aRetValue = string.ToNewCString();
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

nsJSProtocolHandler::nsJSProtocolHandler()
{
    NS_INIT_REFCNT();
}

nsresult
nsJSProtocolHandler::Init()
{
    return NS_OK;
}

nsJSProtocolHandler::~nsJSProtocolHandler()
{
}

NS_IMPL_ISUPPORTS(nsJSProtocolHandler, NS_GET_IID(nsIProtocolHandler));

NS_METHOD
nsJSProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsJSProtocolHandler* ph = new nsJSProtocolHandler();
    if (!ph)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ph);
    nsresult rv = ph->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = ph->QueryInterface(aIID, aResult);
    }
    NS_RELEASE(ph);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsJSProtocolHandler::GetScheme(char* *result)
{
    *result = nsCRT::strdup("javascript");
    if (!*result)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsJSProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for javascript: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsJSProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                            nsIURI **result)
{
    nsresult rv;

    // javascript: URLs (currently) have no additional structure beyond that
    // provided by standard URLs, so there is no "outer" object given to
    // CreateInstance.

    nsIURI* url;
    if (aBaseURI) {
        rv = aBaseURI->Clone(&url);
    } else {
        rv = nsComponentManager::CreateInstance(kSimpleURICID, nsnull,
                                                NS_GET_IID(nsIURI),
                                                (void**)&url);
    }
    if (NS_FAILED(rv))
        return rv;

    rv = url->SetSpec((char*)aSpec);
    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        return rv;
    }

    *result = url;
    return rv;
}

NS_IMETHODIMP
nsJSProtocolHandler::NewChannel(const char* verb, 
                                nsIURI* uri,
                                nsILoadGroup* aLoadGroup,
                                nsIInterfaceRequestor* notificationCallbacks,
                                nsLoadFlags loadAttributes,
                                nsIURI* originalURI,
                                nsIChannel* *result)
{
    NS_ENSURE_ARG_POINTER(uri);

    nsresult rv;

    // The event sink must be a script context owner or we fail.
    nsCOMPtr<nsIScriptContextOwner> owner;
    rv = notificationCallbacks->GetInterface(NS_GET_IID(nsIScriptContextOwner),
                                             getter_AddRefs(owner));
    if (NS_FAILED(rv))
        return rv;
    if (!owner)
        return NS_ERROR_FAILURE;

    // So far so good: get the script context from its owner.
    nsCOMPtr<nsIScriptContext> scriptContext;
    rv = owner->GetScriptContext(getter_AddRefs(scriptContext));
    if (NS_FAILED(rv))
        return rv;

    // Get principal of code for execution
    NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager,
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    PRBool hasPrincipal;
    if (NS_FAILED(securityManager->HasSubjectPrincipal(&hasPrincipal)))
        return NS_ERROR_FAILURE;
    nsCOMPtr<nsIPrincipal> principal;
    if (hasPrincipal) {
        // script is currently executing; get principal from that script
        if (NS_FAILED(securityManager->GetSubjectPrincipal(getter_AddRefs(principal))))
            return NS_ERROR_FAILURE;
    } else {
        // No scripts currently executing; get principal from referrer of link
        nsCOMPtr<nsIWebShell> webShell;
        webShell = do_QueryInterface(owner);
        if (!webShell)
          return NS_ERROR_FAILURE;
        const PRUnichar* url;
        if (NS_FAILED(webShell->GetURL(&url)))
          return NS_ERROR_FAILURE;
        nsCString urlStr(url);
        nsCOMPtr<nsIURI> uri;
        if (NS_FAILED(NewURI(urlStr.GetBuffer(), nsnull, getter_AddRefs(uri))))
          return NS_ERROR_FAILURE;
        if (NS_FAILED(securityManager->GetCodebasePrincipal(uri, getter_AddRefs(principal))))
          return NS_ERROR_FAILURE;
    }


    //TODO Change this to GetSpec and then skip past the javascript:
    // Get the expression:

    char* str;
    rv = uri->GetPath(&str);
    if (NS_FAILED(rv))
        return rv;

    nsString jsExpr(str);

    nsCRT::free(str);

    // Finally, we have everything needed to evaluate the expression.
    // We do this by proxying back to the main thread.

    PRBool isUndefined;

    NS_WITH_SERVICE(nsIProxyObjectManager, proxyObjectManager,
                    nsIProxyObjectManager::GetCID(), &rv);

    if (NS_FAILED(rv))
        return rv;

    nsIEvaluateStringProxy* evalProxy = nsnull;
    nsEvaluateStringProxy*  eval      = new nsEvaluateStringProxy();

    if (eval == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = proxyObjectManager->GetProxyObject(NS_UI_THREAD_EVENTQ,
                                            nsCOMTypeInfo<nsIEvaluateStringProxy>::GetIID(),
                                            NS_STATIC_CAST(nsISupports*, eval),
                                            PROXY_SYNC | PROXY_ALWAYS,
                                            (void**) &evalProxy);

    if (NS_FAILED(rv)) {        
        NS_RELEASE(eval);
        return rv;
    }

    char* retString;
    char* tempString = jsExpr.ToNewCString();
    if (!tempString) {
        NS_RELEASE(evalProxy);
        NS_RELEASE(eval);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = evalProxy->EvaluateString(scriptContext, tempString, nsnull, principal, nsnull, 0, &retString, &isUndefined);

    Recycle(tempString);

    NS_RELEASE(evalProxy);
    NS_RELEASE(eval);

    if (NS_FAILED(rv)) {
        rv = NS_ERROR_MALFORMED_URI;
        return rv;
    }

    if (isUndefined) {
        if (retString) 
          Recycle(retString);
        rv = NS_ERROR_DOM_RETVAL_UNDEFINED;
        return rv;
    }
#if 0
    else {
// This is from the old code which need to be hack on a bit more:

// plaintext is apparently busted
        if (ret[0] != PRUnichar('<'))
            ret.Insert("<plaintext>\n", 0);
        mLength = ret.Length();
        PRUint32 resultSize = mLength + 1;
        mResult = (char *)PR_Malloc(resultSize);
        ret.ToCString(mResult, resultSize);
    }
#endif

    nsCOMPtr<nsISupports> s;
    rv = NS_NewStringInputStream(getter_AddRefs(s), retString);
    int length = PL_strlen(retString);
    Recycle(retString);

    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIInputStream> in = do_QueryInterface(s, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIChannel* channel;

    rv = NS_NewInputStreamChannel(uri, "text/html", length,
                                  in, aLoadGroup,
                                  notificationCallbacks, loadAttributes,
                                  originalURI, &channel);
    if (NS_FAILED(rv))
        return rv;

    *result = channel;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

// Factory methods
extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServMgr,
             const nsCID& aClass,
             const char* aClassName,
             const char *aProgID,
             nsIFactory* *aFactory)
{
    if (!aFactory)
        return NS_ERROR_NULL_POINTER;

    if (!aClass.Equals(kJSProtocolHandlerCID))
        return NS_ERROR_FAILURE;

    nsIGenericFactory* fact;
    nsresult rv;
    rv = NS_NewGenericFactory(&fact, nsJSProtocolHandler::Create);
    if (NS_SUCCEEDED(rv))
        *aFactory = fact;
    return rv;
}

extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;

    NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv))
        return rv;

    rv = compMgr->RegisterComponent(kJSProtocolHandlerCID,
                                    "JavaScript Protocol Handler",
                                    NS_NETWORK_PROTOCOL_PROGID_PREFIX "javascript",
                                    aPath, PR_TRUE, PR_TRUE);

    if (NS_FAILED(rv))
        return rv;

    rv = compMgr->RegisterComponent(kJSProtocolHandlerCID,
                                    "JavaScript Protocol Handler",
                                    NS_NETWORK_PROTOCOL_PROGID_PREFIX "javascript",
                                    aPath, PR_TRUE, PR_TRUE);


    return rv;
}

extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;

    NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv))
        return rv;

    rv = compMgr->UnregisterComponent(kJSProtocolHandlerCID, aPath);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
