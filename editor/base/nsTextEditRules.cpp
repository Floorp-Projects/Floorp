/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsTextEditRules.h"

#include "nsEditor.h"
#include "nsHTMLEditUtils.h"

#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMSelection.h"
#include "nsIDOMRange.h"
#include "nsIDOMCharacterData.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIEnumerator.h"
#include "nsLayoutCID.h"
#include "nsIEditProperty.h"
#include "nsEditorUtils.h"
#include "EditTxn.h"
#include "TypeInState.h"
#include "nsIPref.h"

static NS_DEFINE_CID(kContentIteratorCID,   NS_CONTENTITERATOR_CID);
static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

#define CANCEL_OPERATION_IF_READONLY_OR_DISABLED \
  if ((mFlags & nsIHTMLEditor::eEditorReadonlyMask) || (mFlags & nsIHTMLEditor::eEditorDisabledMask)) \
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
, mBogusNode(nsnull)
, mBody(nsnull)
, mFlags(0) // initialized to 0 ("no flags set").  Real initial value is given in Init()
, mActionNesting(0)
, mLockRulesSniffing(PR_FALSE)
, mTheAction(0)
{
  NS_INIT_REFCNT();
}

nsTextEditRules::~nsTextEditRules()
{
   // do NOT delete mEditor here.  We do not hold a ref count to mEditor.  mEditor owns our lifespan.
}

/********************************************************
 *  XPCOM Cruft
 ********************************************************/

NS_IMPL_ADDREF(nsTextEditRules)
NS_IMPL_RELEASE(nsTextEditRules)
NS_IMPL_QUERY_INTERFACE1(nsTextEditRules, nsIEditRules)


/********************************************************
 *  Public methods 
 ********************************************************/

NS_IMETHODIMP
nsTextEditRules::Init(nsHTMLEditor *aEditor, PRUint32 aFlags)
{
  if (!aEditor) { return NS_ERROR_NULL_POINTER; }

  mEditor = aEditor;  // we hold a non-refcounted reference back to our editor
  // call SetFlags only aftet mEditor has been initialized!
  SetFlags(aFlags);
  nsCOMPtr<nsIDOMSelection> selection;
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

  // create a range that is the entire body contents
  nsCOMPtr<nsIDOMRange> wholeDoc;
  res = nsComponentManager::CreateInstance(kRangeCID, nsnull, NS_GET_IID(nsIDOMRange), 
                                           getter_AddRefs(wholeDoc));
  if (NS_FAILED(res)) return res;
  wholeDoc->SetStart(mBody,0);
  nsCOMPtr<nsIDOMNodeList> list;
  res = mBody->GetChildNodes(getter_AddRefs(list));
  if (NS_FAILED(res) || !list) return res?res:NS_ERROR_FAILURE;
  PRUint32 listCount;
  res = list->GetLength(&listCount);
  if (NS_FAILED(res)) return res;

  res = wholeDoc->SetEnd(mBody,listCount);
  if (NS_FAILED(res)) return res;

  // replace newlines in that range with breaks
  res = ReplaceNewlines(wholeDoc);
  return res;
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
  if (aFlags & nsIHTMLEditor::eEditorPlaintextMask)
  {
    if (!(mFlags & nsIHTMLEditor::eEditorPlaintextMask))
    {
      // Call the editor's SetBodyWrapWidth(), which will
      // set the styles appropriately for plaintext:
      mEditor->SetBodyWrapWidth(72);
    }
  }
  
  mFlags = aFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsTextEditRules::BeforeEdit(PRInt32 action, nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) return NS_OK;
  
  nsAutoLockRulesSniffing lockIt(this);
  
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
    nsCOMPtr<nsIDOMSelection>selection;
    res = mEditor->GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
  
    // detect empty doc
    res = CreateBogusNodeIfNeeded(selection);
    if (NS_FAILED(res)) return res;
    
    // create moz-br and adjust selection if needed
    res = AdjustSelection(selection, aDirection);
  }
  return res;
}


