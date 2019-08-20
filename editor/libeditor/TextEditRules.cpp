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

NS_IMPL_CYCLE_COLLECTION_CLASS(TextEditRules)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(TextEditRules)
  if (HTMLEditRules* htmlEditRules = tmp->AsHTMLEditRules()) {
    HTMLEditRules* tmp = htmlEditRules;
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocChangeRange)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mUtilRange)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mNewBlock)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mRangeItem)
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(TextEditRules)
  if (HTMLEditRules* htmlEditRules = tmp->AsHTMLEditRules()) {
    HTMLEditRules* tmp = htmlEditRules;
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocChangeRange)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mUtilRange)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNewBlock)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRangeItem)
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(TextEditRules, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(TextEditRules, Release)

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
    case EditSubAction::eInsertLineBreak: {
      UndefineCaretBidiLevel();
      EditActionResult result = WillInsertLineBreak(aInfo.maxLength);
      if (NS_WARN_IF(result.Failed())) {
        return result.Rv();
      }
      *aCancel = result.Canceled();
      *aHandled = result.Handled();
      MOZ_ASSERT(!result.Ignored());
      return NS_OK;
    }
    case EditSubAction::eInsertText:
    case EditSubAction::eInsertTextComingFromIME:
      UndefineCaretBidiLevel();
      return WillInsertText(aInfo.mEditSubAction, aCancel, aHandled,
                            aInfo.inString, aInfo.outString, aInfo.maxLength);
    case EditSubAction::eSetText:
      UndefineCaretBidiLevel();
      return WillSetText(aCancel, aHandled, aInfo.inString, aInfo.maxLength);
    case EditSubAction::eDeleteSelectedContent:
      return WillDeleteSelection(aInfo.collapsedAction, aCancel, aHandled);
    case EditSubAction::eSetTextProperty:
      return WillSetTextProperty(aCancel, aHandled);
    case EditSubAction::eRemoveTextProperty:
      return WillRemoveTextProperty(aCancel, aHandled);
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

  AutoSafeEditorData setData(*this, *mTextEditor);

  // don't let any txns in here move the selection around behind our back.
  // Note that this won't prevent explicit selection setting from working.
  AutoTransactionsConserveSelection dontChangeMySelection(TextEditorRef());

  switch (aInfo.mEditSubAction) {
    case EditSubAction::eDeleteSelectedContent:
      MOZ_ASSERT(!mIsHTMLEditRules);
      return DidDeleteSelection();
    case EditSubAction::eInsertElement:
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

EditActionResult TextEditRules::WillInsertLineBreak(int32_t aMaxLength) {
  MOZ_ASSERT(IsEditorDataAvailable());
  MOZ_ASSERT(!IsSingleLineEditor());

  CANCEL_OPERATION_AND_RETURN_EDIT_ACTION_RESULT_IF_READONLY_OF_DISABLED

  // handle docs with a max length
  // NOTE, this function copies inString into outString for us.
  NS_NAMED_LITERAL_STRING(inString, "\n");
  nsAutoString outString;
  bool didTruncate;
  nsresult rv = TruncateInsertionIfNeeded(&inString.AsString(), &outString,
                                          aMaxLength, &didTruncate);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditActionIgnored(rv);
  }
  if (didTruncate) {
    return EditActionCanceled();
  }

  // if the selection isn't collapsed, delete it.
  if (!SelectionRefPtr()->IsCollapsed()) {
    rv = MOZ_KnownLive(TextEditorRef())
             .DeleteSelectionAsSubAction(nsIEditor::eNone, nsIEditor::eStrip);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return EditActionIgnored(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditActionIgnored(rv);
    }
  }

  rv = MOZ_KnownLive(TextEditorRef()).EnsureNoPaddingBRElementForEmptyEditor();
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
      !TextEditorRef().CanContainTag(*pointToInsert.GetContainer(),
                                     *nsGkAtoms::textTagName)) {
    return EditActionIgnored(NS_ERROR_FAILURE);
  }

  RefPtr<Document> doc = TextEditorRef().GetDocument();
  if (NS_WARN_IF(!doc)) {
    return EditActionIgnored(NS_ERROR_NOT_INITIALIZED);
  }

  // Don't change my selection in sub-transactions.
  AutoTransactionsConserveSelection dontChangeMySelection(TextEditorRef());

  // Insert a linefeed character.
  EditorRawDOMPoint pointAfterInsertedLineBreak;
  rv = MOZ_KnownLive(TextEditorRef())
           .InsertTextWithTransaction(*doc, NS_LITERAL_STRING("\n"),
                                      pointToInsert,
                                      &pointAfterInsertedLineBreak);
  if (NS_WARN_IF(!pointAfterInsertedLineBreak.IsSet())) {
    return EditActionIgnored(NS_ERROR_FAILURE);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditActionIgnored(rv);
  }

  // set the selection to the correct location
  MOZ_ASSERT(
      !pointAfterInsertedLineBreak.GetChild(),
      "After inserting text into a text node, pointAfterInsertedLineBreak."
      "GetChild() should be nullptr");
  rv = SelectionRefPtr()->Collapse(pointAfterInsertedLineBreak);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditActionIgnored(rv);
  }

  // see if we're at the end of the editor range
  EditorRawDOMPoint endPoint(EditorBase::GetEndPoint(*SelectionRefPtr()));
  if (endPoint == pointAfterInsertedLineBreak) {
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

void TextEditRules::HandleNewLines(nsString& aString) {
  static const char16_t kLF = static_cast<char16_t>('\n');
  MOZ_ASSERT(IsEditorDataAvailable());
  MOZ_ASSERT(aString.FindChar(static_cast<uint16_t>('\r')) == kNotFound);

  // First of all, check if aString contains '\n' since if the string
  // does not include it, we don't need to do nothing here.
  int32_t firstLF = aString.FindChar(kLF, 0);
  if (firstLF == kNotFound) {
    return;
  }

  switch (TextEditorRef().mNewlineHandling) {
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

nsresult TextEditRules::WillInsertText(EditSubAction aEditSubAction,
                                       bool* aCancel, bool* aHandled,
                                       const nsAString* inString,
                                       nsAString* outString,
                                       int32_t aMaxLength) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (inString->IsEmpty() &&
      aEditSubAction != EditSubAction::eInsertTextComingFromIME) {
    // HACK: this is a fix for bug 19395
    // I can't outlaw all empty insertions
    // because IME transaction depend on them
    // There is more work to do to make the
    // world safe for IME.
    *aCancel = true;
    *aHandled = false;
    return NS_OK;
  }

  // initialize out param
  *aCancel = false;
  *aHandled = true;

  // handle docs with a max length
  // NOTE, this function copies inString into outString for us.
  bool truncated = false;
  nsresult rv =
      TruncateInsertionIfNeeded(inString, outString, aMaxLength, &truncated);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  // If we're exceeding the maxlength when composing IME, we need to clean up
  // the composing text, so we shouldn't return early.
  if (truncated && outString->IsEmpty() &&
      aEditSubAction != EditSubAction::eInsertTextComingFromIME) {
    *aCancel = true;
    return NS_OK;
  }

  uint32_t start = 0;
  if (IsPasswordEditor()) {
    if (TextEditorRef().GetComposition() &&
        !TextEditorRef().GetComposition()->String().IsEmpty()) {
      start = TextEditorRef().GetComposition()->XPOffsetInTextNode();
    } else {
      uint32_t end = 0;
      nsContentUtils::GetSelectionInTextControl(
          SelectionRefPtr(), TextEditorRef().GetRoot(), start, end);
    }
  }

  // if the selection isn't collapsed, delete it.
  if (!SelectionRefPtr()->IsCollapsed()) {
    rv = MOZ_KnownLive(TextEditorRef())
             .DeleteSelectionAsSubAction(nsIEditor::eNone, nsIEditor::eStrip);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // XXX Why do we set `aCancel` here, but ignore it?
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED

  TextEditorRef().MaybeDoAutoPasswordMasking();

  rv = MOZ_KnownLive(TextEditorRef()).EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
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
    nsAutoString tString(*outString);
    // XXX Some callers of TextEditor::InsertTextAsAction()  already make the
    //     string use only \n as a linebreaker.  However, they are not hot
    //     path and nsContentUtils::PlatformToDOMLineBreaks() does nothing
    //     if the string doesn't include \r.  So, let's convert linebreakers
    //     here.  Note that there are too many callers of
    //     TextEditor::InsertTextAsAction().  So, it's difficult to keep
    //     maintaining all of them won't reach here without \r nor \r\n.
    nsContentUtils::PlatformToDOMLineBreaks(tString);
    HandleNewLines(tString);
    outString->Assign(tString);
  }

  // get the (collapsed) selection location
  nsRange* firstRange = SelectionRefPtr()->GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return NS_ERROR_FAILURE;
  }
  EditorRawDOMPoint atStartOfSelection(firstRange->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSetAndValid())) {
    return NS_ERROR_FAILURE;
  }

  // don't put text in places that can't have it
  if (!atStartOfSelection.IsInTextNode() &&
      !TextEditorRef().CanContainTag(*atStartOfSelection.GetContainer(),
                                     *nsGkAtoms::textTagName)) {
    return NS_ERROR_FAILURE;
  }

  // we need to get the doc
  RefPtr<Document> doc = TextEditorRef().GetDocument();
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (aEditSubAction == EditSubAction::eInsertTextComingFromIME) {
    EditorRawDOMPoint compositionStartPoint =
        TextEditorRef().GetCompositionStartPoint();
    if (!compositionStartPoint.IsSet()) {
      compositionStartPoint =
          TextEditorRef().FindBetterInsertionPoint(atStartOfSelection);
    }
    rv =
        MOZ_KnownLive(TextEditorRef())
            .InsertTextWithTransaction(*doc, *outString, compositionStartPoint);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    // aEditSubAction == EditSubAction::eInsertText

    // don't change my selection in subtransactions
    AutoTransactionsConserveSelection dontChangeMySelection(TextEditorRef());

    EditorRawDOMPoint pointAfterStringInserted;
    rv = MOZ_KnownLive(TextEditorRef())
             .InsertTextWithTransaction(*doc, *outString, atStartOfSelection,
                                        &pointAfterStringInserted);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (pointAfterStringInserted.IsSet()) {
      // Make the caret attach to the inserted text, unless this text ends with
      // a LF, in which case make the caret attach to the next line.
      bool endsWithLF = !outString->IsEmpty() && outString->Last() == nsCRT::LF;
      IgnoredErrorResult error;
      SelectionRefPtr()->SetInterlinePosition(endsWithLF, error);
      NS_WARNING_ASSERTION(!error.Failed(),
                           "Failed to set or unset interline position");

      MOZ_ASSERT(
          !pointAfterStringInserted.GetChild(),
          "After inserting text into a text node, pointAfterStringInserted."
          "GetChild() should be nullptr");
      error = IgnoredErrorResult();
      SelectionRefPtr()->Collapse(pointAfterStringInserted, error);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(
          !error.Failed(),
          "Failed to collapse selection after inserting string");
    }
  }

  // Unmask inputted character(s) if necessary.
  if (IsPasswordEditor() && IsMaskingPassword() && !DontEchoPassword()) {
    nsresult rv =
        MOZ_KnownLive(TextEditorRef())
            .SetUnmaskRangeAndNotify(start, outString->Length(),
                                     LookAndFeel::GetPasswordMaskDelay());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
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
    HandleNewLines(tString);
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
  if (tString.IsEmpty()) {
    DebugOnly<nsresult> rvIgnored = DidDeleteSelection();
    MOZ_ASSERT(rvIgnored != NS_ERROR_EDITOR_DESTROYED);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "DidDeleteSelection() failed");
  }

  *aHandled = true;
  return NS_OK;
}

