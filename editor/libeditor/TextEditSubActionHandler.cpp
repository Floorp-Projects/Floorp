/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextEditor.h"

#include "mozilla/Assertions.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextComposition.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/NodeFilterBinding.h"
#include "mozilla/dom/NodeIterator.h"
#include "mozilla/dom/Selection.h"
#include "nsAString.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsNameSpaceManager.h"
#include "nsINode.h"
#include "nsISupportsBase.h"
#include "nsLiteralString.h"
#include "nsTextNode.h"
#include "nsUnicharUtils.h"
#include "nsIHTMLCollection.h"
#include "nsPrintfCString.h"

namespace mozilla {

using namespace dom;

#define CANCEL_OPERATION_AND_RETURN_EDIT_ACTION_RESULT_IF_READONLY \
  if (IsReadonly()) {                                              \
    return EditActionCanceled(NS_OK);                              \
  }

nsresult TextEditor::InitEditorContentAndSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsresult rv = MaybeCreatePaddingBRElementForEmptyEditor();
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "EditorBase::MaybeCreatePaddingBRElementForEmptyEditor() failed");
    return rv;
  }

  // If the selection hasn't been set up yet, set it up collapsed to the end of
  // our editable content.
  // XXX I think that this shouldn't do it in `HTMLEditor` because it maybe
  //     removed by the web app and if they call `Selection::AddRange()`,
  //     it may cause multiple selection ranges.
  if (!SelectionRefPtr()->RangeCount()) {
    nsresult rv = CollapseSelectionToEnd();
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::CollapseSelectionToEnd() failed");
      return rv;
    }
  }

  if (IsPlaintextEditor() && !IsSingleLineEditor()) {
    nsresult rv = EnsurePaddingBRElementInMultilineEditor();
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "TextEditor::EnsurePaddingBRElementInMultilineEditor() failed");
      return rv;
    }
  }

  return NS_OK;
}

void TextEditor::OnStartToHandleTopLevelEditSubAction(
    EditSubAction aTopLevelEditSubAction,
    nsIEditor::EDirection aDirectionOfTopLevelEditSubAction, ErrorResult& aRv) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!AsHTMLEditor());
  MOZ_ASSERT(!aRv.Failed());

  EditorBase::OnStartToHandleTopLevelEditSubAction(
      aTopLevelEditSubAction, aDirectionOfTopLevelEditSubAction, aRv);

  MOZ_ASSERT(GetTopLevelEditSubAction() == aTopLevelEditSubAction);
  MOZ_ASSERT(GetDirectionOfTopLevelEditSubAction() ==
             aDirectionOfTopLevelEditSubAction);

  if (NS_WARN_IF(Destroyed())) {
    aRv.Throw(NS_ERROR_EDITOR_DESTROYED);
    return;
  }

  if (NS_WARN_IF(!mInitSucceeded)) {
    return;
  }

  if (aTopLevelEditSubAction == EditSubAction::eSetText) {
    // SetText replaces all text, so spell checker handles starting from the
    // start of new value.
    SetSpellCheckRestartPoint(EditorDOMPoint(mRootElement, 0));
    return;
  }

  if (aTopLevelEditSubAction == EditSubAction::eInsertText ||
      aTopLevelEditSubAction == EditSubAction::eInsertTextComingFromIME) {
    // For spell checker, previous selected node should be text node if
    // possible. If anchor is root of editor, it may become invalid offset
    // after inserting text.
    EditorRawDOMPoint point = FindBetterInsertionPoint(
        EditorRawDOMPoint(SelectionRefPtr()->AnchorRef()));
    NS_WARNING_ASSERTION(
        point.IsSet(),
        "EditorBase::FindBetterInsertionPoint() failed, but ignored");
    if (point.IsSet()) {
      SetSpellCheckRestartPoint(point);
      return;
    }
  }
  if (SelectionRefPtr()->AnchorRef().IsSet()) {
    SetSpellCheckRestartPoint(
        EditorRawDOMPoint(SelectionRefPtr()->AnchorRef()));
  }
}

