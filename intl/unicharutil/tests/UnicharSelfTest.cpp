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
#include "nsISupports.h"
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

NS_DEFINE_CID(kUnicharUtilCID, NS_UNICHARUTIL_CID);
NS_DEFINE_IID(kCaseConversionIID, NS_ICASECONVERSION_IID);
NS_DEFINE_CID(kEntityConverterCID, NS_ENTITYCONVERTER_CID);
NS_DEFINE_CID(kSaveAsCharsetCID, NS_SAVEASCHARSET_CID);
NS_DEFINE_IID(kIPersistentPropertiesIID,NS_IPERSISTENTPROPERTIES_IID);

#if defined(XP_UNIX) || defined(XP_BEOS)
#define UNICHARUTIL_DLL_NAME "libunicharutil"MOZ_DLL_SUFFIX
#else
#define UNICHARUTIL_DLL_NAME "UNICHARUTIL_DLL"
#endif

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
   cout << "==============================\n";
   cout << "Start nsICaseConversion Test \n";
   cout << "==============================\n";
   nsICaseConversion *t = NULL;
   nsresult res;
   res = nsServiceManager::GetService(kUnicharUtilCID,
                                kCaseConversionIID,
                                (nsISupports**) &t);
           
   cout << "Test 1 - GetService():\n";
   if(NS_FAILED(res) || ( t == NULL ) ) {
     cout << "\t1st GetService failed\n";
   } else {
     res = nsServiceManager::ReleaseService(kUnicharUtilCID, t);
   }

   res = nsServiceManager::GetService(kUnicharUtilCID,
                                kCaseConversionIID,
                                (nsISupports**) &t);
           
   if(NS_FAILED(res) || ( t == NULL ) ) {
     cout << "\t2nd GetService failed\n";
   } else {
     int i;
     PRUnichar ch;
     PRUnichar buf[256];

    cout << "Test 2 - ToUpper(PRUnichar, PRUnichar*):\n";
    for(i=0;i < T2LEN ; i++)
    {
         res = t->ToUpper(t2data[i], &ch);
         if(NS_FAILED(res)) {
            cout << "\tFailed!! return value != NS_OK\n";
            break;
         }
         if(ch != t2result[i]) 
            cout << "\tFailed!! result unexpected " << i << "\n";
     }


    cout << "Test 3 - ToLower(PRUnichar, PRUnichar*):\n";
    for(i=0;i < T3LEN; i++)
    {
         res = t->ToLower(t3data[i], &ch);
         if(NS_FAILED(res)) {
            cout << "\tFailed!! return value != NS_OK\n";
            break;
         }
         if(ch != t3result[i]) 
            cout << "\tFailed!! result unexpected " << i << "\n";
     }


    cout << "Test 4 - ToTitle(PRUnichar, PRUnichar*):\n";
    for(i=0;i < T4LEN; i++)
    {
         res = t->ToTitle(t4data[i], &ch);
         if(NS_FAILED(res)) {
            cout << "\tFailed!! return value != NS_OK\n";
            break;
         }
         if(ch != t4result[i]) 
            cout << "\tFailed!! result unexpected " << i << "\n";
     }


    cout << "Test 5 - ToUpper(PRUnichar*, PRUnichar*, PRUint32):\n";
    res = t->ToUpper(t2data, buf, T2LEN);
    if(NS_FAILED(res)) {
       cout << "\tFailed!! return value != NS_OK\n";
    } else {
       for(i = 0; i < T2LEN; i++)
       {
          if(buf[i] != t2result[i])
          {
            cout << "\tFailed!! result unexpected " << i << "\n";
            break;
          }
       }
    }

    cout << "Test 6 - ToLower(PRUnichar*, PRUnichar*, PRUint32):\n";
    res = t->ToLower(t3data, buf, T3LEN);
    if(NS_FAILED(res)) {
       cout << "\tFailed!! return value != NS_OK\n";
    } else {
       for(i = 0; i < T3LEN; i++)
       {
          if(buf[i] != t3result[i])
          {
            cout << "\tFailed!! result unexpected " << i << "\n";
            break;
          }
       }
    }

     cout << "Test 7 - ToTitle(PRUnichar*, PRUnichar*, PRUint32):\n";
     cout << "!!! To Be Implemented !!!\n";

   res = nsServiceManager::ReleaseService(kUnicharUtilCID, t);
   }
   cout << "==============================\n";
   cout << "Finish nsICaseConversion Test \n";
   cout << "==============================\n";

}

