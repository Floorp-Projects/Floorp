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
#include <stdlib.h>
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsILocale.h"
#include "nsILocaleFactory.h"
#include "nsLocaleCID.h"
#if defined(XP_MAC) || defined(XP_MACOSX)
#include "nsIMacLocale.h"
#include <TextEncodingConverter.h>
#elif defined(XP_WIN) || defined(XP_OS2)
#include "nsIWin32Locale.h"
#else
#include "nsIPosixLocale.h"
#endif
#include "nsCollationCID.h"
#include "nsICollation.h"
#include "nsDateTimeFormatCID.h"
#include "nsIDateTimeFormat.h"
#include "nsReadableUtils.h"

#ifdef XP_MAC
#define LOCALE_DLL_NAME "NSLOCALE_DLL"
#define UCONV_DLL       "UCONV_DLL"
#define UCVLATIN_DLL    "UCVLATIN_DLL"
#define UNICHARUTIL_DLL_NAME "UNICHARUTIL_DLL"
#elif defined(XP_WIN) || defined(XP_OS2)
#define LOCALE_DLL_NAME "NSLOCALE.DLL"
#define UCONV_DLL       "uconv.dll"
#define UCVLATIN_DLL    "ucvlatin.dll"
#define UNICHARUTIL_DLL_NAME "unicharutil.dll"
#else
#define LOCALE_DLL_NAME "libnslocale"MOZ_DLL_SUFFIX
#define UCONV_DLL       "libuconv"MOZ_DLL_SUFFIX
#define UCVLATIN_DLL    "libucvlatin"MOZ_DLL_SUFFIX
#define UNICHARUTIL_DLL_NAME "libunicharutil"MOZ_DLL_SUFFIX
#endif



// Collation
//
static NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
// Date and Time
//
static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);
// locale
//
static NS_DEFINE_CID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);
// platform specific
//
#if defined(XP_MAC) || defined(XP_MACOSX)
static NS_DEFINE_IID(kMacLocaleFactoryCID, NS_MACLOCALEFACTORY_CID);
static NS_DEFINE_IID(kIMacLocaleIID, NS_MACLOCALE_CID);
#elif defined(XP_WIN) || defined(XP_OS2)
static NS_DEFINE_CID(kWin32LocaleFactoryCID, NS_WIN32LOCALEFACTORY_CID);
static NS_DEFINE_IID(kIWin32LocaleIID, NS_IWIN32LOCALE_IID);
#else
static NS_DEFINE_IID(kPosixLocaleFactoryCID, NS_POSIXLOCALEFACTORY_CID);
static NS_DEFINE_IID(kIPosixLocaleIID, NS_IPOSIXLOCALE_IID);
#endif

// global variable
static PRBool g_verbose = PR_FALSE;
static FILE *g_outfp = NULL;
static char g_usage[] = " usage:\n-date\tdate time format test\n-col\tcollation test\n-sort\tsort test\n\
-f file\tsort data file\n-o out file\tsort result file\n-case\tcase sensitive sort\n-locale\tlocale\n-v\tverbose\n";

// Create a collation key, the memory is allocated using new which need to be deleted by a caller.
//
static nsresult CreateCollationKey(nsICollation *t, PRInt32 strength, 
                                   nsString& stringIn, PRUint8 **aKey, PRUint32 *keyLength)
{
  nsresult res;
  
  // create a raw collation key
  res = t->GetSortKeyLen(strength, stringIn, keyLength);
  if(NS_FAILED(res)) {
    printf("\tFailed!! return value != NS_OK\n");
  }
  *aKey = (PRUint8 *) new PRUint8[*keyLength];
  if (NULL == *aKey) {
    printf("\tFailed!! memory allocation failed.\n");
  }
  res = t->CreateRawSortKey(strength, stringIn, *aKey, keyLength);
  if(NS_FAILED(res)) {
    printf("\tFailed!! return value != NS_OK\n");
  }

  // create a key in nsString
  nsString aKeyString;
  res = t->CreateSortKey(strength, stringIn, aKeyString);
  if(NS_FAILED(res)) {
    printf("\tFailed!! return value != NS_OK\n");
  }

  // compare the generated key
  nsString tempString;
  tempString.SetString((PRUnichar *) *aKey, *keyLength / sizeof(PRUnichar));
  NS_ASSERTION(aKeyString == tempString, "created key mismatch");

  return res;
}

