/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
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

#include <Script.h>
#include <TextCommon.h>
#include "nsIPlatformCharset.h"
#include "pratom.h"
#include "nsURLProperties.h"
#include "nsUConvDll.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIMacLocale.h"
#include "nsLocaleCID.h"
#include "nsReadableUtils.h"
#include "nsPlatformCharset.h"
#include "nsEncoderDecoderUtils.h"

static nsURLProperties *gInfo = nsnull;
static PRInt32 gCnt = 0;

NS_IMPL_ISUPPORTS1(nsPlatformCharset, nsIPlatformCharset)

nsPlatformCharset::nsPlatformCharset()
{
  PR_AtomicIncrement(&gCnt);
}
nsPlatformCharset::~nsPlatformCharset()
{
  PR_AtomicDecrement(&gCnt);
  if((0 == gCnt) && (nsnull != gInfo)) {
  	delete gInfo;
  	gInfo = nsnull;
  }
}

nsresult nsPlatformCharset::InitInfo()
{  
  // load the .property file if necessary
  if (gInfo == nsnull) {
    nsURLProperties *info = new nsURLProperties( NS_LITERAL_CSTRING("resource://gre/res/maccharset.properties") );
    NS_ASSERTION(info , "cannot open properties file");
    NS_ENSURE_TRUE(info, NS_ERROR_FAILURE);
    gInfo = info;
  }

  return NS_OK;
}

nsresult nsPlatformCharset::MapToCharset(short script, short region, nsACString& outCharset)
{
  switch (region) {
    case verUS:
    case verFrance:
    case verGermany:
      outCharset.AssignLiteral("x-mac-roman");
      return NS_OK;
    case verJapan:
      outCharset.AssignLiteral("Shift_JIS");
      return NS_OK;
  }

  // ensure the .property file is loaded
  nsresult rv = InitInfo();
  NS_ENSURE_SUCCESS(rv, rv);

  // try mapping from region then from script
  nsAutoString key(NS_LITERAL_STRING("region."));
  key.AppendInt(region, 10);

  nsAutoString uCharset;
  rv = gInfo->Get(key, uCharset);
  if (NS_SUCCEEDED(rv))
    CopyUCS2toASCII(uCharset, outCharset);
  else {
    key.AssignLiteral("script.");
    key.AppendInt(script, 10);
    rv = gInfo->Get(key, uCharset);
    // not found in the .property file, assign x-mac-roman
    if (NS_SUCCEEDED(rv))
      CopyUCS2toASCII(uCharset, outCharset);
    else {
      outCharset.AssignLiteral("x-mac-roman");
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP 
nsPlatformCharset::GetCharset(nsPlatformCharsetSel selector, nsACString& oResult)
{
  nsresult rv;
  if (mCharset.IsEmpty()) {
    rv = MapToCharset(
           (short)(0x0000FFFF & ::GetScriptManagerVariable(smSysScript)),
           (short)(0x0000FFFF & ::GetScriptManagerVariable(smRegionCode)),
           mCharset);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  switch (selector) {
#ifdef XP_MACOSX  
    case kPlatformCharsetSel_FileName:
      oResult.AssignLiteral("UTF-8");
      break;
#endif
    case  kPlatformCharsetSel_KeyboardInput:
      rv = MapToCharset(
             (short) (0x0000FFFF & ::GetScriptManagerVariable(smKeyScript)),
             kTextRegionDontCare, oResult);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    default:
      oResult = mCharset;
      break;
  }

   return NS_OK;
}

NS_IMETHODIMP 
nsPlatformCharset::GetDefaultCharsetForLocale(const nsAString& localeName, nsACString &oResult)
{
  nsresult rv;
  nsCOMPtr<nsIMacLocale> pMacLocale;
  pMacLocale = do_CreateInstance(NS_MACLOCALE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    short script, language, region;
    rv = pMacLocale->GetPlatformLocale(localeName, &script, &language, &region);
    if (NS_SUCCEEDED(rv)) {
      if (NS_SUCCEEDED(MapToCharset(script, region, oResult))) {
        return NS_OK;
      }
    }
  }

  // fallback 
  oResult.AssignLiteral("x-mac-roman");
  return NS_SUCCESS_USING_FALLBACK_LOCALE;
}

NS_IMETHODIMP 
nsPlatformCharset::Init()
{
  return NS_OK;
}

nsresult 
nsPlatformCharset::MapToCharset(nsAString& inANSICodePage, nsACString& outCharset)
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
