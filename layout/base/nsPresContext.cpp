/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* a presentation of a document, part 1 */

#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsILinkHandler.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsPIDOMWindow.h"
#include "nsStyleSet.h"
#include "nsImageLoader.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIRenderingContext.h"
#include "nsIURL.h"
#include "nsIDocument.h"
#include "nsStyleContext.h"
#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsIURIContentListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIServiceManager.h"
#include "nsIDOMElement.h"
#include "nsContentPolicyUtils.h"
#include "nsIDOMWindow.h"
#include "nsXPIDLString.h"
#include "nsIWeakReferenceUtils.h"
#include "nsCSSRendering.h"
#include "prprf.h"
#include "nsContentPolicyUtils.h"
#include "nsIDOMDocument.h"
#include "nsAutoPtr.h"
#include "nsEventStateManager.h"
#include "nsThreadUtils.h"
#include "nsFrameManager.h"
#include "nsLayoutUtils.h"
#include "nsIViewManager.h"
#include "nsCSSFrameConstructor.h"
#include "nsCSSRuleProcessor.h"
#include "nsStyleChangeList.h"
#include "nsRuleNode.h"
#include "nsEventDispatcher.h"
#include "gfxUserFontSet.h"
#include "gfxPlatform.h"
#include "nsCSSRules.h"
#include "nsFontFaceLoader.h"
#include "nsIEventListenerManager.h"
#include "nsStyleStructInlines.h"
#include "nsIAppShell.h"
#include "prenv.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsObjectFrame.h"
#include "nsTransitionManager.h"
#include "mozilla/dom/Element.h"
#include "nsIFrameMessageManager.h"

#ifdef MOZ_SMIL
#include "nsSMILAnimationController.h"
#endif // MOZ_SMIL

#ifdef IBMBIDI
#include "nsBidiPresUtils.h"
#endif // IBMBIDI

#include "nsContentUtils.h"
#include "nsPIWindowRoot.h"

// Needed for Start/Stop of Image Animation
#include "imgIContainer.h"
#include "nsIImageLoadingContent.h"

//needed for resetting of image service color
#include "nsLayoutCID.h"

using mozilla::TimeDuration;
using mozilla::TimeStamp;
using namespace mozilla::dom;

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

int
nsPresContext::PrefChangedCallback(const char* aPrefName, void* instance_data)
{
  nsPresContext*  presContext = (nsPresContext*)instance_data;

  NS_ASSERTION(nsnull != presContext, "bad instance data");
  if (nsnull != presContext) {
    presContext->PreferenceChanged(aPrefName);
  }
  return 0;  // PREF_OK
}


void
nsPresContext::PrefChangedUpdateTimerCallback(nsITimer *aTimer, void *aClosure)
{
  nsPresContext*  presContext = (nsPresContext*)aClosure;
  NS_ASSERTION(presContext != nsnull, "bad instance data");
  if (presContext)
    presContext->UpdateAfterPreferencesChanged();
}

#ifdef IBMBIDI
static PRBool
IsVisualCharset(const nsCString& aCharset)
{
  if (aCharset.LowerCaseEqualsLiteral("ibm864")             // Arabic//ahmed
      || aCharset.LowerCaseEqualsLiteral("ibm862")          // Hebrew
      || aCharset.LowerCaseEqualsLiteral("iso-8859-8") ) {  // Hebrew
    return PR_TRUE; // visual text type
  }
  else {
    return PR_FALSE; // logical text type
  }
}
#endif // IBMBIDI


static PLDHashOperator
destroy_loads(const void * aKey, nsRefPtr<nsImageLoader>& aData, void* closure)
{
  aData->Destroy();
  return PL_DHASH_NEXT;
}

static NS_DEFINE_CID(kLookAndFeelCID,  NS_LOOKANDFEEL_CID);
#include "nsContentCID.h"

  // NOTE! nsPresContext::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

nsPresContext::nsPresContext(nsIDocument* aDocument, nsPresContextType aType)
  : mType(aType), mDocument(aDocument), mTextZoom(1.0), mFullZoom(1.0),
    mPageSize(-1, -1), mPPScale(1.0f),
    mViewportStyleOverflow(NS_STYLE_OVERFLOW_AUTO, NS_STYLE_OVERFLOW_AUTO),
    mImageAnimationModePref(imgIContainer::kNormalAnimMode),
    // Font sizes default to zero; they will be set in GetFontPreferences
    mDefaultVariableFont("serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
                         NS_FONT_WEIGHT_NORMAL, NS_FONT_STRETCH_NORMAL, 0, 0),
    mDefaultFixedFont("monospace", NS_FONT_STYLE_NORMAL,
                      NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL,
                      NS_FONT_STRETCH_NORMAL, 0, 0),
    mDefaultSerifFont("serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
                      NS_FONT_WEIGHT_NORMAL, NS_FONT_STRETCH_NORMAL, 0, 0),
    mDefaultSansSerifFont("sans-serif", NS_FONT_STYLE_NORMAL,
                          NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL,
                          NS_FONT_STRETCH_NORMAL, 0, 0),
    mDefaultMonospaceFont("monospace", NS_FONT_STYLE_NORMAL,
                          NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL,
                          NS_FONT_STRETCH_NORMAL, 0, 0),
    mDefaultCursiveFont("cursive", NS_FONT_STYLE_NORMAL,
                        NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL,
                        NS_FONT_STRETCH_NORMAL, 0, 0),
    mDefaultFantasyFont("fantasy", NS_FONT_STYLE_NORMAL,
                        NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL,
                        NS_FONT_STRETCH_NORMAL, 0, 0)
{
  // NOTE! nsPresContext::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

  mDoScaledTwips = PR_TRUE;

  SetBackgroundImageDraw(PR_TRUE);		// always draw the background
  SetBackgroundColorDraw(PR_TRUE);

  mBackgroundColor = NS_RGB(0xFF, 0xFF, 0xFF);
  
  mUseDocumentColors = PR_TRUE;
  mUseDocumentFonts = PR_TRUE;

  // the minimum font-size is unconstrained by default

  mLinkColor = NS_RGB(0x00, 0x00, 0xEE);
  mActiveLinkColor = NS_RGB(0xEE, 0x00, 0x00);
  mVisitedLinkColor = NS_RGB(0x55, 0x1A, 0x8B);
  mUnderlineLinks = PR_TRUE;

  mFocusTextColor = mDefaultColor;
  mFocusBackgroundColor = mBackgroundColor;
  mFocusRingWidth = 1;

  if (aType == eContext_Galley) {
    mMedium = nsGkAtoms::screen;
  } else {
    mMedium = nsGkAtoms::print;
    mPaginated = PR_TRUE;
  }

  if (!IsDynamic()) {
    mImageAnimationMode = imgIContainer::kDontAnimMode;
    mNeverAnimate = PR_TRUE;
  } else {
    mImageAnimationMode = imgIContainer::kNormalAnimMode;
    mNeverAnimate = PR_FALSE;
  }
  NS_ASSERTION(mDocument, "Null document");
  mUserFontSet = nsnull;
  mUserFontSetDirty = PR_TRUE;
}

nsPresContext::~nsPresContext()
{
  NS_PRECONDITION(!mShell, "Presshell forgot to clear our mShell pointer");
  SetShell(nsnull);

  if (mTransitionManager) {
    mTransitionManager->Disconnect();
  }

  // Disconnect the refresh driver *after* the transition manager, which
  // needs it.
  if (mRefreshDriver && mRefreshDriver->PresContext() == this) {
    mRefreshDriver->Disconnect();
  }

  if (mEventManager) {
    // unclear if these are needed, but can't hurt
    mEventManager->NotifyDestroyPresContext(this);
    mEventManager->SetPresContext(nsnull);

    NS_RELEASE(mEventManager);
  }

  if (mPrefChangedTimer)
  {
    mPrefChangedTimer->Cancel();
    mPrefChangedTimer = nsnull;
  }

  // Unregister preference callbacks
  nsContentUtils::UnregisterPrefCallback("font.",
                                         nsPresContext::PrefChangedCallback,
                                         this);
  nsContentUtils::UnregisterPrefCallback("browser.display.",
                                         nsPresContext::PrefChangedCallback,
                                         this);
  nsContentUtils::UnregisterPrefCallback("browser.underline_anchors",
                                         nsPresContext::PrefChangedCallback,
                                         this);
  nsContentUtils::UnregisterPrefCallback("browser.anchor_color",
                                         nsPresContext::PrefChangedCallback,
                                         this);
  nsContentUtils::UnregisterPrefCallback("browser.active_color",
                                         nsPresContext::PrefChangedCallback,
                                         this);
  nsContentUtils::UnregisterPrefCallback("browser.visited_color",
                                         nsPresContext::PrefChangedCallback,
                                         this);
  nsContentUtils::UnregisterPrefCallback("image.animation_mode",
                                         nsPresContext::PrefChangedCallback,
                                         this);
#ifdef IBMBIDI
  nsContentUtils::UnregisterPrefCallback("bidi.", PrefChangedCallback, this);

  delete mBidiUtils;
#endif // IBMBIDI
  nsContentUtils::UnregisterPrefCallback("gfx.font_rendering.",
                                         nsPresContext::PrefChangedCallback,
                                         this);
  nsContentUtils::UnregisterPrefCallback("layout.css.dpi",
                                         nsPresContext::PrefChangedCallback,
                                         this);
  nsContentUtils::UnregisterPrefCallback("layout.css.devPixelsPerPx",
                                         nsPresContext::PrefChangedCallback,
                                         this);

  NS_IF_RELEASE(mDeviceContext);
  NS_IF_RELEASE(mLookAndFeel);
  NS_IF_RELEASE(mLanguage);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsPresContext)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPresContext)
   NS_INTERFACE_MAP_ENTRY(nsISupports)
   NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPresContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPresContext)

