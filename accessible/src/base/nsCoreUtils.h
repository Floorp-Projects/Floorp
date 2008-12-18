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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#ifndef nsCoreUtils_h_
#define nsCoreUtils_h_

#include "nsAccessibilityAtoms.h"

#include "nsIDOMNode.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIDocShellTreeItem.h"
#include "nsPoint.h"

class nsCoreUtils
{
public:
  /**
   * Return true if the given node has registered event listener of the given
   * type.
   */
  static PRBool HasListener(nsIContent *aContent, const nsAString& aEventType);

  /**
   * Send mouse events to the given element.
   *
   * @param aEventType  an event type (see nsGUIEvent.h for constants)
   * @param aPresShell  the presshell for the given element
   * @param aContent    the element element
   */
  static PRBool DispatchMouseEvent(PRUint32 aEventType,
                                   nsIPresShell *aPresShell,
                                   nsIContent *aContent);

  /**
   * Return an accesskey registered on the given element by
   * nsIEventStateManager or 0 if there is no registered accesskey.
   *
   * @param aContent - the given element.
   */
  static PRUint32 GetAccessKeyFor(nsIContent *aContent);

  /**
   * Return DOM element related with the given node, i.e.
   * a) itself if it is DOM element
   * b) parent element if it is text node
   * c) body element if it is HTML document node
   * d) document element if it is document node.
   *
   * @param aNode  [in] the given DOM node
   */
  static already_AddRefed<nsIDOMElement> GetDOMElementFor(nsIDOMNode *aNode);

  /**
   * Return DOM node for the given DOM point.
   */
  static already_AddRefed<nsIDOMNode> GetDOMNodeFromDOMPoint(nsIDOMNode *aNode,
                                                             PRUint32 aOffset);
  /**
   * Return the nsIContent* to check for ARIA attributes on -- this may not
   * always be the DOM node for the accessible. Specifically, for doc
   * accessibles, it is not the document node, but either the root element or
   * <body> in HTML. Similar with GetDOMElementFor() method.
   *
   * @param aDOMNode  DOM node for the accessible that may be affected by ARIA
   * @return          the nsIContent which may have ARIA markup
   */
  static nsIContent *GetRoleContent(nsIDOMNode *aDOMNode);

  /**
   * Is the first passed in node an ancestor of the second?
   * Note: A node is not considered to be the ancestor of itself.
   * @param aPossibleAncestorNode -- node to test for ancestor-ness of aPossibleDescendantNode
   * @param aPossibleDescendantNode -- node to test for descendant-ness of aPossibleAncestorNode
   * @return PR_TRUE if aPossibleAncestorNode is an ancestor of aPossibleDescendantNode
   */
   static PRBool IsAncestorOf(nsIDOMNode *aPossibleAncestorNode,
                              nsIDOMNode *aPossibleDescendantNode);

  /**
   * Are the first node and the second siblings?
   * @return PR_TRUE if aDOMNode1 and aDOMNode2 have same parent
   */
   static PRBool AreSiblings(nsIDOMNode *aDOMNode1,
                             nsIDOMNode *aDOMNode2);

  /**
   * Helper method to scroll range into view, used for implementation of
   * nsIAccessibleText::scrollSubstringTo().
   *
   * @param aFrame        the frame for accessible the range belongs to.
   * @param aStartNode    start node of a range
   * @param aStartOffset  an offset inside the start node
   * @param aEndNode      end node of a range
   * @param aEndOffset    an offset inside the end node
   * @param aScrollType   the place a range should be scrolled to
   */
  static nsresult ScrollSubstringTo(nsIFrame *aFrame,
                                    nsIDOMNode *aStartNode, PRInt32 aStartIndex,
                                    nsIDOMNode *aEndNode, PRInt32 aEndIndex,
                                    PRUint32 aScrollType);

  /** Helper method to scroll range into view, used for implementation of
   * nsIAccessibleText::scrollSubstringTo[Point]().
   *
   * @param aFrame        the frame for accessible the range belongs to.
   * @param aStartNode    start node of a range
   * @param aStartOffset  an offset inside the start node
   * @param aEndNode      end node of a range
   * @param aEndOffset    an offset inside the end node
   * @param aVPercent     how to align vertically, specified in percents
   * @param aHPercent     how to align horizontally, specified in percents
   */
  static nsresult ScrollSubstringTo(nsIFrame *aFrame,
                                    nsIDOMNode *aStartNode, PRInt32 aStartIndex,
                                    nsIDOMNode *aEndNode, PRInt32 aEndIndex,
                                    PRInt16 aVPercent, PRInt16 aHPercent);

  /**
   * Scrolls the given frame to the point, used for implememntation of
   * nsIAccessNode::scrollToPoint and nsIAccessibleText::scrollSubstringToPoint.
   *
   * @param aScrollableFrame  the scrollable frame
   * @param aFrame            the frame to scroll
   * @param aPoint            the point scroll to
   */
  static void ScrollFrameToPoint(nsIFrame *aScrollableFrame,
                                 nsIFrame *aFrame, const nsIntPoint& aPoint);

  /**
   * Converts scroll type constant defined in nsIAccessibleScrollType to
   * vertical and horizontal percents.
   */
  static void ConvertScrollTypeToPercents(PRUint32 aScrollType,
                                          PRInt16 *aVPercent,
                                          PRInt16 *aHPercent);

  /**
   * Returns coordinates relative screen for the top level window.
   *
   * @param aNode  the DOM node hosted in the window.
   */
  static nsIntPoint GetScreenCoordsForWindow(nsIDOMNode *aNode);

