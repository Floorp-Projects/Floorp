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
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK *****
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
#include "nsILocaleService.h"
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
    NS_ASSERTION(0, "mCollation creation failed");
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
  int length = UniStrxfrm(locObj, NULL, NS_REINTERPRET_CAST(const UniChar *,stringNormalized.get()),0);
  if (length >= 0) {
    length += 5;                        // Allow for the "extra" chars UniStrxfrm()
                                        //  will out put (overrunning the buffer if
                                        // you let it...)
    // Magic, persistant buffer.  If it's not twice the size we need,
    // we grow/reallocate it 4X so it doesn't grow often.
    static UniChar* pLocalBuffer = NULL;
    static int iBufferLength = 100;
    if (iBufferLength < length*2) {
      if ( pLocalBuffer ) {
        free(pLocalBuffer);
        pLocalBuffer = nsnull;
      }
      iBufferLength = length*4;
    }
    if (!pLocalBuffer)
      pLocalBuffer = (UniChar*) malloc(sizeof(UniChar) * iBufferLength);
    if (pLocalBuffer) {
      // Do the Xfrm
      int uLen = UniStrxfrm(locObj, pLocalBuffer, NS_REINTERPRET_CAST(const UniChar *,stringNormalized.get()), iBufferLength);
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
