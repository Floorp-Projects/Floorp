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
 *   John Bandhauer <jband@netscape.com> (original author)
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

/* nsIVariant implementation for xpconnect. */

#include "xpcprivate.h"

NS_IMPL_ISUPPORTS2(XPCVariant, XPCVariant, nsIVariant)

XPCVariant::XPCVariant()
    : mJSVal(JSVAL_VOID)
{
    NS_INIT_ISUPPORTS();
    nsVariant::Initialize(&mData);
}

XPCVariant::~XPCVariant()
{
    nsVariant::Cleanup(&mData);
    
    if(JSVAL_IS_GCTHING(mJSVal))
    {
        JSRuntime* rt;
        nsIJSRuntimeService* rtsrvc = nsXPConnect::GetJSRuntimeService();

        if(rtsrvc && NS_SUCCEEDED(rtsrvc->GetRuntime(&rt)))
            JS_RemoveRootRT(rt, &mJSVal);
    }
}

// static 
XPCVariant* XPCVariant::newVariant(XPCCallContext& ccx, jsval aJSVal)
{
    XPCVariant* variant = new XPCVariant();
    if(!variant)
        return nsnull;
    
    NS_ADDREF(variant);

    variant->mJSVal = aJSVal;

    if(JSVAL_IS_GCTHING(variant->mJSVal))
    {
        JSRuntime* rt;
        if(NS_FAILED(ccx.GetRuntime()->GetJSRuntimeService()->GetRuntime(&rt))||
           !JS_AddNamedRootRT(rt, &variant->mJSVal, "XPCVariant::mJSVal"))
        {
            NS_RELEASE(variant); // Also sets variant to nsnull.
        }
    }

    if(variant && !variant->InitializeData(ccx))
        NS_RELEASE(variant);     // Also sets variant to nsnull.

    return variant;
}

JSBool XPCVariant::InitializeData(XPCCallContext& ccx)
{
    if(JSVAL_IS_INT(mJSVal))
        return NS_SUCCEEDED(nsVariant::SetFromInt32(&mData, 
                                                   JSVAL_TO_INT(mJSVal)));
    if(JSVAL_IS_DOUBLE(mJSVal))
        return NS_SUCCEEDED(nsVariant::SetFromDouble(&mData, 
                                                     *JSVAL_TO_DOUBLE(mJSVal)));
    if(JSVAL_IS_BOOLEAN(mJSVal))
        return NS_SUCCEEDED(nsVariant::SetFromBool(&mData, 
                                                   JSVAL_TO_BOOLEAN(mJSVal)));
    if(JSVAL_IS_VOID(mJSVal))
        return NS_SUCCEEDED(nsVariant::SetToEmpty(&mData));
    if(JSVAL_IS_NULL(mJSVal))
        return NS_SUCCEEDED(nsVariant::SetToEmpty(&mData));
    if(JSVAL_IS_STRING(mJSVal))
    {
        return NS_SUCCEEDED(nsVariant::SetFromWStringWithSize(&mData, 
                    (PRUint32)JS_GetStringLength(JSVAL_TO_STRING(mJSVal)),
                    (PRUnichar*)JS_GetStringChars(JSVAL_TO_STRING(mJSVal))));
    }

    // leaving only JSObject...
    NS_ASSERTION(JSVAL_IS_OBJECT(mJSVal), "invalid type of jsval!");

    // let's see if it is a xpcJSID

    // XXX It might be nice to have a non-allocing version of xpc_JSObjectToID.
    nsID* id = xpc_JSObjectToID(ccx, JSVAL_TO_OBJECT(mJSVal));
    if(id)
    {
        JSBool success = NS_SUCCEEDED(nsVariant::SetFromID(&mData, *id));
        nsMemory::Free((char*)id);
        return success;
    }
    
    // XXX This could be smarter and pick some more interesting iface.

    nsXPConnect*  xpc;
    nsCOMPtr<nsISupports> wrapper;
    const nsIID& iid = NS_GET_IID(nsISupports);

    return nsnull != (xpc = nsXPConnect::GetXPConnect()) &&
           NS_SUCCEEDED(xpc->WrapJS(ccx, JSVAL_TO_OBJECT(mJSVal),
                        iid, getter_AddRefs(wrapper))) &&
           NS_SUCCEEDED(nsVariant::SetFromInterface(&mData, iid, wrapper));
}

