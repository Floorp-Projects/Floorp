/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditor.h"

#include <math.h>

#include "HTMLEditorEventListener.h"
#include "HTMLEditUtils.h"
#include "mozilla/EditAction.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/AncestorIterator.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/mozalloc.h"
#include "nsAString.h"
#include "nsAlgorithm.h"
#include "nsCOMPtr.h"
#include "nsComputedDOMStyle.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsROCSSPrimitiveValue.h"
#include "nsINode.h"
#include "nsIPrincipal.h"
#include "nsISupportsImpl.h"
#include "nsISupportsUtils.h"
#include "nsLiteralString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nscore.h"
#include <algorithm>

namespace mozilla {

using namespace dom;

nsresult HTMLEditor::SetSelectionToAbsoluteOrStaticAsAction(
    bool aEnabled, nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  AutoEditActionDataSetter editActionData(
      *this, EditAction::eSetPositionToAbsoluteOrStatic, aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return rv;
  }

  if (aEnabled) {
    EditActionResult result = SetSelectionToAbsoluteAsSubAction();
    NS_WARNING_ASSERTION(
        result.Succeeded(),
        "HTMLEditor::SetSelectionToAbsoluteAsSubAction() failed");
    return result.Rv();
  }
  EditActionResult result = SetSelectionToStaticAsSubAction();
  NS_WARNING_ASSERTION(result.Succeeded(),
                       "HTMLEditor::SetSelectionToStaticAsSubAction() failed");
  return result.Rv();
}

already_AddRefed<Element>
HTMLEditor::GetAbsolutelyPositionedSelectionContainer() const {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return nullptr;
  }

  Element* selectionContainerElement = GetSelectionContainerElement();
  if (NS_WARN_IF(!selectionContainerElement)) {
    return nullptr;
  }

  AutoTArray<RefPtr<Element>, 24> arrayOfParentElements;
  for (Element* element :
       InclusiveAncestorsOfType<Element>(*selectionContainerElement)) {
    arrayOfParentElements.AppendElement(element);
  }

  nsAutoString positionValue;
  for (RefPtr<Element> element = selectionContainerElement; element;
       element = element->GetParentElement()) {
    if (element->IsHTMLElement(nsGkAtoms::html)) {
      NS_WARNING(
          "HTMLEditor::GetAbsolutelyPositionedSelectionContainer() reached "
          "<html> element");
      return nullptr;
    }
    nsCOMPtr<nsINode> parentNode = element->GetParentNode();
    nsresult rv = CSSEditUtils::GetComputedProperty(
        MOZ_KnownLive(*element), *nsGkAtoms::position, positionValue);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "CSSEditUtils::GetComputedProperty(nsGkAtoms::position) failed");
      return nullptr;
    }
    if (NS_WARN_IF(Destroyed()) ||
        NS_WARN_IF(parentNode != element->GetParentNode())) {
      return nullptr;
    }
    if (positionValue.EqualsLiteral("absolute")) {
      return element.forget();
    }
  }
  return nullptr;
}

NS_IMETHODIMP HTMLEditor::GetAbsolutePositioningEnabled(bool* aIsEnabled) {
  *aIsEnabled = IsAbsolutePositionEditorEnabled();
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::SetAbsolutePositioningEnabled(bool aIsEnabled) {
  EnableAbsolutePositionEditor(aIsEnabled);
  return NS_OK;
}

nsresult HTMLEditor::RelativeChangeElementZIndex(Element& aElement,
                                                 int32_t aChange,
                                                 int32_t* aReturn) {
  if (NS_WARN_IF(!aReturn)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!aChange) {
    return NS_OK;
  }

  int32_t zIndex = GetZIndex(aElement);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  zIndex = std::max(zIndex + aChange, 0);
  SetZIndex(aElement, zIndex);
  *aReturn = zIndex;

  return NS_WARN_IF(Destroyed()) ? NS_ERROR_EDITOR_DESTROYED : NS_OK;
}

void HTMLEditor::SetZIndex(Element& aElement, int32_t aZIndex) {
  nsAutoString zIndexValue;
  zIndexValue.AppendInt(aZIndex);

  DebugOnly<nsresult> rvIgnored =
      mCSSEditUtils->SetCSSProperty(aElement, *nsGkAtoms::z_index, zIndexValue);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "CSSEditUtils::SetCSSProperty(nsGkAtoms::z_index) failed, but ignored");
}

