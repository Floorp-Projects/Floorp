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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer (jband@netscape.com)
 *   Vidur Apparao (vidur@netscape.com)
 *
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

#include "wspprivate.h"


WSPPropertyBagWrapper::WSPPropertyBagWrapper(nsIPropertyBag* aPropertyBag,
                                             nsIInterfaceInfo* aInterfaceInfo)
  : mPropertyBag(aPropertyBag), mInterfaceInfo(aInterfaceInfo)
{
  NS_INIT_ISUPPORTS();
  mInterfaceInfo->GetIIDShared(&mIID);
}

WSPPropertyBagWrapper::~WSPPropertyBagWrapper()
{
}

nsresult 
WSPPropertyBagWrapper::Create(nsIPropertyBag* aPropertyBag,
                              nsIInterfaceInfo* aInterfaceInfo,
                              WSPPropertyBagWrapper** aWrapper)
{
  NS_ENSURE_ARG(aPropertyBag);
  NS_ENSURE_ARG(aInterfaceInfo);
  NS_ENSURE_ARG_POINTER(aWrapper);

  WSPPropertyBagWrapper* wrapper = new WSPPropertyBagWrapper(aPropertyBag,
                                                             aInterfaceInfo);
  if (!wrapper) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  *aWrapper = wrapper;
  NS_ADDREF(*aWrapper);
  
  return NS_OK;
}

NS_IMPL_ADDREF(WSPPropertyBagWrapper)
NS_IMPL_RELEASE(WSPPropertyBagWrapper)

