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

#ifndef nsAccUtils_h_
#define nsAccUtils_h_

#include "nsIAccessible.h"
#include "nsIAccessNode.h"
#include "nsIAccessibleDocument.h"
#include "nsIAccessibleRole.h"
#include "nsIAccessibleText.h"
#include "nsARIAMap.h"

#include "nsIDOMNode.h"
#include "nsIPersistentProperties2.h"
#include "nsIContent.h"
#include "nsPoint.h"

class nsAccessNode;

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
   * Set group attributes - 'level', 'setsize', 'posinset'.
   *
   * @param  aNode        XUL element that implements
   *                      nsIDOMXULContainerItemElement interface
   * @param  aAttributes  attributes container
   */
  static void SetAccAttrsForXULContainerItem(nsIDOMNode *aNode,
                                             nsIPersistentProperties *aAttributes);

  /**
   * Set container-foo live region attributes for the given node.
   *
   * @param aAttributes    where to store the attributes
   * @param aStartContent  node to start from
   * @param aTopContent    node to end at
   */
  static void SetLiveContainerAttributes(nsIPersistentProperties *aAttributes,
                                         nsIContent *aStartContent,
                                         nsIContent *aTopContent);

  /**
   * Return PR_TRUE if the ARIA property should always be exposed as an object
   * attribute.
   */
  static PRBool IsARIAPropForObjectAttr(nsIAtom *aAtom);

  /**
   * Fire accessible event of the given type for the given accessible.
   */
  static nsresult FireAccEvent(PRUint32 aEventType, nsIAccessible *aAccessible,
                               PRBool aIsAsynch = PR_FALSE);

  /**
    * If an ancestor in this document exists with the given role, return it
    * @param aDescendant Descendant to start search with
    * @param aRole Role to find matching ancestor for
    * @return The ancestor accessible with the given role, or nsnull if no match is found
    */
   static already_AddRefed<nsIAccessible>
     GetAncestorWithRole(nsIAccessible *aDescendant, PRUint32 aRole);

   /**
     * For an ARIA tree item , get the accessible that represents its conceptual parent.
     * This method will use the correct method for the given way the tree is constructed.
     * The conceptual parent is what the user sees as the parent, not the DOM or accessible parent.
     * @param aStartTreeItem  The tree item to get the parent for
     * @param aStartTreeItemContent  The content node for the tree item
     * @param The tree item's parent, or null if none
     */
   static void
     GetARIATreeItemParent(nsIAccessible *aStartTreeItem,
                           nsIContent *aStartTreeItemContent,
                           nsIAccessible **aTreeItemParent);

  /**
   * Return text accessible containing focus point of the given selection.
   * Used for normal and misspelling selection changes processing.
   *
   * @param aSelection  [in] the given selection
   * @param aNode       [out, optional] the DOM node of text accessible
   * @return            text accessible
   */
  static already_AddRefed<nsIAccessibleText>
    GetTextAccessibleFromSelection(nsISelection *aSelection,
                                   nsIDOMNode **aNode = nsnull);

  /**
   * Converts the given coordinates to coordinates relative screen.
   *
   * @param aX               [in] the given x coord
   * @param aY               [in] the given y coord
   * @param aCoordinateType  [in] specifies coordinates origin (refer to
   *                         nsIAccessibleCoordinateType)
   * @param aAccessNode      [in] the accessible if coordinates are given
   *                         relative it.
   * @param aCoords          [out] converted coordinates
   */
  static nsresult ConvertToScreenCoords(PRInt32 aX, PRInt32 aY,
                                        PRUint32 aCoordinateType,
                                        nsIAccessNode *aAccessNode,
                                        nsIntPoint *aCoords);

  /**
   * Converts the given coordinates relative screen to another coordinate
   * system.
   *
   * @param aX               [in, out] the given x coord
   * @param aY               [in, out] the given y coord
   * @param aCoordinateType  [in] specifies coordinates origin (refer to
   *                         nsIAccessibleCoordinateType)
   * @param aAccessNode      [in] the accessible if coordinates are given
   *                         relative it
   */
  static nsresult ConvertScreenCoordsTo(PRInt32 *aX, PRInt32 *aY,
                                        PRUint32 aCoordinateType,
                                        nsIAccessNode *aAccessNode);

  /**
   * Returns coordinates relative screen for the top level window.
   *
   * @param aAccessNode  the accessible hosted in the window
   */
  static nsIntPoint GetScreenCoordsForWindow(nsIAccessNode *aAccessNode);

  /**
   * Returns coordinates relative screen for the parent of the given accessible.
   *
   * @param aAccessNode  the accessible
   */
  static nsIntPoint GetScreenCoordsForParent(nsIAccessNode *aAccessNode);

  /**
   * Get the role map entry for a given DOM node. This will use the first
   * ARIA role if the role attribute provides a space delimited list of roles.
   * @param aNode  The DOM node to get the role map entry for
   * @return       A pointer to the role map entry for the ARIA role, or nsnull if none
   */
  static nsRoleMapEntry* GetRoleMapEntry(nsIDOMNode *aNode);

  /**
   * Return the role of the given accessible.
   */
  static PRUint32 Role(nsIAccessible *aAcc)
  {
    PRUint32 role = nsIAccessibleRole::ROLE_NOTHING;
    if (aAcc)
      aAcc->GetFinalRole(&role);

    return role;
  }

  /**
   * Return the state for the given accessible.
   */
  static PRUint32 State(nsIAccessible *aAcc)
  {
    PRUint32 state = 0;
    if (aAcc)
      aAcc->GetState(&state, nsnull);

    return state;
  }

  /**
   * Query nsAccessNode from the given nsIAccessible.
   */
  static already_AddRefed<nsAccessNode>
    QueryAccessNode(nsIAccessible *aAccessible)
  {
    nsAccessNode* accessNode = nsnull;
    if (aAccessible)
      CallQueryInterface(aAccessible, &accessNode);

    return accessNode;
  }

  /**
   * Query nsAccessNode from the given nsIAccessNode.
   */
  static already_AddRefed<nsAccessNode>
    QueryAccessNode(nsIAccessNode *aAccessNode)
  {
    nsAccessNode* accessNode = nsnull;
    if (aAccessNode)
      CallQueryInterface(aAccessNode, &accessNode);
    
    return accessNode;
  }

  /**
   * Query nsAccessNode from the given nsIAccessNode.
   */
  static already_AddRefed<nsAccessNode>
    QueryAccessNode(nsIAccessibleDocument *aAccessibleDocument)
  {
    nsAccessNode* accessNode = nsnull;
    if (aAccessibleDocument)
      CallQueryInterface(aAccessibleDocument, &accessNode);
    
    return accessNode;
  }

