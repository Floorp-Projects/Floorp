/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/LookAndFeel.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Preferences.h"
#include "mozilla/mozalloc.h"
#include "nsAString.h"
#include "nsAlgorithm.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsEditProperty.h"
#include "nsEditorUtils.h"
#include "nsError.h"
#include "nsHTMLCSSUtils.h"
#include "nsHTMLEditUtils.h"
#include "nsHTMLEditor.h"
#include "nsHTMLObjectResizer.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsID.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMNode.h"
#include "nsIDOMText.h"
#include "nsIDocument.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsIHTMLObjectResizeListener.h"
#include "nsIHTMLObjectResizer.h"
#include "nsIPresShell.h"
#include "nsISupportsUtils.h"
#include "nsPIDOMWindow.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsSubstringTuple.h"
#include "nscore.h"
#include <algorithm>

class nsISelection;

using namespace mozilla;

class nsHTMLEditUtils;

// ==================================================================
// DocumentResizeEventListener
// ==================================================================
NS_IMPL_ISUPPORTS(DocumentResizeEventListener, nsIDOMEventListener)

DocumentResizeEventListener::DocumentResizeEventListener(nsIHTMLEditor * aEditor) 
{
  mEditor = do_GetWeakReference(aEditor);
}

DocumentResizeEventListener::~DocumentResizeEventListener()
{
}

NS_IMETHODIMP
DocumentResizeEventListener::HandleEvent(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIHTMLObjectResizer> objectResizer = do_QueryReferent(mEditor);
  if (objectResizer)
    return objectResizer->RefreshResizers();
  return NS_OK;
}

// ==================================================================
// ResizerSelectionListener
// ==================================================================

NS_IMPL_ISUPPORTS(ResizerSelectionListener, nsISelectionListener)

ResizerSelectionListener::ResizerSelectionListener(nsIHTMLEditor * aEditor)
{
  mEditor = do_GetWeakReference(aEditor);
}

ResizerSelectionListener::~ResizerSelectionListener()
{
}

NS_IMETHODIMP
ResizerSelectionListener::NotifySelectionChanged(nsIDOMDocument *, nsISelection *aSelection, int16_t aReason)
{
  if ((aReason & (nsISelectionListener::MOUSEDOWN_REASON |
                  nsISelectionListener::KEYPRESS_REASON |
                  nsISelectionListener::SELECTALL_REASON)) && aSelection) 
  {
    // the selection changed and we need to check if we have to
    // hide and/or redisplay resizing handles
    nsCOMPtr<nsIHTMLEditor> editor = do_QueryReferent(mEditor);
    if (editor)
      editor->CheckSelectionStateForAnonymousButtons(aSelection);
  }

  return NS_OK;
}

// ==================================================================
// ResizerMouseMotionListener
// ==================================================================

NS_IMPL_ISUPPORTS(ResizerMouseMotionListener, nsIDOMEventListener)

ResizerMouseMotionListener::ResizerMouseMotionListener(nsIHTMLEditor * aEditor)
{
  mEditor = do_GetWeakReference(aEditor);
}

ResizerMouseMotionListener::~ResizerMouseMotionListener() 
{
}

