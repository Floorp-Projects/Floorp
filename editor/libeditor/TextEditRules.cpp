/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TextEditRules.h"

#include "TextEditUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEditor.h"
#include "mozilla/dom/Element.h"
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
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeFilter.h"
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
  if (IsReadonly() || IsDisabled()) \
  {                     \
    *aCancel = true; \
    return NS_OK;       \
  };

/********************************************************
 * mozilla::TextEditRules
 ********************************************************/

TextEditRules::TextEditRules()
  : mTextEditor(nullptr)
  , mPasswordIMEIndex(0)
  , mCachedSelectionOffset(0)
  , mActionNesting(0)
  , mLockRulesSniffing(false)
  , mDidExplicitlySetInterline(false)
  , mDeleteBidiImmediately(false)
  , mTheAction(EditAction::none)
  , mLastStart(0)
  , mLastLength(0)
{
  InitFields();
}

void
TextEditRules::InitFields()
{
  mTextEditor = nullptr;
  mPasswordText.Truncate();
  mPasswordIMEText.Truncate();
  mPasswordIMEIndex = 0;
  mBogusNode = nullptr;
  mCachedSelectionNode = nullptr;
  mCachedSelectionOffset = 0;
  mActionNesting = 0;
  mLockRulesSniffing = false;
  mDidExplicitlySetInterline = false;
  mDeleteBidiImmediately = false;
  mTheAction = EditAction::none;
  mTimer = nullptr;
  mLastStart = 0;
  mLastLength = 0;
}

TextEditRules::~TextEditRules()
{
   // do NOT delete mTextEditor here.  We do not hold a ref count to
   // mTextEditor.  mTextEditor owns our lifespan.

  if (mTimer) {
    mTimer->Cancel();
  }
}

NS_IMPL_CYCLE_COLLECTION(TextEditRules, mBogusNode, mCachedSelectionNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextEditRules)
  NS_INTERFACE_MAP_ENTRY(nsIEditRules)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEditRules)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TextEditRules)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TextEditRules)

