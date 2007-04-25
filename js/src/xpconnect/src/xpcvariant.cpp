/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* nsIVariant implementation for xpconnect. */

#include "xpcprivate.h"

NS_IMPL_CYCLE_COLLECTION_CLASS(XPCVariant)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XPCVariant)
  NS_INTERFACE_MAP_ENTRY(XPCVariant)
  NS_INTERFACE_MAP_ENTRY(nsIVariant)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_IMPL_QUERY_CLASSINFO(XPCVariant)
NS_INTERFACE_MAP_END
NS_IMPL_CI_INTERFACE_GETTER2(XPCVariant, XPCVariant, nsIVariant)

NS_IMPL_CYCLE_COLLECTING_ADDREF(XPCVariant)
NS_IMPL_CYCLE_COLLECTING_RELEASE(XPCVariant)

XPCVariant::XPCVariant(JSRuntime* aJSRuntime)
    : mJSVal(JSVAL_VOID),
      mJSRuntime(aJSRuntime)
{
    nsVariant::Initialize(&mData);
}

XPCVariant::~XPCVariant()
{
    // If mJSVal is JSVAL_STRING, we don't need to clean anything up;
    // simply unlocking the string is good.
    if(!JSVAL_IS_STRING(mJSVal))
        nsVariant::Cleanup(&mData);
    
    if(JSVAL_IS_GCTHING(mJSVal))
    {
        NS_ASSERTION(mJSRuntime, "Must have a runtime!");
#ifdef GC_MARK_DEBUG
        JS_RemoveRootRT(mJSRuntime, &mJSVal);
        JS_smprintf_free(mGCRootName);
        mGCRootName = nsnull;
#else
        JS_UnlockGCThingRT(mJSRuntime, JSVAL_TO_GCTHING(mJSVal));
#endif
    }
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(XPCVariant)
    if(JSVAL_IS_OBJECT(tmp->mJSVal))
        cb.NoteScriptChild(nsIProgrammingLanguage::JAVASCRIPT,
                           JSVAL_TO_OBJECT(tmp->mJSVal));

    nsVariant::Traverse(tmp->mData, cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// NB: We might unlink our outgoing references in the future; for now we do
// nothing. This is a harmless conservative behavior; it just means that we rely
// on the cycle being broken by some of the external XPCOM objects' unlink()
// methods, not our own. Typically *any* unlinking will break the cycle.
NS_IMPL_CYCLE_COLLECTION_UNLINK_0(XPCVariant)

// static 
XPCVariant* XPCVariant::newVariant(XPCCallContext& ccx, jsval aJSVal)
{
    XPCVariant* variant = new XPCVariant(JS_GetRuntime(ccx));
    if(!variant)
        return nsnull;
    
    NS_ADDREF(variant);

    variant->mJSVal = aJSVal;

    if(JSVAL_IS_GCTHING(variant->mJSVal))
    {
        PRBool ok;
#ifdef GC_MARK_DEBUG
        variant->mGCRootName = JS_smprintf("XPCVariant::mJSVal[0x%p]", variant);
        ok = JS_AddNamedRoot(ccx, &variant->mJSVal, variant->mGCRootName);
#else
        // use JS_LockGCThingRT, because we get better performance in a lot of
        // cases than with adding a named GC root.
        ok = JS_LockGCThing(ccx, JSVAL_TO_GCTHING(variant->mJSVal));
#endif
        if(!ok)
        {
            NS_RELEASE(variant); // Also sets variant to nsnull.
        }
    }

    if(variant && !variant->InitializeData(ccx))
        NS_RELEASE(variant);     // Also sets variant to nsnull.

    return variant;
}


// Helper class to give us a namespace for the table based code below.
class XPCArrayHomogenizer
{
private:
    enum Type
    {
        tNull  = 0 ,  // null value
        tInt       ,  // Integer
        tDbl       ,  // Double
        tBool      ,  // Boolean
        tStr       ,  // String
        tID        ,  // ID
        tArr       ,  // Array
        tISup      ,  // nsISupports (really just a plain JSObject)
        tUnk       ,  // Unknown. Used only for initial state.

        tTypeCount ,  // Just a count for table dimensioning.

        tVar       ,  // nsVariant - last ditch if no other common type found.
        tErr          // No valid state or type has this value. 
    };

    // Table has tUnk as a state (column) but not as a type (row).
    static Type StateTable[tTypeCount][tTypeCount-1];

public:
    static JSBool GetTypeForArray(XPCCallContext& ccx, JSObject* array, 
                                  jsuint length, 
                                  nsXPTType* resultType, nsID* resultID);
};


// Current state is the column down the side. 
// Current type is the row along the top. 
// New state is in the box at the intersection.

XPCArrayHomogenizer::Type 
XPCArrayHomogenizer::StateTable[tTypeCount][tTypeCount-1] = {
/*          tNull,tInt ,tDbl ,tBool,tStr ,tID  ,tArr ,tISup */
/* tNull */{tNull,tVar ,tVar ,tVar ,tStr ,tID  ,tVar ,tISup },
/* tInt  */{tVar ,tInt ,tDbl ,tVar ,tVar ,tVar ,tVar ,tVar  },
/* tDbl  */{tVar ,tDbl ,tDbl ,tVar ,tVar ,tVar ,tVar ,tVar  },
/* tBool */{tVar ,tVar ,tVar ,tBool,tVar ,tVar ,tVar ,tVar  },
/* tStr  */{tStr ,tVar ,tVar ,tVar ,tStr ,tVar ,tVar ,tVar  },
/* tID   */{tID  ,tVar ,tVar ,tVar ,tVar ,tID  ,tVar ,tVar  },
/* tArr  */{tErr ,tErr ,tErr ,tErr ,tErr ,tErr ,tErr ,tErr  },
/* tISup */{tISup,tVar ,tVar ,tVar ,tVar ,tVar ,tVar ,tISup },
/* tUnk  */{tNull,tInt ,tDbl ,tBool,tStr ,tID  ,tVar ,tISup }};

// static
JSBool
XPCArrayHomogenizer::GetTypeForArray(XPCCallContext& ccx, JSObject* array,
                                     jsuint length, 
                                     nsXPTType* resultType, nsID* resultID)    
{
    Type state = tUnk;
    Type type;
       
    for(jsuint i = 0; i < length; i++)
    {
        jsval val;
        if(!JS_GetElement(ccx, array, i, &val))
            return JS_FALSE;
           
        if(JSVAL_IS_INT(val))
            type = tInt;
        else if(JSVAL_IS_DOUBLE(val))
            type = tDbl;
        else if(JSVAL_IS_BOOLEAN(val))
            type = tBool;
        else if(JSVAL_IS_VOID(val))
        {
            state = tVar;
            break;
        }
        else if(JSVAL_IS_NULL(val))
            type = tNull;
        else if(JSVAL_IS_STRING(val))
            type = tStr;
        else
        {
            NS_ASSERTION(JSVAL_IS_OBJECT(val), "invalid type of jsval!");
            JSObject* jsobj = JSVAL_TO_OBJECT(val);
            if(JS_IsArrayObject(ccx, jsobj))
                type = tArr;
            else if(xpc_JSObjectIsID(ccx, jsobj))
                type = tID;
            else
                type = tISup;
        }

        NS_ASSERTION(state != tErr, "bad state table!");
        NS_ASSERTION(type  != tErr, "bad type!");
        NS_ASSERTION(type  != tVar, "bad type!");
        NS_ASSERTION(type  != tUnk, "bad type!");

        state = StateTable[state][type];
        
        NS_ASSERTION(state != tErr, "bad state table!");
        NS_ASSERTION(state != tUnk, "bad state table!");
        
        if(state == tVar)
            break;
    }

    switch(state)
    {
        case tInt : 
            *resultType = nsXPTType((uint8)TD_INT32);
            break;
        case tDbl : 
            *resultType = nsXPTType((uint8)TD_DOUBLE);
            break;
        case tBool:
            *resultType = nsXPTType((uint8)TD_BOOL);
            break;
        case tStr : 
            *resultType = nsXPTType((uint8)(TD_PWSTRING | XPT_TDP_POINTER));
            break;
        case tID  : 
            *resultType = nsXPTType((uint8)(TD_PNSIID | XPT_TDP_POINTER));
            break;
        case tISup: 
            *resultType = nsXPTType((uint8)(TD_INTERFACE_IS_TYPE | XPT_TDP_POINTER));
            *resultID = NS_GET_IID(nsISupports);
            break;
        case tNull: 
            // FALL THROUGH
        case tVar :
            *resultType = nsXPTType((uint8)(TD_INTERFACE_IS_TYPE | XPT_TDP_POINTER));
            *resultID = NS_GET_IID(nsIVariant);
            break;
        case tArr : 
            // FALL THROUGH
        case tUnk : 
            // FALL THROUGH
        case tErr : 
            // FALL THROUGH
        default:
            NS_ERROR("bad state");
            return JS_FALSE;
    }
    return JS_TRUE;
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
        return NS_SUCCEEDED(nsVariant::SetToVoid(&mData));
    if(JSVAL_IS_NULL(mJSVal))
        return NS_SUCCEEDED(nsVariant::SetToEmpty(&mData));
    if(JSVAL_IS_STRING(mJSVal))
    {
        // Make our string immutable.  This will also ensure null-termination,
        // which nsVariant assumes for its PRUnichar* stuff.
        JSString* str = JSVAL_TO_STRING(mJSVal);
        if(!JS_MakeStringImmutable(ccx, str))
            return JS_FALSE;

        // Don't use nsVariant::SetFromWStringWithSize, because that will copy
        // the data.  Just handle this ourselves.  Note that it's ok to not
        // copy because we added mJSVal as a GC root.
        NS_ASSERTION(mData.mType == nsIDataType::VTYPE_EMPTY,
                     "Why do we already have data?");

        mData.u.wstr.mWStringValue = 
            NS_REINTERPRET_CAST(PRUnichar*, JS_GetStringChars(str));
        // Use C-style cast, because reinterpret cast from size_t to
        // PRUint32 is not valid on some platforms.
        mData.u.wstr.mWStringLength = (PRUint32)JS_GetStringLength(str);
        mData.mType = nsIDataType::VTYPE_WSTRING_SIZE_IS;
        
        return JS_TRUE;
    }

    // leaving only JSObject...
    NS_ASSERTION(JSVAL_IS_OBJECT(mJSVal), "invalid type of jsval!");
    
    JSObject* jsobj = JSVAL_TO_OBJECT(mJSVal);

    // Let's see if it is a xpcJSID.

    // XXX It might be nice to have a non-allocing version of xpc_JSObjectToID.
    nsID* id = xpc_JSObjectToID(ccx, jsobj);
    if(id)
    {
        JSBool success = NS_SUCCEEDED(nsVariant::SetFromID(&mData, *id));
        nsMemory::Free((char*)id);
        return success;
    }
    
    // Let's see if it is a js array object.

    jsuint len;

    if(JS_IsArrayObject(ccx, jsobj) && JS_GetArrayLength(ccx, jsobj, &len))
    {
        if(!len) 
        {
            // Zero length array
            nsVariant::SetToEmptyArray(&mData);
            return JS_TRUE;
        }
        
        nsXPTType type;
        nsID id;

        if(!XPCArrayHomogenizer::GetTypeForArray(ccx, jsobj, len, &type, &id))
            return JS_FALSE; 

        if(!XPCConvert::JSArray2Native(ccx, &mData.u.array.mArrayValue, 
                                       mJSVal, len, len,
                                       type, type.IsPointer(),
                                       &id, nsnull))
            return JS_FALSE;

        mData.mType = nsIDataType::VTYPE_ARRAY;
        if(type.IsInterfacePointer())
            mData.u.array.mArrayInterfaceID = id;
        mData.u.array.mArrayCount = len;
        mData.u.array.mArrayType = type.TagPart();
        
        return JS_TRUE;
    }    

    // XXX This could be smarter and pick some more interesting iface.

    nsXPConnect*  xpc;
    nsCOMPtr<nsISupports> wrapper;
    const nsIID& iid = NS_GET_IID(nsISupports);

    return nsnull != (xpc = nsXPConnect::GetXPConnect()) &&
           NS_SUCCEEDED(xpc->WrapJS(ccx, jsobj,
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
        if(JSVAL_IS_PRIMITIVE(realVal) || 
           type == nsIDataType::VTYPE_ARRAY ||
           type == nsIDataType::VTYPE_ID)
        {
            // Not a JSObject (or is a JSArray or is a JSObject representing 
            // an nsID),.
            // So, just pass through the underlying data.
            *pJSVal = realVal;
            return JS_TRUE;
        }

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
    nsCAutoString cString;
    nsUTF8String utf8String;
    PRUint32 size;
    xpctvar.flags = 0;
    JSBool success;

    switch(type)
    {
        case nsIDataType::VTYPE_INT8:        
        case nsIDataType::VTYPE_INT16:        
        case nsIDataType::VTYPE_INT32:        
        case nsIDataType::VTYPE_INT64:        
        case nsIDataType::VTYPE_UINT8:        
        case nsIDataType::VTYPE_UINT16:        
        case nsIDataType::VTYPE_UINT32:        
        case nsIDataType::VTYPE_UINT64:        
        case nsIDataType::VTYPE_FLOAT:        
        case nsIDataType::VTYPE_DOUBLE:        
        {
            // Easy. Handle inline.
            if(NS_FAILED(variant->GetAsDouble(&xpctvar.val.d)))
                return JS_FALSE;
            return JS_NewNumberValue(ccx, xpctvar.val.d, pJSVal);
        }
        case nsIDataType::VTYPE_BOOL:        
        {
            // Easy. Handle inline.
            if(NS_FAILED(variant->GetAsBool(&xpctvar.val.b)))
                return JS_FALSE;
            *pJSVal = BOOLEAN_TO_JSVAL(xpctvar.val.b);
            return JS_TRUE;
        }
        case nsIDataType::VTYPE_CHAR: 
            if(NS_FAILED(variant->GetAsChar(&xpctvar.val.c)))
                return JS_FALSE;
            xpctvar.type = (uint8)TD_CHAR;
            break;
        case nsIDataType::VTYPE_WCHAR:        
            if(NS_FAILED(variant->GetAsWChar(&xpctvar.val.wc)))
                return JS_FALSE;
            xpctvar.type = (uint8)TD_WCHAR;
            break;
        case nsIDataType::VTYPE_ID:        
            if(NS_FAILED(variant->GetAsID(&iid)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_PNSIID | XPT_TDP_POINTER);
            xpctvar.val.p = &iid;
            break;
        case nsIDataType::VTYPE_ASTRING:        
            if(NS_FAILED(variant->GetAsAString(astring)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_ASTRING | XPT_TDP_POINTER);
            xpctvar.val.p = &astring;
            break;
        case nsIDataType::VTYPE_DOMSTRING:
            if(NS_FAILED(variant->GetAsAString(astring)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_DOMSTRING | XPT_TDP_POINTER);
            xpctvar.val.p = &astring;
            break;
        case nsIDataType::VTYPE_CSTRING:            
            if(NS_FAILED(variant->GetAsACString(cString)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_CSTRING | XPT_TDP_POINTER);
            xpctvar.val.p = &cString;
            break;
        case nsIDataType::VTYPE_UTF8STRING:            
            if(NS_FAILED(variant->GetAsAUTF8String(utf8String)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_UTF8STRING | XPT_TDP_POINTER);
            xpctvar.val.p = &utf8String;
            break;       
        case nsIDataType::VTYPE_CHAR_STR:       
            if(NS_FAILED(variant->GetAsString((char**)&xpctvar.val.p)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_PSTRING | XPT_TDP_POINTER);
            xpctvar.SetValIsAllocated();
            break;
        case nsIDataType::VTYPE_STRING_SIZE_IS:
            if(NS_FAILED(variant->GetAsStringWithSize(&size, 
                                                      (char**)&xpctvar.val.p)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_PSTRING_SIZE_IS | XPT_TDP_POINTER);
            break;
        case nsIDataType::VTYPE_WCHAR_STR:        
            if(NS_FAILED(variant->GetAsWString((PRUnichar**)&xpctvar.val.p)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_PWSTRING | XPT_TDP_POINTER);
            xpctvar.SetValIsAllocated();
            break;
        case nsIDataType::VTYPE_WSTRING_SIZE_IS:        
            if(NS_FAILED(variant->GetAsWStringWithSize(&size, 
                                                      (PRUnichar**)&xpctvar.val.p)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_PWSTRING_SIZE_IS | XPT_TDP_POINTER);
            break;
        case nsIDataType::VTYPE_INTERFACE:        
        case nsIDataType::VTYPE_INTERFACE_IS:        
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
        case nsIDataType::VTYPE_ARRAY:
        {
            nsDiscriminatedUnion du;
            nsVariant::Initialize(&du);
            nsresult rv;

            rv = variant->GetAsArray(&du.u.array.mArrayType,
                                     &du.u.array.mArrayInterfaceID,
                                     &du.u.array.mArrayCount,
                                     &du.u.array.mArrayValue);
            if(NS_FAILED(rv))
                return JS_FALSE;
        
            // must exit via VARIANT_DONE from here on...
            du.mType = nsIDataType::VTYPE_ARRAY;
            success = JS_FALSE;

            nsXPTType conversionType;
            PRUint16 elementType = du.u.array.mArrayType;
            const nsID* pid = nsnull;

            switch(elementType)
            {
                case nsIDataType::VTYPE_INT8:        
                case nsIDataType::VTYPE_INT16:        
                case nsIDataType::VTYPE_INT32:        
                case nsIDataType::VTYPE_INT64:        
                case nsIDataType::VTYPE_UINT8:        
                case nsIDataType::VTYPE_UINT16:        
                case nsIDataType::VTYPE_UINT32:        
                case nsIDataType::VTYPE_UINT64:        
                case nsIDataType::VTYPE_FLOAT:        
                case nsIDataType::VTYPE_DOUBLE:        
                case nsIDataType::VTYPE_BOOL:        
                case nsIDataType::VTYPE_CHAR:        
                case nsIDataType::VTYPE_WCHAR:        
                    conversionType = nsXPTType((uint8)elementType);
                    break;

                case nsIDataType::VTYPE_ID:        
                case nsIDataType::VTYPE_CHAR_STR:        
                case nsIDataType::VTYPE_WCHAR_STR:        
                    conversionType = nsXPTType((uint8)elementType | XPT_TDP_POINTER);
                    break;

                case nsIDataType::VTYPE_INTERFACE:        
                    pid = &NS_GET_IID(nsISupports);
                    conversionType = nsXPTType((uint8)elementType | XPT_TDP_POINTER);
                    break;

                case nsIDataType::VTYPE_INTERFACE_IS:        
                    pid = &du.u.array.mArrayInterfaceID;
                    conversionType = nsXPTType((uint8)elementType | XPT_TDP_POINTER);
                    break;

                // The rest are illegal.
                case nsIDataType::VTYPE_VOID:        
                case nsIDataType::VTYPE_ASTRING:        
                case nsIDataType::VTYPE_DOMSTRING:        
                case nsIDataType::VTYPE_CSTRING:        
                case nsIDataType::VTYPE_UTF8STRING:        
                case nsIDataType::VTYPE_WSTRING_SIZE_IS:        
                case nsIDataType::VTYPE_STRING_SIZE_IS:        
                case nsIDataType::VTYPE_ARRAY:
                case nsIDataType::VTYPE_EMPTY_ARRAY:
                case nsIDataType::VTYPE_EMPTY:
                default:
                    NS_ERROR("bad type in array!");
                    goto VARIANT_DONE;
            }

            success = 
                XPCConvert::NativeArray2JS(ccx, pJSVal, 
                                           (const void**)&du.u.array.mArrayValue,
                                           conversionType, pid,
                                           du.u.array.mArrayCount, 
                                           scope, pErr);

VARIANT_DONE:                                
            nsVariant::Cleanup(&du);
            return success;
        }        
        case nsIDataType::VTYPE_EMPTY_ARRAY: 
        {
            JSObject* array = JS_NewArrayObject(ccx, 0, nsnull);
            if(!array) 
                return JS_FALSE;
            *pJSVal = OBJECT_TO_JSVAL(array);
            return JS_TRUE;
        }
        case nsIDataType::VTYPE_VOID:        
            *pJSVal = JSVAL_VOID;
            return JS_TRUE;
        case nsIDataType::VTYPE_EMPTY:
            *pJSVal = JSVAL_NULL;
            return JS_TRUE;
        default:
            NS_ERROR("bad type in variant!");
            return JS_FALSE;
    }

    // If we are here then we need to convert the data in the xpctvar.
    
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
NS_IMETHODIMP XPCVariant::GetAsAString(nsAString & _retval)
{
    return nsVariant::ConvertToAString(mData, _retval);
}

/* DOMString getAsDOMString (); */
NS_IMETHODIMP XPCVariant::GetAsDOMString(nsAString & _retval)
{
    // A DOMString maps to an AString internally, so we can re-use
    // ConvertToAString here.
    return nsVariant::ConvertToAString(mData, _retval);
}

/* ACString getAsACString (); */
NS_IMETHODIMP XPCVariant::GetAsACString(nsACString & _retval)
{
    return nsVariant::ConvertToACString(mData, _retval);
}

/* AUTF8String getAsAUTF8String (); */
NS_IMETHODIMP XPCVariant::GetAsAUTF8String(nsAUTF8String & _retval)
{
    return nsVariant::ConvertToAUTF8String(mData, _retval);
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