NS_IMETHODIMP
ResizerMouseMotionListener::HandleEvent(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
  if (!mouseEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  // Don't do anything special if not an HTML object resizer editor
  nsCOMPtr<nsIHTMLObjectResizer> objectResizer = do_QueryReferent(mEditor);
  if (objectResizer)
  {
    // check if we have to redisplay a resizing shadow
    objectResizer->MouseMove(aMouseEvent);
  }

  return NS_OK;
}

// ==================================================================
// nsHTMLEditor
// ==================================================================

nsresult
nsHTMLEditor::CreateResizer(nsIDOMElement ** aReturn, int16_t aLocation, nsIDOMNode * aParentNode)
{
  nsresult res = CreateAnonymousElement(NS_LITERAL_STRING("span"),
                                        aParentNode,
                                        NS_LITERAL_STRING("mozResizer"),
                                        false,
                                        aReturn);

  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(*aReturn, NS_ERROR_FAILURE);

  // add the mouse listener so we can detect a click on a resizer
  nsCOMPtr<nsIDOMEventTarget> evtTarget(do_QueryInterface(*aReturn));
  evtTarget->AddEventListener(NS_LITERAL_STRING("mousedown"), mEventListener,
                              true);

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

  res = (*aReturn)->SetAttribute(NS_LITERAL_STRING("anonlocation"),
                                 locationStr);
  return res;
}

nsresult
nsHTMLEditor::CreateShadow(nsIDOMElement ** aReturn, nsIDOMNode * aParentNode,
                           nsIDOMElement * aOriginalObject)
{
  // let's create an image through the element factory
  nsAutoString name;
  if (nsHTMLEditUtils::IsImage(aOriginalObject))
    name.AssignLiteral("img");
  else
    name.AssignLiteral("span");
  nsresult res = CreateAnonymousElement(name,
                                        aParentNode,
                                        NS_LITERAL_STRING("mozResizingShadow"),
                                        true,
                                        aReturn);

  NS_ENSURE_TRUE(*aReturn, NS_ERROR_FAILURE);

  return res;
}

nsresult
nsHTMLEditor::CreateResizingInfo(nsIDOMElement ** aReturn, nsIDOMNode * aParentNode)
{
  // let's create an info box through the element factory
  nsresult res = CreateAnonymousElement(NS_LITERAL_STRING("span"),
                                        aParentNode,
                                        NS_LITERAL_STRING("mozResizingInfo"),
                                        true,
                                        aReturn);

  NS_ENSURE_TRUE(*aReturn, NS_ERROR_FAILURE);

  return res;
}

nsresult
nsHTMLEditor::SetAllResizersPosition()
{
  NS_ENSURE_TRUE(mTopLeftHandle, NS_ERROR_FAILURE);

  int32_t x = mResizedObjectX;
  int32_t y = mResizedObjectY;
  int32_t w = mResizedObjectWidth;
  int32_t h = mResizedObjectHeight;

  // now let's place all the resizers around the image

  // get the size of resizers
  nsAutoString value;
  float resizerWidth, resizerHeight;
  nsCOMPtr<nsIAtom> dummyUnit;
  mHTMLCSSUtils->GetComputedProperty(mTopLeftHandle, nsEditProperty::cssWidth, value);
  mHTMLCSSUtils->ParseLength(value, &resizerWidth, getter_AddRefs(dummyUnit));
  mHTMLCSSUtils->GetComputedProperty(mTopLeftHandle, nsEditProperty::cssHeight, value);
  mHTMLCSSUtils->ParseLength(value, &resizerHeight, getter_AddRefs(dummyUnit));

  int32_t rw  = (int32_t)((resizerWidth + 1) / 2);
  int32_t rh =  (int32_t)((resizerHeight+ 1) / 2);

  SetAnonymousElementPosition(x-rw,     y-rh, mTopLeftHandle);
  SetAnonymousElementPosition(x+w/2-rw, y-rh, mTopHandle);
  SetAnonymousElementPosition(x+w-rw-1, y-rh, mTopRightHandle);

  SetAnonymousElementPosition(x-rw,     y+h/2-rh, mLeftHandle);
  SetAnonymousElementPosition(x+w-rw-1, y+h/2-rh, mRightHandle);

  SetAnonymousElementPosition(x-rw,     y+h-rh-1, mBottomLeftHandle);
  SetAnonymousElementPosition(x+w/2-rw, y+h-rh-1, mBottomHandle);
  SetAnonymousElementPosition(x+w-rw-1, y+h-rh-1, mBottomRightHandle);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::RefreshResizers()
{
  // nothing to do if resizers are not displayed...
  NS_ENSURE_TRUE(mResizedObject, NS_OK);

  nsresult res = GetPositionAndDimensions(mResizedObject,
                                          mResizedObjectX,
                                          mResizedObjectY,
                                          mResizedObjectWidth,
                                          mResizedObjectHeight,
                                          mResizedObjectBorderLeft,
                                          mResizedObjectBorderTop,
                                          mResizedObjectMarginLeft,
                                          mResizedObjectMarginTop);

  NS_ENSURE_SUCCESS(res, res);
  res = SetAllResizersPosition();
  NS_ENSURE_SUCCESS(res, res);
  return SetShadowPosition(mResizingShadow, mResizedObject,
                           mResizedObjectX, mResizedObjectY);
}

NS_IMETHODIMP 
nsHTMLEditor::ShowResizers(nsIDOMElement *aResizedElement)
{
  nsresult res = ShowResizersInner(aResizedElement);
  if (NS_FAILED(res))
    HideResizers();
  return res;
}

nsresult 
nsHTMLEditor::ShowResizersInner(nsIDOMElement *aResizedElement)
{
  NS_ENSURE_ARG_POINTER(aResizedElement);
  nsresult res;

  nsCOMPtr<nsIDOMNode> parentNode;
  res = aResizedElement->GetParentNode(getter_AddRefs(parentNode));
  NS_ENSURE_SUCCESS(res, res);

  if (mResizedObject) {
    NS_ERROR("call HideResizers first");
    return NS_ERROR_UNEXPECTED;
  }
  mResizedObject = aResizedElement;

  // The resizers and the shadow will be anonymous siblings of the element.
  res = CreateResizer(getter_AddRefs(mTopLeftHandle),
                      nsIHTMLObjectResizer::eTopLeft,     parentNode);
  NS_ENSURE_SUCCESS(res, res);
  res = CreateResizer(getter_AddRefs(mTopHandle),
                      nsIHTMLObjectResizer::eTop,         parentNode);
  NS_ENSURE_SUCCESS(res, res);
  res = CreateResizer(getter_AddRefs(mTopRightHandle),
                      nsIHTMLObjectResizer::eTopRight,    parentNode);
  NS_ENSURE_SUCCESS(res, res);

  res = CreateResizer(getter_AddRefs(mLeftHandle),
                      nsIHTMLObjectResizer::eLeft,        parentNode);
  NS_ENSURE_SUCCESS(res, res);
  res = CreateResizer(getter_AddRefs(mRightHandle),
                      nsIHTMLObjectResizer::eRight,       parentNode);
  NS_ENSURE_SUCCESS(res, res);

  res = CreateResizer(getter_AddRefs(mBottomLeftHandle),
                      nsIHTMLObjectResizer::eBottomLeft,  parentNode);
  NS_ENSURE_SUCCESS(res, res);
  res = CreateResizer(getter_AddRefs(mBottomHandle),
                      nsIHTMLObjectResizer::eBottom,      parentNode);
  NS_ENSURE_SUCCESS(res, res);
  res = CreateResizer(getter_AddRefs(mBottomRightHandle),
                      nsIHTMLObjectResizer::eBottomRight, parentNode);
  NS_ENSURE_SUCCESS(res, res);

  res = GetPositionAndDimensions(aResizedElement,
                                 mResizedObjectX,
                                 mResizedObjectY,
                                 mResizedObjectWidth,
                                 mResizedObjectHeight,
                                 mResizedObjectBorderLeft,
                                 mResizedObjectBorderTop,
                                 mResizedObjectMarginLeft,
                                 mResizedObjectMarginTop);
  NS_ENSURE_SUCCESS(res, res);

  // and let's set their absolute positions in the document
  res = SetAllResizersPosition();
  NS_ENSURE_SUCCESS(res, res);

  // now, let's create the resizing shadow
  res = CreateShadow(getter_AddRefs(mResizingShadow), parentNode,
                     aResizedElement);
  NS_ENSURE_SUCCESS(res, res);
  // and set its position
  res = SetShadowPosition(mResizingShadow, mResizedObject,
                          mResizedObjectX, mResizedObjectY);
  NS_ENSURE_SUCCESS(res, res);

  // and then the resizing info tooltip
  res = CreateResizingInfo(getter_AddRefs(mResizingInfo), parentNode);
  NS_ENSURE_SUCCESS(res, res);

  // and listen to the "resize" event on the window first, get the
  // window from the document...
  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(doc->GetWindow());
  if (!target) { return NS_ERROR_NULL_POINTER; }

  mResizeEventListenerP = new DocumentResizeEventListener(this);
  if (!mResizeEventListenerP) { return NS_ERROR_OUT_OF_MEMORY; }
  res = target->AddEventListener(NS_LITERAL_STRING("resize"), mResizeEventListenerP, false);

  aResizedElement->SetAttribute(NS_LITERAL_STRING("_moz_resizing"), NS_LITERAL_STRING("true"));
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::HideResizers(void)
{
  NS_ENSURE_TRUE(mResizedObject, NS_OK);

  // get the presshell's document observer interface.
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  // We allow the pres shell to be null; when it is, we presume there
  // are no document observers to notify, but we still want to
  // UnbindFromTree.

  nsresult res;
  nsCOMPtr<nsIDOMNode> parentNode;
  nsCOMPtr<nsIContent> parentContent;

  if (mTopLeftHandle) {
    res = mTopLeftHandle->GetParentNode(getter_AddRefs(parentNode));
    NS_ENSURE_SUCCESS(res, res);
    parentContent = do_QueryInterface(parentNode);
  }

  NS_NAMED_LITERAL_STRING(mousedown, "mousedown");

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             mTopLeftHandle, parentContent, ps);
  mTopLeftHandle = nullptr;

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             mTopHandle, parentContent, ps);
  mTopHandle = nullptr;

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             mTopRightHandle, parentContent, ps);
  mTopRightHandle = nullptr;

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             mLeftHandle, parentContent, ps);
  mLeftHandle = nullptr;

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             mRightHandle, parentContent, ps);
  mRightHandle = nullptr;

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             mBottomLeftHandle, parentContent, ps);
  mBottomLeftHandle = nullptr;

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             mBottomHandle, parentContent, ps);
  mBottomHandle = nullptr;

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             mBottomRightHandle, parentContent, ps);
  mBottomRightHandle = nullptr;

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             mResizingShadow, parentContent, ps);
  mResizingShadow = nullptr;

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             mResizingInfo, parentContent, ps);
  mResizingInfo = nullptr;

  if (mActivatedHandle) {
    mActivatedHandle->RemoveAttribute(NS_LITERAL_STRING("_moz_activated"));
    mActivatedHandle = nullptr;
  }

  // don't forget to remove the listeners !

  nsCOMPtr<nsIDOMEventTarget> target = GetDOMEventTarget();

  if (target && mMouseMotionListenerP)
  {
    res = target->RemoveEventListener(NS_LITERAL_STRING("mousemove"),
                                      mMouseMotionListenerP, true);
    NS_ASSERTION(NS_SUCCEEDED(res), "failed to remove mouse motion listener");
  }
  mMouseMotionListenerP = nullptr;

  nsCOMPtr<nsIDocument> doc = GetDocument();
  if (!doc) { return NS_ERROR_NULL_POINTER; }
  target = do_QueryInterface(doc->GetWindow());
  if (!target) { return NS_ERROR_NULL_POINTER; }

  if (mResizeEventListenerP) {
    res = target->RemoveEventListener(NS_LITERAL_STRING("resize"), mResizeEventListenerP, false);
    NS_ASSERTION(NS_SUCCEEDED(res), "failed to remove resize event listener");
  }
  mResizeEventListenerP = nullptr;

  mResizedObject->RemoveAttribute(NS_LITERAL_STRING("_moz_resizing"));
  mResizedObject = nullptr;

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
  // First notify the listeners if any
  int32_t listenersCount = objectResizeEventListeners.Count();
  if (listenersCount) {
    nsCOMPtr<nsIHTMLObjectResizeListener> listener;
    int32_t index;
    for (index = 0; index < listenersCount; index++) {
      listener = objectResizeEventListeners[index];
      listener->OnStartResizing(mResizedObject);
    }
  }

  mIsResizing = true;
  mActivatedHandle = aHandle;
  mActivatedHandle->SetAttribute(NS_LITERAL_STRING("_moz_activated"), NS_LITERAL_STRING("true"));

  // do we want to preserve ratio or not?
  bool preserveRatio = nsHTMLEditUtils::IsImage(mResizedObject) &&
    Preferences::GetBool("editor.resizing.preserve_ratio", true);

  // the way we change the position/size of the shadow depends on
  // the handle
  nsAutoString locationStr;
  aHandle->GetAttribute(NS_LITERAL_STRING("anonlocation"), locationStr);
  if (locationStr.Equals(kTopLeft)) {
    SetResizeIncrements(1, 1, -1, -1, preserveRatio);
  }
  else if (locationStr.Equals(kTop)) {
    SetResizeIncrements(0, 1, 0, -1, false);
  }
  else if (locationStr.Equals(kTopRight)) {
    SetResizeIncrements(0, 1, 1, -1, preserveRatio);
  }
  else if (locationStr.Equals(kLeft)) {
    SetResizeIncrements(1, 0, -1, 0, false);
  }
  else if (locationStr.Equals(kRight)) {
    SetResizeIncrements(0, 0, 1, 0, false);
  }
  else if (locationStr.Equals(kBottomLeft)) {
    SetResizeIncrements(1, 0, -1, 1, preserveRatio);
  }
  else if (locationStr.Equals(kBottom)) {
    SetResizeIncrements(0, 0, 0, 1, false);
  }
  else if (locationStr.Equals(kBottomRight)) {
    SetResizeIncrements(0, 0, 1, 1, preserveRatio);
  }

  // make the shadow appear
  mResizingShadow->RemoveAttribute(NS_LITERAL_STRING("class"));

  // position it
  mHTMLCSSUtils->SetCSSPropertyPixels(mResizingShadow,
                                      NS_LITERAL_STRING("width"),
                                      mResizedObjectWidth);
  mHTMLCSSUtils->SetCSSPropertyPixels(mResizingShadow,
                                      NS_LITERAL_STRING("height"),
                                      mResizedObjectHeight);

  // add a mouse move listener to the editor
  nsresult result = NS_OK;
  if (!mMouseMotionListenerP) {
    mMouseMotionListenerP = new ResizerMouseMotionListener(this);
    if (!mMouseMotionListenerP) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIDOMEventTarget> target = GetDOMEventTarget();
    NS_ENSURE_TRUE(target, NS_ERROR_FAILURE);

    result = target->AddEventListener(NS_LITERAL_STRING("mousemove"),
                                      mMouseMotionListenerP, true);
    NS_ASSERTION(NS_SUCCEEDED(result),
                 "failed to register mouse motion listener");
  }
  return result;
}


