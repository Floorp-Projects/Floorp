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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsHTMLEditRules.h"

#include "nsEditor.h"
#include "nsHTMLEditUtils.h"
#include "nsHTMLEditor.h"

#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIDOMNode.h"
#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMSelection.h"
#include "nsIDOMRange.h"
#include "nsIDOMCharacterData.h"
#include "nsIEnumerator.h"
#include "nsIStyleContext.h"
#include "nsIPresShell.h"
#include "nsLayoutCID.h"

#include "nsEditorUtils.h"

#include "InsertTextTxn.h"
#include "DeleteTextTxn.h"

//const static char* kMOZEditorBogusNodeAttr="MOZ_EDITOR_BOGUS_NODE";
//const static char* kMOZEditorBogusNodeValue="TRUE";
const static PRUnichar nbsp = 160;

static NS_DEFINE_IID(kSubtreeIteratorCID, NS_SUBTREEITERATOR_CID);
static NS_DEFINE_IID(kContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);

enum
{
  kLonely = 0,
  kPrevSib = 1,
  kNextSib = 2,
  kBothSibs = 3
};

nsresult
NS_NewHTMLEditRules(nsIEditRules** aInstancePtrResult)
{
  nsHTMLEditRules * rules = new nsHTMLEditRules();
  if (rules)
    return rules->QueryInterface(NS_GET_IID(nsIEditRules), (void**) aInstancePtrResult);
  return NS_ERROR_OUT_OF_MEMORY;
}

/********************************************************
 *  Constructor/Destructor 
 ********************************************************/

nsHTMLEditRules::nsHTMLEditRules() : 
mDocChangeRange(nsnull)
,mListenerEnabled(PR_TRUE)
,mUtilRange(nsnull)
,mBody(nsnull)
,mJoinOffset(0)
{
}

nsHTMLEditRules::~nsHTMLEditRules()
{
  // remove ourselves as a listener to edit actions
  // In the normal case, we have already been removed by 
  // ~nsHTMLEditor, in which case we will get an error here
  // which we ignore.  But this allows us to add the ability to
  // switch rule sets on the fly if we want.
  mEditor->RemoveEditActionListener(this);
}

/********************************************************
 *  XPCOM Cruft
 ********************************************************/

NS_IMPL_ADDREF_INHERITED(nsHTMLEditRules, nsTextEditRules)
NS_IMPL_RELEASE_INHERITED(nsHTMLEditRules, nsTextEditRules)
NS_IMPL_QUERY_INTERFACE2(nsHTMLEditRules, nsIEditRules, nsIEditActionListener)


/********************************************************
 *  Public methods 
 ********************************************************/

NS_IMETHODIMP
nsHTMLEditRules::Init(nsHTMLEditor *aEditor, PRUint32 aFlags)
{
  // call through to base class Init first
  nsresult res = nsTextEditRules::Init(aEditor, aFlags);
  if (NS_FAILED(res)) return res;

  // make a utility range for use by the listenter
  res = nsComponentManager::CreateInstance(kRangeCID, nsnull, NS_GET_IID(nsIDOMRange),
                                                    getter_AddRefs(mUtilRange));
  if (NS_FAILED(res)) return res;
  
  // pass over document and add any needed mozBRs
  // first turn off undo
  mEditor->EnableUndo(PR_FALSE);
  
  // set up mDocChangeRange to be whole doc
  nsCOMPtr<nsIDOMElement> bodyElem;
  nsCOMPtr<nsIDOMNode> bodyNode;
  mEditor->GetBodyElement(getter_AddRefs(bodyElem));
  bodyNode = do_QueryInterface(bodyElem);
  if (bodyNode)
  {
    // temporarily turn off rules sniffing
    nsAutoLockRulesSniffing lockIt((nsTextEditRules*)this);
    res = nsComponentManager::CreateInstance(kRangeCID, nsnull, NS_GET_IID(nsIDOMRange),
                                                    getter_AddRefs(mDocChangeRange));
    if (NS_FAILED(res)) return res;
    if (!mDocChangeRange) return NS_ERROR_NULL_POINTER;
    mDocChangeRange->SelectNode(bodyNode);
    res = ReplaceNewlines(mDocChangeRange);
    if (NS_FAILED(res)) return res;
    res = AdjustSpecialBreaks();
    if (NS_FAILED(res)) return res;
  }
  
  // turn on undo
  mEditor->EnableUndo(PR_TRUE);
  
  // add ourselves as a listener to edit actions
  res = mEditor->AddEditActionListener(this);

  return res;
}


NS_IMETHODIMP
nsHTMLEditRules::BeforeEdit(PRInt32 action, nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) return NS_OK;
  
  nsAutoLockRulesSniffing lockIt((nsTextEditRules*)this);
  
  if (!mActionNesting)
  {
    mDocChangeRange = nsnull;  // clear out our accounting of what changed
    // turn off caret
    nsCOMPtr<nsIPresShell> pres;
    mEditor->GetPresShell(getter_AddRefs(pres));
    if (pres) pres->SetCaretEnabled(PR_FALSE);
    // check that selection is in subtree defined by body node
    ConfirmSelectionInBody();
    // let rules remember the top level action
    mTheAction = action;
  }
  mActionNesting++;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLEditRules::AfterEdit(PRInt32 action, nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) return NS_OK;

  nsAutoLockRulesSniffing lockIt(this);

  NS_PRECONDITION(mActionNesting>0, "bad action nesting!");
  nsresult res = NS_OK;
  if (!--mActionNesting)
  {
    ConfirmSelectionInBody();
    if (action == nsEditor::kOpIgnore) return NS_OK;
    
    nsCOMPtr<nsIDOMSelection>selection;
    res = mEditor->GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    
    if (mDocChangeRange && !((action == nsEditor::kOpUndo) || (action == nsEditor::kOpRedo)))
    {
      // dont let any txns in here move the selection around behind our back.
      // Note that this won't prevent explicit selection setting from working.
      nsAutoTxnsConserveSelection dontSpazMySelection(mEditor);
     
      // expand the "changed doc range" as needed
      res = PromoteRange(mDocChangeRange, action);
      if (NS_FAILED(res)) return res;
      
      // add in any needed <br>s, and remove any unneeded ones.
      res = AdjustSpecialBreaks();
      if (NS_FAILED(res)) return res;
      
      // adjust whitespace for insert text and delete actions
      if ((action == nsEditor::kOpInsertText) || 
          (action == nsEditor::kOpInsertIMEText) ||
          (action == nsEditor::kOpDeleteSelection))
      {
        res = AdjustWhitespace(selection);
        if (NS_FAILED(res)) return res;
      }
      
      // replace newlines that are preformatted
      if ((action == nsEditor::kOpInsertText) || 
          (action == nsEditor::kOpInsertIMEText) ||
          (action == nsEditor::kOpInsertNode))
      {
        res = ReplaceNewlines(mDocChangeRange);
      }
      
      // clean up any empty nodes in the selection
      res = RemoveEmptyNodes();
      if (NS_FAILED(res)) return res;
      
/* I'll move to this code in M15.  For now being very conservative with changes

      // adjust selection for insert text and delete actions
      if ((action == nsEditor::kOpInsertText) || 
          (action == nsEditor::kOpInsertIMEText) ||
          (action == nsEditor::kOpDeleteSelection))
      {
        res = AdjustSelection(selection, aDirection);
        if (NS_FAILED(res)) return res;
      }
*/
    }
    
    // adjust selection unless it was an inline style manipulation
    // see above commented out code: we're just being safe for now
    // with the minimal change to fix selection problem when removing
    // link property
    if ((action != nsEditor::kOpSetTextProperty) && 
        (action != nsEditor::kOpRemoveTextProperty))
    {
      res = AdjustSelection(selection, aDirection);
      if (NS_FAILED(res)) return res;
    }
    
    // detect empty doc
    res = CreateBogusNodeIfNeeded(selection);

    // turn on caret
    nsCOMPtr<nsIPresShell> pres;
    mEditor->GetPresShell(getter_AddRefs(pres));
    if (pres) pres->SetCaretEnabled(PR_TRUE);
  }

  return res;
}


NS_IMETHODIMP 
nsHTMLEditRules::WillDoAction(nsIDOMSelection *aSelection, 
                              nsRulesInfo *aInfo, 
                              PRBool *aCancel, 
                              PRBool *aHandled)
{
  if (!aInfo || !aCancel || !aHandled) 
    return NS_ERROR_NULL_POINTER;
#if defined(DEBUG_ftang)
  printf("nsHTMLEditRules::WillDoAction action = %d\n", aInfo->action);
#endif

  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
    
  // my kingdom for dynamic cast
  nsTextRulesInfo *info = NS_STATIC_CAST(nsTextRulesInfo*, aInfo);
    
  switch (info->action)
  {
    case kInsertText:
    case kInsertTextIME:
      return WillInsertText(info->action,
                            aSelection, 
                            aCancel, 
                            aHandled,
                            info->inString,
                            info->outString,
                            info->typeInState,
                            info->maxLength);
    case kInsertBreak:
      return WillInsertBreak(aSelection, aCancel, aHandled);
    case kDeleteSelection:
      return WillDeleteSelection(aSelection, info->collapsedAction, aCancel, aHandled);
    case kMakeList:
      return WillMakeList(aSelection, info->bOrdered, aCancel, aHandled);
    case kIndent:
      return WillIndent(aSelection, aCancel, aHandled);
    case kOutdent:
      return WillOutdent(aSelection, aCancel, aHandled);
    case kAlign:
      return WillAlign(aSelection, info->alignType, aCancel, aHandled);
    case kMakeBasicBlock:
      return WillMakeBasicBlock(aSelection, info->blockType, aCancel, aHandled);
    case kRemoveList:
      return WillRemoveList(aSelection, info->bOrdered, aCancel, aHandled);
    case kInsertElement:
      return WillInsert(aSelection, aCancel);
  }
  return nsTextEditRules::WillDoAction(aSelection, aInfo, aCancel, aHandled);
}
  
  
NS_IMETHODIMP 
nsHTMLEditRules::DidDoAction(nsIDOMSelection *aSelection,
                             nsRulesInfo *aInfo, nsresult aResult)
{
  // pass thru to nsTextEditRules:
  nsresult res = nsTextEditRules::DidDoAction(aSelection, aInfo, aResult);
  return res;
}
  

/********************************************************
 *  Protected rules methods 
 ********************************************************/


nsresult
nsHTMLEditRules::WillInsertText(PRInt32          aAction,
                                nsIDOMSelection *aSelection, 
                                PRBool          *aCancel,
                                PRBool          *aHandled,
                                const nsString  *inString,
                                nsString        *outString,
                                TypeInState      typeInState,
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

  char specialChars[] = {'\t','\n',0};
  
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
  
  // get the (collapsed) selection location
  res = mEditor->GetStartNodeAndOffset(aSelection, &selNode, &selOffset);
  if (NS_FAILED(res)) return res;

  // dont put text in places that cant have it
  nsAutoString textTag = "__moz_text";
  if (!mEditor->IsTextNode(selNode) && !mEditor->CanContainTag(selNode, textTag))
    return NS_ERROR_FAILURE;

  // take care of typeinstate issues
  if (typeInState.IsAnySet())
  { // for every property that is set, insert a new inline style node
    res = CreateStyleForInsertText(aSelection, typeInState);
    if (NS_FAILED(res)) return res; 
    // refresh the (collapsed) selection location
    res = mEditor->GetStartNodeAndOffset(aSelection, &selNode, &selOffset);
    if (NS_FAILED(res)) return res;
  }
  
  // identify the block
  nsCOMPtr<nsIDOMNode> blockParent;
  
  if (nsEditor::IsBlockNode(selNode)) 
    blockParent = selNode;
  else 
    blockParent = mEditor->GetBlockNodeParent(selNode);
  if (!blockParent) return NS_ERROR_FAILURE;
  
  PRBool bCancel;  
  nsString theString(*inString);  // copy instring for now
  if(aAction == kInsertTextIME) 
  { 
     // special case for IME. We need this to :
     // a) handle null strings, which are meaningful for IME
     // b) prevent the string from being broken into substrings,
     //    which can happen in non-IME processing below.
     // I should probably convert runs of spaces and tabs here as well
     res = DoTextInsertion(aSelection, &bCancel, &theString, typeInState);
  }
  else // aAction == kInsertText
  {
    // we need to get the doc
    nsCOMPtr<nsIDOMDocument>doc;
    res = mEditor->GetDocument(getter_AddRefs(doc));
    if (NS_FAILED(res)) return res;
    if (!doc) return NS_ERROR_NULL_POINTER;
    
    // find where we are
    nsCOMPtr<nsIDOMNode> curNode = selNode;
    PRInt32 curOffset = selOffset;
    
    // is our text going to be PREformatted?  
    // We remember this so that we know how to handle tabs.
    PRBool isPRE;
    res = mEditor->IsPreformatted(selNode, &isPRE);
    if (NS_FAILED(res)) return res;    
    
    // turn off the edit listener: we know how to
    // build the "doc changed range" ourselves, and it's
    // must faster to do it once here than to track all
    // the changes one at a time.
    nsAutoLockListener lockit(&mListenerEnabled); 
    
    // dont spaz my selection in subtransactions
    nsAutoTxnsConserveSelection dontSpazMySelection(mEditor);
    nsAutoString partialString;
    nsCOMPtr<nsIDOMNode> unused;
    PRInt32 pos;
        
    // for efficiency, break out the pre case seperately.  This is because
    // its a lot cheaper to search the input string for only newlines than
    // it is to search for both tabs and newlines.
    if (isPRE)
    {
      char newlineChar = '\n';
      while (theString.Length())
      {
        pos = theString.FindChar(newlineChar);
        // if first char is newline, then use just it
        if (pos == 0) pos = 1;
        if (pos == -1) pos = theString.Length();
        theString.Left(partialString, pos);
        theString.Cut(0, pos);
        // is it a return?
        if (partialString == "\n")
        {
          res = mEditor->JoeCreateBR(&curNode, &curOffset, &unused, nsIEditor::eNone);
        }
        else
        {
          res = mEditor->JoeInsertTextImpl(partialString, &curNode, &curOffset, doc);
        }
        if (NS_FAILED(res)) return res;
      }
    }
    else
    {
      char specialChars[] = {'\t','\n',0};
      nsAutoString tabString = "    ";
      while (theString.Length())
      {
        pos = theString.FindCharInSet(specialChars);
        // if first char is special, then use just it
        if (pos == 0) pos = 1;
        if (pos == -1) pos = theString.Length();
        theString.Left(partialString, pos);
        theString.Cut(0, pos);
        // is it a tab?
        if (partialString == "\t")
        {
          partialString = "    ";
          res = mEditor->JoeInsertTextImpl(tabString, &curNode, &curOffset, doc);
        }
        // is it a return?
        else if (partialString == "\n")
        {
          res = mEditor->JoeCreateBR(&curNode, &curOffset, &unused, nsIEditor::eNone);
        }
        else
        {
          res = mEditor->JoeInsertTextImpl(partialString, &curNode, &curOffset, doc);
        }
        if (NS_FAILED(res)) return res;
      }
    }
    if (curNode) aSelection->Collapse(curNode, curOffset);
    // manually update the doc changed range so that AfterEdit will clean up
    // the correct portion of the document.
    if (!mDocChangeRange)
    {
      res = nsComponentManager::CreateInstance(kRangeCID, nsnull, NS_GET_IID(nsIDOMRange),
                                                    getter_AddRefs(mDocChangeRange));
      if (NS_FAILED(res)) return res;
    		if (!mDocChangeRange) return NS_ERROR_NULL_POINTER;
    }
    res = mDocChangeRange->SetStart(selNode, selOffset);
    if (NS_FAILED(res)) return res;
    res = mDocChangeRange->SetEnd(curNode, curOffset);
    if (NS_FAILED(res)) return res;
  }
  return res;
}

