/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditor.h"

#include <math.h>

#include "HTMLEditorObjectResizerUtils.h"
#include "HTMLEditRules.h"
#include "HTMLEditUtils.h"
#include "TextEditUtils.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEditRules.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Element.h"
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
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNode.h"
#include "nsDOMCSSRGBColor.h"
#include "nsIDOMWindow.h"
#include "nsIHTMLObjectResizer.h"
#include "nsINode.h"
#include "nsIPresShell.h"
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

#define  BLACK_BG_RGB_TRIGGER 0xd0

NS_IMETHODIMP
HTMLEditor::AbsolutePositionSelection(bool aEnabled)
{
  AutoPlaceholderBatch beginBatching(this);
  AutoRules beginRulesSniffing(this,
                               aEnabled ? EditAction::setAbsolutePosition :
                                          EditAction::removeAbsolutePosition,
                               nsIEditor::eNext);

  // the line below does not match the code; should it be removed?
  // Find out if the selection is collapsed:
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  RulesInfo ruleInfo(aEnabled ? EditAction::setAbsolutePosition :
                                EditAction::removeAbsolutePosition);
  bool cancel, handled;
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(rv) || cancel) {
    return rv;
  }

  return rules->DidDoAction(selection, &ruleInfo, rv);
}

NS_IMETHODIMP
HTMLEditor::GetAbsolutelyPositionedSelectionContainer(nsIDOMElement** _retval)
{
  nsCOMPtr<nsINode> container;
  nsresult rv =
    GetAbsolutelyPositionedSelectionContainer(getter_AddRefs(container));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    *_retval = nullptr;
    return rv;
  }

  nsCOMPtr<nsIDOMElement> domContainer = do_QueryInterface(container);
  domContainer.forget(_retval);
  return NS_OK;
}

