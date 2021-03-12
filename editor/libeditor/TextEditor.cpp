/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TextEditor.h"

#include <algorithm>

#include "EditAggregateTransaction.h"
#include "HTMLEditUtils.h"
#include "InternetCiter.h"
#include "PlaceholderTransaction.h"
#include "gfxFontUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/mozalloc.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_editor.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TextServicesDocument.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/StaticRange.h"
#include "nsAString.h"
#include "nsCRT.h"
#include "nsCaret.h"
#include "nsCharTraits.h"
#include "nsComponentManagerUtils.h"
#include "nsContentCID.h"
#include "nsContentList.h"
#include "nsCopySupport.h"
#include "nsDebug.h"
#include "nsDependentSubstring.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIClipboard.h"
#include "nsIContent.h"
#include "nsIDocumentEncoder.h"
#include "nsINode.h"
#include "nsIPrincipal.h"
#include "nsISelectionController.h"
#include "nsISupportsPrimitives.h"
#include "nsITransferable.h"
#include "nsIWeakReferenceUtils.h"
#include "nsNameSpaceManager.h"
#include "nsLiteralString.h"
#include "nsPresContext.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTextFragment.h"
#include "nsTextNode.h"
#include "nsUnicharUtils.h"
#include "nsXPCOM.h"

class nsIOutputStream;
class nsISupports;

namespace mozilla {

using namespace dom;

using LeafNodeType = HTMLEditUtils::LeafNodeType;
using LeafNodeTypes = HTMLEditUtils::LeafNodeTypes;

TextEditor::TextEditor()
    : mMaxTextLength(-1),
      mUnmaskedStart(UINT32_MAX),
      mUnmaskedLength(0),
      mIsMaskingPassword(true) {
  // printf("Size of TextEditor: %zu\n", sizeof(TextEditor));
  static_assert(
      sizeof(TextEditor) <= 512,
      "TextEditor instance should be allocatable in the quantum class bins");
}

TextEditor::~TextEditor() {
  // Remove event listeners. Note that if we had an HTML editor,
  //  it installed its own instead of these
  RemoveEventListeners();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TextEditor)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(TextEditor, EditorBase)
  if (tmp->mMaskTimer) {
    tmp->mMaskTimer->Cancel();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCachedDocumentEncoder)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMaskTimer)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TextEditor, EditorBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCachedDocumentEncoder)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMaskTimer)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(TextEditor, EditorBase)
NS_IMPL_RELEASE_INHERITED(TextEditor, EditorBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextEditor)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
NS_INTERFACE_MAP_END_INHERITING(EditorBase)

nsresult TextEditor::Init(Document& aDoc, Element* aRoot,
                          nsISelectionController* aSelCon, uint32_t aFlags,
                          const nsAString& aInitialValue) {
  MOZ_ASSERT(!AsHTMLEditor());
  MOZ_ASSERT(!mInitSucceeded,
             "TextEditor::Init() called again without calling PreDestroy()?");

  // Init the base editor
  nsresult rv = EditorBase::Init(aDoc, aRoot, aSelCon, aFlags, aInitialValue);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::Init() failed");
    return rv;
  }

  // XXX `eNotEditing` is a lie since InitEditorContentAndSelection() may
  //     insert padding `<br>`.
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_FAILURE;
  }

  // We set mInitSucceeded here rather than at the end of the function,
  // since InitEditorContentAndSelection() can perform some transactions
  // and can warn if mInitSucceeded is still false.
  MOZ_ASSERT(!mInitSucceeded, "TextEditor::Init() shouldn't be nested");
  mInitSucceeded = true;

  rv = InitEditorContentAndSelection();
  if (NS_FAILED(rv)) {
    NS_WARNING("TextEditor::InitEditorContentAndSelection() failed");
    // XXX Sholdn't we expose `NS_ERROR_EDITOR_DESTROYED` even though this
    //     is a public method?
    mInitSucceeded = false;
    return EditorBase::ToGenericNSResult(rv);
  }

  // Throw away the old transaction manager if this is not the first time that
  // we're initializing the editor.
  ClearUndoRedo();
  EnableUndoRedo();
  return NS_OK;
}

NS_IMETHODIMP TextEditor::SetDocumentCharacterSet(
    const nsACString& characterSet) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eSetCharacterSet);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  rv = EditorBase::SetDocumentCharacterSet(characterSet);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::SetDocumentCharacterSet() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  // Update META charset element.
  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (UpdateMetaCharset(*document, characterSet)) {
    return NS_OK;
  }

  RefPtr<nsContentList> headElementList =
      document->GetElementsByTagName(u"head"_ns);
  if (NS_WARN_IF(!headElementList)) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> primaryHeadElement = headElementList->Item(0);
  if (NS_WARN_IF(!primaryHeadElement)) {
    return NS_OK;
  }

  // Create a new meta charset tag
  RefPtr<Element> metaElement = CreateNodeWithTransaction(
      *nsGkAtoms::meta, EditorDOMPoint(primaryHeadElement, 0));
  if (!metaElement) {
    NS_WARNING(
        "EditorBase::CreateNodeWithTransaction(nsGkAtoms::meta) failed, but "
        "ignored");
    return NS_OK;
  }

  // Set attributes to the created element
  if (characterSet.IsEmpty()) {
    return NS_OK;
  }

  // not undoable, undo should undo CreateNodeWithTransaction().
  DebugOnly<nsresult> rvIgnored = NS_OK;
  rvIgnored = metaElement->SetAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv,
                                   u"Content-Type"_ns, true);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "Element::SetAttr(nsGkAtoms::httpEquiv, Content-Type) "
                       "failed, but ignored");
  rvIgnored = metaElement->SetAttr(
      kNameSpaceID_None, nsGkAtoms::content,
      u"text/html;charset="_ns + NS_ConvertASCIItoUTF16(characterSet), true);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "Element::SetAttr(nsGkAtoms::content) failed, but ignored");
  return NS_OK;
}

bool TextEditor::UpdateMetaCharset(Document& aDocument,
                                   const nsACString& aCharacterSet) {
  // get a list of META tags
  RefPtr<nsContentList> metaElementList =
      aDocument.GetElementsByTagName(u"meta"_ns);
  if (NS_WARN_IF(!metaElementList)) {
    return false;
  }

  for (uint32_t i = 0; i < metaElementList->Length(true); ++i) {
    RefPtr<Element> metaElement = metaElementList->Item(i)->AsElement();
    MOZ_ASSERT(metaElement);

    nsAutoString currentValue;
    metaElement->GetAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv, currentValue);

    if (!FindInReadable(u"content-type"_ns, currentValue,
                        nsCaseInsensitiveStringComparator)) {
      continue;
    }

    metaElement->GetAttr(kNameSpaceID_None, nsGkAtoms::content, currentValue);

    constexpr auto charsetEquals = u"charset="_ns;
    nsAString::const_iterator originalStart, start, end;
    originalStart = currentValue.BeginReading(start);
    currentValue.EndReading(end);
    if (!FindInReadable(charsetEquals, start, end,
                        nsCaseInsensitiveStringComparator)) {
      continue;
    }

    // set attribute to <original prefix> charset=text/html
    nsresult rv = SetAttributeWithTransaction(
        *metaElement, *nsGkAtoms::content,
        Substring(originalStart, start) + charsetEquals +
            NS_ConvertASCIItoUTF16(aCharacterSet));
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "EditorBase::SetAttributeWithTransaction(nsGkAtoms::content) failed");
    return NS_SUCCEEDED(rv);
  }
  return false;
}

