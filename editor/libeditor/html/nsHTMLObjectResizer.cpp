/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.org.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Glazman (glazman@netscape.com) (Original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsHTMLObjectResizer.h"

#include "nsIDOMEventTarget.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMText.h"

#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIEditor.h"
#include "nsIPresShell.h"
#include "nsIScriptGlobalObject.h"

#include "nsHTMLEditor.h"
#include "nsEditor.h"
#include "nsEditorUtils.h"
#include "nsHTMLEditUtils.h"

#include "nsPoint.h"

class nsHTMLEditUtils;

// ==================================================================
// ResizeEventListener
// ==================================================================
NS_IMPL_ADDREF(ResizeEventListener)
NS_IMPL_RELEASE(ResizeEventListener)

NS_INTERFACE_MAP_BEGIN(ResizeEventListener)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END

ResizeEventListener::ResizeEventListener(nsIHTMLEditor * aEditor) :
 mEditor(aEditor)
{
}

ResizeEventListener::~ResizeEventListener()
{
}

NS_IMETHODIMP
ResizeEventListener::HandleEvent(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIHTMLObjectResizer> objectResizer = do_QueryInterface(mEditor);
  objectResizer->RefreshResizers();
  return NS_OK;
}

// ==================================================================
// ResizerSelectionListener
// ==================================================================

NS_IMPL_ADDREF(ResizerSelectionListener)
NS_IMPL_RELEASE(ResizerSelectionListener)

NS_INTERFACE_MAP_BEGIN(ResizerSelectionListener)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsISelectionListener)
NS_INTERFACE_MAP_END

ResizerSelectionListener::ResizerSelectionListener(nsIHTMLEditor * aEditor) :
 mEditor(aEditor)
{
}

ResizerSelectionListener::~ResizerSelectionListener()
{
}

NS_IMETHODIMP
ResizerSelectionListener::NotifySelectionChanged(nsIDOMDocument *, nsISelection *aSelection,short aReason)
{
  if ((aReason & (nsISelectionListener::MOUSEDOWN_REASON |
                  nsISelectionListener::KEYPRESS_REASON |
                  nsISelectionListener::SELECTALL_REASON)) && aSelection) {
    // the selection changed and we need to check if we have to
    // hide and/or redisplay resizing handles
    nsCOMPtr<nsIHTMLObjectResizer> objectResizer = do_QueryInterface(mEditor);
    objectResizer->CheckResizingState(aSelection);
  }

  return NS_OK;
}

// ==================================================================
// ResizerMouseMotionListener
// ==================================================================

NS_IMPL_ADDREF(ResizerMouseMotionListener)

NS_IMPL_RELEASE(ResizerMouseMotionListener)

ResizerMouseMotionListener::ResizerMouseMotionListener(nsIHTMLEditor * aEditor):
 mEditor(aEditor)
{
}

ResizerMouseMotionListener::~ResizerMouseMotionListener() 
{
}


