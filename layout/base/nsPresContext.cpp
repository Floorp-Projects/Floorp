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
 */
#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIPref.h"
#include "nsILinkHandler.h"
#include "nsIDocShellTreeItem.h"
#include "nsIStyleSet.h"
#include "nsIFrameManager.h"
#include "nsImageLoader.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIRenderingContext.h"
#include "nsIURL.h"
#include "nsIDocument.h"
#include "nsIStyleContext.h"
#include "nsLayoutAtoms.h"
#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsIURIContentListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIServiceManager.h"
#include "nsBoxLayoutState.h"
#include "nsIBox.h"
#include "nsIDOMElement.h"
#include "nsContentPolicyUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsNetUtil.h"
#include "nsXPIDLString.h"
#include "prprf.h"
#ifdef IBMBIDI
#include "nsBidiPresUtils.h"
#endif // IBMBIDI

static nscolor
MakeColorPref(const char *colstr)
{
  PRUint32 red, green, blue;
  nscolor colorref;

  // 4.x stored RGB color values as a string rather than as an int,
  // thus we need to do this conversion
  PR_sscanf(colstr, "#%02x%02x%02x", &red, &green, &blue);
  colorref = NS_RGB(red, green, blue);
  return colorref;
}

int PR_CALLBACK
nsPresContext::PrefChangedCallback(const char* aPrefName, void* instance_data)
{
  nsPresContext*  presContext = (nsPresContext*)instance_data;

  NS_ASSERTION(nsnull != presContext, "bad instance data");
  if (nsnull != presContext) {
    presContext->PreferenceChanged(aPrefName);
  }
  return 0;  // PREF_OK
}

#ifdef IBMBIDI
PRBool
IsVisualCharset(const nsAutoString& aCharset)
{
  if (aCharset.EqualsIgnoreCase("ibm864")             // Arabic//ahmed
      || aCharset.EqualsIgnoreCase("ibm862")          // Hebrew
      || aCharset.EqualsIgnoreCase("iso-8859-8") ) {  // Hebrew
    return PR_TRUE; // visual text type
  }
  else {
    return PR_FALSE; // logical text type
  }
}
#endif // IBMBIDI


PR_STATIC_CALLBACK(PRBool) destroy_loads(nsHashKey *aKey, void *aData, void* closure)
{
  nsISupports *sup = NS_REINTERPRET_CAST(nsISupports*, aData);
  nsImageLoader *loader = NS_REINTERPRET_CAST(nsImageLoader*, sup);
  loader->Destroy();
  return PR_TRUE;
}

static NS_DEFINE_CID(kLookAndFeelCID,  NS_LOOKANDFEEL_CID);
#include "nsContentCID.h"
static NS_DEFINE_CID(kEventStateManagerCID, NS_EVENTSTATEMANAGER_CID);

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
  mImageAnimationMode = eImageAnimation_Normal;
  mImageAnimationModePref = eImageAnimation_Normal;

  mStopped = PR_FALSE;
  mStopChrome = PR_TRUE;

  mShell = nsnull;

  mDefaultColor = NS_RGB(0x00, 0x00, 0x00);
  mDefaultBackgroundColor = NS_RGB(0xFF, 0xFF, 0xFF);
  nsILookAndFeel* look = nsnull;
  if (NS_SUCCEEDED(GetLookAndFeel(&look)) && look) {
    look->GetColor(nsILookAndFeel::eColor_WindowForeground, mDefaultColor);
    look->GetColor(nsILookAndFeel::eColor_WindowBackground, mDefaultBackgroundColor);
    NS_RELEASE(look);
  }
  
  mUseDocumentColors = PR_TRUE;
  mUseDocumentFonts = PR_TRUE;

  mLinkColor = NS_RGB(0x33, 0x33, 0xFF);
  mVisitedLinkColor = NS_RGB(0x66, 0x00, 0xCC);
  mUnderlineLinks = PR_TRUE;

  mUseFocusColors = PR_FALSE;
  mFocusTextColor = mDefaultColor;
  mFocusBackgroundColor = mDefaultBackgroundColor;
  mFocusRingWidth = 1;
  mFocusRingOnAnything = PR_FALSE;

  mDefaultBackgroundImageAttachment = NS_STYLE_BG_ATTACHMENT_SCROLL;
  mDefaultBackgroundImageRepeat = NS_STYLE_BG_REPEAT_XY;
  mDefaultBackgroundImageOffsetX = mDefaultBackgroundImageOffsetY = 0;
  mLanguageSpecificTransformType = eLanguageSpecificTransformType_Unknown;
  mIsRenderingOnlySelection = PR_FALSE;
