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
#include "XPCWrapper.h"

NS_IMPL_CYCLE_COLLECTION_CLASS(XPCVariant)

NS_IMPL_CLASSINFO(XPCVariant, NULL, 0, XPCVARIANT_CID)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XPCVariant)
  NS_INTERFACE_MAP_ENTRY(XPCVariant)
  NS_INTERFACE_MAP_ENTRY(nsIVariant)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_IMPL_QUERY_CLASSINFO(XPCVariant)
NS_INTERFACE_MAP_END
NS_IMPL_CI_INTERFACE_GETTER2(XPCVariant, XPCVariant, nsIVariant)

NS_IMPL_CYCLE_COLLECTING_ADDREF(XPCVariant)
NS_IMPL_CYCLE_COLLECTING_RELEASE(XPCVariant)

XPCVariant::XPCVariant(XPCCallContext& ccx, jsval aJSVal)
    : mJSVal(aJSVal)
{
    nsVariant::Initialize(&mData);
    if(!JSVAL_IS_PRIMITIVE(mJSVal))
    {
        JSObject *obj = JSVAL_TO_OBJECT(mJSVal);
        OBJ_TO_INNER_OBJECT(ccx, obj);

        mJSVal = OBJECT_TO_JSVAL(obj);

        // If the incoming object is an XPCWrappedNative, then it could be a
        // double-wrapped object, and we should return the double-wrapped
        // object back out to script.

        JSObject* proto;
        XPCWrappedNative* wn =
            XPCWrappedNative::GetWrappedNativeOfJSObject(ccx,
                                                         JSVAL_TO_OBJECT(mJSVal),
                                                         nsnull,
                                                         &proto);
        mReturnRawObject = !wn && !proto;
    }
    else
        mReturnRawObject = JS_FALSE;
}

XPCTraceableVariant::~XPCTraceableVariant()
{
    jsval val = GetJSValPreserveColor();

    NS_ASSERTION(JSVAL_IS_GCTHING(val), "Must be traceable or unlinked");

    // If val is JSVAL_STRING, we don't need to clean anything up; simply
    // removing the string from the root set is good.
    if(!JSVAL_IS_STRING(val))
        nsVariant::Cleanup(&mData);

    if (!JSVAL_IS_NULL(val))
        RemoveFromRootSet(nsXPConnect::GetRuntimeInstance()->GetMapLock());
}

void XPCTraceableVariant::TraceJS(JSTracer* trc)
{
    jsval val = GetJSValPreserveColor();

    NS_ASSERTION(JSVAL_IS_TRACEABLE(val), "Must be traceable");
    JS_SET_TRACING_DETAILS(trc, PrintTraceName, this, 0);
    JS_CallTracer(trc, JSVAL_TO_TRACEABLE(val), JSVAL_TRACE_KIND(val));
}

