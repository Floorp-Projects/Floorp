/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a presentation of a document, part 1 */

#ifndef nsPresContext_h___
#define nsPresContext_h___

#include "mozilla/Attributes.h"
#include "nsColor.h"
#include "nsCoord.h"
#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsRect.h"
#include "nsDeviceContext.h"
#include "nsFont.h"
#include "nsIAtom.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsCRT.h"
#include "FramePropertyTable.h"
#include "nsGkAtoms.h"
#include "nsCycleCollectionParticipant.h"
#include "nsChangeHint.h"
#include <algorithm>
// This also pulls in gfxTypes.h, which we cannot include directly.
#include "gfxRect.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"
#include "prclist.h"
#include "nsThreadUtils.h"
#include "ScrollbarStyles.h"

#ifdef IBMBIDI
class nsBidiPresUtils;
#endif // IBMBIDI

class nsAString;
class nsIPrintSettings;
class nsIDocument;
class nsILanguageAtomService;
class nsITheme;
class nsIContent;
class nsIFrame;
class nsFrameManager;
class nsILinkHandler;
class nsIAtom;
class nsEventStateManager;
class nsICSSPseudoComparator;
struct nsStyleBackground;
struct nsStyleBorder;
class nsIRunnable;
class gfxUserFontSet;
class nsUserFontSet;
struct nsFontFaceRuleContainer;
class nsObjectFrame;
class nsTransitionManager;
class nsAnimationManager;
class nsIDOMMediaQueryList;
class nsRefreshDriver;
class nsIWidget;

namespace mozilla {
class RestyleManager;
namespace layers {
class ContainerLayer;
}
}

// supported values for cached bool types
enum nsPresContext_CachedBoolPrefType {
  kPresContext_UseDocumentColors = 1,
  kPresContext_UseDocumentFonts,
  kPresContext_UnderlineLinks
};

// supported values for cached integer pref types
enum nsPresContext_CachedIntPrefType {
  kPresContext_ScrollbarSide = 1,
  kPresContext_BidiDirection
};

// IDs for the default variable and fixed fonts (not to be changed, see nsFont.h)
// To be used for Get/SetDefaultFont(). The other IDs in nsFont.h are also supported.
const uint8_t kPresContext_DefaultVariableFont_ID = 0x00; // kGenericFont_moz_variable
const uint8_t kPresContext_DefaultFixedFont_ID    = 0x01; // kGenericFont_moz_fixed

#ifdef DEBUG
struct nsAutoLayoutPhase;

enum nsLayoutPhase {
  eLayoutPhase_Paint,
  eLayoutPhase_Reflow,
  eLayoutPhase_FrameC,
  eLayoutPhase_COUNT
};
#endif

class nsInvalidateRequestList {
public:
  struct Request {
    nsRect   mRect;
    uint32_t mFlags;
  };

  void TakeFrom(nsInvalidateRequestList* aList)
  {
    mRequests.MoveElementsFrom(aList->mRequests);
  }
  bool IsEmpty() { return mRequests.IsEmpty(); }

  nsTArray<Request> mRequests;
};

/* Used by nsPresContext::HasAuthorSpecifiedRules */
#define NS_AUTHOR_SPECIFIED_BACKGROUND      (1 << 0)
#define NS_AUTHOR_SPECIFIED_BORDER          (1 << 1)
#define NS_AUTHOR_SPECIFIED_PADDING         (1 << 2)
#define NS_AUTHOR_SPECIFIED_TEXT_SHADOW     (1 << 3)

class nsRootPresContext;

// An interface for presentation contexts. Presentation contexts are
// objects that provide an outer context for a presentation shell.

class nsPresContext : public nsIObserver {
public:
  typedef mozilla::FramePropertyTable FramePropertyTable;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
  NS_DECL_CYCLE_COLLECTION_CLASS(nsPresContext)

  enum nsPresContextType {
    eContext_Galley,       // unpaginated screen presentation
    eContext_PrintPreview, // paginated screen presentation
    eContext_Print,        // paginated printer presentation
    eContext_PageLayout    // paginated & editable.
  };

  // Policies for rebuilding style data.
  enum StyleRebuildType {
    eRebuildStyleIfNeeded,
    eAlwaysRebuildStyle
  };

  nsPresContext(nsIDocument* aDocument, nsPresContextType aType) NS_HIDDEN;

  /**
   * Initialize the presentation context from a particular device.
   */
  NS_HIDDEN_(nsresult) Init(nsDeviceContext* aDeviceContext);

  /**
   * Set the presentation shell that this context is bound to.
   * A presentation context may only be bound to a single shell.
   */
  NS_HIDDEN_(void) SetShell(nsIPresShell* aShell);


  NS_HIDDEN_(nsPresContextType) Type() const { return mType; }

  /**
   * Get the PresentationShell that this context is bound to.
   */
  nsIPresShell* PresShell() const
  {
    NS_ASSERTION(mShell, "Null pres shell");
    return mShell;
  }

  nsIPresShell* GetPresShell() const { return mShell; }

  /**
   * Returns the parent prescontext for this one. Returns null if this is a
   * root.
   */
  nsPresContext* GetParentPresContext();

  /**
   * Returns the prescontext of the toplevel content document that contains
   * this presentation, or null if there isn't one.
   */
  nsPresContext* GetToplevelContentDocumentPresContext();

  /**
   * Returns the nearest widget for the root frame of this.
   *
   * @param aOffset     If non-null the offset from the origin of the root
   *                    frame's view to the widget's origin (usually positive)
   *                    expressed in appunits of this will be returned in
   *                    aOffset.
   */
  nsIWidget* GetNearestWidget(nsPoint* aOffset = nullptr);

  /**
   * Returns the root widget for this.
   * Note that the widget is a mediater with IME.
   */
  nsIWidget* GetRootWidget();

  /**
   * Return the presentation context for the root of the view manager
   * hierarchy that contains this presentation context, or nullptr if it can't
   * be found (e.g. it's detached).
   */
  nsRootPresContext* GetRootPresContext();
  nsRootPresContext* GetDisplayRootPresContext();
  virtual bool IsRoot() { return false; }

  nsIDocument* Document() const
  {
      NS_ASSERTION(!mShell || !mShell->GetDocument() ||
                   mShell->GetDocument() == mDocument,
                   "nsPresContext doesn't have the same document as nsPresShell!");
      return mDocument;
  }

#ifdef MOZILLA_INTERNAL_API
  nsStyleSet* StyleSet() { return GetPresShell()->StyleSet(); }

  nsFrameManager* FrameManager()
    { return PresShell()->FrameManager(); }

  nsCSSFrameConstructor* FrameConstructor()
    { return PresShell()->FrameConstructor(); }

