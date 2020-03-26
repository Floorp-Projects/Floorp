/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a presentation of a document, part 1 */

#include "nsPresContext.h"
#include "nsPresContextInlines.h"

#include "mozilla/ArrayUtils.h"
#if defined(MOZ_WIDGET_ANDROID)
#  include "mozilla/AsyncEventDispatcher.h"
#endif
#include "mozilla/DebugOnly.h"
#include "mozilla/Encoding.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/PresShell.h"
#include "mozilla/PresShellInlines.h"

#include "base/basictypes.h"

#include "nsCOMPtr.h"
#include "nsCSSFrameConstructor.h"
#include "nsDocShell.h"
#include "nsIContentViewer.h"
#include "nsPIDOMWindow.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/MediaFeatureChange.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "nsIPrintSettings.h"
#include "nsLanguageAtomService.h"
#include "mozilla/LookAndFeel.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsHTMLDocument.h"
#include "nsIWeakReferenceUtils.h"
#include "nsThreadUtils.h"
#include "nsLayoutUtils.h"
#include "nsViewManager.h"
#include "mozilla/RestyleManager.h"
#include "SurfaceCacheUtils.h"
#include "nsMediaFeatures.h"
#include "gfxPlatform.h"
#include "nsFontFaceLoader.h"
#include "mozilla/AnimationEventDispatcher.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EventListenerManager.h"
#include "prenv.h"
#include "nsPluginFrame.h"
#include "nsTransitionManager.h"
#include "nsAnimationManager.h"
#include "CounterStyleManager.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Element.h"
#include "nsIMessageManager.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "mozilla/dom/MediaQueryList.h"
#include "mozilla/SMILAnimationController.h"
#include "mozilla/css/ImageLoader.h"
#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/StaticPresData.h"
#include "nsRefreshDriver.h"
#include "Layers.h"
#include "LayerUserData.h"
#include "ClientLayerManager.h"
#include "mozilla/dom/NotifyPaintEvent.h"

#include "nsFrameLoader.h"
#include "nsContentUtils.h"
#include "nsPIWindowRoot.h"
#include "mozilla/Preferences.h"
#include "gfxTextRun.h"
#include "nsFontFaceUtils.h"
#include "mozilla/GlobalStyleSheetCache.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/StaticPrefs_zoom.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/PerformanceTiming.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "MobileViewportManager.h"
#include "mozilla/dom/ImageTracker.h"

// Needed for Start/Stop of Image Animation
#include "imgIContainer.h"
#include "nsIImageLoadingContent.h"

#include "nsBidiUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsBidi.h"

#include "mozilla/dom/URL.h"
#include "mozilla/ServoCSSParser.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::layers;

uint8_t gNotifySubDocInvalidationData;

/**
 * Layer UserData for ContainerLayers that want to be notified
 * of local invalidations of them and their descendant layers.
 * Pass a callback to ComputeDifferences to have these called.
 */
class ContainerLayerPresContext : public LayerUserData {
 public:
  nsPresContext* mPresContext;
};

bool nsPresContext::IsDOMPaintEventPending() {
  if (!mTransactions.IsEmpty()) {
    return true;
  }

  nsRootPresContext* drpc = GetRootPresContext();
  if (drpc && drpc->mRefreshDriver->ViewManagerFlushIsPending()) {
    // Since we're promising that there will be a MozAfterPaint event
    // fired, we record an empty invalidation in case display list
    // invalidation doesn't invalidate anything further.
    NotifyInvalidation(drpc->mRefreshDriver->LastTransactionId().Next(),
                       nsRect(0, 0, 0, 0));
    return true;
  }
  return false;
}

void nsPresContext::ForceReflowForFontInfoUpdate() {
  // Flush the device context's font cache, so that we won't risk getting
  // stale nsFontMetrics objects from it.
  DeviceContext()->FlushFontCache();

  // If there's a user font set, discard any src:local() faces it may have
  // loaded because their font entries may no longer be valid.
  if (Document()->GetFonts()) {
    Document()->GetFonts()->GetUserFontSet()->ForgetLocalFaces();
  }

  // We can trigger reflow by pretending a font.* preference has changed;
  // this is the same mechanism as gfxPlatform::ForceGlobalReflow() uses
  // if new fonts are installed during the session, for example.
  PreferenceChanged("font.internaluseonly.changed");
}

static bool IsVisualCharset(NotNull<const Encoding*> aCharset) {
  return aCharset == ISO_8859_8_ENCODING;
}

nsPresContext::nsPresContext(dom::Document* aDocument, nsPresContextType aType)
    : mType(aType),
      mPresShell(nullptr),
      mDocument(aDocument),
      mMedium(aType == eContext_Galley ? nsGkAtoms::screen : nsGkAtoms::print),
      mInflationDisabledForShrinkWrap(false),
      mSystemFontScale(1.0),
      mTextZoom(1.0),
      mEffectiveTextZoom(1.0),
      mFullZoom(1.0),
      mLastFontInflationScreenSize(gfxSize(-1.0, -1.0)),
      mCurAppUnitsPerDevPixel(0),
      mAutoQualityMinFontSizePixelsPref(0),
      mDynamicToolbarMaxHeight(0),
      mDynamicToolbarHeight(0),
      mPageSize(-1, -1),
      mPageScale(0.0),
      mPPScale(1.0f),
      mViewportScrollOverrideElement(nullptr),
      mViewportScrollStyles(StyleOverflow::Auto, StyleOverflow::Auto),
      mExistThrottledUpdates(false),
      // mImageAnimationMode is initialised below, in constructor body
      mImageAnimationModePref(imgIContainer::kNormalAnimMode),
      mInterruptChecksToSkip(0),
      mNextFrameRateMultiplier(0),
      mElementsRestyled(0),
      mFramesConstructed(0),
      mFramesReflowed(0),
      mInteractionTimeEnabled(true),
      mHasPendingInterrupt(false),
      mPendingInterruptFromTest(false),
      mInterruptsEnabled(false),
      mSendAfterPaintToContent(false),
      mDrawImageBackground(true),  // always draw the background
      mDrawColorBackground(true),
      // mNeverAnimate is initialised below, in constructor body
      mPaginated(aType != eContext_Galley),
      mCanPaginatedScroll(false),
      mDoScaledTwips(true),
      mIsRootPaginatedDocument(false),
      mPrefBidiDirection(false),
      mPrefScrollbarSide(0),
      mPendingSysColorChanged(false),
      mPendingThemeChanged(false),
      mPendingUIResolutionChanged(false),
      mPrefChangePendingNeedsReflow(false),
      mPostedPrefChangedRunnable(false),
      mIsGlyph(false),
      mUsesExChUnits(false),
      mCounterStylesDirty(true),
      mFontFeatureValuesDirty(true),
      mSuppressResizeReflow(false),
      mIsVisual(false),
      mPaintFlashing(false),
      mPaintFlashingInitialized(false),
      mHasWarnedAboutPositionedTableParts(false),
      mHasWarnedAboutTooLargeDashedOrDottedRadius(false),
      mQuirkSheetAdded(false),
      mHadNonBlankPaint(false),
      mHadContentfulPaint(false),
      mHadContentfulPaintComposite(false)
#ifdef DEBUG
      ,
      mInitialized(false)
#endif
{
#ifdef DEBUG
  PodZero(&mLayoutPhaseCount);
#endif

  if (!IsDynamic()) {
    mImageAnimationMode = imgIContainer::kDontAnimMode;
    mNeverAnimate = true;
  } else {
    mImageAnimationMode = imgIContainer::kNormalAnimMode;
    mNeverAnimate = false;
  }
  NS_ASSERTION(mDocument, "Null document");

  // if text perf logging enabled, init stats struct
  if (MOZ_LOG_TEST(gfxPlatform::GetLog(eGfxLog_textperf), LogLevel::Warning)) {
    mTextPerf = MakeUnique<gfxTextPerfMetrics>();
  }

  if (Preferences::GetBool(GFX_MISSING_FONTS_NOTIFY_PREF)) {
    mMissingFonts = MakeUnique<gfxMissingFontRecorder>();
  }
}

static const char* gExactCallbackPrefs[] = {
    "browser.underline_anchors",
    "browser.anchor_color",
    "browser.active_color",
    "browser.visited_color",
    "image.animation_mode",
    "dom.send_after_paint_to_content",
    "layout.css.dpi",
    "layout.css.devPixelsPerPx",
    "nglayout.debug.paint_flashing",
    "nglayout.debug.paint_flashing_chrome",
    "intl.accept_languages",
    nullptr,
};

static const char* gPrefixCallbackPrefs[] = {
    "font.", "browser.display.", "bidi.", "gfx.font_rendering.", nullptr,
};

void nsPresContext::Destroy() {
  if (mEventManager) {
    // unclear if these are needed, but can't hurt
    mEventManager->NotifyDestroyPresContext(this);
    mEventManager->SetPresContext(nullptr);
    mEventManager = nullptr;
  }

  // Unregister preference callbacks
  Preferences::UnregisterPrefixCallbacks(nsPresContext::PreferenceChanged,
                                         gPrefixCallbackPrefs, this);
  Preferences::UnregisterCallbacks(nsPresContext::PreferenceChanged,
                                   gExactCallbackPrefs, this);

  mRefreshDriver = nullptr;
}

nsPresContext::~nsPresContext() {
  MOZ_ASSERT(!mPresShell, "Presshell forgot to clear our mPresShell pointer");
  DetachPresShell();

  Destroy();
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPresContext)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPresContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(nsPresContext, LastRelease())

void nsPresContext::LastRelease() {
  if (mMissingFonts) {
    mMissingFonts->Clear();
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsPresContext)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsPresContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAnimationEventDispatcher);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument);
  // NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mDeviceContext); // not xpcom
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEffectCompositor);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEventManager);
  // NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mLanguage); // an atom

  // NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTheme); // a service
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrintSettings);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsPresContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAnimationEventDispatcher);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDeviceContext);  // worth bothering?
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEffectCompositor);
  // NS_RELEASE(tmp->mLanguage); // an atom
  // NS_IMPL_CYCLE_COLLECTION_UNLINK(mTheme); // a service
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPrintSettings);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR

  tmp->Destroy();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

// Set to true when LookAndFeelChanged needs to be called.  This is used
// because the look and feel is a service, so there's no need to notify it from
// more than one prescontext.
static bool sLookAndFeelChanged;

// Set to true when ThemeChanged needs to be called on mTheme.  This is used
// because mTheme is a service, so there's no need to notify it from more than
// one prescontext.
static bool sThemeChanged;

bool nsPresContext::IsChrome() const {
  return Document()->IsInChromeDocShell();
}

void nsPresContext::GetUserPreferences() {
  if (!GetPresShell()) {
    // No presshell means nothing to do here.  We'll do this when we
    // get a presshell.
    return;
  }

  mAutoQualityMinFontSizePixelsPref =
      Preferences::GetInt("browser.display.auto_quality_min_font_size");

  PreferenceSheet::EnsureInitialized();

  mSendAfterPaintToContent = Preferences::GetBool(
      "dom.send_after_paint_to_content", mSendAfterPaintToContent);

  mPrefScrollbarSide = Preferences::GetInt("layout.scrollbar.side");

  Document()->SetMayNeedFontPrefsUpdate();

  // * image animation
  nsAutoCString animatePref;
  Preferences::GetCString("image.animation_mode", animatePref);
  if (animatePref.EqualsLiteral("normal"))
    mImageAnimationModePref = imgIContainer::kNormalAnimMode;
  else if (animatePref.EqualsLiteral("none"))
    mImageAnimationModePref = imgIContainer::kDontAnimMode;
  else if (animatePref.EqualsLiteral("once"))
    mImageAnimationModePref = imgIContainer::kLoopOnceAnimMode;
  else  // dynamic change to invalid value should act like it does initially
    mImageAnimationModePref = imgIContainer::kNormalAnimMode;

  uint32_t bidiOptions = GetBidi();

  int32_t prefInt = Preferences::GetInt(IBMBIDI_TEXTDIRECTION_STR,
                                        GET_BIDI_OPTION_DIRECTION(bidiOptions));
  SET_BIDI_OPTION_DIRECTION(bidiOptions, prefInt);
  mPrefBidiDirection = prefInt;

  prefInt = Preferences::GetInt(IBMBIDI_TEXTTYPE_STR,
                                GET_BIDI_OPTION_TEXTTYPE(bidiOptions));
  SET_BIDI_OPTION_TEXTTYPE(bidiOptions, prefInt);

  prefInt = Preferences::GetInt(IBMBIDI_NUMERAL_STR,
                                GET_BIDI_OPTION_NUMERAL(bidiOptions));
  SET_BIDI_OPTION_NUMERAL(bidiOptions, prefInt);

  // We don't need to force reflow: either we are initializing a new
  // prescontext or we are being called from UpdateAfterPreferencesChanged()
  // which triggers a reflow anyway.
  SetBidi(bidiOptions);
}