static PLDHashOperator
TraverseImageLoader(const void * aKey, nsRefPtr<nsImageLoader>& aData,
                    void* aClosure)
{
  nsCycleCollectionTraversalCallback *cb =
    static_cast<nsCycleCollectionTraversalCallback*>(aClosure);

  cb->NoteXPCOMChild(aData);

  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsPresContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocument);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mDeviceContext); // worth bothering?
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mEventManager);
  // NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mLookAndFeel); // a service
  // NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mLanguage); // an atom

  for (PRUint32 i = 0; i < IMAGE_LOAD_TYPE_COUNT; ++i)
    tmp->mImageLoaders[i].Enumerate(TraverseImageLoader, &cb);

  // NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mTheme); // a service
  // NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mLangService); // a service
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mPrintSettings);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mPrefChangedTimer);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsPresContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocument);
  NS_RELEASE(tmp->mDeviceContext); // worth bothering?
  if (tmp->mEventManager) {
    // unclear if these are needed, but can't hurt
    tmp->mEventManager->NotifyDestroyPresContext(tmp);
    tmp->mEventManager->SetPresContext(nsnull);

    NS_RELEASE(tmp->mEventManager);
  }

  // NS_RELEASE(tmp->mLookAndFeel); // a service
  // NS_RELEASE(tmp->mLanguage); // an atom

  // NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mTheme); // a service
  // NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mLangService); // a service
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mPrintSettings);
  if (tmp->mPrefChangedTimer)
  {
    tmp->mPrefChangedTimer->Cancel();
    tmp->mPrefChangedTimer = nsnull;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


#define MAKE_FONT_PREF_KEY(_pref, _s0, _s1) \
 _pref.Assign(_s0); \
 _pref.Append(_s1);

static const char* const kGenericFont[] = {
  ".variable.",
  ".fixed.",
  ".serif.", 
  ".sans-serif.", 
  ".monospace.",
  ".cursive.",
  ".fantasy."
};

// whether no native theme service exists;
// if this gets set to true, we'll stop asking for it.
static PRBool sNoTheme = PR_FALSE;

// Set to true when LookAndFeelChanged needs to be called.  This is used
// because the look and feel is a service, so there's no need to notify it from
// more than one prescontext.
static PRBool sLookAndFeelChanged;

// Set to true when ThemeChanged needs to be called on mTheme.  This is used
// because mTheme is a service, so there's no need to notify it from more than
// one prescontext.
static PRBool sThemeChanged;

void
nsPresContext::GetFontPreferences()
{
  /* Fetch the font prefs to be used -- see bug 61883 for details.
     Not all prefs are needed upfront. Some are fallback prefs intended
     for the GFX font sub-system...

  1) unit : assumed to be the same for all language groups -------------
  font.size.unit = px | pt    XXX could be folded in the size... bug 90440

  2) attributes for generic fonts --------------------------------------
  font.default = serif | sans-serif - fallback generic font
  font.name.[generic].[langGroup] = current user' selected font on the pref dialog
  font.name-list.[generic].[langGroup] = fontname1, fontname2, ... [factory pre-built list]
  font.size.[generic].[langGroup] = integer - settable by the user
  font.size-adjust.[generic].[langGroup] = "float" - settable by the user
  font.minimum-size.[langGroup] = integer - settable by the user
  */

  mDefaultVariableFont.size = CSSPixelsToAppUnits(16);
  mDefaultFixedFont.size = CSSPixelsToAppUnits(13);

  // the font prefs are based on langGroup, not actual language
  nsCAutoString langGroup;
  if (mLanguage && mLangService) {
    nsresult rv;
    nsIAtom *group = mLangService->GetLanguageGroup(mLanguage, &rv);
    if (NS_SUCCEEDED(rv) && group) {
      group->ToUTF8String(langGroup);
    }
    else {
      langGroup.AssignLiteral("x-western"); // Assume x-western is safe...
    }
  }
  else {
    langGroup.AssignLiteral("x-western"); // Assume x-western is safe...
  }

  nsCAutoString pref;

  // get the current applicable font-size unit
  enum {eUnit_unknown = -1, eUnit_px, eUnit_pt};
  PRInt32 unit = eUnit_px;

  nsAdoptingCString cvalue =
    nsContentUtils::GetCharPref("font.size.unit");

  if (!cvalue.IsEmpty()) {
    if (cvalue.Equals("px")) {
      unit = eUnit_px;
    }
    else if (cvalue.Equals("pt")) {
      unit = eUnit_pt;
    }
    else {
      NS_WARNING("unexpected font-size unit -- expected: 'px' or 'pt'");
      unit = eUnit_unknown;
    }
  }

  // get font.minimum-size.[langGroup]

  pref.Assign("font.minimum-size.");
  pref.Append(langGroup);

  PRInt32 size = nsContentUtils::GetIntPref(pref.get());
  if (unit == eUnit_px) {
    mMinimumFontSize = CSSPixelsToAppUnits(size);
  }
  else if (unit == eUnit_pt) {
    mMinimumFontSize = CSSPointsToAppUnits(size);
  }

  // get attributes specific to each generic font
  nsCAutoString generic_dot_langGroup;
  for (PRInt32 eType = eDefaultFont_Variable; eType < eDefaultFont_COUNT; ++eType) {
    generic_dot_langGroup.Assign(kGenericFont[eType]);
    generic_dot_langGroup.Append(langGroup);

    nsFont* font;
    switch (eType) {
      case eDefaultFont_Variable:  font = &mDefaultVariableFont;  break;
      case eDefaultFont_Fixed:     font = &mDefaultFixedFont;     break;
      case eDefaultFont_Serif:     font = &mDefaultSerifFont;     break;
      case eDefaultFont_SansSerif: font = &mDefaultSansSerifFont; break;
      case eDefaultFont_Monospace: font = &mDefaultMonospaceFont; break;
      case eDefaultFont_Cursive:   font = &mDefaultCursiveFont;   break;
      case eDefaultFont_Fantasy:   font = &mDefaultFantasyFont;   break;
    }

    // set the default variable font (the other fonts are seen as 'generic' fonts
    // in GFX and will be queried there when hunting for alternative fonts)
    if (eType == eDefaultFont_Variable) {
      MAKE_FONT_PREF_KEY(pref, "font.name", generic_dot_langGroup);

      nsAdoptingString value =
        nsContentUtils::GetStringPref(pref.get());
      if (!value.IsEmpty()) {
        font->name.Assign(value);
      }
      else {
        MAKE_FONT_PREF_KEY(pref, "font.default.", langGroup);
        value = nsContentUtils::GetStringPref(pref.get());
        if (!value.IsEmpty()) {
          mDefaultVariableFont.name.Assign(value);
        }
      } 
    }
    else {
      if (eType == eDefaultFont_Monospace) {
        // This takes care of the confusion whereby people often expect "monospace" 
        // to have the same default font-size as "-moz-fixed" (this tentative
        // size may be overwritten with the specific value for "monospace" when
        // "font.size.monospace.[langGroup]" is read -- see below)
        font->size = mDefaultFixedFont.size;
      }
      else if (eType != eDefaultFont_Fixed) {
        // all the other generic fonts are initialized with the size of the
        // variable font, but their specific size can supersede later -- see below
        font->size = mDefaultVariableFont.size;
      }
    }

    // Bug 84398: for spec purists, a different font-size only applies to the
    // .variable. and .fixed. fonts and the other fonts should get |font-size-adjust|.
    // The problem is that only GfxWin has the support for |font-size-adjust|. So for
    // parity, we enable the ability to set a different font-size on all platforms.

    // get font.size.[generic].[langGroup]
    // size=0 means 'Auto', i.e., generic fonts retain the size of the variable font
    MAKE_FONT_PREF_KEY(pref, "font.size", generic_dot_langGroup);
    size = nsContentUtils::GetIntPref(pref.get());
    if (size > 0) {
      if (unit == eUnit_px) {
        font->size = CSSPixelsToAppUnits(size);
      }
      else if (unit == eUnit_pt) {
        font->size = CSSPointsToAppUnits(size);
      }
    }

    // get font.size-adjust.[generic].[langGroup]
    // XXX only applicable on GFX ports that handle |font-size-adjust|
    MAKE_FONT_PREF_KEY(pref, "font.size-adjust", generic_dot_langGroup);
    cvalue = nsContentUtils::GetCharPref(pref.get());
    if (!cvalue.IsEmpty()) {
      font->sizeAdjust = (float)atof(cvalue.get());
    }

#ifdef DEBUG_rbs
    printf("%s Family-list:%s size:%d sizeAdjust:%.2f\n",
           generic_dot_langGroup.get(),
           NS_ConvertUTF16toUTF8(font->name).get(), font->size,
           font->sizeAdjust);
#endif
  }
}

void
nsPresContext::GetDocumentColorPreferences()
{
  PRInt32 useAccessibilityTheme = 0;
  PRBool usePrefColors = PR_TRUE;
  nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryReferent(mContainer));
  if (docShell) {
    PRInt32 docShellType;
    docShell->GetItemType(&docShellType);
    if (nsIDocShellTreeItem::typeChrome == docShellType) {
      usePrefColors = PR_FALSE;
    }
    else {
      mLookAndFeel->GetMetric(nsILookAndFeel::eMetric_UseAccessibilityTheme, useAccessibilityTheme);
      usePrefColors = !useAccessibilityTheme;
    }

  }
  if (usePrefColors) {
    usePrefColors =
      !nsContentUtils::GetBoolPref("browser.display.use_system_colors",
                                   PR_FALSE);
  }

  if (usePrefColors) {
    nsAdoptingCString colorStr =
      nsContentUtils::GetCharPref("browser.display.foreground_color");

    if (!colorStr.IsEmpty()) {
      mDefaultColor = MakeColorPref(colorStr);
    }

    colorStr =
      nsContentUtils::GetCharPref("browser.display.background_color");

    if (!colorStr.IsEmpty()) {
      mBackgroundColor = MakeColorPref(colorStr);
    }
  }
  else {
    mDefaultColor = NS_RGB(0x00, 0x00, 0x00);
    mBackgroundColor = NS_RGB(0xFF, 0xFF, 0xFF);
    mLookAndFeel->GetColor(nsILookAndFeel::eColor_WindowForeground,
                           mDefaultColor);
    mLookAndFeel->GetColor(nsILookAndFeel::eColor_WindowBackground,
                           mBackgroundColor);
  }

  // Wherever we got the default background color from, ensure it is
  // opaque.
  mBackgroundColor = NS_ComposeColors(NS_RGB(0xFF, 0xFF, 0xFF),
                                      mBackgroundColor);

  mUseDocumentColors = !useAccessibilityTheme &&
    nsContentUtils::GetBoolPref("browser.display.use_document_colors",
                                mUseDocumentColors);
}

void
nsPresContext::GetUserPreferences()
{
  if (!GetPresShell()) {
    // No presshell means nothing to do here.  We'll do this when we
    // get a presshell.
    return;
  }
    
  mFontScaler =
    nsContentUtils::GetIntPref("browser.display.base_font_scaler",
                               mFontScaler);


  mAutoQualityMinFontSizePixelsPref =
    nsContentUtils::GetIntPref("browser.display.auto_quality_min_font_size");

  // * document colors
  GetDocumentColorPreferences();

  // * link colors
  mUnderlineLinks =
    nsContentUtils::GetBoolPref("browser.underline_anchors", mUnderlineLinks);

  nsAdoptingCString colorStr =
    nsContentUtils::GetCharPref("browser.anchor_color");

  if (!colorStr.IsEmpty()) {
    mLinkColor = MakeColorPref(colorStr);
  }

  colorStr =
    nsContentUtils::GetCharPref("browser.active_color");

  if (!colorStr.IsEmpty()) {
    mActiveLinkColor = MakeColorPref(colorStr);
  }

  colorStr = nsContentUtils::GetCharPref("browser.visited_color");

  if (!colorStr.IsEmpty()) {
    mVisitedLinkColor = MakeColorPref(colorStr);
  }

  mUseFocusColors =
    nsContentUtils::GetBoolPref("browser.display.use_focus_colors",
                                mUseFocusColors);

  mFocusTextColor = mDefaultColor;
  mFocusBackgroundColor = mBackgroundColor;

  colorStr = nsContentUtils::GetCharPref("browser.display.focus_text_color");

  if (!colorStr.IsEmpty()) {
    mFocusTextColor = MakeColorPref(colorStr);
  }

  colorStr =
    nsContentUtils::GetCharPref("browser.display.focus_background_color");

  if (!colorStr.IsEmpty()) {
    mFocusBackgroundColor = MakeColorPref(colorStr);
  }

  mFocusRingWidth =
    nsContentUtils::GetIntPref("browser.display.focus_ring_width",
                               mFocusRingWidth);

  mFocusRingOnAnything =
    nsContentUtils::GetBoolPref("browser.display.focus_ring_on_anything",
                                mFocusRingOnAnything);

  mFocusRingStyle =
          nsContentUtils::GetIntPref("browser.display.focus_ring_style",
                                      mFocusRingStyle);
  // * use fonts?
  mUseDocumentFonts =
    nsContentUtils::GetIntPref("browser.display.use_document_fonts") != 0;

  // * replace backslashes with Yen signs? (bug 245770)
  mEnableJapaneseTransform =
    nsContentUtils::GetBoolPref("layout.enable_japanese_specific_transform");

  mPrefScrollbarSide =
    nsContentUtils::GetIntPref("layout.scrollbar.side");

  GetFontPreferences();

  // * image animation
  const nsAdoptingCString& animatePref =
    nsContentUtils::GetCharPref("image.animation_mode");
  if (animatePref.Equals("normal"))
    mImageAnimationModePref = imgIContainer::kNormalAnimMode;
  else if (animatePref.Equals("none"))
    mImageAnimationModePref = imgIContainer::kDontAnimMode;
  else if (animatePref.Equals("once"))
    mImageAnimationModePref = imgIContainer::kLoopOnceAnimMode;
  else // dynamic change to invalid value should act like it does initially
    mImageAnimationModePref = imgIContainer::kNormalAnimMode;

  PRUint32 bidiOptions = GetBidi();

  PRInt32 prefInt =
    nsContentUtils::GetIntPref(IBMBIDI_TEXTDIRECTION_STR,
                               GET_BIDI_OPTION_DIRECTION(bidiOptions));
  SET_BIDI_OPTION_DIRECTION(bidiOptions, prefInt);
  mPrefBidiDirection = prefInt;

  prefInt =
    nsContentUtils::GetIntPref(IBMBIDI_TEXTTYPE_STR,
                               GET_BIDI_OPTION_TEXTTYPE(bidiOptions));
  SET_BIDI_OPTION_TEXTTYPE(bidiOptions, prefInt);

  prefInt =
    nsContentUtils::GetIntPref(IBMBIDI_NUMERAL_STR,
                               GET_BIDI_OPTION_NUMERAL(bidiOptions));
  SET_BIDI_OPTION_NUMERAL(bidiOptions, prefInt);

  prefInt =
    nsContentUtils::GetIntPref(IBMBIDI_SUPPORTMODE_STR,
                               GET_BIDI_OPTION_SUPPORT(bidiOptions));
  SET_BIDI_OPTION_SUPPORT(bidiOptions, prefInt);

  prefInt =
    nsContentUtils::GetIntPref(IBMBIDI_CHARSET_STR,
                               GET_BIDI_OPTION_CHARACTERSET(bidiOptions));
  SET_BIDI_OPTION_CHARACTERSET(bidiOptions, prefInt);

  // We don't need to force reflow: either we are initializing a new
  // prescontext or we are being called from UpdateAfterPreferencesChanged()
  // which triggers a reflow anyway.
  SetBidi(bidiOptions, PR_FALSE);
}

