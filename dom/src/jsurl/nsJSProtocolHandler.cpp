/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifdef NECKO

#include "prmem.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "nsIEventSinkGetter.h"
#include "nsIEventQueue.h"
#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsJSProtocolHandler.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

// XXXbe why isn't there a static GetCID on nsIComponentManager?
static NS_DEFINE_CID(kComponentManagerCID,  NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kJSProtocolHandlerCID, NS_JSPROTOCOLHANDLER_CID);

////////////////////////////////////////////////////////////////////////////////

class nsJSInputStream : public nsIInputStream
{
public:
    NS_DECL_ISUPPORTS

    nsJSInputStream()
        : mVerb(nsnull), mURI(nsnull), mEventSinkGetter(nsnull),
          mResult(nsnull), mLength(0), mReadCursor(0) {
        NS_INIT_REFCNT();
    }

    virtual ~nsJSInputStream() {
        if (mVerb)
            nsCRT::free(mVerb);
        NS_IF_RELEASE(mURI);
        NS_IF_RELEASE(mEventSinkGetter);
        if (mResult)
            nsCRT::free(mResult);
    }

    nsresult Init(const char* verb, nsIURI* uri,
                  nsIEventSinkGetter* eventSinkGetter) {
        mVerb = nsCRT::strdup(verb);    // XXX do we need this?
        if (!mVerb)
            return NS_ERROR_OUT_OF_MEMORY;
        mURI = uri;
        NS_ADDREF(mURI);
        mEventSinkGetter = eventSinkGetter;
        NS_IF_ADDREF(mEventSinkGetter);
        return NS_OK;
    }

    NS_IMETHOD Close() {
        return NS_OK;
    }

    NS_IMETHOD GetLength(PRUint32 *_retval) {
        if (!mResult) {
            nsresult rv = Eval();
            if (NS_FAILED(rv))
                return rv;
            NS_ASSERTION(mResult, "Eval didn't set mResult");
        }
        PRUint32 rest = mLength - mReadCursor;
        if (!rest)
            return NS_BASE_STREAM_EOF;
        *_retval = rest;
        return NS_OK;
    }

    NS_IMETHOD Read(char * buf, PRUint32 count, PRUint32 *_retval) {
        nsresult rv;
        PRUint32 rest;
        rv = GetLength(&rest);
        if (rv != NS_OK)
            return rv;
        PRUint32 amt = PR_MIN(count, rest);
        nsCRT::memcpy(buf, &mResult[mReadCursor], amt);
        mReadCursor += amt;
        *_retval = amt;
        return NS_OK;
    }

    nsresult Eval() {
        nsresult rv;

        if (!mEventSinkGetter)
            return NS_ERROR_FAILURE;

        //TODO Change this to GetSpec and then skip past the javascript:
        // Get the expression:
        char* jsExpr;
        rv = mURI->GetPath(&jsExpr);
        if (NS_FAILED(rv))
            return rv;

        // The event sink must be a script context owner or we fail.
        nsIScriptContextOwner* owner;
        rv = mEventSinkGetter->GetEventSink(mVerb,
                                            NS_GET_IID(nsIScriptContextOwner),
                                            (nsISupports**)&owner);
        if (NS_FAILED(rv))
            return rv;
        if (!owner)
            return NS_ERROR_FAILURE;

        // So far so good: get the script context from its owner.
        nsIScriptContext* scriptContext;
        rv = owner->GetScriptContext(&scriptContext);
        NS_RELEASE(owner);
        if (NS_FAILED(rv))
            return rv;

        // Finally, we have everything needed to evaluate the expression.
        nsAutoString ret;
        PRBool isUndefined;
        rv = scriptContext->EvaluateString(nsString(jsExpr), nsnull, 0, ret,
                                           &isUndefined);
        nsCRT::free(jsExpr);
        if (NS_FAILED(rv)) {
            rv = NS_ERROR_MALFORMED_URI;
        } else {
            // Find out if it can be converted into a string
            if (isUndefined) {
                mResult = nsCRT::strdup("");
            } else {
#if 0   // plaintext is apparently busted
                if (ret[0] != PRUnichar('<'))
                    ret.Insert("<plaintext>\n", 0);
#endif
                mLength = ret.Length();
                PRUint32 resultSize = mLength + 1;
                mResult = (char *)PR_Malloc(resultSize);
                ret.ToCString(mResult, resultSize);
            }
        }
        NS_RELEASE(scriptContext);
        return rv;
    }

protected:
    char*               mVerb;
    nsIURI*             mURI;
    nsIEventSinkGetter* mEventSinkGetter;
    char*               mResult;
    PRUint32            mLength;
    PRUint32            mReadCursor;
};

NS_IMPL_ISUPPORTS(nsJSInputStream, NS_GET_IID(nsIInputStream));

nsresult
NS_NewJSInputStream(const char* verb, nsIURI* uri,
                    nsIEventSinkGetter* eventSinkGetter,
                    nsIInputStream* *result)
{
    nsJSInputStream* js = new nsJSInputStream();
    if (!js)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(js);

    nsresult rv = js->Init(verb, uri, eventSinkGetter);
    if (NS_FAILED(rv)) {
        NS_RELEASE(js);
        return rv;
    }
    *result = js;
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
nsJSProtocolHandler::MakeAbsolute(const char* aSpec,
                                  nsIURI* aBaseURI,
                                  char* *result)
{
    // presumably, there's no such thing as a relative javascript: URI,
    // so just copy the input spec
    char* dup = nsCRT::strdup(aSpec);
    if (!dup)
        return NS_ERROR_OUT_OF_MEMORY;
    *result = dup;
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
nsJSProtocolHandler::NewChannel(const char* verb, nsIURI* uri,
                                nsILoadGroup *aGroup,
                                nsIEventSinkGetter* eventSinkGetter,
                                nsIChannel* *result)
{
    // Strategy: Let's use an "input stream channel" here with a special
    // implementation of nsIInputStream that evaluates the JS expression.
    // That seems simpler than implementing a new channel class.
    nsresult rv;
    nsIChannel* channel;
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv))
        return rv;

    nsIInputStream* in;
    rv = NS_NewJSInputStream(verb, uri, eventSinkGetter, &in);
    if (NS_FAILED(rv))
        return rv;

    rv = serv->NewInputStreamChannel(uri, "text/html", in, &channel);
    NS_RELEASE(in);
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
#endif // NECKO
