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
#include "nsFrameImageLoader.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsIFrame.h"
#include "nsIURL.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIImageGroup.h"
#include "nsIImageRequest.h"
#include "nsString.h"
//#include "prlog.h"
#include "nsIStyleContext.h"
//#include "nsIDeviceContext.h"

static NS_DEFINE_IID(kIFrameImageLoaderIID, NS_IFRAME_IMAGE_LOADER_IID);
static NS_DEFINE_IID(kIImageRequestObserverIID, NS_IIMAGEREQUESTOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_LAYOUT nsresult
NS_NewFrameImageLoader(nsIFrameImageLoader** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsFrameImageLoader* it = new nsFrameImageLoader();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIFrameImageLoaderIID, (void**) aResult);
}

nsFrameImageLoader::nsFrameImageLoader()
  : mPresContext(nsnull),
    mImageRequest(nsnull),
    mImage(nsnull),
    mHaveBackgroundColor(PR_FALSE),
    mHaveDesiredSize(PR_FALSE),
    mBackgroundColor(0),
    mDesiredSize(0, 0),
    mImageLoadStatus(NS_IMAGE_LOAD_STATUS_NONE),
    mImageLoadError( nsImageError(-1) ),
    mNotifyLockCount(0),
    mFrames(nsnull)
{
  NS_INIT_REFCNT();
}

nsFrameImageLoader::~nsFrameImageLoader()
{
  PerFrameData* pfd = mFrames;
  while (nsnull != pfd) {
    PerFrameData* next = pfd->mNext;
    delete pfd;
    pfd = next;
  }
  NS_IF_RELEASE(mImageRequest);
  NS_IF_RELEASE(mPresContext);
  NS_IF_RELEASE(mImage);
}

NS_IMPL_ADDREF(nsFrameImageLoader)

NS_IMPL_RELEASE(nsFrameImageLoader)

