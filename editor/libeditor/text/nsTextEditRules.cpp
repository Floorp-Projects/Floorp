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
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIEnumerator.h"
#include "nsLayoutCID.h"
#include "nsIEditProperty.h"

static NS_DEFINE_CID(kCContentIteratorCID,   NS_CONTENTITERATOR_CID);

#define CANCEL_OPERATION_IF_READONLY_OR_DISABLED \
  if ((mFlags & nsIHTMLEditor::eEditorReadonlyMask) || (mFlags & nsIHTMLEditor::eEditorDisabledMask)) \
  {                     \
    *aCancel = PR_TRUE; \
    return NS_OK;       \
  };

/********************************************************
 *  Constructor/Destructor 
 ********************************************************/

nsTextEditRules::nsTextEditRules()
: mEditor(nsnull)
, mFlags(0) // initialized to 0 ("no flags set").  Real initial value is given in Init()
{
}

nsTextEditRules::~nsTextEditRules()
{
   // do NOT delete mEditor here.  We do not hold a ref count to mEditor.  mEditor owns our lifespan.
}


/********************************************************
 *  Public methods 
 ********************************************************/

NS_IMETHODIMP
nsTextEditRules::Init(nsHTMLEditor *aEditor, PRUint32 aFlags)
{
  if (!aEditor) { return NS_ERROR_NULL_POINTER; }

  mEditor = aEditor;  // we hold a non-refcounted reference back to our editor
  // call SetFlags only aftet mEditor has been initialized!
  SetFlags(aFlags);
  nsCOMPtr<nsIDOMSelection> selection;
  mEditor->GetSelection(getter_AddRefs(selection));
  NS_ASSERTION(selection, "editor cannot get selection");
  nsresult res = CreateBogusNodeIfNeeded(selection);   // this method handles null selection, which should never happen anyway
  return res;
}

NS_IMETHODIMP
nsTextEditRules::GetFlags(PRUint32 *aFlags)
{
  if (!aFlags) { return NS_ERROR_NULL_POINTER; }
  *aFlags = mFlags;
  return NS_OK;
}

// Initial style for plaintext
static char* PlaintextInitalStyle = "white-space: -moz-pre-wrap; width: 72ch; \
                                     font-family: -moz-fixed; \
                                     background-color: rgb(255, 255, 255)";

NS_IMETHODIMP
nsTextEditRules::SetFlags(PRUint32 aFlags)
{
  if (mFlags == aFlags) return NS_OK;
  
  // XXX - this won't work if body element already has
  // a style attribute on it, don't know why.
  // SetFlags() is really meant to only be called once
  // and at editor init time.  
  if (aFlags & nsIHTMLEditor::eEditorPlaintextMask)
  {
    if (!(mFlags & nsIHTMLEditor::eEditorPlaintextMask))
    {
      // we are converting TO a plaintext editor
      // put a "white-space: pre" style on the body
		  nsCOMPtr<nsIDOMElement> bodyElement;
		  nsresult res = mEditor->GetBodyElement(getter_AddRefs(bodyElement));
			if (NS_FAILED(res)) return res;
			if (!bodyElement) return NS_ERROR_NULL_POINTER;
      // not going through the editor to do this.
      // XXX This is not the right way to do this; we need an editor style
      // system so that we can add & replace style attrs.
	    bodyElement->SetAttribute("style", PlaintextInitalStyle);
    }
  }
  mFlags = aFlags;
  return NS_OK;
}

NS_IMETHODIMP 
nsTextEditRules::WillDoAction(nsIDOMSelection *aSelection, 
                              nsRulesInfo *aInfo, PRBool *aCancel)
{
  // null selection is legal
  if (!aInfo || !aCancel) { return NS_ERROR_NULL_POINTER; }

  *aCancel = PR_FALSE;

  // my kingdom for dynamic cast
  nsTextRulesInfo *info = NS_STATIC_CAST(nsTextRulesInfo*, aInfo);
    
  switch (info->action)
  {
    case kInsertBreak:
      return WillInsertBreak(aSelection, aCancel);
    case kInsertText:
      return WillInsertText(aSelection, 
                            aCancel, 
                            info->placeTxn, 
                            info->inString,
                            info->outString,
                            info->typeInState,
                            info->maxLength);
    case kDeleteSelection:
      return WillDeleteSelection(aSelection, info->collapsedAction, aCancel);
    case kUndo:
      return WillUndo(aSelection, aCancel);
    case kRedo:
      return WillRedo(aSelection, aCancel);
    case kSetTextProperty:
      return WillSetTextProperty(aSelection, aCancel);
    case kRemoveTextProperty:
      return WillRemoveTextProperty(aSelection, aCancel);
    case kOutputText:
      return WillOutputText(aSelection, 
                            info->outString,
                            aCancel);
  }
  return NS_ERROR_FAILURE;
}
  
