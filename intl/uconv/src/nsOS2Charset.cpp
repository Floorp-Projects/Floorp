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
#define INCL_PM
#include <os2.h>
#include "nsUConvDll.h"
#include "nsIOS2Locale.h"
#include "nsCOMPtr.h"
#include "nsLocaleCID.h"
#include "nsIComponentManager.h"
#include "nsPlatformCharset.h"

static nsURLProperties *gInfo = nsnull;
static PRInt32 gCnt= 0;

NS_IMPL_ISUPPORTS1(nsPlatformCharset, nsIPlatformCharset);

nsPlatformCharset::nsPlatformCharset()
{
  PR_AtomicIncrement(&gCnt); // count for gInfo

  // XXX We should make the following block critical section
  if(nsnull == gInfo)
  {
    nsURLProperties *info = new nsURLProperties(NS_LITERAL_CSTRING("resource:/res/os2charset.properties"));
    NS_ASSERTION( info , " cannot create nsURLProperties");
    gInfo = info;
  }
  NS_ASSERTION(gInfo, "Cannot open property file");
  if( gInfo ) 
  {
    UINT acp = ::WinQueryCp(HMQ_CURRENT);
    PRInt32 acpint = (PRInt32)(acp & 0x00FFFF);
    nsAutoString acpKey(NS_LITERAL_STRING("os2."));
    acpKey.AppendInt(acpint, 10);

    nsresult res = gInfo->Get(acpKey, mCharset);
    if(NS_FAILED(res)) {
      mCharset.Assign(NS_LITERAL_STRING("IBM850"));
    }

  } else {
    mCharset.Assign(NS_LITERAL_STRING("IBM850"));
  }
}

nsPlatformCharset::~nsPlatformCharset()
{
  PR_AtomicDecrement(&gCnt);
  if(0 == gCnt) {
    delete gInfo;
    gInfo = nsnull;
  }
}

NS_IMETHODIMP
nsPlatformCharset::GetCharset(nsPlatformCharsetSel selector, nsAString& oResult)
{
  if ((selector == kPlatformCharsetSel_4xBookmarkFile) || (selector == kPlatformCharsetSel_4xPrefsJS)) {
    if ((mCharset.Find("IBM850", IGNORE_CASE) != -1) || (mCharset.Find("IBM437", IGNORE_CASE) != -1)) 
      oResult.Assign(NS_LITERAL_STRING("ISO-8859-1"));
    else if (mCharset.Find("IBM852", IGNORE_CASE) != -1)
      oResult.Assign(NS_LITERAL_STRING("windows-1250"));
    else if ((mCharset.Find("IBM855", IGNORE_CASE) != -1) || (mCharset.Find("IBM866", IGNORE_CASE) != -1))
      oResult.Assign(NS_LITERAL_STRING("windows-1251"));
    else if ((mCharset.Find("IBM869", IGNORE_CASE) != -1) || (mCharset.Find("IBM813", IGNORE_CASE) != -1))
      oResult.Assign(NS_LITERAL_STRING("windows-1253"));
    else if (mCharset.Find("IBM857", IGNORE_CASE) != -1)
      oResult.Assign(NS_LITERAL_STRING("windows-1254"));
    else
      oResult = mCharset;
  } else {
    oResult = mCharset;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPlatformCharset::GetDefaultCharsetForLocale(const PRUnichar* localeName, PRUnichar** _retValue)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsPlatformCharset::Init()
{
  return NS_OK;
}
nsresult 
nsPlatformCharset::MapToCharset(short script, short region, nsAString& outCharset)
{
  return NS_OK;
}

nsresult 
nsPlatformCharset::MapToCharset(nsAString& inANSICodePage, nsAString& outCharset)
{
  return NS_OK;
}

nsresult
nsPlatformCharset::InitGetCharset(nsAString &oString)
{
  return NS_OK;
}

nsresult
nsPlatformCharset::ConvertLocaleToCharsetUsingDeprecatedConfig(nsAutoString& locale, nsAString& oResult)
{
  return NS_OK;
}

nsresult
nsPlatformCharset::VerifyCharset(nsString &aCharset)
{
  return NS_OK;
}

nsresult 
nsPlatformCharset::InitInfo()
{  
  return NS_OK;
}
