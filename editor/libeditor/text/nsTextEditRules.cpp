/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsTextEditRules.h"

#include "nsEditor.h"

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
                              nsRulesInfo *aInfo, 
                              PRBool *aCancel, 
                              PRBool *aHandled)
{
  // null selection is legal
  if (!aInfo || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
#if defined(DEBUG_ftang)
  printf("nsTextEditRules::WillDoAction action= %d", aInfo->action);
#endif

  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;

  // my kingdom for dynamic cast
  nsTextRulesInfo *info = NS_STATIC_CAST(nsTextRulesInfo*, aInfo);
    
  switch (info->action)
  {
    case kInsertBreak:
      return WillInsertBreak(aSelection, aCancel, aHandled);
    case kInsertText:
    case kInsertTextIME:
      return WillInsertText(aSelection, 
                            aCancel,
                            aHandled, 
                            info->inString,
                            info->outString,
                            info->typeInState,
                            info->maxLength);
    case kDeleteSelection:
      return WillDeleteSelection(aSelection, info->collapsedAction, aCancel, aHandled);
    case kUndo:
      return WillUndo(aSelection, aCancel, aHandled);
    case kRedo:
      return WillRedo(aSelection, aCancel, aHandled);
    case kSetTextProperty:
      return WillSetTextProperty(aSelection, aCancel, aHandled);
    case kRemoveTextProperty:
      return WillRemoveTextProperty(aSelection, aCancel, aHandled);
    case kOutputText:
      return WillOutputText(aSelection, 
                            info->outputFormat,
                            info->outString,                            
                            aCancel,
                            aHandled);
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
    case kInsertTextIME:
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
  // Don't fail on transactions we don't handle here!
  return NS_OK;
}


NS_IMETHODIMP
nsTextEditRules::DocumentIsEmpty(PRBool *aDocumentIsEmpty)
{
  if (!aDocumentIsEmpty)
    return NS_ERROR_NULL_POINTER;
  
  *aDocumentIsEmpty = (mBogusNode.get() != nsnull);
  return NS_OK;
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
  }

  return NS_OK;
}

nsresult
nsTextEditRules::DidInsert(nsIDOMSelection *aSelection, nsresult aResult)
{
  return NS_OK;
}

nsresult 
nsTextEditRules::GetTopEnclosingPre(nsIDOMNode *aNode,
                                    nsIDOMNode** aOutPreNode)
{
  // check parms
  if (!aNode || !aOutPreNode) 
    return NS_ERROR_NULL_POINTER;
  *aOutPreNode = 0;
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> node, parentNode;
  node = do_QueryInterface(aNode);
  
  while (node)
  {
    nsAutoString tag;
    nsEditor::GetTagString(node, tag);
    if (tag.Equals("pre", PR_TRUE))
      *aOutPreNode = node;
    else if (tag.Equals("body", PR_TRUE))
      break;
    
    res = node->GetParentNode(getter_AddRefs(parentNode));
    if (NS_FAILED(res)) return res;
    node = parentNode;
  }

  NS_IF_ADDREF(*aOutPreNode);
  return res;
}

 nsresult
nsTextEditRules::WillInsertBreak(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  *aHandled = PR_FALSE;
  if (mFlags & nsIHTMLEditor::eEditorSingleLineMask) {
    *aCancel = PR_TRUE;
  }
  else {
    // Mail rule: split any <pre> tags in the way,
    // since they're probably quoted text.
    // For now, do this for all plaintext since mail is our main customer
    // and we don't currently set eEditorMailMask for plaintext mail.
    //if (mFlags & nsIHTMLEditor::eEditorMailMask)
    {
      nsCOMPtr<nsIDOMNode> preNode, selNode;
      PRInt32 selOffset, newOffset;
      nsresult res;
      res = mEditor->GetStartNodeAndOffset(aSelection, &selNode, &selOffset);
      if (NS_FAILED(res)) return res;

      // If any of the following fail, then just proceed with the
      // normal break insertion without worrying about the error
      res = GetTopEnclosingPre(selNode, getter_AddRefs(preNode));
      if (NS_SUCCEEDED(res) && preNode)
      {
        res = mEditor->SplitNodeDeep(preNode, selNode, selOffset, &newOffset);
        if (NS_SUCCEEDED(res))
        {
          res = preNode->GetParentNode(getter_AddRefs(selNode));
          if (NS_SUCCEEDED(res))
            res = aSelection->Collapse(selNode, newOffset);
        }
      }
    }

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
                                PRBool          *aHandled,
                                const nsString  *aInString,
                                nsString        *aOutString,
                                TypeInState      aTypeInState,
                                PRInt32          aMaxLength)
{
  if (!aSelection || !aCancel || !aHandled || !aInString || !aOutString) 
    {return NS_ERROR_NULL_POINTER;}
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED

  nsresult res;

  // initialize out params
  *aCancel = PR_FALSE;
  *aHandled = PR_TRUE;
  *aOutString = *aInString;
  
  // handle docs with a max length
  res = TruncateInsertionIfNeeded(aSelection, aInString, aOutString, aMaxLength);
  if (NS_FAILED(res)) return res;
  
  // handle password field docs
  if (mFlags & nsIHTMLEditor::eEditorPasswordMask)
  {
    res = EchoInsertionToPWBuff(aSelection, aOutString);
    if (NS_FAILED(res)) return res;
  }

  // do text insertion
  PRBool bCancel;
  res = DoTextInsertion(aSelection, &bCancel, aOutString, aTypeInState);

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
  // createNewTextNode is a flag that tells us whether we need to create a new text node or not
  PRBool createNewTextNode = PR_TRUE;
  if (NS_SUCCEEDED(res) && NS_SUCCEEDED(aSelection->GetAnchorOffset(&offset)) && anchor)
  {
    nsCOMPtr<nsIDOMCharacterData>anchorAsText;
    anchorAsText = do_QueryInterface(anchor);
    if (anchorAsText)
    {
      createNewTextNode = PR_FALSE;   // we found a text node, we'll base our insertion on it
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
          else 
          {
            nsCOMPtr<nsIDOMNode>parent;
            res = newTextNode->GetParentNode(getter_AddRefs(parent));
	          if (NS_FAILED(res)) return res;
	          if (!parent) return NS_ERROR_NULL_POINTER;
            res = mEditor->RemoveTextPropertiesForNode (newTextNode, parent, 0, 0, nsIEditProperty::b, nsnull);
          }
        }
        if (aTypeInState.IsSet(NS_TYPEINSTATE_ITALIC))
        {
          if (PR_TRUE==aTypeInState.GetItalic()) { 
            res = InsertStyleNode(newTextNode, nsIEditProperty::i, aSelection, getter_AddRefs(newStyleNode));
          }
          else
          {
            nsCOMPtr<nsIDOMNode>parent;
            res = newTextNode->GetParentNode(getter_AddRefs(parent));
	          if (NS_FAILED(res)) return res;
	          if (!parent) return NS_ERROR_NULL_POINTER;
            res = mEditor->RemoveTextPropertiesForNode (newTextNode, parent, 0, 0, nsIEditProperty::i, nsnull);
          }
        }
        if (aTypeInState.IsSet(NS_TYPEINSTATE_UNDERLINE))
        {
          if (PR_TRUE==aTypeInState.GetUnderline()) {
            res = InsertStyleNode(newTextNode, nsIEditProperty::u, aSelection, getter_AddRefs(newStyleNode));
          }
          else 
          {
            nsCOMPtr<nsIDOMNode>parent;
            res = newTextNode->GetParentNode(getter_AddRefs(parent));
	          if (NS_FAILED(res)) return res;
	          if (!parent) return NS_ERROR_NULL_POINTER;
            res = mEditor->RemoveTextPropertiesForNode (newTextNode, parent, 0, 0, nsIEditProperty::u, nsnull);
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
  }

  // we have no text node, so create a new style tag(s) with a newly created text node in it
  // this is a separate case from the code above because that code needs to handle turning
  // properties on and off, this code only turns them on
  if (PR_TRUE==createNewTextNode)  
  {
    offset = 0;
    nsCOMPtr<nsIDOMNode>parent = do_QueryInterface(anchor);
    if (parent)
    { // we have a selection, get the offset within the parent
      res = aSelection->GetAnchorOffset(&offset);
      if (NS_FAILED(res)) { return res; }
    }
    else
    {
      nsCOMPtr<nsIDOMElement> bodyElement;
      res = mEditor->GetBodyElement(getter_AddRefs(bodyElement));
      if (NS_FAILED(res)) return res;
      if (!bodyElement) return NS_ERROR_NULL_POINTER;
      parent = do_QueryInterface(bodyElement);
      // offset already set to 0
    }		
    if (!parent) { return NS_ERROR_NULL_POINTER; }

    nsAutoString attr, value;

    // now we've got the parent. insert the style tag(s)
    if (aTypeInState.IsSet(NS_TYPEINSTATE_BOLD))
    {
      if (PR_TRUE==aTypeInState.GetBold()) { 
        res = InsertStyleAndNewTextNode(parent, offset, 
                                        nsIEditProperty::b, attr, value, 
                                        aSelection);
        if (NS_FAILED(res)) { return res; }
      }
    }
    if (aTypeInState.IsSet(NS_TYPEINSTATE_ITALIC))
    {
      if (PR_TRUE==aTypeInState.GetItalic()) { 
        res = InsertStyleAndNewTextNode(parent, offset, 
                                        nsIEditProperty::i, attr, value, 
                                        aSelection);
        if (NS_FAILED(res)) { return res; }
      }
    }
    if (aTypeInState.IsSet(NS_TYPEINSTATE_UNDERLINE))
    {
      if (PR_TRUE==aTypeInState.GetUnderline()) { 
        res = InsertStyleAndNewTextNode(parent, offset, 
                                        nsIEditProperty::u, attr, value, 
                                        aSelection);
        if (NS_FAILED(res)) { return res; }
      }
    }
    if (aTypeInState.IsSet(NS_TYPEINSTATE_FONTCOLOR))
    {
      aTypeInState.GetFontColor(value);
      nsIEditProperty::color->ToString(attr);
      res = InsertStyleAndNewTextNode(parent, offset, 
                                      nsIEditProperty::font, attr, value, 
                                      aSelection);
      if (NS_FAILED(res)) { return res; }
    }
    if (aTypeInState.IsSet(NS_TYPEINSTATE_FONTFACE))
    {
      aTypeInState.GetFontFace(value);
      nsIEditProperty::face->ToString(attr);
      res = InsertStyleAndNewTextNode(parent, offset, 
                                      nsIEditProperty::font, attr, value, 
                                      aSelection);
      if (NS_FAILED(res)) { return res; }
    }
    if (aTypeInState.IsSet(NS_TYPEINSTATE_FONTSIZE))
    {
      aTypeInState.GetFontSize(value);
      nsIEditProperty::size->ToString(attr);
      res = InsertStyleAndNewTextNode(parent, offset, 
                                      nsIEditProperty::font, attr, value, 
                                      aSelection);
      if (NS_FAILED(res)) { return res; }
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
    nsCOMPtr<nsIDOMNode>parent;
    res = aNewTextNode->GetParentNode(getter_AddRefs(parent));
	  if (NS_FAILED(res)) return res;
	  if (!parent) return NS_ERROR_NULL_POINTER;
    res = mEditor->RemoveTextPropertiesForNode (aNewTextNode, parent, 0, 0, nsIEditProperty::font, &aAttr);
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
  res = aNode->GetParentNode(getter_AddRefs(parent));
	if (NS_FAILED(res)) return res;
	if (!parent) return NS_ERROR_NULL_POINTER;

  nsAutoString tag;
  aTag->ToString(tag);

  if (PR_FALSE == mEditor->CanContainTag(parent, tag)) {
    NS_ASSERTION(PR_FALSE, "bad use of InsertStyleNode");
    return NS_ERROR_FAILURE;  // illegal place to insert the style tag
  }

  PRInt32 offsetInParent;
  res = nsEditor::GetChildOffset(aNode, parent, offsetInParent);
  if (NS_FAILED(res)) return res;

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
nsTextEditRules::InsertStyleAndNewTextNode(nsIDOMNode *aParentNode, 
                                           PRInt32     aOffset, 
                                           nsIAtom    *aTag, 
                                           const nsString  &aAttr,
                                           const nsString  &aValue,
                                           nsIDOMSelection *aInOutSelection)
{
  NS_ASSERTION(aParentNode && aTag, "bad args");
  if (!aParentNode || !aTag) { return NS_ERROR_NULL_POINTER; }

  nsresult res;
  // if the selection already points to a text node, just call InsertStyleNode()
  if (aInOutSelection)
  {
    PRBool isCollapsed;
    aInOutSelection->GetIsCollapsed(&isCollapsed);
    if (PR_TRUE==isCollapsed)
    {
      nsCOMPtr<nsIDOMNode>anchor;
      PRInt32 offset;
      res = aInOutSelection->GetAnchorNode(getter_AddRefs(anchor));
      if (NS_FAILED(res)) return res;
      if (!anchor) return NS_ERROR_NULL_POINTER;
      res = aInOutSelection->GetAnchorOffset(&offset);  // remember where we were
      if (NS_FAILED(res)) return res;
      // if we have a text node, just wrap it in a new style node
      if (PR_TRUE==mEditor->IsTextNode(anchor))
      {
        nsCOMPtr<nsIDOMNode> newStyleNode;
        res = InsertStyleNode(anchor, aTag, aInOutSelection, getter_AddRefs(newStyleNode));
        if (NS_FAILED(res)) { return res; }
        if (!newStyleNode) { return NS_ERROR_NULL_POINTER; }

        // if we were given an attribute, set it on the new style node
        PRInt32 attrLength = aAttr.Length();
        if (0!=attrLength)
        {
          nsCOMPtr<nsIDOMElement>newStyleElement = do_QueryInterface(newStyleNode);
          res = mEditor->SetAttribute(newStyleElement, aAttr, aValue);
        }
        if (NS_SUCCEEDED(res)) {
          res = aInOutSelection->Collapse(anchor, offset);
        }
        return res;   // we return here because we used the text node passed into us via collapsed selection
      }
    }
  }

  // if we get here, there is no selected text node so we create one.
  // first, create the style node
  nsAutoString tag;
  aTag->ToString(tag);
  if (PR_FALSE == mEditor->CanContainTag(aParentNode, tag)) {
    NS_ASSERTION(PR_FALSE, "bad use of InsertStyleAndNewTextNode");
    return NS_ERROR_FAILURE;  // illegal place to insert the style tag
  }
  nsCOMPtr<nsIDOMNode>newStyleNode;
  nsCOMPtr<nsIDOMNode>newTextNode;
  res = mEditor->CreateNode(tag, aParentNode, aOffset, getter_AddRefs(newStyleNode));
  if (NS_FAILED(res)) return res;
  if (!newStyleNode) return NS_ERROR_NULL_POINTER;

  // if we were given an attribute, set it on the new style node
  PRInt32 attrLength = aAttr.Length();
  if (0!=attrLength)
  {
    nsCOMPtr<nsIDOMElement>newStyleElement = do_QueryInterface(newStyleNode);
    res = mEditor->SetAttribute(newStyleElement, aAttr, aValue);
    if (NS_FAILED(res)) return res;
  }

  // then create the text node
  nsAutoString textNodeTag;
  res = nsEditor::GetTextNodeTag(textNodeTag);
  if (NS_FAILED(res)) { return res; }

  res = mEditor->CreateNode(textNodeTag, newStyleNode, 0, getter_AddRefs(newTextNode));
	if (NS_FAILED(res)) return res;
	if (!newTextNode) return NS_ERROR_NULL_POINTER;

  // if we have a selection collapse the selection to the beginning of the new text node
  if (aInOutSelection) {
    res = aInOutSelection->Collapse(newTextNode, 0);
  }
  return res;
}

nsresult
nsTextEditRules::WillSetTextProperty(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) 
    { return NS_ERROR_NULL_POINTER; }
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
nsTextEditRules::WillRemoveTextProperty(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) 
    { return NS_ERROR_NULL_POINTER; }
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
                                     PRBool *aCancel,
                                     PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED

  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  
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
    nsCRT::free(password);
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
      // get the node that contains the selection point
      nsCOMPtr<nsIDOMNode>anchor;
      PRInt32 offset;
		  res = aSelection->GetAnchorNode(getter_AddRefs(anchor));
      if (NS_FAILED(res)) return res;
      if (!anchor) return NS_ERROR_NULL_POINTER;
      res = aSelection->GetAnchorOffset(&offset);
      if (NS_FAILED(res)) return res;
      // selectedNode is either the anchor itself, 
      // or if anchor has children, it's the referenced child node
      nsCOMPtr<nsIDOMNode> selectedNode = do_QueryInterface(anchor);
      PRBool hasChildren=PR_FALSE;
      anchor->HasChildNodes(&hasChildren);
      if (PR_TRUE==hasChildren)
      { // if anchor has children, set selectedNode to the child pointed at        
        nsCOMPtr<nsIDOMNodeList> anchorChildren;
        res = anchor->GetChildNodes(getter_AddRefs(anchorChildren));
        if ((NS_SUCCEEDED(res)) && anchorChildren) {              
          res = anchorChildren->Item(offset, getter_AddRefs(selectedNode));
        }
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
nsTextEditRules::WillUndo(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
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
      nsCOMPtr<nsIDOMElement> theBody;
      res = mEditor->GetBodyElement(getter_AddRefs(theBody));
      if (NS_FAILED(res)) return res;
      if (!theBody) return NS_ERROR_FAILURE;
      
      nsAutoString tagName("div");
      nsCOMPtr<nsIDOMNodeList> nodeList;
      res = theBody->GetElementsByTagName(tagName, getter_AddRefs(nodeList));
      if (NS_FAILED(res)) return res;
      if (nodeList)
      {
        PRUint32 len;
        nodeList->GetLength(&len);
        
        if (len != 1) return NS_OK;  // only in the case of one div could there be the bogus node
        nsCOMPtr<nsIDOMNode>node;
        nsCOMPtr<nsIDOMElement>element;
        nodeList->Item(0, getter_AddRefs(node));
        if (!node) return NS_ERROR_NULL_POINTER;
        element = do_QueryInterface(node);
        if (element)
        {
          nsAutoString att(nsEditor::kMOZEditorBogusNodeAttr);
          nsAutoString val;
          (void)element->GetAttribute(att, val);
          if (val.Equals(nsEditor::kMOZEditorBogusNodeValue)) 
          {
            mBogusNode = do_QueryInterface(element);
          }
        }
      }
    }
  }
  return res;
}

nsresult
nsTextEditRules::WillRedo(nsIDOMSelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  CANCEL_OPERATION_IF_READONLY_OR_DISABLED
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
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
      nsCOMPtr<nsIDOMElement> theBody;
      res = mEditor->GetBodyElement(getter_AddRefs(theBody));
      if (NS_FAILED(res)) return res;
      if (!theBody) return NS_ERROR_FAILURE;
      
      nsAutoString tagName("div");
      nsCOMPtr<nsIDOMNodeList> nodeList;
      res = theBody->GetElementsByTagName(tagName, getter_AddRefs(nodeList));
      if (NS_FAILED(res)) return res;
      if (nodeList)
      {
        PRUint32 len;
        nodeList->GetLength(&len);
        
        if (len != 1) return NS_OK;  // only in the case of one div could there be the bogus node
        nsCOMPtr<nsIDOMNode>node;
        nsCOMPtr<nsIDOMElement>element;
        nodeList->Item(0, getter_AddRefs(node));
        if (!node) return NS_ERROR_NULL_POINTER;
        element = do_QueryInterface(node);
        if (element)
        {
          nsAutoString att(nsEditor::kMOZEditorBogusNodeAttr);
          nsAutoString val;
          (void)element->GetAttribute(att, val);
          if (val.Equals(nsEditor::kMOZEditorBogusNodeValue)) 
          {
            mBogusNode = do_QueryInterface(element);
          }
        }
      }
    }
  }
  return res;
}

nsresult
nsTextEditRules::WillOutputText(nsIDOMSelection *aSelection, 
                                const nsString  *aOutputFormat,
                                nsString *aOutString,                                
                                PRBool   *aCancel,
                                PRBool   *aHandled)
{
  // null selection ok
  if (!aOutString || !aOutputFormat || !aCancel || !aHandled) 
    { return NS_ERROR_NULL_POINTER; }

  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;

  if (PR_TRUE == aOutputFormat->Equals("text/plain"))
  { // only use these rules for plain text output
    if (mFlags & nsIHTMLEditor::eEditorPasswordMask)
    {
      *aOutString = mPasswordText;
      *aHandled = PR_TRUE;
    }
    else if (mBogusNode)
    { // this means there's no content, so output null string
      *aOutString = "";
      *aHandled = PR_TRUE;
    }
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
  if (mBogusNode) return NS_OK;  // let's not create more than one, ok?
  
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
    // set mBogusNode to be the newly created <br>
    res = mEditor->CreateNode(nsAutoString("br"), bodyNode, 0, 
                                 getter_AddRefs(mBogusNode));
    if (NS_FAILED(res)) return res;
    if (!mBogusNode) return NS_ERROR_NULL_POINTER;

    // give it a special attribute
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
  if (end!=start)
  {
    mPasswordText.Cut(start, end-start);
  }
  mPasswordText.Insert(*aOutString, start);

#ifdef DEBUG_jfrancis
    char *password = mPasswordText.ToNewCString();
    printf("mPasswordText is %s\n", password);
    nsCRT::free(password);
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
                                 const nsString  *aInString,
                                 TypeInState      aTypeInState)
{
  if (!aSelection || !aCancel || !aInString) {return NS_ERROR_NULL_POINTER;}
  nsresult res = NS_OK;
  
  // for now, we always cancel editor handling of insert text.
  // rules code always does the insertion
  *aCancel = PR_TRUE;
  
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
