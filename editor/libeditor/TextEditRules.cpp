/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TextEditRules.h"

#include "HTMLEditRules.h"
#include "TextEditUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEditor.h"
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
#include "nsIDocumentEncoder.h"
#include "nsNameSpaceManager.h"
#include "nsINode.h"
#include "nsIPlaintextEditor.h"
#include "nsISupportsBase.h"
#include "nsLiteralString.h"
#include "nsTextNode.h"
#include "nsUnicharUtils.h"
#include "nsIHTMLCollection.h"
#include "nsPrintfCString.h"

namespace mozilla {

using namespace dom;

#define CANCEL_OPERATION_IF_READONLY_OR_DISABLED \
  if (IsReadonly() || IsDisabled()) {            \
    *aCancel = true;                             \
    return NS_OK;                                \
  }

#define CANCEL_OPERATION_AND_RETURN_EDIT_ACTION_RESULT_IF_READONLY_OF_DISABLED \
  if (IsReadonly() || IsDisabled()) {                                          \
    return EditActionCanceled(NS_OK);                                          \
  }

/********************************************************
 * mozilla::TextEditRules
 ********************************************************/

TextEditRules::TextEditRules()
    : mTextEditor(nullptr),
      mData(nullptr),
#ifdef DEBUG
      mIsHandling(false),
#endif  // #ifdef DEBUG
      mIsHTMLEditRules(false) {
  InitFields();
}

void TextEditRules::InitFields() {
  mTextEditor = nullptr;
}

HTMLEditRules* TextEditRules::AsHTMLEditRules() {
  return mIsHTMLEditRules ? static_cast<HTMLEditRules*>(this) : nullptr;
}

const HTMLEditRules* TextEditRules::AsHTMLEditRules() const {
  return mIsHTMLEditRules ? static_cast<const HTMLEditRules*>(this) : nullptr;
}

nsresult TextEditRules::Init(TextEditor* aTextEditor) {
  if (NS_WARN_IF(!aTextEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  Selection* selection = aTextEditor->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }

  InitFields();

  // We hold a non-refcounted reference back to our editor.
  mTextEditor = aTextEditor;
  AutoSafeEditorData setData(*this, *mTextEditor);

  nsresult rv = MOZ_KnownLive(TextEditorRef())
                    .MaybeCreatePaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // If the selection hasn't been set up yet, set it up collapsed to the end of
  // our editable content.
  if (!SelectionRefPtr()->RangeCount()) {
    rv = TextEditorRef().CollapseSelectionToEnd();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (IsPlaintextEditor()) {
    // ensure trailing br node
    rv = CreateTrailingBRIfNeeded();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult TextEditRules::DetachEditor() {
  mTextEditor = nullptr;
  return NS_OK;
}

nsresult TextEditRules::BeforeEdit() {
  MOZ_ASSERT(!mIsHandling);

  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

#ifdef DEBUG
  mIsHandling = true;
#endif  // #ifdef DEBUG

  return NS_OK;
}

nsresult TextEditRules::AfterEdit() {
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

#ifdef DEBUG
  MOZ_ASSERT(mIsHandling);
  mIsHandling = false;
#endif  // #ifdef DEBUG

  AutoSafeEditorData setData(*this, *mTextEditor);

  // XXX Probably, we should spellcheck again after edit action (not top-level
  //     sub-action) is handled because the ranges can be referred only by
  //     users.
  nsresult rv = TextEditorRef().HandleInlineSpellCheckAfterEdit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = MOZ_KnownLive(TextEditorRef()).EnsurePaddingBRElementForEmptyEditor();
  if (NS_FAILED(rv)) {
    return rv;
  }

  // ensure trailing br node
  rv = CreateTrailingBRIfNeeded();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Collapse the selection to the trailing padding <br> element for empty
  // last line if it's at the end of our text node.
  rv = CollapseSelectionToTrailingBRIfNeeded();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "Failed to selection to after the text node in TextEditor");
  return NS_OK;
}

nsresult TextEditRules::WillDoAction(EditSubActionInfo& aInfo, bool* aCancel,
                                     bool* aHandled) {
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  MOZ_ASSERT(aCancel);
  MOZ_ASSERT(aHandled);

  *aCancel = false;
  *aHandled = false;

  AutoSafeEditorData setData(*this, *mTextEditor);

  // my kingdom for dynamic cast
  switch (aInfo.mEditSubAction) {
    case EditSubAction::eSetText:
      TextEditorRef().UndefineCaretBidiLevel();
      return WillSetText(aCancel, aHandled, aInfo.inString, aInfo.maxLength);
    case EditSubAction::eDeleteSelectedContent:
      return WillDeleteSelection(aInfo.collapsedAction, aCancel, aHandled);
    case EditSubAction::eComputeTextToOutput:
      return WillOutputText(aInfo.outputFormat, aInfo.outString, aInfo.flags,
                            aCancel, aHandled);
    case EditSubAction::eInsertQuotedText: {
      CANCEL_OPERATION_IF_READONLY_OR_DISABLED

      // XXX Do we need to support paste-as-quotation in password editor (and
      //     also in single line editor)?
      TextEditorRef().MaybeDoAutoPasswordMasking();

      nsresult rv = MOZ_KnownLive(TextEditorRef())
                        .EnsureNoPaddingBRElementForEmptyEditor();
      NS_WARNING_ASSERTION(NS_FAILED(rv),
                           "Failed to remove padding <br> element");
      return rv;
    }
    case EditSubAction::eInsertElement:
    case EditSubAction::eInsertLineBreak:
    case EditSubAction::eInsertText:
    case EditSubAction::eInsertTextComingFromIME:
    case EditSubAction::eUndo:
    case EditSubAction::eRedo:
      MOZ_ASSERT_UNREACHABLE("This path should've been dead code");
      return NS_ERROR_UNEXPECTED;
    default:
      return NS_ERROR_FAILURE;
  }
}

nsresult TextEditRules::DidDoAction(EditSubActionInfo& aInfo,
                                    nsresult aResult) {
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  switch (aInfo.mEditSubAction) {
    case EditSubAction::eDeleteSelectedContent:
    case EditSubAction::eInsertElement:
    case EditSubAction::eInsertLineBreak:
    case EditSubAction::eInsertText:
    case EditSubAction::eInsertTextComingFromIME:
    case EditSubAction::eUndo:
    case EditSubAction::eRedo:
      MOZ_ASSERT_UNREACHABLE("This path should've been dead code");
      return NS_ERROR_UNEXPECTED;
    default:
      // Don't fail on transactions we don't handle here!
      return NS_OK;
  }
}

bool TextEditRules::DocumentIsEmpty() const {
  bool retVal = false;
  if (!mTextEditor || NS_FAILED(mTextEditor->IsEmpty(&retVal))) {
    retVal = true;
  }

  return retVal;
}

EditActionResult TextEditor::InsertLineFeedCharacterAtSelection() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!AsHTMLEditor());
  MOZ_ASSERT(!IsSingleLineEditor());

  UndefineCaretBidiLevel();

  CANCEL_OPERATION_AND_RETURN_EDIT_ACTION_RESULT_IF_READONLY_OF_DISABLED

  if (mMaxTextLength >= 0) {
    nsAutoString insertionString(NS_LITERAL_STRING("\n"));
    EditActionResult result =
        TruncateInsertionStringForMaxLength(insertionString);
    if (NS_WARN_IF(result.Failed())) {
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
    if (NS_WARN_IF(Destroyed())) {
      return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditActionIgnored(rv);
    }
  }

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(NS_FAILED(rv))) {
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

  // Don't put text in places that can't have it.
  if (!pointToInsert.IsInTextNode() &&
      !CanContainTag(*pointToInsert.GetContainer(), *nsGkAtoms::textTagName)) {
    return EditActionIgnored(NS_ERROR_FAILURE);
  }

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
  if (NS_WARN_IF(!pointAfterInsertedLineFeed.IsSet())) {
    return EditActionIgnored(NS_ERROR_FAILURE);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditActionIgnored(rv);
  }

  // set the selection to the correct location
  MOZ_ASSERT(
      !pointAfterInsertedLineFeed.GetChild(),
      "After inserting text into a text node, pointAfterInsertedLineFeed."
      "GetChild() should be nullptr");
  rv = SelectionRefPtr()->Collapse(pointAfterInsertedLineFeed);
  if (NS_WARN_IF(NS_FAILED(rv))) {
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
    SelectionRefPtr()->SetInterlinePosition(true, IgnoreErrors());
  }

  return EditActionHandled();
}

nsresult TextEditRules::CollapseSelectionToTrailingBRIfNeeded() {
  MOZ_ASSERT(IsEditorDataAvailable());

  // we only need to execute the stuff below if we are a plaintext editor.
  // html editors have a different mechanism for putting in padding <br>
  // element's (because there are a bunch more places you have to worry about
  // it in html)
  if (!IsPlaintextEditor()) {
    return NS_OK;
  }

  // If there is no selection ranges, we should set to the end of the editor.
  // This is usually performed in TextEditRules::Init(), however, if the
  // editor is reframed, this may be called by AfterEdit().
  if (!SelectionRefPtr()->RangeCount()) {
    TextEditorRef().CollapseSelectionToEnd();
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
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

  Element* rootElement = TextEditorRef().GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_NULL_POINTER;
  }
  nsINode* parentNode = selectionStartPoint.GetContainer()->GetParentNode();
  if (parentNode != rootElement) {
    return NS_OK;
  }

  nsINode* nextNode = selectionStartPoint.GetContainer()->GetNextSibling();
  if (!nextNode || !EditorBase::IsPaddingBRElementForEmptyLastLine(*nextNode)) {
    return NS_OK;
  }

  EditorRawDOMPoint afterStartContainer(selectionStartPoint.GetContainer());
  if (NS_WARN_IF(!afterStartContainer.AdvanceOffset())) {
    return NS_ERROR_FAILURE;
  }
  ErrorResult error;
  SelectionRefPtr()->Collapse(afterStartContainer, error);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    error.SuppressException();
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  return NS_OK;
}

already_AddRefed<nsINode>
TextEditRules::GetTextNodeAroundSelectionStartContainer() {
  MOZ_ASSERT(IsEditorDataAvailable());

  EditorRawDOMPoint selectionStartPoint(
      EditorBase::GetStartPoint(*SelectionRefPtr()));
  if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
    return nullptr;
  }
  if (selectionStartPoint.IsInTextNode()) {
    nsCOMPtr<nsINode> node = selectionStartPoint.GetContainer();
    return node.forget();
  }
  // This should be the root node, walk the tree looking for text nodes.
  // XXX NodeIterator sets mutation observer even for this temporary use.
  //     It's too expensive if this is called from a hot path.
  nsCOMPtr<nsINode> node = selectionStartPoint.GetContainer();
  RefPtr<NodeIterator> iter =
      new NodeIterator(node, NodeFilter_Binding::SHOW_TEXT, nullptr);
  while (!EditorBase::IsTextNode(node)) {
    node = iter->NextNode(IgnoreErrors());
    if (!node) {
      return nullptr;
    }
  }
  return node.forget();
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
    case nsIPlaintextEditor::eNewlinesReplaceWithSpaces:
      // Default of Firefox:
      // Strip trailing newlines first so we don't wind up with trailing spaces
      aString.Trim(LFSTR, false, true);
      aString.ReplaceChar(kLF, ' ');
      break;
    case nsIPlaintextEditor::eNewlinesStrip:
      aString.StripChar(kLF);
      break;
    case nsIPlaintextEditor::eNewlinesPasteToFirst:
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
    case nsIPlaintextEditor::eNewlinesReplaceWithCommas:
      // Default of Thunderbird:
      aString.Trim(LFSTR, true, true);
      aString.ReplaceChar(kLF, ',');
      break;
    case nsIPlaintextEditor::eNewlinesStripSurroundingWhitespace: {
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
    case nsIPlaintextEditor::eNewlinesPasteIntact:
      // even if we're pasting newlines, don't paste leading/trailing ones
      aString.Trim(LFSTR, true, true);
      break;
  }
}

EditActionResult TextEditor::HandleInsertText(
    EditSubAction aEditSubAction, const nsAString& aInsertionString) {
  MOZ_ASSERT(IsEditActionDataAvailable());
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
    if (NS_WARN_IF(result.Failed())) {
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
    if (NS_WARN_IF(Destroyed())) {
      return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditActionHandled(rv);
    }
  }

  // XXX Why don't we cancel here?  Shouldn't we do this first?
  CANCEL_OPERATION_AND_RETURN_EDIT_ACTION_RESULT_IF_READONLY_OF_DISABLED

  MaybeDoAutoPasswordMasking();

  nsresult rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(NS_FAILED(rv))) {
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

  // don't put text in places that can't have it
  if (!atStartOfSelection.IsInTextNode() &&
      !CanContainTag(*atStartOfSelection.GetContainer(),
                     *nsGkAtoms::textTagName)) {
    return EditActionHandled(NS_ERROR_FAILURE);
  }

  RefPtr<Document> document = GetDocument();
  if (NS_WARN_IF(!document)) {
    return EditActionHandled(NS_ERROR_NOT_INITIALIZED);
  }

  if (aEditSubAction == EditSubAction::eInsertTextComingFromIME) {
    EditorRawDOMPoint compositionStartPoint = GetCompositionStartPoint();
    if (!compositionStartPoint.IsSet()) {
      compositionStartPoint = FindBetterInsertionPoint(atStartOfSelection);
    }
    nsresult rv = InsertTextWithTransaction(*document, insertionString,
                                            compositionStartPoint);
    if (NS_WARN_IF(Destroyed())) {
      return EditActionHandled(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
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
    if (NS_WARN_IF(NS_FAILED(rv))) {
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
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetUnmaskRangeAndNotify() failed");
    return EditActionHandled(rv);
  }

  return EditActionHandled();
}

nsresult TextEditRules::WillSetText(bool* aCancel, bool* aHandled,
                                    const nsAString* aString,
                                    int32_t aMaxLength) {
  MOZ_ASSERT(IsEditorDataAvailable());
  MOZ_ASSERT(!mIsHTMLEditRules);
  MOZ_ASSERT(aCancel);
  MOZ_ASSERT(aHandled);
  MOZ_ASSERT(aString);
  MOZ_ASSERT(aString->FindChar(static_cast<char16_t>('\r')) == kNotFound);

  // XXX If we're setting value, shouldn't we keep setting the new value here?
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED

  *aHandled = false;
  *aCancel = false;

  if (!IsPlaintextEditor() || TextEditorRef().IsIMEComposing() ||
      TextEditorRef().IsUndoRedoEnabled() ||
      TextEditorRef().GetEditAction() == EditAction::eReplaceText ||
      aMaxLength != -1) {
    // SetTextImpl only supports plain text editor without IME and
    // when we don't need to make it undoable.
    return NS_OK;
  }

  TextEditorRef().MaybeDoAutoPasswordMasking();

  nsresult rv =
      MOZ_KnownLive(TextEditorRef()).EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<Element> rootElement = TextEditorRef().GetRoot();
  nsIContent* firstChild = rootElement->GetFirstChild();

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
    if (firstChild &&
        (!EditorBase::IsTextNode(firstChild) || firstChild->GetNextSibling())) {
      return NS_OK;
    }
  } else {
    // If we're a multiline text editor, i.e., <textarea>, there is a padding
    // <br> element for empty last line followed by scrollbar/resizer elements.
    // Otherwise, a text node is followed by them.
    if (!firstChild) {
      return NS_OK;
    }
    if (EditorBase::IsTextNode(firstChild)) {
      if (!firstChild->GetNextSibling() ||
          !EditorBase::IsPaddingBRElementForEmptyLastLine(
              *firstChild->GetNextSibling())) {
        return NS_OK;
      }
    } else if (!EditorBase::IsPaddingBRElementForEmptyLastLine(*firstChild)) {
      return NS_OK;
    }
  }

  // XXX Password fields accept line breaks as normal characters with this code.
  //     Is this intentional?
  nsAutoString tString(*aString);
  if (IsSingleLineEditor() && !IsPasswordEditor()) {
    TextEditorRef().HandleNewLinesInStringForSingleLineEditor(tString);
  }

  if (!firstChild || !EditorBase::IsTextNode(firstChild)) {
    if (tString.IsEmpty()) {
      *aHandled = true;
      return NS_OK;
    }
    RefPtr<Document> doc = TextEditorRef().GetDocument();
    if (NS_WARN_IF(!doc)) {
      return NS_OK;
    }
    RefPtr<nsTextNode> newNode = TextEditorRef().CreateTextNode(tString);
    if (NS_WARN_IF(!newNode)) {
      return NS_OK;
    }
    nsresult rv = MOZ_KnownLive(TextEditorRef())
                      .InsertNodeWithTransaction(
                          *newNode, EditorDOMPoint(rootElement, 0));
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    *aHandled = true;
    return NS_OK;
  }

  // Even if empty text, we don't remove text node and set empty text
  // for performance
  RefPtr<Text> textNode = firstChild->GetAsText();
  if (MOZ_UNLIKELY(NS_WARN_IF(!textNode))) {
    return NS_OK;
  }
  rv = MOZ_KnownLive(TextEditorRef()).SetTextImpl(tString, *textNode);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // If we replaced non-empty value with empty string, we need to delete the
  // text node.
  if (tString.IsEmpty() && !textNode->Length()) {
    nsresult rv =
        MOZ_KnownLive(TextEditorRef()).DeleteNodeWithTransaction(*textNode);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "DeleteNodeWithTransaction() failed, but ignored");
    // XXX I don't think this is necessary because the anonymous `<div>`
    //     element has now only padding `<br>` element even if there are
    //     something.
    IgnoredErrorResult ignoredError;
    SelectionRefPtr()->SetInterlinePosition(true, ignoredError);
    NS_WARNING_ASSERTION(!ignoredError.Failed(),
                         "Selection::SetInterlinePoisition() failed");
  }

  *aHandled = true;
  return NS_OK;
}

nsresult TextEditRules::WillDeleteSelection(
    nsIEditor::EDirection aCollapsedAction, bool* aCancel, bool* aHandled) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
  }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED

  // initialize out param
  *aCancel = false;
  *aHandled = false;

  // if there is only padding <br> element for empty editor, cancel the
  // operation.
  if (TextEditorRef().HasPaddingBRElementForEmptyEditor()) {
    *aCancel = true;
    return NS_OK;
  }
  nsresult rv =
      DeleteSelectionWithTransaction(aCollapsedAction, aCancel, aHandled);
  // DeleteSelectionWithTransaction() creates SelectionBatcher.  Therefore,
  // quitting from it might cause having destroyed the editor.
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult TextEditRules::DeleteSelectionWithTransaction(
    nsIEditor::EDirection aCollapsedAction, bool* aCancel, bool* aHandled) {
  MOZ_ASSERT(IsEditorDataAvailable());
  MOZ_ASSERT(aCancel);
  MOZ_ASSERT(*aCancel == false);
  MOZ_ASSERT(aHandled);

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
    TextEditorRef().MaskAllCharacters();
  } else {
    EditorRawDOMPoint selectionStartPoint(
        EditorBase::GetStartPoint(*SelectionRefPtr()));
    if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    if (!SelectionRefPtr()->IsCollapsed()) {
      return NS_OK;
    }

    // Test for distance between caret and text that will be deleted
    EditActionResult result = TextEditorRef().SetCaretBidiLevelForDeletion(
        selectionStartPoint, aCollapsedAction);
    if (NS_WARN_IF(result.Failed())) {
      return result.Rv();
    }
    if (result.Canceled()) {
      *aCancel = true;
      return NS_OK;
    }
  }

  nsresult rv = TextEditorRef().ExtendSelectionForDelete(&aCollapsedAction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = MOZ_KnownLive(TextEditorRef())
           .DeleteSelectionWithTransaction(aCollapsedAction, nsIEditor::eStrip);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aHandled = true;
  return NS_OK;
}

nsresult TextEditRules::WillOutputText(const nsAString* aOutputFormat,
                                       nsAString* aOutString, uint32_t aFlags,
                                       bool* aCancel, bool* aHandled) {
  MOZ_ASSERT(IsEditorDataAvailable());

  // null selection ok
  if (NS_WARN_IF(!aOutString) || NS_WARN_IF(!aOutputFormat) ||
      NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_NULL_POINTER;
  }

  // initialize out param
  *aCancel = false;
  *aHandled = false;

  if (!aOutputFormat->LowerCaseEqualsLiteral("text/plain")) {
    return NS_OK;
  }

  // If there is a padding <br> element, there's no content.  So output empty
  // string.
  if (TextEditorRef().HasPaddingBRElementForEmptyEditor()) {
    aOutString->Truncate();
    *aHandled = true;
    return NS_OK;
  }

  // If it's necessary to check selection range or the editor wraps hard,
  // we need some complicated handling.  In such case, we need to use the
  // expensive path.
  // XXX Anything else what we cannot return plain text simply?
  if (aFlags & nsIDocumentEncoder::OutputSelectionOnly ||
      aFlags & nsIDocumentEncoder::OutputWrap) {
    return NS_OK;
  }

  // If it's neither <input type="text"> nor <textarea>, e.g., an HTML editor
  // which is in plaintext mode (e.g., plaintext email composer on Thunderbird),
  // it should be handled by the expensive path.
  if (TextEditorRef().AsHTMLEditor()) {
    return NS_OK;
  }

  Element* root = TextEditorRef().GetRoot();
  if (!root) {  // Don't warn it, this is possible, e.g., 997805.html
    aOutString->Truncate();
    *aHandled = true;
    return NS_OK;
  }

  nsIContent* firstChild = root->GetFirstChild();
  if (!firstChild) {
    aOutString->Truncate();
    *aHandled = true;
    return NS_OK;
  }

  // If it's an <input type="text"> element, the DOM tree should be:
  // <div class="anonymous-div">
  //   #text
  // </div>
  //
  // If it's a <textarea> element, the DOM tree should be:
  // <div class="anonymous-div">
  //   #text (if there is)
  //   <br type="_moz">
  //   <scrollbar orient="horizontal">
  //   ...
  // </div>

  Text* text = firstChild->GetAsText();
  nsIContent* firstChildExceptText =
      text ? firstChild->GetNextSibling() : firstChild;
  // If the DOM tree is unexpected, fall back to the expensive path.
  bool isInput = IsSingleLineEditor();
  bool isTextarea = !isInput;
  if (NS_WARN_IF(isInput && firstChildExceptText) ||
      NS_WARN_IF(isTextarea && !firstChildExceptText) ||
      NS_WARN_IF(isTextarea &&
                 !EditorBase::IsPaddingBRElementForEmptyLastLine(
                     *firstChildExceptText) &&
                 !firstChildExceptText->IsXULElement(nsGkAtoms::scrollbar))) {
    return NS_OK;
  }

  // If there is no text node in the expected DOM tree, we can say that it's
  // just empty.
  if (!text) {
    aOutString->Truncate();
    *aHandled = true;
    return NS_OK;
  }

  // Otherwise, the text is the value.
  text->GetData(*aOutString);

  *aHandled = true;
  return NS_OK;
}

nsresult TextEditRules::CreateTrailingBRIfNeeded() {
  MOZ_ASSERT(IsEditorDataAvailable());

  // but only if we aren't a single line edit field
  if (IsSingleLineEditor()) {
    return NS_OK;
  }

  Element* rootElement = TextEditorRef().GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_FAILURE;
  }

  // Assuming EditorBase::MaybeCreatePaddingBRElementForEmptyEditor() has been
  // called first.
  // XXX This assumption is wrong.  This method may be called alone.  Actually,
  //     we see this warning in mochitest log.  So, we should fix this bug
  //     later.
  if (NS_WARN_IF(!rootElement->GetLastChild())) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<HTMLBRElement> brElement =
      HTMLBRElement::FromNode(rootElement->GetLastChild());
  if (!brElement) {
    AutoTransactionsConserveSelection dontChangeMySelection(TextEditorRef());
    EditorDOMPoint endOfRoot;
    endOfRoot.SetToEndOf(rootElement);
    CreateElementResult createPaddingBRResult =
        MOZ_KnownLive(TextEditorRef())
            .InsertPaddingBRElementForEmptyLastLineWithTransaction(endOfRoot);
    if (NS_WARN_IF(createPaddingBRResult.Failed())) {
      return createPaddingBRResult.Rv();
    }
    return NS_OK;
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
  if (NS_IS_HIGH_SURROGATE(maybeHighSurrogate) &&
      NS_IS_LOW_SURROGATE(maybeLowSurrogate)) {
    newInsertionStringLength--;
  }
  // XXX What should we do if we're removing IVS but its preceding
  //     character won't be removed?
  aInsertionString.Truncate(newInsertionStringLength);
  return EditActionHandled();
}

bool TextEditRules::IsPasswordEditor() const {
  return mTextEditor ? mTextEditor->IsPasswordEditor() : false;
}

bool TextEditRules::IsMaskingPassword() const {
  MOZ_ASSERT(IsPasswordEditor());
  return mTextEditor ? mTextEditor->IsMaskingPassword() : true;
}

bool TextEditRules::IsSingleLineEditor() const {
  return mTextEditor ? mTextEditor->IsSingleLineEditor() : false;
}

bool TextEditRules::IsPlaintextEditor() const {
  return mTextEditor ? mTextEditor->IsPlaintextEditor() : false;
}

bool TextEditRules::IsReadonly() const {
  return mTextEditor ? mTextEditor->IsReadonly() : false;
}

bool TextEditRules::IsDisabled() const {
  return mTextEditor ? mTextEditor->IsDisabled() : false;
}
bool TextEditRules::IsMailEditor() const {
  return mTextEditor ? mTextEditor->IsMailEditor() : false;
}

bool TextEditor::CanEchoPasswordNow() const {
  if (!LookAndFeel::GetEchoPassword() ||
      (mFlags & nsIPlaintextEditor::eEditorDontEchoPassword)) {
    return false;
  }

  return GetEditAction() != EditAction::eDrop &&
         GetEditAction() != EditAction::ePaste;
}

}  // namespace mozilla