NS_IMETHODIMP 
nsTextEditRules::DidDoAction(nsIDOMSelection *aSelection,
                             nsRulesInfo *aInfo, nsresult aResult)
{
  if (!aSelection || !aInfo) 
    return NS_ERROR_NULL_POINTER;
    
  // my kingdom for dynamic cast
  nsTextRulesInfo *info = NS_STATIC_CAST(nsTextRulesInfo*, aInfo);

  switch (info->action)
  {
   case kInsertBreak:
     return DidInsertBreak(aSelection, aResult);
    case kInsertText:
      return DidInsertText(aSelection, aResult);
    case kDeleteSelection:
      return DidDeleteSelection(aSelection, info->collapsedAction, aResult);
    case kUndo:
      return DidUndo(aSelection, aResult);
    case kRedo:
      return DidRedo(aSelection, aResult);
    case kSetTextProperty:
      return DidSetTextProperty(aSelection, aResult);
    case kRemoveTextProperty:
      return DidRemoveTextProperty(aSelection, aResult);
    case kOutputText:
      return DidOutputText(aSelection, aResult);
  }
  return NS_ERROR_FAILURE;
}
  

/********************************************************
 *  Protected methods 
 ********************************************************/


nsresult
nsTextEditRules::WillInsert(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel)
    return NS_ERROR_NULL_POINTER;
  
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED

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

nsresult
nsTextEditRules::DidInsert(nsIDOMSelection *aSelection, nsresult aResult)
{
  return NS_OK;
}

nsresult
nsTextEditRules::WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  if (mFlags & nsIHTMLEditor::eEditorSingleLineMask) {
    *aCancel = PR_TRUE;
  }
  else {
    *aCancel = PR_FALSE;
  }
  return NS_OK;
}

nsresult
nsTextEditRules::DidInsertBreak(nsIDOMSelection *aSelection, nsresult aResult)
{
  return NS_OK;
}


nsresult
nsTextEditRules::WillInsertText(nsIDOMSelection *aSelection, 
                                PRBool          *aCancel,
                                PlaceholderTxn **aTxn,
                                const nsString  *aInString,
                                nsString        *aOutString,
                                TypeInState      aTypeInState,
                                PRInt32          aMaxLength)
{
  if (!aSelection || !aCancel || !aInString || !aOutString) {return NS_ERROR_NULL_POINTER;}
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED

  nsresult res;

  // initialize out params
  *aCancel = PR_TRUE;
  *aOutString = *aInString;
  
  // handle docs with a max length
  res = TruncateInsertionIfNeeded(aSelection, aInString, aOutString, aMaxLength);
  if (NS_FAILED(res)) return res;
  
  if (aOutString->Length())
  {
    
    // handle password field docs
    if (mFlags & nsIHTMLEditor::eEditorPasswordMask)
    {
      res = EchoInsertionToPWBuff(aSelection, aOutString);
      if (NS_FAILED(res)) return res;
    }
    
    // do text insertion
    PRBool bCancel;
    res = DoTextInsertion(aSelection, &bCancel, aTxn, aOutString, aTypeInState);
    if (NS_FAILED(res)) return res;
  }
  return res;
}

nsresult
nsTextEditRules::DidInsertText(nsIDOMSelection *aSelection, 
                               nsresult aResult)
{
  return DidInsert(aSelection, aResult);
}

