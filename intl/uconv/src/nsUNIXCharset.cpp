/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include <locale.h>
#include "nsIPlatformCharset.h"
#include "pratom.h"
#include "nsURLProperties.h"
#include "nsCOMPtr.h"
#include "nsLocaleCID.h"
#include "nsUConvDll.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
#if HAVE_GNU_LIBC_VERSION_H
#include <gnu/libc-version.h>
#endif
#ifdef HAVE_NL_TYPES_H
#include <nl_types.h>
#endif
#if HAVE_NL_LANGINFO
#include <langinfo.h>
#endif

class nsUNIXCharset : public nsIPlatformCharset
{
  NS_DECL_ISUPPORTS

public:

  nsUNIXCharset();
  virtual ~nsUNIXCharset();

  NS_IMETHOD Init();
  NS_IMETHOD GetCharset(nsPlatformCharsetSel selector, nsString& oResult);
  NS_IMETHOD GetDefaultCharsetForLocale(const PRUnichar* localeName, PRUnichar** _retValue);

private:
  nsString mCharset;
  nsString mLocale; // remember the locale & charset

  NS_IMETHOD InitGetCharset(nsString&);
  NS_IMETHOD ConvertLocaleToCharsetUsingDeprecatedConfig(nsAutoString&, nsString&);
  NS_IMETHOD VerifyCharset(nsString &aCharset);
};

NS_IMPL_ISUPPORTS1(nsUNIXCharset, nsIPlatformCharset);

static nsURLProperties *gNLInfo = nsnull;
static nsURLProperties *gInfo_deprecated = nsnull;
static PRInt32 gCnt=0;

nsUNIXCharset::nsUNIXCharset()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&gCnt);
}

NS_IMETHODIMP
nsUNIXCharset::ConvertLocaleToCharsetUsingDeprecatedConfig(nsAutoString& locale, nsString& oResult)
{

  // XXX for thread safety the following block should be locked
  if(nsnull == gInfo_deprecated)
  {
      nsAutoString propertyURL;
      propertyURL.AssignWithConversion("resource:/res/unixcharset.properties");
      nsURLProperties *info = new nsURLProperties( propertyURL );
      NS_ASSERTION( info, "cannot create nsURLProperties");
      gInfo_deprecated = info;
  }

  if(gInfo_deprecated && locale.Length())
  {
      nsAutoString platformLocaleKey;
      // note: NS_LITERAL_STRING("locale." OSTYPE ".") does not compile on AIX
      platformLocaleKey.Assign(NS_LITERAL_STRING("locale."));
      platformLocaleKey.AppendWithConversion(OSTYPE);
      platformLocaleKey.Append(NS_LITERAL_STRING("."));
      platformLocaleKey.Append(locale.get());

      nsresult res = gInfo_deprecated->Get(platformLocaleKey, oResult);
      if(NS_SUCCEEDED(res))  {
         return NS_OK;
      }
      nsAutoString localeKey;
      localeKey.Assign(NS_LITERAL_STRING("locale.all."));
      localeKey.Append(locale.get());
      res = gInfo_deprecated->Get(localeKey, oResult);
      if(NS_SUCCEEDED(res))  {
         return NS_OK;
      }
   }
   NS_ASSERTION(0, "unable to convert locale to charset using deprecated config");
   mCharset.AssignWithConversion("ISO-8859-1");
   return NS_ERROR_USING_FALLBACK_LOCALE;
}

nsUNIXCharset::~nsUNIXCharset()
{
  PR_AtomicDecrement(&gCnt);
  if(0 == gCnt) {
     if (gNLInfo) {
        delete gNLInfo;
        gNLInfo = nsnull;
     }
     if (gInfo_deprecated) {
        delete gInfo_deprecated;
        gInfo_deprecated = nsnull;
     }
  }
}

NS_IMETHODIMP 
nsUNIXCharset::GetCharset(nsPlatformCharsetSel selector, nsString& oResult)
{
   oResult = mCharset; 
   return NS_OK;
}

