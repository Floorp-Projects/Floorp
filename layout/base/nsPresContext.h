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

#ifndef nsPresContext_h___
#define nsPresContext_h___

#include "nsISupports.h"
#include "nsColor.h"
#include "nsCoord.h"
#include "nsAString.h"
#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsRect.h"
#include "nsIDeviceContext.h"
#include "nsFont.h"
#include "nsIWeakReference.h"
#include "nsITheme.h"
#include "nsILanguageAtomService.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsCRT.h"
#include "nsIPrintSettings.h"
#include "FramePropertyTable.h"
#include "nsGkAtoms.h"
#include "nsIDocument.h"
#include "nsRefPtrHashtable.h"
#include "nsCycleCollectionParticipant.h"
#include "nsChangeHint.h"
// This also pulls in gfxTypes.h, which we cannot include directly.
#include "gfxRect.h"
#include "nsRegion.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"
#include "nsIWidget.h"
#include "mozilla/TimeStamp.h"
#include "nsIContent.h"

class nsImageLoader;
#ifdef IBMBIDI
class nsBidiPresUtils;
#endif // IBMBIDI

struct nsRect;

class imgIRequest;

class nsIFontMetrics;
class nsIFrame;
class nsFrameManager;
class nsILinkHandler;
class nsStyleContext;
class nsIAtom;
class nsIEventStateManager;
class nsIURI;
class nsILookAndFeel;
class nsICSSPseudoComparator;
class nsIAtom;
struct nsStyleBackground;
struct nsStyleBorder;
class nsIRunnable;
class gfxUserFontSet;
class nsUserFontSet;
struct nsFontFaceRuleContainer;
class nsObjectFrame;
class nsTransitionManager;
class nsRefreshDriver;
class imgIContainer;

#ifdef MOZ_REFLOW_PERF
class nsIRenderingContext;
#endif

enum nsWidgetType {
  eWidgetType_Button  	= 1,
  eWidgetType_Checkbox	= 2,
  eWidgetType_Radio			= 3,
  eWidgetType_Text			= 4
};

enum nsLanguageSpecificTransformType {
  eLanguageSpecificTransformType_Unknown = -1,
  eLanguageSpecificTransformType_None = 0,
  eLanguageSpecificTransformType_Japanese
};

// supported values for cached bool types
enum nsPresContext_CachedBoolPrefType {
  kPresContext_UseDocumentColors = 1,
  kPresContext_UseDocumentFonts,
  kPresContext_UnderlineLinks
};

// supported values for cached integer pref types
enum nsPresContext_CachedIntPrefType {
  kPresContext_MinimumFontSize = 1,
  kPresContext_ScrollbarSide,
  kPresContext_BidiDirection
};

// IDs for the default variable and fixed fonts (not to be changed, see nsFont.h)
// To be used for Get/SetDefaultFont(). The other IDs in nsFont.h are also supported.
const PRUint8 kPresContext_DefaultVariableFont_ID = 0x00; // kGenericFont_moz_variable
const PRUint8 kPresContext_DefaultFixedFont_ID    = 0x01; // kGenericFont_moz_fixed

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
    PRUint32 mFlags;
  };

  nsTArray<Request> mRequests;
};

