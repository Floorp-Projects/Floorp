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
#include "nsIPresContext.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsIFrame.h"
#include "nsIURL.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIImageGroup.h"
#include "nsIImageRequest.h"
#include "nsString.h"
#include "prlog.h"
#include "nsISizeOfHandler.h"
#include "nsIStyleContext.h"
#include "nsIDeviceContext.h"

static NS_DEFINE_IID(kIFrameImageLoaderIID, NS_IFRAME_IMAGE_LOADER_IID);
static NS_DEFINE_IID(kIImageRequestObserverIID, NS_IIMAGEREQUESTOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

static PRLogModuleInfo* gFrameImageLoaderLMI;

NS_LAYOUT nsresult
NS_NewFrameImageLoader(nsIFrameImageLoader** aResult)
{
  nsFrameImageLoader* it = new nsFrameImageLoader();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIFrameImageLoaderIID, (void**) aResult);
}

nsFrameImageLoader::nsFrameImageLoader()
{
  NS_INIT_REFCNT();
  mImage = nsnull;
  mSize.width = 0;
  mSize.height = 0;
  mError = (nsImageError) -1;
  mTargetFrame = nsnull;
  mCallBack = nsnull;
  mPresContext = nsnull;
  mImageRequest = nsnull;
  mImageLoadStatus = NS_IMAGE_LOAD_STATUS_NONE;
#ifdef NS_DEBUG
  if (nsnull == gFrameImageLoaderLMI) {
    gFrameImageLoaderLMI = PR_NewLogModule("frameimageloader");
  }
#endif
}

nsFrameImageLoader::~nsFrameImageLoader()
{
  NS_IF_RELEASE(mImageRequest);
  NS_IF_RELEASE(mPresContext);
  NS_IF_RELEASE(mImage);
}

NS_IMPL_ADDREF(nsFrameImageLoader)

NS_IMPL_RELEASE(nsFrameImageLoader)

