/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditor.h"

#include "HTMLEditorEventListener.h"
#include "HTMLEditUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
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
#include "mozilla/dom/Document.h"
#include "nsISupportsUtils.h"
#include "nsPIDOMWindow.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nscore.h"
#include <algorithm>

#define kTopLeft NS_LITERAL_STRING("nw")
#define kTop NS_LITERAL_STRING("n")
#define kTopRight NS_LITERAL_STRING("ne")
#define kLeft NS_LITERAL_STRING("w")
#define kRight NS_LITERAL_STRING("e")
#define kBottomLeft NS_LITERAL_STRING("sw")
#define kBottom NS_LITERAL_STRING("s")
#define kBottomRight NS_LITERAL_STRING("se")

namespace mozilla {

using namespace dom;

/******************************************************************************
 * mozilla::HTMLEditor
 ******************************************************************************/

ManualNACPtr HTMLEditor::CreateResizer(int16_t aLocation,
                                       nsIContent& aParentContent) {
  ManualNACPtr resizer = CreateAnonymousElement(
      nsGkAtoms::span, aParentContent, NS_LITERAL_STRING("mozResizer"), false);
  if (!resizer) {
    NS_WARNING(
        "HTMLEditor::CreateAnonymousElement(nsGkAtoms::span, mozResizer) "
        "failed");
    return nullptr;
  }

  // add the mouse listener so we can detect a click on a resizer
  DebugOnly<nsresult> rvIgnored = resizer->AddEventListener(
      NS_LITERAL_STRING("mousedown"), mEventListener, true);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "EventTarget::AddEventListener(mousedown) failed, but ignored");

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

  if (NS_FAILED(resizer->SetAttr(kNameSpaceID_None, nsGkAtoms::anonlocation,
                                 locationStr, true))) {
    NS_WARNING("Element::SetAttr(nsGkAtoms::anonlocation) failed");
    return nullptr;
  }
  return resizer;
}