nsresult
nsTextEditRules::CreateStyleForInsertText(nsIDOMSelection *aSelection, TypeInState &aTypeInState)
{ 
  // private method, we know aSelection is not null, and that it is collapsed
  NS_ASSERTION(nsnull!=aSelection, "bad selection");

  // We know at least one style is set and we're about to insert at least one character.
  // If the selection is in a text node, split the node (even if we're at the beginning or end)
  // then put the text node inside new inline style parents.
  // Otherwise, create the text node and the new inline style parents.
  nsCOMPtr<nsIDOMNode>anchor;
  PRInt32 offset;
  nsresult res = aSelection->GetAnchorNode( getter_AddRefs(anchor));
  if (NS_SUCCEEDED(res) && NS_SUCCEEDED(aSelection->GetAnchorOffset(&offset)) && anchor)
  {
    nsCOMPtr<nsIDOMCharacterData>anchorAsText;
    anchorAsText = do_QueryInterface(anchor);
    if (anchorAsText)
    {
      nsCOMPtr<nsIDOMNode>newTextNode;
      // create an empty text node by splitting the selected text node according to offset
      if (0==offset)
      {
        res = mEditor->SplitNode(anchorAsText, offset, getter_AddRefs(newTextNode));
      }
      else
      {
        PRUint32 length;
        anchorAsText->GetLength(&length);
        if (length==(PRUint32)offset)
        {
          // newTextNode will be the left node
          res = mEditor->SplitNode(anchorAsText, offset, getter_AddRefs(newTextNode));
          // but we want the right node in this case
          newTextNode = do_QueryInterface(anchor);
        }
        else
        {
          // splitting anchor twice sets newTextNode as an empty text node between 
          // two halves of the original text node
          res = mEditor->SplitNode(anchorAsText, offset, getter_AddRefs(newTextNode));
					if (NS_SUCCEEDED(res)) {
						res = mEditor->SplitNode(anchorAsText, 0, getter_AddRefs(newTextNode));
					}
        }
      }
      // now we have the new text node we are going to insert into.  
      // create style nodes or move it up the content hierarchy as needed.
      if ((NS_SUCCEEDED(res)) && newTextNode)
      {
        nsCOMPtr<nsIDOMNode>newStyleNode;
        if (aTypeInState.IsSet(NS_TYPEINSTATE_BOLD))
        {
          if (PR_TRUE==aTypeInState.GetBold()) {
            res = InsertStyleNode(newTextNode, nsIEditProperty::b, aSelection, getter_AddRefs(newStyleNode));
          }
          else {
            printf("not yet implemented, make not bold in a bold context\n");
          }
        }
        if (aTypeInState.IsSet(NS_TYPEINSTATE_ITALIC))
        {
          if (PR_TRUE==aTypeInState.GetItalic()) { 
            res = InsertStyleNode(newTextNode, nsIEditProperty::i, aSelection, getter_AddRefs(newStyleNode));
          }
          else
          {
            printf("not yet implemented, make not italic in a italic context\n");
          }
        }
        if (aTypeInState.IsSet(NS_TYPEINSTATE_UNDERLINE))
        {
          if (PR_TRUE==aTypeInState.GetUnderline()) {
            res = InsertStyleNode(newTextNode, nsIEditProperty::u, aSelection, getter_AddRefs(newStyleNode));
          }
          else
          {
            printf("not yet implemented, make not underline in an underline context\n");
          }
        }
        if (aTypeInState.IsSet(NS_TYPEINSTATE_FONTCOLOR))
        {
          nsAutoString value;
          aTypeInState.GetFontColor(value);
          nsAutoString attr;
          nsIEditProperty::color->ToString(attr);
          res = CreateFontStyleForInsertText(newTextNode, attr, value, aSelection);
        }
        if (aTypeInState.IsSet(NS_TYPEINSTATE_FONTFACE))
        {
          nsAutoString value;
          aTypeInState.GetFontFace(value);
          nsAutoString attr;
          nsIEditProperty::face->ToString(attr);
          res = CreateFontStyleForInsertText(newTextNode, attr, value, aSelection); 
        }
        if (aTypeInState.IsSet(NS_TYPEINSTATE_FONTSIZE))
        {
          nsAutoString value;
          aTypeInState.GetFontSize(value);
          nsAutoString attr;
          nsIEditProperty::size->ToString(attr);
          res = CreateFontStyleForInsertText(newTextNode, attr, value, aSelection);
        }
      }
    }
    else
    {
      printf("not yet implemented.  selection is not text.\n");
    }
  }
  else  // we have no selection, so insert a style tag in the body
  {
		nsCOMPtr<nsIDOMElement> bodyElement;
		nsresult res = mEditor->GetBodyElement(getter_AddRefs(bodyElement));
		if (NS_FAILED(res)) return res;
		if (!bodyElement) return NS_ERROR_NULL_POINTER;
    
		nsCOMPtr<nsIDOMNode>bodyNode = do_QueryInterface(bodyElement);
    if (bodyNode)
    { // now we've got the body tag.  insert the style tag
      if (aTypeInState.IsSet(NS_TYPEINSTATE_BOLD))
      {
        if (PR_TRUE==aTypeInState.GetBold()) { 
          InsertStyleAndNewTextNode(bodyNode, nsIEditProperty::b, aSelection);
        }
      }
      if (aTypeInState.IsSet(NS_TYPEINSTATE_ITALIC))
      {
        if (PR_TRUE==aTypeInState.GetItalic()) { 
          InsertStyleAndNewTextNode(bodyNode, nsIEditProperty::i, aSelection);
        }
      }
      if (aTypeInState.IsSet(NS_TYPEINSTATE_UNDERLINE))
      {
        if (PR_TRUE==aTypeInState.GetUnderline()) { 
          InsertStyleAndNewTextNode(bodyNode, nsIEditProperty::u, aSelection);
        }
      }
    }
  }
  return res;
}

