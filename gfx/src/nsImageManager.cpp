/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsIImageManager.h"
#include "nsIMemory.h"

#include "libimg.h"

#include "nsCRT.h"
#include "nsImageNet.h"
#include "nsCOMPtr.h"
#include "nsIPref.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kIImageManagerIID, NS_IIMAGEMANAGER_IID);

class ImageManagerImpl : public nsIImageManager, public nsIMemoryPressureObserver
{
public:
  ImageManagerImpl();
  virtual ~ImageManagerImpl();

  nsresult Init();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  NS_DECL_NSIMEMORYPRESSUREOBSERVER
  
  virtual void SetCacheSize(PRInt32 aCacheSize);
  virtual PRInt32 GetCacheSize(void);
  virtual PRInt32 ShrinkCache(void);
  NS_IMETHOD FlushCache(void);
 // virtual nsImageType GetImageType(const char *buf, PRInt32 length);

private:
  nsCOMPtr<ilISystemServices> mSS;
};

// The singleton image manager
// This a service on XP_PC , mac and gtk. Need to convert 
// it to a service on all the remaining platforms.
static ImageManagerImpl*   gImageManager = nsnull;

ImageManagerImpl::ImageManagerImpl()
{
  NS_INIT_REFCNT();
  NS_NewImageSystemServices(getter_AddRefs(mSS));
  IL_Init(mSS);
  
  PRInt32 cacheSize  = (1024L * 2048L);
  
  nsresult res = NS_ERROR_FAILURE;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res);
  if (prefs)
  {
      PRInt32 tempSize;
      res = prefs->GetIntPref("browser.cache.image_cache_size", &tempSize);
      if ( NS_SUCCEEDED(res) )
      {
        cacheSize = tempSize * 1024L;
      }
  }

  IL_SetCacheSize(cacheSize);
}

ImageManagerImpl::~ImageManagerImpl()
{
  // nsMemory needs to be fixed to not hold refs to observers.
  // nsMemory::UnregisterObserver(this);

  IL_Shutdown();
  gImageManager = nsnull;
}

NS_IMPL_ISUPPORTS2(ImageManagerImpl, nsIImageManager, nsIMemoryPressureObserver); 

nsresult 
ImageManagerImpl::Init()
{
  // don't register until bug 52393 is fixed.
  // nsMemory::RegisterObserver(this);
  return NS_OK;
}

void  
ImageManagerImpl::SetCacheSize(PRInt32 aCacheSize)
{
  IL_SetCacheSize(aCacheSize);
}

PRInt32 
ImageManagerImpl::GetCacheSize()
{
  return IL_GetCacheSize();
}
 
PRInt32 
ImageManagerImpl::ShrinkCache(void)
{
  return IL_ShrinkCache();
}

NS_IMETHODIMP
ImageManagerImpl::FlushCache(void)
{
  IL_FlushCache();
  return NS_OK;
}

NS_IMETHODIMP
ImageManagerImpl::FlushMemory(PRUint32 reason, size_t requestedAmount)
{
  IL_FlushCache();
  return NS_OK;
}

extern "C" NS_GFX_(nsresult)
NS_NewImageManager(nsIImageManager **aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (nsnull == gImageManager) {
    gImageManager = new ImageManagerImpl();
  }
  if (nsnull == gImageManager) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return gImageManager->QueryInterface(kIImageManagerIID,
                                       (void **)aInstancePtrResult);
}

/* This is going to be obsolete, once ImageManagerImpl becomes a service on all platforms */
extern "C" NS_GFX_(void)
NS_FreeImageManager()
{
  /*Do not release it on platforms on which ImageManagerImpl is a service. 
  Need to remove this method, once ImageManagerImpl is converted to a service on all platforms.*/
  NS_IF_RELEASE(gImageManager);
}