nsresult TextEditor::HandleKeyPressEvent(WidgetKeyboardEvent* aKeyboardEvent) {
  // NOTE: When you change this method, you should also change:
  //   * editor/libeditor/tests/test_texteditor_keyevent_handling.html
  //   * editor/libeditor/tests/test_htmleditor_keyevent_handling.html
  //
  // And also when you add new key handling, you need to change the subclass's
  // HandleKeyPressEvent()'s switch statement.

  if (IsReadonly()) {
    // When we're not editable, the events handled on EditorBase.
    return EditorBase::HandleKeyPressEvent(aKeyboardEvent);
  }

  if (NS_WARN_IF(!aKeyboardEvent)) {
    return NS_ERROR_UNEXPECTED;
  }
  MOZ_ASSERT(aKeyboardEvent->mMessage == eKeyPress,
             "HandleKeyPressEvent gets non-keypress event");

  switch (aKeyboardEvent->mKeyCode) {
    case NS_VK_META:
    case NS_VK_WIN:
    case NS_VK_SHIFT:
    case NS_VK_CONTROL:
    case NS_VK_ALT:
      // These keys are handled on EditorBase
      return EditorBase::HandleKeyPressEvent(aKeyboardEvent);
    case NS_VK_BACK: {
      if (aKeyboardEvent->IsControl() || aKeyboardEvent->IsAlt() ||
          aKeyboardEvent->IsMeta() || aKeyboardEvent->IsOS()) {
        return NS_OK;
      }
      DebugOnly<nsresult> rvIgnored =
          DeleteSelectionAsAction(nsIEditor::ePrevious, nsIEditor::eStrip);
      aKeyboardEvent->PreventDefault();
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "EditorBase::DeleteSelectionAsAction() failed, but ignored");
      return NS_OK;
    }
    case NS_VK_DELETE: {
      // on certain platforms (such as windows) the shift key
      // modifies what delete does (cmd_cut in this case).
      // bailing here to allow the keybindings to do the cut.
      if (aKeyboardEvent->IsShift() || aKeyboardEvent->IsControl() ||
          aKeyboardEvent->IsAlt() || aKeyboardEvent->IsMeta() ||
          aKeyboardEvent->IsOS()) {
        return NS_OK;
      }
      DebugOnly<nsresult> rvIgnored =
          DeleteSelectionAsAction(nsIEditor::eNext, nsIEditor::eStrip);
      aKeyboardEvent->PreventDefault();
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "EditorBase::DeleteSelectionAsAction() failed, but ignored");
      return NS_OK;
    }
    case NS_VK_TAB: {
      if (IsTabbable()) {
        return NS_OK;  // let it be used for focus switching
      }

      if (aKeyboardEvent->IsShift() || aKeyboardEvent->IsControl() ||
          aKeyboardEvent->IsAlt() || aKeyboardEvent->IsMeta() ||
          aKeyboardEvent->IsOS()) {
        return NS_OK;
      }

      // else we insert the tab straight through
      aKeyboardEvent->PreventDefault();
      nsresult rv = OnInputText(u"\t"_ns);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "TextEditor::OnInputText(\\t) failed");
      return rv;
    }
    case NS_VK_RETURN: {
      if (!aKeyboardEvent->IsInputtingLineBreak()) {
        return NS_OK;
      }
      if (!IsSingleLineEditor()) {
        aKeyboardEvent->PreventDefault();
      }
      // We need to dispatch "beforeinput" event at least even if we're a
      // single line text editor.
      nsresult rv = InsertLineBreakAsAction();
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "TextEditor::InsertLineBreakAsAction() failed");
      return rv;
    }
  }

  if (!aKeyboardEvent->IsInputtingText()) {
    // we don't PreventDefault() here or keybindings like control-x won't work
    return NS_OK;
  }
  // Our widget shouldn't set `\r` to `mCharCode`, but it may be synthesized
  // keyboard event and its value may be `\r`.  In such case, we should treat
  // it as `\n` for the backward compatibility because we stopped converting
  // `\r` and `\r\n` to `\n` at getting `HTMLInputElement.value` and
  // `HTMLTextAreaElement.value` for the performance (i.e., we don't need to
  // take care in `HTMLEditor`).
  char16_t charCode =
      static_cast<char16_t>(aKeyboardEvent->mCharCode) == nsCRT::CR
          ? nsCRT::LF
          : static_cast<char16_t>(aKeyboardEvent->mCharCode);
  aKeyboardEvent->PreventDefault();
  nsAutoString str(charCode);
  nsresult rv = OnInputText(str);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "TextEditor::OnInputText() failed");
  return rv;
}

nsresult TextEditor::OnInputText(const nsAString& aStringToInsert) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertText);
  MOZ_ASSERT(!aStringToInsert.IsVoid());
  editActionData.SetData(aStringToInsert);
  // FYI: For conforming to current UI Events spec, we should dispatch
  //      "beforeinput" event before "keypress" event, but here is in a
  //      "keypress" event listener.  However, the other browsers dispatch
  //      "beforeinput" event after "keypress" event.  Therefore, it makes
  //      sense to follow the other browsers.  Spec issue:
  //      https://github.com/w3c/uievents/issues/220
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this, *nsGkAtoms::TypingTxnName,
                                             ScrollSelectionIntoView::Yes);
  rv = InsertTextAsSubAction(aStringToInsert);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::InsertTextAsSubAction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult TextEditor::InsertLineBreakAsAction(nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertLineBreak,
                                          aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  if (IsSingleLineEditor()) {
    return NS_OK;
  }

  // XXX This may be called by execCommand() with "insertParagraph".
  //     In such case, naming the transaction "TypingTxnName" is odd.
  AutoPlaceholderBatch treatAsOneTransaction(*this, *nsGkAtoms::TypingTxnName,
                                             ScrollSelectionIntoView::Yes);
  rv = InsertLineBreakAsSubAction();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::InsertLineBreakAsSubAction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult TextEditor::SetTextAsAction(
    const nsAString& aString,
    AllowBeforeInputEventCancelable aAllowBeforeInputEventCancelable,
    nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aString.FindChar(nsCRT::CR) == kNotFound);
  MOZ_ASSERT(!AsHTMLEditor());

  AutoEditActionDataSetter editActionData(*this, EditAction::eSetText,
                                          aPrincipal);
  if (aAllowBeforeInputEventCancelable == AllowBeforeInputEventCancelable::No) {
    editActionData.MakeBeforeInputEventNonCancelable();
  }
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);
  rv = SetTextAsSubAction(aString);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TextEditor::SetTextAsSubAction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult TextEditor::ReplaceTextAsAction(
    const nsAString& aString, nsRange* aReplaceRange,
    AllowBeforeInputEventCancelable aAllowBeforeInputEventCancelable,
    nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aString.FindChar(nsCRT::CR) == kNotFound);

  AutoEditActionDataSetter editActionData(*this, EditAction::eReplaceText,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  if (aAllowBeforeInputEventCancelable == AllowBeforeInputEventCancelable::No) {
    editActionData.MakeBeforeInputEventNonCancelable();
  }

  if (!AsHTMLEditor()) {
    editActionData.SetData(aString);
  } else {
    editActionData.InitializeDataTransfer(aString);
    RefPtr<StaticRange> targetRange;
    if (aReplaceRange) {
      // Compute offset of the range before dispatching `beforeinput` event
      // because it may be referred after the DOM tree is changed and the
      // range may have not computed the offset yet.
      targetRange = StaticRange::Create(
          aReplaceRange->GetStartContainer(), aReplaceRange->StartOffset(),
          aReplaceRange->GetEndContainer(), aReplaceRange->EndOffset(),
          IgnoreErrors());
      NS_WARNING_ASSERTION(targetRange && targetRange->IsPositioned(),
                           "StaticRange::Create() failed");
    } else {
      Element* editingHost = AsHTMLEditor()->GetActiveEditingHost();
      NS_WARNING_ASSERTION(editingHost,
                           "No active editing host, no target ranges");
      if (editingHost) {
        targetRange = StaticRange::Create(
            editingHost, 0, editingHost, editingHost->Length(), IgnoreErrors());
        NS_WARNING_ASSERTION(targetRange && targetRange->IsPositioned(),
                             "StaticRange::Create() failed");
      }
    }
    if (targetRange && targetRange->IsPositioned()) {
      editActionData.AppendTargetRange(*targetRange);
    }
  }

  nsresult rv = editActionData.MaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "MaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);

  // This should emulates inserting text for better undo/redo behavior.
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertText, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return EditorBase::ToGenericNSResult(ignoredError.StealNSResult());
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "TextEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  if (!aReplaceRange) {
    nsresult rv = SetTextAsSubAction(aString);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "TextEditor::SetTextAsSubAction() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  if (aString.IsEmpty() && aReplaceRange->Collapsed()) {
    NS_WARNING("Setting value was empty and replaced range was empty");
    return NS_OK;
  }

  // Note that do not notify selectionchange caused by selecting all text
  // because it's preparation of our delete implementation so web apps
  // shouldn't receive such selectionchange before the first mutation.
  AutoUpdateViewBatch preventSelectionChangeEvent(*this);

  // Select the range but as far as possible, we should not create new range
  // even if it's part of special Selection.
  ErrorResult error;
  MOZ_KnownLive(SelectionRefPtr())->RemoveAllRanges(error);
  if (error.Failed()) {
    NS_WARNING("Selection::RemoveAllRanges() failed");
    return error.StealNSResult();
  }
  MOZ_KnownLive(SelectionRefPtr())
      ->AddRangeAndSelectFramesAndNotifyListeners(*aReplaceRange, error);
  if (error.Failed()) {
    NS_WARNING("Selection::AddRangeAndSelectFramesAndNotifyListeners() failed");
    return error.StealNSResult();
  }

  rv = ReplaceSelectionAsSubAction(aString);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TextEditor::ReplaceSelectionAsSubAction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult TextEditor::SetTextAsSubAction(const nsAString& aString) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(mPlaceholderBatch);

  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eSetText, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "TextEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  if (IsPlaintextEditor() && !IsIMEComposing() && !IsUndoRedoEnabled() &&
      GetEditAction() != EditAction::eReplaceText && mMaxTextLength < 0) {
    EditActionResult result = SetTextWithoutTransaction(aString);
    if (result.Failed() || result.Canceled() || result.Handled()) {
      NS_WARNING_ASSERTION(result.Succeeded(),
                           "TextEditor::SetTextWithoutTransaction() failed");
      return result.Rv();
    }
  }

  {
    // Note that do not notify selectionchange caused by selecting all text
    // because it's preparation of our delete implementation so web apps
    // shouldn't receive such selectionchange before the first mutation.
    AutoUpdateViewBatch preventSelectionChangeEvent(*this);

    RefPtr<Element> rootElement = GetRoot();
    if (NS_WARN_IF(!rootElement)) {
      return NS_ERROR_FAILURE;
    }

    // We want to select trailing `<br>` element to remove all nodes to replace
    // all, but TextEditor::SelectEntireDocument() doesn't select such `<br>`
    // elements.
    // XXX We should make ReplaceSelectionAsSubAction() take range.  Then,
    //     we can saving the expensive cost of modifying `Selection` here.
    nsresult rv;
    if (IsEmpty()) {
      rv = MOZ_KnownLive(SelectionRefPtr())->CollapseInLimiter(rootElement, 0);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "Selection::CollapseInLimiter() failed, but ignored");
    } else {
      // XXX Oh, we shouldn't select padding `<br>` element for empty last
      //     line here since we will need to recreate it in multiline
      //     text editor.
      ErrorResult error;
      MOZ_KnownLive(SelectionRefPtr())->SelectAllChildren(*rootElement, error);
      NS_WARNING_ASSERTION(
          !error.Failed(),
          "Selection::SelectAllChildren() failed, but ignored");
      rv = error.StealNSResult();
    }
    if (NS_SUCCEEDED(rv)) {
      DebugOnly<nsresult> rvIgnored = ReplaceSelectionAsSubAction(aString);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "TextEditor::ReplaceSelectionAsSubAction() failed, but ignored");
    }
  }

  // Destroying AutoUpdateViewBatch may cause destroying us.
  return NS_WARN_IF(Destroyed()) ? NS_ERROR_EDITOR_DESTROYED : NS_OK;
}

