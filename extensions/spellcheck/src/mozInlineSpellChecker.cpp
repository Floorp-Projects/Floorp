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
 * The Original Code is Real-time Spellchecking
 *
 * The Initial Developer of the Original Code is Mozdev Group, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Neil Deakin (neil@mozdevgroup.com)
 *                 Scott MacGregor (mscott@mozilla.org)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// TODO FIXME: We should remember whether we checked the last text entered, so that
// we don't have to re-check every single word that the caret moves over.

#include "nsCOMPtr.h"

#include "nsString.h"
#include "nsIMutableArray.h"
#include "nsArrayUtils.h"
#include "nsIServiceManager.h"
#include "nsIEnumerator.h"
#include "nsUnicharUtils.h"
#include "nsReadableUtils.h"

#include "mozISpellI18NManager.h"
#include "mozInlineSpellChecker.h"

#include "nsIDOMKeyEvent.h"
#include "nsIPlaintextEditor.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentRange.h"
#include "nsIDOMNode.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsIDOMNodeList.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsITextServicesDocument.h"
#include "nsITextServicesFilter.h"
#include "nsIDOMRange.h"
#include "nsIDOMNSRange.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMDocumentTraversal.h"
#include "nsIDOMNodeFilter.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsCRT.h"
#include "cattable.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "mozInlineSpellWordUtil.h"

//#define DEBUG_INLINESPELL

#include "nsIDocument.h"

static const char kMaxSpellCheckSelectionSize[] = "extensions.spellcheck.inline.max-misspellings";

NS_INTERFACE_MAP_BEGIN(mozInlineSpellChecker)
NS_INTERFACE_MAP_ENTRY(nsIInlineSpellChecker)
NS_INTERFACE_MAP_ENTRY(nsIEditActionListener)
NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMKeyListener)
NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMKeyListener)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(mozInlineSpellChecker)
NS_IMPL_RELEASE(mozInlineSpellChecker)

mozInlineSpellChecker::SpellCheckingState
  mozInlineSpellChecker::gCanEnableSpellChecking =
  mozInlineSpellChecker::SpellCheck_Uninitialized;

mozInlineSpellChecker::mozInlineSpellChecker():mNumWordsInSpellSelection(0), mMaxNumWordsInSpellSelection(250)
{
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs)
    prefs->GetIntPref(kMaxSpellCheckSelectionSize, &mMaxNumWordsInSpellSelection); 
}

mozInlineSpellChecker::~mozInlineSpellChecker()
{
}

NS_IMETHODIMP
mozInlineSpellChecker::GetSpellChecker(nsIEditorSpellCheck **aSpellCheck)
{
  *aSpellCheck = mSpellCheck;
  NS_IF_ADDREF(*aSpellCheck);
  return NS_OK;
}

NS_IMETHODIMP
mozInlineSpellChecker::Init(nsIEditor *aEditor)
{
  mEditor = do_GetWeakReference(aEditor);
  return NS_OK;
}

nsresult mozInlineSpellChecker::Cleanup()
{
  return UnregisterEventListeners();
}

// mozInlineSpellChecker::CanEnableInlineSpellChecking
//
//    This function can be called to see if it seems likely that we can enable
//    spellchecking before actually creating the InlineSpellChecking objects.
//
//    The problem is that we can't get the dictionary list without actually
//    creating a whole bunch of spellchecking objects. This function tries to
//    do that and caches the result so we don't have to keep allocating those
//    objects if there are no dictionaries or spellchecking.
//
//    This caching will prevent adding dictionaries at runtime if we start out
//    with no dictionaries! Installing dictionaries as extensions will require
//    a restart anyway, so it shouldn't be a problem.

PRBool // static
mozInlineSpellChecker::CanEnableInlineSpellChecking()
{
  nsresult rv;
  if (gCanEnableSpellChecking == SpellCheck_Uninitialized) {
    gCanEnableSpellChecking = SpellCheck_NotAvailable;

    nsCOMPtr<nsIEditorSpellCheck> spellchecker =
      do_CreateInstance("@mozilla.org/editor/editorspellchecker;1", &rv);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    PRBool canSpellCheck = PR_FALSE;
    rv = spellchecker->CanSpellCheck(&canSpellCheck);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    if (canSpellCheck)
      gCanEnableSpellChecking = SpellCheck_Available;
  }
  return (gCanEnableSpellChecking == SpellCheck_Available);
}

// mozInlineSpellChecker::RegisterEventListeners
//
//    The inline spell checker listens to mouse events and keyboard navigation+ //    events.

nsresult
mozInlineSpellChecker::RegisterEventListeners()
{
  nsCOMPtr<nsIEditor> editor (do_QueryReferent(mEditor));
  NS_ENSURE_TRUE(editor, NS_ERROR_NULL_POINTER);

  editor->AddEditActionListener(this);

  nsCOMPtr<nsIDOMDocument> doc;
  nsresult rv = editor->GetDocument(getter_AddRefs(doc));
  NS_ENSURE_SUCCESS(rv, rv); 

  nsCOMPtr<nsIDOMEventReceiver> eventReceiver = do_QueryInterface(doc, &rv);
  NS_ENSURE_SUCCESS(rv, rv); 

  eventReceiver->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseListener*, this), NS_GET_IID(nsIDOMMouseListener));
  eventReceiver->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMKeyListener*, this), NS_GET_IID(nsIDOMKeyListener));

  return NS_OK;
}

