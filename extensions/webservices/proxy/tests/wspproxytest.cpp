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

#include "wspproxytest.h"
#include "nsAString.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIServiceManager.h"

const PRUint8 sInt8Val = 2;
const PRInt16 sInt16Val = 0x1234;
const PRInt32 sInt32Val = 0x12345678;
#ifdef HAVE_LONG_LONG
const PRInt64 sInt64Val = 0x1234567887654321;
#else
const PRInt64 sInt64Val = {0x12345678, 0x87654321};
#endif
const PRUint8  sUint8Val = 2;
const PRUint16 sUint16Val = 0x1234;
const PRUint32 sUint32Val = 0x12345678;
#ifdef HAVE_LONG_LONG
const PRUint64 sUint64Val = 0x1234567887654321;
#else
const PRUint64 sUint64Val = {0x12345678, 0x87654321};
#endif
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
  NS_INIT_ISUPPORTS();
}

WSPProxyTest::~WSPProxyTest()
{
}

NS_IMPL_ISUPPORTS1(WSPProxyTest, nsIWSPProxyTest)

nsresult
WSPProxyTest::CreateComplexTypeWrapper(WSPComplexTypeWrapper** aWrapper,
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

  return WSPComplexTypeWrapper::Create(ct, iinfo, aWrapper);
}

NS_IMETHODIMP
WSPProxyTest::TestComplexTypeWrapper()
{
  nsCOMPtr<nsIInterfaceInfo> info;
  nsCOMPtr<WSPComplexTypeWrapper> wrapper;
  nsresult rv = CreateComplexTypeWrapper(getter_AddRefs(wrapper),
                                         getter_AddRefs(info));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIPropertyBag> propBag = do_QueryInterface(wrapper);
  if (!propBag) {
    return NS_ERROR_FAILURE;
  }

  rv = TestComplexTypeWrapperInstance(propBag);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}


