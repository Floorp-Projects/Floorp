/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *   Adam Lock <adamlock@netscape.com>
 *   Paul Oswald <paul.oswald@isinet.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "stdafx.h"

#include "nsIMozAxPlugin.h"
#include "nsIClassInfo.h"
#include "nsMemory.h"

#include "LegacyPlugin.h"

// We must implement nsIClassInfo because it signals the
// Mozilla Security Manager to allow calls from JavaScript.

// helper class to implement all necessary nsIClassInfo method stubs
// and to set flags used by the security system
class nsClassInfoMozAxPlugin : public nsIClassInfo
{
    // These flags are used by the DOM and security systems to signal that 
    // JavaScript callers are allowed to call this object's scritable methods.
    NS_IMETHOD GetFlags(PRUint32 *aFlags)
        {*aFlags = nsIClassInfo::PLUGIN_OBJECT | nsIClassInfo::DOM_OBJECT;
        return NS_OK;}
    NS_IMETHOD GetImplementationLanguage(PRUint32 *aImplementationLanguage)
        {*aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
        return NS_OK;}

    // The rest of the methods can safely return error codes...
    NS_IMETHOD GetInterfaces(PRUint32 *count, nsIID * **array)
        {return NS_ERROR_NOT_IMPLEMENTED;}
    NS_IMETHOD GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
        {return NS_ERROR_NOT_IMPLEMENTED;}
    NS_IMETHOD GetContractID(char * *aContractID)
        {return NS_ERROR_NOT_IMPLEMENTED;}
    NS_IMETHOD GetClassDescription(char * *aClassDescription)
        {return NS_ERROR_NOT_IMPLEMENTED;}
    NS_IMETHOD GetClassID(nsCID * *aClassID)
        {return NS_ERROR_NOT_IMPLEMENTED;}
    NS_IMETHOD GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
        {return NS_ERROR_NOT_IMPLEMENTED;}
};