/* Used by nsPresContext::HasAuthorSpecifiedRules */
#define NS_AUTHOR_SPECIFIED_BACKGROUND      (1 << 0)
#define NS_AUTHOR_SPECIFIED_BORDER          (1 << 1)
#define NS_AUTHOR_SPECIFIED_PADDING         (1 << 2)

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

  nsPresContext(nsIDocument* aDocument, nsPresContextType aType) NS_HIDDEN;

  /**
   * Initialize the presentation context from a particular device.
   */
  NS_HIDDEN_(nsresult) Init(nsIDeviceContext* aDeviceContext);

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
   * Return the presentation context for the root of the view manager
   * hierarchy that contains this presentation context, or nsnull if it can't
   * be found (e.g. it's detached).
   */
  nsRootPresContext* GetRootPresContext();
  virtual PRBool IsRoot() { return PR_FALSE; }

  nsIDocument* Document() const
  {
      NS_ASSERTION(!mShell || !mShell->GetDocument() ||
                   mShell->GetDocument() == mDocument,
                   "nsPresContext doesn't have the same document as nsPresShell!");
      return mDocument;
  }

#ifdef _IMPL_NS_LAYOUT
  nsStyleSet* StyleSet() { return GetPresShell()->StyleSet(); }

  nsFrameManager* FrameManager()
    { return GetPresShell()->FrameManager(); } 

  nsTransitionManager* TransitionManager() { return mTransitionManager; }

  nsRefreshDriver* RefreshDriver() { return mRefreshDriver; }
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

  void MediaFeatureValuesChanged(PRBool aCallerWillRebuildStyleData);
  void PostMediaFeatureValuesChangedEvent();
  NS_HIDDEN_(void) HandleMediaFeatureValuesChangedEvent();
  void FlushPendingMediaFeatureValuesChanged() {
    if (mPendingMediaFeatureValuesChanged)
      MediaFeatureValuesChanged(PR_FALSE);
  }

  /**
   * Access compatibility mode for this context.  This is the same as
   * our document's compatibility mode.
   */
  nsCompatibility CompatibilityMode() const {
    return Document()->GetCompatibilityMode();
  }
  /**
   * Notify the context that the document's compatibility mode has changed
   */
  NS_HIDDEN_(void) CompatibilityModeChanged();

  /**
   * Access the image animation mode for this context
   */
  PRUint16     ImageAnimationMode() const { return mImageAnimationMode; }
  virtual NS_HIDDEN_(void) SetImageAnimationModeExternal(PRUint16 aMode);
  NS_HIDDEN_(void) SetImageAnimationModeInternal(PRUint16 aMode);
#ifdef _IMPL_NS_LAYOUT
  void SetImageAnimationMode(PRUint16 aMode)
  { SetImageAnimationModeInternal(aMode); }
#else
  void SetImageAnimationMode(PRUint16 aMode)
  { SetImageAnimationModeExternal(aMode); }
#endif

  /**
   * Get cached look and feel service.  This is faster than obtaining it
   * through the service manager.
   */
  nsILookAndFeel* LookAndFeel() { return mLookAndFeel; }

  /** 
   * Get medium of presentation
   */
  nsIAtom* Medium() { return mMedium; }

  void* AllocateFromShell(size_t aSize)
  {
    if (mShell)
      return mShell->AllocateMisc(aSize);
    return nsnull;
  }

  void FreeToShell(size_t aSize, void* aFreeChunk)
  {
    NS_ASSERTION(mShell, "freeing after shutdown");
    if (mShell)
      mShell->FreeMisc(aSize, aFreeChunk);
  }

  /**
   * Get the font metrics for a given font.
   *
   * If aUseUserFontSet is false, don't build or use the user font set.
   * This is intended only for nsRuleNode::CalcLengthWithInitialFont
   * (which is used from media query matching, which is in turn called
   * when building the user font set).
   */
  NS_HIDDEN_(already_AddRefed<nsIFontMetrics>)
  GetMetricsFor(const nsFont& aFont, PRBool aUseUserFontSet = PR_TRUE);

  /**
   * Get the default font corresponding to the given ID.  This object is
   * read-only, you must copy the font to modify it.
   * 
   * When aFontID is kPresContext_DefaultVariableFontID or
   * kPresContext_DefaultFixedFontID (which equals
   * kGenericFont_moz_fixed, which is used for the -moz-fixed generic),
   * the nsFont returned has its name as a CSS generic family (serif or
   * sans-serif for the former, monospace for the latter), and its size
   * as the default font size for variable or fixed fonts for the pres
   * context's language group.
   *
   * For aFontID corresponds to a CSS Generic, the nsFont returned has
   * its name as the name or names of the fonts in the user's
   * preferences for the given generic and the pres context's language
   * group, and its size set to the default variable font size.
   */
  NS_HIDDEN_(const nsFont*) GetDefaultFont(PRUint8 aFontID) const;

  /** Get a cached boolean pref, by its type */
  // *  - initially created for bugs 31816, 20760, 22963
  PRBool GetCachedBoolPref(nsPresContext_CachedBoolPrefType aPrefType) const
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

    return PR_FALSE;
  }

  /** Get a cached integer pref, by its type */
  // *  - initially created for bugs 30910, 61883, 74186, 84398
  PRInt32 GetCachedIntPref(nsPresContext_CachedIntPrefType aPrefType) const
  {
    // If called with a constant parameter, the compiler should optimize
    // this switch statement away.
    switch (aPrefType) {
    case kPresContext_MinimumFontSize:
      return mMinimumFontSize;
    case kPresContext_ScrollbarSide:
      return mPrefScrollbarSide;
    case kPresContext_BidiDirection:
      return mPrefBidiDirection;
    default:
      NS_ERROR("invalid arg passed to GetCachedIntPref");
    }

    return PR_FALSE;
  }

  /**
   * Access Nav's magic font scaler value
   */
  PRInt32 FontScaler() const { return mFontScaler; }

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

  PRBool GetUseFocusColors() const { return mUseFocusColors; }
  PRUint8 FocusRingWidth() const { return mFocusRingWidth; }
  PRBool GetFocusRingOnAnything() const { return mFocusRingOnAnything; }
  PRUint8 GetFocusRingStyle() const { return mFocusRingStyle; }

  /**
   * The types of image load types that the pres context needs image
   * loaders to track invalidation for.
   */
  enum ImageLoadType {
    BACKGROUND_IMAGE,
    BORDER_IMAGE,
    IMAGE_LOAD_TYPE_COUNT
  };

  /**
   * Set the list of image loaders that track invalidation for a
   * specific frame and type of image.  This list will replace any
   * previous list for that frame and image type (and null will remove
   * any previous list).
   */
  NS_HIDDEN_(void) SetImageLoaders(nsIFrame* aTargetFrame,
                                   ImageLoadType aType,
                                   nsImageLoader* aImageLoaders);

  /**
   * Make an appropriate SetImageLoaders call (including potentially
   * with null aImageLoaders) given that aFrame draws its background
   * based on aStyleBackground.
   */
  NS_HIDDEN_(void) SetupBackgroundImageLoaders(nsIFrame* aFrame,
                                               const nsStyleBackground*
                                                 aStyleBackground);

  /**
   * Make an appropriate SetImageLoaders call (including potentially
   * with null aImageLoaders) given that aFrame draws its border
   * based on aStyleBorder.
   */
  NS_HIDDEN_(void) SetupBorderImageLoaders(nsIFrame* aFrame,
                                           const nsStyleBorder* aStyleBorder);

  /**
   * This method is called when a frame is being destroyed to
   * ensure that the image loads get disassociated from the prescontext
   */
  NS_HIDDEN_(void) StopImagesFor(nsIFrame* aTargetFrame);

  NS_HIDDEN_(void) SetContainer(nsISupports* aContainer);

  virtual NS_HIDDEN_(already_AddRefed<nsISupports>) GetContainerExternal() const;
  NS_HIDDEN_(already_AddRefed<nsISupports>) GetContainerInternal() const;