NS_IMETHODIMP 
nsUNIXCharset::GetDefaultCharsetForLocale(const PRUnichar* localeName, PRUnichar** _retValue)
{
  nsAutoString localeNameAsString(localeName);

  // 
  // if this locale is the user's locale then use the charset 
  // we already determined at initialization
  // 
  if (mLocale.Equals(localeNameAsString) ||
     // support the 4.x behavior
     (mLocale.EqualsIgnoreCase("en_US") && localeNameAsString.EqualsIgnoreCase("C"))) {
    *_retValue = mCharset.ToNewUnicode();
    return NS_OK;
  }

#if HAVE_NL_LANGINFO
  //
  // This locale appears to be a different locale from the user's locale. 
  // To do this we would need to lock the global resource we are currently 
  // using or use a library that provides multi locale support. 
  // ICU is a possible example of a multi locale library.
  //     http://oss.software.ibm.com/icu/
  // 
  NS_ASSERTION(0, "GetDefaultCharsetForLocale: need to add multi locale support");
  // until we add multi locale support: use the the charset of the user's locale
  *_retValue = mCharset.ToNewUnicode();
  return NS_ERROR_USING_FALLBACK_LOCALE;
#endif

  //
  // convert from locale to charset
  // using the deprecated locale to charset mapping 
  //
  nsAutoString localeStr(localeName);
  nsString charset;
  nsresult res = ConvertLocaleToCharsetUsingDeprecatedConfig(localeStr, charset);
  if (NS_SUCCEEDED(res)) {
    *_retValue = charset.ToNewUnicode();
    return res; // succeeded
  }

  NS_ASSERTION(0, "unable to convert locale to charset using deprecated config");
  charset.Assign(NS_LITERAL_STRING("ISO-8859-1"));
  *_retValue=charset.ToNewUnicode();
  return NS_ERROR_USING_FALLBACK_LOCALE;
}

//----------------------------------------------------------------------

NS_IMETHODIMP
NS_NewPlatformCharset(nsISupports* aOuter, 
                      const nsIID &aIID,
                      void **aResult)
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aOuter) {
    *aResult = nsnull;
    return NS_ERROR_NO_AGGREGATION;
  }
  nsUNIXCharset* inst = new nsUNIXCharset();
  if (!inst) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult res = inst->Init();
  if (NS_FAILED(res)) {
    if (res != NS_ERROR_USING_FALLBACK_LOCALE) {
      delete inst;
      return res;
    }
  }

  res = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(res)) {
    *aResult = nsnull;
    delete inst;
  }
  return res;
}

NS_IMETHODIMP
nsUNIXCharset::InitGetCharset(nsString &oString)
{
  char* nl_langinfo_codeset = nsnull;
  nsString aCharset;
  nsresult res;

#if HAVE_NL_LANGINFO && defined(CODESET)
  nl_langinfo_codeset = nl_langinfo(CODESET);
  NS_ASSERTION(nl_langinfo_codeset, "cannot get nl_langinfo(CODESET)");

  // XXX for thread safety the following block should be locked
  if(nsnull == gNLInfo) {
    nsAutoString propertyURL;
    // note: NS_LITERAL_STRING("resource:/res/unixcharset." OSARCH ".properties") does not compile on AIX
    propertyURL.Assign(NS_LITERAL_STRING("resource:/res/unixcharset."));
    propertyURL.AppendWithConversion(OSARCH);
    propertyURL.Append(NS_LITERAL_STRING(".properties"));
    nsURLProperties *info;
    info = new nsURLProperties( propertyURL );
    NS_ASSERTION( info, "cannot create nsURLProperties");
    if (info) {
      PRBool didLoad;
      info->DidLoad(didLoad);
      if (!didLoad) {
        delete info;
        info = nsnull;
      }
    }
    gNLInfo = info;
  }
#endif

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
      localeKey.Assign(NS_LITERAL_STRING("nllic."));
      localeKey.AppendWithConversion(glibc_version);
      localeKey.Append(NS_LITERAL_STRING("."));
      localeKey.AppendWithConversion(nl_langinfo_codeset);
      res = gNLInfo->Get(localeKey, aCharset);
      if (NS_SUCCEEDED(res)) {
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
    localeKey.Assign(NS_LITERAL_STRING("nllic."));
    localeKey.AppendWithConversion(nl_langinfo_codeset);
    res = gNLInfo->Get(localeKey, aCharset);
    if (NS_SUCCEEDED(res)) {
       res = VerifyCharset(aCharset);
       if (NS_SUCCEEDED(res)) {
         oString = aCharset;
         return res;
       }
    }
  }

  //
  // Did not find a charset override so
  // see if we can use nl_langinfo(CODESET) directly
  //
  if (nl_langinfo_codeset) {
    aCharset.AssignWithConversion(nl_langinfo_codeset);
    res = VerifyCharset(aCharset);
    if (NS_SUCCEEDED(res)) {
      oString = aCharset;
      return res;
    }
  }

#if HAVE_NL_LANGINFO
   NS_ASSERTION(0, "unable to use nl_langinfo(CODESET)");
#endif

  //
  // try falling back on a deprecated (locale based) name
  //
  char* locale = setlocale(LC_CTYPE, nsnull);
  nsAutoString localeStr;
  localeStr.AssignWithConversion(locale);
  res = ConvertLocaleToCharsetUsingDeprecatedConfig(localeStr, aCharset);
  if (NS_SUCCEEDED(res)) {
    oString = aCharset;
    return res; // succeeded
  }

  return res;
}

