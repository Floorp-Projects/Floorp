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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsIUnicodeNormalizer.h"
#include "nsString.h"

NS_DEFINE_CID(kUnicharUtilCID, NS_UNICHARUTIL_CID);
NS_DEFINE_IID(kCaseConversionIID, NS_ICASECONVERSION_IID);
NS_DEFINE_CID(kEntityConverterCID, NS_ENTITYCONVERTER_CID);
NS_DEFINE_CID(kSaveAsCharsetCID, NS_SAVEASCHARSET_CID);
NS_DEFINE_CID(kUnicodeNormalizerCID, NS_UNICODE_NORMALIZER_CID);

#define TESTLEN 29
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
  0x00  
};
// test data for ToTitle 
static PRUnichar t4data  [T4LEN+1] =  {
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
  0x00  
};
// expected result for ToTitle 
static PRUnichar t4result[T4LEN+1] =  {
  0x0031 ,  //  0
  0x0019 ,  //  1
  0x0043 ,  //  2
  0x0047 ,  //  3
  0x00C8 ,  //  4
  0x00C9 ,  //  5
  0x0147 ,  //  6
  0x01C5 ,  //  7
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
  0x01C8 ,  // 20
  0x01C8 ,  // 21
  0x01C8 ,  // 22
  0x01CB ,  // 23
  0x01CB ,  // 24
  0x01CB ,  // 25
  0x01F2 ,  // 26
  0x01F2 ,  // 27
  0x01F2 ,  // 28
  0x00  
};

