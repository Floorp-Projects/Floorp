/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TextEditor.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/SelectionState.h"
#include "mozilla/TextControlElement.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/DragEvent.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/StaticRange.h"
#include "nsAString.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsFocusManager.h"
#include "nsIClipboard.h"
#include "nsIContent.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPrincipal.h"
#include "nsIFormControl.h"
#include "nsISupportsPrimitives.h"
#include "nsITransferable.h"
#include "nsIVariant.h"
#include "nsLiteralString.h"
#include "nsRange.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsXPCOM.h"
#include "nscore.h"

class nsILoadContext;
class nsISupports;

namespace mozilla {

using namespace dom;

nsresult TextEditor::PrepareTransferable(nsITransferable** aOutTransferable) {
  MOZ_ASSERT(aOutTransferable);
  MOZ_ASSERT(!*aOutTransferable);

  // Create generic Transferable for getting the data
  nsresult rv;
  nsCOMPtr<nsITransferable> transferable =
      do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("do_CreateInstance() failed to create nsITransferable instance");
    return rv;
  }

  if (!transferable) {
    NS_WARNING("do_CreateInstance() returned nullptr, but ignored");
    return NS_OK;
  }

  RefPtr<Document> document = GetDocument();
  nsILoadContext* loadContext = document ? document->GetLoadContext() : nullptr;
  DebugOnly<nsresult> rvIgnored = transferable->Init(loadContext);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "nsITransferable::Init() failed, but ignored");

  rvIgnored = transferable->AddDataFlavor(kUnicodeMime);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "nsITransferable::AddDataFlavor(kUnicodeMime) failed, but ignored");
  rvIgnored = transferable->AddDataFlavor(kMozTextInternal);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "nsITransferable::AddDataFlavor(kMozTextInternal) failed, but ignored");
  transferable.forget(aOutTransferable);
  return NS_OK;
}

nsresult TextEditor::PrepareToInsertContent(
    const EditorDOMPoint& aPointToInsert, bool aDoDeleteSelection) {
  // TODO: Move this method to `EditorBase`.
  MOZ_ASSERT(IsEditActionDataAvailable());

  MOZ_ASSERT(aPointToInsert.IsSet());

  EditorDOMPoint pointToInsert(aPointToInsert);
  if (aDoDeleteSelection) {
    AutoTrackDOMPoint tracker(RangeUpdaterRef(), &pointToInsert);
    nsresult rv = DeleteSelectionAsSubAction(
        nsIEditor::eNone,
        IsTextEditor() ? nsIEditor::eNoStrip : nsIEditor::eStrip);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteSelectionAsSubAction(eNone) failed");
      return rv;
    }
  }

  IgnoredErrorResult error;
  SelectionRefPtr()->Collapse(pointToInsert, error);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(!error.Failed(), "Selection::Collapse() failed");
  return error.StealNSResult();
}

nsresult TextEditor::InsertTextAt(const nsAString& aStringToInsert,
                                  const EditorDOMPoint& aPointToInsert,
                                  bool aDoDeleteSelection) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  MOZ_ASSERT(aPointToInsert.IsSet());

  nsresult rv = PrepareToInsertContent(aPointToInsert, aDoDeleteSelection);
  if (NS_FAILED(rv)) {
    NS_WARNING("TextEditor::PrepareToInsertContent() failed");
    return rv;
  }

  rv = InsertTextAsSubAction(aStringToInsert);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::InsertTextAsSubAction() failed");
  return rv;
}