nsresult
nsHTMLEditRules::WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  

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


  // split any mailcites in the way
  if (mFlags & nsIHTMLEditor::eEditorMailMask)
  {
    nsCOMPtr<nsIDOMNode> citeNode, selNode;
    PRInt32 selOffset, newOffset;
    res = mEditor->GetStartNodeAndOffset(aSelection, &selNode, &selOffset);
    if (NS_FAILED(res)) return res;
    res = GetTopEnclosingMailCite(selNode, &citeNode);
    if (NS_FAILED(res)) return res;
    
    if (citeNode)
    {
      res = mEditor->SplitNodeDeep(citeNode, selNode, selOffset, &newOffset);
      if (NS_FAILED(res)) return res;
      res = citeNode->GetParentNode(getter_AddRefs(selNode));
      if (NS_FAILED(res)) return res;
      res = aSelection->Collapse(selNode, newOffset);
      if (NS_FAILED(res)) return res;
    }
  }

  // smart splitting rules
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  
  res = mEditor->GetStartNodeAndOffset(aSelection, &node, &offset);
  if (NS_FAILED(res)) return res;
  if (!node) return NS_ERROR_FAILURE;
    
  // identify the block
  nsCOMPtr<nsIDOMNode> blockParent;
  
  if (nsEditor::IsBlockNode(node)) 
    blockParent = node;
  else 
    blockParent = mEditor->GetBlockNodeParent(node);
    
  if (!blockParent) return NS_ERROR_FAILURE;
  
  // headers: close (or split) header
  else if (nsHTMLEditUtils::IsHeader(blockParent))
  {
    res = ReturnInHeader(aSelection, blockParent, node, offset);
    *aHandled = PR_TRUE;
    return NS_OK;
  }
  
  // paragraphs: special rules to look for <br>s
  else if (nsHTMLEditUtils::IsParagraph(blockParent))
  {
    res = ReturnInParagraph(aSelection, blockParent, node, offset, aCancel, aHandled);
    return NS_OK;
  }
  
  // list items: special rules to make new list items
  else if (nsHTMLEditUtils::IsListItem(blockParent))
  {
    res = ReturnInListItem(aSelection, blockParent, node, offset);
    *aHandled = PR_TRUE;
    return NS_OK;
  }
  // its something else (body, div, td, ...): insert a normal br
  else
  {
    nsCOMPtr<nsIDOMNode> brNode;
    res = mEditor->CreateBR(node, offset, &brNode);  
    if (NS_FAILED(res)) return res;
    res = nsEditor::GetNodeLocation(brNode, &node, &offset);
    if (NS_FAILED(res)) return res;
  // SetHint(PR_TRUE) means we want the caret to stick to the content on the "right".
  // We want the caret to stick to whatever is past the break.  This is
  // because the break is on the same line we were on, but the next content
  // will be on the following line.
    aSelection->SetHint(PR_TRUE);
    res = aSelection->Collapse(node, offset+1);
    if (NS_FAILED(res)) return res;
    *aHandled = PR_TRUE;
  }
  
  return res;
}



nsresult
nsHTMLEditRules::WillDeleteSelection(nsIDOMSelection *aSelection, 
                                     nsIEditor::EDirection aAction, 
                                     PRBool *aCancel,
                                     PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  
  // if there is only bogus content, cancel the operation
  if (mBogusNode) 
  {
    *aCancel = PR_TRUE;
    return NS_OK;
  }

  nsresult res = NS_OK;
  
  PRBool bCollapsed;
  res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  
  nsCOMPtr<nsIDOMNode> node, selNode;
  PRInt32 offset, selOffset;
  
  res = mEditor->GetStartNodeAndOffset(aSelection, &node, &offset);
  if (NS_FAILED(res)) return res;
  if (!node) return NS_ERROR_FAILURE;
    
  if (bCollapsed)
  {
    // easy case, in a text node:
    if (mEditor->IsTextNode(node))
    {
      nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(node);
      PRUint32 strLength;
      res = textNode->GetLength(&strLength);
      if (NS_FAILED(res)) return res;
    
      // at beginning of text node and backspaced?
      if (!offset && (aAction == nsIEditor::ePrevious))
      {
        nsCOMPtr<nsIDOMNode> priorNode;
        res = GetPriorHTMLNode(node, &priorNode);
        if (NS_FAILED(res)) return res;
        
        // if there is no prior node then cancel the deletion
        if (!priorNode)
        {
          *aCancel = PR_TRUE;
          return res;
        }
        
        // block parents the same?  
        if (mEditor->HasSameBlockNodeParent(node, priorNode)) 
        {
          // is prior node not a container?  (ie, a br, hr, image...)
          if (!mEditor->IsContainer(priorNode))   // MOOSE: anchors not handled
          {
            // delete the break, and join like nodes if appropriate
            res = mEditor->DeleteNode(priorNode);
            if (NS_FAILED(res)) return res;
            // we did something, so lets say so.
            *aHandled = PR_TRUE;
            // get new prior node
            res = GetPriorHTMLNode(node, &priorNode);
            if (NS_FAILED(res)) return res;
            // are they in same block?
            if (mEditor->HasSameBlockNodeParent(node, priorNode)) 
            {
              // are they same type?
              if (mEditor->NodesSameType(node, priorNode))
              {
                // if so, join them!
                nsCOMPtr<nsIDOMNode> topParent;
                priorNode->GetParentNode(getter_AddRefs(topParent));
                res = JoinNodesSmart(priorNode,node,&selNode,&selOffset);
                if (NS_FAILED(res)) return res;
                // fix up selection
                res = aSelection->Collapse(selNode,selOffset);
                return res;
              }
            }
          }
          // is prior node inline and same type?  (This shouldn't happen)
          if ( mEditor->HasSameBlockNodeParent(node, priorNode) && 
                 ( mEditor->IsInlineNode(node) || mEditor->IsTextNode(node) ) &&
                 mEditor->NodesSameType(node, priorNode) )
          {
            // if so, join them!
            nsCOMPtr<nsIDOMNode> topParent;
            priorNode->GetParentNode(getter_AddRefs(topParent));
            *aHandled = PR_TRUE;
            res = JoinNodesSmart(priorNode,node,&selNode,&selOffset);
            if (NS_FAILED(res)) return res;
            // fix up selection
            res = aSelection->Collapse(selNode,selOffset);
            return res;
          }
          else return NS_OK; // punt to default
        }
        
        // deleting across blocks
        nsCOMPtr<nsIDOMNode> leftParent = mEditor->GetBlockNodeParent(priorNode);
        nsCOMPtr<nsIDOMNode> rightParent = mEditor->GetBlockNodeParent(node);
        
        // if leftParent or rightParent is null, it's because the
        // corresponding selection endpoint is in the body node.
        if (!leftParent || !rightParent)
          return NS_OK;  // bail to default
          
        // do not delete across table structures
        if (mEditor->IsTableElement(leftParent) || mEditor->IsTableElement(rightParent))
        {
          *aCancel = PR_TRUE;
          return NS_OK;
        }
                
        // are the blocks of same type?
        if (mEditor->NodesSameType(leftParent, rightParent))
        {
          nsCOMPtr<nsIDOMNode> topParent;
          leftParent->GetParentNode(getter_AddRefs(topParent));
          
          *aHandled = PR_TRUE;
          res = JoinNodesSmart(leftParent,rightParent,&selNode,&selOffset);
          if (NS_FAILED(res)) return res;
          // fix up selection
          res = aSelection->Collapse(selNode,selOffset);
          return res;
        }
        
        // else blocks not same type, bail to default
        return NS_OK;
        
      }
    
      // at end of text node and deleted?
      if ((offset == (PRInt32)strLength)
          && (aAction == nsIEditor::eNext))
      {
        nsCOMPtr<nsIDOMNode> nextNode;
        res = GetNextHTMLNode(node, &nextNode);
        if (NS_FAILED(res)) return res;
         
        // if there is no next node, or it's not in the body, then cancel the deletion
        if (!nextNode || !nsHTMLEditUtils::InBody(nextNode))
        {
          *aCancel = PR_TRUE;
          return res;
        }
       
        // block parents the same?  
        if (mEditor->HasSameBlockNodeParent(node, nextNode)) 
        {
          // is next node not a container?  (ie, a br, hr, image...)
          if (!mEditor->IsContainer(nextNode))  // MOOSE: anchors not handled
          {
            // delete the break, and join like nodes if appropriate
            res = mEditor->DeleteNode(nextNode);
            if (NS_FAILED(res)) return res;
            // we did something, so lets say so.
            *aHandled = PR_TRUE;
            // get new next node
            res = GetNextHTMLNode(node, &nextNode);
            if (NS_FAILED(res)) return res;
            // are they in same block?
            if (mEditor->HasSameBlockNodeParent(node, nextNode)) 
            {
              // are they same type?
              if (mEditor->NodesSameType(node, nextNode))
              {
                // if so, join them!
                nsCOMPtr<nsIDOMNode> topParent;
                nextNode->GetParentNode(getter_AddRefs(topParent));
                res = JoinNodesSmart(node,nextNode,&selNode,&selOffset);
                if (NS_FAILED(res)) return res;
                // fix up selection
                res = aSelection->Collapse(selNode,selOffset);
                return res;
              }
            }
          }
          // is next node inline and same type?  (This shouldn't happen)
          if ( mEditor->HasSameBlockNodeParent(node, nextNode) && 
                 ( mEditor->IsInlineNode(node) || mEditor->IsTextNode(node) ) &&
                 mEditor->NodesSameType(node, nextNode) )
          {
            // if so, join them!
            nsCOMPtr<nsIDOMNode> topParent;
            nextNode->GetParentNode(getter_AddRefs(topParent));
            *aHandled = PR_TRUE;
            res = JoinNodesSmart(node,nextNode,&selNode,&selOffset);
            if (NS_FAILED(res)) return res;
            // fix up selection
            res = aSelection->Collapse(selNode,selOffset);
            return res;
          }
          else return NS_OK; // punt to default
        }
                
        // deleting across blocks
        nsCOMPtr<nsIDOMNode> leftParent = mEditor->GetBlockNodeParent(node);
        nsCOMPtr<nsIDOMNode> rightParent = mEditor->GetBlockNodeParent(nextNode);

        // if leftParent or rightParent is null, it's because the
        // corresponding selection endpoint is in the body node.
        if (!leftParent || !rightParent)
          return NS_OK;  // bail to default
          
        // do not delete across table structures
        if (mEditor->IsTableElement(leftParent) || mEditor->IsTableElement(rightParent))
        {
          *aCancel = PR_TRUE;
          return NS_OK;
        }
                
        // are the blocks of same type?
        if (mEditor->NodesSameType(leftParent, rightParent))
        {
          nsCOMPtr<nsIDOMNode> topParent;
          leftParent->GetParentNode(getter_AddRefs(topParent));
          
          *aHandled = PR_TRUE;
          res = JoinNodesSmart(leftParent,rightParent,&selNode,&selOffset);
          if (NS_FAILED(res)) return res;
          // fix up selection
          res = aSelection->Collapse(selNode,selOffset);
          return res;
        }
        
        // else blocks not same type, bail to default
        return NS_OK;
        
      }
    }
    // else not in text node; we need to find right place to act on
    else
    {
      nsCOMPtr<nsIDOMNode> nodeToDelete;
      
      // first note that the right node to delete might be the one we
      // are in.  For example, if a list item is deleted one character at a time,
      // eventually it will be empty (except for a moz-br).  If the user hits 
      // backspace again, they expect the item itself to go away.  Check to
      // see if we are in an "empty" node.
      // Note: do NOT delete table elements this way.
      PRBool bIsEmptyNode;
      res = IsEmptyNode(node, &bIsEmptyNode, PR_TRUE, PR_FALSE);
      if (bIsEmptyNode && !mEditor->IsTableElement(node))
        nodeToDelete = node;
      else if (aAction == nsIEditor::ePrevious)
        res = GetPriorHTMLNode(node, offset, &nodeToDelete);
      else if (aAction == nsIEditor::eNext)
        res = GetNextHTMLNode(node, offset, &nodeToDelete);
      else
        return NS_OK;
        
      if (NS_FAILED(res)) return res;
      if (!nodeToDelete) return NS_ERROR_NULL_POINTER;
        
      // if this node is text node, adjust selection
      if (nsEditor::IsTextNode(nodeToDelete))
      {
        PRUint32 selPoint = 0;
        nsCOMPtr<nsIDOMCharacterData>nodeAsText;
        nodeAsText = do_QueryInterface(nodeToDelete);
        if (aAction == nsIEditor::ePrevious)
          nodeAsText->GetLength(&selPoint);
        res = aSelection->Collapse(nodeToDelete,selPoint);
        return res;
      }
      else
      {
        // editable leaf node is not text; delete it.
        // that's the default behavior
        res = nsEditor::GetNodeLocation(nodeToDelete, &node, &offset);
        if (NS_FAILED(res)) return res;
        // EXCEPTION: if it's a mozBR, we have to check and see if
        // there is a br in front of it. If so, we must delete both.
        // else you get this: deletion code deletes mozBR, then selection
        // adjusting code puts it back in.  doh
        if (nsHTMLEditUtils::IsMozBR(nodeToDelete))
        {
          nsCOMPtr<nsIDOMNode> brNode;
          res = GetPriorHTMLNode(nodeToDelete, &brNode);
          if (nsHTMLEditUtils::IsBreak(brNode))
          {
            // is brNode also a descendant of same block?
            nsCOMPtr<nsIDOMNode> block, brBlock;
            block = mEditor->GetBlockNodeParent(nodeToDelete);
            brBlock = mEditor->GetBlockNodeParent(brNode);
            if (block == brBlock)
            {
              // delete both breaks
              res = mEditor->DeleteNode(brNode);
              if (NS_FAILED(res)) return res;
              res = mEditor->DeleteNode(nodeToDelete);
              *aHandled = PR_TRUE;
              return res;
            }
            // else fall through
          }
          // else fall through
        }
        // adjust selection to be right after it
        res = aSelection->Collapse(node, offset+1);
        if (NS_FAILED(res)) return res;
        res = mEditor->DeleteNode(nodeToDelete);
        *aHandled = PR_TRUE;
        return res;
      }
    }
    
    return NS_OK;
  }
  
  // else we have a non collapsed selection
  // figure out if the enpoints are in nodes that can be merged
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 endOffset;
  res = mEditor->GetEndNodeAndOffset(aSelection, &endNode, &endOffset);
  if (NS_FAILED(res)) 
  { 
    return res; 
  }
  if (endNode.get() != node.get())
  {
    // block parents the same?  use default deletion
    if (mEditor->HasSameBlockNodeParent(node, endNode)) return NS_OK;
    
    // deleting across blocks
    // are the blocks of same type?
    nsCOMPtr<nsIDOMNode> leftParent;
    nsCOMPtr<nsIDOMNode> rightParent;

    // XXX: Fix for bug #10815: Crash deleting selected text and table.
    //      Make sure leftParent and rightParent are never NULL. This
    //      can happen if we call GetBlockNodeParent() and the node we
    //      pass in is a body node.
    //
    //      Should we be calling IsBlockNode() instead of IsBody() here?

    if (nsHTMLEditUtils::IsBody(node))
      leftParent = node;
    else
      leftParent = mEditor->GetBlockNodeParent(node);

    if (nsHTMLEditUtils::IsBody(endNode))
      rightParent = endNode;
    else
      rightParent = mEditor->GetBlockNodeParent(endNode);
    
    // do not delete across table structures
    if (mEditor->IsTableElement(leftParent) || mEditor->IsTableElement(rightParent))
    {
      *aCancel = PR_TRUE;
      return NS_OK;
    }
                
    // are the blocks siblings?
    nsCOMPtr<nsIDOMNode> leftBlockParent;
    nsCOMPtr<nsIDOMNode> rightBlockParent;
    leftParent->GetParentNode(getter_AddRefs(leftBlockParent));
    rightParent->GetParentNode(getter_AddRefs(rightBlockParent));
    // bail to default if blocks aren't siblings
    if (leftBlockParent.get() != rightBlockParent.get()) return NS_OK;

    if (mEditor->NodesSameType(leftParent, rightParent))
    {
      nsCOMPtr<nsIDOMNode> topParent;
      leftParent->GetParentNode(getter_AddRefs(topParent));
      
      if (nsHTMLEditUtils::IsParagraph(leftParent))
      {
        // first delete the selection
        *aHandled = PR_TRUE;
        res = mEditor->DeleteSelectionImpl(aAction);
        if (NS_FAILED(res)) return res;
        // then join para's, insert break
        res = mEditor->JoinNodeDeep(leftParent,rightParent,&selNode,&selOffset);
        if (NS_FAILED(res)) return res;
        // fix up selection
        res = aSelection->Collapse(selNode,selOffset);
        return res;
      }
      if (nsHTMLEditUtils::IsListItem(leftParent)
          || nsHTMLEditUtils::IsHeader(leftParent))
      {
        // first delete the selection
        *aHandled = PR_TRUE;
        res = mEditor->DeleteSelectionImpl(aAction);
        if (NS_FAILED(res)) return res;
        // join blocks
        res = mEditor->JoinNodeDeep(leftParent,rightParent,&selNode,&selOffset);
        if (NS_FAILED(res)) return res;
        // fix up selection
        res = aSelection->Collapse(selNode,selOffset);
        return res;
      }
    }
    
    // else blocks not same type, bail to default
    return NS_OK;
  }

  return res;
}  


