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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* An xpcom implementation of the JavaScript nsIID and nsCID objects. */

#include "xpcprivate.h"

/***************************************************************************/
// stuff used by all

static void ThrowException(uintN errNum, JSContext* cx)
{
    nsXPConnect::GetJSThrower()->ThrowException(errNum, cx);
}

static void ThrowBadResultException(uintN errNum, JSContext* cx, nsresult rv)
{
    nsXPConnect::GetJSThrower()->ThrowBadResultException(errNum, cx, nsnull, 
                                                         nsnull, rv);
}

/***************************************************************************/
// nsJSID

NS_IMPL_ISUPPORTS(nsJSID, NS_GET_IID(nsIJSID))

char nsJSID::gNoString[] = "";

nsJSID::nsJSID()
    : mID(GetInvalidIID()), mNumber(gNoString), mName(gNoString)
{
    NS_INIT_ISUPPORTS();    
};

nsJSID::~nsJSID()
{
    if(mNumber && mNumber != gNoString)
        delete [] mNumber;
    if(mName && mName != gNoString)
        delete [] mName;
}

void nsJSID::reset()
{
    mID = GetInvalidIID();

    if(mNumber && mNumber != gNoString)
        delete [] mNumber;
    if(mName && mName != gNoString)
        delete [] mName;

    mNumber = mName = NULL;
}

PRBool
nsJSID::setName(const char* name)
{
    NS_ASSERTION(!mName || mName == gNoString ,"name already set");
    NS_ASSERTION(name,"null name");
    int len = strlen(name)+1;
    mName = new char[len];
    if(!mName)
        return PR_FALSE;
    memcpy(mName, name, len);
    return PR_TRUE;
}

