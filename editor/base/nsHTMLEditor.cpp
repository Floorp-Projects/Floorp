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


#include "nsHTMLEditor.h"
#include "nsHTMLEditRules.h"

#include "nsEditorEventListeners.h"

#include "nsIDOMText.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMEventReceiver.h" 
#include "nsIDOMKeyListener.h" 
#include "nsIDOMMouseListener.h"
#include "nsIDOMSelection.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"

#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsIStyleSet.h"
#include "nsIDocumentObserver.h"
#include "nsIDocumentStateListener.h"

#include "nsIStyleContext.h"

#include "nsIEnumerator.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsEditorCID.h"
#include "nsLayoutCID.h"
#include "nsIDOMRange.h"
#include "nsIDOMNSRange.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsFileSpec.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsIDocumentEncoder.h"
#include "nsIPref.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIPresShell.h"
#include "nsIImage.h"
#include "nsAOLCiter.h"
#include "nsInternetCiter.h"

// netwerk
#include "nsIURI.h"
#include "nsNeckoUtil.h"

// Drag & Drop, Clipboard
#include "nsWidgetsCID.h"
#include "nsIClipboard.h"
#include "nsITransferable.h"

// Transactionas
#include "PlaceholderTxn.h"
#include "nsStyleSheetTxns.h"
#include "nsInsertHTMLTxn.h"

// Misc
#include "TextEditorTest.h"
#include "nsEditorUtils.h"

#include "prprf.h"

const unsigned char nbsp = 160;

// HACK - CID for NavDTD until we can get at dtd via the document
// {a6cf9107-15b3-11d2-932e-00805f8add32}
#define NS_CNAVDTD_CID \
{ 0xa6cf9107, 0x15b3, 0x11d2, { 0x93, 0x2e, 0x0, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }
static NS_DEFINE_CID(kCNavDTDCID,	  NS_CNAVDTD_CID);

static NS_DEFINE_CID(kHTMLEditorCID,  NS_HTMLEDITOR_CID);
static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_CID(kCRangeCID,      NS_RANGE_CID);
static NS_DEFINE_IID(kFileWidgetCID,  NS_FILEWIDGET_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

// Drag & Drop, Clipboard Support
static NS_DEFINE_CID(kCClipboardCID,    NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCTransferableCID, NS_TRANSFERABLE_CID);

#ifdef NS_DEBUG
static PRBool gNoisy = PR_FALSE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif

// Some utilities to handle stupid overloading of "A" tag for link and named anchor
static char hrefText[] = "href";
static char linkText[] = "link";
static char anchorText[] = "anchor";
static char namedanchorText[] = "namedanchor";

#define IsLink(s) (s.EqualsIgnoreCase(hrefText) || s.EqualsIgnoreCase(linkText))
#define IsNamedAnchor(s) (s.EqualsIgnoreCase(anchorText) || s.EqualsIgnoreCase(namedanchorText))

static PRBool IsLinkNode(nsIDOMNode *aNode)
{
  if (aNode)
  {
    nsCOMPtr<nsIDOMHTMLAnchorElement> anchor = do_QueryInterface(aNode);
    if (anchor)
    {
      nsString tmpText;
      if (NS_SUCCEEDED(anchor->GetHref(tmpText)) && tmpText.GetUnicode() && tmpText.Length() != 0)
        return PR_TRUE;
    }
  }
  return PR_FALSE;
}

static PRBool IsNamedAnchorNode(nsIDOMNode *aNode)
{
  if (aNode)
  {
    nsCOMPtr<nsIDOMHTMLAnchorElement> anchor = do_QueryInterface(aNode);
    if (anchor)
    {
      nsString tmpText;
      if (NS_SUCCEEDED(anchor->GetName(tmpText)) && tmpText.GetUnicode() && tmpText.Length() != 0)
        return PR_TRUE;
    }
  }
  return PR_FALSE;
}


nsHTMLEditor::nsHTMLEditor()
: nsEditor()
, mTypeInState(nsnull)
, mRules(nsnull)
, mIsComposing(PR_FALSE)
, mMaxTextLength(-1)
, mWrapColumn(0)
{
// Done in nsEditor
// NS_INIT_REFCNT();
} 

nsHTMLEditor::~nsHTMLEditor()
{
  //the autopointers will clear themselves up. 
  //but we need to also remove the listeners or we have a leak
  nsCOMPtr<nsIDOMSelection>selection;
  nsresult result = GetSelection(getter_AddRefs(selection));
	// if we don't get the selection, just skip this
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

NS_IMPL_ADDREF_INHERITED(nsHTMLEditor, nsEditor)
NS_IMPL_RELEASE_INHERITED(nsHTMLEditor, nsEditor)


NS_IMETHODIMP nsHTMLEditor::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr)
    return NS_ERROR_NULL_POINTER;
 
  *aInstancePtr = nsnull;
  
  if (aIID.Equals(nsIHTMLEditor::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsIHTMLEditor*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIEditorMailSupport::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsIEditorMailSupport*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsITableEditor::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsITableEditor*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIEditorStyleSheets::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsIEditorStyleSheets*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  
  return nsEditor::QueryInterface(aIID, aInstancePtr);
}


NS_IMETHODIMP nsHTMLEditor::Init(nsIDOMDocument *aDoc, 
                                 nsIPresShell   *aPresShell, PRUint32 aFlags)
{
  NS_PRECONDITION(aDoc && aPresShell, "bad arg");
  if (!aDoc || !aPresShell)
  	return NS_ERROR_NULL_POINTER;

  nsresult result = NS_ERROR_NULL_POINTER;
  // Init the base editor
  result = nsEditor::Init(aDoc, aPresShell, aFlags);
  if (NS_FAILED(result)) { return result; }

  // init the type-in state
  mTypeInState = new TypeInState();
  if (!mTypeInState) {return NS_ERROR_NULL_POINTER;}
  NS_ADDREF(mTypeInState);

  nsCOMPtr<nsIDOMSelection>selection;
  result = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) { return result; }
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
  if (NS_FAILED(result)) {
    HandleEventListenerError();
    return result;
  }
  
  // get a mouse listener
  result = NS_NewEditorMouseListener(getter_AddRefs(mMouseListenerP), this);
  if (NS_FAILED(result)) {
    HandleEventListenerError();
    return result;
  }

  // get a text listener
  result = NS_NewEditorTextListener(getter_AddRefs(mTextListenerP),this);
  if (NS_FAILED(result)) { 
#ifdef DEBUG_TAGUE
printf("nsTextEditor.cpp: failed to get TextEvent Listener\n");
#endif
	  HandleEventListenerError();
	  return result;
  }

  // get a composition listener
  result = NS_NewEditorCompositionListener(getter_AddRefs(mCompositionListenerP),this);
  if (NS_FAILED(result)) { 
#ifdef DEBUG_TAGUE
printf("nsTextEditor.cpp: failed to get TextEvent Listener\n");
#endif
	  HandleEventListenerError();
	  return result;
  }

  // get a drag listener
  result = NS_NewEditorDragListener(getter_AddRefs(mDragListenerP), this);
  if (NS_FAILED(result)) {
    HandleEventListenerError();
    return result;
  }

  // get a focus listener
  result = NS_NewEditorFocusListener(getter_AddRefs(mFocusListenerP), this);
  if (NS_FAILED(result)) {
    HandleEventListenerError();
    return result;
  }

  // get the DOM event receiver
  nsCOMPtr<nsIDOMEventReceiver> erP;
  result = aDoc->QueryInterface(nsIDOMEventReceiver::GetIID(), getter_AddRefs(erP));
  if (NS_FAILED(result)) {
    HandleEventListenerError();
    return result;
  }

  // register the event listeners with the DOM event reveiver
  result = erP->AddEventListenerByIID(mKeyListenerP, nsIDOMKeyListener::GetIID());
  NS_ASSERTION(NS_SUCCEEDED(result), "failed to register key listener");
	if (NS_SUCCEEDED(result))
	{
		result = erP->AddEventListenerByIID(mMouseListenerP, nsIDOMMouseListener::GetIID());
		NS_ASSERTION(NS_SUCCEEDED(result), "failed to register mouse listener");
		if (NS_SUCCEEDED(result))
		{
			result = erP->AddEventListenerByIID(mFocusListenerP, nsIDOMFocusListener::GetIID());
			NS_ASSERTION(NS_SUCCEEDED(result), "failed to register focus listener");
			if (NS_SUCCEEDED(result))
			{
				result = erP->AddEventListenerByIID(mTextListenerP, nsIDOMTextListener::GetIID());
				NS_ASSERTION(NS_SUCCEEDED(result), "failed to register text listener");
				if (NS_SUCCEEDED(result))
				{
					result = erP->AddEventListenerByIID(mCompositionListenerP, nsIDOMCompositionListener::GetIID());
					NS_ASSERTION(NS_SUCCEEDED(result), "failed to register composition listener");
					if (NS_SUCCEEDED(result))
					{
						result = erP->AddEventListenerByIID(mDragListenerP, nsIDOMDragListener::GetIID());
						NS_ASSERTION(NS_SUCCEEDED(result), "failed to register drag listener");
					}
				}
			}
		}
	}
  if (NS_FAILED(result)) {
    HandleEventListenerError();
    return result;
  }

  result = NS_OK;
	EnableUndo(PR_TRUE);
 
  // Set up a DTD    XXX XXX 
  // HACK: This should have happened in a document specific way
  //       in nsEditor::Init(), but we dont' have a way to do that yet
  result = nsComponentManager::CreateInstance(kCNavDTDCID, nsnull,
                                          nsIDTD::GetIID(), getter_AddRefs(mDTD));
  if (!mDTD) result = NS_ERROR_FAILURE;

  return result;
}

NS_IMETHODIMP 
nsHTMLEditor::GetFlags(PRUint32 *aFlags)
{
  if (!mRules || !aFlags) { return NS_ERROR_NULL_POINTER; }
  return mRules->GetFlags(aFlags);
}


NS_IMETHODIMP 
nsHTMLEditor::SetFlags(PRUint32 aFlags)
{
  if (!mRules) { return NS_ERROR_NULL_POINTER; }
  return mRules->SetFlags(aFlags);
}


void nsHTMLEditor::InitRules()
{
// instantiate the rules for this text editor
// XXX: we should be told which set of rules to instantiate
  if (mFlags & eEditorPlaintextMask)
	  mRules =  new nsTextEditRules();
	else
	  mRules =  new nsHTMLEditRules();

  mRules->Init(this, mFlags);
}

#ifdef XP_MAC
#pragma mark -
#pragma mark --- nsIHTMLEditor methods ---
#pragma mark -
#endif


NS_IMETHODIMP nsHTMLEditor::SetInlineProperty(nsIAtom *aProperty, 
                                            const nsString *aAttribute,
                                            const nsString *aValue)
{
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
  result = GetSelection(getter_AddRefs(selection));
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
        result = startParent->GetParentNode(getter_AddRefs(startGrandParent));
        if (NS_FAILED(result)) return result;
        if (!startGrandParent) return NS_ERROR_NULL_POINTER;
        nsCOMPtr<nsIDOMNode> endGrandParent;
        result = endParent->GetParentNode(getter_AddRefs(endGrandParent));
        if (NS_FAILED(result)) return result;
        if (!endGrandParent) return NS_ERROR_NULL_POINTER;

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

NS_IMETHODIMP nsHTMLEditor::GetInlineProperty(nsIAtom *aProperty, 
                                              const nsString *aAttribute, 
                                              const nsString *aValue,
                                              PRBool &aFirst, PRBool &aAny, PRBool &aAll)
{
  if (!aProperty)
    return NS_ERROR_NULL_POINTER;
/*
  if (gNoisy) 
  { 
    nsAutoString propString;
    aProperty->ToString(propString);
    char *propCString = propString.ToNewCString();
    if (gNoisy) { printf("nsTextEditor::GetTextProperty %s\n", propCString); }
    delete [] propCString;
  }
*/
  nsresult result;
  aAny=PR_FALSE;
  aAll=PR_TRUE;
  aFirst=PR_FALSE;
  PRBool first=PR_TRUE;
  nsCOMPtr<nsIDOMSelection>selection;
  result = GetSelection(getter_AddRefs(selection));
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
  // XXX: should be a while loop, to get each separate range
	// XXX: ERROR_HANDLING can currentItem be null?
  if ((NS_SUCCEEDED(result)) && currentItem)
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

NS_IMETHODIMP nsHTMLEditor::RemoveInlineProperty(nsIAtom *aProperty, const nsString *aAttribute)
{
  if (!aProperty) { return NS_ERROR_NULL_POINTER; }
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  if (gNoisy) 
  { 
    nsAutoString propString;
    aProperty->ToString(propString);
    char *propCString = propString.ToNewCString();
    if (gNoisy) { printf("---------- start nsTextEditor::RemoveInlineProperty %s ----------\n", propCString); }
    delete [] propCString;
  }

  nsresult result;
  nsCOMPtr<nsIDOMSelection>selection;
  result = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) return result;
  if (!selection) return NS_ERROR_NULL_POINTER;

  PRBool cancel;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kRemoveTextProperty);
  result = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if (NS_FAILED(result)) return result;
  if (PR_FALSE==cancel)
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
      //XXX: should be a while loop to get all ranged in selection
      if (currentItem)
      {
        nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
        nsCOMPtr<nsIDOMNode>commonParent;
        result = range->GetCommonParent(getter_AddRefs(commonParent));
				if (NS_FAILED(result)) return result;
				if (!commonParent) return NS_ERROR_NULL_POINTER;

        range->GetStartOffset(&startOffset);
        range->GetEndOffset(&endOffset);
        result = range->GetStartParent(getter_AddRefs(startParent));
				if (NS_FAILED(result)) return result;
				if (!startParent) return NS_ERROR_NULL_POINTER;
        result = range->GetEndParent(getter_AddRefs(endParent));
				if (NS_FAILED(result)) return result;
				if (!endParent) return NS_ERROR_NULL_POINTER;

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
        if (NS_FAILED(result)) return result;

        { // compute a range for the selection
          // don't want to actually do anything with selection, because
          // we are still iterating through it.  Just want to create and remember
          // an nsIDOMRange, and later add the range to the selection after clearing it.
          // XXX: I'm blocked here because nsIDOMSelection doesn't provide a mechanism
          //      for setting a compound selection yet.
        }
      }
			if (NS_SUCCEEDED(result))
			{
				result = CollapseAdjacentTextNodes(selection);  // we may have created wasteful consecutive text nodes.  collapse them.
			}
      nsEditor::EndTransaction();
      if (NS_SUCCEEDED(result))
      { 
				// XXX: ERROR_HANDLING should get a result and validate it
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
    if (gNoisy) { printf("---------- end nsTextEditor::RemoveInlineProperty %s ----------\n", propCString); }
    delete [] propCString;
  }
  return result;
}

NS_IMETHODIMP nsHTMLEditor::DeleteSelection(nsIEditor::ESelectionCollapseDirection aAction)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel= PR_FALSE;

  nsresult result = nsEditor::BeginTransaction();
  if (NS_FAILED(result)) { return result; }

  // pre-process
  result = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) return result;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsTextRulesInfo ruleInfo(nsTextEditRules::kDeleteSelection);
  ruleInfo.collapsedAction = aAction;
  result = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if ((PR_FALSE==cancel) && (NS_SUCCEEDED(result)))
  {
    result = DeleteSelectionImpl(aAction);
    // post-process 
    result = mRules->DidDoAction(selection, &ruleInfo, result);
  }

	// XXX: ERROR_HANDLING is there any reason to not consider this a serious error?
  nsresult endTxnResult = nsEditor::EndTransaction();  // don't return this result!
  NS_ASSERTION ((NS_SUCCEEDED(endTxnResult)), "bad end transaction result");

  return result;
}

NS_IMETHODIMP nsHTMLEditor::InsertText(const nsString& aStringToInsert)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel= PR_FALSE;

  // pre-process
  nsresult result = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) return result;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsString resultString;
  PlaceholderTxn *placeholderTxn=nsnull;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kInsertText);
  ruleInfo.placeTxn = &placeholderTxn;
  ruleInfo.inString = &aStringToInsert;
  ruleInfo.outString = &resultString;
  ruleInfo.typeInState = *mTypeInState;
  ruleInfo.maxLength = mMaxTextLength;

  result = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if ((PR_FALSE==cancel) && (NS_SUCCEEDED(result)))
  {
    result = InsertTextImpl(resultString);
    // post-process 
    result = mRules->DidDoAction(selection, &ruleInfo, result);
  }
  if (placeholderTxn)
    placeholderTxn->SetAbsorb(PR_FALSE);  // this ends the merging of txns into placeholderTxn

  return result;
}