ManualNACPtr HTMLEditor::CreateShadow(nsIContent& aParentContent,
                                      Element& aOriginalObject) {
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

ManualNACPtr HTMLEditor::CreateResizingInfo(nsIContent& aParentContent) {
  // let's create an info box through the element factory
  return CreateAnonymousElement(nsGkAtoms::span, aParentContent,
                                NS_LITERAL_STRING("mozResizingInfo"), true);
}

nsresult HTMLEditor::SetAllResizersPosition() {
  if (NS_WARN_IF(!mTopLeftHandle)) {
    return NS_ERROR_FAILURE;  // There are no resizers.
  }

  int32_t x = mResizedObjectX;
  int32_t y = mResizedObjectY;
  int32_t w = mResizedObjectWidth;
  int32_t h = mResizedObjectHeight;

  nsAutoString value;
  float resizerWidth, resizerHeight;
  RefPtr<nsAtom> dummyUnit;
  DebugOnly<nsresult> rvIgnored = NS_OK;
  OwningNonNull<Element> topLeftHandle = *mTopLeftHandle.get();
  // XXX Do we really need to computed value rather than specified value?
  //     Because it's an anonymous node.
  rvIgnored = CSSEditUtils::GetComputedProperty(*topLeftHandle,
                                                *nsGkAtoms::width, value);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(topLeftHandle != mTopLeftHandle)) {
    return NS_ERROR_FAILURE;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::GetComputedProperty(nsGkAtoms::width) "
                       "failed, but ignored");
  rvIgnored = CSSEditUtils::GetComputedProperty(*topLeftHandle,
                                                *nsGkAtoms::height, value);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(topLeftHandle != mTopLeftHandle)) {
    return NS_ERROR_FAILURE;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::GetComputedProperty(nsGkAtoms::height) "
                       "failed, but ignored");
  CSSEditUtils::ParseLength(value, &resizerWidth, getter_AddRefs(dummyUnit));
  CSSEditUtils::ParseLength(value, &resizerHeight, getter_AddRefs(dummyUnit));

  int32_t rw = static_cast<int32_t>((resizerWidth + 1) / 2);
  int32_t rh = static_cast<int32_t>((resizerHeight + 1) / 2);

  // While moving each resizer, mutation event listener may hide the resizers.
  // And in worst case, new resizers may be recreated.  So, we need to store
  // all resizers here, and then, if we detect a resizer is removed or replaced,
  // we should do nothing anymore.
  // FYI: Note that only checking if mTopLeftHandle is replaced is enough.
  //      We're may be in hot path if user resizes an element a lot.  So,
  //      we should just add-ref mTopLeftHandle.
  SetAnonymousElementPosition(x - rw, y - rh, topLeftHandle);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(topLeftHandle != mTopLeftHandle)) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<Element> topHandle = mTopHandle.get();
  SetAnonymousElementPosition(x + w / 2 - rw, y - rh, topHandle);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(topLeftHandle != mTopLeftHandle)) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<Element> topRightHandle = mTopRightHandle.get();
  SetAnonymousElementPosition(x + w - rw - 1, y - rh, topRightHandle);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(topLeftHandle != mTopLeftHandle)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<Element> leftHandle = mLeftHandle.get();
  SetAnonymousElementPosition(x - rw, y + h / 2 - rh, leftHandle);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(topLeftHandle != mTopLeftHandle)) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<Element> rightHandle = mRightHandle.get();
  SetAnonymousElementPosition(x + w - rw - 1, y + h / 2 - rh, rightHandle);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(topLeftHandle != mTopLeftHandle)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<Element> bottomLeftHandle = mBottomLeftHandle.get();
  SetAnonymousElementPosition(x - rw, y + h - rh - 1, bottomLeftHandle);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(topLeftHandle != mTopLeftHandle)) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<Element> bottomHandle = mBottomHandle.get();
  SetAnonymousElementPosition(x + w / 2 - rw, y + h - rh - 1, bottomHandle);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(topLeftHandle != mTopLeftHandle)) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<Element> bottomRightHandle = mBottomRightHandle.get();
  SetAnonymousElementPosition(x + w - rw - 1, y + h - rh - 1,
                              bottomRightHandle);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(topLeftHandle != mTopLeftHandle)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::RefreshResizers() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = RefreshResizersInternal();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::RefreshResizersInternal() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::RefreshResizersInternal() {
  // Don't warn even if resizers are not visible since script cannot check
  // if they are visible and this is non-virtual method.  So, the cost of
  // calling this can be ignored.
  if (!mResizedObject) {
    return NS_OK;
  }

  OwningNonNull<Element> resizedObject = *mResizedObject;
  nsresult rv = GetPositionAndDimensions(
      resizedObject, mResizedObjectX, mResizedObjectY, mResizedObjectWidth,
      mResizedObjectHeight, mResizedObjectBorderLeft, mResizedObjectBorderTop,
      mResizedObjectMarginLeft, mResizedObjectMarginTop);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetPositionAndDimensions() failed");
    return rv;
  }
  if (NS_WARN_IF(resizedObject != mResizedObject)) {
    return NS_ERROR_FAILURE;
  }

  rv = SetAllResizersPosition();
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::SetAllResizersPosition() failed");
    return rv;
  }
  if (NS_WARN_IF(resizedObject != mResizedObject)) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(
      mResizingShadow,
      "SetAllResizersPosition() should return error if resizers are hidden");
  RefPtr<Element> resizingShadow = mResizingShadow.get();
  rv = SetShadowPosition(*resizingShadow, resizedObject, mResizedObjectX,
                         mResizedObjectY);
  if (NS_WARN_IF(resizedObject != mResizedObject)) {
    return NS_ERROR_FAILURE;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::SetShadowPosition() failed");
  return rv;
}

