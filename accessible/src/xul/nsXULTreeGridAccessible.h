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
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#ifndef __nsXULTreeGridAccessible_h__
#define __nsXULTreeGridAccessible_h__

#include "nsIAccessibleTable.h"

#include "nsXULTreeAccessible.h"

/**
 * Represents accessible for XUL tree in the case when it has multiple columns.
 */
class nsXULTreeGridAccessible : public nsXULTreeAccessible,
                                public nsIAccessibleTable
{
public:
  nsXULTreeGridAccessible(nsIContent *aContent, nsIWeakReference *aShell);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTable
  NS_DECL_NSIACCESSIBLETABLE

  // nsAccessible
  virtual PRUint32 NativeRole();

protected:

  // nsXULTreeAccessible
  virtual already_AddRefed<nsAccessible> CreateTreeItemAccessible(PRInt32 aRow);
};


/**
 * Represents accessible for XUL tree item in the case when XUL tree has
 * multiple columns.
 */
class nsXULTreeGridRowAccessible : public nsXULTreeItemAccessibleBase
{
public:
  using nsAccessible::GetChildCount;
  using nsAccessible::GetChildAt;
  using nsAccessible::GetChildAtPoint;

  nsXULTreeGridRowAccessible(nsIContent *aContent, nsIWeakReference *aShell,
                             nsAccessible *aParent, nsITreeBoxObject *aTree,
                             nsITreeView *aTreeView, PRInt32 aRow);

  // nsISupports and cycle collection
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsXULTreeGridRowAccessible,
                                           nsAccessible)

  // nsAccessNode
  virtual void Shutdown();

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual nsresult GetChildAtPoint(PRInt32 aX, PRInt32 aY,
                                   PRBool aDeepestChild,
                                   nsIAccessible **aChild);

  virtual nsAccessible* GetChildAt(PRUint32 aIndex);
  virtual PRInt32 GetChildCount();

  // nsXULTreeItemAccessibleBase
  virtual nsAccessible* GetCellAccessible(nsITreeColumn *aColumn);
  virtual void RowInvalidated(PRInt32 aStartColIdx, PRInt32 aEndColIdx);

protected:

  // nsAccessible
  virtual void CacheChildren();

  // nsXULTreeItemAccessibleBase
  nsAccessibleHashtable mAccessibleCache;
};


/**
 * Represents an accessible for XUL tree cell in the case when XUL tree has
 * multiple columns.
 */

#define NS_XULTREEGRIDCELLACCESSIBLE_IMPL_CID         \
{  /* 84588ad4-549c-4196-a932-4c5ca5de5dff */         \
  0x84588ad4,                                         \
  0x549c,                                             \
  0x4196,                                             \
  { 0xa9, 0x32, 0x4c, 0x5c, 0xa5, 0xde, 0x5d, 0xff }  \
}

class nsXULTreeGridCellAccessible : public nsLeafAccessible,
                                    public nsIAccessibleTableCell
{
public:
  using nsAccessible::GetParent;

  nsXULTreeGridCellAccessible(nsIContent *aContent, nsIWeakReference *aShell,
                              nsXULTreeGridRowAccessible *aRowAcc,
                              nsITreeBoxObject *aTree, nsITreeView *aTreeView,
                              PRInt32 aRow, nsITreeColumn* aColumn);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetFocusedChild(nsIAccessible **aFocusedChild);

  NS_IMETHOD GetName(nsAString& aName);
  NS_IMETHOD GetBounds(PRInt32 *aX, PRInt32 *aY,
                       PRInt32 *aWidth, PRInt32 *aHeight);

  NS_IMETHOD GetNumActions(PRUint8 *aCount);
  NS_IMETHOD GetActionName(PRUint8 aIndex, nsAString& aName);
  NS_IMETHOD DoAction(PRUint8 aIndex);

  // nsIAccessibleTableCell
  NS_DECL_NSIACCESSIBLETABLECELL

  // nsAccessNode
  virtual PRBool IsDefunct();
  virtual PRBool Init();
  virtual bool IsPrimaryForNode() const;

  // nsAccessible
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);
  virtual PRUint32 NativeRole();
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
  virtual PRInt32 GetIndexInParent() { return GetColumnIndex(); }

  // nsXULTreeGridCellAccessible
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_XULTREEGRIDCELLACCESSIBLE_IMPL_CID)

  /**
   * Return index of the column.
   */
  PRInt32 GetColumnIndex() const;

  /**
   * Fire name or state change event if the accessible text or value has been
   * changed.
   */
  void CellInvalidated();

protected:
  // nsAccessible
  virtual nsAccessible* GetSiblingAtOffset(PRInt32 aOffset,
                                           nsresult *aError = nsnull);
  virtual void DispatchClickEvent(nsIContent *aContent, PRUint32 aActionIndex);

  // nsXULTreeGridCellAccessible

  /**
   * Return true if value of cell can be modified.
   */
  PRBool IsEditable() const;

  enum { eAction_Click = 0 };

  nsCOMPtr<nsITreeBoxObject> mTree;
  nsCOMPtr<nsITreeView> mTreeView;

  PRInt32 mRow;
  nsCOMPtr<nsITreeColumn> mColumn;

  nsString mCachedTextEquiv;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsXULTreeGridCellAccessible,
                              NS_XULTREEGRIDCELLACCESSIBLE_IMPL_CID)

#endif