void
nsPresContext::PreferenceChanged(const char* aPrefName)
{
  nsDependentCString prefName(aPrefName);
  if (prefName.EqualsLiteral("layout.css.dpi") ||
      prefName.EqualsLiteral("layout.css.devPixelsPerPx")) {
    PRInt32 oldAppUnitsPerDevPixel = AppUnitsPerDevPixel();
    if (mDeviceContext->CheckDPIChange() && mShell) {
      mDeviceContext->FlushFontCache();

      // Re-fetch the view manager's window dimensions in case there's a deferred
      // resize which hasn't affected our mVisibleArea yet
      nscoord oldWidthAppUnits, oldHeightAppUnits;
      nsIViewManager* vm = mShell->GetViewManager();
      vm->GetWindowDimensions(&oldWidthAppUnits, &oldHeightAppUnits);
      float oldWidthDevPixels = oldWidthAppUnits/oldAppUnitsPerDevPixel;
      float oldHeightDevPixels = oldHeightAppUnits/oldAppUnitsPerDevPixel;

      nscoord width = NSToCoordRound(oldWidthDevPixels*AppUnitsPerDevPixel());
      nscoord height = NSToCoordRound(oldHeightDevPixels*AppUnitsPerDevPixel());
      vm->SetWindowDimensions(width, height);

      MediaFeatureValuesChanged(PR_TRUE);
      RebuildAllStyleData(NS_STYLE_HINT_REFLOW);
    }
    return;
  }
  if (StringBeginsWith(prefName, NS_LITERAL_CSTRING("font."))) {
    // Changes to font family preferences don't change anything in the
    // computed style data, so the style system won't generate a reflow
    // hint for us.  We need to do that manually.

    // FIXME We could probably also handle changes to
    // browser.display.auto_quality_min_font_size here, but that
    // probably also requires clearing the text run cache, so don't
    // bother (yet, anyway).
    mPrefChangePendingNeedsReflow = PR_TRUE;
  }
  if (StringBeginsWith(prefName, NS_LITERAL_CSTRING("bidi."))) {
    // Changes to bidi prefs need to trigger a reflow (see bug 443629)
    mPrefChangePendingNeedsReflow = PR_TRUE;

    // Changes to bidi.numeral also needs to empty the text run cache.
    // This is handled in gfxTextRunWordCache.cpp.
  }
  if (StringBeginsWith(prefName, NS_LITERAL_CSTRING("gfx.font_rendering."))) {
    // Changes to font_rendering prefs need to trigger a reflow
    mPrefChangePendingNeedsReflow = PR_TRUE;
  }
  // we use a zero-delay timer to coalesce multiple pref updates
  if (!mPrefChangedTimer)
  {
    mPrefChangedTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (!mPrefChangedTimer)
      return;
    mPrefChangedTimer->InitWithFuncCallback(nsPresContext::PrefChangedUpdateTimerCallback, (void*)this, 0, nsITimer::TYPE_ONE_SHOT);
  }
}

void
nsPresContext::UpdateAfterPreferencesChanged()
{
  mPrefChangedTimer = nsnull;

  nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryReferent(mContainer));
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

  mDeviceContext->FlushFontCache();

  nsChangeHint hint = nsChangeHint(0);

  if (mPrefChangePendingNeedsReflow) {
    NS_UpdateHint(hint, NS_STYLE_HINT_REFLOW);
  }

  RebuildAllStyleData(hint);
}

