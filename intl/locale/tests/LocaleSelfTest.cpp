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
#include "nsISupports.h"
#include "nsRepository.h"
#include "nsCollationCID.h"
#include "nsICollation.h"
#include "nsDateTimeFormatCID.h"
#include "nsIDateTimeFormat.h"

#ifdef XP_MAC
#define LOCALE_DLL_NAME "LOCALE_DLL"
#else
#define LOCALE_DLL_NAME "NSLOCALE.DLL"
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
static PRInt32 TestCompare_wcscmp(nsString& string1, nsString& string2)
{
    wchar_t *wchstring1, *wchstring2;
    PRInt32 wcscmpresult;
    PRInt32 i;
    
    wchstring1 = new wchar_t [string1.Length() + 1];
    for (i = 0; i < string1.Length(); i++) {
      wchstring1[i] = string1(i);
    }
    wchstring1[i] = 0;
    wchstring2 = new wchar_t [string2.Length() + 1];
    for (i = 0; i < string2.Length(); i++) {
      wchstring2[i] = string2(i);
    }
    wchstring2[i] = 0;

    wcscmpresult = (PRInt32) wcscmp(wchstring1, wchstring2);

    delete [] wchstring1;
    delete [] wchstring2;

    return wcscmpresult;
}
#endif //WIN32

