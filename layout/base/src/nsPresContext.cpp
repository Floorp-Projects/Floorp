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
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsILinkHandler.h"
#include "nsIStyleContext.h"
#include "nsIStyleSet.h"
#include "nsIFontMetrics.h"
#include "nsIImageManager.h"
#include "nsIImageGroup.h"
#include "nsIImageRequest.h"
#include "nsIImageObserver.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIFrame.h"
#include "nsIRenderingContext.h"
#include "nsTransform2D.h"
#include "nsIDocument.h"
#include "nsIURL.h"
#include "nsString.h"

#define NOISY

static NS_DEFINE_IID(kIPresContextIID, NS_IPRESCONTEXT_IID);

class ImageLoader :  public nsIImageRequestObserver {
public:
  ImageLoader(nsPresContext* aCX, nsIImageGroup* aGroup, nsIFrame* aFrame);
  ~ImageLoader();

  NS_DECL_ISUPPORTS

  void LoadImage(const nsString& aURL);

  const nsString& GetURL() const {
    return mURL;
  }

  nsIImage* GetImage() {
    return mImage;
  }

  PRBool GetImageSize(nsSize& aSize) {
    if ((mSize.width > 0) && (mSize.height > 0)) {
      aSize = mSize;
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  PRInt32 FrameCount() const {
    return mFrames.Count();
  }

  PRBool RemoveFrame(nsIFrame* aFrame);

  void AddFrame(nsIFrame* aFrame);

  void DamageRepairFrames();

  void ReflowFrames();

  virtual void Notify(nsIImageRequest *aImageRequest,
                      nsIImage *aImage,
                      nsImageNotification aNotificationType,
                      PRInt32 aParam1, PRInt32 aParam2,
                      void *aParam3);

  virtual void NotifyError(nsIImageRequest *aImageRequest,
                           nsImageError aErrorType);

  nsVoidArray mFrames;
  nsString mURL;
  nsPresContext* mCX;
  nsIImage* mImage;
  nsIImageRequest* mImageRequest;
  nsIImageGroup* mImageGroup;
  nsSize mSize;              // Note: in pixels
  nsImageError mError;
};

ImageLoader::ImageLoader(nsPresContext* aCX, nsIImageGroup* aGroup,
                         nsIFrame* aFrame)
  : mSize(-1, -1)
{
  mCX = aCX;
  AddFrame(aFrame);
  mImage = nsnull;
  mImageRequest = nsnull;
  mImageGroup = aGroup; NS_ADDREF(aGroup);
}

ImageLoader::~ImageLoader()
{
  if (nsnull != mImageRequest) {
    mImageRequest->Interrupt();
    mImageRequest->RemoveObserver(this);
  }
}

void
ImageLoader::LoadImage(const nsString& aURL)
{
  if (nsnull == mImage) {
    // Save url
    mURL.SetLength(0);
    mURL.Append(aURL);

    // Start image load
    char* cp = aURL.ToNewCString();
    mImageRequest = mImageGroup->GetImage(cp, this, NS_RGB(255,255,255),
                                          0, 0, 0);
    delete cp;
  }
}

static NS_DEFINE_IID(kIImageRequestObserver, NS_IIMAGEREQUESTOBSERVER_IID);

NS_IMPL_ISUPPORTS(ImageLoader, kIImageRequestObserver);

static PRBool gXXXInstalledColorMap;
void
ImageLoader::Notify(nsIImageRequest *aImageRequest,
                    nsIImage *aImage,
                    nsImageNotification aNotificationType,
                    PRInt32 aParam1, PRInt32 aParam2,
                    void *aParam3)
{
  switch (aNotificationType) {
  case nsImageNotification_kDimensions:
#ifdef NOISY
    printf("Image:%d x %d\n", aParam1, aParam2);
#endif
    mSize.width = aParam1;
    mSize.height = aParam2;
    ReflowFrames();
    break;

  case nsImageNotification_kPixmapUpdate:
  case nsImageNotification_kImageComplete:
  case nsImageNotification_kFrameComplete:
    if ((mImage == nsnull) && (nsnull != aImage)) {
      mImage = aImage;
      NS_ADDREF(aImage);
    }
    DamageRepairFrames();
    break;
  }
}

void
ImageLoader::NotifyError(nsIImageRequest *aImageRequest,
                         nsImageError aErrorType)
{
  mError = aErrorType;
  DamageRepairFrames();
}

PRBool
ImageLoader::RemoveFrame(nsIFrame* aFrame)
{
  return mFrames.RemoveElement(aFrame);
}

void
ImageLoader::AddFrame(nsIFrame* aFrame)
{
  PRInt32 i, n = mFrames.Count();
  for (i = 0; i < n; i++) {
    nsIFrame* frame = (nsIFrame*) mFrames.ElementAt(i);
    if (frame == aFrame) {
      return;
    }
  }
  mFrames.AppendElement(aFrame);
}

void
ImageLoader::DamageRepairFrames()
{
  PRInt32 i, n = mFrames.Count();
  for (i = 0; i < n; i++) {
    nsIFrame* frame = (nsIFrame*) mFrames.ElementAt(i);

    // XXX this should be done somewhere else, like when the window
    // is created or something???
    // XXX maybe there should be a seperate notification service for
    // colormap events?
    nsIWidget* window;
    frame->GetWindow(window);
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
    frame->GetRect(bounds);
    frame->GetOffsetFromView(offset, view);
    nsIViewManager* vm = view->GetViewManager();
    bounds.x = offset.x;
    bounds.y = offset.y;
    vm->UpdateView(view, bounds, 0);
    NS_RELEASE(vm);
    NS_RELEASE(view);
  }
}

void
ImageLoader::ReflowFrames()
{
  PRInt32 i, n = mFrames.Count();
  mCX->BeginLoadImageUpdate();
  for (i = 0; i < n; i++) {
    nsIFrame* frame = (nsIFrame*) mFrames.ElementAt(i);
    mCX->ImageUpdate(frame);
  }
  mCX->EndLoadImageUpdate();
}

//----------------------------------------------------------------------

nsPresContext::nsPresContext()
  : mVisibleArea(0, 0, 0, 0),
    mDefaultFont("Times", NS_FONT_STYLE_NORMAL,
                 NS_FONT_VARIANT_NORMAL,
                 NS_FONT_WEIGHT_NORMAL,
                 0,
                 NS_POINTS_TO_TWIPS_INT(12))
{
  NS_INIT_REFCNT();
  mShell = nsnull;
  mDeviceContext = nsnull;
  mImageGroup = nsnull;
  mLinkHandler = nsnull;
  mContainer = nsnull;
  mImageUpdates = 0;
}

nsPresContext::~nsPresContext()
{
  mShell = nsnull;

  // XXX there is a race between an async notify and this code because
  // the presentation shell code deletes the frame tree first and then
  // deletes us. We need an "Deactivation" hook for this code too

  // Release all the image loaders
  PRInt32 n = mImageLoaders.Count();
  for (PRInt32 i = 0; i < n; i++) {
    ImageLoader* loader = (ImageLoader*) mImageLoaders.ElementAt(i);
    delete loader;
  }

  // XXX Release all the cached images

  if (nsnull != mImageGroup) {
    mImageGroup->Interrupt();
    NS_RELEASE(mImageGroup);
  }

  NS_IF_RELEASE(mLinkHandler);
  NS_IF_RELEASE(mContainer);
}

nsrefcnt
nsPresContext::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt
nsPresContext::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "bad refcnt");
  if (--mRefCnt == 0) {
    delete this;
    return 0;
  }
  return mRefCnt;
}

