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
#include "mozilla/dom/Event.h"
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/EventTarget.h"
#include "nsAString.h"
#include "nsAlgorithm.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsAtom.h"
#include "nsIContent.h"
#include "nsID.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsISupportsUtils.h"
#include "nsPIDOMWindow.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nscore.h"
#include <algorithm>

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
DocumentResizeEventListener::HandleEvent(Event* aMouseEvent)
{
  RefPtr<HTMLEditor> htmlEditor = mHTMLEditorWeak.get();
  if (htmlEditor) {
    return htmlEditor->RefreshResizers();
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
ResizerMouseMotionListener::HandleEvent(Event* aMouseEvent)
{
  MouseEvent* mouseEvent = aMouseEvent->AsMouseEvent();
  if (!mouseEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  // Don't do anything special if not an HTML object resizer editor
  RefPtr<HTMLEditor> htmlEditor = mHTMLEditorWeak.get();
  if (htmlEditor) {
    // check if we have to redisplay a resizing shadow
    htmlEditor->OnMouseMove(mouseEvent);
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
  ret->AddEventListener(NS_LITERAL_STRING("mousedown"), mEventListener,
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
  return ret;
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
  CSSEditUtils::GetComputedProperty(*mTopLeftHandle, *nsGkAtoms::width,
                                    value);
  CSSEditUtils::ParseLength(value, &resizerWidth, getter_AddRefs(dummyUnit));
  CSSEditUtils::GetComputedProperty(*mTopLeftHandle, *nsGkAtoms::height,
                                    value);
  CSSEditUtils::ParseLength(value, &resizerHeight, getter_AddRefs(dummyUnit));

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

nsresult
HTMLEditor::ShowResizersInternal(Element& aResizedElement)
{
  // When we have visible resizers, we cannot show new resizers.
  // So, the caller should call HideResizers() first.
  if (NS_WARN_IF(mResizedObject)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIContent> parentContent = aResizedElement.GetParent();
  if (NS_WARN_IF(!parentContent)) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!IsDescendantOfEditorRoot(&aResizedElement))) {
    return NS_ERROR_UNEXPECTED;
  }

  // Let's create and setup resizers.  If we failed something, we should
  // cancel everything which we do in this method.
  do {
    mResizedObject = &aResizedElement;

    // The resizers and the shadow will be anonymous siblings of the element.
    mTopLeftHandle =
      CreateResizer(nsIHTMLObjectResizer::eTopLeft, *parentContent);
    if (NS_WARN_IF(!mTopLeftHandle)) {
      break;
    }
    mTopHandle = CreateResizer(nsIHTMLObjectResizer::eTop, *parentContent);
    if (NS_WARN_IF(!mTopHandle)) {
      break;
    }
    mTopRightHandle =
      CreateResizer(nsIHTMLObjectResizer::eTopRight, *parentContent);
    if (NS_WARN_IF(!mTopRightHandle)) {
      break;
    }

    mLeftHandle = CreateResizer(nsIHTMLObjectResizer::eLeft, *parentContent);
    if (NS_WARN_IF(!mLeftHandle)) {
      break;
    }
    mRightHandle = CreateResizer(nsIHTMLObjectResizer::eRight, *parentContent);
    if (NS_WARN_IF(!mRightHandle)) {
      break;
    }

    mBottomLeftHandle =
      CreateResizer(nsIHTMLObjectResizer::eBottomLeft, *parentContent);
    if (NS_WARN_IF(!mBottomLeftHandle)) {
      break;
    }
    mBottomHandle =
      CreateResizer(nsIHTMLObjectResizer::eBottom, *parentContent);
    if (NS_WARN_IF(!mBottomHandle)) {
      break;
    }
    mBottomRightHandle =
      CreateResizer(nsIHTMLObjectResizer::eBottomRight, *parentContent);
    if (NS_WARN_IF(!mBottomRightHandle)) {
      break;
    }

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
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    // and let's set their absolute positions in the document
    rv = SetAllResizersPosition();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    // now, let's create the resizing shadow
    mResizingShadow = CreateShadow(*parentContent, aResizedElement);
    if (NS_WARN_IF(!mResizingShadow)) {
      break;
    }
    // and set its position
    rv = SetShadowPosition(mResizingShadow, &aResizedElement,
                           mResizedObjectX, mResizedObjectY);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    // and then the resizing info tooltip
    mResizingInfo = CreateResizingInfo(*parentContent);
    if (NS_WARN_IF(!mResizingInfo)) {
      break;
    }

    // and listen to the "resize" event on the window first, get the
    // window from the document...
    nsIDocument* document = GetDocument();
    if (NS_WARN_IF(!document)) {
      break;
    }

    nsCOMPtr<EventTarget> target = do_QueryInterface(document->GetWindow());
    if (!target) {
      break;
    }

    mResizeEventListenerP = new DocumentResizeEventListener(*this);
    rv = target->AddEventListener(NS_LITERAL_STRING("resize"),
                                  mResizeEventListenerP, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    MOZ_ASSERT(mResizedObject == &aResizedElement);

    mHasShownResizers = true;

    // XXX Even when it failed to add event listener, should we need to set
    //     _moz_resizing attribute?
    aResizedElement.SetAttr(kNameSpaceID_None, nsGkAtoms::_moz_resizing,
                            NS_LITERAL_STRING("true"), true);
    return NS_OK;
  } while (true);

  DebugOnly<nsresult> rv = HideResizers();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Failed to clean up unnecessary resizers");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
HTMLEditor::HideResizers()
{
  if (NS_WARN_IF(!mResizedObject)) {
    return NS_OK;
  }

  // get the presshell's document observer interface.
  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  NS_WARNING_ASSERTION(presShell, "There is no presShell");
  // We allow the pres shell to be null; when it is, we presume there
  // are no document observers to notify, but we still want to
  // UnbindFromTree.

  NS_NAMED_LITERAL_STRING(mousedown, "mousedown");

  // HTMLEditor should forget all members related to resizers first since
  // removing a part of UI may cause showing the resizers again.  In such
  // case, the members may be overwritten by ShowResizers() and this will
  // lose the chance to release the old resizers.
  ManualNACPtr topLeftHandle(std::move(mTopLeftHandle));
  ManualNACPtr topHandle(std::move(mTopHandle));
  ManualNACPtr topRightHandle(std::move(mTopRightHandle));
  ManualNACPtr leftHandle(std::move(mLeftHandle));
  ManualNACPtr rightHandle(std::move(mRightHandle));
  ManualNACPtr bottomLeftHandle(std::move(mBottomLeftHandle));
  ManualNACPtr bottomHandle(std::move(mBottomHandle));
  ManualNACPtr bottomRightHandle(std::move(mBottomRightHandle));
  ManualNACPtr resizingShadow(std::move(mResizingShadow));
  ManualNACPtr resizingInfo(std::move(mResizingInfo));
  RefPtr<Element> activatedHandle(std::move(mActivatedHandle));
  nsCOMPtr<nsIDOMEventListener> mouseMotionListener(
                                  std::move(mMouseMotionListenerP));
  nsCOMPtr<nsIDOMEventListener> resizeEventListener(
                                  std::move(mResizeEventListenerP));
  RefPtr<Element> resizedObject(std::move(mResizedObject));

  // Remvoe all handles.
  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             std::move(topLeftHandle), presShell);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             std::move(topHandle), presShell);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             std::move(topRightHandle), presShell);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             std::move(leftHandle), presShell);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             std::move(rightHandle), presShell);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             std::move(bottomLeftHandle), presShell);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             std::move(bottomHandle), presShell);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             std::move(bottomRightHandle), presShell);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             std::move(resizingShadow), presShell);

  RemoveListenerAndDeleteRef(mousedown, mEventListener, true,
                             std::move(resizingInfo), presShell);

  // Remove active state of a resizer.
  if (activatedHandle) {
    activatedHandle->UnsetAttr(kNameSpaceID_None, nsGkAtoms::_moz_activated,
                               true);
  }

  // Remove resizing state of the target element.
  resizedObject->UnsetAttr(kNameSpaceID_None, nsGkAtoms::_moz_resizing, true);

  // Remove mousemove event listener from the event target.
  nsCOMPtr<EventTarget> target = GetDOMEventTarget();
  NS_WARNING_ASSERTION(target, "GetDOMEventTarget() returned nullptr");

  if (target && mouseMotionListener) {
    target->RemoveEventListener(NS_LITERAL_STRING("mousemove"),
                                mouseMotionListener, true);
  }

  // Remove resize event listener from the window.
  if (!resizeEventListener) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> doc = GetDocument();
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_NULL_POINTER;
  }

  // nsIDocument::GetWindow() may return nullptr when HTMLEditor is destroyed
  // while the document is being unloaded.  If we cannot retrieve window as
  // expected, let's ignore it.
  nsPIDOMWindowOuter* window = doc->GetWindow();
  if (NS_WARN_IF(!window)) {
    return NS_OK;
  }

  nsCOMPtr<EventTarget> targetOfWindow = do_QueryInterface(window);
  if (NS_WARN_IF(!targetOfWindow)) {
    return NS_ERROR_NULL_POINTER;
  }
  targetOfWindow->RemoveEventListener(NS_LITERAL_STRING("resize"),
                                      resizeEventListener, false);

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
HTMLEditor::StartResizing(Element* aHandle)
{
  mIsResizing = true;
  mActivatedHandle = aHandle;
  NS_ENSURE_STATE(mActivatedHandle || !aHandle);
  mActivatedHandle->SetAttr(kNameSpaceID_None, nsGkAtoms::_moz_activated,
                            NS_LITERAL_STRING("true"), true);

  // do we want to preserve ratio or not?
  bool preserveRatio = HTMLEditUtils::IsImage(mResizedObject) &&
    Preferences::GetBool("editor.resizing.preserve_ratio", true);

  // the way we change the position/size of the shadow depends on
  // the handle
  nsAutoString locationStr;
  mActivatedHandle->GetAttribute(NS_LITERAL_STRING("anonlocation"), locationStr);
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

    EventTarget* target = GetDOMEventTarget();
    NS_ENSURE_TRUE(target, NS_ERROR_FAILURE);

    result = target->AddEventListener(NS_LITERAL_STRING("mousemove"),
                                      mMouseMotionListenerP, true);
    NS_ASSERTION(NS_SUCCEEDED(result),
                 "failed to register mouse motion listener");
  }
  return result;
}