NS_IMETHODIMP
TextEditRules::Init(TextEditor* aTextEditor)
{
  if (!aTextEditor) {
    return NS_ERROR_NULL_POINTER;
  }

  InitFields();

  // We hold a non-refcounted reference back to our editor.
  mTextEditor = aTextEditor;
  RefPtr<Selection> selection = mTextEditor->GetSelection();
  NS_WARNING_ASSERTION(selection, "editor cannot get selection");

  // Put in a magic br if needed. This method handles null selection,
  // which should never happen anyway
  nsresult rv = CreateBogusNodeIfNeeded(selection);
  NS_ENSURE_SUCCESS(rv, rv);

  // If the selection hasn't been set up yet, set it up collapsed to the end of
  // our editable content.
  if (!selection->RangeCount()) {
    rv = mTextEditor->CollapseSelectionToEnd(selection);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (IsPlaintextEditor()) {
    // ensure trailing br node
    rv = CreateTrailingBRIfNeeded();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mDeleteBidiImmediately =
    Preferences::GetBool("bidi.edit.delete_immediately", false);

  return NS_OK;
}

NS_IMETHODIMP
TextEditRules::SetInitialValue(const nsAString& aValue)
{
  if (IsPasswordEditor()) {
    mPasswordText = aValue;
  }
  return NS_OK;
}

NS_IMETHODIMP
TextEditRules::DetachEditor()
{
  if (mTimer) {
    mTimer->Cancel();
  }
  mTextEditor = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
TextEditRules::BeforeEdit(EditAction action,
                          nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) {
    return NS_OK;
  }

  AutoLockRulesSniffing lockIt(this);
  mDidExplicitlySetInterline = false;
  if (!mActionNesting) {
    // let rules remember the top level action
    mTheAction = action;
  }
  mActionNesting++;

  // get the selection and cache the position before editing
  if (NS_WARN_IF(!mTextEditor)) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<TextEditor> textEditor = mTextEditor;
  RefPtr<Selection> selection = textEditor->GetSelection();
  NS_ENSURE_STATE(selection);

  if (action == EditAction::setText) {
    // setText replaces all text, so mCachedSelectionNode might be invalid on
    // AfterEdit.
    // Since this will be used as start position of spellchecker, we should
    // use root instead.
    mCachedSelectionNode = textEditor->GetRoot();
    mCachedSelectionOffset = 0;
  } else {
    mCachedSelectionNode = selection->GetAnchorNode();
    mCachedSelectionOffset = selection->AnchorOffset();
  }

  return NS_OK;
}

NS_IMETHODIMP
TextEditRules::AfterEdit(EditAction action,
                         nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) {
    return NS_OK;
  }

  AutoLockRulesSniffing lockIt(this);

  NS_PRECONDITION(mActionNesting>0, "bad action nesting!");
  if (!--mActionNesting) {
    NS_ENSURE_STATE(mTextEditor);
    RefPtr<Selection> selection = mTextEditor->GetSelection();
    NS_ENSURE_STATE(selection);

    NS_ENSURE_STATE(mTextEditor);
    nsresult rv =
      mTextEditor->HandleInlineSpellCheck(action, selection,
                                          mCachedSelectionNode,
                                          mCachedSelectionOffset,
                                          nullptr, 0, nullptr, 0);
    NS_ENSURE_SUCCESS(rv, rv);

    // no longer uses mCachedSelectionNode, so release it.
    mCachedSelectionNode = nullptr;

    // if only trailing <br> remaining remove it
    rv = RemoveRedundantTrailingBR();
    if (NS_FAILED(rv)) {
      return rv;
    }

    // detect empty doc
    rv = CreateBogusNodeIfNeeded(selection);
    NS_ENSURE_SUCCESS(rv, rv);

    // ensure trailing br node
    rv = CreateTrailingBRIfNeeded();
    NS_ENSURE_SUCCESS(rv, rv);

    // collapse the selection to the trailing BR if it's at the end of our text node
    CollapseSelectionToTrailingBRIfNeeded(selection);
  }
  return NS_OK;
}

NS_IMETHODIMP
TextEditRules::WillDoAction(Selection* aSelection,
                            RulesInfo* aInfo,
                            bool* aCancel,
                            bool* aHandled)
{
  // null selection is legal
  MOZ_ASSERT(aInfo && aCancel && aHandled);

  *aCancel = false;
  *aHandled = false;

  // my kingdom for dynamic cast
  TextRulesInfo* info = static_cast<TextRulesInfo*>(aInfo);

  switch (info->action) {
    case EditAction::insertBreak:
      UndefineCaretBidiLevel(aSelection);
      return WillInsertBreak(aSelection, aCancel, aHandled, info->maxLength);
    case EditAction::insertText:
    case EditAction::insertIMEText:
      UndefineCaretBidiLevel(aSelection);
      return WillInsertText(info->action, aSelection, aCancel, aHandled,
                            info->inString, info->outString, info->maxLength);
    case EditAction::setText:
      UndefineCaretBidiLevel(aSelection);
      return WillSetText(*aSelection, aCancel, aHandled, info->inString,
                         info->maxLength);
    case EditAction::deleteSelection:
      return WillDeleteSelection(aSelection, info->collapsedAction,
                                 aCancel, aHandled);
    case EditAction::undo:
      return WillUndo(aSelection, aCancel, aHandled);
    case EditAction::redo:
      return WillRedo(aSelection, aCancel, aHandled);
    case EditAction::setTextProperty:
      return WillSetTextProperty(aSelection, aCancel, aHandled);
    case EditAction::removeTextProperty:
      return WillRemoveTextProperty(aSelection, aCancel, aHandled);
    case EditAction::outputText:
      return WillOutputText(aSelection, info->outputFormat, info->outString,
                            info->flags, aCancel, aHandled);
    case EditAction::insertElement:
      // i had thought this would be html rules only.  but we put pre elements
      // into plaintext mail when doing quoting for reply!  doh!
      WillInsert(*aSelection, aCancel);
      return NS_OK;
    default:
      return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP
TextEditRules::DidDoAction(Selection* aSelection,
                           RulesInfo* aInfo,
                           nsresult aResult)
{
  NS_ENSURE_STATE(mTextEditor);
  // don't let any txns in here move the selection around behind our back.
  // Note that this won't prevent explicit selection setting from working.
  AutoTransactionsConserveSelection dontChangeMySelection(mTextEditor);

  NS_ENSURE_TRUE(aSelection && aInfo, NS_ERROR_NULL_POINTER);

  // my kingdom for dynamic cast
  TextRulesInfo* info = static_cast<TextRulesInfo*>(aInfo);

  switch (info->action) {
    case EditAction::insertBreak:
      return DidInsertBreak(aSelection, aResult);
    case EditAction::insertText:
    case EditAction::insertIMEText:
      return DidInsertText(aSelection, aResult);
    case EditAction::setText:
      return DidSetText(*aSelection, aResult);
    case EditAction::deleteSelection:
      return DidDeleteSelection(aSelection, info->collapsedAction, aResult);
    case EditAction::undo:
      return DidUndo(aSelection, aResult);
    case EditAction::redo:
      return DidRedo(aSelection, aResult);
    case EditAction::setTextProperty:
      return DidSetTextProperty(aSelection, aResult);
    case EditAction::removeTextProperty:
      return DidRemoveTextProperty(aSelection, aResult);
    case EditAction::outputText:
      return DidOutputText(aSelection, aResult);
    default:
      // Don't fail on transactions we don't handle here!
      return NS_OK;
  }
}

/**
 * Return false if the editor has non-empty text nodes or non-text
 * nodes.  Otherwise, i.e., there is no meaningful content,
 * return true.
 */
NS_IMETHODIMP_(bool)
TextEditRules::DocumentIsEmpty()
{
  bool retVal = false;
  if (!mTextEditor || NS_FAILED(mTextEditor->DocumentIsEmpty(&retVal))) {
    retVal = true;
  }

  return retVal;
}

void
TextEditRules::WillInsert(Selection& aSelection, bool* aCancel)
{
  MOZ_ASSERT(aCancel);

  if (IsReadonly() || IsDisabled()) {
    *aCancel = true;
    return;
  }

  // initialize out param
  *aCancel = false;

  // check for the magic content node and delete it if it exists
  if (mBogusNode) {
    NS_ENSURE_TRUE_VOID(mTextEditor);
    mTextEditor->DeleteNode(mBogusNode);
    mBogusNode = nullptr;
  }
}

nsresult
TextEditRules::DidInsert(Selection* aSelection,
                         nsresult aResult)
{
  return NS_OK;
}

nsresult
TextEditRules::WillInsertBreak(Selection* aSelection,
                               bool* aCancel,
                               bool* aHandled,
                               int32_t aMaxLength)
{
  if (!aSelection || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  *aHandled = false;
  if (IsSingleLineEditor()) {
    *aCancel = true;
  } else {
    // handle docs with a max length
    // NOTE, this function copies inString into outString for us.
    NS_NAMED_LITERAL_STRING(inString, "\n");
    nsAutoString outString;
    bool didTruncate;
    nsresult rv = TruncateInsertionIfNeeded(aSelection, &inString.AsString(),
                                            &outString, aMaxLength,
                                            &didTruncate);
    NS_ENSURE_SUCCESS(rv, rv);
    if (didTruncate) {
      *aCancel = true;
      return NS_OK;
    }

    *aCancel = false;

    // if the selection isn't collapsed, delete it.
    if (!aSelection->IsCollapsed()) {
      NS_ENSURE_STATE(mTextEditor);
      rv = mTextEditor->DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    WillInsert(*aSelection, aCancel);
    // initialize out param
    // we want to ignore result of WillInsert()
    *aCancel = false;
  }
  return NS_OK;
}

nsresult
TextEditRules::DidInsertBreak(Selection* aSelection,
                              nsresult aResult)
{
  return NS_OK;
}

nsresult
TextEditRules::CollapseSelectionToTrailingBRIfNeeded(Selection* aSelection)
{
  // we only need to execute the stuff below if we are a plaintext editor.
  // html editors have a different mechanism for putting in mozBR's
  // (because there are a bunch more places you have to worry about it in html)
  if (!IsPlaintextEditor()) {
    return NS_OK;
  }

  NS_ENSURE_STATE(mTextEditor);

  // If there is no selection ranges, we should set to the end of the editor.
  // This is usually performed in TextEditRules::Init(), however, if the
  // editor is reframed, this may be called by AfterEdit().
  if (!aSelection->RangeCount()) {
    mTextEditor->CollapseSelectionToEnd(aSelection);
  }

  // if we are at the end of the textarea, we need to set the
  // selection to stick to the mozBR at the end of the textarea.
  int32_t selOffset;
  nsCOMPtr<nsINode> selNode;
  nsresult rv =
    EditorBase::GetStartNodeAndOffset(aSelection,
                                      getter_AddRefs(selNode), &selOffset);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!EditorBase::IsTextNode(selNode)) {
    return NS_OK; // Nothing to do if we're not at a text node.
  }

  // nothing to do if we're not at the end of the text node
  if (selOffset != static_cast<int32_t>(selNode->Length())) {
    return NS_OK;
  }

  NS_ENSURE_STATE(mTextEditor);
  nsINode* root = mTextEditor->GetRoot();
  if (NS_WARN_IF(!root)) {
    return NS_ERROR_NULL_POINTER;
  }
  nsINode* parentNode = selNode->GetParentNode();
  if (parentNode != root) {
    return NS_OK;
  }

  nsINode* nextNode = selNode->GetNextSibling();
  if (nextNode && TextEditUtils::IsMozBR(nextNode)) {
    EditorRawDOMPoint afterSelNode(selNode);
    if (NS_WARN_IF(!afterSelNode.AdvanceOffset())) {
      return NS_ERROR_FAILURE;
    }
    rv = aSelection->Collapse(afterSelNode);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}

static inline already_AddRefed<nsINode>
GetTextNode(Selection* selection)
{
  int32_t selOffset;
  nsCOMPtr<nsINode> selNode;
  nsresult rv =
    EditorBase::GetStartNodeAndOffset(selection,
                                      getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(rv, nullptr);
  if (!EditorBase::IsTextNode(selNode)) {
    // This should be the root node, walk the tree looking for text nodes
    RefPtr<NodeIterator> iter =
      new NodeIterator(selNode, nsIDOMNodeFilter::SHOW_TEXT,
                       NodeFilterHolder());
    while (!EditorBase::IsTextNode(selNode)) {
      IgnoredErrorResult rv;
      selNode = iter->NextNode(rv);
      if (!selNode) {
        return nullptr;
      }
    }
  }
  return selNode.forget();
}
#ifdef DEBUG
#define ASSERT_PASSWORD_LENGTHS_EQUAL()                                \
  if (IsPasswordEditor() && mTextEditor->GetRoot()) {                  \
    int32_t txtLen;                                                    \
    mTextEditor->GetTextLength(&txtLen);                               \
    NS_ASSERTION(mPasswordText.Length() == uint32_t(txtLen),           \
                 "password length not equal to number of asterisks");  \
  }
#else
#define ASSERT_PASSWORD_LENGTHS_EQUAL()
#endif

// static
void
TextEditRules::HandleNewLines(nsString& aString,
                              int32_t aNewlineHandling)
{
  if (aNewlineHandling < 0) {
    int32_t caretStyle;
    TextEditor::GetDefaultEditorPrefs(aNewlineHandling, caretStyle);
  }

  switch(aNewlineHandling) {
    case nsIPlaintextEditor::eNewlinesReplaceWithSpaces:
      // Strip trailing newlines first so we don't wind up with trailing spaces
      aString.Trim(CRLF, false, true);
      aString.ReplaceChar(CRLF, ' ');
      break;
    case nsIPlaintextEditor::eNewlinesStrip:
      aString.StripCRLF();
      break;
    case nsIPlaintextEditor::eNewlinesPasteToFirst:
    default: {
      int32_t firstCRLF = aString.FindCharInSet(CRLF);

      // we get first *non-empty* line.
      int32_t offset = 0;
      while (firstCRLF == offset) {
        offset++;
        firstCRLF = aString.FindCharInSet(CRLF, offset);
      }
      if (firstCRLF > 0) {
        aString.Truncate(firstCRLF);
      }
      if (offset > 0) {
        aString.Cut(0, offset);
      }
      break;
    }
    case nsIPlaintextEditor::eNewlinesReplaceWithCommas:
      aString.Trim(CRLF, true, true);
      aString.ReplaceChar(CRLF, ',');
      break;
    case nsIPlaintextEditor::eNewlinesStripSurroundingWhitespace: {
      nsAutoString result;
      uint32_t offset = 0;
      while (offset < aString.Length()) {
        int32_t nextCRLF = aString.FindCharInSet(CRLF, offset);
        if (nextCRLF < 0) {
          result.Append(nsDependentSubstring(aString, offset));
          break;
        }
        uint32_t wsBegin = nextCRLF;
        // look backwards for the first non-whitespace char
        while (wsBegin > offset && NS_IS_SPACE(aString[wsBegin - 1])) {
          --wsBegin;
        }
        result.Append(nsDependentSubstring(aString, offset, wsBegin - offset));
        offset = nextCRLF + 1;
        while (offset < aString.Length() && NS_IS_SPACE(aString[offset])) {
          ++offset;
        }
      }
      aString = result;
      break;
    }
    case nsIPlaintextEditor::eNewlinesPasteIntact:
      // even if we're pasting newlines, don't paste leading/trailing ones
      aString.Trim(CRLF, true, true);
      break;
  }
}

nsresult
TextEditRules::WillInsertText(EditAction aAction,
                              Selection* aSelection,
                              bool* aCancel,
                              bool* aHandled,
                              const nsAString* inString,
                              nsAString* outString,
                              int32_t aMaxLength)
{
  if (!aSelection || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }

  if (inString->IsEmpty() && aAction != EditAction::insertIMEText) {
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
  nsresult rv = TruncateInsertionIfNeeded(aSelection, inString, outString,
                                          aMaxLength, &truncated);
  NS_ENSURE_SUCCESS(rv, rv);
  // If we're exceeding the maxlength when composing IME, we need to clean up
  // the composing text, so we shouldn't return early.
  if (truncated && outString->IsEmpty() &&
      aAction != EditAction::insertIMEText) {
    *aCancel = true;
    return NS_OK;
  }

  uint32_t start = 0;
  uint32_t end = 0;

  // handle password field docs
  if (IsPasswordEditor()) {
    NS_ENSURE_STATE(mTextEditor);
    nsContentUtils::GetSelectionInTextControl(aSelection,
                                              mTextEditor->GetRoot(),
                                              start, end);
  }

  // if the selection isn't collapsed, delete it.
  if (!aSelection->IsCollapsed()) {
    NS_ENSURE_STATE(mTextEditor);
    rv = mTextEditor->DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  WillInsert(*aSelection, aCancel);
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;

  // handle password field data
  // this has the side effect of changing all the characters in aOutString
  // to the replacement character
  if (IsPasswordEditor() &&
      aAction == EditAction::insertIMEText) {
    RemoveIMETextFromPWBuf(start, outString);
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

    NS_ENSURE_STATE(mTextEditor);
    HandleNewLines(tString, mTextEditor->mNewlineHandling);

    outString->Assign(tString);
  }

  if (IsPasswordEditor()) {
    // manage the password buffer
    mPasswordText.Insert(*outString, start);

    if (LookAndFeel::GetEchoPassword() && !DontEchoPassword()) {
      HideLastPWInput();
      mLastStart = start;
      mLastLength = outString->Length();
      if (mTimer) {
        mTimer->Cancel();
      } else {
        mTimer = NS_NewTimer();
        NS_ENSURE_TRUE(mTimer, NS_ERROR_OUT_OF_MEMORY);
      }
      mTimer->InitWithCallback(this, LookAndFeel::GetPasswordMaskDelay(),
                               nsITimer::TYPE_ONE_SHOT);
    } else {
      FillBufWithPWChars(outString, outString->Length());
    }
  }

  // get the (collapsed) selection location
  NS_ENSURE_STATE(aSelection->GetRangeAt(0));
  EditorRawDOMPoint atStartOfSelection(aSelection->GetRangeAt(0)->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSetAndValid())) {
    return NS_ERROR_FAILURE;
  }

  // don't put text in places that can't have it
  NS_ENSURE_STATE(mTextEditor);
  if (!atStartOfSelection.IsInTextNode() &&
      !mTextEditor->CanContainTag(*atStartOfSelection.GetContainer(),
                                  *nsGkAtoms::textTagName)) {
    return NS_ERROR_FAILURE;
  }

  // we need to get the doc
  NS_ENSURE_STATE(mTextEditor);
  nsCOMPtr<nsIDocument> doc = mTextEditor->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);

  if (aAction == EditAction::insertIMEText) {
    NS_ENSURE_STATE(mTextEditor);
    // Find better insertion point to insert text.
    EditorRawDOMPoint betterInsertionPoint =
      mTextEditor->FindBetterInsertionPoint(atStartOfSelection);
    // If there is one or more IME selections, its minimum offset should be
    // the insertion point.
    int32_t IMESelectionOffset =
      mTextEditor->GetIMESelectionStartOffsetIn(
                     betterInsertionPoint.GetContainer());
    if (IMESelectionOffset >= 0) {
      betterInsertionPoint.Set(betterInsertionPoint.GetContainer(),
                               IMESelectionOffset);
    }
    rv = mTextEditor->InsertTextImpl(*doc, *outString, betterInsertionPoint);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // aAction == EditAction::insertText

    // don't change my selection in subtransactions
    NS_ENSURE_STATE(mTextEditor);
    AutoTransactionsConserveSelection dontChangeMySelection(mTextEditor);

    EditorRawDOMPoint pointAfterStringInserted;
    rv = mTextEditor->InsertTextImpl(*doc, *outString, atStartOfSelection,
                                     &pointAfterStringInserted);
    NS_ENSURE_SUCCESS(rv, rv);

    if (pointAfterStringInserted.IsSet()) {
      // Make the caret attach to the inserted text, unless this text ends with a LF,
      // in which case make the caret attach to the next line.
      bool endsWithLF =
        !outString->IsEmpty() && outString->Last() == nsCRT::LF;
      aSelection->SetInterlinePosition(endsWithLF);

      MOZ_ASSERT(!pointAfterStringInserted.GetChild(),
        "After inserting text into a text node, pointAfterStringInserted."
        "GetChild() should be nullptr");
      IgnoredErrorResult error;
      aSelection->Collapse(pointAfterStringInserted, error);
      if (error.Failed()) {
        NS_WARNING("Failed to collapse selection after inserting string");
      }
    }
  }
  ASSERT_PASSWORD_LENGTHS_EQUAL()
  return NS_OK;
}

nsresult
TextEditRules::DidInsertText(Selection* aSelection,
                             nsresult aResult)
{
  return DidInsert(aSelection, aResult);
}

nsresult
TextEditRules::WillSetText(Selection& aSelection,
                           bool* aCancel,
                           bool* aHandled,
                           const nsAString* aString,
                           int32_t aMaxLength)
{
  MOZ_ASSERT(aCancel);
  MOZ_ASSERT(aHandled);
  MOZ_ASSERT(aString);

  CANCEL_OPERATION_IF_READONLY_OR_DISABLED

  *aHandled = false;
  *aCancel = false;

  if (NS_WARN_IF(!mTextEditor)) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<TextEditor> textEditor = mTextEditor;

  if (!IsPlaintextEditor() || textEditor->IsIMEComposing() ||
      aMaxLength != -1) {
    // SetTextImpl only supports plain text editor without IME.
    return NS_OK;
  }

  if (IsPasswordEditor() && LookAndFeel::GetEchoPassword() &&
      !DontEchoPassword()) {
    // Echo password timer will implement on InsertText.
    return NS_OK;
  }

  WillInsert(aSelection, aCancel);
  // we want to ignore result of WillInsert()
  *aCancel = false;

  RefPtr<Element> rootElement = textEditor->GetRoot();
  uint32_t count = rootElement->GetChildCount();

  // handles only when there is only one node and it's a text node, or empty.

  if (count > 1) {
    return NS_OK;
  }

  nsAutoString tString(*aString);

  if (IsPasswordEditor()) {
    mPasswordText.Assign(tString);
    FillBufWithPWChars(&tString, tString.Length());
  } else if (IsSingleLineEditor()) {
    HandleNewLines(tString, textEditor->mNewlineHandling);
  }

  if (!count) {
    if (tString.IsEmpty()) {
      *aHandled = true;
      return NS_OK;
    }
    RefPtr<nsIDocument> doc = textEditor->GetDocument();
    if (NS_WARN_IF(!doc)) {
      return NS_OK;
    }
    RefPtr<nsTextNode> newNode = EditorBase::CreateTextNode(*doc, tString);
    if (NS_WARN_IF(!newNode)) {
      return NS_OK;
    }
    nsresult rv =
      textEditor->InsertNode(*newNode, EditorRawDOMPoint(rootElement, 0));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    *aHandled = true;

    ASSERT_PASSWORD_LENGTHS_EQUAL();

    return NS_OK;
  }

  nsINode* curNode = rootElement->GetFirstChild();
  if (NS_WARN_IF(!EditorBase::IsTextNode(curNode))) {
    return NS_OK;
  }

  // Even if empty text, we don't remove text node and set empty text
  // for performance
  nsresult rv = textEditor->SetTextImpl(aSelection, tString,
                                        *curNode->GetAsText());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aHandled = true;

  ASSERT_PASSWORD_LENGTHS_EQUAL();

  return NS_OK;
}

nsresult
TextEditRules::DidSetText(Selection& aSelection,
                          nsresult aResult)
{
  return NS_OK;
}

nsresult
TextEditRules::WillSetTextProperty(Selection* aSelection,
                                   bool* aCancel,
                                   bool* aHandled)
{
  if (!aSelection || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }

  // XXX: should probably return a success value other than NS_OK that means "not allowed"
  if (IsPlaintextEditor()) {
    *aCancel = true;
  }
  return NS_OK;
}

nsresult
TextEditRules::DidSetTextProperty(Selection* aSelection,
                                  nsresult aResult)
{
  return NS_OK;
}

nsresult
TextEditRules::WillRemoveTextProperty(Selection* aSelection,
                                      bool* aCancel,
                                      bool* aHandled)
{
  if (!aSelection || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }

  // XXX: should probably return a success value other than NS_OK that means "not allowed"
  if (IsPlaintextEditor()) {
    *aCancel = true;
  }
  return NS_OK;
}

nsresult
TextEditRules::DidRemoveTextProperty(Selection* aSelection,
                                     nsresult aResult)
{
  return NS_OK;
}

nsresult
TextEditRules::WillDeleteSelection(Selection* aSelection,
                                   nsIEditor::EDirection aCollapsedAction,
                                   bool* aCancel,
                                   bool* aHandled)
{
  if (!aSelection || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED

  // initialize out param
  *aCancel = false;
  *aHandled = false;

  // if there is only bogus content, cancel the operation
  if (mBogusNode) {
    *aCancel = true;
    return NS_OK;
  }

  // If the current selection is empty (e.g the user presses backspace with
  // a collapsed selection), then we want to avoid sending the selectstart
  // event to the user, so we hide selection changes. However, we still
  // want to send a single selectionchange event to the document, so we
  // batch the selectionchange events, such that a single event fires after
  // the AutoHideSelectionChanges destructor has been run.
  SelectionBatcher selectionBatcher(aSelection);
  AutoHideSelectionChanges hideSelection(aSelection);
  nsAutoScriptBlocker scriptBlocker;

  if (IsPasswordEditor()) {
    NS_ENSURE_STATE(mTextEditor);
    nsresult rv =
      mTextEditor->ExtendSelectionForDelete(aSelection, &aCollapsedAction);
    NS_ENSURE_SUCCESS(rv, rv);

    // manage the password buffer
    uint32_t start, end;
    nsContentUtils::GetSelectionInTextControl(aSelection,
                                              mTextEditor->GetRoot(),
                                              start, end);

    if (LookAndFeel::GetEchoPassword()) {
      HideLastPWInput();
      mLastStart = start;
      mLastLength = 0;
      if (mTimer) {
        mTimer->Cancel();
      }
    }

    // Collapsed selection.
    if (end == start) {
      // Deleting back.
      if (nsIEditor::ePrevious == aCollapsedAction && start > 0) {
        mPasswordText.Cut(start-1, 1);
      }
      // Deleting forward.
      else if (nsIEditor::eNext == aCollapsedAction) {
        mPasswordText.Cut(start, 1);
      }
      // Otherwise nothing to do for this collapsed selection.
    }
    // Extended selection.
    else {
      mPasswordText.Cut(start, end-start);
    }
  } else {
    nsCOMPtr<nsIDOMNode> startNode;
    int32_t startOffset;
    nsresult rv =
      EditorBase::GetStartNodeAndOffset(aSelection, getter_AddRefs(startNode),
                                        &startOffset);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);

    if (!aSelection->IsCollapsed()) {
      return NS_OK;
    }

    // Test for distance between caret and text that will be deleted
    rv = CheckBidiLevelForDeletion(aSelection, startNode, startOffset,
                                   aCollapsedAction, aCancel);
    NS_ENSURE_SUCCESS(rv, rv);
    if (*aCancel) {
      return NS_OK;
    }

    NS_ENSURE_STATE(mTextEditor);
    rv = mTextEditor->ExtendSelectionForDelete(aSelection, &aCollapsedAction);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ENSURE_STATE(mTextEditor);
  nsresult rv =
    mTextEditor->DeleteSelectionImpl(aCollapsedAction, nsIEditor::eStrip);
  NS_ENSURE_SUCCESS(rv, rv);

  *aHandled = true;
  ASSERT_PASSWORD_LENGTHS_EQUAL()
  return NS_OK;
}

nsresult
TextEditRules::DidDeleteSelection(Selection* aSelection,
                                  nsIEditor::EDirection aCollapsedAction,
                                  nsresult aResult)
{
  nsCOMPtr<nsINode> startNode;
  int32_t startOffset;
  nsresult rv =
    EditorBase::GetStartNodeAndOffset(aSelection,
                                      getter_AddRefs(startNode), &startOffset);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (NS_WARN_IF(!startNode)) {
    return NS_ERROR_FAILURE;
  }

  // delete empty text nodes at selection
  if (EditorBase::IsTextNode(startNode)) {
    // are we in an empty text node?
    if (!startNode->Length()) {
      NS_ENSURE_STATE(mTextEditor);
      rv = mTextEditor->DeleteNode(startNode);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }
  if (mDidExplicitlySetInterline) {
    return NS_OK;
  }
  // We prevent the caret from sticking on the left of prior BR
  // (i.e. the end of previous line) after this deletion.  Bug 92124
  return aSelection->SetInterlinePosition(true);
}

nsresult
TextEditRules::WillUndo(Selection* aSelection,
                        bool* aCancel,
                        bool* aHandled)
{
  if (!aSelection || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  // initialize out param
  *aCancel = false;
  *aHandled = false;
  return NS_OK;
}

/**
 * The idea here is to see if the magic empty node has suddenly reappeared as
 * the result of the undo.  If it has, set our state so we remember it.
 * There is a tradeoff between doing here and at redo, or doing it everywhere
 * else that might care.  Since undo and redo are relatively rare, it makes
 * sense to take the (small) performance hit here.
 */
nsresult
TextEditRules::DidUndo(Selection* aSelection,
                       nsresult aResult)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  // If aResult is an error, we return it.
  NS_ENSURE_SUCCESS(aResult, aResult);

  NS_ENSURE_STATE(mTextEditor);
  dom::Element* theRoot = mTextEditor->GetRoot();
  NS_ENSURE_TRUE(theRoot, NS_ERROR_FAILURE);
  nsIContent* node = mTextEditor->GetLeftmostChild(theRoot);
  if (node && mTextEditor->IsMozEditorBogusNode(node)) {
    mBogusNode = node;
  } else {
    mBogusNode = nullptr;
  }
  return aResult;
}

nsresult
TextEditRules::WillRedo(Selection* aSelection,
                        bool* aCancel,
                        bool* aHandled)
{
  if (!aSelection || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  // initialize out param
  *aCancel = false;
  *aHandled = false;
  return NS_OK;
}

nsresult
TextEditRules::DidRedo(Selection* aSelection,
                       nsresult aResult)
{
  if (!aSelection) {
    return NS_ERROR_NULL_POINTER;
  }
  if (NS_FAILED(aResult)) {
    return aResult; // if aResult is an error, we return it.
  }

  NS_ENSURE_STATE(mTextEditor);
  RefPtr<Element> theRoot = mTextEditor->GetRoot();
  NS_ENSURE_TRUE(theRoot, NS_ERROR_FAILURE);

  nsCOMPtr<nsIHTMLCollection> nodeList =
    theRoot->GetElementsByTagName(NS_LITERAL_STRING("br"));
  MOZ_ASSERT(nodeList);
  uint32_t len = nodeList->Length();

  if (len != 1) {
    // only in the case of one br could there be the bogus node
    mBogusNode = nullptr;
    return NS_OK;
  }

  RefPtr<Element> node = nodeList->Item(0);
  if (mTextEditor->IsMozEditorBogusNode(node)) {
    mBogusNode = node;
  } else {
    mBogusNode = nullptr;
  }
  return NS_OK;
}

nsresult
TextEditRules::WillOutputText(Selection* aSelection,
                              const nsAString* aOutputFormat,
                              nsAString* aOutString,
                              uint32_t aFlags,
                              bool* aCancel,
                              bool* aHandled)
{
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

  // XXX Looks like that even if it's password field, we need to use the
  //     expensive path if the caller requests some complicated handling.
  //     However, changing the behavior for password field might cause
  //     security issue.  So, be careful when you touch here.
  if (IsPasswordEditor()) {
    *aOutString = mPasswordText;
    *aHandled = true;
    return NS_OK;
  }

  // If there is a bogus node, there's no content.  So output empty string.
  if (mBogusNode) {
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
  if (NS_WARN_IF(!mTextEditor) || mTextEditor->AsHTMLEditor()) {
    return NS_OK;
  }

  Element* root = mTextEditor->GetRoot();
  if (!root) { // Don't warn it, this is possible, e.g., 997805.html
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
                 !TextEditUtils::IsMozBR(firstChildExceptText) &&
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
  nsresult rv = text->GetData(*aOutString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Fall back to the expensive path if it fails.
    return NS_OK;
  }

  *aHandled = true;
  return NS_OK;
}

nsresult
TextEditRules::DidOutputText(Selection* aSelection,
                             nsresult aResult)
{
  return NS_OK;
}

nsresult
TextEditRules::RemoveRedundantTrailingBR()
{
  // If the bogus node exists, we have no work to do
  if (mBogusNode) {
    return NS_OK;
  }

  // Likewise, nothing to be done if we could never have inserted a trailing br
  if (IsSingleLineEditor()) {
    return NS_OK;
  }

  NS_ENSURE_STATE(mTextEditor);
  RefPtr<dom::Element> body = mTextEditor->GetRoot();
  if (!body) {
    return NS_ERROR_NULL_POINTER;
  }

  uint32_t childCount = body->GetChildCount();
  if (childCount > 1) {
    // The trailing br is redundant if it is the only remaining child node
    return NS_OK;
  }

  RefPtr<nsIContent> child = body->GetFirstChild();
  if (!child || !child->IsElement()) {
    return NS_OK;
  }

  dom::Element* elem = child->AsElement();
  if (!TextEditUtils::IsMozBR(elem)) {
    return NS_OK;
  }

  // Rather than deleting this node from the DOM tree we should instead
  // morph this br into the bogus node
  elem->UnsetAttr(kNameSpaceID_None, nsGkAtoms::type, true);

  // set mBogusNode to be this <br>
  mBogusNode = elem;

  // give it the bogus node attribute
  elem->SetAttr(kNameSpaceID_None, kMOZEditorBogusNodeAttrAtom,
                kMOZEditorBogusNodeValue, false);
  return NS_OK;
}

nsresult
TextEditRules::CreateTrailingBRIfNeeded()
{
  // but only if we aren't a single line edit field
  if (IsSingleLineEditor()) {
    return NS_OK;
  }

  NS_ENSURE_STATE(mTextEditor);
  dom::Element* body = mTextEditor->GetRoot();
  NS_ENSURE_TRUE(body, NS_ERROR_NULL_POINTER);

  nsIContent* lastChild = body->GetLastChild();
  // assuming CreateBogusNodeIfNeeded() has been called first
  NS_ENSURE_TRUE(lastChild, NS_ERROR_NULL_POINTER);

  if (!lastChild->IsHTMLElement(nsGkAtoms::br)) {
    AutoTransactionsConserveSelection dontChangeMySelection(mTextEditor);
    return CreateMozBR(*body, body->Length());
  }

  // Check to see if the trailing BR is a former bogus node - this will have
  // stuck around if we previously morphed a trailing node into a bogus node.
  if (!mTextEditor->IsMozEditorBogusNode(lastChild)) {
    return NS_OK;
  }

  // Morph it back to a mozBR
  lastChild->AsElement()->UnsetAttr(kNameSpaceID_None,
                                    kMOZEditorBogusNodeAttrAtom,
                                    false);
  lastChild->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                                  NS_LITERAL_STRING("_moz"),
                                  true);
  return NS_OK;
}

nsresult
TextEditRules::CreateBogusNodeIfNeeded(Selection* aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(mTextEditor, NS_ERROR_NULL_POINTER);

  if (mBogusNode) {
    // Let's not create more than one, ok?
    return NS_OK;
  }

  // tell rules system to not do any post-processing
  AutoRules beginRulesSniffing(mTextEditor, EditAction::ignore,
                               nsIEditor::eNone);

  nsCOMPtr<dom::Element> body = mTextEditor->GetRoot();
  if (!body) {
    // We don't even have a body yet, don't insert any bogus nodes at
    // this point.
    return NS_OK;
  }

  // Now we've got the body element. Iterate over the body element's children,
  // looking for editable content. If no editable content is found, insert the
  // bogus node.
  bool bodyEditable = mTextEditor->IsEditable(body);
  for (nsIContent* bodyChild = body->GetFirstChild();
       bodyChild;
       bodyChild = bodyChild->GetNextSibling()) {
    if (mTextEditor->IsMozEditorBogusNode(bodyChild) ||
        !bodyEditable ||
        mTextEditor->IsEditable(bodyChild) ||
        mTextEditor->IsBlockNode(bodyChild)) {
      return NS_OK;
    }
  }

  // Skip adding the bogus node if body is read-only.
  if (!mTextEditor->IsModifiableNode(body)) {
    return NS_OK;
  }

  // Create a br.
  nsCOMPtr<Element> newContent = mTextEditor->CreateHTMLContent(nsGkAtoms::br);
  NS_ENSURE_STATE(newContent);

  // set mBogusNode to be the newly created <br>
  mBogusNode = newContent;

  // Give it a special attribute.
  newContent->SetAttr(kNameSpaceID_None, kMOZEditorBogusNodeAttrAtom,
                      kMOZEditorBogusNodeValue, false);

  // Put the node in the document.
  nsresult rv =
    mTextEditor->InsertNode(*mBogusNode, EditorRawDOMPoint(body, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Set selection.
  ErrorResult error;
  aSelection->Collapse(EditorRawDOMPoint(body, 0), error);
  if (NS_WARN_IF(error.Failed())) {
    error.SuppressException();
  }
  return NS_OK;
}


nsresult
TextEditRules::TruncateInsertionIfNeeded(Selection* aSelection,
                                         const nsAString* aInString,
                                         nsAString* aOutString,
                                         int32_t aMaxLength,
                                         bool* aTruncated)
{
  if (!aSelection || !aInString || !aOutString) {
    return NS_ERROR_NULL_POINTER;
  }

  if (!aOutString->Assign(*aInString, mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (aTruncated) {
    *aTruncated = false;
  }

  NS_ENSURE_STATE(mTextEditor);
  if (-1 != aMaxLength && IsPlaintextEditor() &&
      !mTextEditor->IsIMEComposing()) {
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
    nsresult rv = mTextEditor->GetTextLength(&docLength);
    if (NS_FAILED(rv)) {
      return rv;
    }

    uint32_t start, end;
    nsContentUtils::GetSelectionInTextControl(aSelection,
                                              mTextEditor->GetRoot(),
                                              start, end);

    TextComposition* composition = mTextEditor->GetComposition();
    uint32_t oldCompStrLength = composition ? composition->String().Length() : 0;

    const uint32_t selectionLength = end - start;
    const int32_t resultingDocLength = docLength - selectionLength - oldCompStrLength;
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

void
TextEditRules::ResetIMETextPWBuf()
{
  mPasswordIMEText.Truncate();
}

void
TextEditRules::RemoveIMETextFromPWBuf(uint32_t& aStart,
                                      nsAString* aIMEString)
{
  MOZ_ASSERT(aIMEString);

  // initialize PasswordIME
  if (mPasswordIMEText.IsEmpty()) {
    mPasswordIMEIndex = aStart;
  } else {
    // manage the password buffer
    mPasswordText.Cut(mPasswordIMEIndex, mPasswordIMEText.Length());
    aStart = mPasswordIMEIndex;
  }

  mPasswordIMEText.Assign(*aIMEString);
}

NS_IMETHODIMP
TextEditRules::Notify(nsITimer* aTimer)
{
  MOZ_ASSERT(mTimer);

  // Check whether our text editor's password flag was changed before this
  // "hide password character" timer actually fires.
  nsresult rv = IsPasswordEditor() ? HideLastPWInput() : NS_OK;
  ASSERT_PASSWORD_LENGTHS_EQUAL();
  mLastLength = 0;
  return rv;
}

NS_IMETHODIMP
TextEditRules::GetName(nsACString& aName)
{
  aName.AssignLiteral("TextEditRules");
  return NS_OK;
}

nsresult
TextEditRules::HideLastPWInput()
{
  if (!mLastLength) {
    // Special case, we're trying to replace a range that no longer exists
    return NS_OK;
  }

  nsAutoString hiddenText;
  FillBufWithPWChars(&hiddenText, mLastLength);

  NS_ENSURE_STATE(mTextEditor);
  RefPtr<Selection> selection = mTextEditor->GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  uint32_t start, end;
  nsContentUtils::GetSelectionInTextControl(selection, mTextEditor->GetRoot(),
                                            start, end);

  nsCOMPtr<nsINode> selNode = GetTextNode(selection);
  NS_ENSURE_TRUE(selNode, NS_OK);

  selNode->GetAsText()->ReplaceData(mLastStart, mLastLength, hiddenText);
  // XXXbz Selection::Collapse/Extend take int32_t, but there are tons of
  // callsites... Converting all that is a battle for another day.
  selection->Collapse(selNode, start);
  if (start != end) {
    selection->Extend(selNode, end);
  }
  return NS_OK;
}

// static
void
TextEditRules::FillBufWithPWChars(nsAString* aOutString,
                                  int32_t aLength)
{
  MOZ_ASSERT(aOutString);

  // change the output to the platform password character
  char16_t passwordChar = LookAndFeel::GetPasswordCharacter();

  aOutString->Truncate();
  for (int32_t i = 0; i < aLength; i++) {
    aOutString->Append(passwordChar);
  }
}

nsresult
TextEditRules::CreateBRInternal(nsINode& inParent,
                                int32_t inOffset,
                                bool aMozBR,
                                Element** aOutBRElement)
{
  if (NS_WARN_IF(!mTextEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<TextEditor> textEditor = mTextEditor;

  RefPtr<Element> brElem =
    textEditor->CreateBR(EditorRawDOMPoint(&inParent, inOffset));
  if (NS_WARN_IF(!brElem)) {
    return NS_ERROR_FAILURE;
  }

  // give it special moz attr
  if (aMozBR) {
    nsresult rv = textEditor->SetAttribute(brElem, nsGkAtoms::type,
                                           NS_LITERAL_STRING("_moz"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aOutBRElement) {
    brElem.forget(aOutBRElement);
  }
  return NS_OK;
}

NS_IMETHODIMP
TextEditRules::DocumentModified()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

bool
TextEditRules::IsPasswordEditor() const
{
  return mTextEditor ? mTextEditor->IsPasswordEditor() : false;
}

bool
TextEditRules::IsSingleLineEditor() const
{
  return mTextEditor ? mTextEditor->IsSingleLineEditor() : false;
}

bool
TextEditRules::IsPlaintextEditor() const
{
  return mTextEditor ? mTextEditor->IsPlaintextEditor() : false;
}

bool
TextEditRules::IsReadonly() const
{
  return mTextEditor ? mTextEditor->IsReadonly() : false;
}

bool
TextEditRules::IsDisabled() const
{
  return mTextEditor ? mTextEditor->IsDisabled() : false;
}
bool
TextEditRules::IsMailEditor() const
{
  return mTextEditor ? mTextEditor->IsMailEditor() : false;
}

bool
TextEditRules::DontEchoPassword() const
{
  return mTextEditor ? mTextEditor->DontEchoPassword() : false;
}

} // namespace mozilla
