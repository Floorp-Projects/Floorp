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
 */

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
