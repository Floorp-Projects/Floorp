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
#include "nsIDOMRange.h"
#include "nsIDOMCharacterData.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsEditorUtils.h"
#include "EditTxn.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsUnicharUtils.h"

// for IBMBIDI
#include "nsIPresShell.h"

#define CANCEL_OPERATION_IF_READONLY_OR_DISABLED \
  if ((mFlags & nsIPlaintextEditor::eEditorReadonlyMask) || (mFlags & nsIPlaintextEditor::eEditorDisabledMask)) \
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
, mBogusNode(nsnull)
, mBody(nsnull)
, mFlags(0) // initialized to 0 ("no flags set").  Real initial value is given in Init()
, mActionNesting(0)
, mLockRulesSniffing(PR_FALSE)
, mDidExplicitlySetInterline(PR_FALSE)
, mTheAction(0)
{
}

nsTextEditRules::~nsTextEditRules()
{
   // do NOT delete mEditor here.  We do not hold a ref count to mEditor.  mEditor owns our lifespan.
}

/********************************************************
 *  XPCOM Cruft
 ********************************************************/

NS_IMPL_ISUPPORTS1(nsTextEditRules, nsIEditRules)


/********************************************************
 *  Public methods 
 ********************************************************/

NS_IMETHODIMP
nsTextEditRules::Init(nsPlaintextEditor *aEditor, PRUint32 aFlags)
{
  if (!aEditor) { return NS_ERROR_NULL_POINTER; }

  mEditor = aEditor;  // we hold a non-refcounted reference back to our editor
  // call SetFlags only aftet mEditor has been initialized!
  SetFlags(aFlags);
  nsCOMPtr<nsISelection> selection;
  mEditor->GetSelection(getter_AddRefs(selection));
  NS_ASSERTION(selection, "editor cannot get selection");

  // remember our root node
  nsCOMPtr<nsIDOMElement> bodyElement;
  nsresult res = mEditor->GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  if (!bodyElement) return NS_ERROR_NULL_POINTER;
  mBody = do_QueryInterface(bodyElement);
  if (!mBody) return NS_ERROR_FAILURE;

  // put in a magic br if needed
  res = CreateBogusNodeIfNeeded(selection);   // this method handles null selection, which should never happen anyway
  if (NS_FAILED(res)) return res;

  if (mFlags & nsIPlaintextEditor::eEditorPlaintextMask)
  {
    // ensure trailing br node
    res = CreateTrailingBRIfNeeded();
    if (NS_FAILED(res)) return res;
  }
  
  // create a range that is the entire body contents
  nsCOMPtr<nsIDOMRange> wholeDoc = do_CreateInstance("@mozilla.org/content/range;1");
  if (!wholeDoc) return NS_ERROR_NULL_POINTER;
  wholeDoc->SetStart(mBody,0);
  nsCOMPtr<nsIDOMNodeList> list;
  res = mBody->GetChildNodes(getter_AddRefs(list));
  if (NS_FAILED(res)) return res;
  if (!list) return NS_ERROR_FAILURE;

  PRUint32 listCount;
  res = list->GetLength(&listCount);
  if (NS_FAILED(res)) return res;

  res = wholeDoc->SetEnd(mBody, listCount);
  if (NS_FAILED(res)) return res;

  // replace newlines in that range with breaks
  return ReplaceNewlines(wholeDoc);
}

