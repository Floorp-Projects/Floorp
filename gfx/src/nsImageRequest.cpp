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

#include "nsImageRequest.h"
#include "nsIImageGroup.h"
#include "nsIImageObserver.h"
#include "nsIImage.h"
#include "nsVoidArray.h"
#include "nsRect.h"
#include "nsImageNet.h"

static NS_DEFINE_IID(kIImageRequestIID, NS_IIMAGEREQUEST_IID);

static void ns_observer_proc (XP_Observable aSource,
                              XP_ObservableMsg	aMsg, 
                              void* aMsgData, 
                              void* aClosure);

ImageRequestImpl::ImageRequestImpl()
{
}

ImageRequestImpl::~ImageRequestImpl()
{
  // Delete the list of observers, and release the reference to the image
  // request observer object
  if (nsnull != mObservers) {
    for (PRInt32 cnt = 0; cnt < mObservers->Count(); cnt++)
    {
      nsIImageRequestObserver* observer;
      observer = (nsIImageRequestObserver*)mObservers->ElementAt(cnt);

      NS_IF_RELEASE(observer);
      mObservers->ReplaceElementAt(nsnull, cnt);
    }
    delete mObservers;
  }

  // XP Observer list destroyed by the image library
}

nsresult
ImageRequestImpl::Init(IL_GroupContext *aGroupContext, 
                       const char* aUrl, 
                       nsIImageRequestObserver *aObserver,
                       const nscolor* aBackgroundColor,
                       PRUint32 aWidth, PRUint32 aHeight,
                       PRUint32 aFlags,
                       ilINetContext* aNetContext)
{
  NS_PRECONDITION(nsnull != aGroupContext, "null group context");
  NS_PRECONDITION(nsnull != aUrl, "null URL");

  NS_Error status;
  IL_IRGB bgcolor;
  PRUint32 flags;

  mGroupContext = aGroupContext;

  if (nsnull != aObserver) {
    if (AddObserver(aObserver) == PR_FALSE) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  status = XP_NewObserverList(NULL, &mXPObserver);
  if (status < 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  status = XP_AddObserver(mXPObserver, ns_observer_proc, (void *)this);
  if (status < 0) {
    XP_DisposeObserverList(mXPObserver);
    mXPObserver = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (nsnull != aBackgroundColor)
  {
    bgcolor.red = NS_GET_R(*aBackgroundColor);
    bgcolor.green = NS_GET_G(*aBackgroundColor);
    bgcolor.blue = NS_GET_B(*aBackgroundColor);
  }
  
  flags = ((aFlags & IL_HIGH_PRIORITY) ? nsImageLoadFlags_kHighPriority : 0) |
    ((aFlags & IL_STICKY) ? nsImageLoadFlags_kSticky : 0) |
    ((aFlags & IL_BYPASS_CACHE) ? nsImageLoadFlags_kBypassCache : 0) |
    ((aFlags & IL_ONLY_FROM_CACHE) ? nsImageLoadFlags_kOnlyFromCache : 0);
  
  mImageReq = IL_GetImage(aUrl, aGroupContext, mXPObserver,
                          nsnull == aBackgroundColor ? nsnull : &bgcolor,
                          aWidth, aHeight, flags, (void *)aNetContext);
			  
  if (mImageReq == nsnull) {
    XP_DisposeObserverList(mXPObserver);
    mXPObserver = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMPL_ADDREF(ImageRequestImpl)
NS_IMPL_QUERY_INTERFACE(ImageRequestImpl, kIImageRequestIID)

nsrefcnt ImageRequestImpl::Release(void)                        
{
  if (--mRefCnt == 0) {
    if (mXPObserver) {
      // Make sure dangling reference to this object goes away
      XP_RemoveObserver(mXPObserver, ns_observer_proc, (void*)this);
    }
    if (mImageReq) {
      IL_DestroyImage(mImageReq);
    }
    NS_DELETEXPCOM(this);
    return 0;
  }
  return mRefCnt;
}

nsIImage* 
ImageRequestImpl::GetImage()
{
  if (mImageReq != nsnull) {
    IL_Pixmap *pixmap = IL_GetImagePixmap(mImageReq);

    if (pixmap != nsnull) {
      nsIImage *image = (nsIImage *)pixmap->client_data;
      
      if (image != nsnull) {
        NS_ADDREF(image);
        return image;
      }
    }
  }

  return nsnull;
}

void 
ImageRequestImpl::GetNaturalDimensions(PRUint32 *aWidth, PRUint32 *aHeight)
{
  if (mImageReq != nsnull) {
    IL_GetNaturalDimensions(mImageReq, (int *)aWidth, (int *)aHeight);
  }
  else {
    *aWidth = 0;
    *aHeight = 0;
  }
}

PRBool 
ImageRequestImpl::AddObserver(nsIImageRequestObserver *aObserver)
{
  if (aObserver == nsnull) {
    return PR_FALSE;
  }

  if (mObservers == nsnull) {
    mObservers = new nsVoidArray();
    if (mObservers == nsnull) {
      return PR_FALSE;
    }
  }
  
  NS_ADDREF(aObserver);
  mObservers->AppendElement((void *)aObserver);
  
  return PR_TRUE;
}
 
PRBool 
ImageRequestImpl::RemoveObserver(nsIImageRequestObserver *aObserver)
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

void 
ImageRequestImpl::Interrupt()
{
  if (mImageReq != nsnull) {
    IL_InterruptRequest(mImageReq);
  }
}


static void ns_observer_proc (XP_Observable aSource,
                              XP_ObservableMsg	aMsg, 
                              void* aMsgData, 
                              void* aClosure)
{
  ImageRequestImpl *image_request = (ImageRequestImpl *)aClosure;
  IL_MessageData *message_data = (IL_MessageData *)aMsgData;
  nsVoidArray *observer_list = image_request->GetObservers();
  nsIImage *image = nsnull;
  //  IL_ImageReq *il_image_req = image_request->GetImageRequest();
  IL_ImageReq *il_image_req = (IL_ImageReq *)aSource;

  if (il_image_req != nsnull) {
    IL_Pixmap *pixmap = IL_GetImagePixmap(il_image_req);

    if (pixmap != nsnull) {
      image = (nsIImage *)pixmap->client_data;
    }
  }

  if (observer_list != nsnull) {
    PRInt32 i, count = observer_list->Count();
    nsIImageRequestObserver *observer;
    for (i = 0; i < count; i++) {
      observer = (nsIImageRequestObserver *)observer_list->ElementAt(i);
      if (observer != nsnull) {
        switch (aMsg) {
          case IL_START_URL:
            observer->Notify(image_request,
                             image, nsImageNotification_kStartURL, 0, 0,
                             nsnull);
            break;
          case IL_DESCRIPTION:
            observer->Notify(image_request,
                             image, nsImageNotification_kDescription, 0, 0,
                             message_data->description);
            break;
          case IL_DIMENSIONS:
            observer->Notify(image_request,
                             image, nsImageNotification_kDimensions, 
                             (PRInt32)message_data->width, 
                             (PRInt32)message_data->height,
                             nsnull);
            break;
          case IL_IS_TRANSPARENT:
            observer->Notify(image_request,
                             image, nsImageNotification_kIsTransparent, 0, 0,
                             nsnull);
            break;
          case IL_PIXMAP_UPDATE:
            {
              nsRect rect(message_data->update_rect.x_origin,
                          message_data->update_rect.y_origin,
                          message_data->update_rect.width,
                          message_data->update_rect.height);

              observer->Notify(image_request,
                               image, nsImageNotification_kPixmapUpdate, 0, 0,
                               &rect);
            }
            break;
          case IL_FRAME_COMPLETE:
            observer->Notify(image_request,
                             image, nsImageNotification_kFrameComplete, 0, 0,
                             nsnull);
            break;
          case IL_PROGRESS:
            observer->Notify(image_request,
                             image, nsImageNotification_kProgress, 
                             message_data->percent_progress, 0,
                             nsnull);
            break;
          case IL_IMAGE_COMPLETE:
            observer->Notify(image_request,
                             image, nsImageNotification_kImageComplete, 0, 0,
                             nsnull);
            break;
          case IL_STOP_URL:
            observer->Notify(image_request,
                             image, nsImageNotification_kStopURL, 0, 0,
                             nsnull);
            break;
          case IL_IMAGE_DESTROYED:
            image_request->SetImageRequest(nsnull);
            observer->Notify(image_request,
                             image, nsImageNotification_kImageDestroyed, 0, 0,
                             nsnull);
            break;
          case IL_ABORTED:
            observer->Notify(image_request,
                             image, nsImageNotification_kAborted, 0, 0,
                             nsnull);
            break;
          case IL_INTERNAL_IMAGE:
            observer->Notify(image_request,
                             image, nsImageNotification_kInternalImage, 0, 0,
                             nsnull);
            break;
          case IL_NOT_IN_CACHE:
            observer->NotifyError(image_request, 
                                  nsImageError_kNotInCache);
            break;
          case IL_ERROR_NO_DATA:
            observer->NotifyError(image_request, 
                                  nsImageError_kNoData);
            break;
          case IL_ERROR_IMAGE_DATA_CORRUPT:
            observer->NotifyError(image_request, 
                                  nsImageError_kImageDataCorrupt);
            break;
          case IL_ERROR_IMAGE_DATA_TRUNCATED:
            observer->NotifyError(image_request, 
                                  nsImageError_kImageDataTruncated);
            break;
          case IL_ERROR_IMAGE_DATA_ILLEGAL:
            observer->NotifyError(image_request, 
                                  nsImageError_kImageDataIllegal);
            break;
          case IL_ERROR_INTERNAL:
            observer->NotifyError(image_request, 
                                  nsImageError_kInternalError);
            break;
        }
      }
    }
  }

  /* 
   * If the IL_ImageReq is being destroyed, clear the reference held by
   * the nsImageRequestImpl...  
   * 
   * This will prevent a dangling reference in cases where the image group 
   * is destroyed before the image request...
   */
  if ((IL_IMAGE_DESTROYED == aMsg) && (nsnull != image_request)) {
    image_request->SetImageRequest(nsnull);
    image_request->ImageDestroyed();
  }
}

void
ImageRequestImpl::ImageDestroyed()
{
  if (mXPObserver) {
    // Make sure dangling reference to this object goes
    // away; this is just in case the image library changes
    // and holds onto the observer list after destroying the
    // image.
    XP_RemoveObserver(mXPObserver, ns_observer_proc, (void*)this);
    mXPObserver = nsnull;
  }
}
