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
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsIPlatformCharset.h"
#include "nsILocaleService.h"
#include "nsCOMPtr.h"
#include "nsLocaleCID.h"
#include "nsIComponentManager.h"
#include <stdio.h>

NS_DEFINE_IID(kLocaleServiceIID,NS_ILOCALESERVICE_IID);
NS_DEFINE_CID(kLocaleServiceCID,NS_LOCALESERVICE_CID);
NS_DEFINE_IID(kPlatformCharsetIID,NS_IPLATFORMCHARSET_IID);

int
main(int argc, const char** argv)
{
	nsCOMPtr<nsILocaleService>		locale_service;
	nsCOMPtr<nsILocale>				locale;
	nsCOMPtr<nsIPlatformCharset>	platform_charset;
	nsString						locale_category; locale_category.AssignWithConversion("NSILOCALE_MESSAGES");
	PRUnichar*						category_value, *charset;
	nsString						categoryAsNSString, charsetAsNSString;

	nsresult rv = nsComponentManager::CreateInstance(NS_PLATFORMCHARSET_CONTRACTID,NULL,kPlatformCharsetIID,getter_AddRefs(platform_charset));
	rv = nsComponentManager::CreateInstance(kLocaleServiceCID,NULL,kLocaleServiceIID,getter_AddRefs(locale_service));
	if (NS_FAILED(rv)) return -1;

	rv = locale_service->GetSystemLocale(getter_AddRefs(locale));
	if (NS_FAILED(rv)) return -1;

	rv = locale->GetCategory(locale_category.get(),&category_value);
	if (NS_FAILED(rv)) return -1;

	rv = platform_charset->GetDefaultCharsetForLocale(category_value,&charset);
	if (NS_FAILED(rv)) return -1;

	charsetAsNSString = charset;
	categoryAsNSString = category_value;
	printf("DefaultCharset for %s is %s\n",categoryAsNSString.ToNewCString(),charsetAsNSString.ToNewCString());

	categoryAsNSString.AssignWithConversion("en-US");
	rv = platform_charset->GetDefaultCharsetForLocale(categoryAsNSString.get(),&charset);
	if (NS_FAILED(rv)) return -1;

	charsetAsNSString = charset;
	printf("DefaultCharset for %s is %s\n",categoryAsNSString.ToNewCString(),charsetAsNSString.ToNewCString());

	return 0;
}