#ifdef _IMPL_NS_LAYOUT
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
    if (!r.IsExactEqual(mVisibleArea)) {
      mVisibleArea = r;
      // Visible area does not affect media queries when paginated.
      if (!IsPaginated() && HasCachedStyleData())
        PostMediaFeatureValuesChangedEvent();
    }
  }

  /**
   * Return true if this presentation context is a paginated
   * context.
   */
  PRBool IsPaginated() const { return mPaginated; }
  
  /**
   * Sets whether the presentation context can scroll for a paginated
   * context.
   */
  NS_HIDDEN_(void) SetPaginatedScrolling(PRBool aResult);

  /**
   * Return true if this presentation context can scroll for paginated
   * context.
   */
  PRBool HasPaginatedScrolling() const { return mCanPaginatedScroll; }

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
  PRBool IsRootPaginatedDocument() { return mIsRootPaginatedDocument; }
  void SetIsRootPaginatedDocument(PRBool aIsRootPaginatedDocument)
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

  nsIDeviceContext* DeviceContext() { return mDeviceContext; }
  nsIEventStateManager* EventStateManager() { return mEventManager; }
  nsIAtom* GetLanguageFromCharset() { return mLanguage; }

  float TextZoom() { return mTextZoom; }
  void SetTextZoom(float aZoom) {
    if (aZoom == mTextZoom)
      return;

    mTextZoom = aZoom;
    if (HasCachedStyleData()) {
      // Media queries could have changed since we changed the meaning
      // of 'em' units in them.
      MediaFeatureValuesChanged(PR_TRUE);
      RebuildAllStyleData(NS_STYLE_HINT_REFLOW);
    }
  }

  float GetFullZoom() { return mFullZoom; }
  void SetFullZoom(float aZoom);

  nscoord GetAutoQualityMinFontSize() {
    return DevPixelsToAppUnits(mAutoQualityMinFontSizePixelsPref);
  }
  
  static PRInt32 AppUnitsPerCSSPixel() { return nsIDeviceContext::AppUnitsPerCSSPixel(); }
  PRInt32 AppUnitsPerDevPixel() const  { return mDeviceContext->AppUnitsPerDevPixel(); }
  static PRInt32 AppUnitsPerCSSInch() { return nsIDeviceContext::AppUnitsPerCSSInch(); }

  static nscoord CSSPixelsToAppUnits(PRInt32 aPixels)
  { return NSIntPixelsToAppUnits(aPixels,
                                 nsIDeviceContext::AppUnitsPerCSSPixel()); }

  static nscoord CSSPixelsToAppUnits(float aPixels)
  { return NSFloatPixelsToAppUnits(aPixels,
             float(nsIDeviceContext::AppUnitsPerCSSPixel())); }

  static PRInt32 AppUnitsToIntCSSPixels(nscoord aAppUnits)
  { return NSAppUnitsToIntPixels(aAppUnits,
             float(nsIDeviceContext::AppUnitsPerCSSPixel())); }

  static float AppUnitsToFloatCSSPixels(nscoord aAppUnits)
  { return NSAppUnitsToFloatPixels(aAppUnits,
             float(nsIDeviceContext::AppUnitsPerCSSPixel())); }

  nscoord DevPixelsToAppUnits(PRInt32 aPixels) const
  { return NSIntPixelsToAppUnits(aPixels,
                                 mDeviceContext->AppUnitsPerDevPixel()); }

  PRInt32 AppUnitsToDevPixels(nscoord aAppUnits) const
  { return NSAppUnitsToIntPixels(aAppUnits,
             float(mDeviceContext->AppUnitsPerDevPixel())); }

  PRInt32 CSSPixelsToDevPixels(PRInt32 aPixels)
  { return AppUnitsToDevPixels(CSSPixelsToAppUnits(aPixels)); }

  float CSSPixelsToDevPixels(float aPixels)
  {
    return NSAppUnitsToFloatPixels(CSSPixelsToAppUnits(aPixels),
                                   float(mDeviceContext->AppUnitsPerDevPixel()));
  }

  PRInt32 DevPixelsToIntCSSPixels(PRInt32 aPixels)
  { return AppUnitsToIntCSSPixels(DevPixelsToAppUnits(aPixels)); }

  float DevPixelsToFloatCSSPixels(PRInt32 aPixels)
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
      nsIDeviceContext::AppUnitsPerCSSInch() * NS_TWIPS_TO_INCHES(aTwips)); }

  // Margin-specific version, since they often need TwipsToAppUnits
  static nsMargin CSSTwipsToAppUnits(const nsIntMargin &marginInTwips)
  { return nsMargin(CSSTwipsToAppUnits(float(marginInTwips.left)), 
                    CSSTwipsToAppUnits(float(marginInTwips.top)),
                    CSSTwipsToAppUnits(float(marginInTwips.right)),
                    CSSTwipsToAppUnits(float(marginInTwips.bottom))); }

  static nscoord CSSPointsToAppUnits(float aPoints)
  { return NSToCoordRound(aPoints * nsIDeviceContext::AppUnitsPerCSSInch() /
                          POINTS_PER_INCH_FLOAT); }

  nscoord RoundAppUnitsToNearestDevPixels(nscoord aAppUnits) const
  { return DevPixelsToAppUnits(AppUnitsToDevPixels(aAppUnits)); }

  struct ScrollbarStyles {
    // Always one of NS_STYLE_OVERFLOW_SCROLL, NS_STYLE_OVERFLOW_HIDDEN,
    // or NS_STYLE_OVERFLOW_AUTO.
    PRUint8 mHorizontal, mVertical;
    ScrollbarStyles(PRUint8 h, PRUint8 v) : mHorizontal(h), mVertical(v) {}
    ScrollbarStyles() {}
    PRBool operator==(const ScrollbarStyles& aStyles) const {
      return aStyles.mHorizontal == mHorizontal && aStyles.mVertical == mVertical;
    }
    PRBool operator!=(const ScrollbarStyles& aStyles) const {
      return aStyles.mHorizontal != mHorizontal || aStyles.mVertical != mVertical;
    }
  };
  void SetViewportOverflowOverride(PRUint8 aX, PRUint8 aY)
  {
    mViewportStyleOverflow.mHorizontal = aX;
    mViewportStyleOverflow.mVertical = aY;
  }
  ScrollbarStyles GetViewportOverflowOverride()
  {
    return mViewportStyleOverflow;
  }

  /**
   * Set and get methods for controlling the background drawing
  */
  PRBool GetBackgroundImageDraw() const { return mDrawImageBackground; }
  void   SetBackgroundImageDraw(PRBool aCanDraw)
  {
    NS_ASSERTION(!(aCanDraw & ~1), "Value must be true or false");
    mDrawImageBackground = aCanDraw;
  }

  PRBool GetBackgroundColorDraw() const { return mDrawColorBackground; }
  void   SetBackgroundColorDraw(PRBool aCanDraw)
  {
    NS_ASSERTION(!(aCanDraw & ~1), "Value must be true or false");
    mDrawColorBackground = aCanDraw;
  }

