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

const static char* kMOZEditorBogusNodeAttr="MOZ_EDITOR_BOGUS_NODE";
const static char* kMOZEditorBogusNodeValue="TRUE";
const unsigned char nbsp = nbsp;

static NS_DEFINE_IID(kPlaceholderTxnIID,  PLACEHOLDER_TXN_IID);
static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);

nsIAtom *nsHTMLEditRules::sAAtom;
nsIAtom *nsHTMLEditRules::sAddressAtom;
nsIAtom *nsHTMLEditRules::sBigAtom;
nsIAtom *nsHTMLEditRules::sBlinkAtom;
nsIAtom *nsHTMLEditRules::sBAtom;
nsIAtom *nsHTMLEditRules::sCiteAtom;
nsIAtom *nsHTMLEditRules::sCodeAtom;
nsIAtom *nsHTMLEditRules::sDfnAtom;
nsIAtom *nsHTMLEditRules::sEmAtom;
nsIAtom *nsHTMLEditRules::sFontAtom;
nsIAtom *nsHTMLEditRules::sIAtom;
nsIAtom *nsHTMLEditRules::sKbdAtom;
nsIAtom *nsHTMLEditRules::sKeygenAtom;
nsIAtom *nsHTMLEditRules::sNobrAtom;
nsIAtom *nsHTMLEditRules::sSAtom;
nsIAtom *nsHTMLEditRules::sSampAtom;
nsIAtom *nsHTMLEditRules::sSmallAtom;
nsIAtom *nsHTMLEditRules::sSpacerAtom;
nsIAtom *nsHTMLEditRules::sSpanAtom;      
nsIAtom *nsHTMLEditRules::sStrikeAtom;
nsIAtom *nsHTMLEditRules::sStrongAtom;
nsIAtom *nsHTMLEditRules::sSubAtom;
nsIAtom *nsHTMLEditRules::sSupAtom;
nsIAtom *nsHTMLEditRules::sTtAtom;
nsIAtom *nsHTMLEditRules::sUAtom;
nsIAtom *nsHTMLEditRules::sVarAtom;
nsIAtom *nsHTMLEditRules::sWbrAtom;

nsIAtom *nsHTMLEditRules::sH1Atom;
nsIAtom *nsHTMLEditRules::sH2Atom;
nsIAtom *nsHTMLEditRules::sH3Atom;
nsIAtom *nsHTMLEditRules::sH4Atom;
nsIAtom *nsHTMLEditRules::sH5Atom;
nsIAtom *nsHTMLEditRules::sH6Atom;
nsIAtom *nsHTMLEditRules::sParagraphAtom;
nsIAtom *nsHTMLEditRules::sListItemAtom;
nsIAtom *nsHTMLEditRules::sBreakAtom;



PRInt32 nsHTMLEditRules::sInstanceCount = 0;

/********************************************************
 *  Constructor/Destructor 
 ********************************************************/

nsHTMLEditRules::nsHTMLEditRules()
{
  // XXX: this sux. We are dependant on layout's private conception of tag atom names.
  
  if (sInstanceCount <= 0)
  {
    // inline tags
    sAAtom = NS_NewAtom("a");
    sAddressAtom = NS_NewAtom("address");
    sBigAtom = NS_NewAtom("big");
    sBlinkAtom = NS_NewAtom("blink");
    sBAtom = NS_NewAtom("b");
    sCiteAtom = NS_NewAtom("cite");
    sCodeAtom = NS_NewAtom("code");
    sDfnAtom = NS_NewAtom("dfn");
    sEmAtom = NS_NewAtom("em");
    sFontAtom = NS_NewAtom("font");
    sIAtom = NS_NewAtom("i");
    sKbdAtom = NS_NewAtom("kbd");
    sKeygenAtom = NS_NewAtom("keygen");
    sNobrAtom = NS_NewAtom("nobr");
    sSAtom = NS_NewAtom("s");
    sSampAtom = NS_NewAtom("samp");
    sSmallAtom = NS_NewAtom("small");
    sSpacerAtom = NS_NewAtom("spacer");
    sSpanAtom = NS_NewAtom("span");      
    sStrikeAtom = NS_NewAtom("strike");
    sStrongAtom = NS_NewAtom("strong");
    sSubAtom = NS_NewAtom("sub");
    sSupAtom = NS_NewAtom("sup");
    sTtAtom = NS_NewAtom("tt");
    sUAtom = NS_NewAtom("u");
    sVarAtom = NS_NewAtom("var");
    sWbrAtom = NS_NewAtom("wbr");
    
    sH1Atom = NS_NewAtom("h1");
    sH2Atom = NS_NewAtom("h2");
    sH3Atom = NS_NewAtom("h3");
    sH4Atom = NS_NewAtom("h4");
    sH5Atom = NS_NewAtom("h5");
    sH6Atom = NS_NewAtom("h6");
    sParagraphAtom = NS_NewAtom("p");
    sListItemAtom = NS_NewAtom("li");
    sBreakAtom = NS_NewAtom("br");
  }

  ++sInstanceCount;
}