nsresult
nsHTMLEditRules::WillMakeList(nsIDOMSelection *aSelection, 
                              PRBool aOrdered, 
                              PRBool *aCancel,
                              PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }

  nsresult res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;

  nsAutoString blockType("ul");
  if (aOrdered) blockType = "ol";
    
  PRBool outMakeEmpty;
  res = ShouldMakeEmptyBlock(aSelection, &blockType, &outMakeEmpty);
  if (NS_FAILED(res)) return res;
  if (outMakeEmpty) 
  {
    nsCOMPtr<nsIDOMNode> parent, theList, theListItem;
    PRInt32 offset;
    
    // get selection location
    res = mEditor->GetStartNodeAndOffset(aSelection, &parent, &offset);
    if (NS_FAILED(res)) return res;
    
    // make sure we can put a list here
    nsAutoString listType;
    if (aOrdered) listType = "ol";
    else listType = "ul";
    if (mEditor->CanContainTag(parent,listType))
    {
      res = mEditor->CreateNode(listType, parent, offset, getter_AddRefs(theList));
      if (NS_FAILED(res)) return res;
      res = mEditor->CreateNode("li", theList, 0, getter_AddRefs(theListItem));
      if (NS_FAILED(res)) return res;
      // put selection in new list item
      res = aSelection->Collapse(theListItem,0);
      *aHandled = PR_TRUE;
    }
    else
    {
      // cant make list here - cancel the operation.
      *aCancel = PR_TRUE;
    }
    return res;
  }
  
  // ok, we aren't creating a new empty list.  Instead we are converting
  // the set of blocks implied by the selection into a list.
  
  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  
  *aHandled = PR_TRUE;

  nsAutoSelectionReset selectionResetter(aSelection, mEditor);
  
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(aSelection, &arrayOfRanges, kMakeList);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, &arrayOfNodes, kMakeList);
  if (NS_FAILED(res)) return res;                                 
               
  // pre process our list of nodes...                      
  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  for (i=(PRInt32)listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> testNode( do_QueryInterface(isupports ) );

    // Remove all non-editable nodes.  Leave them be.
    if (!mEditor->IsEditable(testNode))
    {
      arrayOfNodes->RemoveElementAt(i);
    }
    
    // scan for table elements.  If we find table elements other than table,
    // replace it with a list of any editable non-table content.
    if (mEditor->IsTableElement(testNode) && !mEditor->IsTable(testNode))
    {
      arrayOfNodes->RemoveElementAt(i);
      nsCOMPtr<nsISupportsArray> arrayOfTableContent;
      res = GetTableContent(testNode, &arrayOfTableContent);
      if (NS_FAILED(res)) return res;
      arrayOfNodes->AppendElements(arrayOfTableContent);
    }
  }
  
  
  
  // if there is only one node in the array, and it is a list, div, or blockquote,
  // then look inside of it until we find what we want to make a list out of.
  arrayOfNodes->Count(&listCount);
  if (listCount == 1)
  {
    nsCOMPtr<nsISupports> isupports = (dont_AddRef)(arrayOfNodes->ElementAt(0));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );
    
    while (nsHTMLEditUtils::IsDiv(curNode)
           || nsHTMLEditUtils::IsOrderedList(curNode)
           || nsHTMLEditUtils::IsUnorderedList(curNode)
           || nsHTMLEditUtils::IsBlockquote(curNode))
    {
      // dive as long as there is only one child, and it is a list, div, blockquote
      PRUint32 numChildren;
      res = mEditor->CountEditableChildren(curNode, numChildren);
      if (NS_FAILED(res)) return res;
      
      if (numChildren == 1)
      {
        // keep diving
        nsCOMPtr <nsIDOMNode> tmpNode = nsEditor::GetChildAt(curNode, 0);
        if (nsHTMLEditUtils::IsDiv(tmpNode)
            || nsHTMLEditUtils::IsOrderedList(tmpNode)
            || nsHTMLEditUtils::IsUnorderedList(tmpNode)
            || nsHTMLEditUtils::IsBlockquote(tmpNode))
        {
          // check editablility XXX floppy moose
          curNode = tmpNode;
        }
        else break;
      }
      else break;
    }
    // we've found innermost list/blockquote/div: 
    // replace the one node in the array with this node
    isupports = do_QueryInterface(curNode);
    arrayOfNodes->ReplaceElementAt(isupports, 0);
  }

  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.
  
  nsVoidArray transitionList;
  res = MakeTransitionList(arrayOfNodes, &transitionList);
  if (NS_FAILED(res)) return res;                                 

  // Ok, now go through all the nodes and put then in the list, 
  // or whatever is approriate.  Wohoo!

  arrayOfNodes->Count(&listCount);
  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curList;
  nsCOMPtr<nsIDOMNode> prevListItem;
    
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;
  
    if (transitionList[i] &&                                      // transition node
        ((((i+1)<(PRInt32)listCount) && transitionList[i+1]) ||   // and next node is transistion node
        ( i+1 >= (PRInt32)listCount)))                            // or there is no next node
    {
      // the parent of this node has no other children on the 
      // list of nodes to make a list out of.  So if this node
      // is a list, change it's list type if needed instead of 
      // reparenting it 
      nsCOMPtr<nsIDOMNode> newBlock;
      if (nsHTMLEditUtils::IsUnorderedList(curNode))
      {
        if (aOrdered)
        {
          // make a new ordered list, insert it where the current unordered list is,
          // and move all the children to the new list, and remove the old list
          res = mEditor->ReplaceContainer(curNode,&newBlock,blockType);
          if (NS_FAILED(res)) return res;
          curList = newBlock;
          continue;
        }
        else
        {
          // do nothing, we are already the right kind of list
          curList = newBlock;
          continue;
        }
      }
      else if (nsHTMLEditUtils::IsOrderedList(curNode))
      {
        if (!aOrdered)
        {
          // make a new unordered list, insert it where the current ordered list is,
          // and move all the children to the new list, and remove the old list
          mEditor->ReplaceContainer(curNode,&newBlock,blockType);
          if (NS_FAILED(res)) return res;
          curList = newBlock;
          continue;
        }
        else
        {
          // do nothing, we are already the right kind of list
          curList = newBlock;
          continue;
        }
      }
      else if (nsHTMLEditUtils::IsDiv(curNode) || nsHTMLEditUtils::IsBlockquote(curNode))
      {
        // XXX floppy moose
      }
    }  // lonely node
  
    // need to make a list to put things in if we haven't already,
    // or if this node doesn't go in list we used earlier.
    if (!curList || transitionList[i])
    {
      nsAutoString listType;
      if (aOrdered) listType = "ol";
      else listType = "ul";
      res = mEditor->CreateNode(listType, curParent, offset, getter_AddRefs(curList));
      if (NS_FAILED(res)) return res;
      // curList is now the correct thing to put curNode in
      prevListItem = 0;
    }
  
    // if curNode is a Break, delete it, and quit remembering prev list item
    if (nsHTMLEditUtils::IsBreak(curNode)) 
    {
      res = mEditor->DeleteNode(curNode);
      if (NS_FAILED(res)) return res;
      prevListItem = 0;
      continue;
    }
    
    // if curNode isn't a list item, we must wrap it in one
    nsCOMPtr<nsIDOMNode> listItem;
    if (!nsHTMLEditUtils::IsListItem(curNode))
    {
      if (nsEditor::IsInlineNode(curNode) && prevListItem)
      {
        // this is a continuation of some inline nodes that belong together in
        // the same list item.  use prevListItem
        PRUint32 listItemLen;
        res = mEditor->GetLengthOfDOMNode(prevListItem, listItemLen);
        if (NS_FAILED(res)) return res;
        res = mEditor->MoveNode(curNode, prevListItem, listItemLen);
        if (NS_FAILED(res)) return res;
      }
      else
      {
        // don't wrap li around a paragraph.  instead replace paragraph with li
        if (nsHTMLEditUtils::IsParagraph(curNode))
        {
          res = mEditor->ReplaceContainer(curNode, &listItem, "li");
        }
        else
        {
          res = mEditor->InsertContainerAbove(curNode, &listItem, "li");
        }
        if (NS_FAILED(res)) return res;
        if (nsEditor::IsInlineNode(curNode)) 
          prevListItem = listItem;
      }
    }
    else
    {
      listItem = curNode;
    }
  
    if (listItem)  // if we made a new list item, deal with it
    {
      // tuck the listItem into the end of the active list
      PRUint32 listLen;
      res = mEditor->GetLengthOfDOMNode(curList, listLen);
      if (NS_FAILED(res)) return res;
      res = mEditor->MoveNode(listItem, curList, listLen);
      if (NS_FAILED(res)) return res;
    }
  }

  return res;
}


nsresult
nsHTMLEditRules::WillRemoveList(nsIDOMSelection *aSelection, 
                                PRBool aOrdered, 
                                PRBool *aCancel,
                                PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_TRUE;

  nsAutoString blockType("ul");
  if (aOrdered) blockType = "ol";
  
  nsAutoSelectionReset selectionResetter(aSelection, mEditor);
  
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  nsresult res = GetPromotedRanges(aSelection, &arrayOfRanges, kMakeList);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, &arrayOfNodes, kMakeList);
  if (NS_FAILED(res)) return res;                                 
                                     
  // Remove all non-editable nodes.  Leave them be.
  
  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  for (i=listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> testNode( do_QueryInterface(isupports ) );
    if (!mEditor->IsEditable(testNode))
    {
      arrayOfNodes->RemoveElementAt(i);
    }
  }
  
  // Only act on lists or list items in the array
  nsCOMPtr<nsIDOMNode> curParent;
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;
    
    if (nsHTMLEditUtils::IsListItem(curNode))  // unlist this listitem
    {
      PRBool bOutOfList;
      do
      {
        res = PopListItem(curNode, &bOutOfList);
        if (NS_FAILED(res)) return res;
      } while (!bOutOfList); // keep popping it out until it's not in a list anymore
    }
    else if (nsHTMLEditUtils::IsList(curNode)) // node is a list, move list items out
    {
      nsCOMPtr<nsIDOMNode> child;
      curNode->GetLastChild(getter_AddRefs(child));

      while (child)
      {
        if (nsHTMLEditUtils::IsListItem(child))
        {
          PRBool bOutOfList;
          do
          {
            res = PopListItem(child, &bOutOfList);
            if (NS_FAILED(res)) return res;
          } while (!bOutOfList);   // keep popping it out until it's not in a list anymore
        }
        else
        {
          // delete any non- list items for now
          res = mEditor->DeleteNode(child);
          if (NS_FAILED(res)) return res;
        }
        curNode->GetLastChild(getter_AddRefs(child));
      }
      // delete the now-empty list
      res = mEditor->DeleteNode(curNode);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}


nsresult
nsHTMLEditRules::WillMakeBasicBlock(nsIDOMSelection *aSelection, 
                                    const nsString *aBlockType, 
                                    PRBool *aCancel,
                                    PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  
  nsresult res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;

  PRBool makeEmpty;
  res = ShouldMakeEmptyBlock(aSelection, aBlockType, &makeEmpty);
  if (NS_FAILED(res)) return res;
  
  if (makeEmpty) return res;  // just insert a new empty block
  
  // else it's not that easy...
  nsAutoSelectionReset selectionResetter(aSelection, mEditor);
  *aHandled = PR_TRUE;

  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(aSelection, &arrayOfRanges, kMakeBasicBlock);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, &arrayOfNodes, kMakeBasicBlock);
  if (NS_FAILED(res)) return res;                                 
                                     
  // Ok, now go through all the nodes and make the right kind of blocks, 
  // or whatever is approriate.  Wohoo!
  res = ApplyBlockStyle(arrayOfNodes, aBlockType);
  return res;
}


nsresult
nsHTMLEditRules::WillIndent(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool * aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  
  nsresult res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  *aHandled = PR_TRUE;

  nsAutoSelectionReset selectionResetter(aSelection, mEditor);
  
  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(aSelection, &arrayOfRanges, kIndent);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, &arrayOfNodes, kIndent);
  if (NS_FAILED(res)) return res;                                 
                                     
  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.
  
  nsVoidArray transitionList;
  res = MakeTransitionList(arrayOfNodes, &transitionList);
  if (NS_FAILED(res)) return res;                                 
  
  // Ok, now go through all the nodes and put them in a blockquote, 
  // or whatever is appropriate.  Wohoo!

  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curQuote;
  nsCOMPtr<nsIDOMNode> curList;
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;
    
    // some logic for putting list items into nested lists...
    if (nsHTMLEditUtils::IsList(curParent))
    {
      if (!curList || transitionList[i])
      {
        nsAutoString listTag;
        nsEditor::GetTagString(curParent,listTag);
        // create a new nested list of correct type
        res = mEditor->CreateNode(listTag, curParent, offset, getter_AddRefs(curList));
        if (NS_FAILED(res)) return res;
        // curList is now the correct thing to put curNode in
      }
      // tuck the node into the end of the active list
      PRUint32 listLen;
      res = mEditor->GetLengthOfDOMNode(curList, listLen);
      if (NS_FAILED(res)) return res;
      res = mEditor->MoveNode(curNode, curList, listLen);
      if (NS_FAILED(res)) return res;
    }
    
    else // not a list item, use blockquote
    {
      // need to make a blockquote to put things in if we haven't already,
      // or if this node doesn't go in blockquote we used earlier.
      if (!curQuote || transitionList[i])
      {
        nsAutoString quoteType("blockquote");
        if (mEditor->CanContainTag(curParent,quoteType))
        {
          res = mEditor->CreateNode(quoteType, curParent, offset, getter_AddRefs(curQuote));
          if (NS_FAILED(res)) return res;
          // set style to not have unwanted vertical margins
          nsAutoString attr("style"), attrval("margin: 0 0 0 40px;");
          nsCOMPtr<nsIDOMElement> quoteElem = do_QueryInterface(curQuote);
          res = mEditor->SetAttribute(quoteElem, attr, attrval);
          if (NS_FAILED(res)) return res;
          // curQuote is now the correct thing to put curNode in
        }
        else
        {
          printf("trying to put a blockquote in a bad place\n");     
        }
      }
        
      // tuck the node into the end of the active blockquote
      PRUint32 quoteLen;
      res = mEditor->GetLengthOfDOMNode(curQuote, quoteLen);
      if (NS_FAILED(res)) return res;
      res = mEditor->MoveNode(curNode, curQuote, quoteLen);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}


nsresult
nsHTMLEditRules::WillOutdent(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_TRUE;
  
  nsAutoSelectionReset selectionResetter(aSelection, mEditor);
  nsresult res = NS_OK;
  
  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(aSelection, &arrayOfRanges, kOutdent);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, &arrayOfNodes, kOutdent);
  if (NS_FAILED(res)) return res;                                 
                                     
  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.
  
  nsVoidArray transitionList;
  res = MakeTransitionList(arrayOfNodes, &transitionList);
  if (NS_FAILED(res)) return res;                                 
  
  // Ok, now go through all the nodes and remove a level of blockquoting, 
  // or whatever is appropriate.  Wohoo!

  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  nsCOMPtr<nsIDOMNode> curParent;
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;
    
    if (nsHTMLEditUtils::IsList(curParent))  // move node out of list
    {
      if (nsHTMLEditUtils::IsList(curNode))  // just unwrap this sublist
      {
        res = mEditor->RemoveContainer(curNode);
        if (NS_FAILED(res)) return res;
      }
      else  // we are moving a list item, but not whole list
      {
        PRBool bOutOfList;
        res = PopListItem(curNode, &bOutOfList);
        if (NS_FAILED(res)) return res;
      }
    }
    else if (nsHTMLEditUtils::IsList(curNode)) // node is a list, but parent is non-list: move list items out
    {
      nsCOMPtr<nsIDOMNode> child;
      curNode->GetLastChild(getter_AddRefs(child));
      while (child)
      {
        if (nsHTMLEditUtils::IsListItem(child))
        {
          PRBool bOutOfList;
          res = PopListItem(child, &bOutOfList);
          if (NS_FAILED(res)) return res;
        }
        else
        {
          // delete any non- list items for now
          res = mEditor->DeleteNode(child);
          if (NS_FAILED(res)) return res;
        }
        curNode->GetLastChild(getter_AddRefs(child));
      }
      // delete the now-empty list
      res = mEditor->DeleteNode(curNode);
      if (NS_FAILED(res)) return res;
    }
    else if (transitionList[i])  // not list related - look for enclosing blockquotes and remove
    {
      // look for a blockquote somewhere above us and remove it.
      // this is a hack until i think about outdent for real.
      nsCOMPtr<nsIDOMNode> n = curNode;
      nsCOMPtr<nsIDOMNode> tmp;
      while (!nsHTMLEditUtils::IsBody(n))
      {
        if (nsHTMLEditUtils::IsBlockquote(n))
        {
          mEditor->RemoveContainer(n);
          break;
        }
        n->GetParentNode(getter_AddRefs(tmp));
        n = tmp;
      }
    }
  }

  return res;
}


///////////////////////////////////////////////////////////////////////////
// IsEmptyBlock: figure out if aNode is (or is inside) an empty block.
//               A block can have children and still be considered empty,
//               if the children are empty or non-editable.
//                  
nsresult 
nsHTMLEditRules::IsEmptyBlock(nsIDOMNode *aNode, 
                              PRBool *outIsEmptyBlock, 
                              PRBool aMozBRDoesntCount,
                              PRBool aListItemsNotEmpty) 
{
  if (!aNode || !outIsEmptyBlock) return NS_ERROR_NULL_POINTER;
  *outIsEmptyBlock = PR_TRUE;
  
//  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> nodeToTest;
  if (nsEditor::IsBlockNode(aNode)) nodeToTest = do_QueryInterface(aNode);
//  else nsCOMPtr<nsIDOMElement> block;
//  looks like I forgot to finish this.  Wonder what I was going to do?

  if (!nodeToTest) return NS_ERROR_NULL_POINTER;
  return IsEmptyNode(nodeToTest, outIsEmptyBlock,
                     aMozBRDoesntCount, aListItemsNotEmpty);
}


