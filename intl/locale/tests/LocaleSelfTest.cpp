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
#include "nsRepository.h"
#include "nsIServiceManager.h"
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
#define UNICHARUTIL_DLL_NAME "unicharutil_dll"
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
NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
NS_DEFINE_IID(kICollationFactoryIID, NS_ICOLLATIONFACTORY_IID);
NS_DEFINE_CID(kCollationCID, NS_COLLATION_CID);
NS_DEFINE_IID(kICollationIID, NS_ICOLLATION_IID);


// Create a collation key, the memory is allocated using new which need to be deleted by a caller.
//
static nsresult CreateCollationKey(nsICollation *t, nsCollationStrength strength, 
                                   nsString& stringIn, PRUint8 **aKey, PRUint32 *keyLength)
{
  nsresult res;
  
  res = t->GetSortKeyLen(strength, stringIn, keyLength);
  if(NS_FAILED(res)) {
    cout << "\tFailed!! return value != NS_OK\n";
  }
  *aKey = (PRUint8 *) new PRUint8[*keyLength];
  if (NULL == *aKey) {
    cout << "\tFailed!! memory allocation failed.\n";
  }
  res = t->CreateSortKey(strength, stringIn, *aKey, keyLength);
  if(NS_FAILED(res)) {
    cout << "\tFailed!! return value != NS_OK\n";
  }

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
   
   res = nsRepository::CreateInstance(kCollationFactoryCID,
                                NULL,
                                kICollationFactoryIID,
                                (void**) &f);
           
   cout << "Test 1 - CreateInstance():\n";
   if(NS_FAILED(res) || ( f == NULL ) ) {
     cout << "\t1st CreateInstance failed\n";
   } else {
     f->Release();
   }

   res = nsRepository::CreateInstance(kCollationFactoryCID,
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

      cout << "Test 4 - CreateSortKey():\n";
      aKey1 = (PRUint8 *) new PRUint8[keyLength1];
      if (NULL == aKey1) {
        cout << "\tFailed!! memory allocation failed.\n";
      }
      res = t->CreateSortKey(kCollationCaseSensitive, string2, aKey1, &keyLength1);
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

      cout << "Test 5 - CompareSortKey():\n";
      res = CreateCollationKey(t, kCollationCaseSensitive, string1, &aKey3, &keyLength3);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }

      res = t->CompareSortKey(aKey1, keyLength1, aKey3, keyLength3, &result);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }
      cout << "case sensitive comparison (string1 vs string2): " << result << "\n";
      res = t->CompareSortKey(aKey3, keyLength3, aKey1, keyLength1, &result);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }
      cout << "case sensitive comparison (string2 vs string1): " << result << "\n";

      res = t->CompareSortKey(aKey2, keyLength2, aKey3, keyLength3, &result);
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
      res = t->CompareSortKey(aKey1, keyLength1, aKey2, keyLength2, &result);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }
      cout << "case sensitive comparison (string1 vs string3): " << result << "\n";
      res = t->CompareSortKey(aKey1, keyLength1, aKey3, keyLength3, &result);
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
  PRInt32 result;
  nsresult res;

  string1 = *(nsString *) arg1;
  string2 = *(nsString *) arg2;

  res = g_collationInst->CompareString(kCollationCaseInSensitive, string1, string2, &result);

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

  res = g_collationInst->CompareSortKey(keyrec1->aKey, keyrec1->aLength, 
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

// Use nsICollation for qsort.
//
static void TestSort(nsILocale *locale)
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

  res = nsRepository::CreateInstance(kCollationFactoryCID,
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

  strength = kCollationCaseSensitive;

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

  cout << "==============================\n";
  cout << "Finish sort Test \n";
  cout << "==============================\n";
}

// Date and Time
//
NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);
NS_DEFINE_IID(kIDateTimeFormatIID, NS_IDATETIMEFORMAT_IID);

// Test all functions in nsIDateTimeFormat.
//
static void TestDateTimeFormat(nsILocale *locale)
{
  cout << "==============================\n";
  cout << "Start nsIDateTimeFormat Test \n";
  cout << "==============================\n";

  nsIDateTimeFormat *t = NULL;
  nsresult res;
  res = nsRepository::CreateInstance(kDateTimeFormatCID,
                              NULL,
                              kIDateTimeFormatIID,
                              (void**) &t);
       
  cout << "Test 1 - CreateInstance():\n";
  if(NS_FAILED(res) || ( t == NULL ) ) {
    cout << "\t1st CreateInstance failed\n";
  } else {
    t->Release();
  }

  res = nsRepository::CreateInstance(kDateTimeFormatCID,
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
 
// case conversion
//
NS_DEFINE_CID(kUnicharUtilCID, NS_UNICHARUTIL_CID);
NS_DEFINE_IID(kCaseConversionIID, NS_ICASECONVERSION_IID);

int main(int argc, char** argv) {
  nsresult res; 
   
  res = nsRepository::RegisterFactory(kCollationFactoryCID, LOCALE_DLL_NAME, PR_FALSE, PR_FALSE);
  if (NS_FAILED(res)) cout << "RegisterFactory failed\n";

  res = nsRepository::RegisterFactory(kCollationCID, LOCALE_DLL_NAME, PR_FALSE, PR_FALSE);
  if (NS_FAILED(res)) cout << "RegisterFactory failed\n";

  res = nsRepository::RegisterFactory(kDateTimeFormatCID, LOCALE_DLL_NAME, PR_FALSE, PR_FALSE);
  if (NS_FAILED(res)) cout << "RegisterFactory failed\n";

  res = nsRepository::RegisterFactory(kCharsetConverterManagerCID, UCONV_DLL, PR_FALSE, PR_FALSE);
  if (NS_FAILED(res)) cout << "RegisterFactory failed\n";

  res = nsRepository::RegisterFactory(kLatin1ToUnicodeCID, UCVLATIN_DLL, PR_FALSE, PR_FALSE);
  if (NS_FAILED(res)) cout << "RegisterFactory failed\n";

  res = nsRepository::RegisterFactory(kUnicharUtilCID, UNICHARUTIL_DLL_NAME, PR_FALSE, PR_FALSE);
  if (NS_FAILED(res)) cout << "RegisterFactory failed\n";

  // --------------------------------------------

  nsILocale *locale = NULL;

  if (argc == 1) {
    TestCollation(locale);
    TestSort(locale);
    TestDateTimeFormat(locale);
  }
  else {
    while (argc--) {
      if (!strcmp(argv[argc], "col"))
        TestCollation(locale);
      else if (!strcmp(argv[argc], "sort"))
        TestSort(locale);
      else if (!strcmp(argv[argc], "date"))
        TestDateTimeFormat(locale);
    }
  }

  // --------------------------------------------

  cout << "Finish All The Test Cases\n";
  res = NS_OK;
  res = nsRepository::FreeLibraries();

  if(NS_FAILED(res))
    cout << "nsRepository failed\n";
  else
    cout << "nsRepository FreeLibraries Done\n";
  
  return 0;
}