NS_IMETHODIMP
WSPPropertyBagWrapper::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if(aIID.Equals(*mIID) || aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = NS_STATIC_CAST(nsISupports*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
        
  return NS_ERROR_NO_INTERFACE;
}

nsresult
WSPPropertyBagWrapper::VariantToResult(uint8 aTypeTag,
                                       void* aResult,
                                       nsIInterfaceInfo* aInterfaceInfo,
                                       nsIVariant* aProperty)
{
  nsresult rv;

  switch(aTypeTag) {
    case nsXPTType::T_I8:
      rv = aProperty->GetAsInt8((PRUint8*)aResult);
      break;
    case nsXPTType::T_I16:
      rv = aProperty->GetAsInt16((PRInt16*)aResult);
      break;
    case nsXPTType::T_I32:
      rv = aProperty->GetAsInt32((PRInt32*)aResult);
      break;
    case nsXPTType::T_I64:
      rv = aProperty->GetAsInt64((PRInt64*)aResult);
      break; 
    case nsXPTType::T_U8:
      rv = aProperty->GetAsUint8((PRUint8*)aResult);
      break;
    case nsXPTType::T_U16:
      rv = aProperty->GetAsUint16((PRUint16*)aResult);
      break;
    case nsXPTType::T_U32:
      rv = aProperty->GetAsUint32((PRUint32*)aResult);
      break;
    case nsXPTType::T_U64:
      rv = aProperty->GetAsUint64((PRUint64*)aResult);
      break;
    case nsXPTType::T_FLOAT:
      rv = aProperty->GetAsFloat((float*)aResult);
      break;
    case nsXPTType::T_DOUBLE:
      rv = aProperty->GetAsDouble((double*)aResult);
      break;
    case nsXPTType::T_BOOL:
      rv = aProperty->GetAsBool((PRBool*)aResult);
      break;
    case nsXPTType::T_CHAR:
      rv = aProperty->GetAsChar((char*)aResult);
      break;
    case nsXPTType::T_WCHAR:
      rv = aProperty->GetAsWChar((PRUnichar*)aResult);
      break;
    case nsXPTType::T_DOMSTRING:
      rv = aProperty->GetAsAString(*(nsAString*)aResult);
      break;
    case nsXPTType::T_INTERFACE:
    {
      PRUint16 dataType;
      aProperty->GetDataType(&dataType);
      if (dataType == nsIDataType::TYPE_EMPTY) {
        *(nsISupports**)aResult = nsnull;
      }
      else {
        nsCOMPtr<nsISupports> sup;
        rv = aProperty->GetAsISupports(getter_AddRefs(sup));
        if (NS_FAILED(rv)) {
          return rv;
        }
        nsCOMPtr<nsIPropertyBag> propBag = do_QueryInterface(sup, &rv);
        if (NS_FAILED(rv)) {
          return rv;
        }
        WSPPropertyBagWrapper* wrapper;
        rv = Create(propBag, aInterfaceInfo, &wrapper);
        if (NS_FAILED(rv)) {
          return rv;
        }

        const nsIID* iid;
        aInterfaceInfo->GetIIDShared(&iid);
        rv = wrapper->QueryInterface(*iid, (void**)aResult);
        NS_RELEASE(wrapper);
      }
      break;
    }
    default:
      NS_ERROR("Bad attribute type for complex type interface");
      rv = NS_ERROR_FAILURE;
  }

  return rv;
}

nsresult
WSPPropertyBagWrapper::VariantToArrayResult(uint8 aTypeTag,
                                            nsXPTCMiniVariant* aResult,
                                            nsIInterfaceInfo* aInterfaceInfo,
                                            nsIVariant* aProperty)
{
  void* array;
  PRUint16 type;
  PRUint32 count;

  nsresult rv = aProperty->GetAsArray(&type, &count, &array);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // We assume it's an array of variants.
  // XXX need a way to confirm that
  if (type != nsIDataType::TYPE_INTERFACE) {
    return NS_ERROR_FAILURE;
  }
  nsIVariant** variants = (nsIVariant**)array;

  PRUint32 size;
  // Allocate the array
  switch (aTypeTag) {
    case nsXPTType::T_I8:
    case nsXPTType::T_U8:
      size = sizeof(PRUint8);
      break;
    case nsXPTType::T_I16:
    case nsXPTType::T_U16:
      size = sizeof(PRUint16);
      break;
    case nsXPTType::T_I32:
    case nsXPTType::T_U32:
      size = sizeof(PRUint32);
      break;
    case nsXPTType::T_I64:
    case nsXPTType::T_U64:
      size = sizeof(PRUint64);
      break;
    case nsXPTType::T_FLOAT:
      size = sizeof(float);
      break;
    case nsXPTType::T_DOUBLE:
      size = sizeof(double);
      break;
    case nsXPTType::T_BOOL:
      size = sizeof(PRBool);
      break;
    case nsXPTType::T_CHAR:
      size = sizeof(char);
      break;
    case nsXPTType::T_WCHAR:
      size = sizeof(PRUnichar);
      break;
    case nsXPTType::T_INTERFACE:
      size = sizeof(nsISupports*);
      break;
    default:
      NS_ERROR("Unexpected array type");
      return NS_ERROR_FAILURE;
  }

  void* outptr = nsMemory::Alloc(count * size);
  if (!outptr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRUint32 i;
  for (i = 0; i < count; i++) {
    rv = VariantToResult(aTypeTag, (void*)((char*)outptr + (i*size)),
                         aInterfaceInfo, variants[i]);
    if (NS_FAILED(rv)) {
      break;
    }
  }

  // Free the variant array passed back to us
  NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(count, variants);

  // If conversion failed, free the allocated structures
  if (NS_FAILED(rv)) {
    if (aTypeTag == nsXPTType::T_INTERFACE) {
      nsMemory::Free(outptr);
    }
    else {
      NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(i, (nsISupports**)outptr);
    }

    return rv;
  }

  *((PRUint32*)aResult[0].val.p) = count;
  *((void**)aResult[1].val.p) = outptr;

  return NS_OK;
}

NS_IMETHODIMP 
WSPPropertyBagWrapper::CallMethod(PRUint16 methodIndex,
                                  const nsXPTMethodInfo* info,
                                  nsXPTCMiniVariant* params)
{
  nsresult rv = NS_OK;
  nsAutoString propName;
  nsCOMPtr<nsIVariant> val;

  if (methodIndex < 3) {
    NS_ERROR("WSPPropertyBagWrapper: bad method index");
    return NS_ERROR_FAILURE;
  }
  else {
    rv = WSPFactory::MethodToPropertyName(nsDependentCString(info->GetName()),
                                          propName);
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    rv = mPropertyBag->GetProperty(propName, getter_AddRefs(val));
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    nsCOMPtr<nsIInterfaceInfo> iinfo;
    if (info->IsGetter()) {
      const nsXPTParamInfo& paramInfo = info->GetParam(0);
      const nsXPTType& type = paramInfo.GetType();
      uint8 type_tag = type.TagPart();
      
      if (type_tag == nsXPTType::T_INTERFACE) {
        rv = mInterfaceInfo->GetInfoForParam(methodIndex, &paramInfo, 
                                             getter_AddRefs(iinfo));
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
      
      rv = VariantToResult(type_tag, params[0].val.p, iinfo, val);
    }
    else if (info->GetParamCount() == 2) {
      // If it's not an explicit getter, it has to be an array 
      // getter method.

      // The first parameter should be the array length out param
      const nsXPTParamInfo& paramInfo1 = info->GetParam(0);
      const nsXPTType& type1 = paramInfo1.GetType();
      if (!paramInfo1.IsOut() || (type1.TagPart() != nsXPTType::T_U32)) {
        NS_ERROR("Unexpected parameter type for getter");
        return NS_ERROR_FAILURE;
      }

      // The second parameter should be the array out pointer
      // itself.
      const nsXPTParamInfo& paramInfo2 = info->GetParam(1);
      const nsXPTType& type2 = paramInfo2.GetType();
      if (!paramInfo2.IsOut() || !type2.IsArray()) {
        NS_ERROR("Unexpected parameter type for getter");
        return NS_ERROR_FAILURE;
      }

      nsXPTType arrayType;
      rv = mInterfaceInfo->GetTypeForParam(methodIndex, &paramInfo2,
                                           1, &arrayType);
      if (NS_FAILED(rv)) {
        return rv;
      }
    
      if (arrayType.IsInterfacePointer()) {
        rv = mInterfaceInfo->GetInfoForParam(methodIndex, &paramInfo2, 
                                             getter_AddRefs(iinfo));
        if (NS_FAILED(rv)) {
          return rv;
        }
      }

      rv = VariantToArrayResult(arrayType.TagPart(), params, iinfo, val);
    }
    else {
      NS_ERROR("Unexpected method signature for property bag wrapper");
      return NS_ERROR_FAILURE;
    }
  }

  return rv;
}

NS_IMETHODIMP 
WSPPropertyBagWrapper::GetInterfaceInfo(nsIInterfaceInfo** info)
{
  NS_ENSURE_ARG_POINTER(info);

  *info = mInterfaceInfo;
  NS_ADDREF(*info);

  return NS_OK;
}


