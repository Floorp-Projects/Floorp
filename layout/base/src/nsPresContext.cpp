/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   IBM Corporation 
 * 
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/20/2000   IBM Corp.       BiDi - ability to change the default direction of the browser
 *
 */
#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIPref.h"
#include "nsILinkHandler.h"
#include "nsIStyleSet.h"
#include "nsFrameImageLoader.h"
#include "nsIFrameManager.h"
#include "nsIImageGroup.h"
#include "nsIFrame.h"
#include "nsIRenderingContext.h"
#include "nsEventStateManager.h"
#include "nsIURL.h"
#include "nsIDocument.h"
#include "nsIStyleContext.h"
#include "nsLayoutAtoms.h"
#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsIURIContentListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIServiceManager.h"

#ifdef _WIN32
#include <windows.h>
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
static NS_DEFINE_IID(kLookAndFeelCID,  NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);

nsPresContext::nsPresContext()
  : mDefaultFont("serif", NS_FONT_STYLE_NORMAL,
                 NS_FONT_VARIANT_NORMAL,
                 NS_FONT_WEIGHT_NORMAL,
                 0,
                 NSIntPointsToTwips(12)),
    mDefaultFixedFont("monospace", NS_FONT_STYLE_NORMAL,
                      NS_FONT_VARIANT_NORMAL,
                      NS_FONT_WEIGHT_NORMAL,
                      0,
                      NSIntPointsToTwips(10))
{
  NS_INIT_REFCNT();
  mCompatibilityMode = eCompatibility_Standard;
  mCompatibilityLocked = PR_FALSE;
  mWidgetRenderingMode = eWidgetRendering_Gfx; 

  mLookAndFeel = nsnull;
  mShell = nsnull;

#ifdef _WIN32
  // XXX This needs to be elsewhere, e.g., part of nsIDeviceContext
  mDefaultColor = ::GetSysColor(COLOR_WINDOWTEXT);
  mDefaultBackgroundColor = ::GetSysColor(COLOR_WINDOW);
#else
  mDefaultColor = NS_RGB(0x00, 0x00, 0x00);
  mDefaultBackgroundColor = NS_RGB(0xFF, 0xFF, 0xFF);
#endif
  mDefaultBackgroundImageAttachment = NS_STYLE_BG_ATTACHMENT_SCROLL;
  mDefaultBackgroundImageRepeat = NS_STYLE_BG_REPEAT_XY;
  mDefaultBackgroundImageOffsetX = mDefaultBackgroundImageOffsetY = 0;
  mDefaultDirection = NS_STYLE_DIRECTION_LTR;
}

nsPresContext::~nsPresContext()
{
  mShell = nsnull;

  Stop();

  if (mImageGroup) {
    // Interrupt any loading images. This also stops all looping
    // image animations.
    mImageGroup->Interrupt();
  }

  if (mEventManager)
    mEventManager->SetPresContext(nsnull);   // unclear if this is needed, but can't hurt

  NS_IF_RELEASE(mLookAndFeel);

  // Unregister preference callbacks
  if (mPrefs) {
    mPrefs->UnregisterCallback("browser.screen_resolution", PrefChangedCallback, (void*)this);
    mPrefs->UnregisterCallback("browser.use_document_fonts", PrefChangedCallback, (void*)this);
    mPrefs->UnregisterCallback("browser.use_document_colors", PrefChangedCallback, (void*)this);
    mPrefs->UnregisterCallback("browser.background_color", PrefChangedCallback, (void*)this);
    mPrefs->UnregisterCallback("browser.foreground_color", PrefChangedCallback, (void*)this);
    mPrefs->UnregisterCallback("browser.base_font_scaler", PrefChangedCallback, (void*)this);
    mPrefs->UnregisterCallback("browser.wfe.use_windows_colors", PrefChangedCallback, (void*)this);
    mPrefs->UnregisterCallback("font.", PrefChangedCallback, (void*)this);
    mPrefs->UnregisterCallback("browser.display.direction", PrefChangedCallback, (void*)this);
  }
}

NS_IMPL_ISUPPORTS(nsPresContext, kIPresContextIID);