NS_IMETHODIMP 
nsTextEditRules::WillDoAction(nsIDOMSelection *aSelection, 
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
nsTextEditRules::DidDoAction(nsIDOMSelection *aSelection,
                             nsRulesInfo *aInfo, nsresult aResult)
{
  // dont let any txns in here move the selection around behind our back.
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
nsTextEditRules::WillInsert(nsIDOMSelection *aSelection, PRBool *aCancel)
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

  // this next only works for collapsed selections right now,
  // because selection is a pain to work with when not collapsed.
  // (no good way to extend start or end of selection)
  PRBool bCollapsed;
  nsresult res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed) return NS_OK;

  // if we are after a mozBR in the same block, then move selection
  // to be before it
  nsCOMPtr<nsIDOMNode> selNode, priorNode;
  PRInt32 selOffset;
  // get the (collapsed) selection location
  res = mEditor->GetStartNodeAndOffset(aSelection, &selNode, &selOffset);
  if (NS_FAILED(res)) return res;
  // get prior node
  res = mEditor->GetPriorHTMLNode(selNode, selOffset, &priorNode);
  if (NS_SUCCEEDED(res) && priorNode && nsHTMLEditUtils::IsMozBR(priorNode))
  {
    nsCOMPtr<nsIDOMNode> block1, block2;
    if (mEditor->IsBlockNode(selNode)) block1 = selNode;
    else block1 = mEditor->GetBlockNodeParent(selNode);
    block2 = mEditor->GetBlockNodeParent(priorNode);
  
    if (block1 != block2) return NS_OK; 
  
    // if we are here then the selection is right after a mozBR
    // that is in the same block as the selection.  We need to move
    // the selection start to be before the mozBR.
    res = nsEditor::GetNodeLocation(priorNode, &selNode, &selOffset);
    if (NS_FAILED(res)) return res;
    res = aSelection->Collapse(selNode,selOffset);
    if (NS_FAILED(res)) return res;
  }

  return res;
}

nsresult
nsTextEditRules::DidInsert(nsIDOMSelection *aSelection, nsresult aResult)
{
  return NS_OK;
}

nsresult 
nsTextEditRules::GetTopEnclosingPre(nsIDOMNode *aNode,
                                    nsIDOMNode** aOutPreNode)
{
  // check parms
  if (!aNode || !aOutPreNode) 
    return NS_ERROR_NULL_POINTER;
  *aOutPreNode = 0;
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> node, parentNode;
  node = do_QueryInterface(aNode);
  
  while (node)
  {
    nsAutoString tag;
    nsEditor::GetTagString(node, tag);
    if (tag.EqualsWithConversion("pre", PR_TRUE))
      *aOutPreNode = node;
    else if (tag.EqualsWithConversion("body", PR_TRUE))
      break;
    
    res = node->GetParentNode(getter_AddRefs(parentNode));
    if (NS_FAILED(res)) return res;
    node = parentNode;
  }

  NS_IF_ADDREF(*aOutPreNode);
  return res;
}

nsresult
nsTextEditRules::WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  *aHandled = PR_FALSE;
  if (mFlags & nsIHTMLEditor::eEditorSingleLineMask) {
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
  
    // Mail rule: split any <pre> tags in the way,
    // since they're probably quoted text.
    // For now, do this for all plaintext since mail is our main customer
    // and we don't currently set eEditorMailMask for plaintext mail.
    if (mTheAction != nsHTMLEditor::kOpInsertQuotation) // && mFlags & nsIHTMLEditor::eEditorMailMask)
    {
      nsCOMPtr<nsIDOMNode> preNode, selNode;
      PRInt32 selOffset, newOffset;
      res = mEditor->GetStartNodeAndOffset(aSelection, &selNode, &selOffset);
      if (NS_FAILED(res)) return res;

      // If any of the following fail, then just proceed with the
      // normal break insertion without worrying about the error
      res = GetTopEnclosingPre(selNode, getter_AddRefs(preNode));
      if (NS_SUCCEEDED(res) && preNode)
      {
        // Only split quote nodes: see if it has the attribute _moz_quote
        nsCOMPtr<nsIDOMElement> preElement (do_QueryInterface(preNode));
        if (preElement)
        {
          nsString mozQuote; mozQuote.AssignWithConversion("_moz_quote");
          nsString mozQuoteVal;
          PRBool isMozQuote = PR_FALSE;
          if (NS_SUCCEEDED(mEditor->GetAttributeValue(preElement, mozQuote,
                                                      mozQuoteVal, isMozQuote))
              && isMozQuote)
          {
            printf("It's a moz quote -- splitting\n");
            nsCOMPtr<nsIDOMNode> outLeftNode;
            nsCOMPtr<nsIDOMNode> outRightNode;
            res = mEditor->SplitNodeDeep(preNode, selNode, selOffset, &newOffset, PR_TRUE, &outLeftNode, &outRightNode);
            if (NS_FAILED(res)) return res;
            PRBool bIsEmptyNode;
            
            // rememeber parent of selNode now, since we might delete selNode below
            res = preNode->GetParentNode(getter_AddRefs(selNode));
            if (NS_FAILED(res)) return res;
            
            if (outLeftNode)
            {
              res = mEditor->IsEmptyNode(outLeftNode, &bIsEmptyNode, PR_TRUE, PR_FALSE);
              if (NS_FAILED(res)) return res;
              if (bIsEmptyNode) mEditor->DeleteNode(outLeftNode);
            }
            if (outRightNode)
            {
              // HACK alert: consume a br if there is one at front of node
              nsCOMPtr<nsIDOMNode> firstNode;
              res =  mEditor->GetFirstEditableNode(outRightNode, &firstNode);
              if (firstNode &&  nsHTMLEditUtils::IsBreak(firstNode))
              {
                mEditor->DeleteNode(firstNode);
              }
                
              res = mEditor->IsEmptyNode(outRightNode, &bIsEmptyNode, PR_TRUE, PR_FALSE);
              if (NS_FAILED(res)) return res;
              if (bIsEmptyNode) mEditor->DeleteNode(outRightNode);
            }
            nsCOMPtr<nsIDOMNode> brNode;
            // last ePrevious param causes selection to be set before the break
            res = mEditor->CreateBR(selNode, newOffset, &brNode, nsIEditor::ePrevious);
            *aHandled = PR_TRUE;
          }
        }
      }
    }  
  }
  return NS_OK;
}

