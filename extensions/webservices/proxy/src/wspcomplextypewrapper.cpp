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

// xtpcall includes
#include "xptcall.h"
#include "xptinfo.h"

// xpcom includes
#include "nsIServiceManager.h"

class WSPComplexTypeProperty : public nsIProperty {
public:
  WSPComplexTypeProperty(const nsAReadableString& aName,
                         nsIVariant* aValue);
  virtual ~WSPComplexTypeProperty() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROPERTY

protected:
  nsString mName;
  nsCOMPtr<nsIVariant> mValue;
};

WSPComplexTypeProperty::WSPComplexTypeProperty(const nsAReadableString& aName,
                                               nsIVariant* aValue)
  : mName(aName), mValue(aValue)
{
  NS_INIT_ISUPPORTS();
}

NS_IMPL_ISUPPORTS1(WSPComplexTypeProperty, nsIProperty)

/* readonly attribute AString name; */
NS_IMETHODIMP 
WSPComplexTypeProperty::GetName(nsAWritableString & aName)
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
  virtual ~WSPComplexTypeEnumerator() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

protected:
  nsCOMPtr<WSPComplexTypeWrapper> mWrapper;
  nsCOMPtr<nsIInterfaceInfo> mInterfaceInfo;
  PRUint16 mIndex;
  PRUint16 mCount;
};

