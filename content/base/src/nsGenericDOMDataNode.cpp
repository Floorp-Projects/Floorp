/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsGenericDOMDataNode.h"
#include "nsGenericElement.h"
#include "nsIDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIDocument.h"
#include "nsIDOMRange.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsRange.h"
#include "nsTextContentChangeData.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsIEnumerator.h"
#include "nsReadableUtils.h"
#include "nsMutationEvent.h"
#include "nsPIDOMWindow.h"

#include "nsCRT.h"
#include "nsIEventStateManager.h"
#include "nsIPrivateDOMEvent.h"
#include "nsISizeOfHandler.h"
#include "nsIDOMEvent.h"
#include "nsIDOMText.h"
#include "nsIScriptGlobalObject.h"
#include "prprf.h"
#include "nsCOMPtr.h"

//----------------------------------------------------------------------

nsGenericDOMDataNode::nsGenericDOMDataNode()
  : mText()
{
  mDocument = nsnull;
  mParent = nsnull;
  mListenerManager = nsnull;
  mRangeList = nsnull;
}

nsGenericDOMDataNode::~nsGenericDOMDataNode()
{
  if (mListenerManager) {
    mListenerManager->SetListenerTarget(nsnull);
    NS_RELEASE(mListenerManager);
  }
  delete mRangeList;
}

nsresult
nsGenericDOMDataNode::GetNodeValue(nsAWritableString& aNodeValue)
{
  return GetData(aNodeValue);
}

nsresult
nsGenericDOMDataNode::SetNodeValue(nsIContent *aOuterContent,
                                   const nsAReadableString& aNodeValue)
{
  return SetData(aOuterContent, aNodeValue);
}

nsresult
nsGenericDOMDataNode::GetParentNode(nsIDOMNode** aParentNode)
{
  nsresult res = NS_OK;

  if (nsnull != mParent) {
    res = mParent->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aParentNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node");
  }
  else if (nsnull == mDocument) {
    *aParentNode = nsnull;
  } 
  else {
    // If we don't have a parent, but we're in the document, we must
    // be the root node of the document. The DOM says that the root
    // is the document.
    res = mDocument->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aParentNode);
  }

  return res;
}

nsresult
nsGenericDOMDataNode::GetPreviousSibling(nsIContent *aOuterContent,
                                         nsIDOMNode** aPrevSibling)
{
  nsIContent* sibling = nsnull;
  nsresult result = NS_OK;

  if (nsnull != mParent) {
    PRInt32 pos;
    mParent->IndexOf(aOuterContent, pos);
    if (pos > -1 ) {
      mParent->ChildAt(--pos, sibling);
    }
  }
  else if (nsnull != mDocument) {
    // Nodes that are just below the document (their parent is the
    // document) need to go to the document to find their next sibling.
    PRInt32 pos;
    mDocument->IndexOf(aOuterContent, pos);
    if (pos > -1 ) {
      mDocument->ChildAt(--pos, sibling);
    }    
  }

  if (nsnull != sibling) {
    result = sibling->QueryInterface(NS_GET_IID(nsIDOMNode),(void**)aPrevSibling);
    NS_ASSERTION(NS_OK == result, "Must be a DOM Node");
    NS_RELEASE(sibling); // balance the AddRef in ChildAt()
  }
  else {
    *aPrevSibling = nsnull;
  }
  
  return result;
}

nsresult
nsGenericDOMDataNode::GetNextSibling(nsIContent *aOuterContent,
                                     nsIDOMNode** aNextSibling)
{
  nsIContent* sibling = nsnull;
  nsresult result = NS_OK;

  if (nsnull != mParent) {
    PRInt32 pos;
    mParent->IndexOf(aOuterContent, pos);
    if (pos > -1 ) {
      mParent->ChildAt(++pos, sibling);
    }
  }
  else if (nsnull != mDocument) {
    // Nodes that are just below the document (their parent is the
    // document) need to go to the document to find their next sibling.
    PRInt32 pos;
    mDocument->IndexOf(aOuterContent, pos);
    if (pos > -1 ) {
      mDocument->ChildAt(++pos, sibling);
    }    
  }

  if (nsnull != sibling) {
    result = sibling->QueryInterface(NS_GET_IID(nsIDOMNode),(void**)aNextSibling);
    NS_ASSERTION(NS_OK == result, "Must be a DOM Node");
    NS_RELEASE(sibling); // balance the AddRef in ChildAt()
  }
  else {
    *aNextSibling = nsnull;
  }
  
  return result;
}

