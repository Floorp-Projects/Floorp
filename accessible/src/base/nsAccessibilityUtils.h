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

#ifndef nsAccessibilityUtils_h_
#define nsAccessibilityUtils_h_

#include "nsAccessibilityAtoms.h"
#include "nsIAccessible.h"
#include "nsARIAMap.h"

#include "nsIDOMNode.h"
#include "nsIPersistentProperties2.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIDocShellTreeItem.h"
#include "nsPoint.h"
#include "nsIAccessibleDocument.h"

class nsAccUtils
{
public:
  /**
   * Returns value of attribute from the given attributes container.
   *
   * @param aAttributes - attributes container
   * @param aAttrName - the name of requested attribute
   * @param aAttrValue - value of attribute
   */
  static void GetAccAttr(nsIPersistentProperties *aAttributes,
                         nsIAtom *aAttrName,
                         nsAString& aAttrValue);

  /**
   * Set value of attribute for the given attributes container.
   *
   * @param aAttributes - attributes container
   * @param aAttrName - the name of requested attribute
   * @param aAttrValue - new value of attribute
   */
  static void SetAccAttr(nsIPersistentProperties *aAttributes,
                         nsIAtom *aAttrName,
                         const nsAString& aAttrValue);

  /**
   * Return values of group attributes ('level', 'setsize', 'posinset')
   */
  static void GetAccGroupAttrs(nsIPersistentProperties *aAttributes,
                               PRInt32 *aLevel,
                               PRInt32 *aPosInSet,
                               PRInt32 *aSetSize);

  /**
   * Returns true if there are level, posinset and sizeset attributes.
   */
  static PRBool HasAccGroupAttrs(nsIPersistentProperties *aAttributes);

  /**
   * Set group attributes ('level', 'setsize', 'posinset').
   */
  static void SetAccGroupAttrs(nsIPersistentProperties *aAttributes,
                               PRInt32 aLevel,
                               PRInt32 aPosInSet,
                               PRInt32 aSetSize);

  /**
   * Set group attributes - 'level', 'setsize', 'posinset'.
   *
   * @param aNode - XUL element that implements
   *                nsIDOMXULSelectControlItemElement interface
   * @param aAttributes - attributes container
   */
  static void SetAccAttrsForXULSelectControlItem(nsIDOMNode *aNode,
                                                 nsIPersistentProperties *aAttributes);

  /**
   * Return true if the given node has registered event listener of the given
   * type.
   */
  static PRBool HasListener(nsIContent *aContent, const nsAString& aEventType);

  /**
   * Return an accesskey registered on the given element by
   * nsIEventStateManager or 0 if there is no registered accesskey.
   *
   * @param aContent - the given element.
   */
  static PRUint32 GetAccessKeyFor(nsIContent *aContent);

  /**
   * Fire accessible event of the given type for the given accessible.
   */
  static nsresult FireAccEvent(PRUint32 aEventType, nsIAccessible *aAccessible,
                               PRBool aIsAsynch = PR_FALSE);

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
    * If an ancestor in this document exists with the given role, return it
    * @param aDescendant Descendant to start search with
    * @param aRole Role to find matching ancestor for
    * @return The ancestor accessible with the given role, or nsnull if no match is found
    */
   static already_AddRefed<nsIAccessible>
     GetAncestorWithRole(nsIAccessible *aDescendant, PRUint32 aRole);

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
   * @param - aNode - the DOM node hosted in the window.
   */
  static nsIntPoint GetScreenCoordsForWindow(nsIDOMNode *aNode);

  /**
   * Return document shell tree item for the given DOM node.
   */
  static already_AddRefed<nsIDocShellTreeItem>
    GetDocShellTreeItemFor(nsIDOMNode *aNode);

  /**
   * Get the ID for an element, in some types of XML this may not be the ID attribute
   * @param aContent  Node to get the ID for
   * @param aID       Where to put ID string
   * @return          PR_TRUE if there is an ID set for this node
   */
  static PRBool GetID(nsIContent *aContent, nsAString& aID);

  /**
   * Find out what kinds of properties are checked for this content node's document
   * @param aContent     The content node we're going to look for ARIA properties on
   * @param aWeakShell   The presshell for the document we're looking for ARIA properties on (optional optimization)
   * @return             The types of properties checked
   */
  static PRUint32 GetAriaPropTypes(nsIContent *aContent, nsIWeakReference *aWeakShell = nsnull);

