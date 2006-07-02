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

#include "prtypes.h"

class nsINode;
class nsIContent;
class nsIDocument;
class nsIAtom;

class nsNodeUtils
{
public:
  /**
   * Send CharacterDataChanged notifications to nsIMutationObservers.
   * @param aContent  Node whose data changed
   * @param aAppend   True if data was only appended
   * @see nsIMutationObserver::CharacterDataChanged
   */
  static void CharacterDataChanged(nsIContent* aContent, PRBool aAppend);

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
   * Call this before starting to tear down a node. The node should still
   * have children and attributes accessible.
   * The function will notify nsIMutationObservers as well as delete the
   * slots structure.
   * @param aNode  Node that is being destroyed
   * @see nsIMutationObserver::NodeWillBeDestroyed
   */
  static void NodeWillBeDestroyed(nsINode* aNode);
};

#endif // nsNodeUtils_h___