nsresult    
nsGenericDOMDataNode::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  // XXX Since we believe this won't be done very often, we won't
  // burn another slot in the data node and just create a new
  // (empty) childNodes list every time we're asked.
  nsChildContentList* list = new nsChildContentList(nsnull);
  if (nsnull == list) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  return list->QueryInterface(NS_GET_IID(nsIDOMNodeList), (void**)aChildNodes);
}

nsresult    
nsGenericDOMDataNode::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  // XXX Actually the owner document is the document in whose context
  // the node has been created. We should be able to get at it
  // whether or not we are attached to the document.
  if (nsnull != mDocument) {
    return mDocument->QueryInterface(NS_GET_IID(nsIDOMDocument), (void **)aOwnerDocument);
  }
  else {
    *aOwnerDocument = nsnull;
    return NS_OK;
  }
}

nsresult
nsGenericDOMDataNode::GetNamespaceURI(nsAWritableString& aNamespaceURI)
{
  SetDOMStringToNull(aNamespaceURI);

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::GetPrefix(nsAWritableString& aPrefix)
{
  SetDOMStringToNull(aPrefix);

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::SetPrefix(const nsAReadableString& aPrefix)
{
  return NS_ERROR_DOM_NAMESPACE_ERR;
}

nsresult
nsGenericDOMDataNode::GetLocalName(nsAWritableString& aLocalName)
{
  SetDOMStringToNull(aLocalName);

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::Normalize()
{
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::IsSupported(const nsAReadableString& aFeature,
                                  const nsAReadableString& aVersion,
                                  PRBool* aReturn)
{
  return nsGenericElement::InternalIsSupported(aFeature, aVersion, aReturn);
}

nsresult
nsGenericDOMDataNode::GetBaseURI(nsAWritableString& aURI)
{
  aURI.Truncate();
  nsresult rv = NS_OK;
  // DOM Data Node inherits the base from its parent element/document
  nsCOMPtr<nsIDOM3Node> node;
  if (mParent) {
    node = do_QueryInterface(mParent);
  } else if (mDocument) {
    node = do_QueryInterface(mDocument);
  }

#if 0
  if (node)
    rv = node->GetBaseURI(aURI);
#endif

  return rv;
}

nsresult
nsGenericDOMDataNode::LookupNamespacePrefix(const nsAReadableString& aNamespaceURI,
                                            nsAWritableString& aPrefix) 
{
  aPrefix.Truncate();
  // DOM Data Node passes the query on to its parent
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(mParent));
  if (node) {
    return node->LookupNamespacePrefix(aNamespaceURI, aPrefix);
  }

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::LookupNamespaceURI(const nsAReadableString& aNamespacePrefix,
                                         nsAWritableString& aNamespaceURI)
{
  aNamespaceURI.Truncate();
  // DOM Data Node passes the query on to its parent
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(mParent));
  if (node) {
    return node->LookupNamespaceURI(aNamespacePrefix, aNamespaceURI);
  }

  return NS_OK;
}

#if 0
nsresult
nsGenericDOMDataNode::Equals(nsIDOMNode* aNode, PRBool aDeep, PRBool* aReturn)
{
  *aReturn = PR_FALSE;
  PRInt32 nt1, nt2;
  GetNodeType(&nt1);
  aNode->GetNodeType(&nt2);
  if (nt1 != nt2) {
    return NS_OK;
  }
  return NS_OK;
}
#endif

//----------------------------------------------------------------------

// Implementation of nsIDOMCharacterData

nsresult    
nsGenericDOMDataNode::GetData(nsAWritableString& aData)
{
  if (mText.Is2b()) {
    aData.Assign(mText.Get2b(), mText.GetLength());
  }
  else {
    aData.Assign(NS_ConvertASCIItoUCS2(mText.Get1b(), mText.GetLength()));
  }
  return NS_OK;
}

nsresult    
nsGenericDOMDataNode::SetData(nsIContent *aOuterContent, const nsAReadableString& aData)
{
  // inform any enclosed ranges of change
  // we can lie and say we are deleting all the text, since in a total
  // text replacement we should just collapse all the ranges.
  if (mRangeList) nsRange::TextOwnerChanged(aOuterContent, 0,
                                            mText.GetLength(), 0);

  nsresult result;
  nsCOMPtr<nsITextContent> textContent = do_QueryInterface(aOuterContent,
                                                           &result);

  // If possible, let the container content object have a go at it.
  if (NS_SUCCEEDED(result)) {
    result = textContent->SetText(aData, PR_TRUE); 
  }
  else {
    result = SetText(aOuterContent, aData, PR_TRUE); 
  }

  return result;
}

nsresult
nsGenericDOMDataNode::GetLength(PRUint32* aLength)
{
  *aLength = mText.GetLength();
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::SubstringData(PRUint32 aStart,
                                    PRUint32 aCount,
                                    nsAWritableString& aReturn)
{
  aReturn.Truncate();

  // XXX add <0 checks if types change
  PRUint32 textLength = PRUint32( mText.GetLength() );
  if (aStart > textLength) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  PRUint32 amount = aCount;
  if (aStart + amount > textLength) {
    amount = textLength - aStart;
  }
  if (mText.Is2b()) {
    aReturn.Assign(mText.Get2b() + aStart, amount);
  }
  else {
    aReturn.Assign(NS_ConvertASCIItoUCS2(mText.Get1b() + aStart, amount).get(), amount);
  }

  return NS_OK;
}

//----------------------------------------------------------------------

nsresult    
nsGenericDOMDataNode::AppendData(nsIContent *aOuterContent,
                                 const nsAReadableString& aData)
{
#if 1
  // Allocate new buffer
  nsresult result = NS_OK;
  PRUint32 dataLength = aData.Length();
  PRInt32 textLength = mText.GetLength();
  PRInt32 newSize = textLength + dataLength;
  PRUnichar* to = new PRUnichar[newSize + 1];
  if (nsnull == to) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // XXX This is slow...

  // Fill new buffer with old data and new data
  if (textLength) {
    mText.CopyTo(to, 0, textLength);
  }
  CopyUnicodeTo(aData, 0, to + textLength, dataLength);

  // Null terminate the new buffer...
  to[newSize] = (PRUnichar)0;

  nsCOMPtr<nsITextContent> textContent(do_QueryInterface(aOuterContent,
                                                         &result));

  // Switch to new buffer
  // Dont do notification in SetText, since we will do it later
  if (NS_SUCCEEDED(result)) {
    result = textContent->SetText(to, newSize, PR_FALSE);
  }
  else {
    result = SetText(aOuterContent, to, newSize, PR_FALSE);
  }

  delete [] to;

  // Trigger a reflow
  if (nsnull != mDocument) {
    nsTextContentChangeData* tccd = nsnull;
    result = NS_NewTextContentChangeData(&tccd);
    if (NS_SUCCEEDED(result)) {
      tccd->SetData(nsITextContentChangeData::Append, textLength, dataLength);
      result = mDocument->ContentChanged(aOuterContent, tccd);
      NS_RELEASE(tccd);
    }
    else {
      result = mDocument->ContentChanged(aOuterContent, nsnull);
    }
  }

  return result;
#else
  return ReplaceData(mText.GetLength(), 0, aData);
#endif
}

nsresult    
nsGenericDOMDataNode::InsertData(nsIContent *aOuterContent, PRUint32 aOffset,
                                 const nsAReadableString& aData)
{
  return ReplaceData(aOuterContent, aOffset, 0, aData);
}

nsresult    
nsGenericDOMDataNode::DeleteData(nsIContent *aOuterContent, PRUint32 aOffset,
                                 PRUint32 aCount)
{
  nsAutoString empty;
  return ReplaceData(aOuterContent, aOffset, aCount, empty);
}

nsresult    
nsGenericDOMDataNode::ReplaceData(nsIContent *aOuterContent, PRUint32 aOffset,
                                  PRUint32 aCount, const nsAReadableString& aData)
{
  nsresult result = NS_OK;

  // sanitize arguments
  PRUint32 textLength = mText.GetLength();
  if (aOffset > textLength) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Allocate new buffer
  PRUint32 endOffset = aOffset + aCount;
  if (endOffset > textLength) {
    aCount = textLength - aOffset;
    endOffset = textLength;
  }
  PRInt32 dataLength = aData.Length();
  PRInt32 newLength = textLength - aCount + dataLength;
  PRUnichar* to = new PRUnichar[newLength ? newLength+1 : 1];
  if (nsnull == to) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // inform any enclosed ranges of change
  if (mRangeList) nsRange::TextOwnerChanged(aOuterContent, aOffset,
                                            endOffset, dataLength);
  
  // Copy over appropriate data
  if (0 != aOffset) {
    mText.CopyTo(to, 0, aOffset);
  }
  if (0 != dataLength) {
    CopyUnicodeTo(aData, 0, to+aOffset, dataLength);
  }
  if (endOffset != textLength) {
    mText.CopyTo(to + aOffset + dataLength, endOffset, textLength - endOffset);
  }

  // Null terminate the new buffer...
  to[newLength] = (PRUnichar)0;

  // Switch to new buffer
  nsCOMPtr<nsITextContent> textContent(do_QueryInterface(aOuterContent,
                                                         &result));

  // If possible, let the container content object have a go at it.
  if (NS_SUCCEEDED(result)) {
    result = textContent->SetText(to, newLength, PR_TRUE);
  }
  else {
    result = SetText(aOuterContent, to, newLength, PR_TRUE);
  }
  delete [] to;

  return result;
}

//----------------------------------------------------------------------

nsresult
nsGenericDOMDataNode::GetListenerManager(nsIContent* aOuterContent, nsIEventListenerManager** aResult)
{
  if (nsnull != mListenerManager) {
    NS_ADDREF(mListenerManager);
    *aResult = mListenerManager;
    return NS_OK;
  }
  nsresult rv = NS_NewEventListenerManager(aResult);
  if (NS_OK == rv) {
    mListenerManager = *aResult;
    NS_ADDREF(mListenerManager);
    mListenerManager->SetListenerTarget(aOuterContent);
  }
  return rv;
}

//----------------------------------------------------------------------

// Implementation of nsIContent


void
nsGenericDOMDataNode::ToCString(nsAWritableString& aBuf, PRInt32 aOffset,
                                PRInt32 aLen) const
{
  if (mText.Is2b()) {
    const PRUnichar* cp = mText.Get2b() + aOffset;
    const PRUnichar* end = cp + aLen;
    while (cp < end) {
      PRUnichar ch = *cp++;
      if (ch == '\r') {
        aBuf.Append(NS_LITERAL_STRING("\\r"));
      } else if (ch == '\n') {
        aBuf.Append(NS_LITERAL_STRING("\\n"));
      } else if (ch == '\t') {
        aBuf.Append(NS_LITERAL_STRING("\\t"));
      } else if ((ch < ' ') || (ch >= 127)) {
        char buf[10];
        PR_snprintf(buf, sizeof(buf), "\\u%04x", ch);
        aBuf.Append(NS_ConvertASCIItoUCS2(buf));
      } else {
        aBuf.Append(ch);
      }
    }
  }
  else {
    unsigned char* cp = (unsigned char*)mText.Get1b() + aOffset;
    const unsigned char* end = cp + aLen;
    while (cp < end) {
      PRUnichar ch = *cp++;
      if (ch == '\r') {
        aBuf.Append(NS_LITERAL_STRING("\\r"));
      } else if (ch == '\n') {
        aBuf.Append(NS_LITERAL_STRING("\\n"));
      } else if (ch == '\t') {
        aBuf.Append(NS_LITERAL_STRING("\\t"));
      } else if ((ch < ' ') || (ch >= 127)) {
        char buf[10];
        PR_snprintf(buf, sizeof(buf), "\\u%04x", ch);
        aBuf.Append(NS_ConvertASCIItoUCS2(buf));
      } else {
        aBuf.Append(ch);
      }
    }
  }
}

nsresult
nsGenericDOMDataNode::GetDocument(nsIDocument*& aResult) const
{
  aResult = mDocument;
  NS_IF_ADDREF(mDocument);
  return NS_OK;
}


nsresult
nsGenericDOMDataNode::SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers)
{
  // If we were part of a document, make sure we get rid of the
  // script context reference to our script object so that our
  // script object can be freed (or collected).
  if (mDocument) {
    nsCOMPtr<nsIScriptGlobalObject> globalObject;
    mDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));
    if (globalObject) {
      nsCOMPtr<nsIScriptContext> context;
      if (NS_OK == globalObject->GetContext(getter_AddRefs(context)) && context) {
        //        context->RemoveReference((void *)&mScriptObject,
        //                                 mScriptObject);

        // XXX: UnRoot!
      }
    }
  }

  mDocument = aDocument;

  // If we already have a script object and now we're being added
  // to a document, make sure that the script context adds a 
  // reference to our script object. This will ensure that it
  // won't be freed (or collected) out from under us.
  if (mDocument) {
    nsCOMPtr<nsIScriptGlobalObject> globalObject;
    mDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));
    if (globalObject) {
      nsCOMPtr<nsIScriptContext> context;
      if (NS_OK == globalObject->GetContext(getter_AddRefs(context)) && context) {
        //        context->AddNamedReference((void *)&mScriptObject,
        //                                   mScriptObject,
        //                                   "Text");

        // XXX: Root!
      }
    }
  }

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::GetParent(nsIContent*& aResult) const
{
  NS_IF_ADDREF(mParent);
  aResult = mParent;
  return NS_OK;;
}