nsresult TextEditor::OnEndHandlingTopLevelEditSubAction() {
  MOZ_ASSERT(IsTopLevelEditSubActionDataAvailable());
  MOZ_ASSERT(!AsHTMLEditor());

  nsresult rv;
  while (true) {
    if (NS_WARN_IF(Destroyed())) {
      rv = NS_ERROR_EDITOR_DESTROYED;
      break;
    }

    // XXX Probably, we should spellcheck again after edit action (not top-level
    //     sub-action) is handled because the ranges can be referred only by
    //     users.
    if (NS_FAILED(rv = HandleInlineSpellCheckAfterEdit())) {
      NS_WARNING("TextEditor::HandleInlineSpellCheckAfterEdit() failed");
      break;
    }

    if (NS_FAILED(rv = EnsurePaddingBRElementForEmptyEditor())) {
      NS_WARNING("TextEditor::EnsurePaddingBRElementForEmptyEditor() failed");
      break;
    }

    if (!IsSingleLineEditor() &&
        NS_FAILED(rv = EnsurePaddingBRElementInMultilineEditor())) {
      NS_WARNING(
          "TextEditor::EnsurePaddingBRElementInMultilineEditor() failed");
      break;
    }

    rv = EnsureCaretNotAtEndOfTextNode();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      break;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "TextEditor::EnsureCaretNotAtEndOfTextNode() failed, but ignored");
    rv = NS_OK;
    break;
  }
  DebugOnly<nsresult> rvIgnored =
      EditorBase::OnEndHandlingTopLevelEditSubAction();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "EditorBase::OnEndHandlingTopLevelEditSubAction() failed, but ignored");
  MOZ_ASSERT(!GetTopLevelEditSubAction());
  MOZ_ASSERT(GetDirectionOfTopLevelEditSubAction() == eNone);
  return rv;
}

EditActionResult TextEditor::InsertLineFeedCharacterAtSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!AsHTMLEditor());
  MOZ_ASSERT(!IsSingleLineEditor());

  UndefineCaretBidiLevel();

  CANCEL_OPERATION_AND_RETURN_EDIT_ACTION_RESULT_IF_READONLY

  if (mMaxTextLength >= 0) {
    nsAutoString insertionString(NS_LITERAL_STRING("\n"));
    EditActionResult result =
        TruncateInsertionStringForMaxLength(insertionString);
    if (result.Failed()) {
      NS_WARNING("TextEditor::TruncateInsertionStringForMaxLength() failed");
      return result;
    }
    if (result.Handled()) {
      // Don't return as handled since we stopped inserting the line break.
      return EditActionCanceled();
    }
  }

  // if the selection isn't collapsed, delete it.
  if (!SelectionRefPtr()->IsCollapsed()) {
    nsresult rv =
        DeleteSelectionAsSubAction(nsIEditor::eNone, nsIEditor::eStrip);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "TextEditor::DeleteSelectionAsSubAction(eNone, eStrip) failed");
      return EditActionIgnored(rv);
    }
  }

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::EnsureNoPaddingBRElementForEmptyEditor() failed");
    return EditActionIgnored(rv);
  }

  // get the (collapsed) selection location
  nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return EditActionIgnored(NS_ERROR_FAILURE);
  }

  EditorRawDOMPoint pointToInsert(firstRange->StartRef());
  if (NS_WARN_IF(!pointToInsert.IsSet())) {
    return EditActionIgnored(NS_ERROR_FAILURE);
  }
  MOZ_ASSERT(pointToInsert.IsSetAndValid());
  MOZ_ASSERT(!pointToInsert.IsContainerHTMLElement(nsGkAtoms::br));

  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return EditActionIgnored(NS_ERROR_NOT_INITIALIZED);
  }

  // Don't change my selection in sub-transactions.
  AutoTransactionsConserveSelection dontChangeMySelection(*this);

  // Insert a linefeed character.
  EditorRawDOMPoint pointAfterInsertedLineFeed;
  rv = InsertTextWithTransaction(*document, NS_LITERAL_STRING("\n"),
                                 pointToInsert, &pointAfterInsertedLineFeed);
  if (!pointAfterInsertedLineFeed.IsSet()) {
    NS_WARNING(
        "EditorBase::InsertTextWithTransaction(\\n) didn't return position of "
        "inserted linefeed");
    return EditActionIgnored(NS_ERROR_FAILURE);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("TextEditor::InsertTextWithTransaction(\\n) failed");
    return EditActionIgnored(rv);
  }

  // set the selection to the correct location
  MOZ_ASSERT(
      !pointAfterInsertedLineFeed.GetChild(),
      "After inserting text into a text node, pointAfterInsertedLineFeed."
      "GetChild() should be nullptr");
  rv = SelectionRefPtr()->Collapse(pointAfterInsertedLineFeed);
  if (NS_FAILED(rv)) {
    NS_WARNING("Selection::Collapse() failed");
    return EditActionIgnored(rv);
  }

  // XXX I don't think we still need this.  This must have been required when
  //     `<textarea>` was implemented with text nodes and `<br>` elements.
  // see if we're at the end of the editor range
  EditorRawDOMPoint endPoint(EditorBase::GetEndPoint(*SelectionRefPtr()));
  if (endPoint == pointAfterInsertedLineFeed) {
    // SetInterlinePosition(true) means we want the caret to stick to the
    // content on the "right".  We want the caret to stick to whatever is
    // past the break.  This is because the break is on the same line we
    // were on, but the next content will be on the following line.
    IgnoredErrorResult ignoredError;
    SelectionRefPtr()->SetInterlinePosition(true, ignoredError);
    NS_WARNING_ASSERTION(
        !ignoredError.Failed(),
        "Selection::SetInterlinePosition(true) failed, but ignored");
  }

  return EditActionHandled();
}