nsresult
nsTextEditRules::DidInsertBreak(nsIDOMSelection *aSelection, nsresult aResult)
{
  // we only need to execute the stuff below if we are a plaintext editor.
  // html editors have a different mechanism for putting in mozBR's
  // (because there are a bunch more placesyou have to worry about it in html) 
  if (!nsIHTMLEditor::eEditorPlaintextMask & mFlags) return NS_OK;

  // if we are at the end of the document, we need to insert 
  // a special mozBR following the normal br, and then set the
  // selection to stick to the mozBR.
  PRInt32 selOffset;
  nsCOMPtr<nsIDOMNode> nearNode, selNode;
  nsresult res;
  res = mEditor->GetStartNodeAndOffset(aSelection, &selNode, &selOffset);
  if (NS_FAILED(res)) return res;
  res = mEditor->GetPriorHTMLNode(selNode, selOffset, &nearNode);
  if (NS_FAILED(res)) return res;
  if (nearNode && nsHTMLEditUtils::IsBreak(nearNode) && !nsHTMLEditUtils::IsMozBR(nearNode))
  {
    PRBool bIsLast;
    res = mEditor->IsLastEditableChild(nearNode, &bIsLast);
    if (NS_FAILED(res)) return res;
    if (bIsLast)
    {
      // need to insert special moz BR. Why?  Because if we don't
      // the user will see no new line for the break.  Also, things
      // like table cells won't grow in height.
      nsCOMPtr<nsIDOMNode> brNode;
      res = CreateMozBR(selNode, selOffset, &brNode);
      if (NS_FAILED(res)) return res;
      res = nsEditor::GetNodeLocation(brNode, &selNode, &selOffset);
      if (NS_FAILED(res)) return res;
      aSelection->SetHint(PR_TRUE);
      res = aSelection->Collapse(selNode,selOffset);
      if (NS_FAILED(res)) return res;
    }
  }
//  mEditor->DumpContentTree();
  return res;
}