nsresult
nsPresContext::Init(nsIDeviceContext* aDeviceContext)
{
  NS_ASSERTION(!mInitialized, "attempt to reinit pres context");
  NS_ENSURE_ARG(aDeviceContext);

  mDeviceContext = aDeviceContext;
  NS_ADDREF(mDeviceContext);

  if (mDeviceContext->SetPixelScale(mFullZoom))
    mDeviceContext->FlushFontCache();
  mCurAppUnitsPerDevPixel = AppUnitsPerDevPixel();

  for (PRUint32 i = 0; i < IMAGE_LOAD_TYPE_COUNT; ++i)
    if (!mImageLoaders[i].Init())
      return NS_ERROR_OUT_OF_MEMORY;
  
  // Get the look and feel service here; default colors will be initialized
  // from calling GetUserPreferences() when we get a presshell.
  nsresult rv = CallGetService(kLookAndFeelCID, &mLookAndFeel);
  if (NS_FAILED(rv)) {
    NS_ERROR("LookAndFeel service must be implemented for this toolkit");
    return rv;
  }

  mEventManager = new nsEventStateManager();
  if (!mEventManager)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(mEventManager);

  mTransitionManager = new nsTransitionManager(this);
  if (!mTransitionManager)
    return NS_ERROR_OUT_OF_MEMORY;

  if (mDocument->GetDisplayDocument()) {
    NS_ASSERTION(mDocument->GetDisplayDocument()->GetShell() &&
                 mDocument->GetDisplayDocument()->GetShell()->GetPresContext(),
                 "Why are we being initialized?");
    mRefreshDriver = mDocument->GetDisplayDocument()->GetShell()->
      GetPresContext()->RefreshDriver();
  } else {
    nsIDocument* parent = mDocument->GetParentDocument();
    // Unfortunately, sometimes |parent| here has no presshell because
    // printing screws up things.  Assert that in other cases it does,
    // but whenever the shell is null just fall back on using our own
    // refresh driver.
    NS_ASSERTION(!parent || mDocument->IsStaticDocument() || parent->GetShell(),
                 "How did we end up with a presshell if our parent doesn't "
                 "have one?");
    if (parent && parent->GetShell()) {
      NS_ASSERTION(parent->GetShell()->GetPresContext(),
                   "How did we get a presshell?");

      // We don't have our container set yet at this point
      nsCOMPtr<nsISupports> ourContainer = mDocument->GetContainer();

      nsCOMPtr<nsIDocShellTreeItem> ourItem = do_QueryInterface(ourContainer);
      if (ourItem) {
        nsCOMPtr<nsIDocShellTreeItem> parentItem;
        ourItem->GetSameTypeParent(getter_AddRefs(parentItem));
        if (parentItem) {
          mRefreshDriver = parent->GetShell()->GetPresContext()->RefreshDriver();
        }
      }
    }

    if (!mRefreshDriver) {
      mRefreshDriver = new nsRefreshDriver(this);
      if (!mRefreshDriver)
        return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  mLangService = do_GetService(NS_LANGUAGEATOMSERVICE_CONTRACTID);

  // Register callbacks so we're notified when the preferences change
  nsContentUtils::RegisterPrefCallback("font.",
                                       nsPresContext::PrefChangedCallback,
                                       this);
  nsContentUtils::RegisterPrefCallback("browser.display.",
                                       nsPresContext::PrefChangedCallback,
                                       this);
  nsContentUtils::RegisterPrefCallback("browser.underline_anchors",
                                       nsPresContext::PrefChangedCallback,
                                       this);
  nsContentUtils::RegisterPrefCallback("browser.anchor_color",
                                       nsPresContext::PrefChangedCallback,
                                       this);
  nsContentUtils::RegisterPrefCallback("browser.active_color",
                                       nsPresContext::PrefChangedCallback,
                                       this);
  nsContentUtils::RegisterPrefCallback("browser.visited_color",
                                       nsPresContext::PrefChangedCallback,
                                       this);
  nsContentUtils::RegisterPrefCallback("image.animation_mode",
                                       nsPresContext::PrefChangedCallback,
                                       this);
#ifdef IBMBIDI
  nsContentUtils::RegisterPrefCallback("bidi.", PrefChangedCallback,
                                       this);
#endif
  nsContentUtils::RegisterPrefCallback("gfx.font_rendering.", PrefChangedCallback,
                                       this);
  nsContentUtils::RegisterPrefCallback("layout.css.dpi",
                                       nsPresContext::PrefChangedCallback,
                                       this);
  nsContentUtils::RegisterPrefCallback("layout.css.devPixelsPerPx",
                                       nsPresContext::PrefChangedCallback,
                                       this);

  rv = mEventManager->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  mEventManager->SetPresContext(this);

#ifdef DEBUG
  mInitialized = PR_TRUE;
#endif

  mBorderWidthTable[NS_STYLE_BORDER_WIDTH_THIN] = CSSPixelsToAppUnits(1);
  mBorderWidthTable[NS_STYLE_BORDER_WIDTH_MEDIUM] = CSSPixelsToAppUnits(3);
  mBorderWidthTable[NS_STYLE_BORDER_WIDTH_THICK] = CSSPixelsToAppUnits(5);

  return NS_OK;
}

// Note: We don't hold a reference on the shell; it has a reference to
// us
void
nsPresContext::SetShell(nsIPresShell* aShell)
{
  if (mUserFontSet) {
    // Clear out user font set if we have one
    mUserFontSet->Destroy();
    NS_RELEASE(mUserFontSet);
  }

  if (mShell) {
    // Remove ourselves as the charset observer from the shell's doc, because
    // this shell may be going away for good.
    nsIDocument *doc = mShell->GetDocument();
    if (doc) {
      doc->RemoveCharSetObserver(this);
    }
  }    

  mShell = aShell;

  if (mShell) {
    nsIDocument *doc = mShell->GetDocument();
    NS_ASSERTION(doc, "expect document here");
    if (doc) {
      // Have to update PresContext's mDocument before calling any other methods.
      mDocument = doc;
    }
    // Initialize our state from the user preferences, now that we
    // have a presshell, and hence a document.
    GetUserPreferences();

    if (doc) {
      nsIURI *docURI = doc->GetDocumentURI();

      if (IsDynamic() && docURI) {
        PRBool isChrome = PR_FALSE;
        PRBool isRes = PR_FALSE;
        docURI->SchemeIs("chrome", &isChrome);
        docURI->SchemeIs("resource", &isRes);

        if (!isChrome && !isRes)
          mImageAnimationMode = mImageAnimationModePref;
        else
          mImageAnimationMode = imgIContainer::kNormalAnimMode;
      }

      if (mLangService) {
        doc->AddCharSetObserver(this);
        UpdateCharSet(doc->GetDocumentCharacterSet());
      }
    }
  } else {
    // Destroy image loaders now that the presshell is going away.
    // This is important since imageloaders can have pointers to frames and
    // we don't want those pointers to outlive the destruction of the frame
    // arena.
    for (PRUint32 i = 0; i < IMAGE_LOAD_TYPE_COUNT; ++i) {
      mImageLoaders[i].Enumerate(destroy_loads, nsnull);
      mImageLoaders[i].Clear();
    }
  }
}

void
nsPresContext::UpdateCharSet(const nsAFlatCString& aCharSet)
{
  if (mLangService) {
    NS_IF_RELEASE(mLanguage);
    mLanguage = mLangService->LookupCharSet(aCharSet.get()).get();  // addrefs
    // this will be a language group (or script) code rather than a true language code

    // bug 39570: moved from nsLanguageAtomService::LookupCharSet()
#if !defined(XP_BEOS) 
    if (mLanguage == nsGkAtoms::Unicode) {
      NS_RELEASE(mLanguage);
      NS_IF_ADDREF(mLanguage = mLangService->GetLocaleLanguage()); 
    }
#endif
    GetFontPreferences();
  }
#ifdef IBMBIDI
  //ahmed

  switch (GET_BIDI_OPTION_TEXTTYPE(GetBidi())) {

    case IBMBIDI_TEXTTYPE_LOGICAL:
      SetVisualMode(PR_FALSE);
      break;

    case IBMBIDI_TEXTTYPE_VISUAL:
      SetVisualMode(PR_TRUE);
      break;

    case IBMBIDI_TEXTTYPE_CHARSET:
    default:
      SetVisualMode(IsVisualCharset(aCharSet));
  }
#endif // IBMBIDI
}

NS_IMETHODIMP
nsPresContext::Observe(nsISupports* aSubject, 
                        const char* aTopic,
                        const PRUnichar* aData)
{
  if (!nsCRT::strcmp(aTopic, "charset")) {
    UpdateCharSet(NS_LossyConvertUTF16toASCII(aData));
    mDeviceContext->FlushFontCache();
    RebuildAllStyleData(NS_STYLE_HINT_REFLOW);
    return NS_OK;
  }

  NS_WARNING("unrecognized topic in nsPresContext::Observe");
  return NS_ERROR_FAILURE;
}

// We may want to replace this with something faster, maybe caching the root prescontext
nsRootPresContext*
nsPresContext::GetRootPresContext()
{
  nsPresContext* pc = this;
  for (;;) {
    if (pc->mShell) {
      nsIFrame* rootFrame = pc->mShell->FrameManager()->GetRootFrame();
      if (rootFrame) {
        nsIFrame* f = nsLayoutUtils::GetCrossDocParentFrame(rootFrame);
        if (f) {
          pc = f->PresContext();
          continue;
        }
      }
    }
    return pc->IsRoot() ? static_cast<nsRootPresContext*>(pc) : nsnull;
  }
}

void
nsPresContext::CompatibilityModeChanged()
{
  if (!mShell)
    return;

  // enable/disable the QuirkSheet
  mShell->StyleSet()->
    EnableQuirkStyleSheet(CompatibilityMode() == eCompatibility_NavQuirks);
}

// Helper function for setting Anim Mode on image
static void SetImgAnimModeOnImgReq(imgIRequest* aImgReq, PRUint16 aMode)
{
  if (aImgReq) {
    nsCOMPtr<imgIContainer> imgCon;
    aImgReq->GetImage(getter_AddRefs(imgCon));
    if (imgCon) {
      imgCon->SetAnimationMode(aMode);
    }
  }
}

 // Enumeration call back for HashTable
static PLDHashOperator
set_animation_mode(const void * aKey, nsRefPtr<nsImageLoader>& aData, void* closure)
{
  for (nsImageLoader *loader = aData; loader;
       loader = loader->GetNextLoader()) {
    imgIRequest* imgReq = loader->GetRequest();
    SetImgAnimModeOnImgReq(imgReq, (PRUint16)NS_PTR_TO_INT32(closure));
  }
  return PL_DHASH_NEXT;
}

// IMPORTANT: Assumption is that all images for a Presentation 
// have the same Animation Mode (pavlov said this was OK)
//
// Walks content and set the animation mode
// this is a way to turn on/off image animations
void nsPresContext::SetImgAnimations(nsIContent *aParent, PRUint16 aMode)
{
  nsCOMPtr<nsIImageLoadingContent> imgContent(do_QueryInterface(aParent));
  if (imgContent) {
    nsCOMPtr<imgIRequest> imgReq;
    imgContent->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                           getter_AddRefs(imgReq));
    SetImgAnimModeOnImgReq(imgReq, aMode);
  }
  
  PRUint32 count = aParent->GetChildCount();
  for (PRUint32 i = 0; i < count; ++i) {
    SetImgAnimations(aParent->GetChildAt(i), aMode);
  }
}

#ifdef MOZ_SMIL
void
nsPresContext::SetSMILAnimations(nsIDocument *aDoc, PRUint16 aNewMode,
                                 PRUint16 aOldMode)
{
  if (aDoc->HasAnimationController()) {
    nsSMILAnimationController* controller = aDoc->GetAnimationController();
    switch (aNewMode)
    {
      case imgIContainer::kNormalAnimMode:
      case imgIContainer::kLoopOnceAnimMode:
        if (aOldMode == imgIContainer::kDontAnimMode)
          controller->Resume(nsSMILTimeContainer::PAUSE_USERPREF);
        break;

      case imgIContainer::kDontAnimMode:
        if (aOldMode != imgIContainer::kDontAnimMode)
          controller->Pause(nsSMILTimeContainer::PAUSE_USERPREF);
        break;
    }
  }
}
#endif // MOZ_SMIL

void
nsPresContext::SetImageAnimationModeInternal(PRUint16 aMode)
{
  NS_ASSERTION(aMode == imgIContainer::kNormalAnimMode ||
               aMode == imgIContainer::kDontAnimMode ||
               aMode == imgIContainer::kLoopOnceAnimMode, "Wrong Animation Mode is being set!");

  // Image animation mode cannot be changed when rendering to a printer.
  if (!IsDynamic())
    return;

  // Set the mode on the image loaders.
  for (PRUint32 i = 0; i < IMAGE_LOAD_TYPE_COUNT; ++i)
    mImageLoaders[i].Enumerate(set_animation_mode, NS_INT32_TO_PTR(aMode));

  // Now walk the content tree and set the animation mode 
  // on all the images.
  if (mShell != nsnull) {
    nsIDocument *doc = mShell->GetDocument();
    if (doc) {
      Element *rootElement = doc->GetRootElement();
      if (rootElement) {
        SetImgAnimations(rootElement, aMode);
      }

#ifdef MOZ_SMIL
      SetSMILAnimations(doc, aMode, mImageAnimationMode);
#endif // MOZ_SMIL
    }
  }

  mImageAnimationMode = aMode;
}

void
nsPresContext::SetImageAnimationModeExternal(PRUint16 aMode)
{
  SetImageAnimationModeInternal(aMode);
}

already_AddRefed<nsIFontMetrics>
nsPresContext::GetMetricsFor(const nsFont& aFont, PRBool aUseUserFontSet)
{
  nsIFontMetrics* metrics = nsnull;
  mDeviceContext->GetMetricsFor(aFont, mLanguage,
                                aUseUserFontSet ? GetUserFontSet() : nsnull,
                                metrics);
  return metrics;
}

const nsFont*
nsPresContext::GetDefaultFont(PRUint8 aFontID) const
{
  const nsFont *font;
  switch (aFontID) {
    // Special (our default variable width font and fixed width font)
    case kPresContext_DefaultVariableFont_ID:
      font = &mDefaultVariableFont;
      break;
    case kPresContext_DefaultFixedFont_ID:
      font = &mDefaultFixedFont;
      break;
    // CSS
    case kGenericFont_serif:
      font = &mDefaultSerifFont;
      break;
    case kGenericFont_sans_serif:
      font = &mDefaultSansSerifFont;
      break;
    case kGenericFont_monospace:
      font = &mDefaultMonospaceFont;
      break;
    case kGenericFont_cursive:
      font = &mDefaultCursiveFont;
      break;
    case kGenericFont_fantasy: 
      font = &mDefaultFantasyFont;
      break;
    default:
      font = nsnull;
      NS_ERROR("invalid arg");
      break;
  }
  return font;
}

void
nsPresContext::SetFullZoom(float aZoom)
{
  if (!mShell || mFullZoom == aZoom) {
    return;
  }
  // Re-fetch the view manager's window dimensions in case there's a deferred
  // resize which hasn't affected our mVisibleArea yet
  nscoord oldWidthAppUnits, oldHeightAppUnits;
  mShell->GetViewManager()->GetWindowDimensions(&oldWidthAppUnits, &oldHeightAppUnits);
  float oldWidthDevPixels = oldWidthAppUnits / float(mCurAppUnitsPerDevPixel);
  float oldHeightDevPixels = oldHeightAppUnits / float(mCurAppUnitsPerDevPixel);
  if (mDeviceContext->SetPixelScale(aZoom)) {
    mDeviceContext->FlushFontCache();
  }

  NS_ASSERTION(!mSupressResizeReflow, "two zooms happening at the same time? impossible!");
  mSupressResizeReflow = PR_TRUE;

  mFullZoom = aZoom;
  mShell->GetViewManager()->
    SetWindowDimensions(NSToCoordRound(oldWidthDevPixels * AppUnitsPerDevPixel()),
                        NSToCoordRound(oldHeightDevPixels * AppUnitsPerDevPixel()));
  if (HasCachedStyleData()) {
    MediaFeatureValuesChanged(PR_TRUE);
    RebuildAllStyleData(NS_STYLE_HINT_REFLOW);
  }

  mSupressResizeReflow = PR_FALSE;

  mCurAppUnitsPerDevPixel = AppUnitsPerDevPixel();
}

void
nsPresContext::SetImageLoaders(nsIFrame* aTargetFrame,
                               ImageLoadType aType,
                               nsImageLoader* aImageLoaders)
{
  NS_ASSERTION(mShell || !aImageLoaders,
               "Shouldn't add new image loader after the shell is gone");

  nsRefPtr<nsImageLoader> oldLoaders;
  mImageLoaders[aType].Get(aTargetFrame, getter_AddRefs(oldLoaders));

  if (aImageLoaders) {
    mImageLoaders[aType].Put(aTargetFrame, aImageLoaders);
  } else if (oldLoaders) {
    mImageLoaders[aType].Remove(aTargetFrame);
  }

  if (oldLoaders)
    oldLoaders->Destroy();
}

void
nsPresContext::SetupBackgroundImageLoaders(nsIFrame* aFrame,
                                     const nsStyleBackground* aStyleBackground)
{
  nsRefPtr<nsImageLoader> loaders;
  NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(i, aStyleBackground) {
    if (aStyleBackground->mLayers[i].mImage.GetType() == eStyleImageType_Image) {
      PRUint32 actions = nsImageLoader::ACTION_REDRAW_ON_DECODE;
      imgIRequest *image = aStyleBackground->mLayers[i].mImage.GetImageData();
      loaders = nsImageLoader::Create(aFrame, image, actions, loaders);
    }
  }
  SetImageLoaders(aFrame, BACKGROUND_IMAGE, loaders);
}

void
nsPresContext::SetupBorderImageLoaders(nsIFrame* aFrame,
                                       const nsStyleBorder* aStyleBorder)
{
  PRUint32 actions = nsImageLoader::ACTION_REDRAW_ON_LOAD;
  if (aStyleBorder->ImageBorderDiffers())
    actions |= nsImageLoader::ACTION_REFLOW_ON_LOAD;
  nsRefPtr<nsImageLoader> loader =
    nsImageLoader::Create(aFrame, aStyleBorder->GetBorderImage(),
                          actions, nsnull);
  SetImageLoaders(aFrame, BORDER_IMAGE, loader);
}

void
nsPresContext::StopImagesFor(nsIFrame* aTargetFrame)
{
  for (PRUint32 i = 0; i < IMAGE_LOAD_TYPE_COUNT; ++i)
    SetImageLoaders(aTargetFrame, ImageLoadType(i), nsnull);
}

void
nsPresContext::SetContainer(nsISupports* aHandler)
{
  mContainer = do_GetWeakReference(aHandler);
  InvalidateIsChromeCache();
  if (mContainer) {
    GetDocumentColorPreferences();
  }
}

already_AddRefed<nsISupports>
nsPresContext::GetContainerInternal() const
{
  nsISupports *result = nsnull;
  if (mContainer)
    CallQueryReferent(mContainer.get(), &result);

  return result;
}

already_AddRefed<nsISupports>
nsPresContext::GetContainerExternal() const
{
  return GetContainerInternal();
}

#ifdef IBMBIDI
void
nsPresContext::SetBidiEnabled() const
{
  if (mShell) {
    nsIDocument *doc = mShell->GetDocument();
    if (doc) {
      doc->SetBidiEnabled();
    }
  }
}

nsBidiPresUtils*
nsPresContext::GetBidiUtils()
{
  if (!mBidiUtils)
    mBidiUtils = new nsBidiPresUtils;

  return mBidiUtils;
}

void
nsPresContext::SetBidi(PRUint32 aSource, PRBool aForceRestyle)
{
  // Don't do all this stuff unless the options have changed.
  if (aSource == GetBidi()) {
    return;
  }

  NS_ASSERTION(!(aForceRestyle && (GetBidi() == 0)), 
               "ForceReflow on new prescontext");

  Document()->SetBidiOptions(aSource);
  if (IBMBIDI_TEXTDIRECTION_RTL == GET_BIDI_OPTION_DIRECTION(aSource)
      || IBMBIDI_NUMERAL_HINDI == GET_BIDI_OPTION_NUMERAL(aSource)) {
    SetBidiEnabled();
  }
  if (IBMBIDI_TEXTTYPE_VISUAL == GET_BIDI_OPTION_TEXTTYPE(aSource)) {
    SetVisualMode(PR_TRUE);
  }
  else if (IBMBIDI_TEXTTYPE_LOGICAL == GET_BIDI_OPTION_TEXTTYPE(aSource)) {
    SetVisualMode(PR_FALSE);
  }
  else {
    nsIDocument* doc = mShell->GetDocument();
    if (doc) {
      SetVisualMode(IsVisualCharset(doc->GetDocumentCharacterSet()));
    }
  }
  if (aForceRestyle) {
    RebuildAllStyleData(NS_STYLE_HINT_REFLOW);
  }
}

PRUint32
nsPresContext::GetBidi() const
{
  return Document()->GetBidiOptions();
}

PRUint32
nsPresContext::GetBidiMemoryUsed()
{
  if (!mBidiUtils)
    return 0;

  return mBidiUtils->EstimateMemoryUsed();
}

#endif //IBMBIDI

PRBool
nsPresContext::IsTopLevelWindowInactive()
{
  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryReferent(mContainer));
  if (!treeItem)
    return PR_FALSE;

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  treeItem->GetRootTreeItem(getter_AddRefs(rootItem));
  nsCOMPtr<nsPIDOMWindow> domWindow(do_GetInterface(rootItem));

  return domWindow && !domWindow->IsActive();
}