NS_IMETHODIMP
nsTextEditRules::GetFlags(PRUint32 *aFlags)
{
  if (!aFlags) { return NS_ERROR_NULL_POINTER; }
  *aFlags = mFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsTextEditRules::SetFlags(PRUint32 aFlags)
{
  if (mFlags == aFlags) return NS_OK;
  
  // XXX - this won't work if body element already has
  // a style attribute on it, don't know why.
  // SetFlags() is really meant to only be called once
  // and at editor init time.  

  mFlags = aFlags;
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
    if (NS_FAILED(res)) return res;
  
    // detect empty doc
    res = CreateBogusNodeIfNeeded(selection);
    if (NS_FAILED(res)) return res;
    
    // insure trailing br node
    res = CreateTrailingBRIfNeeded();
    if (NS_FAILED(res)) return res;
    
    /* After inserting text the cursor Bidi level must be set to the level of the inserted text.
     * This is difficult, because we cannot know what the level is until after the Bidi algorithm
     * is applied to the whole paragraph.
     *
     * So we set the cursor Bidi level to UNDEFINED here, and the caret code will set it correctly later
     */
    if (action == nsEditor::kOpInsertText
        || action == nsEditor::kOpInsertIMEText) {
      nsCOMPtr<nsIPresShell> shell;
      mEditor->GetPresShell(getter_AddRefs(shell));
      if (shell) {
        shell->UndefineCaretBidiLevel();
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
  nsTextRulesInfo *info = NS_STATIC_CAST(nsTextRulesInfo*, aInfo);
    
  switch (info->action)
  {
    case kInsertBreak:
      return WillInsertBreak(aSelection, aCancel, aHandled);
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

  if (!aSelection || !aInfo) 
    return NS_ERROR_NULL_POINTER;
    
  // my kingdom for dynamic cast
  nsTextRulesInfo *info = NS_STATIC_CAST(nsTextRulesInfo*, aInfo);

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
  if (!aDocumentIsEmpty)
    return NS_ERROR_NULL_POINTER;
  
  *aDocumentIsEmpty = (mBogusNode.get() != nsnull);
  return NS_OK;
}

/********************************************************
 *  Protected methods 
 ********************************************************/


nsresult
nsTextEditRules::WillInsert(nsISelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel)
    return NS_ERROR_NULL_POINTER;
  
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED

  // initialize out param
  *aCancel = PR_FALSE;
  
  // check for the magic content node and delete it if it exists
  if (mBogusNode)
  {
    mEditor->DeleteNode(mBogusNode);
    mBogusNode = do_QueryInterface(nsnull);
  }

  return NS_OK;
}

nsresult
nsTextEditRules::DidInsert(nsISelection *aSelection, nsresult aResult)
{
  return NS_OK;
}

nsresult
nsTextEditRules::WillInsertBreak(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  *aHandled = PR_FALSE;
  if (mFlags & nsIPlaintextEditor::eEditorSingleLineMask) {
    *aCancel = PR_TRUE;
  }
  else 
  {
    *aCancel = PR_FALSE;

    // if the selection isn't collapsed, delete it.
    PRBool bCollapsed;
    nsresult res = aSelection->GetIsCollapsed(&bCollapsed);
    if (NS_FAILED(res)) return res;
    if (!bCollapsed)
    {
      res = mEditor->DeleteSelection(nsIEditor::eNone);
      if (NS_FAILED(res)) return res;
    }

    res = WillInsert(aSelection, aCancel);
    if (NS_FAILED(res)) return res;
    // initialize out param
    // we want to ignore result of WillInsert()
    *aCancel = PR_FALSE;
  
  }
  return NS_OK;
}

nsresult
nsTextEditRules::DidInsertBreak(nsISelection *aSelection, nsresult aResult)
{
  // we only need to execute the stuff below if we are a plaintext editor.
  // html editors have a different mechanism for putting in mozBR's
  // (because there are a bunch more places you have to worry about it in html) 
  if (!nsIPlaintextEditor::eEditorPlaintextMask & mFlags) return NS_OK;

  // if we are at the end of the document, we need to insert 
  // a special mozBR following the normal br, and then set the
  // selection to stick to the mozBR.
  PRInt32 selOffset;
  nsCOMPtr<nsIDOMNode> selNode;
  nsresult res;
  res = mEditor->GetStartNodeAndOffset(aSelection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  // confirm we are at end of document
  if (selOffset == 0) return NS_OK;  // cant be after a br if we are at offset 0
  nsCOMPtr<nsIDOMElement> rootElem;
  res = mEditor->GetRootElement(getter_AddRefs(rootElem));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> root = do_QueryInterface(rootElem);
  if (!root) return NS_ERROR_NULL_POINTER;
  if (selNode != root) return NS_OK; // must be inside text node or somewhere other than end of root

  nsCOMPtr<nsIDOMNode> temp = mEditor->GetChildAt(selNode, selOffset);
  if (temp) return NS_OK; // can't be at end if there is a node after us.

  nsCOMPtr<nsIDOMNode> nearNode = mEditor->GetChildAt(selNode, selOffset-1);
  if (nearNode && nsTextEditUtils::IsBreak(nearNode) && !nsTextEditUtils::IsMozBR(nearNode))
  {
    nsCOMPtr<nsISelectionPrivate>selPrivate(do_QueryInterface(aSelection));
    // need to insert special moz BR. Why?  Because if we don't
    // the user will see no new line for the break.  Also, things
    // like table cells won't grow in height.
    nsCOMPtr<nsIDOMNode> brNode;
    res = CreateMozBR(selNode, selOffset, address_of(brNode));
    if (NS_FAILED(res)) return res;

    res = nsEditor::GetNodeLocation(brNode, address_of(selNode), &selOffset);
    if (NS_FAILED(res)) return res;
    selPrivate->SetInterlinePosition(PR_TRUE);
    res = aSelection->Collapse(selNode, selOffset);
    if (NS_FAILED(res)) return res;
  }
  return res;
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
  nsresult res = TruncateInsertionIfNeeded(aSelection, inString, outString, aMaxLength);
  if (NS_FAILED(res)) return res;
  
  PRInt32 start = 0;
  PRInt32 end = 0;  

  // handle password field docs
  if (mFlags & nsIPlaintextEditor::eEditorPasswordMask)
  {
    res = mEditor->GetTextSelectionOffsets(aSelection, start, end);
    NS_ASSERTION((NS_SUCCEEDED(res)), "getTextSelectionOffsets failed!");
    if (NS_FAILED(res)) return res;
  }

  // if the selection isn't collapsed, delete it.
  PRBool bCollapsed;
  res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed)
  {
    res = mEditor->DeleteSelection(nsIEditor::eNone);
    if (NS_FAILED(res)) return res;
  }

  res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  
  // handle password field data
  // this has the side effect of changing all the characters in aOutString
  // to the replacement character
  if (mFlags & nsIPlaintextEditor::eEditorPasswordMask)
  {
    if (aAction == kInsertTextIME)  {
      res = RemoveIMETextFromPWBuf(start, outString);
      if (NS_FAILED(res)) return res;
    }
  }

  // People have lots of different ideas about what text fields
  // should do with multiline pastes.  See bugs 21032, 23485, 23485, 50935.
  // The four possible options are:
  // 0. paste newlines intact
  // 1. paste up to the first newline
  // 2. replace newlines with spaces
  // 3. strip newlines
  // 4. replace with commas
  // So find out what we're expected to do:
  enum {
    ePasteIntact = 0, ePasteFirstLine = 1,
    eReplaceWithSpaces = 2, eStripNewlines = 3, 
    eReplaceWithCommas = 4
  };
  PRInt32 singleLineNewlineBehavior = 1;
  nsCOMPtr<nsIPrefBranch> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &res);
  if (NS_SUCCEEDED(res) && prefBranch)
    res = prefBranch->GetIntPref("editor.singleLine.pasteNewlines",
                                 &singleLineNewlineBehavior);

  if (nsIPlaintextEditor::eEditorSingleLineMask & mFlags)
  {
    nsAutoString tString(*outString);

    if (singleLineNewlineBehavior == eReplaceWithSpaces)
    {
      //nsAString destString;
      //NormalizeCRLF(outString,destString);

      tString.ReplaceChar(CRLF, ' ');
    }
    else if (singleLineNewlineBehavior == eStripNewlines)
      tString.StripChars(CRLF);
    else if (singleLineNewlineBehavior == ePasteFirstLine)
    {
      PRInt32 firstCRLF = tString.FindCharInSet(CRLF);
      if (firstCRLF > 0)
        tString.Truncate(firstCRLF);
    }
    else if (singleLineNewlineBehavior == eReplaceWithCommas)
    {
      tString.Trim(CRLF, PR_TRUE, PR_TRUE);
      tString.ReplaceChar(CRLF, ',');
    }
    else // even if we're pasting newlines, don't paste leading/trailing ones
      tString.Trim(CRLF, PR_TRUE, PR_TRUE);

    outString->Assign(tString);
  }

  if (mFlags & nsIPlaintextEditor::eEditorPasswordMask)
  {
    res = EchoInsertionToPWBuff(start, end, outString);
    if (NS_FAILED(res)) return res;
  }

  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;
  res = mEditor->GetStartNodeAndOffset(aSelection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;

  // don't put text in places that can't have it
  if (!mEditor->IsTextNode(selNode) && !mEditor->CanContainTag(selNode, NS_LITERAL_STRING("__moz_text")))
    return NS_ERROR_FAILURE;

  // we need to get the doc
  nsCOMPtr<nsIDOMDocument>doc;
  res = mEditor->GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(res)) return res;
  if (!doc) return NS_ERROR_NULL_POINTER;
    
  if (aAction == kInsertTextIME) 
  { 
    res = mEditor->InsertTextImpl(*outString, address_of(selNode), &selOffset, doc);
    if (NS_FAILED(res)) return res;
  }
  else // aAction == kInsertText
  {
    // find where we are
    nsCOMPtr<nsIDOMNode> curNode = selNode;
    PRInt32 curOffset = selOffset;

    // is our text going to be PREformatted?  
    // We remember this so that we know how to handle tabs.
    PRBool isPRE;
    res = mEditor->IsPreformatted(selNode, &isPRE);
    if (NS_FAILED(res)) return res;    

    // don't spaz my selection in subtransactions
    nsAutoTxnsConserveSelection dontSpazMySelection(mEditor);
    nsString tString(*outString);
    const PRUnichar *unicodeBuf = tString.get();
    nsCOMPtr<nsIDOMNode> unused;
    PRInt32 pos = 0;

    // for efficiency, break out the pre case separately.  This is because
    // it's a lot cheaper to search the input string for only newlines than
    // it is to search for both tabs and newlines.
    if (isPRE)
    {
      while (unicodeBuf && (pos != -1) && ((PRUint32)pos < tString.Length()))
      {
        PRInt32 oldPos = pos;
        PRInt32 subStrLen;
        pos = tString.FindChar(nsCRT::LF, oldPos);
        
        if (pos != -1) 
        {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (subStrLen == 0)
            subStrLen = 1;
        }
        else
        {
          subStrLen = tString.Length() - oldPos;
          pos = tString.Length();
        }

        nsDependentSubstring subStr(tString, oldPos, subStrLen);
        
        // is it a return?
        if (subStr.EqualsLiteral(LFSTR))
        {
          if (nsIPlaintextEditor::eEditorSingleLineMask & mFlags)
          {
            NS_ASSERTION((singleLineNewlineBehavior == ePasteIntact),
                  "Newline improperly getting into single-line edit field!");
            res = mEditor->InsertTextImpl(subStr, address_of(curNode), &curOffset, doc);
          }
          else
          {
            res = mEditor->CreateBRImpl(address_of(curNode), &curOffset, address_of(unused), nsIEditor::eNone);

            // If the newline is the last character in the string, and the BR we
            // just inserted is the last node in the content tree, we need to add
            // a mozBR so that a blank line is created.

            if (NS_SUCCEEDED(res) && curNode && pos == (PRInt32)(tString.Length() - 1))
            {
              nsCOMPtr<nsIDOMNode> nextChild = mEditor->GetChildAt(curNode, curOffset);

              if (!nextChild)
              {
                // We must be at the end since there isn't a nextChild.
                //
                // curNode and curOffset should be set to the position after
                // the BR we added above, so just create a mozBR at that position.
                //
                // Note that we don't update curOffset after we've created/inserted
                // the mozBR since we never want the selection to be placed after it.

                res = CreateMozBR(curNode, curOffset, address_of(unused));
              }
            }
          }
          pos++;
        }
        else
        {
          res = mEditor->InsertTextImpl(subStr, address_of(curNode), &curOffset, doc);
        }
        if (NS_FAILED(res)) return res;
      }
    }
    else
    {
      char specialChars[] = {TAB, nsCRT::LF, 0};
      while (unicodeBuf && (pos != -1) && ((PRUint32)pos < tString.Length()))
      {
        PRInt32 oldPos = pos;
        PRInt32 subStrLen;
        pos = tString.FindCharInSet(specialChars, oldPos);
        
        if (pos != -1) 
        {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (subStrLen == 0)
            subStrLen = 1;
        }
        else
        {
          subStrLen = tString.Length() - oldPos;
          pos = tString.Length();
        }

        nsDependentSubstring subStr(tString, oldPos, subStrLen);
        
        // is it a tab?
        if (subStr.EqualsLiteral("\t"))
        {
          res = mEditor->InsertTextImpl(NS_LITERAL_STRING("    "), address_of(curNode), &curOffset, doc);
          pos++;
        }
        // is it a return?
        else if (subStr.EqualsLiteral(LFSTR))
        {
          res = mEditor->CreateBRImpl(address_of(curNode), &curOffset, address_of(unused), nsIEditor::eNone);
          pos++;
        }
        else
        {
          res = mEditor->InsertTextImpl(subStr, address_of(curNode), &curOffset, doc);
        }
        if (NS_FAILED(res)) return res;
      }
    }
    outString->Assign(tString);

    if (curNode) 
      aSelection->Collapse(curNode, curOffset);
  }
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
  if (nsIPlaintextEditor::eEditorPlaintextMask & mFlags) {
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
  if (nsIPlaintextEditor::eEditorPlaintextMask & mFlags) {
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

  if (mFlags & nsIPlaintextEditor::eEditorPasswordMask)
  {
    // manage the password buffer
    PRInt32 start, end;
    mEditor->GetTextSelectionOffsets(aSelection, start, end);
    if (end==start)
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
    res = mEditor->GetStartNodeAndOffset(aSelection, address_of(startNode), &startOffset);
    if (NS_FAILED(res)) return res;
    if (!startNode) return NS_ERROR_FAILURE;
    
    PRBool bCollapsed;
    res = aSelection->GetIsCollapsed(&bCollapsed);
    if (NS_FAILED(res)) return res;
  
    if (bCollapsed)
    {
      // Test for distance between caret and text that will be deleted
      res = CheckBidiLevelForDeletion(startNode, startOffset, aCollapsedAction, aCancel);
      if (NS_FAILED(res)) return res;
      if (*aCancel) return NS_OK;

      nsCOMPtr<nsIDOMText> textNode;
      PRUint32 strLength;
      
      // destroy any empty text nodes in our path
      if (mEditor->IsTextNode(startNode))
      {
        textNode = do_QueryInterface(startNode);
        res = textNode->GetLength(&strLength);
        if (NS_FAILED(res)) return res;
        // if it has a length and we aren't at the edge, we are done
        if (strLength && !( ((aCollapsedAction == nsIEditor::ePrevious) && startOffset) ||
                            ((aCollapsedAction == nsIEditor::eNext) && startOffset==strLength) ) )
          return NS_OK;
        
        // remember where we are
        nsCOMPtr<nsIDOMNode> selNode = startNode;
        res = nsEditor::GetNodeLocation(selNode, address_of(startNode), &startOffset);
        if (NS_FAILED(res)) return res;

        // delete this text node if empty
        if (!strLength)
        {
          // delete empty text node
          res = mEditor->DeleteNode(selNode);
          if (NS_FAILED(res)) return res;
        }
        else
        {
          // if text node isn't empty, but we are at end of it, remeber that we are after it
          if (aCollapsedAction == nsIEditor::eNext)
            startOffset++;
        }
      }

      // find next node (we know we are in container here)
      nsCOMPtr<nsIContent> child, content(do_QueryInterface(startNode));
      if (!content) return NS_ERROR_NULL_POINTER;
      if (aCollapsedAction == nsIEditor::ePrevious)
        --startOffset;
      child = content->GetChildAt(startOffset);

      nsCOMPtr<nsIDOMNode> nextNode = do_QueryInterface(child);
      
      // scan for next node, deleting empty text nodes on way
      while (nextNode && mEditor->IsTextNode(nextNode))
      {
        textNode = do_QueryInterface(nextNode);
        if (!textNode) break;// found a br, stop there

        res = textNode->GetLength(&strLength);
        if (NS_FAILED(res)) return res;
        if (strLength) break;  // found a non-empty text node
        
        // delete empty text node
        res = mEditor->DeleteNode(nextNode);
        if (NS_FAILED(res)) return res;
        
        // find next node
        if (aCollapsedAction == nsIEditor::ePrevious)
          --startOffset;
          // don't need to increment startOffset for nsIEditor::eNext
        child = content->GetChildAt(startOffset);

        nextNode = do_QueryInterface(child);
      }
      // fix for bugzilla #125161: if we are about to forward delete a <BR>,
      // make sure it is not last node in editfield.  If it is, cancel deletion.
      if (nextNode && (aCollapsedAction == nsIEditor::eNext) && nsTextEditUtils::IsBreak(nextNode))
      {
        if (!mBody) return NS_ERROR_NULL_POINTER;
        nsCOMPtr<nsIDOMNode> lastChild;
        res = mBody->GetLastChild(getter_AddRefs(lastChild));
        if (lastChild == nextNode)
        {
          *aCancel = PR_TRUE;
          return NS_OK;
        }
      }
    }
  }

  return res;
}

nsresult
nsTextEditRules::DidDeleteSelection(nsISelection *aSelection, 
                                    nsIEditor::EDirection aCollapsedAction, 
                                    nsresult aResult)
{
  nsCOMPtr<nsIDOMNode> startNode;
  PRInt32 startOffset;
  nsresult res = mEditor->GetStartNodeAndOffset(aSelection, address_of(startNode), &startOffset);
  if (NS_FAILED(res)) return res;
  if (!startNode) return NS_ERROR_FAILURE;
  
  // delete empty text nodes at selection
  if (mEditor->IsTextNode(startNode))
  {
    nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(startNode);
    PRUint32 strLength;
    res = textNode->GetLength(&strLength);
    if (NS_FAILED(res)) return res;
    
    // are we in an empty text node?
    if (!strLength)
    {
      res = mEditor->DeleteNode(startNode);
      if (NS_FAILED(res)) return res;
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
      mBogusNode = do_QueryInterface(nsnull);
    }
    else
    {
      nsCOMPtr<nsIDOMElement> theBody;
      res = mEditor->GetRootElement(getter_AddRefs(theBody));
      if (NS_FAILED(res)) return res;
      if (!theBody) return NS_ERROR_FAILURE;
      nsCOMPtr<nsIDOMNode> node = mEditor->GetLeftmostChild(theBody);
      if (node && mEditor->IsMozEditorBogusNode(node))
        mBogusNode = do_QueryInterface(node);
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
      mBogusNode = do_QueryInterface(nsnull);
    }
    else
    {
      nsCOMPtr<nsIDOMElement> theBody;
      res = mEditor->GetRootElement(getter_AddRefs(theBody));
      if (NS_FAILED(res)) return res;
      if (!theBody) return NS_ERROR_FAILURE;
      
      nsCOMPtr<nsIDOMNodeList> nodeList;
      res = theBody->GetElementsByTagName(NS_LITERAL_STRING("div"), getter_AddRefs(nodeList));
      if (NS_FAILED(res)) return res;
      if (nodeList)
      {
        PRUint32 len;
        nodeList->GetLength(&len);
        
        if (len != 1) return NS_OK;  // only in the case of one div could there be the bogus node
        nsCOMPtr<nsIDOMNode>node;
        nodeList->Item(0, getter_AddRefs(node));
        if (!node) return NS_ERROR_NULL_POINTER;
        if (mEditor->IsMozEditorBogusNode(node))
          mBogusNode = do_QueryInterface(node);
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
    if (mFlags & nsIPlaintextEditor::eEditorPasswordMask)
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
  if (!aRange) return NS_ERROR_NULL_POINTER;
  
  // convert any newlines in editable, preformatted text nodes 
  // into normal breaks.  this is because layout wont give us a place 
  // to put the cursor on empty lines otherwise.

  nsresult res;
  nsCOMPtr<nsIContentIterator> iter =
       do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &res);
  if (NS_FAILED(res)) return res;

  res = iter->Init(aRange);
  if (NS_FAILED(res)) return res;
  
  nsCOMArray<nsIDOMCharacterData> arrayOfNodes;
  
  // gather up a list of editable preformatted text nodes
  while (!iter->IsDone())
  {
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(iter->GetCurrentNode());
    if (!node)
      return NS_ERROR_FAILURE;

    if (mEditor->IsTextNode(node) && mEditor->IsEditable(node))
    {
      PRBool isPRE;
      res = mEditor->IsPreformatted(node, &isPRE);
      if (NS_FAILED(res)) return res;
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
      EditTxn *txn;
      // note 1: we are not telling edit listeners about these because they don't care
      // note 2: we are not wrapping these in a placeholder because we know they already are,
      //         or, failing that, undo is disabled
      res = mEditor->CreateTxnForDeleteText(textNode, offset, 1, (DeleteTextTxn**)&txn);
      if (NS_FAILED(res))  return res; 
      if (!txn)  return NS_ERROR_OUT_OF_MEMORY;
      res = mEditor->DoTransaction(txn); 
      if (NS_FAILED(res))  return res; 
      // The transaction system (if any) has taken ownwership of txn
      NS_IF_RELEASE(txn);
      
      // insert a break
      res = mEditor->CreateBR(textNode, offset, address_of(brNode));
      if (NS_FAILED(res)) return res;
    } while (1);  // break used to exit while loop
  }
  return res;
}

nsresult
nsTextEditRules::CreateTrailingBRIfNeeded()
{
  // but only if we aren't a single line edit field
  if (mFlags & nsIPlaintextEditor::eEditorSingleLineMask)
    return NS_OK;
  if (!mBody) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> lastChild;
  nsresult res = mBody->GetLastChild(getter_AddRefs(lastChild));
  // assuming CreateBogusNodeIfNeeded() has been called first
  if (NS_FAILED(res)) return res;  
  if (!lastChild) return NS_ERROR_NULL_POINTER;

  if (!nsTextEditUtils::IsBreak(lastChild))
  {
    nsAutoTxnsConserveSelection dontSpazMySelection(mEditor);
    PRUint32 rootLen;
    res = mEditor->GetLengthOfDOMNode(mBody, rootLen);
    if (NS_FAILED(res)) return res; 
    nsCOMPtr<nsIDOMNode> unused;
    res = CreateMozBR(mBody, rootLen, address_of(unused));
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
  
  if (!mBody) return NS_ERROR_NULL_POINTER;

  // now we've got the body tag.
  // iterate the body tag, looking for editable content
  // if no editable content is found, insert the bogus node
  PRBool needsBogusContent=PR_TRUE;
  nsCOMPtr<nsIDOMNode>bodyChild;
  nsresult res = mBody->GetFirstChild(getter_AddRefs(bodyChild));        
  while ((NS_SUCCEEDED(res)) && bodyChild)
  { 
    if (mEditor->IsMozEditorBogusNode(bodyChild) || mEditor->IsEditable(bodyChild))
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
    if (NS_FAILED(res)) return res;
    nsCOMPtr<nsIDOMElement>brElement = do_QueryInterface(newContent);

    // set mBogusNode to be the newly created <br>
    mBogusNode = do_QueryInterface(brElement);
    if (!mBogusNode) return NS_ERROR_NULL_POINTER;

    // give it a special attribute
    brElement->SetAttribute( kMOZEditorBogusNodeAttr,
                             kMOZEditorBogusNodeValue );
    
    // put the node in the document
    res = mEditor->InsertNode(mBogusNode, mBody, 0);
    if (NS_FAILED(res)) return res;

    // set selection
    aSelection->Collapse(mBody, 0);
  }
  return res;
}


nsresult
nsTextEditRules::TruncateInsertionIfNeeded(nsISelection *aSelection, 
                                           const nsAString  *aInString,
                                           nsAString  *aOutString,
                                           PRInt32          aMaxLength)
{
  if (!aSelection || !aInString || !aOutString) {return NS_ERROR_NULL_POINTER;}
  
  nsresult res = NS_OK;
  *aOutString = *aInString;
  
  if ((-1 != aMaxLength) && (mFlags & nsIPlaintextEditor::eEditorPlaintextMask)
      && !mEditor->IsIMEComposing() )
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
    res = mEditor->GetTextSelectionOffsets(aSelection, start, end);
    if (NS_FAILED(res)) { return res; }
    PRInt32 selectionLength = end-start;
    if (selectionLength<0) { selectionLength *= (-1); }
    PRInt32 oldCompStrLength;
    res = mEditor->GetIMEBufferLength(&oldCompStrLength);
    if (NS_FAILED(res)) { return res; }
    PRInt32 resultingDocLength = docLength - selectionLength - oldCompStrLength;
    if (resultingDocLength >= aMaxLength) 
    {
      aOutString->Truncate();
      return res;
    }
    else
    {
      PRInt32 inCount = aOutString->Length();
      if ((inCount+resultingDocLength) > aMaxLength)
      {
        aOutString->Truncate(aMaxLength-resultingDocLength);
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
nsTextEditRules::RemoveIMETextFromPWBuf(PRInt32 &aStart, nsAString *aIMEString)
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

nsresult
nsTextEditRules::EchoInsertionToPWBuff(PRInt32 aStart, PRInt32 aEnd, nsAString *aOutString)
{
  if (!aOutString) {return NS_ERROR_NULL_POINTER;}

  // manage the password buffer
  mPasswordText.Insert(*aOutString, aStart);

  // change the output to '*' only
  PRInt32 length = aOutString->Length();
  PRInt32 i;
  aOutString->Truncate();
  for (i=0; i<length; i++)
  {
    aOutString->Append(PRUnichar('*'));
  }

  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// CreateMozBR: put a BR node with moz attribute at {aNode, aOffset}
//                       
nsresult 
nsTextEditRules::CreateMozBR(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outBRNode)
{
  if (!inParent || !outBRNode) return NS_ERROR_NULL_POINTER;

  nsresult res = mEditor->CreateBR(inParent, inOffset, outBRNode);
  if (NS_FAILED(res)) return res;

  // give it special moz attr
  nsCOMPtr<nsIDOMElement> brElem = do_QueryInterface(*outBRNode);
  if (brElem)
  {
    res = mEditor->SetAttribute(brElem, NS_LITERAL_STRING("type"), NS_LITERAL_STRING("_moz"));
    if (NS_FAILED(res)) return res;
  }
  return res;
}
