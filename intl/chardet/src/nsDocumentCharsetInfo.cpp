/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsCharDetDll.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
#include "nsDocumentCharsetInfo.h"
#include "nsCOMPtr.h"

// XXX doc me

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

class nsDocumentCharsetInfo : public nsIDocumentCharsetInfo
{
  NS_DECL_ISUPPORTS

public:

  nsDocumentCharsetInfo ();
  virtual ~nsDocumentCharsetInfo ();

  NS_IMETHOD SetForcedCharset(nsIAtom * aCharset);
  NS_IMETHOD GetForcedCharset(nsIAtom ** aResult);

  NS_IMETHOD SetForcedDetector(PRBool aForced);
  NS_IMETHOD GetForcedDetector(PRBool * aResult);

  NS_IMETHOD SetParentCharset(nsIAtom * aCharset);
  NS_IMETHOD GetParentCharset(nsIAtom ** aResult);

private:
  nsCOMPtr<nsIAtom> mForcedCharset;
  nsCOMPtr<nsIAtom> mParentCharset;
};

class nsDocumentCharsetInfoFactory : public nsIFactory 
{
  NS_DECL_ISUPPORTS

public:

  nsDocumentCharsetInfoFactory() {
    NS_INIT_REFCNT();
    PR_AtomicIncrement(&g_InstanceCount);
  }

  virtual ~nsDocumentCharsetInfoFactory() {
    PR_AtomicDecrement(&g_InstanceCount);
  }

   NS_IMETHOD CreateInstance(nsISupports* aDelegate, const nsIID& aIID, void** aResult);
   NS_IMETHOD LockFactory(PRBool aLock);
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDocumentCharsetInfo, nsIDocumentCharsetInfo);

nsDocumentCharsetInfo::nsDocumentCharsetInfo() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsDocumentCharsetInfo::~nsDocumentCharsetInfo() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

NS_IMETHODIMP nsDocumentCharsetInfo::SetForcedCharset(nsIAtom * aCharset)
{
  mForcedCharset = aCharset;
  return NS_OK;
}

NS_IMETHODIMP nsDocumentCharsetInfo::GetForcedCharset(nsIAtom ** aResult)
{
  *aResult = mForcedCharset;
  if (mForcedCharset) NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP nsDocumentCharsetInfo::SetForcedDetector(PRBool aForced)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP nsDocumentCharsetInfo::GetForcedDetector(PRBool * aResult)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP nsDocumentCharsetInfo::SetParentCharset(nsIAtom * aCharset)
{
  mParentCharset = aCharset;
  return NS_OK;
}

NS_IMETHODIMP nsDocumentCharsetInfo::GetParentCharset(nsIAtom ** aResult)
{
  *aResult = mParentCharset;
  if (mParentCharset) NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsDocumentCharsetInfoFactory, nsIFactory)

NS_IMETHODIMP nsDocumentCharsetInfoFactory::CreateInstance(
                                            nsISupports* aDelegate, 
                                            const nsIID &aIID, 
                                            void** aResult)
{
  if(NULL == aResult)
        return NS_ERROR_NULL_POINTER;
  if(NULL != aDelegate)
        return NS_ERROR_NO_AGGREGATION;

  *aResult = NULL;

  nsDocumentCharsetInfo * inst = new nsDocumentCharsetInfo();


  if(NULL == inst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult res =inst->QueryInterface(aIID, aResult);
  if(NS_FAILED(res)) {
     delete inst;
  }

  return res;
}

NS_IMETHODIMP nsDocumentCharsetInfoFactory::LockFactory(PRBool aLock)
{
  if(aLock)
     PR_AtomicIncrement( &g_LockCount );
  else
     PR_AtomicDecrement( &g_LockCount );
  return NS_OK;
}

nsIFactory * NEW_DOCUMENT_CHARSET_INFO_FACTORY()
{
  return new nsDocumentCharsetInfoFactory();
}