nsresult TextEditor::EnsureCaretNotAtEndOfTextNode() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(IsPlaintextEditor());

  // If there is no selection ranges, we should set to the end of the editor.
  // This is usually performed in InitEditorContentAndSelection(), however,
  // if the editor is reframed, this may be called by
  // OnEndHandlingTopLevelEditSubAction().
  if (!SelectionRefPtr()->RangeCount()) {
    DebugOnly<nsresult> rvIgnored = CollapseSelectionToEnd();
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "EditorBase::CollapseSelectionToEnd() failed, but ignored");
  }

  // If we are at the end of the <textarea> element, we need to set the
  // selection to stick to the padding <br> element for empty last line at the
  // end of the <textarea>.
  EditorRawDOMPoint selectionStartPoint(
      EditorBase::GetStartPoint(*SelectionRefPtr()));
  if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // Nothing to do if we're not at the end of the text node.
  if (!selectionStartPoint.IsInTextNode() ||
      !selectionStartPoint.IsEndOfContainer()) {
    return NS_OK;
  }

  Element* anonymousDivElement = GetRoot();
  if (NS_WARN_IF(!anonymousDivElement)) {
    return NS_ERROR_NULL_POINTER;
  }
  nsINode* parentNode = selectionStartPoint.GetContainer()->GetParentNode();
  if (parentNode != anonymousDivElement) {
    return NS_OK;
  }

  nsINode* nextNode = selectionStartPoint.GetContainer()->GetNextSibling();
  if (!nextNode || !EditorBase::IsPaddingBRElementForEmptyLastLine(*nextNode)) {
    return NS_OK;
  }

  EditorRawDOMPoint afterStartContainer(
      EditorRawDOMPoint::After(*selectionStartPoint.GetContainer()));
  if (NS_WARN_IF(!afterStartContainer.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  IgnoredErrorResult ignoredError;
  SelectionRefPtr()->Collapse(afterStartContainer, ignoredError);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(!ignoredError.Failed(), "Selection::Collapse() failed");
  return ignoredError.StealNSResult();
}

void TextEditor::HandleNewLinesInStringForSingleLineEditor(
    nsString& aString) const {
  static const char16_t kLF = static_cast<char16_t>('\n');
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(IsPlaintextEditor());
  MOZ_ASSERT(aString.FindChar(static_cast<uint16_t>('\r')) == kNotFound);

  // First of all, check if aString contains '\n' since if the string
  // does not include it, we don't need to do nothing here.
  int32_t firstLF = aString.FindChar(kLF, 0);
  if (firstLF == kNotFound) {
    return;
  }

  switch (mNewlineHandling) {
    case nsIEditor::eNewlinesReplaceWithSpaces:
      // Default of Firefox:
      // Strip trailing newlines first so we don't wind up with trailing spaces
      aString.Trim(LFSTR, false, true);
      aString.ReplaceChar(kLF, ' ');
      break;
    case nsIEditor::eNewlinesStrip:
      aString.StripChar(kLF);
      break;
    case nsIEditor::eNewlinesPasteToFirst:
    default: {
      // we get first *non-empty* line.
      int32_t offset = 0;
      while (firstLF == offset) {
        offset++;
        firstLF = aString.FindChar(kLF, offset);
      }
      if (firstLF > 0) {
        aString.Truncate(firstLF);
      }
      if (offset > 0) {
        aString.Cut(0, offset);
      }
      break;
    }
    case nsIEditor::eNewlinesReplaceWithCommas:
      // Default of Thunderbird:
      aString.Trim(LFSTR, true, true);
      aString.ReplaceChar(kLF, ',');
      break;
    case nsIEditor::eNewlinesStripSurroundingWhitespace: {
      nsAutoString result;
      uint32_t offset = 0;
      while (offset < aString.Length()) {
        int32_t nextLF = !offset ? firstLF : aString.FindChar(kLF, offset);
        if (nextLF < 0) {
          result.Append(nsDependentSubstring(aString, offset));
          break;
        }
        uint32_t wsBegin = nextLF;
        // look backwards for the first non-whitespace char
        while (wsBegin > offset && NS_IS_SPACE(aString[wsBegin - 1])) {
          --wsBegin;
        }
        result.Append(nsDependentSubstring(aString, offset, wsBegin - offset));
        offset = nextLF + 1;
        while (offset < aString.Length() && NS_IS_SPACE(aString[offset])) {
          ++offset;
        }
      }
      aString = result;
      break;
    }
    case nsIEditor::eNewlinesPasteIntact:
      // even if we're pasting newlines, don't paste leading/trailing ones
      aString.Trim(LFSTR, true, true);
      break;
  }
}

EditActionResult TextEditor::HandleInsertText(
    EditSubAction aEditSubAction, const nsAString& aInsertionString) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(IsTextEditor());
  MOZ_ASSERT(aEditSubAction == EditSubAction::eInsertText ||
             aEditSubAction == EditSubAction::eInsertTextComingFromIME);

  UndefineCaretBidiLevel();

  if (aInsertionString.IsEmpty() &&
      aEditSubAction != EditSubAction::eInsertTextComingFromIME) {
    // HACK: this is a fix for bug 19395
    // I can't outlaw all empty insertions
    // because IME transaction depend on them
    // There is more work to do to make the
    // world safe for IME.
    return EditActionCanceled();
  }

  nsAutoString insertionString(aInsertionString);
  if (mMaxTextLength >= 0) {
    EditActionResult result =
        TruncateInsertionStringForMaxLength(insertionString);
    if (result.Failed()) {
      NS_WARNING("TextEditor::TruncateInsertionStringForMaxLength() failed");
      return result.MarkAsHandled();
    }
    // If we're exceeding the maxlength when composing IME, we need to clean up
    // the composing text, so we shouldn't return early.
    if (result.Handled() && insertionString.IsEmpty() &&
        aEditSubAction != EditSubAction::eInsertTextComingFromIME) {
      return EditActionCanceled();
    }
  }

  uint32_t start = 0;
  if (IsPasswordEditor()) {
    if (GetComposition() && !GetComposition()->String().IsEmpty()) {
      start = GetComposition()->XPOffsetInTextNode();
    } else {
      uint32_t end = 0;
      nsContentUtils::GetSelectionInTextControl(SelectionRefPtr(), GetRoot(),
                                                start, end);
    }
  }

  // if the selection isn't collapsed, delete it.
  if (!SelectionRefPtr()->IsCollapsed()) {
    nsresult rv =
        DeleteSelectionAsSubAction(nsIEditor::eNone, nsIEditor::eStrip);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "TextEditor::DeleteSelectionAsSubAction(eNone, eStrip) failed");
      return EditActionHandled(rv);
    }
  }

  // XXX Why don't we cancel here?  Shouldn't we do this first?
  CANCEL_OPERATION_AND_RETURN_EDIT_ACTION_RESULT_IF_READONLY

  MaybeDoAutoPasswordMasking();

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::EnsureNoPaddingBRElementForEmptyEditor() failed");
    return EditActionHandled(rv);
  }

  // People have lots of different ideas about what text fields
  // should do with multiline pastes.  See bugs 21032, 23485, 23485, 50935.
  // The six possible options are:
  // 0. paste newlines intact
  // 1. paste up to the first newline (default)
  // 2. replace newlines with spaces
  // 3. strip newlines
  // 4. replace with commas
  // 5. strip newlines and surrounding whitespace
  // So find out what we're expected to do:
  if (IsSingleLineEditor()) {
    // XXX Some callers of TextEditor::InsertTextAsAction()  already make the
    //     string use only \n as a linebreaker.  However, they are not hot
    //     path and nsContentUtils::PlatformToDOMLineBreaks() does nothing
    //     if the string doesn't include \r.  So, let's convert linebreakers
    //     here.  Note that there are too many callers of
    //     TextEditor::InsertTextAsAction().  So, it's difficult to keep
    //     maintaining all of them won't reach here without \r nor \r\n.
    // XXX Should we handle do this before truncating the string for
    //     `maxlength`?
    nsContentUtils::PlatformToDOMLineBreaks(insertionString);
    HandleNewLinesInStringForSingleLineEditor(insertionString);
  }

  // get the (collapsed) selection location
  nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return EditActionHandled(NS_ERROR_FAILURE);
  }
  EditorRawDOMPoint atStartOfSelection(firstRange->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSetAndValid())) {
    return EditActionHandled(NS_ERROR_FAILURE);
  }
  MOZ_ASSERT(!atStartOfSelection.IsContainerHTMLElement(nsGkAtoms::br));

  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return EditActionHandled(NS_ERROR_NOT_INITIALIZED);
  }

  if (aEditSubAction == EditSubAction::eInsertTextComingFromIME) {
    EditorRawDOMPoint compositionStartPoint = GetCompositionStartPoint();
    if (!compositionStartPoint.IsSet()) {
      compositionStartPoint = FindBetterInsertionPoint(atStartOfSelection);
      NS_WARNING_ASSERTION(
          compositionStartPoint.IsSet(),
          "EditorBase::FindBetterInsertionPoint() failed, but ignored");
    }
    nsresult rv = InsertTextWithTransaction(*document, insertionString,
                                            compositionStartPoint);
    if (NS_WARN_IF(Destroyed())) {
      return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InsertTextWithTransaction() failed");
      return EditActionHandled(rv);
    }
  } else {
    MOZ_ASSERT(aEditSubAction == EditSubAction::eInsertText);

    // don't change my selection in subtransactions
    AutoTransactionsConserveSelection dontChangeMySelection(*this);

    EditorRawDOMPoint pointAfterStringInserted;
    nsresult rv = InsertTextWithTransaction(*document, insertionString,
                                            atStartOfSelection,
                                            &pointAfterStringInserted);
    if (NS_WARN_IF(Destroyed())) {
      return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InsertTextWithTransaction() failed");
      return EditActionHandled(rv);
    }

    if (pointAfterStringInserted.IsSet()) {
      // Make the caret attach to the inserted text, unless this text ends with
      // a LF, in which case make the caret attach to the next line.
      bool endsWithLF =
          !insertionString.IsEmpty() && insertionString.Last() == nsCRT::LF;
      IgnoredErrorResult ignoredError;
      SelectionRefPtr()->SetInterlinePosition(endsWithLF, ignoredError);
      NS_WARNING_ASSERTION(
          !ignoredError.Failed(),
          "Selection::SetInterlinePosition() failed, but ignored");

      MOZ_ASSERT(
          !pointAfterStringInserted.GetChild(),
          "After inserting text into a text node, pointAfterStringInserted."
          "GetChild() should be nullptr");
      ignoredError = IgnoredErrorResult();
      SelectionRefPtr()->Collapse(pointAfterStringInserted, ignoredError);
      if (NS_WARN_IF(Destroyed())) {
        return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(!ignoredError.Failed(),
                           "Selection::Collapse() failed, but ignored");
    }
  }

  // Unmask inputted character(s) if necessary.
  if (IsPasswordEditor() && IsMaskingPassword() && CanEchoPasswordNow()) {
    nsresult rv = SetUnmaskRangeAndNotify(start, insertionString.Length(),
                                          LookAndFeel::GetPasswordMaskDelay());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "TextEditor::SetUnmaskRangeAndNotify() failed");
    return EditActionHandled(rv);
  }

  return EditActionHandled();
}

