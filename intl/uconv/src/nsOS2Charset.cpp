/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   John Fairhurst
 *   Henry Sobotka
 *   IBM Corp.
 */
#include "nsIPlatformCharset.h"
#include "nsURLProperties.h"
#include "pratom.h"
#define INCL_WIN
#include <os2.h>
#include "nsUConvDll.h"
#include "nsIOS2Locale.h"
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

  UINT acp = ::WinQueryCp(HMQ_CURRENT);
  PRInt32 acpint = (PRInt32)(acp & 0x00FFFF);
  nsAutoString acpKey(NS_LITERAL_STRING("os2."));
  acpKey.AppendInt(acpint, 10);
  nsresult res = MapToCharset(acpKey, mCharset);

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
    nsURLProperties *info = new nsURLProperties(NS_LITERAL_CSTRING("resource://gre/res/os2charset.properties"));

    NS_ASSERTION(info , "cannot open properties file");
    NS_ENSURE_TRUE(info, NS_ERROR_FAILURE);
    gInfo = info;
  }
  return NS_OK;
}

nsresult
nsPlatformCharset::MapToCharset(nsAString& inANSICodePage, nsACString& outCharset)
{
  //delay loading os2charset.properties bundle if possible
  if (inANSICodePage.Equals(NS_LITERAL_STRING("os2.850"))) {
    outCharset = NS_LITERAL_CSTRING("IBM850");
    return NS_OK;
  } 

  if (inANSICodePage.Equals(NS_LITERAL_STRING("os2.932"))) {
    outCharset = NS_LITERAL_CSTRING("Shift_JIS");
    return NS_OK;
  } 

  // ensure the .property file is loaded
  nsresult rv = InitInfo();
  if (NS_FAILED(rv)) {
    outCharset.Assign(NS_LITERAL_CSTRING("IBM850"));
    return rv;
  }

  nsAutoString charset;
  rv = gInfo->Get(inANSICodePage, charset);
  if (NS_FAILED(rv)) {
    outCharset.Assign(NS_LITERAL_CSTRING("IBM850"));
    return rv;
  }

  LossyCopyUTF16toASCII(charset, outCharset);
  return NS_OK;
}

NS_IMETHODIMP 
nsPlatformCharset::GetCharset(nsPlatformCharsetSel selector,
                              nsACString& oResult)
{
  if ((selector == kPlatformCharsetSel_4xBookmarkFile) || (selector == kPlatformCharsetSel_4xPrefsJS)) {
    if ((mCharset.Find("IBM850", IGNORE_CASE) != -1) || (mCharset.Find("IBM437", IGNORE_CASE) != -1)) 
      oResult.Assign(NS_LITERAL_CSTRING("ISO-8859-1"));
    else if (mCharset.Find("IBM852", IGNORE_CASE) != -1)
      oResult.Assign(NS_LITERAL_CSTRING("windows-1250"));
    else if ((mCharset.Find("IBM855", IGNORE_CASE) != -1) || (mCharset.Find("IBM866", IGNORE_CASE) != -1))
      oResult.Assign(NS_LITERAL_CSTRING("windows-1251"));
    else if ((mCharset.Find("IBM869", IGNORE_CASE) != -1) || (mCharset.Find("IBM813", IGNORE_CASE) != -1))
      oResult.Assign(NS_LITERAL_CSTRING("windows-1253"));
    else if (mCharset.Find("IBM857", IGNORE_CASE) != -1)
      oResult.Assign(NS_LITERAL_CSTRING("windows-1254"));
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
  oResult.Truncate();
  return NS_OK;
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