nsresult
nsTextEditRules::WillInsertText(PRInt32          aAction,
                                nsIDOMSelection *aSelection, 
                                PRBool          *aCancel,
                                PRBool          *aHandled,
                                const nsString  *inString,
                                nsString        *outString,
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
  nsresult res;
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;
  PRInt32 start=0;  PRInt32 end=0;  

  // handle docs with a max length
  // NOTE, this function copies inString into outString for us.
  res = TruncateInsertionIfNeeded(aSelection, inString, outString, aMaxLength);
  if (NS_FAILED(res)) return res;
  
  // handle password field docs
  if (mFlags & nsIHTMLEditor::eEditorPasswordMask)
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
  if (mFlags & nsIHTMLEditor::eEditorPasswordMask)
  {
    res = EchoInsertionToPWBuff(start, end, outString);
    if (NS_FAILED(res)) return res;
  }

  // People have lots of different ideas about what text fields
  // should do with multiline pastes.  See bugs 21032, 23485, 23485, 50935.
  // The four possible options are:
  // 0. paste newlines intact
  // 1. paste up to the first newline
  // 2. replace newlines with spaces
  // 3. strip newlines
  // So find out what we're expected to do:
  enum {
    ePasteIntact = 0, ePasteFirstLine = 1,
    eReplaceWithSpaces = 2, eStripNewlines = 3
  };
  PRInt32 singleLineNewlineBehavior = 1;
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_PROGID, &rv);
  if (NS_SUCCEEDED(rv) && prefs)
    rv = prefs->GetIntPref("editor.singleLine.pasteNewlines",
                           &singleLineNewlineBehavior);

  if (nsIHTMLEditor::eEditorSingleLineMask & mFlags)
  {
    if (singleLineNewlineBehavior == eReplaceWithSpaces)
      outString->ReplaceChar(CRLF, ' ');
    else if (singleLineNewlineBehavior == eStripNewlines)
      outString->StripChars(CRLF);
    else if (singleLineNewlineBehavior == ePasteFirstLine)
    {
      PRInt32 firstCRLF = outString->FindCharInSet(CRLF);
      if (firstCRLF > 0)
      outString->Truncate(firstCRLF);
    }
    else // even if we're pasting newlines, don't paste leading/trailing ones
      outString->Trim(CRLF, PR_TRUE, PR_TRUE);
  }

  // get the (collapsed) selection location
  res = mEditor->GetStartNodeAndOffset(aSelection, &selNode, &selOffset);
  if (NS_FAILED(res)) return res;

  // dont put text in places that cant have it
  nsAutoString textTag; textTag.AssignWithConversion("__moz_text");
  if (!mEditor->IsTextNode(selNode) && !mEditor->CanContainTag(selNode, textTag))
    return NS_ERROR_FAILURE;

  // we need to get the doc
  nsCOMPtr<nsIDOMDocument>doc;
  res = mEditor->GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(res)) return res;
  if (!doc) return NS_ERROR_NULL_POINTER;
    
  if (aAction == kInsertTextIME) 
  { 
    res = mEditor->InsertTextImpl(*outString, &selNode, &selOffset, doc);
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
    
    // dont spaz my selection in subtransactions
    nsAutoTxnsConserveSelection dontSpazMySelection(mEditor);
    nsSubsumeStr subStr;
    const PRUnichar *unicodeBuf = outString->GetUnicode();
    nsCOMPtr<nsIDOMNode> unused;
    PRInt32 pos = 0;
        
    // for efficiency, break out the pre case seperately.  This is because
    // its a lot cheaper to search the input string for only newlines than
    // it is to search for both tabs and newlines.
    if (isPRE)
    {
      char newlineChar = '\n';
      while (unicodeBuf && (pos != -1) && ((PRUint32)pos < outString->Length()))
      {
        PRInt32 oldPos = pos;
        PRInt32 subStrLen;
        pos = outString->FindChar(newlineChar, PR_FALSE, oldPos);
        
        if (pos != -1) 
        {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (subStrLen == 0)
            subStrLen = 1;
        }
        else
        {
          subStrLen = outString->Length() - oldPos;
          pos = outString->Length();
        }

        subStr.Subsume((PRUnichar*)&unicodeBuf[oldPos], PR_FALSE, subStrLen);
        
        // is it a return?
        if (subStr.EqualsWithConversion("\n"))
        {
          if (nsIHTMLEditor::eEditorSingleLineMask & mFlags)
          {
            NS_ASSERTION((singleLineNewlineBehavior == ePasteIntact),
                  "Newline improperly getting into single-line edit field!");
            res = mEditor->InsertTextImpl(subStr, &curNode, &curOffset, doc);
          }
          else
            res = mEditor->CreateBRImpl(&curNode, &curOffset, &unused, nsIEditor::eNone);
          pos++;
        }
        else
        {
          res = mEditor->InsertTextImpl(subStr, &curNode, &curOffset, doc);
        }
        if (NS_FAILED(res)) return res;
      }
    }
    else
    {
      char specialChars[] = {'\t','\n',0};
      nsAutoString tabString; tabString.AssignWithConversion("    ");
      while (unicodeBuf && (pos != -1) && ((PRUint32)pos < outString->Length()))
      {
        PRInt32 oldPos = pos;
        PRInt32 subStrLen;
        pos = outString->FindCharInSet(specialChars, oldPos);
        
        if (pos != -1) 
        {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (subStrLen == 0)
            subStrLen = 1;
        }
        else
        {
          subStrLen = outString->Length() - oldPos;
          pos = outString->Length();
        }

        subStr.Subsume((PRUnichar*)&unicodeBuf[oldPos], PR_FALSE, subStrLen);
        
        // is it a tab?
        if (subStr.EqualsWithConversion("\t"))
        {
          res = mEditor->InsertTextImpl(tabString, &curNode, &curOffset, doc);
          pos++;
        }
        // is it a return?
        else if (subStr.EqualsWithConversion("\n"))
        {
          res = mEditor->CreateBRImpl(&curNode, &curOffset, &unused, nsIEditor::eNone);
          pos++;
        }
        else
        {
          res = mEditor->InsertTextImpl(subStr, &curNode, &curOffset, doc);
        }
        if (NS_FAILED(res)) return res;
      }
    }
    if (curNode) aSelection->Collapse(curNode, curOffset);
  }
  return res;
}

