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


#define NS_IMPL_IDS

#include "nsIPlatformCharset.h"
#include "nsPlatformCharsetFactory.h"
#include "pratom.h"

#include "nsUConvDll.h"



class nsUNIXCharset : public nsIPlatformCharset
{
  NS_DECL_ISUPPORTS

public:

  nsUNIXCharset();
  ~nsUNIXCharset();

  NS_IMETHOD GetCharset(nsPlatformCharsetSel selector, nsString& oResult);

};

NS_IMPL_ISUPPORTS(nsUNIXCharset, kIPlatformCharsetIID);

nsUNIXCharset::nsUNIXCharset()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}
nsUNIXCharset::~nsUNIXCharset()
{
  PR_AtomicDecrement(&g_InstanceCount);
}

NS_IMETHODIMP 
nsUNIXCharset::GetCharset(nsPlatformCharsetSel selector, nsString& oResult)
{
   oResult = "ISO-8859-1"; // XXX- hack to be implement
   return NS_OK;
}

class nsUNIXCharsetFactory : public nsIFactory {
   NS_DECL_ISUPPORTS

public:
   nsUNIXCharsetFactory() {
   }
   ~nsUNIXCharsetFactory() {
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

