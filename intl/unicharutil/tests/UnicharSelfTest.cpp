/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include "nsISupports.h"
#include "nsXPCOM.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsICaseConversion.h"
#include "nsIEntityConverter.h"
#include "nsISaveAsCharset.h"
#include "nsIUnicodeEncoder.h"
#include "nsUnicharUtilCIID.h"
#include "nsIPersistentProperties2.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "nsIUnicodeNormalizer.h"
#include "nsStringAPI.h"
#include "nsUnicharUtils.h"

NS_DEFINE_CID(kEntityConverterCID, NS_ENTITYCONVERTER_CID);
NS_DEFINE_CID(kSaveAsCharsetCID, NS_SAVEASCHARSET_CID);
NS_DEFINE_CID(kUnicodeNormalizerCID, NS_UNICODE_NORMALIZER_CID);

#define TESTLEN 32
#define T2LEN TESTLEN
#define T3LEN TESTLEN
#define T4LEN TESTLEN

// test data for ToUpper 
static PRUnichar t2data  [T2LEN+1] = {
  0x0031 ,  //  0
  0x0019 ,  //  1
  0x0043 ,  //  2
  0x0067 ,  //  3
  0x00C8 ,  //  4
  0x00E9 ,  //  5
  0x0147 ,  //  6
  0x01C4 ,  //  7
  0x01C6 ,  //  8
  0x01C5 ,  //  9
  0x03C0 ,  // 10
  0x03B2 ,  // 11
  0x0438 ,  // 12
  0x04A5 ,  // 13
  0x05D0 ,  // 14
  0x0A20 ,  // 15
  0x30B0 ,  // 16
  0x5185 ,  // 17
  0xC021 ,  // 18
  0xFF48 ,  // 19
  0x01C7 ,  // 20
  0x01C8 ,  // 21
  0x01C9 ,  // 22
  0x01CA ,  // 23
  0x01CB ,  // 24
  0x01CC ,  // 25
  0x01F1 ,  // 26
  0x01F2 ,  // 27
  0x01F3 ,  // 28
  0x0250 ,  // 29
  0x0271 ,  // 30
  0xA641 ,  // 31
  0x00  
};
// expected result for ToUpper 
static PRUnichar t2result[T2LEN+1] =  {
  0x0031 ,  //  0
  0x0019 ,  //  1
  0x0043 ,  //  2
  0x0047 ,  //  3
  0x00C8 ,  //  4
  0x00C9 ,  //  5
  0x0147 ,  //  6
  0x01C4 ,  //  7
  0x01C4 ,  //  8
  0x01C4 ,  //  9
  0x03A0 ,  // 10
  0x0392 ,  // 11
  0x0418 ,  // 12
  0x04A4 ,  // 13
  0x05D0 ,  // 14
  0x0A20 ,  // 15
  0x30B0 ,  // 16
  0x5185 ,  // 17
  0xC021 ,  // 18
  0xFF28 ,  // 19
  0x01C7 ,  // 20
  0x01C7 ,  // 21
  0x01C7 ,  // 22
  0x01CA ,  // 23
  0x01CA ,  // 24
  0x01CA ,  // 25
  0x01F1 ,  // 26
  0x01F1 ,  // 27
  0x01F1 ,  // 28
  0x2C6F ,  // 29
  0x2C6E ,  // 30
  0xA640 ,  // 31
  0x00  
};
// test data for ToLower 
static PRUnichar t3data  [T3LEN+1] =  {
  0x0031 ,  //  0
  0x0019 ,  //  1
  0x0043 ,  //  2
  0x0067 ,  //  3
  0x00C8 ,  //  4
  0x00E9 ,  //  5
  0x0147 ,  //  6
  0x01C4 ,  //  7
  0x01C6 ,  //  8
  0x01C5 ,  //  9
  0x03A0 ,  // 10
  0x0392 ,  // 11
  0x0418 ,  // 12
  0x04A4 ,  // 13
  0x05D0 ,  // 14
  0x0A20 ,  // 15
  0x30B0 ,  // 16
  0x5187 ,  // 17
  0xC023 ,  // 18
  0xFF28 ,  // 19
  0x01C7 ,  // 20
  0x01C8 ,  // 21
  0x01C9 ,  // 22
  0x01CA ,  // 23
  0x01CB ,  // 24
  0x01CC ,  // 25
  0x01F1 ,  // 26
  0x01F2 ,  // 27
  0x01F3 ,  // 28
  0x2C6F ,  // 29
  0x2C6E ,  // 30
  0xA640 ,  // 31
  0x00  
};
// expected result for ToLower 
static PRUnichar t3result[T3LEN+1] =  {
  0x0031 ,  //  0
  0x0019 ,  //  1
  0x0063 ,  //  2
  0x0067 ,  //  3
  0x00E8 ,  //  4
  0x00E9 ,  //  5
  0x0148 ,  //  6
  0x01C6 ,  //  7
  0x01C6 ,  //  8
  0x01C6 ,  //  9
  0x03C0 ,  // 10
  0x03B2 ,  // 11
  0x0438 ,  // 12
  0x04A5 ,  // 13
  0x05D0 ,  // 14
  0x0A20 ,  // 15
  0x30B0 ,  // 16
  0x5187 ,  // 17
  0xC023 ,  // 18
  0xFF48 ,  // 19
  0x01C9 ,  // 20
  0x01C9 ,  // 21
  0x01C9 ,  // 22
  0x01CC ,  // 23
  0x01CC ,  // 24
  0x01CC ,  // 25
  0x01F3 ,  // 26
  0x01F3 ,  // 27
  0x01F3 ,  // 28
  0x0250 ,  // 29
  0x0271 ,  // 30
  0xA641 ,  // 31
  0x00  
};
// test data for ToTitle 
static PRUnichar t4data  [T4LEN+2] =  {
  0x0031 ,  //  0
  0x0019 ,  //  1
  0x0043 ,  //  2
  0x0067 ,  //  3
  0x00C8 ,  //  4
  0x00E9 ,  //  5
  0x0147 ,  //  6
  0x01C4 ,  //  7
  0x01C6 ,  //  8
  0x01C5 ,  //  9
  0x03C0 ,  // 10
  0x03B2 ,  // 11
  0x0438 ,  // 12
  0x04A5 ,  // 13
  0x05D0 ,  // 14
  0x0A20 ,  // 15
  0x30B0 ,  // 16
  0x5189 ,  // 17
  0xC013 ,  // 18
  0xFF52 ,  // 19
  0x01C7 ,  // 20
  0x01C8 ,  // 21
  0x01C9 ,  // 22
  0x01CA ,  // 23
  0x01CB ,  // 24
  0x01CC ,  // 25
  0x01F1 ,  // 26
  0x01F2 ,  // 27
  0x01F3 ,  // 28
  0x0250 ,  // 29
  0x0271 ,  // 30
  0xA641 ,  // 31
  0x0041 ,  // Dummy entry to prevent overflow
  0x00  
};
// expected result for ToTitle 
static PRUnichar t4result[T4LEN+2] =  {
  0x0031 ,  //  0
  0x0019 ,  //  1
  0x0043 ,  //  2
  0x0047 ,  //  3
  0x00C8 ,  //  4
  0x00C9 ,  //  5
  0x0147 ,  //  6
  0x01C4 ,  //  7
  0x01C5 ,  //  8
  0x01C5 ,  //  9
  0x03A0 ,  // 10
  0x0392 ,  // 11
  0x0418 ,  // 12
  0x04A4 ,  // 13
  0x05D0 ,  // 14
  0x0A20 ,  // 15
  0x30B0 ,  // 16
  0x5189 ,  // 17
  0xC013 ,  // 18
  0xFF32 ,  // 19
  0x01C7 ,  // 20
  0x01C8 ,  // 21
  0x01C8 ,  // 22
  0x01CA ,  // 23
  0x01CB ,  // 24
  0x01CB ,  // 25
  0x01F1 ,  // 26
  0x01F2 ,  // 27
  0x01F2 ,  // 28
  0x2C6F ,  // 29
  0x2C6E ,  // 30
  0xA640 ,  // 31
  0x0041 ,  // Dummy entry to prevent overflow
  0x00  
};
 