nsresult TextEditor::InsertTextFromTransferable(
    nsITransferable* aTransferable) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!AsHTMLEditor());

  nsAutoCString bestFlavor;
  nsCOMPtr<nsISupports> genericDataObj;
  nsresult rv = aTransferable->GetAnyTransferData(
      bestFlavor, getter_AddRefs(genericDataObj));
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "nsITransferable::GetAnyDataTransferData() failed, but ignored");
  if (NS_SUCCEEDED(rv) && (bestFlavor.EqualsLiteral(kUnicodeMime) ||
                           bestFlavor.EqualsLiteral(kMozTextInternal))) {
    AutoTransactionsConserveSelection dontChangeMySelection(*this);

    nsAutoString stuffToPaste;
    if (nsCOMPtr<nsISupportsString> text = do_QueryInterface(genericDataObj)) {
      text->GetData(stuffToPaste);
    }
    MOZ_ASSERT(GetEditAction() == EditAction::ePaste);
    // Use native line breaks for compatibility with Chrome.
    // XXX Although, somebody has already converted native line breaks to
    //     XP line breaks.
    UpdateEditActionData(stuffToPaste);

    nsresult rv = MaybeDispatchBeforeInputEvent();
    if (NS_FAILED(rv)) {
      NS_WARNING_ASSERTION(
          rv == NS_ERROR_EDITOR_ACTION_CANCELED,
          "EditorBase::MaybeDispatchBeforeInputEvent() failed");
      return rv;
    }

    if (!stuffToPaste.IsEmpty()) {
      // Sanitize possible carriage returns in the string to be inserted
      nsContentUtils::PlatformToDOMLineBreaks(stuffToPaste);

      AutoPlaceholderBatch treatAsOneTransaction(*this);
      nsresult rv = InsertTextAsSubAction(stuffToPaste);
      if (NS_FAILED(rv)) {
        NS_WARNING("EditorBase::InsertTextAsSubAction() failed");
        return rv;
      }
    }
  }

  // Try to scroll the selection into view if the paste/drop succeeded
  rv = ScrollSelectionFocusIntoView();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::ScrollSelectionFocusIntoView() failed");
  return rv;
}