NS_IMETHODIMP nsHTMLEditor::InsertHTML(const nsString& aInputString)
{
  nsAutoEditBatch beginBatching(this);

  nsresult res;
  nsCOMPtr<nsIDOMNode> parentNode;
  PRInt32 offsetOfNewNode;
  res = DeleteSelectionAndPrepareToCreateNode(parentNode, offsetOfNewNode);

  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

    // Get the first range in the selection, for context:
  nsCOMPtr<nsIDOMRange> range;
  res = selection->GetRangeAt(0, getter_AddRefs(range));
  if (NS_FAILED(res))
    return res;

  nsCOMPtr<nsIDOMNSRange> nsrange (do_QueryInterface(range));
  if (!nsrange)
    return NS_ERROR_NO_INTERFACE;

  nsCOMPtr<nsIDOMDocumentFragment> docfrag;
  res = nsrange->CreateContextualFragment(aInputString,
                                          getter_AddRefs(docfrag));
  if (NS_FAILED(res))
  {
#ifdef DEBUG
    printf("Couldn't create contextual fragment: error was %d\n", res);
#endif
    return res;
  }

#if defined(DEBUG_akkana)
  printf("============ Fragment dump :===========\n");

  nsCOMPtr<nsIContent> fragc (do_QueryInterface(docfrag));
  if (!fragc)
    printf("Couldn't get fragment is nsIContent\n");
  else
    fragc->List(stdout);
#endif

  // Insert the contents of the document fragment:
  nsCOMPtr<nsIDOMNode> fragmentAsNode (do_QueryInterface(docfrag));
#define INSERT_FRAGMENT_DIRECTLY 1
#ifdef INSERT_FRAGMENT_DIRECTLY
  // Make a collapsed range pointing to right after the current selection,
  // and let range gravity keep track of where it is so that we can
  // set the selection back there after the insert.
  // Unfortunately this doesn't work right yet.
  nsCOMPtr<nsIDOMRange> saverange;
  res = selection->GetRangeAt(0, getter_AddRefs(saverange));

  // Insert the node:
  res = InsertNode(fragmentAsNode, parentNode, offsetOfNewNode);

  // Now collapse the selection to the beginning of what we just inserted;
  // would be better to set it to the end.
  if (saverange)
  {
    nsCOMPtr<nsIDOMNode> parent;
    PRInt32 offset;
		// XXX: ERROR_HANDLING  error return codes are lost
    if (NS_SUCCEEDED(saverange->GetEndParent(getter_AddRefs(parent))))
      if (NS_SUCCEEDED(saverange->GetEndOffset(&offset)))
        selection->Collapse(parent, offset);
  }
  else
    selection->Collapse(parentNode, 0/*offsetOfNewNode*/);
#else /* INSERT_FRAGMENT_DIRECTLY */
  // Loop over the contents of the fragment:
  nsCOMPtr<nsIDOMNode> child;
  res = fragmentAsNode->GetFirstChild(getter_AddRefS(child));
  if (NS_FAILED(res))
  {
    printf("GetFirstChild failed!\n");
    return res;
  }
  while (child)
  {
    res = InsertNode(child, parentNode, offsetOfNewNode++);
    if (NS_FAILED(res))
      break;
    nsCOMPtr<nsIDOMNode> nextSib;
    if (NS_FAILED(child->GetNextSibling(getter_AddRefs(nextSib))))
      /*break*/;
    child = nextSib;
  }
  if (NS_FAILED(res))
    return res;

  // Now collapse the selection to the end of what we just inserted:
  selection->Collapse(parentNode, offsetOfNewNode);
#endif /* INSERT_FRAGMENT_DIRECTLY */
  return res;
}


NS_IMETHODIMP nsHTMLEditor::InsertBreak()
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel= PR_FALSE;

  nsAutoEditBatch beginBatching(this);

  // pre-process
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kInsertBreak);
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if ((PR_FALSE==cancel) && (NS_SUCCEEDED(res)))
  {
    // create the new BR node
    nsCOMPtr<nsIDOMNode> newNode;
    nsAutoString tag("BR");
    res = DeleteSelectionAndCreateNode(tag, getter_AddRefs(newNode));
    if (!newNode) res = NS_ERROR_NULL_POINTER; // don't return here, so DidDoAction is called
    if (NS_SUCCEEDED(res))
    {
      // set the selection to the new node
      nsCOMPtr<nsIDOMNode>parent;
      res = newNode->GetParentNode(getter_AddRefs(parent));
      if (!parent) res = NS_ERROR_NULL_POINTER; // don't return here, so DidDoAction is called
      if (NS_SUCCEEDED(res))
      {
        PRInt32 offsetInParent=-1;  // we use the -1 as a marker to see if we need to compute this or not
        nsCOMPtr<nsIDOMNode>nextNode;
        newNode->GetNextSibling(getter_AddRefs(nextNode));
        if (nextNode)
        {
          nsCOMPtr<nsIDOMCharacterData>nextTextNode;
          nextTextNode = do_QueryInterface(nextNode);
          if (!nextTextNode) {
            nextNode = do_QueryInterface(newNode);
          }
          else { 
            offsetInParent=0; 
          }
        }
        else {
          nextNode = do_QueryInterface(newNode);
        }
        res = GetSelection(getter_AddRefs(selection));
        if (!selection) res = NS_ERROR_NULL_POINTER; // don't return here, so DidDoAction is called
        if (NS_SUCCEEDED(res))
        {
          if (-1==offsetInParent) 
          {
            nextNode->GetParentNode(getter_AddRefs(parent));
            res = GetChildOffset(nextNode, parent, offsetInParent);
            if (NS_SUCCEEDED(res)) {
              res = selection->Collapse(parent, offsetInParent+1);  // +1 to insert just after the break
            }
          }
          else
          {
            res = selection->Collapse(nextNode, offsetInParent);
          }
        }
      }
    }
    // post-process, always called if WillInsertBreak didn't return cancel==PR_TRUE
    res = mRules->DidDoAction(selection, &ruleInfo, res);
  }

  return res;
}


NS_IMETHODIMP
nsHTMLEditor::InsertElement(nsIDOMElement* aElement, PRBool aDeleteSelection)
{
  nsresult res = NS_ERROR_NOT_INITIALIZED;
  
  if (!aElement)
    return NS_ERROR_NULL_POINTER;
  
  nsAutoEditBatch beginBatching(this);
  // For most elements, set caret after inserting
  //PRBool setCaretAfterElement = PR_TRUE;

  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (!NS_SUCCEEDED(res) || !selection)
    return NS_ERROR_FAILURE;

  // hand off to the rules system, see if it has anything to say about this
  PRBool cancel;
  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kInsertElement);
  ruleInfo.insertElement = aElement;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if (cancel || (NS_FAILED(res))) return res;

  // Clear current selection.
  // Should put caret at anchor point?
  if (!aDeleteSelection)
  {
    PRBool collapseAfter = PR_TRUE;
    // Named Anchor is a special case,
    // We collapse to insert element BEFORE the selection
    // For all other tags, we insert AFTER the selection
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);
    if (IsNamedAnchorNode(node))
      collapseAfter = PR_FALSE;

    if (collapseAfter)
    {
      // Default behavior is to collapse to the end of the selection
      selection->ClearSelection();
    } else {
      // Collapse to the start of the selection,
      // We must explore the first range and find
      //   its parent and starting offset of selection
      // TODO: Move this logic to a new method nsIDOMSelection::CollapseToStart()???
      nsCOMPtr<nsIDOMRange> firstRange;
      res = selection->GetRangeAt(0, getter_AddRefs(firstRange));
			// XXX: ERROR_HANDLING can firstRange legally be null?
      if (NS_SUCCEEDED(res) && firstRange)
      {
        nsCOMPtr<nsIDOMNode> parent;
				// XXX: ERROR_HANDLING bad XPCOM usage, should check for null parent separately
        res = firstRange->GetCommonParent(getter_AddRefs(parent));
        if (NS_SUCCEEDED(res) && parent)
        {
          PRInt32 startOffset;
          firstRange->GetStartOffset(&startOffset);
          selection->Collapse(parent, startOffset);
        } else {
          // Very unlikely, but collapse to the end if we failed above
          selection->ClearSelection();
        }
      }
    }
  }
  
  nsCOMPtr<nsIDOMNode> parentSelectedNode;
  PRInt32 offsetForInsert;
  res = selection->GetAnchorNode(getter_AddRefs(parentSelectedNode));
	// XXX: ERROR_HANDLING bad XPCOM usage
  if (NS_SUCCEEDED(res) && NS_SUCCEEDED(selection->GetAnchorOffset(&offsetForInsert)) && parentSelectedNode)
  {
#ifdef DEBUG_cmanske
    {
    nsAutoString name;
    parentSelectedNode->GetNodeName(name);
    printf("InsertElement: Anchor node of selection: ");
    wprintf(name.GetUnicode());
    printf(" Offset: %d\n", offsetForInsert);
    }
#endif
    nsAutoString tagName;
    aElement->GetNodeName(tagName);
    tagName.ToLowerCase();
    nsCOMPtr<nsIDOMNode> parent = parentSelectedNode;
    nsCOMPtr<nsIDOMNode> topChild = parentSelectedNode;
    nsCOMPtr<nsIDOMNode> tmp;
    nsAutoString parentTagName;
    PRBool isRoot;

    // Search up the parent chain to find a suitable container    
    while (!CanContainTag(parent, tagName))
    {
      // If the current parent is a root (body or table cell)
      // then go no further - we can't insert
      parent->GetNodeName(parentTagName);
      res = IsRootTag(parentTagName, isRoot);
      if (!NS_SUCCEEDED(res) || isRoot)
        return NS_ERROR_FAILURE;
      // Get the next parent
      parent->GetParentNode(getter_AddRefs(tmp));
      if (!tmp)
        return NS_ERROR_FAILURE;
      topChild = parent;
      parent = tmp;
    }
#ifdef DEBUG_cmanske
    {
    nsAutoString name;
    parent->GetNodeName(name);
    printf("Parent node to insert under: ");
    wprintf(name.GetUnicode());
    printf("\n");
    topChild->GetNodeName(name);
    printf("TopChild to split: ");
    wprintf(name.GetUnicode());
    printf("\n");
    }
#endif
    if (parent != topChild)
    {
      // we need to split some levels above the original selection parent
      res = SplitNodeDeep(topChild, parentSelectedNode, offsetForInsert, &offsetForInsert);
      if (NS_FAILED(res))
        return res;
    }
    // Now we can insert the new node
    res = InsertNode(aElement, parent, offsetForInsert);

    // Set caret after element, but check for special case 
    //  of inserting table-related elements: set in first cell instead
    if (!SetCaretInTableCell(aElement))
      res = SetCaretAfterElement(aElement);

  }
  return res;
}

// XXX: error handling in this routine needs to be cleaned up!
NS_IMETHODIMP
nsHTMLEditor::DeleteSelectionAndCreateNode(const nsString& aTag,
                                           nsIDOMNode ** aNewNode)
{
  nsCOMPtr<nsIDOMNode> parentSelectedNode;
  PRInt32 offsetOfNewNode;
  nsresult result = DeleteSelectionAndPrepareToCreateNode(parentSelectedNode,
                                                          offsetOfNewNode);
  if (!NS_SUCCEEDED(result))
    return result;

  nsCOMPtr<nsIDOMNode> newNode;
  result = CreateNode(aTag, parentSelectedNode, offsetOfNewNode,
                      getter_AddRefs(newNode));
  // XXX: ERROR_HANDLING  check result, and make sure aNewNode is set correctly in success/failure cases
  *aNewNode = newNode;

  // we want the selection to be just after the new node
  nsCOMPtr<nsIDOMSelection> selection;
  result = GetSelection(getter_AddRefs(selection));
	if (NS_FAILED(result)) return result;
	if (!selection) return NS_ERROR_NULL_POINTER;
  result = selection->Collapse(parentSelectedNode, offsetOfNewNode+1);

  return result;
}

NS_IMETHODIMP
nsHTMLEditor::SelectElement(nsIDOMElement* aElement)
{
  nsresult res = NS_ERROR_NULL_POINTER;

  // Must be sure that element is contained in the document body
  if (IsElementInBody(aElement))
  {
    nsCOMPtr<nsIDOMSelection> selection;
    res = GetSelection(getter_AddRefs(selection));
		if (NS_FAILED(res)) return res;
		if (!selection) return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIDOMNode>parent;
    res = aElement->GetParentNode(getter_AddRefs(parent));
    if (NS_SUCCEEDED(res) && parent)
    {
      PRInt32 offsetInParent;
      res = GetChildOffset(aElement, parent, offsetInParent);

      if (NS_SUCCEEDED(res))
      {
        // Collapse selection to just before desired element,
        res = selection->Collapse(parent, offsetInParent);
				if (NS_SUCCEEDED(res)) {
					//  then extend it to just after
					res = selection->Extend(parent, offsetInParent+1);
				}
      }
    }
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SetCaretAfterElement(nsIDOMElement* aElement)
{
  nsresult res = NS_ERROR_NULL_POINTER;

  // Be sure the element is contained in the document body
  if (aElement && IsElementInBody(aElement))
  {
    nsCOMPtr<nsIDOMSelection> selection;
    res = GetSelection(getter_AddRefs(selection));
		if (NS_FAILED(res)) return res;
		if (!selection) return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIDOMNode>parent;
    res = aElement->GetParentNode(getter_AddRefs(parent));
		if (NS_FAILED(res)) return res;
		if (!parent) return NS_ERROR_NULL_POINTER;
    PRInt32 offsetInParent;
    res = GetChildOffset(aElement, parent, offsetInParent);
    if (NS_SUCCEEDED(res))
    {
      // Collapse selection to just after desired element,
      res = selection->Collapse(parent, offsetInParent+1);
#ifdef DEBUG_cmanske
      {
      nsAutoString name;
      parent->GetNodeName(name);
      printf("SetCaretAfterElement: Parent node: ");
      wprintf(name.GetUnicode());
      printf(" Offset: %d\n\nHTML:\n", offsetInParent+1);
      nsString Format("text/html");
      nsString ContentsAs;
      OutputToString(ContentsAs, Format, 2);
      wprintf(ContentsAs.GetUnicode());
      }
#endif
    }
  }
  return res;
}


NS_IMETHODIMP nsHTMLEditor::GetParagraphFormat(nsString& aParagraphFormat)
{
  nsresult res = NS_ERROR_NOT_INITIALIZED;

  return res;
}

NS_IMETHODIMP nsHTMLEditor::SetParagraphFormat(const nsString& aParagraphFormat)
{
  nsresult res = NS_ERROR_NOT_INITIALIZED;
  //Kinda sad to waste memory just to force lower case
  nsAutoString tag = aParagraphFormat;
  tag.ToLowerCase();
  if (tag == "normal") 
  {
    res = InsertBasicBlock("p");
  } 
  else if (tag == "li") 
  {
    res = InsertList("ul");
  } 
  else 
  {
    res = InsertBasicBlock(tag);
  }
  return res;
}


// get the paragraph style(s) for the selection
// XXX: ERROR_HANDLING -- this method needs a little work to ensure all error codes are 
//                        checked properly, all null pointers are checked, and no memory leaks occur
NS_IMETHODIMP 
nsHTMLEditor::GetParagraphStyle(nsStringArray *aTagList)
{
  if (gNoisy) { printf("---------- nsHTMLEditor::GetParagraphStyle ----------\n"); }
  if (!aTagList) { return NS_ERROR_NULL_POINTER; }

  nsresult res;
  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIEnumerator> enumerator;
  res = selection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(res)) return res;
  if (!enumerator) return NS_ERROR_NULL_POINTER;

  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  res = enumerator->CurrentItem(getter_AddRefs(currentItem));
  if (NS_FAILED(res)) return res;
  //XXX: should be while loop?
  if (currentItem)
  {
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    // scan the range for all the independent block content blockSections
    // and get the block parent of each
    nsISupportsArray *blockSections;
    res = NS_NewISupportsArray(&blockSections);
    if (NS_FAILED(res)) return res;
    if (!blockSections) return NS_ERROR_NULL_POINTER;
    res = GetBlockSectionsForRange(range, blockSections);
    if (NS_SUCCEEDED(res))
    {
      nsIDOMRange *subRange;
      subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
      while (subRange)
      {
        nsCOMPtr<nsIDOMNode>startParent;
        res = subRange->GetStartParent(getter_AddRefs(startParent));
        if (NS_SUCCEEDED(res) && startParent) 
        {
          nsCOMPtr<nsIDOMElement> blockParent;
          res = GetBlockParent(startParent, getter_AddRefs(blockParent));
          if (NS_SUCCEEDED(res) && blockParent)
          {
            nsAutoString blockParentTag;
            blockParent->GetTagName(blockParentTag);
            PRBool isRoot;
            IsRootTag(blockParentTag, isRoot);
            if ((PR_FALSE==isRoot) && (-1==aTagList->IndexOf(blockParentTag))) {
              aTagList->AppendString(blockParentTag);
            }
          }
        }
        NS_RELEASE(subRange);
        if (NS_FAILED(res))
          break;  // don't return here, need to release blockSections
        blockSections->RemoveElementAt(0);
        subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
      }
    }
    NS_RELEASE(blockSections);
  }

  return res;
}

// use this when block parents are to be added regardless of current state
NS_IMETHODIMP 
nsHTMLEditor::AddBlockParent(nsString& aParentTag)
{
  if (gNoisy) 
  { 
    char *tag = aParentTag.ToNewCString();
    printf("---------- nsHTMLEditor::AddBlockParent %s ----------\n", tag); 
    delete [] tag;
  }
  
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  // set the block parent for all selected ranges
  nsAutoSelectionReset selectionResetter(selection);
  nsAutoEditBatch beginBatching(this);
  nsCOMPtr<nsIEnumerator> enumerator;
  enumerator = do_QueryInterface(selection);
  if (enumerator)
  {
    enumerator->First(); 
    nsCOMPtr<nsISupports> currentItem;
    res = enumerator->CurrentItem(getter_AddRefs(currentItem));
		// XXX: ERROR_HANDLING  can currentItem be null?
    if ((NS_SUCCEEDED(res)) && (currentItem))
    {
      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
      // scan the range for all the independent block content blockSections
      // and apply the transformation to them
      res = ReParentContentOfRange(range, aParentTag, eInsertParent);
    }
  }
  if (NS_SUCCEEDED(res))
  { // set the selection
    // XXX: can't do anything until I can create ranges
  }
  if (gNoisy) {printf("Finished nsHTMLEditor::AddBlockParent with this content:\n"); DebugDumpContent(); } // DEBUG

  return res;
}

// use this when a paragraph type is being transformed from one type to another
NS_IMETHODIMP 
nsHTMLEditor::ReplaceBlockParent(nsString& aParentTag)
{
  if (gNoisy) 
  { 
    char *tag = aParentTag.ToNewCString();
    printf("---------- nsHTMLEditor::ReplaceBlockParent %s ----------\n", tag); 
    delete [] tag;
  }
  
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsAutoSelectionReset selectionResetter(selection);
  // set the block parent for all selected ranges
  nsAutoEditBatch beginBatching(this);
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(res)) return res;
  if (!enumerator) return NS_ERROR_NULL_POINTER;

  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  res = enumerator->CurrentItem(getter_AddRefs(currentItem));
	// XXX: ERROR_HANDLING  can currentItem be null?
  if ((NS_SUCCEEDED(res)) && (currentItem))
  {
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    // scan the range for all the independent block content blockSections
    // and apply the transformation to them
    res = ReParentContentOfRange(range, aParentTag, eReplaceParent);
  }
  if (gNoisy) {printf("Finished nsHTMLEditor::AddBlockParent with this content:\n"); DebugDumpContent(); } // DEBUG

  return res;
}


