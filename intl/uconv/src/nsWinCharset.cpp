
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

#include "nsIPlatformCharset.h"
#include "nsURLProperties.h"
#include "pratom.h"
#include <windows.h>
#include "nsUConvDll.h"
#include "nsIWin32Locale.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsLocaleCID.h"
#include "nsIComponentManager.h"
#include "nsITimelineService.h"
#include "nsPlatformCharset.h"

static nsURLProperties *gInfo = nsnull;
static PRInt32 gCnt= 0;

NS_IMPL_ISUPPORTS1(nsPlatformCharset, nsIPlatformCharset)

nsPlatformCharset::nsPlatformCharset()
{
  NS_TIMELINE_START_TIMER("nsPlatformCharset()");

  nsAutoString acpKey(NS_LITERAL_STRING("acp."));
  acpKey.AppendInt(PRInt32(::GetACP() & 0x00FFFF), 10);
  MapToCharset(acpKey, mCharset);

  NS_TIMELINE_STOP_TIMER("nsPlatformCharset()");
  NS_TIMELINE_MARK_TIMER("nsPlatformCharset()");
}

nsPlatformCharset::~nsPlatformCharset()
{
  PR_AtomicDecrement(&gCnt);
  if ((0 == gCnt) && (nsnull != gInfo)) {
    delete gInfo;
    gInfo = nsnull;
  }
}

nsresult 
nsPlatformCharset::InitInfo()
{  
  PR_AtomicIncrement(&gCnt); // count for gInfo

  if (gInfo == nsnull) {
    nsURLProperties *info = new nsURLProperties(NS_LITERAL_CSTRING("resource://gre/res/wincharset.properties"));

    NS_ASSERTION(info , "cannot open properties file");
    NS_ENSURE_TRUE(info, NS_ERROR_FAILURE);
    gInfo = info;
  }
  return NS_OK;
}

nsresult
nsPlatformCharset::MapToCharset(nsAString& inANSICodePage, nsACString& outCharset)
{
  //delay loading wincharset.properties bundle if possible
  if (inANSICodePage.EqualsLiteral("acp.1252")) {
    outCharset = NS_LITERAL_CSTRING("windows-1252");
    return NS_OK;
  } 

  if (inANSICodePage.EqualsLiteral("acp.932")) {
    outCharset = NS_LITERAL_CSTRING("Shift_JIS");
    return NS_OK;
  } 

  // ensure the .property file is loaded
  nsresult rv = InitInfo();
  if (NS_FAILED(rv)) {
    outCharset.Assign(NS_LITERAL_CSTRING("windows-1252"));
    return rv;
  }

  nsAutoString charset;
  rv = gInfo->Get(inANSICodePage, charset);
  if (NS_FAILED(rv)) {
    outCharset.Assign(NS_LITERAL_CSTRING("windows-1252"));
    return rv;
  }

  CopyUCS2toASCII(charset, outCharset);
  return NS_OK;
}

NS_IMETHODIMP 
nsPlatformCharset::GetCharset(nsPlatformCharsetSel selector,
                              nsACString& oResult)
{
  oResult = mCharset;
  return NS_OK;
}

NS_IMETHODIMP
nsPlatformCharset::GetDefaultCharsetForLocale(const nsAString& localeName, nsACString& oResult)
{
  nsCOMPtr<nsIWin32Locale>	winLocale;
  LCID						localeAsLCID;
  char						acp_name[6];

  //
  // convert locale name to a code page (through the LCID)
  //
  nsresult rv;
  oResult.Truncate();

  winLocale = do_CreateInstance(NS_WIN32LOCALE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) { return rv; }

  rv = winLocale->GetPlatformLocale(localeName, &localeAsLCID);
  if (NS_FAILED(rv)) { return rv; }

  if (GetLocaleInfo(localeAsLCID, LOCALE_IDEFAULTANSICODEPAGE, acp_name, sizeof(acp_name))==0) { 
    return NS_ERROR_FAILURE; 
  }
  nsAutoString acp_key(NS_LITERAL_STRING("acp."));
  acp_key.AppendWithConversion(acp_name);

  return MapToCharset(acp_key, oResult);
}

NS_IMETHODIMP 
nsPlatformCharset::Init()
{
  return NS_OK;
}

nsresult 
nsPlatformCharset::MapToCharset(short script, short region, nsACString& outCharset)
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