EditActionResult TextEditor::SetTextWithoutTransaction(
    const nsAString& aValue) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!AsHTMLEditor());
  MOZ_ASSERT(IsPlaintextEditor());
  MOZ_ASSERT(!IsIMEComposing());
  MOZ_ASSERT(!IsUndoRedoEnabled());
  MOZ_ASSERT(GetEditAction() != EditAction::eReplaceText);
  MOZ_ASSERT(mMaxTextLength < 0);
  MOZ_ASSERT(aValue.FindChar(static_cast<char16_t>('\r')) == kNotFound);

  UndefineCaretBidiLevel();

  // XXX If we're setting value, shouldn't we keep setting the new value here?
  CANCEL_OPERATION_AND_RETURN_EDIT_ACTION_RESULT_IF_READONLY

  MaybeDoAutoPasswordMasking();

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::EnsureNoPaddingBRElementForEmptyEditor() failed");
    return EditActionResult(rv);
  }

  RefPtr<Element> anonymousDivElement = GetRoot();
  nsIContent* firstChild = anonymousDivElement->GetFirstChild();

  // We can use this fast path only when:
  //  - we need to insert a text node.
  //  - we need to replace content of existing text node.
  // Additionally, for avoiding odd result, we should check whether we're in
  // usual condition.
  if (IsSingleLineEditor()) {
    // If we're a single line text editor, i.e., <input>, there is only padding
    // <br> element.  Otherwise, there should be only one text node.  But note
    // that even if there is a padding <br> element for empty editor, it's
    // already been removed by `EnsureNoPaddingBRElementForEmptyEditor()`.  So,
    // at here, there should be only one text node or no children.
    if (firstChild && (!firstChild->IsText() || firstChild->GetNextSibling())) {
      return EditActionIgnored();
    }
  } else {
    // If we're a multiline text editor, i.e., <textarea>, there is a padding
    // <br> element for empty last line followed by scrollbar/resizer elements.
    // Otherwise, a text node is followed by them.
    if (!firstChild) {
      return EditActionIgnored();
    }
    if (firstChild->IsText()) {
      if (!firstChild->GetNextSibling() ||
          !EditorBase::IsPaddingBRElementForEmptyLastLine(
              *firstChild->GetNextSibling())) {
        return EditActionIgnored();
      }
    } else if (!EditorBase::IsPaddingBRElementForEmptyLastLine(*firstChild)) {
      return EditActionIgnored();
    }
  }

  // XXX Password fields accept line breaks as normal characters with this code.
  //     Is this intentional?
  nsAutoString sanitizedValue(aValue);
  if (IsSingleLineEditor() && !IsPasswordEditor()) {
    HandleNewLinesInStringForSingleLineEditor(sanitizedValue);
  }

  if (!firstChild || !firstChild->IsText()) {
    if (sanitizedValue.IsEmpty()) {
      return EditActionHandled();
    }
    RefPtr<Document> document = GetDocument();
    if (NS_WARN_IF(!document)) {
      return EditActionIgnored();
    }
    RefPtr<nsTextNode> newTextNode = CreateTextNode(sanitizedValue);
    if (!newTextNode) {
      NS_WARNING("EditorBase::CreateTextNode() failed");
      return EditActionIgnored();
    }
    nsresult rv = InsertNodeWithTransaction(
        *newTextNode, EditorDOMPoint(anonymousDivElement, 0));
    if (NS_WARN_IF(Destroyed())) {
      return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::InsertNodeWithTransaction() failed");
      return EditActionResult(rv);
    }
    return EditActionHandled();
  }

  // TODO: If new value is empty string, we should only remove it.
  RefPtr<Text> textNode = firstChild->GetAsText();
  if (MOZ_UNLIKELY(!textNode)) {
    NS_WARNING("The first child was not a text node");
    return EditActionIgnored();
  }
  rv = SetTextNodeWithoutTransaction(sanitizedValue, *textNode);
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::SetTextNodeWithoutTransaction() failed");
    return EditActionResult(rv);
  }

  // If we replaced non-empty value with empty string, we need to delete the
  // text node.
  if (sanitizedValue.IsEmpty() && !textNode->Length()) {
    nsresult rv = DeleteNodeWithTransaction(*textNode);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "EditorBase::DeleteNodeWithTransaction() failed, but ignored");
    // XXX I don't think this is necessary because the anonymous `<div>`
    //     element has now only padding `<br>` element even if there are
    //     something.
    IgnoredErrorResult ignoredError;
    SelectionRefPtr()->SetInterlinePosition(true, ignoredError);
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "Selection::SetInterlinePoisition(true) failed");
  }

  return EditActionHandled();
}

