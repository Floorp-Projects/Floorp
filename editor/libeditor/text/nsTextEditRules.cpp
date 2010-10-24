/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsTextEditRules.h"

#include "nsEditor.h"
#include "nsTextEditUtils.h"
#include "nsCRT.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsIDOMNodeList.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsISelectionController.h"
#include "nsIDOMRange.h"
#include "nsIDOMNSRange.h"
#include "nsIDOMCharacterData.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsEditorUtils.h"
#include "EditTxn.h"
#include "nsEditProperty.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsUnicharUtils.h"
#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "DeleteTextTxn.h"
#include "nsNodeIterator.h"
#include "nsIDOMNodeFilter.h"

// for IBMBIDI
#include "nsFrameSelection.h"

static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

#define CANCEL_OPERATION_IF_READONLY_OR_DISABLED \
  if (IsReadonly() || IsDisabled()) \
  {                     \
    *aCancel = PR_TRUE; \
    return NS_OK;       \
  };


nsresult
NS_NewTextEditRules(nsIEditRules** aInstancePtrResult)
{
  nsTextEditRules * rules = new nsTextEditRules();
  if (rules)
    return rules->QueryInterface(NS_GET_IID(nsIEditRules), (void**) aInstancePtrResult);
  return NS_ERROR_OUT_OF_MEMORY;
}


/********************************************************
 *  Constructor/Destructor 
 ********************************************************/

nsTextEditRules::nsTextEditRules()
: mEditor(nsnull)
, mPasswordText()
, mPasswordIMEText()
, mPasswordIMEIndex(0)
, mActionNesting(0)
, mLockRulesSniffing(PR_FALSE)
, mDidExplicitlySetInterline(PR_FALSE)
, mTheAction(0)
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

  // Cache our body node, if available.
  nsIDOMNode *body = mEditor->GetRoot();

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

  if (body)
  {
    // create a range that is the entire body contents
    nsCOMPtr<nsIDOMRange> wholeDoc =
      do_CreateInstance("@mozilla.org/content/range;1");
    NS_ENSURE_TRUE(wholeDoc, NS_ERROR_NULL_POINTER);
    wholeDoc->SetStart(body,0);
    nsCOMPtr<nsIDOMNodeList> list;
    res = body->GetChildNodes(getter_AddRefs(list));
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(list, NS_ERROR_FAILURE);

    PRUint32 listCount;
    res = list->GetLength(&listCount);
    NS_ENSURE_SUCCESS(res, res);

    res = wholeDoc->SetEnd(body, listCount);
    NS_ENSURE_SUCCESS(res, res);

    // replace newlines in that range with breaks
    res = ReplaceNewlines(wholeDoc);
  }

  PRBool deleteBidiImmediately = PR_FALSE;
  nsCOMPtr<nsIPrefBranch> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &res);
  if (NS_SUCCEEDED(res))
    prefBranch->GetBoolPref("bidi.edit.delete_immediately",
                            &deleteBidiImmediately);
  mDeleteBidiImmediately = deleteBidiImmediately;

  return res;
}

NS_IMETHODIMP
nsTextEditRules::DetachEditor()
{
  if (mTimer)
    mTimer->Cancel();

  mEditor = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsTextEditRules::BeforeEdit(PRInt32 action, nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) return NS_OK;
  
  nsAutoLockRulesSniffing lockIt(this);
  mDidExplicitlySetInterline = PR_FALSE;
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
nsTextEditRules::AfterEdit(PRInt32 action, nsIEditor::EDirection aDirection)
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
                                          nsnull, 0, nsnull, 0);
    NS_ENSURE_SUCCESS(res, res);

    // detect empty doc
    res = CreateBogusNodeIfNeeded(selection);
    NS_ENSURE_SUCCESS(res, res);
    
    // insure trailing br node
    res = CreateTrailingBRIfNeeded();
    NS_ENSURE_SUCCESS(res, res);

    // collapse the selection to the trailing BR if it's at the end of our text node
    CollapseSelectionToTrailingBRIfNeeded(selection);
    
    /* After inserting text the cursor Bidi level must be set to the level of the inserted text.
     * This is difficult, because we cannot know what the level is until after the Bidi algorithm
     * is applied to the whole paragraph.
     *
     * So we set the cursor Bidi level to UNDEFINED here, and the caret code will set it correctly later
     */
    if (action == nsEditor::kOpInsertText
        || action == nsEditor::kOpInsertIMEText) {
      nsCOMPtr<nsISelectionPrivate> privateSelection(do_QueryInterface(selection));
      nsCOMPtr<nsFrameSelection> frameSelection;
      privateSelection->GetFrameSelection(getter_AddRefs(frameSelection));      
      if (frameSelection) {
        frameSelection->UndefineCaretBidiLevel();
      }
    }
  }
  return res;
}