static void DebugDump(nsString& aString) {
#ifdef WIN32
  char s[512];
  int len = WideCharToMultiByte(GetACP(), 0,
                                (LPCWSTR ) aString.get(),  aString.Length(),
                                s, 512, NULL,  NULL);
  s[len] = '\0';
  fflush(stdout);
  printf("%s\n", s);
#elif defined(XP_MAC) || defined(XP_MACOSX)
	// Use TEC (Text Encoding Conversion Manager)
  TextEncoding outEncoding;
  TECObjectRef anEncodingConverter;
  ByteCount oInputRead, oOutputLen;
  char oOutputStr[512];
  OSStatus err = 0;
  err = UpgradeScriptInfoToTextEncoding	(smSystemScript/*smCurrentScript*/, kTextLanguageDontCare, kTextRegionDontCare, NULL, &outEncoding);
  err = TECCreateConverter(&anEncodingConverter, kTextEncodingUnicodeV2_0, outEncoding);
  err = TECConvertText(anEncodingConverter,
                       (ConstTextPtr) aString.get(), aString.Length() * sizeof(PRUnichar), &oInputRead,
                       (TextPtr) oOutputStr, 512, &oOutputLen);
  err = TECDisposeConverter(anEncodingConverter);
  
  oOutputStr[oOutputLen] = '\0';
  fflush(NULL);
  printf("%s\n", oOutputStr);
#else
  for (int i = 0; i < aString.Length(); i++) {
    putchar((char) aString[i]);
  }
  printf("\n");
#endif
}

// Test all functions in nsICollation.
//
static void TestCollation(nsILocale *locale)
{
//   nsString locale("en-US");
   nsICollationFactory *f = NULL;
   nsICollation *t = NULL;
   nsresult res;

   printf("==============================\n");
   printf("Start nsICollation Test \n");
   printf("==============================\n");
   
   res = CallCreateInstance(kCollationFactoryCID, &f);
           
   printf("Test 1 - CreateInstance():\n");
   if(NS_FAILED(res) || ( f == NULL ) ) {
     printf("\t1st CreateInstance failed\n");
   } else {
     f->Release();
   }

   res = CallCreateInstance(kCollationFactoryCID, &f);
   if(NS_FAILED(res) || ( f == NULL ) ) {
     printf("\t2nd CreateInstance failed\n");
   }

   res = f->CreateCollation(locale, &t);
           
   if(NS_FAILED(res) || ( t == NULL ) ) {
     printf("\tCreateCollation failed\n");
   } else {
     nsString string1("abcde");
     nsString string2("ABCDE");
     nsString string3("xyz");
     nsString string4("abc");
     PRUint32 keyLength1, keyLength2, keyLength3;
     PRUint8 *aKey1, *aKey2, *aKey3;
     PRUint32 i;
     PRInt32 result;

      printf("String data used:\n");
      printf("string1: ");
      DebugDump(string1);
      printf("string2: ");
      DebugDump(string2);
      printf("string3: ");
      DebugDump(string3);
      printf("string4: ");
      DebugDump(string4);

      printf("Test 2 - CompareString():\n");
      res = t->CompareString(nsICollation::kCollationCaseInSensitive, string1, string2, &result);
      if(NS_FAILED(res)) {
        printf("\tFailed!! return value != NS_OK\n");
      }
      printf("case insensitive comparison (string1 vs string2): %d\n", result);

      res = t->CompareString(nsICollation::kCollationCaseSensitive, string1, string2, &result);
      if(NS_FAILED(res)) {
        printf("\tFailed!! return value != NS_OK\n");
      }
      printf("case sensitive comparison (string1 vs string2): %d\n", result);

      printf("Test 3 - GetSortKeyLen():\n");
      res = t->GetSortKeyLen(nsICollation::kCollationCaseSensitive, string2, &keyLength1);
      if(NS_FAILED(res)) {
        printf("\tFailed!! return value != NS_OK\n");
      }
      printf("keyLength: %d\n", keyLength1);

      printf("Test 4 - CreateRawSortKey():\n");
      aKey1 = (PRUint8 *) new PRUint8[keyLength1];
      if (NULL == aKey1) {
        printf("\tFailed!! memory allocation failed.\n");
      }
      res = t->CreateRawSortKey(nsICollation::kCollationCaseSensitive, string2, aKey1, &keyLength1);
      if(NS_FAILED(res)) {
        printf("\tFailed!! return value != NS_OK\n");
      }
      printf("case sensitive key creation:\n");
      printf("keyLength: %d\n", keyLength1);
      DebugDump(string2);

      fflush(NULL);
      for (i = 0; i < keyLength1; i++) {
        printf("%.2x ", aKey1[i]);
        //printf("key[" << i << "]: " << aKey1[i] << " ");
      }
      printf("\n");

      res = CreateCollationKey(t, nsICollation::kCollationCaseInSensitive, string2, &aKey2, &keyLength2);
      if(NS_FAILED(res)) {
        printf("\tFailed!! return value != NS_OK\n");
      }
      printf("case insensitive key creation:\n");
      printf("keyLength: %d\n", keyLength2);
      DebugDump(string2);

      fflush(NULL);
      for (i = 0; i < keyLength2; i++) {
       printf("%.2x ", aKey2[i]);
       //printf("key[" << i << "]: " << aKey2[i] << " ");
      }
      printf("\n");

      printf("Test 5 - CompareRawSortKey():\n");
      res = CreateCollationKey(t, nsICollation::kCollationCaseSensitive, string1, &aKey3, &keyLength3);
      if(NS_FAILED(res)) {
        printf("\tFailed!! return value != NS_OK\n");
      }

      res = t->CompareRawSortKey(aKey1, keyLength1, aKey3, keyLength3, &result);
      if(NS_FAILED(res)) {
        printf("\tFailed!! return value != NS_OK\n");
      }
      printf("case sensitive comparison (string1 vs string2): %d\n", result);
      res = t->CompareRawSortKey(aKey3, keyLength3, aKey1, keyLength1, &result);
      if(NS_FAILED(res)) {
        printf("\tFailed!! return value != NS_OK\n");
      }
      printf("case sensitive comparison (string2 vs string1): %d\n", result);

      res = t->CompareRawSortKey(aKey2, keyLength2, aKey3, keyLength3, &result);
      if(NS_FAILED(res)) {
        printf("\tFailed!! return value != NS_OK\n");
      }
      printf("case insensitive comparison (string1 vs string2): %d\n", result);

      if (NULL != aKey1)
        delete[] aKey1; 
      if (NULL != aKey2)
        delete[] aKey2; 
      if (NULL != aKey3)
        delete[] aKey3; 

      res = CreateCollationKey(t, nsICollation::kCollationCaseSensitive, string1, &aKey1, &keyLength1);
      if(NS_FAILED(res)) {
        printf("\tFailed!! return value != NS_OK\n");
      }
      res = CreateCollationKey(t, nsICollation::kCollationCaseSensitive, string3, &aKey2, &keyLength2);
      if(NS_FAILED(res)) {
        printf("\tFailed!! return value != NS_OK\n");
      }
      res = CreateCollationKey(t, nsICollation::kCollationCaseSensitive, string4, &aKey3, &keyLength3);
      if(NS_FAILED(res)) {
        printf("\tFailed!! return value != NS_OK\n");
      }
      res = t->CompareRawSortKey(aKey1, keyLength1, aKey2, keyLength2, &result);
      if(NS_FAILED(res)) {
        printf("\tFailed!! return value != NS_OK\n");
      }
      printf("case sensitive comparison (string1 vs string3): %d\n", result);
      res = t->CompareRawSortKey(aKey1, keyLength1, aKey3, keyLength3, &result);
      if(NS_FAILED(res)) {
        printf("\tFailed!! return value != NS_OK\n");
      }
      printf("case sensitive comparison (string1 vs string4): %d\n", result);

      if (NULL != aKey1)
        delete[] aKey1; 
      if (NULL != aKey2)
        delete[] aKey2; 
      if (NULL != aKey3)
        delete[] aKey3; 

     res = t->Release();

     res = f->Release();
   }
   printf("==============================\n");
   printf("Finish nsICollation Test \n");
   printf("==============================\n");

}