nsresult
nsGenericDOMDataNode::SetParent(nsIContent* aParent)
{
  mParent = aParent;
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::HandleDOMEvent(nsIPresContext* aPresContext,
                                     nsEvent* aEvent,
                                     nsIDOMEvent** aDOMEvent,
                                     PRUint32 aFlags,
                                     nsEventStatus* aEventStatus)
{
  nsresult ret = NS_OK;
  nsIDOMEvent* domEvent = nsnull;

  if (NS_EVENT_FLAG_INIT & aFlags) {
    if (!aDOMEvent) {
      aDOMEvent = &domEvent;
    }
    aEvent->flags = aFlags;
    aFlags &= ~(NS_EVENT_FLAG_CANT_BUBBLE | NS_EVENT_FLAG_CANT_CANCEL);
  }

  PRBool intermediateCapture = PR_TRUE;
  //Capturing stage evaluation
  if (NS_EVENT_FLAG_BUBBLE != aFlags) {
    //Initiate capturing phase.  Special case first call to document
    if (mParent) {
      mParent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_CAPTURE, aEventStatus);
    }
    else if (mDocument != nsnull) {
        ret = mDocument->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                        NS_EVENT_FLAG_CAPTURE, aEventStatus);
    }
  }
  
  //Local handling stage
  if (mListenerManager && !(aEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH) &&
      !(NS_EVENT_FLAG_BUBBLE & aFlags && NS_EVENT_FLAG_CANT_BUBBLE & aEvent->flags)) {
    aEvent->flags |= aFlags;
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, nsnull, aFlags, aEventStatus);
    aEvent->flags &= ~aFlags;
  }

  //Bubbling stage
  if (NS_EVENT_FLAG_CAPTURE != aFlags && mParent != nsnull) {
    ret = mParent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                  NS_EVENT_FLAG_BUBBLE, aEventStatus);
  }

  if (NS_EVENT_FLAG_INIT & aFlags) {
    // We're leaving the DOM event loop so if we created a DOM event,
    // release here.
    if (nsnull != *aDOMEvent) {
      if (0 != (*aDOMEvent)->Release()) {
        // Okay, so someone in the DOM loop (a listener, JS object)
        // still has a ref to the DOM Event but the internal data
        // hasn't been malloc'd.  Force a copy of the data here so the
        // DOM Event is still valid.
        nsIPrivateDOMEvent *privateEvent;
        if (NS_OK == (*aDOMEvent)->QueryInterface(NS_GET_IID(nsIPrivateDOMEvent), (void**)&privateEvent)) {
          privateEvent->DuplicatePrivateData();
          NS_RELEASE(privateEvent);
        }
      }
    }
    aDOMEvent = nsnull;
  }
  return ret;
}