nsresult HTMLEditor::ShowResizersInternal(Element& aResizedElement) {
  // When we have visible resizers, we cannot show new resizers.
  // So, the caller should call HideResizersInternal() first if this
  // returns error.
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
    // Note that creating a resizer or shadow may causes calling
    // HideRisizersInternal() via a mutation event listener.  So, we should
    // store new resizer to a local variable, then, check:
    //   - whether creating resizer is already set to the member or not
    //   - whether resizing element is changed to another element
    // If showing resizers are canceled, we hit the latter check.
    // If resizers for another element is shown during this, we hit the latter
    // check too.
    // If resizers are just shown again for same element, we hit the former
    // check.
    ManualNACPtr newResizer =
        CreateResizer(nsIHTMLObjectResizer::eTopLeft, *parentContent);
    if (!newResizer) {
      NS_WARNING("HTMLEditor::CreateResizer(eTopLeft) failed");
      break;
    }
    if (NS_WARN_IF(mTopLeftHandle) ||
        NS_WARN_IF(mResizedObject != &aResizedElement)) {
      // Don't hide current resizers in this case because they are not what
      // we're creating.
      return NS_ERROR_FAILURE;
    }
    mTopLeftHandle = std::move(newResizer);
    newResizer = CreateResizer(nsIHTMLObjectResizer::eTop, *parentContent);
    if (!newResizer) {
      NS_WARNING("HTMLEditor::CreateResizer(eTop) failed");
      break;
    }
    if (NS_WARN_IF(mTopHandle) ||
        NS_WARN_IF(mResizedObject != &aResizedElement)) {
      return NS_ERROR_FAILURE;
    }
    mTopHandle = std::move(newResizer);
    newResizer = CreateResizer(nsIHTMLObjectResizer::eTopRight, *parentContent);
    if (!newResizer) {
      NS_WARNING("HTMLEditor::CreateResizer(eTopRight) failed");
      break;
    }
    if (NS_WARN_IF(mTopRightHandle) ||
        NS_WARN_IF(mResizedObject != &aResizedElement)) {
      return NS_ERROR_FAILURE;
    }
    mTopRightHandle = std::move(newResizer);

    newResizer = CreateResizer(nsIHTMLObjectResizer::eLeft, *parentContent);
    if (!newResizer) {
      NS_WARNING("HTMLEditor::CreateResizer(eLeft) failed");
      break;
    }
    if (NS_WARN_IF(mLeftHandle) ||
        NS_WARN_IF(mResizedObject != &aResizedElement)) {
      return NS_ERROR_FAILURE;
    }
    mLeftHandle = std::move(newResizer);
    newResizer = CreateResizer(nsIHTMLObjectResizer::eRight, *parentContent);
    if (!newResizer) {
      NS_WARNING("HTMLEditor::CreateResizer(eRight) failed");
      break;
    }
    if (NS_WARN_IF(mRightHandle) ||
        NS_WARN_IF(mResizedObject != &aResizedElement)) {
      return NS_ERROR_FAILURE;
    }
    mRightHandle = std::move(newResizer);

    newResizer =
        CreateResizer(nsIHTMLObjectResizer::eBottomLeft, *parentContent);
    if (!newResizer) {
      NS_WARNING("HTMLEditor::CreateResizer(eBottomLeft) failed");
      break;
    }
    if (NS_WARN_IF(mBottomLeftHandle) ||
        NS_WARN_IF(mResizedObject != &aResizedElement)) {
      return NS_ERROR_FAILURE;
    }
    mBottomLeftHandle = std::move(newResizer);
    newResizer = CreateResizer(nsIHTMLObjectResizer::eBottom, *parentContent);
    if (!newResizer) {
      NS_WARNING("HTMLEditor::CreateResizer(eBottom) failed");
      break;
    }
    if (NS_WARN_IF(mBottomHandle) ||
        NS_WARN_IF(mResizedObject != &aResizedElement)) {
      return NS_ERROR_FAILURE;
    }
    mBottomHandle = std::move(newResizer);
    newResizer =
        CreateResizer(nsIHTMLObjectResizer::eBottomRight, *parentContent);
    if (!newResizer) {
      NS_WARNING("HTMLEditor::CreateResizer(eBottomRight) failed");
      break;
    }
    if (NS_WARN_IF(mBottomRightHandle) ||
        NS_WARN_IF(mResizedObject != &aResizedElement)) {
      return NS_ERROR_FAILURE;
    }
    mBottomRightHandle = std::move(newResizer);

    // Store the last resizer which we created.  This is useful when we
    // need to check whether our resizers are hiddedn and recreated another
    // set of resizers or not.
    RefPtr<Element> createdBottomRightHandle = mBottomRightHandle.get();

    nsresult rv = GetPositionAndDimensions(
        aResizedElement, mResizedObjectX, mResizedObjectY, mResizedObjectWidth,
        mResizedObjectHeight, mResizedObjectBorderLeft, mResizedObjectBorderTop,
        mResizedObjectMarginLeft, mResizedObjectMarginTop);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::GetPositionAndDimensions() failed");
      break;
    }

    // and let's set their absolute positions in the document
    rv = SetAllResizersPosition();
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::SetAllResizersPosition() failed");
      if (NS_WARN_IF(mBottomRightHandle.get() != createdBottomRightHandle)) {
        return NS_ERROR_FAILURE;
      }
      break;
    }

    // now, let's create the resizing shadow
    ManualNACPtr newShadow = CreateShadow(*parentContent, aResizedElement);
    if (!newShadow) {
      NS_WARNING("HTMLEditor::CreateShadow() failed");
      break;
    }
    if (NS_WARN_IF(mResizingShadow) ||
        NS_WARN_IF(mResizedObject != &aResizedElement)) {
      return NS_ERROR_FAILURE;
    }
    mResizingShadow = std::move(newShadow);

    // and set its position
    RefPtr<Element> resizingShadow = mResizingShadow.get();
    rv = SetShadowPosition(*resizingShadow, aResizedElement, mResizedObjectX,
                           mResizedObjectY);
    if (NS_FAILED(rv)) {
      NS_WARNING("HTMLEditor::SetShadowPosition() failed");
      if (NS_WARN_IF(mBottomRightHandle.get() != createdBottomRightHandle)) {
        return NS_ERROR_FAILURE;
      }
      break;
    }

    // and then the resizing info tooltip
    ManualNACPtr newResizingInfo = CreateResizingInfo(*parentContent);
    if (!newResizingInfo) {
      NS_WARNING("HTMLEditor::CreateResizingInfo() failed");
      break;
    }
    if (NS_WARN_IF(mResizingInfo) ||
        NS_WARN_IF(mResizedObject != &aResizedElement)) {
      return NS_ERROR_FAILURE;
    }
    mResizingInfo = std::move(newResizingInfo);

    // and listen to the "resize" event on the window first, get the
    // window from the document...
    if (NS_WARN_IF(!mEventListener)) {
      break;
    }

    rv = static_cast<HTMLEditorEventListener*>(mEventListener.get())
             ->ListenToWindowResizeEvent(true);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditorEventListener::ListenToWindowResizeEvent(true) failed");
      break;
    }

    MOZ_ASSERT(mResizedObject == &aResizedElement);

    // XXX Even when it failed to add event listener, should we need to set
    //     _moz_resizing attribute?
    DebugOnly<nsresult> rvIgnored =
        aResizedElement.SetAttr(kNameSpaceID_None, nsGkAtoms::_moz_resizing,
                                NS_LITERAL_STRING("true"), true);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "Element::SetAttr(nsGkAtoms::_moz_resizing, true) failed, but ignored");
    return NS_OK;
  } while (true);

  DebugOnly<nsresult> rv = HideResizersInternal();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::HideResizersInternal() failed to clean up");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP HTMLEditor::HideResizers() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = HideResizersInternal();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::HideResizersInternal() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::HideResizersInternal() {
  // Don't warn even if resizers are visible since script cannot check
  // if they are visible and this is non-virtual method.  So, the cost of
  // calling this can be ignored.
  if (!mResizedObject) {
    return NS_OK;
  }

  // get the presshell's document observer interface.
  RefPtr<PresShell> presShell = GetPresShell();
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
    DebugOnly<nsresult> rvIgnored = activatedHandle->UnsetAttr(
        kNameSpaceID_None, nsGkAtoms::_moz_activated, true);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "Element::UnsetAttr(nsGkAtoms::_moz_activated) failed, but ignored");
  }

  // Remove resizing state of the target element.
  DebugOnly<nsresult> rvIgnored = resizedObject->UnsetAttr(
      kNameSpaceID_None, nsGkAtoms::_moz_resizing, true);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "Element::UnsetAttr(nsGkAtoms::_moz_resizing) failed, but ignored");

  if (!mEventListener) {
    return NS_OK;
  }

  nsresult rv = static_cast<HTMLEditorEventListener*>(mEventListener.get())
                    ->ListenToMouseMoveEventForResizers(false);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditorEventListener::ListenToMouseMoveEventForResizers(false) "
        "failed");
    return rv;
  }

  // Remove resize event listener from the window.
  if (!mEventListener) {
    return NS_OK;
  }

  rv = static_cast<HTMLEditorEventListener*>(mEventListener.get())
           ->ListenToWindowResizeEvent(false);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "HTMLEditorEventListener::ListenToWindowResizeEvent(false) failed");
  return rv;
}