EditActionResult TextEditor::HandleDeleteSelection(
    nsIEditor::EDirection aDirectionAndAmount,
    nsIEditor::EStripWrappers aStripWrappers) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  UndefineCaretBidiLevel();

  CANCEL_OPERATION_AND_RETURN_EDIT_ACTION_RESULT_IF_READONLY

  // if there is only padding <br> element for empty editor, cancel the
  // operation.
  if (mPaddingBRElementForEmptyEditor) {
    return EditActionCanceled();
  }
  EditActionResult result =
      HandleDeleteSelectionInternal(aDirectionAndAmount, aStripWrappers);
  // HandleDeleteSelectionInternal() creates SelectionBatcher.  Therefore,
  // quitting from it might cause having destroyed the editor.
  if (NS_WARN_IF(Destroyed())) {
    return result.SetResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(result.Succeeded(),
                       "TextEditor::HandleDeleteSelectionInternal() failed");
  return result;
}

EditActionResult TextEditor::HandleDeleteSelectionInternal(
    nsIEditor::EDirection aDirectionAndAmount,
    nsIEditor::EStripWrappers aStripWrappers) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!AsHTMLEditor());

  // If the current selection is empty (e.g the user presses backspace with
  // a collapsed selection), then we want to avoid sending the selectstart
  // event to the user, so we hide selection changes. However, we still
  // want to send a single selectionchange event to the document, so we
  // batch the selectionchange events, such that a single event fires after
  // the AutoHideSelectionChanges destructor has been run.
  SelectionBatcher selectionBatcher(SelectionRefPtr());
  AutoHideSelectionChanges hideSelection(SelectionRefPtr());
  nsAutoScriptBlocker scriptBlocker;

  if (IsPasswordEditor() && IsMaskingPassword()) {
    MaskAllCharacters();
  } else {
    EditorRawDOMPoint selectionStartPoint(
        EditorBase::GetStartPoint(*SelectionRefPtr()));
    if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
      return EditActionResult(NS_ERROR_FAILURE);
    }

    if (!SelectionRefPtr()->IsCollapsed()) {
      nsresult rv =
          DeleteSelectionWithTransaction(aDirectionAndAmount, aStripWrappers);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "TextEditor::DeleteSelectionWithTransaction() failed");
      return EditActionHandled(rv);
    }

    // Test for distance between caret and text that will be deleted
    EditActionResult result =
        SetCaretBidiLevelForDeletion(selectionStartPoint, aDirectionAndAmount);
    if (result.Failed() || result.Canceled()) {
      NS_WARNING_ASSERTION(result.Succeeded(),
                           "EditorBase::SetCaretBidiLevelForDeletion() failed");
      return result;
    }
  }

  nsresult rv = ExtendSelectionForDelete(&aDirectionAndAmount);
  if (NS_FAILED(rv)) {
    NS_WARNING("TextEditor::ExtendSelectionForDelete() failed");
    return EditActionResult(rv);
  }

  rv = DeleteSelectionWithTransaction(aDirectionAndAmount, aStripWrappers);
  if (NS_WARN_IF(Destroyed())) {
    return EditActionResult(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TextEditor::DeleteSelectionWithTransaction() failed");
  return EditActionHandled(rv);
}

EditActionResult TextEditor::ComputeValueFromTextNodeAndPaddingBRElement(
    nsAString& aValue) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // If there is a padding <br> element, there's no content.  So output empty
  // string.
  if (mPaddingBRElementForEmptyEditor) {
    aValue.Truncate();
    return EditActionHandled();
  }

  // If it's neither <input type="text"> nor <textarea>, e.g., an HTML editor
  // which is in plaintext mode (e.g., plaintext email composer on Thunderbird),
  // it should be handled by the expensive path.
  if (AsHTMLEditor()) {
    return EditActionIgnored();
  }

  Element* anonymousDivElement = GetRoot();
  if (!anonymousDivElement) {
    // Don't warn this case, this is possible, e.g., 997805.html
    aValue.Truncate();
    return EditActionHandled();
  }

  nsIContent* textNodeOrPaddingBRElement = anonymousDivElement->GetFirstChild();
  if (!textNodeOrPaddingBRElement) {
    aValue.Truncate();
    return EditActionHandled();
  }

  // If it's an <input type="text"> element, the DOM tree should be:
  // <div (::-moz-text-control-editing-root)>
  //   #text
  // </div>
  //
  // If it's a <textarea> element, the DOM tree should be:
  // <div (::-moz-text-control-editing-root)>
  //   #text (if there is)
  //   <br type="_moz">
  //   <scrollbar orient="horizontal">
  //   ...
  // </div>

  Text* textNode = textNodeOrPaddingBRElement->GetAsText();
  if (!textNode) {
    // If there is no text node in the expected DOM tree, we can say that it's
    // just empty.
    aValue.Truncate();
    return EditActionHandled();
  }

  nsIContent* firstChildExceptText =
      textNode ? textNodeOrPaddingBRElement->GetNextSibling()
               : textNodeOrPaddingBRElement;
  // If the DOM tree is unexpected, fall back to the expensive path.
  bool isInput = IsSingleLineEditor();
  bool isTextarea = !isInput;
  if (NS_WARN_IF(isInput && firstChildExceptText) ||
      NS_WARN_IF(isTextarea && !firstChildExceptText) ||
      NS_WARN_IF(isTextarea &&
                 !EditorBase::IsPaddingBRElementForEmptyLastLine(
                     *firstChildExceptText) &&
                 !firstChildExceptText->IsXULElement(nsGkAtoms::scrollbar))) {
    return EditActionIgnored();
  }

  // Otherwise, the text data is the value.
  textNode->GetData(aValue);
  return EditActionHandled();
}