WSPComplexTypeEnumerator::WSPComplexTypeEnumerator(WSPComplexTypeWrapper* aWrapper, nsIInterfaceInfo* aInterfaceInfo) 
  : mWrapper(aWrapper), mInterfaceInfo(aInterfaceInfo), mIndex(3)
{
  NS_INIT_ISUPPORTS();
  if (mInterfaceInfo) {
    mInterfaceInfo->GetMethodCount(&mCount);
  }
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
  rv = WSPFactory::MethodToPropertyName(nsDependentCString(methodInfo->GetName()), propName);
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


WSPComplexTypeWrapper::WSPComplexTypeWrapper(nsISupports* aComplexTypeInstance,
                                             nsIInterfaceInfo* aInterfaceInfo)
  : mComplexTypeInstance(aComplexTypeInstance), 
    mInterfaceInfo(aInterfaceInfo)
{
  NS_INIT_ISUPPORTS();
}

WSPComplexTypeWrapper::~WSPComplexTypeWrapper()
{
}

nsresult 
WSPComplexTypeWrapper::Create(nsISupports* aComplexTypeInstance,
                              nsIInterfaceInfo* aInterfaceInfo,
                              WSPComplexTypeWrapper** aWrapper)
{
  NS_ENSURE_ARG(aComplexTypeInstance);
  NS_ENSURE_ARG(aInterfaceInfo);
  NS_ENSURE_ARG_POINTER(aWrapper);

  WSPComplexTypeWrapper* wrapper = new WSPComplexTypeWrapper(aComplexTypeInstance, aInterfaceInfo);
  if (!wrapper) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
   
  *aWrapper = wrapper;
  NS_ADDREF(*aWrapper);
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(WSPComplexTypeWrapper, nsIPropertyBag)

/* readonly attribute nsISimpleEnumerator enumerator; */
NS_IMETHODIMP 
WSPComplexTypeWrapper::GetEnumerator(nsISimpleEnumerator * *aEnumerator)
{
  WSPComplexTypeEnumerator* enumerator = new WSPComplexTypeEnumerator(this, mInterfaceInfo);
  if (!enumerator) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aEnumerator = enumerator;
  NS_ADDREF(*aEnumerator);
  return NS_OK;
}

nsresult
WSPComplexTypeWrapper::ResultAsVariant(uint8 aTypeTag,
                                       nsXPTCVariant aResult,
                                       nsIInterfaceInfo* aInterfaceInfo,
                                       nsIVariant** aVariant)
{
  nsresult rv;

  nsCOMPtr<nsIWritableVariant> var = do_CreateInstance(NS_VARIANT_CONTRACTID, 
                                                       &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  switch (aTypeTag) {
    case nsXPTType::T_I8:
      var->SetAsInt8(aResult.val.i8);
      break;
    case nsXPTType::T_I16:
      var->SetAsInt16(aResult.val.i16);
      break;
    case nsXPTType::T_I32:
      var->SetAsInt32(aResult.val.i32);
      break;
    case nsXPTType::T_I64:
      var->SetAsInt64(aResult.val.i64);
      break;
    case nsXPTType::T_U8:
      var->SetAsUint8(aResult.val.u8);
      break;
    case nsXPTType::T_U16:
      var->SetAsUint16(aResult.val.u16);
      break;
    case nsXPTType::T_U32:
      var->SetAsUint32(aResult.val.u32);
      break;
    case nsXPTType::T_U64:
      var->SetAsUint64(aResult.val.u64);
      break;
    case nsXPTType::T_FLOAT:
      var->SetAsFloat(aResult.val.f);
      break;
    case nsXPTType::T_DOUBLE:
      var->SetAsDouble(aResult.val.d);
      break;
    case nsXPTType::T_BOOL:
      var->SetAsBool(aResult.val.b);
      break;
    case nsXPTType::T_CHAR:
      var->SetAsChar(aResult.val.c);
      break;
    case nsXPTType::T_WCHAR:
      var->SetAsWChar(aResult.val.wc);
      break;
    case nsXPTType::T_DOMSTRING:
      var->SetAsAString(*((nsAString*)aResult.val.p));
      break;
    case nsXPTType::T_INTERFACE:
    {
      nsISupports* instance = (nsISupports*)aResult.val.p;
      if (instance) {
        // Rewrap an interface instance in a property bag
        nsCOMPtr<WSPComplexTypeWrapper> wrapper;
        rv = Create(instance, aInterfaceInfo, 
                    getter_AddRefs(wrapper));
        if (NS_FAILED(rv)) {
          return rv;
        }
        var->SetAsInterface(NS_GET_IID(nsIPropertyBag), wrapper);
        NS_RELEASE(instance);
      }
      else {
        var->SetAsEmpty();
      }
      
      break;
    }
    default:
      NS_ERROR("Bad attribute type for complex type interface");
      rv = NS_ERROR_FAILURE;
  }

  *aVariant = var;
  NS_ADDREF(*aVariant);

  return rv;
}

nsresult
WSPComplexTypeWrapper::ArrayResultAsVariant(uint8 aTypeTag,
                                            nsXPTCVariant aResult,
                                            PRUint32 aLength,
                                            nsIInterfaceInfo* aInterfaceInfo,
                                            nsIVariant** aVariant)
{   
  nsresult rv;

  nsCOMPtr<nsIWritableVariant> retvar = do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aLength) {
    nsIVariant** entries = (nsIVariant**)nsMemory::Alloc(aLength * sizeof(nsIVariant*));
    if (!entries) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    
    PRUint32 i = 0;
    nsXPTCVariant var;
    void* array = aResult.val.p;
    nsCOMPtr<nsIVariant> element;

#define POPULATE(_t,_v)                                                  \
  PR_BEGIN_MACRO                                                         \
    for(i = 0; i < aLength; i++)                                         \
    {                                                                    \
      var.type = aTypeTag;                                               \
      var.val._v = *((_t*)array + i);                                    \
      rv = ResultAsVariant(aTypeTag, var, aInterfaceInfo,                \
                           entries+i);                                   \
      if (NS_FAILED(rv)) {                                               \
        break;                                                           \
      }                                                                  \
    }                                                                    \
  PR_END_MACRO

    switch (aTypeTag) {
      case nsXPTType::T_I8:            POPULATE(PRInt8, i8);      break; 
      case nsXPTType::T_I16:           POPULATE(PRInt16, i16);    break;
      case nsXPTType::T_I32:           POPULATE(PRInt32, i32);    break;
      case nsXPTType::T_I64:           POPULATE(PRInt64, i64);    break;
      case nsXPTType::T_U8:            POPULATE(PRUint8, u8);     break; 
      case nsXPTType::T_U16:           POPULATE(PRUint16, u16);   break;
      case nsXPTType::T_U32:           POPULATE(PRUint32, u32);   break;
      case nsXPTType::T_U64:           POPULATE(PRUint64, u64);   break;
      case nsXPTType::T_FLOAT:         POPULATE(float, f);        break;
      case nsXPTType::T_DOUBLE:        POPULATE(double, d);       break;
      case nsXPTType::T_BOOL:          POPULATE(PRBool, b);       break;
      case nsXPTType::T_CHAR:          POPULATE(char, c);         break;
      case nsXPTType::T_WCHAR:         POPULATE(PRUnichar, wc);   break;
      case nsXPTType::T_INTERFACE:     POPULATE(nsISupports*, p); break;
    }

#undef POPULATE

    if (NS_SUCCEEDED(rv)) {
      rv = retvar->SetAsArray(nsIDataType::TYPE_INTERFACE, aLength, entries);
    }

    // Even if we failed while converting, we want to release
    // the entries that were already created.
    NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(i, entries);
  }
  else {
    retvar->SetAsEmpty();
  }

  if (NS_SUCCEEDED(rv)) {
    *aVariant = retvar;
    NS_ADDREF(*aVariant);
  }

  return rv;
}

/* nsIVariant getProperty (in AString name); */
NS_IMETHODIMP 
WSPComplexTypeWrapper::GetProperty(const nsAString & name, 
                                   nsIVariant **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsCAutoString methodName;
  WSPFactory::PropertyToMethodName(name, methodName);

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

    rv = mInterfaceInfo->GetTypeForParam(aMethodIndex, &paramInfo2,
                                         1, &arrayType);
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
    rv = ArrayResultAsVariant(arrayType.TagPart(), var[1], 
                              var[0].val.u32, iinfo, _retval);
  }
  else {
    rv = ResultAsVariant(type_tag, var[0], iinfo, _retval);
  }

  return rv;
}
