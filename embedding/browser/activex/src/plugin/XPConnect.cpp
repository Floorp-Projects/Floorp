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

#include "nsXPCOMGlue.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"

#include "nsIMozAxPlugin.h"
#include "nsIClassInfo.h"
#include "nsIVariant.h"
#include "nsMemory.h"

#include "LegacyPlugin.h"
#include "XPConnect.h"

static NS_DEFINE_IID(kIClassInfoIID, NS_ICLASSINFO_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

static PRUint32 gInstances = 0;

///////////////////////////////////////////////////////////////////////////////
// nsScriptablePeer

nsScriptablePeer::nsScriptablePeer()
{
    mRef = 0;
    if (gInstances == 0)
      XPCOMGlueStartup(nsnull);
    gInstances++;
}

nsScriptablePeer::~nsScriptablePeer()
{
    if (--gInstances == 0)
      XPCOMGlueShutdown();
}

NS_IMPL_ADDREF(nsScriptablePeer)
NS_IMPL_RELEASE(nsScriptablePeer)

NS_INTERFACE_MAP_BEGIN(nsScriptablePeer)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIClassInfo)
    NS_INTERFACE_MAP_ENTRY(nsIMozAxPlugin)
NS_INTERFACE_MAP_END

HRESULT
nsScriptablePeer::GetIDispatch(IDispatch **pdisp)
{
    if (pdisp == NULL)
    {
        return E_INVALIDARG;
    }
    *pdisp = NULL;

    IUnknownPtr unk;
    HRESULT hr = mPlugin->pControlSite->GetControlUnknown(&unk);
    if (unk.GetInterfacePtr() == NULL)
    {
		return E_FAIL; 
	}

    IDispatchPtr disp = unk;
    if (disp.GetInterfacePtr() == NULL)
    { 
        return E_FAIL; 
    }

    *pdisp = disp.GetInterfacePtr();
    (*pdisp)->AddRef();

    return S_OK;
}