nsresult
HTMLEditor::GetAbsolutelyPositionedSelectionContainer(nsINode** aContainer)
{
  MOZ_ASSERT(aContainer);

  nsAutoString positionStr;
  nsCOMPtr<nsINode> node = GetSelectionContainer();
  nsCOMPtr<nsINode> resultNode;

  while (!resultNode && node && !node->IsHTMLElement(nsGkAtoms::html)) {
    nsresult rv =
      mCSSEditUtils->GetComputedProperty(*node, *nsGkAtoms::position,
                                         positionStr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      *aContainer = nullptr;
      return rv;
    }
    if (positionStr.EqualsLiteral("absolute"))
      resultNode = node;
    else {
      node = node->GetParentNode();
    }
  }
  resultNode.forget(aContainer);
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetSelectionContainerAbsolutelyPositioned(
              bool* aIsSelectionContainerAbsolutelyPositioned)
{
  *aIsSelectionContainerAbsolutelyPositioned = (mAbsolutelyPositionedObject != nullptr);
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetAbsolutePositioningEnabled(bool* aIsEnabled)
{
  *aIsEnabled = AbsolutePositioningEnabled();
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::SetAbsolutePositioningEnabled(bool aIsEnabled)
{
  mIsAbsolutelyPositioningEnabled = aIsEnabled;
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::RelativeChangeElementZIndex(nsIDOMElement* aElement,
                                          int32_t aChange,
                                          int32_t* aReturn)
{
  NS_ENSURE_ARG_POINTER(aElement);
  NS_ENSURE_ARG_POINTER(aReturn);
  if (!aChange) // early way out, no change
    return NS_OK;

  int32_t zIndex;
  nsresult rv = GetElementZIndex(aElement, &zIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  zIndex = std::max(zIndex + aChange, 0);
  SetElementZIndex(aElement, zIndex);
  *aReturn = zIndex;

  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::SetElementZIndex(nsIDOMElement* aElement,
                             int32_t aZindex)
{
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_ARG_POINTER(element);

  nsAutoString zIndexStr;
  zIndexStr.AppendInt(aZindex);

  mCSSEditUtils->SetCSSProperty(*element, *nsGkAtoms::z_index, zIndexStr);
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::RelativeChangeZIndex(int32_t aChange)
{
  AutoPlaceholderBatch beginBatching(this);
  AutoRules beginRulesSniffing(this,
                               (aChange < 0) ? EditAction::decreaseZIndex :
                                               EditAction::increaseZIndex,
                               nsIEditor::eNext);

  // brade: can we get rid of this comment?
  // Find out if the selection is collapsed:
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  RulesInfo ruleInfo(aChange < 0 ? EditAction::decreaseZIndex :
                                   EditAction::increaseZIndex);
  bool cancel, handled;
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }

  return rules->DidDoAction(selection, &ruleInfo, rv);
}

NS_IMETHODIMP
HTMLEditor::GetElementZIndex(nsIDOMElement* aElement,
                             int32_t* aZindex)
{
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  return GetElementZIndex(element, aZindex);
}

nsresult
HTMLEditor::GetElementZIndex(Element* aElement,
                             int32_t* aZindex)
{
  if (NS_WARN_IF(!aElement)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoString zIndexStr;
  *aZindex = 0;

  nsresult rv =
    mCSSEditUtils->GetSpecifiedProperty(*aElement, *nsGkAtoms::z_index,
                                        zIndexStr);
  NS_ENSURE_SUCCESS(rv, rv);
  if (zIndexStr.EqualsLiteral("auto")) {
    // we have to look at the positioned ancestors
    // cf. CSS 2 spec section 9.9.1
    nsCOMPtr<nsINode> node = aElement->GetParentNode();
    nsAutoString positionStr;
    while (node && zIndexStr.EqualsLiteral("auto") &&
           !node->IsHTMLElement(nsGkAtoms::body)) {
      rv = mCSSEditUtils->GetComputedProperty(*node, *nsGkAtoms::position,
                                              positionStr);
      NS_ENSURE_SUCCESS(rv, rv);
      if (positionStr.EqualsLiteral("absolute")) {
        // ah, we found one, what's its z-index ? If its z-index is auto,
        // we have to continue climbing the document's tree
        rv = mCSSEditUtils->GetComputedProperty(*node, *nsGkAtoms::z_index,
                                                zIndexStr);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      node = node->GetParentNode();
    }
  }

  if (!zIndexStr.EqualsLiteral("auto")) {
    nsresult errorCode;
    *aZindex = zIndexStr.ToInteger(&errorCode);
  }

  return NS_OK;
}

ManualNACPtr
HTMLEditor::CreateGrabber(nsIContent& aParentContent)
{
  // let's create a grabber through the element factory
  ManualNACPtr ret =
    CreateAnonymousElement(nsGkAtoms::span, aParentContent,
                           NS_LITERAL_STRING("mozGrabber"), false);
  if (NS_WARN_IF(!ret)) {
    return nullptr;
  }

  // add the mouse listener so we can detect a click on a resizer
  nsCOMPtr<nsIDOMEventTarget> evtTarget = do_QueryInterface(ret);
  evtTarget->AddEventListener(NS_LITERAL_STRING("mousedown"),
                              mEventListener, false);

  return ret;
}

NS_IMETHODIMP
HTMLEditor::RefreshGrabber()
{
  NS_ENSURE_TRUE(mAbsolutelyPositionedObject, NS_ERROR_NULL_POINTER);

  nsresult rv =
    GetPositionAndDimensions(
      *mAbsolutelyPositionedObject,
      mPositionedObjectX,
      mPositionedObjectY,
      mPositionedObjectWidth,
      mPositionedObjectHeight,
      mPositionedObjectBorderLeft,
      mPositionedObjectBorderTop,
      mPositionedObjectMarginLeft,
      mPositionedObjectMarginTop);
  NS_ENSURE_SUCCESS(rv, rv);

  SetAnonymousElementPosition(mPositionedObjectX+12,
                              mPositionedObjectY-14,
                              mGrabber);
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::HideGrabber()
{
  nsresult rv = mAbsolutelyPositionedObject->UnsetAttr(kNameSpaceID_None,
                                                       nsGkAtoms::_moz_abspos,
                                                       true);
  NS_ENSURE_SUCCESS(rv, rv);

  mAbsolutelyPositionedObject = nullptr;
  NS_ENSURE_TRUE(mGrabber, NS_ERROR_NULL_POINTER);

  // get the presshell's document observer interface.
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  // We allow the pres shell to be null; when it is, we presume there
  // are no document observers to notify, but we still want to
  // UnbindFromTree.

  DeleteRefToAnonymousNode(Move(mGrabber), ps);
  DeleteRefToAnonymousNode(Move(mPositioningShadow), ps);

  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::ShowGrabberOnElement(nsIDOMElement* aElement)
{
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_ARG_POINTER(element);

  if (NS_WARN_IF(!IsDescendantOfEditorRoot(element))) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mGrabber) {
    NS_ERROR("call HideGrabber first");
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoString classValue;
  nsresult rv = CheckPositionedElementBGandFG(aElement, classValue);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = element->SetAttr(kNameSpaceID_None, nsGkAtoms::_moz_abspos,
                        classValue, true);
  NS_ENSURE_SUCCESS(rv, rv);

  // first, let's keep track of that element...
  mAbsolutelyPositionedObject = element;

  nsIContent* parentContent = element->GetParent();
  if (NS_WARN_IF(!parentContent)) {
    return NS_ERROR_FAILURE;
  }

  mGrabber = CreateGrabber(*parentContent);
  NS_ENSURE_TRUE(mGrabber, NS_ERROR_FAILURE);

  // and set its position
  return RefreshGrabber();
}

nsresult
HTMLEditor::StartMoving(nsIDOMElement* aHandle)
{
  nsCOMPtr<nsIContent> parentContent = mGrabber->GetParent();
  if (NS_WARN_IF(!parentContent) || NS_WARN_IF(!mAbsolutelyPositionedObject)) {
    return NS_ERROR_FAILURE;
  }

  // now, let's create the resizing shadow
  mPositioningShadow =
    CreateShadow(*parentContent, *mAbsolutelyPositionedObject);
  NS_ENSURE_TRUE(mPositioningShadow, NS_ERROR_FAILURE);
  nsresult rv = SetShadowPosition(mPositioningShadow,
                                  mAbsolutelyPositionedObject,
                                  mPositionedObjectX, mPositionedObjectY);
  NS_ENSURE_SUCCESS(rv, rv);

  // make the shadow appear
  mPositioningShadow->UnsetAttr(kNameSpaceID_None, nsGkAtoms::_class, true);

  // position it
  mCSSEditUtils->SetCSSPropertyPixels(*mPositioningShadow, *nsGkAtoms::width,
                                      mPositionedObjectWidth);
  mCSSEditUtils->SetCSSPropertyPixels(*mPositioningShadow, *nsGkAtoms::height,
                                      mPositionedObjectHeight);

  mIsMoving = true;
  return NS_OK; // XXX Looks like nobody refers this result
}

void
HTMLEditor::SnapToGrid(int32_t& newX, int32_t& newY)
{
  if (mSnapToGridEnabled && mGridSize) {
    newX = (int32_t) floor( ((float)newX / (float)mGridSize) + 0.5f ) * mGridSize;
    newY = (int32_t) floor( ((float)newY / (float)mGridSize) + 0.5f ) * mGridSize;
  }
}

nsresult
HTMLEditor::GrabberClicked()
{
  // add a mouse move listener to the editor
  nsresult rv = NS_OK;
  if (!mMouseMotionListenerP) {
    mMouseMotionListenerP = new ResizerMouseMotionListener(*this);
    if (!mMouseMotionListenerP) {return NS_ERROR_NULL_POINTER;}

    nsCOMPtr<nsIDOMEventTarget> piTarget = GetDOMEventTarget();
    NS_ENSURE_TRUE(piTarget, NS_ERROR_FAILURE);

    rv = piTarget->AddEventListener(NS_LITERAL_STRING("mousemove"),
                                     mMouseMotionListenerP,
                                     false, false);
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "failed to register mouse motion listener");
  }
  mGrabberClicked = true;
  return rv;
}

nsresult
HTMLEditor::EndMoving()
{
  if (mPositioningShadow) {
    nsCOMPtr<nsIPresShell> ps = GetPresShell();
    NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

    DeleteRefToAnonymousNode(Move(mPositioningShadow), ps);

    mPositioningShadow = nullptr;
  }
  nsCOMPtr<nsIDOMEventTarget> piTarget = GetDOMEventTarget();

  if (piTarget && mMouseMotionListenerP) {
    DebugOnly<nsresult> rv =
      piTarget->RemoveEventListener(NS_LITERAL_STRING("mousemove"),
                                    mMouseMotionListenerP,
                                    false);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to remove mouse motion listener");
  }
  mMouseMotionListenerP = nullptr;

  mGrabberClicked = false;
  mIsMoving = false;
  RefPtr<Selection> selection = GetSelection();
  if (!selection) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return CheckSelectionStateForAnonymousButtons(selection);
}
nsresult
HTMLEditor::SetFinalPosition(int32_t aX,
                             int32_t aY)
{
  nsresult rv = EndMoving();
  NS_ENSURE_SUCCESS(rv, rv);

  // we have now to set the new width and height of the resized object
  // we don't set the x and y position because we don't control that in
  // a normal HTML layout
  int32_t newX = mPositionedObjectX + aX - mOriginalX - (mPositionedObjectBorderLeft+mPositionedObjectMarginLeft);
  int32_t newY = mPositionedObjectY + aY - mOriginalY - (mPositionedObjectBorderTop+mPositionedObjectMarginTop);

  SnapToGrid(newX, newY);

  nsAutoString x, y;
  x.AppendInt(newX);
  y.AppendInt(newY);

  // we want one transaction only from a user's point of view
  AutoPlaceholderBatch batchIt(this);

  nsCOMPtr<Element> absolutelyPositionedObject =
    do_QueryInterface(mAbsolutelyPositionedObject);
  NS_ENSURE_STATE(absolutelyPositionedObject);
  mCSSEditUtils->SetCSSPropertyPixels(*absolutelyPositionedObject,
                                      *nsGkAtoms::top, newY);
  mCSSEditUtils->SetCSSPropertyPixels(*absolutelyPositionedObject,
                                      *nsGkAtoms::left, newX);
  // keep track of that size
  mPositionedObjectX  = newX;
  mPositionedObjectY  = newY;

  return RefreshResizers();
}

void
HTMLEditor::AddPositioningOffset(int32_t& aX,
                                 int32_t& aY)
{
  // Get the positioning offset
  int32_t positioningOffset =
    Preferences::GetInt("editor.positioning.offset", 0);

  aX += positioningOffset;
  aY += positioningOffset;
}

NS_IMETHODIMP
HTMLEditor::AbsolutelyPositionElement(nsIDOMElement* aElement,
                                      bool aEnabled)
{
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_ARG_POINTER(element);

  nsAutoString positionStr;
  mCSSEditUtils->GetComputedProperty(*element, *nsGkAtoms::position,
                                     positionStr);
  bool isPositioned = (positionStr.EqualsLiteral("absolute"));

  // nothing to do if the element is already in the state we want
  if (isPositioned == aEnabled)
    return NS_OK;

  AutoPlaceholderBatch batchIt(this);

  if (aEnabled) {
    int32_t x, y;
    GetElementOrigin(*element, x, y);

    mCSSEditUtils->SetCSSProperty(*element, *nsGkAtoms::position,
                                  NS_LITERAL_STRING("absolute"));

    AddPositioningOffset(x, y);
    SnapToGrid(x, y);
    SetElementPosition(*element, x, y);

    // we may need to create a br if the positioned element is alone in its
    // container
    nsCOMPtr<nsINode> element = do_QueryInterface(aElement);
    NS_ENSURE_STATE(element);

    nsINode* parentNode = element->GetParentNode();
    if (parentNode->GetChildCount() == 1) {
      RefPtr<Selection> selection = GetSelection();
      if (NS_WARN_IF(!selection)) {
        return NS_ERROR_FAILURE;
      }
      RefPtr<Element> newBRElement =
        CreateBRImpl(*selection, EditorRawDOMPoint(parentNode, 0), eNone);
      if (NS_WARN_IF(!newBRElement)) {
        return NS_ERROR_FAILURE;
      }
    }
  }
  else {
    mCSSEditUtils->RemoveCSSProperty(*element, *nsGkAtoms::position,
                                     EmptyString());
    mCSSEditUtils->RemoveCSSProperty(*element, *nsGkAtoms::top,
                                     EmptyString());
    mCSSEditUtils->RemoveCSSProperty(*element, *nsGkAtoms::left,
                                     EmptyString());
    mCSSEditUtils->RemoveCSSProperty(*element, *nsGkAtoms::z_index,
                                     EmptyString());

    if (!HTMLEditUtils::IsImage(aElement)) {
      mCSSEditUtils->RemoveCSSProperty(*element, *nsGkAtoms::width,
                                       EmptyString());
      mCSSEditUtils->RemoveCSSProperty(*element, *nsGkAtoms::height,
                                       EmptyString());
    }

    nsCOMPtr<dom::Element> element = do_QueryInterface(aElement);
    if (element && element->IsHTMLElement(nsGkAtoms::div) &&
        !HasStyleOrIdOrClass(element)) {
      RefPtr<HTMLEditRules> htmlRules =
        static_cast<HTMLEditRules*>(mRules.get());
      NS_ENSURE_TRUE(htmlRules, NS_ERROR_FAILURE);
      nsresult rv = htmlRules->MakeSureElemStartsOrEndsOnCR(*element);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = RemoveContainer(element);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::SetSnapToGridEnabled(bool aEnabled)
{
  mSnapToGridEnabled = aEnabled;
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetSnapToGridEnabled(bool* aIsEnabled)
{
  *aIsEnabled = mSnapToGridEnabled;
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::SetGridSize(uint32_t aSize)
{
  mGridSize = aSize;
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetGridSize(uint32_t* aSize)
{
  *aSize = mGridSize;
  return NS_OK;
}

// self-explanatory
NS_IMETHODIMP
HTMLEditor::SetElementPosition(nsIDOMElement* aElement,
                               int32_t aX,
                               int32_t aY)
{
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_STATE(element);

  SetElementPosition(*element, aX, aY);
  return NS_OK;
}

void
HTMLEditor::SetElementPosition(Element& aElement,
                               int32_t aX,
                               int32_t aY)
{
  AutoPlaceholderBatch batchIt(this);
  mCSSEditUtils->SetCSSPropertyPixels(aElement, *nsGkAtoms::left, aX);
  mCSSEditUtils->SetCSSPropertyPixels(aElement, *nsGkAtoms::top, aY);
}

// self-explanatory
NS_IMETHODIMP
HTMLEditor::GetPositionedElement(nsIDOMElement** aReturn)
{
  nsCOMPtr<nsIDOMElement> ret =
    static_cast<nsIDOMElement*>(GetAsDOMNode(GetPositionedElement()));
  ret.forget(aReturn);
  return NS_OK;
}

nsresult
HTMLEditor::CheckPositionedElementBGandFG(nsIDOMElement* aElement,
                                          nsAString& aReturn)
{
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
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_STATE(element || !aElement);

  aReturn.Truncate();

  nsAutoString bgImageStr;
  nsresult rv =
    mCSSEditUtils->GetComputedProperty(*element, *nsGkAtoms::background_image,
                                       bgImageStr);
  NS_ENSURE_SUCCESS(rv, rv);
  if (bgImageStr.EqualsLiteral("none")) {
    nsAutoString bgColorStr;
    rv =
      mCSSEditUtils->GetComputedProperty(*element, *nsGkAtoms::backgroundColor,
                                         bgColorStr);
    NS_ENSURE_SUCCESS(rv, rv);
    if (bgColorStr.EqualsLiteral("transparent")) {
      RefPtr<nsComputedDOMStyle> cssDecl =
        mCSSEditUtils->GetComputedStyle(element);
      NS_ENSURE_STATE(cssDecl);

      // from these declarations, get the one we want and that one only
      ErrorResult error;
      RefPtr<dom::CSSValue> cssVal = cssDecl->GetPropertyCSSValue(NS_LITERAL_STRING("color"), error);
      NS_ENSURE_TRUE(!error.Failed(), error.StealNSResult());

      nsROCSSPrimitiveValue* val = cssVal->AsPrimitiveValue();
      NS_ENSURE_TRUE(val, NS_ERROR_FAILURE);

      if (nsIDOMCSSPrimitiveValue::CSS_RGBCOLOR == val->PrimitiveType()) {
        nsDOMCSSRGBColor* rgbVal = val->GetRGBColorValue(error);
        NS_ENSURE_TRUE(!error.Failed(), error.StealNSResult());
        float r = rgbVal->Red()->
          GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_NUMBER, error);
        NS_ENSURE_TRUE(!error.Failed(), error.StealNSResult());
        float g = rgbVal->Green()->
          GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_NUMBER, error);
        NS_ENSURE_TRUE(!error.Failed(), error.StealNSResult());
        float b = rgbVal->Blue()->
          GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_NUMBER, error);
        NS_ENSURE_TRUE(!error.Failed(), error.StealNSResult());
        if (r >= BLACK_BG_RGB_TRIGGER &&
            g >= BLACK_BG_RGB_TRIGGER &&
            b >= BLACK_BG_RGB_TRIGGER)
          aReturn.AssignLiteral("black");
        else
          aReturn.AssignLiteral("white");
        return NS_OK;
      }
    }
  }

  return NS_OK;
}

} // namespace mozilla
