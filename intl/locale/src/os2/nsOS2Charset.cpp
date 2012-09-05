/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  int32_t acpint = (int32_t)(acp & 0x00FFFF);
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
  nsAutoCString key;
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
  os2_key.AppendInt((uint32_t)codepage);

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
nsPlatformCharset::VerifyCharset(nsCString &aCharset)
{
  return NS_OK;
}