nsresult HTMLEditor::AddZIndexAsAction(int32_t aChange,
                                       nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  AutoEditActionDataSetter editActionData(
      *this, EditAction::eIncreaseOrDecreaseZIndex, aPrincipal);
  nsresult rv = editActionData.CanHandleAndMaybeDispatchBeforeInputEvent();
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_EDITOR_ACTION_CANCELED,
                         "CanHandleAndMaybeDispatchBeforeInputEvent(), failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  EditActionResult result = AddZIndexAsSubAction(aChange);
  NS_WARNING_ASSERTION(result.Succeeded(),
                       "HTMLEditor::AddZIndexAsSubAction() failed");
  return EditorBase::ToGenericNSResult(result.Rv());
}

int32_t HTMLEditor::GetZIndex(Element& aElement) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return 0;
  }

  nsAutoString zIndexValue;

  nsresult rv = CSSEditUtils::GetSpecifiedProperty(
      aElement, *nsGkAtoms::z_index, zIndexValue);
  if (NS_FAILED(rv)) {
    NS_WARNING("CSSEditUtils::GetSpecifiedProperty(nsGkAtoms::z_index) failed");
    return 0;
  }
  if (zIndexValue.EqualsLiteral("auto")) {
    if (!aElement.GetParentElement()) {
      NS_WARNING("aElement was an orphan node or the root node");
      return 0;
    }
    // we have to look at the positioned ancestors
    // cf. CSS 2 spec section 9.9.1
    nsAutoString positionValue;
    for (RefPtr<Element> element = aElement.GetParentElement(); element;
         element = element->GetParentElement()) {
      if (element->IsHTMLElement(nsGkAtoms::body)) {
        return 0;
      }
      nsCOMPtr<nsINode> parentNode = element->GetParentElement();
      nsresult rv = CSSEditUtils::GetComputedProperty(
          *element, *nsGkAtoms::position, positionValue);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "CSSEditUtils::GetComputedProperty(nsGkAtoms::position) failed");
        return 0;
      }
      if (NS_WARN_IF(Destroyed()) ||
          NS_WARN_IF(parentNode != element->GetParentNode())) {
        return 0;
      }
      if (!positionValue.EqualsLiteral("absolute")) {
        continue;
      }
      // ah, we found one, what's its z-index ? If its z-index is auto,
      // we have to continue climbing the document's tree
      rv = CSSEditUtils::GetComputedProperty(*element, *nsGkAtoms::z_index,
                                             zIndexValue);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "CSSEditUtils::GetComputedProperty(nsGkAtoms::z_index) failed");
        return 0;
      }
      if (NS_WARN_IF(Destroyed()) ||
          NS_WARN_IF(parentNode != element->GetParentNode())) {
        return 0;
      }
      if (!zIndexValue.EqualsLiteral("auto")) {
        break;
      }
    }
  }

  if (zIndexValue.EqualsLiteral("auto")) {
    return 0;
  }

  nsresult rvIgnored;
  int32_t result = zIndexValue.ToInteger(&rvIgnored);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "nsAString::ToInteger() failed, but ignored");
  return result;
}

bool HTMLEditor::CreateGrabberInternal(nsIContent& aParentContent) {
  if (NS_WARN_IF(mGrabber)) {
    return false;
  }

  mGrabber = CreateAnonymousElement(nsGkAtoms::span, aParentContent,
                                    NS_LITERAL_STRING("mozGrabber"), false);

  // mGrabber may be destroyed during creation due to there may be
  // mutation event listener.
  if (!mGrabber) {
    NS_WARNING(
        "HTMLEditor::CreateAnonymousElement(nsGkAtoms::span, mozGrabber) "
        "failed");
    return false;
  }

  EventListenerManager* eventListenerManager =
      mGrabber->GetOrCreateListenerManager();
  eventListenerManager->AddEventListenerByType(
      mEventListener, NS_LITERAL_STRING("mousedown"),
      TrustedEventsAtSystemGroupBubble());
  MOZ_ASSERT(mGrabber);
  return true;
}

