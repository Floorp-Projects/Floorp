/* -*- Mode: C++ tab-width: 2 indent-tabs-mode: nil c-basic-offset: 2 -*-
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

#include "nsTextEditor.h"
#include "nsIEditorSupport.h"
#include "nsEditorEventListeners.h"
#include "nsIEditProperty.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventReceiver.h" 
#include "nsIDOMKeyListener.h" 
#include "nsIDOMMouseListener.h"
#include "nsIDOMSelection.h"
#include "nsIDOMRange.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMCharacterData.h"
#include "nsEditorCID.h"
#include "nsISupportsArray.h"
#include "nsIEnumerator.h"

#include "CreateElementTxn.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"

static NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID,   NS_IDOMKEYLISTENER_IID);
static NS_DEFINE_IID(kIEditPropertyIID,     NS_IEDITPROPERTY_IID);

static NS_DEFINE_CID(kEditorCID,      NS_EDITOR_CID);
static NS_DEFINE_IID(kIEditorIID,     NS_IEDITOR_IID);
static NS_DEFINE_IID(kISupportsIID,   NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITextEditorIID, NS_ITEXTEDITOR_IID);
static NS_DEFINE_CID(kTextEditorCID,  NS_TEXTEDITOR_CID);


nsTextEditor::nsTextEditor()
{
  NS_INIT_REFCNT();
}

nsTextEditor::~nsTextEditor()
{
  //the autopointers will clear themselves up. 
  //but we need to also remove the listeners or we have a leak
  if (mEditor)
  {
    nsCOMPtr<nsIDOMDocument> doc;
    mEditor->GetDocument(getter_AddRefs(doc));
    if (doc)
    {
      nsCOMPtr<nsIDOMEventReceiver> erP;
      nsresult result = doc->QueryInterface(kIDOMEventReceiverIID, getter_AddRefs(erP));
      if (NS_SUCCEEDED(result) && erP) 
      {
        if (mKeyListenerP) {
          erP->RemoveEventListener(mKeyListenerP, kIDOMKeyListenerIID);
        }
        if (mMouseListenerP) {
          erP->RemoveEventListener(mMouseListenerP, kIDOMMouseListenerIID);
        }
      }
      else
        NS_NOTREACHED("~nsTextEditor");
    }
  }
}

NS_IMETHODIMP nsTextEditor::InitTextEditor(nsIDOMDocument *aDoc, 
                                      nsIPresShell   *aPresShell,
                                      nsIEditorCallback *aCallback)
{
  NS_PRECONDITION(nsnull!=aDoc && nsnull!=aPresShell, "bad arg");
  nsresult result=NS_ERROR_NULL_POINTER;
  if ((nsnull!=aDoc) && (nsnull!=aPresShell))
  {
    // get the editor
    nsIEditor *editor = nsnull;
    result = nsRepository::CreateInstance(kEditorCID, nsnull,
                                          kIEditorIID, (void **)&editor);
    if (NS_FAILED(result) || !editor) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mEditor = do_QueryInterface(editor); // CreateInstance did our addRef

    mEditor->Init(aDoc, aPresShell);
    mEditor->EnableUndo(PR_TRUE);

    result = NS_NewEditorKeyListener(getter_AddRefs(mKeyListenerP), this);
    if (NS_OK != result) {
      return result;
    }
    result = NS_NewEditorMouseListener(getter_AddRefs(mMouseListenerP), this);
    if (NS_OK != result) {
      // drop the key listener if we couldn't get a mouse listener.
      mKeyListenerP = do_QueryInterface(0); 
      return result;
    }
    nsCOMPtr<nsIDOMEventReceiver> erP;
    result = aDoc->QueryInterface(kIDOMEventReceiverIID, getter_AddRefs(erP));
    if (NS_OK != result) 
    {
      mKeyListenerP = do_QueryInterface(0);
      mMouseListenerP = do_QueryInterface(0); //dont need these if we cant register them
      return result;
    }
    erP->AddEventListener(mKeyListenerP, kIDOMKeyListenerIID);
    //erP->AddEventListener(mMouseListenerP, kIDOMMouseListenerIID);

    result = NS_OK;
  }
  return result;
}

// this is a total hack for now.  We don't yet have a way of getting the style properties
// of the current selection, so we can't do anything useful here except show off a little.
NS_IMETHODIMP nsTextEditor::SetTextProperties(nsISupportsArray *aPropList)
{
  if (!aPropList)
    return NS_ERROR_NULL_POINTER;

  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    nsCOMPtr<nsIDOMSelection>selection;
    result = mEditor->GetSelection(getter_AddRefs(selection));
    if ((NS_SUCCEEDED(result)) && selection)
    {
      mEditor->BeginTransaction();
      PRInt32 count = aPropList->Count();
      PRInt32 i=0;
      for ( ; i<count; i++)
      {
        nsISupports *propAsSupports;
        propAsSupports = aPropList->ElementAt(i);
        if (propAsSupports)
        {
          nsCOMPtr<nsIEditProperty> prop;
          result = propAsSupports->QueryInterface(kIEditPropertyIID, 
                                                  getter_AddRefs(prop));
          if ((NS_SUCCEEDED(result)) && prop)
          {
            nsCOMPtr<nsIAtom>propName;
            result = prop->GetProperty(getter_AddRefs(propName));
            if ((NS_SUCCEEDED(result)) && prop)
            {
              nsCOMPtr<nsIEnumerator> enumerator;
              enumerator = do_QueryInterface(selection, &result);
              if ((NS_SUCCEEDED(result)) && enumerator)
              {
                enumerator->First(); 
                nsISupports *currentItem;
                result = enumerator->CurrentItem(&currentItem);
                if ((NS_SUCCEEDED(result)) && (nsnull!=currentItem))
                {
                  nsCOMPtr<nsIDOMRange> range(currentItem);
                  nsCOMPtr<nsIDOMNode>commonParent;
                  result = range->GetCommonParent(getter_AddRefs(commonParent));
                  if ((NS_SUCCEEDED(result)) && commonParent)
                  {
                    PRInt32 startOffset, endOffset;
                    range->GetStartOffset(&startOffset);
                    range->GetEndOffset(&endOffset);
                    nsCOMPtr<nsIDOMNode> startParent;  nsCOMPtr<nsIDOMNode> endParent;
                    range->GetStartParent(getter_AddRefs(startParent));
                    range->GetEndParent(getter_AddRefs(endParent));
                    if (startParent.get()==endParent.get()) 
                    { // the range is entirely contained within a single text node
                      result = SetTextPropertiesForNode(startParent, commonParent, 
                                                        startOffset, endOffset,
                                                        propName);
                    }
                    else
                    {
                      nsCOMPtr<nsIDOMNode> startGrandParent;
                      startParent->GetParentNode(getter_AddRefs(startGrandParent));
                      if (NS_SUCCEEDED(result))
                      {
                        if (commonParent.get()==startGrandParent.get())
                        { // the range is between 2 nodes that have a common (immediate) grandparent
                          result = SetTextPropertiesForNodesWithSameParent(startParent,startOffset,
                                                                           endParent,  endOffset,
                                                                           commonParent,
                                                                           propName);
                        }
                        else
                        { // the range is between 2 nodes that have no simple relationship
                          result = SetTextPropertiesForNodeWithDifferentParents(startParent,startOffset, 
                                                                                endParent,  endOffset,
                                                                                commonParent,
                                                                                propName);
                        }
                      }
                    }
                    if (NS_SUCCEEDED(result))
                    { // compute a range for the selection
                      // don't want to actually do anything with selection, because
                      // we are still iterating through it.  Just want to create and remember
                      // an nsIDOMRange, and later add the range to the selection after clearing it.
                      // XXX: I'm blocked here because nsIDOMSelection doesn't provide a mechanism
                      //      for setting a compound selection yet.
                    }
                  }
                }
              }
            }
          }
          NS_RELEASE(propAsSupports);
        }
      } // end for loop
      mEditor->EndTransaction();
      if (NS_SUCCEEDED(result))
      { // set the selection
        // XXX: can't do anything until I can create ranges
      }
    }
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::GetTextProperties(nsISupportsArray *aPropList)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::RemoveTextProperties(nsISupportsArray *aPropList)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::DeleteSelection(nsIEditor::Direction aDir)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->DeleteSelection(aDir);
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::InsertText(const nsString& aStringToInsert)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->InsertText(aStringToInsert);
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::InsertBreak(PRBool aCtrlKey)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    // Note: Code that does most of the deletion work was 
    // moved to nsEditor::DeleteSelectionAndCreateNode
    // Only difference is we now do BeginTransaction()/EndTransaction()
    //  even if we fail to get a selection
    result = mEditor->BeginTransaction();

    nsCOMPtr<nsIDOMNode> newNode;
    nsAutoString tag("BR");
    mEditor->DeleteSelectionAndCreateNode(tag, getter_AddRefs(newNode));
    // Are we supposed to release newNode?

    result = mEditor->EndTransaction();
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::EnableUndo(PRBool aEnable)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->EnableUndo(aEnable);
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::Undo(PRUint32 aCount)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor) {
    result = mEditor->Undo(aCount);
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->CanUndo(aIsEnabled, aCanUndo);
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::Redo(PRUint32 aCount)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->Redo(aCount);
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->CanRedo(aIsEnabled, aCanRedo);
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::BeginTransaction()
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->BeginTransaction();
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::EndTransaction()
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->EndTransaction();
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection) 
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::ScrollUp(nsIAtom *aIncrement)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::ScrollDown(nsIAtom *aIncrement)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::ScrollIntoView(PRBool aScrollToBegin)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->ScrollIntoView(aScrollToBegin);
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::Insert(nsIInputStream *aInputStream)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::OutputText(nsIOutputStream *aOutputStream)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::OutputHTML(nsIOutputStream *aOutputStream)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}


NS_IMPL_ADDREF(nsTextEditor)

NS_IMPL_RELEASE(nsTextEditor)

NS_IMETHODIMP
nsTextEditor::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kITextEditorIID)) {
    *aInstancePtr = (void*)(nsITextEditor*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIEditorIID) && (mEditor)) {
    nsCOMPtr<nsIEditor> editor;
    nsresult result = mEditor->QueryInterface(kIEditorIID, getter_AddRefs(editor));
    if (NS_SUCCEEDED(result) && editor) {
      *aInstancePtr = (void*)editor;
      return NS_OK;
    }
  }
  return NS_NOINTERFACE;
}


NS_IMETHODIMP nsTextEditor::SetTextPropertiesForNode(nsIDOMNode *aNode, 
                                                nsIDOMNode *aParent,
                                                PRInt32 aStartOffset,
                                                PRInt32 aEndOffset,
                                                nsIAtom *aPropName)
{
  nsresult result=NS_OK;
  nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
  nodeAsChar =  aNode;
  if (!nodeAsChar)
    return NS_ERROR_FAILURE;
  PRUint32 count;
  nodeAsChar->GetLength(&count);

  nsCOMPtr<nsIDOMNode>newTextNode;  // this will be the text node we move into the new style node
  if (aStartOffset!=0)
  {
    result = mEditor->SplitNode(aNode, aStartOffset, getter_AddRefs(newTextNode));
  }
  if (NS_SUCCEEDED(result))
  {
    if (aEndOffset!=count)
    {
      result = mEditor->SplitNode(aNode, aEndOffset-aStartOffset, getter_AddRefs(newTextNode));
    }
    else
    {
      newTextNode = do_QueryInterface(aNode);
    }
    if (NS_SUCCEEDED(result))
    {
      nsAutoString tag;
      result = SetTagFromProperty(tag, aPropName);
      if (NS_SUCCEEDED(result))
      {
        PRInt32 offsetInParent;
        result = nsIEditorSupport::GetChildOffset(aNode, aParent, offsetInParent);
        if (NS_SUCCEEDED(result))
        {
          nsCOMPtr<nsIDOMNode>newStyleNode;
          result = mEditor->CreateNode(tag, aParent, offsetInParent, getter_AddRefs(newStyleNode));
          if (NS_SUCCEEDED(result))
          {
            result = mEditor->DeleteNode(newTextNode);
            if (NS_SUCCEEDED(result)) {
              result = mEditor->InsertNode(newTextNode, newStyleNode, 0);
            }
          }
        }
      }
    }
  }
  return result;
}

NS_IMETHODIMP 
nsTextEditor::SetTextPropertiesForNodesWithSameParent(nsIDOMNode *aStartNode,
                                                      PRInt32     aStartOffset,
                                                      nsIDOMNode *aEndNode,
                                                      PRInt32     aEndOffset,
                                                      nsIDOMNode *aParent,
                                                      nsIAtom    *aPropName)
{
  nsresult result=NS_OK;
  nsCOMPtr<nsIDOMNode>newLeftTextNode;  // this will be the middle text node
  if (0!=aStartOffset) {
    result = mEditor->SplitNode(aStartNode, aStartOffset, getter_AddRefs(newLeftTextNode));
  }
  if (NS_SUCCEEDED(result))
  {
    nsCOMPtr<nsIDOMCharacterData>endNodeAsChar;
    endNodeAsChar = aEndNode;
    if (!endNodeAsChar)
      return NS_ERROR_FAILURE;
    PRUint32 count;
    endNodeAsChar->GetLength(&count);
    nsCOMPtr<nsIDOMNode>newRightTextNode;  // this will be the middle text node
    if (count!=aEndOffset) {
      result = mEditor->SplitNode(aEndNode, aEndOffset, getter_AddRefs(newRightTextNode));
    }
    else {
      newRightTextNode = do_QueryInterface(aEndNode);
    }
    if (NS_SUCCEEDED(result))
    {
      PRInt32 offsetInParent;
      if (newLeftTextNode) {
        result = nsIEditorSupport::GetChildOffset(newLeftTextNode, aParent, offsetInParent);
      }
      else {
        offsetInParent = -1; // relies on +1 below in call to CreateNode
      }
      if (NS_SUCCEEDED(result))
      {
        nsAutoString tag;
        result = SetTagFromProperty(tag, aPropName);
        if (NS_SUCCEEDED(result))
        { // create the new style node, which will be the new parent for the selected nodes
          nsCOMPtr<nsIDOMNode>newStyleNode;
          result = mEditor->CreateNode(tag, aParent, offsetInParent+1, getter_AddRefs(newStyleNode));
          if (NS_SUCCEEDED(result))
          { // move the right half of the start node into the new style node
            nsCOMPtr<nsIDOMNode>intermediateNode;
            result = aStartNode->GetNextSibling(getter_AddRefs(intermediateNode));
            if (NS_SUCCEEDED(result))
            {
              result = mEditor->DeleteNode(aStartNode);
              if (NS_SUCCEEDED(result)) 
              { 
                PRInt32 childIndex=0;
                result = mEditor->InsertNode(aStartNode, newStyleNode, childIndex);
                childIndex++;
                if (NS_SUCCEEDED(result))
                { // move all the intermediate nodes into the new style node
                  nsCOMPtr<nsIDOMNode>nextSibling;
                  while (intermediateNode.get() != aEndNode)
                  {
                    if (!intermediateNode)
                      result = NS_ERROR_NULL_POINTER;
                    if (NS_FAILED(result)) {
                      break;
                    }
                    // get the next sibling before moving the current child!!!
                    intermediateNode->GetNextSibling(getter_AddRefs(nextSibling));
                    result = mEditor->DeleteNode(intermediateNode);
                    if (NS_SUCCEEDED(result)) {
                      result = mEditor->InsertNode(intermediateNode, newStyleNode, childIndex);
                      childIndex++;
                    }
                    intermediateNode = do_QueryInterface(nextSibling);
                  }
                  if (NS_SUCCEEDED(result))
                  { // move the left half of the end node into the new style node
                    result = mEditor->DeleteNode(newRightTextNode);
                    if (NS_SUCCEEDED(result)) 
                    {
                      result = mEditor->InsertNode(newRightTextNode, newStyleNode, childIndex);
                    }
                  }
                }
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
nsTextEditor::SetTextPropertiesForNodeWithDifferentParents(nsIDOMNode *aStartNode,
                                                           PRInt32     aStartOffset,
                                                           nsIDOMNode *aEndNode,
                                                           PRInt32     aEndOffset,
                                                           nsIDOMNode *aParent,
                                                           nsIAtom    *aPropName)
{
  nsresult result = NS_OK;
  return result;
}



NS_IMETHODIMP 
nsTextEditor::SetTagFromProperty(nsAutoString &aTag, nsIAtom *aPropName) const
{
  if (!aPropName) {
    return NS_ERROR_NULL_POINTER;
  }
  if (nsIEditProperty::bold==aPropName) {
    aTag = "b";
  }
  else if (nsIEditProperty::italic==aPropName) {
    aTag = "i";
  }
  else {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

