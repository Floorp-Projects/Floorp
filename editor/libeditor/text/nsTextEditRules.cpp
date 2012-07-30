/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/Preferences.h"
#include "mozilla/Selection.h"
#include "mozilla/dom/Element.h"
#include "nsAString.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsEditor.h"
#include "nsEditorUtils.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeFilter.h"
#include "nsIDOMNodeIterator.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMText.h"
#include "nsINameSpaceManager.h"
#include "nsINode.h"
#include "nsIPlaintextEditor.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsISupportsBase.h"
#include "nsLiteralString.h"
#include "nsNodeIterator.h"
#include "nsTextEditRules.h"
#include "nsTextEditUtils.h"
#include "nsUnicharUtils.h"

using namespace mozilla;

#define CANCEL_OPERATION_IF_READONLY_OR_DISABLED \
  if (IsReadonly() || IsDisabled()) \
  {                     \
    *aCancel = true; \
    return NS_OK;       \
  };


/********************************************************
 *  Constructor/Destructor 
 ********************************************************/

nsTextEditRules::nsTextEditRules()
: mEditor(nullptr)
, mPasswordText()
, mPasswordIMEText()
, mPasswordIMEIndex(0)
, mActionNesting(0)
, mLockRulesSniffing(false)
, mDidExplicitlySetInterline(false)
, mTheAction(nsEditor::kOpNone)
, mLastStart(0)
, mLastLength(0)
{
}

nsTextEditRules::~nsTextEditRules()
{
   // do NOT delete mEditor here.  We do not hold a ref count to mEditor.  mEditor owns our lifespan.

  if (mTimer)
    mTimer->Cancel();
}

/********************************************************
 *  XPCOM Cruft
 ********************************************************/

NS_IMPL_CYCLE_COLLECTION_2(nsTextEditRules, mBogusNode, mCachedSelectionNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsTextEditRules)
  NS_INTERFACE_MAP_ENTRY(nsIEditRules)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEditRules)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsTextEditRules)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsTextEditRules)

/********************************************************
 *  Public methods 
 ********************************************************/

NS_IMETHODIMP
nsTextEditRules::Init(nsPlaintextEditor *aEditor)
{
  if (!aEditor) { return NS_ERROR_NULL_POINTER; }

  mEditor = aEditor;  // we hold a non-refcounted reference back to our editor
  nsCOMPtr<nsISelection> selection;
  mEditor->GetSelection(getter_AddRefs(selection));
  NS_ASSERTION(selection, "editor cannot get selection");

  // Put in a magic br if needed. This method handles null selection,
  // which should never happen anyway
  nsresult res = CreateBogusNodeIfNeeded(selection);
  NS_ENSURE_SUCCESS(res, res);

  // If the selection hasn't been set up yet, set it up collapsed to the end of
  // our editable content.
  PRInt32 rangeCount;
  res = selection->GetRangeCount(&rangeCount);
  NS_ENSURE_SUCCESS(res, res);
  if (!rangeCount) {
    res = mEditor->EndOfDocument();
    NS_ENSURE_SUCCESS(res, res);
  }

  if (IsPlaintextEditor())
  {
    // ensure trailing br node
    res = CreateTrailingBRIfNeeded();
    NS_ENSURE_SUCCESS(res, res);
  }

  mDeleteBidiImmediately =
    Preferences::GetBool("bidi.edit.delete_immediately", false);

  return res;
}

NS_IMETHODIMP
nsTextEditRules::DetachEditor()
{
  if (mTimer)
    mTimer->Cancel();

  mEditor = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsTextEditRules::BeforeEdit(nsEditor::OperationID action,
                            nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) return NS_OK;
  
  nsAutoLockRulesSniffing lockIt(this);
  mDidExplicitlySetInterline = false;
  if (!mActionNesting)
  {
    // let rules remember the top level action
    mTheAction = action;
  }
  mActionNesting++;
  
  // get the selection and cache the position before editing
  nsCOMPtr<nsISelection> selection;
  nsresult res = mEditor->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);

  selection->GetAnchorNode(getter_AddRefs(mCachedSelectionNode));
  selection->GetAnchorOffset(&mCachedSelectionOffset);

  return NS_OK;
}


NS_IMETHODIMP
nsTextEditRules::AfterEdit(nsEditor::OperationID action,
                           nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) return NS_OK;
  
  nsAutoLockRulesSniffing lockIt(this);
  
  NS_PRECONDITION(mActionNesting>0, "bad action nesting!");
  nsresult res = NS_OK;
  if (!--mActionNesting)
  {
    nsCOMPtr<nsISelection>selection;
    res = mEditor->GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(res, res);
  
    res = mEditor->HandleInlineSpellCheck(action, selection,
                                          mCachedSelectionNode, mCachedSelectionOffset,
                                          nullptr, 0, nullptr, 0);
    NS_ENSURE_SUCCESS(res, res);

    // if only trailing <br> remaining remove it
    res = RemoveRedundantTrailingBR();
    if (NS_FAILED(res))
      return res;

    // detect empty doc
    res = CreateBogusNodeIfNeeded(selection);
    NS_ENSURE_SUCCESS(res, res);
    
    // ensure trailing br node
    res = CreateTrailingBRIfNeeded();
    NS_ENSURE_SUCCESS(res, res);

    // collapse the selection to the trailing BR if it's at the end of our text node
    CollapseSelectionToTrailingBRIfNeeded(selection);
  }
  return res;
}


