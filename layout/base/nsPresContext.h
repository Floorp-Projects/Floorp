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

struct nsFont;

class nsIContent;
class nsIDeviceContext;
class nsIFontMetrics;
class nsIFrame;
class nsIFrameImageLoader;
class nsIImageGroup;
class nsILinkHandler;
class nsIPresShell;
class nsIStyleContext;
class nsIAtom;
class nsString;
class nsIEventStateManager;

#define NS_IPRESCONTEXT_IID   \
{ 0x0a5d12e0, 0x944e, 0x11d1, \
  {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }


// An interface for presentation contexts. Presentation contexts are
// objects that provide an outer context for a presentation shell.
class nsIPresContext : public nsISupports {
public:
  /**
   * Set the presentation shell that this context is bound to.
   * A presentation context may only be bound to a single shell.
   */
  virtual void SetShell(nsIPresShell* aShell) = 0;

  /**
   * Get the PresentationShell that this context is bound to.
   */
  virtual nsIPresShell* GetShell() = 0;

  /**
   * Resolve style for the given piece of content that will be a child
   * of the aParentFrame frame. Don't use this for pseudo frames.
   */
  virtual nsIStyleContext* ResolveStyleContextFor(nsIContent* aContent,
                                                  nsIFrame* aParentFrame,
                                                  PRBool aForceUnique = PR_FALSE) = 0;

  /**
   * Resolve style for a pseudo frame within the given aParentFrame frame.
   * The tag should be uppercase and inclue the colon.
   * ie: NS_NewAtom(":FIRST-LINE");
   */
  virtual nsIStyleContext* ResolvePseudoStyleContextFor(nsIAtom* aPseudoTag,
                                                        nsIFrame* aParentFrame,
                                                        PRBool aForceUnique = PR_FALSE) = 0;

  /**
   * Probe style for a pseudo frame within the given aParentFrame frame.
   * This will return nsnull id there are no explicit rules for the pseudo element.
   * The tag should be uppercase and inclue the colon.
   * ie: NS_NewAtom(":FIRST-LINE");
   */
  virtual nsIStyleContext* ProbePseudoStyleContextFor(nsIAtom* aPseudoTag,
                                                      nsIFrame* aParentFrame,
                                                      PRBool aForceUnique = PR_FALSE) = 0;
  /**
   * Get the font metrics for a given font.
   */
  virtual nsIFontMetrics* GetMetricsFor(const nsFont& aFont) = 0;

  /** 
   * Get the default font
   */
  virtual const nsFont& GetDefaultFont(void) = 0;

  NS_IMETHOD GetImageGroup(nsIImageGroup*& aGroupResult) = 0;

  /**
   * Load an image for the target frame. This call can be made
   * repeated with only a single image every being loaded. If
   * aNeedSizeUpdate is PR_TRUE then when the image's size is
   * determined the target frame will be reflowed (via a
   * ContentChanged notification on the presentation shell). When the
   * image's data is ready for rendering the target frames Paint
   * method will be invoked (via the ViewManager so that the
   * appropriate damage repair is done).
   */
  NS_IMETHOD LoadImage(const nsString& aURL,
                       nsIFrame* aTargetFrame,
                       PRBool aNeedSizeUpdate,
                       nsIFrameImageLoader*& aLoader) = 0;

  /**
   * Stop any image loading being done on behalf of the argument frame.
   */
  NS_IMETHOD StopLoadImage(nsIFrame* aForFrame) = 0;

  NS_IMETHOD SetContainer(nsISupports* aContainer) = 0;

  NS_IMETHOD GetContainer(nsISupports** aResult) = 0;

  NS_IMETHOD SetLinkHandler(nsILinkHandler* aHandler) = 0;

  NS_IMETHOD GetLinkHandler(nsILinkHandler** aResult) = 0;

  /**
   * Get the currently visible portion of the RootContent ILayoutable.
   * The units returned are in pixels. An acceptable answer is "null"
   * which means "I don't know".
   * <h2>Speculative</h2>
   */
  virtual void GetVisibleArea(nsRect& aResult) = 0;

  /**
   * Set the currently visible area. This is the size and location
   * of the visible portion of the RootContent ILayoutable.
   */
  virtual void SetVisibleArea(const nsRect& r) = 0;

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

  //be sure to Relase() after you are done with the Get()
  virtual nsIDeviceContext * GetDeviceContext() const = 0;

  NS_IMETHOD GetEventStateManager(nsIEventStateManager** aManager) = 0;
};

// Bit values for LoadImage's aImageStatus
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

#endif /* nsIPresContext_h___ */
