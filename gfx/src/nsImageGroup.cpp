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

#include "nsIImageGroup.h"
#include "nsIImageManager.h"
#include "nsIImageObserver.h"
#include "nsIImageRequest.h"
#include "nsImageRequest.h"
#include "ilIImageRenderer.h"
#include "nsImageNet.h"
#include "nsVoidArray.h"
#include "nsCRT.h"
#include "libimg.h"
#include "il_util.h"
#include "nsIDeviceContext.h"
#include "nsIStreamListener.h"
#include "nsIURLGroup.h"

static NS_DEFINE_IID(kIImageGroupIID, NS_IIMAGEGROUP_IID);

static void ns_observer_proc (XP_Observable aSource,
                              XP_ObservableMsg	aMsg, 
                              void* aMsgData, 
                              void* aClosure);

class ImageGroupImpl : public nsIImageGroup
{
public:
  ImageGroupImpl(nsIImageManager *aManager);
  virtual ~ImageGroupImpl();

  nsresult Init(nsIDeviceContext *aDeviceContext, nsIURLGroup* aURLGroup);

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  virtual PRBool AddObserver(nsIImageGroupObserver *aObserver);
  virtual PRBool RemoveObserver(nsIImageGroupObserver *aObserver);

  virtual nsIImageRequest* GetImage(const char* aUrl, 
                                    nsIImageRequestObserver *aObserver,
                                    const nscolor* aBackgroundColor,
                                    PRUint32 aWidth, PRUint32 aHeight,
                                    PRUint32 aFlags);

  NS_IMETHOD GetImageFromStream(const char* aURL,
                                nsIImageRequestObserver *aObserver,
                                const nscolor* aBackgroundColor,
                                PRUint32 aWidth, PRUint32 aHeight,
                                PRUint32 aFlags,
                                nsIImageRequest*& aResult,
                                nsIStreamListener*& aListenerResult);
  
  virtual void Interrupt(void);

  IL_GroupContext *GetGroupContext() { return mGroupContext; }
  nsVoidArray *GetObservers() { return mObservers; }

  nsIImageManager *mManager;
  IL_GroupContext *mGroupContext;
  nsVoidArray *mObservers;
  nsIDeviceContext *mDeviceContext;
  ilINetContext* mNetContext;
  nsIStreamListener** mListenerRequest;
  nsIURLGroup* mURLGroup;
};

ImageGroupImpl::ImageGroupImpl(nsIImageManager *aManager)
{
  NS_INIT_REFCNT();
  mManager = aManager;
  NS_ADDREF(mManager);
}
 
ImageGroupImpl::~ImageGroupImpl()
{
  NS_IF_RELEASE(mDeviceContext);

  if (mObservers != nsnull) {
    PRInt32 i, count = mObservers->Count();
    nsIImageGroupObserver *observer;
    for (i = 0; i < count; i++) {
      observer = (nsIImageGroupObserver *)mObservers->ElementAt(i);
      if (observer != nsnull) {
        NS_RELEASE(observer);
      }
    }

    delete mObservers;
  }

  if (mGroupContext != nsnull) {
    IL_DestroyGroupContext(mGroupContext);
  }
  
  NS_IF_RELEASE(mManager);
  NS_IF_RELEASE(mNetContext);
  NS_IF_RELEASE(mURLGroup);
}

NS_IMPL_ISUPPORTS(ImageGroupImpl, kIImageGroupIID)

static PRBool
ReconnectHack(void* arg, nsIStreamListener* aListener)
{
  ImageGroupImpl* ig = (ImageGroupImpl*) arg;
  if (nsnull != ig->mListenerRequest) {
    *ig->mListenerRequest = aListener;
    NS_ADDREF(aListener);
    return PR_TRUE;
  }
  return PR_FALSE;
}