NS_IMPL_QUERY_INTERFACE(nsPresContext, kIPresContextIID);

// Note: We don't hold a reference on the shell; it has a reference to
// us
void
nsPresContext::SetShell(nsIPresShell* aShell)
{
  mShell = aShell;
}

nsIPresShell*
nsPresContext::GetShell()
{
  NS_IF_ADDREF(mShell);
  return mShell;
}

nsIStyleContext*
nsPresContext::ResolveStyleContextFor(nsIContent* aContent,
                                      nsIFrame* aParentFrame)
{
  nsIStyleContext* result = nsnull;

  nsIStyleSet* set = mShell->GetStyleSet();
  if (nsnull != set) {
    result = set->ResolveStyleFor(this, aContent, aParentFrame);
    NS_RELEASE(set);
  }

  return result;
}

nsIFontMetrics*
nsPresContext::GetMetricsFor(const nsFont& aFont)
{
  if (nsnull != mDeviceContext)
    return mDeviceContext->GetMetricsFor(aFont);
  return nsnull;
}

const nsFont&
nsPresContext::GetDefaultFont(void)
{
  return mDefaultFont;
}

nsRect
nsPresContext::GetVisibleArea()
{
  return mVisibleArea;
}

void
nsPresContext::SetVisibleArea(const nsRect& r)
{
  mVisibleArea = r;
}

float
nsPresContext::GetPixelsToTwips() const
{
  if (nsnull != mDeviceContext) {
    return mDeviceContext->GetDevUnitsToAppUnits();
  }
  return 1.0f;
}

float
nsPresContext::GetTwipsToPixels() const
{
  if (nsnull != mDeviceContext) {
    return mDeviceContext->GetAppUnitsToDevUnits();
  }
  return 1.0f;
}

nsIDeviceContext*
nsPresContext::GetDeviceContext() const
{
  NS_IF_ADDREF(mDeviceContext);
  return mDeviceContext;
}