NS_IMETHODIMP 
nsHTMLEditor::RemoveParagraphStyle()
{
  if (gNoisy) { 
    printf("---------- nsHTMLEditor::RemoveParagraphStyle ----------\n"); 
  }
  
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsAutoSelectionReset selectionResetter(selection);
  nsAutoEditBatch beginBatching(this);
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(res)) return res;
  if (!enumerator) return NS_ERROR_NULL_POINTER;

  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  res = enumerator->CurrentItem(getter_AddRefs(currentItem));
	// XXX: ERROR_HANDLING  can currentItem be null?
  if ((NS_SUCCEEDED(res)) && (currentItem))
  {
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    res = RemoveParagraphStyleFromRange(range);
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::RemoveParent(const nsString &aParentTag)
{
  if (gNoisy) { 
    printf("---------- nsHTMLEditor::RemoveParent ----------\n"); 
  }
  
  nsresult res;
  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsAutoSelectionReset selectionResetter(selection);
  nsAutoEditBatch beginBatching(this);
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(res)) return res;
  if (!enumerator) return NS_ERROR_NULL_POINTER;

  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  res = enumerator->CurrentItem(getter_AddRefs(currentItem));
	// XXX: ERROR_HANDLING can currentItem be null?
  if ((NS_SUCCEEDED(res)) && (currentItem))
  {
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    res = RemoveParentFromRange(aParentTag, range);
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::InsertList(const nsString& aListType)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel= PR_FALSE;

  nsAutoEditBatch beginBatching(this);
  
  // pre-process
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kMakeList);
  if (aListType == "ol") ruleInfo.bOrdered = PR_TRUE;
  else  ruleInfo.bOrdered = PR_FALSE;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if (cancel || (NS_FAILED(res))) return res;

  // Find out if the selection is collapsed:
  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  
  res = GetStartNodeAndOffset(selection, &node, &offset);
  if (!node) res = NS_ERROR_FAILURE;
  if (NS_FAILED(res)) return res;
  
  if (isCollapsed)
  {
    // have to find a place to put the list
    nsCOMPtr<nsIDOMNode> parent = node;
    nsCOMPtr<nsIDOMNode> topChild = node;
    nsCOMPtr<nsIDOMNode> tmp;
    
    while ( !CanContainTag(parent, aListType))
    {
      parent->GetParentNode(getter_AddRefs(tmp));
      if (!tmp) return NS_ERROR_FAILURE;
      topChild = parent;
      parent = tmp;
    }
    
    if (parent != node)
    {
      // we need to split up to the child of parent
      res = SplitNodeDeep(topChild, node, offset, &offset);
      if (NS_FAILED(res)) return res;
    }

    // make a list
    nsCOMPtr<nsIDOMNode> newList;
    res = CreateNode(aListType, parent, offset, getter_AddRefs(newList));
    if (NS_FAILED(res)) return res;
    // make a list item
    nsAutoString tag("li");
    nsCOMPtr<nsIDOMNode> newItem;
    res = CreateNode(tag, newList, 0, getter_AddRefs(newItem));
    if (NS_FAILED(res)) return res;
    // put a space in it so layout will draw the list item
    // XXX - revisit when layout is fixed
    res = selection->Collapse(newItem,0);
    if (NS_FAILED(res)) return res;
#if 0    
    nsAutoString theText(" ");
    res = InsertText(theText);
    if (NS_FAILED(res)) return res;
    // reposition selection to before the space character
    res = GetStartNodeAndOffset(selection, &node, &offset);
    if (NS_FAILED(res)) return res;
    res = selection->Collapse(node,0);
    if (NS_FAILED(res)) return res;
#endif
  }

  return res;
}


NS_IMETHODIMP
nsHTMLEditor::InsertBasicBlock(const nsString& aBlockType)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  nsCOMPtr<nsIDOMSelection> selection;
  PRBool cancel= PR_FALSE;

  nsAutoEditBatch beginBatching(this);
  
  // pre-process
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kMakeBasicBlock);
  ruleInfo.blockType = &aBlockType;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if (cancel || (NS_FAILED(res))) return res;

  // Find out if the selection is collapsed:
  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  
  res = GetStartNodeAndOffset(selection, &node, &offset);
  if (!node) res = NS_ERROR_FAILURE;
  if (NS_FAILED(res)) return res;
  
  if (isCollapsed)
  {
    // have to find a place to put the block
    nsCOMPtr<nsIDOMNode> parent = node;
    nsCOMPtr<nsIDOMNode> topChild = node;
    nsCOMPtr<nsIDOMNode> tmp;
    
    while ( !CanContainTag(parent, aBlockType))
    {
      parent->GetParentNode(getter_AddRefs(tmp));
      if (!tmp) return NS_ERROR_FAILURE;
      topChild = parent;
      parent = tmp;
    }
    
    if (parent != node)
    {
      // we need to split up to the child of parent
      res = SplitNodeDeep(topChild, node, offset, &offset);
      if (NS_FAILED(res)) return res;
    }

    // make a block
    nsCOMPtr<nsIDOMNode> newBlock;
    res = CreateNode(aBlockType, parent, offset, getter_AddRefs(newBlock));
    if (NS_FAILED(res)) return res;
    
    // xxx
    
    // put a space in it so layout will draw it
    res = selection->Collapse(newBlock,0);
    if (NS_FAILED(res)) return res;
    nsAutoString theText(nbsp);
    res = InsertText(theText);
    if (NS_FAILED(res)) return res;
    // reposition selection to before the space character
    res = GetStartNodeAndOffset(selection, &node, &offset);
    if (NS_FAILED(res)) return res;
    res = selection->Collapse(node,0);
    if (NS_FAILED(res)) return res;
  }

  return res;
}

// TODO: Implement "outdent"
NS_IMETHODIMP
nsHTMLEditor::Indent(const nsString& aIndent)
{
  nsresult res;
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  PRBool cancel= PR_FALSE;

  nsAutoEditBatch beginBatching(this);
  
  // pre-process
  nsCOMPtr<nsIDOMSelection> selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kIndent);
  if (aIndent == "outdent")
    ruleInfo.action = nsHTMLEditRules::kOutdent;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel);
  if (cancel || (NS_FAILED(res))) return res;
  
  // Do default - insert a blockquote node if selection collapsed
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;

  res = GetStartNodeAndOffset(selection, &node, &offset);
  if (!node) res = NS_ERROR_FAILURE;
  if (NS_FAILED(res)) return res;
  
  nsAutoString inward("indent");
  if (aIndent == inward)
  {
    if (isCollapsed)
    {
      // have to find a place to put the blockquote
      nsCOMPtr<nsIDOMNode> parent = node;
      nsCOMPtr<nsIDOMNode> topChild = node;
      nsCOMPtr<nsIDOMNode> tmp;
      nsAutoString bq("blockquote");
      while ( !CanContainTag(parent, bq))
      {
        parent->GetParentNode(getter_AddRefs(tmp));
        if (!tmp) return NS_ERROR_FAILURE;
        topChild = parent;
        parent = tmp;
      }
    
      if (parent != node)
      {
        // we need to split up to the child of parent
        res = SplitNodeDeep(topChild, node, offset, &offset);
        if (NS_FAILED(res)) return res;
      }

      // make a blockquote
      nsCOMPtr<nsIDOMNode> newBQ;
      res = CreateNode(bq, parent, offset, getter_AddRefs(newBQ));
      if (NS_FAILED(res)) return res;
      // put a space in it so layout will draw the list item
      res = selection->Collapse(newBQ,0);
      if (NS_FAILED(res)) return res;
      nsAutoString theText(" ");
      res = InsertText(theText);
      if (NS_FAILED(res)) return res;
      // reposition selection to before the space character
      res = GetStartNodeAndOffset(selection, &node, &offset);
      if (NS_FAILED(res)) return res;
      res = selection->Collapse(node,0);
      if (NS_FAILED(res)) return res;
    }
  }
  
  return res;
}

//TODO: IMPLEMENT ALIGNMENT!

NS_IMETHODIMP
nsHTMLEditor::Align(const nsString& aAlignType)
{
  nsAutoEditBatch beginBatching(this);
  nsCOMPtr<nsIDOMNode> node;
  PRBool cancel= PR_FALSE;
  
  // Find out if the selection is collapsed:
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsTextRulesInfo ruleInfo(nsHTMLEditRules::kAlign);
  ruleInfo.alignType = &aAlignType;
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel);

  return res;
}


