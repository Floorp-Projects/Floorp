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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Hyatt <hyatt@mozilla.org> (Original Author)
 *   Jan Varga <varga@ku.sk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsIDOMElement.h"
#include "nsIBoxObject.h"
#include "nsIDocument.h"
#include "nsTreeColumns.h"
#include "nsTreeUtils.h"
#include "nsStyleContext.h"
#include "nsIDOMClassInfo.h"
#include "nsINodeInfo.h"

// Column class that caches all the info about our column.
nsTreeColumn::nsTreeColumn(nsTreeColumns* aColumns, nsIFrame* aFrame)
  : mFrame(aFrame),
    mColumns(aColumns),
    mNext(nsnull),
    mPrevious(nsnull)
{
  CacheAttributes();
}

nsTreeColumn::~nsTreeColumn()
{
  if (mNext) {
    mNext->SetPrevious(nsnull);
    NS_RELEASE(mNext);
  }
}

// QueryInterface implementation for nsTreeColumn
NS_INTERFACE_MAP_BEGIN(nsTreeColumn)
  NS_INTERFACE_MAP_ENTRY(nsITreeColumn)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_DOM_CLASSINFO(TreeColumn)
NS_INTERFACE_MAP_END
                                                                                
NS_IMPL_ADDREF(nsTreeColumn)
NS_IMPL_RELEASE(nsTreeColumn)

NS_IMETHODIMP
nsTreeColumn::GetElement(nsIDOMElement** aElement)
{
  return CallQueryInterface(GetContent(), aElement);
}

