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

#include "nsTextEditor.h"
#include "nsEditorEventListeners.h"

#include "nsIDocument.h"
#include "nsFileSpec.h"

#include "nsIDOMDocument.h"
#include "nsIDOMEventReceiver.h" 
#include "nsIDOMKeyListener.h" 
#include "nsIDOMMouseListener.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMSelection.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMElement.h"
#include "nsIDOMTextListener.h"
#include "nsIDiskDocument.h"
#include "nsIDocumentEncoder.h"
#include "nsEditorCID.h"
#include "nsISupportsArray.h"
#include "nsIEnumerator.h"
#include "nsIContentIterator.h"
#include "nsIContent.h"
#include "nsLayoutCID.h"
#include "nsIPresShell.h"
#include "nsIStyleContext.h"
#include "nsVoidArray.h"

#if DEBUG
#include "TextEditorTest.h"
#endif

// transactions the text editor knows how to build itself
#include "TransactionFactory.h"
#include "PlaceholderTxn.h"
#include "InsertTextTxn.h"

#include "nsIFileStream.h"
#include "nsIStringStream.h"

#include "nsIAppShell.h"
#include "nsIToolkit.h"
#include "nsWidgetsCID.h"
#include "nsIFileWidget.h"

// Drag & Drop, Clipboard
#include "nsIClipboard.h"
#include "nsITransferable.h"
//#include "nsIFormatConverter.h"

// Drag & Drop, Clipboard Support
static NS_DEFINE_CID(kCClipboardCID,           NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCTransferableCID,        NS_TRANSFERABLE_CID);
//static NS_DEFINE_IID(kCXIFFormatConverterCID,  NS_XIFFORMATCONVERTER_CID);

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsTextEditRules.h"

#include "nsIPref.h"
#include "nsAOLCiter.h"
#include "nsInternetCiter.h"

#ifdef ENABLE_JS_EDITOR_LOG
#include "nsJSEditorLog.h"
#endif // ENABLE_JS_EDITOR_LOG

static NS_DEFINE_CID(kEditorCID,        NS_EDITOR_CID);
static NS_DEFINE_CID(kTextEditorCID,    NS_TEXTEDITOR_CID);
static NS_DEFINE_CID(kCContentIteratorCID,   NS_CONTENTITERATOR_CID);
static NS_DEFINE_CID(kCRangeCID,    NS_RANGE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);


#ifdef NS_DEBUG
static PRBool gNoisy = PR_FALSE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif

/* ---------- TypeInState implementation ---------- */
// most methods are defined inline in TypeInState.h

NS_IMPL_ADDREF(TypeInState)

NS_IMPL_RELEASE(TypeInState)

NS_IMETHODIMP
TypeInState::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMSelectionListener::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMSelectionListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

TypeInState::~TypeInState()
{
};

NS_IMETHODIMP TypeInState::NotifySelectionChanged()
{ 
  Reset(); 
  return NS_OK;
};


/*****************************************************************************
 * nsTextEditor implementation
 ****************************************************************************/

nsTextEditor::nsTextEditor()
:  mTypeInState(nsnull)
,  mRules(nsnull)
,  mKeyListenerP(nsnull)
,  mIsComposing(PR_FALSE)
{
// Done in nsEditor
//  NS_INIT_REFCNT();
  mRules = nsnull;
  mMaxTextLength = -1;
  mWrapColumn = 72;
}

nsTextEditor::~nsTextEditor()
{
  //the autopointers will clear themselves up. 
  //but we need to also remove the listeners or we have a leak
  nsCOMPtr<nsIDOMSelection>selection;
  nsresult result = nsEditor::GetSelection(getter_AddRefs(selection));
  if (NS_SUCCEEDED(result) && selection) 
  {
    nsCOMPtr<nsIDOMSelectionListener>listener;
    listener = do_QueryInterface(mTypeInState);
    if (listener) {
      selection->RemoveSelectionListener(listener); 
    }
  }
  nsCOMPtr<nsIDOMDocument> doc;
  nsEditor::GetDocument(getter_AddRefs(doc));
  if (doc)
  {
    nsCOMPtr<nsIDOMEventReceiver> erP = do_QueryInterface(doc, &result);
    if (NS_SUCCEEDED(result) && erP) 
    {
      if (mKeyListenerP) {
        erP->RemoveEventListenerByIID(mKeyListenerP, nsIDOMKeyListener::GetIID());
      }
      if (mMouseListenerP) {
        erP->RemoveEventListenerByIID(mMouseListenerP, nsIDOMMouseListener::GetIID());
      }
	    if (mTextListenerP) {
        erP->RemoveEventListenerByIID(mTextListenerP, nsIDOMTextListener::GetIID());
	    }
 	    if (mCompositionListenerP) {
		    erP->RemoveEventListenerByIID(mCompositionListenerP, nsIDOMCompositionListener::GetIID());
	    }
      if (mFocusListenerP) {
        erP->RemoveEventListenerByIID(mFocusListenerP, nsIDOMFocusListener::GetIID());
      }
	    if (mDragListenerP) {
          erP->RemoveEventListenerByIID(mDragListenerP, nsIDOMDragListener::GetIID());
      }
    }
    else
      NS_NOTREACHED("~nsTextEditor");
  }

  // deleting a null pointer is safe
  delete mRules;

  NS_IF_RELEASE(mTypeInState);
}

// Adds appropriate AddRef, Release, and QueryInterface methods for derived class
//NS_IMPL_ISUPPORTS_INHERITED(nsTextEditor, nsEditor, nsITextEditor)

//NS_IMPL_ADDREF_INHERITED(Class, Super)
NS_IMETHODIMP_(nsrefcnt) nsTextEditor::AddRef(void)
{
  return nsEditor::AddRef();
}

//NS_IMPL_RELEASE_INHERITED(Class, Super)
NS_IMETHODIMP_(nsrefcnt) nsTextEditor::Release(void)
{
  return nsEditor::Release();
}

//NS_IMPL_QUERY_INTERFACE_INHERITED(Class, Super, AdditionalInterface)
NS_IMETHODIMP nsTextEditor::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
 
  if (aIID.Equals(nsITextEditor::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsITextEditor*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return nsEditor::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP nsTextEditor::Init(nsIDOMDocument *aDoc, 
                                 nsIPresShell   *aPresShell)
{
  NS_PRECONDITION(nsnull!=aDoc && nsnull!=aPresShell, "bad arg");
  nsresult result=NS_ERROR_NULL_POINTER;
  if ((nsnull!=aDoc) && (nsnull!=aPresShell))
  {
    // Init the base editor
    result = nsEditor::Init(aDoc, aPresShell);
    if (NS_OK != result) { return result; }

    // init the type-in state
    mTypeInState = new TypeInState();
    if (!mTypeInState) {return NS_ERROR_NULL_POINTER;}
    NS_ADDREF(mTypeInState);

    nsCOMPtr<nsIDOMSelection>selection;
    result = nsEditor::GetSelection(getter_AddRefs(selection));
    if (NS_OK != result) { return result; }
    if (selection) 
    {
      nsCOMPtr<nsIDOMSelectionListener>listener;
      listener = do_QueryInterface(mTypeInState);
      if (listener) {
        selection->AddSelectionListener(listener); 
      }
    }

    // Init the rules system
    InitRules();

    // get a key listener
    result = NS_NewEditorKeyListener(getter_AddRefs(mKeyListenerP), this);
    if (NS_OK != result) {
      HandleEventListenerError();
      return result;
    }
    
    // get a mouse listener
    result = NS_NewEditorMouseListener(getter_AddRefs(mMouseListenerP), this);
    if (NS_OK != result) {
      HandleEventListenerError();
      return result;
    }

    // get a text listener
	  result = NS_NewEditorTextListener(getter_AddRefs(mTextListenerP),this);
	  if (NS_OK !=result) 
    {
#ifdef DEBUG_TAGUE
	printf("nsTextEditor.cpp: failed to get TextEvent Listener\n");
#endif
		  HandleEventListenerError();
		  return result;
	  }

    // get a composition listener
	  result = NS_NewEditorCompositionListener(getter_AddRefs(mCompositionListenerP),this);
	  if (NS_OK!=result) 
    {
#ifdef DEBUG_TAGUE
	printf("nsTextEditor.cpp: failed to get TextEvent Listener\n");
#endif
		  HandleEventListenerError();
		  return result;
	  }

    // get a drag listener
    result = NS_NewEditorDragListener(getter_AddRefs(mDragListenerP), this);
    if (NS_OK != result) 
    {
      HandleEventListenerError();
      //return result;    // XXX: why is this return commented out?
    }

    // get a focus listener
    result = NS_NewEditorFocusListener(getter_AddRefs(mFocusListenerP), this);
    if (NS_OK != result) 
    {
      HandleEventListenerError();
      return result;
    }

    // get the DOM event receiver
    nsCOMPtr<nsIDOMEventReceiver> erP;
    result = aDoc->QueryInterface(nsIDOMEventReceiver::GetIID(), getter_AddRefs(erP));
    if (!NS_SUCCEEDED(result))
    {
      HandleEventListenerError();
      return result;
    }

    // register the event listeners with the DOM event reveiver
    result = erP->AddEventListenerByIID(mKeyListenerP, nsIDOMKeyListener::GetIID());
    NS_ASSERTION(NS_SUCCEEDED(result), "failed to register key listener");
    result = erP->AddEventListenerByIID(mMouseListenerP, nsIDOMMouseListener::GetIID());
    NS_ASSERTION(NS_SUCCEEDED(result), "failed to register mouse listener");
    result = erP->AddEventListenerByIID(mFocusListenerP, nsIDOMFocusListener::GetIID());
    NS_ASSERTION(NS_SUCCEEDED(result), "failed to register focus listener");
    result = erP->AddEventListenerByIID(mTextListenerP, nsIDOMTextListener::GetIID());
    NS_ASSERTION(NS_SUCCEEDED(result), "failed to register text listener");
    result = erP->AddEventListenerByIID(mCompositionListenerP, nsIDOMCompositionListener::GetIID());
    NS_ASSERTION(NS_SUCCEEDED(result), "failed to register composition listener");
    result = erP->AddEventListenerByIID(mDragListenerP, nsIDOMDragListener::GetIID());
    NS_ASSERTION(NS_SUCCEEDED(result), "failed to register drag listener");

    result = NS_OK;
		EnableUndo(PR_TRUE);
  }
  return result;
}

void nsTextEditor::HandleEventListenerError()
{
  printf("failed to add event listener\n");
  mKeyListenerP = do_QueryInterface(0);
  mMouseListenerP = do_QueryInterface(0);
  mTextListenerP = do_QueryInterface(0);
  mDragListenerP = do_QueryInterface(0); 
  mCompositionListenerP = do_QueryInterface(0);
  mFocusListenerP = do_QueryInterface(0);
}

void nsTextEditor::InitRules()
{
// instantiate the rules for this text editor
  mRules =  new nsTextEditRules();
  mRules->Init(this);
}

NS_IMETHODIMP
nsTextEditor::GetFlags(PRUint32 *aFlags)
{
  if (!mRules || !aFlags) { return NS_ERROR_NULL_POINTER; }
  return mRules->GetFlags(aFlags);
}

NS_IMETHODIMP
nsTextEditor::SetFlags(PRUint32 aFlags)
{
  if (!mRules) { return NS_ERROR_NULL_POINTER; }
  return mRules->SetFlags(aFlags);
}

NS_IMETHODIMP nsTextEditor::SetTextProperty(nsIAtom        *aProperty, 
                                            const nsString *aAttribute, 
                                            const nsString *aValue)
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->SetTextProperty(aProperty, aAttribute, aValue);
#endif // ENABLE_JS_EDITOR_LOG

  if (!aProperty) { return NS_ERROR_NULL_POINTER; }
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  if (gNoisy) 
  { 
    nsAutoString propString;
    aProperty->ToString(propString);
    char *propCString = propString.ToNewCString();
    if (gNoisy) { printf("---------- start nsTextEditor::SetTextProperty %s ----------\n", propCString); }
    delete [] propCString;
  }

  nsresult result=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMSelection>selection;
  result = nsEditor::GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) return result;
  if (!selection) return NS_ERROR_NULL_POINTER;

  PRBool cancel;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kSetTextProperty);
  result = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if ((PR_FALSE==cancel) && (NS_SUCCEEDED(result)))
  {
    PRBool isCollapsed;
    selection->GetIsCollapsed(&isCollapsed);
    if (PR_TRUE==isCollapsed)
    {
      // manipulating text attributes on a collapsed selection only sets state for the next text insertion
      SetTypeInStateForProperty(*mTypeInState, aProperty, aAttribute, aValue);
    }
    else
    {
      // set the text property for all selected ranges
      nsEditor::BeginTransaction();
      nsCOMPtr<nsIEnumerator> enumerator;
      result = selection->GetEnumerator(getter_AddRefs(enumerator));
      if (NS_FAILED(result)) return result;
      if (!enumerator) return NS_ERROR_NULL_POINTER;
      enumerator->First(); 
      
      nsCOMPtr<nsISupports> currentItem;
      result = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if (NS_FAILED(result)) return result;
      if (!currentItem) return NS_ERROR_NULL_POINTER;

      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
      nsCOMPtr<nsIDOMNode>commonParent;
      result = range->GetCommonParent(getter_AddRefs(commonParent));
      if (NS_FAILED(result)) return result;
      if (!commonParent) return NS_ERROR_NULL_POINTER;

      PRInt32 startOffset, endOffset;
      range->GetStartOffset(&startOffset);
      range->GetEndOffset(&endOffset);
      nsCOMPtr<nsIDOMNode> startParent;  nsCOMPtr<nsIDOMNode> endParent;
      range->GetStartParent(getter_AddRefs(startParent));
      range->GetEndParent(getter_AddRefs(endParent));
      PRBool startIsText = IsTextNode(startParent);
      PRBool endIsText = IsTextNode(endParent);
      if ((PR_TRUE==startIsText) && (PR_TRUE==endIsText) &&
          (startParent.get()==endParent.get())) 
      { // the range is entirely contained within a single text node
        // commonParent==aStartParent, so get the "real" parent of the selection
        startParent->GetParentNode(getter_AddRefs(commonParent));
        result = SetTextPropertiesForNode(startParent, commonParent, 
                                          startOffset, endOffset,
                                          aProperty, aAttribute, aValue);
      }
      else
      {
        nsCOMPtr<nsIDOMNode> startGrandParent;
        startParent->GetParentNode(getter_AddRefs(startGrandParent));
        nsCOMPtr<nsIDOMNode> endGrandParent;
        endParent->GetParentNode(getter_AddRefs(endGrandParent));
        if (NS_SUCCEEDED(result))
        {
          PRBool canCollapseStyleNode = PR_FALSE;
          if ((PR_TRUE==startIsText) && (PR_TRUE==endIsText) &&
              endGrandParent.get()==startGrandParent.get())
          {
            result = IntermediateNodesAreInline(range, startParent, startOffset, 
                                                endParent, endOffset, 
                                                canCollapseStyleNode);
          }
          if (NS_SUCCEEDED(result)) 
          {
            if (PR_TRUE==canCollapseStyleNode)
            { // the range is between 2 nodes that have a common (immediate) grandparent,
              // and any intermediate nodes are just inline style nodes
              result = SetTextPropertiesForNodesWithSameParent(startParent,startOffset,
                                                               endParent,  endOffset,
                                                               commonParent,
                                                               aProperty, aAttribute, aValue);
            }
            else
            { // the range is between 2 nodes that have no simple relationship
              result = SetTextPropertiesForNodeWithDifferentParents(range,
                                                                    startParent,startOffset, 
                                                                    endParent,  endOffset,
                                                                    commonParent,
                                                                    aProperty, aAttribute, aValue);
            }
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
      nsEditor::EndTransaction();
    }
    // post-process
    result = mRules->DidDoAction(selection, &ruleInfo, result);
  }
  if (gNoisy) {DebugDumpContent(); } // DEBUG
  if (gNoisy) 
  { 
    nsAutoString propString;
    aProperty->ToString(propString);
    char *propCString = propString.ToNewCString();
    if (gNoisy) { printf("---------- end nsTextEditor::SetTextProperty %s ----------\n", propCString); }
    delete [] propCString;
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::GetTextProperty(nsIAtom *aProperty, 
                                            const nsString *aAttribute, 
                                            const nsString *aValue,
                                            PRBool &aFirst, PRBool &aAny, PRBool &aAll)
{
  if (!aProperty)
    return NS_ERROR_NULL_POINTER;

  nsresult result=NS_ERROR_NOT_INITIALIZED;
  aAny=PR_FALSE;
  aAll=PR_TRUE;
  aFirst=PR_FALSE;
  PRBool first=PR_TRUE;
  nsCOMPtr<nsIDOMSelection>selection;
  result = nsEditor::GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) return result;
  if (!selection) return NS_ERROR_NULL_POINTER;

  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);
  nsCOMPtr<nsIEnumerator> enumerator;
  result = selection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(result)) return result;
  if (!enumerator) return NS_ERROR_NULL_POINTER;
  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  result = enumerator->CurrentItem(getter_AddRefs(currentItem));
  if (NS_FAILED(result)) return result;
  if (!currentItem) return NS_ERROR_NULL_POINTER;
  // XXX: should be a while loop, to get each separate range
  if (currentItem)
  {
    PRBool firstNodeInRange = PR_TRUE; // for each range, set a flag 
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    nsCOMPtr<nsIContentIterator> iter;
    result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                                nsIContentIterator::GetIID(), 
                                                getter_AddRefs(iter));
    if (NS_FAILED(result)) return result;
    if (!iter) return NS_ERROR_NULL_POINTER;
    iter->Init(range);
    nsCOMPtr<nsIContent> content;
    result = iter->CurrentNode(getter_AddRefs(content));
    while (NS_COMFALSE == iter->IsDone())
    {
      //if (gNoisy) { printf("  checking node %p\n", content.get()); }
      nsCOMPtr<nsIDOMCharacterData>text;
      text = do_QueryInterface(content);
      PRBool skipNode = PR_FALSE;
      if (text)
      {
        if (PR_FALSE==isCollapsed && PR_TRUE==first && PR_TRUE==firstNodeInRange)
        {
          firstNodeInRange = PR_FALSE;
          PRInt32 startOffset;
          range->GetStartOffset(&startOffset);
          PRUint32 count;
          text->GetLength(&count);
          if (startOffset==(PRInt32)count) 
          {
            //if (gNoisy) { printf("  skipping node %p\n", content.get()); }
            skipNode = PR_TRUE;
          }
        }
      }
      else
      { // handle non-text leaf nodes here
        PRBool canContainChildren;
        content->CanContainChildren(canContainChildren);
        if (PR_TRUE==canContainChildren)
        {
          //if (gNoisy) { printf("  skipping non-leaf node %p\n", content.get()); }
          skipNode = PR_TRUE;
        }
        else {
          //if (gNoisy) { printf("  testing non-text leaf node %p\n", content.get()); }
        }
      }
      if (PR_FALSE==skipNode)
      {
        nsCOMPtr<nsIDOMNode>node;
        node = do_QueryInterface(content);
        if (node)
        {
          PRBool isSet;
          nsCOMPtr<nsIDOMNode>resultNode;
          IsTextPropertySetByContent(node, aProperty, aAttribute, aValue, isSet, getter_AddRefs(resultNode));
          if (PR_TRUE==first)
          {
            aFirst = isSet;
            first = PR_FALSE;
          }
          if (PR_TRUE==isSet) {
            aAny = PR_TRUE;
          }
          else {
            aAll = PR_FALSE;
          }
        }
      }
      iter->Next();
      iter->CurrentNode(getter_AddRefs(content));
    }
  }
  if (PR_FALSE==aAny) { // make sure that if none of the selection is set, we don't report all is set
    aAll = PR_FALSE;
  }
  //if (gNoisy) { printf("  returning first=%d any=%d all=%d\n", aFirst, aAny, aAll); }
  return result;
}