#ifdef DEBUG
// static
void
XPCTraceableVariant::PrintTraceName(JSTracer* trc, char *buf, size_t bufsize)
{
    JS_snprintf(buf, bufsize, "XPCVariant[0x%p].mJSVal", trc->debugPrintArg);
}
#endif

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(XPCVariant)
    jsval val = tmp->GetJSValPreserveColor();
    if(JSVAL_IS_OBJECT(val))
        cb.NoteScriptChild(nsIProgrammingLanguage::JAVASCRIPT,
                           JSVAL_TO_OBJECT(val));

    nsVariant::Traverse(tmp->mData, cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(XPCVariant)
    jsval val = tmp->GetJSValPreserveColor();

    // We're sharing val's buffer, clear the pointer to it so Cleanup() won't
    // try to delete it
    if(JSVAL_IS_STRING(val))
        tmp->mData.u.wstr.mWStringValue = nsnull;
    nsVariant::Cleanup(&tmp->mData);

    if(JSVAL_IS_TRACEABLE(val))
    {
        XPCTraceableVariant *v = static_cast<XPCTraceableVariant*>(tmp);
        v->RemoveFromRootSet(nsXPConnect::GetRuntimeInstance()->GetMapLock());
    }
    tmp->mJSVal = JSVAL_NULL;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

// static 
XPCVariant* XPCVariant::newVariant(XPCCallContext& ccx, jsval aJSVal)
{
    XPCVariant* variant;

    if(!JSVAL_IS_TRACEABLE(aJSVal))
        variant = new XPCVariant(ccx, aJSVal);
    else
        variant = new XPCTraceableVariant(ccx, aJSVal);

    if(!variant)
        return nsnull;
    NS_ADDREF(variant);

    if(!variant->InitializeData(ccx))
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
    static const Type StateTable[tTypeCount][tTypeCount-1];

public:
    static JSBool GetTypeForArray(XPCCallContext& ccx, JSObject* array, 
                                  jsuint length, 
                                  nsXPTType* resultType, nsID* resultID);
};


// Current state is the column down the side. 
// Current type is the row along the top. 
// New state is in the box at the intersection.

const XPCArrayHomogenizer::Type 
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
    JS_CHECK_RECURSION(ccx.GetJSContext(), return JS_FALSE);

    jsval val = GetJSVal();

    if(JSVAL_IS_INT(val))
        return NS_SUCCEEDED(nsVariant::SetFromInt32(&mData, JSVAL_TO_INT(val)));
    if(JSVAL_IS_DOUBLE(val))
        return NS_SUCCEEDED(nsVariant::SetFromDouble(&mData, 
                                                     JSVAL_TO_DOUBLE(val)));
    if(JSVAL_IS_BOOLEAN(val))
        return NS_SUCCEEDED(nsVariant::SetFromBool(&mData, 
                                                   JSVAL_TO_BOOLEAN(val)));
    if(JSVAL_IS_VOID(val))
        return NS_SUCCEEDED(nsVariant::SetToVoid(&mData));
    if(JSVAL_IS_NULL(val))
        return NS_SUCCEEDED(nsVariant::SetToEmpty(&mData));
    if(JSVAL_IS_STRING(val))
    {
        // Make our string immutable.  This will also ensure null-termination,
        // which nsVariant assumes for its PRUnichar* stuff.
        JSString* str = JSVAL_TO_STRING(val);
        if(!JS_MakeStringImmutable(ccx, str))
            return JS_FALSE;

        // Don't use nsVariant::SetFromWStringWithSize, because that will copy
        // the data.  Just handle this ourselves.  Note that it's ok to not
        // copy because we added mJSVal as a GC root.
        NS_ASSERTION(mData.mType == nsIDataType::VTYPE_EMPTY,
                     "Why do we already have data?");

        // Despite the fact that the variant holds the length, there are
        // implicit assumptions that mWStringValue[mWStringLength] == 0
        size_t length;
        const jschar *chars = JS_GetStringCharsZAndLength(ccx, str, &length);
        if (!chars)
            return JS_FALSE;

        mData.u.wstr.mWStringValue = const_cast<jschar *>(chars);
        // Use C-style cast, because reinterpret cast from size_t to
        // PRUint32 is not valid on some platforms.
        mData.u.wstr.mWStringLength = (PRUint32)length;
        mData.mType = nsIDataType::VTYPE_WSTRING_SIZE_IS;
        
        return JS_TRUE;
    }

    // leaving only JSObject...
    NS_ASSERTION(JSVAL_IS_OBJECT(val), "invalid type of jsval!");
    
    JSObject* jsobj = JSVAL_TO_OBJECT(val);

    // Let's see if it is a xpcJSID.

    const nsID* id = xpc_JSObjectToID(ccx, jsobj);
    if(id)
        return NS_SUCCEEDED(nsVariant::SetFromID(&mData, *id));
    
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
                                       val, len, len,
                                       type, &id, nsnull))
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

NS_IMETHODIMP
XPCVariant::GetAsJSVal(jsval* result)
{
  NS_PRECONDITION(result, "null result arg.");
  *result = GetJSVal();
  return NS_OK;
}