#ifdef IBMBIDI
  mIsVisual = PR_FALSE;
  mBidiUtils = nsnull;
  mIsBidiSystem = PR_FALSE;
  mBidi = 0;
#endif // IBMBIDI
}

nsPresContext::~nsPresContext()
{
  mImageLoaders.Enumerate(destroy_loads);

  if (mShell) {
    nsCOMPtr<nsIDocument> doc;
    mShell->GetDocument(getter_AddRefs(doc));
    if (doc) {
      doc->RemoveCharSetObserver(this);
    }
  }

  mShell = nsnull;

  if (mEventManager)
    mEventManager->SetPresContext(nsnull);   // unclear if this is needed, but can't hurt

  // Unregister preference callbacks
  if (mPrefs) {
    mPrefs->UnregisterCallback("font.", nsPresContext::PrefChangedCallback, (void*)this);
    mPrefs->UnregisterCallback("browser.display.", nsPresContext::PrefChangedCallback, (void*)this);
    mPrefs->UnregisterCallback("browser.underline_anchors", nsPresContext::PrefChangedCallback, (void*)this);
    mPrefs->UnregisterCallback("browser.anchor_color", nsPresContext::PrefChangedCallback, (void*)this);
    mPrefs->UnregisterCallback("browser.visited_color", nsPresContext::PrefChangedCallback, (void*)this);
    mPrefs->UnregisterCallback("network.image.imageBehavior", nsPresContext::PrefChangedCallback, (void*)this);
    mPrefs->UnregisterCallback("image.animation_mode", nsPresContext::PrefChangedCallback, (void*)this);
  }
#ifdef IBMBIDI
  if (mBidiUtils) {
    delete mBidiUtils;
  }
#endif // IBMBIDI
}

NS_IMPL_ISUPPORTS2(nsPresContext, nsIPresContext, nsIObserver)

void
nsPresContext::GetFontPreferences()
{
  if (mPrefs) {
    char* value = nsnull;
    mPrefs->CopyCharPref("font.default", &value);
    if (value) {
      mDefaultFont.name.AssignWithConversion(value);
      nsMemory::Free(value);
      value = nsnull;
    }
    if (mLanguage) {
      nsAutoString pref; pref.AssignWithConversion("font.size.variable.");
      const PRUnichar* langGroup = nsnull;
      nsCOMPtr<nsIAtom> langGroupAtom;
      mLanguage->GetLanguageGroup(getter_AddRefs(langGroupAtom));
      langGroupAtom->GetUnicode(&langGroup);
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
        nsMemory::Free(unit);
        unit = nsnull;
      }
    }
  }
}

