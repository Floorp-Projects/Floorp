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
 *   John Bandhauer (jband@netscape.com)
 *   Vidur Apparao (vidur@netscape.com)
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

#include "nsCOMPtr.h"
#include "wspproxytest.h"
#include "nsString.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIServiceManager.h"

const PRUint8 sInt8Val = 2;
const PRInt16 sInt16Val = 0x1234;
const PRInt32 sInt32Val = 0x12345678;
const PRInt64 sInt64Val = LL_INIT(0x12345678, 0x87654321);
const PRUint8  sUint8Val = 2;
const PRUint16 sUint16Val = 0x1234;
const PRUint32 sUint32Val = 0x12345678;
const PRUint64 sUint64Val = LL_INIT(0x12345678, 0x87654321);
const PRBool sBoolVal = PR_TRUE;
const float sFloatVal = 0.0;
const double sDoubleVal = 0.03;
const char sCharVal = 'a';
const PRUnichar sWcharVal = PRUnichar('a');
#define STRING_VAL "Hello world"
const PRUint32 sArray1Length = 3;
const PRUint32 sArray1[] = { 4, 23, 37 };
const PRUint32 sArray2Length = 4;
const double sArray2[] = { 4.234, 23.97, 3434.2945, 0.03 };

WSPProxyTest::WSPProxyTest()
{
}

WSPProxyTest::~WSPProxyTest()
{
}

NS_IMPL_ISUPPORTS3_CI(WSPProxyTest, 
                      nsIWSPProxyTest,
                      nsIWSDLLoadListener,
                      SpheonJSAOPStatisticsPortTypeListener)

nsresult
WSPProxyTest::CreateComplexTypeWrapper(nsIWebServiceComplexTypeWrapper** aWrapper,
                                       nsIInterfaceInfo** aInfo)
{
  static nsIID sComplexTypeIID = NS_GET_IID(nsIWSPTestComplexType);
  nsCOMPtr<WSPTestComplexType> ct = new WSPTestComplexType();
  if (!ct) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv;
  nsCOMPtr<nsIInterfaceInfoManager> manager = do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIInterfaceInfo> iinfo;
  rv = manager->GetInfoForIID(&sComplexTypeIID, getter_AddRefs(iinfo));
  if (NS_FAILED(rv)) {
    return rv;
  }
  *aInfo = iinfo;
  NS_ADDREF(*aInfo);

  nsCOMPtr<nsIWebServiceComplexTypeWrapper> wrapper = do_CreateInstance(NS_WEBSERVICECOMPLEXTYPEWRAPPER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = wrapper->Init(ct, iinfo);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aWrapper = wrapper;
  NS_ADDREF(*aWrapper);
  return NS_OK;
}

NS_IMETHODIMP
WSPProxyTest::TestComplexTypeWrapper(nsAString& aResult)
{
  nsCOMPtr<nsIInterfaceInfo> info;
  nsCOMPtr<nsIWebServiceComplexTypeWrapper> wrapper;
  nsresult rv = CreateComplexTypeWrapper(getter_AddRefs(wrapper),
                                         getter_AddRefs(info));
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed creating complex type wrapper"));
    return NS_OK;
  }

  nsCOMPtr<nsIPropertyBag> propBag = do_QueryInterface(wrapper);
  if (!propBag) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Wrapper is not property bag"));
    return NS_ERROR_FAILURE;
  }

  TestComplexTypeWrapperInstance(propBag, aResult);

  return NS_OK;
}


nsresult
WSPProxyTest::TestComplexTypeWrapperInstance(nsIPropertyBag* propBag,
                                             nsAString& aResult)
{
  nsCOMPtr<nsISimpleEnumerator> enumerator;
  nsresult rv = propBag->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed getting property bag enumerator"));
    return rv;
  }

  nsCOMPtr<nsISupports> sup;
  nsCOMPtr<nsIProperty> prop;
  nsCOMPtr<nsIVariant> val;
  nsAutoString propName;

