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
    case kMakeHeader:
      return WillMakeHeader(aSelection, aCancel);
    case kMakeAddress:
      return WillMakeAddress(aSelection, aCancel);
    case kMakePRE:
      return WillMakePRE(aSelection, aCancel);
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
  *aCancel = PR_FALSE;
  nsresult res;

  char specialChars[] = {'\t',' ',nbsp,'\n',0};
  
  mEditor->BeginTransaction();  // we have to batch this in case there are returns
                                // Insert Break txns don't auto merge with insert text txns

  // strategy: there are simple cases and harder cases.  The harder cases
  // we handle recursively by breaking them into a series of simple cases.
  // The simple cases are:
  // 1) a single space
  // 2) a single nbsp
  // 3) a single tab
  // 4) a single return
  // 5) a run of chars containing no spaces, nbsp's, tabs, or returns
  char nbspStr[2] = {nbsp, 0};
  
  // is it a solo tab?
  if (*inString == "\t" )
  {
    res = InsertTab(aSelection,aCancel,aTxn,outString);
  }
  // is it a solo space?
  else if (*inString == " ")
  {
    res = InsertSpace(aSelection,aCancel,aTxn,outString);
  }
  // is it a solo nbsp?
  else if (*inString == nbspStr)
  {
    res = InsertSpace(aSelection,aCancel,aTxn,outString);
  }
  // is it a solo return?
  else if (*inString == "\n")
  {
    // special case - cancel default handling
    *aCancel = PR_TRUE;
    res = mEditor->InsertBreak();
  }
  else
  {
    // is it an innocous run of chars?  no spaces, nbsps, returns, tabs?
    PRInt32 pos = inString->FindCharInSet(specialChars);
    if (pos == -1)
    {
      // no special chars, easy case
      res = nsTextEditRules::WillInsertText(aSelection, aCancel, aTxn, inString, outString, typeInState, aMaxLength);
    }
    else
    {
      // else we need to break it up into two parts and recurse
      *aCancel = PR_TRUE;
      nsString firstString;
      nsString restOfString(*inString);
      // if first char is special, then use just it
      if (pos == 0) pos = 1;
      inString->Left(firstString, pos);
      restOfString.Cut(0, pos);
      if (NS_SUCCEEDED(mEditor->InsertText(firstString)))
      {
        res = mEditor->InsertText(restOfString);
      }
    }
  }
  mEditor->EndTransaction();
  return res;
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
nsHTMLEditRules::WillDeleteSelection(nsIDOMSelection *aSelection, nsIEditor::ESelectionCollapseDirection aAction, PRBool *aCancel)
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
        res = mEditor->DeleteSelectionImpl(aAction);
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
  *aCancel = PR_FALSE;
  
  nsAutoSelectionReset selectionResetter(aSelection);
  nsresult res;
  
  // check for collapsed selection in empty block or after a <br>.
  // Either way, we want to get the default behavior of creating
  // an empty list.
  
  PRBool isCollapsed;
  res = aSelection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;
  if (isCollapsed) 
  {
    // is it after a <br>?  Two possibilities: selection right after <br>;
    // and selection at front of text node following <br>
    nsCOMPtr<nsIDOMNode> parent, prevChild;
    PRInt32 offset;
    res = nsEditor::GetStartNodeAndOffset(aSelection, &parent, &offset);
    if (nsEditor::IsTextNode(parent) && !offset)
    {
      nsCOMPtr<nsIDOMNode> node = parent;
      res = nsEditor::GetNodeLocation(node, &parent, &offset);
      if (NS_FAILED(res)) return res;
    }
    if (offset)
    {
      prevChild = nsEditor::GetChildAt(parent, offset-1);
      if (prevChild && IsBreak(prevChild)) 
        return NS_OK; // insertion point after break
    }
    
    // otherwise check for being in an empty block
    PRBool isEmptyBlock;
    res = IsEmptyBlock(parent, &isEmptyBlock);
    if (NS_FAILED(res)) return res;
    if (isEmptyBlock) return NS_OK;
  }
  
  // ok, we aren't vreating a new empty list.  Instead we are converting
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
                                     
  // if we ended up with any nodes in the list that aren't blocknodes, 
  // find their block parent instead and use that.
  
    // i started writing this and then the sky fell.  there are big questions
    // about what to do here.  i may need to switch from thinking about an array of
    // nodes to act on to instead think of an array of ranges to act on.
  
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
          // check editablility XXX moose
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
        prevListItem = 0;
      }
    
      // if curNode is a Break, delete it, and quit remember prev list item
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
nsHTMLEditRules::WillMakeHeader(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_TRUE;
  
  nsAutoSelectionReset selectionResetter(aSelection);
  nsresult res;
  
  // Get the ranges for each break-delimited block
  nsCOMPtr<nsIEnumerator> enumerator;
  res = aSelection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_SUCCEEDED(res) && enumerator)
  {
    for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
    {
      nsCOMPtr<nsISupports> currentItem;
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if ((NS_SUCCEEDED(res)) && (currentItem))
      {
        nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
        PRBool isCollapsed;
        range->GetIsCollapsed(&isCollapsed);
        if (PR_FALSE==isCollapsed)  // don't make headers out of empty ranges
        {
          nsCOMPtr<nsISupportsArray> arrayOfRanges;
          res = mEditor->GetBlockSectionsForRange(range, arrayOfRanges);
          PRUint32 rangeCount;
          res = arrayOfRanges->Count(&rangeCount);
          if (NS_FAILED(res)) return res;
  
          PRInt32 i;
          nsCOMPtr<nsIDOMRange> opRange;
          nsCOMPtr<nsISupports> isupports;

          for (i = 0; i < rangeCount; i++)
          {
            isupports = (dont_AddRef)(arrayOfRanges->ElementAt(i));
            opRange = do_QueryInterface(isupports);
  //          ConvertRangeToTag
          }
        }
      }
    }
  }
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
    if (IsListItem(curNode))
    {
      if (!curList || transitionList[i])
      {
        nsAutoString quoteType("ol");  // XXX - get correct list type
        if (mEditor->CanContainTag(curParent,quoteType))
        {
          res = mEditor->CreateNode(quoteType, curParent, offset, getter_AddRefs(curList));
          if (NS_FAILED(res)) return res;
          // curQuote is now the correct thing to put curNode in
        }
        else
        {
          printf("trying to put a list in a bad place\n");     
        }
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
  
  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  
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
      if (!node) node = parent;  
    }
    
    // if this is an inline node, back up through any prior inline nodes that
    // aren't across a <br> from us, and that are enclosed in the same block.
    
    if (!nsEditor::IsBlockNode(node))
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
      if (!node) node = parent;  
    }

    if (node)
      offset++;  // since this is going to be used for a range _endpoint_, we want to be after the node
    else
      node = parent;
    
    // if this is an inline node, look ahead through any further inline nodes that
    // aren't across a <br> from us, and that are enclosed in the same block.
    
    if (!nsEditor::IsBlockNode(node))
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

  PRInt32 newOffset;
  nsresult res = mEditor->SplitNodeDeep( aListItem, aNode, aOffset, &newOffset);
  if (NS_FAILED(res)) return res;
  res = aSelection->Collapse(aNode,0);
  return res;
}