// Sort test using qsort
//

static nsICollation *g_collationInst = NULL;
static PRInt32 g_CollationStrength= nsICollation::kCollationCaseInSensitive;


static void TestSortPrint1(nsString *string_array, int len)
{
  for (int i = 0; i < len; i++) {
    //string_array[i].DebugDump(cout);
    DebugDump(string_array[i]);
  }
  printf("\n");
}

static void TestSortPrintToFile(nsString *string_array, int len)
{
  for (int i = 0; i < len; i++) {
#ifdef WIN32
    char cstr[512];
    int len = WideCharToMultiByte(GetACP(), 0,
                                  (LPCWSTR ) string_array[i].get(),  string_array[i].Length(),
                                  cstr, 512, NULL,  NULL);
    cstr[len] = '\0';
    fprintf(g_outfp, "%s\n", cstr);
#else
    fprintf(g_outfp, "%s\n",
            NS_LossyConvertUCS2ToASCII(string_array[i]).get());
#endif
  }
  fprintf(g_outfp, "\n");
}

static void DebugPrintCompResult(const nsString& string1, const nsString& string2, int result)
{
#ifdef WIN32
  char s[512];
  int len = WideCharToMultiByte(GetACP(), 0,
                                (LPCWSTR ) string1.get(),  string1.Length(),
                                s, 512, NULL,  NULL);
  s[len] = '\0';
  printf("%s ", s);

    switch ((int)result) {
    case 0:
      printf("==");
      break;
    case 1:
      printf(">");
      break;
    case -1:
      printf("<");
      break;
    }

  len = WideCharToMultiByte(GetACP(), 0,
                            (LPCWSTR ) string2.get(),  string2.Length(),
                            s, 512, NULL,  NULL);
  s[len] = '\0';
  printf(" %s\n", s);
#else
    // Warning: casting to char*
    printf(NS_LossyConvertUCS2toASCII(string1).get() << ' ');

    switch ((int)result) {
    case 0:
      printf("==");
      break;
    case 1:
      printf('>');
      break;
    case -1:
      printf('<');
      break;
    }
    printf(' ' << NS_LossyConvertUCS2toASCII(string2).get() << '\n');
#endif
}

