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
#include "nsISupportsArray.h"
#include "nsIStyleContext.h"
#include "nsIPresShell.h"
#include "nsLayoutCID.h"

class nsIFrame;

//const static char* kMOZEditorBogusNodeAttr="MOZ_EDITOR_BOGUS_NODE";
//const static char* kMOZEditorBogusNodeValue="TRUE";
const unsigned char nbsp = 160;

static NS_DEFINE_IID(kPlaceholderTxnIID,  PLACEHOLDER_TXN_IID);
// static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_IID(kSubtreeIteratorCID, NS_SUBTREEITERATOR_CID);

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
  if (!aSelection || !aInfo || !aCancel) 
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
    case kMakeHeader:
      return WillMakeHeader(aSelection, aCancel);
    case kMakeAddress:
      return WillMakeAddress(aSelection, aCancel);
    case kMakePRE:
      return WillMakePRE(aSelection, aCancel);
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
                                         typeInState,
                                         aMaxLength);
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
      if ((offset == (PRInt32)strLength)
          && (aAction == nsIEditor::eDeleteRight))
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
    // block parents the same?  use default deletion
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


nsresult
nsHTMLEditRules::WillMakeList(nsIDOMSelection *aSelection, PRBool aOrdered, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_TRUE;
  
  nsAutoSelectionReset selectionResetter(aSelection);
  nsresult res = NS_OK;
  
  // get selection range - XXX generalize to collection of ranges
  // gather up factoids about the range
  nsCOMPtr<nsIDOMRange> selectionRange;
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIDOMNode> endNode;
  nsCOMPtr<nsIDOMNode> commonParent;
  PRInt32 startOffset, endOffset;
  
  res = aSelection->GetRangeAt(0,getter_AddRefs(selectionRange));
  if (NS_FAILED(res)) return res;
  res = selectionRange->GetStartParent(getter_AddRefs(startNode));
  if (NS_FAILED(res)) return res;
  res = selectionRange->GetStartOffset(&startOffset);
  if (NS_FAILED(res)) return res;
  res = selectionRange->GetEndParent(getter_AddRefs(endNode));
  if (NS_FAILED(res)) return res;
  res = selectionRange->GetEndOffset(&endOffset);
  if (NS_FAILED(res)) return res;
//  res = selectionRange->GetCommonParent(getter_AddRefs(commonParent));
//  if (NS_FAILED(res)) return res;
  
  // make a new adjusted range to represent the appropriate block content
  // this is tricky.  the basic idea is to push out the range endpoints
  // to truly enclose the blocks that we will affect
  
  nsCOMPtr<nsIDOMNode> listStartNode;
  nsCOMPtr<nsIDOMNode> listEndNode;
  PRInt32 listStartOffset, listEndOffset;
  nsCOMPtr<nsIDOMRange> opRange;
  
  res = GetPromotedPoint( kStart, startNode, startOffset, kMakeList, &listStartNode, &listStartOffset);
  if (NS_FAILED(res)) return res;
  res = GetPromotedPoint( kEnd, endNode, endOffset, kMakeList, &listEndNode, &listEndOffset);
  if (NS_FAILED(res)) return res;
  res = selectionRange->Clone(getter_AddRefs(opRange));
  if (NS_FAILED(res)) return res;
  opRange->SetStart(listStartNode, listStartOffset);
  if (NS_FAILED(res)) return res;
  opRange->SetEnd(listEndNode, listEndOffset);
  if (NS_FAILED(res)) return res;

  // use subtree iterator to boogie over the snazzy new range
  // we wil gather the top level nodes to process into a list
  nsCOMPtr<nsIContentIterator> iter;
  res = nsComponentManager::CreateInstance(kSubtreeIteratorCID,
                                        nsnull,
                                        nsIContentIterator::GetIID(), 
                                        getter_AddRefs(iter));
  if (NS_FAILED(res)) return res;
  res = iter->Init(opRange);
  if (NS_FAILED(res)) return res;
    
  nsCOMPtr<nsISupportsArray> list;
  res = NS_NewISupportsArray(getter_AddRefs(list));

  while (NS_COMFALSE == iter->IsDone())
  {
    nsCOMPtr<nsIDOMNode> node;
    nsCOMPtr<nsIContent> content;
    res = iter->CurrentNode(getter_AddRefs(content));
    node = do_QueryInterface(content);
    if ((NS_SUCCEEDED(res)) && node)
    {
      // can't manipulate tree while using subtree iterator, or we will 
      // confuse it's little mind
      // instead chuck the results into an array.
      list->AppendElement(node);
    }
    res = iter->Next();
    if (NS_FAILED(res)) return res;
  }
  
  // if we ended up with any nodes in the list that aren't blocknodes, 
  // find their block parent instead and use that.
  
    // i started writing this and then the sky fell.  there are big questions
    // about what to do here.  i may need to switch rom think about an array of
    // nodes to act on to instead think of an array of ranges to act on.
  
  // Next we remove all the <br>'s in the array.  This won't prevent <br>'s 
  // inside of <p>'s from being significant - those <br>'s are not hit by 
  // the subtree iterator, since they are enclosed in a <p>.
  
  // We also remove all non-editable nodes.  Leave them be.
  
  PRUint32 listCount;
  PRInt32 i;
  list->Count(&listCount);
  for (i=listCount-1; i>=0; i--)
  {
    nsISupports *thingy = list->ElementAt(i);
    nsCOMPtr<nsIDOMNode> testNode( do_QueryInterface(thingy) );
    if (IsBreak(testNode))
    {
      list->RemoveElementAt(i);
    }
    else if (!nsEditor::IsEditable(testNode))
    {
      list->RemoveElementAt(i);
    }
  }
  
  // if there is only one node in the array, and it is a list, div, or blockquote,
  // then look inside of it until we find what we want to make a list out of.
  if (listCount == 1)
  {
    nsISupports *thingy;
    thingy = list->ElementAt(0);
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(thingy) );
    
    while (IsDiv(curNode) || IsOrderedList(curNode) || IsUnorderedList(curNode) || IsBlockquote(curNode))
    {
      // dive as long as there is only one child, and it is a list, div, or blockquote
      PRUint32 numChildren;
      res = nsEditor::CountEditableChildren(curNode, numChildren);
      if (NS_FAILED(res)) return res;
      
      if (numChildren == 1)
      {
        // keep diving
        nsCOMPtr <nsIDOMNode> tmpNode = nsEditor::GetChildAt(curNode, 0);
        // check editablility XXX moose
        curNode = tmpNode;
      }
      else
      {
        // stop diving
        break;
      }
    }
    // we've found innermost list/blockquote/div: 
    // replace the one node in the array with this node
    list->ReplaceElementAt(curNode, 0);
  }

  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.
  
  list->Count(&listCount);
  nsVoidArray transitionList;
  nsCOMPtr<nsIDOMNode> prevElementParent;
  nsCOMPtr<nsIDOMNode> curElementParent;
  
  for (i=0; i<listCount; i++)
  {
    nsISupports *thingy;
    thingy = list->ElementAt(i);
    nsCOMPtr<nsIDOMNode> transNode( do_QueryInterface(thingy) );
    transNode->GetParentNode(getter_AddRefs(curElementParent));
    if (curElementParent != prevElementParent)
    {
      transitionList.InsertElementAt((void*)PR_TRUE,i);  // different parents: transition point
    }
    else
    {
      transitionList.InsertElementAt((void*)PR_FALSE,i); // same parents: these nodes grew up together
    }
    prevElementParent = curElementParent;
  }
  
  
  // Ok, now go through all the nodes and put then in the list, 
  // or whatever is approriate.  Wohoo!
  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curList;
    
    for (i=0; i<listCount; i++)
    {
      // here's where we actually figure out what to do
      nsISupports *thingy;
      thingy = list->ElementAt(i);
      nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(thingy) );
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
            nsAutoString blockType("ol");
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
            nsAutoString blockType("ul");
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
          // XXX moose
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
      }
    
      // if curNode isn't a list item, we must wrap it in one
      nsCOMPtr<nsIDOMNode> listItem;
      if (!IsListItem(curNode))
      {
        nsAutoString listItemType = "li";
        res = InsertContainerAbove(curNode, &listItem, listItemType);
        if (NS_FAILED(res)) return res;
      }
      else
      {
        listItem = curNode;
      }
    
      // tuck the listItem into the end of the active list
      PRUint32 listLen;
      res = mEditor->GetLengthOfDOMNode(curList, listLen);
      if (NS_FAILED(res)) return res;
      res = mEditor->DeleteNode(listItem);
      if (NS_FAILED(res)) return res;
      res = mEditor->InsertNode(listItem, curList, listLen);
      if (NS_FAILED(res)) return res;
    }

  return res;
}


