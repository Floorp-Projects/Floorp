/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vidur Apparao (vidur@netscape.com)  (Original author)
 *   John Bandhauer (jband@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "wspprivate.h"
#include "nsIXPConnect.h"
#include "jsapi.h"


WSPPropertyBagWrapper::WSPPropertyBagWrapper()
  : mIID(nsnull)
{
}

WSPPropertyBagWrapper::~WSPPropertyBagWrapper()
{
}

nsresult
WSPPropertyBagWrapper::Init(nsIPropertyBag* aPropertyBag,
                            nsIInterfaceInfo* aInterfaceInfo)
{
  mPropertyBag = aPropertyBag;
  mInterfaceInfo = aInterfaceInfo;
  mInterfaceInfo->GetIIDShared(&mIID);
  return NS_OK;
}

NS_METHOD
WSPPropertyBagWrapper::Create(nsISupports* outer, const nsIID& aIID,
                              void* *aInstancePtr)
{
  NS_ENSURE_ARG_POINTER(aInstancePtr);
  NS_ENSURE_NO_AGGREGATION(outer);

  WSPPropertyBagWrapper* wrapper = new WSPPropertyBagWrapper();
  if (!wrapper) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(wrapper);
  nsresult rv = wrapper->QueryInterface(aIID, aInstancePtr);
  NS_RELEASE(wrapper);
  return rv;
}

NS_IMPL_ADDREF(WSPPropertyBagWrapper)
NS_IMPL_RELEASE(WSPPropertyBagWrapper)

NS_IMETHODIMP
WSPPropertyBagWrapper::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if((mIID && aIID.Equals(*mIID)) || aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = NS_STATIC_CAST(nsXPTCStubBase*, this);
  }
  else if (aIID.Equals(NS_GET_IID(nsIWebServicePropertyBagWrapper))) {
    *aInstancePtr = NS_STATIC_CAST(nsIWebServicePropertyBagWrapper*, this);
  }
  else if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {
    *aInstancePtr = NS_STATIC_CAST(nsIClassInfo*, this);
  } else {
    return NS_ERROR_NO_INTERFACE;
  }

  NS_ADDREF_THIS();
  return NS_OK;
}

NS_IMETHODIMP
WSPPropertyBagWrapper::CallMethod(PRUint16 methodIndex,
                                  const nsXPTMethodInfo* info,
                                  nsXPTCMiniVariant* params)
{
  if (methodIndex < 3) {
    NS_ERROR("WSPPropertyBagWrapper: bad method index");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;
  nsAutoString propName;

  rv = WSPFactory::C2XML(nsDependentCString(info->GetName()), propName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIVariant> val;
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

    rv = WSPProxy::VariantToValue(type_tag, params[0].val.p, iinfo, val);
  }
  else if (info->GetParamCount() == 2) {
    // If it's not an explicit getter, it has to be an array getter
    // method.

    // The first parameter should be the array length out param
    const nsXPTParamInfo& paramInfo1 = info->GetParam(0);
    const nsXPTType& type1 = paramInfo1.GetType();
    if (!paramInfo1.IsOut() || (type1.TagPart() != nsXPTType::T_U32)) {
      NS_ERROR("Unexpected parameter type for getter");
      return NS_ERROR_FAILURE;
    }

    // The second parameter should be the array out pointer itself.
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

    rv = WSPProxy::VariantToArrayValue(arrayType.TagPart(), params, params + 1,
                                       iinfo, val);
  }
  else {
    NS_ERROR("Unexpected method signature for property bag wrapper");
    return NS_ERROR_FAILURE;
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

///////////////////////////////////////////////////
//
// Implementation of nsIClassInfo
//
///////////////////////////////////////////////////

/* void getInterfaces (out PRUint32 count, [array, size_is (count),
   retval] out nsIIDPtr array); */
NS_IMETHODIMP
WSPPropertyBagWrapper::GetInterfaces(PRUint32 *count, nsIID * **array)
{
  if (!mIID) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *count = 2;
  nsIID** iids = NS_STATIC_CAST(nsIID**, nsMemory::Alloc(2 * sizeof(nsIID*)));
  if (!iids) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  iids[0] = NS_STATIC_CAST(nsIID *, nsMemory::Clone(mIID, sizeof(nsIID)));
  if (!iids[0]) {
    nsMemory::Free(iids);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  const nsIID& wsiid = NS_GET_IID(nsIWebServicePropertyBagWrapper);
  iids[1] = NS_STATIC_CAST(nsIID *, nsMemory::Clone(&wsiid, sizeof(nsIID)));
  if (!iids[1]) {
    nsMemory::Free(iids[0]);
    nsMemory::Free(iids);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *array = iids;

  return NS_OK;
}

/* nsISupports getHelperForLanguage (in PRUint32 language); */
NS_IMETHODIMP
WSPPropertyBagWrapper::GetHelperForLanguage(PRUint32 language,
                                            nsISupports **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/* readonly attribute string contractID; */
NS_IMETHODIMP
WSPPropertyBagWrapper::GetContractID(char * *aContractID)
{
  *aContractID = (char *)
    nsMemory::Clone(NS_WEBSERVICEPROPERTYBAGWRAPPER_CONTRACTID,
                    sizeof(NS_WEBSERVICEPROPERTYBAGWRAPPER_CONTRACTID));
  if (!*aContractID) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

/* readonly attribute string classDescription; */
NS_IMETHODIMP
WSPPropertyBagWrapper::GetClassDescription(char * *aClassDescription)
{
  *aClassDescription = nsnull;
  return NS_OK;
}

/* readonly attribute nsCIDPtr classID; */
NS_IMETHODIMP
WSPPropertyBagWrapper::GetClassID(nsCID * *aClassID)
{
  *aClassID = nsnull;
  return NS_OK;
}

/* readonly attribute PRUint32 implementationLanguage; */
NS_IMETHODIMP
WSPPropertyBagWrapper::GetImplementationLanguage(PRUint32 *aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

/* readonly attribute PRUint32 flags; */
NS_IMETHODIMP
WSPPropertyBagWrapper::GetFlags(PRUint32 *aFlags)
{
  *aFlags = nsIClassInfo::DOM_OBJECT;
  return NS_OK;
}

/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
NS_IMETHODIMP
WSPPropertyBagWrapper::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  return NS_ERROR_NOT_AVAILABLE;
}