void nsPresContext::InvalidatePaintedLayers() {
  if (!mPresShell) {
    return;
  }
  if (nsIFrame* rootFrame = mPresShell->GetRootFrame()) {
    // FrameLayerBuilder caches invalidation-related values that depend on the
    // appunits-per-dev-pixel ratio, so ensure that all PaintedLayer drawing
    // is completely flushed.
    rootFrame->InvalidateFrameSubtree();
  }
}

void nsPresContext::AppUnitsPerDevPixelChanged() {
  int32_t oldAppUnitsPerDevPixel = mCurAppUnitsPerDevPixel;

  InvalidatePaintedLayers();

  if (mDeviceContext) {
    mDeviceContext->FlushFontCache();
  }

  MediaFeatureValuesChanged({RestyleHint::RecascadeSubtree(),
                             NS_STYLE_HINT_REFLOW,
                             MediaFeatureChangeReason::ResolutionChange});

  mCurAppUnitsPerDevPixel = mDeviceContext->AppUnitsPerDevPixel();

  // Recompute the size for vh units since it's changed by the dynamic toolbar
  // max height which is stored in screen coord.
  if (IsRootContentDocumentCrossProcess()) {
    AdjustSizeForViewportUnits();
  }

  // nsSubDocumentFrame uses a AppUnitsPerDevPixel difference between parent and
  // child document to determine if it needs to build a nsDisplayZoom item. So
  // if we that changes then we need to invalidate the subdoc frame so that
  // item gets created/removed.
  if (mPresShell) {
    if (nsIFrame* frame = mPresShell->GetRootFrame()) {
      frame = nsLayoutUtils::GetCrossDocParentFrame(frame);
      if (frame) {
        int32_t parentAPD = frame->PresContext()->AppUnitsPerDevPixel();
        if ((parentAPD == oldAppUnitsPerDevPixel) !=
            (parentAPD == mCurAppUnitsPerDevPixel)) {
          frame->InvalidateFrame();
        }
      }
    }
  }

  // We would also have to look at all of our child subdocuments but the
  // InvalidatePaintedLayers call above calls InvalidateFrameSubtree which
  // would invalidate all subdocument frames already.
}

// static
void nsPresContext::PreferenceChanged(const char* aPrefName, void* aSelf) {
  static_cast<nsPresContext*>(aSelf)->PreferenceChanged(aPrefName);
}

void nsPresContext::PreferenceChanged(const char* aPrefName) {
  nsDependentCString prefName(aPrefName);
  if (prefName.EqualsLiteral("layout.css.dpi") ||
      prefName.EqualsLiteral("layout.css.devPixelsPerPx")) {
    int32_t oldAppUnitsPerDevPixel = mDeviceContext->AppUnitsPerDevPixel();
    // We need to assume the DPI changes, since `mDeviceContext` is shared with
    // other documents, and we'd need to save the return value of the first call
    // for all of them.
    Unused << mDeviceContext->CheckDPIChange();
    if (mPresShell) {
      OwningNonNull<mozilla::PresShell> presShell(*mPresShell);
      // Re-fetch the view manager's window dimensions in case there's a
      // deferred resize which hasn't affected our mVisibleArea yet
      nscoord oldWidthAppUnits, oldHeightAppUnits;
      RefPtr<nsViewManager> vm = presShell->GetViewManager();
      if (!vm) {
        return;
      }
      vm->GetWindowDimensions(&oldWidthAppUnits, &oldHeightAppUnits);
      float oldWidthDevPixels = oldWidthAppUnits / oldAppUnitsPerDevPixel;
      float oldHeightDevPixels = oldHeightAppUnits / oldAppUnitsPerDevPixel;

      AppUnitsPerDevPixelChanged();

      nscoord width = NSToCoordRound(oldWidthDevPixels * AppUnitsPerDevPixel());
      nscoord height =
          NSToCoordRound(oldHeightDevPixels * AppUnitsPerDevPixel());
      vm->SetWindowDimensions(width, height);
    }
    return;
  }
  if (prefName.EqualsLiteral(GFX_MISSING_FONTS_NOTIFY_PREF)) {
    if (Preferences::GetBool(GFX_MISSING_FONTS_NOTIFY_PREF)) {
      if (!mMissingFonts) {
        mMissingFonts = MakeUnique<gfxMissingFontRecorder>();
        // trigger reflow to detect missing fonts on the current page
        mPrefChangePendingNeedsReflow = true;
      }
    } else {
      if (mMissingFonts) {
        mMissingFonts->Clear();
      }
      mMissingFonts = nullptr;
    }
  }
  if (StringBeginsWith(prefName, NS_LITERAL_CSTRING("font.")) ||
      prefName.EqualsLiteral("intl.accept_languages")) {
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

  // We will end up calling InvalidatePreferenceSheets one from each pres
  // context, but all it's doing is clearing its cached sheet pointers, so it
  // won't be wastefully recreating the sheet multiple times.
  //
  // The first pres context that has its pref changed runnable called will
  // be the one to cause the reconstruction of the pref style sheet.
  GlobalStyleSheetCache::InvalidatePreferenceSheets();
  PreferenceSheet::Refresh();
  DispatchPrefChangedRunnableIfNeeded();

  if (prefName.EqualsLiteral("nglayout.debug.paint_flashing") ||
      prefName.EqualsLiteral("nglayout.debug.paint_flashing_chrome")) {
    mPaintFlashingInitialized = false;
    return;
  }
}

void nsPresContext::DispatchPrefChangedRunnableIfNeeded() {
  if (mPostedPrefChangedRunnable) {
    return;
  }

  nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod("nsPresContext::UpdateAfterPreferencesChanged", this,
                        &nsPresContext::UpdateAfterPreferencesChanged);
  nsresult rv = Document()->Dispatch(TaskCategory::Other, runnable.forget());
  if (NS_SUCCEEDED(rv)) {
    mPostedPrefChangedRunnable = true;
  }
}

void nsPresContext::UpdateAfterPreferencesChanged() {
  mPostedPrefChangedRunnable = false;
  if (!mPresShell) {
    return;
  }

  if (mDocument->IsInChromeDocShell()) {
    // FIXME(emilio): Do really all these prefs not affect chrome docs? I
    // suspect we should move this check somewhere else.
    return;
  }

  StaticPresData::Get()->InvalidateFontPrefs();

  // Initialize our state from the user preferences
  GetUserPreferences();

  // update the presShell: tell it to set the preference style rules up
  mPresShell->UpdatePreferenceStyles();

  InvalidatePaintedLayers();
  mDeviceContext->FlushFontCache();

  nsChangeHint hint = nsChangeHint(0);

  if (mPrefChangePendingNeedsReflow) {
    hint |= NS_STYLE_HINT_REFLOW;
  }

  // Preferences require rerunning selector matching because we rebuild
  // the pref style sheet for some preference changes.
  RebuildAllStyleData(hint, RestyleHint::RestyleSubtree());
}

nsresult nsPresContext::Init(nsDeviceContext* aDeviceContext) {
  NS_ASSERTION(!mInitialized, "attempt to reinit pres context");
  NS_ENSURE_ARG(aDeviceContext);

  mDeviceContext = aDeviceContext;

  // In certain rare cases (such as changing page mode), we tear down layout
  // state and re-initialize a new prescontext for a document. Given that we
  // hang style state off the DOM, we detect that re-initialization case and
  // lazily drop the servo data. We don't do this eagerly during layout teardown
  // because that would incur an extra whole-tree traversal that's unnecessary
  // most of the time.
  //
  // FIXME(emilio): I'm pretty sure this doesn't happen after bug 1414999.
  Element* root = mDocument->GetRootElement();
  if (root && root->HasServoData()) {
    RestyleManager::ClearServoDataFromSubtree(root);
  }

  if (mDeviceContext->SetFullZoom(mFullZoom)) mDeviceContext->FlushFontCache();
  mCurAppUnitsPerDevPixel = mDeviceContext->AppUnitsPerDevPixel();

  mEventManager = new mozilla::EventStateManager();

  mAnimationEventDispatcher = new mozilla::AnimationEventDispatcher(this);
  mEffectCompositor = new mozilla::EffectCompositor(this);
  mTransitionManager = MakeUnique<nsTransitionManager>(this);
  mAnimationManager = MakeUnique<nsAnimationManager>(this);

  if (mDocument->GetDisplayDocument()) {
    NS_ASSERTION(mDocument->GetDisplayDocument()->GetPresContext(),
                 "Why are we being initialized?");
    mRefreshDriver =
        mDocument->GetDisplayDocument()->GetPresContext()->RefreshDriver();
  } else {
    dom::Document* parent = mDocument->GetInProcessParentDocument();
    // Unfortunately, sometimes |parent| here has no presshell because
    // printing screws up things.  Assert that in other cases it does,
    // but whenever the shell is null just fall back on using our own
    // refresh driver.
    NS_ASSERTION(
        !parent || mDocument->IsStaticDocument() || parent->GetPresShell(),
        "How did we end up with a presshell if our parent doesn't "
        "have one?");
    if (parent && parent->GetPresContext()) {
      dom::BrowsingContext* ourItem = mDocument->GetBrowsingContext();
      if (ourItem && !ourItem->IsTop()) {
        Element* containingElement =
            parent->FindContentForSubDocument(mDocument);
        if (!containingElement->IsXULElement() ||
            !containingElement->HasAttr(kNameSpaceID_None,
                                        nsGkAtoms::forceOwnRefreshDriver)) {
          mRefreshDriver = parent->GetPresContext()->RefreshDriver();
        }
      }
    }

    if (!mRefreshDriver) {
      mRefreshDriver = new nsRefreshDriver(this);
      if (XRE_IsContentProcess()) {
        mRefreshDriver->InitializeTimer();
      }
    }
  }

  // Register callbacks so we're notified when the preferences change
  Preferences::RegisterPrefixCallbacks(nsPresContext::PreferenceChanged,
                                       gPrefixCallbackPrefs, this);
  Preferences::RegisterCallbacks(nsPresContext::PreferenceChanged,
                                 gExactCallbackPrefs, this);

  nsresult rv = mEventManager->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  mEventManager->SetPresContext(this);

#if defined(MOZ_WIDGET_ANDROID)
  if (IsRootContentDocumentCrossProcess()) {
    if (BrowserChild* browserChild =
            BrowserChild::GetFrom(mDocument->GetDocShell())) {
      mDynamicToolbarMaxHeight = browserChild->GetDynamicToolbarMaxHeight();
      mDynamicToolbarHeight = mDynamicToolbarMaxHeight;
    }
  }
#endif

#ifdef DEBUG
  mInitialized = true;
#endif

  return NS_OK;
}

// Note: We don't hold a reference on the shell; it has a reference to
// us
void nsPresContext::AttachPresShell(mozilla::PresShell* aPresShell) {
  MOZ_ASSERT(!mPresShell);
  mPresShell = aPresShell;

  mRestyleManager = MakeUnique<mozilla::RestyleManager>(this);

  // Since CounterStyleManager is also the name of a method of
  // nsPresContext, it is necessary to prefix the class with the mozilla
  // namespace here.
  mCounterStyleManager = new mozilla::CounterStyleManager(this);

  dom::Document* doc = mPresShell->GetDocument();
  MOZ_ASSERT(doc);
  // Have to update PresContext's mDocument before calling any other methods.
  mDocument = doc;
  // Initialize our state from the user preferences, now that we
  // have a presshell, and hence a document.
  GetUserPreferences();

  nsIURI* docURI = doc->GetDocumentURI();

  if (IsDynamic() && docURI) {
    if (!docURI->SchemeIs("chrome") && !docURI->SchemeIs("resource"))
      mImageAnimationMode = mImageAnimationModePref;
    else
      mImageAnimationMode = imgIContainer::kNormalAnimMode;
  }

  UpdateCharSet(doc->GetDocumentCharacterSet());
}

void nsPresContext::DetachPresShell() {
  // The counter style manager's destructor needs to deallocate with the
  // presshell arena. Disconnect it before nulling out the shell.
  //
  // XXXbholley: Given recent refactorings, it probably makes more sense to
  // just null our mPresShell at the bottom of this function. I'm leaving it
  // this way to preserve the old ordering, but I doubt anything would break.
  if (mCounterStyleManager) {
    mCounterStyleManager->Disconnect();
    mCounterStyleManager = nullptr;
  }

  mPresShell = nullptr;

  if (mAnimationEventDispatcher) {
    mAnimationEventDispatcher->Disconnect();
    mAnimationEventDispatcher = nullptr;
  }
  if (mEffectCompositor) {
    mEffectCompositor->Disconnect();
    mEffectCompositor = nullptr;
  }
  if (mTransitionManager) {
    mTransitionManager->Disconnect();
    mTransitionManager = nullptr;
  }
  if (mAnimationManager) {
    mAnimationManager->Disconnect();
    mAnimationManager = nullptr;
  }
  if (mRestyleManager) {
    mRestyleManager->Disconnect();
    mRestyleManager = nullptr;
  }
  if (mRefreshDriver && mRefreshDriver->GetPresContext() == this) {
    mRefreshDriver->Disconnect();
    // Can't null out the refresh driver here.
  }

  if (IsRoot()) {
    nsRootPresContext* thisRoot = static_cast<nsRootPresContext*>(this);

    // Have to cancel our plugin geometry timer, because the
    // callback for that depends on a non-null presshell.
    thisRoot->CancelApplyPluginGeometryTimer();
  }
}

void nsPresContext::DocumentCharSetChanged(NotNull<const Encoding*> aCharSet) {
  UpdateCharSet(aCharSet);
  mDeviceContext->FlushFontCache();

  // If a document contains one or more <script> elements, frame construction
  // might happen earlier than the UpdateCharSet(), so we need to restyle
  // descendants to make their style data up-to-date.
  //
  // FIXME(emilio): Revisit whether this is true after bug 1438911.
  RebuildAllStyleData(NS_STYLE_HINT_REFLOW, RestyleHint::RecascadeSubtree());
}

void nsPresContext::UpdateCharSet(NotNull<const Encoding*> aCharSet) {
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
}

nsPresContext* nsPresContext::GetParentPresContext() const {
  mozilla::PresShell* presShell = GetPresShell();
  if (presShell) {
    nsViewManager* viewManager = presShell->GetViewManager();
    if (viewManager) {
      nsView* view = viewManager->GetRootView();
      if (view) {
        view = view->GetParent();  // anonymous inner view
        if (view) {
          view = view->GetParent();  // subdocumentframe's view
          if (view) {
            nsIFrame* f = view->GetFrame();
            if (f) {
              return f->PresContext();
            }
          }
        }
      }
    }
  }
  return nullptr;
}

nsPresContext* nsPresContext::GetToplevelContentDocumentPresContext() {
  if (IsChrome()) return nullptr;
  nsPresContext* pc = this;
  for (;;) {
    nsPresContext* parent = pc->GetParentPresContext();
    if (!parent || parent->IsChrome()) return pc;
    pc = parent;
  }
}

nsIWidget* nsPresContext::GetNearestWidget(nsPoint* aOffset) {
  NS_ENSURE_TRUE(mPresShell, nullptr);
  nsViewManager* vm = mPresShell->GetViewManager();
  NS_ENSURE_TRUE(vm, nullptr);
  nsView* rootView = vm->GetRootView();
  NS_ENSURE_TRUE(rootView, nullptr);
  return rootView->GetNearestWidget(aOffset);
}

nsIWidget* nsPresContext::GetRootWidget() const {
  NS_ENSURE_TRUE(mPresShell, nullptr);
  nsViewManager* vm = mPresShell->GetViewManager();
  if (!vm) {
    return nullptr;
  }
  nsCOMPtr<nsIWidget> widget;
  vm->GetRootWidget(getter_AddRefs(widget));
  return widget.get();
}

// We may want to replace this with something faster, maybe caching the root
// prescontext
nsRootPresContext* nsPresContext::GetRootPresContext() const {
  nsPresContext* pc = const_cast<nsPresContext*>(this);
  for (;;) {
    nsPresContext* parent = pc->GetParentPresContext();
    if (!parent) break;
    pc = parent;
  }
  return pc->IsRoot() ? static_cast<nsRootPresContext*>(pc) : nullptr;
}

// Helper function for setting Anim Mode on image
static void SetImgAnimModeOnImgReq(imgIRequest* aImgReq, uint16_t aMode) {
  if (aImgReq) {
    nsCOMPtr<imgIContainer> imgCon;
    aImgReq->GetImage(getter_AddRefs(imgCon));
    if (imgCon) {
      imgCon->SetAnimationMode(aMode);
    }
  }
}

// IMPORTANT: Assumption is that all images for a Presentation
// have the same Animation Mode (pavlov said this was OK)
//
// Walks content and set the animation mode
// this is a way to turn on/off image animations
void nsPresContext::SetImgAnimations(nsIContent* aParent, uint16_t aMode) {
  nsCOMPtr<nsIImageLoadingContent> imgContent(do_QueryInterface(aParent));
  if (imgContent) {
    nsCOMPtr<imgIRequest> imgReq;
    imgContent->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                           getter_AddRefs(imgReq));
    SetImgAnimModeOnImgReq(imgReq, aMode);
  }

  for (nsIContent* childContent = aParent->GetFirstChild(); childContent;
       childContent = childContent->GetNextSibling()) {
    SetImgAnimations(childContent, aMode);
  }
}