NS_IMETHODIMP 
nsUNIXCharset::Init()
{
  nsString charset;
  nsresult res;

  //
  // remember default locale so we can use the
  // same charset when asked for the same locale
  //
  char* locale = setlocale(LC_CTYPE, nsnull);
  NS_ASSERTION(locale, "cannot setlocale");
  if (locale) {
    mLocale.AssignWithConversion(locale);
  } else {
    mLocale.Assign(NS_LITERAL_STRING("en_US"));
  }

  res = InitGetCharset(charset);
  if (NS_SUCCEEDED(res)) {
    mCharset = charset;
    return res; // succeeded
  }

  // last resort fallback
  NS_ASSERTION(0, "unable to convert locale to charset using deprecated config");
  mCharset.Assign(NS_LITERAL_STRING("ISO-8859-1"));
  return NS_ERROR_USING_FALLBACK_LOCALE;
}

NS_IMETHODIMP 
nsUNIXCharset::VerifyCharset(nsString &aCharset)
{
  nsresult res;
  //
  // get the convert manager
  //
  nsCOMPtr <nsICharsetConverterManager2>  charsetConverterManager;
  charsetConverterManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &res);
  if (NS_FAILED(res))
    return res;

  //
  // check if we recognize the charset string
  //
  nsCOMPtr <nsIAtom> charsetAtom;
  res = charsetConverterManager->GetCharsetAtom(aCharset.get(), 
                                                  getter_AddRefs(charsetAtom));
  if (NS_FAILED(res)) {
    return res;
  }

  //
  // check if we can get an input converter
  //
  nsCOMPtr <nsIUnicodeEncoder> enc;
  res = charsetConverterManager->GetUnicodeEncoder(charsetAtom, getter_AddRefs(enc));
  if (NS_FAILED(res)) {
    NS_ASSERTION(0, "failed to create encoder");
    return res;
  }

  //
  // check if we can get an output converter
  //
  nsCOMPtr <nsIUnicodeDecoder> dec;
  res = charsetConverterManager->GetUnicodeDecoder(charsetAtom, getter_AddRefs(dec));
  if (NS_FAILED(res)) {
    NS_ASSERTION(0, "failed to create decoder");
    return res;
  }

  //
  // return the preferred string
  //
  const PRUnichar *prefName;
  res = charsetAtom->GetUnicode(&prefName);
  if (NS_SUCCEEDED(res))
    aCharset.Assign(prefName);
  NS_ASSERTION(NS_SUCCEEDED(res), "failed to get preferred charset name, using non-preferred");
  return NS_OK;
}