void
nsPresContext::GetUserPreferences()
{
  PRInt32 prefInt;

  if (NS_SUCCEEDED(mPrefs->GetIntPref("browser.display.base_font_scaler", &prefInt))) {
    mFontScaler = prefInt;
  }

  if (NS_SUCCEEDED(mPrefs->GetIntPref("nglayout.compatibility.mode", &prefInt))) {
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

  if (NS_SUCCEEDED(mPrefs->GetIntPref("nglayout.widget.mode", &prefInt))) {
    mWidgetRenderingMode = (enum nsWidgetRendering)prefInt;  // bad cast
  }

  // * document colors
  PRBool usePrefColors = PR_TRUE;
  PRBool boolPref;
  nsXPIDLCString colorStr;
  if (NS_SUCCEEDED(mPrefs->GetBoolPref("browser.display.use_system_colors", &boolPref))) {
    usePrefColors = !boolPref;
  }
  if (usePrefColors) {
    if (NS_SUCCEEDED(mPrefs->CopyCharPref("browser.display.foreground_color", getter_Copies(colorStr)))) {
      mDefaultColor = MakeColorPref(colorStr);
    }
    if (NS_SUCCEEDED(mPrefs->CopyCharPref("browser.display.background_color", getter_Copies(colorStr)))) {
      mDefaultBackgroundColor = MakeColorPref(colorStr);
    }
  }
  else {
    // Without this here, checking the "use system colors" checkbox
    // has no affect until the constructor is called again
    mDefaultColor = NS_RGB(0x00, 0x00, 0x00);
    mDefaultBackgroundColor = NS_RGB(0xFF, 0xFF, 0xFF);
    nsCOMPtr<nsILookAndFeel> look;
    if (NS_SUCCEEDED(GetLookAndFeel(getter_AddRefs(look))) && look) {
      look->GetColor(nsILookAndFeel::eColor_WindowForeground, mDefaultColor);
      look->GetColor(nsILookAndFeel::eColor_WindowBackground, mDefaultBackgroundColor);
    }
  }

  if (NS_SUCCEEDED(mPrefs->GetBoolPref("browser.display.use_document_colors", &boolPref))) {
    mUseDocumentColors = boolPref;
  }
  // * link colors
  if (NS_SUCCEEDED(mPrefs->GetBoolPref("browser.underline_anchors", &boolPref))) {
    mUnderlineLinks = boolPref;
  }
  if (NS_SUCCEEDED(mPrefs->CopyCharPref("browser.anchor_color", getter_Copies(colorStr)))) {
    mLinkColor = MakeColorPref(colorStr);
  }
  if (NS_SUCCEEDED(mPrefs->CopyCharPref("browser.visited_color", getter_Copies(colorStr)))) {
    mVisitedLinkColor = MakeColorPref(colorStr);
  }


  if (NS_SUCCEEDED(mPrefs->GetBoolPref("browser.display.use_focus_colors", &boolPref))) {
    mUseFocusColors = boolPref;
    mFocusTextColor = mDefaultColor;
    mFocusBackgroundColor = mDefaultBackgroundColor;
    if (NS_SUCCEEDED(mPrefs->CopyCharPref("browser.display.focus_text_color", getter_Copies(colorStr)))) {
      mFocusTextColor = MakeColorPref(colorStr);
    }
    if (NS_SUCCEEDED(mPrefs->CopyCharPref("browser.display.focus_background_color", getter_Copies(colorStr)))) {
      mFocusBackgroundColor = MakeColorPref(colorStr);
    }
  }

  if (NS_SUCCEEDED(mPrefs->GetIntPref("browser.display.focus_ring_width", &prefInt))) {
    mFocusRingWidth = prefInt;
  }

  if (NS_SUCCEEDED(mPrefs->GetBoolPref("browser.display.focus_ring_on_anything", &boolPref))) {
    mFocusRingOnAnything = boolPref;
  }

  // * use fonts?
  if (NS_SUCCEEDED(mPrefs->GetIntPref("browser.display.use_document_fonts", &prefInt))) {
    mUseDocumentFonts = prefInt == 0 ? PR_FALSE : PR_TRUE;
  }
  
  GetFontPreferences();

  // * image animation
  char* animatePref = 0;
  nsresult rv = mPrefs->CopyCharPref("image.animation_mode", &animatePref);
  if (NS_SUCCEEDED(rv) && animatePref) {
    if (!nsCRT::strcmp(animatePref, "normal"))
      mImageAnimationModePref = eImageAnimation_Normal;
    else if (!nsCRT::strcmp(animatePref, "none"))
      mImageAnimationModePref = eImageAnimation_None;
    else if (!nsCRT::strcmp(animatePref, "once"))
      mImageAnimationModePref = eImageAnimation_LoopOnce;
    nsMemory::Free(animatePref);
  }
}

NS_IMETHODIMP
nsPresContext::GetCachedBoolPref(PRUint32 prefType, PRBool &aValue)
{
  nsresult rv = NS_ERROR_FAILURE;

  switch(prefType) {
  case kPresContext_UseDocumentFonts :
    aValue = mUseDocumentFonts;
    rv = NS_OK;
    break;
  case kPresContext_UseDocumentColors:
    aValue = mUseDocumentColors;
    rv = NS_OK;
    break;
  case kPresContext_UnderlineLinks:
    aValue = mUnderlineLinks;
    rv = NS_OK;
    break;
  default :
    rv = NS_ERROR_FAILURE;
  }
  return rv;
}

NS_IMETHODIMP
nsPresContext::ClearStyleDataAndReflow()
{
  if (mShell) {
    // Clear out all our style data.
    nsCOMPtr<nsIStyleSet> set;
    mShell->GetStyleSet(getter_AddRefs(set));
    set->ClearStyleData(this, nsnull, nsnull);

    // Have the root frame's style context remap its style based on the
    // user preferences
    nsIFrame* rootFrame;

    mShell->GetRootFrame(&rootFrame);
    if (rootFrame) {
      // boxes know how to coelesce style changes. So if our root frame is a box
      // then tell it to handle it. If its a block we are stuck with a full top to bottom
      // reflow. -EDV
      nsIFrame* child = nsnull;
      rootFrame->FirstChild(this, nsnull, &child);
      nsresult rv;
      nsCOMPtr<nsIBox> box = do_QueryInterface(child, &rv);
      if (NS_SUCCEEDED(rv) && box) {
        nsBoxLayoutState state(this);
        box->MarkStyleChange(state);
      } else {
        // Force a reflow of the root frame
        // XXX We really should only do a reflow if a preference that affects
        // formatting changed, e.g., a font change. If it's just a color change
        // then we only need to repaint...
        mShell->StyleChangeReflow();
      }
    }
  }

  return NS_OK;
}

void
nsPresContext::PreferenceChanged(const char* aPrefName)
{
  nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryInterface(mContainer));
  if (docShell) {
    PRInt32 docShellType;
    docShell->GetItemType(&docShellType);
    if (nsIDocShellTreeItem::typeChrome == docShellType)
      return;
  }

  // Initialize our state from the user preferences
  GetUserPreferences();

  // update the presShell: tell it to set the preference style rules up
  if (mShell) {
    mShell->SetPreferenceStyleRules(PR_TRUE);
  }

  if (mDeviceContext) {
    mDeviceContext->FlushFontCache();
    ClearStyleDataAndReflow();
  }
}