#define GET_AND_TEST_NAME(_name)                                          \
    rv = enumerator->GetNext(getter_AddRefs(sup));                        \
    if (NS_FAILED(rv)) {                                                  \
      aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed getting property ") + NS_LITERAL_STRING(#_name));    \
      return rv;                                                          \
    }                                                                     \
                                                                          \
    prop = do_QueryInterface(sup, &rv);                                   \
    if (NS_FAILED(rv)) {                                                  \
      return rv;                                                          \
    }                                                                     \
                                                                          \
    prop->GetName(propName);                                              \
    if (!propName.EqualsLiteral(#_name)) {                    \
      aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Name doesn't match for property ") + NS_LITERAL_STRING(#_name)); \
      return NS_ERROR_FAILURE;                                            \
    }                                                                     \
    prop->GetValue(getter_AddRefs(val));                                  \
    if (!val) {                                                           \
      return NS_ERROR_FAILURE;                                            \
    }                                                                     

#define GET_AND_TEST(_t, _n, _name, _val)                                   \
    GET_AND_TEST_NAME(_name)                                                \
    _t _name;                                                               \
    rv = val->GetAs##_n(&_name);                                            \
    if (NS_FAILED(rv)) {                                                    \
      aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed getting value for property ") + NS_LITERAL_STRING(#_name)); \
      return rv;                                                            \
    }                                                                       \
    if (_name != _val) {                                                    \
      aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Value doesn't match for property ") + NS_LITERAL_STRING(#_name)); \
      return NS_ERROR_FAILURE;                                              \
    }                                                                       
                                                                             
  GET_AND_TEST(PRUint8, Int8, i8, sInt8Val)
  GET_AND_TEST(PRInt16, Int16, i16, sInt16Val)
  GET_AND_TEST(PRInt32, Int32, i32, sInt32Val)
  GET_AND_TEST(PRInt64, Int64, i64, sInt64Val)
  GET_AND_TEST(PRUint8, Uint8, u8, sUint8Val)
  GET_AND_TEST(PRUint16, Uint16, u16, sUint16Val)
  GET_AND_TEST(PRUint32, Uint32, u32, sUint32Val)
  GET_AND_TEST(PRUint64, Uint64, u64, sUint64Val)
  GET_AND_TEST(PRBool, Bool, b, sBoolVal)
  GET_AND_TEST(float, Float, f, sFloatVal)
  GET_AND_TEST(double, Double, d, sDoubleVal)
  GET_AND_TEST(char, Char, c, sCharVal)
  GET_AND_TEST(PRUnichar, WChar, wc, sWcharVal)

  GET_AND_TEST_NAME(s);
  nsAutoString str;
  rv = val->GetAsAString(str);
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed getting value for property s"));
    return rv;
  }

  if (!str.EqualsLiteral(STRING_VAL)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Value doesn't match for property s"));
    return NS_ERROR_FAILURE;
  }

  GET_AND_TEST_NAME(p)
  nsCOMPtr<nsISupports> supVal;  
  rv = val->GetAsISupports(getter_AddRefs(supVal));  
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed getting value for property p"));
    return rv;
  }
  nsCOMPtr<nsIPropertyBag> propBagVal = do_QueryInterface(supVal, &rv);
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Value doesn't match for property p"));
    return rv;
  }

  GET_AND_TEST_NAME(p2)
  PRUint16 dataType;
  val->GetDataType(&dataType);
  if (dataType != nsIDataType::VTYPE_EMPTY) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Value doesn't match for property p"));
    return NS_ERROR_FAILURE;
  }

  PRUint16 type;
  PRUint32 index, count;
  PRUint32* arrayVal1;

  GET_AND_TEST_NAME(array1)
  nsIID iid;
  rv = val->GetAsArray(&type, &iid, &count, (void**)&arrayVal1);
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed getting value for property array1"));
    return rv;
  }
  for (index = 0; index < count; index++) {
    if (arrayVal1[index] != sArray1[index]) {
      aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Value doesn't match for element of property array1"));
      return rv;      
    }
  }
  nsMemory::Free(arrayVal1);

  double* arrayVal2;
  GET_AND_TEST_NAME(array2)
  rv = val->GetAsArray(&type, &iid, &count, (void**)&arrayVal2);
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed getting value for property array2"));
    return rv;
  }
  for (index = 0; index < count; index++) {
    if (arrayVal2[index] != sArray2[index]) {
      aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Value doesn't match for element of property array2"));
      return rv;      
    }
  }
  nsMemory::Free(arrayVal2);

  nsISupports** arrayVal3;
  GET_AND_TEST_NAME(array3)
  rv = val->GetAsArray(&type, &iid, &count, (void**)&arrayVal3);
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed getting value for property array3"));
    return rv;
  }
  propBagVal = do_QueryInterface(arrayVal3[0], &rv);
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Value doesn't match for element of property array3"));
    return rv;
  }
  if (arrayVal3[1] != nsnull) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Value doesn't match for element of  property array3"));
    return NS_ERROR_FAILURE;
  }
  NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(count, arrayVal3);