NS_METHOD
nsPresContext::LoadImage(const nsString& aURL,
                         nsIFrame* aForFrame,
                         PRInt32& aLoadImageStatus,
                         nsImageError& aError,
                         nsSize& aImageSize,
                         nsIImage*& aImage)
{
  aLoadImageStatus = 0;
  nsresult rv;
  if (nsnull == mImageGroup) {
    // Create image group
    rv = NS_NewImageGroup(&mImageGroup);
    if (NS_OK != rv) {
      return rv;
    }

    // Initialize the image group
    nsIWidget* window;
    aForFrame->GetWindow(window);
    nsIRenderingContext* drawCtx = window->GetRenderingContext();
    drawCtx->Scale(mDeviceContext->GetAppUnitsToDevUnits(),
                   mDeviceContext->GetAppUnitsToDevUnits());
    rv = mImageGroup->Init(drawCtx);
    NS_RELEASE(drawCtx);
    if (NS_OK != rv) {
      return rv;
    }
  }

  // Get absolute version of the url
  nsIDocument* doc = mShell->GetDocument();
  nsIURL* docURL = doc->GetDocumentURL();
  char* spec = aURL.ToNewCString();
  nsIURL* url;
  rv = NS_NewURL(&url, docURL, spec);
  delete spec;
  NS_RELEASE(docURL);
  NS_RELEASE(doc);
  if (NS_OK != rv) {
    return rv;
  }
  nsAutoString absURL;
  url->ToString(absURL);
  NS_RELEASE(url);

  // Lookup image request in our loaders array
  PRInt32 i, n = mImageLoaders.Count();
  ImageLoader* loader;
  for (i = 0; i < n; i++) {
    loader = (ImageLoader*) mImageLoaders.ElementAt(i);
    if (absURL.Equals(loader->GetURL())) {
      loader->AddFrame(aForFrame);
      goto done;
    }
  }

  // We haven't seen that image before. Create a new loader and
  // start it going.
  loader = new ImageLoader(this, mImageGroup, aForFrame);
  if (nsnull == loader) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mImageLoaders.AppendElement(loader);
  loader->LoadImage(absURL);

done:
  aLoadImageStatus = 0;
  if (loader->GetImageSize(aImageSize)) {
    aLoadImageStatus |= NS_LOAD_IMAGE_STATUS_SIZE;
  }
  aImage = loader->GetImage();
  if (nsnull != aImage) {
    aLoadImageStatus |= NS_LOAD_IMAGE_STATUS_BITS;
  }
  return NS_OK;
}

NS_METHOD
nsPresContext::StopLoadImage(nsIFrame* aForFrame)
{
  PRInt32 i, n = mImageLoaders.Count();
  for (i = 0; i < n;) {
    ImageLoader* loader = (ImageLoader*) mImageLoaders.ElementAt(i);
    if (loader->RemoveFrame(aForFrame)) {
      if (0 == loader->FrameCount()) {
        delete loader;
        mImageLoaders.RemoveElementAt(i);
        n--;
        continue;
      }
    }
    i++;
  }
  return NS_OK;
}

void
nsPresContext::BeginLoadImageUpdate()
{
  ++mImageUpdates;
}

void
nsPresContext::ImageUpdate(nsIFrame* aFrame)
{
  nsIContent* content;
  aFrame->GetContent(content);
  mPendingImageUpdates.AppendElement(content);
  if (0 == mImageUpdates) {
    ProcessLoadImageUpdates();
  }
}

void
nsPresContext::EndLoadImageUpdate()
{
  if (--mImageUpdates == 0) {
    ProcessLoadImageUpdates();
  }
}

void
nsPresContext::ProcessLoadImageUpdates()
{
  PRInt32 i, n = mPendingImageUpdates.Count();
  mShell->EnterReflowLock();
  mShell->BeginUpdate();
  for (i = 0; i < n; i++) {
    nsIContent* content = (nsIContent*) mPendingImageUpdates.ElementAt(i);
    mShell->ContentChanged(content, nsnull);
    NS_RELEASE(content);
  }
  mPendingImageUpdates.Clear();
  mShell->EndUpdate();
  mShell->ExitReflowLock();
}

NS_IMETHODIMP
nsPresContext::SetLinkHandler(nsILinkHandler* aHandler)
{ // XXX should probably be a WEAK reference
  NS_IF_RELEASE(mLinkHandler);
  mLinkHandler = aHandler;
  NS_IF_ADDREF(aHandler);
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetLinkHandler(nsILinkHandler** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mLinkHandler;
  NS_IF_ADDREF(mLinkHandler);
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::SetContainer(nsISupports* aHandler)
{ // XXX should most likely be a WEAK reference
  NS_IF_RELEASE(mContainer);
  mContainer = aHandler;
  NS_IF_ADDREF(aHandler);
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetContainer(nsISupports** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mContainer;
  NS_IF_ADDREF(mContainer);
  return NS_OK;
}