static int compare1( const void *arg1, const void *arg2 )
{
  nsString string1, string2;
  PRInt32 result,result2;
  nsresult res;

  string1 = *(nsString *) arg1;
  string2 = *(nsString *) arg2;

  res = g_collationInst->CompareString(g_CollationStrength, string1, string2, &result2);

  nsString key1,key2;
  res = g_collationInst->CreateSortKey(g_CollationStrength, string1, key1);
  NS_ASSERTION(NS_SUCCEEDED(res), "CreateSortKey");
  res = g_collationInst->CreateSortKey(g_CollationStrength, string2, key2);
  NS_ASSERTION(NS_SUCCEEDED(res), "CreateSortKey");

  // dump collation keys
  if (g_verbose) {
    int i;
    for (i = 0; i < key1.Length(); i++) 
      printf("%.2x", key1[i]);
    printf(" ");
    for (i = 0; i < key2.Length(); i++) 
      printf("%.2x", key2[i]);
    printf("\n");
  }

  res = g_collationInst->CompareSortKey(key1, key2, &result);
  NS_ASSERTION(NS_SUCCEEDED(res), "CreateSortKey");
  NS_ASSERTION(NS_SUCCEEDED((PRBool)(result == result2)), "result unmatch");
  if (g_verbose) {
    DebugPrintCompResult(string1, string2, result);
  }

  return (int) result;
}


typedef struct {
  PRUint8   *aKey;
  PRUint32  aLength;
} collation_rec;

static int compare2( const void *arg1, const void *arg2 )
{
  collation_rec *keyrec1, *keyrec2;
  PRInt32 result;
  nsresult res;

  keyrec1 = (collation_rec *) arg1;
  keyrec2 = (collation_rec *) arg2;

  res = g_collationInst->CompareRawSortKey(keyrec1->aKey, keyrec1->aLength, 
                                        keyrec2->aKey, keyrec2->aLength, &result);

  return (int) result;
}

static void TestSortPrint2(collation_rec *key_array, int len)
{
  PRUint32 aLength;
  PRUint8 *aKey;

  fflush(NULL);
  for (int i = 0; i < len; i++) {
    aLength = key_array[i].aLength;
    aKey = key_array[i].aKey;
    for (int j = 0; j < (int)aLength; j++) {
      printf("%.2x ", aKey[j]);
    }
    printf("\n");
  }
  printf("\n");
}

// Sort test by reading data from a file.
//
static void SortTestFile(nsICollation* collationInst, FILE* fp)
{
  nsString string_array[256]; 
  char buf[512];
  int i = 0;

  // read lines and put them to nsStrings
  while (fgets(buf, 512, fp)) {
    if (*buf == '\n' || *buf == '\r')
      continue;
    // trim LF CR
    char *cp = buf + (strlen(buf) - 1);
    while (*cp) {
      if (*cp == '\n' || *cp == '\r')
        *cp = '\0';
      else
        break;
      cp--;
    }
#ifdef WIN32
    wchar_t wcs[512];
    int len = MultiByteToWideChar(GetACP(), 0, buf, strlen(buf), wcs, 512);
    wcs[len] = L'\0';
    string_array[i].SetString((PRUnichar *)wcs);
#else
    string_array[i].SetString(buf);
#endif
    i++;
  }  
  printf("print string before sort\n");
  TestSortPrint1(string_array, i);

  g_collationInst = collationInst;
  qsort( (void *)string_array, i, sizeof(nsString), compare1 );

  printf("print string after sort\n");
  (g_outfp == NULL) ? TestSortPrint1(string_array, i) : TestSortPrintToFile(string_array, i);
}