NS_IMETHODIMP HTMLEditor::RefreshGrabber() {
  if (NS_WARN_IF(!mAbsolutelyPositionedObject)) {
    return NS_ERROR_FAILURE;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = RefreshGrabberInternal();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::RefreshGrabberInternal() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult HTMLEditor::RefreshGrabberInternal() {
  if (!mAbsolutelyPositionedObject) {
    return NS_OK;
  }
  OwningNonNull<Element> absolutelyPositionedObject =
      *mAbsolutelyPositionedObject;
  nsresult rv = GetPositionAndDimensions(
      absolutelyPositionedObject, mPositionedObjectX, mPositionedObjectY,
      mPositionedObjectWidth, mPositionedObjectHeight,
      mPositionedObjectBorderLeft, mPositionedObjectBorderTop,
      mPositionedObjectMarginLeft, mPositionedObjectMarginTop);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::GetPositionAndDimensions() failed");
    return rv;
  }
  if (NS_WARN_IF(absolutelyPositionedObject != mAbsolutelyPositionedObject)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<Element> grabber = mGrabber.get();
  MOZ_ASSERT(grabber);
  SetAnonymousElementPosition(mPositionedObjectX + 12, mPositionedObjectY - 14,
                              grabber);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(grabber != mGrabber.get())) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

void HTMLEditor::HideGrabberInternal() {
  if (NS_WARN_IF(!mAbsolutelyPositionedObject)) {
    return;
  }

  // Move all members to the local variables first since mutation event
  // listener may try to show grabber while we're hiding them.
  RefPtr<Element> absolutePositioningObject =
      std::move(mAbsolutelyPositionedObject);
  ManualNACPtr grabber = std::move(mGrabber);
  ManualNACPtr positioningShadow = std::move(mPositioningShadow);

  DebugOnly<nsresult> rv = absolutePositioningObject->UnsetAttr(
      kNameSpaceID_None, nsGkAtoms::_moz_abspos, true);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "Element::UnsetAttr(nsGkAtoms::_moz_abspos) failed, but ignored");

  // We allow the pres shell to be null; when it is, we presume there
  // are no document observers to notify, but we still want to
  // UnbindFromTree.
  RefPtr<PresShell> presShell = GetPresShell();
  if (grabber) {
    DeleteRefToAnonymousNode(std::move(grabber), presShell);
  }
  if (positioningShadow) {
    DeleteRefToAnonymousNode(std::move(positioningShadow), presShell);
  }
}

nsresult HTMLEditor::ShowGrabberInternal(Element& aElement) {
  if (NS_WARN_IF(!IsDescendantOfEditorRoot(&aElement))) {
    return NS_ERROR_UNEXPECTED;
  }

  if (NS_WARN_IF(mGrabber)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoString classValue;
  nsresult rv =
      GetTemporaryStyleForFocusedPositionedElement(aElement, classValue);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditor::GetTemporaryStyleForFocusedPositionedElement() failed");
    return rv;
  }

  rv = aElement.SetAttr(kNameSpaceID_None, nsGkAtoms::_moz_abspos, classValue,
                        true);
  if (NS_FAILED(rv)) {
    NS_WARNING("Element::SetAttr(nsGkAtoms::_moz_abspos) failed");
    return rv;
  }

  mAbsolutelyPositionedObject = &aElement;

  Element* parentElement = aElement.GetParentElement();
  if (NS_WARN_IF(!parentElement)) {
    return NS_ERROR_FAILURE;
  }

  if (!CreateGrabberInternal(*parentElement)) {
    NS_WARNING("HTMLEditor::CreateGrabberInternal() failed");
    return NS_ERROR_FAILURE;
  }

  // If we succeeded to create the grabber, HideGrabberInternal() hasn't been
  // called yet.  So, mAbsolutelyPositionedObject should be non-nullptr.
  MOZ_ASSERT(mAbsolutelyPositionedObject);

  // Finally, move the grabber to proper position.
  rv = RefreshGrabberInternal();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::RefereshGrabberInternal() failed");
  return rv;
}

nsresult HTMLEditor::StartMoving() {
  RefPtr<Element> parentElement = mGrabber->GetParentElement();
  if (NS_WARN_IF(!parentElement) || NS_WARN_IF(!mAbsolutelyPositionedObject)) {
    return NS_ERROR_FAILURE;
  }

  // now, let's create the resizing shadow
  mPositioningShadow =
      CreateShadow(*parentElement, *mAbsolutelyPositionedObject);
  if (!mPositioningShadow) {
    NS_WARNING("HTMLEditor::CreateShadow() failed");
    return NS_ERROR_FAILURE;
  }
  if (!mAbsolutelyPositionedObject) {
    NS_WARNING("The target has gone during HTMLEditor::CreateShadow()");
    return NS_ERROR_FAILURE;
  }
  RefPtr<Element> positioningShadow = mPositioningShadow.get();
  RefPtr<Element> absolutelyPositionedObject = mAbsolutelyPositionedObject;
  nsresult rv =
      SetShadowPosition(*positioningShadow, *absolutelyPositionedObject,
                        mPositionedObjectX, mPositionedObjectY);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::SetShadowPosition() failed");
    return rv;
  }

  // make the shadow appear
  DebugOnly<nsresult> rvIgnored =
      mPositioningShadow->UnsetAttr(kNameSpaceID_None, nsGkAtoms::_class, true);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "Element::UnsetAttr(nsGkAtoms::_class) failed, but ignored");

  // position it
  positioningShadow = mPositioningShadow.get();
  rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(
      *positioningShadow, *nsGkAtoms::width, mPositionedObjectWidth);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::width) "
                       "failed, but ignored");
  rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(
      *positioningShadow, *nsGkAtoms::height, mPositionedObjectHeight);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::height) "
                       "failed, but ignored");

  mIsMoving = true;
  return NS_OK;  // XXX Looks like nobody refers this result
}