void nsPresContext::SetSMILAnimations(dom::Document* aDoc, uint16_t aNewMode,
                                      uint16_t aOldMode) {
  if (aDoc->HasAnimationController()) {
    SMILAnimationController* controller = aDoc->GetAnimationController();
    switch (aNewMode) {
      case imgIContainer::kNormalAnimMode:
      case imgIContainer::kLoopOnceAnimMode:
        if (aOldMode == imgIContainer::kDontAnimMode)
          controller->Resume(SMILTimeContainer::PAUSE_USERPREF);
        break;

      case imgIContainer::kDontAnimMode:
        if (aOldMode != imgIContainer::kDontAnimMode)
          controller->Pause(SMILTimeContainer::PAUSE_USERPREF);
        break;
    }
  }
}

void nsPresContext::SetImageAnimationMode(uint16_t aMode) {
  NS_ASSERTION(aMode == imgIContainer::kNormalAnimMode ||
                   aMode == imgIContainer::kDontAnimMode ||
                   aMode == imgIContainer::kLoopOnceAnimMode,
               "Wrong Animation Mode is being set!");

  // Image animation mode cannot be changed when rendering to a printer.
  if (!IsDynamic()) return;

  // Now walk the content tree and set the animation mode
  // on all the images.
  if (mPresShell) {
    dom::Document* doc = mPresShell->GetDocument();
    if (doc) {
      doc->StyleImageLoader()->SetAnimationMode(aMode);

      Element* rootElement = doc->GetRootElement();
      if (rootElement) {
        SetImgAnimations(rootElement, aMode);
      }
      SetSMILAnimations(doc, aMode, mImageAnimationMode);
    }
  }

  mImageAnimationMode = aMode;
}

void nsPresContext::UpdateEffectiveTextZoom() {
  float newZoom = mSystemFontScale * mTextZoom;
  float minZoom = StaticPrefs::zoom_minPercent() / 100.0f;
  float maxZoom = StaticPrefs::zoom_maxPercent() / 100.0f;

  if (newZoom < minZoom) {
    newZoom = minZoom;
  } else if (newZoom > maxZoom) {
    newZoom = maxZoom;
  }

  mEffectiveTextZoom = newZoom;

  // Media queries could have changed, since we changed the meaning
  // of 'em' units in them.
  MediaFeatureValuesChanged({RestyleHint::RecascadeSubtree(),
                             NS_STYLE_HINT_REFLOW,
                             MediaFeatureChangeReason::ZoomChange});
}

float nsPresContext::GetDeviceFullZoom() {
  return mDeviceContext->GetFullZoom();
}

void nsPresContext::SetFullZoom(float aZoom) {
  if (!mPresShell || mFullZoom == aZoom) {
    return;
  }

  // Re-fetch the view manager's window dimensions in case there's a deferred
  // resize which hasn't affected our mVisibleArea yet
  nscoord oldWidthAppUnits, oldHeightAppUnits;
  mPresShell->GetViewManager()->GetWindowDimensions(&oldWidthAppUnits,
                                                    &oldHeightAppUnits);
  float oldWidthDevPixels = oldWidthAppUnits / float(mCurAppUnitsPerDevPixel);
  float oldHeightDevPixels = oldHeightAppUnits / float(mCurAppUnitsPerDevPixel);
  mDeviceContext->SetFullZoom(aZoom);

  NS_ASSERTION(!mSuppressResizeReflow,
               "two zooms happening at the same time? impossible!");
  mSuppressResizeReflow = true;

  mFullZoom = aZoom;

  AppUnitsPerDevPixelChanged();

  mPresShell->GetViewManager()->SetWindowDimensions(
      NSToCoordRound(oldWidthDevPixels * AppUnitsPerDevPixel()),
      NSToCoordRound(oldHeightDevPixels * AppUnitsPerDevPixel()));

  mSuppressResizeReflow = false;
}

void nsPresContext::SetOverrideDPPX(float aDPPX) {
  // SetOverrideDPPX is called during navigations, including history
  // traversals.  In that case, it's typically called with our current value,
  // and we don't need to actually do anything.
  if (aDPPX == GetOverrideDPPX()) {
    return;
  }

  mMediaEmulationData.mDPPX = aDPPX;
  MediaFeatureValuesChanged({MediaFeatureChangeReason::ResolutionChange});
}

void nsPresContext::SetOverridePrefersColorScheme(
    const Maybe<StylePrefersColorScheme>& aOverride) {
  if (GetOverridePrefersColorScheme() == aOverride) {
    return;
  }
  mMediaEmulationData.mPrefersColorScheme = aOverride;
  // This is a bit of a lie, but it's the code-path that gets taken for regular
  // system metrics changes via ThemeChanged().
  MediaFeatureValuesChanged({MediaFeatureChangeReason::SystemMetricsChange});
}

gfxSize nsPresContext::ScreenSizeInchesForFontInflation(bool* aChanged) {
  if (aChanged) {
    *aChanged = false;
  }

  nsDeviceContext* dx = DeviceContext();
  nsRect clientRect;
  dx->GetClientRect(clientRect);  // FIXME: GetClientRect looks expensive
  float unitsPerInch = dx->AppUnitsPerPhysicalInch();
  gfxSize deviceSizeInches(float(clientRect.width) / unitsPerInch,
                           float(clientRect.height) / unitsPerInch);

  if (mLastFontInflationScreenSize == gfxSize(-1.0, -1.0)) {
    mLastFontInflationScreenSize = deviceSizeInches;
  }

  if (deviceSizeInches != mLastFontInflationScreenSize && aChanged) {
    *aChanged = true;
    mLastFontInflationScreenSize = deviceSizeInches;
  }

  return deviceSizeInches;
}

static bool CheckOverflow(const ComputedStyle* aComputedStyle,
                          ScrollStyles* aStyles) {
  // If they're not styled yet, we'll get around to it when constructing frames
  // for the element.
  if (!aComputedStyle) {
    return false;
  }
  const nsStyleDisplay* display = aComputedStyle->StyleDisplay();

  // If they will generate no box, just don't.
  if (display->mDisplay == StyleDisplay::None ||
      display->mDisplay == StyleDisplay::Contents) {
    return false;
  }

  if (display->mOverflowX == StyleOverflow::Visible) {
    MOZ_ASSERT(display->mOverflowY == StyleOverflow::Visible);
    return false;
  }

  if (display->mOverflowX == StyleOverflow::MozHiddenUnscrollable) {
    *aStyles = ScrollStyles(StyleOverflow::Hidden, StyleOverflow::Hidden);
  } else {
    *aStyles = ScrollStyles(*display);
  }
  return true;
}

