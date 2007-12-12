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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Aaron Leventhal (aaronl@netscape.com)
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

#ifndef _nsDocAccessible_H_
#define _nsDocAccessible_H_

#include "nsHyperTextAccessibleWrap.h"
#include "nsIAccessibleDocument.h"
#include "nsPIAccessibleDocument.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIEditor.h"
#include "nsIObserver.h"
#include "nsIScrollPositionListener.h"
#include "nsITimer.h"
#include "nsIWeakReference.h"
#include "nsCOMArray.h"
#include "nsIDocShellTreeNode.h"

class nsIScrollableView;

const PRUint32 kDefaultCacheSize = 256;

class nsDocAccessible : public nsHyperTextAccessibleWrap,
                        public nsIAccessibleDocument,
                        public nsPIAccessibleDocument,
                        public nsIDocumentObserver,
                        public nsIObserver,
                        public nsIScrollPositionListener,
                        public nsSupportsWeakReference
{  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLEDOCUMENT
  NS_DECL_NSPIACCESSIBLEDOCUMENT
  NS_DECL_NSIOBSERVER

  public:
    nsDocAccessible(nsIDOMNode *aNode, nsIWeakReference* aShell);
    virtual ~nsDocAccessible();

    NS_IMETHOD GetRole(PRUint32 *aRole);
    NS_IMETHOD GetName(nsAString& aName);
    NS_IMETHOD GetValue(nsAString& aValue);
    NS_IMETHOD GetDescription(nsAString& aDescription);
    NS_IMETHOD GetState(PRUint32 *aState, PRUint32 *aExtraState);
    NS_IMETHOD GetFocusedChild(nsIAccessible **aFocusedChild);
    NS_IMETHOD GetParent(nsIAccessible **aParent);
    NS_IMETHOD TakeFocus(void);

    // ----- nsIScrollPositionListener ---------------------------
    NS_IMETHOD ScrollPositionWillChange(nsIScrollableView *aView, nscoord aX, nscoord aY);
    NS_IMETHOD ScrollPositionDidChange(nsIScrollableView *aView, nscoord aX, nscoord aY);

    // nsIDocumentObserver
    NS_DECL_NSIDOCUMENTOBSERVER

    static void FlushEventsCallback(nsITimer *aTimer, void *aClosure);

    // nsIAccessNode
    NS_IMETHOD Shutdown();
    NS_IMETHOD Init();

    // nsPIAccessNode
    NS_IMETHOD_(nsIFrame *) GetFrame(void);

    // nsIAccessibleText
    NS_IMETHOD GetAssociatedEditor(nsIEditor **aEditor);

    enum EDupeEventRule { eAllowDupes, eCoalesceFromSameSubtree, eRemoveDupes };

    /**
      * Non-virtual method to fire a delayed event after a 0 length timeout
      *
      * @param aEvent - the nsIAccessibleEvent event type
      * @param aDOMNode - DOM node the accesible event should be fired for
      * @param aData - any additional data for the event
      * @param aAllowDupes - eAllowDupes: more than one event of the same type is allowed. 
      *                      eCoalesceFromSameSubtree: if two events are in the same subtree,
      *                                                only the event on ancestor is used
      *                      eRemoveDupes (default): events of the same type are discarded
      *                                              (the last one is used)
      *
      * @param aIsAsyn - set to PR_TRUE if this is not being called from code
      *                  synchronous with a DOM event
      */
    nsresult FireDelayedToolkitEvent(PRUint32 aEvent, nsIDOMNode *aDOMNode,
                                     void *aData, EDupeEventRule aAllowDupes = eRemoveDupes,
                                     PRBool aIsAsynch = PR_FALSE);

    /**
     * Fire accessible event in timeout.
     *
     * @param aEvent - the event to fire
     * @param aAllowDupes - if false then delayed events of the same type and
     *                      for the same DOM node in the event queue won't
     *                      be fired.
     * @param aIsAsych - set to PR_TRUE if this is being called from
     *                   an event asynchronous with the DOM
     */
    nsresult FireDelayedAccessibleEvent(nsIAccessibleEvent *aEvent,
                                        EDupeEventRule aAllowDupes = eRemoveDupes,
                                        PRBool aIsAsynch = PR_FALSE);

    void ShutdownChildDocuments(nsIDocShellTreeItem *aStart);

  protected:
    virtual void GetBoundsRect(nsRect& aRect, nsIFrame** aRelativeFrame);
    virtual nsresult AddEventListeners();
    virtual nsresult RemoveEventListeners();
    void AddScrollListener();
    void RemoveScrollListener();
    void RefreshNodes(nsIDOMNode *aStartNode);
    static void ScrollTimerCallback(nsITimer *aTimer, void *aClosure);

    /**
     * Fires accessible events when attribute is changed.
     *
     * @param aContent - node that attribute is changed for
     * @param aNameSpaceID - namespace of changed attribute
     * @param aAttribute - changed attribute
     */
    void AttributeChangedImpl(nsIContent* aContent, PRInt32 aNameSpaceID, nsIAtom* aAttribute);

    /**
     * Fires accessible events when ARIA attribute is changed.
     *
     * @param aContent - node that attribute is changed for
     * @param aAttribute - changed attribute
     */
    void ARIAAttributeChanged(nsIContent* aContent, nsIAtom* aAttribute);

    /**
     * Fire text changed event for character data changed. The method is used
     * from nsIMutationObserver methods.
     *
     * @param aContent     the text node holding changed data
     * @param aInfo        info structure describing how the data was changed
     * @param aIsInserted  the flag pointed whether removed or inserted
     *                     characters should be cause of event
     */
    void FireTextChangeEventForText(nsIContent *aContent,
                                    CharacterDataChangeInfo* aInfo,
                                    PRBool aIsInserted);

    /**
     * Create a text change event for a changed node
     * @param aContainerAccessible, the first accessible in the container
     * @param aChangeNode, the node that is being inserted or removed, or shown/hidden
     * @param aAccessibleForChangeNode, the accessible for that node, or nsnull if none exists
     * @param aIsInserting, is aChangeNode being created or shown (vs. removed or hidden)
     */
    already_AddRefed<nsIAccessibleTextChangeEvent>
    CreateTextChangeEventForNode(nsIAccessible *aContainerAccessible,
                                 nsIDOMNode *aChangeNode,
                                 nsIAccessible *aAccessibleForNode,
                                 PRBool aIsInserting,
                                 PRBool aIsAsynch);

    /**
     * Fire show/hide events for either the current node if it has an accessible,
     * or the first-line accessible descendants of the given node.
     *
     * @param aDOMNode               the given node
     * @param aEventType             event type to fire an event
     * @param aDelay                 whether to fire the event on a delay
     * @param aForceIsFromUserInput  the event is known to be from user input
     */
    nsresult FireShowHideEvents(nsIDOMNode *aDOMNode, PRUint32 aEventType,
                                PRBool aDelay, PRBool aForceIsFromUserInput);

    nsAccessNodeHashtable mAccessNodeCache;
    void *mWnd;
    nsCOMPtr<nsIDocument> mDocument;
    nsCOMPtr<nsITimer> mScrollWatchTimer;
    nsCOMPtr<nsITimer> mFireEventTimer;
    PRUint16 mScrollPositionChangedTicks; // Used for tracking scroll events
    PRPackedBool mIsContentLoaded;
    nsCOMArray<nsIAccessibleEvent> mEventsToFire;

protected:
    PRBool mIsAnchor;
    PRBool mIsAnchorJumped;
    static PRUint32 gLastFocusedAccessiblesState;

private:
    static void DocLoadCallback(nsITimer *aTimer, void *aClosure);
    nsCOMPtr<nsITimer> mDocLoadTimer;
};

#endif  