void HTMLEditor::SnapToGrid(int32_t& newX, int32_t& newY) {
  if (mSnapToGridEnabled && mGridSize) {
    newX = (int32_t)floor(((float)newX / (float)mGridSize) + 0.5f) * mGridSize;
    newY = (int32_t)floor(((float)newY / (float)mGridSize) + 0.5f) * mGridSize;
  }
}

nsresult HTMLEditor::GrabberClicked() {
  if (NS_WARN_IF(!mEventListener)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  nsresult rv = static_cast<HTMLEditorEventListener*>(mEventListener.get())
                    ->ListenToMouseMoveEventForGrabber(true);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "HTMLEditorEventListener::ListenToMouseMoveEventForGrabber(true) "
        "failed, but ignored");
    return NS_OK;
  }
  mGrabberClicked = true;
  return NS_OK;
}

nsresult HTMLEditor::EndMoving() {
  if (mPositioningShadow) {
    RefPtr<PresShell> presShell = GetPresShell();
    if (NS_WARN_IF(!presShell)) {
      return NS_ERROR_NOT_INITIALIZED;
    }

    DeleteRefToAnonymousNode(std::move(mPositioningShadow), presShell);

    mPositioningShadow = nullptr;
  }

  if (mEventListener) {
    DebugOnly<nsresult> rvIgnored =
        static_cast<HTMLEditorEventListener*>(mEventListener.get())
            ->ListenToMouseMoveEventForGrabber(false);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "HTMLEditorEventListener::"
                         "ListenToMouseMoveEventForGrabber(false) failed");
  }

  mGrabberClicked = false;
  mIsMoving = false;
  nsresult rv = RefreshEditingUI();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::RefreshEditingUI() failed");
  return rv;
}

nsresult HTMLEditor::SetFinalPosition(int32_t aX, int32_t aY) {
  nsresult rv = EndMoving();
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::EndMoving() failed");
    return rv;
  }

  // we have now to set the new width and height of the resized object
  // we don't set the x and y position because we don't control that in
  // a normal HTML layout
  int32_t newX = mPositionedObjectX + aX - mOriginalX -
                 (mPositionedObjectBorderLeft + mPositionedObjectMarginLeft);
  int32_t newY = mPositionedObjectY + aY - mOriginalY -
                 (mPositionedObjectBorderTop + mPositionedObjectMarginTop);

  SnapToGrid(newX, newY);

  nsAutoString x, y;
  x.AppendInt(newX);
  y.AppendInt(newY);

  // we want one transaction only from a user's point of view
  AutoPlaceholderBatch treatAsOneTransaction(*this);

  if (NS_WARN_IF(!mAbsolutelyPositionedObject)) {
    return NS_ERROR_FAILURE;
  }
  OwningNonNull<Element> absolutelyPositionedObject =
      *mAbsolutelyPositionedObject;
  DebugOnly<nsresult> rvIgnored = NS_OK;
  rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(*absolutelyPositionedObject,
                                                  *nsGkAtoms::top, newY);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rvIgnored),
      "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::top) failed, but ignored");
  rvIgnored = mCSSEditUtils->SetCSSPropertyPixels(*absolutelyPositionedObject,
                                                  *nsGkAtoms::left, newX);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::left) "
                       "failed, but ignored");
  // keep track of that size
  mPositionedObjectX = newX;
  mPositionedObjectY = newY;

  rv = RefreshResizers();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::RefreshResizers() failed");
  return rv;
}