static unsigned char t6lhs[] = {
  0x31 ,       //  0
  0x19 ,       //  1
  0x43 ,       //  2
  0x67 ,       //  3
  0xC3, 0x88 , //  4
  0xC3, 0xA9 , //  5
  0xC5, 0x87 , //  6
  0xC7, 0x84 , //  7
  0xC7, 0x86 , //  8
  0xC7, 0x85 , //  9
  0xCF, 0x80 ,  // 10
  0xCE, 0xB2 ,  // 11
  0xD0, 0xB8 ,  // 12
  0xD2, 0xA5 ,  // 13
  0xD7, 0x90 ,  // 14
  0xE0, 0xA8, 0xA0 ,  // 15
  0xE3, 0x82, 0xB0 ,  // 16
  0xE5, 0x86, 0x85 ,  // 17
  0xEC, 0x80, 0xA1 ,  // 18
  0xEF, 0xBD, 0x88 ,  // 19
  0xC7, 0x87 ,  // 20
  0xC7, 0x88 ,  // 21
  0xC7, 0x89 ,  // 22
  0xC7, 0x8A ,  // 23
  0xC7, 0x8B ,  // 24
  0xC7, 0x8C ,  // 25
  0xC7, 0xB1 ,  // 26
  0xC7, 0xB2 ,  // 27
  0xC7, 0xB3 ,  // 28
  0xC9, 0x90 ,  // 29
  0xC9, 0xB1 ,  // 30
  0xEA, 0x99, 0x81 ,  // 31
  0x00  
};

