/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#define NS_IMPL_IDS
#include "nsICharsetConverterManager.h"
#include "nsIPlatformCharset.h"
#undef NS_IMPL_IDS
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsCollation.h"
#include "nsCollationCID.h"
#include "nsUnicharUtilCIID.h"
#include "prmem.h"

////////////////////////////////////////////////////////////////////////////////

NS_DEFINE_IID(kICollationFactoryIID, NS_ICOLLATIONFACTORY_IID);
NS_DEFINE_CID(kCollationCID, NS_COLLATION_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

NS_IMPL_ISUPPORTS1(nsCollationFactory, nsICollationFactory);

nsresult nsCollationFactory::CreateCollation(nsILocale* locale, nsICollation** instancePtr)
{
  // Create a collation interface instance.
  //
  nsICollation *inst;
  nsresult res;
  
  res = nsComponentManager::CreateInstance(kCollationCID, NULL, NS_GET_IID(nsICollation), (void**) &inst);
  if (NS_FAILED(res)) {
    return res;
  }

  inst->Initialize(locale);
  *instancePtr = inst;

  return res;
}

////////////////////////////////////////////////////////////////////////////////

NS_DEFINE_CID(kUnicharUtilCID, NS_UNICHARUTIL_CID);
NS_DEFINE_IID(kCaseConversionIID, NS_ICASECONVERSION_IID);

nsCollation::nsCollation()
{
  mCaseConversion = NULL;
  nsresult res = nsComponentManager::CreateInstance(kUnicharUtilCID, NULL, kCaseConversionIID, (void**) &mCaseConversion);
  NS_ASSERTION(NS_SUCCEEDED(res), "CreateInstance failed for kCaseConversionIID");
}

nsCollation::~nsCollation()
{
  if (mCaseConversion != NULL)
    mCaseConversion->Release();
}

nsresult nsCollation::CompareString(nsICollation *inst, const nsCollationStrength strength, 
                                    const nsString& string1, const nsString& string2, PRInt32* result)
{
  PRUint32 aLength1, aLength2;
  PRUint8 *aKey1, *aKey2;
  nsresult res;

  res = inst->GetSortKeyLen(strength, string1, &aLength1);
  if (NS_FAILED(res))
    return res;
  res = inst->GetSortKeyLen(strength, string2, &aLength2);
  if (NS_FAILED(res)) {
    return res;
  }

  // if input string is small then use local buffer for keys
  if (aLength1 <= 128 && aLength2 <= 128) {
    PRUint8 aKeyBuf1[128], aKeyBuf2[128];
    res = inst->CreateRawSortKey(strength, string1, aKeyBuf1, &aLength1);
    if (NS_SUCCEEDED(res)) {
      res = inst->CreateRawSortKey(strength, string2, aKeyBuf2, &aLength2);
      if (NS_SUCCEEDED(res)) {
        *result = CompareRawSortKey(aKeyBuf1, aLength1, aKeyBuf2, aLength2);
      }
    }
    return res;
  }

  // Create a key for string1
  aKey1 = new PRUint8[aLength1];
  if (NULL == aKey1)
    return NS_ERROR_OUT_OF_MEMORY;
  res = inst->CreateRawSortKey(strength, string1, aKey1, &aLength1);
  if (NS_FAILED(res)) {
    delete [] aKey1;
    return res;
  }

  // Create a key for string2
  aKey2 = new PRUint8[aLength2];
  if (NULL == aKey2) {
    delete [] aKey1;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  res = inst->CreateRawSortKey(strength, string2, aKey2, &aLength2);
  if (NS_FAILED(res)) {
    delete [] aKey1;
    delete [] aKey2;
    return res;
  }

  // Compare keys
  *result = CompareRawSortKey(aKey1, aLength1, aKey2, aLength2);

  // delete keys
  delete [] aKey1;
  delete [] aKey2;

  return res;
}

nsresult nsCollation::CreateSortKey(nsICollation *inst, const nsCollationStrength strength, 
                                    const nsString& stringIn, nsString& key)
{
  PRUint32 aLength;
  PRUint8 *aKey;
  nsresult res;

  res = inst->GetSortKeyLen(strength, stringIn, &aLength);
  if (NS_SUCCEEDED(res)) {
	  PRUint32 bufferLength = (aLength==0) ? 2 :((aLength + 1) / 2 * 2);  // should be even
	
    aKey = new PRUint8[bufferLength];
    if (nsnull != aKey) {
      aKey[bufferLength-1] = 0;                     // pre-set zero to the padding
      res = inst->CreateRawSortKey(strength, stringIn, aKey, &aLength);
      if (NS_SUCCEEDED(res)) {
        PRUnichar *aKeyInUnichar = (PRUnichar *) aKey;
        PRUint32 aLengthUnichar = bufferLength / 2;

        // to avoid nsString to assert, chop off the last null word (padding)
        // however, collation key may contain zero's anywhere in the key
        // so we may still get assertion as long as nsString is used to hold collation key
        // use CreateRawSortKey instead (recommended) to avoid this to happen
        if (aKeyInUnichar[aLengthUnichar-1] == 0)
          aLengthUnichar--;

        key.Assign(aKeyInUnichar, aLengthUnichar);
      }
      delete [] aKey;
    }
    else {
      res = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return res;
}

PRInt32 nsCollation::CompareRawSortKey(const PRUint8* key1, const PRUint32 len1, 
                                       const PRUint8* key2, const PRUint32 len2)
{
  PRUint32 len = (len1 < len2) ? len1 : len2;
  PRInt32 result;

  result = (PRUint32) memcmp(key1, key2, len);
  if (result == 0 && len1 != len2) {
    result = (len1 < len2) ? -1 : 1;
  }

  return result;
}

PRInt32 nsCollation::CompareSortKey(const nsString& key1, const nsString& key2) 
{
  PRUint8 *rawKey1, *rawKey2;
  PRUint32 len1, len2;

  rawKey1 = (PRUint8 *) key1.GetUnicode();
  len1 = key1.Length() * sizeof(PRUnichar);
  rawKey2 = (PRUint8 *) key2.GetUnicode();
  len2 = key2.Length() * sizeof(PRUnichar);

  return CompareRawSortKey(rawKey1, len1, rawKey2, len2);
}

nsresult nsCollation::NormalizeString(nsString& stringInOut)
{
  if (mCaseConversion == NULL) {
    stringInOut.ToLowerCase();
  }
  else {
    PRInt32 aLength = stringInOut.Length();

    if (aLength <= 64) {
      PRUnichar conversionBuf[64];
      mCaseConversion->ToLower(stringInOut.GetUnicode(), conversionBuf, aLength);
      stringInOut.Assign(conversionBuf, aLength);
    }
    else {
      PRUnichar *aBuffer;
      aBuffer = new PRUnichar[aLength];
      if (aBuffer == NULL) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mCaseConversion->ToLower(stringInOut.GetUnicode(), aBuffer, aLength);
      stringInOut.Assign(aBuffer, aLength);
      delete [] aBuffer;
    }
  }
  return NS_OK;
}

nsresult nsCollation::UnicodeToChar(const nsString& src, char** dst, const nsString& aCharset)
{
  nsICharsetConverterManager * ccm = nsnull;
  nsresult res;

  res = nsServiceManager::GetService(kCharsetConverterManagerCID, 
                                     NS_GET_IID(nsICharsetConverterManager), 
                                     (nsISupports**)&ccm);
  if(NS_SUCCEEDED(res) && (nsnull != ccm)) {
    nsIUnicodeEncoder* encoder = nsnull;
    res = ccm->GetUnicodeEncoder(&aCharset, &encoder);
    if(NS_SUCCEEDED(res) && (nsnull != encoder)) {
      const PRUnichar *unichars = src.GetUnicode();
      PRInt32 unicharLength = src.Length();
      PRInt32 dstLength;
      res = encoder->GetMaxLength(unichars, unicharLength, &dstLength);
      *dst = (char *) PR_Malloc(dstLength + 1);
      if (*dst != nsnull) {
        res = encoder->Convert(unichars, &unicharLength, *dst, &dstLength);
        (*dst)[dstLength] = '\0';
      }
      else {
        res = NS_ERROR_OUT_OF_MEMORY;
      }
      NS_IF_RELEASE(encoder);
    }    
    nsServiceManager::ReleaseService(kCharsetConverterManagerCID, ccm);
  }
  return res;
}