NS_IMETHODIMP
nsTreeColumn::GetColumns(nsITreeColumns** aColumns)
{
  NS_IF_ADDREF(*aColumns = mColumns);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetX(PRInt32* aX)
{
  float t2p = mFrame->GetPresContext()->TwipsToPixels();
  *aX = NSToIntRound(GetX() * t2p);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetWidth(PRInt32* aWidth)
{
  float t2p = mFrame->GetPresContext()->TwipsToPixels();
  *aWidth = NSToIntRound(GetWidth() * t2p);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetId(nsAString& aId)
{
  aId = GetId();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetIdConst(const PRUnichar** aIdConst)
{
  *aIdConst = mId.get();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetAtom(nsIAtom** aAtom)
{
  NS_IF_ADDREF(*aAtom = GetAtom());
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetIndex(PRInt32* aIndex)
{
  *aIndex = GetIndex();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetPrimary(PRBool* aPrimary)
{
  *aPrimary = IsPrimary();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetCycler(PRBool* aCycler)
{
  *aCycler = IsCycler();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetEditable(PRBool* aEditable)
{
  *aEditable = IsEditable();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetType(PRInt16* aType)
{
  *aType = GetType();
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetNext(nsITreeColumn** _retval)
{
  NS_IF_ADDREF(*_retval = GetNext());
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::GetPrevious(nsITreeColumn** _retval)
{
  NS_IF_ADDREF(*_retval = GetPrevious());
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumn::Invalidate()
{
  CacheAttributes();
  return NS_OK;
}

void
nsTreeColumn::CacheAttributes()
{
  nsIContent* content = GetContent();

  // Fetch the Id.
  content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, mId);

  // If we have an Id, cache the Id as an atom.
  if (!mId.IsEmpty()) {
    mAtom = do_GetAtom(mId);
  }

  // Cache our index.
  nsTreeUtils::GetColumnIndex(content, &mIndex);

  const nsStyleVisibility* vis = mFrame->GetStyleVisibility();

  // Cache our text alignment policy.
  const nsStyleText* textStyle = mFrame->GetStyleText();

  mTextAlignment = textStyle->mTextAlign;
  if (mTextAlignment == 0 || mTextAlignment == 2) { // Left or Right
    if (vis->mDirection == NS_STYLE_DIRECTION_RTL)
      mTextAlignment = 2 - mTextAlignment; // Right becomes left, left becomes right.
  }

  // Figure out if we're the primary column (that has to have indentation
  // and twisties drawn.
  mIsPrimary = PR_FALSE;
  nsAutoString primary;
  content->GetAttr(kNameSpaceID_None, nsXULAtoms::primary, primary);
  if (primary.EqualsLiteral("true"))
    mIsPrimary = PR_TRUE;

  // Figure out if we're a cycling column (one that doesn't cause a selection
  // to happen).
  mIsCycler = PR_FALSE;
  nsAutoString cycler;
  content->GetAttr(kNameSpaceID_None, nsXULAtoms::cycler, cycler);
  if (cycler.EqualsLiteral("true"))
    mIsCycler = PR_TRUE;

  mIsEditable = PR_FALSE;
  nsAutoString editable;
  content->GetAttr(kNameSpaceID_None, nsXULAtoms::editable, editable);
  if (editable.EqualsLiteral("true"))
    mIsEditable = PR_TRUE;

  // Figure out our column type. Default type is text.
  mType = nsITreeColumn::TYPE_TEXT;
  nsAutoString type;
  content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, type);
  if (type.EqualsLiteral("checkbox"))
    mType = nsITreeColumn::TYPE_CHECKBOX;
  else if (type.EqualsLiteral("progressmeter"))
    mType = nsITreeColumn::TYPE_PROGRESSMETER;

  // Fetch the crop style.
  mCropStyle = 0;
  nsAutoString crop;
  content->GetAttr(kNameSpaceID_None, nsXULAtoms::crop, crop);
  if (crop.EqualsLiteral("center"))
    mCropStyle = 1;
  else if (crop.EqualsLiteral("left") ||
           crop.EqualsLiteral("start"))
    mCropStyle = 2;
}


nsTreeColumns::nsTreeColumns(nsITreeBoxObject* aTree)
  : mTree(aTree),
    mFirstColumn(nsnull),
    mDirty(PR_TRUE)
{
}

nsTreeColumns::~nsTreeColumns()
{
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    currCol->SetColumns(nsnull);
  }
  NS_IF_RELEASE(mFirstColumn);
}

// QueryInterface implementation for nsTreeColumns
NS_INTERFACE_MAP_BEGIN(nsTreeColumns)
  NS_INTERFACE_MAP_ENTRY(nsITreeColumns)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_DOM_CLASSINFO(TreeColumns)
NS_INTERFACE_MAP_END
                                                                                
NS_IMPL_ADDREF(nsTreeColumns)
NS_IMPL_RELEASE(nsTreeColumns)

NS_IMETHODIMP
nsTreeColumns::GetTree(nsITreeBoxObject** _retval)
{
  NS_IF_ADDREF(*_retval = mTree);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetCount(PRInt32* _retval)
{
  EnsureColumns();
  *_retval = 0;
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    ++(*_retval);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetFirstColumn(nsITreeColumn** _retval)
{
  NS_IF_ADDREF(*_retval = GetFirstColumn());
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetLastColumn(nsITreeColumn** _retval)
{
  EnsureColumns();
  *_retval = nsnull;
  nsTreeColumn* currCol = mFirstColumn;
  while (currCol) {
    nsTreeColumn* next = currCol->GetNext();
    if (!next) {
      NS_ADDREF(*_retval = currCol);
      break;
    }
    currCol = next;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetPrimaryColumn(nsITreeColumn** _retval)
{
  NS_IF_ADDREF(*_retval = GetPrimaryColumn());
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetSortedColumn(nsITreeColumn** _retval)
{
  EnsureColumns();
  *_retval = nsnull;
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    nsAutoString attr;
    currCol->GetContent()->GetAttr(kNameSpaceID_None, nsXULAtoms::sortDirection, attr);
    if (!attr.IsEmpty()) {
      NS_ADDREF(*_retval = currCol);
      return NS_OK;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetKeyColumn(nsITreeColumn** _retval)
{
  EnsureColumns();
  *_retval = nsnull;

  nsTreeColumn* first;
  nsTreeColumn* primary;
  nsTreeColumn* sorted;
  first = primary = sorted = nsnull;

  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    nsIContent* content = currCol->GetContent();

    // Skip hidden columns.
    nsAutoString attr;
    content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::hidden, attr);
    if (attr.EqualsLiteral("true"))
      continue;

    // Skip non-text column
    if (currCol->GetType() != nsITreeColumn::TYPE_TEXT)
      continue;

    if (!first)
      first = currCol;
    
    content->GetAttr(kNameSpaceID_None, nsXULAtoms::sortDirection, attr);
    if (!attr.IsEmpty()) {
      // Use sorted column as the key.
      sorted = currCol;
      break;
    }

    if (currCol->IsPrimary())
      if (!primary)
        primary = currCol;
  }

  if (sorted)
    *_retval = sorted;
  else if (primary)
    *_retval = primary;
  else
    *_retval = first;

  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetColumnFor(nsIDOMElement* aElement, nsITreeColumn** _retval)
{
  EnsureColumns();
  *_retval = nsnull;
  nsCOMPtr<nsIContent> element = do_QueryInterface(aElement);
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    if (currCol->GetContent() == element) {
      NS_ADDREF(*_retval = currCol);
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetNamedColumn(const nsAString& aId, nsITreeColumn** _retval)
{
  EnsureColumns();
  *_retval = nsnull;
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    if (currCol->GetId().Equals(aId)) {
      NS_ADDREF(*_retval = currCol);
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::GetColumnAt(PRInt32 aIndex, nsITreeColumn** _retval)
{
  EnsureColumns();
  *_retval = nsnull;
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    if (currCol->GetIndex() == aIndex) {
      NS_ADDREF(*_retval = currCol);
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::InvalidateColumns()
{
  mDirty = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsTreeColumns::RestoreNaturalOrder()
{
  if (!mTree)
    return NS_OK;

  nsCOMPtr<nsIBoxObject> boxObject = do_QueryInterface(mTree);
  nsCOMPtr<nsIDOMElement> element;
  boxObject->GetElement(getter_AddRefs(element));
  nsCOMPtr<nsIContent> content = do_QueryInterface(element);

  nsCOMPtr<nsIContent> colsContent;
  nsTreeUtils::GetImmediateChild(content, nsXULAtoms::treecols, getter_AddRefs(colsContent));
  if (!colsContent)
    return NS_OK;

  PRUint32 numChildren = colsContent->GetChildCount();
  for (PRUint32 i = 0; i < numChildren; ++i) {
    nsIContent *child = colsContent->GetChildAt(i);
    nsAutoString ordinal;
    ordinal.AppendInt(i);
    child->SetAttr(kNameSpaceID_None, nsXULAtoms::ordinal, ordinal, PR_TRUE);
  }

  mDirty = PR_TRUE;

  mTree->Invalidate();

  return NS_OK;
}

nsTreeColumn*
nsTreeColumns::GetPrimaryColumn()
{
  EnsureColumns();
  for (nsTreeColumn* currCol = mFirstColumn; currCol; currCol = currCol->GetNext()) {
    if (currCol->IsPrimary()) {
      return currCol;
    }
  }
  return nsnull;
}

void
nsTreeColumns::EnsureColumns()
{
  if (mTree && (!mFirstColumn || mDirty)) {
    nsCOMPtr<nsIBoxObject> boxObject = do_QueryInterface(mTree);
    nsCOMPtr<nsIDOMElement> treeElement;
    boxObject->GetElement(getter_AddRefs(treeElement));
    nsCOMPtr<nsIContent> treeContent = do_QueryInterface(treeElement);

    nsCOMPtr<nsIContent> colsContent;
    nsTreeUtils::GetDescendantChild(treeContent, nsXULAtoms::treecols, getter_AddRefs(colsContent));
    if (!colsContent)
      return;

    nsCOMPtr<nsIDocument> document = treeContent->GetDocument();
    nsIPresShell *shell = document->GetShellAt(0);
    if (!shell)
      return;

    nsIFrame* colsFrame = nsnull;
    shell->GetPrimaryFrameFor(colsContent, &colsFrame);
    if (!colsFrame)
      return;

    nsIBox* colBox = nsnull;
    colsFrame->GetChildBox(&colBox);

    NS_IF_RELEASE(mFirstColumn);
    nsTreeColumn* currCol = nsnull;
    while (colBox) {
      nsIContent* colContent = colBox->GetContent();

      nsINodeInfo *ni = colContent->GetNodeInfo();
      if (ni && ni->Equals(nsXULAtoms::treecol, kNameSpaceID_XUL)) { 
        // Create a new column structure.
        nsTreeColumn* col = new nsTreeColumn(this, colBox);
        if (!col)
          return;

        if (currCol) {
          currCol->SetNext(col);
          col->SetPrevious(currCol);
        }
        else {
          NS_ADDREF(mFirstColumn = col);
        }
        currCol = col;
      }

      colBox->GetNextBox(&colBox);
    }

    mDirty = PR_FALSE;
  }
}