  nsTransitionManager* TransitionManager() { return mTransitionManager; }
  nsAnimationManager* AnimationManager() { return mAnimationManager; }

  nsRefreshDriver* RefreshDriver() { return mRefreshDriver; }

  mozilla::RestyleManager* RestyleManager() { return mRestyleManager; }
#endif

  /**
   * Rebuilds all style data by throwing out the old rule tree and
   * building a new one, and additionally applying aExtraHint (which
   * must not contain nsChangeHint_ReconstructFrame) to the root frame.
   * Also rebuild the user font set.
   */
  void RebuildAllStyleData(nsChangeHint aExtraHint);
  /**
   * Just like RebuildAllStyleData, except (1) asynchronous and (2) it
   * doesn't rebuild the user font set.
   */
  void PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint);

  void MediaFeatureValuesChanged(StyleRebuildType aShouldRebuild,
                                 nsChangeHint aChangeHint = nsChangeHint(0));
  void PostMediaFeatureValuesChangedEvent();
  NS_HIDDEN_(void) HandleMediaFeatureValuesChangedEvent();
  void FlushPendingMediaFeatureValuesChanged() {
    if (mPendingMediaFeatureValuesChanged)
      MediaFeatureValuesChanged(eRebuildStyleIfNeeded);
  }

  /**
   * Support for window.matchMedia()
   */
  void MatchMedia(const nsAString& aMediaQueryList,
                  nsIDOMMediaQueryList** aResult);

  /**
   * Access compatibility mode for this context.  This is the same as
   * our document's compatibility mode.
   */
  nsCompatibility CompatibilityMode() const;

  /**
   * Notify the context that the document's compatibility mode has changed
   */
  NS_HIDDEN_(void) CompatibilityModeChanged();

  /**
   * Access the image animation mode for this context
   */
  uint16_t     ImageAnimationMode() const { return mImageAnimationMode; }
  virtual NS_HIDDEN_(void) SetImageAnimationModeExternal(uint16_t aMode);
  NS_HIDDEN_(void) SetImageAnimationModeInternal(uint16_t aMode);
#ifdef MOZILLA_INTERNAL_API
  void SetImageAnimationMode(uint16_t aMode)
  { SetImageAnimationModeInternal(aMode); }
#else
  void SetImageAnimationMode(uint16_t aMode)
  { SetImageAnimationModeExternal(aMode); }
#endif

  /** 
   * Get medium of presentation
   */
  nsIAtom* Medium() {
    if (!mIsEmulatingMedia)
      return mMedium;
    return mMediaEmulated;
  }

  /*
   * Render the document as if being viewed on a device with the specified
   * media type.
   */
  void EmulateMedium(const nsAString& aMediaType);

  /*
   * Restore the viewer's natural medium
   */
  void StopEmulatingMedium();

  void* AllocateFromShell(size_t aSize)
  {
    if (mShell)
      return mShell->AllocateMisc(aSize);
    return nullptr;
  }

  void FreeToShell(size_t aSize, void* aFreeChunk)
  {
    NS_ASSERTION(mShell, "freeing after shutdown");
    if (mShell)
      mShell->FreeMisc(aSize, aFreeChunk);
  }

  /**
   * Get the default font for the given language and generic font ID.
   * If aLanguage is nullptr, the document's language is used.
   *
   * This object is read-only, you must copy the font to modify it.
   * 
   * When aFontID is kPresContext_DefaultVariableFontID or
   * kPresContext_DefaultFixedFontID (which equals
   * kGenericFont_moz_fixed, which is used for the -moz-fixed generic),
   * the nsFont returned has its name as a CSS generic family (serif or
   * sans-serif for the former, monospace for the latter), and its size
   * as the default font size for variable or fixed fonts for the
   * language group.
   *
   * For aFontID corresponding to a CSS Generic, the nsFont returned has
   * its name set to that generic font's name, and its size set to
   * the user's preference for font size for that generic and the
   * given language.
   */
  NS_HIDDEN_(const nsFont*) GetDefaultFont(uint8_t aFontID,
                                           nsIAtom *aLanguage) const;

  /** Get a cached boolean pref, by its type */
  // *  - initially created for bugs 31816, 20760, 22963
  bool GetCachedBoolPref(nsPresContext_CachedBoolPrefType aPrefType) const
  {
    // If called with a constant parameter, the compiler should optimize
    // this switch statement away.
    switch (aPrefType) {
    case kPresContext_UseDocumentFonts:
      return mUseDocumentFonts;
    case kPresContext_UseDocumentColors:
      return mUseDocumentColors;
    case kPresContext_UnderlineLinks:
      return mUnderlineLinks;
    default:
      NS_ERROR("Invalid arg passed to GetCachedBoolPref");
    }

    return false;
  }

  /** Get a cached integer pref, by its type */
  // *  - initially created for bugs 30910, 61883, 74186, 84398
  int32_t GetCachedIntPref(nsPresContext_CachedIntPrefType aPrefType) const
  {
    // If called with a constant parameter, the compiler should optimize
    // this switch statement away.
    switch (aPrefType) {
    case kPresContext_ScrollbarSide:
      return mPrefScrollbarSide;
    case kPresContext_BidiDirection:
      return mPrefBidiDirection;
    default:
      NS_ERROR("invalid arg passed to GetCachedIntPref");
    }

    return false;
  }

  /** 
   * Get the default colors
   */
  const nscolor DefaultColor() const { return mDefaultColor; }
  const nscolor DefaultBackgroundColor() const { return mBackgroundColor; }
  const nscolor DefaultLinkColor() const { return mLinkColor; }
  const nscolor DefaultActiveLinkColor() const { return mActiveLinkColor; }
  const nscolor DefaultVisitedLinkColor() const { return mVisitedLinkColor; }
  const nscolor FocusBackgroundColor() const { return mFocusBackgroundColor; }
  const nscolor FocusTextColor() const { return mFocusTextColor; }

  /**
   * Body text color, for use in quirks mode only.
   */
  const nscolor BodyTextColor() const { return mBodyTextColor; }
  void SetBodyTextColor(nscolor aColor) { mBodyTextColor = aColor; }

  bool GetUseFocusColors() const { return mUseFocusColors; }
  uint8_t FocusRingWidth() const { return mFocusRingWidth; }
  bool GetFocusRingOnAnything() const { return mFocusRingOnAnything; }
  uint8_t GetFocusRingStyle() const { return mFocusRingStyle; }

  NS_HIDDEN_(void) SetContainer(nsISupports* aContainer);

  virtual NS_HIDDEN_(already_AddRefed<nsISupports>) GetContainerExternal() const;
  NS_HIDDEN_(already_AddRefed<nsISupports>) GetContainerInternal() const;
