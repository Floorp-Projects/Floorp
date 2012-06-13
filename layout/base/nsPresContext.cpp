/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a presentation of a document, part 1 */

#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsILinkHandler.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "nsPIDOMWindow.h"
#include "nsStyleSet.h"
#include "nsImageLoader.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIURL.h"
#include "nsIDocument.h"
#include "nsStyleContext.h"
#include "mozilla/LookAndFeel.h"
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
#include "nsEventListenerManager.h"
#include "nsStyleStructInlines.h"
#include "nsIAppShell.h"
#include "prenv.h"
#include "nsIDOMEventTarget.h"
#include "nsObjectFrame.h"
#include "nsTransitionManager.h"
#include "nsAnimationManager.h"
#include "mozilla/dom/Element.h"
#include "nsIFrameMessageManager.h"
#include "FrameLayerBuilder.h"
#include "nsDOMMediaQueryList.h"
#include "nsSMILAnimationController.h"

#ifdef IBMBIDI
#include "nsBidiPresUtils.h"
#endif // IBMBIDI

#include "nsContentUtils.h"
#include "nsPIWindowRoot.h"
#include "mozilla/Preferences.h"

// Needed for Start/Stop of Image Animation
#include "imgIContainer.h"
#include "nsIImageLoadingContent.h"

//needed for resetting of image service color
#include "nsLayoutCID.h"

#include "nsCSSParser.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace {

class CharSetChangingRunnable : public nsRunnable
{
public:
  CharSetChangingRunnable(nsPresContext* aPresContext,
                          const nsCString& aCharSet)
    : mPresContext(aPresContext),
      mCharSet(aCharSet)
  {
  }

  NS_IMETHOD Run()
  {
    mPresContext->DoChangeCharSet(mCharSet);
    return NS_OK;
  }

private:
  nsRefPtr<nsPresContext> mPresContext;
  nsCString mCharSet;
};

} // anonymous namespace

nscolor
nsPresContext::MakeColorPref(const nsString& aColor)
{
  nsCSSParser parser;
  nsCSSValue value;
  if (!parser.ParseColorString(aColor, nsnull, 0, value)) {
    // Any better choices?
    return NS_RGB(0, 0, 0);
  }

  nscolor color;
  return nsRuleNode::ComputeColor(value, this, nsnull, color)
    ? color
    : NS_RGB(0, 0, 0);
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
static bool
IsVisualCharset(const nsCString& aCharset)
{
  if (aCharset.LowerCaseEqualsLiteral("ibm864")             // Arabic//ahmed
      || aCharset.LowerCaseEqualsLiteral("ibm862")          // Hebrew
      || aCharset.LowerCaseEqualsLiteral("iso-8859-8") ) {  // Hebrew
    return true; // visual text type
  }
  else {
    return false; // logical text type
  }
}
#endif // IBMBIDI


static PLDHashOperator
destroy_loads(nsIFrame* aKey, nsRefPtr<nsImageLoader>& aData, void* closure)
{
  aData->Destroy();
  return PL_DHASH_NEXT;
}

#include "nsContentCID.h"

  // NOTE! nsPresContext::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

nsPresContext::nsPresContext(nsIDocument* aDocument, nsPresContextType aType)
  : mType(aType), mDocument(aDocument), mMinFontSize(0),
    mTextZoom(1.0), mFullZoom(1.0), mLastFontInflationScreenWidth(-1.0),
    mPageSize(-1, -1), mPPScale(1.0f),
    mViewportStyleOverflow(NS_STYLE_OVERFLOW_AUTO, NS_STYLE_OVERFLOW_AUTO),
    mImageAnimationModePref(imgIContainer::kNormalAnimMode)
{
  // NOTE! nsPresContext::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

  mDoScaledTwips = true;

  SetBackgroundImageDraw(true);		// always draw the background
  SetBackgroundColorDraw(true);

  mBackgroundColor = NS_RGB(0xFF, 0xFF, 0xFF);
  
  mUseDocumentColors = true;
  mUseDocumentFonts = true;

  // the minimum font-size is unconstrained by default

  mLinkColor = NS_RGB(0x00, 0x00, 0xEE);
  mActiveLinkColor = NS_RGB(0xEE, 0x00, 0x00);
  mVisitedLinkColor = NS_RGB(0x55, 0x1A, 0x8B);
  mUnderlineLinks = true;
  mSendAfterPaintToContent = false;

  mFocusTextColor = mDefaultColor;
  mFocusBackgroundColor = mBackgroundColor;
  mFocusRingWidth = 1;

  mBodyTextColor = mDefaultColor;

  if (aType == eContext_Galley) {
    mMedium = nsGkAtoms::screen;
  } else {
    mMedium = nsGkAtoms::print;
    mPaginated = true;
  }

  if (!IsDynamic()) {
    mImageAnimationMode = imgIContainer::kDontAnimMode;
    mNeverAnimate = true;
  } else {
    mImageAnimationMode = imgIContainer::kNormalAnimMode;
    mNeverAnimate = false;
  }
  NS_ASSERTION(mDocument, "Null document");
  mUserFontSet = nsnull;
  mUserFontSetDirty = true;

  PR_INIT_CLIST(&mDOMMediaQueryLists);
}

nsPresContext::~nsPresContext()
{
  NS_PRECONDITION(!mShell, "Presshell forgot to clear our mShell pointer");
  SetShell(nsnull);

  NS_ABORT_IF_FALSE(PR_CLIST_IS_EMPTY(&mDOMMediaQueryLists),
                    "must not have media query lists left");

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
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "font.",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "browser.display.",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "browser.underline_anchors",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "browser.anchor_color",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "browser.active_color",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "browser.visited_color",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "image.animation_mode",
                                  this);
#ifdef IBMBIDI
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "bidi.",
                                  this);
#endif // IBMBIDI
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "dom.send_after_paint_to_content",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "gfx.font_rendering.",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "layout.css.dpi",
                                  this);
  Preferences::UnregisterCallback(nsPresContext::PrefChangedCallback,
                                  "layout.css.devPixelsPerPx",
                                  this);

  NS_IF_RELEASE(mDeviceContext);
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
TraverseImageLoader(nsIFrame* aKey, nsRefPtr<nsImageLoader>& aData,
                    void* aClosure)
{
  nsCycleCollectionTraversalCallback *cb =
    static_cast<nsCycleCollectionTraversalCallback*>(aClosure);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "mImageLoaders[i] item");
  cb->NoteXPCOMChild(aData);

  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsPresContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocument);
  // NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mDeviceContext); // not xpcom
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mEventManager, nsIObserver);
  // NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mLanguage); // an atom

  for (PRUint32 i = 0; i < IMAGE_LOAD_TYPE_COUNT; ++i)
    tmp->mImageLoaders[i].Enumerate(TraverseImageLoader, &cb);

  // We own only the items in mDOMMediaQueryLists that have listeners;
  // this reference is managed by their AddListener and RemoveListener
  // methods.
  for (PRCList *l = PR_LIST_HEAD(&tmp->mDOMMediaQueryLists);
       l != &tmp->mDOMMediaQueryLists; l = PR_NEXT_LINK(l)) {
    nsDOMMediaQueryList *mql = static_cast<nsDOMMediaQueryList*>(l);
    if (mql->HasListeners()) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mDOMMediaQueryLists item");
      cb.NoteXPCOMChild(mql);
    }
  }

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

  // We own only the items in mDOMMediaQueryLists that have listeners;
  // this reference is managed by their AddListener and RemoveListener
  // methods.
  for (PRCList *l = PR_LIST_HEAD(&tmp->mDOMMediaQueryLists);
       l != &tmp->mDOMMediaQueryLists; ) {
    PRCList *next = PR_NEXT_LINK(l);
    nsDOMMediaQueryList *mql = static_cast<nsDOMMediaQueryList*>(l);
    mql->RemoveAllListeners();
    l = next;
  }

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
static bool sNoTheme = false;

