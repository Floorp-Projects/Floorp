/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsIPlatformCharset.h"
#include "nsURLProperties.h"
#include "pratom.h"
#include <windows.h>
#include "nsUConvDll.h"
#include "nsIWin32Locale.h"
#include "nsCOMPtr.h"
#include "nsLocaleCID.h"
#include "nsIComponentManager.h"

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
};

NS_IMPL_ISUPPORTS1(nsWinCharset, nsIPlatformCharset)

nsWinCharset::nsWinCharset()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&gCnt); // count for gInfo

  // XXX We should make the following block critical section
  if(nsnull == gInfo)
  {
     nsAutoString propertyURL; propertyURL.AssignWithConversion("resource:/res/wincharset.properties");

     nsURLProperties *info = new nsURLProperties( propertyURL );
     NS_ASSERTION( info , " cannot create nsURLProperties");
     gInfo = info;
  }
  NS_ASSERTION(gInfo, "Cannot open property file");
  if( gInfo ) 
  {
          UINT acp = ::GetACP();
          PRInt32 acpint = (PRInt32)(acp & 0x00FFFF);
          nsAutoString acpKey; acpKey.AssignWithConversion("acp.");
          acpKey.AppendInt(acpint, 10);

          nsresult res = gInfo->Get(acpKey, mCharset);
          if(NS_FAILED(res)) {
              mCharset.AssignWithConversion("windows-1252");
          }

  } else {
        mCharset.AssignWithConversion("windows-1252");
  }
}
nsWinCharset::~nsWinCharset()
{
  PR_AtomicDecrement(&gCnt);
  if(0 == gCnt) {
     delete gInfo;
     gInfo = nsnull;
  }
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
	nsString					charset;charset.AssignWithConversion("windows-1252");
	nsString					localeAsNSString(localeName);

	//
	// convert locale name to a code page (through the LCID)
	//
	nsresult result;
  winLocale = do_CreateInstance(NS_WIN32LOCALE_CONTRACTID, &result);
	result = winLocale->GetPlatformLocale(&localeAsNSString,&localeAsLCID);
	if (NS_FAILED(result)) { *_retValue = charset.ToNewUnicode(); return result; }

	if (GetLocaleInfo(localeAsLCID,LOCALE_IDEFAULTANSICODEPAGE,acp_name,sizeof(acp_name))==0) { *_retValue = charset.ToNewUnicode(); return NS_ERROR_FAILURE; }

	//
	// load property file and convert from LCID->charset
	//
	if (!gInfo) { *_retValue = charset.ToNewUnicode(); return NS_ERROR_OUT_OF_MEMORY; }

     nsAutoString acp_key; acp_key.AssignWithConversion("acp.");
	 acp_key.AppendWithConversion(acp_name);
	 result = gInfo->Get(acp_key,charset);
	
	 *_retValue = charset.ToNewUnicode();
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