#ifdef MOZILLA_INTERNAL_API
  already_AddRefed<nsISupports> GetContainer() const
  { return GetContainerInternal(); }
#else
  already_AddRefed<nsISupports> GetContainer() const
  { return GetContainerExternal(); }
#endif

  // XXX this are going to be replaced with set/get container
  void SetLinkHandler(nsILinkHandler* aHandler) { mLinkHandler = aHandler; }
  nsILinkHandler* GetLinkHandler() { return mLinkHandler; }

  /**
   * Get the visible area associated with this presentation context.
   * This is the size of the visible area that is used for
   * presenting the document. The returned value is in the standard
   * nscoord units (as scaled by the device context).
   */
  nsRect GetVisibleArea() { return mVisibleArea; }

  /**
   * Set the currently visible area. The units for r are standard
   * nscoord units (as scaled by the device context).
   */
  void SetVisibleArea(const nsRect& r) {
    if (!r.IsEqualEdges(mVisibleArea)) {
      mVisibleArea = r;
      // Visible area does not affect media queries when paginated.
      if (!IsPaginated() && HasCachedStyleData()) {
        mPendingViewportChange = true;
        PostMediaFeatureValuesChangedEvent();
      }
    }
  }

  /**
   * Return true if this presentation context is a paginated
   * context.
   */
  bool IsPaginated() const { return mPaginated; }
  
  /**
   * Sets whether the presentation context can scroll for a paginated
   * context.
   */
  NS_HIDDEN_(void) SetPaginatedScrolling(bool aResult);

  /**
   * Return true if this presentation context can scroll for paginated
   * context.
   */
  bool HasPaginatedScrolling() const { return mCanPaginatedScroll; }

  /**
   * Get/set the size of a page
   */
  nsSize GetPageSize() { return mPageSize; }
  void SetPageSize(nsSize aSize) { mPageSize = aSize; }

  /**
   * Get/set whether this document should be treated as having real pages
   * XXX This raises the obvious question of why a document that isn't a page
   *     is paginated; there isn't a good reason except history
   */
  bool IsRootPaginatedDocument() { return mIsRootPaginatedDocument; }
  void SetIsRootPaginatedDocument(bool aIsRootPaginatedDocument)
    { mIsRootPaginatedDocument = aIsRootPaginatedDocument; }

  /**
  * Get/set the print scaling level; used by nsPageFrame to scale up
  * pages.  Set safe to call before reflow, get guaranteed to be set
  * properly after reflow.
  */

  float GetPageScale() { return mPageScale; }
  void SetPageScale(float aScale) { mPageScale = aScale; }

  /**
  * Get/set the scaling facor to use when rendering the pages for print preview.
  * Only safe to get after print preview set up; safe to set anytime.
  * This is a scaling factor for the display of the print preview.  It
  * does not affect layout.  It only affects the size of the onscreen pages
  * in print preview.
  * XXX Temporary: see http://wiki.mozilla.org/Gecko:PrintPreview
  */
  float GetPrintPreviewScale() { return mPPScale; }
  void SetPrintPreviewScale(float aScale) { mPPScale = aScale; }

  nsDeviceContext* DeviceContext() { return mDeviceContext; }
  nsEventStateManager* EventStateManager() { return mEventManager; }
  nsIAtom* GetLanguageFromCharset() { return mLanguage; }

  float TextZoom() { return mTextZoom; }
  void SetTextZoom(float aZoom) {
    if (aZoom == mTextZoom)
      return;

    mTextZoom = aZoom;
    if (HasCachedStyleData()) {
      // Media queries could have changed, since we changed the meaning
      // of 'em' units in them.
      MediaFeatureValuesChanged(eAlwaysRebuildStyle, NS_STYLE_HINT_REFLOW);
    }
  }

  /**
   * Get the minimum font size for the specified language. If aLanguage
   * is nullptr, then the document's language is used.
   */
  int32_t MinFontSize(nsIAtom *aLanguage) const {
    const LangGroupFontPrefs *prefs = GetFontPrefsForLang(aLanguage);
    return std::max(mMinFontSize, prefs->mMinimumFontSize);
  }

  void SetMinFontSize(int32_t aMinFontSize) {
    if (aMinFontSize == mMinFontSize)
      return;

    mMinFontSize = aMinFontSize;
    if (HasCachedStyleData()) {
      // Media queries could have changed, since we changed the meaning
      // of 'em' units in them.
      MediaFeatureValuesChanged(eAlwaysRebuildStyle, NS_STYLE_HINT_REFLOW);
    }
  }

  float GetFullZoom() { return mFullZoom; }
  void SetFullZoom(float aZoom);

  nscoord GetAutoQualityMinFontSize() {
    return DevPixelsToAppUnits(mAutoQualityMinFontSizePixelsPref);
  }

  /**
   * Return the device's screen width in inches, for font size
   * inflation.
   *
   * If |aChanged| is non-null, then aChanged is filled in with whether
   * the return value has changed since either:
   *  a. the last time the function was called with non-null aChanged, or
   *  b. the first time the function was called.
   */
  float ScreenWidthInchesForFontInflation(bool* aChanged = nullptr);

  static int32_t AppUnitsPerCSSPixel() { return nsDeviceContext::AppUnitsPerCSSPixel(); }
  int32_t AppUnitsPerDevPixel() const  { return mDeviceContext->AppUnitsPerDevPixel(); }
  static int32_t AppUnitsPerCSSInch() { return nsDeviceContext::AppUnitsPerCSSInch(); }

  static nscoord CSSPixelsToAppUnits(int32_t aPixels)
  { return NSToCoordRoundWithClamp(float(aPixels) *
             float(nsDeviceContext::AppUnitsPerCSSPixel())); }

  static nscoord CSSPixelsToAppUnits(float aPixels)
  { return NSToCoordRoundWithClamp(aPixels *
             float(nsDeviceContext::AppUnitsPerCSSPixel())); }

  static int32_t AppUnitsToIntCSSPixels(nscoord aAppUnits)
  { return NSAppUnitsToIntPixels(aAppUnits,
             float(nsDeviceContext::AppUnitsPerCSSPixel())); }

  static float AppUnitsToFloatCSSPixels(nscoord aAppUnits)
  { return NSAppUnitsToFloatPixels(aAppUnits,
             float(nsDeviceContext::AppUnitsPerCSSPixel())); }

  nscoord DevPixelsToAppUnits(int32_t aPixels) const
  { return NSIntPixelsToAppUnits(aPixels,
                                 mDeviceContext->AppUnitsPerDevPixel()); }

  int32_t AppUnitsToDevPixels(nscoord aAppUnits) const
  { return NSAppUnitsToIntPixels(aAppUnits,
             float(mDeviceContext->AppUnitsPerDevPixel())); }

  int32_t CSSPixelsToDevPixels(int32_t aPixels)
  { return AppUnitsToDevPixels(CSSPixelsToAppUnits(aPixels)); }

  float CSSPixelsToDevPixels(float aPixels)
  {
    return NSAppUnitsToFloatPixels(CSSPixelsToAppUnits(aPixels),
                                   float(mDeviceContext->AppUnitsPerDevPixel()));
  }

  int32_t DevPixelsToIntCSSPixels(int32_t aPixels)
  { return AppUnitsToIntCSSPixels(DevPixelsToAppUnits(aPixels)); }

  float DevPixelsToFloatCSSPixels(int32_t aPixels)
  { return AppUnitsToFloatCSSPixels(DevPixelsToAppUnits(aPixels)); }

  // If there is a remainder, it is rounded to nearest app units.
  nscoord GfxUnitsToAppUnits(gfxFloat aGfxUnits) const
  { return mDeviceContext->GfxUnitsToAppUnits(aGfxUnits); }

  gfxFloat AppUnitsToGfxUnits(nscoord aAppUnits) const
  { return mDeviceContext->AppUnitsToGfxUnits(aAppUnits); }

  gfxRect AppUnitsToGfxUnits(const nsRect& aAppRect) const
  { return gfxRect(AppUnitsToGfxUnits(aAppRect.x),
                   AppUnitsToGfxUnits(aAppRect.y),
                   AppUnitsToGfxUnits(aAppRect.width),
                   AppUnitsToGfxUnits(aAppRect.height)); }

  static nscoord CSSTwipsToAppUnits(float aTwips)
  { return NSToCoordRoundWithClamp(
      nsDeviceContext::AppUnitsPerCSSInch() * NS_TWIPS_TO_INCHES(aTwips)); }

  // Margin-specific version, since they often need TwipsToAppUnits
  static nsMargin CSSTwipsToAppUnits(const nsIntMargin &marginInTwips)
  { return nsMargin(CSSTwipsToAppUnits(float(marginInTwips.top)),
                    CSSTwipsToAppUnits(float(marginInTwips.right)),
                    CSSTwipsToAppUnits(float(marginInTwips.bottom)),
                    CSSTwipsToAppUnits(float(marginInTwips.left))); }

  static nscoord CSSPointsToAppUnits(float aPoints)
  { return NSToCoordRound(aPoints * nsDeviceContext::AppUnitsPerCSSInch() /
                          POINTS_PER_INCH_FLOAT); }

  nscoord RoundAppUnitsToNearestDevPixels(nscoord aAppUnits) const
  { return DevPixelsToAppUnits(AppUnitsToDevPixels(aAppUnits)); }

  void SetViewportOverflowOverride(uint8_t aX, uint8_t aY)
  {
    mViewportStyleOverflow.mHorizontal = aX;
    mViewportStyleOverflow.mVertical = aY;
  }
  mozilla::ScrollbarStyles GetViewportOverflowOverride()
  {
    return mViewportStyleOverflow;
  }

  /**
   * Set and get methods for controlling the background drawing
  */
  bool GetBackgroundImageDraw() const { return mDrawImageBackground; }
  void   SetBackgroundImageDraw(bool aCanDraw)
  {
    mDrawImageBackground = aCanDraw;
  }

  bool GetBackgroundColorDraw() const { return mDrawColorBackground; }
  void   SetBackgroundColorDraw(bool aCanDraw)
  {
    mDrawColorBackground = aCanDraw;
  }
  
  /**
   * Getter and setter for OMTA time counters
   */
  bool ThrottledStyleIsUpToDate() const;
  void TickLastUpdateThrottledStyle();
  bool StyleUpdateForAllAnimationsIsUpToDate();
  void TickLastStyleUpdateForAllAnimations();