void
nsPresContext::GetFontPreferences()
{
  if (mPrefs) {
    char* value = nsnull;
    mPrefs->CopyCharPref("font.default", &value);
    if (value) {
      mDefaultFont.name.AssignWithConversion(value);
      nsAllocator::Free(value);
      value = nsnull;
    }
    if (mLangGroup) {
      nsAutoString pref; pref.AssignWithConversion("font.size.variable.");
      const PRUnichar* langGroup = nsnull;
      mLangGroup->GetUnicode(&langGroup);
      pref.Append(langGroup);
      char name[128];
      pref.ToCString(name, sizeof(name));
      PRInt32 variableSize = 16;
      mPrefs->GetIntPref(name, &variableSize);
      pref.AssignWithConversion("font.size.fixed.");
      pref.Append(langGroup);
      pref.ToCString(name, sizeof(name));
      PRInt32 fixedSize = 13;
      mPrefs->GetIntPref(name, &fixedSize);
      char* unit = nsnull;
      mPrefs->CopyCharPref("font.size.unit", &unit);
      char* defaultUnit = "px";
      if (!unit) {
        unit = defaultUnit;
      }
      if (!PL_strcmp(unit, "px")) {
        float p2t;
        GetScaledPixelsToTwips(&p2t);
        mDefaultFont.size = NSFloatPixelsToTwips((float) variableSize, p2t);
        mDefaultFixedFont.size = NSFloatPixelsToTwips((float) fixedSize, p2t);
      }
      else if (!PL_strcmp(unit, "pt")) {
        mDefaultFont.size = NSIntPointsToTwips(variableSize);
        mDefaultFixedFont.size = NSIntPointsToTwips(fixedSize);
      }
      else {
        float p2t;
        GetScaledPixelsToTwips(&p2t);
        mDefaultFont.size = NSFloatPixelsToTwips((float) variableSize, p2t);
        mDefaultFixedFont.size = NSFloatPixelsToTwips((float) fixedSize, p2t);
      }
      if (unit != defaultUnit) {
        nsAllocator::Free(unit);
        unit = nsnull;
      }
    }
  }
}

void
nsPresContext::GetUserPreferences()
{
  PRInt32 prefInt;

  if (NS_OK == mPrefs->GetIntPref("browser.base_font_scaler", &prefInt)) {
    mFontScaler = prefInt;
  }

  if (NS_OK == mPrefs->GetIntPref("nglayout.compatibility.mode", &prefInt)) {
    // XXX this should really be a state on the webshell instead of using prefs
    switch (prefInt) {
      case 1: 
        mCompatibilityLocked = PR_TRUE;  
        mCompatibilityMode = eCompatibility_Standard;   
        break;
      case 2: 
        mCompatibilityLocked = PR_TRUE;  
        mCompatibilityMode = eCompatibility_NavQuirks;  
        break;
      case 0:   // auto
      default:
        mCompatibilityLocked = PR_FALSE;  
        break;
    }
  }
  else {
    mCompatibilityLocked = PR_FALSE;  // auto
  }

  if (NS_OK == mPrefs->GetIntPref("nglayout.widget.mode", &prefInt)) {
    mWidgetRenderingMode = (enum nsWidgetRendering)prefInt;  // bad cast
  }

  PRBool usePrefColors = PR_TRUE;
#ifdef _WIN32
  PRBool boolPref;
  // XXX Is Windows the only platform that uses this?
  if (NS_OK == mPrefs->GetBoolPref("browser.wfe.use_windows_colors", &boolPref)) {
    usePrefColors = !boolPref;
  }
#endif
  if (usePrefColors) {
    PRUint32  colorPref;
    if (NS_OK == mPrefs->GetColorPrefDWord("browser.foreground_color", &colorPref)) {
      mDefaultColor = (nscolor)colorPref;
    }
    if (NS_OK == mPrefs->GetColorPrefDWord("browser.background_color", &colorPref)) {
      mDefaultBackgroundColor = (nscolor)colorPref;
    }
  }
  if (NS_OK == mPrefs->GetIntPref("browser.display.direction", &prefInt)) {
    mDefaultDirection = prefInt;
  }

  GetFontPreferences();
}