static void TestEntityConversion(PRUint32 version)
{
  cout << "==============================\n";
  cout << "Start nsIEntityConverter Test \n";
  cout << "==============================\n";

  PRUint32 i;
  nsString inString;
  PRUnichar uChar;
  nsresult res;


  inString.AssignWithConversion("\xA0\xA1\xA2\xA3");
  uChar = (PRUnichar) 8364; //euro
  inString.Append(&uChar, 1);
  uChar = (PRUnichar) 9830; //
  inString.Append(&uChar, 1);

  nsCOMPtr <nsIEntityConverter> entityConv;
  res = nsComponentManager::CreateInstance(kEntityConverterCID, NULL, NS_GET_IID(nsIEntityConverter), getter_AddRefs(entityConv));
  if (NS_FAILED(res)) {cout << "\tFailed!! return value != NS_OK\n"; return;}


  // convert char by char
  for (i = 0; i < inString.Length(); i++) {
    char *entity = NULL;
    res = entityConv->ConvertToEntity(inString[i], version, &entity);
    if (NS_SUCCEEDED(res) && NULL != entity) {
      cout << inString[i] << " " << entity << "\n";
      nsMemory::Free(entity);
    }
  }

  // convert at once as a string
  PRUnichar *entities;
  res = entityConv->ConvertToEntities(inString.get(), version, &entities);
  if (NS_SUCCEEDED(res) && NULL != entities) {
    for (i = 0; i < nsCRT::strlen(entities); i++) {
      cout << (char) entities[i];
      if (';' == (char) entities[i])
        cout << "\n";
    }
    nsMemory::Free(entities);
  }

  cout << "==============================\n";
  cout << "Finish nsIEntityConverter Test \n";
  cout << "==============================\n\n";
}

