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
#include "nsHTMLEditor.h"

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

#include "nsEditorUtils.h"

//const static char* kMOZEditorBogusNodeAttr="MOZ_EDITOR_BOGUS_NODE";
//const static char* kMOZEditorBogusNodeValue="TRUE";
const unsigned char nbsp = 160;

static NS_DEFINE_IID(kSubtreeIteratorCID, NS_SUBTREEITERATOR_CID);
static NS_DEFINE_IID(kContentIteratorCID, NS_CONTENTITERATOR_CID);

enum
{
  kLonely = 0,
  kPrevSib = 1,
  kNextSib = 2,
  kBothSibs = 3
};

/********************************************************
 *  Constructor/Destructor 
 ********************************************************/

nsHTMLEditRules::nsHTMLEditRules()
: nsTextEditRules()
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
  if (!aInfo || !aCancel) 
    return NS_ERROR_NULL_POINTER;

  *aCancel = PR_FALSE;
    
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
                            info->typeInState,
                            info->maxLength);
    case kInsertBreak:
      return WillInsertBreak(aSelection, aCancel);
    case kDeleteSelection:
      return WillDeleteSelection(aSelection, info->collapsedAction, aCancel);
    case kMakeList:
      return WillMakeList(aSelection, info->bOrdered, aCancel);
    case kIndent:
      return WillIndent(aSelection, aCancel);
    case kOutdent:
      return WillOutdent(aSelection, aCancel);
    case kAlign:
      return WillAlign(aSelection, info->alignType, aCancel);
    case kMakeBasicBlock:
      return WillMakeBasicBlock(aSelection, info->blockType, aCancel);
    case kInsertElement:
      return WillInsert(aSelection, aCancel);
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
nsHTMLEditRules::WillInsertText(nsIDOMSelection *aSelection, 
                                PRBool          *aCancel,
                                PlaceholderTxn **aTxn,
                                const nsString *inString,
                                nsString       *outString,
                                TypeInState    typeInState,
                                PRInt32         aMaxLength)
{  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }

  // initialize out param
  *aCancel = PR_TRUE;
  nsresult res;

  char specialChars[] = {'\t',' ',nbsp,'\n',0};
  
  // we have to batch this in case there are returns
  // Insert Break txns don't auto merge with insert text txns
  nsAutoEditBatch beginBatching(mEditor);

  // strategy: there are simple cases and harder cases.  The harder cases
  // we handle recursively by breaking them into a series of simple cases.
  // The simple cases are:
  // 1) a single space
  // 2) a single nbsp
  // 3) a single tab
  // 4) a single return
  // 5) a run of chars containing no spaces, nbsp's, tabs, or returns
  char nbspStr[2] = {nbsp, 0};
  
    // is it an innocous run of chars?  no spaces, nbsps, returns, tabs?
    nsString theString(*inString);  // copy instr for now
    PRInt32 pos = theString.FindCharInSet(specialChars);
    while (theString.Length())
    {
      PRBool bCancel;
      nsString partialString;
      // if first char is special, then use just it
      if (pos == 0) pos = 1;
      if (pos == -1) pos = theString.Length();
      theString.Left(partialString, pos);
      theString.Cut(0, pos);
	  // is it a solo tab?
	  if (partialString == "\t" )
	  {
	    res = InsertTab(aSelection,&bCancel,aTxn,outString);
	    if (NS_FAILED(res)) return res;
        res = DoTextInsertion(aSelection, aCancel, aTxn, outString, typeInState);
	  }
	  // is it a solo space?
	  else if (partialString == " ")
	  {
	    res = InsertSpace(aSelection,&bCancel,aTxn,outString);
	    if (NS_FAILED(res)) return res;
        res = DoTextInsertion(aSelection, aCancel, aTxn, outString, typeInState);
	  }
	  // is it a solo nbsp?
	  else if (partialString == nbspStr)
	  {
	    res = InsertSpace(aSelection,&bCancel,aTxn,outString);
	    if (NS_FAILED(res)) return res;
        res = DoTextInsertion(aSelection, aCancel, aTxn, outString, typeInState);
	  }
	  // is it a solo return?
	  else if (partialString == "\n")
	  {
	    res = mEditor->InsertBreak();
	  }
	  else
	  {
	    res = DoTextInsertion(aSelection, aCancel, aTxn, &partialString, typeInState);
	  }
	  if (NS_FAILED(res)) return res;
      pos = theString.FindCharInSet(specialChars);
    }
  
  return res;
}

nsresult
nsHTMLEditRules::WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  
  nsresult res;
  res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;
  
  // if the selection isn't collapsed, delete it.
  PRBool bCollapsed;
  res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed)
  {
    res = mEditor->DeleteSelection(nsIEditor::eDoNothing);
    if (NS_FAILED(res)) return res;
  }
  
  // smart splitting rules
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
    return mEditor->InsertTextImpl(theString);
  }

  nsCOMPtr<nsIDOMNode> blockParent;
  
  if (nsEditor::IsBlockNode(node)) 
    blockParent = node;
  else 
    blockParent = mEditor->GetBlockNodeParent(node);
    
  if (!blockParent) return NS_ERROR_FAILURE;
  
  // headers: close (or split) header
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
  
  return res;
}



