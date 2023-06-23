/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditor.h"

#include "EditorEventListener.h"
#include "HTMLEditUtils.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Element.h"
#include "nsAString.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGenericHTMLElement.h"
#include "nsIContent.h"
#include "nsLiteralString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nscore.h"

namespace mozilla {

NS_IMETHODIMP HTMLEditor::SetInlineTableEditingEnabled(bool aIsEnabled) {
  EnableInlineTableEditor(aIsEnabled);
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::GetInlineTableEditingEnabled(bool* aIsEnabled) {
  *aIsEnabled = IsInlineTableEditorEnabled();
  return NS_OK;
}

nsresult HTMLEditor::ShowInlineTableEditingUIInternal(Element& aCellElement) {
  if (NS_WARN_IF(!HTMLEditUtils::IsTableCell(&aCellElement))) {
    return NS_OK;
  }

  const RefPtr<Element> editingHost = ComputeEditingHost();
  if (NS_WARN_IF(!editingHost) ||
      NS_WARN_IF(!aCellElement.IsInclusiveDescendantOf(editingHost))) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(mInlineEditedCell)) {
    return NS_ERROR_FAILURE;
  }

  mInlineEditedCell = &aCellElement;

  // the resizers and the shadow will be anonymous children of the body
  RefPtr<Element> rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
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
        nsGkAtoms::a, *rootElement, u"mozTableAddColumnBefore"_ns, false);
    if (NS_WARN_IF(!addColumnBeforeButton)) {
      NS_WARNING(
          "HTMLEditor::CreateAnonymousElement(nsGkAtoms::a, "
          "mozTableAddColumnBefore) failed");
      break;  // Hide unnecessary buttons created above.
    }
    if (NS_WARN_IF(mAddColumnBeforeButton) ||
        NS_WARN_IF(mInlineEditedCell != &aCellElement)) {
      return NS_ERROR_FAILURE;  // Don't hide another set of buttons.
    }
    mAddColumnBeforeButton = std::move(addColumnBeforeButton);

    ManualNACPtr removeColumnButton = CreateAnonymousElement(
        nsGkAtoms::a, *rootElement, u"mozTableRemoveColumn"_ns, false);
    if (!removeColumnButton) {
      NS_WARNING(
          "HTMLEditor::CreateAnonymousElement(nsGkAtoms::a, "
          "mozTableRemoveColumn) failed");
      break;
    }
    if (NS_WARN_IF(mRemoveColumnButton) ||
        NS_WARN_IF(mInlineEditedCell != &aCellElement)) {
      return NS_ERROR_FAILURE;
    }
    mRemoveColumnButton = std::move(removeColumnButton);

    ManualNACPtr addColumnAfterButton = CreateAnonymousElement(
        nsGkAtoms::a, *rootElement, u"mozTableAddColumnAfter"_ns, false);
    if (!addColumnAfterButton) {
      NS_WARNING(
          "HTMLEditor::CreateAnonymousElement(nsGkAtoms::a, "
          "mozTableAddColumnAfter) failed");
      break;
    }
    if (NS_WARN_IF(mAddColumnAfterButton) ||
        NS_WARN_IF(mInlineEditedCell != &aCellElement)) {
      return NS_ERROR_FAILURE;
    }
    mAddColumnAfterButton = std::move(addColumnAfterButton);

    ManualNACPtr addRowBeforeButton = CreateAnonymousElement(
        nsGkAtoms::a, *rootElement, u"mozTableAddRowBefore"_ns, false);
    if (!addRowBeforeButton) {
      NS_WARNING(
          "HTMLEditor::CreateAnonymousElement(nsGkAtoms::a, "
          "mozTableAddRowBefore) failed");
      break;
    }
    if (NS_WARN_IF(mAddRowBeforeButton) ||
        NS_WARN_IF(mInlineEditedCell != &aCellElement)) {
      return NS_ERROR_FAILURE;
    }
    mAddRowBeforeButton = std::move(addRowBeforeButton);