// Defines to be used as interface names by nsScriptablePeer
static NS_DEFINE_IID(kIMoxAxPluginIID, NS_IMOZAXPLUGIN_IID);
static NS_DEFINE_IID(kIClassInfoIID, NS_ICLASSINFO_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

class nsScriptablePeer : public nsIMozAxPlugin,
                         public nsClassInfoMozAxPlugin
{
    long mRef;
    PluginInstanceData* mPlugin;

protected:
    virtual ~nsScriptablePeer();

public:
    nsScriptablePeer();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIMOZAXPLUGIN
};




// Happy happy fun fun - redefine some NPPVariable values that we might
// be asked for but not defined by every PluginSDK 

const int kVarScriptableInstance = 10; // NPPVpluginScriptableInstance
const int kVarScriptableIID = 11; // NPPVpluginScriptableIID

NPError
xpconnect_getvalue(NPP instance, NPPVariable variable, void *value)
{
    if (instance == NULL)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    if (variable == kVarScriptableInstance)
    {
        PluginInstanceData *pData = (PluginInstanceData *) instance->pdata;
        if (!pData->pScriptingPeer)
        {
            nsScriptablePeer *peer  = new nsScriptablePeer();
            peer->AddRef();
            pData->pScriptingPeer = (nsIMozAxPlugin *) peer;
        }
        if (pData->pScriptingPeer)
        {
            pData->pScriptingPeer->AddRef();
            *((nsISupports **) value)= pData->pScriptingPeer;
            return NPERR_NO_ERROR;
        }
    }
    else if (variable == kVarScriptableIID)
    {
	    static nsIID kIMozAxPluginIID = NS_IMOZAXPLUGIN_IID;
        nsIID *piid = (nsIID *) NPN_MemAlloc(sizeof(nsIID));
        *piid = kIMozAxPluginIID;
        *((nsIID **) value) = piid;
        return NPERR_NO_ERROR;
    }
    return NPERR_GENERIC_ERROR;
}



///////////////////////////////////////////////////////////////////////////////
// nsScriptablePeer

nsScriptablePeer::nsScriptablePeer()
{
    mRef = 0;
}

nsScriptablePeer::~nsScriptablePeer()
{
}

///////////////////////////////////////////////////////////////////////////////
// nsISupports

// NOTE: We're not using the helpful NS_IMPL_ISUPPORTS macro because they drag
//       in dependencies to xpcom that a plugin like ourselves is better off
//       without.

/* void QueryInterface (in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
NS_IMETHODIMP nsScriptablePeer::QueryInterface(const nsIID & aIID, void * *aInstancePtr)
{
    if (aIID.Equals(NS_GET_IID(nsISupports)))
    {
        *aInstancePtr = NS_STATIC_CAST(void *, this);
        AddRef();
        return NS_OK;
    }
    else if (aIID.Equals(NS_GET_IID(nsIMozAxPlugin)))
    {
        *aInstancePtr = NS_STATIC_CAST(void *, this);
        AddRef();
        return NS_OK;
    }
    else if (aIID.Equals(kIClassInfoIID))
    {
        *aInstancePtr = static_cast<nsIClassInfo*>(this); 
        AddRef();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}

/* [noscript, notxpcom] nsrefcnt AddRef (); */
NS_IMETHODIMP_(nsrefcnt) nsScriptablePeer::AddRef()
{
    mRef++;
    return NS_OK;
}

/* [noscript, notxpcom] nsrefcnt Release (); */
NS_IMETHODIMP_(nsrefcnt) nsScriptablePeer::Release()
{
    mRef--;
    if (mRef <= 0)
    {
        delete this;
    }
    return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIMozAxPlugin

// the following method will be callable from JavaScript

NS_IMETHODIMP 
nsScriptablePeer::Invoke(const char *str)
{
/*    HRESULT hr;
    DISPID dispid;
    IDispatch FAR* pdisp = (IDispatch FAR*)NULL;
    // call the requested function
    const char* funcName = str; //_T("Update");
    PluginInstanceData *pData = mPlugin;
    if (pData == NULL) {
        return NPERR_INVALID_INSTANCE_ERROR;
    }
    IUnknown FAR* punk;
    hr = pData->pControlSite->GetControlUnknown(&punk);
    if (FAILED(hr)) {
        return NPERR_GENERIC_ERROR; 
    }
    punk->AddRef();
    hr = punk->QueryInterface(IID_IDispatch,(void FAR* FAR*)&pdisp);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR; 
    }
    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(funcName);
    hr = pdisp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR; 
    }
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    hr = pdisp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_METHOD,
        &dispparamsNoArgs, NULL, NULL, NULL);
    if (FAILED(hr)) { 
        return NPERR_GENERIC_ERROR; 
    }
    punk->Release(); */
    return NS_OK;
}


NS_IMETHODIMP 
nsScriptablePeer::GetProperty(const char *propertyName, char **_retval)
{
/*    HRESULT hr;
    DISPID dispid;
    //VARIANT VarResult;
    _variant_t VarResult;
    //char* propertyValue;
    IDispatch FAR* pdisp = (IDispatch FAR*)NULL;
    const char* property = propertyName;
    PluginInstanceData *pData = mPlugin;
    if (pData == NULL) { 
        return NPERR_INVALID_INSTANCE_ERROR;
    }
    IUnknown FAR* punk;
    hr = pData->pControlSite->GetControlUnknown(&punk);
    if (FAILED(hr)) { return NULL; }
    punk->AddRef();
    hr = punk->QueryInterface(IID_IDispatch,(void FAR* FAR*)&pdisp);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR; 
    }
    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(property);
    hr = pdisp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR;
    }
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    hr = pdisp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYGET,
        &dispparamsNoArgs, &VarResult, NULL, NULL);
    if (FAILED(hr)) { 
        return NPERR_GENERIC_ERROR;
    }
	punk->Release();
    
    char* tempStr;
    switch(VarResult.vt & VT_TYPEMASK) {
    case VT_BSTR:    
        tempStr = OLE2A(VarResult.bstrVal);
        if(!_retval) return NS_ERROR_NULL_POINTER;
        *_retval = (char*) nsMemory::Alloc(strlen(tempStr) + 1);
        if (! *_retval) return NS_ERROR_NULL_POINTER;
        if (VarResult.bstrVal == NULL) {
            *_retval = NULL;
        } else {
            strcpy(*_retval, tempStr);
        }
        break;
//  case VT_I2:
    default:
        VarResult.ChangeType(VT_BSTR);
        tempStr = OLE2A(VarResult.bstrVal);
        if(!_retval) return NS_ERROR_NULL_POINTER;
        *_retval = (char*) nsMemory::Alloc(strlen(tempStr) + 1);
        if (! *_retval) return NS_ERROR_NULL_POINTER;
        if (VarResult.bstrVal == NULL) {
            *_retval = NULL;
        } else {
            strcpy(*_retval, tempStr);
        }
        break;
    } */

    // caller will be responsible for any memory allocated.
	return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::GetNProperty(const char *propertyName, PRInt16 *_retval)
{
/*    HRESULT hr;
    DISPID dispid;
    VARIANT VarResult;
    IDispatch FAR* pdisp = (IDispatch FAR*)NULL;
    const char* property = propertyName;
    PluginInstanceData *pData = mPlugin;
    if (pData == NULL) { 
        return NPERR_INVALID_INSTANCE_ERROR;
    }
    IUnknown FAR* punk;
    hr = pData->pControlSite->GetControlUnknown(&punk);
    if (FAILED(hr)) { return NULL; }
    punk->AddRef();
    hr = punk->QueryInterface(IID_IDispatch,(void FAR* FAR*)&pdisp);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR; 
    }
    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(property);
    hr = pdisp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR;
    }
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    hr = pdisp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYGET,
        &dispparamsNoArgs, &VarResult, NULL, NULL);
    if (FAILED(hr)) { 
        return NPERR_GENERIC_ERROR;
    }
    punk->Release();
    if(!_retval) return NS_ERROR_NULL_POINTER;
    // make sure we are dealing with an int
    if ((VarResult.vt & VT_TYPEMASK) != VT_I2) {
        *_retval = NULL;
        return NPERR_GENERIC_ERROR;
    }
    *_retval = VarResult.iVal; */

	// caller will be responsible for any memory allocated.
	return NS_OK;
}

/* void setProperty (in string propertyName, in string propertyValue); */
NS_IMETHODIMP nsScriptablePeer::SetProperty(const char *propertyName, const char *propertyValue)
{
    HRESULT hr;
    DISPID dispid;
    VARIANT VarResult;
    IDispatch FAR* pdisp = (IDispatch FAR*)NULL;
    const char* property = propertyName;
    PluginInstanceData *pData = mPlugin;
    if (pData == NULL) { 
        return NPERR_INVALID_INSTANCE_ERROR;
    }
    IUnknown FAR* punk;
    hr = pData->pControlSite->GetControlUnknown(&punk);
    if (FAILED(hr)) { return NULL; }
    punk->AddRef();
    hr = punk->QueryInterface(IID_IDispatch,(void FAR* FAR*)&pdisp);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR;
    }
    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(property);
    hr = pdisp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR;
    }
    VARIANT *pvars = new VARIANT[1];
    VariantInit(&pvars[0]);
    pvars->vt = VT_BSTR;
    pvars->bstrVal = A2OLE(propertyValue);
    DISPID dispIdPut = DISPID_PROPERTYPUT;
    DISPPARAMS functionArgs;
    functionArgs.rgdispidNamedArgs = &dispIdPut;
    functionArgs.rgvarg = pvars;
    functionArgs.cArgs = 1;
    functionArgs.cNamedArgs = 1;

    hr = pdisp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYPUT,
        &functionArgs, &VarResult, NULL, NULL);
    delete []pvars;
    if (FAILED(hr)) { 
        return NPERR_GENERIC_ERROR;
    }
    punk->Release();
    return NS_OK;
}

