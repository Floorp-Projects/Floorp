/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditor.h"

#include <math.h>

#include "HTMLEditorEventListener.h"
#include "HTMLEditRules.h"
#include "HTMLEditUtils.h"
#include "TextEditUtils.h"
#include "mozilla/EditAction.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/TextEditRules.h"
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
#include "nsIDOMEventListener.h"
#include "nsIDOMWindow.h"
#include "nsIHTMLObjectResizer.h"
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
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (aEnabled) {
    EditActionResult result = SetSelectionToAbsoluteAsSubAction();
    NS_WARNING_ASSERTION(result.Succeeded(),
                         "SetSelectionToAbsoluteAsSubAction() failed");
    return result.Rv();
  }
  EditActionResult result = SetSelectionToStaticAsSubAction();
  NS_WARNING_ASSERTION(result.Succeeded(),
                       "SetSelectionToStaticAsSubAction() failed");
  return result.Rv();
}

already_AddRefed<Element>
HTMLEditor::GetAbsolutelyPositionedSelectionContainer() const {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return nullptr;
  }

  RefPtr<Element> element = GetSelectionContainerElement();
  if (NS_WARN_IF(!element)) {
    return nullptr;
  }

  nsAutoString positionStr;
  while (element && !element->IsHTMLElement(nsGkAtoms::html)) {
    nsresult rv = CSSEditUtils::GetComputedProperty(
        *element, *nsGkAtoms::position, positionStr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
    if (positionStr.EqualsLiteral("absolute")) {
      return element.forget();
    }
    element = element->GetParentElement();
  }
  return nullptr;
}

NS_IMETHODIMP
HTMLEditor::GetAbsolutePositioningEnabled(bool* aIsEnabled) {
  *aIsEnabled = IsAbsolutePositionEditorEnabled();
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::SetAbsolutePositioningEnabled(bool aIsEnabled) {
  EnableAbsolutePositionEditor(aIsEnabled);
  return NS_OK;
}

nsresult HTMLEditor::RelativeChangeElementZIndex(Element& aElement,
                                                 int32_t aChange,
                                                 int32_t* aReturn) {
  NS_ENSURE_ARG_POINTER(aReturn);
  if (!aChange)  // early way out, no change
    return NS_OK;

  int32_t zIndex = GetZIndex(aElement);
  zIndex = std::max(zIndex + aChange, 0);
  SetZIndex(aElement, zIndex);
  *aReturn = zIndex;

  return NS_OK;
}

void HTMLEditor::SetZIndex(Element& aElement, int32_t aZindex) {
  nsAutoString zIndexStr;
  zIndexStr.AppendInt(aZindex);

  mCSSEditUtils->SetCSSProperty(aElement, *nsGkAtoms::z_index, zIndexStr);
}

nsresult HTMLEditor::AddZIndexAsAction(int32_t aChange,
                                       nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  AutoEditActionDataSetter editActionData(
      *this, EditAction::eIncreaseOrDecreaseZIndex, aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  EditActionResult result = AddZIndexAsSubAction(aChange);
  NS_WARNING_ASSERTION(result.Succeeded(), "AddZIndexAsSubAction() failed");
  return EditorBase::ToGenericNSResult(result.Rv());
}

int32_t HTMLEditor::GetZIndex(Element& aElement) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return 0;
  }

  nsAutoString zIndexStr;

  nsresult rv = CSSEditUtils::GetSpecifiedProperty(
      aElement, *nsGkAtoms::z_index, zIndexStr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return 0;
  }
  if (zIndexStr.EqualsLiteral("auto")) {
    // we have to look at the positioned ancestors
    // cf. CSS 2 spec section 9.9.1
    nsCOMPtr<nsINode> node = aElement.GetParentNode();
    nsAutoString positionStr;
    while (node && zIndexStr.EqualsLiteral("auto") &&
           !node->IsHTMLElement(nsGkAtoms::body)) {
      rv = CSSEditUtils::GetComputedProperty(*node, *nsGkAtoms::position,
                                             positionStr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return 0;
      }
      if (positionStr.EqualsLiteral("absolute")) {
        // ah, we found one, what's its z-index ? If its z-index is auto,
        // we have to continue climbing the document's tree
        rv = CSSEditUtils::GetComputedProperty(*node, *nsGkAtoms::z_index,
                                               zIndexStr);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return 0;
        }
      }
      node = node->GetParentNode();
    }
  }

  if (zIndexStr.EqualsLiteral("auto")) {
    return 0;
  }

  nsresult errorCode;
  return zIndexStr.ToInteger(&errorCode);
}

