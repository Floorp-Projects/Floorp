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
#ifndef nsIPresContext_h___
#define nsIPresContext_h___

#include "nsISupports.h"
#include "nsColor.h"
#include "nsCoord.h"
#include "nsAWritableString.h"
#include "nsIRequest.h"
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
class nsIImage;
class nsILinkHandler;
class nsIPresShell;
class nsIPref;
class nsIStyleContext;
class nsIAtom;
class nsString;
class nsIEventStateManager;
class nsIURI;
class nsILookAndFeel;
class nsICSSPseudoComparator;
class nsILanguageAtom;

#ifdef MOZ_REFLOW_PERF
class nsIRenderingContext;
#endif

#define NS_IPRESCONTEXT_IID   \
{ 0x0a5d12e0, 0x944e, 0x11d1, \
  {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

enum nsCompatibility {
  eCompatibility_Standard   = 1,
  eCompatibility_NavQuirks  = 2
};

enum nsWidgetRendering {
  eWidgetRendering_Native   = 1,
  eWidgetRendering_Gfx      = 2,
  eWidgetRendering_PartialGfx = 3
};

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


enum nsImageAnimation {
  eImageAnimation_Normal      = 0,            // looping controlled by image
  eImageAnimation_None        = 1,            // don't loop; just show first frame
  eImageAnimation_LoopOnce    = 2             // loop just once
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
  static const nsIID& GetIID() { static nsIID iid = NS_IPRESCONTEXT_IID; return iid; }

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
  NS_IMETHOD GetShell(nsIPresShell** aResult) = 0;

  /**
   * Access compatibility mode for this context
   *
   * All users must explicitly set the compatibility mode rather than
   * relying on a default.
   */
  NS_IMETHOD GetCompatibilityMode(nsCompatibility* aModeResult) = 0;
  NS_IMETHOD SetCompatibilityMode(nsCompatibility aMode) = 0;

  /**
   * Access the widget rendering mode for this context
   */
  NS_IMETHOD GetWidgetRenderingMode(nsWidgetRendering* aModeResult) = 0;
  NS_IMETHOD SetWidgetRenderingMode(nsWidgetRendering aMode) = 0;

  /**
   * Access the image animation mode for this context
   */
  NS_IMETHOD GetImageAnimationMode(nsImageAnimation* aModeResult) = 0;
  NS_IMETHOD SetImageAnimationMode(nsImageAnimation aMode) = 0;

  /**
   * Get an special load flags for images for this context
   */
  NS_IMETHOD GetImageLoadFlags(nsLoadFlags& aLoadFlags) = 0;

  /**
   * Get look and feel object
   */
  NS_IMETHOD GetLookAndFeel(nsILookAndFeel** aLookAndFeel) = 0;

  /** 
   * Get base url for presentation
   */
  NS_IMETHOD GetBaseURL(nsIURI** aURLResult) = 0;

  /** 
   * Get medium of presentation
   */
  NS_IMETHOD GetMedium(nsIAtom** aMediumResult) = 0;

  /**
   * Clear style data from the root frame downwards, and reflow.
   */
  NS_IMETHOD ClearStyleDataAndReflow(void) = 0;

  /**
   * Resolve style for the given piece of content that will be a child
   * of the aParentContext. Don't use this for pseudo frames.
   */
  NS_IMETHOD ResolveStyleContextFor(nsIContent* aContent,
                                    nsIStyleContext* aParentContext,
                                    PRBool aForceUnique,
                                    nsIStyleContext** aResult) = 0;

  /**
   * Resolve style for a non-element content node (i.e., one that is
   * guaranteed not to match any rules).  Eventually such nodes
   * shouldn't have style contexts at all, but this at least prevents
   * the rule matching.
   *
   * XXX This is temporary.  It should go away when we stop creating
   * style contexts for text nodes and placeholder frames.  (We also use
   * it once to create a style context for the nsFirstLetterFrame that
   * represents everything except the first letter.)
   *
   */
  NS_IMETHOD ResolveStyleContextForNonElement(
                                    nsIStyleContext* aParentContext,
                                    PRBool aForceUnique,
                                    nsIStyleContext** aResult) = 0;

  /**
   * Resolve style for a pseudo frame within the given aParentContent & aParentContext.
   * The tag should be lowercase and inclue the colon.
   * ie: NS_NewAtom(":first-line");
   */
  NS_IMETHOD ResolvePseudoStyleContextFor(nsIContent* aParentContent,
                                          nsIAtom* aPseudoTag,
                                          nsIStyleContext* aParentContext,
                                          PRBool aForceUnique,
                                          nsIStyleContext** aResult) = 0;

  /**
   * Resolve style for a pseudo frame within the given aParentContent & aParentContext.
   * The tag should be lowercase and inclue the colon.
   * ie: NS_NewAtom(":first-line");
   *
   * Instead of matching solely on aPseudoTag, a comparator function can be
   * passed in to test.
   */
  NS_IMETHOD ResolvePseudoStyleWithComparator(nsIContent* aParentContent,
                                              nsIAtom* aPseudoTag,
                                              nsIStyleContext* aParentContext,
                                              PRBool aForceUnique,
                                              nsICSSPseudoComparator* aComparator,
                                              nsIStyleContext** aResult) = 0;

  /**
   * Probe style for a pseudo frame within the given aParentContent & aParentContext.
   * This will return nsnull id there are no explicit rules for the pseudo element.
   * The tag should be lowercase and inclue the colon.
   * ie: NS_NewAtom(":first-line");
   */
  NS_IMETHOD ProbePseudoStyleContextFor(nsIContent* aParentContent,
                                        nsIAtom* aPseudoTag,
                                        nsIStyleContext* aParentContext,
                                        PRBool aForceUnique,
                                        nsIStyleContext** aResult) = 0;

  /** 
   * For a given frame tree, get a new style context that is the equivalent
   * but within a new parent
   */
  NS_IMETHOD ReParentStyleContext(nsIFrame* aFrame, 
                                  nsIStyleContext* aNewParentContext) = 0;


  NS_IMETHOD AllocateFromShell(size_t aSize, void** aResult) = 0;
  NS_IMETHOD FreeToShell(size_t aSize, void* aFreeChunk) = 0;

  /**
   * Get the font metrics for a given font.
   */
  NS_IMETHOD GetMetricsFor(const nsFont& aFont, nsIFontMetrics** aResult) = 0;

  /** Get the default font correponding to the given ID */
  NS_IMETHOD GetDefaultFont(PRUint8 aFontID, nsFont& aResult) = 0;
  /** Set the default font correponding to the given ID */
  NS_IMETHOD SetDefaultFont(PRUint8 aFontID, const nsFont& aFont) = 0;

  /** Get a cached boolean pref, by its type
       if the type is not supported, then NS_ERROR_INVALID_ARG is returned
       and the aValue argument is undefined, otherwise aValue is set
       to the value of the boolean pref */
  // *  - initially created for bugs 31816, 20760, 22963
  NS_IMETHOD GetCachedBoolPref(PRUint32 aPrefType, PRBool& aValue) = 0;

  /** Get a cached integer pref, by its type
       if the type is not supported, then NS_ERROR_INVALID_ARG is returned
       and the aValue argument is undefined, otherwise aValue is set
       to the value of the integer pref */
  // *  - initially created for bugs 30910, 61883, 74186, 84398
  NS_IMETHOD GetCachedIntPref(PRUint32 aPrefType, PRInt32& aValue) = 0;

  /**
   * Access Nav's magic font scaler value
   */
  NS_IMETHOD GetFontScaler(PRInt32* aResult) = 0;
  NS_IMETHOD SetFontScaler(PRInt32 aScaler) = 0;

  /** 
   * Get the default colors
   */
  NS_IMETHOD GetDefaultColor(nscolor* aColor) = 0;
  NS_IMETHOD GetDefaultBackgroundColor(nscolor* aColor) = 0;
  NS_IMETHOD GetDefaultBackgroundImage(nsString& aImage) = 0;
  NS_IMETHOD GetDefaultBackgroundImageRepeat(PRUint8* aRepeat) = 0;
  NS_IMETHOD GetDefaultBackgroundImageOffset(nscoord* aX, nscoord* aY) = 0;
  NS_IMETHOD GetDefaultBackgroundImageAttachment(PRUint8* aRepeat) = 0;
  NS_IMETHOD GetDefaultLinkColor(nscolor* aColor) = 0;
  NS_IMETHOD GetDefaultVisitedLinkColor(nscolor* aColor) = 0;
  NS_IMETHOD GetFocusBackgroundColor(nscolor* aColor) = 0;
  NS_IMETHOD GetFocusTextColor(nscolor* aColor) = 0; 
  NS_IMETHOD GetUseFocusColors(PRBool& useFocusColors) = 0;
  NS_IMETHOD GetFocusRingWidth(PRUint8 *focusRingWidth) = 0;
  NS_IMETHOD GetFocusRingOnAnything(PRBool& focusRingOnAnything) = 0;
 

  NS_IMETHOD SetDefaultColor(nscolor aColor) = 0;
  NS_IMETHOD SetDefaultBackgroundColor(nscolor aColor) = 0;
  NS_IMETHOD SetDefaultBackgroundImage(const nsString& aImage) = 0;
  NS_IMETHOD SetDefaultBackgroundImageRepeat(PRUint8 aRepeat) = 0;
  NS_IMETHOD SetDefaultBackgroundImageOffset(nscoord aX, nscoord aY) = 0;
  NS_IMETHOD SetDefaultBackgroundImageAttachment(PRUint8 aRepeat) = 0;
  NS_IMETHOD SetDefaultLinkColor(nscolor aColor) = 0;
  NS_IMETHOD SetDefaultVisitedLinkColor(nscolor aColor) = 0;

  /**
   * Load an image for the target frame. This call can be made
   * repeated with only a single image ever being loaded. When the
   * image's data is ready for rendering the target frame's Paint()
   * method will be invoked (via the ViewManager) so that the
   * appropriate damage repair is done.
   */
  NS_IMETHOD LoadImage(const nsString& aURL,
                       nsIFrame* aTargetFrame,
                       imgIRequest **aRequest) = 0;

  /**
   * This method is called when a frame is being destroyed to
   * ensure that the image load gets disassociated from the prescontext
   */
  NS_IMETHOD StopImagesFor(nsIFrame* aTargetFrame) = 0;

  NS_IMETHOD SetContainer(nsISupports* aContainer) = 0;

  NS_IMETHOD GetContainer(nsISupports** aResult) = 0;

  // XXX this are going to be replaced with set/get container
  NS_IMETHOD SetLinkHandler(nsILinkHandler* aHandler) = 0;
  NS_IMETHOD GetLinkHandler(nsILinkHandler** aResult) = 0;

  /**
   * Get the visible area associated with this presentation context.
   * This is the size of the visiable area that is used for
   * presenting the document. The returned value is in the standard
   * nscoord units (as scaled by the device context).
   */
  NS_IMETHOD GetVisibleArea(nsRect& aResult) = 0;

  /**
   * Set the currently visible area. The units for r are standard
   * nscoord units (as scaled by the device context).
   */
  NS_IMETHOD SetVisibleArea(const nsRect& r) = 0;

  /**
   * Return true if this presentation context is a paginated
   * context.
   */
  NS_IMETHOD IsPaginated(PRBool* aResult) = 0;

  /**
   * Gets the rect for the page Dimimensions, 
   * this includes X,Y Offsets which are used to determine 
   * the inclusion of margins
   * Also, indicates whether the size has been overridden
   *
   * @param aActualRect returns the size of the actual device/surface
   * @param aRect returns the adjusted size 
   */
  NS_IMETHOD GetPageDim(nsRect* aActualRect, nsRect* aAdjRect) = 0;

  /**
   * Sets the "adjusted" rect for the page Dimimensions, 
   * this includes X,Y Offsets which are used to determine 
   * the inclusion of margins
   *
   * @param aRect returns the adjusted size 
   */
  NS_IMETHOD SetPageDim(nsRect* aRect) = 0;

  NS_IMETHOD GetPixelsToTwips(float* aResult) const = 0;

  NS_IMETHOD GetTwipsToPixels(float* aResult) const = 0;

  //XXX this is probably not an ideal name. MMP
  /** 
   * Do pixels to twips conversion taking into account
   * differing size of a "pixel" from device to device.
   */
  NS_IMETHOD GetScaledPixelsToTwips(float* aScale) const = 0;

  //be sure to Relase() after you are done with the Get()
  NS_IMETHOD GetDeviceContext(nsIDeviceContext** aResult) const = 0;

  NS_IMETHOD GetEventStateManager(nsIEventStateManager** aManager) = 0;
  NS_IMETHOD GetLanguage(nsILanguageAtom** aLanguage) = 0;

  /**
   * Get the language-specific transform type for the current document.
   * This tells us whether we need to perform special language-dependent
   * transformations such as Unicode U+005C (backslash) to Japanese
   * Yen Sign (Unicode U+00A5, JIS 0x5C).
   *
   * @param aType returns type, must be non-NULL
   */
  NS_IMETHOD GetLanguageSpecificTransformType(
              nsLanguageSpecificTransformType* aType) = 0;

#ifdef IBMBIDI
  /**
   *  Check if bidi enabled (set depending on the presence of RTL
   *  characters or when default directionality is RTL).
   *  If enabled, we should apply the Unicode Bidi Algorithm
   *
   *  @lina 07/12/2000
   */
  NS_IMETHOD GetBidiEnabled(PRBool* aBidiEnabled) const = 0;

  /**
   *  Set bidi enabled. This means we should apply the Unicode Bidi Algorithm
   *
   *  @lina 07/12/2000
   */
  NS_IMETHOD SetBidiEnabled(PRBool aBidiEnabled) const  = 0;

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
  NS_IMETHOD SetVisualMode(PRBool aIsVisual) = 0;

  /**
   *  Check whether the content should be treated as visual.
   *
   *  @lina 05/02/2000
   */
  NS_IMETHOD IsVisualMode(PRBool& aIsVisual) const = 0;

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
  NS_IMETHOD GetBidi(PRUint32* aBidiOptions) = 0;
//ahmed

  /**
   * Check for Bidi text mode and direction
   * @return aResult == TRUE if the text mode is visual and the direction is right-to-left
   */
  NS_IMETHOD IsVisRTL(PRBool &aResult) = 0;

  /**
   * Check for Arabic encoding
   * @return aResult == TRUE if the document encoding is an Arabic codepage
   */
  NS_IMETHOD IsArabicEncoding(PRBool &aResult) = 0;

  /**
   * Set the Bidi capabilities of the system
   * @param aIsBidi == TRUE if the system has the capability of reordering Bidi text
   */
  NS_IMETHOD SetIsBidiSystem(PRBool aIsBidi) = 0;

  /**
   * Get the Bidi capabilities of the system
   * @return aResult == TRUE if the system has the capability of reordering Bidi text
   */
  NS_IMETHOD GetIsBidiSystem(PRBool &aResult) const = 0;

  /**
   * Get the document charset
   */
  NS_IMETHOD GetBidiCharset(nsAWritableString &aCharSet) = 0;
#endif // IBMBIDI

  /**
   * Render only Selection
   */
  NS_IMETHOD SetIsRenderingOnlySelection(PRBool aResult) = 0;
  NS_IMETHOD IsRenderingOnlySelection(PRBool* aResult) = 0;

#ifdef MOZ_REFLOW_PERF
  NS_IMETHOD CountReflows(const char * aName, PRUint32 aType, nsIFrame * aFrame) = 0;
  NS_IMETHOD PaintCount(const char * aName, nsIRenderingContext* aRendingContext, nsIFrame * aFrame, PRUint32 aColor) = 0;
#endif
};

// Bit values for StartLoadImage's aImageStatus
#define NS_LOAD_IMAGE_STATUS_ERROR      0x1
#define NS_LOAD_IMAGE_STATUS_SIZE       0x2
#define NS_LOAD_IMAGE_STATUS_BITS       0x4

// Factory method to create a "galley" presentation context (galley is
// a kind of view that has no limit to the size of a page)
extern NS_EXPORT nsresult
  NS_NewGalleyContext(nsIPresContext** aInstancePtrResult);

// Factory method to create a "paginated" presentation context for
// the screen.
extern NS_EXPORT nsresult
  NS_NewPrintPreviewContext(nsIPresContext** aInstancePtrResult);



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