nsresult TextEditor::ReplaceSelectionAsSubAction(const nsAString& aString) {
  // TODO: Move this method to `EditorBase`.
  if (aString.IsEmpty()) {
    nsresult rv = DeleteSelectionAsSubAction(
        nsIEditor::eNone,
        IsTextEditor() ? nsIEditor::eNoStrip : nsIEditor::eStrip);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "EditorBase::DeleteSelectionAsSubAction(eNone) failed");
    return rv;
  }

  nsresult rv = InsertTextAsSubAction(aString);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::InsertTextAsSubAction() failed");
  return rv;
}

bool TextEditor::EnsureComposition(WidgetCompositionEvent& aCompositionEvent) {
  if (mComposition) {
    return true;
  }
  // The compositionstart event must cause creating new TextComposition
  // instance at being dispatched by IMEStateManager.
  mComposition = IMEStateManager::GetTextCompositionFor(&aCompositionEvent);
  if (!mComposition) {
    // However, TextComposition may be committed before the composition
    // event comes here.
    return false;
  }
  mComposition->StartHandlingComposition(this);
  return true;
}

nsresult TextEditor::OnCompositionStart(
    WidgetCompositionEvent& aCompositionStartEvent) {
  if (mComposition) {
    NS_WARNING("There was a composition at receiving compositionstart event");
    return NS_OK;
  }

  // "beforeinput" event shouldn't be fired before "compositionstart".
  AutoEditActionDataSetter editActionData(*this, EditAction::eStartComposition);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  EnsureComposition(aCompositionStartEvent);
  NS_WARNING_ASSERTION(mComposition, "Failed to get TextComposition instance?");
  return NS_OK;
}

nsresult TextEditor::OnCompositionChange(
    WidgetCompositionEvent& aCompositionChangeEvent) {
  MOZ_ASSERT(aCompositionChangeEvent.mMessage == eCompositionChange,
             "The event should be eCompositionChange");

  if (!mComposition) {
    NS_WARNING(
        "There is no composition, but receiving compositionchange event");
    return NS_ERROR_FAILURE;
  }

  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eUpdateComposition);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // If:
  // - new composition string is not empty,
  // - there is no composition string in the DOM tree,
  // - and there is non-collapsed Selection,
  // the selected content will be removed by this composition.
  if (aCompositionChangeEvent.mData.IsEmpty() &&
      mComposition->String().IsEmpty() && !SelectionRefPtr()->IsCollapsed()) {
    editActionData.UpdateEditAction(EditAction::eDeleteByComposition);
  }

  // If Input Events Level 2 is enabled, EditAction::eDeleteByComposition is
  // mapped to EditorInputType::eDeleteByComposition and it requires null
  // for InputEvent.data.  Therefore, only otherwise, we should set data.
  if (ToInputType(editActionData.GetEditAction()) !=
      EditorInputType::eDeleteByComposition) {
    MOZ_ASSERT(ToInputType(editActionData.GetEditAction()) ==
               EditorInputType::eInsertCompositionText);
    MOZ_ASSERT(!aCompositionChangeEvent.mData.IsVoid());
    editActionData.SetData(aCompositionChangeEvent.mData);
  }

  // If we're an `HTMLEditor` and this is second or later composition change,
  // we should set target range to the range of composition string.
  // Otherwise, set target ranges to selection ranges (will be done by
  // editActionData itself before dispatching `beforeinput` event).
  if (AsHTMLEditor() && mComposition->GetContainerTextNode()) {
    RefPtr<StaticRange> targetRange = StaticRange::Create(
        mComposition->GetContainerTextNode(),
        mComposition->XPOffsetInTextNode(),
        mComposition->GetContainerTextNode(),
        mComposition->XPEndOffsetInTextNode(), IgnoreErrors());
    NS_WARNING_ASSERTION(targetRange && targetRange->IsPositioned(),
                         "StaticRange::Create() failed");
    if (targetRange && targetRange->IsPositioned()) {
      editActionData.AppendTargetRange(*targetRange);
    }
  }

  // TODO: We need to use different EditAction value for beforeinput event
  //       if the event is followed by "compositionend" because corresponding
  //       "input" event will be fired from OnCompositionEnd() later with
  //       different EditAction value.
  // TODO: If Input Events Level 2 is enabled, "beforeinput" event may be
  //       actually canceled if edit action is eDeleteByComposition. In such
  //       case, we might need to keep selected text, but insert composition
  //       string before or after the selection.  However, the spec is still
  //       unstable.  We should keep handling the composition since other
  //       parts including widget may not be ready for such complicated
  //       behavior.
  nsresult rv = editActionData.MaybeDispatchBeforeInputEvent();
  if (rv != NS_ERROR_EDITOR_ACTION_CANCELED && NS_FAILED(rv)) {
    NS_WARNING("MaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  if (!EnsureComposition(aCompositionChangeEvent)) {
    NS_WARNING("TextEditor::EnsureComposition() failed");
    return NS_OK;
  }

  if (NS_WARN_IF(!GetPresShell())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // NOTE: TextComposition should receive selection change notification before
  //       CompositionChangeEventHandlingMarker notifies TextComposition of the
  //       end of handling compositionchange event because TextComposition may
  //       need to ignore selection changes caused by composition.  Therefore,
  //       CompositionChangeEventHandlingMarker must be destroyed after a call
  //       of NotifiyEditorObservers(eNotifyEditorObserversOfEnd) or
  //       NotifiyEditorObservers(eNotifyEditorObserversOfCancel) which notifies
  //       TextComposition of a selection change.
  MOZ_ASSERT(
      !mPlaceholderBatch,
      "UpdateIMEComposition() must be called without place holder batch");
  TextComposition::CompositionChangeEventHandlingMarker
      compositionChangeEventHandlingMarker(mComposition,
                                           &aCompositionChangeEvent);

  RefPtr<nsCaret> caret = GetCaret();

  {
    AutoPlaceholderBatch treatAsOneTransaction(*this, *nsGkAtoms::IMETxnName,
                                               ScrollSelectionIntoView::Yes);

    MOZ_ASSERT(
        mIsInEditSubAction,
        "AutoPlaceholderBatch should've notified the observes of before-edit");
    nsString data(aCompositionChangeEvent.mData);
    if (!AsTextEditor()) {
      nsContentUtils::PlatformToDOMLineBreaks(data);
    }
    rv = InsertTextAsSubAction(data);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "EditorBase::InsertTextAsSubAction() failed");

    if (caret) {
      caret->SetSelection(SelectionRefPtr());
    }
  }

  // If still composing, we should fire input event via observer.
  // Note that if the composition will be committed by the following
  // compositionend event, we don't need to notify editor observes of this
  // change.
  // NOTE: We must notify after the auto batch will be gone.
  if (!aCompositionChangeEvent.IsFollowedByCompositionEnd()) {
    NotifyEditorObservers(eNotifyEditorObserversOfEnd);
  }

  return EditorBase::ToGenericNSResult(rv);
}

