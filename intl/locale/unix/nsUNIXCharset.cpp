/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <locale.h>

#include "mozilla/ArrayUtils.h"

#include "nsIPlatformCharset.h"
#include "nsUConvPropertySearch.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#if HAVE_GNU_LIBC_VERSION_H
#include <gnu/libc-version.h>
#endif
#ifdef HAVE_NL_TYPES_H
#include <nl_types.h>
#endif
#if HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif
#include "nsPlatformCharset.h"
#include "prinit.h"
#include "nsUnicharUtils.h"
#include "mozilla/dom/EncodingUtils.h"

using mozilla::dom::EncodingUtils;
using namespace mozilla;

static constexpr nsUConvProp kUnixCharsets[] = {
#include "unixcharset.properties.h"
};

NS_IMPL_ISUPPORTS(nsPlatformCharset, nsIPlatformCharset)

nsPlatformCharset::nsPlatformCharset()
{
}

static nsresult
ConvertLocaleToCharsetUsingDeprecatedConfig(const nsACString& locale,
                                            nsACString& oResult)
{
  if (!(locale.IsEmpty())) {
    if (NS_SUCCEEDED(nsUConvPropertySearch::SearchPropertyValue(kUnixCharsets,
        ArrayLength(kUnixCharsets), locale, oResult))) {
      return NS_OK;
    }
  }
  NS_ERROR("unable to convert locale to charset using deprecated config");
  oResult.AssignLiteral("ISO-8859-1");
  return NS_SUCCESS_USING_FALLBACK_LOCALE;
}

nsPlatformCharset::~nsPlatformCharset()
{
}

NS_IMETHODIMP 
nsPlatformCharset::GetCharset(nsPlatformCharsetSel selector, nsACString& oResult)
{
  oResult = mCharset; 
  return NS_OK;
}

nsresult
nsPlatformCharset::InitGetCharset(nsACString &oString)
{
#if HAVE_LANGINFO_CODESET
  char* nl_langinfo_codeset = nullptr;
  nsCString aCharset;
  nsresult res;

  nl_langinfo_codeset = nl_langinfo(CODESET);
  NS_ASSERTION(nl_langinfo_codeset, "cannot get nl_langinfo(CODESET)");

  //
  // see if we can use nl_langinfo(CODESET) directly
  //
  if (nl_langinfo_codeset) {
    aCharset.Assign(nl_langinfo_codeset);
    res = VerifyCharset(aCharset);
    if (NS_SUCCEEDED(res)) {
      oString = aCharset;
      return res;
    }
  }

  NS_ERROR("unable to use nl_langinfo(CODESET)");
#endif

  //
  // try falling back on a deprecated (locale based) name
  //
  char* locale = setlocale(LC_CTYPE, nullptr);
  nsAutoCString localeStr;
  localeStr.Assign(locale);
  return ConvertLocaleToCharsetUsingDeprecatedConfig(localeStr, oString);
}

NS_IMETHODIMP 
nsPlatformCharset::Init()
{
  //
  // remember default locale so we can use the
  // same charset when asked for the same locale
  //
  char* locale = setlocale(LC_CTYPE, nullptr);
  NS_ASSERTION(locale, "cannot setlocale");
  if (locale) {
    CopyASCIItoUTF16(locale, mLocale); 
  } else {
    mLocale.AssignLiteral("en_US");
  }

  // InitGetCharset only returns NS_OK or NS_SUCESS_USING_FALLBACK_LOCALE
  return InitGetCharset(mCharset);
}

nsresult
nsPlatformCharset::VerifyCharset(nsCString &aCharset)
{
  // fast path for UTF-8.  Most platform uses UTF-8 as charset now.
  if (aCharset.EqualsLiteral("UTF-8")) {
    return NS_OK;
  }

  nsAutoCString encoding;
  if (!EncodingUtils::FindEncodingForLabelNoReplacement(aCharset, encoding)) {
    return NS_ERROR_UCONV_NOCONV;
  }

  aCharset.Assign(encoding);
  return NS_OK;
}
