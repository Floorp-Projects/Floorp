/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditor.h"
#include "HTMLEditorObjectResizerUtils.h"

#include "HTMLEditUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Preferences.h"
#include "mozilla/mozalloc.h"
#include "nsAString.h"
#include "nsAlgorithm.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsAtom.h"
#include "nsIContent.h"
#include "nsID.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMNode.h"
#include "nsIDocument.h"
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

namespace mozilla {

using namespace dom;

/******************************************************************************
 * mozilla::DocumentResizeEventListener
 ******************************************************************************/

NS_IMPL_ISUPPORTS(DocumentResizeEventListener, nsIDOMEventListener)

DocumentResizeEventListener::DocumentResizeEventListener(
                               HTMLEditor& aHTMLEditor)
  : mHTMLEditorWeak(&aHTMLEditor)
{
}

NS_IMETHODIMP
DocumentResizeEventListener::HandleEvent(nsIDOMEvent* aMouseEvent)
{
  RefPtr<HTMLEditor> htmlEditor = mHTMLEditorWeak.get();
  if (htmlEditor) {
    return htmlEditor->RefreshResizers();
  }
  return NS_OK;
}

/******************************************************************************
 * mozilla::ResizerSelectionListener
 ******************************************************************************/

NS_IMPL_ISUPPORTS(ResizerSelectionListener, nsISelectionListener)

ResizerSelectionListener::ResizerSelectionListener(HTMLEditor& aHTMLEditor)
  : mHTMLEditorWeak(&aHTMLEditor)
{
}

NS_IMETHODIMP
ResizerSelectionListener::NotifySelectionChanged(nsIDOMDocument* aDOMDocument,
                                                 nsISelection* aSelection,
                                                 int16_t aReason)
{
  if ((aReason & (nsISelectionListener::MOUSEDOWN_REASON |
                  nsISelectionListener::KEYPRESS_REASON |
                  nsISelectionListener::SELECTALL_REASON)) && aSelection) {
    // the selection changed and we need to check if we have to
    // hide and/or redisplay resizing handles
    RefPtr<HTMLEditor> htmlEditor = mHTMLEditorWeak.get();
    if (htmlEditor) {
      htmlEditor->CheckSelectionStateForAnonymousButtons(aSelection);
    }
  }

  return NS_OK;
}

/******************************************************************************
 * mozilla::ResizerMouseMotionListener
 ******************************************************************************/

NS_IMPL_ISUPPORTS(ResizerMouseMotionListener, nsIDOMEventListener)

ResizerMouseMotionListener::ResizerMouseMotionListener(HTMLEditor& aHTMLEditor)
  : mHTMLEditorWeak(&aHTMLEditor)
{
}

NS_IMETHODIMP
ResizerMouseMotionListener::HandleEvent(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (!mouseEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  // Don't do anything special if not an HTML object resizer editor
  RefPtr<HTMLEditor> htmlEditor = mHTMLEditorWeak.get();
  if (htmlEditor) {
    // check if we have to redisplay a resizing shadow
    htmlEditor->MouseMove(mouseEvent);
  }

  return NS_OK;
}

/******************************************************************************
 * mozilla::HTMLEditor
 ******************************************************************************/

ManualNACPtr
HTMLEditor::CreateResizer(int16_t aLocation,
                          nsIContent& aParentContent)
{
  ManualNACPtr ret =
    CreateAnonymousElement(nsGkAtoms::span,
                           aParentContent,
                           NS_LITERAL_STRING("mozResizer"),
                           false);
  if (NS_WARN_IF(!ret)) {
    return nullptr;
  }

  // add the mouse listener so we can detect a click on a resizer
  nsCOMPtr<nsIDOMEventTarget> evtTarget = do_QueryInterface(ret);
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

  nsresult rv =
    ret->SetAttr(kNameSpaceID_None, nsGkAtoms::anonlocation, locationStr, true);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return Move(ret);
}

ManualNACPtr
HTMLEditor::CreateShadow(nsIContent& aParentContent,
                         Element& aOriginalObject)
{
  // let's create an image through the element factory
  RefPtr<nsAtom> name;
  if (HTMLEditUtils::IsImage(&aOriginalObject)) {
    name = nsGkAtoms::img;
  } else {
    name = nsGkAtoms::span;
  }

  return CreateAnonymousElement(name, aParentContent,
                                NS_LITERAL_STRING("mozResizingShadow"), true);
}

ManualNACPtr
HTMLEditor::CreateResizingInfo(nsIContent& aParentContent)
{
  // let's create an info box through the element factory
  return CreateAnonymousElement(nsGkAtoms::span, aParentContent,
                                NS_LITERAL_STRING("mozResizingInfo"), true);
}

nsresult
HTMLEditor::SetAllResizersPosition()
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
  RefPtr<nsAtom> dummyUnit;
  mCSSEditUtils->GetComputedProperty(*mTopLeftHandle, *nsGkAtoms::width,
                                     value);
  mCSSEditUtils->ParseLength(value, &resizerWidth, getter_AddRefs(dummyUnit));
  mCSSEditUtils->GetComputedProperty(*mTopLeftHandle, *nsGkAtoms::height,
                                     value);
  mCSSEditUtils->ParseLength(value, &resizerHeight, getter_AddRefs(dummyUnit));

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
HTMLEditor::RefreshResizers()
{
  // nothing to do if resizers are not displayed...
  NS_ENSURE_TRUE(mResizedObject, NS_OK);

  nsresult rv =
    GetPositionAndDimensions(
      *mResizedObject,
      mResizedObjectX,
      mResizedObjectY,
      mResizedObjectWidth,
      mResizedObjectHeight,
      mResizedObjectBorderLeft,
      mResizedObjectBorderTop,
      mResizedObjectMarginLeft,
      mResizedObjectMarginTop);

  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetAllResizersPosition();
  NS_ENSURE_SUCCESS(rv, rv);
  return SetShadowPosition(mResizingShadow, mResizedObject,
                           mResizedObjectX, mResizedObjectY);
}

NS_IMETHODIMP
HTMLEditor::ShowResizers(nsIDOMElement* aResizedElement)
{
  if (NS_WARN_IF(!aResizedElement)) {
   return NS_ERROR_NULL_POINTER;
  }
  nsCOMPtr<Element> element = do_QueryInterface(aResizedElement);
  if (NS_WARN_IF(!element)) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = ShowResizersInner(*element);
  if (NS_FAILED(rv)) {
    HideResizers();
  }
  return rv;
}

nsresult
HTMLEditor::ShowResizersInner(Element& aResizedElement)
{
  if (mResizedObject) {
    NS_ERROR("call HideResizers first");
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIContent> parentContent = aResizedElement.GetParent();
  if (NS_WARN_IF(!parentContent)) {
   return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!IsDescendantOfEditorRoot(&aResizedElement))) {
    return NS_ERROR_UNEXPECTED;
  }

  mResizedObject = &aResizedElement;

  // The resizers and the shadow will be anonymous siblings of the element.
  mTopLeftHandle =
    CreateResizer(nsIHTMLObjectResizer::eTopLeft, *parentContent);
  NS_ENSURE_TRUE(mTopLeftHandle, NS_ERROR_FAILURE);
  mTopHandle = CreateResizer(nsIHTMLObjectResizer::eTop, *parentContent);
  NS_ENSURE_TRUE(mTopHandle, NS_ERROR_FAILURE);
  mTopRightHandle =
    CreateResizer(nsIHTMLObjectResizer::eTopRight, *parentContent);
  NS_ENSURE_TRUE(mTopRightHandle, NS_ERROR_FAILURE);

  mLeftHandle = CreateResizer(nsIHTMLObjectResizer::eLeft, *parentContent);
  NS_ENSURE_TRUE(mLeftHandle, NS_ERROR_FAILURE);
  mRightHandle = CreateResizer(nsIHTMLObjectResizer::eRight, *parentContent);
  NS_ENSURE_TRUE(mRightHandle, NS_ERROR_FAILURE);

  mBottomLeftHandle =
    CreateResizer(nsIHTMLObjectResizer::eBottomLeft, *parentContent);
  NS_ENSURE_TRUE(mBottomLeftHandle, NS_ERROR_FAILURE);
  mBottomHandle = CreateResizer(nsIHTMLObjectResizer::eBottom, *parentContent);
  NS_ENSURE_TRUE(mBottomHandle, NS_ERROR_FAILURE);
  mBottomRightHandle =
    CreateResizer(nsIHTMLObjectResizer::eBottomRight, *parentContent);
  NS_ENSURE_TRUE(mBottomRightHandle, NS_ERROR_FAILURE);

  nsresult rv =
    GetPositionAndDimensions(aResizedElement,
                             mResizedObjectX,
                             mResizedObjectY,
                             mResizedObjectWidth,
                             mResizedObjectHeight,
                             mResizedObjectBorderLeft,
                             mResizedObjectBorderTop,
                             mResizedObjectMarginLeft,
                             mResizedObjectMarginTop);
  NS_ENSURE_SUCCESS(rv, rv);

  // and let's set their absolute positions in the document
  rv = SetAllResizersPosition();
  NS_ENSURE_SUCCESS(rv, rv);

  // now, let's create the resizing shadow
  mResizingShadow = CreateShadow(*parentContent, aResizedElement);
  NS_ENSURE_TRUE(mResizingShadow, NS_ERROR_FAILURE);
  // and set its position
  rv = SetShadowPosition(mResizingShadow, &aResizedElement,
                         mResizedObjectX, mResizedObjectY);
  NS_ENSURE_SUCCESS(rv, rv);

  // and then the resizing info tooltip
  mResizingInfo = CreateResizingInfo(*parentContent);
  NS_ENSURE_TRUE(mResizingInfo, NS_ERROR_FAILURE);

  // and listen to the "resize" event on the window first, get the
  // window from the document...
  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(doc->GetWindow());
  if (!target) {
    return NS_ERROR_NULL_POINTER;
  }

  mResizeEventListenerP = new DocumentResizeEventListener(*this);
  rv = target->AddEventListener(NS_LITERAL_STRING("resize"),
                                mResizeEventListenerP, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  // XXX Even when it failed to add event listener, should we need to set
  //     _moz_resizing attribute?
  aResizedElement.SetAttr(kNameSpaceID_None, nsGkAtoms::_moz_resizing,
                          NS_LITERAL_STRING("true"), true);

  MOZ_ASSERT(mResizedObject == &aResizedElement);

  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::HideResizers()
{
  NS_ENSURE_TRUE(mResizedObject, NS_OK);

  // get the presshell's document observer interface.
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  // We allow the pres shell to be null; when it is, we presume there
  // are no document observers to notify, but we still want to
  // UnbindFromTree.

  NS_NAMED_LITERAL_STRING(mousedown, "mousedown");

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             Move(mTopLeftHandle), ps);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             Move(mTopHandle), ps);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             Move(mTopRightHandle), ps);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             Move(mLeftHandle), ps);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             Move(mRightHandle), ps);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             Move(mBottomLeftHandle), ps);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             Move(mBottomHandle), ps);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             Move(mBottomRightHandle), ps);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             Move(mResizingShadow), ps);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             Move(mResizingInfo), ps);

  if (mActivatedHandle) {
    mActivatedHandle->UnsetAttr(kNameSpaceID_None, nsGkAtoms::_moz_activated,
                                true);
    mActivatedHandle = nullptr;
  }

  // don't forget to remove the listeners !

  nsCOMPtr<nsIDOMEventTarget> target = GetDOMEventTarget();

  if (target && mMouseMotionListenerP) {
    DebugOnly<nsresult> rv =
      target->RemoveEventListener(NS_LITERAL_STRING("mousemove"),
                                  mMouseMotionListenerP, true);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to remove mouse motion listener");
  }
  mMouseMotionListenerP = nullptr;

  nsCOMPtr<nsIDocument> doc = GetDocument();
  if (!doc) {
    return NS_ERROR_NULL_POINTER;
  }
  target = do_QueryInterface(doc->GetWindow());
  if (!target) {
    return NS_ERROR_NULL_POINTER;
  }

  if (mResizeEventListenerP) {
    DebugOnly<nsresult> rv =
      target->RemoveEventListener(NS_LITERAL_STRING("resize"),
                                  mResizeEventListenerP, false);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to remove resize event listener");
  }
  mResizeEventListenerP = nullptr;

  mResizedObject->UnsetAttr(kNameSpaceID_None, nsGkAtoms::_moz_resizing, true);
  mResizedObject = nullptr;

  return NS_OK;
}

