/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* An xpcom implmentation of the JavaScript nsIID and nsCID objects. */

#include "xpcprivate.h"

/***************************************************************************/
// stuff used for both classes...

static const nsID& GetInvalidIID()
{
    // {BB1F47B0-D137-11d2-9841-006008962422}
    static nsID invalid = {0xbb1f47b0, 0xd137, 0x11d2,
                            {0x98, 0x41, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22}};
    return invalid;
}

static char* gNoString = "";

/***************************************************************************/
// nsJSIID

nsresult
nsJSIID::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsISupports::GetIID()) ||
      aIID.Equals(nsIJSID::GetIID()) ||
      aIID.Equals(nsIJSIID::GetIID())) {
    *aInstancePtr = (void*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  *aInstancePtr = NULL;
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsJSIID)
NS_IMPL_RELEASE(nsJSIID)

nsJSIID::nsJSIID()
    : mID(GetInvalidIID()), mNumber(gNoString), mName(gNoString)
{
    NS_INIT_ISUPPORTS();
    NS_ADDREF_THIS();
};

nsJSIID::~nsJSIID()
{
    if(mNumber && mNumber != gNoString)
        delete [] mNumber;
    if(mName && mName != gNoString)
        delete [] mName;
}

void nsJSIID::reset()
{
    mID = GetInvalidIID();

    if(mNumber && mNumber != gNoString)
        delete [] mNumber;
    if(mName && mName != gNoString)
        delete [] mName;

    mNumber = mName = NULL;
}

//static
nsJSIID*
nsJSIID::NewID(const char* str)
{
    PRBool success;
    nsJSIID* idObj = new nsJSIID();
    if(!idObj)
        return NULL;

    if(NS_FAILED(idObj->init(str, &success)) || !success)
    {
        delete idObj;
        return NULL;
    }

    return idObj;
}

void
nsJSIID::setName(const char* name)
{
    int len = strlen(name)+1;
    mName = new char[len];
    if(mName)
        memcpy(mName, name, len);
}

NS_IMETHODIMP
nsJSIID::GetName(char * *aName)
{
    if(!mName)
    {
        nsIInterfaceInfoManager* iim;
        if(NULL != (iim = nsXPConnect::GetInterfaceInfoManager()))
        {
            char* name;
            if(NS_SUCCEEDED(iim->GetNameForIID(&mID, &name)) && name)
            {
                setName(name);
                nsAllocator::Free(name);
            }
            NS_RELEASE(iim);
        }
        if(!mName)
            mName = gNoString;
    }

    *aName = (char*) nsAllocator::Clone(mName, strlen(mName)+1);
    return *aName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJSIID::GetNumber(char * *aNumber)
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
nsJSIID::GetId(nsID* *aId)
{
    *aId = (nsID*) nsAllocator::Clone(&mID, sizeof(nsID));
    return *aId ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJSIID::GetValid(PRBool *aValid)
{
    *aValid = !mID.Equals(GetInvalidIID());
    return NS_OK;
}

NS_IMETHODIMP
nsJSIID::equals(nsIJSID *other, PRBool *_retval)
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
nsJSIID::init(const char *idString, PRBool *_retval)
{
    if(!_retval || !idString)
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
        else
        {
            nsIInterfaceInfoManager* iim;
            if(NULL != (iim = nsXPConnect::GetInterfaceInfoManager()))
            {
                nsID* pid;
                if(NS_SUCCEEDED(iim->GetIIDForName(idString, &pid)) && pid)
                {
                    mID = *pid;
                    setName(idString);
                    success = PR_TRUE;
                    nsAllocator::Free(pid);
                }
                NS_RELEASE(iim);
            }
        }
    }

    *_retval = success;
    return NS_OK;
}

NS_IMETHODIMP
nsJSIID::toString(char **_retval)
{
    return GetName(_retval);
}

/***************************************************************************/
/***************************************************************************/
/*
* nsJSCID supports 'createInstance' and 'getService'. These are implemented
* using the (somewhat complex) nsIXPCScriptable scheme so that when called
* they can do security checks (knows in which JSContect is calling) and also
* so that 'getService' can add a fancy FinalizeListener to follow the
* ServiceManager's protocol for releasing a service.
*/

class CIDCreateInstanceScriptable : public nsIXPCScriptable
{
public:
    NS_DECL_ISUPPORTS
    XPC_DECLARE_IXPCSCRIPTABLE
    CIDCreateInstanceScriptable();
    virtual ~CIDCreateInstanceScriptable();
 };

// {16A43B00-F116-11d2-985A-006008962422}
#define NS_CIDCREATEINSTANCE_IID \
{ 0x16a43b00, 0xf116, 0x11d2, \
    { 0x98, 0x5a, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }

class CIDCreateInstance : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_CIDCREATEINSTANCE_IID)
    NS_DECL_ISUPPORTS
    CIDCreateInstance(nsJSCID* aCID);
    virtual ~CIDCreateInstance();
    nsJSCID* GetCID() const {return mCID;}
private:
    CIDCreateInstance(); // not implemented
    static CIDCreateInstanceScriptable* GetScriptable();
    nsJSCID* mCID;
};

/**********************************************/

CIDCreateInstanceScriptable::CIDCreateInstanceScriptable()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

CIDCreateInstanceScriptable::~CIDCreateInstanceScriptable() {}

static NS_DEFINE_IID(kCIDCreateInstanceScriptableIID, NS_IXPCSCRIPTABLE_IID);
NS_IMPL_ISUPPORTS(CIDCreateInstanceScriptable, kCIDCreateInstanceScriptableIID);

XPC_IMPLEMENT_FORWARD_CREATE(CIDCreateInstanceScriptable);
XPC_IMPLEMENT_IGNORE_LOOKUPPROPERTY(CIDCreateInstanceScriptable);
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(CIDCreateInstanceScriptable);
XPC_IMPLEMENT_IGNORE_GETPROPERTY(CIDCreateInstanceScriptable);
XPC_IMPLEMENT_IGNORE_SETPROPERTY(CIDCreateInstanceScriptable);
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(CIDCreateInstanceScriptable);
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(CIDCreateInstanceScriptable);
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(CIDCreateInstanceScriptable);
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(CIDCreateInstanceScriptable);
XPC_IMPLEMENT_FORWARD_ENUMERATE(CIDCreateInstanceScriptable);
XPC_IMPLEMENT_FORWARD_CHECKACCESS(CIDCreateInstanceScriptable);
// XPC_IMPLEMENT_IGNORE_CALL(CIDCreateInstanceScriptable);
XPC_IMPLEMENT_IGNORE_CONSTRUCT(CIDCreateInstanceScriptable);
XPC_IMPLEMENT_FORWARD_FINALIZE(CIDCreateInstanceScriptable);

NS_IMETHODIMP
CIDCreateInstanceScriptable::Call(JSContext *cx, JSObject *obj,
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

    if(NS_FAILED(wrapper->GetNative((nsISupports**)&self)) ||
       !(cidObj = self->GetCID()) ||
       NS_FAILED(cidObj->GetValid(&valid)) ||
       !valid ||
       NS_FAILED(cidObj->GetId(&cid)) ||
       !cid)
    {
        return NS_ERROR_FAILURE;
    }

    nsISupports* inst;
    nsresult rv;

    // XXX can do security check here (don't forget to free cid)

    // XXX could allow for passing an IID
    rv = nsComponentManager::CreateInstance(*cid, NULL,
                                            nsISupports::GetIID(),
                                            (void**) &inst);
    nsAllocator::Free(cid);

    if(NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    nsIXPConnectWrappedNative* instWrapper;
    rv = XPC_GetXPConnect()->WrapNative(cx, inst, nsISupports::GetIID(),
                                        &instWrapper);
    NS_RELEASE(inst);
    if(NS_FAILED(rv) || !instWrapper)
        return NS_ERROR_FAILURE;

    JSObject* instJSObj;
    instWrapper->GetJSObject(&instJSObj);
    *rval = OBJECT_TO_JSVAL(instJSObj);
    *retval = JS_TRUE;

    NS_RELEASE(instWrapper);
    return NS_OK;
}

/*********************************************/

NS_IMPL_QUERY_INTERFACE_SCRIPTABLE(CIDCreateInstance, GetScriptable())
NS_IMPL_ADDREF(CIDCreateInstance)
NS_IMPL_RELEASE(CIDCreateInstance)

CIDCreateInstance::CIDCreateInstance(nsJSCID *aCID)
    : mCID(aCID)
{
    NS_PRECONDITION(mCID, "bad cid");
    NS_INIT_ISUPPORTS();
    NS_ADDREF_THIS();
    NS_ADDREF(mCID);
}        

CIDCreateInstance::~CIDCreateInstance()
{
    if(mCID)
        NS_RELEASE(mCID);
}

// static 
CIDCreateInstanceScriptable* 
CIDCreateInstance::GetScriptable()
{
    static CIDCreateInstanceScriptable* scriptable = NULL;
    // we leak this singleton
    if(!scriptable)
        scriptable = new CIDCreateInstanceScriptable();
    return scriptable;
}        

/***************************************************************************/

class CIDGetServiceScriptable : public nsIXPCScriptable
{
public:
    NS_DECL_ISUPPORTS
    XPC_DECLARE_IXPCSCRIPTABLE
    CIDGetServiceScriptable();
    virtual ~CIDGetServiceScriptable();
 };

// {C46BC320-F13E-11d2-985A-006008962422}
#define NS_CIDGETSERVICE_IID \
{ 0xc46bc320, 0xf13e, 0x11d2, \
  { 0x98, 0x5a, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }

class CIDGetService : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_CIDGETSERVICE_IID)
    NS_DECL_ISUPPORTS
    CIDGetService(nsJSCID* aCID);
    virtual ~CIDGetService();
    nsJSCID* GetCID() const {return mCID;}
private:
    CIDGetService(); // not implemented

    static CIDGetServiceScriptable* CIDGetService::GetScriptable();
    nsJSCID* mCID;
};

/**********************************************/

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

static NS_DEFINE_IID(kServiceReleaserIID, NS_SERVICE_RELEASER_IID);
NS_IMPL_ISUPPORTS(ServiceReleaser, kServiceReleaserIID);


ServiceReleaser::ServiceReleaser(const nsCID& aCID)
    : mCID(aCID)
{
    NS_INIT_ISUPPORTS();
    NS_ADDREF_THIS();
}        

ServiceReleaser::~ServiceReleaser() {}

NS_IMETHODIMP
ServiceReleaser::AboutToRelease(nsISupports* aObj)
{
    return nsServiceManager::ReleaseService(mCID, aObj, NULL);
}

/**********************************************/

CIDGetServiceScriptable::CIDGetServiceScriptable()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

CIDGetServiceScriptable::~CIDGetServiceScriptable() {}

static NS_DEFINE_IID(kCIDGetServiceScriptableIID, NS_IXPCSCRIPTABLE_IID);
NS_IMPL_ISUPPORTS(CIDGetServiceScriptable, kCIDGetServiceScriptableIID);

XPC_IMPLEMENT_FORWARD_CREATE(CIDGetServiceScriptable);
XPC_IMPLEMENT_IGNORE_LOOKUPPROPERTY(CIDGetServiceScriptable);
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(CIDGetServiceScriptable);
XPC_IMPLEMENT_IGNORE_GETPROPERTY(CIDGetServiceScriptable);
XPC_IMPLEMENT_IGNORE_SETPROPERTY(CIDGetServiceScriptable);
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(CIDGetServiceScriptable);
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(CIDGetServiceScriptable);
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(CIDGetServiceScriptable);
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(CIDGetServiceScriptable);
XPC_IMPLEMENT_FORWARD_ENUMERATE(CIDGetServiceScriptable);
XPC_IMPLEMENT_FORWARD_CHECKACCESS(CIDGetServiceScriptable);
// XPC_IMPLEMENT_IGNORE_CALL(CIDGetServiceScriptable);
XPC_IMPLEMENT_IGNORE_CONSTRUCT(CIDGetServiceScriptable);
XPC_IMPLEMENT_FORWARD_FINALIZE(CIDGetServiceScriptable);

NS_IMETHODIMP
CIDGetServiceScriptable::Call(JSContext *cx, JSObject *obj,
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

    if(NS_FAILED(wrapper->GetNative((nsISupports**)&self)) ||
       !(cidObj = self->GetCID()) ||
       NS_FAILED(cidObj->GetValid(&valid)) ||
       !valid ||
       NS_FAILED(cidObj->GetId(&cid)) ||
       !cid)
    {
        return NS_ERROR_FAILURE;
    }

    nsISupports* srvc;
    nsresult rv;

    // XXX can do security check here (don't forget to free cid)

    // XXX could allow for passing an IID
    rv = nsServiceManager::GetService(*cid, nsISupports::GetIID(),
                                      &srvc, NULL);

    if(NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    nsIXPConnectWrappedNative* srvcWrapper;
    rv = XPC_GetXPConnect()->WrapNative(cx, srvc, nsISupports::GetIID(),
                                        &srvcWrapper);
    if(NS_FAILED(rv) || !srvcWrapper)
    {
        nsServiceManager::ReleaseService(*cid, srvc, NULL);
        nsAllocator::Free(cid);
        return NS_ERROR_FAILURE;
    }

    // This will eventually release the reference we got from
    // nsServiceManager::GetService
    ServiceReleaser* releaser = new ServiceReleaser(*cid);
    if(NS_FAILED(srvcWrapper->SetFinalizeListener(releaser)))
    {
        // Failure means that we are using a preexisting wrapper on
        // this service that has already setup a listener. So, we just
        // release our extra ref and trust the lister that is already in 
        // place to do the right thing.
        NS_RELEASE(srvc);
        NS_RELEASE(releaser);
    }
    nsAllocator::Free(cid);

    JSObject* srvcJSObj;
    srvcWrapper->GetJSObject(&srvcJSObj);
    *rval = OBJECT_TO_JSVAL(srvcJSObj);
    *retval = JS_TRUE;
    NS_RELEASE(srvcWrapper);

    return NS_OK;
}

/*********************************************/

NS_IMPL_QUERY_INTERFACE_SCRIPTABLE(CIDGetService, GetScriptable())
NS_IMPL_ADDREF(CIDGetService)
NS_IMPL_RELEASE(CIDGetService)

CIDGetService::CIDGetService(nsJSCID *aCID)
    : mCID(aCID)
{
    NS_PRECONDITION(mCID, "bad cid");
    NS_INIT_ISUPPORTS();
    NS_ADDREF_THIS();
    NS_ADDREF(mCID);
}        

CIDGetService::~CIDGetService()
{
    if(mCID)
        NS_RELEASE(mCID);
}

// static 
CIDGetServiceScriptable* 
CIDGetService::GetScriptable()
{
    static CIDGetServiceScriptable* scriptable = NULL;
    // we leak this singleton
    if(!scriptable)
        scriptable = new CIDGetServiceScriptable();
    return scriptable;
}        


/***************************************************************************/
// nsJSCID

nsresult
nsJSCID::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsISupports::GetIID()) ||
      aIID.Equals(nsIJSID::GetIID()) ||
      aIID.Equals(nsIJSCID::GetIID())) {
    *aInstancePtr = (void*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  *aInstancePtr = NULL;
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsJSCID)
NS_IMPL_RELEASE(nsJSCID)

nsJSCID::nsJSCID()
    :   mID(GetInvalidIID()),
        mNumber(gNoString),
        mName(gNoString)
{
    NS_INIT_ISUPPORTS();
    NS_ADDREF_THIS();
};

nsJSCID::~nsJSCID()
{
    if(mNumber && mNumber != gNoString)
        delete [] mNumber;
    if(mName && mName != gNoString)
        delete [] mName;
}

void nsJSCID::reset()
{
    mID = GetInvalidIID();

    if(mNumber && mNumber != gNoString)
        delete [] mNumber;
    if(mName && mName != gNoString)
        delete [] mName;

    mNumber = mName = NULL;
}

//static
nsJSCID*
nsJSCID::NewID(const char* str)
{
    PRBool success;
    nsJSCID* idObj = new nsJSCID();
    if(!idObj)
        return NULL;

    if(NS_FAILED(idObj->init(str, &success)) || !success)
    {
        delete idObj;
        return NULL;
    }

    return idObj;
}

void
nsJSCID::setName(const char* name)
{
    int len = strlen(name)+1;
    mName = new char[len];
    if(mName)
        memcpy(mName, name, len);
}

NS_IMETHODIMP
nsJSCID::GetName(char * *aName)
{
    if(!mName)
        mName = gNoString;
    *aName = (char*) nsAllocator::Clone(mName, strlen(mName)+1);
    return *aName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJSCID::GetNumber(char * *aNumber)
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
nsJSCID::GetId(nsID* *aId)
{
    *aId = (nsID*) nsAllocator::Clone(&mID, sizeof(nsID));
    return *aId ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJSCID::GetValid(PRBool *aValid)
{
    *aValid = !mID.Equals(GetInvalidIID());
    return NS_OK;
}

NS_IMETHODIMP
nsJSCID::equals(nsIJSID *other, PRBool *_retval)
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
nsJSCID::init(const char *idString, PRBool *_retval)
{
    if(!_retval || !idString)
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
        else
        {
            nsCID cid;
            if(NS_SUCCEEDED(nsComponentManager::ProgIDToCLSID(idString, &cid)))
            {
                mID = cid;
                setName(idString);
                success = PR_TRUE;
            }
        }
    }

    *_retval = success;
    return NS_OK;
}

NS_IMETHODIMP
nsJSCID::toString(char **_retval)
{
    return GetName(_retval);
}

/* readonly attribute nsISupports createInstance; */
NS_IMETHODIMP
nsJSCID::GetCreateInstance(nsISupports * *aCreateInstance)
{
    if(!aCreateInstance)
        return NS_ERROR_NULL_POINTER;

    *aCreateInstance = new CIDCreateInstance(this);
    return NS_OK;
}

/* readonly attribute nsISupports getService; */
NS_IMETHODIMP
nsJSCID::GetGetService(nsISupports * *aGetService)
{
    if(!aGetService)
        return NS_ERROR_NULL_POINTER;

    *aGetService = new CIDGetService(this);
    return NS_OK;
}

/***************************************************************************/
// additional utilities...

JSObject *
xpc_NewIIDObject(JSContext *cx, const nsID& aID)
{
    JSObject *obj = NULL;

    char* idString = aID.ToString();
    if(idString)
    {
        nsJSIID* iid = nsJSIID::NewID(idString);
        delete [] idString;
        if(iid)
        {
            nsXPConnect* xpc = nsXPConnect::GetXPConnect();
            if(xpc)
            {
                nsIXPConnectWrappedNative* nsid_wrapper;
                if(NS_SUCCEEDED(xpc->WrapNative(cx, iid,
                                                nsIJSIID::GetIID(),
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