// static 
JSBool 
XPCVariant::VariantDataToJS(XPCLazyCallContext& lccx, 
                            nsIVariant* variant,
                            nsresult* pErr, jsval* pJSVal)
{
    // Get the type early because we might need to spoof it below.
    PRUint16 type;
    if (NS_FAILED(variant->GetDataType(&type)))
        return JS_FALSE;

    jsval realVal;
    nsresult rv = variant->GetAsJSVal(&realVal);

    if(NS_SUCCEEDED(rv) &&
      (JSVAL_IS_PRIMITIVE(realVal) ||
       type == nsIDataType::VTYPE_ARRAY ||
       type == nsIDataType::VTYPE_EMPTY_ARRAY ||
       type == nsIDataType::VTYPE_ID))
    {
        JSContext *cx = lccx.GetJSContext();
        if(!JS_WrapValue(cx, &realVal))
            return JS_FALSE;
        *pJSVal = realVal;
        return JS_TRUE;
    }

    nsCOMPtr<XPCVariant> xpcvariant = do_QueryInterface(variant);
    if(xpcvariant && xpcvariant->mReturnRawObject)
    {
        NS_ASSERTION(type == nsIDataType::VTYPE_INTERFACE ||
                     type == nsIDataType::VTYPE_INTERFACE_IS,
                     "Weird variant");

        JSContext *cx = lccx.GetJSContext();
        if(!JS_WrapValue(cx, &realVal))
            return JS_FALSE;
        *pJSVal = realVal;
        return JS_TRUE;
    }

    // else, it's an object and we really need to double wrap it if we've 
    // already decided that its 'natural' type is as some sort of interface.

    // We just fall through to the code below and let it do what it does.

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

    JSContext* cx = lccx.GetJSContext();
    NS_ABORT_IF_FALSE(lccx.GetScopeForNewJSObjects()->compartment() == cx->compartment,
                      "bad scope for new JSObjects");

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
            return JS_NewNumberValue(cx, xpctvar.val.d, pJSVal);
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
            xpctvar.SetValNeedsCleanup();
            break;
        case nsIDataType::VTYPE_STRING_SIZE_IS:
            if(NS_FAILED(variant->GetAsStringWithSize(&size, 
                                                      (char**)&xpctvar.val.p)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_PSTRING_SIZE_IS | XPT_TDP_POINTER);
            xpctvar.SetValNeedsCleanup();
            break;
        case nsIDataType::VTYPE_WCHAR_STR:        
            if(NS_FAILED(variant->GetAsWString((PRUnichar**)&xpctvar.val.p)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_PWSTRING | XPT_TDP_POINTER);
            xpctvar.SetValNeedsCleanup();
            break;
        case nsIDataType::VTYPE_WSTRING_SIZE_IS:        
            if(NS_FAILED(variant->GetAsWStringWithSize(&size, 
                                                      (PRUnichar**)&xpctvar.val.p)))
                return JS_FALSE;
            xpctvar.type = (uint8)(TD_PWSTRING_SIZE_IS | XPT_TDP_POINTER);
            xpctvar.SetValNeedsCleanup();
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
                xpctvar.SetValNeedsCleanup();
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
                XPCConvert::NativeArray2JS(lccx, pJSVal, 
                                           (const void**)&du.u.array.mArrayValue,
                                           conversionType, pid,
                                           du.u.array.mArrayCount, pErr);

VARIANT_DONE:                                
            nsVariant::Cleanup(&du);
            return success;
        }        
        case nsIDataType::VTYPE_EMPTY_ARRAY: 
        {
            JSObject* array = JS_NewArrayObject(cx, 0, nsnull);
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
        success = XPCConvert::NativeStringWithSize2JS(cx, pJSVal,
                                                      (const void*)&xpctvar.val,
                                                      xpctvar.type,
                                                      size, pErr);
    }
    else
    {
        success = XPCConvert::NativeData2JS(lccx, pJSVal,
                                            (const void*)&xpctvar.val,
                                            xpctvar.type,
                                            &iid, pErr);
    }

    // We may have done something in the above code that requires cleanup.
    if (xpctvar.DoesValNeedCleanup())
    {
        if (type == nsIDataType::VTYPE_INTERFACE ||
            type == nsIDataType::VTYPE_INTERFACE_IS)
            ((nsISupports*)xpctvar.val.p)->Release();
        else
            nsMemory::Free((char*)xpctvar.val.p);
    }

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


