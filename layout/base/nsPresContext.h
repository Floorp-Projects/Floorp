/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a presentation of a document, part 1 */

#ifndef nsPresContext_h___
#define nsPresContext_h___

#include "mozilla/Attributes.h"
#include "mozilla/WeakPtr.h"
#include "nsColor.h"
#include "nsCoord.h"
#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsRect.h"
#include "nsFont.h"
#include "gfxFontConstants.h"
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
#include "mozilla/AppUnits.h"
#include "prclist.h"
#include "nsThreadUtils.h"
#include "ScrollbarStyles.h"
#include "nsIMessageManager.h"
#include "mozilla/RestyleLogging.h"

class nsAString;
class nsIPrintSettings;
class nsDocShell;
class nsIDocShell;
class nsIDocument;
class nsILanguageAtomService;
class nsITheme;
class nsIContent;
class nsIFrame;
class nsFrameManager;
class nsILinkHandler;
class nsIAtom;
class nsIRunnable;
class gfxUserFontEntry;
class gfxUserFontSet;
class gfxTextPerfMetrics;
class nsPluginFrame;
class nsTransitionManager;
class nsAnimationManager;
class nsRefreshDriver;
class nsIWidget;
class nsDeviceContext;
class gfxMissingFontRecorder;

namespace mozilla {
class EventStateManager;
class RestyleManager;
class CounterStyleManager;
namespace layers {
class ContainerLayer;
class LayerManager;
} // namespace layers
} // namespace mozilla

// supported values for cached bool types
enum nsPresContext_CachedBoolPrefType {
  kPresContext_UseDocumentFonts = 1,
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
  typedef mozilla::ScrollbarStyles ScrollbarStyles;

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

  nsPresContext(nsIDocument* aDocument, nsPresContextType aType);

  /**
   * Initialize the presentation context from a particular device.
   */
  nsresult Init(nsDeviceContext* aDeviceContext);

  /**
   * Set the presentation shell that this context is bound to.
   * A presentation context may only be bound to a single shell.
   */
  void SetShell(nsIPresShell* aShell);


  nsPresContextType Type() const { return mType; }

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

  mozilla::CounterStyleManager* CounterStyleManager() {
    return mCounterStyleManager;
  }
#endif

