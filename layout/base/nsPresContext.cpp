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
#include "nsIFrame.h"
#include "nsIStyleContext.h"
#include "nsLayoutAtoms.h"
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef DEBUG
#undef NOISY_IMAGES
#else
#undef NOISY_IMAGES
#endif

int
PrefChangedCallback(const char* aPrefName, void* instance_data)
{
  nsPresContext*  presContext = (nsPresContext*)instance_data;

  NS_ASSERTION(nsnull != presContext, "bad instance data");
  if (nsnull != presContext) {
    presContext->PreferenceChanged(aPrefName);
  }
  return 0;  // PREF_OK
}

static NS_DEFINE_IID(kIPresContextIID, NS_IPRESCONTEXT_IID);

nsPresContext::nsPresContext()
  : mDefaultFont("Times", NS_FONT_STYLE_NORMAL,
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
  nsLayoutAtoms::AddrefAtoms();
  mCompatibilityMode = eCompatibility_NavQuirks;

#ifdef _WIN32
  // XXX This needs to be elsewhere, e.g., part of nsIDeviceContext
  mDefaultColor = ::GetSysColor(COLOR_WINDOWTEXT);
  mDefaultBackgroundColor = ::GetSysColor(COLOR_WINDOW);
#else
  mDefaultColor = NS_RGB(0x00, 0x00, 0x00);
  mDefaultBackgroundColor = NS_RGB(0xFF, 0xFF, 0xFF);
#endif
}

