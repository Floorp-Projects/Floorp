/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsIPresContext_h___
#define nsIPresContext_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsRect.h"
#include "nsColor.h"
#include "nsIFrameImageLoader.h"

struct nsFont;

class nsIContent;
class nsIDocument;
class nsIDeviceContext;
class nsIFontMetrics;
class nsIFrame;
class nsIImageGroup;
class nsILinkHandler;
class nsIPresShell;
class nsIPref;
class nsIStyleContext;
class nsIAtom;
class nsString;
class nsIEventStateManager;
class nsIURL;

#define NS_IPRESCONTEXT_IID   \
{ 0x0a5d12e0, 0x944e, 0x11d1, \
  {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

enum nsCompatibility {
  eCompatibility_Standard   = 1,
  eCompatibility_NavQuirks  = 2
};

// An interface for presentation contexts. Presentation contexts are
// objects that provide an outer context for a presentation shell.
class nsIPresContext : public nsISupports {
public:
  /**
   * Initialize the presentation context from a particular device.
   */
  NS_IMETHOD Init(nsIDeviceContext* aDeviceContext, nsIPref* aPrefs) = 0;

  /**
   * Stop the presentation in preperation for destruction.
   */
  NS_IMETHOD Stop(void) = 0;

  /**
   * Set the presentation shell that this context is bound to.
   * A presentation context may only be bound to a single shell.
   */
  NS_IMETHOD SetShell(nsIPresShell* aShell) = 0;

  /**
   * Get the PresentationShell that this context is bound to.
   */
  virtual nsIPresShell* GetShell() = 0;

  /**
   * Get a reference to the prefs API for this context
   */
  NS_IMETHOD GetPrefs(nsIPref*& aPrefs) = 0;

  /**
   * Access compatibility mode for this context
   */
  NS_IMETHOD GetCompatibilityMode(nsCompatibility& aMode) = 0;
  NS_IMETHOD SetCompatibilityMode(nsCompatibility aMode) = 0;

  /** 
   * Get base url for presentation
   */
  NS_IMETHOD GetBaseURL(nsIURL*& aURL) = 0;

  /** 
   * Get medium of presentation
   */
  NS_IMETHOD GetMedium(nsIAtom*& aMedium) = 0;

  /**
   * Resolve style for the given piece of content that will be a child
   * of the aParentContext. Don't use this for pseudo frames.
   */
  virtual nsIStyleContext* ResolveStyleContextFor(nsIContent* aContent,
                                                  nsIStyleContext* aParentContext,
                                                  PRBool aForceUnique = PR_FALSE) = 0;

  /**
   * Resolve style for a pseudo frame within the given aParentContent & aParentContext.
   * The tag should be uppercase and inclue the colon.
   * ie: NS_NewAtom(":FIRST-LINE");
   */
  virtual nsIStyleContext* ResolvePseudoStyleContextFor(nsIContent* aParentContent,
                                                        nsIAtom* aPseudoTag,
                                                        nsIStyleContext* aParentContext,
                                                        PRBool aForceUnique = PR_FALSE) = 0;

  /**
   * Probe style for a pseudo frame within the given aParentContent & aParentContext.
   * This will return nsnull id there are no explicit rules for the pseudo element.
   * The tag should be uppercase and inclue the colon.
   * ie: NS_NewAtom(":FIRST-LINE");
   */
  virtual nsIStyleContext* ProbePseudoStyleContextFor(nsIContent* aParentContent,
                                                      nsIAtom* aPseudoTag,
                                                      nsIStyleContext* aParentContext,
                                                      PRBool aForceUnique = PR_FALSE) = 0;

  /**
   * Get the font metrics for a given font.
   */
  virtual nsIFontMetrics* GetMetricsFor(const nsFont& aFont) = 0;

  /** 
   * Get the default font
   */
  virtual const nsFont& GetDefaultFont(void) = 0;

  /** 
   * Get the default fixed pitch font
   */
  virtual const nsFont& GetDefaultFixedFont(void) = 0;

  /**
   * Access Nav's magic font scaler value
   */
  NS_IMETHOD GetFontScaler(PRInt32& aResult) = 0;
  NS_IMETHOD SetFontScaler(PRInt32 aScaler) = 0;

  /** 
   * Get the defualt colors
   */
  NS_IMETHOD GetDefaultColor(nscolor& aColor) = 0;
  NS_IMETHOD GetDefaultBackgroundColor(nscolor& aColor) = 0;
  NS_IMETHOD SetDefaultColor(const nscolor& aColor) = 0;
  NS_IMETHOD SetDefaultBackgroundColor(const nscolor& aColor) = 0;

  NS_IMETHOD GetImageGroup(nsIImageGroup*& aGroupResult) = 0;

  /**
   * Load an image for the target frame. This call can be made
   * repeated with only a single image ever being loaded. If
   * aNeedSizeUpdate is PR_TRUE, then when the image's size is
   * determined the target frame will be reflowed (via a
   * ContentChanged notification on the presentation shell). When the
   * image's data is ready for rendering the target frame's Paint()
   * method will be invoked (via the ViewManager) so that the
   * appropriate damage repair is done.
   *
   * @param aBackgroundColor - If the background color is NULL, a mask
   *      will be generated for transparent images. If the background
   *      color is non-NULL, it indicates the RGB value to be folded
   *      into the transparent areas of the image and no mask is created.
   */
  NS_IMETHOD StartLoadImage(const nsString& aURL,
                            const nscolor* aBackgroundColor,
                            nsIFrame* aTargetFrame,
                            nsFrameImageLoaderCB aCallBack,
                            PRBool aNeedSizeUpdate,
                            PRBool aNeedErrorNotification,
                            nsIFrameImageLoader*& aLoader) = 0;

  /**
   * Stop any image loading being done on behalf of the argument frame.
   */
  NS_IMETHOD StopLoadImage(nsIFrame* aForFrame) = 0;

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
  virtual PRBool IsPaginated() = 0;

  /**
   * Return the page width if this is a paginated context.
   */
  virtual nscoord GetPageWidth() = 0;

  /**
   * Return the page height if this is a paginated context
   */
  virtual nscoord GetPageHeight() = 0;

  virtual float GetPixelsToTwips() const = 0;
  virtual float GetTwipsToPixels() const = 0;

  //XXX this is probably not an ideal name. MMP
  /** 
   * Do pixels to twips conversion taking into account
   * differing size of a "pixel" from device to device.
   */
  NS_IMETHOD GetScaledPixelsToTwips(float &aScale) const = 0;

  //be sure to Relase() after you are done with the Get()
  virtual nsIDeviceContext * GetDeviceContext() const = 0;

  NS_IMETHOD GetEventStateManager(nsIEventStateManager** aManager) = 0;
};

// Bit values for StartLoadImage's aImageStatus
#define NS_LOAD_IMAGE_STATUS_ERROR      0x1
#define NS_LOAD_IMAGE_STATUS_SIZE       0x2
#define NS_LOAD_IMAGE_STATUS_BITS       0x4

// Factory method to create a "galley" presentation context (galley is
// a kind of view that has no limit to the size of a page)
extern NS_LAYOUT nsresult
  NS_NewGalleyContext(nsIPresContext** aInstancePtrResult);

// Factory method to create a "paginated" presentation context for
// the screen.
extern NS_LAYOUT nsresult
  NS_NewPrintPreviewContext(nsIPresContext** aInstancePtrResult);

// Factory method to create a "paginated" presentation context for
// printing
extern NS_LAYOUT nsresult
  NS_NewPrintContext(nsIPresContext** aInstancePtrResult);

#endif /* nsIPresContext_h___ */
