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
#ifndef nsIPresShell_h___
#define nsIPresShell_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsCoord.h"

class nsIContent;
class nsIDocument;
class nsIDocumentObserver;
class nsIFrame;
class nsIPresContext;
class nsIStyleSet;
class nsIViewManager;
class nsIReflowCommand;
class nsIDeviceContext;
class nsIRenderingContext;
class nsIPageSequenceFrame;
class nsIDOMSelection;
class nsString;
class nsStringArray;
class nsICaret;
class nsIStyleContext;
#ifndef NEW_CLIPBOARD_SUPPORT
class nsISelectionMgr;
#endif

#define NS_IPRESSHELL_IID     \
{ 0x76e79c60, 0x944e, 0x11d1, \
  {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

#define NS_PRESSHELL_SCROLL_TOP      0x00000001
#define NS_PRESSHELL_SCROLL_BOTTOM   0x00000002
#define NS_PRESSHELL_SCROLL_LEFT     0x00000004
#define NS_PRESSHELL_SCROLL_RIGHT    0x00000008
#define NS_PRESSHELL_SCROLL_ANYWHERE 0x00000010
#define NS_PRESSHELL_SCROLL_ENTIRE   0x00000020

/**
 * Presentation shell interface. Presentation shells are the
 * controlling point for managing the presentation of a document. The
 * presentation shell holds a live reference to the document, the
 * presentation context, the style manager, the style set and the root
 * frame. <p>
 *
 * When this object is Release'd, it will release the document, the
 * presentation context, the style manager, the style set and the root
 * frame.
 */
class nsIPresShell : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IPRESSHELL_IID; return iid; }

  NS_IMETHOD Init(nsIDocument* aDocument,
                  nsIPresContext* aPresContext,
                  nsIViewManager* aViewManager,
                  nsIStyleSet* aStyleSet) = 0;

  NS_IMETHOD GetDocument(nsIDocument** aResult) = 0;

  NS_IMETHOD GetPresContext(nsIPresContext** aResult) = 0;

  NS_IMETHOD GetViewManager(nsIViewManager** aResult) = 0;

  NS_IMETHOD GetStyleSet(nsIStyleSet** aResult) = 0;

  NS_IMETHOD GetActiveAlternateStyleSheet(nsString& aSheetTitle) = 0;

  NS_IMETHOD SelectAlternateStyleSheet(const nsString& aSheetTitle) = 0;

  /**
   * Gather titles of all selectable (alternate and preferred) style sheets
   * fills void array with nsString* caller must free strings
   */
  NS_IMETHOD ListAlternateStyleSheets(nsStringArray& aTitleList) = 0;

  /**
   * GetSelection will return the selection that the presentation
   *  shell may implement.
   *
   * @param aSelection will hold the return value
   */
  NS_IMETHOD GetSelection(nsIDOMSelection** aSelection) = 0;

  NS_IMETHOD EnterReflowLock() = 0;

  NS_IMETHOD ExitReflowLock() = 0;

  // Make shell be a document observer
  NS_IMETHOD BeginObservingDocument() = 0;

  // Make shell stop being a document observer
  NS_IMETHOD EndObservingDocument() = 0;

  /**
   * Perform the initial reflow. Constructs the frame for the root content
   * object and then reflows the frame model into the specified width and
   * height.
   *
   * The coordinates for aWidth and aHeight must be in standard nscoord's.
   */
  NS_IMETHOD InitialReflow(nscoord aWidth, nscoord aHeight) = 0;

  /**
   * Reflow the frame model into a new width and height.  The
   * coordinates for aWidth and aHeight must be in standard nscoord's.
   */
  NS_IMETHOD ResizeReflow(nscoord aWidth, nscoord aHeight) = 0;

  /**
   * Reflow the frame model with a reflow reason of eReflowReason_StyleChange
   */
  NS_IMETHOD StyleChangeReflow() = 0;

  NS_IMETHOD GetRootFrame(nsIFrame** aFrame) const = 0;

  /**
   * Returns the page sequence frame associated with the frame hierarchy.
   * Returns NULL if not a paginated view.
   */
  NS_IMETHOD GetPageSequenceFrame(nsIPageSequenceFrame** aResult) const = 0;

  /**
   * Returns the primary frame associated with the content object.
   *
   * The primary frame is the frame that is most closely associated with the
   * content. A frame is more closely associated with the content that another
   * frame if the one frame contains directly or indirectly the other frame (e.g.,
   * when a frame is scrolled there is a scroll frame that contains the frame
   * being scrolled). The primary frame is always the first-in-flow.
   *
   * In the case of absolutely positioned elements and floated elements,
   * the primary frame is the frame that is out of the flow and not the
   * placeholder frame.
   */
  NS_IMETHOD GetPrimaryFrameFor(nsIContent* aContent,
                                nsIFrame**  aPrimaryFrame) const = 0;

  /** Returns the style context associated with the frame.
    * Used by code outside of layout that can't use nsIFrame methods to get
    * the style context directly.
    */
  NS_IMETHOD GetStyleContextFor(nsIFrame*         aFrame,
                                nsIStyleContext** aStyleContext) const = 0;

  /**
   * Returns a layout object associated with the primary frame for the content object.
   *
   * @param aContent   the content object for which we seek a layout object
   * @param aResult    the resulting layout object as an nsISupports, if found.  Refcounted.
   */
  NS_IMETHOD GetLayoutObjectFor(nsIContent*   aContent,
                                nsISupports** aResult) const = 0;

  /**
   * Get/Set the placeholder frame associated with the specified frame.
   *
   * Out of flow frames (e.g., absolutely positioned frames and floated frames)
   * can have placeholder frames that are inserted into the flow and indicate
   * where the frame would be if it were part of the flow
   */
  NS_IMETHOD GetPlaceholderFrameFor(nsIFrame*  aFrame,
                                    nsIFrame** aPlaceholderFrame) const = 0;
  NS_IMETHOD SetPlaceholderFrameFor(nsIFrame* aFrame,
                                    nsIFrame* aPlaceholderFrame) = 0;

  NS_IMETHOD AppendReflowCommand(nsIReflowCommand* aReflowCommand) = 0;

  NS_IMETHOD ProcessReflowCommands() = 0;

  NS_IMETHOD ClearFrameRefs(nsIFrame* aFrame) = 0;

  /**
   * Given a frame, cough up a rendering context suitable for use with
   * the frame.
   */
  NS_IMETHOD CreateRenderingContext(nsIFrame *aFrame,
                                    nsIRenderingContext** aContext) = 0;

  // Notification that we were unable to render a replaced element.
  // Called when the replaced element can not be rendered, and we should
  // instead render the element's contents.
  // The content object associated with aFrame should either be a IMG
  // element, an OBJECT element, or an APPLET element
  NS_IMETHOD CantRenderReplacedElement(nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame) = 0;


  /**
   * Scrolls the view of the document so that the anchor with the specified
   * name is displayed at the top of the window
   */
  NS_IMETHOD GoToAnchor(const nsString& aAnchorName) const = 0;

  /**
   * Scrolls the view of the document so that the frame is displayed at the 
   * top of the window.
   * @param aFrame           The frame to scroll into view
   * @param aVOffsetPercent  The percentage to offset aFrame from the view vertically.  
   *                         Interpretation of aVOffsetPercent is controlled by aVFlags, see below.
   *                         Legal values are 0..100, with 0 
   *                         being flush to the (eTop|eBottom) edge, 100 being flush to the other edge.
   * @param aVFlags          The control flags for vertical scrolling.  
   *                         Legal values are an OR combination of:
   *                         NS_PRESSHELL_SCROLL_TOP
   *                                      position the top of the frame aVOffsetPercent
   *                                      from the top of the view
   *                         NS_PRESSHELL_SCROLL_BOTTOM
   *                                      position the bottom of the frame aVOffsetPercent
   *                                      from the bottom of the view
   *                         NS_PRESSHELL_SCROLL_ANYWHERE
   *                                      if the (eTop|eBottom) of the frame is visible, do nothing 
   *                                      else scroll the frame according to the other bits 
   *                         NS_PRESSHELL_SCROLL_ENTIRE
   *                                      Force the entire frame to be visible, using aVOffsetPercent 
   *                                      as a starting point, but scrolling the (eTop|eBottom)
   *                                      of the frame enough for the entire frame to fit in the view, 
   *                                      if possible 
   * @param aHOffsetPercent  The percentage to offset aFrame from the view horizontally.  
   *                         Interpretation of aHOffsetPercent is controlled by aHFlags, see below.
   *                         Legal values are 0..100, with 0 
   *                         being flush to the (eLeft|eRight) edge, 100 being flush to the other edge.
   * @param aHFlags          The control flags for vertical scrolling.  
   *                         Legal values are an OR combination of:
   *                         NS_PRESSHELL_SCROLL_LEFT
   *                                      position the left of the frame aHOffsetPercent
   *                                      from the left of the view
   *                         NS_PRESSHELL_SCROLL_RIGHT
   *                                      Position the right of the frame aHOffsetPercent
   *                                      from the right of the view
   *                         NS_PRESSHELL_SCROLL_ANYWHERE
   *                                      If the (eLeft|eRight) of the frame is visible, do nothing 
   *                                      else scroll the frame according to the other bits 
   *                         NS_PRESSHELL_SCROLL_ENTIRE
   *                                      Force the entire frame to be visible, using aVOffsetPercent 
   *                                      as a starting point, but scrolling the (eLeft|eRight)
   *                                      of the frame enough for the entire frame to fit in the view, 
   *                                      if possible 
   */
  NS_IMETHOD ScrollFrameIntoView(nsIFrame *aFrame,
                                 PRInt32   aVOffsetPercent, 
                                 PRUint32  aVFlags,
                                 PRInt32   aHOffsetPercent, 
                                 PRUint32  aHFlags) const = 0;

  /**
   * Notify the SelectionMgr that we have something to copy.
   */
#ifndef NEW_CLIPBOARD_SUPPORT
  NS_IMETHOD DoCopy(nsISelectionMgr* aSelectionMgr) = 0;
#else
  NS_IMETHOD DoCopy() = 0;
#endif

  /**
   * Get the caret, if it exists. AddRefs it.
   */
  NS_IMETHOD GetCaret(nsICaret **outCaret) = 0;
  
  /**
   * Set the caret as enabled or disabled. An enabled caret will
   * draw orblink when made visible. A disabled caret will never show up.
   */
  NS_IMETHOD SetCaretEnabled(PRBool inEnable) = 0;

  // XXX events
  // XXX selection

  /**
   * See if reflow verification is enabled. To enable reflow verification add
   * "verifyreflow:1" to your NSPR_LOG_MODULES environment variable
   * (any non-zero debug level will work). Or, call SetVerifyReflowEnable
   * with PR_TRUE.
   */
  static NS_LAYOUT PRBool GetVerifyReflowEnable();

  /**
   * Set the verify-reflow enable flag.
   */
  static NS_LAYOUT void SetVerifyReflowEnable(PRBool aEnabled);
};

/**
 * Create a new empty presentation shell. Upon success, call Init
 * before attempting to use the shell.
 */
extern NS_LAYOUT nsresult
  NS_NewPresShell(nsIPresShell** aInstancePtrResult);

#endif /* nsIPresShell_h___ */