void HTMLEditor::HideShadowAndInfo() {
  if (mResizingShadow) {
    DebugOnly<nsresult> rvIgnored =
        mResizingShadow->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                                 NS_LITERAL_STRING("hidden"), true);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "Element::SetAttr(nsGkAtoms::_class, hidden) failed, but ignored");
  }
  if (mResizingInfo) {
    DebugOnly<nsresult> rvIgnored =
        mResizingInfo->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                               NS_LITERAL_STRING("hidden"), true);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "Element::SetAttr(nsGkAtoms::_class, hidden) failed, but ignored");
  }
}

nsresult HTMLEditor::StartResizing(Element* aHandle) {
  mIsResizing = true;
  if (NS_WARN_IF(!aHandle)) {
    return NS_ERROR_FAILURE;
  }
  mActivatedHandle = aHandle;
  DebugOnly<nsresult> rvIgnored =
      mActivatedHandle->SetAttr(kNameSpaceID_None, nsGkAtoms::_moz_activated,
                                NS_LITERAL_STRING("true"), true);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "Element::SetAttr(nsGkAtoms::_moz_activated, true) failed");

  // do we want to preserve ratio or not?
  bool preserveRatio =
      HTMLEditUtils::IsImage(mResizedObject) &&
      Preferences::GetBool("editor.resizing.preserve_ratio", true);

  // the way we change the position/size of the shadow depends on
  // the handle
  nsAutoString locationStr;
  mActivatedHandle->GetAttr(nsGkAtoms::anonlocation, locationStr);
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
  rvIgnored =
      mResizingShadow->UnsetAttr(kNameSpaceID_None, nsGkAtoms::_class, true);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "Element::UnsetAttr(nsGkAtoms::_class) failed");

  // position it
  RefPtr<Element> resizingShadow = mResizingShadow.get();
  rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(
      *resizingShadow, *nsGkAtoms::width, mResizedObjectWidth);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::width) failed");
  rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(
      *resizingShadow, *nsGkAtoms::height, mResizedObjectHeight);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::height) failed");

  // add a mouse move listener to the editor
  if (NS_WARN_IF(!mEventListener)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsresult rv = static_cast<HTMLEditorEventListener*>(mEventListener.get())
                    ->ListenToMouseMoveEventForResizers(true);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditorEventListener::"
                       "ListenToMouseMoveEventForResizers(true) failed");
  return rv;
}