HRESULT
nsScriptablePeer::ConvertVariants(nsIVariant *aIn, VARIANT *aOut)
{
    if (aIn == NULL || aOut == NULL)
    {
        return NS_ERROR_INVALID_ARG;
    }

    PRBool isWritable = PR_FALSE;
    nsCOMPtr<nsIWritableVariant> writable = do_QueryInterface(aIn);
    if (writable)
    {
        writable->GetWritable(&isWritable);
    }

    PRUint16 type;
    nsresult rv = aIn->GetDataType(&type);
    switch (type)
    {
    case nsIDataType::VTYPE_INT8:
        {
            PRUint8 value = 0;
            rv = aIn->GetAsInt8(&value);
            aOut->vt = VT_I1;
            aOut->cVal = value;
        }
        break;
    case nsIDataType::VTYPE_INT16:
        {
            PRInt16 value = 0;
            rv = aIn->GetAsInt16(&value);
            aOut->vt = VT_I2;
            aOut->iVal = value;
        }
        break;
    case nsIDataType::VTYPE_INT32:
        {
            PRInt32 value = 0;
            rv = aIn->GetAsInt32(&value);
            aOut->vt = VT_I4;
            aOut->lVal = value;
        }
        break;
    case nsIDataType::VTYPE_CHAR:
    case nsIDataType::VTYPE_UINT8:
        {
            PRUint8 value = 0;
            rv = aIn->GetAsInt8(&value);
            aOut->vt = VT_UI1;
            aOut->bVal = value;
        }
        break;
    case nsIDataType::VTYPE_WCHAR:
    case nsIDataType::VTYPE_UINT16:
        {
            PRUint16 value = 0;
            rv = aIn->GetAsUint16(&value);
            aOut->vt = VT_I2;
            aOut->uiVal = value;
        }
        break;
    case nsIDataType::VTYPE_UINT32:
        {
            PRUint32 value = 0;
            rv = aIn->GetAsUint32(&value);
            aOut->vt = VT_I4;
            aOut->ulVal = value;
        }
        break;
    case nsIDataType::VTYPE_FLOAT:
        {
            float value = 0;
            rv = aIn->GetAsFloat(&value);
            aOut->vt = VT_R4;
            aOut->fltVal = value;
        }
        break;
    case nsIDataType::VTYPE_DOUBLE:
        {
            double value = 0;
            rv = aIn->GetAsDouble(&value);
            aOut->vt = VT_R4;
            aOut->dblVal = value;
        }
        break;
    case nsIDataType::VTYPE_BOOL:
        {
            PRBool value = 0;
            rv = aIn->GetAsBool(&value);
            aOut->vt = VT_BOOL;
            aOut->dblVal = value ? VARIANT_TRUE : VARIANT_FALSE;
        }
        break;
    case nsIDataType::VTYPE_EMPTY:
        VariantClear(aOut);
        break;

    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_CHAR_STR:
        {
            nsXPIDLCString value;
            aIn->GetAsString(getter_Copies(value));
            nsAutoString valueWide; valueWide.AssignWithConversion(value);
            aOut->vt = VT_BSTR;
            aOut->bstrVal = SysAllocString(valueWide.get());
        }
        break;
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
    case nsIDataType::VTYPE_WCHAR_STR:
        {
            nsXPIDLString value;
            aIn->GetAsWString(getter_Copies(value));
            aOut->vt = VT_BSTR;
            aOut->bstrVal = SysAllocString(value.get());
        }
        break;

    case nsIDataType::VTYPE_ASTRING:
        {
            nsAutoString value;
            aIn->GetAsAString(value);
            aOut->vt = VT_BSTR;
            aOut->bstrVal = SysAllocString(value.get());
        }
        break;

    case nsIDataType::VTYPE_DOMSTRING:
        {
            nsAutoString value;
            aIn->GetAsAString(value);
            aOut->vt = VT_BSTR;
            aOut->bstrVal = SysAllocString(value.get());
        }
        break;

    case nsIDataType::VTYPE_CSTRING:
        {
            nsCAutoString value;
            aIn->GetAsACString(value);
            nsAutoString valueWide; valueWide.AssignWithConversion(value.get());
            aOut->vt = VT_BSTR;
            aOut->bstrVal = SysAllocString(valueWide.get());
        }
        break;
    case nsIDataType::VTYPE_UTF8STRING:
        {
            nsCAutoString value;
            aIn->GetAsAUTF8String(value);
            nsAutoString valueWide; valueWide.AssignWithConversion(value.get());
            aOut->vt = VT_BSTR;
            aOut->bstrVal = SysAllocString(valueWide.get());
        }

    // Unsupported types
    default:
    case nsIDataType::VTYPE_INT64:
    case nsIDataType::VTYPE_UINT64:
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_ID:
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
    case nsIDataType::VTYPE_ARRAY:
    case nsIDataType::VTYPE_EMPTY_ARRAY:
        return E_INVALIDARG;
    }

    return S_OK;
}