NS_IMETHODIMP 
nsHTMLEditor::MouseDown(int32_t aClientX, int32_t aClientY,
                        nsIDOMElement *aTarget, nsIDOMEvent* aEvent)
{
  bool anonElement = false;
  if (aTarget && NS_SUCCEEDED(aTarget->HasAttribute(NS_LITERAL_STRING("_moz_anonclass"), &anonElement)))
    // we caught a click on an anonymous element
    if (anonElement) {
      nsAutoString anonclass;
      nsresult res = aTarget->GetAttribute(NS_LITERAL_STRING("_moz_anonclass"), anonclass);
      NS_ENSURE_SUCCESS(res, res);
      if (anonclass.EqualsLiteral("mozResizer")) {
        // and that element is a resizer, let's start resizing!
        aEvent->PreventDefault();

        mOriginalX = aClientX;
        mOriginalY = aClientY;
        return StartResizing(aTarget);
      }
      if (anonclass.EqualsLiteral("mozGrabber")) {
        // and that element is a grabber, let's start moving the element!
        mOriginalX = aClientX;
        mOriginalY = aClientY;
        return GrabberClicked();
      }
    }
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLEditor::MouseUp(int32_t aClientX, int32_t aClientY,
                      nsIDOMElement *aTarget)
{
  if (mIsResizing) {
    // we are resizing and release the mouse button, so let's
    // end the resizing process
    mIsResizing = false;
    HideShadowAndInfo();
    SetFinalSize(aClientX, aClientY);
  }
  else if (mIsMoving || mGrabberClicked) {
    if (mIsMoving) {
      mPositioningShadow->SetAttribute(NS_LITERAL_STRING("class"), NS_LITERAL_STRING("hidden"));
      SetFinalPosition(aClientX, aClientY);
    }
    if (mGrabberClicked) {
      EndMoving();
    }
  }
  return NS_OK;
}


void
nsHTMLEditor::SetResizeIncrements(int32_t aX, int32_t aY,
                                  int32_t aW, int32_t aH,
                                  bool aPreserveRatio)
{
  mXIncrementFactor = aX;
  mYIncrementFactor = aY;
  mWidthIncrementFactor = aW;
  mHeightIncrementFactor = aH;
  mPreserveRatio = aPreserveRatio;
}

nsresult
nsHTMLEditor::SetResizingInfoPosition(int32_t aX, int32_t aY, int32_t aW, int32_t aH)
{
  nsCOMPtr<nsIDOMDocument> domdoc = GetDOMDocument();

  NS_NAMED_LITERAL_STRING(leftStr, "left");
  NS_NAMED_LITERAL_STRING(topStr, "top");

  // Determine the position of the resizing info box based upon the new
  // position and size of the element (aX, aY, aW, aH), and which
  // resizer is the "activated handle".  For example, place the resizing
  // info box at the bottom-right corner of the new element, if the element
  // is being resized by the bottom-right resizer.
  int32_t infoXPosition;
  int32_t infoYPosition;

  if (mActivatedHandle == mTopLeftHandle ||
      mActivatedHandle == mLeftHandle ||
      mActivatedHandle == mBottomLeftHandle)
    infoXPosition = aX;
  else if (mActivatedHandle == mTopHandle ||
             mActivatedHandle == mBottomHandle)
    infoXPosition = aX + (aW / 2);
  else
    // should only occur when mActivatedHandle is one of the 3 right-side
    // handles, but this is a reasonable default if it isn't any of them (?)
    infoXPosition = aX + aW;

  if (mActivatedHandle == mTopLeftHandle ||
      mActivatedHandle == mTopHandle ||
      mActivatedHandle == mTopRightHandle)
    infoYPosition = aY;
  else if (mActivatedHandle == mLeftHandle ||
           mActivatedHandle == mRightHandle)
    infoYPosition = aY + (aH / 2);
  else
    // should only occur when mActivatedHandle is one of the 3 bottom-side
    // handles, but this is a reasonable default if it isn't any of them (?)
    infoYPosition = aY + aH;

  // Offset info box by 20 so it's not directly under the mouse cursor.
  const int mouseCursorOffset = 20;
  mHTMLCSSUtils->SetCSSPropertyPixels(mResizingInfo, leftStr,
                                      infoXPosition + mouseCursorOffset);
  mHTMLCSSUtils->SetCSSPropertyPixels(mResizingInfo, topStr,
                                      infoYPosition + mouseCursorOffset);

  nsCOMPtr<nsIDOMNode> textInfo;
  nsresult res = mResizingInfo->GetFirstChild(getter_AddRefs(textInfo));
  NS_ENSURE_SUCCESS(res, res);
  nsCOMPtr<nsIDOMNode> junk;
  if (textInfo) {
    res = mResizingInfo->RemoveChild(textInfo, getter_AddRefs(junk));
    NS_ENSURE_SUCCESS(res, res);
    textInfo = nullptr;
    junk = nullptr;
  }

  nsAutoString widthStr, heightStr, diffWidthStr, diffHeightStr;
  widthStr.AppendInt(aW);
  heightStr.AppendInt(aH);
  int32_t diffWidth  = aW - mResizedObjectWidth;
  int32_t diffHeight = aH - mResizedObjectHeight;
  if (diffWidth > 0)
    diffWidthStr.Assign('+');
  if (diffHeight > 0)
    diffHeightStr.Assign('+');
  diffWidthStr.AppendInt(diffWidth);
  diffHeightStr.AppendInt(diffHeight);

  nsAutoString info(widthStr + NS_LITERAL_STRING(" x ") + heightStr +
                    NS_LITERAL_STRING(" (") + diffWidthStr +
                    NS_LITERAL_STRING(", ") + diffHeightStr +
                    NS_LITERAL_STRING(")"));

  nsCOMPtr<nsIDOMText> nodeAsText;
  res = domdoc->CreateTextNode(info, getter_AddRefs(nodeAsText));
  NS_ENSURE_SUCCESS(res, res);
  textInfo = do_QueryInterface(nodeAsText);
  res =  mResizingInfo->AppendChild(textInfo, getter_AddRefs(junk));
  NS_ENSURE_SUCCESS(res, res);

  bool hasClass = false;
  if (NS_SUCCEEDED(mResizingInfo->HasAttribute(NS_LITERAL_STRING("class"), &hasClass )) && hasClass)
    res = mResizingInfo->RemoveAttribute(NS_LITERAL_STRING("class"));

  return res;
}

nsresult
nsHTMLEditor::SetShadowPosition(nsIDOMElement * aShadow,
                                nsIDOMElement * aOriginalObject,
                                int32_t aOriginalObjectX,
                                int32_t aOriginalObjectY)
{
  SetAnonymousElementPosition(aOriginalObjectX, aOriginalObjectY, aShadow);

  if (nsHTMLEditUtils::IsImage(aOriginalObject)) {
    nsAutoString imageSource;
    nsresult res = aOriginalObject->GetAttribute(NS_LITERAL_STRING("src"),
                                                imageSource);
    NS_ENSURE_SUCCESS(res, res);
    res = aShadow->SetAttribute(NS_LITERAL_STRING("src"), imageSource);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}

int32_t
nsHTMLEditor::GetNewResizingIncrement(int32_t aX, int32_t aY, int32_t aID)
{
  int32_t result = 0;
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

  int32_t xi = (aX - mOriginalX) * mWidthIncrementFactor;
  int32_t yi = (aY - mOriginalY) * mHeightIncrementFactor;
  float objectSizeRatio = 
              ((float)mResizedObjectWidth) / ((float)mResizedObjectHeight);
  result = (xi > yi) ? xi : yi;
  switch (aID) {
    case kX:
    case kWidth:
      if (result == yi)
        result = (int32_t) (((float) result) * objectSizeRatio);
      result = (int32_t) (((float) result) * mWidthIncrementFactor);
      break;
    case kY:
    case kHeight:
      if (result == xi)
        result =  (int32_t) (((float) result) / objectSizeRatio);
      result = (int32_t) (((float) result) * mHeightIncrementFactor);
      break;
  }
  return result;
}

int32_t
nsHTMLEditor::GetNewResizingX(int32_t aX, int32_t aY)
{
  int32_t resized = mResizedObjectX +
                    GetNewResizingIncrement(aX, aY, kX) * mXIncrementFactor;
  int32_t max =   mResizedObjectX + mResizedObjectWidth;
  return std::min(resized, max);
}

int32_t
nsHTMLEditor::GetNewResizingY(int32_t aX, int32_t aY)
{
  int32_t resized = mResizedObjectY +
                    GetNewResizingIncrement(aX, aY, kY) * mYIncrementFactor;
  int32_t max =   mResizedObjectY + mResizedObjectHeight;
  return std::min(resized, max);
}

int32_t
nsHTMLEditor::GetNewResizingWidth(int32_t aX, int32_t aY)
{
  int32_t resized = mResizedObjectWidth +
                     GetNewResizingIncrement(aX, aY, kWidth) *
                         mWidthIncrementFactor;
  return std::max(resized, 1);
}

int32_t
nsHTMLEditor::GetNewResizingHeight(int32_t aX, int32_t aY)
{
  int32_t resized = mResizedObjectHeight +
                     GetNewResizingIncrement(aX, aY, kHeight) *
                         mHeightIncrementFactor;
  return std::max(resized, 1);
}


NS_IMETHODIMP
nsHTMLEditor::MouseMove(nsIDOMEvent* aMouseEvent)
{
  NS_NAMED_LITERAL_STRING(leftStr, "left");
  NS_NAMED_LITERAL_STRING(topStr, "top");

  if (mIsResizing) {
    // we are resizing and the mouse pointer's position has changed
    // we have to resdisplay the shadow
    nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
    int32_t clientX, clientY;
    mouseEvent->GetClientX(&clientX);
    mouseEvent->GetClientY(&clientY);

    int32_t newX = GetNewResizingX(clientX, clientY);
    int32_t newY = GetNewResizingY(clientX, clientY);
    int32_t newWidth  = GetNewResizingWidth(clientX, clientY);
    int32_t newHeight = GetNewResizingHeight(clientX, clientY);

    mHTMLCSSUtils->SetCSSPropertyPixels(mResizingShadow,
                                        leftStr,
                                        newX);
    mHTMLCSSUtils->SetCSSPropertyPixels(mResizingShadow,
                                        topStr,
                                        newY);
    mHTMLCSSUtils->SetCSSPropertyPixels(mResizingShadow,
                                        NS_LITERAL_STRING("width"),
                                        newWidth);
    mHTMLCSSUtils->SetCSSPropertyPixels(mResizingShadow,
                                        NS_LITERAL_STRING("height"),
                                        newHeight);

    return SetResizingInfoPosition(newX, newY, newWidth, newHeight);
  }

  if (mGrabberClicked) {
    nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
    int32_t clientX, clientY;
    mouseEvent->GetClientX(&clientX);
    mouseEvent->GetClientY(&clientY);

    int32_t xThreshold =
      LookAndFeel::GetInt(LookAndFeel::eIntID_DragThresholdX, 1);
    int32_t yThreshold =
      LookAndFeel::GetInt(LookAndFeel::eIntID_DragThresholdY, 1);

    if (DeprecatedAbs(clientX - mOriginalX) * 2 >= xThreshold ||
        DeprecatedAbs(clientY - mOriginalY) * 2 >= yThreshold) {
      mGrabberClicked = false;
      StartMoving(nullptr);
    }
  }
  if (mIsMoving) {
    nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aMouseEvent) );
    int32_t clientX, clientY;
    mouseEvent->GetClientX(&clientX);
    mouseEvent->GetClientY(&clientY);

    int32_t newX = mPositionedObjectX + clientX - mOriginalX;
    int32_t newY = mPositionedObjectY + clientY - mOriginalY;

    SnapToGrid(newX, newY);

    mHTMLCSSUtils->SetCSSPropertyPixels(mPositioningShadow, leftStr, newX);
    mHTMLCSSUtils->SetCSSPropertyPixels(mPositioningShadow, topStr, newY);
  }
  return NS_OK;
}