// mozInlineSpellChecker::UnregisterEventListeners

nsresult
mozInlineSpellChecker::UnregisterEventListeners()
{
  nsCOMPtr<nsIEditor> editor (do_QueryReferent(mEditor));
  NS_ENSURE_TRUE(editor, NS_ERROR_NULL_POINTER);

  editor->RemoveEditActionListener(this);

  nsCOMPtr<nsIDOMDocument> doc;
  editor->GetDocument(getter_AddRefs(doc));
  NS_ENSURE_TRUE(doc, NS_ERROR_NULL_POINTER);
  
  nsCOMPtr<nsIDOMEventReceiver> eventReceiver = do_QueryInterface(doc);
  NS_ENSURE_TRUE(eventReceiver, NS_ERROR_NULL_POINTER);

  eventReceiver->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMMouseListener*, this), NS_GET_IID(nsIDOMMouseListener));
  eventReceiver->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMKeyListener*, this), NS_GET_IID(nsIDOMKeyListener));
  
  return NS_OK;
}

// mozInlineSpellChecker::GetEnableRealTimeSpell

NS_IMETHODIMP
mozInlineSpellChecker::GetEnableRealTimeSpell(PRBool* aEnabled)
{
  NS_ENSURE_ARG_POINTER(aEnabled);
  *aEnabled = mSpellCheck != nsnull;
  return NS_OK;
}

// mozInlineSpellChecker::SetEnableRealTimeSpell

NS_IMETHODIMP
mozInlineSpellChecker::SetEnableRealTimeSpell(PRBool aEnabled)
{
  nsresult res = NS_OK;

  if (aEnabled) {
    if (!mSpellCheck) {
      nsCOMPtr<nsIEditorSpellCheck> spellchecker = do_CreateInstance("@mozilla.org/editor/editorspellchecker;1", &res);
      if (NS_SUCCEEDED(res) && spellchecker)
      {
        nsCOMPtr<nsITextServicesFilter> filter = do_CreateInstance("@mozilla.org/editor/txtsrvfiltermail;1", &res);
        spellchecker->SetFilter(filter);
        nsCOMPtr<nsIEditor> editor (do_QueryReferent(mEditor));
        res = spellchecker->InitSpellChecker(editor, PR_FALSE);
        NS_ENSURE_SUCCESS(res, res);

        nsCOMPtr<nsITextServicesDocument> tsDoc = do_CreateInstance("@mozilla.org/textservices/textservicesdocument;1", &res);
        NS_ENSURE_SUCCESS(res, res);

        res = tsDoc->SetFilter(filter);
        NS_ENSURE_SUCCESS(res, res);

        res = tsDoc->InitWithEditor(editor);
        NS_ENSURE_SUCCESS(res, res);

        mTextServicesDocument = tsDoc;
        mSpellCheck = spellchecker;

        // spell checking is enabled, register our event listeners to track navigation
        RegisterEventListeners();
      }
    }

    // spellcheck the current content
    res = SpellCheckRange(nsnull);
  }
  else 
  {
    nsCOMPtr<nsISelection> spellCheckSelection;
    res = GetSpellCheckSelection(getter_AddRefs(spellCheckSelection));
    NS_ENSURE_SUCCESS(res, res); 
    spellCheckSelection->RemoveAllRanges();    
    mNumWordsInSpellSelection = 0;
    UnregisterEventListeners();
    mSpellCheck = nsnull;
  }

  return res;
}

// mozInlineSpellChecker::SpellCheckAfterEditorChange