HRESULT
nsScriptablePeer::ConvertVariants(VARIANT *aIn, nsIVariant **aOut)
{
    if (aIn == NULL || aOut == NULL)
    {
        return NS_ERROR_INVALID_ARG;
    }

    *aOut = nsnull;

    nsresult rv;
    nsCOMPtr<nsIWritableVariant> v = do_CreateInstance("@mozilla.org/variant;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    switch (aIn->vt)
    {
    case VT_EMPTY:
        v->SetAsEmpty();
        break;
    case VT_BSTR:
        v->SetAsWString(aIn->bstrVal);
        break;
    case VT_I1:
        v->SetAsInt8(aIn->cVal);
        break;
    case VT_I2:
        v->SetAsInt16(aIn->iVal);
        break;
    case VT_I4:
        v->SetAsInt32(aIn->lVal);
        break;
    case VT_UI1:
        v->SetAsUint8(aIn->bVal);
        break;
    case VT_UI2:
        v->SetAsUint16(aIn->uiVal);
        break;
    case VT_UI4:
        v->SetAsUint32(aIn->ulVal);
        break;
    case VT_BOOL:
        v->SetAsBool(aIn->boolVal == VARIANT_TRUE ? PR_TRUE : PR_FALSE);
        break;
    case VT_R4:
        v->SetAsFloat(aIn->fltVal);
        break;
    case VT_R8:
        v->SetAsDouble(aIn->dblVal);
        break;
    }

    *aOut = v;
    NS_ADDREF(*aOut);

    return NS_OK;
} 

NS_IMETHODIMP
nsScriptablePeer::InternalInvoke(const char *aMethod, unsigned int aNumArgs, nsIVariant *aArgs[])
{
    HRESULT hr;
    DISPID dispid;

    IDispatchPtr disp;
    if (FAILED(GetIDispatch(&disp)))
    {
        return NPERR_GENERIC_ERROR; 
    }

    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(aMethod);
    hr = disp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr))
    { 
        return NPERR_GENERIC_ERROR; 
    }
    
    _variant_t *pArgs = NULL;
    if (aNumArgs > 0)
    {
        pArgs = new _variant_t[aNumArgs];
        if (pArgs == NULL)
        {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        for (unsigned int i = 0; i < aNumArgs; i++)
        {
            hr = ConvertVariants(aArgs[i], &pArgs[i]);
            if (FAILED(hr))
            {
                delete []pArgs;
                return NS_ERROR_INVALID_ARG;
            }
        }
    }

    DISPPARAMS dispparams = {NULL, NULL, 0, 0};
    _variant_t vResult;

    dispparams.cArgs = aNumArgs;
    dispparams.rgvarg = pArgs;

    hr = disp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_METHOD,
        &dispparams, &vResult, NULL, NULL);

    if (pArgs)
    {
        delete []pArgs;
    }

    if (FAILED(hr))
    { 
        return NPERR_GENERIC_ERROR; 
    }

    return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsISupports

// We must implement nsIClassInfo because it signals the
// Mozilla Security Manager to allow calls from JavaScript.

NS_IMETHODIMP nsScriptablePeer::GetFlags(PRUint32 *aFlags)
{
    *aFlags = nsIClassInfo::PLUGIN_OBJECT | nsIClassInfo::DOM_OBJECT;
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::GetImplementationLanguage(PRUint32 *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
}

// The rest of the methods can safely return error codes...
NS_IMETHODIMP nsScriptablePeer::GetInterfaces(PRUint32 *count, nsIID * **array)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsScriptablePeer::GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsScriptablePeer::GetContractID(char * *aContractID)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsScriptablePeer::GetClassDescription(char * *aClassDescription)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsScriptablePeer::GetClassID(nsCID * *aClassID)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsScriptablePeer::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


///////////////////////////////////////////////////////////////////////////////
// nsDefaultScriptablePeer

// Defines to be used as interface names by nsDefaultScriptablePeer

///////////////////////////////////////////////////////////////////////////////
// nsIMozAxPlugin

NS_IMETHODIMP 
nsScriptablePeer::Invoke(const char *aMethod)
{
    return InternalInvoke(aMethod, 0, NULL);
}

NS_IMETHODIMP 
nsScriptablePeer::Invoke1(const char *aMethod, nsIVariant *a)
{
    nsIVariant *args[1];
    args[0] = a;
    return InternalInvoke(aMethod, sizeof(args) / sizeof(args[0]), args);
}

NS_IMETHODIMP 
nsScriptablePeer::Invoke2(const char *aMethod, nsIVariant *a, nsIVariant *b)
{
    nsIVariant *args[2];
    args[0] = a;
    args[1] = b;
    return InternalInvoke(aMethod, sizeof(args) / sizeof(args[0]), args);
}

NS_IMETHODIMP 
nsScriptablePeer::Invoke3(const char *aMethod, nsIVariant *a, nsIVariant *b, nsIVariant *c)
{
    nsIVariant *args[3];
    args[0] = a;
    args[1] = b;
    args[2] = c;
    return InternalInvoke(aMethod, sizeof(args) / sizeof(args[0]), args);
}

NS_IMETHODIMP 
nsScriptablePeer::Invoke4(const char *aMethod, nsIVariant *a, nsIVariant *b, nsIVariant *c, nsIVariant *d)
{
    nsIVariant *args[4];
    args[0] = a;
    args[1] = b;
    args[2] = c;
    args[3] = d;
    return InternalInvoke(aMethod, sizeof(args) / sizeof(args[0]), args);
}

NS_IMETHODIMP 
nsScriptablePeer::GetProperty(const char *propertyName, nsIVariant **_retval)
{
    HRESULT hr;
    DISPID dispid;
    IDispatchPtr disp;
    if (FAILED(GetIDispatch(&disp)))
    {
        return NPERR_GENERIC_ERROR; 
    }
    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(propertyName);
    hr = disp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr))
    { 
        return NPERR_GENERIC_ERROR;
    }

    DISPID dispIdPut = DISPID_PROPERTYPUT;
    _variant_t vResult;
    
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    hr = disp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYGET,
        &dispparamsNoArgs, &vResult, NULL, NULL);
    
    if (FAILED(hr))
    { 
        return NPERR_GENERIC_ERROR;
    }

    nsCOMPtr<nsIVariant> propertyValue;
    ConvertVariants(&vResult, getter_AddRefs(propertyValue));
    *_retval = propertyValue;
    NS_IF_ADDREF(*_retval);

    return NS_OK;
}