void nsTextEditor::IsTextStyleSet(nsIStyleContext *aSC, 
                                  nsIAtom *aProperty, 
                                  const nsString *aAttribute,  
                                  PRBool &aIsSet) const
{
  aIsSet = PR_FALSE;
  if (aSC && aProperty)
  {
    nsStyleFont* font = (nsStyleFont*)aSC->GetStyleData(eStyleStruct_Font);
    if (nsIEditProperty::i==aProperty)
    {
      aIsSet = PRBool(font->mFont.style & NS_FONT_STYLE_ITALIC);
    }
    else if (nsIEditProperty::b==aProperty)
    { // XXX: check this logic with Peter
      aIsSet = PRBool(font->mFont.weight > NS_FONT_WEIGHT_NORMAL);
    }
  }
}

// this will NOT find aAttribute unless aAttribute has a non-null value
// so singleton attributes like <Table border> will not be matched!
void nsTextEditor::IsTextPropertySetByContent(nsIDOMNode     *aNode,
                                              nsIAtom        *aProperty, 
                                              const nsString *aAttribute, 
                                              const nsString *aValue, 
                                              PRBool         &aIsSet,
                                              nsIDOMNode    **aStyleNode) const
{
  nsresult result;
  aIsSet = PR_FALSE;  // must be initialized to false for code below to work
  nsAutoString propName;
  aProperty->ToString(propName);
  nsCOMPtr<nsIDOMNode>parent;
  result = aNode->GetParentNode(getter_AddRefs(parent));
  while (NS_SUCCEEDED(result) && parent)
  {
    nsCOMPtr<nsIDOMElement>element;
    element = do_QueryInterface(parent);
    if (element)
    {
      nsString tag;
      element->GetTagName(tag);
      if (propName.EqualsIgnoreCase(tag))
      {
        PRBool found = PR_FALSE;
        if (aAttribute && 0!=aAttribute->Length())
        {
          nsAutoString value;
          element->GetAttribute(*aAttribute, value);
          if (0!=value.Length())
          {
            if (!aValue) {
              found = PR_TRUE;
            }
            else if (aValue->EqualsIgnoreCase(value)) {
              found = PR_TRUE;
            }
            else {  // we found the prop with the attribute, but the value doesn't match
              break;
            }
          }
        }
        else { 
          found = PR_TRUE;
        }
        if (PR_TRUE==found)
        {
          aIsSet = PR_TRUE;
          break;
        }
      }
    }
    nsCOMPtr<nsIDOMNode>temp;
    result = parent->GetParentNode(getter_AddRefs(temp));
    if (NS_SUCCEEDED(result) && temp) {
      parent = do_QueryInterface(temp);
    }
    else {
      parent = do_QueryInterface(nsnull);
    }
  }
}


NS_IMETHODIMP nsTextEditor::RemoveTextProperty(nsIAtom *aProperty, const nsString *aAttribute)
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->RemoveTextProperty(aProperty, aAttribute);
#endif // ENABLE_JS_EDITOR_LOG

  if (!aProperty) { return NS_ERROR_NULL_POINTER; }
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  if (gNoisy) 
  { 
    nsAutoString propString;
    aProperty->ToString(propString);
    char *propCString = propString.ToNewCString();
    if (gNoisy) { printf("---------- start nsTextEditor::RemoveTextProperty %s ----------\n", propCString); }
    delete [] propCString;
  }

  nsresult result;
  nsCOMPtr<nsIDOMSelection>selection;
  result = nsEditor::GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) return result;
  if (!selection) return NS_ERROR_NULL_POINTER;

  PRBool cancel;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kRemoveTextProperty);
  result = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if ((PR_FALSE==cancel) && (NS_SUCCEEDED(result)))
  {
    PRBool isCollapsed;
    selection->GetIsCollapsed(&isCollapsed);
    if (PR_TRUE==isCollapsed)
    {
      // manipulating text attributes on a collapsed selection only sets state for the next text insertion
      SetTypeInStateForProperty(*mTypeInState, aProperty, aAttribute, nsnull);
    }
    else
    {
      // removing text properties can really shuffle text nodes around
      // so we need to keep some extra state to restore a reasonable selection
      // after we're done
      nsCOMPtr<nsIDOMNode> parentForSelection;  // selection's block parent
      PRInt32 rangeStartOffset, rangeEndOffset;
      GetTextSelectionOffsetsForRange(selection, getter_AddRefs(parentForSelection), 
                                      rangeStartOffset, rangeEndOffset);
      nsEditor::BeginTransaction();
      nsCOMPtr<nsIDOMNode> startParent, endParent;
      PRInt32 startOffset, endOffset;
      nsCOMPtr<nsIEnumerator> enumerator;
      result = selection->GetEnumerator(getter_AddRefs(enumerator));
      if (NS_FAILED(result)) return result;
      if (!enumerator) return NS_ERROR_NULL_POINTER;
      enumerator->First(); 
      nsCOMPtr<nsISupports>currentItem;
      result = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if (NS_FAILED(result)) return result;
      if (!currentItem) return NS_ERROR_NULL_POINTER;
      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
      nsCOMPtr<nsIDOMNode>commonParent;
      result = range->GetCommonParent(getter_AddRefs(commonParent));
      if (NS_FAILED(result)) return result;
      if (!commonParent) return NS_ERROR_NULL_POINTER;
      range->GetStartOffset(&startOffset);
      range->GetEndOffset(&endOffset);
      range->GetStartParent(getter_AddRefs(startParent));
      range->GetEndParent(getter_AddRefs(endParent));
      if (startParent.get()==endParent.get()) 
      { // the range is entirely contained within a single text node
        // commonParent==aStartParent, so get the "real" parent of the selection
        startParent->GetParentNode(getter_AddRefs(commonParent));
        result = RemoveTextPropertiesForNode(startParent, commonParent, 
                                             startOffset, endOffset,
                                             aProperty, nsnull);
      }
      else
      {
        result = RemoveTextPropertiesForNodeWithDifferentParents(startParent,startOffset, 
                                                                 endParent,  endOffset,
                                                                 commonParent,
                                                                 aProperty, nsnull);
      }
      if (NS_SUCCEEDED(result))
      { // compute a range for the selection
        // don't want to actually do anything with selection, because
        // we are still iterating through it.  Just want to create and remember
        // an nsIDOMRange, and later add the range to the selection after clearing it.
        // XXX: I'm blocked here because nsIDOMSelection doesn't provide a mechanism
        //      for setting a compound selection yet.
      }
      nsEditor::EndTransaction();
      if (NS_SUCCEEDED(result))
      { 
        ResetTextSelectionForRange(parentForSelection, rangeStartOffset, rangeEndOffset, selection);
      }
    }
    // post-process 
    result = mRules->DidDoAction(selection, &ruleInfo, result);
  }
  if (gNoisy) 
  { 
    nsAutoString propString;
    aProperty->ToString(propString);
    char *propCString = propString.ToNewCString();
    if (gNoisy) { printf("---------- end nsTextEditor::RemoveTextProperty %s ----------\n", propCString); }
    delete [] propCString;
  }
  return result;
}