nsresult
nsTextEditRules::CreateFontStyleForInsertText(nsIDOMNode      *aNewTextNode,
                                              const nsString  &aAttr, 
                                              const nsString  &aValue,
                                              nsIDOMSelection *aSelection)
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode>newStyleNode;
  if (0!=aValue.Length()) 
  { 
    res = InsertStyleNode(aNewTextNode, nsIEditProperty::font, aSelection, getter_AddRefs(newStyleNode));
    if (NS_FAILED(res)) return res;
    if (!newStyleNode) return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIDOMElement>element = do_QueryInterface(newStyleNode);
    if (element) {
      res = mEditor->SetAttribute(element, aAttr, aValue);
    }
  }
  else
  {
    printf("not yet implemented, undo font in an font context\n");
  }
  return res;
}

nsresult
nsTextEditRules::InsertStyleNode(nsIDOMNode      *aNode, 
                                 nsIAtom         *aTag, 
                                 nsIDOMSelection *aSelection,
                                 nsIDOMNode     **aNewNode)
{
  NS_ASSERTION(aNode && aTag, "bad args");
  if (!aNode || !aTag) { return NS_ERROR_NULL_POINTER; }

  nsresult res;
  nsCOMPtr<nsIDOMNode>parent;
  aNode->GetParentNode(getter_AddRefs(parent));
	if (NS_FAILED(res)) return res;
	if (!parent) return NS_ERROR_NULL_POINTER;

  PRInt32 offsetInParent;
  res = nsEditor::GetChildOffset(aNode, parent, offsetInParent);
  if (NS_FAILED(res)) return res;

  nsAutoString tag;
  aTag->ToString(tag);
  res = mEditor->CreateNode(tag, parent, offsetInParent, aNewNode);
	if (NS_FAILED(res)) return res;
	if (!aNewNode) return NS_ERROR_NULL_POINTER;

  res = mEditor->DeleteNode(aNode);
  if (NS_SUCCEEDED(res))
  {
    res = mEditor->InsertNode(aNode, *aNewNode, 0);
    if (NS_SUCCEEDED(res)) {
      if (aSelection) {
        res = aSelection->Collapse(aNode, 0);
      }
    }
  }
  return res;
}