  /**
   * Return document shell tree item for the given DOM node.
   */
  static already_AddRefed<nsIDocShellTreeItem>
    GetDocShellTreeItemFor(nsIDOMNode *aNode);

  /**
   * Retrun frame for the given DOM element.
   */
  static nsIFrame* GetFrameFor(nsIDOMElement *aElm);

  /**
   * Retrun true if the type of given frame equals to the given frame type.
   *
   * @param aFrame  the frame
   * @param aAtom   the frame type
   */
  static PRBool IsCorrectFrameType(nsIFrame* aFrame, nsIAtom* aAtom);

  /**
   * Return presShell for the document containing the given DOM node.
   */
  static already_AddRefed<nsIPresShell> GetPresShellFor(nsIDOMNode *aNode);

  /**
   * Return document node for the given document shell tree item.
   */
  static already_AddRefed<nsIDOMNode>
    GetDOMNodeForContainer(nsIDocShellTreeItem *aContainer);

  /**
   * Get the ID for an element, in some types of XML this may not be the ID attribute
   * @param aContent  Node to get the ID for
   * @param aID       Where to put ID string
   * @return          PR_TRUE if there is an ID set for this node
   */
  static PRBool GetID(nsIContent *aContent, nsAString& aID);

  /**
   * Check if the given element is XLink.
   *
   * @param aContent  the given element
   * @return          PR_TRUE if the given element is XLink
   */
  static PRBool IsXLink(nsIContent *aContent);

  /**
   * Returns language for the given node.
   *
   * @param aContent     [in] the given node
   * @param aRootContent [in] container of the given node
   * @param aLanguage    [out] language
   */
  static void GetLanguageFor(nsIContent *aContent, nsIContent *aRootContent,
                             nsAString& aLanguage);

  /**
   * Return computed styles declaration for the given node.
   */
  static void GetComputedStyleDeclaration(const nsAString& aPseudoElt,
                                          nsIDOMNode *aNode,
                                          nsIDOMCSSStyleDeclaration **aCssDecl);

  /**
   * Search element in neighborhood of the given element by tag name and
   * attribute value that equals to ID attribute of the given element.
   * ID attribute can be either 'id' attribute or 'anonid' if the element is
   * anonymous.
   * The first matched content will be returned.
   *
   * @param aForNode - the given element the search is performed for
   * @param aRelationAttrs - an array of attributes, element is attribute name of searched element, ignored if aAriaProperty passed in
   * @param aAttrNum - how many attributes in aRelationAttrs
   * @param aTagName - tag name of searched element, or nsnull for any -- ignored if aAriaProperty passed in
   * @param aAncestorLevelsToSearch - points how is the neighborhood of the
   *                                  given element big.
   */
  static nsIContent *FindNeighbourPointingToNode(nsIContent *aForNode,
                                                 nsIAtom **aRelationAttrs, 
                                                 PRUint32 aAttrNum,
                                                 nsIAtom *aTagName = nsnull,
                                                 PRUint32 aAncestorLevelsToSearch = 5);

  /**
   * Overloaded version of FindNeighbourPointingToNode to accept only one
   * relation attribute.
   */
  static nsIContent *FindNeighbourPointingToNode(nsIContent *aForNode,
                                                 nsIAtom *aRelationAttr, 
                                                 nsIAtom *aTagName = nsnull,
                                                 PRUint32 aAncestorLevelsToSearch = 5);

  /**
   * Search for element that satisfies the requirements in subtree of the given
   * element. The requirements are tag name, attribute name and value of
   * attribute.
   * The first matched content will be returned.
   *
   * @param aId - value of searched attribute
   * @param aLookContent - element that search is performed inside
   * @param aRelationAttrs - an array of searched attributes
   * @param aAttrNum - how many attributes in aRelationAttrs
   * @param                 if both aAriaProperty and aRelationAttrs are null, then any element with aTagType will do
   * @param aExcludeContent - element that is skiped for search
   * @param aTagType - tag name of searched element, by default it is 'label' --
   *                   ignored if aAriaProperty passed in
   */
  static nsIContent *FindDescendantPointingToID(const nsString *aId,
                                                nsIContent *aLookContent,
                                                nsIAtom **aRelationAttrs,
                                                PRUint32 aAttrNum = 1,
                                                nsIContent *aExcludeContent = nsnull,
                                                nsIAtom *aTagType = nsAccessibilityAtoms::label);

  /**
   * Overloaded version of FindDescendantPointingToID to accept only one
   * relation attribute.
   */
  static nsIContent *FindDescendantPointingToID(const nsString *aId,
                                                nsIContent *aLookContent,
                                                nsIAtom *aRelationAttr,
                                                nsIContent *aExcludeContent = nsnull,
                                                nsIAtom *aTagType = nsAccessibilityAtoms::label);

  // Helper for FindDescendantPointingToID(), same args
  static nsIContent *FindDescendantPointingToIDImpl(nsCString& aIdWithSpaces,
                                                    nsIContent *aLookContent,
                                                    nsIAtom **aRelationAttrs,
                                                    PRUint32 aAttrNum = 1,
                                                    nsIContent *aExcludeContent = nsnull,
                                                    nsIAtom *aTagType = nsAccessibilityAtoms::label);

  /**
   * Return the label element for the given DOM element.
   */
  static nsIContent *GetLabelContent(nsIContent *aForNode);

  /**
   * Return the HTML label element for the given HTML element.
   */
  static nsIContent *GetHTMLLabelContent(nsIContent *aForNode);

  
};

#endif