NS_IMETHODIMP 
nsTextEditRules::WillDoAction(nsISelection *aSelection, 
                              nsRulesInfo *aInfo, 
                              PRBool *aCancel, 
                              PRBool *aHandled)
{
  // null selection is legal
  if (!aInfo || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
#if defined(DEBUG_ftang)
  printf("nsTextEditRules::WillDoAction action= %d", aInfo->action);
#endif

  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;

  // my kingdom for dynamic cast
  nsTextRulesInfo *info = static_cast<nsTextRulesInfo*>(aInfo);
    
  switch (info->action)
  {
    case kInsertBreak:
      return WillInsertBreak(aSelection, aCancel, aHandled, info->maxLength);
    case kInsertText:
    case kInsertTextIME:
      return WillInsertText(info->action,
                            aSelection, 
                            aCancel,
                            aHandled, 
                            info->inString,
                            info->outString,
                            info->maxLength);
    case kDeleteSelection:
      return WillDeleteSelection(aSelection, info->collapsedAction, aCancel, aHandled);
    case kUndo:
      return WillUndo(aSelection, aCancel, aHandled);
    case kRedo:
      return WillRedo(aSelection, aCancel, aHandled);
    case kSetTextProperty:
      return WillSetTextProperty(aSelection, aCancel, aHandled);
    case kRemoveTextProperty:
      return WillRemoveTextProperty(aSelection, aCancel, aHandled);
    case kOutputText:
      return WillOutputText(aSelection, 
                            info->outputFormat,
                            info->outString,                            
                            aCancel,
                            aHandled);
    case kInsertElement:  // i had thought this would be html rules only.  but we put pre elements
                          // into plaintext mail when doing quoting for reply!  doh!
      return WillInsert(aSelection, aCancel);
  }
  return NS_ERROR_FAILURE;
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
   case kInsertBreak:
     return DidInsertBreak(aSelection, aResult);
    case kInsertText:
    case kInsertTextIME:
      return DidInsertText(aSelection, aResult);
    case kDeleteSelection:
      return DidDeleteSelection(aSelection, info->collapsedAction, aResult);
    case kUndo:
      return DidUndo(aSelection, aResult);
    case kRedo:
      return DidRedo(aSelection, aResult);
    case kSetTextProperty:
      return DidSetTextProperty(aSelection, aResult);
    case kRemoveTextProperty:
      return DidRemoveTextProperty(aSelection, aResult);
    case kOutputText:
      return DidOutputText(aSelection, aResult);
  }
  // Don't fail on transactions we don't handle here!
  return NS_OK;
}


NS_IMETHODIMP
nsTextEditRules::DocumentIsEmpty(PRBool *aDocumentIsEmpty)
{
  NS_ENSURE_TRUE(aDocumentIsEmpty, NS_ERROR_NULL_POINTER);
  
  *aDocumentIsEmpty = (mBogusNode != nsnull);
  return NS_OK;
}

/********************************************************
 *  Protected methods 
 ********************************************************/


nsresult
nsTextEditRules::WillInsert(nsISelection *aSelection, PRBool *aCancel)
{
  NS_ENSURE_TRUE(aSelection && aCancel, NS_ERROR_NULL_POINTER);
  
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED

  // initialize out param
  *aCancel = PR_FALSE;
  
  // check for the magic content node and delete it if it exists
  if (mBogusNode)
  {
    mEditor->DeleteNode(mBogusNode);
    mBogusNode = nsnull;
  }

  return NS_OK;
}

nsresult
nsTextEditRules::DidInsert(nsISelection *aSelection, nsresult aResult)
{
  return NS_OK;
}

