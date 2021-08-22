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
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/Assertions.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorDOMPoint.h"
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
#include "nsDebug.h"
#include "nsDependentSubstring.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIClipboard.h"
#include "nsIContent.h"
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

TextEditor::TextEditor() {
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
  if (tmp->mPasswordMaskData) {
    tmp->mPasswordMaskData->CancelTimer(PasswordMaskData::ReleaseTimer::No);
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mPasswordMaskData->mTimer)
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TextEditor, EditorBase)
  if (tmp->mPasswordMaskData) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPasswordMaskData->mTimer)
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(TextEditor, EditorBase)
NS_IMPL_RELEASE_INHERITED(TextEditor, EditorBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextEditor)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
NS_INTERFACE_MAP_END_INHERITING(EditorBase)

nsresult TextEditor::Init(Document& aDocument, Element& aAnonymousDivElement,
                          nsISelectionController& aSelectionController,
                          uint32_t aFlags,
                          UniquePtr<PasswordMaskData>&& aPasswordMaskData) {
  MOZ_ASSERT(!mInitSucceeded,
             "TextEditor::Init() called again without calling PreDestroy()?");
  MOZ_ASSERT(!(aFlags & nsIEditor::eEditorPasswordMask) == !aPasswordMaskData);
  mPasswordMaskData = std::move(aPasswordMaskData);

  // Init the base editor
  nsresult rv = InitInternal(aDocument, &aAnonymousDivElement,
                             aSelectionController, aFlags);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::InitInternal() failed");
    return rv;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eInitializing);
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
    NS_WARNING("EditorBase::InitEditorContentAndSelection() failed");
    // XXX Shouldn't we expose `NS_ERROR_EDITOR_DESTROYED` even though this
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

nsresult TextEditor::PostCreate() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = PostCreateInternal();

  // Restore unmasked range if there is.
  if (IsPasswordEditor() && !IsAllMasked()) {
    DebugOnly<nsresult> rvIgnored =
        SetUnmaskRangeAndNotify(UnmaskedStart(), UnmaskedLength());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "TextEditor::SetUnmaskRangeAndNotify() failed to "
                         "restore unmasked range, but ignored");
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::PostCreateInternal() failed");
  return rv;
}

UniquePtr<PasswordMaskData> TextEditor::PreDestroy() {
  if (mDidPreDestroy) {
    return nullptr;
  }

  UniquePtr<PasswordMaskData> passwordMaskData = std::move(mPasswordMaskData);
  if (passwordMaskData) {
    // Disable auto-masking timer since nobody can catch the notification
    // from the timer and canceling the unmasking.
    passwordMaskData->CancelTimer(PasswordMaskData::ReleaseTimer::Yes);
    // Similary, keeping preventing echoing password temporarily across
    // TextEditor instances is hard.  So, we should forget it.
    passwordMaskData->mEchoingPasswordPrevented = false;
  }

  PreDestroyInternal();

  return passwordMaskData;
}

nsresult TextEditor::HandleKeyPressEvent(WidgetKeyboardEvent* aKeyboardEvent) {
  // NOTE: When you change this method, you should also change:
  //   * editor/libeditor/tests/test_texteditor_keyevent_handling.html
  //   * editor/libeditor/tests/test_htmleditor_keyevent_handling.html
  //
  // And also when you add new key handling, you need to change the subclass's
  // HandleKeyPressEvent()'s switch statement.

  if (NS_WARN_IF(!aKeyboardEvent)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (IsReadonly()) {
    HandleKeyPressEventInReadOnlyMode(*aKeyboardEvent);
    return NS_OK;
  }

  MOZ_ASSERT(aKeyboardEvent->mMessage == eKeyPress,
             "HandleKeyPressEvent gets non-keypress event");

  switch (aKeyboardEvent->mKeyCode) {
    case NS_VK_META:
    case NS_VK_WIN:
    case NS_VK_SHIFT:
    case NS_VK_CONTROL:
    case NS_VK_ALT:
      // FYI: This shouldn't occur since modifier key shouldn't cause eKeyPress
      //      event.
      aKeyboardEvent->PreventDefault();
      return NS_OK;

    case NS_VK_BACK:
    case NS_VK_DELETE:
    case NS_VK_TAB: {
      nsresult rv = EditorBase::HandleKeyPressEvent(aKeyboardEvent);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "EditorBase::HandleKeyPressEvent() failed");
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
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "EditorBase::OnInputText() failed");
  return rv;
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

  if (!IsIMEComposing() && !IsUndoRedoEnabled() &&
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

    // XXX We should make ReplaceSelectionAsSubAction() take range.  Then,
    //     we can saving the expensive cost of modifying `Selection` here.
    if (NS_SUCCEEDED(SelectEntireDocument())) {
      DebugOnly<nsresult> rvIgnored = ReplaceSelectionAsSubAction(aString);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "EditorBase::ReplaceSelectionAsSubAction() failed, but ignored");
    }
  }

  // Destroying AutoUpdateViewBatch may cause destroying us.
  return NS_WARN_IF(Destroyed()) ? NS_ERROR_EDITOR_DESTROYED : NS_OK;
}

