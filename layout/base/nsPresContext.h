/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a presentation of a document, part 1 */

#ifndef nsPresContext_h___
#define nsPresContext_h___

#include "mozilla/Attributes.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/NotNull.h"
#include "mozilla/ScrollStyles.h"
#include "mozilla/PreferenceSheet.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "nsColor.h"
#include "nsCoord.h"
#include "nsCOMPtr.h"
#include "nsRect.h"
#include "nsStringFwd.h"
#include "gfxFontConstants.h"
#include "nsAtom.h"
#include "nsCRT.h"
#include "nsIWidgetListener.h"  // for nsSizeMode
#include "nsGkAtoms.h"
#include "nsCycleCollectionParticipant.h"
#include "nsChangeHint.h"
#include <algorithm>
#include "gfxTypes.h"
#include "gfxRect.h"
#include "nsTArray.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/AppUnits.h"
#include "mozilla/MediaEmulationData.h"
#include "mozilla/PresShellForwards.h"
#include "prclist.h"
#include "nsThreadUtils.h"
#include "Units.h"
#include "prenv.h"

class nsBidi;
class nsIPrintSettings;
class nsDocShell;
class nsIDocShell;
class nsITheme;
class nsITimer;
class nsIContent;
class nsIFrame;
class nsFrameManager;
class nsAtom;
class nsIRunnable;
class gfxFontFeatureValueSet;
class gfxUserFontEntry;
class gfxUserFontSet;
class gfxTextPerfMetrics;
class nsCSSFontFeatureValuesRule;
class nsCSSFrameConstructor;
class nsDisplayList;
class nsDisplayListBuilder;
class nsPluginFrame;
class nsTransitionManager;
class nsAnimationManager;
class nsRefreshDriver;
class nsIWidget;
class nsDeviceContext;
class gfxMissingFontRecorder;
struct FontMatchingStats;

namespace mozilla {
class AnimationEventDispatcher;
class EffectCompositor;
class Encoding;
class EventStateManager;
class CounterStyleManager;
class PresShell;
class RestyleManager;
class StaticPresData;
struct MediaFeatureChange;
namespace layers {
class ContainerLayer;
class LayerManager;
}  // namespace layers
namespace dom {
class Document;
class Element;
}  // namespace dom
}  // namespace mozilla

// supported values for cached integer pref types
enum nsPresContext_CachedIntPrefType {
  kPresContext_ScrollbarSide = 1,
  kPresContext_BidiDirection
};

// IDs for the default variable and fixed fonts (not to be changed, see
// nsFont.h) To be used for Get/SetDefaultFont(). The other IDs in nsFont.h are
// also supported.
//
// kGenericFont_moz_variable
const uint8_t kPresContext_DefaultVariableFont_ID = 0x00;
// kGenericFont_moz_fixed
const uint8_t kPresContext_DefaultFixedFont_ID = 0x01;

#ifdef DEBUG
struct nsAutoLayoutPhase;

enum class nsLayoutPhase : uint8_t {
  Paint,
  DisplayListBuilding,  // sometimes a subset of the paint phase
  Reflow,
  FrameC,
  COUNT
};
#endif

/* Used by nsPresContext::HasAuthorSpecifiedRules */
#define NS_AUTHOR_SPECIFIED_BORDER_OR_BACKGROUND (1 << 0)
#define NS_AUTHOR_SPECIFIED_PADDING (1 << 1)

class nsRootPresContext;

// An interface for presentation contexts. Presentation contexts are
// objects that provide an outer context for a presentation shell.