#undef GET_AND_TEST
#undef GET_AND_TEST_NAME

  aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Test Succeeded!"));
  return NS_OK;
}


NS_IMETHODIMP
WSPProxyTest::TestPropertyBagWrapper(nsAString& aResult)
{
  nsCOMPtr<nsIInterfaceInfo> info;
  nsCOMPtr<nsIWebServiceComplexTypeWrapper> ctwrapper;
  nsresult rv = CreateComplexTypeWrapper(getter_AddRefs(ctwrapper),
                                         getter_AddRefs(info));
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed creating complex type wrapper"));
    return NS_OK;
  }

  nsCOMPtr<nsIPropertyBag> propBag = do_QueryInterface(ctwrapper);
  if (!propBag) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Wrapper is not property bag"));
    return NS_OK;
  }

  nsCOMPtr<nsIWebServicePropertyBagWrapper> wrapper = do_CreateInstance(NS_WEBSERVICEPROPERTYBAGWRAPPER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed creating property bag wrapper"));
    return NS_OK;
  }
  rv = wrapper->Init(propBag, info);
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed initializing property bag wrapper"));
    return NS_OK;
  }

  nsCOMPtr<nsIWSPTestComplexType> ct = do_QueryInterface(wrapper, &rv);
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Property bag wrapper doesn't QI correctly"));
    return NS_OK;
  }
  
#define GET_AND_TEST(_t, _name, _val)                                       \
  _t _name;                                                                 \
  rv = ct->Get##_name(&_name);                                              \
  if (NS_FAILED(rv)) {                                                      \
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed to get value for attribute") + NS_LITERAL_STRING(#_name)); \
    return NS_OK;                                                           \
  }                                                                         \
  if (_name != _val) {                                                      \
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Value doesn't match for attribute ") + NS_LITERAL_STRING(#_name)); \
    return NS_OK;                                                           \
  }
                                                                           
  GET_AND_TEST(PRUint8, I8, sInt8Val)
  GET_AND_TEST(PRInt16, I16, sInt16Val)
  GET_AND_TEST(PRInt32, I32, sInt32Val)
  GET_AND_TEST(PRInt64, I64, sInt64Val)
  GET_AND_TEST(PRUint8, U8, sUint8Val)
  GET_AND_TEST(PRUint16, U16, sUint16Val)
  GET_AND_TEST(PRUint32, U32, sUint32Val)
  GET_AND_TEST(PRUint64, U64, sUint64Val)
  GET_AND_TEST(float, F, sFloatVal)
  GET_AND_TEST(double, D, sDoubleVal)
  GET_AND_TEST(PRBool, B, sBoolVal)
  GET_AND_TEST(char, C, sCharVal)
  GET_AND_TEST(PRUnichar, Wc, sWcharVal)

  nsAutoString str;
  rv = ct->GetS(str);
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed to get value for attribute s"));
    return NS_OK;
  }
  if (!str.EqualsLiteral(STRING_VAL)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Value doesn't match for attribute s"));
    return NS_OK;
  }

  nsCOMPtr<nsIWSPTestComplexType> p;
  rv = ct->GetP(getter_AddRefs(p));
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed to get value for attribute p"));
    return NS_OK;
  }

  rv = ct->GetP2(getter_AddRefs(p));
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed to get value for attribute p2"));
    return NS_OK;
  }
  if (p) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Value doesn't match for attribute p2"));
    return NS_OK;
  }

  PRUint32 index, count;

  PRUint32* array1;
  rv = ct->Array1(&count, &array1);
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed to get value for attribute array1"));
    return NS_OK;
  }
  for (index = 0; index < count; index++) {
    if (array1[index] != sArray1[index]) {
      aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Value doesn't match for element of attribute array1"));
      return NS_OK;
    }
  }
  nsMemory::Free(array1);

  double* array2;
  rv = ct->Array2(&count, &array2);
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed to get value for attribute array2"));
    return NS_OK;
  }
  for (index = 0; index < count; index++) {
    if (array2[index] != sArray2[index]) {
      aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Value doesn't match for element of attribute array2"));
      return NS_OK;
    }
  }
  nsMemory::Free(array2);

  nsIWSPTestComplexType** array3;
  rv = ct->Array3(&count, &array3);
  if (NS_FAILED(rv)) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Failed to get value for attribute array3"));
    return NS_OK;
  }
  if (!array3[0] || array3[1]) {
    aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Value doesn't match for element of attribute array3"));
    return NS_OK;
  }
  NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(count, array3);

