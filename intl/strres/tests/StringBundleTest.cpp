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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsIStringBundle.h"
#include "nsIEventQueueService.h"
#include <iostream.h>

#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsIComponentRegistrar.h"
#include "nsNetCID.h"

#include "nsString.h"

#include "nsXPCOM.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
//
#define TEST_URL "resource:/res/strres.properties"

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// let add some locale stuff
//
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#include "nsILocaleService.h"
#include "nsLocaleCID.h"

//
//
//
nsresult
getCountry(PRUnichar *lc_name_unichar, PRUnichar **aCountry)
{

  nsresult        result = NS_OK;
  nsAutoString  	category(NS_LITERAL_STRING("NSILOCALE_MESSAGES"));

  nsAutoString	  lc_name;
  lc_name.Assign(lc_name_unichar);
  // nsMemory::Free(lc_name_unichar);

  PRInt32   dash = lc_name.FindChar('-');
  if (dash > 0) {
    /* 
     */
    nsAutoString lang;
    nsAutoString country;
    PRInt32 count = 0;
    count = lc_name.Left(lang, dash);
    count = lc_name.Right(country, (lc_name.Length()-dash-1));
    *aCountry = ToNewUnicode(country);
  }
  else
    result = NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
getUILangCountry(PRUnichar** aUILang, PRUnichar** aCountry)
{
	nsresult	 result;
	// get a locale service 
	nsCOMPtr<nsILocaleService> localeService = do_GetService(NS_LOCALESERVICE_CONTRACTID, &result);
	NS_ASSERTION(NS_SUCCEEDED(result),"nsLocaleTest: get locale service failed");

  result = localeService->GetLocaleComponentForUserAgent(aUILang);
  result = getCountry(*aUILang, aCountry);
  return result;
}


int
main(int argc, char *argv[])
{
  nsresult ret;

  nsCOMPtr<nsIServiceManager> servMan;
  NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
  nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
  NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
  registrar->AutoRegister(nsnull);
  
  nsCOMPtr<nsIStringBundleService> service =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  if (!service) {
    printf("cannot create service\n");
    return 1;
  }

  PRUnichar *uiLang = nsnull;
  PRUnichar *country = nsnull;
  ret = getUILangCountry(&uiLang, &country);
#if DEBUG_tao
  nsAutoString uaStr(uiLang); // testing only
  nsAutoString countryStr(country); // testing only
  cout << "\n uaStr=" << ToNewCString(uaStr) 
       << ", country=" << ToNewCString(countryStr) 
       << "\n" << endl;
#endif

  nsIStringBundle* bundle = nsnull;

  ret = service->CreateBundle(TEST_URL, &bundle);

  if (NS_FAILED(ret)) {
    printf("cannot create instance\n");
    return 1;
  }

  nsAutoString v;
  v.Assign(NS_LITERAL_STRING(""));
  PRUnichar *ptrv = nsnull;
  char *value = nsnull;

  // 123
  ret = bundle->GetStringFromID(123, &ptrv);
  if (NS_FAILED(ret)) {
    printf("cannot get string from ID 123, ret=%d\n", ret);
    return 1;
  }
  v = ptrv;
  value = ToNewCString(v);
  cout << "123=\"" << value << "\"" << endl;

  // file
  nsString strfile;
  strfile.Assign(NS_LITERAL_STRING("file"));
  const PRUnichar *ptrFile = strfile.get();
  ret = bundle->GetStringFromName(ptrFile, &ptrv);
  if (NS_FAILED(ret)) {
    printf("cannot get string from name\n");
    return 1;
  }
  v = ptrv;
  value = ToNewCString(v);
  cout << "file=\"" << value << "\"" << endl;

  return 0;
}