NS_IMETHODIMP
nsTextEditRules::WillDoAction(Selection* aSelection,
                              nsRulesInfo* aInfo,
                              bool* aCancel,
                              bool* aHandled)
{
  // null selection is legal
  MOZ_ASSERT(aInfo && aCancel && aHandled);

  *aCancel = false;
  *aHandled = false;

  // my kingdom for dynamic cast
  nsTextRulesInfo *info = static_cast<nsTextRulesInfo*>(aInfo);

  switch (info->action) {
    case nsEditor::kOpInsertBreak:
      return WillInsertBreak(aSelection, aCancel, aHandled, info->maxLength);
    case nsEditor::kOpInsertText:
    case nsEditor::kOpInsertIMEText:
      return WillInsertText(info->action, aSelection, aCancel, aHandled,
                            info->inString, info->outString, info->maxLength);
    case nsEditor::kOpDeleteSelection:
      return WillDeleteSelection(aSelection, info->collapsedAction,
                                 aCancel, aHandled);
    case nsEditor::kOpUndo:
      return WillUndo(aSelection, aCancel, aHandled);
    case nsEditor::kOpRedo:
      return WillRedo(aSelection, aCancel, aHandled);
    case nsEditor::kOpSetTextProperty:
      return WillSetTextProperty(aSelection, aCancel, aHandled);
    case nsEditor::kOpRemoveTextProperty:
      return WillRemoveTextProperty(aSelection, aCancel, aHandled);
    case nsEditor::kOpOutputText:
      return WillOutputText(aSelection, info->outputFormat, info->outString,
                            aCancel, aHandled);
    case nsEditor::kOpInsertElement:
      // i had thought this would be html rules only.  but we put pre elements
      // into plaintext mail when doing quoting for reply!  doh!
      return WillInsert(aSelection, aCancel);
    default:
      return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP 
nsTextEditRules::DidDoAction(nsISelection *aSelection,
                             nsRulesInfo *aInfo, nsresult aResult)
{
  // don't let any txns in here move the selection around behind our back.
  // Note that this won't prevent explicit selection setting from working.
  nsAutoTxnsConserveSelection dontSpazMySelection(mEditor);

  NS_ENSURE_TRUE(aSelection && aInfo, NS_ERROR_NULL_POINTER);
    
  // my kingdom for dynamic cast
  nsTextRulesInfo *info = static_cast<nsTextRulesInfo*>(aInfo);

  switch (info->action)
  {
    case nsEditor::kOpInsertBreak:
      return DidInsertBreak(aSelection, aResult);
    case nsEditor::kOpInsertText:
    case nsEditor::kOpInsertIMEText:
      return DidInsertText(aSelection, aResult);
    case nsEditor::kOpDeleteSelection:
      return DidDeleteSelection(aSelection, info->collapsedAction, aResult);
    case nsEditor::kOpUndo:
      return DidUndo(aSelection, aResult);
    case nsEditor::kOpRedo:
      return DidRedo(aSelection, aResult);
    case nsEditor::kOpSetTextProperty:
      return DidSetTextProperty(aSelection, aResult);
    case nsEditor::kOpRemoveTextProperty:
      return DidRemoveTextProperty(aSelection, aResult);
    case nsEditor::kOpOutputText:
      return DidOutputText(aSelection, aResult);
    default:
      // Don't fail on transactions we don't handle here!
      return NS_OK;
  }
}


NS_IMETHODIMP
nsTextEditRules::DocumentIsEmpty(bool *aDocumentIsEmpty)
{
  NS_ENSURE_TRUE(aDocumentIsEmpty, NS_ERROR_NULL_POINTER);
  
  *aDocumentIsEmpty = (mBogusNode != nullptr);
  return NS_OK;
}

/********************************************************
 *  Protected methods 
 ********************************************************/


nsresult
nsTextEditRules::WillInsert(nsISelection *aSelection, bool *aCancel)
{
  NS_ENSURE_TRUE(aSelection && aCancel, NS_ERROR_NULL_POINTER);
  
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED

  // initialize out param
  *aCancel = false;
  
  // check for the magic content node and delete it if it exists
  if (mBogusNode)
  {
    mEditor->DeleteNode(mBogusNode);
    mBogusNode = nullptr;
  }

  return NS_OK;
}

nsresult
nsTextEditRules::DidInsert(nsISelection *aSelection, nsresult aResult)
{
  return NS_OK;
}

nsresult
nsTextEditRules::WillInsertBreak(Selection* aSelection,
                                 bool *aCancel,
                                 bool *aHandled,
                                 PRInt32 aMaxLength)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  *aHandled = false;
  if (IsSingleLineEditor()) {
    *aCancel = true;
  }
  else 
  {
    // handle docs with a max length
    // NOTE, this function copies inString into outString for us.
    NS_NAMED_LITERAL_STRING(inString, "\n");
    nsAutoString outString;
    bool didTruncate;
    nsresult res = TruncateInsertionIfNeeded(aSelection, &inString, &outString,
                                             aMaxLength, &didTruncate);
    NS_ENSURE_SUCCESS(res, res);
    if (didTruncate) {
      *aCancel = true;
      return NS_OK;
    }

    *aCancel = false;

    // if the selection isn't collapsed, delete it.
    bool bCollapsed;
    res = aSelection->GetIsCollapsed(&bCollapsed);
    NS_ENSURE_SUCCESS(res, res);
    if (!bCollapsed)
    {
      res = mEditor->DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);
      NS_ENSURE_SUCCESS(res, res);
    }

    res = WillInsert(aSelection, aCancel);
    NS_ENSURE_SUCCESS(res, res);
    // initialize out param
    // we want to ignore result of WillInsert()
    *aCancel = false;
  
  }
  return NS_OK;
}