nsPresContext::~nsPresContext()
{
  mShell = nsnull;

  Stop();

  if (nsnull != mImageGroup) {
    // Interrupt any loading images. This also stops all looping
    // image animations.
    mImageGroup->Interrupt();
    NS_RELEASE(mImageGroup);
  }

  NS_IF_RELEASE(mLinkHandler);
  NS_IF_RELEASE(mContainer);

  if (nsnull != mEventManager) {
    mEventManager->SetPresContext(nsnull);
    NS_RELEASE(mEventManager);
  }

  NS_IF_RELEASE(mDeviceContext);
  // Unregister preference callbacks
  if (nsnull != mPrefs) {
    mPrefs->UnregisterCallback("browser.", PrefChangedCallback, (void*)this);
    mPrefs->UnregisterCallback("intl.font2.", PrefChangedCallback, (void*)this);
  }
  NS_IF_RELEASE(mPrefs);
  NS_IF_RELEASE(mBaseURL);
  nsLayoutAtoms::ReleaseAtoms();
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

void
nsPresContext::GetUserPreferences()
{
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

  PRBool usePrefColors = PR_TRUE;
#ifdef _WIN32
  XP_Bool boolPref;
  // XXX Is Windows the only platform that uses this?
  if (NS_OK == mPrefs->GetBoolPref("browser.wfe.use_windows_colors", &boolPref)) {
    usePrefColors = !PRBool(boolPref);
  }
#endif
  if (usePrefColors) {
    uint32  colorPref;
    if (NS_OK == mPrefs->GetColorPrefDWord("browser.foreground_color", &colorPref)) {
      mDefaultColor = (nscolor)colorPref;
    }
    if (NS_OK == mPrefs->GetColorPrefDWord("browser.background_color", &colorPref)) {
      mDefaultBackgroundColor = (nscolor)colorPref;
    }
  }
}

void
nsPresContext::PreferenceChanged(const char* aPrefName)
{
  // Initialize our state from the user preferences
  GetUserPreferences();

  // Have the root frame's style context remap its style based on the
  // user preferences
  nsIFrame*         rootFrame;
  nsIStyleContext*  rootStyleContext;

  mShell->GetRootFrame(rootFrame);
  if (nsnull != rootFrame) {
    rootFrame->GetStyleContext(&rootStyleContext);
    rootStyleContext->RemapStyle(this);
    NS_RELEASE(rootStyleContext);
  
    // Force a reflow of the root frame
    // XXX We really should only do a reflow if a preference that affects
    // formatting changed, e.g., a font change. If it's just a color change
    // then we only need to repaint...
    mShell->StyleChangeReflow();
  }
}

NS_IMETHODIMP
nsPresContext::Init(nsIDeviceContext* aDeviceContext, nsIPref* aPrefs)
{
  NS_ASSERTION(!(mInitialized == PR_TRUE), "attempt to reinit pres context");

  mDeviceContext = aDeviceContext;
  NS_IF_ADDREF(mDeviceContext);

  mPrefs = aPrefs;
  NS_IF_ADDREF(mPrefs);

  if (nsnull != mPrefs) {
    // Register callbacks so we're notified when the preferences change
    mPrefs->RegisterCallback("browser.", PrefChangedCallback, (void*)this);
    mPrefs->RegisterCallback("intl.font2.", PrefChangedCallback, (void*)this);

    // Initialize our state from the user preferences
    GetUserPreferences();
  }

#ifdef DEBUG
  mInitialized = PR_TRUE;
#endif

  return NS_OK;
}

// Note: We don't hold a reference on the shell; it has a reference to
// us
NS_IMETHODIMP
nsPresContext::SetShell(nsIPresShell* aShell)
{
  NS_IF_RELEASE(mBaseURL);
  mShell = aShell;
  if (nsnull != mShell) {
    nsIDocument*  doc = mShell->GetDocument();
    NS_ASSERTION(nsnull != doc, "expect document here");
    if (nsnull != doc) {
      doc->GetBaseURL(mBaseURL);
      NS_RELEASE(doc);
    }
  }
  return NS_OK;
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

NS_IMETHODIMP
nsPresContext::GetFontScaler(PRInt32& aResult)
{
  aResult = mFontScaler;
  return NS_OK;
}

NS_IMETHODIMP 
nsPresContext::SetFontScaler(PRInt32 aScaler)
{
  mFontScaler = aScaler;
  return NS_OK;
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

NS_IMETHODIMP
nsPresContext::GetVisibleArea(nsRect& aResult)
{
  aResult = mVisibleArea;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::SetVisibleArea(const nsRect& r)
{
  mVisibleArea = r;
  return NS_OK;
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

NS_IMETHODIMP nsPresContext :: GetScaledPixelsToTwips(float &aScale) const
{
  if (nsnull != mDeviceContext)
  {
    float p2t, scale;
    mDeviceContext->GetDevUnitsToAppUnits(p2t);
    mDeviceContext->GetCanonicalPixelScale(scale);
    aScale = p2t * scale;
  }
  else
    aScale = 1.0f;
  
  return NS_OK;
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
    nsIURLGroup* urlGroup;
    rv = mBaseURL->GetURLGroup(&urlGroup);
    if (rv == NS_OK)
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
                              PRBool aNeedErrorNotification,
                              nsIFrameImageLoader*& aLoaderResult)
{
  if (mStopped) {
    return NS_OK;
  }

#ifdef NOISY_IMAGES
  nsIDocument* doc = mShell->GetDocument();
  if (nsnull != doc) {
    nsIURL* url = doc->GetDocumentURL();
    if (nsnull != url) {
      printf("%s: ", url->GetSpec());
      NS_RELEASE(url);
    }
    NS_RELEASE(doc);
  }
  printf("%p: start load for %p (", this, aTargetFrame);
  fputs(aURL, stdout);
  printf(")\n");
#endif

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
                    aCallBack, aNeedSizeUpdate, aNeedErrorNotification);
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
nsPresContext::Stop(void)
{
  PRInt32 n = mImageLoaders.Count();
  for (PRInt32 i = 0; i < n; i++) {
    nsIFrameImageLoader* loader;
    loader = (nsIFrameImageLoader*) mImageLoaders.ElementAt(i);
    loader->StopImageLoad();
    NS_RELEASE(loader);
  }
  mImageLoaders.Clear();
  mStopped = PR_TRUE;
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
#ifdef NOISY_IMAGES
      nsIDocument* doc = mShell->GetDocument();
      if (nsnull != doc) {
        nsIURL* url = doc->GetDocumentURL();
        if (nsnull != url) {
          printf("%s: ", url->GetSpec());
          NS_RELEASE(url);
        }
        NS_RELEASE(doc);
      }
      printf("%p: stop load for %p\n", this, aForFrame);
#endif
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

  //Not refcnted, set null in destructor
  mEventManager->SetPresContext(this);

  *aManager = mEventManager;
  NS_IF_ADDREF(mEventManager);
  return NS_OK;
}