void TestCaseConversion()
{
   printf("==============================\n");
   printf("Start nsICaseConversion Test \n");
   printf("==============================\n");
   nsICaseConversion *t = NULL;
   nsresult res;
   res = nsServiceManager::GetService(kUnicharUtilCID,
                                kCaseConversionIID,
                                (nsISupports**) &t);
           
   printf("Test 1 - GetService():\n");
   if(NS_FAILED(res) || ( t == NULL ) ) {
     printf("\t1st GetService failed\n");
   } else {
     res = nsServiceManager::ReleaseService(kUnicharUtilCID, t);
   }

   res = nsServiceManager::GetService(kUnicharUtilCID,
                                kCaseConversionIID,
                                (nsISupports**) &t);
           
   if(NS_FAILED(res) || ( t == NULL ) ) {
     printf("\t2nd GetService failed\n");
   } else {
     int i;
     PRUnichar ch;
     PRUnichar buf[256];

    printf("Test 2 - ToUpper(PRUnichar, PRUnichar*):\n");
    for(i=0;i < T2LEN ; i++)
    {
         res = t->ToUpper(t2data[i], &ch);
         if(NS_FAILED(res)) {
            printf("\tFailed!! return value != NS_OK\n");
            break;
         }
         if(ch != t2result[i]) 
            printf("\tFailed!! result unexpected %d\n", i);
     }


    printf("Test 3 - ToLower(PRUnichar, PRUnichar*):\n");
    for(i=0;i < T3LEN; i++)
    {
         res = t->ToLower(t3data[i], &ch);
         if(NS_FAILED(res)) {
            printf("\tFailed!! return value != NS_OK\n");
            break;
         }
         if(ch != t3result[i]) 
            printf("\tFailed!! result unexpected %d\n", i);
     }


    printf("Test 4 - ToTitle(PRUnichar, PRUnichar*):\n");
    for(i=0;i < T4LEN; i++)
    {
         res = t->ToTitle(t4data[i], &ch);
         if(NS_FAILED(res)) {
            printf("\tFailed!! return value != NS_OK\n");
            break;
         }
         if(ch != t4result[i]) 
            printf("\tFailed!! result unexpected %d\n", i);
     }


    printf("Test 5 - ToUpper(PRUnichar*, PRUnichar*, PRUint32):\n");
    res = t->ToUpper(t2data, buf, T2LEN);
    if(NS_FAILED(res)) {
       printf("\tFailed!! return value != NS_OK\n");
    } else {
       for(i = 0; i < T2LEN; i++)
       {
          if(buf[i] != t2result[i])
          {
            printf("\tFailed!! result unexpected %d\n", i);
            break;
          }
       }
    }

    printf("Test 6 - ToLower(PRUnichar*, PRUnichar*, PRUint32):\n");
    res = t->ToLower(t3data, buf, T3LEN);
    if(NS_FAILED(res)) {
       printf("\tFailed!! return value != NS_OK\n");
    } else {
       for(i = 0; i < T3LEN; i++)
       {
          if(buf[i] != t3result[i])
          {
            printf("\tFailed!! result unexpected %d\n", i);
            break;
          }
       }
    }

     printf("Test 7 - ToTitle(PRUnichar*, PRUnichar*, PRUint32):\n");
     printf("!!! To Be Implemented !!!\n");

   res = nsServiceManager::ReleaseService(kUnicharUtilCID, t);
   }
   printf("==============================\n");
   printf("Finish nsICaseConversion Test \n");
   printf("==============================\n");

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


  inString.Assign(NS_ConvertASCIItoUCS2("\xA0\xA1\xA2\xA3"));
  uChar = (PRUnichar) 8364; //euro
  inString.Append(&uChar, 1);
  uChar = (PRUnichar) 9830; //
  inString.Append(&uChar, 1);

  nsCOMPtr <nsIEntityConverter> entityConv = do_CreateInstance(kEntityConverterCID, &res);;
  if (NS_FAILED(res)) {printf("\tFailed!! return value != NS_OK\n"); return;}

  // convert char by char
  for (i = 0; i < inString.Length(); i++) {
    char *entity = NULL;
    res = entityConv->ConvertToEntity(inString[i], version, &entity);
    if (NS_SUCCEEDED(res) && NULL != entity) {
      printf("%c %s\n", inString[i], entity);
      nsMemory::Free(entity);
    }
  }

  // convert at once as a string
  PRUnichar *entities;
  res = entityConv->ConvertToEntities(inString.get(), version, &entities);
  if (NS_SUCCEEDED(res) && NULL != entities) {
    for (i = 0; i < nsCRT::strlen(entities); i++) {
      printf("%c", (char) entities[i]);
      if (';' == (char) entities[i])
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
  inString.Assign(NS_ConvertASCIItoUCS2("\x61\x62\x80\xA0\x63"));
  char *outString;
  
  // first, dump input string
  for (PRUint32 i = 0; i < inString.Length(); i++) {
    printf("%c ", inString[i]);
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
   res = nsServiceManager::GetService(kUnicodeNormalizerCID,
                                      NS_GET_IID(nsIUnicodeNormalizer),
                                      (nsISupports**) &t);
           
   printf("Test 1 - GetService():\n");
   if(NS_FAILED(res) || ( t == NULL ) ) {
     printf("\t1st Norm GetService failed\n");
   } else {
     res = nsServiceManager::ReleaseService(kUnicodeNormalizerCID, t);
   }

   res = nsServiceManager::GetService(kUnicodeNormalizerCID,
                                NS_GET_IID(nsIUnicodeNormalizer),
                                (nsISupports**) &t);
           
   if(NS_FAILED(res) || ( t == NULL ) ) {
     printf("\t2nd GetService failed\n");
   } else {
    printf("Test 2 - NormalizeUnicode(PRUint32, const nsAString&, nsAString&):\n");
    nsAutoString resultStr;
    res =  t->NormalizeUnicodeNFD(nsDependentString(normStr), resultStr);
    if (resultStr.Equals(nfdForm)) {
      printf(" Succeeded in NFD UnicodeNormalizer test. \n");
    } else {
      printf(" Failed in NFD UnicodeNormalizer test. \n");
    }


    res = nsServiceManager::ReleaseService(kUnicodeNormalizerCID, t);
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

   TestEntityConversion(nsIEntityConverter::html40);

   // --------------------------------------------

   TestSaveAsCharset();

   // --------------------------------------------

   TestNormalization();

   // --------------------------------------------
   printf("Finish All The Test Cases\n");

   return 0;
}