nsresult
nsTextEditRules::DidInsertBreak(nsISelection *aSelection, nsresult aResult)
{
  return NS_OK;
}

nsresult
nsTextEditRules::CollapseSelectionToTrailingBRIfNeeded(nsISelection* aSelection)
{
  // we only need to execute the stuff below if we are a plaintext editor.
  // html editors have a different mechanism for putting in mozBR's
  // (because there are a bunch more places you have to worry about it in html) 
  if (!IsPlaintextEditor()) {
    return NS_OK;
  }

  // if we are at the end of the textarea, we need to set the
  // selection to stick to the mozBR at the end of the textarea.
  PRInt32 selOffset;
  nsCOMPtr<nsIDOMNode> selNode;
  nsresult res;
  res = mEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsIDOMText> nodeAsText = do_QueryInterface(selNode);
  if (!nodeAsText) return NS_OK; // nothing to do if we're not at a text node

  PRUint32 length;
  res = nodeAsText->GetLength(&length);
  NS_ENSURE_SUCCESS(res, res);

  // nothing to do if we're not at the end of the text node
  if (selOffset != PRInt32(length))
    return NS_OK;

  PRInt32 parentOffset;
  nsCOMPtr<nsIDOMNode> parentNode = nsEditor::GetNodeLocation(selNode, &parentOffset);

  nsCOMPtr<nsIDOMNode> root = do_QueryInterface(mEditor->GetRoot());
  NS_ENSURE_TRUE(root, NS_ERROR_NULL_POINTER);
  if (parentNode != root) return NS_OK;

  nsCOMPtr<nsIDOMNode> nextNode = mEditor->GetChildAt(parentNode,
                                                      parentOffset + 1);
  if (nextNode && nsTextEditUtils::IsMozBR(nextNode))
  {
    res = aSelection->Collapse(parentNode, parentOffset + 1);
    NS_ENSURE_SUCCESS(res, res);
  }
  return res;
}

static inline already_AddRefed<nsIDOMNode>
GetTextNode(nsISelection *selection, nsEditor *editor) {
  PRInt32 selOffset;
  nsCOMPtr<nsIDOMNode> selNode;
  nsresult res = editor->GetStartNodeAndOffset(selection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, nullptr);
  if (!editor->IsTextNode(selNode)) {
    // Get an nsINode from the nsIDOMNode
    nsCOMPtr<nsINode> node = do_QueryInterface(selNode);
    // if node is null, return it to indicate there's no text
    NS_ENSURE_TRUE(node, nullptr);
    // This should be the root node, walk the tree looking for text nodes
    nsNodeIterator iter(node, nsIDOMNodeFilter::SHOW_TEXT, nullptr);
    while (!editor->IsTextNode(selNode)) {
      if (NS_FAILED(res = iter.NextNode(getter_AddRefs(selNode))) || !selNode) {
        return nullptr;
      }
    }
  }
  return selNode.forget();
}
#ifdef DEBUG
#define ASSERT_PASSWORD_LENGTHS_EQUAL()                                \
  if (IsPasswordEditor()) {                                            \
    PRInt32 txtLen;                                                    \
    mEditor->GetTextLength(&txtLen);                                    \
    NS_ASSERTION(mPasswordText.Length() == PRUint32(txtLen),           \
                 "password length not equal to number of asterisks");  \
  }
#else
#define ASSERT_PASSWORD_LENGTHS_EQUAL()
#endif