// static 
JSBool 
XPCVariant::VariantDataToJS(XPCCallContext& ccx, 
                            nsIVariant* variant,
                            JSObject* scope, nsresult* pErr,
                            jsval* pJSVal)
{
    // Get the type early because we might need to spoof it below.
    PRUint16 type;
    if(NS_FAILED(variant->GetDataType(&type)))
        return JS_FALSE;

    nsCOMPtr<XPCVariant> xpcvariant = do_QueryInterface(variant);
    if(xpcvariant)
    {
        jsval realVal = xpcvariant->GetJSVal();
        if(JSVAL_IS_PRIMITIVE(realVal) || type == nsIDataType::TYPE_ID)
        {
            // Not a JSObject (or is a JSObject representing an nsID),.
            // So, just pass through the underlying data.
            *pJSVal = realVal;
            return JS_TRUE;
        }

        // XXX Need to deal with the Array case!
        
        // else, it's an object and we really need to double wrap it if we've 
        // already decided that its 'natural' type is as some sort of interface.
        
        // We just fall through to the code below and let it do what it does.
    }

    // The nsIVariant is not a XPCVariant (or we act like it isn't).
    // So we extract the data and do the Right Thing.
    
    // We ASSUME that the variant implementation can do these conversions...

    nsXPTCVariant xpctvar;
    nsID iid;
    nsAutoString astring;
    PRUint32 size;
    xpctvar.flags = 0;

    switch(type)
    {
        case nsIDataType::TYPE_INT8:        
        case nsIDataType::TYPE_INT16:        
        case nsIDataType::TYPE_INT32:        
        case nsIDataType::TYPE_INT64:        
        case nsIDataType::TYPE_UINT8:        
        case nsIDataType::TYPE_UINT16:        
        case nsIDataType::TYPE_UINT32:        
        case nsIDataType::TYPE_UINT64:        
        case nsIDataType::TYPE_FLOAT:        
        case nsIDataType::TYPE_DOUBLE:        
        {
            // Easy. Handle inline.
            if(NS_FAILED(variant->GetAsDouble(&xpctvar.val.d)))
                return JS_FALSE;
            return JS_NewNumberValue(ccx, xpctvar.val.d, pJSVal);
        }
        case nsIDataType::TYPE_BOOL:        
        {
            // Easy. Handle inline.
            if(NS_FAILED(variant->GetAsBool(&xpctvar.val.b)))
                return JS_FALSE;
            *pJSVal = BOOLEAN_TO_JSVAL(xpctvar.val.b);
            return JS_TRUE;
        }
        case nsIDataType::TYPE_CHAR: 
            if(NS_FAILED(variant->GetAsChar(&xpctvar.val.c)))
                return JS_FALSE;
            xpctvar.type = (uint8)TD_CHAR;
            break;
        case nsIDataType::TYPE_WCHAR:        
            if(NS_FAILED(variant->GetAsWChar(&xpctvar.val.wc)))
                return JS_FALSE;
            xpctvar.type = (uint8)TD_WCHAR;
            break;
        case nsIDataType::TYPE_ID:        
            if(NS_FAILED(variant->GetAsID(&iid)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_PNSIID | XPT_TDP_POINTER);
            xpctvar.val.p = &iid;
            break;
        case nsIDataType::TYPE_ASTRING:        
            if(NS_FAILED(variant->GetAsAString(astring)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_DOMSTRING | XPT_TDP_POINTER);
            xpctvar.val.p = &astring;
            break;
        case nsIDataType::TYPE_CHAR_STR:       
            if(NS_FAILED(variant->GetAsString((char**)&xpctvar.val.p)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_PSTRING | XPT_TDP_POINTER);
            xpctvar.SetValIsAllocated();
            break;
        case nsIDataType::TYPE_STRING_SIZE_IS:
            if(NS_FAILED(variant->GetAsStringWithSize(&size, 
                                                      (char**)&xpctvar.val.p)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_PSTRING_SIZE_IS | XPT_TDP_POINTER);
            break;
        case nsIDataType::TYPE_WCHAR_STR:        
            if(NS_FAILED(variant->GetAsWString((PRUnichar**)&xpctvar.val.p)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_PWSTRING | XPT_TDP_POINTER);
            xpctvar.SetValIsAllocated();
            break;
        case nsIDataType::TYPE_WSTRING_SIZE_IS:        
            if(NS_FAILED(variant->GetAsWStringWithSize(&size, 
                                                      (PRUnichar**)&xpctvar.val.p)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_PWSTRING_SIZE_IS | XPT_TDP_POINTER);
            break;
        case nsIDataType::TYPE_INTERFACE:        
        case nsIDataType::TYPE_INTERFACE_IS:        
        {
            nsID* piid;
            if(NS_FAILED(variant->GetAsInterface(&piid, &xpctvar.val.p)))
                return JS_FALSE;

            iid = *piid;
            nsMemory::Free((char*)piid);

            xpctvar.type = (uint8)(TD_INTERFACE_IS_TYPE | XPT_TDP_POINTER);
            if(xpctvar.val.p)
                xpctvar.SetValIsInterface();
            break;
        }
        case nsIDataType::TYPE_ARRAY:        
            // XXX FIXME
            return JS_FALSE;
        case nsIDataType::TYPE_VOID:        
        case nsIDataType::TYPE_EMPTY:
            *pJSVal = JSVAL_VOID;
            return JS_TRUE;
        default:
            NS_ERROR("bad type in variant!");
            return JS_FALSE;
    }

    // If we are here then we need to convert the data in the xpctvar.

    PRBool success;
    
    if(xpctvar.type.TagPart() == TD_PSTRING_SIZE_IS ||
       xpctvar.type.TagPart() == TD_PWSTRING_SIZE_IS)
    {
        success = XPCConvert::NativeStringWithSize2JS(ccx, pJSVal,
                                                      (const void*)&xpctvar.val,
                                                      xpctvar.type,
                                                      size, pErr);
    }
    else
    {
        success = XPCConvert::NativeData2JS(ccx, pJSVal,
                                            (const void*)&xpctvar.val,
                                            xpctvar.type,
                                            &iid, scope, pErr);
    }

    if(xpctvar.IsValAllocated())
        nsMemory::Free((char*)xpctvar.val.p);
    else if(xpctvar.IsValInterface())
        ((nsISupports*)xpctvar.val.p)->Release();

    return success;
}

/***************************************************************************/
/***************************************************************************/
// XXX These default implementations need to be improved to allow for
// some more interesting conversions.


/* readonly attribute PRUint16 dataType; */
NS_IMETHODIMP XPCVariant::GetDataType(PRUint16 *aDataType)
{
    *aDataType = mData.mType;
    return NS_OK;
}

/* PRUint8 getAsInt8 (); */
NS_IMETHODIMP XPCVariant::GetAsInt8(PRUint8 *_retval)
{
    return nsVariant::ConvertToInt8(mData, _retval);
}

/* PRInt16 getAsInt16 (); */
NS_IMETHODIMP XPCVariant::GetAsInt16(PRInt16 *_retval)
{
    return nsVariant::ConvertToInt16(mData, _retval);
}

/* PRInt32 getAsInt32 (); */
NS_IMETHODIMP XPCVariant::GetAsInt32(PRInt32 *_retval)
{
    return nsVariant::ConvertToInt32(mData, _retval);
}

/* PRInt64 getAsInt64 (); */
NS_IMETHODIMP XPCVariant::GetAsInt64(PRInt64 *_retval)
{
    return nsVariant::ConvertToInt64(mData, _retval);
}

/* PRUint8 getAsUint8 (); */
NS_IMETHODIMP XPCVariant::GetAsUint8(PRUint8 *_retval)
{
    return nsVariant::ConvertToUint8(mData, _retval);
}

/* PRUint16 getAsUint16 (); */
NS_IMETHODIMP XPCVariant::GetAsUint16(PRUint16 *_retval)
{
    return nsVariant::ConvertToUint16(mData, _retval);
}

/* PRUint32 getAsUint32 (); */
NS_IMETHODIMP XPCVariant::GetAsUint32(PRUint32 *_retval)
{
    return nsVariant::ConvertToUint32(mData, _retval);
}

/* PRUint64 getAsUint64 (); */
NS_IMETHODIMP XPCVariant::GetAsUint64(PRUint64 *_retval)
{
    return nsVariant::ConvertToUint64(mData, _retval);
}

/* float getAsFloat (); */
NS_IMETHODIMP XPCVariant::GetAsFloat(float *_retval)
{
    return nsVariant::ConvertToFloat(mData, _retval);
}

/* double getAsDouble (); */
NS_IMETHODIMP XPCVariant::GetAsDouble(double *_retval)
{
    return nsVariant::ConvertToDouble(mData, _retval);
}

/* PRBool getAsBool (); */
NS_IMETHODIMP XPCVariant::GetAsBool(PRBool *_retval)
{
    return nsVariant::ConvertToBool(mData, _retval);
}

/* char getAsChar (); */
NS_IMETHODIMP XPCVariant::GetAsChar(char *_retval)
{
    return nsVariant::ConvertToChar(mData, _retval);
}

/* wchar getAsWChar (); */
NS_IMETHODIMP XPCVariant::GetAsWChar(PRUnichar *_retval)
{
    return nsVariant::ConvertToWChar(mData, _retval);
}

/* [notxpcom] nsresult getAsID (out nsID retval); */
NS_IMETHODIMP_(nsresult) XPCVariant::GetAsID(nsID *retval)
{
    return nsVariant::ConvertToID(mData, retval);
}

/* AString getAsAString (); */
NS_IMETHODIMP XPCVariant::GetAsAString(nsAWritableString & _retval)
{
    return nsVariant::ConvertToAString(mData, _retval);
}

/* string getAsString (); */
NS_IMETHODIMP XPCVariant::GetAsString(char **_retval)
{
    return nsVariant::ConvertToString(mData, _retval);
}

/* wstring getAsWString (); */
NS_IMETHODIMP XPCVariant::GetAsWString(PRUnichar **_retval)
{
    return nsVariant::ConvertToWString(mData, _retval);
}

/* nsISupports getAsISupports (); */
NS_IMETHODIMP XPCVariant::GetAsISupports(nsISupports **_retval)
{
    return nsVariant::ConvertToISupports(mData, _retval);
}

/* void getAsInterface (out nsIIDPtr iid, [iid_is (iid), retval] out nsQIResult iface); */
NS_IMETHODIMP XPCVariant::GetAsInterface(nsIID * *iid, void * *iface)
{
    return nsVariant::ConvertToInterface(mData, iid, iface);
}


/* [notxpcom] nsresult getAsArray (out PRUint16 type, out nsIID iid, out PRUint32 count, out voidPtr ptr); */
NS_IMETHODIMP_(nsresult) XPCVariant::GetAsArray(PRUint16 *type, nsIID *iid, PRUint32 *count, void * *ptr)
{
    return nsVariant::ConvertToArray(mData, type, iid, count, ptr);
}

/* void getAsStringWithSize (out PRUint32 size, [size_is (size), retval] out string str); */
NS_IMETHODIMP XPCVariant::GetAsStringWithSize(PRUint32 *size, char **str)
{
    return nsVariant::ConvertToStringWithSize(mData, size, str);
}

/* void getAsWStringWithSize (out PRUint32 size, [size_is (size), retval] out wstring str); */
NS_IMETHODIMP XPCVariant::GetAsWStringWithSize(PRUint32 *size, PRUnichar **str)
{
    return nsVariant::ConvertToWStringWithSize(mData, size, str);
}


