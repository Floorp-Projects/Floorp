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
#ifndef nsIPresContext_h___
#define nsIPresContext_h___

#include "nsISupports.h"
#include "nsColor.h"
#include "nsCoord.h"
#include "nsAString.h"
#include "nsIRequest.h"
#include "nsCompatibility.h"
#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsRect.h"
#include "nsIDeviceContext.h"
#ifdef IBMBIDI
class nsBidiPresUtils;
#endif // IBMBIDI

struct nsFont;
struct nsRect;

class imgIRequest;

class nsIContent;
class nsIDocument;
class nsIDeviceContext;
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
class nsITheme;
struct nsStyleStruct;
struct nsStyleBackground;

#ifdef MOZ_REFLOW_PERF
class nsIRenderingContext;
#endif

#define NS_IPRESCONTEXT_IID   \
{ 0x2820eeff, 0x7e66, 0x43df, \
  {0xae, 0x19, 0xee, 0xf6, 0x09, 0xc1, 0xcf, 0xfe} }

enum nsWidgetType {
  eWidgetType_Button  	= 1,
  eWidgetType_Checkbox	= 2,
  eWidgetType_Radio			= 3,
  eWidgetType_Text			= 4
};

enum nsLanguageSpecificTransformType {
  eLanguageSpecificTransformType_Unknown = -1,
  eLanguageSpecificTransformType_None = 0,
  eLanguageSpecificTransformType_Japanese,
  eLanguageSpecificTransformType_Korean
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
class nsIPresContext : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPRESCONTEXT_IID)

  /**
   * Initialize the presentation context from a particular device.
   */
  NS_IMETHOD Init(nsIDeviceContext* aDeviceContext) = 0;

  /**
   * Set the presentation shell that this context is bound to.
   * A presentation context may only be bound to a single shell.
   */
  NS_IMETHOD SetShell(nsIPresShell* aShell) = 0;

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
  virtual void    SetCompatibilityMode(nsCompatibility aMode) = 0;

  /**
   * Access the image animation mode for this context
   */
  PRUint16     ImageAnimationMode() const { return mImageAnimationMode; }
  virtual void SetImageAnimationMode(PRUint16 aMode) = 0;

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
  virtual void ClearStyleDataAndReflow() = 0;

   /**
    * Resolve a new style context for a content node and return the URL
    * for its XBL binding, or null if it has no binding specified in CSS.
    */
  virtual nsresult GetXBLBindingURL(nsIContent* aContent,
                                    nsIURI** aResult) = 0;

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
  NS_IMETHOD GetMetricsFor(const nsFont& aFont, nsIFontMetrics** aResult) = 0;

  /**
   * Get the default font correponding to the given ID.  This object is
   * read-only, you must copy the font to modify it.
   */
  virtual const nsFont* GetDefaultFont(PRUint8 aFontID) const = 0;

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
  virtual nsresult LoadImage(imgIRequest* aImage,
                             nsIFrame* aTargetFrame,
                             imgIRequest **aRequest) = 0;

  /**
   * This method is called when a frame is being destroyed to
   * ensure that the image load gets disassociated from the prescontext
   */
  virtual void StopImagesFor(nsIFrame* aTargetFrame) = 0;

  virtual void SetContainer(nsISupports* aContainer) = 0;

  virtual already_AddRefed<nsISupports> GetContainer() = 0;

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
  virtual void SetPaginatedScrolling(PRBool aResult) = 0;

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
  virtual void GetPageDim(nsRect* aActualRect, nsRect* aAdjRect) = 0;

  /**
   * Sets the "adjusted" rect for the page Dimimensions, 
   * this includes X,Y Offsets which are used to determine 
   * the inclusion of margins
   *
   * @param aRect returns the adjusted size 
   */
  virtual void SetPageDim(nsRect* aRect) = 0;

  float PixelsToTwips() const { return mDeviceContext->DevUnitsToAppUnits(); }

  float TwipsToPixels() const { return mDeviceContext->AppUnitsToDevUnits(); }

  NS_IMETHOD GetTwipsToPixelsForFonts(float* aResult) const = 0;

  //XXX this is probably not an ideal name. MMP
  /** 
   * Do pixels to twips conversion taking into account
   * differing size of a "pixel" from device to device.
   */
  NS_IMETHOD GetScaledPixelsToTwips(float* aScale) const = 0;

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
  virtual PRBool BidiEnabled() const = 0;

  /**
   *  Set bidi enabled. This means we should apply the Unicode Bidi Algorithm
   *
   *  @lina 07/12/2000
   */
  virtual void SetBidiEnabled(PRBool aBidiEnabled) const  = 0;

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
  NS_IMETHOD GetBidiUtils(nsBidiPresUtils** aBidiUtils) = 0;

  /**
   * Set the Bidi options for the presentation context
   */  
  NS_IMETHOD SetBidi(PRUint32 aBidiOptions, PRBool aForceReflow = PR_FALSE) = 0;

  /**
   * Get the Bidi options for the presentation context
   */  
  NS_IMETHOD GetBidi(PRUint32* aBidiOptions) const = 0;

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
  NS_IMETHOD GetTheme(nsITheme** aResult) = 0;

  /*
   * Notify the pres context that the theme has changed.  An internal switch
   * means it's one of our Mozilla themes that changed (e.g., Modern to Classic).
   * Otherwise, the OS is telling us that the native theme for the platform
   * has changed.
   */
  NS_IMETHOD ThemeChanged() = 0;

  /*
   * Notify the pres context that a system color has changed
   */
  NS_IMETHOD SysColorChanged() = 0;

#ifdef MOZ_REFLOW_PERF
  NS_IMETHOD CountReflows(const char * aName, PRUint32 aType, nsIFrame * aFrame) = 0;
  NS_IMETHOD PaintCount(const char * aName, nsIRenderingContext* aRendingContext, nsIFrame * aFrame, PRUint32 aColor) = 0;
#endif

protected:
  // IMPORTANT: The ownership implicit in the following member variables
  // has been explicitly checked.  If you add any members to this class,
  // please make the ownership explicit (pinkerton, scc).
  
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

  nsLanguageSpecificTransformType mLanguageSpecificTransformType;
  PRInt32               mFontScaler;
  nscoord               mMinimumFontSize;

  nsRect                mVisibleArea;

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
#ifdef IBMBIDI
  unsigned              mIsVisual : 1;
  unsigned              mIsBidiSystem : 1;
#endif
};

// Bit values for StartLoadImage's aImageStatus
#define NS_LOAD_IMAGE_STATUS_ERROR      0x1
#define NS_LOAD_IMAGE_STATUS_SIZE       0x2
#define NS_LOAD_IMAGE_STATUS_BITS       0x4

// Factory method to create a "galley" presentation context (galley is
// a kind of view that has no limit to the size of a page)
nsresult
NS_NewGalleyContext(nsIPresContext** aInstancePtrResult);

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

#endif /* nsIPresContext_h___ */