NS_IMETHODIMP
ResizerMouseMotionListener::MouseMove(nsIDOMEvent* aMouseEvent)
{
  NS_ENSURE_ARG_POINTER(aMouseEvent);
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
  if (!mouseEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  // Don't do anything special if not an HTML editor
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(mEditor);
  if (htmlEditor)
  {
    // check if we have to redisplay a resizing shadow
    nsCOMPtr<nsIHTMLObjectResizer> objectResizer = do_QueryInterface(htmlEditor);
    objectResizer->MouseMove(aMouseEvent);
  }

  return NS_OK;
}

NS_IMETHODIMP
ResizerMouseMotionListener::HandleEvent(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
ResizerMouseMotionListener::DragMove(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(ResizerMouseMotionListener)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseMotionListener)
NS_INTERFACE_MAP_END

// ==================================================================
// nsHTMLEditor
// ==================================================================

nsresult
nsHTMLEditor::CreateResizer(nsIDOMElement ** aReturn, PRInt16 aLocation, nsISupportsArray * aArray)
{
  // let's create a span through the element factory
  nsCOMPtr<nsIContent> newContent;
  nsresult res = CreateHTMLContent(NS_LITERAL_STRING("span"), getter_AddRefs(newContent));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMElement> newElement = do_QueryInterface(newContent);
  if (!newElement)
    return NS_ERROR_FAILURE;
  *aReturn = newElement;
  NS_ADDREF(*aReturn);

  // add the mouse listener so we can detect a click on a resizer
  nsCOMPtr<nsIDOMEventTarget> evtTarget(do_QueryInterface(*aReturn));
  evtTarget->AddEventListener(NS_LITERAL_STRING("mousedown"), mMouseListenerP, PR_TRUE);

  nsAutoString locationStr;
  switch (aLocation) {
    case nsIHTMLObjectResizer::eTopLeft:
      locationStr = kTopLeft;
      break;
    case nsIHTMLObjectResizer::eTop:
      locationStr = kTop;
      break;
    case nsIHTMLObjectResizer::eTopRight:
      locationStr = kTopRight;
      break;

    case nsIHTMLObjectResizer::eLeft:
      locationStr = kLeft;
      break;
    case nsIHTMLObjectResizer::eRight:
      locationStr = kRight;
      break;

    case nsIHTMLObjectResizer::eBottomLeft:
      locationStr = kBottomLeft;
      break;
    case nsIHTMLObjectResizer::eBottom:
      locationStr = kBottom;
      break;
    case nsIHTMLObjectResizer::eBottomRight:
      locationStr = kBottomRight;
      break;
  }


  res = newElement->SetAttribute(NS_LITERAL_STRING("_moz_anonclass"),
                                 NS_LITERAL_STRING("mozResizer"));
  if (NS_FAILED(res)) return res;
  res = newElement->SetAttribute(NS_LITERAL_STRING("anonlocation"),
                                 locationStr);
  if (NS_FAILED(res)) return res;

  return aArray->AppendElement(newContent);
}

nsresult
nsHTMLEditor::CreateShadow(nsIDOMElement ** aReturn, nsISupportsArray * aArray)
{
  // let's create an image through the element factory
  nsCOMPtr<nsIContent> newContent;
  nsAutoString name;
  if (NodeIsType(mResizedObject, NS_LITERAL_STRING("img")))
    name = NS_LITERAL_STRING("img");
  else
    name = NS_LITERAL_STRING("span");
  nsresult res = CreateHTMLContent(name, getter_AddRefs(newContent));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMElement> newElement = do_QueryInterface(newContent);
  if (!newElement)
    return NS_ERROR_FAILURE;
  *aReturn = newElement;
  NS_ADDREF(*aReturn);

  return aArray->AppendElement(newContent);
}

nsresult
nsHTMLEditor::CreateResizingInfo(nsIDOMElement ** aReturn, nsISupportsArray * aArray)
{
  // let's create an image through the element factory
  nsCOMPtr<nsIContent> newContent;
  nsresult res = CreateHTMLContent(NS_LITERAL_STRING("span"), getter_AddRefs(newContent));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMElement> newElement = do_QueryInterface(newContent);
  if (NS_FAILED(res) || !newElement)
    return NS_ERROR_FAILURE;
  *aReturn = newElement;
  NS_ADDREF(*aReturn);

  res = newElement->SetAttribute(NS_LITERAL_STRING("_moz_anonclass"),
                                 NS_LITERAL_STRING("mozResizingInfo"));
  if (NS_FAILED(res)) return res;
  res = newElement->SetAttribute(NS_LITERAL_STRING("class"),
                                 NS_LITERAL_STRING("hidden"));
  if (NS_FAILED(res)) return res;

  return aArray->AppendElement(newContent);
}

void
nsHTMLEditor::SetResizerPosition(PRInt32 aX, PRInt32 aY, nsIDOMElement *aResizer)
{
  nsAutoString x, y;
  x.AppendInt(aX);
  y.AppendInt(aY);

  mHTMLCSSUtils->SetCSSProperty(aResizer,
                                nsIEditProperty::cssLeft,
                                x + NS_LITERAL_STRING("px"),
                                PR_TRUE);
  mHTMLCSSUtils->SetCSSProperty(aResizer,
                                nsIEditProperty::cssTop,
                                y + NS_LITERAL_STRING("px"),
                                PR_TRUE);
}

nsresult
nsHTMLEditor::SetAllResizersPosition(nsIDOMElement * aResizedElement, PRInt32 & aX, PRInt32 & aY)
{
  nsCOMPtr<nsIDOMNSHTMLElement> nsElement = do_QueryInterface(aResizedElement);
  if (!nsElement) {return NS_ERROR_NULL_POINTER; }

  GetElementOrigin(aResizedElement, aX, aY);

  mResizedObjectX = aX;
  mResizedObjectY = aY;

  nsresult res = nsElement->GetOffsetWidth(&mResizedObjectWidth );
  if (NS_FAILED(res)) return res;
  res = nsElement->GetOffsetHeight(&mResizedObjectHeight);
  if (NS_FAILED(res)) return res;
  PRInt32 w = mResizedObjectWidth;
  PRInt32 h = mResizedObjectHeight;

  // now let's place all the resizers around the image
  SetResizerPosition(aX-4,     aY-4, mTopLeftHandle);
  SetResizerPosition(aX+w/2-3, aY-4, mTopHandle);
  SetResizerPosition(aX+w-2,   aY-4, mTopRightHandle);

  SetResizerPosition(aX-4,     aY+h/2-3, mLeftHandle);
  SetResizerPosition(aX+w-2,   aY+h/2-3, mRightHandle);

  SetResizerPosition(aX-4,     aY+h-2, mBottomLeftHandle);
  SetResizerPosition(aX+w/2-3, aY+h-2, mBottomHandle);
  SetResizerPosition(aX+w-2,   aY+h-2, mBottomRightHandle);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::RefreshResizers()
{
  PRInt32 x, y;
  nsresult res = SetAllResizersPosition(mResizedObject, x, y);
  if (NS_FAILED(res)) return res;
  return SetShadowPosition(mResizedObject, x, y);
}

NS_IMETHODIMP 
nsHTMLEditor::ShowResizers(nsIDOMElement *aResizedElement)
{
  // we are going to need the PresShell
  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsISupportsArray> anonymousItems;
  NS_NewISupportsArray(getter_AddRefs(anonymousItems));
  if (!anonymousItems) return NS_ERROR_NULL_POINTER;

  mResizedObject = aResizedElement;

  // the resizers and the shadow will be anonymous children of the body
  nsCOMPtr<nsIDOMElement> bodyElement;
  nsresult res = nsEditor::GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  if (!bodyElement)   return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIContent> bodyContent( do_QueryInterface(bodyElement) );

  // let's create the resizers
  res = CreateResizer(getter_AddRefs(mTopLeftHandle),
                      nsIHTMLObjectResizer::eTopLeft, anonymousItems);
  if (NS_FAILED(res)) return res;
  res = CreateResizer(getter_AddRefs(mTopHandle),
                      nsIHTMLObjectResizer::eTop,  anonymousItems);
  if (NS_FAILED(res)) return res;
  res = CreateResizer(getter_AddRefs(mTopRightHandle),
                      nsIHTMLObjectResizer::eTopRight,  anonymousItems);
  if (NS_FAILED(res)) return res;

  res = CreateResizer(getter_AddRefs(mLeftHandle),
                      nsIHTMLObjectResizer::eLeft,  anonymousItems);
  if (NS_FAILED(res)) return res;
  res = CreateResizer(getter_AddRefs(mRightHandle),
                      nsIHTMLObjectResizer::eRight,  anonymousItems);
  if (NS_FAILED(res)) return res;

  res = CreateResizer(getter_AddRefs(mBottomLeftHandle),
                      nsIHTMLObjectResizer::eBottomLeft,  anonymousItems);
  if (NS_FAILED(res)) return res;
  res = CreateResizer(getter_AddRefs(mBottomHandle),
                      nsIHTMLObjectResizer::eBottom,  anonymousItems);
  if (NS_FAILED(res)) return res;
  res = CreateResizer(getter_AddRefs(mBottomRightHandle),
                      nsIHTMLObjectResizer::eBottomRight,  anonymousItems);
  if (NS_FAILED(res)) return res;

  // and let's set their absolute positions in the document
  PRInt32 x, y;
  res = SetAllResizersPosition(aResizedElement, x, y);
  if (NS_FAILED(res)) return res;

  // now, let's create the resizing shadow
  res = CreateShadow(getter_AddRefs(mResizingShadow), anonymousItems);
  if (NS_FAILED(res)) return res;
  // and set its position
  res = SetShadowPosition(aResizedElement, x, y);
  if (NS_FAILED(res)) return res;
  mResizingShadow->SetAttribute(NS_LITERAL_STRING("class"),
                                NS_LITERAL_STRING("hidden"));


  // and then the resizing info tooltip
  res = CreateResizingInfo(getter_AddRefs(mResizingInfo), anonymousItems);
  if (NS_FAILED(res)) return res;

  PRUint32 count = 0;
  anonymousItems->Count(&count);

  // we are going to need the document too
  nsCOMPtr<nsIDOMDocument> domDoc;
  GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if (!doc) return NS_ERROR_NULL_POINTER;

  for (PRUint32 i=0; i < count; i++) {
    // for each anonymous node we created, set what's needed to make it work
    // and create its frames
    nsCOMPtr<nsIContent> content;
    if (NS_FAILED(anonymousItems->QueryElementAt(i, NS_GET_IID(nsIContent), getter_AddRefs(content))))
      continue;

    content->SetNativeAnonymous(PR_TRUE);
    content->SetParent(bodyContent);
    content->SetDocument(doc, PR_TRUE, PR_TRUE);
    content->SetBindingParent(content);
    ps->RecreateFramesFor(content);
  }

  // and listen to the "resize" event on the window
  // first, get the script global object from the document...
  nsCOMPtr<nsIScriptGlobalObject> global;
  res = doc->GetScriptGlobalObject(getter_AddRefs(global));
  if (NS_FAILED(res)) return res;
  if (!global) { return NS_ERROR_NULL_POINTER; }

  mResizeEventListenerP = new ResizeEventListener(this);
  if (!mResizeEventListenerP) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(global);
  res = target->AddEventListener(NS_LITERAL_STRING("resize"), mResizeEventListenerP, PR_FALSE);

  return res;
}

void
nsHTMLEditor::DeleteRefToAnonymousNode(nsIDOMElement* aElement,
                                       nsIContent * aParentContent,
                                       nsIDocumentObserver * aDocObserver)
{
  // call ContentRemoved() for the anonymous content
  // node so its references get removed from the frame manager's
  // undisplay map, and its layout frames get destroyed!

  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  if (content) {
    aDocObserver->ContentRemoved(nsnull, aParentContent, content, -1);
    content->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    content->SetParent(nsnull);
  }
}  

NS_IMETHODIMP 
nsHTMLEditor::HideResizers(void)
{
  if (!mIsShowingResizeHandles || !mResizedObject)
    return NS_OK;

  // get the presshell's document observer interface.

  if (!mPresShellWeak) return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
  if (!ps) return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDocumentObserver> docObserver(do_QueryInterface(ps));
  if (!docObserver) return NS_ERROR_FAILURE;

  // get the root content node.

  nsCOMPtr<nsIDOMElement> bodyElement;
  nsresult res = nsEditor::GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  if (!bodyElement)   return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIContent> bodyContent( do_QueryInterface(bodyElement) );
  if (!bodyContent) return NS_ERROR_FAILURE;

  DeleteRefToAnonymousNode(mTopLeftHandle, bodyContent, docObserver);
  mTopLeftHandle = nsnull;
  DeleteRefToAnonymousNode(mTopHandle, bodyContent, docObserver);
  mTopHandle = nsnull;
  DeleteRefToAnonymousNode(mTopRightHandle, bodyContent, docObserver);
  mTopRightHandle = nsnull;
  DeleteRefToAnonymousNode(mLeftHandle, bodyContent, docObserver);
  mLeftHandle = nsnull;
  DeleteRefToAnonymousNode(mRightHandle, bodyContent, docObserver);
  mRightHandle = nsnull;
  DeleteRefToAnonymousNode(mBottomLeftHandle, bodyContent, docObserver);
  mBottomLeftHandle = nsnull;
  DeleteRefToAnonymousNode(mBottomHandle, bodyContent, docObserver);
  mBottomHandle = nsnull;
  DeleteRefToAnonymousNode(mBottomRightHandle, bodyContent, docObserver);
  mBottomRightHandle = nsnull;
  DeleteRefToAnonymousNode(mResizingShadow, bodyContent, docObserver);
  mResizingShadow = nsnull;
  DeleteRefToAnonymousNode(mResizingInfo, bodyContent, docObserver);
  mResizingInfo = nsnull;

  // don't forget to remove the listeners !

  nsCOMPtr<nsIDOMEventReceiver> erP;
  res = GetDOMEventReceiver(getter_AddRefs(erP));

  if (NS_SUCCEEDED(res) && erP && mMouseMotionListenerP)
  {
    res = erP->RemoveEventListener(NS_LITERAL_STRING("mousemove"), mMouseMotionListenerP, PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(res), "failed to remove mouse motion listener");
  }
  mMouseMotionListenerP = nsnull;

  nsCOMPtr<nsIDOMDocument> domDoc;
  GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if (!doc) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsIScriptGlobalObject> global;
  res = doc->GetScriptGlobalObject(getter_AddRefs(global));
  if (NS_FAILED(res)) return res;
  if (!global) { return NS_ERROR_NULL_POINTER; }

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(global);
  if (target && mResizeEventListenerP) {
    res = target->RemoveEventListener(NS_LITERAL_STRING("resize"), mResizeEventListenerP, PR_FALSE);
    NS_ASSERTION(NS_SUCCEEDED(res), "failed to remove resize event listener");
  }
  mResizeEventListenerP = nsnull;

  mIsShowingResizeHandles = PR_FALSE;
  mResizedObject = nsnull;
  mIsResizing = PR_FALSE;

  return NS_OK;
}

void
nsHTMLEditor::HideShadowAndInfo()
{
  if (mResizingShadow)
    mResizingShadow->SetAttribute(NS_LITERAL_STRING("class"), NS_LITERAL_STRING("hidden"));
  if (mResizingInfo)
    mResizingInfo->SetAttribute(NS_LITERAL_STRING("class"), NS_LITERAL_STRING("hidden"));
}

nsresult
nsHTMLEditor::StartResizing(nsIDOMElement *aHandle)
{
  mIsResizing = PR_TRUE;

  // the way we change the position/size of the shadow depends on
  // the handle
  nsAutoString locationStr;
  aHandle->GetAttribute(NS_LITERAL_STRING("anonlocation"), locationStr);
  if (locationStr.Equals(kTopLeft))
    SetResizeIncrements(1, 1, -1, -1, PR_TRUE);
  else if (locationStr.Equals(kTop))
    SetResizeIncrements(0, 1, 0, -1, PR_FALSE);
  else if (locationStr.Equals(kTopRight))
    SetResizeIncrements(0, 1, 1, -1, PR_TRUE);
  else if (locationStr.Equals(kLeft))
    SetResizeIncrements(1, 0, -1, 0, PR_FALSE);
  else if (locationStr.Equals(kRight))
    SetResizeIncrements(0, 0, 1, 0, PR_FALSE);
  else if (locationStr.Equals(kBottomLeft))
    SetResizeIncrements(1, 0, -1, 1, PR_TRUE);
  else if (locationStr.Equals(kBottom))
    SetResizeIncrements(0, 0, 0, 1, PR_FALSE);
  else if (locationStr.Equals(kBottomRight))
    SetResizeIncrements(0, 0, 1, 1, PR_TRUE);

  // make the shadow appear
  mResizingShadow->RemoveAttribute(NS_LITERAL_STRING("class"));

  // position it
  nsAutoString w, h;
  w.AppendInt(mResizedObjectWidth);
  h.AppendInt(mResizedObjectHeight);
  mHTMLCSSUtils->SetCSSProperty(mResizingShadow,
                                nsIEditProperty::cssWidth,
                                w + NS_LITERAL_STRING("px"),
                                PR_TRUE);
  mHTMLCSSUtils->SetCSSProperty(mResizingShadow,
                                nsIEditProperty::cssHeight,
                                h + NS_LITERAL_STRING("px"),
                                PR_TRUE);

  nsresult result = NS_OK;

  if (!mMouseMotionListenerP)
  {
    // add a mouse move listener to the editor
    mMouseMotionListenerP = new ResizerMouseMotionListener(this);
    if (!mMouseMotionListenerP) {return NS_ERROR_NULL_POINTER;}

    nsCOMPtr<nsIDOMEventReceiver> erP;
    result = GetDOMEventReceiver(getter_AddRefs(erP));
    if (NS_SUCCEEDED(result) && erP)
    {
      result = erP->AddEventListener(NS_LITERAL_STRING("mousemove"), mMouseMotionListenerP, PR_TRUE);
      NS_ASSERTION(NS_SUCCEEDED(result), "failed to register mouse motion listener");
    }
    else
      HandleEventListenerError();
  }

  return result;
}

NS_IMETHODIMP 
nsHTMLEditor::MouseDown(PRInt32 aClientX, PRInt32 aClientY,
                        nsIDOMElement *aTarget)
{
  PRBool anonElement = PR_FALSE;
  if (aTarget && NS_SUCCEEDED(aTarget->HasAttribute(NS_LITERAL_STRING("_moz_anonclass"), &anonElement)))
    // we caught a click on an anonymous element
    if (anonElement) {
      nsAutoString anonclass;
      nsresult res = aTarget->GetAttribute(NS_LITERAL_STRING("_moz_anonclass"), anonclass);
      if (NS_FAILED(res)) return res;
      if (anonclass.Equals(NS_LITERAL_STRING("mozResizer"))) {
        // and that element is a resizer, let's start resizing!
        mOriginalX = aClientX;
        mOriginalY = aClientY;
        res = StartResizing(aTarget);
        if (NS_FAILED(res)) return res;
      }
      return NS_OK;
    }
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLEditor::MouseUp(PRInt32 aClientX, PRInt32 aClientY,
                      nsIDOMElement *aResizedObject)
{
  if (mIsResizing) {
    // we are resizing and release the mouse button, so let's
    // end the resizing process
    mIsResizing = PR_FALSE;
    HideShadowAndInfo();
    SetFinalSize(aClientX, aClientY);
  }

  return NS_OK;
}


void
nsHTMLEditor::SetResizeIncrements(PRInt32 aX, PRInt32 aY,
                                  PRInt32 aW, PRInt32 aH,
                                  PRBool aPreserveRatio)
{
  mXIncrementFactor = aX;
  mYIncrementFactor = aY;
  mWidthIncrementFactor = aW;
  mHeightIncrementFactor = aH;
  mPreserveRatio = aPreserveRatio;
}

nsresult
nsHTMLEditor::SetResizingInfoPosition(PRInt32 aX, PRInt32 aY, PRInt32 aW, PRInt32 aH)
{
  nsAutoString x, y;
  x.AppendInt(aX + 20);
  y.AppendInt(aY + 20);
  mHTMLCSSUtils->SetCSSProperty(mResizingInfo,
                                nsIEditProperty::cssLeft,
                                x + NS_LITERAL_STRING("px"),
                                PR_TRUE);
  mHTMLCSSUtils->SetCSSProperty(mResizingInfo,
                                nsIEditProperty::cssTop,
                                y + NS_LITERAL_STRING("px"),
                                PR_TRUE);

  nsCOMPtr<nsIDOMNode> textInfo;
  nsresult res = mResizingInfo->GetFirstChild(getter_AddRefs(textInfo));
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsIDOMNode> junk;
  if (textInfo) {
    res = mResizingInfo->RemoveChild(textInfo, getter_AddRefs(junk));
    if (NS_FAILED(res)) return res;
    textInfo = nsnull;
    junk = nsnull;
  }
  nsCOMPtr<nsIDOMDocument> domDoc;
  GetDocument(getter_AddRefs(domDoc));

  nsAutoString widthStr, heightStr, diffWidthStr, diffHeightStr;
  widthStr.AppendInt(aW);
  heightStr.AppendInt(aH);
  PRInt32 diffWidth  = aW - mResizedObjectWidth;
  PRInt32 diffHeight = aH - mResizedObjectHeight;
  if (diffWidth > 0)
    diffWidthStr = NS_LITERAL_STRING("+");
  if (diffHeight > 0)
    diffHeightStr = NS_LITERAL_STRING("+");
  diffWidthStr.AppendInt(diffWidth);
  diffHeightStr.AppendInt(diffHeight);

  nsAutoString info(widthStr + NS_LITERAL_STRING(" x ") + heightStr +
                    NS_LITERAL_STRING(" (") + diffWidthStr +
                    NS_LITERAL_STRING(", ") + diffHeightStr +
                    NS_LITERAL_STRING(")"));

  nsCOMPtr<nsIDOMText> nodeAsText;
  res = domDoc->CreateTextNode(info, getter_AddRefs(nodeAsText));
  if (NS_FAILED(res)) return res;
  textInfo = do_QueryInterface(nodeAsText);
  res =  mResizingInfo->AppendChild(textInfo, getter_AddRefs(junk));
  if (NS_FAILED(res)) return res;

  PRBool hasClass = PR_FALSE;
  if (NS_SUCCEEDED(mResizingInfo->HasAttribute(NS_LITERAL_STRING("class"), &hasClass )) && hasClass)
    res = mResizingInfo->RemoveAttribute(NS_LITERAL_STRING("class"));

  return res;
}

nsresult
nsHTMLEditor::SetShadowPosition(nsIDOMElement *aResizedObject,
                                PRInt32 aX, PRInt32 aY)
{
  nsAutoString x, y;
  x.AppendInt(aX);
  y.AppendInt(aY);
  mHTMLCSSUtils->SetCSSProperty(mResizingShadow,
                                nsIEditProperty::cssLeft,
                                x + NS_LITERAL_STRING("px"),
                                PR_TRUE);
  mHTMLCSSUtils->SetCSSProperty(mResizingShadow,
                                nsIEditProperty::cssTop,
                                y + NS_LITERAL_STRING("px"),
                                PR_TRUE);

  if(NodeIsType(mResizedObject, NS_LITERAL_STRING("img"))) {
    nsAutoString imageSource;
    nsresult res = aResizedObject->GetAttribute(NS_LITERAL_STRING("src"),
                                                imageSource);
    if (NS_FAILED(res)) return res;
    res = mResizingShadow->SetAttribute(NS_LITERAL_STRING("src"), imageSource);
    if (NS_FAILED(res)) return res;
  }
  return mResizingShadow->SetAttribute(NS_LITERAL_STRING("_moz_anonclass"),
                                       NS_LITERAL_STRING("mozResizingShadow"));
}

PRInt32
nsHTMLEditor::GetNewResizingIncrement(PRInt32 aX, PRInt32 aY, PRInt32 aID)
{
  PRInt32 result = 0;
  if (!mPreserveRatio) {
    switch (aID) {
      case kX:
      case kWidth:
        result = aX - mOriginalX;
        break;
      case kY:
      case kHeight:
        result = aY - mOriginalY;
        break;
    }
    return result;
  }

  PRInt32 xi = (aX - mOriginalX) * mWidthIncrementFactor;
  PRInt32 yi = (aY - mOriginalY) * mHeightIncrementFactor;
  float objectSizeRatio = 
              ((float)mResizedObjectWidth) / ((float)mResizedObjectHeight);
  result = (xi > yi) ? xi : yi;
  switch (aID) {
    case kX:
    case kWidth:
      if (result == yi)
        result *= objectSizeRatio;
      result *= mWidthIncrementFactor;
      break;
    case kY:
    case kHeight:
      if (result == xi)
        result /=  objectSizeRatio;
      result *= mHeightIncrementFactor;
      break;
  }
  return result;
}

PRInt32
nsHTMLEditor::GetNewResizingX(PRInt32 aX, PRInt32 aY)
{
  PRInt32 resized = mResizedObjectX +
                    GetNewResizingIncrement(aX, aY, kX) * mXIncrementFactor;
  PRInt32 max =   mResizedObjectX + mResizedObjectWidth;
  return PR_MIN(resized, max);
}

PRInt32
nsHTMLEditor::GetNewResizingY(PRInt32 aX, PRInt32 aY)
{
  PRInt32 resized = mResizedObjectY +
                    GetNewResizingIncrement(aX, aY, kY) * mYIncrementFactor;
  PRInt32 max =   mResizedObjectY + mResizedObjectHeight;
  return PR_MIN(resized, max);
}

PRInt32
nsHTMLEditor::GetNewResizingWidth(PRInt32 aX, PRInt32 aY)
{
  PRInt32 resized = mResizedObjectWidth +
                     GetNewResizingIncrement(aX, aY, kWidth) *
                         mWidthIncrementFactor;
  return PR_MAX(resized, 1);
}

PRInt32
nsHTMLEditor::GetNewResizingHeight(PRInt32 aX, PRInt32 aY)
{
  PRInt32 resized = mResizedObjectHeight +
                     GetNewResizingIncrement(aX, aY, kHeight) *
                         mHeightIncrementFactor;
  return PR_MAX(resized, 1);
}


NS_IMETHODIMP
nsHTMLEditor::MouseMove(nsIDOMEvent* aMouseEvent)
{
  if (mIsResizing) {
    // we are resizing and the mouse pointer's position has changed
    // we have to resdisplay the shadow
    nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
    PRInt32 clientX, clientY;
    mouseEvent->GetClientX(&clientX);
    mouseEvent->GetClientY(&clientY);

    nsAutoString x, y, w, h;
    PRInt32 newWidth  = GetNewResizingWidth(clientX, clientY);
    PRInt32 newHeight = GetNewResizingHeight(clientX, clientY);
    x.AppendInt(GetNewResizingX(clientX, clientY));
    y.AppendInt(GetNewResizingY(clientX, clientY));
    w.AppendInt(newWidth);
    h.AppendInt(newHeight);

    mHTMLCSSUtils->SetCSSProperty(mResizingShadow,
                                nsIEditProperty::cssLeft,
                                x + NS_LITERAL_STRING("px"),
                                PR_TRUE);
    mHTMLCSSUtils->SetCSSProperty(mResizingShadow,
                                nsIEditProperty::cssTop,
                                y + NS_LITERAL_STRING("px"),
                                PR_TRUE);
    mHTMLCSSUtils->SetCSSProperty(mResizingShadow,
                                nsIEditProperty::cssWidth,
                                w + NS_LITERAL_STRING("px"),
                                PR_TRUE);
    mHTMLCSSUtils->SetCSSProperty(mResizingShadow,
                                nsIEditProperty::cssHeight,
                                h + NS_LITERAL_STRING("px"),
                                PR_TRUE);

    SetResizingInfoPosition(clientX, clientY, newWidth, newHeight);
  }
  return NS_OK;
}

void
nsHTMLEditor::SetFinalSize(PRInt32 aX, PRInt32 aY)
{
  NS_ASSERTION(mResizedObject, "SetFinalSize() called with null mResizedObject ptr!");
  if (!mResizedObject) return;

  // we have now to set the new width and height of the resized object
  // we don't set the x and y position because we don't control that in
  // a normal HTML layout
  PRInt32 width  = GetNewResizingWidth(aX, aY);
  PRInt32 height = GetNewResizingHeight(aX, aY);
  nsAutoString w, h;
  w.AppendInt(width);
  h.AppendInt(height);
  // keep track of that size
  mResizedObjectWidth  = width;
  mResizedObjectHeight = height;

  // we need to know if we're in a CSS-enabled editor or not
  PRBool useCSS;
  GetIsCSSEnabled(&useCSS);

  // we want one transaction only from a user's point of view
  nsAutoEditBatch batchIt(this);

  NS_NAMED_LITERAL_STRING(widthStr,  "width");
  NS_NAMED_LITERAL_STRING(heightStr, "height");
  
  PRBool hasAttr = PR_FALSE;
  if (useCSS) {
    if (NS_SUCCEEDED(mResizedObject->HasAttribute(widthStr, &hasAttr)) && hasAttr)
      RemoveAttribute(mResizedObject, widthStr);

    hasAttr = PR_FALSE;
    if (NS_SUCCEEDED(mResizedObject->HasAttribute(heightStr, &hasAttr)) && hasAttr)
      RemoveAttribute(mResizedObject, heightStr);

    mHTMLCSSUtils->SetCSSProperty(mResizedObject,
                                  nsIEditProperty::cssWidth,
                                  w + NS_LITERAL_STRING("px"),
                                  PR_FALSE);
    mHTMLCSSUtils->SetCSSProperty(mResizedObject,
                                  nsIEditProperty::cssHeight,
                                  h + NS_LITERAL_STRING("px"),
                                  PR_FALSE);
  }
  else {
    // we use HTML size and remove all equivalent CSS properties

    // we set the CSS width and height to remove it later,
    // triggering an immediate reflow; otherwise, we have problems
    // with asynchronous reflow
    mHTMLCSSUtils->SetCSSProperty(mResizedObject,
                                  nsIEditProperty::cssWidth,
                                  w + NS_LITERAL_STRING("px"),
                                  PR_FALSE);
    mHTMLCSSUtils->SetCSSProperty(mResizedObject,
                                  nsIEditProperty::cssHeight,
                                  h + NS_LITERAL_STRING("px"),
                                  PR_FALSE);

    SetAttribute(mResizedObject, widthStr, w);
    SetAttribute(mResizedObject, heightStr, h);

    mHTMLCSSUtils->RemoveCSSProperty(mResizedObject,
                                     nsIEditProperty::cssWidth,
                                     NS_LITERAL_STRING(""),
                                     PR_FALSE);
    mHTMLCSSUtils->RemoveCSSProperty(mResizedObject,
                                    nsIEditProperty::cssHeight,
                                    NS_LITERAL_STRING(""),
                                    PR_FALSE);
  }
  RefreshResizers();
}

NS_IMETHODIMP
nsHTMLEditor::CheckResizingState(nsISelection *aSelection)
{
  NS_ENSURE_ARG_POINTER(aSelection);

  if (!mIsImageResizingEnabled)
    return NS_OK;

  PRBool bCollapsed;
  nsresult res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> focusNode;

  if (bCollapsed) {
    res = aSelection->GetFocusNode(getter_AddRefs(focusNode));
    if (NS_FAILED(res)) return res;
  }
  else {

    PRInt32 rangeCount;
    res = aSelection->GetRangeCount(&rangeCount);
    if (NS_FAILED(res)) return res;

    if (rangeCount == 1) {

      nsCOMPtr<nsIDOMRange> range;
      res = aSelection->GetRangeAt(0, getter_AddRefs(range));
      if (NS_FAILED(res)) return res;
      if (!range) return NS_ERROR_NULL_POINTER;

      nsCOMPtr<nsIDOMNode> startContainer, endContainer;
      res = range->GetStartContainer(getter_AddRefs(startContainer));
      if (NS_FAILED(res)) return res;
      res = range->GetEndContainer(getter_AddRefs(endContainer));
      if (NS_FAILED(res)) return res;
      PRInt32 startOffset, endOffset;
      res = range->GetStartOffset(&startOffset);
      if (NS_FAILED(res)) return res;
      res = range->GetEndOffset(&endOffset);
      if (NS_FAILED(res)) return res;

      nsCOMPtr<nsIDOMElement> focusElement;
      if (startContainer == endContainer && startOffset + 1 == endOffset) {
        res = GetSelectedElement(NS_LITERAL_STRING(""), getter_AddRefs(focusElement));
        if (NS_FAILED(res)) return res;
        if (focusElement)
          focusNode = do_QueryInterface(focusElement);
      }
      if (!focusNode) {
        res = range->GetCommonAncestorContainer(getter_AddRefs(focusNode));
        if (NS_FAILED(res)) return res;
      }
    }
    else {
      PRInt32 i;
      nsCOMPtr<nsIDOMRange> range;
      for (i = 0; i < rangeCount; i++)
      {
        res = aSelection->GetRangeAt(i, getter_AddRefs(range));
        if (NS_FAILED(res)) return res;
        nsCOMPtr<nsIDOMNode> startContainer;
        range->GetStartContainer(getter_AddRefs(startContainer));
        if (!focusNode)
          focusNode = startContainer;
        else if (focusNode != startContainer) {
          res = startContainer->GetParentNode(getter_AddRefs(focusNode));
          if (NS_FAILED(res)) return res;
          break;
        }
      }
    }
  }

  PRUint16 nodeType;
  focusNode->GetNodeType(&nodeType);
  if (nsIDOMNode::TEXT_NODE == nodeType) {
    nsCOMPtr<nsIDOMNode> parent;
    res = focusNode->GetParentNode(getter_AddRefs(parent));
    if (NS_FAILED(res)) return res;
    focusNode = parent;
  }

  if (focusNode) {

    nsCOMPtr<nsIDOMElement>element;
    element = do_QueryInterface(focusNode);
    nsAutoString tagName;
    res = element->GetTagName(tagName);
    if (NS_FAILED(res)) return res;

    ToLowerCase(tagName);
    nsCOMPtr<nsIAtom> tagAtom = getter_AddRefs(NS_NewAtom(tagName));
    if (!tagAtom) return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIDOMNode> tableContainer = GetEnclosingTable(focusNode);

    if (nsIEditProperty::img == tagAtom ||
        tableContainer) {
      mResizedObjectIsAnImage = PR_FALSE;

      if (nsIEditProperty::img == tagAtom)
        mResizedObjectIsAnImage = PR_TRUE;
      else {
        focusNode = tableContainer;
      }

      // that node is an image, we have to show resizers
      element = do_QueryInterface(focusNode);
      if (mIsShowingResizeHandles) {
        // we were already displaying resizers and we only have to
        // refresh the position
        nsCOMPtr<nsIDOMNode> resizedNode = do_QueryInterface(mResizedObject);
        if (!NodesSameType(focusNode, resizedNode)) {
          res = HideResizers();
          if (NS_FAILED(res)) return res;
          mResizedObject = element;
          res = ShowResizers(element);
        }
        else {
          mResizedObject = element;
          res = RefreshResizers();
        }
      }
      else {
        // run through the whole thing
        res = ShowResizers(element);
      }
      if (NS_FAILED(res)) return res;
      mIsShowingResizeHandles = PR_TRUE;
      return NS_OK;
    }
  }
  if (mIsShowingResizeHandles) {
    // we were showing resizers but the selection is not an image any longer
    res = HideResizers();
    // just in case we were resizing... (bug 195412)
    mIsResizing = PR_FALSE;
    if (NS_FAILED(res)) return res;
    // paranoia...
    // mIsResizing = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetResizedObject(nsIDOMElement * *aResizedObject)
{
  NS_IF_ADDREF(*aResizedObject = mResizedObject);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetResizedObjectSize(PRInt32 *aWidth, PRInt32 *aHeight)
{
  *aWidth  = mResizedObjectWidth;
  *aHeight = mResizedObjectHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetIsImageResizingEnabled(PRBool *aIsImageResizingEnabled)
{
  *aIsImageResizingEnabled = mIsImageResizingEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::SetIsImageResizingEnabled(PRBool aIsImageResizingEnabled)
{
  mIsImageResizingEnabled = aIsImageResizingEnabled;
  return NS_OK;
}
