/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIPrefService.h"
#include "nsIPrefBranchInternal.h"
#include "nsISupportsPrimitives.h"
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
#include "nsIDOMElement.h"
#include "nsContentPolicyUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsNetUtil.h"
#include "nsXPIDLString.h"
#include "nsIWeakReferenceUtils.h"

#include "prprf.h"
#include "nsContentPolicyUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMDocument.h"
#ifdef IBMBIDI
#include "nsBidiPresUtils.h"
#endif // IBMBIDI

// Needed for Start/Stop of Image Animation
#include "imgIContainer.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIImageFrame.h"

//needed for resetting of image service color
#include "nsLayoutCID.h"
#include "nsISelectionImageService.h"

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
static NS_DEFINE_CID(kSelectionImageService, NS_SELECTIONIMAGESERVICE_CID);


nsPresContext::nsPresContext()
  : mDefaultVariableFont("serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
      NS_FONT_WEIGHT_NORMAL, 0, NSIntPointsToTwips(12)),
    mDefaultFixedFont("monospace", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
      NS_FONT_WEIGHT_NORMAL, 0, NSIntPointsToTwips(10)),
    mDefaultSerifFont("serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
      NS_FONT_WEIGHT_NORMAL, 0, NSIntPointsToTwips(12)),
    mDefaultSansSerifFont("sans-serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
      NS_FONT_WEIGHT_NORMAL, 0, NSIntPointsToTwips(12)),
    mDefaultMonospaceFont("monospace", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
      NS_FONT_WEIGHT_NORMAL, 0, NSIntPointsToTwips(10)),
    mDefaultCursiveFont("cursive", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL, 
      NS_FONT_WEIGHT_NORMAL, 0, NSIntPointsToTwips(12)),
    mDefaultFantasyFont("fantasy", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL, 
      NS_FONT_WEIGHT_NORMAL, 0, NSIntPointsToTwips(12)),
    mNoTheme(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
  mCompatibilityMode = eCompatibility_FullStandards;
  mWidgetRenderingMode = eWidgetRendering_Gfx; 
  mImageAnimationMode = imgIContainer::kNormalAnimMode;
  mImageAnimationModePref = imgIContainer::kNormalAnimMode;

  SetBackgroundImageDraw(PR_TRUE);		// always draw the background
  SetBackgroundColorDraw(PR_TRUE);

  mStopped = PR_FALSE;
  mStopChrome = PR_TRUE;

  mShell = nsnull;
  mLinkHandler = nsnull;
  mContainer = nsnull;


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

  // the minimum font-size is unconstrained by default
  mMinimumFontSize = 0;

  mLinkColor = NS_RGB(0x33, 0x33, 0xFF);
  mVisitedLinkColor = NS_RGB(0x66, 0x00, 0xCC);
  mUnderlineLinks = PR_TRUE;

  mUseFocusColors = PR_FALSE;
  mFocusTextColor = mDefaultColor;
  mFocusBackgroundColor = mDefaultBackgroundColor;
  mFocusRingWidth = 1;
  mFocusRingOnAnything = PR_FALSE;

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

  // Unregister preference observers
  nsCOMPtr<nsIPrefBranchInternal> prefInternal(do_QueryInterface(mPrefBranch));
  if (prefInternal) {
    prefInternal->RemoveObserver("browser.anchor_color", this);
    prefInternal->RemoveObserver("browser.display.", this);
    prefInternal->RemoveObserver("browser.underline_anchors", this);
    prefInternal->RemoveObserver("browser.visited_color", this);
    prefInternal->RemoveObserver("font.", this);
    prefInternal->RemoveObserver("image.animation_mode", this);
    prefInternal->RemoveObserver("network.image.imageBehavior", this);
#ifdef IBMBIDI
    prefInternal->RemoveObserver("bidi.", this);
#endif
  }
#ifdef IBMBIDI
  delete mBidiUtils;
#endif // IBMBIDI
}

NS_IMPL_ISUPPORTS2(nsPresContext, nsIPresContext, nsIObserver)

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