nsresult HTMLEditor::OnMouseDown(int32_t aClientX, int32_t aClientY,
                                 Element* aTarget, Event* aEvent) {
  if (NS_WARN_IF(!aTarget)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoString anonclass;
  aTarget->GetAttr(nsGkAtoms::_moz_anonclass, anonclass);

  if (anonclass.EqualsLiteral("mozResizer")) {
    AutoEditActionDataSetter editActionData(*this,
                                            EditAction::eResizingElement);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return NS_ERROR_NOT_INITIALIZED;
    }

    // If we have an anonymous element and that element is a resizer,
    // let's start resizing!
    aEvent->PreventDefault();
    mOriginalX = aClientX;
    mOriginalY = aClientY;
    nsresult rv = StartResizing(aTarget);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::StartResizing() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  if (anonclass.EqualsLiteral("mozGrabber")) {
    AutoEditActionDataSetter editActionData(*this, EditAction::eMovingElement);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return NS_ERROR_NOT_INITIALIZED;
    }

    // If we have an anonymous element and that element is a grabber,
    // let's start moving the element!
    mOriginalX = aClientX;
    mOriginalY = aClientY;
    nsresult rv = GrabberClicked();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::GrabberClicked() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  return NS_OK;
}

nsresult HTMLEditor::OnMouseUp(int32_t aClientX, int32_t aClientY) {
  if (mIsResizing) {
    AutoEditActionDataSetter editActionData(*this, EditAction::eResizeElement);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return NS_ERROR_NOT_INITIALIZED;
    }

    // we are resizing and release the mouse button, so let's
    // end the resizing process
    mIsResizing = false;
    HideShadowAndInfo();

    nsresult rv = editActionData.MaybeDispatchBeforeInputEvent();
    if (NS_FAILED(rv)) {
      NS_WARNING_ASSERTION(
          rv == NS_ERROR_EDITOR_ACTION_CANCELED,
          "EditorBase::MaybeDispatchBeforeInputEvent(), failed");
      return EditorBase::ToGenericNSResult(rv);
    }

    SetFinalSize(aClientX, aClientY);
    return NS_OK;
  }

  if (mIsMoving || mGrabberClicked) {
    AutoEditActionDataSetter editActionData(*this, EditAction::eMoveElement);
    nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
    if (rv != NS_ERROR_EDITOR_ACTION_CANCELED && NS_WARN_IF(NS_FAILED(rv))) {
      NS_WARNING("CanHandleAndMaybeDispatchBeforeInputEvent() failed");
      return EditorBase::ToGenericNSResult(rv);
    }

    if (mIsMoving) {
      DebugOnly<nsresult> rvIgnored =
          mPositioningShadow->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                                      NS_LITERAL_STRING("hidden"), true);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "Element::SetAttr(nsGkAtoms::_class, hidden) failed");
      if (rv != NS_ERROR_EDITOR_ACTION_CANCELED) {
        SetFinalPosition(aClientX, aClientY);
      }
    }
    if (mGrabberClicked) {
      DebugOnly<nsresult> rvIgnored = EndMoving();
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "HTMLEditor::EndMoving() failed");
    }
    return EditorBase::ToGenericNSResult(rv);
  }

  return NS_OK;
}

void HTMLEditor::SetResizeIncrements(int32_t aX, int32_t aY, int32_t aW,
                                     int32_t aH, bool aPreserveRatio) {
  mXIncrementFactor = aX;
  mYIncrementFactor = aY;
  mWidthIncrementFactor = aW;
  mHeightIncrementFactor = aH;
  mPreserveRatio = aPreserveRatio;
}

