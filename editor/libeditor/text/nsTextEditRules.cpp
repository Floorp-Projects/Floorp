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
#include "nsEditor.h"
#include "PlaceholderTxn.h"
#include "InsertTextTxn.h"
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

static NS_DEFINE_IID(kPlaceholderTxnIID,  PLACEHOLDER_TXN_IID);

/*-------------------------------------------------------------------*
 *  Helper Functions 
 *-------------------------------------------------------------------*/

PRBool nsTextEditRules::NodeIsType(nsIDOMNode *aNode, nsIAtom *aTag)
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

PRBool nsTextEditRules::IsEditable(nsIDOMNode *aNode)
{
  if (!aNode) return PR_FALSE;
  nsCOMPtr<nsIDOMElement>element;
  element = do_QueryInterface(aNode);
  if (element)
  {
    nsAutoString att(kMOZEditorBogusNodeAttr);
    nsAutoString val;
    (void)element->GetAttribute(att, val);
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


/*-------------------------------------------------------------------*
 *  Constructor/Destructor 
 *-------------------------------------------------------------------*/


nsTextEditRules::nsTextEditRules()
{
  mEditor = nsnull;
}

nsTextEditRules::~nsTextEditRules()
{
   // do NOT delete mEditor here.  We do not hold a ref count to mEditor.  mEditor owns our lifespan.
}



/*-------------------------------------------------------------------*
 *  Public methods 
 *-------------------------------------------------------------------*/


NS_IMETHODIMP
nsTextEditRules::Init(nsEditor *aEditor)
{
  // null aNextRule is ok
  if (!aEditor) { return NS_ERROR_NULL_POINTER; }
  mEditor = aEditor;  // we hold a non-refcounted reference back to our editor
  return NS_OK;
}

NS_IMETHODIMP
nsTextEditRules::WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsTextEditRules::DidInsertBreak(nsIDOMSelection *aSelection, nsresult aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsTextEditRules::WillInsert(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  
  // check for the magic content node and delete it if it exists
  if (mBogusNode)
  {
    mEditor->DeleteNode(mBogusNode);
    mBogusNode = do_QueryInterface(nsnull);
    // there is no longer any legit selection, so clear it.
    aSelection->ClearSelection();
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
                                nsString& aOutputString,
                                PlaceholderTxn **aTxn)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  // by default, we insert what we're told to insert
  aOutputString = aInputString;
  if (mBogusNode)
  {
    nsresult result = TransactionFactory::GetNewTransaction(kPlaceholderTxnIID, (EditTxn **)aTxn);
    if (NS_FAILED(result)) { return result; }
    if (!*aTxn) { return NS_ERROR_NULL_POINTER; }
    (*aTxn)->SetName(InsertTextTxn::gInsertTextTxnName);
    mEditor->Do(*aTxn);
  }
  nsresult result = WillInsert(aSelection, aCancel);
  return result;
}

NS_IMETHODIMP
nsTextEditRules::DidInsertText(nsIDOMSelection *aSelection, 
                               const nsString& aStringToInsert, 
                               nsresult aResult)
{
  return DidInsert(aSelection, aResult);
}

NS_IMETHODIMP
nsTextEditRules::WillDeleteSelection(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  
  // if there is only bogus content, cancel the operation
  if (mBogusNode) {
    *aCancel = PR_TRUE;
    return NS_OK;
  }
  return NS_OK;
}

// if the document is empty, insert a bogus text node with a &nbsp;
// if we ended up with consecutive text nodes, merge them
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
  if (NS_SUCCEEDED(result)) // only do this work if DeleteSelection completed successfully
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
          // set mBogusNode to be the newly created <P>
          result = mEditor->CreateNode(nsAutoString("P"), bodyNode, 0, 
                                       getter_AddRefs(mBogusNode));
          if ((NS_SUCCEEDED(result)) && mBogusNode)
          {
            nsCOMPtr<nsIDOMNode>newTNode;
            result = mEditor->CreateNode(nsIEditor::GetTextNodeTag(), mBogusNode, 0, 
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
            newPElement = do_QueryInterface(mBogusNode);
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
    // if we don't have an empty document, check the selection to see if any collapsing is necessary
    if (!mBogusNode)
    {
      nsCOMPtr<nsIDOMNode>anchor;
      PRInt32 offset;
      nsresult result = aSelection->GetAnchorNodeAndOffset(getter_AddRefs(anchor), &offset);
      if ((NS_SUCCEEDED(result)) && anchor)
      {
        nsCOMPtr<nsIDOMNodeList> anchorChildren;
        result = anchor->GetChildNodes(getter_AddRefs(anchorChildren));
        nsCOMPtr<nsIDOMNode> selectedNode;
        if ((NS_SUCCEEDED(result)) && anchorChildren) {              
          result = anchorChildren->Item(offset, getter_AddRefs(selectedNode));
        }
        else {
          selectedNode = do_QueryInterface(anchor);
        }
        if ((NS_SUCCEEDED(result)) && selectedNode)
        {
          nsCOMPtr<nsIDOMCharacterData>selectedNodeAsText;
          selectedNodeAsText = do_QueryInterface(selectedNode);
          if (selectedNodeAsText)
          {
            nsCOMPtr<nsIDOMNode> siblingNode;
            selectedNode->GetPreviousSibling(getter_AddRefs(siblingNode));
            if (siblingNode)
            {
              nsCOMPtr<nsIDOMCharacterData>siblingNodeAsText;
              siblingNodeAsText = do_QueryInterface(siblingNode);
              if (siblingNodeAsText)
              {
                PRUint32 siblingLength; // the length of siblingNode before the join
                siblingNodeAsText->GetLength(&siblingLength);
                nsCOMPtr<nsIDOMNode> parentNode;
                selectedNode->GetParentNode(getter_AddRefs(parentNode));
                result = mEditor->JoinNodes(siblingNode, selectedNode, parentNode);
                // set selection to point between the end of siblingNode and start of selectedNode
                aSelection->Collapse(siblingNode, siblingLength);
                selectedNode = do_QueryInterface(siblingNode);  // subsequent code relies on selectedNode
                                                                // being the real node in the DOM tree
              }
            }
            selectedNode->GetNextSibling(getter_AddRefs(siblingNode));
            if (siblingNode)
            {
              nsCOMPtr<nsIDOMCharacterData>siblingNodeAsText;
              siblingNodeAsText = do_QueryInterface(siblingNode);
              if (siblingNodeAsText)
              {
                PRUint32 selectedNodeLength; // the length of siblingNode before the join
                selectedNodeAsText->GetLength(&selectedNodeLength);
                nsCOMPtr<nsIDOMNode> parentNode;
                selectedNode->GetParentNode(getter_AddRefs(parentNode));
                result = mEditor->JoinNodes(selectedNode, siblingNode, parentNode);
                // set selection
                aSelection->Collapse(selectedNode, selectedNodeLength);
              }
            }
          }
        }
      }
    }
  }
  return result;
}

NS_IMETHODIMP
nsTextEditRules::WillUndo(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  return NS_OK;
}

/* the idea here is to see if the magic empty node has suddenly reappeared as the result of the undo.
 * if it has, set our state so we remember it.
 * There is a tradeoff between doing here and at redo, or doing it everywhere else that might care.
 * Since undo and redo are relatively rare, it makes sense to take the (small) performance hit here.
 */
NS_IMETHODIMP
nsTextEditRules:: DidUndo(nsIDOMSelection *aSelection, nsresult aResult)
{
  nsresult result = aResult;  // if aResult is an error, we return it.
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  if (NS_SUCCEEDED(result)) 
  {
    if (mBogusNode) {
      mBogusNode = do_QueryInterface(nsnull);
    }
    else
    {
      nsCOMPtr<nsIDOMNode>node;
      PRInt32 offset;
      nsresult result = aSelection->GetAnchorNodeAndOffset(getter_AddRefs(node), &offset);
      while ((NS_SUCCEEDED(result)) && node)
      {
        nsCOMPtr<nsIDOMElement>element;
        element = do_QueryInterface(node);
        if (element)
        {
          nsAutoString att(kMOZEditorBogusNodeAttr);
          nsAutoString val;
          (void)element->GetAttribute(att, val);
          if (val.Equals(kMOZEditorBogusNodeValue)) {
            mBogusNode = do_QueryInterface(element);
          }
        }
        nsCOMPtr<nsIDOMNode> temp;
        result = node->GetParentNode(getter_AddRefs(temp));
        node = do_QueryInterface(temp);
      }
    }
  }
  return result;
}

NS_IMETHODIMP
nsTextEditRules::WillRedo(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsTextEditRules::DidRedo(nsIDOMSelection *aSelection, nsresult aResult)
{
  nsresult result = aResult;  // if aResult is an error, we return it.
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  if (NS_SUCCEEDED(result)) 
  {
    if (mBogusNode) {
      mBogusNode = do_QueryInterface(nsnull);
    }
    else
    {
      nsCOMPtr<nsIDOMNode>node;
      PRInt32 offset;
      nsresult result = aSelection->GetAnchorNodeAndOffset(getter_AddRefs(node), &offset);
      while ((NS_SUCCEEDED(result)) && node)
      {
        nsCOMPtr<nsIDOMElement>element;
        element = do_QueryInterface(node);
        if (element)
        {
          nsAutoString att(kMOZEditorBogusNodeAttr);
          nsAutoString val;
          (void)element->GetAttribute(att, val);
          if (val.Equals(kMOZEditorBogusNodeValue)) {
            mBogusNode = do_QueryInterface(element);
          }
        }
        nsCOMPtr<nsIDOMNode> temp;
        result = node->GetParentNode(getter_AddRefs(temp));
        node = do_QueryInterface(temp);
      }
    }
  }
  return result;
}