// https://drafts.csswg.org/css-overflow/#overflow-propagation
//
// NOTE(emilio): We may need to use out-of-date styles for this, since this is
// called from nsCSSFrameConstructor::ContentRemoved. We could refactor this a
// bit to avoid doing that, and also fix correctness issues (we don't invalidate
// properly when we insert a body element and there is a previous one, for
// example).
static Element* GetPropagatedScrollStylesForViewport(
    nsPresContext* aPresContext, ScrollStyles* aStyles) {
  Document* document = aPresContext->Document();
  Element* docElement = document->GetRootElement();
  // docElement might be null if we're doing this after removing it.
  if (!docElement) {
    return nullptr;
  }

  // Check the style on the document root element
  const auto* rootStyle = Servo_Element_GetMaybeOutOfDateStyle(docElement);
  if (CheckOverflow(rootStyle, aStyles)) {
    // tell caller we stole the overflow style from the root element
    return docElement;
  }

  // Don't look in the BODY for non-HTML documents or HTML documents
  // with non-HTML roots.
  // XXX this should be earlier; we shouldn't even look at the document root
  // for non-HTML documents. Fix this once we support explicit CSS styling
  // of the viewport
  if (!document->IsHTMLOrXHTML() || !docElement->IsHTMLElement()) {
    return nullptr;
  }

  Element* bodyElement = document->AsHTMLDocument()->GetBodyElement();
  if (!bodyElement) {
    return nullptr;
  }

  MOZ_ASSERT(bodyElement->IsHTMLElement(nsGkAtoms::body),
             "GetBodyElement returned something bogus");

  const auto* bodyStyle = Servo_Element_GetMaybeOutOfDateStyle(bodyElement);
  if (CheckOverflow(bodyStyle, aStyles)) {
    // tell caller we stole the overflow style from the body element
    return bodyElement;
  }

  return nullptr;
}

Element* nsPresContext::UpdateViewportScrollStylesOverride() {
  // Start off with our default styles, and then update them as needed.
  mViewportScrollStyles =
      ScrollStyles(StyleOverflow::Auto, StyleOverflow::Auto);
  mViewportScrollOverrideElement = nullptr;
  // Don't propagate the scrollbar state in printing or print preview.
  if (!IsPaginated()) {
    mViewportScrollOverrideElement =
        GetPropagatedScrollStylesForViewport(this, &mViewportScrollStyles);
  }

  dom::Document* document = Document();
  if (Element* fullscreenElement = document->GetFullscreenElement()) {
    // If the document is in fullscreen, but the fullscreen element is
    // not the root element, we should explicitly suppress the scrollbar
    // here. Note that, we still need to return the original element
    // the styles are from, so that the state of those elements is not
    // affected across fullscreen change.
    if (fullscreenElement != document->GetRootElement() &&
        fullscreenElement != mViewportScrollOverrideElement) {
      mViewportScrollStyles =
          ScrollStyles(StyleOverflow::Hidden, StyleOverflow::Hidden);
    }
  }
  return mViewportScrollOverrideElement;
}

bool nsPresContext::ElementWouldPropagateScrollStyles(const Element& aElement) {
  MOZ_ASSERT(IsPaginated(), "Should only be called on paginated contexts");
  if (aElement.GetParent() && !aElement.IsHTMLElement(nsGkAtoms::body)) {
    // We certainly won't be propagating from this element.
    return false;
  }

  // Go ahead and just call GetPropagatedScrollStylesForViewport, but update
  // a dummy ScrollStyles we don't care about.  It'll do a bit of extra work,
  // but saves us having to have more complicated code or more code duplication;
  // in practice we will make this call quite rarely, because we checked for all
  // the common cases above.
  ScrollStyles dummy(StyleOverflow::Auto, StyleOverflow::Auto);
  return GetPropagatedScrollStylesForViewport(this, &dummy) == &aElement;
}

nsISupports* nsPresContext::GetContainerWeak() const {
  return mDocument->GetDocShell();
}

nsDocShell* nsPresContext::GetDocShell() const {
  return nsDocShell::Cast(mDocument->GetDocShell());
}

bool nsPresContext::BidiEnabled() const { return Document()->GetBidiEnabled(); }

void nsPresContext::SetBidiEnabled() const { Document()->SetBidiEnabled(); }

void nsPresContext::SetBidi(uint32_t aSource) {
  // Don't do all this stuff unless the options have changed.
  if (aSource == GetBidi()) {
    return;
  }

  Document()->SetBidiOptions(aSource);
  if (IBMBIDI_TEXTDIRECTION_RTL == GET_BIDI_OPTION_DIRECTION(aSource) ||
      IBMBIDI_NUMERAL_HINDI == GET_BIDI_OPTION_NUMERAL(aSource)) {
    SetBidiEnabled();
  }
  if (IBMBIDI_TEXTTYPE_VISUAL == GET_BIDI_OPTION_TEXTTYPE(aSource)) {
    SetVisualMode(true);
  } else if (IBMBIDI_TEXTTYPE_LOGICAL == GET_BIDI_OPTION_TEXTTYPE(aSource)) {
    SetVisualMode(false);
  } else {
    SetVisualMode(IsVisualCharset(Document()->GetDocumentCharacterSet()));
  }
}

uint32_t nsPresContext::GetBidi() const { return Document()->GetBidiOptions(); }

void nsPresContext::RecordInteractionTime(InteractionType aType,
                                          const TimeStamp& aTimeStamp) {
  if (!mInteractionTimeEnabled || aTimeStamp.IsNull()) {
    return;
  }

  // Array of references to the member variable of each time stamp
  // for the different interaction types, keyed by InteractionType.
  TimeStamp nsPresContext::*interactionTimes[] = {
      &nsPresContext::mFirstClickTime, &nsPresContext::mFirstKeyTime,
      &nsPresContext::mFirstMouseMoveTime, &nsPresContext::mFirstScrollTime};

  // Array of histogram IDs for the different interaction types,
  // keyed by InteractionType.
  Telemetry::HistogramID histogramIds[] = {
      Telemetry::TIME_TO_FIRST_CLICK_MS, Telemetry::TIME_TO_FIRST_KEY_INPUT_MS,
      Telemetry::TIME_TO_FIRST_MOUSE_MOVE_MS,
      Telemetry::TIME_TO_FIRST_SCROLL_MS};

  TimeStamp& interactionTime =
      this->*(interactionTimes[static_cast<uint32_t>(aType)]);
  if (!interactionTime.IsNull()) {
    // We have already recorded an interaction time.
    return;
  }

  // Record the interaction time if it occurs after the first paint
  // of the top level content document.
  nsPresContext* topContentPresContext =
      GetToplevelContentDocumentPresContext();

  if (!topContentPresContext) {
    // There is no top content pres context so we don't care
    // about the interaction time. Record a value anyways to avoid
    // trying to find the top content pres context in future interactions.
    interactionTime = TimeStamp::Now();
    return;
  }

  if (topContentPresContext->mFirstNonBlankPaintTime.IsNull() ||
      topContentPresContext->mFirstNonBlankPaintTime > aTimeStamp) {
    // Top content pres context has not had a non-blank paint yet
    // or the event timestamp is before the first non-blank paint,
    // so don't record interaction time.
    return;
  }

  // Check if we are recording the first of any of the interaction types.
  bool isFirstInteraction = true;
  for (TimeStamp nsPresContext::*memberPtr : interactionTimes) {
    TimeStamp& timeStamp = this->*(memberPtr);
    if (!timeStamp.IsNull()) {
      isFirstInteraction = false;
      break;
    }
  }

  interactionTime = TimeStamp::Now();
  // Only the top level content pres context reports first interaction
  // time to telemetry (if it hasn't already done so).
  if (this == topContentPresContext) {
    if (Telemetry::CanRecordExtended()) {
      double millis =
          (interactionTime - mFirstNonBlankPaintTime).ToMilliseconds();
      Telemetry::Accumulate(histogramIds[static_cast<uint32_t>(aType)], millis);

      if (isFirstInteraction) {
        Telemetry::Accumulate(Telemetry::TIME_TO_FIRST_INTERACTION_MS, millis);
      }
    }
  } else {
    topContentPresContext->RecordInteractionTime(aType, aTimeStamp);
  }
}

nsITheme* nsPresContext::EnsureTheme() {
  MOZ_ASSERT(!mTheme);
  if (StaticPrefs::widget_disable_native_theme_for_content() &&
      (!IsChrome() || XRE_IsContentProcess())) {
    mTheme = do_GetBasicNativeThemeDoNotUseDirectly();
  } else {
    mTheme = do_GetNativeThemeDoNotUseDirectly();
  }
  MOZ_RELEASE_ASSERT(mTheme);
  return mTheme;
}

void nsPresContext::ThemeChanged() {
  if (!mPendingThemeChanged) {
    sLookAndFeelChanged = true;
    sThemeChanged = true;

    nsCOMPtr<nsIRunnable> ev =
        NewRunnableMethod("nsPresContext::ThemeChangedInternal", this,
                          &nsPresContext::ThemeChangedInternal);
    nsresult rv = Document()->Dispatch(TaskCategory::Other, ev.forget());
    if (NS_SUCCEEDED(rv)) {
      mPendingThemeChanged = true;
    }
  }
}

void nsPresContext::ThemeChangedInternal() {
  mPendingThemeChanged = false;

  // Tell the theme that it changed, so it can flush any handles to stale theme
  // data.
  if (mTheme && sThemeChanged) {
    mTheme->ThemeChanged();
    sThemeChanged = false;
  }

  if (sLookAndFeelChanged) {
    // Clear all cached LookAndFeel colors.
    LookAndFeel::Refresh();
    sLookAndFeelChanged = false;

    // Vector images (SVG) may be using theme colors so we discard all cached
    // surfaces. (We could add a vector image only version of DiscardAll, but
    // in bug 940625 we decided theme changes are rare enough not to bother.)
    image::SurfaceCacheUtils::DiscardAll();
  }

  RefreshSystemMetrics();
  PreferenceSheet::Refresh();

  // Recursively notify all remote leaf descendants that the
  // system theme has changed.
  if (nsPIDOMWindowOuter* window = mDocument->GetWindow()) {
    if (RefPtr<nsPIWindowRoot> topLevelWin = window->GetTopWindowRoot()) {
      topLevelWin->EnumerateBrowsers(
          [](nsIRemoteTab* aBrowserParent, void*) {
            aBrowserParent->NotifyThemeChanged();
          },
          nullptr);
    }
  }
}

void nsPresContext::SysColorChanged() {
  if (!mPendingSysColorChanged) {
    sLookAndFeelChanged = true;
    nsCOMPtr<nsIRunnable> ev =
        NewRunnableMethod("nsPresContext::SysColorChangedInternal", this,
                          &nsPresContext::SysColorChangedInternal);
    nsresult rv = Document()->Dispatch(TaskCategory::Other, ev.forget());
    if (NS_SUCCEEDED(rv)) {
      mPendingSysColorChanged = true;
    }
  }
}

void nsPresContext::SysColorChangedInternal() {
  mPendingSysColorChanged = false;

  if (sLookAndFeelChanged) {
    // Don't use the cached values for the system colors
    LookAndFeel::Refresh();
    sLookAndFeelChanged = false;
  }

  // Invalidate cached '-moz-windows-accent-color-applies' media query:
  RefreshSystemMetrics();

  // Reset default background and foreground colors for the document since they
  // may be using system colors
  PreferenceSheet::Refresh();
}

void nsPresContext::RefreshSystemMetrics() {
  // This will force the system metrics to be generated the next time they're
  // used.
  nsMediaFeatures::FreeSystemMetrics();

  // Changes to system metrics can change media queries on them.
  //
  // Changes in theme can change system colors (whose changes are
  // properly reflected in computed style data), system fonts (whose
  // changes are not), and -moz-appearance (whose changes likewise are
  // not), so we need to recascade for the first, and reflow for the rest.
  MediaFeatureValuesChangedAllDocuments({
      RestyleHint::RecascadeSubtree(),
      NS_STYLE_HINT_REFLOW,
      MediaFeatureChangeReason::SystemMetricsChange,
  });
}

void nsPresContext::UIResolutionChanged() {
  if (!mPendingUIResolutionChanged) {
    nsCOMPtr<nsIRunnable> ev =
        NewRunnableMethod("nsPresContext::UIResolutionChangedInternal", this,
                          &nsPresContext::UIResolutionChangedInternal);
    nsresult rv = Document()->Dispatch(TaskCategory::Other, ev.forget());
    if (NS_SUCCEEDED(rv)) {
      mPendingUIResolutionChanged = true;
    }
  }
}

void nsPresContext::UIResolutionChangedSync() {
  if (!mPendingUIResolutionChanged) {
    mPendingUIResolutionChanged = true;
    UIResolutionChangedInternalScale(0.0);
  }
}

