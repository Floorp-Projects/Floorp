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

// xtpcall includes
#include "xptcall.h"
#include "xptinfo.h"

// xpcom includes
#include "nsIServiceManager.h"

class WSPComplexTypeProperty : public nsIProperty
{
public:
  WSPComplexTypeProperty(const nsAString& aName, nsIVariant* aValue);
  virtual ~WSPComplexTypeProperty()
  {
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROPERTY

protected:
  nsString mName;
  nsCOMPtr<nsIVariant> mValue;
};

WSPComplexTypeProperty::WSPComplexTypeProperty(const nsAString& aName,
                                               nsIVariant* aValue)
  : mName(aName), mValue(aValue)
{
  NS_ASSERTION(mValue, "Null value!");
}

NS_IMPL_ISUPPORTS1(WSPComplexTypeProperty, nsIProperty)

/* readonly attribute AString name; */
NS_IMETHODIMP 
WSPComplexTypeProperty::GetName(nsAString & aName)
{
  aName.Assign(mName);
  return NS_OK;
}

/* readonly attribute nsIVariant value; */
NS_IMETHODIMP 
WSPComplexTypeProperty::GetValue(nsIVariant * *aValue)
{
  NS_ADDREF(*aValue = mValue);
  return NS_OK;
}


class WSPComplexTypeEnumerator : public nsISimpleEnumerator {
public:
  WSPComplexTypeEnumerator(WSPComplexTypeWrapper* aWrapper,
                           nsIInterfaceInfo* aInterfaceInfo);
  virtual ~WSPComplexTypeEnumerator();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

protected:
  WSPComplexTypeWrapper* mWrapper;
  nsCOMPtr<nsIInterfaceInfo> mInterfaceInfo;
  PRUint16 mIndex;
  PRUint16 mCount;
};

WSPComplexTypeEnumerator::WSPComplexTypeEnumerator(WSPComplexTypeWrapper* aWrapper, nsIInterfaceInfo* aInterfaceInfo) 
  : mInterfaceInfo(aInterfaceInfo), mIndex(3)
{
  mWrapper = aWrapper;
  NS_ADDREF(mWrapper);
  if (mInterfaceInfo) {
    mInterfaceInfo->GetMethodCount(&mCount);
  }
}

WSPComplexTypeEnumerator::~WSPComplexTypeEnumerator()
{
  NS_RELEASE(mWrapper);
}

NS_IMPL_ISUPPORTS1(WSPComplexTypeEnumerator, nsISimpleEnumerator)

/* boolean hasMoreElements (); */
NS_IMETHODIMP 
WSPComplexTypeEnumerator::HasMoreElements(PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = mIndex < mCount;
  return NS_OK;
}

/* nsISupports getNext (); */
NS_IMETHODIMP 
WSPComplexTypeEnumerator::GetNext(nsISupports **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  if (mIndex >= mCount) {
    NS_ERROR("Bad nsISimpleEnumerator caller!");
    return NS_ERROR_FAILURE;
  }

  const nsXPTMethodInfo* methodInfo;
  nsresult rv = mInterfaceInfo->GetMethodInfo(mIndex, &methodInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  nsCOMPtr<nsIVariant> var;
  rv =  mWrapper->GetPropertyValue(mIndex++, methodInfo, getter_AddRefs(var));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoString propName;
  rv = WSPFactory::C2XML(nsDependentCString(methodInfo->GetName()), propName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  WSPComplexTypeProperty* prop = new WSPComplexTypeProperty(propName, var);
  if (!prop) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *_retval = prop;
  NS_ADDREF(*_retval);

  return NS_OK;
}


WSPComplexTypeWrapper::WSPComplexTypeWrapper()
{
}

WSPComplexTypeWrapper::~WSPComplexTypeWrapper()
{
}

nsresult
WSPComplexTypeWrapper::Init(nsISupports* aComplexTypeInstance,
                            nsIInterfaceInfo* aInterfaceInfo)
{
  mComplexTypeInstance = aComplexTypeInstance;
  mInterfaceInfo = aInterfaceInfo;
  return NS_OK;
}

NS_METHOD
WSPComplexTypeWrapper::Create(nsISupports* outer, const nsIID& aIID, 
                              void* *aInstancePtr)
{
  NS_ENSURE_ARG_POINTER(aInstancePtr);
  NS_ENSURE_NO_AGGREGATION(outer);

  WSPComplexTypeWrapper* wrapper = new WSPComplexTypeWrapper();
  if (!wrapper) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(wrapper);
  nsresult rv = wrapper->QueryInterface(aIID, aInstancePtr);
  NS_RELEASE(wrapper);
  return rv;
}

NS_IMPL_ISUPPORTS2_CI(WSPComplexTypeWrapper, 
                      nsIWebServiceComplexTypeWrapper,
                      nsIPropertyBag)

/* readonly attribute nsISimpleEnumerator enumerator; */
NS_IMETHODIMP 
WSPComplexTypeWrapper::GetEnumerator(nsISimpleEnumerator * *aEnumerator)
{
  WSPComplexTypeEnumerator* enumerator =
    new WSPComplexTypeEnumerator(this, mInterfaceInfo);
  if (!enumerator) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aEnumerator = enumerator;
  NS_ADDREF(*aEnumerator);
  return NS_OK;
}

/* nsIVariant getProperty (in AString name); */
NS_IMETHODIMP 
WSPComplexTypeWrapper::GetProperty(const nsAString & name,
                                   nsIVariant **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsCAutoString methodName;
  WSPFactory::XML2C(name, methodName);

  const nsXPTMethodInfo* methodInfo;
  PRUint16 methodIndex;
  nsresult rv = mInterfaceInfo->GetMethodInfoForName(methodName.get(),
                                                     &methodIndex,
                                                     &methodInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return GetPropertyValue(methodIndex, methodInfo, _retval);
}

nsresult
WSPComplexTypeWrapper::GetPropertyValue(PRUint32 aMethodIndex,
                                        const nsXPTMethodInfo* aMethodInfo,
                                        nsIVariant** _retval)
{
  nsresult rv;
  nsAutoString outstr;
  PRUint32 numParams;
  nsXPTCVariant var[2];
  uint8 type_tag;
  nsXPTType arrayType;
  nsCOMPtr<nsIInterfaceInfo> iinfo;

  var[0].ClearFlags();
  var[1].ClearFlags();

  // There are two possibilities here: a getter or a
  // method that returns array and array size out parameters.
  if (aMethodInfo->IsGetter()) {
    // If it's a getter make sure that it takes just a single (out) param
    if (aMethodInfo->GetParamCount() != 1) {
      return NS_ERROR_FAILURE;
    }
  
    const nsXPTParamInfo& paramInfo = aMethodInfo->GetParam(0);
    const nsXPTType& type = paramInfo.GetType();
    type_tag = type.TagPart();
    
    numParams = 1;
    var[0].type = type_tag;
    if (paramInfo.IsOut()) {
      var[0].SetPtrIsData();
      var[0].ptr = &var[0].val;
    }
    else if (paramInfo.IsDipper() && type.IsPointer() && 
             (type_tag == nsXPTType::T_DOMSTRING)) {
      var[0].val.p = &outstr;
    }
    else {
      NS_ERROR("Unexpected parameter type for getter");
      return NS_ERROR_FAILURE;
    }

    if (type_tag == nsXPTType::T_INTERFACE) {
      rv = mInterfaceInfo->GetInfoForParam(aMethodIndex, &paramInfo, 
                                           getter_AddRefs(iinfo));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }
  // If it isn't a getter, then it has to be an array
  // getter method
  else {
    // It must take two parameters for this to work
    if (aMethodInfo->GetParamCount() != 2) {
      return NS_ERROR_FAILURE;
    }

    numParams = 2;

    // The first parameter must be "out PRUint32"
    const nsXPTParamInfo& paramInfo1 = aMethodInfo->GetParam(0);
    const nsXPTType& type1 = paramInfo1.GetType();
    if (!paramInfo1.IsOut() || (type1.TagPart() != nsXPTType::T_U32)) {
      NS_ERROR("Unexpected parameter type for getter");
      return NS_ERROR_FAILURE;
    }
    
    var[0].type = nsXPTType::T_U32;
    var[0].SetPtrIsData();
    var[0].ptr = &var[0].val;
    
    // The second parameter must be "[array] out"
    const nsXPTParamInfo& paramInfo2 = aMethodInfo->GetParam(1);
    const nsXPTType& type2 = paramInfo2.GetType();
    type_tag = type2.TagPart();
    if (!paramInfo2.IsOut() || !type2.IsArray()) {
      NS_ERROR("Unexpected parameter type for getter");
      return NS_ERROR_FAILURE;
    }

    var[1].type = type_tag;
    var[1].SetPtrIsData();
    var[1].ptr = &var[1].val;

    rv = mInterfaceInfo->GetTypeForParam(aMethodIndex, &paramInfo2, 1,
                                         &arrayType);
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    if (arrayType.IsInterfacePointer()) {
      rv = mInterfaceInfo->GetInfoForParam(aMethodIndex, &paramInfo2, 
                                           getter_AddRefs(iinfo));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  rv = XPTC_InvokeByIndex(mComplexTypeInstance, aMethodIndex,
                          numParams, var);

  if (NS_FAILED(rv)) {
    return rv;
  }

  if (type_tag == nsXPTType::T_ARRAY) {
    rv = WSPProxy::ArrayXPTCMiniVariantToVariant(arrayType.TagPart(), var[1], 
                                                 var[0].val.u32, iinfo, 
                                                 _retval);
  }
  else {
    rv = WSPProxy::XPTCMiniVariantToVariant(type_tag, var[0], iinfo, _retval);
  }

  return rv;
}
