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
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *         Jonas Sicking <jonas@sicking.cc> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsNodeUtils_h___
#define nsNodeUtils_h___

#include "nsDOMAttributeMap.h"
#include "nsIDOMNode.h"
#include "nsIMutationObserver.h"

struct JSContext;
struct JSObject;
class nsINode;
class nsNodeInfoManager;
template<class E> class nsCOMArray;

class nsNodeUtils
{
public:
  /**
   * Send CharacterDataChanged notifications to nsIMutationObservers.
   * @param aContent  Node whose data changed
   * @param aInfo     Struct with information details about the change
   * @see nsIMutationObserver::CharacterDataChanged
   */
  static void CharacterDataChanged(nsIContent* aContent,
                                   CharacterDataChangeInfo* aInfo);

  /**
   * Send AttributeChanged notifications to nsIMutationObservers.
   * @param aContent      Node whose data changed
   * @param aNameSpaceID  Namespace of changed attribute
   * @param aAttribute    Local-name of changed attribute
   * @param aModType      Type of change (add/change/removal)
   * @see nsIMutationObserver::AttributeChanged
   */
  static void AttributeChanged(nsIContent* aContent,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aModType);

  /**
   * Send ContentAppended notifications to nsIMutationObservers
   * @param aContainer           Node into which new child/children were added
   * @param aNewIndexInContainer Index of first new child
   * @see nsIMutationObserver::ContentAppended
   */
  static void ContentAppended(nsIContent* aContainer,
                              PRInt32 aNewIndexInContainer);

  /**
   * Send ContentInserted notifications to nsIMutationObservers
   * @param aContainer        Node into which new child was inserted
   * @param aChild            Newly inserted child
   * @param aIndexInContainer Index of new child
   * @see nsIMutationObserver::ContentInserted
   */
  static void ContentInserted(nsINode* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer);
  /**
   * Send ContentRemoved notifications to nsIMutationObservers
   * @param aContainer        Node from which child was removed
   * @param aChild            Removed child
   * @param aIndexInContainer Index of removed child
   * @see nsIMutationObserver::ContentRemoved
   */
  static void ContentRemoved(nsINode* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer);
  /**
   * Send ParentChainChanged notifications to nsIMutationObservers
   * @param aContent  The piece of content that had its parent changed.
   * @see nsIMutationObserver::ParentChainChanged
   */
  static void ParentChainChanged(nsIContent *aContent);

  /**
   * To be called when reference count of aNode drops to zero.
   * @param aNode The node which is going to be deleted.
   * @param aDelete If PR_TRUE, calling this method also deletes aNode.
   */
  static void LastRelease(nsINode* aNode, PRBool aDelete);

  /**
   * Clones aNode, its attributes and, if aDeep is PR_TRUE, its descendant nodes
   * If aNewNodeInfoManager is not null, it is used to create new nodeinfos for
   * the clones. aNodesWithProperties will be filled with all the nodes that
   * have properties, and every node in it will be followed by its clone.
   *
   * @param aNode Node to clone.
   * @param aDeep If PR_TRUE the function will be called recursively on
   *              descendants of the node
   * @param aNewNodeInfoManager The nodeinfo manager to use to create new
   *                            nodeinfos for aNode and its attributes and
   *                            descendants. May be null if the nodeinfos
   *                            shouldn't be changed.
   * @param aNodesWithProperties All nodes (from amongst aNode and its
   *                             descendants) with properties. Every node will
   *                             be followed by its clone.
   * @param aResult *aResult will contain the cloned node.
   */
  static nsresult Clone(nsINode *aNode, PRBool aDeep,
                        nsNodeInfoManager *aNewNodeInfoManager,
                        nsCOMArray<nsINode> &aNodesWithProperties,
                        nsIDOMNode **aResult)
  {
    return CloneAndAdopt(aNode, PR_TRUE, aDeep, aNewNodeInfoManager, nsnull,
                         nsnull, nsnull, aNodesWithProperties, nsnull,
                         aResult);
  }

