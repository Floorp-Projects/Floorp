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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
 
#include <iostream.h>
#include <stdlib.h>
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsILocale.h"
#include "nsILocaleFactory.h"
#include "nsLocaleCID.h"
#ifdef XP_MAC
#include "nsIMacLocale.h"
#include <TextEncodingConverter.h>
#elif defined(XP_PC)
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
#elif defined(XP_PC)
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
static NS_DEFINE_CID(kCollationCID, NS_COLLATION_CID);
// Date and Time
//
static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);
// locale
//
static NS_DEFINE_CID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);
// platform specific
//
#ifdef XP_MAC
static NS_DEFINE_IID(kMacLocaleFactoryCID, NS_MACLOCALEFACTORY_CID);
static NS_DEFINE_IID(kIMacLocaleIID, NS_MACLOCALE_CID);
#elif defined(XP_PC)
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
static nsresult CreateCollationKey(nsICollation *t, nsCollationStrength strength, 
                                   nsString& stringIn, PRUint8 **aKey, PRUint32 *keyLength)
{
  nsresult res;
  
  // create a raw collation key
  res = t->GetSortKeyLen(strength, stringIn, keyLength);
  if(NS_FAILED(res)) {
    cout << "\tFailed!! return value != NS_OK\n";
  }
  *aKey = (PRUint8 *) new PRUint8[*keyLength];
  if (NULL == *aKey) {
    cout << "\tFailed!! memory allocation failed.\n";
  }
  res = t->CreateRawSortKey(strength, stringIn, *aKey, keyLength);
  if(NS_FAILED(res)) {
    cout << "\tFailed!! return value != NS_OK\n";
  }

  // create a key in nsString
  nsString aKeyString;
  res = t->CreateSortKey(strength, stringIn, aKeyString);
  if(NS_FAILED(res)) {
    cout << "\tFailed!! return value != NS_OK\n";
  }

  // compare the generated key
  nsString tempString;
  tempString.SetString((PRUnichar *) *aKey, *keyLength / sizeof(PRUnichar));
  NS_ASSERTION(aKeyString == tempString, "created key mismatch");

  return res;
}

