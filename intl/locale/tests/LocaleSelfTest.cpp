/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
 
#include <iostream.h>
#include <stdlib.h>
#define NS_IMPL_IDS
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsILocale.h"
#include "nsILocaleFactory.h"
#include "nsLocaleCID.h"
#ifdef XP_PC
#include "nsIWin32Locale.h"
#endif
#include "nsICharsetConverterManager.h"
#include "nsUCvLatinCID.h"
#include "nsICaseConversion.h"
#include "nsUnicharUtilCIID.h"
#include "nsCollationCID.h"
#include "nsICollation.h"
#include "nsDateTimeFormatCID.h"
#include "nsIDateTimeFormat.h"

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
#define LOCALE_DLL_NAME "libnslocale.so"
#define UCONV_DLL       "libuconv.so"
#define UCVLATIN_DLL    "libucvlatin.so"
#define UNICHARUTIL_DLL_NAME "libunicharutil.so"
#endif


// Collation
//
static NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
static NS_DEFINE_IID(kICollationFactoryIID, NS_ICOLLATIONFACTORY_IID);
static NS_DEFINE_CID(kCollationCID, NS_COLLATION_CID);
static NS_DEFINE_IID(kICollationIID, NS_ICOLLATION_IID);
// Date and Time
//
static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);
static NS_DEFINE_IID(kIDateTimeFormatIID, NS_IDATETIMEFORMAT_IID);
// locale
//
static NS_DEFINE_CID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);
#ifdef XP_PC
static NS_DEFINE_CID(kWin32LocaleFactoryCID, NS_WIN32LOCALEFACTORY_CID);
#endif
// case conversion
//
static NS_DEFINE_CID(kUnicharUtilCID, NS_UNICHARUTIL_CID);

// global variable
static PRBool g_verbose = PR_FALSE;

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

#ifdef WIN32
static wchar_t* new_wchar(const nsString& aString)
{
  wchar_t *wchstring;

  wchstring = new wchar_t [aString.Length() + 1];
  if (wchstring) {
    for (int i = 0; i < aString.Length(); i++) {
      wchstring[i] = aString[i];
    }
    wchstring[i] = 0;
  }
  return wchstring;
}
static void delete_wchar(wchar_t *wch_ptr)
{
  if (wch_ptr)
    delete [] wch_ptr;
}
static PRInt32 TestCompare_wcscmp(nsString& string1, nsString& string2)
{
  wchar_t *wchstring1, *wchstring2;
  PRInt32 wcscmpresult;
  
  wchstring1 = new_wchar(string1);
  wchstring2 = new_wchar(string2);

  wcscmpresult = (PRInt32) wcscmp(wchstring1, wchstring2);

  delete [] wchstring1;
  delete [] wchstring2;

  delete_wchar(wchstring1);
  delete_wchar(wchstring2);

  return wcscmpresult;
}
#endif //WIN32