NS_IMETHODIMP
nsFrameImageLoader::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIFrameImageLoaderIID)) {
    nsIFrameImageLoader* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIImageRequestObserverIID)) {
    nsIImageRequestObserver* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIFrameImageLoader* tmp1 = this;
    nsISupports* tmp2 = tmp1;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsFrameImageLoader::Init(nsIPresContext* aPresContext,
                         nsIImageGroup* aGroup,
                         const nsString& aURL,
                         const nscolor* aBackgroundColor,
                         const nsSize* aDesiredSize,
                         nsIFrame* aTargetFrame,
                         nsIFrameImageLoaderCB aCallBack,
                         void* aClosure)
{
  NS_PRECONDITION(nsnull != aPresContext, "null ptr");
  if (nsnull == aPresContext) {
    return NS_ERROR_NULL_POINTER;
  }
  NS_PRECONDITION(nsnull == mPresContext, "double init");
  if (nsnull != mPresContext) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  mPresContext = aPresContext;
  NS_IF_ADDREF(aPresContext);
  mURL = aURL;
  if (aBackgroundColor) {
    mHaveBackgroundColor = PR_TRUE;
    mBackgroundColor = *aBackgroundColor;
  }

  // Computed desired size, in pixels
  nscoord desiredWidth = 0;
  nscoord desiredHeight = 0;
  if (aDesiredSize) {
    mHaveDesiredSize = PR_TRUE;
    mDesiredSize = *aDesiredSize;

    float t2p;
    aPresContext->GetTwipsToPixels(&t2p);
    desiredWidth = NSToCoordRound(mDesiredSize.width * t2p);
    desiredHeight = NSToCoordRound(mDesiredSize.height * t2p);
  }

  PerFrameData* pfd = new PerFrameData;
  if (nsnull == pfd) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  pfd->mFrame = aTargetFrame;
  pfd->mCallBack = aCallBack;
  pfd->mClosure = aClosure;
  pfd->mNext = mFrames;
  mFrames = pfd;

  // Start image load request
  char* cp = aURL.ToNewCString();
  mImageRequest = aGroup->GetImage(cp, this, aBackgroundColor,
                                   desiredWidth, desiredHeight, 0);
  delete[] cp;

  return NS_OK;
}

NS_IMETHODIMP
nsFrameImageLoader::AddFrame(nsIFrame* aFrame,
                             nsIFrameImageLoaderCB aCallBack,
                             void* aClosure)
{
  PerFrameData* pfd = mFrames;
  while (pfd) {
    if (pfd->mFrame == aFrame) {
      pfd->mCallBack = aCallBack;
      pfd->mClosure = aClosure;
      return NS_OK;
    }
    pfd = pfd->mNext;
  }

  pfd = new PerFrameData;
  if (nsnull == pfd) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  pfd->mFrame = aFrame;
  pfd->mCallBack = aCallBack;
  pfd->mClosure = aClosure;
  pfd->mNext = mFrames;
  mFrames = pfd;
  if (aCallBack &&
      ((NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE |
        NS_IMAGE_LOAD_STATUS_ERROR) & mImageLoadStatus)) {
    // Fire notification callback right away so that caller doesn't
    // miss it...
    (*aCallBack)(mPresContext, this, pfd->mFrame, pfd->mClosure,
                 mImageLoadStatus);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameImageLoader::RemoveFrame(nsIFrame* aFrame)
{
  PerFrameData** pfdp = &mFrames;
  PerFrameData* pfd;
  while (nsnull != (pfd = *pfdp)) {
    if (pfd->mFrame == aFrame) {
      *pfdp = pfd->mNext;
      delete pfd;
      return NS_OK;
    }
    pfdp = &pfd->mNext;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameImageLoader::StopImageLoad()
{
  if (nsnull != mImageRequest) {
    mImageRequest->RemoveObserver(this);
    NS_RELEASE(mImageRequest);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameImageLoader::AbortImageLoad()
{
  if (nsnull != mImageRequest) {
    mImageRequest->Interrupt();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameImageLoader::IsSameImageRequest(const nsString& aURL,
                                       const nscolor* aBackgroundColor,
                                       const nsSize* aDesiredSize,
                                       PRBool* aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  // URL's must match (XXX: not quite right; need real comparison here...)
  if (!aURL.Equals(mURL)) {
    *aResult = PR_FALSE;
    return NS_OK;
  }

  // Background colors must match
  if (aBackgroundColor) {
    if (!mHaveBackgroundColor) {
      *aResult = PR_FALSE;
      return NS_OK;
    }
    if (mBackgroundColor != *aBackgroundColor) {
      *aResult = PR_FALSE;
      return NS_OK;
    }
  }
  else if (mHaveBackgroundColor) {
    *aResult = PR_FALSE;
    return NS_OK;
  }

  // Desired sizes must match
  if (aDesiredSize) {
    if (!mHaveDesiredSize) {
      *aResult = PR_FALSE;
      return NS_OK;
    }
    if ((mDesiredSize.width != aDesiredSize->width) ||
        (mDesiredSize.height != aDesiredSize->height)) {
      *aResult = PR_FALSE;
      return NS_OK;
    }
  }
  else if (mHaveDesiredSize) {
    *aResult = PR_FALSE;
    return NS_OK;
  }

  *aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameImageLoader::SafeToDestroy(PRBool* aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResult = PR_FALSE;
  if ((nsnull == mFrames) && (0 == mNotifyLockCount)) {
    *aResult = PR_TRUE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameImageLoader::GetURL(nsString& aResult)
{
  aResult = mURL;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameImageLoader::GetImage(nsIImage** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mImage;
  NS_IF_ADDREF(mImage);
  return NS_OK;
}

// Return the size of the image, in twips
NS_IMETHODIMP
nsFrameImageLoader::GetSize(nsSize& aResult)
{
  aResult = mDesiredSize;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameImageLoader::GetImageLoadStatus(PRUint32* aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mImageLoadStatus;
  return NS_OK;
}

void
nsFrameImageLoader::Notify(nsIImageRequest *aImageRequest,
                           nsIImage *aImage,
                           nsImageNotification aNotificationType,
                           PRInt32 aParam1, PRInt32 aParam2,
                           void *aParam3)
{
  nsIView*      view;
  nsRect        damageRect;
  float         p2t;
  const nsRect* changeRect;
  PerFrameData* pfd;

  mNotifyLockCount++;

  switch (aNotificationType) {
  case nsImageNotification_kDimensions:
    mPresContext->GetScaledPixelsToTwips(&p2t);
    mDesiredSize.width = NSIntPixelsToTwips(aParam1, p2t);
    mDesiredSize.height = NSIntPixelsToTwips(aParam2, p2t);
    mImageLoadStatus |= NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE;
    NotifyFrames();
    break;

  case nsImageNotification_kPixmapUpdate:
    if ((nsnull == mImage) && (nsnull != aImage)) {
      mImage = aImage;
      NS_ADDREF(aImage);
    }
    mImageLoadStatus |= NS_IMAGE_LOAD_STATUS_IMAGE_READY;

    // Convert the rect from pixels to twips
    mPresContext->GetScaledPixelsToTwips(&p2t);
    changeRect = (const nsRect*) aParam3;
    damageRect.x = NSIntPixelsToTwips(changeRect->x, p2t);
    damageRect.y = NSIntPixelsToTwips(changeRect->y, p2t);
    damageRect.width = NSIntPixelsToTwips(changeRect->width, p2t);
    damageRect.height = NSIntPixelsToTwips(changeRect->height, p2t);
    DamageRepairFrames(&damageRect);
    break;

  case nsImageNotification_kImageComplete:
    if ((nsnull == mImage) && (nsnull != aImage)) {
      mImage = aImage;
      NS_ADDREF(aImage);
      mImageLoadStatus |= NS_IMAGE_LOAD_STATUS_IMAGE_READY;
      DamageRepairFrames(nsnull);
    }
    break;

  case nsImageNotification_kFrameComplete:
    // New frame of a GIF animation
    // XXX Image library sends this for all images, and not just for animated
    // images. You bastards. It's a waste to re-draw the image if it's not an
    // animated image, but unfortunately we don't currently have a way to tell
    // whether the image is animated
    if (mImage) {
      DamageRepairFrames(nsnull);
    }
    break;

  case nsImageNotification_kIsTransparent:
    // Mark the frame's view as having transparent areas
    // XXX this is pretty vile
    pfd = mFrames;
    while (pfd) {
      pfd->mFrame->GetView(&view);
      if (view) {
        view->SetContentTransparency(PR_TRUE);
      }
      pfd = pfd->mNext;
    }
    break;

  case nsImageNotification_kAborted:
    // Treat this like an error
    mImageLoadError = nsImageError_kNoData;
    mImageLoadStatus |= NS_IMAGE_LOAD_STATUS_ERROR;
    NotifyFrames();
    break;

  default:
    break;
  }

  mNotifyLockCount--;
}

void
nsFrameImageLoader::NotifyError(nsIImageRequest *aImageRequest,
                                nsImageError aErrorType)
{
  mNotifyLockCount++;

  mImageLoadError = aErrorType;
  mImageLoadStatus |= NS_IMAGE_LOAD_STATUS_ERROR;
  NotifyFrames();

  mNotifyLockCount--;
}

void
nsFrameImageLoader::NotifyFrames()
{
  nsIPresShell* shell;
  mPresContext->GetShell(&shell);
  if (shell) {
    shell->EnterReflowLock();
  }

  PerFrameData* pfd = mFrames;
  while (nsnull != pfd) {
    if (pfd->mCallBack) {
      (*pfd->mCallBack)(mPresContext, this, pfd->mFrame, pfd->mClosure,
                        mImageLoadStatus);
    }
    pfd = pfd->mNext;
  }

  if (shell) {
    shell->ExitReflowLock();
    NS_RELEASE(shell);
  }
}

void
nsFrameImageLoader::DamageRepairFrames(const nsRect* aDamageRect)
{
  // Determine damaged area and tell view manager to redraw it
  nsPoint offset;
  nsRect bounds;
  nsIView* view;

  PerFrameData* pfd = mFrames;
  while (nsnull != pfd) {
    nsIFrame* frame = pfd->mFrame;

    if (nsnull == aDamageRect) {
      // Invalidate the entire frame
      // XXX We really only need to invalidate the clientg area of the frame...
      frame->GetRect(bounds);
      bounds.x = bounds.y = 0;
    }
    else {
      // aDamageRect represents the part of the content area that
      // needs updating.
      bounds = *aDamageRect;

      // Offset damage origin by border/padding
      // XXX This is pretty sleazy. See the XXX remark below...
      const nsStyleSpacing* space;
      nsMargin  borderPadding;
      frame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)space);
      space->CalcBorderPaddingFor(frame, borderPadding);
      bounds.MoveBy(borderPadding.left, borderPadding.top);
    }

    // XXX We should tell the frame the damage area and let it invalidate
    // itself. Add some API calls to nsIFrame to allow a caller to invalidate
    // parts of the frame...
    frame->GetView(&view);
    if (nsnull == view) {
      frame->GetOffsetFromView(offset, &view);
      bounds.x += offset.x;
      bounds.y += offset.y;
    }

    nsIViewManager* vm;
    view->GetViewManager(vm);
    vm->UpdateView(view, bounds, NS_VMREFRESH_NO_SYNC);
    NS_RELEASE(vm);

    pfd = pfd->mNext;
  }
}