static void DebugDump(nsString& aString, ostream& aStream) {
#ifdef WIN32
  char s[512];
  int len = WideCharToMultiByte(GetACP(), 0,
                                (LPCWSTR ) aString.get(),  aString.Length(),
                                s, 512, NULL,  NULL);
  s[len] = '\0';
  aStream.flush();
  printf("%s\n", s);
#elif defined(XP_MAC)
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
  aStream.flush();
  printf("%s\n", oOutputStr);
#else
  for (int i = 0; i < aString.Length(); i++) {
    aStream << (char) aString[i];
  }
  aStream << "\n";
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

   cout << "==============================\n";
   cout << "Start nsICollation Test \n";
   cout << "==============================\n";
   
   res = nsComponentManager::CreateInstance(kCollationFactoryCID,
                                            NULL,
                                            NS_GET_IID(nsICollationFactory),
                                            (void**) &f);
           
   cout << "Test 1 - CreateInstance():\n";
   if(NS_FAILED(res) || ( f == NULL ) ) {
     cout << "\t1st CreateInstance failed\n";
   } else {
     f->Release();
   }

   res = nsComponentManager::CreateInstance(kCollationFactoryCID,
                                            NULL,
                                            NS_GET_IID(nsICollationFactory),
                                            (void**) &f);
   if(NS_FAILED(res) || ( f == NULL ) ) {
     cout << "\t2nd CreateInstance failed\n";
   }

   res = f->CreateCollation(locale, &t);
           
   if(NS_FAILED(res) || ( t == NULL ) ) {
     cout << "\tCreateCollation failed\n";
   } else {
     nsString string1("abcde");
     nsString string2("ABCDE");
     nsString string3("xyz");
     nsString string4("abc");
     PRUint32 keyLength1, keyLength2, keyLength3;
     PRUint8 *aKey1, *aKey2, *aKey3;
     PRUint32 i;
     PRInt32 result;

      cout << "String data used:\n";
      cout << "string1: ";
      DebugDump(string1, cout);
      cout << "string2: ";
      DebugDump(string2, cout);
      cout << "string3: ";
      DebugDump(string3, cout);
      cout << "string4: ";
      DebugDump(string4, cout);

      cout << "Test 2 - CompareString():\n";
      res = t->CompareString(kCollationCaseInSensitive, string1, string2, &result);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }
      cout << "case insensitive comparison (string1 vs string2): " << result << "\n";

      res = t->CompareString(kCollationCaseSensitive, string1, string2, &result);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }
      cout << "case sensitive comparison (string1 vs string2): " << result << "\n";

      cout << "Test 3 - GetSortKeyLen():\n";
      res = t->GetSortKeyLen(kCollationCaseSensitive, string2, &keyLength1);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }
      cout << "keyLength: " << keyLength1 << "\n";

      cout << "Test 4 - CreateRawSortKey():\n";
      aKey1 = (PRUint8 *) new PRUint8[keyLength1];
      if (NULL == aKey1) {
        cout << "\tFailed!! memory allocation failed.\n";
      }
      res = t->CreateRawSortKey(kCollationCaseSensitive, string2, aKey1, &keyLength1);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }
      cout << "case sensitive key creation:\n";
      cout << "keyLength: " << keyLength1 << "\n";
      DebugDump(string2, cout);

      cout.flush();
      for (i = 0; i < keyLength1; i++) {
        printf("%.2x ", aKey1[i]);
        //cout << "key[" << i << "]: " << aKey1[i] << " ";
      }
      cout << "\n";

      res = CreateCollationKey(t, kCollationCaseInSensitive, string2, &aKey2, &keyLength2);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }
      cout << "case insensitive key creation:\n";
      cout << "keyLength: " << keyLength2 << "\n";
      DebugDump(string2, cout);

      cout.flush();
      for (i = 0; i < keyLength2; i++) {
       printf("%.2x ", aKey2[i]);
       //cout << "key[" << i << "]: " << aKey2[i] << " ";
      }
      cout << "\n";

      cout << "Test 5 - CompareRawSortKey():\n";
      res = CreateCollationKey(t, kCollationCaseSensitive, string1, &aKey3, &keyLength3);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }

      res = t->CompareRawSortKey(aKey1, keyLength1, aKey3, keyLength3, &result);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }
      cout << "case sensitive comparison (string1 vs string2): " << result << "\n";
      res = t->CompareRawSortKey(aKey3, keyLength3, aKey1, keyLength1, &result);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }
      cout << "case sensitive comparison (string2 vs string1): " << result << "\n";

      res = t->CompareRawSortKey(aKey2, keyLength2, aKey3, keyLength3, &result);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }
      cout << "case insensitive comparison (string1 vs string2): " << result << "\n";

      if (NULL != aKey1)
        delete[] aKey1; 
      if (NULL != aKey2)
        delete[] aKey2; 
      if (NULL != aKey3)
        delete[] aKey3; 

      res = CreateCollationKey(t, kCollationCaseSensitive, string1, &aKey1, &keyLength1);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }
      res = CreateCollationKey(t, kCollationCaseSensitive, string3, &aKey2, &keyLength2);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }
      res = CreateCollationKey(t, kCollationCaseSensitive, string4, &aKey3, &keyLength3);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }
      res = t->CompareRawSortKey(aKey1, keyLength1, aKey2, keyLength2, &result);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }
      cout << "case sensitive comparison (string1 vs string3): " << result << "\n";
      res = t->CompareRawSortKey(aKey1, keyLength1, aKey3, keyLength3, &result);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }
      cout << "case sensitive comparison (string1 vs string4): " << result << "\n";

      if (NULL != aKey1)
        delete[] aKey1; 
      if (NULL != aKey2)
        delete[] aKey2; 
      if (NULL != aKey3)
        delete[] aKey3; 

     res = t->Release();

     res = f->Release();
   }
   cout << "==============================\n";
   cout << "Finish nsICollation Test \n";
   cout << "==============================\n";

}