nsresult
nsHTMLEditRules::WillDeleteSelection(nsIDOMSelection *aSelection, nsIEditor::ESelectionCollapseDirection aAction, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  
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
      if (!offset && (aAction == nsIEditor::eDeletePrevious))
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
        
        // block parents the same?  use default deletion
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
          
          *aCancel = PR_TRUE;
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
          && (aAction == nsIEditor::eDeleteNext))
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
       
        // block parents the same?  use default deletion
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
          
          *aCancel = PR_TRUE;
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
    
      // XXX add (aAction == nsIEditor::eDeleteNext) case!!
      
      nsCOMPtr<nsIDOMNode> nodeToBackspace;
      
      res = mEditor->GetPriorNode(node, offset, PR_TRUE, getter_AddRefs(nodeToBackspace));
      if (NS_FAILED(res)) return res;
      if (!nodeToBackspace) return NS_ERROR_NULL_POINTER;
        
      // if this node is text node, adjust selection
      if (nsEditor::IsTextNode(nodeToBackspace))
      {
        PRUint32 len;
        nsCOMPtr<nsIDOMCharacterData>nodeAsText;
        nodeAsText = do_QueryInterface(nodeToBackspace);
        nodeAsText->GetLength(&len);
        res = aSelection->Collapse(nodeToBackspace,len);
        return res;
      }
      else
      {
        // editable leaf node is not text; delete it.
        // that's the default behavior
        res = nsEditor::GetNodeLocation(nodeToBackspace, &node, &offset);
        if (NS_FAILED(res)) return res;
        // adjust selection to be right after it, for benefit of 
        // deletion code
        res = aSelection->Collapse(node, offset+1);
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

    if (IsBody(node))
      leftParent = node;
    else
      leftParent = mEditor->GetBlockNodeParent(node);

    if (IsBody(endNode))
      rightParent = endNode;
    else
      rightParent = mEditor->GetBlockNodeParent(endNode);
    
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
        res = mEditor->DeleteSelectionImpl(aAction);
        if (NS_FAILED(res)) return res;
        // then join para's, insert break
        res = mEditor->JoinNodeDeep(leftParent,rightParent,&selNode,&selOffset);
        if (NS_FAILED(res)) return res;
        // fix up selection
        res = aSelection->Collapse(selNode,selOffset);
        return res;
      }
      if (IsListItem(leftParent) || IsHeader(leftParent))
      {
        // first delete the selection
        *aCancel = PR_TRUE;
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
nsHTMLEditRules::WillMakeList(nsIDOMSelection *aSelection, PRBool aOrdered, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  nsAutoString blockType("ul");
  if (aOrdered) blockType = "ol";
  
  nsAutoSelectionReset selectionResetter(aSelection);
  nsresult res;
  
  PRBool outMakeEmpty;
  res = ShouldMakeEmptyBlock(aSelection, &blockType, &outMakeEmpty);
  if (NS_FAILED(res)) return res;
  if (outMakeEmpty) return NS_OK;
  
  // ok, we aren't creating a new empty list.  Instead we are converting
  // the set of blocks implied by the selection into a list.
  
  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  
  *aCancel = PR_TRUE;

  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(aSelection, &arrayOfRanges, kMakeList);
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
  
  // if there is only one node in the array, and it is a list, div, or blockquote,
  // then look inside of it until we find what we want to make a list out of.
  arrayOfNodes->Count(&listCount);
  if (listCount == 1)
  {
    nsCOMPtr<nsISupports> isupports = (dont_AddRef)(arrayOfNodes->ElementAt(0));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );
    
    while (IsDiv(curNode) || IsOrderedList(curNode) || IsUnorderedList(curNode) || IsBlockquote(curNode))
    {
      // dive as long as there is only one child, and it is a list, div, or blockquote
      PRUint32 numChildren;
      res = mEditor->CountEditableChildren(curNode, numChildren);
      if (NS_FAILED(res)) return res;
      
      if (numChildren == 1)
      {
        // keep diving
        nsCOMPtr <nsIDOMNode> tmpNode = nsEditor::GetChildAt(curNode, 0);
        if (IsDiv(tmpNode) || IsOrderedList(tmpNode) || IsUnorderedList(tmpNode) || IsBlockquote(tmpNode))
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
    
    for (i=0; i<listCount; i++)
    {
      // here's where we actually figure out what to do
      nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
      nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
      PRInt32 offset;
      res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
      if (NS_FAILED(res)) return res;
    
      if (transitionList[i] &&                             // transition node
          ((((i+1)<listCount) && transitionList[i+1]) ||   // and next node is transistion node
          ( i+1 >= listCount)))                            // or there is no next node
      {
        // the parent of this node has no other children on the 
        // list of nodes to make a list out of.  So if this node
        // is a list, change it's list type if needed instead of 
        // reparenting it 
        nsCOMPtr<nsIDOMNode> newBlock;
        if (IsUnorderedList(curNode))
        {
          if (aOrdered)
          {
            // make a new ordered list, insert it where the current unordered list is,
            // and move all the children to the new list, and remove the old list
            res = ReplaceContainer(curNode,&newBlock,blockType);
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
        else if (IsOrderedList(curNode))
        {
          if (!aOrdered)
          {
            // make a new unordered list, insert it where the current ordered list is,
            // and move all the children to the new list, and remove the old list
            ReplaceContainer(curNode,&newBlock,blockType);
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
        else if (IsDiv(curNode) || IsBlockquote(curNode))
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
      if (IsBreak(curNode)) 
      {
        res = mEditor->DeleteNode(curNode);
        if (NS_FAILED(res)) return res;
        prevListItem = 0;
        continue;
      }
      
      // if curNode isn't a list item, we must wrap it in one
      nsCOMPtr<nsIDOMNode> listItem;
      if (!IsListItem(curNode))
      {
        if (nsEditor::IsInlineNode(curNode) && prevListItem)
        {
          // this is a continuation of some inline nodes that belong together in
          // the same list item.  use prevListItem
          PRUint32 listItemLen;
          res = mEditor->GetLengthOfDOMNode(prevListItem, listItemLen);
          if (NS_FAILED(res)) return res;
          res = mEditor->DeleteNode(curNode);
          if (NS_FAILED(res)) return res;
          res = mEditor->InsertNode(curNode, prevListItem, listItemLen);
          if (NS_FAILED(res)) return res;
        }
        else
        {
          nsAutoString listItemType = "li";
          res = InsertContainerAbove(curNode, &listItem, listItemType);
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
        res = mEditor->DeleteNode(listItem);
        if (NS_FAILED(res)) return res;
        res = mEditor->InsertNode(listItem, curList, listLen);
        if (NS_FAILED(res)) return res;
      }
    }

  return res;
}


nsresult
nsHTMLEditRules::WillMakeBasicBlock(nsIDOMSelection *aSelection, const nsString *aBlockType, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  
  PRBool makeEmpty;
  nsresult res = ShouldMakeEmptyBlock(aSelection, aBlockType, &makeEmpty);
  if (NS_FAILED(res)) return res;
  
  if (makeEmpty) return res;  // just insert a new empty block
  
  // else it's not that easy...
  nsAutoSelectionReset selectionResetter(aSelection);
  *aCancel = PR_TRUE;

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
nsHTMLEditRules::WillIndent(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_TRUE;
  
  nsAutoSelectionReset selectionResetter(aSelection);
  nsresult res;
  
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
  for (i=0; i<listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;
    
    // some logic for putting list items into nested lists...
    if (IsList(curParent))
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
      res = mEditor->DeleteNode(curNode);
      if (NS_FAILED(res)) return res;
      res = mEditor->InsertNode(curNode, curList, listLen);
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
      res = mEditor->DeleteNode(curNode);
      if (NS_FAILED(res)) return res;
      res = mEditor->InsertNode(curNode, curQuote, quoteLen);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}


nsresult
nsHTMLEditRules::WillOutdent(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_TRUE;
  
  nsAutoSelectionReset selectionResetter(aSelection);
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
  for (i=0; i<listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;
    
    if (IsList(curParent))  // move node out of list
    {
      if (IsList(curNode))  // just unwrap this sublist
      {
        res = RemoveContainer(curNode);
        if (NS_FAILED(res)) return res;
      }
      else  // we are moving a list item, but not whole list
      {
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
        
        res = mEditor->DeleteNode(curNode);
        if (NS_FAILED(res)) return res;
        res = mEditor->InsertNode(curNode, curParPar, parOffset);
        if (NS_FAILED(res)) return res;
      
        // convert list items to divs if we promoted them out of list
        if (!IsList(curParPar) && IsListItem(curNode)) 
        {
          nsAutoString blockType("div");
          nsCOMPtr<nsIDOMNode> newBlock;
          res = ReplaceContainer(curNode,&newBlock,blockType);
          if (NS_FAILED(res)) return res;
        }
      }
    }
    else if (IsList(curNode)) // node is a list, but parent is non-list: move list items out
    {
      nsCOMPtr<nsIDOMNode> child;
      curNode->GetLastChild(getter_AddRefs(child));

      while (child)
      {
        res = mEditor->DeleteNode(child);
        if (NS_FAILED(res)) return res;
        res = mEditor->InsertNode(child, curParent, offset);
        if (NS_FAILED(res)) return res;
        
        if (IsListItem(child))
        {
          nsAutoString blockType("div");
          nsCOMPtr<nsIDOMNode> newBlock;
          res = ReplaceContainer(child,&newBlock,blockType);
          if (NS_FAILED(res)) return res;
        }
        curNode->GetLastChild(getter_AddRefs(child));
      }
    }
    else if (transitionList[i])  // not list related - look for enclosing blockquotes and remove
    {
      // look for a blockquote somewhere above us and remove it.
      // this is a hack until i think about outdent for real.
      nsCOMPtr<nsIDOMNode> n = curNode;
      nsCOMPtr<nsIDOMNode> tmp;
      while (!IsBody(n))
      {
        if (IsBlockquote(n))
        {
          RemoveContainer(n);
          break;
        }
        n->GetParentNode(getter_AddRefs(tmp));
        n = tmp;
      }
    }
  }

  return res;
}


nsresult
nsHTMLEditRules::WillAlign(nsIDOMSelection *aSelection, const nsString *alignType, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  
  nsAutoSelectionReset selectionResetter(aSelection);
  nsresult res = NS_OK;

  PRBool outMakeEmpty;
  res = ShouldMakeEmptyBlock(aSelection, alignType, &outMakeEmpty);
  if (NS_FAILED(res)) return res;
  if (outMakeEmpty) return NS_OK;
  

  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  *aCancel = PR_TRUE;
  
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
  for (i=0; i<listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;
    
    // if it's a div, don't nest it, just set the alignment
    if (IsDiv(curNode))
    {
      nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(curNode);
      nsAutoString attr("align");
      res = mEditor->SetAttribute(divElem, attr, *alignType);
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
    res = mEditor->DeleteNode(curNode);
    if (NS_FAILED(res)) return res;
    res = mEditor->InsertNode(curNode, curDiv, listLen);
    if (NS_FAILED(res)) return res;
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
// IsList: true if node an html list
//                  
PRBool 
nsHTMLEditRules::IsList(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditRules::IsList");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  if ( (tag == "ol") ||
       (tag == "ul") )
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsOrderedList: true if node an html orderd list
//                  
PRBool 
nsHTMLEditRules::IsOrderedList(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditRules::IsOrderedList");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  if (tag == "ol")
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsUnorderedList: true if node an html orderd list
//                  
PRBool 
nsHTMLEditRules::IsUnorderedList(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditRules::IsUnorderedList");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  if (tag == "ul")
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
// IsBody: true if node an html body node
//                  
PRBool 
nsHTMLEditRules::IsBody(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditRules::IsBody");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  if (tag == "body")
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsBlockquote: true if node an html blockquote node
//                  
PRBool 
nsHTMLEditRules::IsBlockquote(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditRules::IsBlockquote");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  if (tag == "blockquote")
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsDiv: true if node an html div node
//                  
PRBool 
nsHTMLEditRules::IsDiv(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditRules::IsDiv");
  nsAutoString tag;
  nsEditor::GetTagString(node,tag);
  if (tag == "div")
  {
    return PR_TRUE;
  }
  return PR_FALSE;
}


///////////////////////////////////////////////////////////////////////////
// IsEmptyBlock: figure out if aNode is (or is inside) an empty block.
//               A block can have children and still be considered empty,
//               if the children are empty or non-editable.
//                  
nsresult 
nsHTMLEditRules::IsEmptyBlock(nsIDOMNode *aNode, PRBool *outIsEmptyBlock)
{
  if (!aNode || !outIsEmptyBlock) return NS_ERROR_NULL_POINTER;
  *outIsEmptyBlock = PR_TRUE;
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIContent> blockContent;
  if (nsEditor::IsBlockNode(aNode)) blockContent = do_QueryInterface(aNode);
  else 
  {
    nsCOMPtr<nsIDOMElement> block;
    res = nsEditor::GetBlockParent(aNode, getter_AddRefs(block));
    if (NS_FAILED(res)) return res;
    blockContent = do_QueryInterface(block);
  }
  
  if (!blockContent) return NS_ERROR_NULL_POINTER;
  
  // iterate over block. if no children, or all children are either 
  // empty text nodes or non-editable, then block qualifies as empty
  nsCOMPtr<nsIContentIterator> iter;
  res = nsComponentManager::CreateInstance(kContentIteratorCID,
                                        nsnull,
                                        nsIContentIterator::GetIID(), 
                                        getter_AddRefs(iter));
  if (NS_FAILED(res)) return res;
  res = iter->Init(blockContent);
  if (NS_FAILED(res)) return res;
    
  while (NS_COMFALSE == iter->IsDone())
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
        if (length) *outIsEmptyBlock = PR_FALSE;
      }
      else  // an editable, non-text node. we aren't an empty block 
      {
        // is it the node we are iterating over?
        if (node.get() == aNode) break;
        // otherwise it ain't empty
        *outIsEmptyBlock = PR_FALSE;
      }
    }
    res = iter->Next();
    if (NS_FAILED(res)) return res;
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
// IsFirstNode: Are we the first edittable node in our parent?
//                  
PRBool
nsHTMLEditRules::IsFirstNode(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset, j=0;
  nsEditor::GetNodeLocation(aNode, &parent, &offset);
  if (!offset)  // easy case, we are first dom child
    return PR_TRUE;
  
  // ok, so there are earlier children.  But are they editable???
  nsCOMPtr<nsIDOMNodeList>childList;
  nsCOMPtr<nsIDOMNode> child;
  parent->GetChildNodes(getter_AddRefs(childList));
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
  nsEditor::GetNodeLocation(aNode, &parent, &offset);
  nsEditor::GetLengthOfDOMNode(parent, numChildren); 
  if (offset+1 == numChildren) // easy case, we are last dom child
    return PR_TRUE;
  
  // ok, so there are later children.  But are they editable???
  j = offset+1;
  nsCOMPtr<nsIDOMNodeList>childList;
  nsCOMPtr<nsIDOMNode> child;
  parent->GetChildNodes(getter_AddRefs(childList));
  while (j < numChildren)
  {
    childList->Item(j, getter_AddRefs(child));
    if (mEditor->IsEditable(child)) 
      return PR_FALSE;
    j++;
  }
  return PR_TRUE;
}


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
  
  if (IsBody(aNode))
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
      if (IsBody(block))
      {
        nsCOMPtr<nsIDOMNode> prevNode;
        prevNode = nsEditor::NextNodeInBlock(node, nsEditor::kIterBackward);
        while (prevNode)
        {
          if (IsBreak(prevNode))
            break;
          if (nsEditor::IsBlockNode(prevNode))
            break;
          node = prevNode;
          prevNode = nsEditor::NextNodeInBlock(node, nsEditor::kIterBackward);
        }
      }
      else
      {
        // just grap the whole block
        node = block;
      }
    }
    
    // finding the real start for this point.  look up the tree for as long as we are the 
    // first node in the container, and as long as we haven't hit the body node.
    nsEditor::GetNodeLocation(node, &parent, &offset);
    if (NS_FAILED(res)) return res;
    while ((IsFirstNode(node)) && (!IsBody(parent)))
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
      nsEditor::GetNodeLocation(aNode, &parent, &offset);
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
      if (IsBody(block))
      {
        nsCOMPtr<nsIDOMNode> nextNode;
        nextNode = nsEditor::NextNodeInBlock(node, nsEditor::kIterForward);
        while (nextNode)
        {
          if (IsBreak(nextNode))
            break;
          if (nsEditor::IsBlockNode(nextNode))
            break;
          node = nextNode;
          nextNode = nsEditor::NextNodeInBlock(node, nsEditor::kIterForward);
        }
      }
      else
      {
        // just grap the whole block
        node = block;
      }
    }
    
    // finding the real end for this point.  look up the tree for as long as we are the 
    // last node in the container, and as long as we haven't hit the body node.
    nsEditor::GetNodeLocation(node, &parent, &offset);
    if (NS_FAILED(res)) return res;
    while ((IsLastNode(node)) && (!IsBody(parent)))
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
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 startOffset, endOffset;

  for (i = 0; i < rangeCount; i++)
  {
    res = inSelection->GetRangeAt(i, getter_AddRefs(selectionRange));
    if (NS_FAILED(res)) return res;
    res = selectionRange->GetStartParent(getter_AddRefs(startNode));
    if (NS_FAILED(res)) return res;
    res = selectionRange->GetStartOffset(&startOffset);
    if (NS_FAILED(res)) return res;
    res = selectionRange->GetEndParent(getter_AddRefs(endNode));
    if (NS_FAILED(res)) return res;
    res = selectionRange->GetEndOffset(&endOffset);
    if (NS_FAILED(res)) return res;
  
    // make a new adjusted range to represent the appropriate block content
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
    res = selectionRange->Clone(getter_AddRefs(opRange));
    if (NS_FAILED(res)) return res;
    opRange->SetStart(opStartNode, opStartOffset);
    if (NS_FAILED(res)) return res;
    opRange->SetEnd(opEndNode, opEndOffset);
    if (NS_FAILED(res)) return res;
    
    // stuff new opRange into nsISupportsArray
    nsCOMPtr<nsISupports> isupports = do_QueryInterface(opRange);
    (*outArrayOfRanges)->AppendElement(isupports);
  }
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

  for (i = 0; i < rangeCount; i++)
  {
    isupports = (dont_AddRef)(inArrayOfRanges->ElementAt(i));
    opRange = do_QueryInterface(isupports);
    res = nsComponentManager::CreateInstance(kSubtreeIteratorCID,
                                        nsnull,
                                        nsIContentIterator::GetIID(), 
                                        getter_AddRefs(iter));
    if (NS_FAILED(res)) return res;
    res = iter->Init(opRange);
    if (NS_FAILED(res)) return res;
    
    while (NS_COMFALSE == iter->IsDone())
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
  
  for (i=0; i<listCount; i++)
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


///////////////////////////////////////////////////////////////////////////
// ReplaceContainer: replace inNode with a new node (outNode) which is contructed 
//                   to be of type aNodeType.  Put inNodes children into outNode.
//                   Callers responsibility to make sure inNode's children can 
//                   go in outNode.
nsresult
nsHTMLEditRules::ReplaceContainer(nsIDOMNode *inNode, 
                                  nsCOMPtr<nsIDOMNode> *outNode, 
                                  const nsString &aNodeType)
{
  if (!inNode || !outNode)
    return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  res = nsEditor::GetNodeLocation(inNode, &parent, &offset);
  if (NS_FAILED(res)) return res;
  res = mEditor->CreateNode(aNodeType, parent, offset, getter_AddRefs(*outNode));
  if (NS_FAILED(res)) return res;
  PRBool bHasMoreChildren;
  inNode->HasChildNodes(&bHasMoreChildren);
  nsCOMPtr<nsIDOMNode> child;
  offset = 0;
  while (bHasMoreChildren)
  {
    inNode->GetLastChild(getter_AddRefs(child));
    res = mEditor->DeleteNode(child);
    if (NS_FAILED(res)) return res;
    res = mEditor->InsertNode(child, *outNode, 0);
    if (NS_FAILED(res)) return res;
    inNode->HasChildNodes(&bHasMoreChildren);
  }
  res = mEditor->DeleteNode(inNode);
  return res;
}


///////////////////////////////////////////////////////////////////////////
// RemoveContainer: remove inNode, reparenting it's children into their
//                  the parent of inNode
//
nsresult
nsHTMLEditRules::RemoveContainer(nsIDOMNode *inNode)
{
  if (!inNode)
    return NS_ERROR_NULL_POINTER;
  if (IsBody(inNode))
    return NS_ERROR_UNEXPECTED;
  nsresult res;
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  res = nsEditor::GetNodeLocation(inNode, &parent, &offset);
  if (NS_FAILED(res)) return res;
  PRBool bHasMoreChildren;
  inNode->HasChildNodes(&bHasMoreChildren);
  nsCOMPtr<nsIDOMNode> child;
  while (bHasMoreChildren)
  {
    inNode->GetLastChild(getter_AddRefs(child));
    res = mEditor->DeleteNode(child);
    if (NS_FAILED(res)) return res;
    res = mEditor->InsertNode(child, parent, offset);
    if (NS_FAILED(res)) return res;
    inNode->HasChildNodes(&bHasMoreChildren);
  }
  res = mEditor->DeleteNode(inNode);
  return res;
}


///////////////////////////////////////////////////////////////////////////
// InsertContainerAbove:  insert a new parent for inNode, returned in outNode,
//                   which is contructed to be of type aNodeType.  outNode becomes
//                   a child of inNode's earlier parent.
//                   Callers responsibility to make sure inNode's can be child
//                   of outNode, and outNode can be child of old parent.
nsresult
nsHTMLEditRules::InsertContainerAbove(nsIDOMNode *inNode, 
                                  nsCOMPtr<nsIDOMNode> *outNode, 
                                  nsString &aNodeType)
{
  if (!inNode || !outNode)
    return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  res = nsEditor::GetNodeLocation(inNode, &parent, &offset);
  if (NS_FAILED(res)) return res;
  
  // make new parent, outNode
  res = mEditor->CreateNode(aNodeType, parent, offset, getter_AddRefs(*outNode));
  if (NS_FAILED(res)) return res;

  // put inNode in new parent, outNode
  res = mEditor->DeleteNode(inNode);
  if (NS_FAILED(res)) return res;
  res = mEditor->InsertNode(inNode, *outNode, 0);
  if (NS_FAILED(res)) return res;

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
                           PRBool *aCancel, 
                           PlaceholderTxn **aTxn,
                           nsString *outString)
{
  nsCOMPtr<nsIDOMNode> parentNode;
  PRInt32 offset;
  PRBool isPRE;
  PRBool isNextWhiteSpace;
  PRBool isPrevWhiteSpace;
  
  nsresult res = mEditor->GetStartNodeAndOffset(aSelection, &parentNode, &offset);
  if (NS_FAILED(res)) return res;
  
  if (!parentNode) return NS_ERROR_FAILURE;
    
  res = mEditor->IsPreformatted(parentNode,&isPRE);
  if (NS_FAILED(res)) return res;
    
  if (isPRE)
  {
    *outString += '\t';
    return NS_OK;
  }

  res = mEditor->IsNextCharWhitespace(parentNode, offset, &isNextWhiteSpace);
  if (NS_FAILED(res)) return res;
  
  res = mEditor->IsPrevCharWhitespace(parentNode, offset, &isPrevWhiteSpace);
  if (NS_FAILED(res)) return res;

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
  
  nsresult res = mEditor->GetStartNodeAndOffset(aSelection, &parentNode, &offset);
  if (NS_FAILED(res)) return res;

  if (!parentNode) return NS_ERROR_FAILURE;
  
  res = mEditor->IsPreformatted(parentNode,&isPRE);
  if (NS_FAILED(res)) return res;
  
  if (isPRE)
  {
    *outString += " ";
    return NS_OK;
  }
  
  res = mEditor->IsNextCharWhitespace(parentNode, offset, &isNextWhiteSpace);
  if (NS_FAILED(res)) return res;
  
  res = mEditor->IsPrevCharWhitespace(parentNode, offset, &isPrevWhiteSpace);
  if (NS_FAILED(res)) return res;
  
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
  
// revisit the below when we move to using divs as standard paragraphs.
// need to create an "empty" div in the fisrt case below, and need to
// use a div instead of a <p> in the second case below

  // if the new (righthand) header node is empty, delete it
  PRBool isEmpty;
  res = IsEmptyBlock(aHeader, &isEmpty);
  if (NS_FAILED(res)) return res;
  if (isEmpty)
  {
    res = mEditor->DeleteNode(aHeader);
    if (NS_FAILED(res)) return res;
    res = aSelection->Collapse(headerParent,offset+1);
    return res;
  }
  // else rewrap it in a paragraph
  nsCOMPtr<nsIDOMNode> newBlock;
  nsAutoString blockType("p");
  res = ReplaceContainer(aHeader,&newBlock,blockType);
  if (NS_FAILED(res)) return res;
  res = aSelection->Collapse(newBlock,0);
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

//bombs away - nuking this for now...
#if 0

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
        PRInt32 newOffset;
        *aCancel = PR_TRUE;
        // get rid of the break
        res = mEditor->DeleteNode(sibling);  
        if (NS_FAILED(res)) return res;
        // split the paragraph
        res = mEditor->SplitNodeDeep( aHeader, aNode, aOffset, &newOffset);
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
        PRInt32 newOffset;
        *aCancel = PR_TRUE;
        // get rid of the break
        res = mEditor->DeleteNode(sibling);  
        if (NS_FAILED(res)) return res;
        // split the paragraph
        res = mEditor->SplitNodeDeep( aHeader, aNode, aOffset, &newOffset);
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
#endif
  
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
  NS_PRECONDITION(PR_TRUE == IsListItem(aListItem), "expected a list item and didnt get one");
  
  // if we are in an empty listitem, then we want to pop up out of the list
  PRBool isEmpty;
  res = IsEmptyBlock(aListItem, &isEmpty);
  if (NS_FAILED(res)) return res;
  if (isEmpty)
  {
    nsCOMPtr<nsIDOMNode> list, listparent;
    PRInt32 offset;
    list = nsEditor::GetBlockNodeParent(aListItem);
    res = nsEditor::GetNodeLocation(list, &listparent, &offset);
    if (NS_FAILED(res)) return res;
    
    // are we in a sublist?
    if (IsList(listparent))  //in a sublist
    {
      // if so, move this list item out of this list and into the grandparent list
      res = mEditor->DeleteNode(aListItem);
      if (NS_FAILED(res)) return res;
      res = mEditor->InsertNode(aListItem,listparent,offset+1);
      if (NS_FAILED(res)) return res;
      res = aSelection->Collapse(aListItem,0);
    }
    else
    {
      // otherwise kill this listitem and set the selection to after the parent list
      res = mEditor->DeleteNode(aListItem);
      if (NS_FAILED(res)) return res;
      res = aSelection->Collapse(listparent,offset+1);
    }
    return res;
  }
  
  // else we want a new list item at the same list level
  PRInt32 newOffset;
  res = mEditor->SplitNodeDeep( aListItem, aNode, aOffset, &newOffset);
  if (NS_FAILED(res)) return res;
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
  // no more inline content before the next block, then we want
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
    if (IsBody(parent))
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
      
      if (prevChild && IsBreak(prevChild)) 
      {
        nextChild = nsEditor::GetChildAt(parent, offset);
        while (nextChild && !mEditor->IsEditable(nextChild))
        {
          // search back until we either find an editable node, 
          // or hit the beginning of the block
          tmp = nsEditor::NextNodeInBlock(nextChild, nsEditor::kIterForward);
          nextChild = tmp;
        }
        if (!nextChild || IsBreak(nextChild) || nsEditor::IsBlockNode(nextChild))
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
  // headers, paragraphs (or moz-divs), pre, and address.  Those blocks
  // that pretty much just contain inline things...
  
  if (!arrayOfNodes || !aBlockTag) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> curNode, curParent, curBlock, newBlock;
  PRInt32 offset;
  PRUint32 listCount;
  arrayOfNodes->Count(&listCount);
  
  PRUint32 i;
  for (i=0; i<listCount; i++)
  {
    // get the node to act on, and it's location
    nsCOMPtr<nsISupports> isupports  = (dont_AddRef)(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;
    nsAutoString curNodeTag;
    nsEditor::GetTagString(curNode, curNodeTag);
        
 
    // is it already the right kind of block?
    if (curNodeTag == *aBlockTag)
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      continue;  // do nothing to this block
    }
        
    // if curNode is a <pre> and we are converting to non-pre, we need
    // to process the text inside the <pre> so as to convert returns
    // to breaks, and runs of spaces to nbsps.
    // xxx floppy moose
      
    // if curNode is a p, header, address, or pre, replace 
    // it with a new block of correct type.
    // xxx floppy moose: pre cant hold everything the others can
    if ((curNodeTag == "pre") || 
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
      res = ReplaceContainer(curNode, &newBlock, *aBlockTag);
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
             (curNodeTag == "div"))
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      // recursion time
      nsCOMPtr<nsISupportsArray> childArray;
      res = GetChildNodesForOperation(curNode, &childArray);
      if (NS_FAILED(res)) return res;
      res = ApplyBlockStyle(childArray, aBlockTag);
      if (NS_FAILED(res)) return res;
    }
    
    // if curNode is inline, pull it into curBlock
    // note: it's assumed that consecutive inline nodes in the 
    // arrayOfNodes are actually members of the same block parent.
    // this happens to be true now as a side effect of how
    // arrayOfNodes is contructed, but some additional logic should
    // be added here if that should change
    
    else if (nsEditor::IsInlineNode(curNode))
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
      res = mEditor->DeleteNode(curNode);
      if (NS_FAILED(res)) return res;
      res = mEditor->InsertNode(curNode, curBlock, blockLen);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}


nsresult 
nsHTMLEditRules::IsFirstEditableChild( nsIDOMNode *aNode, PRBool *aOutIsFirst)
{
  // check parms
  if (!aOutIsFirst || !aNode) return NS_ERROR_NULL_POINTER;
  
  // init out parms
  *aOutIsFirst = PR_FALSE;
  
  // find first editable child and compare it to aNode
  nsCOMPtr<nsIDOMNode> parent;
  nsresult res = aNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(res)) return res;
  if (!parent) return NS_ERROR_FAILURE;
  nsCOMPtr<nsIDOMNode> child;
  parent->GetFirstChild(getter_AddRefs(child));
  if (!child) return NS_ERROR_FAILURE;

  if (!mEditor->IsEditable(child))
  {
    nsCOMPtr<nsIDOMNode> tmp;
    res = mEditor->GetNextNode(child, PR_TRUE, getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
    if (!tmp) return NS_ERROR_FAILURE;
    child = tmp;
  }
  
  *aOutIsFirst = (child.get() == aNode);
  return res;
}


nsresult 
nsHTMLEditRules::IsLastEditableChild( nsIDOMNode *aNode, PRBool *aOutIsLast)
{
  // check parms
  if (!aOutIsLast || !aNode) return NS_ERROR_NULL_POINTER;
  
  // init out parms
  *aOutIsLast = PR_FALSE;
  
  // find last editable child and compare it to aNode
  nsCOMPtr<nsIDOMNode> parent;
  nsresult res = aNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(res)) return res;
  if (!parent) return NS_ERROR_FAILURE;
  nsCOMPtr<nsIDOMNode> child;
  parent->GetLastChild(getter_AddRefs(child));
  if (!child) return NS_ERROR_FAILURE;

  if (!mEditor->IsEditable(child))
  {
    nsCOMPtr<nsIDOMNode> tmp;
    res = mEditor->GetPriorNode(child, PR_TRUE, getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
    if (!tmp) return NS_ERROR_FAILURE;
    child = tmp;
  }
  
  *aOutIsLast = (child.get() == aNode);
  return res;
}


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
  //   left & right node are smae type
  //   left & right node have smae parent
  
  nsCOMPtr<nsIDOMNode> parent;
  aNodeLeft->GetParentNode(getter_AddRefs(parent));
  
  // defaults for outParams
  *aOutMergeParent = aNodeRight;
  res = mEditor->GetLengthOfDOMNode(aNodeLeft, *((PRUint32*)aOutMergeOffset));
  if (NS_FAILED(res)) return res;

  // seperate join rules for differing blocks
  if (IsParagraph(aNodeLeft))
  {
    // for para's, merge deep & add a <br> after merging
    res = mEditor->JoinNodeDeep(aNodeLeft, aNodeRight, aOutMergeParent, aOutMergeOffset);
    if (NS_FAILED(res)) return res;
    nsAutoString brType("br");
    nsCOMPtr<nsIDOMNode> brNode;
    res = mEditor->CreateNode(brType, *aOutMergeParent, *aOutMergeOffset, getter_AddRefs(brNode));
    // out offset _after_ <br>
    *aOutMergeOffset++;
    return res;
  }
  else if (IsList(aNodeLeft) || mEditor->IsTextNode(aNodeLeft))
  {
    // for list's, merge shallow (wouldn't want to combine list items)
    res = mEditor->JoinNodes(aNodeLeft, aNodeRight, parent);
    if (NS_FAILED(res)) return res;
    return res;
  }
  else
  {
    // for list items, divs, etc, merge smart
    res = JoinNodesSmart(aNodeLeft, aNodeRight, aOutMergeParent, aOutMergeOffset);
    if (NS_FAILED(res)) return res;
    return res;
  }
}