static void DebugDump(nsString& aString, ostream& aStream) {
#ifdef WIN32
  wchar_t *wchstring;

  wchstring = new_wchar(aString);
  if (wchstring) {
    aStream.flush();
    wprintf(L"%s\n", wchstring);
    delete_wchar(wchstring);
  }
#else
  aString.DebugDump(aStream);
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
                                kICollationFactoryIID,
                                (void**) &f);
           
   cout << "Test 1 - CreateInstance():\n";
   if(NS_FAILED(res) || ( f == NULL ) ) {
     cout << "\t1st CreateInstance failed\n";
   } else {
     f->Release();
   }

   res = nsComponentManager::CreateInstance(kCollationFactoryCID,
                                NULL,
                                kICollationFactoryIID,
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
     nsresult res;

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

#ifdef WIN32
// always asserts because WinAPI compare 'a' < 'A' whild wcscmp result is 'a' > 'A'
//      NS_ASSERTION(result == TestCompare_wcscmp(string1, string2), "WIN32 the result did not match with wcscmp().");
#endif //WIN32

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
        printf("%0.2x ", aKey1[i]);
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
       printf("%0.2x ", aKey2[i]);
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

  res = g_collationInst->CompareSortKey(key1, key2, &result);
  NS_ASSERTION(NS_SUCCEEDED(res), "CreateSortKey");
  NS_ASSERTION(NS_SUCCEEDED((PRBool)(result == result2)), "result unmatch");
  if (g_verbose) {
    // Warning: casting to char*
    char *cstr = string1.ToNewCString();
    cout << cstr << ' ';
    delete [] cstr;
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
    cstr = string2.ToNewCString();
    cout << ' ' << cstr << '\n';
    delete [] cstr;
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
      printf("%0.2x ", aKey[j]);
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
  char buf[256];
  int i = 0;

  // read lines and put them to nsStrings
  while (fgets(buf, 256, fp)) {
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
    string_array[i].SetString(buf);
    i++;
  }  
  cout << "print string before sort\n";
  TestSortPrint1(string_array, i);

  g_collationInst = collationInst;
  qsort( (void *)string_array, i, sizeof(nsString), compare1 );

  cout << "print string after sort\n";
  TestSortPrint1(string_array, i);
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
                              kICollationFactoryIID,
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
  cout << "==============================\n";
  cout << "Start nsIDateTimeFormat Test \n";
  cout << "==============================\n";

  nsIDateTimeFormat *t = NULL;
  nsresult res;
  res = nsComponentManager::CreateInstance(kDateTimeFormatCID,
                              NULL,
                              kIDateTimeFormatIID,
                              (void**) &t);
       
  cout << "Test 1 - CreateInstance():\n";
  if(NS_FAILED(res) || ( t == NULL ) ) {
    cout << "\t1st CreateInstance failed\n";
  } else {
    t->Release();
  }

  res = nsComponentManager::CreateInstance(kDateTimeFormatCID,
                              NULL,
                              kIDateTimeFormatIID,
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
  dateString.DebugDump(cout);

  cout << "Test 3 - FormatTMTime():\n";
  time_t ltime;
  time( &ltime );

  // try (almost) all format combination
  res = t->FormatTMTime(locale, kDateFormatNone, kTimeFormatNone, localtime( &ltime ), dateString);
  cout << "kDateFormatNone, kTimeFormatNone:\n";
  dateString.DebugDump(cout);
  res = t->FormatTMTime(locale, kDateFormatNone, kTimeFormatSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatNone, kTimeFormatSeconds:\n";
  dateString.DebugDump(cout);
  res = t->FormatTMTime(locale, kDateFormatNone, kTimeFormatNoSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatNone, kTimeFormatNoSeconds:\n";
  dateString.DebugDump(cout);
  res = t->FormatTMTime(locale, kDateFormatLong, kTimeFormatNone, localtime( &ltime ), dateString);
  cout << "kDateFormatLong, kTimeFormatNone:\n";
  dateString.DebugDump(cout);
  res = t->FormatTMTime(locale, kDateFormatLong, kTimeFormatSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatLong, kTimeFormatSeconds:\n";
  dateString.DebugDump(cout);
  res = t->FormatTMTime(locale, kDateFormatLong, kTimeFormatNoSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatLong, kTimeFormatNoSeconds:\n";
  dateString.DebugDump(cout);
  res = t->FormatTMTime(locale, kDateFormatShort, kTimeFormatNone, localtime( &ltime ), dateString);
  cout << "kDateFormatShort, kTimeFormatNone:\n";
  dateString.DebugDump(cout);
  res = t->FormatTMTime(locale, kDateFormatShort, kTimeFormatSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatShort, kTimeFormatSeconds:\n";
  dateString.DebugDump(cout);
  res = t->FormatTMTime(locale, kDateFormatShort, kTimeFormatNoSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatShort, kTimeFormatNoSeconds:\n";
  dateString.DebugDump(cout);
  res = t->FormatTMTime(locale, kDateFormatYearMonth, kTimeFormatNone, localtime( &ltime ), dateString);
  cout << "kDateFormatYearMonth, kTimeFormatNone:\n";
  dateString.DebugDump(cout);
  res = t->FormatTMTime(locale, kDateFormatYearMonth, kTimeFormatSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatYearMonth, kTimeFormatSeconds:\n";
  dateString.DebugDump(cout);
  res = t->FormatTMTime(locale, kDateFormatYearMonth, kTimeFormatNoSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatYearMonth, kTimeFormatNoSeconds:\n";
  dateString.DebugDump(cout);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatNone, localtime( &ltime ), dateString);
  cout << "kDateFormatWeekday, kTimeFormatNone:\n";
  dateString.DebugDump(cout);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatWeekday, kTimeFormatSeconds:\n";
  dateString.DebugDump(cout);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatNoSeconds, localtime( &ltime ), dateString);
  cout << "kDateFormatWeekday, kTimeFormatNoSeconds:\n";
  dateString.DebugDump(cout);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatSecondsForce24Hour, localtime( &ltime ), dateString);
  cout << "kDateFormatWeekday, kTimeFormatSecondsForce24Hour:\n";
  dateString.DebugDump(cout);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatNoSecondsForce24Hour, localtime( &ltime ), dateString);
  cout << "kDateFormatWeekday, kTimeFormatNoSecondsForce24Hour:\n";
  dateString.DebugDump(cout);

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
  if (NS_FAILED(res) || locale == nsnull) cout << "GetSystemLocale failed\n";

	localeFactory->Release();

  return res;
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

int main(int argc, char** argv) {
  nsILocale *locale = NULL;
  nsresult res; 

#if 0
	nsILocaleFactory*	localeFactory;

	res = nsComponentManager::FindFactory(kLocaleFactoryCID, (nsIFactory**)&localeFactory);
  if (NS_FAILED(res) || localeFactory == nsnull) cout << "FindFactory nsILocaleFactory failed\n";

  res = localeFactory->GetApplicationLocale(&locale);
  if (NS_FAILED(res) || locale == nsnull) cout << "GetSystemLocale failed\n";

	localeFactory->Release();
#endif//0
  
  // --------------------------------------------
    nsCollationStrength strength = kCollationCaseInSensitive;
    FILE *fp = NULL;

#ifdef XP_MAC
    TestCollation(locale);
    // open "sort.txt" in the same directory
    fp = fopen("sort.txt", "r");
    TestSort(locale, kCollationCaseInSensitive, fp);
    if (fp != NULL) {
      fclose(fp);
    }
    TestDateTimeFormat(locale);
#else  
  if (argc == 1) {
    TestCollation(locale);
    TestSort(locale, kCollationCaseInSensitive, NULL);
    TestDateTimeFormat(locale);
  }
  else {
    char *s;
    s = find_option(argc, argv, "-h");
    if (s) {
      cout << argv[0] << " usage:\n-date\tdate time format test\n-col\tcollation test\n-sort\tsort test\n\
-f file\tsort data file\n-case\tcase sensitive sort\n-locale\tlocale\n-v\tverbose";
      return 0;
    }
    s = find_option(argc, argv, "-v");
    if (s) {
      g_verbose = PR_TRUE;
    }
    s = get_option(argc, argv, "-f");
    if (s) {
      fp = fopen(s, "r");
    }
    s = find_option(argc, argv, "-case");
    if (s) {
      strength = kCollationCaseSensitive;
    }
    s = get_option(argc, argv, "-locale");
    if (s) {
      nsString localeName(s);
      NS_IF_RELEASE(locale);
 //     res = NewLocale(&localeName, &locale);  // reset the locale
    }

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
  }
#endif//XP_MAC

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
