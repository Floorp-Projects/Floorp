/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
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
#include "nsIPlatformCharset.h"
#include "nsILocaleService.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsLocaleCID.h"
#include "nsIComponentManager.h"
#include <stdio.h>

NS_DEFINE_IID(kLocaleServiceIID,NS_ILOCALESERVICE_IID);
NS_DEFINE_CID(kLocaleServiceCID,NS_LOCALESERVICE_CID);
NS_DEFINE_IID(kPlatformCharsetIID,NS_IPLATFORMCHARSET_IID);

int
main(int argc, const char** argv)
{

    nsCOMPtr<nsIPlatformCharset> platform_charset = 
        do_CreateInstance(NS_PLATFORMCHARSET_CONTRACTID);
    if (!platform_charset) return -1;

    nsCOMPtr<nsILocaleService>      locale_service = 
        do_CreateInstance(kLocaleServiceCID);
    if (!locale_service) return -1;

    nsCOMPtr<nsILocale>             locale;
    nsCAutoString                   charset;
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
