/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* An implementaion nsIXPCException. */

#include "xpcprivate.h"

/***************************************************************************/
/* Quick and dirty mapping of well known result codes to strings. We only 
*  call this when building an exception object, so iterating the short array
*  is not too bad.
* 
*  It sure would be nice to have exceptions declared in idl and available
*  in some more global way at runtime.
*/

// static 
const char* 
nsXPCException::NameForNSResult(nsresult rv)
{
#ifdef MAP_ENTRY
#undef MAP_ENTRY
#endif
#define MAP_ENTRY(r) {(r),#r},
    struct ResultMap {nsresult rv; const char* name;} map[] = {
        MAP_ENTRY(NS_OK)
        MAP_ENTRY(NS_ERROR_BASE)
        MAP_ENTRY(NS_ERROR_NOT_INITIALIZED)
        MAP_ENTRY(NS_ERROR_ALREADY_INITIALIZED)
        MAP_ENTRY(NS_ERROR_NOT_IMPLEMENTED)
        MAP_ENTRY(NS_ERROR_NO_INTERFACE)
        MAP_ENTRY(NS_ERROR_INVALID_POINTER)
        MAP_ENTRY(NS_ERROR_ABORT)
        MAP_ENTRY(NS_ERROR_FAILURE)
        MAP_ENTRY(NS_ERROR_UNEXPECTED)
        MAP_ENTRY(NS_ERROR_OUT_OF_MEMORY)
        MAP_ENTRY(NS_ERROR_ILLEGAL_VALUE)
        MAP_ENTRY(NS_ERROR_NO_AGGREGATION)
        MAP_ENTRY(NS_ERROR_NOT_AVAILABLE)
        MAP_ENTRY(NS_ERROR_FACTORY_NOT_REGISTERED)
        MAP_ENTRY(NS_ERROR_FACTORY_NOT_LOADED)
        MAP_ENTRY(NS_ERROR_FACTORY_NO_SIGNATURE_SUPPORT)
        MAP_ENTRY(NS_ERROR_FACTORY_EXISTS)
        MAP_ENTRY(NS_ERROR_PROXY_INVALID_IN_PARAMETER)
        MAP_ENTRY(NS_ERROR_PROXY_INVALID_OUT_PARAMETER)
        {0,0}   // sentinel to mark end of array
    };
#undef MAP_ENTRY

    for(ResultMap* p = map; p->name; p++)
        if(rv == p->rv)
            return p->name;
    return nsnull;    
}        

/***************************************************************************/

NS_IMPL_ISUPPORTS(nsXPCException, NS_GET_IID(nsIXPCException))

nsXPCException::nsXPCException()
    : mMessage(nsnull),
      mCode(0),
      mLocation(nsnull),
      mData(nsnull),
      mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsXPCException::~nsXPCException()
{
    reset();
}

void
nsXPCException::reset()
{
    if(mMessage)
    {
        nsAllocator::Free(mMessage);
        mMessage = nsnull;
    }

    NS_IF_RELEASE(mLocation);
    NS_IF_RELEASE(mData);
}

