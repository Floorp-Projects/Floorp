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
#include "nsIRenderingContext.h"
#include "nsCRT.h"
#include "libimg.h"
#include "il_util.h"

static NS_DEFINE_IID(kIImageGroupIID, NS_IIMAGEGROUP_IID);

static void ns_observer_proc (XP_Observable aSource,
                              XP_ObservableMsg	aMsg, 
                              void* aMsgData, 
                              void* aClosure);

class ImageGroupImpl : public nsIImageGroup
{
public:
  ImageGroupImpl(nsIImageManager *aManager);
  ~ImageGroupImpl();

  nsresult Init(nsIRenderingContext *aRenderingContext);

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  NS_DECL_ISUPPORTS

  virtual PRBool AddObserver(nsIImageGroupObserver *aObserver);
  virtual PRBool RemoveObserver(nsIImageGroupObserver *aObserver);

  virtual nsIImageRequest* GetImage(const char* aUrl, 
                                    nsIImageRequestObserver *aObserver,
                                    nscolor aBackgroundColor,
                                    PRUint32 aWidth, PRUint32 aHeight,
                                    PRUint32 aFlags);
  
  virtual void Interrupt(void);

  IL_GroupContext *GetGroupContext() { return mGroupContext; }
  nsVoidArray *GetObservers() { return mObservers; }

private:
  nsIImageManager *mManager;
  IL_GroupContext *mGroupContext;
  nsVoidArray *mObservers;
  nsIRenderingContext *mRenderingContext;
  IL_ColorSpace *mColorSpace;
};

ImageGroupImpl::ImageGroupImpl(nsIImageManager *aManager)
{
  NS_INIT_REFCNT();
  // XXX: The caller has already called AddRef() on aManager...
  mManager = aManager;
}
 
ImageGroupImpl::~ImageGroupImpl()
{
  if (mRenderingContext != nsnull) {
      NS_RELEASE(mRenderingContext);
  }

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
  
  if (mColorSpace != nsnull) {
    IL_ReleaseColorSpace(mColorSpace);
  }

  if (mManager != nsnull) {
    NS_RELEASE(mManager);
  }
}

NS_IMPL_ISUPPORTS(ImageGroupImpl, kIImageGroupIID)

nsresult 
ImageGroupImpl::Init(nsIRenderingContext *aRenderingContext)
{
  ilIImageRenderer *renderer;
  nsresult result;

  if ((result = NS_NewImageRenderer(&renderer)) != NS_OK) {
      return result;
  }

  // XXX Need to create an image renderer
  mGroupContext = IL_NewGroupContext((void *)aRenderingContext,
                                     renderer);

  if (mGroupContext == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mRenderingContext = aRenderingContext;
  NS_ADDREF(mRenderingContext);

  // XXX Hard coded to be 24-bit. We need to get the right information
  // from the rendering context (or device context).
  IL_RGBBits colorRGBBits;

  colorRGBBits.red_shift = 16;  
  colorRGBBits.red_bits = 8;
  colorRGBBits.green_shift = 8;
  colorRGBBits.green_bits = 8; 
  colorRGBBits.blue_shift = 0; 
  colorRGBBits.blue_bits = 8;  
  mColorSpace = IL_CreateTrueColorSpace(&colorRGBBits, 24);
  IL_AddRefToColorSpace(mColorSpace);

  IL_DisplayData displayData;
	displayData.dither_mode = IL_Auto;
  displayData.color_space = mColorSpace;
  displayData.progressive_display = PR_TRUE;
  IL_SetDisplayMode(mGroupContext, 
                    IL_COLOR_SPACE | IL_PROGRESSIVE_DISPLAY | IL_DITHER_MODE,
                    &displayData);

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
                         nscolor aBackgroundColor,
                         PRUint32 aWidth, PRUint32 aHeight,
                         PRUint32 aFlags)
{
  NS_PRECONDITION(nsnull != aUrl, "null URL");
  
  ImageRequestImpl *image_req = new ImageRequestImpl();
  if (image_req != nsnull) {
    if (image_req->Init(mGroupContext, aUrl, aObserver, aBackgroundColor,
                        aWidth, aHeight, aFlags) == NS_OK) {
      NS_ADDREF(image_req);
    }
    else {
      delete image_req;
      image_req = nsnull;
    }
  }

  return image_req;
}
  
void 
ImageGroupImpl::Interrupt(void)
{
  if (mGroupContext != nsnull) {
    IL_InterruptContext(mGroupContext);
  }
}

nsresult
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

