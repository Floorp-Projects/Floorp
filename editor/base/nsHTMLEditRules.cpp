/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL") you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsHTMLEditRules.h"
#include "nsEditor.h"
#include "nsTextEditor.h"
#include "PlaceholderTxn.h"
#include "InsertTextTxn.h"
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

class nsIFrame;

//const static char* kMOZEditorBogusNodeAttr="MOZ_EDITOR_BOGUS_NODE";
//const static char* kMOZEditorBogusNodeValue="TRUE";
const unsigned char nbsp = 160;

static NS_DEFINE_IID(kPlaceholderTxnIID,  PLACEHOLDER_TXN_IID);
static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);

/********************************************************
 *  Constructor/Destructor 
 ********************************************************/

nsHTMLEditRules::nsHTMLEditRules()
{
}

nsHTMLEditRules::~nsHTMLEditRules()
{
}


/********************************************************
 *  Public methods 
 ********************************************************/

NS_IMETHODIMP 
nsHTMLEditRules::WillDoAction(nsIDOMSelection *aSelection, 
                              nsRulesInfo *aInfo, PRBool *aCancel)
{
  if (!aSelection || !aInfo) 
    return NS_ERROR_NULL_POINTER;
    
  // my kingdom for dynamic cast
  nsTextRulesInfo *info = NS_STATIC_CAST(nsTextRulesInfo*, aInfo);
    
  switch (info->action)
  {
    case kInsertText:
      return WillInsertText(aSelection, 
                            aCancel, 
                            info->placeTxn,
                            info->inString,
                            info->outString,
                            info->typeInState);
    case kInsertBreak:
      return WillInsertBreak(aSelection, aCancel);
    case kDeleteSelection:
      return WillDeleteSelection(aSelection, info->collapsedAction, aCancel);
  }
  return nsTextEditRules::WillDoAction(aSelection, aInfo, aCancel);
}
  
  
NS_IMETHODIMP 
nsHTMLEditRules::DidDoAction(nsIDOMSelection *aSelection,
                             nsRulesInfo *aInfo, nsresult aResult)
{
  // my kingdom for dynamic cast
  nsTextRulesInfo *info = NS_STATIC_CAST(nsTextRulesInfo*, aInfo);
    
  switch (info->action)
  {
    case kInsertBreak:
      return NS_OK;
  }

  return nsTextEditRules::DidDoAction(aSelection, aInfo, aResult);
}
  

/********************************************************
 *  Protected rules methods 
 ********************************************************/
 
nsresult
nsHTMLEditRules::WillInsertText(nsIDOMSelection  *aSelection, 
                                PRBool          *aCancel,
                                PlaceholderTxn **aTxn,
                                const nsString *inString,
                                nsString       *outString,
                                TypeInState    typeInState)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;

  // XXX - need to handle strings of length >1 with embedded tabs or spaces
  // XXX - what about embedded returns?
  
  // is it a tab?
  if (*inString == "\t" )
    return InsertTab(aSelection,aCancel,aTxn,outString);
  // is it a space?
  if (*inString == " ")
    return InsertSpace(aSelection,aCancel,aTxn,outString);
  
  // otherwise, return nsTextEditRules version
  return nsTextEditRules::WillInsertText(aSelection, 
                                         aCancel, 
                                         aTxn,
                                         inString,
                                         outString,
                                         typeInState);
}

nsresult
nsHTMLEditRules::WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  
  nsresult res;
  
  // if the selection isn't collapsed, delete it.
  PRBool bCollapsed;
  res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed)
  {
    res = mEditor->DeleteSelection(nsIEditor::eDoNothing);
    if (NS_FAILED(res)) return res;
  }
  
  //smart splitting rules
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  PRBool isPRE;
  
  res = mEditor->GetStartNodeAndOffset(aSelection, &node, &offset);
  if (NS_FAILED(res)) return res;
  if (!node) return NS_ERROR_FAILURE;
    
  res = mEditor->IsPreformatted(node,&isPRE);
  if (NS_FAILED(res)) return res;
    
  if (isPRE)
  {
    nsString theString = "\n";
    *aCancel = PR_TRUE;
    return mEditor->InsertText(theString);
  }

  nsCOMPtr<nsIDOMNode> blockParent = mEditor->GetBlockNodeParent(node);
  if (!blockParent) return NS_ERROR_FAILURE;
  
  // headers: put selection after the header
  if (IsHeader(blockParent))
  {
    res = ReturnInHeader(aSelection, blockParent, node, offset);
    *aCancel = PR_TRUE;
    return NS_OK;
  }
  
  // paragraphs: special rules to look for <br>s
  if (IsParagraph(blockParent))
  {
    res = ReturnInParagraph(aSelection, blockParent, node, offset, aCancel);
    return NS_OK;
  }
  
  // list items: special rules to make new list items
  if (IsListItem(blockParent))
  {
    res = ReturnInListItem(aSelection, blockParent, node, offset);
    *aCancel = PR_TRUE;
    return NS_OK;
  }
  
  
  return WillInsert(aSelection, aCancel);
}



