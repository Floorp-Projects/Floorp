/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s):  Henry Sobotka <sobotka@axess.com>
 *                  00/01: general review and update against Win/Unix versions
 *
 */

#include "nsIPlatformCharset.h"
#include "pratom.h"
#include "nsURLProperties.h"
#include "nsCOMPtr.h"
#include "nsIOS2Locale.h"
#include "nsLocaleCID.h"
#include "nsUConvDll.h"
#include "nsIComponentManager.h"
#include <unidef.h>
#include <ulsitem.h>

NS_DEFINE_IID(kIOS2LocaleIID,NS_IOS2LOCALE_IID);
NS_DEFINE_CID(kOS2LocaleFactoryCID,NS_OS2LOCALEFACTORY_CID);

// 90% copied from the unix version 

class nsOS2Charset : public nsIPlatformCharset
{
  NS_DECL_ISUPPORTS

public:

  nsOS2Charset();
  virtual ~nsOS2Charset();

  NS_IMETHOD GetCharset( nsPlatformCharsetSel selector, nsString& oResult);
  NS_IMETHOD GetDefaultCharsetForLocale(const PRUnichar* localeName, PRUnichar** _retValue);

private:
  nsString mCharset;
};

NS_IMPL_ISUPPORTS(nsOS2Charset, kIPlatformCharsetIID);

nsOS2Charset::nsOS2Charset()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement( &g_InstanceCount);

  // Probably ought to go via the localefactory, but hey...
  LocaleObject locale_object = 0;
  UniCreateLocaleObject( UNI_UCS_STRING_POINTER,
                         (UniChar*)L"", &locale_object);

  UniChar *pString = 0;                          
  UniQueryLocaleItem( locale_object, (LocaleItem)109, &pString);

  // This is something like ISO8859-1
  mCharset.SetString( (PRUnichar*) pString);

  UniFreeMem( pString);
  UniFreeLocaleObject( locale_object);
}

nsOS2Charset::~nsOS2Charset()
{
  PR_AtomicDecrement(&g_InstanceCount);
}

NS_IMETHODIMP 
nsOS2Charset::GetCharset(nsPlatformCharsetSel selector, nsString& oResult)
{
   oResult = mCharset; 
   return NS_OK;
}

// XXXX STUB
NS_IMETHODIMP 
nsOS2Charset::GetDefaultCharsetForLocale(const PRUnichar* localeName, PRUnichar** _retValue)
{
  return NS_OK;
}

class nsOS2CharsetFactory : public nsIFactory {
   NS_DECL_ISUPPORTS

public:
   nsOS2CharsetFactory() {
     NS_INIT_REFCNT();
     PR_AtomicIncrement(&g_InstanceCount);
   }
   virtual ~nsOS2CharsetFactory() {
     PR_AtomicDecrement(&g_InstanceCount);
   }

   NS_IMETHOD CreateInstance(nsISupports* aDelegate, const nsIID& aIID, void** aResult);
   NS_IMETHOD LockFactory(PRBool aLock);
 
};

NS_DEFINE_IID( kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS( nsOS2CharsetFactory , kIFactoryIID);

NS_IMETHODIMP nsOS2CharsetFactory::CreateInstance(
    nsISupports* aDelegate, const nsIID &aIID, void** aResult)
{
  if( !aResult) 
        return NS_ERROR_NULL_POINTER;
  if( aDelegate) 
        return NS_ERROR_NO_AGGREGATION;

  *aResult = NULL;
  nsISupports *inst = new nsOS2Charset;
  if( !inst)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult res =inst->QueryInterface(aIID, aResult);
  if(NS_FAILED(res))
     delete inst;
  
  return res;
}
NS_IMETHODIMP nsOS2CharsetFactory::LockFactory(PRBool aLock)
{
  if(aLock)
     PR_AtomicIncrement( &g_LockCount );
  else
     PR_AtomicDecrement( &g_LockCount );
  return NS_OK;
}

nsIFactory* NEW_PLATFORMCHARSETFACTORY()
{
  return new nsOS2CharsetFactory();
}

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
  nsOS2Charset* inst = new nsOS2Charset();
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