NS_IMETHODIMP
mozInlineSpellChecker::SpellCheckAfterEditorChange(
    PRInt32 aAction, nsISelection *aSelection,
    nsIDOMNode *aPreviousSelectedNode, PRInt32 aPreviousSelectedOffset,
    nsIDOMNode *aStartNode, PRInt32 aStartOffset,
    nsIDOMNode *aEndNode, PRInt32 aEndOffset)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aSelection);
  if (!mSpellCheck)
    return NS_OK; // disabling spell checking is not an error

  // the anchor node is the position of the caret
  nsCOMPtr<nsIDOMNode> anchorNode;
  rv = aSelection->GetAnchorNode(getter_AddRefs(anchorNode));
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt32 anchorOffset;
  rv = aSelection->GetAnchorOffset(&anchorOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  // the spell check selection includes all misspelled words
  nsCOMPtr<nsISelection> spellCheckSelection;
  rv = GetSpellCheckSelection(getter_AddRefs(spellCheckSelection));
  NS_ENSURE_SUCCESS(rv, rv);
  CleanupRangesInSelection(spellCheckSelection);

  mozInlineSpellWordUtil wordUtil;
  rv = wordUtil.Init(mEditor);
  if (NS_FAILED(rv))
    return NS_OK; // editor doesn't like us
  wordUtil.GetDocument()->FlushPendingNotifications(Flush_Style);

  nsCOMPtr<nsIDOMRange> wordRange;
  rv = wordUtil.GetRangeForWord(anchorNode, anchorOffset,
                                getter_AddRefs(wordRange));
  NS_ENSURE_SUCCESS(rv, rv);
#ifdef DEBUG_INLINESPELL
  nsString wordRangeText;
  wordRange->ToString(wordRangeText);
  printf("->Editor change, current word is \"%s\"\n",
         NS_ConvertUTF16toUTF8(wordRangeText).get());
#endif

  nsCOMPtr<nsIDOMRange> rangeToCheck;
  if (aAction != kOpDeleteSelection) {
    // Construct a range of everything between the current caret position and
    // the previous caret position. We need to check these. This may fail
    // because text was deleted.
    //
    // We don't do this for deletions and fall through to seeing the range to
    // be the same as the word. This will cause the word to be unhighlighted.
    // When we delete something, nothing but the current word changes.
    wordUtil.GetDocumentRange()->CreateRange(getter_AddRefs(rangeToCheck));
    rv = rangeToCheck->SetStart(aPreviousSelectedNode, aPreviousSelectedOffset);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = rangeToCheck->SetEnd(aPreviousSelectedNode, aPreviousSelectedOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNSRange> nsRange = do_QueryInterface(rangeToCheck, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    PRInt16 cmpResult;
    rv = nsRange->ComparePoint(anchorNode, anchorOffset, &cmpResult);
    NS_ENSURE_SUCCESS(rv, rv);
    if (cmpResult < 0) {
      rv = rangeToCheck->SetStart(anchorNode, anchorOffset);
    } else {
      rv = rangeToCheck->SetEnd(anchorNode, anchorOffset);
    }
    // fall through with NULL rangeToCheck on failure
  }
  if (!rangeToCheck) {
    // On failure, set the range to check to be the range of the current
    // word. This is correct for deletes, and if we have some other error,
    // this is probably the best we can do. Since the range is the same
    // as the word, any selection will be cleared from the word.
    // This may be NULL.
    rangeToCheck = wordRange;
  }

  // When we are given a range of stuff that changed (usually not the case),
  // we should be sure to check all of that stuff, too, so create the union
  // of this and our checking range.
  if (aStartNode && aEndNode) {
    wordUtil.ExpandFor(aStartNode, aStartOffset, aEndNode, aEndOffset);
  }

  if (rangeToCheck) {
    rv = DoSpellCheck(wordUtil, rangeToCheck, wordRange, spellCheckSelection);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // remember the current caret position after every change..
  SaveCurrentSelectionPosition();
  return NS_OK;
}

// mozInlineSpellChecker::SpellCheckRange
//
//    Spellchecks all the words in the given range.
//    Supply a NULL range and this will check the entire editor.

nsresult
mozInlineSpellChecker::SpellCheckRange(nsIDOMRange* aRange)
{
  NS_ENSURE_TRUE(mSpellCheck, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsISelection> spellCheckSelection;
  nsresult rv = GetSpellCheckSelection(getter_AddRefs(spellCheckSelection));
  NS_ENSURE_SUCCESS(rv, rv); 

  CleanupRangesInSelection(spellCheckSelection);

  if(aRange) {
    mozInlineSpellWordUtil wordUtil;
    rv = wordUtil.Init(mEditor);
    if (NS_FAILED(rv))
      return NS_OK; // editor doesn't like us
    rv = DoSpellCheck(wordUtil, aRange, nsnull, spellCheckSelection);
  } else 
  {
    // use full range: SpellCheckBetweenNodes will do the somewhat complicated
    // task of creating a range over the element we give it and call
    // SpellCheckRange(range,selection) for us
    nsCOMPtr<nsIEditor> editor (do_QueryReferent(mEditor));
    if (!editor)
      return NS_ERROR_NOT_INITIALIZED;
    nsCOMPtr<nsIDOMElement> rootElem;
    rv = editor->GetRootElement(getter_AddRefs(rootElem));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = SpellCheckBetweenNodes(rootElem, 0, rootElem, -1, spellCheckSelection);
  }
  return rv;
}

// mozInlineSpellChecker::GetMispelledWord

NS_IMETHODIMP
mozInlineSpellChecker::GetMispelledWord(nsIDOMNode *aNode, PRInt32 aOffset,
                                        nsIDOMRange **newword)
{
  nsCOMPtr<nsISelection> spellCheckSelection;
  nsresult res = GetSpellCheckSelection(getter_AddRefs(spellCheckSelection));
  NS_ENSURE_SUCCESS(res, res); 

  return IsPointInSelection(spellCheckSelection, aNode, aOffset, newword);
}

// mozInlineSpellChecker::ReplaceWord

NS_IMETHODIMP
mozInlineSpellChecker::ReplaceWord(nsIDOMNode *aNode, PRInt32 aOffset,
                                   const nsAString &newword)
{
  nsCOMPtr<nsIEditor> editor (do_QueryReferent(mEditor));
  NS_ENSURE_TRUE(editor, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(newword.Length() != 0, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMRange> range;
  nsresult res = GetMispelledWord(aNode, aOffset, getter_AddRefs(range));
  NS_ENSURE_SUCCESS(res, res); 

  if (range)
  {
    editor->BeginTransaction();
  
    nsCOMPtr<nsISelection> selection;
    res = editor->GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(res, res);
    selection->RemoveAllRanges();
    selection->AddRange(range);
    editor->DeleteSelection(nsIEditor::eNone);

    nsCOMPtr<nsIPlaintextEditor> textEditor(do_QueryReferent(mEditor));
    textEditor->InsertText(newword);

    editor->EndTransaction();
  }

  return NS_OK;
}

// mozInlineSpellChecker::AddWordToDictionary

NS_IMETHODIMP
mozInlineSpellChecker::AddWordToDictionary(const nsAString &word)
{
  NS_ENSURE_TRUE(mSpellCheck, NS_ERROR_NOT_INITIALIZED);

  nsAutoString wordstr(word);
  nsresult res = mSpellCheck->AddWordToDictionary(wordstr.get());
  NS_ENSURE_SUCCESS(res, res); 

  nsCOMPtr<nsISelection> spellCheckSelection;
  nsresult rv = GetSpellCheckSelection(getter_AddRefs(spellCheckSelection));
  NS_ENSURE_SUCCESS(rv, rv);

  return SpellCheckSelection(spellCheckSelection);
}

// mozInlineSpellChecker::IgnoreWord

NS_IMETHODIMP
mozInlineSpellChecker::IgnoreWord(const nsAString &word)
{
  NS_ENSURE_TRUE(mSpellCheck, NS_ERROR_NOT_INITIALIZED);

  nsAutoString wordstr(word);
  nsresult res = mSpellCheck->IgnoreWordAllOccurrences(wordstr.get());
  NS_ENSURE_SUCCESS(res, res); 
  
  nsCOMPtr<nsISelection> spellCheckSelection;
  nsresult rv = GetSpellCheckSelection(getter_AddRefs(spellCheckSelection));
  NS_ENSURE_SUCCESS(rv, rv);
  return SpellCheckSelection(spellCheckSelection);
}

// mozInlineSpellChecker::IgnoreWords

NS_IMETHODIMP
mozInlineSpellChecker::IgnoreWords(const PRUnichar **aWordsToIgnore,
                                   PRUint32 aCount)
{
  // add each word to the ignore list and then recheck the document
  for (PRUint32 index = 0; index < aCount; index++)
    mSpellCheck->IgnoreWordAllOccurrences(aWordsToIgnore[index]);

  nsCOMPtr<nsISelection> spellCheckSelection;
  nsresult rv = GetSpellCheckSelection(getter_AddRefs(spellCheckSelection));
  NS_ENSURE_SUCCESS(rv, rv);

  return SpellCheckSelection(spellCheckSelection);
}

// mozInlineSpellChecker::SpellCheckSelection
//
//    When the user ignores a word or adds a word to the dictionary, we used to
//    re check the entire document, starting at the root element and walking
//    through all the nodes.
//
//    Optimization: The only words in the document that would change as a
//    result of these actions are words that are already in the spell check
//    selection (i.e. words previsouly marked as misspelled). Therefore callers
//    spell check the spell check selection instead of the entire document for
//    ignore word and add word.

nsresult
mozInlineSpellChecker::SpellCheckSelection(nsISelection* aSelection)
{
  NS_ENSURE_ARG_POINTER(aSelection);

  nsCOMPtr<nsISelection> spellCheckSelection;
  nsresult rv = GetSpellCheckSelection(getter_AddRefs(spellCheckSelection));
  NS_ENSURE_SUCCESS(rv, rv);

  // if we are going to be checking the spell check selection, then
  // clear out mNumWordsInSpellSelection since we'll be rebuilding the ranges.
  if (aSelection == spellCheckSelection.get())
    mNumWordsInSpellSelection = 0; 

  // Optimize for the case where aSelection is in fact the spell check selection.
  // Since we could be modifying the ranges for the spellCheckSelection while looping
  // on the spell check selection, keep a separate array of range elements inside the selection
  PRInt32 count;
  nsCOMPtr <nsIMutableArray> ranges = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  aSelection->GetRangeCount(&count);
  PRInt32 index;
  for (index = 0; index < count; index++)
  {
    nsCOMPtr<nsIDOMRange> checkRange;
    aSelection->GetRangeAt(index, getter_AddRefs(checkRange));
    if (checkRange)
      ranges->AppendElement(checkRange, PR_FALSE);
  }

  // now loop over these ranges and spell check each range
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 startOffset, endOffset;
  nsCOMPtr<nsIDOMRange> checkRange;

  for (index = 0; index < count; index++)
  {
    checkRange = do_QueryElementAt(ranges, index); 
    if (checkRange)
    {
      checkRange->GetStartContainer(getter_AddRefs(startNode));
      checkRange->GetEndContainer(getter_AddRefs(endNode));
      checkRange->GetStartOffset(&startOffset);
      checkRange->GetEndOffset(&endOffset);

      rv |= SpellCheckBetweenNodes(startNode, startOffset, endNode,
                                   endOffset, spellCheckSelection);
    }
  }

  return rv;
}

NS_IMETHODIMP mozInlineSpellChecker::WillCreateNode(const nsAString & aTag, nsIDOMNode *aParent, PRInt32 aPosition)
{
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::DidCreateNode(const nsAString & aTag, nsIDOMNode *aNode, nsIDOMNode *aParent,
                                                   PRInt32 aPosition, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::WillInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent,
                                                    PRInt32 aPosition)
{
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::DidInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent,
                                                   PRInt32 aPosition, nsresult aResult)
{

  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::WillDeleteNode(nsIDOMNode *aChild)
{
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::DidDeleteNode(nsIDOMNode *aChild, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::WillSplitNode(nsIDOMNode *aExistingRightNode, PRInt32 aOffset)
{
  return NS_OK;
}

NS_IMETHODIMP
mozInlineSpellChecker::DidSplitNode(nsIDOMNode *aExistingRightNode,
                                    PRInt32 aOffset,
                                    nsIDOMNode *aNewLeftNode, nsresult aResult)
{
  return SpellCheckBetweenNodes(aNewLeftNode, 0, aNewLeftNode, 0, NULL);
}

NS_IMETHODIMP mozInlineSpellChecker::WillJoinNodes(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode, nsIDOMNode *aParent)
{
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::DidJoinNodes(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode, 
                                                  nsIDOMNode *aParent, nsresult aResult)
{
  return SpellCheckBetweenNodes(aRightNode, 0, aRightNode, 0, NULL);
}

NS_IMETHODIMP mozInlineSpellChecker::WillInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsAString & aString)
{
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::DidInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset,
                                                   const nsAString & aString, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::WillDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, PRInt32 aLength)
{
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::DidDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, PRInt32 aLength, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::WillDeleteSelection(nsISelection *aSelection)
{
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::DidDeleteSelection(nsISelection *aSelection)
{
  return NS_OK;
}


// mozInlineSpellChecker::SpellCheckBetweenNodes
//
//    Given begin and end positions, this function constructs a range as
//    required for DoSpellCheck, which then does the actual checking.

nsresult
mozInlineSpellChecker::SpellCheckBetweenNodes(nsIDOMNode *aStartNode,
                                              PRInt32 aStartOffset,
                                              nsIDOMNode *aEndNode,
                                              PRInt32 aEndOffset,
                                              nsISelection *aSpellCheckSelection)
{
  nsresult res;
  nsCOMPtr<nsISelection> spellCheckSelection = aSpellCheckSelection;
  nsCOMPtr<nsIEditor> editor (do_QueryReferent(mEditor));
  NS_ENSURE_TRUE(editor, NS_ERROR_NULL_POINTER);

  if (!spellCheckSelection)
  {
    res = GetSpellCheckSelection(getter_AddRefs(spellCheckSelection));
    NS_ENSURE_SUCCESS(res, res); 
  }

  nsCOMPtr<nsIDOMDocument> doc;
  res = editor->GetDocument(getter_AddRefs(doc));
  NS_ENSURE_SUCCESS(res, res); 

  nsCOMPtr<nsIDOMDocumentRange> docrange = do_QueryInterface(doc);
  NS_ENSURE_TRUE(docrange, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMRange> range;
  res = docrange->CreateRange(getter_AddRefs(range));
  NS_ENSURE_SUCCESS(res, res); 

  if (aEndOffset == -1)
  {
    nsCOMPtr<nsIDOMNodeList> childNodes;
    res = aEndNode->GetChildNodes(getter_AddRefs(childNodes));
    NS_ENSURE_SUCCESS(res, res);

    PRUint32 childCount;
    res = childNodes->GetLength(&childCount);
    NS_ENSURE_SUCCESS(res, res); 

    aEndOffset = childCount;
  }

  // sometimes we are are requested to check an empty range (possibly an empty
  // document). This will result in assertions later.
  if (aStartNode == aEndNode && aStartOffset == aEndOffset)
    return NS_OK;

  range->SetStart(aStartNode,aStartOffset);

  if (aEndOffset) 
    range->SetEnd(aEndNode, aEndOffset);
  else 
    range->SetEndAfter(aEndNode);

  // HACK: try to avoid an assertion later on in the code when we iterate
  // over a range with only one character in it. if the range is really only one
  // character, bail out early..
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 startOffset;
  PRInt32 endOffset; 
  range->GetStartContainer(getter_AddRefs(startNode));
  range->GetStartOffset(&startOffset);
  range->GetEndContainer(getter_AddRefs(endNode));
  range->GetEndOffset(&endOffset);

  if (startNode == endNode && startOffset == endOffset)
    return NS_OK; // don't call adjust spell highlighting for a single character

  mozInlineSpellWordUtil wordUtil;
  res = wordUtil.Init(mEditor);
  if (NS_FAILED(res))
    return NS_OK; // editor doesn't like us
  return DoSpellCheck(wordUtil, range, nsnull, spellCheckSelection);
}

static inline PRBool IsNonwordChar(PRUnichar chr)
{
  // a non-word character is one that can end a word, such as whitespace or
  // most punctuation. 
  // mscott: We probably need to modify this to make it work for non ascii 
  // based languages...  but then again our spell checker doesn't support 
  // multi byte languages anyway.
  // jshin: one way to make the word boundary checker more generic is to
  // use 'word breaker(s)' in intl.
  // Need to fix callers (of IsNonwordChar) to pass PRUint32
  return ((chr != '\'') && (GetCat(PRUint32(chr)) != 5));
}


// mozInlineSpellChecker::SkipSpellCheckForNode
//
//    There are certain conditions when we don't want to spell check a node. In
//    particular quotations, moz signatures, etc. This routine returns false
//    for these cases.

nsresult
mozInlineSpellChecker::SkipSpellCheckForNode(nsIDOMNode *aNode,
                                             PRBool *checkSpelling)
{
  *checkSpelling = PR_TRUE;
  NS_ENSURE_ARG_POINTER(aNode);

  nsCOMPtr<nsIEditor> editor (do_QueryReferent(mEditor));
  NS_ENSURE_TRUE(editor, NS_ERROR_NULL_POINTER);

  PRUint32 flags;
  editor->GetFlags(&flags);
  if (flags & nsIPlaintextEditor::eEditorMailMask)
  {
    nsCOMPtr<nsIDOMNode> parent;
    aNode->GetParentNode(getter_AddRefs(parent));

    while (parent)
    {
      nsCOMPtr<nsIDOMElement> parentElement = do_QueryInterface(parent);
      if (!parentElement)
        break;

      nsAutoString parentTagName;
      parentElement->GetTagName(parentTagName);

      if (parentTagName.Equals(NS_LITERAL_STRING("blockquote"), nsCaseInsensitiveStringComparator()))
      {
        *checkSpelling = PR_FALSE;
        break;
      }
      else if (parentTagName.Equals(NS_LITERAL_STRING("pre"), nsCaseInsensitiveStringComparator()))
      {
        nsAutoString classname;
        parentElement->GetAttribute(NS_LITERAL_STRING("class"),classname);
        if (classname.Equals(NS_LITERAL_STRING("moz-signature")))
          *checkSpelling = PR_FALSE;
      }

      nsCOMPtr<nsIDOMNode> nextParent;
      parent->GetParentNode(getter_AddRefs(nextParent));
      parent = nextParent;
    }
  }

  return NS_OK;
}


// mozInlineSpellChecker::DoSpellCheck
//
//    This function checks words intersecting the given range, excluding those
//    inside aNoCheckRange (can be NULL). Words inside aNoCheckRange will have
//    any spell selection removed (this is used to hide the underlining for the
//    word that the caret is in). aNoCheckRange should be on word boundaries.
//
//    Normal case when editing text by typing
//       h e l l o   w o r k d   h o w   a r e   y o u
//                            ^ caret
//                   [-------] aRange
//                   [-------] aNoCheckRange
//      -> does nothing (range is the same as the no check range)
//
//    Case when pasting:
//             [---------- pasted text ----------]
//       h e l l o   w o r k d   h o w   a r e   y o u
//                                                ^ caret
//                                               [---] aNoCheckRange
//      -> recheck all words in range except those in aNoCheckRange

nsresult mozInlineSpellChecker::DoSpellCheck(mozInlineSpellWordUtil& aWordUtil,
                                             nsIDOMRange *aRange,
                                             nsIDOMRange *aNoCheckRange,
                                             nsISelection *aSpellCheckSelection)
{
  nsCOMPtr<nsIDOMNode> beginNode, endNode;
  PRInt32 beginOffset, endOffset;

  PRBool iscollapsed;
  nsresult rv = aRange->GetCollapsed(&iscollapsed);
  NS_ENSURE_SUCCESS(rv, rv);
  if (iscollapsed)
    return NS_OK;

  // set the starting DOM position to be the beginning of our range
  NS_ENSURE_SUCCESS(rv, rv);
  aRange->GetStartContainer(getter_AddRefs(beginNode));
  aRange->GetStartOffset(&beginOffset);
  aRange->GetEndContainer(getter_AddRefs(endNode));
  aRange->GetEndOffset(&endOffset);
  aWordUtil.SetEnd(endNode, endOffset);
  aWordUtil.SetPosition(beginNode, beginOffset);

  // we need to use IsPointInRange which is on a more specific interface
  nsCOMPtr<nsIDOMNSRange> noCheckRange;
  if (aNoCheckRange)
    noCheckRange = do_QueryInterface(aNoCheckRange);

  nsAutoString wordText;
  nsCOMPtr<nsIDOMRange> wordRange;
  PRBool dontCheckWord;
  while (NS_SUCCEEDED(aWordUtil.GetNextWord(wordText,
                                             getter_AddRefs(wordRange),
                                             &dontCheckWord)) &&
         wordRange) {
    // get the range for the current word
    wordRange->GetStartContainer(getter_AddRefs(beginNode));
    wordRange->GetEndContainer(getter_AddRefs(endNode));
    wordRange->GetStartOffset(&beginOffset);
    wordRange->GetEndOffset(&endOffset);

#ifdef DEBUG_INLINESPELL
    printf("->Got word \"%s\"", NS_ConvertUTF16toUTF8(wordText).get());
    if (dontCheckWord)
      printf(" (not checking)");
    printf("\n");
#endif

    // see if there is a spellcheck range that already intersects the word
    // and remove it
    nsCOMPtr<nsIDOMRange> currentRange;
    IsPointInSelection(aSpellCheckSelection, beginNode, beginOffset,
                       getter_AddRefs(currentRange));
    if (!currentRange)
      IsPointInSelection(aSpellCheckSelection, endNode, endOffset - 1,
                         getter_AddRefs(currentRange));
    if (currentRange)
      RemoveRange(aSpellCheckSelection, currentRange);

    // some words are special and don't need checking
    if (dontCheckWord)
      continue;

    // some nodes we don't spellcheck
    PRBool checkSpelling;
    SkipSpellCheckForNode(beginNode, &checkSpelling);
    if (!checkSpelling)
      continue;

    // Don't check spelling if we're inside the noCheckRange. This needs to
    // be done after we clear any old selection because the excluded word
    // might have been previously marked.
    //
    // We do a simple check to see if the beginning of our word is in the
    // exclusion range. Because the exclusion range is a multiple of a word,
    // this is sufficient.
    if (noCheckRange) {
      PRBool inExclusion = PR_FALSE;
      noCheckRange->IsPointInRange(beginNode, beginOffset, &inExclusion);
      if (inExclusion)
        continue;
    }

    // check spelling and add to selection if misspelled
    PRBool isMisspelled;
    aWordUtil.NormalizeWord(wordText);
    rv = mSpellCheck->CheckCurrentWordNoSuggest(wordText.get(), &isMisspelled);
    if (isMisspelled)
      AddRange(aSpellCheckSelection, wordRange);
  }

  return NS_OK;
}


// mozInlineSpellChecker::IsPointInSelection
//
//    Determines if a given (node,offset) point is inside the given
//    selection. If so, the specific range of the selection that
//    intersects is places in *aRange. (There may be multiple disjoint
//    ranges in a selection.)
//
//    If there is no intersection, *aRange will be NULL.

nsresult
mozInlineSpellChecker::IsPointInSelection(nsISelection *aSelection,
                                          nsIDOMNode *aNode,
                                          PRInt32 aOffset,
                                          nsIDOMRange **aRange)
{
  *aRange = NULL;
  NS_ENSURE_ARG_POINTER(aNode);
  NS_ENSURE_ARG_POINTER(aSelection);

  PRInt32 count;
  aSelection->GetRangeCount(&count);

  PRInt32 t;
  for (t=0; t<count; t++)
  {
    nsCOMPtr<nsIDOMRange> checkRange;
    aSelection->GetRangeAt(t,getter_AddRefs(checkRange));
    nsCOMPtr<nsIDOMNSRange> nsCheckRange = do_QueryInterface(checkRange);

    PRInt32 start, end;
    checkRange->GetStartOffset(&start);
    checkRange->GetEndOffset(&end);

    PRBool isInRange;
    nsCheckRange->IsPointInRange(aNode,aOffset,&isInRange);
    if (isInRange)
    {
      *aRange = checkRange;
      NS_ADDREF(*aRange);
      break;
    }
  }

  return NS_OK;
}

nsresult
mozInlineSpellChecker::CleanupRangesInSelection(nsISelection *aSelection)
{
  // integrity check - remove ranges that have collapsed to nothing. This
  // can happen if the node containing a highlighted word was removed.
  NS_ENSURE_ARG_POINTER(aSelection);

  PRInt32 count;
  aSelection->GetRangeCount(&count);

  for (PRInt32 index = 0; index < count; index++)
  {
    nsCOMPtr<nsIDOMRange> checkRange;
    aSelection->GetRangeAt(index, getter_AddRefs(checkRange));

    if (checkRange)
    {
      PRBool collapsed;
      checkRange->GetCollapsed(&collapsed);
      if (collapsed)
      {
        RemoveRange(aSelection, checkRange);
        index--;
        count--;
      }
    }
  }

  return NS_OK;
}


// mozInlineSpellChecker::RemoveRange
//
//    For performance reasons, we have an upper bound on the number of word
//    ranges  in the spell check selection. When removing a range from the
//    selection, we need to decrement mNumWordsInSpellSelection

nsresult
mozInlineSpellChecker::RemoveRange(nsISelection* aSpellCheckSelection,
                                   nsIDOMRange* aRange)
{
  NS_ENSURE_ARG_POINTER(aSpellCheckSelection);
  NS_ENSURE_ARG_POINTER(aRange);

  nsresult rv = aSpellCheckSelection->RemoveRange(aRange);
  if (NS_SUCCEEDED(rv) && mNumWordsInSpellSelection)
    mNumWordsInSpellSelection--;

  return rv;
}


// mozInlineSpellChecker::AddRange
//
//    For performance reasons, we have an upper bound on the number of word
//    ranges we'll add to the spell check selection. Once we reach that upper
//    bound, stop adding the ranges

nsresult
mozInlineSpellChecker::AddRange(nsISelection* aSpellCheckSelection,
                                nsIDOMRange* aRange)
{
  NS_ENSURE_ARG_POINTER(aSpellCheckSelection);
  NS_ENSURE_ARG_POINTER(aRange);

  nsresult rv = NS_OK;

  if (!SpellCheckSelectionIsFull())
  {
    rv = aSpellCheckSelection->AddRange(aRange);
    if (NS_SUCCEEDED(rv))
      mNumWordsInSpellSelection++;
  }

  return rv;
}

nsresult mozInlineSpellChecker::GetSpellCheckSelection(nsISelection ** aSpellCheckSelection)
{ 
  nsCOMPtr<nsIEditor> editor (do_QueryReferent(mEditor));
  NS_ENSURE_TRUE(editor, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsISelectionController> selcon;
  nsresult rv = editor->GetSelectionController(getter_AddRefs(selcon));
  NS_ENSURE_SUCCESS(rv, rv); 

  nsCOMPtr<nsISelection> spellCheckSelection;
  return selcon->GetSelection(nsISelectionController::SELECTION_SPELLCHECK, aSpellCheckSelection);
}

nsresult mozInlineSpellChecker::SaveCurrentSelectionPosition()
{
  nsCOMPtr<nsIEditor> editor (do_QueryReferent(mEditor));
  NS_ENSURE_TRUE(editor, NS_OK);

  // figure out the old caret position based on the current selection
  nsCOMPtr<nsISelection> selection;
  nsresult rv = editor->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = selection->GetFocusNode(getter_AddRefs(mCurrentSelectionAnchorNode));
  NS_ENSURE_SUCCESS(rv, rv);
  
  selection->GetFocusOffset(&mCurrentSelectionOffset);

  return NS_OK;
}

// mozInlineSpellChecker::HandleNavigationEvent
//
//    Acts upon mouse clicks and keyboard navigation changes, spell checking
//    the previous word if the new navigation location moves us to another
//    word.
//
//    This is complicated by the fact that our mouse events are happening after
//    selection has been changed to account for the mouse click. But keyboard
//    events are happening before the caret selection has changed. Working
//    around this by letting keyboard events setting forceWordSpellCheck to
//    true. aNewPositionOffset also tries to work around this for the
//    DOM_VK_RIGHT and DOM_VK_LEFT cases.

nsresult
mozInlineSpellChecker::HandleNavigationEvent(nsIDOMEvent* aEvent,
                                             PRBool aForceWordSpellCheck,
                                             PRInt32 aNewPositionOffset)
{
  // get the current selection and compare it to the new selection.
  nsresult rv;

  nsCOMPtr<nsIDOMNode> currentAnchorNode = mCurrentSelectionAnchorNode;
  PRInt32 currentAnchorOffset = mCurrentSelectionOffset;

  // now remember the new focus position resulting from the event
  rv = SaveCurrentSelectionPosition();
  NS_ENSURE_SUCCESS(rv, rv);

  // No current selection (this can happen the first time you focus empty
  // windows). Since we just called SaveCurrentSelectionPosition it will be
  // initialized for next time.
  if (! currentAnchorNode)
    return NS_OK; 

  mozInlineSpellWordUtil wordUtil;
  rv = wordUtil.Init(mEditor);
  if (NS_FAILED(rv))
    return NS_OK; // editor doesn't like us

  // expand the old selection into a range for the nearest word boundary
  nsCOMPtr<nsIDOMRange> currentWordRange;
  rv = wordUtil.GetRangeForWord(currentAnchorNode, currentAnchorOffset,
                                getter_AddRefs(currentWordRange));
  NS_ENSURE_SUCCESS(rv, rv);
  if (! currentWordRange)
    return NS_OK;

  nsCOMPtr<nsIDOMNSRange> currentWordNSRange = do_QueryInterface(currentWordRange, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isInRange;
  rv = currentWordNSRange->IsPointInRange(mCurrentSelectionAnchorNode, mCurrentSelectionOffset + aNewPositionOffset, &isInRange);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!isInRange || aForceWordSpellCheck) // selection is moving to a new word, spell check the current word
  {
    nsCOMPtr<nsISelection> spellCheckSelection; 
    GetSpellCheckSelection(getter_AddRefs(spellCheckSelection)); 

    rv = DoSpellCheck(wordUtil, currentWordRange, nsnull, spellCheckSelection);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::MouseClick(nsIDOMEvent *aMouseEvent)
{
  // ignore any errors from HandleNavigationEvent as we don't want to prevent 
  // anyone else from seeing this event.
  //
  // FIXME-performance: this re-checks the word on all mouse click events,
  // even right click or other clicks that do not change the caret position.
  // We should notice when this happens and not waste time re-checking.
  HandleNavigationEvent(aMouseEvent, PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::MouseDown(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::MouseUp(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::MouseOver(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::MouseOut(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

NS_IMETHODIMP mozInlineSpellChecker::KeyDown(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}


NS_IMETHODIMP mozInlineSpellChecker::KeyUp(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}


NS_IMETHODIMP mozInlineSpellChecker::KeyPress(nsIDOMEvent* aKeyEvent)
{
  nsCOMPtr<nsIDOMKeyEvent>keyEvent = do_QueryInterface(aKeyEvent);
  NS_ENSURE_TRUE(keyEvent, NS_OK);

  PRUint32 keyCode;
  keyEvent->GetKeyCode(&keyCode);

  // we only care about navigation keys that moved selection 
  switch (keyCode)
  {
    case nsIDOMKeyEvent::DOM_VK_RIGHT:
    case nsIDOMKeyEvent::DOM_VK_LEFT:
      HandleNavigationEvent(aKeyEvent, PR_FALSE, keyCode == nsIDOMKeyEvent::DOM_VK_RIGHT ? 1 : -1);
      break;
    case nsIDOMKeyEvent::DOM_VK_UP:
    case nsIDOMKeyEvent::DOM_VK_DOWN:
    case nsIDOMKeyEvent::DOM_VK_HOME:
    case nsIDOMKeyEvent::DOM_VK_END:
    case nsIDOMKeyEvent::DOM_VK_PAGE_UP:
    case nsIDOMKeyEvent::DOM_VK_PAGE_DOWN:
      HandleNavigationEvent(aKeyEvent, PR_TRUE /* force a spelling correction */);
      break;
  }

  return NS_OK;
}