///////////////////////////////////////////////////////////////////////////
// IsEmptyNode: figure out if aNode is an empty node.
//               A block can have children and still be considered empty,
//               if the children are empty or non-editable.
//                  
nsresult 
nsHTMLEditRules::IsEmptyNode( nsIDOMNode *aNode, 
                              PRBool *outIsEmptyNode, 
                              PRBool aMozBRDoesntCount,
                              PRBool aListItemsNotEmpty)
{
  if (!aNode || !outIsEmptyNode) return NS_ERROR_NULL_POINTER;
  *outIsEmptyNode = PR_TRUE;
  
  // effeciency hack - special case if it's a text node
  if (nsEditor::IsTextNode(aNode))
  {
    PRUint32 length = 0;
    nsCOMPtr<nsIDOMCharacterData>nodeAsText;
    nodeAsText = do_QueryInterface(aNode);
    nodeAsText->GetLength(&length);
    if (length) *outIsEmptyNode = PR_FALSE;
    return NS_OK;
  }

  // if it's not a text node (handled above) and it's not a container,
  // then we dont call it empty (it's an <hr>, or <br>, etc).
  // Also, if it's an anchor then dont treat it as empty - even though
  // anchors are containers, named anchors are "empty" but we don't
  // want to treat them as such.  Also, don't call ListItems or table
  // cells empty if caller desires.
  if (!mEditor->IsContainer(aNode) || nsHTMLEditUtils::IsAnchor(aNode) || 
       (aListItemsNotEmpty && nsHTMLEditUtils::IsListItem(aNode)) ||
       (aListItemsNotEmpty && nsHTMLEditUtils::IsTableCell(aNode)) ) 
  {
    *outIsEmptyNode = PR_FALSE;
    return NS_OK;
  }
  
  // iterate over node. if no children, or all children are either 
  // empty text nodes or non-editable, then node qualifies as empty
  nsCOMPtr<nsIContentIterator> iter;
  nsCOMPtr<nsIContent> nodeAsContent = do_QueryInterface(aNode);
  if (!nodeAsContent) return NS_ERROR_FAILURE;
  nsresult res = nsComponentManager::CreateInstance(kContentIteratorCID,
                                        nsnull,
                                        NS_GET_IID(nsIContentIterator), 
                                        getter_AddRefs(iter));
  if (NS_FAILED(res)) return res;
  res = iter->Init(nodeAsContent);
  if (NS_FAILED(res)) return res;
    
  while (NS_ENUMERATOR_FALSE == iter->IsDone())
  {
    nsCOMPtr<nsIDOMNode> node;
    nsCOMPtr<nsIContent> content;
    res = iter->CurrentNode(getter_AddRefs(content));
    if (NS_FAILED(res)) return res;
    node = do_QueryInterface(content);
    if (!node) return NS_ERROR_FAILURE;
 
    // is the node editable and non-empty?  if so, return false
    if (mEditor->IsEditable(node))
    {
      if (nsEditor::IsTextNode(node))
      {
        PRUint32 length = 0;
        nsCOMPtr<nsIDOMCharacterData>nodeAsText;
        nodeAsText = do_QueryInterface(node);
        nodeAsText->GetLength(&length);
        if (length) *outIsEmptyNode = PR_FALSE;
      }
      else  // an editable, non-text node. we aren't an empty block 
      {
        // is it the node we are iterating over?
        if (node.get() == aNode) break;
        // is it a moz-BR and did the caller ask us not to consider those relevant?
        if (!(aMozBRDoesntCount && nsHTMLEditUtils::IsMozBR(node))) 
        {
          // is it an empty node of some sort?
          PRBool isEmptyNode;
          res = IsEmptyNode(node, &isEmptyNode, aMozBRDoesntCount, aListItemsNotEmpty);
          if (NS_FAILED(res)) return res;
          if (!isEmptyNode) 
          {
            // otherwise it ain't empty
            *outIsEmptyNode = PR_FALSE;
            break;
          }
        }
      }
    }
    res = iter->Next();
    if (NS_FAILED(res)) return res;
  }
  
  return NS_OK;
}


nsresult
nsHTMLEditRules::WillAlign(nsIDOMSelection *aSelection, 
                           const nsString *alignType, 
                           PRBool *aCancel,
                           PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  
  nsresult res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  

  PRBool outMakeEmpty;
  res = ShouldMakeEmptyBlock(aSelection, alignType, &outMakeEmpty);
  if (NS_FAILED(res)) return res;
  if (outMakeEmpty) 
  {
    PRInt32 offset;
    nsCOMPtr<nsIDOMNode> brNode, parent, theDiv;
    nsAutoString divType("div");
    res = mEditor->GetStartNodeAndOffset(aSelection, &parent, &offset);
    if (NS_FAILED(res)) return res;
    res = mEditor->CreateNode(divType, parent, offset, getter_AddRefs(theDiv));
    if (NS_FAILED(res)) return res;
    // set up the alignment on the div
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(theDiv);
    nsAutoString attr("align");
    res = mEditor->SetAttribute(divElem, attr, *alignType);
    if (NS_FAILED(res)) return res;
    *aHandled = PR_TRUE;
    // put in a moz-br so that it won't get deleted
    res = CreateMozBR(theDiv, 0, &brNode);
    if (NS_FAILED(res)) return res;
    res = aSelection->Collapse(theDiv, 0);
    return res;
  }

  nsAutoSelectionReset selectionResetter(aSelection, mEditor);

  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  *aHandled = PR_TRUE;
  
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(aSelection, &arrayOfRanges, kAlign);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, &arrayOfNodes, kAlign);
  if (NS_FAILED(res)) return res;                                 
                                     
  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.
  
  nsVoidArray transitionList;
  res = MakeTransitionList(arrayOfNodes, &transitionList);
  if (NS_FAILED(res)) return res;                                 

  // Ok, now go through all the nodes and give them an align attrib or put them in a div, 
  // or whatever is appropriate.  Wohoo!
  
  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curDiv;
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;
    
    // if it's a div, don't nest it, just set the alignment
    if (nsHTMLEditUtils::IsDiv(curNode))
    {
      nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(curNode);
      nsAutoString attr("align");
      res = mEditor->SetAttribute(divElem, attr, *alignType);
      if (NS_FAILED(res)) return res;
      // clear out curDiv so that we don't put nodes after this one into it
      curDiv = 0;
      continue;
    }
    
    // if it's a table element (but not a table), forget any "current" div, and 
    // instead put divs inside any th's and td's inside the table element
    if (mEditor->IsTableElement(curNode) && !mEditor->IsTable(curNode))
    {
      res = AlignTableElement(curNode, alignType);
      if (NS_FAILED(res)) return res;
      // clear out curDiv so that we don't put nodes after this one into it
      curDiv = 0;
      continue;
    }      
    
    // need to make a div to put things in if we haven't already,
    // or if this node doesn't go in div we used earlier.
    if (!curDiv || transitionList[i])
    {
      nsAutoString divType("div");
      res = mEditor->CreateNode(divType, curParent, offset, getter_AddRefs(curDiv));
      if (NS_FAILED(res)) return res;
      // set up the alignment on the div
      nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(curDiv);
      nsAutoString attr("align");
      res = mEditor->SetAttribute(divElem, attr, *alignType);
      if (NS_FAILED(res)) return res;
      // curDiv is now the correct thing to put curNode in
    }
        
    // tuck the node into the end of the active div
    PRUint32 listLen;
    res = mEditor->GetLengthOfDOMNode(curDiv, listLen);
    if (NS_FAILED(res)) return res;
    res = mEditor->MoveNode(curNode, curDiv, listLen);
    if (NS_FAILED(res)) return res;
  }

  return res;
}


///////////////////////////////////////////////////////////////////////////
// AlignTableElement: take a table element and align it's contents
//                  
nsresult
nsHTMLEditRules::AlignTableElement(nsIDOMNode *aNode, const nsString *alignType)
{
  if (!aNode || !alignType) return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr<nsIContentIterator> iter;
  res = nsComponentManager::CreateInstance(kContentIteratorCID, nsnull,
                                            NS_GET_IID(nsIContentIterator), 
                                            getter_AddRefs(iter));
  if (NS_FAILED(res)) return res;
  if (!iter)          return NS_ERROR_FAILURE;

  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsISupports> isupports;
  
  // make a array
  res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
  if (NS_FAILED(res)) return res;
  
  // iterate node and build up array of td's and th's
  content = do_QueryInterface(aNode);
  iter->Init(content);
  while (NS_ENUMERATOR_FALSE == iter->IsDone())
  {
    res = iter->CurrentNode(getter_AddRefs(content));
    if (NS_FAILED(res)) return res;
    node = do_QueryInterface(content);
    if (!node) return NS_ERROR_FAILURE;
    if (mEditor->IsTableCell(node))
    {
      isupports = do_QueryInterface(node);
      arrayOfNodes->AppendElement(isupports);
    }
    res = iter->Next();
    if (NS_FAILED(res)) return res;
  }
  
  // now that we have the list, align their contents as requested
  PRUint32 listCount;
  PRUint32 j;
  arrayOfNodes->Count(&listCount);
  for (j = 0; j < listCount; j++)
  {
    isupports = (dont_AddRef)(arrayOfNodes->ElementAt(0));
    node = do_QueryInterface(isupports);
    res = AlignTableCellContents(node, alignType);
    if (NS_FAILED(res)) return res;
    arrayOfNodes->RemoveElementAt(0);
  }

  return res;  
}