void
nsPresContext::GetFontPreferences()
{
  if (!mPrefBranch || !mLanguage)
    return;

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

  float p2t;
  GetScaledPixelsToTwips(&p2t);
  mDefaultVariableFont.size = NSFloatPixelsToTwips((float)16, p2t);
  mDefaultFixedFont.size = NSFloatPixelsToTwips((float)13, p2t);

  nsAutoString langGroup;
  nsCOMPtr<nsIAtom> langGroupAtom;
  mLanguage->GetLanguageGroup(getter_AddRefs(langGroupAtom));
  langGroupAtom->ToString(langGroup);

  nsCAutoString pref;
  nsXPIDLCString cvalue;

  // get the current applicable font-size unit
  enum {eUnit_unknown = -1, eUnit_px, eUnit_pt};
  PRInt32 unit = eUnit_px;
  nsresult rv = mPrefBranch->GetCharPref("font.size.unit", getter_Copies(cvalue));
  if (NS_SUCCEEDED(rv)) {
    if (!PL_strcmp(cvalue.get(), "px")) {
      unit = eUnit_px;
    }
    else if (!PL_strcmp(cvalue.get(), "pt")) {
      unit = eUnit_pt;
    }
    else {
      NS_WARNING("unexpected font-size unit -- expected: 'px' or 'pt'");
      unit = eUnit_unknown;
    }
  }

  // get font.minimum-size.[langGroup]
  PRInt32 size;
  pref.Assign("font.minimum-size.");
  pref.Append(NS_ConvertUCS2toUTF8(langGroup));
  rv = mPrefBranch->GetIntPref(pref.get(), &size);
  if (NS_SUCCEEDED(rv)) {
    if (unit == eUnit_px) {
      mMinimumFontSize = NSFloatPixelsToTwips((float)size, p2t);
    }
    else if (unit == eUnit_pt) {
      mMinimumFontSize = NSIntPointsToTwips(size);
    }
  }

  // get attributes specific to each generic font
  nsCAutoString generic_dot_langGroup;
  for (PRInt32 eType = eDefaultFont_Variable; eType < eDefaultFont_COUNT; ++eType) {
    generic_dot_langGroup.Assign(kGenericFont[eType]);
    generic_dot_langGroup.Append(NS_ConvertUCS2toUTF8(langGroup));

    nsFont* font = nsnull;
    switch (eType) {
      case eDefaultFont_Variable:  font = &mDefaultVariableFont;  break;
      case eDefaultFont_Fixed:     font = &mDefaultFixedFont;     break;
      case eDefaultFont_Serif:     font = &mDefaultSerifFont;     break;
      case eDefaultFont_SansSerif: font = &mDefaultSansSerifFont; break;
      case eDefaultFont_Monospace: font = &mDefaultMonospaceFont; break;
      case eDefaultFont_Cursive:   font = &mDefaultCursiveFont;   break;
      case eDefaultFont_Fantasy:   font = &mDefaultFantasyFont;   break;
      default: NS_ERROR("not reached - bogus to silence some compilers"); break;
    }

    // set the default variable font (the other fonts are seen as 'generic' fonts
    // in GFX and will be queried there when hunting for alternative fonts)
    if (eType == eDefaultFont_Variable) {
      MAKE_FONT_PREF_KEY(pref, "font.name", generic_dot_langGroup);
      nsCOMPtr<nsISupportsString> prefString;
      mPrefBranch->GetComplexValue(pref.get(),
                                   NS_GET_IID(nsISupportsString),
                                   getter_AddRefs(prefString));
      if (prefString) {
        prefString->GetData(font->name);
      }
      else {
        mPrefBranch->GetComplexValue("font.default",
                                     NS_GET_IID(nsISupportsString),
                                     getter_AddRefs(prefString));
        if (prefString) {
          prefString->GetData(mDefaultVariableFont.name);
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
    rv = mPrefBranch->GetIntPref(pref.get(), &size);
    if (NS_SUCCEEDED(rv) && size > 0) {
      if (unit == eUnit_px) {
        font->size = NSFloatPixelsToTwips((float)size, p2t);
      }
      else if (unit == eUnit_pt) {
        font->size = NSIntPointsToTwips(size);
      }
    }

    // get font.size-adjust.[generic].[langGroup]
    // XXX only applicable on GFX ports that handle |font-size-adjust|
    MAKE_FONT_PREF_KEY(pref, "font.size-adjust", generic_dot_langGroup);
    rv = mPrefBranch->GetCharPref(pref.get(), getter_Copies(cvalue));
    if (NS_SUCCEEDED(rv)) {
      font->sizeAdjust = (float)atof(cvalue.get());
    }

#ifdef DEBUG_rbs
    nsCAutoString family(NS_ConvertUCS2toUTF8(font->name));
    printf("%s Family-list:%s size:%d sizeAdjust:%.2f\n",
            generic_dot_langGroup.get(), family.get(), font->size, font->sizeAdjust);
#endif
  }
}

void
nsPresContext::GetDocumentColorPreferences()
{
  PRBool usePrefColors = PR_TRUE;
  PRBool boolPref;
  nsXPIDLCString colorStr;
  nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryReferent(mContainer));
  if (docShell) {
    PRInt32 docShellType;
    docShell->GetItemType(&docShellType);
    if (nsIDocShellTreeItem::typeChrome == docShellType)
      usePrefColors = PR_FALSE;
  }
  if (usePrefColors) {
    if (NS_SUCCEEDED(mPrefBranch->GetBoolPref("browser.display.use_system_colors", &boolPref))) {
      usePrefColors = !boolPref;
    }
  }
  if (usePrefColors) {
    if (NS_SUCCEEDED(mPrefBranch->GetCharPref("browser.display.foreground_color", getter_Copies(colorStr)))) {
      mDefaultColor = MakeColorPref(colorStr);
    }
    if (NS_SUCCEEDED(mPrefBranch->GetCharPref("browser.display.background_color", getter_Copies(colorStr)))) {
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

  if (NS_SUCCEEDED(mPrefBranch->GetBoolPref("browser.display.use_document_colors", &boolPref))) {
    mUseDocumentColors = boolPref;
  }
}

void
nsPresContext::GetUserPreferences()
{
  PRInt32 prefInt = 0;

  if (NS_SUCCEEDED(mPrefBranch->GetIntPref("browser.display.base_font_scaler", &prefInt))) {
    mFontScaler = prefInt;
  }

  if (NS_SUCCEEDED(mPrefBranch->GetIntPref("nglayout.widget.mode", &prefInt))) {
    mWidgetRenderingMode = (enum nsWidgetRendering)prefInt;  // bad cast
  }

  // * document colors
  GetDocumentColorPreferences();

  // * link colors
  PRBool boolPref;
  nsXPIDLCString stringPref;
  if (NS_SUCCEEDED(mPrefBranch->GetBoolPref("browser.underline_anchors", &boolPref))) {
    mUnderlineLinks = boolPref;
  }
  if (NS_SUCCEEDED(mPrefBranch->GetCharPref("browser.anchor_color", getter_Copies(stringPref)))) {
    mLinkColor = MakeColorPref(stringPref);
  }
  if (NS_SUCCEEDED(mPrefBranch->GetCharPref("browser.visited_color", getter_Copies(stringPref)))) {
    mVisitedLinkColor = MakeColorPref(stringPref);
  }

  if (NS_SUCCEEDED(mPrefBranch->GetBoolPref("browser.display.use_focus_colors", &boolPref))) {
    mUseFocusColors = boolPref;
    mFocusTextColor = mDefaultColor;
    mFocusBackgroundColor = mDefaultBackgroundColor;
    if (NS_SUCCEEDED(mPrefBranch->GetCharPref("browser.display.focus_text_color", getter_Copies(stringPref)))) {
      mFocusTextColor = MakeColorPref(stringPref);
    }
    if (NS_SUCCEEDED(mPrefBranch->GetCharPref("browser.display.focus_background_color", getter_Copies(stringPref)))) {
      mFocusBackgroundColor = MakeColorPref(stringPref);
    }
  }

  if (NS_SUCCEEDED(mPrefBranch->GetIntPref("browser.display.focus_ring_width", &prefInt))) {
    mFocusRingWidth = prefInt;
  }

  if (NS_SUCCEEDED(mPrefBranch->GetBoolPref("browser.display.focus_ring_on_anything", &boolPref))) {
    mFocusRingOnAnything = boolPref;
  }

  // * use fonts?
  if (NS_SUCCEEDED(mPrefBranch->GetIntPref("browser.display.use_document_fonts", &prefInt))) {
    mUseDocumentFonts = prefInt == 0 ? PR_FALSE : PR_TRUE;
  }
  
  GetFontPreferences();

  // * image animation
  nsresult rv = mPrefBranch->GetCharPref("image.animation_mode", getter_Copies(stringPref));
  if (NS_SUCCEEDED(rv)) {
    if (stringPref.Equals("normal"))
      mImageAnimationModePref = imgIContainer::kNormalAnimMode;
    else if (stringPref.Equals("none"))
      mImageAnimationModePref = imgIContainer::kDontAnimMode;
    else if (stringPref.Equals("once"))
      mImageAnimationModePref = imgIContainer::kLoopOnceAnimMode;
  }

#ifdef IBMBIDI
  if (NS_SUCCEEDED(mPrefBranch->GetIntPref("bidi.direction", &prefInt))) {
     SET_BIDI_OPTION_DIRECTION(mBidi, prefInt);
  }
  if (NS_SUCCEEDED(mPrefBranch->GetIntPref("bidi.texttype", &prefInt))) {
     SET_BIDI_OPTION_TEXTTYPE(mBidi, prefInt);
  }
  if (NS_SUCCEEDED(mPrefBranch->GetIntPref("bidi.controlstextmode", &prefInt))) {
     SET_BIDI_OPTION_CONTROLSTEXTMODE(mBidi, prefInt);
  }
  if (NS_SUCCEEDED(mPrefBranch->GetIntPref("bidi.clipboardtextmode", &prefInt))) {
     SET_BIDI_OPTION_CLIPBOARDTEXTMODE(mBidi, prefInt);
  }
  if (NS_SUCCEEDED(mPrefBranch->GetIntPref("bidi.numeral", &prefInt))) {
     SET_BIDI_OPTION_NUMERAL(mBidi, prefInt);
  }
  if (NS_SUCCEEDED(mPrefBranch->GetIntPref("bidi.support", &prefInt))) {
     SET_BIDI_OPTION_SUPPORT(mBidi, prefInt);
  }
  if (NS_SUCCEEDED(mPrefBranch->GetIntPref("bidi.characterset", &prefInt))) {
     SET_BIDI_OPTION_CHARACTERSET(mBidi, prefInt);
  }
#endif
}

NS_IMETHODIMP
nsPresContext::GetCachedBoolPref(PRUint32 aPrefType, PRBool& aValue)
{
  nsresult rv = NS_OK;

  switch (aPrefType) {
  case kPresContext_UseDocumentFonts :
    aValue = mUseDocumentFonts;
    break;
  case kPresContext_UseDocumentColors:
    aValue = mUseDocumentColors;
    break;
  case kPresContext_UnderlineLinks:
    aValue = mUnderlineLinks;
    break;
  default :
    rv = NS_ERROR_INVALID_ARG;
    NS_ERROR("invalid arg");
  }
  return rv;
}

NS_IMETHODIMP
nsPresContext::GetCachedIntPref(PRUint32 aPrefType, PRInt32& aValue) 
{
  nsresult rv = NS_OK;
  switch (aPrefType) {
    case kPresContext_MinimumFontSize:
      aValue = mMinimumFontSize;
      break;
    default:
      rv = NS_ERROR_INVALID_ARG;
      NS_ERROR("invalid arg");
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

    // Force a reflow of the root frame
    // XXX We really should only do a reflow if a preference that affects
    // formatting changed, e.g., a font change. If it's just a color change
    // then we only need to repaint...
    mShell->StyleChangeReflow();
  }

  return NS_OK;
}

void
nsPresContext::PreferenceChanged(const char* aPrefName)
{
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
  mPrefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  nsCOMPtr<nsIPrefBranchInternal> prefInternal(do_QueryInterface(mPrefBranch));
  if (prefInternal) {
    // Register observers so we're notified when the preferences change
    prefInternal->AddObserver("browser.anchor_color", this, PR_FALSE);
    prefInternal->AddObserver("browser.display.", this, PR_FALSE);
    prefInternal->AddObserver("browser.underline_anchors", this, PR_FALSE);
    prefInternal->AddObserver("browser.visited_color", this, PR_FALSE);
    prefInternal->AddObserver("font.", this, PR_FALSE);
    prefInternal->AddObserver("image.animation_mode", this, PR_FALSE);
    prefInternal->AddObserver("network.image.imageBehavior", this, PR_FALSE);
#ifdef IBMBIDI
    prefInternal->AddObserver("bidi.", this, PR_FALSE);
#endif

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
            mImageAnimationMode = imgIContainer::kNormalAnimMode;
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
  NS_PRECONDITION(aResult, "null out param");
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
nsPresContext::Observe(nsISupports* aSubject, 
                       const char* aTopic,
                       const PRUnichar* aData)
{
  if (!nsCRT::strcmp(aTopic, "charset")) {
    UpdateCharSet(aData);
    if (mDeviceContext) {
      mDeviceContext->FlushFontCache();
      ClearStyleDataAndReflow();
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    PreferenceChanged(NS_LossyConvertUCS2toASCII(aData).get());
    return NS_OK;
  }

  NS_WARNING("unrecognized topic in nsPresContext::Observe");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPresContext::GetCompatibilityMode(nsCompatibility* aResult)
{
  NS_PRECONDITION(aResult, "null out param");
  *aResult = mCompatibilityMode;
  return NS_OK;
}

 
NS_IMETHODIMP
nsPresContext::SetCompatibilityMode(nsCompatibility aMode)
{
  mCompatibilityMode = aMode;

  NS_ENSURE_TRUE(mShell, NS_OK);

  // enable/disable the QuirkSheet
  nsCOMPtr<nsIStyleSet> set;
  mShell->GetStyleSet(getter_AddRefs(set));
  if (set) {
    set->EnableQuirkStyleSheet(mCompatibilityMode == eCompatibility_NavQuirks);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsPresContext::GetWidgetRenderingMode(nsWidgetRendering* aResult)
{
  NS_PRECONDITION(aResult, "null out param");
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
nsPresContext::GetImageAnimationMode(PRUint16* aModeResult)
{
  NS_PRECONDITION(aModeResult, "null out param");
  *aModeResult = mImageAnimationMode;
  return NS_OK;
}

// Helper function for setting Anim Mode on image
static void SetImgAnimModeOnImgReq(imgIRequest* aImgReq, PRUint16 aMode)
{
  if (aImgReq != nsnull) {
    nsCOMPtr<imgIContainer> imgCon;
    aImgReq->GetImage(getter_AddRefs(imgCon));
    if (imgCon) {
      imgCon->SetAnimationMode(aMode);
    }
  }
}

 // Enumeration call back for HashTable
PR_STATIC_CALLBACK(PRBool) set_animation_mode(nsHashKey *aKey, void *aData, void* closure)
{
  nsISupports *sup = NS_REINTERPRET_CAST(nsISupports*, aData);
  nsImageLoader *loader = NS_REINTERPRET_CAST(nsImageLoader*, sup);
  nsCOMPtr<imgIRequest> imgReq;
  loader->GetRequest(getter_AddRefs(imgReq));
  SetImgAnimModeOnImgReq(imgReq, (PRUint16)NS_PTR_TO_INT32(closure));
  return PR_TRUE;
}

// IMPORTANT: Assumption is that all images for a Presentation 
// have the same Animation Mode (pavlov said this was OK)
//
// Walks content and set the animation mode
// this is a way to turn on/off image animations
void nsPresContext::SetImgAnimations(nsCOMPtr<nsIContent>& aParent, PRUint16 aMode)
{
  nsCOMPtr<nsIDOMHTMLImageElement> imgContent(do_QueryInterface(aParent));
  if (imgContent) {
    nsIFrame* frame;
    mShell->GetPrimaryFrameFor(aParent, &frame);
    if (frame != nsnull) {
      nsIImageFrame* imgFrame = nsnull;
      CallQueryInterface(frame, &imgFrame);
      if (imgFrame != nsnull) {
        nsCOMPtr<imgIRequest> imgReq;
        imgFrame->GetImageRequest(getter_AddRefs(imgReq));
        SetImgAnimModeOnImgReq(imgReq, aMode);
      }
    }
  }
  PRInt32 count;
  aParent->ChildCount(count);
  for (PRInt32 i=0;i<count;i++) {
    nsCOMPtr<nsIContent> child;
    aParent->ChildAt(i, *getter_AddRefs(child));
    if (child) {
      SetImgAnimations(child, aMode);
    }
  }
}

NS_IMETHODIMP
nsPresContext::SetImageAnimationMode(PRUint16 aMode)
{
  NS_ASSERTION(aMode == imgIContainer::kNormalAnimMode ||
               aMode == imgIContainer::kDontAnimMode ||
               aMode == imgIContainer::kLoopOnceAnimMode, "Wrong Animation Mode is being set!");

  // This hash table contains a list of background images
  // so iterate over it and set the mode
  mImageLoaders.Enumerate(set_animation_mode, (void*)aMode);

  // Now walk the content tree and set the animation mode 
  // on all the images
  nsCOMPtr<nsIDocument> doc;
  if (mShell != nsnull) {
    mShell->GetDocument(getter_AddRefs(doc));
    if (doc) {
      nsCOMPtr<nsIContent> rootContent;
      doc->GetRootContent(getter_AddRefs(rootContent));
      if (rootContent) {
        SetImgAnimations(rootContent, aMode);
      }
    }
  }

  mImageAnimationMode = aMode;
  return NS_OK;
}

/*
 * It is no longer necesary to hold on to presContext just to get a
 * nsILookAndFeel, which can now be obtained through the service
 * manager.  However, this cached copy can be used when a pres context
 * is available, for faster performance.
 */
NS_IMETHODIMP
nsPresContext::GetLookAndFeel(nsILookAndFeel** aLookAndFeel)
{
  NS_PRECONDITION(aLookAndFeel, "null out param");
  if (! mLookAndFeel) {
    nsresult rv;
    mLookAndFeel = do_GetService(kLookAndFeelCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  *aLookAndFeel = mLookAndFeel;
  NS_ADDREF(*aLookAndFeel);
  return NS_OK;
}

/*
 * Get the cached IO service, faster than the service manager could.
 */
NS_IMETHODIMP
nsPresContext::GetIOService(nsIIOService** aIOService)
{
  NS_PRECONDITION(aIOService, "null out param");
  if (! mIOService) {
    nsresult rv;
    mIOService = do_GetIOService(&rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  *aIOService = mIOService;
  NS_ADDREF(*aIOService);
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetBaseURL(nsIURI** aResult)
{
  NS_PRECONDITION(aResult, "null out param");
  *aResult = mBaseURL;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::ResolveStyleContextFor(nsIContent* aContent,
                                      nsIStyleContext* aParentContext,
                                      nsIStyleContext** aResult)
{
  NS_PRECONDITION(aResult, "null out param");

  nsIStyleContext* result = nsnull;
  nsCOMPtr<nsIStyleSet> set;
  nsresult rv = mShell->GetStyleSet(getter_AddRefs(set));
  if (NS_SUCCEEDED(rv)) {
    if (set) {
      result = set->ResolveStyleFor(this, aContent, aParentContext);
      if (nsnull == result) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  *aResult = result;
  return rv;
}

NS_IMETHODIMP
nsPresContext::ResolveStyleContextForNonElement(
                                      nsIStyleContext* aParentContext,
                                      nsIStyleContext** aResult)
{
  NS_PRECONDITION(aResult, "null out param");

  nsIStyleContext* result = nsnull;
  nsCOMPtr<nsIStyleSet> set;
  nsresult rv = mShell->GetStyleSet(getter_AddRefs(set));
  if (NS_SUCCEEDED(rv) && set) {
    result = set->ResolveStyleForNonElement(this, aParentContext);
    if (!result)
      rv = NS_ERROR_OUT_OF_MEMORY;
  }
  *aResult = result;
  return rv;
}

NS_IMETHODIMP
nsPresContext::ResolvePseudoStyleContextFor(nsIContent* aParentContent,
                                            nsIAtom* aPseudoTag,
                                            nsIStyleContext* aParentContext,
                                            nsIStyleContext** aResult)
{
  return ResolvePseudoStyleWithComparator(aParentContent, aPseudoTag, aParentContext,
                                          nsnull, aResult);
}

NS_IMETHODIMP
nsPresContext::ResolvePseudoStyleWithComparator(nsIContent* aParentContent,
                                                nsIAtom* aPseudoTag,
                                                nsIStyleContext* aParentContext,
                                                nsICSSPseudoComparator* aComparator,
                                                nsIStyleContext** aResult)
{
  NS_PRECONDITION(aResult, "null out param");

  nsIStyleContext* result = nsnull;
  nsCOMPtr<nsIStyleSet> set;
  nsresult rv = mShell->GetStyleSet(getter_AddRefs(set));
  if (NS_SUCCEEDED(rv)) {
    if (set) {
      result = set->ResolvePseudoStyleFor(this, aParentContent, aPseudoTag,
                                          aParentContext, aComparator);
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
                                          nsIStyleContext** aResult)
{
  NS_PRECONDITION(aResult, "null out param");

  nsIStyleContext* result = nsnull;
  nsCOMPtr<nsIStyleSet> set;
  nsresult rv = mShell->GetStyleSet(getter_AddRefs(set));
  if (NS_SUCCEEDED(rv)) {
    if (set) {
      result = set->ProbePseudoStyleFor(this, aParentContent, aPseudoTag,
                                        aParentContext);
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
  NS_PRECONDITION(aResult, "null out param");

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
nsPresContext::GetDefaultFont(PRUint8 aFontID, const nsFont** aResult)
{
  nsresult rv = NS_OK;
  switch (aFontID) {
    // Special (our default variable width font and fixed width font)
    case kPresContext_DefaultVariableFont_ID:
      *aResult = &mDefaultVariableFont;
      break;
    case kPresContext_DefaultFixedFont_ID:
      *aResult = &mDefaultFixedFont;
      break;
    // CSS
    case kGenericFont_serif:
      *aResult = &mDefaultSerifFont;
      break;
    case kGenericFont_sans_serif:
      *aResult = &mDefaultSansSerifFont;
      break;
    case kGenericFont_monospace:
      *aResult = &mDefaultMonospaceFont;
      break;
    case kGenericFont_cursive:
      *aResult = &mDefaultCursiveFont;
      break;
    case kGenericFont_fantasy: 
      *aResult = &mDefaultFantasyFont;
      break;
    default:
      rv = NS_ERROR_INVALID_ARG;
      NS_ERROR("invalid arg");
      break;
  }
  return rv;
}

NS_IMETHODIMP
nsPresContext::SetDefaultFont(PRUint8 aFontID, const nsFont& aFont)
{
  nsresult rv = NS_OK;
  switch (aFontID) {
    // Special (our default variable width font and fixed width font)
    case kPresContext_DefaultVariableFont_ID: 
      mDefaultVariableFont = aFont;
      break;
    case kPresContext_DefaultFixedFont_ID:
      mDefaultFixedFont = aFont;
      break;
    // CSS
    case kGenericFont_serif:
      mDefaultSerifFont = aFont;
      break;
    case kGenericFont_sans_serif:
      mDefaultSansSerifFont = aFont;
      break;
    case kGenericFont_monospace:
      mDefaultMonospaceFont = aFont;
      break;
    case kGenericFont_cursive:
      mDefaultCursiveFont = aFont;
      break;
    case kGenericFont_fantasy: 
      mDefaultFantasyFont = aFont;
      break;
    default:
      rv = NS_ERROR_INVALID_ARG;
      NS_ERROR("invalid arg");
      break;
  }
  return rv;
}

NS_IMETHODIMP
nsPresContext::GetFontScaler(PRInt32* aResult)
{
  NS_PRECONDITION(aResult, "null out param");

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
  NS_PRECONDITION(aResult, "null out param");

  *aResult = mDefaultColor;
  return NS_OK;
}

NS_IMETHODIMP 
nsPresContext::GetDefaultBackgroundColor(nscolor* aResult)
{
  NS_PRECONDITION(aResult, "null out param");

  *aResult = mDefaultBackgroundColor;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetDefaultLinkColor(nscolor* aColor)
{
  NS_PRECONDITION(aColor, "null out param");
  *aColor = mLinkColor;
  return NS_OK;
}

NS_IMETHODIMP 
nsPresContext::GetDefaultVisitedLinkColor(nscolor* aColor)
{
  NS_PRECONDITION(aColor, "null out param");
  *aColor = mVisitedLinkColor;
  return NS_OK;
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
  NS_PRECONDITION(aColor, "null out param");
  *aColor = mFocusTextColor;
  return NS_OK;
}


NS_IMETHODIMP
nsPresContext::GetFocusBackgroundColor(nscolor* aColor)
{
  NS_PRECONDITION(aColor, "null out param");
  *aColor = mFocusBackgroundColor;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetFocusRingWidth(PRUint8 *aFocusRingWidth)
{
  NS_PRECONDITION(aFocusRingWidth, "null out param");
  *aFocusRingWidth = mFocusRingWidth;
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
  NS_PRECONDITION(aResult, "null out param");

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
  NS_PRECONDITION(aResult, "null out param");

  float app2dev = 1.0f;
  if (mDeviceContext) {
    mDeviceContext->GetAppUnitsToDevUnits(app2dev);
  }
  *aResult = app2dev;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetTwipsToPixelsForFonts(float* aResult) const
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  float app2dev = 1.0f;
  if (mDeviceContext) {
#ifdef NS_PRINT_PREVIEW
    // If an alternative DC is available we want to use
    // it to get the scaling factor for fonts. Usually, the AltDC
    // is a printing DC so therefore we need to get the printers
    // scaling values for calculating the font heights
    nsCOMPtr<nsIDeviceContext> altDC;
    mDeviceContext->GetAltDevice(getter_AddRefs(altDC));
    if (altDC) {
      altDC->GetAppUnitsToDevUnits(app2dev);
    } else {
      mDeviceContext->GetAppUnitsToDevUnits(app2dev);
    }
#else
    mDeviceContext->GetAppUnitsToDevUnits(app2dev);
#endif
  }
  *aResult = app2dev;
  return NS_OK;
}



NS_IMETHODIMP
nsPresContext::GetScaledPixelsToTwips(float* aResult) const
{
  NS_PRECONDITION(aResult, "null out param");

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
  NS_PRECONDITION(aResult, "null out param");
  *aResult = mDeviceContext;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetImageLoadFlags(nsLoadFlags& aLoadFlags)
{
  aLoadFlags = nsIRequest::LOAD_NORMAL;

  nsCOMPtr<nsIDocument> doc;
  (void) mShell->GetDocument(getter_AddRefs(doc));

  if (doc) {
    nsCOMPtr<nsILoadGroup> loadGroup;
    (void) doc->GetDocumentLoadGroup(getter_AddRefs(loadGroup));

    if (loadGroup) {
      loadGroup->GetLoadFlags(&aLoadFlags);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::LoadImage(const nsString& aURL,
                         nsIFrame* aTargetFrame,
                         imgIRequest **aRequest)
{
  // look and see if we have a loader for the target frame.

  nsresult rv;

  nsVoidKey key(aTargetFrame);
  nsImageLoader *loader = NS_REINTERPRET_CAST(nsImageLoader*, mImageLoaders.Get(&key)); // addrefs

  nsCOMPtr<nsIDocument> doc;
  rv = mShell->GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIURI> baseURI;
  doc->GetBaseURL(*getter_AddRefs(baseURI));

  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), aURL, nsnull, baseURI);

  if (!loader) {
    nsCOMPtr<nsIContent> content;
    aTargetFrame->GetContent(getter_AddRefs(content));

    // Check with the content-policy things to make sure this load is permitted.
    nsresult rv;
    nsCOMPtr<nsIDOMElement> element(do_QueryInterface(content));

    if (content && element) {
      nsCOMPtr<nsIDocument> document;
      rv = content->GetDocument(*getter_AddRefs(document));

      // If there is no document, skip the policy check
      // XXXldb This really means the document is being destroyed, so
      // perhaps we're better off skipping the load entirely.
      if (document) {
        nsCOMPtr<nsIScriptGlobalObject> globalScript;
        rv = document->GetScriptGlobalObject(getter_AddRefs(globalScript));

        if (globalScript) {
          nsCOMPtr<nsIDOMWindow> domWin(do_QueryInterface(globalScript));

          PRBool shouldLoad = PR_TRUE;
          rv = NS_CheckContentLoadPolicy(nsIContentPolicy::IMAGE,
                                         uri, element, domWin, &shouldLoad);
          if (NS_SUCCEEDED(rv) && !shouldLoad)
            return NS_ERROR_FAILURE;
        }
      }
    }

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

  loader->Load(uri);

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
  NS_PRECONDITION(aResult, "null out param");
  *aResult = mLinkHandler;
  NS_IF_ADDREF(mLinkHandler);
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::SetContainer(nsISupports* aHandler)
{
  mContainer = do_GetWeakReference(aHandler);
  if (mContainer) {
    GetDocumentColorPreferences();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetContainer(nsISupports** aResult)
{
  NS_PRECONDITION(aResult, "null out param");
  if (!mContainer) {
    *aResult = nsnull;
    return NS_OK;
  }

  return CallQueryReferent(mContainer.get(), aResult);
}

NS_IMETHODIMP
nsPresContext::GetEventStateManager(nsIEventStateManager** aManager)
{
  NS_PRECONDITION(aManager, "null ptr");

  if (!mEventManager) {
    nsresult rv;
    mEventManager = do_CreateInstance(kEventStateManagerCID,&rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    //Not refcnted, set null in destructor
    mEventManager->SetPresContext(this);
  }

  *aManager = mEventManager;
  NS_IF_ADDREF(*aManager);
  return NS_OK;
}

#ifdef IBMBIDI
//ahmed
NS_IMETHODIMP
nsPresContext::IsArabicEncoding(PRBool& aResult) const
{
  aResult=PR_FALSE;
  if ( (mCharset.EqualsIgnoreCase("ibm864") )||(mCharset.EqualsIgnoreCase("ibm864i") )||(mCharset.EqualsIgnoreCase("windows-1256") )||(mCharset.EqualsIgnoreCase("iso-8859-6") ))
    aResult=PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::IsVisRTL(PRBool& aResult) const
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
NS_IMETHODIMP   nsPresContext::GetBidi(PRUint32* aDest) const
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
nsPresContext::GetBidiCharset(nsAString &aCharSet) const
{
  aCharSet = mCharset;
  return NS_OK;
}
//Mohamed End

#endif //IBMBIDI

NS_IMETHODIMP
nsPresContext::GetLanguage(nsILanguageAtom** aLanguage)
{
  NS_PRECONDITION(aLanguage, "null out param");

  *aLanguage = mLanguage;
  NS_IF_ADDREF(*aLanguage);

  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetLanguageSpecificTransformType(
                nsLanguageSpecificTransformType* aType)
{
  NS_PRECONDITION(aType, "null out param");
  *aType = mLanguageSpecificTransformType;

  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::IsRenderingOnlySelection(PRBool* aResult)
{
  NS_PRECONDITION(aResult, "null out param");
  *aResult = mIsRenderingOnlySelection;

  return NS_OK;
}

NS_IMETHODIMP
nsPresContext::GetTheme(nsITheme** aResult)
{
  if (!mNoTheme && !mTheme) {
    mTheme = do_GetService("@mozilla.org/chrome/chrome-native-theme;1");
    if (!mTheme)
      mNoTheme = PR_TRUE;
  }

  *aResult = mTheme;
  NS_IF_ADDREF(*aResult);
  return mTheme ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPresContext::ThemeChanged()
{
  // Tell the theme that it changed, so it can flush any handles to stale theme
  // data.
  if (mTheme)
    mTheme->ThemeChanged();

  // Clear all cached nsILookAndFeel colors.
  if (mLookAndFeel)
    mLookAndFeel->LookAndFeelChanged();
  
  if (!mShell)
    return NS_OK;

  return mShell->ReconstructStyleData(PR_FALSE);
}

NS_IMETHODIMP
nsPresContext::SysColorChanged()
{
  if (mLookAndFeel) {
     // Don't use the cached values for the system colors
    mLookAndFeel->LookAndFeelChanged();
  }
   
  // Reset default background and foreground colors for the document since
  // they may be using system colors
  GetDocumentColorPreferences();

  // Clear out all of the style data since it may contain RGB values
  // which originated from system colors.
  if (mShell) {
    // Clear out all our style data.
    nsCOMPtr<nsIStyleSet> set;
    mShell->GetStyleSet(getter_AddRefs(set));
    set->ClearStyleData(this, nsnull, nsnull);
  }
  nsCOMPtr<nsISelectionImageService> imageService;
  nsresult result;
  imageService = do_GetService(kSelectionImageService, &result);
  if (NS_SUCCEEDED(result) && imageService)
  {
    imageService->Reset();
  }
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