// static
void
nsTextEditRules::HandleNewLines(nsString &aString,
                                PRInt32 aNewlineHandling)
{
  if (aNewlineHandling < 0) {
    PRInt32 caretStyle;
    nsPlaintextEditor::GetDefaultEditorPrefs(aNewlineHandling, caretStyle);
  }

  switch(aNewlineHandling)
  {
  case nsIPlaintextEditor::eNewlinesReplaceWithSpaces:
    // Strip trailing newlines first so we don't wind up with trailing spaces
    aString.Trim(CRLF, false, true);
    aString.ReplaceChar(CRLF, ' ');
    break;
  case nsIPlaintextEditor::eNewlinesStrip:
    aString.StripChars(CRLF);
    break;
  case nsIPlaintextEditor::eNewlinesPasteToFirst:
  default:
    {
      PRInt32 firstCRLF = aString.FindCharInSet(CRLF);

      // we get first *non-empty* line.
      PRInt32 offset = 0;
      while (firstCRLF == offset)
      {
        offset++;
        firstCRLF = aString.FindCharInSet(CRLF, offset);
      }
      if (firstCRLF > 0)
        aString.Truncate(firstCRLF);
      if (offset > 0)
        aString.Cut(0, offset);
    }
    break;
  case nsIPlaintextEditor::eNewlinesReplaceWithCommas:
    aString.Trim(CRLF, true, true);
    aString.ReplaceChar(CRLF, ',');
    break;
  case nsIPlaintextEditor::eNewlinesStripSurroundingWhitespace:
    {
      // find each newline, and strip all the whitespace before
      // and after it
      PRInt32 firstCRLF = aString.FindCharInSet(CRLF);
      while (firstCRLF >= 0)
      {
        PRUint32 wsBegin = firstCRLF, wsEnd = firstCRLF + 1;
        // look backwards for the first non-whitespace char
        while (wsBegin > 0 && NS_IS_SPACE(aString[wsBegin - 1]))
          --wsBegin;
        while (wsEnd < aString.Length() && NS_IS_SPACE(aString[wsEnd]))
          ++wsEnd;
        // now cut this range out of the string
        aString.Cut(wsBegin, wsEnd - wsBegin);
        // look for another CR or LF
        firstCRLF = aString.FindCharInSet(CRLF);
      }
    }
    break;
  case nsIPlaintextEditor::eNewlinesPasteIntact:
    // even if we're pasting newlines, don't paste leading/trailing ones
    aString.Trim(CRLF, true, true);
    break;
  }
}

nsresult
nsTextEditRules::WillInsertText(nsEditor::OperationID aAction,
                                Selection* aSelection,
                                bool            *aCancel,
                                bool            *aHandled,
                                const nsAString *inString,
                                nsAString *outString,
                                PRInt32          aMaxLength)
{  
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }

  if (inString->IsEmpty() && aAction != nsEditor::kOpInsertIMEText) {
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
  nsresult res = TruncateInsertionIfNeeded(aSelection, inString, outString,
                                           aMaxLength, &truncated);
  NS_ENSURE_SUCCESS(res, res);
  // If we're exceeding the maxlength when composing IME, we need to clean up
  // the composing text, so we shouldn't return early.
  if (truncated && outString->IsEmpty() &&
      aAction != nsEditor::kOpInsertIMEText) {
    *aCancel = true;
    return NS_OK;
  }
  
  PRInt32 start = 0;
  PRInt32 end = 0;

  // handle password field docs
  if (IsPasswordEditor()) {
    nsContentUtils::GetSelectionInTextControl(aSelection, mEditor->GetRoot(),
                                              start, end);
  }

  // if the selection isn't collapsed, delete it.
  bool bCollapsed;
  res = aSelection->GetIsCollapsed(&bCollapsed);
  NS_ENSURE_SUCCESS(res, res);
  if (!bCollapsed)
  {
    res = mEditor->DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);
    NS_ENSURE_SUCCESS(res, res);
  }

  res = WillInsert(aSelection, aCancel);
  NS_ENSURE_SUCCESS(res, res);
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;
  
  // handle password field data
  // this has the side effect of changing all the characters in aOutString
  // to the replacement character
  if (IsPasswordEditor())
  {
    if (aAction == nsEditor::kOpInsertIMEText) {
      RemoveIMETextFromPWBuf(start, outString);
    }
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
  if (IsSingleLineEditor())
  {
    nsAutoString tString(*outString);

    HandleNewLines(tString, mEditor->mNewlineHandling);

    outString->Assign(tString);
  }

  if (IsPasswordEditor())
  {
    // manage the password buffer
    mPasswordText.Insert(*outString, start);

    if (LookAndFeel::GetEchoPassword() && !DontEchoPassword()) {
      HideLastPWInput();
      mLastStart = start;
      mLastLength = outString->Length();
      if (mTimer)
      {
        mTimer->Cancel();
      }
      else
      {
        mTimer = do_CreateInstance("@mozilla.org/timer;1", &res);
        NS_ENSURE_SUCCESS(res, res);
      }
      mTimer->InitWithCallback(this, LookAndFeel::GetPasswordMaskDelay(),
                               nsITimer::TYPE_ONE_SHOT);
    } 
    else 
    {
      FillBufWithPWChars(outString, outString->Length());
    }
  }

  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;
  res = mEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);

  // don't put text in places that can't have it
  if (!mEditor->IsTextNode(selNode) &&
      !mEditor->CanContainTag(selNode, nsGkAtoms::textTagName)) {
    return NS_ERROR_FAILURE;
  }

  // we need to get the doc
  nsCOMPtr<nsIDOMDocument> doc = mEditor->GetDOMDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);
    
  if (aAction == nsEditor::kOpInsertIMEText) {
    res = mEditor->InsertTextImpl(*outString, address_of(selNode), &selOffset, doc);
    NS_ENSURE_SUCCESS(res, res);
  } else {
    // aAction == nsEditor::kOpInsertText; find where we are
    nsCOMPtr<nsIDOMNode> curNode = selNode;
    PRInt32 curOffset = selOffset;

    // don't spaz my selection in subtransactions
    nsAutoTxnsConserveSelection dontSpazMySelection(mEditor);

    res = mEditor->InsertTextImpl(*outString, address_of(curNode),
                                  &curOffset, doc);
    NS_ENSURE_SUCCESS(res, res);

    if (curNode) 
    {
      // Make the caret attach to the inserted text, unless this text ends with a LF, 
      // in which case make the caret attach to the next line.
      bool endsWithLF =
        !outString->IsEmpty() && outString->Last() == nsCRT::LF;
      aSelection->SetInterlinePosition(endsWithLF);

      aSelection->Collapse(curNode, curOffset);
    }
  }
  ASSERT_PASSWORD_LENGTHS_EQUAL()
  return res;
}

