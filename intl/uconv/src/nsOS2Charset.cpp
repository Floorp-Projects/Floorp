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
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 06/01/2000       IBM Corp.       Fixed querying of locale charset
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

static nsURLProperties *gInfo = nsnull;
static PRInt32 gCnt=0;

nsOS2Charset::nsOS2Charset()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
  PR_AtomicIncrement(&gCnt);

  // XXX we should make the following block critical section
  if(nsnull == gInfo)
  {
      nsAutoString propertyURL;
      propertyURL.AssignWithConversion("resource:/res/os2charset.properties");
      nsURLProperties *info = new nsURLProperties( propertyURL );
      NS_ASSERTION( info, "cannot create nsURLProperties");
      gInfo = info;
  }

  // Probably ought to go via the localefactory, but hey...
  LocaleObject locale_object = 0;
  UniCreateLocaleObject( UNI_UCS_STRING_POINTER,
                         (UniChar*)L"", &locale_object);

  if(gInfo && locale_object)
  {
      UniChar *pString = 0;

      nsAutoString platformLocaleKey;
      platformLocaleKey.AssignWithConversion("locale." OSTYPE ".");
      UniQueryLocaleItem( locale_object, LocaleItem(100), &pString); // locale name (e.g. "en_US")
      platformLocaleKey.Append((PRUnichar *)pString);
      UniFreeMem( pString);
      platformLocaleKey.AppendWithConversion(".");
      UniQueryLocaleItem( locale_object, LocaleItem(109), &pString); // ULS codepage (e.g. "iso8859-1")
      platformLocaleKey.Append((PRUnichar *)pString);
      UniFreeMem( pString);

      nsresult res = gInfo->Get(platformLocaleKey, mCharset);
      if(NS_FAILED(res))
      {
         nsAutoString localeKey;
         localeKey.AssignWithConversion("locale.all.");
         UniQueryLocaleItem( locale_object, LocaleItem(100), &pString); // locale name (e.g. "en_US")
         localeKey.Append((PRUnichar *)pString);
         UniFreeMem( pString);
         localeKey.AppendWithConversion(".");
         UniQueryLocaleItem( locale_object, LocaleItem(109), &pString); // ULS codepage (e.g. "iso8859-1")
         localeKey.Append((PRUnichar *)pString);
         UniFreeMem( pString);
         res = gInfo->Get(localeKey, mCharset);
      }

      UniFreeLocaleObject( locale_object);

      if(NS_SUCCEEDED(res))
      {
         return; // succeeded
      }
  }
  mCharset.AssignWithConversion("ISO-8859-1");
  return; // failed
}

nsOS2Charset::~nsOS2Charset()
{
  PR_AtomicDecrement(&g_InstanceCount);
  PR_AtomicDecrement(&gCnt);
  if(0 == gCnt) {
     delete gInfo;
     gInfo = nsnull;
  }
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
  // OS2TODO
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