nsresult
nsTextEditRules::DidInsertText(nsIDOMSelection *aSelection, 
                               nsresult aResult)
{
  return DidInsert(aSelection, aResult);
}



nsresult
nsTextEditRules::WillSetTextProperty(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) 
    { return NS_ERROR_NULL_POINTER; }
  nsresult res = NS_OK;

  // XXX: should probably return a success value other than NS_OK that means "not allowed"
  if (nsIHTMLEditor::eEditorPlaintextMask & mFlags) {
    *aCancel = PR_TRUE;
  }
  return res;
}

nsresult
nsTextEditRules::DidSetTextProperty(nsIDOMSelection *aSelection, nsresult aResult)
{
  return NS_OK;
}

nsresult
nsTextEditRules::WillRemoveTextProperty(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) 
    { return NS_ERROR_NULL_POINTER; }
  nsresult res = NS_OK;

  // XXX: should probably return a success value other than NS_OK that means "not allowed"
  if (nsIHTMLEditor::eEditorPlaintextMask & mFlags) {
    *aCancel = PR_TRUE;
  }
  return res;
}

nsresult
nsTextEditRules::DidRemoveTextProperty(nsIDOMSelection *aSelection, nsresult aResult)
{
  return NS_OK;
}

nsresult
nsTextEditRules::WillDeleteSelection(nsIDOMSelection *aSelection, 
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
  if (mFlags & nsIHTMLEditor::eEditorPasswordMask)
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

#ifdef DEBUG_buster
    char *password = mPasswordText.ToNewCString();
    printf("mPasswordText is %s\n", password);
    nsCRT::free(password);
#endif
  }
  return NS_OK;
}

// if the document is empty, insert a bogus text node with a &nbsp;
// if we ended up with consecutive text nodes, merge them
nsresult
nsTextEditRules::DidDeleteSelection(nsIDOMSelection *aSelection, 
                                    nsIEditor::EDirection aCollapsedAction, 
                                    nsresult aResult)
{
  nsresult res = aResult;  // if aResult is an error, we just return it
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  PRBool isCollapsed;
  aSelection->GetIsCollapsed(&isCollapsed);
  NS_ASSERTION(PR_TRUE==isCollapsed, "selection not collapsed after delete selection.");
  if (NS_SUCCEEDED(res)) // only do this work if DeleteSelection completed successfully
  {
    // if we don't have an empty document, check the selection to see if any collapsing is necessary
    if (!mBogusNode)
    {
      // get the node that contains the selection point
      nsCOMPtr<nsIDOMNode>anchor;
      PRInt32 offset;
      res = aSelection->GetAnchorNode(getter_AddRefs(anchor));
      if (NS_FAILED(res)) return res;
      if (!anchor) return NS_ERROR_NULL_POINTER;
      res = aSelection->GetAnchorOffset(&offset);
      if (NS_FAILED(res)) return res;
      // selectedNode is either the anchor itself, 
      // or if anchor has children, it's the referenced child node
      // XXX ----------------------------------------------- XXX
      //  I believe this s wrong.  Assuming anchor and focus
      // alwas correspond to selection endpoints, it is possible 
      // for the first node after the selectin start point to not
      // be selected.  As an example consider a selection that
      // starts right before a <ul>, and ends after the first character
      // in the text of the first list item.  Really all that is
      // selected is one letter of text, not the <ul>
      nsCOMPtr<nsIDOMNode> selectedNode = do_QueryInterface(anchor);
      PRBool hasChildren=PR_FALSE;
      anchor->HasChildNodes(&hasChildren);
      if (PR_TRUE==hasChildren)
      { // if anchor has children, set selectedNode to the child pointed at        
        nsCOMPtr<nsIDOMNodeList> anchorChildren;
        res = anchor->GetChildNodes(getter_AddRefs(anchorChildren));
        if ((NS_SUCCEEDED(res)) && anchorChildren) {              
          res = anchorChildren->Item(offset, getter_AddRefs(selectedNode));
        }
      }

      if ((NS_SUCCEEDED(res)) && selectedNode && !DeleteEmptyTextNode(selectedNode))
      {
        nsCOMPtr<nsIDOMCharacterData>selectedNodeAsText;
        selectedNodeAsText = do_QueryInterface(selectedNode);
        if (selectedNodeAsText && mEditor->IsEditable(selectedNode))
        {
          nsCOMPtr<nsIDOMNode> siblingNode;
          selectedNode->GetPreviousSibling(getter_AddRefs(siblingNode));
          if (siblingNode && !DeleteEmptyTextNode(siblingNode))
          {
            nsCOMPtr<nsIDOMCharacterData>siblingNodeAsText;
            siblingNodeAsText = do_QueryInterface(siblingNode);
            if (siblingNodeAsText && mEditor->IsEditable(siblingNode))
            {
              PRUint32 siblingLength; // the length of siblingNode before the join
              siblingNodeAsText->GetLength(&siblingLength);
              nsCOMPtr<nsIDOMNode> parentNode;
              res = selectedNode->GetParentNode(getter_AddRefs(parentNode));
              if (NS_FAILED(res)) return res;
              if (!parentNode) return NS_ERROR_NULL_POINTER;
              res = mEditor->JoinNodes(siblingNode, selectedNode, parentNode);
              // selectedNode will remain after the join, siblingNode is removed
            }
          }
          selectedNode->GetNextSibling(getter_AddRefs(siblingNode));
          if (siblingNode && !DeleteEmptyTextNode(siblingNode))
          {
            nsCOMPtr<nsIDOMCharacterData>siblingNodeAsText;
            siblingNodeAsText = do_QueryInterface(siblingNode);
            if (siblingNodeAsText && mEditor->IsEditable(siblingNode))
            {
              PRUint32 selectedNodeLength; // the length of siblingNode before the join
              selectedNodeAsText->GetLength(&selectedNodeLength);
              nsCOMPtr<nsIDOMNode> parentNode;
              res = selectedNode->GetParentNode(getter_AddRefs(parentNode));
              if (NS_FAILED(res)) return res;
              if (!parentNode) return NS_ERROR_NULL_POINTER;

              res = mEditor->JoinNodes(selectedNode, siblingNode, parentNode);
              if (NS_FAILED(res)) return res;
              // selectedNode will remain after the join, siblingNode is removed
            }
          }
        }
      }
    }
  }
  return res;
}

