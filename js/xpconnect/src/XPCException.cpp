/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* An implementaion of nsIException. */

#include "xpcprivate.h"
#include "jsprf.h"
#include "nsError.h"
#include "nsIUnicodeDecoder.h"

/***************************************************************************/
/* Quick and dirty mapping of well known result codes to strings. We only
*  call this when building an exception object, so iterating the short array
*  is not too bad.
*
*  It sure would be nice to have exceptions declared in idl and available
*  in some more global way at runtime.
*/

static const struct ResultMap
{nsresult rv; const char* name; const char* format;} map[] = {
#define XPC_MSG_DEF(val, format) \
    {(val), #val, format},
#include "xpc.msg"
#undef XPC_MSG_DEF
    {NS_OK,0,0}   // sentinel to mark end of array
};

#define RESULT_COUNT ((sizeof(map) / sizeof(map[0]))-1)

// static
bool
nsXPCException::NameAndFormatForNSResult(nsresult rv,
                                         const char** name,
                                         const char** format)
{

    for (const ResultMap* p = map; p->name; p++) {
        if (rv == p->rv) {
            if (name) *name = p->name;
            if (format) *format = p->format;
            return true;
        }
    }
    return false;
}

// static
const void*
nsXPCException::IterateNSResults(nsresult* rv,
                                 const char** name,
                                 const char** format,
                                 const void** iterp)
{
    const ResultMap* p = (const ResultMap*) *iterp;
    if (!p)
        p = map;
    else
        p++;
    if (!p->name)
        p = nullptr;
    else {
        if (rv)
            *rv = p->rv;
        if (name)
            *name = p->name;
        if (format)
            *format = p->format;
    }
    *iterp = p;
    return p;
}

// static
uint32_t
nsXPCException::GetNSResultCount()
{
    return RESULT_COUNT;
}

/***************************************************************************/

NS_IMPL_CLASSINFO(nsXPCException, NULL, nsIClassInfo::DOM_OBJECT,
                  NS_XPCEXCEPTION_CID)
NS_INTERFACE_MAP_BEGIN(nsXPCException)
  NS_INTERFACE_MAP_ENTRY(nsIException)
  NS_INTERFACE_MAP_ENTRY(nsIXPCException)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIException)
  NS_IMPL_QUERY_CLASSINFO(nsXPCException)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_ADDREF(nsXPCException)
NS_IMPL_RELEASE(nsXPCException)

NS_IMPL_CI_INTERFACE_GETTER1(nsXPCException, nsIXPCException)

nsXPCException::nsXPCException()
    : mMessage(nullptr),
      mResult(NS_OK),
      mName(nullptr),
      mLocation(nullptr),
      mData(nullptr),
      mFilename(nullptr),
      mLineNumber(0),
      mInner(nullptr),
      mInitialized(false)
{
    MOZ_COUNT_CTOR(nsXPCException);
}

nsXPCException::~nsXPCException()
{
    MOZ_COUNT_DTOR(nsXPCException);
    Reset();
}

