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
#include "pratom.h"
#include "nsURLProperties.h"
#include <Script.h>
#include "nsUConvDll.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIMacLocale.h"
#include "nsLocaleCID.h"
#include "nsReadableUtils.h"

static nsURLProperties *gInfo = nsnull;
static PRInt32 gCnt = 0;

class nsMacCharset : public nsIPlatformCharset
{
  NS_DECL_ISUPPORTS

public:

  nsMacCharset();
  virtual ~nsMacCharset();

  NS_IMETHOD GetCharset(nsPlatformCharsetSel selector, nsString& oResult);
  NS_IMETHOD GetDefaultCharsetForLocale(const PRUnichar* localeName, PRUnichar** _retValue);

private:
  nsresult InitInfo();
  nsresult MapToCharset(short script, short region, nsString& outCharset); 
  nsString mCharset;
};

NS_IMPL_ISUPPORTS1(nsMacCharset, nsIPlatformCharset);

nsMacCharset::nsMacCharset()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&gCnt);
}
nsMacCharset::~nsMacCharset()
{
  PR_AtomicDecrement(&gCnt);
  if((0 == gCnt) && (nsnull != gInfo)) {
  	delete gInfo;
  	gInfo = nsnull;
  }
}

nsresult nsMacCharset::InitInfo()
{  
  // load the .property file if necessary
  if (gInfo == nsnull) {
  
    nsAutoString propertyURL(NS_LITERAL_STRING("resource:/res/maccharset.properties"));

    nsURLProperties *info = new nsURLProperties( propertyURL );
    NS_ASSERTION(info , "cannot open properties file");
    NS_ENSURE_TRUE(info, NS_ERROR_FAILURE);
    gInfo = info;
  }

  return NS_OK;
}

nsresult nsMacCharset::MapToCharset(short script, short region, nsString& outCharset)
{
  switch (region) {
    case verUS:
    case verFrance:
    case verGermany:
      outCharset.Assign(NS_LITERAL_STRING("x-mac-roman"));
      return NS_OK;
    case verJapan:
      outCharset.Assign(NS_LITERAL_STRING("Shift_JIS"));
      return NS_OK;
  }

  // ensure the .property file is loaded
  nsresult rv = InitInfo();
  NS_ENSURE_SUCCESS(rv, rv);

  // try mapping from region then from script
  nsAutoString key(NS_LITERAL_STRING("region."));
  key.AppendInt(region, 10);

  rv = gInfo->Get(key, outCharset);
  if (NS_FAILED(rv)) {
    key.AssignWithConversion("script.");
    key.AppendInt(script, 10);
    rv = gInfo->Get(key, outCharset);
    // not found in the .property file, assign x-mac-roman
    if (NS_FAILED(rv)) { 
      outCharset.AssignWithConversion("x-mac-roman");
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP 
nsMacCharset::GetCharset(nsPlatformCharsetSel selector, nsString& oResult)
{
  if (mCharset.IsEmpty()) {
    nsresult rv = MapToCharset((short)(0x0000FFFF & ::GetScriptManagerVariable(smSysScript)), 
                               (short)(0x0000FFFF & ::GetScriptManagerVariable(smRegionCode)), 
                               mCharset);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
   oResult = mCharset; 
   return NS_OK;
}

NS_IMETHODIMP 
nsMacCharset::GetDefaultCharsetForLocale(const PRUnichar* localeName, PRUnichar** _retValue)
{
  nsCOMPtr<nsIMacLocale>	pMacLocale;
  nsAutoString localeAsString(localeName), charset(NS_LITERAL_STRING("x-mac-roman"));
  short script, language, region;
	
  nsresult rv;
  pMacLocale = do_CreateInstance(NS_MACLOCALE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = pMacLocale->GetPlatformLocale(&localeAsString, &script, &language, &region);
    if (NS_SUCCEEDED(rv)) {
      rv = MapToCharset(script, region, charset);
    }
  }
  *_retValue = ToNewUnicode(charset);
  NS_ENSURE_TRUE(*_retValue, NS_ERROR_OUT_OF_MEMORY);
  
  return rv;
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
  nsMacCharset* inst = new nsMacCharset();
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
