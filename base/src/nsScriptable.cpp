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

#include "nsIScriptable.h"
#include "nscore.h"
#include "xptcall.h"
#include "nsIInterfaceInfoManager.h"

class nsScriptable : public nsIScriptable {
public:
    NS_DECL_ISUPPORTS

    // nsIProperties methods:
    NS_IMETHOD DefineProperty(const char* prop, nsISupports* initialValue);
    NS_IMETHOD UndefineProperty(const char* prop);
    NS_IMETHOD GetProperty(const char* prop, nsISupports* *result);
    NS_IMETHOD SetProperty(const char* prop, nsISupports* value);
    NS_IMETHOD HasProperty(const char* prop, nsISupports* value); 

    // nsIScriptable methods:
    NS_IMETHOD Call(const char* command,
                    nsISupportsArray* arguments,
                    nsISupports* *result);

    // nsScriptable methods:
    nsScriptable(REFNSIID iid, nsISupports* object);
    virtual ~nsScriptable();

    nsresult Init();
    // XXX should this be a method on nsIScriptable?
    NS_IMETHOD QueryInterfaceScriptable(REFNSIID aIID, void** aInstancePtr);

    // XXX later this will be a service
    static nsIInterfaceInfoManager* gInterfaceInfoManager;

protected:
    nsISupports* mObject;
    nsIInterfaceInfo* mInterfaceInfo;    
    nsID mIID;
//    nsXPCWrappedNativeClass* mClazz;
//    nsXPCWrappedNative* mWrapper;
};

// XXX later this will be a service
nsIInterfaceInfoManager* nsScriptable::gInterfaceInfoManager = nsnull;

////////////////////////////////////////////////////////////////////////////////

nsScriptable::nsScriptable(REFNSIID iid, nsISupports* object)
    : mObject(object), mInterfaceInfo(nsnull), mIID(iid)
//  , mClazz(nsnull), mWrapper(nsnull)
{
    NS_INIT_REFCNT();
    NS_ADDREF(mObject);
}

nsScriptable::~nsScriptable()
{
    NS_RELEASE(mObject);
}

NS_IMPL_ISUPPORTS(nsScriptable, nsIScriptable::GetIID());

nsresult
nsScriptable::Init()
{
    nsresult rv;
    if (gInterfaceInfoManager == nsnull) {
        gInterfaceInfoManager = XPTI_GetInterfaceInfoManager();
        if (gInterfaceInfoManager == nsnull)
            return NS_ERROR_FAILURE;
    }

    // get the interface info
    NS_IF_RELEASE(mInterfaceInfo);
    rv = gInterfaceInfoManager->GetInfoForIID(&mIID, &mInterfaceInfo);
    if (NS_FAILED(rv)) return rv;

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProperties methods:

NS_IMETHODIMP
nsScriptable::DefineProperty(const char* prop, nsISupports* initialValue)
{
    return NS_OK;
}

NS_IMETHODIMP
nsScriptable::UndefineProperty(const char* prop)
{
    return NS_OK;
}

NS_IMETHODIMP
nsScriptable::GetProperty(const char* prop, nsISupports* *result)
{
    return NS_OK;
}

NS_IMETHODIMP
nsScriptable::SetProperty(const char* prop, nsISupports* value)
{
    return NS_OK;
}

NS_IMETHODIMP
nsScriptable::HasProperty(const char* prop, nsISupports* expectedValue)
{
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIScriptable methods:

#define PARAM_BUFFER_COUNT     16

NS_IMETHODIMP
nsScriptable::Call(const char* methodName,
                   nsISupportsArray* arguments,
                   nsISupports* *result)
{
    nsresult rv;
    const nsXPTMethodInfo* methodInfo;
    PRUint16 vtblIndex;
    PRUint8 paramCount = arguments->Count();
    nsXPTCVariant paramBuffer[PARAM_BUFFER_COUNT];
    nsXPTCVariant* dispatchParams;
    PRUint8 requiredArgs;

    // get the method info and vtable index
    rv = mInterfaceInfo->GetMethodInfoForName(methodName, &vtblIndex, &methodInfo);
    if (NS_FAILED(rv)) return rv;

    if (paramCount < PARAM_BUFFER_COUNT) {
        dispatchParams = paramBuffer;
    }
    else {
        dispatchParams = new nsXPTCVariant[paramCount];
        if (dispatchParams == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    // put together the parameter list
    for (PRUint8 i = 0; i < paramCount; i++) {
        nsISupports* arg = (*arguments)[i];
        const nsXPTParamInfo& paramInfo = methodInfo->GetParam(i);
        const nsXPTType& paramType = paramInfo.GetType();
        nsXPTCVariant* dp = &dispatchParams[i];

        if (paramInfo.IsOut()) {
        }
        else {
            
        }
        switch (paramType.TagPart()) {
          case nsXPTType::T_INTERFACE:
            dp->flags |= nsXPTCVariant::VAL_IS_IFACE;
            nsID* iid;
            rv = mInterfaceInfo->GetIIDForParam(&paramInfo, &iid);
            if (NS_FAILED(rv)) goto done;
            break;
          case nsXPTType::T_INTERFACE_IS:
            break;
          default:
            break;
        }
    }

    // invoke the method
    rv = XPTC_InvokeByIndex(mObject, vtblIndex, paramCount, dispatchParams);
    // return any out parameters

  done:
    if (dispatchParams != paramBuffer)
        delete[] dispatchParams;

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsScriptable::QueryInterfaceScriptable(REFNSIID aIID, void** aInstancePtr)
{
    NS_PRECONDITION(aInstancePtr, "null aInstancePtr");
    nsresult rv;
    rv = mObject->QueryInterface(aIID, aInstancePtr);
    if (NS_FAILED(rv)) return rv;
    NS_RELEASE(mObject);
    mObject = *(nsISupports**)aInstancePtr;
    mIID = aIID;
    return Init();
}

////////////////////////////////////////////////////////////////////////////////

extern nsresult
NS_NewIScriptable(REFNSIID iid, nsISupports* object, nsIScriptable* *result)
{
    nsScriptable* s = new nsScriptable(iid, object);
    if (s == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    nsresult rv = s->Init();
    if (NS_FAILED(rv)) {
        delete s;
        return rv;
    }
    NS_ADDREF(s);
    *result = s;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