nsresult 
ImageGroupImpl::Init(nsIDeviceContext *aDeviceContext, nsIURLGroup* aURLGroup)
{
  ilIImageRenderer *renderer;
  nsresult result;

  if ((result = NS_NewImageRenderer(&renderer)) != NS_OK) {
    return result;
  }

  mGroupContext = IL_NewGroupContext((void *)aDeviceContext,
                                     renderer);
  if (mGroupContext == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mURLGroup = aURLGroup;
  NS_IF_ADDREF(mURLGroup);

  // Create an async net context
  result = NS_NewImageNetContext(&mNetContext, mURLGroup, ReconnectHack, this);
  if (NS_OK != result) {
    return result;
  }

  mDeviceContext = aDeviceContext;
  NS_ADDREF(mDeviceContext);

  // Get color space to use for this device context.
  IL_ColorSpace*  colorSpace;

  mDeviceContext->GetILColorSpace(colorSpace);

  // Set the image group context display mode
  IL_DisplayData displayData;
	displayData.dither_mode = IL_Auto;
  displayData.color_space = colorSpace;
  displayData.progressive_display = PR_TRUE;
  IL_SetDisplayMode(mGroupContext, 
                    IL_COLOR_SPACE | IL_PROGRESSIVE_DISPLAY | IL_DITHER_MODE,
                    &displayData);

  // Release the color space
  IL_ReleaseColorSpace(colorSpace);
  return NS_OK;
}

PRBool 
ImageGroupImpl::AddObserver(nsIImageGroupObserver *aObserver)
{
  if (aObserver == nsnull) {
    return PR_FALSE;
  }

  if (mObservers == nsnull) {
    mObservers = new nsVoidArray();
    if (mObservers == nsnull) {
      return PR_FALSE;
    }
    IL_AddGroupObserver(mGroupContext, ns_observer_proc, (void *)this);
  }
  
  NS_ADDREF(aObserver);
  mObservers->AppendElement((void *)aObserver);
  
  return PR_TRUE;
}
 
PRBool 
ImageGroupImpl::RemoveObserver(nsIImageGroupObserver *aObserver)
{
  PRBool ret;

  if (aObserver == nsnull || mObservers == nsnull) {
    return PR_FALSE;
  }
  
  ret = mObservers->RemoveElement((void *)aObserver);
  
  if (ret == PR_TRUE) {
    NS_RELEASE(aObserver);
  }
  
  return ret;
}

nsIImageRequest* 
ImageGroupImpl::GetImage(const char* aUrl, 
                         nsIImageRequestObserver *aObserver,
                         const nscolor* aBackgroundColor,
                         PRUint32 aWidth, PRUint32 aHeight,
                         PRUint32 aFlags)
{
  NS_PRECONDITION(nsnull != aUrl, "null URL");
  
  ImageRequestImpl *image_req = new ImageRequestImpl;
  if (nsnull != image_req) {
    nsresult  result;

    // Ask the image request object to get the image.
    mListenerRequest = nsnull;
    result = image_req->Init(mGroupContext, aUrl, aObserver, aBackgroundColor,
                             aWidth, aHeight, aFlags, mNetContext);

    if (NS_SUCCEEDED(result)) {
      NS_ADDREF(image_req);
    } else {
      delete image_req;
      image_req = nsnull;
    }
  }

  return image_req;
}

NS_IMETHODIMP
ImageGroupImpl::GetImageFromStream(const char* aUrl,
                                   nsIImageRequestObserver *aObserver,
                                   const nscolor* aBackgroundColor,
                                   PRUint32 aWidth, PRUint32 aHeight,
                                   PRUint32 aFlags,
                                   nsIImageRequest*& aResult,
                                   nsIStreamListener*& aListenerResult)
{
  NS_PRECONDITION(nsnull != aUrl, "null URL");
  
  nsresult  result = NS_OK;
  ImageRequestImpl *image_req = new ImageRequestImpl;
  if (nsnull == image_req) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
    
  // Ask the image request object to get the image.
  nsIStreamListener* listener = nsnull;
  mListenerRequest = &listener;
  result = image_req->Init(mGroupContext, aUrl, aObserver, aBackgroundColor,
                           aWidth, aHeight, aFlags, mNetContext);
  aListenerResult = listener;
  mListenerRequest = nsnull;

  if (NS_SUCCEEDED(result)) {
    NS_ADDREF(image_req);
  } else {
    delete image_req;
    image_req = nsnull;
  }

  aResult = image_req;
  return result;
}
  
void 
ImageGroupImpl::Interrupt(void)
{
  if (mGroupContext != nsnull) {
    IL_InterruptContext(mGroupContext);
  }
}

extern "C" NS_GFX_(nsresult)
NS_NewImageGroup(nsIImageGroup **aInstancePtrResult)
{
  nsresult result;

  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIImageManager *manager;
  if ((result = NS_NewImageManager(&manager)) != NS_OK) {
      return result;
  }

  nsIImageGroup *group = new ImageGroupImpl(manager);
  if (group == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_RELEASE(manager);

  return group->QueryInterface(kIImageGroupIID, (void **) aInstancePtrResult);
}


static void ns_observer_proc (XP_Observable aSource,
                              XP_ObservableMsg	aMsg, 
                              void* aMsgData, 
                              void* aClosure)
{
  ImageGroupImpl *image_group = (ImageGroupImpl *)aClosure;
  nsVoidArray *observer_list = image_group->GetObservers();

  if (observer_list != nsnull) {
    PRInt32 i, count = observer_list->Count();
    nsIImageGroupObserver *observer;
    for (i = 0; i < count; i++) {
      observer = (nsIImageGroupObserver *)observer_list->ElementAt(i);
      if (observer != nsnull) {
        switch (aMsg) {
          case IL_STARTED_LOADING:
            observer->Notify(image_group,  
                             nsImageGroupNotification_kStartedLoading);
            break;
          case IL_ABORTED_LOADING:
            observer->Notify(image_group, 
                             nsImageGroupNotification_kAbortedLoading);
          case IL_FINISHED_LOADING:
            observer->Notify(image_group, 
                             nsImageGroupNotification_kFinishedLoading);
          case IL_STARTED_LOOPING:
            observer->Notify(image_group, 
                             nsImageGroupNotification_kStartedLooping);
          case IL_FINISHED_LOOPING:
            observer->Notify(image_group, 
                             nsImageGroupNotification_kFinishedLooping);
        }
      }
    }
  }
}