nsresult HTMLEditor::SetResizingInfoPosition(int32_t aX, int32_t aY, int32_t aW,
                                             int32_t aH) {
  // Determine the position of the resizing info box based upon the new
  // position and size of the element (aX, aY, aW, aH), and which
  // resizer is the "activated handle".  For example, place the resizing
  // info box at the bottom-right corner of the new element, if the element
  // is being resized by the bottom-right resizer.
  int32_t infoXPosition;
  int32_t infoYPosition;

  if (mActivatedHandle == mTopLeftHandle || mActivatedHandle == mLeftHandle ||
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

  if (mActivatedHandle == mTopLeftHandle || mActivatedHandle == mTopHandle ||
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
  RefPtr<Element> resizingInfo = mResizingInfo.get();
  DebugOnly<nsresult> rvIgnored = NS_OK;
  rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(
      *resizingInfo, *nsGkAtoms::left, infoXPosition + mouseCursorOffset);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::left) "
                       "failed, but ignored");
  rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(
      *resizingInfo, *nsGkAtoms::top, infoYPosition + mouseCursorOffset);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::top) "
                       "failed, but ignored");

  nsCOMPtr<nsIContent> textInfo = mResizingInfo->GetFirstChild();
  ErrorResult error;
  if (textInfo) {
    mResizingInfo->RemoveChild(*textInfo, error);
    if (error.Failed()) {
      NS_WARNING("nsINode::RemoveChild() failed");
      return error.StealNSResult();
    }
    textInfo = nullptr;
  }

  nsAutoString widthStr, heightStr, diffWidthStr, diffHeightStr;
  widthStr.AppendInt(aW);
  heightStr.AppendInt(aH);
  int32_t diffWidth = aW - mResizedObjectWidth;
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

  RefPtr<Document> document = GetDocument();
  textInfo = document->CreateTextNode(info);
  if (!textInfo) {
    NS_WARNING("Document::CreateTextNode() failed");
    return NS_ERROR_FAILURE;
  }
  mResizingInfo->AppendChild(*textInfo, error);
  if (error.Failed()) {
    NS_WARNING("nsINode::AppendChild() failed");
    return error.StealNSResult();
  }

  nsresult rv =
      mResizingInfo->UnsetAttr(kNameSpaceID_None, nsGkAtoms::_class, true);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Element::UnsetAttr(nsGkAtoms::_class) failed");
  return rv;
}

nsresult HTMLEditor::SetShadowPosition(Element& aShadowElement,
                                       Element& aElement, int32_t aElementX,
                                       int32_t aElementY) {
  MOZ_ASSERT(&aShadowElement == mResizingShadow ||
             &aShadowElement == mPositioningShadow);
  RefPtr<Element> handlingShadowElement = &aShadowElement == mResizingShadow
                                              ? mResizingShadow.get()
                                              : mPositioningShadow.get();

  SetAnonymousElementPosition(aElementX, aElementY, &aShadowElement);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(&aShadowElement != handlingShadowElement)) {
    return NS_ERROR_FAILURE;
  }

  if (!HTMLEditUtils::IsImage(&aElement)) {
    return NS_OK;
  }

  nsAutoString imageSource;
  aElement.GetAttr(kNameSpaceID_None, nsGkAtoms::src, imageSource);
  nsresult rv = aShadowElement.SetAttr(kNameSpaceID_None, nsGkAtoms::src,
                                       imageSource, true);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("Element::SetAttr(nsGkAtoms::src) failed");
    return NS_ERROR_FAILURE;
  }
  return NS_WARN_IF(&aShadowElement != handlingShadowElement) ? NS_ERROR_FAILURE
                                                              : NS_OK;
}

int32_t HTMLEditor::GetNewResizingIncrement(int32_t aX, int32_t aY,
                                            ResizeAt aResizeAt) {
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
      if (result == yi) result = (int32_t)(((float)result) * objectSizeRatio);
      result = (int32_t)(((float)result) * mWidthIncrementFactor);
      break;
    case ResizeAt::eY:
    case ResizeAt::eHeight:
      if (result == xi) result = (int32_t)(((float)result) / objectSizeRatio);
      result = (int32_t)(((float)result) * mHeightIncrementFactor);
      break;
  }
  return result;
}

int32_t HTMLEditor::GetNewResizingX(int32_t aX, int32_t aY) {
  int32_t resized =
      mResizedObjectX +
      GetNewResizingIncrement(aX, aY, ResizeAt::eX) * mXIncrementFactor;
  int32_t max = mResizedObjectX + mResizedObjectWidth;
  return std::min(resized, max);
}

int32_t HTMLEditor::GetNewResizingY(int32_t aX, int32_t aY) {
  int32_t resized =
      mResizedObjectY +
      GetNewResizingIncrement(aX, aY, ResizeAt::eY) * mYIncrementFactor;
  int32_t max = mResizedObjectY + mResizedObjectHeight;
  return std::min(resized, max);
}