nsresult
nsTextEditRules::DidInsertText(nsISelection *aSelection, 
                               nsresult aResult)
{
  return DidInsert(aSelection, aResult);
}



nsresult
nsTextEditRules::WillSetTextProperty(nsISelection *aSelection, bool *aCancel, bool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) 
    { return NS_ERROR_NULL_POINTER; }

  // XXX: should probably return a success value other than NS_OK that means "not allowed"
  if (IsPlaintextEditor()) {
    *aCancel = true;
  }
  return NS_OK;
}

nsresult
nsTextEditRules::DidSetTextProperty(nsISelection *aSelection, nsresult aResult)
{
  return NS_OK;
}

nsresult
nsTextEditRules::WillRemoveTextProperty(nsISelection *aSelection, bool *aCancel, bool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) 
    { return NS_ERROR_NULL_POINTER; }

  // XXX: should probably return a success value other than NS_OK that means "not allowed"
  if (IsPlaintextEditor()) {
    *aCancel = true;
  }
  return NS_OK;
}

nsresult
nsTextEditRules::DidRemoveTextProperty(nsISelection *aSelection, nsresult aResult)
{
  return NS_OK;
}

nsresult
nsTextEditRules::WillDeleteSelection(Selection* aSelection,
                                     nsIEditor::EDirection aCollapsedAction, 
                                     bool *aCancel,
                                     bool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED

  // initialize out param
  *aCancel = false;
  *aHandled = false;
  
  // if there is only bogus content, cancel the operation
  if (mBogusNode) {
    *aCancel = true;
    return NS_OK;
  }

  nsresult res = NS_OK;

  if (IsPasswordEditor())
  {
    res = mEditor->ExtendSelectionForDelete(aSelection, &aCollapsedAction);
    NS_ENSURE_SUCCESS(res, res);

    // manage the password buffer
    PRInt32 start, end;
    nsContentUtils::GetSelectionInTextControl(aSelection, mEditor->GetRoot(),
                                              start, end);

    if (LookAndFeel::GetEchoPassword()) {
      HideLastPWInput();
      mLastStart = start;
      mLastLength = 0;
      if (mTimer)
      {
        mTimer->Cancel();
      }
    }

    if (end == start)
    { // collapsed selection
      if (nsIEditor::ePrevious==aCollapsedAction && 0<start) { // del back
        mPasswordText.Cut(start-1, 1);
      }
      else if (nsIEditor::eNext==aCollapsedAction) {      // del forward
        mPasswordText.Cut(start, 1);
      }
      // otherwise nothing to do for this collapsed selection
    }
    else {  // extended selection
      mPasswordText.Cut(start, end-start);
    }
  }
  else
  {
    nsCOMPtr<nsIDOMNode> startNode;
    PRInt32 startOffset;
    res = mEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(startNode), &startOffset);
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);
    
    bool bCollapsed;
    res = aSelection->GetIsCollapsed(&bCollapsed);
    NS_ENSURE_SUCCESS(res, res);
  
    if (!bCollapsed)
      return NS_OK;

    // Test for distance between caret and text that will be deleted
    res = CheckBidiLevelForDeletion(aSelection, startNode, startOffset, aCollapsedAction, aCancel);
    NS_ENSURE_SUCCESS(res, res);
    if (*aCancel) return NS_OK;

    res = mEditor->ExtendSelectionForDelete(aSelection, &aCollapsedAction);
    NS_ENSURE_SUCCESS(res, res);
  }

  res = mEditor->DeleteSelectionImpl(aCollapsedAction, nsIEditor::eStrip);
  NS_ENSURE_SUCCESS(res, res);

  *aHandled = true;
  ASSERT_PASSWORD_LENGTHS_EQUAL()
  return NS_OK;
}