nsITheme*
nsPresContext::GetTheme()
{
  if (!sNoTheme && !mTheme) {
    mTheme = do_GetService("@mozilla.org/chrome/chrome-native-theme;1");
    if (!mTheme)
      sNoTheme = PR_TRUE;
  }

  return mTheme;
}

void
nsPresContext::ThemeChanged()
{
  if (!mPendingThemeChanged) {
    sLookAndFeelChanged = PR_TRUE;
    sThemeChanged = PR_TRUE;

    nsCOMPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(this, &nsPresContext::ThemeChangedInternal);
    if (NS_SUCCEEDED(NS_DispatchToCurrentThread(ev))) {
      mPendingThemeChanged = PR_TRUE;
    }
  }    
}

void
nsPresContext::ThemeChangedInternal()
{
  mPendingThemeChanged = PR_FALSE;
  
  // Tell the theme that it changed, so it can flush any handles to stale theme
  // data.
  if (mTheme && sThemeChanged) {
    mTheme->ThemeChanged();
    sThemeChanged = PR_FALSE;
  }

  // Clear all cached nsILookAndFeel colors.
  if (mLookAndFeel && sLookAndFeelChanged) {
    mLookAndFeel->LookAndFeelChanged();
    sLookAndFeelChanged = PR_FALSE;
  }

  // This will force the system metrics to be generated the next time they're used
  nsCSSRuleProcessor::FreeSystemMetrics();

  // Changes to system metrics can change media queries on them.
  MediaFeatureValuesChanged(PR_TRUE);

  // Changes in theme can change system colors (whose changes are
  // properly reflected in computed style data), system fonts (whose
  // changes are not), and -moz-appearance (whose changes likewise are
  // not), so we need to reflow.
  RebuildAllStyleData(NS_STYLE_HINT_REFLOW);
}

void
nsPresContext::SysColorChanged()
{
  if (!mPendingSysColorChanged) {
    sLookAndFeelChanged = PR_TRUE;
    nsCOMPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(this, &nsPresContext::SysColorChangedInternal);
    if (NS_SUCCEEDED(NS_DispatchToCurrentThread(ev))) {
      mPendingSysColorChanged = PR_TRUE;
    }
  }
}

void
nsPresContext::SysColorChangedInternal()
{
  mPendingSysColorChanged = PR_FALSE;
  
  if (mLookAndFeel && sLookAndFeelChanged) {
     // Don't use the cached values for the system colors
    mLookAndFeel->LookAndFeelChanged();
    sLookAndFeelChanged = PR_FALSE;
  }
   
  // Reset default background and foreground colors for the document since
  // they may be using system colors
  GetDocumentColorPreferences();

  // The system color values are computed to colors in the style data,
  // so normal style data comparison is sufficient here.
  RebuildAllStyleData(nsChangeHint(0));
}

void
nsPresContext::RebuildAllStyleData(nsChangeHint aExtraHint)
{
  if (!mShell) {
    // We must have been torn down. Nothing to do here.
    return;
  }

  RebuildUserFontSet();

  mShell->FrameConstructor()->RebuildAllStyleData(aExtraHint);
}

void
nsPresContext::PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint)
{
  if (!mShell) {
    // We must have been torn down. Nothing to do here.
    return;
  }
  mShell->FrameConstructor()->PostRebuildAllStyleDataEvent(aExtraHint);
}

void
nsPresContext::MediaFeatureValuesChanged(PRBool aCallerWillRebuildStyleData)
{
  mPendingMediaFeatureValuesChanged = PR_FALSE;
  if (mShell &&
      mShell->StyleSet()->MediumFeaturesChanged(this) &&
      !aCallerWillRebuildStyleData) {
    RebuildAllStyleData(nsChangeHint(0));
  }
}

void
nsPresContext::PostMediaFeatureValuesChangedEvent()
{
  if (!mPendingMediaFeatureValuesChanged) {
    nsCOMPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(this, &nsPresContext::HandleMediaFeatureValuesChangedEvent);
    if (NS_SUCCEEDED(NS_DispatchToCurrentThread(ev))) {
      mPendingMediaFeatureValuesChanged = PR_TRUE;
    }
  }
}

void
nsPresContext::HandleMediaFeatureValuesChangedEvent()
{
  // Null-check mShell in case the shell has been destroyed (and the
  // event is the only thing holding the pres context alive).
  if (mPendingMediaFeatureValuesChanged && mShell) {
    MediaFeatureValuesChanged(PR_FALSE);
  }
}

void
nsPresContext::SetPaginatedScrolling(PRBool aPaginated)
{
  if (mType == eContext_PrintPreview || mType == eContext_PageLayout)
    mCanPaginatedScroll = aPaginated;
}

void
nsPresContext::SetPrintSettings(nsIPrintSettings *aPrintSettings)
{
  if (mMedium == nsGkAtoms::print)
    mPrintSettings = aPrintSettings;
}