nsresult TextEditor::OnDrop(DragEvent* aDropEvent) {
  if (NS_WARN_IF(!aDropEvent)) {
    return NS_ERROR_INVALID_ARG;
  }

  DebugOnly<nsresult> rvIgnored = CommitComposition();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "EditorBase::CommitComposition() failed, but ignored");

  AutoEditActionDataSetter editActionData(*this, EditAction::eDrop);
  // We need to initialize data or dataTransfer later.  Therefore, we cannot
  // dispatch "beforeinput" event until then.
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<DataTransfer> dataTransfer = aDropEvent->GetDataTransfer();
  if (NS_WARN_IF(!dataTransfer)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (NS_WARN_IF(!dragSession)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsINode> sourceNode = dataTransfer->GetMozSourceNode();

  RefPtr<Document> srcdoc;
  if (sourceNode) {
    srcdoc = sourceNode->OwnerDoc();
  }

  if (nsContentUtils::CheckForSubFrameDrop(
          dragSession, aDropEvent->WidgetEventPtr()->AsDragEvent())) {
    // Don't allow drags from subframe documents with different origins than
    // the drop destination.
    if (srcdoc && !IsSafeToInsertData(srcdoc)) {
      return NS_OK;
    }
  }

  // Current doc is destination
  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  uint32_t numItems = dataTransfer->MozItemCount();
  if (NS_WARN_IF(!numItems)) {
    return NS_ERROR_FAILURE;  // Nothing to drop?
  }

  // We have to figure out whether to delete and relocate caret only once
  // Parent and offset are under the mouse cursor.
  int32_t dropOffset = -1;
  nsCOMPtr<nsIContent> dropParentContent =
      aDropEvent->GetRangeParentContentAndOffset(&dropOffset);
  EditorDOMPoint droppedAt(dropParentContent, dropOffset);
  if (NS_WARN_IF(!droppedAt.IsSet()) ||
      NS_WARN_IF(!droppedAt.GetContainerAsContent())) {
    return NS_ERROR_FAILURE;
  }

  // Check if dropping into a selected range.  If so and the source comes from
  // same document, jump through some hoops to determine if mouse is over
  // selection (bail) and whether user wants to copy selection or delete it.
  if (!SelectionRefPtr()->IsCollapsed() && sourceNode &&
      sourceNode->IsEditable() && srcdoc == document) {
    uint32_t rangeCount = SelectionRefPtr()->RangeCount();
    for (uint32_t j = 0; j < rangeCount; j++) {
      const nsRange* range = SelectionRefPtr()->GetRangeAt(j);
      if (NS_WARN_IF(!range)) {
        // don't bail yet, iterate through them all
        continue;
      }
      IgnoredErrorResult ignoredError;
      if (range->IsPointInRange(*droppedAt.GetContainer(), droppedAt.Offset(),
                                ignoredError) &&
          !ignoredError.Failed()) {
        // If source document and destination document is same and dropping
        // into one of selected ranges, we don't need to do nothing.
        // XXX If the source comes from outside of this editor, this check
        //     means that we don't allow to drop the item in the selected
        //     range.  However, the selection is hidden until the <input> or
        //     <textarea> gets focus, therefore, this looks odd.
        return NS_OK;
      }
    }
  }

  // Delete if user doesn't want to copy when user moves selected content
  // to different place in same editor.
  // XXX Do we need the check whether it's in same document or not?
  RefPtr<TextEditor> editorToDeleteSelection;
  if (sourceNode && sourceNode->IsEditable() && srcdoc == document) {
    if ((dataTransfer->DropEffectInt() &
         nsIDragService::DRAGDROP_ACTION_MOVE) &&
        !(dataTransfer->DropEffectInt() &
          nsIDragService::DRAGDROP_ACTION_COPY)) {
      // If the source node is in native anonymous tree, it must be in
      // <input> or <textarea> element.  If so, its TextEditor can remove it.
      if (sourceNode->IsInNativeAnonymousSubtree()) {
        if (RefPtr<TextControlElement> textControlElement =
                TextControlElement::FromNodeOrNull(
                    sourceNode->GetClosestNativeAnonymousSubtreeRootParent())) {
          editorToDeleteSelection = textControlElement->GetTextEditor();
        }
      }
      // Otherwise, must be the content is in HTMLEditor.
      else if (AsHTMLEditor()) {
        editorToDeleteSelection = this;
      } else {
        editorToDeleteSelection =
            nsContentUtils::GetHTMLEditor(srcdoc->GetPresContext());
      }
    }
    // If the found editor isn't modifiable, we should not try to delete
    // selection.
    if (editorToDeleteSelection && !editorToDeleteSelection->IsModifiable()) {
      editorToDeleteSelection = nullptr;
    }
    // If the found editor has collapsed selection, we need to delete nothing
    // in the editor.
    if (editorToDeleteSelection) {
      if (Selection* selection = editorToDeleteSelection->GetSelection()) {
        if (selection->IsCollapsed()) {
          editorToDeleteSelection = nullptr;
        }
      }
    }
  }

  if (IsPlaintextEditor()) {
    for (nsIContent* content = droppedAt.GetContainerAsContent(); content;
         content = content->GetParent()) {
      nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(content));
      if (formControl && !formControl->AllowDrop()) {
        // Don't allow dropping into a form control that doesn't allow being
        // dropped into.
        return NS_OK;
      }
    }
  }

  // Combine any deletion and drop insertion into one transaction.
  AutoPlaceholderBatch treatAsOneTransaction(*this);

  // Don't dispatch "selectionchange" event until inserting all contents.
  SelectionBatcher selectionBatcher(SelectionRefPtr());

  // Track dropped point with nsRange because we shouldn't insert the
  // dropped content into different position even if some event listeners
  // modify selection.  Note that Chrome's behavior is really odd.  So,
  // we don't need to worry about web-compat about this.
  IgnoredErrorResult ignoredError;
  RefPtr<nsRange> rangeAtDropPoint =
      nsRange::Create(droppedAt.ToRawRangeBoundary(),
                      droppedAt.ToRawRangeBoundary(), ignoredError);
  if (NS_WARN_IF(ignoredError.Failed()) ||
      NS_WARN_IF(!rangeAtDropPoint->IsPositioned())) {
    editActionData.Abort();
    return NS_ERROR_FAILURE;
  }

  // Remove selected contents first here because we need to fire a pair of
  // "beforeinput" and "input" for deletion and web apps can cancel only
  // this deletion.  Note that callee may handle insertion asynchronously.
  // Therefore, it is the best to remove selected content here.
  if (editorToDeleteSelection) {
    nsresult rv = editorToDeleteSelection->DeleteSelectionByDragAsAction(
        mDispatchInputEvent);
    if (NS_WARN_IF(Destroyed())) {
      editActionData.Abort();
      return NS_OK;
    }
    // Ignore the editor instance specific error if it's another editor.
    if (this != editorToDeleteSelection &&
        (rv == NS_ERROR_NOT_INITIALIZED || rv == NS_ERROR_EDITOR_DESTROYED)) {
      rv = NS_OK;
    }
    // Don't cancel "insertFromDrop" even if "deleteByDrag" is canceled.
    if (rv != NS_ERROR_EDITOR_ACTION_CANCELED && NS_FAILED(rv)) {
      NS_WARNING("TextEditor::DeleteSelectionByDragAsAction() failed");
      editActionData.Abort();
      return EditorBase::ToGenericNSResult(rv);
    }
    if (NS_WARN_IF(!rangeAtDropPoint->IsPositioned()) ||
        NS_WARN_IF(!rangeAtDropPoint->GetStartContainer()->IsContent())) {
      editActionData.Abort();
      return NS_ERROR_FAILURE;
    }
    droppedAt = rangeAtDropPoint->StartRef();
    MOZ_ASSERT(droppedAt.IsSetAndValid());
  }

  // Before inserting dropping content, we need to move focus for compatibility
  // with Chrome and firing "beforeinput" event on new editing host.
  RefPtr<Element> focusedElement, newFocusedElement;
  if (!AsHTMLEditor()) {
    newFocusedElement = GetExposedRoot();
    focusedElement = IsActiveInDOMWindow() ? newFocusedElement : nullptr;
  } else if (!AsHTMLEditor()->IsInDesignMode()) {
    focusedElement = AsHTMLEditor()->GetActiveEditingHost();
    if (focusedElement &&
        droppedAt.GetContainerAsContent()->IsInclusiveDescendantOf(
            focusedElement)) {
      newFocusedElement = focusedElement;
    } else {
      newFocusedElement = droppedAt.GetContainerAsContent()->GetEditingHost();
    }
  }
  // Move selection right now.  Note that this does not move focus because
  // `Selection` moves focus with selection change only when the API caller is
  // JS.  And also this does not notify selection listeners (nor
  // "selectionchange") since we created SelectionBatcher above.
  ErrorResult error;
  MOZ_KnownLive(SelectionRefPtr())
      ->SetStartAndEnd(droppedAt.ToRawRangeBoundary(),
                       droppedAt.ToRawRangeBoundary(), error);
  if (error.Failed()) {
    NS_WARNING("Selection::SetStartAndEnd() failed");
    editActionData.Abort();
    return error.StealNSResult();
  }
  if (NS_WARN_IF(Destroyed())) {
    editActionData.Abort();
    return NS_OK;
  }
  // Then, move focus if necessary.  This must cause dispatching "blur" event
  // and "focus" event.
  if (newFocusedElement && focusedElement != newFocusedElement) {
    DebugOnly<nsresult> rvIgnored =
        nsFocusManager::GetFocusManager()->SetFocus(newFocusedElement, 0);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "nsFocusManager::SetFocus() failed to set focus "
                         "to the element, but ignored");
    if (NS_WARN_IF(Destroyed())) {
      editActionData.Abort();
      return NS_OK;
    }
    // "blur" or "focus" event listener may have changed the value.
    // Let's keep using the original point.
    if (NS_WARN_IF(!rangeAtDropPoint->IsPositioned()) ||
        NS_WARN_IF(!rangeAtDropPoint->GetStartContainer()->IsContent())) {
      return NS_ERROR_FAILURE;
    }
    droppedAt = rangeAtDropPoint->StartRef();
    MOZ_ASSERT(droppedAt.IsSetAndValid());

    // If focus is changed to different element and we're handling drop in
    // contenteditable, we cannot handle it without focus.  So, we should give
    // it up.
    if (AsHTMLEditor() && !AsHTMLEditor()->IsInDesignMode() &&
        NS_WARN_IF(newFocusedElement !=
                   AsHTMLEditor()->GetActiveEditingHost())) {
      editActionData.Abort();
      return NS_OK;
    }
  }

  if (!AsHTMLEditor()) {
    AutoTArray<nsString, 5> textArray;
    textArray.SetCapacity(numItems);
    uint32_t textLength = 0;
    for (uint32_t i = 0; i < numItems; ++i) {
      nsCOMPtr<nsIVariant> data;
      dataTransfer->GetDataAtNoSecurityCheck(NS_LITERAL_STRING("text/plain"), i,
                                             getter_AddRefs(data));
      if (!data) {
        continue;
      }
      // Use nsString to avoid copying its storage to textArray.
      nsString insertText;
      data->GetAsAString(insertText);
      if (insertText.IsEmpty()) {
        continue;
      }
      textArray.AppendElement(insertText);
      textLength += insertText.Length();
    }
    // Use nsString to avoid copying its storage to editActionData.
    nsString data;
    data.SetCapacity(textLength);
    // Join the text array from end to start because we insert each items
    // in the dataTransfer at same point from start to end.  Although I
    // don't know whether this is intentional behavior.
    for (nsString& text : Reversed(textArray)) {
      data.Append(text);
    }
    // Use native line breaks for compatibility with Chrome.
    // XXX Although, somebody has already converted native line breaks to
    //     XP line breaks.
    editActionData.SetData(data);

    nsresult rv = editActionData.MaybeDispatchBeforeInputEvent();
    if (NS_FAILED(rv)) {
      NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                           "MaybeDispatchBeforeInputEvent() failed");
      return EditorBase::ToGenericNSResult(rv);
    }

    // Then, insert the text.  Note that we shouldn't need to walk the array
    // anymore because nobody should listen to mutation events of anonymous
    // text node in <input>/<textarea>.
    nsContentUtils::PlatformToDOMLineBreaks(data);
    DebugOnly<nsresult> rvIgnored = InsertTextAt(data, droppedAt, false);
    if (NS_WARN_IF(Destroyed())) {
      return NS_OK;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "TextEditor::InsertTextAt() failed, but ignored");
  } else {
    editActionData.InitializeDataTransfer(dataTransfer);
    RefPtr<StaticRange> targetRange = StaticRange::Create(
        droppedAt.GetContainer(), droppedAt.Offset(), droppedAt.GetContainer(),
        droppedAt.Offset(), IgnoreErrors());
    NS_WARNING_ASSERTION(targetRange && targetRange->IsPositioned(),
                         "Why did we fail to create collapsed static range at "
                         "dropped position?");
    if (targetRange && targetRange->IsPositioned()) {
      editActionData.AppendTargetRange(*targetRange);
    }
    nsresult rv = editActionData.MaybeDispatchBeforeInputEvent();
    if (NS_FAILED(rv)) {
      NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                           "MaybeDispatchBeforeInputEvent() failed");
      return EditorBase::ToGenericNSResult(rv);
    }
    RefPtr<HTMLEditor> htmlEditor(AsHTMLEditor());
    for (uint32_t i = 0; i < numItems; ++i) {
      DebugOnly<nsresult> rvIgnored = htmlEditor->InsertFromDataTransfer(
          dataTransfer, i, srcdoc, droppedAt, false);
      if (NS_WARN_IF(Destroyed())) {
        return NS_OK;
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "HTMLEditor::InsertFromDataTransfer() failed, but ignored");
    }
  }

  nsresult rv = ScrollSelectionFocusIntoView();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::ScrollSelectionFocusIntoView() failed");
  return rv;
}