#ifdef IBMBIDI
  /**
   *  Check if bidi enabled (set depending on the presence of RTL
   *  characters or when default directionality is RTL).
   *  If enabled, we should apply the Unicode Bidi Algorithm
   *
   *  @lina 07/12/2000
   */
  virtual PRBool BidiEnabledExternal() const { return BidiEnabledInternal(); }
  PRBool BidiEnabledInternal() const { return Document()->GetBidiEnabled(); }
#ifdef _IMPL_NS_LAYOUT
  PRBool BidiEnabled() const { return BidiEnabledInternal(); }
#else
  PRBool BidiEnabled() const { return BidiEnabledExternal(); }
#endif

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
  void SetVisualMode(PRBool aIsVisual)
  {
    NS_ASSERTION(!(aIsVisual & ~1), "Value must be true or false");
    mIsVisual = aIsVisual;
  }

  /**
   *  Check whether the content should be treated as visual.
   *
   *  @lina 05/02/2000
   */
  PRBool IsVisualMode() const { return mIsVisual; }

//Mohamed

  /**
   * Get a Bidi presentation utilities object
   */
  NS_HIDDEN_(nsBidiPresUtils*) GetBidiUtils();

  /**
   * Set the Bidi options for the presentation context
   */  
  NS_HIDDEN_(void) SetBidi(PRUint32 aBidiOptions,
                           PRBool aForceRestyle = PR_FALSE);

  /**
   * Get the Bidi options for the presentation context
   * Not inline so consumers of nsPresContext are not forced to
   * include nsIDocument.
   */  
  NS_HIDDEN_(PRUint32) GetBidi() const;

  PRUint32 GetBidiMemoryUsed();