/* void setNProperty (in string propertyName, in string propertyValue); */
NS_IMETHODIMP nsScriptablePeer::SetNProperty(const char *propertyName, PRInt16 propertyValue)
{
    HRESULT hr;
    DISPID dispid;
    VARIANT VarResult;
    IDispatch FAR* pdisp = (IDispatch FAR*)NULL;
    const char* property = propertyName;
    PluginInstanceData *pData = mPlugin;
    if (pData == NULL) { 
        return NPERR_INVALID_INSTANCE_ERROR;
    }
    IUnknown FAR* punk;
    hr = pData->pControlSite->GetControlUnknown(&punk);
    if (FAILED(hr)) { return NULL; }
    punk->AddRef();
    hr = punk->QueryInterface(IID_IDispatch,(void FAR* FAR*)&pdisp);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR;
    }
    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(property);
    hr = pdisp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR;
    }
   
    VARIANT *pvars = new VARIANT[1];
    VariantInit(&pvars[0]);
    pvars->vt = VT_I2;
    pvars->iVal = propertyValue;

    DISPID dispIdPut = DISPID_PROPERTYPUT;
    DISPPARAMS functionArgs;
    functionArgs.rgdispidNamedArgs = &dispIdPut;
    functionArgs.rgvarg = pvars;
    functionArgs.cArgs = 1;
    functionArgs.cNamedArgs = 1;

    hr = pdisp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYPUT,
        &functionArgs, &VarResult, NULL, NULL);
    delete []pvars;
    if (FAILED(hr)) { 
        return NPERR_GENERIC_ERROR;
    }
    punk->Release();
    return NS_OK;
}