static void NotifyTabUIResolutionChanged(nsIRemoteTab* aTab, void* aArg) {
  aTab->NotifyResolutionChanged();
}

static void NotifyChildrenUIResolutionChanged(nsPIDOMWindowOuter* aWindow) {
  nsCOMPtr<Document> doc = aWindow->GetExtantDoc();
  RefPtr<nsPIWindowRoot> topLevelWin = nsContentUtils::GetWindowRoot(doc);
  if (!topLevelWin) {
    return;
  }
  topLevelWin->EnumerateBrowsers(NotifyTabUIResolutionChanged, nullptr);
}

void nsPresContext::UIResolutionChangedInternal() {
  UIResolutionChangedInternalScale(0.0);
}

void nsPresContext::UIResolutionChangedInternalScale(double aScale) {
  mPendingUIResolutionChanged = false;

  mDeviceContext->CheckDPIChange(&aScale);
  if (mCurAppUnitsPerDevPixel != mDeviceContext->AppUnitsPerDevPixel()) {
    AppUnitsPerDevPixelChanged();
  }

  // Recursively notify all remote leaf descendants of the change.
  if (nsPIDOMWindowOuter* window = mDocument->GetWindow()) {
    NotifyChildrenUIResolutionChanged(window);
  }

  auto recurse = [aScale](dom::Document& aSubDoc) {
    if (nsPresContext* pc = aSubDoc.GetPresContext()) {
      // For subdocuments, we want to apply the parent's scale, because there
      // are cases where the subdoc's device context is connected to a widget
      // that has an out-of-date resolution (it's on a different screen, but
      // currently hidden, and will not be updated until shown): bug 1249279.
      pc->UIResolutionChangedInternalScale(aScale);
    }
    return CallState::Continue;
  };
  mDocument->EnumerateSubDocuments(recurse);
}

void nsPresContext::EmulateMedium(nsAtom* aMediaType) {
  MOZ_ASSERT(!aMediaType || aMediaType->IsAsciiLowercase());
  RefPtr<const nsAtom> oldMedium = Medium();
  mMediaEmulationData.mMedium = aMediaType;

  if (Medium() != oldMedium) {
    MediaFeatureValuesChanged({MediaFeatureChangeReason::MediumChange});
  }
}

void nsPresContext::ContentLanguageChanged() {
  PostRebuildAllStyleDataEvent(nsChangeHint(0),
                               RestyleHint::RecascadeSubtree());
}

void nsPresContext::MediaFeatureValuesChanged(
    const MediaFeatureChange& aChange) {
  if (mPresShell) {
    mPresShell->EnsureStyleFlush();
  }

  if (!mPendingMediaFeatureValuesChange) {
    mPendingMediaFeatureValuesChange = MakeUnique<MediaFeatureChange>(aChange);
    return;
  }

  *mPendingMediaFeatureValuesChange |= aChange;
}

void nsPresContext::RebuildAllStyleData(nsChangeHint aExtraHint,
                                        const RestyleHint& aRestyleHint) {
  if (!mPresShell) {
    // We must have been torn down. Nothing to do here.
    return;
  }

  // TODO(emilio): It's unclear to me why would these three calls below be
  // needed. In particular, RebuildAllStyleData doesn't rebuild rules or
  // specified style information and such (note the comment in
  // RestyleManager::RebuildAllStyleData re. the funny semantics), so I
  // don't know why should we rebuild the user font set / counter styles /
  // etc...
  mDocument->MarkUserFontSetDirty();
  MarkCounterStylesDirty();
  MarkFontFeatureValuesDirty();
  PostRebuildAllStyleDataEvent(aExtraHint, aRestyleHint);
}

void nsPresContext::PostRebuildAllStyleDataEvent(
    nsChangeHint aExtraHint, const RestyleHint& aRestyleHint) {
  if (!mPresShell) {
    // We must have been torn down. Nothing to do here.
    return;
  }
  if (aRestyleHint.DefinitelyRecascadesAllSubtree()) {
    mUsesExChUnits = false;
  }
  RestyleManager()->RebuildAllStyleData(aExtraHint, aRestyleHint);
}

void nsPresContext::MediaFeatureValuesChangedAllDocuments(
    const MediaFeatureChange& aChange) {
  // Handle the media feature value change in this document.
  MediaFeatureValuesChanged(aChange);

  // Propagate the media feature value change down to any SVG images the
  // document is using.
  mDocument->ImageTracker()->MediaFeatureValuesChangedAllDocuments(aChange);

  // And then into any subdocuments.
  auto recurse = [&aChange](dom::Document& aSubDoc) {
    if (nsPresContext* pc = aSubDoc.GetPresContext()) {
      pc->MediaFeatureValuesChangedAllDocuments(aChange);
    }
    return CallState::Continue;
  };
  mDocument->EnumerateSubDocuments(recurse);
}

void nsPresContext::FlushPendingMediaFeatureValuesChanged() {
  if (!mPendingMediaFeatureValuesChange) {
    return;
  }

  MediaFeatureChange change = *mPendingMediaFeatureValuesChange;
  mPendingMediaFeatureValuesChange.reset();

  // MediumFeaturesChanged updates the applied rules, so it always gets called.
  if (mPresShell) {
    change.mRestyleHint |=
        mPresShell->StyleSet()->MediumFeaturesChanged(change.mReason);
  }

  if (change.mRestyleHint || change.mChangeHint) {
    RebuildAllStyleData(change.mChangeHint, change.mRestyleHint);
  }

  if (!mPresShell || !mPresShell->DidInitialize()) {
    return;
  }

  if (mDocument->IsBeingUsedAsImage()) {
    MOZ_ASSERT(mDocument->MediaQueryLists().isEmpty());
    return;
  }

  mDocument->NotifyMediaFeatureValuesChanged();

  MOZ_DIAGNOSTIC_ASSERT(nsContentUtils::IsSafeToRunScript());

  // Media query list listeners should be notified from a queued task
  // (in HTML5 terms), although we also want to notify them on certain
  // flushes.  (We're already running off an event.)
  //
  // Note that we do this after the new style from media queries in
  // style sheets has been computed.

  if (mDocument->MediaQueryLists().isEmpty()) {
    return;
  }

  // We build a list of all the notifications we're going to send
  // before we send any of them.

  // Copy pointers to all the lists into a new array, in case one of our
  // notifications modifies the list.
  nsTArray<RefPtr<mozilla::dom::MediaQueryList>> localMediaQueryLists;
  for (MediaQueryList* mql = mDocument->MediaQueryLists().getFirst(); mql;
       mql = static_cast<LinkedListElement<MediaQueryList>*>(mql)->getNext()) {
    localMediaQueryLists.AppendElement(mql);
  }

  // Now iterate our local array of the lists.
  for (const auto& mql : localMediaQueryLists) {
    nsAutoMicroTask mt;
    mql->MaybeNotify();
  }
}

void nsPresContext::SizeModeChanged(nsSizeMode aSizeMode) {
  if (nsPIDOMWindowOuter* window = mDocument->GetWindow()) {
    nsContentUtils::CallOnAllRemoteChildren(
        window, [&aSizeMode](BrowserParent* aBrowserParent) -> CallState {
          aBrowserParent->SizeModeChanged(aSizeMode);
          return CallState::Continue;
        });
  }
  MediaFeatureValuesChangedAllDocuments(
      {MediaFeatureChangeReason::SizeModeChange});
}

nsCompatibility nsPresContext::CompatibilityMode() const {
  return Document()->GetCompatibilityMode();
}

void nsPresContext::SetPaginatedScrolling(bool aPaginated) {
  if (mType == eContext_PrintPreview || mType == eContext_PageLayout)
    mCanPaginatedScroll = aPaginated;
}

void nsPresContext::SetPrintSettings(nsIPrintSettings* aPrintSettings) {
  if (mMedium == nsGkAtoms::print) mPrintSettings = aPrintSettings;
}

bool nsPresContext::EnsureVisible() {
  nsCOMPtr<nsIDocShell> docShell(GetDocShell());
  if (!docShell) {
    return false;
  }
  nsCOMPtr<nsIContentViewer> cv;
  docShell->GetContentViewer(getter_AddRefs(cv));
  // Make sure this is the content viewer we belong with
  if (!cv || cv->GetPresContext() != this) {
    return false;
  }
  // OK, this is us.  We want to call Show() on the content viewer.
  nsresult result = cv->Show();
  return NS_SUCCEEDED(result);
}

#ifdef MOZ_REFLOW_PERF
void nsPresContext::CountReflows(const char* aName, nsIFrame* aFrame) {
  if (mPresShell) {
    mPresShell->CountReflows(aName, aFrame);
  }
}
#endif

bool nsPresContext::HasAuthorSpecifiedRules(const nsIFrame* aFrame,
                                            uint32_t aRuleTypeMask) const {
  Element* elem = aFrame->GetContent()->AsElement();

  // We need to handle non-generated content pseudos too, so we use
  // the parent of generated content pseudo to be consistent.
  if (elem->GetPseudoElementType() != PseudoStyleType::NotPseudo) {
    MOZ_ASSERT(elem->GetParent(), "Pseudo element has no parent element?");
    elem = elem->GetParent()->AsElement();
  }
  if (MOZ_UNLIKELY(!elem->HasServoData())) {
    // Probably shouldn't happen, but does. See bug 1387953
    return false;
  }

  // Anonymous boxes are more complicated, and we just assume that they
  // cannot have any author-specified rules here.
  if (aFrame->Style()->IsAnonBox()) {
    return false;
  }

  auto* set = PresShell()->StyleSet()->RawSet();
  return Servo_HasAuthorSpecifiedRules(set, aFrame->Style(), elem,
                                       aRuleTypeMask);
}

gfxUserFontSet* nsPresContext::GetUserFontSet() {
  return mDocument->GetUserFontSet();
}

void nsPresContext::UserFontSetUpdated(gfxUserFontEntry* aUpdatedFont) {
  if (!mPresShell) {
    return;
  }

  // Note: this method is called without a font when rules in the userfont set
  // are updated.
  //
  // We can avoid a full restyle if font-metric-dependent units are not in use,
  // since we know there's no style resolution that would depend on this font
  // and trigger its load.
  //
  // TODO(emilio): We could be more granular if we knew which families have
  // potentially changed.
  if (!aUpdatedFont) {
    auto hint =
        UsesExChUnits() ? RestyleHint::RecascadeSubtree() : RestyleHint{0};
    PostRebuildAllStyleDataEvent(NS_STYLE_HINT_REFLOW, hint);
    return;
  }

  // Iterate over the frame tree looking for frames associated with the
  // downloadable font family in question. If a frame's nsStyleFont has
  // the name, check the font group associated with the metrics to see if
  // it contains that specific font (i.e. the one chosen within the family
  // given the weight, width, and slant from the nsStyleFont). If it does,
  // mark that frame dirty and skip inspecting its descendants.
  if (nsIFrame* root = mPresShell->GetRootFrame()) {
    nsFontFaceUtils::MarkDirtyForFontChange(root, aUpdatedFont);
  }
}

class CounterStyleCleaner final : public nsAPostRefreshObserver {
 public:
  CounterStyleCleaner(nsRefreshDriver* aRefreshDriver,
                      CounterStyleManager* aCounterStyleManager)
      : mRefreshDriver(aRefreshDriver),
        mCounterStyleManager(aCounterStyleManager) {}
  virtual ~CounterStyleCleaner() = default;

  void DidRefresh() final {
    mRefreshDriver->RemovePostRefreshObserver(this);
    mCounterStyleManager->CleanRetiredStyles();
    delete this;
  }

 private:
  RefPtr<nsRefreshDriver> mRefreshDriver;
  RefPtr<CounterStyleManager> mCounterStyleManager;
};

void nsPresContext::FlushCounterStyles() {
  if (!mPresShell) {
    return;  // we've been torn down
  }
  if (mCounterStyleManager->IsInitial()) {
    // Still in its initial state, no need to clean.
    return;
  }

  if (mCounterStylesDirty) {
    bool changed = mCounterStyleManager->NotifyRuleChanged();
    if (changed) {
      PresShell()->NotifyCounterStylesAreDirty();
      PostRebuildAllStyleDataEvent(NS_STYLE_HINT_REFLOW, RestyleHint{0});
      RefreshDriver()->AddPostRefreshObserver(
          new CounterStyleCleaner(RefreshDriver(), mCounterStyleManager));
    }
    mCounterStylesDirty = false;
  }
}