void TextEditor::OnCompositionEnd(
    WidgetCompositionEvent& aCompositionEndEvent) {
  if (!mComposition) {
    NS_WARNING("There is no composition, but receiving compositionend event");
    return;
  }

  EditAction editAction = aCompositionEndEvent.mData.IsEmpty()
                              ? EditAction::eCancelComposition
                              : EditAction::eCommitComposition;
  AutoEditActionDataSetter editActionData(*this, editAction);
  // If Input Events Level 2 is enabled, EditAction::eCancelComposition is
  // mapped to EditorInputType::eDeleteCompositionText and it requires null
  // for InputEvent.data.  Therefore, only otherwise, we should set data.
  if (ToInputType(editAction) != EditorInputType::eDeleteCompositionText) {
    MOZ_ASSERT(
        ToInputType(editAction) == EditorInputType::eInsertCompositionText ||
        ToInputType(editAction) == EditorInputType::eInsertFromComposition);
    MOZ_ASSERT(!aCompositionEndEvent.mData.IsVoid());
    editActionData.SetData(aCompositionEndEvent.mData);
  }

  // commit the IME transaction..we can get at it via the transaction mgr.
  // Note that this means IME won't work without an undo stack!
  if (mTransactionManager) {
    if (nsCOMPtr<nsITransaction> transaction =
            mTransactionManager->PeekUndoStack()) {
      if (RefPtr<EditTransactionBase> transactionBase =
              transaction->GetAsEditTransactionBase()) {
        if (PlaceholderTransaction* placeholderTransaction =
                transactionBase->GetAsPlaceholderTransaction()) {
          placeholderTransaction->Commit();
        }
      }
    }
  }

  // Note that this just marks as that we've already handled "beforeinput" for
  // preventing assertions in FireInputEvent().  Note that corresponding
  // "beforeinput" event for the following "input" event should've already
  // been dispatched from `OnCompositionChange()`.
  DebugOnly<nsresult> rvIgnored =
      editActionData.MaybeDispatchBeforeInputEvent();
  MOZ_ASSERT(rvIgnored != NS_ERROR_EDITOR_ACTION_CANCELED,
             "Why beforeinput event was canceled in this case?");
  MOZ_ASSERT(NS_SUCCEEDED(rvIgnored),
             "MaybeDispatchBeforeInputEvent() should just mark the instance as "
             "handled it");

  // Composition string may have hidden the caret.  Therefore, we need to
  // cancel it here.
  HideCaret(false);

  // FYI: mComposition still keeps storing container text node of committed
  //      string, its offset and length.  However, they will be invalidated
  //      soon since its Destroy() will be called by IMEStateManager.
  mComposition->EndHandlingComposition(this);
  mComposition = nullptr;

  // notify editor observers of action
  // FYI: With current draft, "input" event should be fired from
  //      OnCompositionChange(), however, it requires a lot of our UI code
  //      change and does not make sense.  See spec issue:
  //      https://github.com/w3c/uievents/issues/202
  NotifyEditorObservers(eNotifyEditorObserversOfEnd);
}

already_AddRefed<Element> TextEditor::GetInputEventTargetElement() const {
  nsCOMPtr<Element> target = do_QueryInterface(mEventTarget);
  return target.forget();
}

bool TextEditor::IsEmpty() const {
  if (mPaddingBRElementForEmptyEditor) {
    return true;
  }

  // Even if there is no padding <br> element for empty editor, we should be
  // detected as empty editor if all the children are text nodes and these
  // have no content.
  Element* anonymousDivElement = GetRoot();
  if (!anonymousDivElement) {
    return true;  // Don't warn it, this is possible, e.g., 997805.html
  }

  // Only when there is non-empty text node, we are not empty.
  return !anonymousDivElement->GetFirstChild() ||
         !anonymousDivElement->GetFirstChild()->IsText() ||
         !anonymousDivElement->GetFirstChild()->Length();
}

NS_IMETHODIMP TextEditor::GetDocumentIsEmpty(bool* aDocumentIsEmpty) {
  MOZ_ASSERT(aDocumentIsEmpty);
  *aDocumentIsEmpty = IsEmpty();
  return NS_OK;
}

NS_IMETHODIMP TextEditor::GetTextLength(int32_t* aCount) {
  MOZ_ASSERT(aCount);

  // initialize out params
  *aCount = 0;

  // special-case for empty document, to account for the padding <br> element
  // for empty editor.
  // XXX This should be overridden by `HTMLEditor` and we should return the
  //     first text node's length from `TextEditor` instead.  The following
  //     code is too expensive.
  if (IsEmpty()) {
    return NS_OK;
  }

  Element* rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_FAILURE;
  }

  uint32_t totalLength = 0;
  PostContentIterator postOrderIter;
  DebugOnly<nsresult> rvIgnored = postOrderIter.Init(rootElement);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "PostContentIterator::Init() failed, but ignored");
  EditorType editorType = GetEditorType();
  for (; !postOrderIter.IsDone(); postOrderIter.Next()) {
    nsINode* currentNode = postOrderIter.GetCurrentNode();
    if (currentNode && currentNode->IsText() &&
        EditorUtils::IsEditableContent(*currentNode->AsText(), editorType)) {
      totalLength += currentNode->Length();
    }
  }

  *aCount = totalLength;
  return NS_OK;
}

nsresult TextEditor::UndoAsAction(uint32_t aCount, nsIPrincipal* aPrincipal) {
  if (aCount == 0 || IsReadonly()) {
    return NS_OK;
  }

  // If we don't have transaction in the undo stack, we shouldn't notify
  // anybody of trying to undo since it's not useful notification but we
  // need to pay some runtime cost.
  if (!CanUndo()) {
    return NS_OK;
  }

  // If there is composition, we shouldn't allow to undo with committing
  // composition since Chrome doesn't allow it and it doesn't make sense
  // because committing composition causes one transaction and Undo(1)
  // undoes the committing composition.
  if (GetComposition()) {
    return NS_OK;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eUndo, aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  AutoUpdateViewBatch preventSelectionChangeEvent(*this);

  NotifyEditorObservers(eNotifyEditorObserversOfBefore);
  if (NS_WARN_IF(!CanUndo()) || NS_WARN_IF(Destroyed())) {
    return NS_ERROR_FAILURE;
  }

  rv = NS_OK;
  {
    IgnoredErrorResult ignoredError;
    AutoEditSubActionNotifier startToHandleEditSubAction(
        *this, EditSubAction::eUndo, nsIEditor::eNone, ignoredError);
    if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
      return EditorBase::ToGenericNSResult(ignoredError.StealNSResult());
    }
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "TextEditor::OnStartToHandleTopLevelEditSubAction() "
                         "failed, but ignored");

    RefPtr<TransactionManager> transactionManager(mTransactionManager);
    for (uint32_t i = 0; i < aCount; ++i) {
      if (NS_FAILED(transactionManager->Undo())) {
        NS_WARNING("TransactionManager::Undo() failed");
        break;
      }
      DoAfterUndoTransaction();
    }

    if (NS_WARN_IF(!mRootElement)) {
      NS_WARNING("Failed to handle padding BR Element due to no root element");
      rv = NS_ERROR_FAILURE;
    } else {
      // The idea here is to see if the magic empty node has suddenly
      // reappeared as the result of the undo.  If it has, set our state
      // so we remember it.  There is a tradeoff between doing here and
      // at redo, or doing it everywhere else that might care.  Since undo
      // and redo are relatively rare, it makes sense to take the (small)
      // performance hit here.
      nsIContent* firstLeafChild = HTMLEditUtils::GetFirstLeafChild(
          *mRootElement, {LeafNodeType::OnlyLeafNode});
      if (firstLeafChild &&
          EditorUtils::IsPaddingBRElementForEmptyEditor(*firstLeafChild)) {
        mPaddingBRElementForEmptyEditor =
            static_cast<HTMLBRElement*>(firstLeafChild);
      } else {
        mPaddingBRElementForEmptyEditor = nullptr;
      }
    }
  }

  NotifyEditorObservers(eNotifyEditorObserversOfEnd);
  return EditorBase::ToGenericNSResult(rv);
}