nsresult TextEditor::DeleteSelectionByDragAsAction(bool aDispatchInputEvent) {
  // TODO: Move this method to `EditorBase`.
  AutoRestore<bool> saveDispatchInputEvent(mDispatchInputEvent);
  mDispatchInputEvent = aDispatchInputEvent;
  // Even if we're handling "deleteByDrag" in same editor as "insertFromDrop",
  // we need to recreate edit action data here because
  // `AutoEditActionDataSetter` needs to manage event state separately.
  bool requestedByAnotherEditor = GetEditAction() != EditAction::eDrop;
  AutoEditActionDataSetter editActionData(*this, EditAction::eDeleteByDrag);
  MOZ_ASSERT(!SelectionRefPtr()->IsCollapsed());
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return rv;
  }
  // But keep using placeholder transaction for "insertFromDrop" if there is.
  Maybe<AutoPlaceholderBatch> treatAsOneTransaction;
  if (requestedByAnotherEditor) {
    treatAsOneTransaction.emplace(*this);
  }

  rv = DeleteSelectionAsSubAction(nsIEditor::eNone, IsTextEditor()
                                                        ? nsIEditor::eNoStrip
                                                        : nsIEditor::eStrip);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DeleteSelectionAsSubAction(eNone) failed");
    return rv;
  }

  if (!mDispatchInputEvent) {
    return NS_OK;
  }

  if (treatAsOneTransaction.isNothing()) {
    DispatchInputEvent();
  }
  return NS_WARN_IF(Destroyed()) ? NS_ERROR_EDITOR_DESTROYED : NS_OK;
}

