/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsIPlatformCharset.h"
#include "nsILocaleService.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsIComponentManager.h"
#include <stdio.h>

int
main(int argc, const char** argv)
{

    nsCOMPtr<nsIPlatformCharset> platform_charset = 
        do_CreateInstance(NS_PLATFORMCHARSET_CONTRACTID);
    if (!platform_charset) return -1;

    nsCOMPtr<nsILocaleService>      locale_service = 
        do_CreateInstance(NS_LOCALESERVICE_CONTRACTID);
    if (!locale_service) return -1;

    nsCOMPtr<nsILocale>             locale;
    nsAutoCString                   charset;
    nsAutoString                    category;

    nsresult rv = locale_service->GetSystemLocale(getter_AddRefs(locale));
    if (NS_FAILED(rv)) return -1;

    rv = locale->GetCategory(NS_LITERAL_STRING("NSILOCALE_MESSAGES"), category);
    if (NS_FAILED(rv)) return -1;

    rv = platform_charset->GetDefaultCharsetForLocale(category, charset);
    if (NS_FAILED(rv)) return -1;

    printf("DefaultCharset for %s is %s\n", NS_LossyConvertUTF16toASCII(category).get(), charset.get());

    category.AssignLiteral("en-US");
    rv = platform_charset->GetDefaultCharsetForLocale(category, charset);
    if (NS_FAILED(rv)) return -1;

    printf("DefaultCharset for %s is %s\n", NS_LossyConvertUTF16toASCII(category).get(), charset.get());

    return 0;
}