// Sort test using qsort
//

static nsICollation *g_collationInst = NULL;
static nsCollationStrength g_CollationStrength= kCollationCaseInSensitive;


static void TestSortPrint1(nsString *string_array, int len)
{
  for (int i = 0; i < len; i++) {
    //string_array[i].DebugDump(cout);
    DebugDump(string_array[i], cout);
  }
  cout << "\n";
}

static void TestSortPrintToFile(nsString *string_array, int len)
{
  char cstr[512];
  for (int i = 0; i < len; i++) {
#ifdef WIN32
  int len = WideCharToMultiByte(GetACP(), 0,
                                (LPCWSTR ) string_array[i].get(),  string_array[i].Length(),
                                cstr, 512, NULL,  NULL);
  cstr[len] = '\0';
#else
    string_array[i].ToCString(cstr, 512);
#endif
    fprintf(g_outfp, "%s\n", cstr);
  }
  fprintf(g_outfp, "\n");
}

static void DebugPrintCompResult(nsString string1, nsString string2, int result)
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
    char *cstr = ToNewCString(string1);
    
    if (cstr) {
      cout << cstr << ' ';
      delete [] cstr;
    }
    switch ((int)result) {
    case 0:
      cout << "==";
      break;
    case 1:
      cout << '>';
      break;
    case -1:
      cout << '<';
      break;
    }
    cstr = ToNewCString(string2);
    if (cstr) {
      cout << ' ' << cstr << '\n';
      delete [] cstr;
    }
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

  cout.flush();
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
  cout << "print string before sort\n";
  TestSortPrint1(string_array, i);

  g_collationInst = collationInst;
  qsort( (void *)string_array, i, sizeof(nsString), compare1 );

  cout << "print string after sort\n";
  (g_outfp == NULL) ? TestSortPrint1(string_array, i) : TestSortPrintToFile(string_array, i);
}

