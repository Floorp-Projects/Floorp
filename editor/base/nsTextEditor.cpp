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

nsresult nsTextEditor::InitTextEditor(nsIDOMDocument *aDoc, 
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
nsresult nsTextEditor::SetTextProperties(nsISupportsArray *aPropList)
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
                    if (startParent.get()==endParent.get()) {
                      result = SetTextPropertiesForNode(startParent, commonParent, 
                                                        startOffset, endOffset,
                                                        propName);
                      if (NS_SUCCEEDED(result))
                      { // set the selection
                        // don't want to actually do anything with selection, because
                        // I'm still iterating through it.  Just want to create and remember
                        // an nsIDOMRange, and later add the range to the selection after clearing it.
                      }
                    }
                  }
                }
              }
            }
          }
          NS_RELEASE(propAsSupports);
        }
      }
      mEditor->EndTransaction();
    }
  }
  return result;
}

nsresult nsTextEditor::GetTextProperties(nsISupportsArray *aPropList)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::RemoveTextProperties(nsISupportsArray *aPropList)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::DeleteSelection(nsIEditor::Direction aDir)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->DeleteSelection(aDir);
  }
  return result;
}

nsresult nsTextEditor::InsertText(const nsString& aStringToInsert)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->InsertText(aStringToInsert);
  }
  return result;
}

nsresult nsTextEditor::InsertBreak(PRBool aCtrlKey)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    PRBool beganTransaction = PR_FALSE;
    nsCOMPtr<nsIDOMSelection> selection;
    result = mEditor->GetSelection(getter_AddRefs(selection));
    if ((NS_SUCCEEDED(result)) && selection)
    {
      beganTransaction = PR_TRUE;
      result = mEditor->BeginTransaction();
      PRBool collapsed;
      result = selection->IsCollapsed(&collapsed);
      if (NS_SUCCEEDED(result) && !collapsed) 
      {
        result = mEditor->DeleteSelection(nsIEditor::eLTR);
        // get the new selection
        result = mEditor->GetSelection(getter_AddRefs(selection));
#ifdef NS_DEBUG
        PRBool testCollapsed;
        result = selection->IsCollapsed(&testCollapsed);
        NS_ASSERTION(PR_TRUE==testCollapsed, "selection not reset after deletion");
#endif
      }
      // split the text node
      nsCOMPtr<nsIDOMNode> node;
      PRInt32 offset;
      result = selection->GetAnchorNodeAndOffset(getter_AddRefs(node), &offset);
      if ((NS_SUCCEEDED(result)) && node)
      {
        nsCOMPtr<nsIDOMNode> parentNode;
        nsCOMPtr<nsIDOMNode> newNode;
        result = node->GetParentNode(getter_AddRefs(parentNode));
        if ((NS_SUCCEEDED(result)) && parentNode)
        {
          result = mEditor->SplitNode(node, offset, getter_AddRefs(newNode));
          if (NS_SUCCEEDED(result))
          { // now get the node's offset in it's parent, and insert the new BR there
            result = nsIEditorSupport::GetChildOffset(node, parentNode, offset);
            if (NS_SUCCEEDED(result))
            {
              nsAutoString tag("BR");
              result = mEditor->CreateNode(tag, parentNode, offset, getter_AddRefs(newNode));
              selection->Collapse(parentNode, offset);
            }
          }
        }
      }
    }
    if (PR_TRUE==beganTransaction) {
      result = mEditor->EndTransaction();
    }
  }
  return result;
}

nsresult nsTextEditor::EnableUndo(PRBool aEnable)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->EnableUndo(aEnable);
  }
  return result;
}

nsresult nsTextEditor::Undo(PRUint32 aCount)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor) {
    result = mEditor->Undo(aCount);
  }
  return result;
}

nsresult nsTextEditor::CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->CanUndo(aIsEnabled, aCanUndo);
  }
  return result;
}

nsresult nsTextEditor::Redo(PRUint32 aCount)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->Redo(aCount);
  }
  return result;
}

nsresult nsTextEditor::CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->CanRedo(aIsEnabled, aCanRedo);
  }
  return result;
}

nsresult nsTextEditor::BeginTransaction()
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->BeginTransaction();
  }
  return result;
}

nsresult nsTextEditor::EndTransaction()
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->EndTransaction();
  }
  return result;
}

nsresult nsTextEditor::MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection) 
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::ScrollUp(nsIAtom *aIncrement)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::ScrollDown(nsIAtom *aIncrement)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::ScrollIntoView(PRBool aScrollToBegin)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = mEditor->ScrollIntoView(aScrollToBegin);
  }
  return result;
}

nsresult nsTextEditor::Insert(nsIInputStream *aInputStream)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::OutputText(nsIOutputStream *aOutputStream)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  if (mEditor)
  {
    result = NS_ERROR_NOT_IMPLEMENTED;
  }
  return result;
}

nsresult nsTextEditor::OutputHTML(nsIOutputStream *aOutputStream)
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

nsresult
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
  return NS_NOINTERFACE;
}


nsresult nsTextEditor::SetTextPropertiesForNode(nsIDOMNode *aNode, 
                                                nsIDOMNode *aParent,
                                                PRInt32 aStartOffset,
                                                PRInt32 aEndOffset,
                                                nsIAtom *aPropName)
{
  nsresult result;
  nsCOMPtr<nsIDOMNode>newTextNode;  // this will be the middle text node
  result = mEditor->SplitNode(aNode, aStartOffset, getter_AddRefs(newTextNode));
  if (NS_SUCCEEDED(result))
  {
    result = mEditor->SplitNode(aNode, aEndOffset-aStartOffset, getter_AddRefs(newTextNode));
    if (NS_SUCCEEDED(result))
    {
      nsAutoString tag;
      if (nsIEditProperty::bold==aPropName) {
        tag = "b";
      }
      else if (nsIEditProperty::italic==aPropName) {
        tag = "i";
      }
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
  return result;
}