nsresult
nsTextEditRules::WillUndo(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  return NS_OK;
}

/* the idea here is to see if the magic empty node has suddenly reappeared as the res of the undo.
 * if it has, set our state so we remember it.
 * There is a tradeoff between doing here and at redo, or doing it everywhere else that might care.
 * Since undo and redo are relatively rare, it makes sense to take the (small) performance hit here.
 */
nsresult
nsTextEditRules:: DidUndo(nsIDOMSelection *aSelection, nsresult aResult)
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
      nsCOMPtr<nsIDOMNode> node;
      res = mEditor->GetRootElement(getter_AddRefs(theBody));
      if (NS_FAILED(res)) return res;
      if (!theBody) return NS_ERROR_FAILURE;
      res = mEditor->GetLeftmostChild(theBody,getter_AddRefs(node));
      if (NS_FAILED(res)) return res;
      if (!node) return NS_ERROR_NULL_POINTER;
      if (mEditor->IsMozEditorBogusNode(node))
        mBogusNode = do_QueryInterface(node);
    }
  }
  return res;
}

nsresult
nsTextEditRules::WillRedo(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  return NS_OK;
}

nsresult
nsTextEditRules::DidRedo(nsIDOMSelection *aSelection, nsresult aResult)
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
      
      nsAutoString tagName; tagName.AssignWithConversion("div");
      nsCOMPtr<nsIDOMNodeList> nodeList;
      res = theBody->GetElementsByTagName(tagName, getter_AddRefs(nodeList));
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
nsTextEditRules::WillOutputText(nsIDOMSelection *aSelection, 
                                const nsString  *aOutputFormat,
                                nsString *aOutString,                                
                                PRBool   *aCancel,
                                PRBool   *aHandled)
{
  // null selection ok
  if (!aOutString || !aOutputFormat || !aCancel || !aHandled) 
    { return NS_ERROR_NULL_POINTER; }

  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;

  if (PR_TRUE == aOutputFormat->EqualsWithConversion("text/plain"))
  { // only use these rules for plain text output
    if (mFlags & nsIHTMLEditor::eEditorPasswordMask)
    {
      *aOutString = mPasswordText;
      *aHandled = PR_TRUE;
    }
    else if (mBogusNode)
    { // this means there's no content, so output null string
      aOutString->SetLength(0);
      *aHandled = PR_TRUE;
    }
  }
  return NS_OK;
}