  /**
   * Rebuilds all style data by throwing out the old rule tree and
   * building a new one, and additionally applying aExtraHint (which
   * must not contain nsChangeHint_ReconstructFrame) to the root frame.
   * For aRestyleHint, see RestyleManager::RebuildAllStyleData.
   * Also rebuild the user font set and counter style manager.
   */
  void RebuildAllStyleData(nsChangeHint aExtraHint, nsRestyleHint aRestyleHint);
  /**
   * Just like RebuildAllStyleData, except (1) asynchronous and (2) it
   * doesn't rebuild the user font set.
   */
  void PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint,
                                    nsRestyleHint aRestyleHint);

  /**
   * Handle changes in the values of media features (used in media
   * queries).
   *
   * There are three sensible values to use for aRestyleHint:
   *  * nsRestyleHint(0) to rebuild style data, with rerunning of
   *    selector matching, only if media features have changed
   *  * eRestyle_ForceDescendants to force rebuilding of style data (but
   *    still only rerun selector matching if media query results have
   *    changed).  (RebuildAllStyleData always adds
   *    eRestyle_ForceDescendants internally, so here we're only using
   *    it to distinguish from nsRestyleHint(0) whether we need to call
   *    RebuildAllStyleData at all.)
   *  * eRestyle_Subtree to force rebuilding of style data with
   *    rerunning of selector matching
   *
   * For aChangeHint, see RestyleManager::RebuildAllStyleData.  (Passing
   * a nonzero aChangeHint forces rebuilding style data even if
   * nsRestyleHint(0) is passed.)
   */
  void MediaFeatureValuesChanged(nsRestyleHint aRestyleHint,
                                 nsChangeHint aChangeHint = nsChangeHint(0));
  void PostMediaFeatureValuesChangedEvent();
  void HandleMediaFeatureValuesChangedEvent();
  void FlushPendingMediaFeatureValuesChanged() {
    if (mPendingMediaFeatureValuesChanged)
      MediaFeatureValuesChanged(nsRestyleHint(0));
  }

  /**
   * Access compatibility mode for this context.  This is the same as
   * our document's compatibility mode.
   */
  nsCompatibility CompatibilityMode() const;

  /**
   * Notify the context that the document's compatibility mode has changed
   */
  void CompatibilityModeChanged();

  /**
   * Access the image animation mode for this context
   */
  uint16_t     ImageAnimationMode() const { return mImageAnimationMode; }
  virtual void SetImageAnimationModeExternal(uint16_t aMode);
  void SetImageAnimationModeInternal(uint16_t aMode);
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
  const nsFont* GetDefaultFont(uint8_t aFontID,
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

  void SetContainer(nsIDocShell* aContainer);

  virtual nsISupports* GetContainerWeakExternal() const;
  nsISupports* GetContainerWeakInternal() const;
#ifdef MOZILLA_INTERNAL_API
  nsISupports* GetContainerWeak() const
  { return GetContainerWeakInternal(); }
#else
  nsISupports* GetContainerWeak() const
  { return GetContainerWeakExternal(); }
#endif

  nsIDocShell* GetDocShell() const;

  // XXX this are going to be replaced with set/get container
  void SetLinkHandler(nsILinkHandler* aHandler) { mLinkHandler = aHandler; }
  nsILinkHandler* GetLinkHandler() { return mLinkHandler; }

  /**
   * Detach this pres context - i.e. cancel relevant timers,
   * SetLinkHandler(null), SetContainer(null) etc.
   * Only to be used by the DocumentViewer.
   */
  virtual void Detach();

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
  void SetPaginatedScrolling(bool aResult);

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
  mozilla::EventStateManager* EventStateManager() { return mEventManager; }
  nsIAtom* GetLanguageFromCharset() { return mLanguage; }

  float TextZoom() { return mTextZoom; }
  void SetTextZoom(float aZoom) {
    if (aZoom == mTextZoom)
      return;

    mTextZoom = aZoom;
    if (HasCachedStyleData()) {
      // Media queries could have changed, since we changed the meaning
      // of 'em' units in them.
      MediaFeatureValuesChanged(eRestyle_ForceDescendants,
                                NS_STYLE_HINT_REFLOW);
    }
  }

  /**
   * Get the minimum font size for the specified language. If aLanguage
   * is nullptr, then the document's language is used.  This combines
   * the language-specific global preference with the per-presentation
   * base minimum font size.
   */
  int32_t MinFontSize(nsIAtom *aLanguage) const {
    const LangGroupFontPrefs *prefs = GetFontPrefsForLang(aLanguage);
    return std::max(mBaseMinFontSize, prefs->mMinimumFontSize);
  }

  /**
   * Get the per-presentation base minimum font size.  This size is
   * independent of the language-specific global preference.
   */
  int32_t BaseMinFontSize() const {
    return mBaseMinFontSize;
  }

  /**
   * Set the per-presentation base minimum font size.  This size is
   * independent of the language-specific global preference.
   */
  void SetBaseMinFontSize(int32_t aMinFontSize) {
    if (aMinFontSize == mBaseMinFontSize)
      return;

    mBaseMinFontSize = aMinFontSize;
    if (HasCachedStyleData()) {
      // Media queries could have changed, since we changed the meaning
      // of 'em' units in them.
      MediaFeatureValuesChanged(eRestyle_ForceDescendants,
                                NS_STYLE_HINT_REFLOW);
    }
  }

  float GetFullZoom() { return mFullZoom; }
  void SetFullZoom(float aZoom);

  nscoord GetAutoQualityMinFontSize() {
    return DevPixelsToAppUnits(mAutoQualityMinFontSizePixelsPref);
  }

  /**
   * Return the device's screen size in inches, for font size
   * inflation.
   *
   * If |aChanged| is non-null, then aChanged is filled in with whether
   * the screen size value has changed since either:
   *  a. the last time the function was called with non-null aChanged, or
   *  b. the first time the function was called.
   */
  gfxSize ScreenSizeInchesForFontInflation(bool* aChanged = nullptr);

  static int32_t AppUnitsPerCSSPixel() { return mozilla::AppUnitsPerCSSPixel(); }
  int32_t AppUnitsPerDevPixel() const;
  static int32_t AppUnitsPerCSSInch() { return mozilla::AppUnitsPerCSSInch(); }

  static nscoord CSSPixelsToAppUnits(int32_t aPixels)
  { return NSToCoordRoundWithClamp(float(aPixels) *
             float(AppUnitsPerCSSPixel())); }

  static nscoord CSSPixelsToAppUnits(float aPixels)
  { return NSToCoordRoundWithClamp(aPixels *
             float(AppUnitsPerCSSPixel())); }

  static int32_t AppUnitsToIntCSSPixels(nscoord aAppUnits)
  { return NSAppUnitsToIntPixels(aAppUnits,
             float(AppUnitsPerCSSPixel())); }

  static float AppUnitsToFloatCSSPixels(nscoord aAppUnits)
  { return NSAppUnitsToFloatPixels(aAppUnits,
             float(AppUnitsPerCSSPixel())); }

  nscoord DevPixelsToAppUnits(int32_t aPixels) const
  { return NSIntPixelsToAppUnits(aPixels, AppUnitsPerDevPixel()); }

  int32_t AppUnitsToDevPixels(nscoord aAppUnits) const
  { return NSAppUnitsToIntPixels(aAppUnits,
             float(AppUnitsPerDevPixel())); }

  int32_t CSSPixelsToDevPixels(int32_t aPixels)
  { return AppUnitsToDevPixels(CSSPixelsToAppUnits(aPixels)); }

  float CSSPixelsToDevPixels(float aPixels)
  {
    return NSAppUnitsToFloatPixels(CSSPixelsToAppUnits(aPixels),
                                   float(AppUnitsPerDevPixel()));
  }

  int32_t DevPixelsToIntCSSPixels(int32_t aPixels)
  { return AppUnitsToIntCSSPixels(DevPixelsToAppUnits(aPixels)); }

  float DevPixelsToFloatCSSPixels(int32_t aPixels)
  { return AppUnitsToFloatCSSPixels(DevPixelsToAppUnits(aPixels)); }

  // If there is a remainder, it is rounded to nearest app units.
  nscoord GfxUnitsToAppUnits(gfxFloat aGfxUnits) const;

  gfxFloat AppUnitsToGfxUnits(nscoord aAppUnits) const;

  gfxRect AppUnitsToGfxUnits(const nsRect& aAppRect) const
  { return gfxRect(AppUnitsToGfxUnits(aAppRect.x),
                   AppUnitsToGfxUnits(aAppRect.y),
                   AppUnitsToGfxUnits(aAppRect.width),
                   AppUnitsToGfxUnits(aAppRect.height)); }

  static nscoord CSSTwipsToAppUnits(float aTwips)
  { return NSToCoordRoundWithClamp(
      mozilla::AppUnitsPerCSSInch() * NS_TWIPS_TO_INCHES(aTwips)); }

  // Margin-specific version, since they often need TwipsToAppUnits
  static nsMargin CSSTwipsToAppUnits(const nsIntMargin &marginInTwips)
  { return nsMargin(CSSTwipsToAppUnits(float(marginInTwips.top)),
                    CSSTwipsToAppUnits(float(marginInTwips.right)),
                    CSSTwipsToAppUnits(float(marginInTwips.bottom)),
                    CSSTwipsToAppUnits(float(marginInTwips.left))); }

  static nscoord CSSPointsToAppUnits(float aPoints)
  { return NSToCoordRound(aPoints * mozilla::AppUnitsPerCSSInch() /
                          POINTS_PER_INCH_FLOAT); }

  nscoord RoundAppUnitsToNearestDevPixels(nscoord aAppUnits) const
  { return DevPixelsToAppUnits(AppUnitsToDevPixels(aAppUnits)); }

  void SetViewportScrollbarStylesOverride(const ScrollbarStyles& aScrollbarStyle)
  {
    mViewportStyleScrollbar = aScrollbarStyle;
  }
  ScrollbarStyles GetViewportScrollbarStylesOverride()
  {
    return mViewportStyleScrollbar;
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
   * Getter and setters for OMTA time counters
   */
  bool StyleUpdateForAllAnimationsIsUpToDate() const;
  void TickLastStyleUpdateForAllAnimations();
  void ClearLastStyleUpdateForAllAnimations();

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
  void SetBidiEnabled() const;

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
  void SetBidi(uint32_t aBidiOptions,
                           bool aForceRestyle = false);

  /**
   * Get the Bidi options for the presentation context
   * Not inline so consumers of nsPresContext are not forced to
   * include nsIDocument.
   */
  uint32_t GetBidi() const;

  /**
   * Render only Selection
   */
  void SetIsRenderingOnlySelection(bool aResult)
  {
    mIsRenderingOnlySelection = aResult;
  }

  bool IsRenderingOnlySelection() const { return mIsRenderingOnlySelection; }

  bool IsTopLevelWindowInactive();

  /*
   * Obtain a native them for rendering our widgets (both form controls and html)
   */
  nsITheme* GetTheme();

  /*
   * Notify the pres context that the theme has changed.  An internal switch
   * means it's one of our Mozilla themes that changed (e.g., Modern to Classic).
   * Otherwise, the OS is telling us that the native theme for the platform
   * has changed.
   */
  void ThemeChanged();

  /*
   * Notify the pres context that the resolution of the user interface has
   * changed. This happens if a window is moved between HiDPI and non-HiDPI
   * displays, so that the ratio of points to device pixels changes.
   * The notification happens asynchronously.
   */
  void UIResolutionChanged();

 /*
  * Like UIResolutionChanged() but invalidates values immediately.
  */
  void UIResolutionChangedSync();

  /*
   * Notify the pres context that a system color has changed
   */
  void SysColorChanged();

  /** Printing methods below should only be used for Medium() == print **/
  void SetPrintSettings(nsIPrintSettings *aPrintSettings);

  nsIPrintSettings* GetPrintSettings() { return mPrintSettings; }

  /* Accessor for table of frame properties */
  FramePropertyTable* PropertyTable() { return &mPropertyTable; }

  /* Helper function that ensures that this prescontext is shown in its
     docshell if it's the most recent prescontext for the docshell.  Returns
     whether the prescontext is now being shown.
  */
  bool EnsureVisible();

#ifdef MOZ_REFLOW_PERF
  void CountReflows(const char * aName,
                                nsIFrame * aFrame);
#endif

  void ConstructedFrame() {
    ++mFramesConstructed;
  }
  void ReflowedFrame() {
    ++mFramesReflowed;
  }

  uint64_t FramesConstructedCount() {
    return mFramesConstructed;
  }
  uint64_t FramesReflowedCount() {
    return mFramesReflowed;
  }

  /**
   * This table maps border-width enums 'thin', 'medium', 'thick'
   * to actual nscoord values.
   */
  const nscoord* GetBorderWidthTable() { return mBorderWidthTable; }

  gfxTextPerfMetrics *GetTextPerfMetrics() { return mTextPerf; }

  bool IsDynamic() { return (mType == eContext_PageLayout || mType == eContext_Galley); }
  bool IsScreen() { return (mMedium == nsGkAtoms::screen ||
                              mType == eContext_PageLayout ||
                              mType == eContext_PrintPreview); }

  // Is this presentation in a chrome docshell?
  bool IsChrome() const { return mIsChrome; }
  bool IsChromeOriginImage() const { return mIsChromeOriginImage; }
  void UpdateIsChrome();

  // Public API for native theme code to get style internals.
  virtual bool HasAuthorSpecifiedRules(nsIFrame *aFrame, uint32_t ruleTypeMask) const;

  // Is it OK to let the page specify colors and backgrounds?
  bool UseDocumentColors() const {
    MOZ_ASSERT(mUseDocumentColors || !(IsChrome() || IsChromeOriginImage()),
               "We should never have a chrome doc or image that can't use its colors.");
    return mUseDocumentColors;
  }

  // Explicitly enable and disable paint flashing.
  void SetPaintFlashing(bool aPaintFlashing) {
    mPaintFlashing = aPaintFlashing;
    mPaintFlashingInitialized = true;
  }

  // This method should be used instead of directly accessing mPaintFlashing,
  // as that value may be out of date when mPaintFlashingInitialized is false.
  bool GetPaintFlashing() const;

  bool             SuppressingResizeReflow() const { return mSuppressResizeReflow; }

  gfxUserFontSet* GetUserFontSet();

  // Should be called whenever the set of fonts available in the user
  // font set changes (e.g., because a new font loads, or because the
  // user font set is changed and fonts become unavailable).
  void UserFontSetUpdated(gfxUserFontEntry* aUpdatedFont = nullptr);

  gfxMissingFontRecorder *MissingFontRecorder() { return mMissingFonts; }
  void NotifyMissingFonts();

  void FlushCounterStyles();
  void RebuildCounterStyles(); // asynchronously

  // Ensure that it is safe to hand out CSS rules outside the layout
  // engine by ensuring that all CSS style sheets have unique inners
  // and, if necessary, synchronously rebuilding all style data.
  void EnsureSafeToHandOutCSSRules();

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

  /**
   * Returns whether there are any pending restyles or reflows.
   */
  bool HasPendingRestyleOrReflow();

  /**
   * Informs the document's FontFaceSet that the refresh driver ticked,
   * flushing style and layout.
   */
  void NotifyFontFaceSetOnRefresh();

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
    explicit InterruptPreventer(nsPresContext* aCtx) :
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

  bool UsesExChUnits() const {
    return mUsesExChUnits;
  }

  void SetUsesExChUnits(bool aValue) {
    mUsesExChUnits = aValue;
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

  bool IsDeviceSizePageSize();

  bool HasWarnedAboutPositionedTableParts() const {
    return mHasWarnedAboutPositionedTableParts;
  }

  void SetHasWarnedAboutPositionedTableParts() {
    mHasWarnedAboutPositionedTableParts = true;
  }

protected:
  friend class nsRunnableMethod<nsPresContext>;
  void ThemeChangedInternal();
  void SysColorChangedInternal();
  void UIResolutionChangedInternal();

  static bool
  UIResolutionChangedSubdocumentCallback(nsIDocument* aDocument, void* aData);

  void SetImgAnimations(nsIContent *aParent, uint16_t aMode);
  void SetSMILAnimations(nsIDocument *aDoc, uint16_t aNewMode,
                                     uint16_t aOldMode);
  void GetDocumentColorPreferences();

  void PreferenceChanged(const char* aPrefName);
  static void PrefChangedCallback(const char*, void*);

  void UpdateAfterPreferencesChanged();
  static void PrefChangedUpdateTimerCallback(nsITimer *aTimer, void *aClosure);

  void GetUserPreferences();

  // Allow nsAutoPtr<LangGroupFontPrefs> dtor to access this protected struct's
  // dtor:
  struct LangGroupFontPrefs;
  friend class nsAutoPtr<LangGroupFontPrefs>;
  struct LangGroupFontPrefs {
    // Font sizes default to zero; they will be set in GetFontPreferences
    LangGroupFontPrefs()
      : mLangGroup(nullptr)
      , mMinimumFontSize(0)
      , mDefaultVariableFont(mozilla::eFamily_serif, NS_FONT_STYLE_NORMAL,
                             NS_FONT_WEIGHT_NORMAL,
                             NS_FONT_STRETCH_NORMAL, 0, 0)
      , mDefaultFixedFont(mozilla::eFamily_monospace, NS_FONT_STYLE_NORMAL,
                          NS_FONT_WEIGHT_NORMAL,
                          NS_FONT_STRETCH_NORMAL, 0, 0)
      , mDefaultSerifFont(mozilla::eFamily_serif, NS_FONT_STYLE_NORMAL,
                          NS_FONT_WEIGHT_NORMAL,
                          NS_FONT_STRETCH_NORMAL, 0, 0)
      , mDefaultSansSerifFont(mozilla::eFamily_sans_serif,
                              NS_FONT_STYLE_NORMAL,
                              NS_FONT_WEIGHT_NORMAL,
                              NS_FONT_STRETCH_NORMAL, 0, 0)
      , mDefaultMonospaceFont(mozilla::eFamily_monospace, NS_FONT_STYLE_NORMAL,
                              NS_FONT_WEIGHT_NORMAL,
                              NS_FONT_STRETCH_NORMAL, 0, 0)
      , mDefaultCursiveFont(mozilla::eFamily_cursive, NS_FONT_STYLE_NORMAL,
                            NS_FONT_WEIGHT_NORMAL,
                            NS_FONT_STRETCH_NORMAL, 0, 0)
      , mDefaultFantasyFont(mozilla::eFamily_fantasy, NS_FONT_STYLE_NORMAL,
                            NS_FONT_WEIGHT_NORMAL,
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

  void UpdateCharSet(const nsCString& aCharSet);

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

#ifdef RESTYLE_LOGGING
  // Controls for whether debug information about restyling in this
  // document should be output.
  bool RestyleLoggingEnabled() const { return mRestyleLoggingEnabled; }
  void StartRestyleLogging() { mRestyleLoggingEnabled = true; }
  void StopRestyleLogging() { mRestyleLoggingEnabled = false; }
#endif

  void InvalidatePaintedLayers();

protected:
  // May be called multiple times (unlink, destructor)
  void Destroy();

  void AppUnitsPerDevPixelChanged();

  void HandleRebuildCounterStyles() {
    mPostedFlushCounterStyles = false;
    FlushCounterStyles();
  }

  bool HavePendingInputEvent();

  // Can't be inline because we can't include nsStyleSet.h.
  bool HasCachedStyleData();

  bool IsChromeSlow() const;

  // IMPORTANT: The ownership implicit in the following member variables
  // has been explicitly checked.  If you add any members to this class,
  // please make the ownership explicit (pinkerton, scc).

  nsPresContextType     mType;
  // the nsPresShell owns a strong reference to the nsPresContext, and is responsible
  // for nulling this pointer before it is destroyed
  nsIPresShell* MOZ_NON_OWNING_REF mShell;         // [WEAK]
  nsCOMPtr<nsIDocument> mDocument;
  nsRefPtr<nsDeviceContext> mDeviceContext; // [STRONG] could be weak, but
                                            // better safe than sorry.
                                            // Cannot reintroduce cycles
                                            // since there is no dependency
                                            // from gfx back to layout.
  nsRefPtr<mozilla::EventStateManager> mEventManager;
  nsRefPtr<nsRefreshDriver> mRefreshDriver;
  nsRefPtr<nsTransitionManager> mTransitionManager;
  nsRefPtr<nsAnimationManager> mAnimationManager;
  nsRefPtr<mozilla::RestyleManager> mRestyleManager;
  nsRefPtr<mozilla::CounterStyleManager> mCounterStyleManager;
  nsIAtom* MOZ_UNSAFE_REF("always a static atom") mMedium; // initialized by subclass ctors
  nsCOMPtr<nsIAtom> mMediaEmulated;

  // This pointer is nulled out through SetLinkHandler() in the destructors of
  // the classes which set it. (using SetLinkHandler() again).
  nsILinkHandler* MOZ_NON_OWNING_REF mLinkHandler;

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

  mozilla::WeakPtr<nsDocShell>             mContainer;

  // Base minimum font size, independent of the language-specific global preference. Defaults to 0
  int32_t               mBaseMinFontSize;
  float                 mTextZoom;      // Text zoom, defaults to 1.0
  float                 mFullZoom;      // Page zoom, defaults to 1.0

  gfxSize               mLastFontInflationScreenSize;

  int32_t               mCurAppUnitsPerDevPixel;
  int32_t               mAutoQualityMinFontSizePixelsPref;

  nsCOMPtr<nsITheme> mTheme;
  nsCOMPtr<nsILanguageAtomService> mLangService;
  nsCOMPtr<nsIPrintSettings> mPrintSettings;
  nsCOMPtr<nsITimer>    mPrefChangedTimer;

  FramePropertyTable    mPropertyTable;

  nsInvalidateRequestList mInvalidateRequestsSinceLastPaint;
  nsInvalidateRequestList mUndeliveredInvalidateRequestsBeforeLastPaint;

  // text performance metrics
  nsAutoPtr<gfxTextPerfMetrics>   mTextPerf;

  nsAutoPtr<gfxMissingFontRecorder> mMissingFonts;

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

  ScrollbarStyles       mViewportStyleScrollbar;
  uint8_t               mFocusRingWidth;

  bool mExistThrottledUpdates;

  uint16_t              mImageAnimationMode;
  uint16_t              mImageAnimationModePref;

  LangGroupFontPrefs    mLangGroupFontPrefs;

  nscoord               mBorderWidthTable[3];

  uint32_t              mInterruptChecksToSkip;

  // Counters for tests and tools that want to detect frame construction
  // or reflow.
  uint64_t              mFramesConstructed;
  uint64_t              mFramesReflowed;

  mozilla::TimeStamp    mReflowStartTime;

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
  // Does the associated document use ex or ch units?
  unsigned              mUsesExChUnits : 1;
  // Does the associated document use viewport units (vw/vh/vmin/vmax)?
  unsigned              mUsesViewportUnits : 1;

  // Has there been a change to the viewport's dimensions?
  unsigned              mPendingViewportChange : 1;

  // Is the current mCounterStyleManager valid?
  unsigned              mCounterStylesDirty : 1;
  // Do we currently have an event posted to call FlushCounterStyles?
  unsigned              mPostedFlushCounterStyles: 1;

  // resize reflow is suppressed when the only change has been to zoom
  // the document rather than to change the document's dimensions
  unsigned              mSuppressResizeReflow : 1;

  unsigned              mIsVisual : 1;

  unsigned              mFireAfterPaintEvents : 1;

  unsigned              mIsChrome : 1;
  unsigned              mIsChromeOriginImage : 1;

  // Should we paint flash in this context? Do not use this variable directly.
  // Use GetPaintFlashing() method instead.
  mutable unsigned mPaintFlashing : 1;
  mutable unsigned mPaintFlashingInitialized : 1;

  unsigned mHasWarnedAboutPositionedTableParts : 1;

  // Have we added quirk.css to the style set?
  unsigned              mQuirkSheetAdded : 1;

#ifdef RESTYLE_LOGGING
  // Should we output debug information about restyling for this document?
  bool                  mRestyleLoggingEnabled;
#endif

#ifdef DEBUG
  bool                  mInitialized;
#endif


protected:

  virtual ~nsPresContext();

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

class nsRootPresContext final : public nsPresContext {
public:
  nsRootPresContext(nsIDocument* aDocument, nsPresContextType aType);
  virtual ~nsRootPresContext();
  virtual void Detach() override;

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
   * are stored on each nsPluginFrame.
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

  /**
   * Transfer stored plugin geometry updates to the compositor. Called during
   * reflow, data is shipped over with layer updates. e10s specific.
   */
  void CollectPluginGeometryUpdates(mozilla::layers::LayerManager* aLayerManager);

  virtual bool IsRoot() override { return true; }

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

  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

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
    explicit RunWillPaintObservers(nsRootPresContext* aPresContext) : mPresContext(aPresContext) {}
    void Revoke() { mPresContext = nullptr; }
    NS_IMETHOD Run() override
    {
      if (mPresContext) {
        mPresContext->FlushWillPaintObservers();
      }
      return NS_OK;
    }
    // The lifetime of this reference is handled by an nsRevocableEventPtr
    nsRootPresContext* MOZ_NON_OWNING_REF mPresContext;
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