// Set to true when LookAndFeelChanged needs to be called.  This is used
// because the look and feel is a service, so there's no need to notify it from
// more than one prescontext.
static bool sLookAndFeelChanged;

// Set to true when ThemeChanged needs to be called on mTheme.  This is used
// because mTheme is a service, so there's no need to notify it from more than
// one prescontext.
static bool sThemeChanged;

const nsPresContext::LangGroupFontPrefs*
nsPresContext::GetFontPrefsForLang(nsIAtom *aLanguage) const
{
  // Get language group for aLanguage:

  nsresult rv;
  nsIAtom *langGroupAtom = nsnull;
  if (!aLanguage) {
    aLanguage = mLanguage;
  }
  if (aLanguage && mLangService) {
    langGroupAtom = mLangService->GetLanguageGroup(aLanguage, &rv);
  }
  if (NS_FAILED(rv) || !langGroupAtom) {
    langGroupAtom = nsGkAtoms::x_western; // Assume x-western is safe...
  }

  // Look for cached prefs for this lang group.
  // Most documents will only use one (or very few) language groups. Rather
  // than have the overhead of a hash lookup, we simply look along what will
  // typically be a very short (usually of length 1) linked list. There are 31
  // language groups, so in the worst case scenario we'll need to traverse 31
  // link items.

  LangGroupFontPrefs *prefs =
    const_cast<LangGroupFontPrefs*>(&mLangGroupFontPrefs);
  if (prefs->mLangGroup) { // if initialized
    DebugOnly<PRUint32> count = 0;
    for (;;) {
      NS_ASSERTION(++count < 35, "Lang group count exceeded!!!");
      if (prefs->mLangGroup == langGroupAtom) {
        return prefs;
      }
      if (!prefs->mNext) {
        break;
      }
      prefs = prefs->mNext;
    }

    // nothing cached, so go on and fetch the prefs for this lang group:
    prefs = prefs->mNext = new LangGroupFontPrefs;
  }

  prefs->mLangGroup = langGroupAtom;

  /* Fetch the font prefs to be used -- see bug 61883 for details.
     Not all prefs are needed upfront. Some are fallback prefs intended
     for the GFX font sub-system...

  1) unit : assumed to be the same for all language groups -------------
  font.size.unit = px | pt    XXX could be folded in the size... bug 90440

  2) attributes for generic fonts --------------------------------------
  font.default.[langGroup] = serif | sans-serif - fallback generic font
  font.name.[generic].[langGroup] = current user' selected font on the pref dialog
  font.name-list.[generic].[langGroup] = fontname1, fontname2, ... [factory pre-built list]
  font.size.[generic].[langGroup] = integer - settable by the user
  font.size-adjust.[generic].[langGroup] = "float" - settable by the user
  font.minimum-size.[langGroup] = integer - settable by the user
  */

  nsCAutoString langGroup;
  langGroupAtom->ToUTF8String(langGroup);

  prefs->mDefaultVariableFont.size = CSSPixelsToAppUnits(16);
  prefs->mDefaultFixedFont.size = CSSPixelsToAppUnits(13);

  nsCAutoString pref;

  // get the current applicable font-size unit
  enum {eUnit_unknown = -1, eUnit_px, eUnit_pt};
  PRInt32 unit = eUnit_px;

  nsAdoptingCString cvalue =
    Preferences::GetCString("font.size.unit");

  if (!cvalue.IsEmpty()) {
    if (cvalue.Equals("px")) {
      unit = eUnit_px;
    }
    else if (cvalue.Equals("pt")) {
      unit = eUnit_pt;
    }
    else {
      // XXX should really send this warning to the user (Error Console?).
      // And just default to unit = eUnit_px?
      NS_WARNING("unexpected font-size unit -- expected: 'px' or 'pt'");
      unit = eUnit_unknown;
    }
  }

  // get font.minimum-size.[langGroup]

  MAKE_FONT_PREF_KEY(pref, "font.minimum-size.", langGroup);

  PRInt32 size = Preferences::GetInt(pref.get());
  if (unit == eUnit_px) {
    prefs->mMinimumFontSize = CSSPixelsToAppUnits(size);
  }
  else if (unit == eUnit_pt) {
    prefs->mMinimumFontSize = CSSPointsToAppUnits(size);
  }

  nsFont* fontTypes[] = {
    &prefs->mDefaultVariableFont,
    &prefs->mDefaultFixedFont,
    &prefs->mDefaultSerifFont,
    &prefs->mDefaultSansSerifFont,
    &prefs->mDefaultMonospaceFont,
    &prefs->mDefaultCursiveFont,
    &prefs->mDefaultFantasyFont
  };
  MOZ_STATIC_ASSERT(NS_ARRAY_LENGTH(fontTypes) == eDefaultFont_COUNT,
                    "FontTypes array count is not correct");

  // Get attributes specific to each generic font. We do not get the user's
  // generic-font-name-to-specific-family-name preferences because its the
  // generic name that should be fed into the cascade. It is up to the GFX
  // code to look up the font prefs to convert generic names to specific
  // family names as necessary.
  nsCAutoString generic_dot_langGroup;
  for (PRUint32 eType = 0; eType < ArrayLength(fontTypes); ++eType) {
    generic_dot_langGroup.Assign(kGenericFont[eType]);
    generic_dot_langGroup.Append(langGroup);

    nsFont* font = fontTypes[eType];

    // set the default variable font (the other fonts are seen as 'generic' fonts
    // in GFX and will be queried there when hunting for alternative fonts)
    if (eType == eDefaultFont_Variable) {
      MAKE_FONT_PREF_KEY(pref, "font.name.variable.", langGroup);

      nsAdoptingString value = Preferences::GetString(pref.get());
      if (!value.IsEmpty()) {
        prefs->mDefaultVariableFont.name.Assign(value);
      }
      else {
        MAKE_FONT_PREF_KEY(pref, "font.default.", langGroup);
        value = Preferences::GetString(pref.get());
        if (!value.IsEmpty()) {
          prefs->mDefaultVariableFont.name.Assign(value);
        }
      } 
    }
    else {
      if (eType == eDefaultFont_Monospace) {
        // This takes care of the confusion whereby people often expect "monospace" 
        // to have the same default font-size as "-moz-fixed" (this tentative
        // size may be overwritten with the specific value for "monospace" when
        // "font.size.monospace.[langGroup]" is read -- see below)
        prefs->mDefaultMonospaceFont.size = prefs->mDefaultFixedFont.size;
      }
      else if (eType != eDefaultFont_Fixed) {
        // all the other generic fonts are initialized with the size of the
        // variable font, but their specific size can supersede later -- see below
        font->size = prefs->mDefaultVariableFont.size;
      }
    }

    // Bug 84398: for spec purists, a different font-size only applies to the
    // .variable. and .fixed. fonts and the other fonts should get |font-size-adjust|.
    // The problem is that only GfxWin has the support for |font-size-adjust|. So for
    // parity, we enable the ability to set a different font-size on all platforms.

    // get font.size.[generic].[langGroup]
    // size=0 means 'Auto', i.e., generic fonts retain the size of the variable font
    MAKE_FONT_PREF_KEY(pref, "font.size", generic_dot_langGroup);
    size = Preferences::GetInt(pref.get());
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
    cvalue = Preferences::GetCString(pref.get());
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

  return prefs;
}

void
nsPresContext::GetDocumentColorPreferences()
{
  PRInt32 useAccessibilityTheme = 0;
  bool usePrefColors = true;
  nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryReferent(mContainer));
  if (docShell) {
    PRInt32 docShellType;
    docShell->GetItemType(&docShellType);
    if (nsIDocShellTreeItem::typeChrome == docShellType) {
      usePrefColors = false;
    }
    else {
      useAccessibilityTheme =
        LookAndFeel::GetInt(LookAndFeel::eIntID_UseAccessibilityTheme, 0);
      usePrefColors = !useAccessibilityTheme;
    }

  }
  if (usePrefColors) {
    usePrefColors =
      !Preferences::GetBool("browser.display.use_system_colors", false);
  }

  if (usePrefColors) {
    nsAdoptingString colorStr =
      Preferences::GetString("browser.display.foreground_color");

    if (!colorStr.IsEmpty()) {
      mDefaultColor = MakeColorPref(colorStr);
    }

    colorStr = Preferences::GetString("browser.display.background_color");

    if (!colorStr.IsEmpty()) {
      mBackgroundColor = MakeColorPref(colorStr);
    }
  }
  else {
    mDefaultColor =
      LookAndFeel::GetColor(LookAndFeel::eColorID_WindowForeground,
                            NS_RGB(0x00, 0x00, 0x00));
    mBackgroundColor =
      LookAndFeel::GetColor(LookAndFeel::eColorID_WindowBackground,
                            NS_RGB(0xFF, 0xFF, 0xFF));
  }

  // Wherever we got the default background color from, ensure it is
  // opaque.
  mBackgroundColor = NS_ComposeColors(NS_RGB(0xFF, 0xFF, 0xFF),
                                      mBackgroundColor);

  mUseDocumentColors = !useAccessibilityTheme &&
    Preferences::GetBool("browser.display.use_document_colors",
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

  mAutoQualityMinFontSizePixelsPref =
    Preferences::GetInt("browser.display.auto_quality_min_font_size");

  // * document colors
  GetDocumentColorPreferences();

  mSendAfterPaintToContent =
    Preferences::GetBool("dom.send_after_paint_to_content",
                         mSendAfterPaintToContent);

  // * link colors
  mUnderlineLinks =
    Preferences::GetBool("browser.underline_anchors", mUnderlineLinks);

  nsAdoptingString colorStr = Preferences::GetString("browser.anchor_color");

  if (!colorStr.IsEmpty()) {
    mLinkColor = MakeColorPref(colorStr);
  }

  colorStr = Preferences::GetString("browser.active_color");

  if (!colorStr.IsEmpty()) {
    mActiveLinkColor = MakeColorPref(colorStr);
  }

  colorStr = Preferences::GetString("browser.visited_color");

  if (!colorStr.IsEmpty()) {
    mVisitedLinkColor = MakeColorPref(colorStr);
  }

  mUseFocusColors =
    Preferences::GetBool("browser.display.use_focus_colors", mUseFocusColors);

  mFocusTextColor = mDefaultColor;
  mFocusBackgroundColor = mBackgroundColor;

  colorStr = Preferences::GetString("browser.display.focus_text_color");

  if (!colorStr.IsEmpty()) {
    mFocusTextColor = MakeColorPref(colorStr);
  }

  colorStr = Preferences::GetString("browser.display.focus_background_color");

  if (!colorStr.IsEmpty()) {
    mFocusBackgroundColor = MakeColorPref(colorStr);
  }

  mFocusRingWidth =
    Preferences::GetInt("browser.display.focus_ring_width", mFocusRingWidth);

  mFocusRingOnAnything =
    Preferences::GetBool("browser.display.focus_ring_on_anything",
                         mFocusRingOnAnything);

  mFocusRingStyle =
    Preferences::GetInt("browser.display.focus_ring_style", mFocusRingStyle);

  mBodyTextColor = mDefaultColor;
  
  // * use fonts?
  mUseDocumentFonts =
    Preferences::GetInt("browser.display.use_document_fonts") != 0;

  // * replace backslashes with Yen signs? (bug 245770)
  mEnableJapaneseTransform =
    Preferences::GetBool("layout.enable_japanese_specific_transform");

  mPrefScrollbarSide = Preferences::GetInt("layout.scrollbar.side");

  ResetCachedFontPrefs();

  // * image animation
  const nsAdoptingCString& animatePref =
    Preferences::GetCString("image.animation_mode");
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
    Preferences::GetInt(IBMBIDI_TEXTDIRECTION_STR,
                        GET_BIDI_OPTION_DIRECTION(bidiOptions));
  SET_BIDI_OPTION_DIRECTION(bidiOptions, prefInt);
  mPrefBidiDirection = prefInt;

  prefInt =
    Preferences::GetInt(IBMBIDI_TEXTTYPE_STR,
                        GET_BIDI_OPTION_TEXTTYPE(bidiOptions));
  SET_BIDI_OPTION_TEXTTYPE(bidiOptions, prefInt);

  prefInt =
    Preferences::GetInt(IBMBIDI_NUMERAL_STR,
                        GET_BIDI_OPTION_NUMERAL(bidiOptions));
  SET_BIDI_OPTION_NUMERAL(bidiOptions, prefInt);

  prefInt =
    Preferences::GetInt(IBMBIDI_SUPPORTMODE_STR,
                        GET_BIDI_OPTION_SUPPORT(bidiOptions));
  SET_BIDI_OPTION_SUPPORT(bidiOptions, prefInt);

  // We don't need to force reflow: either we are initializing a new
  // prescontext or we are being called from UpdateAfterPreferencesChanged()
  // which triggers a reflow anyway.
  SetBidi(bidiOptions, false);
}

void
nsPresContext::InvalidateThebesLayers()
{
  if (!mShell)
    return;
  nsIFrame* rootFrame = mShell->FrameManager()->GetRootFrame();
  if (rootFrame) {
    // FrameLayerBuilder caches invalidation-related values that depend on the
    // appunits-per-dev-pixel ratio, so ensure that all ThebesLayer drawing
    // is completely flushed.
    FrameLayerBuilder::InvalidateThebesLayersInSubtree(rootFrame);
  }
}

void
nsPresContext::AppUnitsPerDevPixelChanged()
{
  InvalidateThebesLayers();

  mDeviceContext->FlushFontCache();

  // All cached style data must be recomputed.
  if (HasCachedStyleData()) {
    MediaFeatureValuesChanged(true);
    RebuildAllStyleData(NS_STYLE_HINT_REFLOW);
  }
}

void
nsPresContext::PreferenceChanged(const char* aPrefName)
{
  nsDependentCString prefName(aPrefName);
  if (prefName.EqualsLiteral("layout.css.dpi") ||
      prefName.EqualsLiteral("layout.css.devPixelsPerPx")) {
    PRInt32 oldAppUnitsPerDevPixel = AppUnitsPerDevPixel();
    if (mDeviceContext->CheckDPIChange() && mShell) {
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

      AppUnitsPerDevPixelChanged();
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
    mPrefChangePendingNeedsReflow = true;
  }
  if (StringBeginsWith(prefName, NS_LITERAL_CSTRING("bidi."))) {
    // Changes to bidi prefs need to trigger a reflow (see bug 443629)
    mPrefChangePendingNeedsReflow = true;

    // Changes to bidi.numeral also needs to empty the text run cache.
    // This is handled in gfxTextRunWordCache.cpp.
  }
  if (StringBeginsWith(prefName, NS_LITERAL_CSTRING("gfx.font_rendering."))) {
    // Changes to font_rendering prefs need to trigger a reflow
    mPrefChangePendingNeedsReflow = true;
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
    mShell->SetPreferenceStyleRules(true);
  }

  InvalidateThebesLayers();
  mDeviceContext->FlushFontCache();

  nsChangeHint hint = nsChangeHint(0);

  if (mPrefChangePendingNeedsReflow) {
    NS_UpdateHint(hint, NS_STYLE_HINT_REFLOW);
  }

  RebuildAllStyleData(hint);
}

nsresult
nsPresContext::Init(nsDeviceContext* aDeviceContext)
{
  NS_ASSERTION(!mInitialized, "attempt to reinit pres context");
  NS_ENSURE_ARG(aDeviceContext);

  mDeviceContext = aDeviceContext;
  NS_ADDREF(mDeviceContext);

  if (mDeviceContext->SetPixelScale(mFullZoom))
    mDeviceContext->FlushFontCache();
  mCurAppUnitsPerDevPixel = AppUnitsPerDevPixel();

  for (PRUint32 i = 0; i < IMAGE_LOAD_TYPE_COUNT; ++i) {
    mImageLoaders[i].Init();
  }

  mEventManager = new nsEventStateManager();
  NS_ADDREF(mEventManager);

  mTransitionManager = new nsTransitionManager(this);

  mAnimationManager = new nsAnimationManager(this);

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
    }
  }

  mLangService = do_GetService(NS_LANGUAGEATOMSERVICE_CONTRACTID);

  // Register callbacks so we're notified when the preferences change
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "font.",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "browser.display.",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "browser.underline_anchors",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "browser.anchor_color",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "browser.active_color",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "browser.visited_color",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "image.animation_mode",
                                this);