nsresult TextEditor::RedoAsAction(uint32_t aCount, nsIPrincipal* aPrincipal) {
  if (aCount == 0 || IsReadonly()) {
    return NS_OK;
  }

  // If we don't have transaction in the redo stack, we shouldn't notify
  // anybody of trying to redo since it's not useful notification but we
  // need to pay some runtime cost.
  if (!CanRedo()) {
    return NS_OK;
  }

  // If there is composition, we shouldn't allow to redo with committing
  // composition since Chrome doesn't allow it and it doesn't make sense
  // because committing composition causes removing all transactions from
  // the redo queue.  So, it becomes impossible to redo anything.
  if (GetComposition()) {
    return NS_OK;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eRedo, aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  AutoUpdateViewBatch preventSelectionChangeEvent(*this);

  NotifyEditorObservers(eNotifyEditorObserversOfBefore);
  if (NS_WARN_IF(!CanRedo()) || NS_WARN_IF(Destroyed())) {
    return NS_ERROR_FAILURE;
  }

  rv = NS_OK;
  {
    IgnoredErrorResult ignoredError;
    AutoEditSubActionNotifier startToHandleEditSubAction(
        *this, EditSubAction::eRedo, nsIEditor::eNone, ignoredError);
    if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
      return ignoredError.StealNSResult();
    }
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "TextEditor::OnStartToHandleTopLevelEditSubAction() "
                         "failed, but ignored");

    RefPtr<TransactionManager> transactionManager(mTransactionManager);
    for (uint32_t i = 0; i < aCount; ++i) {
      if (NS_FAILED(transactionManager->Redo())) {
        NS_WARNING("TransactionManager::Redo() failed");
        break;
      }
      DoAfterRedoTransaction();
    }

    if (NS_WARN_IF(!mRootElement)) {
      NS_WARNING("Failed to handle padding BR element due to no root element");
      rv = NS_ERROR_FAILURE;
    } else {
      // We may take empty <br> element for empty editor back with this redo.
      // We need to store it again.
      // XXX Looks like that this is too slow if there are a lot of nodes.
      //     Shouldn't we just scan children in the root?
      nsCOMPtr<nsIHTMLCollection> nodeList =
          mRootElement->GetElementsByTagName(u"br"_ns);
      MOZ_ASSERT(nodeList);
      Element* brElement =
          nodeList->Length() == 1 ? nodeList->Item(0) : nullptr;
      if (brElement &&
          EditorUtils::IsPaddingBRElementForEmptyEditor(*brElement)) {
        mPaddingBRElementForEmptyEditor =
            static_cast<HTMLBRElement*>(brElement);
      } else {
        mPaddingBRElementForEmptyEditor = nullptr;
      }
    }
  }

  NotifyEditorObservers(eNotifyEditorObserversOfEnd);
  return EditorBase::ToGenericNSResult(rv);
}

bool TextEditor::IsCopyToClipboardAllowedInternal() const {
  MOZ_ASSERT(IsEditActionDataAvailable());
  if (SelectionRefPtr()->IsCollapsed()) {
    return false;
  }

  if (!IsSingleLineEditor() || !IsPasswordEditor()) {
    return true;
  }

  // If we're a password editor, we should allow selected text to be copied
  // to the clipboard only when selection range is in unmasked range.
  if (IsAllMasked() || IsMaskingPassword() || mUnmaskedLength == 0) {
    return false;
  }

  // If there are 2 or more ranges, we don't allow to copy/cut for now since
  // we need to check whether all ranges are in unmasked range or not.
  // Anyway, such operation in password field does not make sense.
  if (SelectionRefPtr()->RangeCount() > 1) {
    return false;
  }

  uint32_t selectionStart = 0, selectionEnd = 0;
  nsContentUtils::GetSelectionInTextControl(SelectionRefPtr(), mRootElement,
                                            selectionStart, selectionEnd);
  return mUnmaskedStart <= selectionStart && UnmaskedEnd() >= selectionEnd;
}

bool TextEditor::FireClipboardEvent(EventMessage aEventMessage,
                                    int32_t aSelectionType,
                                    bool* aActionTaken) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (aEventMessage == ePaste) {
    CommitComposition();
  }

  RefPtr<PresShell> presShell = GetPresShell();
  if (NS_WARN_IF(!presShell)) {
    return false;
  }

  RefPtr<Selection> sel = SelectionRefPtr();
  if (IsHTMLEditor() && aEventMessage == eCopy && sel->IsCollapsed()) {
    // If we don't have a usable selection for copy and we're an HTML editor
    // (which is global for the document) try to use the last focused selection
    // instead.
    sel = nsCopySupport::GetSelectionForCopy(GetDocument());
  }

  const bool clipboardEventCanceled = !nsCopySupport::FireClipboardEvent(
      aEventMessage, aSelectionType, presShell, sel, aActionTaken);
  NotifyOfDispatchingClipboardEvent();

  // If the event handler caused the editor to be destroyed, return false.
  // Otherwise return true if the event was not cancelled.
  return !clipboardEventCanceled && !mDidPreDestroy;
}

nsresult TextEditor::CutAsAction(nsIPrincipal* aPrincipal) {
  // TODO: Move this method to `EditorBase`.
  AutoEditActionDataSetter editActionData(*this, EditAction::eCut, aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  bool actionTaken = false;
  if (!FireClipboardEvent(eCut, nsIClipboard::kGlobalClipboard, &actionTaken)) {
    return EditorBase::ToGenericNSResult(
        actionTaken ? NS_OK : NS_ERROR_EDITOR_ACTION_CANCELED);
  }

  // Dispatch "beforeinput" event after dispatching "cut" event.
  nsresult rv = editActionData.MaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "MaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }
  // XXX This transaction name is referred by PlaceholderTransaction::Merge()
  //     so that we need to keep using it here.
  AutoPlaceholderBatch treatAsOneTransaction(*this, *nsGkAtoms::DeleteTxnName,
                                             ScrollSelectionIntoView::Yes);
  rv = DeleteSelectionAsSubAction(
      eNone, IsTextEditor() ? nsIEditor::eNoStrip : nsIEditor::eStrip);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "EditorBase::DeleteSelectionAsSubAction(eNone) failed, but ignored");
  return EditorBase::ToGenericNSResult(rv);
}

bool TextEditor::AreClipboardCommandsUnconditionallyEnabled() const {
  Document* document = GetDocument();
  return document && document->AreClipboardCommandsUnconditionallyEnabled();
}

bool TextEditor::CheckForClipboardCommandListener(
    nsAtom* aCommand, EventMessage aEventMessage) const {
  RefPtr<Document> document = GetDocument();
  if (!document) {
    return false;
  }

  // We exclude XUL and chrome docs here to maintain current behavior where
  // in these cases the editor element alone is expected to handle clipboard
  // command availability.
  if (!document->AreClipboardCommandsUnconditionallyEnabled()) {
    return false;
  }

  // So in web content documents, "unconditionally" enabled Cut/Copy are not
  // really unconditional; they're enabled if there is a listener that wants
  // to handle them. What they're not conditional on here is whether there is
  // currently a selection in the editor.
  RefPtr<PresShell> presShell = document->GetObservingPresShell();
  if (!presShell) {
    return false;
  }
  RefPtr<nsPresContext> presContext = presShell->GetPresContext();
  if (!presContext) {
    return false;
  }

  RefPtr<EventTarget> et = GetDOMEventTarget();
  while (et) {
    EventListenerManager* elm = et->GetOrCreateListenerManager();
    if (elm && elm->HasListenersFor(aCommand)) {
      return true;
    }
    InternalClipboardEvent event(true, aEventMessage);
    EventChainPreVisitor visitor(presContext, &event, nullptr,
                                 nsEventStatus_eIgnore, false, et);
    et->GetEventTargetParent(visitor);
    et = visitor.GetParentTarget();
  }

  return false;
}

