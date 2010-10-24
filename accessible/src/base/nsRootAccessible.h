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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef _nsRootAccessible_H_
#define _nsRootAccessible_H_

#include "nsCaretAccessible.h"
#include "nsDocAccessibleWrap.h"

#include "nsIAccessibleDocument.h"
#ifdef MOZ_XUL
#include "nsXULTreeAccessible.h"
#endif

#include "nsHashtable.h"
#include "nsCaretAccessible.h"
#include "nsIDocument.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMFormListener.h"

#define NS_ROOTACCESSIBLE_IMPL_CID                      \
{  /* eaba2cf0-21b1-4e2b-b711-d3a89dcd5e1a */           \
  0xeaba2cf0,                                           \
  0x21b1,                                               \
  0x4e2b,                                               \
  { 0xb7, 0x11, 0xd3, 0xa8, 0x9d, 0xcd, 0x5e, 0x1a }    \
}

const PRInt32 SCROLL_HASH_START_SIZE = 6;

class nsRootAccessible : public nsDocAccessibleWrap,
                         public nsIDOMEventListener
{
  NS_DECL_ISUPPORTS_INHERITED

public:
  nsRootAccessible(nsIDocument *aDocument, nsIContent *aRootContent,
                   nsIWeakReference *aShell);
  virtual ~nsRootAccessible();

  // nsIAccessible
  NS_IMETHOD GetName(nsAString& aName);
  NS_IMETHOD GetRelationByType(PRUint32 aRelationType,
                               nsIAccessibleRelation **aRelation);

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

  // nsAccessNode
  virtual void Shutdown();

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);

  // nsRootAccessible
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ROOTACCESSIBLE_IMPL_CID)

  /**
   * Fire an accessible focus event for the current focusAccssible
   * and attach a new selection listener, if necessary.
   *
   * @param  aFocusAccessible  [in] the accessible which has received focus
   * @param  aFocusNode        [in] the DOM node which has received focus
   * @param  aFocusEvent       [in] DOM focus event that caused
   *                             the node/accessible to receive focus
   * @param  aForceEvent       [in] fire a focus event even if the last focused
   *                             item was the same
   * @return                    boolean -- was a focus event actually fired
   */
  PRBool FireAccessibleFocusEvent(nsAccessible *aFocusAccessible,
                                  nsINode *aFocusNode,
                                  nsIDOMEvent *aFocusEvent,
                                  PRBool aForceEvent = PR_FALSE,
                                  EIsFromUserInput aIsFromUserInput = eAutoDetect);

    /**
      * Fire an accessible focus event for the current focused node,
      * if there is a focus.
      */
    void FireCurrentFocusEvent();

    nsCaretAccessible *GetCaretAccessible();

protected:
  NS_DECL_RUNNABLEMETHOD(nsRootAccessible, FireCurrentFocusEvent)

    nsresult AddEventListeners();
    nsresult RemoveEventListeners();

  /**
   * Process "popupshown" event. Used by HandleEvent().
   */

  nsresult HandlePopupShownEvent(nsAccessible *aAccessible);
  /*
   * Process "popuphiding" event. Used by HandleEvent().
   */
  nsresult HandlePopupHidingEvent(nsINode *aNode, nsAccessible *aAccessible);

#ifdef MOZ_XUL
    nsresult HandleTreeRowCountChangedEvent(nsIDOMEvent *aEvent,
                                            nsXULTreeAccessible *aAccessible);
    nsresult HandleTreeInvalidatedEvent(nsIDOMEvent *aEvent,
                                        nsXULTreeAccessible *aAccessible);

    PRUint32 GetChromeFlags();
#endif
    already_AddRefed<nsIDocShellTreeItem>
           GetContentDocShell(nsIDocShellTreeItem *aStart);
    nsRefPtr<nsCaretAccessible> mCaretAccessible;
  nsCOMPtr<nsINode> mCurrentARIAMenubar;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsRootAccessible, NS_ROOTACCESSIBLE_IMPL_CID)

#endif  
