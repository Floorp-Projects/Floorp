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
#ifndef nsPresContext_h___
#define nsPresContext_h___

#include "nsISupports.h"
#include "nsColor.h"
#include "nsCoord.h"
#include "nsAString.h"
#include "nsCompatibility.h"
#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsRect.h"
#include "nsIDeviceContext.h"
#include "nsHashtable.h"
#include "nsFont.h"
#include "nsIWeakReference.h"
#include "nsITheme.h"
#include "nsILanguageAtomService.h"
#include "nsIObserver.h"
#include "nsCRT.h"
#include "nsIPrintSettings.h"
#ifdef IBMBIDI
class nsBidiPresUtils;
#endif // IBMBIDI

struct nsRect;

class imgIRequest;

class nsIContent;
class nsIDocument;
class nsIFontMetrics;
class nsIFrame;
class nsFrameManager;
class nsIImage;
class nsILinkHandler;
class nsStyleContext;
class nsIAtom;
class nsIEventStateManager;
class nsIURI;
class nsILookAndFeel;
class nsICSSPseudoComparator;
class nsIAtom;
struct nsStyleStruct;
struct nsStyleBackground;

#ifdef MOZ_REFLOW_PERF
class nsIRenderingContext;
#endif

#define NS_IPRESCONTEXT_IID   \
{ 0x96e4bc06, 0x8e72, 0x4941, \
  {0xa6, 0x6c, 0x70, 0xee, 0x7d, 0x1b, 0x58, 0x21} }

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
const PRUint32 kPresContext_UseDocumentColors = 0x01;
const PRUint32 kPresContext_UseDocumentFonts = 0x02;
const PRUint32 kPresContext_UnderlineLinks = 0x03;

// supported values for cached integer pref types
const PRUint32 kPresContext_MinimumFontSize = 0x01;

// IDs for the default variable and fixed fonts (not to be changed, see nsFont.h)
// To be used for Get/SetDefaultFont(). The other IDs in nsFont.h are also supported.
const PRUint8 kPresContext_DefaultVariableFont_ID = 0x00; // kGenericFont_moz_variable
const PRUint8 kPresContext_DefaultFixedFont_ID    = 0x01; // kGenericFont_moz_fixed

// An interface for presentation contexts. Presentation contexts are
// objects that provide an outer context for a presentation shell.

// hack to make egcs / gcc 2.95.2 happy
class nsPresContext_base : public nsIObserver
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPRESCONTEXT_IID)
};

class nsPresContext : public nsPresContext_base {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  enum nsPresContextType {
    eContext_Galley,       // unpaginated screen presentation
    eContext_PrintPreview, // paginated screen presentation
    eContext_Print         // paginated printer presentation
  };

  nsPresContext(nsPresContextType aType) NS_HIDDEN;

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

  nsIDocument* GetDocument() { return GetPresShell()->GetDocument(); } 
  nsIViewManager* GetViewManager() { return GetPresShell()->GetViewManager(); } 
#ifdef _IMPL_NS_LAYOUT
  nsStyleSet* StyleSet() { return GetPresShell()->StyleSet(); }

  nsFrameManager* FrameManager()
    { return GetPresShell()->FrameManager(); } 
#endif

  /**
   * Access compatibility mode for this context
   *
   * All users must explicitly set the compatibility mode rather than
   * relying on a default.
   */
  nsCompatibility CompatibilityMode() const { return mCompatibilityMode; }
  NS_HIDDEN_(void) SetCompatibilityMode(nsCompatibility aMode);

  /**
   * Access the image animation mode for this context
   */
  PRUint16     ImageAnimationMode() const { return mImageAnimationMode; }
  virtual void SetImageAnimationModeExternal(PRUint16 aMode);
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

  /**
   * Clear style data from the root frame downwards, and reflow.
   */
  NS_HIDDEN_(void) ClearStyleDataAndReflow();

  void* AllocateFromShell(size_t aSize)
  {
    if (mShell)
      return mShell->AllocateFrame(aSize);
    return nsnull;
  }

  void FreeToShell(size_t aSize, void* aFreeChunk)
  {
    if (mShell)
      mShell->FreeFrame(aSize, aFreeChunk);
  }