NS_IMETHODIMP
nsHTMLEditor::GetElementOrParentByTagName(const nsString &aTagName, nsIDOMNode *aNode, nsIDOMElement** aReturn)
{
  if (aTagName.Length() == 0 || !aReturn )
    return NS_ERROR_NULL_POINTER;
  
  nsresult res = NS_OK;
  // If no node supplied, use anchor node of current selection
  if (!aNode)
  {
    nsCOMPtr<nsIDOMSelection>selection;
    res = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    if (!selection) return NS_ERROR_NULL_POINTER;

    //TODO: Do I need to release the node?
    if(NS_FAILED(selection->GetAnchorNode(&aNode)))
      return NS_ERROR_FAILURE;
  }
   
  nsAutoString TagName = aTagName;
  TagName.ToLowerCase();
  PRBool getLink = IsLink(TagName);
  PRBool getNamedAnchor = IsNamedAnchor(TagName);
  if ( getLink || getNamedAnchor)
  {
    TagName = "a";  
  }
  PRBool findTableCell = aTagName.EqualsIgnoreCase("td");

  // default is null - no element found
  *aReturn = nsnull;
  
  nsCOMPtr<nsIDOMNode> currentNode = aNode;
  nsCOMPtr<nsIDOMNode> parent;
  PRBool bNodeFound = PR_FALSE;

  while (PR_TRUE)
  {
    // Test if we have a link (an anchor with href set)
    if ( (getLink && IsLinkNode(currentNode)) ||
         (getNamedAnchor && IsNamedAnchorNode(currentNode)) )
    {
      bNodeFound = PR_TRUE;
      break;
    } else {
      nsAutoString currentTagName; 
      currentNode->GetNodeName(currentTagName);
      // Table cells are a special case:
      // Match either "td" or "th" for them
      if (currentTagName.EqualsIgnoreCase(TagName) ||
          (findTableCell && currentTagName.EqualsIgnoreCase("th")))
      {
        bNodeFound = PR_TRUE;
        break;
      } 
    }
    // Search up the parent chain
    // We should never fail because of root test below, but lets be safe
		// XXX: ERROR_HANDLING error return code lost
    if (!NS_SUCCEEDED(currentNode->GetParentNode(getter_AddRefs(parent))) || !parent)
      break;

    // Stop searching if parent is a body tag
    nsAutoString parentTagName;
    parent->GetNodeName(parentTagName);
    // Note: Originally used IsRoot to stop at table cells,
    //  but that's too messy when you are trying to find the parent table
    //PRBool isRoot;
    //if (!NS_SUCCEEDED(IsRootTag(parentTagName, isRoot)) || isRoot)
    if(parentTagName.EqualsIgnoreCase("body"))
      break;

    currentNode = parent;
  }
  if (bNodeFound)
  {
    nsCOMPtr<nsIDOMElement> currentElement = do_QueryInterface(currentNode);
    if (currentElement)
    {
      *aReturn = currentElement;
      // Getters must addref
      NS_ADDREF(*aReturn);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetSelectedElement(const nsString& aTagName, nsIDOMElement** aReturn)
{
  if (!aReturn )
    return NS_ERROR_NULL_POINTER;
  
  // default is null - no element found
  *aReturn = nsnull;
  
  nsAutoString TagName = aTagName;
  TagName.ToLowerCase();
  // Empty string indicates we should match any element tag
  PRBool anyTag = (TagName == "");
  
  //Note that this doesn't need to go through the transaction system

  nsresult res=NS_ERROR_NOT_INITIALIZED;
  //PRBool first=PR_TRUE;
  nsCOMPtr<nsIDOMSelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  
  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);
  nsCOMPtr<nsIDOMElement> selectedElement;
  PRBool bNodeFound = PR_FALSE;

  if (IsLink(TagName))
  {
    // Link tag is a special case - we return the anchor node
    //  found for any selection that is totally within a link,
    //  included a collapsed selection (just a caret in a link)
    nsCOMPtr<nsIDOMNode> anchorNode;
    res = selection->GetAnchorNode(getter_AddRefs(anchorNode));
		if (NS_FAILED(res)) return res;
    PRInt32 anchorOffset = -1;
    if (anchorNode)
      selection->GetAnchorOffset(&anchorOffset);
    
    nsCOMPtr<nsIDOMNode> focusNode;
    res = selection->GetFocusNode(getter_AddRefs(focusNode));
		if (NS_FAILED(res)) return res;
    PRInt32 focusOffset = -1;
    if (focusNode)
      selection->GetFocusOffset(&focusOffset);

    // Link node must be the same for both ends of selection
    if (NS_SUCCEEDED(res) && anchorNode)
    {
#ifdef DEBUG_cmanske
      {
      nsAutoString name;
      anchorNode->GetNodeName(name);
      printf("GetSelectedElement: Anchor node of selection: ");
      wprintf(name.GetUnicode());
      printf(" Offset: %d\n", anchorOffset);
      focusNode->GetNodeName(name);
      printf("Focus node of selection: ");
      wprintf(name.GetUnicode());
      printf(" Offset: %d\n", focusOffset);
      }
#endif
      nsCOMPtr<nsIDOMElement> parentLinkOfAnchor;
      res = GetElementOrParentByTagName("href", anchorNode, getter_AddRefs(parentLinkOfAnchor));
			// XXX: ERROR_HANDLING  can parentLinkOfAnchor be null?
      if (NS_SUCCEEDED(res) && parentLinkOfAnchor)
      {
        if (isCollapsed)
        {
          // We have just a caret in the link
          bNodeFound = PR_TRUE;
        } else if(focusNode) 
        {  // Link node must be the same for both ends of selection
          nsCOMPtr<nsIDOMElement> parentLinkOfFocus;
          res = GetElementOrParentByTagName("href", focusNode, getter_AddRefs(parentLinkOfFocus));
          if (NS_SUCCEEDED(res) && parentLinkOfFocus == parentLinkOfAnchor)
            bNodeFound = PR_TRUE;
        }
      
        // We found a link node parent
        if (bNodeFound) {
          // GetElementOrParentByTagName addref'd this, so we don't need to do it here
          *aReturn = parentLinkOfAnchor;
          return NS_OK;
        }
      }
      else if (anchorOffset >= 0)  // Check if link node is the only thing selected
      {
        nsCOMPtr<nsIDOMNode> anchorChild;
        anchorChild = GetChildAt(anchorNode,anchorOffset);
        if (anchorChild && IsLinkNode(anchorChild) && 
            (anchorNode == focusNode) && focusOffset == (anchorOffset+1))
        {
          selectedElement = do_QueryInterface(anchorChild);
          bNodeFound = PR_TRUE;
        }
      }
    }
  } 

  if (!bNodeFound && !isCollapsed)   // Don't bother to examine selection if it is collapsed
  {
    nsCOMPtr<nsIEnumerator> enumerator;
    res = selection->GetEnumerator(getter_AddRefs(enumerator));
		// XXX: ERROR_HANDLING  unclear what to do here, should an error just be returned if enumerator is null or res failed?
    if (NS_SUCCEEDED(res) && enumerator)
    {
      enumerator->First(); 
      nsCOMPtr<nsISupports> currentItem;
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if ((NS_SUCCEEDED(res)) && currentItem)
      {
        nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
        nsCOMPtr<nsIContentIterator> iter;
        res = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                                    nsIContentIterator::GetIID(), 
                                                    getter_AddRefs(iter));
				// XXX: ERROR_HANDLING  XPCOM usage
        if ((NS_SUCCEEDED(res)) && iter)
        {
          iter->Init(range);
          // loop through the content iterator for each content node
          nsCOMPtr<nsIContent> content;
          while (NS_COMFALSE == iter->IsDone())
          {
            res = iter->CurrentNode(getter_AddRefs(content));
            // Note likely!
            if (NS_FAILED(res))
              return NS_ERROR_FAILURE;

             // Query interface to cast nsIContent to nsIDOMNode
             //  then get tagType to compare to  aTagName
             // Clone node of each desired type and append it to the aDomFrag
            selectedElement = do_QueryInterface(content);
            if (selectedElement)
            {
              // If we already found a node, then we have another element,
              //   so don't return an element
              if (bNodeFound)
              {
                //bNodeFound;
                break;
              }

              nsAutoString domTagName;
              selectedElement->GetNodeName(domTagName);
              domTagName.ToLowerCase();

              if (anyTag)
              {
                // Get name of first selected element
                selectedElement->GetTagName(TagName);
                TagName.ToLowerCase();
                anyTag = PR_FALSE;
              }

              // The "A" tag is a pain,
              //  used for both link(href is set) and "Named Anchor"
              nsCOMPtr<nsIDOMNode> selectedNode = do_QueryInterface(selectedElement);
              if ( (IsLink(TagName) && IsLinkNode(selectedNode)) ||
                   (IsNamedAnchor(TagName) && IsNamedAnchorNode(selectedNode)) )
              {
                bNodeFound = PR_TRUE;
              } else if (TagName == domTagName) { // All other tag names are handled here
                bNodeFound = PR_TRUE;
              }
              if (!bNodeFound)
              {
                // Check if node we have is really part of the selection???
                break;
              }
            }
            iter->Next();
          }
        }
      } else {
        // Should never get here?
        isCollapsed = PR_TRUE;
        printf("isCollapsed was FALSE, but no elements found in selection\n");
      }
    } else {
      printf("Could not create enumerator for GetSelectionProperties\n");
    }
  }
  if (bNodeFound)
  {
    
    *aReturn = selectedElement;  
    // Getters must addref
    NS_ADDREF(*aReturn);
  }
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::CreateElementWithDefaults(const nsString& aTagName, nsIDOMElement** aReturn)
{
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  if (aReturn)
    *aReturn = nsnull;

  if (aTagName == "" || !aReturn)
    return NS_ERROR_NULL_POINTER;
    
  nsAutoString TagName = aTagName;
  TagName.ToLowerCase();
  nsAutoString realTagName;

  if (IsLink(TagName) || IsNamedAnchor(TagName))
  {
    realTagName = "a";
  } else {
    realTagName = TagName;
  }
  //We don't use editor's CreateElement because we don't want to 
  //  go through the transaction system

  nsCOMPtr<nsIDOMElement>newElement;
  res = mDoc->CreateElement(realTagName, getter_AddRefs(newElement));
  if (NS_FAILED(res) || !newElement)
    return NS_ERROR_FAILURE;

  // Set default values for new elements
  if (TagName.Equals("hr"))
  {
    // Hard coded defaults in case there's no prefs
    nsAutoString align("center");
    nsAutoString width("100%");
    nsAutoString height("2");
    PRBool bNoShade = PR_FALSE;

    if (mPrefs)
    {
      char buf[16];
      PRInt32 iAlign;
      // Currently using 0=left, 1=center, and 2=right
			// XXX: ERROR_HANDLING  if these results are intentionally thrown away, it should be documented here
      if( NS_SUCCEEDED(mPrefs->GetIntPref("editor.hrule.align", &iAlign)))
      {
        switch (iAlign) {
          case 0:
            align = "left";
            break;
          case 2:
            align = "right";
            break;
        }
      }
      PRInt32 iHeight;
      PRUint32 count;
      if( NS_SUCCEEDED(mPrefs->GetIntPref("editor.hrule.height", &iHeight)))
      {
        count = PR_snprintf(buf, 16, "%d", iHeight);
        if (count > 0)
        {
          height = buf;
        }
      }
      PRInt32 iWidth;
      PRBool  bPercent;
      if( NS_SUCCEEDED(mPrefs->GetIntPref("editor.hrule.width", &iWidth)) &&
          NS_SUCCEEDED(mPrefs->GetBoolPref("editor.hrule.width_percent", &bPercent)))
      {
        count = PR_snprintf(buf, 16, "%d", iWidth);
        if (count > 0)
        {
          width = buf;
          if (bPercent)
            width.Append("%");
        }
      }
      PRBool  bShading;
      if (NS_SUCCEEDED(mPrefs->GetBoolPref("editor.hrule.shading", &bShading)))
      {
        bNoShade = !bShading;
      }
    }
    newElement->SetAttribute("align", align);
    newElement->SetAttribute("height", height);
    newElement->SetAttribute("width", width);
    if (bNoShade)    
      newElement->SetAttribute("noshade", "");

  } else if (TagName.Equals("table"))
  {
    newElement->SetAttribute("cellpadding","2");
    newElement->SetAttribute("cellspacing","2");
    newElement->SetAttribute("width","50%");
    newElement->SetAttribute("border","1");
  } else if (TagName.Equals("tr"))
  {
    newElement->SetAttribute("valign","top");
  } else if (TagName.Equals("td"))
  {
    newElement->SetAttribute("valign","top");

    // Insert the default space in a cell so border displays
    nsCOMPtr<nsIDOMNode> newCellNode = do_QueryInterface(newElement);
    if (newCellNode)
    {
      // TODO: This should probably be in the RULES code or 
      //       preference based for "should we add the nbsp"
      nsCOMPtr<nsIDOMText>newTextNode;
      nsString space;
      // Set contents to the &nbsp character by concatanating the char code
      space += nbsp;
      // If we fail here, we return NS_OK anyway, since we have an OK cell node
      nsresult result = mDoc->CreateTextNode(space, getter_AddRefs(newTextNode));
			if (NS_FAILED(result)) return result;
			if (!newTextNode) return NS_ERROR_NULL_POINTER;

      nsCOMPtr<nsIDOMNode>resultNode;
      result = newCellNode->AppendChild(newTextNode, getter_AddRefs(resultNode));
    }
  }
  // ADD OTHER DEFAULT ATTRIBUTES HERE

  if (NS_SUCCEEDED(res))
  {
    *aReturn = newElement;
  }

  return res;
}

NS_IMETHODIMP
nsHTMLEditor::SaveHLineSettings(nsIDOMElement* aElement)
{
  nsresult res=NS_ERROR_NOT_INITIALIZED;
  if (!aElement || !mPrefs)
    return res;

  nsAutoString align, width, height, noshade;
  res = NS_ERROR_UNEXPECTED;
	// XXX: ERROR_HANDLING  if return codes are intentionally thrown away, it should be documented here
	//                      it looks like if any GetAttribute call failes, an error is returned
	//                      is that the desired behavior?
  if (NS_SUCCEEDED(aElement->GetAttribute("align", align)) &&
      NS_SUCCEEDED(aElement->GetAttribute("height", height)) &&
      NS_SUCCEEDED(aElement->GetAttribute("width", width)) &&
      NS_SUCCEEDED(aElement->GetAttribute("noshade", noshade)))
  {
    PRInt32 iAlign = 0;
    if (align == "center")
      iAlign = 1;
    else if (align == "right")
      iAlign = 2;
    mPrefs->SetIntPref("editor.hrule.align", iAlign);

    PRInt32 errorCode;
    PRInt32 iHeight = height.ToInteger(&errorCode);
    
    if (errorCode == NS_OK && iHeight > 0)
      mPrefs->SetIntPref("editor.hrule.height", iHeight);

    PRInt32 iWidth = width.ToInteger(&errorCode);
    if (errorCode == NS_OK && iWidth > 0) {
      mPrefs->SetIntPref("editor.hrule.width", iWidth);
      mPrefs->SetBoolPref("editor.hrule.width_percent", (width.Find("%") > 0));
    }

    mPrefs->SetBoolPref("editor.hrule.shading", (noshade == ""));
    res = NS_OK;
  }
        
  return res;
}

NS_IMETHODIMP
nsHTMLEditor::InsertLinkAroundSelection(nsIDOMElement* aAnchorElement)
{
  nsresult res=NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMSelection> selection;

  // DON'T RETURN EXCEPT AT THE END -- WE NEED TO RELEASE THE aAnchorElement
  if (!aAnchorElement)
    goto DELETE_ANCHOR;		// DON'T USE GOTO IN C++!

  // We must have a real selection
  res = GetSelection(getter_AddRefs(selection));
  if (!selection)
  {
    res = NS_ERROR_NULL_POINTER;
  }
  if (NS_FAILED(res) || !selection)
    goto DELETE_ANCHOR;

  PRBool isCollapsed;
  res = selection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res))
    isCollapsed = PR_TRUE;
  
  if (isCollapsed)
  {
    printf("InsertLinkAroundSelection called but there is no selection!!!\n");     
    res = NS_OK;
  } else {
    // Be sure we were given an anchor element
    nsCOMPtr<nsIDOMHTMLAnchorElement> anchor = do_QueryInterface(aAnchorElement);
    if (anchor)
    {
      nsAutoString href;
			// XXX: ERROR_HANDLING   return code lost
      if (NS_SUCCEEDED(anchor->GetHref(href)) && href.GetUnicode() && href.Length() > 0)      
      {
        nsAutoEditBatch beginBatching(this);
        const nsString attribute("href");
        SetInlineProperty(nsIEditProperty::a, &attribute, &href);
        //TODO: Enumerate through other properties of the anchor tag
        // and set those as well. 
        // Optimization: Modify SetTextProperty to set all attributes at once?
      }
    }
  }
DELETE_ANCHOR:
  // We don't insert the element created in CreateElementWithDefaults 
  //   into the document like we do in InsertElement,
  //   so shouldn't we have to do this here?
  //  It crashes in JavaScript if we do this!
  //NS_RELEASE(aAnchorElement);
  return res;
}

// XXX: this method sets the attribute on the body element directly, 
//      and is not undoable.  It should go through the transaction system!
NS_IMETHODIMP nsHTMLEditor::SetBackgroundColor(const nsString& aColor)
{
//  nsresult result;
  NS_PRECONDITION(mDoc, "Missing Editor DOM Document");
  
  // TODO: Check selection for Cell, Row, Column or table and do color on appropriate level
  // For initial testing, just set the background on the BODY tag (the document's background)

  // TODO: Check selection for Cell, Row, Column or table and do color on appropriate level
  // For initial testing, just set the background on the BODY tag (the document's background)

  // Set the background color attribute on the body tag
  nsCOMPtr<nsIDOMElement> bodyElement;
  nsresult res = nsEditor::GetBodyElement(getter_AddRefs(bodyElement));
  if (!bodyElement) res = NS_ERROR_NULL_POINTER;
  if (NS_SUCCEEDED(res))
  {
    nsAutoEditBatch beginBatching(this);
		// XXX: ERROR_HANDLING  should this be "res = SetAttribute..."
    bodyElement->SetAttribute("bgcolor", aColor);
  }
  
  return res;
}

NS_IMETHODIMP nsHTMLEditor::SetBodyAttribute(const nsString& aAttribute, const nsString& aValue)
{
  nsresult res;
  // TODO: Check selection for Cell, Row, Column or table and do color on appropriate level

  NS_ASSERTION(mDoc, "Missing Editor DOM Document");
  
  // Set the background color attribute on the body tag
  nsCOMPtr<nsIDOMElement> bodyElement;

  res = nsEditor::GetBodyElement(getter_AddRefs(bodyElement));
  if (!bodyElement) res = NS_ERROR_NULL_POINTER;
  if (NS_SUCCEEDED(res))
  {
    // Use the editor's method which goes through the transaction system
    // XXX: ERROR_HANDLING  should this be "res = SetAttribute..."
    SetAttribute(bodyElement, aAttribute, aValue);
  }
  return res;
}

/*
NS_IMETHODIMP nsHTMLEditor::MoveSelectionUp(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::MoveSelectionDown(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::MoveSelectionNext(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::MoveSelectionPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::SelectNext(nsIAtom *aIncrement, PRBool aExtendSelection) 
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::SelectPrevious(nsIAtom *aIncrement, PRBool aExtendSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::ScrollUp(nsIAtom *aIncrement)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::ScrollDown(nsIAtom *aIncrement)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLEditor::ScrollIntoView(PRBool aScrollToBegin)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
*/



NS_IMETHODIMP
nsHTMLEditor::GetDocumentLength(PRInt32 *aCount)                                              
{
  if (!aCount) { return NS_ERROR_NULL_POINTER; }
  nsresult result;
  // initialize out params
  *aCount = 0;
  
  nsCOMPtr<nsIDOMSelection> sel;
  result = GetSelection(getter_AddRefs(sel));
  if (NS_FAILED(result)) return result;
  if (!sel) return NS_ERROR_NULL_POINTER;

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
  return result;
}

NS_IMETHODIMP nsHTMLEditor::SetMaxTextLength(PRInt32 aMaxTextLength)
{
  mMaxTextLength = aMaxTextLength;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLEditor::GetMaxTextLength(PRInt32& aMaxTextLength)
{
  aMaxTextLength = mMaxTextLength;
  return NS_OK;
}


#ifdef XP_MAC
#pragma mark -
#pragma mark --- nsIEditorStyleSheets methods ---
#pragma mark -
#endif


NS_IMETHODIMP
nsHTMLEditor::AddStyleSheet(nsICSSStyleSheet* aSheet)
{
	AddStyleSheetTxn* txn;
	nsresult rv = CreateTxnForAddStyleSheet(aSheet, &txn);
  if (!txn) rv = NS_ERROR_NULL_POINTER;
	if (NS_SUCCEEDED(rv))
	{
	  rv = Do(txn);
	  if (NS_SUCCEEDED(rv))
	  {
	    mLastStyleSheet = do_QueryInterface(aSheet);		// save it so we can remove before applying the next one
	  }
	}
	
	return rv;
}


NS_IMETHODIMP
nsHTMLEditor::RemoveStyleSheet(nsICSSStyleSheet* aSheet)
{
	RemoveStyleSheetTxn* txn;
	nsresult rv = CreateTxnForRemoveStyleSheet(aSheet, &txn);
  if (!txn) rv = NS_ERROR_NULL_POINTER;
	if (NS_SUCCEEDED(rv))
	{
	  rv = Do(txn);
	  if (NS_SUCCEEDED(rv))
	  {
	    mLastStyleSheet = nsnull;				// forget it
	  }
	}
	
	return rv;
}


NS_IMETHODIMP nsHTMLEditor::ApplyStyleSheet(const nsString& aURL)
{
  // XXX: Note that this is not an undo-able action yet!

  nsresult rv   = NS_OK;
  nsIURI* uaURL = 0;

#ifndef NECKO
  rv = NS_NewURL(&uaURL, aURL);
#else
  rv = NS_NewURI(&uaURL, aURL);
#endif // NECKO

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDocument> document;

    rv = mPresShell->GetDocument(getter_AddRefs(document));

    if (NS_SUCCEEDED(rv)) {
      if (document) {
        nsCOMPtr<nsIHTMLContentContainer> container = do_QueryInterface(document);

        if (container) {
          nsICSSLoader *cssLoader         = 0;
          nsICSSStyleSheet *cssStyleSheet = 0;

          rv = container->GetCSSLoader(cssLoader);

          if (NS_SUCCEEDED(rv)) {
            if (cssLoader) {
              PRBool complete;

              rv = cssLoader->LoadAgentSheet(uaURL, cssStyleSheet, complete,
                                             ApplyStyleSheetToPresShellDocument,
                                             this);

              if (NS_SUCCEEDED(rv)) {
                if (complete) {
                  if (cssStyleSheet) {
                    ApplyStyleSheetToPresShellDocument(cssStyleSheet,
                                                       this);
                  }
                  else
                    rv = NS_ERROR_NULL_POINTER;
                }
                    
                //
                // If not complete, we will be notified later
                // with a call to AddStyleSheetToEditorDocument().
                //
              }
            }
            else
              rv = NS_ERROR_NULL_POINTER;
          }
        }
        else
          rv = NS_ERROR_NULL_POINTER;
      }
      else
        rv = NS_ERROR_NULL_POINTER;
    }

    NS_RELEASE(uaURL);
  }

  return rv;
}


#ifdef XP_MAC
#pragma mark -
#pragma mark --- nsIEditorMailSupport methods ---
#pragma mark -
#endif

//
// Get the wrap width for the first PRE tag in the document.
// If no PRE tag, throw an error.
//
NS_IMETHODIMP nsHTMLEditor::GetBodyWrapWidth(PRInt32 *aWrapColumn)
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
NS_IMETHODIMP nsHTMLEditor::SetBodyWrapWidth(PRInt32 aWrapColumn)
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



// 
// HTML PasteAsQuotation: Paste in a blockquote type=cite
//
NS_IMETHODIMP nsHTMLEditor::PasteAsQuotation()
{
  nsAutoString citation("");
  return PasteAsCitedQuotation(citation);
}