#ifdef IBMBIDI
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "bidi.",
                                this);
#endif
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "dom.send_after_paint_to_content",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "gfx.font_rendering.",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "layout.css.dpi",
                                this);
  Preferences::RegisterCallback(nsPresContext::PrefChangedCallback,
                                "layout.css.devPixelsPerPx",
                                this);

  nsresult rv = mEventManager->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  mEventManager->SetPresContext(this);

#ifdef DEBUG
  mInitialized = true;
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
        bool isChrome = false;
        bool isRes = false;
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
    if (mTransitionManager) {
      mTransitionManager->Disconnect();
      mTransitionManager = nsnull;
    }
    if (mAnimationManager) {
      mAnimationManager->Disconnect();
      mAnimationManager = nsnull;
    }

    if (IsRoot()) {
      // Have to cancel our plugin geometry timer, because the
      // callback for that depends on a non-null presshell.
      static_cast<nsRootPresContext*>(this)->CancelUpdatePluginGeometryTimer();
    }
  }
}

void
nsPresContext::DestroyImageLoaders()
{
  // Destroy image loaders. This is important to do when frames are being
  // destroyed because imageloaders can have pointers to frames and we don't
  // want those pointers to outlive the destruction of the frame arena.
  for (PRUint32 i = 0; i < IMAGE_LOAD_TYPE_COUNT; ++i) {
    mImageLoaders[i].Enumerate(destroy_loads, nsnull);
    mImageLoaders[i].Clear();
  }
}