already_AddRefed<Element> TextEditor::GetInputEventTargetElement() const {
  nsCOMPtr<Element> target = do_QueryInterface(mEventTarget);
  return target.forget();
}

bool TextEditor::IsEmpty() const {
  // Even if there is no padding <br> element for empty editor, we should be
  // detected as empty editor if all the children are text nodes and these
  // have no content.
  Element* anonymousDivElement = GetRoot();
  if (!anonymousDivElement) {
    return true;  // Don't warn it, this is possible, e.g., 997805.html
  }

  MOZ_ASSERT(anonymousDivElement->GetFirstChild() &&
             anonymousDivElement->GetFirstChild()->IsText());

  // Only when there is non-empty text node, we are not empty.
  return !anonymousDivElement->GetFirstChild()->Length();
}

NS_IMETHODIMP TextEditor::GetTextLength(uint32_t* aCount) {
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

bool TextEditor::IsCopyToClipboardAllowedInternal() const {
  MOZ_ASSERT(IsEditActionDataAvailable());
  if (!EditorBase::IsCopyToClipboardAllowedInternal()) {
    return false;
  }

  if (!IsSingleLineEditor() || !IsPasswordEditor() ||
      NS_WARN_IF(!mPasswordMaskData)) {
    return true;
  }

  // If we're a password editor, we should allow selected text to be copied
  // to the clipboard only when selection range is in unmasked range.
  if (IsAllMasked() || IsMaskingPassword() || !UnmaskedLength()) {
    return false;
  }

  // If there are 2 or more ranges, we don't allow to copy/cut for now since
  // we need to check whether all ranges are in unmasked range or not.
  // Anyway, such operation in password field does not make sense.
  if (SelectionRef().RangeCount() > 1) {
    return false;
  }

  uint32_t selectionStart = 0, selectionEnd = 0;
  nsContentUtils::GetSelectionInTextControl(&SelectionRef(), mRootElement,
                                            selectionStart, selectionEnd);
  return UnmaskedStart() <= selectionStart && UnmaskedEnd() >= selectionEnd;
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
  MOZ_ASSERT(GetDocument());

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
  Result<nsCOMPtr<nsITransferable>, nsresult> maybeTransferable =
      EditorUtils::CreateTransferableForPlainText(*GetDocument());
  if (maybeTransferable.isErr()) {
    NS_WARNING("EditorUtils::CreateTransferableForPlainText() failed");
    return EditorBase::ToGenericNSResult(maybeTransferable.unwrapErr());
  }
  nsCOMPtr<nsITransferable> trans(maybeTransferable.unwrap());
  if (!trans) {
    NS_WARNING(
        "EditorUtils::CreateTransferableForPlainText() returned nullptr, but "
        "ignored");
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

  rv = InsertTextAsSubAction(quotedStuff, SelectionHandling::Delete);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::InsertTextAsSubAction() failed");
  return rv;
}

nsresult TextEditor::SelectEntireDocument() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<Element> anonymousDivElement = GetRoot();
  if (NS_WARN_IF(!anonymousDivElement)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<Text> text =
      Text::FromNodeOrNull(anonymousDivElement->GetFirstChild());
  MOZ_ASSERT(text);

  MOZ_TRY(SelectionRef().SetStartAndEndInLimiter(
      *text, 0, *text, text->TextDataLength(), eDirNext,
      nsISelectionListener::SELECTALL_REASON));

  return NS_OK;
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

nsresult TextEditor::SetUnmaskRangeInternal(uint32_t aStart, uint32_t aLength,
                                            uint32_t aTimeout, bool aNotify,
                                            bool aForceStartMasking) {
  if (mPasswordMaskData) {
    mPasswordMaskData->mIsMaskingPassword = aForceStartMasking || aTimeout != 0;

    // We cannot manage multiple unmasked ranges so that shrink the previous
    // range first.
    if (!IsAllMasked()) {
      mPasswordMaskData->mUnmaskedLength = 0;
      mPasswordMaskData->CancelTimer(PasswordMaskData::ReleaseTimer::No);
    }
  }

  // If we're not a password editor, return error since this call does not
  // make sense.
  if (!IsPasswordEditor() || NS_WARN_IF(!mPasswordMaskData)) {
    mPasswordMaskData->CancelTimer(PasswordMaskData::ReleaseTimer::Yes);
    return NS_ERROR_NOT_AVAILABLE;
  }

  Element* rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  Text* text = Text::FromNodeOrNull(rootElement->GetFirstChild());
  if (!text || !text->Length()) {
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
      mPasswordMaskData->mUnmaskedStart = aStart - 1;
      // If caller collapses the range, keep it.  Otherwise, expand the length.
      if (aLength > 0) {
        ++aLength;
      }
    } else {
      mPasswordMaskData->mUnmaskedStart = aStart;
    }
    mPasswordMaskData->mUnmaskedLength =
        std::min(valueLength - UnmaskedStart(), aLength);
    // If unmasked end is middle of a surrogate pair, expand it to include
    // the following low surrogate because the caller may want to show a
    // character after the character at `aStart + aLength`.
    if (UnmaskedEnd() < valueLength &&
        textFragment->IsLowSurrogateFollowingHighSurrogateAt(UnmaskedEnd())) {
      mPasswordMaskData->mUnmaskedLength++;
    }
    // If it's first time to mask the unmasking characters with timer, create
    // the timer now.  Then, we'll keep using it for saving the creation cost.
    if (!HasAutoMaskingTimer() && aLength && aTimeout && UnmaskedLength()) {
      mPasswordMaskData->mTimer = NS_NewTimer();
    }
  } else {
    if (NS_WARN_IF(aLength != 0)) {
      return NS_ERROR_INVALID_ARG;
    }
    mPasswordMaskData->MaskAll();
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
    MOZ_ASSERT(HasAutoMaskingTimer());
    DebugOnly<nsresult> rvIgnored = mPasswordMaskData->mTimer->InitWithCallback(
        this, aTimeout, nsITimer::TYPE_ONE_SHOT);
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
  if (!IsPasswordEditor() || NS_WARN_IF(!mPasswordMaskData)) {
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

  if (!IsPasswordEditor() || NS_WARN_IF(!mPasswordMaskData) || IsAllMasked()) {
    return;
  }

  // Adjust unmasked range before deletion since DOM mutation may cause
  // layout referring the range in old text.

  // If we need to mask automatically, mask all now.
  if (IsMaskingPassword()) {
    DebugOnly<nsresult> rvIgnored = MaskAllCharacters();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "TextEditor::MaskAllCharacters() failed, but ignored");
    return;
  }

  if (aRemoveStartOffset < UnmaskedStart()) {
    // If removing range is before the unmasked range, move it.
    if (aRemoveStartOffset + aRemoveLength <= UnmaskedStart()) {
      DebugOnly<nsresult> rvIgnored =
          SetUnmaskRange(UnmaskedStart() - aRemoveLength, UnmaskedLength());
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "TextEditor::SetUnmaskRange() failed, but ignored");
      return;
    }

    // If removing range starts before unmasked range, and ends in unmasked
    // range, move and shrink the range.
    if (aRemoveStartOffset + aRemoveLength < UnmaskedEnd()) {
      uint32_t unmaskedLengthInRemovingRange =
          aRemoveStartOffset + aRemoveLength - UnmaskedStart();
      DebugOnly<nsresult> rvIgnored = SetUnmaskRange(
          aRemoveStartOffset, UnmaskedLength() - unmaskedLengthInRemovingRange);
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
          SetUnmaskRange(UnmaskedStart(), UnmaskedLength() - aRemoveLength);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "TextEditor::SetUnmaskRange() failed, but ignored");
      return;
    }

    // If removing range starts from unmasked range, and ends after it,
    // shrink it.
    DebugOnly<nsresult> rvIgnored =
        SetUnmaskRange(UnmaskedStart(), aRemoveStartOffset - UnmaskedStart());
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

  if (!IsPasswordEditor() || NS_WARN_IF(!mPasswordMaskData) || IsAllMasked()) {
    return NS_OK;
  }

  if (IsMaskingPassword()) {
    // If we need to mask password, mask all right now.
    nsresult rv = MaskAllCharactersAndNotify();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "TextEditor::MaskAllCharacters() failed");
    return rv;
  }

  if (aInsertedOffset < UnmaskedStart()) {
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
    nsresult rv = SetUnmaskRangeAndNotify(UnmaskedStart(),
                                          UnmaskedLength() + aInsertedLength);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "TextEditor::SetUnmaskRangeAndNotify() failed");
    return rv;
  }

  // If insertion point is after unmasked range, extend the unmask range to
  // include the new text.
  nsresult rv = SetUnmaskRangeAndNotify(
      UnmaskedStart(), aInsertedOffset + aInsertedLength - UnmaskedStart());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TextEditor::SetUnmaskRangeAndNotify() failed");
  return rv;
}

}  // namespace mozilla
