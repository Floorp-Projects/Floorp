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
#include "nsXULListboxAccessible.h"

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
  using nsAccessible::GetChildAtPoint;

  nsXULTreeAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);

  // nsISupports and cycle collection
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsXULTreeAccessible,
                                           nsAccessible)

  // nsIAccessible
  NS_IMETHOD GetValue(nsAString& aValue);

  NS_IMETHOD GetFirstChild(nsIAccessible **aFirstChild);
  NS_IMETHOD GetLastChild(nsIAccessible **aLastChild);
  NS_IMETHOD GetChildCount(PRInt32 *aChildCount);
  NS_IMETHOD GetChildAt(PRInt32 aChildIndex, nsIAccessible **aChild);

  NS_IMETHOD GetFocusedChild(nsIAccessible **aFocusedChild);

  // nsIAccessibleSelectable
  NS_DECL_NSIACCESSIBLESELECTABLE

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
   * Return tree item accessible at the givem row. If accessible doesn't exist
   * in the cache then create and cache it.
   *
   * @param aRow         [in] the given row index
   * @param aAccessible  [out] tree item accessible
   */
  void GetTreeItemAccessible(PRInt32 aRow, nsIAccessible **aAccessible);

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

protected:
  /**
   * Creates tree item accessible for the given row index.
   */
  virtual void CreateTreeItemAccessible(PRInt32 aRowIndex,
                                        nsAccessNode** aAccessNode);

  nsCOMPtr<nsITreeBoxObject> mTree;
  nsCOMPtr<nsITreeView> mTreeView;
  nsAccessNodeHashtable mAccessNodeCache;

  NS_IMETHOD ChangeSelection(PRInt32 aIndex, PRUint8 aMethod, PRBool *aSelState);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsXULTreeAccessible,
                              NS_XULTREEACCESSIBLE_IMPL_CID)

/**
 * Base class for tree item accessibles.
 */

#define NS_XULTREEITEMBASEACCESSIBLE_IMPL_CID         \
{  /* 1ab79ae7-766a-443c-940b-b1e6b0831dfc */         \
  0x1ab79ae7,                                         \
  0x766a,                                             \
  0x443c,                                             \
  { 0x94, 0x0b, 0xb1, 0xe6, 0xb0, 0x83, 0x1d, 0xfc }  \
}

class nsXULTreeItemAccessibleBase : public nsAccessibleWrap
{
public:
  nsXULTreeItemAccessibleBase(nsIDOMNode *aDOMNode, nsIWeakReference *aShell,
                              nsIAccessible *aParent, nsITreeBoxObject *aTree,
                              nsITreeView *aTreeView, PRInt32 aRow);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessNode
  NS_IMETHOD GetUniqueID(void **aUniqueID);

  // nsIAccessible
  NS_IMETHOD GetParent(nsIAccessible **aParent);
  NS_IMETHOD GetNextSibling(nsIAccessible **aNextSibling);
  NS_IMETHOD GetPreviousSibling(nsIAccessible **aPreviousSibling);

  NS_IMETHOD GetFocusedChild(nsIAccessible **aFocusedChild);

  NS_IMETHOD GetBounds(PRInt32 *aX, PRInt32 *aY,
                       PRInt32 *aWidth, PRInt32 *aHeight);

  NS_IMETHOD SetSelected(PRBool aSelect); 
  NS_IMETHOD TakeFocus();

  NS_IMETHOD GetRelationByType(PRUint32 aRelationType,
                               nsIAccessibleRelation **aRelation);

  NS_IMETHOD GetNumActions(PRUint8 *aCount);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsAccessNode
  virtual PRBool IsDefunct();
  virtual nsresult Shutdown();

  // nsAccessible
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);

  // nsXULTreeItemAccessibleBase
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_XULTREEITEMBASEACCESSIBLE_IMPL_CID)

  /**
   * Return cell accessible for the given column. If XUL tree accessible is not
   * accessible table then return null.
   */
  virtual void GetCellAccessible(nsITreeColumn *aColumn,
                                 nsIAccessible **aCellAcc)
    { *aCellAcc = nsnull; }

  /**
   * Proccess row invalidation. Used to fires name change events.
   */
  virtual void RowInvalidated(PRInt32 aStartColIdx, PRInt32 aEndColIdx) = 0;

protected:
  enum { eAction_Click = 0, eAction_Expand = 1 };

  // nsAccessible
  virtual void DispatchClickEvent(nsIContent *aContent, PRUint32 aActionIndex);

  // nsXULTreeItemAccessibleBase

  /**
   * Return true if the tree item accessible is expandable (contains subrows).
   */
  PRBool IsExpandable();

  nsCOMPtr<nsITreeBoxObject> mTree;
  nsCOMPtr<nsITreeView> mTreeView;
  PRInt32 mRow;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsXULTreeItemAccessibleBase,
                              NS_XULTREEITEMBASEACCESSIBLE_IMPL_CID)


/**
 * Accessible class for items for XUL tree.
 */
class nsXULTreeItemAccessible : public nsXULTreeItemAccessibleBase
{
public:
  nsXULTreeItemAccessible(nsIDOMNode *aDOMNode, nsIWeakReference *aShell,
                          nsIAccessible *aParent, nsITreeBoxObject *aTree,
                          nsITreeView *aTreeView, PRInt32 aRow);

  // nsIAccessible
  NS_IMETHOD GetFirstChild(nsIAccessible **aFirstChild);
  NS_IMETHOD GetLastChild(nsIAccessible **aLastChild);
  NS_IMETHOD GetChildCount(PRInt32 *aChildCount);

  NS_IMETHOD GetName(nsAString& aName);

  // nsAccessNode
  virtual PRBool IsDefunct();
  virtual nsresult Init();
  virtual nsresult Shutdown();

  // nsAccessible
  virtual nsresult GetRoleInternal(PRUint32 *aRole);

  // nsXULTreeItemAccessibleBase
  virtual void RowInvalidated(PRInt32 aStartColIdx, PRInt32 aEndColIdx);

protected:
  nsCOMPtr<nsITreeColumn> mColumn;
  nsString mCachedName;
};


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