/* readonly attribute string message; */
NS_IMETHODIMP
nsXPCException::GetMessage(char * *aMessage)
{
    if(!aMessage)
        return NS_ERROR_NULL_POINTER;
    if(!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;
    if(!mMessage)
    {
        *aMessage = nsnull;
        return NS_OK;
    }
    char* retval = (char*) nsAllocator::Clone(mMessage,
                                            sizeof(char)*(strlen(mMessage)+1));
    *aMessage = retval;
    return retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute PRInt32 code; */
NS_IMETHODIMP
nsXPCException::GetCode(PRInt32 *aCode)
{
    if(!aCode)
        return NS_ERROR_NULL_POINTER;
    if(!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;
    *aCode = mCode;
    return NS_OK;
}

/* readonly attribute nsIJSStackFrameLocation location; */
NS_IMETHODIMP
nsXPCException::GetLocation(nsIJSStackFrameLocation * *aLocation)
{
    if(!aLocation)
        return NS_ERROR_NULL_POINTER;
    if(!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;
    *aLocation = mLocation;
    NS_IF_ADDREF(mLocation);
    return NS_OK;
}

/* readonly attribute nsISupports data; */
NS_IMETHODIMP
nsXPCException::GetData(nsISupports * *aData)
{
    if(!aData)
        return NS_ERROR_NULL_POINTER;
    if(!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;
    *aData = mData;
    NS_IF_ADDREF(mData);
    return NS_OK;
}

/* void initialize (in string aMessage, in PRInt32 aCode, in nsIJSStackFrameLocation aLocation); */
NS_IMETHODIMP
nsXPCException::initialize(const char *aMessage, PRInt32 aCode, nsIJSStackFrameLocation *aLocation, nsISupports *aData)
{
    if(mInitialized)
        return NS_ERROR_ALREADY_INITIALIZED;

    reset();

    if(aMessage)
    {
        if(!(mMessage = (char*) nsAllocator::Clone(aMessage,
                                           sizeof(char)*(strlen(aMessage)+1))))
            return NS_ERROR_OUT_OF_MEMORY;
    }

    mCode = aCode;

    if(aLocation)
    {
        mLocation = aLocation;
        NS_ADDREF(mLocation);
    }
    else
    {
        nsresult rv;
        rv = nsXPConnect::GetXPConnect()->GetCurrentJSStack(&mLocation);
        if(NS_FAILED(rv))
            return rv;
    }

    if(aData)
    {
        mData = aData;
        NS_ADDREF(mData);
    }

    mInitialized = PR_TRUE;
    return NS_OK;
}

/* string toString (); */
NS_IMETHODIMP
nsXPCException::toString(char **_retval)
{
    if(!_retval)
        return NS_ERROR_NULL_POINTER;
    if(!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;

    /* XXX we'd like to convert the error code into a string in the future */

    static const char defaultMsg[] = "<no message>";
    static const char defaultLocation[] = "<unknown>";
    static const char format[] = 
             "[Exception... \"%s\"  nsresult: %x (%s)  location: \"%s\"  data: %s]";

    char* indicatedLocation = nsnull;

    if(mLocation)
    {
        // we need to free this if it does not fail
        nsresult rv = mLocation->toString(&indicatedLocation);
        if(NS_FAILED(rv))
            return rv;
    }

    const char* msg = mMessage ? mMessage : defaultMsg;
    const char* location = indicatedLocation ? 
                                indicatedLocation : defaultLocation;
    const char* resultName = nsXPCException::NameForNSResult(mCode);
    if(!resultName)
        resultName ="<unknown>";

    const char* data = mData ? "yes" : "no";

    char* temp = JS_smprintf(format, msg, mCode, resultName, location, data);
    if(indicatedLocation)
        nsAllocator::Free(indicatedLocation);

    char* final = nsnull;
    if(temp)
    {
        final = (char*) nsAllocator::Clone(temp, 
                                        sizeof(char)*(strlen(temp)+1));
        JS_smprintf_free(temp);            
    }

    *_retval = final;
    return final ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


// static 
nsIXPCException* 
nsXPCException::NewException(const char *aMessage, 
                             PRInt32 aCode, 
                             nsIJSStackFrameLocation *aLocation,
                             nsISupports *aData,
                             PRInt32 aLeadingFramesToTrim)
{
    nsresult rv;
    nsIXPCException* e = new nsXPCException();
    if(e)
    {
        NS_ADDREF(e);

        nsIJSStackFrameLocation* location;        
        if(aLocation)
        {
            location = aLocation;
            NS_ADDREF(location);
        }
        else
        {
            rv = nsXPConnect::GetXPConnect()->GetCurrentJSStack(&location);
            if(NS_FAILED(rv) || !location)
            {
                NS_RELEASE(e);
                return nsnull;
            }
        }
        // at this point we have non-null location with one extra addref
        for(PRInt32 i = aLeadingFramesToTrim - 1; i >= 0; i--)
        {
            nsIJSStackFrameLocation* caller;
            if(NS_FAILED(location->GetCaller(&caller)) || !caller)
            {
                NS_ASSERTION(0, "tried to trim too many frames");
                NS_RELEASE(location);
                NS_RELEASE(e);
                return nsnull;
            }
            NS_ADDREF(caller);
            NS_RELEASE(location);
            location = caller;
        }
        // at this point we have non-null location with one extra addref
        rv = e->initialize(aMessage, aCode, location, aData);
        NS_RELEASE(location);
        if(NS_FAILED(rv))
            NS_RELEASE(e);
    }
    return e;        
}