nsresult
nsTextEditRules::DidDeleteSelection(nsISelection *aSelection, 
                                    nsIEditor::EDirection aCollapsedAction, 
                                    nsresult aResult)
{
  nsCOMPtr<nsIDOMNode> startNode;
  PRInt32 startOffset;
  nsresult res = mEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(startNode), &startOffset);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);
  
  // delete empty text nodes at selection
  if (mEditor->IsTextNode(startNode))
  {
    nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(startNode);
    PRUint32 strLength;
    res = textNode->GetLength(&strLength);
    NS_ENSURE_SUCCESS(res, res);
    
    // are we in an empty text node?
    if (!strLength)
    {
      res = mEditor->DeleteNode(startNode);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  if (!mDidExplicitlySetInterline)
  {
    // We prevent the caret from sticking on the left of prior BR
    // (i.e. the end of previous line) after this deletion.  Bug 92124
    nsCOMPtr<nsISelectionPrivate> selPriv = do_QueryInterface(aSelection);
    if (selPriv) res = selPriv->SetInterlinePosition(true);
  }
  return res;
}

nsresult
nsTextEditRules::WillUndo(nsISelection *aSelection, bool *aCancel, bool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  // initialize out param
  *aCancel = false;
  *aHandled = false;
  return NS_OK;
}

/* the idea here is to see if the magic empty node has suddenly reappeared as the result of the undo.
 * if it has, set our state so we remember it.
 * There is a tradeoff between doing here and at redo, or doing it everywhere else that might care.
 * Since undo and redo are relatively rare, it makes sense to take the (small) performance hit here.
 */
nsresult
nsTextEditRules::DidUndo(nsISelection *aSelection, nsresult aResult)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  // If aResult is an error, we return it.
  NS_ENSURE_SUCCESS(aResult, aResult);

  dom::Element* theRoot = mEditor->GetRoot();
  NS_ENSURE_TRUE(theRoot, NS_ERROR_FAILURE);
  nsIContent* node = mEditor->GetLeftmostChild(theRoot);
  if (node && mEditor->IsMozEditorBogusNode(node)) {
    mBogusNode = do_QueryInterface(node);
  } else {
    mBogusNode = nullptr;
  }
  return aResult;
}

nsresult
nsTextEditRules::WillRedo(nsISelection *aSelection, bool *aCancel, bool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  // initialize out param
  *aCancel = false;
  *aHandled = false;
  return NS_OK;
}

nsresult
nsTextEditRules::DidRedo(nsISelection *aSelection, nsresult aResult)
{
  nsresult res = aResult;  // if aResult is an error, we return it.
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  if (NS_SUCCEEDED(res)) 
  {
    nsCOMPtr<nsIDOMElement> theRoot = do_QueryInterface(mEditor->GetRoot());
    NS_ENSURE_TRUE(theRoot, NS_ERROR_FAILURE);
    
    nsCOMPtr<nsIDOMNodeList> nodeList;
    res = theRoot->GetElementsByTagName(NS_LITERAL_STRING("br"),
                                        getter_AddRefs(nodeList));
    NS_ENSURE_SUCCESS(res, res);
    if (nodeList)
    {
      PRUint32 len;
      nodeList->GetLength(&len);
      
      if (len != 1) {
        // only in the case of one br could there be the bogus node
        mBogusNode = nullptr;
        return NS_OK;  
      }

      nsCOMPtr<nsIContent> content = nodeList->GetNodeAt(0);
      MOZ_ASSERT(content);
      if (mEditor->IsMozEditorBogusNode(content)) {
        mBogusNode = do_QueryInterface(content);
      } else {
        mBogusNode = nullptr;
      }
    }
  }
  return res;
}

nsresult
nsTextEditRules::WillOutputText(nsISelection *aSelection, 
                                const nsAString  *aOutputFormat,
                                nsAString *aOutString,                                
                                bool     *aCancel,
                                bool     *aHandled)
{
  // null selection ok
  if (!aOutString || !aOutputFormat || !aCancel || !aHandled) 
    { return NS_ERROR_NULL_POINTER; }

  // initialize out param
  *aCancel = false;
  *aHandled = false;

  nsAutoString outputFormat(*aOutputFormat);
  ToLowerCase(outputFormat);
  if (outputFormat.EqualsLiteral("text/plain"))
  { // only use these rules for plain text output
    if (IsPasswordEditor())
    {
      *aOutString = mPasswordText;
      *aHandled = true;
    }
    else if (mBogusNode)
    { // this means there's no content, so output null string
      aOutString->Truncate();
      *aHandled = true;
    }
  }
  return NS_OK;
}

nsresult
nsTextEditRules::DidOutputText(nsISelection *aSelection, nsresult aResult)
{
  return NS_OK;
}

