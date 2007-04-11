/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * ***** END LICENSE BLOCK ***** */

#include <locale.h>
#include "nsIPlatformCharset.h"
#include "pratom.h"
#include "nsGREResProperties.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsLocaleCID.h"
#include "nsUConvDll.h"
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
#include "nsAutoLock.h"
#include "prinit.h"
#include "nsUnicharUtils.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsPlatformCharset, nsIPlatformCharset)

static nsGREResProperties *gNLInfo = nsnull;
static nsGREResProperties *gInfo_deprecated = nsnull;
static PRInt32 gCnt=0;

//this lock is for protecting above static variable operation
static PRLock  *gLock = nsnull;

static PRStatus InitLock(void)
{
  gLock = PR_NewLock();
  if (gLock)
    return PR_SUCCESS;
  return PR_FAILURE;
}

nsPlatformCharset::nsPlatformCharset()
{
  PR_AtomicIncrement(&gCnt);
  static PRCallOnceType once;
  PR_CallOnce(&once, InitLock);
  NS_ASSERTION(gLock, "Can't allocate a lock?!");
}

nsresult
nsPlatformCharset::ConvertLocaleToCharsetUsingDeprecatedConfig(nsAString& locale, nsACString& oResult)
{

  // locked for thread safety 
  {
    nsAutoLock guard(gLock);
    if (!gInfo_deprecated) {
      nsGREResProperties *info =
          new nsGREResProperties(NS_LITERAL_CSTRING("unixcharset.properties"));
      NS_ASSERTION(info, "cannot create nsGREResProperties");
      gInfo_deprecated = info;
    }
  }

  if (gInfo_deprecated && !(locale.IsEmpty())) {
    nsAutoString platformLocaleKey;
    // note: NS_LITERAL_STRING("locale." OSTYPE ".") does not compile on AIX
    platformLocaleKey.AssignLiteral("locale.");
    platformLocaleKey.AppendWithConversion(OSTYPE);
    platformLocaleKey.AppendLiteral(".");
    platformLocaleKey.Append(locale);

    nsAutoString charset;
    nsresult res = gInfo_deprecated->Get(platformLocaleKey, charset);
    if (NS_SUCCEEDED(res))  {
      LossyCopyUTF16toASCII(charset, oResult);
      return NS_OK;
    }
    nsAutoString localeKey;
    localeKey.AssignLiteral("locale.all.");
    localeKey.Append(locale);
    res = gInfo_deprecated->Get(localeKey, charset);
    if (NS_SUCCEEDED(res))  {
      LossyCopyUTF16toASCII(charset, oResult);
      return NS_OK;
    }
   }
   NS_ASSERTION(0, "unable to convert locale to charset using deprecated config");
   mCharset.AssignLiteral("ISO-8859-1");
   oResult.AssignLiteral("ISO-8859-1");
   return NS_SUCCESS_USING_FALLBACK_LOCALE;
}

nsPlatformCharset::~nsPlatformCharset()
{
  PR_AtomicDecrement(&gCnt);
  if (!gCnt) {
    if (gNLInfo) {
      delete gNLInfo;
      gNLInfo = nsnull;
      PR_DestroyLock(gLock);
      gLock = nsnull;
    }
    if (gInfo_deprecated) {
      delete gInfo_deprecated;
      gInfo_deprecated = nsnull;
    }
  }
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
#endif

  //
  // convert from locale to charset
  // using the deprecated locale to charset mapping 
  //
  nsAutoString localeStr(localeName);
  nsresult res = ConvertLocaleToCharsetUsingDeprecatedConfig(localeStr, oResult);
  if (NS_SUCCEEDED(res))
    return res;

  NS_ASSERTION(0, "unable to convert locale to charset using deprecated config");
  oResult.AssignLiteral("ISO-8859-1");
  return NS_SUCCESS_USING_FALLBACK_LOCALE;
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

  // locked for thread safety 
  {
    nsAutoLock guard(gLock);

    if (!gNLInfo) {
      nsCAutoString propertyFile;
      // note: NS_LITERAL_CSTRING("unixcharset." OSARCH ".properties") does not compile on AIX
      propertyFile.AssignLiteral("unixcharset.");
      propertyFile.AppendLiteral(NS_STRINGIFY(OSARCH));
      propertyFile.AppendLiteral(".properties");
      nsGREResProperties *info = new nsGREResProperties(propertyFile);
      NS_ASSERTION(info, "cannot create nsGREResProperties");
      if (info) {
        PRBool didLoad = info->DidLoad();
        if (!didLoad) {
          delete info;
          info = nsnull;
        }
      }
      gNLInfo = info;
    }
  }

  //
  // See if we are remapping nl_langinfo(CODESET)
  //
  if (gNLInfo && nl_langinfo_codeset) {
    nsAutoString localeKey;

#if HAVE_GNU_GET_LIBC_VERSION
    //
    // look for an glibc version specific charset remap
    //
    const char *glibc_version = gnu_get_libc_version();
    if ((glibc_version != nsnull) && (strlen(glibc_version))) {
      localeKey.AssignLiteral("nllic.");
      localeKey.AppendWithConversion(glibc_version);
      localeKey.AppendLiteral(".");
      localeKey.AppendWithConversion(nl_langinfo_codeset);
      nsAutoString uCharset;
      res = gNLInfo->Get(localeKey, uCharset);
      if (NS_SUCCEEDED(res)) {
        aCharset.AssignWithConversion(uCharset);
        res = VerifyCharset(aCharset);
        if (NS_SUCCEEDED(res)) {
          oString = aCharset;
          return res;
        }
      }
    }
#endif

    //
    // look for a charset specific charset remap
    //
    localeKey.AssignLiteral("nllic.");
    localeKey.AppendWithConversion(nl_langinfo_codeset);
    nsAutoString uCharset;
    res = gNLInfo->Get(localeKey, uCharset);
    if (NS_SUCCEEDED(res)) {
      aCharset.AssignWithConversion(uCharset);
      res = VerifyCharset(aCharset);
      if (NS_SUCCEEDED(res)) {
        oString = aCharset;
        return res;
      }
    }
  }

  NS_ASSERTION(0, "unable to use nl_langinfo(CODESET)");
#endif

  //
  // try falling back on a deprecated (locale based) name
  //
  char* locale = setlocale(LC_CTYPE, nsnull);
  nsAutoString localeStr;
  localeStr.AssignWithConversion(locale);
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
  NS_ASSERTION(0, "unable to convert locale to charset using deprecated config");
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
    NS_ASSERTION(0, "failed to create encoder");
    return res;
  }

  //
  // check if we can get an output converter
  //
  nsCOMPtr <nsIUnicodeDecoder> dec;
  res = charsetConverterManager->GetUnicodeDecoder(aCharset.get(), getter_AddRefs(dec));
  if (NS_FAILED(res)) {
    NS_ASSERTION(0, "failed to create decoder");
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

nsresult 
nsPlatformCharset::InitInfo()
{  
  return NS_OK;
}
