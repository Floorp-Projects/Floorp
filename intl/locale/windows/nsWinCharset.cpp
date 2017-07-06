
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsIPlatformCharset.h"
#include "nsUConvPropertySearch.h"
#include <windows.h>
#include "nsWin32Locale.h"
#include "nsString.h"
#include "nsPlatformCharset.h"

using namespace mozilla;

static constexpr nsUConvProp kWinCharsets[] = {
#include "wincharset.properties.h"
};

NS_IMPL_ISUPPORTS(nsPlatformCharset, nsIPlatformCharset)

nsPlatformCharset::nsPlatformCharset()
{
  nsAutoString acpKey(NS_LITERAL_STRING("acp."));
  acpKey.AppendInt(int32_t(::GetACP() & 0x00FFFF), 10);
  MapToCharset(acpKey, mCharset);
}

nsPlatformCharset::~nsPlatformCharset()
{
}

nsresult
nsPlatformCharset::MapToCharset(nsAString& inANSICodePage, nsACString& outCharset)
{
  nsAutoCString key;
  LossyCopyUTF16toASCII(inANSICodePage, key);

  nsresult rv = nsUConvPropertySearch::SearchPropertyValue(kWinCharsets,
      ArrayLength(kWinCharsets), key, outCharset);
  if (NS_FAILED(rv)) {
    outCharset.AssignLiteral("windows-1252");
    return NS_SUCCESS_USING_FALLBACK_LOCALE;
  }
  return rv;
}

NS_IMETHODIMP
nsPlatformCharset::GetCharset(nsPlatformCharsetSel selector,
                              nsACString& oResult)
{
  oResult = mCharset;
  return NS_OK;
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
