/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#include "nsIWalletPreview.h"
#include "nsISignonViewer.h"
#include "nsICookieViewer.h"
#include "nsIWalletEditor.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

static NS_DEFINE_CID(kWalletPreviewCID,           NS_WALLETPREVIEW_CID);
static NS_DEFINE_CID(kSignonViewerCID,            NS_SIGNONVIEWER_CID);
static NS_DEFINE_CID(kCookieViewerCID,            NS_COOKIEVIEWER_CID);
static NS_DEFINE_CID(kWalletEditorCID,            NS_WALLETEDITOR_CID);

class nsWalletViewerFactory : public nsIFactory
{   
public:   
  // nsISupports methods   
  NS_IMETHOD QueryInterface(const nsIID &aIID,    
                            void **aResult);   
  NS_IMETHOD_(nsrefcnt) AddRef(void);   
  NS_IMETHOD_(nsrefcnt) Release(void);   

  // nsIFactory methods   
  NS_IMETHOD CreateInstance(nsISupports *aOuter,   
                            const nsIID &aIID,   
                            void **aResult);   

  NS_IMETHOD LockFactory(PRBool aLock);   

  nsWalletViewerFactory(const nsCID &aClass);   

protected:
  virtual ~nsWalletViewerFactory();   

private:   
  nsrefcnt  mRefCnt;   
  nsCID     mClassID;
};   

nsWalletViewerFactory::nsWalletViewerFactory(const nsCID &aClass)   
{   
  mRefCnt = 0;
  mClassID = aClass;
}   

nsWalletViewerFactory::~nsWalletViewerFactory()   
{   
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}   

nsresult
nsWalletViewerFactory::QueryInterface(const nsIID &aIID, void **aResult)   
{   
  if (aResult == NULL) {   
    return NS_ERROR_NULL_POINTER;   
  }   

  // Always NULL result, in case of failure   
  *aResult = NULL;   

  if (aIID.Equals(kISupportsIID)) {   
    *aResult = (void *)(nsISupports*)this;   
  } else if (aIID.Equals(kIFactoryIID)) {   
    *aResult = (void *)(nsIFactory*)this;   
  }   

  if (*aResult == NULL) {   
    return NS_NOINTERFACE;   
  }   

  AddRef(); // Increase reference count for caller   
  return NS_OK;   
}   

nsrefcnt
nsWalletViewerFactory::AddRef()   
{   
  return ++mRefCnt;   
}   

nsrefcnt
nsWalletViewerFactory::Release()   
{   
  if (--mRefCnt == 0) {   
    delete this;   
    return 0; // Don't access mRefCnt after deleting!   
  }   
  return mRefCnt;   
}  

nsresult
nsWalletViewerFactory::CreateInstance(nsISupports *aOuter,  
                                const nsIID &aIID,  
                                void **aResult)  
{  
  nsresult res;
  PRBool refCounted = PR_TRUE;

  if (aResult == NULL) {  
    return NS_ERROR_NULL_POINTER;  
  }  

  *aResult = NULL;  
  
  nsISupports *inst = nsnull;

  if (mClassID.Equals(kWalletPreviewCID)) {
      if (NS_FAILED(res = NS_NewWalletPreview((nsIWalletPreview**) &inst)))
          return res;
  } 
  else if (mClassID.Equals(kSignonViewerCID)) {
      if (NS_FAILED(res = NS_NewSignonViewer((nsISignonViewer**) &inst)))
          return res;
  }
  else if (mClassID.Equals(kCookieViewerCID)) {
      if (NS_FAILED(res = NS_NewCookieViewer((nsICookieViewer**) &inst)))
          return res;
  } 
  else if (mClassID.Equals(kWalletEditorCID)) {
      if (NS_FAILED(res = NS_NewWalletEditor((nsIWalletEditor**) &inst)))
          return res;
  }
  else {
      return NS_ERROR_NO_INTERFACE;
  }

  if (inst == NULL) {  
    return NS_ERROR_OUT_OF_MEMORY;  
  }  

  res = inst->QueryInterface(aIID, aResult);

  if (refCounted) {
    NS_RELEASE(inst);
  }
  else if (res != NS_OK) {  
    // We didn't get the right interface, so clean up  
    delete inst;  
  }  

  return res;  
}  

nsresult nsWalletViewerFactory::LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = new nsWalletViewerFactory(aClass);

  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}

extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{

  nsresult rv = NS_OK;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kWalletPreviewCID,
                                  "WalletPreview World Component",
                                  "component://netscape/walletpreview/walletpreview-world",
                                  aPath, PR_TRUE, PR_TRUE);

  rv = compMgr->RegisterComponent(kSignonViewerCID,
                                  "SignonViewer World Component",
                                  "component://netscape/signonviewer/signonviewer-world",
                                  aPath, PR_TRUE, PR_TRUE);

  rv = compMgr->RegisterComponent(kCookieViewerCID,
                                  "CookieViewer World Component",
                                  "component://netscape/cookieviewer/cookieviewer-world",
                                  aPath, PR_TRUE, PR_TRUE);

  rv = compMgr->RegisterComponent(kWalletEditorCID,
                                  "WalletEditor World Component",
                                  "component://netscape/walleteditor/walleteditor-world",
                                  aPath, PR_TRUE, PR_TRUE);

  return NS_OK;
}


extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{

  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kWalletPreviewCID, aPath);

  rv = compMgr->UnregisterComponent(kSignonViewerCID, aPath);

  rv = compMgr->UnregisterComponent(kCookieViewerCID, aPath);

  rv = compMgr->UnregisterComponent(kWalletEditorCID, aPath);

  return NS_OK;
}