nsresult
nsTextEditRules::InsertStyleAndNewTextNode(nsIDOMNode *aParentNode, nsIAtom *aTag, nsIDOMSelection *aSelection)
{
  NS_ASSERTION(aParentNode && aTag, "bad args");
  if (!aParentNode || !aTag) { return NS_ERROR_NULL_POINTER; }

  nsresult res;
  // if the selection already points to a text node, just call InsertStyleNode()
  if (aSelection)
  {
    nsCOMPtr<nsIDOMNode>anchor;
    PRInt32 offset;
    res = aSelection->GetAnchorNode(getter_AddRefs(anchor));
    if (NS_FAILED(res)) return res;
    if (!anchor) return NS_ERROR_NULL_POINTER;
    res = aSelection->GetAnchorOffset(&offset);
    if (NS_FAILED(res)) return res;
    nsCOMPtr<nsIDOMCharacterData>anchorAsText;
    anchorAsText = do_QueryInterface(anchor);
    if (anchorAsText)
    {
      nsCOMPtr<nsIDOMNode> newStyleNode;
      res = InsertStyleNode(anchor, aTag, aSelection, getter_AddRefs(newStyleNode));
      return res;
    }
  }
  // if we get here, there is no selected text node so we create one.
  nsAutoString tag;
  aTag->ToString(tag);
  nsCOMPtr<nsIDOMNode>newStyleNode;
  nsCOMPtr<nsIDOMNode>newTextNode;
  res = mEditor->CreateNode(tag, aParentNode, 0, getter_AddRefs(newStyleNode));
	if (NS_FAILED(res)) return res;
	if (!newStyleNode) return NS_ERROR_NULL_POINTER;

  nsAutoString textNodeTag;
  res = nsEditor::GetTextNodeTag(textNodeTag);
  if (NS_FAILED(res)) { return res; }

  res = mEditor->CreateNode(textNodeTag, newStyleNode, 0, getter_AddRefs(newTextNode));
	if (NS_FAILED(res)) return res;
	if (!newTextNode) return NS_ERROR_NULL_POINTER;

  if (aSelection) {
    res = aSelection->Collapse(newTextNode, 0);
  }
  return res;
}

nsresult
nsTextEditRules::WillSetTextProperty(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  nsresult res = NS_OK;

  // XXX: should probably return a success value other than NS_OK that means "not allowed"
  if (nsIHTMLEditor::eEditorPlaintextMask & mFlags) {
    *aCancel = PR_TRUE;
  }
  return res;
}

nsresult
nsTextEditRules::DidSetTextProperty(nsIDOMSelection *aSelection, nsresult aResult)
{
  return NS_OK;
}

nsresult
nsTextEditRules::WillRemoveTextProperty(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  nsresult res = NS_OK;

  // XXX: should probably return a success value other than NS_OK that means "not allowed"
  if (nsIHTMLEditor::eEditorPlaintextMask & mFlags) {
    *aCancel = PR_TRUE;
  }
  return res;
}

nsresult
nsTextEditRules::DidRemoveTextProperty(nsIDOMSelection *aSelection, nsresult aResult)
{
  return NS_OK;
}

nsresult
nsTextEditRules::WillDeleteSelection(nsIDOMSelection *aSelection, 
                                     nsIEditor::ESelectionCollapseDirection aCollapsedAction, 
                                     PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED

  // initialize out param
  *aCancel = PR_FALSE;
  
  // if there is only bogus content, cancel the operation
  if (mBogusNode) {
    *aCancel = PR_TRUE;
    return NS_OK;
  }
  if (mFlags & nsIHTMLEditor::eEditorPasswordMask)
  {
    // manage the password buffer
    PRInt32 start, end;
    mEditor->GetTextSelectionOffsets(aSelection, start, end);
    if (end==start)
    { // collapsed selection
      if (nsIEditor::eDeletePrevious==aCollapsedAction && 0<start) { // del back
        mPasswordText.Cut(start-1, 1);
      }
      else if (nsIEditor::eDeleteNext==aCollapsedAction) {      // del forward
        mPasswordText.Cut(start, 1);
      }
      // otherwise nothing to do for this collapsed selection
    }
    else {  // extended selection
      mPasswordText.Cut(start, end-start);
    }

#ifdef DEBUG_buster
    char *password = mPasswordText.ToNewCString();
    printf("mPasswordText is %s\n", password);
    delete [] password;
#endif
  }
  return NS_OK;
}