static void TestSaveAsCharset()
{
  cout << "==============================\n";
  cout << "Start nsISaveAsCharset Test \n";
  cout << "==============================\n";

  nsresult res;

  nsString inString;
  inString.AssignWithConversion("\x61\x62\x80\xA0\x63");
  char *outString;
  
  // first, dump input string
  for (PRUint32 i = 0; i < inString.Length(); i++) {
    cout << inString[i] << " ";
  }
  cout << "\n";

  nsCOMPtr <nsISaveAsCharset> saveAsCharset;
  res = nsComponentManager::CreateInstance(kSaveAsCharsetCID, NULL, NS_GET_IID(nsISaveAsCharset), getter_AddRefs(saveAsCharset));
  if (NS_FAILED(res)) {cout << "\tFailed!! return value != NS_OK\n";}
  
  cout << "ISO-8859-1 " << "attr_plainTextDefault " << "entityNone " << "\n";
  res = saveAsCharset->Init("ISO-8859-1", 
                                 nsISaveAsCharset::attr_plainTextDefault, 
                                 nsIEntityConverter::entityNone);
  if (NS_FAILED(res)) {cout << "\tFailed!! return value != NS_OK\n";}
  res = saveAsCharset->Convert(inString.get(), &outString);
  if (NS_FAILED(res)) {cout << "\tFailed!! return value != NS_OK\n";}
  if (NULL == outString) {cout << "\tFailed!! output null\n";}
  else {cout << outString << "\n"; nsMemory::Free(outString);}

  cout << "ISO-2022-JP " << "attr_plainTextDefault " << "entityNone " << "\n";
  res = saveAsCharset->Init("ISO-2022-JP", 
                                 nsISaveAsCharset::attr_plainTextDefault,
                                 nsIEntityConverter::entityNone);
  if (NS_FAILED(res)) {cout << "\tFailed!! return value != NS_OK\n";}
  res = saveAsCharset->Convert(inString.get(), &outString);
  if (NS_FAILED(res)) {cout << "\tFailed!! return value != NS_OK\n";}
  if (NULL == outString) {cout << "\tFailed!! output null\n";}
  else {cout << outString << "\n"; nsMemory::Free(outString);}
  if (NS_ERROR_UENC_NOMAPPING == res) {
    outString = inString.ToNewUTF8String();
    if (NULL == outString) {cout << "\tFailed!! output null\n";}
    else {cout << "Fall back to UTF-8: " << outString << "\n"; nsMemory::Free(outString);}
  }

  cout << "ISO-2022-JP " << "attr_FallbackQuestionMark " << "entityNone " << "\n";
  res = saveAsCharset->Init("ISO-2022-JP", 
                                 nsISaveAsCharset::attr_FallbackQuestionMark,
                                 nsIEntityConverter::entityNone);
  if (NS_FAILED(res)) {cout << "\tFailed!! return value != NS_OK\n";}
  res = saveAsCharset->Convert(inString.get(), &outString);
  if (NS_FAILED(res)) {cout << "\tFailed!! return value != NS_OK\n";}
  if (NULL == outString) {cout << "\tFailed!! output null\n";}
  else {cout << outString << "\n"; nsMemory::Free(outString);}

  cout << "ISO-2022-JP " << "attr_FallbackEscapeU " << "entityNone " << "\n";
  res = saveAsCharset->Init("ISO-2022-JP", 
                                 nsISaveAsCharset::attr_FallbackEscapeU,
                                 nsIEntityConverter::entityNone);
  if (NS_FAILED(res)) {cout << "\tFailed!! return value != NS_OK\n";}
  res = saveAsCharset->Convert(inString.get(), &outString);
  if (NS_FAILED(res)) {cout << "\tFailed!! return value != NS_OK\n";}
  if (NULL == outString) {cout << "\tFailed!! output null\n";}
  else {cout << outString << "\n"; nsMemory::Free(outString);}

  cout << "ISO-8859-1 " << "attr_htmlTextDefault " << "html40Latin1 " << "\n";
  res = saveAsCharset->Init("ISO-8859-1", 
                                 nsISaveAsCharset::attr_htmlTextDefault, 
                                 nsIEntityConverter::html40Latin1);
  if (NS_FAILED(res)) {cout << "\tFailed!! return value != NS_OK\n";}
  res = saveAsCharset->Convert(inString.get(), &outString);
  if (NS_ERROR_UENC_NOMAPPING != res && NS_FAILED(res)) {cout << "\tFailed!! return value != NS_OK\n";}
  if (NULL == outString) {cout << "\tFailed!! output null\n";}
  else {cout << outString << "\n"; nsMemory::Free(outString);}

  cout << "ISO-8859-1 " << "attr_FallbackHexNCR+attr_EntityAfterCharsetConv " << "html40Latin1 " << "\n";
  res = saveAsCharset->Init("ISO-8859-1", 
                                 nsISaveAsCharset::attr_FallbackHexNCR + 
                                 nsISaveAsCharset::attr_EntityAfterCharsetConv, 
                                 nsIEntityConverter::html40Latin1);
  if (NS_FAILED(res)) {cout << "\tFailed!! return value != NS_OK\n";}
  res = saveAsCharset->Convert(inString.get(), &outString);
  if (NS_ERROR_UENC_NOMAPPING != res && NS_FAILED(res)) {cout << "\tFailed!! return value != NS_OK\n";}
  if (NULL == outString) {cout << "\tFailed!! output null\n";}
  else {cout << outString << "\n"; nsMemory::Free(outString);}


  cout << "==============================\n";
  cout << "Finish nsISaveAsCharset Test \n";
  cout << "==============================\n\n";
}

void RegisterFactories()
{
   nsresult res;
   res = nsComponentManager::RegisterComponent(kUnicharUtilCID,
                                 NULL,
                                 NULL,
                                 UNICHARUTIL_DLL_NAME,
                                 PR_FALSE,
                                 PR_TRUE);
   if(NS_FAILED(res))
     cout << "RegisterComponent failed\n";
}
 
int main(int argc, char** argv) {
   
#ifndef USE_NSREG
   RegisterFactories();
#endif

   // --------------------------------------------

   TestCaseConversion();

   // --------------------------------------------

   TestEntityConversion(nsIEntityConverter::html40);

   // --------------------------------------------

   TestSaveAsCharset();

   // --------------------------------------------
   cout << "Finish All The Test Cases\n";
   nsresult res = NS_OK;
   res = nsComponentManager::FreeLibraries();

   if(NS_FAILED(res))
      cout << "nsComponentManager failed\n";
   else
      cout << "nsComponentManager FreeLibraries Done\n";
   return 0;
}