#else
  PRUint32 GetBidiMemoryUsed() { return 0; }
#endif // IBMBIDI

  /**
   * Render only Selection
   */
  void SetIsRenderingOnlySelection(PRBool aResult)
  {
    NS_ASSERTION(!(aResult & ~1), "Value must be true or false");
    mIsRenderingOnlySelection = aResult;
  }

  PRBool IsRenderingOnlySelection() const { return mIsRenderingOnlySelection; }

  NS_HIDDEN_(PRBool) IsTopLevelWindowInactive();

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
  NS_HIDDEN_(PRBool) EnsureVisible();
  
#ifdef MOZ_REFLOW_PERF
  NS_HIDDEN_(void) CountReflows(const char * aName,
                                nsIFrame * aFrame);
#endif

  /**
   * This table maps border-width enums 'thin', 'medium', 'thick'
   * to actual nscoord values.
   */
  const nscoord* GetBorderWidthTable() { return mBorderWidthTable; }

  PRBool IsDynamic() { return (mType == eContext_PageLayout || mType == eContext_Galley); }
  PRBool IsScreen() { return (mMedium == nsGkAtoms::screen ||
                              mType == eContext_PageLayout ||
                              mType == eContext_PrintPreview); }

  // Is this presentation in a chrome docshell?
  PRBool IsChrome() const
  {
    return mIsChromeIsCached ? mIsChrome : IsChromeSlow();
  }

  virtual void InvalidateIsChromeCacheExternal();
  void InvalidateIsChromeCacheInternal() { mIsChromeIsCached = PR_FALSE; }
#ifdef _IMPL_NS_LAYOUT
  void InvalidateIsChromeCache()
  { InvalidateIsChromeCacheInternal(); }
#else
  void InvalidateIsChromeCache()
  { InvalidateIsChromeCacheExternal(); }
#endif

  // Public API for native theme code to get style internals.
  virtual PRBool HasAuthorSpecifiedRules(nsIFrame *aFrame, PRUint32 ruleTypeMask) const;

  // Is it OK to let the page specify colors and backgrounds?
  PRBool UseDocumentColors() const {
    return GetCachedBoolPref(kPresContext_UseDocumentColors) || IsChrome();
  }

  PRBool           SupressingResizeReflow() const { return mSupressResizeReflow; }
  
  virtual NS_HIDDEN_(gfxUserFontSet*) GetUserFontSetExternal();
  NS_HIDDEN_(gfxUserFontSet*) GetUserFontSetInternal();