// if the document is empty, insert a bogus text node with a &nbsp;
// if we ended up with consecutive text nodes, merge them
nsresult
nsTextEditRules::DidDeleteSelection(nsIDOMSelection *aSelection, 
                                    nsIEditor::ESelectionCollapseDirection aCollapsedAction, 
                                    nsresult aResult)
{
  nsresult res = aResult;  // if aResult is an error, we just return it
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  PRBool isCollapsed;
  aSelection->GetIsCollapsed(&isCollapsed);
  NS_ASSERTION(PR_TRUE==isCollapsed, "selection not collapsed after delete selection.");
  // if the delete selection resulted in no content 
  // insert a special bogus text node with a &nbsp; character in it.
  if (NS_SUCCEEDED(res)) // only do this work if DeleteSelection completed successfully
  {
    res = CreateBogusNodeIfNeeded(aSelection);
    // if we don't have an empty document, check the selection to see if any collapsing is necessary
    if (!mBogusNode)
    {
      nsCOMPtr<nsIDOMNode>anchor;
      PRInt32 offset;
		  res = aSelection->GetAnchorNode(getter_AddRefs(anchor));
      if (NS_FAILED(res)) return res;
      if (!anchor) return NS_ERROR_NULL_POINTER;
      res = aSelection->GetAnchorOffset(&offset);
      if (NS_FAILED(res)) return res;

      nsCOMPtr<nsIDOMNodeList> anchorChildren;
      res = anchor->GetChildNodes(getter_AddRefs(anchorChildren));
      nsCOMPtr<nsIDOMNode> selectedNode;
      if ((NS_SUCCEEDED(res)) && anchorChildren) {              
        res = anchorChildren->Item(offset, getter_AddRefs(selectedNode));
      }
      else {
        selectedNode = do_QueryInterface(anchor);
      }
      if ((NS_SUCCEEDED(res)) && selectedNode)
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
              res = selectedNode->GetParentNode(getter_AddRefs(parentNode));
							if (NS_FAILED(res)) return res;
							if (!parentNode) return NS_ERROR_NULL_POINTER;
              res = mEditor->JoinNodes(siblingNode, selectedNode, parentNode);
              // selectedNode will remain after the join, siblingNode is removed
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
              res = selectedNode->GetParentNode(getter_AddRefs(parentNode));
							if (NS_FAILED(res)) return res;
							if (!parentNode) return NS_ERROR_NULL_POINTER;

              res = mEditor->JoinNodes(selectedNode, siblingNode, parentNode);
        			if (NS_FAILED(res)) return res;
              // selectedNode will remain after the join, siblingNode is removed
              // set selection
              res = aSelection->Collapse(siblingNode, selectedNodeLength);
            }
          }
        }
      }
    }
  }
  return res;
}

nsresult
nsTextEditRules::WillUndo(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  // initialize out param
  *aCancel = PR_FALSE;
  return NS_OK;
}

/* the idea here is to see if the magic empty node has suddenly reappeared as the res of the undo.
 * if it has, set our state so we remember it.
 * There is a tradeoff between doing here and at redo, or doing it everywhere else that might care.
 * Since undo and redo are relatively rare, it makes sense to take the (small) performance hit here.
 */
nsresult
nsTextEditRules:: DidUndo(nsIDOMSelection *aSelection, nsresult aResult)
{
  nsresult res = aResult;  // if aResult is an error, we return it.
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  if (NS_SUCCEEDED(res)) 
  {
    if (mBogusNode) {
      mBogusNode = do_QueryInterface(nsnull);
    }
    else
    {
      nsCOMPtr<nsIDOMNode>node;
      PRInt32 offset;
		  res = aSelection->GetAnchorNode(getter_AddRefs(node));
      if (NS_FAILED(res)) return res;
      if (!node) return NS_ERROR_NULL_POINTER;
      res = aSelection->GetAnchorOffset(&offset);
      if (NS_FAILED(res)) return res;
      nsCOMPtr<nsIDOMElement>element;
      element = do_QueryInterface(node);
      if (element)
      {
        nsAutoString att(nsEditor::kMOZEditorBogusNodeAttr);
        nsAutoString val;
        (void)element->GetAttribute(att, val);
        if (val.Equals(nsEditor::kMOZEditorBogusNodeValue)) {
          mBogusNode = do_QueryInterface(element);
        }
      }
      nsCOMPtr<nsIDOMNode> temp;
      res = node->GetParentNode(getter_AddRefs(temp));
      node = do_QueryInterface(temp);
    }
  }
  return res;
}

nsresult
nsTextEditRules::WillRedo(nsIDOMSelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  // initialize out param
  *aCancel = PR_FALSE;
  return NS_OK;
}

