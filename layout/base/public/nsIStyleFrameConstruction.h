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
#ifndef nsIStyleFrameConstruction_h___
#define nsIStyleFrameConstruction_h___

#include "nsISupports.h"
#include "nsIStyleSet.h"

class nsIPresShell;
class nsIPresContext;
class nsIContent;
class nsIFrame;
class nsIAtom;
class nsIStyleSheet;
class nsIStyleRule;
class nsStyleChangeList;
class nsIFrameManager;
class nsILayoutHistoryState;

// IID for the nsIStyleSet interface {a6cf9066-15b3-11d2-932e-00805f8add32}
#define NS_ISTYLE_FRAME_CONSTRUCTION_IID \
{0xa6cf9066, 0x15b3, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

/** Interface for objects that handle frame construction based on style.
  * All frame construction goes through an object that implements this interface.
  * Frames are built based on the state of the content model and the style model.
  * Changes to either content or style can cause changes to the frame model.
  */
class nsIStyleFrameConstruction : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISTYLE_FRAME_CONSTRUCTION_IID)

  /**
    * Create frames for the root content element and its child content.
    *
    * @param aPresShell    the presentation shell for which we will consturct a root frame
    * @param aPresContext  the presentation context
    * @param aDocElement   the content node to map into a root frame
    * @param aFrameSubTree [OUT] the resulting frame tree (a root frame and its children)
    *
    * @return  NS_OK
    */
  NS_IMETHOD ConstructRootFrame(nsIPresShell* aPresShell, 
                                nsIPresContext* aPresContext,
                                nsIContent*     aDocElement,
                                nsIFrame*&      aFrameSubTree) = 0;

  /**
    * Causes reconstruction of a frame hierarchy rooted by the
    * document element frame. This is often called when radical style
    * change precludes incremental reflow.
    *
    * @param aPresContext  the presentation context
    *
    * @return  NS_OK
    */
  NS_IMETHOD  ReconstructDocElementHierarchy(nsIPresContext* aPresContext) = 0;


  /////////////// Content change notifications ////////////////// 

  /**
    * Notification that content was appended in the content tree.
    * This may have the side effect of creating frames for the new content.
    *
    * @param aPresContext         the presentation context
    * @param aContainer           the content node into which content was appended
    * @param aNewIndexInContainer the index in aContainer at which the first
    *                             content node was appended.
    * @return  NS_OK
    * @see     nsIDocumentObserver
    */
  NS_IMETHOD ContentAppended(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             PRInt32         aNewIndexInContainer) = 0;

  /**
    * Notification that content was inserted in the content tree.
    * This may have the side effect of creating frames for the new content.
    *
    * @param aPresContext         the presentation context
    * @param aContainer           the content node into which content was appended
    * @param aChild               the content node that was inserted
    * @param aNewIndexInContainer the index of aChild in aContainer
    * @param aFrameState          the layout history object used to initialize the new frame(s)
    *
    * @return  NS_OK
    * @see     nsIDocumentObserver
    */
  NS_IMETHOD ContentInserted(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aChild,
                             PRInt32         aIndexInContainer,
                             nsILayoutHistoryState* aFrameState) = 0;

  /**
    * Notification that content was replaced in the content tree.
    * This may have the side effect of creating frames for the new content.
    *
    * @param aPresContext         the presentation context
    * @param aContainer           the content node into which content was appended
    * @param aOldChild            the content node that was replaced
    * @param aNewChild            the new content node that replaced aOldChild
    * @param aNewIndexInContainer the index of aNewChild in aContainer
    *
    * @return  NS_OK
    * @see     nsIDocumentObserver
    */
  NS_IMETHOD ContentReplaced(nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aOldChild,
                             nsIContent*     aNewChild,
                             PRInt32         aIndexInContainer) = 0;

  /**
    * Notification that content was inserted in the content tree.
    * This may have the side effect of changing the frame tree
    *
    * @param aPresContext         the presentation context
    * @param aContainer           the content node into which content was appended
    * @param aChild               the content node that was inserted
    * @param aNewIndexInContainer the index of aChild in aContainer
    *
    * @return  NS_OK
    * @see     nsIDocumentObserver
    */
  NS_IMETHOD ContentRemoved(nsIPresContext* aPresContext,
                            nsIContent*     aContainer,
                            nsIContent*     aChild,
                            PRInt32         aIndexInContainer) = 0;

  /**
    * Notification that content was changed in the content tree.
    * This may have the side effect of changing the frame tree
    *
    * @param aPresContext the presentation context
    * @param aContent     the content node that was changed
    * @param aSubContent  a hint to the frame system about the change
    *
    * @return  NS_OK
    * @see     nsIDocumentObserver
    */
  NS_IMETHOD ContentChanged(nsIPresContext* aPresContext,
                            nsIContent* aContent,
                            nsISupports* aSubContent) = 0;


  /**
    * Notification that the state of a content node has changed. 
    * (ie: gained or lost focus, became active or hovered over)
    *
    * @param aPresContext the presentation context
    * @param aContent1    the content node whose state was changed
    * @param aContent2    an optional second content node whose state 
    *                     has also changed.  The meaning of aContent2
    *                     depends on the type of state change.
    *
    * @return  NS_OK
    * @see     nsIDocumentObserver
    */
  NS_IMETHOD ContentStatesChanged(nsIPresContext* aPresContext, 
                                  nsIContent* aContent1,
                                  nsIContent* aContent2) = 0;

  /**
    * Notification that an attribute was changed for a content node
    * This may have the side effect of changing the frame tree
    *
    * @param aPresContext the presentation context
    * @param aContent     the content node on which an attribute was changed
    * @param aNameSpaceID the name space for the changed attribute
    * @param aAttribute   the attribute that was changed
    * @param aHint        a hint about the effect of the change
    *                     see nsStyleConsts.h for legal values
    *                     any of the consts of the form NS_STYLE_HINT_*
    *
    * @return  NS_OK
    * @see     nsIDocumentObserver
    */
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aContent,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType, 
                              PRInt32 aHint) = 0;


  /////////////// Style change notifications ////////////////// 

  /**
   * A StyleRule has just been modified within a style sheet.
   *
   * @param aDocument The document being observed
   * @param aStyleSheet the StyleSheet that contians the rule
   * @param aStyleRule  the rule that was modified
   * @param aHint       some possible info about the nature of the change.
   *                    see nsStyleConsts for hint values
   *
   * @return  NS_OK
   * @see     nsIDocumentObserver
   */
  NS_IMETHOD StyleRuleChanged(nsIPresContext* aPresContext,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint) = 0; // See nsStyleConsts fot hint values

  /**
   * A StyleRule has just been added to a style sheet.
   * This method is called automatically when the rule gets
   * added to the sheet. The style sheet passes this
   * notification to the document. The notification is passed on 
   * to all of the document observers.
   *
   * @param aDocument The document being observed
   * @param aStyleSheet the StyleSheet that has been modified
   * @param aStyleRule the rule that was added
   *
   * @return  NS_OK
   * @see     nsIDocumentObserver
   */
  NS_IMETHOD StyleRuleAdded(nsIPresContext* aPresContext,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) = 0;

  /**
   * A StyleRule has just been removed from a style sheet.
   * This method is called automatically when the rule gets
   * removed from the sheet. The style sheet passes this
   * notification to the document. The notification is passed on 
   * to all of the document observers.
   *
   * @param aDocument The document being observed
   * @param aStyleSheet the StyleSheet that has been modified
   * @param aStyleRule the rule that was removed
   *
   * @return  NS_OK
   * @see     nsIDocumentObserver
   */
  NS_IMETHOD StyleRuleRemoved(nsIPresContext* aPresContext,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) = 0;

  /**
   * Method that actually handles style changes for effected frames.
   * Note:  this may not need to be a public method.  Use with extreme caution.
   *
   * @param aRestyleArray   A list of effected frames.
   * @param aPresContext    The presentation context.
   *
   * @return  NS_OK
   */
  NS_IMETHOD ProcessRestyledFrames(nsStyleChangeList& aRestyleArray, 
                                   nsIPresContext* aPresContext) = 0;
  
  /////////////// misc methods //////////////////////

  /**
    * Notification that we were unable to render a replaced element.
    * Called when the replaced element can not be rendered, and we should
    * instead render the element's contents.
    * For HTML, the content object associated with aFrame should either be a IMG
    * element or an OBJECT element.
    * This may have the side effect of changing the frame tree.
    *
    * @param aPresShell   the presentation shell
    * @param aPresContext the presentation context
    * @param aFrame       the frame constructed for the replaced element
    *
    * @return  NS_OK
    */
  NS_IMETHOD CantRenderReplacedElement(nsIPresShell*   aPresShell, 
                                       nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame) = 0;

  /**
    * Request to create a continuing frame
    *
    * @param aPresShell   the presentation shell
    * @param aPresContext the presentation context
    * @param aFrame       the frame for which we need a continuation
    * @param aParentFrame the parent of aFrame
    * @param aContinuingFrame [OUT] the resulting frame
    *
    * @return  NS_OK
    */
  NS_IMETHOD CreateContinuingFrame(nsIPresShell*   aPresShell, 
                                   nsIPresContext* aPresContext,
                                   nsIFrame*       aFrame,
                                   nsIFrame*       aParentFrame,
                                   nsIFrame**      aContinuingFrame) = 0;

  /** Request to find the primary frame associated with a given content object.
    * This is typically called by the pres shell when there is no mapping in
    * the pres shell hash table
    *
    * @param aPresContext  the presentation context
    * @param aFrameManager the frame manager for the frame being sought
    * @param aContent      the content node for which we seek a frame
    * @param aFrame        [OUT] the resulting frame, if any.  may be null,
    *                            indicating that no frame maps aContent
    * @param aHint         an optional hint used to make the search for aFrame faster
    */
  NS_IMETHOD FindPrimaryFrameFor(nsIPresContext*  aPresContext,
                                 nsIFrameManager* aFrameManager,
                                 nsIContent*      aContent,
                                 nsIFrame**       aFrame,
                                 nsFindFrameHint* aHint=nsnull) = 0;


  /**
   * Return the point in the frame hierarchy where the frame that
   * will be constructed for |aChildContent| ought be inserted.
   *
   * @param aPresShell      the presentation shell
   * @param aParentFrame    the frame that will parent the frame that is
   *                        created for aChildContent
   * @param aChildContent   the child content for which a frame is to be
   *                        created
   * @param aInsertionPoint [OUT] the frame that should parent the frame
   *                              for |aChildContent|.
   */
  NS_IMETHOD GetInsertionPoint(nsIPresShell* aPresShell,
                               nsIFrame*     aParentFrame,
                               nsIContent*   aChildContent,
                               nsIFrame**    aInsertionPoint,
                               PRBool*       aMultiple = nsnull) = 0;
};

#endif /* nsIStyleFrameConstruction_h___ */