void nsPresContext::MarkCounterStylesDirty() {
  if (mCounterStyleManager->IsInitial()) {
    // Still in its initial state, no need to touch anything.
    return;
  }

  mCounterStylesDirty = true;
}

void nsPresContext::NotifyMissingFonts() {
  if (mMissingFonts) {
    mMissingFonts->Flush();
  }
}

void nsPresContext::EnsureSafeToHandOutCSSRules() {
  if (!mPresShell->StyleSet()->EnsureUniqueInnerOnCSSSheets()) {
    // Nothing to do.
    return;
  }

  RebuildAllStyleData(nsChangeHint(0), RestyleHint::RestyleSubtree());
}

void nsPresContext::FireDOMPaintEvent(
    nsTArray<nsRect>* aList, TransactionId aTransactionId,
    mozilla::TimeStamp aTimeStamp /* = mozilla::TimeStamp() */) {
  nsPIDOMWindowInner* ourWindow = mDocument->GetInnerWindow();
  if (!ourWindow) return;

  nsCOMPtr<EventTarget> dispatchTarget = do_QueryInterface(ourWindow);
  nsCOMPtr<EventTarget> eventTarget = dispatchTarget;
  if (!IsChrome() && !mSendAfterPaintToContent) {
    // Don't tell the window about this event, it should not know that
    // something happened in a subdocument. Tell only the chrome event handler.
    // (Events sent to the window get propagated to the chrome event handler
    // automatically.)
    dispatchTarget = ourWindow->GetParentTarget();
    if (!dispatchTarget) {
      return;
    }
  }

  if (aTimeStamp.IsNull()) {
    aTimeStamp = mozilla::TimeStamp::Now();
  }
  DOMHighResTimeStamp timeStamp = 0;
  if (ourWindow) {
    mozilla::dom::Performance* perf = ourWindow->GetPerformance();
    if (perf) {
      timeStamp = perf->GetDOMTiming()->TimeStampToDOMHighRes(aTimeStamp);
    }
  }

  // Events sent to the window get propagated to the chrome event handler
  // automatically.
  //
  // This will empty our list in case dispatching the event causes more damage
  // (hopefully it won't, or we're likely to get an infinite loop! At least
  // it won't be blocking app execution though).
  RefPtr<NotifyPaintEvent> event =
      NS_NewDOMNotifyPaintEvent(eventTarget, this, nullptr, eAfterPaint, aList,
                                uint64_t(aTransactionId), timeStamp);

  // Even if we're not telling the window about the event (so eventTarget is
  // the chrome event handler, not the window), the window is still
  // logically the event target.
  event->SetTarget(eventTarget);
  event->SetTrusted(true);
  EventDispatcher::DispatchDOMEvent(dispatchTarget, nullptr,
                                    static_cast<Event*>(event), this, nullptr);
}

