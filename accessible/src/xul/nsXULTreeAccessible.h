/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Author: Kyle Yuan (kyle.yuan@sun.com)
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
#ifndef __nsXULTreeAccessible_h__
#define __nsXULTreeAccessible_h__

#include "nsAccessible.h"
#include "nsBaseWidgetAccessible.h"
#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsIWeakReference.h"
#include "nsITreeBoxObject.h"
#include "nsITreeView.h"
#include "nsXULSelectAccessible.h"
#include "nsIAccessibleTable.h"

/*
 * A class the represents the XUL Tree widget.
 */
class nsXULTreeAccessible : public nsXULSelectableAccessible,
                            public nsIAccessibleTable
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLESELECTABLE
  NS_DECL_NSIACCESSIBLETABLE

  nsXULTreeAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsXULTreeAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);
  NS_IMETHOD GetAccValue(nsAString& _retval);

  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccChildCount(PRInt32 *_retval);

  static void GetTreeBoxObject(nsIDOMNode* aDOMNode, nsITreeBoxObject** aBoxObject);

private:
  nsCOMPtr<nsITreeBoxObject> mTree;
  nsCOMPtr<nsITreeView> mTreeView;
  nsCOMPtr<nsIAccessible> mCaption;
  nsString mSummary;

  NS_IMETHOD ChangeSelection(PRInt32 aIndex, PRUint8 aMethod, PRBool *aSelState);
};

/**
  * Treeitems -- used in Trees
  */
class nsXULTreeitemAccessible : public nsLeafAccessible
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsXULTreeitemAccessible(nsIAccessible *aParent, nsIDOMNode *aDOMNode, nsIWeakReference *aShell, PRInt32 aRow, PRInt32 aColumn = -1);
  virtual ~nsXULTreeitemAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccName(nsAString& _retval);
  NS_IMETHOD GetAccValue(nsAString& _retval);
  NS_IMETHOD GetAccId(PRInt32 *_retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);
  NS_IMETHOD GetAccNumActions(PRUint8 *_retval);
  NS_IMETHOD GetAccActionName(PRUint8 index, nsAString& _retval);

  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);

  NS_IMETHOD AccDoAction(PRUint8 index);
  NS_IMETHOD AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height);
  NS_IMETHOD AccRemoveSelection(void); 
  NS_IMETHOD AccTakeSelection(void); 
  NS_IMETHOD AccTakeFocus(void); 

private:
  nsCOMPtr<nsITreeBoxObject> mTree;
  nsCOMPtr<nsITreeView> mTreeView;
  PRInt32  mRow, mColumnIndex;
  nsString mColumn;
};

class nsXULTreeColumnsAccessible : public nsAccessible,
                                   public nsIAccessibleTable
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLETABLE

  nsXULTreeColumnsAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsXULTreeColumnsAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);
  NS_IMETHOD GetAccNumActions(PRUint8 *_retval);
  NS_IMETHOD GetAccActionName(PRUint8 index, nsAString& _retval);

  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval); 
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval); 

  NS_IMETHOD AccDoAction(PRUint8 index);

private:
  nsCOMPtr<nsIAccessible> mCaption;
  nsString mSummary;
};

class nsXULTreeColumnitemAccessible : public nsLeafAccessible
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsXULTreeColumnitemAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsXULTreeColumnitemAccessible() {}

  /* ----- nsIAccessible ----- */
  NS_IMETHOD GetAccName(nsAString& _retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);
  NS_IMETHOD GetAccNumActions(PRUint8 *_retval);
  NS_IMETHOD GetAccActionName(PRUint8 index, nsAString& _retval);

  NS_IMETHOD AccDoAction(PRUint8 index);
};

#endif