  /**
   * Get the font metrics for a given font.
   */
  virtual already_AddRefed<nsIFontMetrics>
   GetMetricsForExternal(const nsFont& aFont);
  NS_HIDDEN_(already_AddRefed<nsIFontMetrics>)
    GetMetricsForInternal(const nsFont& aFont);
#ifdef _IMPL_NS_LAYOUT
  already_AddRefed<nsIFontMetrics> GetMetricsFor(const nsFont& aFont)
  { return GetMetricsForInternal(aFont); }
#else
  already_AddRefed<nsIFontMetrics> GetMetricsFor(const nsFont& aFont)
  { return GetMetricsForExternal(aFont); }
#endif

  /**
   * Get the default font correponding to the given ID.  This object is
   * read-only, you must copy the font to modify it.
   */
  virtual const nsFont* GetDefaultFontExternal(PRUint8 aFontID) const;
  NS_HIDDEN_(const nsFont*) GetDefaultFontInternal(PRUint8 aFontID) const;
#ifdef _IMPL_NS_LAYOUT
  const nsFont* GetDefaultFont(PRUint8 aFontID) const
  { return GetDefaultFontInternal(aFontID); }
#else
  const nsFont* GetDefaultFont(PRUint8 aFontID) const
  { return GetDefaultFontExternal(aFontID); }
#endif

