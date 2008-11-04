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
#ifndef __nsXULSelectAccessible_h__
#define __nsXULSelectAccessible_h__

#include "nsIAccessibleTable.h"

#include "nsCOMPtr.h"
#include "nsXULMenuAccessible.h"
#include "nsBaseWidgetAccessible.h"

class nsIWeakReference;

/**
 * nsXULColumnsAccessible are accessible for list and tree columns elements
 * (xul:treecols and xul:listcols).
 */
class nsXULColumnsAccessible : public nsAccessibleWrap
{
public:
  nsXULColumnsAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);

  // nsIAccessible
  NS_IMETHOD GetRole(PRUint32 *aRole);

  // nsAccessible
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
};

/**
 * nsXULColumnAccessible are accessibles for list and tree column elements
 * (xul:listcol and xul:treecol).
 */
class nsXULColumnItemAccessible : public nsLeafAccessible
{
public:
  nsXULColumnItemAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);

  // nsIAccessible
  NS_IMETHOD GetRole(PRUint32 *aRole);

  NS_IMETHOD GetNumActions(PRUint8 *aNumActions);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsAccessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);

  enum { eAction_Click = 0 };
};

/**
  * Listboxes (xul:listbox) and Comboboxes (xul:menulist) are made up of a
  * number of different widgets, some of which are shared between the two.
  * This file contains all of the widgets for both of them, for XUL only.
  *
  *  Listbox:
  *     - nsXULListboxAccessible
  *        - nsXULSelectOptionAccessible
  *
  *  Comboboxes:
  *     - nsXULComboboxAccessible      <menulist />
  *        - nsXULMenuAccessible          <menupopup />
  *           - nsXULMenuitemAccessible(s)   <menuitem />
  */

/** ------------------------------------------------------ */
/**  First, the common widgets                             */
/** ------------------------------------------------------ */

/*
 * A class the represents the XUL Listbox widget.
 */
class nsXULListboxAccessible : public nsXULSelectableAccessible,
                               public nsIAccessibleTable
{
public:
  nsXULListboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsXULListboxAccessible() {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLETABLE

  // nsIAccessible
  NS_IMETHOD GetRole(PRUint32 *aRole);
  NS_IMETHOD GetValue(nsAString& aValue);

  // nsAccessible
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);

protected:
  PRBool IsTree();
};

/**
  * Listitems -- used in listboxes 
  */
class nsXULListitemAccessible : public nsXULMenuitemAccessible
{
public:
  enum { eAction_Click = 0 };

  NS_DECL_ISUPPORTS_INHERITED
  
  nsXULListitemAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsXULListitemAccessible() {}

  // nsIAccessible
  NS_IMETHOD GetRole(PRUint32 *_retval);
  NS_IMETHOD GetActionName(PRUint8 index, nsAString& aName);
  // Don't use XUL menuitems's description attribute
  NS_IMETHOD GetDescription(nsAString& aDesc) { return nsAccessibleWrap::GetDescription(aDesc); }
  NS_IMETHOD GetAllowsAnonChildAccessibles(PRBool *aAllowsAnonChildren);

  // nsAccessible
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);

protected:
  already_AddRefed<nsIAccessible> GetListAccessible();

private:
  PRBool mIsCheckbox;
};

/**
 * Class represents xul:listcell.
 */
class nsXULListCellAccessible : public nsHyperTextAccessibleWrap
{
public:
  nsXULListCellAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);

  // nsIAccessible
  NS_IMETHOD GetRole(PRUint32 *aRole);
};

/** ------------------------------------------------------ */
/**  Finally, the Combobox widgets                         */
/** ------------------------------------------------------ */

/*
 * A class the represents the XUL Combobox widget.
 */
class nsXULComboboxAccessible : public nsAccessibleWrap
{
public:
  enum { eAction_Click = 0 };

  nsXULComboboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsXULComboboxAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetRole(PRUint32 *_retval);
  NS_IMETHOD GetValue(nsAString& _retval);
  NS_IMETHOD GetDescription(nsAString& aDescription);
  NS_IMETHOD GetAllowsAnonChildAccessibles(PRBool *aAllowsAnonChildren);
  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetNumActions(PRUint8 *aNumActions);
  NS_IMETHOD GetActionName(PRUint8 index, nsAString& aName);

  // nsAccessNode
  virtual nsresult Init();

  // nsAccessible
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
};

#endif