// Use nsICollation for qsort.
//
static void TestSort(nsILocale *locale, nsCollationStrength collationStrength, FILE *fp)
{
  nsresult res;
  nsICollationFactory *factoryInst;
  nsICollation *collationInst;
  nsCollationStrength strength;
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

  cout << "==============================\n";
  cout << "Start sort Test \n";
  cout << "==============================\n";

  res = nsComponentManager::CreateInstance(kCollationFactoryCID,
                                           NULL,
                                           NS_GET_IID(nsICollationFactory),
                                           (void**) &factoryInst);
  if(NS_FAILED(res)) {
    cout << "\tFailed!! return value != NS_OK\n";
  }

  res = factoryInst->CreateCollation(locale, &collationInst);
  if(NS_FAILED(res)) {
    cout << "\tFailed!! return value != NS_OK\n";
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

  cout << "==============================\n";
  cout << "Sort Test by comparestring.\n";
  cout << "==============================\n";
  string_array[0] = string1;
  string_array[1] = string2;
  string_array[2] = string3;
  string_array[3] = string4;
  string_array[4] = string5;

  cout << "print string before sort\n";
  TestSortPrint1(string_array, 5);

  g_collationInst = collationInst;
  res = collationInst->AddRef();
  qsort( (void *)string_array, 5, sizeof(nsString), compare1 );
  res = collationInst->Release();

  cout << "print string after sort\n";
  TestSortPrint1(string_array, 5);

  cout << "==============================\n";
  cout << "Sort Test by collation key.\n";
  cout << "==============================\n";


  res = CreateCollationKey(collationInst, strength, string1, &aKey, &aLength);
  if(NS_FAILED(res)) {
    cout << "\tFailed!! return value != NS_OK\n";
  }
  key_array[0].aKey = aKey;
  key_array[0].aLength = aLength;

  res = CreateCollationKey(collationInst, strength, string2, &aKey, &aLength);
  if(NS_FAILED(res)) {
    cout << "\tFailed!! return value != NS_OK\n";
  }
  key_array[1].aKey = aKey;
  key_array[1].aLength = aLength;

  res = CreateCollationKey(collationInst, strength, string3, &aKey, &aLength);
  if(NS_FAILED(res)) {
    cout << "\tFailed!! return value != NS_OK\n";
  }
  key_array[2].aKey = aKey;
  key_array[2].aLength = aLength;

  res = CreateCollationKey(collationInst, strength, string4, &aKey, &aLength);
  if(NS_FAILED(res)) {
    cout << "\tFailed!! return value != NS_OK\n";
  }
  key_array[3].aKey = aKey;
  key_array[3].aLength = aLength;

  res = CreateCollationKey(collationInst, strength, string5, &aKey, &aLength);
  if(NS_FAILED(res)) {
    cout << "\tFailed!! return value != NS_OK\n";
  }
  key_array[4].aKey = aKey;
  key_array[4].aLength = aLength;

  cout << "print key before sort\n";
  TestSortPrint2(key_array, 5);

  g_collationInst = collationInst;
  res = collationInst->AddRef();
  qsort( (void *)key_array, 5, sizeof(collation_rec), compare2 );
  res = collationInst->Release();

  cout << "print key after sort\n";
  TestSortPrint2(key_array, 5);


  res = collationInst->Release();

  res = factoryInst->Release();

  for (int i = 0; i < 5; i++) {
    delete [] key_array[i].aKey;
  }

  cout << "==============================\n";
  cout << "Finish sort Test \n";
  cout << "==============================\n";
}

// Test all functions in nsIDateTimeFormat.
//
static void TestDateTimeFormat(nsILocale *locale)
{
  nsresult res;

  cout << "==============================\n";
  cout << "Start nsIScriptableDateFormat Test \n";
  cout << "==============================\n";

  nsIScriptableDateFormat *aScriptableDateFormat;
  res = nsComponentManager::CreateInstance(kDateTimeFormatCID,
                                           NULL,
                                           NS_GET_IID(nsIScriptableDateFormat),
                                           (void**) &aScriptableDateFormat);
  if(NS_FAILED(res) || ( aScriptableDateFormat == NULL ) ) {
    cout << "\tnsIScriptableDateFormat CreateInstance failed\n";
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
  DebugDump(aString, cout);

  res = aScriptableDateFormat->FormatDate(aLocaleUnichar, kDateFormatLong,
                        1970, 
                        4, 
                        20, 
                        &aUnichar);
  aString.SetString(aUnichar);
  DebugDump(aString, cout);

  res = aScriptableDateFormat->FormatTime(aLocaleUnichar, kTimeFormatSecondsForce24Hour,
                        13, 
                        59, 
                        31,
                        &aUnichar);
  aString.SetString(aUnichar);
  DebugDump(aString, cout);

  aScriptableDateFormat->Release();

  cout << "==============================\n";
  cout << "Start nsIDateTimeFormat Test \n";
  cout << "==============================\n";

  nsIDateTimeFormat *t = NULL;
  res = nsComponentManager::CreateInstance(kDateTimeFormatCID,
                                           NULL,
                                           NS_GET_IID(nsIDateTimeFormat),
                                           (void**) &t);
       
  cout << "Test 1 - CreateInstance():\n";
  if(NS_FAILED(res) || ( t == NULL ) ) {
    cout << "\t1st CreateInstance failed\n";
  } else {
    t->Release();
  }

  res = nsComponentManager::CreateInstance(kDateTimeFormatCID,
                                           NULL,
                                           NS_GET_IID(nsIDateTimeFormat),
                                           (void**) &t);
       
  if(NS_FAILED(res) || ( t == NULL ) ) {
    cout << "\t2nd CreateInstance failed\n";
  } else {
  }


  nsAutoString dateString;
//  nsString locale("en-GB");
  time_t  timetTime;


  cout << "Test 2 - FormatTime():\n";
  time( &timetTime );
  res = t->FormatTime(locale, kDateFormatShort, kTimeFormatSeconds, timetTime, dateString);
  DebugDump(dateString, cout);

  cout << "Test 3 - FormatTMTime():\n";
  time_t ltime;
  time( &ltime );

  // try (almost) all format combination
  res = t->FormatTMTime(locale, kDateFormatNone, kTimeFormatNone, localtime( &ltime ), dateString);
  cout << "kDateFormatNone, kTimeFormatNone:\n";
  DebugDump(dateString, cout);
  res = t->FormatTMTime(locale, kDateFormatNone, kTimeFormatSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatNone, kTimeFormatSeconds:\n";
  DebugDump(dateString, cout);
  res = t->FormatTMTime(locale, kDateFormatNone, kTimeFormatNoSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatNone, kTimeFormatNoSeconds:\n";
  DebugDump(dateString, cout);
  res = t->FormatTMTime(locale, kDateFormatLong, kTimeFormatNone, localtime( &ltime ), dateString);
  cout << "kDateFormatLong, kTimeFormatNone:\n";
  DebugDump(dateString, cout);
  res = t->FormatTMTime(locale, kDateFormatLong, kTimeFormatSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatLong, kTimeFormatSeconds:\n";
  DebugDump(dateString, cout);
  res = t->FormatTMTime(locale, kDateFormatLong, kTimeFormatNoSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatLong, kTimeFormatNoSeconds:\n";
  DebugDump(dateString, cout);
  res = t->FormatTMTime(locale, kDateFormatShort, kTimeFormatNone, localtime( &ltime ), dateString);
  cout << "kDateFormatShort, kTimeFormatNone:\n";
  DebugDump(dateString, cout);
  res = t->FormatTMTime(locale, kDateFormatShort, kTimeFormatSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatShort, kTimeFormatSeconds:\n";
  DebugDump(dateString, cout);
  res = t->FormatTMTime(locale, kDateFormatShort, kTimeFormatNoSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatShort, kTimeFormatNoSeconds:\n";
  DebugDump(dateString, cout);
  res = t->FormatTMTime(locale, kDateFormatYearMonth, kTimeFormatNone, localtime( &ltime ), dateString);
  cout << "kDateFormatYearMonth, kTimeFormatNone:\n";
  DebugDump(dateString, cout);
  res = t->FormatTMTime(locale, kDateFormatYearMonth, kTimeFormatSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatYearMonth, kTimeFormatSeconds:\n";
  DebugDump(dateString, cout);
  res = t->FormatTMTime(locale, kDateFormatYearMonth, kTimeFormatNoSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatYearMonth, kTimeFormatNoSeconds:\n";
  DebugDump(dateString, cout);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatNone, localtime( &ltime ), dateString);
  cout << "kDateFormatWeekday, kTimeFormatNone:\n";
  DebugDump(dateString, cout);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatWeekday, kTimeFormatSeconds:\n";
  DebugDump(dateString, cout);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatNoSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatWeekday, kTimeFormatNoSeconds:\n";
  DebugDump(dateString, cout);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatSecondsForce24Hour, localtime( &ltime ), dateString);
  cout << "kDateFormatWeekday, kTimeFormatSecondsForce24Hour:\n";
  DebugDump(dateString, cout);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatNoSecondsForce24Hour, localtime( &ltime ), dateString);
  cout << "kDateFormatWeekday, kTimeFormatNoSecondsForce24Hour:\n";
  DebugDump(dateString, cout);

  res = t->Release();
  
  cout << "==============================\n";
  cout << "Finish nsIDateTimeFormat Test \n";
  cout << "==============================\n";
}

static nsresult NewLocale(const nsString* localeName, nsILocale** locale)
{
	nsILocaleFactory*	localeFactory;
  nsresult res;

	res = nsComponentManager::FindFactory(kLocaleFactoryCID, (nsIFactory**)&localeFactory); 
  if (NS_FAILED(res) || localeFactory == nsnull) cout << "FindFactory nsILocaleFactory failed\n";

  res = localeFactory->NewLocale(localeName, locale);
  if (NS_FAILED(res) || locale == nsnull) cout << "NewLocale failed\n";

	localeFactory->Release();

  return res;
}

static void Test_nsLocale()
{
#ifdef XP_MAC
  nsString localeName;
  nsIMacLocale* macLocale;
  short script, lang;
  nsresult res;

  if (NS_SUCCEEDED(res = nsComponentManager::CreateInstance(
                         kMacLocaleFactoryCID, NULL, kIMacLocaleIID, (void**)&macLocale))) {
    
    localeName.SetString("en-US");
    res = macLocale->GetPlatformLocale(&localeName, &script, &lang);
    printf("script for en-US is 0\n");
    printf("result: script = %d lang = %d\n", script, lang);

    localeName.SetString("en-GB");
    res = macLocale->GetPlatformLocale(&localeName, &script, &lang);
    printf("script for en-GB is 0\n");
    printf("result: script = %d lang = %d\n", script, lang);

    localeName.SetString("fr-FR");
    res = macLocale->GetPlatformLocale(&localeName, &script, &lang);
    printf("script for fr-FR is 0\n");
    printf("result: script = %d lang = %d\n", script, lang);

    localeName.SetString("de-DE");
    res = macLocale->GetPlatformLocale(&localeName, &script, &lang);
    printf("script for de-DE is 0\n");
    printf("result: script = %d lang = %d\n", script, lang);

    localeName.SetString("ja-JP");
    res = macLocale->GetPlatformLocale(&localeName, &script, &lang);
    printf("script for ja-JP is 1\n");
    printf("result: script = %d lang = %d\n", script, lang);

    macLocale->Release();
  }
#elif defined(XP_PC)
  nsString localeName;
  nsIWin32Locale* win32Locale;
  LCID lcid;
  char cstr[32];
  nsresult res;

  if (NS_SUCCEEDED(res = nsComponentManager::CreateInstance(
                         kWin32LocaleFactoryCID, NULL, kIWin32LocaleIID, (void**)&win32Locale))) {
    
    localeName.SetString("en-US");
    res = win32Locale->GetPlatformLocale(&localeName, &lcid);
    printf("LCID for en-US is 1033\n");
    printf("result: locale = %s LCID = 0x%0.4x %d\n", localeName.ToCString(cstr, 32), lcid, lcid);

    localeName.SetString("en-GB");
    res = win32Locale->GetPlatformLocale(&localeName, &lcid);
    printf("LCID for en-GB is 2057\n");
    printf("result: locale = %s LCID = 0x%0.4x %d\n", localeName.ToCString(cstr, 32), lcid, lcid);

    localeName.SetString("fr-FR");
    res = win32Locale->GetPlatformLocale(&localeName, &lcid);
    printf("LCID for fr-FR is 1036\n");
    printf("result: locale = %s LCID = 0x%0.4x %d\n", localeName.ToCString(cstr, 32), lcid, lcid);

    localeName.SetString("de-DE");
    res = win32Locale->GetPlatformLocale(&localeName, &lcid);
    printf("LCID for de-DE is 1031\n");
    printf("result: locale = %s LCID = 0x%0.4x %d\n", localeName.ToCString(cstr, 32), lcid, lcid);

    localeName.SetString("ja-JP");
    res = win32Locale->GetPlatformLocale(&localeName, &lcid);
    printf("LCID for ja-JP is 1041\n");
    printf("result: locale = %s LCID = 0x%0.4x %d\n", localeName.ToCString(cstr, 32), lcid, lcid);

    win32Locale->Release();
  }
#else
  nsString localeName;
  char locale[32];
  size_t length = 32;
  nsIPosixLocale* posixLocale;
  char cstr[32];
  nsresult res;

  if (NS_SUCCEEDED(res = nsComponentManager::CreateInstance(
                         kPosixLocaleFactoryCID, NULL, kIPosixLocaleIID, (void**)&posixLocale))) {
    localeName.SetString("en-US");
    res = posixLocale->GetPlatformLocale(&localeName, locale, length);
    printf("result: locale = %s POSIX = %s\n", localeName.ToCString(cstr, 32), locale);

    localeName.SetString("en-GB");
    res = posixLocale->GetPlatformLocale(&localeName, locale, length);
    printf("result: locale = %s POSIX = %s\n", localeName.ToCString(cstr, 32), locale);

    localeName.SetString("fr-FR");
    res = posixLocale->GetPlatformLocale(&localeName, locale, length);
    printf("result: locale = %s POSIX = %s\n", localeName.ToCString(cstr, 32), locale);

    localeName.SetString("de-DE");
    res = posixLocale->GetPlatformLocale(&localeName, locale, length);
    printf("result: locale = %s POSIX = %s\n", localeName.ToCString(cstr, 32), locale);

    localeName.SetString("ja-JP");
    res = posixLocale->GetPlatformLocale(&localeName, locale, length);
    printf("result: locale = %s POSIX = %s\n", localeName.ToCString(cstr, 32), locale);

    posixLocale->Release();
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
  nsILocale *locale = NULL;
  nsresult res; 

	nsILocaleFactory*	localeFactory = nsnull;

	res = nsComponentManager::FindFactory(kLocaleFactoryCID, (nsIFactory**)&localeFactory); 
  if (NS_FAILED(res) || localeFactory == nsnull) cout << "FindFactory nsILocaleFactory failed\n";

  res = localeFactory->GetApplicationLocale(&locale);
  if (NS_FAILED(res) || locale == nsnull) cout << "GetApplicationLocale failed\n";
	localeFactory->Release();
  
  // --------------------------------------------
    nsCollationStrength strength = kCollationCaseInSensitive;
    FILE *fp = NULL;

  if (argc == 1) {
    TestCollation(locale);
    TestSort(locale, kCollationCaseInSensitive, NULL);
    TestDateTimeFormat(locale);
  }
  else {
    char *s;
    s = find_option(argc, argv, "-h");
    if (s) {
      cout << argv[0] << g_usage;
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
      strength = kCollationCaseSensitive;
    }
    s = get_option(argc, argv, "-locale");
    if (s) {
      nsString localeName(s);
      NS_IF_RELEASE(locale);
      res = NewLocale(&localeName, &locale);  // reset the locale
    }

    // print locale string
    PRUnichar *localeUnichar;
    nsString aLocaleString, aCategory("NSILOCALE_COLLATE");
    locale->GetCategory(aCategory.get(), &localeUnichar);
    aLocaleString.SetString(localeUnichar);
    cout << "locale setting for collation is ";
    DebugDump(aLocaleString, cout);
    aCategory.SetString("NSILOCALE_TIME");
    locale->GetCategory(aCategory.get(), &localeUnichar);
    aLocaleString.SetString(localeUnichar);
    cout << "locale setting for time is ";
    DebugDump(aLocaleString, cout);

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
  NS_IF_RELEASE(locale);

  // --------------------------------------------

  cout << "Finish All The Test Cases\n";

  res = nsComponentManager::FreeLibraries();
  if(NS_FAILED(res))
    cout << "nsComponentManager failed\n";
  else
    cout << "nsComponentManager FreeLibraries Done\n";
  
  return 0;
}