nsresult 
nsGenericDOMDataNode::RangeAdd(nsIDOMRange& aRange)
{
  // lazy allocation of range list
  if (nsnull == mRangeList) {
    mRangeList = new nsAutoVoidArray();
  }
  if (nsnull == mRangeList) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Make sure we don't add a range that is already
  // in the list!
  PRInt32 i = mRangeList->IndexOf(&aRange);
  if (i >= 0) {
    // Range is already in the list, so there
    // is nothing to do!
    return NS_OK;
  }
  
  // dont need to addref - this call is made by the range object itself
  PRBool rv = mRangeList->AppendElement(&aRange);
  if (rv)  return NS_OK;
  return NS_ERROR_FAILURE;
}


nsresult 
nsGenericDOMDataNode::RangeRemove(nsIDOMRange& aRange)
{
  if (mRangeList) {
    // dont need to release - this call is made by the range object itself
    PRBool rv = mRangeList->RemoveElement(&aRange);
    if (rv) {
      if (mRangeList->Count() == 0) {
        delete mRangeList;
        mRangeList = nsnull;
      }
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}


nsresult 
nsGenericDOMDataNode::GetRangeList(nsVoidArray*& aResult) const
{
  aResult = mRangeList;
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::SetFocus(nsIPresContext* aPresContext)
{
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::RemoveFocus(nsIPresContext* aPresContext)
{
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::GetBindingParent(nsIContent** aContent) 
{
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::SetBindingParent(nsIContent* aParent)
{
  return NS_OK;
}  

nsresult
nsGenericDOMDataNode::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult,
                             size_t aInstanceSize) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  PRUint32 sum = 0;
#ifdef DEBUG
  sum += (PRUint32) aInstanceSize;
  sum += mText.GetLength() *
    (mText.Is2b() ? sizeof(PRUnichar) : sizeof(char));
#endif
  *aResult = sum;
  return NS_OK;
}

//----------------------------------------------------------------------

// Implementation of the nsIDOMText interface

nsresult
nsGenericDOMDataNode::SplitText(nsIContent *aOuterContent, PRUint32 aOffset,
                                nsIDOMText** aReturn)
{
  nsresult rv = NS_OK;
  nsAutoString cutText;
  PRUint32 length;

  GetLength(&length);
  if (aOffset > length) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  rv = SubstringData(aOffset, length-aOffset, cutText);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = DeleteData(aOuterContent, aOffset, length-aOffset);
  if (NS_FAILED(rv)) {
    return rv;
  }

  /*
   * Use CloneContent() for creating the new node so that the new node is of
   * same class as this node!
   */

  nsCOMPtr<nsITextContent> tmpContent(do_QueryInterface(aOuterContent, &rv));
  nsCOMPtr<nsITextContent> newContent;
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  rv = tmpContent->CloneContent(PR_FALSE, getter_AddRefs(newContent));
  if (NS_FAILED(rv)) {
    return rv;
  }


  nsCOMPtr<nsIDOMNode> newNode = do_QueryInterface(newContent, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = newNode->SetNodeValue(cutText);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIContent> parentNode;
  GetParent(*getter_AddRefs(parentNode));

  if (parentNode) {
    PRInt32 index;
    
    rv = parentNode->IndexOf(aOuterContent, index);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIContent> content(do_QueryInterface(newNode));
      
      rv = parentNode->InsertChildAt(content, index+1, PR_TRUE, PR_FALSE);
    }
  }
  
  return newNode->QueryInterface(NS_GET_IID(nsIDOMText), (void**)aReturn);
}

//----------------------------------------------------------------------

// Implementation of the nsITextContent interface

nsresult
nsGenericDOMDataNode::GetText(const nsTextFragment** aFragmentsResult)
{
  *aFragmentsResult = &mText;
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::GetTextLength(PRInt32* aLengthResult)
{
  if (!aLengthResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aLengthResult = mText.GetLength();
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::CopyText(nsAWritableString& aResult)
{
  if (mText.Is2b()) {
    aResult.Assign(mText.Get2b(), mText.GetLength());
  }
  else {
    aResult.Assign(NS_ConvertASCIItoUCS2(mText.Get1b(), mText.GetLength()).get(),
                   mText.GetLength());
  }
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::SetText(nsIContent *aOuterContent,
                              const PRUnichar* aBuffer,
                              PRInt32 aLength,
                              PRBool aNotify)
{
  NS_PRECONDITION((aLength >= 0) && (nsnull != aBuffer), "bad args");
  if (aLength < 0) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (nsnull == aBuffer) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aNotify && (nsnull != mDocument)) {
    mDocument->BeginUpdate();
  }
#ifdef IBMBIDI
  if (mDocument != nsnull) {
    PRBool bidiEnabled = mText.SetTo(aBuffer, aLength);
    if (bidiEnabled) {
      mDocument->SetBidiEnabled(PR_TRUE);
    }
  }
  else
#endif // IBMBIDI
  mText.SetTo(aBuffer, aLength);

  if (mDocument && nsGenericElement::HasMutationListeners(aOuterContent, NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED)) {
    nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(aOuterContent));
    nsMutationEvent mutation;
    mutation.eventStructType = NS_MUTATION_EVENT;
    mutation.message = NS_MUTATION_CHARACTERDATAMODIFIED;
    mutation.mTarget = node;

    // XXX Handle the setting of prevValue!
    nsAutoString newVal(aBuffer);
    if (!newVal.IsEmpty())
      mutation.mNewAttrValue = getter_AddRefs(NS_NewAtom(newVal));
    nsEventStatus status = nsEventStatus_eIgnore;
    HandleDOMEvent(nsnull, &mutation, nsnull,
                   NS_EVENT_FLAG_INIT, &status);
  }

  // Trigger a reflow
  if (aNotify && (nsnull != mDocument)) {
    mDocument->ContentChanged(aOuterContent, nsnull);
    mDocument->EndUpdate();
  }
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::SetText(nsIContent *aOuterContent, const char* aBuffer, 
                              PRInt32 aLength, PRBool aNotify)
{
  NS_PRECONDITION((aLength >= 0) && (nsnull != aBuffer), "bad args");
  if (aLength < 0) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (nsnull == aBuffer) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aNotify && (nsnull != mDocument)) {
    mDocument->BeginUpdate();
  }
  mText.SetTo(aBuffer, aLength);

  if (mDocument && nsGenericElement::HasMutationListeners(aOuterContent, NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED)) {
    nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(aOuterContent));
    nsMutationEvent mutation;
    mutation.eventStructType = NS_MUTATION_EVENT;
    mutation.message = NS_MUTATION_CHARACTERDATAMODIFIED;
    mutation.mTarget = node;

    // XXX Handle the setting of prevValue!
    nsAutoString newVal; newVal.AssignWithConversion(aBuffer);
    if (!newVal.IsEmpty())
      mutation.mNewAttrValue = getter_AddRefs(NS_NewAtom(newVal));
    nsEventStatus status = nsEventStatus_eIgnore;
    HandleDOMEvent(nsnull, &mutation, nsnull,
                   NS_EVENT_FLAG_INIT, &status);
  }

  // Trigger a reflow
  if (aNotify && (nsnull != mDocument)) {
    mDocument->ContentChanged(aOuterContent, nsnull);
    mDocument->EndUpdate();
  }
  return NS_OK;
}

nsresult 
nsGenericDOMDataNode::SetText(nsIContent *aOuterContent,
                              const nsAReadableString& aStr,
                              PRBool aNotify)
{
  if (aNotify && (nsnull != mDocument)) {
    mDocument->BeginUpdate();
  }
  mText = aStr;

  if (mDocument && nsGenericElement::HasMutationListeners(aOuterContent, NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED)) {
    nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(aOuterContent));
    nsMutationEvent mutation;
    mutation.eventStructType = NS_MUTATION_EVENT;
    mutation.message = NS_MUTATION_CHARACTERDATAMODIFIED;
    mutation.mTarget = node;

    // XXX Handle the setting of prevValue!
    nsAutoString newVal(aStr);
    if (!newVal.IsEmpty())
      mutation.mNewAttrValue = getter_AddRefs(NS_NewAtom(newVal));
    nsEventStatus status = nsEventStatus_eIgnore;
    HandleDOMEvent(nsnull, &mutation, nsnull,
                   NS_EVENT_FLAG_INIT, &status);
  }

  // Trigger a reflow
  if (aNotify && (nsnull != mDocument)) {
    mDocument->ContentChanged(aOuterContent, nsnull);
    mDocument->EndUpdate();
  }
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::IsOnlyWhitespace(PRBool* aResult)
{
  nsTextFragment& frag = mText;
  if (frag.Is2b()) {
    const PRUnichar* cp = frag.Get2b();
    const PRUnichar* end = cp + frag.GetLength();
    while (cp < end) {
      PRUnichar ch = *cp++;
      if (!XP_IS_SPACE(ch)) {
        *aResult = PR_FALSE;
        return NS_OK;
      }
    }
  }
  else {
    const char* cp = frag.Get1b();
    const char* end = cp + frag.GetLength();
    while (cp < end) {
      PRUnichar ch = PRUnichar(*(unsigned char*)cp);
      cp++;
      if (!XP_IS_SPACE(ch)) {
        *aResult = PR_FALSE;
        return NS_OK;
      }
    }
  }

  *aResult = PR_TRUE;
  return NS_OK;
}
