/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   John Bandhauer <jband@netscape.com>
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

/* implement nsITestVariant for testing. */

#include "xpctest_private.h"
#include "nsString.h"

class nsTestVariant : public nsITestVariant
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITESTVARIANT

  nsTestVariant();
  virtual ~nsTestVariant();
};

NS_IMPL_ISUPPORTS1(nsTestVariant, nsITestVariant)

nsTestVariant::nsTestVariant()
{
}

nsTestVariant::~nsTestVariant()
{
}

/* nsIVariant passThruVariant (in nsIVariant value); */
NS_IMETHODIMP nsTestVariant::PassThruVariant(nsIVariant *value, nsIVariant **_retval)
{
    *_retval = value;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}

/* PRUint16 returnVariantType (in nsIVariant value); */
NS_IMETHODIMP nsTestVariant::ReturnVariantType(nsIVariant *value, PRUint16 *_retval)
{
    return value->GetDataType(_retval);
}

#define MEMBER_COPY(type_)                                                    \
    rv = inVar->GetAs##type_(&du.u.m##type_##Value);                          \
    if(NS_FAILED(rv)) return rv;                                              \
    rv = outVar->SetAs##type_(du.u.m##type_##Value);                          \
    NS_ENSURE_SUCCESS(rv,rv);

#define MEMBER_COPY_CAST(type_, cast_)                                        \
    rv = inVar->GetAs##type_( (cast_*) &du.u.m##type_##Value);                \
    if(NS_FAILED(rv)) return rv;                                              \
    rv = outVar->SetAs##type_( (cast_) du.u.m##type_##Value);                 \
    NS_ENSURE_SUCCESS(rv,rv);

static nsresult ConvertAndCopyVariant(nsIVariant *inVar, PRUint16 type, nsIVariant **_retval)
{
    nsresult rv;
    
    nsCOMPtr<nsIWritableVariant> outVar;
    outVar = do_CreateInstance("@mozilla.org/variant;1");
    if(!outVar)
        return NS_ERROR_FAILURE;

    PRUint16 inVarType;
    rv = inVar->GetDataType(&inVarType);
    if(NS_FAILED(rv))
        return rv;

    nsDiscriminatedUnion du;
    nsVariant::Initialize(&du);

    switch(type)
    {
    case nsIDataType::VTYPE_INT8:
        MEMBER_COPY_CAST(Int8, PRUint8)
        break;
    case nsIDataType::VTYPE_INT16:
        MEMBER_COPY(Int16)
        break;
    case nsIDataType::VTYPE_INT32:        
        MEMBER_COPY(Int32)
        break;
    case nsIDataType::VTYPE_INT64:        
        MEMBER_COPY(Int64)
        break;
    case nsIDataType::VTYPE_UINT8:        
        MEMBER_COPY(Uint8)
        break;
    case nsIDataType::VTYPE_UINT16:        
        MEMBER_COPY(Uint16)
        break;
    case nsIDataType::VTYPE_UINT32:        
        MEMBER_COPY(Uint32)
        break;
    case nsIDataType::VTYPE_UINT64:        
        MEMBER_COPY(Uint64)
        break;
    case nsIDataType::VTYPE_FLOAT:        
        MEMBER_COPY(Float)
        break;
    case nsIDataType::VTYPE_DOUBLE:        
        MEMBER_COPY(Double)
        break;
    case nsIDataType::VTYPE_BOOL:        
        MEMBER_COPY(Bool)
        break;
    case nsIDataType::VTYPE_CHAR:        
        MEMBER_COPY(Char)
        break;
    case nsIDataType::VTYPE_WCHAR:        
        MEMBER_COPY(WChar)
        break;
    case nsIDataType::VTYPE_VOID:        
        if(inVarType != nsIDataType::VTYPE_VOID)
            return NS_ERROR_CANNOT_CONVERT_DATA;
        rv = outVar->SetAsVoid();
        NS_ENSURE_SUCCESS(rv,rv);
        break;
    case nsIDataType::VTYPE_ID:        
        MEMBER_COPY(ID)
        break;
    case nsIDataType::VTYPE_ASTRING:        
    case nsIDataType::VTYPE_DOMSTRING:
    {
        nsAutoString str;
        rv = inVar->GetAsAString(str);
        if(NS_FAILED(rv)) return rv;
        rv = outVar->SetAsAString(str);
        NS_ENSURE_SUCCESS(rv,rv);
        break;
    }
    case nsIDataType::VTYPE_UTF8STRING:
    {
        nsUTF8String str;
        rv = inVar->GetAsAUTF8String(str);
        if(NS_FAILED(rv)) return rv;
        rv = outVar->SetAsAUTF8String(str);
        NS_ENSURE_SUCCESS(rv,rv);
        break;
    }
    case nsIDataType::VTYPE_CSTRING:
    {
        nsCAutoString str;
        rv = inVar->GetAsACString(str);
        if(NS_FAILED(rv)) return rv;
        rv = outVar->SetAsACString(str);
        NS_ENSURE_SUCCESS(rv,rv);
        break;
    }    
    case nsIDataType::VTYPE_CHAR_STR:        
    {
        char* str;
        rv = inVar->GetAsString(&str);
        if(NS_FAILED(rv)) return rv;
        rv = outVar->SetAsString(str);
        if(str) nsMemory::Free(str);
        NS_ENSURE_SUCCESS(rv,rv);
        break;
    }
    case nsIDataType::VTYPE_STRING_SIZE_IS:        
    {
        char* str;
        PRUint32 size;
        rv = inVar->GetAsStringWithSize(&size, &str);
        if(NS_FAILED(rv)) return rv;
        rv = outVar->SetAsStringWithSize(size, str);
        if(str) nsMemory::Free(str);
        NS_ENSURE_SUCCESS(rv,rv);
        break;
    }
    case nsIDataType::VTYPE_WCHAR_STR:        
    {
        PRUnichar* str;
        rv = inVar->GetAsWString(&str);
        if(NS_FAILED(rv)) return rv;
        rv = outVar->SetAsWString(str);
        if(str) nsMemory::Free((char*)str);
        NS_ENSURE_SUCCESS(rv,rv);
        break;
    }
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:        
    {
        PRUnichar* str;
        PRUint32 size;
        rv = inVar->GetAsWStringWithSize(&size, &str);
        if(NS_FAILED(rv)) return rv;
        rv = outVar->SetAsWStringWithSize(size, str);
        if(str) nsMemory::Free((char*)str);
        NS_ENSURE_SUCCESS(rv,rv);
        break;
    }
    case nsIDataType::VTYPE_INTERFACE:        
    {
        nsISupports* ptr;
        rv = inVar->GetAsISupports(&ptr);
        if(NS_FAILED(rv)) return rv;
        rv = outVar->SetAsISupports(ptr);
        NS_IF_RELEASE(ptr);
        NS_ENSURE_SUCCESS(rv,rv);
        break;
    }
    case nsIDataType::VTYPE_INTERFACE_IS:        
    {
        nsISupports* ptr;
        nsIID* iid;
        rv = inVar->GetAsInterface(&iid, (void**)&ptr);
        if(NS_FAILED(rv)) return rv;
        rv = outVar->SetAsInterface(*iid, ptr);
        NS_IF_RELEASE(ptr);
        if(iid) nsMemory::Free((char*)iid);
        NS_ENSURE_SUCCESS(rv,rv);
        break;
    }
        break;
    case nsIDataType::VTYPE_ARRAY:
        rv = inVar->GetAsArray(&du.u.array.mArrayType,
                               &du.u.array.mArrayInterfaceID,
                               &du.u.array.mArrayCount,
                               &du.u.array.mArrayValue);
        if(NS_FAILED(rv)) return rv;
        du.mType = type;
        rv = outVar->SetAsArray(du.u.array.mArrayType,
                                &du.u.array.mArrayInterfaceID,
                                du.u.array.mArrayCount,
                                du.u.array.mArrayValue);
        NS_ENSURE_SUCCESS(rv,rv);
        break;
    case nsIDataType::VTYPE_EMPTY_ARRAY:
        if(inVarType != nsIDataType::VTYPE_EMPTY_ARRAY)
            return NS_ERROR_CANNOT_CONVERT_DATA;
        rv = outVar->SetAsEmptyArray();
        NS_ENSURE_SUCCESS(rv,rv);
        break;        
    case nsIDataType::VTYPE_EMPTY:
        if(inVarType != nsIDataType::VTYPE_EMPTY)
            return NS_ERROR_CANNOT_CONVERT_DATA;
        rv = outVar->SetAsEmpty();
        NS_ENSURE_SUCCESS(rv,rv);
        break;
    default:
        NS_ERROR("bad type in variant!");
        break;
    }

    nsVariant::Cleanup(&du);
    *_retval = outVar;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}        