    ManualNACPtr removeRowButton = CreateAnonymousElement(
        nsGkAtoms::a, *rootElement, u"mozTableRemoveRow"_ns, false);
    if (!removeRowButton) {
      NS_WARNING(
          "HTMLEditor::CreateAnonymousElement(nsGkAtoms::a, "
          "mozTableRemoveRow) failed");
      break;
    }
    if (NS_WARN_IF(mRemoveRowButton) ||
        NS_WARN_IF(mInlineEditedCell != &aCellElement)) {
      return NS_ERROR_FAILURE;
    }
    mRemoveRowButton = std::move(removeRowButton);

    ManualNACPtr addRowAfterButton = CreateAnonymousElement(
        nsGkAtoms::a, *rootElement, u"mozTableAddRowAfter"_ns, false);
    if (!addRowAfterButton) {
      NS_WARNING(
          "HTMLEditor::CreateAnonymousElement(nsGkAtoms::a, "
          "mozTableAddRowAfter) failed");
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
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "HTMLEditor::RefreshInlineTableEditingUIInternal() failed");
    return rv;
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
  aElement.GetAttr(nsGkAtoms::_moz_anonclass, anonclass);

  if (!StringBeginsWith(anonclass, u"mozTable"_ns)) {
    return NS_OK;
  }

