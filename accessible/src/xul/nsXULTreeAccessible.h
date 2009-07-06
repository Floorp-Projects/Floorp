/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Author: Kyle Yuan (kyle.yuan@sun.com)
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
#ifndef __nsXULTreeAccessible_h__
#define __nsXULTreeAccessible_h__

#include "nsITreeBoxObject.h"
#include "nsITreeView.h"
#include "nsITreeColumns.h"
#include "nsXULSelectAccessible.h"

/*
 * A class the represents the XUL Tree widget.
 */
const PRUint32 kMaxTreeColumns = 100;
const PRUint32 kDefaultTreeCacheSize = 256;

/**
 * Accessible class for XUL tree element.
 */

#define NS_XULTREEACCESSIBLE_IMPL_CID                   \
{  /* 2692e149-6176-42ee-b8e1-2c44b04185e3 */           \
  0x2692e149,                                           \
  0x6176,                                               \
  0x42ee,                                               \
  { 0xb8, 0xe1, 0x2c, 0x44, 0xb0, 0x41, 0x85, 0xe3 }    \
}

class nsXULTreeAccessible : public nsXULSelectableAccessible
{
public:
  nsXULTreeAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsXULTreeAccessible() {}

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleSelectable
  NS_DECL_NSIACCESSIBLESELECTABLE

  // nsIAccessible
  NS_IMETHOD GetValue(nsAString& _retval);

  NS_IMETHOD GetFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetChildCount(PRInt32 *_retval);
  NS_IMETHOD GetFocusedChild(nsIAccessible **aFocusedChild);

  // nsAccessNode
  virtual PRBool IsDefunct();
  virtual nsresult Shutdown();

  // nsAccessible
  virtual nsresult GetRoleInternal(PRUint32 *aRole);
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
  virtual nsresult GetChildAtPoint(PRInt32 aX, PRInt32 aY,
                                   PRBool aDeepestChild,
                                   nsIAccessible **aChild);

  // nsXULTreeAccessible

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_XULTREEACCESSIBLE_IMPL_CID)

  /**
   * Return tree item accessible at the givem row and column. If accessible
   * doesn't exist in the cache then create it.
   *
   * @param aRow         [in] the given row index
   * @param aColumn      [in] the given column object. If is is nsnull then
   *                      primary column is used
   * @param aAccessible  [out] tree item accessible
   */
  void GetCachedTreeitemAccessible(PRInt32 aRow, nsITreeColumn *aColumn,
                                   nsIAccessible **aAccessible);

  /**
   * Invalidates the number of cached treeitem accessibles.
   *
   * @param aRow    [in] row index the invalidation starts from
   * @param aCount  [in] the number of treeitem accessibles to invalidate,
   *                 the number sign specifies whether rows have been
   *                 inserted (plus) or removed (minus)
   */
  void InvalidateCache(PRInt32 aRow, PRInt32 aCount);

  /**
   * Fires name change events for invalidated area of tree.
   *
   * @param aStartRow  [in] row index invalidation starts from
   * @param aEndRow    [in] row index invalidation ends, -1 means last row index
   * @param aStartCol  [in] column index invalidation starts from
   * @param aEndCol    [in] column index invalidation ends, -1 mens last column
   *                    index
   */
  void TreeViewInvalidated(PRInt32 aStartRow, PRInt32 aEndRow,
                           PRInt32 aStartCol, PRInt32 aEndCol);

  /**
   * Invalidates children created for previous tree view.
   */
  void TreeViewChanged();

  static void GetTreeBoxObject(nsIDOMNode* aDOMNode, nsITreeBoxObject** aBoxObject);
  static nsresult GetColumnCount(nsITreeBoxObject* aBoxObject, PRInt32 *aCount);

  static PRBool IsColumnHidden(nsITreeColumn *aColumn);
  static already_AddRefed<nsITreeColumn> GetNextVisibleColumn(nsITreeColumn *aColumn);
  static already_AddRefed<nsITreeColumn> GetFirstVisibleColumn(nsITreeBoxObject *aTree);
  static already_AddRefed<nsITreeColumn> GetLastVisibleColumn(nsITreeBoxObject *aTree);

protected:
  nsCOMPtr<nsITreeBoxObject> mTree;
  nsCOMPtr<nsITreeView> mTreeView;
  nsAccessNodeHashtable *mAccessNodeCache;

  NS_IMETHOD ChangeSelection(PRInt32 aIndex, PRUint8 aMethod, PRBool *aSelState);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsXULTreeAccessible,
                              NS_XULTREEACCESSIBLE_IMPL_CID)

/**
 * Accessible class for items for XUL tree.
 */

#define NS_XULTREEITEMACCESSIBLE_IMPL_CID             \
{  /* 7b1aa039-7270-4523-aeb3-61063a13ac3f */         \
  0x7b1aa039,                                         \
  0x7270,                                             \
  0x4523,                                             \
  { 0xae, 0xb3, 0x61, 0x06, 0x3a, 0x13, 0xac, 0x3f }  \
}

class nsXULTreeitemAccessible : public nsLeafAccessible
{
public:
  enum { eAction_Click = 0, eAction_Expand = 1 };

  nsXULTreeitemAccessible(nsIAccessible *aParent, nsIDOMNode *aDOMNode, nsIWeakReference *aShell, PRInt32 aRow, nsITreeColumn* aColumn = nsnull);
  virtual ~nsXULTreeitemAccessible() {}

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetName(nsAString& aName);
  NS_IMETHOD GetNumActions(PRUint8 *_retval);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);

  NS_IMETHOD GetParent(nsIAccessible **_retval);
  NS_IMETHOD GetNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetPreviousSibling(nsIAccessible **_retval);

  NS_IMETHOD DoAction(PRUint8 index);
  NS_IMETHOD GetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height);
  NS_IMETHOD SetSelected(PRBool aSelect); 
  NS_IMETHOD TakeFocus(void); 

  NS_IMETHOD GetRelationByType(PRUint32 aRelationType,
                               nsIAccessibleRelation **aRelation);

  // nsIAccessNode
  NS_IMETHOD GetUniqueID(void **aUniqueID);

  // nsAccessNode
  virtual PRBool IsDefunct();
  virtual nsresult Init();
  virtual nsresult Shutdown();

  // nsAccessible
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);
  virtual nsresult GetRoleInternal(PRUint32 *aRole);
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);

  // nsXULTreeitemAccessible
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_XULTREEITEMACCESSIBLE_IMPL_CID)

  /**
   * Get/set cached name.
   */
  void GetCachedName(nsAString& aName);
  void SetCachedName(const nsAString& aName);

protected:
  PRBool IsExpandable();
  nsCOMPtr<nsITreeBoxObject> mTree;
  nsCOMPtr<nsITreeView> mTreeView;
  PRInt32 mRow;
  nsCOMPtr<nsITreeColumn> mColumn;
  nsString mCachedName;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsXULTreeitemAccessible,
                              NS_XULTREEITEMACCESSIBLE_IMPL_CID)

/**
 * Accessible class for columns element of XUL tree.
 */
class nsXULTreeColumnsAccessible : public nsXULColumnsAccessible
{
public:
  nsXULTreeColumnsAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);

  // nsIAccessible
  NS_IMETHOD GetNextSibling(nsIAccessible **aNextSibling);
};

#endif