NS_IMETHODIMP
nsJSID::GetName(char * *aName)
{
    if(!nameIsSet())
        setNameToNoString();
    NS_ASSERTION(mName, "name not set");
    *aName = (char*) nsAllocator::Clone(mName, strlen(mName)+1);
    return *aName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJSID::GetNumber(char * *aNumber)
{
    if(!mNumber)
    {
        if(!(mNumber = mID.ToString()))
            mNumber = gNoString;
    }

    *aNumber = (char*) nsAllocator::Clone(mNumber, strlen(mNumber)+1);
    return *aNumber ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJSID::GetId(nsID* *aId)
{
    *aId = (nsID*) nsAllocator::Clone(&mID, sizeof(nsID));
    return *aId ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJSID::GetValid(PRBool *aValid)
{
    *aValid = !mID.Equals(GetInvalidIID());
    return NS_OK;
}

NS_IMETHODIMP
nsJSID::equals(nsIJSID *other, PRBool *_retval)
{
    if(!_retval)
        return NS_ERROR_NULL_POINTER;

    *_retval = PR_FALSE;

    if(mID.Equals(GetInvalidIID()))
        return NS_OK;

    nsID* otherID;
    if(NS_SUCCEEDED(other->GetId(&otherID)))
    {
        *_retval = mID.Equals(*otherID);
        nsAllocator::Free(otherID);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsJSID::initialize(const char *idString)
{
    if(!idString)
        return NS_ERROR_NULL_POINTER;

    PRBool success = PR_FALSE;

    if(strlen(idString) && mID.Equals(GetInvalidIID()))
    {
        reset();

        if(idString[0] == '{')
        {
            nsID id;
            if(id.Parse((char*)idString))
            {
                mID = id;
                success = PR_TRUE;
            }
        }
    }
    return success ? NS_OK : NS_ERROR_FAILURE;
}

PRBool 
nsJSID::initWithName(const nsID& id, const char *nameString)
{
    NS_ASSERTION(nameString, "no name");
    reset();
    mID = id;
    return setName(nameString);
}        

// try to use the name, if no name, then use the number
NS_IMETHODIMP
nsJSID::toString(char **_retval)
{
    if(mName != gNoString)
    {
        char* str;
        if(NS_SUCCEEDED(GetName(&str)))
        {
            if(mName != gNoString)
            {
                *_retval = str;
                return NS_OK;                            
            }
            else
                nsAllocator::Free(str);
        }
    }
    return GetNumber(_retval);
}

const nsID& 
nsJSID::GetInvalidIID() const
{
    // {BB1F47B0-D137-11d2-9841-006008962422}
    static nsID invalid = {0xbb1f47b0, 0xd137, 0x11d2,
                            {0x98, 0x41, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22}};
    return invalid;
}

//static
nsJSID*
nsJSID::NewID(const char* str)
{
    if(!str)
    {
        NS_ASSERTION(0,"no string");
        return nsnull;    
    }

    nsJSID* idObj = new nsJSID();
    if(idObj)
    {
        NS_ADDREF(idObj);
        if(NS_FAILED(idObj->initialize(str)))
            NS_RELEASE(idObj);
    }
    return idObj;
}

/***************************************************************************/
/***************************************************************************/

NS_IMETHODIMP
nsJSIID::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsISupports)) ||
      aIID.Equals(NS_GET_IID(nsIJSID)) ||
      aIID.Equals(NS_GET_IID(nsIJSIID))) {
    *aInstancePtr = (void*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  *aInstancePtr = NULL;
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsJSIID)
NS_IMPL_RELEASE(nsJSIID)

nsJSIID::nsJSIID()  {NS_INIT_ISUPPORTS();}
nsJSIID::~nsJSIID() {}

NS_IMETHODIMP nsJSIID::GetName(char * *aName)
    {resolveName(); return mDetails.GetName(aName);}

NS_IMETHODIMP nsJSIID::GetNumber(char * *aNumber)
    {return mDetails.GetNumber(aNumber);}

NS_IMETHODIMP nsJSIID::GetId(nsID* *aId)
    {return mDetails.GetId(aId);}

NS_IMETHODIMP nsJSIID::GetValid(PRBool *aValid)
    {return mDetails.GetValid(aValid);}

NS_IMETHODIMP nsJSIID::equals(nsIJSID *other, PRBool *_retval)
    {return mDetails.equals(other, _retval);}

NS_IMETHODIMP nsJSIID::initialize(const char *idString)
    {return mDetails.initialize(idString);}

NS_IMETHODIMP nsJSIID::toString(char **_retval)
    {resolveName(); return mDetails.toString(_retval);}

void 
nsJSIID::resolveName()
{
    if(!mDetails.nameIsSet())
    {
        nsIInterfaceInfoManager* iim;
        if(NULL != (iim = nsXPConnect::GetInterfaceInfoManager()))
        {
            char* name;
            if(NS_SUCCEEDED(iim->GetNameForIID(mDetails.getID(), &name)) && name)
            {
                mDetails.setName(name);
                nsAllocator::Free(name);
            }
            NS_RELEASE(iim);
        }
        if(!mDetails.nameIsSet())
            mDetails.setNameToNoString();
    }
}        

//static
nsJSIID*
nsJSIID::NewID(const char* str)
{
    if(!str)
    {
        NS_ASSERTION(0,"no string");
        return nsnull;    
    }

    nsJSIID* idObj = new nsJSIID();
    if(idObj)
    {
        PRBool success = PR_FALSE;
        NS_ADDREF(idObj);

        if(str[0] == '{')
        {
            if(NS_SUCCEEDED(idObj->initialize(str)))
                success = PR_TRUE;
        }
        else
        {
            nsIInterfaceInfoManager* iim;
            if(NULL != (iim = nsXPConnect::GetInterfaceInfoManager()))
            {
                nsID* pid;
                if(NS_SUCCEEDED(iim->GetIIDForName(str, &pid)) && pid)
                {
                    success = idObj->mDetails.initWithName(*pid, str);
                    nsAllocator::Free(pid);
                }
                NS_RELEASE(iim);
            }
        }
        if(!success)
            NS_RELEASE(idObj);
    }
    return idObj;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/*
* nsJSCID supports 'createInstance' and 'getService'. These are implemented
* using the (somewhat complex) nsIXPCScriptable scheme so that when called
* they can do security checks (knows in which JSContect is calling) and also
* so that 'getService' can add a fancy FinalizeListener to follow the
* ServiceManager's protocol for releasing a service.
*/

// {16A43B00-F116-11d2-985A-006008962422}
#define NS_CIDCREATEINSTANCE_IID \
{ 0x16a43b00, 0xf116, 0x11d2, \
    { 0x98, 0x5a, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }

class CIDCreateInstance : public nsIXPCScriptable
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_CIDCREATEINSTANCE_IID)
    NS_DECL_ISUPPORTS
    XPC_DECLARE_IXPCSCRIPTABLE

    CIDCreateInstance(nsJSCID* aCID);
    virtual ~CIDCreateInstance();
    nsJSCID* GetCID() const {return mCID;}

private:
    CIDCreateInstance(); // not implemented

private:
    nsJSCID* mCID;
};

/*********************************************/

NS_IMPL_QUERY_INTERFACE_SCRIPTABLE(CIDCreateInstance, \
                                   NS_GET_IID(CIDCreateInstance), \
                                   this)
NS_IMPL_ADDREF(CIDCreateInstance)
NS_IMPL_RELEASE(CIDCreateInstance)

CIDCreateInstance::CIDCreateInstance(nsJSCID *aCID)
    : mCID(aCID)
{
    NS_PRECONDITION(mCID, "bad cid");
    NS_INIT_ISUPPORTS();
    NS_IF_ADDREF(mCID);
}        

CIDCreateInstance::~CIDCreateInstance()
{
    NS_IF_RELEASE(mCID);
}

XPC_IMPLEMENT_FORWARD_CREATE(CIDCreateInstance);
XPC_IMPLEMENT_IGNORE_GETFLAGS(CIDCreateInstance);
XPC_IMPLEMENT_IGNORE_LOOKUPPROPERTY(CIDCreateInstance);
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(CIDCreateInstance);
XPC_IMPLEMENT_IGNORE_GETPROPERTY(CIDCreateInstance);
XPC_IMPLEMENT_IGNORE_SETPROPERTY(CIDCreateInstance);
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(CIDCreateInstance);
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(CIDCreateInstance);
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(CIDCreateInstance);
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(CIDCreateInstance);
XPC_IMPLEMENT_FORWARD_ENUMERATE(CIDCreateInstance);
XPC_IMPLEMENT_FORWARD_CHECKACCESS(CIDCreateInstance);
// XPC_IMPLEMENT_IGNORE_CALL(CIDCreateInstance);
XPC_IMPLEMENT_IGNORE_CONSTRUCT(CIDCreateInstance);
XPC_IMPLEMENT_FORWARD_HASINSTANCE(CIDCreateInstance);
XPC_IMPLEMENT_FORWARD_FINALIZE(CIDCreateInstance);

NS_IMETHODIMP
CIDCreateInstance::Call(JSContext *cx, JSObject *obj,
                        uintN argc, jsval *argv,
                        jsval *rval,
                        nsIXPConnectWrappedNative* wrapper,
                        nsIXPCScriptable* arbitrary,
                        JSBool* retval)
{
    CIDCreateInstance* self;
    nsJSCID* cidObj;
    nsCID*  cid;
    PRBool valid;

    if(NS_FAILED(wrapper->GetNative((nsISupports**)&self)))
        return NS_ERROR_FAILURE;

    if(!(cidObj = self->GetCID()) ||
       NS_FAILED(cidObj->GetValid(&valid)) ||
       !valid ||
       NS_FAILED(cidObj->GetId(&cid)) ||
       !cid)
    {
        ThrowException(XPCJSError::BAD_CID, cx);
        *retval = JS_FALSE;
        return NS_OK;
    }

    // Do the security check if necessary

    XPCContext* xpcc;
    if(NULL != (xpcc = nsXPConnect::GetContext(cx)))
    {
        nsIXPCSecurityManager* sm;
        if(NULL != (sm = xpcc->GetSecurityManager()) &&
           (xpcc->GetSecurityManagerFlags() & 
            nsIXPCSecurityManager::HOOK_CREATE_INSTANCE) &&
           NS_OK != sm->CanCreateInstance(cx, *cid))
        {
            // the security manager vetoed. It should have set an exception.
            nsAllocator::Free(cid);
            *rval = JSVAL_NULL;
            *retval = JS_TRUE;
            return NS_OK;
        }
    }

    // If an IID was passed in then use it
    // XXX should it be JS error to pass something that is *not* and JSID?
    const nsID* piid = NULL;
    if(argc)
    {
        JSObject* iidobj;
        jsval val = *argv;
        if(!JSVAL_IS_VOID(val) && !JSVAL_IS_NULL(val) &&
            JSVAL_IS_OBJECT(val) && NULL != (iidobj = JSVAL_TO_OBJECT(val)))
        {
            piid = xpc_JSObjectToID(cx, iidobj);
        }
    }
    if(!piid)
        piid = &(nsCOMTypeInfo<nsISupports>::GetIID());

    nsISupports* inst;
    nsresult rv;

    rv = nsComponentManager::CreateInstance(*cid, NULL, *piid, (void**) &inst);
    NS_ASSERTION(NS_FAILED(rv) || inst, "component manager returned success, but instance is null!");
    nsAllocator::Free(cid);

    if(NS_FAILED(rv) || !inst)
    {
        ThrowBadResultException(XPCJSError::CI_RETURNED_FAILURE, cx, rv);
        *retval = JS_FALSE;
        return NS_OK;
    }

    nsIXPConnectWrappedNative* instWrapper = NULL;

    nsIXPConnect* xpc = nsXPConnect::GetXPConnect();
    if(xpc)
    {
        rv = xpc->WrapNative(cx, inst, *piid, &instWrapper);
        NS_RELEASE(xpc);
    }

    NS_RELEASE(inst);
    if(NS_FAILED(rv) || !instWrapper)
    {
        ThrowException(XPCJSError::CANT_CREATE_WN, cx);
        *retval = JS_FALSE;
        return NS_OK;
    }

    JSObject* instJSObj;
    instWrapper->GetJSObject(&instJSObj);
    *rval = OBJECT_TO_JSVAL(instJSObj);
    *retval = JS_TRUE;

    NS_RELEASE(instWrapper);
    return NS_OK;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

// {23423AA0-F142-11d2-985A-006008962422}
#define NS_SERVICE_RELEASER_IID \
{ 0x23423aa0, 0xf142, 0x11d2, \
  { 0x98, 0x5a, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }

class ServiceReleaser : public nsIXPConnectFinalizeListener
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_SERVICE_RELEASER_IID)
    NS_DECL_ISUPPORTS
    NS_IMETHOD AboutToRelease(nsISupports* aObj);
    ServiceReleaser(const nsCID& aCID);

private:
    ServiceReleaser(); // not implemented
    virtual ~ServiceReleaser();
    nsCID mCID;
};

NS_IMPL_ISUPPORTS(ServiceReleaser, NS_GET_IID(ServiceReleaser));

ServiceReleaser::ServiceReleaser(const nsCID& aCID)
    : mCID(aCID)
{
    NS_INIT_ISUPPORTS();
}        

ServiceReleaser::~ServiceReleaser() {}

NS_IMETHODIMP
ServiceReleaser::AboutToRelease(nsISupports* aObj)
{
    return nsServiceManager::ReleaseService(mCID, aObj, NULL);
}

/*********************************************/

// {C46BC320-F13E-11d2-985A-006008962422}
#define NS_CIDGETSERVICE_IID \
{ 0xc46bc320, 0xf13e, 0x11d2, \
  { 0x98, 0x5a, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }

class CIDGetService : public nsIXPCScriptable
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_CIDGETSERVICE_IID)
    NS_DECL_ISUPPORTS
    XPC_DECLARE_IXPCSCRIPTABLE

    CIDGetService(nsJSCID* aCID);
    virtual ~CIDGetService();
    nsJSCID* GetCID() const {return mCID;}

private:
    CIDGetService(); // not implemented

private:
    nsJSCID* mCID;
};

/*********************************************/

NS_IMPL_QUERY_INTERFACE_SCRIPTABLE(CIDGetService, \
                                   NS_GET_IID(CIDGetService), \
                                   this)
NS_IMPL_ADDREF(CIDGetService)
NS_IMPL_RELEASE(CIDGetService)

CIDGetService::CIDGetService(nsJSCID *aCID)
    : mCID(aCID)
{
    NS_PRECONDITION(mCID, "bad cid");
    NS_INIT_ISUPPORTS();
    NS_IF_ADDREF(mCID);
}        

CIDGetService::~CIDGetService()
{
    NS_IF_RELEASE(mCID);
}

XPC_IMPLEMENT_FORWARD_CREATE(CIDGetService);
XPC_IMPLEMENT_IGNORE_GETFLAGS(CIDGetService);
XPC_IMPLEMENT_IGNORE_LOOKUPPROPERTY(CIDGetService);
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(CIDGetService);
XPC_IMPLEMENT_IGNORE_GETPROPERTY(CIDGetService);
XPC_IMPLEMENT_IGNORE_SETPROPERTY(CIDGetService);
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(CIDGetService);
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(CIDGetService);
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(CIDGetService);
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(CIDGetService);
XPC_IMPLEMENT_FORWARD_ENUMERATE(CIDGetService);
XPC_IMPLEMENT_FORWARD_CHECKACCESS(CIDGetService);
// XPC_IMPLEMENT_IGNORE_CALL(CIDGetService);
XPC_IMPLEMENT_IGNORE_CONSTRUCT(CIDGetService);
XPC_IMPLEMENT_FORWARD_HASINSTANCE(CIDGetService);
XPC_IMPLEMENT_FORWARD_FINALIZE(CIDGetService);

NS_IMETHODIMP
CIDGetService::Call(JSContext *cx, JSObject *obj,
                    uintN argc, jsval *argv,
                    jsval *rval,
                    nsIXPConnectWrappedNative* wrapper,
                    nsIXPCScriptable* arbitrary,
                    JSBool* retval)
{
    CIDGetService* self;
    nsJSCID* cidObj;
    nsCID*  cid;
    PRBool valid;

    if(NS_FAILED(wrapper->GetNative((nsISupports**)&self)))
        return NS_ERROR_FAILURE;

    if(!(cidObj = self->GetCID()) ||
       NS_FAILED(cidObj->GetValid(&valid)) ||
       !valid ||
       NS_FAILED(cidObj->GetId(&cid)) ||
       !cid)
    {
        ThrowException(XPCJSError::BAD_CID, cx);
        *retval = JS_FALSE;
        return NS_OK;
    }

    // Do the security check if necessary

    XPCContext* xpcc;
    if(NULL != (xpcc = nsXPConnect::GetContext(cx)))
    {
        nsIXPCSecurityManager* sm;
        if(NULL != (sm = xpcc->GetSecurityManager()) &&
           (xpcc->GetSecurityManagerFlags() & 
            nsIXPCSecurityManager::HOOK_GET_SERVICE) &&
           NS_OK != sm->CanGetService(cx, *cid))
        {
            // the security manager vetoed. It should have set an exception.
            nsAllocator::Free(cid);
            *rval = JSVAL_NULL;
            *retval = JS_TRUE;
            return NS_OK;
        }
    }

    // If an IID was passed in then use it
    // XXX should it be JS error to pass something that is *not* and JSID?
    const nsID* piid = NULL;
    if(argc)
    {
        JSObject* iidobj;
        jsval val = *argv;
        if(!JSVAL_IS_VOID(val) && !JSVAL_IS_NULL(val) &&
            JSVAL_IS_OBJECT(val) && NULL != (iidobj = JSVAL_TO_OBJECT(val)))
        {
            piid = xpc_JSObjectToID(cx, iidobj);
        }
    }
    if(!piid)
        piid = &(nsCOMTypeInfo<nsISupports>::GetIID());

    nsISupports* srvc;
    nsresult rv;

    rv = nsServiceManager::GetService(*cid, *piid, &srvc, NULL);
    NS_ASSERTION(NS_FAILED(rv) || srvc, "service manager returned success, but service is null!");

    if(NS_FAILED(rv) || !srvc)
    {
        ThrowBadResultException(XPCJSError::GS_RETURNED_FAILURE, cx, rv);
        *retval = JS_FALSE;
        return NS_OK;
    }

    nsIXPConnectWrappedNative* srvcWrapper = NULL;

    nsIXPConnect* xpc = nsXPConnect::GetXPConnect();
    if(xpc)
    {
        rv = xpc->WrapNative(cx, srvc, *piid, &srvcWrapper);
        NS_RELEASE(xpc);
    }

    if(NS_FAILED(rv) || !srvcWrapper)
    {
        nsServiceManager::ReleaseService(*cid, srvc, NULL);
        nsAllocator::Free(cid);
        ThrowException(XPCJSError::CANT_CREATE_WN, cx);
        *retval = JS_FALSE;
        return NS_OK;
    }

    // This will eventually release the reference we got from
    // nsServiceManager::GetService
    ServiceReleaser* releaser = new ServiceReleaser(*cid);
    if(releaser)
    {
        NS_ADDREF(releaser);
        if(NS_FAILED(srvcWrapper->SetFinalizeListener(releaser)))
        {
            // Failure means that we are using a preexisting wrapper on
            // this service that has already setup a listener. So, we just
            // release our extra ref and trust the lister that is already in 
            // place to do the right thing.
            NS_RELEASE(srvc);
            NS_RELEASE(releaser);
        }
    }
    nsAllocator::Free(cid);

    JSObject* srvcJSObj;
    srvcWrapper->GetJSObject(&srvcJSObj);
    *rval = OBJECT_TO_JSVAL(srvcJSObj);
    *retval = JS_TRUE;
    NS_RELEASE(srvcWrapper);

    return NS_OK;
}

/***************************************************************************/
/***************************************************************************/

NS_IMETHODIMP
nsJSCID::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsISupports)) ||
      aIID.Equals(NS_GET_IID(nsIJSID)) ||
      aIID.Equals(NS_GET_IID(nsIJSCID))) {
    *aInstancePtr = (void*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  *aInstancePtr = NULL;
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsJSCID)
NS_IMPL_RELEASE(nsJSCID)

nsJSCID::nsJSCID()  {NS_INIT_ISUPPORTS();}
nsJSCID::~nsJSCID() {}

NS_IMETHODIMP nsJSCID::GetName(char * *aName)
    {resolveName(); return mDetails.GetName(aName);}

NS_IMETHODIMP nsJSCID::GetNumber(char * *aNumber)
    {return mDetails.GetNumber(aNumber);}

NS_IMETHODIMP nsJSCID::GetId(nsID* *aId)
    {return mDetails.GetId(aId);}

NS_IMETHODIMP nsJSCID::GetValid(PRBool *aValid)
    {return mDetails.GetValid(aValid);}

NS_IMETHODIMP nsJSCID::equals(nsIJSID *other, PRBool *_retval)
    {return mDetails.equals(other, _retval);}

NS_IMETHODIMP nsJSCID::initialize(const char *idString)
    {return mDetails.initialize(idString);}

NS_IMETHODIMP nsJSCID::toString(char **_retval)
    {resolveName(); return mDetails.toString(_retval);}

void 
nsJSCID::resolveName()
{
    if(!mDetails.nameIsSet())
        mDetails.setNameToNoString();
}

//static
nsJSCID*
nsJSCID::NewID(const char* str)
{
    if(!str)
    {
        NS_ASSERTION(0,"no string");
        return nsnull;    
    }

    nsJSCID* idObj = new nsJSCID();
    if(idObj)
    {
        PRBool success = PR_FALSE;
        NS_ADDREF(idObj);

        if(str[0] == '{')
        {
            if(NS_SUCCEEDED(idObj->initialize(str)))
                success = PR_TRUE;
        }
        else
        {
            nsCID cid;
            if(NS_SUCCEEDED(nsComponentManager::ProgIDToCLSID(str, &cid)))
            {
                success = idObj->mDetails.initWithName(cid, str);
            }
        }
        if(!success)
            NS_RELEASE(idObj);
    }
    return idObj;
}

/* readonly attribute nsISupports createInstance; */
NS_IMETHODIMP
nsJSCID::GetCreateInstance(nsISupports * *aCreateInstance)
{
    if(!aCreateInstance)
        return NS_ERROR_NULL_POINTER;

    *aCreateInstance = new CIDCreateInstance(this);
    if(*aCreateInstance)
        NS_ADDREF(*aCreateInstance);

    return NS_OK;
}

/* readonly attribute nsISupports getService; */
NS_IMETHODIMP
nsJSCID::GetGetService(nsISupports * *aGetService)
{
    if(!aGetService)
        return NS_ERROR_NULL_POINTER;

    *aGetService = new CIDGetService(this);
    if(*aGetService)
        NS_ADDREF(*aGetService);
    return NS_OK;
}

/***************************************************************************/
// additional utilities...

JSObject *
xpc_NewIDObject(JSContext *cx, const nsID& aID)
{
    JSObject *obj = NULL;

    char* idString = aID.ToString();
    if(idString)
    {
        nsJSID* iid = nsJSID::NewID(idString);
        delete [] idString;
        if(iid)
        {
            nsXPConnect* xpc = nsXPConnect::GetXPConnect();
            if(xpc)
            {
                nsIXPConnectWrappedNative* nsid_wrapper;
                if(NS_SUCCEEDED(xpc->WrapNative(cx, 
                                        NS_STATIC_CAST(nsISupports*,iid),
                                        NS_GET_IID(nsIJSID),
                                        &nsid_wrapper)))
                {
                    nsid_wrapper->GetJSObject(&obj);
                    NS_RELEASE(nsid_wrapper);
                }
                NS_RELEASE(xpc);
            }
            NS_RELEASE(iid);
        }
    }
    return obj;
}

nsID*
xpc_JSObjectToID(JSContext *cx, JSObject* obj)
{
    nsID* id = NULL;
    if(!cx || !obj)
        return NULL;

    // NOTE: this call does NOT addref
    nsXPCWrappedNative* wrapper =
        nsXPCWrappedNativeClass::GetWrappedNativeOfJSObject(cx, obj);
    if(wrapper)
    {
        if(wrapper->GetIID().Equals(nsIJSID::GetIID())  ||
           wrapper->GetIID().Equals(nsIJSIID::GetIID()) ||
           wrapper->GetIID().Equals(nsIJSCID::GetIID()))
        {
            ((nsIJSID*)wrapper->GetNative())->GetId(&id);
        }
    }
    // XXX it would be nice to try to construct one from an object that can be
    // converted into a string.
    return id;
}

