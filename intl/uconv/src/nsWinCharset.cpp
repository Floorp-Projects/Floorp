/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsIPlatformCharset.h"
#include "nsURLProperties.h"
#include "pratom.h"
#include <windows.h>
#include "nsUConvDll.h"
#include "nsPlatformCharsetFactory.h"
#include "nsIWin32Locale.h"
#include "nsCOMPtr.h"
#include "nsLocaleCID.h"
#include "nsIComponentManager.h"

NS_DEFINE_IID(kIWin32LocaleIID, NS_IWIN32LOCALE_IID);
NS_DEFINE_CID(kWin32LocaleFactoryCID, NS_WIN32LOCALEFACTORY_CID);

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

NS_IMPL_ISUPPORTS(nsWinCharset, kIPlatformCharsetIID);

nsWinCharset::nsWinCharset()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
  nsAutoString propertyURL("resource:/res/wincharset.properties");

  nsURLProperties *info = new nsURLProperties( propertyURL );

  if( info ) 
  {
          UINT acp = ::GetACP();
          PRInt32 acpint = (PRInt32)(acp & 0x00FFFF);
          nsAutoString acpKey("acp.");
          acpKey.Append(acpint, 10);

          nsresult res = info->Get(acpKey, mCharset);
          if(NS_FAILED(res)) {
              mCharset = "windows-1252";
          }

          delete info;
  } else {
        mCharset = "windows-1252";
  }
}
nsWinCharset::~nsWinCharset()
{
  PR_AtomicDecrement(&g_InstanceCount);
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
	nsString					charset("windows-1252");
	nsString					localeAsNSString(localeName);

	//
	// convert locale name to a code page (through the LCID)
	//
	nsresult result = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID,nsnull,kIWin32LocaleIID,
												getter_AddRefs(winLocale));
	result = winLocale->GetPlatformLocale(&localeAsNSString,&localeAsLCID);
	if (NS_FAILED(result)) { *_retValue = charset.ToNewUnicode(); return result; }

	if (GetLocaleInfo(localeAsLCID,LOCALE_IDEFAULTANSICODEPAGE,acp_name,sizeof(acp_name))==0) { *_retValue = charset.ToNewUnicode(); return NS_ERROR_FAILURE; }

	//
	// load property file and convert from LCID->charset
	//

	nsAutoString property_url("resource:/res/wincharset.properties");
	nsURLProperties *charset_properties = new nsURLProperties(property_url);
	if (!charset_properties) { *_retValue = charset.ToNewUnicode(); return NS_ERROR_OUT_OF_MEMORY; }

     nsAutoString acp_key("acp.");
	 acp_key.Append(acp_name);
	 result = charset_properties->Get(acp_key,charset);
	
	 *_retValue = charset.ToNewUnicode();
	 return result;
}

class nsWinCharsetFactory : public nsIFactory {
   NS_DECL_ISUPPORTS

public:
   nsWinCharsetFactory() {
     NS_INIT_REFCNT();
     PR_AtomicIncrement(&g_InstanceCount);
   }
   virtual ~nsWinCharsetFactory() {
     PR_AtomicDecrement(&g_InstanceCount);
   }

   NS_IMETHOD CreateInstance(nsISupports* aDelegate, const nsIID& aIID, void** aResult);
   NS_IMETHOD LockFactory(PRBool aLock);
 
};

NS_DEFINE_IID( kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS( nsWinCharsetFactory , kIFactoryIID);

NS_IMETHODIMP nsWinCharsetFactory::CreateInstance(
    nsISupports* aDelegate, const nsIID &aIID, void** aResult)
{
  if(NULL == aResult) 
        return NS_ERROR_NULL_POINTER;
  if(NULL != aDelegate) 
        return NS_ERROR_NO_AGGREGATION;

  *aResult = NULL;
  nsISupports *inst = new nsWinCharset();
  if(NULL == inst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult res =inst->QueryInterface(aIID, aResult);
  if(NS_FAILED(res)) {
     delete inst;
  }
  
  return res;
}
NS_IMETHODIMP nsWinCharsetFactory::LockFactory(PRBool aLock)
{
  if(aLock)
     PR_AtomicIncrement( &g_LockCount );
  else
     PR_AtomicDecrement( &g_LockCount );
  return NS_OK;
}

nsIFactory* NEW_PLATFORMCHARSETFACTORY()
{
  return new nsWinCharsetFactory();
}