// Use nsICollation for qsort.
//
static void TestSort(nsILocale *locale, PRInt32 collationStrength, FILE *fp)
{
  nsresult res;
  nsICollationFactory *factoryInst;
  nsICollation *collationInst;
  PRInt32 strength;
  collation_rec key_array[5];
  PRUint8 *aKey;
  PRUint32 aLength;
  //nsString locale("en-US");
  nsString string1("AAC");
  nsString string2("aac");
  nsString string3("AAC");
  nsString string4("aac");
  nsString string5("AAC");
  nsString string_array[5];

  printf("==============================\n");
  printf("Start sort Test \n");
  printf("==============================\n");

  res = CallCreateInstance(kCollationFactoryCID, &factoryInst);
  if(NS_FAILED(res)) {
    printf("\tFailed!! return value != NS_OK\n");
  }

  res = factoryInst->CreateCollation(locale, &collationInst);
  if(NS_FAILED(res)) {
    printf("\tFailed!! return value != NS_OK\n");
  }

  // set collation strength
  g_CollationStrength = collationStrength;
  strength = g_CollationStrength;

  if (fp != NULL) {
    res = NS_ADDREF(collationInst);
    SortTestFile(collationInst, fp);
    NS_RELEASE(collationInst);
    return;
  }

  printf("==============================\n");
  printf("Sort Test by comparestring.\n");
  printf("==============================\n");
  string_array[0] = string1;
  string_array[1] = string2;
  string_array[2] = string3;
  string_array[3] = string4;
  string_array[4] = string5;

  printf("print string before sort\n");
  TestSortPrint1(string_array, 5);

  g_collationInst = collationInst;
  res = collationInst->AddRef();
  qsort( (void *)string_array, 5, sizeof(nsString), compare1 );
  res = collationInst->Release();

  printf("print string after sort\n");
  TestSortPrint1(string_array, 5);

  printf("==============================\n");
  printf("Sort Test by collation key.\n");
  printf("==============================\n");


  res = CreateCollationKey(collationInst, strength, string1, &aKey, &aLength);
  if(NS_FAILED(res)) {
    printf("\tFailed!! return value != NS_OK\n");
  }
  key_array[0].aKey = aKey;
  key_array[0].aLength = aLength;

  res = CreateCollationKey(collationInst, strength, string2, &aKey, &aLength);
  if(NS_FAILED(res)) {
    printf("\tFailed!! return value != NS_OK\n");
  }
  key_array[1].aKey = aKey;
  key_array[1].aLength = aLength;

  res = CreateCollationKey(collationInst, strength, string3, &aKey, &aLength);
  if(NS_FAILED(res)) {
    printf("\tFailed!! return value != NS_OK\n");
  }
  key_array[2].aKey = aKey;
  key_array[2].aLength = aLength;

  res = CreateCollationKey(collationInst, strength, string4, &aKey, &aLength);
  if(NS_FAILED(res)) {
    printf("\tFailed!! return value != NS_OK\n");
  }
  key_array[3].aKey = aKey;
  key_array[3].aLength = aLength;

  res = CreateCollationKey(collationInst, strength, string5, &aKey, &aLength);
  if(NS_FAILED(res)) {
    printf("\tFailed!! return value != NS_OK\n");
  }
  key_array[4].aKey = aKey;
  key_array[4].aLength = aLength;

  printf("print key before sort\n");
  TestSortPrint2(key_array, 5);

  g_collationInst = collationInst;
  res = collationInst->AddRef();
  qsort( (void *)key_array, 5, sizeof(collation_rec), compare2 );
  res = collationInst->Release();

  printf("print key after sort\n");
  TestSortPrint2(key_array, 5);


  res = collationInst->Release();

  res = factoryInst->Release();

  for (int i = 0; i < 5; i++) {
    delete [] key_array[i].aKey;
  }

  printf("==============================\n");
  printf("Finish sort Test \n");
  printf("==============================\n");
}

