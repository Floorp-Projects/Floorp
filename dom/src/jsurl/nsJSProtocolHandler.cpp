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

#include "nsJSProtocolHandler.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "nsIEventSinkGetter.h"
#include "nsIEventQueue.h"
#include "nsIScriptContext.h"
#include "prmem.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

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
        if (mVerb) nsCRT::free(mVerb);
        NS_IF_RELEASE(mURI);
        NS_IF_RELEASE(mEventSinkGetter);
        if (mResult) nsCRT::free(mResult);
    }

    nsresult Init(const char* verb, nsIURI* uri,
                  nsIEventSinkGetter* eventSinkGetter) {
        mVerb = nsCRT::strdup(verb);    // XXX do we need this?
        if (mVerb == nsnull) return NS_ERROR_OUT_OF_MEMORY;
        mURI = uri;
        NS_ADDREF(mURI);
        mEventSinkGetter = eventSinkGetter;
        NS_ADDREF(mEventSinkGetter);
        return NS_OK;
    }

    NS_IMETHOD Close() {
        return NS_OK;
    }

    NS_IMETHOD GetLength(PRUint32 *_retval) {
        return mResult ? nsCRT::strlen(mResult) : 0;
    }

    NS_IMETHOD Read(char * buf, PRUint32 count, PRUint32 *_retval) {
        if (mResult == nsnull) {
            Eval();
            NS_ASSERTION(mResult, "Eval didn't set mResult");
        }

        // stream out the result:
        PRUint32 rest = mLength - mReadCursor;
        if (rest == 0)
            return NS_BASE_STREAM_EOF;
        PRUint32 amt = PR_MIN(count, rest);
        nsCRT::memcpy(buf, &mResult[mReadCursor], amt);
        mReadCursor += amt;
        return NS_OK;
    }

    nsresult Eval() {
        nsresult rv;

        //TODO Change this to GetSpec and then skip past the javascript: 
        // Get the expression:
        char* jsExpr;
        rv = mURI->GetPath(&jsExpr);
        if (NS_FAILED(rv)) return rv;

        // Get the script context:
        nsIScriptContext* scriptContext;
        rv = mEventSinkGetter->GetEventSink(mVerb, nsIScriptContext::GetIID(),
                                            (nsISupports**)&scriptContext);
        if (NS_FAILED(rv)) return rv;

        nsAutoString ret;
        PRBool isUndefined;
        PRBool ok;
        ok = scriptContext->EvaluateString(nsString(jsExpr),
                                           nsnull, 0, ret, &isUndefined);
        nsCRT::free(jsExpr);
        if (ok) {
//            JSContext* cx = (JSContext*)scriptContext->GetNativeContext();
            // Find out if it can be converted into a string
            if (!isUndefined) {
                mLength = ret.Length();
                mResult = (char *)PR_Malloc(mLength + 1);
                ret.ToCString(mResult, mLength);
            }
            else {
                mResult = nsCRT::strdup("");
            }
        }
        else {
            rv = NS_ERROR_MALFORMED_URI;
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

NS_IMPL_ISUPPORTS(nsJSInputStream, nsIInputStream::GetIID());

nsresult
NS_NewJSInputStream(const char* verb, nsIURI* uri,
                    nsIEventSinkGetter* eventSinkGetter,
                    nsIInputStream* *result)
{
    nsJSInputStream* js = new nsJSInputStream();
    if (js == nsnull)
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

NS_IMPL_ISUPPORTS(nsJSProtocolHandler, nsCOMTypeInfo<nsIProtocolHandler>::GetIID());

NS_METHOD
nsJSProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsJSProtocolHandler* ph = new nsJSProtocolHandler();
    if (ph == nsnull)
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
    if (*result == nsnull)
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
    if (dup == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    *result = dup;
    return NS_OK;
}

NS_IMETHODIMP
nsJSProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                            nsIURI **result)
{
    nsresult rv;

    // javascript: URLs (currently) have no additional structure beyond that provided by standard
    // URLs, so there is no "outer" given to CreateInstance 

    nsIURI* url;
    if (aBaseURI) {
        rv = aBaseURI->Clone(&url);
    }
    else {
        rv = nsComponentManager::CreateInstance(kSimpleURICID, nsnull,
                                                nsCOMTypeInfo<nsIURI>::GetIID(),
                                                (void**)&url);
    }
    if (NS_FAILED(rv)) return rv;

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
                                nsIEventSinkGetter* eventSinkGetter,
                                nsIChannel* *result)
{
    // Strategy: Let's use an "input stream channel" here with a special 
    // implementation of nsIInputStream that evaluates the JS expression.
    // That seems simpler than implementing a new channel class.
    nsresult rv;
    nsIChannel* channel;
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIInputStream* in;
    rv = NS_NewJSInputStream(verb, uri, eventSinkGetter, &in);
    if (NS_FAILED(rv)) return rv;

    rv = serv->NewInputStreamChannel(uri, "text/html", in, &channel);
    NS_RELEASE(in);
    if (NS_FAILED(rv)) return rv;

    *result = channel;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
#endif // NECKO
