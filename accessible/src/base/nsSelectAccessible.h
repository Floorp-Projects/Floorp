/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Original Author: Eric Vaughan (evaughan@netscape.com)
 * Contributor(s):  John Gaunt (jgaunt@netscape.com)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nsSelectAccessible_h__
#define __nsSelectAccessible_h__

#include "nsBaseWidgetAccessible.h"
#include "nsIAccessibleSelectable.h"
#include "nsIDOMXULListener.h"

/** ------------------------------------------------------ */
/**  First, the common widgets                             */
/** ------------------------------------------------------ */

/**
  * The list that contains all the options in the select.
  */
class nsSelectListAccessible : public nsAccessible
{
public:
  
  nsSelectListAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsSelectListAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height);
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);

protected:
  
  nsCOMPtr<nsIAccessible> mParent;
};

/**
  * Options inside the select, contained within the list
  */
class nsSelectOptionAccessible : public nsLeafAccessible
{
public:
  
  nsSelectOptionAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsSelectOptionAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccName(nsAWritableString& _retval);

protected:
  
  nsCOMPtr<nsIAccessible> mParent;
};

/** ------------------------------------------------------ */
/**  Secondly, the Listbox widget                          */
/** ------------------------------------------------------ */

/**
  * A class that represents the Listbox widget.
  */
class nsListboxAccessible : public nsAccessible
{
public:
  
  nsListboxAccessible (nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsListboxAccessible () {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccChildCount(PRInt32 *_retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);

};

/** ------------------------------------------------------ */
/**  Finally, the Combobox widgets                         */
/** ------------------------------------------------------ */

/**
  * A class the represents the HTML Combobox widget.
  */
class nsComboboxAccessible : public nsAccessible,
                             public nsIAccessibleSelectable,
                             public nsIDOMXULListener
{
public:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLESELECTABLE
  
  nsComboboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsComboboxAccessible();

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccChildCount(PRInt32 *_retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);

  /* ----- nsIDOMXULListener ----- */
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

/**
  * A class the represents the text field in the Combobox to the left
  *     of the drop down button
  */
class nsComboboxTextFieldAccessible  : public nsLeafAccessible
{
public:
  
  nsComboboxTextFieldAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsComboboxTextFieldAccessible() {}

  /* ----- nsIAccessible ----- */
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
class nsComboboxButtonAccessible  : public nsLeafAccessible
{
public:

  nsComboboxButtonAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsComboboxButtonAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccName(nsAWritableString& _retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccNumActions(PRUint8 *_retval);
  NS_IMETHOD GetAccActionName(PRUint8 index, nsAWritableString& _retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);

  virtual void GetBounds(nsRect& aBounds, nsIFrame** aBoundingFrame);

protected:
  
  nsCOMPtr<nsIAccessible> mParent;
};

/**
  * A class that represents the window that lives to the right
  * of the drop down button inside the Select. This is the window
  * that is made visible when the button is pressed.
  */
class nsComboboxWindowAccessible : public nsAccessible
{
public:

  nsComboboxWindowAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsComboboxWindowAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccChildCount(PRInt32 *_retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);

  virtual void GetBounds(nsRect& aBounds, nsIFrame** aBoundingFrame);
   
protected:
  
  nsCOMPtr<nsIAccessible> mParent; 
};

#endif