  /**
   * Walks aNode, its attributes and descendant nodes. If aNewNodeInfoManager is
   * not null, it is used to create new nodeinfos for the nodes. Also reparents
   * the XPConnect wrappers for the nodes in aNewScope if aCx is not null.
   * aNodesWithProperties will be filled with all the nodes that have
   * properties.
   *
   * @param aNode Node to adopt.
   * @param aNewNodeInfoManager The nodeinfo manager to use to create new
   *                            nodeinfos for aNode and its attributes and
   *                            descendants. May be null if the nodeinfos
   *                            shouldn't be changed.
   * @param aCx Context to use for reparenting the wrappers, or null if no
   *            reparenting should be done. Must be null if aNewNodeInfoManager
   *            is null.
   * @param aOldScope Old scope for the wrappers. May be null if aCx is null.
   * @param aNewScope New scope for the wrappers. May be null if aCx is null.
   * @param aNodesWithProperties All nodes (from amongst aNode and its
   *                             descendants) with properties.
   */
  static nsresult Adopt(nsINode *aNode, nsNodeInfoManager *aNewNodeInfoManager,
                        JSContext *aCx, JSObject *aOldScope,
                        JSObject *aNewScope,
                        nsCOMArray<nsINode> &aNodesWithProperties)
  {
    nsCOMPtr<nsIDOMNode> dummy;
    return CloneAndAdopt(aNode, PR_FALSE, PR_TRUE, aNewNodeInfoManager, aCx,
                         aOldScope, aNewScope, aNodesWithProperties,
                         nsnull, getter_AddRefs(dummy));
  }

  /**
   * Call registered userdata handlers for operation aOperation for the nodes in
   * aNodesWithProperties. If aCloned is PR_TRUE aNodesWithProperties should
   * contain both the original and the cloned nodes (and only the userdata
   * handlers registered for the original nodes will be called).
   *
   * @param aNodesWithProperties Contains the nodes that might have properties
   *                             registered on them. If aCloned is PR_TRUE every
   *                             one of those nodes should be immediately
   *                             followed in the array by the cloned node.
   * @param aOwnerDocument The ownerDocument of the original nodes.
   * @param aOperation The operation to call a userdata handler for.
   * @param aCloned If PR_TRUE aNodesWithProperties will contain both original
   *                and cloned nodes.
   */
  static nsresult CallUserDataHandlers(nsCOMArray<nsINode> &aNodesWithProperties,
                                       nsIDocument *aOwnerDocument,
                                       PRUint16 aOperation, PRBool aCloned);

  /**
   * A basic implementation of the DOM cloneNode method. Calls nsINode::Clone to
   * do the actual cloning of the node.
   *
   * @param aNode the node to clone
   * @param aDeep if true all descendants will be cloned too
   * @param aResult the clone
   */
  static nsresult CloneNodeImpl(nsINode *aNode, PRBool aDeep,
                                nsIDOMNode **aResult);

private:
  friend PLDHashOperator PR_CALLBACK
    AdoptFunc(nsAttrHashKey::KeyType aKey, nsIDOMNode *aData, void* aUserArg);

  /**
   * Walks aNode, its attributes and, if aDeep is PR_TRUE, its descendant nodes.
   * If aClone is PR_TRUE the nodes will be cloned. If aNewNodeInfoManager is
   * not null, it is used to create new nodeinfos for the nodes. Also reparents
   * the XPConnect wrappers for the nodes in aNewScope if aCx is not null.
   * aNodesWithProperties will be filled with all the nodes that have
   * properties.
   *
   * @param aNode Node to adopt/clone.
   * @param aClone If PR_TRUE the node will be cloned and the cloned node will
   *               be in aResult.
   * @param aDeep If PR_TRUE the function will be called recursively on
   *              descendants of the node
   * @param aNewNodeInfoManager The nodeinfo manager to use to create new
   *                            nodeinfos for aNode and its attributes and
   *                            descendants. May be null if the nodeinfos
   *                            shouldn't be changed.
   * @param aCx Context to use for reparenting the wrappers, or null if no
   *            reparenting should be done. Must be null if aClone is PR_TRUE or
   *            if aNewNodeInfoManager is null.
   * @param aOldScope Old scope for the wrappers. May be null if aCx is null.
   * @param aNewScope New scope for the wrappers. May be null if aCx is null.
   * @param aNodesWithProperties All nodes (from amongst aNode and its
   *                             descendants) with properties. If aClone is
   *                             PR_TRUE every node will be followed by its
   *                             clone.
   * @param aParent If aClone is PR_TRUE the cloned node will be appended to
   *                aParent's children. May be null.
   * @param aResult *aResult will contain the cloned node (if aClone is
   *                PR_TRUE).
   */
  static nsresult CloneAndAdopt(nsINode *aNode, PRBool aClone, PRBool aDeep,
                                nsNodeInfoManager *aNewNodeInfoManager,
                                JSContext *aCx, JSObject *aOldScope,
                                JSObject *aNewScope,
                                nsCOMArray<nsINode> &aNodesWithProperties,
                                nsINode *aParent, nsIDOMNode **aResult);
};

#endif // nsNodeUtils_h___