/* void setProperty (in string propertyName, in string propertyValue); */
NS_IMETHODIMP
nsScriptablePeer::SetProperty(const char *propertyName, nsIVariant *propertyValue)
{
    HRESULT hr;
    DISPID dispid;
    IDispatchPtr disp;
    if (FAILED(GetIDispatch(&disp)))
    {
        return NPERR_GENERIC_ERROR; 
    }
    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(propertyName);
    hr = disp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr))
    { 
        return NPERR_GENERIC_ERROR;
    }

    _variant_t v;
    ConvertVariants(propertyValue, &v);
    
    DISPID dispIdPut = DISPID_PROPERTYPUT;
    DISPPARAMS functionArgs;
    _variant_t vResult;
    
    functionArgs.rgdispidNamedArgs = &dispIdPut;
    functionArgs.rgvarg = &v;
    functionArgs.cArgs = 1;
    functionArgs.cNamedArgs = 1;

    hr = disp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYPUT,
        &functionArgs, &vResult, NULL, NULL);
    
    if (FAILED(hr))
    { 
        return NPERR_GENERIC_ERROR;
    }

    return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIMozAxPluginIID, NS_IMOZAXPLUGIN_IID);

NPError
xpconnect_getvalue(NPP instance, NPPVariable variable, void *value)
{
    if (instance == NULL)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    // Happy happy fun fun - redefine some NPPVariable values that we might
    // be asked for but not defined by every PluginSDK 

    const int kVarScriptableInstance = 10; // NPPVpluginScriptableInstance
    const int kVarScriptableIID = 11; // NPPVpluginScriptableIID

    if (variable == kVarScriptableInstance)
    {
        PluginInstanceData *pData = (PluginInstanceData *) instance->pdata;
        if (!pData->pScriptingPeer)
        {
            nsScriptablePeer *peer  = new nsScriptablePeer();
            peer->AddRef();
            pData->pScriptingPeer = (nsIMozAxPlugin *) peer;
            peer->mPlugin = pData;
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
        nsIID *piid = (nsIID *) NPN_MemAlloc(sizeof(nsIID));
        *piid = kIMozAxPluginIID;
        *((nsIID **) value) = piid;
        return NPERR_NO_ERROR;
    }
    return NPERR_GENERIC_ERROR;
}