bool TextEditor::IsCutCommandEnabled() const {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return false;
  }

  if (IsModifiable() && IsCopyToClipboardAllowedInternal()) {
    return true;
  }

  // If there's an event listener for "cut", we always enable the command
  // as we don't really know what the listener may want to do in response.
  // We look up the event target chain for a possible listener on a parent
  // in addition to checking the immediate target.
  return CheckForClipboardCommandListener(nsGkAtoms::oncut, eCut);
}

NS_IMETHODIMP TextEditor::Copy() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eCopy);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  bool actionTaken = false;
  FireClipboardEvent(eCopy, nsIClipboard::kGlobalClipboard, &actionTaken);

  return EditorBase::ToGenericNSResult(
      actionTaken ? NS_OK : NS_ERROR_EDITOR_ACTION_CANCELED);
}

bool TextEditor::IsCopyCommandEnabled() const {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return false;
  }

  if (IsCopyToClipboardAllowedInternal()) {
    return true;
  }

  // Like "cut", always enable "copy" if there's a listener.
  return CheckForClipboardCommandListener(nsGkAtoms::oncopy, eCopy);
}

bool TextEditor::CanDeleteSelection() const {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return false;
  }

  return IsModifiable() && !SelectionRefPtr()->IsCollapsed();
}

already_AddRefed<nsIDocumentEncoder> TextEditor::GetAndInitDocEncoder(
    const nsAString& aFormatType, uint32_t aDocumentEncoderFlags,
    const nsACString& aCharset) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsCOMPtr<nsIDocumentEncoder> docEncoder;
  if (!mCachedDocumentEncoder ||
      !mCachedDocumentEncoderType.Equals(aFormatType)) {
    nsAutoCString formatType;
    LossyAppendUTF16toASCII(aFormatType, formatType);
    docEncoder = do_createDocumentEncoder(PromiseFlatCString(formatType).get());
    if (NS_WARN_IF(!docEncoder)) {
      return nullptr;
    }
    mCachedDocumentEncoder = docEncoder;
    mCachedDocumentEncoderType = aFormatType;
  } else {
    docEncoder = mCachedDocumentEncoder;
  }

  RefPtr<Document> doc = GetDocument();
  NS_ASSERTION(doc, "Need a document");

  nsresult rv = docEncoder->NativeInit(
      doc, aFormatType,
      aDocumentEncoderFlags | nsIDocumentEncoder::RequiresReinitAfterOutput);
  if (NS_FAILED(rv)) {
    NS_WARNING("nsIDocumentEncoder::NativeInit() failed");
    return nullptr;
  }

  if (!aCharset.IsEmpty() && !aCharset.EqualsLiteral("null")) {
    DebugOnly<nsresult> rvIgnored = docEncoder->SetCharset(aCharset);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "nsIDocumentEncoder::SetCharset() failed, but ignored");
  }

  const int32_t wrapWidth = std::max(WrapWidth(), 0);
  DebugOnly<nsresult> rvIgnored = docEncoder->SetWrapColumn(wrapWidth);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "nsIDocumentEncoder::SetWrapColumn() failed, but ignored");

  // Set the selection, if appropriate.
  // We do this either if the OutputSelectionOnly flag is set,
  // in which case we use our existing selection ...
  if (aDocumentEncoderFlags & nsIDocumentEncoder::OutputSelectionOnly) {
    if (NS_FAILED(docEncoder->SetSelection(SelectionRefPtr()))) {
      NS_WARNING("nsIDocumentEncoder::SetSelection() failed");
      return nullptr;
    }
  }
  // ... or if the root element is not a body,
  // in which case we set the selection to encompass the root.
  else {
    dom::Element* rootElement = GetRoot();
    if (NS_WARN_IF(!rootElement)) {
      return nullptr;
    }
    if (!rootElement->IsHTMLElement(nsGkAtoms::body)) {
      if (NS_FAILED(docEncoder->SetContainerNode(rootElement))) {
        NS_WARNING("nsIDocumentEncoder::SetContainerNode() failed");
        return nullptr;
      }
    }
  }

  return docEncoder.forget();
}

NS_IMETHODIMP TextEditor::OutputToString(const nsAString& aFormatType,
                                         uint32_t aDocumentEncoderFlags,
                                         nsAString& aOutputString) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv =
      ComputeValueInternal(aFormatType, aDocumentEncoderFlags, aOutputString);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TextEditor::ComputeValueInternal() failed");
  // This is low level API for XUL application.  So, we should return raw
  // error code here.
  return rv;
}

nsresult TextEditor::ComputeValueInternal(const nsAString& aFormatType,
                                          uint32_t aDocumentEncoderFlags,
                                          nsAString& aOutputString) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // First, let's try to get the value simply only from text node if the
  // caller wants plaintext value.
  if (aFormatType.LowerCaseEqualsLiteral("text/plain")) {
    // If it's necessary to check selection range or the editor wraps hard,
    // we need some complicated handling.  In such case, we need to use the
    // expensive path.
    // XXX Anything else what we cannot return the text node data simply?
    if (!(aDocumentEncoderFlags & (nsIDocumentEncoder::OutputSelectionOnly |
                                   nsIDocumentEncoder::OutputWrap))) {
      EditActionResult result =
          ComputeValueFromTextNodeAndPaddingBRElement(aOutputString);
      if (result.Failed() || result.Canceled() || result.Handled()) {
        NS_WARNING_ASSERTION(
            result.Succeeded(),
            "TextEditor::ComputeValueFromTextNodeAndPaddingBRElement() failed");
        return result.Rv();
      }
    }
  }

  nsAutoCString charset;
  nsresult rv = GetDocumentCharsetInternal(charset);
  if (NS_FAILED(rv) || charset.IsEmpty()) {
    charset.AssignLiteral("windows-1252");
  }

  nsCOMPtr<nsIDocumentEncoder> encoder =
      GetAndInitDocEncoder(aFormatType, aDocumentEncoderFlags, charset);
  if (!encoder) {
    NS_WARNING("TextEditor::GetAndInitDocEncoder() failed");
    return NS_ERROR_FAILURE;
  }

  rv = encoder->EncodeToString(aOutputString);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "nsIDocumentEncoder::EncodeToString() failed");
  return rv;
}

