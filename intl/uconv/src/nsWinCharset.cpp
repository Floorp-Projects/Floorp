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

#include <windows.h>
#include "nsUConvDll.h"



// We should put the data into a wincharset.properties file
// so we can easily extend it.
static const char* ACPToCharset(UINT aACP)
{
  switch(aACP)
  {
        case 874:  return "tis-620"; break;
        case 932:  return "Shift_JIS"; break;
        case 936:  return "GB2312"; break;
        case 949:  return "EUC-KR"; break;
        case 950:  return "Big5"; break;
        case 1250:  return "windows-1250"; break;
        case 1251:  return "windows-1251"; break;
        case 1252:  return "ISO-8859-1"; break;
        case 1253:  return "windows-1253"; break;
        case 1254:  return "ISO-8859-4"; break;
        case 1255:  return "windows-1255"; break;
        case 1256:  return "windows-1256"; break;
        case 1257:  return "windows-1257"; break;
        case 1258:  return "windows-1258"; break;
        default:
           return "ISO-8859-1";
  };
}

class nsWinCharset : public nsIPlatformCharset
{
  NS_DECL_ISUPPORTS

public:

  nsWinCharset();
  ~nsWinCharset();

  NS_IMETHOD GetCharset(nsPlatformCharsetSel selector, nsString& oResult);

};

NS_IMPL_ISUPPORTS(nsWinCharset, kIPlatformCharsetIID);

nsWinCharset::nsWinCharset()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}
nsWinCharset::~nsWinCharset()
{
  PR_AtomicDecrement(&g_InstanceCount);
}

NS_IMETHODIMP 
nsWinCharset::GetCharset(nsPlatformCharsetSel selector, nsString& oResult)
{
   oResult = ACPToCharset(GetACP());
   return NS_OK;
}

class nsWinCharsetFactory : public nsIFactory {
   NS_DECL_ISUPPORTS

public:
   nsWinCharsetFactory() {
     NS_INIT_REFCNT();
     PR_AtomicIncrement(&g_InstanceCount);
   }
   ~nsWinCharsetFactory() {
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

