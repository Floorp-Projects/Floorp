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
#include "nsIPref.h"
#include "nsILinkHandler.h"
#include "nsIStyleSet.h"
#include "nsFrameImageLoader.h"
#include "nsIImageGroup.h"
#include "nsIFrame.h"
#include "nsIRenderingContext.h"
#include "nsEventStateManager.h"
#include "nsIURL.h"
#include "nsIURLGroup.h"
#include "nsIDocument.h"

#define NOISY_IMAGES

static NS_DEFINE_IID(kIPresContextIID, NS_IPRESCONTEXT_IID);

nsPresContext::nsPresContext()
  : mVisibleArea(0, 0, 0, 0),
    mDefaultFont("Times", NS_FONT_STYLE_NORMAL,
                 NS_FONT_VARIANT_NORMAL,
                 NS_FONT_WEIGHT_NORMAL,
                 0,
                 NSIntPointsToTwips(12)),
    mDefaultFixedFont("Courier", NS_FONT_STYLE_NORMAL,
                      NS_FONT_VARIANT_NORMAL,
                      NS_FONT_WEIGHT_NORMAL,
                      0,
                      NSIntPointsToTwips(10))
{
  NS_INIT_REFCNT();
  mShell = nsnull;
  mDeviceContext = nsnull;
  mPrefs = nsnull;
  mImageGroup = nsnull;
  mLinkHandler = nsnull;
  mContainer = nsnull;
  mEventManager = nsnull;

  mFontScaler = 0;  

  mCompatibilityMode = eCompatibility_NavQuirks;
  mBaseURL = nsnull;

  mDefaultColor = NS_RGB(0x00, 0x00, 0x00);
  mDefaultBackgroundColor = NS_RGB(0xFF, 0xFF, 0xFF);

#ifdef DEBUG
  mInitialized = PR_FALSE;
#endif
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

  if (nsnull != mImageGroup) {
    // Interrupt any loading images. This also stops all looping
    // image animations.
    mImageGroup->Interrupt();
    NS_RELEASE(mImageGroup);
  }

  NS_IF_RELEASE(mLinkHandler);
  NS_IF_RELEASE(mContainer);
  NS_IF_RELEASE(mEventManager);
  NS_IF_RELEASE(mDeviceContext);
  NS_IF_RELEASE(mPrefs);
  NS_IF_RELEASE(mBaseURL);
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

nsresult
nsPresContext::Init(nsIDeviceContext* aDeviceContext, nsIPref* aPrefs)
{
  NS_ASSERTION(!(mInitialized == PR_TRUE), "attempt to reinit pres context");

  mDeviceContext = aDeviceContext;
  NS_IF_ADDREF(mDeviceContext);

  mPrefs = aPrefs;
  NS_IF_ADDREF(mPrefs);

  if (nsnull != mPrefs) { // initialize pres context state from preferences
    int32 prefInt;
    char  prefChar[512];
    int   charSize = sizeof(prefChar);

    if (NS_OK == mPrefs->GetIntPref("browser.base_font_scaler", &prefInt)) {
      mFontScaler = prefInt;
    }

    // XXX these font prefs strings don't take font encoding into account
    if (NS_OK == mPrefs->GetCharPref("intl.font2.win.prop_font", &(prefChar[0]), &charSize)) {
      mDefaultFont.name = prefChar;
    }
    if (NS_OK == mPrefs->GetIntPref("intl.font2.win.prop_size", &prefInt)) {
      mDefaultFont.size = NSIntPointsToTwips(prefInt);
    }
    if (NS_OK == mPrefs->GetCharPref("intl.font2.win.fixed_font", &(prefChar[0]), &charSize)) {
      mDefaultFixedFont.name = prefChar;
    }
    if (NS_OK == mPrefs->GetIntPref("intl.font2.win.fixed_size", &prefInt)) {
      mDefaultFixedFont.size = NSIntPointsToTwips(prefInt);
    }
    if (NS_OK == mPrefs->GetIntPref("nglayout.compatibility.mode", &prefInt)) {
      mCompatibilityMode = (enum nsCompatibility)prefInt;  // bad cast
    }
  }

#ifdef DEBUG
  mInitialized = PR_TRUE;
#endif

  return NS_OK;
}

// Note: We don't hold a reference on the shell; it has a reference to
// us
void
nsPresContext::SetShell(nsIPresShell* aShell)
{
  NS_IF_RELEASE(mBaseURL);
  mShell = aShell;
  if (nsnull != mShell) {
    nsIDocument*  doc = mShell->GetDocument();
    NS_ASSERTION(nsnull != doc, "expect document here");
    if (nsnull != doc) {
      mBaseURL = doc->GetDocumentURL();
      NS_RELEASE(doc);
    }
  }
}

nsIPresShell*
nsPresContext::GetShell()
{
  NS_IF_ADDREF(mShell);
  return mShell;
}

NS_IMETHODIMP
nsPresContext::GetPrefs(nsIPref*& aPrefs)
{
  aPrefs = mPrefs;
  NS_IF_ADDREF(aPrefs);
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetCompatibilityMode(nsCompatibility& aMode)
{
  aMode = mCompatibilityMode;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::SetCompatibilityMode(nsCompatibility aMode)
{
  mCompatibilityMode = aMode;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetBaseURL(nsIURL*& aURL)
{
  NS_IF_ADDREF(mBaseURL);
  aURL = mBaseURL;
  return NS_OK;
}

nsIStyleContext*
nsPresContext::ResolveStyleContextFor(nsIContent* aContent,
                                      nsIStyleContext* aParentContext,
                                      PRBool aForceUnique)
{
  nsIStyleContext* result = nsnull;

  nsIStyleSet* set = mShell->GetStyleSet();
  if (nsnull != set) {
    result = set->ResolveStyleFor(this, aContent, aParentContext, aForceUnique);
    NS_RELEASE(set);
  }

  return result;
}

nsIStyleContext*
nsPresContext::ResolvePseudoStyleContextFor(nsIContent* aParentContent,
                                            nsIAtom* aPseudoTag,
                                            nsIStyleContext* aParentContext,
                                            PRBool aForceUnique)
{
  nsIStyleContext* result = nsnull;

  nsIStyleSet* set = mShell->GetStyleSet();
  if (nsnull != set) {
    result = set->ResolvePseudoStyleFor(this, aParentContent, aPseudoTag, aParentContext, aForceUnique);
    NS_RELEASE(set);
  }

  return result;
}

nsIStyleContext*
nsPresContext::ProbePseudoStyleContextFor(nsIContent* aParentContent,
                                          nsIAtom* aPseudoTag,
                                          nsIStyleContext* aParentContext,
                                          PRBool aForceUnique)
{
  nsIStyleContext* result = nsnull;

  nsIStyleSet* set = mShell->GetStyleSet();
  if (nsnull != set) {
    result = set->ProbePseudoStyleFor(this, aParentContent, aPseudoTag, aParentContext, aForceUnique);
    NS_RELEASE(set);
  }

  return result;
}

nsIFontMetrics*
nsPresContext::GetMetricsFor(const nsFont& aFont)
{
  if (nsnull != mDeviceContext) {
    nsIFontMetrics* metrics;

    mDeviceContext->GetMetricsFor(aFont, metrics);
    return metrics;
  }
  return nsnull;
}

const nsFont&
nsPresContext::GetDefaultFont(void)
{
  return mDefaultFont;
}

const nsFont&
nsPresContext::GetDefaultFixedFont(void)
{
  return mDefaultFixedFont;
}

PRInt32 
nsPresContext::GetFontScaler(void)
{
  return mFontScaler;
}

void 
nsPresContext::SetFontScaler(PRInt32 aScaler)
{
  mFontScaler = aScaler;
}

NS_IMETHODIMP
nsPresContext::GetDefaultColor(nscolor& aColor)
{
  aColor = mDefaultColor;
  return NS_OK;
}

NS_IMETHODIMP 
nsPresContext::GetDefaultBackgroundColor(nscolor& aColor)
{
  aColor = mDefaultBackgroundColor;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::SetDefaultColor(const nscolor& aColor)
{
  mDefaultColor = aColor;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::SetDefaultBackgroundColor(const nscolor& aColor)
{
  mDefaultBackgroundColor = aColor;
  return NS_OK;
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
    float p2t;
    mDeviceContext->GetDevUnitsToAppUnits(p2t);
    return p2t;
  }
  return 1.0f;
}

float
nsPresContext::GetTwipsToPixels() const
{
  if (nsnull != mDeviceContext) {
    float app2dev;
    mDeviceContext->GetAppUnitsToDevUnits(app2dev);
    return app2dev;
  }
  return 1.0f;
}

nsIDeviceContext*
nsPresContext::GetDeviceContext() const
{
  NS_IF_ADDREF(mDeviceContext);
  return mDeviceContext;
}

NS_IMETHODIMP
nsPresContext::GetImageGroup(nsIImageGroup*& aGroupResult)
{
  if (nsnull == mImageGroup) {
    // Create image group
    nsresult rv = NS_NewImageGroup(&mImageGroup);
    if (NS_OK != rv) {
      return rv;
    }


    // Initialize the image group
    nsIURLGroup* urlGroup = mBaseURL->GetURLGroup();
    rv = mImageGroup->Init(mDeviceContext, urlGroup);
    NS_IF_RELEASE(urlGroup);
    if (NS_OK != rv) {
      return rv;
    }
  }
  aGroupResult = mImageGroup;
  NS_IF_ADDREF(mImageGroup);
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::StartLoadImage(const nsString& aURL,
                              const nscolor* aBackgroundColor,
                              nsIFrame* aTargetFrame,
                              nsFrameImageLoaderCB aCallBack,
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

  rv = loader->Init(this, mImageGroup, aURL, aBackgroundColor, aTargetFrame,
                    aCallBack, aNeedSizeUpdate);
  if (NS_OK != rv) {
    mImageLoaders.RemoveElement(loader);
    NS_RELEASE(loader);
    return rv;
  }
  // Return the loader
  NS_ADDREF(loader);
  aLoaderResult = loader;
  return NS_OK;
}

NS_IMETHODIMP
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

NS_IMETHODIMP
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


