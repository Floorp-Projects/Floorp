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

#include "nsTextEditRules.h"
#include "nsTextEditor.h"
#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMSelection.h"
#include "nsIDOMRange.h"
#include "nsIDOMCharacterData.h"
#include "nsIEnumerator.h"
#include "nsIContent.h"

const static char* kMOZEditorBogusNodeAttr="MOZ_EDITOR_BOGUS_NODE";
const static char* kMOZEditorBogusNodeValue="TRUE";

static PRBool NodeIsType(nsIDOMNode *aNode, nsIAtom *aTag)
{
  nsCOMPtr<nsIDOMElement>element;
  element = do_QueryInterface(aNode);
  if (element)
  {
    nsAutoString tag;
    element->GetTagName(tag);
    if (tag.Equals(aTag))
    {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRBool IsEditable(nsIDOMNode *aNode)
{
  if (!aNode) return PR_FALSE;
  nsCOMPtr<nsIDOMElement>element;
  element = do_QueryInterface(aNode);
  if (element)
  {
    nsAutoString att(kMOZEditorBogusNodeAttr);
    nsAutoString val;
    nsresult result = element->GetAttribute(att, val);
    if (val.Equals(kMOZEditorBogusNodeValue)) {
      return PR_FALSE;
    }
    else {
      return PR_TRUE;
    }
  }
  else
  { 
    nsCOMPtr<nsIDOMCharacterData>text;
    text = do_QueryInterface(aNode);
    if (text)
    {
      nsAutoString data;
      text->GetData(data);
      if (0==data.Length()) {
        return PR_FALSE;
      }
      if ('\n'==data[0]) {
        return PR_FALSE;
      }
      else {
        return PR_TRUE;
      }
    }
  }
  return PR_TRUE;
}


nsTextEditRules::nsTextEditRules()
{
  mEditor = nsnull;
}

nsTextEditRules::~nsTextEditRules()
{
   // do NOT delete mEditor here.  We do not hold a ref count to mEditor.  mEditor owns our lifespan.
}

NS_IMETHODIMP
nsTextEditRules::Init(nsTextEditor *aEditor)
{
  // null aNextRule is ok
  if (!aEditor) { return NS_ERROR_NULL_POINTER; }
  mEditor = aEditor;  // we hold a non-refcounted reference back to our editor
  return NS_OK;
}


NS_IMETHODIMP
nsTextEditRules::WillInsert(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  
  // check for the magic content node and delete it if it exists
  nsCOMPtr<nsIDOMDocument>doc;
  mEditor->GetDocument(getter_AddRefs(doc));  
  nsCOMPtr<nsIDOMNodeList>nodeList;
  nsAutoString bodyTag = "body";
  nsresult result = doc->GetElementsByTagName(bodyTag, getter_AddRefs(nodeList));
  if ((NS_SUCCEEDED(result)) && nodeList)
  {
    PRUint32 count;
    nodeList->GetLength(&count);
    NS_ASSERTION(1==count, "there is not exactly 1 body in the document!");
    nsCOMPtr<nsIDOMNode>bodyNode;
    result = nodeList->Item(0, getter_AddRefs(bodyNode));
    if ((NS_SUCCEEDED(result)) && bodyNode)
    { // now we've got the body tag.
      // iterate the body tag, looking for editable content
      // if the magic node is found, delete it
      PRBool foundBogusContent=PR_TRUE;
      nsCOMPtr<nsIDOMNode>bodyChild;  // a child of the body, for iteration
      nsCOMPtr<nsIDOMNode>bogusNode;  // this will be the magic node
      result = bodyNode->GetFirstChild(getter_AddRefs(bodyChild));        
      while ((NS_SUCCEEDED(result)) && bodyChild)
      {
        bogusNode = do_QueryInterface(bodyChild);
        if (PR_TRUE==IsEditable(bodyChild))
        {
          foundBogusContent = PR_FALSE;
          break;
        }
        nsCOMPtr<nsIDOMNode>temp;
        bodyChild->GetNextSibling(getter_AddRefs(temp));
        bodyChild = do_QueryInterface(temp);
      }
      if (PR_TRUE==foundBogusContent)
      {
        mEditor->DeleteNode(bogusNode);
        // there is no longer any legit selection, so clear it.
        aSelection->ClearSelection();
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTextEditRules::DidInsert(nsIDOMSelection *aSelection, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTextEditRules::WillInsertText(nsIDOMSelection *aSelection, 
                                const nsString& aInputString,
                                PRBool *aCancel,
                                nsString& aOutputString)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  // by default, we insert what we're told to insert
  aOutputString = aInputString;
  return WillInsert(aSelection, aCancel);
}

NS_IMETHODIMP
nsTextEditRules::DidInsertText(nsIDOMSelection *aSelection, 
                               const nsString& aStringToInsert, 
                               nsresult aResult)
{
  return DidInsert(aSelection, aResult);
}

NS_IMETHODIMP
nsTextEditRules::WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  return WillInsert(aSelection, aCancel);
}

// XXX: this code is all experimental, and has no effect on the content model yet
//      the point here is to collapse adjacent BR's into P's
NS_IMETHODIMP
nsTextEditRules::DidInsertBreak(nsIDOMSelection *aSelection, nsresult aResult)
{
  nsresult result = aResult;  // if aResult is an error, we return it.
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  PRBool isCollapsed;
  aSelection->IsCollapsed(&isCollapsed);
  NS_ASSERTION(PR_TRUE==isCollapsed, "selection not collapsed after insert break.");
  // if the insert break resulted in consecutive BR tags, 
  // collapse the two BR tags into a single P
  if (NS_SUCCEEDED(aResult))  // note we're checking aResult, the param that tells us if the insert break happened or not
  {
    nsCOMPtr<nsIEnumerator> enumerator;
    enumerator = do_QueryInterface(aSelection,&result);
    if (enumerator)
    {
      enumerator->First(); 
      nsISupports *currentItem;
      result = enumerator->CurrentItem(&currentItem);
      if ((NS_SUCCEEDED(result)) && currentItem)
      {
        result = NS_ERROR_UNEXPECTED; 
        nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
        if (range)
        {
          nsIAtom *brTag = NS_NewAtom("BR");
          nsCOMPtr<nsIDOMNode> startNode;
          result = range->GetStartParent(getter_AddRefs(startNode));
          if ((NS_SUCCEEDED(result)) && startNode)
          {
            PRInt32 offset;
            range->GetStartOffset(&offset);
            nsCOMPtr<nsIDOMNodeList>startNodeChildren;
            result = startNode->GetChildNodes(getter_AddRefs(startNodeChildren));
            if ((NS_SUCCEEDED(result)) && startNodeChildren)
            {              
              nsCOMPtr<nsIDOMNode> selectedNode;
              result = startNodeChildren->Item(offset, getter_AddRefs(selectedNode));
              if ((NS_SUCCEEDED(result)) && selectedNode)
              {
                nsCOMPtr<nsIDOMNode> prevNode;
                result = selectedNode->GetPreviousSibling(getter_AddRefs(prevNode));
                if ((NS_SUCCEEDED(result)) && prevNode)
                {
                  if (PR_TRUE==NodeIsType(prevNode, brTag))
                  { // the previous node is a BR, check it's siblings
                    nsCOMPtr<nsIDOMNode> leftNode;
                    result = prevNode->GetPreviousSibling(getter_AddRefs(leftNode));
                    if ((NS_SUCCEEDED(result)) && leftNode)
                    {
                      if (PR_TRUE==NodeIsType(leftNode, brTag))
                      { // left sibling is also a BR, collapse
                        printf("1\n");
                      }
                      else
                      {
                        if (PR_TRUE==NodeIsType(selectedNode, brTag))
                        { // right sibling is also a BR, collapse
                          printf("2\n");
                        }
                      }
                    }
                  }
                }
                // now check the next node from selectedNode
                nsCOMPtr<nsIDOMNode> nextNode;
                result = selectedNode->GetNextSibling(getter_AddRefs(nextNode));
                if ((NS_SUCCEEDED(result)) && nextNode)
                {
                  if (PR_TRUE==NodeIsType(nextNode, brTag))
                  { // the previous node is a BR, check it's siblings
                    nsCOMPtr<nsIDOMNode> rightNode;
                    result = prevNode->GetNextSibling(getter_AddRefs(rightNode));
                    if ((NS_SUCCEEDED(result)) && rightNode)
                    {
                      if (PR_TRUE==NodeIsType(rightNode, brTag))
                      { // right sibling is also a BR, collapse
                        printf("3\n");
                      }
                      else
                      {
                        if (PR_TRUE==NodeIsType(selectedNode, brTag))
                        { // left sibling is also a BR, collapse
                          printf("4\n");
                        }
                      }
                    }
                  }
                }
              }
            }
          }
          NS_RELEASE(brTag);
        }
      }
    }
  }
  return result;
}

NS_IMETHODIMP
nsTextEditRules::GetInsertBreakTag(nsIAtom **aTag)
{
  if (!aTag) { return NS_ERROR_NULL_POINTER; }
  *aTag = NS_NewAtom("BR");
  return NS_OK;
}


NS_IMETHODIMP
nsTextEditRules::WillDeleteSelection(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  
  // if there is only bogus content, cancel the operation
  nsCOMPtr<nsIDOMNode>node;
  PRInt32 offset;
  nsresult result = aSelection->GetAnchorNodeAndOffset(getter_AddRefs(node), &offset);
  if ((NS_SUCCEEDED(result)) && node)
  {
    nsCOMPtr<nsIDOMNode>parent;
    parent = do_QueryInterface(node);
    while (node)
    { //if we find the bogus node, cancel the operation
      nsCOMPtr<nsIDOMElement>element;
      element = do_QueryInterface(parent);
      if (element)
      {
        nsAutoString att(kMOZEditorBogusNodeAttr);
        nsAutoString val;
        nsresult result = element->GetAttribute(att, val);
        if (val.Equals(kMOZEditorBogusNodeValue)) {
          *aCancel = PR_TRUE;
          return NS_OK;
        }
      }
      // walk up the content hierarchy
      parent->GetParentNode(getter_AddRefs(node));
      parent = do_QueryInterface(node);
    }
  }
  return NS_OK;
}

// if the document is empty, insert a bogus text node with a &nbsp;
NS_IMETHODIMP
nsTextEditRules::DidDeleteSelection(nsIDOMSelection *aSelection, nsresult aResult)
{
  nsresult result = aResult;  // if aResult is an error, we just return it
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  PRBool isCollapsed;
  aSelection->IsCollapsed(&isCollapsed);
  NS_ASSERTION(PR_TRUE==isCollapsed, "selection not collapsed after delete selection.");
  // if the delete selection resulted in no content 
  // insert a special bogus text node with a &nbsp; character in it.
  if (NS_SUCCEEDED(aResult))  // note we're checking aResult, the param that tells us if the insert break happened or not
  {
    nsCOMPtr<nsIDOMDocument>doc;
    mEditor->GetDocument(getter_AddRefs(doc));  
    nsCOMPtr<nsIDOMNodeList>nodeList;
    nsAutoString bodyTag = "body";
    nsresult result = doc->GetElementsByTagName(bodyTag, getter_AddRefs(nodeList));
    if ((NS_SUCCEEDED(result)) && nodeList)
    {
      PRUint32 count;
      nodeList->GetLength(&count);
      NS_ASSERTION(1==count, "there is not exactly 1 body in the document!");
      nsCOMPtr<nsIDOMNode>bodyNode;
      result = nodeList->Item(0, getter_AddRefs(bodyNode));
      if ((NS_SUCCEEDED(result)) && bodyNode)
      { // now we've got the body tag.
        // iterate the body tag, looking for editable content
        // if no editable content is found, insert the bogus node
        PRBool needsBogusContent=PR_TRUE;
        nsCOMPtr<nsIDOMNode>bodyChild;
        result = bodyNode->GetFirstChild(getter_AddRefs(bodyChild));        
        while ((NS_SUCCEEDED(result)) && bodyChild)
        { 
          if (PR_TRUE==IsEditable(bodyChild))
          {
            needsBogusContent = PR_FALSE;
            break;
          }
          nsCOMPtr<nsIDOMNode>temp;
          bodyChild->GetNextSibling(getter_AddRefs(temp));
          bodyChild = do_QueryInterface(temp);
        }
        if (PR_TRUE==needsBogusContent)
        {
          nsCOMPtr<nsIDOMNode>newPNode;
          result = mEditor->CreateNode(nsAutoString("P"), bodyNode, 0, 
                                       getter_AddRefs(newPNode));
          if ((NS_SUCCEEDED(result)) && newPNode)
          {
            nsCOMPtr<nsIDOMNode>newTNode;
            result = mEditor->CreateNode(nsIEditor::GetTextNodeTag(), newPNode, 0, 
                                         getter_AddRefs(newTNode));
            if ((NS_SUCCEEDED(result)) && newTNode)
            {
              nsCOMPtr<nsIDOMCharacterData>newNodeAsText;
              newNodeAsText = do_QueryInterface(newTNode);
              if (newNodeAsText)
              {
                nsAutoString data;
                data += 160;
                newNodeAsText->SetData(data);
                aSelection->Collapse(newTNode, 0);
              }
            }
            // make sure we know the PNode is bogus
            nsCOMPtr<nsIDOMElement>newPElement;
            newPElement = do_QueryInterface(newPNode);
            if (newPElement)
            {
              nsAutoString att(kMOZEditorBogusNodeAttr);
              nsAutoString val(kMOZEditorBogusNodeValue);
              newPElement->SetAttribute(att, val);
            }
          }
        }
      }
    }
  }
  return result;
}