nsresult
nsHTMLEditRules::WillIndent(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_TRUE;
  
  nsAutoSelectionReset selectionResetter(aSelection);
  nsresult res = NS_OK;
  
  // get selection range - XXX generalize to collection of ranges
  // gather up factoids about the range
  nsCOMPtr<nsIDOMRange> selectionRange;
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 startOffset, endOffset;
  
  res = aSelection->GetRangeAt(0,getter_AddRefs(selectionRange));
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
  
  nsCOMPtr<nsIDOMNode> quoteStartNode;
  nsCOMPtr<nsIDOMNode> quoteEndNode;
  PRInt32 quoteStartOffset, quoteEndOffset;
  nsCOMPtr<nsIDOMRange> opRange;
  
  res = GetPromotedPoint( kStart, startNode, startOffset, kIndent, &quoteStartNode, &quoteStartOffset);
  if (NS_FAILED(res)) return res;
  res = GetPromotedPoint( kEnd, endNode, endOffset, kIndent, &quoteEndNode, &quoteEndOffset);
  if (NS_FAILED(res)) return res;
  res = selectionRange->Clone(getter_AddRefs(opRange));
  if (NS_FAILED(res)) return res;
  opRange->SetStart(quoteStartNode, quoteStartOffset);
  if (NS_FAILED(res)) return res;
  opRange->SetEnd(quoteEndNode, quoteEndOffset);
  if (NS_FAILED(res)) return res;

  // use subtree iterator to boogie over the snazzy new range
  // we wil gather the top level nodes to process into an array
  nsCOMPtr<nsIContentIterator> iter;
  res = nsComponentManager::CreateInstance(kSubtreeIteratorCID,
                                        nsnull,
                                        nsIContentIterator::GetIID(), 
                                        getter_AddRefs(iter));
  if (NS_FAILED(res)) return res;
  res = iter->Init(opRange);
  if (NS_FAILED(res)) return res;
    
  nsCOMPtr<nsISupportsArray> list;
  res = NS_NewISupportsArray(getter_AddRefs(list));

  while (NS_COMFALSE == iter->IsDone())
  {
    nsCOMPtr<nsIDOMNode> node;
    nsCOMPtr<nsIContent> content;
    res = iter->CurrentNode(getter_AddRefs(content));
    node = do_QueryInterface(content);
    if ((NS_SUCCEEDED(res)) && node)
    {
      // can't manipulate tree while using subtree iterator, or we will 
      // confuse it's little mind
      // instead chuck the results into an array.
      list->AppendElement(node);
    }
    res = iter->Next();
    if (NS_FAILED(res)) return res;
  }
  
  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.
  
  PRUint32 listCount;
  PRInt32 i;
  list->Count(&listCount);
  nsVoidArray transitionList;
  nsCOMPtr<nsIDOMNode> prevElementParent;
  nsCOMPtr<nsIDOMNode> curElementParent;
  
  for (i=0; i<listCount; i++)
  {
    nsISupports *thingy;
    thingy = list->ElementAt(i);
    nsCOMPtr<nsIDOMNode> transNode( do_QueryInterface(thingy) );
    transNode->GetParentNode(getter_AddRefs(curElementParent));
    if (curElementParent != prevElementParent)
    {
      transitionList.InsertElementAt((void*)PR_TRUE,i);  // different parents: transition point
    }
    else
    {
      transitionList.InsertElementAt((void*)PR_FALSE,i); // same parents: these nodes grew up together
    }
    prevElementParent = curElementParent;
  }
  
  
  // Ok, now go through all the lodes and put them in a blockquote, 
  // or whatever is appropriate.  Wohoo!
  
  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curQuote;
  for (i=0; i<listCount; i++)
  {
    // here's where we actually figure out what to do
    nsISupports *thingy;
    thingy = list->ElementAt(i);
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(thingy) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;
    
    // need to make a blockquote to put things in if we haven't already,
    // or if this node doesn't go in list we used earlier.
    if (!curQuote || transitionList[i])
    {
      nsAutoString quoteType("blockquote");
      res = mEditor->CreateNode(quoteType, curParent, offset, getter_AddRefs(curQuote));
      if (NS_FAILED(res)) return res;
      // curQuote is now the correct thing to put curNode in
    }
        
    // tuck the node into the end of the active blockquote
    PRUint32 listLen;
    res = mEditor->GetLengthOfDOMNode(curQuote, listLen);
    if (NS_FAILED(res)) return res;
    res = mEditor->DeleteNode(curNode);
    if (NS_FAILED(res)) return res;
    res = mEditor->InsertNode(curNode, curQuote, listLen);
    if (NS_FAILED(res)) return res;
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
  
  // get selection range - XXX generalize to collection of ranges
  // gather up factoids about the range
  nsCOMPtr<nsIDOMRange> selectionRange;
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 startOffset, endOffset;
  
  res = aSelection->GetRangeAt(0,getter_AddRefs(selectionRange));
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
  
  nsCOMPtr<nsIDOMNode> quoteStartNode;
  nsCOMPtr<nsIDOMNode> quoteEndNode;
  PRInt32 quoteStartOffset, quoteEndOffset;
  nsCOMPtr<nsIDOMRange> opRange;
  
  res = GetPromotedPoint( kStart, startNode, startOffset, kIndent, &quoteStartNode, &quoteStartOffset);
  if (NS_FAILED(res)) return res;
  res = GetPromotedPoint( kEnd, endNode, endOffset, kIndent, &quoteEndNode, &quoteEndOffset);
  if (NS_FAILED(res)) return res;
  res = selectionRange->Clone(getter_AddRefs(opRange));
  if (NS_FAILED(res)) return res;
  opRange->SetStart(quoteStartNode, quoteStartOffset);
  if (NS_FAILED(res)) return res;
  opRange->SetEnd(quoteEndNode, quoteEndOffset);
  if (NS_FAILED(res)) return res;

  // use subtree iterator to boogie over the snazzy new range
  // we wil gather the top level nodes to process into an array
  nsCOMPtr<nsIContentIterator> iter;
  res = nsComponentManager::CreateInstance(kSubtreeIteratorCID,
                                        nsnull,
                                        nsIContentIterator::GetIID(), 
                                        getter_AddRefs(iter));
  if (NS_FAILED(res)) return res;
  res = iter->Init(opRange);
  if (NS_FAILED(res)) return res;
    
  nsCOMPtr<nsISupportsArray> list;
  res = NS_NewISupportsArray(getter_AddRefs(list));

  while (NS_COMFALSE == iter->IsDone())
  {
    nsCOMPtr<nsIDOMNode> node;
    nsCOMPtr<nsIContent> content;
    res = iter->CurrentNode(getter_AddRefs(content));
    node = do_QueryInterface(content);
    if ((NS_SUCCEEDED(res)) && node)
    {
      // can't manipulate tree while using subtree iterator, or we will 
      // confuse it's little mind
      // instead chuck the results into an array.
      list->AppendElement(node);
    }
    res = iter->Next();
    if (NS_FAILED(res)) return res;
  }
  
  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.
  
  PRUint32 listCount;
  PRInt32 i;
  list->Count(&listCount);
  nsVoidArray transitionList;
  nsCOMPtr<nsIDOMNode> prevElementParent;
  nsCOMPtr<nsIDOMNode> curElementParent;
  
  for (i=0; i<listCount; i++)
  {
    nsISupports *thingy;
    thingy = list->ElementAt(i);
    nsCOMPtr<nsIDOMNode> transNode( do_QueryInterface(thingy) );
    transNode->GetParentNode(getter_AddRefs(curElementParent));
    if (curElementParent != prevElementParent)
    {
      transitionList.InsertElementAt((void*)PR_TRUE,i);  // different parents: transition point
    }
    else
    {
      transitionList.InsertElementAt((void*)PR_FALSE,i); // same parents: these nodes grew up together
    }
    prevElementParent = curElementParent;
  }
  
  
  // Ok, now go through all the lodes and remove a level of blockquoting, 
  // or whatever is appropriate.  Wohoo!
  
  nsCOMPtr<nsIDOMNode> curParent;
  for (i=0; i<listCount; i++)
  {
    // here's where we actually figure out what to do
    nsISupports *thingy;
    thingy = list->ElementAt(i);
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(thingy) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, &curParent, &offset);
    if (NS_FAILED(res)) return res;
    
    if (transitionList[i])
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
  *aCancel = PR_TRUE;
  
  nsAutoSelectionReset selectionResetter(aSelection);
  nsresult res = NS_OK;
  
  // get selection range - XXX generalize to collection of ranges
  // gather up factoids about the range
  nsCOMPtr<nsIDOMRange> selectionRange;
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 startOffset, endOffset;
  
  res = aSelection->GetRangeAt(0,getter_AddRefs(selectionRange));
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
  
  nsCOMPtr<nsIDOMNode> quoteStartNode;
  nsCOMPtr<nsIDOMNode> quoteEndNode;
  PRInt32 quoteStartOffset, quoteEndOffset;
  nsCOMPtr<nsIDOMRange> opRange;
  
  res = GetPromotedPoint( kStart, startNode, startOffset, kIndent, &quoteStartNode, &quoteStartOffset);
  if (NS_FAILED(res)) return res;
  res = GetPromotedPoint( kEnd, endNode, endOffset, kIndent, &quoteEndNode, &quoteEndOffset);
  if (NS_FAILED(res)) return res;
  res = selectionRange->Clone(getter_AddRefs(opRange));
  if (NS_FAILED(res)) return res;
  opRange->SetStart(quoteStartNode, quoteStartOffset);
  if (NS_FAILED(res)) return res;
  opRange->SetEnd(quoteEndNode, quoteEndOffset);
  if (NS_FAILED(res)) return res;

  // use subtree iterator to boogie over the snazzy new range
  // we wil gather the top level nodes to process into an array
  nsCOMPtr<nsIContentIterator> iter;
  res = nsComponentManager::CreateInstance(kSubtreeIteratorCID,
                                        nsnull,
                                        nsIContentIterator::GetIID(), 
                                        getter_AddRefs(iter));
  if (NS_FAILED(res)) return res;
  res = iter->Init(opRange);
  if (NS_FAILED(res)) return res;
    
  nsCOMPtr<nsISupportsArray> list;
  res = NS_NewISupportsArray(getter_AddRefs(list));

  while (NS_COMFALSE == iter->IsDone())
  {
    nsCOMPtr<nsIDOMNode> node;
    nsCOMPtr<nsIContent> content;
    res = iter->CurrentNode(getter_AddRefs(content));
    node = do_QueryInterface(content);
    if ((NS_SUCCEEDED(res)) && node)
    {
      // can't manipulate tree while using subtree iterator, or we will 
      // confuse it's little mind
      // instead chuck the results into an array.
      list->AppendElement(node);
    }
    res = iter->Next();
    if (NS_FAILED(res)) return res;
  }
  
  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.
  
  PRUint32 listCount;
  PRInt32 i;
  list->Count(&listCount);
  nsVoidArray transitionList;
  nsCOMPtr<nsIDOMNode> prevElementParent;
  nsCOMPtr<nsIDOMNode> curElementParent;
  
  for (i=0; i<listCount; i++)
  {
    nsISupports *thingy;
    thingy = list->ElementAt(i);
    nsCOMPtr<nsIDOMNode> transNode( do_QueryInterface(thingy) );
    transNode->GetParentNode(getter_AddRefs(curElementParent));
    if (curElementParent != prevElementParent)
    {
      transitionList.InsertElementAt((void*)PR_TRUE,i);  // different parents: transition point
    }
    else
    {
      transitionList.InsertElementAt((void*)PR_FALSE,i); // same parents: these nodes grew up together
    }
    prevElementParent = curElementParent;
  }
  
  
  // Ok, now go through all the lodes and give them an align attrib or put them in a div, 
  // or whatever is appropriate.  Wohoo!
  
  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curDiv;
  for (i=0; i<listCount; i++)
  {
    // here's where we actually figure out what to do
    nsISupports *thingy;
    thingy = list->ElementAt(i);
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(thingy) );
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


nsresult
nsHTMLEditRules::WillMakeHeader(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  
  nsresult res = NS_OK;
  
  return res;
}


nsresult
nsHTMLEditRules::WillMakeAddress(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  
  nsresult res = NS_OK;
  
  return res;
}


nsresult
nsHTMLEditRules::WillMakePRE(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  
  nsresult res = NS_OK;
  
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
    if (nsEditor::IsEditable(child)) 
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
    if (nsEditor::IsEditable(child)) 
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
  
  if (aWhere == kStart)
  {
    // some special casing for text nodes
    if (nsEditor::IsTextNode(aNode))  
    {
      nsEditor::GetNodeLocation(aNode, &parent, &offset);
    }
    else
    {
      node = nsEditor::GetChildAt(parent,offset);
    }
    
    // finding the real start for this point.  look up the tree for as long as we are the 
    // first node in the container, and as long as we haven't hit the body node.
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
    }
    offset++;  // since this is going to be used for a range _endpoint_, we want to be after the node
    
    // finding the real end for this point.  look up the tree for as long as we are the 
    // last node in the container, and as long as we haven't hit the body node.
    while ((IsLastNode(node)) && (!IsBody(parent)))
    {
      node = parent;
      res = nsEditor::GetNodeLocation(node, &parent, &offset);
      if (NS_FAILED(res)) return res;
      offset++;
    } 
    *outNode = parent;
    *outOffset = offset;
    return res;
  }
  
  return res;
}

///////////////////////////////////////////////////////////////////////////
// ReplaceContainer: replace inNode with a new node (outNode) which is contructed 
//                   to be of type aNodeType.  Put inNodes children into outNode.
//                   Callers responsibility to make sure inNode's children can 
//                   go in outNode.
nsresult
nsHTMLEditRules::ReplaceContainer(nsIDOMNode *inNode, 
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
  res = mEditor->CreateNode(aNodeType, parent, offset, getter_AddRefs(*outNode));
  if (NS_FAILED(res)) return res;
  PRBool bHasMoreChildren;
  inNode->HasChildNodes(&bHasMoreChildren);
  nsCOMPtr<nsIDOMNode> child;
  offset = 0;
  while (bHasMoreChildren)
  {
    inNode->GetFirstChild(getter_AddRefs(child));
    res = mEditor->DeleteNode(child);
    if (NS_FAILED(res)) return res;
    res = mEditor->InsertNode(child, *outNode, offset);
    if (NS_FAILED(res)) return res;
    inNode->HasChildNodes(&bHasMoreChildren);
    offset++;
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
  if (sibling && mEditor->IsTextNode(sibling) && mEditor->IsEditable(sibling))
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













