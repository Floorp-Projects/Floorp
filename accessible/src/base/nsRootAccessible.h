/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation..
 * Portions created by Netscape Communications Corporation. are
 * Copyright (C) 1998 Netscape Communications Corporation.. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef _nsRootAccessible_H_
#define _nsRootAccessible_H_

#include "nsAccessible.h"
#include "nsIAccessibleEventReceiver.h"
#include "nsIAccessibleEventListener.h"
#include "nsIAccessibleDocument.h"
#include "nsIDOMFormListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDocument.h"
#include "nsIAccessibilityService.h"

class nsDocAccessibleMixin
{
  public:
    nsDocAccessibleMixin(nsIDocument *doc);
    nsDocAccessibleMixin(nsIWeakReference *aShell);
    virtual ~nsDocAccessibleMixin();

    NS_DECL_NSIACCESSIBLEDOCUMENT

    NS_IMETHOD GetAccState(PRUint32 *aAccState);

  protected:
    nsCOMPtr<nsIDocument> mDocument;
};

class nsRootAccessible : public nsAccessible,
                         public nsDocAccessibleMixin,
                         public nsIAccessibleDocument,
                         public nsIAccessibleEventReceiver,
                         public nsIDOMFocusListener,
                         public nsIDOMFormListener

{
  NS_DECL_ISUPPORTS_INHERITED

  public:
    nsRootAccessible(nsIWeakReference* aShell);
    virtual ~nsRootAccessible();

    /* attribute wstring accName; */
    NS_IMETHOD GetAccName(nsAWritableString& aAccName);
    NS_IMETHOD GetAccValue(nsAWritableString& aAccValue);
    NS_IMETHOD GetAccParent(nsIAccessible * *aAccParent);
    NS_IMETHOD GetAccRole(PRUint32 *aAccRole);
    NS_IMETHOD GetAccState(PRUint32 *aAccState);

    // ----- nsIAccessibleEventReceiver -------------------

    NS_IMETHOD AddAccessibleEventListener(nsIAccessibleEventListener *aListener);
    NS_IMETHOD RemoveAccessibleEventListener(nsIAccessibleEventListener *aListener);

    // ----- nsIDOMEventListener --------------------------
    NS_IMETHOD HandleEvent(nsIDOMEvent* anEvent);

    // ----- nsIDOMFocusListener --------------------------
    NS_IMETHOD Focus(nsIDOMEvent* aEvent);
    NS_IMETHOD Blur(nsIDOMEvent* aEvent);

    // ----- nsIDOMFormListener ---------------------------
    NS_IMETHOD Submit(nsIDOMEvent* aEvent);
    NS_IMETHOD Reset(nsIDOMEvent* aEvent);
    NS_IMETHOD Change(nsIDOMEvent* aEvent);
    NS_IMETHOD Select(nsIDOMEvent* aEvent);
    NS_IMETHOD Input(nsIDOMEvent* aEvent);

    NS_DECL_NSIACCESSIBLEDOCUMENT

  protected:
    NS_IMETHOD GetTargetNode(nsIDOMEvent *aEvent, nsCOMPtr<nsIDOMNode>& aTargetNode);
    virtual void GetBounds(nsRect& aRect, nsIFrame** aRelativeFrame);
    virtual nsIFrame* GetFrame();

    // not a com pointer. We don't own the listener
    // it is the callers responsibility to remove the listener
    // otherwise we will get into circular referencing problems
    nsIAccessibleEventListener* mListener;
    nsCOMPtr<nsIDOMNode> mCurrentFocus;
    nsCOMPtr<nsIAccessibilityService> mAccService;
};


#endif  
