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
 * The Original Code is Mozilla.org.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Glazman (glazman@netscape.com) (Original author)
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

#include "nsHTMLEditor.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIPresShell.h"
#include "nsIDocumentObserver.h"
#include "nsIContent.h"
#include "nsHTMLEditUtils.h"
#include "nsReadableUtils.h"

// Uncomment the following line if you want to disable
// table deletion when the only column/row is removed
// #define DISABLE_TABLE_DELETION 1

NS_IMETHODIMP
nsHTMLEditor::SetInlineTableEditingEnabled(PRBool aIsEnabled)
{
  mIsInlineTableEditingEnabled = aIsEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetInlineTableEditingEnabled(PRBool * aIsEnabled)
{
  *aIsEnabled = mIsInlineTableEditingEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::ShowInlineTableEditingUI(nsIDOMElement * aCell)
{
  NS_ENSURE_ARG_POINTER(aCell);

  // do nothing if aCell is not a table cell...
  if (!nsHTMLEditUtils::IsTableCell(aCell))
    return NS_OK;

  // the resizers and the shadow will be anonymous children of the body
  nsCOMPtr<nsIDOMElement> bodyElement;
  nsresult res = nsEditor::GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  if (!bodyElement)   return NS_ERROR_NULL_POINTER;

  CreateAnonymousElement(NS_LITERAL_STRING("a"), bodyElement,
                         NS_LITERAL_STRING("mozTableAddColumnBefore"),
                         PR_FALSE, getter_AddRefs(mAddColumnBeforeButton));
  CreateAnonymousElement(NS_LITERAL_STRING("a"), bodyElement,
                         NS_LITERAL_STRING("mozTableRemoveColumn"),
                         PR_FALSE, getter_AddRefs(mRemoveColumnButton));
  CreateAnonymousElement(NS_LITERAL_STRING("a"), bodyElement,
                         NS_LITERAL_STRING("mozTableAddColumnAfter"),
                         PR_FALSE, getter_AddRefs(mAddColumnAfterButton));

  CreateAnonymousElement(NS_LITERAL_STRING("a"), bodyElement,
                         NS_LITERAL_STRING("mozTableAddRowBefore"),
                         PR_FALSE, getter_AddRefs(mAddRowBeforeButton));
  CreateAnonymousElement(NS_LITERAL_STRING("a"), bodyElement,
                         NS_LITERAL_STRING("mozTableRemoveRow"),
                         PR_FALSE, getter_AddRefs(mRemoveRowButton));
  CreateAnonymousElement(NS_LITERAL_STRING("a"), bodyElement,
                         NS_LITERAL_STRING("mozTableAddRowAfter"),
                         PR_FALSE, getter_AddRefs(mAddRowAfterButton));

  AddMouseClickListener(mAddColumnBeforeButton);
  AddMouseClickListener(mRemoveColumnButton);
  AddMouseClickListener(mAddColumnAfterButton);
  AddMouseClickListener(mAddRowBeforeButton);
  AddMouseClickListener(mRemoveRowButton);
  AddMouseClickListener(mAddRowAfterButton);

  mInlineEditedCell = aCell;
  return RefreshInlineTableEditingUI();
}

NS_IMETHODIMP
nsHTMLEditor::HideInlineTableEditingUI()
{
  mInlineEditedCell = nsnull;

  RemoveMouseClickListener(mAddColumnBeforeButton);
  RemoveMouseClickListener(mRemoveColumnButton);
  RemoveMouseClickListener(mAddColumnAfterButton);
  RemoveMouseClickListener(mAddRowBeforeButton);
  RemoveMouseClickListener(mRemoveRowButton);
  RemoveMouseClickListener(mAddRowAfterButton);

  // get the presshell's document observer interface.
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDocumentObserver> docObserver(do_QueryInterface(ps));
  if (!docObserver) return NS_ERROR_FAILURE;

  // get the root content node.

  nsCOMPtr<nsIDOMElement> bodyElement;
  nsresult res = nsEditor::GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIContent> bodyContent( do_QueryInterface(bodyElement) );
  if (!bodyContent) return NS_ERROR_FAILURE;

  DeleteRefToAnonymousNode(mAddColumnBeforeButton, bodyContent, docObserver);
  mAddColumnBeforeButton = nsnull;
  DeleteRefToAnonymousNode(mRemoveColumnButton, bodyContent, docObserver);
  mRemoveColumnButton = nsnull;
  DeleteRefToAnonymousNode(mAddColumnAfterButton, bodyContent, docObserver);
  mAddColumnAfterButton = nsnull;
  DeleteRefToAnonymousNode(mAddRowBeforeButton, bodyContent, docObserver);
  mAddRowBeforeButton = nsnull;
  DeleteRefToAnonymousNode(mRemoveRowButton, bodyContent, docObserver);
  mRemoveRowButton = nsnull;
  DeleteRefToAnonymousNode(mAddRowAfterButton, bodyContent, docObserver);
  mAddRowAfterButton = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::DoInlineTableEditingAction(nsIDOMElement * aElement)
{
  NS_ENSURE_ARG_POINTER(aElement);
  PRBool anonElement = PR_FALSE;
  if (aElement &&
      NS_SUCCEEDED(aElement->HasAttribute(NS_LITERAL_STRING("_moz_anonclass"), &anonElement)) &&
      anonElement) {
    nsAutoString anonclass;
    nsresult res = aElement->GetAttribute(NS_LITERAL_STRING("_moz_anonclass"), anonclass);
    if (NS_FAILED(res)) return res;

    if (!StringBeginsWith(anonclass, NS_LITERAL_STRING("mozTable")))
      return NS_OK;

    nsCOMPtr<nsIDOMNode> tableNode = GetEnclosingTable(mInlineEditedCell);
    nsCOMPtr<nsIDOMElement> tableElement = do_QueryInterface(tableNode);
    PRInt32 rowCount, colCount;
    res = GetTableSize(tableElement, &rowCount, &colCount);
    if (NS_FAILED(res)) return res;

    PRBool hideUI = PR_FALSE;
    PRBool hideResizersWithInlineTableUI = (mResizedObject == tableElement);

    if (anonclass.EqualsLiteral("mozTableAddColumnBefore"))
      InsertTableColumn(1, PR_FALSE);
    else if (anonclass.EqualsLiteral("mozTableAddColumnAfter"))
      InsertTableColumn(1, PR_TRUE);
    else if (anonclass.EqualsLiteral("mozTableAddRowBefore"))
      InsertTableRow(1, PR_FALSE);
    else if (anonclass.EqualsLiteral("mozTableAddRowAfter"))
      InsertTableRow(1, PR_TRUE);
    else if (anonclass.EqualsLiteral("mozTableRemoveColumn")) {
      DeleteTableColumn(1);
#ifndef DISABLE_TABLE_DELETION
      hideUI = (colCount == 1);
#endif
    }
    else if (anonclass.EqualsLiteral("mozTableRemoveRow")) {
      DeleteTableRow(1);
#ifndef DISABLE_TABLE_DELETION
      hideUI = (rowCount == 1);
#endif
    }
    else
      return NS_OK;

    if (hideUI) {
      HideInlineTableEditingUI();
      if (hideResizersWithInlineTableUI)
        HideResizers();
    }
  }

  return NS_OK;
}

void
nsHTMLEditor::AddMouseClickListener(nsIDOMElement * aElement)
{
  nsCOMPtr<nsIDOMEventTarget> evtTarget(do_QueryInterface(aElement));
  if (evtTarget)
    evtTarget->AddEventListener(NS_LITERAL_STRING("click"), mMouseListenerP, PR_TRUE);
}

void
nsHTMLEditor::RemoveMouseClickListener(nsIDOMElement * aElement)
{
  nsCOMPtr<nsIDOMEventTarget> evtTarget(do_QueryInterface(aElement));
  if (evtTarget)
    evtTarget->RemoveEventListener(NS_LITERAL_STRING("click"), mMouseListenerP, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLEditor::RefreshInlineTableEditingUI()
{
  nsCOMPtr<nsIDOMNSHTMLElement> nsElement = do_QueryInterface(mInlineEditedCell);
  if (!nsElement) {return NS_ERROR_NULL_POINTER; }

  PRInt32 xCell, yCell, wCell, hCell;
  GetElementOrigin(mInlineEditedCell, xCell, yCell);

  nsresult res = nsElement->GetOffsetWidth(&wCell);
  if (NS_FAILED(res)) return res;
  res = nsElement->GetOffsetHeight(&hCell);
  if (NS_FAILED(res)) return res;

  PRInt32 xHoriz = xCell + wCell/2;
  PRInt32 yVert  = yCell + hCell/2;

  nsCOMPtr<nsIDOMNode> tableNode = GetEnclosingTable(mInlineEditedCell);
  nsCOMPtr<nsIDOMElement> tableElement = do_QueryInterface(tableNode);
  PRInt32 rowCount, colCount;
  res = GetTableSize(tableElement, &rowCount, &colCount);
  if (NS_FAILED(res)) return res;

  SetAnonymousElementPosition(xHoriz-10, yCell-7,  mAddColumnBeforeButton);
#ifdef DISABLE_TABLE_DELETION
  NS_NAMED_LITERAL_STRING(classStr, "class");

  if (colCount== 1) {
    mRemoveColumnButton->SetAttribute(classStr,
                                      NS_LITERAL_STRING("hidden"));
  }
  else {
    PRBool hasClass = PR_FALSE;
    res = mRemoveColumnButton->HasAttribute(classStr, &hasClass);
    if (NS_SUCCEEDED(res) && hasClass)
      mRemoveColumnButton->RemoveAttribute(classStr);
#endif
    SetAnonymousElementPosition(xHoriz-4, yCell-7,  mRemoveColumnButton);
#ifdef DISABLE_TABLE_DELETION
  }
#endif
  SetAnonymousElementPosition(xHoriz+6, yCell-7,  mAddColumnAfterButton);

  SetAnonymousElementPosition(xCell-7, yVert-10,  mAddRowBeforeButton);
#ifdef DISABLE_TABLE_DELETION
  if (rowCount== 1) {
    mRemoveRowButton->SetAttribute(classStr,
                                   NS_LITERAL_STRING("hidden"));
  }
  else {
    PRBool hasClass = PR_FALSE;
    res = mRemoveRowButton->HasAttribute(classStr, &hasClass);
    if (NS_SUCCEEDED(res) && hasClass)
      mRemoveRowButton->RemoveAttribute(classStr);
#endif
    SetAnonymousElementPosition(xCell-7, yVert-4,  mRemoveRowButton);
#ifdef DISABLE_TABLE_DELETION
  }
#endif
  SetAnonymousElementPosition(xCell-7, yVert+6,  mAddRowAfterButton);

  return NS_OK;
}