nsresult
nsTextEditRules::RemoveRedundantTrailingBR()
{
  // If the bogus node exists, we have no work to do
  if (mBogusNode)
    return NS_OK;

  // Likewise, nothing to be done if we could never have inserted a trailing br
  if (IsSingleLineEditor())
    return NS_OK;

  nsRefPtr<dom::Element> body = mEditor->GetRoot();
  if (!body)
    return NS_ERROR_NULL_POINTER;

  PRUint32 childCount = body->GetChildCount();
  if (childCount > 1) {
    // The trailing br is redundant if it is the only remaining child node
    return NS_OK;
  }

  nsRefPtr<nsIContent> child = body->GetFirstChild();
  if (!child || !child->IsElement()) {
    return NS_OK;
  }

  dom::Element* elem = child->AsElement();
  if (!nsTextEditUtils::IsMozBR(elem)) {
    return NS_OK;
  }

  // Rather than deleting this node from the DOM tree we should instead
  // morph this br into the bogus node
  elem->UnsetAttr(kNameSpaceID_None, nsGkAtoms::type, true);

  // set mBogusNode to be this <br>
  mBogusNode = do_QueryInterface(elem);

  // give it the bogus node attribute
  elem->SetAttr(kNameSpaceID_None, kMOZEditorBogusNodeAttrAtom,
                kMOZEditorBogusNodeValue, false);
  return NS_OK;
}

nsresult
nsTextEditRules::CreateTrailingBRIfNeeded()
{
  // but only if we aren't a single line edit field
  if (IsSingleLineEditor()) {
    return NS_OK;
  }

  dom::Element* body = mEditor->GetRoot();
  NS_ENSURE_TRUE(body, NS_ERROR_NULL_POINTER);

  nsIContent* lastChild = body->GetLastChild();
  // assuming CreateBogusNodeIfNeeded() has been called first
  NS_ENSURE_TRUE(lastChild, NS_ERROR_NULL_POINTER);

  if (!lastChild->IsHTML(nsGkAtoms::br)) {
    nsAutoTxnsConserveSelection dontSpazMySelection(mEditor);
    nsCOMPtr<nsIDOMNode> domBody = do_QueryInterface(body);
    return CreateMozBR(domBody, body->Length());
  }

  // Check to see if the trailing BR is a former bogus node - this will have
  // stuck around if we previously morphed a trailing node into a bogus node.
  if (!mEditor->IsMozEditorBogusNode(lastChild)) {
    return NS_OK;
  }

  // Morph it back to a mozBR
  lastChild->UnsetAttr(kNameSpaceID_None, kMOZEditorBogusNodeAttrAtom, false);
  lastChild->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                     NS_LITERAL_STRING("_moz"), true);
  return NS_OK;
}

