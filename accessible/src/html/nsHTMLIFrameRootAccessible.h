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

#ifndef _nsIFrameRootAccessible_H_
#define _nsIFrameRootAccessible_H_

#include "nsRootAccessible.h"
#include "nsAccessible.h"
#include "nsIAccessibleDocument.h"

class nsIWebShell;
class nsIWeakReference;

class nsHTMLIFrameAccessible : public nsHTMLBlockAccessible, 
                               public nsIAccessibleDocument,
                               public nsDocAccessibleMixin
{
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLEDOCUMENT

  public:
    nsHTMLIFrameAccessible(nsIDOMNode* aNode, nsIAccessible* aRoot, nsIWeakReference* aShell, nsIDocument *doc);

    NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
    NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
    NS_IMETHOD GetAccChildCount(PRInt32 *_retval);
    NS_IMETHOD GetAccName(nsAWritableString& aAccName);
    NS_IMETHOD GetAccValue(nsAWritableString& AccValue);
    NS_IMETHOD GetAccRole(PRUint32 *aAccRole);
    NS_IMETHOD GetAccState(PRUint32 *aAccState);

  protected:
    nsCOMPtr<nsIAccessible> mRootAccessible;
};

class nsHTMLIFrameRootAccessible : public nsRootAccessible
{
  NS_DECL_ISUPPORTS_INHERITED

  public:
    nsHTMLIFrameRootAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
    virtual ~nsHTMLIFrameRootAccessible();

    /* attribute wstring accName; */
    NS_IMETHOD GetAccParent(nsIAccessible * *aAccParent);

    /* nsIAccessible getAccNextSibling (); */
    NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);

    /* nsIAccessible getAccPreviousSibling (); */
    NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);

  protected:
    NS_IMETHOD GetHTMLIFrameAccessible(nsIAccessible** aAcc);
    nsCOMPtr<nsIDOMNode> mRealDOMNode;
};


#endif  