nsresult
nsTextEditRules::DidRedo(nsIDOMSelection *aSelection, nsresult aResult)
{
  nsresult res = aResult;  // if aResult is an error, we return it.
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  if (NS_SUCCEEDED(res)) 
  {
    if (mBogusNode) {
      mBogusNode = do_QueryInterface(nsnull);
    }
    else
    {
      nsCOMPtr<nsIDOMNode>node;
      PRInt32 offset;
		  res = aSelection->GetAnchorNode(getter_AddRefs(node));
      if (NS_FAILED(res)) return res;
      if (!node) return NS_ERROR_NULL_POINTER;
		  res = aSelection->GetAnchorOffset(&offset);
      if (NS_FAILED(res)) return res;
      nsCOMPtr<nsIDOMElement>element;
      element = do_QueryInterface(node);
      if (element)
      {
        nsAutoString att(nsEditor::kMOZEditorBogusNodeAttr);
        nsAutoString val;
        (void)element->GetAttribute(att, val);
        if (val.Equals(nsEditor::kMOZEditorBogusNodeValue)) {
          mBogusNode = do_QueryInterface(element);
        }
      }
      nsCOMPtr<nsIDOMNode> temp;
      res = node->GetParentNode(getter_AddRefs(temp));
      node = do_QueryInterface(temp);
    }
  }
  return res;
}

nsresult
nsTextEditRules::WillOutputText(nsIDOMSelection *aSelection, 
                                nsString *aOutString,
                                PRBool *aCancel)
{
  // null selection ok
  if (!aOutString || !aCancel) { return NS_ERROR_NULL_POINTER; }

  // initialize out param
  *aCancel = PR_FALSE;
  
  if (mFlags & nsIHTMLEditor::eEditorPasswordMask)
  {
    *aOutString = mPasswordText;
    *aCancel = PR_TRUE;
  }
  else if (mBogusNode)
  { // this means there's no content, so output null string
    *aOutString = "";
    *aCancel = PR_TRUE;
  }
  return NS_OK;
}

nsresult
nsTextEditRules::DidOutputText(nsIDOMSelection *aSelection, nsresult aResult)
{
  return NS_OK;
}


nsresult
nsTextEditRules::CreateBogusNodeIfNeeded(nsIDOMSelection *aSelection)
{
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }
  if (!mEditor) { return NS_ERROR_NULL_POINTER; }

	nsCOMPtr<nsIDOMElement> bodyElement;
	nsresult res = mEditor->GetBodyElement(getter_AddRefs(bodyElement));  
	if (NS_FAILED(res)) return res;
	if (!bodyElement) return NS_ERROR_NULL_POINTER;
	nsCOMPtr<nsIDOMNode>bodyNode = do_QueryInterface(bodyElement);

  // now we've got the body tag.
  // iterate the body tag, looking for editable content
  // if no editable content is found, insert the bogus node
  PRBool needsBogusContent=PR_TRUE;
  nsCOMPtr<nsIDOMNode>bodyChild;
  res = bodyNode->GetFirstChild(getter_AddRefs(bodyChild));        
  while ((NS_SUCCEEDED(res)) && bodyChild)
  { 
    if (PR_TRUE==mEditor->IsEditable(bodyChild))
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
    res = mEditor->CreateNode(nsAutoString("P"), bodyNode, 0, 
                                 getter_AddRefs(mBogusNode));
		if (NS_FAILED(res)) return res;
		if (!mBogusNode) return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIDOMNode>newTNode;
    nsAutoString textNodeTag;
    res = nsEditor::GetTextNodeTag(textNodeTag);
    if (NS_FAILED(res)) { return res; }
    res = mEditor->CreateNode(textNodeTag, mBogusNode, 0, 
                                 getter_AddRefs(newTNode));
    if (NS_FAILED(res)) return res;
    if (!newTNode) return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIDOMCharacterData>newNodeAsText;
    newNodeAsText = do_QueryInterface(newTNode);
    if (newNodeAsText)
    {
      nsAutoString data;
      data += 160;
      newNodeAsText->SetData(data);
      aSelection->Collapse(newTNode, 0);
    }
    // make sure we know the PNode is bogus
    nsCOMPtr<nsIDOMElement>newPElement;
    newPElement = do_QueryInterface(mBogusNode);
    if (newPElement)
    {
      nsAutoString att(nsEditor::kMOZEditorBogusNodeAttr);
      nsAutoString val(nsEditor::kMOZEditorBogusNodeValue);
      newPElement->SetAttribute(att, val);
    }
  }
  return res;
}