static unsigned char t6rhs[] =  {
  0x31 ,  //  0
  0x19 ,  //  1
  0x43 ,  //  2
  0x47 ,  //  3
  0xC3, 0x88 ,  //  4
  0xC3, 0x89 ,  //  5
  0xC5, 0x87 ,  //  6
  0xC7, 0x84 ,  //  7
  0xC7, 0x84 ,  //  8
  0xC7, 0x84 ,  //  9
  0xCE, 0xA0 ,  // 10
  0xCE, 0x92 ,  // 11
  0xD0, 0x98 ,  // 12
  0xD2, 0xA4 ,  // 13
  0xD7, 0x90 ,  // 14
  0xE0, 0xA8, 0xA0 ,  // 15
  0xE3, 0x82, 0xB0 ,  // 16
  0xE5, 0x86, 0x85 ,  // 17
  0xEC, 0x80, 0xA1 ,  // 18
  0xEF, 0xBC, 0xA8 ,  // 19
  0xC7, 0x87 ,  // 20
  0xC7, 0x87 ,  // 21
  0xC7, 0x87 ,  // 22
  0xC7, 0x8a ,  // 23
  0xC7, 0x8a ,  // 24
  0xC7, 0x8a ,  // 25
  0xC7, 0xB1 ,  // 26
  0xC7, 0xB1 ,  // 27
  0xC7, 0xB1 ,  // 28
  0xE2, 0xB1, 0xAF ,  // 29
  0xE2, 0xB1, 0xAE ,  // 30
  0xEA, 0x99, 0x80 ,  // 31
  0x00  
};

static const char *t7lhs = "aBcDeFGHIJKL1!!2!!a!uuuu";
static const char *t7rhs = "AbCdEFghijkL1!!2!!A!UUuU";

static const char *t8lhs = "aazzz";
static const char *t8rhs = "aBa";

static const char *t9lhs = "@a";
static const char *t9rhs = "`a";