nsresult
WSPProxyTest::TestComplexTypeWrapperInstance(nsIPropertyBag* propBag)
{
  nsCOMPtr<nsISimpleEnumerator> enumerator;
  nsresult rv = propBag->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsISupports> sup;
  nsCOMPtr<nsIProperty> prop;
  nsCOMPtr<nsIVariant> val;
  nsAutoString propName;

#define GET_AND_TEST_NAME(_name)                                          \
    rv = enumerator->GetNext(getter_AddRefs(sup));                        \
    if (NS_FAILED(rv)) {                                                  \
      printf("WSPProxyTest: Failed getting property " #_name "\n");       \
      return rv;                                                          \
    }                                                                     \
                                                                          \
    prop = do_QueryInterface(sup, &rv);                                   \
    if (NS_FAILED(rv)) {                                                  \
      return rv;                                                          \
    }                                                                     \
                                                                          \
    prop->GetName(propName);                                              \
    if (!propName.Equals(NS_LITERAL_STRING(#_name))) {                    \
      printf("WSPProxyTest: Name doesn't match for property " #_name "\n");\
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
      printf("WSPProxyTest: Failed getting value for property " #_name "\n");\
      return rv;                                                            \
    }                                                                       \
    if (_name != _val) {                                                    \
      printf("WSPProxyTest: Value doesn't match for property " #_name "\n");\
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
    printf("WSPProxyTest: Failed getting value for property s\n");
    return rv;
  }

  if (!str.Equals(NS_LITERAL_STRING(STRING_VAL))) {
    printf("WSPProxyTest: Value doesn't match for property s\n");
    return NS_ERROR_FAILURE;
  }

  GET_AND_TEST_NAME(p)
  nsCOMPtr<nsISupports> supVal;  
  rv = val->GetAsISupports(getter_AddRefs(supVal));  
  if (NS_FAILED(rv)) {
    printf("WSPProxyTest: Failed getting value for property p\n");
    return rv;
  }
  nsCOMPtr<nsIPropertyBag> propBagVal = do_QueryInterface(supVal, &rv);
  if (NS_FAILED(rv)) {
    printf("WSPProxyTest: Value doesn't match for property p\n");
    return rv;
  }

  GET_AND_TEST_NAME(p2)
  PRUint16 dataType;
  val->GetDataType(&dataType);
  if (dataType != nsIDataType::TYPE_EMPTY) {
    printf("WSPProxyTest: Value doesn't match for property p\n");
    return NS_ERROR_FAILURE;
  }

  PRUint16 type;
  PRUint32 index, count;
  nsIVariant** arrayVal;

  GET_AND_TEST_NAME(array1)
  rv = val->GetAsArray(&type, &count, (void**)&arrayVal);
  if (NS_FAILED(rv)) {
    printf("WSPProxyTest: Failed getting value for property array1\n");
    return rv;
  }
  for (index = 0; index < count; index++) {
    nsIVariant* arrayVar = arrayVal[index];
    PRUint32 arrayInt;
    rv = arrayVar->GetAsUint32(&arrayInt);
    if (NS_FAILED(rv)) {
      printf("WSPProxyTest: Failed getting value for element of property array1\n");
      return rv;
    }
    
    if (arrayInt != sArray1[index]) {
      printf("WSPProxyTest: Value doesn't match for element of property array1\n");
      return rv;      
    }
  }
  NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(count, arrayVal);

  GET_AND_TEST_NAME(array2)
  rv = val->GetAsArray(&type, &count, (void**)&arrayVal);
  if (NS_FAILED(rv)) {
    printf("WSPProxyTest: Failed getting value for property array2\n");
    return rv;
  }
  for (index = 0; index < count; index++) {
    nsIVariant* arrayVar = arrayVal[index];
    double arrayDouble;
    rv = arrayVar->GetAsDouble(&arrayDouble);
    if (NS_FAILED(rv)) {
      printf("WSPProxyTest: Failed getting value for element of property array1\n");
      return rv;
    }
    
    if (arrayDouble != sArray2[index]) {
      printf("WSPProxyTest: Value doesn't match for element of property array1\n");
      return rv;      
    }
  }
  NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(count, arrayVal);

  GET_AND_TEST_NAME(array3)
  rv = val->GetAsArray(&type, &count, (void**)&arrayVal);
  if (NS_FAILED(rv)) {
    printf("WSPProxyTest: Failed getting value for property array2\n");
    return rv;
  }
  nsIVariant* array3Var = arrayVal[0];
  rv = array3Var->GetAsISupports(getter_AddRefs(supVal));  
  if (NS_FAILED(rv)) {
    printf("WSPProxyTest: Failed getting value for element of property array3\n");
    return rv;
  }
  propBagVal = do_QueryInterface(supVal, &rv);
  if (NS_FAILED(rv)) {
    printf("WSPProxyTest: Value doesn't match for element of property array3\n");
    return rv;
  }
  array3Var = arrayVal[1];
  array3Var->GetDataType(&dataType);
  if (dataType != nsIDataType::TYPE_EMPTY) {
    printf("WSPProxyTest: Value doesn't match for element of  property array3\n");
    return NS_ERROR_FAILURE;
  }
  NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(count, arrayVal);

#undef GET_AND_TEST
#undef GET_AND_TEST_NAME

  return NS_OK;
}


NS_IMETHODIMP
WSPProxyTest::TestPropertyBagWrapper()
{
  nsCOMPtr<nsIInterfaceInfo> info;
  nsCOMPtr<WSPComplexTypeWrapper> ctwrapper;
  nsresult rv = CreateComplexTypeWrapper(getter_AddRefs(ctwrapper),
                                         getter_AddRefs(info));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIPropertyBag> propBag = do_QueryInterface(ctwrapper);
  if (!propBag) {
    return NS_ERROR_FAILURE;
  }

  WSPPropertyBagWrapper* wrapper;
  rv = WSPPropertyBagWrapper::Create(propBag, info, &wrapper);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIWSPTestComplexType> ct = do_QueryInterface(wrapper, &rv);
  NS_RELEASE(wrapper);
  if (NS_FAILED(rv)) {
    return rv;
  }
  
#define GET_AND_TEST(_t, _name, _val)                                       \
  _t _name;                                                                 \
  rv = ct->Get##_name(&_name);                                              \
  if (NS_FAILED(rv)) {                                                      \
    printf("WSPProxyTest: Failed to get value for attribute" #_name "\n");  \
    return rv;                                                              \
  }                                                                         \
  if (_name != _val) {                                                      \
    printf("WSPProxyTest: Value doesn't match for attribute " #_name "\n"); \
    return NS_ERROR_FAILURE;                                                \
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
    printf("WSPProxyTest: Failed to get value for attribute s\n");
    return rv;
  }
  if (!str.Equals(NS_LITERAL_STRING(STRING_VAL))) {
    printf("WSPProxyTest: Value doesn't match for attribute s\n");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIWSPTestComplexType> p;
  rv = ct->GetP(getter_AddRefs(p));
  if (NS_FAILED(rv)) {
    printf("WSPProxyTest: Failed to get value for attribute p\n");
    return rv;
  }

  rv = ct->GetP2(getter_AddRefs(p));
  if (NS_FAILED(rv)) {
    printf("WSPProxyTest: Failed to get value for attribute p2\n");
    return rv;
  }
  if (p) {
    printf("WSPProxyTest: Value doesn't match for attribute p2\n");
    return NS_ERROR_FAILURE;
  }

  PRUint32 index, count;

  PRUint32* array1;
  rv = ct->Array1(&count, &array1);
  if (NS_FAILED(rv)) {
    printf("WSPProxyTest: Failed to get value for attribute array1\n");
    return rv;
  }
  for (index = 0; index < count; index++) {
    if (array1[index] != sArray1[index]) {
      printf("WSPProxyTest: Value doesn't match for element of attribute array1\n");
      return NS_ERROR_FAILURE;
    }
  }
  nsMemory::Free(array1);

  double* array2;
  rv = ct->Array2(&count, &array2);
  if (NS_FAILED(rv)) {
    printf("WSPProxyTest: Failed to get value for attribute array2\n");
    return rv;
  }
  for (index = 0; index < count; index++) {
    if (array2[index] != sArray2[index]) {
      printf("WSPProxyTest: Value doesn't match for element of attribute array2\n");
      return NS_ERROR_FAILURE;
    }
  }
  nsMemory::Free(array2);

  nsIWSPTestComplexType** array3;
  rv = ct->Array3(&count, &array3);
  if (NS_FAILED(rv)) {
    printf("WSPProxyTest: Failed to get value for attribute array3\n");
    return rv;
  }
  if (!array3[0] || array3[1]) {
    printf("WSPProxyTest: Value doesn't match for element of attribute array3\n");
    return NS_ERROR_FAILURE;
  }
  NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(count, array3);

#undef GET_AND_TEST

  return NS_OK;
}

WSPTestComplexType::WSPTestComplexType()
{
  NS_INIT_ISUPPORTS();
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


int main (int argc, char* argv[]) 
{
  nsIServiceManager *servMgr;

  nsresult rv = NS_InitXPCOM(&servMgr, nsnull);
  if (NS_FAILED(rv)) {
    printf("failed to initialize XPCOM");
    return rv;
  }

  nsCOMPtr<WSPProxyTest> test = new WSPProxyTest();
  if (!test) {
    printf("Error creating proxy test instance\n");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  printf("--- Testing complex type wrapper --- \n");
  rv = test->TestComplexTypeWrapper();
  if (NS_FAILED(rv)) {
    printf("--- Test FAILED --- \n");
  }
  else {
    printf("--- Test SUCCEEDED --- \n");
  }

  printf("--- Testing property bag wrapper --- \n");
  rv = test->TestPropertyBagWrapper();
  if (NS_FAILED(rv)) {
    printf("--- Test FAILED --- \n");
  }
  else {
    printf("--- Test SUCCEEDED --- \n");
  }

  if (servMgr)
    rv = NS_ShutdownXPCOM(servMgr);

  return rv;
}