void HTMLEditor::AddPositioningOffset(int32_t& aX, int32_t& aY) {
  // Get the positioning offset
  int32_t positioningOffset =
      Preferences::GetInt("editor.positioning.offset", 0);

  aX += positioningOffset;
  aY += positioningOffset;
}

nsresult HTMLEditor::SetPositionToAbsoluteOrStatic(Element& aElement,
                                                   bool aEnabled) {
  nsAutoString positionValue;
  DebugOnly<nsresult> rvIgnored = CSSEditUtils::GetComputedProperty(
      aElement, *nsGkAtoms::position, positionValue);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::GetComputedProperty(nsGkAtoms::position) "
                       "failed, but ignored");
  // nothing to do if the element is already in the state we want
  if (positionValue.EqualsLiteral("absolute") == aEnabled) {
    return NS_OK;
  }

  if (aEnabled) {
    nsresult rv = SetPositionToAbsolute(aElement);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::SetPositionToAbsolute() failed");
    return rv;
  }

  nsresult rv = SetPositionToStatic(aElement);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "HTMLEditor::SetPositionToStatic() failed");
  return rv;
}

nsresult HTMLEditor::SetPositionToAbsolute(Element& aElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  AutoPlaceholderBatch treatAsOneTransaction(*this);

  int32_t x, y;
  DebugOnly<nsresult> rvIgnored = GetElementOrigin(aElement, x, y);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "HTMLEditor::GetElementOrigin() failed, but ignored");

  rvIgnored = mCSSEditUtils->SetCSSProperty(aElement, *nsGkAtoms::position,
                                            NS_LITERAL_STRING("absolute"));
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::SetCSSProperty(nsGkAtoms::position, "
                       "absolute) failed, but ignored");

  AddPositioningOffset(x, y);
  SnapToGrid(x, y);
  SetTopAndLeft(aElement, x, y);

  // we may need to create a br if the positioned element is alone in its
  // container
  nsINode* parentNode = aElement.GetParentNode();
  if (parentNode->GetChildCount() != 1) {
    return NS_OK;
  }
  RefPtr<Element> newBRElement =
      InsertBRElementWithTransaction(EditorDOMPoint(parentNode, 0));
  NS_WARNING_ASSERTION(newBRElement,
                       "HTMLEditor::InsertBRElementWithTransaction() failed");
  return newBRElement ? NS_OK : NS_ERROR_FAILURE;
}

nsresult HTMLEditor::SetPositionToStatic(Element& aElement) {
  AutoPlaceholderBatch treatAsOneTransaction(*this);

  DebugOnly<nsresult> rvIgnored = NS_OK;
  rvIgnored = mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::position,
                                               EmptyString());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::RemoveCSSProperty(nsGkAtoms::position) "
                       "failed, but ignored");
  rvIgnored = mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::top,
                                               EmptyString());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::RemoveCSSProperty(nsGkAtoms::top) "
                       "failed, but ignored");
  rvIgnored = mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::left,
                                               EmptyString());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::RemoveCSSProperty(nsGkAtoms::left) "
                       "failed, but ignored");
  rvIgnored = mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::z_index,
                                               EmptyString());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::RemoveCSSProperty(nsGkAtoms::z_index) "
                       "failed, but ignored");

  if (!HTMLEditUtils::IsImage(&aElement)) {
    rvIgnored = mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::width,
                                                 EmptyString());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "CSSEditUtils::RemoveCSSProperty(nsGkAtoms::width) "
                         "failed, but ignored");
    rvIgnored = mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::height,
                                                 EmptyString());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "CSSEditUtils::RemoveCSSProperty(nsGkAtoms::height) "
                         "failed, but ignored");
  }

  if (!aElement.IsHTMLElement(nsGkAtoms::div) ||
      HTMLEditor::HasStyleOrIdOrClassAttribute(aElement)) {
    return NS_OK;
  }

  // Make sure the first fild and last child of aElement starts/ends hard
  // line(s) even after removing `aElement`.
  nsresult rv = EnsureHardLineBeginsWithFirstChildOf(aElement);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::EnsureHardLineBeginsWithFirstChildOf() failed");
    return rv;
  }
  rv = EnsureHardLineEndsWithLastChildOf(aElement);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::EnsureHardLineEndsWithLastChildOf() failed");
    return rv;
  }
  rv = RemoveContainerWithTransaction(aElement);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::RemoveContainerWithTransaction() failed");
  return rv;
}