bool CharByCharCompareEqual(const char *a, const char *b,
                            PRUint32 aLen, PRUint32 bLen)
{
  // Do basically a CaseInsensitiveCompare(), but using
  // CaseInsensitiveUTF8CharsEqual().

  const char *aEnd = a + aLen;
  const char *bEnd = b + bLen;
  while (a < aEnd && b < bEnd) {
    bool err;
    if (!CaseInsensitiveUTF8CharsEqual(a, b, aEnd, bEnd, &a, &b, &err) || err)
      return false;
  }
  return true;
}

void TestCaseConversion()
{
  printf("==========================\n");
  printf("Start case conversion test\n");
  printf("==========================\n");

  int i;
  PRUnichar buf[256];

  printf("Test 1 - ToUpper(PRUnichar, PRUnichar*):\n");
  for(i=0;i < T2LEN ; i++)
  {
    PRUnichar ch = ToUpperCase(t2data[i]);
    if(ch != t2result[i])
      printf("\tFailed!! result unexpected %d\n", i);
  }


  printf("Test 2 - ToLower(PRUnichar, PRUnichar*):\n");
  for(i=0;i < T3LEN; i++)
  {
    PRUnichar ch = ToLowerCase(t3data[i]);
    if(ch != t3result[i])
      printf("\tFailed!! result unexpected %d\n", i);
  }

  printf("Test 3 - ToTitle(PRUnichar, PRUnichar*):\n");
  for(i=0;i < T4LEN; i++)
  {
    PRUnichar ch = ToTitleCase(t4data[i]);
    if(ch != t4result[i])
      printf("\tFailed!! result unexpected %d\n", i);
  }

  printf("Test 4 - ToUpper(PRUnichar*, PRUnichar*, PRUint32):\n");
  ToUpperCase(t2data, buf, T2LEN);
  for(i = 0; i < T2LEN; i++)
  {
     if(buf[i] != t2result[i])
     {
       printf("\tFailed!! result unexpected %d\n", i);
       break;
     }
  }

  printf("Test 5 - ToLower(PRUnichar*, PRUnichar*, PRUint32):\n");
  ToLowerCase(t3data, buf, T3LEN);
  for(i = 0; i < T3LEN; i++)
  {
     if(buf[i] != t3result[i])
     {
       printf("\tFailed!! result unexpected %d\n", i);
       break;
     }
  }

  printf("Test 6 - CaseInsensitiveCompare UTF-8 (1):\n");
  if (CaseInsensitiveCompare((char*)t6lhs, (char*)t6rhs, sizeof(t6lhs), sizeof(t6rhs)))
    printf("\tFailed!\n");
  if (!CharByCharCompareEqual((char*)t6lhs, (char*)t6rhs, sizeof(t6lhs), sizeof(t6rhs)))
    printf("\tFailed character-by-character comparison!\n");

  printf("Test 7 - CaseInsensitiveCompare UTF-8 (2):\n");
  if (CaseInsensitiveCompare(t7lhs, t7rhs, strlen(t7lhs), strlen(t7rhs)))
    printf("\tFailed!\n");
  if (!CharByCharCompareEqual(t7lhs, t7rhs, sizeof(t7lhs), sizeof(t7rhs)))
    printf("\tFailed character-by-character comparison!\n");

  printf("Test 8a - CaseInsensitiveCompare UTF-8 (3):\n");
  if (CaseInsensitiveCompare(t8lhs, t8rhs, strlen(t8lhs), strlen(t8rhs)) != -1)
    printf("\tFailed!\n");
  if (CharByCharCompareEqual(t8lhs, t8rhs, strlen(t8lhs), strlen(t8rhs)))
    printf("\tFailed character-by-character comparison!\n");

  printf("Test 8b - CaseInsensitiveCompare UTF-8 (4):\n");
  if (CaseInsensitiveCompare(t8rhs, t8lhs, strlen(t8rhs), strlen(t8lhs)) != 1)
    printf("\tFailed!\n");

  // This test may seem a bit strange.  But it's actually an easy bug to make
  // if we tried to be clever and say that two ASCII characters x and y are
  // case-insensitively equal if (x & ~0x20) == (y & ~0x20).
  printf("Test 9 - CaseInsensitiveCompare UTF-8 (5):\n");
  if (CaseInsensitiveCompare(t9rhs, t9lhs, strlen(t9lhs), strlen(t9rhs)) != 1)
    printf("\tFailed!\n");
  if (CharByCharCompareEqual(t9lhs, t9rhs, strlen(t9lhs), strlen(t9rhs)))
    printf("\tFailed character-by-character comparison!\n");

  printf("===========================\n");
  printf("Finish case conversion test\n");
  printf("===========================\n");
}

