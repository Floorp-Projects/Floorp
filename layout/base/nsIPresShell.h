/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s):
 *   Steve Clark <buster@netscape.com>
 *   Dan Rosen <dr@netscape.com>
 *
 *   IBM Corporation
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 05/03/2000   IBM Corp.       Observer related defines for reflow
 */
#ifndef nsIPresShell_h___
#define nsIPresShell_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsCoord.h"
#include "nsIReflowCommand.h"
#include "nsEvent.h"

class nsIContent;
class nsIContentIterator;
class nsIDocument;
class nsIDocumentObserver;
class nsIFrame;
class nsIPresContext;
class nsIStyleSet;
class nsIViewManager;
class nsIDeviceContext;
class nsIRenderingContext;
class nsIPageSequenceFrame;
class nsString;
class nsStringArray;
class nsICaret;
class nsIStyleContext;
class nsIFrameSelection;
class nsIFrameManager;
class nsILayoutHistoryState;
class nsIArena;
class nsIReflowCallback;
class nsISupportsArray;
class nsIDOMNode;

#define NS_IPRESSHELL_IID     \
{ 0x76e79c60, 0x944e, 0x11d1, \
  {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

// Constants uses for ScrollFrameIntoView() function
#define NS_PRESSHELL_SCROLL_TOP      0
#define NS_PRESSHELL_SCROLL_BOTTOM   100
#define NS_PRESSHELL_SCROLL_LEFT     0
#define NS_PRESSHELL_SCROLL_RIGHT    100
#define NS_PRESSHELL_SCROLL_CENTER   50
#define NS_PRESSHELL_SCROLL_ANYWHERE -1
#define NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE -2

// Observer related defines
#define NS_PRESSHELL_REFLOW_TOPIC         "REFLOW"              // Observer Topic
#define NS_PRESSHELL_INITIAL_REFLOW       "INITIAL REFLOW"      // Observer Data
#define NS_PRESSHELL_RESIZE_REFLOW        "RESIZE REFLOW"       // Observer Data
#define NS_PRESSHELL_STYLE_CHANGE_REFLOW  "STYLE CHANGE REFLOW" // Observer Data

// debug VerifyReflow flags
#define VERIFY_REFLOW_ON              0x01
#define VERIFY_REFLOW_NOISY           0x02
#define VERIFY_REFLOW_ALL             0x04
#define VERIFY_REFLOW_DUMP_COMMANDS   0x08
#define VERIFY_REFLOW_NOISY_RC        0x10
#define VERIFY_REFLOW_REALLY_NOISY_RC 0x20
#define VERIFY_REFLOW_INCLUDE_SPACE_MANAGER 0x40
#define VERIFY_REFLOW_DURING_RESIZE_REFLOW  0x80

#ifdef IBMBIDI // Constant for Set/Get CaretBidiLevel
#define BIDI_LEVEL_UNDEFINED 0x80
#endif

// for PostAttributeChanged
enum nsAttributeChangeType {
  eChangeType_Set = 0,       // Set attribute
  eChangeType_Remove = 1     // Remove attribute
};

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

  /**
   * All callers are responsible for calling |Destroy| after calling
   * |EndObservingDocument|.  It needs to be separate only because form
   * controls incorrectly store their data in the frames rather than the
   * content model and printing calls |EndObservingDocument| multiple
   * times to make form controls behave nicely when printed.
   */
  NS_IMETHOD Destroy() = 0;

  // All frames owned by the shell are allocated from an arena.  They are also recycled
  // using free lists (separate free lists being maintained for each size_t).
  // Methods for recycling frames.
  NS_IMETHOD AllocateFrame(size_t aSize, void** aResult) = 0;
  NS_IMETHOD FreeFrame(size_t aSize, void* aFreeChunk) = 0;

  // Dynamic stack memory allocation
  NS_IMETHOD PushStackMemory() = 0;
  NS_IMETHOD PopStackMemory() = 0;
  NS_IMETHOD AllocateStackMemory(size_t aSize, void** aResult) = 0;
  
  NS_IMETHOD GetDocument(nsIDocument** aResult) = 0;

  NS_IMETHOD GetPresContext(nsIPresContext** aResult) = 0;

  NS_IMETHOD GetViewManager(nsIViewManager** aResult) = 0;

  NS_IMETHOD GetStyleSet(nsIStyleSet** aResult) = 0;

  NS_IMETHOD GetActiveAlternateStyleSheet(nsString& aSheetTitle) = 0;

  NS_IMETHOD SelectAlternateStyleSheet(const nsString& aSheetTitle) = 0;

  /** Setup all style rules required to implement preferences
   * - used for background/text/link colors and link underlining
   *    may be extended for any prefs that are implemented via style rules
   * - aForceReflow argument is used to force a full reframe to make the rules show
   *   (only used when the current page needs to reflect changed pref rules)
   *
   * - initially created for bugs 31816, 20760, 22963
   */
  NS_IMETHOD SetPreferenceStyleRules(PRBool aForceReflow) = 0;

  /** Allow client to enable and disable the use of the preference style rules,
   *  by type. 
   *  NOTE: type argument is currently ignored, but is in the API for 
   *        future refinement
   *
   *  - initially created for bugs 31816, 20760, 22963
   */
  NS_IMETHOD EnablePrefStyleRules(PRBool aEnable, PRUint8 aPrefType=0xFF) = 0;
  NS_IMETHOD ArePrefStyleRulesEnabled(PRBool& aEnabled) = 0;

  /**
   * Gather titles of all selectable (alternate and preferred) style sheets
   * fills void array with nsString* caller must free strings
   */
  NS_IMETHOD ListAlternateStyleSheets(nsStringArray& aTitleList) = 0;

  /**
   * GetFrameSelection will return the Frame based selection API you 
   *  cannot go back and forth anymore with QI with nsIDOM sel and nsIFrame sel.
   */
  NS_IMETHOD GetFrameSelection(nsIFrameSelection** aSelection) = 0;  

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
   * Gets the primary frame associated with the content object. This is a
   * helper function that just forwards the request to the frame manager.
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
   * Returns the object that acts as a subshell for aContent.
   * For example, for an html frame aResult will be an nsIDocShell.
   */
  NS_IMETHOD GetSubShellFor(nsIContent*   aContent,
                            nsISupports** aResult) const = 0;

  /**
   * Establish a relationship between aContent and aSubShell.
   * aSubShell will be returned from GetSubShellFor(aContent, ...);
   */
  NS_IMETHOD SetSubShellFor(nsIContent*  aContent,
                            nsISupports* aSubShell) = 0;

  /**
   * Find the content in the map that has aSubShell set
   * as its subshell.  If there is more than one mapping for
   * aSubShell, the result is undefined.
   */
  NS_IMETHOD FindContentForShell(nsISupports* aSubShell,
                                 nsIContent** aContent) const = 0;

  /**
   * Gets the placeholder frame associated with the specified frame. This is
   * a helper frame that forwards the request to the frame manager.
   */
  NS_IMETHOD GetPlaceholderFrameFor(nsIFrame*  aFrame,
                                    nsIFrame** aPlaceholderFrame) const = 0;

  /**
   * Reflow commands
   */
  NS_IMETHOD AppendReflowCommand(nsIReflowCommand* aReflowCommand) = 0;
  NS_IMETHOD CancelReflowCommand(nsIFrame* aTargetFrame, nsIReflowCommand::ReflowType* aCmdType) = 0;
  NS_IMETHOD CancelAllReflowCommands() = 0;


  /**
   * Determine if it is safe to flush all pending notifications
   * @param aIsSafeToFlush PR_TRUE if it is safe, PR_FALSE otherwise.
   * 
   */
  NS_IMETHOD IsSafeToFlush(PRBool& aIsSafeToFlush) = 0;

  /**
   * Flush all pending notifications such that the presentation is
   * in sync with the content.
   * @param aUpdateViews PR_TRUE causes the affected views to be refreshed immediately.
   */
  NS_IMETHOD FlushPendingNotifications(PRBool aUpdateViews) = 0;

  /**
   * Post a request to handle a DOM event after Reflow has finished.
   */
  NS_IMETHOD PostDOMEvent(nsIContent* aContent, nsEvent* aEvent)=0;

  /**
   * Post a request to set and attribute after reflow has finished.
   */
  NS_IMETHOD PostAttributeChange(nsIContent* aContent,
                                 PRInt32 aNameSpaceID, 
                                 nsIAtom* aName,
                                 const nsString& aValue,
                                 PRBool aNotify,
                                 nsAttributeChangeType aType) = 0;

  NS_IMETHOD PostReflowCallback(nsIReflowCallback* aCallback) = 0;
  NS_IMETHOD CancelReflowCallback(nsIReflowCallback* aCallback) = 0;
 /**
   * Reflow batching
   */   
  NS_IMETHOD BeginReflowBatching() = 0;
  NS_IMETHOD EndReflowBatching(PRBool aFlushPendingReflows) = 0;
  NS_IMETHOD GetReflowBatchingStatus(PRBool* aIsBatching) = 0;

  NS_IMETHOD ClearFrameRefs(nsIFrame* aFrame) = 0;

  /**
   * Given a frame, create a rendering context suitable for use with
   * the frame.
   */
  NS_IMETHOD CreateRenderingContext(nsIFrame *aFrame,
                                    nsIRenderingContext** aContext) = 0;

  /**
   * Notification that we were unable to render a replaced element.
   * Called when the replaced element can not be rendered, and we should
   * instead render the element's contents.
   * The content object associated with aFrame should either be a IMG
   * element, an OBJECT element, or an APPLET element
   */
  NS_IMETHOD CantRenderReplacedElement(nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame) = 0;


  /**
   * Scrolls the view of the document so that the anchor with the specified
   * name is displayed at the top of the window
   */
  NS_IMETHOD GoToAnchor(const nsString& aAnchorName) = 0;

  /**
   * Scrolls the view of the document so that the frame is displayed at the 
   * top of the window.
   *
   * @param aFrame    The frame to scroll into view
   * @param aVPercent How to align the frame vertically. A value of 0
   *                    (NS_PRESSHELL_SCROLL_TOP) means the frame's upper edge is
   *                    aligned with the top edge of the visible area. A value of
   *                    100 (NS_PRESSHELL_SCROLL_BOTTOM) means the frame's bottom
   *                    edge is aligned with the bottom edge of the visible area.
   *                    For values in between, the point "aVPercent" down the frame
   *                    is placed at the point "aVPercent" down the visible area. A
   *                    value of 50 (NS_PRESSHELL_SCROLL_CENTER) centers the frame
   *                    vertically. A value of NS_PRESSHELL_SCROLL_ANYWHERE means move
   *                    the frame the minimum amount necessary in order for the entire
   *                    frame to be visible vertically (if possible)
   * @param aHPercent How to align the frame horizontally. A value of 0
   *                    (NS_PRESSHELL_SCROLL_LEFT) means the frame's left edge is
   *                    aligned with the left edge of the visible area. A value of
   *                    100 (NS_PRESSHELL_SCROLL_RIGHT) means the frame's right
   *                    edge is aligned with the right edge of the visible area.
   *                    For values in between, the point "aVPercent" across the frame
   *                    is placed at the point "aVPercent" across the visible area.
   *                    A value of 50 (NS_PRESSHELL_SCROLL_CENTER) centers the frame
   *                    horizontally . A value of NS_PRESSHELL_SCROLL_ANYWHERE means move
   *                    the frame the minimum amount necessary in order for the entire
   *                    frame to be visible horizontally (if possible)
   */
  NS_IMETHOD ScrollFrameIntoView(nsIFrame *aFrame,
                                 PRIntn   aVPercent, 
                                 PRIntn   aHPercent) const = 0;

  /**
   * Notification sent by a frame informing the pres shell that it is about to
   * be destroyed.
   * This allows any outstanding references to the frame to be cleaned up
   */
  NS_IMETHOD NotifyDestroyingFrame(nsIFrame* aFrame) = 0;

  /**
   * Returns the frame manager object
   */
  NS_IMETHOD GetFrameManager(nsIFrameManager** aFrameManager) const = 0;

  /**
   * Notify the Clipboard that we have something to copy.
   */
  NS_IMETHOD DoCopy() = 0;

  /**
   * Copy link location.
   */
  NS_IMETHOD DoCopyLinkLocation(nsIDOMNode* aNode) = 0;

  /**
   * Copy image methods.
   */
  NS_IMETHOD DoCopyImageLocation(nsIDOMNode* aNode) = 0;
  NS_IMETHOD DoCopyImageContents(nsIDOMNode* aNode) = 0;

  /**
   * Get the caret, if it exists. AddRefs it.
   */
  NS_IMETHOD GetCaret(nsICaret **aOutCaret) = 0;

  /**
   * Should the images have borders etc.  Actual visual effects are determined
   * by the frames.  Visual effects may not effect layout, only display.
   * Takes effect on next repaint, does not force a repaint itself.
   *
   * @param aEnabled  if PR_TRUE, visual selection effects are enabled
   *                  if PR_FALSE visual selection effects are disabled
   * @return  always NS_OK
   */
  NS_IMETHOD SetDisplayNonTextSelection(PRBool aInEnable) = 0;

  /** 
    * Gets the current state of non text selection effects
    * @param aEnabled  [OUT] set to the current state of non text selection,
    *                  as set by SetDisplayNonTextSelection
    * @return   if aOutEnabled==null, returns NS_ERROR_INVALID_ARG
    *           else NS_OK
    */
  NS_IMETHOD GetDisplayNonTextSelection(PRBool *aOutEnabled) = 0;

  /**
    * Interface to dispatch events via the presshell
    */
  NS_IMETHOD HandleEventWithTarget(nsEvent* aEvent, 
                                   nsIFrame* aFrame, 
                                   nsIContent* aContent,
                                   PRUint32 aFlags,
                                   nsEventStatus* aStatus) = 0;

  /**
   * Dispatch event to content only (NOT full processing)
   */
  NS_IMETHOD HandleDOMEventWithTarget(nsIContent* aTargetContent,
                            nsEvent* aEvent,
                            nsEventStatus* aStatus) = 0;

  /**
    * Gets the current target event frame from the PresShell
    */
  NS_IMETHOD GetEventTargetFrame(nsIFrame** aFrame) = 0;

  /**
   * Get and set the history state for the current document 
   */

  NS_IMETHOD CaptureHistoryState(nsILayoutHistoryState** aLayoutHistoryState, PRBool aLeavingPage = PR_FALSE) = 0;
  NS_IMETHOD GetHistoryState(nsILayoutHistoryState** aLayoutHistoryState) = 0;
  NS_IMETHOD SetHistoryState(nsILayoutHistoryState* aLayoutHistoryState) = 0;

  /**
   * Determine if reflow is currently locked
   * @param aIsReflowLocked returns PR_TRUE if reflow is locked, PR_FALSE otherwise
   */
  NS_IMETHOD IsReflowLocked(PRBool* aIsLocked) = 0;  

  /**
   * Returns a content iterator to iterate the generated content nodes.
   * You must specify whether you want to iterate the "before" generated
   * content or the "after" generated content. If there is no generated
   * content of the specified type for the promary frame associated with
   * with the content object then NULL is returned
   */
  enum GeneratedContentType {Before, After};
  NS_IMETHOD GetGeneratedContentIterator(nsIContent*          aContent,
                                         GeneratedContentType aType,
                                         nsIContentIterator** aIterator) const = 0;


  /**
   * Store the nsIAnonymousContentCreator-generated anonymous
   * content that's associated with an element.
   * @param aContent the element with which the anonymous
   *   content is to be associated with
   * @param aAnonymousElements an array of nsIContent
   *   objects, or null to indicate that any anonymous
   *   content should be dissociated from the aContent
   */
  NS_IMETHOD SetAnonymousContentFor(nsIContent* aContent, nsISupportsArray* aAnonymousElements) = 0;

  /**
   * Retrieve the nsIAnonymousContentCreator-generated anonymous
   * content that's associated with an element.
   * @param aContent the element for which to retrieve the
   *   associated anonymous content
   * @param aAnonymousElements an array of nsIContent objects,
   *   or null to indicate that there are no anonymous elements
   *   associated with aContent
   */
  NS_IMETHOD GetAnonymousContentFor(nsIContent* aContent, nsISupportsArray** aAnonymousElements) = 0;

  /**
   * Release all nsIAnonymousContentCreator-generated
   * anonymous content associated with the shell.
   */
  NS_IMETHOD ReleaseAnonymousContent() = 0;

  /**
   * Called to find out if painting is suppressed for this presshell.  If it is suppressd,
   * we don't allow the painting of any layer but the background, and we don't
   * recur into our children.
   */
  NS_IMETHOD IsPaintingSuppressed(PRBool* aResult)=0;

  /**
   * Unsuppress painting.
   */
  NS_IMETHOD UnsuppressPainting() = 0;

  enum InterruptType {Timeout};
  /**
   * Notify aFrame via a reflow command when an aInterruptType event occurs 
   */
  NS_IMETHOD SendInterruptNotificationTo(nsIFrame*     aFrame,
                                         InterruptType aInterruptType) = 0;
  /**
   * Cancel Notifications to aFrame when an aInterruptType event occurs 
   */
  NS_IMETHOD CancelInterruptNotificationTo(nsIFrame*     aFrame,
                                           InterruptType aInterruptType) = 0;


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

  /**
   * Get the flags associated with the VerifyReflow debug tool
   */
  static NS_LAYOUT PRInt32 GetVerifyReflowFlags();


#ifdef MOZ_REFLOW_PERF
  NS_IMETHOD DumpReflows() = 0;
  NS_IMETHOD CountReflows(const char * aName, PRUint32 aType, nsIFrame * aFrame) = 0;
  NS_IMETHOD PaintCount(const char * aName, 
                        nsIRenderingContext* aRenderingContext, 
                        nsIPresContext * aPresContext, 
                        nsIFrame * aFrame,
                        PRUint32 aColor) = 0;
  NS_IMETHOD SetPaintFrameCount(PRBool aOn) = 0;
#endif

#ifdef IBMBIDI
  /**
   * SetCaretBidiLevel will set the Bidi embedding level for the cursor. 0-63
   */
  NS_IMETHOD SetCaretBidiLevel(PRUint8 aLevel) = 0;

  /**
   * GetCaretBidiLevel will get the Bidi embedding level for the cursor. 0-63
   */
  NS_IMETHOD GetCaretBidiLevel(PRUint8 *aOutLevel) = 0;

  /**
   * UndefineCaretBidiLevel will set the Bidi embedding level for the cursor to an out-of-range value
   */
  NS_IMETHOD UndefineCaretBidiLevel(void) = 0;

  /**
   * Reconstruct and reflow frame model 
   */
  NS_IMETHOD BidiStyleChangeReflow(void) = 0;
#endif

};

/**
 * Create a new empty presentation shell. Upon success, call Init
 * before attempting to use the shell.
 */
extern NS_LAYOUT nsresult
  NS_NewPresShell(nsIPresShell** aInstancePtrResult);

#endif /* nsIPresShell_h___ */