nsHTMLEditRules::~nsHTMLEditRules()
{
  if (sInstanceCount <= 1)
  {
    NS_IF_RELEASE(sAAtom);
    NS_IF_RELEASE(sAddressAtom);
    NS_IF_RELEASE(sBigAtom);
    NS_IF_RELEASE(sBlinkAtom);
    NS_IF_RELEASE(sBAtom);
    NS_IF_RELEASE(sCiteAtom);
    NS_IF_RELEASE(sCodeAtom);
    NS_IF_RELEASE(sDfnAtom);
    NS_IF_RELEASE(sEmAtom);
    NS_IF_RELEASE(sFontAtom);
    NS_IF_RELEASE(sIAtom);
    NS_IF_RELEASE(sKbdAtom);
    NS_IF_RELEASE(sKeygenAtom);
    NS_IF_RELEASE(sNobrAtom);
    NS_IF_RELEASE(sSAtom);
    NS_IF_RELEASE(sSampAtom);
    NS_IF_RELEASE(sSmallAtom);
    NS_IF_RELEASE(sSpacerAtom);
    NS_IF_RELEASE(sSpanAtom);      
    NS_IF_RELEASE(sStrikeAtom);
    NS_IF_RELEASE(sStrongAtom);
    NS_IF_RELEASE(sSubAtom);
    NS_IF_RELEASE(sSupAtom);
    NS_IF_RELEASE(sTtAtom);
    NS_IF_RELEASE(sUAtom);
    NS_IF_RELEASE(sVarAtom);
    NS_IF_RELEASE(sWbrAtom);
    
    NS_IF_RELEASE(sH1Atom);
    NS_IF_RELEASE(sH2Atom);
    NS_IF_RELEASE(sH3Atom);
    NS_IF_RELEASE(sH4Atom);
    NS_IF_RELEASE(sH5Atom);
    NS_IF_RELEASE(sH6Atom);
    NS_IF_RELEASE(sParagraphAtom);
    NS_IF_RELEASE(sListItemAtom);
    NS_IF_RELEASE(sBreakAtom);
  }

  --sInstanceCount;
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
      return WillDeleteSelection(aSelection, info->dir, aCancel);
  }
  return nsTextEditRules::WillDoAction(aSelection, aInfo, aCancel);
}
  