#ifdef _IMPL_NS_LAYOUT
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
  PRBool EnsureSafeToHandOutCSSRules();

  void NotifyInvalidation(const nsRect& aRect, PRUint32 aFlags);
  void NotifyInvalidateForScrolling(const nsRegion& aBlitRegion,
                                    const nsRegion& aInvalidateRegion);
  void FireDOMPaintEvent();
  PRBool IsDOMPaintEventPending() {
    return !mInvalidateRequests.mRequests.IsEmpty();
  }

  void ClearMozAfterPaintEvents() {
    mInvalidateRequests.mRequests.Clear();
  }

  PRBool IsProcessingRestyles() const {
    return mProcessingRestyles;
  }

  void SetProcessingRestyles(PRBool aProcessing) {
    NS_ASSERTION(aProcessing != PRBool(mProcessingRestyles),
                 "should never nest");
    mProcessingRestyles = aProcessing;
  }

  PRBool IsProcessingAnimationStyleChange() const {
    return mProcessingAnimationStyleChange;
  }

  void SetProcessingAnimationStyleChange(PRBool aProcessing) {
    NS_ASSERTION(aProcessing != PRBool(mProcessingAnimationStyleChange),
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
  void ReflowStarted(PRBool aInterruptible);

  /**
   * A class that can be used to temporarily disable reflow interruption.
   */
  class InterruptPreventer;
  friend class InterruptPreventer;
  class NS_STACK_CLASS InterruptPreventer {
  public:
    InterruptPreventer(nsPresContext* aCtx) :
      mCtx(aCtx),
      mInterruptsEnabled(aCtx->mInterruptsEnabled),
      mHasPendingInterrupt(aCtx->mHasPendingInterrupt)
    {
      mCtx->mInterruptsEnabled = PR_FALSE;
      mCtx->mHasPendingInterrupt = PR_FALSE;
    }
    ~InterruptPreventer() {
      mCtx->mInterruptsEnabled = mInterruptsEnabled;
      mCtx->mHasPendingInterrupt = mHasPendingInterrupt;
    }

  private:
    nsPresContext* mCtx;
    PRBool mInterruptsEnabled;
    PRBool mHasPendingInterrupt;
  };
    
  /**
   * Check for interrupts. This may return true if a pending event is
   * detected. Once it has returned true, it will keep returning true
   * until ReflowStarted is called. In all cases where this returns true,
   * the passed-in frame (which should be the frame whose reflow will be
   * interrupted if true is returned) will be passed to
   * nsIPresShell::FrameNeedsToContinueReflow.
   */
  PRBool CheckForInterrupt(nsIFrame* aFrame);
  /**
   * Returns true if CheckForInterrupt has returned true since the last
   * ReflowStarted call. Cannot itself trigger an interrupt check.
   */
  PRBool HasPendingInterrupt() { return mHasPendingInterrupt; }

  /**
   * If we have a presshell, and if the given content's current
   * document is the same as our presshell's document, return the
   * content's primary frame.  Otherwise, return null.  Only use this
   * if you care about which presshell the primary frame is in.
   */
  nsIFrame* GetPrimaryFrameFor(nsIContent* aContent) {
    NS_PRECONDITION(aContent, "Don't do that");
    if (GetPresShell() &&
        GetPresShell()->GetDocument() == aContent->GetCurrentDoc()) {
      return aContent->GetPrimaryFrame();
    }
    return nsnull;
  }

  void NotifyDestroyingFrame(nsIFrame* aFrame)
  {
    PropertyTable()->DeleteAllFor(aFrame);
  }
  inline void ForgetUpdatePluginGeometryFrame(nsIFrame* aFrame);

  void SetContainsUpdatePluginGeometryFrame(PRBool aValue)
  {
    mContainsUpdatePluginGeometryFrame = aValue;
  }

  PRBool MayHaveFixedBackgroundFrames() { return mMayHaveFixedBackgroundFrames; }
  void SetHasFixedBackgroundFrame() { mMayHaveFixedBackgroundFrames = PR_TRUE; }

  PRUint32 EstimateMemoryUsed() {
    PRUint32 result = 0;

    result += sizeof(nsPresContext);
    result += GetBidiMemoryUsed();

    return result;
  }

  PRBool IsRootContentDocument();

protected:
  friend class nsRunnableMethod<nsPresContext>;
  NS_HIDDEN_(void) ThemeChangedInternal();
  NS_HIDDEN_(void) SysColorChangedInternal();

  NS_HIDDEN_(void) SetImgAnimations(nsIContent *aParent, PRUint16 aMode);
#ifdef MOZ_SMIL
  NS_HIDDEN_(void) SetSMILAnimations(nsIDocument *aDoc, PRUint16 aNewMode,
                                     PRUint16 aOldMode);
#endif // MOZ_SMIL
  NS_HIDDEN_(void) GetDocumentColorPreferences();

  NS_HIDDEN_(void) PreferenceChanged(const char* aPrefName);
  static NS_HIDDEN_(int) PrefChangedCallback(const char*, void*);

  NS_HIDDEN_(void) UpdateAfterPreferencesChanged();
  static NS_HIDDEN_(void) PrefChangedUpdateTimerCallback(nsITimer *aTimer, void *aClosure);

  NS_HIDDEN_(void) GetUserPreferences();
  NS_HIDDEN_(void) GetFontPreferences();

  NS_HIDDEN_(void) UpdateCharSet(const nsAFlatCString& aCharSet);

  PRBool MayHavePaintEventListener();
  void NotifyInvalidateRegion(const nsRegion& aRegion, nsPoint aOffset,
                              PRUint32 aFlags);

  void HandleRebuildUserFontSet() {
    mPostedFlushUserFontSet = PR_FALSE;
    FlushUserFontSet();
  }

  PRBool HavePendingInputEvent();

  // Can't be inline because we can't include nsStyleSet.h.
  PRBool HasCachedStyleData();

  PRBool IsChromeSlow() const;

  // IMPORTANT: The ownership implicit in the following member variables
  // has been explicitly checked.  If you add any members to this class,
  // please make the ownership explicit (pinkerton, scc).
  
  nsPresContextType     mType;
  nsIPresShell*         mShell;         // [WEAK]
  nsCOMPtr<nsIDocument> mDocument;
  nsIDeviceContext*     mDeviceContext; // [STRONG] could be weak, but
                                        // better safe than sorry.
                                        // Cannot reintroduce cycles
                                        // since there is no dependency
                                        // from gfx back to layout.
  nsIEventStateManager* mEventManager;  // [STRONG]
  nsILookAndFeel*       mLookAndFeel;   // [STRONG]
  nsRefPtr<nsRefreshDriver> mRefreshDriver;
  nsRefPtr<nsTransitionManager> mTransitionManager;
  nsIAtom*              mMedium;        // initialized by subclass ctors;
                                        // weak pointer to static atom

  nsILinkHandler*       mLinkHandler;   // [WEAK]

  // Formerly mLangGroup; moving from charset-oriented langGroup to
  // maintaining actual language settings everywhere (see bug 524107).
  // This may in fact hold a langGroup such as x-western rather than
  // a specific language, however (e.g, if it is inferred from the
  // charset rather than explicitly specified as a lang attribute).
  nsIAtom*              mLanguage;      // [STRONG]

  nsRefPtrHashtable<nsVoidPtrHashKey, nsImageLoader>
                        mImageLoaders[IMAGE_LOAD_TYPE_COUNT];

  nsWeakPtr             mContainer;

  float                 mTextZoom;      // Text zoom, defaults to 1.0
  float                 mFullZoom;      // Page zoom, defaults to 1.0

  PRInt32               mCurAppUnitsPerDevPixel;
  PRInt32               mAutoQualityMinFontSizePixelsPref;

#ifdef IBMBIDI
  nsBidiPresUtils*      mBidiUtils;
#endif

  nsCOMPtr<nsITheme> mTheme;
  nsCOMPtr<nsILanguageAtomService> mLangService;
  nsCOMPtr<nsIPrintSettings> mPrintSettings;
  nsCOMPtr<nsITimer>    mPrefChangedTimer;

  FramePropertyTable    mPropertyTable;

  nsInvalidateRequestList mInvalidateRequests;

  // container for per-context fonts (downloadable, SVG, etc.)
  nsUserFontSet*        mUserFontSet;
  // The list of @font-face rules that we put into mUserFontSet
  nsTArray<nsFontFaceRuleContainer> mFontFaceRules;
  
  PRInt32               mFontScaler;
  nscoord               mMinimumFontSize;

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

  ScrollbarStyles       mViewportStyleOverflow;
  PRUint8               mFocusRingWidth;

  PRUint16              mImageAnimationMode;
  PRUint16              mImageAnimationModePref;

  nsFont                mDefaultVariableFont;
  nsFont                mDefaultFixedFont;
  nsFont                mDefaultSerifFont;
  nsFont                mDefaultSansSerifFont;
  nsFont                mDefaultMonospaceFont;
  nsFont                mDefaultCursiveFont;
  nsFont                mDefaultFantasyFont;

  nscoord               mBorderWidthTable[3];

  PRUint32              mInterruptChecksToSkip;

  mozilla::TimeStamp    mReflowStartTime;

  unsigned              mHasPendingInterrupt : 1;
  unsigned              mInterruptsEnabled : 1;
  unsigned              mUseDocumentFonts : 1;
  unsigned              mUseDocumentColors : 1;
  unsigned              mUnderlineLinks : 1;
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
  unsigned              mPendingMediaFeatureValuesChanged : 1;
  unsigned              mPrefChangePendingNeedsReflow : 1;
  unsigned              mMayHaveFixedBackgroundFrames : 1;

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

  unsigned              mContainsUpdatePluginGeometryFrame : 1;

  // Cache whether we are chrome or not because it is expensive.  
  // mIsChromeIsCached tells us if mIsChrome is valid or we need to get the
  // value the slow way.
  mutable unsigned      mIsChromeIsCached : 1;
  mutable unsigned      mIsChrome : 1;

#ifdef DEBUG
  PRBool                mInitialized;
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

#ifdef DEBUG
private:
  friend struct nsAutoLayoutPhase;
  PRUint32 mLayoutPhaseCount[eLayoutPhase_COUNT];
public:
  PRUint32 LayoutPhaseCount(nsLayoutPhase aPhase) {
    return mLayoutPhaseCount[aPhase];
  }
#endif

};

class nsRootPresContext : public nsPresContext {
public:
  nsRootPresContext(nsIDocument* aDocument, nsPresContextType aType) NS_HIDDEN;
  virtual ~nsRootPresContext();

  /**
   * Registers a plugin to receive geometry updates (position and clip
   * region) so it can update its widget.
   * Callers must call UnregisterPluginForGeometryUpdates before
   * the aPlugin frame is destroyed.
   */
  void RegisterPluginForGeometryUpdates(nsObjectFrame* aPlugin);
  /**
   * Stops a plugin receiving geometry updates (position and clip
   * region). If the plugin was not already registered, this does
   * nothing.
   */
  void UnregisterPluginForGeometryUpdates(nsObjectFrame* aPlugin);

  /**
   * Iterate through all plugins that are registered for geometry updates
   * and update their position and clip region to match the current frame
   * tree.
   */
  void UpdatePluginGeometry();

  /**
   * Iterate through all plugins that are registered for geometry updates
   * and compute their position and clip region according to the
   * current frame tree. Only frames at or under aChangedRoot can have
   * changed their geometry. The computed positions and clip regions are
   * appended to aConfigurations.
   */
  void GetPluginGeometryUpdates(nsIFrame* aChangedRoot,
                                nsTArray<nsIWidget::Configuration>* aConfigurations);

  /**
   * When all geometry updates have been applied, call this function
   * in case the nsObjectFrames have work to do after the widgets
   * have been updated.
   */
  void DidApplyPluginGeometryUpdates();

  virtual PRBool IsRoot() { return PR_TRUE; }

  /**
   * This method is called off an event to force the plugin geometry to
   * be updated. First we try to paint, since updating plugin geometry
   * during paint is best for keeping plugins in sync with content.
   * But we also force geometry updates in case painting doesn't work.
   */
  void ForcePluginGeometryUpdate();

  /**
   * Call this after reflow and scrolling to ensure that the geometry
   * of any windowed plugins is updated. aFrame is the root of the
   * frame subtree whose geometry has changed.
   */
  void RequestUpdatePluginGeometry(nsIFrame* aFrame);

  /**
   * Call this when a frame is being destroyed and
   * mContainsUpdatePluginGeometryFrame is set in the frame's prescontext.
   */
  void RootForgetUpdatePluginGeometryFrame(nsIFrame* aFrame);

private:
  nsTHashtable<nsPtrHashKey<nsObjectFrame> > mRegisteredPlugins;
  // if mNeedsToUpdatePluginGeometry is set, then this is the frame to
  // use as the root of the subtree to search for plugin updates, or
  // null to use the root frame of this prescontext
  nsIFrame* mUpdatePluginGeometryForFrame;
  PRPackedBool mNeedsToUpdatePluginGeometry;
};

inline void
nsPresContext::ForgetUpdatePluginGeometryFrame(nsIFrame* aFrame)
{
  if (mContainsUpdatePluginGeometryFrame) {
    nsRootPresContext* rootPC = GetRootPresContext();
    if (rootPC) {
      rootPC->RootForgetUpdatePluginGeometryFrame(aFrame);
    }
  }
}

#ifdef DEBUG

struct nsAutoLayoutPhase {
  nsAutoLayoutPhase(nsPresContext* aPresContext, nsLayoutPhase aPhase)
    : mPresContext(aPresContext), mPhase(aPhase), mCount(0)
  {
    Enter();
  }

  ~nsAutoLayoutPhase()
  {
    Exit();
    NS_ASSERTION(mCount == 0, "imbalanced");
  }

  void Enter()
  {
    switch (mPhase) {
      case eLayoutPhase_Paint:
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_Paint] == 0,
                     "recurring into paint");
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_Reflow] == 0,
                     "painting in the middle of reflow");
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_FrameC] == 0,
                     "painting in the middle of frame construction");
        break;
      case eLayoutPhase_Reflow:
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_Paint] == 0,
                     "reflowing in the middle of a paint");
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_Reflow] == 0,
                     "recurring into reflow");
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_FrameC] == 0,
                     "reflowing in the middle of frame construction");
        break;
      case eLayoutPhase_FrameC:
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_Paint] == 0,
                     "constructing frames in the middle of a paint");
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_Reflow] == 0,
                     "constructing frames in the middle of reflow");
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_FrameC] == 0,
                     "recurring into frame construction");
        NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
                     "constructing frames and scripts are not blocked");
        break;
      default:
        break;
    }
    ++(mPresContext->mLayoutPhaseCount[mPhase]);
    ++mCount;
  }

  void Exit()
  {
    NS_ASSERTION(mCount > 0 && mPresContext->mLayoutPhaseCount[mPhase] > 0,
                 "imbalanced");
    --(mPresContext->mLayoutPhaseCount[mPhase]);
    --mCount;
  }