// Test all functions in nsIDateTimeFormat.
//
static void TestDateTimeFormat(nsILocale *locale)
{
  nsresult res;

  printf("==============================\n");
  printf("Start nsIScriptableDateFormat Test \n");
  printf("==============================\n");

  nsIScriptableDateFormat *aScriptableDateFormat;
  res = CallCreateInstance(kDateTimeFormatCID, &aScriptableDateFormat);
  if(NS_FAILED(res) || ( aScriptableDateFormat == NULL ) ) {
    printf("\tnsIScriptableDateFormat CreateInstance failed\n");
  }

  PRUnichar *aUnichar;
  nsString aString;
  PRUnichar aLocaleUnichar[1];
  *aLocaleUnichar = 0;
  res = aScriptableDateFormat->FormatDateTime(aLocaleUnichar, kDateFormatShort, kTimeFormatSeconds,
                        1999, 
                        7, 
                        31, 
                        8, 
                        21, 
                        58,
                        &aUnichar);
  aString.SetString(aUnichar);
  DebugDump(aString);

  res = aScriptableDateFormat->FormatDate(aLocaleUnichar, kDateFormatLong,
                        1970, 
                        4, 
                        20, 
                        &aUnichar);
  aString.SetString(aUnichar);
  DebugDump(aString);

  res = aScriptableDateFormat->FormatTime(aLocaleUnichar, kTimeFormatSecondsForce24Hour,
                        13, 
                        59, 
                        31,
                        &aUnichar);
  aString.SetString(aUnichar);
  DebugDump(aString);

  aScriptableDateFormat->Release();

  printf("==============================\n");
  printf("Start nsIDateTimeFormat Test \n");
  printf("==============================\n");

  nsIDateTimeFormat *t = NULL;
  res = CallCreateInstance(kDateTimeFormatCID, &t);
       
  printf("Test 1 - CreateInstance():\n");
  if(NS_FAILED(res) || ( t == NULL ) ) {
    printf("\t1st CreateInstance failed\n");
  } else {
    t->Release();
  }

  res = CallCreateInstance(kDateTimeFormatCID, &t);
       
  if(NS_FAILED(res) || ( t == NULL ) ) {
    printf("\t2nd CreateInstance failed\n");
  } else {
  }


  nsAutoString dateString;
//  nsString locale("en-GB");
  time_t  timetTime;


  printf("Test 2 - FormatTime():\n");
  time( &timetTime );
  res = t->FormatTime(locale, kDateFormatShort, kTimeFormatSeconds, timetTime, dateString);
  DebugDump(dateString);

  printf("Test 3 - FormatTMTime():\n");
  time_t ltime;
  time( &ltime );

  // try (almost) all format combination
  res = t->FormatTMTime(locale, kDateFormatNone, kTimeFormatNone, localtime( &ltime ), dateString);
  printf("kDateFormatNone, kTimeFormatNone:\n");
  DebugDump(dateString);
  res = t->FormatTMTime(locale, kDateFormatNone, kTimeFormatSeconds, localtime( &ltime ), dateString);
  printf("kDateFormatNone, kTimeFormatSeconds:\n");
  DebugDump(dateString);
  res = t->FormatTMTime(locale, kDateFormatNone, kTimeFormatNoSeconds, localtime( &ltime ), dateString);
  printf("kDateFormatNone, kTimeFormatNoSeconds:\n");
  DebugDump(dateString);
  res = t->FormatTMTime(locale, kDateFormatLong, kTimeFormatNone, localtime( &ltime ), dateString);
  printf("kDateFormatLong, kTimeFormatNone:\n");
  DebugDump(dateString);
  res = t->FormatTMTime(locale, kDateFormatLong, kTimeFormatSeconds, localtime( &ltime ), dateString);
  printf("kDateFormatLong, kTimeFormatSeconds:\n");
  DebugDump(dateString);
  res = t->FormatTMTime(locale, kDateFormatLong, kTimeFormatNoSeconds, localtime( &ltime ), dateString);
  printf("kDateFormatLong, kTimeFormatNoSeconds:\n");
  DebugDump(dateString);
  res = t->FormatTMTime(locale, kDateFormatShort, kTimeFormatNone, localtime( &ltime ), dateString);
  printf("kDateFormatShort, kTimeFormatNone:\n");
  DebugDump(dateString);
  res = t->FormatTMTime(locale, kDateFormatShort, kTimeFormatSeconds, localtime( &ltime ), dateString);
  printf("kDateFormatShort, kTimeFormatSeconds:\n");
  DebugDump(dateString);
  res = t->FormatTMTime(locale, kDateFormatShort, kTimeFormatNoSeconds, localtime( &ltime ), dateString);
  printf("kDateFormatShort, kTimeFormatNoSeconds:\n");
  DebugDump(dateString);
  res = t->FormatTMTime(locale, kDateFormatYearMonth, kTimeFormatNone, localtime( &ltime ), dateString);
  printf("kDateFormatYearMonth, kTimeFormatNone:\n");
  DebugDump(dateString);
  res = t->FormatTMTime(locale, kDateFormatYearMonth, kTimeFormatSeconds, localtime( &ltime ), dateString);
  printf("kDateFormatYearMonth, kTimeFormatSeconds:\n");
  DebugDump(dateString);
  res = t->FormatTMTime(locale, kDateFormatYearMonth, kTimeFormatNoSeconds, localtime( &ltime ), dateString);
  printf("kDateFormatYearMonth, kTimeFormatNoSeconds:\n");
  DebugDump(dateString);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatNone, localtime( &ltime ), dateString);
  printf("kDateFormatWeekday, kTimeFormatNone:\n");
  DebugDump(dateString);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatSeconds, localtime( &ltime ), dateString);
  printf("kDateFormatWeekday, kTimeFormatSeconds:\n");
  DebugDump(dateString);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatNoSeconds, localtime( &ltime ), dateString);
  printf("kDateFormatWeekday, kTimeFormatNoSeconds:\n");
  DebugDump(dateString);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatSecondsForce24Hour, localtime( &ltime ), dateString);
  printf("kDateFormatWeekday, kTimeFormatSecondsForce24Hour:\n");
  DebugDump(dateString);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatNoSecondsForce24Hour, localtime( &ltime ), dateString);
  printf("kDateFormatWeekday, kTimeFormatNoSecondsForce24Hour:\n");
  DebugDump(dateString);

  res = t->Release();
  
  printf("==============================\n");
  printf("Finish nsIDateTimeFormat Test \n");
  printf("==============================\n");
}