void
HTMLEditor::HideShadowAndInfo()
{
  if (mResizingShadow) {
    mResizingShadow->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                             NS_LITERAL_STRING("hidden"), true);
  }
  if (mResizingInfo) {
    mResizingInfo->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                           NS_LITERAL_STRING("hidden"), true);
  }
}

nsresult
HTMLEditor::StartResizing(nsIDOMElement* aHandle)
{
  mIsResizing = true;
  mActivatedHandle = do_QueryInterface(aHandle);
  NS_ENSURE_STATE(mActivatedHandle || !aHandle);
  mActivatedHandle->SetAttr(kNameSpaceID_None, nsGkAtoms::_moz_activated,
                            NS_LITERAL_STRING("true"), true);

  // do we want to preserve ratio or not?
  bool preserveRatio = HTMLEditUtils::IsImage(mResizedObject) &&
    Preferences::GetBool("editor.resizing.preserve_ratio", true);

  // the way we change the position/size of the shadow depends on
  // the handle
  nsAutoString locationStr;
  aHandle->GetAttribute(NS_LITERAL_STRING("anonlocation"), locationStr);
  if (locationStr.Equals(kTopLeft)) {
    SetResizeIncrements(1, 1, -1, -1, preserveRatio);
  } else if (locationStr.Equals(kTop)) {
    SetResizeIncrements(0, 1, 0, -1, false);
  } else if (locationStr.Equals(kTopRight)) {
    SetResizeIncrements(0, 1, 1, -1, preserveRatio);
  } else if (locationStr.Equals(kLeft)) {
    SetResizeIncrements(1, 0, -1, 0, false);
  } else if (locationStr.Equals(kRight)) {
    SetResizeIncrements(0, 0, 1, 0, false);
  } else if (locationStr.Equals(kBottomLeft)) {
    SetResizeIncrements(1, 0, -1, 1, preserveRatio);
  } else if (locationStr.Equals(kBottom)) {
    SetResizeIncrements(0, 0, 0, 1, false);
  } else if (locationStr.Equals(kBottomRight)) {
    SetResizeIncrements(0, 0, 1, 1, preserveRatio);
  }

  // make the shadow appear
  mResizingShadow->UnsetAttr(kNameSpaceID_None, nsGkAtoms::_class, true);

  // position it
  mCSSEditUtils->SetCSSPropertyPixels(*mResizingShadow, *nsGkAtoms::width,
                                      mResizedObjectWidth);
  mCSSEditUtils->SetCSSPropertyPixels(*mResizingShadow, *nsGkAtoms::height,
                                      mResizedObjectHeight);

  // add a mouse move listener to the editor
  nsresult result = NS_OK;
  if (!mMouseMotionListenerP) {
    mMouseMotionListenerP = new ResizerMouseMotionListener(*this);
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
HTMLEditor::MouseDown(int32_t aClientX,
                      int32_t aClientY,
                      nsIDOMElement* aTarget,
                      nsIDOMEvent* aEvent)
{
  bool anonElement = false;
  if (aTarget && NS_SUCCEEDED(aTarget->HasAttribute(NS_LITERAL_STRING("_moz_anonclass"), &anonElement)))
    // we caught a click on an anonymous element
    if (anonElement) {
      nsAutoString anonclass;
      nsresult rv =
        aTarget->GetAttribute(NS_LITERAL_STRING("_moz_anonclass"), anonclass);
      NS_ENSURE_SUCCESS(rv, rv);
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
HTMLEditor::MouseUp(int32_t aClientX,
                    int32_t aClientY,
                    nsIDOMElement* aTarget)
{
  if (mIsResizing) {
    // we are resizing and release the mouse button, so let's
    // end the resizing process
    mIsResizing = false;
    HideShadowAndInfo();
    SetFinalSize(aClientX, aClientY);
  } else if (mIsMoving || mGrabberClicked) {
    if (mIsMoving) {
      mPositioningShadow->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                                  NS_LITERAL_STRING("hidden"), true);
      SetFinalPosition(aClientX, aClientY);
    }
    if (mGrabberClicked) {
      EndMoving();
    }
  }
  return NS_OK;
}

void
HTMLEditor::SetResizeIncrements(int32_t aX,
                                int32_t aY,
                                int32_t aW,
                                int32_t aH,
                                bool aPreserveRatio)
{
  mXIncrementFactor = aX;
  mYIncrementFactor = aY;
  mWidthIncrementFactor = aW;
  mHeightIncrementFactor = aH;
  mPreserveRatio = aPreserveRatio;
}

nsresult
HTMLEditor::SetResizingInfoPosition(int32_t aX,
                                    int32_t aY,
                                    int32_t aW,
                                    int32_t aH)
{
  // Determine the position of the resizing info box based upon the new
  // position and size of the element (aX, aY, aW, aH), and which
  // resizer is the "activated handle".  For example, place the resizing
  // info box at the bottom-right corner of the new element, if the element
  // is being resized by the bottom-right resizer.
  int32_t infoXPosition;
  int32_t infoYPosition;

  if (mActivatedHandle == mTopLeftHandle ||
      mActivatedHandle == mLeftHandle ||
      mActivatedHandle == mBottomLeftHandle) {
    infoXPosition = aX;
  } else if (mActivatedHandle == mTopHandle ||
             mActivatedHandle == mBottomHandle) {
    infoXPosition = aX + (aW / 2);
  } else {
    // should only occur when mActivatedHandle is one of the 3 right-side
    // handles, but this is a reasonable default if it isn't any of them (?)
    infoXPosition = aX + aW;
  }

  if (mActivatedHandle == mTopLeftHandle ||
      mActivatedHandle == mTopHandle ||
      mActivatedHandle == mTopRightHandle) {
    infoYPosition = aY;
  } else if (mActivatedHandle == mLeftHandle ||
             mActivatedHandle == mRightHandle) {
    infoYPosition = aY + (aH / 2);
  } else {
    // should only occur when mActivatedHandle is one of the 3 bottom-side
    // handles, but this is a reasonable default if it isn't any of them (?)
    infoYPosition = aY + aH;
  }

  // Offset info box by 20 so it's not directly under the mouse cursor.
  const int mouseCursorOffset = 20;
  mCSSEditUtils->SetCSSPropertyPixels(*mResizingInfo, *nsGkAtoms::left,
                                      infoXPosition + mouseCursorOffset);
  mCSSEditUtils->SetCSSPropertyPixels(*mResizingInfo, *nsGkAtoms::top,
                                      infoYPosition + mouseCursorOffset);

  nsCOMPtr<nsIContent> textInfo = mResizingInfo->GetFirstChild();
  ErrorResult erv;
  if (textInfo) {
    mResizingInfo->RemoveChild(*textInfo, erv);
    NS_ENSURE_TRUE(!erv.Failed(), erv.StealNSResult());
    textInfo = nullptr;
  }

  nsAutoString widthStr, heightStr, diffWidthStr, diffHeightStr;
  widthStr.AppendInt(aW);
  heightStr.AppendInt(aH);
  int32_t diffWidth  = aW - mResizedObjectWidth;
  int32_t diffHeight = aH - mResizedObjectHeight;
  if (diffWidth > 0) {
    diffWidthStr.Assign('+');
  }
  if (diffHeight > 0) {
    diffHeightStr.Assign('+');
  }
  diffWidthStr.AppendInt(diffWidth);
  diffHeightStr.AppendInt(diffHeight);

  nsAutoString info(widthStr + NS_LITERAL_STRING(" x ") + heightStr +
                    NS_LITERAL_STRING(" (") + diffWidthStr +
                    NS_LITERAL_STRING(", ") + diffHeightStr +
                    NS_LITERAL_STRING(")"));

  nsCOMPtr<nsIDocument> doc = GetDocument();
  textInfo = doc->CreateTextNode(info);
  if (NS_WARN_IF(!textInfo)) {
    return NS_ERROR_FAILURE;
  }
  mResizingInfo->AppendChild(*textInfo, erv);
  if (NS_WARN_IF(erv.Failed())) {
    return erv.StealNSResult();
  }

  return mResizingInfo->UnsetAttr(kNameSpaceID_None, nsGkAtoms::_class, true);
}

nsresult
HTMLEditor::SetShadowPosition(Element* aShadow,
                              Element* aOriginalObject,
                              int32_t aOriginalObjectX,
                              int32_t aOriginalObjectY)
{
  SetAnonymousElementPosition(aOriginalObjectX, aOriginalObjectY, aShadow);

  if (HTMLEditUtils::IsImage(aOriginalObject)) {
    nsAutoString imageSource;
    aOriginalObject->GetAttr(kNameSpaceID_None, nsGkAtoms::src, imageSource);
    nsresult rv = aShadow->SetAttr(kNameSpaceID_None, nsGkAtoms::src,
                                   imageSource, true);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

int32_t
HTMLEditor::GetNewResizingIncrement(int32_t aX,
                                    int32_t aY,
                                    int32_t aID)
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
HTMLEditor::GetNewResizingX(int32_t aX,
                            int32_t aY)
{
  int32_t resized = mResizedObjectX +
                    GetNewResizingIncrement(aX, aY, kX) * mXIncrementFactor;
  int32_t max =   mResizedObjectX + mResizedObjectWidth;
  return std::min(resized, max);
}

int32_t
HTMLEditor::GetNewResizingY(int32_t aX,
                            int32_t aY)
{
  int32_t resized = mResizedObjectY +
                    GetNewResizingIncrement(aX, aY, kY) * mYIncrementFactor;
  int32_t max =   mResizedObjectY + mResizedObjectHeight;
  return std::min(resized, max);
}

int32_t
HTMLEditor::GetNewResizingWidth(int32_t aX,
                                int32_t aY)
{
  int32_t resized = mResizedObjectWidth +
                     GetNewResizingIncrement(aX, aY, kWidth) *
                         mWidthIncrementFactor;
  return std::max(resized, 1);
}

int32_t
HTMLEditor::GetNewResizingHeight(int32_t aX,
                                 int32_t aY)
{
  int32_t resized = mResizedObjectHeight +
                     GetNewResizingIncrement(aX, aY, kHeight) *
                         mHeightIncrementFactor;
  return std::max(resized, 1);
}

NS_IMETHODIMP
HTMLEditor::MouseMove(nsIDOMEvent* aMouseEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aMouseEvent);
  if (NS_WARN_IF(!mouseEvent)) {
    return NS_OK;
  }
  return MouseMove(mouseEvent);
}

nsresult
HTMLEditor::MouseMove(nsIDOMMouseEvent* aMouseEvent)
{
  MOZ_ASSERT(aMouseEvent);

  if (mIsResizing) {
    // we are resizing and the mouse pointer's position has changed
    // we have to resdisplay the shadow
    int32_t clientX, clientY;
    aMouseEvent->GetClientX(&clientX);
    aMouseEvent->GetClientY(&clientY);

    int32_t newX = GetNewResizingX(clientX, clientY);
    int32_t newY = GetNewResizingY(clientX, clientY);
    int32_t newWidth  = GetNewResizingWidth(clientX, clientY);
    int32_t newHeight = GetNewResizingHeight(clientX, clientY);

    mCSSEditUtils->SetCSSPropertyPixels(*mResizingShadow, *nsGkAtoms::left,
                                        newX);
    mCSSEditUtils->SetCSSPropertyPixels(*mResizingShadow, *nsGkAtoms::top,
                                        newY);
    mCSSEditUtils->SetCSSPropertyPixels(*mResizingShadow, *nsGkAtoms::width,
                                        newWidth);
    mCSSEditUtils->SetCSSPropertyPixels(*mResizingShadow, *nsGkAtoms::height,
                                        newHeight);

    return SetResizingInfoPosition(newX, newY, newWidth, newHeight);
  }

  if (mGrabberClicked) {
    int32_t clientX, clientY;
    aMouseEvent->GetClientX(&clientX);
    aMouseEvent->GetClientY(&clientY);

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
    int32_t clientX, clientY;
    aMouseEvent->GetClientX(&clientX);
    aMouseEvent->GetClientY(&clientY);

    int32_t newX = mPositionedObjectX + clientX - mOriginalX;
    int32_t newY = mPositionedObjectY + clientY - mOriginalY;

    SnapToGrid(newX, newY);

    mCSSEditUtils->SetCSSPropertyPixels(*mPositioningShadow, *nsGkAtoms::left,
                                        newX);
    mCSSEditUtils->SetCSSPropertyPixels(*mPositioningShadow, *nsGkAtoms::top,
                                        newY);
  }
  return NS_OK;
}

void
HTMLEditor::SetFinalSize(int32_t aX,
                         int32_t aY)
{
  if (!mResizedObject) {
    // paranoia
    return;
  }

  if (mActivatedHandle) {
    mActivatedHandle->UnsetAttr(kNameSpaceID_None, nsGkAtoms::_moz_activated, true);
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
  AutoPlaceholderBatch batchIt(this);

  if (mResizedObjectIsAbsolutelyPositioned) {
    if (setHeight) {
      mCSSEditUtils->SetCSSPropertyPixels(*mResizedObject, *nsGkAtoms::top, y);
    }
    if (setWidth) {
      mCSSEditUtils->SetCSSPropertyPixels(*mResizedObject, *nsGkAtoms::left, x);
    }
  }
  if (IsCSSEnabled() || mResizedObjectIsAbsolutelyPositioned) {
    if (setWidth && mResizedObject->HasAttr(kNameSpaceID_None, nsGkAtoms::width)) {
      RemoveAttribute(mResizedObject, nsGkAtoms::width);
    }

    if (setHeight && mResizedObject->HasAttr(kNameSpaceID_None,
                                             nsGkAtoms::height)) {
      RemoveAttribute(mResizedObject, nsGkAtoms::height);
    }

    if (setWidth) {
      mCSSEditUtils->SetCSSPropertyPixels(*mResizedObject, *nsGkAtoms::width,
                                          width);
    }
    if (setHeight) {
      mCSSEditUtils->SetCSSPropertyPixels(*mResizedObject, *nsGkAtoms::height,
                                          height);
    }
  } else {
    // we use HTML size and remove all equivalent CSS properties

    // we set the CSS width and height to remove it later,
    // triggering an immediate reflow; otherwise, we have problems
    // with asynchronous reflow
    if (setWidth) {
      mCSSEditUtils->SetCSSPropertyPixels(*mResizedObject, *nsGkAtoms::width,
                                          width);
    }
    if (setHeight) {
      mCSSEditUtils->SetCSSPropertyPixels(*mResizedObject, *nsGkAtoms::height,
                                          height);
    }
    if (setWidth) {
      nsAutoString w;
      w.AppendInt(width);
      SetAttribute(mResizedObject, nsGkAtoms::width, w);
    }
    if (setHeight) {
      nsAutoString h;
      h.AppendInt(height);
      SetAttribute(mResizedObject, nsGkAtoms::height, h);
    }

    if (setWidth) {
      mCSSEditUtils->RemoveCSSProperty(*mResizedObject, *nsGkAtoms::width,
                                       EmptyString());
    }
    if (setHeight) {
      mCSSEditUtils->RemoveCSSProperty(*mResizedObject, *nsGkAtoms::height,
                                       EmptyString());
    }
  }

  // keep track of that size
  mResizedObjectWidth  = width;
  mResizedObjectHeight = height;

  RefreshResizers();
}

NS_IMETHODIMP
HTMLEditor::GetResizedObject(nsIDOMElement** aResizedObject)
{
  nsCOMPtr<nsIDOMElement> ret = static_cast<nsIDOMElement*>(GetAsDOMNode(mResizedObject));
  ret.forget(aResizedObject);
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetObjectResizingEnabled(bool* aIsObjectResizingEnabled)
{
  *aIsObjectResizingEnabled = mIsObjectResizingEnabled;
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::SetObjectResizingEnabled(bool aObjectResizingEnabled)
{
  mIsObjectResizingEnabled = aObjectResizingEnabled;
  return NS_OK;
}

} // namespace mozilla