static void FuzzOneInvalidCaseConversion()
{
  PRUint32 aLen = rand() % 32;
  PRUint32 bLen = rand() % 32;

  // We could use a static length-32 buffer for these, but then Valgrind
  // wouldn't be able to detect errors.
  unsigned char *aBuf = (unsigned char*)malloc(aLen * sizeof(unsigned char));
  unsigned char *bBuf = (unsigned char*)malloc(bLen * sizeof(unsigned char));

  for (PRUint32 i = 0; i < aLen; i++) {
    aBuf[i] = rand() & 0xff;
  }

  for (PRUint32 i = 0; i < bLen; i++) {
    bBuf[i] = rand() & 0xff;
  }

  if (!CaseInsensitiveCompare((char*)aBuf, (char*)bBuf, aLen, bLen))
    printf("\tSurprise, two random strings compared insensitively as equal!\n");
  if (CharByCharCompareEqual((char*)aBuf, (char*)bBuf, aLen, bLen))
    printf("\tSurprise, two random strings compared as exactly equal!\n");

  free(aBuf);
  free(bBuf);
}

static void FuzzCaseConversion()
{
  printf("==========================\n");
  printf("Start fuzz case conversion\n");
  printf("==========================\n");

  srand(0);

  printf("Fuzzing invalid UTF8 data...\n");
  for (PRUint32 i = 0; i < 100000; i++) {
    FuzzOneInvalidCaseConversion();
  }

  printf("===========================\n");
  printf("Finish fuzz case conversion\n");
  printf("===========================\n");
}

static void TestEntityConversion(PRUint32 version)
{
  printf("==============================\n");
  printf("Start nsIEntityConverter Test \n");
  printf("==============================\n");

  PRUint32 i;
  nsString inString;
  PRUnichar uChar;
  nsresult res;


  inString.Assign(NS_ConvertASCIItoUTF16("\xA0\xA1\xA2\xA3"));
  uChar = (PRUnichar) 8364; //euro
  inString.Append(&uChar, 1);
  uChar = (PRUnichar) 9830; //
  inString.Append(&uChar, 1);

  nsCOMPtr <nsIEntityConverter> entityConv = do_CreateInstance(kEntityConverterCID, &res);;
  if (NS_FAILED(res)) {printf("\tFailed!! return value != NS_OK\n"); return;}

  const PRUnichar *data;
  PRUint32 length = NS_StringGetData(inString, &data);

  // convert char by char
  for (i = 0; i < length; i++) {
    char *entity = NULL;
    res = entityConv->ConvertToEntity(data[i], version, &entity);
    if (NS_SUCCEEDED(res) && NULL != entity) {
      printf("%c %s\n", data[i], entity);
      nsMemory::Free(entity);
    }
  }

  // convert at once as a string
  PRUnichar *entities;
  res = entityConv->ConvertToEntities(inString.get(), version, &entities);
  if (NS_SUCCEEDED(res) && NULL != entities) {
    for (PRUnichar *centity = entities; *centity; ++centity) {
      printf("%c", (char) *centity);
      if (';' == (char) *centity)
        printf("\n");
    }
    nsMemory::Free(entities);
  }

  printf("==============================\n");
  printf("Finish nsIEntityConverter Test \n");
  printf("==============================\n\n");
}