nsresult TextEditRules::WillSetTextProperty(bool* aCancel, bool* aHandled) {
  if (NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
  }

  // XXX: should probably return a success value other than NS_OK that means
  // "not allowed"
  if (IsPlaintextEditor()) {
    *aCancel = true;
  }
  return NS_OK;
}

nsresult TextEditRules::WillRemoveTextProperty(bool* aCancel, bool* aHandled) {
  if (NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
  }

  // XXX: should probably return a success value other than NS_OK that means
  // "not allowed"
  if (IsPlaintextEditor()) {
    *aCancel = true;
  }
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
    nsresult rv = CheckBidiLevelForDeletion(selectionStartPoint,
                                            aCollapsedAction, aCancel);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (*aCancel) {
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

nsresult TextEditRules::DidDeleteSelection() {
  MOZ_ASSERT(IsEditorDataAvailable());

  EditorDOMPoint selectionStartPoint(
      EditorBase::GetStartPoint(*SelectionRefPtr()));
  if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // Delete empty text nodes at selection.
  if (selectionStartPoint.IsInTextNode() &&
      !selectionStartPoint.GetContainer()->Length()) {
    nsresult rv = MOZ_KnownLive(TextEditorRef())
                      .DeleteNodeWithTransaction(
                          MOZ_KnownLive(*selectionStartPoint.GetContainer()));
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Note that this may be true only when this is HTMLEditRules.
  if (TextEditorRef()
          .TopLevelEditSubActionDataRef()
          .mDidExplicitlySetInterLine) {
    return NS_OK;
  }
  // We prevent the caret from sticking on the left of prior BR
  // (i.e. the end of previous line) after this deletion.  Bug 92124
  ErrorResult err;
  SelectionRefPtr()->SetInterlinePosition(true, err);
  NS_WARNING_ASSERTION(!err.Failed(), "Failed to set interline position");
  return err.StealNSResult();
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

nsresult TextEditRules::TruncateInsertionIfNeeded(const nsAString* aInString,
                                                  nsAString* aOutString,
                                                  int32_t aMaxLength,
                                                  bool* aTruncated) {
  MOZ_ASSERT(IsEditorDataAvailable());

  if (NS_WARN_IF(!aInString) || NS_WARN_IF(!aOutString)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!aOutString->Assign(*aInString, mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (aTruncated) {
    *aTruncated = false;
  }

  if (-1 != aMaxLength && IsPlaintextEditor() &&
      !TextEditorRef().IsIMEComposing()) {
    // Get the current text length.
    // Get the length of inString.
    // Get the length of the selection.
    //   If selection is collapsed, it is length 0.
    //   Subtract the length of the selection from the len(doc)
    //   since we'll delete the selection on insert.
    //   This is resultingDocLength.
    // Get old length of IME composing string
    //   which will be replaced by new one.
    // If (resultingDocLength) is at or over max, cancel the insert
    // If (resultingDocLength) + (length of input) > max,
    //    set aOutString to subset of inString so length = max
    int32_t docLength;
    nsresult rv = TextEditorRef().GetTextLength(&docLength);
    if (NS_FAILED(rv)) {
      return rv;
    }

    uint32_t start, end;
    nsContentUtils::GetSelectionInTextControl(
        SelectionRefPtr(), TextEditorRef().GetRoot(), start, end);

    TextComposition* composition = TextEditorRef().GetComposition();
    uint32_t oldCompStrLength =
        composition ? composition->String().Length() : 0;

    const uint32_t selectionLength = end - start;
    const int32_t resultingDocLength =
        docLength - selectionLength - oldCompStrLength;
    if (resultingDocLength >= aMaxLength) {
      // This call is guaranteed to reduce the capacity of the string, so it
      // cannot cause an OOM.
      aOutString->Truncate();
      if (aTruncated) {
        *aTruncated = true;
      }
    } else {
      int32_t oldLength = aOutString->Length();
      if (oldLength + resultingDocLength > aMaxLength) {
        int32_t newLength = aMaxLength - resultingDocLength;
        MOZ_ASSERT(newLength > 0);
        char16_t newLastChar = aOutString->CharAt(newLength - 1);
        char16_t removingFirstChar = aOutString->CharAt(newLength);
        // Don't separate the string between a surrogate pair.
        if (NS_IS_HIGH_SURROGATE(newLastChar) &&
            NS_IS_LOW_SURROGATE(removingFirstChar)) {
          newLength--;
        }
        // XXX What should we do if we're removing IVS and its preceding
        //     character won't be removed?
        // This call is guaranteed to reduce the capacity of the string, so it
        // cannot cause an OOM.
        aOutString->Truncate(newLength);
        if (aTruncated) {
          *aTruncated = true;
        }
      }
    }
  }
  return NS_OK;
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

bool TextEditRules::DontEchoPassword() const {
  if (!mTextEditor) {
    // XXX Why do we use echo password if editor is null?
    return false;
  }
  if (!LookAndFeel::GetEchoPassword() || mTextEditor->DontEchoPassword()) {
    return true;
  }

  if (mTextEditor->GetEditAction() != EditAction::eDrop &&
      mTextEditor->GetEditAction() != EditAction::ePaste) {
    return false;
  }
  return true;
}

}  // namespace mozilla
