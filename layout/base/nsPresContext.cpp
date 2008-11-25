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
#include "nsIFocusController.h"
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

#ifdef IBMBIDI
#include "nsBidiPresUtils.h"
#endif // IBMBIDI

#include "nsContentUtils.h"

// Needed for Start/Stop of Image Animation
#include "imgIContainer.h"
#include "nsIImageLoadingContent.h"

//needed for resetting of image service color
#include "nsLayoutCID.h"

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
destroy_loads(const void * aKey, nsCOMPtr<nsImageLoader>& aData, void* closure)
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
                         NS_FONT_WEIGHT_NORMAL, 0, 0),
    mDefaultFixedFont("monospace", NS_FONT_STYLE_NORMAL,
                      NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL, 0, 0),
    mDefaultSerifFont("serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
                      NS_FONT_WEIGHT_NORMAL, 0, 0),
    mDefaultSansSerifFont("sans-serif", NS_FONT_STYLE_NORMAL,
                          NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL, 0, 0),
    mDefaultMonospaceFont("monospace", NS_FONT_STYLE_NORMAL,
                          NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL, 0, 0),
    mDefaultCursiveFont("cursive", NS_FONT_STYLE_NORMAL,
                        NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL, 0, 0),
    mDefaultFantasyFont("fantasy", NS_FONT_STYLE_NORMAL,
                        NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL, 0, 0),
    mCanPaginatedScroll(PR_FALSE),
    mIsRootPaginatedDocument(PR_FALSE), mSupressResizeReflow(PR_FALSE)
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
    SetBackgroundImageDraw(PR_FALSE);
    SetBackgroundColorDraw(PR_FALSE);
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
  mImageLoaders.Enumerate(destroy_loads, nsnull);
  mBorderImageLoaders.Enumerate(destroy_loads, nsnull);

  NS_PRECONDITION(!mShell, "Presshell forgot to clear our mShell pointer");
  SetShell(nsnull);

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
  nsContentUtils::UnregisterPrefCallback("layout.css.dpi",
                                         nsPresContext::PrefChangedCallback,
                                         this);

  NS_IF_RELEASE(mDeviceContext);
  NS_IF_RELEASE(mLookAndFeel);
  NS_IF_RELEASE(mLangGroup);
  NS_IF_RELEASE(mUserFontSet);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsPresContext)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPresContext)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
   NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPresContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPresContext)

