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
#include "nsEditProperty.h"  // temporary, to get html atoms

#include "nsIStreamListener.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsIDocument.h"
#include "nsIHTMLContentSink.h"
#include "nsHTMLContentSinkStream.h"
#include "nsHTMLToTXTSinkStream.h"
#include "nsXIFDTD.h"


#include "nsIDOMDocument.h"
#include "nsIDOMEventReceiver.h" 
#include "nsIDOMKeyListener.h" 
#include "nsIDOMMouseListener.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMSelection.h"
#include "nsIDOMRange.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMElement.h"
#include "nsIDOMTextListener.h"
#include "nsEditorCID.h"
#include "nsISupportsArray.h"
#include "nsIEnumerator.h"
#include "nsIContentIterator.h"
#include "nsIContent.h"
#include "nsLayoutCID.h"
#include "nsIPresShell.h"
#include "nsIStyleContext.h"

// transactions the text editor knows how to build itself
#include "TransactionFactory.h"
#include "PlaceholderTxn.h"
#include "InsertTextTxn.h"


class nsIFrame;

#ifdef XP_UNIX
#include <strstream.h>
#endif

#ifdef XP_MAC
#include <sstream>
#include <string>
#endif

#ifdef XP_PC
#include <strstrea.h>
#endif

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsTextEditRules.h"


static NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID,   NS_IDOMKEYLISTENER_IID);
static NS_DEFINE_IID(kIDOMTextListenerIID,  NS_IDOMTEXTLISTENER_IID);
static NS_DEFINE_IID(kIDOMDragListenerIID,  NS_IDOMDRAGLISTENER_IID);
static NS_DEFINE_IID(kIDOMSelectionListenerIID, NS_IDOMSELECTIONLISTENER_IID);

static NS_DEFINE_IID(kIEditPropertyIID, NS_IEDITPROPERTY_IID);
static NS_DEFINE_CID(kEditorCID,        NS_EDITOR_CID);
static NS_DEFINE_IID(kIEditorIID,       NS_IEDITOR_IID);
static NS_DEFINE_IID(kISupportsIID,     NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITextEditorIID,   NS_ITEXTEDITOR_IID);
static NS_DEFINE_CID(kTextEditorCID,    NS_TEXTEDITOR_CID);