#ifdef IBMBIDI
  /**
   *  Check if bidi enabled (set depending on the presence of RTL
   *  characters or when default directionality is RTL).
   *  If enabled, we should apply the Unicode Bidi Algorithm
   *
   *  @lina 07/12/2000
   */
#ifdef MOZILLA_INTERNAL_API
  bool BidiEnabled() const { return BidiEnabledInternal(); }
#else
  bool BidiEnabled() const { return BidiEnabledExternal(); }
#endif
  virtual bool BidiEnabledExternal() const;
  bool BidiEnabledInternal() const;

  /**
   *  Set bidi enabled. This means we should apply the Unicode Bidi Algorithm
   *
   *  @lina 07/12/2000
   */
  NS_HIDDEN_(void) SetBidiEnabled() const;

  /**
   *  Set visual or implicit mode into the pres context.
   *
   *  Visual directionality is a presentation method that displays text
   *  as if it were a uni-directional, according to the primary display
   *  direction only. 
   *
   *  Implicit directionality is a presentation method in which the
   *  direction is determined by the Bidi algorithm according to the
   *  category of the characters and the category of the adjacent
   *  characters, and according to their primary direction.
   *
   *  @lina 05/02/2000
   */
  void SetVisualMode(bool aIsVisual)
  {
    mIsVisual = aIsVisual;
  }

  /**
   *  Check whether the content should be treated as visual.
   *
   *  @lina 05/02/2000
   */
  bool IsVisualMode() const { return mIsVisual; }

//Mohamed

  /**
   * Set the Bidi options for the presentation context
   */  
  NS_HIDDEN_(void) SetBidi(uint32_t aBidiOptions,
                           bool aForceRestyle = false);

  /**
   * Get the Bidi options for the presentation context
   * Not inline so consumers of nsPresContext are not forced to
   * include nsIDocument.
   */
  NS_HIDDEN_(uint32_t) GetBidi() const;
#endif // IBMBIDI

  /**
   * Render only Selection
   */
  void SetIsRenderingOnlySelection(bool aResult)
  {
    mIsRenderingOnlySelection = aResult;
  }

  bool IsRenderingOnlySelection() const { return mIsRenderingOnlySelection; }

  NS_HIDDEN_(bool) IsTopLevelWindowInactive();

  /*
   * Obtain a native them for rendering our widgets (both form controls and html)
   */
  NS_HIDDEN_(nsITheme*) GetTheme();

  /*
   * Notify the pres context that the theme has changed.  An internal switch
   * means it's one of our Mozilla themes that changed (e.g., Modern to Classic).
   * Otherwise, the OS is telling us that the native theme for the platform
   * has changed.
   */
  NS_HIDDEN_(void) ThemeChanged();

  /*
   * Notify the pres context that the resolution of the user interface has
   * changed. This happens if a window is moved between HiDPI and non-HiDPI
   * displays, so that the ratio of points to device pixels changes.
   */
  NS_HIDDEN_(void) UIResolutionChanged();

  /*
   * Notify the pres context that a system color has changed
   */
  NS_HIDDEN_(void) SysColorChanged();

  /** Printing methods below should only be used for Medium() == print **/
  NS_HIDDEN_(void) SetPrintSettings(nsIPrintSettings *aPrintSettings);

  nsIPrintSettings* GetPrintSettings() { return mPrintSettings; }

  /* Accessor for table of frame properties */
  FramePropertyTable* PropertyTable() { return &mPropertyTable; }

  /* Helper function that ensures that this prescontext is shown in its
     docshell if it's the most recent prescontext for the docshell.  Returns
     whether the prescontext is now being shown.
  */
  NS_HIDDEN_(bool) EnsureVisible();
  