PRBool
nsPresContext::EnsureVisible()
{
  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mContainer));
  if (docShell) {
    nsCOMPtr<nsIContentViewer> cv;
    docShell->GetContentViewer(getter_AddRefs(cv));
    // Make sure this is the content viewer we belong with
    nsCOMPtr<nsIDocumentViewer> docV(do_QueryInterface(cv));
    if (docV) {
      nsRefPtr<nsPresContext> currentPresContext;
      docV->GetPresContext(getter_AddRefs(currentPresContext));
      if (currentPresContext == this) {
        // OK, this is us.  We want to call Show() on the content viewer.
        cv->Show();
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

#ifdef MOZ_REFLOW_PERF
void
nsPresContext::CountReflows(const char * aName, nsIFrame * aFrame)
{
  if (mShell) {
    mShell->CountReflows(aName, aFrame);
  }
}
#endif

PRBool
nsPresContext::IsChromeSlow() const
{
  PRBool isChrome = PR_FALSE;
  nsCOMPtr<nsISupports> container = GetContainer();
  if (container) {
    nsresult result;
    nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryInterface(container, &result));
    if (NS_SUCCEEDED(result) && docShell) {
      PRInt32 docShellType;
      result = docShell->GetItemType(&docShellType);
      if (NS_SUCCEEDED(result)) {
        isChrome = nsIDocShellTreeItem::typeChrome == docShellType;
      }
    }
  }
  mIsChrome = isChrome;
  mIsChromeIsCached = PR_TRUE;
  return mIsChrome;
}

void
nsPresContext::InvalidateIsChromeCacheExternal()
{
  InvalidateIsChromeCacheInternal();
}

/* virtual */ PRBool
nsPresContext::HasAuthorSpecifiedRules(nsIFrame *aFrame, PRUint32 ruleTypeMask) const
{
  return
    nsRuleNode::HasAuthorSpecifiedRules(aFrame->GetStyleContext(),
                                        ruleTypeMask,
                                        UseDocumentColors());
}

static void
InsertFontFaceRule(nsCSSFontFaceRule *aRule, gfxUserFontSet* aFontSet,
                   PRUint8 aSheetType)
{
  NS_ABORT_IF_FALSE(aRule->GetType() == nsICSSRule::FONT_FACE_RULE,
                    "InsertFontFaceRule passed a non-fontface CSS rule");

  // aRule->List();

  nsAutoString fontfamily;
  nsCSSValue val;

  PRUint32 unit;
  PRUint32 weight = NS_STYLE_FONT_WEIGHT_NORMAL;
  PRUint32 stretch = NS_STYLE_FONT_STRETCH_NORMAL;
  PRUint32 italicStyle = FONT_STYLE_NORMAL;
  nsString featureSettings, languageOverride;

  // set up family name
  aRule->GetDesc(eCSSFontDesc_Family, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_String) {
    val.GetStringValue(fontfamily);
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face family name has unexpected unit");
    // If there is no family name, this rule cannot contribute a
    // usable font, so there is no point in processing it further.
    return;
  }

  // set up weight
  aRule->GetDesc(eCSSFontDesc_Weight, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Integer || unit == eCSSUnit_Enumerated) {
    weight = val.GetIntValue();
  } else if (unit == eCSSUnit_Normal) {
    weight = NS_STYLE_FONT_WEIGHT_NORMAL;
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face weight has unexpected unit");
  }

  // set up stretch
  aRule->GetDesc(eCSSFontDesc_Stretch, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Enumerated) {
    stretch = val.GetIntValue();
  } else if (unit == eCSSUnit_Normal) {
    stretch = NS_STYLE_FONT_STRETCH_NORMAL;
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face stretch has unexpected unit");
  }

  // set up font style
  aRule->GetDesc(eCSSFontDesc_Style, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Enumerated) {
    italicStyle = val.GetIntValue();
  } else if (unit == eCSSUnit_Normal) {
    italicStyle = FONT_STYLE_NORMAL;
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face style has unexpected unit");
  }

  // set up font features
  aRule->GetDesc(eCSSFontDesc_FontFeatureSettings, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Normal) {
    // empty feature string
  } else if (unit == eCSSUnit_String) {
    val.GetStringValue(featureSettings);
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face font-feature-settings has unexpected unit");
  }

  // set up font language override
  aRule->GetDesc(eCSSFontDesc_FontLanguageOverride, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Normal) {
    // empty feature string
  } else if (unit == eCSSUnit_String) {
    val.GetStringValue(languageOverride);
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face font-language-override has unexpected unit");
  }

  // set up src array
  nsTArray<gfxFontFaceSrc> srcArray;

  aRule->GetDesc(eCSSFontDesc_Src, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Array) {
    nsCSSValue::Array *srcArr = val.GetArrayValue();
    size_t numSrc = srcArr->Count();
    
    for (size_t i = 0; i < numSrc; i++) {
      val = srcArr->Item(i);
      unit = val.GetUnit();
      gfxFontFaceSrc *face = srcArray.AppendElements(1);
      if (!face)
        return;
            
      switch (unit) {
       
      case eCSSUnit_Local_Font:
        val.GetStringValue(face->mLocalName);
        face->mIsLocal = PR_TRUE;
        face->mURI = nsnull;
        face->mFormatFlags = 0;
        break;
      case eCSSUnit_URL:
        face->mIsLocal = PR_FALSE;
        face->mURI = val.GetURLValue();
        NS_ASSERTION(face->mURI, "null url in @font-face rule");
        face->mReferrer = val.GetURLStructValue()->mReferrer;
        face->mOriginPrincipal = val.GetURLStructValue()->mOriginPrincipal;
        NS_ASSERTION(face->mOriginPrincipal, "null origin principal in @font-face rule");
        
        // agent and user stylesheets are treated slightly differently,
        // the same-site origin check and access control headers are
        // enforced against the sheet principal rather than the document
        // principal to allow user stylesheets to include @font-face rules
        face->mUseOriginPrincipal = (aSheetType == nsStyleSet::eUserSheet ||
                                     aSheetType == nsStyleSet::eAgentSheet);
                                     
        face->mLocalName.Truncate();
        face->mFormatFlags = 0;
        while (i + 1 < numSrc && (val = srcArr->Item(i+1), 
                 val.GetUnit() == eCSSUnit_Font_Format)) {
          nsDependentString valueString(val.GetStringBufferValue());
          if (valueString.LowerCaseEqualsASCII("woff")) {
            face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_WOFF; 
          } else if (valueString.LowerCaseEqualsASCII("opentype")) {
            face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_OPENTYPE; 
          } else if (valueString.LowerCaseEqualsASCII("truetype")) {
            face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_TRUETYPE; 
          } else if (valueString.LowerCaseEqualsASCII("truetype-aat")) {
            face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_TRUETYPE_AAT; 
          } else if (valueString.LowerCaseEqualsASCII("embedded-opentype")) {
            face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_EOT;   
          } else if (valueString.LowerCaseEqualsASCII("svg")) {
            face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_SVG;   
          } else {
            // unknown format specified, mark to distinguish from the 
            // case where no format hints are specified
            face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_UNKNOWN;
          }
          i++;
        }
        break;
      default:
        NS_ASSERTION(unit == eCSSUnit_Local_Font || unit == eCSSUnit_URL,
                     "strange unit type in font-face src array");
        break;
      }
     }
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null, "@font-face src has unexpected unit");
  }
  
  if (!fontfamily.IsEmpty() && srcArray.Length() > 0) {
    aFontSet->AddFontFace(fontfamily, srcArray, weight, stretch, italicStyle,
                          featureSettings, languageOverride);
  }
}

gfxUserFontSet*
nsPresContext::GetUserFontSetInternal()
{
  // We want to initialize the user font set lazily the first time the
  // user asks for it, rather than building it too early and forcing
  // rule cascade creation.  Thus we try to enforce the invariant that
  // we *never* build the user font set until the first call to
  // GetUserFontSet.  However, once it's been requested, we can't wait
  // for somebody to call GetUserFontSet in order to rebuild it (see
  // comments below in RebuildUserFontSet for why).
#ifdef DEBUG
  PRBool userFontSetGottenBefore = mGetUserFontSetCalled;
#endif
  // Set mGetUserFontSetCalled up front, so that FlushUserFontSet will actually
  // flush.
  mGetUserFontSetCalled = PR_TRUE;
  if (mUserFontSetDirty) {
    // If this assertion fails, and there have actually been changes to
    // @font-face rules, then we will call StyleChangeReflow in
    // FlushUserFontSet.  If we're in the middle of reflow,
    // that's a bad thing to do, and the caller was responsible for
    // flushing first.  If we're not (e.g., in frame construction), it's
    // ok.
    NS_ASSERTION(!userFontSetGottenBefore || !mShell->IsReflowLocked(),
                 "FlushUserFontSet should have been called first");
    FlushUserFontSet();
  }

  return mUserFontSet;
}

gfxUserFontSet*
nsPresContext::GetUserFontSetExternal()
{
  return GetUserFontSetInternal();
}

void
nsPresContext::FlushUserFontSet()
{
  if (!mShell)
    return; // we've been torn down

  if (!mGetUserFontSetCalled) {
    return; // No one cares about this font set yet, but we want to be careful
            // to not unset our mUserFontSetDirty bit, so when someone really
            // does we'll create it.
  }

  if (mUserFontSetDirty) {
    if (gfxPlatform::GetPlatform()->DownloadableFontsEnabled()) {
      nsRefPtr<gfxUserFontSet> oldUserFontSet = mUserFontSet;

      nsTArray<nsFontFaceRuleContainer> rules;
      if (!mShell->StyleSet()->AppendFontFaceRules(this, rules))
        return;

      PRBool differ;
      if (rules.Length() == mFontFaceRules.Length()) {
        differ = PR_FALSE;
        for (PRUint32 i = 0, i_end = rules.Length(); i < i_end; ++i) {
          if (rules[i].mRule != mFontFaceRules[i].mRule ||
              rules[i].mSheetType != mFontFaceRules[i].mSheetType) {
            differ = PR_TRUE;
            break;
          }
        }
      } else {
        differ = PR_TRUE;
      }

      // Only rebuild things if the set of @font-face rules is different.
      if (differ) {
        if (mUserFontSet) {
          mUserFontSet->Destroy();
          NS_RELEASE(mUserFontSet);
        }

        if (rules.Length() > 0) {
          nsUserFontSet *fs = new nsUserFontSet(this);
          if (!fs)
            return;
          mUserFontSet = fs;
          NS_ADDREF(mUserFontSet);

          for (PRUint32 i = 0, i_end = rules.Length(); i < i_end; ++i) {
            InsertFontFaceRule(rules[i].mRule, fs, rules[i].mSheetType);
          }
        }
      }

#ifdef DEBUG
      PRBool success =
#endif
        rules.SwapElements(mFontFaceRules);
      NS_ASSERTION(success, "should never fail given both are heap arrays");

      if (mGetUserFontSetCalled && oldUserFontSet != mUserFontSet) {
        // If we've changed, created, or destroyed a user font set, we
        // need to trigger a style change reflow.
        // We need to enqueue a style change reflow (for later) to
        // reflect that we're dropping @font-face rules.  (However,
        // without a reflow, nothing will happen to start any downloads
        // that are needed.)
        UserFontSetUpdated();
      }
    }

    mUserFontSetDirty = PR_FALSE;
  }
}

void
nsPresContext::RebuildUserFontSet()
{
  if (!mGetUserFontSetCalled) {
    // We want to lazily build the user font set the first time it's
    // requested (so we don't force creation of rule cascades too
    // early), so don't do anything now.
    return;
  }

  mUserFontSetDirty = PR_TRUE;

  // Somebody has already asked for the user font set, so we need to
  // post an event to rebuild it.  Setting the user font set to be dirty
  // and lazily rebuilding it isn't sufficient, since it is only the act
  // of rebuilding it that will trigger the style change reflow that
  // calls GetUserFontSet.  (This reflow causes rebuilding of text runs,
  // which starts font loads, whose completion causes another style
  // change reflow).
  if (!mPostedFlushUserFontSet) {
    nsCOMPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(this, &nsPresContext::HandleRebuildUserFontSet);
    if (NS_SUCCEEDED(NS_DispatchToCurrentThread(ev))) {
      mPostedFlushUserFontSet = PR_TRUE;
    }
  }    
}

void
nsPresContext::UserFontSetUpdated()
{
  if (!mShell)
    return;

  // Changes to the set of available fonts can cause updates to layout by:
  //
  //   1. Changing the font used for text, which changes anything that
  //      depends on text measurement, including line breaking and
  //      intrinsic widths, and any other parts of layout that depend on
  //      font metrics.  This requires a style change reflow to update.
  //
  //   2. Changing the value of the 'ex' and 'ch' units in style data,
  //      which also depend on font metrics.  Updating this information
  //      requires rebuilding the rule tree from the top, avoiding the
  //      reuse of cached data even when no style rules have changed.

  PostRebuildAllStyleDataEvent(NS_STYLE_HINT_REFLOW);
}

PRBool
nsPresContext::EnsureSafeToHandOutCSSRules()
{
  nsCSSStyleSheet::EnsureUniqueInnerResult res =
    mShell->StyleSet()->EnsureUniqueInnerOnCSSSheets();
  if (res == nsCSSStyleSheet::eUniqueInner_AlreadyUnique) {
    // Nothing to do.
    return PR_TRUE;
  }
  if (res == nsCSSStyleSheet::eUniqueInner_CloneFailed) {
    return PR_FALSE;
  }

  NS_ABORT_IF_FALSE(res == nsCSSStyleSheet::eUniqueInner_ClonedInner,
                    "unexpected result");
  RebuildAllStyleData(nsChangeHint(0));
  return PR_TRUE;
}

void
nsPresContext::FireDOMPaintEvent()
{
  nsPIDOMWindow* ourWindow = mDocument->GetWindow();
  if (!ourWindow)
    return;

  nsCOMPtr<nsIDOMEventTarget> dispatchTarget = do_QueryInterface(ourWindow);
  nsCOMPtr<nsIDOMEventTarget> eventTarget = dispatchTarget;
  if (!IsChrome()) {
    PRBool isCrossDocOnly = PR_TRUE;
    for (PRUint32 i = 0; i < mInvalidateRequests.mRequests.Length(); ++i) {
      if (!(mInvalidateRequests.mRequests[i].mFlags & nsIFrame::INVALIDATE_CROSS_DOC)) {
        isCrossDocOnly = PR_FALSE;
      }
    }
    if (isCrossDocOnly) {
      // Don't tell the window about this event, it should not know that
      // something happened in a subdocument. Tell only the chrome event handler.
      // (Events sent to the window get propagated to the chrome event handler
      // automatically.)
      dispatchTarget = do_QueryInterface(ourWindow->GetChromeEventHandler());
      if (!dispatchTarget) {
        return;
      }
    }
  }
  // Events sent to the window get propagated to the chrome event handler
  // automatically.
  nsCOMPtr<nsIDOMEvent> event;
  // This will empty our list in case dispatching the event causes more damage
  // (hopefully it won't, or we're likely to get an infinite loop! At least
  // it won't be blocking app execution though).
  NS_NewDOMNotifyPaintEvent(getter_AddRefs(event), this, nsnull,
                            NS_AFTERPAINT,
                            &mInvalidateRequests);
  nsCOMPtr<nsIPrivateDOMEvent> pEvent = do_QueryInterface(event);
  if (!pEvent) return;

  // Even if we're not telling the window about the event (so eventTarget is
  // the chrome event handler, not the window), the window is still
  // logically the event target.
  pEvent->SetTarget(eventTarget);
  pEvent->SetTrusted(PR_TRUE);
  nsEventDispatcher::DispatchDOMEvent(dispatchTarget, nsnull, event, this, nsnull);
}

static PRBool
MayHavePaintEventListener(nsPIDOMWindow* aInnerWindow)
{
  if (!aInnerWindow)
    return PR_FALSE;
  if (aInnerWindow->HasPaintEventListeners())
    return PR_TRUE;

  nsPIDOMEventTarget* parentTarget = aInnerWindow->GetParentTarget();
  if (!parentTarget)
    return PR_FALSE;

  nsIEventListenerManager* manager = nsnull;
  if ((manager = parentTarget->GetListenerManager(PR_FALSE)) &&
      manager->MayHavePaintEventListener()) {
    return PR_TRUE;
  }

  nsCOMPtr<nsINode> node;
  if (parentTarget != aInnerWindow->GetChromeEventHandler()) {
    nsCOMPtr<nsIInProcessContentFrameMessageManager> mm =
      do_QueryInterface(parentTarget);
    if (mm) {
      node = mm->GetOwnerContent();
    }
  }

  if (!node) {
    node = do_QueryInterface(parentTarget);
  }
  if (node)
    return MayHavePaintEventListener(node->GetOwnerDoc()->GetInnerWindow());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(parentTarget);
  if (window)
    return MayHavePaintEventListener(window);

  nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(parentTarget);
  nsPIDOMEventTarget* tabChildGlobal;
  return root &&
         (tabChildGlobal = root->GetParentTarget()) &&
         (manager = tabChildGlobal->GetListenerManager(PR_FALSE)) &&
         manager->MayHavePaintEventListener();
}

PRBool
nsPresContext::MayHavePaintEventListener()
{
  return ::MayHavePaintEventListener(mDocument->GetInnerWindow());
}

void
nsPresContext::NotifyInvalidation(const nsRect& aRect, PRUint32 aFlags)
{
  // If there is no paint event listener, then we don't need to fire
  // the asynchronous event. We don't even need to record invalidation.
  // MayHavePaintEventListener is pretty cheap and we could make it
  // even cheaper by providing a more efficient
  // nsPIDOMWindow::GetListenerManager.
  if (aRect.IsEmpty() || !MayHavePaintEventListener())
    return;

  if (!IsDOMPaintEventPending()) {
    // No event is pending. Dispatch one now.
    nsCOMPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(this, &nsPresContext::FireDOMPaintEvent);
    NS_DispatchToCurrentThread(ev);
  }

  nsInvalidateRequestList::Request* request =
    mInvalidateRequests.mRequests.AppendElement();
  if (!request)
    return;

  request->mRect = aRect;
  request->mFlags = aFlags;
}

PRBool
nsPresContext::HasCachedStyleData()
{
  return mShell && mShell->StyleSet()->HasCachedStyleData();
}

static PRBool sGotInterruptEnv = PR_FALSE;
enum InterruptMode {
  ModeRandom,
  ModeCounter,
  ModeEvent
};
// Controlled by the GECKO_REFLOW_INTERRUPT_MODE env var; allowed values are
// "random" (except on Windows) or "counter".  If neither is used, the mode is
// ModeEvent.
static InterruptMode sInterruptMode = ModeEvent;
// Used for the "random" mode.  Controlled by the GECKO_REFLOW_INTERRUPT_SEED
// env var.
static PRUint32 sInterruptSeed = 1;
// Used for the "counter" mode.  This is the number of unskipped interrupt
// checks that have to happen before we interrupt.  Controlled by the
// GECKO_REFLOW_INTERRUPT_FREQUENCY env var.
static PRUint32 sInterruptMaxCounter = 10;
// Used for the "counter" mode.  This counts up to sInterruptMaxCounter and is
// then reset to 0.
static PRUint32 sInterruptCounter;
// Number of interrupt checks to skip before really trying to interrupt.
// Controlled by the GECKO_REFLOW_INTERRUPT_CHECKS_TO_SKIP env var.
static PRUint32 sInterruptChecksToSkip = 200;
// Number of milliseconds that a reflow should be allowed to run for before we
// actually allow interruption.  Controlled by the
// GECKO_REFLOW_MIN_NOINTERRUPT_DURATION env var.
static TimeDuration sInterruptTimeout = TimeDuration::FromMilliseconds(100);

static void GetInterruptEnv()
{
  char *ev = PR_GetEnv("GECKO_REFLOW_INTERRUPT_MODE");
  if (ev) {
#ifndef XP_WIN
    if (PL_strcasecmp(ev, "random") == 0) {
      ev = PR_GetEnv("GECKO_REFLOW_INTERRUPT_SEED");
      if (ev) {
        sInterruptSeed = atoi(ev);
      }
      srandom(sInterruptSeed);
      sInterruptMode = ModeRandom;
    } else
#endif
      if (PL_strcasecmp(ev, "counter") == 0) {
      ev = PR_GetEnv("GECKO_REFLOW_INTERRUPT_FREQUENCY");
      if (ev) {
        sInterruptMaxCounter = atoi(ev);
      }
      sInterruptCounter = 0;
      sInterruptMode = ModeCounter;
    }
  }
  ev = PR_GetEnv("GECKO_REFLOW_INTERRUPT_CHECKS_TO_SKIP");
  if (ev) {
    sInterruptChecksToSkip = atoi(ev);
  }

  ev = PR_GetEnv("GECKO_REFLOW_MIN_NOINTERRUPT_DURATION");
  if (ev) {
    sInterruptTimeout = TimeDuration::FromMilliseconds(atoi(ev));
  }
}

PRBool
nsPresContext::HavePendingInputEvent()
{
  switch (sInterruptMode) {
#ifndef XP_WIN
    case ModeRandom:
      return (random() & 1);
#endif
    case ModeCounter:
      if (sInterruptCounter < sInterruptMaxCounter) {
        ++sInterruptCounter;
        return PR_FALSE;
      }
      sInterruptCounter = 0;
      return PR_TRUE;
    default:
    case ModeEvent: {
      nsIFrame* f = PresShell()->GetRootFrame();
      if (f) {
        nsIWidget* w = f->GetNearestWidget();
        if (w) {
          return w->HasPendingInputEvent();
        }
      }
      return PR_FALSE;
    }
  }
}

void
nsPresContext::ReflowStarted(PRBool aInterruptible)
{
#ifdef NOISY_INTERRUPTIBLE_REFLOW
  if (!aInterruptible) {
    printf("STARTING NONINTERRUPTIBLE REFLOW\n");
  }
#endif
  // We don't support interrupting in paginated contexts, since page
  // sequences only handle initial reflow
  mInterruptsEnabled = aInterruptible && !IsPaginated();

  // Don't set mHasPendingInterrupt based on HavePendingInputEvent() here.  If
  // we ever change that, then we need to update the code in
  // PresShell::DoReflow to only add the just-reflown root to dirty roots if
  // it's actually dirty.  Otherwise we can end up adding a root that has no
  // interruptible descendants, just because we detected an interrupt at reflow
  // start.
  mHasPendingInterrupt = PR_FALSE;

  mInterruptChecksToSkip = sInterruptChecksToSkip;

  if (mInterruptsEnabled) {
    mReflowStartTime = TimeStamp::Now();
  }
}

PRBool
nsPresContext::CheckForInterrupt(nsIFrame* aFrame)
{
  if (mHasPendingInterrupt) {
    mShell->FrameNeedsToContinueReflow(aFrame);
    return PR_TRUE;
  }

  if (!sGotInterruptEnv) {
    sGotInterruptEnv = PR_TRUE;
    GetInterruptEnv();
  }

  if (!mInterruptsEnabled) {
    return PR_FALSE;
  }

  if (mInterruptChecksToSkip > 0) {
    --mInterruptChecksToSkip;
    return PR_FALSE;
  }
  mInterruptChecksToSkip = sInterruptChecksToSkip;

  // Don't interrupt if it's been less than sInterruptTimeout since we started
  // the reflow.
  mHasPendingInterrupt =
    TimeStamp::Now() - mReflowStartTime > sInterruptTimeout &&
    HavePendingInputEvent() &&
    !IsChrome();
  if (mHasPendingInterrupt) {
#ifdef NOISY_INTERRUPTIBLE_REFLOW
    printf("*** DETECTED pending interrupt (time=%lld)\n", PR_Now());
#endif /* NOISY_INTERRUPTIBLE_REFLOW */
    mShell->FrameNeedsToContinueReflow(aFrame);
  }
  return mHasPendingInterrupt;
}

PRBool
nsPresContext::IsRootContentDocument()
{
  // We are a root content document if: we are not a resource doc, we are
  // not chrome, and we either have no parent or our parent is chrome.
  if (mDocument->IsResourceDoc()) {
    return PR_FALSE;
  }
  if (IsChrome()) {
    return PR_FALSE;
  }
  // We may not have a root frame, so use views.
  nsIViewManager* vm = PresShell()->GetViewManager();
  nsIView* view = nsnull;
  if (NS_FAILED(vm->GetRootView(view)) || !view) {
    return PR_FALSE;
  }
  view = view->GetParent(); // anonymous inner view
  if (!view) {
    return PR_TRUE;
  }
  view = view->GetParent(); // subdocumentframe's view
  if (!view) {
    return PR_TRUE;
  }

  nsIFrame* f = static_cast<nsIFrame*>(view->GetClientData());
  return (f && f->PresContext()->IsChrome());
}

nsRootPresContext::nsRootPresContext(nsIDocument* aDocument,
                                     nsPresContextType aType)
  : nsPresContext(aDocument, aType),
    mUpdatePluginGeometryForFrame(nsnull),
    mDOMGeneration(0),
    mNeedsToUpdatePluginGeometry(PR_FALSE)
{
  mRegisteredPlugins.Init();
}

nsRootPresContext::~nsRootPresContext()
{
  NS_ASSERTION(mRegisteredPlugins.Count() == 0,
               "All plugins should have been unregistered");
}

void
nsRootPresContext::RegisterPluginForGeometryUpdates(nsObjectFrame* aPlugin)
{
  mRegisteredPlugins.PutEntry(aPlugin);
}

void
nsRootPresContext::UnregisterPluginForGeometryUpdates(nsObjectFrame* aPlugin)
{
  mRegisteredPlugins.RemoveEntry(aPlugin);
}

struct PluginGeometryClosure {
  nsIFrame* mRootFrame;
  PRInt32   mRootAPD;
  nsIFrame* mChangedSubtree;
  nsRect    mChangedRect;
  nsTHashtable<nsPtrHashKey<nsObjectFrame> > mAffectedPlugins;
  nsRect    mAffectedPluginBounds;
  nsTArray<nsIWidget::Configuration>* mOutputConfigurations;
};
static PLDHashOperator
PluginBoundsEnumerator(nsPtrHashKey<nsObjectFrame>* aEntry, void* userArg)
{
  PluginGeometryClosure* closure = static_cast<PluginGeometryClosure*>(userArg);
  nsObjectFrame* f = aEntry->GetKey();
  nsRect fBounds = f->GetContentRect() +
      f->GetParent()->GetOffsetToCrossDoc(closure->mRootFrame);
  PRInt32 APD = f->PresContext()->AppUnitsPerDevPixel();
  fBounds = fBounds.ConvertAppUnitsRoundOut(APD, closure->mRootAPD);
  // We're identifying the plugins that may have been affected by changes
  // to the frame subtree rooted at aChangedRoot. Any plugin that overlaps
  // the overflow area of aChangedRoot could have its clip region affected
  // because it might be covered (or uncovered) by changes to the subtree.
  // Plugins in the subtree might have changed position and/or size, and
  // they might not be in aChangedRoot's overflow area (because they're
  // being clipped by an ancestor in the subtree).
  if (fBounds.Intersects(closure->mChangedRect) ||
      nsLayoutUtils::IsAncestorFrameCrossDoc(closure->mChangedSubtree, f)) {
    closure->mAffectedPluginBounds.UnionRect(
        closure->mAffectedPluginBounds, fBounds);
    closure->mAffectedPlugins.PutEntry(f);
  }
  return PL_DHASH_NEXT;
}

static PLDHashOperator
PluginHideEnumerator(nsPtrHashKey<nsObjectFrame>* aEntry, void* userArg)
{
  PluginGeometryClosure* closure = static_cast<PluginGeometryClosure*>(userArg);
  nsObjectFrame* f = aEntry->GetKey();
  f->GetEmptyClipConfiguration(closure->mOutputConfigurations);
  return PL_DHASH_NEXT;
}

static void
RecoverPluginGeometry(nsDisplayListBuilder* aBuilder,
    nsDisplayList* aList, PluginGeometryClosure* aClosure)
{
  for (nsDisplayItem* i = aList->GetBottom(); i; i = i->GetAbove()) {
    switch (i->GetType()) {
    case nsDisplayItem::TYPE_PLUGIN: {
      nsDisplayPlugin* displayPlugin = static_cast<nsDisplayPlugin*>(i);
      nsObjectFrame* f = static_cast<nsObjectFrame*>(
          displayPlugin->GetUnderlyingFrame());
      // Ignore plugins which aren't supposed to be affected by this
      // operation --- their bounds will not have been included in the
      // display list computations so the visible region computed for them
      // would be incorrect
      nsPtrHashKey<nsObjectFrame>* entry =
        aClosure->mAffectedPlugins.GetEntry(f);
      if (entry) {
        displayPlugin->GetWidgetConfiguration(aBuilder,
                                              aClosure->mOutputConfigurations);
        // we've dealt with this plugin now
        aClosure->mAffectedPlugins.RawRemoveEntry(entry);
      }
      break;
    }
    default: {
      nsDisplayList* sublist = i->GetList();
      if (sublist) {
        RecoverPluginGeometry(aBuilder, sublist, aClosure);
      }
      break;
    }
    }
  }
}

#ifdef DEBUG
#include <stdio.h>

static PRBool gDumpPluginList = PR_FALSE;
#endif

void
nsRootPresContext::GetPluginGeometryUpdates(nsIFrame* aChangedSubtree,
                                            nsTArray<nsIWidget::Configuration>* aConfigurations)
{
  if (mRegisteredPlugins.Count() == 0)
    return;

  PluginGeometryClosure closure;
  closure.mRootFrame = mShell->FrameManager()->GetRootFrame();
  closure.mRootAPD = closure.mRootFrame->PresContext()->AppUnitsPerDevPixel();
  closure.mChangedSubtree = aChangedSubtree;
  closure.mChangedRect = aChangedSubtree->GetVisualOverflowRect() +
      aChangedSubtree->GetOffsetToCrossDoc(closure.mRootFrame);
  PRInt32 subtreeAPD = aChangedSubtree->PresContext()->AppUnitsPerDevPixel();
  closure.mChangedRect =
    closure.mChangedRect.ConvertAppUnitsRoundOut(subtreeAPD, closure.mRootAPD);
  closure.mAffectedPlugins.Init();
  closure.mOutputConfigurations = aConfigurations;
  // Fill in closure.mAffectedPlugins and closure.mAffectedPluginBounds
  mRegisteredPlugins.EnumerateEntries(PluginBoundsEnumerator, &closure);

  nsRect bounds;
  if (bounds.IntersectRect(closure.mAffectedPluginBounds,
                           closure.mRootFrame->GetRect())) {
    nsDisplayListBuilder builder(closure.mRootFrame,
    		nsDisplayListBuilder::PLUGIN_GEOMETRY, PR_FALSE);
    builder.SetAccurateVisibleRegions();
    nsDisplayList list;

    builder.EnterPresShell(closure.mRootFrame, bounds);
    closure.mRootFrame->BuildDisplayListForStackingContext(
        &builder, bounds, &list);
    builder.LeavePresShell(closure.mRootFrame, bounds);

#ifdef DEBUG
    if (gDumpPluginList) {
      fprintf(stderr, "Plugins --- before optimization (bounds %d,%d,%d,%d):\n",
          bounds.x, bounds.y, bounds.width, bounds.height);
      nsFrame::PrintDisplayList(&builder, list);
    }
#endif

    nsRegion visibleRegion(bounds);
    list.ComputeVisibilityForRoot(&builder, &visibleRegion);

#ifdef DEBUG
    if (gDumpPluginList) {
      fprintf(stderr, "Plugins --- after optimization:\n");
      nsFrame::PrintDisplayList(&builder, list);
    }
#endif

    RecoverPluginGeometry(&builder, &list, &closure);
    list.DeleteAll();
  }

  // Plugins that we didn't find in the display list are not visible
  closure.mAffectedPlugins.EnumerateEntries(PluginHideEnumerator, &closure);
}

static PRBool
HasOverlap(const nsIntPoint& aOffset1, const nsTArray<nsIntRect>& aClipRects1,
           const nsIntPoint& aOffset2, const nsTArray<nsIntRect>& aClipRects2)
{
  nsIntPoint offsetDelta = aOffset1 - aOffset2;
  for (PRUint32 i = 0; i < aClipRects1.Length(); ++i) {
    for (PRUint32 j = 0; j < aClipRects2.Length(); ++j) {
      if ((aClipRects1[i] + offsetDelta).Intersects(aClipRects2[j]))
        return PR_TRUE;
    }
  }
  return PR_FALSE;
}

/**
 * Given a list of plugin windows to move to new locations, sort the list
 * so that for each window move, the window moves to a location that
 * does not intersect other windows. This minimizes flicker and repainting.
 * It's not always possible to do this perfectly, since in general
 * we might have cycles. But we do our best.
 * We need to take into account that windows are clipped to particular
 * regions and the clip regions change as the windows are moved.
 */
static void
SortConfigurations(nsTArray<nsIWidget::Configuration>* aConfigurations)
{
  if (aConfigurations->Length() > 10) {
    // Give up, we don't want to get bogged down here
    return;
  }

  nsTArray<nsIWidget::Configuration> pluginsToMove;
  pluginsToMove.SwapElements(*aConfigurations);

  // Our algorithm is quite naive. At each step we try to identify
  // a window that can be moved to its new location that won't overlap
  // any other windows at the new location. If there is no such
  // window, we just move the last window in the list anyway.
  while (!pluginsToMove.IsEmpty()) {
    // Find a window whose destination does not overlap any other window
    PRUint32 i;
    for (i = 0; i + 1 < pluginsToMove.Length(); ++i) {
      nsIWidget::Configuration* config = &pluginsToMove[i];
      PRBool foundOverlap = PR_FALSE;
      for (PRUint32 j = 0; j < pluginsToMove.Length(); ++j) {
        if (i == j)
          continue;
        nsIntRect bounds;
        pluginsToMove[j].mChild->GetBounds(bounds);
        nsAutoTArray<nsIntRect,1> clipRects;
        pluginsToMove[j].mChild->GetWindowClipRegion(&clipRects);
        if (HasOverlap(bounds.TopLeft(), clipRects,
                       config->mBounds.TopLeft(),
                       config->mClipRegion)) {
          foundOverlap = PR_TRUE;
          break;
        }
      }
      if (!foundOverlap)
        break;
    }
    // Note that we always move the last plugin in pluginsToMove, if we
    // can't find any other plugin to move
    aConfigurations->AppendElement(pluginsToMove[i]);
    pluginsToMove.RemoveElementAt(i);
  }
}

void
nsRootPresContext::UpdatePluginGeometry()
{
  if (!mNeedsToUpdatePluginGeometry)
    return;
  mNeedsToUpdatePluginGeometry = PR_FALSE;

  nsIFrame* f = mUpdatePluginGeometryForFrame;
  if (f) {
    mUpdatePluginGeometryForFrame->PresContext()->
      SetContainsUpdatePluginGeometryFrame(PR_FALSE);
    mUpdatePluginGeometryForFrame = nsnull;
  } else {
    f = FrameManager()->GetRootFrame();
  }

  nsTArray<nsIWidget::Configuration> configurations;
  GetPluginGeometryUpdates(f, &configurations);
  if (configurations.IsEmpty())
    return;
  SortConfigurations(&configurations);
  nsIWidget* widget = FrameManager()->GetRootFrame()->GetNearestWidget();
  NS_ASSERTION(widget, "Plugins must have a parent window");
  widget->ConfigureChildren(configurations);
  DidApplyPluginGeometryUpdates();
}

void
nsRootPresContext::SynchronousPluginGeometryUpdate()
{
  if (!mNeedsToUpdatePluginGeometry) {
    // Nothing to do
    return;
  }

  // Force synchronous paint
  nsIPresShell* shell = GetPresShell();
  if (!shell)
    return;
  nsIFrame* rootFrame = shell->GetRootFrame();
  if (!rootFrame)
    return;
  nsCOMPtr<nsIWidget> widget = rootFrame->GetNearestWidget();
  if (!widget)
    return;
  // Force synchronous paint of a single pixel, just to force plugin
  // updates to be flushed. Doing plugin updates during paint is the best
  // way to ensure that plugin updates are in sync with our content.
  widget->Invalidate(nsIntRect(0,0,1,1), PR_TRUE);

  // Update plugin geometry just in case that invalidate didn't work
  // (e.g. if none of the widget is visible, it might not have processed
  // a paint event). Normally this won't need to do anything.
  UpdatePluginGeometry();
}

void
nsRootPresContext::RequestUpdatePluginGeometry(nsIFrame* aFrame)
{
  if (mRegisteredPlugins.Count() == 0)
    return;

  if (!mNeedsToUpdatePluginGeometry) {
    mNeedsToUpdatePluginGeometry = PR_TRUE;

    // Dispatch a Gecko event to ensure plugin geometry gets updated
    // XXX this really should be done through the refresh driver, once
    // all painting happens in the refresh driver
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &nsRootPresContext::SynchronousPluginGeometryUpdate);
    NS_DispatchToMainThread(event);

    mUpdatePluginGeometryForFrame = aFrame;
    mUpdatePluginGeometryForFrame->PresContext()->
      SetContainsUpdatePluginGeometryFrame(PR_TRUE);
  } else {
    if (!mUpdatePluginGeometryForFrame ||
        aFrame == mUpdatePluginGeometryForFrame)
      return;
    mUpdatePluginGeometryForFrame->PresContext()->
      SetContainsUpdatePluginGeometryFrame(PR_FALSE);
    mUpdatePluginGeometryForFrame = nsnull;
  }
}

static PLDHashOperator
PluginDidSetGeometryEnumerator(nsPtrHashKey<nsObjectFrame>* aEntry, void* userArg)
{
  nsObjectFrame* f = aEntry->GetKey();
  f->DidSetWidgetGeometry();
  return PL_DHASH_NEXT;
}

void
nsRootPresContext::DidApplyPluginGeometryUpdates()
{
  mRegisteredPlugins.EnumerateEntries(PluginDidSetGeometryEnumerator, nsnull);
}

void
nsRootPresContext::RootForgetUpdatePluginGeometryFrame(nsIFrame* aFrame)
{
  if (aFrame == mUpdatePluginGeometryForFrame) {
    mUpdatePluginGeometryForFrame->PresContext()->
      SetContainsUpdatePluginGeometryFrame(PR_FALSE);
    mUpdatePluginGeometryForFrame = nsnull;
  }
}