/* [noscript] xpcexJSVal stealJSVal (); */
NS_IMETHODIMP
nsXPCException::StealJSVal(jsval *vp)
{
    if (mThrownJSVal.IsHeld()) {
        *vp = mThrownJSVal.Release();
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

/* [noscript] void stowJSVal (in xpcexJSContextPtr cx, in xpcexJSVal val); */
NS_IMETHODIMP
nsXPCException::StowJSVal(JSContext* cx, jsval v)
{
    if (mThrownJSVal.Hold(cx)) {
        mThrownJSVal = v;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

void
nsXPCException::Reset()
{
    if (mMessage) {
        nsMemory::Free(mMessage);
        mMessage = nullptr;
    }
    if (mName) {
        nsMemory::Free(mName);
        mName = nullptr;
    }
    if (mFilename) {
        nsMemory::Free(mFilename);
        mFilename = nullptr;
    }
    mLineNumber = (uint32_t)-1;
    NS_IF_RELEASE(mLocation);
    NS_IF_RELEASE(mData);
    NS_IF_RELEASE(mInner);
}

/* readonly attribute string message; */
NS_IMETHODIMP
nsXPCException::GetMessageMoz(char * *aMessage)
{
    if (!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;
    XPC_STRING_GETTER_BODY(aMessage, mMessage);
}

/* readonly attribute nsresult result; */
NS_IMETHODIMP
nsXPCException::GetResult(nsresult *aResult)
{
    if (!aResult)
        return NS_ERROR_NULL_POINTER;
    if (!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;
    *aResult = mResult;
    return NS_OK;
}

/* readonly attribute string name; */
NS_IMETHODIMP
nsXPCException::GetName(char * *aName)
{
    if (!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;

    const char* name = mName;
    if (!name)
        NameAndFormatForNSResult(mResult, &name, nullptr);

    XPC_STRING_GETTER_BODY(aName, name);
}

/* readonly attribute string filename; */
NS_IMETHODIMP nsXPCException::GetFilename(char * *aFilename)
{
    if (!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;
    XPC_STRING_GETTER_BODY(aFilename, mFilename);
}

/* readonly attribute uint32_t lineNumber; */
NS_IMETHODIMP nsXPCException::GetLineNumber(uint32_t *aLineNumber)
{
    if (!aLineNumber)
        return NS_ERROR_NULL_POINTER;
    if (!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;
    *aLineNumber = mLineNumber;
    return NS_OK;
}

/* readonly attribute uint32_t columnNumber; */
NS_IMETHODIMP nsXPCException::GetColumnNumber(uint32_t *aColumnNumber)
{
    NS_ENSURE_ARG_POINTER(aColumnNumber);
    if (!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;
    *aColumnNumber = 0;
    return NS_OK;
}

/* readonly attribute nsIStackFrame location; */
NS_IMETHODIMP
nsXPCException::GetLocation(nsIStackFrame * *aLocation)
{
    if (!aLocation)
        return NS_ERROR_NULL_POINTER;
    if (!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;
    *aLocation = mLocation;
    NS_IF_ADDREF(mLocation);
    return NS_OK;
}

/* readonly attribute nsISupports data; */
NS_IMETHODIMP
nsXPCException::GetData(nsISupports * *aData)
{
    if (!aData)
        return NS_ERROR_NULL_POINTER;
    if (!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;
    *aData = mData;
    NS_IF_ADDREF(mData);
    return NS_OK;
}

/* readonly attribute nsIException inner; */
NS_IMETHODIMP
nsXPCException::GetInner(nsIException* *aException)
{
    if (!aException)
        return NS_ERROR_NULL_POINTER;
    if (!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;
    *aException = mInner;
    NS_IF_ADDREF(mInner);
    return NS_OK;
}

/* void initialize (in string aMessage, in nsresult aResult, in string aName, in nsIStackFrame aLocation, in nsISupports aData, in nsIException aInner); */
NS_IMETHODIMP
nsXPCException::Initialize(const char *aMessage, nsresult aResult, const char *aName, nsIStackFrame *aLocation, nsISupports *aData, nsIException *aInner)
{
    if (mInitialized)
        return NS_ERROR_ALREADY_INITIALIZED;

    Reset();

    if (aMessage) {
        if (!(mMessage = (char*) nsMemory::Clone(aMessage,
                                                 sizeof(char)*(strlen(aMessage)+1))))
            return NS_ERROR_OUT_OF_MEMORY;
    }

    if (aName) {
        if (!(mName = (char*) nsMemory::Clone(aName,
                                              sizeof(char)*(strlen(aName)+1))))
            return NS_ERROR_OUT_OF_MEMORY;
    }

    mResult = aResult;

    if (aLocation) {
        mLocation = aLocation;
        NS_ADDREF(mLocation);
        // For now, fill in our location details from our stack frame.
        // Later we may allow other locations?
        nsresult rc;
        if (NS_FAILED(rc = aLocation->GetFilename(&mFilename)))
            return rc;
        if (NS_FAILED(rc = aLocation->GetLineNumber(&mLineNumber)))
            return rc;
    } else {
        nsresult rv;
        nsXPConnect* xpc = nsXPConnect::XPConnect();
        rv = xpc->GetCurrentJSStack(&mLocation);
        if (NS_FAILED(rv))
            return rv;
    }

    if (aData) {
        mData = aData;
        NS_ADDREF(mData);
    }
    if (aInner) {
        mInner = aInner;
        NS_ADDREF(mInner);
    }

    mInitialized = true;
    return NS_OK;
}

/* string toString (); */
NS_IMETHODIMP
nsXPCException::ToString(char **_retval)
{
    if (!_retval)
        return NS_ERROR_NULL_POINTER;
    if (!mInitialized)
        return NS_ERROR_NOT_INITIALIZED;

    static const char defaultMsg[] = "<no message>";
    static const char defaultLocation[] = "<unknown>";
    static const char format[] =
 "[Exception... \"%s\"  nsresult: \"0x%x (%s)\"  location: \"%s\"  data: %s]";

    char* indicatedLocation = nullptr;

    if (mLocation) {
        // we need to free this if it does not fail
        nsresult rv = mLocation->ToString(&indicatedLocation);
        if (NS_FAILED(rv))
            return rv;
    }

    const char* msg = mMessage ? mMessage : nullptr;
    const char* location = indicatedLocation ?
                                indicatedLocation : defaultLocation;
    const char* resultName = mName;
    if (!resultName && !NameAndFormatForNSResult(mResult, &resultName,
                                                 (!msg) ? &msg : nullptr)) {
        if (!msg)
            msg = defaultMsg;
        resultName = "<unknown>";
    }
    const char* data = mData ? "yes" : "no";

    char* temp = JS_smprintf(format, msg, mResult, resultName, location, data);
    if (indicatedLocation)
        nsMemory::Free(indicatedLocation);

    char* final = nullptr;
    if (temp) {
        final = (char*) nsMemory::Clone(temp, sizeof(char)*(strlen(temp)+1));
        JS_smprintf_free(temp);
    }

    *_retval = final;
    return final ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

bool nsXPCException::sEverMadeOneFromFactory = false;

// static
nsresult
nsXPCException::NewException(const char *aMessage,
                             nsresult aResult,
                             nsIStackFrame *aLocation,
                             nsISupports *aData,
                             nsIException** exceptn)
{
    // A little hack... The nsIGenericModule nsIClassInfo scheme relies on there
    // having been at least one instance made via the factory. Otherwise, the
    // shared factory/classinsance object never gets created and our QI getter
    // for our instance's pointer to our nsIClassInfo will always return null.
    // This is bad because it means that wrapped exceptions will never have a
    // shared prototype. So... We force one to be created via the factory
    // *once* and then go about our business.
    if (!sEverMadeOneFromFactory) {
        nsCOMPtr<nsIXPCException> e =
            do_CreateInstance(XPC_EXCEPTION_CONTRACTID);
        sEverMadeOneFromFactory = true;
    }

    nsresult rv;
    nsXPCException* e = new nsXPCException();
    if (e) {
        NS_ADDREF(e);

        nsIStackFrame* location;
        if (aLocation) {
            location = aLocation;
            NS_ADDREF(location);
        } else {
            nsXPConnect* xpc = nsXPConnect::XPConnect();
            rv = xpc->GetCurrentJSStack(&location);
            if (NS_FAILED(rv)) {
                NS_RELEASE(e);
                return NS_ERROR_FAILURE;
            }
            // it is legal for there to be no active JS stack, if C++ code
            // is operating on a JS-implemented interface pointer without
            // having been called in turn by JS.  This happens in the JS
            // component loader, and will become more common as additional
            // components are implemented in JS.
        }
        // We want to trim off any leading native 'dataless' frames
        if (location)
            while (1) {
                uint32_t language;
                int32_t lineNumber;
                if (NS_FAILED(location->GetLanguage(&language)) ||
                    language == nsIProgrammingLanguage::JAVASCRIPT ||
                    NS_FAILED(location->GetLineNumber(&lineNumber)) ||
                    lineNumber) {
                    break;
                }
                nsCOMPtr<nsIStackFrame> caller;
                if (NS_FAILED(location->GetCaller(getter_AddRefs(caller))) || !caller)
                    break;
                NS_RELEASE(location);
                caller->QueryInterface(NS_GET_IID(nsIStackFrame), (void **)&location);
            }
        // at this point we have non-null location with one extra addref,
        // or no location at all
        rv = e->Initialize(aMessage, aResult, nullptr, location, aData, nullptr);
        NS_IF_RELEASE(location);
        if (NS_FAILED(rv))
            NS_RELEASE(e);
    }

    if (!e)
        return NS_ERROR_FAILURE;

    *exceptn = static_cast<nsIXPCException*>(e);
    return NS_OK;
}
