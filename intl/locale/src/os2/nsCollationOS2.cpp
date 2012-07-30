/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 07/05/2000       IBM Corp.      Reworked file.
 */

#include "unidef.h"
#include "prmem.h"
#include "nsCollationOS2.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsLocaleCID.h"
#include "nsIPlatformCharset.h"
#include "nsIOS2Locale.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsCRT.h"

NS_IMPL_ISUPPORTS1(nsCollationOS2, nsICollation)

nsCollationOS2::nsCollationOS2()
{
  mCollation = NULL;
}

nsCollationOS2::~nsCollationOS2()
{
   if (mCollation != NULL)
     delete mCollation;
}

/* Workaround for GCC problem */
#ifndef LOCI_iCodepage
#define LOCI_iCodepage ((LocaleItem)111)
#endif

nsresult nsCollationOS2::Initialize(nsILocale *locale)
{
  NS_ASSERTION(mCollation == NULL, "Should only be initialized once");

  nsresult res;

  mCollation = new nsCollation;
  if (mCollation == NULL) {
    NS_ERROR("mCollation creation failed");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}


nsresult nsCollationOS2::CompareString(PRInt32 strength, 
                                       const nsAString& string1, const nsAString& string2, PRInt32* result)
{
  nsAutoString stringNormalized1, stringNormalized2;
  if (strength != kCollationCaseSensitive) {
    nsresult res;
    res = mCollation->NormalizeString(string1, stringNormalized1);
    if (NS_FAILED(res)) {
      return res;
    }
    res = mCollation->NormalizeString(string2, stringNormalized2);
    if (NS_FAILED(res)) {
      return res;
    }
  } else {
    stringNormalized1 = string1;
    stringNormalized2 = string2;
  }

  LocaleObject locObj = NULL;
  int ret = UniCreateLocaleObject(UNI_UCS_STRING_POINTER, (UniChar *)L"", &locObj);
  if (ret != ULS_SUCCESS)
    UniCreateLocaleObject(UNI_UCS_STRING_POINTER, (UniChar *)L"C", &locObj);

  *result = UniStrcoll(locObj, (UniChar *)stringNormalized1.get(), (UniChar *)stringNormalized2.get());

  UniFreeLocaleObject(locObj);

  return NS_OK;
}
 

nsresult nsCollationOS2::AllocateRawSortKey(PRInt32 strength, 
                                            const nsAString& stringIn, PRUint8** key, PRUint32* outLen)
{
  nsresult res = NS_OK;

  nsAutoString stringNormalized;
  if (strength != kCollationCaseSensitive) {
    res = mCollation->NormalizeString(stringIn, stringNormalized);
    if (NS_FAILED(res))
      return res;
  } else {
    stringNormalized = stringIn;
  }

  LocaleObject locObj = NULL;
  int ret = UniCreateLocaleObject(UNI_UCS_STRING_POINTER, (UniChar *)L"", &locObj);
  if (ret != ULS_SUCCESS)
    UniCreateLocaleObject(UNI_UCS_STRING_POINTER, (UniChar *)L"C", &locObj);

  res = NS_ERROR_FAILURE;               // From here on out assume failure...
  int length = UniStrxfrm(locObj, NULL, reinterpret_cast<const UniChar *>(stringNormalized.get()),0);
  if (length >= 0) {
    length += 5;                        // Allow for the "extra" chars UniStrxfrm()
                                        //  will out put (overrunning the buffer if
                                        // you let it...)
    // Magic, persistent buffer.  If it's not twice the size we need,
    // we grow/reallocate it 4X so it doesn't grow often.
    static UniChar* pLocalBuffer = NULL;
    static int iBufferLength = 100;
    if (iBufferLength < length*2) {
      if ( pLocalBuffer ) {
        free(pLocalBuffer);
        pLocalBuffer = nullptr;
      }
      iBufferLength = length*4;
    }
    if (!pLocalBuffer)
      pLocalBuffer = (UniChar*) malloc(sizeof(UniChar) * iBufferLength);
    if (pLocalBuffer) {
      // Do the Xfrm
      int uLen = UniStrxfrm(locObj, pLocalBuffer, reinterpret_cast<const UniChar *>(stringNormalized.get()), iBufferLength);
      // See how big the result really is
      uLen = UniStrlen(pLocalBuffer);
      // make sure it will fit in the output buffer...
      if (uLen < iBufferLength) {
          // Success!
          // Give 'em the real size in bytes...
          *key = (PRUint8 *)nsCRT::strdup((PRUnichar*) pLocalBuffer);
          *outLen = uLen * 2 + 2;
          res = NS_OK;
      }
    }
  }
  UniFreeLocaleObject(locObj);

  return res;
}

nsresult nsCollationOS2::CompareRawSortKey(const PRUint8* key1, PRUint32 len1, 
                                           const PRUint8* key2, PRUint32 len2, 
                                           PRInt32* result)
{
  *result = PL_strcmp((const char *)key1, (const char *)key2);
  return NS_OK;
}
