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
#include "nsPlatformCharsetFactory.h"
#include "pratom.h"
#include "nsURLProperties.h"
#include <Script.h>
#include "nsUConvDll.h"



class nsMacCharset : public nsIPlatformCharset
{
  NS_DECL_ISUPPORTS

public:

  nsMacCharset();
  virtual ~nsMacCharset();

  NS_IMETHOD GetCharset(nsPlatformCharsetSel selector, nsString& oResult);
  NS_IMETHOD GetDefaultCharsetForLocale(const PRUnichar* localeName, PRUnichar** _retValue);

private:
  nsString mCharset;
};

NS_IMPL_ISUPPORTS(nsMacCharset, kIPlatformCharsetIID);

nsMacCharset::nsMacCharset()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
  nsAutoString propertyURL("resource:/res/maccharset.properties");

  nsURLProperties *info = new nsURLProperties( propertyURL );

  if( info ) { 
	  long ret = ::GetScriptManagerVariable(smRegionCode);
	  PRInt32 reg = (PRInt32)(ret & 0x00FFFF);
 	  nsAutoString regionKey("region.");
	  regionKey.Append(reg, 10);
	  
	  nsresult res = info->Get(regionKey, mCharset);
	  if(NS_FAILED(res)) {
		  ret = ::GetScriptManagerVariable(smSysScript);
		  PRInt32 script = (PRInt32)(ret & 0x00FFFF);
		  nsAutoString scriptKey("script.");
		  scriptKey.Append(script, 10);
	 	  nsresult res = info->Get(scriptKey, mCharset);
	      if(NS_FAILED(res)) {
	      	  mCharset = "x-mac-roman";
		  }   
	  }
	  
	  delete info;
  } else {
  	mCharset = "x-mac-roman";
  }
}
nsMacCharset::~nsMacCharset()
{
  PR_AtomicDecrement(&g_InstanceCount);
}

NS_IMETHODIMP 
nsMacCharset::GetCharset(nsPlatformCharsetSel selector, nsString& oResult)
{
   oResult = mCharset; 
   return NS_OK;
}

NS_IMETHODIMP 
nsMacCharset::GetDefaultCharsetForLocale(const PRUnichar* localeName, PRUnichar** _retValue)
{
	return NS_OK;
}

class nsMacCharsetFactory : public nsIFactory {
   NS_DECL_ISUPPORTS

public:
   nsMacCharsetFactory() {
     NS_INIT_REFCNT();
     PR_AtomicIncrement(&g_InstanceCount);
   }
   virtual ~nsMacCharsetFactory() {
     PR_AtomicDecrement(&g_InstanceCount);
   }

   NS_IMETHOD CreateInstance(nsISupports* aDelegate, const nsIID& aIID, void** aResult);
   NS_IMETHOD LockFactory(PRBool aLock);
 
};

NS_DEFINE_IID( kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS( nsMacCharsetFactory , kIFactoryIID);

NS_IMETHODIMP nsMacCharsetFactory::CreateInstance(
    nsISupports* aDelegate, const nsIID &aIID, void** aResult)
{
  if(NULL == aResult) 
        return NS_ERROR_NULL_POINTER;
  if(NULL != aDelegate) 
        return NS_ERROR_NO_AGGREGATION;

  *aResult = NULL;
  nsISupports *inst = new nsMacCharset();
  if(NULL == inst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult res =inst->QueryInterface(aIID, aResult);
  if(NS_FAILED(res)) {
     delete inst;
  }
  
  return res;
}
NS_IMETHODIMP nsMacCharsetFactory::LockFactory(PRBool aLock)
{
  if(aLock)
     PR_AtomicIncrement( &g_LockCount );
  else
     PR_AtomicDecrement( &g_LockCount );
  return NS_OK;
}

nsIFactory* NEW_PLATFORMCHARSETFACTORY()
{
  return new nsMacCharsetFactory();
}

