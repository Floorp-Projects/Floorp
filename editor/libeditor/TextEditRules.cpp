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

template CreateElementResult
TextEditRules::CreateBRInternal(const EditorDOMPoint& aPointToInsert,
                                bool aCreateMozBR);
template CreateElementResult
TextEditRules::CreateBRInternal(const EditorRawDOMPoint& aPointToInsert,
                                bool aCreateMozBR);

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
  , mData(nullptr)
  , mPasswordIMEIndex(0)
  , mCachedSelectionOffset(0)
  , mActionNesting(0)
  , mLockRulesSniffing(false)
  , mDidExplicitlySetInterline(false)
  , mDeleteBidiImmediately(false)
  , mIsHTMLEditRules(false)
  , mTopLevelEditSubAction(EditSubAction::eNone)
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
  mTopLevelEditSubAction = EditSubAction::eNone;
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

HTMLEditRules*
TextEditRules::AsHTMLEditRules()
{
  return mIsHTMLEditRules ? static_cast<HTMLEditRules*>(this) : nullptr;
}

const HTMLEditRules*
TextEditRules::AsHTMLEditRules() const
{
  return mIsHTMLEditRules ? static_cast<const HTMLEditRules*>(this) : nullptr;
}

NS_IMPL_CYCLE_COLLECTION(TextEditRules, mBogusNode, mCachedSelectionNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextEditRules)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITimerCallback)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TextEditRules)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TextEditRules)

nsresult
TextEditRules::Init(TextEditor* aTextEditor)
{
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
  AutoSafeEditorData setData(*this, *mTextEditor, *selection);

  // Put in a magic <br> if needed. This method handles null selection,
  // which should never happen anyway
  nsresult rv = CreateBogusNodeIfNeeded();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // If the selection hasn't been set up yet, set it up collapsed to the end of
  // our editable content.
  if (!SelectionRef().RangeCount()) {
    rv = TextEditorRef().CollapseSelectionToEnd(&SelectionRef());
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

  // XXX We should use AddBoolVarCache and use "current" value at initializing.
  mDeleteBidiImmediately =
    Preferences::GetBool("bidi.edit.delete_immediately", false);

  return NS_OK;
}

nsresult
TextEditRules::SetInitialValue(const nsAString& aValue)
{
  if (IsPasswordEditor()) {
    mPasswordText = aValue;
  }
  return NS_OK;
}

nsresult
TextEditRules::DetachEditor()
{
  if (mTimer) {
    mTimer->Cancel();
  }
  mTextEditor = nullptr;
  return NS_OK;
}

nsresult
TextEditRules::BeforeEdit(EditSubAction aEditSubAction,
                          nsIEditor::EDirection aDirection)
{
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  if (mLockRulesSniffing) {
    return NS_OK;
  }

  AutoLockRulesSniffing lockIt(this);
  mDidExplicitlySetInterline = false;
  if (!mActionNesting) {
    // let rules remember the top level action
    mTopLevelEditSubAction = aEditSubAction;
  }
  mActionNesting++;

  if (aEditSubAction == EditSubAction::eSetText) {
    // setText replaces all text, so mCachedSelectionNode might be invalid on
    // AfterEdit.
    // Since this will be used as start position of spellchecker, we should
    // use root instead.
    mCachedSelectionNode = mTextEditor->GetRoot();
    mCachedSelectionOffset = 0;
  } else {
    Selection* selection = mTextEditor->GetSelection();
    if (NS_WARN_IF(!selection)) {
      return NS_ERROR_FAILURE;
    }
    mCachedSelectionNode = selection->GetAnchorNode();
    mCachedSelectionOffset = selection->AnchorOffset();
  }

  return NS_OK;
}

nsresult
TextEditRules::AfterEdit(EditSubAction aEditSubAction,
                         nsIEditor::EDirection aDirection)
{
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  if (mLockRulesSniffing) {
    return NS_OK;
  }

  AutoLockRulesSniffing lockIt(this);

  MOZ_ASSERT(mActionNesting>0, "bad action nesting!");
  if (!--mActionNesting) {
    Selection* selection = mTextEditor->GetSelection();
    if (NS_WARN_IF(!selection)) {
      return NS_ERROR_FAILURE;
    }

    AutoSafeEditorData setData(*this, *mTextEditor, *selection);

    nsresult rv =
      TextEditorRef().HandleInlineSpellCheck(aEditSubAction, *selection,
                                             mCachedSelectionNode,
                                             mCachedSelectionOffset,
                                             nullptr, 0, nullptr, 0);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // no longer uses mCachedSelectionNode, so release it.
    mCachedSelectionNode = nullptr;

    // if only trailing <br> remaining remove it
    rv = RemoveRedundantTrailingBR();
    if (NS_FAILED(rv)) {
      return rv;
    }

    // detect empty doc
    rv = CreateBogusNodeIfNeeded();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // ensure trailing br node
    rv = CreateTrailingBRIfNeeded();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Collapse the selection to the trailing moz-<br> if it's at the end of
    // our text node.
    rv = CollapseSelectionToTrailingBRIfNeeded();
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
      "Failed to selection to after the text node in TextEditor");
  }
  return NS_OK;
}

