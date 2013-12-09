/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsCharsetAlias.h"

// for NS_ERROR_UCONV_NOCONV
#include "nsCharsetConverterManager.h"

#include "nsUConvPropertySearch.h"

using namespace mozilla;

// 
static const char* kAliases[][3] = {
#include "charsetalias.properties.h"
};

//--------------------------------------------------------------
// static
nsresult
nsCharsetAlias::GetPreferredInternal(const nsACString& aAlias,
                                     nsACString& oResult)
{
   nsAutoCString key(aAlias);
   ToLowerCase(key);

   return nsUConvPropertySearch::SearchPropertyValue(kAliases,
      ArrayLength(kAliases), key, oResult);
}

//--------------------------------------------------------------
// static
nsresult
nsCharsetAlias::GetPreferred(const nsACString& aAlias,
                             nsACString& oResult)
{
   if (aAlias.IsEmpty()) return NS_ERROR_NULL_POINTER;

   nsresult res = GetPreferredInternal(aAlias, oResult);
   if (NS_FAILED(res))
      return res;

   if (nsCharsetConverterManager::IsInternal(oResult))
      return NS_ERROR_UCONV_NOCONV;

   return res;
}

//--------------------------------------------------------------
// static
nsresult
nsCharsetAlias::Equals(const nsACString& aCharset1,
                       const nsACString& aCharset2, bool* oResult)
{
   nsresult res = NS_OK;

   if(aCharset1.Equals(aCharset2, nsCaseInsensitiveCStringComparator())) {
      *oResult = true;
      return res;
   }

   if(aCharset1.IsEmpty() || aCharset2.IsEmpty()) {
      *oResult = false;
      return res;
   }

   *oResult = false;
   nsAutoCString name1;
   res = GetPreferredInternal(aCharset1, name1);
   if (NS_FAILED(res))
     return res;

   nsAutoCString name2;
   res = GetPreferredInternal(aCharset2, name2);
   if (NS_FAILED(res))
     return res;

   *oResult = name1.Equals(name2);
   return NS_OK;
}
