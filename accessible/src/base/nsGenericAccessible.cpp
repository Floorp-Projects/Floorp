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
 * Author: Eric Vaughan (evaughan@netscape.com)
 * Contributor(s): 
 */

#include "nsGenericAccessible.h"
#include "nsIEventStateManager.h"
#include "nsIFrame.h"
#include "nsCOMPtr.h"
#include "nsIWeakReference.h"
#include "nsReadableUtils.h"
#include "nsILink.h"

#include "nsIContent.h"
#include "nsITextContent.h"
#include "nsIDOMComment.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLBRElement.h"
#include "nsIAtom.h"
#include "nsHTMLAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsGUIEvent.h"

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsGenericAccessible, nsIAccessible)

nsGenericAccessible::nsGenericAccessible()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsGenericAccessible::~nsGenericAccessible()
{
  /* destructor code */
}

/* nsIAccessible getAccParent (); */
NS_IMETHODIMP nsGenericAccessible::GetAccParent(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccNextSibling (); */
NS_IMETHODIMP nsGenericAccessible::GetAccNextSibling(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccPreviousSibling (); */
NS_IMETHODIMP nsGenericAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccFirstChild (); */
NS_IMETHODIMP nsGenericAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccLastChild (); */
NS_IMETHODIMP nsGenericAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* long getAccChildCount (); */
NS_IMETHODIMP nsGenericAccessible::GetAccChildCount(PRInt32 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* wstring getAccName (); */
NS_IMETHODIMP nsGenericAccessible::GetAccName(PRUnichar **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* wstring getAccValue (); */
NS_IMETHODIMP nsGenericAccessible::GetAccValue(PRUnichar **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setAccName (in wstring name); */
NS_IMETHODIMP nsGenericAccessible::SetAccName(const PRUnichar *name)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setAccValue (in wstring value); */
NS_IMETHODIMP nsGenericAccessible::SetAccValue(const PRUnichar *value)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* wstring getAccDescription (); */
NS_IMETHODIMP nsGenericAccessible::GetAccDescription(PRUnichar **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long getAccRole (); */
NS_IMETHODIMP nsGenericAccessible::GetAccRole(PRUint32 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* wstring getAccState (); */
NS_IMETHODIMP nsGenericAccessible::GetAccState(PRUint32 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRUint8 getAccNumActions (); */
NS_IMETHODIMP nsGenericAccessible::GetAccNumActions(PRUint8 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* wstring getAccActionName (in PRUint8 index); */
NS_IMETHODIMP nsGenericAccessible::GetAccActionName(PRUint8 index, PRUnichar **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accDoAction (in PRUint8 index); */
NS_IMETHODIMP nsGenericAccessible::AccDoAction(PRUint8 index)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccFocused(); */
NS_IMETHODIMP nsGenericAccessible::GetAccFocused(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* wstring getAccHelp (); */
NS_IMETHODIMP nsGenericAccessible::GetAccHelp(PRUnichar **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible accGetAt (in long x, in long y); */
NS_IMETHODIMP nsGenericAccessible::AccGetAt(PRInt32 x, PRInt32 y, nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible accNavigateRight (); */
NS_IMETHODIMP nsGenericAccessible::AccNavigateRight(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible accNavigateLeft (); */
NS_IMETHODIMP nsGenericAccessible::AccNavigateLeft(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible accNavigateUp (); */
NS_IMETHODIMP nsGenericAccessible::AccNavigateUp(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible accNavigateDown (); */
NS_IMETHODIMP nsGenericAccessible::AccNavigateDown(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accGetBounds (out long x, out long y, out long width, out long height); */
NS_IMETHODIMP nsGenericAccessible::AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accAddSelection (); */
NS_IMETHODIMP nsGenericAccessible::AccAddSelection()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accRemoveSelection (); */
NS_IMETHODIMP nsGenericAccessible::AccRemoveSelection()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accExtendSelection (); */
NS_IMETHODIMP nsGenericAccessible::AccExtendSelection()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accTakeSelection (); */
NS_IMETHODIMP nsGenericAccessible::AccTakeSelection()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accTakeFocus (); */
NS_IMETHODIMP nsGenericAccessible::AccTakeFocus()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long getAccExtState (); */
NS_IMETHODIMP nsGenericAccessible::GetAccExtState(PRUint32 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsGenericAccessible::AccGetDOMNode(nsIDOMNode **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-------------
// nsDOMAccessible
//-------------

NS_IMETHODIMP 
nsDOMAccessible::QueryInterface(REFNSIID aIID, void** aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;
  
  if (aIID.Equals(NS_GET_IID(nsIDOMNode))) {                                         
    nsIDOMNode* node = mNode;
    *aResult = (void*) node;
    NS_ADDREF(node);
    return NS_OK;
  }

  return nsGenericAccessible::QueryInterface(aIID, aResult);
}

nsDOMAccessible::nsDOMAccessible(nsIPresShell* aShell, nsIDOMNode* aNode)
{
  mPresShell = do_GetWeakReference(aShell);
  mNode = aNode;
}

NS_IMETHODIMP nsDOMAccessible::AccGetDOMNode(nsIDOMNode **_retval)
{
    *_retval = mNode;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}

/* void accRemoveSelection (); */
NS_IMETHODIMP nsDOMAccessible::AccRemoveSelection()
{
  nsCOMPtr<nsISelectionController> control(do_QueryReferent(mPresShell));
  nsCOMPtr<nsISelection> selection;
  nsresult rv = control->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDOMNode> parent;
  rv = mNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(rv))
    return rv;

  rv = selection->Collapse(parent, 0);
  if (NS_FAILED(rv))
    return rv;

  return NS_OK;
}

/* void accTakeSelection (); */
NS_IMETHODIMP nsDOMAccessible::AccTakeSelection()
{
  nsCOMPtr<nsISelectionController> control(do_QueryReferent(mPresShell));
  nsCOMPtr<nsISelection> selection;
  nsresult rv = control->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDOMNode> parent;
  rv = mNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(rv))
    return rv;

  PRInt32 offsetInParent = 0;
  nsCOMPtr<nsIDOMNode> child;
  rv = parent->GetFirstChild(getter_AddRefs(child));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDOMNode> next; 

  while(child)
  {
    if (child == mNode) {
      // Collapse selection to just before desired element,
      rv = selection->Collapse(parent, offsetInParent);
      if (NS_FAILED(rv))
        return rv;

      // then extend it to just after
      rv = selection->Extend(parent, offsetInParent+1);
      return rv;
    }

     child->GetNextSibling(getter_AddRefs(next));
     child = next;
     offsetInParent++;
  }

  // didn't find a child
  return NS_ERROR_FAILURE;
}

/* void accTakeFocus (); */
NS_IMETHODIMP nsDOMAccessible::AccTakeFocus()
{ 
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  nsCOMPtr<nsIPresContext> context;
  shell->GetPresContext(getter_AddRefs(context));
  nsCOMPtr<nsIContent> content(do_QueryInterface(mNode));
  content->SetFocus(context);
  
  return NS_OK;
}


NS_IMETHODIMP nsDOMAccessible::AppendFlatStringFromContentNode(nsIContent *aContent, nsAWritableString *aFlatString)
{
  nsCOMPtr<nsITextContent> textContent(do_QueryInterface(aContent));
  if (textContent) {
    nsCOMPtr<nsIDOMComment> commentNode(do_QueryInterface(aContent));
    if (!commentNode) {
      PRBool isHTMLBlock = PR_FALSE;
      nsIFrame *frame;
      nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
      nsCOMPtr<nsIContent> parentContent;
      aContent->GetParent(*getter_AddRefs(parentContent));
      if (parentContent) {
        nsresult rv = shell->GetPrimaryFrameFor(parentContent, &frame);
        if (NS_SUCCEEDED(rv)) {
          // If this text is inside a block level frame (as opposed to span level), we need to add spaces around that 
          // block's text, so we don't get words jammed together in final name
          // Extra spaces will be trimmed out later
          nsCOMPtr<nsIStyleContext> styleContext;
          frame->GetStyleContext(getter_AddRefs(styleContext));
          if (styleContext) {
            const nsStyleDisplay* display = (const nsStyleDisplay*)styleContext->GetStyleData(eStyleStruct_Display);
            if (display->IsBlockLevel() || display->mDisplay == NS_STYLE_DISPLAY_TABLE_CELL) {
              isHTMLBlock = PR_TRUE;
              aFlatString->Append(NS_LITERAL_STRING(" "));
            }
          }
        }
      }
      nsAutoString text;
      textContent->CopyText(text);
      if (text.Length()>0)
        aFlatString->Append(text);
      if (isHTMLBlock)
        aFlatString->Append(NS_LITERAL_STRING(" "));
    }
    return NS_OK;
  }
  nsCOMPtr<nsIDOMHTMLBRElement> brElement(do_QueryInterface(aContent));
  if (brElement) {
    aFlatString->Append(NS_LITERAL_STRING(" "));
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLImageElement> imageContent(do_QueryInterface(aContent));
  nsCOMPtr<nsIDOMHTMLInputElement> inputContent(do_QueryInterface(aContent));
  if (imageContent || inputContent) {
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aContent));
    nsAutoString textEquivalent;
    elt->GetAttribute(NS_LITERAL_STRING("alt"), textEquivalent);
    if (textEquivalent.IsEmpty())
      elt->GetAttribute(NS_LITERAL_STRING("title"), textEquivalent);
    if (textEquivalent.IsEmpty())
      elt->GetAttribute(NS_LITERAL_STRING("name"), textEquivalent);
    if (textEquivalent.IsEmpty())
      elt->GetAttribute(NS_LITERAL_STRING("src"), textEquivalent);
    if (!textEquivalent.IsEmpty()) {
      aFlatString->Append(NS_LITERAL_STRING(" "));
      aFlatString->Append(textEquivalent);
      aFlatString->Append(NS_LITERAL_STRING(" "));
      return NS_OK;
    }
  }
  return NS_OK;
}


NS_IMETHODIMP nsDOMAccessible::AppendFlatStringFromSubtree(nsIContent *aContent, nsAWritableString *aFlatString)
{
  // Depth first search for all text nodes that are decendants of content node.
  // Append all the text into one flat string

  PRInt32 numChildren = 0;

  aContent->ChildCount(numChildren);
  if (numChildren == 0) {
    nsAutoString contentText;
    AppendFlatStringFromContentNode(aContent, aFlatString);
    return NS_OK;
    }
    
  nsIContent *contentWalker;
  PRInt32 index;
  for (index = 0; index < numChildren; index++) {
    aContent->ChildAt(index, contentWalker);
    AppendFlatStringFromSubtree(contentWalker, aFlatString);
  }
  return NS_OK;
}

//-------------
// nsLeafFrameAccessible
//-------------

nsLeafDOMAccessible::nsLeafDOMAccessible(nsIPresShell* aShell, nsIDOMNode* aNode):
nsDOMAccessible(aShell, aNode)
{
}

/* nsIAccessible getAccFirstChild (); */
NS_IMETHODIMP nsLeafDOMAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/* nsIAccessible getAccLastChild (); */
NS_IMETHODIMP nsLeafDOMAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/* long getAccChildCount (); */
NS_IMETHODIMP nsLeafDOMAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}



//----------------
// nsLinkableAccessible
//----------------

nsLinkableAccessible::nsLinkableAccessible(nsIPresShell* aShell, nsIDOMNode* aDomNode):
nsDOMAccessible(aShell, aDomNode), mIsALinkCached(PR_FALSE), mLinkContent(nsnull), mIsLinkVisited(PR_FALSE)
{ 
}

/* long GetAccState (); */
NS_IMETHODIMP nsLinkableAccessible::GetAccState(PRUint32 *_retval)
{
  *_retval |= STATE_READONLY | STATE_SELECTABLE;
  if (IsALink()) {
    *_retval |= STATE_FOCUSABLE | STATE_LINKED;
    if (mIsLinkVisited)
      *_retval |= STATE_TRAVERSED;
  }
  
  // Get current selection and find out if current node is in it
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  nsCOMPtr<nsIPresContext> context;
  shell->GetPresContext(getter_AddRefs(context));
  nsCOMPtr<nsIContent> content(do_QueryInterface(mNode));
  nsIFrame *frame;
  if (content && NS_SUCCEEDED(shell->GetPrimaryFrameFor(content, &frame))) {
    nsCOMPtr<nsISelectionController> selCon;
    frame->GetSelectionController(context,getter_AddRefs(selCon));
    if (selCon) {
      nsCOMPtr<nsISelection> domSel;
      selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));
      if (domSel) {
        PRBool isSelected = PR_FALSE, isCollapsed = PR_TRUE;
        domSel->ContainsNode(mNode, PR_TRUE, &isSelected);
        domSel->GetIsCollapsed(&isCollapsed);
        if (isSelected && !isCollapsed)
          *_retval |=STATE_SELECTED;
      }
    }
  }

  // Focused? Do we implement that here or up the chain?
  return NS_OK;
}


/* PRUint8 getAccNumActions (); */
NS_IMETHODIMP nsLinkableAccessible::GetAccNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;;
}

/* wstring getAccActionName (in PRUint8 index); */
NS_IMETHODIMP nsLinkableAccessible::GetAccActionName(PRUint8 index, PRUnichar **_retval)
{
  if (index == 0) {
    if (IsALink()) {
      *_retval = ToNewUnicode(NS_LITERAL_STRING("jump")); 
      return NS_OK;
    }
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accDoAction (in PRUint8 index); */
NS_IMETHODIMP nsLinkableAccessible::AccDoAction(PRUint8 index)
{
  if (index == 0) {
    if (IsALink()) {
      nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
      nsCOMPtr<nsIPresContext> presContext;
      shell->GetPresContext(getter_AddRefs(presContext));
      if (presContext) {
        nsMouseEvent linkClickEvent;
        linkClickEvent.eventStructType = NS_EVENT;
        linkClickEvent.message = NS_MOUSE_LEFT_CLICK;
        linkClickEvent.isShift = PR_FALSE;
        linkClickEvent.isControl = PR_FALSE;
        linkClickEvent.isAlt = PR_FALSE;
        linkClickEvent.isMeta = PR_FALSE;
        linkClickEvent.clickCount = 0;
        linkClickEvent.widget = nsnull;

        nsEventStatus eventStatus =  nsEventStatus_eIgnore;
        mLinkContent->HandleDOMEvent(presContext, &linkClickEvent, 
          nsnull, NS_EVENT_FLAG_INIT, &eventStatus);
        return NS_OK;
      }
    }
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}


PRBool nsLinkableAccessible::IsALink()
{
  if (mIsALinkCached)  // Cached answer?
    return mLinkContent? PR_TRUE: PR_FALSE;

  nsCOMPtr<nsIContent> walkUpContent(do_QueryInterface(mNode));
  if (walkUpContent) {
    nsCOMPtr<nsIContent> tempContent = walkUpContent;
    while (walkUpContent) {
      nsCOMPtr<nsILink> link(do_QueryInterface(walkUpContent));
      if (link) {
        mLinkContent = tempContent;
        mIsALinkCached = PR_TRUE;
        nsLinkState linkState;
        link->GetLinkState(linkState);
        if (linkState == eLinkState_Visited)
          mIsLinkVisited = PR_TRUE;
        return PR_TRUE;
      }
      walkUpContent->GetParent(*getter_AddRefs(tempContent));
      walkUpContent = tempContent;
    }
  }
  mIsALinkCached = PR_TRUE;  // Cached that there is no link
  return PR_FALSE;
}