nsresult TextEditor::EnsurePaddingBRElementInMultilineEditor() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(IsPlaintextEditor());
  MOZ_ASSERT(!IsSingleLineEditor());

  Element* anonymousDivElement = GetRoot();
  if (NS_WARN_IF(!anonymousDivElement)) {
    return NS_ERROR_FAILURE;
  }

  // Assuming EditorBase::MaybeCreatePaddingBRElementForEmptyEditor() has been
  // called first.
  // XXX This assumption is wrong.  This method may be called alone.  Actually,
  //     we see this warning in mochitest log.  So, we should fix this bug
  //     later.
  if (NS_WARN_IF(!anonymousDivElement->GetLastChild())) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<HTMLBRElement> brElement =
      HTMLBRElement::FromNode(anonymousDivElement->GetLastChild());
  if (!brElement) {
    AutoTransactionsConserveSelection dontChangeMySelection(*this);
    EditorDOMPoint endOfAnonymousDiv(
        EditorDOMPoint::AtEndOf(*anonymousDivElement));
    CreateElementResult createPaddingBRResult =
        InsertPaddingBRElementForEmptyLastLineWithTransaction(
            endOfAnonymousDiv);
    NS_WARNING_ASSERTION(
        createPaddingBRResult.Succeeded(),
        "EditorBase::InsertPaddingBRElementForEmptyLastLineWithTransaction() "
        "failed");
    return createPaddingBRResult.Rv();
  }

  // Check to see if the trailing BR is a former padding <br> element for empty
  // editor - this will have stuck around if we previously morphed a trailing
  // node into a padding <br> element.
  if (!brElement->IsPaddingForEmptyEditor()) {
    return NS_OK;
  }

  // Morph it back to a padding <br> element for empty last line.
  brElement->UnsetFlags(NS_PADDING_FOR_EMPTY_EDITOR);
  brElement->SetFlags(NS_PADDING_FOR_EMPTY_LAST_LINE);

  return NS_OK;
}

