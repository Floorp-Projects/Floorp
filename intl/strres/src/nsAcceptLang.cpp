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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Contributor(s): Tao Cheng <tao@netscape>
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

#define NS_IMPL_IDS

#include "prmem.h"
#include "nsIMemory.h"
#include "nsIServiceManager.h"
#include "nsID.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIStringBundle.h"
#include "nsAcceptLang.h"

/* define CID & IID */
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

/////////////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(nsAcceptLang, nsIAcceptLang)

nsAcceptLang::nsAcceptLang()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsAcceptLang::~nsAcceptLang()
{
  /* destructor code */
}

// NS_IMPL_THREADSAFE_ISUPPORTS1(nsAcceptLang, nsIAcceptLang)

/* wstring getAcceptLangFromLocale (in wstring aLocale); */
NS_IMETHODIMP
nsAcceptLang::GetAcceptLangFromLocale(const PRUnichar *aLocale, PRUnichar **_retval)
{
  nsDependentString lc_name(aLocale);
  if (lc_name.Length() <=0) {
#ifdef DEBUG
    printf("nsAcceptLang::GetAcceptLangFromLocale: aLocale is empty!");
#endif
    // TODO: don't return; instead, use system locale: lc_name=...
    return NS_ERROR_FAILURE;
  }

  nsresult res;

	nsCOMPtr<nsIStringBundleService> sBundleService = 
	         do_GetService(kStringBundleServiceCID, &res);
 	if (NS_FAILED(res) || (nsnull == sBundleService)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIStringBundle> bundle;
#if 1
  res = sBundleService->CreateBundle("resource:/res/language.properties",
                                     getter_AddRefs(bundle));
#else
  res = sBundleService->CreateBundle("chrome://global/locale/languageNames.properties",
                                     getter_AddRefs(bundle));
#endif
  PRUnichar *ptrv = nsnull;
  nsAutoString  lc_tmp(aLocale);
  NS_NAMED_LITERAL_STRING(sAccept, ".accept");
  NS_NAMED_LITERAL_STRING(sTrue, "true");

  lc_tmp.ToLowerCase();
  lc_tmp.Append(sAccept);
  if (NS_OK == (res = bundle->GetStringFromName(lc_tmp.get(), &ptrv))) {
    if (sTrue.Equals(ptrv)) {
      // valid name already
      *_retval = ToNewUnicode(lc_name);
      return res;
    }
  }

  /* not in languageNames.properties; lang only?
   */
  PRInt32  dash = lc_tmp.FindCharInSet("-");
  nsAutoString lang;
  nsAutoString country;
  if (dash > 0) {
    /* lang-country
     */
    PRInt32 count = 0;
    count = lc_tmp.Left(lang, dash);
    count = lc_tmp.Right(country, (lc_tmp.Length()-dash-1));
    /* ja-JP -> ja*/
  }
  else {
    /* ja ?? en-JP 
       en-JP ->ja (how about product locale or syste, locale ???)
       ja-EN ->ja
    */
    lang = lc_name;
  }
  
  // lang always in lower case; don't convert
  *_retval = ToNewUnicode(lang);
  lang.Append(sAccept);
  if (NS_OK == (res = bundle->GetStringFromName(lang.get(), &ptrv))) {

    if (sTrue.Equals(ptrv)) {
      /* lang is accepted */
      return res;
    }
  }

  /* unsupported lang */
  *_retval = nsnull;
  return NS_ERROR_FAILURE;
}

/* wstring getLocaleFromAcceptLang (in wstring aName); */
NS_IMETHODIMP
nsAcceptLang::GetLocaleFromAcceptLang(const PRUnichar *aName, PRUnichar **_retval)
{
  nsresult res = NS_OK;

  if (!aName) {
    /* shall we use system locale instead */
    *_retval = nsnull;
    return NS_ERROR_FAILURE;
  }
  
  nsAutoString acceptLang(aName);

  /* TODO: need to parse accept lang since "en; q=0.3, ja,... 
   */

  /* always return lang-country 
   */
  PRInt32   dash = acceptLang.FindCharInSet("-");
  if (dash > 0) {
    /* lang-country already */
    *_retval = ToNewUnicode(acceptLang);
    return res;
  }
  /* lang only 
   */
  nsCOMPtr<nsIStringBundleService> sBundleService = 
           do_GetService(kStringBundleServiceCID, &res);
  if (NS_FAILED(res) || (nsnull == sBundleService)) {
#ifdef DEBUG
    printf("\n** nsAcceptLang::GetLocaleFromAcceptLang: failed to get nsIStringBundleService!! **\n");
#endif
    return NS_ERROR_FAILURE;
  }
  
  nsIStringBundle *bundle = nsnull;
  /* shall we put the file in res/ instead ? */
  res = sBundleService->CreateBundle("chrome://global/locale/accept2locale.properties",
                                     &bundle);
    
  PRUnichar *ptrv = nsnull;
  if (NS_OK == (res = bundle->GetStringFromName(acceptLang.get(), &ptrv))) {
    
    // valid name already
    *_retval = ToNewUnicode(nsDependentString(ptrv));
  }
  else {
    /* shall we use system locale instead ? */
  }
  
  /* ja -> ja-JP
   * en-JP -> ?
   * default -> system locale, product locale, or good guess from aName?
   */
  return res;
}

/* wstring acceptLang2List (in wstring aName, in wstring aList); */
NS_IMETHODIMP 
nsAcceptLang::AcceptLang2List(const PRUnichar *aName, const PRUnichar *aList, PRUnichar **_retval)
{
  nsresult rv = NS_OK;

  /* if aName is not in aList: append it
   */

  /* if aName is in aList: bring it to the front and assign higher value
   */

  return rv;                                                     
}