#undef GET_AND_TEST

  aResult.Assign(NS_LITERAL_STRING("WSPProxyTest: Test Succeeded!"));
  return NS_OK;
}

/* void testIsPrimeProxy(nsIWSPProxyTestListener* aListener); */
NS_IMETHODIMP
WSPProxyTest::TestIsPrimeProxy(nsIWSPProxyTestListener* aListener)
{
  mListener = aListener;
  nsCOMPtr<nsIWSDLLoader> loader = do_CreateInstance(NS_WSDLLOADER_CONTRACTID);
  if (!loader) {
    mListener->OnIsPrimeProxyTestComplete(NS_LITERAL_STRING("Couldn't create WSDL loader"));
    return NS_OK;
  }
  
#define ISPRIMEURL "http://ray.dsl.xmission.com:8080/wsdl/statistics.wsdl"
#define ISPRIMEPORT "SpheonJSAOPStatisticsPort"
  nsresult rv = loader->LoadAsync(NS_LITERAL_STRING(ISPRIMEURL),
                                  NS_LITERAL_STRING(ISPRIMEPORT),
                                  this);
  if (NS_FAILED(rv)) {
    mListener->OnIsPrimeProxyTestComplete(NS_LITERAL_STRING("Failed loading WSDL file"));
  }
  return NS_OK;
}

/* void onLoad (in nsIWSDLPort port); */
NS_IMETHODIMP 
WSPProxyTest::OnLoad(nsIWSDLPort *port)
{
  nsresult rv;
  nsCOMPtr<nsIInterfaceInfoManager> manager = do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    mListener->OnIsPrimeProxyTestComplete(NS_LITERAL_STRING("Couldn't create interface info manager"));
    return NS_OK;
  }

  static nsIID sPortIID = NS_GET_IID(SpheonJSAOPStatisticsPortTypeAsync);
  nsCOMPtr<nsIInterfaceInfo> iinfo;
  rv = manager->GetInfoForIID(&sPortIID, getter_AddRefs(iinfo));
  if (NS_FAILED(rv)) {
    mListener->OnIsPrimeProxyTestComplete(NS_LITERAL_STRING("Couldn't find interface info for port"));
    return NS_OK;
  }

  nsCOMPtr<nsIWebServiceProxy> proxy(do_CreateInstance(NS_WEBSERVICEPROXY_CONTRACTID));
  if (!proxy) {
    mListener->OnIsPrimeProxyTestComplete(NS_LITERAL_STRING("Couldn't create proxy"));
    return NS_OK;    
  }

  proxy->Init(port, iinfo, manager, NS_LITERAL_STRING("foo"), PR_TRUE);

  mProxy = do_QueryInterface(proxy);
  if (!mProxy) {
    mListener->OnIsPrimeProxyTestComplete(NS_LITERAL_STRING("Couldn't QI proxy to isPrime interface"));
    return NS_OK;
  }

  mProxy->SetListener(this);

  nsCOMPtr<nsIWebServiceCallContext> context;
  rv = mProxy->IsPrimeNumber(5, getter_AddRefs(context));
  if (NS_FAILED(rv)) {
    mListener->OnIsPrimeProxyTestComplete(NS_LITERAL_STRING("Failed call to IsPrimeNumber"));
    return NS_OK;
  }

  return NS_OK;
}