nsresult
HTMLEditor::OnMouseDown(int32_t aClientX,
                        int32_t aClientY,
                        Element* aTarget,
                        Event* aEvent)
{
  NS_ENSURE_ARG_POINTER(aTarget);

  nsAutoString anonclass;
  aTarget->GetAttribute(NS_LITERAL_STRING("_moz_anonclass"), anonclass);

  if (anonclass.EqualsLiteral("mozResizer")) {
    // If we have an anonymous element and that element is a resizer,
    // let's start resizing!
    aEvent->PreventDefault();
    mResizerUsedCount++;
    mOriginalX = aClientX;
    mOriginalY = aClientY;
    return StartResizing(aTarget);
  }

  if (anonclass.EqualsLiteral("mozGrabber")) {
    // If we have an anonymous element and that element is a grabber,
    // let's start moving the element!
    mGrabberUsedCount++;
    mOriginalX = aClientX;
    mOriginalY = aClientY;
    return GrabberClicked();
  }

  return NS_OK;
}

nsresult
HTMLEditor::OnMouseUp(int32_t aClientX,
                      int32_t aClientY,
                      Element* aTarget)
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
                                    ResizeAt aResizeAt)
{
  int32_t result = 0;
  if (!mPreserveRatio) {
    switch (aResizeAt) {
      case ResizeAt::eX:
      case ResizeAt::eWidth:
        result = aX - mOriginalX;
        break;
      case ResizeAt::eY:
      case ResizeAt::eHeight:
        result = aY - mOriginalY;
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid resizing request");
    }
    return result;
  }

  int32_t xi = (aX - mOriginalX) * mWidthIncrementFactor;
  int32_t yi = (aY - mOriginalY) * mHeightIncrementFactor;
  float objectSizeRatio =
              ((float)mResizedObjectWidth) / ((float)mResizedObjectHeight);
  result = (xi > yi) ? xi : yi;
  switch (aResizeAt) {
    case ResizeAt::eX:
    case ResizeAt::eWidth:
      if (result == yi)
        result = (int32_t) (((float) result) * objectSizeRatio);
      result = (int32_t) (((float) result) * mWidthIncrementFactor);
      break;
    case ResizeAt::eY:
    case ResizeAt::eHeight:
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
  int32_t resized =
    mResizedObjectX +
      GetNewResizingIncrement(aX, aY, ResizeAt::eX) * mXIncrementFactor;
  int32_t max = mResizedObjectX + mResizedObjectWidth;
  return std::min(resized, max);
}

int32_t
HTMLEditor::GetNewResizingY(int32_t aX,
                            int32_t aY)
{
  int32_t resized =
    mResizedObjectY +
      GetNewResizingIncrement(aX, aY, ResizeAt::eY) * mYIncrementFactor;
  int32_t max =   mResizedObjectY + mResizedObjectHeight;
  return std::min(resized, max);
}

int32_t
HTMLEditor::GetNewResizingWidth(int32_t aX,
                                int32_t aY)
{
  int32_t resized =
    mResizedObjectWidth +
      GetNewResizingIncrement(aX, aY,
                              ResizeAt::eWidth) * mWidthIncrementFactor;
  return std::max(resized, 1);
}

int32_t
HTMLEditor::GetNewResizingHeight(int32_t aX,
                                 int32_t aY)
{
  int32_t resized =
    mResizedObjectHeight +
      GetNewResizingIncrement(aX, aY,
                              ResizeAt::eHeight) * mHeightIncrementFactor;
  return std::max(resized, 1);
}

nsresult
HTMLEditor::OnMouseMove(MouseEvent* aMouseEvent)
{
  MOZ_ASSERT(aMouseEvent);

  if (mIsResizing) {
    // we are resizing and the mouse pointer's position has changed
    // we have to resdisplay the shadow
    int32_t clientX = aMouseEvent->ClientX();
    int32_t clientY = aMouseEvent->ClientY();

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
    int32_t clientX = aMouseEvent->ClientX();
    int32_t clientY = aMouseEvent->ClientY();

    int32_t xThreshold =
      LookAndFeel::GetInt(LookAndFeel::eIntID_DragThresholdX, 1);
    int32_t yThreshold =
      LookAndFeel::GetInt(LookAndFeel::eIntID_DragThresholdY, 1);

    if (DeprecatedAbs(clientX - mOriginalX) * 2 >= xThreshold ||
        DeprecatedAbs(clientY - mOriginalY) * 2 >= yThreshold) {
      mGrabberClicked = false;
      StartMoving();
    }
  }
  if (mIsMoving) {
    int32_t clientX = aMouseEvent->ClientX();
    int32_t clientY = aMouseEvent->ClientY();

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
      RemoveAttributeWithTransaction(*mResizedObject, *nsGkAtoms::width);
    }

    if (setHeight && mResizedObject->HasAttr(kNameSpaceID_None,
                                             nsGkAtoms::height)) {
      RemoveAttributeWithTransaction(*mResizedObject, *nsGkAtoms::height);
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
      SetAttributeWithTransaction(*mResizedObject, *nsGkAtoms::width, w);
    }
    if (setHeight) {
      nsAutoString h;
      h.AppendInt(height);
      SetAttributeWithTransaction(*mResizedObject, *nsGkAtoms::height, h);
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
HTMLEditor::GetResizedObject(Element** aResizedObject)
{
  RefPtr<Element> ret = mResizedObject;
  ret.forget(aResizedObject);
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetObjectResizingEnabled(bool* aIsObjectResizingEnabled)
{
  *aIsObjectResizingEnabled = IsObjectResizerEnabled();
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::SetObjectResizingEnabled(bool aObjectResizingEnabled)
{
  EnableObjectResizer(aObjectResizingEnabled);
  return NS_OK;
}

} // namespace mozilla