class nsPresContext : public nsISupports,
                      public mozilla::SupportsWeakPtr<nsPresContext> {
 public:
  using Encoding = mozilla::Encoding;
  template <typename T>
  using NotNull = mozilla::NotNull<T>;
  using MediaEmulationData = mozilla::MediaEmulationData;
  using StylePrefersColorScheme = mozilla::StylePrefersColorScheme;

  typedef mozilla::ScrollStyles ScrollStyles;
  using TransactionId = mozilla::layers::TransactionId;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS_FINAL
  NS_DECL_CYCLE_COLLECTION_CLASS(nsPresContext)
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(nsPresContext)

  enum nsPresContextType {
    eContext_Galley,        // unpaginated screen presentation
    eContext_PrintPreview,  // paginated screen presentation
    eContext_Print,         // paginated printer presentation
    eContext_PageLayout     // paginated & editable.
  };

  nsPresContext(mozilla::dom::Document* aDocument, nsPresContextType aType);

  /**
   * Initialize the presentation context from a particular device.
   */
  nsresult Init(nsDeviceContext* aDeviceContext);

  /**
   * Set and detach presentation shell that this context is bound to.
   * A presentation context may only be bound to a single shell.
   */
  void AttachPresShell(mozilla::PresShell* aPresShell);
  void DetachPresShell();

  nsPresContextType Type() const { return mType; }

  /**
   * Get the PresentationShell that this context is bound to.
   */
  mozilla::PresShell* PresShell() const {
    NS_ASSERTION(mPresShell, "Null pres shell");
    return mPresShell;
  }

  mozilla::PresShell* GetPresShell() const { return mPresShell; }

  void DocumentCharSetChanged(NotNull<const Encoding*> aCharSet);

  /**
   * Returns the parent prescontext for this one. Returns null if this is a
   * root.
   */
  nsPresContext* GetParentPresContext() const;

  /**
   * Returns the prescontext of the toplevel content document that contains
   * this presentation, or null if there isn't one.
   */
  nsPresContext* GetToplevelContentDocumentPresContext();

  /**
   * Returns the nearest widget for the root frame or view of this.
   *
   * @param aOffset     If non-null the offset from the origin of the root
   *                    frame's view to the widget's origin (usually positive)
   *                    expressed in appunits of this will be returned in
   *                    aOffset.
   */
  nsIWidget* GetNearestWidget(nsPoint* aOffset = nullptr);

  /**
   * Returns the root widget for this.
   */
  nsIWidget* GetRootWidget() const;

  /**
   * Returns the widget which may have native focus and handles text input
   * like keyboard input, IME, etc.
   */
  nsIWidget* GetTextInputHandlingWidget() const {
    // Currently, root widget for each PresContext handles text input.
    return GetRootWidget();
  }

  /**
   * Return the presentation context for the root of the view manager
   * hierarchy that contains this presentation context, or nullptr if it can't
   * be found (e.g. it's detached).
   */
  nsRootPresContext* GetRootPresContext() const;

  virtual bool IsRoot() { return false; }

  mozilla::dom::Document* Document() const {
#ifdef DEBUG
    ValidatePresShellAndDocumentReleation();
#endif  // #ifdef DEBUG
    return mDocument;
  }

  inline mozilla::ServoStyleSet* StyleSet() const;

  bool HasPendingMediaQueryUpdates() const {
    return !!mPendingMediaFeatureValuesChange;
  }

  inline nsCSSFrameConstructor* FrameConstructor();

  mozilla::AnimationEventDispatcher* AnimationEventDispatcher() {
    return mAnimationEventDispatcher;
  }

  mozilla::EffectCompositor* EffectCompositor() { return mEffectCompositor; }
  nsTransitionManager* TransitionManager() { return mTransitionManager.get(); }
  nsAnimationManager* AnimationManager() { return mAnimationManager.get(); }
  const nsAnimationManager* AnimationManager() const {
    return mAnimationManager.get();
  }

  nsRefreshDriver* RefreshDriver() { return mRefreshDriver; }

  mozilla::RestyleManager* RestyleManager() {
    MOZ_ASSERT(mRestyleManager);
    return mRestyleManager.get();
  }

  mozilla::CounterStyleManager* CounterStyleManager() const {
    return mCounterStyleManager;
  }

  /**
   * Rebuilds all style data by throwing out the old rule tree and
   * building a new one, and additionally applying a change hint (which must not
   * contain nsChangeHint_ReconstructFrame) to the root frame.
   *
   * For the restyle hint argument, see RestyleManager::RebuildAllStyleData.
   * Also rebuild the user font set and counter style manager.
   *
   * FIXME(emilio): The name of this is an utter lie. We should probably call
   * this PostGlobalStyleChange or something, as it doesn't really rebuild
   * anything unless you tell it to via the change hint / restyle hint
   * machinery.
   */
  void RebuildAllStyleData(nsChangeHint, const mozilla::RestyleHint&);
  /**
   * Just like RebuildAllStyleData, except (1) asynchronous and (2) it
   * doesn't rebuild the user font set / counter-style manager / etc.
   */
  void PostRebuildAllStyleDataEvent(nsChangeHint, const mozilla::RestyleHint&);

  void ContentLanguageChanged();

  /**
   * Handle changes in the values of media features (used in media queries).
   */
  void MediaFeatureValuesChanged(const mozilla::MediaFeatureChange& aChange);

  void FlushPendingMediaFeatureValuesChanged();

  /**
   * Calls MediaFeatureValuesChanged for this pres context and all descendant
   * subdocuments that have a pres context. This should be used for media
   * features that must be updated in all subdocuments e.g. display-mode.
   */
  void MediaFeatureValuesChangedAllDocuments(
      const mozilla::MediaFeatureChange&);

  /**
   * Updates the size mode on all remote children and recursively notifies this
   * document and all subdocuments (including remote children) that a media
   * feature value has changed.
   */
  void SizeModeChanged(nsSizeMode aSizeMode);

  /**
   * Access compatibility mode for this context.  This is the same as
   * our document's compatibility mode.
   */
  nsCompatibility CompatibilityMode() const;

  /**
   * Access the image animation mode for this context
   */
  uint16_t ImageAnimationMode() const { return mImageAnimationMode; }
  void SetImageAnimationMode(uint16_t aMode);

  /**
   * Get medium of presentation
   */
  const nsAtom* Medium() {
    MOZ_ASSERT(mMedium);
    return mMediaEmulationData.mMedium ? mMediaEmulationData.mMedium.get()
                                       : mMedium;
  }

  /*
   * Render the document as if being viewed on a device with the specified
   * media type.
   *
   * If passed null, it stops emulating.
   */
  void EmulateMedium(nsAtom* aMediaType);

  /** Get a cached integer pref, by its type */
  // *  - initially created for bugs 30910, 61883, 74186, 84398
  int32_t GetCachedIntPref(nsPresContext_CachedIntPrefType aPrefType) const {
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

  const mozilla::PreferenceSheet::Prefs& PrefSheetPrefs() const {
    return mozilla::PreferenceSheet::PrefsFor(*mDocument);
  }
  nscolor DefaultBackgroundColor() const {
    return PrefSheetPrefs().mDefaultBackgroundColor;
  }

  nsISupports* GetContainerWeak() const;

  nsDocShell* GetDocShell() const;

  /**
   * Get the visible area associated with this presentation context.
   * This is the size of the visible area that is used for
   * presenting the document. The returned value is in the standard
   * nscoord units (as scaled by the device context).
   */
  nsRect GetVisibleArea() const { return mVisibleArea; }

  /**
   * Set the currently visible area. The units for r are standard
   * nscoord units (as scaled by the device context).
   */
  void SetVisibleArea(const nsRect& r);

  nsSize GetSizeForViewportUnits() const { return mSizeForViewportUnits; }

  /**
   * Set the maximum height of the dynamic toolbar in nscoord units.
   */
  MOZ_CAN_RUN_SCRIPT
  void SetDynamicToolbarMaxHeight(mozilla::ScreenIntCoord aHeight);

  mozilla::ScreenIntCoord GetDynamicToolbarMaxHeight() const {
    MOZ_ASSERT(IsRootContentDocumentCrossProcess());
    return mDynamicToolbarMaxHeight;
  }

  /**
   * Returns true if we are using the dynamic toolbar.
   */
  bool HasDynamicToolbar() const {
    MOZ_ASSERT(IsRootContentDocumentCrossProcess());
    return mDynamicToolbarMaxHeight > 0;
  }

  /*
   * |aOffset| must be offset from the bottom edge of the ICB and it's negative.
   */
  void UpdateDynamicToolbarOffset(mozilla::ScreenIntCoord aOffset);
  mozilla::ScreenIntCoord GetDynamicToolbarHeight() const {
    MOZ_ASSERT(IsRootContentDocumentCrossProcess());
    return mDynamicToolbarHeight;
  }

  /**
   * Returns the state of the dynamic toolbar.
   */
  mozilla::DynamicToolbarState GetDynamicToolbarState() const;

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
  void SetIsRootPaginatedDocument(bool aIsRootPaginatedDocument) {
    mIsRootPaginatedDocument = aIsRootPaginatedDocument;
  }

  /**
   * Get/set the print scaling level; used by nsPageFrame to scale up
   * pages.  Set safe to call before reflow, get guaranteed to be set
   * properly after reflow.
   */

  float GetPageScale() { return mPageScale; }
  void SetPageScale(float aScale) { mPageScale = aScale; }

  /**
   * Get/set the scaling facor to use when rendering the pages for print
   * preview. Only safe to get after print preview set up; safe to set anytime.
   * This is a scaling factor for the display of the print preview.  It
   * does not affect layout.  It only affects the size of the onscreen pages
   * in print preview.
   * XXX Temporary: see http://wiki.mozilla.org/Gecko:PrintPreview
   */
  float GetPrintPreviewScale() { return mPPScale; }
  void SetPrintPreviewScale(float aScale) { mPPScale = aScale; }

  nsDeviceContext* DeviceContext() const { return mDeviceContext; }
  mozilla::EventStateManager* EventStateManager() { return mEventManager; }

  /**
   * Get/set a text zoom factor that is applied on top of the normal text zoom
   * set by the front-end/user.
   */
  float GetSystemFontScale() const { return mSystemFontScale; }
  void SetSystemFontScale(float aFontScale) {
    MOZ_ASSERT(aFontScale > 0.0f, "invalid font scale");
    if (aFontScale == mSystemFontScale || IsPrintingOrPrintPreview()) {
      return;
    }

    mSystemFontScale = aFontScale;
    UpdateEffectiveTextZoom();
  }

  /**
   * Get/set the text zoom factor in use.
   * This value should be used if you're interested in the pure text zoom value
   * controlled by the front-end, e.g. when transferring zoom levels to a new
   * document.
   * Code that wants to use this value for layouting and rendering purposes
   * should consider using EffectiveTextZoom() instead, so as to take the system
   * font scale into account as well.
   */
  float TextZoom() const { return mTextZoom; }
  void SetTextZoom(float aZoom) {
    MOZ_ASSERT(aZoom > 0.0f, "invalid zoom factor");
    if (aZoom == mTextZoom) return;

    mTextZoom = aZoom;
    UpdateEffectiveTextZoom();
  }

  /**
   * Notify the pres context that the safe area insets have changed.
   */
  void SetSafeAreaInsets(const mozilla::ScreenIntMargin& aInsets);

  mozilla::ScreenIntMargin GetSafeAreaInsets() const { return mSafeAreaInsets; }

 protected:
  void UpdateEffectiveTextZoom();

#ifdef DEBUG
  void ValidatePresShellAndDocumentReleation() const;
#endif  // #ifdef DEBUG

 public:
  /**
   * Corresponds to the product of text zoom and system font scale, limited
   * by zoom.maxPercent and minPercent.
   * As the system font scale is automatically set by the PresShell, code that
   * e.g. wants to transfer zoom levels to a new document should use TextZoom()
   * instead, which corresponds to the text zoom level that was actually set by
   * the front-end/user.
   */
  float EffectiveTextZoom() const { return mEffectiveTextZoom; }

  float GetFullZoom() { return mFullZoom; }
  /**
   * Device full zoom differs from full zoom because it gets the zoom from
   * the device context, which may be using a different zoom due to rounding
   * of app units to device pixels.
   */
  float GetDeviceFullZoom();
  void SetFullZoom(float aZoom);

  float GetOverrideDPPX() const { return mMediaEmulationData.mDPPX; }
  void SetOverrideDPPX(float);

  Maybe<StylePrefersColorScheme> GetOverridePrefersColorScheme() const {
    return mMediaEmulationData.mPrefersColorScheme;
  }
  void SetOverridePrefersColorScheme(const Maybe<StylePrefersColorScheme>&);

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

  int32_t AppUnitsPerDevPixel() const { return mCurAppUnitsPerDevPixel; }

  static nscoord CSSPixelsToAppUnits(int32_t aPixels) {
    return NSToCoordRoundWithClamp(float(aPixels) *
                                   float(mozilla::AppUnitsPerCSSPixel()));
  }

  static nscoord CSSPixelsToAppUnits(float aPixels) {
    return NSToCoordRoundWithClamp(aPixels *
                                   float(mozilla::AppUnitsPerCSSPixel()));
  }

  static int32_t AppUnitsToIntCSSPixels(nscoord aAppUnits) {
    return NSAppUnitsToIntPixels(aAppUnits,
                                 float(mozilla::AppUnitsPerCSSPixel()));
  }

  static float AppUnitsToFloatCSSPixels(nscoord aAppUnits) {
    return NSAppUnitsToFloatPixels(aAppUnits,
                                   float(mozilla::AppUnitsPerCSSPixel()));
  }

  static double AppUnitsToDoubleCSSPixels(nscoord aAppUnits) {
    return NSAppUnitsToDoublePixels(aAppUnits,
                                    double(mozilla::AppUnitsPerCSSPixel()));
  }

  nscoord DevPixelsToAppUnits(int32_t aPixels) const {
    return NSIntPixelsToAppUnits(aPixels, AppUnitsPerDevPixel());
  }

  int32_t AppUnitsToDevPixels(nscoord aAppUnits) const {
    return NSAppUnitsToIntPixels(aAppUnits, float(AppUnitsPerDevPixel()));
  }

  float AppUnitsToFloatDevPixels(nscoord aAppUnits) {
    return aAppUnits / float(AppUnitsPerDevPixel());
  }

  int32_t CSSPixelsToDevPixels(int32_t aPixels) {
    return AppUnitsToDevPixels(CSSPixelsToAppUnits(aPixels));
  }

  float CSSPixelsToDevPixels(float aPixels) {
    return NSAppUnitsToFloatPixels(CSSPixelsToAppUnits(aPixels),
                                   float(AppUnitsPerDevPixel()));
  }

  int32_t DevPixelsToIntCSSPixels(int32_t aPixels) {
    return AppUnitsToIntCSSPixels(DevPixelsToAppUnits(aPixels));
  }

  float DevPixelsToFloatCSSPixels(int32_t aPixels) const {
    return AppUnitsToFloatCSSPixels(DevPixelsToAppUnits(aPixels));
  }

  mozilla::CSSToLayoutDeviceScale CSSToDevPixelScale() const {
    return mozilla::CSSToLayoutDeviceScale(
        float(mozilla::AppUnitsPerCSSPixel()) / float(AppUnitsPerDevPixel()));
  }

  // If there is a remainder, it is rounded to nearest app units.
  nscoord GfxUnitsToAppUnits(gfxFloat aGfxUnits) const;

  gfxFloat AppUnitsToGfxUnits(nscoord aAppUnits) const;

  gfxRect AppUnitsToGfxUnits(const nsRect& aAppRect) const {
    return gfxRect(AppUnitsToGfxUnits(aAppRect.x),
                   AppUnitsToGfxUnits(aAppRect.y),
                   AppUnitsToGfxUnits(aAppRect.Width()),
                   AppUnitsToGfxUnits(aAppRect.Height()));
  }

  static nscoord CSSTwipsToAppUnits(float aTwips) {
    return NSToCoordRoundWithClamp(mozilla::AppUnitsPerCSSInch() *
                                   NS_TWIPS_TO_INCHES(aTwips));
  }

  // Margin-specific version, since they often need TwipsToAppUnits
  static nsMargin CSSTwipsToAppUnits(const nsIntMargin& marginInTwips) {
    return nsMargin(CSSTwipsToAppUnits(float(marginInTwips.top)),
                    CSSTwipsToAppUnits(float(marginInTwips.right)),
                    CSSTwipsToAppUnits(float(marginInTwips.bottom)),
                    CSSTwipsToAppUnits(float(marginInTwips.left)));
  }

  static nscoord CSSPointsToAppUnits(float aPoints) {
    return NSToCoordRound(aPoints * mozilla::AppUnitsPerCSSInch() /
                          POINTS_PER_INCH_FLOAT);
  }

  nscoord PhysicalMillimetersToAppUnits(float aMM) const;

  nscoord RoundAppUnitsToNearestDevPixels(nscoord aAppUnits) const {
    return DevPixelsToAppUnits(AppUnitsToDevPixels(aAppUnits));
  }

  /**
   * This checks the root element and the HTML BODY, if any, for an "overflow"
   * property that should be applied to the viewport. If one is found then we
   * return the element that we took the overflow from (which should then be
   * treated as "overflow: visible"), and we store the overflow style here.
   * If the document is in fullscreen, and the fullscreen element is not the
   * root, the scrollbar of viewport will be suppressed.
   * @return if scroll was propagated from some content node, the content node
   *         it was propagated from.
   */
  mozilla::dom::Element* UpdateViewportScrollStylesOverride();

  /**
   * Returns the cached result from the last call to
   * UpdateViewportScrollStylesOverride() -- i.e. return the node
   * whose scrollbar styles we have propagated to the viewport (or nullptr if
   * there is no such node).
   */
  mozilla::dom::Element* GetViewportScrollStylesOverrideElement() const {
    return mViewportScrollOverrideElement;
  }

  const ScrollStyles& GetViewportScrollStylesOverride() const {
    return mViewportScrollStyles;
  }

  /**
   * Check whether the given element would propagate its scrollbar styles to the
   * viewport in non-paginated mode.  Must only be called if IsPaginated().
   */
  bool ElementWouldPropagateScrollStyles(const mozilla::dom::Element&);

  /**
   * Set and get methods for controlling the background drawing
   */
  bool GetBackgroundImageDraw() const { return mDrawImageBackground; }
  void SetBackgroundImageDraw(bool aCanDraw) {
    mDrawImageBackground = aCanDraw;
  }

  bool GetBackgroundColorDraw() const { return mDrawColorBackground; }
  void SetBackgroundColorDraw(bool aCanDraw) {
    mDrawColorBackground = aCanDraw;
  }

  /**
   *  Check if bidi enabled (set depending on the presence of RTL
   *  characters or when default directionality is RTL).
   *  If enabled, we should apply the Unicode Bidi Algorithm
   *
   *  @lina 07/12/2000
   */
  bool BidiEnabled() const;

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
  void SetVisualMode(bool aIsVisual) { mIsVisual = aIsVisual; }

  /**
   *  Check whether the content should be treated as visual.
   *
   *  @lina 05/02/2000
   */
  bool IsVisualMode() const { return mIsVisual; }

  enum class InteractionType : uint32_t {
    ClickInteraction,
    KeyInteraction,
    MouseMoveInteraction,
    ScrollInteraction
  };

  void RecordInteractionTime(InteractionType aType,
                             const mozilla::TimeStamp& aTimeStamp);

  void DisableInteractionTimeRecording() { mInteractionTimeEnabled = false; }

  // Mohamed

  /**
   * Set the Bidi options for the presentation context
   */
  void SetBidi(uint32_t aBidiOptions);

  /**
   * Get the Bidi options for the presentation context
   * Not inline so consumers of nsPresContext are not forced to
   * include Document.
   */
  uint32_t GetBidi() const;

  /*
   * Obtain a native theme for rendering our widgets (both form controls and
   * html)
   *
   * Guaranteed to return non-null.
   */
  nsITheme* Theme() MOZ_NONNULL_RETURN {
    if (MOZ_LIKELY(mTheme)) {
      return mTheme;
    }
    return EnsureTheme();
  }

  /*
   * Notify the pres context that the theme has changed.  An internal switch
   * means it's one of our Mozilla themes that changed (e.g., Modern to
   * Classic). Otherwise, the OS is telling us that the native theme for the
   * platform has changed.
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
  void SetPrintSettings(nsIPrintSettings* aPrintSettings);

  nsIPrintSettings* GetPrintSettings() { return mPrintSettings; }

  /* Helper function that ensures that this prescontext is shown in its
     docshell if it's the most recent prescontext for the docshell.  Returns
     whether the prescontext is now being shown.
  */
  bool EnsureVisible();

#ifdef MOZ_REFLOW_PERF
  void CountReflows(const char* aName, nsIFrame* aFrame);
#endif

  void ConstructedFrame() { ++mFramesConstructed; }
  void ReflowedFrame() { ++mFramesReflowed; }

  uint64_t FramesConstructedCount() { return mFramesConstructed; }
  uint64_t FramesReflowedCount() { return mFramesReflowed; }

  static nscoord GetBorderWidthForKeyword(unsigned int aBorderWidthKeyword) {
    // This table maps border-width enums 'thin', 'medium', 'thick'
    // to actual nscoord values.
    static const nscoord kBorderWidths[] = {
        CSSPixelsToAppUnits(1), CSSPixelsToAppUnits(3), CSSPixelsToAppUnits(5)};
    MOZ_ASSERT(size_t(aBorderWidthKeyword) <
               mozilla::ArrayLength(kBorderWidths));

    return kBorderWidths[aBorderWidthKeyword];
  }

  gfxTextPerfMetrics* GetTextPerfMetrics() { return mTextPerf.get(); }
  FontMatchingStats* GetFontMatchingStats() { return mFontStats.get(); }

  bool IsDynamic() {
    return (mType == eContext_PageLayout || mType == eContext_Galley);
  }
  bool IsScreen() {
    return (mMedium == nsGkAtoms::screen || mType == eContext_PageLayout ||
            mType == eContext_PrintPreview);
  }
  bool IsPrintingOrPrintPreview() {
    return (mType == eContext_Print || mType == eContext_PrintPreview);
  }

  // Is this presentation in a chrome docshell?
  bool IsChrome() const;

  // Public API for native theme code to get style internals.
  bool HasAuthorSpecifiedRules(const nsIFrame* aFrame,
                               uint32_t ruleTypeMask) const;

  // Explicitly enable and disable paint flashing.
  void SetPaintFlashing(bool aPaintFlashing) {
    mPaintFlashing = aPaintFlashing;
    mPaintFlashingInitialized = true;
  }

  // This method should be used instead of directly accessing mPaintFlashing,
  // as that value may be out of date when mPaintFlashingInitialized is false.
  bool GetPaintFlashing() const;

  bool SuppressingResizeReflow() const { return mSuppressResizeReflow; }

  gfxUserFontSet* GetUserFontSet();

  // Should be called whenever the set of fonts available in the user
  // font set changes (e.g., because a new font loads, or because the
  // user font set is changed and fonts become unavailable).
  void UserFontSetUpdated(gfxUserFontEntry* aUpdatedFont = nullptr);

  gfxMissingFontRecorder* MissingFontRecorder() { return mMissingFonts.get(); }

  void NotifyMissingFonts();

  void FlushCounterStyles();
  void MarkCounterStylesDirty();

  void FlushFontFeatureValues();
  void MarkFontFeatureValuesDirty() { mFontFeatureValuesDirty = true; }

  // Ensure that it is safe to hand out CSS rules outside the layout
  // engine by ensuring that all CSS style sheets have unique inners
  // and, if necessary, synchronously rebuilding all style data.
  void EnsureSafeToHandOutCSSRules();

  // Mark an area as invalidated, associated with a given transaction id
  // (allocated by nsRefreshDriver::GetTransactionId). Invalidated regions will
  // be dispatched to MozAfterPaint events when NotifyDidPaintForSubtree is
  // called for the transaction id (or any higher id).
  void NotifyInvalidation(TransactionId aTransactionId, const nsRect& aRect);
  // aRect is in device pixels
  void NotifyInvalidation(TransactionId aTransactionId, const nsIntRect& aRect);
  void NotifyDidPaintForSubtree(
      TransactionId aTransactionId = TransactionId{0},
      const mozilla::TimeStamp& aTimeStamp = mozilla::TimeStamp());
  void NotifyRevokingDidPaint(TransactionId aTransactionId);
  void FireDOMPaintEvent(nsTArray<nsRect>* aList, TransactionId aTransactionId,
                         mozilla::TimeStamp aTimeStamp = mozilla::TimeStamp());

  // Callback for catching invalidations in ContainerLayers
  // Passed to LayerProperties::ComputeDifference
  static void NotifySubDocInvalidation(
      mozilla::layers::ContainerLayer* aContainer, const nsIntRegion* aRegion);
  void SetNotifySubDocInvalidationData(
      mozilla::layers::ContainerLayer* aContainer);
  static void ClearNotifySubDocInvalidationData(
      mozilla::layers::ContainerLayer* aContainer);
  bool IsDOMPaintEventPending();

  /**
   * Returns the RestyleManager's restyle generation counter.
   */
  uint64_t GetRestyleGeneration() const;
  uint64_t GetUndisplayedRestyleGeneration() const;

  /**
   * Returns whether there are any pending restyles or reflows.
   */
  bool HasPendingRestyleOrReflow();

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
    explicit InterruptPreventer(nsPresContext* aCtx)
        : mCtx(aCtx),
          mInterruptsEnabled(aCtx->mInterruptsEnabled),
          mHasPendingInterrupt(aCtx->mHasPendingInterrupt) {
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
   * PresShell::FrameNeedsToContinueReflow.
   */
  bool CheckForInterrupt(nsIFrame* aFrame);
  /**
   * Returns true if CheckForInterrupt has returned true since the last
   * ReflowStarted call. Cannot itself trigger an interrupt check.
   */
  bool HasPendingInterrupt() { return mHasPendingInterrupt; }
  /**
   * Sets a flag that will trip a reflow interrupt. This only bypasses the
   * interrupt timeout and the pending event check; other checks such as whether
   * interrupts are enabled and the interrupt check skipping still take effect.
   */
  void SetPendingInterruptFromTest() { mPendingInterruptFromTest = true; }

  /**
   * If we have a presshell, and if the given content's current
   * document is the same as our presshell's document, return the
   * content's primary frame.  Otherwise, return null.  Only use this
   * if you care about which presshell the primary frame is in.
   */
  nsIFrame* GetPrimaryFrameFor(nsIContent* aContent);

  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  virtual size_t SizeOfIncludingThis(
      mozilla::MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  /**
   * Deprecated. Please use the InProcess or CrossProcess variants
   * to specify which behaviour you want.
   */
  bool IsRootContentDocument() const;

  /**
   * We are a root content document in process if: we are not a resource doc, we
   * are not chrome, and we either have no parent in the current process or our
   * parent is chrome.
   */
  bool IsRootContentDocumentInProcess() const;

  /**
   * We are a root content document cross process if: we are not a resource doc,
   * we are not chrome, and we either have no parent in any process or our
   * parent is chrome.
   */
  bool IsRootContentDocumentCrossProcess() const;

  bool HadNonBlankPaint() const { return mHadNonBlankPaint; }
  bool HadContentfulPaint() const { return mHadContentfulPaint; }
  void NotifyNonBlankPaint();
  void NotifyContentfulPaint();
  void NotifyDOMContentFlushed();

  bool UsesExChUnits() const { return mUsesExChUnits; }

  void SetUsesExChUnits(bool aValue) { mUsesExChUnits = aValue; }

  // true if there are OMTA transition updates for the current document which
  // have been throttled, and therefore some style information may not be up
  // to date
  bool ExistThrottledUpdates() const { return mExistThrottledUpdates; }

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

  bool HasWarnedAboutTooLargeDashedOrDottedRadius() const {
    return mHasWarnedAboutTooLargeDashedOrDottedRadius;
  }

  void SetHasWarnedAboutTooLargeDashedOrDottedRadius() {
    mHasWarnedAboutTooLargeDashedOrDottedRadius = true;
  }

  nsBidi& GetBidiEngine();

  gfxFontFeatureValueSet* GetFontFeatureValuesLookup() const {
    return mFontFeatureValuesLookup;
  }

 protected:
  friend class nsRunnableMethod<nsPresContext>;
  void ThemeChangedInternal();
  void SysColorChangedInternal();
  void RefreshSystemMetrics();

  // update device context's resolution from the widget
  void UIResolutionChangedInternal();

  // if aScale > 0.0, use it as resolution scale factor to the device context
  // (otherwise get it from the widget)
  void UIResolutionChangedInternalScale(double aScale);

  void SetImgAnimations(nsIContent* aParent, uint16_t aMode);
  void SetSMILAnimations(mozilla::dom::Document* aDoc, uint16_t aNewMode,
                         uint16_t aOldMode);

  static void PreferenceChanged(const char* aPrefName, void* aSelf);
  void PreferenceChanged(const char* aPrefName);

  void UpdateAfterPreferencesChanged();
  void DispatchPrefChangedRunnableIfNeeded();

  void GetUserPreferences();

  void UpdateCharSet(NotNull<const Encoding*> aCharSet);

 public:
  // Used by the PresShell to force a reflow when some aspect of font info
  // has been updated, potentially affecting font selection and layout.
  void ForceReflowForFontInfoUpdate();

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

  void InvalidatePaintedLayers();

  uint32_t GetNextFrameRateMultiplier() const {
    return mNextFrameRateMultiplier;
  }

  void DidUseFrameRateMultiplier() {
    if (!mNextFrameRateMultiplier) {
      mNextFrameRateMultiplier = 1;
    } else if (mNextFrameRateMultiplier < 8) {
      mNextFrameRateMultiplier = mNextFrameRateMultiplier * 2;
    }
  }

 protected:
  // May be called multiple times (unlink, destructor)
  void Destroy();

  void AppUnitsPerDevPixelChanged();

  bool HavePendingInputEvent();

  // Creates a one-shot timer with the given aCallback & aDelay.
  // Returns a refcounted pointer to the timer (or nullptr on failure).
  already_AddRefed<nsITimer> CreateTimer(nsTimerCallbackFunc aCallback,
                                         const char* aName, uint32_t aDelay);

  struct TransactionInvalidations {
    TransactionId mTransactionId;
    nsTArray<nsRect> mInvalidations;
    bool mIsWaitingForPreviousTransaction = false;
  };
  TransactionInvalidations* GetInvalidations(TransactionId aTransactionId);

  // This should be called only when we update mVisibleArea or
  // mDynamicToolbarMaxHeight or `app units per device pixels` changes.
  void AdjustSizeForViewportUnits();

  // IMPORTANT: The ownership implicit in the following member variables
  // has been explicitly checked.  If you add any members to this class,
  // please make the ownership explicit (pinkerton, scc).

  nsPresContextType mType;
  // the PresShell owns a strong reference to the nsPresContext, and is
  // responsible for nulling this pointer before it is destroyed
  mozilla::PresShell* MOZ_NON_OWNING_REF mPresShell;  // [WEAK]
  RefPtr<mozilla::dom::Document> mDocument;
  RefPtr<nsDeviceContext> mDeviceContext;  // [STRONG] could be weak, but
                                           // better safe than sorry.
                                           // Cannot reintroduce cycles
                                           // since there is no dependency
                                           // from gfx back to layout.
  RefPtr<mozilla::EventStateManager> mEventManager;
  RefPtr<nsRefreshDriver> mRefreshDriver;
  RefPtr<mozilla::AnimationEventDispatcher> mAnimationEventDispatcher;
  RefPtr<mozilla::EffectCompositor> mEffectCompositor;
  mozilla::UniquePtr<nsTransitionManager> mTransitionManager;
  mozilla::UniquePtr<nsAnimationManager> mAnimationManager;
  mozilla::UniquePtr<mozilla::RestyleManager> mRestyleManager;
  RefPtr<mozilla::CounterStyleManager> mCounterStyleManager;
  const nsStaticAtom* mMedium;
  RefPtr<gfxFontFeatureValueSet> mFontFeatureValuesLookup;
  // TODO(emilio): Maybe lazily create and put under a UniquePtr if this grows a
  // lot?
  MediaEmulationData mMediaEmulationData;

 public:
  // The following are public member variables so that we can use them
  // with mozilla::AutoToggle or mozilla::AutoRestore.

  // Should we disable font size inflation because we're inside of
  // shrink-wrapping calculations on an inflation container?
  bool mInflationDisabledForShrinkWrap;

 protected:
  float mSystemFontScale;    // Internal text zoom factor, defaults to 1.0
  float mTextZoom;           // Text zoom, defaults to 1.0
  float mEffectiveTextZoom;  // Text zoom * system font scale
  float mFullZoom;           // Page zoom, defaults to 1.0
  gfxSize mLastFontInflationScreenSize;

  int32_t mCurAppUnitsPerDevPixel;
  int32_t mAutoQualityMinFontSizePixelsPref;

  nsCOMPtr<nsITheme> mTheme;
  nsCOMPtr<nsIPrintSettings> mPrintSettings;

  mozilla::UniquePtr<nsBidi> mBidiEngine;

  AutoTArray<TransactionInvalidations, 4> mTransactions;

  // text performance metrics
  mozilla::UniquePtr<gfxTextPerfMetrics> mTextPerf;

  mozilla::UniquePtr<FontMatchingStats> mFontStats;

  mozilla::UniquePtr<gfxMissingFontRecorder> mMissingFonts;

  nsRect mVisibleArea;
  // This value is used to resolve viewport units.
  // On mobile this size is including the dynamic toolbar maximum height below.
  // On desktops this size is pretty much the same as |mVisibleArea|.
  nsSize mSizeForViewportUnits;
  // The maximum height of the dynamic toolbar on mobile.
  mozilla::ScreenIntCoord mDynamicToolbarMaxHeight;
  mozilla::ScreenIntCoord mDynamicToolbarHeight;
  // Safe area insets support
  mozilla::ScreenIntMargin mSafeAreaInsets;
  nsSize mPageSize;
  float mPageScale;
  float mPPScale;

  // This is a non-owning pointer. May be null. If non-null, it's guaranteed to
  // be pointing to an element that's still alive, because we'll reset it in
  // UpdateViewportScrollStylesOverride() as part of the cleanup code when
  // this element is removed from the document. (For <body> and the root
  // element, this call happens in nsCSSFrameConstructor::ContentRemoved(). For
  // fullscreen elements, it happens in the fullscreen-specific cleanup invoked
  // by Element::UnbindFromTree().)
  mozilla::dom::Element* MOZ_NON_OWNING_REF mViewportScrollOverrideElement;
  ScrollStyles mViewportScrollStyles;

  bool mExistThrottledUpdates;

  uint16_t mImageAnimationMode;
  uint16_t mImageAnimationModePref;

  uint32_t mInterruptChecksToSkip;

  // During page load we use slower frame rate.
  uint32_t mNextFrameRateMultiplier;

  // Counters for tests and tools that want to detect frame construction
  // or reflow.
  uint64_t mElementsRestyled;
  uint64_t mFramesConstructed;
  uint64_t mFramesReflowed;

  mozilla::TimeStamp mReflowStartTime;

  mozilla::Maybe<TransactionId> mFirstContentfulPaintTransactionId;

  // Time of various first interaction types, used to report time from
  // first paint of the top level content pres shell to first interaction.
  mozilla::TimeStamp mFirstNonBlankPaintTime;
  mozilla::TimeStamp mFirstClickTime;
  mozilla::TimeStamp mFirstKeyTime;
  mozilla::TimeStamp mFirstMouseMoveTime;
  mozilla::TimeStamp mFirstScrollTime;

  bool mInteractionTimeEnabled;

  // last time we did a full style flush
  mozilla::TimeStamp mLastStyleUpdateForAllAnimations;

  unsigned mHasPendingInterrupt : 1;
  unsigned mPendingInterruptFromTest : 1;
  unsigned mInterruptsEnabled : 1;
  unsigned mSendAfterPaintToContent : 1;
  unsigned mDrawImageBackground : 1;
  unsigned mDrawColorBackground : 1;
  unsigned mNeverAnimate : 1;
  unsigned mPaginated : 1;
  unsigned mCanPaginatedScroll : 1;
  unsigned mDoScaledTwips : 1;
  unsigned mIsRootPaginatedDocument : 1;
  unsigned mPrefBidiDirection : 1;
  unsigned mPrefScrollbarSide : 2;
  unsigned mPendingSysColorChanged : 1;
  unsigned mPendingThemeChanged : 1;
  unsigned mPendingUIResolutionChanged : 1;
  unsigned mPrefChangePendingNeedsReflow : 1;
  unsigned mPostedPrefChangedRunnable : 1;

  // Are we currently drawing an SVG glyph?
  unsigned mIsGlyph : 1;

  // Does the associated document use ex or ch units?
  //
  // TODO(emilio): It's a bit weird that this lives here but all the other
  // relevant bits live in Device on the rust side.
  unsigned mUsesExChUnits : 1;

  // Is the current mCounterStyleManager valid?
  unsigned mCounterStylesDirty : 1;

  // Is the current mFontFeatureValuesLookup valid?
  unsigned mFontFeatureValuesDirty : 1;

  // resize reflow is suppressed when the only change has been to zoom
  // the document rather than to change the document's dimensions
  unsigned mSuppressResizeReflow : 1;

  unsigned mIsVisual : 1;

  // Should we paint flash in this context? Do not use this variable directly.
  // Use GetPaintFlashing() method instead.
  mutable unsigned mPaintFlashing : 1;
  mutable unsigned mPaintFlashingInitialized : 1;

  unsigned mHasWarnedAboutPositionedTableParts : 1;

  unsigned mHasWarnedAboutTooLargeDashedOrDottedRadius : 1;

  // Have we added quirk.css to the style set?
  unsigned mQuirkSheetAdded : 1;

  // Has NotifyNonBlankPaint been called on this PresContext?
  unsigned mHadNonBlankPaint : 1;
  // Has NotifyContentfulPaint been called on this PresContext?
  unsigned mHadContentfulPaint : 1;
  // Has NotifyDidPaintForSubtree been called for a contentful paint?
  unsigned mHadContentfulPaintComposite : 1;

#ifdef DEBUG
  unsigned mInitialized : 1;
#endif

  mozilla::UniquePtr<mozilla::MediaFeatureChange>
      mPendingMediaFeatureValuesChange;

 protected:
  virtual ~nsPresContext();

  void LastRelease();

  nsITheme* EnsureTheme();

#ifdef DEBUG
 private:
  friend struct nsAutoLayoutPhase;
  mozilla::EnumeratedArray<nsLayoutPhase, nsLayoutPhase::COUNT, uint32_t>
      mLayoutPhaseCount;

 public:
  uint32_t LayoutPhaseCount(nsLayoutPhase aPhase) {
    return mLayoutPhaseCount[aPhase];
  }
#endif
};

class nsRootPresContext final : public nsPresContext {
 public:
  nsRootPresContext(mozilla::dom::Document* aDocument, nsPresContextType aType);
  virtual ~nsRootPresContext();

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

  bool NeedToComputePluginGeometryUpdates() {
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
   * aBuilder and aList can be null. This indicates that all plugins are
   * hidden because we're in a background tab.
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
  void CollectPluginGeometryUpdates(
      mozilla::layers::LayerManager* aLayerManager);

  virtual bool IsRoot() override { return true; }

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

  virtual size_t SizeOfExcludingThis(
      mozilla::MallocSizeOf aMallocSizeOf) const override;

 protected:
  /**
   * Start a timer to ensure we eventually run ApplyPluginGeometryUpdates.
   */
  void InitApplyPluginGeometryTimer();
  /**
   * Cancel the timer that ensures we eventually run ApplyPluginGeometryUpdates.
   */
  void CancelApplyPluginGeometryTimer();

  class RunWillPaintObservers : public mozilla::Runnable {
   public:
    explicit RunWillPaintObservers(nsRootPresContext* aPresContext)
        : Runnable("nsPresContextType::RunWillPaintObservers"),
          mPresContext(aPresContext) {}
    void Revoke() { mPresContext = nullptr; }
    NS_IMETHOD Run() override {
      if (mPresContext) {
        mPresContext->FlushWillPaintObservers();
      }
      return NS_OK;
    }
    // The lifetime of this reference is handled by an nsRevocableEventPtr
    nsRootPresContext* MOZ_NON_OWNING_REF mPresContext;
  };

  friend class nsPresContext;

  nsCOMPtr<nsITimer> mApplyPluginGeometryTimer;
  nsTHashtable<nsRefPtrHashKey<nsIContent>> mRegisteredPlugins;
  nsTArray<nsCOMPtr<nsIRunnable>> mWillPaintObservers;
  nsRevocableEventPtr<RunWillPaintObservers> mWillPaintFallbackEvent;
};

#ifdef MOZ_REFLOW_PERF

#  define DO_GLOBAL_REFLOW_COUNT(_name) \
    aPresContext->CountReflows((_name), (nsIFrame*)this);
#else
#  define DO_GLOBAL_REFLOW_COUNT(_name)
#endif  // MOZ_REFLOW_PERF

#endif /* nsPresContext_h___ */