/* void onError (in nsresult status, in AString statusMessage); */
NS_IMETHODIMP 
WSPProxyTest::OnError(nsresult status, const nsAString & statusMessage)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onError (in nsIException error, in nsIWebServiceCallContext cx); */
NS_IMETHODIMP WSPProxyTest::OnError(nsIException *error, nsIWebServiceCallContext *cx)
{
  nsXPIDLCString str;
  error->ToString(getter_Copies(str));
  mListener->OnIsPrimeProxyTestComplete(NS_ConvertASCIItoUCS2(str.get()));
  return NS_OK;
}

/* void getStatisticsCallback (in statisticStruct retval, in nsIWebServiceCallContext cx); */
NS_IMETHODIMP WSPProxyTest::GetStatisticsCallback(statisticStruct *retval, nsIWebServiceCallContext *cx)
{
  if (!retval) {
    mListener->OnIsPrimeProxyTestComplete(NS_LITERAL_STRING("Incorrect value returned from IsPrimeNumber"));
  }
  else {
    double average;
    retval->GetAverage(&average);
    if (average != 2.725) {
      mListener->OnIsPrimeProxyTestComplete(NS_LITERAL_STRING("Incorrect value returned from IsPrimeNumber"));
    }
    else {
      mListener->OnIsPrimeProxyTestComplete(NS_LITERAL_STRING("Test Succeeded!"));
    }
  }

  return NS_OK;
}

/* void isPrimeNumberCallback (in boolean retval, in nsIWebServiceCallContext cx); */
NS_IMETHODIMP WSPProxyTest::IsPrimeNumberCallback(PRBool retval, nsIWebServiceCallContext *cx)
{
  if (!retval) {
    mListener->OnIsPrimeProxyTestComplete(NS_LITERAL_STRING("Incorrect value returned from IsPrimeNumber"));
  }
  else {
    // Success!!
    mListener->OnIsPrimeProxyTestComplete(NS_LITERAL_STRING("Test Succeeded!"));
#if 0
    static double vals[4] = { 1.7, 3.4, 5.6, 0.2 };
    nsCOMPtr<nsIWebServiceCallContext> context;
    nsresult rv = mProxy->GetStatistics(sizeof(vals)/sizeof(double), vals, 
                                        getter_AddRefs(context));
    if (NS_FAILED(rv)) {
      mListener->OnIsPrimeProxyTestComplete(NS_LITERAL_STRING("Failed call to GetStatistics"));
    }
#endif
  }

  return NS_OK;
}

/* void crossSumCallback (in PRInt32 retval, in nsIWebServiceCallContext cx); */
NS_IMETHODIMP WSPProxyTest::CrossSumCallback(PRInt32 retval, nsIWebServiceCallContext *cx)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

///////////////////////////////////////////////////
//
// Implementation of WSPTestComplexType
//
///////////////////////////////////////////////////
WSPTestComplexType::WSPTestComplexType()
{
}

WSPTestComplexType::~WSPTestComplexType()
{
}

NS_IMPL_ISUPPORTS1(WSPTestComplexType, nsIWSPTestComplexType)

/* readonly attribute PRUint8 i8; */
NS_IMETHODIMP 
WSPTestComplexType::GetI8(PRUint8 *aI8)
{
  *aI8 = sInt8Val;
  return NS_OK;
}

/* readonly attribute PRInt16 i16; */
NS_IMETHODIMP 
WSPTestComplexType::GetI16(PRInt16 *aI16)
{
  *aI16 = sInt16Val;
  return NS_OK;
}

/* readonly attribute PRInt32 i32; */
NS_IMETHODIMP 
WSPTestComplexType::GetI32(PRInt32 *aI32)
{
  *aI32 = sInt32Val;
  return NS_OK;
}

/* readonly attribute PRInt64 i64; */
NS_IMETHODIMP 
WSPTestComplexType::GetI64(PRInt64 *aI64)
{
  *aI64 = sInt64Val;
  return NS_OK;
}

/* readonly attribute PRUint8 u8; */
NS_IMETHODIMP 
WSPTestComplexType::GetU8(PRUint8 *aU8)
{
  *aU8 = sUint8Val;
  return NS_OK;
}

