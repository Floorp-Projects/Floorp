/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 *   John Fairhurst
 *   Henry Sobotka
 *   IBM Corp.
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

#include "mozilla/Util.h"

#include "nsIPlatformCharset.h"
#include "nsUConvPropertySearch.h"
#include "pratom.h"
#define INCL_WIN
#include <os2.h>
#include "nsIOS2Locale.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsLocaleCID.h"
#include "nsIServiceManager.h"
#include "nsPlatformCharset.h"

using namespace mozilla;

static const char* kOS2Charsets[][3] = {
#include "os2charset.properties.h"
};

NS_IMPL_ISUPPORTS1(nsPlatformCharset, nsIPlatformCharset)

nsPlatformCharset::nsPlatformCharset()
{
  UINT acp = ::WinQueryCp(HMQ_CURRENT);
  PRInt32 acpint = (PRInt32)(acp & 0x00FFFF);
  nsAutoString acpKey(NS_LITERAL_STRING("os2."));
  acpKey.AppendInt(acpint, 10);
  nsresult res = MapToCharset(acpKey, mCharset);
}

nsPlatformCharset::~nsPlatformCharset()
{
}

nsresult
nsPlatformCharset::MapToCharset(nsAString& inANSICodePage, nsACString& outCharset)
{
  nsCAutoString key;
  LossyCopyUTF16toASCII(inANSICodePage, key);

  nsresult rv = nsUConvPropertySearch::SearchPropertyValue(kOS2Charsets,
      ArrayLength(kOS2Charsets), key, outCharset);
  if (NS_FAILED(rv)) {
    outCharset.AssignLiteral("IBM850");
  }
  return rv;
}

NS_IMETHODIMP 
nsPlatformCharset::GetCharset(nsPlatformCharsetSel selector,
                              nsACString& oResult)
{
  if ((selector == kPlatformCharsetSel_4xBookmarkFile) || (selector == kPlatformCharsetSel_4xPrefsJS)) {
    if ((mCharset.Find("IBM850", IGNORE_CASE) != -1) || (mCharset.Find("IBM437", IGNORE_CASE) != -1)) 
      oResult.AssignLiteral("ISO-8859-1");
    else if (mCharset.Find("IBM852", IGNORE_CASE) != -1)
      oResult.AssignLiteral("windows-1250");
    else if ((mCharset.Find("IBM855", IGNORE_CASE) != -1) || (mCharset.Find("IBM866", IGNORE_CASE) != -1))
      oResult.AssignLiteral("windows-1251");
    else if ((mCharset.Find("IBM869", IGNORE_CASE) != -1) || (mCharset.Find("IBM813", IGNORE_CASE) != -1))
      oResult.AssignLiteral("windows-1253");
    else if (mCharset.Find("IBM857", IGNORE_CASE) != -1)
      oResult.AssignLiteral("windows-1254");
    else
      oResult = mCharset;
  } else {
    oResult = mCharset;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPlatformCharset::GetDefaultCharsetForLocale(const nsAString& localeName, nsACString &oResult)
{
  nsCOMPtr<nsIOS2Locale>	os2Locale;
  ULONG						codepage;
  char						acp_name[6];

  //
  // convert locale name to a code page
  //
  nsresult rv;
  oResult.Truncate();

  os2Locale = do_GetService(NS_OS2LOCALE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) { return rv; }

  rv = os2Locale->GetPlatformLocale(localeName, &codepage);
  if (NS_FAILED(rv)) { return rv; }

  nsAutoString os2_key(NS_LITERAL_STRING("os2."));
  os2_key.AppendInt((PRUint32)codepage);

  return MapToCharset(os2_key, oResult);

}

NS_IMETHODIMP 
nsPlatformCharset::Init()
{
  return NS_OK;
}

nsresult
nsPlatformCharset::InitGetCharset(nsACString &oString)
{
  return NS_OK;
}

nsresult
nsPlatformCharset::ConvertLocaleToCharsetUsingDeprecatedConfig(nsAString& locale, nsACString& oResult)
{
  return NS_OK;
}

nsresult
nsPlatformCharset::VerifyCharset(nsCString &aCharset)
{
  return NS_OK;
}
