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

#include "nsIAtom.h"

#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventReceiver.h"

#include "nsIEventListenerManager.h"
#include "nsGUIEvent.h"

#include "LegacyPlugin.h"
#include "XPConnect.h"

// TODO remove me
#ifdef MOZ_ACTIVEX_PLUGIN_WMPSUPPORT
#include "XPCMediaPlayer.h"
#endif

static NS_DEFINE_IID(kIClassInfoIID, NS_ICLASSINFO_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

///////////////////////////////////////////////////////////////////////////////
// nsScriptablePeer

nsScriptablePeer::nsScriptablePeer() :
    mTearOff(new nsScriptablePeerTearOff(this))
{
    NS_INIT_ISUPPORTS();
    NS_ASSERTION(mTearOff, "can't create tearoff");
    xpc_AddRef();
}

nsScriptablePeer::~nsScriptablePeer()
{
    delete mTearOff;
    xpc_Release();
}

NS_IMPL_ADDREF(nsScriptablePeer)
NS_IMPL_RELEASE(nsScriptablePeer)

// Custom QueryInterface impl to deal with the IDispatch tearoff
NS_IMETHODIMP
nsScriptablePeer::QueryInterface(const nsIID & aIID, void **aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "QueryInterface requires a non-NULL destination!");
    if (!aInstancePtr)
        return NS_ERROR_NULL_POINTER;
    *aInstancePtr = nsnull;

    nsISupports* foundInterface = nsnull;
    if (aIID.Equals(NS_GET_IID(nsISupports)))
        foundInterface = NS_STATIC_CAST(nsISupports *, NS_STATIC_CAST(nsIClassInfo *, this));
    else if (aIID.Equals(NS_GET_IID(nsIClassInfo)))
        foundInterface = NS_STATIC_CAST(nsIClassInfo*, this);
    else if (aIID.Equals(NS_GET_IID(nsIMozAxPlugin)))
        foundInterface = NS_STATIC_CAST(nsIMozAxPlugin*, this);
    else if (memcmp(&aIID, &__uuidof(IDispatch), sizeof(nsID)) == 0)
    {
        HRESULT hr = mTearOff->QueryInterface(__uuidof(IDispatch), aInstancePtr);
        if (SUCCEEDED(hr)) return NS_OK;
        return E_FAIL;
    }

    NS_IF_ADDREF(foundInterface);
    *aInstancePtr = foundInterface;
    return (*aInstancePtr) ? NS_OK : NS_NOINTERFACE;
}

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