  /**
   *  Check for the relevant ARIA property. Can check either for a properly namespaced property,
   *  or a fake hyphenated namespace using "aria-" as a prefix in HTML. Is optimized to only
   *  check for each type when it is possible to exist on a given node.
   *  @param aContent     Node to check for property on
   *  @param aWeakShell   The current pres shell if known (as an optimization), or nsnull if not known by caller
   *  @param aProperty    An enumeration indicating which ARIA property we are checking
   *  @param aAriaPropTypes  A bitflag for the property types to check for (namespaced, hyphenated or both), if known by caller
   *  @return             PR_TRUE if the property is defined
   */
  static PRBool HasAriaProperty(nsIContent *aContent, nsIWeakReference *aWeakShell,
                                EAriaProperty aProperty,
                                PRUint32 aCheckFlags = 0);

  /**
   *  Get the relevant ARIA property. Can check either for a properly namespaced property,
   *  or a fake hyphenated namespace using "aria-" as a prefix in HTML. Is optimized to only
   *  check for each type when it is possible to exist on a given node.
   *  @param aContent     Node to check for property on
   *  @param aWeakShell   The current pres shell if known (as an optimization), or nsnull if not known by caller
   *  @param aProperty    An enumeration indicating which ARIA property we are checking
   *  @param aValue       Where to store the property value
   *  @param aAriaPropTypes  A bitflag for the property types to check for (namespaced, hyphenated or both), if known by caller
   *  @return             PR_TRUE if the property is defined
   */
  static PRBool GetAriaProperty(nsIContent *aContent, nsIWeakReference *aWeakShell,
                                EAriaProperty aProperty, nsAString& aValue, 
                                PRUint32 aCheckFlags = 0);

  /**
   * Search element in neighborhood of the given element by tag name and
   * attribute value that equals to ID attribute of the given element.
   * ID attribute can be either 'id' attribute or 'anonid' if the element is
   * anonymous.
   *
   * @param aAriaProperty - the ARIA property to search for or eAria_none, if aRelationAttr is passed in
   * @param aForNode - the given element the search is performed for
   * @param aTagName - tag name of searched element, or nsnull for any -- ignored if aAriaProperty passed in
   * @param aRelationAttr - attribute name of searched element, ignored if aAriaProperty passed in
   * @param aAncestorLevelsToSearch - points how is the neighborhood of the
   *                                  given element big.
   */
  static nsIContent *FindNeighbourPointingToNode(nsIContent *aForNode,
                                                 EAriaProperty aAriaProperty,
                                                 nsIAtom *aTagName = nsnull,
                                                 nsIAtom *aRelationAttr = nsnull,
                                                 PRUint32 aAncestorLevelsToSearch = 5);

  /**
   * Search for element that satisfies the requirements in subtree of the given
   * element. The requirements are tag name, attribute name and value of
   * attribute.
   *
   * @param aId - value of searched attribute
   * @param aLookContent - element that search is performed inside
   * @param aAriaProperty - the ARIA property to search for or eAria_none, if aRelationAttr is passed in
   * @param aRelationAttr - searched attribute-- ignored if aAriaProperty passed in
   * @param                 if both aAriaProperty and aRelationAttr are null, then any element with aTagType will do
   * @param aExcludeContent - element that is skiped for search
   * @param aTagType - tag name of searched element, by default it is 'label' --
   *                   ignored if aAriaProperty passed in
   */
  static nsIContent *FindDescendantPointingToID(const nsString *aId,
                                                nsIContent *aLookContent,
                                                EAriaProperty aAriaProperty,
                                                nsIAtom *aRelationAttr = nsnull,
                                                nsIContent *aExcludeContent = nsnull,
                                                nsIAtom *aTagType = nsAccessibilityAtoms::label);

  // Helper for FindDescendantPointingToID(), same args
  static nsIContent *FindDescendantPointingToIDImpl(nsCString& aIdWithSpaces,
                                                    nsIContent *aLookContent,
                                                    EAriaProperty aAriaProperty,
                                                    PRUint32 aAriaPropTypes,
                                                    nsIAtom *aRelationAttr = nsnull,
                                                    nsIContent *aExcludeContent = nsnull,
                                                    nsIAtom *aTagType = nsAccessibilityAtoms::label);
};

#endif