NS_IMETHODIMP nsHTMLEditor::PasteAsCitedQuotation(const nsString& aCitation)
{
  nsAutoEditBatch beginBatching(this);
  nsCOMPtr<nsIDOMNode> newNode;
  nsAutoString tag("blockquote");
  nsresult res = DeleteSelectionAndCreateNode(tag, getter_AddRefs(newNode));
  if (NS_FAILED(res)) return res;
  if (!newNode) return NS_ERROR_NULL_POINTER;

  // Try to set type=cite.  Ignore it if this fails.
  nsCOMPtr<nsIDOMElement> newElement (do_QueryInterface(newNode));
  if (newElement)
  {
    nsAutoString type ("type");
    nsAutoString cite ("cite");
    newElement->SetAttribute(type, cite);
  }

  // Set the selection to the underneath the node we just inserted:
  nsCOMPtr<nsIDOMSelection> selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res) || !selection)
  {
#ifdef DEBUG_akkana
    printf("Can't get selection!");
#endif
  }
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;

  res = selection->Collapse(newNode, 0);
  if (NS_FAILED(res))
  {
#ifdef DEBUG_akkana
    printf("Couldn't collapse");
#endif
    // XXX: error result:  should res be returned here?
  }

  res = Paste();
  return res;
}

//
// Paste a plaintext quotation
//
NS_IMETHODIMP nsHTMLEditor::PasteAsPlaintextQuotation()
{
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
  if (NS_SUCCEEDED(rv) && trans)
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
    rv = trans->GetTransferData(&flavor, (void **)&str, &len);
    if (NS_SUCCEEDED(rv) && str && len > 0)
    {
      nsString stuffToPaste;
      stuffToPaste.SetString(str, len);
      rv = InsertAsPlaintextQuotation(stuffToPaste);
    }
  }
  nsServiceManager::ReleaseService(kCClipboardCID, clipboard);

  return rv;
}

NS_IMETHODIMP nsHTMLEditor::InsertAsQuotation(const nsString& aQuotedText)
{
  nsAutoString citation ("");
  return InsertAsCitedQuotation(aQuotedText, citation);
}

// text insert.
NS_IMETHODIMP nsHTMLEditor::InsertAsPlaintextQuotation(const nsString& aQuotedText)
{
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

NS_IMETHODIMP nsHTMLEditor::InsertAsCitedQuotation(const nsString& aQuotedText,
                                                   const nsString& aCitation)
{
  nsAutoEditBatch beginBatching(this);
  nsCOMPtr<nsIDOMNode> newNode;
  nsAutoString tag("blockquote");
  nsresult res = DeleteSelectionAndCreateNode(tag, getter_AddRefs(newNode));
  if (NS_FAILED(res)) return res;
  if (!newNode) return NS_ERROR_NULL_POINTER;

  // Try to set type=cite.  Ignore it if this fails.
  nsCOMPtr<nsIDOMElement> newElement (do_QueryInterface(newNode));
  if (newElement)
  {
    nsAutoString type ("type");
    nsAutoString cite ("cite");
    newElement->SetAttribute(type, cite);

    if (aCitation.Length() > 0)
      newElement->SetAttribute(cite, aCitation);
  }

  res = InsertHTML(aQuotedText);
  return res;
}


NS_IMETHODIMP
nsHTMLEditor::GetEmbeddedObjects(nsISupportsArray** aNodeList)
{
  if (!aNodeList)
    return NS_ERROR_NULL_POINTER;

  nsresult res;

  res = NS_NewISupportsArray(aNodeList);
  if (NS_FAILED(res)) return res;
  if (!*aNodeList) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIContentIterator> iter;
  res = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                           nsIContentIterator::GetIID(), 
                                           getter_AddRefs(iter));
  if (!iter) return NS_ERROR_NULL_POINTER;
  if ((NS_SUCCEEDED(res)))
  {
    // get the root content
    nsCOMPtr<nsIContent> rootContent;

    nsCOMPtr<nsIDOMDocument> domdoc;
    nsEditor::GetDocument(getter_AddRefs(domdoc));
    if (!domdoc)
      return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIDocument> doc (do_QueryInterface(domdoc));
    if (!doc)
      return NS_ERROR_UNEXPECTED;

    rootContent = doc->GetRootContent();

    iter->Init(rootContent);

    // loop through the content iterator for each content node
    while (NS_COMFALSE == iter->IsDone())
    {
      nsCOMPtr<nsIContent> content;
      res = iter->CurrentNode(getter_AddRefs(content));
      if (NS_FAILED(res))
        break;
      nsCOMPtr<nsIDOMNode> node (do_QueryInterface(content));
      if (node)
      {
        nsAutoString tagName;
        node->GetNodeName(tagName);
        tagName.ToLowerCase();

        // See if it's an image or an embed
        if (tagName == "img" || tagName == "embed")
          (*aNodeList)->AppendElement(node);
        else if (tagName == "a")
        {
          // XXX Only include links if they're links to file: URLs
          nsCOMPtr<nsIDOMHTMLAnchorElement> anchor (do_QueryInterface(content));
          if (anchor)
          {
            nsAutoString href;
            if (NS_SUCCEEDED(anchor->GetHref(href)))
              if (href.Compare("file:", PR_TRUE, 5) == 0)
                (*aNodeList)->AppendElement(node);
          }
        }
      }
      iter->Next();
    }
  }

  return res;
}


#ifdef XP_MAC
#pragma mark -
#pragma mark --- nsIEditor overrides ---
#pragma mark -
#endif

NS_IMETHODIMP nsHTMLEditor::Cut()
{
  nsCOMPtr<nsIDOMSelection> selection;
  nsresult res = mPresShell->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
  if (!NS_SUCCEEDED(res))
    return res;

  PRBool isCollapsed;
  if (NS_SUCCEEDED(selection->GetIsCollapsed(&isCollapsed)) && isCollapsed)
    return NS_ERROR_NOT_AVAILABLE;

  res = Copy();
  if (NS_SUCCEEDED(res))
    res = DeleteSelection(eDoNothing);
  return res;
}

NS_IMETHODIMP nsHTMLEditor::Copy()
{
  //printf("nsEditor::Copy\n");

  return mPresShell->DoCopy();
}

NS_IMETHODIMP nsHTMLEditor::Paste()
{
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
  if (NS_SUCCEEDED(rv))
  {
    // Get the nsITransferable interface for getting the data from the clipboard
    if (trans)
    {
      // Create the desired DataFlavor for the type of data
      // we want to get out of the transferable
      nsAutoString imageFlavor(kJPEGImageMime);
      nsAutoString htmlFlavor(kHTMLMime);
      nsAutoString textFlavor(kTextMime);

      if ((mFlags & eEditorPlaintextMask) == 0)  // This should only happen in html editors, not plaintext
      {
        trans->AddDataFlavor(&imageFlavor);
        trans->AddDataFlavor(&htmlFlavor);
      }
      trans->AddDataFlavor(&textFlavor);

      // Get the Data from the clipboard
      if (NS_SUCCEEDED(clipboard->GetData(trans)))
      {
        nsAutoString flavor;
        char *       data;
        PRUint32     len;
        if (NS_SUCCEEDED(trans->GetAnyTransferData(&flavor, (void **)&data, &len)))
        {
#ifdef DEBUG_akkana
          printf("Got flavor [%s]\n", flavor.ToNewCString());
#endif
          if (flavor.Equals(htmlFlavor))
          {
            if (data && len > 0) // stuffToPaste is ready for insertion into the content
            {
              stuffToPaste.SetString((PRUnichar *)data, len/2);
              rv = InsertHTML(stuffToPaste);
            }
          }
          else if (flavor.Equals(textFlavor))
          {
            if (data && len > 0) // stuffToPaste is ready for insertion into the content
            {
              stuffToPaste.SetString(data, len);
              rv = InsertText(stuffToPaste);
            }
          }
          else if (flavor.Equals(imageFlavor))
          {
            // Insert Image code here
            printf("Don't know how to insert an image yet!\n");
            //nsIImage* image = (nsIImage *)data;
            //NS_RELEASE(image);
            rv = NS_ERROR_FAILURE; // for now give error code
          }
        }

      }
    }
  }
  nsServiceManager::ReleaseService(kCClipboardCID, clipboard);

  return rv;
}

