/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Corp.
 * Portions created by Netscape Corp.are Copyright (C) 2003 Netscape
 * Corp. All Rights Reserved.
 *
 * Original Author: Aaron Leventhal
 *
 * Contributor(s):
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

#include "nsXULTreeAccessibleWrap.h"
#include "nsTextFormatter.h"

// --------------------------------------------------------
// nsXULTreeitemAccessibleWrap Accessible
// --------------------------------------------------------

nsXULTreeitemAccessibleWrap::nsXULTreeitemAccessibleWrap(nsIAccessible *aParent, 
                                                         nsIDOMNode *aDOMNode, 
                                                         nsIWeakReference *aShell, 
                                                         PRInt32 aRow, 
                                                         nsITreeColumn* aColumn) :
nsXULTreeitemAccessible(aParent, aDOMNode, aShell, aRow, aColumn)
{
}

NS_IMETHODIMP nsXULTreeitemAccessibleWrap::GetDescription(nsAString& aDescription)
{
  if (!mParent || !mWeakShell || !mTreeView) {
    return NS_ERROR_FAILURE;
  }

  aDescription.Truncate();

  PRInt32 level = 0;
  mTreeView->GetLevel(mRow, &level);

  PRInt32 testRow = -1;
  if (level > 0) {
    mTreeView->GetParentIndex(mRow, &testRow);
  }

  PRInt32 numRows;
  mTreeView->GetRowCount(&numRows);

  PRInt32 indexInParent = 0, numSiblings = 0;

  while (++ testRow < numRows) {
    PRInt32 testLevel;
    mTreeView->GetLevel(testRow, &testLevel);
    if (testLevel == level) {
      if (testRow <= mRow) {
        ++indexInParent;
      }
      ++numSiblings;
    }
    else if (testLevel < level) {
      break;
    }
  }

  nsTextFormatter::ssprintf(aDescription, L"L%d, %d of %d", level + 1, indexInParent, numSiblings);

  return NS_OK;
}