nsresult
TextEditRules::WillDoAction(Selection* aSelection,
                            EditSubActionInfo& aInfo,
                            bool* aCancel,
                            bool* aHandled)
{
  if (NS_WARN_IF(!aSelection)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  MOZ_ASSERT(aCancel);
  MOZ_ASSERT(aHandled);

  *aCancel = false;
  *aHandled = false;

  AutoSafeEditorData setData(*this, *mTextEditor, *aSelection);

  // my kingdom for dynamic cast
  switch (aInfo.mEditSubAction) {
    case EditSubAction::eInsertParagraphSeparator:
      UndefineCaretBidiLevel();
      return WillInsertBreak(aCancel, aHandled, aInfo.maxLength);
    case EditSubAction::eInsertText:
    case EditSubAction::eInsertTextComingFromIME:
      UndefineCaretBidiLevel();
      return WillInsertText(aInfo.mEditSubAction, aCancel, aHandled,
                            aInfo.inString, aInfo.outString,
                            aInfo.maxLength);
    case EditSubAction::eSetText:
      UndefineCaretBidiLevel();
      return WillSetText(aCancel, aHandled, aInfo.inString,
                         aInfo.maxLength);
    case EditSubAction::eDeleteSelectedContent:
      return WillDeleteSelection(aInfo.collapsedAction, aCancel, aHandled);
    case EditSubAction::eUndo:
      return WillUndo(aCancel, aHandled);
    case EditSubAction::eRedo:
      return WillRedo(aCancel, aHandled);
    case EditSubAction::eSetTextProperty:
      return WillSetTextProperty(aCancel, aHandled);
    case EditSubAction::eRemoveTextProperty:
      return WillRemoveTextProperty(aCancel, aHandled);
    case EditSubAction::eComputeTextToOutput:
      return WillOutputText(aInfo.outputFormat, aInfo.outString, aInfo.flags,
                            aCancel, aHandled);
    case EditSubAction::eInsertElement:
      // i had thought this would be html rules only.  but we put pre elements
      // into plaintext mail when doing quoting for reply!  doh!
      return WillInsert(aCancel);
    default:
      return NS_ERROR_FAILURE;
  }
}

nsresult
TextEditRules::DidDoAction(Selection* aSelection,
                           EditSubActionInfo& aInfo,
                           nsresult aResult)
{
  if (NS_WARN_IF(!aSelection)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  AutoSafeEditorData setData(*this, *mTextEditor, *aSelection);

  // don't let any txns in here move the selection around behind our back.
  // Note that this won't prevent explicit selection setting from working.
  AutoTransactionsConserveSelection dontChangeMySelection(&TextEditorRef());

  switch (aInfo.mEditSubAction) {
    case EditSubAction::eDeleteSelectedContent:
      return DidDeleteSelection();
    case EditSubAction::eUndo:
      return DidUndo(aResult);
    case EditSubAction::eRedo:
      return DidRedo(aResult);
    default:
      // Don't fail on transactions we don't handle here!
      return NS_OK;
  }
}

bool
TextEditRules::DocumentIsEmpty()
{
  bool retVal = false;
  if (!mTextEditor || NS_FAILED(mTextEditor->DocumentIsEmpty(&retVal))) {
    retVal = true;
  }

  return retVal;
}

nsresult
TextEditRules::WillInsert(bool* aCancel)
{
  MOZ_ASSERT(IsEditorDataAvailable());

  if (IsReadonly() || IsDisabled()) {
    if (aCancel) {
      *aCancel = true;
    }
    return NS_OK;
  }

  // initialize out param
  if (aCancel) {
    *aCancel = false;
  }

  // check for the magic content node and delete it if it exists
  if (!mBogusNode) {
    return NS_OK;
  }

  DebugOnly<nsresult> rv =
    TextEditorRef().DeleteNodeWithTransaction(*mBogusNode);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
    "Failed to remove the bogus node");
  mBogusNode = nullptr;
  return NS_OK;
}

nsresult
TextEditRules::WillInsertBreak(bool* aCancel,
                               bool* aHandled,
                               int32_t aMaxLength)
{
  MOZ_ASSERT(IsEditorDataAvailable());
  if (NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
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
    nsresult rv =
      TruncateInsertionIfNeeded(&inString.AsString(),
                                &outString, aMaxLength, &didTruncate);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (didTruncate) {
      *aCancel = true;
      return NS_OK;
    }

    *aCancel = false;

    // if the selection isn't collapsed, delete it.
    if (!SelectionRef().IsCollapsed()) {
      rv = TextEditorRef().DeleteSelectionAsAction(nsIEditor::eNone,
                                                   nsIEditor::eStrip);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    rv = WillInsert();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
TextEditRules::CollapseSelectionToTrailingBRIfNeeded()
{
  MOZ_ASSERT(IsEditorDataAvailable());

  // we only need to execute the stuff below if we are a plaintext editor.
  // html editors have a different mechanism for putting in mozBR's
  // (because there are a bunch more places you have to worry about it in html)
  if (!IsPlaintextEditor()) {
    return NS_OK;
  }

  // If there is no selection ranges, we should set to the end of the editor.
  // This is usually performed in TextEditRules::Init(), however, if the
  // editor is reframed, this may be called by AfterEdit().
  if (!SelectionRef().RangeCount()) {
    TextEditorRef().CollapseSelectionToEnd(&SelectionRef());
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
  }

  // If we are at the end of the <textarea> element, we need to set the
  // selection to stick to the moz-<br> at the end of the <textarea>.
  EditorRawDOMPoint selectionStartPoint(
                      EditorBase::GetStartPoint(&SelectionRef()));
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
  if (!nextNode || !TextEditUtils::IsMozBR(nextNode)) {
    return NS_OK;
  }

  EditorRawDOMPoint afterStartContainer(selectionStartPoint.GetContainer());
  if (NS_WARN_IF(!afterStartContainer.AdvanceOffset())) {
    return NS_ERROR_FAILURE;
  }
  ErrorResult error;
  SelectionRef().Collapse(afterStartContainer, error);
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
TextEditRules::GetTextNodeAroundSelectionStartContainer()
{
  MOZ_ASSERT(IsEditorDataAvailable());

  EditorRawDOMPoint selectionStartPoint(
                      EditorBase::GetStartPoint(&SelectionRef()));
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
TextEditRules::WillInsertText(EditSubAction aEditSubAction,
                              bool* aCancel,
                              bool* aHandled,
                              const nsAString* inString,
                              nsAString* outString,
                              int32_t aMaxLength)
{
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
  uint32_t end = 0;

  // handle password field docs
  if (IsPasswordEditor()) {
    nsContentUtils::GetSelectionInTextControl(&SelectionRef(),
                                              TextEditorRef().GetRoot(),
                                              start, end);
  }

  // if the selection isn't collapsed, delete it.
  if (!SelectionRef().IsCollapsed()) {
    rv = TextEditorRef().DeleteSelectionAsAction(nsIEditor::eNone,
                                                 nsIEditor::eStrip);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = WillInsert(aCancel);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // handle password field data
  // this has the side effect of changing all the characters in aOutString
  // to the replacement character
  if (IsPasswordEditor() &&
      aEditSubAction == EditSubAction::eInsertTextComingFromIME) {
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
    HandleNewLines(tString, TextEditorRef().mNewlineHandling);
    outString->Assign(tString);
  }

  if (IsPasswordEditor()) {
    // manage the password buffer
    mPasswordText.Insert(*outString, start);

    if (LookAndFeel::GetEchoPassword() && !DontEchoPassword()) {
      nsresult rv = HideLastPWInput();
      mLastStart = start;
      mLastLength = outString->Length();
      if (mTimer) {
        mTimer->Cancel();
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (!mTimer) {
        mTimer = NS_NewTimer();
      }
      mTimer->InitWithCallback(this, LookAndFeel::GetPasswordMaskDelay(),
                               nsITimer::TYPE_ONE_SHOT);
    } else {
      FillBufWithPWChars(outString, outString->Length());
    }
  }

  // get the (collapsed) selection location
  nsRange* firstRange = SelectionRef().GetRangeAt(0);
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
  nsCOMPtr<nsIDocument> doc = TextEditorRef().GetDocument();
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (aEditSubAction == EditSubAction::eInsertTextComingFromIME) {
    // Find better insertion point to insert text.
    EditorRawDOMPoint betterInsertionPoint =
      TextEditorRef().FindBetterInsertionPoint(atStartOfSelection);
    // If there is one or more IME selections, its minimum offset should be
    // the insertion point.
    int32_t IMESelectionOffset =
      TextEditorRef().GetIMESelectionStartOffsetIn(
                        betterInsertionPoint.GetContainer());
    if (IMESelectionOffset >= 0) {
      betterInsertionPoint.Set(betterInsertionPoint.GetContainer(),
                               IMESelectionOffset);
    }
    rv = TextEditorRef().InsertTextWithTransaction(*doc, *outString,
                                                   betterInsertionPoint);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    // aEditSubAction == EditSubAction::eInsertText

    // don't change my selection in subtransactions
    AutoTransactionsConserveSelection dontChangeMySelection(&TextEditorRef());

    EditorRawDOMPoint pointAfterStringInserted;
    rv = TextEditorRef().InsertTextWithTransaction(*doc, *outString,
                                                   atStartOfSelection,
                                                   &pointAfterStringInserted);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (pointAfterStringInserted.IsSet()) {
      // Make the caret attach to the inserted text, unless this text ends with a LF,
      // in which case make the caret attach to the next line.
      bool endsWithLF =
        !outString->IsEmpty() && outString->Last() == nsCRT::LF;
      IgnoredErrorResult error;
      SelectionRef().SetInterlinePosition(endsWithLF, error);
      NS_WARNING_ASSERTION(!error.Failed(),
        "Failed to set or unset interline position");

      MOZ_ASSERT(!pointAfterStringInserted.GetChild(),
        "After inserting text into a text node, pointAfterStringInserted."
        "GetChild() should be nullptr");
      error = IgnoredErrorResult();
      SelectionRef().Collapse(pointAfterStringInserted, error);
      if (NS_WARN_IF(!CanHandleEditAction())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(!error.Failed(),
        "Failed to collapse selection after inserting string");
    }
  }
  ASSERT_PASSWORD_LENGTHS_EQUAL()
  return NS_OK;
}

nsresult
TextEditRules::WillSetText(bool* aCancel,
                           bool* aHandled,
                           const nsAString* aString,
                           int32_t aMaxLength)
{
  MOZ_ASSERT(IsEditorDataAvailable());
  MOZ_ASSERT(aCancel);
  MOZ_ASSERT(aHandled);
  MOZ_ASSERT(aString);

  CANCEL_OPERATION_IF_READONLY_OR_DISABLED

  *aHandled = false;
  *aCancel = false;

  if (!IsPlaintextEditor() || TextEditorRef().IsIMEComposing() ||
      aMaxLength != -1) {
    // SetTextImpl only supports plain text editor without IME.
    return NS_OK;
  }

  if (IsPasswordEditor() && LookAndFeel::GetEchoPassword() &&
      !DontEchoPassword()) {
    // Echo password timer will implement on InsertText.
    return NS_OK;
  }

  nsresult rv = WillInsert(aCancel);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<Element> rootElement = TextEditorRef().GetRoot();
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
    HandleNewLines(tString, TextEditorRef().mNewlineHandling);
  }

  if (!count) {
    if (tString.IsEmpty()) {
      *aHandled = true;
      return NS_OK;
    }
    RefPtr<nsIDocument> doc = TextEditorRef().GetDocument();
    if (NS_WARN_IF(!doc)) {
      return NS_OK;
    }
    RefPtr<nsTextNode> newNode = EditorBase::CreateTextNode(*doc, tString);
    if (NS_WARN_IF(!newNode)) {
      return NS_OK;
    }
    nsresult rv =
      TextEditorRef().InsertNodeWithTransaction(
                        *newNode, EditorRawDOMPoint(rootElement, 0));
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
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
  rv = TextEditorRef().SetTextImpl(SelectionRef(), tString,
                                   *curNode->GetAsText());
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aHandled = true;

  ASSERT_PASSWORD_LENGTHS_EQUAL();

  return NS_OK;
}

nsresult
TextEditRules::WillSetTextProperty(bool* aCancel,
                                   bool* aHandled)
{
  if (NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
  }

  // XXX: should probably return a success value other than NS_OK that means "not allowed"
  if (IsPlaintextEditor()) {
    *aCancel = true;
  }
  return NS_OK;
}

nsresult
TextEditRules::WillRemoveTextProperty(bool* aCancel,
                                      bool* aHandled)
{
  if (NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
  }

  // XXX: should probably return a success value other than NS_OK that means "not allowed"
  if (IsPlaintextEditor()) {
    *aCancel = true;
  }
  return NS_OK;
}

nsresult
TextEditRules::WillDeleteSelection(nsIEditor::EDirection aCollapsedAction,
                                   bool* aCancel,
                                   bool* aHandled)
{
  MOZ_ASSERT(IsEditorDataAvailable());

  if (NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
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

nsresult
TextEditRules::DeleteSelectionWithTransaction(
                 nsIEditor::EDirection aCollapsedAction,
                 bool* aCancel,
                 bool* aHandled)
{
  MOZ_ASSERT(IsEditorDataAvailable());
  MOZ_ASSERT(aCancel);
  MOZ_ASSERT(aHandled);

  // If the current selection is empty (e.g the user presses backspace with
  // a collapsed selection), then we want to avoid sending the selectstart
  // event to the user, so we hide selection changes. However, we still
  // want to send a single selectionchange event to the document, so we
  // batch the selectionchange events, such that a single event fires after
  // the AutoHideSelectionChanges destructor has been run.
  SelectionBatcher selectionBatcher(&SelectionRef());
  AutoHideSelectionChanges hideSelection(&SelectionRef());
  nsAutoScriptBlocker scriptBlocker;

  if (IsPasswordEditor()) {
    nsresult rv =
      TextEditorRef().ExtendSelectionForDelete(&SelectionRef(),
                                               &aCollapsedAction);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // manage the password buffer
    uint32_t start, end;
    nsContentUtils::GetSelectionInTextControl(&SelectionRef(),
                                              TextEditorRef().GetRoot(),
                                              start, end);

    if (LookAndFeel::GetEchoPassword()) {
      rv = HideLastPWInput();
      mLastStart = start;
      mLastLength = 0;
      if (mTimer) {
        mTimer->Cancel();
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
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
    EditorRawDOMPoint selectionStartPoint(
                        EditorBase::GetStartPoint(&SelectionRef()));
    if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    if (!SelectionRef().IsCollapsed()) {
      return NS_OK;
    }

    // Test for distance between caret and text that will be deleted
    nsresult rv =
      CheckBidiLevelForDeletion(selectionStartPoint, aCollapsedAction, aCancel);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (*aCancel) {
      return NS_OK;
    }

    rv = TextEditorRef().ExtendSelectionForDelete(&SelectionRef(),
                                                  &aCollapsedAction);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsresult rv =
    TextEditorRef().DeleteSelectionWithTransaction(aCollapsedAction,
                                                   nsIEditor::eStrip);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aHandled = true;
  ASSERT_PASSWORD_LENGTHS_EQUAL()
  return NS_OK;
}

nsresult
TextEditRules::DidDeleteSelection()
{
  MOZ_ASSERT(IsEditorDataAvailable());

  EditorRawDOMPoint selectionStartPoint(
                      EditorBase::GetStartPoint(&SelectionRef()));
  if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // Delete empty text nodes at selection.
  if (selectionStartPoint.IsInTextNode() &&
      !selectionStartPoint.GetContainer()->Length()) {
    nsresult rv =
      TextEditorRef().DeleteNodeWithTransaction(
                        *selectionStartPoint.GetContainer());
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (mDidExplicitlySetInterline) {
    return NS_OK;
  }
  // We prevent the caret from sticking on the left of prior BR
  // (i.e. the end of previous line) after this deletion.  Bug 92124
  ErrorResult err;
  SelectionRef().SetInterlinePosition(true, err);
  NS_WARNING_ASSERTION(!err.Failed(), "Failed to set interline position");
  return err.StealNSResult();
}

nsresult
TextEditRules::WillUndo(bool* aCancel,
                        bool* aHandled)
{
  if (NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
  }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  // initialize out param
  *aCancel = false;
  *aHandled = false;
  return NS_OK;
}

nsresult
TextEditRules::DidUndo(nsresult aResult)
{
  MOZ_ASSERT(IsEditorDataAvailable());

  // If aResult is an error, we return it.
  if (NS_WARN_IF(NS_FAILED(aResult))) {
    return aResult;
  }

  Element* rootElement = TextEditorRef().GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_FAILURE;
  }

  // The idea here is to see if the magic empty node has suddenly reappeared as
  // the result of the undo.  If it has, set our state so we remember it.
  // There is a tradeoff between doing here and at redo, or doing it everywhere
  // else that might care.  Since undo and redo are relatively rare, it makes
  // sense to take the (small) performance hit here.
  nsIContent* node = TextEditorRef().GetLeftmostChild(rootElement);
  if (node && TextEditorRef().IsMozEditorBogusNode(node)) {
    mBogusNode = node;
  } else {
    mBogusNode = nullptr;
  }
  return aResult;
}

nsresult
TextEditRules::WillRedo(bool* aCancel,
                        bool* aHandled)
{
  if (NS_WARN_IF(!aCancel) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
  }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  // initialize out param
  *aCancel = false;
  *aHandled = false;
  return NS_OK;
}

nsresult
TextEditRules::DidRedo(nsresult aResult)
{
  MOZ_ASSERT(IsEditorDataAvailable());

  if (NS_FAILED(aResult)) {
    return aResult; // if aResult is an error, we return it.
  }

  Element* rootElement = TextEditorRef().GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIHTMLCollection> nodeList =
    rootElement->GetElementsByTagName(NS_LITERAL_STRING("br"));
  MOZ_ASSERT(nodeList);
  uint32_t len = nodeList->Length();

  if (len != 1) {
    // only in the case of one br could there be the bogus node
    mBogusNode = nullptr;
    return NS_OK;
  }

  Element* brElement = nodeList->Item(0);
  if (TextEditorRef().IsMozEditorBogusNode(brElement)) {
    mBogusNode = brElement;
  } else {
    mBogusNode = nullptr;
  }
  return NS_OK;
}

nsresult
TextEditRules::WillOutputText(const nsAString* aOutputFormat,
                              nsAString* aOutString,
                              uint32_t aFlags,
                              bool* aCancel,
                              bool* aHandled)
{
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
  if (TextEditorRef().AsHTMLEditor()) {
    return NS_OK;
  }

  Element* root = TextEditorRef().GetRoot();
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
  text->GetData(*aOutString);

  *aHandled = true;
  return NS_OK;
}

nsresult
TextEditRules::RemoveRedundantTrailingBR()
{
  MOZ_ASSERT(IsEditorDataAvailable());

  // If the bogus node exists, we have no work to do
  if (mBogusNode) {
    return NS_OK;
  }

  // Likewise, nothing to be done if we could never have inserted a trailing br
  if (IsSingleLineEditor()) {
    return NS_OK;
  }

  Element* rootElement = TextEditorRef().GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_NULL_POINTER;
  }

  uint32_t childCount = rootElement->GetChildCount();
  if (childCount > 1) {
    // The trailing br is redundant if it is the only remaining child node
    return NS_OK;
  }

  RefPtr<nsIContent> child = rootElement->GetFirstChild();
  if (!child || !child->IsElement()) {
    return NS_OK;
  }

  RefPtr<Element> childElement = child->AsElement();
  if (!TextEditUtils::IsMozBR(childElement)) {
    return NS_OK;
  }

  // Rather than deleting this node from the DOM tree we should instead
  // morph this br into the bogus node
  childElement->UnsetAttr(kNameSpaceID_None, nsGkAtoms::type, true);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }

  // set mBogusNode to be this <br>
  mBogusNode = childElement;

  // give it the bogus node attribute
  childElement->SetAttr(kNameSpaceID_None, kMOZEditorBogusNodeAttrAtom,
                        kMOZEditorBogusNodeValue, false);
  return NS_OK;
}

nsresult
TextEditRules::CreateTrailingBRIfNeeded()
{
  MOZ_ASSERT(IsEditorDataAvailable());

  // but only if we aren't a single line edit field
  if (IsSingleLineEditor()) {
    return NS_OK;
  }

  Element* rootElement = TextEditorRef().GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContent> lastChild = rootElement->GetLastChild();
  // assuming CreateBogusNodeIfNeeded() has been called first
  if (NS_WARN_IF(!lastChild)) {
    return NS_ERROR_FAILURE;
  }

  if (!lastChild->IsHTMLElement(nsGkAtoms::br)) {
    AutoTransactionsConserveSelection dontChangeMySelection(&TextEditorRef());
    EditorRawDOMPoint endOfRoot;
    endOfRoot.SetToEndOf(rootElement);
    CreateElementResult createMozBrResult = CreateMozBR(endOfRoot);
    if (NS_WARN_IF(createMozBrResult.Failed())) {
      return createMozBrResult.Rv();
    }
    return NS_OK;
  }

  // Check to see if the trailing BR is a former bogus node - this will have
  // stuck around if we previously morphed a trailing node into a bogus node.
  if (!TextEditorRef().IsMozEditorBogusNode(lastChild)) {
    return NS_OK;
  }

  // Morph it back to a mozBR
  lastChild->AsElement()->UnsetAttr(kNameSpaceID_None,
                                    kMOZEditorBogusNodeAttrAtom,
                                    false);
  lastChild->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                                  NS_LITERAL_STRING("_moz"),
                                  true);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  return NS_OK;
}

nsresult
TextEditRules::CreateBogusNodeIfNeeded()
{
  MOZ_ASSERT(IsEditorDataAvailable());

  if (mBogusNode) {
    // Let's not create more than one, ok?
    return NS_OK;
  }

  // tell rules system to not do any post-processing
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
                                      TextEditorRef(),
                                      EditSubAction::eCreateBogusNode,
                                      nsIEditor::eNone);

  RefPtr<Element> rootElement = TextEditorRef().GetRoot();
  if (!rootElement) {
    // We don't even have a body yet, don't insert any bogus nodes at
    // this point.
    return NS_OK;
  }

  // Now we've got the body element. Iterate over the body element's children,
  // looking for editable content. If no editable content is found, insert the
  // bogus node.
  bool isRootEditable = TextEditorRef().IsEditable(rootElement);
  for (nsIContent* rootChild = rootElement->GetFirstChild();
       rootChild;
       rootChild = rootChild->GetNextSibling()) {
    if (TextEditorRef().IsMozEditorBogusNode(rootChild) ||
        !isRootEditable ||
        TextEditorRef().IsEditable(rootChild) ||
        TextEditorRef().IsBlockNode(rootChild)) {
      return NS_OK;
    }
  }

  // Skip adding the bogus node if body is read-only.
  if (!TextEditorRef().IsModifiableNode(rootElement)) {
    return NS_OK;
  }

  // Create a br.
  RefPtr<Element> newBrElement =
    TextEditorRef().CreateHTMLContent(nsGkAtoms::br);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(!newBrElement)) {
    return NS_ERROR_FAILURE;
  }

  // set mBogusNode to be the newly created <br>
  mBogusNode = newBrElement;

  // Give it a special attribute.
  newBrElement->SetAttr(kNameSpaceID_None, kMOZEditorBogusNodeAttrAtom,
                        kMOZEditorBogusNodeValue, false);

  // Put the node in the document.
  nsresult rv =
    TextEditorRef().InsertNodeWithTransaction(
                      *mBogusNode, EditorRawDOMPoint(rootElement, 0));
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Set selection.
  IgnoredErrorResult error;
  SelectionRef().Collapse(EditorRawDOMPoint(rootElement, 0), error);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(!error.Failed(),
    "Failed to collapse selection at start of the root element");
  return NS_OK;
}


nsresult
TextEditRules::TruncateInsertionIfNeeded(const nsAString* aInString,
                                         nsAString* aOutString,
                                         int32_t aMaxLength,
                                         bool* aTruncated)
{
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
    nsContentUtils::GetSelectionInTextControl(&SelectionRef(),
                                              TextEditorRef().GetRoot(),
                                              start, end);

    TextComposition* composition = TextEditorRef().GetComposition();
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

  if (NS_WARN_IF(!mTextEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  Selection* selection = mTextEditor->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }

  AutoSafeEditorData setData(*this, *mTextEditor, *selection);

  // Check whether our text editor's password flag was changed before this
  // "hide password character" timer actually fires.
  nsresult rv = IsPasswordEditor() ? HideLastPWInput() : NS_OK;
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to hide last password input");
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
  MOZ_ASSERT(IsEditorDataAvailable());

  if (!mLastLength) {
    // Special case, we're trying to replace a range that no longer exists
    return NS_OK;
  }

  nsAutoString hiddenText;
  FillBufWithPWChars(&hiddenText, mLastLength);

  uint32_t start, end;
  nsContentUtils::GetSelectionInTextControl(&SelectionRef(),
                                            TextEditorRef().GetRoot(),
                                            start, end);

  nsCOMPtr<nsINode> selNode = GetTextNodeAroundSelectionStartContainer();
  if (NS_WARN_IF(!selNode)) {
    return NS_OK;
  }

  selNode->GetAsText()->ReplaceData(mLastStart, mLastLength, hiddenText,
                                    IgnoreErrors());
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  // XXXbz Selection::Collapse/Extend take int32_t, but there are tons of
  // callsites... Converting all that is a battle for another day.
  DebugOnly<nsresult> rv = SelectionRef().Collapse(selNode, start);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to collapse selection");
  if (start != end) {
    rv = SelectionRef().Extend(selNode, end);
    if (NS_WARN_IF(!CanHandleEditAction())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to extend selection");
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

template<typename PT, typename CT>
CreateElementResult
TextEditRules::CreateBRInternal(
                 const EditorDOMPointBase<PT, CT>& aPointToInsert,
                 bool aCreateMozBR)
{
  MOZ_ASSERT(IsEditorDataAvailable());

  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return CreateElementResult(NS_ERROR_FAILURE);
  }

  RefPtr<Element> brElement =
    TextEditorRef().InsertBrElementWithTransaction(SelectionRef(),
                                                   aPointToInsert);
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return CreateElementResult(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_WARN_IF(!brElement)) {
    return CreateElementResult(NS_ERROR_FAILURE);
  }

  // give it special moz attr
  if (!aCreateMozBR) {
    return CreateElementResult(brElement.forget());
  }

  // XXX Why do we need to set this attribute with transaction?
  nsresult rv =
    TextEditorRef().SetAttributeWithTransaction(*brElement, *nsGkAtoms::type,
                                                NS_LITERAL_STRING("_moz"));
  // XXX Don't we need to remove the new <br> element from the DOM tree
  //     in these case?
  if (NS_WARN_IF(!CanHandleEditAction())) {
    return CreateElementResult(NS_ERROR_EDITOR_DESTROYED);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return CreateElementResult(NS_ERROR_FAILURE);
  }
  return CreateElementResult(brElement.forget());
}

nsresult
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
