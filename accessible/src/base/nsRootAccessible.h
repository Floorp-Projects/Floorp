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
#include "nsIDOMFocusListener.h"

class nsRootAccessible : public nsAccessible,
                         public nsIAccessibleEventReceiver,
                         public nsIDOMFocusListener

{
  
  NS_DECL_ISUPPORTS_INHERITED

  public:
    nsRootAccessible(nsIWeakReference* aShell, nsIFrame* aFrame = nsnull);
    virtual ~nsRootAccessible();

    /* attribute wstring accName; */
    NS_IMETHOD GetAccName(PRUnichar * *aAccName);
    NS_IMETHOD GetAccParent(nsIAccessible * *aAccParent);
    NS_IMETHOD GetAccRole(PRUnichar * *aAccRole);

    // ----- nsIAccessibleEventReceiver ------

    NS_IMETHOD AddAccessibleEventListener(nsIAccessibleEventListener *aListener);
    NS_IMETHOD RemoveAccessibleEventListener(nsIAccessibleEventListener *aListener);

    // ----- nsIDOMEventListener --------
    NS_IMETHOD HandleEvent(nsIDOMEvent* anEvent);
    NS_IMETHOD Focus(nsIDOMEvent* aEvent);
    NS_IMETHOD Blur(nsIDOMEvent* aEvent);

protected:
  virtual nsIFrame* GetFrame();
  virtual nsIAccessible* CreateNewAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell);

  // not a com pointer. We don't own the listener
  // it is the callers responsibility to remove the listener
  // otherwise we will get into circular referencing problems
  nsIAccessibleEventListener* mListener;
  nsCOMPtr<nsIContent> mCurrentFocus;
};


#endif  