static PLDHashOperator
TraverseImageLoader(const void * aKey, nsCOMPtr<nsImageLoader>& aData,
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
  // NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mLangGroup); // an atom

  tmp->mImageLoaders.Enumerate(TraverseImageLoader, &cb);
  tmp->mBorderImageLoaders.Enumerate(TraverseImageLoader, &cb);

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
  // NS_RELEASE(tmp->mLangGroup); // an atom

  tmp->mImageLoaders.Enumerate(destroy_loads, nsnull);
  tmp->mImageLoaders.Clear();
  tmp->mBorderImageLoaders.Enumerate(destroy_loads, nsnull);
  tmp->mBorderImageLoaders.Clear();

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

  const char *langGroup = "x-western"; // Assume x-western is safe...
  if (mLangGroup) {
    mLangGroup->GetUTF8String(&langGroup);
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
    mMinimumFontSize = this->PointsToAppUnits(size);
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
        font->size = nsPresContext::CSSPixelsToAppUnits(size);
      }
      else if (unit == eUnit_pt) {
        font->size = this->PointsToAppUnits(size);
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
    nsContentUtils::GetIntPref(IBMBIDI_CONTROLSTEXTMODE_STR,
                               GET_BIDI_OPTION_CONTROLSTEXTMODE(bidiOptions));
  SET_BIDI_OPTION_CONTROLSTEXTMODE(bidiOptions, prefInt);

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
  if (prefName.EqualsLiteral("layout.css.dpi")) {
    PRInt32 oldAppUnitsPerDevPixel = AppUnitsPerDevPixel();
    if (mDeviceContext->CheckDPIChange() && mShell) {
      mDeviceContext->FlushFontCache();

      // Re-fetch the view manager's window dimensions in case there's a deferred
      // resize which hasn't affected our mVisibleArea yet
      nscoord oldWidthAppUnits, oldHeightAppUnits;
      nsIViewManager* vm = GetViewManager();
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
  NS_ASSERTION(!(mInitialized == PR_TRUE), "attempt to reinit pres context");
  NS_ENSURE_ARG(aDeviceContext);

  mDeviceContext = aDeviceContext;
  NS_ADDREF(mDeviceContext);

  if (mDeviceContext->SetPixelScale(mFullZoom))
    mDeviceContext->FlushFontCache();
  mCurAppUnitsPerDevPixel = AppUnitsPerDevPixel();

  if (!mImageLoaders.Init())
    return NS_ERROR_OUT_OF_MEMORY;
  
  if (!mBorderImageLoaders.Init())
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
  nsContentUtils::RegisterPrefCallback("layout.css.dpi",
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
  }
}

void
nsPresContext::UpdateCharSet(const nsAFlatCString& aCharSet)
{
  if (mLangService) {
    NS_IF_RELEASE(mLangGroup);
    mLangGroup = mLangService->LookupCharSet(aCharSet.get()).get();  // addrefs

    // bug 39570: moved from nsLanguageAtomService::LookupCharSet()
#if !defined(XP_BEOS) 
    if (mLangGroup == nsGkAtoms::Unicode) {
      NS_RELEASE(mLangGroup);
      NS_IF_ADDREF(mLangGroup = mLangService->GetLocaleLanguageGroup()); 
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
nsPresContext*
nsPresContext::RootPresContext()
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
    return pc;
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
set_animation_mode(const void * aKey, nsCOMPtr<nsImageLoader>& aData, void* closure)
{
  imgIRequest* imgReq = aData->GetRequest();
  SetImgAnimModeOnImgReq(imgReq, (PRUint16)NS_PTR_TO_INT32(closure));
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
  mImageLoaders.Enumerate(set_animation_mode, NS_INT32_TO_PTR(aMode));
  mBorderImageLoaders.Enumerate(set_animation_mode, NS_INT32_TO_PTR(aMode));

  // Now walk the content tree and set the animation mode 
  // on all the images.
  if (mShell != nsnull) {
    nsIDocument *doc = mShell->GetDocument();
    if (doc) {
      nsIContent *rootContent = doc->GetRootContent();
      if (rootContent) {
        SetImgAnimations(rootContent, aMode);
      }
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
nsPresContext::GetMetricsFor(const nsFont& aFont)
{
  nsIFontMetrics* metrics = nsnull;
  mDeviceContext->GetMetricsFor(aFont, mLangGroup, metrics);
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
  GetViewManager()->GetWindowDimensions(&oldWidthAppUnits, &oldHeightAppUnits);
  float oldWidthDevPixels = oldWidthAppUnits / float(mCurAppUnitsPerDevPixel);
  float oldHeightDevPixels = oldHeightAppUnits / float(mCurAppUnitsPerDevPixel);
  if (mDeviceContext->SetPixelScale(aZoom)) {
    mDeviceContext->FlushFontCache();
  }

  NS_ASSERTION(mSupressResizeReflow == PR_FALSE, "two zooms happening at the same time? impossible!");
  mSupressResizeReflow = PR_TRUE;

  mFullZoom = aZoom;
  GetViewManager()->SetWindowDimensions(NSToCoordRound(oldWidthDevPixels * AppUnitsPerDevPixel()),
                                        NSToCoordRound(oldHeightDevPixels * AppUnitsPerDevPixel()));
  MediaFeatureValuesChanged(PR_TRUE);
  RebuildAllStyleData(NS_STYLE_HINT_REFLOW);

  mSupressResizeReflow = PR_FALSE;

  mCurAppUnitsPerDevPixel = AppUnitsPerDevPixel();
}

imgIRequest*
nsPresContext::DoLoadImage(nsPresContext::ImageLoaderTable& aTable,
                           imgIRequest* aImage,
                           nsIFrame* aTargetFrame,
                           PRBool aReflowOnLoad)
{
  // look and see if we have a loader for the target frame.
  nsCOMPtr<nsImageLoader> loader;
  aTable.Get(aTargetFrame, getter_AddRefs(loader));

  if (!loader) {
    loader = new nsImageLoader();
    if (!loader)
      return nsnull;

    loader->Init(aTargetFrame, this, aReflowOnLoad);
    aTable.Put(aTargetFrame, loader);
  }

  loader->Load(aImage);

  imgIRequest *request = loader->GetRequest();

  return request;
}

imgIRequest*
nsPresContext::LoadImage(imgIRequest* aImage, nsIFrame* aTargetFrame)
{
  return DoLoadImage(mImageLoaders, aImage, aTargetFrame, PR_FALSE);
}

imgIRequest*
nsPresContext::LoadBorderImage(imgIRequest* aImage, nsIFrame* aTargetFrame)
{
  return DoLoadImage(mBorderImageLoaders, aImage, aTargetFrame,
                     aTargetFrame->GetStyleBorder()->ImageBorderDiffers());
}

void
nsPresContext::StopImagesFor(nsIFrame* aTargetFrame)
{
  StopBackgroundImageFor(aTargetFrame);
  StopBorderImageFor(aTargetFrame);
}

void
nsPresContext::DoStopImageFor(nsPresContext::ImageLoaderTable& aTable,
                              nsIFrame* aTargetFrame)
{
  nsCOMPtr<nsImageLoader> loader;
  aTable.Get(aTargetFrame, getter_AddRefs(loader));

  if (loader) {
    loader->Destroy();

    aTable.Remove(aTargetFrame);
  }
}
  
void
nsPresContext::SetContainer(nsISupports* aHandler)
{
  mContainer = do_GetWeakReference(aHandler);
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
PRBool
nsPresContext::BidiEnabledInternal() const
{
  PRBool bidiEnabled = PR_FALSE;
  NS_ASSERTION(mShell, "PresShell must be set on PresContext before calling nsPresContext::GetBidiEnabled");
  if (mShell) {
    nsIDocument *doc = mShell->GetDocument();
    NS_ASSERTION(doc, "PresShell has no document in nsPresContext::GetBidiEnabled");
    if (doc) {
      bidiEnabled = doc->GetBidiEnabled();
    }
  }
  return bidiEnabled;
}

PRBool
nsPresContext::BidiEnabledExternal() const
{
  return BidiEnabledInternal();
}

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
#endif //IBMBIDI

nsITheme*
nsPresContext::GetTheme()
{
  if (!mNoTheme && !mTheme) {
    mTheme = do_GetService("@mozilla.org/chrome/chrome-native-theme;1");
    if (!mTheme)
      mNoTheme = PR_TRUE;
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
      new nsRunnableMethod<nsPresContext>(this,
                                          &nsPresContext::ThemeChangedInternal);
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
      new nsRunnableMethod<nsPresContext>(this,
                                          &nsPresContext::SysColorChangedInternal);
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
nsPresContext::PostRebuildAllStyleDataEvent()
{
  if (!mShell) {
    // We must have been torn down. Nothing to do here.
    return;
  }
  mShell->FrameConstructor()->PostRebuildAllStyleDataEvent();
}

void
nsPresContext::MediaFeatureValuesChanged(PRBool aCallerWillRebuildStyleData)
{
  mPendingMediaFeatureValuesChanged = PR_FALSE;
  if (mShell->StyleSet()->MediumFeaturesChanged(this) &&
      !aCallerWillRebuildStyleData) {
    RebuildAllStyleData(nsChangeHint(0));
  }
}

void
nsPresContext::PostMediaFeatureValuesChangedEvent()
{
  if (!mPendingMediaFeatureValuesChanged) {
    nsCOMPtr<nsIRunnable> ev =
      new nsRunnableMethod<nsPresContext>(this,
                         &nsPresContext::HandleMediaFeatureValuesChangedEvent);
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
nsPresContext::EnsureVisible(PRBool aUnsuppressFocus)
{
  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mContainer));
  if (docShell) {
    nsCOMPtr<nsIContentViewer> cv;
    docShell->GetContentViewer(getter_AddRefs(cv));
    // Make sure this is the content viewer we belong with
    nsCOMPtr<nsIDocumentViewer> docV(do_QueryInterface(cv));
    if (docV) {
      nsCOMPtr<nsPresContext> currentPresContext;
      docV->GetPresContext(getter_AddRefs(currentPresContext));
      if (currentPresContext == this) {
        // OK, this is us.  We want to call Show() on the content viewer.  But
        // first, we need to suppress focus changes; otherwise the focus will
        // get sent to the wrong place (toplevel window).
        nsCOMPtr<nsPIDOMWindow> privWindow = do_GetInterface(docShell);
        // XXXbz privWindow should never really be null!
        nsIFocusController* fc =
          privWindow ? privWindow->GetRootFocusController() : nsnull;
        if (fc) {
          fc->SetSuppressFocus(PR_TRUE,
                               "nsPresContext::EnsureVisible Suppression");
        }
        cv->Show();
        if (fc && aUnsuppressFocus) {
          fc->SetSuppressFocus(PR_FALSE,
                               "nsPresContext::EnsureVisible Suppression");
        }
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
nsPresContext::IsChrome() const
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
  return isChrome;
}

/* virtual */ PRBool
nsPresContext::HasAuthorSpecifiedRules(nsIFrame *aFrame, PRUint32 ruleTypeMask) const
{
  return nsRuleNode::
    HasAuthorSpecifiedRules(aFrame->GetStyleContext(), ruleTypeMask);
}

static void
InsertFontFaceRule(nsCSSFontFaceRule *aRule, gfxUserFontSet* aFontSet)
{
  PRInt32 type;
  NS_ABORT_IF_FALSE(NS_SUCCEEDED(aRule->GetType(type)) 
                    && type == nsICSSRule::FONT_FACE_RULE, 
                    "InsertFontFaceRule passed a non-fontface CSS rule");
  
  // aRule->List();
  
  nsAutoString fontfamily;
  nsCSSValue val;
  
  PRUint32 unit;
  PRUint32 weight = NS_STYLE_FONT_WEIGHT_NORMAL;
  PRUint32 stretch = NS_STYLE_FONT_STRETCH_NORMAL;
  PRUint32 italicStyle = FONT_STYLE_NORMAL;
  
  // set up family name
  aRule->GetDesc(eCSSFontDesc_Family, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_String) {
    val.GetStringValue(fontfamily);
    fontfamily.Trim("\"");
  } else {
    NS_ASSERTION(unit == eCSSUnit_String, 
                 "@font-face family name has non-string unit type");
    return;
  }
  
  // set up weight
  aRule->GetDesc(eCSSFontDesc_Weight, val);
  unit = val.GetUnit();
  if (unit != eCSSUnit_Null) {
    if (unit == eCSSUnit_Normal) {
      weight = NS_STYLE_FONT_WEIGHT_NORMAL;
    } else {
      weight = val.GetIntValue();
    }
  }
  
  // set up stretch
  aRule->GetDesc(eCSSFontDesc_Stretch, val);
  unit = val.GetUnit();
  if (unit != eCSSUnit_Null) {
    if (unit == eCSSUnit_Normal) {
      stretch = NS_STYLE_FONT_STRETCH_NORMAL;
    } else {
      stretch = val.GetIntValue();
    }
  }
  
  // set up font style
  aRule->GetDesc(eCSSFontDesc_Style, val);
  if (val.GetUnit() != eCSSUnit_Null) {
    if (val.GetUnit() == eCSSUnit_Normal) {
      italicStyle = FONT_STYLE_NORMAL;
    } else {
      italicStyle = val.GetIntValue();
    }
  }
  
  // set up src array
  nsTArray<gfxFontFaceSrc> srcArray;

  aRule->GetDesc(eCSSFontDesc_Src, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Array) {
    nsCSSValue::Array *srcArr = val.GetArrayValue();
    PRUint32 i, numSrc = srcArr->Count();
    
    for (i = 0; i < numSrc; i++) {
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
        face->mLocalName.Truncate();
        face->mFormatFlags = 0;
        while (i + 1 < numSrc && (val = srcArr->Item(i+1), 
                 val.GetUnit() == eCSSUnit_Font_Format)) {
          nsDependentString valueString(val.GetStringBufferValue());
          if (valueString.LowerCaseEqualsASCII("opentype")) {
            face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_OPENTYPE; 
          } else if (valueString.LowerCaseEqualsASCII("truetype")) {
            face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_TRUETYPE; 
          } else if (valueString.LowerCaseEqualsASCII("truetype-aat")) {
            face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_TRUETYPE_AAT; 
          } else if (valueString.LowerCaseEqualsASCII("embedded-opentype")) {
            face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_EOT;   
          } else if (valueString.LowerCaseEqualsASCII("svg")) {
            face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_SVG;   
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
  }
  
  if (!fontfamily.IsEmpty() && srcArray.Length() > 0) {
    aFontSet->AddFontFace(fontfamily, srcArray, weight, stretch, italicStyle);
  }
}

gfxUserFontSet*
nsPresContext::GetUserFontSet()
{
  // We want to initialize the user font set lazily the first time the
  // user asks for it, rather than building it too early and forcing
  // rule cascade creation.  Thus we try to enforce the invariant that
  // we *never* build the user font set until the first call to
  // GetUserFontSet.  However, once it's been requested, we can't wait
  // for somebody to call GetUserFontSet in order to rebuild it (see
  // comments below in RebuildUserFontSet for why).
  if (mUserFontSetDirty) {
    // If this assertion fails, and there have actually been changes to
    // @font-face rules, then we will call StyleChangeReflow in
    // FlushUserFontSet.  Since we're likely in the middle of reflow,
    // that's a bad thing to do.
    NS_ASSERTION(!mGetUserFontSetCalled,
                 "FlushUserFontSet should have been called first");
    FlushUserFontSet();
  }

  mGetUserFontSetCalled = PR_TRUE;
  return mUserFontSet;
}

void
nsPresContext::FlushUserFontSet()
{
  if (!mShell)
    return; // we've been torn down

  if (mUserFontSetDirty) {
    if (gfxPlatform::GetPlatform()->DownloadableFontsEnabled()) {
      nsRefPtr<gfxUserFontSet> oldUserFontSet = mUserFontSet;

      nsTArray< nsRefPtr<nsCSSFontFaceRule> > rules;
      if (!mShell->StyleSet()->AppendFontFaceRules(this, rules))
        return;

      PRBool differ;
      if (rules.Length() == mFontFaceRules.Length()) {
        differ = PR_FALSE;
        for (PRUint32 i = 0, i_end = rules.Length(); i < i_end; ++i) {
          if (rules[i] != mFontFaceRules[i]) {
            differ = PR_TRUE;
            break;
          }
        }
      } else {
        differ = PR_TRUE;
      }

      // Only rebuild things if the set of @font-face rules is different.
      if (differ) {
        NS_IF_RELEASE(mUserFontSet);

        if (rules.Length() > 0) {
          nsFontFaceLoaderContext *loaderCtx =
            new nsFontFaceLoaderContext(this);
          if (!loaderCtx)
            return;
          gfxUserFontSet *fs = new gfxUserFontSet(loaderCtx);
          // user font set owns loader context
          if (!fs) {
            delete loaderCtx;
            return;
          }
          mUserFontSet = fs;
          NS_ADDREF(mUserFontSet);

          for (PRUint32 i = 0, i_end = rules.Length(); i < i_end; ++i) {
            InsertFontFaceRule(rules[i], fs);
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
        // reflect that we're dropping @font-face rules.  This is the
        // same thing nsFontFaceLoader does when font downloads
        // complete.  (However, without a reflow, nothing will happen
        // to start any downloads that are needed.)
        mShell->StyleChangeReflow();
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
      new nsRunnableMethod<nsPresContext>(this,
                                     &nsPresContext::HandleRebuildUserFontSet);
    if (NS_SUCCEEDED(NS_DispatchToCurrentThread(ev))) {
      mPostedFlushUserFontSet = PR_TRUE;
    }
  }    
}

void
nsPresContext::FireDOMPaintEvent()
{
  nsCOMPtr<nsPIDOMWindow> ourWindow = mDocument->GetWindow();
  if (!ourWindow)
    return;

  nsISupports* eventTarget = ourWindow;
  if (mSameDocDirtyRegion.IsEmpty() && !IsChrome()) {
    // Don't tell the window about this event, it should not know that
    // something happened in a subdocument. Tell only the chrome event handler.
    // (Events sent to the window get propagated to the chrome event handler
    // automatically.)
    eventTarget = ourWindow->GetChromeEventHandler();
    if (!eventTarget) {
      return;
    }
  }
  // Events sent to the window get propagated to the chrome event handler
  // automatically.

  nsNotifyPaintEvent event(PR_TRUE, NS_AFTERPAINT, mSameDocDirtyRegion,
                           mCrossDocDirtyRegion);
  // Empty our regions now in case dispatching the event causes more damage
  // (hopefully it won't, or we're likely to get an infinite loop! At least
  // it won't be blocking app execution though).
  mSameDocDirtyRegion.SetEmpty();
  mCrossDocDirtyRegion.SetEmpty();
  // Even if we're not telling the window about the event (so eventTarget is
  // the chrome event handler, not the window), the window is still
  // logically the event target.
  event.target = do_QueryInterface(ourWindow);
  nsEventDispatcher::Dispatch(eventTarget, this, &event);
}

static PRBool MayHavePaintEventListener(nsPIDOMWindow* aInnerWindow)
{
  if (!aInnerWindow)
    return PR_FALSE;
  if (aInnerWindow->HasPaintEventListeners())
    return PR_TRUE;

  nsPIDOMEventTarget* chromeEventHandler = aInnerWindow->GetChromeEventHandler();
  if (!chromeEventHandler)
    return PR_FALSE;

  nsCOMPtr<nsIEventListenerManager> manager;
  chromeEventHandler->GetListenerManager(PR_FALSE, getter_AddRefs(manager));
  if (manager && manager->MayHavePaintEventListener())
    return PR_TRUE;

  nsCOMPtr<nsINode> node = do_QueryInterface(chromeEventHandler);
  if (node)
    return MayHavePaintEventListener(node->GetOwnerDoc()->GetInnerWindow());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(chromeEventHandler);
  if (window)
    return MayHavePaintEventListener(window);

  return PR_FALSE;
}

void
nsPresContext::NotifyInvalidation(const nsRect& aRect, PRBool aIsCrossDoc)
{
  // If there is no paint event listener, then we don't need to fire
  // the asynchronous event. We don't even need to record invalidation.
  // MayHavePaintEventListener is pretty cheap and we could make it
  // even cheaper by providing a more efficient
  // nsPIDOMWindow::GetListenerManager.
  if (aRect.IsEmpty() ||
      !MayHavePaintEventListener(mDocument->GetInnerWindow()))
    return;

  if (mSameDocDirtyRegion.IsEmpty() && mCrossDocDirtyRegion.IsEmpty()) {
    // No event is pending. Dispatch one now.
    nsCOMPtr<nsIRunnable> ev =
      new nsRunnableMethod<nsPresContext>(this,
                                          &nsPresContext::FireDOMPaintEvent);
    NS_DispatchToCurrentThread(ev);
  }

  nsRegion* r = aIsCrossDoc ? &mCrossDocDirtyRegion : &mSameDocDirtyRegion;
  r->Or(*r, aRect);
  r->SimplifyOutward(10);
}