NS_IMETHODIMP 
nsHTMLEditRules::DidDoAction(nsIDOMSelection *aSelection,
                             nsRulesInfo *aInfo, nsresult aResult)
{
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
    res = mEditor->DeleteSelection(nsIEditor::eLTR);
    if (NS_FAILED(res)) return res;
  }
  
  //smart splitting rules
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  PRBool isPRE;
  
  res = GetStartNodeAndOffset(aSelection, &node, &offset);
  if (NS_FAILED(res)) return res;
  if (!node) return NS_ERROR_FAILURE;
    
  res = IsPreformatted(node,&isPRE);
  if (NS_FAILED(res)) return res;
    
  if (isPRE)
  {
    nsString theString = "\n";
    *aCancel = PR_TRUE;
    return mEditor->InsertText(theString);
  }

  nsCOMPtr<nsIDOMNode> blockParent = GetBlockNodeParent(node);
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
nsHTMLEditRules::WillDeleteSelection(nsIDOMSelection *aSelection, nsIEditor::Direction aDir, PRBool *aCancel)
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
  
  res = GetStartNodeAndOffset(aSelection, &node, &offset);
  if (NS_FAILED(res)) return res;
  if (!node) return NS_ERROR_FAILURE;
    
  if (bCollapsed)
  {
    // easy case, in a text node:
    if (IsTextNode(node))
    {
      nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(node);
      PRUint32 strLength;
      res = textNode->GetLength(&strLength);
      if (NS_FAILED(res)) return res;
    
      // at beginning of text node and backspaced?
      if (!offset && (aDir == nsIEditor::eRTL))
      {
        nsCOMPtr<nsIDOMNode> priorNode;
        res = GetPriorNode(node, getter_AddRefs(priorNode));
        if (NS_FAILED(res)) return res;
        
        // XXX hackery - using this to skip empty text nodes,
        // since these are almost always non displayed preservation
        // of returns in the original html.  but they could
        // actually be significant, then we're hosed.  FIXME
        while (IsEmptyTextNode(priorNode))
        {
          res = mEditor->DeleteNode(priorNode);
          if (NS_FAILED(res)) return res;
          res = GetPriorNode(node, getter_AddRefs(priorNode));
          if (NS_FAILED(res)) return res;
        }
        
        // block parents the same?  use defaul deletion
        if (HasSameBlockNodeParent(node, priorNode)) return NS_OK;
		
		// deleting across blocks
		// are the blocks of same type?
		nsCOMPtr<nsIDOMNode> leftParent = GetBlockNodeParent(priorNode);
		nsCOMPtr<nsIDOMNode> rightParent = GetBlockNodeParent(node);
		nsCOMPtr<nsIAtom> leftAtom = GetTag(leftParent);
		nsCOMPtr<nsIAtom> rightAtom = GetTag(rightParent);
		
		if (leftAtom.get() == rightAtom.get())
		{
		  nsCOMPtr<nsIDOMNode> topParent;
		  leftParent->GetParentNode(getter_AddRefs(topParent));
		  
		  if (IsParagraph(leftParent))
		  {
		    // join para's, insert break
            *aCancel = PR_TRUE;
            res = mEditor->JoinNodes(leftParent,rightParent,topParent);
            if (NS_FAILED(res)) return res;
            res = mEditor->InsertBreak();
            return res;
		  }
		  if (IsListItem(leftParent) || IsHeader(leftParent))
		  {
		    // join blocks
            *aCancel = PR_TRUE;
            res = mEditor->JoinNodes(leftParent,rightParent,topParent);
            return res;
		  }
		}
        
        // else blocks not same type, bail to default
        return NS_OK;
        
      }
    
      // at end of text node and deleted?
      if ((offset == strLength) && (aDir == nsIEditor::eLTR))
      {
        nsCOMPtr<nsIDOMNode> nextNode;
        res = GetNextNode(node, getter_AddRefs(nextNode));
        if (NS_FAILED(res)) return res;
        if (HasSameBlockNodeParent(node, nextNode)) return NS_OK;
		
		// deleting across blocks
        // XXX hackery - using this to skip empty text nodes,
        // since these are almost always non displayed preservation
        // of returns in the original html.  but they could
        // actually be significant, then we're hosed.  FIXME
        while (IsEmptyTextNode(nextNode))
        {
          res = mEditor->DeleteNode(nextNode);
          if (NS_FAILED(res)) return res;
          res = GetNextNode(node, getter_AddRefs(nextNode));
          if (NS_FAILED(res)) return res;
        }
        
        // block parents the same?  use defaul deletion
        if (HasSameBlockNodeParent(node, nextNode)) return NS_OK;
		
		// deleting across blocks
		// are the blocks of same type?
		nsCOMPtr<nsIDOMNode> leftParent = GetBlockNodeParent(node);
		nsCOMPtr<nsIDOMNode> rightParent = GetBlockNodeParent(nextNode);
		nsCOMPtr<nsIAtom> leftAtom = GetTag(leftParent);
		nsCOMPtr<nsIAtom> rightAtom = GetTag(rightParent);
		
		if (leftAtom.get() == rightAtom.get())
		{
		  nsCOMPtr<nsIDOMNode> topParent;
		  leftParent->GetParentNode(getter_AddRefs(topParent));
		  
		  if (IsParagraph(leftParent))
		  {
		    // join para's, insert break
            *aCancel = PR_TRUE;
            res = mEditor->JoinNodes(leftParent,rightParent,topParent);
            if (NS_FAILED(res)) return res;
            res = mEditor->InsertBreak();
            return res;
		  }
		  if (IsListItem(leftParent) || IsHeader(leftParent))
		  {
		    // join blocks
            *aCancel = PR_TRUE;
            res = mEditor->JoinNodes(leftParent,rightParent,topParent);
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
  res = GetEndNodeAndOffset(aSelection, &endNode, &endOffset);
  if (endNode.get() != node.get())
  {
    // block parents the same?  use defaul deletion
    if (HasSameBlockNodeParent(node, endNode)) return NS_OK;
	
	// deleting across blocks
	// are the blocks of same type?
	nsCOMPtr<nsIDOMNode> leftParent = GetBlockNodeParent(node);
	nsCOMPtr<nsIDOMNode> rightParent = GetBlockNodeParent(endNode);
	
	// are the blocks siblings?
	nsCOMPtr<nsIDOMNode> leftBlockParent;
	nsCOMPtr<nsIDOMNode> rightBlockParent;
	leftParent->GetParentNode(getter_AddRefs(leftBlockParent));
	rightParent->GetParentNode(getter_AddRefs(rightBlockParent));
	// bail to default if blocks aren't siblings
	if (leftBlockParent.get() != rightBlockParent.get()) return NS_OK;

	nsCOMPtr<nsIAtom> leftAtom = GetTag(leftParent);
	nsCOMPtr<nsIAtom> rightAtom = GetTag(rightParent);

	if (leftAtom.get() == rightAtom.get())
	{
	  nsCOMPtr<nsIDOMNode> topParent;
	  leftParent->GetParentNode(getter_AddRefs(topParent));
	  
	  if (IsParagraph(leftParent))
	  {
	    // first delete the selection
        *aCancel = PR_TRUE;
        res = mEditor->nsEditor::DeleteSelection(aDir);
        if (NS_FAILED(res)) return res;
	    // then join para's, insert break
        res = mEditor->JoinNodes(leftParent,rightParent,topParent);
        if (NS_FAILED(res)) return res;
        res = mEditor->InsertBreak();
        return res;
	  }
	  if (IsListItem(leftParent) || IsHeader(leftParent))
	  {
	    // first delete the selection
        *aCancel = PR_TRUE;
        res = mEditor->nsEditor::DeleteSelection(aDir);
        if (NS_FAILED(res)) return res;
	    // join blocks
        res = mEditor->JoinNodes(leftParent,rightParent,topParent);
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
 
nsresult 
nsHTMLEditRules::GetRightmostChild(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> resultNode(do_QueryInterface(aCurrentNode));
  PRBool hasChildren;
  resultNode->HasChildNodes(&hasChildren);
  while ((NS_SUCCEEDED(result)) && (PR_TRUE==hasChildren))
  {
    nsCOMPtr<nsIDOMNode> temp(resultNode);
    temp->GetLastChild(getter_AddRefs(resultNode));
    resultNode->HasChildNodes(&hasChildren);
  }

  if (NS_SUCCEEDED(result)) {
    *aResultNode = resultNode;
    NS_ADDREF(*aResultNode);
  }
  return result;
}

nsresult 
nsHTMLEditRules::GetLeftmostChild(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> resultNode(do_QueryInterface(aCurrentNode));
  PRBool hasChildren;
  resultNode->HasChildNodes(&hasChildren);
  while ((NS_SUCCEEDED(result)) && (PR_TRUE==hasChildren))
  {
    nsCOMPtr<nsIDOMNode> temp(resultNode);
    temp->GetFirstChild(getter_AddRefs(resultNode));
    resultNode->HasChildNodes(&hasChildren);
  }

  if (NS_SUCCEEDED(result)) {
    *aResultNode = resultNode;
    NS_ADDREF(*aResultNode);
  }
  return result;
}

nsresult 
nsHTMLEditRules::GetPriorNode(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode)
{
  nsresult result;
  *aResultNode = nsnull;
  // if aCurrentNode has a left sibling, return that sibling's rightmost child (or itself if it has no children)
  result = aCurrentNode->GetPreviousSibling(aResultNode);
  if ((NS_SUCCEEDED(result)) && *aResultNode)
  {
    return GetRightmostChild(*aResultNode, aResultNode);
  }
  // otherwise, walk up the parent change until there is a child that comes before 
  // the ancestor of aCurrentNode.  Then return that node's rightmost child

  nsCOMPtr<nsIDOMNode> parent(do_QueryInterface(aCurrentNode));
  do {
    nsCOMPtr<nsIDOMNode> node(parent);
    result = node->GetParentNode(getter_AddRefs(parent));
    if ((NS_SUCCEEDED(result)) && parent)
    {
      result = parent->GetPreviousSibling(getter_AddRefs(node));
      if ((NS_SUCCEEDED(result)) && node)
      {
        return GetRightmostChild(node, aResultNode);
      }
    }
  } while ((NS_SUCCEEDED(result)) && parent);

  return result;
}

nsresult 
nsHTMLEditRules::GetNextNode(nsIDOMNode *aCurrentNode, nsIDOMNode **aResultNode)
{
  nsresult result;
  *aResultNode = nsnull;
  // if aCurrentNode has a right sibling, return that sibling's leftmost child (or itself if it has no children)
  result = aCurrentNode->GetNextSibling(aResultNode);
  if ((NS_SUCCEEDED(result)) && *aResultNode)
  {
    return GetLeftmostChild(*aResultNode, aResultNode);
  }  
  // otherwise, walk up the parent change until there is a child that comes before 
  // the ancestor of aCurrentNode.  Then return that node's rightmost child

  nsCOMPtr<nsIDOMNode> parent(do_QueryInterface(aCurrentNode));
  do {
    nsCOMPtr<nsIDOMNode> node(parent);
    result = node->GetParentNode(getter_AddRefs(parent));
    if ((NS_SUCCEEDED(result)) && parent)
    {
      result = parent->GetNextSibling(getter_AddRefs(node));
      if ((NS_SUCCEEDED(result)) && node)
      {
        return GetLeftmostChild(node, aResultNode);
      }
    }
  } while ((NS_SUCCEEDED(result)) && parent);

  return result;
}

 
 
///////////////////////////////////////////////////////////////////////////
// GetTag: digs out the atom for the tag of this node
//                    
nsCOMPtr<nsIAtom> 
nsHTMLEditRules::GetTag(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIAtom> atom;
  
  if (!aNode) 
  {
    NS_NOTREACHED("null node passed to nsHTMLEditRules::GetTag()");
    return atom;
  }
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  content->GetTag(*getter_AddRefs(atom));

  return atom;
}

///////////////////////////////////////////////////////////////////////////
// IsBlockNode: true if this node is an html block node
//                    
PRBool
nsHTMLEditRules::IsBlockNode(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIAtom> atom;
  
  if (!aNode) 
  {
    NS_NOTREACHED("null node passed to IsBlockNode()");
    return PR_FALSE;
  }
  
  if (IsTextNode(aNode))
    return PR_FALSE;
    
  atom = GetTag(aNode);

  if (!atom)
    return PR_TRUE;

  if (sAAtom != atom.get() &&
      sAddressAtom != atom.get() &&
      sBigAtom != atom.get() &&
      sBlinkAtom != atom.get() &&
      sBAtom != atom.get() &&
      sCiteAtom != atom.get() &&
      sCodeAtom != atom.get() &&
      sDfnAtom != atom.get() &&
      sEmAtom != atom.get() &&
      sFontAtom != atom.get() &&
      sIAtom != atom.get() &&
      sKbdAtom != atom.get() &&
      sKeygenAtom != atom.get() &&
      sNobrAtom != atom.get() &&
      sSAtom != atom.get() &&
      sSampAtom != atom.get() &&
      sSmallAtom != atom.get() &&
      sSpacerAtom != atom.get() &&
      sSpanAtom != atom.get() &&
      sStrikeAtom != atom.get() &&
      sStrongAtom != atom.get() &&
      sSubAtom != atom.get() &&
      sSupAtom != atom.get() &&
      sTtAtom != atom.get() &&
      sUAtom != atom.get() &&
      sVarAtom != atom.get() &&
      sWbrAtom != atom.get())
   {
     return PR_TRUE;
   }
   return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsInlineNode: true if this node is an html inline node
//                    
PRBool
nsHTMLEditRules::IsInlineNode(nsIDOMNode *aNode)
{
  return !IsBlockNode(aNode);
}

///////////////////////////////////////////////////////////////////////////
// GetBlockNodeParent: returns enclosing block level ancestor, if any
//                     else returns the node itself.  Note that if the
//                     node itself is a block node, it is returned.
nsCOMPtr<nsIDOMNode>
nsHTMLEditRules::GetBlockNodeParent(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMNode> tmp = do_QueryInterface(aNode);
  nsCOMPtr<nsIDOMNode> p;

  if (IsBlockNode(aNode))
    return tmp;
  if (NS_FAILED(aNode->GetParentNode(getter_AddRefs(p))))  // no parent, ran off top of tree
    return tmp;

  while (p && !IsBlockNode(p))
  {
    if (NS_FAILED(p->GetParentNode(getter_AddRefs(tmp)))) // no parent, ran off top of tree
      return p;

    p = tmp;
  }
  return p;
}


///////////////////////////////////////////////////////////////////////////
// HasSameBlockNodeParent: true if nodes have same block level ancestor
//               
PRBool
nsHTMLEditRules::HasSameBlockNodeParent(nsIDOMNode *aNode1, nsIDOMNode *aNode2)
{
  if (!aNode1 || !aNode2)
  {
    NS_NOTREACHED("null node passed to HasSameBlockNodeParent()");
    return PR_FALSE;
  }
  
  if (aNode1 == aNode2)
    return PR_TRUE;
    
  nsCOMPtr<nsIDOMNode> p1 = GetBlockNodeParent(aNode1);
  nsCOMPtr<nsIDOMNode> p2 = GetBlockNodeParent(aNode2);

  return (p1 == p2);
}


///////////////////////////////////////////////////////////////////////////
// IsTextOrElementNode: true if node of dom type element or text
//               
PRBool
nsHTMLEditRules::IsTextOrElementNode(nsIDOMNode *aNode)
{
  if (!aNode)
  {
    NS_NOTREACHED("null node passed to IsTextOrElementNode()");
    return PR_FALSE;
  }
  
  PRUint16 nodeType;
  aNode->GetNodeType(&nodeType);
  if ((nodeType == nsIDOMNode::ELEMENT_NODE) || (nodeType == nsIDOMNode::TEXT_NODE))
    return PR_TRUE;
    
  return PR_FALSE;
}



///////////////////////////////////////////////////////////////////////////
// IsTextNode: true if node of dom type text
//               
PRBool
nsHTMLEditRules::IsTextNode(nsIDOMNode *aNode)
{
  if (!aNode)
  {
    NS_NOTREACHED("null node passed to IsTextNode()");
    return PR_FALSE;
  }
  
  PRUint16 nodeType;
  aNode->GetNodeType(&nodeType);
  if (nodeType == nsIDOMNode::TEXT_NODE)
    return PR_TRUE;
    
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsEmptyTextNode: true if node of dom type text and is empty
//                  or if it has only char which is whitespace.  HACKEROONY!
PRBool
nsHTMLEditRules::IsEmptyTextNode(nsIDOMNode *aNode)
{
  if (!aNode)
  {
    NS_NOTREACHED("null node passed to IsTextNode()");
    return PR_FALSE;
  }
  
  if (!IsTextNode(aNode))
    return PR_FALSE;
    
  nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aNode);
  PRUint32 strLength;
  textNode->GetLength(&strLength);
  if (!strLength)
    return PR_TRUE;
    
  nsString tempString;
  textNode->GetData(tempString);
  tempString.StripWhitespace();
  if (!tempString.Length())
    return PR_TRUE;
  
  return PR_FALSE;
}



PRInt32 
nsHTMLEditRules::GetIndexOf(nsIDOMNode *parent, nsIDOMNode *child)
{
  PRInt32 index = 0;
  
  NS_PRECONDITION(parent, "null parent passed to nsHTMLEditRules::GetIndexOf");
  NS_PRECONDITION(parent, "null child passed to nsHTMLEditRules::GetIndexOf");
  nsCOMPtr<nsIContent> content = do_QueryInterface(parent);
  nsCOMPtr<nsIContent> cChild = do_QueryInterface(child);
  NS_PRECONDITION(content, "null content in nsHTMLEditRules::GetIndexOf");
  NS_PRECONDITION(cChild, "null content in nsHTMLEditRules::GetIndexOf");
  
  nsresult res = content->IndexOf(cChild, index);
  if (NS_FAILED(res)) 
  {
    NS_NOTREACHED("could not find child in parent - nsHTMLEditRules::GetIndexOf");
  }
  return index;
}
  

///////////////////////////////////////////////////////////////////////////
// IsHeader: true if node an html header
//                  
PRBool 
nsHTMLEditRules::IsHeader(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditRules::IsHeader");
  nsCOMPtr<nsIAtom> atom = GetTag(node);
  if ( (atom.get() == sH1Atom) ||
       (atom.get() == sH2Atom) ||
       (atom.get() == sH3Atom) ||
       (atom.get() == sH4Atom) ||
       (atom.get() == sH5Atom) ||
       (atom.get() == sH6Atom) )
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
  nsCOMPtr<nsIAtom> atom = GetTag(node);
  if (atom.get() == sParagraphAtom)
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
  nsCOMPtr<nsIAtom> atom = GetTag(node);
  if (atom.get() == sListItemAtom)
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsListItem: true if node an html list item
//                  
PRBool 
nsHTMLEditRules::IsBreak(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditRules::IsBreak");
  nsCOMPtr<nsIAtom> atom = GetTag(node);
  if (atom.get() == sBreakAtom)
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// NextNodeInBlock: gets the next/prev node in the block, if any.  Next node
//                  must be an element or text node, others are ignored
nsCOMPtr<nsIDOMNode>
nsHTMLEditRules::NextNodeInBlock(nsIDOMNode *aNode, IterDirection aDir)
{
  nsCOMPtr<nsIDOMNode> nullNode;
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIContent> blockContent;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIDOMNode> blockParent;
  
  if (!aNode)  return nullNode;

  nsCOMPtr<nsIContentIterator> iter;
  if (NS_FAILED(nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                        nsIContentIterator::GetIID(), 
                                        getter_AddRefs(iter))))
    return nullNode;

  // much gnashing of teeth as we twit back and forth between content and domnode types
  content = do_QueryInterface(aNode);
  blockParent = GetBlockNodeParent(aNode);
  if (!blockParent) return nullNode;
  blockContent = do_QueryInterface(blockParent);
  if (!blockContent) return nullNode;
  
  if (NS_FAILED(iter->Init(blockContent)))  return nullNode;
  if (NS_FAILED(iter->PositionAt(content)))  return nullNode;
  
  while (NS_COMFALSE == iter->IsDone())
  {
  	if (NS_FAILED(iter->CurrentNode(getter_AddRefs(content)))) return nullNode;
    // ignore nodes that aren't elements or text, or that are the block parent 
    node = do_QueryInterface(content);
    if (node && IsTextOrElementNode(node) && (node != blockParent))
      return node;
    
    if (aDir == kIterForward)
      iter->Next();
    else
      iter->Prev();
  }
  
  return nullNode;
}


///////////////////////////////////////////////////////////////////////////
// GetStartNodeAndOffset: returns whatever the start parent & offset is of 
//                        the first range in the selection.
nsresult 
nsHTMLEditRules::GetStartNodeAndOffset(nsIDOMSelection *aSelection,
                                       nsCOMPtr<nsIDOMNode> *outStartNode,
                                       PRInt32 *outStartOffset)
{
  if (!outStartNode || !outStartOffset) 
    return NS_ERROR_NULL_POINTER;
    
  nsCOMPtr<nsIEnumerator> enumerator;
  enumerator = do_QueryInterface(aSelection);
  if (!enumerator) 
    return NS_ERROR_FAILURE;
    
  enumerator->First(); 
  nsISupports *currentItem;
  if ((NS_FAILED(enumerator->CurrentItem(&currentItem))) || !currentItem)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
  if (!range)
    return NS_ERROR_FAILURE;
    
  if (NS_FAILED(range->GetStartParent(getter_AddRefs(*outStartNode))))
    return NS_ERROR_FAILURE;
    
  if (NS_FAILED(range->GetStartOffset(outStartOffset)))
    return NS_ERROR_FAILURE;
    
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// GetEndNodeAndOffset: returns whatever the end parent & offset is of 
//                        the first range in the selection.
nsresult 
nsHTMLEditRules::GetEndNodeAndOffset(nsIDOMSelection *aSelection,
                                       nsCOMPtr<nsIDOMNode> *outEndNode,
                                       PRInt32 *outEndOffset)
{
  if (!outEndNode || !outEndOffset) 
    return NS_ERROR_NULL_POINTER;
    
  nsCOMPtr<nsIEnumerator> enumerator;
  enumerator = do_QueryInterface(aSelection);
  if (!enumerator) 
    return NS_ERROR_FAILURE;
    
  enumerator->First(); 
  nsISupports *currentItem;
  if ((NS_FAILED(enumerator->CurrentItem(&currentItem))) || !currentItem)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
  if (!range)
    return NS_ERROR_FAILURE;
    
  if (NS_FAILED(range->GetEndParent(getter_AddRefs(*outEndNode))))
    return NS_ERROR_FAILURE;
    
  if (NS_FAILED(range->GetEndOffset(outEndOffset)))
    return NS_ERROR_FAILURE;
    
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// IsPreformatted: checks the style info for the node for the preformatted
//                 text style.
nsresult 
nsHTMLEditRules::IsPreformatted(nsIDOMNode *aNode, PRBool *aResult)
{
  nsIPresShell* shell = nsnull;
  nsresult result;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  nsIFrame *frame;
  nsCOMPtr<nsIStyleContext> styleContext;
  const nsStyleText* styleText;
  PRBool bPreformatted;
  
  if (!aResult || !content) return NS_ERROR_NULL_POINTER;
  
  result = mEditor->GetPresShell(&shell);
  if (NS_FAILED(result)) return result;
  
  result = shell->GetPrimaryFrameFor(content, &frame);
  if (NS_FAILED(result)) return result;
  
  result = shell->GetStyleContextFor(frame, getter_AddRefs(styleContext));
  if (NS_FAILED(result)) return result;

  styleText = (const nsStyleText*)styleContext->GetStyleData(eStyleStruct_Text);

  bPreformatted = (NS_STYLE_WHITESPACE_PRE == styleText->mWhiteSpace) ||
    (NS_STYLE_WHITESPACE_MOZ_PRE_WRAP == styleText->mWhiteSpace);

  *aResult = bPreformatted;
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// IsNextCharWhitespace: checks the adjacent content in the same block
//                       to see if following selection is whitespace
nsresult 
nsHTMLEditRules::IsNextCharWhitespace(nsIDOMNode *aParentNode, 
                                      PRInt32 aOffset,
                                      PRBool *aResult)
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
  *aResult = PR_FALSE;
  
  nsString tempString;
  PRUint32 strLength;
  nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aParentNode);
  if (textNode)
  {
    textNode->GetLength(&strLength);
    if (aOffset < strLength)
    {
      // easy case: next char is in same node
      textNode->SubstringData(aOffset,aOffset+1,tempString);
      *aResult = nsString::IsSpace(tempString.First());
      return NS_OK;
    }
  }
  
  // harder case: next char in next node.
  nsCOMPtr<nsIDOMNode> node = NextNodeInBlock(aParentNode, kIterForward);
  nsCOMPtr<nsIDOMNode> tmp;
  while (node) 
  {
    if (!IsInlineNode(node))  // skip over bold, italic, link, ect nodes
    {
      if (IsTextNode(node))
      {
        textNode = do_QueryInterface(node);
        textNode->GetLength(&strLength);
        if (strLength)
        {
          textNode->SubstringData(0,1,tempString);
          *aResult = nsString::IsSpace(tempString.First());
          return NS_OK;
        }
        // else it's an empty text node, skip it.
      }
      else  // node is an image or some other thingy that doesn't count as whitespace
      {
        break;
      }
    }
    tmp = node;
    node = NextNodeInBlock(tmp, kIterForward);
  }
  
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// IsPrevCharWhitespace: checks the adjacent content in the same block
//                       to see if following selection is whitespace
nsresult 
nsHTMLEditRules::IsPrevCharWhitespace(nsIDOMNode *aParentNode, 
                                      PRInt32 aOffset,
                                      PRBool *aResult)
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
  *aResult = PR_FALSE;
  
  nsString tempString;
  PRUint32 strLength;
  nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aParentNode);
  if (textNode)
  {
    if (aOffset > 0)
    {
      // easy case: prev char is in same node
      textNode->SubstringData(aOffset-1,aOffset,tempString);
      *aResult = nsString::IsSpace(tempString.First());
      return NS_OK;
    }
  }
  
  // harder case: prev char in next node
  nsCOMPtr<nsIDOMNode> node = NextNodeInBlock(aParentNode, kIterBackward);
  nsCOMPtr<nsIDOMNode> tmp;
  while (node) 
  {
    if (!IsInlineNode(node))  // skip over bold, italic, link, ect nodes
    {
      if (IsTextNode(node))
      {
        textNode = do_QueryInterface(node);
        textNode->GetLength(&strLength);
        if (strLength)
        {
          textNode->SubstringData(strLength-1,strLength,tempString);
          *aResult = nsString::IsSpace(tempString.First());
          return NS_OK;
        }
        // else it's an empty text node, skip it.
      }
      else  // node is an image or some other thingy that doesn't count as whitespace
      {
        break;
      }
    }
    // otherwise we found a node we want to skip, keep going
    tmp = node;
    node = NextNodeInBlock(tmp, kIterBackward);
  }
  
  return NS_OK;
  
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



///////////////////////////////////////////////////////////////////////////
// SplitNodeDeep: this plits a node "deeply", splitting children as 
//                appropriate.  The place to split is represented by
//                a dom point at {splitPointParent, splitPointOffset}.
//                That dom point must be inside aNode, which is the node to 
//                split.
nsresult
nsHTMLEditRules::SplitNodeDeep(nsIDOMNode *aNode, 
                               nsIDOMNode *aSplitPointParent, 
                               PRInt32 aSplitPointOffset)
{
  if (!aNode || !aSplitPointParent) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> nodeToSplit = do_QueryInterface(aSplitPointParent);
  nsCOMPtr<nsIDOMNode> tempNode;  
  PRInt32 offset = aSplitPointOffset;
  
  while (nodeToSplit)
  {
    nsresult res = mEditor->SplitNode(nodeToSplit, offset, getter_AddRefs(tempNode));
    if (NS_FAILED(res)) return res;
    
    if (nodeToSplit.get() == aNode)  // we split all the way up to (and including) aNode; we're done
      break;
      
    tempNode = nodeToSplit;
    res = tempNode->GetParentNode(getter_AddRefs(nodeToSplit));
    offset = GetIndexOf(nodeToSplit, tempNode);
  }
  
  if (!nodeToSplit)
  {
    NS_NOTREACHED("null node obtained in nsHTMLEditRules::SplitNodeDeep()");
    return NS_ERROR_FAILURE;
  }
  
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
  
  nsresult result = GetStartNodeAndOffset(aSelection, &parentNode, &offset);
  if (NS_FAILED(result)) return result;
  
  if (!parentNode) return NS_ERROR_FAILURE;
    
  result = IsPreformatted(parentNode,&isPRE);
  if (NS_FAILED(result)) return result;
    
  if (isPRE)
  {
    *outString += '\t';
    return NS_OK;
  }

  result = IsNextCharWhitespace(parentNode, offset, &isNextWhiteSpace);
  if (NS_FAILED(result)) return result;
  
  result = IsPrevCharWhitespace(parentNode, offset, &isPrevWhiteSpace);
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
  
  nsresult result = GetStartNodeAndOffset(aSelection, &parentNode, &offset);
  if (NS_FAILED(result)) return result;

  if (!parentNode) return NS_ERROR_FAILURE;
  
  result = IsPreformatted(parentNode,&isPRE);
  if (NS_FAILED(result)) return result;
  
  if (isPRE)
  {
    *outString += " ";
    return NS_OK;
  }
  
  result = IsNextCharWhitespace(parentNode, offset, &isNextWhiteSpace);
  if (NS_FAILED(result)) return result;
  
  result = IsPrevCharWhitespace(parentNode, offset, &isPrevWhiteSpace);
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
  PRInt32 indx = GetIndexOf(p,aHeader);
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
  if (IsTextNode(aNode))
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
        res = SplitNodeDeep( aHeader, aNode, aOffset);
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
        res = SplitNodeDeep( aHeader, aNode, aOffset);
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

  nsresult res = SplitNodeDeep( aListItem, aNode, aOffset);
  if (NS_FAILED(res)) return res;
  res = aSelection->Collapse(aNode,0);
  return res;
}













