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

struct ResultMap {nsresult rv; const char* name; const char* format;} map[] = {
#define XPC_MSG_DEF(val, format) \
    {(val), #val, format},
#include "xpc.msg"
#undef MSG_DEF
    {0,0,0}   // sentinel to mark end of array
};

// static
JSBool
nsXPCException::NameAndFormatForNSResult(nsresult rv,
                                         const char** name,
                                         const char** format)
{

    for(ResultMap* p = map; p->name; p++)
    {
        if(rv == p->rv)
        {
            if(name) *name = p->name;
            if(format) *format = p->format;
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

// static
void*
nsXPCException::IterateNSResults(nsresult* rv,
                                 const char** name,
                                 const char** format,
                                 void** iterp)
{
    ResultMap* p = (ResultMap*) *iterp;
    if(!p)
        p = map;
    NS_ASSERTION(p->name, "iterated off the end of the array");
    if(rv)
        *rv = p->rv;
    if(name)
        *name = p->name;
    if(format)
        *format = p->format;
    p++;
    if(!p->name)
        p = nsnull;
    *iterp = p;
    return p;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS1(nsXPCException, nsIXPCException)

nsXPCException::nsXPCException()
    : mMessage(nsnull),
      mCode(0),
      mName(nsnull),
      mLocation(nsnull),
      mData(nsnull),
      mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsXPCException::~nsXPCException()
{
    Reset();
}

void
nsXPCException::Reset()
{
    if(mMessage)
    {
        nsAllocator::Free(mMessage);
        mMessage = nsnull;
    }
    if(mName)
    {
        nsAllocator::Free(mName);
        mName = nsnull;
    }
    NS_IF_RELEASE(mLocation);
    NS_IF_RELEASE(mData);
}

/* readonly attribute string message; */
NS_IMETHODIMP
nsXPCException::GetMessage(char * *aMessage)
{
    if(!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;
    XPC_STRING_GETTER_BODY(aMessage, mMessage);
}

/* readonly attribute nsresult code; */
NS_IMETHODIMP
nsXPCException::GetCode(nsresult *aCode)
{
    if(!aCode)
        return NS_ERROR_NULL_POINTER;
    if(!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;
    *aCode = mCode;
    return NS_OK;
}

/* readonly attribute string name; */
NS_IMETHODIMP
nsXPCException::GetName(char * *aName)
{
    if(!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;

    const char* name = mName;
    if(!name)
        NameAndFormatForNSResult(mCode, &name, nsnull);

    XPC_STRING_GETTER_BODY(aName, name);
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

/* void initialize (in string aMessage, in nsresult aCode, in string aName, in nsIJSStackFrameLocation aLocation, in nsISupports aData); */
NS_IMETHODIMP
nsXPCException::Initialize(const char *aMessage, nsresult aCode, const char *aName, nsIJSStackFrameLocation *aLocation, nsISupports *aData)
{
    if(mInitialized)
        return NS_ERROR_ALREADY_INITIALIZED;

    Reset();

    if(aMessage)
    {
        if(!(mMessage = (char*) nsAllocator::Clone(aMessage,
                                           sizeof(char)*(strlen(aMessage)+1))))
            return NS_ERROR_OUT_OF_MEMORY;
    }

    if(aName)
    {
        if(!(mName = (char*) nsAllocator::Clone(aName,
                                           sizeof(char)*(strlen(aName)+1))))
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
nsXPCException::ToString(char **_retval)
{
    if(!_retval)
        return NS_ERROR_NULL_POINTER;
    if(!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;

    static const char defaultMsg[] = "<no message>";
    static const char defaultLocation[] = "<unknown>";
    static const char format[] =
 "[Exception... \"%s\"  nsresult: \"0x%x (%s)\"  location: \"%s\"  data: %s]";

    char* indicatedLocation = nsnull;

    if(mLocation)
    {
        // we need to free this if it does not fail
        nsresult rv = mLocation->ToString(&indicatedLocation);
        if(NS_FAILED(rv))
            return rv;
    }

    const char* msg = mMessage ? mMessage : defaultMsg;
    const char* location = indicatedLocation ?
                                indicatedLocation : defaultLocation;
    const char* resultName = mName;
    if(!resultName && !NameAndFormatForNSResult(mCode, &resultName, nsnull))
        resultName = "<unknown>";
    const char* data = mData ? "yes" : "no";

    char* temp = JS_smprintf(format, msg, mCode, resultName, location, data);
    if(indicatedLocation)
        nsAllocator::Free(indicatedLocation);

    char* final = nsnull;
    if(temp)
    {
        final = (char*) nsAllocator::Clone(temp, sizeof(char)*(strlen(temp)+1));
        JS_smprintf_free(temp);
    }

    *_retval = final;
    return final ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


// static
nsXPCException*
nsXPCException::NewException(const char *aMessage,
                             nsresult aCode,
                             nsIJSStackFrameLocation *aLocation,
                             nsISupports *aData,
                             PRInt32 aLeadingFramesToTrim)
{
    nsresult rv;
    nsXPCException* e = new nsXPCException();
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
        rv = e->Initialize(aMessage, aCode, nsnull, location, aData);
        NS_RELEASE(location);
        if(NS_FAILED(rv))
            NS_RELEASE(e);
    }
    return e;
}

/***************************************************************************/
// Code for converting JSErrorReports to nsIXPCException with (optionally)
// attached nsIJSErrorReport.

NS_IMPL_ISUPPORTS1(xpcJSErrorReport, nsIJSErrorReport)

xpcJSErrorReport::xpcJSErrorReport()
    :   mMessage(nsnull),
        mFilename(nsnull),
        mLineno(0),
        mLinebuf(nsnull),
        mTokenIndex(0),
        mFlags(0),
        mErrorNumber(0)
{
    NS_INIT_ISUPPORTS();
}

xpcJSErrorReport::~xpcJSErrorReport()
{
    if(mMessage)  nsAllocator::Free(mMessage);
    if(mFilename) nsAllocator::Free(mFilename);
    if(mLinebuf)  nsAllocator::Free(mLinebuf);
}

// static
xpcJSErrorReport*
xpcJSErrorReport::NewReport(const char* aMessage,
                            const JSErrorReport* aReport)
{
    if(!aMessage || !aReport)
    {
        NS_ASSERTION(0,"invalid params - you should know better!");
        return nsnull;
    }
    xpcJSErrorReport* self = new xpcJSErrorReport();
    if(!self)
        return nsnull;
    NS_ADDREF(self);

    JSBool success = JS_TRUE;

    if(!(self->mMessage = (char*) nsAllocator::Clone(aMessage,
                                       sizeof(char)*(strlen(aMessage)+1))))
        success = JS_FALSE;

    if(success && aReport->filename)
       if(!(self->mFilename = (char*) nsAllocator::Clone(aReport->filename,
                                sizeof(char)*(strlen(aReport->filename)+1))))
        success = JS_FALSE;

    if(success && aReport->linebuf)
    {
       if(!(self->mLinebuf = (char*) nsAllocator::Clone(aReport->linebuf,
                                sizeof(char)*(strlen(aReport->linebuf)+1))))
        success = JS_FALSE;

        if(aReport->tokenptr)
            self->mTokenIndex = (PRUint32)(aReport->tokenptr - aReport->linebuf);
    }

    if(success)
    {
        self->mLineno       = (PRUint32) aReport->lineno;
        self->mFlags        = (PRUint32) aReport->flags;
        self->mErrorNumber  = (PRUint32) aReport->errorNumber;
    }

    if(!success)
        NS_RELEASE(self);
    return self;
}

/* readonly attribute string message; */
NS_IMETHODIMP
xpcJSErrorReport::GetMessage(char * *aMessage)
{
    XPC_STRING_GETTER_BODY(aMessage, mMessage);
}

/* readonly attribute string filename; */
NS_IMETHODIMP
xpcJSErrorReport::GetFilename(char * *aFilename)
{
    XPC_STRING_GETTER_BODY(aFilename, mFilename);
}

/* readonly attribute PRUint32 lineno; */
NS_IMETHODIMP
xpcJSErrorReport::GetLineno(PRUint32 *aLineno)
{
    NS_ENSURE_ARG_POINTER(aLineno);
    *aLineno = mLineno;
    return NS_OK;
}

/* readonly attribute string linebuf; */
NS_IMETHODIMP
xpcJSErrorReport::GetLinebuf(char * *aLinebuf)
{
    XPC_STRING_GETTER_BODY(aLinebuf, mLinebuf);
}

/* readonly attribute PRUint32 tokenIndex; */
NS_IMETHODIMP
xpcJSErrorReport::GetTokenIndex(PRUint32 *aTokenIndex)
{
    NS_ENSURE_ARG_POINTER(aTokenIndex);
    *aTokenIndex = mTokenIndex;
    return NS_OK;
}

/* readonly attribute PRUint32 flags; */
NS_IMETHODIMP
xpcJSErrorReport::GetFlags(PRUint32 *aFlags)
{
    NS_ENSURE_ARG_POINTER(aFlags);
    *aFlags = mFlags;
    return NS_OK;
}

/* readonly attribute PRUint32 errorNumber; */
NS_IMETHODIMP
xpcJSErrorReport::GetErrorNumber(PRUint32 *aErrorNumber)
{
    NS_ENSURE_ARG_POINTER(aErrorNumber);
    *aErrorNumber = mErrorNumber;
    return NS_OK;
}

/* readonly attribute PRBool warning; */
NS_IMETHODIMP
xpcJSErrorReport::GetWarning(PRBool *aWarning)
{
    NS_ENSURE_ARG_POINTER(aWarning);
    *aWarning = mFlags & JSREPORT_WARNING;
    return NS_OK;
}

/* readonly attribute PRBool error; */
NS_IMETHODIMP
xpcJSErrorReport::GetError(PRBool *aError)
{
    NS_ENSURE_ARG_POINTER(aError);
    *aError = !(mFlags & JSREPORT_WARNING);
    return NS_OK;
}

/* string toString (); */
NS_IMETHODIMP
xpcJSErrorReport::ToString(char **_retval)
{
    static const char format0[] =
        "[%s: \"%s\" {file: \"%s\" line: %d column: %d source: \"%s\"}]";
    static const char format1[] =
        "[%s: \"%s\" {file: \"%s\" line: %d}]";
    static const char format2[] =
        "[%s: \"%s\"]";

    static const char error[]   = "JS Error";
    static const char warning[] = "JS Warning";

    const char * severity = !(mFlags & JSREPORT_WARNING) ? error : warning;

    char* temp;

    if(mFilename && mLinebuf)
        temp = JS_smprintf(format0,
                           severity,
                           mMessage,
                           mFilename,
                           mLineno,
                           mTokenIndex,
                           mLinebuf);
    else if(mFilename)
        temp = JS_smprintf(format1,
                           severity,
                           mMessage,
                           mFilename,
                           mLineno);
    else
        temp = JS_smprintf(format2,
                           severity,
                           mMessage);

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