#ifdef MOZ_REFLOW_PERF
  NS_HIDDEN_(void) CountReflows(const char * aName,
                                nsIFrame * aFrame);
#endif

  /**
   * This table maps border-width enums 'thin', 'medium', 'thick'
   * to actual nscoord values.
   */
  const nscoord* GetBorderWidthTable() { return mBorderWidthTable; }

  bool IsDynamic() { return (mType == eContext_PageLayout || mType == eContext_Galley); }
  bool IsScreen() { return (mMedium == nsGkAtoms::screen ||
                              mType == eContext_PageLayout ||
                              mType == eContext_PrintPreview); }

  // Is this presentation in a chrome docshell?
  bool IsChrome() const
  {
    return mIsChromeIsCached ? mIsChrome : IsChromeSlow();
  }

  virtual void InvalidateIsChromeCacheExternal();
  void InvalidateIsChromeCacheInternal() { mIsChromeIsCached = false; }
#ifdef MOZILLA_INTERNAL_API
  void InvalidateIsChromeCache()
  { InvalidateIsChromeCacheInternal(); }
#else
  void InvalidateIsChromeCache()
  { InvalidateIsChromeCacheExternal(); }
#endif

  // Public API for native theme code to get style internals.
  virtual bool HasAuthorSpecifiedRules(nsIFrame *aFrame, uint32_t ruleTypeMask) const;

  // Is it OK to let the page specify colors and backgrounds?
  bool UseDocumentColors() const {
    return GetCachedBoolPref(kPresContext_UseDocumentColors) || IsChrome();
  }

  // Explicitly enable and disable paint flashing.
  void SetPaintFlashing(bool aPaintFlashing) {
    mPaintFlashing = aPaintFlashing;
    mPaintFlashingInitialized = true;
  }

  // This method should be used instead of directly accessing mPaintFlashing,
  // as that value may be out of date when mPaintFlashingInitialized is false.
  bool GetPaintFlashing() const;

  bool             SupressingResizeReflow() const { return mSupressResizeReflow; }
  
  virtual NS_HIDDEN_(gfxUserFontSet*) GetUserFontSetExternal();
  NS_HIDDEN_(gfxUserFontSet*) GetUserFontSetInternal();
#ifdef MOZILLA_INTERNAL_API
  gfxUserFontSet* GetUserFontSet() { return GetUserFontSetInternal(); }
#else
  gfxUserFontSet* GetUserFontSet() { return GetUserFontSetExternal(); }
#endif

  void FlushUserFontSet();
  void RebuildUserFontSet(); // asynchronously

  // Should be called whenever the set of fonts available in the user
  // font set changes (e.g., because a new font loads, or because the
  // user font set is changed and fonts become unavailable).
  void UserFontSetUpdated();

  // Ensure that it is safe to hand out CSS rules outside the layout
  // engine by ensuring that all CSS style sheets have unique inners
  // and, if necessary, synchronously rebuilding all style data.
  // Returns true on success and false on failure (not safe).
  bool EnsureSafeToHandOutCSSRules();

  void NotifyInvalidation(uint32_t aFlags);
  void NotifyInvalidation(const nsRect& aRect, uint32_t aFlags);
  // aRect is in device pixels
  void NotifyInvalidation(const nsIntRect& aRect, uint32_t aFlags);
  // aFlags are nsIPresShell::PAINT_ flags
  void NotifyDidPaintForSubtree(uint32_t aFlags);
  void FireDOMPaintEvent(nsInvalidateRequestList* aList);

  // Callback for catching invalidations in ContainerLayers
  // Passed to LayerProperties::ComputeDifference
  static void NotifySubDocInvalidation(mozilla::layers::ContainerLayer* aContainer,
                                       const nsIntRegion& aRegion);
  void SetNotifySubDocInvalidationData(mozilla::layers::ContainerLayer* aContainer);
  static void ClearNotifySubDocInvalidationData(mozilla::layers::ContainerLayer* aContainer);
  bool IsDOMPaintEventPending();
  void ClearMozAfterPaintEvents() {
    mInvalidateRequestsSinceLastPaint.mRequests.Clear();
    mUndeliveredInvalidateRequestsBeforeLastPaint.mRequests.Clear();
    mAllInvalidated = false;
  }

  bool IsProcessingRestyles() const {
    return mProcessingRestyles;
  }

  void SetProcessingRestyles(bool aProcessing) {
    NS_ASSERTION(aProcessing != bool(mProcessingRestyles),
                 "should never nest");
    mProcessingRestyles = aProcessing;
  }

  bool IsProcessingAnimationStyleChange() const {
    return mProcessingAnimationStyleChange;
  }

  void SetProcessingAnimationStyleChange(bool aProcessing) {
    NS_ASSERTION(aProcessing != bool(mProcessingAnimationStyleChange),
                 "should never nest");
    mProcessingAnimationStyleChange = aProcessing;
  }

  /**
   * Notify the prescontext that the presshell is about to reflow a reflow root.
   * The single argument indicates whether this reflow should be interruptible.
   * If aInterruptible is false then CheckForInterrupt and HasPendingInterrupt
   * will always return false. If aInterruptible is true then CheckForInterrupt
   * will return true when a pending event is detected.  This is for use by the
   * presshell only.  Reflow code wanting to prevent interrupts should use
   * InterruptPreventer.
   */
  void ReflowStarted(bool aInterruptible);

  /**
   * A class that can be used to temporarily disable reflow interruption.
   */
  class InterruptPreventer;
  friend class InterruptPreventer;
  class MOZ_STACK_CLASS InterruptPreventer {
  public:
    InterruptPreventer(nsPresContext* aCtx) :
      mCtx(aCtx),
      mInterruptsEnabled(aCtx->mInterruptsEnabled),
      mHasPendingInterrupt(aCtx->mHasPendingInterrupt)
    {
      mCtx->mInterruptsEnabled = false;
      mCtx->mHasPendingInterrupt = false;
    }
    ~InterruptPreventer() {
      mCtx->mInterruptsEnabled = mInterruptsEnabled;
      mCtx->mHasPendingInterrupt = mHasPendingInterrupt;
    }

  private:
    nsPresContext* mCtx;
    bool mInterruptsEnabled;
    bool mHasPendingInterrupt;
  };
    
  /**
   * Check for interrupts. This may return true if a pending event is
   * detected. Once it has returned true, it will keep returning true
   * until ReflowStarted is called. In all cases where this returns true,
   * the passed-in frame (which should be the frame whose reflow will be
   * interrupted if true is returned) will be passed to
   * nsIPresShell::FrameNeedsToContinueReflow.
   */
  bool CheckForInterrupt(nsIFrame* aFrame);
  /**
   * Returns true if CheckForInterrupt has returned true since the last
   * ReflowStarted call. Cannot itself trigger an interrupt check.
   */
  bool HasPendingInterrupt() { return mHasPendingInterrupt; }

  /**
   * If we have a presshell, and if the given content's current
   * document is the same as our presshell's document, return the
   * content's primary frame.  Otherwise, return null.  Only use this
   * if you care about which presshell the primary frame is in.
   */
  nsIFrame* GetPrimaryFrameFor(nsIContent* aContent);

  void NotifyDestroyingFrame(nsIFrame* aFrame)
  {
    PropertyTable()->DeleteAllFor(aFrame);
  }

  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  bool IsRootContentDocument();
  bool IsCrossProcessRootContentDocument();

  bool IsGlyph() const {
    return mIsGlyph;
  }

  void SetIsGlyph(bool aValue) {
    mIsGlyph = aValue;
  }

  bool UsesRootEMUnits() const {
    return mUsesRootEMUnits;
  }

  void SetUsesRootEMUnits(bool aValue) {
    mUsesRootEMUnits = aValue;
  }

  bool UsesViewportUnits() const {
    return mUsesViewportUnits;
  }

  void SetUsesViewportUnits(bool aValue) {
    mUsesViewportUnits = aValue;
  }

  // true if there are OMTA transition updates for the current document which
  // have been throttled, and therefore some style information may not be up
  // to date
  bool ExistThrottledUpdates() const {
    return mExistThrottledUpdates;
  }

  void SetExistThrottledUpdates(bool aExistThrottledUpdates) {
    mExistThrottledUpdates = aExistThrottledUpdates;
  }

