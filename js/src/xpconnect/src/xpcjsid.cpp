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
    : mID(GetInvalidIID()), mNumber(gNoString), mName(gNoString)
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

NS_IMETHODIMP
nsJSCID::createInstance(nsISupports **_retval)
{
    if(!_retval)
        return NS_ERROR_NULL_POINTER;

    *_retval = NULL;
    if(mID.Equals(GetInvalidIID()))
        return NS_ERROR_FAILURE;

    return nsComponentManager::CreateInstance(mID, NULL,
                                              nsISupports::GetIID(),
                                              (void**) _retval);
}

// XXX this does not yet address the issue of using the SM to release
// XXX this does not yet address the issue of security protections
// XXX we'll need to make createInstance and getService dynamic to 
// support both of these issues

NS_IMETHODIMP
nsJSCID::getService(nsISupports **_retval)
{
    if(!_retval)
        return NS_ERROR_NULL_POINTER;

    *_retval = NULL;
    if(mID.Equals(GetInvalidIID()))
        return NS_ERROR_FAILURE;
    
    return nsServiceManager::GetService(mID, 
                                        nsISupports::GetIID(),
                                        _retval, NULL);
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