NS_METHOD
nsFrameImageLoader::QueryInterface(REFNSIID aIID,
                                   void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIImageRequestObserverIID)) {
    nsIImageRequestObserver* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIFrameImageLoaderIID)) {
    nsIFrameImageLoader* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsISupports* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsFrameImageLoader::Init(nsIPresContext* aPresContext,
                         nsIImageGroup* aGroup,
                         const nsString& aURL,
                         const nscolor* aBackgroundColor,
                         nsIFrame* aTargetFrame,
                         nsFrameImageLoaderCB aCallBack,
                         PRBool aNeedSizeUpdate,
                         PRBool aNeedErrorNotification)
{
  NS_PRECONDITION(nsnull != aPresContext, "null ptr");
  NS_PRECONDITION(nsnull == mPresContext, "double init");

  if (nsnull == aPresContext) {
    return NS_ERROR_NULL_POINTER;
  }
  if (nsnull != mPresContext) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  mTargetFrame = aTargetFrame;
  mCallBack = aCallBack;
  mPresContext = aPresContext;
  NS_IF_ADDREF(mPresContext);
  if (aNeedSizeUpdate) {
    mImageLoadStatus = NS_IMAGE_LOAD_STATUS_SIZE_REQUESTED;
  }
  if (aNeedErrorNotification) {
    mImageLoadStatus |= NS_IMAGE_LOAD_STATUS_ERROR_REQUESTED;
  }

  // Translate url to a C string
  mURL = aURL;
  char* cp = aURL.ToNewCString();

  PR_LOG(gFrameImageLoaderLMI, PR_LOG_DEBUG,
    ("nsFrameImageLoader::Init start loading for '%s', frame=%p loadStatus=%x",
     cp ? cp : "(null)", mTargetFrame, mImageLoadStatus));

  // Start image load request
  mImageRequest = aGroup->GetImage(cp, this, aBackgroundColor, 0, 0, 0);
  delete cp;

  return NS_OK;
}

nsresult
nsFrameImageLoader::StopImageLoad()
{
  PR_LOG(gFrameImageLoaderLMI, PR_LOG_DEBUG,
         ("nsFrameImageLoader::StopImageLoad frame=%p loadStatus=%x",
          mTargetFrame, mImageLoadStatus));

  if (nsnull != mImageRequest) {
    mImageRequest->RemoveObserver(this);
    NS_RELEASE(mImageRequest);
  }
  return NS_OK;
}

nsresult
nsFrameImageLoader::AbortImageLoad()
{
  PR_LOG(gFrameImageLoaderLMI, PR_LOG_DEBUG,
         ("nsFrameImageLoader::AbortImageLoad frame=%p loadStatus=%x",
          mTargetFrame, mImageLoadStatus));

  if (nsnull != mImageRequest) {
    mImageRequest->Interrupt();
  }
  return NS_OK;
}

void
nsFrameImageLoader::Notify(nsIImageRequest *aImageRequest,
                           nsIImage *aImage,
                           nsImageNotification aNotificationType,
                           PRInt32 aParam1, PRInt32 aParam2,
                           void *aParam3)
{
  PR_LOG(gFrameImageLoaderLMI, PR_LOG_DEBUG,
         ("nsFrameImageLoader::Notify frame=%p type=%x p1=%x p2=%x p3=%x",
          mTargetFrame, aNotificationType, aParam1, aParam2, aParam3));

  nsIView*      view;
  nsRect        damageRect;
  float         p2t;
  const nsRect* changeRect;

  switch (aNotificationType) {
  case nsImageNotification_kDimensions:
    mSize.width = aParam1;
    mSize.height = aParam2;
    mImageLoadStatus |= NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE;
    if (0 != (mImageLoadStatus & NS_IMAGE_LOAD_STATUS_SIZE_REQUESTED)) {
      if (nsnull != mCallBack) {
        (*mCallBack)(*mPresContext, mTargetFrame, mImageLoadStatus);
      }
    }
    break;

  case nsImageNotification_kPixmapUpdate:
    if ((nsnull == mImage) && (nsnull != aImage)) {
      mImage = aImage;
      NS_ADDREF(aImage);
    }
    mImageLoadStatus |= NS_IMAGE_LOAD_STATUS_IMAGE_READY;

    // Convert the rect from pixels to twips
    mPresContext->GetScaledPixelsToTwips(p2t);
    changeRect = (const nsRect*)aParam3;
    damageRect.x = NSIntPixelsToTwips(changeRect->x, p2t);
    damageRect.y = NSIntPixelsToTwips(changeRect->y, p2t);
    damageRect.width = NSIntPixelsToTwips(changeRect->width, p2t);
    damageRect.height = NSIntPixelsToTwips(changeRect->height, p2t);
    DamageRepairFrame(&damageRect);
    break;

  case nsImageNotification_kImageComplete:
    // We expect to have gotten at least one pixmap update.
#if 0
    // XXX Not true. Damn image library...
    NS_ASSERTION((nsnull != mImage) &&
                 (mImageLoadStatus & NS_IMAGE_LOAD_STATUS_IMAGE_READY),
                 "unexpected notification order");
#else
    if ((nsnull == mImage) && (nsnull != aImage)) {
      mImage = aImage;
      NS_ADDREF(aImage);
      mImageLoadStatus |= NS_IMAGE_LOAD_STATUS_IMAGE_READY;
      DamageRepairFrame(nsnull);
    }
#endif
    break;

  case nsImageNotification_kFrameComplete:
    // New frame of a GIF animation
    // XXX Image library sends this for all images, and not just for animated
    // images. You bastards. It's a waste to re-draw the image if it's not an
    // animated image, but unfortunately we don't currently have a way to tell
    // whether the image is animated
    DamageRepairFrame(nsnull);
    break;

  case nsImageNotification_kIsTransparent:
    // Mark the frame's view as having transparent areas
    mTargetFrame->GetView(&view);
    if (nsnull != view) {
      view->SetContentTransparency(PR_TRUE);
    }
    break;

  case nsImageNotification_kAborted:
    // Treat this like an error
    NotifyError(aImageRequest, nsImageError_kNoData);
    break;
  }
}

void
nsFrameImageLoader::NotifyError(nsIImageRequest *aImageRequest,
                                nsImageError aErrorType)
{
  PR_LOG(gFrameImageLoaderLMI, PR_LOG_DEBUG,
         ("nsFrameImageLoader::NotifyError frame=%p error=%x",
          mTargetFrame, aErrorType));

  mError = aErrorType;
  mImageLoadStatus |= NS_IMAGE_LOAD_STATUS_ERROR;
  if (0 != (mImageLoadStatus & NS_IMAGE_LOAD_STATUS_ERROR_REQUESTED)) {
    if (nsnull != mCallBack) {
      (*mCallBack)(*mPresContext, mTargetFrame, mImageLoadStatus);
    }
  }
  else {
    DamageRepairFrame(nsnull);
  }
}

static PRBool gXXXInstalledColorMap;

void
nsFrameImageLoader::DamageRepairFrame(const nsRect* aDamageRect)
{
  PR_LOG(gFrameImageLoaderLMI, PR_LOG_DEBUG,
         ("nsFrameImageLoader::DamageRepairFrame frame=%p status=%x",
          mTargetFrame, mImageLoadStatus));

#if 0
  // XXX I #if 0'd this per troy's instruction... MMP
  // XXX this should be done somewhere else, like when the window
  // is created or something???
  // XXX maybe there should be a seperate notification service for
  // colormap events?
  nsIWidget* window;
  mTargetFrame->GetWindow(window);
  if (!gXXXInstalledColorMap && mImage) {
    nsColorMap* cmap = mImage->GetColorMap();
    if ((nsnull != cmap) && (cmap->NumColors > 0)) {
      window->SetColorMap(cmap);
    }
    gXXXInstalledColorMap = PR_TRUE;
  }
  NS_RELEASE(window);
#endif

  // Determine damaged area and tell view manager to redraw it
  nsPoint offset;
  nsRect bounds;
  nsIView* view;

  if (nsnull == aDamageRect) {
    // Invalidate the entire frame
    // XXX We really only need to invalidate the clientg area of the frame...
    mTargetFrame->GetRect(bounds);
    bounds.x = bounds.y = 0;
  }
  else {
    // aDamageRect represents the part of the content area that needs
    // updating
    bounds = *aDamageRect;

    // Offset damage origin by border/padding
    // XXX This is pretty sleazy. See the XXX remark below...
    const nsStyleSpacing* space;
    nsMargin  borderPadding;

    mTargetFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)space);
    space->CalcBorderPaddingFor(mTargetFrame, borderPadding);
    bounds.MoveBy(borderPadding.left, borderPadding.top);
  }

  // XXX We should tell the frame the damage area and let it invalidate
  // itself. Add some API calls to nsIFrame to allow a caller to invalidate
  // parts of the frame...
  mTargetFrame->GetView(&view);
  if (nsnull == view) {
    mTargetFrame->GetOffsetFromView(offset, &view);
    bounds.x += offset.x;
    bounds.y += offset.y;
  }
  // XXX At least for the time being don't allow a synchronous repaint, because
  // we may already be repainting and we don't want to go re-entrant...
  nsIViewManager* vm;
  view->GetViewManager(vm);
  vm->UpdateView(view, bounds, NS_VMREFRESH_NO_SYNC);
  NS_RELEASE(vm);
}

NS_METHOD
nsFrameImageLoader::GetTargetFrame(nsIFrame*& aFrameResult) const
{
  aFrameResult = mTargetFrame;
  return NS_OK;
}

NS_METHOD
nsFrameImageLoader::GetURL(nsString& aResult) const
{
  aResult = mURL;
  return NS_OK;
}

NS_METHOD
nsFrameImageLoader::GetImage(nsIImage*& aResult) const
{
  aResult = mImage;
  return NS_OK;
}

NS_METHOD
nsFrameImageLoader::GetSize(nsSize& aResult) const
{
  aResult = mSize;
  return NS_OK;
}

NS_METHOD
nsFrameImageLoader::GetImageLoadStatus(PRIntn& aLoadStatus) const
{
  aLoadStatus = mImageLoadStatus;
  return NS_OK;
}