static nsresult NewLocale(const nsString* localeName, nsILocale** locale)
{
  nsresult res;

  nsCOMPtr<nsILocaleFactory> localeFactory = do_GetClassObject(kLocaleFactoryCID, &res);
  if (NS_FAILED(res) || localeFactory == nsnull) printf("FindFactory nsILocaleFactory failed\n");

  res = localeFactory->NewLocale(localeName, locale);
  if (NS_FAILED(res) || locale == nsnull) printf("NewLocale failed\n");

  return res;
}

static void Test_nsLocale()
{
#if defined(XP_MAC) || defined(XP_MACOSX)
  nsAutoString localeName;
  nsCOMPtr<nsIMacLocale> macLocale;
  short script, lang;
  nsresult res;

  nsCOMPtr<nsIMacLocale> win32Locale = do_CreateInstance(kMacLocaleFactoryCID);
  if (macLocale) {
    localeName.AssignLiteral("en-US");
    res = macLocale->GetPlatformLocale(localeName, &script, &lang);
    printf("script for en-US is 0\n");
    printf("result: script = %d lang = %d\n", script, lang);

    localeName.AssignLiteral("en-GB");
    res = macLocale->GetPlatformLocale(localeName, &script, &lang);
    printf("script for en-GB is 0\n");
    printf("result: script = %d lang = %d\n", script, lang);

    localeName.AssignLiteral("fr-FR");
    res = macLocale->GetPlatformLocale(localeName, &script, &lang);
    printf("script for fr-FR is 0\n");
    printf("result: script = %d lang = %d\n", script, lang);

    localeName.AssignLiteral("de-DE");
    res = macLocale->GetPlatformLocale(localeName, &script, &lang);
    printf("script for de-DE is 0\n");
    printf("result: script = %d lang = %d\n", script, lang);

    localeName.AssignLiteral("ja-JP");
    res = macLocale->GetPlatformLocale(localeName, &script, &lang);
    printf("script for ja-JP is 1\n");
    printf("result: script = %d lang = %d\n", script, lang);
  }
#elif defined(XP_WIN) || defined(XP_OS2)
  nsAutoString localeName;
  LCID lcid;
  nsresult res;

  nsCOMPtr<nsIWin32Locale> win32Locale = do_CreateInstance(kWin32LocaleFactoryCID);
  if (win32Locale) {
    localeName.AssignLiteral("en-US");
    res = win32Locale->GetPlatformLocale(localeName, &lcid);
    printf("LCID for en-US is 1033\n");
    printf("result: locale = %s LCID = 0x%0.4x %d\n", NS_LossyConvertUCS2toASCII(localeName).get(), lcid, lcid);

    localeName.AssignLiteral("en-GB");
    res = win32Locale->GetPlatformLocale(localeName, &lcid);
    printf("LCID for en-GB is 2057\n");
    printf("result: locale = %s LCID = 0x%0.4x %d\n", NS_LossyConvertUCS2toASCII(localeName).get(), lcid, lcid);

    localeName.AssignLiteral("fr-FR");
    res = win32Locale->GetPlatformLocale(localeName, &lcid);
    printf("LCID for fr-FR is 1036\n");
    printf("result: locale = %s LCID = 0x%0.4x %d\n", NS_LossyConvertUCS2toASCII(localeName).get(), lcid, lcid);

    localeName.AssignLiteral("de-DE");
    res = win32Locale->GetPlatformLocale(localeName, &lcid);
    printf("LCID for de-DE is 1031\n");
    printf("result: locale = %s LCID = 0x%0.4x %d\n", NS_LossyConvertUCS2toASCII(localeName).get(), lcid, lcid);

    localeName.AssignLiteral("ja-JP");
    res = win32Locale->GetPlatformLocale(localeName, &lcid);
    printf("LCID for ja-JP is 1041\n");
    printf("result: locale = %s LCID = 0x%0.4x %d\n", NS_LossyConvertUCS2toASCII(localeName).get(), lcid, lcid);

  }
#else
  nsAutoString localeName;
  char locale[32];
  size_t length = 32;
  nsresult res;

  nsCOMPtr<nsIPosixLocale> posixLocale = do_CreateInstance(kPosixLocaleFactoryCID);
  if (posixLocale) {
    localeName.AssignLiteral("en-US");
    res = posixLocale->GetPlatformLocale(localeName, locale, length);
    printf("result: locale = %s POSIX = %s\n", NS_LossyConvertUCS2toASCII(localeName).get(), locale);

    localeName.AssignLiteral("en-GB");
    res = posixLocale->GetPlatformLocale(localeName, locale, length);
    printf("result: locale = %s POSIX = %s\n", NS_LossyConvertUCS2toASCII(localeName).get(), locale);

    localeName.AssignLiteral("fr-FR");
    res = posixLocale->GetPlatformLocale(localeName, locale, length);
    printf("result: locale = %s POSIX = %s\n", NS_LossyConvertUCS2toASCII(localeName).get(), locale);

    localeName.AssignLiteral("de-DE");
    res = posixLocale->GetPlatformLocale(localeName, locale, length);
    printf("result: locale = %s POSIX = %s\n", NS_LossyConvertUCS2toASCII(localeName).get(), locale);

    localeName.AssignLiteral("ja-JP");
    res = posixLocale->GetPlatformLocale(localeName, locale, length);
    printf("result: locale = %s POSIX = %s\n", NS_LossyConvertUCS2toASCII(localeName).get(), locale);

  }
  else {
    printf("Fail: CreateInstance PosixLocale\n");
  }
#endif
}