NS_IMETHODIMP
nsPresContext::RemapStyleAndReflow()
{
  if (mShell) {
    // Have the root frame's style context remap its style based on the
    // user preferences
    nsIFrame* rootFrame;

    mShell->GetRootFrame(&rootFrame);
    if (rootFrame) {
      nsIStyleContext*  rootStyleContext;

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

  return NS_OK;
}

void
nsPresContext::PreferenceChanged(const char* aPrefName)
{
  // Initialize our state from the user preferences
  GetUserPreferences();
  if (mDeviceContext) {
    mDeviceContext->FlushFontCache();
    RemapStyleAndReflow();
  }
}

NS_IMETHODIMP
nsPresContext::Init(nsIDeviceContext* aDeviceContext)
{
  NS_ASSERTION(!(mInitialized == PR_TRUE), "attempt to reinit pres context");

  mDeviceContext = dont_QueryInterface(aDeviceContext);

  mCharSets = do_GetService(NS_CHARSETCONVERTERMANAGER_PROGID);
  mPrefs = do_GetService(NS_PREF_PROGID);
  if (mPrefs) {
    // Register callbacks so we're notified when the preferences change
    mPrefs->RegisterCallback("browser.screen_resolution", PrefChangedCallback, (void*)this);
    mPrefs->RegisterCallback("browser.use_document_fonts", PrefChangedCallback, (void*)this);
    mPrefs->RegisterCallback("browser.use_document_colors", PrefChangedCallback, (void*)this);
    mPrefs->RegisterCallback("browser.background_color", PrefChangedCallback, (void*)this);
    mPrefs->RegisterCallback("browser.foreground_color", PrefChangedCallback, (void*)this);
    mPrefs->RegisterCallback("browser.base_font_scaler", PrefChangedCallback, (void*)this);
    mPrefs->RegisterCallback("browser.wfe.use_windows_colors", PrefChangedCallback, (void*)this);
    mPrefs->RegisterCallback("font.", PrefChangedCallback, (void*)this);
    mPrefs->RegisterCallback("browser.display.direction", PrefChangedCallback, (void*)this);

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
  mShell = aShell;
  if (nsnull != mShell) {
    nsCOMPtr<nsIDocument> doc;
    if (NS_SUCCEEDED(mShell->GetDocument(getter_AddRefs(doc)))) {
      NS_ASSERTION(doc, "expect document here");
      if (doc) {
        doc->GetBaseURL(*getter_AddRefs(mBaseURL));
        if (mCharSets) {
          nsAutoString charset;
          doc->GetDocumentCharacterSet(charset);
          // XXX GetCharsetLangGroup ought to do the lower-casing for me
          charset.ToLowerCase();
          mCharSets->GetCharsetLangGroup(&charset, getter_AddRefs(mLangGroup));
          GetFontPreferences();
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetShell(nsIPresShell** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mShell;
  NS_IF_ADDREF(mShell);
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetCompatibilityMode(nsCompatibility* aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mCompatibilityMode;
  return NS_OK;
}

 
NS_IMETHODIMP
nsPresContext::SetCompatibilityMode(nsCompatibility aMode)
{
  if (! mCompatibilityLocked) {
    mCompatibilityMode = aMode;
  }
  return NS_OK;
}


NS_IMETHODIMP
nsPresContext::GetWidgetRenderingMode(nsWidgetRendering* aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mWidgetRenderingMode;
  return NS_OK;
}

 
NS_IMETHODIMP
nsPresContext::SetWidgetRenderingMode(nsWidgetRendering aMode)
{
  mWidgetRenderingMode = aMode;
  return NS_OK;
}


NS_IMETHODIMP
nsPresContext::GetLookAndFeel(nsILookAndFeel** aLookAndFeel)
{
  NS_PRECONDITION(nsnull != aLookAndFeel, "null ptr");
  if (nsnull == aLookAndFeel) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult result = NS_OK;
  if (! mLookAndFeel) {
    result = nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, 
                                                kILookAndFeelIID, (void**)&mLookAndFeel);
    if (NS_FAILED(result)) {
      mLookAndFeel = nsnull;
    }
  }
  NS_IF_ADDREF(mLookAndFeel);
  *aLookAndFeel = mLookAndFeel;
  return result;
}



NS_IMETHODIMP
nsPresContext::GetBaseURL(nsIURI** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mBaseURL;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::ResolveStyleContextFor(nsIContent* aContent,
                                      nsIStyleContext* aParentContext,
                                      PRBool aForceUnique,
                                      nsIStyleContext** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIStyleContext* result = nsnull;
  nsCOMPtr<nsIStyleSet> set;
  nsresult rv = mShell->GetStyleSet(getter_AddRefs(set));
  if (NS_SUCCEEDED(rv)) {
    if (set) {
      result = set->ResolveStyleFor(this, aContent, aParentContext,
                                    aForceUnique);
      if (nsnull == result) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  *aResult = result;
  return rv;
}

NS_IMETHODIMP
nsPresContext::ResolvePseudoStyleContextFor(nsIContent* aParentContent,
                                            nsIAtom* aPseudoTag,
                                            nsIStyleContext* aParentContext,
                                            PRBool aForceUnique,
                                            nsIStyleContext** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIStyleContext* result = nsnull;
  nsCOMPtr<nsIStyleSet> set;
  nsresult rv = mShell->GetStyleSet(getter_AddRefs(set));
  if (NS_SUCCEEDED(rv)) {
    if (set) {
      result = set->ResolvePseudoStyleFor(this, aParentContent, aPseudoTag,
                                          aParentContext, aForceUnique);
      if (nsnull == result) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  *aResult = result;
  return rv;
}

NS_IMETHODIMP
nsPresContext::ProbePseudoStyleContextFor(nsIContent* aParentContent,
                                          nsIAtom* aPseudoTag,
                                          nsIStyleContext* aParentContext,
                                          PRBool aForceUnique,
                                          nsIStyleContext** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIStyleContext* result = nsnull;
  nsCOMPtr<nsIStyleSet> set;
  nsresult rv = mShell->GetStyleSet(getter_AddRefs(set));
  if (NS_SUCCEEDED(rv)) {
    if (set) {
      result = set->ProbePseudoStyleFor(this, aParentContent, aPseudoTag,
                                        aParentContext, aForceUnique);
    }
  }
  *aResult = result;
  return rv;
}

NS_IMETHODIMP
nsPresContext::ReParentStyleContext(nsIFrame* aFrame, 
                                    nsIStyleContext* aNewParentContext)
{
  NS_PRECONDITION(aFrame, "null ptr");
  if (! aFrame) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIFrameManager> manager;
  nsresult rv = mShell->GetFrameManager(getter_AddRefs(manager));
  if (NS_SUCCEEDED(rv) && manager) {
    rv = manager->ReParentStyleContext(this, aFrame, aNewParentContext);
  }
  return rv;
}


NS_IMETHODIMP
nsPresContext::GetMetricsFor(const nsFont& aFont, nsIFontMetrics** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIFontMetrics* metrics = nsnull;
  if (mDeviceContext) {
    mDeviceContext->GetMetricsFor(aFont, mLangGroup, metrics);
  }
  *aResult = metrics;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetDefaultFont(nsFont& aResult)
{
  aResult = mDefaultFont;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::SetDefaultFont(const nsFont& aFont)
{
  mDefaultFont = aFont;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetDefaultFixedFont(nsFont& aResult)
{
  aResult = mDefaultFixedFont;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::SetDefaultFixedFont(const nsFont& aFont)
{
  mDefaultFixedFont = aFont;
  return NS_OK;
}

const nsFont&
nsPresContext::GetDefaultFontDeprecated()
{
  return mDefaultFont;
}

const nsFont&
nsPresContext::GetDefaultFixedFontDeprecated()
{
  return mDefaultFixedFont;
}

NS_IMETHODIMP
nsPresContext::GetFontScaler(PRInt32* aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResult = mFontScaler;
  return NS_OK;
}

NS_IMETHODIMP 
nsPresContext::SetFontScaler(PRInt32 aScaler)
{
  mFontScaler = aScaler;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetDefaultColor(nscolor* aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResult = mDefaultColor;
  return NS_OK;
}

NS_IMETHODIMP 
nsPresContext::GetDefaultBackgroundColor(nscolor* aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResult = mDefaultBackgroundColor;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetDefaultBackgroundImage(nsString& aImage)
{
  aImage = mDefaultBackgroundImage;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetDefaultBackgroundImageRepeat(PRUint8* aRepeat)
{
  NS_PRECONDITION(nsnull != aRepeat, "null ptr");
  if (nsnull == aRepeat) { return NS_ERROR_NULL_POINTER; }
  *aRepeat = mDefaultBackgroundImageRepeat;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetDefaultBackgroundImageOffset(nscoord* aX, nscoord* aY)
{
  NS_PRECONDITION((nsnull != aX) && (nsnull != aY), "null ptr");
  if (!aX || !aY) { return NS_ERROR_NULL_POINTER; }
  *aX = mDefaultBackgroundImageOffsetX;
  *aY = mDefaultBackgroundImageOffsetY;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetDefaultBackgroundImageAttachment(PRUint8* aAttachment)
{
  NS_PRECONDITION(nsnull != aAttachment, "null ptr");
  if (nsnull == aAttachment) { return NS_ERROR_NULL_POINTER; }
  *aAttachment = mDefaultBackgroundImageAttachment;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::SetDefaultColor(nscolor aColor)
{
  mDefaultColor = aColor;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::SetDefaultBackgroundColor(nscolor aColor)
{
  mDefaultBackgroundColor = aColor;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::SetDefaultBackgroundImage(const nsString& aImage)
{
  mDefaultBackgroundImage = aImage;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::SetDefaultBackgroundImageRepeat(PRUint8 aRepeat)
{
  mDefaultBackgroundImageRepeat = aRepeat;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::SetDefaultBackgroundImageOffset(nscoord aX, nscoord aY)
{
  mDefaultBackgroundImageOffsetX = aX;
  mDefaultBackgroundImageOffsetY = aY;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::SetDefaultBackgroundImageAttachment(PRUint8 aAttachment)
{
  mDefaultBackgroundImageAttachment = aAttachment;
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

NS_IMETHODIMP
nsPresContext::GetPixelsToTwips(float* aResult) const
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  float p2t = 1.0f;
  if (mDeviceContext) {
    mDeviceContext->GetDevUnitsToAppUnits(p2t);
  }
  *aResult = p2t;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetTwipsToPixels(float* aResult) const
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  float app2dev = 1.0f;
  if (mDeviceContext) {
    mDeviceContext->GetAppUnitsToDevUnits(app2dev);
  }
  *aResult = app2dev;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetScaledPixelsToTwips(float* aResult) const
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  float scale = 1.0f;
  if (mDeviceContext)
  {
    float p2t;
    mDeviceContext->GetDevUnitsToAppUnits(p2t);
    mDeviceContext->GetCanonicalPixelScale(scale);
    scale = p2t * scale;
  }
  *aResult = scale;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetDeviceContext(nsIDeviceContext** aResult) const
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mDeviceContext;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetImageGroup(nsIImageGroup** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  if (!mImageGroup) {
    // Create image group
    nsresult rv = NS_NewImageGroup(getter_AddRefs(mImageGroup));
    if (NS_OK != rv) {
      return rv;
    }

    // Initialize the image group
    nsCOMPtr<nsIURIContentListener> loadHandler (do_GetInterface(mContainer, &rv));
    if (NS_SUCCEEDED(rv) && loadHandler)
    {
      nsCOMPtr<nsISupports> loadContext;
      loadHandler->GetLoadCookie(getter_AddRefs(loadContext));
      rv = mImageGroup->Init(mDeviceContext, loadContext);
    }

    if (NS_OK != rv) {
      return rv;
    }
  }

  *aResult = mImageGroup;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::StartLoadImage(const nsString& aURL,
                              const nscolor* aBackgroundColor,
                              const nsSize* aDesiredSize,
                              nsIFrame* aTargetFrame,
                              nsIFrameImageLoaderCB aCallBack,
                              void* aClosure,
                              nsIFrameImageLoader** aResult)
{
  if (mStopped) {
    if (aResult) {
      *aResult = nsnull;
    }
    return NS_OK;
  }

  // Allow for a null target frame argument (for precached images)
  if (nsnull != aTargetFrame) {
    // Mark frame as having loaded an image
    nsFrameState state;
    aTargetFrame->GetFrameState(&state);
    state |= NS_FRAME_HAS_LOADED_IMAGES;
    aTargetFrame->SetFrameState(state);
  }

  // Lookup image request in our loaders array (maybe the load request
  // has already been made for that url at the desired size).
  PRInt32 i, n = mImageLoaders.Count();
  nsIFrameImageLoader* loader;
  for (i = 0; i < n; i++) {
    PRBool same;
    loader = (nsIFrameImageLoader*) mImageLoaders.ElementAt(i);
    loader->IsSameImageRequest(aURL, aBackgroundColor, aDesiredSize, &same);
    if (same) {
      // This is pretty sick, but because we can get a notification
      // *before* the caller has returned we have to store into
      // aResult before calling the AddFrame method.
      if (aResult) {
        NS_ADDREF(loader);
        *aResult = loader;
      }

      // Add frame to list of interested frames for this loader
      loader->AddFrame(aTargetFrame, aCallBack, aClosure);
      return NS_OK;
    }
  }

  // Create image group if needed
  nsresult rv;
  if (!mImageGroup) {
    nsCOMPtr<nsIImageGroup> group;
    rv = GetImageGroup(getter_AddRefs(group));   // sets mImageGroup as side effect
    if (NS_OK != rv) {
      return rv;
    }
  }

  // We haven't seen that image before. Create a new loader and
  // start it going.
  rv = NS_NewFrameImageLoader(&loader);
  if (NS_OK != rv) {
    return rv;
  }
  mImageLoaders.AppendElement(loader);

  // This is pretty sick, but because we can get a notification
  // *before* the caller has returned we have to store into aResult
  // before calling the loaders init method.
  if (aResult) {
    *aResult = loader;
    NS_ADDREF(loader);
  }

  rv = loader->Init(this, mImageGroup, aURL, aBackgroundColor, aDesiredSize,
                    aTargetFrame, aCallBack, aClosure);
  if (NS_OK != rv) {
    mImageLoaders.RemoveElement(loader);
    loader->StopImageLoad();
    NS_RELEASE(loader);

    // Undo premature store of reslut
    if (aResult) {
      *aResult = nsnull;
      NS_RELEASE(loader);
    }
    return rv;
  }
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
nsPresContext::StopLoadImage(nsIFrame* aTargetFrame,
                             nsIFrameImageLoader* aLoader)
{
  PRInt32 i, n = mImageLoaders.Count();
  nsIFrameImageLoader* loader;
  for (i = 0; i < n; i++) {
    loader = (nsIFrameImageLoader*) mImageLoaders.ElementAt(i);
    if (loader == aLoader) {
      // Remove frame from list of interested frames for this loader
      loader->RemoveFrame(aTargetFrame);

      // If loader is no longer loading for anybody and its safe to
      // nuke it, nuke it.
      PRBool safe;
      loader->SafeToDestroy(&safe);
      if (safe) {
        loader->StopImageLoad();
        NS_RELEASE(loader);
        mImageLoaders.RemoveElementAt(i);
        n--;
        i--;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::StopAllLoadImagesFor(nsIFrame* aTargetFrame)
{
  nsFrameState state;
  aTargetFrame->GetFrameState(&state);
  if (NS_FRAME_HAS_LOADED_IMAGES & state) {
    nsIFrameImageLoader* loader;
    PRInt32 i, n = mImageLoaders.Count();
    for (i = 0; i < n; i++) {
      PRBool safe;
      loader = (nsIFrameImageLoader*) mImageLoaders.ElementAt(i);
      loader->RemoveFrame(aTargetFrame);
      loader->SafeToDestroy(&safe);
      if (safe) {
        loader->StopImageLoad();
        NS_RELEASE(loader);
        mImageLoaders.RemoveElementAt(i);
        n--;
        i--;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::SetLinkHandler(nsILinkHandler* aHandler)
{
  mLinkHandler = aHandler;
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
{
  mContainer = aHandler;
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

  if (!mEventManager) {
    nsresult rv = NS_NewEventStateManager(getter_AddRefs(mEventManager));
    if (NS_OK != rv) {
      return rv;
    }
  }

  //Not refcnted, set null in destructor
  mEventManager->SetPresContext(this);

  *aManager = mEventManager;
  NS_IF_ADDREF(*aManager);
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetDefaultDirection(PRUint8* aDirection)
{
  *aDirection = mDefaultDirection;
  return NS_OK;
}


NS_IMETHODIMP
nsPresContext::SetDefaultDirection(PRUint8 aDirection)
{
  mDefaultDirection = aDirection;
  return NS_OK;
}

#ifdef MOZ_REFLOW_PERF
NS_IMETHODIMP
nsPresContext::CountReflows(const char * aName, PRUint32 aType)
{
  if (mShell) {
    mShell->CountReflows(aName, aType);
  }
  return NS_OK;
}
#endif