nsresult TextEditor::PasteAsQuotationAsAction(int32_t aClipboardType,
                                              bool aDispatchPasteEvent,
                                              nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aClipboardType == nsIClipboard::kGlobalClipboard ||
             aClipboardType == nsIClipboard::kSelectionClipboard);

  AutoEditActionDataSetter editActionData(*this, EditAction::ePasteAsQuotation,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard =
      do_GetService("@mozilla.org/widget/clipboard;1", &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get nsIClipboard service");
    return rv;
  }

  // XXX Why don't we dispatch ePaste event here?

  // Get the nsITransferable interface for getting the data from the clipboard
  nsCOMPtr<nsITransferable> trans;
  rv = PrepareTransferable(getter_AddRefs(trans));
  if (NS_FAILED(rv)) {
    NS_WARNING("TextEditor::PrepareTransferable() failed");
    return EditorBase::ToGenericNSResult(rv);
  }
  if (!trans) {
    return NS_OK;
  }

  // Get the Data from the clipboard
  clipboard->GetData(trans, aClipboardType);

  // Now we ask the transferable for the data
  // it still owns the data, we just have a pointer to it.
  // If it can't support a "text" output of the data the call will fail
  nsCOMPtr<nsISupports> genericDataObj;
  nsAutoCString flav;
  rv = trans->GetAnyTransferData(flav, getter_AddRefs(genericDataObj));
  if (NS_FAILED(rv)) {
    NS_WARNING("nsITransferable::GetAnyTransferData() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  if (!flav.EqualsLiteral(kUnicodeMime) &&
      !flav.EqualsLiteral(kMozTextInternal)) {
    return NS_OK;
  }

  nsCOMPtr<nsISupportsString> text = do_QueryInterface(genericDataObj);
  if (!text) {
    return NS_OK;
  }

  nsString stuffToPaste;
  DebugOnly<nsresult> rvIgnored = text->GetData(stuffToPaste);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "nsISupportsString::GetData() failed, but ignored");
  if (stuffToPaste.IsEmpty()) {
    return NS_OK;
  }

  editActionData.SetData(stuffToPaste);
  if (!stuffToPaste.IsEmpty()) {
    nsContentUtils::PlatformToDOMLineBreaks(stuffToPaste);
  }
  // XXX Perhaps, we should dispatch "paste" event with the pasting text data.
  editActionData.NotifyOfDispatchingClipboardEvent();
  rv = editActionData.MaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "MaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this,
                                             ScrollSelectionIntoView::Yes);
  rv = InsertWithQuotationsAsSubAction(stuffToPaste);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TextEditor::InsertWithQuotationsAsSubAction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult TextEditor::InsertWithQuotationsAsSubAction(
    const nsAString& aQuotedText) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (IsReadonly()) {
    return NS_OK;
  }

  // Let the citer quote it for us:
  nsString quotedStuff;
  nsresult rv = InternetCiter::GetCiteString(aQuotedText, quotedStuff);
  if (NS_FAILED(rv)) {
    NS_WARNING("InternetCiter::GetCiteString() failed");
    return rv;
  }

  // It's best to put a blank line after the quoted text so that mails
  // written without thinking won't be so ugly.
  if (!aQuotedText.IsEmpty() && (aQuotedText.Last() != char16_t('\n'))) {
    quotedStuff.Append(char16_t('\n'));
  }

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertText, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "TextEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  // XXX Do we need to support paste-as-quotation in password editor (and
  //     also in single line editor)?
  MaybeDoAutoPasswordMasking();

  rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::EnsureNoPaddingBRElementForEmptyEditor() failed");
    return rv;
  }

  rv = InsertTextAsSubAction(quotedStuff);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::InsertTextAsSubAction() failed");
  return rv;
}

nsresult TextEditor::SharedOutputString(uint32_t aFlags, bool* aIsCollapsed,
                                        nsAString& aResult) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  *aIsCollapsed = SelectionRefPtr()->IsCollapsed();

  if (!*aIsCollapsed) {
    aFlags |= nsIDocumentEncoder::OutputSelectionOnly;
  }
  // If the selection isn't collapsed, we'll use the whole document.
  nsresult rv = ComputeValueInternal(u"text/plain"_ns, aFlags, aResult);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TextEditor::ComputeValueInternal(text/plain) failed");
  return rv;
}

nsresult TextEditor::SelectEntireDocument() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!AsHTMLEditor());

  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<Element> anonymousDivElement = GetRoot();
  if (NS_WARN_IF(!anonymousDivElement)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // If we're empty, don't select all children because that would select the
  // padding <br> element for empty editor.
  if (IsEmpty()) {
    nsresult rv = MOZ_KnownLive(SelectionRefPtr())
                      ->CollapseInLimiter(anonymousDivElement, 0);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Selection::CollapseInLimiter() failed");
    return rv;
  }

  // XXX We just need to select all of first text node (if there is).
  //     Why do we do this kind of complicated things?

  // Don't select the trailing BR node if we have one
  nsCOMPtr<nsIContent> childNode;
  nsresult rv = EditorBase::GetEndChildNode(*SelectionRefPtr(),
                                            getter_AddRefs(childNode));
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::GetEndChildNode() failed");
    return rv;
  }
  if (childNode) {
    childNode = childNode->GetPreviousSibling();
  }

  if (childNode &&
      EditorUtils::IsPaddingBRElementForEmptyLastLine(*childNode)) {
    ErrorResult error;
    MOZ_KnownLive(SelectionRefPtr())
        ->SetStartAndEndInLimiter(RawRangeBoundary(anonymousDivElement, 0u),
                                  EditorRawDOMPoint(childNode), error);
    NS_WARNING_ASSERTION(!error.Failed(),
                         "Selection::SetStartAndEndInLimiter() failed");
    return error.StealNSResult();
  }

  ErrorResult error;
  MOZ_KnownLive(SelectionRefPtr())
      ->SelectAllChildren(*anonymousDivElement, error);
  NS_WARNING_ASSERTION(!error.Failed(),
                       "Selection::SelectAllChildren() failed");
  return error.StealNSResult();
}

EventTarget* TextEditor::GetDOMEventTarget() const { return mEventTarget; }

nsresult TextEditor::SetAttributeOrEquivalent(Element* aElement,
                                              nsAtom* aAttribute,
                                              const nsAString& aValue,
                                              bool aSuppressTransaction) {
  if (NS_WARN_IF(!aElement) || NS_WARN_IF(!aAttribute)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eSetAttribute);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  rv = SetAttributeWithTransaction(*aElement, *aAttribute, aValue);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::SetAttributeWithTransaction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult TextEditor::RemoveAttributeOrEquivalent(Element* aElement,
                                                 nsAtom* aAttribute,
                                                 bool aSuppressTransaction) {
  if (NS_WARN_IF(!aElement) || NS_WARN_IF(!aAttribute)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eRemoveAttribute);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  rv = RemoveAttributeWithTransaction(*aElement, *aAttribute);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::RemoveAttributeWithTransaction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult TextEditor::EnsurePaddingBRElementForEmptyEditor() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!AsHTMLEditor());

  // If there is padding <br> element for empty editor, we have no work to do.
  if (mPaddingBRElementForEmptyEditor) {
    return NS_OK;
  }

  // Likewise, nothing to be done if we could never have inserted a trailing
  // <br> element.
  // XXX Why don't we use same path for <textarea> and <input>?
  if (IsSingleLineEditor()) {
    nsresult rv = MaybeCreatePaddingBRElementForEmptyEditor();
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "EditorBase::MaybeCreatePaddingBRElementForEmptyEditor() failed");
    return rv;
  }

  if (NS_WARN_IF(!mRootElement)) {
    return NS_ERROR_FAILURE;
  }

  uint32_t childCount = mRootElement->GetChildCount();
  if (childCount == 0) {
    nsresult rv = MaybeCreatePaddingBRElementForEmptyEditor();
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "EditorBase::MaybeCreatePaddingBRElementForEmptyEditor() failed");
    return rv;
  }

  if (childCount > 1) {
    return NS_OK;
  }

  RefPtr<HTMLBRElement> brElement =
      HTMLBRElement::FromNodeOrNull(mRootElement->GetFirstChild());
  if (!brElement ||
      !EditorUtils::IsPaddingBRElementForEmptyLastLine(*brElement)) {
    return NS_OK;
  }

  // Rather than deleting this node from the DOM tree we should instead
  // morph this <br> element into the padding <br> element for editor.
  mPaddingBRElementForEmptyEditor = std::move(brElement);
  mPaddingBRElementForEmptyEditor->UnsetFlags(NS_PADDING_FOR_EMPTY_LAST_LINE);
  mPaddingBRElementForEmptyEditor->SetFlags(NS_PADDING_FOR_EMPTY_EDITOR);

  return NS_OK;
}