protected:
  friend class nsRunnableMethod<nsPresContext>;
  NS_HIDDEN_(void) ThemeChangedInternal();
  NS_HIDDEN_(void) SysColorChangedInternal();
  NS_HIDDEN_(void) UIResolutionChangedInternal();

  static NS_HIDDEN_(bool)
  UIResolutionChangedSubdocumentCallback(nsIDocument* aDocument, void* aData);

  NS_HIDDEN_(void) SetImgAnimations(nsIContent *aParent, uint16_t aMode);
  NS_HIDDEN_(void) SetSMILAnimations(nsIDocument *aDoc, uint16_t aNewMode,
                                     uint16_t aOldMode);
  NS_HIDDEN_(void) GetDocumentColorPreferences();

  NS_HIDDEN_(void) PreferenceChanged(const char* aPrefName);
  static NS_HIDDEN_(int) PrefChangedCallback(const char*, void*);

  NS_HIDDEN_(void) UpdateAfterPreferencesChanged();
  static NS_HIDDEN_(void) PrefChangedUpdateTimerCallback(nsITimer *aTimer, void *aClosure);

  NS_HIDDEN_(void) GetUserPreferences();

  // Allow nsAutoPtr<LangGroupFontPrefs> dtor to access this protected struct's
  // dtor:
  struct LangGroupFontPrefs;
  friend class nsAutoPtr<LangGroupFontPrefs>;
  struct LangGroupFontPrefs {
    // Font sizes default to zero; they will be set in GetFontPreferences
    LangGroupFontPrefs()
      : mLangGroup(nullptr)
      , mMinimumFontSize(0)
      , mDefaultVariableFont("serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
                             NS_FONT_WEIGHT_NORMAL, NS_FONT_STRETCH_NORMAL, 0, 0)
      , mDefaultFixedFont("monospace", NS_FONT_STYLE_NORMAL,
                          NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL,
                          NS_FONT_STRETCH_NORMAL, 0, 0)
      , mDefaultSerifFont("serif", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
                        NS_FONT_WEIGHT_NORMAL, NS_FONT_STRETCH_NORMAL, 0, 0)
      , mDefaultSansSerifFont("sans-serif", NS_FONT_STYLE_NORMAL,
                              NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL,
                              NS_FONT_STRETCH_NORMAL, 0, 0)
      , mDefaultMonospaceFont("monospace", NS_FONT_STYLE_NORMAL,
                              NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL,
                              NS_FONT_STRETCH_NORMAL, 0, 0)
      , mDefaultCursiveFont("cursive", NS_FONT_STYLE_NORMAL,
                            NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL,
                            NS_FONT_STRETCH_NORMAL, 0, 0)
      , mDefaultFantasyFont("fantasy", NS_FONT_STYLE_NORMAL,
                            NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL,
                            NS_FONT_STRETCH_NORMAL, 0, 0)
    {}

    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
      size_t n = 0;
      LangGroupFontPrefs *curr = mNext;
      while (curr) {
        n += aMallocSizeOf(curr);

        // Measurement of the following members may be added later if DMD finds
        // it is worthwhile:
        // - mLangGroup
        // - mDefault*Font

        curr = curr->mNext;
      }
      return n;
    }

    nsCOMPtr<nsIAtom> mLangGroup;
    nscoord mMinimumFontSize;
    nsFont mDefaultVariableFont;
    nsFont mDefaultFixedFont;
    nsFont mDefaultSerifFont;
    nsFont mDefaultSansSerifFont;
    nsFont mDefaultMonospaceFont;
    nsFont mDefaultCursiveFont;
    nsFont mDefaultFantasyFont;
    nsAutoPtr<LangGroupFontPrefs> mNext;
  };

  /**
   * Fetch the user's font preferences for the given aLanguage's
   * langugage group.
   */
  const LangGroupFontPrefs* GetFontPrefsForLang(nsIAtom *aLanguage) const;

  void ResetCachedFontPrefs() {
    // Throw away any other LangGroupFontPrefs objects:
    mLangGroupFontPrefs.mNext = nullptr;

    // Make GetFontPreferences reinitialize mLangGroupFontPrefs:
    mLangGroupFontPrefs.mLangGroup = nullptr;
  }

  NS_HIDDEN_(void) UpdateCharSet(const nsCString& aCharSet);

