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

NS_DEFINE_IID(kIOS2LocaleIID,NS_IOS2LOCALE_IID);
NS_DEFINE_CID(kOS2LocaleFactoryCID,NS_OS2LOCALEFACTORY_CID);

static nsURLProperties *gInfo = nsnull;
static PRInt32 gCnt= 0;

class nsOS2Charset : public nsIPlatformCharset
{
  NS_DECL_ISUPPORTS

public:

  nsOS2Charset();
  virtual ~nsOS2Charset();

  NS_IMETHOD GetCharset(nsPlatformCharsetSel selector, nsString& oResult);
  NS_IMETHOD GetDefaultCharsetForLocale(const PRUnichar* localeName, PRUnichar** _retValue);
private:
  nsString mCharset;
};

NS_IMPL_ISUPPORTS1(nsOS2Charset, nsIPlatformCharset);

nsOS2Charset::nsOS2Charset()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
  PR_AtomicIncrement(&gCnt); // count for gInfo

  // XXX We should make the following block critical section
  if(nsnull == gInfo)
  {
     nsAutoString propertyURL; propertyURL.AssignWithConversion("resource:/res/os2charset.properties");

     nsURLProperties *info = new nsURLProperties( propertyURL );
     NS_ASSERTION( info , " cannot create nsURLProperties");
     gInfo = info;
  }
  NS_ASSERTION(gInfo, "Cannot open property file");
  if( gInfo ) 
  {
    UINT acp = ::WinQueryCp(HMQ_CURRENT);
    PRInt32 acpint = (PRInt32)(acp & 0x00FFFF);
    nsAutoString acpKey; acpKey.AssignWithConversion("os2.");
    acpKey.AppendInt(acpint, 10);

    nsresult res = gInfo->Get(acpKey, mCharset);
    if(NS_FAILED(res)) {
      mCharset.AssignWithConversion("IBM850");
    }

  } else {
    mCharset.AssignWithConversion("IBM850");
  }
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

NS_IMETHODIMP
nsOS2Charset::GetDefaultCharsetForLocale(const PRUnichar* localeName, PRUnichar** _retValue)
{
  // OS2TODO
  return NS_OK;
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