nsresult
nsHTMLEditRules::WillDeleteSelection(nsIDOMSelection *aSelection, nsIEditor::ECollapsedSelectionAction aAction, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  
  nsresult res = NS_OK;
  
  PRBool bCollapsed;
  res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  
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
      if (!offset && (aAction == nsIEditor::eDeleteLeft))
      {
        nsCOMPtr<nsIDOMNode> priorNode;
        res = mEditor->GetPriorNode(node, PR_TRUE, getter_AddRefs(priorNode));
        if (NS_FAILED(res)) return res;
        
        // if there is no prior node then cancel the deletion
        if (!priorNode)
        {
          *aCancel = PR_TRUE;
          return res;
        }
        
        // block parents the same?  use defaul deletion
        if (mEditor->HasSameBlockNodeParent(node, priorNode)) return NS_OK;
		
		// deleting across blocks
		// are the blocks of same type?
		nsCOMPtr<nsIDOMNode> leftParent = mEditor->GetBlockNodeParent(priorNode);
		nsCOMPtr<nsIDOMNode> rightParent = mEditor->GetBlockNodeParent(node);
		nsCOMPtr<nsIAtom> leftAtom = mEditor->GetTag(leftParent);
		nsCOMPtr<nsIAtom> rightAtom = mEditor->GetTag(rightParent);
		
		if (leftAtom.get() == rightAtom.get())
		{
		  nsCOMPtr<nsIDOMNode> topParent;
		  leftParent->GetParentNode(getter_AddRefs(topParent));
		  
		  if (IsParagraph(leftParent))
		  {
		    // join para's, insert break
            *aCancel = PR_TRUE;
            res = mEditor->JoinNodeDeep(leftParent,rightParent,aSelection);
            if (NS_FAILED(res)) return res;
            res = mEditor->InsertBreak();
            return res;
		  }
		  if (IsListItem(leftParent) || IsHeader(leftParent))
		  {
		    // join blocks
            *aCancel = PR_TRUE;
            res = mEditor->JoinNodeDeep(leftParent,rightParent,aSelection);
            return res;
		  }
		}
        
        // else blocks not same type, bail to default
        return NS_OK;
        
      }
    
      // at end of text node and deleted?
      if ((offset == strLength) && (aAction == nsIEditor::eDeleteRight))
      {
        nsCOMPtr<nsIDOMNode> nextNode;
        res = mEditor->GetNextNode(node, PR_TRUE, getter_AddRefs(nextNode));
        if (NS_FAILED(res)) return res;
         
        // if there is no next node then cancel the deletion
        if (!nextNode)
        {
          *aCancel = PR_TRUE;
          return res;
        }
       
        // block parents the same?  use defaul deletion
        if (mEditor->HasSameBlockNodeParent(node, nextNode)) return NS_OK;
		
		// deleting across blocks
		// are the blocks of same type?
		nsCOMPtr<nsIDOMNode> leftParent = mEditor->GetBlockNodeParent(node);
		nsCOMPtr<nsIDOMNode> rightParent = mEditor->GetBlockNodeParent(nextNode);
		nsCOMPtr<nsIAtom> leftAtom = mEditor->GetTag(leftParent);
		nsCOMPtr<nsIAtom> rightAtom = mEditor->GetTag(rightParent);
		
		if (leftAtom.get() == rightAtom.get())
		{
		  nsCOMPtr<nsIDOMNode> topParent;
		  leftParent->GetParentNode(getter_AddRefs(topParent));
		  
		  if (IsParagraph(leftParent))
		  {
		    // join para's, insert break
            *aCancel = PR_TRUE;
            res = mEditor->JoinNodeDeep(leftParent,rightParent,aSelection);
            if (NS_FAILED(res)) return res;
            res = mEditor->InsertBreak();
            return res;
		  }
		  if (IsListItem(leftParent) || IsHeader(leftParent))
		  {
		    // join blocks
            *aCancel = PR_TRUE;
            res = mEditor->JoinNodeDeep(leftParent,rightParent,aSelection);
            return res;
		  }
		}
        
        // else blocks not same type, bail to default
        return NS_OK;
        
      }
    
      // else do default
      return NS_OK;
    }
  }
  
  // else we have a non collapsed selection
  // figure out if the enpoints are in nodes that can be merged
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 endOffset;
  res = mEditor->GetEndNodeAndOffset(aSelection, &endNode, &endOffset);
  if (endNode.get() != node.get())
  {
    // block parents the same?  use defaul deletion
    if (mEditor->HasSameBlockNodeParent(node, endNode)) return NS_OK;
	
	// deleting across blocks
	// are the blocks of same type?
	nsCOMPtr<nsIDOMNode> leftParent = mEditor->GetBlockNodeParent(node);
	nsCOMPtr<nsIDOMNode> rightParent = mEditor->GetBlockNodeParent(endNode);
	
	// are the blocks siblings?
	nsCOMPtr<nsIDOMNode> leftBlockParent;
	nsCOMPtr<nsIDOMNode> rightBlockParent;
	leftParent->GetParentNode(getter_AddRefs(leftBlockParent));
	rightParent->GetParentNode(getter_AddRefs(rightBlockParent));
	// bail to default if blocks aren't siblings
	if (leftBlockParent.get() != rightBlockParent.get()) return NS_OK;

	nsCOMPtr<nsIAtom> leftAtom = mEditor->GetTag(leftParent);
	nsCOMPtr<nsIAtom> rightAtom = mEditor->GetTag(rightParent);

	if (leftAtom.get() == rightAtom.get())
	{
	  nsCOMPtr<nsIDOMNode> topParent;
	  leftParent->GetParentNode(getter_AddRefs(topParent));
	  
	  if (IsParagraph(leftParent))
	  {
	    // first delete the selection
        *aCancel = PR_TRUE;
        res = mEditor->nsEditor::DeleteSelection(aAction);
        if (NS_FAILED(res)) return res;
	    // then join para's, insert break
        res = mEditor->JoinNodeDeep(leftParent,rightParent,aSelection);
        if (NS_FAILED(res)) return res;
        //res = mEditor->InsertBreak();
        // uhh, no, we don't want to have the <br> on a deleted selction across para's
        return res;
	  }
	  if (IsListItem(leftParent) || IsHeader(leftParent))
	  {
	    // first delete the selection
        *aCancel = PR_TRUE;
        res = mEditor->nsEditor::DeleteSelection(aAction);
        if (NS_FAILED(res)) return res;
	    // join blocks
        res = mEditor->JoinNodeDeep(leftParent,rightParent,aSelection);
        return res;
	  }
	}
    
    // else blocks not same type, bail to default
    return NS_OK;
  }

  return res;
}  