private:
  nsPresContext *mPresContext;
  nsLayoutPhase mPhase;
  PRUint32 mCount;
};

#define AUTO_LAYOUT_PHASE_ENTRY_POINT(pc_, phase_) \
  nsAutoLayoutPhase autoLayoutPhase((pc_), (eLayoutPhase_##phase_))
#define LAYOUT_PHASE_TEMP_EXIT() \
  PR_BEGIN_MACRO \
    autoLayoutPhase.Exit(); \
  PR_END_MACRO
#define LAYOUT_PHASE_TEMP_REENTER() \
  PR_BEGIN_MACRO \
    autoLayoutPhase.Enter(); \
  PR_END_MACRO

#else

#define AUTO_LAYOUT_PHASE_ENTRY_POINT(pc_, phase_) \
  PR_BEGIN_MACRO PR_END_MACRO
#define LAYOUT_PHASE_TEMP_EXIT() \
  PR_BEGIN_MACRO PR_END_MACRO
#define LAYOUT_PHASE_TEMP_REENTER() \
  PR_BEGIN_MACRO PR_END_MACRO

#endif

#ifdef MOZ_REFLOW_PERF

#define DO_GLOBAL_REFLOW_COUNT(_name) \
  aPresContext->CountReflows((_name), (nsIFrame*)this); 
#else
#define DO_GLOBAL_REFLOW_COUNT(_name)
#endif // MOZ_REFLOW_PERF

#endif /* nsPresContext_h___ */
