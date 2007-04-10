/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Aaron Leventhal (aaronl@netscape.com)
 *   Kyle Yuan (kyle.yuan@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef __nsHTMLSelectAccessible_h__
#define __nsHTMLSelectAccessible_h__

#include "nsIAccessibleSelectable.h"
#include "nsAccessibilityAtoms.h"
#include "nsHTMLFormControlAccessible.h"
#include "nsIDOMHTMLOptionsCollection.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMNode.h"
#include "nsIAccessibilityService.h"
#include "nsAccessibleTreeWalker.h"

class nsIMutableArray;

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
class nsHTMLSelectableAccessible : public nsAccessibleWrap
{
public:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLESELECTABLE

  nsHTMLSelectableAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLSelectableAccessible() {}

  NS_IMETHOD GetName(nsAString &aName) { return GetHTMLName(aName, PR_FALSE); }

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
                                 nsPresContext *aContext);
    PRBool GetAccessibleIfSelected(PRInt32 aIndex, nsIAccessibilityService *aAccService, nsPresContext *aContext, nsIAccessible **_retval);

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
  NS_IMETHOD GetRole(PRUint32 *aRole);
  NS_IMETHOD GetState(PRUint32 *aState, PRUint32 *aExtraState);
  void CacheChildren();

protected:
  already_AddRefed<nsIAccessible>
    AccessibleForOption(nsIAccessibilityService *aAccService,
                        nsIContent *aContent,
                        nsIAccessible *aLastGoodAccessible,
                        PRInt32 *aChildCount);
  already_AddRefed<nsIAccessible>
    CacheOptSiblings(nsIAccessibilityService *aAccService,
                     nsIContent *aParentContent,
                     nsIAccessible *aLastGoodAccessible,
                     PRInt32 *aChildCount);
};

/*
 * Options inside the select, contained within the list
 */
class nsHTMLSelectOptionAccessible : public nsHyperTextAccessible
{
public:
  enum { eAction_Select = 0 };  
  
  nsHTMLSelectOptionAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLSelectOptionAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD GetNumActions(PRUint8 *_retval);
  NS_IMETHOD GetState(PRUint32 *aState, PRUint32 *aExtraState);
  NS_IMETHOD GetRole(PRUint32 *aRole);
  NS_IMETHOD GetName(nsAString& aName);
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);

  nsIFrame*  GetBoundsFrame();
  static nsresult GetFocusedOptionNode(nsIDOMNode *aListNode, nsIDOMNode **aFocusedOptionNode);
  static void SelectionChangedIfOption(nsIContent *aPossibleOption);

private:
  
  /**
   * Get Select element's accessible state
   * @param aState, Select element state
   * @return Select element content, returns null if not avaliable
   */ 
  nsIContent* GetSelectState(PRUint32* aState);
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
  NS_IMETHOD GetState(PRUint32 *aState, PRUint32 *aExtraState);
  NS_IMETHOD DoAction(PRUint8 index);  
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD GetNumActions(PRUint8 *_retval);
};

/** ------------------------------------------------------ */
/**  Finally, the Combobox widgets                         */
/** ------------------------------------------------------ */

/*
 * A class the represents the HTML Combobox widget.
 */
class nsHTMLComboboxAccessible : public nsAccessibleWrap
{
public:
  enum { eAction_Click = 0 };

  nsHTMLComboboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLComboboxAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetRole(PRUint32 *_retval);
  NS_IMETHOD GetState(PRUint32 *aState, PRUint32 *aExtraState);
  NS_IMETHOD GetValue(nsAString& _retval);
  NS_IMETHOD GetDescription(nsAString& aDescription);
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetNumActions(PRUint8 *aNumActions);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);

  void CacheChildren();

protected:
  already_AddRefed<nsIAccessible> GetFocusedOptionAccessible();
};

#ifdef COMBO_BOX_WITH_THREE_CHILDREN
/*
 * A class the represents the text field in the Select to the left
 *     of the drop down button
 */
class nsHTMLComboboxTextFieldAccessible  : public nsHTMLTextFieldAccessible
{
public:
  
  nsHTMLComboboxTextFieldAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLComboboxTextFieldAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetUniqueID(void **aUniqueID);

  virtual void GetBoundsRect(nsRect& aBounds, nsIFrame** aBoundingFrame);

protected:
  void CacheChildren();
};

/**
  * A class that represents the button inside the Select to the
  *     right of the text field
  */
class nsHTMLComboboxButtonAccessible  : public nsLeafAccessible
{
public:
  enum { eAction_Click = 0 };

  nsHTMLComboboxButtonAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLComboboxButtonAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetNumActions(PRUint8 *_retval);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD GetParent(nsIAccessible **_retval);
  NS_IMETHOD GetName(nsAString& _retval);
  NS_IMETHOD GetRole(PRUint32 *_retval);
  NS_IMETHOD GetState(PRUint32 *aState, PRUint32 *aExtraState);
  NS_IMETHOD GetUniqueID(void **aUniqueID);

  virtual void GetBoundsRect(nsRect& aBounds, nsIFrame** aBoundingFrame);
};
#endif

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
                               nsIFrame *aListFrame,
                               nsIWeakReference* aShell);
  virtual ~nsHTMLComboboxListAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetState(PRUint32 *aState, PRUint32 *aExtraState);
  NS_IMETHOD GetParent(nsIAccessible **aParent);
  NS_IMETHOD GetUniqueID(void **aUniqueID);

  // nsPIAccessNode
  NS_IMETHOD_(nsIFrame *) GetFrame(void);

  virtual void GetBoundsRect(nsRect& aBounds, nsIFrame** aBoundingFrame);

protected:
  nsIFrame *mListFrame;
};

#endif