int32_t HTMLEditor::GetNewResizingWidth(int32_t aX, int32_t aY) {
  int32_t resized =
      mResizedObjectWidth +
      GetNewResizingIncrement(aX, aY, ResizeAt::eWidth) * mWidthIncrementFactor;
  return std::max(resized, 1);
}

int32_t HTMLEditor::GetNewResizingHeight(int32_t aX, int32_t aY) {
  int32_t resized = mResizedObjectHeight +
                    GetNewResizingIncrement(aX, aY, ResizeAt::eHeight) *
                        mHeightIncrementFactor;
  return std::max(resized, 1);
}

nsresult HTMLEditor::OnMouseMove(MouseEvent* aMouseEvent) {
  MOZ_ASSERT(aMouseEvent);

  if (mIsResizing) {
    AutoEditActionDataSetter editActionData(*this,
                                            EditAction::eResizingElement);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return NS_ERROR_NOT_INITIALIZED;
    }

    // we are resizing and the mouse pointer's position has changed
    // we have to resdisplay the shadow
    int32_t clientX = aMouseEvent->ClientX();
    int32_t clientY = aMouseEvent->ClientY();

    int32_t newX = GetNewResizingX(clientX, clientY);
    int32_t newY = GetNewResizingY(clientX, clientY);
    int32_t newWidth = GetNewResizingWidth(clientX, clientY);
    int32_t newHeight = GetNewResizingHeight(clientX, clientY);

    RefPtr<Element> resizingShadow = mResizingShadow.get();
    DebugOnly<nsresult> rvIgnored = NS_OK;
    rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(*resizingShadow,
                                                    *nsGkAtoms::left, newX);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::left) "
                         "failed, but ignored");
    rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(*resizingShadow,
                                                    *nsGkAtoms::top, newY);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::top) "
                         "failed, but ignored");
    rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(
        *resizingShadow, *nsGkAtoms::width, newWidth);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::width) "
                         "failed, but ignored");
    rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(
        *resizingShadow, *nsGkAtoms::height, newHeight);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::height)"
                         " failed, but ignored");

    nsresult rv = SetResizingInfoPosition(newX, newY, newWidth, newHeight);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::SetResizingInfoPosition() failed");
    return rv;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eMovingElement);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
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
      DebugOnly<nsresult> rvIgnored = StartMoving();
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "HTMLEditor::StartMoving() failed, but ignored");
    }
  }
  if (mIsMoving) {
    int32_t clientX = aMouseEvent->ClientX();
    int32_t clientY = aMouseEvent->ClientY();

    int32_t newX = mPositionedObjectX + clientX - mOriginalX;
    int32_t newY = mPositionedObjectY + clientY - mOriginalY;

    SnapToGrid(newX, newY);

    RefPtr<Element> positioningShadow = mPositioningShadow.get();
    DebugOnly<nsresult> rvIgnored = NS_OK;
    rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(*positioningShadow,
                                                    *nsGkAtoms::left, newX);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::left) "
                         "failed, but ignored");
    rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(*positioningShadow,
                                                    *nsGkAtoms::top, newY);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::top) "
                         "failed, but ignored");
  }
  return NS_OK;
}