NS_IMETHODIMP nsHTMLEditor::OutputToString(nsString& aOutputString,
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

NS_IMETHODIMP nsHTMLEditor::OutputToStream(nsIOutputStream* aOutputStream,
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


NS_IMETHODIMP
nsHTMLEditor::DebugUnitTests(PRInt32 *outNumTests, PRInt32 *outNumTestsFailed)
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


#ifdef XP_MAC
#pragma mark -
#pragma mark --- StyleSheet utils ---
#pragma mark -
#endif


NS_IMETHODIMP
nsHTMLEditor::ReplaceStyleSheet(nsICSSStyleSheet *aNewSheet)
{
  nsresult  rv = NS_OK;
  
  BeginTransaction();

  if (mLastStyleSheet)
  {
    rv = RemoveStyleSheet(mLastStyleSheet);
    //XXX: rv is ignored here, why?
  }

  rv = AddStyleSheet(aNewSheet);
  
  EndTransaction();

  return rv;
}



/* static callback */
void nsHTMLEditor::ApplyStyleSheetToPresShellDocument(nsICSSStyleSheet* aSheet, void *aData)
{
  nsresult rv = NS_OK;

  nsHTMLEditor *editor = NS_STATIC_CAST(nsHTMLEditor*, aData);
  if (editor)
  {
    rv = editor->ReplaceStyleSheet(aSheet);
  }
  
  // XXX: we lose the return value here. Set a flag in the editor?
}


#ifdef XP_MAC
#pragma mark -
#pragma mark --- Random methods ---
#pragma mark -
#endif


NS_IMETHODIMP nsHTMLEditor::GetLayoutObject(nsIDOMNode *aNode, nsISupports **aLayoutObject)
{
  nsresult result = NS_ERROR_FAILURE;  // we return an error unless we get the index
  if( mPresShell != nsnull )
  {
    if ((nsnull!=aNode))
    { // get the content interface
      nsCOMPtr<nsIContent> nodeAsContent( do_QueryInterface(aNode) );
      if (nodeAsContent)
      { // get the frame from the content interface
        //Note: frames are not ref counted, so don't use an nsCOMPtr
        *aLayoutObject = nsnull;
        result = mPresShell->GetLayoutObjectFor(nodeAsContent, aLayoutObject);
      }
    }
    else {
      result = NS_ERROR_NULL_POINTER;
    }
  }
  return result;
}


NS_IMETHODIMP
nsHTMLEditor::SetTypeInStateForProperty(TypeInState    &aTypeInState, 
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
      GetInlineProperty(aPropName, aAttribute, nsnull, first, any, all); // operates on current selection
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
        GetInlineProperty(aPropName, aAttribute, aValue, first, any, all); // operates on current selection
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


// this will NOT find aAttribute unless aAttribute has a non-null value
// so singleton attributes like <Table border> will not be matched!
void nsHTMLEditor::IsTextPropertySetByContent(nsIDOMNode     *aNode,
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
	if (NS_FAILED(result)) return;
  while (parent)
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

void nsHTMLEditor::IsTextStyleSet(nsIStyleContext *aSC, 
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


// this is a complete ripoff from nsTextEditor::GetTextSelectionOffsetsForRange
// the two should use common code, or even just be one method
nsresult nsHTMLEditor::GetTextSelectionOffsets(nsIDOMSelection *aSelection,
                                               PRInt32 &aStartOffset, 
                                               PRInt32 &aEndOffset)
{
	if(!aSelection) { return NS_ERROR_NULL_POINTER; }
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
	if (NS_FAILED(result)) return result;
	if (!enumerator) return NS_ERROR_NULL_POINTER;

  // don't use "result" in this block
  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  nsresult findParentResult = enumerator->CurrentItem(getter_AddRefs(currentItem));
  if ((NS_SUCCEEDED(findParentResult)) && (currentItem))
  {
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    range->GetCommonParent(getter_AddRefs(parentNode));
  }
  else {
		parentNode = do_QueryInterface(startNode);
	}


  nsCOMPtr<nsIContentIterator> iter;
  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                              nsIContentIterator::GetIID(), 
                                              getter_AddRefs(iter));
	if (NS_FAILED(result)) return result;
	if (!iter) return NS_ERROR_NULL_POINTER;

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
  return result;
}


NS_IMETHODIMP
nsHTMLEditor::GetTextSelectionOffsetsForRange(nsIDOMSelection *aSelection,
                                              nsIDOMNode **aParent,
                                              PRInt32     &aStartOffset, 
                                              PRInt32     &aEndOffset)
{
	if (!aSelection) { return NS_ERROR_NULL_POINTER; }
	aStartOffset = aEndOffset = 0;
  nsresult result;

  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset, endOffset;
  aSelection->GetAnchorNode(getter_AddRefs(startNode));
  aSelection->GetAnchorOffset(&startOffset);
  aSelection->GetFocusNode(getter_AddRefs(endNode));
  aSelection->GetFocusOffset(&endOffset);

  nsCOMPtr<nsIEnumerator> enumerator;
  result = aSelection->GetEnumerator(getter_AddRefs(enumerator));
	if (NS_FAILED(result)) return result;
	if (!enumerator) return NS_ERROR_NULL_POINTER;
  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  result = enumerator->CurrentItem(getter_AddRefs(currentItem));
  if ((NS_SUCCEEDED(result)) && currentItem)
  {
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    range->GetCommonParent(aParent);
  }

  nsCOMPtr<nsIContentIterator> iter;
  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                              nsIContentIterator::GetIID(), 
                                              getter_AddRefs(iter));
	if (NS_FAILED(result)) return result;
	if (!iter) return NS_ERROR_NULL_POINTER;

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
	return result;
}

void nsHTMLEditor::ResetTextSelectionForRange(nsIDOMNode *aParent,
                                              PRInt32     aStartOffset,
                                              PRInt32     aEndOffset,
                                              nsIDOMSelection *aSelection)
{
	if (!aParent || !aSelection) { return; }	// XXX: should return an error
  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset, endOffset;

  nsresult result;
  nsCOMPtr<nsIContentIterator> iter;
  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                              nsIContentIterator::GetIID(), 
                                              getter_AddRefs(iter));
	if (NS_FAILED(result)) return;
	if (!iter) return;
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

#pragma mark -

//================================================================
// HTML Editor methods
//
// Note: Table Editing methods are implemented in EditTable.cpp
//

NS_IMETHODIMP 
nsHTMLEditor::ReParentContentOfNode(nsIDOMNode *aNode, 
                                    nsString &aParentTag,
                                    BlockTransformationType aTransformation)
{
  if (!aNode) { return NS_ERROR_NULL_POINTER; }
  if (gNoisy) 
  { 
    char *tag = aParentTag.ToNewCString();
    printf("---------- ReParentContentOfNode(%p,%s,%d) -----------\n", aNode, tag, aTransformation); 
    delete [] tag;
  }
  // find the current block parent, or just use aNode if it is a block node
  nsCOMPtr<nsIDOMElement>blockParentElement;
  nsCOMPtr<nsIDOMNode>nodeToReParent; // this is the node we'll operate on, by default it's aNode
  nsresult res = aNode->QueryInterface(nsIDOMNode::GetIID(), getter_AddRefs(nodeToReParent));
  PRBool nodeIsInline;
  PRBool nodeIsBlock=PR_FALSE;
  IsNodeInline(aNode, nodeIsInline);
  if (PR_FALSE==nodeIsInline)
  {
    nsresult QIResult;
    nsCOMPtr<nsIDOMCharacterData>nodeAsText;
    QIResult = aNode->QueryInterface(nsIDOMCharacterData::GetIID(), getter_AddRefs(nodeAsText));
    if (NS_FAILED(QIResult) || !nodeAsText) {
      nodeIsBlock=PR_TRUE;
    }
  }
  // if aNode is the block parent, then the node to reparent is one of its children
  if (PR_TRUE==nodeIsBlock) 
  {
    res = aNode->QueryInterface(nsIDOMNode::GetIID(), getter_AddRefs(blockParentElement));
    if (NS_SUCCEEDED(res) && blockParentElement) {
      res = aNode->GetFirstChild(getter_AddRefs(nodeToReParent));
    }
  }
  else { // we just need to get the block parent of aNode
    res = GetBlockParent(aNode, getter_AddRefs(blockParentElement));
  }
  // at this point, we must have a good res, a node to reparent, and a block parent
  if (!nodeToReParent) { return NS_ERROR_UNEXPECTED;}
  if (!blockParentElement) { return NS_ERROR_NULL_POINTER;}
  if (NS_SUCCEEDED(res))
  {
    nsCOMPtr<nsIDOMNode> newParentNode;
    nsCOMPtr<nsIDOMNode> blockParentNode = do_QueryInterface(blockParentElement);
    // we need to treat nodes directly inside the body differently
    nsAutoString parentTag;
    blockParentElement->GetTagName(parentTag);
    PRBool isRoot;
    IsRootTag(parentTag, isRoot);
    if (PR_TRUE==isRoot)    
    {
      // if nodeToReParent is a text node, we have <ROOT>Text.
      // re-parent Text into a new <aTag> at the offset of Text in <ROOT>
      // so we end up with <ROOT><aTag>Text
      // ignore aTransformation, replaces act like inserts
      nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(nodeToReParent);
      if (nodeAsText)
      {
        res = ReParentBlockContent(nodeToReParent, aParentTag, blockParentNode, parentTag, 
                                      aTransformation, getter_AddRefs(newParentNode));
      }
      else
      { // this is the case of an insertion point between 2 non-text objects
        // XXX: how to you know it's an insertion point???
        PRInt32 offsetInParent=0;
        res = GetChildOffset(nodeToReParent, blockParentNode, offsetInParent);
        NS_ASSERTION((NS_SUCCEEDED(res)), "bad res from GetChildOffset");
        // otherwise, just create the block parent at the selection
        res = CreateNode(aParentTag, blockParentNode, offsetInParent, 
                                          getter_AddRefs(newParentNode));
        // XXX: need to move some of the children of blockParentNode into the newParentNode?
        // XXX: need to create a bogus text node inside this new block?
        //      that means, I need to generalize bogus node handling
      }
    }
    else
    { // the block parent is not a ROOT, 
      // for the selected block content, transform blockParentNode 
      if (((eReplaceParent==aTransformation) && (PR_FALSE==parentTag.EqualsIgnoreCase(aParentTag))) ||
           (eInsertParent==aTransformation))
      {
		    if (gNoisy) { DebugDumpContent(); } // DEBUG
        res = ReParentBlockContent(nodeToReParent, aParentTag, blockParentNode, parentTag, 
                                      aTransformation, getter_AddRefs(newParentNode));
        if ((NS_SUCCEEDED(res)) && (newParentNode) && (eReplaceParent==aTransformation))
        {
          PRBool hasChildren;
          blockParentNode->HasChildNodes(&hasChildren);
          if (PR_FALSE==hasChildren)
          {
            res = nsEditor::DeleteNode(blockParentNode);
            if (gNoisy) 
            {
              printf("deleted old block parent node %p\n", blockParentNode.get());
              DebugDumpContent(); // DEBUG
            }
          }
        }
      }
      else { // otherwise, it's a no-op
        if (gNoisy) { printf("AddBlockParent is a no-op for this collapsed selection.\n"); }
      }
    }
  }
  return res;
}


NS_IMETHODIMP 
nsHTMLEditor::ReParentBlockContent(nsIDOMNode  *aNode, 
                                   nsString    &aParentTag,
                                   nsIDOMNode  *aBlockParentNode,
                                   nsString    &aBlockParentTag,
                                   BlockTransformationType aTransformation,
                                   nsIDOMNode **aNewParentNode)
{
  if (!aNode || !aBlockParentNode || !aNewParentNode) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsIDOMNode> blockParentNode = do_QueryInterface(aBlockParentNode);
  PRBool removeBlockParent = PR_FALSE;
  PRBool removeBreakBefore = PR_FALSE;
  PRBool removeBreakAfter = PR_FALSE;
  nsCOMPtr<nsIDOMNode>ancestor;
  nsresult res = aNode->GetParentNode(getter_AddRefs(ancestor));
	if (NS_FAILED(res)) return res;
  nsCOMPtr<nsIDOMNode>previousAncestor = do_QueryInterface(aNode);
  while (ancestor)
  {
    nsCOMPtr<nsIDOMElement>ancestorElement = do_QueryInterface(ancestor);
    nsAutoString ancestorTag;
    ancestorElement->GetTagName(ancestorTag);
    if (ancestorTag.EqualsIgnoreCase(aBlockParentTag))
    {
      break;  // previousAncestor will contain the node to operate on
    }
    previousAncestor = do_QueryInterface(ancestor);
    res = ancestorElement->GetParentNode(getter_AddRefs(ancestor));
  	if (NS_FAILED(res)) return res;
  }
  // now, previousAncestor is the node we are operating on
  nsCOMPtr<nsIDOMNode>leftNode, rightNode;
  res = GetBlockSection(previousAncestor, 
                        getter_AddRefs(leftNode), 
                        getter_AddRefs(rightNode));
	if (NS_FAILED(res)) return res;
	if (!leftNode || !rightNode) return NS_ERROR_NULL_POINTER;

  // determine some state for managing <BR>s around the new block
  PRBool isSubordinateBlock = PR_FALSE;  // if true, the content is already in a subordinate block
  PRBool isRootBlock = PR_FALSE;  // if true, the content is in a root block
  nsCOMPtr<nsIDOMElement>blockParentElement = do_QueryInterface(blockParentNode);
  if (blockParentElement)
  {
    nsAutoString blockParentTag;
    blockParentElement->GetTagName(blockParentTag);
    IsSubordinateBlock(blockParentTag, isSubordinateBlock);
    IsRootTag(blockParentTag, isRootBlock);
  }

  if (PR_TRUE==isRootBlock)
  { // we're creating a block element where a block element did not previously exist 
    removeBreakBefore = PR_TRUE;
    removeBreakAfter = PR_TRUE;
  }

  // apply the transformation
  PRInt32 offsetInParent=0;
  if (eInsertParent==aTransformation || PR_TRUE==isRootBlock)
  {
    res = GetChildOffset(leftNode, blockParentNode, offsetInParent);
    NS_ASSERTION((NS_SUCCEEDED(res)), "bad res from GetChildOffset");
    res = CreateNode(aParentTag, blockParentNode, offsetInParent, aNewParentNode);
    if (gNoisy) { printf("created a node in blockParentNode at offset %d\n", offsetInParent); }
  }
  else
  {
    nsCOMPtr<nsIDOMNode> grandParent;
    res = blockParentNode->GetParentNode(getter_AddRefs(grandParent));
		if (NS_FAILED(res)) return res;
		if (!grandParent) return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIDOMNode>firstChildNode, lastChildNode;
    blockParentNode->GetFirstChild(getter_AddRefs(firstChildNode));
    blockParentNode->GetLastChild(getter_AddRefs(lastChildNode));
    if (firstChildNode==leftNode && lastChildNode==rightNode)
    {
      res = GetChildOffset(blockParentNode, grandParent, offsetInParent);
      NS_ASSERTION((NS_SUCCEEDED(res)), "bad res from GetChildOffset");
      res = CreateNode(aParentTag, grandParent, offsetInParent, aNewParentNode);
      if (gNoisy) { printf("created a node in grandParent at offset %d\n", offsetInParent); }
    }
    else
    {
      // We're in the case where the content of blockParentNode is separated by <BR>'s, 
      // creating multiple block content ranges.
      // Split blockParentNode around the blockContent
      if (gNoisy) { printf("splitting a node because of <BR>s\n"); }
      nsCOMPtr<nsIDOMNode> newLeftNode;
      if (firstChildNode!=leftNode)
      {
        res = GetChildOffset(leftNode, blockParentNode, offsetInParent);
        if (gNoisy) { printf("splitting left at %d\n", offsetInParent); }
        res = SplitNode(blockParentNode, offsetInParent, getter_AddRefs(newLeftNode));
        // after this split, blockParentNode still contains leftNode and rightNode
      }
      if (lastChildNode!=rightNode)
      {
        res = GetChildOffset(rightNode, blockParentNode, offsetInParent);
        offsetInParent++;
        if (gNoisy) { printf("splitting right at %d\n", offsetInParent); }
        res = SplitNode(blockParentNode, offsetInParent, getter_AddRefs(newLeftNode));
        blockParentNode = do_QueryInterface(newLeftNode);
      }
      res = GetChildOffset(leftNode, blockParentNode, offsetInParent);
      NS_ASSERTION((NS_SUCCEEDED(res)), "bad res from GetChildOffset");
      res = CreateNode(aParentTag, blockParentNode, offsetInParent, aNewParentNode);
      if (gNoisy) { printf("created a node in blockParentNode at offset %d\n", offsetInParent); }
      // what we need to do here is remove the existing block parent when we're all done.
      removeBlockParent = PR_TRUE;
    }
  }
  if ((NS_SUCCEEDED(res)) && *aNewParentNode)
  { // move all the children/contents of blockParentNode to aNewParentNode
    nsCOMPtr<nsIDOMNode>childNode = do_QueryInterface(rightNode);
    nsCOMPtr<nsIDOMNode>previousSiblingNode;
    while (NS_SUCCEEDED(res) && childNode)
    {
      childNode->GetPreviousSibling(getter_AddRefs(previousSiblingNode));
      // explicitly delete of childNode from it's current parent
      // can't just rely on DOM semantics of InsertNode doing the delete implicitly, doesn't undo! 
      res = nsEditor::DeleteNode(childNode); 
      if (NS_SUCCEEDED(res))
      {
        res = nsEditor::InsertNode(childNode, *aNewParentNode, 0);
        if (gNoisy) 
        {
          printf("re-parented sibling node %p\n", childNode.get());
        }
      }
      if (childNode==leftNode || rightNode==leftNode) {
        break;
      }
      childNode = do_QueryInterface(previousSiblingNode);
    } // end while loop 
  }
  // clean up the surrounding content to maintain vertical whitespace
  if (NS_SUCCEEDED(res))
  {
    // if the prior node is a <BR> and we did something to change vertical whitespacing, delete the <BR>
    nsCOMPtr<nsIDOMNode> brNode;
    res = GetPriorNode(leftNode, PR_TRUE, getter_AddRefs(brNode));
    if (NS_SUCCEEDED(res) && brNode)
    {
      nsCOMPtr<nsIContent> brContent = do_QueryInterface(brNode);
      if (brContent)
      {
        nsCOMPtr<nsIAtom> brContentTag;
        brContent->GetTag(*getter_AddRefs(brContentTag));
        if (nsIEditProperty::br==brContentTag.get()) {
          res = DeleteNode(brNode);
        }
      }
    }

    // if the next node is a <BR> and we did something to change vertical whitespacing, delete the <BR>
    if (NS_SUCCEEDED(res))
    {
      res = GetNextNode(rightNode, PR_TRUE, getter_AddRefs(brNode));
      if (NS_SUCCEEDED(res) && brNode)
      {
        nsCOMPtr<nsIContent> brContent = do_QueryInterface(brNode);
        if (brContent)
        {
          nsCOMPtr<nsIAtom> brContentTag;
          brContent->GetTag(*getter_AddRefs(brContentTag));
          if (nsIEditProperty::br==brContentTag.get()) {
            res = DeleteNode(brNode);
          }
        }
      }
    }
  }
  if ((NS_SUCCEEDED(res)) && (PR_TRUE==removeBlockParent))
  { // we determined we need to remove the previous block parent.  Do it!
    // go through list backwards so deletes don't interfere with the iteration
    nsCOMPtr<nsIDOMNodeList> childNodes;
    res = blockParentNode->GetChildNodes(getter_AddRefs(childNodes));
    if ((NS_SUCCEEDED(res)) && (childNodes))
    {
      nsCOMPtr<nsIDOMNode>grandParent;
      blockParentNode->GetParentNode(getter_AddRefs(grandParent));
      //PRInt32 offsetInParent;
      res = GetChildOffset(blockParentNode, grandParent, offsetInParent);
      PRUint32 childCount;
      childNodes->GetLength(&childCount);
      PRInt32 i=childCount-1;
      for ( ; ((NS_SUCCEEDED(res)) && (0<=i)); i--)
      {
        nsCOMPtr<nsIDOMNode> childNode;
        res = childNodes->Item(i, getter_AddRefs(childNode));
        if ((NS_SUCCEEDED(res)) && (childNode))
        {
          res = DeleteNode(childNode);
          if (NS_SUCCEEDED(res)) {
            res = InsertNode(childNode, grandParent, offsetInParent);
          }
        }
      }
      if (gNoisy) { printf("removing the old block parent %p\n", blockParentNode.get()); }
      res = DeleteNode(blockParentNode);
    }
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::ReParentContentOfRange(nsIDOMRange *aRange, 
                                     nsString &aParentTag,
                                     BlockTransformationType aTranformation)
{
  if (!aRange) { return NS_ERROR_NULL_POINTER; }
  nsresult res;
  nsISupportsArray *blockSections;
  res = NS_NewISupportsArray(&blockSections);
	if (NS_FAILED(res)) return res;
	if (!blockSections) return NS_ERROR_NULL_POINTER;

  res = GetBlockSectionsForRange(aRange, blockSections);
	if (NS_FAILED(res)) return res;

	// no embedded returns allowed below here in this method, or you'll get a space leak
  nsIDOMRange *subRange;
  subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
  while (subRange)
  {
    nsCOMPtr<nsIDOMNode>startParent;
    res = subRange->GetStartParent(getter_AddRefs(startParent));
    if (NS_SUCCEEDED(res) && startParent) 
    {
      if (gNoisy) { printf("ReParentContentOfRange calling ReParentContentOfNode\n"); }
      res = ReParentContentOfNode(startParent, aParentTag, aTranformation);
    }
    NS_RELEASE(subRange);
		if (NS_FAILED(res)) 
		{ // be sure to break after free of subRange
			break; 
		}
    blockSections->RemoveElementAt(0);
    subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
  }
  NS_RELEASE(blockSections);

  return res;
}


NS_IMETHODIMP 
nsHTMLEditor::RemoveParagraphStyleFromRange(nsIDOMRange *aRange)
{
  if (!aRange) { return NS_ERROR_NULL_POINTER; }
  nsresult res;
  nsISupportsArray *blockSections;
  res = NS_NewISupportsArray(&blockSections);
  if (NS_FAILED(res)) return res;
	if (!blockSections) return NS_ERROR_NULL_POINTER;

  res = GetBlockSectionsForRange(aRange, blockSections);
	if (NS_FAILED(res)) return res;

	nsIDOMRange *subRange;
  subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
  while (subRange)
  {
    res = RemoveParagraphStyleFromBlockContent(subRange);
    NS_RELEASE(subRange);
		if (NS_FAILED(res))
		{ // be sure to break after subrange is released
			break;
		}
    blockSections->RemoveElementAt(0);
    subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
  }
  NS_RELEASE(blockSections);
  
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::RemoveParagraphStyleFromBlockContent(nsIDOMRange *aRange)
{
  if (!aRange) { return NS_ERROR_NULL_POINTER; }
  nsresult res;
  nsCOMPtr<nsIDOMNode>startParent;
  aRange->GetStartParent(getter_AddRefs(startParent));
  nsCOMPtr<nsIDOMElement>blockParentElement;
  res = GetBlockParent(startParent, getter_AddRefs(blockParentElement));
	if (NS_FAILED(res)) return res;

  while (blockParentElement)
  {
    nsAutoString blockParentTag;
    blockParentElement->GetTagName(blockParentTag);
    PRBool isSubordinateBlock;
    IsSubordinateBlock(blockParentTag, isSubordinateBlock);
    if (PR_FALSE==isSubordinateBlock) {
      break;
    }
    else 
    {
      // go through list backwards so deletes don't interfere with the iteration
      nsCOMPtr<nsIDOMNodeList> childNodes;
      res = blockParentElement->GetChildNodes(getter_AddRefs(childNodes));
      if ((NS_SUCCEEDED(res)) && (childNodes))
      {
        nsCOMPtr<nsIDOMNode>grandParent;
        blockParentElement->GetParentNode(getter_AddRefs(grandParent));
				if (NS_FAILED(res)) return res;
				if (!grandParent) return NS_ERROR_NULL_POINTER;

        PRInt32 offsetInParent;
        res = GetChildOffset(blockParentElement, grandParent, offsetInParent);
        PRUint32 childCount;
        childNodes->GetLength(&childCount);
        PRInt32 i=childCount-1;
        for ( ; ((NS_SUCCEEDED(res)) && (0<=i)); i--)
        {
          nsCOMPtr<nsIDOMNode> childNode;
          res = childNodes->Item(i, getter_AddRefs(childNode));
          if ((NS_SUCCEEDED(res)) && (childNode))
          {
            res = DeleteNode(childNode);
            if (NS_SUCCEEDED(res)) 
						{
              res = InsertNode(childNode, grandParent, offsetInParent);
	            if (NS_FAILED(res)) return res;
	          }
          }
        }
        if (NS_SUCCEEDED(res)) 
				{
          res = DeleteNode(blockParentElement);
        	if (NS_FAILED(res)) return res;
        }
      }
    }
    res = GetBlockParent(startParent, getter_AddRefs(blockParentElement));
  	if (NS_FAILED(res)) return res;
  }
  return res;
}


NS_IMETHODIMP 
nsHTMLEditor::RemoveParentFromRange(const nsString &aParentTag, nsIDOMRange *aRange)
{
  if (!aRange) { return NS_ERROR_NULL_POINTER; }
  nsresult res;
  nsISupportsArray *blockSections;
  res = NS_NewISupportsArray(&blockSections);
	if (NS_FAILED(res)) return res;
	if (!blockSections) return NS_ERROR_NULL_POINTER;
  res = GetBlockSectionsForRange(aRange, blockSections);
  if (NS_SUCCEEDED(res)) 
  {
    nsIDOMRange *subRange;
    subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
    while (subRange && (NS_SUCCEEDED(res)))
    {
      res = RemoveParentFromBlockContent(aParentTag, subRange);
      NS_RELEASE(subRange);
			if (NS_FAILED(res))
			{
				break;
			}
      blockSections->RemoveElementAt(0);
      subRange = (nsIDOMRange *)(blockSections->ElementAt(0));
    }
  }
  NS_RELEASE(blockSections);

  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::RemoveParentFromBlockContent(const nsString &aParentTag, nsIDOMRange *aRange)
{
  if (!aRange) { return NS_ERROR_NULL_POINTER; }
  nsresult res;
  nsCOMPtr<nsIDOMNode>startParent;
  res = aRange->GetStartParent(getter_AddRefs(startParent));
	if (NS_FAILED(res)) return res;
	if (!startParent) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode>parentNode;
  nsCOMPtr<nsIDOMElement>parentElement;
  res = startParent->GetParentNode(getter_AddRefs(parentNode));
	if (NS_FAILED(res)) return res;

  while (parentNode)
  {
    parentElement = do_QueryInterface(parentNode);
    nsAutoString parentTag;
    parentElement->GetTagName(parentTag);
    PRBool isRoot;
    IsRootTag(parentTag, isRoot);
    if (aParentTag.EqualsIgnoreCase(parentTag))
    {
      // go through list backwards so deletes don't interfere with the iteration
      nsCOMPtr<nsIDOMNodeList> childNodes;
      res = parentElement->GetChildNodes(getter_AddRefs(childNodes));
			if (NS_FAILED(res)) return res;
			if (!childNodes) return NS_ERROR_NULL_POINTER;

      nsCOMPtr<nsIDOMNode>grandParent;
      parentElement->GetParentNode(getter_AddRefs(grandParent));
      PRInt32 offsetInParent;
      res = GetChildOffset(parentElement, grandParent, offsetInParent);
      if (NS_FAILED(res)) return res;

      PRUint32 childCount;
      childNodes->GetLength(&childCount);
      PRInt32 i=childCount-1;
      for ( ; ((NS_SUCCEEDED(res)) && (0<=i)); i--)
      {
        nsCOMPtr<nsIDOMNode> childNode;
        res = childNodes->Item(i, getter_AddRefs(childNode));
        if (NS_FAILED(res)) return res;
        if (!childNode) return NS_ERROR_NULL_POINTER;
        res = DeleteNode(childNode);
        if (NS_FAILED(res)) return res;
        res = InsertNode(childNode, grandParent, offsetInParent);
	      if (NS_FAILED(res)) return res;
      }
      res = DeleteNode(parentElement);
      if (NS_FAILED(res)) { return res; }

      break;
    }
    else if (PR_TRUE==isRoot) {  // hit a local root node, terminate loop
      break;
    }
    res = parentElement->GetParentNode(getter_AddRefs(parentNode));
  	if (NS_FAILED(res)) return res;
  }
  return res;
}



PRBool nsHTMLEditor::IsElementInBody(nsIDOMElement* aElement)
{
  if ( aElement )
  {
    nsIDOMElement* bodyElement = nsnull;
    nsresult res = nsEditor::GetBodyElement(&bodyElement);
		if (NS_FAILED(res)) return res;
		if (!bodyElement) return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIDOMNode> parent;
    nsCOMPtr<nsIDOMNode> currentElement = do_QueryInterface(aElement);
    if (currentElement)
    {
      do {
        currentElement->GetParentNode(getter_AddRefs(parent));
        if (parent)
        {
          if (parent == bodyElement)
            return PR_TRUE;

          currentElement = parent;
        }
      } while(parent);
    }  
  }
  return PR_FALSE;
}

PRBool
nsHTMLEditor::SetCaretInTableCell(nsIDOMElement* aElement)
{
  PRBool caretIsSet = PR_FALSE;

  if (aElement && IsElementInBody(aElement))
  {
    nsresult res = NS_OK;
    nsAutoString tagName;
    aElement->GetNodeName(tagName);
    tagName.ToLowerCase();
    if (tagName == "table" || tagName == "tr" || 
        tagName == "td"    || tagName == "th" ||
        tagName == "thead" || tagName == "tfoot" ||
        tagName == "tbody" || tagName == "caption")
    {
      nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);
      nsCOMPtr<nsIDOMNode> parent;
      // This MUST succeed if IsElementInBody was TRUE
      node->GetParentNode(getter_AddRefs(parent));
      nsCOMPtr<nsIDOMNode>firstChild;
      // Find deepest child
      PRBool hasChild;
      while (NS_SUCCEEDED(node->HasChildNodes(&hasChild)) && hasChild)
      {
        if (NS_SUCCEEDED(node->GetFirstChild(getter_AddRefs(firstChild))))
        {
          parent = node;
          node = firstChild;
        }
      }
      PRInt32 offset = 0;
      nsCOMPtr<nsIDOMNode>lastChild;
      res = parent->GetLastChild(getter_AddRefs(lastChild));
      if (NS_SUCCEEDED(res) && lastChild && node != lastChild)
      {
        if (node == lastChild)
        {
          // Check if node is text and has more than just a &nbsp
          nsCOMPtr<nsIDOMCharacterData>textNode = do_QueryInterface(node);
          nsString text;
          char nbspStr[2] = {nbsp, 0};
          if (textNode && textNode->GetData(text))
          {
            // Set selection relative to the text node
            parent = node;
            PRInt32 len = text.Length();
            if (len > 1 || text != nbspStr)
            {
              offset = len;
            }
          }
        } else {
          // We have > 1 node, so set to end of content
        }
      }
      // Set selection at beginning of deepest node
      // Should we set 
      nsCOMPtr<nsIDOMSelection> selection;
      res = GetSelection(getter_AddRefs(selection));
      if (NS_SUCCEEDED(res) && selection)
      {
        res = selection->Collapse(parent, offset);
        if (NS_SUCCEEDED(res))
          caretIsSet = PR_TRUE;
      }
    }
  }
  return caretIsSet;
}            



NS_IMETHODIMP
nsHTMLEditor::IsRootTag(nsString &aTag, PRBool &aIsTag)
{
  static nsAutoString bodyTag = "body";
  static nsAutoString tdTag = "td";
  static nsAutoString thTag = "th";
  static nsAutoString captionTag = "caption";
  if (PR_TRUE==aTag.EqualsIgnoreCase(bodyTag) ||
      PR_TRUE==aTag.EqualsIgnoreCase(tdTag) ||
      PR_TRUE==aTag.EqualsIgnoreCase(thTag) ||
      PR_TRUE==aTag.EqualsIgnoreCase(captionTag) )
  {
    aIsTag = PR_TRUE;
  }
  else {
    aIsTag = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::IsSubordinateBlock(nsString &aTag, PRBool &aIsTag)
{
  static nsAutoString p = "p";
  static nsAutoString h1 = "h1";
  static nsAutoString h2 = "h2";
  static nsAutoString h3 = "h3";
  static nsAutoString h4 = "h4";
  static nsAutoString h5 = "h5";
  static nsAutoString h6 = "h6";
  static nsAutoString address = "address";
  static nsAutoString pre = "pre";
  static nsAutoString li = "li";
  static nsAutoString dt = "dt";
  static nsAutoString dd = "dd";
  if (PR_TRUE==aTag.EqualsIgnoreCase(p)  ||
      PR_TRUE==aTag.EqualsIgnoreCase(h1) ||
      PR_TRUE==aTag.EqualsIgnoreCase(h2) ||
      PR_TRUE==aTag.EqualsIgnoreCase(h3) ||
      PR_TRUE==aTag.EqualsIgnoreCase(h4) ||
      PR_TRUE==aTag.EqualsIgnoreCase(h5) ||
      PR_TRUE==aTag.EqualsIgnoreCase(h6) ||
      PR_TRUE==aTag.EqualsIgnoreCase(address) ||
      PR_TRUE==aTag.EqualsIgnoreCase(pre) ||
      PR_TRUE==aTag.EqualsIgnoreCase(li) ||
      PR_TRUE==aTag.EqualsIgnoreCase(dt) ||
      PR_TRUE==aTag.EqualsIgnoreCase(dd) )
  {
    aIsTag = PR_TRUE;
  }
  else {
    aIsTag = PR_FALSE;
  }
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLEditor::DeleteSelectionAndPrepareToCreateNode(nsCOMPtr<nsIDOMNode> &parentSelectedNode, PRInt32& offsetOfNewNode)
{
  nsresult result=NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIDOMSelection> selection;
  result = GetSelection(getter_AddRefs(selection));
	if (NS_FAILED(result)) return result;
	if (!selection) return NS_ERROR_NULL_POINTER;

  PRBool collapsed;
  result = selection->GetIsCollapsed(&collapsed);
  if (NS_SUCCEEDED(result) && !collapsed) 
  {
    result = DeleteSelection(nsIEditor::eDoNothing);
    if (NS_FAILED(result)) {
      return result;
    }
    // get the new selection
    result = GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(result)) {
      return result;
    }
#ifdef NS_DEBUG
    nsCOMPtr<nsIDOMNode>testSelectedNode;
    nsresult debugResult = selection->GetAnchorNode(getter_AddRefs(testSelectedNode));
    // no selection is ok.
    // if there is a selection, it must be collapsed
    if (testSelectedNode)
    {
      PRBool testCollapsed;
      debugResult = selection->GetIsCollapsed(&testCollapsed);
      NS_ASSERTION((NS_SUCCEEDED(result)), "couldn't get a selection after deletion");
      NS_ASSERTION(PR_TRUE==testCollapsed, "selection not reset after deletion");
    }
#endif
  }
  // split the selected node
  PRInt32 offsetOfSelectedNode;
  result = selection->GetAnchorNode(getter_AddRefs(parentSelectedNode));
  if (NS_SUCCEEDED(result) && NS_SUCCEEDED(selection->GetAnchorOffset(&offsetOfSelectedNode)) && parentSelectedNode)
  {
    nsCOMPtr<nsIDOMNode> selectedNode;
    PRUint32 selectedNodeContentCount=0;
    nsCOMPtr<nsIDOMCharacterData>selectedParentNodeAsText;
    selectedParentNodeAsText = do_QueryInterface(parentSelectedNode);

    /* if the selection is a text node, split the text node if necesary
       and compute where to put the new node
    */
    if (selectedParentNodeAsText) 
    { 
      PRInt32 indexOfTextNodeInParent;
      selectedNode = do_QueryInterface(parentSelectedNode);
      selectedNode->GetParentNode(getter_AddRefs(parentSelectedNode));
      selectedParentNodeAsText->GetLength(&selectedNodeContentCount);
      GetChildOffset(selectedNode, parentSelectedNode, indexOfTextNodeInParent);

      if ((offsetOfSelectedNode!=0) && (((PRUint32)offsetOfSelectedNode)!=selectedNodeContentCount))
      {
        nsCOMPtr<nsIDOMNode> newSiblingNode;
        result = SplitNode(selectedNode, offsetOfSelectedNode, getter_AddRefs(newSiblingNode));
        // now get the node's offset in it's parent, and insert the new tag there
        if (NS_SUCCEEDED(result)) {
          result = GetChildOffset(selectedNode, parentSelectedNode, offsetOfNewNode);
        }
      }
      else 
      { // determine where to insert the new node
        if (0==offsetOfSelectedNode) {
          offsetOfNewNode = indexOfTextNodeInParent; // insert new node as previous sibling to selection parent
        }
        else {                 // insert new node as last child
          GetChildOffset(selectedNode, parentSelectedNode, offsetOfNewNode);
          offsetOfNewNode++;    // offsets are 0-based, and we need the index of the new node
        }
      }
    }
    /* if the selection is not a text node, split the parent node if necesary
       and compute where to put the new node
    */
    else
    { // it's an interior node
      nsCOMPtr<nsIDOMNodeList>parentChildList;
      parentSelectedNode->GetChildNodes(getter_AddRefs(parentChildList));
      if ((NS_SUCCEEDED(result)) && parentChildList)
      {
        result = parentChildList->Item(offsetOfSelectedNode, getter_AddRefs(selectedNode));
        if ((NS_SUCCEEDED(result)) && selectedNode)
        {
          nsCOMPtr<nsIDOMCharacterData>selectedNodeAsText;
          selectedNodeAsText = do_QueryInterface(selectedNode);
          nsCOMPtr<nsIDOMNodeList>childList;
          //CM: I added "result ="
          result = selectedNode->GetChildNodes(getter_AddRefs(childList));
          if (NS_SUCCEEDED(result)) 
          {
            if (childList)
            {
              childList->GetLength(&selectedNodeContentCount);
            } 
            else 
            {
              // This is the case where the collapsed selection offset
              //   points to an inline node with no children
              //   This must also be where the new node should be inserted
              //   and there is no splitting necessary
              offsetOfNewNode = offsetOfSelectedNode;
              return NS_OK;
            }
          }
          else 
          {
            return NS_ERROR_FAILURE;
          }
          if ((offsetOfSelectedNode!=0) && (((PRUint32)offsetOfSelectedNode)!=selectedNodeContentCount))
          {
            nsCOMPtr<nsIDOMNode> newSiblingNode;
            result = SplitNode(selectedNode, offsetOfSelectedNode, getter_AddRefs(newSiblingNode));
            // now get the node's offset in it's parent, and insert the new tag there
            if (NS_SUCCEEDED(result)) {
              result = GetChildOffset(selectedNode, parentSelectedNode, offsetOfNewNode);
            }
          }
          else 
          { // determine where to insert the new node
            if (0==offsetOfSelectedNode) {
              offsetOfNewNode = 0; // insert new node as first child
            }
            else {                 // insert new node as last child
              GetChildOffset(selectedNode, parentSelectedNode, offsetOfNewNode);
              offsetOfNewNode++;    // offsets are 0-based, and we need the index of the new node
            }
          }
        }
      }
    }

    // Here's where the new node was inserted
  }
  else {
    printf("InsertBreak into an empty document is not yet supported\n");
  }
  return result;
}


#pragma mark -

nsCOMPtr<nsIDOMElement> nsHTMLEditor::FindPreElement()
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


void nsHTMLEditor::HandleEventListenerError()
{
  if (gNoisy) { printf("failed to add event listener\n"); }
  // null out the nsCOMPtrs
  mKeyListenerP = nsnull;
  mMouseListenerP = nsnull;
  mTextListenerP = nsnull;
  mDragListenerP = nsnull;
  mCompositionListenerP = nsnull;
  mFocusListenerP = nsnull;
}


TypeInState * nsHTMLEditor::GetTypeInState()
{
  if (mTypeInState) {
    NS_ADDREF(mTypeInState);
  }
  return mTypeInState;
}


NS_IMETHODIMP
nsHTMLEditor::SetTextPropertiesForNode(nsIDOMNode  *aNode, 
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

NS_IMETHODIMP nsHTMLEditor::MoveContentOfNodeIntoNewParent(nsIDOMNode  *aNode, 
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
                        result = GetSelection(getter_AddRefs(selection));
												if (NS_FAILED(result)) return result;
												if (!selection) return NS_ERROR_NULL_POINTER;
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
  return result;
}

/* this should only get called if the only intervening nodes are inline style nodes */
NS_IMETHODIMP 
nsHTMLEditor::SetTextPropertiesForNodesWithSameParent(nsIDOMNode  *aStartNode,
                                                      PRInt32      aStartOffset,
                                                      nsIDOMNode  *aEndNode,
                                                      PRInt32      aEndOffset,
                                                      nsIDOMNode  *aParent,
                                                      nsIAtom     *aPropName,
                                                      const nsString *aAttribute,
                                                      const nsString *aValue)
{
  if (gNoisy) { printf("---------- start nsTextEditor::SetTextPropertiesForNodesWithSameParent ----------\n"); }
  nsresult result=NS_OK;
  PRBool textPropertySet;
  nsCOMPtr<nsIDOMNode>resultNode;
  IsTextPropertySetByContent(aStartNode, aPropName, aAttribute, aValue, textPropertySet, getter_AddRefs(resultNode));
  if (PR_FALSE==textPropertySet)
  {
		result = RemoveTextPropertiesForNodeWithDifferentParents(aStartNode, aStartOffset,
  																													 aEndNode,   aEndOffset,
																														 aParent,    aPropName, aAttribute);
		if (NS_FAILED(result)) return result;

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
	if (gNoisy) { printf("---------- end nsTextEditor::SetTextPropertiesForNodesWithSameParent ----------\n"); }
  return result;
}

NS_IMETHODIMP
nsHTMLEditor::MoveContiguousContentIntoNewParent(nsIDOMNode  *aStartNode, 
                                                 PRInt32      aStartOffset, 
                                                 nsIDOMNode  *aEndNode,
                                                 PRInt32      aEndOffset,
                                                 nsIDOMNode  *aGrandParentNode,
                                                 nsIDOMNode  *aNewParentNode)
{
  if (!aStartNode || !aEndNode || !aNewParentNode) { return NS_ERROR_NULL_POINTER; }
  if (gNoisy) { printf("--- start nsTextEditor::MoveContiguousContentIntoNewParent ---\n"); }
	if (gNoisy) {DebugDumpContent(); } // DEBUG

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
			if (gNoisy) {DebugDumpContent(); } // DEBUG
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
				if (gNoisy) {DebugDumpContent(); } // DEBUG
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
				if (gNoisy) printf("just after InsertNode 1\n");
				if (gNoisy) {DebugDumpContent(); } // DEBUG
        if (NS_SUCCEEDED(result))
        { // move the right half of the start node into the new parent node
          nsCOMPtr<nsIDOMNode>intermediateNode;
          result = startNode->GetNextSibling(getter_AddRefs(intermediateNode));
          if (NS_SUCCEEDED(result))
          {
            result = nsEditor::DeleteNode(startNode);
						if (gNoisy) printf("just after DeleteNode 1\n");
						if (gNoisy) {DebugDumpContent(); } // DEBUG
            if (NS_SUCCEEDED(result)) 
            { 
              PRInt32 childIndex=0;
              result = nsEditor::InsertNode(startNode, aNewParentNode, childIndex);
							if (gNoisy) printf("just after InsertNode 2\n");
				      if (gNoisy) {DebugDumpContent(); } // DEBUG
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
									if (gNoisy) printf("just after DeleteNode 3\n");
				          if (gNoisy) {DebugDumpContent(); } // DEBUG
                  if (NS_SUCCEEDED(result)) 
									{
                    result = nsEditor::InsertNode(intermediateNode, aNewParentNode, childIndex);
										if (gNoisy) printf("just after InsertNode 4\n");
										if (gNoisy) {DebugDumpContent(); } // DEBUG
                    childIndex++;
                  }
                  intermediateNode = do_QueryInterface(nextSibling);
                }
                if (NS_SUCCEEDED(result))
                { // move the left half of the end node into the new parent node
                  result = nsEditor::DeleteNode(newRightNode);
									if (gNoisy) printf("just after DeleteNode 5\n");
									if (gNoisy) {DebugDumpContent(); } // DEBUG
                  if (NS_SUCCEEDED(result)) 
                  {
                    result = nsEditor::InsertNode(newRightNode, aNewParentNode, childIndex);
										if (gNoisy) printf("just after InsertNode 5\n");
										if (gNoisy) {DebugDumpContent(); } // DEBUG
                    // now set the selection
                    if (NS_SUCCEEDED(result)) 
                    {
                      nsCOMPtr<nsIDOMSelection>selection;
                      result = GetSelection(getter_AddRefs(selection));
											if (NS_FAILED(result)) return result;
											if (!selection) return NS_ERROR_NULL_POINTER;
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
	if (gNoisy) { printf("--- end nsTextEditor::MoveContiguousContentIntoNewParent ---\n"); }
	if (gNoisy) {DebugDumpContent(); } // DEBUG
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
nsHTMLEditor::SetTextPropertiesForNodeWithDifferentParents(nsIDOMRange *aRange,
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
  result = GetSelection(getter_AddRefs(selection));
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
		if (NS_FAILED(result)) return result;
		if (!parent) return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIDOMCharacterData>nodeAsChar;
    nodeAsChar = do_QueryInterface(startNode);
    if (nodeAsChar)
    {   
      nodeAsChar->GetLength(&count);
      if (gNoisy) { printf("processing start node %p.\n", nodeAsChar.get()); }
			if (aStartOffset!=(PRInt32)count)
			{
				result = SetTextPropertiesForNode(startNode, parent, aStartOffset, count, aPropName, aAttribute, aValue);
				startOffset = 0;
			}
    }
    else 
    {
      nsCOMPtr<nsIDOMNode>grandParent;
      result = parent->GetParentNode(getter_AddRefs(grandParent));
			if (NS_FAILED(result)) return result;
			if (!grandParent) return NS_ERROR_NULL_POINTER;
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
				if (0!=aEndOffset)
				{
					result = SetTextPropertiesForNode(endNode, parent, 0, aEndOffset, aPropName, aAttribute, aValue);
					// SetTextPropertiesForNode kindly computed the proper selection focus node and offset for us, 
					// remember them here
					selection->GetFocusOffset(&endOffset);
					selection->GetFocusNode(getter_AddRefs(endNode));
				}
      }
      else 
      {
        NS_ASSERTION(0!=aEndOffset, "unexpected selection end offset");
        if (0==aEndOffset) { return NS_ERROR_NOT_IMPLEMENTED; }
        nsCOMPtr<nsIDOMNode>grandParent;
        result = parent->GetParentNode(getter_AddRefs(grandParent));
				if (NS_FAILED(result)) return result;
				if (!grandParent) return NS_ERROR_NULL_POINTER;
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

NS_IMETHODIMP
nsHTMLEditor::RemoveTextPropertiesForNode(nsIDOMNode *aNode, 
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
    if (NS_SUCCEEDED(result) && newMiddleNode)
    {
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
            if (PR_FALSE==tag.EqualsIgnoreCase(aPropName->GetUnicode()))
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
                  if (NS_SUCCEEDED(result) && childNodes)
                  {
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
                if (NS_SUCCEEDED(result) && childNodes)
                {
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
                    if (NS_SUCCEEDED(result) && grandParent)
                    {
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
                }
              }
              break;
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
nsHTMLEditor::RemoveTextPropertiesForNodesWithSameParent(nsIDOMNode *aStartNode,
                                                         PRInt32     aStartOffset,
                                                         nsIDOMNode *aEndNode,
                                                         PRInt32     aEndOffset,
                                                         nsIDOMNode *aParent,
                                                         nsIAtom    *aPropName,
                                                         const nsString *aAttribute)
{
	NS_ASSERTION(0, "obsolete");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLEditor::RemoveTextPropertiesForNodeWithDifferentParents(nsIDOMNode  *aStartNode,
                                                              PRInt32      aStartOffset,
                                                              nsIDOMNode  *aEndNode,
                                                              PRInt32      aEndOffset,
                                                              nsIDOMNode  *aParent,
                                                              nsIAtom     *aPropName,
                                                              const nsString *aAttribute)
{
  if (gNoisy) { printf("----- start nsTextEditor::RemoveTextPropertiesForNodeWithDifferentParents-----\n"); }
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
  PRBool skippedEndNode = PR_FALSE;                             // we skip the end node if aProp is not set on it
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
			if (NS_FAILED(result)) return result;
			if (!parent) return NS_ERROR_NULL_POINTER;
      nodeAsChar->GetLength(&count);
      if (aEndOffset!=0) 
      {  // only do this if at least one child is selected
        IsTextPropertySetByContent(endNode, aPropName, aAttribute, nsnull, textPropertySet, getter_AddRefs(resultNode));
        if (PR_TRUE==textPropertySet)
        {
          result = RemoveTextPropertiesForNode(endNode, parent, 0, aEndOffset, aPropName, aAttribute);
          if (0!=aEndOffset && (((PRInt32)count)!=aEndOffset)) {
            rangeEndOffset = 0; // we split endNode at aEndOffset and it is the right node now
          }
        }
        else 
        { 
          skippedEndNode = PR_TRUE;
          if (gNoisy) { printf("skipping end node because aProperty not set.\n"); } 
        }
      }
      else 
      { 
        skippedEndNode = PR_TRUE;
        if (gNoisy) { printf("skipping end node because aEndOffset==0\n"); } 
			}
    }
    else
    {
      endNode = GetChildAt(aEndNode, aEndOffset);
      parent = do_QueryInterface(aEndNode);
      if (!endNode || !parent) {return NS_ERROR_NULL_POINTER;}
      IsTextPropertySetByContent(endNode, aPropName, aAttribute, nsnull, textPropertySet, getter_AddRefs(resultNode));
      if (PR_TRUE==textPropertySet)
      {
        result = RemoveTextPropertiesForNode(endNode, parent, aEndOffset, aEndOffset+1, aPropName, aAttribute);
      }
      else
      {
        skippedEndNode = PR_TRUE;
        if (gNoisy) { printf("skipping end node because property not set\n"); }
      }
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
    if (NS_SUCCEEDED(result))
    {
      nsIContent *contentPtr;
      contentPtr = (nsIContent*)(nodeList.ElementAt(0));
      while (NS_SUCCEEDED(result) && contentPtr)
      {
        nsCOMPtr<nsIDOMNode>styleNode;
        styleNode = do_QueryInterface(contentPtr);
        // promote the children of styleNode
        nsCOMPtr<nsIDOMNode>parentNode;
        result = styleNode->GetParentNode(getter_AddRefs(parentNode));
        if (NS_SUCCEEDED(result) && parentNode)
        {
          PRInt32 position;
          result = GetChildOffset(styleNode, parentNode, position);
          if (NS_SUCCEEDED(result))
          {
            nsCOMPtr<nsIDOMNode>previousSiblingNode;
            nsCOMPtr<nsIDOMNode>childNode;
            result = styleNode->GetLastChild(getter_AddRefs(childNode));
            while (NS_SUCCEEDED(result) && childNode)
            {
              childNode->GetPreviousSibling(getter_AddRefs(previousSiblingNode));
              // explicitly delete of childNode from styleNode
              // can't just rely on DOM semantics of InsertNode doing the delete implicitly, doesn't undo! 
              result = nsEditor::DeleteNode(childNode); 
              if (NS_SUCCEEDED(result))
              {
                result = nsEditor::InsertNode(childNode, parentNode, position);
                if (gNoisy) 
                {
                  printf("deleted next sibling node %p\n", childNode.get());
                }
              }
              childNode = do_QueryInterface(previousSiblingNode);        
            } // end while loop 
            // delete styleNode
            result = nsEditor::DeleteNode(styleNode);
            if (gNoisy) 
            {
              printf("deleted style node %p\n", styleNode.get());
            }
          }
        }

        // get next content ptr
        nodeList.RemoveElementAt(0);
        contentPtr = (nsIContent*)(nodeList.ElementAt(0));
      }
    }
    nsCOMPtr<nsIDOMSelection>selection;
    result = GetSelection(getter_AddRefs(selection));
		if (NS_FAILED(result)) return result;
		if (!selection) return NS_ERROR_NULL_POINTER;

    // set the sel. start point.  if we skipped the start node, just use it
    if (PR_TRUE==skippedStartNode)
      startNode = do_QueryInterface(aStartNode);
    result = selection->Collapse(startNode, rangeStartOffset);
    if (NS_FAILED(result)) return result;

    // set the sel. end point.  if we skipped the end node, just use it
    if (PR_TRUE==skippedEndNode)
      endNode = do_QueryInterface(aEndNode);
		result = selection->Extend(endNode, rangeEndOffset);
    if (NS_FAILED(result)) return result;
    if (gNoisy) { printf("RTPFNWDP set selection.\n"); }
  }
  if (gNoisy) 
  { 
    printf("----- end nsTextEditor::RemoveTextPropertiesForNodeWithDifferentParents, dumping content...-----\n"); 
    DebugDumpContent();
  }

  return result;
}

/* this method scans the selection for adjacent text nodes
 * and collapses them into a single text node.
 * "adjacent" means literally adjacent siblings of the same parent.
 * In all cases, the content of the right text node is moved into 
 * the left node, and the right node is deleted.
 * Uses nsEditor::JoinNodes so action is undoable. 
 * Should be called within the context of a batch transaction.
 */
 /*
 XXX:  TODO: use a helper function of next/prev sibling that only deals with editable content
 */
NS_IMETHODIMP
nsHTMLEditor::CollapseAdjacentTextNodes(nsIDOMSelection *aInSelection)
{
  if (gNoisy) 
  { 
    printf("---------- start nsTextEditor::CollapseAdjacentTextNodes ----------\n"); 
    DebugDumpContent();
  }
	if (!aInSelection) return NS_ERROR_NULL_POINTER;

	nsVoidArray textNodes;	// we can't actually do anything during iteration, so store the text nodes in an array
                          // don't bother ref counting them because we know we can hold them for the 
                          // lifetime of this method

  PRBool isCollapsed;
  aInSelection->GetIsCollapsed(&isCollapsed);
  if (PR_TRUE==isCollapsed) { return NS_OK; } // no need to scan collapsed selection

  // store info about selection endpoints so we can re-establish selection after collapsing text nodes
  // XXX: won't work for multiple selections (this will create a single selection from anchor to focus)
  nsCOMPtr<nsIDOMNode> parentForSelection;  // selection's block parent
  PRInt32 rangeStartOffset, rangeEndOffset; // selection offsets
  nsresult result = GetTextSelectionOffsetsForRange(aInSelection, getter_AddRefs(parentForSelection), 
                                                    rangeStartOffset, rangeEndOffset);  
  if (NS_FAILED(result)) return result;
  if (!parentForSelection) return NS_ERROR_NULL_POINTER;

  // first, do the endpoints of the selection, so we don't rely on the iteration to include them
  nsCOMPtr<nsIDOMNode>anchor, focus;
  nsCOMPtr<nsIDOMCharacterData>anchorText, focusText;
  aInSelection->GetAnchorNode(getter_AddRefs(anchor));
  anchorText = do_QueryInterface(anchor);
  if (anchorText)
  {
		textNodes.AppendElement((void*)(anchorText.get()));
  }
  aInSelection->GetFocusNode(getter_AddRefs(focus));
  focusText = do_QueryInterface(focus);

  // for all the ranges in the selection, build a list of text nodes
  nsCOMPtr<nsIEnumerator> enumerator;
  result = aInSelection->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(result)) return result;
  if (!enumerator) return NS_ERROR_NULL_POINTER;

  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  result = enumerator->CurrentItem(getter_AddRefs(currentItem));
  if (NS_FAILED(result)) return result;
  if (!currentItem) return NS_OK; // there are no ranges in the selection, so nothing to do

  while ((NS_COMFALSE == enumerator->IsDone()))
  {
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
      nsCOMPtr<nsIDOMCharacterData> text = do_QueryInterface(content);
      if (text && anchorText.get() != text.get() && focusText.get() != text.get())
      {
			  textNodes.AppendElement((void*)(text.get()));
      }
      iter->Next();
      iter->CurrentNode(getter_AddRefs(content));
    }
    enumerator->Next();
    enumerator->CurrentItem(getter_AddRefs(currentItem));
  }
  // now add the focus to the list, if it's a text node
  if (focusText)
  {
		textNodes.AppendElement((void*)(focusText.get()));
  }


  // now that I have a list of text nodes, collapse adjacent text nodes
	PRInt32 count = textNodes.Count();
  const PRInt32 initialCount = count;
	while (1<count)
	{
		// get the leftmost 2 elements
		nsIDOMCharacterData *leftTextNode = (nsIDOMCharacterData *)(textNodes.ElementAt(0));
		nsIDOMCharacterData *rightTextNode = (nsIDOMCharacterData *)(textNodes.ElementAt(1));

    // special case:  check prev sibling of the absolute leftmost text node
    if (initialCount==count)
    {
      nsCOMPtr<nsIDOMNode> prevSiblingOfLeftTextNode;
      result = leftTextNode->GetPreviousSibling(getter_AddRefs(prevSiblingOfLeftTextNode));
      if (NS_FAILED(result)) return result;
      if (prevSiblingOfLeftTextNode)
      {
        nsCOMPtr<nsIDOMCharacterData>prevSiblingAsText = do_QueryInterface(prevSiblingOfLeftTextNode);
        if (prevSiblingAsText)
        {
          nsCOMPtr<nsIDOMNode> parent;
          result = leftTextNode->GetParentNode(getter_AddRefs(parent));
          if (NS_FAILED(result)) return result;
          if (!parent) return NS_ERROR_NULL_POINTER;
          result = JoinNodes(prevSiblingOfLeftTextNode, leftTextNode, parent);
          if (NS_FAILED(result)) return result;
        }
      }
    }

		// get the prev sibling of the right node, and see if it's leftTextNode
		nsCOMPtr<nsIDOMNode> prevSiblingOfRightTextNode;
    result = rightTextNode->GetPreviousSibling(getter_AddRefs(prevSiblingOfRightTextNode));
    if (NS_FAILED(result)) return result;
    if (prevSiblingOfRightTextNode)
    {
      nsCOMPtr<nsIDOMCharacterData>prevSiblingAsText = do_QueryInterface(prevSiblingOfRightTextNode);
      if (prevSiblingAsText)
      {
        if (prevSiblingAsText.get()==leftTextNode)
        {
          nsCOMPtr<nsIDOMNode> parent;
          result = rightTextNode->GetParentNode(getter_AddRefs(parent));
          if (NS_FAILED(result)) return result;
          if (!parent) return NS_ERROR_NULL_POINTER;
          result = JoinNodes(leftTextNode, rightTextNode, parent);
          if (NS_FAILED(result)) return result;
        }
      }
    }

    // special case:  check next sibling of the rightmost text node
    if (2==count)
    {
      nsCOMPtr<nsIDOMNode> nextSiblingOfRightTextNode;
      result = rightTextNode->GetNextSibling(getter_AddRefs(nextSiblingOfRightTextNode));
      if (NS_FAILED(result)) return result;
      if (nextSiblingOfRightTextNode)
      {
        nsCOMPtr<nsIDOMCharacterData>nextSiblingAsText = do_QueryInterface(nextSiblingOfRightTextNode);
        if (nextSiblingAsText)
        {
          nsCOMPtr<nsIDOMNode> parent;
          result = rightTextNode->GetParentNode(getter_AddRefs(parent));
          if (NS_FAILED(result)) return result;
          if (!parent) return NS_ERROR_NULL_POINTER;
          result = JoinNodes(rightTextNode, nextSiblingOfRightTextNode, parent);
          if (NS_FAILED(result)) return result;
        }
      }
    }

    // remove the rightmost remaining text node from the array and work our way towards the beginning
    textNodes.RemoveElementAt(0); // remove the leftmost text node from the list
    count --;
	}

  ResetTextSelectionForRange(parentForSelection, rangeStartOffset, rangeEndOffset, aInSelection);

  if (gNoisy) 
  { 
    printf("---------- end nsTextEditor::CollapseAdjacentTextNodes ----------\n"); 
    DebugDumpContent();
  }

	return result;
}