nsresult
nsScriptablePeer::HR2NS(HRESULT hr) const
{
    switch (hr)
    {
    case S_OK:
        return NS_OK;
    case E_NOTIMPL:
        return NS_ERROR_NOT_IMPLEMENTED;
    case E_NOINTERFACE:
        return NS_ERROR_NO_INTERFACE;
    case E_INVALIDARG:
        return NS_ERROR_INVALID_ARG;
    case E_ABORT:
        return NS_ERROR_ABORT;
    case E_UNEXPECTED:
        return NS_ERROR_UNEXPECTED;
    case E_OUTOFMEMORY:
        return NS_ERROR_OUT_OF_MEMORY;
    case E_POINTER:
        return NS_ERROR_INVALID_POINTER;
    case E_FAIL:
    default:
        return NS_ERROR_FAILURE;
    }
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

HRESULT
nsEventSink::InternalInvoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    FUNCDESC *pFuncDesc = NULL;
    HRESULT hr = S_OK;
    CComBSTR bstrName;

    // Must search and compare each member to the dispid...
    if (m_spEventSinkTypeInfo)
    {
	    HRESULT hr = S_OK;
	    TYPEATTR* pAttr;
	    hr = m_spEventSinkTypeInfo->GetTypeAttr(&pAttr);
        if (pAttr)
        {
	        int i;
	        for (i = 0; i < pAttr->cFuncs;i++)
	        {
		        hr = m_spEventSinkTypeInfo->GetFuncDesc(i, &pFuncDesc);
		        if (FAILED(hr))
			        return hr;
		        if (pFuncDesc->memid == dispIdMember)
                {
                    UINT cNames = 0;
                    m_spEventSinkTypeInfo->GetNames(dispIdMember, &bstrName, 1, &cNames);
			        break;
                }
                
		        m_spEventSinkTypeInfo->ReleaseFuncDesc(pFuncDesc);
		        pFuncDesc = NULL;
	        }
	        m_spEventSinkTypeInfo->ReleaseTypeAttr(pAttr);
        }
    }
    if (!pFuncDesc)
    {
        // Return
        return S_OK;
    }

#ifdef DEBUG
    {
        // Dump out some info to look at
        ATLTRACE(_T("Invoke(%d)\n"), (int) dispIdMember);

        ATLTRACE(_T("  "));

        /* Return code */
        switch (pFuncDesc->elemdescFunc.tdesc.vt)
        {
        case VT_HRESULT:     ATLTRACE(_T("HRESULT")); break;
        case VT_VOID:        ATLTRACE(_T("void")); break;
        default:             ATLTRACE(_T("void /* vt = %d */"), pFuncDesc->elemdescFunc.tdesc.vt); break;
        }

        /* Function name */
        ATLTRACE(_T(" %S("), SUCCEEDED(hr) ? bstrName.m_str : L"?unknown?");

        /* Parameters */
        for (int i = 0; i < pFuncDesc->cParams; i++)
        {
            USHORT  paramFlags = pFuncDesc->lprgelemdescParam[i].paramdesc.wParamFlags;
            ATLTRACE("[");
            BOOL addComma = FALSE;
            if (paramFlags & PARAMFLAG_FIN)
            {
                ATLTRACE(_T("in")); addComma = TRUE;
            }
            if (paramFlags & PARAMFLAG_FOUT)
            {
                ATLTRACE(addComma ? _T(",out") : _T("out")); addComma = TRUE;
            }
            if (paramFlags & PARAMFLAG_FRETVAL)
            {
                ATLTRACE(addComma ? _T(",retval") : _T("retval")); addComma = TRUE;
            }
            ATLTRACE("] ");

            VARTYPE vt = pFuncDesc->lprgelemdescParam[i].tdesc.vt;
            switch (vt)
            {
            case VT_HRESULT:     ATLTRACE(_T("HRESULT")); break;
            case VT_VARIANT:     ATLTRACE(_T("VARIANT")); break;
            case VT_I2:          ATLTRACE(_T("short")); break;
            case VT_I4:          ATLTRACE(_T("long")); break;
            case VT_R8:          ATLTRACE(_T("double")); break;
            case VT_BOOL:        ATLTRACE(_T("VARIANT_BOOL")); break;
            case VT_BSTR:        ATLTRACE(_T("BSTR")); break;
            case VT_DISPATCH:    ATLTRACE(_T("IDispatch *")); break;
            case VT_UNKNOWN:     ATLTRACE(_T("IUnknown *")); break;
            case VT_USERDEFINED: ATLTRACE(_T("/* Userdefined */")); break;
            case VT_PTR:         ATLTRACE(_T("void *")); break;
            case VT_VOID:        ATLTRACE(_T("void")); break;
            // More could be added...
            default:             ATLTRACE(_T("/* vt = %d */"), vt); break;
            }
            if (i + 1 < pFuncDesc->cParams)
            {
                ATLTRACE(_T(", "));
            }
        }
        ATLTRACE(_T(");\n"));
    }
#endif

    nsCOMPtr<nsIDOMElement> element;
    NPN_GetValue(mPlugin->pPluginInstance, NPNVDOMElement, (void *) &element);
    if (!element)
    {
        NS_ASSERTION(element, "can't get the object element");
        return S_OK;
    }
    nsAutoString id;
    if (NS_FAILED(element->GetAttribute(NS_LITERAL_STRING("id"), id)) ||
        id.IsEmpty())
    {
        // Object has no name so it can't fire events
        return S_OK;
    }

    // TODO Turn VARIANT args into js objects

    // Fire event to DOM 2 event listeners

    nsCOMPtr<nsIDOMEventReceiver> eventReceiver = do_QueryInterface(element);
    if (eventReceiver)
    {
        // Get the event manager
        nsCOMPtr<nsIEventListenerManager> eventManager;
        eventReceiver->GetListenerManager(getter_AddRefs(eventManager));
        if (eventManager)
        {
            nsAutoString keyName(bstrName.m_str);
            nsStringKey key(keyName);
            nsEvent event;
            event.message = NS_USER_DEFINED_EVENT;
            event.userType = &key;

            // Fire the event!
            nsCOMPtr<nsIDOMEvent> domEvent;
            nsEventStatus eventStatus;
            nsresult rv = eventManager->HandleEvent(nsnull, &event, getter_AddRefs(domEvent),
                eventReceiver, NS_EVENT_FLAG_INIT, &eventStatus);
        }
    }

    // Loop through all script tags looking for event handlers

    nsCOMPtr<nsIDOMDocument> doc;
    NPN_GetValue(mPlugin->pPluginInstance, NPNVDOMDocument, (void *)&doc);
    if (doc)
    {
        nsCOMPtr<nsIDOMNodeList> scriptList;
        doc->GetElementsByTagName(NS_LITERAL_STRING("script"), getter_AddRefs(scriptList));
        if (scriptList)
        {
            PRUint32 length = 0;
            scriptList->GetLength(&length);
            for (PRUint32 i = 0; i < length; i++)
            {
                nsCOMPtr<nsIDOMNode> node;
                scriptList->Item(i, getter_AddRefs(node));
                if (!node)
                {
                    continue;
                }
                nsCOMPtr<nsIDOMElement> element = do_QueryInterface(node);
                if (!element)
                {
                    continue;
                }

                // Get the <script FOR="foo" EVENT="bar"> attributes
                nsAutoString forAttr;
                nsAutoString eventAttr;
                if (NS_FAILED(element->GetAttribute(NS_LITERAL_STRING("for"), forAttr)) ||
                    forAttr.IsEmpty() ||
                    NS_FAILED(element->GetAttribute(NS_LITERAL_STRING("event"), eventAttr)) ||
                    eventAttr.IsEmpty())
                {
                    continue;
                }

                if (!forAttr.Equals(id)) // TODO compare no case?
                {
                    // Someone elses event
                    continue;
                }

                // TODO fire the event
            }
        }
    }

    // TODO Turn js objects for out params back into VARIANTS

    // TODO Turn js return code into VARIANT

    // TODO handle js exception and fill in exception info (do we care?)

    if (pExcepInfo)
    {
        pExcepInfo->wCode = 0;
    }

    m_spEventSinkTypeInfo->ReleaseFuncDesc(pFuncDesc);

    return S_OK;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// nsScriptablePeerTearOff

nsScriptablePeerTearOff::nsScriptablePeerTearOff(nsScriptablePeer *pOwner) :
    mOwner(pOwner)
{
    NS_ASSERTION(mOwner, "no owner");
}

HRESULT STDMETHODCALLTYPE nsScriptablePeerTearOff::QueryInterface(REFIID riid, void **ppvObject)
{
    if (::IsEqualIID(riid, _uuidof(IDispatch)))
    {
        *ppvObject = dynamic_cast<IDispatch *>(this);
        mOwner->AddRef();
        return NS_OK;
    }
    nsID iid;
    memcpy(&iid, &riid, sizeof(nsID));
    return mOwner->QueryInterface(iid, ppvObject);
}

ULONG STDMETHODCALLTYPE nsScriptablePeerTearOff::AddRef()
{
    return mOwner->AddRef();
}

ULONG STDMETHODCALLTYPE nsScriptablePeerTearOff::Release()
{
    return mOwner->Release();
}

// IDispatch
HRESULT STDMETHODCALLTYPE nsScriptablePeerTearOff::GetTypeInfoCount(UINT __RPC_FAR *pctinfo)
{
    CComPtr<IDispatch> disp;
    if (FAILED(mOwner->GetIDispatch(&disp)))
    {
        return E_UNEXPECTED;
    }
    return disp->GetTypeInfoCount(pctinfo);
}

HRESULT STDMETHODCALLTYPE nsScriptablePeerTearOff::GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo)
{
    CComPtr<IDispatch> disp;
    if (FAILED(mOwner->GetIDispatch(&disp)))
    {
        return E_UNEXPECTED;
    }
    return disp->GetTypeInfo(iTInfo, lcid, ppTInfo);
}