public:
  void DoChangeCharSet(const nsCString& aCharSet);

  /**
   * Checks for MozAfterPaint listeners on the document
   */
  bool MayHavePaintEventListener();

  /**
   * Checks for MozAfterPaint listeners on the document and 
   * any subdocuments, except for subdocuments that are non-top-level
   * content documents.
   */
  bool MayHavePaintEventListenerInSubDocument();

protected:
  void InvalidateThebesLayers();
  void AppUnitsPerDevPixelChanged();

  void HandleRebuildUserFontSet() {
    mPostedFlushUserFontSet = false;
    FlushUserFontSet();
  }

  bool HavePendingInputEvent();

  // Can't be inline because we can't include nsStyleSet.h.
  bool HasCachedStyleData();

  bool IsChromeSlow() const;

  // IMPORTANT: The ownership implicit in the following member variables
  // has been explicitly checked.  If you add any members to this class,
  // please make the ownership explicit (pinkerton, scc).

  nsPresContextType     mType;
  nsIPresShell*         mShell;         // [WEAK]
  nsCOMPtr<nsIDocument> mDocument;
  nsRefPtr<nsDeviceContext> mDeviceContext; // [STRONG] could be weak, but
                                            // better safe than sorry.
                                            // Cannot reintroduce cycles
                                            // since there is no dependency
                                            // from gfx back to layout.
  nsRefPtr<nsEventStateManager> mEventManager;
  nsRefPtr<nsRefreshDriver> mRefreshDriver;
  nsRefPtr<nsTransitionManager> mTransitionManager;
  nsRefPtr<nsAnimationManager> mAnimationManager;
  nsRefPtr<mozilla::RestyleManager> mRestyleManager;
  nsIAtom*              mMedium;        // initialized by subclass ctors;
                                        // weak pointer to static atom
  nsCOMPtr<nsIAtom> mMediaEmulated;

  nsILinkHandler*       mLinkHandler;   // [WEAK]

  // Formerly mLangGroup; moving from charset-oriented langGroup to
  // maintaining actual language settings everywhere (see bug 524107).
  // This may in fact hold a langGroup such as x-western rather than
  // a specific language, however (e.g, if it is inferred from the
  // charset rather than explicitly specified as a lang attribute).
  nsCOMPtr<nsIAtom>     mLanguage;

public:
  // The following are public member variables so that we can use them
  // with mozilla::AutoToggle or mozilla::AutoRestore.

  // Should we disable font size inflation because we're inside of
  // shrink-wrapping calculations on an inflation container?
  bool                  mInflationDisabledForShrinkWrap;

protected:

  nsWeakPtr             mContainer;

  PRCList               mDOMMediaQueryLists;

  int32_t               mMinFontSize;   // Min font size, defaults to 0
  float                 mTextZoom;      // Text zoom, defaults to 1.0
  float                 mFullZoom;      // Page zoom, defaults to 1.0

  float                 mLastFontInflationScreenWidth;

  int32_t               mCurAppUnitsPerDevPixel;
  int32_t               mAutoQualityMinFontSizePixelsPref;

  nsCOMPtr<nsITheme> mTheme;
  nsCOMPtr<nsILanguageAtomService> mLangService;
  nsCOMPtr<nsIPrintSettings> mPrintSettings;
  nsCOMPtr<nsITimer>    mPrefChangedTimer;

  FramePropertyTable    mPropertyTable;

  nsInvalidateRequestList mInvalidateRequestsSinceLastPaint;
  nsInvalidateRequestList mUndeliveredInvalidateRequestsBeforeLastPaint;

  // container for per-context fonts (downloadable, SVG, etc.)
  nsUserFontSet*        mUserFontSet;

  nsRect                mVisibleArea;
  nsSize                mPageSize;
  float                 mPageScale;
  float                 mPPScale;

  nscolor               mDefaultColor;
  nscolor               mBackgroundColor;

  nscolor               mLinkColor;
  nscolor               mActiveLinkColor;
  nscolor               mVisitedLinkColor;

  nscolor               mFocusBackgroundColor;
  nscolor               mFocusTextColor;

  nscolor               mBodyTextColor;

  mozilla::ScrollbarStyles mViewportStyleOverflow;
  uint8_t               mFocusRingWidth;

  bool mExistThrottledUpdates;

  uint16_t              mImageAnimationMode;
  uint16_t              mImageAnimationModePref;

  LangGroupFontPrefs    mLangGroupFontPrefs;

  nscoord               mBorderWidthTable[3];

  uint32_t              mInterruptChecksToSkip;

  mozilla::TimeStamp    mReflowStartTime;

  // last time animations/transition styles were flushed to their primary frames
  mozilla::TimeStamp    mLastUpdateThrottledStyle;
  // last time we did a full style flush
  mozilla::TimeStamp    mLastStyleUpdateForAllAnimations;

  unsigned              mHasPendingInterrupt : 1;
  unsigned              mInterruptsEnabled : 1;
  unsigned              mUseDocumentFonts : 1;
  unsigned              mUseDocumentColors : 1;
  unsigned              mUnderlineLinks : 1;
  unsigned              mSendAfterPaintToContent : 1;
  unsigned              mUseFocusColors : 1;
  unsigned              mFocusRingOnAnything : 1;
  unsigned              mFocusRingStyle : 1;
  unsigned              mDrawImageBackground : 1;
  unsigned              mDrawColorBackground : 1;
  unsigned              mNeverAnimate : 1;
  unsigned              mIsRenderingOnlySelection : 1;
  unsigned              mPaginated : 1;
  unsigned              mCanPaginatedScroll : 1;
  unsigned              mDoScaledTwips : 1;
  unsigned              mEnableJapaneseTransform : 1;
  unsigned              mIsRootPaginatedDocument : 1;
  unsigned              mPrefBidiDirection : 1;
  unsigned              mPrefScrollbarSide : 2;
  unsigned              mPendingSysColorChanged : 1;
  unsigned              mPendingThemeChanged : 1;
  unsigned              mPendingUIResolutionChanged : 1;
  unsigned              mPendingMediaFeatureValuesChanged : 1;
  unsigned              mPrefChangePendingNeedsReflow : 1;
  unsigned              mIsEmulatingMedia : 1;
  // True if the requests in mInvalidateRequestsSinceLastPaint cover the
  // entire viewport
  unsigned              mAllInvalidated : 1;

  // Are we currently drawing an SVG glyph?
  unsigned              mIsGlyph : 1;

  // Does the associated document use root-em (rem) units?
  unsigned              mUsesRootEMUnits : 1;
  // Does the associated document use viewport units (vw/vh/vmin/vmax)?
  unsigned              mUsesViewportUnits : 1;

  // Has there been a change to the viewport's dimensions?
  unsigned              mPendingViewportChange : 1;

  // Is the current mUserFontSet valid?
  unsigned              mUserFontSetDirty : 1;
  // Has GetUserFontSet() been called?
  unsigned              mGetUserFontSetCalled : 1;
  // Do we currently have an event posted to call FlushUserFontSet?
  unsigned              mPostedFlushUserFontSet : 1;

  // resize reflow is suppressed when the only change has been to zoom
  // the document rather than to change the document's dimensions
  unsigned              mSupressResizeReflow : 1;

  unsigned              mIsVisual : 1;

  unsigned              mProcessingRestyles : 1;
  unsigned              mProcessingAnimationStyleChange : 1;

  unsigned              mFireAfterPaintEvents : 1;

  // Cache whether we are chrome or not because it is expensive.  
  // mIsChromeIsCached tells us if mIsChrome is valid or we need to get the
  // value the slow way.
  mutable unsigned      mIsChromeIsCached : 1;
  mutable unsigned      mIsChrome : 1;

  // Should we paint flash in this context? Do not use this variable directly.
  // Use GetPaintFlashing() method instead.
  mutable unsigned mPaintFlashing : 1;
  mutable unsigned mPaintFlashingInitialized : 1;

