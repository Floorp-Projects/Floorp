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
#include "nsILinkHandler.h"
#include "nsIStyleSet.h"
#include "nsFrameImageLoader.h"
#include "nsIImageGroup.h"
#include "nsIFrame.h"
#include "nsIRenderingContext.h"
#include "nsEventStateManager.h"

#define NOISY_IMAGES

static NS_DEFINE_IID(kIPresContextIID, NS_IPRESCONTEXT_IID);

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
  mEventManager = nsnull;
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
    nsIFrameImageLoader* loader;
    loader = (nsIFrameImageLoader*) mImageLoaders.ElementAt(i);
    NS_RELEASE(loader);
  }

  // XXX Tell the image library we are done with the cached images?

  if (nsnull != mImageGroup) {
    /*XXX mImageGroup->Interrupt(); required? */
    NS_RELEASE(mImageGroup);
  }

  NS_IF_RELEASE(mLinkHandler);
  NS_IF_RELEASE(mContainer);
  NS_IF_RELEASE(mEventManager);
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
                                      nsIFrame* aParentFrame,
                                      PRBool aForceUnique)
{
  nsIStyleContext* result = nsnull;

  nsIStyleSet* set = mShell->GetStyleSet();
  if (nsnull != set) {
    result = set->ResolveStyleFor(this, aContent, aParentFrame, aForceUnique);
    NS_RELEASE(set);
  }

  return result;
}

nsIStyleContext*
nsPresContext::ResolvePseudoStyleContextFor(nsIAtom* aPseudoTag,
                                            nsIFrame* aParentFrame,
                                            PRBool aForceUnique)
{
  nsIStyleContext* result = nsnull;

  nsIStyleSet* set = mShell->GetStyleSet();
  if (nsnull != set) {
    result = set->ResolvePseudoStyleFor(this, aPseudoTag, aParentFrame, aForceUnique);
    NS_RELEASE(set);
  }

  return result;
}

nsIStyleContext*
nsPresContext::ProbePseudoStyleContextFor(nsIAtom* aPseudoTag,
                                          nsIFrame* aParentFrame,
                                          PRBool aForceUnique)
{
  nsIStyleContext* result = nsnull;

  nsIStyleSet* set = mShell->GetStyleSet();
  if (nsnull != set) {
    result = set->ProbePseudoStyleFor(this, aPseudoTag, aParentFrame, aForceUnique);
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

void
nsPresContext::GetVisibleArea(nsRect& aResult)
{
  aResult = mVisibleArea;
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
nsPresContext::GetImageGroup(nsIImageGroup*& aGroupResult)
{
  if (nsnull == mImageGroup) {
    // Create image group
    nsresult rv = NS_NewImageGroup(&mImageGroup);
    if (NS_OK != rv) {
      return rv;
    }

    // Initialize the image group
    nsIWidget* window;
    nsIFrame* rootFrame;
    rootFrame = mShell->GetRootFrame();
    rootFrame->GetWindow(window);
    nsIRenderingContext* drawCtx = window->GetRenderingContext();
    drawCtx->Scale(mDeviceContext->GetAppUnitsToDevUnits(),
                   mDeviceContext->GetAppUnitsToDevUnits());
    rv = mImageGroup->Init(drawCtx);
    NS_RELEASE(drawCtx);
    NS_RELEASE(window);
    if (NS_OK != rv) {
      return rv;
    }
  }
  aGroupResult = mImageGroup;
  NS_IF_ADDREF(mImageGroup);
  return NS_OK;
}

NS_METHOD
nsPresContext::LoadImage(const nsString& aURL,
                         nsIFrame* aTargetFrame,
                         PRBool aNeedSizeUpdate,
                         nsIFrameImageLoader*& aLoaderResult)
{
  aLoaderResult = nsnull;

  // Lookup image request in our loaders array. Note that we need
  // to get a loader that is observing the same image and that has
  // the same value for aNeedSizeUpdate.
  PRInt32 i, n = mImageLoaders.Count();
  nsIFrameImageLoader* loader;
  nsAutoString loaderURL;
  for (i = 0; i < n; i++) {
    loader = (nsIFrameImageLoader*) mImageLoaders.ElementAt(i);
    nsIFrame* targetFrame;
    loader->GetTargetFrame(targetFrame);
    if (targetFrame == aTargetFrame) {
      loader->GetURL(loaderURL);
      // XXX case doesn't matter for all of the url so using Equals
      // isn't quite right
      if (aURL.Equals(loaderURL)) {
        // Make sure the size update flags are the same, if not keep looking
        PRIntn status;
        loader->GetImageLoadStatus(status);
        PRBool wantSize = 0 != (status & NS_IMAGE_LOAD_STATUS_SIZE_REQUESTED);
        if (aNeedSizeUpdate == wantSize) {
          NS_ADDREF(loader);
          aLoaderResult = loader;
          return NS_OK;
        }
      }
    }
  }

  // Create image group if needed
  nsresult rv;
  if (nsnull == mImageGroup) {
    nsIImageGroup* group;
    rv = GetImageGroup(group);
    if (NS_OK != rv) {
      return rv;
    }
    NS_RELEASE(group);
  }

  // We haven't seen that image before. Create a new loader and
  // start it going.
  rv = NS_NewFrameImageLoader(&loader);
  if (NS_OK != rv) {
    return rv;
  }
  mImageLoaders.AppendElement(loader);

  // Return new loader
  NS_ADDREF(loader);

  rv = loader->Init(this, mImageGroup, aURL, aTargetFrame, aNeedSizeUpdate);
  if (NS_OK != rv) {
    return rv;
  }
  aLoaderResult = loader;
  return NS_OK;
}

NS_METHOD
nsPresContext::StopLoadImage(nsIFrame* aForFrame)
{
  nsIFrameImageLoader* loader;
  PRInt32 i, n = mImageLoaders.Count();
  for (i = 0; i < n;) {
    loader = (nsIFrameImageLoader*) mImageLoaders.ElementAt(i);
    nsIFrame* loaderFrame;
    loader->GetTargetFrame(loaderFrame);
    if (loaderFrame == aForFrame) {
      loader->StopImageLoad();
      NS_RELEASE(loader);
      mImageLoaders.RemoveElementAt(i);
      n--;
      continue;
    }
    i++;
  }
  return NS_OK;
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

NS_METHOD
nsPresContext::GetEventStateManager(nsIEventStateManager** aManager)
{
  NS_PRECONDITION(nsnull != aManager, "null ptr");
  if (nsnull == aManager) {
    return NS_ERROR_NULL_POINTER;
  }

  if (nsnull == mEventManager) {
    nsresult rv = NS_NewEventStateManager(&mEventManager);
    if (NS_OK != rv) {
      return rv;
    }
  }

  *aManager = mEventManager;
  NS_IF_ADDREF(mEventManager);
  return NS_OK;
}

