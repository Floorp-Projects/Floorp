/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditor.h"

#include "HTMLEditUtils.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Element.h"
#include "nsAString.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGenericHTMLElement.h"
#include "nsIContent.h"
#include "nsIHTMLObjectResizer.h"
#include "nsLiteralString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nscore.h"

namespace mozilla {

NS_IMETHODIMP
HTMLEditor::SetInlineTableEditingEnabled(bool aIsEnabled) {
  EnableInlineTableEditor(aIsEnabled);
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetInlineTableEditingEnabled(bool* aIsEnabled) {
  *aIsEnabled = IsInlineTableEditorEnabled();
  return NS_OK;
}

nsresult HTMLEditor::ShowInlineTableEditingUIInternal(Element& aCellElement) {
  if (NS_WARN_IF(!HTMLEditUtils::IsTableCell(&aCellElement))) {
    return NS_OK;
  }

  if (NS_WARN_IF(!IsDescendantOfEditorRoot(&aCellElement))) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(mInlineEditedCell)) {
    return NS_ERROR_FAILURE;
  }

  mInlineEditedCell = &aCellElement;

  // the resizers and the shadow will be anonymous children of the body
  RefPtr<Element> bodyElement = GetRoot();
  if (NS_WARN_IF(!bodyElement)) {
    return NS_ERROR_FAILURE;
  }

  do {
    // The buttons of inline table editor will be children of the <body>
    // element.  Creating the anonymous elements may cause calling
    // HideInlineTableEditingUIInternal() via a mutation event listener.
    // So, we should store new button to a local variable, then, check:
    //   - whether creating a button is already set to the member or not
    //   - whether already created buttons are changed to another set
    // If creating the buttons are canceled, we hit the latter check.
    // If buttons for another table are created during this, we hit the latter
    // check too.
    // If buttons are just created again for same element, we hit the former
    // check.
    ManualNACPtr addColumnBeforeButton = CreateAnonymousElement(
        nsGkAtoms::a, *bodyElement,
        NS_LITERAL_STRING("mozTableAddColumnBefore"), false);
    if (NS_WARN_IF(!addColumnBeforeButton)) {
      break;  // Hide unnecessary buttons created above.
    }
    if (NS_WARN_IF(mAddColumnBeforeButton) ||
        NS_WARN_IF(mInlineEditedCell != &aCellElement)) {
      return NS_ERROR_FAILURE;  // Don't hide another set of buttons.
    }
    mAddColumnBeforeButton = std::move(addColumnBeforeButton);

    ManualNACPtr removeColumnButton = CreateAnonymousElement(
        nsGkAtoms::a, *bodyElement, NS_LITERAL_STRING("mozTableRemoveColumn"),
        false);
    if (NS_WARN_IF(!removeColumnButton)) {
      break;
    }
    if (NS_WARN_IF(mRemoveColumnButton) ||
        NS_WARN_IF(mInlineEditedCell != &aCellElement)) {
      return NS_ERROR_FAILURE;
    }
    mRemoveColumnButton = std::move(removeColumnButton);

    ManualNACPtr addColumnAfterButton = CreateAnonymousElement(
        nsGkAtoms::a, *bodyElement, NS_LITERAL_STRING("mozTableAddColumnAfter"),
        false);
    if (NS_WARN_IF(!addColumnAfterButton)) {
      break;
    }
    if (NS_WARN_IF(mAddColumnAfterButton) ||
        NS_WARN_IF(mInlineEditedCell != &aCellElement)) {
      return NS_ERROR_FAILURE;
    }
    mAddColumnAfterButton = std::move(addColumnAfterButton);

    ManualNACPtr addRowBeforeButton = CreateAnonymousElement(
        nsGkAtoms::a, *bodyElement, NS_LITERAL_STRING("mozTableAddRowBefore"),
        false);
    if (NS_WARN_IF(!addRowBeforeButton)) {
      break;
    }
    if (NS_WARN_IF(mAddRowBeforeButton) ||
        NS_WARN_IF(mInlineEditedCell != &aCellElement)) {
      return NS_ERROR_FAILURE;
    }
    mAddRowBeforeButton = std::move(addRowBeforeButton);

    ManualNACPtr removeRowButton =
        CreateAnonymousElement(nsGkAtoms::a, *bodyElement,
                               NS_LITERAL_STRING("mozTableRemoveRow"), false);
    if (NS_WARN_IF(!removeRowButton)) {
      break;
    }
    if (NS_WARN_IF(mRemoveRowButton) ||
        NS_WARN_IF(mInlineEditedCell != &aCellElement)) {
      return NS_ERROR_FAILURE;
    }
    mRemoveRowButton = std::move(removeRowButton);

    ManualNACPtr addRowAfterButton =
        CreateAnonymousElement(nsGkAtoms::a, *bodyElement,
                               NS_LITERAL_STRING("mozTableAddRowAfter"), false);
    if (NS_WARN_IF(!addRowAfterButton)) {
      break;
    }
    if (NS_WARN_IF(mAddRowAfterButton) ||
        NS_WARN_IF(mInlineEditedCell != &aCellElement)) {
      return NS_ERROR_FAILURE;
    }
    mAddRowAfterButton = std::move(addRowAfterButton);

    AddMouseClickListener(mAddColumnBeforeButton);
    AddMouseClickListener(mRemoveColumnButton);
    AddMouseClickListener(mAddColumnAfterButton);
    AddMouseClickListener(mAddRowBeforeButton);
    AddMouseClickListener(mRemoveRowButton);
    AddMouseClickListener(mAddRowAfterButton);

    nsresult rv = RefreshInlineTableEditingUIInternal();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  } while (true);

  HideInlineTableEditingUIInternal();
  return NS_ERROR_FAILURE;
}

void HTMLEditor::HideInlineTableEditingUIInternal() {
  mInlineEditedCell = nullptr;

  RemoveMouseClickListener(mAddColumnBeforeButton);
  RemoveMouseClickListener(mRemoveColumnButton);
  RemoveMouseClickListener(mAddColumnAfterButton);
  RemoveMouseClickListener(mAddRowBeforeButton);
  RemoveMouseClickListener(mRemoveRowButton);
  RemoveMouseClickListener(mAddRowAfterButton);

  // get the presshell's document observer interface.
  RefPtr<PresShell> presShell = GetPresShell();
  // We allow the pres shell to be null; when it is, we presume there
  // are no document observers to notify, but we still want to
  // UnbindFromTree.

  // Calling DeleteRefToAnonymousNode() may cause showing the UI again.
  // Therefore, we should forget all anonymous contents first.
  // Otherwise, we could leak the old content because of overwritten by
  // ShowInlineTableEditingUIInternal().
  ManualNACPtr addColumnBeforeButton(std::move(mAddColumnBeforeButton));
  ManualNACPtr removeColumnButton(std::move(mRemoveColumnButton));
  ManualNACPtr addColumnAfterButton(std::move(mAddColumnAfterButton));
  ManualNACPtr addRowBeforeButton(std::move(mAddRowBeforeButton));
  ManualNACPtr removeRowButton(std::move(mRemoveRowButton));
  ManualNACPtr addRowAfterButton(std::move(mAddRowAfterButton));

  DeleteRefToAnonymousNode(std::move(addColumnBeforeButton), presShell);
  DeleteRefToAnonymousNode(std::move(removeColumnButton), presShell);
  DeleteRefToAnonymousNode(std::move(addColumnAfterButton), presShell);
  DeleteRefToAnonymousNode(std::move(addRowBeforeButton), presShell);
  DeleteRefToAnonymousNode(std::move(removeRowButton), presShell);
  DeleteRefToAnonymousNode(std::move(addRowAfterButton), presShell);
}

nsresult HTMLEditor::DoInlineTableEditingAction(const Element& aElement) {
  nsAutoString anonclass;
  aElement.GetAttr(kNameSpaceID_None, nsGkAtoms::_moz_anonclass, anonclass);

  if (!StringBeginsWith(anonclass, NS_LITERAL_STRING("mozTable"))) {
    return NS_OK;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<Element> tableElement = GetEnclosingTable(mInlineEditedCell);
  int32_t rowCount, colCount;
  nsresult rv = GetTableSize(tableElement, &rowCount, &colCount);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }

  bool hideUI = false;
  bool hideResizersWithInlineTableUI = (mResizedObject == tableElement);

  if (anonclass.EqualsLiteral("mozTableAddColumnBefore")) {
    AutoEditActionDataSetter editActionData(*this,
                                            EditAction::eInsertTableColumn);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    DebugOnly<nsresult> rv = InsertTableColumnsWithTransaction(
        1, InsertPosition::eBeforeSelectedCell);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Failed to insert a column before the selected cell");
  } else if (anonclass.EqualsLiteral("mozTableAddColumnAfter")) {
    AutoEditActionDataSetter editActionData(*this,
                                            EditAction::eInsertTableColumn);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    DebugOnly<nsresult> rv = InsertTableColumnsWithTransaction(
        1, InsertPosition::eAfterSelectedCell);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Failed to insert a column after the selected cell");
  } else if (anonclass.EqualsLiteral("mozTableAddRowBefore")) {
    AutoEditActionDataSetter editActionData(*this,
                                            EditAction::eInsertTableRowElement);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    DebugOnly<nsresult> rv =
        InsertTableRowsWithTransaction(1, InsertPosition::eBeforeSelectedCell);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Failed to insert a row before the selected cell");
  } else if (anonclass.EqualsLiteral("mozTableAddRowAfter")) {
    AutoEditActionDataSetter editActionData(*this,
                                            EditAction::eInsertTableRowElement);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    DebugOnly<nsresult> rv =
        InsertTableRowsWithTransaction(1, InsertPosition::eAfterSelectedCell);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Failed to insert a row after the selected cell");
  } else if (anonclass.EqualsLiteral("mozTableRemoveColumn")) {
    AutoEditActionDataSetter editActionData(*this,
                                            EditAction::eRemoveTableColumn);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    DebugOnly<nsresult> rv = DeleteSelectedTableColumnsWithTransaction(1);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Failed to delete the selected table column");
    hideUI = (colCount == 1);
  } else if (anonclass.EqualsLiteral("mozTableRemoveRow")) {
    AutoEditActionDataSetter editActionData(*this,
                                            EditAction::eRemoveTableRowElement);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    DebugOnly<nsresult> rv = DeleteSelectedTableRowsWithTransaction(1);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Failed to delete the selected table row");
    hideUI = (rowCount == 1);
  } else {
    return NS_OK;
  }

  // InsertTableRowsWithTransaction() might causes reframe.
  if (Destroyed()) {
    return NS_OK;
  }

  if (hideUI) {
    HideInlineTableEditingUIInternal();
    if (hideResizersWithInlineTableUI) {
      DebugOnly<nsresult> rv = HideResizersInternal();
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to hide resizers");
    }
  }

  return NS_OK;
}

void HTMLEditor::AddMouseClickListener(Element* aElement) {
  if (aElement) {
    aElement->AddEventListener(NS_LITERAL_STRING("click"), mEventListener,
                               true);
  }
}

void HTMLEditor::RemoveMouseClickListener(Element* aElement) {
  if (aElement) {
    aElement->RemoveEventListener(NS_LITERAL_STRING("click"), mEventListener,
                                  true);
  }
}

NS_IMETHODIMP
HTMLEditor::RefreshInlineTableEditingUI() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = RefreshInlineTableEditingUIInternal();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult HTMLEditor::RefreshInlineTableEditingUIInternal() {
  if (!mInlineEditedCell) {
    return NS_OK;
  }

  RefPtr<nsGenericHTMLElement> inlineEditingCellElement =
      nsGenericHTMLElement::FromNode(mInlineEditedCell);
  if (NS_WARN_IF(!inlineEditingCellElement)) {
    return NS_ERROR_FAILURE;
  }

  int32_t cellX = 0, cellY = 0;
  GetElementOrigin(*mInlineEditedCell, cellX, cellY);

  int32_t cellWidth = inlineEditingCellElement->OffsetWidth();
  int32_t cellHeight = inlineEditingCellElement->OffsetHeight();

  int32_t centerOfCellX = cellX + cellWidth / 2;
  int32_t centerOfCellY = cellY + cellHeight / 2;

  RefPtr<Element> tableElement = GetEnclosingTable(mInlineEditedCell);
  int32_t rowCount = 0, colCount = 0;
  nsresult rv = GetTableSize(tableElement, &rowCount, &colCount);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<Element> addColumunBeforeButton = mAddColumnBeforeButton.get();
  SetAnonymousElementPosition(centerOfCellX - 10, cellY - 7,
                              addColumunBeforeButton);
  if (NS_WARN_IF(addColumunBeforeButton != mAddColumnBeforeButton.get())) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<Element> removeColumnButton = mRemoveColumnButton.get();
  SetAnonymousElementPosition(centerOfCellX - 4, cellY - 7, removeColumnButton);
  if (NS_WARN_IF(removeColumnButton != mRemoveColumnButton.get())) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<Element> addColumnAfterButton = mAddColumnAfterButton.get();
  SetAnonymousElementPosition(centerOfCellX + 6, cellY - 7,
                              addColumnAfterButton);
  if (NS_WARN_IF(addColumnAfterButton != mAddColumnAfterButton.get())) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<Element> addRowBeforeButton = mAddRowBeforeButton.get();
  SetAnonymousElementPosition(cellX - 7, centerOfCellY - 10,
                              addRowBeforeButton);
  if (NS_WARN_IF(addRowBeforeButton != mAddRowBeforeButton.get())) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<Element> removeRowButton = mRemoveRowButton.get();
  SetAnonymousElementPosition(cellX - 7, centerOfCellY - 4, removeRowButton);
  if (NS_WARN_IF(removeRowButton != mRemoveRowButton.get())) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<Element> addRowAfterButton = mAddRowAfterButton.get();
  SetAnonymousElementPosition(cellX - 7, centerOfCellY + 6, addRowAfterButton);
  if (NS_WARN_IF(addRowAfterButton != mAddRowAfterButton.get())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

}  // namespace mozilla