void
nsHTMLEditor::SetFinalSize(int32_t aX, int32_t aY)
{
  if (!mResizedObject) {
    // paranoia
    return;
  }

  if (mActivatedHandle) {
    mActivatedHandle->RemoveAttribute(NS_LITERAL_STRING("_moz_activated"));
    mActivatedHandle = nullptr;
  }

  // we have now to set the new width and height of the resized object
  // we don't set the x and y position because we don't control that in
  // a normal HTML layout
  int32_t left   = GetNewResizingX(aX, aY);
  int32_t top    = GetNewResizingY(aX, aY);
  int32_t width  = GetNewResizingWidth(aX, aY);
  int32_t height = GetNewResizingHeight(aX, aY);
  bool setWidth  = !mResizedObjectIsAbsolutelyPositioned || (width != mResizedObjectWidth);
  bool setHeight = !mResizedObjectIsAbsolutelyPositioned || (height != mResizedObjectHeight);
  
  int32_t x, y;
  x = left - ((mResizedObjectIsAbsolutelyPositioned) ? mResizedObjectBorderLeft+mResizedObjectMarginLeft : 0);
  y = top - ((mResizedObjectIsAbsolutelyPositioned) ? mResizedObjectBorderTop+mResizedObjectMarginTop : 0);

  // we want one transaction only from a user's point of view
  nsAutoEditBatch batchIt(this);

  NS_NAMED_LITERAL_STRING(widthStr,  "width");
  NS_NAMED_LITERAL_STRING(heightStr, "height");
  
  bool hasAttr = false;
  if (mResizedObjectIsAbsolutelyPositioned) {
    if (setHeight)
      mHTMLCSSUtils->SetCSSPropertyPixels(mResizedObject,
                                          nsEditProperty::cssTop,
                                          y,
                                          false);
    if (setWidth)
      mHTMLCSSUtils->SetCSSPropertyPixels(mResizedObject,
                                          nsEditProperty::cssLeft,
                                          x,
                                          false);
  }
  if (IsCSSEnabled() || mResizedObjectIsAbsolutelyPositioned) {
    if (setWidth && NS_SUCCEEDED(mResizedObject->HasAttribute(widthStr, &hasAttr)) && hasAttr)
      RemoveAttribute(mResizedObject, widthStr);

    hasAttr = false;
    if (setHeight && NS_SUCCEEDED(mResizedObject->HasAttribute(heightStr, &hasAttr)) && hasAttr)
      RemoveAttribute(mResizedObject, heightStr);

    if (setWidth)
      mHTMLCSSUtils->SetCSSPropertyPixels(mResizedObject,
                                          nsEditProperty::cssWidth,
                                          width,
                                          false);
    if (setHeight)
      mHTMLCSSUtils->SetCSSPropertyPixels(mResizedObject,
                                    nsEditProperty::cssHeight,
                                    height,
                                    false);
  }
  else {
    // we use HTML size and remove all equivalent CSS properties

    // we set the CSS width and height to remove it later,
    // triggering an immediate reflow; otherwise, we have problems
    // with asynchronous reflow
    if (setWidth)
      mHTMLCSSUtils->SetCSSPropertyPixels(mResizedObject,
                                          nsEditProperty::cssWidth,
                                          width,
                                          false);
    if (setHeight)
      mHTMLCSSUtils->SetCSSPropertyPixels(mResizedObject,
                                          nsEditProperty::cssHeight,
                                          height,
                                          false);

    if (setWidth) {
      nsAutoString w;
      w.AppendInt(width);
      SetAttribute(mResizedObject, widthStr, w);
    }
    if (setHeight) {
      nsAutoString h;
      h.AppendInt(height);
      SetAttribute(mResizedObject, heightStr, h);
    }

    if (setWidth)
      mHTMLCSSUtils->RemoveCSSProperty(mResizedObject,
                                       nsEditProperty::cssWidth,
                                       EmptyString(),
                                       false);
    if (setHeight)
      mHTMLCSSUtils->RemoveCSSProperty(mResizedObject,
                                      nsEditProperty::cssHeight,
                                      EmptyString(),
                                      false);
  }
  // finally notify the listeners if any
  int32_t listenersCount = objectResizeEventListeners.Count();
  if (listenersCount) {
    nsCOMPtr<nsIHTMLObjectResizeListener> listener;
    int32_t index;
    for (index = 0; index < listenersCount; index++) {
      listener = objectResizeEventListeners[index];
      listener->OnEndResizing(mResizedObject,
                              mResizedObjectWidth, mResizedObjectHeight,
                              width, height);
    }
  }

  // keep track of that size
  mResizedObjectWidth  = width;
  mResizedObjectHeight = height;

  RefreshResizers();
}