void
nsPresContext::DoChangeCharSet(const nsCString& aCharSet)
{
  UpdateCharSet(aCharSet);
  mDeviceContext->FlushFontCache();
  RebuildAllStyleData(NS_STYLE_HINT_REFLOW);
}

void
nsPresContext::UpdateCharSet(const nsCString& aCharSet)
{
  if (mLangService) {
    NS_IF_RELEASE(mLanguage);
    mLanguage = mLangService->LookupCharSet(aCharSet.get()).get();  // addrefs
    // this will be a language group (or script) code rather than a true language code

    // bug 39570: moved from nsLanguageAtomService::LookupCharSet()
    if (mLanguage == nsGkAtoms::Unicode) {
      NS_RELEASE(mLanguage);
      NS_IF_ADDREF(mLanguage = mLangService->GetLocaleLanguage()); 
    }
    ResetCachedFontPrefs();
  }
#ifdef IBMBIDI
  //ahmed

  switch (GET_BIDI_OPTION_TEXTTYPE(GetBidi())) {

    case IBMBIDI_TEXTTYPE_LOGICAL:
      SetVisualMode(false);
      break;

    case IBMBIDI_TEXTTYPE_VISUAL:
      SetVisualMode(true);
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
    nsRefPtr<CharSetChangingRunnable> runnable =
      new CharSetChangingRunnable(this, NS_LossyConvertUTF16toASCII(aData));
    return NS_DispatchToCurrentThread(runnable);
  }

  NS_WARNING("unrecognized topic in nsPresContext::Observe");
  return NS_ERROR_FAILURE;
}

nsPresContext*
nsPresContext::GetParentPresContext()
{
  nsIPresShell* shell = GetPresShell();
  if (shell) {
    nsIFrame* rootFrame = shell->FrameManager()->GetRootFrame();
    if (rootFrame) {
      nsIFrame* f = nsLayoutUtils::GetCrossDocParentFrame(rootFrame);
      if (f)
        return f->PresContext();
    }
  }
  return nsnull;
}