nsresult
nsTextEditRules::WillInsertBreak(nsISelection *aSelection,
                                 PRBool *aCancel,
                                 PRBool *aHandled,
                                 PRInt32 aMaxLength)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  *aHandled = PR_FALSE;
  if (IsSingleLineEditor()) {
    *aCancel = PR_TRUE;
  }
  else 
  {
    // handle docs with a max length
    // NOTE, this function copies inString into outString for us.
    NS_NAMED_LITERAL_STRING(inString, "\n");
    nsAutoString outString;
    PRBool didTruncate;
    nsresult res = TruncateInsertionIfNeeded(aSelection, &inString, &outString,
                                             aMaxLength, &didTruncate);
    NS_ENSURE_SUCCESS(res, res);
    if (didTruncate) {
      *aCancel = PR_TRUE;
      return NS_OK;
    }

    *aCancel = PR_FALSE;

    // if the selection isn't collapsed, delete it.
    PRBool bCollapsed;
    res = aSelection->GetIsCollapsed(&bCollapsed);
    NS_ENSURE_SUCCESS(res, res);
    if (!bCollapsed)
    {
      res = mEditor->DeleteSelection(nsIEditor::eNone);
      NS_ENSURE_SUCCESS(res, res);
    }

    res = WillInsert(aSelection, aCancel);
    NS_ENSURE_SUCCESS(res, res);
    // initialize out param
    // we want to ignore result of WillInsert()
    *aCancel = PR_FALSE;
  
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
  if (selOffset != length) return NS_OK;

  nsCOMPtr<nsIDOMNode> parentNode;
  PRInt32 parentOffset;
  res = nsEditor::GetNodeLocation(selNode, address_of(parentNode),
                                  &parentOffset);
  NS_ENSURE_SUCCESS(res, res);

  nsIDOMElement *rootElem = mEditor->GetRoot();
  nsCOMPtr<nsIDOMNode> root = do_QueryInterface(rootElem);
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
  NS_ENSURE_SUCCESS(res, nsnull);
  if (!editor->IsTextNode(selNode)) {
    // Get an nsINode from the nsIDOMNode
    nsCOMPtr<nsINode> node = do_QueryInterface(selNode);
    // if node is null, return it to indicate there's no text
    NS_ENSURE_TRUE(node, nsnull);
    // This should be the root node, walk the tree looking for text nodes
    nsNodeIterator iter(node, nsIDOMNodeFilter::SHOW_TEXT, nsnull, PR_TRUE);
    while (!editor->IsTextNode(selNode)) {
      if (NS_FAILED(res = iter.NextNode(getter_AddRefs(selNode))) || !selNode) {
        return nsnull;
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
    aString.Trim(CRLF, PR_FALSE, PR_TRUE);
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
    aString.Trim(CRLF, PR_TRUE, PR_TRUE);
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
    aString.Trim(CRLF, PR_TRUE, PR_TRUE);
    break;
  }
}

nsresult
nsTextEditRules::WillInsertText(PRInt32          aAction,
                                nsISelection *aSelection, 
                                PRBool          *aCancel,
                                PRBool          *aHandled,
                                const nsAString *inString,
                                nsAString *outString,
                                PRInt32          aMaxLength)
{  
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }

  if (inString->IsEmpty() && (aAction != kInsertTextIME))
  {
    // HACK: this is a fix for bug 19395
    // I can't outlaw all empty insertions
    // because IME transaction depend on them
    // There is more work to do to make the 
    // world safe for IME.
    *aCancel = PR_TRUE;
    *aHandled = PR_FALSE;
    return NS_OK;
  }
  
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_TRUE;

  // handle docs with a max length
  // NOTE, this function copies inString into outString for us.
  PRBool truncated = PR_FALSE;
  nsresult res = TruncateInsertionIfNeeded(aSelection, inString, outString,
                                           aMaxLength, &truncated);
  NS_ENSURE_SUCCESS(res, res);
  if (truncated && outString->IsEmpty()) {
    *aCancel = PR_TRUE;
    return NS_OK;
  }
  
  PRUint32 start = 0;
  PRUint32 end = 0;  

  // handle password field docs
  if (IsPasswordEditor())
  {
    res = mEditor->GetTextSelectionOffsets(aSelection, start, end);
    NS_ASSERTION((NS_SUCCEEDED(res)), "getTextSelectionOffsets failed!");
    NS_ENSURE_SUCCESS(res, res);
  }

  // if the selection isn't collapsed, delete it.
  PRBool bCollapsed;
  res = aSelection->GetIsCollapsed(&bCollapsed);
  NS_ENSURE_SUCCESS(res, res);
  if (!bCollapsed)
  {
    res = mEditor->DeleteSelection(nsIEditor::eNone);
    NS_ENSURE_SUCCESS(res, res);
  }

  res = WillInsert(aSelection, aCancel);
  NS_ENSURE_SUCCESS(res, res);
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  
  // handle password field data
  // this has the side effect of changing all the characters in aOutString
  // to the replacement character
  if (IsPasswordEditor())
  {
    if (aAction == kInsertTextIME)  {
      res = RemoveIMETextFromPWBuf(start, outString);
      NS_ENSURE_SUCCESS(res, res);
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

    nsCOMPtr<nsILookAndFeel> lookAndFeel = do_GetService(kLookAndFeelCID);
    if (lookAndFeel->GetEchoPassword() && !DontEchoPassword()) {
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
      mTimer->InitWithCallback(this, 600, nsITimer::TYPE_ONE_SHOT);
    } 
    else 
    {
      res = FillBufWithPWChars(outString, outString->Length());
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;
  res = mEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);

  // don't put text in places that can't have it
  if (!mEditor->IsTextNode(selNode) && !mEditor->CanContainTag(selNode, NS_LITERAL_STRING("#text")))
    return NS_ERROR_FAILURE;

  // we need to get the doc
  nsCOMPtr<nsIDOMDocument>doc;
  res = mEditor->GetDocument(getter_AddRefs(doc));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(doc, NS_ERROR_NULL_POINTER);
    
  if (aAction == kInsertTextIME) 
  { 
    res = mEditor->InsertTextImpl(*outString, address_of(selNode), &selOffset, doc);
    NS_ENSURE_SUCCESS(res, res);
  }
  else // aAction == kInsertText
  {
    // find where we are
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
      PRBool endsWithLF =
        !outString->IsEmpty() && outString->Last() == nsCRT::LF;
      nsCOMPtr<nsISelectionPrivate>selPrivate(do_QueryInterface(aSelection));
      selPrivate->SetInterlinePosition(endsWithLF);

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
nsTextEditRules::WillSetTextProperty(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) 
    { return NS_ERROR_NULL_POINTER; }

  // XXX: should probably return a success value other than NS_OK that means "not allowed"
  if (IsPlaintextEditor()) {
    *aCancel = PR_TRUE;
  }
  return NS_OK;
}

nsresult
nsTextEditRules::DidSetTextProperty(nsISelection *aSelection, nsresult aResult)
{
  return NS_OK;
}

nsresult
nsTextEditRules::WillRemoveTextProperty(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) 
    { return NS_ERROR_NULL_POINTER; }

  // XXX: should probably return a success value other than NS_OK that means "not allowed"
  if (IsPlaintextEditor()) {
    *aCancel = PR_TRUE;
  }
  return NS_OK;
}

nsresult
nsTextEditRules::DidRemoveTextProperty(nsISelection *aSelection, nsresult aResult)
{
  return NS_OK;
}

nsresult
nsTextEditRules::WillDeleteSelection(nsISelection *aSelection, 
                                     nsIEditor::EDirection aCollapsedAction, 
                                     PRBool *aCancel,
                                     PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED

  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  
  // if there is only bogus content, cancel the operation
  if (mBogusNode) {
    *aCancel = PR_TRUE;
    return NS_OK;
  }

  nsresult res = NS_OK;

  if (IsPasswordEditor())
  {
    res = mEditor->ExtendSelectionForDelete(aSelection, &aCollapsedAction);
    NS_ENSURE_SUCCESS(res, res);

    // manage the password buffer
    PRUint32 start, end;
    mEditor->GetTextSelectionOffsets(aSelection, start, end);
    NS_ENSURE_SUCCESS(res, res);
    nsCOMPtr<nsILookAndFeel> lookAndFeel = do_GetService(kLookAndFeelCID);

    if (lookAndFeel->GetEchoPassword()) {
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
    
    PRBool bCollapsed;
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

  res = mEditor->DeleteSelectionImpl(aCollapsedAction);
  NS_ENSURE_SUCCESS(res, res);

  *aHandled = PR_TRUE;
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
    if (selPriv) res = selPriv->SetInterlinePosition(PR_TRUE);
  }
  return res;
}

nsresult
nsTextEditRules::WillUndo(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  return NS_OK;
}

/* the idea here is to see if the magic empty node has suddenly reappeared as the result of the undo.
 * if it has, set our state so we remember it.
 * There is a tradeoff between doing here and at redo, or doing it everywhere else that might care.
 * Since undo and redo are relatively rare, it makes sense to take the (small) performance hit here.
 */
nsresult
nsTextEditRules:: DidUndo(nsISelection *aSelection, nsresult aResult)
{
  nsresult res = aResult;  // if aResult is an error, we return it.
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  if (NS_SUCCEEDED(res)) 
  {
    if (mBogusNode) {
      mBogusNode = nsnull;
    }
    else
    {
      nsIDOMElement *theRoot = mEditor->GetRoot();
      NS_ENSURE_TRUE(theRoot, NS_ERROR_FAILURE);
      nsCOMPtr<nsIDOMNode> node = mEditor->GetLeftmostChild(theRoot);
      if (node && mEditor->IsMozEditorBogusNode(node))
        mBogusNode = node;
    }
  }
  return res;
}

nsresult
nsTextEditRules::WillRedo(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  return NS_OK;
}

nsresult
nsTextEditRules::DidRedo(nsISelection *aSelection, nsresult aResult)
{
  nsresult res = aResult;  // if aResult is an error, we return it.
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  if (NS_SUCCEEDED(res)) 
  {
    if (mBogusNode) {
      mBogusNode = nsnull;
    }
    else
    {
      nsIDOMElement *theRoot = mEditor->GetRoot();
      NS_ENSURE_TRUE(theRoot, NS_ERROR_FAILURE);
      
      nsCOMPtr<nsIDOMNodeList> nodeList;
      res = theRoot->GetElementsByTagName(NS_LITERAL_STRING("br"),
                                          getter_AddRefs(nodeList));
      NS_ENSURE_SUCCESS(res, res);
      if (nodeList)
      {
        PRUint32 len;
        nodeList->GetLength(&len);
        
        if (len != 1) return NS_OK;  // only in the case of one br could there be the bogus node
        nsCOMPtr<nsIDOMNode> node;
        nodeList->Item(0, getter_AddRefs(node));
        NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);
        if (mEditor->IsMozEditorBogusNode(node))
          mBogusNode = node;
      }
    }
  }
  return res;
}

nsresult
nsTextEditRules::WillOutputText(nsISelection *aSelection, 
                                const nsAString  *aOutputFormat,
                                nsAString *aOutString,                                
                                PRBool   *aCancel,
                                PRBool   *aHandled)
{
  // null selection ok
  if (!aOutString || !aOutputFormat || !aCancel || !aHandled) 
    { return NS_ERROR_NULL_POINTER; }

  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;

  nsAutoString outputFormat(*aOutputFormat);
  ToLowerCase(outputFormat);
  if (outputFormat.EqualsLiteral("text/plain"))
  { // only use these rules for plain text output
    if (IsPasswordEditor())
    {
      *aOutString = mPasswordText;
      *aHandled = PR_TRUE;
    }
    else if (mBogusNode)
    { // this means there's no content, so output null string
      aOutString->Truncate();
      *aHandled = PR_TRUE;
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
nsTextEditRules::ReplaceNewlines(nsIDOMRange *aRange)
{
  NS_ENSURE_TRUE(aRange, NS_ERROR_NULL_POINTER);
  
  // convert any newlines in editable, preformatted text nodes 
  // into normal breaks.  this is because layout won't give us a place 
  // to put the cursor on empty lines otherwise.

  nsresult res;
  nsCOMPtr<nsIContentIterator> iter =
       do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &res);
  NS_ENSURE_SUCCESS(res, res);

  res = iter->Init(aRange);
  NS_ENSURE_SUCCESS(res, res);
  
  nsCOMArray<nsIDOMCharacterData> arrayOfNodes;
  
  // gather up a list of editable preformatted text nodes
  while (!iter->IsDone())
  {
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(iter->GetCurrentNode());
    NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

    if (mEditor->IsTextNode(node) && mEditor->IsEditable(node))
    {
      PRBool isPRE;
      res = mEditor->IsPreformatted(node, &isPRE);
      NS_ENSURE_SUCCESS(res, res);
      if (isPRE)
      {
        nsCOMPtr<nsIDOMCharacterData> data = do_QueryInterface(node);
        arrayOfNodes.AppendObject(data);
      }
    }
    iter->Next();
  }
  
  // replace newlines with breaks.  have to do this left to right,
  // since inserting the break can split the text node, and the
  // original node becomes the righthand node.
  PRInt32 j, nodeCount = arrayOfNodes.Count();
  for (j = 0; j < nodeCount; j++)
  {
    nsCOMPtr<nsIDOMNode> brNode;
    nsCOMPtr<nsIDOMCharacterData> textNode = arrayOfNodes[0];
    arrayOfNodes.RemoveObjectAt(0);
    // find the newline
    PRInt32 offset;
    nsAutoString tempString;
    do 
    {
      textNode->GetData(tempString);
      offset = tempString.FindChar(nsCRT::LF);
      if (offset == -1) break; // done with this node
      
      // delete the newline
      nsRefPtr<DeleteTextTxn> txn;
      // note 1: we are not telling edit listeners about these because they don't care
      // note 2: we are not wrapping these in a placeholder because we know they already are,
      //         or, failing that, undo is disabled
      res = mEditor->CreateTxnForDeleteText(textNode, offset, 1,
                                            getter_AddRefs(txn));
      NS_ENSURE_SUCCESS(res, res); 
      NS_ENSURE_TRUE(txn, NS_ERROR_OUT_OF_MEMORY);
      res = mEditor->DoTransaction(txn); 
      NS_ENSURE_SUCCESS(res, res); 
      
      // insert a break
      res = mEditor->CreateBR(textNode, offset, address_of(brNode));
      NS_ENSURE_SUCCESS(res, res);
    } while (1);  // break used to exit while loop
  }
  return res;
}

nsresult
nsTextEditRules::CreateTrailingBRIfNeeded()
{
  // but only if we aren't a single line edit field
  if (IsSingleLineEditor())
    return NS_OK;
  nsIDOMNode *body = mEditor->GetRoot();
  NS_ENSURE_TRUE(body, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode> lastChild;
  nsresult res = body->GetLastChild(getter_AddRefs(lastChild));
  // assuming CreateBogusNodeIfNeeded() has been called first
  NS_ENSURE_SUCCESS(res, res);  
  NS_ENSURE_TRUE(lastChild, NS_ERROR_NULL_POINTER);

  if (!nsTextEditUtils::IsBreak(lastChild))
  {
    nsAutoTxnsConserveSelection dontSpazMySelection(mEditor);
    PRUint32 rootLen;
    res = mEditor->GetLengthOfDOMNode(body, rootLen);
    NS_ENSURE_SUCCESS(res, res); 
    nsCOMPtr<nsIDOMNode> unused;
    res = CreateMozBR(body, rootLen, address_of(unused));
  }
  return res;
}

nsresult
nsTextEditRules::CreateBogusNodeIfNeeded(nsISelection *aSelection)
{
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  if (!mEditor) { return NS_ERROR_NULL_POINTER; }
  if (mBogusNode) return NS_OK;  // let's not create more than one, ok?

  // tell rules system to not do any post-processing
  nsAutoRules beginRulesSniffing(mEditor, nsEditor::kOpIgnore, nsIEditor::eNone);

  nsIDOMNode* body = mEditor->GetRoot();
  if (!body)
  {
    // we don't even have a body yet, don't insert any bogus nodes at
    // this point.

    return NS_OK;
  }

  // now we've got the body tag.
  // iterate the body tag, looking for editable content
  // if no editable content is found, insert the bogus node
  PRBool needsBogusContent=PR_TRUE;
  nsCOMPtr<nsIDOMNode> bodyChild;
  nsresult res = body->GetFirstChild(getter_AddRefs(bodyChild));        
  while ((NS_SUCCEEDED(res)) && bodyChild)
  { 
    if (mEditor->IsMozEditorBogusNode(bodyChild) ||
        !mEditor->IsEditable(body) ||
        mEditor->IsEditable(bodyChild))
    {
      needsBogusContent = PR_FALSE;
      break;
    }
    nsCOMPtr<nsIDOMNode>temp;
    bodyChild->GetNextSibling(getter_AddRefs(temp));
    bodyChild = do_QueryInterface(temp);
  }
  if (needsBogusContent)
  {
    // create a br
    nsCOMPtr<nsIContent> newContent;
    res = mEditor->CreateHTMLContent(NS_LITERAL_STRING("br"), getter_AddRefs(newContent));
    NS_ENSURE_SUCCESS(res, res);
    nsCOMPtr<nsIDOMElement>brElement = do_QueryInterface(newContent);

    // set mBogusNode to be the newly created <br>
    mBogusNode = brElement;
    NS_ENSURE_TRUE(mBogusNode, NS_ERROR_NULL_POINTER);

    // give it a special attribute
    newContent->SetAttr(kNameSpaceID_None, kMOZEditorBogusNodeAttrAtom,
                        kMOZEditorBogusNodeValue, PR_FALSE);
    
    // put the node in the document
    res = mEditor->InsertNode(mBogusNode, body, 0);
    NS_ENSURE_SUCCESS(res, res);

    // set selection
    aSelection->Collapse(body, 0);
  }
  return res;
}


nsresult
nsTextEditRules::TruncateInsertionIfNeeded(nsISelection *aSelection, 
                                           const nsAString  *aInString,
                                           nsAString  *aOutString,
                                           PRInt32          aMaxLength,
                                           PRBool *aTruncated)
{
  if (!aSelection || !aInString || !aOutString) {return NS_ERROR_NULL_POINTER;}
  
  nsresult res = NS_OK;
  *aOutString = *aInString;
  if (aTruncated) {
    *aTruncated = PR_FALSE;
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

    PRUint32 start, end;
    res = mEditor->GetTextSelectionOffsets(aSelection, start, end);
    if (NS_FAILED(res)) { return res; }

    PRInt32 oldCompStrLength;
    res = mEditor->GetIMEBufferLength(&oldCompStrLength);
    if (NS_FAILED(res)) { return res; }

    const PRInt32 selectionLength = end - start;
    const PRInt32 resultingDocLength = docLength - selectionLength - oldCompStrLength;
    if (resultingDocLength >= aMaxLength)
    {
      aOutString->Truncate();
      if (aTruncated) {
        *aTruncated = PR_TRUE;
      }
    }
    else
    {
      PRInt32 inCount = aOutString->Length();
      if (inCount + resultingDocLength > aMaxLength)
      {
        aOutString->Truncate(aMaxLength - resultingDocLength);
        if (aTruncated) {
          *aTruncated = PR_TRUE;
        }
      }
    }
  }
  return res;
}

nsresult
nsTextEditRules::ResetIMETextPWBuf()
{
  mPasswordIMEText.Truncate();
  return NS_OK;
}

nsresult
nsTextEditRules::RemoveIMETextFromPWBuf(PRUint32 &aStart, nsAString *aIMEString)
{
  if (!aIMEString) {
    return NS_ERROR_NULL_POINTER;
  }

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
  return NS_OK;
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

  nsCOMPtr<nsISelection> selection;
  PRUint32 start, end;
  nsresult res = mEditor->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  res = mEditor->GetTextSelectionOffsets(selection, start, end);
  NS_ENSURE_SUCCESS(res, res);

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
nsresult
nsTextEditRules::FillBufWithPWChars(nsAString *aOutString, PRInt32 aLength)
{
  if (!aOutString) {return NS_ERROR_NULL_POINTER;}

  // change the output to the platform password character
  PRUnichar passwordChar = PRUnichar('*');
  nsCOMPtr<nsILookAndFeel> lookAndFeel = do_GetService(kLookAndFeelCID);
  if (lookAndFeel)
  {
    passwordChar = lookAndFeel->GetPasswordCharacter();
  }

  PRInt32 i;
  aOutString->Truncate();
  for (i=0; i < aLength; i++)
    aOutString->Append(passwordChar);

  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// CreateMozBR: put a BR node with moz attribute at {aNode, aOffset}
//                       
nsresult 
nsTextEditRules::CreateMozBR(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outBRNode)
{
  NS_ENSURE_TRUE(inParent && outBRNode, NS_ERROR_NULL_POINTER);

  nsresult res = mEditor->CreateBR(inParent, inOffset, outBRNode);
  NS_ENSURE_SUCCESS(res, res);

  // give it special moz attr
  nsCOMPtr<nsIDOMElement> brElem = do_QueryInterface(*outBRNode);
  if (brElem)
  {
    res = mEditor->SetAttribute(brElem, NS_LITERAL_STRING("type"), NS_LITERAL_STRING("_moz"));
    NS_ENSURE_SUCCESS(res, res);
  }
  return res;
}