///////////////////////////////////////////////////////////////////////////
// AlignTableElement: take a table cell and align it's contents
//                  
nsresult
nsHTMLEditRules::AlignTableCellContents(nsIDOMNode *aNode, const nsString *alignType)
{
  if (!aNode || !alignType) return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr <nsIDOMNode> firstChild, lastChild, divNode;
  
  res = GetFirstEditableChild(aNode, &firstChild);
  if (NS_FAILED(res)) return res;
  res = GetLastEditableChild(aNode, &lastChild);
  if (NS_FAILED(res)) return res;
  if (!firstChild)
  {
    // this cell has no content, nothing to align
  }
  else if ((firstChild==lastChild) && nsHTMLEditUtils::IsDiv(firstChild))
  {
    // the cell already has a div containing all of it's content: just
    // act on this div.
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(firstChild);
    nsAutoString attr("align");
    res = mEditor->SetAttribute(divElem, attr, *alignType);
    if (NS_FAILED(res)) return res;
  }
  else
  {
    // else we need to put in a div, set the alignment, and toss in al the children
    nsAutoString divType("div");
    res = mEditor->CreateNode(divType, aNode, 0, getter_AddRefs(divNode));
    if (NS_FAILED(res)) return res;
    // set up the alignment on the div
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(divNode);
    nsAutoString attr("align");
    res = mEditor->SetAttribute(divElem, attr, *alignType);
    if (NS_FAILED(res)) return res;
    // tuck the children into the end of the active div
    while (lastChild && (lastChild != divNode))
    {
      res = mEditor->MoveNode(lastChild, divNode, 0);
      if (NS_FAILED(res)) return res;
      res = GetLastEditableChild(aNode, &lastChild);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// GetTableContent: take a table element and list it's editable, non-table contents
//                  
nsresult
nsHTMLEditRules::GetTableContent(nsIDOMNode *aNode, nsCOMPtr<nsISupportsArray> *outArrayOfNodes)
{
  if (!aNode || !outArrayOfNodes) return NS_ERROR_NULL_POINTER;

  // make a array
  nsresult res = NS_NewISupportsArray(getter_AddRefs(*outArrayOfNodes));
  if (NS_FAILED(res)) return res;
  
  // make an iter
  nsCOMPtr<nsIContentIterator> iter;
  res = nsComponentManager::CreateInstance(kContentIteratorCID, nsnull,
                                            NS_GET_IID(nsIContentIterator), 
                                            getter_AddRefs(iter));
  if (NS_FAILED(res)) return res;
  if (!iter)          return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsISupports> isupports;
  
  // iterate node and build up array of non-table content
  content = do_QueryInterface(aNode);
  iter->Init(content);
  while (NS_ENUMERATOR_FALSE == iter->IsDone())
  {
    res = iter->CurrentNode(getter_AddRefs(content));
    if (NS_FAILED(res)) return res;
    node = do_QueryInterface(content);
    if (!node) return NS_ERROR_FAILURE;
    if (mEditor->IsTableCell(node))
    {
      nsCOMPtr <nsIDOMNode> child, tmp;
      res = GetFirstEditableChild(node, &child);
      if (NS_FAILED(res)) return res;
      while (child)
      {
        isupports = do_QueryInterface(child);
        (*outArrayOfNodes)->AppendElement(isupports);
        GetNextHTMLSibling(child, &tmp);
        child = tmp;
      }
    }
    res = iter->Next();
    if (NS_FAILED(res)) return res;
  }
  return res;
}

///////////////////////////////////////////////////////////////////////////
// IsFirstNode: Are we the first edittable node in our parent?
//                  
PRBool
nsHTMLEditRules::IsFirstNode(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset, j=0;
  nsresult res = nsEditor::GetNodeLocation(aNode, &parent, &offset);
  if (NS_FAILED(res)) 
  {
    NS_NOTREACHED("failure in nsHTMLEditRules::IsFirstNode");
    return PR_FALSE;
  }
  if (!offset)  // easy case, we are first dom child
    return PR_TRUE;
  if (!parent)  
    return PR_TRUE;
  
  // ok, so there are earlier children.  But are they editable???
  nsCOMPtr<nsIDOMNodeList> childList;
  nsCOMPtr<nsIDOMNode> child;

  res = parent->GetChildNodes(getter_AddRefs(childList));
  if (NS_FAILED(res) || !childList) 
  {
    NS_NOTREACHED("failure in nsHTMLEditUtils::IsFirstNode");
    return PR_TRUE;
  }
  while (j < offset)
  {
    childList->Item(j, getter_AddRefs(child));
    if (mEditor->IsEditable(child)) 
      return PR_FALSE;
    j++;
  }
  return PR_TRUE;
}


///////////////////////////////////////////////////////////////////////////
// IsLastNode: Are we the first edittable node in our parent?
//                  
PRBool
nsHTMLEditRules::IsLastNode(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset, j;
  PRUint32 numChildren;
  nsresult res = nsEditor::GetNodeLocation(aNode, &parent, &offset);
  if (NS_FAILED(res)) 
  {
    NS_NOTREACHED("failure in nsHTMLEditUtils::IsLastNode");
    return PR_FALSE;
  }
  nsEditor::GetLengthOfDOMNode(parent, numChildren); 
  if (offset+1 == (PRInt32)numChildren) // easy case, we are last dom child
    return PR_TRUE;
  if (!parent)
    return PR_TRUE;
    
  // ok, so there are later children.  But are they editable???
  j = offset+1;
  nsCOMPtr<nsIDOMNodeList>childList;
  nsCOMPtr<nsIDOMNode> child;
  res = parent->GetChildNodes(getter_AddRefs(childList));
  if (NS_FAILED(res) || !childList) 
  {
    NS_NOTREACHED("failure in nsHTMLEditRules::IsLastNode");
    return PR_TRUE;
  }
  while (j < (PRInt32)numChildren)
  {
    childList->Item(j, getter_AddRefs(child));
    if (mEditor->IsEditable(child)) 
      return PR_FALSE;
    j++;
  }
  return PR_TRUE;
}


///////////////////////////////////////////////////////////////////////////
// AtStartOfBlock: is node/offset at the start of the editable material in this block?
//                  
PRBool
nsHTMLEditRules::AtStartOfBlock(nsIDOMNode *aNode, PRInt32 aOffset, nsIDOMNode *aBlock)
{
  nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(aNode);
  if (nodeAsText && aOffset) return PR_FALSE;  // there are chars in front of us
  
  nsCOMPtr<nsIDOMNode> priorNode;
  nsresult  res = GetPriorHTMLNode(aNode, aOffset, &priorNode);
  if (NS_FAILED(res)) return PR_TRUE;
  if (!priorNode) return PR_TRUE;
  nsCOMPtr<nsIDOMNode> blockParent = mEditor->GetBlockNodeParent(priorNode);
  if (blockParent && (blockParent.get() == aBlock)) return PR_FALSE;
  return PR_TRUE;
}


///////////////////////////////////////////////////////////////////////////
// AtEndOfBlock: is node/offset at the end of the editable material in this block?
//                  
PRBool
nsHTMLEditRules::AtEndOfBlock(nsIDOMNode *aNode, PRInt32 aOffset, nsIDOMNode *aBlock)
{
  nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(aNode);
  if (nodeAsText)   
  {
    PRUint32 strLength;
    nodeAsText->GetLength(&strLength);
    if ((PRInt32)strLength > aOffset) return PR_FALSE;  // there are chars in after us
  }
  nsCOMPtr<nsIDOMNode> nextNode;
  nsresult  res = GetNextHTMLNode(aNode, aOffset, &nextNode);
  if (NS_FAILED(res)) return PR_TRUE;
  if (!nextNode) return PR_TRUE;
  nsCOMPtr<nsIDOMNode> blockParent = mEditor->GetBlockNodeParent(nextNode);
  if (blockParent && (blockParent.get() == aBlock)) return PR_FALSE;
  return PR_TRUE;
}


// not needed at moment - leaving around in case we go back to it.
#if 0
///////////////////////////////////////////////////////////////////////////
// CreateMozDiv: makes a div with type = _moz
//                       
nsresult
nsHTMLEditRules::CreateMozDiv(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outDiv)
{
  if (!inParent || !outDiv) return NS_ERROR_NULL_POINTER;
  nsAutoString divType= "div";
  *outDiv = nsnull;
  nsresult res = mEditor->CreateNode(divType, inParent, inOffset, getter_AddRefs(*outDiv));
  if (NS_FAILED(res)) return res;
  // give it special moz attr
  nsCOMPtr<nsIDOMElement> mozDivElem = do_QueryInterface(*outDiv);
  res = mEditor->SetAttribute(mozDivElem, "type", "_moz");
  if (NS_FAILED(res)) return res;
  res = AddTrailerBR(*outDiv);
  return res;
}
#endif    


///////////////////////////////////////////////////////////////////////////
// GetPromotedPoint: figure out where a start or end point for a block
//                   operation really is
nsresult
nsHTMLEditRules::GetPromotedPoint(RulesEndpoint aWhere, nsIDOMNode *aNode, PRInt32 aOffset, 
                                  PRInt32 actionID, nsCOMPtr<nsIDOMNode> *outNode, PRInt32 *outOffset)
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> node = aNode;
  nsCOMPtr<nsIDOMNode> parent = aNode;
  PRInt32 offset = aOffset;
  
  // we do one thing for InsertText actions, something else entirely for other actions
  if (actionID == kInsertText)
  {
    PRBool isSpace, isNBSP; 
    nsCOMPtr<nsIDOMNode> temp;   
    // for insert text or delete actions, we want to look backwards (or forwards, as appropriate)
    // for additional whitespace or nbsp's.  We may have to act on these later even though
    // they are outside of the initial selection.  Even if they are in another node!
    if (aWhere == kStart)
    {
      do
      {
        res = mEditor->IsPrevCharWhitespace(node, offset, &isSpace, &isNBSP, &temp, &offset);
        if (NS_FAILED(res)) return res;
        if (isSpace || isNBSP) node = temp;
        else break;
      } while (node);
  
      *outNode = node;
      *outOffset = offset;
    }
    else if (aWhere == kEnd)
    {
      do
      {
        res = mEditor->IsNextCharWhitespace(node, offset, &isSpace, &isNBSP, &temp, &offset);
        if (NS_FAILED(res)) return res;
        if (isSpace || isNBSP) node = temp;
        else break;
      } while (node);
  
      *outNode = node;
      *outOffset = offset;
    }
    return res;
  }
  
  // else not kInsertText.  In this case we want to see if we should
  // grab any adjacent inline nodes and/or parents and other ancestors
  if (nsHTMLEditUtils::IsBody(aNode))
  {
    // we cant go any higher
    *outNode = do_QueryInterface(aNode);
    *outOffset = aOffset;
    return res;
  }
  
  if (aWhere == kStart)
  {
    // some special casing for text nodes
    if (nsEditor::IsTextNode(aNode))  
    {
      res = nsEditor::GetNodeLocation(aNode, &parent, &offset);
      if (NS_FAILED(res)) return res;
    }
    else
    {
      node = nsEditor::GetChildAt(parent,offset);
      if (!node) node = parent;  
    }
    
    // if this is an inline node who's block parent is the body,
    // back up through any prior inline nodes that
    // aren't across a <br> from us.
    
    if (!nsEditor::IsBlockNode(node))
    {
      nsCOMPtr<nsIDOMNode> block = nsEditor::GetBlockNodeParent(node);
//    if (IsBody(block))
//    {
        nsCOMPtr<nsIDOMNode> prevNode;
        prevNode = nsEditor::NextNodeInBlock(node, nsEditor::kIterBackward);
        while (prevNode)
        {
          if (nsHTMLEditUtils::IsBreak(prevNode) && !nsHTMLEditUtils::HasMozAttr(prevNode))
            break;
          if (nsEditor::IsBlockNode(prevNode))
            break;
          node = prevNode;
          prevNode = nsEditor::NextNodeInBlock(node, nsEditor::kIterBackward);
        }
//    }
//    else
//    {
        // just grap the whole block
//      node = block;
//    }
    }
    
    // finding the real start for this point.  look up the tree for as long as we are the 
    // first node in the container, and as long as we haven't hit the body node.
    res = nsEditor::GetNodeLocation(node, &parent, &offset);
    if (NS_FAILED(res)) return res;
    while ((IsFirstNode(node)) && (!nsHTMLEditUtils::IsBody(parent)))
    {
      node = parent;
      res = nsEditor::GetNodeLocation(node, &parent, &offset);
      if (NS_FAILED(res)) return res;
    } 
    *outNode = parent;
    *outOffset = offset;
    return res;
  }
  
  if (aWhere == kEnd)
  {
    // some special casing for text nodes
    if (nsEditor::IsTextNode(aNode))  
    {
      res = nsEditor::GetNodeLocation(aNode, &parent, &offset);
      if (NS_FAILED(res)) return res;
    }
    else
    {
      node = nsEditor::GetChildAt(parent,offset);
      if (!node) node = parent;  
    }

    if (!node)
      node = parent;
    
    // if this is an inline node who's block parent is the body, 
    // look ahead through any further inline nodes that
    // aren't across a <br> from us, and that are enclosed in the same block.
    
    if (!nsEditor::IsBlockNode(node))
    {
      nsCOMPtr<nsIDOMNode> block = nsEditor::GetBlockNodeParent(node);
//    if (IsBody(block))
//    {
        nsCOMPtr<nsIDOMNode> nextNode;
        nextNode = nsEditor::NextNodeInBlock(node, nsEditor::kIterForward);
        while (nextNode)
        {
          if (nsHTMLEditUtils::IsBreak(nextNode) && !nsHTMLEditUtils::HasMozAttr(nextNode))
            break;
          if (nsEditor::IsBlockNode(nextNode))
            break;
          node = nextNode;
          nextNode = nsEditor::NextNodeInBlock(node, nsEditor::kIterForward);
        }
//    }
//    else
//    {
        // just grap the whole block
//      node = block;
//    }
    }
    
    // finding the real end for this point.  look up the tree for as long as we are the 
    // last node in the container, and as long as we haven't hit the body node.
    res = nsEditor::GetNodeLocation(node, &parent, &offset);
    if (NS_FAILED(res)) return res;
    while ((IsLastNode(node)) && (!nsHTMLEditUtils::IsBody(parent)))
    {
      node = parent;
      res = nsEditor::GetNodeLocation(node, &parent, &offset);
      if (NS_FAILED(res)) return res;
    } 
    *outNode = parent;
    offset++;  // add one since this in an endpoint - want to be AFTER node.
    *outOffset = offset;
    return res;
  }
  
  return res;
}


///////////////////////////////////////////////////////////////////////////
// GetPromotedRanges: run all the selection range endpoint through 
//                    GetPromotedPoint()
//                       
nsresult 
nsHTMLEditRules::GetPromotedRanges(nsIDOMSelection *inSelection, 
                                   nsCOMPtr<nsISupportsArray> *outArrayOfRanges, 
                                   PRInt32 inOperationType)
{
  if (!inSelection || !outArrayOfRanges) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_NewISupportsArray(getter_AddRefs(*outArrayOfRanges));
  if (NS_FAILED(res)) return res;
  
  PRInt32 rangeCount;
  res = inSelection->GetRangeCount(&rangeCount);
  if (NS_FAILED(res)) return res;
  
  PRInt32 i;
  nsCOMPtr<nsIDOMRange> selectionRange;
  nsCOMPtr<nsIDOMRange> opRange;

  for (i = 0; i < rangeCount; i++)
  {
    res = inSelection->GetRangeAt(i, getter_AddRefs(selectionRange));
    if (NS_FAILED(res)) return res;

    // clone range so we dont muck with actual selection ranges
    res = selectionRange->Clone(getter_AddRefs(opRange));
    if (NS_FAILED(res)) return res;

    // make a new adjusted range to represent the appropriate block content.
    // The basic idea is to push out the range endpoints
    // to truly enclose the blocks that we will affect.
    // This call alters opRange.
    res = PromoteRange(opRange, inOperationType);
    if (NS_FAILED(res)) return res;
      
    // stuff new opRange into nsISupportsArray
    nsCOMPtr<nsISupports> isupports = do_QueryInterface(opRange);
    (*outArrayOfRanges)->AppendElement(isupports);
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// PromoteRange: expand a range to include any parents for which all
//               editable children are already in range. 
//                       
nsresult 
nsHTMLEditRules::PromoteRange(nsIDOMRange *inRange, 
                              PRInt32 inOperationType)
{
  if (!inRange) return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset, endOffset;
  
  res = inRange->GetStartParent(getter_AddRefs(startNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetStartOffset(&startOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndParent(getter_AddRefs(endNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndOffset(&endOffset);
  if (NS_FAILED(res)) return res;
  
  // make a new adjusted range to represent the appropriate block content.
  // this is tricky.  the basic idea is to push out the range endpoints
  // to truly enclose the blocks that we will affect
  
  nsCOMPtr<nsIDOMNode> opStartNode;
  nsCOMPtr<nsIDOMNode> opEndNode;
  PRInt32 opStartOffset, opEndOffset;
  nsCOMPtr<nsIDOMRange> opRange;
  
  res = GetPromotedPoint( kStart, startNode, startOffset, inOperationType, &opStartNode, &opStartOffset);
  if (NS_FAILED(res)) return res;
  res = GetPromotedPoint( kEnd, endNode, endOffset, inOperationType, &opEndNode, &opEndOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->SetStart(opStartNode, opStartOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->SetEnd(opEndNode, opEndOffset);
  return res;
} 

///////////////////////////////////////////////////////////////////////////
// GetNodesForOperation: run through the ranges in the array and construct 
//                       a new array of nodes to be acted on.
//                       
nsresult 
nsHTMLEditRules::GetNodesForOperation(nsISupportsArray *inArrayOfRanges, 
                                   nsCOMPtr<nsISupportsArray> *outArrayOfNodes, 
                                   PRInt32 inOperationType)
{
  if (!inArrayOfRanges || !outArrayOfNodes) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_NewISupportsArray(getter_AddRefs(*outArrayOfNodes));
  if (NS_FAILED(res)) return res;
  
  PRUint32 rangeCount;
  res = inArrayOfRanges->Count(&rangeCount);
  if (NS_FAILED(res)) return res;
  
  PRInt32 i;
  nsCOMPtr<nsIDOMRange> opRange;
  nsCOMPtr<nsISupports> isupports;
  nsCOMPtr<nsIContentIterator> iter;

  for (i = 0; i < (PRInt32)rangeCount; i++)
  {
    isupports = (dont_AddRef)(inArrayOfRanges->ElementAt(i));
    opRange = do_QueryInterface(isupports);
    res = nsComponentManager::CreateInstance(kSubtreeIteratorCID,
                                        nsnull,
                                        NS_GET_IID(nsIContentIterator), 
                                        getter_AddRefs(iter));
    if (NS_FAILED(res)) return res;
    res = iter->Init(opRange);
    if (NS_FAILED(res)) return res;
    
    while (NS_ENUMERATOR_FALSE == iter->IsDone())
    {
      nsCOMPtr<nsIDOMNode> node;
      nsCOMPtr<nsIContent> content;
      res = iter->CurrentNode(getter_AddRefs(content));
      if (NS_FAILED(res)) return res;
      node = do_QueryInterface(content);
      if (!node) return NS_ERROR_FAILURE;
      isupports = do_QueryInterface(node);
      (*outArrayOfNodes)->AppendElement(isupports);
      res = iter->Next();
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}



///////////////////////////////////////////////////////////////////////////
// GetChildNodesForOperation: 
//                       
nsresult 
nsHTMLEditRules::GetChildNodesForOperation(nsIDOMNode *inNode, 
                                   nsCOMPtr<nsISupportsArray> *outArrayOfNodes)
{
  if (!inNode || !outArrayOfNodes) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_NewISupportsArray(getter_AddRefs(*outArrayOfNodes));
  if (NS_FAILED(res)) return res;
  
  nsCOMPtr<nsIDOMNodeList> childNodes;
  res = inNode->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(res)) return res;
  if (!childNodes) return NS_ERROR_NULL_POINTER;
  PRUint32 childCount;
  res = childNodes->GetLength(&childCount);
  if (NS_FAILED(res)) return res;
  
  PRUint32 i;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsISupports> isupports;
  for (i = 0; i < childCount; i++)
  {
    res = childNodes->Item( i, getter_AddRefs(node));
    if (!node) return NS_ERROR_FAILURE;
    isupports = do_QueryInterface(node);
    (*outArrayOfNodes)->AppendElement(isupports);
    if (NS_FAILED(res)) return res;
  }
  return res;
}



///////////////////////////////////////////////////////////////////////////
// MakeTransitionList: detect all the transitions in the array, where a 
//                     transition means that adjacent nodes in the array 
//                     don't have the same parent.
//                       
nsresult 
nsHTMLEditRules::MakeTransitionList(nsISupportsArray *inArrayOfNodes, 
                                   nsVoidArray *inTransitionArray)
{
  if (!inArrayOfNodes || !inTransitionArray) return NS_ERROR_NULL_POINTER;

  PRUint32 listCount;
  PRInt32 i;
  inArrayOfNodes->Count(&listCount);
  nsVoidArray transitionList;
  nsCOMPtr<nsIDOMNode> prevElementParent;
  nsCOMPtr<nsIDOMNode> curElementParent;
  
  for (i=0; i<(PRInt32)listCount; i++)
  {
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(inArrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> transNode( do_QueryInterface(isupports ) );
    transNode->GetParentNode(getter_AddRefs(curElementParent));
    if (curElementParent != prevElementParent)
    {
      // different parents, or seperated by <br>: transition point
      inTransitionArray->InsertElementAt((void*)PR_TRUE,i);  
    }
    else
    {
      // same parents: these nodes grew up together
      inTransitionArray->InsertElementAt((void*)PR_FALSE,i); 
    }
    prevElementParent = curElementParent;
  }
  return NS_OK;
}



/********************************************************
 *  main implementation methods 
 ********************************************************/
 
///////////////////////////////////////////////////////////////////////////
// InsertTab: top level logic for determining how to insert a tab
//                       
nsresult 
nsHTMLEditRules::InsertTab(nsIDOMSelection *aSelection, 
                           nsString *outString)
{
  nsCOMPtr<nsIDOMNode> parentNode;
  PRInt32 offset;
  PRBool isPRE;
  
  nsresult res = mEditor->GetStartNodeAndOffset(aSelection, &parentNode, &offset);
  if (NS_FAILED(res)) return res;
  
  if (!parentNode) return NS_ERROR_FAILURE;
    
  res = mEditor->IsPreformatted(parentNode, &isPRE);
  if (NS_FAILED(res)) return res;
    
  if (isPRE)
  {
    *outString = '\t';
  }
  else
  {
    // number of spaces should be a pref?
    // note that we dont play around with nbsps here anymore.  
    // let the AfterEdit whitespace cleanup code handle it.
    *outString = "    ";
  }
  
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// ReturnInHeader: do the right thing for returns pressed in headers
//                       
nsresult 
nsHTMLEditRules::ReturnInHeader(nsIDOMSelection *aSelection, 
                                nsIDOMNode *aHeader, 
                                nsIDOMNode *aNode, 
                                PRInt32 aOffset)
{
  if (!aSelection || !aHeader || !aNode) return NS_ERROR_NULL_POINTER;  
  
  // remeber where the header is
  nsCOMPtr<nsIDOMNode> headerParent;
  PRInt32 offset;
  nsresult res = nsEditor::GetNodeLocation(aHeader, &headerParent, &offset);
  if (NS_FAILED(res)) return res;

  // split the header
  PRInt32 newOffset;
  res = mEditor->SplitNodeDeep( aHeader, aNode, aOffset, &newOffset);
  if (NS_FAILED(res)) return res;

  // if the leftand heading is empty, put a mozbr in it
  nsCOMPtr<nsIDOMNode> prevItem;
  GetPriorHTMLSibling(aHeader, &prevItem);
  if (prevItem && nsHTMLEditUtils::IsHeader(prevItem))
  {
    PRBool bIsEmptyNode;
    res = IsEmptyNode(prevItem, &bIsEmptyNode);
    if (NS_FAILED(res)) return res;
    if (bIsEmptyNode)
    {
      nsCOMPtr<nsIDOMNode> brNode;
      res = CreateMozBR(prevItem, 0, &brNode);
      if (NS_FAILED(res)) return res;
    }
  }
  
  // if the new (righthand) header node is empty, delete it
  PRBool isEmpty;
  res = IsEmptyBlock(aHeader, &isEmpty, PR_TRUE);
  if (NS_FAILED(res)) return res;
  if (isEmpty)
  {
    res = mEditor->DeleteNode(aHeader);
    if (NS_FAILED(res)) return res;
    // layout tells the caret to blink in a weird place
    // if we dont place a break after the header.
    nsCOMPtr<nsIDOMNode> sibling;
    res = GetNextHTMLSibling(headerParent, offset+1, &sibling);
    if (NS_FAILED(res)) return res;
    if (!sibling || !nsHTMLEditUtils::IsBreak(sibling))
    {
      res = CreateMozBR(headerParent, offset+1, &sibling);
      if (NS_FAILED(res)) return res;
    }
    res = nsEditor::GetNodeLocation(sibling, &headerParent, &offset);
    if (NS_FAILED(res)) return res;
    // put selection after break
    res = aSelection->Collapse(headerParent,offset+1);
  }
  else
  {
    // put selection at front of righthand heading
    res = aSelection->Collapse(aHeader,0);
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// ReturnInParagraph: do the right thing for returns pressed in paragraphs
//                       
nsresult 
nsHTMLEditRules::ReturnInParagraph(nsIDOMSelection *aSelection, 
                                   nsIDOMNode *aPara, 
                                   nsIDOMNode *aNode, 
                                   PRInt32 aOffset,
                                   PRBool *aCancel,
                                   PRBool *aHandled)
{
  if (!aSelection || !aPara || !aNode || !aCancel || !aHandled) 
    { return NS_ERROR_NULL_POINTER; }
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;

  nsCOMPtr<nsIDOMNode> sibling;
  nsresult res = NS_OK;

  // easy case, in a text node:
  if (mEditor->IsTextNode(aNode))
  {
    nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aNode);
    PRUint32 strLength;
    res = textNode->GetLength(&strLength);
    if (NS_FAILED(res)) return res;
    
    // at beginning of text node?
    if (!aOffset)
    {
      // is there a BR prior to it?
      GetPriorHTMLSibling(aNode, &sibling);
      if (!sibling) 
      {
        // no previous sib, so
        // just fall out to default of inserting a BR
        return res;
      }
      if (nsHTMLEditUtils::IsBreak(sibling)
          && !nsHTMLEditUtils::HasMozAttr(sibling))
      {
        PRInt32 newOffset;
        *aCancel = PR_TRUE;
        // split the paragraph
        res = mEditor->SplitNodeDeep( aPara, aNode, aOffset, &newOffset);
        if (NS_FAILED(res)) return res;
        // get rid of the break
        res = mEditor->DeleteNode(sibling);  
        if (NS_FAILED(res)) return res;
        // position selection inside right hand para
        res = aSelection->Collapse(aPara,0);
      }
      // else just fall out to default of inserting a BR
      return res;
    }
    // at end of text node?
    if (aOffset == (PRInt32)strLength)
    {
      // is there a BR after to it?
      res = GetNextHTMLSibling(aNode, &sibling);
      if (!sibling) 
      {
        // no next sib, so
        // just fall out to default of inserting a BR
        return res;
      }
      if (nsHTMLEditUtils::IsBreak(sibling)
          && !nsHTMLEditUtils::HasMozAttr(sibling))
      {
        PRInt32 newOffset;
        *aCancel = PR_TRUE;
        // split the paragraph
        res = mEditor->SplitNodeDeep(aPara, aNode, aOffset, &newOffset);
        if (NS_FAILED(res)) return res;
        // get rid of the break
        res = mEditor->DeleteNode(sibling);  
        if (NS_FAILED(res)) return res;
        // position selection inside right hand para
        res = aSelection->Collapse(aPara,0);
      }
      // else just fall out to default of inserting a BR
      return res;
    }
    // inside text node
    // just fall out to default of inserting a BR
    return res;
  }
  else
  {
    // not in a text node.  
    // is there a BR prior to it?
    nsCOMPtr<nsIDOMNode> nearNode;
    res = GetPriorHTMLNode(aNode, aOffset, &nearNode);
    if (NS_FAILED(res)) return res;
    if (!nearNode || !nsHTMLEditUtils::IsBreak(nearNode)
        || nsHTMLEditUtils::HasMozAttr(nearNode)) 
    {
      // is there a BR after to it?
      res = GetNextHTMLNode(aNode, aOffset, &nearNode);
      if (NS_FAILED(res)) return res;
      if (!nearNode || !nsHTMLEditUtils::IsBreak(nearNode)
          || nsHTMLEditUtils::HasMozAttr(nearNode)) 
      {
        // just fall out to default of inserting a BR
        return res;
      }
    }
    // else remove sibling br and split para
    PRInt32 newOffset;
    *aCancel = PR_TRUE;
    // split the paragraph
    res = mEditor->SplitNodeDeep( aPara, aNode, aOffset, &newOffset);
    if (NS_FAILED(res)) return res;
    // get rid of the break
    res = mEditor->DeleteNode(nearNode);  
    if (NS_FAILED(res)) return res;
    // selection to beginning of right hand para
    aSelection->Collapse(aPara,0);
  }
  
  return res;
}



///////////////////////////////////////////////////////////////////////////
// ReturnInListItem: do the right thing for returns pressed in list items
//                       
nsresult 
nsHTMLEditRules::ReturnInListItem(nsIDOMSelection *aSelection, 
                                  nsIDOMNode *aListItem, 
                                  nsIDOMNode *aNode, 
                                  PRInt32 aOffset)
{
  if (!aSelection || !aListItem || !aNode) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> listitem;
  
  // sanity check
  NS_PRECONDITION(PR_TRUE == nsHTMLEditUtils::IsListItem(aListItem),
                  "expected a list item and didnt get one");
  
  // if we are in an empty listitem, then we want to pop up out of the list
  PRBool isEmpty;
  res = IsEmptyBlock(aListItem, &isEmpty, PR_TRUE, PR_FALSE);
  if (NS_FAILED(res)) return res;
  if (isEmpty)
  {
    nsCOMPtr<nsIDOMNode> list, listparent;
    PRInt32 offset, itemOffset;
    res = nsEditor::GetNodeLocation(aListItem, &list, &itemOffset);
    if (NS_FAILED(res)) return res;
    res = nsEditor::GetNodeLocation(list, &listparent, &offset);
    if (NS_FAILED(res)) return res;
    
    // are we the last list item in the list?
    PRBool bIsLast;
    res = IsLastEditableChild(aListItem, &bIsLast);
    if (NS_FAILED(res)) return res;
    if (!bIsLast)
    {
      // we need to split the list!
      nsCOMPtr<nsIDOMNode> tempNode;
      res = mEditor->SplitNode(list, itemOffset, getter_AddRefs(tempNode));
      if (NS_FAILED(res)) return res;
    }
    // are we in a sublist?
    if (nsHTMLEditUtils::IsList(listparent))  //in a sublist
    {
      // if so, move this list item out of this list and into the grandparent list
      res = mEditor->MoveNode(aListItem,listparent,offset+1);
      if (NS_FAILED(res)) return res;
      res = aSelection->Collapse(aListItem,0);
    }
    else
    {
      // otherwise kill this listitem
      res = mEditor->DeleteNode(aListItem);
      if (NS_FAILED(res)) return res;
      
      // time to insert a break
      nsCOMPtr<nsIDOMNode> brNode;
      res = CreateMozBR(listparent, offset+1, &brNode);
      if (NS_FAILED(res)) return res;
      
      // set selection to before the moz br
      aSelection->SetHint(PR_TRUE);
      res = aSelection->Collapse(listparent,offset+1);
    }
    return res;
  }
  
  // else we want a new list item at the same list level
  PRInt32 newOffset;
  res = mEditor->SplitNodeDeep( aListItem, aNode, aOffset, &newOffset);
  if (NS_FAILED(res)) return res;
  // hack: until I can change the damaged doc range code back to being
  // extra inclusive, I have to manually detect certain list items that
  // may be left empty.
  nsCOMPtr<nsIDOMNode> prevItem;
  GetPriorHTMLSibling(aListItem, &prevItem);
  if (prevItem && nsHTMLEditUtils::IsListItem(prevItem))
  {
    PRBool bIsEmptyNode;
    res = IsEmptyNode(prevItem, &bIsEmptyNode);
    if (NS_FAILED(res)) return res;
    if (bIsEmptyNode)
    {
      nsCOMPtr<nsIDOMNode> brNode;
      res = CreateMozBR(prevItem, 0, &brNode);
      if (NS_FAILED(res)) return res;
    }
  }
  res = aSelection->Collapse(aListItem,0);
  return res;
}


///////////////////////////////////////////////////////////////////////////
// ShouldMakeEmptyBlock: determine if a block transformation should make 
//                       a new empty block, or instead transform a block
//                       
nsresult 
nsHTMLEditRules::ShouldMakeEmptyBlock(nsIDOMSelection *aSelection, 
                                      const nsString *blockTag, 
                                      PRBool *outMakeEmpty)
{
  // a note about strategy:
  // this routine will be called by the rules code to figure out
  // if it should do something, or let the nsHTMLEditor default
  // action happen.  The default action is to insert a new block.
  // Note that if _nothing_ should happen, ie, the selection is
  // already entireyl inside a block (or blocks) or the correct type,
  // then you don't want to return true in outMakeEmpty, since the
  // defualt code will insert a new empty block anyway, rather than
  // doing nothing.  So we have to detect that case and return false.
  
  if (!aSelection || !outMakeEmpty) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  
  // if the selection is collapsed, and
  // if we in the body, or after a <br> with
  // no more inline content before the next block, 
  // or in an empty block (empty td, li, etc), then we want
  // a new block.  Otherwise we want to trasform a block
  
  // xxx possible bug: selection could be not callapsed, but
  // still empty.  it would be nice to have a call for this: IsEmptySelection()
  
  PRBool isCollapsed;
  res = aSelection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;
  if (isCollapsed) 
  {
    nsCOMPtr<nsIDOMNode> parent;
    PRInt32 offset;
    res = nsEditor::GetStartNodeAndOffset(aSelection, &parent, &offset);
    if (NS_FAILED(res)) return res;
    
    // is selection point in the body?
    if (nsHTMLEditUtils::IsBody(parent))
    {
      *outMakeEmpty = PR_TRUE;
      return res;
    }
    
    // see if block parent is already right kind of block.  
    // See strategy comment above.
    nsCOMPtr<nsIDOMNode> block;
    if (!nsEditor::IsBlockNode(parent))
      block = nsEditor::GetBlockNodeParent(parent);
    else
      block = parent;
    if (block)
    {
      nsAutoString tag;
      nsEditor::GetTagString(block,tag);
      if (tag == *blockTag)
      {
        *outMakeEmpty = PR_FALSE;
        return res;
      }
    }
    
    // are we in a textnode or inline node?
    if (!nsEditor::IsBlockNode(parent))
    {
      // we must be in a text or inline node - convert existing block
      *outMakeEmpty = PR_FALSE;
      return res;
    }
    
    // in an empty block?
    PRBool bIsEmptyNode;
    // the PR_TRUE param tell IsEmptyNode to not count moz-BRs as content
    res = IsEmptyNode(block, &bIsEmptyNode, PR_TRUE, PR_FALSE);
    if (bIsEmptyNode)
    {
      // we must be in a text or inline node - convert existing block
      *outMakeEmpty = PR_TRUE;
      return res;
    }    
    
    // is it after a <br> with no inline nodes after it, or a <br> after it??
    if (offset)
    {
      nsCOMPtr<nsIDOMNode> prevChild, nextChild, tmp;
      prevChild = nsEditor::GetChildAt(parent, offset-1);
      while (prevChild && !mEditor->IsEditable(prevChild))
      {
        // search back until we either find an editable node, 
        // or hit the beginning of the block
        tmp = nsEditor::NextNodeInBlock(prevChild, nsEditor::kIterBackward);
        prevChild = tmp;
      }
      
      if (prevChild && nsHTMLEditUtils::IsBreak(prevChild)) 
      {
        nextChild = nsEditor::GetChildAt(parent, offset);
        while (nextChild && !mEditor->IsEditable(nextChild))
        {
          // search back until we either find an editable node, 
          // or hit the beginning of the block
          tmp = nsEditor::NextNodeInBlock(nextChild, nsEditor::kIterForward);
          nextChild = tmp;
        }
        if (!nextChild || nsHTMLEditUtils::IsBreak(nextChild)
            || nsEditor::IsBlockNode(nextChild))
        {
          // we are after a <br> and not before inline content,
          // or we are between <br>s.
          // make an empty block
          *outMakeEmpty = PR_FALSE;
          return res;
        }
      }
    }
  }    
  // otherwise transform an existing block
  *outMakeEmpty = PR_FALSE;
  return res;
}


///////////////////////////////////////////////////////////////////////////
// ApplyBlockStyle:  do whatever it takes to make the list of nodes into 
//                   one or more blocks of type blockTag.  
//                       
nsresult 
nsHTMLEditRules::ApplyBlockStyle(nsISupportsArray *arrayOfNodes, const nsString *aBlockTag)
{
  // intent of this routine is to be used for converting to/from
  // headers, paragraphs, pre, and address.  Those blocks
  // that pretty much just contain inline things...
  
  if (!arrayOfNodes || !aBlockTag) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> curNode, curParent, curBlock, newBlock;
  PRInt32 offset;
  PRUint32 listCount;
  PRBool bNoParent = PR_FALSE;
  
  // we special case an empty tag name to mean "remove block parents".
  // This is used for the "normal" paragraph style in mail-compose
  if (aBlockTag->IsEmpty() || *aBlockTag=="normal") bNoParent = PR_TRUE;
  
  arrayOfNodes->Count(&listCount);
  
  PRUint32 i;
  for (i=0; i<listCount; i++)
  {
    // get the node to act on, and it's location
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    curNode = do_QueryInterface(isupports);
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;
    nsAutoString curNodeTag;
    nsEditor::GetTagString(curNode, curNodeTag);
        
 
    // is it already the right kind of block?
    if (!bNoParent && curNodeTag == *aBlockTag)
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      continue;  // do nothing to this block
    }
        
    // if curNode is a mozdiv, p, header, address, or pre, replace 
    // it with a new block of correct type.
    // xxx floppy moose: pre cant hold everything the others can
    if (nsHTMLEditUtils::IsMozDiv(curNode)     ||
        (curNodeTag == "pre") || 
        (curNodeTag == "p")   ||
        (curNodeTag == "h1")  ||
        (curNodeTag == "h2")  ||
        (curNodeTag == "h3")  ||
        (curNodeTag == "h4")  ||
        (curNodeTag == "h5")  ||
        (curNodeTag == "h6")  ||
        (curNodeTag == "address"))
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      if (bNoParent)
      {
//        nsCOMPtr<nsIDOMNode> brNode;
//        res = mEditor->CreateBR(curParent, offset+1, &brNode);
//        if (NS_FAILED(res)) return res;
        res = mEditor->RemoveContainer(curNode); 
      }
      else
      {
        res = mEditor->ReplaceContainer(curNode, &newBlock, *aBlockTag);
      }
      if (NS_FAILED(res)) return res;
    }
    else if ((curNodeTag == "table")      || 
             (curNodeTag == "tbody")      ||
             (curNodeTag == "tr")         ||
             (curNodeTag == "td")         ||
             (curNodeTag == "ol")         ||
             (curNodeTag == "ul")         ||
             (curNodeTag == "li")         ||
             (curNodeTag == "blockquote") ||
             (curNodeTag == "div"))  // div's other than mozdivs
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      // recursion time
      nsCOMPtr<nsISupportsArray> childArray;
      res = GetChildNodesForOperation(curNode, &childArray);
      if (NS_FAILED(res)) return res;
      res = ApplyBlockStyle(childArray, aBlockTag);
      if (NS_FAILED(res)) return res;
    }
    
    // if the node is a break, we honor it by putting further nodes in a new parent
    else if (curNodeTag == "br")
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      if (!bNoParent)
      {
        res = mEditor->DeleteNode(curNode);
        if (NS_FAILED(res)) return res;
      }
    }
        
    
    // if curNode is inline, pull it into curBlock
    // note: it's assumed that consecutive inline nodes in the 
    // arrayOfNodes are actually members of the same block parent.
    // this happens to be true now as a side effect of how
    // arrayOfNodes is contructed, but some additional logic should
    // be added here if that should change
    
    else if (nsEditor::IsInlineNode(curNode) && !bNoParent)
    {
      // if curNode is a non editable, drop it if we are going to <pre>
      if ((*aBlockTag == "pre") && (!mEditor->IsEditable(curNode)))
        continue; // do nothing to this block
      
      // if no curBlock, make one
      if (!curBlock)
      {
        res = mEditor->CreateNode(*aBlockTag, curParent, offset, getter_AddRefs(curBlock));
        if (NS_FAILED(res)) return res;
      }
      
      // if curNode is a Break, replace it with a return if we are going to <pre>
      // xxx floppy moose
 
      // this is a continuation of some inline nodes that belong together in
      // the same block item.  use curBlock
      PRUint32 blockLen;
      res = mEditor->GetLengthOfDOMNode(curBlock, blockLen);
      if (NS_FAILED(res)) return res;
      res = mEditor->MoveNode(curNode, curBlock, blockLen);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}




///////////////////////////////////////////////////////////////////////////
// JoinNodesSmart:  join two nodes, doing whatever makes sense for their  
//                  children (which often means joining them, too).
//                  aNodeLeft & aNodeRight must be same type of node.
nsresult 
nsHTMLEditRules::JoinNodesSmart( nsIDOMNode *aNodeLeft, 
                                 nsIDOMNode *aNodeRight, 
                                 nsCOMPtr<nsIDOMNode> *aOutMergeParent, 
                                 PRInt32 *aOutMergeOffset)
{
  // check parms
  if (!aNodeLeft ||  
      !aNodeRight || 
      !aOutMergeParent ||
      !aOutMergeOffset) 
    return NS_ERROR_NULL_POINTER;
  
  nsresult res = NS_OK;
  // caller responsible for:
  //   left & right node are same type
  PRInt32 parOffset;
  nsCOMPtr<nsIDOMNode> parent, rightParent;
  res = nsEditor::GetNodeLocation(aNodeLeft, &parent, &parOffset);
  if (NS_FAILED(res)) return res;
  aNodeRight->GetParentNode(getter_AddRefs(rightParent));

  // if they don't have the same parent, first move the 'right' node 
  // to after the 'left' one
  if (parent != rightParent)
  {
    res = mEditor->MoveNode(aNodeRight, parent, parOffset);
    if (NS_FAILED(res)) return res;
  }
  
  // defaults for outParams
  *aOutMergeParent = aNodeRight;
  res = mEditor->GetLengthOfDOMNode(aNodeLeft, *((PRUint32*)aOutMergeOffset));
  if (NS_FAILED(res)) return res;

  // seperate join rules for differing blocks
  if (nsHTMLEditUtils::IsParagraph(aNodeLeft))
  {
    // for para's, merge deep & add a <br> after merging
    res = mEditor->JoinNodeDeep(aNodeLeft, aNodeRight, aOutMergeParent, aOutMergeOffset);
    if (NS_FAILED(res)) return res;
    // now we need to insert a br.  
    nsCOMPtr<nsIDOMNode> brNode;
    res = mEditor->CreateBR(*aOutMergeParent, *aOutMergeOffset, &brNode);
    if (NS_FAILED(res)) return res;
    res = nsEditor::GetNodeLocation(brNode, aOutMergeParent, aOutMergeOffset);
    if (NS_FAILED(res)) return res;
    (*aOutMergeOffset)++;
    return res;
  }
  else if (nsHTMLEditUtils::IsList(aNodeLeft)
           || mEditor->IsTextNode(aNodeLeft))
  {
    // for list's, merge shallow (wouldn't want to combine list items)
    res = mEditor->JoinNodes(aNodeLeft, aNodeRight, parent);
    if (NS_FAILED(res)) return res;
    return res;
  }
  else
  {
    // remember the last left child, and firt right child
    nsCOMPtr<nsIDOMNode> lastLeft, firstRight;
    res = GetLastEditableChild(aNodeLeft, &lastLeft);
    if (NS_FAILED(res)) return res;
    res = GetFirstEditableChild(aNodeRight, &firstRight);
    if (NS_FAILED(res)) return res;

    // for list items, divs, etc, merge smart
    res = mEditor->JoinNodes(aNodeLeft, aNodeRight, parent);
    if (NS_FAILED(res)) return res;

    if (lastLeft && firstRight && mEditor->NodesSameType(lastLeft, firstRight))
    {
      return JoinNodesSmart(lastLeft, firstRight, aOutMergeParent, aOutMergeOffset);
    }
  }
  return res;
}


nsresult 
nsHTMLEditRules::GetTopEnclosingMailCite(nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutCiteNode)
{
  // check parms
  if (!aNode || !aOutCiteNode) 
    return NS_ERROR_NULL_POINTER;
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> node, parentNode;
  node = do_QueryInterface(aNode);
  
  while (node)
  {
    if (nsHTMLEditUtils::IsMailCite(node)) *aOutCiteNode = node;
    if (nsHTMLEditUtils::IsBody(node)) break;
    
    res = node->GetParentNode(getter_AddRefs(parentNode));
    if (NS_FAILED(res)) return res;
    node = parentNode;
  }

  return res;
}


nsresult 
nsHTMLEditRules::AdjustSpecialBreaks()
{
  nsCOMPtr<nsIContentIterator> iter;
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsCOMPtr<nsISupports> isupports;
  PRUint32 nodeCount,j;
  
  // make an isupportsArray to hold a list of nodes
  nsresult res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
  if (NS_FAILED(res)) return res;

  // need an iterator
  res = nsComponentManager::CreateInstance(kContentIteratorCID,
                                        nsnull,
                                        NS_GET_IID(nsIContentIterator), 
                                        getter_AddRefs(iter));
  if (NS_FAILED(res)) return res;
  
  // loop over iter and create list of empty containers
  res = iter->Init(mDocChangeRange);
  if (NS_FAILED(res)) return res;
  
  // gather up a list of empty nodes
  while (NS_ENUMERATOR_FALSE == iter->IsDone())
  {
    nsCOMPtr<nsIDOMNode> node;
    nsCOMPtr<nsIContent> content;
    res = iter->CurrentNode(getter_AddRefs(content));
    if (NS_FAILED(res)) return res;
    node = do_QueryInterface(content);
    if (!node) return NS_ERROR_FAILURE;
    
    PRBool bIsEmptyNode;
    res = IsEmptyNode(node, &bIsEmptyNode, PR_FALSE, PR_FALSE);
    if (NS_FAILED(res)) return res;
    if (bIsEmptyNode
        && (nsHTMLEditUtils::IsListItem(node) || mEditor->IsTableCell(node)))
    {
      isupports = do_QueryInterface(node);
      arrayOfNodes->AppendElement(isupports);
    }
    
    res = iter->Next();
    if (NS_FAILED(res)) return res;
  }
  
  // put moz-br's into these empty li's and td's
  res = arrayOfNodes->Count(&nodeCount);
  if (NS_FAILED(res)) return res;
  for (j = 0; j < nodeCount; j++)
  {
    isupports = (dont_AddRef)(arrayOfNodes->ElementAt(0));
    nsCOMPtr<nsIDOMNode> brNode, theNode( do_QueryInterface(isupports ) );
    arrayOfNodes->RemoveElementAt(0);
    res = CreateMozBR(theNode, 0, &brNode);
    if (NS_FAILED(res)) return res;
  }
  
  return res;
}


nsresult 
nsHTMLEditRules::AdjustWhitespace(nsIDOMSelection *aSelection)
{
  nsCOMPtr<nsIContentIterator> iter;
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsCOMPtr<nsISupports> isupports;
  PRUint32 nodeCount,j;
  nsresult res;

  nsAutoSelectionReset selectionResetter(aSelection, mEditor);
  
  // special case for mDocChangeRange entirely in one text node.
  // This is an efficiency hack for normal typing in the editor.
  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset, endOffset;
  res = mDocChangeRange->GetStartParent(getter_AddRefs(startNode));
  if (NS_FAILED(res)) return res;
  res = mDocChangeRange->GetStartOffset(&startOffset);
  if (NS_FAILED(res)) return res;
  res = mDocChangeRange->GetEndParent(getter_AddRefs(endNode));
  if (NS_FAILED(res)) return res;
  res = mDocChangeRange->GetEndOffset(&endOffset);
  if (NS_FAILED(res)) return res;

  if (startNode == endNode)
  {
    nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
    if (nodeAsText)
    {
      res = DoTextNodeWhitespace(nodeAsText, startOffset, endOffset);
      return res;
    }
  }
  
  // make an isupportsArray to hold a list of nodes
  res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
  if (NS_FAILED(res)) return res;

  // need an iterator
  res = nsComponentManager::CreateInstance(kContentIteratorCID,
                                        nsnull,
                                        NS_GET_IID(nsIContentIterator), 
                                        getter_AddRefs(iter));
  if (NS_FAILED(res)) return res;
  
  // loop over iter and adjust whitespace in any text nodes we find
  res = iter->Init(mDocChangeRange);
  if (NS_FAILED(res)) return res;
  
  // gather up a list of text nodes
  while (NS_ENUMERATOR_FALSE == iter->IsDone())
  {
    nsCOMPtr<nsIDOMNode> node;
    nsCOMPtr<nsIContent> content;
    res = iter->CurrentNode(getter_AddRefs(content));
    if (NS_FAILED(res)) return res;
    node = do_QueryInterface(content);
    if (!node) return NS_ERROR_FAILURE;
    
    if (nsEditor::IsTextNode(node) && mEditor->IsEditable(node)) 
    {
      isupports = do_QueryInterface(node);
      arrayOfNodes->AppendElement(isupports);
    }
    
    res = iter->Next();
    if (NS_FAILED(res)) return res;
  }
  
  // now adjust whitespace on node we found
  res = arrayOfNodes->Count(&nodeCount);
  if (NS_FAILED(res)) return res;
  for (j = 0; j < nodeCount; j++)
  {
    isupports = (dont_AddRef)(arrayOfNodes->ElementAt(0));
    nsCOMPtr<nsIDOMCharacterData> textNode( do_QueryInterface(isupports ) );
    arrayOfNodes->RemoveElementAt(0);
    res = DoTextNodeWhitespace(textNode, -1, -1);
    if (NS_FAILED(res)) return res;
  }
  
  return res;
}


nsresult 
nsHTMLEditRules::AdjustSelection(nsIDOMSelection *aSelection, nsIEditor::EDirection aAction)
{
  if (!aSelection) return NS_ERROR_NULL_POINTER;
  
  // if the selection isn't collapsed, do nothing.
  // moose: one thing to do instead is check for the case of
  // only a single break selected, and collapse it.  Good thing?  Beats me.
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
  // 1) that block is same block where selection is AND
  // 2) in a collapsed selection AND
  // 3) after a normal (non-moz) br AND
  // 4) that br is the last editable node in it's block 

  nsCOMPtr<nsIDOMNode> nearNode;
  res = GetPriorHTMLNode(selNode, selOffset, &nearNode);
  if (NS_FAILED(res)) return res;
  if (!nearNode) return res;
  
  // is nearNode also a descendant of same block?
  nsCOMPtr<nsIDOMNode> block, nearBlock;
  if (mEditor->IsBlockNode(selNode)) block = selNode;
  else block = mEditor->GetBlockNodeParent(selNode);
  nearBlock = mEditor->GetBlockNodeParent(nearNode);
  if (block == nearBlock)
  {
    if (nearNode && nsHTMLEditUtils::IsBreak(nearNode)
        && !nsHTMLEditUtils::IsMozBR(nearNode))
    {
      PRBool bIsLast;
      res = IsLastEditableChild(nearNode, &bIsLast);
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
        // selection stays *before* moz-br, sticking to it
        aSelection->SetHint(PR_TRUE);
        res = aSelection->Collapse(selNode,selOffset);
        if (NS_FAILED(res)) return res;
      }
      else
      {
        // ok, the br inst the last child.  
        // the br might be right in front of a new block (ie,:
        // <body> text<br> <ol><li>list item</li></ol></body>  )
        // in this case we also need moz-br.
        nsCOMPtr<nsIDOMNode> nextNode;
        res = GetNextHTMLNode(nearNode, &nextNode);
        if (NS_FAILED(res)) return res;
        res = GetNextHTMLSibling(nearNode, &nextNode);
        if (NS_FAILED(res)) return res;
        if (nextNode && mEditor->IsBlockNode(nextNode))
        {
          // need to insert special moz BR. Why?  Because if we don't
          // the user will see no new line for the break.  
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
    }
  }

  // we aren't in a textnode: are we adjacent to a break or an image?
  res = GetPriorHTMLSibling(selNode, selOffset, &nearNode);
  if (NS_FAILED(res)) return res;
  if (nearNode && (nsHTMLEditUtils::IsBreak(nearNode)
                   || nsHTMLEditUtils::IsImage(nearNode)))
    return NS_OK; // this is a good place for the caret to be
  res = GetNextHTMLSibling(selNode, selOffset, &nearNode);
  if (NS_FAILED(res)) return res;
  if (nearNode && (nsHTMLEditUtils::IsBreak(nearNode)
                   || nsHTMLEditUtils::IsImage(nearNode)))
    return NS_OK; // this is a good place for the caret to be

  // look for a nearby text node.
  // prefer the correct direction.
  res = FindNearSelectableNode(selNode, selOffset, aAction, &nearNode);
  if (NS_FAILED(res)) return res;

  if (!nearNode) return NS_OK; // couldn't find a near text node

  // is the nearnode a text node?
  textNode = do_QueryInterface(nearNode);
  if (textNode)
  {
    PRInt32 offset = 0;
    // put selection in right place:
    if (aAction == nsIEditor::ePrevious)
      textNode->GetLength((PRUint32*)&offset);
    res = aSelection->Collapse(nearNode,offset);
  }
  else  // must be break or image
  {
    res = nsEditor::GetNodeLocation(nearNode, &selNode, &selOffset);
    if (NS_FAILED(res)) return res;
    if (aAction == nsIEditor::ePrevious) selOffset++;  // want to be beyond it if we backed up to it
    res = aSelection->Collapse(selNode, selOffset);
  }

  return res;
}

nsresult
nsHTMLEditRules::FindNearSelectableNode(nsIDOMNode *aSelNode, 
                                        PRInt32 aSelOffset, 
                                        nsIEditor::EDirection aDirection,
                                        nsCOMPtr<nsIDOMNode> *outSelectableNode)
{
  if (!aSelNode || !outSelectableNode) return NS_ERROR_NULL_POINTER;
  *outSelectableNode = nsnull;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> nearNode, curNode;
  if (aDirection == nsIEditor::ePrevious)
    res = GetPriorHTMLNode(aSelNode, aSelOffset, &nearNode);
  else
    res = GetNextHTMLNode(aSelNode, aSelOffset, &nearNode);
  if (NS_FAILED(res)) return res;
  
  // scan in the right direction until we find an eligible text node,
  // but dont cross any breaks, images, or table elements.
  while (nearNode && !(mEditor->IsTextNode(nearNode)
                       || nsHTMLEditUtils::IsBreak(nearNode)
                       || nsHTMLEditUtils::IsImage(nearNode)))
  {
    // dont cross any table elements
    if (mEditor->IsTableElement(nearNode))
      return NS_OK;  

    curNode = nearNode;
    if (aDirection == nsIEditor::ePrevious)
      res = GetPriorHTMLNode(curNode, &nearNode);
    else
      res = GetNextHTMLNode(curNode, &nearNode);
    if (NS_FAILED(res)) return res;
  }
  
  if (nearNode) *outSelectableNode = do_QueryInterface(nearNode);
  return res;
}


nsresult 
nsHTMLEditRules::RemoveEmptyNodes()
{
  nsCOMPtr<nsIContentIterator> iter;
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsCOMPtr<nsISupports> isupports;
  PRUint32 nodeCount,j;
  
  // make an isupportsArray to hold a list of nodes
  nsresult res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
  if (NS_FAILED(res)) return res;

  // need an iterator
  res = nsComponentManager::CreateInstance(kContentIteratorCID,
                                        nsnull,
                                        NS_GET_IID(nsIContentIterator), 
                                        getter_AddRefs(iter));
  if (NS_FAILED(res)) return res;
  
  // loop over iter and create list of empty containers
  do
  {
    res = iter->Init(mDocChangeRange);
    if (NS_FAILED(res)) return res;
    
    // gather up a list of empty nodes
    while (NS_ENUMERATOR_FALSE == iter->IsDone())
    {
      nsCOMPtr<nsIDOMNode> node;
      nsCOMPtr<nsIContent> content;
      res = iter->CurrentNode(getter_AddRefs(content));
      if (NS_FAILED(res)) return res;
      node = do_QueryInterface(content);
      if (!node) return NS_ERROR_FAILURE;
      
      PRBool bIsEmptyNode;
      res = IsEmptyNode(node, &bIsEmptyNode, PR_FALSE, PR_TRUE);
      if (NS_FAILED(res)) return res;
      if (bIsEmptyNode && !nsHTMLEditUtils::IsBody(node))
      {
        if (nsHTMLEditUtils::IsParagraph(node) ||
            nsHTMLEditUtils::IsHeader(node)    ||
            nsHTMLEditUtils::IsListItem(node)  ||
            nsHTMLEditUtils::IsBlockquote(node)||
            nsHTMLEditUtils::IsPre(node)       ||
            nsHTMLEditUtils::IsAddress(node) )
        {
          // if it is one of these, dont delete if sel inside.
          // this is so we can create empty headings, etc, for the
          // user to type into.
          PRBool bIsSelInNode;
          res = SelectionEndpointInNode(node, &bIsSelInNode);
          if (NS_FAILED(res)) return res;
          if (!bIsSelInNode)
          {
            isupports = do_QueryInterface(node);
            arrayOfNodes->AppendElement(isupports);
          }
        }
        else
        {
          // if it's not such an element, delete it even if sel is inside
          isupports = do_QueryInterface(node);
          arrayOfNodes->AppendElement(isupports);
        }
      }
      
      res = iter->Next();
      if (NS_FAILED(res)) return res;
    }
    
    // now delete the empty nodes
    res = arrayOfNodes->Count(&nodeCount);
    if (NS_FAILED(res)) return res;
    for (j = 0; j < nodeCount; j++)
    {
      isupports = (dont_AddRef)(arrayOfNodes->ElementAt(0));
      nsCOMPtr<nsIDOMNode> delNode( do_QueryInterface(isupports ) );
      arrayOfNodes->RemoveElementAt(0);
      res = mEditor->DeleteNode(delNode);
      if (NS_FAILED(res)) return res;
    }
    
  } while (nodeCount);  // if we deleted any, loop again
                        // deleting some nodes may make some parents now empty
  return res;
}

nsresult
nsHTMLEditRules::SelectionEndpointInNode(nsIDOMNode *aNode, PRBool *aResult)
{
  if (!aNode || !aResult) return NS_ERROR_NULL_POINTER;
  
  *aResult = PR_FALSE;
  
  nsCOMPtr<nsIDOMSelection>selection;
  nsresult res = mEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(res)) return res;
  if (!enumerator) return NS_ERROR_UNEXPECTED;

  for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
  {
    nsCOMPtr<nsISupports> currentItem;
    res = enumerator->CurrentItem(getter_AddRefs(currentItem));
    if (NS_FAILED(res)) return res;
    if (!currentItem) return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    nsCOMPtr<nsIDOMNode> startParent, endParent;
    range->GetStartParent(getter_AddRefs(startParent));
    if (startParent)
    {
      if (aNode == startParent.get())
      {
        *aResult = PR_TRUE;
        return NS_OK;
      }
      if (nsHTMLEditUtils::IsDescendantOf(startParent, aNode)) 
      {
        *aResult = PR_TRUE;
        return NS_OK;
      }
    }
    range->GetEndParent(getter_AddRefs(endParent));
    if (startParent == endParent) continue;
    if (endParent)
    {
      if (aNode == endParent.get()) 
      {
        *aResult = PR_TRUE;
        return NS_OK;
      }
      if (nsHTMLEditUtils::IsDescendantOf(endParent, aNode))
      {
        *aResult = PR_TRUE;
        return NS_OK;
      }
    }
  }
  return res;
}


nsresult 
nsHTMLEditRules::DoTextNodeWhitespace(nsIDOMCharacterData *aTextNode, PRInt32 aStart, PRInt32 aEnd)
{
  // check parms
  if (!aTextNode) return NS_ERROR_NULL_POINTER;
  if (aStart == -1)  // -1 means do the whole darn node please
  {
    aStart = 0;
    aTextNode->GetLength((PRUint32*)&aEnd);
  }
  if (aStart == aEnd) return NS_OK;
  
  nsresult res = NS_OK;
  PRBool isPRE;
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aTextNode);
  
  res = mEditor->IsPreformatted(node,&isPRE);
  if (NS_FAILED(res)) return res;
    
  if (isPRE)
  {
    // text node has a Preformatted style. All we need to do is strip out any nbsp's 
    // we just put in and replace them with spaces.
  
    // moose: write me!
    return res;
  }
  
  // else we are not preformatted.  Need to convert any adjacent spaces to alterating
  // space/nbsp pairs; need to convert stranded nbsp's to spaces; need to convert
  // tabs to whitespace; need to convert returns to whitespace.
  PRInt32 j = 0;
  nsAutoString tempString;

  aTextNode->SubstringData(aStart, aEnd, tempString);
  
  // identify runs of whitespace
  PRInt32 runStart = -1, runEnd = -1;
  do {
    PRUnichar c = tempString[j];
    PRBool isSpace = nsCRT::IsAsciiSpace(c);
    if (isSpace || c==nbsp)
    {
      if (runStart<0) runStart = j;
      runEnd = j+1;
    }
    // translation of below line:
    // if we have a whitespace run, AND
    // either we are at the end of it, or the end of the whole string,
    // THEN process it
    if (runStart>=0 && (!(isSpace || c==nbsp)  ||  (j==aEnd-1)) )
    {
      // current char is non whitespace, but we have identified an earlier
      // run of whitespace.  convert it if needed.
      NS_PRECONDITION(runEnd>runStart, "this is what happens when integers turn bad!");
      // runStart to runEnd is a run of whitespace
      nsAutoString runStr, newStr;
      tempString.Mid(runStr, runStart, runEnd-runStart);
      
      res = ConvertWhitespace(runStr, newStr);
      if (NS_FAILED(res)) return res;
      
      if (runStr != newStr)
      {
        // delete the original whitespace run
        EditTxn *txn;
        // note 1: we are not telling edit listeners about these because they don't care
        // note 2: we are not wrapping these in a placeholder because we know they already are
        res = mEditor->CreateTxnForDeleteText(aTextNode, aStart+runStart, runEnd-runStart, (DeleteTextTxn**)&txn);
        if (NS_FAILED(res))  return res; 
        if (!txn)  return NS_ERROR_OUT_OF_MEMORY;
        res = mEditor->Do(txn); 
        if (NS_FAILED(res))  return res; 
        // The transaction system (if any) has taken ownwership of txn
        NS_IF_RELEASE(txn);
        
        // insert the new run
        res = mEditor->CreateTxnForInsertText(newStr, aTextNode, aStart+runStart, (InsertTextTxn**)&txn);
        if (NS_FAILED(res))  return res; 
        if (!txn)  return NS_ERROR_OUT_OF_MEMORY;
        res = mEditor->Do(txn);
        // The transaction system (if any) has taken ownwership of txns.
        NS_IF_RELEASE(txn);
      } 
      runStart = -1; // reset our run     
    }
    j++; // next char please!
  } while (j<aEnd);

  return res;
}


nsresult 
nsHTMLEditRules::ConvertWhitespace(const nsString & inString, nsString & outString)
{
  PRUint32 j,len = inString.Length();
  switch (len)
  {
    case 0:
      outString = "";
      return NS_OK;
    case 1:
      if (inString == "\n")   // a bit of a hack: don't convert single newlines that 
        outString = "\n";     // dont have whitespace adjacent.  This is to preserve 
      else                    // html source formatting to some degree.
        outString = " ";
      return NS_OK;
    case 2:
      outString = (PRUnichar)nbsp;
      outString += " ";
      return NS_OK;
    case 3:
      outString = " ";
      outString += (PRUnichar)nbsp;
      outString += " ";
      return NS_OK;
  }
  if (len%2)  // length is odd
  {
    for (j=0;j<len;j++)
    {
      if (!(j%2)) outString += " ";      // even char
      else outString += (PRUnichar)nbsp; // odd char
    }
  }
  else
  {
    outString = " ";
    outString += (PRUnichar)nbsp;
    outString += (PRUnichar)nbsp;
    for (j=0;j<len-3;j++)
    {
      if (!(j%2)) outString += " ";      // even char
      else outString += (PRUnichar)nbsp; // odd char
    }
  }  
  return NS_OK;
}


nsresult 
nsHTMLEditRules::PopListItem(nsIDOMNode *aListItem, PRBool *aOutOfList)
{
  // check parms
  if (!aListItem || !aOutOfList) 
    return NS_ERROR_NULL_POINTER;
  
  // init out params
  *aOutOfList = PR_FALSE;
  
  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(aListItem));
  PRInt32 offset;
  nsresult res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
  if (NS_FAILED(res)) return res;
    
  if (!nsHTMLEditUtils::IsListItem(curNode))
    return NS_ERROR_FAILURE;
    
  // if it's first or last list item, dont need to split the list
  // otherwise we do.
  nsCOMPtr<nsIDOMNode> curParPar;
  PRInt32 parOffset;
  res = nsEditor::GetNodeLocation(curParent, &curParPar, &parOffset);
  if (NS_FAILED(res)) return res;
  
  PRBool bIsFirstListItem;
  res = IsFirstEditableChild(curNode, &bIsFirstListItem);
  if (NS_FAILED(res)) return res;

  PRBool bIsLastListItem;
  res = IsLastEditableChild(curNode, &bIsLastListItem);
  if (NS_FAILED(res)) return res;
    
  if (!bIsFirstListItem && !bIsLastListItem)
  {
    // split the list
    nsCOMPtr<nsIDOMNode> newBlock;
    res = mEditor->SplitNode(curParent, offset, getter_AddRefs(newBlock));
    if (NS_FAILED(res)) return res;
  }
  
  if (!bIsFirstListItem) parOffset++;
  
  res = mEditor->MoveNode(curNode, curParPar, parOffset);
  if (NS_FAILED(res)) return res;
    
  // unwrap list item contents if they are no longer in a list
  if (!nsHTMLEditUtils::IsList(curParPar)
      && nsHTMLEditUtils::IsListItem(curNode)) 
  {
    nsCOMPtr<nsIDOMNode> lastChild;
    res = GetLastEditableChild(curNode, &lastChild);
    if (NS_FAILED(res)) return res;
    res = mEditor->RemoveContainer(curNode);
    if (NS_FAILED(res)) return res;
    if (mEditor->IsInlineNode(lastChild) && !(nsHTMLEditUtils::IsBreak(lastChild)))
    {
      // last thing inside of the listitem wasn't a block node.
      // insert a BR to preserve the illusion of block boundaries
      nsCOMPtr<nsIDOMNode> node, brNode;
      PRInt32 theOffset;
      res = nsEditor::GetNodeLocation(lastChild, &node, &theOffset);
      if (NS_FAILED(res)) return res;
      // last ePrevious param causes selection to be set before the break
      res = mEditor->CreateBR(node, theOffset+1, &brNode, nsIEditor::ePrevious);
      if (NS_FAILED(res)) return res;
    }
    *aOutOfList = PR_TRUE;
  }
  return res;
}


nsresult 
nsHTMLEditRules::ConfirmSelectionInBody()
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMElement> bodyElement;   
  nsCOMPtr<nsIDOMNode> bodyNode; 
  
  // get the body  
  res = mEditor->GetBodyElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  if (!bodyElement) return NS_ERROR_UNEXPECTED;
  bodyNode = do_QueryInterface(bodyElement);

  // get the selection
  nsCOMPtr<nsIDOMSelection>selection;
  res = mEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  
  // get the selection start location
  nsCOMPtr<nsIDOMNode> selNode, temp, parent;
  PRInt32 selOffset;
  res = mEditor->GetStartNodeAndOffset(selection, &selNode, &selOffset);
  if (NS_FAILED(res)) return res;
  temp = selNode;
  
  // check that selNode is inside body
  while (temp && !nsHTMLEditUtils::IsBody(temp))
  {
    res = temp->GetParentNode(getter_AddRefs(parent));
    temp = parent;
  }
  
  // if we aren't in the body, force the issue
  if (!temp) 
  {
//    uncomment this to see when we get bad selections
//    NS_NOTREACHED("selection not in body");
    selection->Collapse(bodyNode,0);
  }
  
  // get the selection end location
  res = mEditor->GetEndNodeAndOffset(selection, &selNode, &selOffset);
  if (NS_FAILED(res)) return res;
  temp = selNode;
  
  // check that selNode is inside body
  while (temp && !nsHTMLEditUtils::IsBody(temp))
  {
    res = temp->GetParentNode(getter_AddRefs(parent));
    temp = parent;
  }
  
  // if we aren't in the body, force the issue
  if (!temp) 
  {
//    uncomment this to see when we get bad selections
//    NS_NOTREACHED("selection not in body");
    selection->Collapse(bodyNode,0);
  }
  
  return res;
}


nsresult 
nsHTMLEditRules::UpdateDocChangeRange(nsIDOMRange *aRange)
{
  nsresult res = NS_OK;
  
  if (!mDocChangeRange)
  {
    mDocChangeRange = do_QueryInterface(aRange);
    return NS_OK;
  }
  else
  {
    PRInt32 result;
    
    // compare starts of ranges
    res = mDocChangeRange->CompareEndPoints(nsIDOMRange::START_TO_START, aRange, &result);
    if (NS_FAILED(res)) return res;
    if (result < 0)  // negative result means aRange start is before mDocChangeRange start
    {
      nsCOMPtr<nsIDOMNode> startNode;
      PRInt32 startOffset;
      res = aRange->GetStartParent(getter_AddRefs(startNode));
      if (NS_FAILED(res)) return res;
      res = aRange->GetStartOffset(&startOffset);
      if (NS_FAILED(res)) return res;
      res = mDocChangeRange->SetStart(startNode, startOffset);
      if (NS_FAILED(res)) return res;
    }
    
    // compare ends of ranges
    res = mDocChangeRange->CompareEndPoints(nsIDOMRange::END_TO_END, aRange, &result);
    if (NS_FAILED(res)) return res;
    if (result > 0)  // positive result means aRange end is after mDocChangeRange end
    {
      nsCOMPtr<nsIDOMNode> endNode;
      PRInt32 endOffset;
      res = aRange->GetEndParent(getter_AddRefs(endNode));
      if (NS_FAILED(res)) return res;
      res = aRange->GetEndOffset(&endOffset);
      if (NS_FAILED(res)) return res;
      res = mDocChangeRange->SetEnd(endNode, endOffset);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}



PRBool 
nsHTMLEditRules::IsDescendantOfBody(nsIDOMNode *inNode) 
{
  if (!inNode) return PR_FALSE;
  if (inNode == mBody.get()) return PR_TRUE;
  
  nsCOMPtr<nsIDOMNode> parent, node = do_QueryInterface(inNode);
  nsresult res;
  
  do
  {
    res = node->GetParentNode(getter_AddRefs(parent));
    if (NS_FAILED(res)) return PR_FALSE;
    if (parent == mBody) return PR_TRUE;
    node = parent;
  } while (parent);
  
  return PR_FALSE;
}



#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditActionListener methods 
#pragma mark -
#endif

NS_IMETHODIMP 
nsHTMLEditRules::WillCreateNode(const nsString& aTag, nsIDOMNode *aParent, PRInt32 aPosition)
{
  return NS_OK;  
}

NS_IMETHODIMP 
nsHTMLEditRules::DidCreateNode(const nsString& aTag, 
                               nsIDOMNode *aNode, 
                               nsIDOMNode *aParent, 
                               PRInt32 aPosition, 
                               nsresult aResult)
{
  if (!mListenerEnabled) return NS_OK;
  // assumption that Join keeps the righthand node
  nsresult res = mUtilRange->SelectNode(aNode);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aPosition)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidInsertNode(nsIDOMNode *aNode, 
                               nsIDOMNode *aParent, 
                               PRInt32 aPosition, 
                               nsresult aResult)
{
  if (!mListenerEnabled) return NS_OK;
  nsresult res = mUtilRange->SelectNode(aNode);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillDeleteNode(nsIDOMNode *aChild)
{
  if (!mListenerEnabled) return NS_OK;
  nsresult res = mUtilRange->SelectNode(aChild);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidDeleteNode(nsIDOMNode *aChild, nsresult aResult)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillSplitNode(nsIDOMNode *aExistingRightNode, PRInt32 aOffset)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidSplitNode(nsIDOMNode *aExistingRightNode, 
                              PRInt32 aOffset, 
                              nsIDOMNode *aNewLeftNode, 
                              nsresult aResult)
{
  if (!mListenerEnabled) return NS_OK;
  nsresult res = mUtilRange->SetStart(aExistingRightNode, 0);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetEnd(aExistingRightNode, 0);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillJoinNodes(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode, nsIDOMNode *aParent)
{
  if (!mListenerEnabled) return NS_OK;
  // remember split point
  nsresult res = nsEditor::GetLengthOfDOMNode(aLeftNode, mJoinOffset);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidJoinNodes(nsIDOMNode  *aLeftNode, 
                              nsIDOMNode *aRightNode, 
                              nsIDOMNode *aParent, 
                              nsresult aResult)
{
  if (!mListenerEnabled) return NS_OK;
  // assumption that Join keeps the righthand node
  nsresult res = mUtilRange->SetStart(aRightNode, mJoinOffset);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetEnd(aRightNode, mJoinOffset);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsString &aString)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidInsertText(nsIDOMCharacterData *aTextNode, 
                                  PRInt32 aOffset, 
                                  const nsString &aString, 
                                  nsresult aResult)
{
  if (!mListenerEnabled) return NS_OK;
  PRInt32 length = aString.Length();
  nsCOMPtr<nsIDOMNode> theNode = do_QueryInterface(aTextNode);
  nsresult res = mUtilRange->SetStart(theNode, aOffset);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetEnd(theNode, aOffset+length);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, PRInt32 aLength)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidDeleteText(nsIDOMCharacterData *aTextNode, 
                                  PRInt32 aOffset, 
                                  PRInt32 aLength, 
                                  nsresult aResult)
{
  if (!mListenerEnabled) return NS_OK;
  nsCOMPtr<nsIDOMNode> theNode = do_QueryInterface(aTextNode);
  nsresult res = mUtilRange->SetStart(theNode, aOffset);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetEnd(theNode, aOffset);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}

NS_IMETHODIMP
nsHTMLEditRules::WillDeleteSelection(nsIDOMSelection *aSelection)
{
  if (!mListenerEnabled) return NS_OK;
  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;

  nsresult res = mEditor->GetStartNodeAndOffset(aSelection, &selNode, &selOffset);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetStart(selNode, selOffset);
  if (NS_FAILED(res)) return res;
  res = mEditor->GetEndNodeAndOffset(aSelection, &selNode, &selOffset);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetEnd(selNode, selOffset);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}

NS_IMETHODIMP
nsHTMLEditRules::DidDeleteSelection(nsIDOMSelection *aSelection)
{
  return NS_OK;
}









