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
#ifndef __nsHTMLSelectAccessible_h__
#define __nsHTMLSelectAccessible_h__

#include "nsCOMPtr.h"
#include "nsIAccessibleSelectable.h"
#include "nsIDOMNode.h"
#include "nsIWeakReference.h"
#include "nsSelectAccessible.h"

/**
  * Selects, Listboxes and Comboboxes, are made up of a number of different
  *  widgets, some of which are shared between the two. This file contains 
  *  all of the widgets for both of the Selects, for HTML only. Some of them
  *  extend classes from nsSelectAccessible.cpp, which contains base classes 
  *  that are also extended by the XUL Select Accessibility support.
  *
  *  Listbox:
  *     - nsHTMLListboxAccessible
  *        - nsHTMLSelctListAccessible
  *           - nsHTMLSelectOptionAccessible
  *
  *  Comboboxes:
  *     - nsHTMLComboboxAccessilbe
  *        - nsHTMLComboboxTextFieldAccessible
  *        - nsHTMLComboboxButtonAccessible
  *        - nsHTMLComboboxWindowAccessilbe
  *           - nsHTMLSelectListAccessible
  *              - nsHTMLSelectOptionAccessible(s)
  */

/** ------------------------------------------------------ */
/**  First, the common widgets                             */
/** ------------------------------------------------------ */

/*
 * The list that contains all the options in the select.
 */
class nsHTMLSelectListAccessible : public nsSelectListAccessible
{
public:
  
  nsHTMLSelectListAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLSelectListAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccState(PRUint32 *_retval);
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);

};

/*
 * Options inside the select, contained within the list
 */
class nsHTMLSelectOptionAccessible : public nsSelectOptionAccessible
{
public:
  
  nsHTMLSelectOptionAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLSelectOptionAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccState(PRUint32 *_retval);
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);

  static nsresult GetFocusedOptionNode(nsIWeakReference *aPresShell, nsIDOMNode *aListNode, nsCOMPtr<nsIDOMNode>& aFocusedOptionNode);

};

/** ------------------------------------------------------ */
/**  Secondly, the Listbox widget                          */
/** ------------------------------------------------------ */

/*
 * A class the represents the HTML Listbox widget.
 */
class nsHTMLListboxAccessible : public nsListboxAccessible,
                                public nsIAccessibleSelectable  
{
public:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLESELECTABLE
  
  nsHTMLListboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLListboxAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccValue(nsAWritableString& _retval);

};

/** ------------------------------------------------------ */
/**  Finally, the Combobox widgets                         */
/** ------------------------------------------------------ */

/*
 * A class the represents the HTML Combobox widget.
 */
class nsHTMLComboboxAccessible : public nsComboboxAccessible
{
public:

  nsHTMLComboboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLComboboxAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccValue(nsAWritableString& _retval);

};

/*
 * A class the represents the text field in the Select to the left
 *     of the drop down button
 */
class nsHTMLComboboxTextFieldAccessible  : public nsComboboxTextFieldAccessible
{
public:
  
  nsHTMLComboboxTextFieldAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLComboboxTextFieldAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);

};

/**
  * A class that represents the button inside the Select to the
  *     right of the text field
  */
class nsHTMLComboboxButtonAccessible  : public nsComboboxButtonAccessible
{
public:

  nsHTMLComboboxButtonAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLComboboxButtonAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);
  NS_IMETHOD AccDoAction(PRUint8 index);

};

/*
 * A class that represents the window that lives to the right
 * of the drop down button inside the Select. This is the window
 * that is made visible when the button is pressed.
 */
class nsHTMLComboboxWindowAccessible : public nsComboboxWindowAccessible
{
public:

  nsHTMLComboboxWindowAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLComboboxWindowAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);

};


#endif
