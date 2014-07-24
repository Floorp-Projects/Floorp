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
#include "nsEncoderDecoderUtils.h"
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

static const char* kUnixCharsets[][3] = {
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
    nsAutoCString localeKey;
    localeKey.AssignLiteral("locale.all.");
    localeKey.Append(locale);
    if (NS_SUCCEEDED(nsUConvPropertySearch::SearchPropertyValue(kUnixCharsets,
        ArrayLength(kUnixCharsets), localeKey, oResult))) {
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

NS_IMETHODIMP 
nsPlatformCharset::GetDefaultCharsetForLocale(const nsAString& localeName, nsACString &oResult)
{
  // 
  // if this locale is the user's locale then use the charset 
  // we already determined at initialization
  // 
  if (mLocale.Equals(localeName) ||
    // support the 4.x behavior
    (mLocale.LowerCaseEqualsLiteral("en_us") && 
     localeName.LowerCaseEqualsLiteral("c"))) {
    oResult = mCharset;
    return NS_OK;
  }

#if HAVE_LANGINFO_CODESET
  //
  // This locale appears to be a different locale from the user's locale. 
  // To do this we would need to lock the global resource we are currently 
  // using or use a library that provides multi locale support. 
  // ICU is a possible example of a multi locale library.
  //     http://oss.software.ibm.com/icu/
  //
  // A more common cause of hitting this warning than the above is that 
  // Mozilla is launched under an ll_CC.UTF-8 locale. In xpLocale, 
  // we only store the language and the region (ll-CC) losing 'UTF-8', which
  // leads |mLocale| to be different from |localeName|. Although we lose
  // 'UTF-8', we init'd |mCharset| with the value obtained via 
  // |nl_langinfo(CODESET)| so that we're all right here.
  // 
  NS_WARNING("GetDefaultCharsetForLocale: need to add multi locale support");
#ifdef DEBUG_jungshik
  printf("localeName=%s mCharset=%s\n", NS_ConvertUTF16toUTF8(localeName).get(),
         mCharset.get());
#endif
  // until we add multi locale support: use the the charset of the user's locale
  oResult = mCharset;
  return NS_SUCCESS_USING_FALLBACK_LOCALE;
#else
  //
  // convert from locale to charset
  // using the deprecated locale to charset mapping 
  //
  NS_LossyConvertUTF16toASCII localeStr(localeName);
  return ConvertLocaleToCharsetUsingDeprecatedConfig(localeStr, oResult);
#endif
}

nsresult
nsPlatformCharset::InitGetCharset(nsACString &oString)
{
  char* nl_langinfo_codeset = nullptr;
  nsCString aCharset;
  nsresult res;

#if HAVE_LANGINFO_CODESET
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