nsresult
nsTextEditRules::DidOutputText(nsIDOMSelection *aSelection, nsresult aResult)
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

  nsCOMPtr<nsIContentIterator> iter;
  nsCOMPtr<nsISupports> isupports;
  PRUint32 nodeCount,j;
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  
  // make an isupportsArray to hold a list of nodes
  nsresult res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
  if (NS_FAILED(res)) return res;

  // need an iterator
  res = nsComponentManager::CreateInstance(kContentIteratorCID,
                                        nsnull,
                                        NS_GET_IID(nsIContentIterator), 
                                        getter_AddRefs(iter));
  if (NS_FAILED(res)) return res;
  res = iter->Init(aRange);
  if (NS_FAILED(res)) return res;
  
  // gather up a list of editable preformatted text nodes
  while (NS_ENUMERATOR_FALSE == iter->IsDone())
  {
    nsCOMPtr<nsIDOMNode> node;
    nsCOMPtr<nsIContent> content;
    res = iter->CurrentNode(getter_AddRefs(content));
    if (NS_FAILED(res)) return res;
    node = do_QueryInterface(content);
    if (!node) return NS_ERROR_FAILURE;
    
    if (mEditor->IsTextNode(node) && mEditor->IsEditable(node))
    {
      PRBool isPRE;
      res = mEditor->IsPreformatted(node, &isPRE);
      if (NS_FAILED(res)) return res;
      if (isPRE)
      {
        isupports = do_QueryInterface(node);
        arrayOfNodes->AppendElement(isupports);
      }
    }
    res = iter->Next();
    if (NS_FAILED(res)) return res;
  }
  
  // replace newlines with breaks.  have to do this left to right,
  // since inserting the break can split the text node, and the
  // original node becomes the righthand node.
  char newlineChar[] = {'\n',0};
  res = arrayOfNodes->Count(&nodeCount);
  if (NS_FAILED(res)) return res;
  for (j = 0; j < nodeCount; j++)
  {
    isupports = (dont_AddRef)(arrayOfNodes->ElementAt(0));
    nsCOMPtr<nsIDOMNode> brNode, theNode( do_QueryInterface(isupports) );
    nsCOMPtr<nsIDOMCharacterData> textNode( do_QueryInterface(theNode) );
    arrayOfNodes->RemoveElementAt(0);
    // find the newline
    PRInt32 offset;
    nsAutoString tempString;
    do 
    {
      textNode->GetData(tempString);
      offset = tempString.FindCharInSet(newlineChar);
      if (offset == -1) break; // done with this node
      
      // delete the newline
      EditTxn *txn;
      // note 1: we are not telling edit listeners about these because they don't care
      // note 2: we are not wrapping these in a placeholder because we know they already are,
      //         or, failing that, undo is disabled
      res = mEditor->CreateTxnForDeleteText(textNode, offset, 1, (DeleteTextTxn**)&txn);
      if (NS_FAILED(res))  return res; 
      if (!txn)  return NS_ERROR_OUT_OF_MEMORY;
      res = mEditor->Do(txn); 
      if (NS_FAILED(res))  return res; 
      // The transaction system (if any) has taken ownwership of txn
      NS_IF_RELEASE(txn);
      
      // insert a break
      res = mEditor->CreateBR(textNode, offset, &brNode);
      if (NS_FAILED(res)) return res;
    } while (1);  // break used to exit while loop
  }
  return res;
}


nsresult
nsTextEditRules::CreateBogusNodeIfNeeded(nsIDOMSelection *aSelection)
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
    nsCOMPtr<nsIDOMDocument>  domDoc;
    res = mEditor->GetDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDOMElement>brElement;
    nsCOMPtr<nsIContent> newContent;
    
    nsString qualifiedTag;
    qualifiedTag.AssignWithConversion("br");

    res = mEditor->CreateHTMLContent(qualifiedTag, getter_AddRefs(newContent));
    brElement = do_QueryInterface(newContent);
    if (NS_FAILED(res)) return res;
    
    // set mBogusNode to be the newly created <br>
    mBogusNode = do_QueryInterface(brElement);
    if (!mBogusNode) return NS_ERROR_NULL_POINTER;

    // give it a special attribute
    brElement->SetAttribute( NS_ConvertASCIItoUCS2(nsEditor::kMOZEditorBogusNodeAttr),
                             NS_ConvertASCIItoUCS2(nsEditor::kMOZEditorBogusNodeValue) );
    
    // put the node in the document
    res = mEditor->InsertNode(mBogusNode,mBody,0);
    if (NS_FAILED(res)) return res;

    // set selection
    aSelection->Collapse(mBody,0);
  }
  return res;
}