static void TestSaveAsCharset()
{
  printf("==============================\n");
  printf("Start nsISaveAsCharset Test \n");
  printf("==============================\n");

  nsresult res;

  nsString inString;
  inString.Assign(NS_ConvertASCIItoUTF16("\x61\x62\x80\xA0\x63"));
  char *outString;
  
  const PRUnichar *data;
  PRUint32 length = NS_StringGetData(inString, &data);

  // first, dump input string
  for (PRUint32 i = 0; i < length; i++) {
    printf("%c ", data[i]);
  }
  printf("\n");

  nsCOMPtr <nsISaveAsCharset> saveAsCharset = do_CreateInstance(kSaveAsCharsetCID, &res);
  if (NS_FAILED(res)) {printf("\tFailed!! return value != NS_OK\n");}
  
  printf("ISO-8859-1 attr_plainTextDefault entityNone\n");
  res = saveAsCharset->Init("ISO-8859-1", 
                                 nsISaveAsCharset::attr_plainTextDefault, 
                                 nsIEntityConverter::entityNone);
  if (NS_FAILED(res)) {printf("\tFailed!! return value != NS_OK\n");}
  res = saveAsCharset->Convert(inString.get(), &outString);
  if (NS_FAILED(res)) {printf("\tFailed!! return value != NS_OK\n");}
  if (NULL == outString) {printf("\tFailed!! output null\n");}
  else {printf("%s\n", outString); nsMemory::Free(outString);}

  printf("ISO-2022-JP attr_plainTextDefault entityNone\n");
  res = saveAsCharset->Init("ISO-2022-JP", 
                                 nsISaveAsCharset::attr_plainTextDefault,
                                 nsIEntityConverter::entityNone);
  if (NS_FAILED(res)) {printf("\tFailed!! return value != NS_OK\n");}
  res = saveAsCharset->Convert(inString.get(), &outString);
  if (NS_FAILED(res)) {printf("\tFailed!! return value != NS_OK\n");}
  if (NULL == outString) {printf("\tFailed!! output null\n");}
  else {printf("%s\n", outString); nsMemory::Free(outString);}
  if (NS_ERROR_UENC_NOMAPPING == res) {
    outString = ToNewUTF8String(inString);
    if (NULL == outString) {printf("\tFailed!! output null\n");}
    else {printf("Fall back to UTF-8: %s\n", outString); nsMemory::Free(outString);}
  }

  printf("ISO-2022-JP attr_FallbackQuestionMark entityNone\n");
  res = saveAsCharset->Init("ISO-2022-JP", 
                                 nsISaveAsCharset::attr_FallbackQuestionMark,
                                 nsIEntityConverter::entityNone);
  if (NS_FAILED(res)) {printf("\tFailed!! return value != NS_OK\n");}
  res = saveAsCharset->Convert(inString.get(), &outString);
  if (NS_FAILED(res)) {printf("\tFailed!! return value != NS_OK\n");}
  if (NULL == outString) {printf("\tFailed!! output null\n");}
  else {printf("%s\n", outString); nsMemory::Free(outString);}

  printf("ISO-2022-JP attr_FallbackEscapeU entityNone\n");
  res = saveAsCharset->Init("ISO-2022-JP", 
                                 nsISaveAsCharset::attr_FallbackEscapeU,
                                 nsIEntityConverter::entityNone);
  if (NS_FAILED(res)) {printf("\tFailed!! return value != NS_OK\n");}
  res = saveAsCharset->Convert(inString.get(), &outString);
  if (NS_FAILED(res)) {printf("\tFailed!! return value != NS_OK\n");}
  if (NULL == outString) {printf("\tFailed!! output null\n");}
  else {printf("%s\n", outString); nsMemory::Free(outString);}

  printf("ISO-8859-1 attr_htmlTextDefault html40Latin1\n");
  res = saveAsCharset->Init("ISO-8859-1", 
                                 nsISaveAsCharset::attr_htmlTextDefault, 
                                 nsIEntityConverter::html40Latin1);
  if (NS_FAILED(res)) {printf("\tFailed!! return value != NS_OK\n");}
  res = saveAsCharset->Convert(inString.get(), &outString);
  if (NS_ERROR_UENC_NOMAPPING != res && NS_FAILED(res)) {printf("\tFailed!! return value != NS_OK\n");}
  if (NULL == outString) {printf("\tFailed!! output null\n");}
  else {printf("%s\n", outString); nsMemory::Free(outString);}

  printf("ISO-8859-1 attr_FallbackHexNCR+attr_EntityAfterCharsetConv html40Latin1 \n");
  res = saveAsCharset->Init("ISO-8859-1", 
                                 nsISaveAsCharset::attr_FallbackHexNCR + 
                                 nsISaveAsCharset::attr_EntityAfterCharsetConv, 
                                 nsIEntityConverter::html40Latin1);
  if (NS_FAILED(res)) {printf("\tFailed!! return value != NS_OK\n");}
  res = saveAsCharset->Convert(inString.get(), &outString);
  if (NS_ERROR_UENC_NOMAPPING != res && NS_FAILED(res)) {printf("\tFailed!! return value != NS_OK\n");}
  if (NULL == outString) {printf("\tFailed!! output null\n");}
  else {printf("%s\n", outString); nsMemory::Free(outString);}


  printf("==============================\n");
  printf("Finish nsISaveAsCharset Test \n");
  printf("==============================\n\n");
}

