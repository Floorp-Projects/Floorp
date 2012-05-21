/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <locale.h>

#include "mozilla/Util.h"

#include "nsIPlatformCharset.h"
#include "pratom.h"
#include "nsUConvPropertySearch.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"
#include "nsICharsetConverterManager.h"
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

using namespace mozilla;

static const char* kUnixCharsets[][3] = {
#include "unixcharset.properties.h"
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsPlatformCharset, nsIPlatformCharset)

nsPlatformCharset::nsPlatformCharset()
{
}

nsresult
nsPlatformCharset::ConvertLocaleToCharsetUsingDeprecatedConfig(nsACString& locale, nsACString& oResult)
{
  if (!(locale.IsEmpty())) {
    nsCAutoString platformLocaleKey;
    // note: NS_LITERAL_STRING("locale." OSTYPE ".") does not compile on AIX
    platformLocaleKey.AssignLiteral("locale.");
    platformLocaleKey.Append(OSTYPE);
    platformLocaleKey.AppendLiteral(".");
    platformLocaleKey.Append(locale);

    nsresult res = nsUConvPropertySearch::SearchPropertyValue(kUnixCharsets,
        ArrayLength(kUnixCharsets), platformLocaleKey, oResult);
    if (NS_SUCCEEDED(res))  {
      return NS_OK;
    }
    nsCAutoString localeKey;
    localeKey.AssignLiteral("locale.all.");
    localeKey.Append(locale);
    res = nsUConvPropertySearch::SearchPropertyValue(kUnixCharsets,
        ArrayLength(kUnixCharsets), localeKey, oResult);
    if (NS_SUCCEEDED(res))  {
      return NS_OK;
    }
   }
   NS_ERROR("unable to convert locale to charset using deprecated config");
   mCharset.AssignLiteral("ISO-8859-1");
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
  nsresult res = ConvertLocaleToCharsetUsingDeprecatedConfig(localeStr, oResult);
  if (NS_SUCCEEDED(res))
    return res;

  NS_ERROR("unable to convert locale to charset using deprecated config");
  oResult.AssignLiteral("ISO-8859-1");
  return NS_SUCCESS_USING_FALLBACK_LOCALE;
#endif
}

nsresult
nsPlatformCharset::InitGetCharset(nsACString &oString)
{
  char* nl_langinfo_codeset = nsnull;
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
  char* locale = setlocale(LC_CTYPE, nsnull);
  nsCAutoString localeStr;
  localeStr.Assign(locale);
  res = ConvertLocaleToCharsetUsingDeprecatedConfig(localeStr, oString);
  if (NS_SUCCEEDED(res)) {
    return res; // succeeded
  }

  oString.Truncate();
  return res;
}

NS_IMETHODIMP 
nsPlatformCharset::Init()
{
  nsCAutoString charset;
  nsresult res = NS_OK;

  //
  // remember default locale so we can use the
  // same charset when asked for the same locale
  //
  char* locale = setlocale(LC_CTYPE, nsnull);
  NS_ASSERTION(locale, "cannot setlocale");
  if (locale) {
    CopyASCIItoUTF16(locale, mLocale); 
  } else {
    mLocale.AssignLiteral("en_US");
  }

  res = InitGetCharset(charset);
  if (NS_SUCCEEDED(res)) {
    mCharset = charset;
    return res; // succeeded
  }

  // last resort fallback
  NS_ERROR("unable to convert locale to charset using deprecated config");
  mCharset.AssignLiteral("ISO-8859-1");
  return NS_SUCCESS_USING_FALLBACK_LOCALE;
}

nsresult
nsPlatformCharset::VerifyCharset(nsCString &aCharset)
{
  nsresult res;
  //
  // get the convert manager
  //
  nsCOMPtr <nsICharsetConverterManager>  charsetConverterManager;
  charsetConverterManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &res);
  if (NS_FAILED(res))
    return res;

  //
  // check if we can get an input converter
  //
  nsCOMPtr <nsIUnicodeEncoder> enc;
  res = charsetConverterManager->GetUnicodeEncoder(aCharset.get(), getter_AddRefs(enc));
  if (NS_FAILED(res)) {
    NS_ERROR("failed to create encoder");
    return res;
  }

  //
  // check if we can get an output converter
  //
  nsCOMPtr <nsIUnicodeDecoder> dec;
  res = charsetConverterManager->GetUnicodeDecoder(aCharset.get(), getter_AddRefs(dec));
  if (NS_FAILED(res)) {
    NS_ERROR("failed to create decoder");
    return res;
  }

  //
  // check if we recognize the charset string
  //

  nsCAutoString result;
  res = charsetConverterManager->GetCharsetAlias(aCharset.get(), result);
  if (NS_FAILED(res)) {
    return res;
  }

  //
  // return the preferred string
  //

  aCharset.Assign(result);
  NS_ASSERTION(NS_SUCCEEDED(res), "failed to get preferred charset name, using non-preferred");
  return NS_OK;
}