nsresult
nsTextEditRules::TruncateInsertionIfNeeded(nsIDOMSelection *aSelection, 
                                           const nsString  *aInString,
                                           nsString        *aOutString,
                                           PRInt32          aMaxLength)
{
  if (!aSelection || !aInString || !aOutString) {return NS_ERROR_NULL_POINTER;}
  
  nsresult res = NS_OK;
  *aOutString = *aInString;
  
  if ((-1 != aMaxLength) && (mFlags & nsIHTMLEditor::eEditorPlaintextMask))
  {
    // Get the current text length.
    // Get the length of inString.
    // Get the length of the selection.
    //   If selection is collapsed, it is length 0.
    //   Subtract the length of the selection from the len(doc) 
    //   since we'll delete the selection on insert.
    //   This is resultingDocLength.
    // If (resultingDocLength) is at or over max, cancel the insert
    // If (resultingDocLength) + (length of input) > max, 
    //    set aOutString to subset of inString so length = max
    PRInt32 docLength;
    res = mEditor->GetDocumentLength(&docLength);
    if (NS_FAILED(res)) { return res; }
    PRInt32 start, end;
    res = mEditor->GetTextSelectionOffsets(aSelection, start, end);
    if (NS_FAILED(res)) { return res; }
    PRInt32 selectionLength = end-start;
    if (selectionLength<0) { selectionLength *= (-1); }
    PRInt32 resultingDocLength = docLength - selectionLength;
    if (resultingDocLength >= aMaxLength) 
    {
      aOutString->SetLength(0);
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
nsTextEditRules::EchoInsertionToPWBuff(PRInt32 aStart, PRInt32 aEnd, nsString *aOutString)
{
  if (!aOutString) {return NS_ERROR_NULL_POINTER;}

  // manage the password buffer
  mPasswordText.Insert(*aOutString, aStart);

  // change the output to '*' only
  PRInt32 length = aOutString->Length();
  PRInt32 i;
  aOutString->SetLength(0);
  for (i=0; i<length; i++)
    aOutString->AppendWithConversion('*');

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
    res = mEditor->SetAttribute(brElem, NS_ConvertASCIItoUCS2("type"), NS_ConvertASCIItoUCS2("_moz"));
    if (NS_FAILED(res)) return res;
  }
  return res;
}

PRBool
nsTextEditRules::DeleteEmptyTextNode(nsIDOMNode *aNode)
{
  if (aNode)
  {
    nsCOMPtr<nsIDOMCharacterData>nodeAsText;
    nodeAsText = do_QueryInterface(aNode);
    if (nodeAsText)
    {
      PRUint32 len;
      nodeAsText->GetLength(&len);
      if (!len) 
      {
        mEditor->DeleteNode(aNode);
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}


nsresult 
nsTextEditRules::AdjustSelection(nsIDOMSelection *aSelection, nsIEditor::EDirection aDirection)
{
  if (!aSelection) return NS_ERROR_NULL_POINTER;
  
  // if the selection isn't collapsed, do nothing.
  PRBool bCollapsed;
  nsresult res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed) return res;

  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode, temp;
  PRInt32 selOffset;
  res = mEditor->GetStartNodeAndOffset(aSelection, &selNode, &selOffset);
  if (NS_FAILED(res)) return res;
  temp = selNode;
  
  // are we in an editable node?
  while (!mEditor->IsEditable(selNode))
  {
    // scan up the tree until we find an editable place to be
    res = nsEditor::GetNodeLocation(temp, &selNode, &selOffset);
    if (NS_FAILED(res)) return res;
    if (!selNode) return NS_ERROR_FAILURE;
    temp = selNode;
  }
  
  // are we in a text node? 
  nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(selNode);
  if (textNode) 
    return NS_OK; // we LIKE it when we are in a text node.  that RULZ
  
  // do we need to insert a special mozBR?  We do if we are:
  // 1) after a block element AND
  // 2) at the end of the body OR before another block

  nsCOMPtr<nsIDOMNode> priorNode, nextNode;
  res = mEditor->GetPriorHTMLSibling(selNode, selOffset, &priorNode);
  if (NS_FAILED(res)) return res;
  res = mEditor->GetNextHTMLSibling(selNode, selOffset, &nextNode);
  if (NS_FAILED(res)) return res;
  
  // is priorNode a block?
  if (priorNode && mEditor->IsBlockNode(priorNode)) 
  {
    if (!nextNode || mEditor->IsBlockNode(nextNode))
    {
      nsCOMPtr<nsIDOMNode> brNode;
      res = CreateMozBR(selNode, selOffset, &brNode);
      if (NS_FAILED(res)) return res;
      res = nsEditor::GetNodeLocation(brNode, &selNode, &selOffset);
      if (NS_FAILED(res)) return res;
      // selection stays *before* moz-br, sticking to it
      aSelection->SetHint(PR_TRUE);
      res = aSelection->Collapse(selNode,selOffset);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}