static NS_DEFINE_IID(kIContentIteratorIID,   NS_ICONTENTITERTOR_IID);
static NS_DEFINE_CID(kCContentIteratorCID,   NS_CONTENTITERATOR_CID);

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
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMSelectionListenerIID)) {
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


/* ---------- nsTextEditor implementation ---------- */

nsTextEditor::nsTextEditor()
{
// Done in nsEditor
//  NS_INIT_REFCNT();
  mRules = nsnull;
  nsEditProperty::InstanceInit();
}

nsTextEditor::~nsTextEditor()
{
  //the autopointers will clear themselves up. 
  //but we need to also remove the listeners or we have a leak
  nsCOMPtr<nsIDOMDocument> doc;
  nsEditor::GetDocument(getter_AddRefs(doc));
  if (doc)
  {
    nsCOMPtr<nsIDOMEventReceiver> erP;
    nsresult result = doc->QueryInterface(kIDOMEventReceiverIID, getter_AddRefs(erP));
    if (NS_SUCCEEDED(result) && erP) 
    {
      if (mKeyListenerP) {
        erP->RemoveEventListenerByIID(mKeyListenerP, kIDOMKeyListenerIID);
      }
      if (mMouseListenerP) {
        erP->RemoveEventListenerByIID(mMouseListenerP, kIDOMMouseListenerIID);
      }

	  if (mTextListenerP) {
		  erP->RemoveEventListenerByIID(mTextListenerP, kIDOMTextListenerIID);
	  }

      if (mDragListenerP) {
        erP->RemoveEventListenerByIID(mDragListenerP, kIDOMDragListenerIID);
      }

    }
    else
      NS_NOTREACHED("~nsTextEditor");
  }
  if (mRules) {
    delete mRules;
  }
  nsEditProperty::InstanceShutdown();
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

NS_IMETHODIMP nsTextEditor::Init(nsIDOMDocument *aDoc, nsIPresShell *aPresShell)
{
  NS_PRECONDITION(nsnull!=aDoc && nsnull!=aPresShell, "bad arg");
  nsresult result=NS_ERROR_NULL_POINTER;
  if ((nsnull!=aDoc) && (nsnull!=aPresShell))
  {
    // Init the base editor
    result = nsEditor::Init(aDoc, aPresShell);
    if (NS_OK != result) { return result; }

    nsCOMPtr<nsIDOMSelection>selection;
    result = nsEditor::GetSelection(getter_AddRefs(selection));
    if (NS_OK != result) { return result; }
    if (selection) {
      nsCOMPtr<nsIDOMSelectionListener>listener;
      listener = do_QueryInterface(&mTypeInState);
      if (listener) {
        selection->AddSelectionListener(listener); 
      }
    }

    // Init the rules system
    InitRules();

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

	result = NS_NewEditorTextListener(getter_AddRefs(mTextListenerP),this);
	if (NS_OK !=result) {
		// drop the key and mouse listeners
#ifdef DEBUG_TAGUE
	printf("nsTextEditor.cpp: failed to get TextEvent Listener\n");
#endif
		mMouseListenerP = do_QueryInterface(0);
		mKeyListenerP = do_QueryInterface(0);
		return result;
	}

    result = NS_NewEditorDragListener(getter_AddRefs(mDragListenerP), this);
    if (NS_OK != result) {
      //return result;
		mMouseListenerP = do_QueryInterface(0);
		mKeyListenerP = do_QueryInterface(0);
		mTextListenerP = do_QueryInterface(0);
    }

    nsCOMPtr<nsIDOMEventReceiver> erP;
    result = aDoc->QueryInterface(kIDOMEventReceiverIID, getter_AddRefs(erP));
    if (NS_OK != result) 
    {
      mKeyListenerP = do_QueryInterface(0);
      mMouseListenerP = do_QueryInterface(0); //dont need these if we cant register them
	  mTextListenerP = do_QueryInterface(0);
      mDragListenerP = do_QueryInterface(0); //dont need these if we cant register them
      return result;
    }
    //cmanske: Shouldn't we check result from this?
    erP->AddEventListenerByIID(mKeyListenerP, kIDOMKeyListenerIID);
    //erP->AddEventListener(mDragListenerP, kIDOMDragListenerIID);
    //erP->AddEventListener(mMouseListenerP, kIDOMMouseListenerIID);
	
	erP->AddEventListenerByIID(mTextListenerP,kIDOMTextListenerIID);

    result = NS_OK;

		EnableUndo(PR_TRUE);
  }
  return result;
}

void  nsTextEditor::InitRules()
{
// instantiate the rules for this text editor
// XXX: we should be told which set of rules to instantiate
  mRules =  new nsTextEditRules();
  mRules->Init(this);
}


NS_IMETHODIMP nsTextEditor::SetTextProperty(nsIAtom *aProperty)
{
  if (!aProperty)
    return NS_ERROR_NULL_POINTER;

  nsresult result=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMSelection>selection;
  result = nsEditor::GetSelection(getter_AddRefs(selection));
  if ((NS_SUCCEEDED(result)) && selection)
  {
    PRBool isCollapsed;
    selection->IsCollapsed(&isCollapsed);
    if (PR_TRUE==isCollapsed)
    {
      // manipulating text attributes on a collapsed selection only sets state for the next text insertion
      SetTypeInStateForProperty(mTypeInState, aProperty);
    }
    else
    {
      nsEditor::BeginTransaction();
      nsCOMPtr<nsIEnumerator> enumerator;
      enumerator = do_QueryInterface(selection);
      if (enumerator)
      {
        enumerator->First(); 
        nsISupports *currentItem;
        result = enumerator->CurrentItem(&currentItem);
        if ((NS_SUCCEEDED(result)) && (nsnull!=currentItem))
        {
          nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
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
              // commonParent==aStartParent, so get the "real" parent of the selection
              startParent->GetParentNode(getter_AddRefs(commonParent));
              result = SetTextPropertiesForNode(startParent, commonParent, 
                                                startOffset, endOffset,
                                                aProperty);
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
                if (endGrandParent.get()==startGrandParent.get())
                {
                  result = IntermediateNodesAreInline(range, startParent, startOffset, 
                                                      endParent, endOffset, 
                                                      startGrandParent, canCollapseStyleNode);
                }
                if (NS_SUCCEEDED(result)) 
                {
                  if (PR_TRUE==canCollapseStyleNode)
                  { // the range is between 2 nodes that have a common (immediate) grandparent,
                    // and any intermediate nodes are just inline style nodes
                    result = SetTextPropertiesForNodesWithSameParent(startParent,startOffset,
                                                                     endParent,  endOffset,
                                                                     commonParent,
                                                                     aProperty);
                  }
                  else
                  { // the range is between 2 nodes that have no simple relationship
                    result = SetTextPropertiesForNodeWithDifferentParents(range,
                                                                          startParent,startOffset, 
                                                                          endParent,  endOffset,
                                                                          commonParent,
                                                                          aProperty);
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
          }
        }
      }
      nsEditor::EndTransaction();
    }
    if (NS_SUCCEEDED(result))
    { // set the selection
      // XXX: can't do anything until I can create ranges
    }
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::GetTextProperty(nsIAtom *aProperty, PRBool &aFirst, PRBool &aAny, PRBool &aAll)
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
  if ((NS_SUCCEEDED(result)) && selection)
  {
    nsCOMPtr<nsIEnumerator> enumerator;
    enumerator = do_QueryInterface(selection);
    if (enumerator)
    {
      enumerator->First(); 
      nsISupports *currentItem;
      result = enumerator->CurrentItem(&currentItem);
      if ((NS_SUCCEEDED(result)) && currentItem)
      {
        nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
        nsCOMPtr<nsIContentIterator> iter;
        result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                                    kIContentIteratorIID, 
                                                    getter_AddRefs(iter));
        if ((NS_SUCCEEDED(result)) && iter)
        {
          iter->Init(range);
          // loop through the content iterator for each content node
          // for each text node:
          // get the frame for the content, and from it the style context
          // ask the style context about the property
          nsCOMPtr<nsIContent> content;
          result = iter->CurrentNode(getter_AddRefs(content));
          while (NS_COMFALSE == iter->IsDone())
          {
            nsCOMPtr<nsIDOMCharacterData>text;
            text = do_QueryInterface(content);
            if (text)
            {
              nsCOMPtr<nsIDOMNode>node;
              node = do_QueryInterface(content);
              if (node)
              {
                PRBool isSet;
                IsTextPropertySetByContent(node, aProperty, isSet);
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
            result = iter->CurrentNode(getter_AddRefs(content));
          }
        }
      }
    }
  }
  return result;
}

void nsTextEditor::IsTextStyleSet(nsIStyleContext *aSC, 
                                  nsIAtom *aProperty, 
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

void nsTextEditor::IsTextPropertySetByContent(nsIDOMNode *aNode,
                                              nsIAtom    *aProperty, 
                                              PRBool     &aIsSet) const
{
  nsresult result;
  aIsSet = PR_FALSE;
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
      if (propName.Equals(tag))
      {
        aIsSet = PR_TRUE;
        break;
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


NS_IMETHODIMP nsTextEditor::RemoveTextProperty(nsIAtom *aProperty)
{
  if (!aProperty)
    return NS_ERROR_NULL_POINTER;

  nsresult result=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMSelection>selection;
  result = nsEditor::GetSelection(getter_AddRefs(selection));
  if ((NS_SUCCEEDED(result)) && selection)
  {
    PRBool isCollapsed;
    selection->IsCollapsed(&isCollapsed);
    if (PR_TRUE==isCollapsed)
    {
      // manipulating text attributes on a collapsed selection only sets state for the next text insertion
      SetTypeInStateForProperty(mTypeInState, aProperty);
    }
    else
    {
      nsEditor::BeginTransaction();
      nsCOMPtr<nsIEnumerator> enumerator;
      enumerator = do_QueryInterface(selection);
      if (enumerator)
      {
        enumerator->First(); 
        nsISupports *currentItem;
        result = enumerator->CurrentItem(&currentItem);
        if ((NS_SUCCEEDED(result)) && (nsnull!=currentItem))
        {
          nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
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
              // commonParent==aStartParent, so get the "real" parent of the selection
              startParent->GetParentNode(getter_AddRefs(commonParent));
              result = RemoveTextPropertiesForNode(startParent, commonParent, 
                                                   startOffset, endOffset,
                                                   aProperty);
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
                if (endGrandParent.get()==startGrandParent.get())
                {
                  result = IntermediateNodesAreInline(range, startParent, startOffset, 
                                                      endParent, endOffset, 
                                                      startGrandParent, canCollapseStyleNode);
                }
                if (NS_SUCCEEDED(result)) 
                {
                  if (PR_TRUE==canCollapseStyleNode)
                  { // the range is between 2 nodes that have a common (immediate) grandparent,
                    // and any intermediate nodes are just inline style nodes
                    result = RemoveTextPropertiesForNodesWithSameParent(startParent,startOffset,
                                                                        endParent,  endOffset,
                                                                        commonParent,
                                                                        aProperty);
                  }
                  else
                  { // the range is between 2 nodes that have no simple relationship
                    result = RemoveTextPropertiesForNodeWithDifferentParents(range,
                                                                             startParent,startOffset, 
                                                                             endParent,  endOffset,
                                                                             commonParent,
                                                                             aProperty);
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
          }
        }
      }
      nsEditor::EndTransaction();
    }
    if (NS_SUCCEEDED(result))
    { // set the selection
      // XXX: can't do anything until I can create ranges
    }
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::DeleteSelection(nsIEditor::Direction aDir)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel= PR_FALSE;

  nsresult result = nsEditor::BeginTransaction();
  if (NS_FAILED(result)) { return result; }

  // pre-process
  nsEditor::GetSelection(getter_AddRefs(selection));
  result = mRules->WillDoAction(nsTextEditRules::kDeleteSelection, selection, nsnull, &cancel);
  if ((PR_FALSE==cancel) && (NS_SUCCEEDED(result)))
  {
    result = nsEditor::DeleteSelection(aDir);
    // post-process 
    result = mRules->DidDoAction(nsTextEditRules::kDeleteSelection, selection, nsnull, result);
  }

  nsresult endTxnResult = nsEditor::EndTransaction();  // don't return this result!
  NS_ASSERTION ((NS_SUCCEEDED(endTxnResult)), "bad end transaction result");


// XXXX: Horrible hack! We are doing this because
// of an error in Gecko which is not rendering the
// document after a change via the DOM - gpk 2/13/99
  // BEGIN HACK!!!
  HACKForceRedraw();
  // END HACK

  return result;
}

NS_IMETHODIMP nsTextEditor::InsertText(const nsString& aStringToInsert)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel= PR_FALSE;

  // pre-process
  nsEditor::GetSelection(getter_AddRefs(selection));
  nsAutoString stringToInsert;
  PlaceholderTxn *placeholderTxn=nsnull;
  nsresult result = mRules->WillDoAction(nsTextEditRules::kInsertText, selection, (void**)&placeholderTxn, &cancel);
  if ((PR_FALSE==cancel) && (NS_SUCCEEDED(result)))
  {
    result = nsEditor::InsertText(stringToInsert);
    // post-process 
    result = mRules->DidDoAction(nsTextEditRules::kInsertText, selection, nsnull, result);
  }
  if (placeholderTxn)
    placeholderTxn->SetAbsorb(PR_FALSE);  // this ends the merging of txns into placeholderTxn

  // BEGIN HACK!!!
  HACKForceRedraw();
  // END HACK
  return result;
}

NS_IMETHODIMP nsTextEditor::InsertBreak()
{
  // For plainttext just pass newlines through
  nsAutoString  key;
  key += '\n';
  return InsertText(key);
}


NS_IMETHODIMP nsTextEditor::EnableUndo(PRBool aEnable)
{
  return nsEditor::EnableUndo(aEnable);
}

NS_IMETHODIMP nsTextEditor::Undo(PRUint32 aCount)
{
  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel= PR_FALSE;

  // pre-process
  nsEditor::GetSelection(getter_AddRefs(selection));
  nsresult result = mRules->WillDoAction(nsTextEditRules::kUndo, selection, nsnull, &cancel);
  if ((PR_FALSE==cancel) && (NS_SUCCEEDED(result)))
  {
    result = nsEditor::Undo(aCount);
    nsEditor::GetSelection(getter_AddRefs(selection));
    result = mRules->DidDoAction(nsTextEditRules::kUndo, selection, nsnull, result);
  }
  return result;
}

NS_IMETHODIMP nsTextEditor::CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo)
{
  return nsEditor::CanUndo(aIsEnabled, aCanUndo);
}

NS_IMETHODIMP nsTextEditor::Redo(PRUint32 aCount)
{
  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel= PR_FALSE;

  // pre-process
  nsEditor::GetSelection(getter_AddRefs(selection));
  nsresult result = mRules->WillDoAction(nsTextEditRules::kRedo, selection, nsnull, &cancel);
  if ((PR_FALSE==cancel) && (NS_SUCCEEDED(result)))
  {
    result = nsEditor::Redo(aCount);
    nsEditor::GetSelection(getter_AddRefs(selection));
    result = mRules->DidDoAction(nsTextEditRules::kRedo, selection, nsnull, result);
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

NS_IMETHODIMP nsTextEditor::Insert(nsString& aInputString)
{
  printf("nsTextEditor::Insert not yet implemented\n");
  return NS_ERROR_NOT_IMPLEMENTED;
}

#ifdef XP_MAC 
static void WriteFromStringstream(stringstream& aIn, nsString& aOutputString)
{
  string      theString = aIn.str();

	aOutputString.SetLength(0);											// empty the string
	aOutputString += theString.data();
	aOutputString.SetLength(theString.length());		// make sure it's terminated
	
	/* relace LF with CR. Don't do this here, because strings passed out
	   to JavaScript need LF termination.
	PRUnichar		lineFeed = '\n';
	PRUnichar		carriageReturn = '\r';
	aOutputString.ReplaceChar(lineFeed, carriageReturn);
	*/
}
#else
static void WriteFromOstrstream(ostrstream& aIn, nsString& aOutputString)
{
  char* strData = aIn.str();		// get a copy of the buffer (unterminated)

	aOutputString.SetLength(0);		// empty the string
	aOutputString += strData;
	aOutputString.SetLength(aIn.pcount());		// terminate

  // in ostrstreams if you call the str() function
  // then you are responsible for deleting the string
  delete [] strData;
}
#endif

NS_IMETHODIMP nsTextEditor::OutputText(nsString& aOutputString)
{
#ifdef XP_MAC
  stringstream outStream;
#else
  ostrstream outStream;
#endif

  nsresult rv = NS_ERROR_FAILURE;
  nsIPresShell* shell = nsnull;
  
 	GetPresShell(&shell);
  if (nsnull != shell) {
    nsCOMPtr<nsIDocument> doc;
    shell->GetDocument(getter_AddRefs(doc));
    if (doc) {
      nsString buffer;

      doc->CreateXIF(buffer);

      nsIParser* parser;

      static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
      static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

      rv = nsComponentManager::CreateInstance(kCParserCID, 
                                                 nsnull, 
                                                 kCParserIID, 
                                                 (void **)&parser);

      if (NS_OK == rv) {
        nsIHTMLContentSink* sink = nsnull;

        rv = NS_New_HTMLToTXT_SinkStream(&sink);
     	 if (NS_OK == rv) {
	  			// what't this cast doing here, Greg?
	        ((nsHTMLContentSinkStream*)sink)->SetOutputStream(outStream);

	        if (NS_OK == rv) {
	          parser->SetContentSink(sink);
	    
	          nsIDTD* dtd = nsnull;
	          rv = NS_NewXIFDTD(&dtd);
	          if (NS_OK == rv) {
	            parser->RegisterDTD(dtd);
	            parser->Parse(buffer, 0, "text/xif",PR_FALSE,PR_TRUE);           
	          }
#ifdef XP_MAC
	          WriteFromStringstream(outStream, aOutputString);
#else
	          WriteFromOstrstream(outStream, aOutputString);
#endif

	          NS_IF_RELEASE(dtd);
	          NS_IF_RELEASE(sink);
          }
        }
        NS_RELEASE(parser);
      }
    }
    NS_RELEASE(shell);
  }
  
  return rv;
}



NS_IMETHODIMP nsTextEditor::OutputHTML(nsString& aOutputString)
{
#ifdef XP_MAC
  stringstream outStream;
#else
  ostrstream outStream;
#endif
  
  nsresult rv = NS_ERROR_FAILURE;
  nsIPresShell* shell = nsnull;
  
  GetPresShell(&shell);
  if (nsnull != shell) {
    nsCOMPtr<nsIDocument> doc;
    shell->GetDocument(getter_AddRefs(doc));
    if (doc) {
      nsString buffer;

      doc->CreateXIF(buffer);

      nsIParser* parser;

      static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
      static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

      rv = nsComponentManager::CreateInstance(kCParserCID, 
                                                 nsnull, 
                                                 kCParserIID, 
                                                 (void **)&parser);

      if (NS_OK == rv) {
        nsIHTMLContentSink* sink = nsnull;

        rv = NS_New_HTML_ContentSinkStream(&sink);
  
      	if (NS_OK == rv) {
	        ((nsHTMLContentSinkStream*)sink)->SetOutputStream(outStream);

	        if (NS_OK == rv) {
	          parser->SetContentSink(sink);
	    
	          nsIDTD* dtd = nsnull;
	          rv = NS_NewXIFDTD(&dtd);
	          if (NS_OK == rv) {
	            parser->RegisterDTD(dtd);
	            parser->Parse(buffer, 0, "text/xif",PR_FALSE,PR_TRUE);           
	          }
#ifdef XP_MAC
	          WriteFromStringstream(outStream, aOutputString);
#else
	          WriteFromOstrstream(outStream, aOutputString);
#endif
	          NS_IF_RELEASE(dtd);
	          NS_IF_RELEASE(sink);
	        }
        }
        NS_RELEASE(parser);
      }
  	}
	}
  return rv;
}


NS_IMETHODIMP nsTextEditor::SetTextPropertiesForNode(nsIDOMNode *aNode, 
                                                     nsIDOMNode *aParent,
                                                     PRInt32     aStartOffset,
                                                     PRInt32     aEndOffset,
                                                     nsIAtom    *aPropName)
{
  nsresult result=NS_OK;
  nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
  nodeAsChar =  do_QueryInterface(aNode);
  if (!nodeAsChar)
    return NS_ERROR_FAILURE;

  PRBool textPropertySet;
  IsTextPropertySetByContent(aNode, aPropName, textPropertySet);
  if (PR_FALSE==textPropertySet)
  {
    PRUint32 count;
    nodeAsChar->GetLength(&count);

    nsCOMPtr<nsIDOMNode>newTextNode;  // this will be the text node we move into the new style node
    if (aStartOffset!=0)
    {
      result = nsEditor::SplitNode(aNode, aStartOffset, getter_AddRefs(newTextNode));
    }
    if (NS_SUCCEEDED(result))
    {
      if (aEndOffset!=(PRInt32)count)
      {
        result = nsEditor::SplitNode(aNode, aEndOffset-aStartOffset, getter_AddRefs(newTextNode));
      }
      else
      {
        newTextNode = do_QueryInterface(aNode);
      }
      if (NS_SUCCEEDED(result))
      {
        nsAutoString tag;
        aPropName->ToString(tag);
        PRInt32 offsetInParent;
        result = nsIEditorSupport::GetChildOffset(aNode, aParent, offsetInParent);
        if (NS_SUCCEEDED(result))
        {
          nsCOMPtr<nsIDOMNode>newStyleNode;
          result = nsEditor::CreateNode(tag, aParent, offsetInParent, getter_AddRefs(newStyleNode));
          if (NS_SUCCEEDED(result))
          {
            result = nsEditor::DeleteNode(newTextNode);
            if (NS_SUCCEEDED(result)) {
              result = nsEditor::InsertNode(newTextNode, newStyleNode, 0);
            }
          }
        }
      }
    }
  }
  return result;
}

// XXX: WRITE ME!
// We can do this content based or style based
// content-based has the advantage of being "correct" regardless of the style sheet
// it has the disadvantage of not supporting nonHTML tags
NS_IMETHODIMP nsTextEditor::IsNodeInline(nsIDOMNode *aNode, PRBool &aIsInline) const
{
  // this is a content-based implementation
  if (!aNode) { return NS_ERROR_NULL_POINTER; }

  nsresult result;
  aIsInline = PR_FALSE;
  nsCOMPtr<nsIDOMElement>element;
  element = do_QueryInterface(aNode);
  if (element)
  {
    nsAutoString tag;
    result = element->GetTagName(tag);
    if (NS_SUCCEEDED(result))
    {
      nsIAtom *tagAtom = NS_NewAtom(tag);
      if (!tagAtom) { return NS_ERROR_NULL_POINTER; }
      if (tagAtom==nsIEditProperty::a      ||
          tagAtom==nsIEditProperty::b      ||
          tagAtom==nsIEditProperty::big    ||
          tagAtom==nsIEditProperty::font   ||
          tagAtom==nsIEditProperty::i      ||
          tagAtom==nsIEditProperty::span   ||
          tagAtom==nsIEditProperty::small  ||
          tagAtom==nsIEditProperty::strike ||
          tagAtom==nsIEditProperty::sub    ||
          tagAtom==nsIEditProperty::sup    ||
          tagAtom==nsIEditProperty::tt     ||
          tagAtom==nsIEditProperty::u        )
      {
        aIsInline = PR_TRUE;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTextEditor::IntermediateNodesAreInline(nsIDOMRange *aRange,
                                         nsIDOMNode  *aStartNode, 
                                         PRInt32      aStartOffset, 
                                         nsIDOMNode  *aEndNode,
                                         PRInt32      aEndOffset,
                                         nsIDOMNode  *aParent,
                                         PRBool      &aResult) const
{
  aResult = PR_TRUE; // init out param.  we assume the condition is true unless we find a node that violates it
  if (!aStartNode || !aEndNode || !aParent) { return NS_ERROR_NULL_POINTER; }

  nsCOMPtr<nsIContentIterator>iter;
  nsresult result;
  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                              kIContentIteratorIID, getter_AddRefs(iter));
  //XXX: maybe CreateInstance is expensive, and I should keep around a static iter?  
  //     as long as this method can't be called recursively or re-entrantly!

  if ((NS_SUCCEEDED(result)) && iter)
  {
    nsCOMPtr<nsIContent>startContent;
    startContent = do_QueryInterface(aStartNode);
    nsCOMPtr<nsIContent>endContent;
    endContent = do_QueryInterface(aEndNode);
    if (startContent && endContent)
    {
      iter->Init(aRange);
      nsCOMPtr<nsIContent> content;
      iter->CurrentNode(getter_AddRefs(content));
      while (NS_COMFALSE == iter->IsDone())
      {
        if ((content.get() != startContent.get()) &&
            (content.get() != endContent.get()))
        {
          nsCOMPtr<nsIDOMNode>currentNode;
          currentNode = do_QueryInterface(content);
          PRBool isInline=PR_FALSE;
          IsNodeInline(currentNode, isInline);
          if (PR_FALSE==isInline)
          {
            nsCOMPtr<nsIDOMCharacterData>nodeAsText;
            nodeAsText = do_QueryInterface(currentNode);
            if (!nodeAsText)  // text nodes don't count in this check, so ignore them
            {
              aResult = PR_FALSE;
              break;
            }
          }
        }
        /* do not check result here, and especially do not return the result code.
         * we rely on iter->IsDone to tell us when the iteration is complete
         */
        iter->Next();
        iter->CurrentNode(getter_AddRefs(content));
      }
    }
  }

  return result;
}

/* this should only get called if the only intervening nodes are inline style nodes */
NS_IMETHODIMP 
nsTextEditor::SetTextPropertiesForNodesWithSameParent(nsIDOMNode *aStartNode,
                                                      PRInt32     aStartOffset,
                                                      nsIDOMNode *aEndNode,
                                                      PRInt32     aEndOffset,
                                                      nsIDOMNode *aParent,
                                                      nsIAtom    *aPropName)
{
  nsresult result=NS_OK;
  PRBool textPropertySet;
  IsTextPropertySetByContent(aStartNode, aPropName, textPropertySet);
  if (PR_FALSE==textPropertySet)
  {
    nsCOMPtr<nsIDOMNode>newLeftTextNode;  // this will be the middle text node
    if (0!=aStartOffset) {
      result = nsEditor::SplitNode(aStartNode, aStartOffset, getter_AddRefs(newLeftTextNode));
    }
    if (NS_SUCCEEDED(result))
    {
      nsCOMPtr<nsIDOMCharacterData>endNodeAsChar;
      endNodeAsChar = do_QueryInterface(aEndNode);
      if (!endNodeAsChar)
        return NS_ERROR_FAILURE;
      PRUint32 count;
      endNodeAsChar->GetLength(&count);
      nsCOMPtr<nsIDOMNode>newRightTextNode;  // this will be the middle text node
      if ((PRInt32)count!=aEndOffset) {
        result = nsEditor::SplitNode(aEndNode, aEndOffset, getter_AddRefs(newRightTextNode));
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
          aPropName->ToString(tag);
          // create the new style node, which will be the new parent for the selected nodes
          nsCOMPtr<nsIDOMNode>newStyleNode;
          result = nsEditor::CreateNode(tag, aParent, offsetInParent+1, getter_AddRefs(newStyleNode));
          if (NS_SUCCEEDED(result))
          { // move the right half of the start node into the new style node
            nsCOMPtr<nsIDOMNode>intermediateNode;
            result = aStartNode->GetNextSibling(getter_AddRefs(intermediateNode));
            if (NS_SUCCEEDED(result))
            {
              result = nsEditor::DeleteNode(aStartNode);
              if (NS_SUCCEEDED(result)) 
              { 
                PRInt32 childIndex=0;
                result = nsEditor::InsertNode(aStartNode, newStyleNode, childIndex);
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
                    result = nsEditor::DeleteNode(intermediateNode);
                    if (NS_SUCCEEDED(result)) {
                      result = nsEditor::InsertNode(intermediateNode, newStyleNode, childIndex);
                      childIndex++;
                    }
                    intermediateNode = do_QueryInterface(nextSibling);
                  }
                  if (NS_SUCCEEDED(result))
                  { // move the left half of the end node into the new style node
                    result = nsEditor::DeleteNode(newRightTextNode);
                    if (NS_SUCCEEDED(result)) 
                    {
                      result = nsEditor::InsertNode(newRightTextNode, newStyleNode, childIndex);
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
                                                           nsIAtom     *aPropName)
{
  nsresult result=NS_OK;
  if (!aRange || !aStartNode || !aEndNode || !aParent || !aPropName)
    return NS_ERROR_NULL_POINTER;
  // create a style node for the text in the start parent
  nsCOMPtr<nsIDOMNode>parent;
  result = aStartNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(result)) {
    return result;
  }

  // create style nodes for all the content between the start and end nodes
  nsCOMPtr<nsIContentIterator>iter;
  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                              kIContentIteratorIID, getter_AddRefs(iter));
  if ((NS_SUCCEEDED(result)) && iter)
  {
    nsCOMPtr<nsIContent>startContent;
    startContent = do_QueryInterface(aStartNode);
    nsCOMPtr<nsIContent>endContent;
    endContent = do_QueryInterface(aEndNode);
    if (startContent && endContent)
    {
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
          nsCOMPtr<nsIDOMCharacterData>charNode;
          charNode = do_QueryInterface(content);
          if (charNode)
          {
            // only want to wrap the text node in a new style node if it doesn't already have that style
            nsCOMPtr<nsIDOMNode>node;
            node = do_QueryInterface(content);
            PRBool textPropertySet;
            IsTextPropertySetByContent(node, aPropName, textPropertySet);
            if (PR_FALSE==textPropertySet)
            {
              nsCOMPtr<nsIDOMNode>parent;
              charNode->GetParentNode(getter_AddRefs(parent));
              if (!parent) {
                return NS_ERROR_NULL_POINTER;
              }
              nsCOMPtr<nsIContent>parentContent;
              parentContent = do_QueryInterface(parent);
            
              PRInt32 offsetInParent;
              parentContent->IndexOf(content, offsetInParent);

              nsCOMPtr<nsIDOMNode>newStyleNode;
              result = nsEditor::CreateNode(tag, parent, offsetInParent, getter_AddRefs(newStyleNode));
              if (NS_SUCCEEDED(result) && newStyleNode) {
                nsCOMPtr<nsIDOMNode>contentNode;
                contentNode = do_QueryInterface(content);
                result = nsEditor::DeleteNode(contentNode);
                if (NS_SUCCEEDED(result)) {
                  result = nsEditor::InsertNode(contentNode, newStyleNode, 0);
                }
              }
            }
          }
        }
        // note we don't check the result, we just rely on iter->IsDone
        iter->Next();
        result = iter->CurrentNode(getter_AddRefs(content));
      }
    }
  }

  nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
  nodeAsChar = do_QueryInterface(aStartNode);
  if (!nodeAsChar)
    return NS_ERROR_FAILURE;
  PRUint32 count;
  nodeAsChar->GetLength(&count);
  result = SetTextPropertiesForNode(aStartNode, parent, aStartOffset, count, aPropName);

  // create a style node for the text in the end parent
  result = aEndNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(result)) {
    return result;
  }
  nodeAsChar = do_QueryInterface(aEndNode);
  if (!nodeAsChar)
    return NS_ERROR_FAILURE;
  nodeAsChar->GetLength(&count);
  result = SetTextPropertiesForNode(aEndNode, parent, 0, aEndOffset, aPropName);

  return result;
}

NS_IMETHODIMP nsTextEditor::RemoveTextPropertiesForNode(nsIDOMNode *aNode, 
                                                        nsIDOMNode *aParent,
                                                        PRInt32     aStartOffset,
                                                        PRInt32     aEndOffset,
                                                        nsIAtom    *aPropName)
{
  nsresult result=NS_OK;
  nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
  nodeAsChar =  do_QueryInterface(aNode);
  if (!nodeAsChar)
    return NS_ERROR_FAILURE;

  PRBool textPropertySet;
  IsTextPropertySetByContent(aNode, aPropName, textPropertySet);
  if (PR_TRUE==textPropertySet)
  {
    PRUint32 count;
    nodeAsChar->GetLength(&count);
    // split the node, and all parent nodes up to the style node
    // then promote the selected content to the parent of the style node
    nsCOMPtr<nsIDOMNode>newLeftNode;  // this will be the leftmost node, 
                                      // the node being split will be rightmost
    if (0!=aStartOffset) {
      printf("* splitting text node %p at %d\n", aNode, aStartOffset);
      result = nsEditor::SplitNode(aNode, aStartOffset, getter_AddRefs(newLeftNode));
      printf("* split created left node %p\n", newLeftNode.get());
      if (gNoisy) {DebugDumpContent(); } // DEBUG
    }
    if (NS_SUCCEEDED(result))
    {
      nsCOMPtr<nsIDOMNode>newMiddleNode;  // this will be the middle node
      if ((PRInt32)count!=aEndOffset) {
        printf("* splitting text node (right node) %p at %d\n", aNode, aEndOffset-aStartOffset);
        result = nsEditor::SplitNode(aNode, aEndOffset-aStartOffset, getter_AddRefs(newMiddleNode));
        printf("* split created middle node %p\n", newMiddleNode.get());
        if (gNoisy) {DebugDumpContent(); } // DEBUG
      }
      else {
        printf("* no need to split text node\n");
        newMiddleNode = do_QueryInterface(aNode);
      }
      NS_ASSERTION(newMiddleNode, "no middle node created");
      // now that the text node is split, split parent nodes until we get to the style node
      nsCOMPtr<nsIDOMNode>parent;
      parent = do_QueryInterface(aParent);  // we know this has to succeed, no need to check
      if (NS_SUCCEEDED(result) && newMiddleNode)
      {
        // split every ancestor until we find the node that is giving us the style we want to remove
        // then split the style node and promote the selected content to the style node's parent
        while (NS_SUCCEEDED(result) && parent)
        {
          printf("* looking at parent %p\n", parent);
          // get the tag from parent and see if we're done
          nsCOMPtr<nsIDOMNode>temp;
          nsCOMPtr<nsIDOMElement>element;
          element = do_QueryInterface(parent);
          if (element)
          {
            nsAutoString tag;
            result = element->GetTagName(tag);
            printf("* parent has tag %s\n", tag.ToNewCString());  // leak!
            if (NS_SUCCEEDED(result))
            {
              if (PR_FALSE==tag.Equals(aPropName))
              {
                printf("* this is not the style node\n");
                PRInt32 offsetInParent;
                result = nsIEditorSupport::GetChildOffset(newMiddleNode, parent, offsetInParent);
                if (NS_SUCCEEDED(result))
                {
                  if (0!=offsetInParent) {
                    printf("* splitting parent %p at offset %d\n", parent, offsetInParent);
                    result = nsEditor::SplitNode(parent, offsetInParent, getter_AddRefs(newLeftNode));
                    printf("* split created left node %p sibling of parent\n", newLeftNode.get());
                    if (gNoisy) {DebugDumpContent(); } // DEBUG
                  }
                  if (NS_SUCCEEDED(result))
                  {
                    nsCOMPtr<nsIDOMNodeList>childNodes;
                    result = parent->GetChildNodes(getter_AddRefs(childNodes));
                    if (NS_SUCCEEDED(result) && childNodes)
                    {
                      childNodes->GetLength(&count);
                      NS_ASSERTION(count>0, "bad child count in newly split node");
                      if ((PRInt32)count!=1) 
                      {
                        printf("* splitting parent %p at offset %d\n", parent, 1);
                        result = nsEditor::SplitNode(parent, 1, getter_AddRefs(newMiddleNode));
                        printf("* split created middle node %p sibling of parent\n", newMiddleNode.get());
                        if (gNoisy) {DebugDumpContent(); } // DEBUG
                      }
                      else {
                        printf("* no need to split parent, newMiddleNode=parent\n");
                        newMiddleNode = do_QueryInterface(parent);
                      }
                      NS_ASSERTION(newMiddleNode, "no middle node created");
                      parent->GetParentNode(getter_AddRefs(temp));
                      parent = do_QueryInterface(temp);
                    }
                  }
                }
              }
              // else we've found the style tag (referred to by "parent")
              // nwMiddleNode is the node that is an ancestor to the selection
              else
              {
                printf("* this is the style node\n");
                PRInt32 offsetInParent;
                result = nsIEditorSupport::GetChildOffset(newMiddleNode, parent, offsetInParent);
                if (NS_SUCCEEDED(result))
                {
                  nsCOMPtr<nsIDOMNodeList>childNodes;
                  result = parent->GetChildNodes(getter_AddRefs(childNodes));
                  if (NS_SUCCEEDED(result) && childNodes)
                  {
                    childNodes->GetLength(&count);
                    if (0!=offsetInParent && ((PRInt32)count!=offsetInParent+1)) {
                      printf("* splitting parent %p at offset %d\n", parent, offsetInParent);
                      result = nsEditor::SplitNode(parent, offsetInParent, getter_AddRefs(newLeftNode));
                      printf("* split created left node %p sibling of parent\n", newLeftNode.get());
                      if (gNoisy) {DebugDumpContent(); } // DEBUG
                    }
                    if (NS_SUCCEEDED(result))
                    { // promote the selection to the grandparent
                      nsCOMPtr<nsIDOMNode>grandParent;
                      result = parent->GetParentNode(getter_AddRefs(grandParent));
                      if (NS_SUCCEEDED(result) && grandParent)
                      {
                        printf("* deleting middle node %p\n", newMiddleNode.get());
                        result = nsEditor::DeleteNode(newMiddleNode);
                        if (gNoisy) {DebugDumpContent(); } // DEBUG
                        if (NS_SUCCEEDED(result))
                        {
                          PRInt32 position;
                          result = nsIEditorSupport::GetChildOffset(parent, grandParent, position);
                          if (NS_SUCCEEDED(result)) 
                          {
                            printf("* inserting node %p in grandparent %p at offset %d\n", 
                                    newMiddleNode.get(), grandParent.get(), position);
                            result = nsEditor::InsertNode(newMiddleNode, grandParent, position);
                            if (gNoisy) {DebugDumpContent(); } // DEBUG
                            if (NS_SUCCEEDED(result)) 
                            {
                              PRBool hasChildren=PR_TRUE;
                              parent->HasChildNodes(&hasChildren);
                              if (PR_FALSE==hasChildren) {
                                printf("* deleting empty style node %p\n", parent.get());
                                result = nsEditor::DeleteNode(parent);
                                if (gNoisy) {DebugDumpContent(); } // DEBUG
                              }
                            }
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
                                                         nsIAtom    *aPropName)
{
  printf("not yet implemented\n");
  return NS_OK;
  nsresult result=NS_OK;
  PRBool textPropertySet;
  IsTextPropertySetByContent(aStartNode, aPropName, textPropertySet);
  if (PR_FALSE==textPropertySet)
  {
    nsCOMPtr<nsIDOMNode>newLeftTextNode;  // this will be the middle text node
    if (0!=aStartOffset) {
      result = nsEditor::SplitNode(aStartNode, aStartOffset, getter_AddRefs(newLeftTextNode));
    }
    if (NS_SUCCEEDED(result))
    {
      nsCOMPtr<nsIDOMCharacterData>endNodeAsChar;
      endNodeAsChar = do_QueryInterface(aEndNode);
      if (!endNodeAsChar)
        return NS_ERROR_FAILURE;
      PRUint32 count;
      endNodeAsChar->GetLength(&count);
      nsCOMPtr<nsIDOMNode>newRightTextNode;  // this will be the middle text node
      if ((PRInt32)count!=aEndOffset) {
        result = nsEditor::SplitNode(aEndNode, aEndOffset, getter_AddRefs(newRightTextNode));
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
          aPropName->ToString(tag);
          // create the new style node, which will be the new parent for the selected nodes
          nsCOMPtr<nsIDOMNode>newStyleNode;
          result = nsEditor::CreateNode(tag, aParent, offsetInParent+1, getter_AddRefs(newStyleNode));
          if (NS_SUCCEEDED(result))
          { // move the right half of the start node into the new style node
            nsCOMPtr<nsIDOMNode>intermediateNode;
            result = aStartNode->GetNextSibling(getter_AddRefs(intermediateNode));
            if (NS_SUCCEEDED(result))
            {
              result = nsEditor::DeleteNode(aStartNode);
              if (NS_SUCCEEDED(result)) 
              { 
                PRInt32 childIndex=0;
                result = nsEditor::InsertNode(aStartNode, newStyleNode, childIndex);
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
                    result = nsEditor::DeleteNode(intermediateNode);
                    if (NS_SUCCEEDED(result)) {
                      result = nsEditor::InsertNode(intermediateNode, newStyleNode, childIndex);
                      childIndex++;
                    }
                    intermediateNode = do_QueryInterface(nextSibling);
                  }
                  if (NS_SUCCEEDED(result))
                  { // move the left half of the end node into the new style node
                    result = nsEditor::DeleteNode(newRightTextNode);
                    if (NS_SUCCEEDED(result)) 
                    {
                      result = nsEditor::InsertNode(newRightTextNode, newStyleNode, childIndex);
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
nsTextEditor::RemoveTextPropertiesForNodeWithDifferentParents(nsIDOMRange *aRange,
                                                              nsIDOMNode  *aStartNode,
                                                              PRInt32      aStartOffset,
                                                              nsIDOMNode  *aEndNode,
                                                              PRInt32      aEndOffset,
                                                              nsIDOMNode  *aParent,
                                                              nsIAtom     *aPropName)
{
  printf("not yet implemented\n");
  return NS_OK;
  nsresult result=NS_OK;
  if (!aRange || !aStartNode || !aEndNode || !aParent || !aPropName)
    return NS_ERROR_NULL_POINTER;
  // create a style node for the text in the start parent
  nsCOMPtr<nsIDOMNode>parent;
  result = aStartNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(result)) {
    return result;
  }

  // create style nodes for all the content between the start and end nodes
  nsCOMPtr<nsIContentIterator>iter;
  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                              kIContentIteratorIID, getter_AddRefs(iter));
  if ((NS_SUCCEEDED(result)) && iter)
  {
    nsCOMPtr<nsIContent>startContent;
    startContent = do_QueryInterface(aStartNode);
    nsCOMPtr<nsIContent>endContent;
    endContent = do_QueryInterface(aEndNode);
    if (startContent && endContent)
    {
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
          nsCOMPtr<nsIDOMCharacterData>charNode;
          charNode = do_QueryInterface(content);
          if (charNode)
          {
            // only want to wrap the text node in a new style node if it doesn't already have that style
            nsCOMPtr<nsIDOMNode>node;
            node = do_QueryInterface(content);
            PRBool textPropertySet;
            IsTextPropertySetByContent(node, aPropName, textPropertySet);
            if (PR_FALSE==textPropertySet)
            {
              nsCOMPtr<nsIDOMNode>parent;
              charNode->GetParentNode(getter_AddRefs(parent));
              if (!parent) {
                return NS_ERROR_NULL_POINTER;
              }
              nsCOMPtr<nsIContent>parentContent;
              parentContent = do_QueryInterface(parent);
            
              PRInt32 offsetInParent;
              parentContent->IndexOf(content, offsetInParent);

              nsCOMPtr<nsIDOMNode>newStyleNode;
              result = nsEditor::CreateNode(tag, parent, offsetInParent, getter_AddRefs(newStyleNode));
              if (NS_SUCCEEDED(result) && newStyleNode) {
                nsCOMPtr<nsIDOMNode>contentNode;
                contentNode = do_QueryInterface(content);
                result = nsEditor::DeleteNode(contentNode);
                if (NS_SUCCEEDED(result)) {
                  result = nsEditor::InsertNode(contentNode, newStyleNode, 0);
                }
              }
            }
          }
        }
        // note we don't check the result, we just rely on iter->IsDone
        iter->Next();
        result = iter->CurrentNode(getter_AddRefs(content));
      }
    }
  }

  nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
  nodeAsChar = do_QueryInterface(aStartNode);
  if (!nodeAsChar)
    return NS_ERROR_FAILURE;
  PRUint32 count;
  nodeAsChar->GetLength(&count);
  result = SetTextPropertiesForNode(aStartNode, parent, aStartOffset, count, aPropName);

  // create a style node for the text in the end parent
  result = aEndNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(result)) {
    return result;
  }
  nodeAsChar = do_QueryInterface(aEndNode);
  if (!nodeAsChar)
    return NS_ERROR_FAILURE;
  nodeAsChar->GetLength(&count);
  result = SetTextPropertiesForNode(aEndNode, parent, 0, aEndOffset, aPropName);

  return result;
}

NS_IMETHODIMP
nsTextEditor::SetTypeInStateForProperty(TypeInState &aTypeInState, nsIAtom *aPropName)
{
  if (!aPropName) {
    return NS_ERROR_NULL_POINTER;
  }
  if (nsIEditProperty::b==aPropName) 
  {
    if (PR_TRUE==aTypeInState.IsSet(NS_TYPEINSTATE_BOLD))
    { // toggle currently set boldness
      aTypeInState.UnSet(NS_TYPEINSTATE_BOLD);
    }
    else
    { // get the current style and set boldness to the opposite of the current state
      PRBool any = PR_FALSE;
      PRBool all = PR_FALSE;
      PRBool first = PR_FALSE;
      GetTextProperty(aPropName, first, any, all); // operates on current selection
      aTypeInState.SetBold(!any);
    }
  }
  else if (nsIEditProperty::i==aPropName) 
  {
    if (PR_TRUE==aTypeInState.IsSet(NS_TYPEINSTATE_ITALIC))
    { // toggle currently set italicness
      aTypeInState.UnSet(NS_TYPEINSTATE_ITALIC);
    }
    else
    { // get the current style and set boldness to the opposite of the current state
      PRBool any = PR_FALSE;
      PRBool all = PR_FALSE;
      PRBool first = PR_FALSE;
      GetTextProperty(aPropName, first, any, all); // operates on current selection
      aTypeInState.SetItalic(!any);
    }    
  }
  else if (nsIEditProperty::u==aPropName) 
  {
    if (PR_TRUE==aTypeInState.IsSet(NS_TYPEINSTATE_UNDERLINE))
    { // toggle currently set italicness
      aTypeInState.UnSet(NS_TYPEINSTATE_UNDERLINE);
    }
    else
    { // get the current style and set boldness to the opposite of the current state
      PRBool any = PR_FALSE;
      PRBool all = PR_FALSE;
      PRBool first = PR_FALSE;
      GetTextProperty(aPropName, first, any, all); // operates on current selection
      aTypeInState.SetUnderline(!any);
    }    
  }
  else {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}
