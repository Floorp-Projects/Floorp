/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_A11Y_ARIAGridAccessible_h_
#define MOZILLA_A11Y_ARIAGridAccessible_h_

#include "nsIAccessibleTable.h"

#include "HyperTextAccessibleWrap.h"
#include "TableAccessible.h"
#include "xpcAccessibleTable.h"

namespace mozilla {
namespace a11y {

/**
 * Accessible for ARIA grid and treegrid.
 */
class ARIAGridAccessible : public AccessibleWrap,
                           public xpcAccessibleTable,
                           public nsIAccessibleTable,
                           public TableAccessible
{
public:
  ARIAGridAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTable
  NS_DECL_OR_FORWARD_NSIACCESSIBLETABLE_WITH_XPCACCESSIBLETABLE

  // Accessible
  virtual mozilla::a11y::TableAccessible* AsTable() { return this; }

  // nsAccessNode
  virtual void Shutdown();

  // TableAccessible
  virtual PRUint32 ColCount();
  virtual PRUint32 RowCount();
  virtual Accessible* CellAt(PRUint32 aRowIndex, PRUint32 aColumnIndex);
  virtual void SelectCol(PRUint32 aColIdx);
  virtual void SelectRow(PRUint32 aRowIdx);
  virtual void UnselectCol(PRUint32 aColIdx);
  virtual void UnselectRow(PRUint32 aRowIdx);

protected:
  /**
   * Return true if the given row index is valid.
   */
  bool IsValidRow(PRInt32 aRow);

  /**
   * Retrn true if the given column index is valid.
   */
  bool IsValidColumn(PRInt32 aColumn);

  /**
   * Return row accessible at the given row index.
   */
  Accessible* GetRowAt(PRInt32 aRow);

  /**
   * Return cell accessible at the given column index in the row.
   */
  Accessible* GetCellInRowAt(Accessible* aRow, PRInt32 aColumn);

  /**
   * Set aria-selected attribute value on DOM node of the given accessible.
   *
   * @param  aAccessible  [in] accessible
   * @param  aIsSelected  [in] new value of aria-selected attribute
   * @param  aNotify      [in, optional] specifies if DOM should be notified
   *                       about attribute change (used internally).
   */
  nsresult SetARIASelected(Accessible* aAccessible, bool aIsSelected,
                           bool aNotify = true);

  /**
   * Helper method for GetSelectedColumnCount and GetSelectedColumns.
   */
  nsresult GetSelectedColumnsArray(PRUint32 *acolumnCount,
                                   PRInt32 **aColumns = nsnull);
};


/**
 * Accessible for ARIA gridcell and rowheader/columnheader.
 */
class ARIAGridCellAccessible : public HyperTextAccessibleWrap,
                               public nsIAccessibleTableCell
{
public:
  ARIAGridCellAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTableCell
  NS_DECL_NSIACCESSIBLETABLECELL

  // Accessible
  virtual void ApplyARIAState(PRUint64* aState) const;
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);
};

} // namespace a11y
} // namespace mozilla

#endif