bool HTMLEditor::CreateGrabberInternal(nsIContent& aParentContent) {
  if (NS_WARN_IF(mGrabber)) {
    return false;
  }

  mGrabber = CreateAnonymousElement(nsGkAtoms::span, aParentContent,
                                    NS_LITERAL_STRING("mozGrabber"), false);

  // mGrabber may be destroyed during creation due to there may be
  // mutation event listener.
  if (NS_WARN_IF(!mGrabber)) {
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

NS_IMETHODIMP
HTMLEditor::RefreshGrabber() {
  if (NS_WARN_IF(!mAbsolutelyPositionedObject)) {
    return NS_ERROR_FAILURE;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = RefreshGrabberInternal();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult HTMLEditor::RefreshGrabberInternal() {
  if (!mAbsolutelyPositionedObject) {
    return NS_OK;
  }
  nsresult rv = GetPositionAndDimensions(
      *mAbsolutelyPositionedObject, mPositionedObjectX, mPositionedObjectY,
      mPositionedObjectWidth, mPositionedObjectHeight,
      mPositionedObjectBorderLeft, mPositionedObjectBorderTop,
      mPositionedObjectMarginLeft, mPositionedObjectMarginTop);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<Element> grabber = mGrabber.get();
  SetAnonymousElementPosition(mPositionedObjectX + 12, mPositionedObjectY - 14,
                              grabber);
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
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to unset the attribute");

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
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aElement.SetAttr(kNameSpaceID_None, nsGkAtoms::_moz_abspos, classValue,
                        true);
  NS_ENSURE_SUCCESS(rv, rv);

  mAbsolutelyPositionedObject = &aElement;

  nsIContent* parentContent = aElement.GetParent();
  if (NS_WARN_IF(!parentContent)) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!CreateGrabberInternal(*parentContent))) {
    return NS_ERROR_FAILURE;
  }

  // If we succeeded to create the grabber, HideGrabberInternal() hasn't been
  // called yet.  So, mAbsolutelyPositionedObject should be non-nullptr.
  MOZ_ASSERT(mAbsolutelyPositionedObject);

  // Finally, move the grabber to proper position.
  rv = RefreshGrabberInternal();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditor::StartMoving() {
  nsCOMPtr<nsIContent> parentContent = mGrabber->GetParent();
  if (NS_WARN_IF(!parentContent) || NS_WARN_IF(!mAbsolutelyPositionedObject)) {
    return NS_ERROR_FAILURE;
  }

  // now, let's create the resizing shadow
  mPositioningShadow =
      CreateShadow(*parentContent, *mAbsolutelyPositionedObject);
  if (NS_WARN_IF(!mPositioningShadow) ||
      NS_WARN_IF(!mAbsolutelyPositionedObject)) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<Element> positioningShadow = mPositioningShadow.get();
  RefPtr<Element> absolutelyPositionedObject = mAbsolutelyPositionedObject;
  nsresult rv =
      SetShadowPosition(*positioningShadow, *absolutelyPositionedObject,
                        mPositionedObjectX, mPositionedObjectY);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // make the shadow appear
  mPositioningShadow->UnsetAttr(kNameSpaceID_None, nsGkAtoms::_class, true);

  // position it
  positioningShadow = mPositioningShadow.get();
  mCSSEditUtils->SetCSSPropertyPixels(*positioningShadow, *nsGkAtoms::width,
                                      mPositionedObjectWidth);
  mCSSEditUtils->SetCSSPropertyPixels(*positioningShadow, *nsGkAtoms::height,
                                      mPositionedObjectHeight);

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
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_OK;
  }
  mGrabberClicked = true;
  return rv;
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
                         "Failed to remove mousemove event listener");
  }

  mGrabberClicked = false;
  mIsMoving = false;
  nsresult rv = RefreshEditingUI();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}