nsPresContext*
nsPresContext::GetToplevelContentDocumentPresContext()
{
  if (IsChrome())
    return nsnull;
  nsPresContext* pc = this;
  for (;;) {
    nsPresContext* parent = pc->GetParentPresContext();
    if (!parent || parent->IsChrome())
      return pc;
    pc = parent;
  }
}

// We may want to replace this with something faster, maybe caching the root prescontext
nsRootPresContext*
nsPresContext::GetRootPresContext()
{
  nsPresContext* pc = this;
  for (;;) {
    nsPresContext* parent = pc->GetParentPresContext();
    if (!parent)
      break;
    pc = parent;
  }
  return pc->IsRoot() ? static_cast<nsRootPresContext*>(pc) : nsnull;
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
set_animation_mode(nsIFrame* aKey, nsRefPtr<nsImageLoader>& aData, void* closure)
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
      SetSMILAnimations(doc, aMode, mImageAnimationMode);
    }
  }

  mImageAnimationMode = aMode;
}

void
nsPresContext::SetImageAnimationModeExternal(PRUint16 aMode)
{
  SetImageAnimationModeInternal(aMode);
}

const nsFont*
nsPresContext::GetDefaultFont(PRUint8 aFontID, nsIAtom *aLanguage) const
{
  const LangGroupFontPrefs *prefs = GetFontPrefsForLang(aLanguage);

  const nsFont *font;
  switch (aFontID) {
    // Special (our default variable width font and fixed width font)
    case kPresContext_DefaultVariableFont_ID:
      font = &prefs->mDefaultVariableFont;
      break;
    case kPresContext_DefaultFixedFont_ID:
      font = &prefs->mDefaultFixedFont;
      break;
    // CSS
    case kGenericFont_serif:
      font = &prefs->mDefaultSerifFont;
      break;
    case kGenericFont_sans_serif:
      font = &prefs->mDefaultSansSerifFont;
      break;
    case kGenericFont_monospace:
      font = &prefs->mDefaultMonospaceFont;
      break;
    case kGenericFont_cursive:
      font = &prefs->mDefaultCursiveFont;
      break;
    case kGenericFont_fantasy: 
      font = &prefs->mDefaultFantasyFont;
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
  mDeviceContext->SetPixelScale(aZoom);

  NS_ASSERTION(!mSupressResizeReflow, "two zooms happening at the same time? impossible!");
  mSupressResizeReflow = true;

  mFullZoom = aZoom;
  mShell->GetViewManager()->
    SetWindowDimensions(NSToCoordRound(oldWidthDevPixels * AppUnitsPerDevPixel()),
                        NSToCoordRound(oldHeightDevPixels * AppUnitsPerDevPixel()));

  AppUnitsPerDevPixelChanged();

  mSupressResizeReflow = false;

  mCurAppUnitsPerDevPixel = AppUnitsPerDevPixel();
}

float
nsPresContext::ScreenWidthInchesForFontInflation(bool* aChanged)
{
  if (aChanged) {
    *aChanged = false;
  }

  nsDeviceContext *dx = DeviceContext();
  nsRect clientRect;
  dx->GetClientRect(clientRect); // FIXME: GetClientRect looks expensive
  float deviceWidthInches =
    float(clientRect.width) / float(dx->AppUnitsPerPhysicalInch());

  if (mLastFontInflationScreenWidth == -1.0) {
    mLastFontInflationScreenWidth = deviceWidthInches;
  }

  if (deviceWidthInches != mLastFontInflationScreenWidth && aChanged) {
    *aChanged = true;
    mLastFontInflationScreenWidth = deviceWidthInches;
  }

  return deviceWidthInches;
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
  // We get called the first time we try to draw a border-image, and
  // also when the border image changes (including when it changes from
  // non-null to null).
  imgIRequest *borderImage = aStyleBorder->GetBorderImage();
  if (!borderImage) {
    SetImageLoaders(aFrame, BORDER_IMAGE, nsnull);
    return;
  }

  PRUint32 actions = nsImageLoader::ACTION_REDRAW_ON_LOAD;
  nsRefPtr<nsImageLoader> loader =
    nsImageLoader::Create(aFrame, borderImage, actions, nsnull);
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

void
nsPresContext::SetBidi(PRUint32 aSource, bool aForceRestyle)
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
    SetVisualMode(true);
  }
  else if (IBMBIDI_TEXTTYPE_LOGICAL == GET_BIDI_OPTION_TEXTTYPE(aSource)) {
    SetVisualMode(false);
  }
  else {
    nsIDocument* doc = mShell->GetDocument();
    if (doc) {
      SetVisualMode(IsVisualCharset(doc->GetDocumentCharacterSet()));
    }
  }
  if (aForceRestyle && mShell) {
    // Reconstruct the root document element's frame and its children,
    // because we need to trigger frame reconstruction for direction change.
    RebuildUserFontSet();
    mShell->ReconstructFrames();
  }
}

PRUint32
nsPresContext::GetBidi() const
{
  return Document()->GetBidiOptions();
}

#endif //IBMBIDI

bool
nsPresContext::IsTopLevelWindowInactive()
{
  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryReferent(mContainer));
  if (!treeItem)
    return false;

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
      sNoTheme = true;
  }

  return mTheme;
}

void
nsPresContext::ThemeChanged()
{
  if (!mPendingThemeChanged) {
    sLookAndFeelChanged = true;
    sThemeChanged = true;

    nsCOMPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(this, &nsPresContext::ThemeChangedInternal);
    if (NS_SUCCEEDED(NS_DispatchToCurrentThread(ev))) {
      mPendingThemeChanged = true;
    }
  }    
}

void
nsPresContext::ThemeChangedInternal()
{
  mPendingThemeChanged = false;
  
  // Tell the theme that it changed, so it can flush any handles to stale theme
  // data.
  if (mTheme && sThemeChanged) {
    mTheme->ThemeChanged();
    sThemeChanged = false;
  }

  // Clear all cached LookAndFeel colors.
  if (sLookAndFeelChanged) {
    LookAndFeel::Refresh();
    sLookAndFeelChanged = false;
  }

  // This will force the system metrics to be generated the next time they're used
  nsCSSRuleProcessor::FreeSystemMetrics();

  // Changes to system metrics can change media queries on them.
  MediaFeatureValuesChanged(true);

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
    sLookAndFeelChanged = true;
    nsCOMPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(this, &nsPresContext::SysColorChangedInternal);
    if (NS_SUCCEEDED(NS_DispatchToCurrentThread(ev))) {
      mPendingSysColorChanged = true;
    }
  }
}