static bool MayHavePaintEventListener(nsPIDOMWindowInner* aInnerWindow) {
  if (!aInnerWindow) return false;
  if (aInnerWindow->HasPaintEventListeners()) return true;

  EventTarget* parentTarget = aInnerWindow->GetParentTarget();
  if (!parentTarget) return false;

  EventListenerManager* manager = nullptr;
  if ((manager = parentTarget->GetExistingListenerManager()) &&
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

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(parentTarget);
  if (window) return MayHavePaintEventListener(window);

  nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(parentTarget);
  EventTarget* browserChildGlobal;
  return root && (browserChildGlobal = root->GetParentTarget()) &&
         (manager = browserChildGlobal->GetExistingListenerManager()) &&
         manager->MayHavePaintEventListener();
}

bool nsPresContext::MayHavePaintEventListener() {
  return ::MayHavePaintEventListener(mDocument->GetInnerWindow());
}

bool nsPresContext::MayHavePaintEventListenerInSubDocument() {
  if (MayHavePaintEventListener()) {
    return true;
  }

  bool result = false;
  auto recurse = [&result](dom::Document& aSubDoc) {
    if (nsPresContext* pc = aSubDoc.GetPresContext()) {
      if (pc->MayHavePaintEventListenerInSubDocument()) {
        result = true;
        return CallState::Stop;
      }
    }
    return CallState::Continue;
  };

  mDocument->EnumerateSubDocuments(recurse);
  return result;
}

void nsPresContext::NotifyInvalidation(TransactionId aTransactionId,
                                       const nsIntRect& aRect) {
  // Prevent values from overflow after DevPixelsToAppUnits().
  //
  // DevPixelsTopAppUnits() will multiple a factor (60) to the value,
  // it may make the result value over the edge (overflow) of max or
  // min value of int32_t. Compute the max sized dev pixel rect that
  // we can support and intersect with it.
  nsIntRect clampedRect = nsIntRect::MaxIntRect();
  clampedRect.ScaleInverseRoundIn(AppUnitsPerDevPixel());

  clampedRect = clampedRect.Intersect(aRect);

  nsRect rect(DevPixelsToAppUnits(clampedRect.x),
              DevPixelsToAppUnits(clampedRect.y),
              DevPixelsToAppUnits(clampedRect.width),
              DevPixelsToAppUnits(clampedRect.height));
  NotifyInvalidation(aTransactionId, rect);
}

nsPresContext::TransactionInvalidations* nsPresContext::GetInvalidations(
    TransactionId aTransactionId) {
  for (TransactionInvalidations& t : mTransactions) {
    if (t.mTransactionId == aTransactionId) {
      return &t;
    }
  }
  return nullptr;
}

void nsPresContext::NotifyInvalidation(TransactionId aTransactionId,
                                       const nsRect& aRect) {
  MOZ_ASSERT(GetContainerWeak(), "Invalidation in detached pres context");

  // If there is no paint event listener, then we don't need to fire
  // the asynchronous event. We don't even need to record invalidation.
  // MayHavePaintEventListener is pretty cheap and we could make it
  // even cheaper by providing a more efficient
  // nsPIDOMWindow::GetListenerManager.

  nsPresContext* pc;
  for (pc = this; pc; pc = pc->GetParentPresContext()) {
    TransactionInvalidations* transaction =
        pc->GetInvalidations(aTransactionId);
    if (transaction) {
      break;
    } else {
      transaction = pc->mTransactions.AppendElement();
      transaction->mTransactionId = aTransactionId;
    }
  }

  TransactionInvalidations* transaction = GetInvalidations(aTransactionId);
  MOZ_ASSERT(transaction);
  transaction->mInvalidations.AppendElement(aRect);
}

/* static */
void nsPresContext::NotifySubDocInvalidation(ContainerLayer* aContainer,
                                             const nsIntRegion* aRegion) {
  ContainerLayerPresContext* data = static_cast<ContainerLayerPresContext*>(
      aContainer->GetUserData(&gNotifySubDocInvalidationData));
  if (!data) {
    return;
  }

  TransactionId transactionId = aContainer->Manager()->GetLastTransactionId();
  IntRect visibleBounds =
      aContainer->GetVisibleRegion().GetBounds().ToUnknownRect();

  if (!aRegion) {
    IntRect rect(IntPoint(0, 0), visibleBounds.Size());
    data->mPresContext->NotifyInvalidation(transactionId, rect);
    return;
  }

  nsIntPoint topLeft = visibleBounds.TopLeft();
  for (auto iter = aRegion->RectIter(); !iter.Done(); iter.Next()) {
    nsIntRect rect(iter.Get());
    // PresContext coordinate space is relative to the start of our visible
    // region. Is this really true? This feels like the wrong way to get the
    // right answer.
    rect.MoveBy(-topLeft);
    data->mPresContext->NotifyInvalidation(transactionId, rect);
  }
}

void nsPresContext::SetNotifySubDocInvalidationData(
    ContainerLayer* aContainer) {
  ContainerLayerPresContext* pres = new ContainerLayerPresContext;
  pres->mPresContext = this;
  aContainer->SetUserData(&gNotifySubDocInvalidationData, pres);
}

/* static */
void nsPresContext::ClearNotifySubDocInvalidationData(
    ContainerLayer* aContainer) {
  aContainer->SetUserData(&gNotifySubDocInvalidationData, nullptr);
}

class DelayedFireDOMPaintEvent : public Runnable {
 public:
  DelayedFireDOMPaintEvent(
      nsPresContext* aPresContext, nsTArray<nsRect>* aList,
      TransactionId aTransactionId,
      const mozilla::TimeStamp& aTimeStamp = mozilla::TimeStamp())
      : mozilla::Runnable("DelayedFireDOMPaintEvent"),
        mPresContext(aPresContext),
        mTransactionId(aTransactionId),
        mTimeStamp(aTimeStamp) {
    MOZ_ASSERT(mPresContext->GetContainerWeak(),
               "DOMPaintEvent requested for a detached pres context");
    mList.SwapElements(*aList);
  }
  NS_IMETHOD Run() override {
    // The pres context might have been detached during the delay -
    // that's fine, just don't fire the event.
    if (mPresContext->GetContainerWeak()) {
      mPresContext->FireDOMPaintEvent(&mList, mTransactionId, mTimeStamp);
    }
    return NS_OK;
  }

  RefPtr<nsPresContext> mPresContext;
  TransactionId mTransactionId;
  const mozilla::TimeStamp mTimeStamp;
  nsTArray<nsRect> mList;
};

void nsPresContext::NotifyRevokingDidPaint(TransactionId aTransactionId) {
  if ((IsRoot() || !PresShell()->IsVisible()) && mTransactions.IsEmpty()) {
    return;
  }

  TransactionInvalidations* transaction = nullptr;
  for (auto& t : mTransactions) {
    if (t.mTransactionId == aTransactionId) {
      transaction = &t;
      break;
    }
  }
  // If there are no transaction invalidations (which imply callers waiting
  // on the event) for this revoked id, then we don't need to fire a
  // MozAfterPaint.
  if (!transaction) {
    return;
  }

  // If there are queued transactions with an earlier id, we can't send
  // our event now since it will arrive out of order. Set the waiting for
  // previous transaction flag to true, and we'll send the event when
  // the others are completed.
  // If this is the only transaction, then we can send it immediately.
  if (mTransactions.Length() == 1) {
    nsCOMPtr<nsIRunnable> ev = new DelayedFireDOMPaintEvent(
        this, &transaction->mInvalidations, transaction->mTransactionId,
        mozilla::TimeStamp());
    nsContentUtils::AddScriptRunner(ev);
    mTransactions.RemoveElementAt(0);
  } else {
    transaction->mIsWaitingForPreviousTransaction = true;
  }

  auto recurse = [&aTransactionId](dom::Document& aSubDoc) {
    if (nsPresContext* pc = aSubDoc.GetPresContext()) {
      pc->NotifyRevokingDidPaint(aTransactionId);
    }
    return CallState::Continue;
  };
  mDocument->EnumerateSubDocuments(recurse);
}

void nsPresContext::NotifyDidPaintForSubtree(
    TransactionId aTransactionId, const mozilla::TimeStamp& aTimeStamp) {
  if (mFirstContentfulPaintTransactionId && !mHadContentfulPaintComposite) {
    if (aTransactionId >= *mFirstContentfulPaintTransactionId) {
      mHadContentfulPaintComposite = true;
      RefPtr<nsDOMNavigationTiming> timing = mDocument->GetNavigationTiming();
      if (timing) {
        timing->NotifyContentfulPaintForRootContentDocument(aTimeStamp);
      }
    }
  }

  if (IsRoot() && mTransactions.IsEmpty()) {
    return;
  }

  if (!PresShell()->IsVisible() && mTransactions.IsEmpty()) {
    return;
  }

  // Non-root prescontexts fire MozAfterPaint to all their descendants
  // unconditionally, even if no invalidations have been collected. This is
  // because we don't want to eat the cost of collecting invalidations for
  // every subdocument (which would require putting every subdocument in its
  // own layer).

  bool sent = false;
  uint32_t i = 0;
  while (i < mTransactions.Length()) {
    if (mTransactions[i].mTransactionId <= aTransactionId) {
      if (!mTransactions[i].mInvalidations.IsEmpty()) {
        nsCOMPtr<nsIRunnable> ev = new DelayedFireDOMPaintEvent(
            this, &mTransactions[i].mInvalidations,
            mTransactions[i].mTransactionId, aTimeStamp);
        nsContentUtils::AddScriptRunner(ev);
        sent = true;
      }
      mTransactions.RemoveElementAt(i);
    } else {
      // If there are transaction which is waiting for this transaction,
      // we should fire a MozAfterPaint immediately.
      if (sent && mTransactions[i].mIsWaitingForPreviousTransaction) {
        nsCOMPtr<nsIRunnable> ev = new DelayedFireDOMPaintEvent(
            this, &mTransactions[i].mInvalidations,
            mTransactions[i].mTransactionId, aTimeStamp);
        nsContentUtils::AddScriptRunner(ev);
        sent = true;
        mTransactions.RemoveElementAt(i);
        continue;
      }
      i++;
    }
  }

  if (!sent) {
    nsTArray<nsRect> dummy;
    nsCOMPtr<nsIRunnable> ev =
        new DelayedFireDOMPaintEvent(this, &dummy, aTransactionId, aTimeStamp);
    nsContentUtils::AddScriptRunner(ev);
  }

  auto recurse = [&aTransactionId, &aTimeStamp](dom::Document& aSubDoc) {
    if (nsPresContext* pc = aSubDoc.GetPresContext()) {
      pc->NotifyDidPaintForSubtree(aTransactionId, aTimeStamp);
    }
    return CallState::Continue;
  };
  mDocument->EnumerateSubDocuments(recurse);
}

already_AddRefed<nsITimer> nsPresContext::CreateTimer(
    nsTimerCallbackFunc aCallback, const char* aName, uint32_t aDelay) {
  nsCOMPtr<nsITimer> timer;
  NS_NewTimerWithFuncCallback(getter_AddRefs(timer), aCallback, this, aDelay,
                              nsITimer::TYPE_ONE_SHOT, aName,
                              Document()->EventTargetFor(TaskCategory::Other));
  return timer.forget();
}

static bool sGotInterruptEnv = false;
enum InterruptMode { ModeRandom, ModeCounter, ModeEvent };
// Controlled by the GECKO_REFLOW_INTERRUPT_MODE env var; allowed values are
// "random" (except on Windows) or "counter".  If neither is used, the mode is
// ModeEvent.
static InterruptMode sInterruptMode = ModeEvent;
#ifndef XP_WIN
// Used for the "random" mode.  Controlled by the GECKO_REFLOW_INTERRUPT_SEED
// env var.
static uint32_t sInterruptSeed = 1;
#endif
// Used for the "counter" mode.  This is the number of unskipped interrupt
// checks that have to happen before we interrupt.  Controlled by the
// GECKO_REFLOW_INTERRUPT_FREQUENCY env var.
static uint32_t sInterruptMaxCounter = 10;
// Used for the "counter" mode.  This counts up to sInterruptMaxCounter and is
// then reset to 0.
static uint32_t sInterruptCounter;
// Number of interrupt checks to skip before really trying to interrupt.
// Controlled by the GECKO_REFLOW_INTERRUPT_CHECKS_TO_SKIP env var.
static uint32_t sInterruptChecksToSkip = 200;
// Number of milliseconds that a reflow should be allowed to run for before we
// actually allow interruption.  Controlled by the
// GECKO_REFLOW_MIN_NOINTERRUPT_DURATION env var.  Can't be initialized here,
// because TimeDuration/TimeStamp is not safe to use in static constructors..
static TimeDuration sInterruptTimeout;

static void GetInterruptEnv() {
  char* ev = PR_GetEnv("GECKO_REFLOW_INTERRUPT_MODE");
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

bool nsPresContext::HavePendingInputEvent() {
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

bool nsPresContext::HasPendingRestyleOrReflow() {
  mozilla::PresShell* presShell = PresShell();
  return presShell->NeedStyleFlush() || presShell->HasPendingReflow();
}

void nsPresContext::ReflowStarted(bool aInterruptible) {
#ifdef NOISY_INTERRUPTIBLE_REFLOW
  if (!aInterruptible) {
    printf("STARTING NONINTERRUPTIBLE REFLOW\n");
  }
#endif
  // We don't support interrupting in paginated contexts, since page
  // sequences only handle initial reflow
  mInterruptsEnabled = aInterruptible && !IsPaginated() &&
                       StaticPrefs::layout_interruptible_reflow_enabled();

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

bool nsPresContext::CheckForInterrupt(nsIFrame* aFrame) {
  if (mHasPendingInterrupt) {
    mPresShell->FrameNeedsToContinueReflow(aFrame);
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
      HavePendingInputEvent() && !IsChrome();

  if (mPendingInterruptFromTest) {
    mPendingInterruptFromTest = false;
    mHasPendingInterrupt = true;
  }

  if (mHasPendingInterrupt) {
#ifdef NOISY_INTERRUPTIBLE_REFLOW
    printf("*** DETECTED pending interrupt (time=%lld)\n", PR_Now());
#endif /* NOISY_INTERRUPTIBLE_REFLOW */
    mPresShell->FrameNeedsToContinueReflow(aFrame);
  }
  return mHasPendingInterrupt;
}

nsIFrame* nsPresContext::GetPrimaryFrameFor(nsIContent* aContent) {
  MOZ_ASSERT(aContent, "Don't do that");
  if (GetPresShell() &&
      GetPresShell()->GetDocument() == aContent->GetComposedDoc()) {
    return aContent->GetPrimaryFrame();
  }
  return nullptr;
}

size_t nsPresContext::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  // Measurement may be added later if DMD finds it is worthwhile.
  return 0;
}

bool nsPresContext::IsRootContentDocument() const {
  // Default to the in process version for now, but we should try
  // to remove callers of this.
  return IsRootContentDocumentInProcess();
}

bool nsPresContext::IsRootContentDocumentInProcess() const {
  if (mDocument->IsResourceDoc()) {
    return false;
  }
  if (IsChrome()) {
    return false;
  }
  // We may not have a root frame, so use views.
  nsView* view = PresShell()->GetViewManager()->GetRootView();
  if (!view) {
    return false;
  }
  view = view->GetParent();  // anonymous inner view
  if (!view) {
    return true;
  }
  view = view->GetParent();  // subdocumentframe's view
  if (!view) {
    return true;
  }

  nsIFrame* f = view->GetFrame();
  return (f && f->PresContext()->IsChrome());
}

bool nsPresContext::IsRootContentDocumentCrossProcess() const {
  if (mDocument->IsResourceDoc()) {
    return false;
  }

  return mDocument->IsTopLevelContentDocument();
}

void nsPresContext::NotifyNonBlankPaint() {
  MOZ_ASSERT(!mHadNonBlankPaint);
  mHadNonBlankPaint = true;
  if (IsRootContentDocument()) {
    RefPtr<nsDOMNavigationTiming> timing = mDocument->GetNavigationTiming();
    if (timing) {
      timing->NotifyNonBlankPaintForRootContentDocument();
    }

    mFirstNonBlankPaintTime = TimeStamp::Now();
  }
}

void nsPresContext::NotifyContentfulPaint() {
  if (!mHadContentfulPaint) {
    mHadContentfulPaint = true;
    if (IsRootContentDocument()) {
      if (nsRootPresContext* rootPresContext = GetRootPresContext()) {
        mFirstContentfulPaintTransactionId =
            Some(rootPresContext->mRefreshDriver->LastTransactionId().Next());
#if defined(MOZ_WIDGET_ANDROID)
        (new AsyncEventDispatcher(mDocument,
                                  NS_LITERAL_STRING("MozFirstContentfulPaint"),
                                  CanBubble::eYes, ChromeOnlyDispatch::eYes))
            ->PostDOMEvent();
#endif
      }
    }
  }
}

void nsPresContext::NotifyDOMContentFlushed() {
  NS_ENSURE_TRUE_VOID(mPresShell);
  if (IsRootContentDocument()) {
    RefPtr<nsDOMNavigationTiming> timing = mDocument->GetNavigationTiming();
    if (timing) {
      timing->NotifyDOMContentFlushedForRootContentDocument();
    }
  }
}

bool nsPresContext::GetPaintFlashing() const {
  if (!mPaintFlashingInitialized) {
    bool pref = Preferences::GetBool("nglayout.debug.paint_flashing");
    if (!pref && IsChrome()) {
      pref = Preferences::GetBool("nglayout.debug.paint_flashing_chrome");
    }
    mPaintFlashing = pref;
    mPaintFlashingInitialized = true;
  }
  return mPaintFlashing;
}

nscoord nsPresContext::GfxUnitsToAppUnits(gfxFloat aGfxUnits) const {
  return mDeviceContext->GfxUnitsToAppUnits(aGfxUnits);
}

gfxFloat nsPresContext::AppUnitsToGfxUnits(nscoord aAppUnits) const {
  return mDeviceContext->AppUnitsToGfxUnits(aAppUnits);
}

nscoord nsPresContext::PhysicalMillimetersToAppUnits(float aMM) const {
  float inches = aMM / MM_PER_INCH_FLOAT;
  return NSToCoordFloorClamped(
      inches * float(DeviceContext()->AppUnitsPerPhysicalInch()));
}

bool nsPresContext::IsDeviceSizePageSize() {
  nsIDocShell* docShell = GetDocShell();
  return docShell && docShell->GetDeviceSizeIsPageSize();
}

uint64_t nsPresContext::GetRestyleGeneration() const {
  if (!mRestyleManager) {
    return 0;
  }
  return mRestyleManager->GetRestyleGeneration();
}

uint64_t nsPresContext::GetUndisplayedRestyleGeneration() const {
  if (!mRestyleManager) {
    return 0;
  }
  return mRestyleManager->GetUndisplayedRestyleGeneration();
}

nsBidi& nsPresContext::GetBidiEngine() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mBidiEngine) {
    mBidiEngine.reset(new nsBidi());
  }
  return *mBidiEngine;
}

void nsPresContext::FlushFontFeatureValues() {
  if (!mPresShell) {
    return;  // we've been torn down
  }

  if (mFontFeatureValuesDirty) {
    ServoStyleSet* styleSet = mPresShell->StyleSet();
    mFontFeatureValuesLookup = styleSet->BuildFontFeatureValueSet();
    mFontFeatureValuesDirty = false;
  }
}

void nsPresContext::SetVisibleArea(const nsRect& r) {
  if (!r.IsEqualEdges(mVisibleArea)) {
    mVisibleArea = r;
    mSizeForViewportUnits = mVisibleArea.Size();
    if (IsRootContentDocumentCrossProcess()) {
      AdjustSizeForViewportUnits();
    }
    // Visible area does not affect media queries when paginated.
    if (!IsPaginated()) {
      MediaFeatureValuesChanged(
          {mozilla::MediaFeatureChangeReason::ViewportChange});
    }
  }
}

void nsPresContext::SetDynamicToolbarMaxHeight(ScreenIntCoord aHeight) {
  MOZ_ASSERT(IsRootContentDocumentCrossProcess());

  if (mDynamicToolbarMaxHeight == aHeight) {
    return;
  }
  mDynamicToolbarMaxHeight = aHeight;

  AdjustSizeForViewportUnits();

  if (RefPtr<mozilla::PresShell> presShell = mPresShell) {
    // Changing the max height of the dynamic toolbar changes the ICB size, we
    // need to kick a reflow with the current window dimensions since the max
    // height change doesn't change the window dimensions but
    // PresShell::ResizeReflow ends up subtracting the new dynamic toolbar
    // height from the window dimensions and kick a reflow with the proper ICB
    // size.
    nscoord currentWidth, currentHeight;
    presShell->GetViewManager()->GetWindowDimensions(&currentWidth,
                                                     &currentHeight);
    presShell->ResizeReflow(currentWidth, currentHeight,
                            ResizeReflowOptions::NoOption);
  }
}

void nsPresContext::AdjustSizeForViewportUnits() {
  MOZ_ASSERT(IsRootContentDocumentCrossProcess());
  if (mVisibleArea.height == NS_UNCONSTRAINEDSIZE) {
    // Ignore `NS_UNCONSTRAINEDSIZE` since it's a temporary state during a
    // reflow. We will end up calling this function again with a proper size in
    // the same reflow.
    return;
  }

  if (MOZ_UNLIKELY(mVisibleArea.height +
                       NSIntPixelsToAppUnits(mDynamicToolbarMaxHeight,
                                             mCurAppUnitsPerDevPixel) >
                   nscoord_MAX)) {
    MOZ_ASSERT_UNREACHABLE("The dynamic toolbar max height is probably wrong");
    return;
  }

  mSizeForViewportUnits.height =
      mVisibleArea.height +
      NSIntPixelsToAppUnits(mDynamicToolbarMaxHeight, mCurAppUnitsPerDevPixel);
}

void nsPresContext::UpdateDynamicToolbarOffset(ScreenIntCoord aOffset) {
  MOZ_ASSERT(IsRootContentDocumentCrossProcess());
  if (!mPresShell) {
    return;
  }

  if (!HasDynamicToolbar()) {
    return;
  }

  MOZ_ASSERT(-mDynamicToolbarMaxHeight <= aOffset && aOffset <= 0);
  if (mDynamicToolbarHeight == mDynamicToolbarMaxHeight + aOffset) {
    return;
  }

  // Forcibly flush position:fixed elements in the case where the dynamic
  // toolbar is going to be completely hidden or starts to be visible so that
  // %-based style values will be recomputed with the visual viewport size which
  // is including the area covered by the dynamic toolbar.
  if (mDynamicToolbarHeight == 0 || aOffset == -mDynamicToolbarMaxHeight) {
    mPresShell->MarkFixedFramesForReflow(IntrinsicDirty::Resize);
    mPresShell->AddResizeEventFlushObserverIfNeeded();
  }

  mDynamicToolbarHeight = mDynamicToolbarMaxHeight + aOffset;

  if (RefPtr<MobileViewportManager> mvm =
          mPresShell->GetMobileViewportManager()) {
    mvm->UpdateVisualViewportSizeByDynamicToolbar(-aOffset);
  }
}

DynamicToolbarState nsPresContext::GetDynamicToolbarState() const {
  if (!IsRootContentDocumentCrossProcess() || !HasDynamicToolbar()) {
    return DynamicToolbarState::None;
  }

  if (mDynamicToolbarMaxHeight == mDynamicToolbarHeight) {
    return DynamicToolbarState::Expanded;
  } else if (mDynamicToolbarHeight == 0) {
    return DynamicToolbarState::Collapsed;
  }
  return DynamicToolbarState::InTransition;
}

void nsPresContext::SetSafeAreaInsets(const ScreenIntMargin& aSafeAreaInsets) {
  if (mSafeAreaInsets == aSafeAreaInsets) {
    return;
  }
  mSafeAreaInsets = aSafeAreaInsets;

  PostRebuildAllStyleDataEvent(nsChangeHint(0),
                               RestyleHint::RecascadeSubtree());
}

#ifdef DEBUG

void nsPresContext::ValidatePresShellAndDocumentReleation() const {
  NS_ASSERTION(!mPresShell || !mPresShell->GetDocument() ||
                   mPresShell->GetDocument() == mDocument,
               "nsPresContext doesn't have the same document as nsPresShell!");
}

#endif  // #ifdef DEBUG

nsRootPresContext::nsRootPresContext(dom::Document* aDocument,
                                     nsPresContextType aType)
    : nsPresContext(aDocument, aType) {}

nsRootPresContext::~nsRootPresContext() {
  NS_ASSERTION(mRegisteredPlugins.Count() == 0,
               "All plugins should have been unregistered");
  CancelApplyPluginGeometryTimer();
}

void nsRootPresContext::RegisterPluginForGeometryUpdates(nsIContent* aPlugin) {
  mRegisteredPlugins.PutEntry(aPlugin);
}

void nsRootPresContext::UnregisterPluginForGeometryUpdates(
    nsIContent* aPlugin) {
  mRegisteredPlugins.RemoveEntry(aPlugin);
}

void nsRootPresContext::ComputePluginGeometryUpdates(
    nsIFrame* aFrame, nsDisplayListBuilder* aBuilder, nsDisplayList* aList) {
  if (mRegisteredPlugins.Count() == 0) {
    return;
  }

  // Initially make the next state for each plugin descendant of aFrame be
  // "hidden". Plugins that are visible will have their next state set to
  // unhidden by nsDisplayPlugin::ComputeVisibility.
  for (auto iter = mRegisteredPlugins.Iter(); !iter.Done(); iter.Next()) {
    auto f =
        static_cast<nsPluginFrame*>(iter.Get()->GetKey()->GetPrimaryFrame());
    if (!f) {
      NS_WARNING("Null frame in ComputePluginGeometryUpdates");
      continue;
    }
    if (!nsLayoutUtils::IsAncestorFrameCrossDoc(aFrame, f)) {
      // f is not managed by this frame so we should ignore it.
      continue;
    }
    f->SetEmptyWidgetConfiguration();
  }

  if (aBuilder) {
    MOZ_ASSERT(aList);
    nsIFrame* rootFrame = mPresShell->GetRootFrame();

    if (rootFrame && aBuilder->ContainsPluginItem()) {
      aBuilder->SetForPluginGeometry(true);
      // Merging and flattening has already been done and we should not do it
      // again. nsDisplayScroll(Info)Layer doesn't support trying to flatten
      // again.
      aBuilder->SetAllowMergingAndFlattening(false);
      nsRegion region = rootFrame->GetVisualOverflowRectRelativeToSelf();
      // nsDisplayPlugin::ComputeVisibility will automatically set a non-hidden
      // widget configuration for the plugin, if it's visible.
      aList->ComputeVisibilityForRoot(aBuilder, &region);
      aBuilder->SetForPluginGeometry(false);
    }
  }

#ifdef XP_MACOSX
  // We control painting of Mac plugins, so just apply geometry updates now.
  // This is not happening during a paint event.
  ApplyPluginGeometryUpdates();
#else
  if (XRE_IsParentProcess()) {
    InitApplyPluginGeometryTimer();
  }
#endif
}

static void ApplyPluginGeometryUpdatesCallback(nsITimer* aTimer,
                                               void* aClosure) {
  static_cast<nsRootPresContext*>(aClosure)->ApplyPluginGeometryUpdates();
}

void nsRootPresContext::InitApplyPluginGeometryTimer() {
  if (mApplyPluginGeometryTimer) {
    return;
  }

  // We'll apply the plugin geometry updates during the next compositing paint
  // in this presContext (either from PresShell::WillPaintWindow() or from
  // PresShell::DidPaintWindow(), depending on the platform).  But paints might
  // get optimized away if the old plugin geometry covers the invalid region,
  // so set a backup timer to do this too.  We want to make sure this
  // won't fire before our normal paint notifications, if those would
  // update the geometry, so set it for double the refresh driver interval.
  mApplyPluginGeometryTimer = CreateTimer(
      ApplyPluginGeometryUpdatesCallback, "ApplyPluginGeometryUpdatesCallback",
      nsRefreshDriver::DefaultInterval() * 2);
}

void nsRootPresContext::CancelApplyPluginGeometryTimer() {
  if (mApplyPluginGeometryTimer) {
    mApplyPluginGeometryTimer->Cancel();
    mApplyPluginGeometryTimer = nullptr;
  }
}

#ifndef XP_MACOSX

static bool HasOverlap(const LayoutDeviceIntPoint& aOffset1,
                       const nsTArray<LayoutDeviceIntRect>& aClipRects1,
                       const LayoutDeviceIntPoint& aOffset2,
                       const nsTArray<LayoutDeviceIntRect>& aClipRects2) {
  LayoutDeviceIntPoint offsetDelta = aOffset1 - aOffset2;
  for (uint32_t i = 0; i < aClipRects1.Length(); ++i) {
    for (uint32_t j = 0; j < aClipRects2.Length(); ++j) {
      if ((aClipRects1[i] + offsetDelta).Intersects(aClipRects2[j])) {
        return true;
      }
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
static void SortConfigurations(
    nsTArray<nsIWidget::Configuration>* aConfigurations) {
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
    uint32_t i;
    for (i = 0; i + 1 < pluginsToMove.Length(); ++i) {
      nsIWidget::Configuration* config = &pluginsToMove[i];
      bool foundOverlap = false;
      for (uint32_t j = 0; j < pluginsToMove.Length(); ++j) {
        if (i == j) continue;
        LayoutDeviceIntRect bounds = pluginsToMove[j].mChild->GetBounds();
        AutoTArray<LayoutDeviceIntRect, 1> clipRects;
        pluginsToMove[j].mChild->GetWindowClipRegion(&clipRects);
        if (HasOverlap(bounds.TopLeft(), clipRects, config->mBounds.TopLeft(),
                       config->mClipRegion)) {
          foundOverlap = true;
          break;
        }
      }
      if (!foundOverlap) break;
    }
    // Note that we always move the last plugin in pluginsToMove, if we
    // can't find any other plugin to move
    aConfigurations->AppendElement(pluginsToMove[i]);
    pluginsToMove.RemoveElementAt(i);
  }
}

static void PluginGetGeometryUpdate(
    nsTHashtable<nsRefPtrHashKey<nsIContent>>& aPlugins,
    nsTArray<nsIWidget::Configuration>* aConfigurations) {
  for (auto iter = aPlugins.Iter(); !iter.Done(); iter.Next()) {
    auto f =
        static_cast<nsPluginFrame*>(iter.Get()->GetKey()->GetPrimaryFrame());
    if (!f) {
      NS_WARNING("Null frame in PluginGeometryUpdate");
      continue;
    }
    f->GetWidgetConfiguration(aConfigurations);
  }
}

#endif  // #ifndef XP_MACOSX

static void PluginDidSetGeometry(
    nsTHashtable<nsRefPtrHashKey<nsIContent>>& aPlugins) {
  for (auto iter = aPlugins.Iter(); !iter.Done(); iter.Next()) {
    auto f =
        static_cast<nsPluginFrame*>(iter.Get()->GetKey()->GetPrimaryFrame());
    if (!f) {
      NS_WARNING("Null frame in PluginDidSetGeometry");
      continue;
    }
    f->DidSetWidgetGeometry();
  }
}

void nsRootPresContext::ApplyPluginGeometryUpdates() {
#ifndef XP_MACOSX
  CancelApplyPluginGeometryTimer();

  nsTArray<nsIWidget::Configuration> configurations;
  PluginGetGeometryUpdate(mRegisteredPlugins, &configurations);
  // Walk mRegisteredPlugins and ask each plugin for its configuration
  if (!configurations.IsEmpty()) {
    nsIWidget* widget = configurations[0].mChild->GetParent();
    NS_ASSERTION(widget, "Plugins must have a parent window");
    SortConfigurations(&configurations);
    widget->ConfigureChildren(configurations);
  }
#endif  // #ifndef XP_MACOSX

  PluginDidSetGeometry(mRegisteredPlugins);
}

void nsRootPresContext::CollectPluginGeometryUpdates(
    LayerManager* aLayerManager) {
#ifndef XP_MACOSX
  // Collect and pass plugin widget configurations down to the compositor
  // for transmission to the chrome process.
  NS_ASSERTION(aLayerManager, "layer manager is invalid!");
  mozilla::layers::ClientLayerManager* clm =
      aLayerManager->AsClientLayerManager();

  nsTArray<nsIWidget::Configuration> configurations;
  // If there aren't any plugins to configure, clear the plugin data cache
  // in the layer system.
  if (!mRegisteredPlugins.Count() && clm) {
    clm->StorePluginWidgetConfigurations(configurations);
    return;
  }
  PluginGetGeometryUpdate(mRegisteredPlugins, &configurations);
  if (configurations.IsEmpty()) {
    PluginDidSetGeometry(mRegisteredPlugins);
    return;
  }
  SortConfigurations(&configurations);
  if (clm) {
    clm->StorePluginWidgetConfigurations(configurations);
  }
  PluginDidSetGeometry(mRegisteredPlugins);
#endif  // #ifndef XP_MACOSX
}

void nsRootPresContext::AddWillPaintObserver(nsIRunnable* aRunnable) {
  if (!mWillPaintFallbackEvent.IsPending()) {
    mWillPaintFallbackEvent = new RunWillPaintObservers(this);
    Document()->Dispatch(TaskCategory::Other,
                         do_AddRef(mWillPaintFallbackEvent));
  }
  mWillPaintObservers.AppendElement(aRunnable);
}

/**
 * Run all runnables that need to get called before the next paint.
 */
void nsRootPresContext::FlushWillPaintObservers() {
  mWillPaintFallbackEvent = nullptr;
  nsTArray<nsCOMPtr<nsIRunnable>> observers;
  observers.SwapElements(mWillPaintObservers);
  for (uint32_t i = 0; i < observers.Length(); ++i) {
    observers[i]->Run();
  }
}

size_t nsRootPresContext::SizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  return nsPresContext::SizeOfExcludingThis(aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mRegisteredPlugins
  // - mWillPaintObservers
  // - mWillPaintFallbackEvent
}