  if (NS_WARN_IF(!mInlineEditedCell) ||
      NS_WARN_IF(!mInlineEditedCell->IsInComposedDoc()) ||
      NS_WARN_IF(
          !HTMLEditUtils::IsTableRow(mInlineEditedCell->GetParentNode()))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<Element> tableElement =
      HTMLEditUtils::GetClosestAncestorTableElement(*mInlineEditedCell);
  if (!tableElement) {
    NS_WARNING("HTMLEditor::GetClosestAncestorTableElement() returned nullptr");
    return NS_ERROR_FAILURE;
  }
  int32_t rowCount, colCount;
  nsresult rv = GetTableSize(tableElement, &rowCount, &colCount);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetTableSize() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  bool hideUI = false;
  bool hideResizersWithInlineTableUI = (mResizedObject == tableElement);

  if (anonclass.EqualsLiteral("mozTableAddColumnBefore")) {
    AutoEditActionDataSetter editActionData(*this,
                                            EditAction::eInsertTableColumn);
    nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
    if (NS_FAILED(rv)) {
      NS_WARNING_ASSERTION(
          rv == NS_ERROR_EDITOR_ACTION_CANCELED,
          "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
      return EditorBase::ToGenericNSResult(rv);
    }

    DebugOnly<nsresult> rvIgnored =
        InsertTableColumnsWithTransaction(EditorDOMPoint(mInlineEditedCell), 1);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditor::InsertTableColumnsWithTransaction("
        "EditorDOMPoint(mInlineEditedCell), 1) failed, but ignored");
  } else if (anonclass.EqualsLiteral("mozTableAddColumnAfter")) {
    AutoEditActionDataSetter editActionData(*this,
                                            EditAction::eInsertTableColumn);
    nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
    if (NS_FAILED(rv)) {
      NS_WARNING_ASSERTION(
          rv == NS_ERROR_EDITOR_ACTION_CANCELED,
          "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
      return EditorBase::ToGenericNSResult(rv);
    }
    Element* nextCellElement = nullptr;
    for (nsIContent* maybeNextCellElement = mInlineEditedCell->GetNextSibling();
         maybeNextCellElement;
         maybeNextCellElement = maybeNextCellElement->GetNextSibling()) {
      if (HTMLEditUtils::IsTableCell(maybeNextCellElement)) {
        nextCellElement = maybeNextCellElement->AsElement();
        break;
      }
    }
    DebugOnly<nsresult> rvIgnored = InsertTableColumnsWithTransaction(
        nextCellElement
            ? EditorDOMPoint(nextCellElement)
            : EditorDOMPoint::AtEndOf(*mInlineEditedCell->GetParentElement()),
        1);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditor::InsertTableColumnsWithTransaction("
        "EditorDOMPoint(nextCellElement), 1) failed, but ignored");
  } else if (anonclass.EqualsLiteral("mozTableAddRowBefore")) {
    AutoEditActionDataSetter editActionData(*this,
                                            EditAction::eInsertTableRowElement);
    nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
    if (NS_FAILED(rv)) {
      NS_WARNING_ASSERTION(
          rv == NS_ERROR_EDITOR_ACTION_CANCELED,
          "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
      return EditorBase::ToGenericNSResult(rv);
    }
    OwningNonNull<Element> targetCellElement(*mInlineEditedCell);
    DebugOnly<nsresult> rvIgnored = InsertTableRowsWithTransaction(
        targetCellElement, 1, InsertPosition::eBeforeSelectedCell);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditor::InsertTableRowsWithTransaction(targetCellElement, 1, "
        "InsertPosition::eBeforeSelectedCell) failed, but ignored");
  } else if (anonclass.EqualsLiteral("mozTableAddRowAfter")) {
    AutoEditActionDataSetter editActionData(*this,
                                            EditAction::eInsertTableRowElement);
    nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
    if (NS_FAILED(rv)) {
      NS_WARNING_ASSERTION(
          rv == NS_ERROR_EDITOR_ACTION_CANCELED,
          "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
      return EditorBase::ToGenericNSResult(rv);
    }
    OwningNonNull<Element> targetCellElement(*mInlineEditedCell);
    DebugOnly<nsresult> rvIgnored = InsertTableRowsWithTransaction(
        targetCellElement, 1, InsertPosition::eAfterSelectedCell);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditor::InsertTableRowsWithTransaction(targetCellElement, 1, "
        "InsertPosition::eAfterSelectedCell) failed, but ignored");
  } else if (anonclass.EqualsLiteral("mozTableRemoveColumn")) {
    AutoEditActionDataSetter editActionData(*this,
                                            EditAction::eRemoveTableColumn);
    nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
    if (NS_FAILED(rv)) {
      NS_WARNING_ASSERTION(
          rv == NS_ERROR_EDITOR_ACTION_CANCELED,
          "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
      return EditorBase::ToGenericNSResult(rv);
    }
    DebugOnly<nsresult> rvIgnored =
        DeleteSelectedTableColumnsWithTransaction(1);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "HTMLEditor::DeleteSelectedTableColumnsWithTransaction(1) failed, but "
        "ignored");
    hideUI = (colCount == 1);
  } else if (anonclass.EqualsLiteral("mozTableRemoveRow")) {
    AutoEditActionDataSetter editActionData(*this,
                                            EditAction::eRemoveTableRowElement);
    nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
    if (NS_FAILED(rv)) {
      NS_WARNING_ASSERTION(
          rv == NS_ERROR_EDITOR_ACTION_CANCELED,
          "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
      return EditorBase::ToGenericNSResult(rv);
    }
    DebugOnly<nsresult> rvIgnored = DeleteSelectedTableRowsWithTransaction(1);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "HTMLEditor::DeleteSelectedTableRowsWithTransaction(1)"
                         " failed, but ignored");
    hideUI = (rowCount == 1);
  } else {
    return NS_OK;
  }

  if (NS_WARN_IF(Destroyed())) {
    return NS_OK;
  }

  if (hideUI) {
    HideInlineTableEditingUIInternal();
    if (hideResizersWithInlineTableUI) {
      DebugOnly<nsresult> rvIgnored = HideResizersInternal();
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "HTMLEditor::HideResizersInternal() failed, but ignored");
    }
  }

  return NS_OK;
}

void HTMLEditor::AddMouseClickListener(Element* aElement) {
  if (NS_WARN_IF(!aElement)) {
    return;
  }
  DebugOnly<nsresult> rvIgnored =
      aElement->AddEventListener(u"click"_ns, mEventListener, true);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "EventTarget::AddEventListener(click) failed, but ignored");
}

void HTMLEditor::RemoveMouseClickListener(Element* aElement) {
  if (NS_WARN_IF(!aElement)) {
    return;
  }
  aElement->RemoveEventListener(u"click"_ns, mEventListener, true);
}

nsresult HTMLEditor::RefreshInlineTableEditingUIInternal() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!mInlineEditedCell) {
    return NS_OK;
  }

  RefPtr<nsGenericHTMLElement> inlineEditingCellElement =
      nsGenericHTMLElement::FromNode(mInlineEditedCell);
  if (NS_WARN_IF(!inlineEditingCellElement)) {
    return NS_ERROR_FAILURE;
  }

  int32_t cellX = 0, cellY = 0;
  DebugOnly<nsresult> rvIgnored =
      GetElementOrigin(*mInlineEditedCell, cellX, cellY);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "HTMLEditor::GetElementOrigin() failed, but ignored");