void
nsPresContext::SysColorChangedInternal()
{
  mPendingSysColorChanged = false;
  
  if (sLookAndFeelChanged) {
     // Don't use the cached values for the system colors
    LookAndFeel::Refresh();
    sLookAndFeelChanged = false;
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
  AnimationManager()->KeyframesListIsDirty();

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
nsPresContext::MediaFeatureValuesChanged(bool aCallerWillRebuildStyleData)
{
  mPendingMediaFeatureValuesChanged = false;
  if (mShell &&
      mShell->StyleSet()->MediumFeaturesChanged(this) &&
      !aCallerWillRebuildStyleData) {
    RebuildAllStyleData(nsChangeHint(0));
  }

  if (!nsContentUtils::IsSafeToRunScript()) {
    NS_ABORT_IF_FALSE(mDocument->IsBeingUsedAsImage(),
                      "How did we get here?  Are we failing to notify "
                      "listeners that we should notify?");
    return;
  }

  // Media query list listeners should be notified from a queued task
  // (in HTML5 terms), although we also want to notify them on certain
  // flushes.  (We're already running off an event.)
  //
  // Note that we do this after the new style from media queries in
  // style sheets has been computed.

  if (!PR_CLIST_IS_EMPTY(&mDOMMediaQueryLists)) {
    // We build a list of all the notifications we're going to send
    // before we send any of them.  (The spec says the notifications
    // should be a queued task, so any removals that happen during the
    // notifications shouldn't affect what gets notified.)  Furthermore,
    // we hold strong pointers to everything we're going to make
    // notification calls to, since each notification involves calling
    // arbitrary script that might otherwise destroy these objects, or,
    // for that matter, |this|.
    //
    // Note that we intentionally send the notifications to media query
    // list in the order they were created and, for each list, to the
    // listeners in the order added.
    nsDOMMediaQueryList::NotifyList notifyList;
    for (PRCList *l = PR_LIST_HEAD(&mDOMMediaQueryLists);
         l != &mDOMMediaQueryLists; l = PR_NEXT_LINK(l)) {
      nsDOMMediaQueryList *mql = static_cast<nsDOMMediaQueryList*>(l);
      mql->MediumFeaturesChanged(notifyList);
    }

    if (!notifyList.IsEmpty()) {
      nsPIDOMWindow *win = mDocument->GetInnerWindow();
      nsCOMPtr<nsIDOMEventTarget> et = do_QueryInterface(win);
      nsCxPusher pusher;

      for (PRUint32 i = 0, i_end = notifyList.Length(); i != i_end; ++i) {
        if (pusher.RePush(et)) {
          nsAutoMicroTask mt;
          nsDOMMediaQueryList::HandleChangeData &d = notifyList[i];
          d.listener->HandleChange(d.mql);
        }
      }
    }

    // NOTE:  When |notifyList| goes out of scope, our destructor could run.
  }
}

void
nsPresContext::PostMediaFeatureValuesChangedEvent()
{
  // FIXME: We should probably replace this event with use of
  // nsRefreshDriver::AddStyleFlushObserver (except the pres shell would
  // need to track whether it's been added).
  if (!mPendingMediaFeatureValuesChanged) {
    nsCOMPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(this, &nsPresContext::HandleMediaFeatureValuesChangedEvent);
    if (NS_SUCCEEDED(NS_DispatchToCurrentThread(ev))) {
      mPendingMediaFeatureValuesChanged = true;
      mDocument->SetNeedStyleFlush();
    }
  }
}

void
nsPresContext::HandleMediaFeatureValuesChangedEvent()
{
  // Null-check mShell in case the shell has been destroyed (and the
  // event is the only thing holding the pres context alive).
  if (mPendingMediaFeatureValuesChanged && mShell) {
    MediaFeatureValuesChanged(false);
  }
}

void
nsPresContext::MatchMedia(const nsAString& aMediaQueryList,
                          nsIDOMMediaQueryList** aResult)
{
  nsRefPtr<nsDOMMediaQueryList> result =
    new nsDOMMediaQueryList(this, aMediaQueryList);

  // Insert the new item at the end of the linked list.
  PR_INSERT_BEFORE(result, &mDOMMediaQueryLists);

  result.forget(aResult);
}

void
nsPresContext::SetPaginatedScrolling(bool aPaginated)
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

bool
nsPresContext::EnsureVisible()
{
  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mContainer));
  if (docShell) {
    nsCOMPtr<nsIContentViewer> cv;
    docShell->GetContentViewer(getter_AddRefs(cv));
    // Make sure this is the content viewer we belong with
    if (cv) {
      nsRefPtr<nsPresContext> currentPresContext;
      cv->GetPresContext(getter_AddRefs(currentPresContext));
      if (currentPresContext == this) {
        // OK, this is us.  We want to call Show() on the content viewer.
        nsresult result = cv->Show();
        if (NS_SUCCEEDED(result)) {
          return true;
        }
      }
    }
  }
  return false;
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

bool
nsPresContext::IsChromeSlow() const
{
  bool isChrome = false;
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
  mIsChromeIsCached = true;
  return mIsChrome;
}

void
nsPresContext::InvalidateIsChromeCacheExternal()
{
  InvalidateIsChromeCacheInternal();
}

/* virtual */ bool
nsPresContext::HasAuthorSpecifiedRules(nsIFrame *aFrame, PRUint32 ruleTypeMask) const
{
  return
    nsRuleNode::HasAuthorSpecifiedRules(aFrame->GetStyleContext(),
                                        ruleTypeMask,
                                        UseDocumentColors());
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
  bool userFontSetGottenBefore = mGetUserFontSetCalled;
#endif
  // Set mGetUserFontSetCalled up front, so that FlushUserFontSet will actually
  // flush.
  mGetUserFontSetCalled = true;
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
  if (!mShell) {
    return; // we've been torn down
  }

  if (!mGetUserFontSetCalled) {
    return; // No one cares about this font set yet, but we want to be careful
            // to not unset our mUserFontSetDirty bit, so when someone really
            // does we'll create it.
  }

  if (mUserFontSetDirty) {
    if (gfxPlatform::GetPlatform()->DownloadableFontsEnabled()) {
      nsTArray<nsFontFaceRuleContainer> rules;
      if (!mShell->StyleSet()->AppendFontFaceRules(this, rules)) {
        if (mUserFontSet) {
          mUserFontSet->Destroy();
          NS_RELEASE(mUserFontSet);
        }
        return;
      }

      bool changed = false;

      if (rules.Length() == 0) {
        if (mUserFontSet) {
          mUserFontSet->Destroy();
          NS_RELEASE(mUserFontSet);
          changed = true;
        }
      } else {
        if (!mUserFontSet) {
          mUserFontSet = new nsUserFontSet(this);
          NS_ADDREF(mUserFontSet);
        }
        changed = mUserFontSet->UpdateRules(rules);
      }

      // We need to enqueue a style change reflow (for later) to
      // reflect that we're modifying @font-face rules.  (However,
      // without a reflow, nothing will happen to start any downloads
      // that are needed.)
      if (changed) {
        UserFontSetUpdated();
      }
    }

    mUserFontSetDirty = false;
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

  mUserFontSetDirty = true;
  mDocument->SetNeedStyleFlush();

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
      mPostedFlushUserFontSet = true;
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

bool
nsPresContext::EnsureSafeToHandOutCSSRules()
{
  nsCSSStyleSheet::EnsureUniqueInnerResult res =
    mShell->StyleSet()->EnsureUniqueInnerOnCSSSheets();
  if (res == nsCSSStyleSheet::eUniqueInner_AlreadyUnique) {
    // Nothing to do.
    return true;
  }
  if (res == nsCSSStyleSheet::eUniqueInner_CloneFailed) {
    return false;
  }

  NS_ABORT_IF_FALSE(res == nsCSSStyleSheet::eUniqueInner_ClonedInner,
                    "unexpected result");
  RebuildAllStyleData(nsChangeHint(0));
  return true;
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
    bool notifyContent = mSendAfterPaintToContent;

    if (notifyContent) {
      // If the pref is set, we still don't post events when they're
      // entirely cross-doc.
      notifyContent = false;
      for (PRUint32 i = 0; i < mInvalidateRequests.mRequests.Length(); ++i) {
        if (!(mInvalidateRequests.mRequests[i].mFlags &
              nsIFrame::INVALIDATE_CROSS_DOC)) {
          notifyContent = true;
        }
      }
    }
    if (!notifyContent) {
      // Don't tell the window about this event, it should not know that
      // something happened in a subdocument. Tell only the chrome event handler.
      // (Events sent to the window get propagated to the chrome event handler
      // automatically.)
      dispatchTarget = do_QueryInterface(ourWindow->GetParentTarget());
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
  if (!event) {
    return;
  }

  // Even if we're not telling the window about the event (so eventTarget is
  // the chrome event handler, not the window), the window is still
  // logically the event target.
  event->SetTarget(eventTarget);
  event->SetTrusted(true);
  nsEventDispatcher::DispatchDOMEvent(dispatchTarget, nsnull, event, this, nsnull);
}

static bool
MayHavePaintEventListener(nsPIDOMWindow* aInnerWindow)
{
  if (!aInnerWindow)
    return false;
  if (aInnerWindow->HasPaintEventListeners())
    return true;

  nsIDOMEventTarget* parentTarget = aInnerWindow->GetParentTarget();
  if (!parentTarget)
    return false;

  nsEventListenerManager* manager = nsnull;
  if ((manager = parentTarget->GetListenerManager(false)) &&
      manager->MayHavePaintEventListener()) {
    return true;
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
    return MayHavePaintEventListener(node->OwnerDoc()->GetInnerWindow());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(parentTarget);
  if (window)
    return MayHavePaintEventListener(window);

  nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(parentTarget);
  nsIDOMEventTarget* tabChildGlobal;
  return root &&
         (tabChildGlobal = root->GetParentTarget()) &&
         (manager = tabChildGlobal->GetListenerManager(false)) &&
         manager->MayHavePaintEventListener();
}

bool
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

  nsPresContext* pc;
  for (pc = this; pc; pc = pc->GetParentPresContext()) {
    if (pc->mFireAfterPaintEvents)
      break;
    pc->mFireAfterPaintEvents = true;
  }
  if (!pc) {
    nsRootPresContext* rpc = GetRootPresContext();
    if (rpc) {
      rpc->EnsureEventualDidPaintEvent();
    }
  }

  nsInvalidateRequestList::Request* request =
    mInvalidateRequests.mRequests.AppendElement();
  if (!request)
    return;

  request->mRect = aRect;
  request->mFlags = aFlags;
}

static bool
NotifyDidPaintSubdocumentCallback(nsIDocument* aDocument, void* aData)
{
  nsIPresShell* shell = aDocument->GetShell();
  if (shell) {
    nsPresContext* pc = shell->GetPresContext();
    if (pc) {
      pc->NotifyDidPaintForSubtree();
    }
  }
  return true;
}

void
nsPresContext::NotifyDidPaintForSubtree()
{
  if (!mFireAfterPaintEvents)
    return;
  mFireAfterPaintEvents = false;

  if (IsRoot()) {
    static_cast<nsRootPresContext*>(this)->CancelDidPaintTimer();
  }

  if (!mInvalidateRequests.mRequests.IsEmpty()) {
    nsCOMPtr<nsIRunnable> ev =
      NS_NewRunnableMethod(this, &nsPresContext::FireDOMPaintEvent);
    nsContentUtils::AddScriptRunner(ev);
  }

  mDocument->EnumerateSubDocuments(NotifyDidPaintSubdocumentCallback, nsnull);
}

bool
nsPresContext::HasCachedStyleData()
{
  return mShell && mShell->StyleSet()->HasCachedStyleData();
}

static bool sGotInterruptEnv = false;
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
// GECKO_REFLOW_MIN_NOINTERRUPT_DURATION env var.  Can't be initialized here,
// because TimeDuration/TimeStamp is not safe to use in static constructors..
static TimeDuration sInterruptTimeout;

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
  int duration_ms = ev ? atoi(ev) : 100;
  sInterruptTimeout = TimeDuration::FromMilliseconds(duration_ms);
}

bool
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
        return false;
      }
      sInterruptCounter = 0;
      return true;
    default:
    case ModeEvent: {
      nsIFrame* f = PresShell()->GetRootFrame();
      if (f) {
        nsIWidget* w = f->GetNearestWidget();
        if (w) {
          return w->HasPendingInputEvent();
        }
      }
      return false;
    }
  }
}