NS_IMETHODIMP HTMLEditor::SetSnapToGridEnabled(bool aEnabled) {
  mSnapToGridEnabled = aEnabled;
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::GetSnapToGridEnabled(bool* aIsEnabled) {
  *aIsEnabled = mSnapToGridEnabled;
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::SetGridSize(uint32_t aSize) {
  mGridSize = aSize;
  return NS_OK;
}

NS_IMETHODIMP HTMLEditor::GetGridSize(uint32_t* aSize) {
  *aSize = mGridSize;
  return NS_OK;
}

// self-explanatory
void HTMLEditor::SetTopAndLeft(Element& aElement, int32_t aX, int32_t aY) {
  AutoPlaceholderBatch treatAsOneTransaction(*this);
  DebugOnly<nsresult> rvIgnored = NS_OK;
  rvIgnored =
      mCSSEditUtils->SetCSSPropertyPixels(aElement, *nsGkAtoms::left, aX);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::left) "
                       "failed, but ignored");
  rvIgnored =
      mCSSEditUtils->SetCSSPropertyPixels(aElement, *nsGkAtoms::top, aY);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "CSSEditUtils::SetCSSPropertyPixels(nsGkAtoms::top) "
                       "failed, but ignored");
}

nsresult HTMLEditor::GetTemporaryStyleForFocusedPositionedElement(
    Element& aElement, nsAString& aReturn) {
  // we are going to outline the positioned element and bring it to the
  // front to overlap any other element intersecting with it. But
  // first, let's see what's the background and foreground colors of the
  // positioned element.
  // if background-image computed value is 'none,
  //   If the background color is 'auto' and R G B values of the foreground are
  //       each above #d0, use a black background
  //   If the background color is 'auto' and at least one of R G B values of
  //       the foreground is below #d0, use a white background
  // Otherwise don't change background/foreground
  aReturn.Truncate();

  nsAutoString backgroundImageValue;
  nsresult rv = CSSEditUtils::GetComputedProperty(
      aElement, *nsGkAtoms::background_image, backgroundImageValue);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "CSSEditUtils::GetComputedProperty(nsGkAtoms::background_image) "
        "failed");
    return rv;
  }
  if (!backgroundImageValue.EqualsLiteral("none")) {
    return NS_OK;
  }

  nsAutoString backgroundColorValue;
  rv = CSSEditUtils::GetComputedProperty(aElement, *nsGkAtoms::backgroundColor,
                                         backgroundColorValue);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "CSSEditUtils::GetComputedProperty(nsGkAtoms::backgroundColor) "
        "failed");
    return rv;
  }
  if (!backgroundColorValue.EqualsLiteral("rgba(0, 0, 0, 0)")) {
    return NS_OK;
  }

  RefPtr<ComputedStyle> style =
      nsComputedDOMStyle::GetComputedStyle(&aElement, nullptr);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (!style) {
    NS_WARNING("nsComputedDOMStyle::GetComputedStyle() failed");
    return NS_ERROR_FAILURE;
  }

  static const uint8_t kBlackBgTrigger = 0xd0;

  const auto& color = style->StyleText()->mColor;
  if (color.red >= kBlackBgTrigger && color.green >= kBlackBgTrigger &&
      color.blue >= kBlackBgTrigger) {
    aReturn.AssignLiteral("black");
  } else {
    aReturn.AssignLiteral("white");
  }

  return NS_OK;
}

}  // namespace mozilla