static char* get_option(int argc, char** argv, char* arg)
{
  for (int i = 0; i < argc; i++) {
    if (!strcmp(argv[i], arg)) {
      NS_ASSERTION((i <= argc && argv[i+1]), "option not specified");
      return argv[i+1];
    }
  }
  return NULL;
}
static char* find_option(int argc, char** argv, char* arg)
{
  for (int i = 0; i < argc; i++) {
    if (!strcmp(argv[i], arg)) {
      return argv[i];
    }
  }
  return NULL;
}

#ifdef XP_MAC
#define kMaxArgs 16
static char g_arg_array[kMaxArgs][256];
static int make_args(char **argv)
{
  int argc = 0;
  char *token;
  char s[256];
  char seps[] = " ,\t";
	
  strcpy(g_arg_array[argc], "LocaleSelfTest");
  argc++;

  printf("%s\ntype option(s) then hit return\n", g_usage);
  if (gets(s)) {
    token = strtok(s, seps);
    while (token != NULL && argc < kMaxArgs) {
      strcpy(g_arg_array[argc], token);
      argv[argc] = g_arg_array[argc];
      argc++;
      token = strtok(NULL, seps);
    }
	  printf("\noptions specified: ");

    for (int i = 0; i < argc; i++) {
      printf("%s ", argv[i]);
    }
    printf("\n");
  }
  
  return argc;
}
#endif//XP_MAC

int main(int argc, char** argv) {
#ifdef XP_MAC
  char *mac_argv[kMaxArgs];
  argc = make_args(mac_argv);
  argv = mac_argv;
#endif//XP_MAC
  nsCOMPtr<nsILocale> locale;
  nsresult res; 

	nsCOMPtr<nsILocaleFactory>	localeFactory = do_GetClassObject(kLocaleFactoryCID, &res);
  if (NS_FAILED(res) || localeFactory == nsnull) printf("FindFactory nsILocaleFactory failed\n");

  res = localeFactory->GetApplicationLocale(getter_AddRefs(locale));
  if (NS_FAILED(res) || locale == nsnull) printf("GetApplicationLocale failed\n");
  
  // --------------------------------------------
    PRInt32 strength = nsICollation::kCollationCaseInSensitive;
    FILE *fp = NULL;

  if (argc == 1) {
    TestCollation(locale);
    TestSort(locale, nsICollation::kCollationCaseInSensitive, NULL);
    TestDateTimeFormat(locale);
  }
  else {
    char *s;
    s = find_option(argc, argv, "-h");
    if (s) {
      printf(argv[0] << g_usage);
      return 0;
    }
    s = find_option(argc, argv, "-v");
    if (s) {
      g_verbose = PR_TRUE;
      //test for locale
      Test_nsLocale();
    }
    s = get_option(argc, argv, "-f");
    if (s) {
      fp = fopen(s, "r");
    }
    s = get_option(argc, argv, "-o");
    if (s) {
      g_outfp = fopen(s, "w");
    }
    s = find_option(argc, argv, "-case");
    if (s) {
      strength = nsICollation::kCollationCaseSensitive;
    }
    s = get_option(argc, argv, "-locale");
    if (s) {
      nsString localeName(s);
      res = NewLocale(localeName, getter_AddRefs(locale));  // reset the locale
    }

    // print locale string
    nsAutoString localeStr;
    locale->GetCategory(NS_LITERAL_STRING("NSILOCALE_COLLATE"), localeStr);
    printf("locale setting for collation is ");
    DebugDump(localeStr);
    locale->GetCategory(NS_LITERAL_STRING("NSILOCALE_TIME"), localeStr);
    printf("locale setting for time is ");
    DebugDump(localeStr);

    while (argc--) {
      if (!strcmp(argv[argc], "-col"))
        TestCollation(locale);
      else if (!strcmp(argv[argc], "-sort"))
        TestSort(locale, strength, fp);
      else if (!strcmp(argv[argc], "-date"))
        TestDateTimeFormat(locale);
    }
    if (fp != NULL) {
      fclose(fp);
    }
    if (g_outfp != NULL) {
      fclose(g_outfp);
    }
  }

  // --------------------------------------------

  printf("Finish All The Test Cases\n");

  return 0;
}