nsresult
nsTextEditRules::CreateBogusNodeIfNeeded(nsISelection *aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(mEditor, NS_ERROR_NULL_POINTER);

  if (mBogusNode) {
    // Let's not create more than one, ok?
    return NS_OK;
  }

  // tell rules system to not do any post-processing
  nsAutoRules beginRulesSniffing(mEditor, nsEditor::kOpIgnore, nsIEditor::eNone);

  nsCOMPtr<dom::Element> body = mEditor->GetRoot();
  if (!body) {
    // We don't even have a body yet, don't insert any bogus nodes at
    // this point.
    return NS_OK;
  }

  // Now we've got the body element. Iterate over the body element's children,
  // looking for editable content. If no editable content is found, insert the
  // bogus node.
  for (nsCOMPtr<nsIContent> bodyChild = body->GetFirstChild();
       bodyChild;
       bodyChild = bodyChild->GetNextSibling()) {
    if (mEditor->IsMozEditorBogusNode(bodyChild) ||
        !mEditor->IsEditable(body) || // XXX hoist out of the loop?
        mEditor->IsEditable(bodyChild)) {
      return NS_OK;
    }
  }

  // Skip adding the bogus node if body is read-only.
  if (!mEditor->IsModifiableNode(body)) {
    return NS_OK;
  }

  // Create a br.
  nsCOMPtr<dom::Element> newContent;
  nsresult rv = mEditor->CreateHTMLContent(NS_LITERAL_STRING("br"), getter_AddRefs(newContent));
  NS_ENSURE_SUCCESS(rv, rv);

  // set mBogusNode to be the newly created <br>
  mBogusNode = do_QueryInterface(newContent);
  NS_ENSURE_TRUE(mBogusNode, NS_ERROR_NULL_POINTER);

  // Give it a special attribute.
  newContent->SetAttr(kNameSpaceID_None, kMOZEditorBogusNodeAttrAtom,
                      kMOZEditorBogusNodeValue, false);

  // Put the node in the document.
  nsCOMPtr<nsIDOMNode> bodyNode = do_QueryInterface(body);
  rv = mEditor->InsertNode(mBogusNode, bodyNode, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set selection.
  aSelection->CollapseNative(body, 0);
  return NS_OK;
}


nsresult
nsTextEditRules::TruncateInsertionIfNeeded(Selection* aSelection,
                                           const nsAString  *aInString,
                                           nsAString  *aOutString,
                                           PRInt32          aMaxLength,
                                           bool *aTruncated)
{
  if (!aSelection || !aInString || !aOutString) {return NS_ERROR_NULL_POINTER;}
  
  nsresult res = NS_OK;
  *aOutString = *aInString;
  if (aTruncated) {
    *aTruncated = false;
  }
  
  if ((-1 != aMaxLength) && IsPlaintextEditor() && !mEditor->IsIMEComposing() )
  {
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
    PRInt32 docLength;
    res = mEditor->GetTextLength(&docLength);
    if (NS_FAILED(res)) { return res; }

    PRInt32 start, end;
    nsContentUtils::GetSelectionInTextControl(aSelection, mEditor->GetRoot(),
                                              start, end);

    PRInt32 oldCompStrLength = mEditor->GetIMEBufferLength();

    const PRInt32 selectionLength = end - start;
    const PRInt32 resultingDocLength = docLength - selectionLength - oldCompStrLength;
    if (resultingDocLength >= aMaxLength)
    {
      aOutString->Truncate();
      if (aTruncated) {
        *aTruncated = true;
      }
    }
    else
    {
      PRInt32 inCount = aOutString->Length();
      if (inCount + resultingDocLength > aMaxLength)
      {
        aOutString->Truncate(aMaxLength - resultingDocLength);
        if (aTruncated) {
          *aTruncated = true;
        }
      }
    }
  }
  return res;
}

void
nsTextEditRules::ResetIMETextPWBuf()
{
  mPasswordIMEText.Truncate();
}

void
nsTextEditRules::RemoveIMETextFromPWBuf(PRInt32 &aStart, nsAString *aIMEString)
{
  MOZ_ASSERT(aIMEString);

  // initialize PasswordIME
  if (mPasswordIMEText.IsEmpty()) {
    mPasswordIMEIndex = aStart;
  }
  else {
    // manage the password buffer
    mPasswordText.Cut(mPasswordIMEIndex, mPasswordIMEText.Length());
    aStart = mPasswordIMEIndex;
  }

  mPasswordIMEText.Assign(*aIMEString);
}

NS_IMETHODIMP nsTextEditRules::Notify(class nsITimer *) {
  nsresult res = HideLastPWInput();
  ASSERT_PASSWORD_LENGTHS_EQUAL();
  mLastLength = 0;
  return res;
}

nsresult nsTextEditRules::HideLastPWInput() {
  if (!mLastLength) {
    // Special case, we're trying to replace a range that no longer exists
    return NS_OK;
  }

  nsAutoString hiddenText;
  FillBufWithPWChars(&hiddenText, mLastLength);

  nsRefPtr<Selection> selection = mEditor->GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  PRInt32 start, end;
  nsContentUtils::GetSelectionInTextControl(selection, mEditor->GetRoot(),
                                            start, end);

  nsCOMPtr<nsIDOMNode> selNode = GetTextNode(selection, mEditor);
  NS_ENSURE_TRUE(selNode, NS_OK);
  
  nsCOMPtr<nsIDOMCharacterData> nodeAsText(do_QueryInterface(selNode));
  NS_ENSURE_TRUE(nodeAsText, NS_OK);
  
  nodeAsText->ReplaceData(mLastStart, mLastLength, hiddenText);
  selection->Collapse(selNode, start);
  if (start != end)
    selection->Extend(selNode, end);
  return NS_OK;
}

// static
void
nsTextEditRules::FillBufWithPWChars(nsAString *aOutString, PRInt32 aLength)
{
  MOZ_ASSERT(aOutString);

  // change the output to the platform password character
  PRUnichar passwordChar = LookAndFeel::GetPasswordCharacter();

  PRInt32 i;
  aOutString->Truncate();
  for (i=0; i < aLength; i++)
    aOutString->Append(passwordChar);
}


///////////////////////////////////////////////////////////////////////////
// CreateMozBR: put a BR node with moz attribute at {aNode, aOffset}
//                       
nsresult 
nsTextEditRules::CreateMozBR(nsIDOMNode* inParent, PRInt32 inOffset,
                             nsIDOMNode** outBRNode)
{
  NS_ENSURE_TRUE(inParent, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMNode> brNode;
  nsresult res = mEditor->CreateBR(inParent, inOffset, address_of(brNode));
  NS_ENSURE_SUCCESS(res, res);

  // give it special moz attr
  nsCOMPtr<nsIDOMElement> brElem = do_QueryInterface(brNode);
  if (brElem) {
    res = mEditor->SetAttribute(brElem, NS_LITERAL_STRING("type"), NS_LITERAL_STRING("_moz"));
    NS_ENSURE_SUCCESS(res, res);
  }

  if (outBRNode) {
    brNode.forget(outBRNode);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTextEditRules::DocumentModified()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