#ifdef DEBUG_A11Y
  /**
   * Detect whether the given accessible object implements nsIAccessibleText,
   * when it is text or has text child node.
   */
  static PRBool IsTextInterfaceSupportCorrect(nsIAccessible *aAccessible);
#endif

  /**
   * Return true if the given accessible has text role.
   */
  static PRBool IsText(nsIAccessible *aAcc)
  {
    PRUint32 role = Role(aAcc);
    return role == nsIAccessibleRole::ROLE_TEXT_LEAF ||
           role == nsIAccessibleRole::ROLE_STATICTEXT;
  }

  /**
   * Return text length of the given accessible, return -1 on failure.
   */
  static PRInt32 TextLength(nsIAccessible *aAccessible);

  /**
   * Return true if the given accessible is embedded object.
   */
  static PRBool IsEmbeddedObject(nsIAccessible *aAcc)
  {
    PRUint32 role = Role(aAcc);
    return role != nsIAccessibleRole::ROLE_TEXT_LEAF &&
           role != nsIAccessibleRole::ROLE_WHITESPACE &&
           role != nsIAccessibleRole::ROLE_STATICTEXT;
  }

  /**
   * Return true if the given accessible hasn't children.
   */
  static PRBool IsLeaf(nsIAccessible *aAcc)
  {
    PRInt32 numChildren;
    aAcc->GetChildCount(&numChildren);
    return numChildren > 0;
  }

  /**
   * Return true if the given accessible can't have children. Used when exposing
   * to platform accessibility APIs, should the children be pruned off?
   */
  static PRBool MustPrune(nsIAccessible *aAccessible);

  /**
   * Return true if the given node can be accessible and attached to
   * the document's accessible tree.
   */
  static PRBool IsNodeRelevant(nsIDOMNode *aNode);

  /**
   * Return multiselectable parent for the given selectable accessible if any.
   */
  static already_AddRefed<nsIAccessible> GetMultiSelectFor(nsIDOMNode *aNode);
};

#endif