#ifdef DEBUG
  bool                  mInitialized;
#endif


protected:

  virtual ~nsPresContext() NS_HIDDEN;

  // these are private, use the list in nsFont.h if you want a public list
  enum {
    eDefaultFont_Variable,
    eDefaultFont_Fixed,
    eDefaultFont_Serif,
    eDefaultFont_SansSerif,
    eDefaultFont_Monospace,
    eDefaultFont_Cursive,
    eDefaultFont_Fantasy,
    eDefaultFont_COUNT
  };

  nscolor MakeColorPref(const nsString& aColor);

  void LastRelease();

#ifdef DEBUG
private:
  friend struct nsAutoLayoutPhase;
  uint32_t mLayoutPhaseCount[eLayoutPhase_COUNT];
public:
  uint32_t LayoutPhaseCount(nsLayoutPhase aPhase) {
    return mLayoutPhaseCount[aPhase];
  }
#endif

};

class nsRootPresContext : public nsPresContext {
public:
  nsRootPresContext(nsIDocument* aDocument, nsPresContextType aType) NS_HIDDEN;
  virtual ~nsRootPresContext();

  /**
   * Ensure that NotifyDidPaintForSubtree is eventually called on this
   * object after a timeout.
   */
  void EnsureEventualDidPaintEvent();

  void CancelDidPaintTimer()
  {
    if (mNotifyDidPaintTimer) {
      mNotifyDidPaintTimer->Cancel();
      mNotifyDidPaintTimer = nullptr;
    }
  }

  /**
   * Registers a plugin to receive geometry updates (position and clip
   * region) so it can update its widget.
   * Callers must call UnregisterPluginForGeometryUpdates before
   * the aPlugin frame is destroyed.
   */
  void RegisterPluginForGeometryUpdates(nsIContent* aPlugin);
  /**
   * Stops a plugin receiving geometry updates (position and clip
   * region). If the plugin was not already registered, this does
   * nothing.
   */
  void UnregisterPluginForGeometryUpdates(nsIContent* aPlugin);

  bool NeedToComputePluginGeometryUpdates()
  {
    return mRegisteredPlugins.Count() > 0;
  }
  /**
   * Compute geometry updates for each plugin given that aList is the display
   * list for aFrame. The updates are not yet applied;
   * ApplyPluginGeometryUpdates is responsible for that. In the meantime they
   * are stored on each nsObjectFrame.
   * This needs to be called even when aFrame is a popup, since although
   * windowed plugins aren't allowed in popups, windowless plugins are
   * and ComputePluginGeometryUpdates needs to be called for them.
   */
  void ComputePluginGeometryUpdates(nsIFrame* aFrame,
                                    nsDisplayListBuilder* aBuilder,
                                    nsDisplayList* aList);

  /**
   * Apply the stored plugin geometry updates. This should normally be called
   * in DidPaint so the plugins are moved/clipped immediately after we've
   * updated our window, so they look in sync with our window.
   */
  void ApplyPluginGeometryUpdates();

  virtual bool IsRoot() MOZ_OVERRIDE { return true; }

  /**
   * Increment DOM-modification generation counter to indicate that
   * the DOM has changed in a way that might lead to style changes/
   * reflows/frame creation and destruction.
   */
  void IncrementDOMGeneration() { mDOMGeneration++; }

  /**
   * Get the current DOM generation counter.
   *
   * See nsFrameManagerBase::GetGlobalGenerationNumber() for a
   * global generation number.
   */
  uint32_t GetDOMGeneration() { return mDOMGeneration; }

  /**
   * Add a runnable that will get called before the next paint. They will get
   * run eventually even if painting doesn't happen. They might run well before
   * painting happens.
   */
  void AddWillPaintObserver(nsIRunnable* aRunnable);

  /**
   * Run all runnables that need to get called before the next paint.
   */
  void FlushWillPaintObservers();

  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const MOZ_OVERRIDE;

protected:
  /**
   * Start a timer to ensure we eventually run ApplyPluginGeometryUpdates.
   */
  void InitApplyPluginGeometryTimer();
  /**
   * Cancel the timer that ensures we eventually run ApplyPluginGeometryUpdates.
   */
  void CancelApplyPluginGeometryTimer();

  class RunWillPaintObservers : public nsRunnable {
  public:
    RunWillPaintObservers(nsRootPresContext* aPresContext) : mPresContext(aPresContext) {}
    void Revoke() { mPresContext = nullptr; }
    NS_IMETHOD Run() MOZ_OVERRIDE
    {
      if (mPresContext) {
        mPresContext->FlushWillPaintObservers();
      }
      return NS_OK;
    }
    nsRootPresContext* mPresContext;
  };

  friend class nsPresContext;

  nsCOMPtr<nsITimer> mNotifyDidPaintTimer;
  nsCOMPtr<nsITimer> mApplyPluginGeometryTimer;
  nsTHashtable<nsRefPtrHashKey<nsIContent> > mRegisteredPlugins;
  nsTArray<nsCOMPtr<nsIRunnable> > mWillPaintObservers;
  nsRevocableEventPtr<RunWillPaintObservers> mWillPaintFallbackEvent;
  uint32_t mDOMGeneration;
};

#ifdef MOZ_REFLOW_PERF

#define DO_GLOBAL_REFLOW_COUNT(_name) \
  aPresContext->CountReflows((_name), (nsIFrame*)this); 
#else
#define DO_GLOBAL_REFLOW_COUNT(_name)
#endif // MOZ_REFLOW_PERF

#endif /* nsPresContext_h___ */