void
nsPresContext::ReflowStarted(bool aInterruptible)
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
  mHasPendingInterrupt = false;

  mInterruptChecksToSkip = sInterruptChecksToSkip;

  if (mInterruptsEnabled) {
    mReflowStartTime = TimeStamp::Now();
  }
}

bool
nsPresContext::CheckForInterrupt(nsIFrame* aFrame)
{
  if (mHasPendingInterrupt) {
    mShell->FrameNeedsToContinueReflow(aFrame);
    return true;
  }

  if (!sGotInterruptEnv) {
    sGotInterruptEnv = true;
    GetInterruptEnv();
  }

  if (!mInterruptsEnabled) {
    return false;
  }

  if (mInterruptChecksToSkip > 0) {
    --mInterruptChecksToSkip;
    return false;
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

size_t
nsPresContext::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return mPropertyTable.SizeOfExcludingThis(aMallocSizeOf);
         mLangGroupFontPrefs.SizeOfExcludingThis(aMallocSizeOf);

  // Measurement of other members may be added later if DMD finds it is
  // worthwhile.
}

bool
nsPresContext::IsRootContentDocument()
{
  // We are a root content document if: we are not a resource doc, we are
  // not chrome, and we either have no parent or our parent is chrome.
  if (mDocument->IsResourceDoc()) {
    return false;
  }
  if (IsChrome()) {
    return false;
  }
  // We may not have a root frame, so use views.
  nsIView* view = PresShell()->GetViewManager()->GetRootView();
  if (!view) {
    return false;
  }
  view = view->GetParent(); // anonymous inner view
  if (!view) {
    return true;
  }
  view = view->GetParent(); // subdocumentframe's view
  if (!view) {
    return true;
  }

  nsIFrame* f = view->GetFrame();
  return (f && f->PresContext()->IsChrome());
}

nsRootPresContext::nsRootPresContext(nsIDocument* aDocument,
                                     nsPresContextType aType)
  : nsPresContext(aDocument, aType),
    mUpdatePluginGeometryForFrame(nsnull),
    mDOMGeneration(0),
    mNeedsToUpdatePluginGeometry(false)
{
  mRegisteredPlugins.Init();
}

nsRootPresContext::~nsRootPresContext()
{
  NS_ASSERTION(mRegisteredPlugins.Count() == 0,
               "All plugins should have been unregistered");
  CancelDidPaintTimer();
  CancelUpdatePluginGeometryTimer();
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
    nsDisplayList* aList, bool aInTransform, PluginGeometryClosure* aClosure)
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
      // Windowed plugins in transforms are always ignored, we don't
      // create configurations for them
      if (entry && (!aInTransform || f->PaintedByGecko())) {
        displayPlugin->GetWidgetConfiguration(aBuilder,
                                              aClosure->mOutputConfigurations);
        // we've dealt with this plugin now
        aClosure->mAffectedPlugins.RawRemoveEntry(entry);
      }
      break;
    }
    case nsDisplayItem::TYPE_TRANSFORM: {
      nsDisplayList* sublist =
          static_cast<nsDisplayTransform*>(i)->GetStoredList()->GetList();
      RecoverPluginGeometry(aBuilder, sublist, true, aClosure);
      break;
    }
    default: {
      nsDisplayList* sublist = i->GetList();
      if (sublist) {
        RecoverPluginGeometry(aBuilder, sublist, aInTransform, aClosure);
      }
      break;
    }
    }
  }
}