nsresult TextEditor::SetUnmaskRangeInternal(uint32_t aStart, uint32_t aLength,
                                            uint32_t aTimeout, bool aNotify,
                                            bool aForceStartMasking) {
  mIsMaskingPassword = aForceStartMasking || aTimeout != 0;

  // We cannot manage multiple unmasked ranges so that shrink the previous
  // range first.
  if (!IsAllMasked()) {
    mUnmaskedLength = 0;
    if (mMaskTimer) {
      mMaskTimer->Cancel();
    }
  }

  // If we're not a password editor, return error since this call does not
  // make sense.
  if (!IsPasswordEditor()) {
    if (mMaskTimer) {
      mMaskTimer = nullptr;
    }
    return NS_ERROR_NOT_AVAILABLE;
  }

  Element* rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  Text* text = Text::FromNodeOrNull(rootElement->GetFirstChild());
  if (!text) {
    // There is no anonymous text node in the editor.
    return aStart > 0 && aStart != UINT32_MAX ? NS_ERROR_INVALID_ARG : NS_OK;
  }

  if (aStart < UINT32_MAX) {
    uint32_t valueLength = text->Length();
    if (aStart >= valueLength) {
      return NS_ERROR_INVALID_ARG;  // There is no character can be masked.
    }
    // If aStart is middle of a surrogate pair, expand it to include the
    // preceding high surrogate because the caller may want to show a
    // character before the character at `aStart + 1`.
    const nsTextFragment* textFragment = text->GetText();
    if (textFragment->IsLowSurrogateFollowingHighSurrogateAt(aStart)) {
      mUnmaskedStart = aStart - 1;
      // If caller collapses the range, keep it.  Otherwise, expand the length.
      if (aLength > 0) {
        ++aLength;
      }
    } else {
      mUnmaskedStart = aStart;
    }
    mUnmaskedLength = std::min(valueLength - mUnmaskedStart, aLength);
    // If unmasked end is middle of a surrogate pair, expand it to include
    // the following low surrogate because the caller may want to show a
    // character after the character at `aStart + aLength`.
    if (UnmaskedEnd() < valueLength &&
        textFragment->IsLowSurrogateFollowingHighSurrogateAt(UnmaskedEnd())) {
      ++mUnmaskedLength;
    }
    // If it's first time to mask the unmasking characters with timer, create
    // the timer now.  Then, we'll keep using it for saving the creation cost.
    if (!mMaskTimer && aLength && aTimeout && mUnmaskedLength) {
      mMaskTimer = NS_NewTimer();
    }
  } else {
    if (NS_WARN_IF(aLength != 0)) {
      return NS_ERROR_INVALID_ARG;
    }
    mUnmaskedStart = UINT32_MAX;
    mUnmaskedLength = 0;
  }

  // Notify nsTextFrame of this update if the caller wants this to do it.
  // Only in this case, script may run.
  if (aNotify) {
    MOZ_ASSERT(IsEditActionDataAvailable());

    RefPtr<Document> document = GetDocument();
    if (NS_WARN_IF(!document)) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    // Notify nsTextFrame of masking range change.
    if (RefPtr<PresShell> presShell = document->GetObservingPresShell()) {
      nsAutoScriptBlocker blockRunningScript;
      uint32_t valueLength = text->Length();
      CharacterDataChangeInfo changeInfo = {false, 0, valueLength, valueLength,
                                            nullptr};
      presShell->CharacterDataChanged(text, changeInfo);
    }

    // Scroll caret into the view since masking or unmasking character may
    // move caret to outside of the view.
    nsresult rv = ScrollSelectionFocusIntoView();
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::ScrollSelectionFocusIntoView() failed");
      return rv;
    }
  }

  if (!IsAllMasked() && aTimeout != 0) {
    // Initialize the timer to mask the range automatically.
    MOZ_ASSERT(mMaskTimer);
    DebugOnly<nsresult> rvIgnored =
        mMaskTimer->InitWithCallback(this, aTimeout, nsITimer::TYPE_ONE_SHOT);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "nsITimer::InitWithCallback() failed, but ignored");
  }

  return NS_OK;
}

// static
char16_t TextEditor::PasswordMask() {
  char16_t ret = LookAndFeel::GetPasswordCharacter();
  if (!ret) {
    ret = '*';
  }
  return ret;
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP TextEditor::Notify(nsITimer* aTimer) {
  // Check whether our text editor's password flag was changed before this
  // "hide password character" timer actually fires.
  if (!IsPasswordEditor()) {
    return NS_OK;
  }

  if (IsAllMasked()) {
    return NS_OK;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eHidePassword);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Mask all characters.
  nsresult rv = MaskAllCharactersAndNotify();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TextEditor::MaskAllCharactersAndNotify() failed");

  if (StaticPrefs::editor_password_testing_mask_delay()) {
    if (RefPtr<Element> target = GetInputEventTargetElement()) {
      RefPtr<Document> document = target->OwnerDoc();
      DebugOnly<nsresult> rvIgnored = nsContentUtils::DispatchTrustedEvent(
          document, target, u"MozLastInputMasked"_ns, CanBubble::eYes,
          Cancelable::eNo);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "nsContentUtils::DispatchTrustedEvent("
                           "MozLastInputMasked) failed, but ignored");
    }
  }

  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP TextEditor::GetName(nsACString& aName) {
  aName.AssignLiteral("TextEditor");
  return NS_OK;
}

void TextEditor::WillDeleteText(uint32_t aCurrentLength,
                                uint32_t aRemoveStartOffset,
                                uint32_t aRemoveLength) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!IsPasswordEditor() || IsAllMasked()) {
    return;
  }

  // Adjust unmasked range before deletion since DOM mutation may cause
  // layout referring the range in old text.

  // If we need to mask automatically, mask all now.
  if (mIsMaskingPassword) {
    DebugOnly<nsresult> rvIgnored = MaskAllCharacters();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "TextEditor::MaskAllCharacters() failed, but ignored");
    return;
  }

  if (aRemoveStartOffset < mUnmaskedStart) {
    // If removing range is before the unmasked range, move it.
    if (aRemoveStartOffset + aRemoveLength <= mUnmaskedStart) {
      DebugOnly<nsresult> rvIgnored =
          SetUnmaskRange(mUnmaskedStart - aRemoveLength, mUnmaskedLength);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "TextEditor::SetUnmaskRange() failed, but ignored");
      return;
    }

    // If removing range starts before unmasked range, and ends in unmasked
    // range, move and shrink the range.
    if (aRemoveStartOffset + aRemoveLength < UnmaskedEnd()) {
      uint32_t unmaskedLengthInRemovingRange =
          aRemoveStartOffset + aRemoveLength - mUnmaskedStart;
      DebugOnly<nsresult> rvIgnored = SetUnmaskRange(
          aRemoveStartOffset, mUnmaskedLength - unmaskedLengthInRemovingRange);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "TextEditor::SetUnmaskRange() failed, but ignored");
      return;
    }

    // If removing range includes all unmasked range, collapse it to the
    // remove offset.
    DebugOnly<nsresult> rvIgnored = SetUnmaskRange(aRemoveStartOffset, 0);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "TextEditor::SetUnmaskRange() failed, but ignored");
    return;
  }

  if (aRemoveStartOffset < UnmaskedEnd()) {
    // If removing range is in unmasked range, shrink the range.
    if (aRemoveStartOffset + aRemoveLength <= UnmaskedEnd()) {
      DebugOnly<nsresult> rvIgnored =
          SetUnmaskRange(mUnmaskedStart, mUnmaskedLength - aRemoveLength);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "TextEditor::SetUnmaskRange() failed, but ignored");
      return;
    }

    // If removing range starts from unmasked range, and ends after it,
    // shrink it.
    DebugOnly<nsresult> rvIgnored =
        SetUnmaskRange(mUnmaskedStart, aRemoveStartOffset - mUnmaskedStart);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "TextEditor::SetUnmaskRange() failed, but ignored");
    return;
  }

  // If removing range is after the unmasked range, keep it.
}

nsresult TextEditor::DidInsertText(uint32_t aNewLength,
                                   uint32_t aInsertedOffset,
                                   uint32_t aInsertedLength) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!IsPasswordEditor() || IsAllMasked()) {
    return NS_OK;
  }

  if (mIsMaskingPassword) {
    // If we need to mask password, mask all right now.
    nsresult rv = MaskAllCharactersAndNotify();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "TextEditor::MaskAllCharacters() failed");
    return rv;
  }

  if (aInsertedOffset < mUnmaskedStart) {
    // If insertion point is before unmasked range, expand the unmasked range
    // to include the new text.
    nsresult rv = SetUnmaskRangeAndNotify(
        aInsertedOffset, UnmaskedEnd() + aInsertedLength - aInsertedOffset);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "TextEditor::SetUnmaskRangeAndNotify() failed");
    return rv;
  }

  if (aInsertedOffset <= UnmaskedEnd()) {
    // If insertion point is in unmasked range, unmask new text.
    nsresult rv = SetUnmaskRangeAndNotify(mUnmaskedStart,
                                          mUnmaskedLength + aInsertedLength);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "TextEditor::SetUnmaskRangeAndNotify() failed");
    return rv;
  }

  // If insertion point is after unmasked range, extend the unmask range to
  // include the new text.
  nsresult rv = SetUnmaskRangeAndNotify(
      mUnmaskedStart, aInsertedOffset + aInsertedLength - mUnmaskedStart);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TextEditor::SetUnmaskRangeAndNotify() failed");
  return rv;
}

}  // namespace mozilla