nsresult TextEditor::PasteAsAction(int32_t aClipboardType,
                                   bool aDispatchPasteEvent,
                                   nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(*this, EditAction::ePaste,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (aDispatchPasteEvent && !FireClipboardEvent(ePaste, aClipboardType)) {
    return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_ACTION_CANCELED);
  }

  if (AsHTMLEditor()) {
    editActionData.InitializeDataTransferWithClipboard(
        SettingDataTransfer::eWithFormat, aClipboardType);
    nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
    if (NS_FAILED(rv)) {
      NS_WARNING_ASSERTION(
          rv == NS_ERROR_EDITOR_ACTION_CANCELED,
          "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
      return EditorBase::ToGenericNSResult(rv);
    }
    rv = MOZ_KnownLive(AsHTMLEditor())->PasteInternal(aClipboardType);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::PasteInternal() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  // The data will be initialized in InsertTextFromTransferable() if we're not
  // an HTMLEditor.  Therefore, we cannot dispatch "beforeinput" here.

  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard =
      do_GetService("@mozilla.org/widget/clipboard;1", &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get nsIClipboard service");
    return rv;
  }

  // Get the nsITransferable interface for getting the data from the clipboard
  nsCOMPtr<nsITransferable> transferable;
  rv = PrepareTransferable(getter_AddRefs(transferable));
  if (NS_FAILED(rv)) {
    NS_WARNING("TextEditor::PrepareTransferable() failed");
    return EditorBase::ToGenericNSResult(rv);
  }
  if (NS_WARN_IF(!transferable)) {
    NS_WARNING(
        "TextEditor::PrepareTransferable() returned nullptr, but ignored");
    return NS_OK;  // XXX Why?
  }
  // Get the Data from the clipboard.
  rv = clipboard->GetData(transferable, aClipboardType);
  if (NS_FAILED(rv)) {
    NS_WARNING("nsIClipboard::GetData() failed, but ignored");
    return NS_OK;  // XXX Why?
  }
  // XXX Why don't we check this first?
  if (!IsModifiable()) {
    return NS_OK;
  }
  rv = InsertTextFromTransferable(transferable);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TextEditor::InsertTextFromTransferable() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult TextEditor::PasteTransferableAsAction(nsITransferable* aTransferable,
                                               nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(*this, EditAction::ePaste,
                                          aPrincipal);
  // The data will be initialized in InsertTextFromTransferable().  Therefore,
  // we cannot dispatch "beforeinput" here.
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Use an invalid value for the clipboard type as data comes from
  // aTransferable and we don't currently implement a way to put that in the
  // data transfer yet.
  if (!FireClipboardEvent(ePaste, -1)) {
    return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_ACTION_CANCELED);
  }

  if (!IsModifiable()) {
    return NS_OK;
  }

  nsresult rv = InsertTextFromTransferable(aTransferable);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TextEditor::InsertTextFromTransferable() failed");
  return EditorBase::ToGenericNSResult(rv);
}

bool TextEditor::CanPaste(int32_t aClipboardType) const {
  // Always enable the paste command when inside of a HTML or XHTML document,
  // but if the document is chrome, let it control it.
  RefPtr<Document> doc = GetDocument();
  if (doc && doc->IsHTMLOrXHTML() && !nsContentUtils::IsChromeDoc(doc)) {
    return true;
  }

  // can't paste if readonly
  if (!IsModifiable()) {
    return false;
  }

  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(
      do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get nsIClipboard service");
    return false;
  }

  // the flavors that we can deal with
  AutoTArray<nsCString, 1> textEditorFlavors = {
      nsDependentCString(kUnicodeMime)};

  bool haveFlavors;
  rv = clipboard->HasDataMatchingFlavors(textEditorFlavors, aClipboardType,
                                         &haveFlavors);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "nsIClipboard::HasDataMatchingFlavors() failed");
  return NS_SUCCEEDED(rv) && haveFlavors;
}

bool TextEditor::CanPasteTransferable(nsITransferable* aTransferable) {
  // can't paste if readonly
  if (!IsModifiable()) {
    return false;
  }

  // If |aTransferable| is null, assume that a paste will succeed.
  if (!aTransferable) {
    return true;
  }

  nsCOMPtr<nsISupports> data;
  nsresult rv =
      aTransferable->GetTransferData(kUnicodeMime, getter_AddRefs(data));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "nsITransferable::GetTransferData(kUnicodeMime) failed");
  return NS_SUCCEEDED(rv) && data;
}