/* nsIVariant copyVariant (in nsIVariant value); */
NS_IMETHODIMP nsTestVariant::CopyVariant(nsIVariant *value, nsIVariant **_retval)
{
    PRUint16 type;
    if(NS_FAILED(value->GetDataType(&type)))
        return NS_ERROR_FAILURE;
    return ConvertAndCopyVariant(value, type, _retval);
}

/* nsIVariant copyVariantAsType (in nsIVariant value, in PRUint16 type); */
NS_IMETHODIMP nsTestVariant::CopyVariantAsType(nsIVariant *value, PRUint16 type, nsIVariant **_retval)
{
    return ConvertAndCopyVariant(value, type, _retval);
}

/* nsIVariant copyVariantAsTypeTwice (in nsIVariant value, in PRUint16 type1, in PRUint16 type2); */
NS_IMETHODIMP nsTestVariant::CopyVariantAsTypeTwice(nsIVariant *value, PRUint16 type1, PRUint16 type2, nsIVariant **_retval)
{
    nsCOMPtr<nsIVariant> temp;
    nsresult rv = ConvertAndCopyVariant(value, type1, getter_AddRefs(temp));
    if(NS_FAILED(rv))
        return rv;
    return ConvertAndCopyVariant(temp, type2, _retval);
}

/* nsIVariant getNamedProperty (in nsISupports aBag, in AString aName); */
NS_IMETHODIMP nsTestVariant::GetNamedProperty(nsISupports *aObj, const nsAString & aName, nsIVariant **_retval)
{
    nsresult rv;
    nsCOMPtr<nsIPropertyBag> bag = do_QueryInterface(aObj, &rv);
    if(!bag)
        return rv;
    return bag->GetProperty(aName, _retval);
}

/* nsISimpleEnumerator getEnumerator (in nsISupports aBag); */
NS_IMETHODIMP nsTestVariant::GetEnumerator(nsISupports *aObj, nsISimpleEnumerator **_retval)
{
    nsresult rv;
    nsCOMPtr<nsIPropertyBag> bag = do_QueryInterface(aObj, &rv);
    if(!bag)
        return rv;
    return bag->GetEnumerator(_retval);
}

/***************************************************************************/

// static
NS_IMETHODIMP
xpctest::ConstructXPCTestVariant(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    NS_ASSERTION(aOuter == nsnull, "no aggregation");
    nsTestVariant* obj = new nsTestVariant();

    if(!obj)
    {
        *aResult = nsnull;
        rv = NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(obj);
    rv = obj->QueryInterface(aIID, aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find correct interface");
    NS_RELEASE(obj);
    return rv;
}