NS_IMETHODIMP
nsHTMLEditor::GetResizedObject(nsIDOMElement * *aResizedObject)
{
  *aResizedObject = mResizedObject;
  NS_IF_ADDREF(*aResizedObject);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::GetObjectResizingEnabled(bool *aIsObjectResizingEnabled)
{
  *aIsObjectResizingEnabled = mIsObjectResizingEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::SetObjectResizingEnabled(bool aObjectResizingEnabled)
{
  mIsObjectResizingEnabled = aObjectResizingEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::AddObjectResizeEventListener(nsIHTMLObjectResizeListener * aListener)
{
  NS_ENSURE_ARG_POINTER(aListener);
  if (objectResizeEventListeners.Count() &&
      objectResizeEventListeners.IndexOf(aListener) != -1) {
    /* listener already registered */
    NS_ASSERTION(false,
                 "trying to register an already registered object resize event listener");
    return NS_OK;
  }
  objectResizeEventListeners.AppendObject(aListener);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditor::RemoveObjectResizeEventListener(nsIHTMLObjectResizeListener * aListener)
{
  NS_ENSURE_ARG_POINTER(aListener);
  if (!objectResizeEventListeners.Count() ||
      objectResizeEventListeners.IndexOf(aListener) == -1) {
    /* listener was not registered */
    NS_ASSERTION(false,
                 "trying to remove an object resize event listener that was not already registered");
    return NS_OK;
  }
  objectResizeEventListeners.RemoveObject(aListener);
  return NS_OK;
}