  /** Get a cached boolean pref, by its type */
  // *  - initially created for bugs 31816, 20760, 22963
  PRBool GetCachedBoolPref(PRUint32 aPrefType) const
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
  PRInt32 GetCachedIntPref(PRUint32 aPrefType) const
  {
    // If called with a constant parameter, the compiler should optimize
    // this switch statement away.
    switch (aPrefType) {
    case kPresContext_MinimumFontSize:
      return mMinimumFontSize;
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
 

  /**
   * Load an image for the target frame. This call can be made
   * repeated with only a single image ever being loaded. When the
   * image's data is ready for rendering the target frame's Paint()
   * method will be invoked (via the ViewManager) so that the
   * appropriate damage repair is done.
   */
  NS_HIDDEN_(imgIRequest*) LoadImage(imgIRequest* aImage,
                                     nsIFrame* aTargetFrame);

  /**
   * This method is called when a frame is being destroyed to
   * ensure that the image load gets disassociated from the prescontext
   */
  NS_HIDDEN_(void) StopImagesFor(nsIFrame* aTargetFrame);

  NS_HIDDEN_(void) SetContainer(nsISupports* aContainer);

  virtual already_AddRefed<nsISupports> GetContainerExternal();
  NS_HIDDEN_(already_AddRefed<nsISupports>) GetContainerInternal();
#ifdef _IMPL_NS_LAYOUT
  already_AddRefed<nsISupports> GetContainer()
  { return GetContainerInternal(); }
#else
  already_AddRefed<nsISupports> GetContainer()
  { return GetContainerExternal(); }
#endif

  // XXX this are going to be replaced with set/get container
  void SetLinkHandler(nsILinkHandler* aHandler) { mLinkHandler = aHandler; }
  nsILinkHandler* GetLinkHandler() { return mLinkHandler; }

  /**
   * Get the visible area associated with this presentation context.
   * This is the size of the visiable area that is used for
   * presenting the document. The returned value is in the standard
   * nscoord units (as scaled by the device context).
   */
  nsRect GetVisibleArea() { return mVisibleArea; }

  /**
   * Set the currently visible area. The units for r are standard
   * nscoord units (as scaled by the device context).
   */
  void SetVisibleArea(const nsRect& r) { mVisibleArea = r; }

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
   * Gets the rect for the page dimensions,
   * this includes X,Y Offsets which are used to determine 
   * the inclusion of margins
   * Also, indicates whether the size has been overridden
   *
   * @param aActualRect returns the size of the actual device/surface
   * @param aRect returns the adjusted size 
   */
  NS_HIDDEN_(void) GetPageDim(nsRect* aActualRect, nsRect* aAdjRect);

  /**
   * Sets the "adjusted" rect for the page Dimimensions, 
   * this includes X,Y Offsets which are used to determine 
   * the inclusion of margins
   *
   * @param aRect returns the adjusted size 
   */
  NS_HIDDEN_(void) SetPageDim(const nsRect& aRect);

  float PixelsToTwips() const { return mDeviceContext->DevUnitsToAppUnits(); }

  float TwipsToPixels() const { return mDeviceContext->AppUnitsToDevUnits(); }

  NS_HIDDEN_(float) TwipsToPixelsForFonts() const;

  //XXX this is probably not an ideal name. MMP
  /** 
   * Do pixels to twips conversion taking into account
   * differing size of a "pixel" from device to device.
   */
  NS_HIDDEN_(float) ScaledPixelsToTwips() const;

  /* Convenience method for converting one pixel value to twips */
  nscoord IntScaledPixelsToTwips(nscoord aPixels) const
  { return NSIntPixelsToTwips(aPixels, ScaledPixelsToTwips()); }

  /* Set whether twip scaling is used */
  void SetScalingOfTwips(PRBool aOn) { mDoScaledTwips = aOn; }

  nsIDeviceContext* DeviceContext() { return mDeviceContext; }
  nsIEventStateManager* EventStateManager() { return mEventManager; }
  nsIAtom* GetLangGroup() { return mLangGroup; }

  /**
   * Get the language-specific transform type for the current document.
   * This tells us whether we need to perform special language-dependent
   * transformations such as Unicode U+005C (backslash) to Japanese
   * Yen Sign (Unicode U+00A5, JIS 0x5C).
   *
   * @param aType returns type, must be non-NULL
   */
  nsLanguageSpecificTransformType LanguageSpecificTransformType() const
  {
    return mLanguageSpecificTransformType;
  }

  void SetViewportOverflowOverride(PRUint8 aStyle)
  {
    mViewportStyleOverflow = aStyle;
  }
  PRUint8 GetViewportOverflowOverride() { return mViewportStyleOverflow; }

  /**
   * Set and get methods for controling the background drawing
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
  virtual PRBool BidiEnabledExternal() const;
  NS_HIDDEN_(PRBool) BidiEnabledInternal() const;
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
  NS_HIDDEN_(void) SetBidiEnabled(PRBool aBidiEnabled) const;

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
                           PRBool aForceReflow = PR_FALSE);

  /**
   * Get the Bidi options for the presentation context
   */  
  NS_HIDDEN_(PRUint32) GetBidi() const { return mBidi; }

  /**
   * Set the Bidi capabilities of the system
   * @param aIsBidi == TRUE if the system has the capability of reordering Bidi text
   */
  void SetIsBidiSystem(PRBool aIsBidi)
  {
    NS_ASSERTION(!(aIsBidi & ~1), "Value must be true or false");
    mIsBidiSystem = aIsBidi;
  }

  /**
   * Get the Bidi capabilities of the system
   * @return TRUE if the system has the capability of reordering Bidi text
   */
  PRBool IsBidiSystem() const { return mIsBidiSystem; }
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

#ifdef MOZ_REFLOW_PERF
  NS_HIDDEN_(void) CountReflows(const char * aName,
                                PRUint32 aType, nsIFrame * aFrame);
  NS_HIDDEN_(void) PaintCount(const char * aName,
                              nsIRenderingContext* aRendingContext,
                              nsIFrame * aFrame, PRUint32 aColor);
#endif

protected:
  NS_HIDDEN_(void) SetImgAnimations(nsIContent *aParent, PRUint16 aMode);
  NS_HIDDEN_(void) GetDocumentColorPreferences();
  NS_HIDDEN_(void) PreferenceChanged(const char* aPrefName);
  static NS_HIDDEN_(int) PR_CALLBACK PrefChangedCallback(const char*, void*);

  NS_HIDDEN_(void) GetUserPreferences();
  NS_HIDDEN_(void) GetFontPreferences();

  NS_HIDDEN_(void) UpdateCharSet(const char* aCharSet);

  // IMPORTANT: The ownership implicit in the following member variables
  // has been explicitly checked.  If you add any members to this class,
  // please make the ownership explicit (pinkerton, scc).
  
  nsPresContextType     mType;
  nsIPresShell*         mShell;         // [WEAK]
  nsIDeviceContext*     mDeviceContext; // [STRONG] could be weak, but
                                        // better safe than sorry.
                                        // Cannot reintroduce cycles
                                        // since there is no dependency
                                        // from gfx back to layout.
  nsIEventStateManager* mEventManager;  // [STRONG]
  nsILookAndFeel*       mLookAndFeel;   // [STRONG]
  nsIAtom*              mMedium;        // initialized by subclass ctors;
                                        // weak pointer to static atom

  nsILinkHandler*       mLinkHandler;   // [WEAK]
  nsIAtom*              mLangGroup;     // [STRONG]

  nsSupportsHashtable   mImageLoaders;
  nsWeakPtr             mContainer;

#ifdef IBMBIDI
  nsBidiPresUtils*      mBidiUtils;
  nsCString             mCharset;                 // the charset we are using
#endif

  nsCOMPtr<nsITheme> mTheme;
  nsCOMPtr<nsILanguageAtomService> mLangService;
  nsCOMPtr<nsIPrintSettings> mPrintSettings;

  nsLanguageSpecificTransformType mLanguageSpecificTransformType;
  PRInt32               mFontScaler;
  nscoord               mMinimumFontSize;

  nsRect                mVisibleArea;
  nsRect                mPageDim;

  nscolor               mDefaultColor;
  nscolor               mBackgroundColor;

  nscolor               mLinkColor;
  nscolor               mActiveLinkColor;
  nscolor               mVisitedLinkColor;

  nscolor               mFocusBackgroundColor;
  nscolor               mFocusTextColor;

  PRUint8               mFocusRingWidth;
  PRUint8               mViewportStyleOverflow;

  nsCompatibility       mCompatibilityMode;
  PRUint16              mImageAnimationMode;
  PRUint16              mImageAnimationModePref;

  nsFont                mDefaultVariableFont;
  nsFont                mDefaultFixedFont;
  nsFont                mDefaultSerifFont;
  nsFont                mDefaultSansSerifFont;
  nsFont                mDefaultMonospaceFont;
  nsFont                mDefaultCursiveFont;
  nsFont                mDefaultFantasyFont;

  unsigned              mUseDocumentFonts : 1;
  unsigned              mUseDocumentColors : 1;
  unsigned              mUnderlineLinks : 1;
  unsigned              mUseFocusColors : 1;
  unsigned              mFocusRingOnAnything : 1;
  unsigned              mDrawImageBackground : 1;
  unsigned              mDrawColorBackground : 1;
  unsigned              mNeverAnimate : 1;
  unsigned              mIsRenderingOnlySelection : 1;
  unsigned              mNoTheme : 1;
  unsigned              mPaginated : 1;
  unsigned              mCanPaginatedScroll : 1;
  unsigned              mDoScaledTwips : 1;
  unsigned              mEnableJapaneseTransform : 1;
#ifdef IBMBIDI
  unsigned              mIsVisual : 1;
  unsigned              mIsBidiSystem : 1;

  PRUint32              mBidi;
#endif
#ifdef DEBUG
  PRBool                mInitialized;
#endif


private:

  ~nsPresContext() NS_HIDDEN;

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

};

// Bit values for StartLoadImage's aImageStatus
#define NS_LOAD_IMAGE_STATUS_ERROR      0x1
#define NS_LOAD_IMAGE_STATUS_SIZE       0x2
#define NS_LOAD_IMAGE_STATUS_BITS       0x4

#ifdef MOZ_REFLOW_PERF

#define DO_GLOBAL_REFLOW_COUNT(_name, _type) \
  aPresContext->CountReflows((_name), (_type), (nsIFrame*)this); 
#else
#define DO_GLOBAL_REFLOW_COUNT(_name, _type)
#endif // MOZ_REFLOW_PERF

#if defined(MOZ_REFLOW_PERF_DSP) && defined(MOZ_REFLOW_PERF)
#define DO_GLOBAL_REFLOW_COUNT_DSP(_name, _rend) \
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) { \
    aPresContext->PaintCount((_name), (_rend), (nsIFrame*)this, 0); \
  }
#define DO_GLOBAL_REFLOW_COUNT_DSP_J(_name, _rend, _just) \
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) { \
    aPresContext->PaintCount((_name), (_rend), (nsIFrame*)this, (_just)); \
  }
#else
#define DO_GLOBAL_REFLOW_COUNT_DSP(_name, _rend)
#define DO_GLOBAL_REFLOW_COUNT_DSP_J(_name, _rend, _just)
#endif // MOZ_REFLOW_PERF_DSP

#endif /* nsPresContext_h___ */