static void TestCollation()
{
   nsString locale("en-US");
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
      for (i = 0; i < (PRUint32) string1.Length(); i++) {
        cout << "string1[" << i << "]: " << (char) string1.CharAt(i) << "\n"; // warning:casting down to char
      }
      for (i = 0; i < (PRUint32) string2.Length(); i++) {
        cout << "string2[" << i << "]: " << (char) string2.CharAt(i) << "\n"; // warning:casting down to char
      }
      for (i = 0; i < (PRUint32) string3.Length(); i++) {
        cout << "string3[" << i << "]: " << (char) string3.CharAt(i) << "\n"; // warning:casting down to char
      }
      for (i = 0; i < (PRUint32) string4.Length(); i++) {
        cout << "string4[" << i << "]: " << (char) string4.CharAt(i) << "\n"; // warning:casting down to char
      }

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
      for (i = 0; i < keyLength1; i++) {
        cout << "key[" << i << "]: " << aKey1[i] << "\n";
      }
      cout << "\n";

      res = CreateCollationKey(t, kCollationCaseInSensitive, string2, &aKey2, &keyLength2);
      if(NS_FAILED(res)) {
        cout << "\tFailed!! return value != NS_OK\n";
      }
      cout << "case insensitive key creation:\n";
      cout << "keyLength: " << keyLength2 << "\n";
      for (i = 0; i < keyLength2; i++) {
        cout << "key[" << i << "]: " << aKey2[i] << "\n";
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
    string_array[i].DebugDump(cout);
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

  res = g_collationInst->CompareString(kCollationCaseSensitive, string1, string2, &result);

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

  for (int i = 0; i < len; i++) {
    aLength = key_array[i].aLength;
    aKey = key_array[i].aKey;
    for (int j = 0; j < (int)aLength; j++) {
      char str[8];
      sprintf(str, "%0.2x ", aKey[j]);
      cout << str;
    }
    cout << "\n";
  }
  cout << "\n";
}

static void TestSort()
{
  nsresult res;
  nsICollationFactory *factoryInst;
  nsICollation *collationInst;
  collation_rec key_array[5];
  PRUint8 *aKey;
  PRUint32 aLength;
  nsString locale("en-US");
  nsString string1("AAC");
  nsString string2("aac");
  nsString string3("xyz");
  nsString string4("abb");
  nsString string5("aacA");
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

  res = CreateCollationKey(collationInst, kCollationCaseSensitive, string1, &aKey, &aLength);
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

  res = CreateCollationKey(collationInst, kCollationCaseSensitive, string1, &aKey, &aLength);
  if(NS_FAILED(res)) {
    cout << "\tFailed!! return value != NS_OK\n";
  }
  key_array[0].aKey = aKey;
  key_array[0].aLength = aLength;

  res = CreateCollationKey(collationInst, kCollationCaseSensitive, string2, &aKey, &aLength);
  if(NS_FAILED(res)) {
    cout << "\tFailed!! return value != NS_OK\n";
  }
  key_array[1].aKey = aKey;
  key_array[1].aLength = aLength;

  res = CreateCollationKey(collationInst, kCollationCaseSensitive, string3, &aKey, &aLength);
  if(NS_FAILED(res)) {
    cout << "\tFailed!! return value != NS_OK\n";
  }
  key_array[2].aKey = aKey;
  key_array[2].aLength = aLength;

  res = CreateCollationKey(collationInst, kCollationCaseSensitive, string4, &aKey, &aLength);
  if(NS_FAILED(res)) {
    cout << "\tFailed!! return value != NS_OK\n";
  }
  key_array[3].aKey = aKey;
  key_array[3].aLength = aLength;

  res = CreateCollationKey(collationInst, kCollationCaseSensitive, string5, &aKey, &aLength);
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

static void TestDateTimeFormat()
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


  PRUnichar dateString[64];
  PRUint32 length = sizeof(dateString)/sizeof(PRUnichar);
  nsString locale("en-GB");
  time_t  timetTime;
  nsString s_print;


  cout << "Test 2 - FormatTime():\n";
  time( &timetTime );
  res = t->FormatTime(locale, kDateFormatShort, kTimeFormatSeconds, timetTime, 
                        dateString, &length);
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);

  cout << "Test 3 - FormatTMTime():\n";
  time_t ltime;
  time( &ltime );
  length = sizeof(dateString)/sizeof(PRUnichar);

  // try (almost) all format combination
  res = t->FormatTMTime(locale, kDateFormatNone, kTimeFormatNone, localtime( &ltime ), 
                        dateString, &length);
  cout << "kDateFormatNone, kTimeFormatNone:\n";
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);
  length = sizeof(dateString)/sizeof(PRUnichar);
  res = t->FormatTMTime(locale, kDateFormatNone, kTimeFormatSeconds, localtime( &ltime ), 
                        dateString, &length);
  cout << "kDateFormatNone, kTimeFormatSeconds:\n";
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);
  length = sizeof(dateString)/sizeof(PRUnichar);
  res = t->FormatTMTime(locale, kDateFormatNone, kTimeFormatNoSeconds, localtime( &ltime ), 
                        dateString, &length);
  cout << "kDateFormatNone, kTimeFormatNoSeconds:\n";
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);
  length = sizeof(dateString)/sizeof(PRUnichar);
  res = t->FormatTMTime(locale, kDateFormatLong, kTimeFormatNone, localtime( &ltime ), 
                        dateString, &length);
  cout << "kDateFormatLong, kTimeFormatNone:\n";
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);
  length = sizeof(dateString)/sizeof(PRUnichar);
  res = t->FormatTMTime(locale, kDateFormatLong, kTimeFormatSeconds, localtime( &ltime ), 
                        dateString, &length);
  cout << "kDateFormatLong, kTimeFormatSeconds:\n";
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);
  length = sizeof(dateString)/sizeof(PRUnichar);
  res = t->FormatTMTime(locale, kDateFormatLong, kTimeFormatNoSeconds, localtime( &ltime ), 
                        dateString, &length);
  cout << "kDateFormatLong, kTimeFormatNoSeconds:\n";
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);
  length = sizeof(dateString)/sizeof(PRUnichar);
  res = t->FormatTMTime(locale, kDateFormatShort, kTimeFormatNone, localtime( &ltime ), 
                        dateString, &length);
  cout << "kDateFormatShort, kTimeFormatNone:\n";
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);
  length = sizeof(dateString)/sizeof(PRUnichar);
  res = t->FormatTMTime(locale, kDateFormatShort, kTimeFormatSeconds, localtime( &ltime ), 
                        dateString, &length);
  cout << "kDateFormatShort, kTimeFormatSeconds:\n";
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);
  length = sizeof(dateString)/sizeof(PRUnichar);
  res = t->FormatTMTime(locale, kDateFormatShort, kTimeFormatNoSeconds, localtime( &ltime ), 
                        dateString, &length);
  cout << "kDateFormatShort, kTimeFormatNoSeconds:\n";
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);
  length = sizeof(dateString)/sizeof(PRUnichar);
  res = t->FormatTMTime(locale, kDateFormatYearMonth, kTimeFormatNone, localtime( &ltime ), 
                        dateString, &length);
  cout << "kDateFormatYearMonth, kTimeFormatNone:\n";
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);
  length = sizeof(dateString)/sizeof(PRUnichar);
  res = t->FormatTMTime(locale, kDateFormatYearMonth, kTimeFormatSeconds, localtime( &ltime ), 
                        dateString, &length);
  cout << "kDateFormatYearMonth, kTimeFormatSeconds:\n";
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);
  length = sizeof(dateString)/sizeof(PRUnichar);
  res = t->FormatTMTime(locale, kDateFormatYearMonth, kTimeFormatNoSeconds, localtime( &ltime ), 
                        dateString, &length);
  cout << "kDateFormatYearMonth, kTimeFormatNoSeconds:\n";
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);
  length = sizeof(dateString)/sizeof(PRUnichar);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatNone, localtime( &ltime ), 
                        dateString, &length);
  cout << "kDateFormatWeekday, kTimeFormatNone:\n";
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);
  length = sizeof(dateString)/sizeof(PRUnichar);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatSeconds, localtime( &ltime ), 
                        dateString, &length);
  cout << "kDateFormatWeekday, kTimeFormatSeconds:\n";
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);
  length = sizeof(dateString)/sizeof(PRUnichar);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatNoSeconds, localtime( &ltime ), 
                        dateString, &length);
  cout << "kDateFormatWeekday, kTimeFormatNoSeconds:\n";
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);
  length = sizeof(dateString)/sizeof(PRUnichar);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatSecondsForce24Hour, localtime( &ltime ), 
                        dateString, &length);
  cout << "kDateFormatWeekday, kTimeFormatSecondsForce24Hour:\n";
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);
  length = sizeof(dateString)/sizeof(PRUnichar);
  res = t->FormatTMTime(locale, kDateFormatWeekday, kTimeFormatNoSecondsForce24Hour, localtime( &ltime ), 
                        dateString, &length);
  cout << "kDateFormatWeekday, kTimeFormatNoSecondsForce24Hour:\n";
  s_print.SetString(dateString, length);
  s_print.DebugDump(cout);
  length = sizeof(dateString)/sizeof(PRUnichar);

  res = t->Release();
  
  cout << "==============================\n";
  cout << "Finish nsIDateTimeFormat Test \n";
  cout << "==============================\n";
}
 
int main(int argc, char** argv) {
  nsresult res; 
   
  res = nsRepository::RegisterFactory(kCollationFactoryCID,
                                 LOCALE_DLL_NAME,
                                 PR_FALSE,
                                 PR_FALSE);
  if(NS_FAILED(res)) {
    cout << "RegisterFactory failed\n";
  }
  res = nsRepository::RegisterFactory(kCollationCID,
                                 LOCALE_DLL_NAME,
                                 PR_FALSE,
                                 PR_FALSE);
  if(NS_FAILED(res)) {
    cout << "RegisterFactory failed\n";
  }
  res = nsRepository::RegisterFactory(kDateTimeFormatCID,
                                 LOCALE_DLL_NAME,
                                 PR_FALSE,
                                 PR_FALSE);
  if(NS_FAILED(res)) {
    cout << "RegisterFactory failed\n";
  }

  // --------------------------------------------

  TestCollation();
  TestSort();
  TestDateTimeFormat();

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