nsresult HTMLEditor::SetFinalPosition(int32_t aX, int32_t aY) {
  nsresult rv = EndMoving();
  NS_ENSURE_SUCCESS(rv, rv);

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
  mCSSEditUtils->SetCSSPropertyPixels(*absolutelyPositionedObject,
                                      *nsGkAtoms::top, newY);
  mCSSEditUtils->SetCSSPropertyPixels(*absolutelyPositionedObject,
                                      *nsGkAtoms::left, newX);
  // keep track of that size
  mPositionedObjectX = newX;
  mPositionedObjectY = newY;

  rv = RefreshResizers();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
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
  nsAutoString positionStr;
  CSSEditUtils::GetComputedProperty(aElement, *nsGkAtoms::position,
                                    positionStr);
  bool isPositioned = (positionStr.EqualsLiteral("absolute"));

  // nothing to do if the element is already in the state we want
  if (isPositioned == aEnabled) {
    return NS_OK;
  }

  if (aEnabled) {
    return SetPositionToAbsolute(aElement);
  }

  return SetPositionToStatic(aElement);
}

nsresult HTMLEditor::SetPositionToAbsolute(Element& aElement) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  AutoPlaceholderBatch treatAsOneTransaction(*this);

  int32_t x, y;
  GetElementOrigin(aElement, x, y);

  mCSSEditUtils->SetCSSProperty(aElement, *nsGkAtoms::position,
                                NS_LITERAL_STRING("absolute"));

  AddPositioningOffset(x, y);
  SnapToGrid(x, y);
  SetTopAndLeft(aElement, x, y);

  // we may need to create a br if the positioned element is alone in its
  // container
  nsINode* parentNode = aElement.GetParentNode();
  if (parentNode->GetChildCount() == 1) {
    RefPtr<Element> newBrElement =
        InsertBRElementWithTransaction(EditorDOMPoint(parentNode, 0));
    if (NS_WARN_IF(!newBrElement)) {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

nsresult HTMLEditor::SetPositionToStatic(Element& aElement) {
  AutoPlaceholderBatch treatAsOneTransaction(*this);

  mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::position,
                                   EmptyString());
  mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::top, EmptyString());
  mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::left, EmptyString());
  mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::z_index,
                                   EmptyString());

  if (!HTMLEditUtils::IsImage(&aElement)) {
    mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::width,
                                     EmptyString());
    mCSSEditUtils->RemoveCSSProperty(aElement, *nsGkAtoms::height,
                                     EmptyString());
  }

  if (!aElement.IsHTMLElement(nsGkAtoms::div) ||
      HasStyleOrIdOrClass(&aElement)) {
    return NS_OK;
  }

  // Make sure the first fild and last child of aElement starts/ends hard
  // line(s) even after removing `aElement`.
  nsresult rv = EnsureHardLineBeginsWithFirstChildOf(aElement);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rv = EnsureHardLineEndsWithLastChildOf(aElement);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rv = RemoveContainerWithTransaction(aElement);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "RemoveContainerWithTransaction() failed");
  return rv;
}

NS_IMETHODIMP
HTMLEditor::SetSnapToGridEnabled(bool aEnabled) {
  mSnapToGridEnabled = aEnabled;
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetSnapToGridEnabled(bool* aIsEnabled) {
  *aIsEnabled = mSnapToGridEnabled;
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::SetGridSize(uint32_t aSize) {
  mGridSize = aSize;
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetGridSize(uint32_t* aSize) {
  *aSize = mGridSize;
  return NS_OK;
}

// self-explanatory
void HTMLEditor::SetTopAndLeft(Element& aElement, int32_t aX, int32_t aY) {
  AutoPlaceholderBatch treatAsOneTransaction(*this);
  mCSSEditUtils->SetCSSPropertyPixels(aElement, *nsGkAtoms::left, aX);
  mCSSEditUtils->SetCSSPropertyPixels(aElement, *nsGkAtoms::top, aY);
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

  nsAutoString bgImageStr;
  nsresult rv = CSSEditUtils::GetComputedProperty(
      aElement, *nsGkAtoms::background_image, bgImageStr);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!bgImageStr.EqualsLiteral("none")) {
    return NS_OK;
  }

  nsAutoString bgColorStr;
  rv = CSSEditUtils::GetComputedProperty(aElement, *nsGkAtoms::backgroundColor,
                                         bgColorStr);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!bgColorStr.EqualsLiteral("rgba(0, 0, 0, 0)")) {
    return NS_OK;
  }

  RefPtr<ComputedStyle> style =
      nsComputedDOMStyle::GetComputedStyle(&aElement, nullptr);
  NS_ENSURE_STATE(style);

  const uint8_t kBlackBgTrigger = 0xd0;

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