// XXX: should return nsresult
void nsTextEditor::GetTextSelectionOffsetsForRange(nsIDOMSelection *aSelection,
                                                   nsIDOMNode **aParent,
                                                   PRInt32     &aStartOffset, 
                                                   PRInt32     &aEndOffset)
{
  nsresult result;

  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset, endOffset;
  aSelection->GetAnchorNode(getter_AddRefs(startNode));
  aSelection->GetAnchorOffset(&startOffset);
  aSelection->GetFocusNode(getter_AddRefs(endNode));
  aSelection->GetFocusOffset(&endOffset);

  nsCOMPtr<nsIEnumerator> enumerator;
  result = aSelection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_SUCCEEDED(result) && enumerator)
  {
    enumerator->First(); 
    nsCOMPtr<nsISupports> currentItem;
    result = enumerator->CurrentItem(getter_AddRefs(currentItem));
    if ((NS_SUCCEEDED(result)) && currentItem)
    {
      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
      range->GetCommonParent(aParent);
    }
  }

  nsCOMPtr<nsIContentIterator> iter;
  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                              nsIContentIterator::GetIID(), 
                                              getter_AddRefs(iter));
  if ((NS_SUCCEEDED(result)) && iter)
  {
    PRUint32 totalLength=0;
    nsCOMPtr<nsIDOMCharacterData>textNode;
    nsCOMPtr<nsIContent>blockParentContent = do_QueryInterface(*aParent);
    iter->Init(blockParentContent);
    // loop through the content iterator for each content node
    nsCOMPtr<nsIContent> content;
    result = iter->CurrentNode(getter_AddRefs(content));
    while (NS_COMFALSE == iter->IsDone())
    {
      textNode = do_QueryInterface(content);
      if (textNode)
      {
        nsCOMPtr<nsIDOMNode>currentNode = do_QueryInterface(textNode);
        if (currentNode.get() == startNode.get())
        {
          aStartOffset = totalLength + startOffset;
        }
        if (currentNode.get() == endNode.get())
        {
          aEndOffset = totalLength + endOffset;
          break;
        }
        PRUint32 length;
        textNode->GetLength(&length);
        totalLength += length;
      }
      iter->Next();
      iter->CurrentNode(getter_AddRefs(content));
    }
  }
}

// XXX: should return nsresult
void nsTextEditor::ResetTextSelectionForRange(nsIDOMNode *aParent,
                                              PRInt32     aStartOffset,
                                              PRInt32     aEndOffset,
                                              nsIDOMSelection *aSelection)
{
  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset, endOffset;

  nsresult result;
  nsCOMPtr<nsIContentIterator> iter;
  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                              nsIContentIterator::GetIID(), 
                                              getter_AddRefs(iter));
  if ((NS_SUCCEEDED(result)) && iter)
  {
    PRBool setStart = PR_FALSE;
    PRUint32 totalLength=0;
    nsCOMPtr<nsIDOMCharacterData>textNode;
    nsCOMPtr<nsIContent>blockParentContent = do_QueryInterface(aParent);
    iter->Init(blockParentContent);
    // loop through the content iterator for each content node
    nsCOMPtr<nsIContent> content;
    result = iter->CurrentNode(getter_AddRefs(content));
    while (NS_COMFALSE == iter->IsDone())
    {
      textNode = do_QueryInterface(content);
      if (textNode)
      {
        PRUint32 length;
        textNode->GetLength(&length);
        if ((PR_FALSE==setStart) && aStartOffset<=(PRInt32)(totalLength+length))
        {
          setStart = PR_TRUE;
          startNode = do_QueryInterface(textNode);
          startOffset = aStartOffset-totalLength;
        }
        if (aEndOffset<=(PRInt32)(totalLength+length))
        {
          endNode = do_QueryInterface(textNode);
          endOffset = aEndOffset-totalLength;
          break;
        }        
        totalLength += length;
      }
      iter->Next();
      iter->CurrentNode(getter_AddRefs(content));
    }
    aSelection->Collapse(startNode, startOffset);
    aSelection->Extend(endNode, endOffset);
  }
}


NS_IMETHODIMP nsTextEditor::DeleteSelection(nsIEditor::ECollapsedSelectionAction aAction)
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->DeleteSelection(aAction);
#endif // ENABLE_JS_EDITOR_LOG

  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel= PR_FALSE;

  nsresult result = nsEditor::BeginTransaction();
  if (NS_FAILED(result)) { return result; }

  // pre-process
  nsEditor::GetSelection(getter_AddRefs(selection));
  nsTextRulesInfo ruleInfo(nsTextEditRules::kDeleteSelection);
  ruleInfo.collapsedAction = aAction;
  result = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if ((PR_FALSE==cancel) && (NS_SUCCEEDED(result)))
  {
    result = nsEditor::DeleteSelection(aAction);
    // post-process 
    result = mRules->DidDoAction(selection, &ruleInfo, result);
  }

  nsresult endTxnResult = nsEditor::EndTransaction();  // don't return this result!
  NS_ASSERTION ((NS_SUCCEEDED(endTxnResult)), "bad end transaction result");

  return result;
}

NS_IMETHODIMP nsTextEditor::InsertText(const nsString& aStringToInsert)
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->InsertText(aStringToInsert);
#endif // ENABLE_JS_EDITOR_LOG

  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel= PR_FALSE;

  // pre-process
  nsEditor::GetSelection(getter_AddRefs(selection));
  nsString resultString;
  PlaceholderTxn *placeholderTxn=nsnull;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kInsertText);
  ruleInfo.placeTxn = &placeholderTxn;
  ruleInfo.inString = &aStringToInsert;
  ruleInfo.outString = &resultString;
  ruleInfo.typeInState = *mTypeInState;
  ruleInfo.maxLength = mMaxTextLength;

  nsresult result = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if ((PR_FALSE==cancel) && (NS_SUCCEEDED(result)))
  {
    result = nsEditor::InsertText(resultString);
    // post-process 
    result = mRules->DidDoAction(selection, &ruleInfo, result);
  }
  if (placeholderTxn)
    placeholderTxn->SetAbsorb(PR_FALSE);  // this ends the merging of txns into placeholderTxn

  return result;
}

NS_IMETHODIMP nsTextEditor::SetMaxTextLength(PRInt32 aMaxTextLength)
{
  mMaxTextLength = aMaxTextLength;
  return NS_OK;
}

NS_IMETHODIMP nsTextEditor::GetMaxTextLength(PRInt32& aMaxTextLength)
{
  aMaxTextLength = mMaxTextLength;
  return NS_OK;
}

NS_IMETHODIMP nsTextEditor::InsertBreak()
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->InsertBreak();
#endif // ENABLE_JS_EDITOR_LOG

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel= PR_FALSE;

  // pre-process
  nsEditor::GetSelection(getter_AddRefs(selection));
  nsTextRulesInfo ruleInfo(nsTextEditRules::kInsertBreak);
  nsresult result = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if ((PR_FALSE==cancel) && (NS_SUCCEEDED(result)))
  {
    // For plainttext just pass newlines through
    nsAutoString  key;
    key += '\n';
    result = InsertText(key);
    nsEditor::GetSelection(getter_AddRefs(selection));
    result = mRules->DidDoAction(selection, &ruleInfo, result);
  }
  return result;
}


NS_IMETHODIMP nsTextEditor::EnableUndo(PRBool aEnable)
{
  return nsEditor::EnableUndo(aEnable);
}

NS_IMETHODIMP nsTextEditor::Undo(PRUint32 aCount)
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->Undo(aCount);
#endif // ENABLE_JS_EDITOR_LOG

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel= PR_FALSE;

  // pre-process
  nsEditor::GetSelection(getter_AddRefs(selection));
  nsTextRulesInfo ruleInfo(nsTextEditRules::kUndo);
  nsresult result = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if ((PR_FALSE==cancel) && (NS_SUCCEEDED(result)))
  {
    result = nsEditor::Undo(aCount);
    nsEditor::GetSelection(getter_AddRefs(selection));
    result = mRules->DidDoAction(selection, &ruleInfo, result);
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo)
{
  return nsEditor::CanUndo(aIsEnabled, aCanUndo);
}

NS_IMETHODIMP nsTextEditor::Redo(PRUint32 aCount)
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->Redo(aCount);
#endif // ENABLE_JS_EDITOR_LOG

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel= PR_FALSE;

  // pre-process
  nsEditor::GetSelection(getter_AddRefs(selection));
  nsTextRulesInfo ruleInfo(nsTextEditRules::kRedo);
  nsresult result = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if ((PR_FALSE==cancel) && (NS_SUCCEEDED(result)))
  {
    result = nsEditor::Redo(aCount);
    nsEditor::GetSelection(getter_AddRefs(selection));
    result = mRules->DidDoAction(selection, &ruleInfo, result);
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo)
{
  return nsEditor::CanRedo(aIsEnabled, aCanRedo);
}

NS_IMETHODIMP nsTextEditor::BeginTransaction()
{
  return nsEditor::BeginTransaction();
}

NS_IMETHODIMP nsTextEditor::EndTransaction()
{
  return nsEditor::EndTransaction();
}

NS_IMETHODIMP nsTextEditor::MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTextEditor::MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTextEditor::MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTextEditor::MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTextEditor::SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection) 
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTextEditor::SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTextEditor::SelectAll()
{
  return nsEditor::SelectAll();
}

NS_IMETHODIMP nsTextEditor::BeginningOfDocument()
{
  return nsEditor::BeginningOfDocument();
}

NS_IMETHODIMP nsTextEditor::EndOfDocument()
{
  return nsEditor::EndOfDocument();
}

NS_IMETHODIMP nsTextEditor::ScrollUp(nsIAtom *aIncrement)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTextEditor::ScrollDown(nsIAtom *aIncrement)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsTextEditor::ScrollIntoView(PRBool aScrollToBegin)
{
  return nsEditor::ScrollIntoView(aScrollToBegin);
}

static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);

NS_IMETHODIMP nsTextEditor::SaveDocument(PRBool saveAs, PRBool saveCopy)
{
	nsresult rv = NS_OK;
	
  // get the document
  nsCOMPtr<nsIDOMDocument> doc;
  rv = GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(rv) || !doc)
    return rv;
  
  nsCOMPtr<nsIDiskDocument>  diskDoc = do_QueryInterface(doc);
  if (!diskDoc)
    return NS_ERROR_NO_INTERFACE;
  
  // this should really call out to the appcore for the display of the put file
  // dialog.
  
  // find out if the doc already has a fileSpec associated with it.
  nsFileSpec		docFileSpec;
  PRBool mustShowFileDialog = saveAs || (diskDoc->GetFileSpec(docFileSpec) == NS_ERROR_NOT_INITIALIZED);
  PRBool replacing = !saveAs;
  
  if (mustShowFileDialog)
  {
  	nsCOMPtr<nsIFileWidget>	fileWidget;
    rv = nsComponentManager::CreateInstance(kCFileWidgetCID, nsnull, kIFileWidgetIID, getter_AddRefs(fileWidget));
    NS_ASSERTION((NS_SUCCEEDED(result) && fileWidget), "Failed to get file widget");
    if (NS_FAILED(result)) return result;
    if (!fileWidget) return NS_ERROR_NULL_POINTER;
    nsAutoString  promptString("Save this document as:");			// XXX i18n, l10n
  	nsFileDlgResults dialogResult;
  	dialogResult = fileWidget->PutFile(nsnull, promptString, docFileSpec);
  	if (dialogResult == nsFileDlgResults_Cancel)
  	  return NS_OK;
  	  
  	replacing = (dialogResult == nsFileDlgResults_Replace);
  }

  nsAutoString  charsetStr("");
  rv = diskDoc->SaveFile(&docFileSpec, replacing, saveCopy, nsIDiskDocument::eSaveFileHTML, charsetStr);
  
  if (NS_FAILED(rv))
  {
    // show some error dialog?
    NS_WARNING("Saving file failed");
  }
  
  return rv;
}

NS_IMETHODIMP nsTextEditor::Save()
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->Save();
#endif // ENABLE_JS_EDITOR_LOG

  nsresult rv = SaveDocument(PR_FALSE, PR_FALSE);
  if (NS_FAILED(rv))
    return rv;
   
  rv = DoAfterDocumentSave();
  return rv;
}

NS_IMETHODIMP nsTextEditor::SaveAs(PRBool aSavingCopy)
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->SaveAs(aSavingCopy);
#endif // ENABLE_JS_EDITOR_LOG

  return SaveDocument(PR_TRUE, aSavingCopy);
}

NS_IMETHODIMP nsTextEditor::Cut()
{
  return nsEditor::Cut();
}

NS_IMETHODIMP nsTextEditor::Copy()
{
  return nsEditor::Copy();
}

NS_IMETHODIMP nsTextEditor::Paste()
{
  return nsEditor::Paste();
}