NS_IMETHODIMP
nsPresContext::Init(nsIDeviceContext* aDeviceContext)
{
  NS_ASSERTION(!(mInitialized == PR_TRUE), "attempt to reinit pres context");

  mDeviceContext = dont_QueryInterface(aDeviceContext);

  mLangService = do_GetService(NS_LANGUAGEATOMSERVICE_CONTRACTID);
  mPrefs = do_GetService(NS_PREF_CONTRACTID);
  if (mPrefs) {
    // Register callbacks so we're notified when the preferences change
    mPrefs->RegisterCallback("font.", nsPresContext::PrefChangedCallback, (void*)this);
    mPrefs->RegisterCallback("browser.display.", nsPresContext::PrefChangedCallback, (void*)this);
    mPrefs->RegisterCallback("browser.underline_anchors", nsPresContext::PrefChangedCallback, (void*)this);
    mPrefs->RegisterCallback("browser.anchor_color", nsPresContext::PrefChangedCallback, (void*)this);
    mPrefs->RegisterCallback("browser.visited_color", nsPresContext::PrefChangedCallback, (void*)this);
    mPrefs->RegisterCallback("network.image.imageBehavior", nsPresContext::PrefChangedCallback, (void*)this);
    mPrefs->RegisterCallback("image.animation_mode", nsPresContext::PrefChangedCallback, (void*)this);

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

        if (mBaseURL) {
            PRBool isChrome = PR_FALSE;
            PRBool isRes = PR_FALSE;
            mBaseURL->SchemeIs("chrome", &isChrome);
            mBaseURL->SchemeIs("resource", &isRes);

          if (!isChrome && !isRes)
            mImageAnimationMode = mImageAnimationModePref;
          else
            mImageAnimationMode = eImageAnimation_Normal;
        }

        if (mLangService) {
          nsAutoString charset;
          doc->AddCharSetObserver(this);
          doc->GetDocumentCharacterSet(charset);
          UpdateCharSet(charset.get());
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

void
nsPresContext::UpdateCharSet(const PRUnichar* aCharSet)
{
  if (mLangService) {
    mLangService->LookupCharSet(aCharSet, getter_AddRefs(mLanguage));
    GetFontPreferences();
    if (mLanguage) {
      nsCOMPtr<nsIAtom> langGroupAtom;
      mLanguage->GetLanguageGroup(getter_AddRefs(langGroupAtom));
      NS_ASSERTION(langGroupAtom, "non-NULL language group atom expected");
      if (langGroupAtom.get() == nsLayoutAtoms::Japanese) {
        mLanguageSpecificTransformType =
        eLanguageSpecificTransformType_Japanese;
      }
      else if (langGroupAtom.get() == nsLayoutAtoms::Korean) {
        mLanguageSpecificTransformType =
        eLanguageSpecificTransformType_Korean;
      }
      else {
        mLanguageSpecificTransformType =
        eLanguageSpecificTransformType_None;
      }
    }
  }
#ifdef IBMBIDI
  //ahmed
  mCharset=aCharSet;

  SetVisualMode(IsVisualCharset(mCharset) );
#endif // IBMBIDI
}

NS_IMETHODIMP
nsPresContext::Observe(nsISupports* aSubject, const PRUnichar* aTopic,
                       const PRUnichar* aData)
{
  if (nsAutoString(aTopic).EqualsWithConversion("charset")) {
    UpdateCharSet(aData);
  }
  else {
    NS_WARNING("unrecognized topic in nsPresContext::Observe");
    return NS_ERROR_FAILURE;
  }

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

  // enable/disable the QuirkSheet
  NS_ASSERTION(mShell, "PresShell must be set on PresContext before calling nsPresContext::SetCompatibilityMode");
  if (mShell) {
    nsCOMPtr<nsIStyleSet> set;
    nsresult rv = mShell->GetStyleSet(getter_AddRefs(set));
    if (NS_SUCCEEDED(rv) && set) {
      set->EnableQuirkStyleSheet((mCompatibilityMode != eCompatibility_Standard) ? PR_TRUE : PR_FALSE);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsPresContext::GetWidgetRenderingMode(nsWidgetRendering* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
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
nsPresContext::GetImageAnimationMode(nsImageAnimation* aModeResult)
{
  NS_ENSURE_ARG_POINTER(aModeResult);
  *aModeResult = mImageAnimationMode;
  return NS_OK;
}


NS_IMETHODIMP
nsPresContext::SetImageAnimationMode(nsImageAnimation aMode)
{
  mImageAnimationMode = aMode;
  return NS_OK;
}

/* This function has now been depricated.  It is no longer necesary to
 * hold on to presContext just to get a nsLookAndFeel.  nsLookAndFeel is
 * now a service provided by ServiceManager.
 */
NS_IMETHODIMP
nsPresContext::GetLookAndFeel(nsILookAndFeel** aLookAndFeel)
{
  NS_PRECONDITION(nsnull != aLookAndFeel, "null ptr");
  if (nsnull == aLookAndFeel) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult result = NS_OK;
  if (! mLookAndFeel) {
    mLookAndFeel = do_GetService(kLookAndFeelCID,&result);
  }
  *aLookAndFeel = mLookAndFeel;
  NS_IF_ADDREF(*aLookAndFeel);
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
  return ResolvePseudoStyleWithComparator(aParentContent, aPseudoTag, aParentContext,
                                          aForceUnique, nsnull, aResult);
}

NS_IMETHODIMP
nsPresContext::ResolvePseudoStyleWithComparator(nsIContent* aParentContent,
                                                nsIAtom* aPseudoTag,
                                                nsIStyleContext* aParentContext,
                                                PRBool aForceUnique,
                                                nsICSSPseudoComparator* aComparator,
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
                                          aParentContext, aForceUnique, aComparator);
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
nsPresContext::AllocateFromShell(size_t aSize, void** aResult)
{
  if (mShell)
    return mShell->AllocateFrame(aSize, aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::FreeToShell(size_t aSize, void* aFreeChunk)
{
  if (mShell)
    return mShell->FreeFrame(aSize, aFreeChunk);
  return NS_OK;
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
    nsCOMPtr<nsIAtom> langGroup;
    if (mLanguage) {
      mLanguage->GetLanguageGroup(getter_AddRefs(langGroup));
    }
    mDeviceContext->GetMetricsFor(aFont, langGroup, metrics);
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
nsPresContext::GetDefaultLinkColor(nscolor* aColor)
{
  NS_PRECONDITION(nsnull != aColor, "null argument");
  if (aColor) {
    *aColor = mLinkColor;
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP 
nsPresContext::GetDefaultVisitedLinkColor(nscolor* aColor)
{
  NS_PRECONDITION(nsnull != aColor, "null argument");
  if (aColor) {
    *aColor = mVisitedLinkColor;
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP
nsPresContext::GetFocusRingOnAnything(PRBool& aFocusRingOnAnything)
{
  aFocusRingOnAnything = mFocusRingOnAnything;
  return NS_OK;
}


NS_IMETHODIMP
nsPresContext::GetUseFocusColors(PRBool& aUseFocusColors)
{
  aUseFocusColors = mUseFocusColors;
  return NS_OK;
}


NS_IMETHODIMP
nsPresContext::GetFocusTextColor(nscolor* aColor)
{
  NS_PRECONDITION(nsnull != aColor, "null argument");
  if (aColor) {
    *aColor = mFocusTextColor;
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP
nsPresContext::GetFocusBackgroundColor(nscolor* aColor)
{
  NS_PRECONDITION(nsnull != aColor, "null argument");
  if (aColor) {
    *aColor = mFocusBackgroundColor;
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsPresContext::GetFocusRingWidth(PRUint8 *aFocusRingWidth)
{
  NS_PRECONDITION(nsnull != aFocusRingWidth, "null argument");
  if (aFocusRingWidth) {
    *aFocusRingWidth = mFocusRingWidth;
    return NS_OK;
    }
  return NS_ERROR_NULL_POINTER;
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
nsPresContext::SetDefaultLinkColor(nscolor aColor)
{
  mLinkColor = aColor;
  return NS_OK;
}

NS_IMETHODIMP 
nsPresContext::SetDefaultVisitedLinkColor(nscolor aColor)
{
  mVisitedLinkColor = aColor;
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
nsPresContext::LoadImage(const nsString& aURL,
                         nsIFrame* aTargetFrame,
                         imgIRequest **aRequest)
{
  // look and see if we have a loader for the target frame.

  nsVoidKey key(aTargetFrame);
  nsImageLoader *loader = NS_REINTERPRET_CAST(nsImageLoader*, mImageLoaders.Get(&key)); // addrefs

  if (!loader) {
    nsImageLoader *newLoader = new nsImageLoader();
    if (!newLoader)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(newLoader); // new

    nsCOMPtr<nsISupports> sup;
    newLoader->QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(sup));

    loader = NS_REINTERPRET_CAST(nsImageLoader*, sup.get());

    loader->Init(aTargetFrame, this);

    mImageLoaders.Put(&key, sup);
  }

  // Allow for a null target frame argument (for precached images)
  if (aTargetFrame) {
    // Mark frame as having loaded an image
    nsFrameState state;
    aTargetFrame->GetFrameState(&state);
    state |= NS_FRAME_HAS_LOADED_IMAGES;
    aTargetFrame->SetFrameState(state);
  }

  loader->Load(aURL);

  loader->GetRequest(aRequest);

  NS_RELEASE(loader);

  return NS_OK;
}


NS_IMETHODIMP
nsPresContext::StopImagesFor(nsIFrame* aTargetFrame)
{
  nsVoidKey key(aTargetFrame);
  nsImageLoader *loader = NS_REINTERPRET_CAST(nsImageLoader*, mImageLoaders.Get(&key)); // addrefs

  if (loader) {
    loader->Destroy();
    NS_RELEASE(loader);

    mImageLoaders.Remove(&key);
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
    nsresult rv;
    mEventManager = do_CreateInstance(kEventStateManagerCID,&rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  //Not refcnted, set null in destructor
  mEventManager->SetPresContext(this);

  *aManager = mEventManager;
  NS_IF_ADDREF(*aManager);
  return NS_OK;
}

#ifdef IBMBIDI
//ahmed
NS_IMETHODIMP
nsPresContext::IsArabicEncoding(PRBool& aResult)
{
  aResult=PR_FALSE;
  if ( (mCharset.EqualsIgnoreCase("ibm864") )||(mCharset.EqualsIgnoreCase("ibm864i") )||(mCharset.EqualsIgnoreCase("windows-1256") )||(mCharset.EqualsIgnoreCase("iso-8859-6") ))
    aResult=PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::IsVisRTL(PRBool& aResult)
{
  aResult=PR_FALSE;
  if ( (mIsVisual)&&(GET_BIDI_OPTION_DIRECTION(mBidi) == IBMBIDI_TEXTDIRECTION_RTL) )
    aResult=PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetBidiEnabled(PRBool* aBidiEnabled) const
{
  NS_ENSURE_ARG_POINTER(aBidiEnabled);
  *aBidiEnabled = PR_FALSE;
  NS_ASSERTION(mShell, "PresShell must be set on PresContext before calling nsPresContext::GetBidiEnabled");
  if (mShell) {
    nsCOMPtr<nsIDocument> doc;
    mShell->GetDocument(getter_AddRefs(doc) );
    NS_ASSERTION(doc, "PresShell has no document in nsPresContext::GetBidiEnabled");
    if (doc) {
      doc->GetBidiEnabled(aBidiEnabled);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::SetBidiEnabled(PRBool aBidiEnabled) const
{
  if (mShell) {
    nsCOMPtr<nsIDocument> doc;
    mShell->GetDocument(getter_AddRefs(doc) );
    if (doc) {
      doc->SetBidiEnabled(aBidiEnabled);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::SetVisualMode(PRBool aIsVisual)
{
  mIsVisual = aIsVisual;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::IsVisualMode(PRBool& aIsVisual) const
{
  aIsVisual = mIsVisual;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetBidiUtils(nsBidiPresUtils** aBidiUtils)
{
  nsresult rv = NS_OK;

  if (!mBidiUtils) {
    mBidiUtils = new nsBidiPresUtils;
    if (!mBidiUtils) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  *aBidiUtils = mBidiUtils;
  return rv;
}

NS_IMETHODIMP   nsPresContext::SetBidi(PRUint32 aSource, PRBool aForceReflow)
{
  mBidi = aSource;
  if (IBMBIDI_TEXTDIRECTION_RTL == GET_BIDI_OPTION_DIRECTION(mBidi)
      || IBMBIDI_NUMERAL_HINDI == GET_BIDI_OPTION_NUMERAL(mBidi)) {
    SetBidiEnabled(PR_TRUE);
  }
  if (IBMBIDI_TEXTTYPE_VISUAL == GET_BIDI_OPTION_TEXTTYPE(mBidi)) {
    SetVisualMode(PR_TRUE);
  }
  else if (IBMBIDI_TEXTTYPE_LOGICAL == GET_BIDI_OPTION_TEXTTYPE(mBidi)) {
    SetVisualMode(PR_FALSE);
  }
  else {
    SetVisualMode(IsVisualCharset(mCharset) );
  }
  if (mShell && aForceReflow) {
    ClearStyleDataAndReflow();
  }
  return NS_OK;
}
NS_IMETHODIMP   nsPresContext::GetBidi(PRUint32* aDest)
{
  if (aDest)
    *aDest = mBidi;
  return NS_OK;
}

//Mohamed 17-1-01
NS_IMETHODIMP
nsPresContext::SetIsBidiSystem(PRBool aIsBidi)
{
  mIsBidiSystem = aIsBidi;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetIsBidiSystem(PRBool& aResult) const
{
  aResult = mIsBidiSystem;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetBidiCharset(nsAWritableString &aCharSet)
{
  aCharSet = mCharset;
  return NS_OK;
}
//Mohamed End

#endif //IBMBIDI

NS_IMETHODIMP
nsPresContext::GetLanguage(nsILanguageAtom** aLanguage)
{
  NS_ENSURE_ARG_POINTER(aLanguage);

  *aLanguage = mLanguage;
  NS_IF_ADDREF(*aLanguage);

  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetLanguageSpecificTransformType(
                nsLanguageSpecificTransformType* aType)
{
  NS_ENSURE_ARG_POINTER(aType);
  *aType = mLanguageSpecificTransformType;

  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::IsRenderingOnlySelection(PRBool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mIsRenderingOnlySelection;

  return NS_OK;
}

#ifdef MOZ_REFLOW_PERF
NS_IMETHODIMP
nsPresContext::CountReflows(const char * aName, PRUint32 aType, nsIFrame * aFrame)
{
  if (mShell) {
    mShell->CountReflows(aName, aType, aFrame);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::PaintCount(const char * aName, nsIRenderingContext* aRenderingContext, nsIFrame * aFrame, PRUint32 aColor)
{
  if (mShell) {
    mShell->PaintCount(aName, aRenderingContext, this, aFrame, aColor);
  }
  return NS_OK;
}
#endif