/* readonly attribute PRUint16 u16; */
NS_IMETHODIMP 
WSPTestComplexType::GetU16(PRUint16 *aU16)
{
  *aU16 = sUint16Val;
  return NS_OK;
}

/* readonly attribute PRUint32 u32; */
NS_IMETHODIMP 
WSPTestComplexType::GetU32(PRUint32 *aU32)
{
  *aU32 = sUint32Val;
  return NS_OK;
}

/* readonly attribute PRUint64 u64; */
NS_IMETHODIMP 
WSPTestComplexType::GetU64(PRUint64 *aU64)
{
  *aU64 = sUint64Val;
  return NS_OK;
}

/* readonly attribute PRBool b; */
NS_IMETHODIMP 
WSPTestComplexType::GetB(PRBool *aB)
{
  *aB = sBoolVal;
  return NS_OK;
}

/* readonly attribute float f; */
NS_IMETHODIMP 
WSPTestComplexType::GetF(float *aF)
{
  *aF = sFloatVal;
  return NS_OK;
}

/* readonly attribute double d; */
NS_IMETHODIMP 
WSPTestComplexType::GetD(double *aD)
{
  *aD = sDoubleVal;
  return NS_OK;
}

/* readonly attribute char c */
NS_IMETHODIMP
WSPTestComplexType::GetC(char *aC)
{
  *aC = sCharVal;
  return NS_OK;
}

/* readonly attribute wchar wc */
NS_IMETHODIMP
WSPTestComplexType::GetWc(PRUnichar *aWC)
{
  *aWC = sWcharVal;
  return NS_OK;
}

/* readonly attribute nsIWSPTestComplexType p; */
NS_IMETHODIMP 
WSPTestComplexType::GetP(nsIWSPTestComplexType * *aP)
{
  WSPTestComplexType* inst = new WSPTestComplexType();
  if (!inst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aP = inst;
  NS_ADDREF(*aP);
  return NS_OK;
}

/* readonly attribute nsIWSPTestComplexType p2; */
NS_IMETHODIMP 
WSPTestComplexType::GetP2(nsIWSPTestComplexType * *aP2)
{
  *aP2 = nsnull;
  return NS_OK;
}

/* readonly attribute AString s; */
NS_IMETHODIMP 
WSPTestComplexType::GetS(nsAString & aS)
{
  aS.Assign(NS_LITERAL_STRING(STRING_VAL));
  return NS_OK;
}

/* void array1 (out PRUint32 length, [array, size_is (length), retval] out PRUint32 array1); */
NS_IMETHODIMP 
WSPTestComplexType::Array1(PRUint32 *length, PRUint32 **array1)
{
  PRUint32* ptr = (PRUint32*)nsMemory::Alloc(sArray1Length * sizeof(PRUint32));
  if (!ptr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PRUint32 index;
  for (index = 0; index < sArray1Length; index++) {
    ptr[index] = sArray1[index];
  }
  
  *length = sArray1Length;
  *array1 = ptr;

  return NS_OK;
}

/* void array2 (out PRUint32 length, [array, size_is (length), retval] out double array2); */
NS_IMETHODIMP 
WSPTestComplexType::Array2(PRUint32 *length, double **array2)
{
  double* ptr = (double*)nsMemory::Alloc(sArray2Length * sizeof(double));
  if (!ptr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PRUint32 index;
  for (index = 0; index < sArray2Length; index++) {
    ptr[index] = sArray2[index];
  }
  
  *length = sArray2Length;
  *array2 = ptr;

  return NS_OK;
}

/* void array3 (out PRUint32 length, [array, size_is (length), retval] out nsIWSPTestComplexType array3); */
NS_IMETHODIMP 
WSPTestComplexType::Array3(PRUint32 *length, nsIWSPTestComplexType ***array3)
{
  nsIWSPTestComplexType** ptr = (nsIWSPTestComplexType**)nsMemory::Alloc(2 * 
                                                         sizeof(nsISupports*));
  if (!ptr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  WSPTestComplexType* inst = new WSPTestComplexType();
  if (!inst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  ptr[0] = inst;
  NS_ADDREF(ptr[0]);
  ptr[1] = nsnull;

  *length = 2;
  *array3 = ptr;
  
  return NS_OK;
}