  int32_t cellWidth = inlineEditingCellElement->OffsetWidth();
  int32_t cellHeight = inlineEditingCellElement->OffsetHeight();

  int32_t centerOfCellX = cellX + cellWidth / 2;
  int32_t centerOfCellY = cellY + cellHeight / 2;

  RefPtr<Element> tableElement =
      HTMLEditUtils::GetClosestAncestorTableElement(*mInlineEditedCell);
  if (!tableElement) {
    NS_WARNING("HTMLEditor::GetClosestAncestorTableElement() returned nullptr");
    return NS_ERROR_FAILURE;
  }
  int32_t rowCount = 0, colCount = 0;
  nsresult rv = GetTableSize(tableElement, &rowCount, &colCount);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetTableSize() failed");
    return rv;
  }

  auto setInlineTableEditButtonPosition =
      [this](ManualNACPtr& aButtonElement, int32_t aNewX, int32_t aNewY)
          MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION -> nsresult {
    RefPtr<nsStyledElement> buttonStyledElement =
        nsStyledElement::FromNodeOrNull(aButtonElement.get());
    if (!buttonStyledElement) {
      return NS_OK;
    }
    nsresult rv = SetAnonymousElementPositionWithoutTransaction(
        *buttonStyledElement, aNewX, aNewY);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::SetAnonymousElementPositionWithoutTransaction() "
          "failed");
      return rv;
    }
    return NS_WARN_IF(buttonStyledElement != aButtonElement.get())
               ? NS_ERROR_FAILURE
               : NS_OK;
  };

  // clang-format off
  rv = setInlineTableEditButtonPosition(mAddColumnBeforeButton,
                                        centerOfCellX - 10, cellY - 7);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to move button for add-column-before");
    return rv;
  }
  rv = setInlineTableEditButtonPosition(mRemoveColumnButton,
                                        centerOfCellX - 4, cellY - 7);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to move button for remove-column");
    return rv;
  }
  rv = setInlineTableEditButtonPosition(mAddColumnAfterButton,
                                        centerOfCellX + 6, cellY - 7);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to move button for add-column-after");
    return rv;
  }
  rv = setInlineTableEditButtonPosition(mAddRowBeforeButton,
                                        cellX - 7, centerOfCellY - 10);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to move button for add-row-before");
    return rv;
  }
  rv = setInlineTableEditButtonPosition(mRemoveRowButton,
                                        cellX - 7, centerOfCellY - 4);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to move button for remove-row");
    return rv;
  }
  rv = setInlineTableEditButtonPosition(mAddRowAfterButton,
                                        cellX - 7, centerOfCellY + 6);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to move button for add-row-after");
    return rv;
  }
  // clang-format on

  return NS_WARN_IF(Destroyed()) ? NS_ERROR_EDITOR_DESTROYED : NS_OK;
}

}  // namespace mozilla
