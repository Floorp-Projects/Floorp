/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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

static nsURLProperties *gInfo = nsnull;
static PRInt32 gCnt= 0;

class nsWinCharset : public nsIPlatformCharset
{
  NS_DECL_ISUPPORTS

public:

  nsWinCharset();
  virtual ~nsWinCharset();

  NS_IMETHOD GetCharset(nsPlatformCharsetSel selector, nsString& oResult);
  NS_IMETHOD GetDefaultCharsetForLocale(const PRUnichar* localeName, PRUnichar** _retValue);

private:

  nsString mCharset;

private:

  nsresult InitInfo();
  nsresult MapToCharset(nsString& inANSICodePage, nsString& outCharset);
};

NS_IMPL_ISUPPORTS1(nsWinCharset, nsIPlatformCharset)

nsWinCharset::nsWinCharset()
{
  NS_TIMELINE_START_TIMER("nsWinCharset()");
  NS_INIT_REFCNT();

          UINT acp = ::GetACP();
          PRInt32 acpint = (PRInt32)(acp & 0x00FFFF);
          nsAutoString acpKey; acpKey.AssignWithConversion("acp.");
          acpKey.AppendInt(acpint, 10);
  nsresult res = MapToCharset(acpKey, mCharset);

  NS_TIMELINE_STOP_TIMER("nsWinCharset()");
  NS_TIMELINE_MARK_TIMER("nsWinCharset()");
          }
nsWinCharset::~nsWinCharset()
{
  PR_AtomicDecrement(&gCnt);
  if ((0 == gCnt) && (nsnull != gInfo)) {
     delete gInfo;
     gInfo = nsnull;
  }
}

nsresult 
nsWinCharset::InitInfo()
{  
  PR_AtomicIncrement(&gCnt); // count for gInfo

  if (gInfo == nsnull) {
    nsURLProperties *info = new nsURLProperties(nsAutoString(NS_LITERAL_STRING("resource:/res/wincharset.properties")));

    NS_ASSERTION(info , "cannot open properties file");
    NS_ENSURE_TRUE(info, NS_ERROR_FAILURE);
    gInfo = info;
  }
  return NS_OK;
}

nsresult
nsWinCharset::MapToCharset(nsString& inANSICodePage, nsString& outCharset)
{
  //delay loading wincharset.properties bundle if possible
  if (inANSICodePage.Equals(NS_LITERAL_STRING("acp.1252"))) {
    outCharset = NS_LITERAL_STRING("windows-1252");
    return NS_OK;
  } 

  if (inANSICodePage.Equals(NS_LITERAL_STRING("acp.932"))) {
    outCharset = NS_LITERAL_STRING("Shift_JIS");
    return NS_OK;
  } 

  // ensure the .property file is loaded
  nsresult rv = InitInfo();
  if (NS_FAILED(rv)) {
    outCharset.Assign(NS_LITERAL_STRING("windows-1252"));
    return rv;
  }

  rv = gInfo->Get(inANSICodePage, outCharset);
  if (NS_FAILED(rv)) {
    outCharset.Assign(NS_LITERAL_STRING("windows-1252"));
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsWinCharset::GetCharset(nsPlatformCharsetSel selector, nsString& oResult)
{
   oResult = mCharset;
   return NS_OK;
}

NS_IMETHODIMP
nsWinCharset::GetDefaultCharsetForLocale(const PRUnichar* localeName, PRUnichar** _retValue)
{
	nsCOMPtr<nsIWin32Locale>	winLocale;
	LCID						localeAsLCID;
	char						acp_name[6];
	nsAutoString    charset;
	nsAutoString    localeAsNSString(localeName);

	//
	// convert locale name to a code page (through the LCID)
	//
	nsresult result;
  winLocale = do_CreateInstance(NS_WIN32LOCALE_CONTRACTID, &result);
	result = winLocale->GetPlatformLocale(&localeAsNSString,&localeAsLCID);

	if (NS_FAILED(result)) { *_retValue = ToNewUnicode(charset); return result; }

	if (GetLocaleInfo(localeAsLCID,LOCALE_IDEFAULTANSICODEPAGE,acp_name,sizeof(acp_name))==0) { *_retValue = ToNewUnicode(charset); return NS_ERROR_FAILURE; }
     nsAutoString acp_key; acp_key.AssignWithConversion("acp.");
	 acp_key.AppendWithConversion(acp_name);

	result = MapToCharset(acp_key,charset);
	
	 *_retValue = ToNewUnicode(charset);
	 return result;
}

//----------------------------------------------------------------------

NS_IMETHODIMP
NS_NewPlatformCharset(nsISupports* aOuter, 
                      const nsIID &aIID,
                      void **aResult)
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aOuter) {
    *aResult = nsnull;
    return NS_ERROR_NO_AGGREGATION;
  }
  nsWinCharset* inst = new nsWinCharset();
  if (!inst) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult res = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(res)) {
    *aResult = nsnull;
    delete inst;
  }
  return res;
}
