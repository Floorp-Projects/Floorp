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
 * Author: Eric D Vaughan (evaughan@netscape.com)
 * Contributor(s): 
 */

#ifndef _nsGenericAccessible_H_
#define _nsGenericAccessible_H_

#include "nsISupports.h"
#include "nsIAccessible.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsCOMPtr.h"

class nsIWeakReference;

/**
 * Basic implementation
 * supports nothing
 */
class nsGenericAccessible : public nsIAccessible
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCESSIBLE

	public:
    nsGenericAccessible();
    virtual ~nsGenericAccessible();
};

/**
 * And accessible that observes a dom node
 * supports:
 * - selection
 * - focus
 */
class nsDOMAccessible : public nsGenericAccessible
{
  public:
    nsDOMAccessible(nsIPresShell* aShell, nsIDOMNode* aNode);

    NS_IMETHOD AccTakeSelection(void); 
    NS_IMETHOD AccTakeFocus(void); 
    NS_IMETHOD AccRemoveSelection(void);

  protected:
    nsIWeakReference* mPresShell;
    nsCOMPtr<nsIDOMNode> mNode;
};

/* Leaf version of DOM Accessible
 * has no children
 */
class nsLeafDOMAccessible : public nsDOMAccessible
{
  public:
    nsLeafDOMAccessible(nsIPresShell* aShell, nsIDOMNode* aNode);

    NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
    NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
    NS_IMETHOD GetAccChildCount(PRInt32 *_retval);
};

#endif  
