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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): Tao Cheng <tao@netscape>
 */

#define NS_IMPL_IDS

#include "prmem.h"
#include "nsIMemory.h"
#include "nsIServiceManager.h"
#include "nsID.h"
#include "nsString.h"
#include "nsIStringBundle.h"
#include "nsAcceptLang.h"

/* define CID & IID */
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
NS_DEFINE_IID(kAcceptLangCID, NS_ACCEPTLANG_CID);

/////////////////////////////////////////////////////////////////////////////////////////

/* util func */
static PRUnichar *copyUnicode(const nsString str) {

  PRInt32   len =  str.Length()+1;
  PRUnichar *retval = (PRUnichar *) PR_Calloc(len, sizeof(PRUnichar));
  retval = (PRUnichar *) memcpy(retval, str.get(), sizeof(PRUnichar)*len);
  retval[len-1] = '\0';

  return retval;
}

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

// NS_IMPL_THREADSAFE_ISUPPORTS(nsAcceptLang, NS_GET_IID(nsIAcceptLang))

/* wstring getAcceptLangFromLocale ([const] in wstring aLocale); */
NS_IMETHODIMP
nsAcceptLang::GetAcceptLangFromLocale(const PRUnichar *aLocale, PRUnichar **_retval)
{
  nsString lc_name(aLocale);
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

  nsIStringBundle *bundle = nsnull;
#if 1
  res = sBundleService->CreateBundle("resource:/res/language.properties",
                              &bundle);
#else
  res = sBundleService->CreateBundle("chrome://global/locale/languageNames.properties",
                              &bundle);
#endif
  PRUnichar *ptrv = nsnull;
  nsString  lc_tmp(aLocale);
  nsCString sAccept(".accept");
  nsCString sTrue("true");

  lc_tmp.ToLowerCase();
  lc_tmp.AppendWithConversion(sAccept);
  if (NS_OK == (res = bundle->GetStringFromName(lc_tmp.get(), &ptrv))) {
    nsString tmp(ptrv);
    if (tmp.EqualsWithConversion(sTrue)) {
      // valid name already
      *_retval = copyUnicode(lc_name);
      return res;
    }
  }

  /* not in languageNames.properties; lang only?
   */
  PRInt32  dash = lc_tmp.FindCharInSet("-");
  nsString lang;
  nsString country;
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
  *_retval = copyUnicode(lang);
  lang.AppendWithConversion(sAccept);
  if (NS_OK == (res = bundle->GetStringFromName(lang.get(), &ptrv))) {

    nsString tmp(ptrv);
    if (tmp.EqualsWithConversion(sTrue)) {
      /* lang is accepted */
      return res;
    }
  }

  /* unsupported lang */
  *_retval = nsnull;
  return NS_ERROR_FAILURE;
}

/* wstring getLocaleFromAcceptLang ([const] in wstring aName); */
NS_IMETHODIMP
nsAcceptLang::GetLocaleFromAcceptLang(const PRUnichar *aName, PRUnichar **_retval)
{
  nsresult res = NS_OK;

  if (!aName) {
    /* shall we use system locale instead */
    *_retval = nsnull;
    return NS_ERROR_FAILURE;
  }
  
  nsString acceptLang(aName);

  /* TODO: need to parse accept lang since "en; q=0.3, ja,... 
   */

  /* always return lang-country 
   */
  PRInt32   dash = acceptLang.FindCharInSet("-");
  if (dash > 0) {
    /* lang-country already */
    *_retval = copyUnicode(acceptLang);
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
    nsString lc_name(ptrv);
    *_retval = copyUnicode(lc_name);
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

/* wstring acceptLang2List ([const] in wstring aName, [const] in wstring aList); */
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