/********************************************************
 *  helper methods 
 ********************************************************/
 
///////////////////////////////////////////////////////////////////////////
// IsHeader: true if node an html header
//                  
PRBool 
nsHTMLEditRules::IsHeader(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditRules::IsHeader");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  if ( (tag == "h1") ||
       (tag == "h2") ||
       (tag == "h3") ||
       (tag == "h4") ||
       (tag == "h5") ||
       (tag == "h6") )
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsParagraph: true if node an html paragraph
//                  
PRBool 
nsHTMLEditRules::IsParagraph(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditRules::IsParagraph");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  if (tag == "p")
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsListItem: true if node an html list item
//                  
PRBool 
nsHTMLEditRules::IsListItem(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditRules::IsListItem");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  if (tag == "li")
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsBreak: true if node an html break node
//                  
PRBool 
nsHTMLEditRules::IsBreak(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditRules::IsBreak");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  if (tag == "br")
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// GetTabAsNBSPs: stuff the right number of nbsp's into outString
//                       
nsresult
nsHTMLEditRules::GetTabAsNBSPs(nsString *outString)
{
  if (!outString) return NS_ERROR_NULL_POINTER;
  // XXX - this should get the right number from prefs
  *outString += nbsp; *outString += nbsp; *outString += nbsp; *outString += nbsp; 
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// GetTabAsNBSPsAndSpace: stuff the right number of nbsp's followed by a 
//                        space into outString
nsresult
nsHTMLEditRules::GetTabAsNBSPsAndSpace(nsString *outString)
{
  if (!outString) return NS_ERROR_NULL_POINTER;
  // XXX - this should get the right number from prefs
  *outString += nbsp; *outString += nbsp; *outString += nbsp; *outString += ' '; 
  return NS_OK;
}



/********************************************************
 *  main implemntation methods 
 ********************************************************/
 
///////////////////////////////////////////////////////////////////////////
// InsertTab: top level logic for determining how to insert a tab
//                       
nsresult 
nsHTMLEditRules::InsertTab(nsIDOMSelection *aSelection, 
                           PRBool *aCancel, 
                           PlaceholderTxn **aTxn,
                           nsString *outString)
{
  nsCOMPtr<nsIDOMNode> parentNode;
  PRInt32 offset;
  PRBool isPRE;
  PRBool isNextWhiteSpace;
  PRBool isPrevWhiteSpace;
  
  nsresult result = mEditor->GetStartNodeAndOffset(aSelection, &parentNode, &offset);
  if (NS_FAILED(result)) return result;
  
  if (!parentNode) return NS_ERROR_FAILURE;
    
  result = mEditor->IsPreformatted(parentNode,&isPRE);
  if (NS_FAILED(result)) return result;
    
  if (isPRE)
  {
    *outString += '\t';
    return NS_OK;
  }

  result = mEditor->IsNextCharWhitespace(parentNode, offset, &isNextWhiteSpace);
  if (NS_FAILED(result)) return result;
  
  result = mEditor->IsPrevCharWhitespace(parentNode, offset, &isPrevWhiteSpace);
  if (NS_FAILED(result)) return result;

  if (isPrevWhiteSpace)
  {
    // prev character is a whitespace; Need to
    // insert nbsp's BEFORE the space
    
    // XXX for now put tab in wrong place
    if (isNextWhiteSpace)
    {
      GetTabAsNBSPs(outString);
      return NS_OK;
    }
    GetTabAsNBSPsAndSpace(outString);
    return NS_OK;
  }
  
  if (isNextWhiteSpace)
  {
    // character after us is ws.  insert nbsps
    GetTabAsNBSPs(outString);
    return NS_OK;
  }
  
  // else we are in middle of a word; use n-1 nbsp's plus a space
  GetTabAsNBSPsAndSpace(outString);
  
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// InsertSpace: top level logic for determining how to insert a space
//                       
nsresult 
nsHTMLEditRules::InsertSpace(nsIDOMSelection *aSelection, 
                             PRBool *aCancel, 
                             PlaceholderTxn **aTxn,
                             nsString *outString)
{
  nsCOMPtr<nsIDOMNode> parentNode;
  PRInt32 offset;
  PRBool isPRE;
  PRBool isNextWhiteSpace;
  PRBool isPrevWhiteSpace;
  
  nsresult result = mEditor->GetStartNodeAndOffset(aSelection, &parentNode, &offset);
  if (NS_FAILED(result)) return result;

  if (!parentNode) return NS_ERROR_FAILURE;
  
  result = mEditor->IsPreformatted(parentNode,&isPRE);
  if (NS_FAILED(result)) return result;
  
  if (isPRE)
  {
    *outString += " ";
    return NS_OK;
  }
  
  result = mEditor->IsNextCharWhitespace(parentNode, offset, &isNextWhiteSpace);
  if (NS_FAILED(result)) return result;
  
  result = mEditor->IsPrevCharWhitespace(parentNode, offset, &isPrevWhiteSpace);
  if (NS_FAILED(result)) return result;
  
  if (isPrevWhiteSpace)
  {
    // prev character is a whitespace; Need to
    // insert nbsp BEFORE the space
    
    // XXX for now put in wrong place
    if (isNextWhiteSpace)
    {
      *outString += nbsp;
      return NS_OK;
    }
    *outString += nbsp;
    return NS_OK;
  }
  
  if (isNextWhiteSpace)
  {
    // character after us is ws.  insert nbsp
    *outString += nbsp;
    return NS_OK;
  }
    
  // else just a space
  *outString += " ";
  
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// ReturnInHeader: do the right thing for returns pressed in headers
//                       
nsresult 
nsHTMLEditRules::ReturnInHeader(nsIDOMSelection *aSelection, 
                                nsIDOMNode *aHeader, 
                                nsIDOMNode *aTextNode, 
                                PRInt32 aOffset)
{
  if (!aSelection || !aHeader || !aTextNode) return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIDOMNode> leftNode;
  nsCOMPtr<nsIDOMNode> textNode = do_QueryInterface(aTextNode);  // to hold a ref across the delete call
  // split the node
  nsresult res = mEditor->SplitNode(aTextNode, aOffset, getter_AddRefs(leftNode));
  if (NS_FAILED(res)) return res;
  
  // move the right node outside of the header, via deletion/insertion
  // delete the right node
  res = mEditor->DeleteNode(textNode);  
  if (NS_FAILED(res)) return res;
  
  // insert the right node
  nsCOMPtr<nsIDOMNode> p;
  aHeader->GetParentNode(getter_AddRefs(p));
  PRInt32 indx = mEditor->GetIndexOf(p,aHeader);
  res = mEditor->InsertNode(textNode,p,indx+1);
  if (NS_FAILED(res)) return res;
  
  // merge text node with like sibling, if any
  nsCOMPtr<nsIDOMNode> sibling;
  textNode->GetNextSibling(getter_AddRefs(sibling));
  if (sibling)
  {
    res = mEditor->JoinNodes(textNode,sibling,p);
    if (NS_FAILED(res)) return res;
    textNode = sibling;  // sibling was the node kept by the join; remember it in "textNode"
  }
  
  // position selection before inserted node
  res = aSelection->Collapse(textNode,0);

  return res;
}


///////////////////////////////////////////////////////////////////////////
// ReturnInParagraph: do the right thing for returns pressed in paragraphs
//                       
nsresult 
nsHTMLEditRules::ReturnInParagraph(nsIDOMSelection *aSelection, 
                                   nsIDOMNode *aHeader, 
                                   nsIDOMNode *aNode, 
                                   PRInt32 aOffset,
                                   PRBool *aCancel)
{
  if (!aSelection || !aHeader || !aNode || !aCancel) return NS_ERROR_NULL_POINTER;
  *aCancel = PR_FALSE;

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
      aNode->GetPreviousSibling(getter_AddRefs(sibling));
      if (!sibling) 
      {
        // no previous sib, so
        // just fall out to default of inserting a BR
        return res;
      }
      if (IsBreak(sibling))
      {
        *aCancel = PR_TRUE;
        // get rid of the break
        res = mEditor->DeleteNode(sibling);  
        if (NS_FAILED(res)) return res;
        // split the paragraph
        res = mEditor->SplitNodeDeep( aHeader, aNode, aOffset);
        if (NS_FAILED(res)) return res;
        // position selection inside textnode
        res = aSelection->Collapse(aNode,0);
      }
      // else just fall out to default of inserting a BR
      return res;
    }
    // at end of text node?
    if (aOffset == strLength)
    {
      // is there a BR after to it?
      res = aNode->GetNextSibling(getter_AddRefs(sibling));
      if (!sibling) 
      {
        // no next sib, so
        // just fall out to default of inserting a BR
        return res;
      }
      if (IsBreak(sibling))
      {
        *aCancel = PR_TRUE;
        // get rid of the break
        res = mEditor->DeleteNode(sibling);  
        if (NS_FAILED(res)) return res;
        // split the paragraph
        res = mEditor->SplitNodeDeep( aHeader, aNode, aOffset);
        if (NS_FAILED(res)) return res;
        // position selection inside textnode
        res = aSelection->Collapse(aNode,0);
      }
      // else just fall out to default of inserting a BR
      return res;
    }
    // inside text node
    // just fall out to default of inserting a BR
    return res;
  }
  
  // not in a text node.  are we next to BR's?
  // XXX  
  
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

  nsresult res = mEditor->SplitNodeDeep( aListItem, aNode, aOffset);
  if (NS_FAILED(res)) return res;
  res = aSelection->Collapse(aNode,0);
  return res;
}