//
// Similar to that in nsEditor::Paste except that it does indentation:
//
NS_IMETHODIMP nsTextEditor::PasteAsQuotation()
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->PasteAsQuotation();
#endif // ENABLE_JS_EDITOR_LOG

  nsString stuffToPaste;

  // Get Clipboard Service
  nsIClipboard* clipboard;
  nsresult rv = nsServiceManager::GetService(kCClipboardCID,
                                             nsIClipboard::GetIID(),
                                             (nsISupports **)&clipboard);

  // Create generic Transferable for getting the data
  nsCOMPtr<nsITransferable> trans;
  rv = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, 
                                          nsITransferable::GetIID(), 
                                          (void**) getter_AddRefs(trans));
  if (NS_OK == rv)
  {
    // Get nsITransferable interface for getting the data from the clipboard
    if (trans)
    {
      // We only handle plaintext pastes here
      nsAutoString flavor(kTextMime);

      trans->AddDataFlavor(&flavor);

      // Get the Data from the clipboard
      clipboard->GetData(trans);

      // Now we ask the transferable for the data
      // it still owns the data, we just have a pointer to it.
      // If it can't support a "text" output of the data the call will fail
      char *str = 0;
      PRUint32 len;
      if (NS_OK == trans->GetTransferData(&flavor, (void **)&str, &len)) {

        // Make adjustments for null terminated strings
        if (str && len > 0) {
          // stuffToPaste is ready for insertion into the content
          stuffToPaste.SetString(str, len);
        }
      }
    }
  }
  nsServiceManager::ReleaseService(kCClipboardCID, clipboard);

  return InsertAsQuotation(stuffToPaste);
}

NS_IMETHODIMP nsTextEditor::InsertAsQuotation(const nsString& aQuotedText)
{
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->InsertAsQuotation(aQuotedText);
#endif // ENABLE_JS_EDITOR_LOG

  // Now we have the text.  Cite it appropriately:
  nsCOMPtr<nsICiter> citer;
  nsCOMPtr<nsIPref> prefs;
  nsresult rv = nsServiceManager::GetService(kPrefServiceCID,
                                             nsIPref::GetIID(),
                                             (nsISupports**)&prefs);
  char *citationType = 0;
  rv = prefs->CopyCharPref("mail.compose.citationType", &citationType);
                          
  if (NS_SUCCEEDED(rv) && citationType[0])
  {
    if (!strncmp(citationType, "aol", 3))
      citer = new nsAOLCiter;
    else
      citer = new nsInternetCiter;
    PL_strfree(citationType);
  }
  else
    citer = new nsInternetCiter;
  
  nsServiceManager::ReleaseService(kPrefServiceCID, prefs);

  // Let the citer quote it for us:
  nsString quotedStuff;
  rv = citer->GetCiteString(aQuotedText, quotedStuff);
  if (!NS_SUCCEEDED(rv))
    return rv;

  // Insert blank lines after the quoted text:
  quotedStuff += "\n\n";

  return InsertText(quotedStuff);
}

//
// Get the wrap width for the first PRE tag in the document.
// If no PRE tag, throw an error.
//
NS_IMETHODIMP nsTextEditor::GetBodyWrapWidth(PRInt32 *aWrapColumn)
{
  nsresult res;

  if (! aWrapColumn)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMElement> preElement = FindPreElement();
  if (!preElement)
    return NS_ERROR_UNEXPECTED;
  nsString colsStr ("cols");
  nsString numCols;
  PRBool isSet;
  res = GetAttributeValue(preElement, colsStr, numCols, isSet);
  if (!NS_SUCCEEDED(res))
    return NS_ERROR_UNEXPECTED;

  if (isSet)
  {
    PRInt32 errCode;
    *aWrapColumn = numCols.ToInteger(&errCode);
    if (errCode)
      return NS_ERROR_FAILURE;
    return NS_OK;
  }

  // if we get here, cols isn't set, so check whether wrap is set:
  nsString wrapStr ("wrap");
  res = GetAttributeValue(preElement, colsStr, numCols, isSet);
  if (!NS_SUCCEEDED(res))
    return NS_ERROR_UNEXPECTED;

  if (isSet)
    *aWrapColumn = 0;   // wrap to window width
  else
    *aWrapColumn = -1;  // no wrap

  return NS_OK;
}

//
// Change the wrap width on the first <PRE> tag in this document.
// (Eventually want to search for more than one in case there are
// interspersed quoted text blocks.)
// 
NS_IMETHODIMP nsTextEditor::SetBodyWrapWidth(PRInt32 aWrapColumn)
{
  nsresult res;

  mWrapColumn = aWrapColumn;

  nsCOMPtr<nsIDOMElement> preElement = FindPreElement();
  if (!preElement)
    return NS_ERROR_UNEXPECTED;

  nsString wrapStr ("wrap");
  nsString colsStr ("cols");

  // If wrap col is nonpositive, then we need to remove any existing "cols":
  if (aWrapColumn <= 0)
  {
    (void)RemoveAttribute(preElement, colsStr);

    if (aWrapColumn == 0)        // Wrap to window width
    {
      nsString oneStr ("1");
      res = SetAttribute(preElement, wrapStr, oneStr);
    }
    else res = NS_OK;
    return res;
  }

  // Otherwise we're setting cols, want to remove wrap
  (void)RemoveAttribute(preElement, wrapStr);
  nsString numCols;
  numCols.Append(aWrapColumn, 10);
  res = SetAttribute(preElement, colsStr, numCols);

  // Layout doesn't detect that this attribute change requires redraw.  Sigh.
  //HACKForceRedraw();  // This doesn't do it either!

  return res;
}  

NS_IMETHODIMP nsTextEditor::ApplyStyleSheet(const nsString& aURL)
{
  return nsEditor::ApplyStyleSheet(aURL);
}

NS_IMETHODIMP nsTextEditor::OutputToString(nsString& aOutputString,
                                           const nsString& aFormatType,
                                           PRUint32 aFlags)
{
  PRBool cancel;
  nsString resultString;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kOutputText);
  ruleInfo.outString = &resultString;
  nsresult rv = mRules->WillDoAction(nsnull, &ruleInfo, &cancel);
  if (NS_FAILED(rv)) { return rv; }
  if (PR_TRUE==cancel)
  { // this case will get triggered by password fields
    aOutputString = *(ruleInfo.outString);
  }
  else
  { // default processing
    nsCOMPtr<nsITextEncoder> encoder;
    char* progid = new char[strlen(NS_DOC_ENCODER_PROGID_BASE) + aFormatType.Length() + 1];
    if (! progid)
      return NS_ERROR_OUT_OF_MEMORY;
    strcpy(progid, NS_DOC_ENCODER_PROGID_BASE);
    char* type = aFormatType.ToNewCString();
    strcat(progid, type);
    delete[] type;
    rv = nsComponentManager::CreateInstance(progid,
                                            nsnull,
                                            nsIDocumentEncoder::GetIID(),
                                            getter_AddRefs(encoder));

    delete[] progid;
    if (NS_FAILED(rv))
    {
      printf("Couldn't get progid %s\n", progid);
      return rv;
    }

    nsCOMPtr<nsIDOMDocument> domdoc;
    rv = GetDocument(getter_AddRefs(domdoc));
    if (NS_FAILED(rv))
      return rv;
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);

    nsCOMPtr<nsIPresShell> shell;

 	  rv = GetPresShell(getter_AddRefs(shell));
    if (NS_FAILED(rv))
      return rv;
    rv = encoder->Init(shell, doc, aFormatType);
    if (NS_FAILED(rv))
      return rv;

	  if (aFlags & EditorOutputSelectionOnly)
    {
	    nsCOMPtr<nsIDOMSelection> selection;
	    rv = GetSelection(getter_AddRefs(selection));
      // XXX: should we return here if we can't get the selection?
	    if (NS_SUCCEEDED(rv) && selection)
	      encoder->SetSelection(selection);
	  }
	  
    // Try to set pretty printing, but don't panic if it doesn't work:
    (void)encoder->PrettyPrint((aFlags & EditorOutputFormatted)
                               ? PR_TRUE : PR_FALSE);
    // Indicate whether we want the comment and doctype headers prepended:
    (void)encoder->AddHeader((aFlags & EditorOutputNoDoctype)
                             ? PR_FALSE : PR_TRUE);
    // Set the wrap column.  If our wrap column is 0,
    // i.e. wrap to body width, then don't set it, let the
    // document encoder use its own default.
    if (mWrapColumn != 0)
    {
      PRUint32 wc;
      if (mWrapColumn < 0)
        wc = 0;
      else
        wc = (PRUint32)mWrapColumn;
      if (mWrapColumn > 0)
        (void)encoder->SetWrapColumn(wc);
    }

    rv = encoder->EncodeToString(aOutputString);
  }
  return rv;
}

NS_IMETHODIMP nsTextEditor::OutputToStream(nsIOutputStream* aOutputStream,
                                           const nsString& aFormatType,
                                           const nsString* aCharset,
                                           PRUint32 aFlags)
{
  nsresult rv;
  nsCOMPtr<nsITextEncoder> encoder;
  char* progid = new char[strlen(NS_DOC_ENCODER_PROGID_BASE) + aFormatType.Length() + 1];
  if (! progid)
      return NS_ERROR_OUT_OF_MEMORY;

  strcpy(progid, NS_DOC_ENCODER_PROGID_BASE);
  char* type = aFormatType.ToNewCString();
  strcat(progid, type);
  delete[] type;
  rv = nsComponentManager::CreateInstance(progid,
                                          nsnull,
                                          nsIDocumentEncoder::GetIID(),
                                          getter_AddRefs(encoder));

  delete[] progid;
  if (NS_FAILED(rv))
  {
    printf("Couldn't get progid %s\n", progid);
    return rv;
  }

  nsCOMPtr<nsIDOMDocument> domdoc;
  rv = GetDocument(getter_AddRefs(domdoc));
  if (NS_FAILED(rv))
    return rv;
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);

  if (aCharset && aCharset->Length() != 0 && aCharset->Equals("null")==PR_FALSE)
    encoder->SetCharset(*aCharset);

  nsCOMPtr<nsIPresShell> shell;

 	rv = GetPresShell(getter_AddRefs(shell));
  if (NS_SUCCEEDED(rv)) {
    rv = encoder->Init(shell,doc, aFormatType);
    if (NS_FAILED(rv))
      return rv;
  }

  if (aFlags & EditorOutputSelectionOnly)
  {
    nsCOMPtr<nsIDOMSelection>  selection;
    rv = GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(rv) && selection)
      //XXX: should we return here if we can't get selection?
      encoder->SetSelection(selection);
  }

  // Try to set pretty printing, but don't panic if it doesn't work:
  (void)encoder->PrettyPrint((aFlags & EditorOutputFormatted)
                             ? PR_TRUE : PR_FALSE);
  // Indicate whether we want the comment and doc type headers prepended:
  (void)encoder->AddHeader((aFlags & EditorOutputNoDoctype)
                           ? PR_FALSE : PR_TRUE);
  // Set the wrap column.  If our wrap column is 0,
  // i.e. wrap to body width, then don't set it, let the
  // document encoder use its own default.
  if (mWrapColumn != 0)
  {
    PRUint32 wc;
    if (mWrapColumn < 0)
      wc = 0;
    else
      wc = (PRUint32)mWrapColumn;
    if (mWrapColumn > 0)
      (void)encoder->SetWrapColumn(wc);
  }

  return encoder->EncodeToStream(aOutputStream);
}

nsCOMPtr<nsIDOMElement>
nsTextEditor::FindPreElement()
{
  nsCOMPtr<nsIDOMDocument> domdoc;
  nsEditor::GetDocument(getter_AddRefs(domdoc));
  if (!domdoc)
    return 0;

  nsCOMPtr<nsIDocument> doc (do_QueryInterface(domdoc));
  if (!doc)
    return 0;

  nsIContent* rootContent = doc->GetRootContent();
  if (!rootContent)
    return 0;

  nsCOMPtr<nsIDOMNode> rootNode (do_QueryInterface(rootContent));
  if (!rootNode)
    return 0;

  nsString prestr ("PRE");  // GetFirstNodeOfType requires capitals
  nsCOMPtr<nsIDOMNode> preNode;
  if (!NS_SUCCEEDED(nsEditor::GetFirstNodeOfType(rootNode, prestr,
                                                 getter_AddRefs(preNode))))
    return 0;

  return do_QueryInterface(preNode);
}



NS_IMETHODIMP nsTextEditor::SetTextPropertiesForNode(nsIDOMNode  *aNode, 
                                                     nsIDOMNode  *aParent,
                                                     PRInt32      aStartOffset,
                                                     PRInt32      aEndOffset,
                                                     nsIAtom     *aPropName, 
                                                     const nsString *aAttribute,
                                                     const nsString *aValue)
{
  if (gNoisy) { printf("nsTextEditor::SetTextPropertyForNode\n"); }
  nsresult result=NS_OK;
  PRBool textPropertySet;
  nsCOMPtr<nsIDOMNode>resultNode;
  IsTextPropertySetByContent(aNode, aPropName, aAttribute, aValue, textPropertySet, getter_AddRefs(resultNode));
  if (PR_FALSE==textPropertySet)
  {
    if (aValue  &&  0!=aValue->Length())
    {
      result = RemoveTextPropertiesForNode(aNode, aParent, aStartOffset, aEndOffset, aPropName, aAttribute);
    }
    nsAutoString tag;
    aPropName->ToString(tag);
    if (NS_SUCCEEDED(result)) 
    {
      nsCOMPtr<nsIDOMNode>newStyleNode;
      result = nsEditor::CreateNode(tag, aParent, 0, getter_AddRefs(newStyleNode));
      if (NS_FAILED(result)) return result;
      if (!newStyleNode) return NS_ERROR_NULL_POINTER;

      nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
      nodeAsChar =  do_QueryInterface(aNode);
      if (nodeAsChar)
      {
        result = MoveContentOfNodeIntoNewParent(aNode, newStyleNode, aStartOffset, aEndOffset);
      }
      else
      { // handle non-text selection
        nsCOMPtr<nsIDOMNode> parent;  // used just to make the code easier to understand
        nsCOMPtr<nsIDOMNode> child;
        parent = do_QueryInterface(aNode);
        child = GetChildAt(parent, aStartOffset);
        // XXX: need to loop for aStartOffset!=aEndOffset-1?
        PRInt32 offsetInParent = aStartOffset; // remember where aNode was in aParent
        if (NS_SUCCEEDED(result))
        { // remove child from parent
          result = nsEditor::DeleteNode(child);
          if (NS_SUCCEEDED(result)) 
          { // put child into the newStyleNode
            result = nsEditor::InsertNode(child, newStyleNode, 0);
            if (NS_SUCCEEDED(result)) 
            { // put newStyleNode in parent where child was
              result = nsEditor::InsertNode(newStyleNode, parent, offsetInParent);
            }
          }
        }
      }
      if (NS_SUCCEEDED(result)) 
      {
        if (aAttribute && 0!=aAttribute->Length())
        {
          nsCOMPtr<nsIDOMElement> newStyleElement;
          newStyleElement = do_QueryInterface(newStyleNode);
          nsAutoString value;
          if (aValue) {
            value = *aValue;
          }
          // XXX should be a call to editor to change attribute!
          result = newStyleElement->SetAttribute(*aAttribute, value);
        }
      }
    }
  }
  if (gNoisy) { printf("SetTextPropertyForNode returning %d, dumping content...\n", result);}
  if (gNoisy) {DebugDumpContent(); } // DEBUG
  return result;
}

