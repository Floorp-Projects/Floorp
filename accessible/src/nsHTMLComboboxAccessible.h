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
 * Original Author: John Gaunt (jgaunt@netscape.com)
 *
 * Contributor(s):
 */
#ifndef __nsHTMLComboboxAccessible_h__
#define __nsHTMLComboboxAccessible_h__

#include "nsAccessible.h"
#include "nsIAccessibleSelectable.h"

#include "nsCOMPtr.h"
#include "nsHTMLSelectListAccessible.h"
#include "nsIDOMXULListener.h"

/*
 * A class the represents the HTML Combobox widget.
 */
class nsHTMLComboboxAccessible : public nsAccessible,
                                 public nsIAccessibleSelectable,
                                 public nsIDOMXULListener
{
public:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLESELECTABLE
  
  nsHTMLComboboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLComboboxAccessible();

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccChildCount(PRInt32 *_retval);
  NS_IMETHOD GetAccValue(nsAWritableString& _retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);

  // popup listener
  NS_IMETHOD PopupShowing(nsIDOMEvent* aEvent);
  NS_IMETHOD PopupShown(nsIDOMEvent* aEvent) { return NS_OK; }
  NS_IMETHOD PopupHiding(nsIDOMEvent* aEvent);
  NS_IMETHOD PopupHidden(nsIDOMEvent* aEvent) { return NS_OK; }
  
  NS_IMETHOD Close(nsIDOMEvent* aEvent);
  NS_IMETHOD Command(nsIDOMEvent* aEvent) { return NS_OK; }
  NS_IMETHOD Broadcast(nsIDOMEvent* aEvent) { return NS_OK; }
  NS_IMETHOD CommandUpdate(nsIDOMEvent* aEvent) { return NS_OK; }
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; }

  virtual void SetupMenuListener();

protected:

  PRBool mRegistered;
  PRBool mOpen;

};

/*
 * A class the represents the text field in the Select to the left
 *     of the drop down button
 */
class nsHTMLComboboxTextFieldAccessible  : public nsLeafAccessible
{
public:
  
  nsHTMLComboboxTextFieldAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccValue(nsAWritableString& _retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);

  virtual void GetBounds(nsRect& aBounds, nsIFrame** aBoundingFrame);

protected:
  
  nsCOMPtr<nsIAccessible> mParent;
};

/**
  * A class that represents the button inside the Select to the
  *     right of the text field
  */
class nsHTMLComboboxButtonAccessible  : public nsAccessible
{
public:

  enum { eAction_Click=0 };
  
  nsHTMLComboboxButtonAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccName(nsAWritableString& _retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccChildCount(PRInt32 *_retval);
  NS_IMETHOD GetAccNumActions(PRUint8 *_retval);
  NS_IMETHOD GetAccActionName(PRUint8 index, nsAWritableString& _retval);
  NS_IMETHOD AccDoAction(PRUint8 index);
  NS_IMETHOD GetAccState(PRUint32 *_retval);

  virtual void GetBounds(nsRect& aBounds, nsIFrame** aBoundingFrame);

protected:
  
  nsCOMPtr<nsIAccessible> mParent;
};

/*
 * A class that represents the window that lives to the right
 * of the drop down button inside the Select. This is the window
 * that is made visible when the button is pressed.
 */
class nsHTMLComboboxWindowAccessible : public nsAccessible
{
public:

  nsHTMLComboboxWindowAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccChildCount(PRInt32 *_retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);

  virtual void GetBounds(nsRect& aBounds, nsIFrame** aBoundingFrame);
   
protected:
  
  nsCOMPtr<nsIAccessible> mParent; 
};

#endif