nsresult
nsTextEditRules::TruncateInsertionIfNeeded(nsIDOMSelection *aSelection, 
                                           const nsString  *aInString,
                                           nsString        *aOutString,
                                           PRInt32          aMaxLength)
{
  if (!aSelection || !aInString || !aOutString) {return NS_ERROR_NULL_POINTER;}
  
  nsresult res = NS_OK;
  *aOutString = *aInString;
  
  if ((-1 != aMaxLength) && (mFlags & nsIHTMLEditor::eEditorPlaintextMask))
  {
    // Get the current text length.
    // Get the length of inString.
		// Get the length of the selection.
		//   If selection is collapsed, it is length 0.
		//   Subtract the length of the selection from the len(doc) 
		//   since we'll delete the selection on insert.
		//   This is resultingDocLength.
    // If (resultingDocLength) is at or over max, cancel the insert
    // If (resultingDocLength) + (length of input) > max, 
		//    set aOutString to subset of inString so length = max
    PRInt32 docLength;
    res = mEditor->GetDocumentLength(&docLength);
    if (NS_FAILED(res)) { return res; }
    PRInt32 start, end;
    res = mEditor->GetTextSelectionOffsets(aSelection, start, end);
    if (NS_FAILED(res)) { return res; }
    PRInt32 selectionLength = end-start;
    if (selectionLength<0) { selectionLength *= (-1); }
    PRInt32 resultingDocLength = docLength - selectionLength;
    if (resultingDocLength >= aMaxLength) 
    {
      *aOutString = "";
      return res;
    }
    else
    {
      PRInt32 inCount = aOutString->Length();
      if ((inCount+resultingDocLength) > aMaxLength)
      {
        aOutString->Truncate(aMaxLength-resultingDocLength);
      }
    }
  }
  return res;
}


nsresult
nsTextEditRules::EchoInsertionToPWBuff(nsIDOMSelection *aSelection, nsString *aOutString)
{
  if (!aSelection || !aOutString) {return NS_ERROR_NULL_POINTER;}

  // manage the password buffer
  PRInt32 start, end;
  nsresult res = mEditor->GetTextSelectionOffsets(aSelection, start, end);
  NS_ASSERTION((NS_SUCCEEDED(res)), "getTextSelectionOffsets failed!");
  mPasswordText.Insert(*aOutString, start);

#ifdef DEBUG_jfrancis
    char *password = mPasswordText.ToNewCString();
    printf("mPasswordText is %s\n", password);
    delete [] password;
#endif

  // change the output to '*' only
  PRInt32 length = aOutString->Length();
  PRInt32 i;
  *aOutString = "";
  for (i=0; i<length; i++)
    *aOutString += '*';

  return res;
}


nsresult
nsTextEditRules::DoTextInsertion(nsIDOMSelection *aSelection, 
                                 PRBool          *aCancel,
                                 PlaceholderTxn **aTxn,
                                 const nsString  *aInString,
                                 TypeInState      aTypeInState)
{
  if (!aSelection || !aCancel || !aInString) {return NS_ERROR_NULL_POINTER;}
  nsresult res = NS_OK;
  
  // for now, we always cancel editor handling of insert text.
  // rules code always does the insertion
  *aCancel = PR_TRUE;
  
  if (mBogusNode || (PR_TRUE==aTypeInState.IsAnySet()))
  {
    res = TransactionFactory::GetNewTransaction(PlaceholderTxn::GetCID(), (EditTxn **)aTxn);
    if (NS_FAILED(res)) { return res; }
    if (!*aTxn) { return NS_ERROR_NULL_POINTER; }
    (*aTxn)->SetName(InsertTextTxn::gInsertTextTxnName);
    mEditor->Do(*aTxn);
  }
  PRBool bCancel;
  res = WillInsert(aSelection, &bCancel);
  if (NS_SUCCEEDED(res) && (!bCancel))
  {
    if (PR_TRUE==aTypeInState.IsAnySet())
    { // for every property that is set, insert a new inline style node
      res = CreateStyleForInsertText(aSelection, aTypeInState);
      if (NS_FAILED(res)) { return res; }
    }
    res = mEditor->InsertTextImpl(*aInString);
  }
  return res;
}