bool TextEditor::IsSafeToInsertData(Document* aSourceDoc) {
  // Try to determine whether we should use a sanitizing fragment sink
  bool isSafe = false;

  RefPtr<Document> destdoc = GetDocument();
  NS_ASSERTION(destdoc, "Where is our destination doc?");

  nsCOMPtr<nsIDocShell> docShell;
  if (RefPtr<BrowsingContext> bc = destdoc->GetBrowsingContext()) {
    RefPtr<BrowsingContext> root = bc->Top();
    MOZ_ASSERT(root, "root should not be null");

    docShell = root->GetDocShell();
  }

  isSafe = docShell && docShell->GetAppType() == nsIDocShell::APP_TYPE_EDITOR;

  if (!isSafe && aSourceDoc) {
    nsIPrincipal* srcPrincipal = aSourceDoc->NodePrincipal();
    nsIPrincipal* destPrincipal = destdoc->NodePrincipal();
    NS_ASSERTION(srcPrincipal && destPrincipal,
                 "How come we don't have a principal?");
    DebugOnly<nsresult> rvIgnored =
        srcPrincipal->Subsumes(destPrincipal, &isSafe);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "nsIPrincipal::Subsumes() failed, but ignored");
  }

  return isSafe;
}

}  // namespace mozilla
