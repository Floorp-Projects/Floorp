/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Original Author: Eric Vaughan (evaughan@netscape.com)
 *
 * Contributor(s): 
 */
#ifndef __nsHTMLSelectListAccessible_h__
#define __nsHTMLSelectListAccessible_h__

#include "nsAccessible.h"

#include "nsCOMPtr.h"

/*
 * The list that contains all the options in the select.
 */
class nsHTMLSelectListAccessible : public nsAccessible
{
public:
  
  nsHTMLSelectListAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLSelectListAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);
  NS_IMETHOD AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height);
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);

protected:
  
  nsCOMPtr<nsIAccessible> mParent;
};

/*
 * Options inside the select, contained within the list
 */
class nsHTMLSelectOptionAccessible : public nsLeafAccessible
{
public:
  
  nsHTMLSelectOptionAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccName(nsAWritableString& _retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);
  static nsresult GetFocusedOptionNode(nsIWeakReference *aPresShell, nsIDOMNode *aListNode, nsCOMPtr<nsIDOMNode>& aFocusedOptionNode);

protected:
  
  nsCOMPtr<nsIAccessible> mParent;
};

#endif
