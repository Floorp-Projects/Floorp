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
 * Original Author: Eric Vaughan (evaughan@netscape.com)
 *
 * Contributor(s):
 *           Aaron Leventhal (aaronl@netscape.com)
 *           Kyle Yuan (kyle.yuan@sun.com)
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

#include "nsIAccessibleSelectable.h"
#include "nsIDOMHTMLOptionsCollection.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMNode.h"
#include "nsFormControlAccessible.h"
#include "nsIAccessibilityService.h"

/**
  *  Selects, Listboxes and Comboboxes, are made up of a number of different
  *  widgets, some of which are shared between the two. This file contains
	*  all of the widgets for both of the Selects, for HTML only.
  *
  *  Listbox:
  *     - nsHTMLSelectListAccessible
  *        - nsHTMLSelectOptionAccessible
  *
  *  Comboboxes:
  *     - nsHTMLComboboxAccessible
  *        - nsHTMLComboboxTextFieldAccessible
  *        - nsHTMLComboboxButtonAccessible
  *        - nsHTMLComboboxListAccessible  [ inserted in accessible tree ]
  *           - nsHTMLSelectOptionAccessible(s)
  */

/** ------------------------------------------------------ */
/**  First, the common widgets                             */
/** ------------------------------------------------------ */

/*
 * The HTML implementation of nsIAccessibleSelectable.
 */
class nsHTMLSelectableAccessible : public nsAccessibleWrap,
                                   public nsIAccessibleSelectable
{
public:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLESELECTABLE

  nsHTMLSelectableAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLSelectableAccessible() {}

protected:

  NS_IMETHOD ChangeSelection(PRInt32 aIndex, PRUint8 aMethod, PRBool *aSelState);

  class iterator 
  {
  protected:
    PRUint32 mLength;
    PRUint32 mIndex;
    PRInt32 mSelCount;
    nsCOMPtr<nsIDOMHTMLOptionsCollection> mOptions;
    nsCOMPtr<nsIDOMHTMLOptionElement> mOption;
    nsCOMPtr<nsIWeakReference> mWeakShell;
    nsHTMLSelectableAccessible *mParentSelect;

  public:
    void Shutdown();
    iterator(nsHTMLSelectableAccessible *aParent, nsIWeakReference *aWeakShell);

    void CalcSelectionCount(PRInt32 *aSelectionCount);
    void Select(PRBool aSelect);
    void AddAccessibleIfSelected(nsIAccessibilityService *aAccService, 
                                 nsIMutableArray *aSelectedAccessibles, 
                                 nsIPresContext *aContext);
    PRBool GetAccessibleIfSelected(PRInt32 aIndex, nsIAccessibilityService *aAccService, nsIPresContext *aContext, nsIAccessible **_retval);

    PRBool Advance();
  };

  friend class iterator;
};

/*
 * The list that contains all the options in the select.
 */
class nsHTMLSelectListAccessible : public nsHTMLSelectableAccessible
{
public:
  
  nsHTMLSelectListAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLSelectListAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccRole(PRUint32 *aAccRole);
  NS_IMETHOD GetAccState(PRUint32 *_retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **aAccFirstChild);
  NS_IMETHOD GetAccLastChild(nsIAccessible **aAccFirstChild);
  NS_IMETHOD GetAccChildCount(PRInt32 *aAccChildCount) ;
};

/*
 * Options inside the select, contained within the list
 */
class nsHTMLSelectOptionAccessible : public nsLeafAccessible
{
public:
  
  nsHTMLSelectOptionAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLSelectOptionAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD AccDoAction(PRUint8 index);
  NS_IMETHOD GetAccActionName(PRUint8 index, nsAString& _retval);
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccNumActions(PRUint8 *_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);
  NS_IMETHOD GetAccRole(PRUint32 *aAccRole);
  NS_IMETHOD GetAccParent(nsIAccessible **aParent);
  NS_IMETHOD GetAccName(nsAString& aName);
  static nsresult GetFocusedOptionNode(nsIDOMNode *aListNode, nsIDOMNode **aFocusedOptionNode);
};

/*
 * Opt Groups inside the select, contained within the list
 */
class nsHTMLSelectOptGroupAccessible : public nsHTMLSelectOptionAccessible
{
public:

  nsHTMLSelectOptGroupAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLSelectOptGroupAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccState(PRUint32 *_retval);
  NS_IMETHOD AccDoAction(PRUint8 index);  
  NS_IMETHOD GetAccActionName(PRUint8 index, nsAString& _retval);
  NS_IMETHOD GetAccNumActions(PRUint8 *_retval);
};

/** ------------------------------------------------------ */
/**  Finally, the Combobox widgets                         */
/** ------------------------------------------------------ */

/*
 * A class the represents the HTML Combobox widget.
 */
class nsHTMLComboboxAccessible : public nsHTMLSelectableAccessible
{
public:

  nsHTMLComboboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLComboboxAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccChildCount(PRInt32 *_retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);

  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccValue(nsAString& _retval);
  NS_IMETHOD Shutdown();
  NS_IMETHOD Init();

protected:
  // Hold references to our generated children
  // So that we can shut them down when we need to
  nsCOMPtr<nsIAccessible> mComboboxTextFieldAccessible;
  nsCOMPtr<nsIAccessible> mComboboxButtonAccessible;
  nsCOMPtr<nsIAccessible> mComboboxListAccessible;
};

/*
 * A class the represents the text field in the Select to the left
 *     of the drop down button
 */
class nsHTMLComboboxTextFieldAccessible  : public nsLeafAccessible
{
public:
  
  nsHTMLComboboxTextFieldAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLComboboxTextFieldAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccValue(nsAString& _retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);
  NS_IMETHOD GetUniqueID(void **aUniqueID);

  virtual void GetBounds(nsRect& aBounds, nsIFrame** aBoundingFrame);
};

/**
  * A class that represents the button inside the Select to the
  *     right of the text field
  */
class nsHTMLComboboxButtonAccessible  : public nsLeafAccessible
{
public:

  nsHTMLComboboxButtonAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLComboboxButtonAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD AccDoAction(PRUint8 index);
  NS_IMETHOD GetAccNumActions(PRUint8 *_retval);
  NS_IMETHOD GetAccActionName(PRUint8 index, nsAString& _retval);
  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccName(nsAString& _retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);
  NS_IMETHOD GetUniqueID(void **aUniqueID);

  virtual void GetBounds(nsRect& aBounds, nsIFrame** aBoundingFrame);
};

/*
 * A class that represents the window that lives to the right
 * of the drop down button inside the Select. This is the window
 * that is made visible when the button is pressed.
 */
class nsHTMLComboboxListAccessible : public nsHTMLSelectListAccessible
{
public:

  nsHTMLComboboxListAccessible(nsIAccessible *aParent, 
		                           nsIDOMNode* aDOMNode, 
															 nsIWeakReference* aShell);
  virtual ~nsHTMLComboboxListAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccState(PRUint32 *aAccState);
  NS_IMETHOD GetAccParent(nsIAccessible **aParent);
  NS_IMETHOD GetUniqueID(void **aUniqueID);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **aAccPrevSibling);

  virtual void GetBounds(nsRect& aBounds, nsIFrame** aBoundingFrame);
};

#endif
