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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   John Bandhauer <jband@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

NS_IMPL_THREADSAFE_ISUPPORTS1(nsJSID, nsIJSID)

char nsJSID::gNoString[] = "";

nsJSID::nsJSID()
    : mID(GetInvalidIID()), mNumber(gNoString), mName(gNoString)
{
    NS_INIT_ISUPPORTS();
};

nsJSID::~nsJSID()
{
    if(mNumber && mNumber != gNoString)
        PR_Free(mNumber);
    if(mName && mName != gNoString)
        PR_Free(mName);
}

void nsJSID::Reset()
{
    mID = GetInvalidIID();

    if(mNumber && mNumber != gNoString)
        PR_Free(mNumber);
    if(mName && mName != gNoString)
        PR_Free(mName);

    mNumber = mName = nsnull;
}

PRBool
nsJSID::SetName(const char* name)
{
    NS_ASSERTION(!mName || mName == gNoString ,"name already set");
    NS_ASSERTION(name,"null name");
    int len = strlen(name)+1;
    mName = (char*)PR_Malloc(len);
    if(!mName)
        return PR_FALSE;
    memcpy(mName, name, len);
    return PR_TRUE;
}

NS_IMETHODIMP
nsJSID::GetName(char * *aName)
{
    if(!aName)
        return NS_ERROR_NULL_POINTER;

    if(!NameIsSet())
        SetNameToNoString();
    NS_ASSERTION(mName, "name not set");
    *aName = (char*) nsMemory::Clone(mName, strlen(mName)+1);
    return *aName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJSID::GetNumber(char * *aNumber)
{
    if(!aNumber)
        return NS_ERROR_NULL_POINTER;

    if(!mNumber)
    {
        if(!(mNumber = mID.ToString()))
            mNumber = gNoString;
    }

    *aNumber = (char*) nsMemory::Clone(mNumber, strlen(mNumber)+1);
    return *aNumber ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJSID::GetId(nsID* *aId)
{
    if(!aId)
        return NS_ERROR_NULL_POINTER;

    *aId = (nsID*) nsMemory::Clone(&mID, sizeof(nsID));
    return *aId ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJSID::GetValid(PRBool *aValid)
{
    if(!aValid)
        return NS_ERROR_NULL_POINTER;

    *aValid = !mID.Equals(GetInvalidIID());
    return NS_OK;
}

NS_IMETHODIMP
nsJSID::Equals(nsIJSID *other, PRBool *_retval)
{
    if(!_retval)
        return NS_ERROR_NULL_POINTER;

    *_retval = PR_FALSE;

    if(!other || mID.Equals(GetInvalidIID()))
        return NS_OK;

    nsID* otherID;
    if(NS_SUCCEEDED(other->GetId(&otherID)))
    {
        *_retval = mID.Equals(*otherID);
        nsMemory::Free(otherID);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsJSID::Initialize(const char *idString)
{
    if(!idString)
        return NS_ERROR_NULL_POINTER;

    PRBool success = PR_FALSE;

    if(strlen(idString) && mID.Equals(GetInvalidIID()))
    {
        Reset();

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
nsJSID::InitWithName(const nsID& id, const char *nameString)
{
    NS_ASSERTION(nameString, "no name");
    Reset();
    mID = id;
    return SetName(nameString);
}

// try to use the name, if no name, then use the number
NS_IMETHODIMP
nsJSID::ToString(char **_retval)
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
                nsMemory::Free(str);
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
        if(NS_FAILED(idObj->Initialize(str)))
            NS_RELEASE(idObj);
    }
    return idObj;
}

/***************************************************************************/
/***************************************************************************/

NS_IMPL_THREADSAFE_ISUPPORTS3(nsJSIID, nsIJSID, nsIJSIID, nsIXPCScriptable)

XPC_IMPLEMENT_FORWARD_CREATE(nsJSIID)
// XPC_IMPLEMENT_IGNORE_GETFLAGS(nsJSIID)
// XPC_IMPLEMENT_IGNORE_LOOKUPPROPERTY(nsJSIID)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(nsJSIID)
// XPC_IMPLEMENT_IGNORE_GETPROPERTY(nsJSIID)
XPC_IMPLEMENT_IGNORE_SETPROPERTY(nsJSIID)
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(nsJSIID)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(nsJSIID)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(nsJSIID)
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(nsJSIID)
// XPC_IMPLEMENT_IGNORE_ENUMERATE(nsJSIID)
XPC_IMPLEMENT_IGNORE_CHECKACCESS(nsJSIID)
XPC_IMPLEMENT_IGNORE_CALL(nsJSIID)
XPC_IMPLEMENT_IGNORE_CONSTRUCT(nsJSIID)
// XPC_IMPLEMENT_FORWARD_HASINSTANCE(nsJSIID)
XPC_IMPLEMENT_FORWARD_FINALIZE(nsJSIID)

nsJSIID::nsJSIID()
    :   mCacheFilled(JS_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsJSIID::~nsJSIID() {}

NS_IMETHODIMP nsJSIID::GetName(char * *aName)
    {ResolveName(); return mDetails.GetName(aName);}

NS_IMETHODIMP nsJSIID::GetNumber(char * *aNumber)
    {return mDetails.GetNumber(aNumber);}

NS_IMETHODIMP nsJSIID::GetId(nsID* *aId)
    {return mDetails.GetId(aId);}

NS_IMETHODIMP nsJSIID::GetValid(PRBool *aValid)
    {return mDetails.GetValid(aValid);}

NS_IMETHODIMP nsJSIID::Equals(nsIJSID *other, PRBool *_retval)
    {return mDetails.Equals(other, _retval);}

NS_IMETHODIMP nsJSIID::Initialize(const char *idString)
    {return mDetails.Initialize(idString);}

NS_IMETHODIMP nsJSIID::ToString(char **_retval)
    {ResolveName(); return mDetails.ToString(_retval);}

void
nsJSIID::ResolveName()
{
    if(!mDetails.NameIsSet())
    {
        nsIInterfaceInfoManager* iim;
        if(nsnull != (iim = nsXPConnect::GetInterfaceInfoManager()))
        {
            char* name;
            if(NS_SUCCEEDED(iim->GetNameForIID(mDetails.GetID(), &name)) && name)
            {
                mDetails.SetName(name);
                nsMemory::Free(name);
            }
            NS_RELEASE(iim);
        }
        if(!mDetails.NameIsSet())
            mDetails.SetNameToNoString();
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
            if(NS_SUCCEEDED(idObj->Initialize(str)))
                success = PR_TRUE;
        }
        else
        {
            nsIInterfaceInfoManager* iim;
            if(nsnull != (iim = nsXPConnect::GetInterfaceInfoManager()))
            {
                nsCOMPtr<nsIInterfaceInfo> iinfo;
                PRBool canScript;
                nsID* pid;
                if(NS_SUCCEEDED(iim->GetInfoForName(str,
                                                    getter_AddRefs(iinfo))) &&
                   NS_SUCCEEDED(iinfo->IsScriptable(&canScript)) && canScript &&
                   NS_SUCCEEDED(iinfo->GetIID(&pid)) && pid)
                {
                    success = idObj->mDetails.InitWithName(*pid, str);
                    nsMemory::Free(pid);
                }
                NS_RELEASE(iim);
            }
        }
        if(!success)
            NS_RELEASE(idObj);
    }
    return idObj;
}

NS_IMETHODIMP
nsJSIID::HasInstance(JSContext *cx, JSObject *obj,
                     jsval v, JSBool *bp,
                     nsIXPConnectWrappedNative* wrapper,
                     nsIXPCScriptable* arbitrary,
                     JSBool* retval)
{
    *bp = JS_FALSE;
    *retval = JS_TRUE;
    nsresult rv = NS_OK;

    if(!JSVAL_IS_PRIMITIVE(v))
    {
        // we have a JSObject
        JSObject* obj = JSVAL_TO_OBJECT(v);

        NS_ASSERTION(obj, "when is an object not an object?");

        // is this really a native xpcom object with a wrapper?
        nsXPCWrappedNative* other_wrapper =
           nsXPCWrappedNativeClass::GetWrappedNativeOfJSObject(cx,obj);

        if(!other_wrapper)
            return NS_OK;

        if(mDetails.GetID()->Equals(other_wrapper->GetIID()))
            *bp = JS_TRUE;
        else
        {
            // This would be so easy except for the case...
            // other_wrapper might inherit from our type
            nsXPCWrappedNativeClass* clazz;
            nsIInterfaceInfo* prev;
            if(!(clazz = other_wrapper->GetClass()) ||
               !(prev = clazz->GetInterfaceInfo()))
                return NS_ERROR_UNEXPECTED;

            nsIInterfaceInfo* cur;
            NS_ADDREF(prev);
            while(NS_SUCCEEDED(prev->GetParent(&cur)) && cur)
            {
                NS_RELEASE(prev);
                prev = cur;

                nsID* iid;
                if(NS_SUCCEEDED(cur->GetIID(&iid)))
                {
                    JSBool found = mDetails.GetID()->Equals(*iid);
                    nsMemory::Free(iid);
                    if(found)
                    {
                        *bp = JS_TRUE;
                        break;
                    }
                }
                else
                {
                    rv = NS_ERROR_OUT_OF_MEMORY;
                    break;
                }
            }
            NS_RELEASE(prev);
        }
    }
    return rv;
}

void
nsJSIID::FillCache(JSContext *cx, JSObject *obj,
                   nsIXPConnectWrappedNative *wrapper,
                   nsIXPCScriptable *arbitrary)
{
    nsIInterfaceInfoManager* iim = nsnull;
    nsCOMPtr<nsIInterfaceInfo> iinfo;
    PRUint16 count;

    if(!(iim = XPTI_GetInterfaceInfoManager()) ||
       NS_FAILED(iim->GetInfoForIID(mDetails.GetID(), getter_AddRefs(iinfo))) ||
       !iinfo ||
       NS_FAILED(iinfo->GetConstantCount(&count)))

    {
        NS_ASSERTION(0,"access to interface info is truly horked");
        NS_IF_RELEASE(iim);
        ThrowException(NS_ERROR_XPC_UNEXPECTED, cx);
        return;
    }
    NS_RELEASE(iim);


    for(PRUint16 i = 0; i < count; i++)
    {
        const nsXPTConstant* constant;
        jsid id;
        JSString *jstrid;
        jsval val;
        JSBool retval;

        if(NS_FAILED(iinfo->GetConstant(i, &constant)) ||
           !(jstrid = JS_InternString(cx, constant->GetName())) ||
           !JS_ValueToId(cx, STRING_TO_JSVAL(jstrid), &id) ||
           !nsXPCWrappedNativeClass::GetConstantAsJSVal(cx, iinfo, i, &val) ||
           NS_FAILED(arbitrary->SetProperty(cx, obj, id, &val, wrapper, nsnull, &retval)) ||
           !retval)
        {
            ThrowException(NS_ERROR_XPC_UNEXPECTED, cx);
            return;
        }
    }

    mCacheFilled = JS_TRUE;
    return;
}

NS_IMETHODIMP
nsJSIID::GetFlags(JSContext *cx, JSObject *obj,
                  nsIXPConnectWrappedNative* wrapper,
                  JSUint32* flagsp,
                  nsIXPCScriptable* arbitrary)
{
    NS_PRECONDITION(flagsp, "bad param");
    *flagsp = XPCSCRIPTABLE_DONT_ENUM_STATIC_PROPS;
    return NS_OK;
}

NS_IMETHODIMP
nsJSIID::LookupProperty(JSContext *cx, JSObject *obj,
                        jsid id,
                        JSObject **objp, JSProperty **propp,
                        nsIXPConnectWrappedNative* wrapper,
                        nsIXPCScriptable* arbitrary,
                        JSBool* retval)
{
    if(!mCacheFilled)
        FillCache(cx, obj, wrapper, arbitrary);
    return arbitrary->LookupProperty(cx, obj, id, objp, propp, wrapper,
                                     nsnull, retval);
}

NS_IMETHODIMP
nsJSIID::GetProperty(JSContext *cx, JSObject *obj,
                     jsid id, jsval *vp,
                     nsIXPConnectWrappedNative* wrapper,
                     nsIXPCScriptable* arbitrary,
                     JSBool* retval)
{
    if(!mCacheFilled)
        FillCache(cx, obj, wrapper, arbitrary);
    return arbitrary->GetProperty(cx, obj, id, vp, wrapper, nsnull, retval);
}


NS_IMETHODIMP
nsJSIID::Enumerate(JSContext *cx, JSObject *obj,
                   JSIterateOp enum_op,
                   jsval *statep, jsid *idp,
                   nsIXPConnectWrappedNative *wrapper,
                   nsIXPCScriptable *arbitrary,
                   JSBool *retval)
{
    if(enum_op == JSENUMERATE_INIT && !mCacheFilled)
        FillCache(cx, obj, wrapper, arbitrary);

    return arbitrary->Enumerate(cx, obj, enum_op, statep, idp, wrapper,
                                arbitrary, retval);
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/*
* nsJSCID supports 'createInstance' and 'getService'. These are implemented
* using the (somewhat complex) nsIXPCScriptable scheme so that when called
* they can do security checks (i.e so that the methods know in which JSContect 
* the call is being made).
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

NS_IMPL_THREADSAFE_ISUPPORTS2(CIDCreateInstance, CIDCreateInstance, nsIXPCScriptable)

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

XPC_IMPLEMENT_FORWARD_CREATE(CIDCreateInstance)
XPC_IMPLEMENT_IGNORE_GETFLAGS(CIDCreateInstance)
XPC_IMPLEMENT_IGNORE_LOOKUPPROPERTY(CIDCreateInstance)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(CIDCreateInstance)
XPC_IMPLEMENT_IGNORE_GETPROPERTY(CIDCreateInstance)
XPC_IMPLEMENT_IGNORE_SETPROPERTY(CIDCreateInstance)
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(CIDCreateInstance)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(CIDCreateInstance)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(CIDCreateInstance)
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(CIDCreateInstance)
XPC_IMPLEMENT_FORWARD_ENUMERATE(CIDCreateInstance)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(CIDCreateInstance)
// XPC_IMPLEMENT_IGNORE_CALL(CIDCreateInstance)
XPC_IMPLEMENT_IGNORE_CONSTRUCT(CIDCreateInstance)
XPC_IMPLEMENT_FORWARD_HASINSTANCE(CIDCreateInstance)
XPC_IMPLEMENT_FORWARD_FINALIZE(CIDCreateInstance)

NS_IMETHODIMP
CIDCreateInstance::Call(JSContext *cx, JSObject *obj,
                        uintN argc, jsval *argv,
                        jsval *rval,
                        nsIXPConnectWrappedNative* wrapper,
                        nsIXPCScriptable* arbitrary,
                        JSBool* retval)
{
    nsJSCID* cidObj;
    nsCID*  cid;
    PRBool valid;
    nsID iid;

    if(!(cidObj = GetCID()) ||
       NS_FAILED(cidObj->GetValid(&valid)) ||
       !valid ||
       NS_FAILED(cidObj->GetId(&cid)) ||
       !cid)
    {
        ThrowException(NS_ERROR_XPC_BAD_CID, cx);
        *retval = JS_FALSE;
        return NS_OK;
    }

    // Do the security check if necessary

    XPCContext* xpcc;
    if(nsnull != (xpcc = nsXPConnect::GetContext(cx)))
    {
        nsIXPCSecurityManager* sm;
        sm = xpcc->GetAppropriateSecurityManager(
                            nsIXPCSecurityManager::HOOK_CREATE_INSTANCE);
        if(sm && NS_OK != sm->CanCreateInstance(cx, *cid))
        {
            // the security manager vetoed. It should have set an exception.
            nsMemory::Free(cid);
            *rval = JSVAL_NULL;
            *retval = JS_FALSE;
            return NS_OK;
        }
    }

    // If an IID was passed in then use it
    if(argc)
    {
        JSObject* iidobj;
        jsval val = *argv;
        nsID* piid = nsnull;
        if(JSVAL_IS_PRIMITIVE(val) || 
           !(iidobj = JSVAL_TO_OBJECT(val)) ||
           !(piid = xpc_JSObjectToID(cx, iidobj)))
        {
            ThrowException(NS_ERROR_XPC_BAD_IID, cx);
            *retval = JS_FALSE;
            return NS_OK;
        }
        iid = *piid;
        nsMemory::Free(piid);
    }
    else
        iid = NS_GET_IID(nsISupports);

    nsCOMPtr<nsISupports> inst;
    nsresult rv;

    rv = nsComponentManager::CreateInstance(*cid, nsnull, iid, 
                                            (void**) getter_AddRefs(inst));
    NS_ASSERTION(NS_FAILED(rv) || inst, "component manager returned success, but instance is null!");
    nsMemory::Free(cid);

    if(NS_FAILED(rv) || !inst)
    {
        ThrowBadResultException(NS_ERROR_XPC_CI_RETURNED_FAILURE, cx, rv);
        *retval = JS_FALSE;
        return NS_OK;
    }

    nsCOMPtr<nsXPConnect> xpc = dont_AddRef(nsXPConnect::GetXPConnect());
    if(!xpc)
    {
        ThrowBadResultException(NS_ERROR_UNEXPECTED, cx, rv);
        *retval = JS_FALSE;
        return NS_OK;
    }

    JSObject* instJSObj;
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = xpc->WrapNative(cx, obj, inst, iid, getter_AddRefs(holder));
    if(NS_FAILED(rv) || !holder || NS_FAILED(holder->GetJSObject(&instJSObj)))
    {
        ThrowException(NS_ERROR_XPC_CANT_CREATE_WN, cx);
        *retval = JS_FALSE;
        return NS_OK;
    }

    *rval = OBJECT_TO_JSVAL(instJSObj);
    *retval = JS_TRUE;

    return NS_OK;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

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

NS_IMPL_THREADSAFE_ISUPPORTS2(CIDGetService, CIDGetService, nsIXPCScriptable)

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

XPC_IMPLEMENT_FORWARD_CREATE(CIDGetService)
XPC_IMPLEMENT_IGNORE_GETFLAGS(CIDGetService)
XPC_IMPLEMENT_IGNORE_LOOKUPPROPERTY(CIDGetService)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(CIDGetService)
XPC_IMPLEMENT_IGNORE_GETPROPERTY(CIDGetService)
XPC_IMPLEMENT_IGNORE_SETPROPERTY(CIDGetService)
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(CIDGetService)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(CIDGetService)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(CIDGetService)
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(CIDGetService)
XPC_IMPLEMENT_FORWARD_ENUMERATE(CIDGetService)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(CIDGetService)
// XPC_IMPLEMENT_IGNORE_CALL(CIDGetService)
XPC_IMPLEMENT_IGNORE_CONSTRUCT(CIDGetService)
XPC_IMPLEMENT_FORWARD_HASINSTANCE(CIDGetService)
XPC_IMPLEMENT_FORWARD_FINALIZE(CIDGetService)

NS_IMETHODIMP
CIDGetService::Call(JSContext *cx, JSObject *obj,
                    uintN argc, jsval *argv,
                    jsval *rval,
                    nsIXPConnectWrappedNative* wrapper,
                    nsIXPCScriptable* arbitrary,
                    JSBool* retval)
{
    nsJSCID* cidObj;
    nsCID*  cid;
    PRBool valid;
    nsID iid;

    if(!(cidObj = GetCID()) ||
       NS_FAILED(cidObj->GetValid(&valid)) ||
       !valid ||
       NS_FAILED(cidObj->GetId(&cid)) ||
       !cid)
    {
        ThrowException(NS_ERROR_XPC_BAD_CID, cx);
        *retval = JS_FALSE;
        return NS_OK;
    }

    // Do the security check if necessary

    XPCContext* xpcc;
    if(nsnull != (xpcc = nsXPConnect::GetContext(cx)))
    {
        nsIXPCSecurityManager* sm;
        sm = xpcc->GetAppropriateSecurityManager(
                            nsIXPCSecurityManager::HOOK_GET_SERVICE);
        if(sm && NS_OK != sm->CanGetService(cx, *cid))
        {
            // the security manager vetoed. It should have set an exception.
            nsMemory::Free(cid);
            *rval = JSVAL_NULL;
            *retval = JS_FALSE;
            return NS_OK;
        }
    }

    // If an IID was passed in then use it
    if(argc)
    {
        JSObject* iidobj;
        jsval val = *argv;
        nsID* piid = nsnull;
        if(JSVAL_IS_PRIMITIVE(val) || 
           !(iidobj = JSVAL_TO_OBJECT(val)) ||
           !(piid = xpc_JSObjectToID(cx, iidobj)))
        {
            nsMemory::Free(cid);
            ThrowException(NS_ERROR_XPC_BAD_IID, cx);
            *retval = JS_FALSE;
            return NS_OK;
        }
        iid = *piid;
        nsMemory::Free(piid);
    }
    else
        iid = NS_GET_IID(nsISupports);

    nsresult rv;

    nsCOMPtr<nsISupports> srvc;
    rv = nsServiceManager::GetService(*cid, iid, getter_AddRefs(srvc), nsnull);
    nsMemory::Free(cid);
    NS_ASSERTION(NS_FAILED(rv) || srvc, "service manager returned success, but service is null!");
    if(NS_FAILED(rv) || !srvc)
    {
        ThrowBadResultException(NS_ERROR_XPC_GS_RETURNED_FAILURE, cx, rv);
        *retval = JS_FALSE;
        return NS_OK;
    }

    nsCOMPtr<nsXPConnect> xpc = dont_AddRef(nsXPConnect::GetXPConnect());
    if(!xpc)
    {
        ThrowBadResultException(NS_ERROR_UNEXPECTED, cx, rv);
        *retval = JS_FALSE;
        return NS_OK;
    }

    JSObject* srvcJSObj;
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = xpc->WrapNative(cx, obj, srvc, iid, getter_AddRefs(holder));
    if(NS_FAILED(rv) || !holder || NS_FAILED(holder->GetJSObject(&srvcJSObj)))
    {
        ThrowException(NS_ERROR_XPC_CANT_CREATE_WN, cx);
        *retval = JS_FALSE;
        return NS_OK;
    }

    *rval = OBJECT_TO_JSVAL(srvcJSObj);
    *retval = JS_TRUE;

    return NS_OK;
}

/***************************************************************************/
/***************************************************************************/

NS_IMPL_THREADSAFE_ISUPPORTS3(nsJSCID, nsIJSID, nsIJSCID, nsIXPCScriptable)

XPC_IMPLEMENT_FORWARD_CREATE(nsJSCID)
XPC_IMPLEMENT_IGNORE_GETFLAGS(nsJSCID)
XPC_IMPLEMENT_IGNORE_LOOKUPPROPERTY(nsJSCID)
XPC_IMPLEMENT_IGNORE_DEFINEPROPERTY(nsJSCID)
XPC_IMPLEMENT_IGNORE_GETPROPERTY(nsJSCID)
XPC_IMPLEMENT_IGNORE_SETPROPERTY(nsJSCID)
XPC_IMPLEMENT_IGNORE_GETATTRIBUTES(nsJSCID)
XPC_IMPLEMENT_IGNORE_SETATTRIBUTES(nsJSCID)
XPC_IMPLEMENT_IGNORE_DELETEPROPERTY(nsJSCID)
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(nsJSCID)
XPC_IMPLEMENT_FORWARD_ENUMERATE(nsJSCID)
XPC_IMPLEMENT_IGNORE_CHECKACCESS(nsJSCID)
XPC_IMPLEMENT_IGNORE_CALL(nsJSCID)
// XPC_IMPLEMENT_IGNORE_CONSTRUCT(nsJSCID)
XPC_IMPLEMENT_FORWARD_HASINSTANCE(nsJSCID)
XPC_IMPLEMENT_FORWARD_FINALIZE(nsJSCID)

nsJSCID::nsJSCID()  {NS_INIT_ISUPPORTS();}
nsJSCID::~nsJSCID() {}

NS_IMETHODIMP nsJSCID::GetName(char * *aName)
    {ResolveName(); return mDetails.GetName(aName);}

NS_IMETHODIMP nsJSCID::GetNumber(char * *aNumber)
    {return mDetails.GetNumber(aNumber);}

NS_IMETHODIMP nsJSCID::GetId(nsID* *aId)
    {return mDetails.GetId(aId);}

NS_IMETHODIMP nsJSCID::GetValid(PRBool *aValid)
    {return mDetails.GetValid(aValid);}

NS_IMETHODIMP nsJSCID::Equals(nsIJSID *other, PRBool *_retval)
    {return mDetails.Equals(other, _retval);}

NS_IMETHODIMP nsJSCID::Initialize(const char *idString)
    {return mDetails.Initialize(idString);}

NS_IMETHODIMP nsJSCID::ToString(char **_retval)
    {ResolveName(); return mDetails.ToString(_retval);}

void
nsJSCID::ResolveName()
{
    if(!mDetails.NameIsSet())
        mDetails.SetNameToNoString();
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
            if(NS_SUCCEEDED(idObj->Initialize(str)))
                success = PR_TRUE;
        }
        else
        {
            nsCID cid;
            if(NS_SUCCEEDED(nsComponentManager::ContractIDToClassID(str, &cid)))
            {
                success = idObj->mDetails.InitWithName(cid, str);
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

NS_IMETHODIMP
nsJSCID::Construct(JSContext *cx, JSObject *obj,
                   uintN argc, jsval *argv,
                   jsval *rval,
                   nsIXPConnectWrappedNative* wrapper,
                   nsIXPCScriptable* arbitrary,
                   JSBool* retval)
{
    nsIXPCScriptable *ci;
    CIDCreateInstance *p;
    if(nsnull == (p = new CIDCreateInstance(this)) ||
       NS_FAILED(p->QueryInterface(NS_GET_IID(nsIXPCScriptable),(void**)&ci)))
    {
        ThrowException(NS_ERROR_OUT_OF_MEMORY, cx);
        *retval = JS_FALSE;
        return NS_OK;
    }
    nsresult rv = ci->Call(cx, obj, argc, argv, rval, 
                           wrapper, arbitrary, retval);
    NS_RELEASE(ci);
    return rv;
}

/***************************************************************************/
// additional utilities...

JSObject *
xpc_NewIDObject(JSContext *cx, JSObject* jsobj, const nsID& aID)
{
    JSObject *obj = nsnull;

    char* idString = aID.ToString();
    if(idString)
    {
        nsCOMPtr<nsIJSID> iid = 
            dont_AddRef(NS_STATIC_CAST(nsIJSID*, nsJSID::NewID(idString)));
        nsCRT::free(idString);
        if(iid)
        {
            nsCOMPtr<nsXPConnect> xpc = 
                dont_AddRef(nsXPConnect::GetXPConnect());
            if(xpc)
            {
                nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
                nsresult rv = xpc->WrapNative(cx, jsobj,
                                              NS_STATIC_CAST(nsISupports*,iid),
                                              NS_GET_IID(nsIJSID),
                                              getter_AddRefs(holder));
                if(NS_SUCCEEDED(rv) && holder)
                {
                    holder->GetJSObject(&obj);
                }
            }
        }
    }
    return obj;
}

nsID*
xpc_JSObjectToID(JSContext *cx, JSObject* obj)
{
    nsID* id = nsnull;
    if(!cx || !obj)
        return nsnull;

    // NOTE: this call does NOT addref
    nsXPCWrappedNative* wrapper =
        nsXPCWrappedNativeClass::GetWrappedNativeOfJSObject(cx, obj);
    if(wrapper)
    {
        if(wrapper->GetIID().Equals(NS_GET_IID(nsIJSID))  ||
           wrapper->GetIID().Equals(NS_GET_IID(nsIJSIID)) ||
           wrapper->GetIID().Equals(NS_GET_IID(nsIJSCID)))
        {
            ((nsIJSID*)wrapper->GetNative())->GetId(&id);
        }
    }
    // XXX it might be nice to try to construct one from an object that can be
    // converted into a string.
    return id;
}