static PRUnichar normStr[] = 
{
  0x00E1,   
  0x0061,
  0x0301,
  0x0107,
  0x0063,
  0x0301,
  0x0000
};

static PRUnichar nfdForm[] = 
{
  0x0061,
  0x0301,
  0x0061,
  0x0301,
  0x0063,
  0x0301,
  0x0063,
  0x0301,
  0x0000
};

void TestNormalization()
{
   printf("==============================\n");
   printf("Start nsIUnicodeNormalizer Test \n");
   printf("==============================\n");
   nsIUnicodeNormalizer *t = NULL;
   nsresult res;
   res = CallGetService(kUnicodeNormalizerCID, &t);
           
   printf("Test 1 - GetService():\n");
   if(NS_FAILED(res) || ( t == NULL ) ) {
     printf("\t1st Norm GetService failed\n");
   } else {
     NS_RELEASE(t);
   }

   res = CallGetService(kUnicodeNormalizerCID, &t);
           
   if(NS_FAILED(res) || ( t == NULL ) ) {
     printf("\t2nd GetService failed\n");
   } else {
    printf("Test 2 - NormalizeUnicode(PRUint32, const nsAString&, nsAString&):\n");
    nsAutoString resultStr;
    res =  t->NormalizeUnicodeNFD(nsDependentString(normStr), resultStr);
    if (resultStr.Equals(nsDependentString(nfdForm))) {
      printf(" Succeeded in NFD UnicodeNormalizer test. \n");
    } else {
      printf(" Failed in NFD UnicodeNormalizer test. \n");
    }

    NS_RELEASE(t);
   }
   printf("==============================\n");
   printf("Finish nsIUnicodeNormalizer Test \n");
   printf("==============================\n");

}


int main(int argc, char** argv) {
   
   nsresult rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
   if (NS_FAILED(rv)) {
      printf("NS_InitXPCOM2 failed\n");
      return 1;
   }

   // --------------------------------------------

   TestCaseConversion();

   // --------------------------------------------

   FuzzCaseConversion();

   // --------------------------------------------

   TestEntityConversion(nsIEntityConverter::html40);

   // --------------------------------------------

   TestSaveAsCharset();

   // --------------------------------------------

   TestNormalization();

   // --------------------------------------------
   printf("Finish All The Test Cases\n");

   return 0;
}