HRESULT STDMETHODCALLTYPE nsScriptablePeerTearOff::GetIDsOfNames(REFIID riid, LPOLESTR __RPC_FAR *rgszNames, UINT cNames, LCID lcid, DISPID __RPC_FAR *rgDispId)
{
    CComPtr<IDispatch> disp;
    if (FAILED(mOwner->GetIDispatch(&disp)))
    {
        return E_UNEXPECTED;
    }
    return disp->GetIDsOfNames(riid, rgszNames, cNames, lcid, rgDispId);
}

HRESULT STDMETHODCALLTYPE nsScriptablePeerTearOff::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS __RPC_FAR *pDispParams, VARIANT __RPC_FAR *pVarResult, EXCEPINFO __RPC_FAR *pExcepInfo, UINT __RPC_FAR *puArgErr)
{
    CComPtr<IDispatch> disp;
    if (FAILED(mOwner->GetIDispatch(&disp)))
    {
        return E_UNEXPECTED;
    }
    return disp->Invoke(dispIdMember, riid, lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Some public methods


static PRUint32 gInstances = 0;

void xpc_AddRef()
{
    if (gInstances == 0)
      XPCOMGlueStartup(nsnull);
    gInstances++;
}

void xpc_Release()
{
    if (--gInstances == 0)
      XPCOMGlueShutdown();
}

// TODO remove me
#ifdef MOZ_ACTIVEX_PLUGIN_WMPSUPPORT
const CLSID kWindowsMediaPlayer = {
    0x6BF52A52, 0x394A, 0x11d3, { 0xB1, 0x53, 0x00, 0xC0, 0x4F, 0x79, 0xFA, 0xA6 } };
#endif

CLSID xpc_GetCLSIDForType(const char *mimeType)
{
    if (mimeType == NULL)
    {
        return CLSID_NULL;
    }

    // Read the registry to see if there is a CLSID for an object to be associated with
    // this MIME type.
    USES_CONVERSION;
    CRegKey keyMimeDB;
    if (keyMimeDB.Open(HKEY_CLASSES_ROOT, _T("MIME\\Database\\Content Type"), KEY_READ) == ERROR_SUCCESS)
    {
        CRegKey keyMimeType;
        if (keyMimeType.Open(keyMimeDB, A2CT(mimeType), KEY_READ) == ERROR_SUCCESS)
        {
	        USES_CONVERSION;
	        TCHAR szGUID[64];
	        ULONG nCount = 64;

            GUID guidValue = GUID_NULL;
            if (keyMimeType.QueryValue(_T("CLSID"), szGUID, &nCount) == ERROR_SUCCESS &&
                SUCCEEDED(::CLSIDFromString(T2OLE(szGUID), &guidValue)))
            {
                return guidValue;
            }
        }
    }
    return CLSID_NULL;
}

nsScriptablePeer *
xpc_GetPeerForCLSID(const CLSID &clsid)
{
#ifdef XPC_IDISPATCH_SUPPORT
    return new nsScriptablePeer();
#else
// TODO remove me
#ifdef MOZ_ACTIVEX_PLUGIN_WMPSUPPORT
    if (::IsEqualCLSID(clsid, kWindowsMediaPlayer))
    {
        return new nsWMPScriptablePeer();
    }
#endif
    return new nsScriptablePeer();
#endif
}

nsIID
xpc_GetIIDForCLSID(const CLSID &clsid)
{
#ifdef XPC_IDISPATCH_SUPPORT
    nsIID iid;
    memcpy(&iid, &_uuidof(IDispatch), sizeof(iid));
    return iid;
#else
// TODO remove me
#ifdef MOZ_ACTIVEX_PLUGIN_WMPSUPPORT
    if (::IsEqualCLSID(clsid, kWindowsMediaPlayer))
    {
        return NS_GET_IID(nsIWMPPlayer2);
    }
#endif
    return NS_GET_IID(nsIMozAxPlugin);
#endif
}

// Called by NPP_GetValue to provide the scripting values
NPError
xpc_GetValue(NPP instance, NPPVariable variable, void *value)
{
    if (instance == NULL)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    PluginInstanceData *pData = (PluginInstanceData *) instance->pdata;

    // Happy happy fun fun - redefine some NPPVariable values that we might
    // be asked for but not defined by every PluginSDK 

    const int kVarScriptableInstance = 10; // NPPVpluginScriptableInstance
    const int kVarScriptableIID = 11; // NPPVpluginScriptableIID

    if (variable == kVarScriptableInstance)
    {
        if (!pData->pScriptingPeer)
        {
            nsScriptablePeer *peer  = xpc_GetPeerForCLSID(pData->clsid);
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
        *piid = xpc_GetIIDForCLSID(pData->clsid);
        *((nsIID **) value) = piid;
        return NPERR_NO_ERROR;
    }
    return NPERR_GENERIC_ERROR;
}