#ifdef DEBUG
#include <stdio.h>

static bool gDumpPluginList = false;
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
    		nsDisplayListBuilder::PLUGIN_GEOMETRY, false);
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

    RecoverPluginGeometry(&builder, &list, false, &closure);
    list.DeleteAll();
  }

  // Plugins that we didn't find in the display list are not visible
  closure.mAffectedPlugins.EnumerateEntries(PluginHideEnumerator, &closure);
}

static bool
HasOverlap(const nsIntPoint& aOffset1, const nsTArray<nsIntRect>& aClipRects1,
           const nsIntPoint& aOffset2, const nsTArray<nsIntRect>& aClipRects2)
{
  nsIntPoint offsetDelta = aOffset1 - aOffset2;
  for (PRUint32 i = 0; i < aClipRects1.Length(); ++i) {
    for (PRUint32 j = 0; j < aClipRects2.Length(); ++j) {
      if ((aClipRects1[i] + offsetDelta).Intersects(aClipRects2[j]))
        return true;
    }
  }
  return false;
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
      bool foundOverlap = false;
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
          foundOverlap = true;
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
  mNeedsToUpdatePluginGeometry = false;
  // Cancel out mUpdatePluginGeometryTimer so it doesn't do a random
  // update when we don't actually want one.
  CancelUpdatePluginGeometryTimer();

  nsIFrame* f = mUpdatePluginGeometryForFrame;
  if (f) {
    mUpdatePluginGeometryForFrame->PresContext()->
      SetContainsUpdatePluginGeometryFrame(false);
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

static void
UpdatePluginGeometryCallback(nsITimer *aTimer, void *aClosure)
{
  static_cast<nsRootPresContext*>(aClosure)->UpdatePluginGeometry();
}

void
nsRootPresContext::RequestUpdatePluginGeometry(nsIFrame* aFrame)
{
  if (mRegisteredPlugins.Count() == 0)
    return;

  if (!mNeedsToUpdatePluginGeometry) {
    // We'll update the plugin geometry during the next paint in this
    // presContext (either from nsPresShell::WillPaint or from
    // nsPresShell::DidPaint, depending on the platform) or on the next
    // layout flush, whichever comes first.  But we may not have anyone
    // flush layout, and paints might get optimized away if the old
    // plugin geometry covers the whole canvas, so set a backup timer to
    // do this too.  We want to make sure this won't fire before our
    // normal paint notifications, if those would update the geometry,
    // so set it for double the refresh driver interval.
    mUpdatePluginGeometryTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (mUpdatePluginGeometryTimer) {
      mUpdatePluginGeometryTimer->
        InitWithFuncCallback(UpdatePluginGeometryCallback, this,
                             nsRefreshDriver::DefaultInterval() * 2,
                             nsITimer::TYPE_ONE_SHOT);
    }
    mNeedsToUpdatePluginGeometry = true;
    mUpdatePluginGeometryForFrame = aFrame;
    mUpdatePluginGeometryForFrame->PresContext()->
      SetContainsUpdatePluginGeometryFrame(true);
  } else {
    if (!mUpdatePluginGeometryForFrame ||
        aFrame == mUpdatePluginGeometryForFrame)
      return;
    mUpdatePluginGeometryForFrame->PresContext()->
      SetContainsUpdatePluginGeometryFrame(false);
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
      SetContainsUpdatePluginGeometryFrame(false);
    mUpdatePluginGeometryForFrame = nsnull;
  }
}

void
nsRootPresContext::RootForgetUpdatePluginGeometryFrameForPresContext(
  nsPresContext* aPresContext)
{
  if (aPresContext->GetContainsUpdatePluginGeometryFrame()) {
    aPresContext->SetContainsUpdatePluginGeometryFrame(false);
    mUpdatePluginGeometryForFrame = nsnull;
  }
}

static void
NotifyDidPaintForSubtreeCallback(nsITimer *aTimer, void *aClosure)
{
  nsPresContext* presContext = (nsPresContext*)aClosure;
  nsAutoScriptBlocker blockScripts;
  presContext->NotifyDidPaintForSubtree();
}

void
nsRootPresContext::EnsureEventualDidPaintEvent()
{
  if (mNotifyDidPaintTimer)
    return;
  mNotifyDidPaintTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (!mNotifyDidPaintTimer)
    return;
  mNotifyDidPaintTimer->InitWithFuncCallback(NotifyDidPaintForSubtreeCallback,
                                             (void*)this, 100, nsITimer::TYPE_ONE_SHOT);
}

void
nsRootPresContext::AddWillPaintObserver(nsIRunnable* aRunnable)
{
  if (!mWillPaintFallbackEvent.IsPending()) {
    mWillPaintFallbackEvent = new RunWillPaintObservers(this);
    NS_DispatchToMainThread(mWillPaintFallbackEvent.get());
  }
  mWillPaintObservers.AppendElement(aRunnable);
}

/**
 * Run all runnables that need to get called before the next paint.
 */
void
nsRootPresContext::FlushWillPaintObservers()
{
  mWillPaintFallbackEvent = nsnull;
  nsTArray<nsCOMPtr<nsIRunnable> > observers;
  observers.SwapElements(mWillPaintObservers);
  for (PRUint32 i = 0; i < observers.Length(); ++i) {
    observers[i]->Run();
  }
}

size_t
nsRootPresContext::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return nsPresContext::SizeOfExcludingThis(aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mNotifyDidPaintTimer
  // - mRegisteredPlugins
  // - mWillPaintObservers
  // - mWillPaintFallbackEvent
  //
  // The following member are not measured:
  // - mUpdatePluginGeometryForFrame, because it is non-owning
}

