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

static NS_DEFINE_IID(kIFrameImageLoaderIID, NS_IFRAME_IMAGE_LOADER_IID);
static NS_DEFINE_IID(kIImageRequestObserverIID, NS_IIMAGEREQUESTOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

static PRLogModuleInfo* gFrameImageLoaderLMI;

NS_LAYOUT nsresult
NS_NewFrameImageLoader(nsIFrameImageLoader** aInstancePtrResult)
{
  nsFrameImageLoader* it = new nsFrameImageLoader();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIFrameImageLoaderIID,
                            (void**) aInstancePtrResult);
}

nsFrameImageLoader::nsFrameImageLoader()
{
  NS_INIT_REFCNT();
  mImage = nsnull;
  mSize.width = 0;
  mSize.height = 0;
  mError = (nsImageError) -1;
  mTargetFrame = nsnull;
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
    *aInstancePtr = (void*) ((nsIImageRequestObserver*) this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIFrameImageLoaderIID)) {
    *aInstancePtr = (void*) ((nsIFrameImageLoader*) this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*) this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsFrameImageLoader::Init(nsIPresContext* aPresContext,
                         nsIImageGroup* aGroup,
                         const nsString& aURL,
                         nsIFrame* aTargetFrame,
                         PRBool aNeedSizeUpdate)
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
  mPresContext = aPresContext;
  NS_IF_ADDREF(mPresContext);
  if (aNeedSizeUpdate) {
    mImageLoadStatus = NS_IMAGE_LOAD_STATUS_SIZE_REQUESTED;
  }

  // Translate url to a C string
  mURL = aURL;
  char* cp = aURL.ToNewCString();

  PR_LOG(gFrameImageLoaderLMI, PR_LOG_DEBUG,
    ("nsFrameImageLoader::Init start loading for '%s', frame=%p loadStatus=%x",
     cp ? cp : "(null)", mTargetFrame, mImageLoadStatus));

  // Start image load request
  mImageRequest = aGroup->GetImage(cp, this, NS_RGB(255,255,255), 0, 0, 0);
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

  switch (aNotificationType) {
  case nsImageNotification_kDimensions:
    mSize.width = aParam1;
    mSize.height = aParam2;
    mImageLoadStatus |= NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE;
    if (0 != (mImageLoadStatus & NS_IMAGE_LOAD_STATUS_SIZE_REQUESTED)) {
      ReflowFrame();
    }
    break;

  case nsImageNotification_kPixmapUpdate:
  case nsImageNotification_kImageComplete:
  case nsImageNotification_kFrameComplete:
    if ((mImage == nsnull) && (nsnull != aImage)) {
      mImage = aImage;
      NS_ADDREF(aImage);
    }
    mImageLoadStatus |= NS_IMAGE_LOAD_STATUS_IMAGE_READY;
    DamageRepairFrame();
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
  if (0 != (mImageLoadStatus & NS_IMAGE_LOAD_STATUS_SIZE_REQUESTED)) {
    ReflowFrame();
  }
  else {
    DamageRepairFrame();
  }
}

static PRBool gXXXInstalledColorMap;

void
nsFrameImageLoader::DamageRepairFrame()
{
  PR_LOG(gFrameImageLoaderLMI, PR_LOG_DEBUG,
         ("nsFrameImageLoader::DamageRepairFrame frame=%p status=%x",
          mTargetFrame, mImageLoadStatus));

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

  // Determine damaged area and tell view manager to redraw it
  nsPoint offset;
  nsRect bounds;
  nsIView* view;
  mTargetFrame->GetRect(bounds);
  mTargetFrame->GetOffsetFromView(offset, view);
  nsIViewManager* vm = view->GetViewManager();
  bounds.x = offset.x;
  bounds.y = offset.y;
  vm->UpdateView(view, bounds, 0);
  NS_RELEASE(vm);
  NS_RELEASE(view);
}

void
nsFrameImageLoader::ReflowFrame()
{
  PR_LOG(gFrameImageLoaderLMI, PR_LOG_DEBUG,
         ("nsFrameImageLoader::ReflowFrame frame=%p status=%x",
          mTargetFrame, mImageLoadStatus));

  nsIContent* content = nsnull;
  mTargetFrame->GetContent(content);
  if (nsnull != content) {
    nsIDocument* document = nsnull;
    content->GetDocument(document);
    if (nsnull != document) {
      document->ContentChanged(content, nsnull);
      NS_RELEASE(document);
    }
    NS_RELEASE(content);
  }
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

NS_IMETHODIMP
nsFrameImageLoader::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
  // XXX mImage
  // XXX mImageRequest
  return NS_OK;
}