NS_IMETHODIMP nsTextEditor::MoveContentOfNodeIntoNewParent(nsIDOMNode  *aNode, 
                                                           nsIDOMNode  *aNewParentNode, 
                                                           PRInt32      aStartOffset, 
                                                           PRInt32      aEndOffset)
{
  if (!aNode || !aNewParentNode) { return NS_ERROR_NULL_POINTER; }
  if (gNoisy) { printf("nsTextEditor::MoveContentOfNodeIntoNewParent\n"); }
  nsresult result=NS_OK;

  PRUint32 count;
  result = GetLengthOfDOMNode(aNode, count);

  if (NS_SUCCEEDED(result))
  {
    nsCOMPtr<nsIDOMNode>newChildNode;  // this will be the child node we move into the new style node
    // split the node at the start offset unless the split would create an empty node
    if (aStartOffset!=0)
    {
      result = nsEditor::SplitNode(aNode, aStartOffset, getter_AddRefs(newChildNode));
      if (gNoisy) { printf("* split created left node %p\n", newChildNode.get());}
      if (gNoisy) {DebugDumpContent(); } // DEBUG
    }
    if (NS_SUCCEEDED(result))
    {
      if (aEndOffset!=(PRInt32)count)
      {
        result = nsEditor::SplitNode(aNode, aEndOffset-aStartOffset, getter_AddRefs(newChildNode));
        if (gNoisy) { printf("* split created left node %p\n", newChildNode.get());}
        if (gNoisy) {DebugDumpContent(); } // DEBUG
      }
      else
      {
        newChildNode = do_QueryInterface(aNode);
        if (gNoisy) { printf("* second split not required, new text node set to aNode = %p\n", newChildNode.get());}
      }
      if (NS_SUCCEEDED(result))
      {
        // move aNewParentNode into the right location

        // optimization:  if all we're doing is changing a value for an existing attribute for the
        //                entire selection, then just twiddle the existing style node
        PRBool done = PR_FALSE; // set to true in optimized case if we can really do the optimization
        /*
        if (aAttribute && aValue && (0==aStartOffset) && (aEndOffset==(PRInt32)count))
        {
          // ??? can we really compute this?
        }
        */
        if (PR_FALSE==done)
        {
          // if we've ended up with an empty text node, just delete it and we're done
          nsCOMPtr<nsIDOMCharacterData>newChildNodeAsChar;
          newChildNodeAsChar =  do_QueryInterface(newChildNode);
          PRUint32 newChildNodeLength;
          if (newChildNodeAsChar)
          {
            newChildNodeAsChar->GetLength(&newChildNodeLength);
            if (0==newChildNodeLength)
            {
              result = nsEditor::DeleteNode(newChildNode); 
              done = PR_TRUE;
              // XXX: need to set selection here
            }
          }
          // move the new child node into the new parent
          if (PR_FALSE==done)
          {
            // first, move the new parent into the correct location
            PRInt32 offsetInParent;
            nsCOMPtr<nsIDOMNode>parentNode;
            result = aNode->GetParentNode(getter_AddRefs(parentNode));
            if (NS_SUCCEEDED(result))
            {
              result = nsEditor::DeleteNode(aNewParentNode);
              if (NS_SUCCEEDED(result))
              { // must get child offset AFTER delete of aNewParentNode!
                result = GetChildOffset(aNode, parentNode, offsetInParent);
                if (NS_SUCCEEDED(result))
                {
                  result = nsEditor::InsertNode(aNewParentNode, parentNode, offsetInParent);
                  if (NS_SUCCEEDED(result))
                  {
                    // then move the new child into the new parent node
                    result = nsEditor::DeleteNode(newChildNode);
                    if (NS_SUCCEEDED(result)) 
                    {
                      result = nsEditor::InsertNode(newChildNode, aNewParentNode, 0);
                      if (NS_SUCCEEDED(result)) 
                      { // set the selection
                        nsCOMPtr<nsIDOMSelection>selection;
                        result = nsEditor::GetSelection(getter_AddRefs(selection));
                        if (NS_SUCCEEDED(result)) 
                        {
                          selection->Collapse(newChildNode, 0);
                          PRInt32 endOffset = aEndOffset-aStartOffset;
                          selection->Extend(newChildNode, endOffset);
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
    }
  }
  return result;
}

/* this should only get called if the only intervening nodes are inline style nodes */
NS_IMETHODIMP 
nsTextEditor::SetTextPropertiesForNodesWithSameParent(nsIDOMNode  *aStartNode,
                                                      PRInt32      aStartOffset,
                                                      nsIDOMNode  *aEndNode,
                                                      PRInt32      aEndOffset,
                                                      nsIDOMNode  *aParent,
                                                      nsIAtom     *aPropName,
                                                      const nsString *aAttribute,
                                                      const nsString *aValue)
{
  if (gNoisy) { printf("nsTextEditor::SetTextPropertiesForNodesWithSameParent\n"); }
  nsresult result=NS_OK;
  PRBool textPropertySet;
  nsCOMPtr<nsIDOMNode>resultNode;
  IsTextPropertySetByContent(aStartNode, aPropName, aAttribute, aValue, textPropertySet, getter_AddRefs(resultNode));
  if (PR_FALSE==textPropertySet)
  {
    if (aValue  &&  0!=aValue->Length())
    {
      result = RemoveTextPropertiesForNodesWithSameParent(aStartNode, aStartOffset,
                                                          aEndNode,   aEndOffset,
                                                          aParent, aPropName, aAttribute);
    }
    nsAutoString tag;
    aPropName->ToString(tag);
    // create the new style node, which will be the new parent for the selected nodes
    nsCOMPtr<nsIDOMNode>newStyleNode;
    nsCOMPtr<nsIDOMDocument>doc;
    result = GetDocument(getter_AddRefs(doc));
    if (NS_FAILED(result)) return result;
    if (!doc) return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIDOMElement>newElement;
    result = doc->CreateElement(tag, getter_AddRefs(newElement));
    if (NS_FAILED(result)) return result;
    if (!newElement) return NS_ERROR_NULL_POINTER;
    newStyleNode = do_QueryInterface(newElement);
    if (!newStyleNode) return NS_ERROR_NULL_POINTER;
    result = MoveContiguousContentIntoNewParent(aStartNode, aStartOffset, aEndNode, aEndOffset, aParent, newStyleNode);
    if (NS_SUCCEEDED(result) && aAttribute && 0!=aAttribute->Length())
    {
      nsCOMPtr<nsIDOMElement> newStyleElement;
      newStyleElement = do_QueryInterface(newStyleNode);
      nsAutoString value;
      if (aValue) {
        value = *aValue;
      }
      result = newStyleElement->SetAttribute(*aAttribute, value);
    }
  }
  return result;
}

//XXX won't work for selections that are not leaf nodes!
//    should fix up the end points to make sure they are leaf nodes
NS_IMETHODIMP nsTextEditor::MoveContiguousContentIntoNewParent(nsIDOMNode  *aStartNode, 
                                                               PRInt32      aStartOffset, 
                                                               nsIDOMNode  *aEndNode,
                                                               PRInt32      aEndOffset,
                                                               nsIDOMNode  *aGrandParentNode,
                                                               nsIDOMNode  *aNewParentNode)
{
  if (!aStartNode || !aEndNode || !aNewParentNode) { return NS_ERROR_NULL_POINTER; }
  if (gNoisy) { printf("nsTextEditor::MoveContiguousContentIntoNewParent\n"); }

  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode>startNode, endNode;
  PRInt32 startOffset = aStartOffset; // this will be the left edge of what we change
  PRInt32 endOffset = aEndOffset;     // this will be the right edge of what we change
  nsCOMPtr<nsIDOMNode>newLeftNode;  // this will be the middle text node
  if (IsTextNode(aStartNode))
  {
    startOffset = 0;
    if (gNoisy) { printf("aStartNode is a text node.\n"); }
    startNode = do_QueryInterface(aStartNode);
    if (0!=aStartOffset) 
    {
      result = nsEditor::SplitNode(aStartNode, aStartOffset, getter_AddRefs(newLeftNode));
      if (gNoisy) { printf("split aStartNode, newLeftNode = %p\n", newLeftNode.get()); }
    }
    else { 
      if (gNoisy) { printf("did not split aStartNode\n"); } 
    }
  }
  else {
    startNode = GetChildAt(aStartNode, aStartOffset);
    if (gNoisy) { printf("aStartNode is not a text node, got startNode = %p.\n", startNode.get()); }
  }
  if (NS_SUCCEEDED(result))
  {
    nsCOMPtr<nsIDOMNode>newRightNode;  // this will be the middle text node
    if (IsTextNode(aEndNode))
    {
      if (gNoisy) { printf("aEndNode is a text node.\n"); }
      endNode = do_QueryInterface(aEndNode);
      PRUint32 count;
      GetLengthOfDOMNode(aEndNode, count);
      if ((PRInt32)count!=aEndOffset) 
      {
        result = nsEditor::SplitNode(aEndNode, aEndOffset, getter_AddRefs(newRightNode));
        if (gNoisy) { printf("split aEndNode, newRightNode = %p\n", newRightNode.get()); }
      }
      else {
        newRightNode = do_QueryInterface(aEndNode);
        if (gNoisy) { printf("did not split aEndNode\n"); }
      }
    }
    else
    {
      endNode = GetChildAt(aEndNode, aEndOffset-1);
      newRightNode = do_QueryInterface(endNode);
      if (gNoisy) { printf("aEndNode is not a text node, got endNode = %p.\n", endNode.get()); }
    }
    if (NS_SUCCEEDED(result))
    {
      PRInt32 offsetInParent;
      result = GetChildOffset(startNode, aGrandParentNode, offsetInParent);
      /*
      if (newLeftNode) {
        result = GetChildOffset(newLeftNode, aGrandParentNode, offsetInParent);
      }
      else {
        offsetInParent = 0; // relies on +1 below in call to CreateNode
      }
      */
      if (NS_SUCCEEDED(result))
      {
        // insert aNewParentNode into aGrandParentNode
        result = nsEditor::InsertNode(aNewParentNode, aGrandParentNode, offsetInParent);
        if (NS_SUCCEEDED(result))
        { // move the right half of the start node into the new parent node
          nsCOMPtr<nsIDOMNode>intermediateNode;
          result = startNode->GetNextSibling(getter_AddRefs(intermediateNode));
          if (NS_SUCCEEDED(result))
          {
            result = nsEditor::DeleteNode(startNode);
            if (NS_SUCCEEDED(result)) 
            { 
              PRInt32 childIndex=0;
              result = nsEditor::InsertNode(startNode, aNewParentNode, childIndex);
              childIndex++;
              if (NS_SUCCEEDED(result))
              { // move all the intermediate nodes into the new parent node
                nsCOMPtr<nsIDOMNode>nextSibling;
                while (intermediateNode.get() != endNode.get())
                {
                  if (!intermediateNode)
                    result = NS_ERROR_NULL_POINTER;
                  if (NS_FAILED(result)) {
                    break;
                  }
                  // get the next sibling before moving the current child!!!
                  intermediateNode->GetNextSibling(getter_AddRefs(nextSibling));
                  result = nsEditor::DeleteNode(intermediateNode);
                  if (NS_SUCCEEDED(result)) {
                    result = nsEditor::InsertNode(intermediateNode, aNewParentNode, childIndex);
                    childIndex++;
                  }
                  intermediateNode = do_QueryInterface(nextSibling);
                }
                if (NS_SUCCEEDED(result))
                { // move the left half of the end node into the new parent node
                  result = nsEditor::DeleteNode(newRightNode);
                  if (NS_SUCCEEDED(result)) 
                  {
                    result = nsEditor::InsertNode(newRightNode, aNewParentNode, childIndex);
                    // now set the selection
                    if (NS_SUCCEEDED(result)) 
                    {
                      nsCOMPtr<nsIDOMSelection>selection;
                      result = nsEditor::GetSelection(getter_AddRefs(selection));
                      if (NS_SUCCEEDED(result)) 
                      {
                        selection->Collapse(startNode, startOffset);
                        selection->Extend(newRightNode, endOffset);
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
  }
  return result;
}


/* this wraps every selected text node in a new inline style node if needed
   the text nodes are treated as being unique -- each needs it's own style node 
   if the style is not already present.
   each action has immediate effect on the content tree and resolved style, so
   doing outermost text nodes first removes the need for interior style nodes in some cases.
   XXX: need to code test to see if new style node is needed
*/
NS_IMETHODIMP
nsTextEditor::SetTextPropertiesForNodeWithDifferentParents(nsIDOMRange *aRange,
                                                           nsIDOMNode  *aStartNode,
                                                           PRInt32      aStartOffset,
                                                           nsIDOMNode  *aEndNode,
                                                           PRInt32      aEndOffset,
                                                           nsIDOMNode  *aParent,
                                                           nsIAtom     *aPropName,
                                                           const nsString *aAttribute,
                                                           const nsString *aValue)
{
  if (gNoisy) { printf("start nsTextEditor::SetTextPropertiesForNodeWithDifferentParents\n"); }
  nsresult result=NS_OK;
  PRUint32 count;
  if (!aRange || !aStartNode || !aEndNode || !aParent || !aPropName)
    return NS_ERROR_NULL_POINTER;

  PRInt32 startOffset, endOffset;
  // create a style node for the text in the start parent
  nsCOMPtr<nsIDOMNode>parent;

  result = RemoveTextPropertiesForNodeWithDifferentParents(aStartNode,aStartOffset, 
                                                           aEndNode,  aEndOffset, 
                                                           aParent,   aPropName, aAttribute);
  if (NS_FAILED(result)) { return result; }

  // RemoveTextProperties... might have changed selection endpoints, get new ones
  nsCOMPtr<nsIDOMSelection>selection;
  result = nsEditor::GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) { return result; }
  if (!selection) { return NS_ERROR_NULL_POINTER; } 
  selection->GetAnchorOffset(&aStartOffset);
  selection->GetFocusOffset(&aEndOffset);

  // create new parent nodes for all the content between the start and end nodes
  nsCOMPtr<nsIContentIterator>iter;
  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                              nsIContentIterator::GetIID(), getter_AddRefs(iter));
  if (NS_FAILED(result)) return result;
  if (!iter) return NS_ERROR_NULL_POINTER;

  // find our starting point
  PRBool startIsText = IsTextNode(aStartNode);
  nsCOMPtr<nsIContent>startContent;
  if (PR_TRUE==startIsText) {
    startContent = do_QueryInterface(aStartNode);
  }
  else {
    nsCOMPtr<nsIDOMNode>node = GetChildAt(aStartNode, aStartOffset);
    startContent = do_QueryInterface(node);
  }

  // find our ending point
  PRBool endIsText = IsTextNode(aEndNode);
  nsCOMPtr<nsIContent>endContent;
  if (PR_TRUE==endIsText) {
    endContent = do_QueryInterface(aEndNode);
  }
  else 
  {
    nsCOMPtr<nsIDOMNode>theEndNode;
    if (aEndOffset>0)
    {
      theEndNode = GetChildAt(aEndNode, aEndOffset-1);
    }
    else {
      // XXX: we need to find the previous node and set the selection correctly
      NS_ASSERTION(0, "unexpected selection");
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    endContent = do_QueryInterface(theEndNode);
  }

  if (!startContent || !endContent) { return NS_ERROR_NULL_POINTER; }
  // iterate over the nodes between the starting and ending points
  iter->Init(aRange);
  nsCOMPtr<nsIContent> content;
  iter->CurrentNode(getter_AddRefs(content));
  nsAutoString tag;
  aPropName->ToString(tag);
  while (NS_COMFALSE == iter->IsDone())
  {
    if ((content.get() != startContent.get()) &&
        (content.get() != endContent.get()))
    {
      nsCOMPtr<nsIDOMNode>node;
      node = do_QueryInterface(content);
      if (IsEditable(node))
      {
        PRBool canContainChildren;
        content->CanContainChildren(canContainChildren);
        if (PR_FALSE==canContainChildren)
        { 
          nsEditor::GetTagString(node,tag);
          if (tag != "br")  // skip <BR>, even though it's a leaf
          { // only want to wrap the text node in a new style node if it doesn't already have that style
            if (gNoisy) { printf("node %p is an editable leaf.\n", node.get()); }
            PRBool textPropertySet;
            nsCOMPtr<nsIDOMNode>resultNode;
            IsTextPropertySetByContent(node, aPropName, aAttribute, aValue, textPropertySet, getter_AddRefs(resultNode));
            if (PR_FALSE==textPropertySet)
            {
              if (gNoisy) { printf("property not set\n"); }
              node->GetParentNode(getter_AddRefs(parent));
              if (!parent) { return NS_ERROR_NULL_POINTER; }
              nsCOMPtr<nsIContent>parentContent;
              parentContent = do_QueryInterface(parent);
              nsCOMPtr<nsIDOMNode>parentNode = do_QueryInterface(parent);
              if (PR_TRUE==IsTextNode(node))
              {
                startOffset = 0;
                result = GetLengthOfDOMNode(node, (PRUint32&)endOffset);
              }
              else
              {
                parentContent->IndexOf(content, startOffset);
                endOffset = startOffset+1;
              }
              if (gNoisy) { printf("start/end = %d %d\n", startOffset, endOffset); }
              if (NS_SUCCEEDED(result)) {
                result = SetTextPropertiesForNode(node, parentNode, startOffset, endOffset, aPropName, aAttribute, aValue);
              }
            }
          }
        }
      }
      // XXX: shouldn't there be an else here for non-text leaf nodes?
    }
    // note we don't check the result, we just rely on iter->IsDone
    iter->Next();
    iter->CurrentNode(getter_AddRefs(content));
  }
  // handle endpoints
  if (NS_SUCCEEDED(result))
  {
    // create a style node for the text in the start parent
    nsCOMPtr<nsIDOMNode>startNode = do_QueryInterface(startContent);
    result = startNode->GetParentNode(getter_AddRefs(parent));
    if (NS_FAILED(result)) {
      return result;
    }
    nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
    nodeAsChar = do_QueryInterface(startNode);
    if (nodeAsChar)
    {   
      nodeAsChar->GetLength(&count);
      if (gNoisy) { printf("processing start node %p.\n", nodeAsChar.get()); }
      result = SetTextPropertiesForNode(startNode, parent, aStartOffset, count, aPropName, aAttribute, aValue);
      startOffset = 0;
    }
    else 
    {
      nsCOMPtr<nsIDOMNode>grandParent;
      result = parent->GetParentNode(getter_AddRefs(grandParent));
      if (gNoisy) { printf("processing start node %p.\n", parent.get()); }
      result = SetTextPropertiesForNode(parent, grandParent, aStartOffset, aStartOffset+1, aPropName, aAttribute, aValue);
      startNode = do_QueryInterface(parent);
      startOffset = aStartOffset;
    }


    if (NS_SUCCEEDED(result))
    {
      // create a style node for the text in the end parent
      nsCOMPtr<nsIDOMNode>endNode = do_QueryInterface(endContent);
      result = endNode->GetParentNode(getter_AddRefs(parent));
      if (NS_FAILED(result)) {
        return result;
      }
      nodeAsChar = do_QueryInterface(endNode);
      if (nodeAsChar)
      {
        nodeAsChar->GetLength(&count);
        if (gNoisy) { printf("processing end node %p.\n", nodeAsChar.get()); }
        result = SetTextPropertiesForNode(endNode, parent, 0, aEndOffset, aPropName, aAttribute, aValue);
        // SetTextPropertiesForNode kindly computed the proper selection focus node and offset for us, 
        // remember them here
        selection->GetFocusOffset(&endOffset);
        selection->GetFocusNode(getter_AddRefs(endNode));
      }
      else 
      {
        NS_ASSERTION(0!=aEndOffset, "unexpected selection end offset");
        if (0==aEndOffset) { return NS_ERROR_NOT_IMPLEMENTED; }
        nsCOMPtr<nsIDOMNode>grandParent;
        result = parent->GetParentNode(getter_AddRefs(grandParent));
        if (gNoisy) { printf("processing end node %p.\n", parent.get()); }
        result = SetTextPropertiesForNode(parent, grandParent, aEndOffset-1, aEndOffset, aPropName, aAttribute, aValue);
        endNode = do_QueryInterface(parent);
        endOffset = 0;
      }
      if (NS_SUCCEEDED(result))
      {

        selection->Collapse(startNode, startOffset);
        selection->Extend(endNode, aEndOffset);
      }
    }
  }
  if (gNoisy) { printf("end nsTextEditor::SetTextPropertiesForNodeWithDifferentParents\n"); }

  return result;
}

NS_IMETHODIMP nsTextEditor::RemoveTextPropertiesForNode(nsIDOMNode *aNode, 
                                                        nsIDOMNode *aParent,
                                                        PRInt32     aStartOffset,
                                                        PRInt32     aEndOffset,
                                                        nsIAtom    *aPropName,
                                                        const nsString *aAttribute)
{
  if (gNoisy) { printf("nsTextEditor::RemoveTextPropertyForNode\n"); }
  nsresult result=NS_OK;
  nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
  nodeAsChar =  do_QueryInterface(aNode);
  PRBool textPropertySet;
  nsCOMPtr<nsIDOMNode>resultNode;
  IsTextPropertySetByContent(aNode, aPropName, aAttribute, nsnull, textPropertySet, getter_AddRefs(resultNode));
  if (PR_TRUE==textPropertySet)
  {
    nsCOMPtr<nsIDOMNode>parent; // initially set to first interior parent node to process
    nsCOMPtr<nsIDOMNode>newMiddleNode; // this will be the middle node after any required splits
    nsCOMPtr<nsIDOMNode>newLeftNode;   // this will be the leftmost node, 
                                       // the node being split will be rightmost
    PRUint32 count;
    // if aNode is a text node, treat is specially
    if (nodeAsChar)
    {
      nodeAsChar->GetLength(&count);
      // split the node, and all parent nodes up to the style node
      // then promote the selected content to the parent of the style node
      if (0!=aStartOffset) {
        if (gNoisy) { printf("* splitting text node %p at %d\n", aNode, aStartOffset);}
        result = nsEditor::SplitNode(aNode, aStartOffset, getter_AddRefs(newLeftNode));
        if (gNoisy) { printf("* split created left node %p\n", newLeftNode.get());}
        if (gNoisy) {DebugDumpContent(); } // DEBUG
      }
      if (NS_SUCCEEDED(result))
      {
        if ((PRInt32)count!=aEndOffset) {
          if (gNoisy) { printf("* splitting text node (right node) %p at %d\n", aNode, aEndOffset-aStartOffset);}
          result = nsEditor::SplitNode(aNode, aEndOffset-aStartOffset, getter_AddRefs(newMiddleNode));
          if (gNoisy) { printf("* split created middle node %p\n", newMiddleNode.get());}
          if (gNoisy) {DebugDumpContent(); } // DEBUG
        }
        else {
          if (gNoisy) { printf("* no need to split text node, middle to aNode\n");}
          newMiddleNode = do_QueryInterface(aNode);
        }
        NS_ASSERTION(newMiddleNode, "no middle node created");
        // now that the text node is split, split parent nodes until we get to the style node
        parent = do_QueryInterface(aParent);  // we know this has to succeed, no need to check
      }
    }
    else {
      newMiddleNode = do_QueryInterface(aNode);
      parent = do_QueryInterface(aParent); 
    }
    if (NS_FAILED(result)) return result;
    if (!newMiddleNode) return NS_ERROR_NULL_POINTER;

    // split every ancestor until we find the node that is giving us the style we want to remove
    // then split the style node and promote the selected content to the style node's parent
    while (NS_SUCCEEDED(result) && parent)
    {
      if (gNoisy) { printf("* looking at parent %p\n", parent.get());}
      // get the tag from parent and see if we're done
      nsCOMPtr<nsIDOMNode>temp;
      nsCOMPtr<nsIDOMElement>element;
      element = do_QueryInterface(parent);
      if (element)
      {
        nsAutoString tag;
        result = element->GetTagName(tag);
        if (gNoisy) { printf("* parent has tag %s\n", tag.ToNewCString()); } // XXX leak!
        if (NS_SUCCEEDED(result))
        {
          const PRUnichar *unicodeString;
          aPropName->GetUnicode(&unicodeString);
          if (PR_FALSE==tag.EqualsIgnoreCase(unicodeString))
          {
            PRInt32 offsetInParent;
            result = GetChildOffset(newMiddleNode, parent, offsetInParent);
            if (NS_SUCCEEDED(result))
            {
              if (0!=offsetInParent) {
                if (gNoisy) { printf("* splitting parent %p at offset %d\n", parent.get(), offsetInParent);}
                result = nsEditor::SplitNode(parent, offsetInParent, getter_AddRefs(newLeftNode));
                if (gNoisy) { printf("* split created left node %p sibling of parent\n", newLeftNode.get());}
                if (gNoisy) {DebugDumpContent(); } // DEBUG
              }
              if (NS_SUCCEEDED(result))
              {
                nsCOMPtr<nsIDOMNodeList>childNodes;
                result = parent->GetChildNodes(getter_AddRefs(childNodes));
                if (NS_FAILED(result)) return result;
                if (!childNodes) return NS_ERROR_NULL_POINTER;

                childNodes->GetLength(&count);
                NS_ASSERTION(count>0, "bad child count in newly split node");
                if ((PRInt32)count!=1) 
                {
                  if (gNoisy) { printf("* splitting parent %p at offset %d\n", parent.get(), 1);}
                  result = nsEditor::SplitNode(parent, 1, getter_AddRefs(newMiddleNode));
                  if (gNoisy) { printf("* split created middle node %p sibling of parent\n", newMiddleNode.get());}
                  if (gNoisy) {DebugDumpContent(); } // DEBUG
                }
                else {
                  if (gNoisy) { printf("* no need to split parent, newMiddleNode=parent\n");}
                  newMiddleNode = do_QueryInterface(parent);
                }
                NS_ASSERTION(newMiddleNode, "no middle node created");
                parent->GetParentNode(getter_AddRefs(temp));
                parent = do_QueryInterface(temp);
              }
            }
          }
          // else we've found the style tag (referred to by "parent")
          // newMiddleNode is the node that is an ancestor to the selection
          else
          {
            if (gNoisy) { printf("* this is the style node\n");}
            PRInt32 offsetInParent;
            result = GetChildOffset(newMiddleNode, parent, offsetInParent);
            if (NS_SUCCEEDED(result))
            {
              nsCOMPtr<nsIDOMNodeList>childNodes;
              result = parent->GetChildNodes(getter_AddRefs(childNodes));
              if (NS_FAILED(result)) return result;
              if (!childNodes) return NS_ERROR_NULL_POINTER;
              childNodes->GetLength(&count);
              // if there are siblings to the right, split parent at offsetInParent+1
              if ((PRInt32)count!=offsetInParent+1)
              {
                nsCOMPtr<nsIDOMNode>newRightNode;
                //nsCOMPtr<nsIDOMNode>temp;
                if (gNoisy) { printf("* splitting parent %p at offset %d for right side\n", parent.get(), offsetInParent+1);}
                result = nsEditor::SplitNode(parent, offsetInParent+1, getter_AddRefs(temp));
                if (NS_SUCCEEDED(result))
                {
                  newRightNode = do_QueryInterface(parent);
                  parent = do_QueryInterface(temp);
                  if (gNoisy) { printf("* split created right node %p sibling of parent %p\n", newRightNode.get(), parent.get());}
                  if (gNoisy) {DebugDumpContent(); } // DEBUG
                }
              }
              if (NS_SUCCEEDED(result) && 0!=offsetInParent) {
                if (gNoisy) { printf("* splitting parent %p at offset %d for left side\n", parent.get(), offsetInParent);}
                result = nsEditor::SplitNode(parent, offsetInParent, getter_AddRefs(newLeftNode));
                if (gNoisy) { printf("* split created left node %p sibling of parent %p\n", newLeftNode.get(), parent.get());}
                if (gNoisy) {DebugDumpContent(); } // DEBUG
              }
              if (NS_SUCCEEDED(result))
              { // promote the selection to the grandparent
                // first, determine the child's position in it's parent
                PRInt32 childPositionInParent;
                GetChildOffset(newMiddleNode, parent, childPositionInParent);
                // compare childPositionInParent to the number of children in parent
                //PRUint32 count=0;
                //nsCOMPtr<nsIDOMNodeList>childNodes;
                result = parent->GetChildNodes(getter_AddRefs(childNodes));
                if (NS_SUCCEEDED(result) && childNodes) {
                  childNodes->GetLength(&count);
                }
                PRBool insertAfter = PR_FALSE;
                // if they're equal, we'll insert newMiddleNode in grandParent after the parent
                if ((PRInt32)count==childPositionInParent) {
                  insertAfter = PR_TRUE;
                }
                // now that we know where to put newMiddleNode, do it.
                nsCOMPtr<nsIDOMNode>grandParent;
                result = parent->GetParentNode(getter_AddRefs(grandParent));
                if (NS_FAILED(result)) return result;
                if (!grandParent) return NS_ERROR_NULL_POINTER;

                if (gNoisy) { printf("* deleting middle node %p\n", newMiddleNode.get());}
                result = nsEditor::DeleteNode(newMiddleNode);
                if (gNoisy) {DebugDumpContent(); } // DEBUG
                if (NS_SUCCEEDED(result))
                {
                  PRInt32 position;
                  result = GetChildOffset(parent, grandParent, position);
                  if (NS_SUCCEEDED(result)) 
                  {
                    if (PR_TRUE==insertAfter)
                    {
                      if (gNoisy) {printf("insertAfter=PR_TRUE, incr. position\n"); }
                      position++;
                    }
                    if (gNoisy) { 
                      printf("* inserting node %p in grandparent %p at offset %d\n", 
                             newMiddleNode.get(), grandParent.get(), position);
                    }
                    result = nsEditor::InsertNode(newMiddleNode, grandParent, position);
                    if (gNoisy) {DebugDumpContent(); } // DEBUG
                    if (NS_SUCCEEDED(result)) 
                    {
                      PRBool hasChildren=PR_TRUE;
                      parent->HasChildNodes(&hasChildren);
                      if (PR_FALSE==hasChildren) {
                        if (gNoisy) { printf("* deleting empty style node %p\n", parent.get());}
                        result = nsEditor::DeleteNode(parent);
                        if (gNoisy) {DebugDumpContent(); } // DEBUG
                      }
                    }
                  }
                }
              }
            }
            break;
          }
        }
      }
    }
  }
  return result;
}

/* this should only get called if the only intervening nodes are inline style nodes */
NS_IMETHODIMP 
nsTextEditor::RemoveTextPropertiesForNodesWithSameParent(nsIDOMNode *aStartNode,
                                                         PRInt32     aStartOffset,
                                                         nsIDOMNode *aEndNode,
                                                         PRInt32     aEndOffset,
                                                         nsIDOMNode *aParent,
                                                         nsIAtom    *aPropName,
                                                         const nsString *aAttribute)
{
  if (gNoisy) { printf("nsTextEditor::RemoveTextPropertiesForNodesWithSameParent\n"); }
  nsresult result=NS_OK;
  PRInt32 startOffset = aStartOffset;
  PRInt32 endOffset;
  nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
  nsCOMPtr<nsIDOMNode>parentNode = do_QueryInterface(aParent);
  
  // remove aPropName from all intermediate nodes
  nsCOMPtr<nsIDOMNode>siblingNode;
  nsCOMPtr<nsIDOMNode>nextSiblingNode;  // temp to hold the next node in the list
  result = aStartNode->GetNextSibling(getter_AddRefs(siblingNode));
  if (NS_FAILED(result)) return result;
  while (siblingNode)
  {
    // get next sibling right away, before we move siblingNode!
    siblingNode->GetNextSibling(getter_AddRefs(nextSiblingNode));
    if (aEndNode==siblingNode.get()) {  // found the end node, handle that below
      break;
    }
    else
    { // found a sibling node between aStartNode and aEndNode, remove the style node
      PRUint32 childCount=0;
      nodeAsChar =  do_QueryInterface(siblingNode);
      if (nodeAsChar) {
        nodeAsChar->GetLength(&childCount);
      }
      else
      {
        nsCOMPtr<nsIDOMNodeList>grandChildNodes;
        result = siblingNode->GetChildNodes(getter_AddRefs(grandChildNodes));
        if (NS_SUCCEEDED(result) && grandChildNodes) {
          grandChildNodes->GetLength(&childCount);
        }
        if (0==childCount)
        { // node has no children
          // XXX: for now, I think that's ok.  just pass in 0
        }
      }
      if (NS_FAILED(result)) return result;
      siblingNode->GetParentNode(getter_AddRefs(parentNode));
      result = RemoveTextPropertiesForNode(siblingNode, parentNode, 0, childCount, aPropName, aAttribute);
      if (NS_FAILED(result)) return result;
    }
    siblingNode = do_QueryInterface(nextSiblingNode);    
  }
  // remove aPropName from aStartNode
  //nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
  nodeAsChar =  do_QueryInterface(aStartNode);
  if (nodeAsChar) {
    nodeAsChar->GetLength((PRUint32 *)&endOffset);
  }
  else
  {
    if (gNoisy) { printf("not yet supported\n");}
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  result = aStartNode->GetParentNode(getter_AddRefs(parentNode));
  if (NS_SUCCEEDED(result)) {
    result = RemoveTextPropertiesForNode(aStartNode, parentNode, startOffset, endOffset, aPropName, aAttribute);
  }
  if (NS_FAILED(result)) return result;
  // remove aPropName from the end node
  startOffset = 0;
  endOffset = aEndOffset;
  result = aEndNode->GetParentNode(getter_AddRefs(parentNode));
  if (NS_SUCCEEDED(result)) {
    result = RemoveTextPropertiesForNode(aEndNode, parentNode, startOffset, endOffset, aPropName, aAttribute);
  }
  return result;
}

NS_IMETHODIMP
nsTextEditor::RemoveTextPropertiesForNodeWithDifferentParents(nsIDOMNode  *aStartNode,
                                                              PRInt32      aStartOffset,
                                                              nsIDOMNode  *aEndNode,
                                                              PRInt32      aEndOffset,
                                                              nsIDOMNode  *aParent,
                                                              nsIAtom     *aPropName,
                                                              const nsString *aAttribute)
{
  if (gNoisy) { printf("start nsTextEditor::RemoveTextPropertiesForNodeWithDifferentParents\n"); }
  nsresult result=NS_OK;
  if (!aStartNode || !aEndNode || !aParent || !aPropName)
    return NS_ERROR_NULL_POINTER;

  PRInt32 rangeStartOffset = aStartOffset;  // used to construct a range for the nodes between
  PRInt32 rangeEndOffset = aEndOffset;      // aStartNode and aEndNode after we've processed those endpoints

  // temporary state variables
  PRBool textPropertySet;
  nsCOMPtr<nsIDOMNode>resultNode;

  // delete the style node for the text in the start parent
  nsCOMPtr<nsIDOMNode>startNode = do_QueryInterface(aStartNode);  // use computed endpoint based on end points passed in
  nsCOMPtr<nsIDOMNode>endNode = do_QueryInterface(aEndNode);      // use computed endpoint based on end points passed in
  nsCOMPtr<nsIDOMNode>parent;                                     // the parent of the node we're operating on
  PRBool skippedStartNode = PR_FALSE;                             // we skip the start node if aProp is not set on it
  PRUint32 count;
  nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
  nodeAsChar = do_QueryInterface(startNode);
  if (nodeAsChar) 
  {
    result = aStartNode->GetParentNode(getter_AddRefs(parent));
    if (NS_FAILED(result)) { return result; }
    nodeAsChar->GetLength(&count);
    if ((PRUint32)aStartOffset!=count) 
    {  // only do this if at least one child is selected
      IsTextPropertySetByContent(aStartNode, aPropName, aAttribute, nsnull, textPropertySet, getter_AddRefs(resultNode));
      if (PR_TRUE==textPropertySet)
      {
        result = RemoveTextPropertiesForNode(aStartNode, parent, aStartOffset, count, aPropName, aAttribute);
        if (0!=aStartOffset) {
          rangeStartOffset = 0; // we split aStartNode at aStartOffset and it is the right node now
        }
      }
      else 
      {
        skippedStartNode = PR_TRUE;
        if (gNoisy) { printf("skipping start node because property not set\n"); }
      }
    }
    else 
    { 
      skippedStartNode = PR_TRUE;
      if (gNoisy) { printf("skipping start node because aStartOffset==count\n"); } 
    }
  }
  else
  {
    startNode = GetChildAt(aStartNode, aStartOffset);
    parent = do_QueryInterface(aStartNode);
    if (!startNode || !parent) {return NS_ERROR_NULL_POINTER;}
    IsTextPropertySetByContent(startNode, aPropName, aAttribute, nsnull, textPropertySet, getter_AddRefs(resultNode));
    if (PR_TRUE==textPropertySet)
    {
      result = RemoveTextPropertiesForNode(startNode, parent, aStartOffset, aStartOffset+1, aPropName, aAttribute);
    }
    else
    {
      skippedStartNode = PR_TRUE;
      if (gNoisy) { printf("skipping start node because property not set\n"); }
    }
  }

  // delete the style node for the text in the end parent
  if (NS_SUCCEEDED(result))
  {
    nodeAsChar = do_QueryInterface(endNode);
    if (nodeAsChar)
    {
      result = endNode->GetParentNode(getter_AddRefs(parent));
      if (NS_FAILED(result)) { return result; }
      nodeAsChar->GetLength(&count);
      if (aEndOffset!=0) 
      {  // only do this if at least one child is selected
        IsTextPropertySetByContent(endNode, aPropName, aAttribute, nsnull, textPropertySet, getter_AddRefs(resultNode));
        if (PR_TRUE==textPropertySet)
        {
          result = RemoveTextPropertiesForNode(endNode, parent, 0, aEndOffset, aPropName, aAttribute);
          if (0!=aEndOffset) {
            rangeEndOffset = 0; // we split endNode at aEndOffset and it is the right node now
          }
        }
        else { if (gNoisy) { printf("skipping end node because aProperty not set.\n"); } }
      }
      else { if (gNoisy) { printf("skipping end node because aEndOffset==0\n"); } }
    }
    else
    {
      endNode = GetChildAt(aEndNode, aEndOffset);
      parent = do_QueryInterface(aEndNode);
      if (!endNode || !parent) {return NS_ERROR_NULL_POINTER;}
      result = RemoveTextPropertiesForNode(endNode, parent, aEndOffset, aEndOffset+1, aPropName, aAttribute);
    }
  }

  // remove aPropName style nodes for all the content between the start and end nodes
  if (NS_SUCCEEDED(result))
  {
    // build our own range now, because the endpoints may have shifted during shipping
    nsCOMPtr<nsIDOMRange> range;
    result = nsComponentManager::CreateInstance(kCRangeCID, 
                                                nsnull, 
                                                nsIDOMRange::GetIID(), 
                                                getter_AddRefs(range));
    if (NS_FAILED(result)) { return result; }
    if (!range) { return NS_ERROR_NULL_POINTER; }
    // compute the start node
    if (PR_TRUE==skippedStartNode) 
    {
      nsCOMPtr<nsIDOMNode>tempNode = do_QueryInterface(startNode);
      nsEditor::GetNextNode(startNode, PR_TRUE, getter_AddRefs(tempNode));
      startNode = do_QueryInterface(tempNode);
    }
    range->SetStart(startNode, rangeStartOffset);
    range->SetEnd(endNode, rangeEndOffset);
    if (gNoisy) 
    { 
      printf("created range [(%p,%d), (%p,%d)]\n", 
              startNode.get(), rangeStartOffset,
              endNode.get(), rangeEndOffset);
    }

    nsVoidArray nodeList;
    nsCOMPtr<nsIContentIterator>iter;
    result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                                nsIContentIterator::GetIID(), getter_AddRefs(iter));
    if (NS_FAILED(result)) return result;
    if (!iter) return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIContent>startContent;
    startContent = do_QueryInterface(startNode);
    nsCOMPtr<nsIContent>endContent;
    endContent = do_QueryInterface(endNode);
    if (startContent && endContent)
    {
      iter->Init(range);
      nsCOMPtr<nsIContent> content;
      iter->CurrentNode(getter_AddRefs(content));
      nsAutoString propName;  // the property we are removing
      aPropName->ToString(propName);
      while (NS_COMFALSE == iter->IsDone())
      {
        if ((content.get() != startContent.get()) &&
            (content.get() != endContent.get()))
        {
          nsCOMPtr<nsIDOMElement>element;
          element = do_QueryInterface(content);
          if (element)
          {
            nsString tag;
            element->GetTagName(tag);
            if (propName.EqualsIgnoreCase(tag))
            {
              if (-1==nodeList.IndexOf(content.get())) {
                nodeList.AppendElement((void *)(content.get()));
              }
            }
          }
        }
        // note we don't check the result, we just rely on iter->IsDone
        iter->Next();
        iter->CurrentNode(getter_AddRefs(content));
      }
    }

    // now delete all the style nodes we found
    nsIContent *contentPtr;
    contentPtr = (nsIContent*)(nodeList.ElementAt(0));
    while (contentPtr)
    {
      nsCOMPtr<nsIDOMNode>styleNode;
      styleNode = do_QueryInterface(contentPtr);
      // promote the children of styleNode
      nsCOMPtr<nsIDOMNode>parentNode;
      result = styleNode->GetParentNode(getter_AddRefs(parentNode));
      if (NS_FAILED(result)) return result;
      if (!parentNode) return NS_ERROR_NULL_POINTER;

      PRInt32 position;
      result = GetChildOffset(styleNode, parentNode, position);
      if (NS_FAILED(result)) return result;

      nsCOMPtr<nsIDOMNode>previousSiblingNode;
      nsCOMPtr<nsIDOMNode>childNode;
      result = styleNode->GetLastChild(getter_AddRefs(childNode));
      if (NS_FAILED(result)) return result;
      while (childNode)
      {
        childNode->GetPreviousSibling(getter_AddRefs(previousSiblingNode));
        // explicitly delete of childNode from styleNode
        // can't just rely on DOM semantics of InsertNode doing the delete implicitly, doesn't undo! 
        result = nsEditor::DeleteNode(childNode); 
        if (NS_FAILED(result)) return result;
        result = nsEditor::InsertNode(childNode, parentNode, position);
        if (gNoisy) 
        {
          printf("deleted next sibling node %p\n", childNode.get());
          DebugDumpContent(); // DEBUG
        }
        childNode = do_QueryInterface(previousSiblingNode);        
      } // end while loop 
      // delete styleNode
      result = nsEditor::DeleteNode(styleNode);
      if (NS_FAILED(result)) return result;
      if (gNoisy) 
      {
        printf("deleted style node %p\n", styleNode.get());
        DebugDumpContent(); // DEBUG
      }

      // get next content ptr
      nodeList.RemoveElementAt(0);
      contentPtr = (nsIContent*)(nodeList.ElementAt(0));
    }
    nsCOMPtr<nsIDOMSelection>selection;
    result = nsEditor::GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(result)) return result;
    if (!selection) return NS_ERROR_NULL_POINTER;
    selection->Collapse(startNode, rangeStartOffset);
    selection->Extend(endNode, rangeEndOffset);
    if (gNoisy) { printf("RTPFNWDP set selection.\n"); }
  }
  if (gNoisy) 
  { 
    printf("end nsTextEditor::RemoveTextPropertiesForNodeWithDifferentParents, dumping content...\n"); 
    DebugDumpContent();
  }

  return result;
}

TypeInState * nsTextEditor::GetTypeInState()
{
  if (mTypeInState) {
    NS_ADDREF(mTypeInState);
  }
  return mTypeInState;
}

NS_IMETHODIMP
nsTextEditor::SetTypeInStateForProperty(TypeInState    &aTypeInState, 
                                        nsIAtom        *aPropName, 
                                        const nsString *aAttribute,
                                        const nsString *aValue)
{
  if (!aPropName) {
    return NS_ERROR_NULL_POINTER;
  }
  PRUint32 propEnum;
  aTypeInState.GetEnumForName(aPropName, propEnum);
  if (nsIEditProperty::b==aPropName || nsIEditProperty::i==aPropName || nsIEditProperty::u==aPropName) 
  {
    if (PR_TRUE==aTypeInState.IsSet(propEnum))
    { // toggle currently set boldness
      aTypeInState.UnSet(propEnum);
    }
    else
    { // get the current style and set boldness to the opposite of the current state
      PRBool any = PR_FALSE;
      PRBool all = PR_FALSE;
      PRBool first = PR_FALSE;
      GetTextProperty(aPropName, aAttribute, nsnull, first, any, all); // operates on current selection
      aTypeInState.SetProp(propEnum, !any);
    }
  }
  else if (nsIEditProperty::font==aPropName)
  {
    if (!aAttribute) { return NS_ERROR_NULL_POINTER; }
    nsIAtom *attribute = NS_NewAtom(*aAttribute);
    if (!attribute) { return NS_ERROR_NULL_POINTER; }
    PRUint32 attrEnum;
    aTypeInState.GetEnumForName(attribute, attrEnum);
    if (nsIEditProperty::color==attribute || nsIEditProperty::face==attribute || nsIEditProperty::size==attribute)
    {
      if (PR_TRUE==aTypeInState.IsSet(attrEnum))
      { 
        if (nsnull==aValue) {
          aTypeInState.UnSet(attrEnum);
        }
        else { // we're just changing the value of color
          aTypeInState.SetPropValue(attrEnum, *aValue);
        }
      }
      else
      { // get the current style and set font color if it's needed
        if (!aValue) { return NS_ERROR_NULL_POINTER; }
        PRBool any = PR_FALSE;
        PRBool all = PR_FALSE;
        PRBool first = PR_FALSE;
        GetTextProperty(aPropName, aAttribute, aValue, first, any, all); // operates on current selection
        if (PR_FALSE==all) {
          aTypeInState.SetPropValue(attrEnum, *aValue);
        }
      }    
    }
    else { return NS_ERROR_FAILURE; }
  }
  else { return NS_ERROR_FAILURE; }
  return NS_OK;
}

NS_IMETHODIMP nsTextEditor::SetBackgroundColor(const nsString& aColor)
{
// This is done in nsHTMLEditor::SetBackgroundColor should we do it here?
#ifdef ENABLE_JS_EDITOR_LOG
  nsAutoJSEditorLogLock logLock(mJSEditorLog);

  if (mJSEditorLog)
    mJSEditorLog->SetBackgroundColor(aColor);
#endif // ENABLE_JS_EDITOR_LOG

  nsresult res;
  NS_ASSERTION(mDoc, "Missing Editor DOM Document");
  
  // Set the background color attribute on the body tag
  nsCOMPtr<nsIDOMElement> bodyElement;
  res = nsEditor::GetBodyElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  if (!bodyElement) return NS_ERROR_NULL_POINTER;

  nsAutoEditBatch beginBatching(this);
  bodyElement->SetAttribute("bgcolor", aColor);
  return res;
}

// This file should be rearranged to put all methods that simply call nsEditor together
NS_IMETHODIMP
nsTextEditor::CopyAttributes(nsIDOMNode *aDestNode, nsIDOMNode *aSourceNode)
{
  return nsEditor::CopyAttributes(aDestNode, aSourceNode);
}

NS_IMETHODIMP
nsTextEditor::BeginComposition(void)
{
	return nsEditor::BeginComposition();
}

NS_IMETHODIMP
nsTextEditor::SetCompositionString(const nsString& aCompositionString,nsIPrivateTextRangeList* aTextRangeList,nsTextEventReply* aReply)
{
	return nsEditor::SetCompositionString(aCompositionString,aTextRangeList,aReply);
}

NS_IMETHODIMP
nsTextEditor::EndComposition(void)
{
	return nsEditor::EndComposition();
}
NS_IMETHODIMP
nsTextEditor::DebugUnitTests(PRInt32 *outNumTests, PRInt32 *outNumTestsFailed)
{
#ifdef DEBUG
  if (!outNumTests || !outNumTestsFailed)
    return NS_ERROR_NULL_POINTER;

	TextEditorTest *tester = new TextEditorTest();
	if (!tester)
	  return NS_ERROR_OUT_OF_MEMORY;
	 
  tester->Run(this, outNumTests, outNumTestsFailed);
  delete tester;
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsTextEditor::StartLogging(nsIFileSpec *aLogFile)
{
  return nsEditor::StartLogging(aLogFile);
}

NS_IMETHODIMP
nsTextEditor::StopLogging()
{
  return nsEditor::StopLogging();
}


NS_IMETHODIMP
nsTextEditor::GetDocumentLength(PRInt32 *aCount)                                              
{
  if (!aCount) { return NS_ERROR_NULL_POINTER; }
  nsresult result;
  // initialize out params
  *aCount = 0;
  
  nsCOMPtr<nsIDOMSelection> sel;
  result = GetSelection(getter_AddRefs(sel));
  if ((NS_SUCCEEDED(result)) && sel)
  {
    nsAutoSelectionReset selectionResetter(sel);
    result = SelectAll();
    if (NS_SUCCEEDED(result))
    {
      PRInt32 start, end;
      result = GetTextSelectionOffsets(sel, start, end);
      if (NS_SUCCEEDED(result))
      {
        NS_ASSERTION(0==start, "GetTextSelectionOffsets failed to set start correctly.");
        NS_ASSERTION(0<=end, "GetTextSelectionOffsets failed to set end correctly.");
        if (0<=end) {
          *aCount = end;
        }
      }
    }
  }
  return result;
}

// this is a complete ripoff from nsTextEditor::GetTextSelectionOffsetsForRange
// the two should use common code, or even just be one method
nsresult nsTextEditor::GetTextSelectionOffsets(nsIDOMSelection *aSelection,
                                               PRInt32 &aStartOffset, 
                                               PRInt32 &aEndOffset)
{
  nsresult result;
  // initialize out params
  aStartOffset = 0; // default to first char in selection
  aEndOffset = -1;  // default to total length of text in selection

  nsCOMPtr<nsIDOMNode> startNode, endNode, parentNode;
  PRInt32 startOffset, endOffset;
  aSelection->GetAnchorNode(getter_AddRefs(startNode));
  aSelection->GetAnchorOffset(&startOffset);
  aSelection->GetFocusNode(getter_AddRefs(endNode));
  aSelection->GetFocusOffset(&endOffset);

  nsCOMPtr<nsIEnumerator> enumerator;
  result = aSelection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_SUCCEEDED(result) && enumerator)
  {
    // don't use "result" in this block
    enumerator->First(); 
    nsCOMPtr<nsISupports> currentItem;
    nsresult findParentResult = enumerator->CurrentItem(getter_AddRefs(currentItem));
    if ((NS_SUCCEEDED(findParentResult)) && (currentItem))
    {
      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
      range->GetCommonParent(getter_AddRefs(parentNode));
    }
    else parentNode = do_QueryInterface(startNode);
  }

  nsCOMPtr<nsIContentIterator> iter;
  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                              nsIContentIterator::GetIID(), 
                                              getter_AddRefs(iter));
  if ((NS_SUCCEEDED(result)) && iter)
  {
    PRUint32 totalLength=0;
    nsCOMPtr<nsIDOMCharacterData>textNode;
    nsCOMPtr<nsIContent>blockParentContent = do_QueryInterface(parentNode);
    iter->Init(blockParentContent);
    // loop through the content iterator for each content node
    nsCOMPtr<nsIContent> content;
    result = iter->CurrentNode(getter_AddRefs(content));
    while (NS_COMFALSE == iter->IsDone())
    {
      textNode = do_QueryInterface(content);
      if (textNode)
      {
        nsCOMPtr<nsIDOMNode>currentNode = do_QueryInterface(textNode);
        if (!currentNode) {return NS_ERROR_NO_INTERFACE;}
        if (PR_TRUE==IsEditable(currentNode))
        {
          if (currentNode.get() == startNode.get())
          {
            aStartOffset = totalLength + startOffset;
          }
          if (currentNode.get() == endNode.get())
          {
            aEndOffset = totalLength + endOffset;
            break;
          }
          PRUint32 length;
          textNode->GetLength(&length);
          totalLength += length;
        }
      }
      iter->Next();
      iter->CurrentNode(getter_AddRefs(content));
    }
    if (-1==aEndOffset) {
      aEndOffset = totalLength;
    }
  }
  return result;
}