EditActionResult TextEditor::TruncateInsertionStringForMaxLength(
    nsAString& aInsertionString) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(mMaxTextLength >= 0);

  if (!IsPlaintextEditor() || IsIMEComposing()) {
    return EditActionIgnored();
  }

  int32_t currentLength = INT32_MAX;
  nsresult rv = GetTextLength(&currentLength);
  if (NS_FAILED(rv)) {
    NS_WARNING("TextEditor::GetTextLength() failed");
    return EditActionResult(rv);
  }

  uint32_t selectionStart, selectionEnd;
  nsContentUtils::GetSelectionInTextControl(SelectionRefPtr(), GetRoot(),
                                            selectionStart, selectionEnd);

  TextComposition* composition = GetComposition();
  const uint32_t kOldCompositionStringLength =
      composition ? composition->String().Length() : 0;

  const uint32_t kSelectionLength = selectionEnd - selectionStart;
  // XXX This computation must be wrong.  If we'll support non-collapsed
  //     selection even during composition for Korean IME, kSelectionLength
  //     is part of kOldCompositionStringLength.
  const uint32_t kNewLength =
      currentLength - kSelectionLength - kOldCompositionStringLength;
  if (kNewLength >= static_cast<uint32_t>(mMaxTextLength)) {
    aInsertionString.Truncate();  // Too long, we cannot accept new character.
    return EditActionHandled();
  }

  if (aInsertionString.Length() + kNewLength <=
      static_cast<uint32_t>(mMaxTextLength)) {
    return EditActionIgnored();  // Enough short string.
  }

  int32_t newInsertionStringLength = mMaxTextLength - kNewLength;
  MOZ_ASSERT(newInsertionStringLength > 0);
  char16_t maybeHighSurrogate =
      aInsertionString.CharAt(newInsertionStringLength - 1);
  char16_t maybeLowSurrogate =
      aInsertionString.CharAt(newInsertionStringLength);
  // Don't split the surrogate pair.
  if (NS_IS_SURROGATE_PAIR(maybeHighSurrogate, maybeLowSurrogate)) {
    newInsertionStringLength--;
  }
  // XXX What should we do if we're removing IVS but its preceding
  //     character won't be removed?
  aInsertionString.Truncate(newInsertionStringLength);
  return EditActionHandled();
}

bool TextEditor::CanEchoPasswordNow() const {
  if (!LookAndFeel::GetEchoPassword() ||
      (mFlags & nsIEditor::eEditorDontEchoPassword)) {
    return false;
  }

  return GetEditAction() != EditAction::eDrop &&
         GetEditAction() != EditAction::ePaste;
}

}  // namespace mozilla
