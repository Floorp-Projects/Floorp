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
 * Copyright (C) 1999 Netscape Communications Corporation. All
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

static struct ResultMap 
{nsresult rv; const char* name; const char* format;} map[] = {
#define XPC_MSG_DEF(val, format) \
    {(val), #val, format},
#include "xpc.msg"
#undef XPC_MSG_DEF
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

#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
NS_IMPL_THREADSAFE_ISUPPORTS2(nsXPCException, 
                              nsIXPCException, 
                              nsISecurityCheckedComponent)
#else
NS_IMPL_THREADSAFE_ISUPPORTS1(nsXPCException, nsIXPCException)
#endif

nsXPCException::nsXPCException()
    : mMessage(nsnull),
      mResult(0),
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
        nsMemory::Free(mMessage);
        mMessage = nsnull;
    }
    if(mName)
    {
        nsMemory::Free(mName);
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

/* readonly attribute nsresult result; */
NS_IMETHODIMP
nsXPCException::GetResult(nsresult *aResult)
{
    if(!aResult)
        return NS_ERROR_NULL_POINTER;
    if(!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;
    *aResult = mResult;
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
        NameAndFormatForNSResult(mResult, &name, nsnull);

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

/* void initialize (in string aMessage, in nsresult aResult, in string aName, in nsIJSStackFrameLocation aLocation, in nsISupports aData); */
NS_IMETHODIMP
nsXPCException::Initialize(const char *aMessage, nsresult aResult, const char *aName, nsIJSStackFrameLocation *aLocation, nsISupports *aData)
{
    if(mInitialized)
        return NS_ERROR_ALREADY_INITIALIZED;

    Reset();

    if(aMessage)
    {
        if(!(mMessage = (char*) nsMemory::Clone(aMessage,
                                           sizeof(char)*(strlen(aMessage)+1))))
            return NS_ERROR_OUT_OF_MEMORY;
    }

    if(aName)
    {
        if(!(mName = (char*) nsMemory::Clone(aName,
                                           sizeof(char)*(strlen(aName)+1))))
            return NS_ERROR_OUT_OF_MEMORY;
    }

    mResult = aResult;

    if(aLocation)
    {
        mLocation = aLocation;
        NS_ADDREF(mLocation);
    }
    else
    {
        nsresult rv;
        nsXPConnect* xpc = nsXPConnect::GetXPConnect();
        if(!xpc)
            return NS_ERROR_FAILURE;
        rv = xpc->GetCurrentJSStack(&mLocation);
        NS_RELEASE(xpc);
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
    if(!resultName && !NameAndFormatForNSResult(mResult, &resultName, nsnull))
        resultName = "<unknown>";
    const char* data = mData ? "yes" : "no";

    char* temp = JS_smprintf(format, msg, mResult, resultName, location, data);
    if(indicatedLocation)
        nsMemory::Free(indicatedLocation);

    char* final = nsnull;
    if(temp)
    {
        final = (char*) nsMemory::Clone(temp, sizeof(char)*(strlen(temp)+1));
        JS_smprintf_free(temp);
    }

    *_retval = final;
    return final ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


// static
nsXPCException*
nsXPCException::NewException(const char *aMessage,
                             nsresult aResult,
                             nsIJSStackFrameLocation *aLocation,
                             nsISupports *aData)
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
            nsXPConnect* xpc = nsXPConnect::GetXPConnect();
            if(!xpc)
            {
                NS_RELEASE(e);
                return nsnull;
            }
            rv = xpc->GetCurrentJSStack(&location);
            NS_RELEASE(xpc);
            if(NS_FAILED(rv))
            {
                NS_RELEASE(e);
                return nsnull;
            }
            // it is legal for there to be no active JS stack, if C++ code
            // is operating on a JS-implemented interface pointer without
            // having been called in turn by JS.  This happens in the JS
            // component loader, and will become more common as additional
            // components are implemented in JS.
        }
        // We want to trim off any leading native 'dataless' frames
        if (location)
            while(1) {
                PRBool  isJSFrame;
                PRInt32 lineNumber;
                if(NS_FAILED(location->GetIsJSFrame(&isJSFrame)) || isJSFrame ||
                   NS_FAILED(location->GetLineNumber(&lineNumber)) || lineNumber)
                    break;
                nsIJSStackFrameLocation* caller;
                if(NS_FAILED(location->GetCaller(&caller)) || !caller)
                    break;
                NS_RELEASE(location);
                location = caller;
            }
        // at this point we have non-null location with one extra addref,
        // or no location at all
        rv = e->Initialize(aMessage, aResult, nsnull, location, aData);
        NS_IF_RELEASE(location);
        if(NS_FAILED(rv))
            NS_RELEASE(e);
    }
    return e;
}

#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
static char* CloneAllAccess()
{
    static const char allAccess[] = "AllAccess";
    return (char*)nsMemory::Clone(allAccess, sizeof(allAccess));        
}

/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP 
nsXPCException::CanCreateWrapper(const nsIID * iid, char **_retval)
{
    // If you have to ask, then the answer is NO
    *_retval = nsnull;
    return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP 
nsXPCException::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
    static const NS_NAMED_LITERAL_STRING(s_toString, "toString");
    
    const nsLiteralString name(methodName);

    if(name.Equals(s_toString))
        *_retval = CloneAllAccess();
    else
        *_retval = nsnull;
    return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsXPCException::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
    static const NS_NAMED_LITERAL_STRING(s_message, "message");
    static const NS_NAMED_LITERAL_STRING(s_result , "result");
    static const NS_NAMED_LITERAL_STRING(s_name   , "name");

    const nsLiteralString name(propertyName);

    if(name.Equals(s_message) ||
       name.Equals(s_result)  ||
       name.Equals(s_name))
        *_retval = CloneAllAccess();
    else
        *_retval = nsnull;
    return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsXPCException::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
    // If you have to ask, then the answer is NO
    *_retval = nsnull;
    return NS_OK;
}
#endif