void HTMLEditor::SetFinalSize(int32_t aX, int32_t aY) {
  if (!mResizedObject) {
    // paranoia
    return;
  }

  if (mActivatedHandle) {
    DebugOnly<nsresult> rvIgnored = mActivatedHandle->UnsetAttr(
        kNameSpaceID_None, nsGkAtoms::_moz_activated, true);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rvIgnored),
        "Element::UnsetAttr(nsGkAtoms::_moz_activated) failed, but ignored");
    mActivatedHandle = nullptr;
  }

  // we have now to set the new width and height of the resized object
  // we don't set the x and y position because we don't control that in
  // a normal HTML layout
  int32_t left = GetNewResizingX(aX, aY);
  int32_t top = GetNewResizingY(aX, aY);
  int32_t width = GetNewResizingWidth(aX, aY);
  int32_t height = GetNewResizingHeight(aX, aY);
  bool setWidth =
      !mResizedObjectIsAbsolutelyPositioned || (width != mResizedObjectWidth);
  bool setHeight =
      !mResizedObjectIsAbsolutelyPositioned || (height != mResizedObjectHeight);

  int32_t x, y;
  x = left - ((mResizedObjectIsAbsolutelyPositioned)
                  ? mResizedObjectBorderLeft + mResizedObjectMarginLeft
                  : 0);
  y = top - ((mResizedObjectIsAbsolutelyPositioned)
                 ? mResizedObjectBorderTop + mResizedObjectMarginTop
                 : 0);

  // we want one transaction only from a user's point of view
  AutoPlaceholderBatch treatAsOneTransaction(*this);
  RefPtr<Element> resizedObject(mResizedObject);

  if (mResizedObjectIsAbsolutelyPositioned) {
    if (setHeight) {
      DebugOnly<nsresult> rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(
          *resizedObject, *nsGkAtoms::top, y);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::top) "
                           "failed, but ignored");
    }
    if (setWidth) {
      DebugOnly<nsresult> rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(
          *resizedObject, *nsGkAtoms::left, x);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::left) "
          "failed, but ignored");
    }
  }
  if (IsCSSEnabled() || mResizedObjectIsAbsolutelyPositioned) {
    if (setWidth &&
        resizedObject->HasAttr(kNameSpaceID_None, nsGkAtoms::width)) {
      DebugOnly<nsresult> rvIgnored =
          RemoveAttributeWithTransaction(*resizedObject, *nsGkAtoms::width);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "EditorBase::RemoveAttributeWithTransaction(nsGkAtoms::width) "
          "failed, but ignored");
    }

    if (setHeight &&
        resizedObject->HasAttr(kNameSpaceID_None, nsGkAtoms::height)) {
      DebugOnly<nsresult> rvIgnored =
          RemoveAttributeWithTransaction(*resizedObject, *nsGkAtoms::height);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "EditorBase::RemoveAttributeWithTransaction(nsGkAtoms::height) "
          "failed, but ignored");
    }

    if (setWidth) {
      DebugOnly<nsresult> rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(
          *resizedObject, *nsGkAtoms::width, width);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::width) "
          "failed, but ignored");
    }
    if (setHeight) {
      DebugOnly<nsresult> rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(
          *resizedObject, *nsGkAtoms::height, height);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::height) "
          "failed, but ignored");
    }
  } else {
    // we use HTML size and remove all equivalent CSS properties

    // we set the CSS width and height to remove it later,
    // triggering an immediate reflow; otherwise, we have problems
    // with asynchronous reflow
    if (setWidth) {
      DebugOnly<nsresult> rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(
          *resizedObject, *nsGkAtoms::width, width);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::width) "
          "failed, but ignored");
    }
    if (setHeight) {
      DebugOnly<nsresult> rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(
          *resizedObject, *nsGkAtoms::height, height);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::height) "
          "failed, but ignored");
    }
    if (setWidth) {
      nsAutoString w;
      w.AppendInt(width);
      DebugOnly<nsresult> rvIgnored =
          SetAttributeWithTransaction(*resizedObject, *nsGkAtoms::width, w);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "EditorBase::SetAttributeWithTransaction(nsGkAtoms::width) "
          "failed, but ignored");
    }
    if (setHeight) {
      nsAutoString h;
      h.AppendInt(height);
      DebugOnly<nsresult> rvIgnored =
          SetAttributeWithTransaction(*resizedObject, *nsGkAtoms::height, h);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "EditorBase::SetAttributeWithTransaction(nsGkAtoms::height) "
          "failed, but ignored");
    }

    if (setWidth) {
      DebugOnly<nsresult> rvIgnored = mCSSEditUtils->RemoveCSSProperty(
          *resizedObject, *nsGkAtoms::width, EmptyString());
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "CSSEditUtils::RemoveCSSProperty(nsGkAtoms::width) "
                           "failed, but ignored");
    }
    if (setHeight) {
      DebugOnly<nsresult> rvIgnored = mCSSEditUtils->RemoveCSSProperty(
          *resizedObject, *nsGkAtoms::height, EmptyString());
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "CSSEditUtils::RemoveCSSProperty(nsGkAtoms::height) "
                           "failed, but ignored");
    }
  }

  // keep track of that size
  mResizedObjectWidth = width;
  mResizedObjectHeight = height;

  DebugOnly<nsresult> rvIgnored = RefreshResizersInternal();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "HTMLEditor::RefreshResizersInternal() failed, but ignored");
}

NS_IMETHODIMP HTMLEditor::GetObjectResizingEnabled(
    bool* aIsObjectResizingEnabled) {
  *aIsObjectResizingEnabled = IsObjectResizerEnabled();
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::SetObjectResizingEnabled(
    bool aObjectResizingEnabled) {
  EnableObjectResizer(aObjectResizingEnabled);
  return NS_OK;
}

#undef kTopLeft
#undef kTop
#undef kTopRight
#undef kLeft
#undef kRight
#undef kBottomLeft
#undef kBottom
#undef kBottomRight

}  // namespace mozilla
