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

#include <locale.h>
#include "nsIPlatformCharset.h"
#include "nsPlatformCharsetFactory.h"
#include "pratom.h"
#include "nsURLProperties.h"
#include "nsCOMPtr.h"
#include "nsIPosixLocale.h"
#include "nsLocaleCID.h"
#include "nsUConvDll.h"
#include "nsIComponentManager.h"

NS_DEFINE_IID(kIPosixLocaleIID,NS_IPOSIXLOCALE_IID);
NS_DEFINE_CID(kPosixLocaleFactoryCID,NS_POSIXLOCALEFACTORY_CID);

class nsUNIXCharset : public nsIPlatformCharset
{
  NS_DECL_ISUPPORTS

public:

  nsUNIXCharset();
  virtual ~nsUNIXCharset();

  NS_IMETHOD GetCharset(nsPlatformCharsetSel selector, nsString& oResult);
  NS_IMETHOD GetDefaultCharsetForLocale(const PRUnichar* localeName, PRUnichar** _retValue);

private:
  nsString mCharset;
};

NS_IMPL_ISUPPORTS(nsUNIXCharset, kIPlatformCharsetIID);

nsUNIXCharset::nsUNIXCharset()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);

  char* locale = setlocale(LC_CTYPE, NULL);
  if(locale) 
  {
      nsAutoString propertyURL("resource:/res/unixcharset.properties");
  
      nsURLProperties *info = new nsURLProperties( propertyURL );
      if( info )
      {
          nsAutoString platformLocaleKey("locale." OSTYPE ".");
          platformLocaleKey.Append(locale);

          nsresult res = info->Get(platformLocaleKey, mCharset);
          if(NS_FAILED(res)) {
              nsAutoString localeKey("locale.all.");
              localeKey.Append(locale);
              res = info->Get(localeKey, mCharset);
              if(NS_SUCCEEDED(res))  {
                  delete info;
                  return; // succeeded
              }
          }

          delete info;
      } 
   }
   mCharset = "ISO-8859-1";
   return; // failed
}
nsUNIXCharset::~nsUNIXCharset()
{
  PR_AtomicDecrement(&g_InstanceCount);
}

NS_IMETHODIMP 
nsUNIXCharset::GetCharset(nsPlatformCharsetSel selector, nsString& oResult)
{
   oResult = mCharset; 
   return NS_OK;
}

NS_IMETHODIMP 
nsUNIXCharset::GetDefaultCharsetForLocale(const PRUnichar* localeName, PRUnichar** _retValue)
{
  nsCOMPtr<nsIPosixLocale> pPosixLocale;
  nsString charset("ISO-8859-1"), localeNameAsString(localeName);
  char posix_locale[9];

  //
  // convert the locale name
  //
  nsresult rv = nsComponentManager::CreateInstance(kPosixLocaleFactoryCID,nsnull,
                                                   kIPosixLocaleIID,
                                                   getter_AddRefs(pPosixLocale));
  if (NS_FAILED(rv)) { *_retValue = charset.ToNewUnicode(); return rv; }

  rv = pPosixLocale->GetPlatformLocale(&localeNameAsString,posix_locale,sizeof(posix_locale));
  if (NS_FAILED(rv)) { *_retValue = charset.ToNewUnicode(); return rv; }

  //
  // convert from locale to charset
  //
  nsAutoString property_url("resource:/res/unixcharset.properties"); 
  nsURLProperties *charset_properties = new nsURLProperties(property_url);
  
  if (!charset_properties) { *_retValue=charset.ToNewUnicode(); return NS_ERROR_OUT_OF_MEMORY; }


  nsAutoString locale_key("locale." OSTYPE "."); 
  locale_key.Append(posix_locale); 
 
  rv = charset_properties->Get(locale_key,charset);
  if(NS_FAILED(rv)) { 
    locale_key="locale.all."; 
    locale_key.Append(posix_locale); 
    rv = charset_properties->Get(locale_key,charset); 
    if(NS_FAILED(rv)) { charset="ISO-8859-1";}
  }

  delete charset_properties;
  *_retValue = charset.ToNewUnicode();
	return rv;

}

class nsUNIXCharsetFactory : public nsIFactory {
   NS_DECL_ISUPPORTS

public:
   nsUNIXCharsetFactory() {
     NS_INIT_REFCNT();
     PR_AtomicIncrement(&g_InstanceCount);
   }
   virtual ~nsUNIXCharsetFactory() {
     PR_AtomicDecrement(&g_InstanceCount);
   }

   NS_IMETHOD CreateInstance(nsISupports* aDelegate, const nsIID& aIID, void** aResult);
   NS_IMETHOD LockFactory(PRBool aLock);
 
};

NS_DEFINE_IID( kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS( nsUNIXCharsetFactory , kIFactoryIID);

NS_IMETHODIMP nsUNIXCharsetFactory::CreateInstance(
    nsISupports* aDelegate, const nsIID &aIID, void** aResult)
{
  if(NULL == aResult) 
        return NS_ERROR_NULL_POINTER;
  if(NULL != aDelegate) 
        return NS_ERROR_NO_AGGREGATION;

  *aResult = NULL;
  nsISupports *inst = new nsUNIXCharset();
  if(NULL == inst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult res =inst->QueryInterface(aIID, aResult);
  if(NS_FAILED(res)) {
     delete inst;
  }
  
  return res;
}
NS_IMETHODIMP nsUNIXCharsetFactory::LockFactory(PRBool aLock)
{
  if(aLock)
     PR_AtomicIncrement( &g_LockCount );
  else
     PR_AtomicDecrement( &g_LockCount );
  return NS_OK;
}

nsIFactory* NEW_PLATFORMCHARSETFACTORY()
{
  return new nsUNIXCharsetFactory();
}

